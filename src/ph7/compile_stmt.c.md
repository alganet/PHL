# src/ph7/compile_stmt.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2092/2612 lines (80.09%)

[Root index](../../index.md) | [Directory index](index.md)

|     Hits | Line | Source |
| -------: | ---: | :--- |
|        - |    1 | `/**` |
|        - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|        - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|        - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|        - |    5 | ` */` |
|        - |    6 | `#include "ph7int.h"` |
|        - |    7 | `#include "compile_int.h"` |
|        - |    8 | `/*` |
|        - |    9 | ` * Section:` |
|        - |   10 | ` *    Statement compilation: const, continue/break, labels and goto, blocks,` |
|        - |   11 | ` *    loops (while/do/for/foreach), if, global, return, yield, halt, echo,` |
|        - |   12 | ` *    static, var, namespace/use/declare, throw, try/catch/finally, switch.` |
|        - |   13 | ` * Status:` |
|        - |   14 | ` *    Stable.` |
|        - |   15 | ` */` |
|        - |   16 | `/*` |
|        - |   17 | ` * Compile the 'const' statement.` |
|        - |   18 | ` * According to the PHP language reference` |
|        - |   19 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|        - |   20 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|        - |   21 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|        - |   22 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|        - |   23 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|        - |   24 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|        - |   25 | ` *  Syntax` |
|        - |   26 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|        - |   27 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|        - |   28 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|        - |   29 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|        - |   30 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|        - |   31 | ` *  to get a list of all defined constants.` |
|        - |   32 | ` *` |
|        - |   33 | ` * Symisc eXtension.` |
|        - |   34 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|        - |   35 | ` *  would allow only simple scalar value.` |
|        - |   36 | ` *  Example` |
|        - |   37 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|        - |   38 | ` *    Refer to the official documentation for more information on this feature.` |
|        - |   39 | ` */` |
|        - |   40 | `/*` |
|        - |   41 | ` * The keyword-shaped names php still accepts as a GLOBAL constant name: its lexer` |
|        - |   42 | `` * does not reserve the type words or the scope words, so `const int = 1;` and`` |
|        - |   43 | `` * `const self = 1;` parse there while every other keyword is a syntax error. Kept`` |
|        - |   44 | ` * as a list rather than a token-class test because the two engines' keyword TABLES` |
|        - |   45 | ` * are not the same set — PHL lexes some words php leaves as plain identifiers.` |
|        - |   46 | ` */` |
|       20 |   47 | `static int GenStateConstNameKeywordOk(SyString *pName)` |
|        3 |   48 | `{` |
|        - |   49 | ``	/* `integer` and `boolean` lex to the same tokens as `int` and `bool` here,`` |
|        - |   50 | `	 * but php has no reserved word under either spelling and takes both as a` |
|        - |   51 | `	 * constant name. */` |
|        - |   52 | `	static const char *azOk[] = { "bool", "boolean", "float", "int", "integer",` |
|        - |   53 | `	                              "object", "string", "self", "parent" };` |
|        - |   54 | `	sxu32 i;` |
|      113 |   55 | `	for( i = 0 ; i < SX_ARRAYSIZE(azOk) ; ++i ){` |
|      111 |   56 | `		sxu32 n = (sxu32)SyStrlen(azOk[i]);` |
|      111 |   57 | `		if( pName->nByte == n && SyStrnicmp(pName->zString,azOk[i],n) == 0 ){` |
|       20 |   58 | `			return 1;` |
|        - |   59 | `		}` |
|       48 |   60 | `	}` |
|        3 |   61 | `	return 0;` |
|       13 |   62 | `}` |
|      200 |   63 | `PH7_PRIVATE sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|        5 |   64 | `{` |
|        - |   65 | `	SySet *pConsCode,*pInstrContainer;` |
|        - |   66 | `	sxu32 nLineLocal;` |
|        - |   67 | `	SyString *pName;` |
|        - |   68 | `	sxi32 rc;` |
|        - |   69 | `	/* php forbids attributes on a comma-separated const list. Snapshot whether the` |
|        - |   70 | `	 * statement carries any now, before the first constant consumes them. */` |
|      205 |   71 | `	int bHadAttrs = SySetUsed(&pGen->aPendingAttrs) > 0;` |
|        - |   72 | ``	/* php attributes every const-statement compile error to the `const` keyword's`` |
|        - |   73 | ``	 * line, not the offending list element's own line (`const A=1,\nB=strlen()` blames`` |
|        - |   74 | `	 * line 1). Capture it once here, before jumping the keyword. */` |
|      205 |   75 | `	nLineLocal = pGen->pIn->nLine;` |
|      205 |   76 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|        - |   77 | ``	/* php allows a single `const` statement to declare several constants at once`` |
|        - |   78 | ``	 * (`const A = 1, B = 2;`). Loop over the comma-separated name = value pairs;`` |
|        - |   79 | `	 * PH7_CompileExpr(EXPR_FLAG_COMMA_STATEMENT) stops each value at the first` |
|        - |   80 | `	 * top-level comma so the next pair starts cleanly. */` |
|      105 |   81 | `Loop:` |
|      215 |   82 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |   83 | `		/* Invalid constant name */` |
|        9 |   84 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"identifier");` |
|        9 |   85 | `		if( rc == SXERR_ABORT ){` |
|        - |   86 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |   87 | `			return SXERR_ABORT;` |
|        - |   88 | `		}` |
|        9 |   89 | `		goto Synchronize;` |
|        - |   90 | `	}` |
|        - |   91 | `	/* Peek constant name */` |
|      209 |   92 | `	pName = &pGen->pIn->sData;` |
|        - |   93 | ``	/* php's global `const` takes an IDENTIFIER: a reserved word is a parse error`` |
|        - |   94 | `` 	 * there, and only a CLASS constant may carry one (`class C { const list = 5; }` `` |
|        - |   95 | ``	 * is php-legal, `const list = 5;` at file scope is not). PHL accepted both, so`` |
|        - |   96 | ``	 * `const LIST = 1;` compiled and READ back — source php refuses to parse.`` |
|        - |   97 | `	 * The words php still allows are the ones its lexer does not reserve: the type` |
|        - |   98 | ``	 * names and the scope words. The word OPERATORS (`and`, `or`, `xor`, `new`,`` |
|        - |   99 | ``	 * `clone`, `instanceof`) are reserved too — the lexer types those ID\|OP rather`` |
|        - |  100 | `	 * than KEYWORD, which is why the test reads both bits. */` |
|      204 |  101 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_OP))` |
|      117 |  102 | `	 && !GenStateConstNameKeywordOk(pName) ){` |
|        3 |  103 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn,"identifier");` |
|        3 |  104 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  105 | `			return SXERR_ABORT;` |
|        - |  106 | `		}` |
|        3 |  107 | `		goto Synchronize;` |
|        - |  108 | `	}` |
|        - |  109 | `	/* Make sure the constant name isn't reserved */` |
|      207 |  110 | `	if( GenStateIsReservedConstant(pName) ){` |
|        - |  111 | `		/* Reserved constant */` |
|        9 |  112 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Cannot redeclare constant '%z'",pName);` |
|        9 |  113 | `		if( rc == SXERR_ABORT ){` |
|        - |  114 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  115 | `			return SXERR_ABORT;` |
|        - |  116 | `		}` |
|        9 |  117 | `		goto Synchronize;` |
|        - |  118 | `	}` |
|      199 |  119 | `	pGen->pIn++;` |
|      199 |  120 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|        - |  121 | `		/* Invalid statement*/` |
|        6 |  122 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"=\"");` |
|        6 |  123 | `		if( rc == SXERR_ABORT ){` |
|        - |  124 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  125 | `			return SXERR_ABORT;` |
|        - |  126 | `		}` |
|        6 |  127 | `		goto Synchronize;` |
|        - |  128 | `	}` |
|      195 |  129 | `	pGen->pIn++; /*Jump the equal sign */` |
|        - |  130 | `	/* php's constant-expression rules, first offender wins (see` |
|        - |  131 | ``	 * PH7_GenStateConstExprError). A global `const` DOES take `new` (PHP 8.1's`` |
|        - |  132 | ``	 * "new in initializers"), so the `new` rule is off here. */`` |
|        - |  133 | `	{` |
|      195 |  134 | `		const char *zCErr = PH7_GenStateConstExprError(pGen,1);` |
|      195 |  135 | `		if( zCErr ){` |
|       11 |  136 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"%s",zCErr);` |
|       11 |  137 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  138 | `				return SXERR_ABORT;` |
|        - |  139 | `			}` |
|       11 |  140 | `			goto Synchronize;` |
|        - |  141 | `		}` |
|        - |  142 | `	}` |
|        - |  143 | `	/* Allocate a new constant value container */` |
|      187 |  144 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      187 |  145 | `	if( pConsCode == 0 ){` |
|      ! 0 |  146 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  147 | `		return SXERR_ABORT;` |
|        - |  148 | `	}` |
|      187 |  149 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - |  150 | `	/* Swap bytecode container */` |
|      187 |  151 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      187 |  152 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|        - |  153 | ``	/* Compile constant value. php: a stray token after `const X = EXPR` is`` |
|        - |  154 | ``	 * `... expecting "," or ";"` (const supports a comma-separated list).`` |
|        - |  155 | `	 * EXPR_FLAG_COMMA_STATEMENT stops this value at the first top-level comma so a` |
|        - |  156 | `	 * following declaration is left for the loop below. */` |
|        - |  157 | `	{` |
|      187 |  158 | `		const char *zSaveConst = pGen->zClauseCloser;` |
|      187 |  159 | `		pGen->zClauseCloser = "\",\" or \";\"";` |
|      187 |  160 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      187 |  161 | `		pGen->zClauseCloser = zSaveConst;` |
|        - |  162 | `	}` |
|        - |  163 | `	/* Emit the done instruction */` |
|      187 |  164 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      187 |  165 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      187 |  166 | `	if( rc == SXERR_ABORT ){` |
|        - |  167 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  168 | `		return SXERR_ABORT;` |
|        - |  169 | `	}` |
|        - |  170 | ``	/* php parse-errors an empty initializer (`const A = ;`, or a `,B=` list hole);`` |
|        - |  171 | `	 * the class-const path rejects it too. Reject loudly rather than silently` |
|        - |  172 | `	 * defining a NULL constant. (php's error KIND -- a parse error -- differs from` |
|        - |  173 | `	 * PHL's compile fatal, the recursive-descent-vs-bison family; both refuse.) */` |
|      187 |  174 | `	if( rc == SXERR_EMPTY && PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|        2 |  175 | `			"Empty constant '%z' value",pName) == SXERR_ABORT ){` |
|      ! 0 |  176 | `		return SXERR_ABORT;` |
|        - |  177 | `	}` |
|      187 |  178 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|        - |  179 | `	/* Register the constant with namespace-qualified name */` |
|        - |  180 | `	{` |
|        - |  181 | `		SyBlob sFQN;` |
|        - |  182 | `		SyString sFQNStr;` |
|      187 |  183 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      187 |  184 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      187 |  185 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|        - |  186 | ``		/* php refuses a `const` whose name a local `use const` already took. */`` |
|      187 |  187 | `		if( GenStateGuardImportRedeclare(pGen,2,pName,&sFQNStr,nLineLocal) == SXERR_ABORT ){` |
|      ! 0 |  188 | `			SyBlobRelease(&sFQN);` |
|      ! 0 |  189 | `			return SXERR_ABORT;` |
|        - |  190 | `		}` |
|      278 |  191 | `		rc = PH7_VmRegisterConstantEx(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode,` |
|      182 |  192 | `			(SyString *)SySetPeek(&pGen->pVm->aFiles),nLineLocal,1);` |
|      187 |  193 | `		if( rc == SXRET_OK && SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|        - |  194 | ``			/* php 8.5: attributes on `const` statements — attach the pending`` |
|        - |  195 | `			 * groups to the registered constant record for Reflection. */` |
|       15 |  196 | `			SyHashEntry *pCEntry = SyHashGet(&pGen->pVm->hConstant,` |
|        8 |  197 | `				SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|       11 |  198 | `			if( pCEntry ){` |
|       11 |  199 | `				ph7_constant *pRegCons = (ph7_constant *)pCEntry->pUserData;` |
|       11 |  200 | `				if( GenStateConsumeAttrs(&(*pGen),&pRegCons->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  201 | `					SyBlobRelease(&sFQN);` |
|      ! 0 |  202 | `					return SXERR_ABORT;` |
|        - |  203 | `				}` |
|        8 |  204 | `				if( GenStateCheckAttrPlacement(&(*pGen),&pRegCons->aAttrs,64,64,0,0)` |
|        7 |  205 | `					== SXERR_ABORT ){` |
|      ! 0 |  206 | `					SyBlobRelease(&sFQN);` |
|      ! 0 |  207 | `					return SXERR_ABORT;` |
|        - |  208 | `				}` |
|        4 |  209 | `			}` |
|        4 |  210 | `		}` |
|      187 |  211 | `		SyBlobRelease(&sFQN);` |
|        - |  212 | `	}` |
|      187 |  213 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  214 | `		SySetRelease(pConsCode);` |
|      ! 0 |  215 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|      ! 0 |  216 | `	}` |
|        - |  217 | ``	/* Another declaration in the same statement: `const A = 1, B = 2;`. */`` |
|      187 |  218 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /* ',' */) ){` |
|       14 |  219 | `		if( bHadAttrs ){` |
|        - |  220 | ``			/* php compile-fatals `#[Attr] const A = 1, B = 2;` outright. */`` |
|        3 |  221 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|        - |  222 | `				"Cannot apply attributes to multiple constants at once");` |
|        3 |  223 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  224 | `				return SXERR_ABORT;` |
|        - |  225 | `			}` |
|        3 |  226 | `			goto Synchronize;` |
|        - |  227 | `		}` |
|       12 |  228 | `		pGen->pIn++; /* Jump the comma */` |
|       12 |  229 | `		goto Loop;` |
|        - |  230 | `	}` |
|      175 |  231 | `	return SXRET_OK;` |
|       15 |  232 | `Synchronize:` |
|        - |  233 | `	/* Synchronize with the next top-level semicolon and avoid compiling this` |
|        - |  234 | ``	 * erroneous statement. BRACE-aware (only `{`...`}`): a rejected closure`` |
|        - |  235 | ``	 * initializer's body holds inner `;` that are not statement terminators, so`` |
|        - |  236 | ``	 * without this its `;` and `}` dangle into a spurious second error where php`` |
|        - |  237 | `	 * halts at the first fatal. Parentheses and brackets are deliberately NOT` |
|        - |  238 | ``	 * tracked -- a lone unbalanced `(`/`[` in erroneous input must not swallow the`` |
|        - |  239 | ``	 * following statements (which would drop later error reports); a `{` never`` |
|        - |  240 | `	 * appears in a valid global-const initializer except as a closure body. */` |
|        - |  241 | `	{` |
|       34 |  242 | `		int iBrace = 0;` |
|      130 |  243 | `		while( pGen->pIn < pGen->pEnd ){` |
|      130 |  244 | `			if( iBrace == 0 && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       34 |  245 | `				break;` |
|        - |  246 | `			}` |
|      100 |  247 | `			if( pGen->pIn->nType & PH7_TK_OCB ){` |
|        3 |  248 | `				iBrace++;` |
|       99 |  249 | `			}else if( pGen->pIn->nType & PH7_TK_CCB ){` |
|        3 |  250 | `				if( iBrace > 0 ){ iBrace--; }` |
|        1 |  251 | `			}` |
|      100 |  252 | `			pGen->pIn++;` |
|        4 |  253 | `		}` |
|        - |  254 | `	}` |
|       34 |  255 | `	return SXRET_OK;` |
|      105 |  256 | `}` |
|        - |  257 | `/*` |
|        - |  258 | ` * Compile the 'continue' statement.` |
|        - |  259 | ` * According to the PHP language reference` |
|        - |  260 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|        - |  261 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|        - |  262 | ` *  iteration.` |
|        - |  263 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|        - |  264 | ` *  the purposes of continue.` |
|        - |  265 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|        - |  266 | ` *  of enclosing loops it should skip to the end of.` |
|        - |  267 | ` *  Note:` |
|        - |  268 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|        - |  269 | ` */` |
|        - |  270 | `/*` |
|        - |  271 | `` * Pick the jump opcode for a `break`/`continue` targeting pLoop and fill in its iP1,`` |
|        - |  272 | ` * emitting the POP_EXCEPTIONs for the crossed trys this statement can resolve here and` |
|        - |  273 | `` * now. All the classification lives in GenStateJumpScope, which `goto` shares.`` |
|        - |  274 | ` */` |
|    56259 |  275 | `static sxi32 GenStateLoopJumpOp(ph7_gen_state *pGen,GenBlock *pLoop,sxi32 *piP1,` |
|        - |  276 | `	GenJumpScope *pCross)` |
|        5 |  277 | `{` |
|        - |  278 | `	/* The loop encloses the break by construction, so the walk always reaches it. Note the` |
|        - |  279 | `	 * walk EMITS the crossed trys' POP_EXCEPTIONs as it goes, before the caller can see` |
|        - |  280 | `	 * pCross->nFinally and reject: a statement about to be fatal therefore leaves a few` |
|        - |  281 | `	 * dead instructions behind. Harmless — the compile error stops the program from` |
|        - |  282 | `	 * running at all — and the alternative is walking the chain twice on every jump. */` |
|    56264 |  283 | `	GenStateJumpScope(&(*pGen),pGen->nCurScopeId,pLoop->nScopeId,TRUE,pCross);` |
|    56264 |  284 | `	return GenStateScopeJumpOp(pCross,piP1);` |
|        5 |  285 | `}` |
|        - |  286 | `/*` |
|        - |  287 | `` * php compile-rejects a `break`/`continue`/`goto` that leaves a `finally` body (a`` |
|        - |  288 | `` * `return` is fine). One wording, one place, for all three statements.`` |
|        - |  289 | ` */` |
|        - |  290 | `/* Whether the cursor has run past the LAST token of a chunk that met the end of` |
|        - |  291 | ` * the FILE -- the shared end-of-input question, asked here by the three statements` |
|        - |  292 | ` * that would otherwise report a complaint of their own first. */` |
|        2 |  293 | `static int GenStateAtChunkEofStmt(ph7_gen_state *pGen)` |
|      ! 0 |  294 | `{` |
|        - |  295 | `	SyToken *pBase;` |
|        2 |  296 | `	if( !pGen->bChunkAtEof \|\| pGen->pTokenSet == 0 ){` |
|      ! 0 |  297 | `		return 0;` |
|        - |  298 | `	}` |
|        2 |  299 | `	pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        2 |  300 | `	return pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)];` |
|        1 |  301 | `}` |
|       10 |  302 | `PH7_PRIVATE sxi32 GenStateJumpOutOfFinally(ph7_gen_state *pGen,sxu32 nLine)` |
|        4 |  303 | `{` |
|       14 |  304 | `	return PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  305 | `		"jump out of a finally block is disallowed");` |
|        4 |  306 | `}` |
|        - |  307 | `/*` |
|        - |  308 | `` * php's `break`/`continue` grammar is `KEYWORD optional_expr ";"`, and the level is`` |
|        - |  309 | ` * then screened as a compile-time VALUE rather than parsed as a number: an operand` |
|        - |  310 | `` * that is not a literal at all -- a constant, a variable, `-1`, `(1+0)` -- is`` |
|        - |  311 | `` * `'break' operator with non-integer operand is no longer supported`, and one that`` |
|        - |  312 | `` * IS a literal but not a positive integer -- `1.5`, `"1"`, `0` -- is`` |
|        - |  313 | `` * `'break' operator accepts only positive integers`. Parentheses are transparent`` |
|        - |  314 | `` * (`((1))` is 1) because they leave no node of their own, which is also why the`` |
|        - |  315 | ` * arithmetic inside them is not folded away first.` |
|        - |  316 | ` *` |
|        - |  317 | `` * PHL used to read a NUMBER token and ignore anything else, so `break 1.5;` broke`` |
|        - |  318 | `` * one level in silence, `break $x;` and `break foo;` compiled to a plain break with`` |
|        - |  319 | ` * a WARNING about the missing semicolon, and the program then RAN.` |
|        - |  320 | ` *` |
|        - |  321 | ` * Answers SXRET_OK with *piLevel set (1 when there is no operand), SXERR_ABORT to` |
|        - |  322 | ` * abort the compile, or SXERR_SYNTAX once a refusal has been reported.` |
|        - |  323 | ` */` |
|    56327 |  324 | `static sxi32 GenStateJumpLevelArg(ph7_gen_state *pGen,const char *zWhich,sxu32 nLine,sxi32 *piLevel)` |
|        5 |  325 | `{` |
|    56332 |  326 | `	SyToken *pStart = pGen->pIn,*pEnd = pGen->pEnd,*pAfter;` |
|        - |  327 | `	sxi32 rc;` |
|    56332 |  328 | `	*piLevel = 1;` |
|        - |  329 | `	/* The statement slice runs to the end of the enclosing block, so the operand` |
|        - |  330 | `	 * stops at its own terminator. */` |
|    56464 |  331 | `	for( pAfter = pStart ; pAfter < pEnd ; pAfter++ ){` |
|    56462 |  332 | `		if( pAfter->nType & PH7_TK_SEMI ){` |
|    56330 |  333 | `			pEnd = pAfter;` |
|    56330 |  334 | `			break;` |
|        - |  335 | `		}` |
|       71 |  336 | `	}` |
|    56332 |  337 | `	if( pStart >= pEnd ){` |
|    56250 |  338 | `		return SXRET_OK; /* No operand at all: one level */` |
|        - |  339 | `	}` |
|       87 |  340 | `	pGen->pIn = pEnd; /* The whole operand belongs to this statement either way */` |
|        - |  341 | `	/* Peel parenthesis pairs that wrap the WHOLE operand. */` |
|       51 |  342 | `	for(;;){` |
|        - |  343 | `		SyToken *pTok;` |
|       97 |  344 | `		sxi32 nDepth = 0;` |
|       97 |  345 | `		if( !(pStart->nType & PH7_TK_LPAREN) \|\| !(pEnd[-1].nType & PH7_TK_RPAREN) ){` |
|       46 |  346 | `			break;` |
|        - |  347 | `		}` |
|       36 |  348 | `		for( pTok = pStart ; pTok < pEnd ; pTok++ ){` |
|       36 |  349 | `			if( pTok->nType & PH7_TK_LPAREN ){` |
|       12 |  350 | `				nDepth++;` |
|       30 |  351 | `			}else if( pTok->nType & PH7_TK_RPAREN ){` |
|       12 |  352 | `				nDepth--;` |
|       12 |  353 | `				if( nDepth == 0 ){` |
|       10 |  354 | `					break;` |
|        - |  355 | `				}` |
|        1 |  356 | `			}` |
|       13 |  357 | `		}` |
|       10 |  358 | `		if( pTok != pEnd - 1 ){` |
|      ! 0 |  359 | `			break; /* The opening paren closes before the end: not a wrap */` |
|        - |  360 | `		}` |
|       10 |  361 | `		pStart++;` |
|       10 |  362 | `		pEnd--;` |
|      ! 0 |  363 | `	}` |
|       87 |  364 | `	if( pStart >= pEnd ){` |
|        - |  365 | ``		/* The parentheses held nothing (`break ();`): php's expression parser has`` |
|        - |  366 | `		 * no operand to read and names the closing one, which is where the peel` |
|        - |  367 | `		 * above left the cursor. */` |
|        2 |  368 | `		rc = PH7_GenSyntaxError(&(*pGen),pStart,0);` |
|        2 |  369 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  370 | `	}` |
|       80 |  371 | `	if( (pStart->nType & (PH7_TK_INTEGER\|PH7_TK_REAL\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_HEREDOC` |
|       45 |  372 | `	                     \|PH7_TK_NOWDOC\|PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|        - |  373 | `		/* An operator leads the operand, so it cannot be a literal node. */` |
|        4 |  374 | `		goto NonInteger;` |
|        - |  375 | `	}` |
|        - |  376 | ``	/* The primary this operand starts with -- `$name` is two tokens here. */`` |
|       81 |  377 | `	pAfter = &pStart[(pStart->nType & PH7_TK_DOLLAR) ? 2 : 1];` |
|       81 |  378 | `	if( pAfter < pEnd ){` |
|       18 |  379 | `		if( (pAfter->nType & (PH7_TK_OP\|PH7_TK_OSB\|PH7_TK_LPAREN))` |
|       14 |  380 | `		 && (pAfter->nType & PH7_TK_COMMA) == 0 ){` |
|        - |  381 | `			/* The primary continues into a larger expression php will not take.` |
|        - |  382 | `			 * A comma is typed as an operator here and is not one to php: the` |
|        - |  383 | `			 * expression has ENDED there, so the semicolon is what it wants. */` |
|        4 |  384 | `			goto NonInteger;` |
|        - |  385 | `		}` |
|        - |  386 | `		/* php read its expression and now wants the semicolon. */` |
|       16 |  387 | `		rc = PH7_GenSyntaxError(&(*pGen),pAfter,"\";\"");` |
|       16 |  388 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  389 | `	}` |
|       62 |  390 | `	if( pStart->nType & PH7_TK_INTEGER ){` |
|        - |  391 | `		char zScratch[GEN_NUM_SCRATCH];` |
|       50 |  392 | `		char *zAlloc = 0;` |
|        - |  393 | `		SyString sNum;` |
|       73 |  394 | `		if( SXRET_OK != GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|       46 |  395 | `				&pStart->sData,zScratch,sizeof(zScratch),&sNum,&zAlloc) ){` |
|      ! 0 |  396 | `			return SXERR_ABORT;` |
|        - |  397 | `		}` |
|       50 |  398 | `		*piLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|       50 |  399 | `		if( zAlloc ){` |
|      ! 0 |  400 | `			SyMemBackendFree(&pGen->pVm->sAllocator,zAlloc);` |
|      ! 0 |  401 | `		}` |
|       50 |  402 | `		if( *piLevel < 1 ){` |
|        3 |  403 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 |  404 | `				"'%s' operator accepts only positive integers",zWhich);` |
|        2 |  405 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  406 | `		}` |
|       48 |  407 | `		return SXRET_OK;` |
|        - |  408 | `	}` |
|       12 |  409 | `	if( pStart->nType & (PH7_TK_REAL\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){` |
|        9 |  410 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        3 |  411 | `			"'%s' operator accepts only positive integers",zWhich);` |
|        6 |  412 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  413 | `	}` |
|        3 |  414 | `NonInteger:` |
|       21 |  415 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        7 |  416 | `		"'%s' operator with non-integer operand is no longer supported",zWhich);` |
|       14 |  417 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|    28130 |  418 | `}` |
|    39849 |  419 | `PH7_PRIVATE sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|        5 |  420 | `{` |
|        - |  421 | `	GenBlock *pLoop; /* Target loop */` |
|        - |  422 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|        - |  423 | `	sxi32 iRawLevel; /* The level as WRITTEN, kept for php's diagnostics */` |
|        - |  424 | `	sxu32 nLineLocal;` |
|        - |  425 | `	sxi32 rc;` |
|    39854 |  426 | `	iRawLevel = 1;` |
|    39854 |  427 | `	nLineLocal = pGen->pIn->nLine;` |
|    39854 |  428 | `	iLevel = 0;` |
|        - |  429 | `	/* Jump the 'continue' keyword */` |
|    39854 |  430 | `	pGen->pIn++;` |
|    39854 |  431 | `	rc = GenStateJumpLevelArg(&(*pGen),"continue",nLineLocal,&iRawLevel);` |
|    39854 |  432 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  433 | `		return SXERR_ABORT;` |
|    39854 |  434 | `	}else if( rc != SXRET_OK ){` |
|        9 |  435 | `		return SXRET_OK; /* Refused and reported */` |
|        - |  436 | `	}` |
|    39846 |  437 | `	if( pGen->pIn >= pGen->pEnd && GenStateAtChunkEofStmt(pGen) ){` |
|        - |  438 | `		/* php's parser wants the semicolon before it ever asks where the jump` |
|        - |  439 | ``		 * lands, so a `continue` that ends the FILE is its parse error and not a`` |
|        - |  440 | `		 * sentence about the loop this one is not in. */` |
|      ! 0 |  441 | `		rc = PH7_GenSyntaxError(&(*pGen),0,"\";\"");` |
|      ! 0 |  442 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - |  443 | `	}` |
|    39846 |  444 | `	iLevel = iRawLevel < 2 ? 0 : iRawLevel;` |
|        - |  445 | `	/* Point to the target loop */` |
|    39846 |  446 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    39846 |  447 | `	if( pLoop == 0 ){` |
|        - |  448 | ``		/* Same split php makes for `break`: no loop at all keeps the not-in-context`` |
|        - |  449 | ``		 * wording, too FEW loops is `Cannot 'continue' N levels`. */`` |
|       11 |  450 | `		if( GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,0) != 0 ){` |
|      ! 0 |  451 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|      ! 0 |  452 | `				"Cannot 'continue' %d level%s",iRawLevel,iRawLevel == 1 ? "" : "s");` |
|      ! 0 |  453 | `		}else{` |
|       11 |  454 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"'continue' not in the 'loop' or 'switch' context");` |
|        - |  455 | `		}` |
|       11 |  456 | `		if( rc == SXERR_ABORT ){` |
|        - |  457 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  458 | `			return SXERR_ABORT;` |
|        - |  459 | `		}` |
|        6 |  460 | `	}else{` |
|    39836 |  461 | `		sxu32 nInstrIdx = 0;` |
|    39836 |  462 | `		sxi32 iP1 = 0;` |
|        - |  463 | `		GenJumpScope sCross;` |
|    39836 |  464 | `		sxi32 iJmpOp = GenStateLoopJumpOp(&(*pGen),pLoop,&iP1,&sCross);` |
|    39836 |  465 | `		if( sCross.nFinally > 0 ){` |
|        3 |  466 | `			if( GenStateJumpOutOfFinally(&(*pGen),nLineLocal) == SXERR_ABORT ){` |
|      ! 0 |  467 | `				return SXERR_ABORT;` |
|        1 |  468 | `			}` |
|    39835 |  469 | `		}else if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|        - |  470 | ``			/* `continue` inside a switch acts like `break` — which is almost never what`` |
|        - |  471 | `			 * the author meant, so php 7.3+ says so at compile time. The generated jump` |
|        - |  472 | `			 * is unchanged; only the diagnostic was missing. An explicit level` |
|        - |  473 | ``			 * (`continue 2`) targets the enclosing loop and stays silent. */`` |
|       18 |  474 | `			if( iLevel < 1 ){` |
|       18 |  475 | `				PH7_GenCompileError(&(*pGen),E_WARNING,nLineLocal,` |
|        - |  476 | `					"\"continue\" targeting switch is equivalent to \"break\"."` |
|        - |  477 | `					" Did you mean to use \"continue 2\"?");` |
|        8 |  478 | `			}` |
|       18 |  479 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,iP1,0,0,&nInstrIdx);` |
|       18 |  480 | `			if( rc == SXRET_OK ){` |
|       18 |  481 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|        8 |  482 | `			}` |
|       10 |  483 | `		}else{` |
|        - |  484 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    39818 |  485 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,iP1,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    39818 |  486 | `			if( pLoop->bPostContinue == TRUE ){` |
|        - |  487 | `				JumpFixup sJumpFix;` |
|        - |  488 | `				/* Post-continue */` |
|    15949 |  489 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|    15949 |  490 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|    15949 |  491 | `				sJumpFix.pContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    15949 |  492 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|     7961 |  493 | `			}` |
|        - |  494 | `		}` |
|        - |  495 | `	}` |
|    39841 |  496 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0` |
|    19898 |  497 | `	 && PH7_GenSyntaxError(&(*pGen),pGen->pIn,"\";\"") == SXERR_ABORT ){` |
|      ! 0 |  498 | `		return SXERR_ABORT;` |
|        - |  499 | `	}` |
|        - |  500 | `	/* Statement successfully compiled */` |
|    39846 |  501 | `	return SXRET_OK;` |
|    19902 |  502 | `}` |
|        - |  503 | `/*` |
|        - |  504 | ` * Compile the 'break' statement.` |
|        - |  505 | ` * According to the PHP language reference` |
|        - |  506 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|        - |  507 | ` *  structure.` |
|        - |  508 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|        - |  509 | ` *  enclosing structures are to be broken out of.` |
|        - |  510 | ` */` |
|    16478 |  511 | `PH7_PRIVATE sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|        5 |  512 | `{` |
|        - |  513 | `	GenBlock *pLoop; /* Target loop */` |
|        - |  514 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|        - |  515 | `	sxi32 iRawLevel; /* The level as WRITTEN, kept for php's diagnostics */` |
|        - |  516 | `	sxu32 nLineLocal;` |
|        - |  517 | `	sxi32 rc;` |
|    16483 |  518 | `	iLevel = 0;` |
|    16483 |  519 | `	iRawLevel = 1;` |
|    16483 |  520 | `	nLineLocal = pGen->pIn->nLine;` |
|        - |  521 | `	/* Jump the 'break' keyword */` |
|    16483 |  522 | `	pGen->pIn++;` |
|    16483 |  523 | `	rc = GenStateJumpLevelArg(&(*pGen),"break",nLineLocal,&iRawLevel);` |
|    16483 |  524 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  525 | `		return SXERR_ABORT;` |
|    16483 |  526 | `	}else if( rc != SXRET_OK ){` |
|       31 |  527 | `		return SXRET_OK; /* Refused and reported */` |
|        - |  528 | `	}` |
|    16453 |  529 | `	if( pGen->pIn >= pGen->pEnd && GenStateAtChunkEofStmt(pGen) ){` |
|        - |  530 | ``		/* As for `continue` above: the missing semicolon is what php's parser meets`` |
|        - |  531 | `		 * first, before it can ask which loop this breaks out of. */` |
|        2 |  532 | `		rc = PH7_GenSyntaxError(&(*pGen),0,"\";\"");` |
|        2 |  533 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - |  534 | `	}` |
|    16451 |  535 | `	iLevel = iRawLevel < 2 ? 0 : iRawLevel;` |
|        - |  536 | `	/* Extract the target loop */` |
|    16451 |  537 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    16451 |  538 | `	if( pLoop == 0 ){` |
|        - |  539 | `		/* php distinguishes "there is no loop at all here" from "there is one, but` |
|        - |  540 | `		 * not N of them": the first keeps the not-in-context wording, the second is` |
|        - |  541 | ``		 * `Cannot 'break' N levels`. */`` |
|       21 |  542 | `		if( GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,0) != 0 ){` |
|        5 |  543 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        4 |  544 | `				"Cannot 'break' %d level%s",iRawLevel,iRawLevel == 1 ? "" : "s");` |
|        3 |  545 | `		}else{` |
|       17 |  546 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"'break' not in the 'loop' or 'switch' context");` |
|        - |  547 | `		}` |
|       21 |  548 | `		if( rc == SXERR_ABORT ){` |
|        - |  549 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  550 | `			return SXERR_ABORT;` |
|        - |  551 | `		}` |
|       12 |  552 | `	}else{` |
|        - |  553 | `		sxu32 nInstrIdx;` |
|    16433 |  554 | `		sxi32 iP1 = 0;` |
|        - |  555 | `		GenJumpScope sCross;` |
|    16433 |  556 | `		sxi32 iJmpOp = GenStateLoopJumpOp(&(*pGen),pLoop,&iP1,&sCross);` |
|    16433 |  557 | `		if( sCross.nFinally > 0 ){` |
|        9 |  558 | `			if( GenStateJumpOutOfFinally(&(*pGen),nLineLocal) == SXERR_ABORT ){` |
|      ! 0 |  559 | `				return SXERR_ABORT;` |
|        - |  560 | `			}` |
|        6 |  561 | `		}else{` |
|    16427 |  562 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,iP1,0,0,&nInstrIdx);` |
|    16427 |  563 | `			if( rc == SXRET_OK ){` |
|        - |  564 | `				/* Fix the jump later when the jump destination is resolved */` |
|    16427 |  565 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|     8200 |  566 | `			}` |
|        - |  567 | `		}` |
|        - |  568 | `	}` |
|    16446 |  569 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0` |
|     8217 |  570 | `	 && PH7_GenSyntaxError(&(*pGen),pGen->pIn,"\";\"") == SXERR_ABORT ){` |
|      ! 0 |  571 | `		return SXERR_ABORT;` |
|        - |  572 | `	}` |
|        - |  573 | `	/* Statement successfully compiled */` |
|    16451 |  574 | `	return SXRET_OK;` |
|     8233 |  575 | `}` |
|        - |  576 | `/*` |
|        - |  577 | ` * The function body a goto or a label sits in, or NULL at file scope. A goto may not` |
|        - |  578 | ` * cross functions, so GenStateFixGoto pairs the two on this.` |
|        - |  579 | ` */` |
|      446 |  580 | `static ph7_vm_func * GenStateOwningFunc(ph7_gen_state *pGen)` |
|        5 |  581 | `{` |
|      451 |  582 | `	GenBlock *pBlock = pGen->pCurrent;` |
|     1137 |  583 | `	while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|      691 |  584 | `		pBlock = pBlock->pParent;` |
|        5 |  585 | `	}` |
|      451 |  586 | `	return pBlock ? (ph7_vm_func *)pBlock->pUserData : 0;` |
|        5 |  587 | `}` |
|        - |  588 | `/*` |
|        - |  589 | ` * Compile or record a label.` |
|        - |  590 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|        - |  591 | ` * Example` |
|        - |  592 | ` *  goto LABEL;` |
|        - |  593 | ` *   echo 'Foo';` |
|        - |  594 | ` *  LABEL:` |
|        - |  595 | ` *   echo 'Bar';` |
|        - |  596 | ` */` |
|      220 |  597 | `PH7_PRIVATE sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|        5 |  598 | `{` |
|        - |  599 | `	Label sLabel;` |
|        - |  600 | `	/* php places almost NO restriction on where a label may be DEFINED — inside a loop, a` |
|        - |  601 | `	 * switch or a try{} is all fine; the one rule is that a name may not be declared twice` |
|        - |  602 | `	 * in the same function (below). The rest is on the jump: you may not goto INTO a loop` |
|        - |  603 | `	 * or switch from outside it, which is checked once the labels are all known (see` |
|        - |  604 | `	 * GenStateFixJumps). Record the loop this label sits in so that check can run. */` |
|        - |  605 | `	{` |
|      225 |  606 | `		SyString *pTarget = &pGen->pIn->sData;` |
|      225 |  607 | `		ph7_vm_func *pFunc = GenStateOwningFunc(&(*pGen));` |
|        - |  608 | `		char *zDup;` |
|        - |  609 | `		/* One name, one destination: php compile-rejects a label its function already` |
|        - |  610 | ``		 * declares, wherever the two sit (`L: L:`, one per branch of an if, one in a loop`` |
|        - |  611 | `		 * and one after it). PHL used to accept the redeclaration and silently give every` |
|        - |  612 | `		 * goto the FIRST one. The owning function is part of the key, so the same name in` |
|        - |  613 | `		 * another function — or at file scope beside it — is untouched by this. On the` |
|        - |  614 | `		 * duplicate, keep the first declaration and record nothing: the compile has already` |
|        - |  615 | `		 * failed, and a second entry under the same key would only shadow it. */` |
|      225 |  616 | `		if( SXRET_OK == GenStateGetLabel(&(*pGen),pTarget,pFunc,0) ){` |
|        8 |  617 | `			if( SXERR_ABORT == PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        2 |  618 | `				"Label '%z' already defined",pTarget) ){` |
|      ! 0 |  619 | `				return SXERR_ABORT;` |
|        - |  620 | `			}` |
|        6 |  621 | `			pGen->pIn += 2; /* Jump the label name and the semi-colon */` |
|        6 |  622 | `			return SXRET_OK;` |
|        - |  623 | `		}` |
|        - |  624 | `		/* Initialize label fields */` |
|      221 |  625 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|        - |  626 | `		/* Duplicate label name */` |
|      221 |  627 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|      221 |  628 | `		if( zDup == 0 ){` |
|      ! 0 |  629 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  630 | `			return SXERR_ABORT;` |
|        - |  631 | `		}` |
|      221 |  632 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|      221 |  633 | `		sLabel.nLine = pGen->pIn->nLine;` |
|      221 |  634 | `		sLabel.nLoopId = pGen->nCurLoopId;` |
|        - |  635 | `		/* Where the label sits, so a goto from a detached catch/finally body can be told` |
|        - |  636 | `		 * what it has to cross to reach it (GenStateJumpScope). */` |
|      221 |  637 | `		sLabel.pContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      221 |  638 | `		sLabel.nScopeId = pGen->nCurScopeId;` |
|        - |  639 | `		/* The owning FUNCTION, matched against the goto's in GenStateFixGoto. This used` |
|        - |  640 | `		 * to stop at GEN_BLOCK_EXCEPTION as well, which attributed every label inside a` |
|        - |  641 | `		 * try or catch body to "no function" — so from anywhere in a function such a` |
|        - |  642 | `		 * label read as undefined, including from the very catch body declaring it.` |
|        - |  643 | `		 * Whether a label may be jumped TO is decided by its container, not by this. */` |
|      221 |  644 | `		sLabel.pFunc = pFunc;` |
|        - |  645 | `		/* Insert in label set */` |
|      221 |  646 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|        - |  647 | `	}` |
|      221 |  648 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|      221 |  649 | `	return SXRET_OK;` |
|      115 |  650 | `}` |
|        - |  651 | `/*` |
|        - |  652 | ` * Compile the so hated 'goto' statement.` |
|        - |  653 | ` * You've probably been taught that gotos are bad, but this sort` |
|        - |  654 | ` * of rewriting  happens all the time, in fact every time you run` |
|        - |  655 | ` * a compiler it has to do this.` |
|        - |  656 | ` * According to the PHP language reference manual` |
|        - |  657 | ` *   The goto operator can be used to jump to another section in the program.` |
|        - |  658 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|        - |  659 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|        - |  660 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|        - |  661 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|        - |  662 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|        - |  663 | ` *   of a multi-level break` |
|        - |  664 | ` */` |
|      230 |  665 | `PH7_PRIVATE sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|        5 |  666 | `{` |
|        - |  667 | `	JumpFixup sJump;` |
|        - |  668 | `	sxi32 rc;` |
|      235 |  669 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|      235 |  670 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - |  671 | `		/* Missing label */` |
|      ! 0 |  672 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|      ! 0 |  673 | `		if( rc == SXERR_ABORT ){` |
|        - |  674 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  675 | `			return SXERR_ABORT;` |
|        - |  676 | `		}` |
|      ! 0 |  677 | `		return SXRET_OK;` |
|        - |  678 | `	}` |
|      235 |  679 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|        6 |  680 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn,"identifier");` |
|        6 |  681 | `		if( rc == SXERR_ABORT ){` |
|        - |  682 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  683 | `			return SXERR_ABORT;` |
|        - |  684 | `		}` |
|        4 |  685 | `	}else{` |
|      231 |  686 | `		SyString *pTarget = &pGen->pIn->sData;` |
|        - |  687 | `		char *zDup;` |
|        - |  688 | `		/* Prepare the jump destination */` |
|      231 |  689 | `		sJump.nJumpType = PH7_OP_JMP;` |
|      231 |  690 | `		sJump.nLine = pGen->pIn->nLine;` |
|        - |  691 | `		/* Gotos resolve at end of compilation, well after any container swap. */` |
|      231 |  692 | `		sJump.pContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      231 |  693 | `		sJump.nScopeId = pGen->nCurScopeId;` |
|        - |  694 | `		/* Duplicate label name */` |
|      231 |  695 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|      231 |  696 | `		if( zDup == 0 ){` |
|      ! 0 |  697 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  698 | `			return SXERR_ABORT;` |
|        - |  699 | `		}` |
|      231 |  700 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|        - |  701 | `		/* The loop/switch this goto sits in, for the "goto into a loop" check later. */` |
|      231 |  702 | `		sJump.nLoopId = pGen->nCurLoopId;` |
|        - |  703 | `		/* A goto inside a try{}/catch{} is legal php (jumping OUT of the block is fine);` |
|        - |  704 | `		 * only the owning function matters here, since a goto may not cross functions. */` |
|      231 |  705 | `		sJump.pFunc = GenStateOwningFunc(&(*pGen));` |
|        - |  706 | `		/* Emit the unconditional jump. Inside a DETACHED catch/finally body the target may` |
|        - |  707 | `		 * lie in another bytecode array, which a plain OP_JMP cannot address; enclosing` |
|        - |  708 | `		 * trys likewise need their finally run on the way out, which a plain jump would` |
|        - |  709 | `		 * skip. Emit a structure-crossing jump whenever either is possible — the label is` |
|        - |  710 | `		 * not known yet, so GenStateFixGoto picks the final opcode (and may downgrade it` |
|        - |  711 | `		 * back to OP_JMP once the counts prove to cancel). */` |
|      344 |  712 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,` |
|      226 |  713 | `			sJump.nScopeId > 0 ? PH7_OP_CATCH_JMP : PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|      231 |  714 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|      113 |  715 | `		}` |
|        - |  716 | `	}` |
|      235 |  717 | `	pGen->pIn++; /* Jump the label name */` |
|        - |  718 | ``	/* php reads `goto LABEL ;` and nothing else: a stray token there is its parse`` |
|        - |  719 | `	 * error naming the token, where this said so in a sentence of its own. */` |
|      230 |  720 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0` |
|      121 |  721 | `	 && PH7_GenSyntaxError(&(*pGen),pGen->pIn,"\";\"") == SXERR_ABORT ){` |
|      ! 0 |  722 | `		return SXERR_ABORT;` |
|        - |  723 | `	}` |
|        - |  724 | `	/* Statement successfully compiled */` |
|      235 |  725 | `	return SXRET_OK;` |
|      120 |  726 | `}` |
|        - |  727 | `/*` |
|        - |  728 | ` * Point to the next PHP chunk that will be processed shortly.` |
|        - |  729 | ` * Return SXRET_OK on success. Any other return value indicates` |
|        - |  730 | ` * failure.` |
|        - |  731 | ` */` |
|       30 |  732 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|        2 |  733 | `{` |
|        - |  734 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|        - |  735 | `	sxu32 nRawObj;` |
|       15 |  736 | `	sxu32 nObjIdx;` |
|        - |  737 | `	/* Consume raw chunks verbatim without any processing until we get` |
|        - |  738 | `	 * a PHP block.` |
|        - |  739 | `	 */` |
|        2 |  740 | `Consume:` |
|       36 |  741 | `	nRawObj = nObjIdx = 0;` |
|       48 |  742 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|       13 |  743 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|       13 |  744 | `		if( pRawObj == 0 ){` |
|      ! 0 |  745 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  746 | `			return SXERR_ABORT;` |
|        - |  747 | `		}` |
|        - |  748 | `		/* Mark as constant and emit the load constant instruction */` |
|       13 |  749 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|       13 |  750 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|       13 |  751 | `		++nRawObj;` |
|       13 |  752 | `		pGen->pRawIn++; /* Next chunk */` |
|        1 |  753 | `	}` |
|       36 |  754 | `	if( nRawObj > 0 ){` |
|        - |  755 | `		/* Emit the consume instruction */` |
|       13 |  756 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|        6 |  757 | `	}` |
|       36 |  758 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|       13 |  759 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|        - |  760 | `		/* Reset the token set (and its trivia sidecar) */` |
|       13 |  761 | `		SySetReset(pTokenSet);` |
|       13 |  762 | `		SySetReset(&pGen->aTrivia);` |
|        - |  763 | `		/* Tokenize input */` |
|       19 |  764 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|       12 |  765 | `			pGen->pRawIn->nLine,pTokenSet,&pGen->aTrivia);` |
|        - |  766 | `		/* Point to the fresh token stream */` |
|       13 |  767 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|       13 |  768 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|        - |  769 | `		/* Advance the stream cursor */` |
|       13 |  770 | `		pGen->pRawIn++;` |
|       13 |  771 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - |  772 | ``			/* The chunk held no TOKENS. `<?php // note ?>` is a whole PHP block that`` |
|        - |  773 | `			 * produces none, and so is a block holding only a comment of any kind --` |
|        - |  774 | `			 * the lexer emits the chunk (its bytes are not empty) and tokenizing it` |
|        - |  775 | `			 * yields nothing. Handing that empty stream back reads as END OF INPUT to` |
|        - |  776 | ``			 * every caller, so the enclosing block ended there and the real `}` two`` |
|        - |  777 | `			 * lines later was "Unmatched". A php TEMPLATE writes exactly this shape --` |
|        - |  778 | ``			 * `<?php } else { ?>` … `<?php // why ?>` … `<?php } ?>` is symfony's`` |
|        - |  779 | `			 * error-handler view -- so take the NEXT chunk instead. The cursor has` |
|        - |  780 | `			 * already advanced, so the loop always makes progress. */` |
|        5 |  781 | `			goto Consume;` |
|        - |  782 | `		}` |
|        - |  783 | `		/* TICKET 1433-011 */` |
|        9 |  784 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|        - |  785 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|        - |  786 | `			sxi32 rc;` |
|        - |  787 | `			/* Refer to TICKET 1433-009  */` |
|      ! 0 |  788 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|      ! 0 |  789 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|      ! 0 |  790 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|        - |  791 | `			/* Synthesized short-tag echo: legitimately an expression here. */` |
|      ! 0 |  792 | `			pGen->nExprEchoOk++;` |
|      ! 0 |  793 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|      ! 0 |  794 | `			pGen->nExprEchoOk--;` |
|      ! 0 |  795 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  796 | `				return SXERR_ABORT;` |
|      ! 0 |  797 | `			}else if( rc != SXERR_EMPTY ){` |
|      ! 0 |  798 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      ! 0 |  799 | `			}` |
|      ! 0 |  800 | `			goto Consume;` |
|        - |  801 | `		}` |
|        5 |  802 | `	}else{` |
|        - |  803 | `		/* No more chunks to process */` |
|       24 |  804 | `		pGen->pIn = pGen->pEnd;` |
|       24 |  805 | `		return SXERR_EOF;` |
|        - |  806 | `	}` |
|        9 |  807 | `	return SXRET_OK;` |
|       17 |  808 | `}` |
|        - |  809 | `/*` |
|        - |  810 | ` * Compile a PHP block.` |
|        - |  811 | ` * A block is simply one or more PHP statements and expressions to compile` |
|        - |  812 | ` * optionally delimited by braces {}.` |
|        - |  813 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|        - |  814 | ` * and this function takes care of generating the appropriate error` |
|        - |  815 | ` * message.` |
|        - |  816 | ` */` |
|  1020740 |  817 | `PH7_PRIVATE sxi32 PH7_CompileBlock(` |
|        - |  818 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |  819 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|        - |  820 | `	)` |
|        5 |  821 | `{` |
|        - |  822 | `	sxi32 rc;` |
|        - |  823 | `	sxu32 nLine;` |
|  1020745 |  824 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  1019173 |  825 | `		nLine = pGen->pIn->nLine;` |
|  1019173 |  826 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  1019173 |  827 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  828 | `			return SXERR_ABORT;` |
|        - |  829 | `		}` |
|  1019173 |  830 | `		pGen->pIn++;` |
|        - |  831 | `		/* Compile until we hit the closing braces '}' */` |
|  1575382 |  832 | `		for(;;){` |
|  3155308 |  833 | `			if( pGen->pIn >= pGen->pEnd ){` |
|       32 |  834 | `				rc = GenStateNextChunk(&(*pGen));` |
|       32 |  835 | `				if (rc == SXERR_ABORT ){` |
|      ! 0 |  836 | `			 	   return SXERR_ABORT;` |
|        - |  837 | `				}` |
|       32 |  838 | `				if( rc == SXERR_EOF ){` |
|        - |  839 | `					/* No more token to process: the block was never closed. php reports` |
|        - |  840 | `					 * the line the '{' was opened on, not where the input ran out. */` |
|       24 |  841 | `					PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"Unclosed '{' on line %u",nLine);` |
|       24 |  842 | `					break;` |
|        - |  843 | `				}` |
|        4 |  844 | `			}` |
|  3155286 |  845 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|        - |  846 | `				/* Closing braces found,break immediately*/` |
|  1019147 |  847 | `				pGen->pIn++;` |
|  1019147 |  848 | `				break;` |
|        - |  849 | `			}` |
|        - |  850 | `			/* Compile a single statement */` |
|  2136144 |  851 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  2136144 |  852 | `			if( rc == SXERR_ABORT ){` |
|        5 |  853 | `				return SXERR_ABORT;` |
|        - |  854 | `			}` |
|        5 |  855 | `		}` |
|  1019169 |  856 | `		GenStateLeaveBlock(&(*pGen),0);` |
|   510431 |  857 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|       19 |  858 | `		pGen->pIn++;` |
|       19 |  859 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|       19 |  860 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  861 | `			return SXERR_ABORT;` |
|        - |  862 | `		}` |
|        - |  863 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|       18 |  864 | `		for(;;){` |
|       37 |  865 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  866 | `				rc = GenStateNextChunk(&(*pGen));` |
|      ! 0 |  867 | `				if (rc == SXERR_ABORT ){` |
|      ! 0 |  868 | `			 	   return SXERR_ABORT;` |
|        - |  869 | `				}` |
|      ! 0 |  870 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|        - |  871 | `					/* No more token to process */` |
|      ! 0 |  872 | `					if( rc == SXERR_EOF ){` |
|      ! 0 |  873 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|        - |  874 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|      ! 0 |  875 | `					}` |
|      ! 0 |  876 | `					break;` |
|        - |  877 | `				}` |
|      ! 0 |  878 | `			}` |
|       37 |  879 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|        - |  880 | `				sxi32 nKwrd;` |
|        - |  881 | `				/* Keyword found */` |
|       35 |  882 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       39 |  883 | `				if( nKwrd == nKeywordEnd \|\|` |
|       14 |  884 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|        - |  885 | `						/* Delimiter keyword found,break */` |
|       19 |  886 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|       17 |  887 | `							pGen->pIn++; /*  endif;endswitch... */` |
|        8 |  888 | `						}` |
|       19 |  889 | `						break;` |
|        - |  890 | `				}` |
|        8 |  891 | `			}` |
|        - |  892 | `			/* Compile a single statement */` |
|       19 |  893 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|       19 |  894 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  895 | `				return SXERR_ABORT;` |
|        - |  896 | `			}` |
|        1 |  897 | `		}` |
|       19 |  898 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       10 |  899 | `	}else{` |
|        - |  900 | `		/* Compile a single statement */` |
|     1559 |  901 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     1559 |  902 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  903 | `			return SXERR_ABORT;` |
|        - |  904 | `		}` |
|        - |  905 | `	}` |
|        - |  906 | `	/* Jump trailing semi-colons ';' */` |
|  1020753 |  907 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       13 |  908 | `		pGen->pIn++;` |
|        1 |  909 | `	}` |
|  1020741 |  910 | `	return SXRET_OK;` |
|   509647 |  911 | `}` |
|        - |  912 | `/*` |
|        - |  913 | ` * Compile the gentle 'while' statement.` |
|        - |  914 | ` * According to the PHP language reference` |
|        - |  915 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|        - |  916 | ` *  The basic form of a while statement is:` |
|        - |  917 | ` *  while (expr)` |
|        - |  918 | ` *   statement` |
|        - |  919 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|        - |  920 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|        - |  921 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|        - |  922 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|        - |  923 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|        - |  924 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|        - |  925 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|        - |  926 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|        - |  927 | ` *  while (expr):` |
|        - |  928 | ` *    statement` |
|        - |  929 | ` *   endwhile;` |
|        - |  930 | ` */` |
|    16396 |  931 | `PH7_PRIVATE sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|        5 |  932 | `{` |
|    16401 |  933 | `	GenBlock *pWhileBlock = 0;` |
|    16401 |  934 | `	SyToken *pTmp,*pEnd = 0;` |
|        - |  935 | `	sxu32 nFalseJump;` |
|        - |  936 | `	sxu32 nLine;` |
|        - |  937 | `	sxi32 rc;` |
|    16401 |  938 | `	nLine = pGen->pIn->nLine;` |
|        - |  939 | `	/* Jump the 'while' keyword */` |
|    16401 |  940 | `	pGen->pIn++;` |
|    16401 |  941 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  942 | `		/* Syntax error */` |
|      ! 0 |  943 | `		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Expected '(' after 'while' keyword");` |
|      ! 0 |  944 | `		if( rc == SXERR_ABORT ){` |
|        - |  945 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  946 | `			return SXERR_ABORT;` |
|        - |  947 | `		}` |
|      ! 0 |  948 | `		goto Synchronize;` |
|        - |  949 | `	}` |
|        - |  950 | `	/* Jump the left parenthesis '(' */` |
|    16401 |  951 | `	pGen->pIn++;` |
|        - |  952 | `	/* Create the loop block */` |
|    16401 |  953 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|    16401 |  954 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  955 | `		return SXERR_ABORT;` |
|        - |  956 | `	}` |
|        - |  957 | `	/* Delimit the condition */` |
|    16401 |  958 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    16401 |  959 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  960 | `		/* Empty condition. php reports the token that actually stopped it -- for` |
|        - |  961 | ``		 * `while ()` that is the ')' -- not a hand-written "expected expression". */`` |
|        3 |  962 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,0);` |
|        3 |  963 | `		if( rc == SXERR_ABORT ){` |
|        - |  964 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  965 | `			return SXERR_ABORT;` |
|        - |  966 | `		}` |
|        1 |  967 | `	}` |
|        - |  968 | `	/* Swap token streams */` |
|    16401 |  969 | `	pTmp = pGen->pEnd;` |
|    16401 |  970 | `	pGen->pEnd = pEnd;` |
|        - |  971 | `	/* Compile the expression */` |
|    16401 |  972 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    16401 |  973 | `	if( rc == SXERR_ABORT ){` |
|        - |  974 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  975 | `		return SXERR_ABORT;` |
|        - |  976 | `	}` |
|        - |  977 | `	/* Update token stream */` |
|    16401 |  978 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 |  979 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 |  980 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  981 | `			return SXERR_ABORT;` |
|        - |  982 | `		}` |
|      ! 0 |  983 | `		pGen->pIn++;` |
|      ! 0 |  984 | `	}` |
|        - |  985 | `	/* Synchronize pointers */` |
|    16401 |  986 | `	pGen->pIn  = &pEnd[1];` |
|    16401 |  987 | `	pGen->pEnd = pTmp;` |
|        - |  988 | `	/* Emit the false jump */` |
|    16401 |  989 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|        - |  990 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    16401 |  991 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|        - |  992 | `	/* Compile the loop body */` |
|    16401 |  993 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|    16401 |  994 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  995 | `		return SXERR_ABORT;` |
|        - |  996 | `	}` |
|        - |  997 | `	/* Emit the unconditional jump to the start of the loop */` |
|    16401 |  998 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|        - |  999 | `	/* Fix all jumps now the destination is resolved */` |
|    16401 | 1000 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 1001 | `	/* Release the loop block */` |
|    16401 | 1002 | `	GenStateLeaveBlock(pGen,0);` |
|        - | 1003 | `	/* Statement successfully compiled */` |
|    16401 | 1004 | `	return SXRET_OK;` |
|      ! 0 | 1005 | `Synchronize:` |
|        - | 1006 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - | 1007 | `	 * compiling this erroneous block.` |
|        - | 1008 | `	 */` |
|      ! 0 | 1009 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 | 1010 | `		pGen->pIn++;` |
|      ! 0 | 1011 | `	}` |
|      ! 0 | 1012 | `	return SXRET_OK;` |
|     8192 | 1013 | `}` |
|        - | 1014 | `/*` |
|        - | 1015 | ` * Compile the ugly do..while() statement.` |
|        - | 1016 | ` * According to the PHP language reference` |
|        - | 1017 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|        - | 1018 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|        - | 1019 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|        - | 1020 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|        - | 1021 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|        - | 1022 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|        - | 1023 | ` *  would end immediately).` |
|        - | 1024 | ` *  There is just one syntax for do-while loops:` |
|        - | 1025 | ` *  <?php` |
|        - | 1026 | ` *  $i = 0;` |
|        - | 1027 | ` *  do {` |
|        - | 1028 | ` *   echo $i;` |
|        - | 1029 | ` *  } while ($i > 0);` |
|        - | 1030 | ` * ?>` |
|        - | 1031 | ` */` |
|       12 | 1032 | `PH7_PRIVATE sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|        3 | 1033 | `{` |
|       15 | 1034 | `	SyToken *pTmp,*pEnd = 0;` |
|       15 | 1035 | `	GenBlock *pDoBlock = 0;` |
|        - | 1036 | `	sxu32 nLine;` |
|        - | 1037 | `	sxi32 rc;` |
|       15 | 1038 | `	nLine = pGen->pIn->nLine;` |
|        - | 1039 | `	/* Jump the 'do' keyword */` |
|       15 | 1040 | `	pGen->pIn++;` |
|        - | 1041 | `	/* Create the loop block */` |
|       15 | 1042 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|       15 | 1043 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1044 | `		return SXERR_ABORT;` |
|        - | 1045 | `	}` |
|        - | 1046 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|       15 | 1047 | `	pDoBlock->bPostContinue = TRUE;` |
|       15 | 1048 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       15 | 1049 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 1050 | `		return SXERR_ABORT;` |
|        - | 1051 | `	}` |
|       15 | 1052 | `	if( pGen->pIn < pGen->pEnd ){` |
|       13 | 1053 | `		nLine = pGen->pIn->nLine;` |
|        5 | 1054 | `	}` |
|       15 | 1055 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|       10 | 1056 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|        - | 1057 | `			/* Missing 'while' statement */` |
|        - | 1058 | ``			/* php: `do {} ;` names the ';', while `do {}` at EOF is "unexpected end of`` |
|        - | 1059 | `			 * file". The do-block's own slice stops before its terminator, so look at` |
|        - | 1060 | `			 * the whole CHUNK stream: a token still there is the one php names; nothing` |
|        - | 1061 | `			 * left means end of file (NULL). */` |
|        - | 1062 | `			{` |
|        3 | 1063 | `				SyToken *pBad = pGen->pIn < pGen->pEnd ? pGen->pIn : 0;` |
|        3 | 1064 | `				if( pBad == 0 && pGen->pTokenSet ){` |
|        - | 1065 | `					/* The do-block consumed its terminator, so the token php names is` |
|        - | 1066 | `					 * the one just behind the cursor -- but only when it is a real` |
|        - | 1067 | ``					 * terminator (`do {} ;` -> the ';'). If the block simply ended on`` |
|        - | 1068 | `					 * its '}' with nothing after it, php reports end of file. */` |
|        3 | 1069 | `					SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        3 | 1070 | `					if( pGen->pIn > pBase && (pGen->pIn[-1].nType & PH7_TK_SEMI) ){` |
|      ! 0 | 1071 | `						pBad = &pGen->pIn[-1];` |
|      ! 0 | 1072 | `					}` |
|        1 | 1073 | `				}` |
|        3 | 1074 | `				rc = PH7_GenSyntaxError(pGen,pBad,"\"while\"");` |
|        - | 1075 | `			}` |
|        3 | 1076 | `			if( rc == SXERR_ABORT ){` |
|        - | 1077 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 1078 | `				return SXERR_ABORT;` |
|        - | 1079 | `			}` |
|        3 | 1080 | `			goto Synchronize;` |
|        - | 1081 | `	}` |
|        - | 1082 | `	/* Jump the 'while' keyword */` |
|       13 | 1083 | `	pGen->pIn++;` |
|       13 | 1084 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1085 | `		/* Syntax error */` |
|      ! 0 | 1086 | `		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Expected '(' after 'while' keyword");` |
|      ! 0 | 1087 | `		if( rc == SXERR_ABORT ){` |
|        - | 1088 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1089 | `			return SXERR_ABORT;` |
|        - | 1090 | `		}` |
|      ! 0 | 1091 | `		goto Synchronize;` |
|        - | 1092 | `	}` |
|        - | 1093 | `	/* Jump the left parenthesis '(' */` |
|       13 | 1094 | `	pGen->pIn++;` |
|        - | 1095 | `	/* Delimit the condition */` |
|       13 | 1096 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|       13 | 1097 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - | 1098 | `		/* Empty condition. php reports the token that actually stopped it -- for` |
|        - | 1099 | ``		 * `while ()` that is the ')' -- not a hand-written "expected expression". */`` |
|      ! 0 | 1100 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,0);` |
|      ! 0 | 1101 | `		if( rc == SXERR_ABORT ){` |
|        - | 1102 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1103 | `			return SXERR_ABORT;` |
|        - | 1104 | `		}` |
|      ! 0 | 1105 | `		goto Synchronize;` |
|        - | 1106 | `	}` |
|        - | 1107 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|       13 | 1108 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|        - | 1109 | `		JumpFixup *aPost;` |
|        - | 1110 | `		VmInstr *pInstr;` |
|        - | 1111 | `		sxu32 nJumpDest;` |
|        - | 1112 | `		sxu32 n;` |
|        3 | 1113 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|        3 | 1114 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|        5 | 1115 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|        3 | 1116 | `			pInstr = GenStateFixupInstr(&aPost[n]);` |
|        3 | 1117 | `			if( pInstr ){` |
|        - | 1118 | `				/* Fix */` |
|        3 | 1119 | `				pInstr->iP2 = nJumpDest;` |
|        1 | 1120 | `			}` |
|        2 | 1121 | `		}` |
|        1 | 1122 | `	}` |
|        - | 1123 | `	/* Swap token streams */` |
|       13 | 1124 | `	pTmp = pGen->pEnd;` |
|       13 | 1125 | `	pGen->pEnd = pEnd;` |
|        - | 1126 | `	/* Compile the expression */` |
|       13 | 1127 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       13 | 1128 | `	if( rc == SXERR_ABORT ){` |
|        - | 1129 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1130 | `		return SXERR_ABORT;` |
|        - | 1131 | `	}` |
|        - | 1132 | `	/* Update token stream */` |
|       13 | 1133 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 | 1134 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 | 1135 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1136 | `			return SXERR_ABORT;` |
|        - | 1137 | `		}` |
|      ! 0 | 1138 | `		pGen->pIn++;` |
|      ! 0 | 1139 | `	}` |
|       13 | 1140 | `	pGen->pIn  = &pEnd[1];` |
|       13 | 1141 | `	pGen->pEnd = pTmp;` |
|        - | 1142 | `	/* Emit the true jump to the beginning of the loop */` |
|       13 | 1143 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|        - | 1144 | `	/* Fix all jumps now the destination is resolved */` |
|       13 | 1145 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 1146 | `	/* Release the loop block */` |
|       13 | 1147 | `	GenStateLeaveBlock(pGen,0);` |
|        - | 1148 | `	/* Statement successfully compiled */` |
|       13 | 1149 | `	return SXRET_OK;` |
|        1 | 1150 | `Synchronize:` |
|        - | 1151 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - | 1152 | `	 * compiling this erroneous block.` |
|        - | 1153 | `	 */` |
|        3 | 1154 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 | 1155 | `		pGen->pIn++;` |
|      ! 0 | 1156 | `	}` |
|        3 | 1157 | `	return SXRET_OK;` |
|        9 | 1158 | `}` |
|        - | 1159 | `/*` |
|        - | 1160 | ` * Compile the complex and powerful 'for' statement.` |
|        - | 1161 | ` * According to the PHP language reference` |
|        - | 1162 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|        - | 1163 | ` *  The syntax of a for loop is:` |
|        - | 1164 | ` *  for (expr1; expr2; expr3)` |
|        - | 1165 | ` *   statement` |
|        - | 1166 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|        - | 1167 | ` *  the beginning of the loop.` |
|        - | 1168 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|        - | 1169 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|        - | 1170 | ` *  to FALSE, the execution of the loop ends.` |
|        - | 1171 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|        - | 1172 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|        - | 1173 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|        - | 1174 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|        - | 1175 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|        - | 1176 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|        - | 1177 | ` *  of using the for truth expression.` |
|        - | 1178 | ` */` |
|    48141 | 1179 | `PH7_PRIVATE sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|        5 | 1180 | `{` |
|    48146 | 1181 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|    48146 | 1182 | `	GenBlock *pForBlock = 0;` |
|        - | 1183 | `	sxu32 nFalseJump;` |
|        - | 1184 | `	sxu32 nLine;` |
|        - | 1185 | `	sxi32 rc;` |
|    48146 | 1186 | `	nLine = pGen->pIn->nLine;` |
|        - | 1187 | `	/* Jump the 'for' keyword */` |
|    48146 | 1188 | `	pGen->pIn++;` |
|    48146 | 1189 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1190 | `		/* Syntax error */` |
|      ! 0 | 1191 | `		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Expected '(' after 'for' keyword");` |
|      ! 0 | 1192 | `		if( rc == SXERR_ABORT ){` |
|        - | 1193 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1194 | `			return SXERR_ABORT;` |
|        - | 1195 | `		}` |
|      ! 0 | 1196 | `		return SXRET_OK;` |
|        - | 1197 | `	}` |
|        - | 1198 | `	/* Jump the left parenthesis '(' */` |
|    48146 | 1199 | `	pGen->pIn++;` |
|        - | 1200 | `	/* Delimit the init-expr;condition;post-expr */` |
|    48146 | 1201 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    48146 | 1202 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - | 1203 | `		/* Empty expression */` |
|      ! 0 | 1204 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|      ! 0 | 1205 | `		if( rc == SXERR_ABORT ){` |
|        - | 1206 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1207 | `			return SXERR_ABORT;` |
|        - | 1208 | `		}` |
|        - | 1209 | `		/* Synchronize */` |
|      ! 0 | 1210 | `		pGen->pIn = pEnd;` |
|      ! 0 | 1211 | `		if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 | 1212 | `			pGen->pIn++;` |
|      ! 0 | 1213 | `		}` |
|      ! 0 | 1214 | `		return SXRET_OK;` |
|        - | 1215 | `	}` |
|        - | 1216 | `	/* Swap token streams */` |
|    48146 | 1217 | `	pTmp = pGen->pEnd;` |
|    48146 | 1218 | `	pGen->pEnd = pEnd;` |
|        - | 1219 | `	/* for() clauses are the ONLY place php's grammar allows a comma-separated` |
|        - | 1220 | `	 * expression list, so the comma operator is permitted for their duration` |
|        - | 1221 | `	 * (see GenStateTreeHasComma). A closure body nested inside a clause is` |
|        - | 1222 | `	 * compiled through this same window — recorded as a known leniency. */` |
|    48146 | 1223 | `	pGen->nCommaExprOk++;` |
|    48146 | 1224 | `	pGen->zClauseCloser = "\";\""; /* init/condition clauses close on ';' */` |
|        - | 1225 | `` 	/* Compile initialization expressions if available. Every element of a `for` `` |
|        - | 1226 | ``	 * clause is a statement position in php, `(void)` cast included. */`` |
|    48146 | 1227 | `	GenStateEnableClauseVoidCasts(&(*pGen),1);` |
|    48146 | 1228 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 1229 | `	/* Pop operand lvalues */` |
|    48146 | 1230 | `	if( rc == SXERR_ABORT ){` |
|        - | 1231 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1232 | `		return SXERR_ABORT;` |
|    48146 | 1233 | `	}else if( rc != SXERR_EMPTY ){` |
|    48142 | 1234 | `		GenStateMarkDiscardedCall(&(*pGen));` |
|    48142 | 1235 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    24034 | 1236 | `	}` |
|    48146 | 1237 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - | 1238 | `		/* Syntax error */` |
|      ! 0 | 1239 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|      ! 0 | 1240 | `		if( rc == SXERR_ABORT ){` |
|        - | 1241 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1242 | `			return SXERR_ABORT;` |
|        - | 1243 | `		}` |
|      ! 0 | 1244 | `		return SXRET_OK;` |
|        - | 1245 | `	}` |
|        - | 1246 | `	/* Jump the trailing ';' */` |
|    48146 | 1247 | `	pGen->pIn++;` |
|        - | 1248 | `	/* Create the loop block */` |
|    48146 | 1249 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|    48146 | 1250 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1251 | `		return SXERR_ABORT;` |
|        - | 1252 | `	}` |
|        - | 1253 | `	/* Deffer continue jumps */` |
|    48146 | 1254 | `	pForBlock->bPostContinue = TRUE;` |
|        - | 1255 | `	/* Compile the condition */` |
|    48146 | 1256 | `	if( GenStateEnableClauseVoidCasts(&(*pGen),0) ){` |
|        - | 1257 | `		/* php names the clause terminator, not the cast, when the offending` |
|        - | 1258 | ``		 * `(void)` is on the element that has to BE the condition. */`` |
|        3 | 1259 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|        - | 1260 | `			"syntax error, unexpected token \";\", expecting \",\"");` |
|        3 | 1261 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1262 | `			return SXERR_ABORT;` |
|        - | 1263 | `		}` |
|        3 | 1264 | `		return SXRET_OK;` |
|        - | 1265 | `	}` |
|    48144 | 1266 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    48144 | 1267 | `	if( rc == SXERR_ABORT ){` |
|        - | 1268 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1269 | `		return SXERR_ABORT;` |
|    48144 | 1270 | `	}else if( rc != SXERR_EMPTY ){` |
|        - | 1271 | `		/* Emit the false jump */` |
|    48140 | 1272 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|        - | 1273 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    48140 | 1274 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    24033 | 1275 | `	}` |
|    48144 | 1276 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - | 1277 | `		/* Syntax error */` |
|        6 | 1278 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|        6 | 1279 | `		if( rc == SXERR_ABORT ){` |
|        - | 1280 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1281 | `			return SXERR_ABORT;` |
|        - | 1282 | `		}` |
|        6 | 1283 | `		return SXRET_OK;` |
|        - | 1284 | `	}` |
|        - | 1285 | `	/* Jump the trailing ';' */` |
|    48140 | 1286 | `	pGen->pIn++;` |
|        - | 1287 | `	/* Save the post condition stream */` |
|    48140 | 1288 | `	pPostStart = pGen->pIn;` |
|        - | 1289 | `	/* Compile the loop body — OUTSIDE the comma window (the body is ordinary` |
|        - | 1290 | ``	 * php, so `(1, 2)` inside it is the parse error it should be). */`` |
|    48140 | 1291 | `	pGen->nCommaExprOk--;` |
|    48140 | 1292 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|    48140 | 1293 | `	pGen->pEnd = pTmp;` |
|    48140 | 1294 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|    48140 | 1295 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 1296 | `		return SXERR_ABORT;` |
|        - | 1297 | `	}` |
|        - | 1298 | `	/* Fix post-continue jumps */` |
|    48140 | 1299 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|        - | 1300 | `		JumpFixup *aPost;` |
|        - | 1301 | `		VmInstr *pInstr;` |
|        - | 1302 | `		sxu32 nJumpDest;` |
|        - | 1303 | `		sxu32 n;` |
|    15893 | 1304 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|    15893 | 1305 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|    31835 | 1306 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|    15947 | 1307 | `			pInstr = GenStateFixupInstr(&aPost[n]);` |
|    15947 | 1308 | `			if( pInstr ){` |
|        - | 1309 | `				/* Fix jump */` |
|    15947 | 1310 | `				pInstr->iP2 = nJumpDest;` |
|     7960 | 1311 | `			}` |
|     7965 | 1312 | `		}` |
|     7933 | 1313 | `	}` |
|        - | 1314 | `	/* compile the post-expressions if available */` |
|    48140 | 1315 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|      ! 0 | 1316 | `		pPostStart++;` |
|      ! 0 | 1317 | `	}` |
|    48140 | 1318 | `	if( pPostStart < pEnd ){` |
|        - | 1319 | `		SyToken *pTmpIn,*pTmpEnd;` |
|    48138 | 1320 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|    48138 | 1321 | `		pGen->nCommaExprOk++; /* post-expressions are a clause list again */` |
|    48138 | 1322 | `		pGen->zClauseCloser = "\")\""; /* the post clause closes on ')' */` |
|    48138 | 1323 | `		GenStateEnableClauseVoidCasts(&(*pGen),1);` |
|    48138 | 1324 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    48138 | 1325 | `		pGen->nCommaExprOk--;` |
|    48138 | 1326 | `		pGen->zClauseCloser = 0;` |
|    48138 | 1327 | `		if( pGen->pIn < pGen->pEnd ){` |
|        - | 1328 | `			/* php names the token and expects ')'; the post-clause runs to the ')'. */` |
|      ! 0 | 1329 | `			rc = PH7_GenSyntaxError(pGen,pGen->pIn,"\")\"");` |
|      ! 0 | 1330 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1331 | `				return SXERR_ABORT;` |
|        - | 1332 | `			}` |
|      ! 0 | 1333 | `			return SXRET_OK;` |
|        - | 1334 | `		}` |
|    48138 | 1335 | `		RE_SWAP_DELIMITER(pGen);` |
|    48138 | 1336 | `		if( rc == SXERR_ABORT ){` |
|        - | 1337 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1338 | `			return SXERR_ABORT;` |
|    48138 | 1339 | `		}else if( rc != SXERR_EMPTY){` |
|    48138 | 1340 | `			GenStateMarkDiscardedCall(&(*pGen));` |
|        - | 1341 | `			/* Pop operand lvalue */` |
|    48138 | 1342 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    24032 | 1343 | `		}` |
|    24032 | 1344 | `	}` |
|        - | 1345 | `	/* Emit the unconditional jump to the start of the loop */` |
|    48140 | 1346 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|        - | 1347 | `	/* Fix all jumps now the destination is resolved */` |
|    48140 | 1348 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 1349 | `	/* Release the loop block */` |
|    48140 | 1350 | `	GenStateLeaveBlock(pGen,0);` |
|        - | 1351 | `	/* Statement successfully compiled */` |
|    48140 | 1352 | `	return SXRET_OK;` |
|    24041 | 1353 | `}` |
|        - | 1354 | `/*` |
|        - | 1355 | ` * Is this keyword token part of a NAME rather than a keyword in its own right?` |
|        - | 1356 | ` *` |
|        - | 1357 | `` * The lexer marks `as` a keyword wherever it appears, and php lets it BE a name`` |
|        - | 1358 | ` * in three places the token stream reaches through a preceding operator: a` |
|        - | 1359 | ` * variable ($as lexes as PH7_TK_DOLLAR followed by the identifier), a property` |
|        - | 1360 | ` * ($o->as, $o?->as) and a class constant (A::as). foreach's separator scan has` |
|        - | 1361 | ` * to look at what PRECEDES the keyword, or it splits inside the name and hands` |
|        - | 1362 | `` * the subject compiler a bare `$`.`` |
|        - | 1363 | ` */` |
|    91952 | 1364 | `static int GenStateKeywordIsName(SyToken *pStart,SyToken *pCur)` |
|        5 | 1365 | `{` |
|        - | 1366 | `	SyToken *pPrev;` |
|    91957 | 1367 | `	if( pCur <= pStart ){` |
|      ! 0 | 1368 | `		return 0;` |
|        - | 1369 | `	}` |
|    91957 | 1370 | `	pPrev = pCur - 1;` |
|    91957 | 1371 | `	if( pPrev->nType & PH7_TK_DOLLAR ){` |
|        6 | 1372 | `		return 1;` |
|        - | 1373 | `	}` |
|    91953 | 1374 | `	if( (pPrev->nType & PH7_TK_OP) == 0 ){` |
|    91945 | 1375 | `		return 0;` |
|        - | 1376 | `	}` |
|       13 | 1377 | `	return (pPrev->sData.nByte == sizeof("->")-1` |
|        6 | 1378 | `			&& SyMemcmp(pPrev->sData.zString,"->",sizeof("->")-1) == 0)` |
|        7 | 1379 | `		\|\| (pPrev->sData.nByte == sizeof("::")-1` |
|        3 | 1380 | `			&& SyMemcmp(pPrev->sData.zString,"::",sizeof("::")-1) == 0)` |
|       16 | 1381 | `		\|\| (pPrev->sData.nByte == sizeof("?->")-1` |
|        4 | 1382 | `			&& SyMemcmp(pPrev->sData.zString,"?->",sizeof("?->")-1) == 0);` |
|    45909 | 1383 | `}` |
|        - | 1384 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|        - | 1385 | ` *` |
|        - | 1386 | `` * php's `as` target is any WRITABLE expression, not just a variable: a property`` |
|        - | 1387 | ` * ($o->p), a static property (C::$s), an array element ($a['k']) and an append` |
|        - | 1388 | ` * ($a[]) are all accepted, on the key side as much as on the value side. The` |
|        - | 1389 | ` * three shapes php rejects get php's own wording; everything else keeps PH7's.` |
|        - | 1390 | ` */` |
|   108471 | 1391 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 1392 | `{` |
|   108476 | 1393 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|   108476 | 1394 | `	const char *zMsg = 0;` |
|        - | 1395 | ``	/* A loop target is a write target: `as $this` and `as (new A)->p` are php's`` |
|        - | 1396 | `	 * own compile fatals. */` |
|   108476 | 1397 | `	rc = GenStateWriteTargetCheck(&(*pGen),pRoot,0);` |
|   108476 | 1398 | `	if( rc != SXRET_OK ){` |
|        3 | 1399 | `		return rc;` |
|        - | 1400 | `	}` |
|   108474 | 1401 | `	if( pRoot->pOp ){` |
|       40 | 1402 | `		switch( pRoot->pOp->iOp ){` |
|       17 | 1403 | `		case EXPR_OP_ARROW:     /* $o->p */` |
|        - | 1404 | `		case EXPR_OP_SUBSCRIPT: /* $a['k'], $a[] */` |
|       35 | 1405 | `			return SXRET_OK;` |
|        2 | 1406 | ``		case EXPR_OP_DC:        /* C::$s — but `as C::K` is php's parse error */`` |
|        6 | 1407 | `			if( !PH7_ExprNodeIsClassConst(pRoot) ){` |
|        3 | 1408 | `				return SXRET_OK;` |
|        - | 1409 | `			}` |
|        3 | 1410 | `			break;` |
|      ! 0 | 1411 | `		case EXPR_OP_NULLSAFE_ARROW:` |
|      ! 0 | 1412 | `			zMsg = "Can't use nullsafe operator in write context";` |
|      ! 0 | 1413 | `			break;` |
|        - | 1414 | ``		/* A CALL target (`as f()`) never reaches here — GenStateWriteTargetCheck`` |
|        - | 1415 | `		 * above already refused it, and with php's function/method distinction. */` |
|      ! 0 | 1416 | `		default:` |
|      ! 0 | 1417 | `			break;` |
|        - | 1418 | `		}` |
|   108433 | 1419 | `	}else if( pRoot->xCode == PH7_CompileVariable ){` |
|   108436 | 1420 | `		return SXRET_OK;` |
|        - | 1421 | `	}` |
|        3 | 1422 | `	if( zMsg == 0 ){` |
|        - | 1423 | ``		/* Not a `variable` in php's grammar: php's own syntax error, which names`` |
|        - | 1424 | `		 * the token that follows the target. */` |
|        3 | 1425 | `		return PH7_ExprOperandNotAVariable(&(*pGen),pRoot);` |
|        - | 1426 | `	}` |
|      ! 0 | 1427 | `	rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,zMsg);` |
|      ! 0 | 1428 | `	if( rc != SXERR_ABORT ){` |
|      ! 0 | 1429 | `		rc = SXERR_INVALID;` |
|      ! 0 | 1430 | `	}` |
|      ! 0 | 1431 | `	return rc;` |
|    54155 | 1432 | `}` |
|        - | 1433 | `/*` |
|        - | 1434 | `` * Is this `as` target the plain `$name` shape?`` |
|        - | 1435 | ` *` |
|        - | 1436 | ` * Only that shape can be installed by NAME the way ph7_foreach_info records it` |
|        - | 1437 | ` * (the step writes straight into the frame's symbol table). Every other writable` |
|        - | 1438 | ` * target — including a variable-variable, whose name php re-evaluates per step —` |
|        - | 1439 | ` * goes through a synthetic temporary plus a real store (GenStateForeachStoreTarget).` |
|        - | 1440 | ` */` |
|   108471 | 1441 | `static int GenStateForeachTargetIsPlainVar(SyToken *pStart,SyToken *pEnd)` |
|        5 | 1442 | `{` |
|   162606 | 1443 | `	return (pEnd == &pStart[2])` |
|   108451 | 1444 | `		&& (pStart[0].nType & PH7_TK_DOLLAR)` |
|   162772 | 1445 | `		&& (pStart[1].nType & PH7_TK_ID);` |
|        5 | 1446 | `}` |
|        - | 1447 | `/*` |
|        - | 1448 | `` * Reserve the synthetic temporary a complex `as` target's step value lands in.`` |
|        - | 1449 | ` * The bracketed name cannot collide with a user variable — the same trick the` |
|        - | 1450 | ` * list()/[...] destructuring path uses.` |
|        - | 1451 | ` */` |
|      120 | 1452 | `static sxi32 GenStateForeachTempName(ph7_gen_state *pGen,const char *zTag,SyString *pOut)` |
|        4 | 1453 | `{` |
|        - | 1454 | `	static int iForeachTargetCnt = 0;` |
|        - | 1455 | `	char zTmp[128];` |
|        - | 1456 | `	sxu32 nLen;` |
|        - | 1457 | `	char *zDup;` |
|      124 | 1458 | `	nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_%s_%d__]",zTag,iForeachTargetCnt++);` |
|      124 | 1459 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      124 | 1460 | `	if( zDup == 0 ){` |
|      ! 0 | 1461 | `		return SXERR_ABORT;` |
|        - | 1462 | `	}` |
|      124 | 1463 | `	SyStringInitFromBuf(pOut,zDup,nLen);` |
|      124 | 1464 | `	return SXRET_OK;` |
|       64 | 1465 | `}` |
|        - | 1466 | `/*` |
|        - | 1467 | `` * Emit `<target> = <temp>` (or `<target> =& <temp>` for a by-reference value) for`` |
|        - | 1468 | `` * one complex `as` target, at the top of the loop body — where php performs the`` |
|        - | 1469 | ` * assignment, once per step.` |
|        - | 1470 | ` *` |
|        - | 1471 | ` * The store is folded exactly as the assignment operator's own codegen folds it` |
|        - | 1472 | ` * (compile.c, precedence-18 site): a member LHS keeps its OP_MEMBER, a subscript` |
|        - | 1473 | ` * becomes STORE_IDX, and a plain name folds into the STORE's p3.` |
|        - | 1474 | ` */` |
|      120 | 1475 | `static sxi32 GenStateForeachStoreTarget(` |
|        - | 1476 | `	ph7_gen_state *pGen,` |
|        - | 1477 | `	SyString *pTemp,   /* Synthetic variable holding this step's value/key */` |
|        - | 1478 | `	SyToken *pStart,   /* Target expression token range */` |
|        - | 1479 | `	SyToken *pEnd,` |
|        - | 1480 | `	int bRef           /* True for a by-reference value target */` |
|        - | 1481 | `	)` |
|        4 | 1482 | `{` |
|      124 | 1483 | `	SyToken *pSavedIn = pGen->pIn;` |
|      124 | 1484 | `	SyToken *pSavedEnd = pGen->pEnd;` |
|      124 | 1485 | `	sxi32 iVmOp = bRef ? PH7_OP_STORE_REF : PH7_OP_STORE;` |
|        - | 1486 | `	VmInstr *pInstr;` |
|      124 | 1487 | `	sxi32 iP1 = 0;` |
|      124 | 1488 | `	sxi32 iP2 = 0;` |
|      124 | 1489 | `	void *p3 = 0;` |
|        - | 1490 | `	sxi32 rc;` |
|        - | 1491 | `	/* The value being stored, below the target — the operand order OP_STORE expects. */` |
|      124 | 1492 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(pTemp),0);` |
|      124 | 1493 | `	pGen->pIn = pStart;` |
|      124 | 1494 | `	pGen->pEnd = pEnd;` |
|      124 | 1495 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE,` |
|        - | 1496 | `		GenStateForEachNodeValidator);` |
|      124 | 1497 | `	pGen->pIn = pSavedIn;` |
|      124 | 1498 | `	pGen->pEnd = pSavedEnd;` |
|      124 | 1499 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 1500 | `		return SXERR_ABORT;` |
|      124 | 1501 | `	}else if( rc != SXRET_OK ){` |
|        - | 1502 | `		/* The validator already reported it; drop the pushed value and carry on. */` |
|      ! 0 | 1503 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      ! 0 | 1504 | `		return SXRET_OK;` |
|        - | 1505 | `	}` |
|      124 | 1506 | `	pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      124 | 1507 | `	if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|        - | 1508 | `		/* A member target resolves (and, for a reference, stashes) its own slot. */` |
|       22 | 1509 | `		if( bRef ){` |
|        3 | 1510 | `			pInstr->iP2 = PH7_MEMBER_REF_TARGET;` |
|        1 | 1511 | `		}` |
|       22 | 1512 | `		iP2 = 1;` |
|      114 | 1513 | `	}else if( pInstr ){` |
|      104 | 1514 | `		(void)PH7_VmPopInstr(pGen->pVm);` |
|      104 | 1515 | `		if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       19 | 1516 | `			iVmOp = bRef ? PH7_OP_STORE_IDX_REF : PH7_OP_STORE_IDX;` |
|       19 | 1517 | `			iP1 = pInstr->iP1;` |
|       19 | 1518 | `			if( bRef ){` |
|        3 | 1519 | `				iP2 = pInstr->iP2;` |
|        3 | 1520 | `				p3 = pInstr->p3;` |
|        1 | 1521 | `			}` |
|       10 | 1522 | `		}else{` |
|       86 | 1523 | `			p3 = pInstr->p3;` |
|        - | 1524 | `		}` |
|       50 | 1525 | `	}` |
|      124 | 1526 | `	PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|        - | 1527 | `	/* Discard the stored value the store leaves behind */` |
|      124 | 1528 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      124 | 1529 | `	if( bRef ){` |
|        - | 1530 | `		/* The target now holds the element; drop the temporary's own hold, or it` |
|        - | 1531 | `		 * would keep the element a REFERENCE for the rest of the script — an extra` |
|        - | 1532 | ``		 * holder no `unset()` the program can write is able to reach. Dropping the`` |
|        - | 1533 | `		 * NAME never releases the slot the target still refers to. */` |
|        5 | 1534 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UNSET_VAR,0,0,(void *)pTemp,0);` |
|        2 | 1535 | `	}` |
|      124 | 1536 | `	return SXRET_OK;` |
|       64 | 1537 | `}` |
|        - | 1538 | `/*` |
|        - | 1539 | ` * Compile the 'foreach' statement.` |
|        - | 1540 | ` * According to the PHP language reference` |
|        - | 1541 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|        - | 1542 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|        - | 1543 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|        - | 1544 | ` *  is a minor but useful extension of the first:` |
|        - | 1545 | ` *  foreach (array_expression as $value)` |
|        - | 1546 | ` *    statement` |
|        - | 1547 | ` *  foreach (array_expression as $key => $value)` |
|        - | 1548 | ` *   statement` |
|        - | 1549 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|        - | 1550 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|        - | 1551 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|        - | 1552 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|        - | 1553 | ` *  to the variable $key on each loop.` |
|        - | 1554 | ` *  Note:` |
|        - | 1555 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|        - | 1556 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|        - | 1557 | ` *  Note:` |
|        - | 1558 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|        - | 1559 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|        - | 1560 | ` *  or after the foreach without resetting it.` |
|        - | 1561 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|        - | 1562 | ` *  of copying the value.` |
|        - | 1563 | ` */` |
|    91940 | 1564 | `PH7_PRIVATE sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|        5 | 1565 | `{` |
|    91945 | 1566 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    91945 | 1567 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|        - | 1568 | ``	/* Token ranges of a KEY / VALUE target that is not a plain `$name`: it is`` |
|        - | 1569 | `	 * compiled as a real store at the top of the loop body (php assigns the value` |
|        - | 1570 | `	 * first, then the key), against a synthetic temporary the step writes. */` |
|    91945 | 1571 | `	SyToken *pKeyStart = 0,*pKeyEnd = 0;` |
|    91945 | 1572 | `	SyToken *pValStart = 0,*pValEnd = 0;` |
|    91945 | 1573 | `	GenBlock *pForeachBlock = 0;` |
|        - | 1574 | `	ph7_foreach_info *pInfo;` |
|        - | 1575 | `	sxu32 nFalseJump;` |
|    91945 | 1576 | `	sxu32 nContainerEnd = 0; /* instruction count just after the iterated expression */` |
|        - | 1577 | `	VmInstr *pInstr;` |
|        - | 1578 | `	sxu32 nLine;` |
|        - | 1579 | `	sxi32 rc;` |
|    91945 | 1580 | `	nLine = pGen->pIn->nLine;` |
|        - | 1581 | `	/* Jump the 'foreach' keyword */` |
|    91945 | 1582 | `	pGen->pIn++;` |
|    91945 | 1583 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1584 | `		/* Syntax error */` |
|      ! 0 | 1585 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|      ! 0 | 1586 | `		if( rc == SXERR_ABORT ){` |
|        - | 1587 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1588 | `			return SXERR_ABORT;` |
|        - | 1589 | `		}` |
|      ! 0 | 1590 | `		goto Synchronize;` |
|        - | 1591 | `	}` |
|        - | 1592 | `	/* Jump the left parenthesis '(' */` |
|    91945 | 1593 | `	pGen->pIn++;` |
|        - | 1594 | `	/* Create the loop block */` |
|    91945 | 1595 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    91945 | 1596 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1597 | `		return SXERR_ABORT;` |
|        - | 1598 | `	}` |
|        - | 1599 | `	/* Delimit the expression */` |
|    91945 | 1600 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    91945 | 1601 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - | 1602 | `		/* Empty expression */` |
|      ! 0 | 1603 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|      ! 0 | 1604 | `		if( rc == SXERR_ABORT ){` |
|        - | 1605 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1606 | `			return SXERR_ABORT;` |
|        - | 1607 | `		}` |
|        - | 1608 | `		/* Synchronize */` |
|      ! 0 | 1609 | `		pGen->pIn = pEnd;` |
|      ! 0 | 1610 | `		if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 | 1611 | `			pGen->pIn++;` |
|      ! 0 | 1612 | `		}` |
|      ! 0 | 1613 | `		return SXRET_OK;` |
|        - | 1614 | `	}` |
|        - | 1615 | `	/* Compile the array expression.` |
|        - | 1616 | `	 *` |
|        - | 1617 | ``	 * The separator is the first TOP-LEVEL `as`: one nested inside brackets, parens`` |
|        - | 1618 | `	 * or braces belongs to something else the iterated expression contains — a` |
|        - | 1619 | ``	 * closure with a `foreach` of its own is the shape that finds this, and cutting`` |
|        - | 1620 | ``	 * at its inner `as` left the outer expression with an unclosed bracket and made`` |
|        - | 1621 | ``	 * `foreach ([function(){ foreach ([1] as $k) … }] as $f)` a compile fatal on`` |
|        - | 1622 | ``	 * source php runs. An `as` that is part of a NAME ($as, $o->as, A::as) is not a`` |
|        - | 1623 | `	 * separator either. */` |
|    91945 | 1624 | `	pCur = pGen->pIn;` |
|        - | 1625 | `	{` |
|    91945 | 1626 | `		sxi32 iNest = 0;` |
|   774849 | 1627 | `		while( pCur < pEnd ){` |
|   774849 | 1628 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|    78063 | 1629 | `				iNest++;` |
|   735759 | 1630 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        - | 1631 | `				/* A mismatch here is the expression parser's to report. */` |
|    78063 | 1632 | `				iNest--;` |
|   657701 | 1633 | `			}else if( iNest <= 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|   108183 | 1634 | `				sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|   108183 | 1635 | `				if( nKeywrd == PH7_TKWRD_AS && !GenStateKeywordIsName(pGen->pIn,pCur) ){` |
|    91945 | 1636 | `					break;` |
|        - | 1637 | `				}` |
|     8108 | 1638 | `			}` |
|        - | 1639 | `			/* Advance the stream cursor */` |
|   682909 | 1640 | `			pCur++;` |
|        5 | 1641 | `		}` |
|        - | 1642 | `	}` |
|    91945 | 1643 | `	if( pCur <= pGen->pIn ){` |
|      ! 0 | 1644 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 1645 | `			"foreach: Missing array/object expression");` |
|      ! 0 | 1646 | `		if( rc == SXERR_ABORT ){` |
|        - | 1647 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1648 | `			return SXERR_ABORT;` |
|        - | 1649 | `		}` |
|      ! 0 | 1650 | `		goto Synchronize;` |
|        - | 1651 | `	}` |
|        - | 1652 | `	/* Swap token streams */` |
|    91945 | 1653 | `	pTmp = pGen->pEnd;` |
|    91945 | 1654 | `	pGen->pEnd = pCur;` |
|    91945 | 1655 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    91945 | 1656 | `	if( rc == SXERR_ABORT ){` |
|        - | 1657 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1658 | `		return SXERR_ABORT;` |
|        - | 1659 | `	}` |
|        - | 1660 | `	/* Remember where the iterated expression ends: whether it was fetched for` |
|        - | 1661 | ``	 * WRITING is only known once the `&` after `as` has been read, and a`` |
|        - | 1662 | `	 * by-reference walk fetches its container the way a reference bind does` |
|        - | 1663 | `	 * (php's BP_VAR_W) -- so a missing property is created, not warned about. */` |
|    91945 | 1664 | `	nContainerEnd = PH7_VmInstrLength(pGen->pVm);` |
|        - | 1665 | `	/* Update token stream */` |
|    91945 | 1666 | `	while(pGen->pIn < pCur ){` |
|      ! 0 | 1667 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 1668 | `		if( rc == SXERR_ABORT ){` |
|        - | 1669 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1670 | `			return SXERR_ABORT;` |
|        - | 1671 | `		}` |
|      ! 0 | 1672 | `		pGen->pIn++;` |
|      ! 0 | 1673 | `	}` |
|    91945 | 1674 | `	pCur++; /* Jump the 'as' keyword */` |
|    91945 | 1675 | `	pGen->pIn = pCur;` |
|    91945 | 1676 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 1677 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|      ! 0 | 1678 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1679 | `			return SXERR_ABORT;` |
|        - | 1680 | `		}` |
|      ! 0 | 1681 | `	}` |
|        - | 1682 | `	/* Create the foreach context */` |
|    91945 | 1683 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    91945 | 1684 | `	if( pInfo == 0 ){` |
|      ! 0 | 1685 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|      ! 0 | 1686 | `		return SXERR_ABORT;` |
|        - | 1687 | `	}` |
|        - | 1688 | `	/* Zero the structure */` |
|    91945 | 1689 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|        - | 1690 | `	/* Initialize structure fields */` |
|    91945 | 1691 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|        - | 1692 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|        - | 1693 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|        - | 1694 | `	 * '=>'. */` |
|    91945 | 1695 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|    91945 | 1696 | `	if( pCur < pEnd ){` |
|        - | 1697 | `		/* Compile the expression holding the key name */` |
|    16783 | 1698 | `		if( pGen->pIn >= pCur ){` |
|      ! 0 | 1699 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|      ! 0 | 1700 | `			if( rc == SXERR_ABORT ){` |
|        - | 1701 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1702 | `				return SXERR_ABORT;` |
|      ! 0 | 1703 | `			}` |
|    16783 | 1704 | `		}else if( !GenStateForeachTargetIsPlainVar(pGen->pIn,pCur) ){` |
|        - | 1705 | `			/* A writable but non-name key target ($o->k, C::$s, $a['k'], $$n): the` |
|        - | 1706 | `			 * step lands in a temporary and the store runs in the loop body. */` |
|       13 | 1707 | `			pKeyStart = pGen->pIn;` |
|       13 | 1708 | `			pKeyEnd = pCur;` |
|       13 | 1709 | `			if( GenStateForeachTempName(&(*pGen),"key",&pInfo->sKey) != SXRET_OK ){` |
|      ! 0 | 1710 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1711 | `				return SXERR_ABORT;` |
|        - | 1712 | `			}` |
|       13 | 1713 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|        7 | 1714 | `		}else{` |
|    16771 | 1715 | `			pGen->pEnd = pCur;` |
|    16771 | 1716 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    16771 | 1717 | `			if( rc == SXERR_ABORT ){` |
|        - | 1718 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1719 | `				return SXERR_ABORT;` |
|        - | 1720 | `			}` |
|    16771 | 1721 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    16771 | 1722 | `			if( pInstr->p3 ){` |
|        - | 1723 | `				/* Record key name */` |
|    16771 | 1724 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|     8369 | 1725 | `			}` |
|    16771 | 1726 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|        - | 1727 | `		}` |
|    16783 | 1728 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|     8375 | 1729 | `	}` |
|    91945 | 1730 | `	pGen->pEnd = pEnd;` |
|    91945 | 1731 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 1732 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|      ! 0 | 1733 | `		if( rc == SXERR_ABORT ){` |
|        - | 1734 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1735 | `			return SXERR_ABORT;` |
|        - | 1736 | `		}` |
|      ! 0 | 1737 | `		goto Synchronize;` |
|        - | 1738 | `	}` |
|    91945 | 1739 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|       87 | 1740 | `		pGen->pIn++;` |
|        - | 1741 | `		/* Pass by reference  */` |
|       87 | 1742 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       42 | 1743 | `	}` |
|        - | 1744 | `	/* Check if the value target is list() */` |
|    91945 | 1745 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        8 | 1746 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|        - | 1747 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|        - | 1748 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|        - | 1749 | `		 */` |
|        - | 1750 | `		static int iForeachListCnt = 0;` |
|        - | 1751 | `		char zTmp[128];` |
|        - | 1752 | `		sxu32 nLen;` |
|        - | 1753 | `		char *zDup;` |
|       10 | 1754 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|       10 | 1755 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       10 | 1756 | `		if( zDup == 0 ){` |
|      ! 0 | 1757 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1758 | `			return SXERR_ABORT;` |
|        - | 1759 | `		}` |
|       10 | 1760 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|        - | 1761 | `		/* Save list() token boundaries */` |
|       10 | 1762 | `		pListStart = pGen->pIn;` |
|        - | 1763 | `		/* Advance past list(...) — validate parentheses */` |
|       10 | 1764 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|       10 | 1765 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1766 | `			/* php: syntax error, unexpected variable "$x", expecting "(" */` |
|        3 | 1767 | `			rc = PH7_GenSyntaxError(pGen,pGen->pIn < pEnd ? pGen->pIn : 0,"\"(\"");` |
|        3 | 1768 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1769 | `				return SXERR_ABORT;` |
|        - | 1770 | `			}` |
|        3 | 1771 | `			goto Synchronize;` |
|        - | 1772 | `		}` |
|        7 | 1773 | `		pGen->pIn++; /* Jump '(' */` |
|        7 | 1774 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|        7 | 1775 | `		if( pListEnd >= pEnd ){` |
|      ! 0 | 1776 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1777 | `				"foreach: Missing closing ')' after list");` |
|      ! 0 | 1778 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1779 | `				return SXERR_ABORT;` |
|        - | 1780 | `			}` |
|      ! 0 | 1781 | `			goto Synchronize;` |
|        - | 1782 | `		}` |
|        7 | 1783 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|        7 | 1784 | `		pListEnd = pGen->pIn;` |
|        7 | 1785 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    91940 | 1786 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|        - | 1787 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|        - | 1788 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|        - | 1789 | `		 */` |
|        - | 1790 | `		static int iForeachShortListCnt = 0;` |
|        - | 1791 | `		char zTmp[128];` |
|        - | 1792 | `		sxu32 nLen;` |
|        - | 1793 | `		char *zDup;` |
|      244 | 1794 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|      244 | 1795 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      244 | 1796 | `		if( zDup == 0 ){` |
|      ! 0 | 1797 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1798 | `			return SXERR_ABORT;` |
|        - | 1799 | `		}` |
|      244 | 1800 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|        - | 1801 | `		/* Save [...] token boundaries */` |
|      244 | 1802 | `		pListStart = pGen->pIn;` |
|        - | 1803 | `		/* Advance past [...] */` |
|      244 | 1804 | `		pGen->pIn++; /* Jump '[' */` |
|      244 | 1805 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|      244 | 1806 | `		if( pListEnd >= pEnd ){` |
|      ! 0 | 1807 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1808 | `				"foreach: Missing closing ']' after short list");` |
|      ! 0 | 1809 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1810 | `				return SXERR_ABORT;` |
|        - | 1811 | `			}` |
|      ! 0 | 1812 | `			goto Synchronize;` |
|        - | 1813 | `		}` |
|      244 | 1814 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|      244 | 1815 | `		pListEnd = pGen->pIn;` |
|      244 | 1816 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    91817 | 1817 | `	}else if( !GenStateForeachTargetIsPlainVar(pGen->pIn,pEnd) ){` |
|        - | 1818 | `		/* A writable but non-name value target — same treatment as the key above. */` |
|      112 | 1819 | `		pValStart = pGen->pIn;` |
|      112 | 1820 | `		pValEnd = pEnd;` |
|      112 | 1821 | `		if( GenStateForeachTempName(&(*pGen),"val",&pInfo->sValue) != SXRET_OK ){` |
|      ! 0 | 1822 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1823 | `			return SXERR_ABORT;` |
|        - | 1824 | `		}` |
|       58 | 1825 | `	}else{` |
|        - | 1826 | `		/* Compile the expression holding the value name */` |
|    91590 | 1827 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    91590 | 1828 | `		if( rc == SXERR_ABORT ){` |
|        - | 1829 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1830 | `			return SXERR_ABORT;` |
|        - | 1831 | `		}` |
|    91590 | 1832 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    91590 | 1833 | `		if( pInstr->p3 ){` |
|        - | 1834 | `			/* Record value name */` |
|    91590 | 1835 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    45721 | 1836 | `		}` |
|        - | 1837 | `	}` |
|    91938 | 1838 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd` |
|      250 | 1839 | `	 && PH7_GenStateListSpanHasRef(pListStart,pListEnd) ){` |
|        - | 1840 | `		/* The list binds by reference, so this step's value has to BE the array` |
|        - | 1841 | `		 * element rather than a copy of it. */` |
|        3 | 1842 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|        1 | 1843 | `	}` |
|    91943 | 1844 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_REF) && nContainerEnd > 0 ){` |
|       89 | 1845 | `		VmInstr *pContainer = PH7_VmGetInstr(pGen->pVm,nContainerEnd - 1);` |
|       86 | 1846 | `		if( pContainer && pContainer->iOp == PH7_OP_MEMBER` |
|       56 | 1847 | `		 && pContainer->iP2 == PH7_MEMBER_READ ){` |
|       22 | 1848 | `			pContainer->bRefSrc = 1;` |
|       10 | 1849 | `		}` |
|       43 | 1850 | `	}` |
|        - | 1851 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    91943 | 1852 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|        - | 1853 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    91943 | 1854 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|        - | 1855 | `	/* Record the first instruction to execute */` |
|    91943 | 1856 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|        - | 1857 | `	/* Emit the FOREACH_STEP instruction */` |
|    91943 | 1858 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|        - | 1859 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    91943 | 1860 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|        - | 1861 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    91943 | 1862 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|        - | 1863 | `		SyToken *pSavedIn,*pSavedEnd;` |
|        - | 1864 | `		/* Load the temporary variable holding the current value onto the stack.` |
|        - | 1865 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|        - | 1866 | `		 */` |
|      250 | 1867 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|        - | 1868 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|        - | 1869 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|        - | 1870 | `		 * picks up the delimiter and the variable names inside.` |
|        - | 1871 | `		 */` |
|      250 | 1872 | `		pSavedIn = pGen->pIn;` |
|      250 | 1873 | `		pSavedEnd = pGen->pEnd;` |
|      250 | 1874 | `		pGen->pIn = pListStart;` |
|      250 | 1875 | `		pGen->pEnd = pListEnd;` |
|      250 | 1876 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|      244 | 1877 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|      124 | 1878 | `		}else{` |
|        7 | 1879 | `			rc = PH7_CompileList(&(*pGen),0);` |
|        - | 1880 | `		}` |
|      250 | 1881 | `		pGen->pIn = pSavedIn;` |
|      250 | 1882 | `		pGen->pEnd = pSavedEnd;` |
|      250 | 1883 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1884 | `			return SXERR_ABORT;` |
|        - | 1885 | `		}` |
|        - | 1886 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|      250 | 1887 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      250 | 1888 | `		if( pInfo->iFlags & PH7_4EACH_STEP_REF ){` |
|        - | 1889 | `			/* The bind is done; drop the STEP's own hold on the element, exactly as` |
|        - | 1890 | `			 * the non-list by-ref target does. Left in place it would keep the row a` |
|        - | 1891 | `			 * REFERENCE for the rest of the script -- php marks the ELEMENT the` |
|        - | 1892 | `			 * target still aliases, never the row. */` |
|        3 | 1893 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_UNSET_VAR,0,0,(void *)&pInfo->sValue,0);` |
|        1 | 1894 | `		}` |
|      122 | 1895 | `	}` |
|        - | 1896 | `	/* Store this step's value and key into their non-name targets. php performs the` |
|        - | 1897 | `	 * VALUE assignment first — visible through a __set() pair, and the order the` |
|        - | 1898 | `	 * symbol table records the two locals in for the plain-name shape. */` |
|    91943 | 1899 | `	if( pValStart ){` |
|      166 | 1900 | `		rc = GenStateForeachStoreTarget(&(*pGen),&pInfo->sValue,pValStart,pValEnd,` |
|      108 | 1901 | `			(pInfo->iFlags & PH7_4EACH_STEP_REF) != 0);` |
|      112 | 1902 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1903 | `			return SXERR_ABORT;` |
|        - | 1904 | `		}` |
|       54 | 1905 | `	}` |
|    91943 | 1906 | `	if( pKeyStart ){` |
|       13 | 1907 | `		rc = GenStateForeachStoreTarget(&(*pGen),&pInfo->sKey,pKeyStart,pKeyEnd,0);` |
|       13 | 1908 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1909 | `			return SXERR_ABORT;` |
|        - | 1910 | `		}` |
|        6 | 1911 | `	}` |
|        - | 1912 | `	/* Compile the loop body */` |
|    91943 | 1913 | `	pGen->pIn = &pEnd[1];` |
|    91943 | 1914 | `	pGen->pEnd = pTmp;` |
|    91943 | 1915 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    91943 | 1916 | `	if( rc == SXERR_ABORT ){` |
|        - | 1917 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1918 | `		return SXERR_ABORT;` |
|        - | 1919 | `	}` |
|        - | 1920 | `	/* Emit the unconditional jump to the start of the loop */` |
|    91943 | 1921 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|        - | 1922 | `	/* Fix all jumps now the destination is resolved */` |
|    91943 | 1923 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 1924 | `	/* Release the loop block */` |
|    91943 | 1925 | `	GenStateLeaveBlock(pGen,0);` |
|        - | 1926 | `	/* Statement successfully compiled */` |
|    91943 | 1927 | `	return SXRET_OK;` |
|        1 | 1928 | `Synchronize:` |
|        - | 1929 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - | 1930 | `	 * compiling this erroneous block.` |
|        - | 1931 | `	 */` |
|        3 | 1932 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 | 1933 | `		pGen->pIn++;` |
|      ! 0 | 1934 | `	}` |
|        3 | 1935 | `	return SXRET_OK;` |
|    45903 | 1936 | `}` |
|        - | 1937 | `/*` |
|        - | 1938 | ` * Compile the infamous if/elseif/else if/else statements.` |
|        - | 1939 | ` * According to the PHP language reference` |
|        - | 1940 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|        - | 1941 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|        - | 1942 | ` *  that is similar to that of C:` |
|        - | 1943 | ` *  if (expr)` |
|        - | 1944 | ` *   statement` |
|        - | 1945 | ` *  else construct:` |
|        - | 1946 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|        - | 1947 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|        - | 1948 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|        - | 1949 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|        - | 1950 | ` *   $b, and a is NOT greater than b otherwise.` |
|        - | 1951 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|        - | 1952 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|        - | 1953 | ` *  elseif` |
|        - | 1954 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|        - | 1955 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|        - | 1956 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|        - | 1957 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|        - | 1958 | ` *   than b, a equal to b or a is smaller than b:` |
|        - | 1959 | ` *   <?php` |
|        - | 1960 | ` *    if ($a > $b) {` |
|        - | 1961 | ` *     echo "a is bigger than b";` |
|        - | 1962 | ` *    } elseif ($a == $b) {` |
|        - | 1963 | ` *     echo "a is equal to b";` |
|        - | 1964 | ` *    } else {` |
|        - | 1965 | ` *     echo "a is smaller than b";` |
|        - | 1966 | ` *    }` |
|        - | 1967 | ` *    ?>` |
|        - | 1968 | ` */` |
|   543287 | 1969 | `PH7_PRIVATE sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|        5 | 1970 | `{` |
|   543292 | 1971 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|   543292 | 1972 | `	GenBlock *pCondBlock = 0;` |
|        - | 1973 | `	sxu32 nJumpIdx;` |
|        - | 1974 | `	sxu32 nKeyID;` |
|        - | 1975 | `	sxi32 rc;` |
|        - | 1976 | `	/* Jump the 'if' keyword */` |
|   543292 | 1977 | `	pGen->pIn++;` |
|   543292 | 1978 | `	pToken = pGen->pIn;` |
|        - | 1979 | `	/* Create the conditional block */` |
|   543292 | 1980 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|   543292 | 1981 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1982 | `		return SXERR_ABORT;` |
|        - | 1983 | `	}` |
|        - | 1984 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   303069 | 1985 | `	for(;;){` |
|   606984 | 1986 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1987 | `			/* Syntax error */` |
|      ! 0 | 1988 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 1989 | `				pToken--;` |
|      ! 0 | 1990 | `			}` |
|      ! 0 | 1991 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|      ! 0 | 1992 | `			if( rc == SXERR_ABORT ){` |
|        - | 1993 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 1994 | `				return SXERR_ABORT;` |
|        - | 1995 | `			}` |
|      ! 0 | 1996 | `			goto Synchronize;` |
|        - | 1997 | `		}` |
|        - | 1998 | `		/* Jump the left parenthesis '(' */` |
|   606984 | 1999 | `		pToken++;` |
|        - | 2000 | `		/* Delimit the condition */` |
|   606984 | 2001 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   606984 | 2002 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|        - | 2003 | `			/* Syntax error */` |
|      ! 0 | 2004 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 2005 | `				pToken--;` |
|      ! 0 | 2006 | `			}` |
|      ! 0 | 2007 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|      ! 0 | 2008 | `			if( rc == SXERR_ABORT ){` |
|        - | 2009 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 2010 | `				return SXERR_ABORT;` |
|        - | 2011 | `			}` |
|      ! 0 | 2012 | `			goto Synchronize;` |
|        - | 2013 | `		}` |
|        - | 2014 | `		/* Swap token streams */` |
|   606984 | 2015 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|        - | 2016 | `		/* Compile the condition */` |
|   606984 | 2017 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 2018 | `		/* Update token stream */` |
|   606984 | 2019 | `		while(pGen->pIn < pEnd ){` |
|      ! 0 | 2020 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 | 2021 | `			pGen->pIn++;` |
|      ! 0 | 2022 | `		}` |
|   606984 | 2023 | `		pGen->pIn  = &pEnd[1];` |
|   606984 | 2024 | `		pGen->pEnd = pTmp;` |
|   606984 | 2025 | `		if( rc == SXERR_ABORT ){` |
|        - | 2026 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|        3 | 2027 | `			return SXERR_ABORT;` |
|        - | 2028 | `		}` |
|        - | 2029 | `		/* Emit the false jump */` |
|   606982 | 2030 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|        - | 2031 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   606982 | 2032 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|        - | 2033 | `		/* Compile the body */` |
|   606982 | 2034 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   606982 | 2035 | `		if( rc == SXERR_ABORT ){` |
|        3 | 2036 | `			return SXERR_ABORT;` |
|        - | 2037 | `		}` |
|   606980 | 2038 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   131950 | 2039 | `			break;` |
|        - | 2040 | `		}` |
|        - | 2041 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   342723 | 2042 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   342723 | 2043 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   223038 | 2044 | `			break;` |
|        - | 2045 | `		}` |
|        - | 2046 | `		/* Emit the unconditional jump */` |
|   119690 | 2047 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|        - | 2048 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   119690 | 2049 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   119690 | 2050 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|    79773 | 2051 | `			pToken = &pGen->pIn[1];` |
|    79773 | 2052 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    23813 | 2053 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    27963 | 2054 | `					break;` |
|        - | 2055 | `			}` |
|    23780 | 2056 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    11871 | 2057 | `		}` |
|    63697 | 2058 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|        - | 2059 | `		/* Synchronize cursors */` |
|    63697 | 2060 | `		pToken = pGen->pIn;` |
|        - | 2061 | `		/* Fix the false jump */` |
|    63697 | 2062 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|        5 | 2063 | `	} /* For(;;) */` |
|        - | 2064 | `	/* Fix the false jump */` |
|   543288 | 2065 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|   543288 | 2066 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   279026 | 2067 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|        - | 2068 | `			/* Compile the else block */` |
|    55998 | 2069 | `			pGen->pIn++;` |
|    55998 | 2070 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    55998 | 2071 | `			if( rc == SXERR_ABORT ){` |
|        - | 2072 |  |
|      ! 0 | 2073 | `				return SXERR_ABORT;` |
|        - | 2074 | `			}` |
|    27958 | 2075 | `	}` |
|   543288 | 2076 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|        - | 2077 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|   543288 | 2078 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|        - | 2079 | `	/* Release the conditional block */` |
|   543288 | 2080 | `	GenStateLeaveBlock(pGen,0);` |
|        - | 2081 | `	/* Statement successfully compiled */` |
|   543288 | 2082 | `	return SXRET_OK;` |
|      ! 0 | 2083 | `Synchronize:` |
|        - | 2084 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|        - | 2085 | `	 */` |
|      ! 0 | 2086 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 | 2087 | `		pGen->pIn++;` |
|      ! 0 | 2088 | `	}` |
|      ! 0 | 2089 | `	return SXRET_OK;` |
|   271272 | 2090 | `}` |
|        - | 2091 | `/*` |
|        - | 2092 | ` * Compile the global construct.` |
|        - | 2093 | ` * According to the PHP language reference` |
|        - | 2094 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|        - | 2095 | ` *  to be used in that function.` |
|        - | 2096 | ` *  Example #1 Using global` |
|        - | 2097 | ` *  <?php` |
|        - | 2098 | ` *   $a = 1;` |
|        - | 2099 | ` *   $b = 2;` |
|        - | 2100 | ` *   function Sum()` |
|        - | 2101 | ` *   {` |
|        - | 2102 | ` *    global $a, $b;` |
|        - | 2103 | ` *    $b = $a + $b;` |
|        - | 2104 | ` *   }` |
|        - | 2105 | ` *   Sum();` |
|        - | 2106 | ` *   echo $b;` |
|        - | 2107 | ` *  ?>` |
|        - | 2108 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|        - | 2109 | ` *  all references to either variable will refer to the global version. There is no limit` |
|        - | 2110 | ` *  to the number of global variables that can be manipulated by a function.` |
|        - | 2111 | ` */` |
|      145 | 2112 | `PH7_PRIVATE sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|        5 | 2113 | `{` |
|      150 | 2114 | `	SyToken *pTmp,*pNext = 0;` |
|        - | 2115 | `	sxi32 nExpr;` |
|        - | 2116 | `	sxi32 rc;` |
|        - | 2117 | `	/* Jump the 'global' keyword */` |
|      150 | 2118 | `	pGen->pIn++;` |
|      150 | 2119 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|        - | 2120 | `		/* Nothing to process */` |
|      ! 0 | 2121 | `		return SXRET_OK;` |
|        - | 2122 | `	}` |
|      150 | 2123 | `	pTmp = pGen->pEnd;` |
|      150 | 2124 | `	nExpr = 0;` |
|      335 | 2125 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      190 | 2126 | `		if( pGen->pIn < pNext ){` |
|      190 | 2127 | `			pGen->pEnd = pNext;` |
|      190 | 2128 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 | 2129 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|      ! 0 | 2130 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2131 | `					return SXERR_ABORT;` |
|        - | 2132 | `				}` |
|      185 | 2133 | `			}else if( &pGen->pIn[1] < pGen->pEnd` |
|      185 | 2134 | `			 && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|      180 | 2135 | `			 && pGen->pIn[1].sData.nByte == sizeof("this")-1` |
|       99 | 2136 | `			 && SyMemcmp((const void *)pGen->pIn[1].sData.zString,` |
|        7 | 2137 | `			             (const void *)"this",sizeof("this")-1) == 0 ){` |
|        - | 2138 | ``				/* php refuses `global $this;` at the declaration. */`` |
|        3 | 2139 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 2140 | `					"Cannot use $this as global variable");` |
|        3 | 2141 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2142 | `					return SXERR_ABORT;` |
|        - | 2143 | `				}` |
|        2 | 2144 | `			}else{` |
|      188 | 2145 | `				pGen->pIn++;` |
|      188 | 2146 | `				if( pGen->pIn >= pGen->pEnd ){` |
|        - | 2147 | `					/* Emit a warning */` |
|      ! 0 | 2148 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|      ! 0 | 2149 | `				}else{` |
|      188 | 2150 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      188 | 2151 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 2152 | `						return SXERR_ABORT;` |
|      188 | 2153 | `					}else if(rc != SXERR_EMPTY ){` |
|      188 | 2154 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      188 | 2155 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|        - | 2156 | `							/* Variable name, not a constant */` |
|      178 | 2157 | `							pLast->iP1 = 0;` |
|       86 | 2158 | `						}` |
|      188 | 2159 | `						nExpr++;` |
|       91 | 2160 | `					}` |
|        - | 2161 | `				}` |
|        - | 2162 | `			}` |
|       92 | 2163 | `		}` |
|        - | 2164 | `		/* Next expression in the stream */` |
|      190 | 2165 | `		pGen->pIn = pNext;` |
|        - | 2166 | `		/* Jump trailing commas */` |
|      230 | 2167 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       45 | 2168 | `			pGen->pIn++;` |
|        5 | 2169 | `		}` |
|        5 | 2170 | `	}` |
|        - | 2171 | `	/* Restore token stream */` |
|      150 | 2172 | `	pGen->pEnd = pTmp;` |
|      150 | 2173 | `	if( nExpr > 0 ){` |
|        - | 2174 | `		/* Emit the uplink instruction */` |
|      148 | 2175 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|       71 | 2176 | `	}` |
|      150 | 2177 | `	return SXRET_OK;` |
|       77 | 2178 | `}` |
|        - | 2179 | `/*` |
|        - | 2180 | ` * php's NOUN for a compile-time return diagnostic is the LEXICAL scope, not the` |
|        - | 2181 | `` * kind of function the `return` sits in: zend reads CG(active_class_entry), so a`` |
|        - | 2182 | ` * closure written inside a class body reports "method" and the very same closure` |
|        - | 2183 | ` * written at file scope reports "function". pCurClass is the compiler's exact` |
|        - | 2184 | ` * counterpart (the class/interface/trait/enum whose BODY is being compiled).` |
|        - | 2185 | ` */` |
|       26 | 2186 | `static const char * GenStateReturnNoun(ph7_gen_state *pGen)` |
|        4 | 2187 | `{` |
|       30 | 2188 | `	return pGen->pCurClass ? "method" : "function";` |
|        4 | 2189 | `}` |
|        - | 2190 | `/*` |
|        - | 2191 | ` * TRUE when a declared return type ACCEPTS null — the condition under which php` |
|        - | 2192 | `` * appends its `did you mean "return null;"` hint to the missing-value error.`` |
|        - | 2193 | ``  * That is every nullable declaration (`?T`, `T\|null`, and the standalone `null` `` |
|        - | 2194 | `` * type, all of which set VM_FUNC_RETURN_NULLABLE) plus `mixed`, which includes`` |
|        - | 2195 | ` * null but is stored as a pseudo-CLASS atom rather than through the flag.` |
|        - | 2196 | ` */` |
|       10 | 2197 | `static int GenStateReturnTypeAllowsNull(ph7_vm_func *pFunc)` |
|        4 | 2198 | `{` |
|        - | 2199 | `	SyString *pCls;` |
|       14 | 2200 | `	if( pFunc->iFlags & VM_FUNC_RETURN_NULLABLE ){` |
|        3 | 2201 | `		return 1;` |
|        - | 2202 | `	}` |
|       11 | 2203 | `	pCls = &pFunc->sReturnClass;` |
|        8 | 2204 | `	if( pFunc->nReturnType == SXU32_HIGH && pCls->nByte == sizeof("mixed")-1` |
|        3 | 2205 | `	 && SyStrnicmp(pCls->zString,"mixed",sizeof("mixed")-1) == 0 ){` |
|      ! 0 | 2206 | `		return 1;` |
|        - | 2207 | `	}` |
|       11 | 2208 | `	return 0;` |
|        9 | 2209 | `}` |
|        - | 2210 | `/*` |
|        - | 2211 | ` * Compile the return statement.` |
|        - | 2212 | ` * According to the PHP language reference` |
|        - | 2213 | ` *  If called from within a function, the return() statement immediately ends execution` |
|        - | 2214 | ` *  of the current function, and returns its argument as the value of the function call.` |
|        - | 2215 | ` *  return() will also end the execution of an eval() statement or script file.` |
|        - | 2216 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|        - | 2217 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|        - | 2218 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|        - | 2219 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|        - | 2220 | ` *  from within the main script file, then script execution end.` |
|        - | 2221 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|        - | 2222 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|        - | 2223 | ` *  should do so as PHP has less work to do in this case.` |
|        - | 2224 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|        - | 2225 | ` */` |
|   345802 | 2226 | `PH7_PRIVATE sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|        5 | 2227 | `{` |
|   345807 | 2228 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|        - | 2229 | `	sxi32 rc;` |
|   345807 | 2230 | `	sxu32 nLine = pGen->pIn->nLine;` |
|   345807 | 2231 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|        - | 2232 | `	ph7_vm_func *pFunc;` |
|        - | 2233 | `	sxu32 nInstrBefore;` |
|        - | 2234 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|        - | 2235 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|        - | 2236 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|        - | 2237 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|        - | 2238 | `	 * normally below so token processing stays consistent. */` |
|  1138715 | 2239 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|   792913 | 2240 | `		pFuncBlock = pFuncBlock->pParent;` |
|        5 | 2241 | `	}` |
|   345807 | 2242 | `	pFunc = pFuncBlock ? (ph7_vm_func *)pFuncBlock->pUserData : 0;` |
|   345807 | 2243 | `	if( pFunc && pFunc->nReturnType == MEMOBJ_NEVER ){` |
|        8 | 2244 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        2 | 2245 | `			"A never-returning %s must not return", GenStateReturnNoun(pGen));` |
|        6 | 2246 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2247 | `			return SXERR_ABORT;` |
|        - | 2248 | `		}` |
|        2 | 2249 | `	}` |
|        - | 2250 | `	/* Jump the 'return' keyword */` |
|   345807 | 2251 | `	pGen->pIn++;` |
|   345807 | 2252 | `	nInstrBefore = PH7_VmInstrLength(pGen->pVm);` |
|   345807 | 2253 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_VOID_CAST) ){` |
|        - | 2254 | ``		/* php's `(void)` cast is a STATEMENT prefix, so `return (void) f();` is a`` |
|        - | 2255 | ``		 * parse error there — and the expected-token set is the one `return` has,`` |
|        - | 2256 | ``		 * which is why php names `";"` here and nothing after `$x = (void)`. */`` |
|        3 | 2257 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|        - | 2258 | `			"syntax error, unexpected token \"(void)\", expecting \";\"");` |
|        3 | 2259 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2260 | `			return SXERR_ABORT;` |
|        - | 2261 | `		}` |
|        3 | 2262 | `		return SXRET_OK;` |
|        - | 2263 | `	}` |
|   345805 | 2264 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - | 2265 | ``		/* php: a stray token after `return EXPR` is `... expecting ";"`. */`` |
|   345651 | 2266 | `		const char *zSave = pGen->zClauseCloser;` |
|   345651 | 2267 | `		pGen->zClauseCloser = "\";\"";` |
|        - | 2268 | ``		/* A `return` READS its operand (the value is consumed), so compile it`` |
|        - | 2269 | ``		 * read-only: a lone undefined variable `return $z` must warn at the read`` |
|        - | 2270 | `		 * exactly like echo/interpolation, not be loaded quietly (the same quiet` |
|        - | 2271 | ``		 * load that correctly keeps a bare `$z;` statement silent). Matches the`` |
|        - | 2272 | `		 * arrow-fn implicit-return body fix. */` |
|   345651 | 2273 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|   345651 | 2274 | `		pGen->zClauseCloser = zSave;` |
|   345651 | 2275 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2276 | `			return SXERR_ABORT;` |
|   345651 | 2277 | `		}else if(rc != SXERR_EMPTY ){` |
|   345647 | 2278 | `			nRet = 1;` |
|   172580 | 2279 | `		}` |
|   172582 | 2280 | `	}` |
|        - | 2281 | ``	/* A bare `return;` inside a function that DECLARES a return type is a php`` |
|        - | 2282 | `	 * COMPILE error, not the runtime TypeError PHL used to raise on the way out:` |
|        - | 2283 | `` 	 * php rejects the program before it runs. `void` (which is what `return;` `` |
|        - | 2284 | ``	 * means) and `never` (handled above) are the two declarations exempt from it,`` |
|        - | 2285 | ``	 * and a GENERATOR is exempt whatever it declares — there `return;` ends the`` |
|        - | 2286 | `	 * generator, and the declared type describes the Generator object the call` |
|        - | 2287 | `	 * produced, never the returned value. */` |
|   345800 | 2288 | `	if( nRet == 0 && pFunc && !pGen->bInGenerator && VmFuncHasReturnType(pFunc)` |
|       66 | 2289 | `	 && pFunc->nReturnType != MEMOBJ_VOID && pFunc->nReturnType != MEMOBJ_NEVER ){` |
|       19 | 2290 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 2291 | `			"A %s with return type must return a value%s",` |
|        5 | 2292 | `			GenStateReturnNoun(pGen),` |
|       10 | 2293 | `			GenStateReturnTypeAllowsNull(pFunc)` |
|        - | 2294 | `				? " (did you mean \"return null;\" instead of \"return;\"?)" : "");` |
|       14 | 2295 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2296 | `			return SXERR_ABORT;` |
|        - | 2297 | `		}` |
|        5 | 2298 | `	}` |
|        - | 2299 | ``	/* The mirror rule: a `void` function must not return a VALUE, and php stops`` |
|        - | 2300 | `	 * the program at the return statement rather than throwing on the way out.` |
|        - | 2301 | `	 * Generators keep their own diagnostic (a void generator is rejected as` |
|        - | 2302 | ``	 * `Generator return type must be a supertype of Generator`), so they are`` |
|        - | 2303 | `	 * skipped here exactly as above. php's hint fires when the operand is a` |
|        - | 2304 | ``	 * compile-time constant null; PHL folds the `null` KEYWORD (constant index 0,`` |
|        - | 2305 | `	 * emitted as a lone OP_LOADC and unchanged by any wrapping parens), which is` |
|        - | 2306 | `	 * every shape real code writes. */` |
|   345800 | 2307 | `	if( nRet != 0 && pFunc && !pGen->bInGenerator` |
|   335959 | 2308 | `	 && pFunc->nReturnType == MEMOBJ_VOID ){` |
|       16 | 2309 | `		VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|       22 | 2310 | `		int bNullLiteral = (PH7_VmInstrLength(pGen->pVm) == nInstrBefore + 1)` |
|       12 | 2311 | `			&& pLast && pLast->iOp == PH7_OP_LOADC` |
|       18 | 2312 | `			&& pLast->iP1 == 0 && pLast->iP2 == 0;` |
|       22 | 2313 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 2314 | `			"A void %s must not return a value%s",` |
|        6 | 2315 | `			GenStateReturnNoun(pGen),` |
|        6 | 2316 | `			bNullLiteral` |
|        - | 2317 | `				? " (did you mean \"return;\" instead of \"return null;\"?)" : "");` |
|       16 | 2318 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2319 | `			return SXERR_ABORT;` |
|        - | 2320 | `		}` |
|        6 | 2321 | `	}` |
|        - | 2322 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|        - | 2323 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|        - | 2324 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|        - | 2325 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|   345805 | 2326 | `	if( pGen->bInGenerator ){` |
|       53 | 2327 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|       53 | 2328 | `		return SXRET_OK;` |
|        - | 2329 | `	}` |
|        - | 2330 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|        - | 2331 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|        - | 2332 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|        - | 2333 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|        - | 2334 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|   345757 | 2335 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|   345757 | 2336 | `	return SXRET_OK;` |
|   172665 | 2337 | `}` |
|        - | 2338 | `/*` |
|        - | 2339 | ` * Compile a yield expression.` |
|        - | 2340 | ` * Called from the expression code generator when a yield node is encountered.` |
|        - | 2341 | ` * Handles: yield, yield $value, yield $key => $value` |
|        - | 2342 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|        - | 2343 | ` */` |
|      644 | 2344 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|        5 | 2345 | `{` |
|        - | 2346 | `	SyToken *pTmp, *pSplit;` |
|      649 | 2347 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|      649 | 2348 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|        - | 2349 | `	sxi32 rc;` |
|      322 | 2350 | `	(void)iCompileFlag;` |
|        - | 2351 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|      649 | 2352 | `	pGen->pIn++;` |
|        - | 2353 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|        - | 2354 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|        - | 2355 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|        - | 2356 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|        - | 2357 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|      644 | 2358 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|      368 | 2359 | `		&& pGen->pIn->sData.nByte == 4` |
|       95 | 2360 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|       87 | 2361 | `		pGen->pIn++; /* Skip 'from' */` |
|       87 | 2362 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       87 | 2363 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2364 | `			return SXERR_ABORT;` |
|        - | 2365 | `		}` |
|       87 | 2366 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 | 2367 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|      ! 0 | 2368 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|        - | 2369 | `				"Missing expression after 'yield from'");` |
|      ! 0 | 2370 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2371 | `				return SXERR_ABORT;` |
|        - | 2372 | `			}` |
|      ! 0 | 2373 | `		}` |
|       87 | 2374 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|       87 | 2375 | `		return SXRET_OK;` |
|        - | 2376 | `	}` |
|      567 | 2377 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - | 2378 | `		/* Bare yield — no value */` |
|        8 | 2379 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|        8 | 2380 | `		return SXRET_OK;` |
|        - | 2381 | `	}` |
|        - | 2382 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax. The shared` |
|        - | 2383 | `	 * array-entry scanner is the one that already knows which '=>' are NOT` |
|        - | 2384 | ``	 * separators — an arrow function's body and a match arm's — so `yield fn($x)`` |
|        - | 2385 | `` 	 * => $x` and `yield match($k){ 1 => 2 }` stop being read as `key => value` `` |
|        - | 2386 | `	 * pairs (the first was a parse error, the second yielded the wrong pair). */` |
|      561 | 2387 | `	pSplit = GenStateFindTopLevelArrow(pGen->pIn,pGen->pEnd);` |
|      561 | 2388 | `	if( pSplit >= pGen->pEnd ){` |
|      517 | 2389 | `		pSplit = 0;` |
|      256 | 2390 | `	}` |
|      561 | 2391 | `	pTmp = pGen->pEnd;` |
|      561 | 2392 | `	if( pSplit ){` |
|        - | 2393 | `		/* yield $key => $value */` |
|       48 | 2394 | `		pGen->pEnd = pSplit;` |
|       48 | 2395 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       48 | 2396 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       48 | 2397 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|       48 | 2398 | `		pGen->pEnd = pTmp;` |
|       48 | 2399 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       48 | 2400 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       48 | 2401 | `		iP1 = 1;` |
|       48 | 2402 | `		iP2 = 1;` |
|       26 | 2403 | `	}else{` |
|        - | 2404 | `		/* yield $value */` |
|      517 | 2405 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      517 | 2406 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      517 | 2407 | `		if( rc != SXERR_EMPTY ){` |
|      517 | 2408 | `			iP1 = 1;` |
|      256 | 2409 | `		}` |
|        - | 2410 | `	}` |
|      561 | 2411 | `	pGen->pEnd = pTmp;` |
|      561 | 2412 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|      561 | 2413 | `	return SXRET_OK;` |
|      327 | 2414 | `}` |
|        - | 2415 | `/*` |
|        - | 2416 | ` * Compile the die/exit language construct.` |
|        - | 2417 | ` * The role of these constructs is to terminate execution of the script.` |
|        - | 2418 | ` * Shutdown functions will always be executed even if exit() is called.` |
|        - | 2419 | ` */` |
|      390 | 2420 | `PH7_PRIVATE sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|        5 | 2421 | `{` |
|      395 | 2422 | `	sxi32 nExpr = 0;` |
|        - | 2423 | `	sxi32 rc;` |
|        - | 2424 | `	/* Jump the die/exit keyword */` |
|      395 | 2425 | `	pGen->pIn++;` |
|      395 | 2426 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - | 2427 | `		/* Compile the expression */` |
|      395 | 2428 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      395 | 2429 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2430 | `			return SXERR_ABORT;` |
|      395 | 2431 | `		}else if(rc != SXERR_EMPTY ){` |
|      395 | 2432 | `			nExpr = 1;` |
|      195 | 2433 | `		}` |
|      195 | 2434 | `	}` |
|        - | 2435 | `	/* Emit the HALT instruction */` |
|      395 | 2436 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|      395 | 2437 | `	return SXRET_OK;` |
|      200 | 2438 | `}` |
|        - | 2439 | `/*` |
|        - | 2440 | ` * Compile the 'echo' language construct.` |
|        - | 2441 | ` */` |
|    32269 | 2442 | `PH7_PRIVATE sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|        5 | 2443 | `{` |
|    32274 | 2444 | `	SyToken *pTmp,*pNext = 0;` |
|    32274 | 2445 | `	sxu32 nLine = pGen->pIn->nLine;` |
|    32274 | 2446 | `	int nExpr = 0;      /* expressions actually compiled */` |
|    32274 | 2447 | `	int bExpectMore = 1;/* after 'echo' or a comma an expression is REQUIRED */` |
|        - | 2448 | `	sxi32 rc;` |
|        - | 2449 | `	/* Jump the 'echo' keyword */` |
|    32274 | 2450 | `	pGen->pIn++;` |
|        - | 2451 | `	/* Compile arguments one after one. php: a stray token in an echo list is` |
|        - | 2452 | ``	 * `... expecting "," or ";"` — set the closer for each argument's compile. */`` |
|    32274 | 2453 | `	pTmp = pGen->pEnd;` |
|        - | 2454 | `	{` |
|    32274 | 2455 | `	const char *zSaveEcho = pGen->zClauseCloser;` |
|    93266 | 2456 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|    61003 | 2457 | `		if( pGen->pIn < pNext ){` |
|    61003 | 2458 | `			pGen->pEnd = pNext;` |
|    61003 | 2459 | `			pGen->zClauseCloser = "\",\" or \";\"";` |
|    61003 | 2460 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|    61003 | 2461 | `			pGen->zClauseCloser = zSaveEcho;` |
|    61003 | 2462 | `			if( rc == SXERR_ABORT ){` |
|        6 | 2463 | `				return SXERR_ABORT;` |
|    60999 | 2464 | `			}else if( rc != SXERR_EMPTY ){` |
|        - | 2465 | `				/* Emit the consume instruction */` |
|    60913 | 2466 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    60913 | 2467 | `				nExpr++;` |
|    60913 | 2468 | `				bExpectMore = 0;` |
|    30408 | 2469 | `			}` |
|    30451 | 2470 | `		}` |
|        - | 2471 | `		/* Jump trailing commas (php: exactly one between expressions; a` |
|        - | 2472 | `		 * dangling or doubled comma is a parse error, enforced below) */` |
|    89734 | 2473 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|    28742 | 2474 | `			if( bExpectMore ){` |
|        - | 2475 | `				/* two commas in a row */` |
|        3 | 2476 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,pNext->nLine,` |
|        - | 2477 | `					"syntax error, unexpected token \",\"");` |
|        3 | 2478 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 2479 | `			}` |
|    28740 | 2480 | `			bExpectMore = 1;` |
|    28740 | 2481 | `			pNext++;` |
|        5 | 2482 | `		}` |
|    60997 | 2483 | `		pGen->pIn = pNext;` |
|        5 | 2484 | `	}` |
|        - | 2485 | `	}` |
|        - | 2486 | `	/* Restore token stream */` |
|    32268 | 2487 | `	pGen->pEnd = pTmp;` |
|    32268 | 2488 | `	if( nExpr == 0 \|\| bExpectMore ){` |
|        - | 2489 | ``		/* `echo ;` or `echo expr, ;` — php rejects both */`` |
|       94 | 2490 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - | 2491 | `			"syntax error, unexpected token \";\"");` |
|       94 | 2492 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 2493 | `	}` |
|    32178 | 2494 | `	return SXRET_OK;` |
|    16101 | 2495 | `}` |
|        - | 2496 | `/*` |
|        - | 2497 | ` * Compile the static statement.` |
|        - | 2498 | ` * According to the PHP language reference` |
|        - | 2499 | ` *  Another important feature of variable scoping is the static variable.` |
|        - | 2500 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|        - | 2501 | ` *  when program execution leaves this scope.` |
|        - | 2502 | ` *  Static variables also provide one way to deal with recursive functions.` |
|        - | 2503 | ` * Symisc eXtension.` |
|        - | 2504 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|        - | 2505 | ` *  the zend engine would allow only simple scalar value.` |
|        - | 2506 | ` *  Example` |
|        - | 2507 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|        - | 2508 | ` *    Refer to the official documentation for more information on this feature.` |
|        - | 2509 | ` */` |
|       82 | 2510 | `PH7_PRIVATE sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|        5 | 2511 | `{` |
|        - | 2512 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|        - | 2513 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|        - | 2514 | `	GenBlock *pBlock;` |
|        - | 2515 | `	SyString *pName;` |
|        - | 2516 | `	char *zDup;` |
|        - | 2517 | `	sxu32 nLine;` |
|        - | 2518 | `	sxi32 rc;` |
|        - | 2519 | ``	/* `static function () {}` / `static fn () =>` at statement position is an`` |
|        - | 2520 | `	 * EXPRESSION statement (a bare static closure), not a static-variable` |
|        - | 2521 | `	 * declaration — hand it to the expression compiler (php accepts it). */` |
|       82 | 2522 | `	if( &pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & PH7_TK_KEYWORD)` |
|       48 | 2523 | `	 && (SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FUNCTION` |
|        2 | 2524 | `	  \|\| SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FN) ){` |
|        5 | 2525 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        5 | 2526 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2527 | `			return SXERR_ABORT;` |
|        5 | 2528 | `		}else if( rc != SXERR_EMPTY ){` |
|        5 | 2529 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        2 | 2530 | `		}` |
|        5 | 2531 | `		return SXRET_OK;` |
|        - | 2532 | `	}` |
|        - | 2533 | `	/* Jump the static keyword */` |
|       83 | 2534 | `	nLine = pGen->pIn->nLine;` |
|       83 | 2535 | `	pGen->pIn++;` |
|        - | 2536 | `	/* Extract the enclosing function if any */` |
|       83 | 2537 | `	pBlock = pGen->pCurrent;` |
|      161 | 2538 | `	while( pBlock ){` |
|      157 | 2539 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       79 | 2540 | `			break;` |
|        - | 2541 | `		}` |
|        - | 2542 | `		/* Point to the upper block */` |
|       83 | 2543 | `		pBlock = pBlock->pParent;` |
|        5 | 2544 | `	}` |
|       83 | 2545 | `	if( pBlock == 0 ){` |
|        - | 2546 | `		/* Static statement,called outside of a function body,treat it as a simple variable.` |
|        - | 2547 | `		 * php's list form applies here too, so each declarator is compiled on its own` |
|        - | 2548 | `		 * and the comma is stepped over rather than reaching the expression parser` |
|        - | 2549 | `		 * (which has no comma operator). */` |
|        3 | 2550 | `		for(;;){` |
|        7 | 2551 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|        - | 2552 | ``				/* php: `static FOO;` is a syntax error naming FOO and expecting "::"`` |
|        - | 2553 | ``				 * (the parser is still open to `static::` at that point). */`` |
|      ! 0 | 2554 | `				rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"::\"");` |
|      ! 0 | 2555 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2556 | `					return SXERR_ABORT;` |
|        - | 2557 | `				}` |
|      ! 0 | 2558 | `				goto Synchronize;` |
|        - | 2559 | `			}` |
|        - | 2560 | `			/* Compile the expression holding the variable */` |
|        7 | 2561 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|        7 | 2562 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2563 | `				return SXERR_ABORT;` |
|        7 | 2564 | `			}else if( rc != SXERR_EMPTY ){` |
|        - | 2565 | `				/* Emit the POP instruction */` |
|        7 | 2566 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        3 | 2567 | `			}` |
|        7 | 2568 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|        3 | 2569 | `				break;` |
|        - | 2570 | `			}` |
|        3 | 2571 | `			pGen->pIn++; /* Jump the comma and take the next declarator */` |
|        1 | 2572 | `		}` |
|        5 | 2573 | `		return SXRET_OK;` |
|        - | 2574 | `	}` |
|       79 | 2575 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        - | 2576 | ``	/* php declares a LIST here -- `static $a, $b = 2, $c;` -- each element its own`` |
|        - | 2577 | `	 * slot with its own optional initializer. PHL took the first declarator and` |
|        - | 2578 | ``	 * then refused the comma (`static: Unexpected token ','`), which is a fatal on`` |
|        - | 2579 | `	 * an everyday spelling: two counters in one statement is how the construct is` |
|        - | 2580 | `	 * usually written. */` |
|       40 | 2581 | `Declarator:` |
|        - | 2582 | `	/* Make sure we are dealing with a valid statement */` |
|       85 | 2583 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|       78 | 2584 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - | 2585 | ``			/* php: `static FOO;` is a syntax error naming FOO and expecting "::"`` |
|        - | 2586 | ``			 * (the parser is still open to `static::` at that point). */`` |
|        3 | 2587 | `			rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"::\"");` |
|        3 | 2588 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2589 | `				return SXERR_ABORT;` |
|        - | 2590 | `			}` |
|        3 | 2591 | `			goto Synchronize;` |
|        - | 2592 | `	}` |
|       83 | 2593 | `	pGen->pIn++;` |
|        - | 2594 | `	/* Extract variable name */` |
|       83 | 2595 | `	pName = &pGen->pIn->sData;` |
|        - | 2596 | ``	/* php refuses `static $this;` at the declaration — the name is not a slot a`` |
|        - | 2597 | `	 * function may own. */` |
|       78 | 2598 | `	if( pName->nByte == sizeof("this")-1` |
|       47 | 2599 | `	 && SyMemcmp((const void *)pName->zString,(const void *)"this",sizeof("this")-1) == 0 ){` |
|        3 | 2600 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 2601 | `			"Cannot use $this as static variable");` |
|        3 | 2602 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2603 | `			return SXERR_ABORT;` |
|        - | 2604 | `		}` |
|        3 | 2605 | `		goto Synchronize;` |
|        - | 2606 | `	}` |
|       81 | 2607 | `	pGen->pIn++; /* Jump the var name */` |
|       76 | 2608 | `	if( pGen->pIn < pGen->pEnd` |
|       81 | 2609 | `	 && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|      ! 0 | 2610 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 2611 | `		goto Synchronize;` |
|        - | 2612 | `	}` |
|        - | 2613 | `	/* Initialize the structure describing the static variable */` |
|       81 | 2614 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       81 | 2615 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|        - | 2616 | `	/* Duplicate variable name */` |
|       81 | 2617 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       81 | 2618 | `	if( zDup == 0 ){` |
|      ! 0 | 2619 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 2620 | `		return SXERR_ABORT;` |
|        - | 2621 | `	}` |
|       81 | 2622 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|        - | 2623 | `	/* Check if we have an expression to compile */` |
|       81 | 2624 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|        - | 2625 | `		SySet *pInstrContainer;` |
|        - | 2626 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|        - | 2627 | `		 * Static variable can take any complex expression including function` |
|        - | 2628 | `		 * call as their initialization value.` |
|        - | 2629 | `		 * Example:` |
|        - | 2630 | `		 *		static $var = foo(1,4+5,bar());` |
|        - | 2631 | `		 */` |
|       64 | 2632 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|        - | 2633 | `		/* Swap bytecode container */` |
|       64 | 2634 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       64 | 2635 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|        - | 2636 | `		/* Compile the expression. EXPR_FLAG_COMMA_STATEMENT stops it at the first` |
|        - | 2637 | `		 * top-level comma so the declarator after it is left for the loop. */` |
|       64 | 2638 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|        - | 2639 | `		/* Emit the done instruction */` |
|       64 | 2640 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        - | 2641 | `		/* Restore default bytecode container */` |
|       64 | 2642 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       30 | 2643 | `	}` |
|        - | 2644 | `	/* Finally save the compiled static variable in the appropriate container */` |
|       81 | 2645 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|       81 | 2646 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|        7 | 2647 | `		pGen->pIn++; /* Jump the comma and take the next declarator */` |
|        7 | 2648 | `		goto Declarator;` |
|        - | 2649 | `	}` |
|       75 | 2650 | `	return SXRET_OK;` |
|        2 | 2651 | `Synchronize:` |
|        - | 2652 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|        - | 2653 | `	 * statement.` |
|        - | 2654 | `	 */` |
|        9 | 2655 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|        5 | 2656 | `		pGen->pIn++;` |
|        1 | 2657 | `	}` |
|        5 | 2658 | `	return SXRET_OK;` |
|       46 | 2659 | `}` |
|        - | 2660 | `/*` |
|        - | 2661 | ` * Compile the var statement.` |
|        - | 2662 | ` * Symisc Extension:` |
|        - | 2663 | ` *      var statement can be used outside of a class definition.` |
|        - | 2664 | ` */` |
|        2 | 2665 | `PH7_PRIVATE sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|        1 | 2666 | `{` |
|        - | 2667 | `` 	/* `var` is a PROPERTY modifier and nothing else. A class body's `var $x;` `` |
|        - | 2668 | `	 * is compiled by compile_class.c, never here, so reaching this statement` |
|        - | 2669 | ``	 * handler means `var` appeared outside a class — which php rejects as a`` |
|        - | 2670 | `	 * parse error. PHL used to compile it as an ordinary expression statement,` |
|        - | 2671 | ``	 * so top-level `var $x = 1;` silently ran (a Symisc extension). */`` |
|        3 | 2672 | `	PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|        3 | 2673 | `	return SXERR_ABORT;` |
|        1 | 2674 | `}` |
|        - | 2675 | `/*` |
|        - | 2676 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|        - | 2677 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|        - | 2678 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|        - | 2679 | ` */` |
|        - | 2680 | `/*` |
|        - | 2681 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|        - | 2682 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|        - | 2683 | ` * hash and any shared references), this creates a new literal entry with the` |
|        - | 2684 | ` * qualified name and updates the instruction's operand index.` |
|        - | 2685 | ` *` |
|        - | 2686 | ` * Resolution order:` |
|        - | 2687 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|        - | 2688 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|        - | 2689 | ` *   3. Otherwise return the original literal index unchanged.` |
|        - | 2690 | ` *` |
|        - | 2691 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|        - | 2692 | ` * came from an import (step 1) and 0 otherwise.` |
|        - | 2693 | ` * Returns the (possibly new) literal index.` |
|        - | 2694 | ` */` |
|  1228026 | 2695 | `PH7_PRIVATE sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|        5 | 2696 | `{` |
|        - | 2697 | `	ph7_value *pLit;` |
|        - | 2698 | `	const char *zLit;` |
|        - | 2699 | `	SyString sQualified;` |
|        - | 2700 | `	sxu32 nLit;` |
|        - | 2701 | `	sxu32 k;` |
|        - | 2702 | `	sxu32 nNewIdx;` |
|        - | 2703 | `	int hasNsSep;` |
|        - | 2704 | `	SyHashEntry *pImport;` |
|        - | 2705 | `	ph7_value *pNew;` |
|  1228031 | 2706 | `	if( pFromImport ){` |
|  1092608 | 2707 | `		*pFromImport = 0;` |
|   545166 | 2708 | `	}` |
|  1228031 | 2709 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  1228031 | 2710 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|      ! 0 | 2711 | `		return nOrigIdx;` |
|        - | 2712 | `	}` |
|  1228031 | 2713 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  1228031 | 2714 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|        - | 2715 | `	/* Skip if already qualified (contains backslash) */` |
|  1228031 | 2716 | `	hasNsSep = 0;` |
| 10905445 | 2717 | `	for( k = 0; k < nLit; k++ ){` |
|  9677838 | 2718 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
|  4828910 | 2719 | `	}` |
|  1228031 | 2720 | `	if( hasNsSep ){` |
|      424 | 2721 | `		return nOrigIdx;` |
|        - | 2722 | `	}` |
|        - | 2723 | `	/* Check use imports first (works even outside namespaces) */` |
|  1227612 | 2724 | `	SyBlobReset(&pGen->sWorker);` |
|  1227612 | 2725 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  1227612 | 2726 | `	if( pImport ){` |
|      235 | 2727 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      235 | 2728 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      235 | 2729 | `		if( pFromImport ){` |
|       41 | 2730 | `			*pFromImport = 1;` |
|       18 | 2731 | `		}` |
|      120 | 2732 | `	}else{` |
|  1227382 | 2733 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  1227024 | 2734 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|        - | 2735 | `		}` |
|        - | 2736 | `		/* Prepend current namespace */` |
|      363 | 2737 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      363 | 2738 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      363 | 2739 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|        - | 2740 | `	}` |
|        - | 2741 | `	/* Look up or create a new literal for the qualified name */` |
|      593 | 2742 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|      593 | 2743 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      329 | 2744 | `		return nNewIdx; /* Already interned */` |
|        - | 2745 | `	}` |
|      269 | 2746 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      269 | 2747 | `	if( pNew == 0 ){` |
|      ! 0 | 2748 | `		return nOrigIdx; /* OOM, fall back to original */` |
|        - | 2749 | `	}` |
|      269 | 2750 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      269 | 2751 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      269 | 2752 | `	return nNewIdx;` |
|   612786 | 2753 | `}` |
|        - | 2754 | `/*` |
|        - | 2755 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|        - | 2756 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|        - | 2757 | ` */` |
|     8860 | 2758 | `PH7_PRIVATE void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|        5 | 2759 | `{` |
|        - | 2760 | `	SyHashEntry *pImport;` |
|     8865 | 2761 | `	const char *zName = pName->zString;` |
|     8865 | 2762 | `	sxu32 nName = pName->nByte;` |
|     8865 | 2763 | `	sxu32 nFirst = 0;` |
|        - | 2764 | `	/* php resolves a name through use-imports on its LEADING segment: an` |
|        - | 2765 | ``	 * unqualified `C` maps the whole name (`use X\C;` -> X\C), while a QUALIFIED`` |
|        - | 2766 | ``	 * `A\B\C` maps just `A` (`use X\A;` -> X\A\B\C) and keeps the `\B\C` tail.`` |
|        - | 2767 | `	 * The old code looked up the whole qualified string (which never matches a` |
|        - | 2768 | `	 * single-segment import alias) and then blindly prefixed the current` |
|        - | 2769 | ``	 * namespace, so `use PHPUnit\Framework; extends Framework\TestCase` resolved`` |
|        - | 2770 | `	 * to Ns\Framework\TestCase. Mirror GenStateResolveNamespaceLiteral here. */` |
|    84341 | 2771 | `	while( nFirst < nName && zName[nFirst] != '\\' ){ nFirst++; }` |
|     8865 | 2772 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)zName,nFirst);` |
|     8865 | 2773 | `	if( pImport ){` |
|      113 | 2774 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      113 | 2775 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|      113 | 2776 | `		SyBlobAppend(pOut,zName + nFirst,nName - nFirst); /* the \B\C tail, if any */` |
|      113 | 2777 | `		return;` |
|        - | 2778 | `	}` |
|        - | 2779 | `	/* Prepend current namespace if active */` |
|     8757 | 2780 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       56 | 2781 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       56 | 2782 | `		SyBlobAppend(pOut,"\\",1);` |
|       26 | 2783 | `	}` |
|     8757 | 2784 | `	SyBlobAppend(pOut,zName,nName);` |
|     4433 | 2785 | `}` |
|        - | 2786 | `/*` |
|        - | 2787 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|        - | 2788 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|        - | 2789 | ` * The caller must release pOut when done.` |
|        - | 2790 | ` */` |
|    11984 | 2791 | `PH7_PRIVATE void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|        5 | 2792 | `{` |
|    11989 | 2793 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      785 | 2794 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      785 | 2795 | `		SyBlobAppend(pOut,"\\",1);` |
|      390 | 2796 | `	}` |
|    11989 | 2797 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    11989 | 2798 | `}` |
|        - | 2799 | `/*` |
|        - | 2800 | ` * TRUE when pB starts exactly where pA ends. The tokenizer drops whitespace, so` |
|        - | 2801 | ` * source offsets are the only record of it — and php's qualified-name token` |
|        - | 2802 | `` * (`{LABEL}("\\"{LABEL})+`) is matched by its LEXER, which means the `\` has to`` |
|        - | 2803 | `` * be glued: `private\Q` is one name, `private \Q` is a modifier and a type, and`` |
|        - | 2804 | ` * only this test tells the two apart.` |
|        - | 2805 | ` */` |
|      440 | 2806 | `PH7_PRIVATE int GenStateTokensGlued(SyToken *pA,SyToken *pB)` |
|        5 | 2807 | `{` |
|      445 | 2808 | `	return pA->sData.zString + pA->sData.nByte == pB->sData.zString;` |
|        5 | 2809 | `}` |
|        - | 2810 | `/*` |
|        - | 2811 | `` * php's `namespace\X` NAME OPERATOR (5.3): a leading `namespace` keyword glued to`` |
|        - | 2812 | `` * a `\` names the CURRENT namespace, and the whole name is then FULLY QUALIFIED —`` |
|        - | 2813 | `` * `namespace\X` inside `namespace B;` is `\B\X`, and plain `\X` at global scope.`` |
|        - | 2814 | `` * php's lexer matches it as one token (T_NAME_RELATIVE, `"namespace"("\\"{LABEL})+`,`` |
|        - | 2815 | `` * case-insensitively), so the `\` must be GLUED to the keyword: `namespace \X` is a`` |
|        - | 2816 | ` * php parse error, and this mirrors that by comparing source offsets.` |
|        - | 2817 | ` *` |
|        - | 2818 | ` * This predicate only RECOGNIZES the operator (it consumes nothing), which is what` |
|        - | 2819 | `` * the statement dispatcher needs to tell `namespace\X::m();` from a namespace`` |
|        - | 2820 | ` * DECLARATION; GenStateNsRelPrefix below is what the name collectors call.` |
|        - | 2821 | ` */` |
|  2889411 | 2822 | `PH7_PRIVATE int GenStateIsNsRelName(SyToken *pIn,SyToken *pEnd)` |
|        5 | 2823 | `{` |
|  2889411 | 2824 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|  2349267 | 2825 | `	 \|\| (sxu32)SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_NAMESPACE ){` |
|  2888510 | 2826 | `		return 0;` |
|        - | 2827 | `	}` |
|      911 | 2828 | `	if( &pIn[1] >= pEnd \|\| (pIn[1].nType & PH7_TK_NSSEP) == 0 ){` |
|      797 | 2829 | `		return 0;` |
|        - | 2830 | `	}` |
|      117 | 2831 | `	if( &pIn[2] >= pEnd \|\| (pIn[2].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 2832 | ``		return 0; /* php's T_NAME_RELATIVE needs at least one segment after the `\` */`` |
|        - | 2833 | `	}` |
|      117 | 2834 | `	return GenStateTokensGlued(pIn,&pIn[1]);` |
|  1442452 | 2835 | `}` |
|        - | 2836 | `/*` |
|        - | 2837 | `` * Consume a leading `namespace\` (see GenStateIsNsRelName) at *ppIn and seed pOut`` |
|        - | 2838 | ` * with the current namespace plus its separator — nothing at global scope, where` |
|        - | 2839 | ` * the bare name already IS the FQN. Returns TRUE when it fired, and the caller` |
|        - | 2840 | `` * must then treat the name it goes on to collect as ABSOLUTE: no `use` import may`` |
|        - | 2841 | ` * apply to it, and the current namespace is already in place.` |
|        - | 2842 | ` */` |
|   458341 | 2843 | `PH7_PRIVATE int GenStateNsRelPrefix(ph7_gen_state *pGen,SyToken **ppIn,SyToken *pEnd,SyBlob *pOut)` |
|        5 | 2844 | `{` |
|   458346 | 2845 | `	SyToken *pIn = *ppIn;` |
|   458346 | 2846 | `	if( !GenStateIsNsRelName(pIn,pEnd) ){` |
|   458254 | 2847 | `		return 0;` |
|        - | 2848 | `	}` |
|       95 | 2849 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       85 | 2850 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       85 | 2851 | `		SyBlobAppend(pOut,"\\",1);` |
|       41 | 2852 | `	}` |
|       95 | 2853 | `	*ppIn = &pIn[2];` |
|       95 | 2854 | `	return 1;` |
|   228844 | 2855 | `}` |
|        - | 2856 | `/*` |
|        - | 2857 | ` * Compile a namespace statement` |
|        - | 2858 | ` * According to the PHP language reference manual` |
|        - | 2859 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|        - | 2860 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|        - | 2861 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|        - | 2862 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|        - | 2863 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|        - | 2864 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|        - | 2865 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|        - | 2866 | ` *  programming world.` |
|        - | 2867 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|        - | 2868 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|        - | 2869 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|        - | 2870 | ` *  classes/functions/constants.` |
|        - | 2871 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|        - | 2872 | ` *  readability of source code.` |
|        - | 2873 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|        - | 2874 | ` *  Here is an example of namespace syntax in PHP:` |
|        - | 2875 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|        - | 2876 | ` *       class MyClass {}` |
|        - | 2877 | ` *       function myfunction() {}` |
|        - | 2878 | ` *       const MYCONST = 1;` |
|        - | 2879 | ` *       $a = new MyClass;` |
|        - | 2880 | ` *       $c = new \my\name\MyClass;` |
|        - | 2881 | ` *       $a = strlen('hi');` |
|        - | 2882 | ` *       $d = namespace\MYCONST;` |
|        - | 2883 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|        - | 2884 | ` *       echo constant($d);` |
|        - | 2885 | ` * NOTE` |
|        - | 2886 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|        - | 2887 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|        - | 2888 | ` */` |
|        - | 2889 | `/*` |
|        - | 2890 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|        - | 2891 | ` */` |
|       24 | 2892 | `PH7_PRIVATE const char * TokenTypeName(sxu32 nType)` |
|        4 | 2893 | `{` |
|       28 | 2894 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|       21 | 2895 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|       21 | 2896 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|        - | 2897 | `	/* php's parse errors call a reserved word a "token", never a "keyword" — the` |
|        - | 2898 | ``	 * word only ever appears in ITS vocabulary as `unexpected token "while"`. */`` |
|       21 | 2899 | `	if( nType & PH7_TK_KEYWORD ){ return "token"; }` |
|       21 | 2900 | `	if( nType & PH7_TK_FQNAME ){ return "fully qualified name"; }` |
|       19 | 2901 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|       19 | 2902 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|       13 | 2903 | `	return "token";` |
|       16 | 2904 | `}` |
|      398 | 2905 | `PH7_PRIVATE sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|        5 | 2906 | `{` |
|        - | 2907 | `	SyBlob sName;` |
|        - | 2908 | `	sxu32 nLine;` |
|        - | 2909 | `	sxi32 rc;` |
|        - | 2910 | `	int bBracket;` |
|        - | 2911 | `	int bFirst;` |
|      403 | 2912 | `	nLine = pGen->pIn->nLine;` |
|      403 | 2913 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|        - | 2914 | `` 	/* php's grammar has three shapes -- `namespace NAME;`, `namespace NAME { }` `` |
|        - | 2915 | ``	 * and `namespace { }` -- and a bare `namespace;` is none of them. */`` |
|      403 | 2916 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|        3 | 2917 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"{\"");` |
|        3 | 2918 | `		return ( rc == SXERR_ABORT ) ? SXERR_ABORT : SXRET_OK;` |
|        - | 2919 | `	}` |
|      401 | 2920 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|      401 | 2921 | `	if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|        - | 2922 | ``		/* `namespace \A;` -- php lexed a fully-qualified name where its grammar`` |
|        - | 2923 | `		 * wants a plain one, and names the whole token. */` |
|        3 | 2924 | `		SyBlobAppend(&sName,"\\",1);` |
|        3 | 2925 | `		pGen->pIn++;` |
|        5 | 2926 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 | 2927 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      ! 0 | 2928 | `				SyBlobAppend(&sName,"\\",1);` |
|      ! 0 | 2929 | `			}else{` |
|        3 | 2930 | `				SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        - | 2931 | `			}` |
|        3 | 2932 | `			pGen->pIn++;` |
|        1 | 2933 | `		}` |
|        4 | 2934 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - | 2935 | `			"syntax error, unexpected fully qualified name \"%.*s\", expecting \"{\"",` |
|        2 | 2936 | `			(int)SyBlobLength(&sName),(const char *)SyBlobData(&sName));` |
|        3 | 2937 | `		SyBlobRelease(&sName);` |
|        3 | 2938 | `		return ( rc == SXERR_ABORT ) ? SXERR_ABORT : SXRET_OK;` |
|        - | 2939 | `	}` |
|        - | 2940 | ``	/* Collect the namespace path: namespace Foo\Bar\Baz. A `\` that is not glued`` |
|        - | 2941 | `	 * to a following segment is not a separator token any more (the lexer hands` |
|        - | 2942 | ``	 * it over as php's bare T_NS_SEPARATOR), so `namespace A\ B;` and `A\;` stop`` |
|        - | 2943 | `	 * here and are named below the way php names them. */` |
|      983 | 2944 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      589 | 2945 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      107 | 2946 | `			if( SyBlobLength(&sName) > 0 ){` |
|      107 | 2947 | `				SyBlobAppend(&sName,"\\",1);` |
|       51 | 2948 | `			}` |
|       56 | 2949 | `		}else{` |
|      487 | 2950 | `			SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        - | 2951 | `		}` |
|      589 | 2952 | `		pGen->pIn++;` |
|        5 | 2953 | `	}` |
|      399 | 2954 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       21 | 2955 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - | 2956 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       12 | 2957 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       15 | 2958 | `		SyBlobRelease(&sName);` |
|       15 | 2959 | `		return ( rc == SXERR_ABORT ) ? SXERR_ABORT : SXRET_OK;` |
|        - | 2960 | `	}` |
|      387 | 2961 | `	bBracket = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|        - | 2962 | `	/* php's zend_compile_namespace, in its order: the two forms never mix in one` |
|        - | 2963 | `	 * file, a block never nests, and the FIRST declaration of either form must be` |
|        - | 2964 | `	 * the first statement -- declares and empty statements aside. */` |
|      387 | 2965 | `	rc = SXRET_OK;` |
|      387 | 2966 | `	if( !pGen->bNsBracketed ){` |
|      341 | 2967 | `		if( pGen->bNsNamed && bBracket ){` |
|        3 | 2968 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 2969 | `				"Cannot mix bracketed namespace declarations with unbracketed namespace declarations");` |
|        6 | 2970 | `		}` |
|      218 | 2971 | `	}else if( !bBracket ){` |
|        3 | 2972 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 2973 | `			"Cannot mix bracketed namespace declarations with unbracketed namespace declarations");` |
|       49 | 2974 | `	}else if( pGen->bNsNamed \|\| pGen->bInNsBlock ){` |
|        3 | 2975 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Namespace declarations cannot be nested");` |
|        1 | 2976 | `	}` |
|      387 | 2977 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 2978 | `		SyBlobRelease(&sName);` |
|      ! 0 | 2979 | `		return SXERR_ABORT;` |
|        - | 2980 | `	}` |
|      387 | 2981 | `	bFirst = ( (!bBracket && !pGen->bNsNamed) \|\| (bBracket && !pGen->bNsBracketed) );` |
|      387 | 2982 | `	if( bFirst && pGen->bStrictTypesLocked ){` |
|       12 | 2983 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 2984 | `			"Namespace declaration statement has to be the very first statement or after any declare call in the script");` |
|       12 | 2985 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2986 | `			SyBlobRelease(&sName);` |
|      ! 0 | 2987 | `			return SXERR_ABORT;` |
|        - | 2988 | `		}` |
|        5 | 2989 | `	}` |
|        - | 2990 | `	/* Switch namespace and clear the previous imports */` |
|      387 | 2991 | `	SyBlobReset(&pGen->sNamespace);` |
|      387 | 2992 | `	GenStateResetUseImports(&(*pGen),pGen->pVm);` |
|      387 | 2993 | `	if( SyBlobLength(&sName) > 0 ){` |
|      375 | 2994 | `		SyBlobAppend(&pGen->sNamespace,SyBlobData(&sName),SyBlobLength(&sName));` |
|      185 | 2995 | `	}` |
|      387 | 2996 | `	pGen->bNsNamed = (sxi8)( SyBlobLength(&sName) > 0 );` |
|      387 | 2997 | `	SyBlobRelease(&sName);` |
|        - | 2998 | `	/* A namespace statement is code for what follows: a declare after it is late. */` |
|      387 | 2999 | `	pGen->bStrictTypesLocked = 1;` |
|      387 | 3000 | `	if( bBracket ){` |
|       86 | 3001 | `		pGen->bNsBracketed = 1;` |
|       86 | 3002 | `		pGen->bInNsBlock = 1;` |
|       86 | 3003 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       86 | 3004 | `		pGen->bInNsBlock = 0;` |
|        - | 3005 | `		/* php's zend_end_namespace: the block's close ends the namespace, and` |
|        - | 3006 | `		 * whatever comes after it outside a block is refused by the dispatcher. */` |
|       86 | 3007 | `		SyBlobReset(&pGen->sNamespace);` |
|       86 | 3008 | `		GenStateResetUseImports(&(*pGen),pGen->pVm);` |
|       86 | 3009 | `		pGen->bNsNamed = 0;` |
|       86 | 3010 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3011 | `			return SXERR_ABORT;` |
|        - | 3012 | `		}` |
|       41 | 3013 | `	}` |
|      387 | 3014 | `	return SXRET_OK;` |
|      204 | 3015 | `}` |
|        - | 3016 | `/*` |
|        - | 3017 | ` * Initialize the three use-import tables of a code generator.` |
|        - | 3018 | ` *` |
|        - | 3019 | ` * php resolves CLASS and FUNCTION imports case-INSENSITIVELY, like every other` |
|        - | 3020 | ``  * name in those two families: `use A\Cee;` then `CEE::K`, `use A\Cee as Alias;` `` |
|        - | 3021 | `` * then `ALIAS::K`, `use function A\eff;` then `EFF()`, and a wrong-case leading`` |
|        - | 3022 | `` * segment of an imported namespace (`use A\B;` then `b\Cee::K`) all resolve.`` |
|        - | 3023 | ` * So both tables fold through SyStrHash/SyStrnmicmp, exactly like hClass /` |
|        - | 3024 | ` * hMethod / hFunction.` |
|        - | 3025 | ` *` |
|        - | 3026 | ` * The CONST table stays BYTE-EXACT: php keeps constant names case-sensitive,` |
|        - | 3027 | `` * so `use const A\KAY;` followed by `kay` must remain an undefined constant.`` |
|        - | 3028 | ` * That asymmetry is why the three tables exist separately.` |
|        - | 3029 | ` */` |
|    52570 | 3030 | `PH7_PRIVATE void GenStateInitUseImports(ph7_gen_state *pGen,ph7_vm *pVm)` |
|        5 | 3031 | `{` |
|    52575 | 3032 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|    52575 | 3033 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|    52575 | 3034 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|    52575 | 3035 | `}` |
|        - | 3036 | `/*` |
|        - | 3037 | ` * Drop every import currently in scope and start a fresh set (a namespace` |
|        - | 3038 | ` * switch clears imports).  Keeps the case rules of GenStateInitUseImports.` |
|        - | 3039 | ` */` |
|    44635 | 3040 | `PH7_PRIVATE void GenStateResetUseImports(ph7_gen_state *pGen,ph7_vm *pVm)` |
|        5 | 3041 | `{` |
|    44640 | 3042 | `	SyHashRelease(&pGen->hUseImports);` |
|    44640 | 3043 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|    44640 | 3044 | `	SyHashRelease(&pGen->hUseConstImports);` |
|    44640 | 3045 | `	GenStateInitUseImports(&(*pGen),&(*pVm));` |
|    44640 | 3046 | `}` |
|        - | 3047 | `/*` |
|        - | 3048 | ` * The two DECLARED-name tables (classes and functions declared so far in this` |
|        - | 3049 | ` * compile unit). php refuses an import whose name a declaration already took, and` |
|        - | 3050 | ` * the check is per COMPILE UNIT and case-INSENSITIVE — a class declared by a file` |
|        - | 3051 | `` * this one later `require`s is invisible to it, because that file compiles after`` |
|        - | 3052 | ` * this one has finished. Both tables key on the FQN, so they survive a namespace` |
|        - | 3053 | ` * switch (which clears only the imports).` |
|        - | 3054 | ` */` |
|    52106 | 3055 | `PH7_PRIVATE void GenStateInitSeenSymbols(ph7_gen_state *pGen,ph7_vm *pVm)` |
|        5 | 3056 | `{` |
|    52111 | 3057 | `	SyHashInit(&pGen->hSeenClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|    52111 | 3058 | `	SyHashInit(&pGen->hSeenFunc,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|    52111 | 3059 | `}` |
|    44181 | 3060 | `PH7_PRIVATE void GenStateReleaseSeenSymbols(ph7_gen_state *pGen)` |
|        5 | 3061 | `{` |
|    44186 | 3062 | `	SyHashRelease(&pGen->hSeenClass);` |
|    44186 | 3063 | `	SyHashRelease(&pGen->hSeenFunc);` |
|    44186 | 3064 | `}` |
|    44171 | 3065 | `PH7_PRIVATE void GenStateResetSeenSymbols(ph7_gen_state *pGen,ph7_vm *pVm)` |
|        5 | 3066 | `{` |
|    44176 | 3067 | `	GenStateReleaseSeenSymbols(&(*pGen));` |
|    44176 | 3068 | `	GenStateInitSeenSymbols(&(*pGen),&(*pVm));` |
|    44176 | 3069 | `}` |
|        - | 3070 | `/*` |
|        - | 3071 | ` * Record one declared CLASS (bFunc = 0) or FUNCTION (bFunc = 1) FQN so a later` |
|        - | 3072 | `` * `use` in this compile unit can see that the name is taken.`` |
|        - | 3073 | ` */` |
|   184408 | 3074 | `PH7_PRIVATE void GenStateRecordDeclaredName(ph7_gen_state *pGen,int bFunc,const SyString *pFqn)` |
|        5 | 3075 | `{` |
|   184413 | 3076 | `	SyHash *pHash = bFunc ? &pGen->hSeenFunc : &pGen->hSeenClass;` |
|        - | 3077 | `	char *zDup;` |
|   184413 | 3078 | `	if( pFqn->nByte < 1 \|\| SyHashGet(pHash,pFqn->zString,pFqn->nByte) != 0 ){` |
|       33 | 3079 | `		return;` |
|        - | 3080 | `	}` |
|   184385 | 3081 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pFqn->zString,pFqn->nByte);` |
|   184385 | 3082 | `	if( zDup ){` |
|        - | 3083 | `		/* The blob the caller built is released on its way out, so the table owns` |
|        - | 3084 | `		 * a pool copy (freed in bulk with the VM, like the import FQNs). */` |
|   184385 | 3085 | `		SyHashInsert(pHash,zDup,pFqn->nByte,zDup);` |
|    92063 | 3086 | `	}` |
|    92082 | 3087 | `}` |
|        - | 3088 | `/*` |
|        - | 3089 | `` * php refuses a DECLARATION whose short name a local `use` import already took:`` |
|        - | 3090 | ` *` |
|        - | 3091 | ` *   use A\Cee;  class Cee {}    Cannot redeclare class B\Cee (previously declared as local import)` |
|        - | 3092 | ` *   use function A\eff;  function eff(){}` |
|        - | 3093 | ` *                               Cannot redeclare function B\eff() (previously declared as local import)` |
|        - | 3094 | ` *   use const A\KAY;  const KAY = 1;` |
|        - | 3095 | ` *                               Cannot declare const B\KAY because the name is already in use` |
|        - | 3096 | ` *` |
|        - | 3097 | `` * A SELF-import (`use B\Cee;` inside `namespace B;`) names this very declaration`` |
|        - | 3098 | ` * and is a no-op, so it is exempt. iKind: 0 = class family (interface/trait/enum` |
|        - | 3099 | ` * included — php says "class" for all four), 1 = function, 2 = const.` |
|        - | 3100 | ` */` |
|   184590 | 3101 | `PH7_PRIVATE sxi32 GenStateGuardImportRedeclare(ph7_gen_state *pGen,int iKind,` |
|        - | 3102 | `	const SyString *pShort,const SyString *pFqn,sxu32 nLine)` |
|        5 | 3103 | `{` |
|        - | 3104 | `	SyHash *pImports;` |
|        - | 3105 | `	SyHashEntry *pEntry;` |
|        - | 3106 | `	const char *zImported;` |
|        - | 3107 | `	sxu32 nImported;` |
|   184595 | 3108 | `	switch( iKind ){` |
|   178677 | 3109 | `		case 1:  pImports = &pGen->hUseFuncImports; break;` |
|      187 | 3110 | `		case 2:  pImports = &pGen->hUseConstImports; break;` |
|     5741 | 3111 | `		default: pImports = &pGen->hUseImports; break;` |
|        - | 3112 | `	}` |
|   184595 | 3113 | `	pEntry = SyHashGet(pImports,(const void *)pShort->zString,pShort->nByte);` |
|   184595 | 3114 | `	if( pEntry == 0 ){` |
|   184581 | 3115 | `		return SXRET_OK;` |
|        - | 3116 | `	}` |
|       18 | 3117 | `	zImported = (const char *)pEntry->pUserData;` |
|       18 | 3118 | `	nImported = SyStrlen(zImported);` |
|       14 | 3119 | `	if( nImported == pFqn->nByte` |
|       22 | 3120 | `	 && (iKind == 2 ? SyMemcmp((const void *)zImported,(const void *)pFqn->zString,nImported) == 0` |
|        8 | 3121 | `	                : SyStrnicmp(zImported,pFqn->zString,nImported) == 0) ){` |
|        7 | 3122 | `		return SXRET_OK; /* the import IS this declaration */` |
|        - | 3123 | `	}` |
|       13 | 3124 | `	if( iKind == 2 ){` |
|        4 | 3125 | `		return PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        1 | 3126 | `			"Cannot declare const %z because the name is already in use",pFqn);` |
|        - | 3127 | `	}` |
|        8 | 3128 | `	return PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        2 | 3129 | `		iKind == 1 ? "Cannot redeclare function %z() (previously declared as local import)"` |
|        2 | 3130 | `		           : "Cannot redeclare class %z (previously declared as local import)",pFqn);` |
|    92171 | 3131 | `}` |
|        - | 3132 | `/*` |
|        - | 3133 | ` * TRUE when pTok is a PHL KEYWORD token that php's lexer nevertheless hands back` |
|        - | 3134 | ` * as a plain T_STRING. php reserves fewer words than PHL's table does, and the` |
|        - | 3135 | `` * `as` clause of a `use` accepts a T_STRING and nothing else — so `use A\Q as`` |
|        - | 3136 | `` * integer;` is a legal (if odd) php import while `use A\Q as echo;` is a parse`` |
|        - | 3137 | ` * error. These are exactly the type-NAME keywords: php spells its scalar types` |
|        - | 3138 | `` * with ordinary labels, and `self`/`parent` too (only `static` is a real token).`` |
|        - | 3139 | ` */` |
|        2 | 3140 | `static int GenStateKeywordIsPhpLabel(SyToken *pTok)` |
|        1 | 3141 | `{` |
|        - | 3142 | `	sxu32 nKey;` |
|        3 | 3143 | `	if( (pTok->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 3144 | `		return 0;` |
|        - | 3145 | `	}` |
|        3 | 3146 | `	nKey = (sxu32)(SX_PTR_TO_INT(pTok->pUserData));` |
|        4 | 3147 | `	return nKey == PH7_TKWRD_BOOL \|\| nKey == PH7_TKWRD_INT \|\| nKey == PH7_TKWRD_FLOAT` |
|        2 | 3148 | `		\|\| nKey == PH7_TKWRD_STRING \|\| nKey == PH7_TKWRD_OBJECT` |
|        3 | 3149 | `		\|\| nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT;` |
|        2 | 3150 | `}` |
|        - | 3151 | `/*` |
|        - | 3152 | ` * TRUE for the names php refuses to let a CLASS import occupy — zend's reserved` |
|        - | 3153 | ` * class names. Distinct from compile_func.c's GenStateIsReservedTypeWord, which` |
|        - | 3154 | ` * answers "is this word a built-in TYPE rather than a class name": that one also` |
|        - | 3155 | `` * covers the `boolean`/`integer`/`double` aliases, and php imports those happily`` |
|        - | 3156 | `` * (`use A\boolean;` is accepted). Matched case-insensitively, like php.`` |
|        - | 3157 | ` */` |
|      224 | 3158 | `static int GenStateIsReservedClassName(const SyString *pName)` |
|        5 | 3159 | `{` |
|        - | 3160 | `	static const char *azWords[] = {` |
|        - | 3161 | `		"self","parent","static","int","float","string","bool","array","object",` |
|        - | 3162 | `		"null","false","true","void","iterable","mixed","never","callable"` |
|        - | 3163 | `	};` |
|        - | 3164 | `	sxu32 i;` |
|     3995 | 3165 | `	for( i = 0 ; i < SX_ARRAYSIZE(azWords) ; i++ ){` |
|     3775 | 3166 | `		sxu32 n = (sxu32)SyStrlen(azWords[i]);` |
|     3775 | 3167 | `		if( pName->nByte == n && SyStrnicmp(pName->zString,azWords[i],n) == 0 ){` |
|        6 | 3168 | `			return 1;` |
|        - | 3169 | `		}` |
|     1888 | 3170 | `	}` |
|      225 | 3171 | `	return 0;` |
|      117 | 3172 | `}` |
|        - | 3173 | `/*` |
|        - | 3174 | `` * Register one resolved `use` import: alias -> FQN, in the table its KIND owns`` |
|        - | 3175 | ` * (iUseType: 0 = class, 1 = function, 2 = const).  Shared by the plain form` |
|        - | 3176 | `` * (`use A\Cee;`) and by each member of a group (`use A\{Cee, Dee};`).`` |
|        - | 3177 | ` */` |
|      298 | 3178 | `static sxi32 GenStateAddImport(` |
|        - | 3179 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - | 3180 | `	int iUseType,         /* 0=class, 1=function, 2=const */` |
|        - | 3181 | `	SyBlob *pPath,        /* Fully qualified name being imported */` |
|        - | 3182 | `	SyString *pAlias,     /* Short name it is imported under */` |
|        - | 3183 | `	sxu32 nLine           /* Line of the 'use' keyword (for diagnostics) */` |
|        - | 3184 | `	)` |
|        5 | 3185 | `{` |
|        - | 3186 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|        - | 3187 | `	const char *zKind;  /* php's kind word in the "already in use" message */` |
|        - | 3188 | `	char *zDup;` |
|        - | 3189 | `	sxi32 rc;` |
|        - | 3190 | `	/* Select the target hash table based on import type. */` |
|      303 | 3191 | `	switch( iUseType ){` |
|       43 | 3192 | `		case 1:  pGenHash = &pGen->hUseFuncImports; break;` |
|       40 | 3193 | `		case 2:  pGenHash = &pGen->hUseConstImports; break;` |
|      229 | 3194 | `		default: pGenHash = &pGen->hUseImports; break;` |
|        - | 3195 | `	}` |
|        - | 3196 | `	/* php names the KIND of a non-class import in this message: "Cannot use` |
|        - | 3197 | `	 * function A\eff as eff …" / "Cannot use const A\KAY as KAY …". */` |
|      303 | 3198 | `	zKind = iUseType == 1 ? "function " : iUseType == 2 ? "const " : "";` |
|        - | 3199 | `	/* A CLASS import may not take a reserved class name, however it got there:` |
|        - | 3200 | ``	 * as the trailing segment (`use A\self;`) or as an explicit alias`` |
|        - | 3201 | ``	 * (`use A\Q as self;`). Only classes — `use function A\self;` and`` |
|        - | 3202 | ``	 * `use const A\self;` are both accepted by php. */`` |
|      303 | 3203 | `	if( iUseType == 0 && GenStateIsReservedClassName(pAlias) ){` |
|        8 | 3204 | `		return PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 3205 | `			"Cannot use %.*s as %z because '%z' is a special class name",` |
|        4 | 3206 | `			(int)SyBlobLength(pPath),(const char *)SyBlobData(pPath),pAlias,pAlias);` |
|        - | 3207 | `	}` |
|        - | 3208 | `	/* Check for duplicate import alias (per-type) */` |
|      299 | 3209 | `	if( SyHashGet(pGenHash,pAlias->zString,pAlias->nByte) != 0 ){` |
|       12 | 3210 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 3211 | `			"Cannot use %s%.*s as %z because the name is already in use",` |
|        6 | 3212 | `			zKind,(int)SyBlobLength(pPath),(const char *)SyBlobData(pPath),pAlias);` |
|        9 | 3213 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3214 | `			return SXERR_ABORT;` |
|        - | 3215 | `		}` |
|        3 | 3216 | `	}` |
|        - | 3217 | `	/* …and refuses one whose name a DECLARATION in this compile unit already took` |
|        - | 3218 | ``	 * (`class Cee {} use A\Cee;`), unless the import names that very declaration.`` |
|        - | 3219 | `	 * The name an import occupies is the alias in the CURRENT namespace, which is` |
|        - | 3220 | `	 * what the seen tables key on. php runs this check for classes and functions` |
|        - | 3221 | ``	 * only — a `const` declaration followed by its own `use const` is accepted. */`` |
|      299 | 3222 | `	if( iUseType != 2 ){` |
|        - | 3223 | `		SyBlob sTaken;` |
|      263 | 3224 | `		SyBlobInit(&sTaken,&pGen->pVm->sAllocator);` |
|      263 | 3225 | `		GenStateBuildFQN(&(*pGen),pAlias,&sTaken);` |
|      258 | 3226 | `		if( SyHashGet(iUseType == 1 ? &pGen->hSeenFunc : &pGen->hSeenClass,` |
|      387 | 3227 | `				SyBlobData(&sTaken),SyBlobLength(&sTaken)) != 0` |
|      136 | 3228 | `		 && (SyBlobLength(&sTaken) != SyBlobLength(pPath)` |
|        4 | 3229 | `			\|\| SyStrnicmp((const char *)SyBlobData(&sTaken),(const char *)SyBlobData(pPath),` |
|        6 | 3230 | `				(sxu32)SyBlobLength(&sTaken)) != 0) ){` |
|        8 | 3231 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 3232 | `				"Cannot use %s%.*s as %z because the name is already in use",` |
|        4 | 3233 | `				zKind,(int)SyBlobLength(pPath),(const char *)SyBlobData(pPath),pAlias);` |
|        6 | 3234 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3235 | `				SyBlobRelease(&sTaken);` |
|      ! 0 | 3236 | `				return SXERR_ABORT;` |
|        - | 3237 | `			}` |
|        2 | 3238 | `		}` |
|      263 | 3239 | `		SyBlobRelease(&sTaken);` |
|      129 | 3240 | `	}` |
|        - | 3241 | `	/* Register the import: alias -> FQN.` |
|        - | 3242 | `	 * Strings are allocated from the VM pool allocator and freed` |
|        - | 3243 | `	 * when the entire VM is released. SyHashRelease does not free` |
|        - | 3244 | `	 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|      446 | 3245 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      294 | 3246 | `		(const char *)SyBlobData(pPath),SyBlobLength(pPath));` |
|      299 | 3247 | `	if( zDup ){` |
|        - | 3248 | `		/* All three kinds resolve entirely at COMPILE time — a const import is read` |
|        - | 3249 | `		 * by the OP_LOADC candidate builder (compile_node.c), so no runtime table` |
|        - | 3250 | `		 * is needed for it either. */` |
|      299 | 3251 | `		SyHashInsert(pGenHash,pAlias->zString,pAlias->nByte,zDup);` |
|      147 | 3252 | `	}` |
|      299 | 3253 | `	return SXRET_OK;` |
|      154 | 3254 | `}` |
|        - | 3255 | `/*` |
|        - | 3256 | `` * Collect one `\`-separated name into pOut (appending to whatever it holds, with`` |
|        - | 3257 | ` * a separator when needed) and return its LAST segment token, or 0 when the` |
|        - | 3258 | ` * cursor is not on a name at all.` |
|        - | 3259 | ` */` |
|      320 | 3260 | `static SyToken * GenStateCollectNsPath(ph7_gen_state *pGen,SyBlob *pOut)` |
|        5 | 3261 | `{` |
|      325 | 3262 | `	SyToken *pLast = 0;` |
|      325 | 3263 | ``	int bAfterSep = 0;   /* the token just consumed was a `\` */`` |
|     1193 | 3264 | `	while( pGen->pIn < pGen->pEnd ){` |
|     1191 | 3265 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      557 | 3266 | `			bAfterSep = ( pGen->pIn + 1 < pGen->pEnd` |
|      276 | 3267 | `				&& GenStateTokensGlued(pGen->pIn,&pGen->pIn[1]) );` |
|      281 | 3268 | `			pGen->pIn++;` |
|      281 | 3269 | `			continue;` |
|        - | 3270 | `		}` |
|      915 | 3271 | `		if( (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|        - | 3272 | `			/* php lexes a QUALIFIED name as one T_NAME_QUALIFIED token before it` |
|        - | 3273 | `			 * consults the keyword table, so every reserved word is a legal SEGMENT` |
|        - | 3274 | ``			 * of it — `use A\Default\Q;`, `use Default\Q;`, `use A\as;` are all`` |
|        - | 3275 | `			 * accepted — while a BARE reserved word is not a name at all` |
|        - | 3276 | ``			 * (`use Default;` is a php parse error, and so is `use A\{Default};`).`` |
|        - | 3277 | ``			 * A `\` on one side or the other is exactly what separates the two, and`` |
|        - | 3278 | `` 			 * it must be the IMMEDIATE neighbour: the `as` of `use A\Cee as Baz;` `` |
|        - | 3279 | `			 * carries no separator and must still end the path. */` |
|      351 | 3280 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      181 | 3281 | `				break;` |
|        - | 3282 | `			}` |
|      170 | 3283 | `			if( !bAfterSep` |
|      161 | 3284 | `			 && !(pGen->pIn + 1 < pGen->pEnd && (pGen->pIn[1].nType & PH7_TK_NSSEP)` |
|       71 | 3285 | `				&& GenStateTokensGlued(pGen->pIn,&pGen->pIn[1])) ){` |
|       76 | 3286 | `				break;` |
|        - | 3287 | `			}` |
|       14 | 3288 | `		}` |
|      597 | 3289 | `		pLast = pGen->pIn;` |
|      597 | 3290 | `		if( SyBlobLength(pOut) > 0 ){` |
|      315 | 3291 | `			SyBlobAppend(pOut,"\\",1);` |
|      155 | 3292 | `		}` |
|      597 | 3293 | `		SyBlobAppend(pOut,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      597 | 3294 | `		bAfterSep = 0;` |
|      597 | 3295 | `		pGen->pIn++;` |
|        5 | 3296 | `	}` |
|      325 | 3297 | `	return pLast;` |
|        5 | 3298 | `}` |
|        - | 3299 | `/*` |
|        - | 3300 | `` * Park the cursor on the `;` that ends this declaration so a refused `use` does`` |
|        - | 3301 | ` * not leave its remaining tokens for the statement dispatcher to read as an` |
|        - | 3302 | ` * expression, which would pile a second diagnostic on the first.` |
|        - | 3303 | ` */` |
|        4 | 3304 | `static void GenStateSkipToStatementEnd(ph7_gen_state *pGen)` |
|        1 | 3305 | `{` |
|        7 | 3306 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        3 | 3307 | `		pGen->pIn++;` |
|        1 | 3308 | `	}` |
|        5 | 3309 | `}` |
|        - | 3310 | `/*` |
|        - | 3311 | `` * `namespace\X` lexes as php's T_NAME_RELATIVE, and a `use` statement takes a`` |
|        - | 3312 | ` * plain or fully-qualified name only. php refuses it and NAMES the token kind in` |
|        - | 3313 | ` * the message, a wording TokenTypeName cannot spell. Consumes the name, reports,` |
|        - | 3314 | ` * and answers TRUE when it fired; zExpecting is php's trailing clause (empty for` |
|        - | 3315 | ` * the plain form, the member list for a group).` |
|        - | 3316 | ` */` |
|      322 | 3317 | `static int GenStateUseRejectNsRelName(ph7_gen_state *pGen,sxu32 nLine,const char *zExpecting,sxi32 *pRc)` |
|        5 | 3318 | `{` |
|        - | 3319 | `	SyBlob sName;` |
|      327 | 3320 | `	if( !GenStateIsNsRelName(pGen->pIn,pGen->pEnd) ){` |
|      325 | 3321 | `		return 0;` |
|        - | 3322 | `	}` |
|        3 | 3323 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|        3 | 3324 | `	SyBlobAppend(&sName,"namespace",sizeof("namespace")-1);` |
|        3 | 3325 | ``	pGen->pIn++; /* the `namespace` keyword; the `\` and its segments follow */`` |
|        5 | 3326 | `	while( pGen->pIn + 1 < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP)` |
|        5 | 3327 | `		&& (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 | 3328 | `		SyBlobAppend(&sName,"\\",1);` |
|        3 | 3329 | `		SyBlobAppend(&sName,pGen->pIn[1].sData.zString,pGen->pIn[1].sData.nByte);` |
|        3 | 3330 | `		pGen->pIn += 2;` |
|        1 | 3331 | `	}` |
|        5 | 3332 | `	*pRc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - | 3333 | `		"syntax error, unexpected namespace-relative name \"%.*s\"%s",` |
|        2 | 3334 | `		(int)SyBlobLength(&sName),(const char *)SyBlobData(&sName),zExpecting);` |
|        3 | 3335 | `	SyBlobRelease(&sName);` |
|        3 | 3336 | `	return 1;` |
|      166 | 3337 | `}` |
|        - | 3338 | `/*` |
|        - | 3339 | ` * TRUE when pTok is a PHL IDENTIFIER that php's lexer nevertheless reserves. The` |
|        - | 3340 | `` * alpha operators (`and`, `or`, `xor`, `new`, `clone`, `instanceof`) reach the`` |
|        - | 3341 | `` * parser here as PH7_TK_ID\|PH7_TK_OP, and `readonly`/`callable` are`` |
|        - | 3342 | ` * context-sensitive identifiers — php has a real token for every one of them, so` |
|        - | 3343 | ` * none may stand where its grammar asks for a T_STRING.` |
|        - | 3344 | ` */` |
|      140 | 3345 | `static int GenStateIdIsPhpKeyword(SyToken *pTok)` |
|        5 | 3346 | `{` |
|        - | 3347 | `	static const char *azWords[] = {` |
|        - | 3348 | `		"and","or","xor","new","clone","instanceof","readonly","callable"` |
|        - | 3349 | `	};` |
|        - | 3350 | `	sxu32 i;` |
|      145 | 3351 | `	if( (pTok->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 | 3352 | `		return 0;` |
|        - | 3353 | `	}` |
|     1265 | 3354 | `	for( i = 0 ; i < SX_ARRAYSIZE(azWords) ; i++ ){` |
|     1125 | 3355 | `		sxu32 n = (sxu32)SyStrlen(azWords[i]);` |
|     1125 | 3356 | `		if( pTok->sData.nByte == n && SyStrnicmp(pTok->sData.zString,azWords[i],n) == 0 ){` |
|      ! 0 | 3357 | `			return 1;` |
|        - | 3358 | `		}` |
|      565 | 3359 | `	}` |
|      145 | 3360 | `	return 0;` |
|       75 | 3361 | `}` |
|        - | 3362 | `/*` |
|        - | 3363 | `` * Consume the optional `as Alias` clause, leaving *pAlias untouched when absent.`` |
|        - | 3364 | ` * php's grammar takes a T_STRING there and nothing else, so a reserved word is a` |
|        - | 3365 | ` * parse error however PHL's lexer happens to have classified it. Returns TRUE` |
|        - | 3366 | ` * when it reported one, and the caller must then abandon the declaration.` |
|        - | 3367 | ` */` |
|      300 | 3368 | `static int GenStateCollectImportAlias(ph7_gen_state *pGen,SyString *pAlias,sxi32 *pRc)` |
|        5 | 3369 | `{` |
|      300 | 3370 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      225 | 3371 | `		&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|      147 | 3372 | `		pGen->pIn++; /* Jump 'as' */` |
|      147 | 3373 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 3374 | `			return 0;` |
|        - | 3375 | `		}` |
|      147 | 3376 | `		if( (pGen->pIn->nType & PH7_TK_ID) && !GenStateIdIsPhpKeyword(pGen->pIn) ){` |
|      145 | 3377 | `			*pAlias = pGen->pIn->sData;` |
|      145 | 3378 | `			pGen->pIn++;` |
|      145 | 3379 | `			return 0;` |
|        - | 3380 | `		}` |
|        3 | 3381 | `		if( GenStateKeywordIsPhpLabel(pGen->pIn) ){` |
|        - | 3382 | ``			/* php spells its scalar types and `self`/`parent` with plain labels,`` |
|        - | 3383 | ``			 * so those ARE legal aliases — `use A\Q as integer;` compiles, and`` |
|        - | 3384 | ``			 * `use A\Q as self;` reaches the special-class-name check instead. */`` |
|      ! 0 | 3385 | `			*pAlias = pGen->pIn->sData;` |
|      ! 0 | 3386 | `			pGen->pIn++;` |
|      ! 0 | 3387 | `			return 0;` |
|        - | 3388 | `		}` |
|        - | 3389 | `		{` |
|        - | 3390 | `			/* php prints a reserved word LOWER-CASED in this message, whatever the` |
|        - | 3391 | ``			 * source spelled: `use A\Q as Default;` reads `unexpected token`` |
|        - | 3392 | ``			 * "default"`. Longest reserved word here is `include_once` (12). */`` |
|        - | 3393 | `			char zLower[16];` |
|        3 | 3394 | `			SyString sTok = pGen->pIn->sData;` |
|        - | 3395 | `			sxu32 i;` |
|        4 | 3396 | `			int bKeyword = ( (pGen->pIn->nType & PH7_TK_KEYWORD) != 0` |
|        2 | 3397 | `				\|\| GenStateIdIsPhpKeyword(pGen->pIn) );` |
|        5 | 3398 | `			int bWord = ( (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        2 | 3399 | `				&& sTok.nByte <= sizeof(zLower) );` |
|       17 | 3400 | `			for( i = 0 ; bWord && i < sTok.nByte ; i++ ){` |
|       15 | 3401 | `				unsigned char c = (unsigned char)sTok.zString[i];` |
|       15 | 3402 | `				zLower[i] = (char)((c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c);` |
|        8 | 3403 | `			}` |
|        3 | 3404 | `			if( bWord ){` |
|        3 | 3405 | `				SyStringInitFromBuf(&sTok,zLower,sTok.nByte);` |
|        1 | 3406 | `			}` |
|        - | 3407 | ``			/* `die` and `exit` are the same token to php, and it names it `exit`. */`` |
|        3 | 3408 | `			if( bWord && sTok.nByte == 3 && SyMemcmp(sTok.zString,"die",3) == 0 ){` |
|      ! 0 | 3409 | `				SyStringInitFromBuf(&sTok,"exit",sizeof("exit")-1);` |
|      ! 0 | 3410 | `			}` |
|        4 | 3411 | `			*pRc = PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|        - | 3412 | `				"syntax error, unexpected %s \"%z\", expecting identifier",` |
|        1 | 3413 | `				bKeyword ? "token" : TokenTypeName(pGen->pIn->nType),&sTok);` |
|        - | 3414 | `		}` |
|        3 | 3415 | `		return 1;` |
|        - | 3416 | `	}` |
|      163 | 3417 | `	return 0;` |
|      155 | 3418 | `}` |
|        - | 3419 | `/*` |
|        - | 3420 | `` * Park the cursor on whichever of `}` / `;` ends the group, so a refused member`` |
|        - | 3421 | ` * does not cascade into the ones after it.` |
|        - | 3422 | ` */` |
|      ! 0 | 3423 | `static void GenStateSkipToGroupEnd(ph7_gen_state *pGen)` |
|      ! 0 | 3424 | `{` |
|      ! 0 | 3425 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_CCB\|PH7_TK_SEMI)) == 0 ){` |
|      ! 0 | 3426 | `		pGen->pIn++;` |
|      ! 0 | 3427 | `	}` |
|      ! 0 | 3428 | `}` |
|        - | 3429 | `/*` |
|        - | 3430 | ` * Compile the members of a GROUP use declaration (php 7.0):` |
|        - | 3431 | ` *` |
|        - | 3432 | ` *      use A\{Cee, Dee as D2, Sub\Eee};` |
|        - | 3433 | ` *      use function A\{f, g as h};` |
|        - | 3434 | ` *      use A\{function f, const K, Cee};   // per-member kind, untyped group only` |
|        - | 3435 | ` *` |
|        - | 3436 | `` * pPrefix holds the path before the brace; the cursor sits on `{`.  Each member`` |
|        - | 3437 | `` * is the prefix, a `\`, and the member's own (possibly multi-segment) name.  A`` |
|        - | 3438 | ` * trailing comma is allowed, an empty group is not.` |
|        - | 3439 | ` */` |
|       16 | 3440 | `static sxi32 GenStateCompileGroupUse(ph7_gen_state *pGen,SyBlob *pPrefix,int iUseType,sxu32 nLine)` |
|        3 | 3441 | `{` |
|        - | 3442 | `	SyBlob sPath;` |
|       19 | 3443 | `	sxi32 rc = SXRET_OK;` |
|       19 | 3444 | `	pGen->pIn++; /* Jump '{' */` |
|       19 | 3445 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       17 | 3446 | `	for(;;){` |
|       37 | 3447 | `		int iMemberType = iUseType;` |
|        - | 3448 | `		SyString sAlias;` |
|        - | 3449 | `		SyToken *pLast;` |
|        - | 3450 | ``		/* `function`/`const` may qualify a single member, but only inside a`` |
|        - | 3451 | ``		 * group that is not itself typed (php rejects `use function A\{const C}`). */`` |
|       37 | 3452 | `		if( iUseType == 0 && pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        5 | 3453 | `			sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|        5 | 3454 | `			if( nKey == PH7_TKWRD_FUNCTION ){` |
|        3 | 3455 | `				iMemberType = 1;` |
|        3 | 3456 | `				pGen->pIn++;` |
|        4 | 3457 | `			}else if( nKey == PH7_TKWRD_CONST ){` |
|        3 | 3458 | `				iMemberType = 2;` |
|        3 | 3459 | `				pGen->pIn++;` |
|        1 | 3460 | `			}` |
|        2 | 3461 | `		}` |
|       37 | 3462 | `		SyBlobReset(&sPath);` |
|       37 | 3463 | `		SyBlobAppend(&sPath,SyBlobData(pPrefix),SyBlobLength(pPrefix));` |
|       37 | 3464 | `		if( GenStateUseRejectNsRelName(&(*pGen),nLine,` |
|        - | 3465 | `				", expecting identifier or namespaced name or \"function\" or \"const\"",&rc) ){` |
|      ! 0 | 3466 | `			GenStateSkipToGroupEnd(&(*pGen));` |
|      ! 0 | 3467 | `			break;` |
|        - | 3468 | `		}` |
|       37 | 3469 | `		pLast = GenStateCollectNsPath(pGen,&sPath);` |
|       37 | 3470 | `		if( pLast == 0 ){` |
|        - | 3471 | ``			/* No member name: `use A\{};` or a stray token.  Report once, then`` |
|        - | 3472 | `			 * skip to the end of the group so the statement does not cascade. */` |
|      ! 0 | 3473 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - | 3474 | `				"syntax error, unexpected %s \"%z\", expecting identifier",` |
|      ! 0 | 3475 | `				TokenTypeName(pGen->pIn < pGen->pEnd ? pGen->pIn->nType : 0),` |
|      ! 0 | 3476 | `				pGen->pIn < pGen->pEnd ? &pGen->pIn->sData : 0);` |
|      ! 0 | 3477 | `			GenStateSkipToGroupEnd(&(*pGen));` |
|      ! 0 | 3478 | `			break;` |
|        - | 3479 | `		}` |
|       37 | 3480 | `		sAlias = pLast->sData; /* Default alias is the member's last component */` |
|       37 | 3481 | `		if( GenStateCollectImportAlias(pGen,&sAlias,&rc) ){` |
|      ! 0 | 3482 | `			GenStateSkipToGroupEnd(&(*pGen));` |
|      ! 0 | 3483 | `			break;` |
|        - | 3484 | `		}` |
|       37 | 3485 | `		rc = GenStateAddImport(&(*pGen),iMemberType,&sPath,&sAlias,nLine);` |
|       37 | 3486 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3487 | `			break;` |
|        - | 3488 | `		}` |
|       37 | 3489 | `		rc = SXRET_OK;` |
|       37 | 3490 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       23 | 3491 | `			pGen->pIn++;` |
|       23 | 3492 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) ){` |
|        3 | 3493 | `				break; /* Trailing comma before the closing brace */` |
|        - | 3494 | `			}` |
|       21 | 3495 | `			continue;` |
|        - | 3496 | `		}` |
|       17 | 3497 | `		break;` |
|      ! 0 | 3498 | `	}` |
|       19 | 3499 | `	SyBlobRelease(&sPath);` |
|       19 | 3500 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 3501 | `		return SXERR_ABORT;` |
|        - | 3502 | `	}` |
|       19 | 3503 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) ){` |
|       19 | 3504 | `		pGen->pIn++; /* Jump '}' */` |
|        8 | 3505 | `	}` |
|       19 | 3506 | `	return SXRET_OK;` |
|       11 | 3507 | `}` |
|        - | 3508 | `/*` |
|        - | 3509 | ` * Compile the 'use' statement` |
|        - | 3510 | ` * According to the PHP language reference manual` |
|        - | 3511 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|        - | 3512 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|        - | 3513 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|        - | 3514 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|        - | 3515 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|        - | 3516 | ` *  a function or constant is not supported.` |
|        - | 3517 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|        - | 3518 | ` * NOTE` |
|        - | 3519 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|        - | 3520 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|        - | 3521 | ` */` |
|      286 | 3522 | `PH7_PRIVATE sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|        5 | 3523 | `{` |
|        - | 3524 | `	sxu32 nLine;` |
|        - | 3525 | `	sxi32 rc;` |
|        - | 3526 | `	SyBlob sPath;` |
|        - | 3527 | `	SyString sAlias;` |
|        - | 3528 | `	SyToken *pLast;` |
|        - | 3529 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|        - | 3530 | `	int bGroup;` |
|      291 | 3531 | `	nLine = pGen->pIn->nLine;` |
|      291 | 3532 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|        - | 3533 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|      291 | 3534 | `	iUseType = 0;` |
|      291 | 3535 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       73 | 3536 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|       73 | 3537 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|       39 | 3538 | `			iUseType = 1;` |
|       39 | 3539 | `			pGen->pIn++;` |
|       55 | 3540 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|       36 | 3541 | `			iUseType = 2;` |
|       36 | 3542 | `			pGen->pIn++;` |
|       16 | 3543 | `		}` |
|       34 | 3544 | `	}` |
|      291 | 3545 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|        - | 3546 | `	/* Process one or more use declarations separated by commas */` |
|      144 | 3547 | `	for(;;){` |
|      293 | 3548 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 3549 | `			break;` |
|        - | 3550 | `		}` |
|      293 | 3551 | `		SyBlobReset(&sPath);` |
|      293 | 3552 | `		if( GenStateUseRejectNsRelName(&(*pGen),nLine,"",&rc) ){` |
|        3 | 3553 | `			SyBlobRelease(&sPath);` |
|        3 | 3554 | `			GenStateSkipToStatementEnd(&(*pGen));` |
|        3 | 3555 | `			return ( rc == SXERR_ABORT ) ? SXERR_ABORT : SXRET_OK;` |
|        - | 3556 | `		}` |
|        - | 3557 | `		/* Collect the full namespace path */` |
|      291 | 3558 | `		pLast = GenStateCollectNsPath(pGen,&sPath);` |
|        - | 3559 | ``		/* php's group form is `NAME \ {`: the separator before the brace is the`` |
|        - | 3560 | `		 * one bare T_NS_SEPARATOR its grammar takes, a token of its own (spaces` |
|        - | 3561 | ``		 * on either side are fine), and `use A\B{C}` without it is refused. */`` |
|      291 | 3562 | `		bGroup = 0;` |
|      286 | 3563 | `		if( pGen->pIn + 1 < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OTHER)` |
|      147 | 3564 | `		 && pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\\'` |
|       21 | 3565 | `		 && (pGen->pIn[1].nType & PH7_TK_OCB) ){` |
|       19 | 3566 | `			pGen->pIn++;` |
|       19 | 3567 | `			bGroup = 1;` |
|      281 | 3568 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OTHER)` |
|      139 | 3569 | `		 && pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\\' ){` |
|        - | 3570 | ``			/* `use A\ B;` -- php's parser is past the separator and wants the brace,`` |
|        - | 3571 | `			 * so it names what stood there instead. */` |
|      ! 0 | 3572 | `			rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn + 1 < pGen->pEnd ? &pGen->pIn[1] : 0,"\"{\"");` |
|      ! 0 | 3573 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 3574 | `			GenStateSkipToStatementEnd(&(*pGen));` |
|      ! 0 | 3575 | `			return ( rc == SXERR_ABORT ) ? SXERR_ABORT : SXRET_OK;` |
|        - | 3576 | `		}` |
|      291 | 3577 | `		if( bGroup && SyBlobLength(&sPath) > 0 ){` |
|        - | 3578 | `			/* GROUP declaration: what was collected is the shared prefix.  php` |
|        - | 3579 | `			 * does not let a group be comma-combined with another declaration,` |
|        - | 3580 | `			 * so the members close the statement. */` |
|       19 | 3581 | `			rc = GenStateCompileGroupUse(&(*pGen),&sPath,iUseType,nLine);` |
|       19 | 3582 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3583 | `				SyBlobRelease(&sPath);` |
|      ! 0 | 3584 | `				return SXERR_ABORT;` |
|        - | 3585 | `			}` |
|       19 | 3586 | `			break;` |
|        - | 3587 | `		}` |
|      275 | 3588 | `		if( pLast == 0 ){` |
|        - | 3589 | `			/* Empty path */` |
|        6 | 3590 | `			break;` |
|        - | 3591 | `		}` |
|        - | 3592 | `		/* Default alias is the last component of the path */` |
|      271 | 3593 | `		sAlias = pLast->sData;` |
|        - | 3594 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      271 | 3595 | `		if( GenStateCollectImportAlias(pGen,&sAlias,&rc) ){` |
|        3 | 3596 | `			SyBlobRelease(&sPath);` |
|        3 | 3597 | `			GenStateSkipToStatementEnd(&(*pGen));` |
|        3 | 3598 | `			return ( rc == SXERR_ABORT ) ? SXERR_ABORT : SXRET_OK;` |
|        - | 3599 | `		}` |
|      269 | 3600 | `		rc = GenStateAddImport(&(*pGen),iUseType,&sPath,&sAlias,nLine);` |
|      269 | 3601 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3602 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 3603 | `			return SXERR_ABORT;` |
|        - | 3604 | `		}` |
|        - | 3605 | `		/* Check for comma (multiple use declarations) */` |
|      269 | 3606 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|        3 | 3607 | `			pGen->pIn++;` |
|        2 | 3608 | `		}else{` |
|      136 | 3609 | `			break;` |
|        - | 3610 | `		}` |
|        1 | 3611 | `	}` |
|      287 | 3612 | `	SyBlobRelease(&sPath);` |
|      287 | 3613 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        8 | 3614 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|        4 | 3615 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        6 | 3616 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3617 | `			return SXERR_ABORT;` |
|        - | 3618 | `		}` |
|        2 | 3619 | `	}` |
|      287 | 3620 | `	return SXRET_OK;` |
|      148 | 3621 | `}` |
|        - | 3622 | `/*` |
|        - | 3623 | ` * Compile the stupid 'declare' language construct.` |
|        - | 3624 | ` *` |
|        - | 3625 | ` * According to the PHP language reference manual.` |
|        - | 3626 | ` *  The declare construct is used to set execution directives for a block of code.` |
|        - | 3627 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|        - | 3628 | ` *  declare (directive)` |
|        - | 3629 | ` *   statement` |
|        - | 3630 | ` * The directive section allows the behavior of the declare block to be set.` |
|        - | 3631 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|        - | 3632 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|        - | 3633 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|        - | 3634 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|        - | 3635 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|        - | 3636 | ` * <?php` |
|        - | 3637 | ` * // these are the same:` |
|        - | 3638 | ` * // you can use this:` |
|        - | 3639 | ` * declare(ticks=1) {` |
|        - | 3640 | ` *   // entire script here` |
|        - | 3641 | ` * }` |
|        - | 3642 | ` * // or you can use this:` |
|        - | 3643 | ` * declare(ticks=1);` |
|        - | 3644 | ` * // entire script here` |
|        - | 3645 | ` * ?>` |
|        - | 3646 | ` *` |
|        - | 3647 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|        - | 3648 | ` */` |
|        - | 3649 | `/*` |
|        - | 3650 | ` * Match a directive name against a known literal (case-insensitive).` |
|        - | 3651 | ` */` |
|      120 | 3652 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|        5 | 3653 | `{` |
|      177 | 3654 | `	return SyStringLength(pName) == nWant` |
|      120 | 3655 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|        5 | 3656 | `}` |
|        - | 3657 |  |
|       64 | 3658 | `PH7_PRIVATE sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|        5 | 3659 | `{` |
|       69 | 3660 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       69 | 3661 | `	SyToken *pBodyEnd = 0;` |
|        - | 3662 | `	SyToken *pBodyStart;` |
|        - | 3663 | `	SyToken *pCursor;` |
|        - | 3664 | `	int bHasStrictTypes;` |
|        - | 3665 | `	int bBlockForm;` |
|        - | 3666 | `	int bPlacementOk;` |
|        - | 3667 | `	sxi32 rc;` |
|       69 | 3668 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|       69 | 3669 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|        6 | 3670 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|        6 | 3671 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3672 | `			return SXERR_ABORT;` |
|        - | 3673 | `		}` |
|        6 | 3674 | `		goto Synchro;` |
|        - | 3675 | `	}` |
|       65 | 3676 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|       65 | 3677 | `	pBodyStart = pGen->pIn;` |
|        - | 3678 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|       65 | 3679 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|       65 | 3680 | `	if( pBodyEnd >= pGen->pEnd ){` |
|      ! 0 | 3681 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\")\"");` |
|      ! 0 | 3682 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3683 | `			return SXERR_ABORT;` |
|        - | 3684 | `		}` |
|      ! 0 | 3685 | `		return SXRET_OK;` |
|        - | 3686 | `	}` |
|        - | 3687 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|        - | 3688 | `	 * now delimits the comma-separated directive list. */` |
|       65 | 3689 | `	pGen->pIn = &pBodyEnd[1];` |
|       65 | 3690 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      ! 0 | 3691 | `		if( pGen->pIn >= pGen->pEnd && GenStateAtChunkEofStmt(pGen) ){` |
|        - | 3692 | `			/* Ran out of input: php's parser has an unfinished statement and names` |
|        - | 3693 | ``			 * that, not a sentence about `declare` (the shared end-of-input check`` |
|        - | 3694 | `			 * in compile.c words every other statement the same way). */` |
|      ! 0 | 3695 | `			rc = PH7_GenSyntaxError(&(*pGen),0,0);` |
|      ! 0 | 3696 | `		}else{` |
|      ! 0 | 3697 | `			rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"declare: Expecting ';' or '{' after directive");` |
|        - | 3698 | `		}` |
|      ! 0 | 3699 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3700 | `			return SXERR_ABORT;` |
|        - | 3701 | `		}` |
|      ! 0 | 3702 | `	}` |
|       65 | 3703 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|       65 | 3704 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|       65 | 3705 | `	bHasStrictTypes = 0;` |
|        - | 3706 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|        - | 3707 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|        - | 3708 | `	 * directive appears anywhere in the list, before validating values. */` |
|       65 | 3709 | `	pCursor = pBodyStart;` |
|       83 | 3710 | `	while( pCursor < pBodyEnd ){` |
|       77 | 3711 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|       65 | 3712 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|       59 | 3713 | `				bHasStrictTypes = 1;` |
|       59 | 3714 | `				break;` |
|        - | 3715 | `			}` |
|        3 | 3716 | `		}` |
|       20 | 3717 | `		pCursor++;` |
|        2 | 3718 | `	}` |
|       65 | 3719 | `	if( bHasStrictTypes && bBlockForm ){` |
|        3 | 3720 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 3721 | `			"strict_types declaration must not use block mode");` |
|        3 | 3722 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        3 | 3723 | `		return SXRET_OK;` |
|        - | 3724 | `	}` |
|       63 | 3725 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|        9 | 3726 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 3727 | `			"strict_types declaration must be the very first statement in the script");` |
|        9 | 3728 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        9 | 3729 | `		return SXRET_OK;` |
|        - | 3730 | `	}` |
|        - | 3731 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|       57 | 3732 | `	pCursor = pBodyStart;` |
|      109 | 3733 | `	while( pCursor < pBodyEnd ){` |
|        - | 3734 | `		SyToken *pNameTok;` |
|        - | 3735 | `		SyToken *pEqTok;` |
|        - | 3736 | `		SyToken *pValTok;` |
|        - | 3737 | `		SyString *pDirName;` |
|        - | 3738 | `		int bIsStrict;` |
|        - | 3739 | `		int iStrictValue;` |
|       59 | 3740 | `		pNameTok = pCursor;` |
|       59 | 3741 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 3742 | `			rc = PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        - | 3743 | `				"declare: Expecting a directive name");` |
|      ! 0 | 3744 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 3745 | `			return SXRET_OK;` |
|        - | 3746 | `		}` |
|       59 | 3747 | `		pEqTok = pNameTok + 1;` |
|       59 | 3748 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|      ! 0 | 3749 | `			rc = PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        - | 3750 | `				"declare: Expecting '=' after directive name");` |
|      ! 0 | 3751 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 3752 | `			return SXRET_OK;` |
|        - | 3753 | `		}` |
|       59 | 3754 | `		pValTok = pEqTok + 1;` |
|       59 | 3755 | `		if( pValTok >= pBodyEnd ){` |
|      ! 0 | 3756 | `			rc = PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        - | 3757 | `				"declare: Expecting value after '='");` |
|      ! 0 | 3758 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 3759 | `			return SXRET_OK;` |
|        - | 3760 | `		}` |
|       59 | 3761 | `		pDirName = &pNameTok->sData;` |
|       59 | 3762 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|       59 | 3763 | `		if( bIsStrict ){` |
|        - | 3764 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|        - | 3765 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|       53 | 3766 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      ! 0 | 3767 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 3768 | `					"declare(strict_types) value must be a literal");` |
|      ! 0 | 3769 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 3770 | `				return SXRET_OK;` |
|        - | 3771 | `			}` |
|       53 | 3772 | `			iStrictValue = -1;` |
|       53 | 3773 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|       53 | 3774 | `				const char *zv = SyStringData(&pValTok->sData);` |
|       53 | 3775 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|       53 | 3776 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|       51 | 3777 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|       24 | 3778 | `			}` |
|       53 | 3779 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|        3 | 3780 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 3781 | `					"strict_types declaration must have 0 or 1 as its value");` |
|        3 | 3782 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        3 | 3783 | `				return SXRET_OK;` |
|        - | 3784 | `			}` |
|       51 | 3785 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|       31 | 3786 | `		}else if( DeclareNameIs(pDirName, "encoding", sizeof("encoding")-1) ){` |
|        - | 3787 | `			/* php always ignores declare(encoding=...) unless it was built with` |
|        - | 3788 | `			 * Zend multibyte, and says so in these exact words. */` |
|        - | 3789 | `			/* php's E_COMPILE_WARNING (probe-verified against 8.5). */` |
|        3 | 3790 | `			PH7_GenCompileError(&(*pGen),128 /* E_COMPILE_WARNING */,nLine,` |
|        - | 3791 | `				"declare(encoding=...) ignored because Zend multibyte feature is turned off by settings");` |
|        1 | 3792 | `		}else{` |
|        - | 3793 | `			/* Other directives (ticks and friends) are accepted as no-ops.` |
|        - | 3794 | `			 * This used to emit a NOTICE naming the upstream engine and its` |
|        - | 3795 | `			 * version ("the declare construct is a no-op in the current release` |
|        - | 3796 | `			 * of the PH7(2.1.4) engine") — php prints nothing at all for` |
|        - | 3797 | ``			 * `declare(ticks=1)`, and leaking the old engine's branding into`` |
|        - | 3798 | `			 * user-visible diagnostics was wrong on its own. */` |
|        - | 3799 | `		}` |
|       57 | 3800 | `		pCursor = pValTok + 1;` |
|        - | 3801 | `		/* Consume separating comma (or end). */` |
|       57 | 3802 | `		if( pCursor < pBodyEnd ){` |
|        3 | 3803 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|      ! 0 | 3804 | `				rc = PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        - | 3805 | `					"declare: Expecting ',' or ')' after directive value");` |
|      ! 0 | 3806 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 3807 | `				return SXRET_OK;` |
|        - | 3808 | `			}` |
|        3 | 3809 | `			pCursor++;` |
|        1 | 3810 | `		}` |
|        5 | 3811 | `	}` |
|        - | 3812 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|        - | 3813 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|        - | 3814 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|       55 | 3815 | `	return SXRET_OK;` |
|        2 | 3816 | `Synchro:` |
|        - | 3817 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|       16 | 3818 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       12 | 3819 | `		pGen->pIn++;` |
|        2 | 3820 | `	}` |
|        6 | 3821 | `	return SXRET_OK;` |
|       37 | 3822 | `}` |
|        - | 3823 | `/*` |
|        - | 3824 | ` * Compile a class constant.` |
|        - | 3825 | ` * According to the PHP language reference manual` |
|        - | 3826 | ` *  Class Constants` |
|        - | 3827 | ` *   It is possible to define constant values on a per-class basis remaining` |
|        - | 3828 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|        - | 3829 | ` *   you don't use the $ symbol to declare or use them.` |
|        - | 3830 | ` *   The value must be a constant expression, not (for example) a variable,` |
|        - | 3831 | ` *   a property, a result of a mathematical operation, or a function call.` |
|        - | 3832 | ` *   It's also possible for interfaces to have constants.` |
|        - | 3833 | ` * Symisc eXtension.` |
|        - | 3834 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|        - | 3835 | ` *  the zend engine would allow only simple scalar value.` |
|        - | 3836 | ` *  Example:` |
|        - | 3837 | ` *   class Test{` |
|        - | 3838 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|        - | 3839 | ` *   };` |
|        - | 3840 | ` *   var_dump(TEST::MyConst);` |
|        - | 3841 | ` *   Refer to the official documentation for more information on the powerful extension` |
|        - | 3842 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|        - | 3843 | ` */` |
|        - | 3844 | `/*` |
|        - | 3845 | ` * Exception handling.` |
|        - | 3846 | ` *  According to the PHP language reference manual` |
|        - | 3847 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|        - | 3848 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|        - | 3849 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|        - | 3850 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|        - | 3851 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|        - | 3852 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|        - | 3853 | ` *    (or re-thrown) within a catch block.` |
|        - | 3854 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|        - | 3855 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|        - | 3856 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|        - | 3857 | ` *    been defined with set_exception_handler().` |
|        - | 3858 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|        - | 3859 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|        - | 3860 | ` */` |
|        - | 3861 | `/*` |
|        - | 3862 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|        - | 3863 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|        - | 3864 | ` * indicates failure.` |
|        - | 3865 | ` */` |
|   103827 | 3866 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 3867 | `{` |
|        - | 3868 | ``	/* php decides throwability at RUNTIME: `throw 5` compiles and raises the`` |
|        - | 3869 | `	 * catchable Error "Can only throw objects" when it executes, and a` |
|        - | 3870 | `	 * non-Throwable object raises "Cannot throw objects that do not implement` |
|        - | 3871 | `	 * Throwable". This used to whitelist a handful of expression shapes and` |
|        - | 3872 | `	 * reject the rest at compile time as a "friendlier" error, which meant the` |
|        - | 3873 | `	 * program never ran and no catch could ever see it — and it was inconsistent` |
|        - | 3874 | ``	 * anyway, since `throw $v` (a variable holding an int) always compiled and`` |
|        - | 3875 | `	 * fell through to the same runtime path. OP_THROW now owns the whole rule,` |
|        - | 3876 | ``	 * so accept any expression here; `throw;` with no operand is still rejected`` |
|        - | 3877 | `	 * by PH7_CompileThrow's SXERR_EMPTY branch. */` |
|    51841 | 3878 | `	SXUNUSED(pGen);` |
|    51841 | 3879 | `	SXUNUSED(pRoot);` |
|   103832 | 3880 | `	return SXRET_OK;` |
|        5 | 3881 | `}` |
|        - | 3882 | `/*` |
|        - | 3883 | ` * Compile a 'throw' statement.` |
|        - | 3884 | ` * throw: This is how you trigger an exception.` |
|        - | 3885 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|        - | 3886 | ` */` |
|   103781 | 3887 | `PH7_PRIVATE sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|        5 | 3888 | `{` |
|   103786 | 3889 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 3890 | `	GenBlock *pBlock;` |
|        - | 3891 | `	sxu32 nIdx;` |
|        - | 3892 | `	sxi32 rc;` |
|   103786 | 3893 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|        - | 3894 | `	/* Compile the expression */` |
|   103786 | 3895 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|   103786 | 3896 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 | 3897 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|      ! 0 | 3898 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3899 | `			return SXERR_ABORT;` |
|        - | 3900 | `		}` |
|      ! 0 | 3901 | `		return SXRET_OK;` |
|        - | 3902 | `	}` |
|   103786 | 3903 | `	pBlock = pGen->pCurrent;` |
|        - | 3904 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   477081 | 3905 | `	while(pBlock->pParent){` |
|   477055 | 3906 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|   103760 | 3907 | `			break;` |
|        - | 3908 | `		}` |
|        - | 3909 | `		/* Point to the parent block */` |
|   373300 | 3910 | `		pBlock = pBlock->pParent;` |
|        5 | 3911 | `	}` |
|        - | 3912 | `	/* Emit the throw instruction */` |
|   103786 | 3913 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|        - | 3914 | `	/* Emit the jump */` |
|   103786 | 3915 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|   103786 | 3916 | `	return SXRET_OK;` |
|    51823 | 3917 | `}` |
|        - | 3918 | `/*` |
|        - | 3919 | ` * Compile a PHP 8.0 'throw' expression.` |
|        - | 3920 | ` * Called from the expression code generator when a 'throw' keyword is` |
|        - | 3921 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|        - | 3922 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|        - | 3923 | ` * the validator guarantees the operand is a valid exception target.` |
|        - | 3924 | ` */` |
|       46 | 3925 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|        5 | 3926 | `{` |
|       51 | 3927 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 3928 | `	GenBlock *pBlock;` |
|        - | 3929 | `	sxu32 nIdx;` |
|        - | 3930 | `	sxi32 rc;` |
|       23 | 3931 | `	(void)iCompileFlag;` |
|       51 | 3932 | `	pGen->pIn++; /* Skip 'throw' */` |
|       51 | 3933 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 3934 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 3935 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 3936 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3937 | `			return SXERR_ABORT;` |
|        - | 3938 | `		}` |
|      ! 0 | 3939 | `		return SXRET_OK;` |
|        - | 3940 | `	}` |
|       51 | 3941 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|       51 | 3942 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 3943 | `		return SXERR_ABORT;` |
|        - | 3944 | `	}` |
|       51 | 3945 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 | 3946 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 3947 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 3948 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3949 | `			return SXERR_ABORT;` |
|        - | 3950 | `		}` |
|      ! 0 | 3951 | `		return SXRET_OK;` |
|        - | 3952 | `	}` |
|        - | 3953 | `	/* Walk up to nearest exception/function block for the jump target */` |
|       51 | 3954 | `	pBlock = pGen->pCurrent;` |
|       77 | 3955 | `	while( pBlock->pParent ){` |
|       66 | 3956 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|       40 | 3957 | `			break;` |
|        - | 3958 | `		}` |
|       28 | 3959 | `		pBlock = pBlock->pParent;` |
|        2 | 3960 | `	}` |
|       51 | 3961 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       51 | 3962 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|       51 | 3963 | `	return SXRET_OK;` |
|       28 | 3964 | `}` |
|        - | 3965 | `/*` |
|        - | 3966 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|        - | 3967 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|        - | 3968 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|        - | 3969 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|        - | 3970 | ` * compile error propagated from the parser.` |
|        - | 3971 | ` */` |
|       74 | 3972 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|        5 | 3973 | `{` |
|        - | 3974 | `	SyString sClassName;` |
|        - | 3975 | `	SyToken *pToken;` |
|        - | 3976 | `	SyString *pName;` |
|        - | 3977 | `	char *zDup;` |
|        - | 3978 | `	sxi32 rc;` |
|       79 | 3979 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       79 | 3980 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|       79 | 3981 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|        - | 3982 | `	/* Inline catches compile into the function's own container; pByteCode stays NULL. */` |
|       79 | 3983 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 | 3984 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 3985 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 3986 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 3987 | `		return SXERR_INVALID;` |
|        - | 3988 | `	}` |
|       79 | 3989 | `	pGen->pIn++; /* '(' */` |
|       37 | 3990 | `	for(;;){` |
|        - | 3991 | `		SyBlob sResolved;` |
|       79 | 3992 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       79 | 3993 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 3994 | `			SyBlobRelease(&sResolved);` |
|      ! 0 | 3995 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 3996 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 3997 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 3998 | `			return SXERR_INVALID;` |
|        - | 3999 | `		}` |
|      116 | 4000 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       74 | 4001 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       79 | 4002 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|       79 | 4003 | `		SyBlobRelease(&sResolved);` |
|       79 | 4004 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|       79 | 4005 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|       79 | 4006 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       74 | 4007 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|        5 | 4008 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|      ! 0 | 4009 | `			pGen->pIn++; continue;` |
|        - | 4010 | `		}` |
|       79 | 4011 | `		break;` |
|      ! 0 | 4012 | `	}` |
|        - | 4013 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|        - | 4014 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding. */` |
|       79 | 4015 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|        3 | 4016 | `		pGen->pIn++; /* ')' */` |
|        3 | 4017 | `		return SXRET_OK;` |
|        - | 4018 | `	}` |
|       72 | 4019 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|       77 | 4020 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 4021 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 4022 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 4023 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 4024 | `		return SXERR_INVALID;` |
|        - | 4025 | `	}` |
|       77 | 4026 | `	pGen->pIn++; /* '$' */` |
|       77 | 4027 | `	pName = &pGen->pIn->sData;` |
|       77 | 4028 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       77 | 4029 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|       77 | 4030 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|       77 | 4031 | `	pGen->pIn++;` |
|       77 | 4032 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 | 4033 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 4034 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 4035 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 4036 | `		return SXERR_INVALID;` |
|        - | 4037 | `	}` |
|       77 | 4038 | `	pGen->pIn++; /* ')' */` |
|       77 | 4039 | `	return SXRET_OK;` |
|       42 | 4040 | `}` |
|        - | 4041 | `/*` |
|        - | 4042 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|        - | 4043 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|        - | 4044 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|        - | 4045 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|        - | 4046 | ` * VmThrowException):` |
|        - | 4047 | ` *` |
|        - | 4048 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|        - | 4049 | ` *    <try body>` |
|        - | 4050 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|        - | 4051 | ` *    JMP  -> finally\|end` |
|        - | 4052 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|        - | 4053 | ` *    <catch body>` |
|        - | 4054 | ` *    JMP  -> finally\|end` |
|        - | 4055 | ` *    ... more catches ...` |
|        - | 4056 | ` *  Lfin: <finally body>` |
|        - | 4057 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|        - | 4058 | ` *  Lend:` |
|        - | 4059 | ` */` |
|      132 | 4060 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|        5 | 4061 | `{` |
|      137 | 4062 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 4063 | `	GenBlock *pTry;` |
|        - | 4064 | `	VmInstr *pInstr;` |
|      137 | 4065 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|        - | 4066 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|        - | 4067 | `	sxi32 rc;` |
|      137 | 4068 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|        - | 4069 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION; passed at` |
|        - | 4070 | `	 * ENTRY so GenStateEnterBlock can classify the scope with it) */` |
|      203 | 4071 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),` |
|       66 | 4072 | `		pException,&pTry);` |
|      137 | 4073 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      137 | 4074 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|      137 | 4075 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|      137 | 4076 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      137 | 4077 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      137 | 4078 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|      137 | 4079 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|        - | 4080 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|      137 | 4081 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|      137 | 4082 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|      137 | 4083 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|      137 | 4084 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 4085 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|      137 | 4086 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|        - | 4087 | `	/* Catch clauses (inline) */` |
|      137 | 4088 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      132 | 4089 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       79 | 4090 | `		sxu32 k = 0;` |
|      111 | 4091 | `		for(;;){` |
|        - | 4092 | `			ph7_exception_block sCatch;` |
|        - | 4093 | `			GenBlock *pCatchBlk;` |
|      153 | 4094 | `			sxu32 idxJmp = 0;` |
|      148 | 4095 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      140 | 4096 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|       42 | 4097 | `				break;` |
|        - | 4098 | `			}` |
|       79 | 4099 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|       79 | 4100 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       79 | 4101 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|       79 | 4102 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|       79 | 4103 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|        - | 4104 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|        - | 4105 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|        - | 4106 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump).` |
|        - | 4107 | `			 * Passed at ENTRY: GenStateEnterBlock reads it to classify the block's scope. */` |
|      116 | 4108 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),` |
|       37 | 4109 | `				pException,&pCatchBlk);` |
|       79 | 4110 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       79 | 4111 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|       79 | 4112 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       79 | 4113 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|       79 | 4114 | `			GenStateLeaveBlock(&(*pGen),0);` |
|        - | 4115 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|        - | 4116 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|       79 | 4117 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       79 | 4118 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|       79 | 4119 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|       79 | 4120 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|       79 | 4121 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       79 | 4122 | `			k++;` |
|        5 | 4123 | `		}` |
|       37 | 4124 | `	}` |
|        - | 4125 | `	/* Finally (inline) */` |
|      137 | 4126 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      106 | 4127 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|        - | 4128 | `		GenBlock *pFinBlk;` |
|       69 | 4129 | `		pGen->pIn++; /* Jump 'finally' */` |
|       69 | 4130 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|      101 | 4131 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FINALLY,` |
|       32 | 4132 | `			PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|       69 | 4133 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       69 | 4134 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       69 | 4135 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       69 | 4136 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|       69 | 4137 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       69 | 4138 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|       69 | 4139 | `		pException->iHasFinally = 1;` |
|       32 | 4140 | `	}` |
|      137 | 4141 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|      137 | 4142 | `	pException->iInlined = 1;` |
|        - | 4143 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|        - | 4144 | `	{` |
|      137 | 4145 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|        - | 4146 | `		sxu32 *aJ; sxu32 n;` |
|      137 | 4147 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|      137 | 4148 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|      137 | 4149 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|      211 | 4150 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|       79 | 4151 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|       79 | 4152 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|       42 | 4153 | `		}` |
|        - | 4154 | `	}` |
|      137 | 4155 | `	SySetRelease(&aCatchJmp);` |
|      137 | 4156 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|      ! 0 | 4157 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|      ! 0 | 4158 | `	}` |
|      137 | 4159 | `	return SXRET_OK;` |
|       71 | 4160 | `}` |
|        - | 4161 | `/*` |
|        - | 4162 | ` * Compile a 'catch' block.` |
|        - | 4163 | ` * Catch: A "catch" block retrieves an exception and creates` |
|        - | 4164 | ` * an object containing the exception information.` |
|        - | 4165 | ` */` |
|     4920 | 4166 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|        5 | 4167 | `{` |
|     4925 | 4168 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 4169 | `	ph7_exception_block sCatch;` |
|        - | 4170 | `	SySet *pInstrContainer;` |
|        - | 4171 | `	SyString sClassName;` |
|        - | 4172 | `	GenBlock *pCatch;` |
|        - | 4173 | `	SyToken *pToken;` |
|        - | 4174 | `	SyString *pName;` |
|        - | 4175 | `	char *zDup;` |
|        - | 4176 | `	sxi32 rc;` |
|     4925 | 4177 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|        - | 4178 | `	/* Zero the structure */` |
|     4925 | 4179 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|        - | 4180 | `	/* Initialize fields */` |
|     4925 | 4181 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|        - | 4182 | `	/* The catch body gets its own bytecode array, allocated (not embedded) so its address` |
|        - | 4183 | `	 * survives both this stack frame and any later growth of pException->sEntry — a` |
|        - | 4184 | `	 * break/continue inside the body records it in its JumpFixup (see JumpFixup). */` |
|     4925 | 4185 | `	sCatch.pByteCode = (SySet *)SyMemBackendAlloc(&pException->pVm->sAllocator,sizeof(SySet));` |
|     4925 | 4186 | `	if( sCatch.pByteCode == 0 ){` |
|      ! 0 | 4187 | `		goto Mem;` |
|        - | 4188 | `	}` |
|     4925 | 4189 | `	SySetInit(sCatch.pByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     4925 | 4190 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|        - | 4191 | `			/* Unexpected token,break immediately */` |
|      ! 0 | 4192 | `			pToken = pGen->pIn;` |
|      ! 0 | 4193 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 4194 | `				pToken--;` |
|      ! 0 | 4195 | `			}` |
|      ! 0 | 4196 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 4197 | `				"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 4198 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 4199 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 4200 | `				return SXERR_ABORT;` |
|        - | 4201 | `			}` |
|      ! 0 | 4202 | `			return SXERR_INVALID;` |
|        - | 4203 | `	}` |
|        - | 4204 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     4925 | 4205 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     2473 | 4206 | `	for(;;){` |
|        - | 4207 | `		SyBlob sResolved;` |
|     4959 | 4208 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     4959 | 4209 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|        6 | 4210 | `			SyBlobRelease(&sResolved);` |
|        6 | 4211 | `			pToken = pGen->pIn;` |
|        6 | 4212 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 4213 | `				pToken--;` |
|      ! 0 | 4214 | `			}` |
|        8 | 4215 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 4216 | `				"syntax error, unexpected %s \"%z\"",` |
|        2 | 4217 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|        6 | 4218 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 4219 | `				return SXERR_ABORT;` |
|        - | 4220 | `			}` |
|        6 | 4221 | `			return SXERR_INVALID;` |
|        - | 4222 | `		}` |
|        - | 4223 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|        - | 4224 | `		 * transient SyBlob allocation. */` |
|     7426 | 4225 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     4950 | 4226 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     4955 | 4227 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     4955 | 4228 | `		SyBlobRelease(&sResolved);` |
|     4955 | 4229 | `		if( zDup == 0 ){` |
|      ! 0 | 4230 | `			goto Mem;` |
|        - | 4231 | `		}` |
|     4955 | 4232 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     4955 | 4233 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 4234 | `			goto Mem;` |
|        - | 4235 | `		}` |
|        - | 4236 | `		/* Check for '\|' (multi-catch separator) */` |
|     4950 | 4237 | `		if( pGen->pIn < pGen->pEnd &&` |
|     4950 | 4238 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|       39 | 4239 | `			pGen->pIn->sData.nByte == 1 &&` |
|       34 | 4240 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|       37 | 4241 | `			pGen->pIn++; /* Consume the '\|' */` |
|       37 | 4242 | `			continue;` |
|        - | 4243 | `		}` |
|     4921 | 4244 | `		break;` |
|      ! 0 | 4245 | `	}` |
|        - | 4246 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|        - | 4247 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding;` |
|        - | 4248 | `	 * jump straight to compiling the block below. */` |
|     4921 | 4249 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) /*)*/ ){` |
|        7 | 4250 | `		goto CatchBody;` |
|        - | 4251 | `	}` |
|     4910 | 4252 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     4915 | 4253 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - | 4254 | `			/* Unexpected token,break immediately */` |
|      ! 0 | 4255 | `			pToken = pGen->pIn;` |
|      ! 0 | 4256 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 4257 | `				pToken--;` |
|      ! 0 | 4258 | `			}` |
|      ! 0 | 4259 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 4260 | `				"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 4261 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 4262 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 4263 | `				return SXERR_ABORT;` |
|        - | 4264 | `			}` |
|      ! 0 | 4265 | `			return SXERR_INVALID;` |
|        - | 4266 | `	}` |
|     4915 | 4267 | `	pGen->pIn++; /* Jump the dollar sign */` |
|        - | 4268 | `	/* Duplicate instance name */` |
|     4915 | 4269 | `	pName = &pGen->pIn->sData;` |
|     4915 | 4270 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     4915 | 4271 | `	if( zDup == 0 ){` |
|      ! 0 | 4272 | `		goto Mem;` |
|        - | 4273 | `	}` |
|     4915 | 4274 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     4915 | 4275 | `	pGen->pIn++;` |
|     2462 | 4276 | `CatchBody:` |
|     4921 | 4277 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|        - | 4278 | `		/* Unexpected token,break immediately */` |
|      ! 0 | 4279 | `		pToken = pGen->pIn;` |
|      ! 0 | 4280 | `		if( pToken >= pGen->pEnd ){` |
|      ! 0 | 4281 | `			pToken--;` |
|      ! 0 | 4282 | `		}` |
|      ! 0 | 4283 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 4284 | `			"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 4285 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 4286 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4287 | `			return SXERR_ABORT;` |
|        - | 4288 | `		}` |
|      ! 0 | 4289 | `		return SXERR_INVALID;` |
|        - | 4290 | `	}` |
|        - | 4291 | `	/* Compile the block */` |
|     4921 | 4292 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|        - | 4293 | `	/* Create the catch block. GEN_BLOCK_DETACHED: the body below compiles into` |
|        - | 4294 | `	 * sCatch.pByteCode, not into the enclosing function's array. */` |
|     4921 | 4295 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION\|GEN_BLOCK_DETACHED,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     4921 | 4296 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 4297 | `		return SXERR_ABORT;` |
|        - | 4298 | `	}` |
|        - | 4299 | `	/* Swap bytecode container */` |
|     4921 | 4300 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     4921 | 4301 | `	PH7_VmSetByteCodeContainer(pGen->pVm,sCatch.pByteCode);` |
|        - | 4302 | `	/* Compile the block */` |
|     4921 | 4303 | `	PH7_CompileBlock(&(*pGen),0);` |
|        - | 4304 | `	/* Fix forward jumps now the destination is resolved  */` |
|     4921 | 4305 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 4306 | `	/* Emit the DONE instruction */` |
|     4921 | 4307 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - | 4308 | `	/* Leave the block */` |
|     4921 | 4309 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 4310 | `	/* Restore the default container */` |
|     4921 | 4311 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - | 4312 | `	/* Install the catch block */` |
|     4921 | 4313 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     4921 | 4314 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 4315 | `		goto Mem;` |
|        - | 4316 | `	}` |
|     4921 | 4317 | `	return SXRET_OK;` |
|      ! 0 | 4318 | `Mem:` |
|      ! 0 | 4319 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 4320 | `	return SXERR_ABORT;` |
|     2461 | 4321 | `}` |
|        - | 4322 | `/*` |
|        - | 4323 | ` * Compile a 'try' block.` |
|        - | 4324 | ` * A function using an exception should be in a "try" block.` |
|        - | 4325 | ` * If the exception does not trigger, the code will continue` |
|        - | 4326 | ` * as normal. However if the exception triggers, an exception` |
|        - | 4327 | ` * is "thrown".` |
|        - | 4328 | ` */` |
|     5218 | 4329 | `PH7_PRIVATE sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|        5 | 4330 | `{` |
|        - | 4331 | `	ph7_exception *pException;` |
|     5223 | 4332 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 4333 | `	GenBlock *pTry;` |
|        - | 4334 | `	sxu32 nJmpIdx;` |
|        - | 4335 | `	sxi32 rc;` |
|        - | 4336 | `	/* Create the exception container */` |
|     5223 | 4337 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     5223 | 4338 | `	if( pException == 0 ){` |
|      ! 0 | 4339 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|      ! 0 | 4340 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 4341 | `		return SXERR_ABORT;` |
|        - | 4342 | `	}` |
|        - | 4343 | `	/* Zero the structure */` |
|     5223 | 4344 | `	SyZero(pException,sizeof(ph7_exception));` |
|        - | 4345 | `	/* Initialize fields */` |
|     5223 | 4346 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     5223 | 4347 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     5223 | 4348 | `	pException->iHasFinally = 0;` |
|     5223 | 4349 | `	pException->iFinallyDone = 0;` |
|     5223 | 4350 | `	pException->pVm = pGen->pVm;` |
|        - | 4351 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|        - | 4352 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the DETACHED path`` |
|        - | 4353 | `	 * below — deliberately, not pending migration: it is the proven one, and inlining was` |
|        - | 4354 | ``	 * scoped to generators so no other code path changed. `bInlineTryCatch` is 1 since the`` |
|        - | 4355 | ``	 * inline VM handlers landed, so `bInGenerator` is what actually selects here. */`` |
|     5223 | 4356 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|      137 | 4357 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|        - | 4358 | `	}` |
|        - | 4359 | `	/* Create the try block */` |
|        - | 4360 | `	/* pUserData is the exception context, passed at ENTRY (not assigned after) because` |
|        - | 4361 | `	 * GenStateEnterBlock reads it to classify the block's try/catch scope — see aScope.` |
|        - | 4362 | `	 * It is also what a break/continue crossing this try emits its POP_EXCEPTION with. */` |
|     7630 | 4363 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),` |
|     2539 | 4364 | `		pException,&pTry);` |
|     5091 | 4365 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 4366 | `		return SXERR_ABORT;` |
|        - | 4367 | `	}` |
|        - | 4368 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     5091 | 4369 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|        - | 4370 | `	/* Fix the jump later when the destination is resolved */` |
|     5091 | 4371 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     5091 | 4372 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|        - | 4373 | `	/* Compile the block */` |
|     5091 | 4374 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     5091 | 4375 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 4376 | `		return SXERR_ABORT;` |
|        - | 4377 | `	}` |
|        - | 4378 | `	/* Fix forward jumps now the destination is resolved */` |
|     5091 | 4379 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 4380 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     5091 | 4381 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|        - | 4382 | `	/* Leave the block */` |
|     5091 | 4383 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 4384 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     5091 | 4385 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     5084 | 4386 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|        - | 4387 | `		/* Compile one or more catch blocks */` |
|     4907 | 4388 | `		for(;;){` |
|     9830 | 4389 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     8149 | 4390 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     2456 | 4391 | `					break;` |
|        - | 4392 | `			}` |
|     4925 | 4393 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     4925 | 4394 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 4395 | `				return SXERR_ABORT;` |
|        - | 4396 | `			}` |
|        5 | 4397 | `		}` |
|     2451 | 4398 | `	}` |
|        - | 4399 | `	/* Compile optional finally block */` |
|     5091 | 4400 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     2474 | 4401 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|        - | 4402 | `		SySet *pInstrContainer;` |
|        - | 4403 | `		GenBlock *pFinBlock;` |
|      265 | 4404 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|        - | 4405 | `		/* Create the finally block for jump fixup bookkeeping (detached: the body` |
|        - | 4406 | `		 * compiles into pException->sFinally, see GEN_BLOCK_DETACHED). */` |
|      395 | 4407 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION\|GEN_BLOCK_DETACHED\|GEN_BLOCK_FINALLY,` |
|      130 | 4408 | `			PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      265 | 4409 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 4410 | `			return SXERR_ABORT;` |
|        - | 4411 | `		}` |
|        - | 4412 | `		/* Swap bytecode container */` |
|      265 | 4413 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      265 | 4414 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|        - | 4415 | `		/* Compile the finally body */` |
|      265 | 4416 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      265 | 4417 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4418 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 | 4419 | `			return SXERR_ABORT;` |
|        - | 4420 | `		}` |
|        - | 4421 | `		/* Fix forward jumps now the destination is resolved */` |
|      265 | 4422 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 4423 | `		/* Emit DONE to terminate the finally block */` |
|      265 | 4424 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - | 4425 | `		/* Leave the block */` |
|      265 | 4426 | `		GenStateLeaveBlock(&(*pGen),0);` |
|        - | 4427 | `		/* Restore the default container */` |
|      265 | 4428 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      265 | 4429 | `		pException->iHasFinally = 1;` |
|      130 | 4430 | `	}` |
|        - | 4431 | `	/* Must have at least one catch or finally */` |
|     5091 | 4432 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|        9 | 4433 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 4434 | `			"Cannot use try without catch or finally");` |
|        9 | 4435 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4436 | `			return SXERR_ABORT;` |
|        - | 4437 | `		}` |
|        3 | 4438 | `	}` |
|     5091 | 4439 | `	return SXRET_OK;` |
|     2610 | 4440 | `}` |
|        - | 4441 | `/*` |
|        - | 4442 | ` * Compile a switch block.` |
|        - | 4443 | ` *  (See block-comment below for more information)` |
|        - | 4444 | ` */` |
|      244 | 4445 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|        5 | 4446 | `{` |
|      249 | 4447 | `	sxi32 rc = SXRET_OK;` |
|      249 | 4448 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|        - | 4449 | `		/* Unexpected token */` |
|      ! 0 | 4450 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 | 4451 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4452 | `			return SXERR_ABORT;` |
|        - | 4453 | `		}` |
|      ! 0 | 4454 | `		pGen->pIn++;` |
|      ! 0 | 4455 | `	}` |
|      249 | 4456 | `	pGen->pIn++;` |
|        - | 4457 | `	/* First instruction to execute in this block. */` |
|      249 | 4458 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|        - | 4459 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|        - | 4460 | `	 * or the '}' token */` |
|      381 | 4461 | `	for(;;){` |
|      767 | 4462 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 4463 | `			/* No more input to process */` |
|      ! 0 | 4464 | `			break;` |
|        - | 4465 | `		}` |
|      767 | 4466 | `		rc = SXRET_OK;` |
|      767 | 4467 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      181 | 4468 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|       81 | 4469 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|        - | 4470 | `					/* Unexpected token */` |
|      ! 0 | 4471 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,"Unexpected token '%z'",` |
|      ! 0 | 4472 | `						&pGen->pIn->sData);` |
|      ! 0 | 4473 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 4474 | `						return SXERR_ABORT;` |
|        - | 4475 | `					}` |
|        - | 4476 | `					/* FALL THROUGH */` |
|      ! 0 | 4477 | `				}` |
|       81 | 4478 | `				rc = SXERR_EOF;` |
|       81 | 4479 | `				break;` |
|        - | 4480 | `			}` |
|       55 | 4481 | `		}else{` |
|        - | 4482 | `			sxi32 nKwrd;` |
|        - | 4483 | `			/* Extract the keyword */` |
|      591 | 4484 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      591 | 4485 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|       86 | 4486 | `				break;` |
|        - | 4487 | `			}` |
|      429 | 4488 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|        7 | 4489 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|        - | 4490 | `					/* Unexpected token */` |
|      ! 0 | 4491 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,"Unexpected token '%z'",` |
|      ! 0 | 4492 | `						&pGen->pIn->sData);` |
|      ! 0 | 4493 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 4494 | `						return SXERR_ABORT;` |
|        - | 4495 | `					}` |
|        - | 4496 | `					/* FALL THROUGH */` |
|      ! 0 | 4497 | `				}` |
|        - | 4498 | `				/* Block compiled */` |
|        7 | 4499 | `				break;` |
|        - | 4500 | `			}` |
|        - | 4501 | `		}` |
|        - | 4502 | `		/* Compile block */` |
|      523 | 4503 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      523 | 4504 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4505 | `			return SXERR_ABORT;` |
|        - | 4506 | `		}` |
|        5 | 4507 | `	}` |
|      249 | 4508 | `	return rc;` |
|      127 | 4509 | `}` |
|        - | 4510 | `/*` |
|        - | 4511 | ` * Compile a case eXpression.` |
|        - | 4512 | ` *  (See block-comment below for more information)` |
|        - | 4513 | ` */` |
|      194 | 4514 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|        5 | 4515 | `{` |
|        - | 4516 | `	SySet *pInstrContainer;` |
|        - | 4517 | `	SyToken *pEnd,*pTmp;` |
|      199 | 4518 | `	sxi32 iNest = 0;` |
|      199 | 4519 | ``	sxi32 iQuesty = 0;  /* `?`s opened in the case expression and still unclosed */`` |
|        - | 4520 | `	sxi32 rc;` |
|        - | 4521 | ``	/* Delimit the expression. The `:` that ends a case label is the one standing`` |
|        - | 4522 | `	 * outside every paren AND outside every open ternary: php reads the whole` |
|        - | 4523 | `` 	 * expression first, so `case \PHP_VERSION_ID < 80100 ? \T_CLASS : \T_ENUM:` `` |
|        - | 4524 | `	 * is one label with three colons' worth of punctuation in it. Stopping at the` |
|        - | 4525 | `` 	 * first colon cut that label at the ternary's, and the leftover `\T_ENUM:` `` |
|        - | 4526 | ``	 * came back as `syntax error, unexpected token ":"` -- it is how nette/utils`` |
|        - | 4527 | `	 * spells its token switch, so no phpstan run got past its own bootstrap. A` |
|        - | 4528 | ``	 * `?:` closes itself here (its two tokens are adjacent), and a named`` |
|        - | 4529 | ``	 * argument's `:` sits at iNest >= 1 where neither test can see it. */`` |
|      199 | 4530 | `	pEnd = pGen->pIn;` |
|      499 | 4531 | `	while( pEnd < pGen->pEnd ){` |
|      499 | 4532 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|        - | 4533 | `			/* Increment nesting level */` |
|       16 | 4534 | `			iNest++;` |
|      492 | 4535 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|        - | 4536 | `			/* Decrement nesting level */` |
|       16 | 4537 | `			iNest--;` |
|      475 | 4538 | `		}else if( iNest < 1 && (pEnd->nType & PH7_TK_OP)` |
|      226 | 4539 | `			&& pEnd->sData.nByte == 1 && pEnd->sData.zString[0] == '?' ){` |
|        - | 4540 | ``			/* `??` and `?->` are tokens of their own, so this cannot see them */`` |
|       11 | 4541 | `			iQuesty++;` |
|      466 | 4542 | `		}else if( (pEnd->nType & PH7_TK_COLON) && iNest < 1 ){` |
|      209 | 4543 | `			if( iQuesty < 1 ){` |
|      199 | 4544 | `				break;` |
|        - | 4545 | `			}` |
|       11 | 4546 | `			iQuesty--;` |
|      262 | 4547 | `		}else if( (pEnd->nType & PH7_TK_SEMI/*';'*/) && iNest < 1 ){` |
|      ! 0 | 4548 | `			break;` |
|        - | 4549 | `		}` |
|      305 | 4550 | `		pEnd++;` |
|        5 | 4551 | `	}` |
|      199 | 4552 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 4553 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|      ! 0 | 4554 | `		if( rc == SXERR_ABORT ){` |
|        - | 4555 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 4556 | `			return SXERR_ABORT;` |
|        - | 4557 | `		}` |
|      ! 0 | 4558 | `	}` |
|        - | 4559 | `	/* Swap token stream */` |
|      199 | 4560 | `	pTmp = pGen->pEnd;` |
|      199 | 4561 | `	pGen->pEnd = pEnd;` |
|      199 | 4562 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      199 | 4563 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      199 | 4564 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 4565 | `	/* Emit the done instruction */` |
|      199 | 4566 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      199 | 4567 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - | 4568 | `	/* Update token stream */` |
|      199 | 4569 | `	pGen->pIn  = pEnd;` |
|      199 | 4570 | `	pGen->pEnd = pTmp;` |
|      199 | 4571 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 4572 | `		return SXERR_ABORT;` |
|        - | 4573 | `	}` |
|      199 | 4574 | `	return SXRET_OK;` |
|      102 | 4575 | `}` |
|        - | 4576 | `/*` |
|        - | 4577 | ` * Compile the smart switch statement.` |
|        - | 4578 | ` * According to the PHP language reference manual` |
|        - | 4579 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|        - | 4580 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|        - | 4581 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|        - | 4582 | ` *  This is exactly what the switch statement is for.` |
|        - | 4583 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|        - | 4584 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|        - | 4585 | ` *  of the outer loop, use continue 2.` |
|        - | 4586 | ` *  Note that switch/case does loose comparision.` |
|        - | 4587 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|        - | 4588 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|        - | 4589 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|        - | 4590 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|        - | 4591 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|        - | 4592 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|        - | 4593 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|        - | 4594 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|        - | 4595 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|        - | 4596 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|        - | 4597 | ` *  list for the next case.` |
|        - | 4598 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|        - | 4599 | ` *  or floating-point numbers and strings.` |
|        - | 4600 | ` */` |
|       82 | 4601 | `PH7_PRIVATE sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|        5 | 4602 | `{` |
|        - | 4603 | `	GenBlock *pSwitchBlock;` |
|        - | 4604 | `	SyToken *pTmp,*pEnd;` |
|        - | 4605 | `	ph7_switch *pSwitch;` |
|        - | 4606 | `	sxu32 nToken;` |
|        - | 4607 | `	sxu32 nLine;` |
|        - | 4608 | `	sxi32 rc;` |
|       87 | 4609 | `	nLine = pGen->pIn->nLine;` |
|        - | 4610 | `	/* Jump the 'switch' keyword */` |
|       87 | 4611 | `	pGen->pIn++;` |
|       87 | 4612 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 4613 | `		/* Syntax error */` |
|      ! 0 | 4614 | `		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Expected '(' after 'switch' keyword");` |
|      ! 0 | 4615 | `		if( rc == SXERR_ABORT ){` |
|        - | 4616 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 4617 | `			return SXERR_ABORT;` |
|        - | 4618 | `		}` |
|      ! 0 | 4619 | `		goto Synchronize;` |
|        - | 4620 | `	}` |
|        - | 4621 | `	/* Jump the left parenthesis '(' */` |
|       87 | 4622 | `	pGen->pIn++;` |
|       87 | 4623 | `	pEnd = 0; /* cc warning */` |
|        - | 4624 | `	/* Create the loop block */` |
|      128 | 4625 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|       41 | 4626 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|       87 | 4627 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 4628 | `		return SXERR_ABORT;` |
|        - | 4629 | `	}` |
|        - | 4630 | `	/* Delimit the condition */` |
|       87 | 4631 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|       87 | 4632 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - | 4633 | `		/* Empty expression */` |
|      ! 0 | 4634 | `		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Expected expression after 'switch' keyword");` |
|      ! 0 | 4635 | `		if( rc == SXERR_ABORT ){` |
|        - | 4636 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 4637 | `			return SXERR_ABORT;` |
|        - | 4638 | `		}` |
|      ! 0 | 4639 | `	}` |
|        - | 4640 | `	/* Swap token streams */` |
|       87 | 4641 | `	pTmp = pGen->pEnd;` |
|       87 | 4642 | `	pGen->pEnd = pEnd;` |
|        - | 4643 | `	/* Compile the expression */` |
|       87 | 4644 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       87 | 4645 | `	if( rc == SXERR_ABORT ){` |
|        - | 4646 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 4647 | `		return SXERR_ABORT;` |
|        - | 4648 | `	}` |
|        - | 4649 | `	/* Update token stream */` |
|       87 | 4650 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 | 4651 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 4652 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 4653 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4654 | `			return SXERR_ABORT;` |
|        - | 4655 | `		}` |
|      ! 0 | 4656 | `		pGen->pIn++;` |
|      ! 0 | 4657 | `	}` |
|       87 | 4658 | `	pGen->pIn  = &pEnd[1];` |
|       87 | 4659 | `	pGen->pEnd = pTmp;` |
|       87 | 4660 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|       82 | 4661 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|      ! 0 | 4662 | `			pTmp = pGen->pIn;` |
|      ! 0 | 4663 | `			if( pTmp >= pGen->pEnd ){` |
|      ! 0 | 4664 | `				pTmp--;` |
|      ! 0 | 4665 | `			}` |
|        - | 4666 | `			/* Unexpected token */` |
|      ! 0 | 4667 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|      ! 0 | 4668 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 4669 | `				return SXERR_ABORT;` |
|        - | 4670 | `			}` |
|      ! 0 | 4671 | `			goto Synchronize;` |
|        - | 4672 | `	}` |
|        - | 4673 | `	/* Set the delimiter token */` |
|       87 | 4674 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|        7 | 4675 | `		nToken = PH7_TK_KEYWORD;` |
|        - | 4676 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|        4 | 4677 | `	}else{` |
|       81 | 4678 | `		nToken = PH7_TK_CCB; /* '}' */` |
|        - | 4679 | `	}` |
|       87 | 4680 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|        - | 4681 | `	/* Create the switch blocks container */` |
|       87 | 4682 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|       87 | 4683 | `	if( pSwitch == 0 ){` |
|        - | 4684 | `		/* Abort compilation */` |
|      ! 0 | 4685 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 4686 | `		return SXERR_ABORT;` |
|        - | 4687 | `	}` |
|        - | 4688 | `	/* Zero the structure */` |
|       87 | 4689 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|        - | 4690 | `	/* Initialize fields */` |
|       87 | 4691 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|        - | 4692 | `	/* Emit the switch instruction */` |
|       87 | 4693 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|        - | 4694 | `	/* Compile case blocks */` |
|      209 | 4695 | `	for(;;){` |
|        - | 4696 | `		sxu32 nKwrd;` |
|      255 | 4697 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 4698 | `			/* No more input to process */` |
|      ! 0 | 4699 | `			break;` |
|        - | 4700 | `		}` |
|      255 | 4701 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 4702 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|        - | 4703 | `				/* Unexpected token */` |
|      ! 0 | 4704 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 4705 | `					&pGen->pIn->sData);` |
|      ! 0 | 4706 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 4707 | `					return SXERR_ABORT;` |
|        - | 4708 | `				}` |
|        - | 4709 | `				/* FALL THROUGH */` |
|      ! 0 | 4710 | `			}` |
|        - | 4711 | `			/* Block compiled */` |
|      ! 0 | 4712 | `			break;` |
|        - | 4713 | `		}` |
|        - | 4714 | `		/* Extract the keyword */` |
|      255 | 4715 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      255 | 4716 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|        7 | 4717 | `			if( nToken != PH7_TK_KEYWORD ){` |
|        - | 4718 | `				/* Unexpected token */` |
|      ! 0 | 4719 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 4720 | `					&pGen->pIn->sData);` |
|      ! 0 | 4721 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 4722 | `					return SXERR_ABORT;` |
|        - | 4723 | `				}` |
|        - | 4724 | `				/* FALL THROUGH */` |
|      ! 0 | 4725 | `			}` |
|        - | 4726 | `			/* Block compiled */` |
|        7 | 4727 | `			break;` |
|        - | 4728 | `		}` |
|      249 | 4729 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|        - | 4730 | `			/*` |
|        - | 4731 | `			 * Accroding to the PHP language reference manual` |
|        - | 4732 | `			 *  A special case is the default case. This case matches anything` |
|        - | 4733 | `			 *  that wasn't matched by the other cases.` |
|        - | 4734 | `			 */` |
|       55 | 4735 | `			if( pSwitch->nDefault > 0 ){` |
|        - | 4736 | `				/* Default case already compiled */` |
|      ! 0 | 4737 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|      ! 0 | 4738 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 4739 | `					return SXERR_ABORT;` |
|        - | 4740 | `				}` |
|      ! 0 | 4741 | `			}` |
|       55 | 4742 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|        - | 4743 | `			/* Compile the default block */` |
|       55 | 4744 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|       55 | 4745 | `			if( rc == SXERR_ABORT){` |
|      ! 0 | 4746 | `				return SXERR_ABORT;` |
|       55 | 4747 | `			}else if( rc == SXERR_EOF ){` |
|       53 | 4748 | `				break;` |
|        1 | 4749 | `			}` |
|      200 | 4750 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|        - | 4751 | `			ph7_case_expr sCase;` |
|        - | 4752 | `			/* Standard case block */` |
|      199 | 4753 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|        - | 4754 | `			/* initialize the structure */` |
|      199 | 4755 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - | 4756 | `			/* Compile the case expression */` |
|      199 | 4757 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      199 | 4758 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 4759 | `				return SXERR_ABORT;` |
|        - | 4760 | `			}` |
|        - | 4761 | `			/* Compile the case block */` |
|      199 | 4762 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|        - | 4763 | `			/* Insert in the switch container */` |
|      199 | 4764 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      199 | 4765 | `			if( rc == SXERR_ABORT){` |
|      ! 0 | 4766 | `				return SXERR_ABORT;` |
|      199 | 4767 | `			}else if( rc == SXERR_EOF ){` |
|       32 | 4768 | `				break;` |
|        - | 4769 | `			}` |
|       88 | 4770 | `		}else{` |
|        - | 4771 | `			/* Unexpected token */` |
|      ! 0 | 4772 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 4773 | `				&pGen->pIn->sData);` |
|      ! 0 | 4774 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 4775 | `				return SXERR_ABORT;` |
|        - | 4776 | `			}` |
|      ! 0 | 4777 | `			break;` |
|        - | 4778 | `		}` |
|        5 | 4779 | `	}` |
|        - | 4780 | `	/* Fix all jumps now the destination is resolved */` |
|       87 | 4781 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|       87 | 4782 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 4783 | `	/* Release the loop block */` |
|       87 | 4784 | `	GenStateLeaveBlock(pGen,0);` |
|       87 | 4785 | `	if( pGen->pIn < pGen->pEnd ){` |
|        - | 4786 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|       87 | 4787 | `		pGen->pIn++;` |
|       41 | 4788 | `	}` |
|        - | 4789 | `	/* Statement successfully compiled */` |
|       87 | 4790 | `	return SXRET_OK;` |
|      ! 0 | 4791 | `Synchronize:` |
|        - | 4792 | `	/* Synchronize with the first semi-colon */` |
|      ! 0 | 4793 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      ! 0 | 4794 | `		pGen->pIn++;` |
|      ! 0 | 4795 | `	}` |
|      ! 0 | 4796 | `	return SXRET_OK;` |
|       46 | 4797 | `}` |
|        - | 4798 |  |
