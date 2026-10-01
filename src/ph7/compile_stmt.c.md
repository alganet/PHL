# src/ph7/compile_stmt.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2038/2550 lines (79.92%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h"` |
|       - |    7 | `#include "compile_int.h"` |
|       - |    8 | `/*` |
|       - |    9 | ` * Section:` |
|       - |   10 | ` *    Statement compilation: const, continue/break, labels and goto, blocks,` |
|       - |   11 | ` *    loops (while/do/for/foreach), if, global, return, yield, halt, echo,` |
|       - |   12 | ` *    static, var, namespace/use/declare, throw, try/catch/finally, switch.` |
|       - |   13 | ` * Status:` |
|       - |   14 | ` *    Stable.` |
|       - |   15 | ` */` |
|       - |   16 | `/*` |
|       - |   17 | ` * Compile the 'const' statement.` |
|       - |   18 | ` * According to the PHP language reference` |
|       - |   19 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|       - |   20 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|       - |   21 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|       - |   22 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|       - |   23 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |   24 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|       - |   25 | ` *  Syntax` |
|       - |   26 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|       - |   27 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|       - |   28 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|       - |   29 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|       - |   30 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|       - |   31 | ` *  to get a list of all defined constants.` |
|       - |   32 | ` *` |
|       - |   33 | ` * Symisc eXtension.` |
|       - |   34 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|       - |   35 | ` *  would allow only simple scalar value.` |
|       - |   36 | ` *  Example` |
|       - |   37 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |   38 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |   39 | ` */` |
|       - |   40 | `/*` |
|       - |   41 | ` * The keyword-shaped names php still accepts as a GLOBAL constant name: its lexer` |
|       - |   42 | `` * does not reserve the type words or the scope words, so `const int = 1;` and`` |
|       - |   43 | `` * `const self = 1;` parse there while every other keyword is a syntax error. Kept`` |
|       - |   44 | ` * as a list rather than a token-class test because the two engines' keyword TABLES` |
|       - |   45 | ` * are not the same set — PHL lexes some words php leaves as plain identifiers.` |
|       - |   46 | ` */` |
|      20 |   47 | `static int GenStateConstNameKeywordOk(SyString *pName)` |
|       2 |   48 | `{` |
|       - |   49 | ``	/* `integer` and `boolean` lex to the same tokens as `int` and `bool` here,`` |
|       - |   50 | `	 * but php has no reserved word under either spelling and takes both as a` |
|       - |   51 | `	 * constant name. */` |
|       - |   52 | `	static const char *azOk[] = { "bool", "boolean", "float", "int", "integer",` |
|       - |   53 | `	                              "object", "string", "self", "parent" };` |
|       - |   54 | `	sxu32 i;` |
|     112 |   55 | `	for( i = 0 ; i < SX_ARRAYSIZE(azOk) ; ++i ){` |
|     110 |   56 | `		sxu32 n = (sxu32)SyStrlen(azOk[i]);` |
|     110 |   57 | `		if( pName->nByte == n && SyStrnicmp(pName->zString,azOk[i],n) == 0 ){` |
|      19 |   58 | `			return 1;` |
|       - |   59 | `		}` |
|      47 |   60 | `	}` |
|       3 |   61 | `	return 0;` |
|      12 |   62 | `}` |
|     200 |   63 | `PH7_PRIVATE sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       5 |   64 | `{` |
|       - |   65 | `	SySet *pConsCode,*pInstrContainer;` |
|       - |   66 | `	sxu32 nLineLocal;` |
|       - |   67 | `	SyString *pName;` |
|       - |   68 | `	sxi32 rc;` |
|       - |   69 | `	/* php forbids attributes on a comma-separated const list. Snapshot whether the` |
|       - |   70 | `	 * statement carries any now, before the first constant consumes them. */` |
|     205 |   71 | `	int bHadAttrs = SySetUsed(&pGen->aPendingAttrs) > 0;` |
|       - |   72 | ``	/* php attributes every const-statement compile error to the `const` keyword's`` |
|       - |   73 | ``	 * line, not the offending list element's own line (`const A=1,\nB=strlen()` blames`` |
|       - |   74 | `	 * line 1). Capture it once here, before jumping the keyword. */` |
|     205 |   75 | `	nLineLocal = pGen->pIn->nLine;` |
|     205 |   76 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       - |   77 | ``	/* php allows a single `const` statement to declare several constants at once`` |
|       - |   78 | ``	 * (`const A = 1, B = 2;`). Loop over the comma-separated name = value pairs;`` |
|       - |   79 | `	 * PH7_CompileExpr(EXPR_FLAG_COMMA_STATEMENT) stops each value at the first` |
|       - |   80 | `	 * top-level comma so the next pair starts cleanly. */` |
|     105 |   81 | `Loop:` |
|     215 |   82 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |   83 | `		/* Invalid constant name */` |
|       9 |   84 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"identifier");` |
|       9 |   85 | `		if( rc == SXERR_ABORT ){` |
|       - |   86 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |   87 | `			return SXERR_ABORT;` |
|       - |   88 | `		}` |
|       9 |   89 | `		goto Synchronize;` |
|       - |   90 | `	}` |
|       - |   91 | `	/* Peek constant name */` |
|     209 |   92 | `	pName = &pGen->pIn->sData;` |
|       - |   93 | ``	/* php's global `const` takes an IDENTIFIER: a reserved word is a parse error`` |
|       - |   94 | `` 	 * there, and only a CLASS constant may carry one (`class C { const list = 5; }` `` |
|       - |   95 | ``	 * is php-legal, `const list = 5;` at file scope is not). PHL accepted both, so`` |
|       - |   96 | ``	 * `const LIST = 1;` compiled and READ back — source php refuses to parse.`` |
|       - |   97 | `	 * The words php still allows are the ones its lexer does not reserve: the type` |
|       - |   98 | ``	 * names and the scope words. The word OPERATORS (`and`, `or`, `xor`, `new`,`` |
|       - |   99 | ``	 * `clone`, `instanceof`) are reserved too — the lexer types those ID\|OP rather`` |
|       - |  100 | `	 * than KEYWORD, which is why the test reads both bits. */` |
|     204 |  101 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_OP))` |
|     117 |  102 | `	 && !GenStateConstNameKeywordOk(pName) ){` |
|       3 |  103 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn,"identifier");` |
|       3 |  104 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  105 | `			return SXERR_ABORT;` |
|       - |  106 | `		}` |
|       3 |  107 | `		goto Synchronize;` |
|       - |  108 | `	}` |
|       - |  109 | `	/* Make sure the constant name isn't reserved */` |
|     207 |  110 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  111 | `		/* Reserved constant */` |
|       9 |  112 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Cannot redeclare constant '%z'",pName);` |
|       9 |  113 | `		if( rc == SXERR_ABORT ){` |
|       - |  114 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  115 | `			return SXERR_ABORT;` |
|       - |  116 | `		}` |
|       9 |  117 | `		goto Synchronize;` |
|       - |  118 | `	}` |
|     199 |  119 | `	pGen->pIn++;` |
|     199 |  120 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  121 | `		/* Invalid statement*/` |
|       6 |  122 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"=\"");` |
|       6 |  123 | `		if( rc == SXERR_ABORT ){` |
|       - |  124 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  125 | `			return SXERR_ABORT;` |
|       - |  126 | `		}` |
|       6 |  127 | `		goto Synchronize;` |
|       - |  128 | `	}` |
|     195 |  129 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - |  130 | `	/* php's constant-expression rules, first offender wins (see` |
|       - |  131 | ``	 * PH7_GenStateConstExprError). A global `const` DOES take `new` (PHP 8.1's`` |
|       - |  132 | ``	 * "new in initializers"), so the `new` rule is off here. */`` |
|       - |  133 | `	{` |
|     195 |  134 | `		const char *zCErr = PH7_GenStateConstExprError(pGen,1);` |
|     195 |  135 | `		if( zCErr ){` |
|      11 |  136 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"%s",zCErr);` |
|      11 |  137 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  138 | `				return SXERR_ABORT;` |
|       - |  139 | `			}` |
|      11 |  140 | `			goto Synchronize;` |
|       - |  141 | `		}` |
|       - |  142 | `	}` |
|       - |  143 | `	/* Allocate a new constant value container */` |
|     187 |  144 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|     187 |  145 | `	if( pConsCode == 0 ){` |
|     ! 0 |  146 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  147 | `		return SXERR_ABORT;` |
|       - |  148 | `	}` |
|     187 |  149 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  150 | `	/* Swap bytecode container */` |
|     187 |  151 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     187 |  152 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - |  153 | ``	/* Compile constant value. php: a stray token after `const X = EXPR` is`` |
|       - |  154 | ``	 * `... expecting "," or ";"` (const supports a comma-separated list).`` |
|       - |  155 | `	 * EXPR_FLAG_COMMA_STATEMENT stops this value at the first top-level comma so a` |
|       - |  156 | `	 * following declaration is left for the loop below. */` |
|       - |  157 | `	{` |
|     187 |  158 | `		const char *zSaveConst = pGen->zClauseCloser;` |
|     187 |  159 | `		pGen->zClauseCloser = "\",\" or \";\"";` |
|     187 |  160 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|     187 |  161 | `		pGen->zClauseCloser = zSaveConst;` |
|       - |  162 | `	}` |
|       - |  163 | `	/* Emit the done instruction */` |
|     187 |  164 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     187 |  165 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     187 |  166 | `	if( rc == SXERR_ABORT ){` |
|       - |  167 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  168 | `		return SXERR_ABORT;` |
|       - |  169 | `	}` |
|       - |  170 | ``	/* php parse-errors an empty initializer (`const A = ;`, or a `,B=` list hole);`` |
|       - |  171 | `	 * the class-const path rejects it too. Reject loudly rather than silently` |
|       - |  172 | `	 * defining a NULL constant. (php's error KIND -- a parse error -- differs from` |
|       - |  173 | `	 * PHL's compile fatal, the recursive-descent-vs-bison family; both refuse.) */` |
|     187 |  174 | `	if( rc == SXERR_EMPTY && PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       2 |  175 | `			"Empty constant '%z' value",pName) == SXERR_ABORT ){` |
|     ! 0 |  176 | `		return SXERR_ABORT;` |
|       - |  177 | `	}` |
|     187 |  178 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - |  179 | `	/* Register the constant with namespace-qualified name */` |
|       - |  180 | `	{` |
|       - |  181 | `		SyBlob sFQN;` |
|       - |  182 | `		SyString sFQNStr;` |
|     187 |  183 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     187 |  184 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|     187 |  185 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|       - |  186 | ``		/* php refuses a `const` whose name a local `use const` already took. */`` |
|     187 |  187 | `		if( GenStateGuardImportRedeclare(pGen,2,pName,&sFQNStr,nLineLocal) == SXERR_ABORT ){` |
|     ! 0 |  188 | `			SyBlobRelease(&sFQN);` |
|     ! 0 |  189 | `			return SXERR_ABORT;` |
|       - |  190 | `		}` |
|     278 |  191 | `		rc = PH7_VmRegisterConstantEx(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode,` |
|     182 |  192 | `			(SyString *)SySetPeek(&pGen->pVm->aFiles),nLineLocal,1);` |
|     187 |  193 | `		if( rc == SXRET_OK && SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|       - |  194 | ``			/* php 8.5: attributes on `const` statements — attach the pending`` |
|       - |  195 | `			 * groups to the registered constant record for Reflection. */` |
|      15 |  196 | `			SyHashEntry *pCEntry = SyHashGet(&pGen->pVm->hConstant,` |
|       8 |  197 | `				SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      11 |  198 | `			if( pCEntry ){` |
|      11 |  199 | `				ph7_constant *pRegCons = (ph7_constant *)pCEntry->pUserData;` |
|      11 |  200 | `				if( GenStateConsumeAttrs(&(*pGen),&pRegCons->aAttrs) == SXERR_ABORT ){` |
|     ! 0 |  201 | `					SyBlobRelease(&sFQN);` |
|     ! 0 |  202 | `					return SXERR_ABORT;` |
|       - |  203 | `				}` |
|       8 |  204 | `				if( GenStateCheckAttrPlacement(&(*pGen),&pRegCons->aAttrs,64,64,0,0)` |
|       7 |  205 | `					== SXERR_ABORT ){` |
|     ! 0 |  206 | `					SyBlobRelease(&sFQN);` |
|     ! 0 |  207 | `					return SXERR_ABORT;` |
|       - |  208 | `				}` |
|       4 |  209 | `			}` |
|       4 |  210 | `		}` |
|     187 |  211 | `		SyBlobRelease(&sFQN);` |
|       - |  212 | `	}` |
|     187 |  213 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  214 | `		SySetRelease(pConsCode);` |
|     ! 0 |  215 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 |  216 | `	}` |
|       - |  217 | ``	/* Another declaration in the same statement: `const A = 1, B = 2;`. */`` |
|     187 |  218 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /* ',' */) ){` |
|      14 |  219 | `		if( bHadAttrs ){` |
|       - |  220 | ``			/* php compile-fatals `#[Attr] const A = 1, B = 2;` outright. */`` |
|       3 |  221 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - |  222 | `				"Cannot apply attributes to multiple constants at once");` |
|       3 |  223 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  224 | `				return SXERR_ABORT;` |
|       - |  225 | `			}` |
|       3 |  226 | `			goto Synchronize;` |
|       - |  227 | `		}` |
|      12 |  228 | `		pGen->pIn++; /* Jump the comma */` |
|      12 |  229 | `		goto Loop;` |
|       - |  230 | `	}` |
|     175 |  231 | `	return SXRET_OK;` |
|      15 |  232 | `Synchronize:` |
|       - |  233 | `	/* Synchronize with the next top-level semicolon and avoid compiling this` |
|       - |  234 | ``	 * erroneous statement. BRACE-aware (only `{`...`}`): a rejected closure`` |
|       - |  235 | ``	 * initializer's body holds inner `;` that are not statement terminators, so`` |
|       - |  236 | ``	 * without this its `;` and `}` dangle into a spurious second error where php`` |
|       - |  237 | `	 * halts at the first fatal. Parentheses and brackets are deliberately NOT` |
|       - |  238 | ``	 * tracked -- a lone unbalanced `(`/`[` in erroneous input must not swallow the`` |
|       - |  239 | ``	 * following statements (which would drop later error reports); a `{` never`` |
|       - |  240 | `	 * appears in a valid global-const initializer except as a closure body. */` |
|       - |  241 | `	{` |
|      34 |  242 | `		int iBrace = 0;` |
|     130 |  243 | `		while( pGen->pIn < pGen->pEnd ){` |
|     130 |  244 | `			if( iBrace == 0 && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      34 |  245 | `				break;` |
|       - |  246 | `			}` |
|     100 |  247 | `			if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       3 |  248 | `				iBrace++;` |
|      99 |  249 | `			}else if( pGen->pIn->nType & PH7_TK_CCB ){` |
|       3 |  250 | `				if( iBrace > 0 ){ iBrace--; }` |
|       1 |  251 | `			}` |
|     100 |  252 | `			pGen->pIn++;` |
|       4 |  253 | `		}` |
|       - |  254 | `	}` |
|      34 |  255 | `	return SXRET_OK;` |
|     105 |  256 | `}` |
|       - |  257 | `/*` |
|       - |  258 | ` * Compile the 'continue' statement.` |
|       - |  259 | ` * According to the PHP language reference` |
|       - |  260 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|       - |  261 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|       - |  262 | ` *  iteration.` |
|       - |  263 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|       - |  264 | ` *  the purposes of continue.` |
|       - |  265 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|       - |  266 | ` *  of enclosing loops it should skip to the end of.` |
|       - |  267 | ` *  Note:` |
|       - |  268 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|       - |  269 | ` */` |
|       - |  270 | `/*` |
|       - |  271 | `` * Pick the jump opcode for a `break`/`continue` targeting pLoop and fill in its iP1,`` |
|       - |  272 | ` * emitting the POP_EXCEPTIONs for the crossed trys this statement can resolve here and` |
|       - |  273 | `` * now. All the classification lives in GenStateJumpScope, which `goto` shares.`` |
|       - |  274 | ` */` |
|   47615 |  275 | `static sxi32 GenStateLoopJumpOp(ph7_gen_state *pGen,GenBlock *pLoop,sxi32 *piP1,` |
|       - |  276 | `	GenJumpScope *pCross)` |
|       5 |  277 | `{` |
|       - |  278 | `	/* The loop encloses the break by construction, so the walk always reaches it. Note the` |
|       - |  279 | `	 * walk EMITS the crossed trys' POP_EXCEPTIONs as it goes, before the caller can see` |
|       - |  280 | `	 * pCross->nFinally and reject: a statement about to be fatal therefore leaves a few` |
|       - |  281 | `	 * dead instructions behind. Harmless — the compile error stops the program from` |
|       - |  282 | `	 * running at all — and the alternative is walking the chain twice on every jump. */` |
|   47620 |  283 | `	GenStateJumpScope(&(*pGen),pGen->nCurScopeId,pLoop->nScopeId,TRUE,pCross);` |
|   47620 |  284 | `	return GenStateScopeJumpOp(pCross,piP1);` |
|       5 |  285 | `}` |
|       - |  286 | `/*` |
|       - |  287 | `` * php compile-rejects a `break`/`continue`/`goto` that leaves a `finally` body (a`` |
|       - |  288 | `` * `return` is fine). One wording, one place, for all three statements.`` |
|       - |  289 | ` */` |
|       - |  290 | `/* Whether the cursor has run past the LAST token of a chunk that met the end of` |
|       - |  291 | ` * the FILE -- the shared end-of-input question, asked here by the three statements` |
|       - |  292 | ` * that would otherwise report a complaint of their own first. */` |
|       2 |  293 | `static int GenStateAtChunkEofStmt(ph7_gen_state *pGen)` |
|     ! 0 |  294 | `{` |
|       - |  295 | `	SyToken *pBase;` |
|       2 |  296 | `	if( !pGen->bChunkAtEof \|\| pGen->pTokenSet == 0 ){` |
|     ! 0 |  297 | `		return 0;` |
|       - |  298 | `	}` |
|       2 |  299 | `	pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|       2 |  300 | `	return pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)];` |
|       1 |  301 | `}` |
|      10 |  302 | `PH7_PRIVATE sxi32 GenStateJumpOutOfFinally(ph7_gen_state *pGen,sxu32 nLine)` |
|       4 |  303 | `{` |
|      14 |  304 | `	return PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  305 | `		"jump out of a finally block is disallowed");` |
|       4 |  306 | `}` |
|       - |  307 | `/*` |
|       - |  308 | `` * php's `break`/`continue` grammar is `KEYWORD optional_expr ";"`, and the level is`` |
|       - |  309 | ` * then screened as a compile-time VALUE rather than parsed as a number: an operand` |
|       - |  310 | `` * that is not a literal at all -- a constant, a variable, `-1`, `(1+0)` -- is`` |
|       - |  311 | `` * `'break' operator with non-integer operand is no longer supported`, and one that`` |
|       - |  312 | `` * IS a literal but not a positive integer -- `1.5`, `"1"`, `0` -- is`` |
|       - |  313 | `` * `'break' operator accepts only positive integers`. Parentheses are transparent`` |
|       - |  314 | `` * (`((1))` is 1) because they leave no node of their own, which is also why the`` |
|       - |  315 | ` * arithmetic inside them is not folded away first.` |
|       - |  316 | ` *` |
|       - |  317 | `` * PHL used to read a NUMBER token and ignore anything else, so `break 1.5;` broke`` |
|       - |  318 | `` * one level in silence, `break $x;` and `break foo;` compiled to a plain break with`` |
|       - |  319 | ` * a WARNING about the missing semicolon, and the program then RAN.` |
|       - |  320 | ` *` |
|       - |  321 | ` * Answers SXRET_OK with *piLevel set (1 when there is no operand), SXERR_ABORT to` |
|       - |  322 | ` * abort the compile, or SXERR_SYNTAX once a refusal has been reported.` |
|       - |  323 | ` */` |
|   47683 |  324 | `static sxi32 GenStateJumpLevelArg(ph7_gen_state *pGen,const char *zWhich,sxu32 nLine,sxi32 *piLevel)` |
|       5 |  325 | `{` |
|   47688 |  326 | `	SyToken *pStart = pGen->pIn,*pEnd = pGen->pEnd,*pAfter;` |
|       - |  327 | `	sxi32 rc;` |
|   47688 |  328 | `	*piLevel = 1;` |
|       - |  329 | `	/* The statement slice runs to the end of the enclosing block, so the operand` |
|       - |  330 | `	 * stops at its own terminator. */` |
|   47818 |  331 | `	for( pAfter = pStart ; pAfter < pEnd ; pAfter++ ){` |
|   47816 |  332 | `		if( pAfter->nType & PH7_TK_SEMI ){` |
|   47686 |  333 | `			pEnd = pAfter;` |
|   47686 |  334 | `			break;` |
|       - |  335 | `		}` |
|      70 |  336 | `	}` |
|   47688 |  337 | `	if( pStart >= pEnd ){` |
|   47608 |  338 | `		return SXRET_OK; /* No operand at all: one level */` |
|       - |  339 | `	}` |
|      85 |  340 | `	pGen->pIn = pEnd; /* The whole operand belongs to this statement either way */` |
|       - |  341 | `	/* Peel parenthesis pairs that wrap the WHOLE operand. */` |
|      50 |  342 | `	for(;;){` |
|       - |  343 | `		SyToken *pTok;` |
|      95 |  344 | `		sxi32 nDepth = 0;` |
|      95 |  345 | `		if( !(pStart->nType & PH7_TK_LPAREN) \|\| !(pEnd[-1].nType & PH7_TK_RPAREN) ){` |
|      45 |  346 | `			break;` |
|       - |  347 | `		}` |
|      36 |  348 | `		for( pTok = pStart ; pTok < pEnd ; pTok++ ){` |
|      36 |  349 | `			if( pTok->nType & PH7_TK_LPAREN ){` |
|      12 |  350 | `				nDepth++;` |
|      30 |  351 | `			}else if( pTok->nType & PH7_TK_RPAREN ){` |
|      12 |  352 | `				nDepth--;` |
|      12 |  353 | `				if( nDepth == 0 ){` |
|      10 |  354 | `					break;` |
|       - |  355 | `				}` |
|       1 |  356 | `			}` |
|      13 |  357 | `		}` |
|      10 |  358 | `		if( pTok != pEnd - 1 ){` |
|     ! 0 |  359 | `			break; /* The opening paren closes before the end: not a wrap */` |
|       - |  360 | `		}` |
|      10 |  361 | `		pStart++;` |
|      10 |  362 | `		pEnd--;` |
|     ! 0 |  363 | `	}` |
|      85 |  364 | `	if( pStart >= pEnd ){` |
|       - |  365 | ``		/* The parentheses held nothing (`break ();`): php's expression parser has`` |
|       - |  366 | `		 * no operand to read and names the closing one, which is where the peel` |
|       - |  367 | `		 * above left the cursor. */` |
|       2 |  368 | `		rc = PH7_GenSyntaxError(&(*pGen),pStart,0);` |
|       2 |  369 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  370 | `	}` |
|      78 |  371 | `	if( (pStart->nType & (PH7_TK_INTEGER\|PH7_TK_REAL\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_HEREDOC` |
|      44 |  372 | `	                     \|PH7_TK_NOWDOC\|PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|       - |  373 | `		/* An operator leads the operand, so it cannot be a literal node. */` |
|       4 |  374 | `		goto NonInteger;` |
|       - |  375 | `	}` |
|       - |  376 | ``	/* The primary this operand starts with -- `$name` is two tokens here. */`` |
|      79 |  377 | `	pAfter = &pStart[(pStart->nType & PH7_TK_DOLLAR) ? 2 : 1];` |
|      79 |  378 | `	if( pAfter < pEnd ){` |
|      18 |  379 | `		if( (pAfter->nType & (PH7_TK_OP\|PH7_TK_OSB\|PH7_TK_LPAREN))` |
|      14 |  380 | `		 && (pAfter->nType & PH7_TK_COMMA) == 0 ){` |
|       - |  381 | `			/* The primary continues into a larger expression php will not take.` |
|       - |  382 | `			 * A comma is typed as an operator here and is not one to php: the` |
|       - |  383 | `			 * expression has ENDED there, so the semicolon is what it wants. */` |
|       4 |  384 | `			goto NonInteger;` |
|       - |  385 | `		}` |
|       - |  386 | `		/* php read its expression and now wants the semicolon. */` |
|      16 |  387 | `		rc = PH7_GenSyntaxError(&(*pGen),pAfter,"\";\"");` |
|      16 |  388 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  389 | `	}` |
|      61 |  390 | `	if( pStart->nType & PH7_TK_INTEGER ){` |
|       - |  391 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      49 |  392 | `		char *zAlloc = 0;` |
|       - |  393 | `		SyString sNum;` |
|      71 |  394 | `		if( SXRET_OK != GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      44 |  395 | `				&pStart->sData,zScratch,sizeof(zScratch),&sNum,&zAlloc) ){` |
|     ! 0 |  396 | `			return SXERR_ABORT;` |
|       - |  397 | `		}` |
|      49 |  398 | `		*piLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      49 |  399 | `		if( zAlloc ){` |
|     ! 0 |  400 | `			SyMemBackendFree(&pGen->pVm->sAllocator,zAlloc);` |
|     ! 0 |  401 | `		}` |
|      49 |  402 | `		if( *piLevel < 1 ){` |
|       3 |  403 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  404 | `				"'%s' operator accepts only positive integers",zWhich);` |
|       2 |  405 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  406 | `		}` |
|      47 |  407 | `		return SXRET_OK;` |
|       - |  408 | `	}` |
|      12 |  409 | `	if( pStart->nType & (PH7_TK_REAL\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){` |
|       9 |  410 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       3 |  411 | `			"'%s' operator accepts only positive integers",zWhich);` |
|       6 |  412 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  413 | `	}` |
|       3 |  414 | `NonInteger:` |
|      21 |  415 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       7 |  416 | `		"'%s' operator with non-integer operand is no longer supported",zWhich);` |
|      14 |  417 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|   23815 |  418 | `}` |
|   33741 |  419 | `PH7_PRIVATE sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       5 |  420 | `{` |
|       - |  421 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  422 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  423 | `	sxi32 iRawLevel; /* The level as WRITTEN, kept for php's diagnostics */` |
|       - |  424 | `	sxu32 nLineLocal;` |
|       - |  425 | `	sxi32 rc;` |
|   33746 |  426 | `	iRawLevel = 1;` |
|   33746 |  427 | `	nLineLocal = pGen->pIn->nLine;` |
|   33746 |  428 | `	iLevel = 0;` |
|       - |  429 | `	/* Jump the 'continue' keyword */` |
|   33746 |  430 | `	pGen->pIn++;` |
|   33746 |  431 | `	rc = GenStateJumpLevelArg(&(*pGen),"continue",nLineLocal,&iRawLevel);` |
|   33746 |  432 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  433 | `		return SXERR_ABORT;` |
|   33746 |  434 | `	}else if( rc != SXRET_OK ){` |
|       9 |  435 | `		return SXRET_OK; /* Refused and reported */` |
|       - |  436 | `	}` |
|   33738 |  437 | `	if( pGen->pIn >= pGen->pEnd && GenStateAtChunkEofStmt(pGen) ){` |
|       - |  438 | `		/* php's parser wants the semicolon before it ever asks where the jump` |
|       - |  439 | ``		 * lands, so a `continue` that ends the FILE is its parse error and not a`` |
|       - |  440 | `		 * sentence about the loop this one is not in. */` |
|     ! 0 |  441 | `		rc = PH7_GenSyntaxError(&(*pGen),0,"\";\"");` |
|     ! 0 |  442 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  443 | `	}` |
|   33738 |  444 | `	iLevel = iRawLevel < 2 ? 0 : iRawLevel;` |
|       - |  445 | `	/* Point to the target loop */` |
|   33738 |  446 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|   33738 |  447 | `	if( pLoop == 0 ){` |
|       - |  448 | ``		/* Same split php makes for `break`: no loop at all keeps the not-in-context`` |
|       - |  449 | ``		 * wording, too FEW loops is `Cannot 'continue' N levels`. */`` |
|      11 |  450 | `		if( GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,0) != 0 ){` |
|     ! 0 |  451 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|     ! 0 |  452 | `				"Cannot 'continue' %d level%s",iRawLevel,iRawLevel == 1 ? "" : "s");` |
|     ! 0 |  453 | `		}else{` |
|      11 |  454 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"'continue' not in the 'loop' or 'switch' context");` |
|       - |  455 | `		}` |
|      11 |  456 | `		if( rc == SXERR_ABORT ){` |
|       - |  457 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  458 | `			return SXERR_ABORT;` |
|       - |  459 | `		}` |
|       6 |  460 | `	}else{` |
|   33728 |  461 | `		sxu32 nInstrIdx = 0;` |
|   33728 |  462 | `		sxi32 iP1 = 0;` |
|       - |  463 | `		GenJumpScope sCross;` |
|   33728 |  464 | `		sxi32 iJmpOp = GenStateLoopJumpOp(&(*pGen),pLoop,&iP1,&sCross);` |
|   33728 |  465 | `		if( sCross.nFinally > 0 ){` |
|       3 |  466 | `			if( GenStateJumpOutOfFinally(&(*pGen),nLineLocal) == SXERR_ABORT ){` |
|     ! 0 |  467 | `				return SXERR_ABORT;` |
|       1 |  468 | `			}` |
|   33727 |  469 | `		}else if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|       - |  470 | ``			/* `continue` inside a switch acts like `break` — which is almost never what`` |
|       - |  471 | `			 * the author meant, so php 7.3+ says so at compile time. The generated jump` |
|       - |  472 | `			 * is unchanged; only the diagnostic was missing. An explicit level` |
|       - |  473 | ``			 * (`continue 2`) targets the enclosing loop and stays silent. */`` |
|       5 |  474 | `			if( iLevel < 1 ){` |
|       5 |  475 | `				PH7_GenCompileError(&(*pGen),E_WARNING,nLineLocal,` |
|       - |  476 | `					"\"continue\" targeting switch is equivalent to \"break\"."` |
|       - |  477 | `					" Did you mean to use \"continue 2\"?");` |
|       2 |  478 | `			}` |
|       5 |  479 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,iP1,0,0,&nInstrIdx);` |
|       5 |  480 | `			if( rc == SXRET_OK ){` |
|       5 |  481 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|       2 |  482 | `			}` |
|       3 |  483 | `		}else{` |
|       - |  484 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|   33722 |  485 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,iP1,pLoop->nFirstInstr,0,&nInstrIdx);` |
|   33722 |  486 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - |  487 | `				JumpFixup sJumpFix;` |
|       - |  488 | `				/* Post-continue */` |
|   13469 |  489 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|   13469 |  490 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|   13469 |  491 | `				sJumpFix.pContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   13469 |  492 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|    6723 |  493 | `			}` |
|       - |  494 | `		}` |
|       - |  495 | `	}` |
|   33733 |  496 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0` |
|   16849 |  497 | `	 && PH7_GenSyntaxError(&(*pGen),pGen->pIn,"\";\"") == SXERR_ABORT ){` |
|     ! 0 |  498 | `		return SXERR_ABORT;` |
|       - |  499 | `	}` |
|       - |  500 | `	/* Statement successfully compiled */` |
|   33738 |  501 | `	return SXRET_OK;` |
|   16853 |  502 | `}` |
|       - |  503 | `/*` |
|       - |  504 | ` * Compile the 'break' statement.` |
|       - |  505 | ` * According to the PHP language reference` |
|       - |  506 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - |  507 | ` *  structure.` |
|       - |  508 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - |  509 | ` *  enclosing structures are to be broken out of.` |
|       - |  510 | ` */` |
|   13942 |  511 | `PH7_PRIVATE sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       5 |  512 | `{` |
|       - |  513 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  514 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  515 | `	sxi32 iRawLevel; /* The level as WRITTEN, kept for php's diagnostics */` |
|       - |  516 | `	sxu32 nLineLocal;` |
|       - |  517 | `	sxi32 rc;` |
|   13947 |  518 | `	iLevel = 0;` |
|   13947 |  519 | `	iRawLevel = 1;` |
|   13947 |  520 | `	nLineLocal = pGen->pIn->nLine;` |
|       - |  521 | `	/* Jump the 'break' keyword */` |
|   13947 |  522 | `	pGen->pIn++;` |
|   13947 |  523 | `	rc = GenStateJumpLevelArg(&(*pGen),"break",nLineLocal,&iRawLevel);` |
|   13947 |  524 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  525 | `		return SXERR_ABORT;` |
|   13947 |  526 | `	}else if( rc != SXRET_OK ){` |
|      31 |  527 | `		return SXRET_OK; /* Refused and reported */` |
|       - |  528 | `	}` |
|   13917 |  529 | `	if( pGen->pIn >= pGen->pEnd && GenStateAtChunkEofStmt(pGen) ){` |
|       - |  530 | ``		/* As for `continue` above: the missing semicolon is what php's parser meets`` |
|       - |  531 | `		 * first, before it can ask which loop this breaks out of. */` |
|       2 |  532 | `		rc = PH7_GenSyntaxError(&(*pGen),0,"\";\"");` |
|       2 |  533 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  534 | `	}` |
|   13915 |  535 | `	iLevel = iRawLevel < 2 ? 0 : iRawLevel;` |
|       - |  536 | `	/* Extract the target loop */` |
|   13915 |  537 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|   13915 |  538 | `	if( pLoop == 0 ){` |
|       - |  539 | `		/* php distinguishes "there is no loop at all here" from "there is one, but` |
|       - |  540 | `		 * not N of them": the first keeps the not-in-context wording, the second is` |
|       - |  541 | ``		 * `Cannot 'break' N levels`. */`` |
|      21 |  542 | `		if( GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,0) != 0 ){` |
|       5 |  543 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       4 |  544 | `				"Cannot 'break' %d level%s",iRawLevel,iRawLevel == 1 ? "" : "s");` |
|       3 |  545 | `		}else{` |
|      16 |  546 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"'break' not in the 'loop' or 'switch' context");` |
|       - |  547 | `		}` |
|      21 |  548 | `		if( rc == SXERR_ABORT ){` |
|       - |  549 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  550 | `			return SXERR_ABORT;` |
|       - |  551 | `		}` |
|      12 |  552 | `	}else{` |
|       - |  553 | `		sxu32 nInstrIdx;` |
|   13897 |  554 | `		sxi32 iP1 = 0;` |
|       - |  555 | `		GenJumpScope sCross;` |
|   13897 |  556 | `		sxi32 iJmpOp = GenStateLoopJumpOp(&(*pGen),pLoop,&iP1,&sCross);` |
|   13897 |  557 | `		if( sCross.nFinally > 0 ){` |
|       9 |  558 | `			if( GenStateJumpOutOfFinally(&(*pGen),nLineLocal) == SXERR_ABORT ){` |
|     ! 0 |  559 | `				return SXERR_ABORT;` |
|       - |  560 | `			}` |
|       6 |  561 | `		}else{` |
|   13891 |  562 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,iP1,0,0,&nInstrIdx);` |
|   13891 |  563 | `			if( rc == SXRET_OK ){` |
|       - |  564 | `				/* Fix the jump later when the jump destination is resolved */` |
|   13891 |  565 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|    6934 |  566 | `			}` |
|       - |  567 | `		}` |
|       - |  568 | `	}` |
|   13910 |  569 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0` |
|    6951 |  570 | `	 && PH7_GenSyntaxError(&(*pGen),pGen->pIn,"\";\"") == SXERR_ABORT ){` |
|     ! 0 |  571 | `		return SXERR_ABORT;` |
|       - |  572 | `	}` |
|       - |  573 | `	/* Statement successfully compiled */` |
|   13915 |  574 | `	return SXRET_OK;` |
|    6967 |  575 | `}` |
|       - |  576 | `/*` |
|       - |  577 | ` * The function body a goto or a label sits in, or NULL at file scope. A goto may not` |
|       - |  578 | ` * cross functions, so GenStateFixGoto pairs the two on this.` |
|       - |  579 | ` */` |
|     446 |  580 | `static ph7_vm_func * GenStateOwningFunc(ph7_gen_state *pGen)` |
|       5 |  581 | `{` |
|     451 |  582 | `	GenBlock *pBlock = pGen->pCurrent;` |
|    1137 |  583 | `	while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|     691 |  584 | `		pBlock = pBlock->pParent;` |
|       5 |  585 | `	}` |
|     451 |  586 | `	return pBlock ? (ph7_vm_func *)pBlock->pUserData : 0;` |
|       5 |  587 | `}` |
|       - |  588 | `/*` |
|       - |  589 | ` * Compile or record a label.` |
|       - |  590 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|       - |  591 | ` * Example` |
|       - |  592 | ` *  goto LABEL;` |
|       - |  593 | ` *   echo 'Foo';` |
|       - |  594 | ` *  LABEL:` |
|       - |  595 | ` *   echo 'Bar';` |
|       - |  596 | ` */` |
|     220 |  597 | `PH7_PRIVATE sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|       5 |  598 | `{` |
|       - |  599 | `	Label sLabel;` |
|       - |  600 | `	/* php places almost NO restriction on where a label may be DEFINED — inside a loop, a` |
|       - |  601 | `	 * switch or a try{} is all fine; the one rule is that a name may not be declared twice` |
|       - |  602 | `	 * in the same function (below). The rest is on the jump: you may not goto INTO a loop` |
|       - |  603 | `	 * or switch from outside it, which is checked once the labels are all known (see` |
|       - |  604 | `	 * GenStateFixJumps). Record the loop this label sits in so that check can run. */` |
|       - |  605 | `	{` |
|     225 |  606 | `		SyString *pTarget = &pGen->pIn->sData;` |
|     225 |  607 | `		ph7_vm_func *pFunc = GenStateOwningFunc(&(*pGen));` |
|       - |  608 | `		char *zDup;` |
|       - |  609 | `		/* One name, one destination: php compile-rejects a label its function already` |
|       - |  610 | ``		 * declares, wherever the two sit (`L: L:`, one per branch of an if, one in a loop`` |
|       - |  611 | `		 * and one after it). PHL used to accept the redeclaration and silently give every` |
|       - |  612 | `		 * goto the FIRST one. The owning function is part of the key, so the same name in` |
|       - |  613 | `		 * another function — or at file scope beside it — is untouched by this. On the` |
|       - |  614 | `		 * duplicate, keep the first declaration and record nothing: the compile has already` |
|       - |  615 | `		 * failed, and a second entry under the same key would only shadow it. */` |
|     225 |  616 | `		if( SXRET_OK == GenStateGetLabel(&(*pGen),pTarget,pFunc,0) ){` |
|       8 |  617 | `			if( SXERR_ABORT == PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       2 |  618 | `				"Label '%z' already defined",pTarget) ){` |
|     ! 0 |  619 | `				return SXERR_ABORT;` |
|       - |  620 | `			}` |
|       6 |  621 | `			pGen->pIn += 2; /* Jump the label name and the semi-colon */` |
|       6 |  622 | `			return SXRET_OK;` |
|       - |  623 | `		}` |
|       - |  624 | `		/* Initialize label fields */` |
|     221 |  625 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       - |  626 | `		/* Duplicate label name */` |
|     221 |  627 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     221 |  628 | `		if( zDup == 0 ){` |
|     ! 0 |  629 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  630 | `			return SXERR_ABORT;` |
|       - |  631 | `		}` |
|     221 |  632 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|     221 |  633 | `		sLabel.nLine = pGen->pIn->nLine;` |
|     221 |  634 | `		sLabel.nLoopId = pGen->nCurLoopId;` |
|       - |  635 | `		/* Where the label sits, so a goto from a detached catch/finally body can be told` |
|       - |  636 | `		 * what it has to cross to reach it (GenStateJumpScope). */` |
|     221 |  637 | `		sLabel.pContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     221 |  638 | `		sLabel.nScopeId = pGen->nCurScopeId;` |
|       - |  639 | `		/* The owning FUNCTION, matched against the goto's in GenStateFixGoto. This used` |
|       - |  640 | `		 * to stop at GEN_BLOCK_EXCEPTION as well, which attributed every label inside a` |
|       - |  641 | `		 * try or catch body to "no function" — so from anywhere in a function such a` |
|       - |  642 | `		 * label read as undefined, including from the very catch body declaring it.` |
|       - |  643 | `		 * Whether a label may be jumped TO is decided by its container, not by this. */` |
|     221 |  644 | `		sLabel.pFunc = pFunc;` |
|       - |  645 | `		/* Insert in label set */` |
|     221 |  646 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|       - |  647 | `	}` |
|     221 |  648 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|     221 |  649 | `	return SXRET_OK;` |
|     115 |  650 | `}` |
|       - |  651 | `/*` |
|       - |  652 | ` * Compile the so hated 'goto' statement.` |
|       - |  653 | ` * You've probably been taught that gotos are bad, but this sort` |
|       - |  654 | ` * of rewriting  happens all the time, in fact every time you run` |
|       - |  655 | ` * a compiler it has to do this.` |
|       - |  656 | ` * According to the PHP language reference manual` |
|       - |  657 | ` *   The goto operator can be used to jump to another section in the program.` |
|       - |  658 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|       - |  659 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|       - |  660 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|       - |  661 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|       - |  662 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|       - |  663 | ` *   of a multi-level break` |
|       - |  664 | ` */` |
|     230 |  665 | `PH7_PRIVATE sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|       5 |  666 | `{` |
|       - |  667 | `	JumpFixup sJump;` |
|       - |  668 | `	sxi32 rc;` |
|     235 |  669 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|     235 |  670 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  671 | `		/* Missing label */` |
|     ! 0 |  672 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|     ! 0 |  673 | `		if( rc == SXERR_ABORT ){` |
|       - |  674 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  675 | `			return SXERR_ABORT;` |
|       - |  676 | `		}` |
|     ! 0 |  677 | `		return SXRET_OK;` |
|       - |  678 | `	}` |
|     235 |  679 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       5 |  680 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn,"identifier");` |
|       5 |  681 | `		if( rc == SXERR_ABORT ){` |
|       - |  682 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  683 | `			return SXERR_ABORT;` |
|       - |  684 | `		}` |
|       3 |  685 | `	}else{` |
|     231 |  686 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  687 | `		char *zDup;` |
|       - |  688 | `		/* Prepare the jump destination */` |
|     231 |  689 | `		sJump.nJumpType = PH7_OP_JMP;` |
|     231 |  690 | `		sJump.nLine = pGen->pIn->nLine;` |
|       - |  691 | `		/* Gotos resolve at end of compilation, well after any container swap. */` |
|     231 |  692 | `		sJump.pContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     231 |  693 | `		sJump.nScopeId = pGen->nCurScopeId;` |
|       - |  694 | `		/* Duplicate label name */` |
|     231 |  695 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     231 |  696 | `		if( zDup == 0 ){` |
|     ! 0 |  697 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  698 | `			return SXERR_ABORT;` |
|       - |  699 | `		}` |
|     231 |  700 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|       - |  701 | `		/* The loop/switch this goto sits in, for the "goto into a loop" check later. */` |
|     231 |  702 | `		sJump.nLoopId = pGen->nCurLoopId;` |
|       - |  703 | `		/* A goto inside a try{}/catch{} is legal php (jumping OUT of the block is fine);` |
|       - |  704 | `		 * only the owning function matters here, since a goto may not cross functions. */` |
|     231 |  705 | `		sJump.pFunc = GenStateOwningFunc(&(*pGen));` |
|       - |  706 | `		/* Emit the unconditional jump. Inside a DETACHED catch/finally body the target may` |
|       - |  707 | `		 * lie in another bytecode array, which a plain OP_JMP cannot address; enclosing` |
|       - |  708 | `		 * trys likewise need their finally run on the way out, which a plain jump would` |
|       - |  709 | `		 * skip. Emit a structure-crossing jump whenever either is possible — the label is` |
|       - |  710 | `		 * not known yet, so GenStateFixGoto picks the final opcode (and may downgrade it` |
|       - |  711 | `		 * back to OP_JMP once the counts prove to cancel). */` |
|     344 |  712 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,` |
|     226 |  713 | `			sJump.nScopeId > 0 ? PH7_OP_CATCH_JMP : PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|     231 |  714 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|     113 |  715 | `		}` |
|       - |  716 | `	}` |
|     235 |  717 | `	pGen->pIn++; /* Jump the label name */` |
|       - |  718 | ``	/* php reads `goto LABEL ;` and nothing else: a stray token there is its parse`` |
|       - |  719 | `	 * error naming the token, where this said so in a sentence of its own. */` |
|     230 |  720 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0` |
|     121 |  721 | `	 && PH7_GenSyntaxError(&(*pGen),pGen->pIn,"\";\"") == SXERR_ABORT ){` |
|     ! 0 |  722 | `		return SXERR_ABORT;` |
|       - |  723 | `	}` |
|       - |  724 | `	/* Statement successfully compiled */` |
|     235 |  725 | `	return SXRET_OK;` |
|     120 |  726 | `}` |
|       - |  727 | `/*` |
|       - |  728 | ` * Point to the next PHP chunk that will be processed shortly.` |
|       - |  729 | ` * Return SXRET_OK on success. Any other return value indicates` |
|       - |  730 | ` * failure.` |
|       - |  731 | ` */` |
|      28 |  732 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|       3 |  733 | `{` |
|       - |  734 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - |  735 | `	sxu32 nRawObj;` |
|      14 |  736 | `	sxu32 nObjIdx;` |
|       - |  737 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - |  738 | `	 * a PHP block.` |
|       - |  739 | `	 */` |
|       2 |  740 | `Consume:` |
|      35 |  741 | `	nRawObj = nObjIdx = 0;` |
|      47 |  742 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|      13 |  743 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|      13 |  744 | `		if( pRawObj == 0 ){` |
|     ! 0 |  745 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  746 | `			return SXERR_ABORT;` |
|       - |  747 | `		}` |
|       - |  748 | `		/* Mark as constant and emit the load constant instruction */` |
|      13 |  749 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|      13 |  750 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|      13 |  751 | `		++nRawObj;` |
|      13 |  752 | `		pGen->pRawIn++; /* Next chunk */` |
|       1 |  753 | `	}` |
|      35 |  754 | `	if( nRawObj > 0 ){` |
|       - |  755 | `		/* Emit the consume instruction */` |
|      13 |  756 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|       6 |  757 | `	}` |
|      35 |  758 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|      13 |  759 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|       - |  760 | `		/* Reset the token set (and its trivia sidecar) */` |
|      13 |  761 | `		SySetReset(pTokenSet);` |
|      13 |  762 | `		SySetReset(&pGen->aTrivia);` |
|       - |  763 | `		/* Tokenize input */` |
|      19 |  764 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|      12 |  765 | `			pGen->pRawIn->nLine,pTokenSet,&pGen->aTrivia);` |
|       - |  766 | `		/* Point to the fresh token stream */` |
|      13 |  767 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|      13 |  768 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|       - |  769 | `		/* Advance the stream cursor */` |
|      13 |  770 | `		pGen->pRawIn++;` |
|      13 |  771 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  772 | ``			/* The chunk held no TOKENS. `<?php // note ?>` is a whole PHP block that`` |
|       - |  773 | `			 * produces none, and so is a block holding only a comment of any kind --` |
|       - |  774 | `			 * the lexer emits the chunk (its bytes are not empty) and tokenizing it` |
|       - |  775 | `			 * yields nothing. Handing that empty stream back reads as END OF INPUT to` |
|       - |  776 | ``			 * every caller, so the enclosing block ended there and the real `}` two`` |
|       - |  777 | `			 * lines later was "Unmatched". A php TEMPLATE writes exactly this shape --` |
|       - |  778 | ``			 * `<?php } else { ?>` … `<?php // why ?>` … `<?php } ?>` is symfony's`` |
|       - |  779 | `			 * error-handler view -- so take the NEXT chunk instead. The cursor has` |
|       - |  780 | `			 * already advanced, so the loop always makes progress. */` |
|       5 |  781 | `			goto Consume;` |
|       - |  782 | `		}` |
|       - |  783 | `		/* TICKET 1433-011 */` |
|       9 |  784 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - |  785 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - |  786 | `			sxi32 rc;` |
|       - |  787 | `			/* Refer to TICKET 1433-009  */` |
|     ! 0 |  788 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|     ! 0 |  789 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|     ! 0 |  790 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       - |  791 | `			/* Synthesized short-tag echo: legitimately an expression here. */` |
|     ! 0 |  792 | `			pGen->nExprEchoOk++;` |
|     ! 0 |  793 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 |  794 | `			pGen->nExprEchoOk--;` |
|     ! 0 |  795 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  796 | `				return SXERR_ABORT;` |
|     ! 0 |  797 | `			}else if( rc != SXERR_EMPTY ){` |
|     ! 0 |  798 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  799 | `			}` |
|     ! 0 |  800 | `			goto Consume;` |
|       - |  801 | `		}` |
|       5 |  802 | `	}else{` |
|       - |  803 | `		/* No more chunks to process */` |
|      22 |  804 | `		pGen->pIn = pGen->pEnd;` |
|      22 |  805 | `		return SXERR_EOF;` |
|       - |  806 | `	}` |
|       9 |  807 | `	return SXRET_OK;` |
|      17 |  808 | `}` |
|       - |  809 | `/*` |
|       - |  810 | ` * Compile a PHP block.` |
|       - |  811 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - |  812 | ` * optionally delimited by braces {}.` |
|       - |  813 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  814 | ` * and this function takes care of generating the appropriate error` |
|       - |  815 | ` * message.` |
|       - |  816 | ` */` |
|  861581 |  817 | `PH7_PRIVATE sxi32 PH7_CompileBlock(` |
|       - |  818 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  819 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - |  820 | `	)` |
|       5 |  821 | `{` |
|       - |  822 | `	sxi32 rc;` |
|       - |  823 | `	sxu32 nLine;` |
|  861586 |  824 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  860384 |  825 | `		nLine = pGen->pIn->nLine;` |
|  860384 |  826 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  860384 |  827 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  828 | `			return SXERR_ABORT;` |
|       - |  829 | `		}` |
|  860384 |  830 | `		pGen->pIn++;` |
|       - |  831 | `		/* Compile until we hit the closing braces '}' */` |
| 1325655 |  832 | `		for(;;){` |
| 2655036 |  833 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      31 |  834 | `				rc = GenStateNextChunk(&(*pGen));` |
|      31 |  835 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  836 | `			 	   return SXERR_ABORT;` |
|       - |  837 | `				}` |
|      31 |  838 | `				if( rc == SXERR_EOF ){` |
|       - |  839 | `					/* No more token to process: the block was never closed. php reports` |
|       - |  840 | `					 * the line the '{' was opened on, not where the input ran out. */` |
|      22 |  841 | `					PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"Unclosed '{' on line %u",nLine);` |
|      22 |  842 | `					break;` |
|       - |  843 | `				}` |
|       4 |  844 | `			}` |
| 2655016 |  845 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - |  846 | `				/* Closing braces found,break immediately*/` |
|  860360 |  847 | `				pGen->pIn++;` |
|  860360 |  848 | `				break;` |
|       - |  849 | `			}` |
|       - |  850 | `			/* Compile a single statement */` |
| 1794661 |  851 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
| 1794661 |  852 | `			if( rc == SXERR_ABORT ){` |
|       5 |  853 | `				return SXERR_ABORT;` |
|       - |  854 | `			}` |
|       5 |  855 | `		}` |
|  860380 |  856 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  430797 |  857 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|      19 |  858 | `		pGen->pIn++;` |
|      19 |  859 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|      19 |  860 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  861 | `			return SXERR_ABORT;` |
|       - |  862 | `		}` |
|       - |  863 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|      18 |  864 | `		for(;;){` |
|      37 |  865 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  866 | `				rc = GenStateNextChunk(&(*pGen));` |
|     ! 0 |  867 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  868 | `			 	   return SXERR_ABORT;` |
|       - |  869 | `				}` |
|     ! 0 |  870 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|       - |  871 | `					/* No more token to process */` |
|     ! 0 |  872 | `					if( rc == SXERR_EOF ){` |
|     ! 0 |  873 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|       - |  874 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|     ! 0 |  875 | `					}` |
|     ! 0 |  876 | `					break;` |
|       - |  877 | `				}` |
|     ! 0 |  878 | `			}` |
|      37 |  879 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - |  880 | `				sxi32 nKwrd;` |
|       - |  881 | `				/* Keyword found */` |
|      35 |  882 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      39 |  883 | `				if( nKwrd == nKeywordEnd \|\|` |
|      14 |  884 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|       - |  885 | `						/* Delimiter keyword found,break */` |
|      19 |  886 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|      17 |  887 | `							pGen->pIn++; /*  endif;endswitch... */` |
|       8 |  888 | `						}` |
|      19 |  889 | `						break;` |
|       - |  890 | `				}` |
|       8 |  891 | `			}` |
|       - |  892 | `			/* Compile a single statement */` |
|      19 |  893 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|      19 |  894 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  895 | `				return SXERR_ABORT;` |
|       - |  896 | `			}` |
|       1 |  897 | `		}` |
|      19 |  898 | `		GenStateLeaveBlock(&(*pGen),0);` |
|      10 |  899 | `	}else{` |
|       - |  900 | `		/* Compile a single statement */` |
|    1189 |  901 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1189 |  902 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  903 | `			return SXERR_ABORT;` |
|       - |  904 | `		}` |
|       - |  905 | `	}` |
|       - |  906 | `	/* Jump trailing semi-colons ';' */` |
|  861594 |  907 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      13 |  908 | `		pGen->pIn++;` |
|       1 |  909 | `	}` |
|  861582 |  910 | `	return SXRET_OK;` |
|  430198 |  911 | `}` |
|       - |  912 | `/*` |
|       - |  913 | ` * Compile the gentle 'while' statement.` |
|       - |  914 | ` * According to the PHP language reference` |
|       - |  915 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|       - |  916 | ` *  The basic form of a while statement is:` |
|       - |  917 | ` *  while (expr)` |
|       - |  918 | ` *   statement` |
|       - |  919 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|       - |  920 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|       - |  921 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|       - |  922 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|       - |  923 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|       - |  924 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|       - |  925 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|       - |  926 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|       - |  927 | ` *  while (expr):` |
|       - |  928 | ` *    statement` |
|       - |  929 | ` *   endwhile;` |
|       - |  930 | ` */` |
|   13908 |  931 | `PH7_PRIVATE sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       5 |  932 | `{` |
|   13913 |  933 | `	GenBlock *pWhileBlock = 0;` |
|   13913 |  934 | `	SyToken *pTmp,*pEnd = 0;` |
|       - |  935 | `	sxu32 nFalseJump;` |
|       - |  936 | `	sxu32 nLine;` |
|       - |  937 | `	sxi32 rc;` |
|   13913 |  938 | `	nLine = pGen->pIn->nLine;` |
|       - |  939 | `	/* Jump the 'while' keyword */` |
|   13913 |  940 | `	pGen->pIn++;` |
|   13913 |  941 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  942 | `		/* Syntax error */` |
|     ! 0 |  943 | `		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  944 | `		if( rc == SXERR_ABORT ){` |
|       - |  945 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  946 | `			return SXERR_ABORT;` |
|       - |  947 | `		}` |
|     ! 0 |  948 | `		goto Synchronize;` |
|       - |  949 | `	}` |
|       - |  950 | `	/* Jump the left parenthesis '(' */` |
|   13913 |  951 | `	pGen->pIn++;` |
|       - |  952 | `	/* Create the loop block */` |
|   13913 |  953 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   13913 |  954 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  955 | `		return SXERR_ABORT;` |
|       - |  956 | `	}` |
|       - |  957 | `	/* Delimit the condition */` |
|   13913 |  958 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   13913 |  959 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  960 | `		/* Empty condition. php reports the token that actually stopped it -- for` |
|       - |  961 | ``		 * `while ()` that is the ')' -- not a hand-written "expected expression". */`` |
|       3 |  962 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,0);` |
|       3 |  963 | `		if( rc == SXERR_ABORT ){` |
|       - |  964 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  965 | `			return SXERR_ABORT;` |
|       - |  966 | `		}` |
|       1 |  967 | `	}` |
|       - |  968 | `	/* Swap token streams */` |
|   13913 |  969 | `	pTmp = pGen->pEnd;` |
|   13913 |  970 | `	pGen->pEnd = pEnd;` |
|       - |  971 | `	/* Compile the expression */` |
|   13913 |  972 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   13913 |  973 | `	if( rc == SXERR_ABORT ){` |
|       - |  974 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  975 | `		return SXERR_ABORT;` |
|       - |  976 | `	}` |
|       - |  977 | `	/* Update token stream */` |
|   13913 |  978 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  979 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|     ! 0 |  980 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  981 | `			return SXERR_ABORT;` |
|       - |  982 | `		}` |
|     ! 0 |  983 | `		pGen->pIn++;` |
|     ! 0 |  984 | `	}` |
|       - |  985 | `	/* Synchronize pointers */` |
|   13913 |  986 | `	pGen->pIn  = &pEnd[1];` |
|   13913 |  987 | `	pGen->pEnd = pTmp;` |
|       - |  988 | `	/* Emit the false jump */` |
|   13913 |  989 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  990 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   13913 |  991 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - |  992 | `	/* Compile the loop body */` |
|   13913 |  993 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   13913 |  994 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  995 | `		return SXERR_ABORT;` |
|       - |  996 | `	}` |
|       - |  997 | `	/* Emit the unconditional jump to the start of the loop */` |
|   13913 |  998 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - |  999 | `	/* Fix all jumps now the destination is resolved */` |
|   13913 | 1000 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 1001 | `	/* Release the loop block */` |
|   13913 | 1002 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 1003 | `	/* Statement successfully compiled */` |
|   13913 | 1004 | `	return SXRET_OK;` |
|     ! 0 | 1005 | `Synchronize:` |
|       - | 1006 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 1007 | `	 * compiling this erroneous block.` |
|       - | 1008 | `	 */` |
|     ! 0 | 1009 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 1010 | `		pGen->pIn++;` |
|     ! 0 | 1011 | `	}` |
|     ! 0 | 1012 | `	return SXRET_OK;` |
|    6950 | 1013 | `}` |
|       - | 1014 | `/*` |
|       - | 1015 | ` * Compile the ugly do..while() statement.` |
|       - | 1016 | ` * According to the PHP language reference` |
|       - | 1017 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|       - | 1018 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|       - | 1019 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|       - | 1020 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|       - | 1021 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|       - | 1022 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|       - | 1023 | ` *  would end immediately).` |
|       - | 1024 | ` *  There is just one syntax for do-while loops:` |
|       - | 1025 | ` *  <?php` |
|       - | 1026 | ` *  $i = 0;` |
|       - | 1027 | ` *  do {` |
|       - | 1028 | ` *   echo $i;` |
|       - | 1029 | ` *  } while ($i > 0);` |
|       - | 1030 | ` * ?>` |
|       - | 1031 | ` */` |
|      12 | 1032 | `PH7_PRIVATE sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|       4 | 1033 | `{` |
|      16 | 1034 | `	SyToken *pTmp,*pEnd = 0;` |
|      16 | 1035 | `	GenBlock *pDoBlock = 0;` |
|       - | 1036 | `	sxu32 nLine;` |
|       - | 1037 | `	sxi32 rc;` |
|      16 | 1038 | `	nLine = pGen->pIn->nLine;` |
|       - | 1039 | `	/* Jump the 'do' keyword */` |
|      16 | 1040 | `	pGen->pIn++;` |
|       - | 1041 | `	/* Create the loop block */` |
|      16 | 1042 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|      16 | 1043 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1044 | `		return SXERR_ABORT;` |
|       - | 1045 | `	}` |
|       - | 1046 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|      16 | 1047 | `	pDoBlock->bPostContinue = TRUE;` |
|      16 | 1048 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      16 | 1049 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 1050 | `		return SXERR_ABORT;` |
|       - | 1051 | `	}` |
|      16 | 1052 | `	if( pGen->pIn < pGen->pEnd ){` |
|      13 | 1053 | `		nLine = pGen->pIn->nLine;` |
|       5 | 1054 | `	}` |
|      16 | 1055 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|      10 | 1056 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|       - | 1057 | `			/* Missing 'while' statement */` |
|       - | 1058 | ``			/* php: `do {} ;` names the ';', while `do {}` at EOF is "unexpected end of`` |
|       - | 1059 | `			 * file". The do-block's own slice stops before its terminator, so look at` |
|       - | 1060 | `			 * the whole CHUNK stream: a token still there is the one php names; nothing` |
|       - | 1061 | `			 * left means end of file (NULL). */` |
|       - | 1062 | `			{` |
|       3 | 1063 | `				SyToken *pBad = pGen->pIn < pGen->pEnd ? pGen->pIn : 0;` |
|       3 | 1064 | `				if( pBad == 0 && pGen->pTokenSet ){` |
|       - | 1065 | `					/* The do-block consumed its terminator, so the token php names is` |
|       - | 1066 | `					 * the one just behind the cursor -- but only when it is a real` |
|       - | 1067 | ``					 * terminator (`do {} ;` -> the ';'). If the block simply ended on`` |
|       - | 1068 | `					 * its '}' with nothing after it, php reports end of file. */` |
|       3 | 1069 | `					SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|       3 | 1070 | `					if( pGen->pIn > pBase && (pGen->pIn[-1].nType & PH7_TK_SEMI) ){` |
|     ! 0 | 1071 | `						pBad = &pGen->pIn[-1];` |
|     ! 0 | 1072 | `					}` |
|       1 | 1073 | `				}` |
|       3 | 1074 | `				rc = PH7_GenSyntaxError(pGen,pBad,"\"while\"");` |
|       - | 1075 | `			}` |
|       3 | 1076 | `			if( rc == SXERR_ABORT ){` |
|       - | 1077 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 1078 | `				return SXERR_ABORT;` |
|       - | 1079 | `			}` |
|       3 | 1080 | `			goto Synchronize;` |
|       - | 1081 | `	}` |
|       - | 1082 | `	/* Jump the 'while' keyword */` |
|      13 | 1083 | `	pGen->pIn++;` |
|      13 | 1084 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 1085 | `		/* Syntax error */` |
|     ! 0 | 1086 | `		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 | 1087 | `		if( rc == SXERR_ABORT ){` |
|       - | 1088 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1089 | `			return SXERR_ABORT;` |
|       - | 1090 | `		}` |
|     ! 0 | 1091 | `		goto Synchronize;` |
|       - | 1092 | `	}` |
|       - | 1093 | `	/* Jump the left parenthesis '(' */` |
|      13 | 1094 | `	pGen->pIn++;` |
|       - | 1095 | `	/* Delimit the condition */` |
|      13 | 1096 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      13 | 1097 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 1098 | `		/* Empty condition. php reports the token that actually stopped it -- for` |
|       - | 1099 | ``		 * `while ()` that is the ')' -- not a hand-written "expected expression". */`` |
|     ! 0 | 1100 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,0);` |
|     ! 0 | 1101 | `		if( rc == SXERR_ABORT ){` |
|       - | 1102 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1103 | `			return SXERR_ABORT;` |
|       - | 1104 | `		}` |
|     ! 0 | 1105 | `		goto Synchronize;` |
|       - | 1106 | `	}` |
|       - | 1107 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|      13 | 1108 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|       - | 1109 | `		JumpFixup *aPost;` |
|       - | 1110 | `		VmInstr *pInstr;` |
|       - | 1111 | `		sxu32 nJumpDest;` |
|       - | 1112 | `		sxu32 n;` |
|       3 | 1113 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|       3 | 1114 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       5 | 1115 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|       3 | 1116 | `			pInstr = GenStateFixupInstr(&aPost[n]);` |
|       3 | 1117 | `			if( pInstr ){` |
|       - | 1118 | `				/* Fix */` |
|       3 | 1119 | `				pInstr->iP2 = nJumpDest;` |
|       1 | 1120 | `			}` |
|       2 | 1121 | `		}` |
|       1 | 1122 | `	}` |
|       - | 1123 | `	/* Swap token streams */` |
|      13 | 1124 | `	pTmp = pGen->pEnd;` |
|      13 | 1125 | `	pGen->pEnd = pEnd;` |
|       - | 1126 | `	/* Compile the expression */` |
|      13 | 1127 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      13 | 1128 | `	if( rc == SXERR_ABORT ){` |
|       - | 1129 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 1130 | `		return SXERR_ABORT;` |
|       - | 1131 | `	}` |
|       - | 1132 | `	/* Update token stream */` |
|      13 | 1133 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 1134 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|     ! 0 | 1135 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1136 | `			return SXERR_ABORT;` |
|       - | 1137 | `		}` |
|     ! 0 | 1138 | `		pGen->pIn++;` |
|     ! 0 | 1139 | `	}` |
|      13 | 1140 | `	pGen->pIn  = &pEnd[1];` |
|      13 | 1141 | `	pGen->pEnd = pTmp;` |
|       - | 1142 | `	/* Emit the true jump to the beginning of the loop */` |
|      13 | 1143 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|       - | 1144 | `	/* Fix all jumps now the destination is resolved */` |
|      13 | 1145 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 1146 | `	/* Release the loop block */` |
|      13 | 1147 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 1148 | `	/* Statement successfully compiled */` |
|      13 | 1149 | `	return SXRET_OK;` |
|       1 | 1150 | `Synchronize:` |
|       - | 1151 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 1152 | `	 * compiling this erroneous block.` |
|       - | 1153 | `	 */` |
|       3 | 1154 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 1155 | `		pGen->pIn++;` |
|     ! 0 | 1156 | `	}` |
|       3 | 1157 | `	return SXRET_OK;` |
|      10 | 1158 | `}` |
|       - | 1159 | `/*` |
|       - | 1160 | ` * Compile the complex and powerful 'for' statement.` |
|       - | 1161 | ` * According to the PHP language reference` |
|       - | 1162 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|       - | 1163 | ` *  The syntax of a for loop is:` |
|       - | 1164 | ` *  for (expr1; expr2; expr3)` |
|       - | 1165 | ` *   statement` |
|       - | 1166 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|       - | 1167 | ` *  the beginning of the loop.` |
|       - | 1168 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|       - | 1169 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|       - | 1170 | ` *  to FALSE, the execution of the loop ends.` |
|       - | 1171 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|       - | 1172 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|       - | 1173 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|       - | 1174 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|       - | 1175 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|       - | 1176 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|       - | 1177 | ` *  of using the for truth expression.` |
|       - | 1178 | ` */` |
|   40817 | 1179 | `PH7_PRIVATE sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       5 | 1180 | `{` |
|   40822 | 1181 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   40822 | 1182 | `	GenBlock *pForBlock = 0;` |
|       - | 1183 | `	sxu32 nFalseJump;` |
|       - | 1184 | `	sxu32 nLine;` |
|       - | 1185 | `	sxi32 rc;` |
|   40822 | 1186 | `	nLine = pGen->pIn->nLine;` |
|       - | 1187 | `	/* Jump the 'for' keyword */` |
|   40822 | 1188 | `	pGen->pIn++;` |
|   40822 | 1189 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 1190 | `		/* Syntax error */` |
|     ! 0 | 1191 | `		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 | 1192 | `		if( rc == SXERR_ABORT ){` |
|       - | 1193 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1194 | `			return SXERR_ABORT;` |
|       - | 1195 | `		}` |
|     ! 0 | 1196 | `		return SXRET_OK;` |
|       - | 1197 | `	}` |
|       - | 1198 | `	/* Jump the left parenthesis '(' */` |
|   40822 | 1199 | `	pGen->pIn++;` |
|       - | 1200 | `	/* Delimit the init-expr;condition;post-expr */` |
|   40822 | 1201 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   40822 | 1202 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 1203 | `		/* Empty expression */` |
|     ! 0 | 1204 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|     ! 0 | 1205 | `		if( rc == SXERR_ABORT ){` |
|       - | 1206 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1207 | `			return SXERR_ABORT;` |
|       - | 1208 | `		}` |
|       - | 1209 | `		/* Synchronize */` |
|     ! 0 | 1210 | `		pGen->pIn = pEnd;` |
|     ! 0 | 1211 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 1212 | `			pGen->pIn++;` |
|     ! 0 | 1213 | `		}` |
|     ! 0 | 1214 | `		return SXRET_OK;` |
|       - | 1215 | `	}` |
|       - | 1216 | `	/* Swap token streams */` |
|   40822 | 1217 | `	pTmp = pGen->pEnd;` |
|   40822 | 1218 | `	pGen->pEnd = pEnd;` |
|       - | 1219 | `	/* for() clauses are the ONLY place php's grammar allows a comma-separated` |
|       - | 1220 | `	 * expression list, so the comma operator is permitted for their duration` |
|       - | 1221 | `	 * (see GenStateTreeHasComma). A closure body nested inside a clause is` |
|       - | 1222 | `	 * compiled through this same window — recorded as a known leniency. */` |
|   40822 | 1223 | `	pGen->nCommaExprOk++;` |
|   40822 | 1224 | `	pGen->zClauseCloser = "\";\""; /* init/condition clauses close on ';' */` |
|       - | 1225 | `` 	/* Compile initialization expressions if available. Every element of a `for` `` |
|       - | 1226 | ``	 * clause is a statement position in php, `(void)` cast included. */`` |
|   40822 | 1227 | `	GenStateEnableClauseVoidCasts(&(*pGen),1);` |
|   40822 | 1228 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 1229 | `	/* Pop operand lvalues */` |
|   40822 | 1230 | `	if( rc == SXERR_ABORT ){` |
|       - | 1231 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 1232 | `		return SXERR_ABORT;` |
|   40822 | 1233 | `	}else if( rc != SXERR_EMPTY ){` |
|   40818 | 1234 | `		GenStateMarkDiscardedCall(&(*pGen));` |
|   40818 | 1235 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|   20379 | 1236 | `	}` |
|   40822 | 1237 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 1238 | `		/* Syntax error */` |
|     ! 0 | 1239 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|     ! 0 | 1240 | `		if( rc == SXERR_ABORT ){` |
|       - | 1241 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1242 | `			return SXERR_ABORT;` |
|       - | 1243 | `		}` |
|     ! 0 | 1244 | `		return SXRET_OK;` |
|       - | 1245 | `	}` |
|       - | 1246 | `	/* Jump the trailing ';' */` |
|   40822 | 1247 | `	pGen->pIn++;` |
|       - | 1248 | `	/* Create the loop block */` |
|   40822 | 1249 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   40822 | 1250 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1251 | `		return SXERR_ABORT;` |
|       - | 1252 | `	}` |
|       - | 1253 | `	/* Deffer continue jumps */` |
|   40822 | 1254 | `	pForBlock->bPostContinue = TRUE;` |
|       - | 1255 | `	/* Compile the condition */` |
|   40822 | 1256 | `	if( GenStateEnableClauseVoidCasts(&(*pGen),0) ){` |
|       - | 1257 | `		/* php names the clause terminator, not the cast, when the offending` |
|       - | 1258 | ``		 * `(void)` is on the element that has to BE the condition. */`` |
|       3 | 1259 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|       - | 1260 | `			"syntax error, unexpected token \";\", expecting \",\"");` |
|       3 | 1261 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1262 | `			return SXERR_ABORT;` |
|       - | 1263 | `		}` |
|       3 | 1264 | `		return SXRET_OK;` |
|       - | 1265 | `	}` |
|   40820 | 1266 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   40820 | 1267 | `	if( rc == SXERR_ABORT ){` |
|       - | 1268 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 1269 | `		return SXERR_ABORT;` |
|   40820 | 1270 | `	}else if( rc != SXERR_EMPTY ){` |
|       - | 1271 | `		/* Emit the false jump */` |
|   40816 | 1272 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 1273 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   40816 | 1274 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|   20378 | 1275 | `	}` |
|   40820 | 1276 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 1277 | `		/* Syntax error */` |
|       6 | 1278 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|       6 | 1279 | `		if( rc == SXERR_ABORT ){` |
|       - | 1280 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1281 | `			return SXERR_ABORT;` |
|       - | 1282 | `		}` |
|       6 | 1283 | `		return SXRET_OK;` |
|       - | 1284 | `	}` |
|       - | 1285 | `	/* Jump the trailing ';' */` |
|   40816 | 1286 | `	pGen->pIn++;` |
|       - | 1287 | `	/* Save the post condition stream */` |
|   40816 | 1288 | `	pPostStart = pGen->pIn;` |
|       - | 1289 | `	/* Compile the loop body — OUTSIDE the comma window (the body is ordinary` |
|       - | 1290 | ``	 * php, so `(1, 2)` inside it is the parse error it should be). */`` |
|   40816 | 1291 | `	pGen->nCommaExprOk--;` |
|   40816 | 1292 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   40816 | 1293 | `	pGen->pEnd = pTmp;` |
|   40816 | 1294 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   40816 | 1295 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 1296 | `		return SXERR_ABORT;` |
|       - | 1297 | `	}` |
|       - | 1298 | `	/* Fix post-continue jumps */` |
|   40816 | 1299 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - | 1300 | `		JumpFixup *aPost;` |
|       - | 1301 | `		VmInstr *pInstr;` |
|       - | 1302 | `		sxu32 nJumpDest;` |
|       - | 1303 | `		sxu32 n;` |
|   13467 | 1304 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|   13467 | 1305 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|   26929 | 1306 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|   13467 | 1307 | `			pInstr = GenStateFixupInstr(&aPost[n]);` |
|   13467 | 1308 | `			if( pInstr ){` |
|       - | 1309 | `				/* Fix jump */` |
|   13467 | 1310 | `				pInstr->iP2 = nJumpDest;` |
|    6722 | 1311 | `			}` |
|    6727 | 1312 | `		}` |
|    6722 | 1313 | `	}` |
|       - | 1314 | `	/* compile the post-expressions if available */` |
|   40816 | 1315 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 1316 | `		pPostStart++;` |
|     ! 0 | 1317 | `	}` |
|   40816 | 1318 | `	if( pPostStart < pEnd ){` |
|       - | 1319 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   40814 | 1320 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   40814 | 1321 | `		pGen->nCommaExprOk++; /* post-expressions are a clause list again */` |
|   40814 | 1322 | `		pGen->zClauseCloser = "\")\""; /* the post clause closes on ')' */` |
|   40814 | 1323 | `		GenStateEnableClauseVoidCasts(&(*pGen),1);` |
|   40814 | 1324 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   40814 | 1325 | `		pGen->nCommaExprOk--;` |
|   40814 | 1326 | `		pGen->zClauseCloser = 0;` |
|   40814 | 1327 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 1328 | `			/* php names the token and expects ')'; the post-clause runs to the ')'. */` |
|     ! 0 | 1329 | `			rc = PH7_GenSyntaxError(pGen,pGen->pIn,"\")\"");` |
|     ! 0 | 1330 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1331 | `				return SXERR_ABORT;` |
|       - | 1332 | `			}` |
|     ! 0 | 1333 | `			return SXRET_OK;` |
|       - | 1334 | `		}` |
|   40814 | 1335 | `		RE_SWAP_DELIMITER(pGen);` |
|   40814 | 1336 | `		if( rc == SXERR_ABORT ){` |
|       - | 1337 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 1338 | `			return SXERR_ABORT;` |
|   40814 | 1339 | `		}else if( rc != SXERR_EMPTY){` |
|   40814 | 1340 | `			GenStateMarkDiscardedCall(&(*pGen));` |
|       - | 1341 | `			/* Pop operand lvalue */` |
|   40814 | 1342 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|   20377 | 1343 | `		}` |
|   20377 | 1344 | `	}` |
|       - | 1345 | `	/* Emit the unconditional jump to the start of the loop */` |
|   40816 | 1346 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - | 1347 | `	/* Fix all jumps now the destination is resolved */` |
|   40816 | 1348 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 1349 | `	/* Release the loop block */` |
|   40816 | 1350 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 1351 | `	/* Statement successfully compiled */` |
|   40816 | 1352 | `	return SXRET_OK;` |
|   20386 | 1353 | `}` |
|       - | 1354 | `/*` |
|       - | 1355 | ` * Is this keyword token part of a NAME rather than a keyword in its own right?` |
|       - | 1356 | ` *` |
|       - | 1357 | `` * The lexer marks `as` a keyword wherever it appears, and php lets it BE a name`` |
|       - | 1358 | ` * in three places the token stream reaches through a preceding operator: a` |
|       - | 1359 | ` * variable ($as lexes as PH7_TK_DOLLAR followed by the identifier), a property` |
|       - | 1360 | ` * ($o->as, $o?->as) and a class constant (A::as). foreach's separator scan has` |
|       - | 1361 | ` * to look at what PRECEDES the keyword, or it splits inside the name and hands` |
|       - | 1362 | `` * the subject compiler a bare `$`.`` |
|       - | 1363 | ` */` |
|   71309 | 1364 | `static int GenStateKeywordIsName(SyToken *pStart,SyToken *pCur)` |
|       5 | 1365 | `{` |
|       - | 1366 | `	SyToken *pPrev;` |
|   71314 | 1367 | `	if( pCur <= pStart ){` |
|     ! 0 | 1368 | `		return 0;` |
|       - | 1369 | `	}` |
|   71314 | 1370 | `	pPrev = pCur - 1;` |
|   71314 | 1371 | `	if( pPrev->nType & PH7_TK_DOLLAR ){` |
|       6 | 1372 | `		return 1;` |
|       - | 1373 | `	}` |
|   71310 | 1374 | `	if( (pPrev->nType & PH7_TK_OP) == 0 ){` |
|   71302 | 1375 | `		return 0;` |
|       - | 1376 | `	}` |
|      13 | 1377 | `	return (pPrev->sData.nByte == sizeof("->")-1` |
|       6 | 1378 | `			&& SyMemcmp(pPrev->sData.zString,"->",sizeof("->")-1) == 0)` |
|       7 | 1379 | `		\|\| (pPrev->sData.nByte == sizeof("::")-1` |
|       3 | 1380 | `			&& SyMemcmp(pPrev->sData.zString,"::",sizeof("::")-1) == 0)` |
|      16 | 1381 | `		\|\| (pPrev->sData.nByte == sizeof("?->")-1` |
|       4 | 1382 | `			&& SyMemcmp(pPrev->sData.zString,"?->",sizeof("?->")-1) == 0);` |
|   35603 | 1383 | `}` |
|       - | 1384 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - | 1385 | ` *` |
|       - | 1386 | `` * php's `as` target is any WRITABLE expression, not just a variable: a property`` |
|       - | 1387 | ` * ($o->p), a static property (C::$s), an array element ($a['k']) and an append` |
|       - | 1388 | ` * ($a[]) are all accepted, on the key side as much as on the value side. The` |
|       - | 1389 | ` * three shapes php rejects get php's own wording; everything else keeps PH7's.` |
|       - | 1390 | ` */` |
|   85362 | 1391 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 | 1392 | `{` |
|   85367 | 1393 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|   85367 | 1394 | `	const char *zMsg = 0;` |
|       - | 1395 | ``	/* A loop target is a write target: `as $this` and `as (new A)->p` are php's`` |
|       - | 1396 | `	 * own compile fatals. */` |
|   85367 | 1397 | `	rc = GenStateWriteTargetCheck(&(*pGen),pRoot,0);` |
|   85367 | 1398 | `	if( rc != SXRET_OK ){` |
|       3 | 1399 | `		return rc;` |
|       - | 1400 | `	}` |
|   85365 | 1401 | `	if( pRoot->pOp ){` |
|      40 | 1402 | `		switch( pRoot->pOp->iOp ){` |
|      17 | 1403 | `		case EXPR_OP_ARROW:     /* $o->p */` |
|       - | 1404 | `		case EXPR_OP_SUBSCRIPT: /* $a['k'], $a[] */` |
|      35 | 1405 | `			return SXRET_OK;` |
|       2 | 1406 | ``		case EXPR_OP_DC:        /* C::$s — but `as C::K` is php's parse error */`` |
|       6 | 1407 | `			if( !PH7_ExprNodeIsClassConst(pRoot) ){` |
|       3 | 1408 | `				return SXRET_OK;` |
|       - | 1409 | `			}` |
|       3 | 1410 | `			break;` |
|     ! 0 | 1411 | `		case EXPR_OP_NULLSAFE_ARROW:` |
|     ! 0 | 1412 | `			zMsg = "Can't use nullsafe operator in write context";` |
|     ! 0 | 1413 | `			break;` |
|       - | 1414 | ``		/* A CALL target (`as f()`) never reaches here — GenStateWriteTargetCheck`` |
|       - | 1415 | `		 * above already refused it, and with php's function/method distinction. */` |
|     ! 0 | 1416 | `		default:` |
|     ! 0 | 1417 | `			break;` |
|       - | 1418 | `		}` |
|   85324 | 1419 | `	}else if( pRoot->xCode == PH7_CompileVariable ){` |
|   85327 | 1420 | `		return SXRET_OK;` |
|       - | 1421 | `	}` |
|       3 | 1422 | `	if( zMsg == 0 ){` |
|       - | 1423 | ``		/* Not a `variable` in php's grammar: php's own syntax error, which names`` |
|       - | 1424 | `		 * the token that follows the target. */` |
|       3 | 1425 | `		return PH7_ExprOperandNotAVariable(&(*pGen),pRoot);` |
|       - | 1426 | `	}` |
|     ! 0 | 1427 | `	rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,zMsg);` |
|     ! 0 | 1428 | `	if( rc != SXERR_ABORT ){` |
|     ! 0 | 1429 | `		rc = SXERR_INVALID;` |
|     ! 0 | 1430 | `	}` |
|     ! 0 | 1431 | `	return rc;` |
|   42618 | 1432 | `}` |
|       - | 1433 | `/*` |
|       - | 1434 | `` * Is this `as` target the plain `$name` shape?`` |
|       - | 1435 | ` *` |
|       - | 1436 | ` * Only that shape can be installed by NAME the way ph7_foreach_info records it` |
|       - | 1437 | ` * (the step writes straight into the frame's symbol table). Every other writable` |
|       - | 1438 | ` * target — including a variable-variable, whose name php re-evaluates per step —` |
|       - | 1439 | ` * goes through a synthetic temporary plus a real store (GenStateForeachStoreTarget).` |
|       - | 1440 | ` */` |
|   85362 | 1441 | `static int GenStateForeachTargetIsPlainVar(SyToken *pStart,SyToken *pEnd)` |
|       5 | 1442 | `{` |
|  127960 | 1443 | `	return (pEnd == &pStart[2])` |
|   85342 | 1444 | `		&& (pStart[0].nType & PH7_TK_DOLLAR)` |
|  128091 | 1445 | `		&& (pStart[1].nType & PH7_TK_ID);` |
|       5 | 1446 | `}` |
|       - | 1447 | `/*` |
|       - | 1448 | `` * Reserve the synthetic temporary a complex `as` target's step value lands in.`` |
|       - | 1449 | ` * The bracketed name cannot collide with a user variable — the same trick the` |
|       - | 1450 | ` * list()/[...] destructuring path uses.` |
|       - | 1451 | ` */` |
|     116 | 1452 | `static sxi32 GenStateForeachTempName(ph7_gen_state *pGen,const char *zTag,SyString *pOut)` |
|       5 | 1453 | `{` |
|       - | 1454 | `	static int iForeachTargetCnt = 0;` |
|       - | 1455 | `	char zTmp[128];` |
|       - | 1456 | `	sxu32 nLen;` |
|       - | 1457 | `	char *zDup;` |
|     121 | 1458 | `	nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_%s_%d__]",zTag,iForeachTargetCnt++);` |
|     121 | 1459 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|     121 | 1460 | `	if( zDup == 0 ){` |
|     ! 0 | 1461 | `		return SXERR_ABORT;` |
|       - | 1462 | `	}` |
|     121 | 1463 | `	SyStringInitFromBuf(pOut,zDup,nLen);` |
|     121 | 1464 | `	return SXRET_OK;` |
|      63 | 1465 | `}` |
|       - | 1466 | `/*` |
|       - | 1467 | `` * Emit `<target> = <temp>` (or `<target> =& <temp>` for a by-reference value) for`` |
|       - | 1468 | `` * one complex `as` target, at the top of the loop body — where php performs the`` |
|       - | 1469 | ` * assignment, once per step.` |
|       - | 1470 | ` *` |
|       - | 1471 | ` * The store is folded exactly as the assignment operator's own codegen folds it` |
|       - | 1472 | ` * (compile.c, precedence-18 site): a member LHS keeps its OP_MEMBER, a subscript` |
|       - | 1473 | ` * becomes STORE_IDX, and a plain name folds into the STORE's p3.` |
|       - | 1474 | ` */` |
|     116 | 1475 | `static sxi32 GenStateForeachStoreTarget(` |
|       - | 1476 | `	ph7_gen_state *pGen,` |
|       - | 1477 | `	SyString *pTemp,   /* Synthetic variable holding this step's value/key */` |
|       - | 1478 | `	SyToken *pStart,   /* Target expression token range */` |
|       - | 1479 | `	SyToken *pEnd,` |
|       - | 1480 | `	int bRef           /* True for a by-reference value target */` |
|       - | 1481 | `	)` |
|       5 | 1482 | `{` |
|     121 | 1483 | `	SyToken *pSavedIn = pGen->pIn;` |
|     121 | 1484 | `	SyToken *pSavedEnd = pGen->pEnd;` |
|     121 | 1485 | `	sxi32 iVmOp = bRef ? PH7_OP_STORE_REF : PH7_OP_STORE;` |
|       - | 1486 | `	VmInstr *pInstr;` |
|     121 | 1487 | `	sxi32 iP1 = 0;` |
|     121 | 1488 | `	sxi32 iP2 = 0;` |
|     121 | 1489 | `	void *p3 = 0;` |
|       - | 1490 | `	sxi32 rc;` |
|       - | 1491 | `	/* The value being stored, below the target — the operand order OP_STORE expects. */` |
|     121 | 1492 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(pTemp),0);` |
|     121 | 1493 | `	pGen->pIn = pStart;` |
|     121 | 1494 | `	pGen->pEnd = pEnd;` |
|     121 | 1495 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE,` |
|       - | 1496 | `		GenStateForEachNodeValidator);` |
|     121 | 1497 | `	pGen->pIn = pSavedIn;` |
|     121 | 1498 | `	pGen->pEnd = pSavedEnd;` |
|     121 | 1499 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 1500 | `		return SXERR_ABORT;` |
|     121 | 1501 | `	}else if( rc != SXRET_OK ){` |
|       - | 1502 | `		/* The validator already reported it; drop the pushed value and carry on. */` |
|     ! 0 | 1503 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 | 1504 | `		return SXRET_OK;` |
|       - | 1505 | `	}` |
|     121 | 1506 | `	pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     121 | 1507 | `	if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 1508 | `		/* A member target resolves (and, for a reference, stashes) its own slot. */` |
|      22 | 1509 | `		if( bRef ){` |
|       3 | 1510 | `			pInstr->iP2 = PH7_MEMBER_REF_TARGET;` |
|       1 | 1511 | `		}` |
|      22 | 1512 | `		iP2 = 1;` |
|     111 | 1513 | `	}else if( pInstr ){` |
|     101 | 1514 | `		(void)PH7_VmPopInstr(pGen->pVm);` |
|     101 | 1515 | `		if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|      19 | 1516 | `			iVmOp = bRef ? PH7_OP_STORE_IDX_REF : PH7_OP_STORE_IDX;` |
|      19 | 1517 | `			iP1 = pInstr->iP1;` |
|      19 | 1518 | `			if( bRef ){` |
|       3 | 1519 | `				iP2 = pInstr->iP2;` |
|       3 | 1520 | `				p3 = pInstr->p3;` |
|       1 | 1521 | `			}` |
|      10 | 1522 | `		}else{` |
|      83 | 1523 | `			p3 = pInstr->p3;` |
|       - | 1524 | `		}` |
|      48 | 1525 | `	}` |
|     121 | 1526 | `	PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|       - | 1527 | `	/* Discard the stored value the store leaves behind */` |
|     121 | 1528 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     121 | 1529 | `	if( bRef ){` |
|       - | 1530 | `		/* The target now holds the element; drop the temporary's own hold, or it` |
|       - | 1531 | `		 * would keep the element a REFERENCE for the rest of the script — an extra` |
|       - | 1532 | ``		 * holder no `unset()` the program can write is able to reach. Dropping the`` |
|       - | 1533 | `		 * NAME never releases the slot the target still refers to. */` |
|       5 | 1534 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UNSET_VAR,0,0,(void *)pTemp,0);` |
|       2 | 1535 | `	}` |
|     121 | 1536 | `	return SXRET_OK;` |
|      63 | 1537 | `}` |
|       - | 1538 | `/*` |
|       - | 1539 | ` * Compile the 'foreach' statement.` |
|       - | 1540 | ` * According to the PHP language reference` |
|       - | 1541 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|       - | 1542 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|       - | 1543 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|       - | 1544 | ` *  is a minor but useful extension of the first:` |
|       - | 1545 | ` *  foreach (array_expression as $value)` |
|       - | 1546 | ` *    statement` |
|       - | 1547 | ` *  foreach (array_expression as $key => $value)` |
|       - | 1548 | ` *   statement` |
|       - | 1549 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|       - | 1550 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|       - | 1551 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|       - | 1552 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|       - | 1553 | ` *  to the variable $key on each loop.` |
|       - | 1554 | ` *  Note:` |
|       - | 1555 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|       - | 1556 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|       - | 1557 | ` *  Note:` |
|       - | 1558 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|       - | 1559 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|       - | 1560 | ` *  or after the foreach without resetting it.` |
|       - | 1561 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|       - | 1562 | ` *  of copying the value.` |
|       - | 1563 | ` */` |
|   71297 | 1564 | `PH7_PRIVATE sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       5 | 1565 | `{` |
|   71302 | 1566 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|   71302 | 1567 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|       - | 1568 | ``	/* Token ranges of a KEY / VALUE target that is not a plain `$name`: it is`` |
|       - | 1569 | `	 * compiled as a real store at the top of the loop body (php assigns the value` |
|       - | 1570 | `	 * first, then the key), against a synthetic temporary the step writes. */` |
|   71302 | 1571 | `	SyToken *pKeyStart = 0,*pKeyEnd = 0;` |
|   71302 | 1572 | `	SyToken *pValStart = 0,*pValEnd = 0;` |
|   71302 | 1573 | `	GenBlock *pForeachBlock = 0;` |
|       - | 1574 | `	ph7_foreach_info *pInfo;` |
|       - | 1575 | `	sxu32 nFalseJump;` |
|   71302 | 1576 | `	sxu32 nContainerEnd = 0; /* instruction count just after the iterated expression */` |
|       - | 1577 | `	VmInstr *pInstr;` |
|       - | 1578 | `	sxu32 nLine;` |
|       - | 1579 | `	sxi32 rc;` |
|   71302 | 1580 | `	nLine = pGen->pIn->nLine;` |
|       - | 1581 | `	/* Jump the 'foreach' keyword */` |
|   71302 | 1582 | `	pGen->pIn++;` |
|   71302 | 1583 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 1584 | `		/* Syntax error */` |
|     ! 0 | 1585 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 | 1586 | `		if( rc == SXERR_ABORT ){` |
|       - | 1587 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1588 | `			return SXERR_ABORT;` |
|       - | 1589 | `		}` |
|     ! 0 | 1590 | `		goto Synchronize;` |
|       - | 1591 | `	}` |
|       - | 1592 | `	/* Jump the left parenthesis '(' */` |
|   71302 | 1593 | `	pGen->pIn++;` |
|       - | 1594 | `	/* Create the loop block */` |
|   71302 | 1595 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|   71302 | 1596 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1597 | `		return SXERR_ABORT;` |
|       - | 1598 | `	}` |
|       - | 1599 | `	/* Delimit the expression */` |
|   71302 | 1600 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   71302 | 1601 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 1602 | `		/* Empty expression */` |
|     ! 0 | 1603 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|     ! 0 | 1604 | `		if( rc == SXERR_ABORT ){` |
|       - | 1605 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1606 | `			return SXERR_ABORT;` |
|       - | 1607 | `		}` |
|       - | 1608 | `		/* Synchronize */` |
|     ! 0 | 1609 | `		pGen->pIn = pEnd;` |
|     ! 0 | 1610 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 1611 | `			pGen->pIn++;` |
|     ! 0 | 1612 | `		}` |
|     ! 0 | 1613 | `		return SXRET_OK;` |
|       - | 1614 | `	}` |
|       - | 1615 | `	/* Compile the array expression.` |
|       - | 1616 | `	 *` |
|       - | 1617 | ``	 * The separator is the first TOP-LEVEL `as`: one nested inside brackets, parens`` |
|       - | 1618 | `	 * or braces belongs to something else the iterated expression contains — a` |
|       - | 1619 | ``	 * closure with a `foreach` of its own is the shape that finds this, and cutting`` |
|       - | 1620 | ``	 * at its inner `as` left the outer expression with an unclosed bracket and made`` |
|       - | 1621 | ``	 * `foreach ([function(){ foreach ([1] as $k) … }] as $f)` a compile fatal on`` |
|       - | 1622 | ``	 * source php runs. An `as` that is part of a NAME ($as, $o->as, A::as) is not a`` |
|       - | 1623 | `	 * separator either. */` |
|   71302 | 1624 | `	pCur = pGen->pIn;` |
|       - | 1625 | `	{` |
|   71302 | 1626 | `		sxi32 iNest = 0;` |
|  537063 | 1627 | `		while( pCur < pEnd ){` |
|  537063 | 1628 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|   46108 | 1629 | `				iNest++;` |
|  513973 | 1630 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       - | 1631 | `				/* A mismatch here is the expression parser's to report. */` |
|   46108 | 1632 | `				iNest--;` |
|  467870 | 1633 | `			}else if( iNest <= 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|   84802 | 1634 | `				sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|   84802 | 1635 | `				if( nKeywrd == PH7_TKWRD_AS && !GenStateKeywordIsName(pGen->pIn,pCur) ){` |
|   71302 | 1636 | `					break;` |
|       - | 1637 | `				}` |
|    6741 | 1638 | `			}` |
|       - | 1639 | `			/* Advance the stream cursor */` |
|  465766 | 1640 | `			pCur++;` |
|       5 | 1641 | `		}` |
|       - | 1642 | `	}` |
|   71302 | 1643 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 | 1644 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - | 1645 | `			"foreach: Missing array/object expression");` |
|     ! 0 | 1646 | `		if( rc == SXERR_ABORT ){` |
|       - | 1647 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 1648 | `			return SXERR_ABORT;` |
|       - | 1649 | `		}` |
|     ! 0 | 1650 | `		goto Synchronize;` |
|       - | 1651 | `	}` |
|       - | 1652 | `	/* Swap token streams */` |
|   71302 | 1653 | `	pTmp = pGen->pEnd;` |
|   71302 | 1654 | `	pGen->pEnd = pCur;` |
|   71302 | 1655 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   71302 | 1656 | `	if( rc == SXERR_ABORT ){` |
|       - | 1657 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 1658 | `		return SXERR_ABORT;` |
|       - | 1659 | `	}` |
|       - | 1660 | `	/* Remember where the iterated expression ends: whether it was fetched for` |
|       - | 1661 | ``	 * WRITING is only known once the `&` after `as` has been read, and a`` |
|       - | 1662 | `	 * by-reference walk fetches its container the way a reference bind does` |
|       - | 1663 | `	 * (php's BP_VAR_W) -- so a missing property is created, not warned about. */` |
|   71302 | 1664 | `	nContainerEnd = PH7_VmInstrLength(pGen->pVm);` |
|       - | 1665 | `	/* Update token stream */` |
|   71302 | 1666 | `	while(pGen->pIn < pCur ){` |
|     ! 0 | 1667 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 1668 | `		if( rc == SXERR_ABORT ){` |
|       - | 1669 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 1670 | `			return SXERR_ABORT;` |
|       - | 1671 | `		}` |
|     ! 0 | 1672 | `		pGen->pIn++;` |
|     ! 0 | 1673 | `	}` |
|   71302 | 1674 | `	pCur++; /* Jump the 'as' keyword */` |
|   71302 | 1675 | `	pGen->pIn = pCur;` |
|   71302 | 1676 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 1677 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 | 1678 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1679 | `			return SXERR_ABORT;` |
|       - | 1680 | `		}` |
|     ! 0 | 1681 | `	}` |
|       - | 1682 | `	/* Create the foreach context */` |
|   71302 | 1683 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|   71302 | 1684 | `	if( pInfo == 0 ){` |
|     ! 0 | 1685 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 1686 | `		return SXERR_ABORT;` |
|       - | 1687 | `	}` |
|       - | 1688 | `	/* Zero the structure */` |
|   71302 | 1689 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - | 1690 | `	/* Initialize structure fields */` |
|   71302 | 1691 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - | 1692 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|       - | 1693 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|       - | 1694 | `	 * '=>'. */` |
|   71302 | 1695 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|   71302 | 1696 | `	if( pCur < pEnd ){` |
|       - | 1697 | `		/* Compile the expression holding the key name */` |
|   14297 | 1698 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 | 1699 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 | 1700 | `			if( rc == SXERR_ABORT ){` |
|       - | 1701 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 1702 | `				return SXERR_ABORT;` |
|     ! 0 | 1703 | `			}` |
|   14297 | 1704 | `		}else if( !GenStateForeachTargetIsPlainVar(pGen->pIn,pCur) ){` |
|       - | 1705 | `			/* A writable but non-name key target ($o->k, C::$s, $a['k'], $$n): the` |
|       - | 1706 | `			 * step lands in a temporary and the store runs in the loop body. */` |
|      13 | 1707 | `			pKeyStart = pGen->pIn;` |
|      13 | 1708 | `			pKeyEnd = pCur;` |
|      13 | 1709 | `			if( GenStateForeachTempName(&(*pGen),"key",&pInfo->sKey) != SXRET_OK ){` |
|     ! 0 | 1710 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1711 | `				return SXERR_ABORT;` |
|       - | 1712 | `			}` |
|      13 | 1713 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       7 | 1714 | `		}else{` |
|   14285 | 1715 | `			pGen->pEnd = pCur;` |
|   14285 | 1716 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|   14285 | 1717 | `			if( rc == SXERR_ABORT ){` |
|       - | 1718 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 1719 | `				return SXERR_ABORT;` |
|       - | 1720 | `			}` |
|   14285 | 1721 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|   14285 | 1722 | `			if( pInstr->p3 ){` |
|       - | 1723 | `				/* Record key name */` |
|   14285 | 1724 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    7128 | 1725 | `			}` |
|   14285 | 1726 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - | 1727 | `		}` |
|   14297 | 1728 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    7134 | 1729 | `	}` |
|   71302 | 1730 | `	pGen->pEnd = pEnd;` |
|   71302 | 1731 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 1732 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 | 1733 | `		if( rc == SXERR_ABORT ){` |
|       - | 1734 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 1735 | `			return SXERR_ABORT;` |
|       - | 1736 | `		}` |
|     ! 0 | 1737 | `		goto Synchronize;` |
|       - | 1738 | `	}` |
|   71302 | 1739 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      88 | 1740 | `		pGen->pIn++;` |
|       - | 1741 | `		/* Pass by reference  */` |
|      88 | 1742 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|      42 | 1743 | `	}` |
|       - | 1744 | `	/* Check if the value target is list() */` |
|   71302 | 1745 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 | 1746 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - | 1747 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|       - | 1748 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|       - | 1749 | `		 */` |
|       - | 1750 | `		static int iForeachListCnt = 0;` |
|       - | 1751 | `		char zTmp[128];` |
|       - | 1752 | `		sxu32 nLen;` |
|       - | 1753 | `		char *zDup;` |
|      10 | 1754 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|      10 | 1755 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      10 | 1756 | `		if( zDup == 0 ){` |
|     ! 0 | 1757 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1758 | `			return SXERR_ABORT;` |
|       - | 1759 | `		}` |
|      10 | 1760 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - | 1761 | `		/* Save list() token boundaries */` |
|      10 | 1762 | `		pListStart = pGen->pIn;` |
|       - | 1763 | `		/* Advance past list(...) — validate parentheses */` |
|      10 | 1764 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|      10 | 1765 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 1766 | `			/* php: syntax error, unexpected variable "$x", expecting "(" */` |
|       3 | 1767 | `			rc = PH7_GenSyntaxError(pGen,pGen->pIn < pEnd ? pGen->pIn : 0,"\"(\"");` |
|       3 | 1768 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1769 | `				return SXERR_ABORT;` |
|       - | 1770 | `			}` |
|       3 | 1771 | `			goto Synchronize;` |
|       - | 1772 | `		}` |
|       7 | 1773 | `		pGen->pIn++; /* Jump '(' */` |
|       7 | 1774 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       7 | 1775 | `		if( pListEnd >= pEnd ){` |
|     ! 0 | 1776 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 1777 | `				"foreach: Missing closing ')' after list");` |
|     ! 0 | 1778 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1779 | `				return SXERR_ABORT;` |
|       - | 1780 | `			}` |
|     ! 0 | 1781 | `			goto Synchronize;` |
|       - | 1782 | `		}` |
|       7 | 1783 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|       7 | 1784 | `		pListEnd = pGen->pIn;` |
|       7 | 1785 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|   71297 | 1786 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - | 1787 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|       - | 1788 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|       - | 1789 | `		 */` |
|       - | 1790 | `		static int iForeachShortListCnt = 0;` |
|       - | 1791 | `		char zTmp[128];` |
|       - | 1792 | `		sxu32 nLen;` |
|       - | 1793 | `		char *zDup;` |
|     223 | 1794 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|     223 | 1795 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|     223 | 1796 | `		if( zDup == 0 ){` |
|     ! 0 | 1797 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1798 | `			return SXERR_ABORT;` |
|       - | 1799 | `		}` |
|     223 | 1800 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - | 1801 | `		/* Save [...] token boundaries */` |
|     223 | 1802 | `		pListStart = pGen->pIn;` |
|       - | 1803 | `		/* Advance past [...] */` |
|     223 | 1804 | `		pGen->pIn++; /* Jump '[' */` |
|     223 | 1805 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|     223 | 1806 | `		if( pListEnd >= pEnd ){` |
|     ! 0 | 1807 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 1808 | `				"foreach: Missing closing ']' after short list");` |
|     ! 0 | 1809 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1810 | `				return SXERR_ABORT;` |
|       - | 1811 | `			}` |
|     ! 0 | 1812 | `			goto Synchronize;` |
|       - | 1813 | `		}` |
|     223 | 1814 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|     223 | 1815 | `		pListEnd = pGen->pIn;` |
|     223 | 1816 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|   71184 | 1817 | `	}else if( !GenStateForeachTargetIsPlainVar(pGen->pIn,pEnd) ){` |
|       - | 1818 | `		/* A writable but non-name value target — same treatment as the key above. */` |
|     109 | 1819 | `		pValStart = pGen->pIn;` |
|     109 | 1820 | `		pValEnd = pEnd;` |
|     109 | 1821 | `		if( GenStateForeachTempName(&(*pGen),"val",&pInfo->sValue) != SXRET_OK ){` |
|     ! 0 | 1822 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1823 | `			return SXERR_ABORT;` |
|       - | 1824 | `		}` |
|      57 | 1825 | `	}else{` |
|       - | 1826 | `		/* Compile the expression holding the value name */` |
|   70971 | 1827 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|   70971 | 1828 | `		if( rc == SXERR_ABORT ){` |
|       - | 1829 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 1830 | `			return SXERR_ABORT;` |
|       - | 1831 | `		}` |
|   70971 | 1832 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|   70971 | 1833 | `		if( pInstr->p3 ){` |
|       - | 1834 | `			/* Record value name */` |
|   70971 | 1835 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|   35427 | 1836 | `		}` |
|       - | 1837 | `	}` |
|   71295 | 1838 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd` |
|     230 | 1839 | `	 && PH7_GenStateListSpanHasRef(pListStart,pListEnd) ){` |
|       - | 1840 | `		/* The list binds by reference, so this step's value has to BE the array` |
|       - | 1841 | `		 * element rather than a copy of it. */` |
|       3 | 1842 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       1 | 1843 | `	}` |
|   71300 | 1844 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_REF) && nContainerEnd > 0 ){` |
|      90 | 1845 | `		VmInstr *pContainer = PH7_VmGetInstr(pGen->pVm,nContainerEnd - 1);` |
|      86 | 1846 | `		if( pContainer && pContainer->iOp == PH7_OP_MEMBER` |
|      57 | 1847 | `		 && pContainer->iP2 == PH7_MEMBER_READ ){` |
|      22 | 1848 | `			pContainer->bRefSrc = 1;` |
|      10 | 1849 | `		}` |
|      43 | 1850 | `	}` |
|       - | 1851 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|   71300 | 1852 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - | 1853 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   71300 | 1854 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - | 1855 | `	/* Record the first instruction to execute */` |
|   71300 | 1856 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - | 1857 | `	/* Emit the FOREACH_STEP instruction */` |
|   71300 | 1858 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - | 1859 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   71300 | 1860 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - | 1861 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|   71300 | 1862 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|       - | 1863 | `		SyToken *pSavedIn,*pSavedEnd;` |
|       - | 1864 | `		/* Load the temporary variable holding the current value onto the stack.` |
|       - | 1865 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|       - | 1866 | `		 */` |
|     229 | 1867 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|       - | 1868 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|       - | 1869 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|       - | 1870 | `		 * picks up the delimiter and the variable names inside.` |
|       - | 1871 | `		 */` |
|     229 | 1872 | `		pSavedIn = pGen->pIn;` |
|     229 | 1873 | `		pSavedEnd = pGen->pEnd;` |
|     229 | 1874 | `		pGen->pIn = pListStart;` |
|     229 | 1875 | `		pGen->pEnd = pListEnd;` |
|     229 | 1876 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|     223 | 1877 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|     113 | 1878 | `		}else{` |
|       7 | 1879 | `			rc = PH7_CompileList(&(*pGen),0);` |
|       - | 1880 | `		}` |
|     229 | 1881 | `		pGen->pIn = pSavedIn;` |
|     229 | 1882 | `		pGen->pEnd = pSavedEnd;` |
|     229 | 1883 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1884 | `			return SXERR_ABORT;` |
|       - | 1885 | `		}` |
|       - | 1886 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|     229 | 1887 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     229 | 1888 | `		if( pInfo->iFlags & PH7_4EACH_STEP_REF ){` |
|       - | 1889 | `			/* The bind is done; drop the STEP's own hold on the element, exactly as` |
|       - | 1890 | `			 * the non-list by-ref target does. Left in place it would keep the row a` |
|       - | 1891 | `			 * REFERENCE for the rest of the script -- php marks the ELEMENT the` |
|       - | 1892 | `			 * target still aliases, never the row. */` |
|       3 | 1893 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_UNSET_VAR,0,0,(void *)&pInfo->sValue,0);` |
|       1 | 1894 | `		}` |
|     112 | 1895 | `	}` |
|       - | 1896 | `	/* Store this step's value and key into their non-name targets. php performs the` |
|       - | 1897 | `	 * VALUE assignment first — visible through a __set() pair, and the order the` |
|       - | 1898 | `	 * symbol table records the two locals in for the plain-name shape. */` |
|   71300 | 1899 | `	if( pValStart ){` |
|     161 | 1900 | `		rc = GenStateForeachStoreTarget(&(*pGen),&pInfo->sValue,pValStart,pValEnd,` |
|     104 | 1901 | `			(pInfo->iFlags & PH7_4EACH_STEP_REF) != 0);` |
|     109 | 1902 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1903 | `			return SXERR_ABORT;` |
|       - | 1904 | `		}` |
|      52 | 1905 | `	}` |
|   71300 | 1906 | `	if( pKeyStart ){` |
|      13 | 1907 | `		rc = GenStateForeachStoreTarget(&(*pGen),&pInfo->sKey,pKeyStart,pKeyEnd,0);` |
|      13 | 1908 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1909 | `			return SXERR_ABORT;` |
|       - | 1910 | `		}` |
|       6 | 1911 | `	}` |
|       - | 1912 | `	/* Compile the loop body */` |
|   71300 | 1913 | `	pGen->pIn = &pEnd[1];` |
|   71300 | 1914 | `	pGen->pEnd = pTmp;` |
|   71300 | 1915 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|   71300 | 1916 | `	if( rc == SXERR_ABORT ){` |
|       - | 1917 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 1918 | `		return SXERR_ABORT;` |
|       - | 1919 | `	}` |
|       - | 1920 | `	/* Emit the unconditional jump to the start of the loop */` |
|   71300 | 1921 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - | 1922 | `	/* Fix all jumps now the destination is resolved */` |
|   71300 | 1923 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 1924 | `	/* Release the loop block */` |
|   71300 | 1925 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 1926 | `	/* Statement successfully compiled */` |
|   71300 | 1927 | `	return SXRET_OK;` |
|       1 | 1928 | `Synchronize:` |
|       - | 1929 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 1930 | `	 * compiling this erroneous block.` |
|       - | 1931 | `	 */` |
|       3 | 1932 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 1933 | `		pGen->pIn++;` |
|     ! 0 | 1934 | `	}` |
|       3 | 1935 | `	return SXRET_OK;` |
|   35597 | 1936 | `}` |
|       - | 1937 | `/*` |
|       - | 1938 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - | 1939 | ` * According to the PHP language reference` |
|       - | 1940 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - | 1941 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - | 1942 | ` *  that is similar to that of C:` |
|       - | 1943 | ` *  if (expr)` |
|       - | 1944 | ` *   statement` |
|       - | 1945 | ` *  else construct:` |
|       - | 1946 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - | 1947 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - | 1948 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - | 1949 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - | 1950 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - | 1951 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - | 1952 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - | 1953 | ` *  elseif` |
|       - | 1954 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - | 1955 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - | 1956 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - | 1957 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - | 1958 | ` *   than b, a equal to b or a is smaller than b:` |
|       - | 1959 | ` *   <?php` |
|       - | 1960 | ` *    if ($a > $b) {` |
|       - | 1961 | ` *     echo "a is bigger than b";` |
|       - | 1962 | ` *    } elseif ($a == $b) {` |
|       - | 1963 | ` *     echo "a is equal to b";` |
|       - | 1964 | ` *    } else {` |
|       - | 1965 | ` *     echo "a is smaller than b";` |
|       - | 1966 | ` *    }` |
|       - | 1967 | ` *    ?>` |
|       - | 1968 | ` */` |
|  460625 | 1969 | `PH7_PRIVATE sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       5 | 1970 | `{` |
|  460630 | 1971 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  460630 | 1972 | `	GenBlock *pCondBlock = 0;` |
|       - | 1973 | `	sxu32 nJumpIdx;` |
|       - | 1974 | `	sxu32 nKeyID;` |
|       - | 1975 | `	sxi32 rc;` |
|       - | 1976 | `	/* Jump the 'if' keyword */` |
|  460630 | 1977 | `	pGen->pIn++;` |
|  460630 | 1978 | `	pToken = pGen->pIn;` |
|       - | 1979 | `	/* Create the conditional block */` |
|  460630 | 1980 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  460630 | 1981 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1982 | `		return SXERR_ABORT;` |
|       - | 1983 | `	}` |
|       - | 1984 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|  256983 | 1985 | `	for(;;){` |
|  514660 | 1986 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 1987 | `			/* Syntax error */` |
|     ! 0 | 1988 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 1989 | `				pToken--;` |
|     ! 0 | 1990 | `			}` |
|     ! 0 | 1991 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 | 1992 | `			if( rc == SXERR_ABORT ){` |
|       - | 1993 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 1994 | `				return SXERR_ABORT;` |
|       - | 1995 | `			}` |
|     ! 0 | 1996 | `			goto Synchronize;` |
|       - | 1997 | `		}` |
|       - | 1998 | `		/* Jump the left parenthesis '(' */` |
|  514660 | 1999 | `		pToken++;` |
|       - | 2000 | `		/* Delimit the condition */` |
|  514660 | 2001 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  514660 | 2002 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - | 2003 | `			/* Syntax error */` |
|     ! 0 | 2004 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 2005 | `				pToken--;` |
|     ! 0 | 2006 | `			}` |
|     ! 0 | 2007 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 | 2008 | `			if( rc == SXERR_ABORT ){` |
|       - | 2009 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2010 | `				return SXERR_ABORT;` |
|       - | 2011 | `			}` |
|     ! 0 | 2012 | `			goto Synchronize;` |
|       - | 2013 | `		}` |
|       - | 2014 | `		/* Swap token streams */` |
|  514660 | 2015 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - | 2016 | `		/* Compile the condition */` |
|  514660 | 2017 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 2018 | `		/* Update token stream */` |
|  514660 | 2019 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 | 2020 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|     ! 0 | 2021 | `			pGen->pIn++;` |
|     ! 0 | 2022 | `		}` |
|  514660 | 2023 | `		pGen->pIn  = &pEnd[1];` |
|  514660 | 2024 | `		pGen->pEnd = pTmp;` |
|  514660 | 2025 | `		if( rc == SXERR_ABORT ){` |
|       - | 2026 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       3 | 2027 | `			return SXERR_ABORT;` |
|       - | 2028 | `		}` |
|       - | 2029 | `		/* Emit the false jump */` |
|  514658 | 2030 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - | 2031 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  514658 | 2032 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - | 2033 | `		/* Compile the body */` |
|  514658 | 2034 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  514658 | 2035 | `		if( rc == SXERR_ABORT ){` |
|       3 | 2036 | `			return SXERR_ABORT;` |
|       - | 2037 | `		}` |
|  514656 | 2038 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|  111846 | 2039 | `			break;` |
|       - | 2040 | `		}` |
|       - | 2041 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|  290673 | 2042 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  290673 | 2043 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|  189098 | 2044 | `			break;` |
|       - | 2045 | `		}` |
|       - | 2046 | `		/* Emit the unconditional jump */` |
|  101580 | 2047 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - | 2048 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  101580 | 2049 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|  101580 | 2050 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   67713 | 2051 | `			pToken = &pGen->pIn[1];` |
|   67713 | 2052 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|   20201 | 2053 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|   23746 | 2054 | `					break;` |
|       - | 2055 | `			}` |
|   20168 | 2056 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|   10068 | 2057 | `		}` |
|   54035 | 2058 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - | 2059 | `		/* Synchronize cursors */` |
|   54035 | 2060 | `		pToken = pGen->pIn;` |
|       - | 2061 | `		/* Fix the false jump */` |
|   54035 | 2062 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       5 | 2063 | `	} /* For(;;) */` |
|       - | 2064 | `	/* Fix the false jump */` |
|  460626 | 2065 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  460626 | 2066 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|  236638 | 2067 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - | 2068 | `			/* Compile the else block */` |
|   47550 | 2069 | `			pGen->pIn++;` |
|   47550 | 2070 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   47550 | 2071 | `			if( rc == SXERR_ABORT ){` |
|       - | 2072 |  |
|     ! 0 | 2073 | `				return SXERR_ABORT;` |
|       - | 2074 | `			}` |
|   23741 | 2075 | `	}` |
|  460626 | 2076 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - | 2077 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  460626 | 2078 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - | 2079 | `	/* Release the conditional block */` |
|  460626 | 2080 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2081 | `	/* Statement successfully compiled */` |
|  460626 | 2082 | `	return SXRET_OK;` |
|     ! 0 | 2083 | `Synchronize:` |
|       - | 2084 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - | 2085 | `	 */` |
|     ! 0 | 2086 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 2087 | `		pGen->pIn++;` |
|     ! 0 | 2088 | `	}` |
|     ! 0 | 2089 | `	return SXRET_OK;` |
|  230009 | 2090 | `}` |
|       - | 2091 | `/*` |
|       - | 2092 | ` * Compile the global construct.` |
|       - | 2093 | ` * According to the PHP language reference` |
|       - | 2094 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - | 2095 | ` *  to be used in that function.` |
|       - | 2096 | ` *  Example #1 Using global` |
|       - | 2097 | ` *  <?php` |
|       - | 2098 | ` *   $a = 1;` |
|       - | 2099 | ` *   $b = 2;` |
|       - | 2100 | ` *   function Sum()` |
|       - | 2101 | ` *   {` |
|       - | 2102 | ` *    global $a, $b;` |
|       - | 2103 | ` *    $b = $a + $b;` |
|       - | 2104 | ` *   }` |
|       - | 2105 | ` *   Sum();` |
|       - | 2106 | ` *   echo $b;` |
|       - | 2107 | ` *  ?>` |
|       - | 2108 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - | 2109 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - | 2110 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - | 2111 | ` */` |
|     125 | 2112 | `PH7_PRIVATE sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       5 | 2113 | `{` |
|     130 | 2114 | `	SyToken *pTmp,*pNext = 0;` |
|       - | 2115 | `	sxi32 nExpr;` |
|       - | 2116 | `	sxi32 rc;` |
|       - | 2117 | `	/* Jump the 'global' keyword */` |
|     130 | 2118 | `	pGen->pIn++;` |
|     130 | 2119 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - | 2120 | `		/* Nothing to process */` |
|     ! 0 | 2121 | `		return SXRET_OK;` |
|       - | 2122 | `	}` |
|     130 | 2123 | `	pTmp = pGen->pEnd;` |
|     130 | 2124 | `	nExpr = 0;` |
|     287 | 2125 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|     162 | 2126 | `		if( pGen->pIn < pNext ){` |
|     162 | 2127 | `			pGen->pEnd = pNext;` |
|     162 | 2128 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 2129 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 | 2130 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 2131 | `					return SXERR_ABORT;` |
|       - | 2132 | `				}` |
|     157 | 2133 | `			}else if( &pGen->pIn[1] < pGen->pEnd` |
|     157 | 2134 | `			 && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|     152 | 2135 | `			 && pGen->pIn[1].sData.nByte == sizeof("this")-1` |
|      85 | 2136 | `			 && SyMemcmp((const void *)pGen->pIn[1].sData.zString,` |
|       7 | 2137 | `			             (const void *)"this",sizeof("this")-1) == 0 ){` |
|       - | 2138 | ``				/* php refuses `global $this;` at the declaration. */`` |
|       3 | 2139 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - | 2140 | `					"Cannot use $this as global variable");` |
|       3 | 2141 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 2142 | `					return SXERR_ABORT;` |
|       - | 2143 | `				}` |
|       2 | 2144 | `			}else{` |
|     160 | 2145 | `				pGen->pIn++;` |
|     160 | 2146 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - | 2147 | `					/* Emit a warning */` |
|     ! 0 | 2148 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 | 2149 | `				}else{` |
|     160 | 2150 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     160 | 2151 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 2152 | `						return SXERR_ABORT;` |
|     160 | 2153 | `					}else if(rc != SXERR_EMPTY ){` |
|     160 | 2154 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|     160 | 2155 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|       - | 2156 | `							/* Variable name, not a constant */` |
|     150 | 2157 | `							pLast->iP1 = 0;` |
|      72 | 2158 | `						}` |
|     160 | 2159 | `						nExpr++;` |
|      77 | 2160 | `					}` |
|       - | 2161 | `				}` |
|       - | 2162 | `			}` |
|      78 | 2163 | `		}` |
|       - | 2164 | `		/* Next expression in the stream */` |
|     162 | 2165 | `		pGen->pIn = pNext;` |
|       - | 2166 | `		/* Jump trailing commas */` |
|     194 | 2167 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|      37 | 2168 | `			pGen->pIn++;` |
|       5 | 2169 | `		}` |
|       5 | 2170 | `	}` |
|       - | 2171 | `	/* Restore token stream */` |
|     130 | 2172 | `	pGen->pEnd = pTmp;` |
|     130 | 2173 | `	if( nExpr > 0 ){` |
|       - | 2174 | `		/* Emit the uplink instruction */` |
|     128 | 2175 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|      61 | 2176 | `	}` |
|     130 | 2177 | `	return SXRET_OK;` |
|      67 | 2178 | `}` |
|       - | 2179 | `/*` |
|       - | 2180 | ` * php's NOUN for a compile-time return diagnostic is the LEXICAL scope, not the` |
|       - | 2181 | `` * kind of function the `return` sits in: zend reads CG(active_class_entry), so a`` |
|       - | 2182 | ` * closure written inside a class body reports "method" and the very same closure` |
|       - | 2183 | ` * written at file scope reports "function". pCurClass is the compiler's exact` |
|       - | 2184 | ` * counterpart (the class/interface/trait/enum whose BODY is being compiled).` |
|       - | 2185 | ` */` |
|      26 | 2186 | `static const char * GenStateReturnNoun(ph7_gen_state *pGen)` |
|       4 | 2187 | `{` |
|      30 | 2188 | `	return pGen->pCurClass ? "method" : "function";` |
|       4 | 2189 | `}` |
|       - | 2190 | `/*` |
|       - | 2191 | ` * TRUE when a declared return type ACCEPTS null — the condition under which php` |
|       - | 2192 | `` * appends its `did you mean "return null;"` hint to the missing-value error.`` |
|       - | 2193 | ``  * That is every nullable declaration (`?T`, `T\|null`, and the standalone `null` `` |
|       - | 2194 | `` * type, all of which set VM_FUNC_RETURN_NULLABLE) plus `mixed`, which includes`` |
|       - | 2195 | ` * null but is stored as a pseudo-CLASS atom rather than through the flag.` |
|       - | 2196 | ` */` |
|      10 | 2197 | `static int GenStateReturnTypeAllowsNull(ph7_vm_func *pFunc)` |
|       4 | 2198 | `{` |
|       - | 2199 | `	SyString *pCls;` |
|      14 | 2200 | `	if( pFunc->iFlags & VM_FUNC_RETURN_NULLABLE ){` |
|       3 | 2201 | `		return 1;` |
|       - | 2202 | `	}` |
|      11 | 2203 | `	pCls = &pFunc->sReturnClass;` |
|       8 | 2204 | `	if( pFunc->nReturnType == SXU32_HIGH && pCls->nByte == sizeof("mixed")-1` |
|       3 | 2205 | `	 && SyStrnicmp(pCls->zString,"mixed",sizeof("mixed")-1) == 0 ){` |
|     ! 0 | 2206 | `		return 1;` |
|       - | 2207 | `	}` |
|      11 | 2208 | `	return 0;` |
|       9 | 2209 | `}` |
|       - | 2210 | `/*` |
|       - | 2211 | ` * Compile the return statement.` |
|       - | 2212 | ` * According to the PHP language reference` |
|       - | 2213 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - | 2214 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - | 2215 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - | 2216 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - | 2217 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - | 2218 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - | 2219 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - | 2220 | ` *  from within the main script file, then script execution end.` |
|       - | 2221 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - | 2222 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - | 2223 | ` *  should do so as PHP has less work to do in this case.` |
|       - | 2224 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - | 2225 | ` */` |
|  295636 | 2226 | `PH7_PRIVATE sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       5 | 2227 | `{` |
|  295641 | 2228 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - | 2229 | `	sxi32 rc;` |
|  295641 | 2230 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  295641 | 2231 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|       - | 2232 | `	ph7_vm_func *pFunc;` |
|       - | 2233 | `	sxu32 nInstrBefore;` |
|       - | 2234 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|       - | 2235 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|       - | 2236 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|       - | 2237 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|       - | 2238 | `	 * normally below so token processing stays consistent. */` |
|  970251 | 2239 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|  674615 | 2240 | `		pFuncBlock = pFuncBlock->pParent;` |
|       5 | 2241 | `	}` |
|  295641 | 2242 | `	pFunc = pFuncBlock ? (ph7_vm_func *)pFuncBlock->pUserData : 0;` |
|  295641 | 2243 | `	if( pFunc && pFunc->nReturnType == MEMOBJ_NEVER ){` |
|       8 | 2244 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       2 | 2245 | `			"A never-returning %s must not return", GenStateReturnNoun(pGen));` |
|       6 | 2246 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2247 | `			return SXERR_ABORT;` |
|       - | 2248 | `		}` |
|       2 | 2249 | `	}` |
|       - | 2250 | `	/* Jump the 'return' keyword */` |
|  295641 | 2251 | `	pGen->pIn++;` |
|  295641 | 2252 | `	nInstrBefore = PH7_VmInstrLength(pGen->pVm);` |
|  295641 | 2253 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_VOID_CAST) ){` |
|       - | 2254 | ``		/* php's `(void)` cast is a STATEMENT prefix, so `return (void) f();` is a`` |
|       - | 2255 | ``		 * parse error there — and the expected-token set is the one `return` has,`` |
|       - | 2256 | ``		 * which is why php names `";"` here and nothing after `$x = (void)`. */`` |
|       3 | 2257 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|       - | 2258 | `			"syntax error, unexpected token \"(void)\", expecting \";\"");` |
|       3 | 2259 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2260 | `			return SXERR_ABORT;` |
|       - | 2261 | `		}` |
|       3 | 2262 | `		return SXRET_OK;` |
|       - | 2263 | `	}` |
|  295639 | 2264 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2265 | ``		/* php: a stray token after `return EXPR` is `... expecting ";"`. */`` |
|  295521 | 2266 | `		const char *zSave = pGen->zClauseCloser;` |
|  295521 | 2267 | `		pGen->zClauseCloser = "\";\"";` |
|       - | 2268 | ``		/* A `return` READS its operand (the value is consumed), so compile it`` |
|       - | 2269 | ``		 * read-only: a lone undefined variable `return $z` must warn at the read`` |
|       - | 2270 | `		 * exactly like echo/interpolation, not be loaded quietly (the same quiet` |
|       - | 2271 | ``		 * load that correctly keeps a bare `$z;` statement silent). Matches the`` |
|       - | 2272 | `		 * arrow-fn implicit-return body fix. */` |
|  295521 | 2273 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|  295521 | 2274 | `		pGen->zClauseCloser = zSave;` |
|  295521 | 2275 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2276 | `			return SXERR_ABORT;` |
|  295521 | 2277 | `		}else if(rc != SXERR_EMPTY ){` |
|  295521 | 2278 | `			nRet = 1;` |
|  147559 | 2279 | `		}` |
|  147559 | 2280 | `	}` |
|       - | 2281 | ``	/* A bare `return;` inside a function that DECLARES a return type is a php`` |
|       - | 2282 | `	 * COMPILE error, not the runtime TypeError PHL used to raise on the way out:` |
|       - | 2283 | `` 	 * php rejects the program before it runs. `void` (which is what `return;` `` |
|       - | 2284 | ``	 * means) and `never` (handled above) are the two declarations exempt from it,`` |
|       - | 2285 | ``	 * and a GENERATOR is exempt whatever it declares — there `return;` ends the`` |
|       - | 2286 | `	 * generator, and the declared type describes the Generator object the call` |
|       - | 2287 | `	 * produced, never the returned value. */` |
|  295634 | 2288 | `	if( nRet == 0 && pFunc && !pGen->bInGenerator && VmFuncHasReturnType(pFunc)` |
|      48 | 2289 | `	 && pFunc->nReturnType != MEMOBJ_VOID && pFunc->nReturnType != MEMOBJ_NEVER ){` |
|      19 | 2290 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - | 2291 | `			"A %s with return type must return a value%s",` |
|       5 | 2292 | `			GenStateReturnNoun(pGen),` |
|      10 | 2293 | `			GenStateReturnTypeAllowsNull(pFunc)` |
|       - | 2294 | `				? " (did you mean \"return null;\" instead of \"return;\"?)" : "");` |
|      14 | 2295 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2296 | `			return SXERR_ABORT;` |
|       - | 2297 | `		}` |
|       5 | 2298 | `	}` |
|       - | 2299 | ``	/* The mirror rule: a `void` function must not return a VALUE, and php stops`` |
|       - | 2300 | `	 * the program at the return statement rather than throwing on the way out.` |
|       - | 2301 | `	 * Generators keep their own diagnostic (a void generator is rejected as` |
|       - | 2302 | ``	 * `Generator return type must be a supertype of Generator`), so they are`` |
|       - | 2303 | `	 * skipped here exactly as above. php's hint fires when the operand is a` |
|       - | 2304 | ``	 * compile-time constant null; PHL folds the `null` KEYWORD (constant index 0,`` |
|       - | 2305 | `	 * emitted as a lone OP_LOADC and unchanged by any wrapping parens), which is` |
|       - | 2306 | `	 * every shape real code writes. */` |
|  295634 | 2307 | `	if( nRet != 0 && pFunc && !pGen->bInGenerator` |
|  285841 | 2308 | `	 && pFunc->nReturnType == MEMOBJ_VOID ){` |
|      16 | 2309 | `		VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      22 | 2310 | `		int bNullLiteral = (PH7_VmInstrLength(pGen->pVm) == nInstrBefore + 1)` |
|      12 | 2311 | `			&& pLast && pLast->iOp == PH7_OP_LOADC` |
|      18 | 2312 | `			&& pLast->iP1 == 0 && pLast->iP2 == 0;` |
|      22 | 2313 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - | 2314 | `			"A void %s must not return a value%s",` |
|       6 | 2315 | `			GenStateReturnNoun(pGen),` |
|       6 | 2316 | `			bNullLiteral` |
|       - | 2317 | `				? " (did you mean \"return;\" instead of \"return null;\"?)" : "");` |
|      16 | 2318 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2319 | `			return SXERR_ABORT;` |
|       - | 2320 | `		}` |
|       6 | 2321 | `	}` |
|       - | 2322 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|       - | 2323 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|       - | 2324 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|       - | 2325 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|  295639 | 2326 | `	if( pGen->bInGenerator ){` |
|      52 | 2327 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|      52 | 2328 | `		return SXRET_OK;` |
|       - | 2329 | `	}` |
|       - | 2330 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|       - | 2331 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|       - | 2332 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|       - | 2333 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|       - | 2334 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|  295591 | 2335 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|  295591 | 2336 | `	return SXRET_OK;` |
|  147624 | 2337 | `}` |
|       - | 2338 | `/*` |
|       - | 2339 | ` * Compile a yield expression.` |
|       - | 2340 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - | 2341 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - | 2342 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - | 2343 | ` */` |
|     612 | 2344 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       5 | 2345 | `{` |
|       - | 2346 | `	SyToken *pTmp, *pSplit;` |
|     617 | 2347 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|     617 | 2348 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - | 2349 | `	sxi32 rc;` |
|     306 | 2350 | `	(void)iCompileFlag;` |
|       - | 2351 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|     617 | 2352 | `	pGen->pIn++;` |
|       - | 2353 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - | 2354 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|       - | 2355 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|       - | 2356 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|       - | 2357 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|     612 | 2358 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|     352 | 2359 | `		&& pGen->pIn->sData.nByte == 4` |
|      95 | 2360 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|      87 | 2361 | `		pGen->pIn++; /* Skip 'from' */` |
|      87 | 2362 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      87 | 2363 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2364 | `			return SXERR_ABORT;` |
|       - | 2365 | `		}` |
|      87 | 2366 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 | 2367 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|     ! 0 | 2368 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|       - | 2369 | `				"Missing expression after 'yield from'");` |
|     ! 0 | 2370 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2371 | `				return SXERR_ABORT;` |
|       - | 2372 | `			}` |
|     ! 0 | 2373 | `		}` |
|      87 | 2374 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|      87 | 2375 | `		return SXRET_OK;` |
|       - | 2376 | `	}` |
|     535 | 2377 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 2378 | `		/* Bare yield — no value */` |
|       8 | 2379 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|       8 | 2380 | `		return SXRET_OK;` |
|       - | 2381 | `	}` |
|       - | 2382 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax. The shared` |
|       - | 2383 | `	 * array-entry scanner is the one that already knows which '=>' are NOT` |
|       - | 2384 | ``	 * separators — an arrow function's body and a match arm's — so `yield fn($x)`` |
|       - | 2385 | `` 	 * => $x` and `yield match($k){ 1 => 2 }` stop being read as `key => value` `` |
|       - | 2386 | `	 * pairs (the first was a parse error, the second yielded the wrong pair). */` |
|     529 | 2387 | `	pSplit = GenStateFindTopLevelArrow(pGen->pIn,pGen->pEnd);` |
|     529 | 2388 | `	if( pSplit >= pGen->pEnd ){` |
|     485 | 2389 | `		pSplit = 0;` |
|     240 | 2390 | `	}` |
|     529 | 2391 | `	pTmp = pGen->pEnd;` |
|     529 | 2392 | `	if( pSplit ){` |
|       - | 2393 | `		/* yield $key => $value */` |
|      48 | 2394 | `		pGen->pEnd = pSplit;` |
|      48 | 2395 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      48 | 2396 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      48 | 2397 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|      48 | 2398 | `		pGen->pEnd = pTmp;` |
|      48 | 2399 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      48 | 2400 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      48 | 2401 | `		iP1 = 1;` |
|      48 | 2402 | `		iP2 = 1;` |
|      26 | 2403 | `	}else{` |
|       - | 2404 | `		/* yield $value */` |
|     485 | 2405 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     485 | 2406 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     485 | 2407 | `		if( rc != SXERR_EMPTY ){` |
|     485 | 2408 | `			iP1 = 1;` |
|     240 | 2409 | `		}` |
|       - | 2410 | `	}` |
|     529 | 2411 | `	pGen->pEnd = pTmp;` |
|     529 | 2412 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|     529 | 2413 | `	return SXRET_OK;` |
|     311 | 2414 | `}` |
|       - | 2415 | `/*` |
|       - | 2416 | ` * Compile the die/exit language construct.` |
|       - | 2417 | ` * The role of these constructs is to terminate execution of the script.` |
|       - | 2418 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - | 2419 | ` */` |
|     316 | 2420 | `PH7_PRIVATE sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       5 | 2421 | `{` |
|     321 | 2422 | `	sxi32 nExpr = 0;` |
|       - | 2423 | `	sxi32 rc;` |
|       - | 2424 | `	/* Jump the die/exit keyword */` |
|     321 | 2425 | `	pGen->pIn++;` |
|     321 | 2426 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2427 | `		/* Compile the expression */` |
|     321 | 2428 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     321 | 2429 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2430 | `			return SXERR_ABORT;` |
|     321 | 2431 | `		}else if(rc != SXERR_EMPTY ){` |
|     321 | 2432 | `			nExpr = 1;` |
|     158 | 2433 | `		}` |
|     158 | 2434 | `	}` |
|       - | 2435 | `	/* Emit the HALT instruction */` |
|     321 | 2436 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|     321 | 2437 | `	return SXRET_OK;` |
|     163 | 2438 | `}` |
|       - | 2439 | `/*` |
|       - | 2440 | ` * Compile the 'echo' language construct.` |
|       - | 2441 | ` */` |
|   30714 | 2442 | `PH7_PRIVATE sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       5 | 2443 | `{` |
|   30719 | 2444 | `	SyToken *pTmp,*pNext = 0;` |
|   30719 | 2445 | `	sxu32 nLine = pGen->pIn->nLine;` |
|   30719 | 2446 | `	int nExpr = 0;      /* expressions actually compiled */` |
|   30719 | 2447 | `	int bExpectMore = 1;/* after 'echo' or a comma an expression is REQUIRED */` |
|       - | 2448 | `	sxi32 rc;` |
|       - | 2449 | `	/* Jump the 'echo' keyword */` |
|   30719 | 2450 | `	pGen->pIn++;` |
|       - | 2451 | `	/* Compile arguments one after one. php: a stray token in an echo list is` |
|       - | 2452 | ``	 * `... expecting "," or ";"` — set the closer for each argument's compile. */`` |
|   30719 | 2453 | `	pTmp = pGen->pEnd;` |
|       - | 2454 | `	{` |
|   30719 | 2455 | `	const char *zSaveEcho = pGen->zClauseCloser;` |
|   88874 | 2456 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   58166 | 2457 | `		if( pGen->pIn < pNext ){` |
|   58166 | 2458 | `			pGen->pEnd = pNext;` |
|   58166 | 2459 | `			pGen->zClauseCloser = "\",\" or \";\"";` |
|   58166 | 2460 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   58166 | 2461 | `			pGen->zClauseCloser = zSaveEcho;` |
|   58166 | 2462 | `			if( rc == SXERR_ABORT ){` |
|       6 | 2463 | `				return SXERR_ABORT;` |
|   58162 | 2464 | `			}else if( rc != SXERR_EMPTY ){` |
|       - | 2465 | `				/* Emit the consume instruction */` |
|   58094 | 2466 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|   58094 | 2467 | `				nExpr++;` |
|   58094 | 2468 | `				bExpectMore = 0;` |
|   28999 | 2469 | `			}` |
|   29033 | 2470 | `		}` |
|       - | 2471 | `		/* Jump trailing commas (php: exactly one between expressions; a` |
|       - | 2472 | `		 * dangling or doubled comma is a parse error, enforced below) */` |
|   85615 | 2473 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|   27460 | 2474 | `			if( bExpectMore ){` |
|       - | 2475 | `				/* two commas in a row */` |
|       3 | 2476 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,pNext->nLine,` |
|       - | 2477 | `					"syntax error, unexpected token \",\"");` |
|       3 | 2478 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - | 2479 | `			}` |
|   27458 | 2480 | `			bExpectMore = 1;` |
|   27458 | 2481 | `			pNext++;` |
|       5 | 2482 | `		}` |
|   58160 | 2483 | `		pGen->pIn = pNext;` |
|       5 | 2484 | `	}` |
|       - | 2485 | `	}` |
|       - | 2486 | `	/* Restore token stream */` |
|   30713 | 2487 | `	pGen->pEnd = pTmp;` |
|   30713 | 2488 | `	if( nExpr == 0 \|\| bExpectMore ){` |
|       - | 2489 | ``		/* `echo ;` or `echo expr, ;` — php rejects both */`` |
|      76 | 2490 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - | 2491 | `			"syntax error, unexpected token \";\"");` |
|      76 | 2492 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - | 2493 | `	}` |
|   30641 | 2494 | `	return SXRET_OK;` |
|   15324 | 2495 | `}` |
|       - | 2496 | `/*` |
|       - | 2497 | ` * Compile the static statement.` |
|       - | 2498 | ` * According to the PHP language reference` |
|       - | 2499 | ` *  Another important feature of variable scoping is the static variable.` |
|       - | 2500 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - | 2501 | ` *  when program execution leaves this scope.` |
|       - | 2502 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - | 2503 | ` * Symisc eXtension.` |
|       - | 2504 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - | 2505 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 2506 | ` *  Example` |
|       - | 2507 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - | 2508 | ` *    Refer to the official documentation for more information on this feature.` |
|       - | 2509 | ` */` |
|      80 | 2510 | `PH7_PRIVATE sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       5 | 2511 | `{` |
|       - | 2512 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - | 2513 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - | 2514 | `	GenBlock *pBlock;` |
|       - | 2515 | `	SyString *pName;` |
|       - | 2516 | `	char *zDup;` |
|       - | 2517 | `	sxu32 nLine;` |
|       - | 2518 | `	sxi32 rc;` |
|       - | 2519 | ``	/* `static function () {}` / `static fn () =>` at statement position is an`` |
|       - | 2520 | `	 * EXPRESSION statement (a bare static closure), not a static-variable` |
|       - | 2521 | `	 * declaration — hand it to the expression compiler (php accepts it). */` |
|      80 | 2522 | `	if( &pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & PH7_TK_KEYWORD)` |
|      47 | 2523 | `	 && (SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FUNCTION` |
|       2 | 2524 | `	  \|\| SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FN) ){` |
|       5 | 2525 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 | 2526 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2527 | `			return SXERR_ABORT;` |
|       5 | 2528 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 | 2529 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 | 2530 | `		}` |
|       5 | 2531 | `		return SXRET_OK;` |
|       - | 2532 | `	}` |
|       - | 2533 | `	/* Jump the static keyword */` |
|      81 | 2534 | `	nLine = pGen->pIn->nLine;` |
|      81 | 2535 | `	pGen->pIn++;` |
|       - | 2536 | `	/* Extract the enclosing function if any */` |
|      81 | 2537 | `	pBlock = pGen->pCurrent;` |
|     157 | 2538 | `	while( pBlock ){` |
|     153 | 2539 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|      77 | 2540 | `			break;` |
|       - | 2541 | `		}` |
|       - | 2542 | `		/* Point to the upper block */` |
|      81 | 2543 | `		pBlock = pBlock->pParent;` |
|       5 | 2544 | `	}` |
|      81 | 2545 | `	if( pBlock == 0 ){` |
|       - | 2546 | `		/* Static statement,called outside of a function body,treat it as a simple variable.` |
|       - | 2547 | `		 * php's list form applies here too, so each declarator is compiled on its own` |
|       - | 2548 | `		 * and the comma is stepped over rather than reaching the expression parser` |
|       - | 2549 | `		 * (which has no comma operator). */` |
|       3 | 2550 | `		for(;;){` |
|       7 | 2551 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       - | 2552 | ``				/* php: `static FOO;` is a syntax error naming FOO and expecting "::"`` |
|       - | 2553 | ``				 * (the parser is still open to `static::` at that point). */`` |
|     ! 0 | 2554 | `				rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"::\"");` |
|     ! 0 | 2555 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 2556 | `					return SXERR_ABORT;` |
|       - | 2557 | `				}` |
|     ! 0 | 2558 | `				goto Synchronize;` |
|       - | 2559 | `			}` |
|       - | 2560 | `			/* Compile the expression holding the variable */` |
|       7 | 2561 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|       7 | 2562 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2563 | `				return SXERR_ABORT;` |
|       7 | 2564 | `			}else if( rc != SXERR_EMPTY ){` |
|       - | 2565 | `				/* Emit the POP instruction */` |
|       7 | 2566 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       3 | 2567 | `			}` |
|       7 | 2568 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 | 2569 | `				break;` |
|       - | 2570 | `			}` |
|       3 | 2571 | `			pGen->pIn++; /* Jump the comma and take the next declarator */` |
|       1 | 2572 | `		}` |
|       5 | 2573 | `		return SXRET_OK;` |
|       - | 2574 | `	}` |
|      77 | 2575 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - | 2576 | ``	/* php declares a LIST here -- `static $a, $b = 2, $c;` -- each element its own`` |
|       - | 2577 | `	 * slot with its own optional initializer. PHL took the first declarator and` |
|       - | 2578 | ``	 * then refused the comma (`static: Unexpected token ','`), which is a fatal on`` |
|       - | 2579 | `	 * an everyday spelling: two counters in one statement is how the construct is` |
|       - | 2580 | `	 * usually written. */` |
|      39 | 2581 | `Declarator:` |
|       - | 2582 | `	/* Make sure we are dealing with a valid statement */` |
|      83 | 2583 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      76 | 2584 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 2585 | ``			/* php: `static FOO;` is a syntax error naming FOO and expecting "::"`` |
|       - | 2586 | ``			 * (the parser is still open to `static::` at that point). */`` |
|       3 | 2587 | `			rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"::\"");` |
|       3 | 2588 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2589 | `				return SXERR_ABORT;` |
|       - | 2590 | `			}` |
|       3 | 2591 | `			goto Synchronize;` |
|       - | 2592 | `	}` |
|      81 | 2593 | `	pGen->pIn++;` |
|       - | 2594 | `	/* Extract variable name */` |
|      81 | 2595 | `	pName = &pGen->pIn->sData;` |
|       - | 2596 | ``	/* php refuses `static $this;` at the declaration — the name is not a slot a`` |
|       - | 2597 | `	 * function may own. */` |
|      76 | 2598 | `	if( pName->nByte == sizeof("this")-1` |
|      46 | 2599 | `	 && SyMemcmp((const void *)pName->zString,(const void *)"this",sizeof("this")-1) == 0 ){` |
|       3 | 2600 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - | 2601 | `			"Cannot use $this as static variable");` |
|       3 | 2602 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2603 | `			return SXERR_ABORT;` |
|       - | 2604 | `		}` |
|       3 | 2605 | `		goto Synchronize;` |
|       - | 2606 | `	}` |
|      78 | 2607 | `	pGen->pIn++; /* Jump the var name */` |
|      74 | 2608 | `	if( pGen->pIn < pGen->pEnd` |
|      78 | 2609 | `	 && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|     ! 0 | 2610 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2611 | `		goto Synchronize;` |
|       - | 2612 | `	}` |
|       - | 2613 | `	/* Initialize the structure describing the static variable */` |
|      78 | 2614 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      78 | 2615 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - | 2616 | `	/* Duplicate variable name */` |
|      78 | 2617 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      78 | 2618 | `	if( zDup == 0 ){` |
|     ! 0 | 2619 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2620 | `		return SXERR_ABORT;` |
|       - | 2621 | `	}` |
|      78 | 2622 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - | 2623 | `	/* Check if we have an expression to compile */` |
|      78 | 2624 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - | 2625 | `		SySet *pInstrContainer;` |
|       - | 2626 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - | 2627 | `		 * Static variable can take any complex expression including function` |
|       - | 2628 | `		 * call as their initialization value.` |
|       - | 2629 | `		 * Example:` |
|       - | 2630 | `		 *		static $var = foo(1,4+5,bar());` |
|       - | 2631 | `		 */` |
|      61 | 2632 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - | 2633 | `		/* Swap bytecode container */` |
|      61 | 2634 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      61 | 2635 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - | 2636 | `		/* Compile the expression. EXPR_FLAG_COMMA_STATEMENT stops it at the first` |
|       - | 2637 | `		 * top-level comma so the declarator after it is left for the loop. */` |
|      61 | 2638 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|       - | 2639 | `		/* Emit the done instruction */` |
|      61 | 2640 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - | 2641 | `		/* Restore default bytecode container */` |
|      61 | 2642 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      29 | 2643 | `	}` |
|       - | 2644 | `	/* Finally save the compiled static variable in the appropriate container */` |
|      78 | 2645 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|      78 | 2646 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       7 | 2647 | `		pGen->pIn++; /* Jump the comma and take the next declarator */` |
|       7 | 2648 | `		goto Declarator;` |
|       - | 2649 | `	}` |
|      72 | 2650 | `	return SXRET_OK;` |
|       2 | 2651 | `Synchronize:` |
|       - | 2652 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - | 2653 | `	 * statement.` |
|       - | 2654 | `	 */` |
|      10 | 2655 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       6 | 2656 | `		pGen->pIn++;` |
|       2 | 2657 | `	}` |
|       6 | 2658 | `	return SXRET_OK;` |
|      45 | 2659 | `}` |
|       - | 2660 | `/*` |
|       - | 2661 | ` * Compile the var statement.` |
|       - | 2662 | ` * Symisc Extension:` |
|       - | 2663 | ` *      var statement can be used outside of a class definition.` |
|       - | 2664 | ` */` |
|       2 | 2665 | `PH7_PRIVATE sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 | 2666 | `{` |
|       - | 2667 | `` 	/* `var` is a PROPERTY modifier and nothing else. A class body's `var $x;` `` |
|       - | 2668 | `	 * is compiled by compile_class.c, never here, so reaching this statement` |
|       - | 2669 | ``	 * handler means `var` appeared outside a class — which php rejects as a`` |
|       - | 2670 | `	 * parse error. PHL used to compile it as an ordinary expression statement,` |
|       - | 2671 | ``	 * so top-level `var $x = 1;` silently ran (a Symisc extension). */`` |
|       3 | 2672 | `	PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       3 | 2673 | `	return SXERR_ABORT;` |
|       1 | 2674 | `}` |
|       - | 2675 | `/*` |
|       - | 2676 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - | 2677 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - | 2678 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - | 2679 | ` */` |
|       - | 2680 | `/*` |
|       - | 2681 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|       - | 2682 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - | 2683 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - | 2684 | ` * qualified name and updates the instruction's operand index.` |
|       - | 2685 | ` *` |
|       - | 2686 | ` * Resolution order:` |
|       - | 2687 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|       - | 2688 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|       - | 2689 | ` *   3. Otherwise return the original literal index unchanged.` |
|       - | 2690 | ` *` |
|       - | 2691 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|       - | 2692 | ` * came from an import (step 1) and 0 otherwise.` |
|       - | 2693 | ` * Returns the (possibly new) literal index.` |
|       - | 2694 | ` */` |
| 1023276 | 2695 | `PH7_PRIVATE sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|       5 | 2696 | `{` |
|       - | 2697 | `	ph7_value *pLit;` |
|       - | 2698 | `	const char *zLit;` |
|       - | 2699 | `	SyString sQualified;` |
|       - | 2700 | `	sxu32 nLit;` |
|       - | 2701 | `	sxu32 k;` |
|       - | 2702 | `	sxu32 nNewIdx;` |
|       - | 2703 | `	int hasNsSep;` |
|       - | 2704 | `	SyHashEntry *pImport;` |
|       - | 2705 | `	ph7_value *pNew;` |
| 1023281 | 2706 | `	if( pFromImport ){` |
|  906410 | 2707 | `		*pFromImport = 0;` |
|  452226 | 2708 | `	}` |
| 1023281 | 2709 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
| 1023281 | 2710 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 | 2711 | `		return nOrigIdx;` |
|       - | 2712 | `	}` |
| 1023281 | 2713 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
| 1023281 | 2714 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - | 2715 | `	/* Skip if already qualified (contains backslash) */` |
| 1023281 | 2716 | `	hasNsSep = 0;` |
| 9167795 | 2717 | `	for( k = 0; k < nLit; k++ ){` |
| 8144938 | 2718 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 4063762 | 2719 | `	}` |
| 1023281 | 2720 | `	if( hasNsSep ){` |
|     424 | 2721 | `		return nOrigIdx;` |
|       - | 2722 | `	}` |
|       - | 2723 | `	/* Check use imports first (works even outside namespaces) */` |
| 1022862 | 2724 | `	SyBlobReset(&pGen->sWorker);` |
| 1022862 | 2725 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
| 1022862 | 2726 | `	if( pImport ){` |
|     235 | 2727 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|     235 | 2728 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|     235 | 2729 | `		if( pFromImport ){` |
|      41 | 2730 | `			*pFromImport = 1;` |
|      18 | 2731 | `		}` |
|     120 | 2732 | `	}else{` |
| 1022632 | 2733 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
| 1022288 | 2734 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|       - | 2735 | `		}` |
|       - | 2736 | `		/* Prepend current namespace */` |
|     349 | 2737 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     349 | 2738 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|     349 | 2739 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - | 2740 | `	}` |
|       - | 2741 | `	/* Look up or create a new literal for the qualified name */` |
|     579 | 2742 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|     579 | 2743 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|     325 | 2744 | `		return nNewIdx; /* Already interned */` |
|       - | 2745 | `	}` |
|     259 | 2746 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|     259 | 2747 | `	if( pNew == 0 ){` |
|     ! 0 | 2748 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - | 2749 | `	}` |
|     259 | 2750 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|     259 | 2751 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|     259 | 2752 | `	return nNewIdx;` |
|  510585 | 2753 | `}` |
|       - | 2754 | `/*` |
|       - | 2755 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - | 2756 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - | 2757 | ` */` |
|    8310 | 2758 | `PH7_PRIVATE void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 | 2759 | `{` |
|       - | 2760 | `	SyHashEntry *pImport;` |
|    8315 | 2761 | `	const char *zName = pName->zString;` |
|    8315 | 2762 | `	sxu32 nName = pName->nByte;` |
|    8315 | 2763 | `	sxu32 nFirst = 0;` |
|       - | 2764 | `	/* php resolves a name through use-imports on its LEADING segment: an` |
|       - | 2765 | ``	 * unqualified `C` maps the whole name (`use X\C;` -> X\C), while a QUALIFIED`` |
|       - | 2766 | ``	 * `A\B\C` maps just `A` (`use X\A;` -> X\A\B\C) and keeps the `\B\C` tail.`` |
|       - | 2767 | `	 * The old code looked up the whole qualified string (which never matches a` |
|       - | 2768 | `	 * single-segment import alias) and then blindly prefixed the current` |
|       - | 2769 | ``	 * namespace, so `use PHPUnit\Framework; extends Framework\TestCase` resolved`` |
|       - | 2770 | `	 * to Ns\Framework\TestCase. Mirror GenStateResolveNamespaceLiteral here. */` |
|   79363 | 2771 | `	while( nFirst < nName && zName[nFirst] != '\\' ){ nFirst++; }` |
|    8315 | 2772 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)zName,nFirst);` |
|    8315 | 2773 | `	if( pImport ){` |
|     111 | 2774 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|     111 | 2775 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|     111 | 2776 | `		SyBlobAppend(pOut,zName + nFirst,nName - nFirst); /* the \B\C tail, if any */` |
|     111 | 2777 | `		return;` |
|       - | 2778 | `	}` |
|       - | 2779 | `	/* Prepend current namespace if active */` |
|    8209 | 2780 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      56 | 2781 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      56 | 2782 | `		SyBlobAppend(pOut,"\\",1);` |
|      26 | 2783 | `	}` |
|    8209 | 2784 | `	SyBlobAppend(pOut,zName,nName);` |
|    4158 | 2785 | `}` |
|       - | 2786 | `/*` |
|       - | 2787 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - | 2788 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - | 2789 | ` * The caller must release pOut when done.` |
|       - | 2790 | ` */` |
|   11542 | 2791 | `PH7_PRIVATE void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 | 2792 | `{` |
|   11547 | 2793 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     781 | 2794 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     781 | 2795 | `		SyBlobAppend(pOut,"\\",1);` |
|     388 | 2796 | `	}` |
|   11547 | 2797 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   11547 | 2798 | `}` |
|       - | 2799 | `/*` |
|       - | 2800 | ` * TRUE when pB starts exactly where pA ends. The tokenizer drops whitespace, so` |
|       - | 2801 | ` * source offsets are the only record of it — and php's qualified-name token` |
|       - | 2802 | `` * (`{LABEL}("\\"{LABEL})+`) is matched by its LEXER, which means the `\` has to`` |
|       - | 2803 | `` * be glued: `private\Q` is one name, `private \Q` is a modifier and a type, and`` |
|       - | 2804 | ` * only this test tells the two apart.` |
|       - | 2805 | ` */` |
|     426 | 2806 | `PH7_PRIVATE int GenStateTokensGlued(SyToken *pA,SyToken *pB)` |
|       5 | 2807 | `{` |
|     431 | 2808 | `	return pA->sData.zString + pA->sData.nByte == pB->sData.zString;` |
|       5 | 2809 | `}` |
|       - | 2810 | `/*` |
|       - | 2811 | `` * php's `namespace\X` NAME OPERATOR (5.3): a leading `namespace` keyword glued to`` |
|       - | 2812 | `` * a `\` names the CURRENT namespace, and the whole name is then FULLY QUALIFIED —`` |
|       - | 2813 | `` * `namespace\X` inside `namespace B;` is `\B\X`, and plain `\X` at global scope.`` |
|       - | 2814 | `` * php's lexer matches it as one token (T_NAME_RELATIVE, `"namespace"("\\"{LABEL})+`,`` |
|       - | 2815 | `` * case-insensitively), so the `\` must be GLUED to the keyword: `namespace \X` is a`` |
|       - | 2816 | ` * php parse error, and this mirrors that by comparing source offsets.` |
|       - | 2817 | ` *` |
|       - | 2818 | ` * This predicate only RECOGNIZES the operator (it consumes nothing), which is what` |
|       - | 2819 | `` * the statement dispatcher needs to tell `namespace\X::m();` from a namespace`` |
|       - | 2820 | ` * DECLARATION; GenStateNsRelPrefix below is what the name collectors call.` |
|       - | 2821 | ` */` |
| 2446329 | 2822 | `PH7_PRIVATE int GenStateIsNsRelName(SyToken *pIn,SyToken *pEnd)` |
|       5 | 2823 | `{` |
| 2446329 | 2824 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
| 1990844 | 2825 | `	 \|\| (sxu32)SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_NAMESPACE ){` |
| 2445898 | 2826 | `		return 0;` |
|       - | 2827 | `	}` |
|     441 | 2828 | `	if( &pIn[1] >= pEnd \|\| (pIn[1].nType & PH7_TK_NSSEP) == 0 ){` |
|     345 | 2829 | `		return 0;` |
|       - | 2830 | `	}` |
|      98 | 2831 | `	if( &pIn[2] >= pEnd \|\| (pIn[2].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 2832 | ``		return 0; /* php's T_NAME_RELATIVE needs at least one segment after the `\` */`` |
|       - | 2833 | `	}` |
|      98 | 2834 | `	return GenStateTokensGlued(pIn,&pIn[1]);` |
| 1221281 | 2835 | `}` |
|       - | 2836 | `/*` |
|       - | 2837 | `` * Consume a leading `namespace\` (see GenStateIsNsRelName) at *ppIn and seed pOut`` |
|       - | 2838 | ` * with the current namespace plus its separator — nothing at global scope, where` |
|       - | 2839 | ` * the bare name already IS the FQN. Returns TRUE when it fired, and the caller` |
|       - | 2840 | `` * must then treat the name it goes on to collect as ABSOLUTE: no `use` import may`` |
|       - | 2841 | ` * apply to it, and the current namespace is already in place.` |
|       - | 2842 | ` */` |
|  390145 | 2843 | `PH7_PRIVATE int GenStateNsRelPrefix(ph7_gen_state *pGen,SyToken **ppIn,SyToken *pEnd,SyBlob *pOut)` |
|       5 | 2844 | `{` |
|  390150 | 2845 | `	SyToken *pIn = *ppIn;` |
|  390150 | 2846 | `	if( !GenStateIsNsRelName(pIn,pEnd) ){` |
|  390062 | 2847 | `		return 0;` |
|       - | 2848 | `	}` |
|      90 | 2849 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      80 | 2850 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      80 | 2851 | `		SyBlobAppend(pOut,"\\",1);` |
|      39 | 2852 | `	}` |
|      90 | 2853 | `	*ppIn = &pIn[2];` |
|      90 | 2854 | `	return 1;` |
|  194802 | 2855 | `}` |
|       - | 2856 | `/*` |
|       - | 2857 | ` * Compile a namespace statement` |
|       - | 2858 | ` * According to the PHP language reference manual` |
|       - | 2859 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - | 2860 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - | 2861 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - | 2862 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - | 2863 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - | 2864 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - | 2865 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - | 2866 | ` *  programming world.` |
|       - | 2867 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - | 2868 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - | 2869 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - | 2870 | ` *  classes/functions/constants.` |
|       - | 2871 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - | 2872 | ` *  readability of source code.` |
|       - | 2873 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - | 2874 | ` *  Here is an example of namespace syntax in PHP:` |
|       - | 2875 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - | 2876 | ` *       class MyClass {}` |
|       - | 2877 | ` *       function myfunction() {}` |
|       - | 2878 | ` *       const MYCONST = 1;` |
|       - | 2879 | ` *       $a = new MyClass;` |
|       - | 2880 | ` *       $c = new \my\name\MyClass;` |
|       - | 2881 | ` *       $a = strlen('hi');` |
|       - | 2882 | ` *       $d = namespace\MYCONST;` |
|       - | 2883 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - | 2884 | ` *       echo constant($d);` |
|       - | 2885 | ` * NOTE` |
|       - | 2886 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - | 2887 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - | 2888 | ` */` |
|       - | 2889 | `/*` |
|       - | 2890 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - | 2891 | ` */` |
|      14 | 2892 | `PH7_PRIVATE const char * TokenTypeName(sxu32 nType)` |
|       4 | 2893 | `{` |
|      18 | 2894 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|      10 | 2895 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|      10 | 2896 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|       - | 2897 | `	/* php's parse errors call a reserved word a "token", never a "keyword" — the` |
|       - | 2898 | ``	 * word only ever appears in ITS vocabulary as `unexpected token "while"`. */`` |
|      10 | 2899 | `	if( nType & PH7_TK_KEYWORD ){ return "token"; }` |
|      10 | 2900 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|      10 | 2901 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|       3 | 2902 | `	return "token";` |
|      11 | 2903 | `}` |
|     340 | 2904 | `PH7_PRIVATE sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       5 | 2905 | `{` |
|       - | 2906 | `	sxu32 nLine;` |
|       - | 2907 | `	sxi32 rc;` |
|     345 | 2908 | `	nLine = pGen->pIn->nLine;` |
|     345 | 2909 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - | 2910 | `	/* Reset namespace and clear previous use imports */` |
|     345 | 2911 | `	SyBlobReset(&pGen->sNamespace);` |
|     345 | 2912 | `	GenStateResetUseImports(&(*pGen),pGen->pVm);` |
|     345 | 2913 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 2914 | `		return SXRET_OK; /* Global namespace (bare "namespace;") */` |
|       - | 2915 | `	}` |
|     345 | 2916 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|     ! 0 | 2917 | `		return SXRET_OK; /* namespace; — switch to global namespace */` |
|       - | 2918 | `	}` |
|     345 | 2919 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|      13 | 2920 | `		return SXRET_OK; /* namespace { } — global namespace block */` |
|       - | 2921 | `	}` |
|       - | 2922 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     859 | 2923 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     529 | 2924 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - | 2925 | `			/* Append backslash separator */` |
|     102 | 2926 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     102 | 2927 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|      49 | 2928 | `			}` |
|      53 | 2929 | `		}else{` |
|       - | 2930 | `			/* Append identifier */` |
|     431 | 2931 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - | 2932 | `		}` |
|     529 | 2933 | `		pGen->pIn++;` |
|       5 | 2934 | `	}` |
|     335 | 2935 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       8 | 2936 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - | 2937 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 | 2938 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       6 | 2939 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2940 | `			return SXERR_ABORT;` |
|       - | 2941 | `		}` |
|       2 | 2942 | `	}` |
|     335 | 2943 | `	return SXRET_OK;` |
|     175 | 2944 | `}` |
|       - | 2945 | `/*` |
|       - | 2946 | ` * Initialize the three use-import tables of a code generator.` |
|       - | 2947 | ` *` |
|       - | 2948 | ` * php resolves CLASS and FUNCTION imports case-INSENSITIVELY, like every other` |
|       - | 2949 | ``  * name in those two families: `use A\Cee;` then `CEE::K`, `use A\Cee as Alias;` `` |
|       - | 2950 | `` * then `ALIAS::K`, `use function A\eff;` then `EFF()`, and a wrong-case leading`` |
|       - | 2951 | `` * segment of an imported namespace (`use A\B;` then `b\Cee::K`) all resolve.`` |
|       - | 2952 | ` * So both tables fold through SyStrHash/SyStrnmicmp, exactly like hClass /` |
|       - | 2953 | ` * hMethod / hFunction.` |
|       - | 2954 | ` *` |
|       - | 2955 | ` * The CONST table stays BYTE-EXACT: php keeps constant names case-sensitive,` |
|       - | 2956 | `` * so `use const A\KAY;` followed by `kay` must remain an undefined constant.`` |
|       - | 2957 | ` * That asymmetry is why the three tables exist separately.` |
|       - | 2958 | ` */` |
|   47562 | 2959 | `PH7_PRIVATE void GenStateInitUseImports(ph7_gen_state *pGen,ph7_vm *pVm)` |
|       5 | 2960 | `{` |
|   47567 | 2961 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|   47567 | 2962 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|   47567 | 2963 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|   47567 | 2964 | `}` |
|       - | 2965 | `/*` |
|       - | 2966 | ` * Drop every import currently in scope and start a fresh set (a namespace` |
|       - | 2967 | ` * switch clears imports).  Keeps the case rules of GenStateInitUseImports.` |
|       - | 2968 | ` */` |
|   40837 | 2969 | `PH7_PRIVATE void GenStateResetUseImports(ph7_gen_state *pGen,ph7_vm *pVm)` |
|       5 | 2970 | `{` |
|   40842 | 2971 | `	SyHashRelease(&pGen->hUseImports);` |
|   40842 | 2972 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   40842 | 2973 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   40842 | 2974 | `	GenStateInitUseImports(&(*pGen),&(*pVm));` |
|   40842 | 2975 | `}` |
|       - | 2976 | `/*` |
|       - | 2977 | ` * The two DECLARED-name tables (classes and functions declared so far in this` |
|       - | 2978 | ` * compile unit). php refuses an import whose name a declaration already took, and` |
|       - | 2979 | ` * the check is per COMPILE UNIT and case-INSENSITIVE — a class declared by a file` |
|       - | 2980 | `` * this one later `require`s is invisible to it, because that file compiles after`` |
|       - | 2981 | ` * this one has finished. Both tables key on the FQN, so they survive a namespace` |
|       - | 2982 | ` * switch (which clears only the imports).` |
|       - | 2983 | ` */` |
|   47222 | 2984 | `PH7_PRIVATE void GenStateInitSeenSymbols(ph7_gen_state *pGen,ph7_vm *pVm)` |
|       5 | 2985 | `{` |
|   47227 | 2986 | `	SyHashInit(&pGen->hSeenClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|   47227 | 2987 | `	SyHashInit(&pGen->hSeenFunc,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|   47227 | 2988 | `}` |
|   40501 | 2989 | `PH7_PRIVATE void GenStateReleaseSeenSymbols(ph7_gen_state *pGen)` |
|       5 | 2990 | `{` |
|   40506 | 2991 | `	SyHashRelease(&pGen->hSeenClass);` |
|   40506 | 2992 | `	SyHashRelease(&pGen->hSeenFunc);` |
|   40506 | 2993 | `}` |
|   40497 | 2994 | `PH7_PRIVATE void GenStateResetSeenSymbols(ph7_gen_state *pGen,ph7_vm *pVm)` |
|       5 | 2995 | `{` |
|   40502 | 2996 | `	GenStateReleaseSeenSymbols(&(*pGen));` |
|   40502 | 2997 | `	GenStateInitSeenSymbols(&(*pGen),&(*pVm));` |
|   40502 | 2998 | `}` |
|       - | 2999 | `/*` |
|       - | 3000 | ` * Record one declared CLASS (bFunc = 0) or FUNCTION (bFunc = 1) FQN so a later` |
|       - | 3001 | `` * `use` in this compile unit can see that the name is taken.`` |
|       - | 3002 | ` */` |
|  157280 | 3003 | `PH7_PRIVATE void GenStateRecordDeclaredName(ph7_gen_state *pGen,int bFunc,const SyString *pFqn)` |
|       5 | 3004 | `{` |
|  157285 | 3005 | `	SyHash *pHash = bFunc ? &pGen->hSeenFunc : &pGen->hSeenClass;` |
|       - | 3006 | `	char *zDup;` |
|  157285 | 3007 | `	if( pFqn->nByte < 1 \|\| SyHashGet(pHash,pFqn->zString,pFqn->nByte) != 0 ){` |
|      33 | 3008 | `		return;` |
|       - | 3009 | `	}` |
|  157257 | 3010 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pFqn->zString,pFqn->nByte);` |
|  157257 | 3011 | `	if( zDup ){` |
|       - | 3012 | `		/* The blob the caller built is released on its way out, so the table owns` |
|       - | 3013 | `		 * a pool copy (freed in bulk with the VM, like the import FQNs). */` |
|  157257 | 3014 | `		SyHashInsert(pHash,zDup,pFqn->nByte,zDup);` |
|   78522 | 3015 | `	}` |
|   78541 | 3016 | `}` |
|       - | 3017 | `/*` |
|       - | 3018 | `` * php refuses a DECLARATION whose short name a local `use` import already took:`` |
|       - | 3019 | ` *` |
|       - | 3020 | ` *   use A\Cee;  class Cee {}    Cannot redeclare class B\Cee (previously declared as local import)` |
|       - | 3021 | ` *   use function A\eff;  function eff(){}` |
|       - | 3022 | ` *                               Cannot redeclare function B\eff() (previously declared as local import)` |
|       - | 3023 | ` *   use const A\KAY;  const KAY = 1;` |
|       - | 3024 | ` *                               Cannot declare const B\KAY because the name is already in use` |
|       - | 3025 | ` *` |
|       - | 3026 | `` * A SELF-import (`use B\Cee;` inside `namespace B;`) names this very declaration`` |
|       - | 3027 | ` * and is a no-op, so it is exempt. iKind: 0 = class family (interface/trait/enum` |
|       - | 3028 | ` * included — php says "class" for all four), 1 = function, 2 = const.` |
|       - | 3029 | ` */` |
|  157462 | 3030 | `PH7_PRIVATE sxi32 GenStateGuardImportRedeclare(ph7_gen_state *pGen,int iKind,` |
|       - | 3031 | `	const SyString *pShort,const SyString *pFqn,sxu32 nLine)` |
|       5 | 3032 | `{` |
|       - | 3033 | `	SyHash *pImports;` |
|       - | 3034 | `	SyHashEntry *pEntry;` |
|       - | 3035 | `	const char *zImported;` |
|       - | 3036 | `	sxu32 nImported;` |
|  157467 | 3037 | `	switch( iKind ){` |
|  151767 | 3038 | `		case 1:  pImports = &pGen->hUseFuncImports; break;` |
|     187 | 3039 | `		case 2:  pImports = &pGen->hUseConstImports; break;` |
|    5523 | 3040 | `		default: pImports = &pGen->hUseImports; break;` |
|       - | 3041 | `	}` |
|  157467 | 3042 | `	pEntry = SyHashGet(pImports,(const void *)pShort->zString,pShort->nByte);` |
|  157467 | 3043 | `	if( pEntry == 0 ){` |
|  157453 | 3044 | `		return SXRET_OK;` |
|       - | 3045 | `	}` |
|      18 | 3046 | `	zImported = (const char *)pEntry->pUserData;` |
|      18 | 3047 | `	nImported = SyStrlen(zImported);` |
|      14 | 3048 | `	if( nImported == pFqn->nByte` |
|      22 | 3049 | `	 && (iKind == 2 ? SyMemcmp((const void *)zImported,(const void *)pFqn->zString,nImported) == 0` |
|       8 | 3050 | `	                : SyStrnicmp(zImported,pFqn->zString,nImported) == 0) ){` |
|       7 | 3051 | `		return SXRET_OK; /* the import IS this declaration */` |
|       - | 3052 | `	}` |
|      13 | 3053 | `	if( iKind == 2 ){` |
|       4 | 3054 | `		return PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       1 | 3055 | `			"Cannot declare const %z because the name is already in use",pFqn);` |
|       - | 3056 | `	}` |
|       8 | 3057 | `	return PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       2 | 3058 | `		iKind == 1 ? "Cannot redeclare function %z() (previously declared as local import)"` |
|       2 | 3059 | `		           : "Cannot redeclare class %z (previously declared as local import)",pFqn);` |
|   78630 | 3060 | `}` |
|       - | 3061 | `/*` |
|       - | 3062 | ` * TRUE when pTok is a PHL KEYWORD token that php's lexer nevertheless hands back` |
|       - | 3063 | ` * as a plain T_STRING. php reserves fewer words than PHL's table does, and the` |
|       - | 3064 | `` * `as` clause of a `use` accepts a T_STRING and nothing else — so `use A\Q as`` |
|       - | 3065 | `` * integer;` is a legal (if odd) php import while `use A\Q as echo;` is a parse`` |
|       - | 3066 | ` * error. These are exactly the type-NAME keywords: php spells its scalar types` |
|       - | 3067 | `` * with ordinary labels, and `self`/`parent` too (only `static` is a real token).`` |
|       - | 3068 | ` */` |
|       2 | 3069 | `static int GenStateKeywordIsPhpLabel(SyToken *pTok)` |
|       1 | 3070 | `{` |
|       - | 3071 | `	sxu32 nKey;` |
|       3 | 3072 | `	if( (pTok->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 3073 | `		return 0;` |
|       - | 3074 | `	}` |
|       3 | 3075 | `	nKey = (sxu32)(SX_PTR_TO_INT(pTok->pUserData));` |
|       4 | 3076 | `	return nKey == PH7_TKWRD_BOOL \|\| nKey == PH7_TKWRD_INT \|\| nKey == PH7_TKWRD_FLOAT` |
|       2 | 3077 | `		\|\| nKey == PH7_TKWRD_STRING \|\| nKey == PH7_TKWRD_OBJECT` |
|       3 | 3078 | `		\|\| nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT;` |
|       2 | 3079 | `}` |
|       - | 3080 | `/*` |
|       - | 3081 | ` * TRUE for the names php refuses to let a CLASS import occupy — zend's reserved` |
|       - | 3082 | ` * class names. Distinct from compile_func.c's GenStateIsReservedTypeWord, which` |
|       - | 3083 | ` * answers "is this word a built-in TYPE rather than a class name": that one also` |
|       - | 3084 | `` * covers the `boolean`/`integer`/`double` aliases, and php imports those happily`` |
|       - | 3085 | `` * (`use A\boolean;` is accepted). Matched case-insensitively, like php.`` |
|       - | 3086 | ` */` |
|     214 | 3087 | `static int GenStateIsReservedClassName(const SyString *pName)` |
|       5 | 3088 | `{` |
|       - | 3089 | `	static const char *azWords[] = {` |
|       - | 3090 | `		"self","parent","static","int","float","string","bool","array","object",` |
|       - | 3091 | `		"null","false","true","void","iterable","mixed","never","callable"` |
|       - | 3092 | `	};` |
|       - | 3093 | `	sxu32 i;` |
|    3815 | 3094 | `	for( i = 0 ; i < SX_ARRAYSIZE(azWords) ; i++ ){` |
|    3605 | 3095 | `		sxu32 n = (sxu32)SyStrlen(azWords[i]);` |
|    3605 | 3096 | `		if( pName->nByte == n && SyStrnicmp(pName->zString,azWords[i],n) == 0 ){` |
|       6 | 3097 | `			return 1;` |
|       - | 3098 | `		}` |
|    1803 | 3099 | `	}` |
|     215 | 3100 | `	return 0;` |
|     112 | 3101 | `}` |
|       - | 3102 | `/*` |
|       - | 3103 | `` * Register one resolved `use` import: alias -> FQN, in the table its KIND owns`` |
|       - | 3104 | ` * (iUseType: 0 = class, 1 = function, 2 = const).  Shared by the plain form` |
|       - | 3105 | `` * (`use A\Cee;`) and by each member of a group (`use A\{Cee, Dee};`).`` |
|       - | 3106 | ` */` |
|     288 | 3107 | `static sxi32 GenStateAddImport(` |
|       - | 3108 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 3109 | `	int iUseType,         /* 0=class, 1=function, 2=const */` |
|       - | 3110 | `	SyBlob *pPath,        /* Fully qualified name being imported */` |
|       - | 3111 | `	SyString *pAlias,     /* Short name it is imported under */` |
|       - | 3112 | `	sxu32 nLine           /* Line of the 'use' keyword (for diagnostics) */` |
|       - | 3113 | `	)` |
|       5 | 3114 | `{` |
|       - | 3115 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|       - | 3116 | `	const char *zKind;  /* php's kind word in the "already in use" message */` |
|       - | 3117 | `	char *zDup;` |
|       - | 3118 | `	sxi32 rc;` |
|       - | 3119 | `	/* Select the target hash table based on import type. */` |
|     293 | 3120 | `	switch( iUseType ){` |
|      43 | 3121 | `		case 1:  pGenHash = &pGen->hUseFuncImports; break;` |
|      41 | 3122 | `		case 2:  pGenHash = &pGen->hUseConstImports; break;` |
|     219 | 3123 | `		default: pGenHash = &pGen->hUseImports; break;` |
|       - | 3124 | `	}` |
|       - | 3125 | `	/* php names the KIND of a non-class import in this message: "Cannot use` |
|       - | 3126 | `	 * function A\eff as eff …" / "Cannot use const A\KAY as KAY …". */` |
|     293 | 3127 | `	zKind = iUseType == 1 ? "function " : iUseType == 2 ? "const " : "";` |
|       - | 3128 | `	/* A CLASS import may not take a reserved class name, however it got there:` |
|       - | 3129 | ``	 * as the trailing segment (`use A\self;`) or as an explicit alias`` |
|       - | 3130 | ``	 * (`use A\Q as self;`). Only classes — `use function A\self;` and`` |
|       - | 3131 | ``	 * `use const A\self;` are both accepted by php. */`` |
|     293 | 3132 | `	if( iUseType == 0 && GenStateIsReservedClassName(pAlias) ){` |
|       8 | 3133 | `		return PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 3134 | `			"Cannot use %.*s as %z because '%z' is a special class name",` |
|       4 | 3135 | `			(int)SyBlobLength(pPath),(const char *)SyBlobData(pPath),pAlias,pAlias);` |
|       - | 3136 | `	}` |
|       - | 3137 | `	/* Check for duplicate import alias (per-type) */` |
|     289 | 3138 | `	if( SyHashGet(pGenHash,pAlias->zString,pAlias->nByte) != 0 ){` |
|      12 | 3139 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 3140 | `			"Cannot use %s%.*s as %z because the name is already in use",` |
|       6 | 3141 | `			zKind,(int)SyBlobLength(pPath),(const char *)SyBlobData(pPath),pAlias);` |
|       9 | 3142 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3143 | `			return SXERR_ABORT;` |
|       - | 3144 | `		}` |
|       3 | 3145 | `	}` |
|       - | 3146 | `	/* …and refuses one whose name a DECLARATION in this compile unit already took` |
|       - | 3147 | ``	 * (`class Cee {} use A\Cee;`), unless the import names that very declaration.`` |
|       - | 3148 | `	 * The name an import occupies is the alias in the CURRENT namespace, which is` |
|       - | 3149 | `	 * what the seen tables key on. php runs this check for classes and functions` |
|       - | 3150 | ``	 * only — a `const` declaration followed by its own `use const` is accepted. */`` |
|     289 | 3151 | `	if( iUseType != 2 ){` |
|       - | 3152 | `		SyBlob sTaken;` |
|     253 | 3153 | `		SyBlobInit(&sTaken,&pGen->pVm->sAllocator);` |
|     253 | 3154 | `		GenStateBuildFQN(&(*pGen),pAlias,&sTaken);` |
|     248 | 3155 | `		if( SyHashGet(iUseType == 1 ? &pGen->hSeenFunc : &pGen->hSeenClass,` |
|     372 | 3156 | `				SyBlobData(&sTaken),SyBlobLength(&sTaken)) != 0` |
|     131 | 3157 | `		 && (SyBlobLength(&sTaken) != SyBlobLength(pPath)` |
|       4 | 3158 | `			\|\| SyStrnicmp((const char *)SyBlobData(&sTaken),(const char *)SyBlobData(pPath),` |
|       6 | 3159 | `				(sxu32)SyBlobLength(&sTaken)) != 0) ){` |
|       8 | 3160 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 3161 | `				"Cannot use %s%.*s as %z because the name is already in use",` |
|       4 | 3162 | `				zKind,(int)SyBlobLength(pPath),(const char *)SyBlobData(pPath),pAlias);` |
|       6 | 3163 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3164 | `				SyBlobRelease(&sTaken);` |
|     ! 0 | 3165 | `				return SXERR_ABORT;` |
|       - | 3166 | `			}` |
|       2 | 3167 | `		}` |
|     253 | 3168 | `		SyBlobRelease(&sTaken);` |
|     124 | 3169 | `	}` |
|       - | 3170 | `	/* Register the import: alias -> FQN.` |
|       - | 3171 | `	 * Strings are allocated from the VM pool allocator and freed` |
|       - | 3172 | `	 * when the entire VM is released. SyHashRelease does not free` |
|       - | 3173 | `	 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|     431 | 3174 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     284 | 3175 | `		(const char *)SyBlobData(pPath),SyBlobLength(pPath));` |
|     289 | 3176 | `	if( zDup ){` |
|       - | 3177 | `		/* All three kinds resolve entirely at COMPILE time — a const import is read` |
|       - | 3178 | `		 * by the OP_LOADC candidate builder (compile_node.c), so no runtime table` |
|       - | 3179 | `		 * is needed for it either. */` |
|     289 | 3180 | `		SyHashInsert(pGenHash,pAlias->zString,pAlias->nByte,zDup);` |
|     142 | 3181 | `	}` |
|     289 | 3182 | `	return SXRET_OK;` |
|     149 | 3183 | `}` |
|       - | 3184 | `/*` |
|       - | 3185 | `` * Collect one `\`-separated name into pOut (appending to whatever it holds, with`` |
|       - | 3186 | ` * a separator when needed) and return its LAST segment token, or 0 when the` |
|       - | 3187 | ` * cursor is not on a name at all.` |
|       - | 3188 | ` */` |
|     306 | 3189 | `static SyToken * GenStateCollectNsPath(ph7_gen_state *pGen,SyBlob *pOut)` |
|       5 | 3190 | `{` |
|     311 | 3191 | `	SyToken *pLast = 0;` |
|     311 | 3192 | ``	int bAfterSep = 0;   /* the token just consumed was a `\` */`` |
|    1165 | 3193 | `	while( pGen->pIn < pGen->pEnd ){` |
|    1163 | 3194 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|     569 | 3195 | `			bAfterSep = ( pGen->pIn + 1 < pGen->pEnd` |
|     282 | 3196 | `				&& GenStateTokensGlued(pGen->pIn,&pGen->pIn[1]) );` |
|     287 | 3197 | `			pGen->pIn++;` |
|     287 | 3198 | `			continue;` |
|       - | 3199 | `		}` |
|     881 | 3200 | `		if( (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 3201 | `			/* php lexes a QUALIFIED name as one T_NAME_QUALIFIED token before it` |
|       - | 3202 | `			 * consults the keyword table, so every reserved word is a legal SEGMENT` |
|       - | 3203 | ``			 * of it — `use A\Default\Q;`, `use Default\Q;`, `use A\as;` are all`` |
|       - | 3204 | `			 * accepted — while a BARE reserved word is not a name at all` |
|       - | 3205 | ``			 * (`use Default;` is a php parse error, and so is `use A\{Default};`).`` |
|       - | 3206 | ``			 * A `\` on one side or the other is exactly what separates the two, and`` |
|       - | 3207 | `` 			 * it must be the IMMEDIATE neighbour: the `as` of `use A\Cee as Baz;` `` |
|       - | 3208 | `			 * carries no separator and must still end the path. */` |
|     337 | 3209 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     167 | 3210 | `				break;` |
|       - | 3211 | `			}` |
|     170 | 3212 | `			if( !bAfterSep` |
|     161 | 3213 | `			 && !(pGen->pIn + 1 < pGen->pEnd && (pGen->pIn[1].nType & PH7_TK_NSSEP)` |
|      71 | 3214 | `				&& GenStateTokensGlued(pGen->pIn,&pGen->pIn[1])) ){` |
|      76 | 3215 | `				break;` |
|       - | 3216 | `			}` |
|      14 | 3217 | `		}` |
|     577 | 3218 | `		pLast = pGen->pIn;` |
|     577 | 3219 | `		if( SyBlobLength(pOut) > 0 ){` |
|     301 | 3220 | `			SyBlobAppend(pOut,"\\",1);` |
|     148 | 3221 | `		}` |
|     577 | 3222 | `		SyBlobAppend(pOut,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|     577 | 3223 | `		bAfterSep = 0;` |
|     577 | 3224 | `		pGen->pIn++;` |
|       5 | 3225 | `	}` |
|     311 | 3226 | `	return pLast;` |
|       5 | 3227 | `}` |
|       - | 3228 | `/*` |
|       - | 3229 | `` * Park the cursor on the `;` that ends this declaration so a refused `use` does`` |
|       - | 3230 | ` * not leave its remaining tokens for the statement dispatcher to read as an` |
|       - | 3231 | ` * expression, which would pile a second diagnostic on the first.` |
|       - | 3232 | ` */` |
|       4 | 3233 | `static void GenStateSkipToStatementEnd(ph7_gen_state *pGen)` |
|       1 | 3234 | `{` |
|       7 | 3235 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       3 | 3236 | `		pGen->pIn++;` |
|       1 | 3237 | `	}` |
|       5 | 3238 | `}` |
|       - | 3239 | `/*` |
|       - | 3240 | `` * `namespace\X` lexes as php's T_NAME_RELATIVE, and a `use` statement takes a`` |
|       - | 3241 | ` * plain or fully-qualified name only. php refuses it and NAMES the token kind in` |
|       - | 3242 | ` * the message, a wording TokenTypeName cannot spell. Consumes the name, reports,` |
|       - | 3243 | ` * and answers TRUE when it fired; zExpecting is php's trailing clause (empty for` |
|       - | 3244 | ` * the plain form, the member list for a group).` |
|       - | 3245 | ` */` |
|     308 | 3246 | `static int GenStateUseRejectNsRelName(ph7_gen_state *pGen,sxu32 nLine,const char *zExpecting,sxi32 *pRc)` |
|       5 | 3247 | `{` |
|       - | 3248 | `	SyBlob sName;` |
|     313 | 3249 | `	if( !GenStateIsNsRelName(pGen->pIn,pGen->pEnd) ){` |
|     311 | 3250 | `		return 0;` |
|       - | 3251 | `	}` |
|       3 | 3252 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|       3 | 3253 | `	SyBlobAppend(&sName,"namespace",sizeof("namespace")-1);` |
|       3 | 3254 | ``	pGen->pIn++; /* the `namespace` keyword; the `\` and its segments follow */`` |
|       5 | 3255 | `	while( pGen->pIn + 1 < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP)` |
|       5 | 3256 | `		&& (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 | 3257 | `		SyBlobAppend(&sName,"\\",1);` |
|       3 | 3258 | `		SyBlobAppend(&sName,pGen->pIn[1].sData.zString,pGen->pIn[1].sData.nByte);` |
|       3 | 3259 | `		pGen->pIn += 2;` |
|       1 | 3260 | `	}` |
|       5 | 3261 | `	*pRc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - | 3262 | `		"syntax error, unexpected namespace-relative name \"%.*s\"%s",` |
|       2 | 3263 | `		(int)SyBlobLength(&sName),(const char *)SyBlobData(&sName),zExpecting);` |
|       3 | 3264 | `	SyBlobRelease(&sName);` |
|       3 | 3265 | `	return 1;` |
|     159 | 3266 | `}` |
|       - | 3267 | `/*` |
|       - | 3268 | ` * TRUE when pTok is a PHL IDENTIFIER that php's lexer nevertheless reserves. The` |
|       - | 3269 | `` * alpha operators (`and`, `or`, `xor`, `new`, `clone`, `instanceof`) reach the`` |
|       - | 3270 | `` * parser here as PH7_TK_ID\|PH7_TK_OP, and `readonly`/`callable` are`` |
|       - | 3271 | ` * context-sensitive identifiers — php has a real token for every one of them, so` |
|       - | 3272 | ` * none may stand where its grammar asks for a T_STRING.` |
|       - | 3273 | ` */` |
|     140 | 3274 | `static int GenStateIdIsPhpKeyword(SyToken *pTok)` |
|       5 | 3275 | `{` |
|       - | 3276 | `	static const char *azWords[] = {` |
|       - | 3277 | `		"and","or","xor","new","clone","instanceof","readonly","callable"` |
|       - | 3278 | `	};` |
|       - | 3279 | `	sxu32 i;` |
|     145 | 3280 | `	if( (pTok->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 3281 | `		return 0;` |
|       - | 3282 | `	}` |
|    1265 | 3283 | `	for( i = 0 ; i < SX_ARRAYSIZE(azWords) ; i++ ){` |
|    1125 | 3284 | `		sxu32 n = (sxu32)SyStrlen(azWords[i]);` |
|    1125 | 3285 | `		if( pTok->sData.nByte == n && SyStrnicmp(pTok->sData.zString,azWords[i],n) == 0 ){` |
|     ! 0 | 3286 | `			return 1;` |
|       - | 3287 | `		}` |
|     565 | 3288 | `	}` |
|     145 | 3289 | `	return 0;` |
|      75 | 3290 | `}` |
|       - | 3291 | `/*` |
|       - | 3292 | `` * Consume the optional `as Alias` clause, leaving *pAlias untouched when absent.`` |
|       - | 3293 | ` * php's grammar takes a T_STRING there and nothing else, so a reserved word is a` |
|       - | 3294 | ` * parse error however PHL's lexer happens to have classified it. Returns TRUE` |
|       - | 3295 | ` * when it reported one, and the caller must then abandon the declaration.` |
|       - | 3296 | ` */` |
|     290 | 3297 | `static int GenStateCollectImportAlias(ph7_gen_state *pGen,SyString *pAlias,sxi32 *pRc)` |
|       5 | 3298 | `{` |
|     290 | 3299 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     220 | 3300 | `		&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|     147 | 3301 | `		pGen->pIn++; /* Jump 'as' */` |
|     147 | 3302 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 3303 | `			return 0;` |
|       - | 3304 | `		}` |
|     147 | 3305 | `		if( (pGen->pIn->nType & PH7_TK_ID) && !GenStateIdIsPhpKeyword(pGen->pIn) ){` |
|     145 | 3306 | `			*pAlias = pGen->pIn->sData;` |
|     145 | 3307 | `			pGen->pIn++;` |
|     145 | 3308 | `			return 0;` |
|       - | 3309 | `		}` |
|       3 | 3310 | `		if( GenStateKeywordIsPhpLabel(pGen->pIn) ){` |
|       - | 3311 | ``			/* php spells its scalar types and `self`/`parent` with plain labels,`` |
|       - | 3312 | ``			 * so those ARE legal aliases — `use A\Q as integer;` compiles, and`` |
|       - | 3313 | ``			 * `use A\Q as self;` reaches the special-class-name check instead. */`` |
|     ! 0 | 3314 | `			*pAlias = pGen->pIn->sData;` |
|     ! 0 | 3315 | `			pGen->pIn++;` |
|     ! 0 | 3316 | `			return 0;` |
|       - | 3317 | `		}` |
|       - | 3318 | `		{` |
|       - | 3319 | `			/* php prints a reserved word LOWER-CASED in this message, whatever the` |
|       - | 3320 | ``			 * source spelled: `use A\Q as Default;` reads `unexpected token`` |
|       - | 3321 | ``			 * "default"`. Longest reserved word here is `include_once` (12). */`` |
|       - | 3322 | `			char zLower[16];` |
|       3 | 3323 | `			SyString sTok = pGen->pIn->sData;` |
|       - | 3324 | `			sxu32 i;` |
|       4 | 3325 | `			int bKeyword = ( (pGen->pIn->nType & PH7_TK_KEYWORD) != 0` |
|       2 | 3326 | `				\|\| GenStateIdIsPhpKeyword(pGen->pIn) );` |
|       5 | 3327 | `			int bWord = ( (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|       2 | 3328 | `				&& sTok.nByte <= sizeof(zLower) );` |
|      17 | 3329 | `			for( i = 0 ; bWord && i < sTok.nByte ; i++ ){` |
|      15 | 3330 | `				unsigned char c = (unsigned char)sTok.zString[i];` |
|      15 | 3331 | `				zLower[i] = (char)((c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c);` |
|       8 | 3332 | `			}` |
|       3 | 3333 | `			if( bWord ){` |
|       3 | 3334 | `				SyStringInitFromBuf(&sTok,zLower,sTok.nByte);` |
|       1 | 3335 | `			}` |
|       - | 3336 | ``			/* `die` and `exit` are the same token to php, and it names it `exit`. */`` |
|       3 | 3337 | `			if( bWord && sTok.nByte == 3 && SyMemcmp(sTok.zString,"die",3) == 0 ){` |
|     ! 0 | 3338 | `				SyStringInitFromBuf(&sTok,"exit",sizeof("exit")-1);` |
|     ! 0 | 3339 | `			}` |
|       4 | 3340 | `			*pRc = PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - | 3341 | `				"syntax error, unexpected %s \"%z\", expecting identifier",` |
|       1 | 3342 | `				bKeyword ? "token" : TokenTypeName(pGen->pIn->nType),&sTok);` |
|       - | 3343 | `		}` |
|       3 | 3344 | `		return 1;` |
|       - | 3345 | `	}` |
|     153 | 3346 | `	return 0;` |
|     150 | 3347 | `}` |
|       - | 3348 | `/*` |
|       - | 3349 | `` * Park the cursor on whichever of `}` / `;` ends the group, so a refused member`` |
|       - | 3350 | ` * does not cascade into the ones after it.` |
|       - | 3351 | ` */` |
|     ! 0 | 3352 | `static void GenStateSkipToGroupEnd(ph7_gen_state *pGen)` |
|     ! 0 | 3353 | `{` |
|     ! 0 | 3354 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_CCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 | 3355 | `		pGen->pIn++;` |
|     ! 0 | 3356 | `	}` |
|     ! 0 | 3357 | `}` |
|       - | 3358 | `/*` |
|       - | 3359 | ` * Compile the members of a GROUP use declaration (php 7.0):` |
|       - | 3360 | ` *` |
|       - | 3361 | ` *      use A\{Cee, Dee as D2, Sub\Eee};` |
|       - | 3362 | ` *      use function A\{f, g as h};` |
|       - | 3363 | ` *      use A\{function f, const K, Cee};   // per-member kind, untyped group only` |
|       - | 3364 | ` *` |
|       - | 3365 | `` * pPrefix holds the path before the brace; the cursor sits on `{`.  Each member`` |
|       - | 3366 | `` * is the prefix, a `\`, and the member's own (possibly multi-segment) name.  A`` |
|       - | 3367 | ` * trailing comma is allowed, an empty group is not.` |
|       - | 3368 | ` */` |
|      12 | 3369 | `static sxi32 GenStateCompileGroupUse(ph7_gen_state *pGen,SyBlob *pPrefix,int iUseType,sxu32 nLine)` |
|       2 | 3370 | `{` |
|       - | 3371 | `	SyBlob sPath;` |
|      14 | 3372 | `	sxi32 rc = SXRET_OK;` |
|      14 | 3373 | `	pGen->pIn++; /* Jump '{' */` |
|      14 | 3374 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|      13 | 3375 | `	for(;;){` |
|      28 | 3376 | `		int iMemberType = iUseType;` |
|       - | 3377 | `		SyString sAlias;` |
|       - | 3378 | `		SyToken *pLast;` |
|       - | 3379 | ``		/* `function`/`const` may qualify a single member, but only inside a`` |
|       - | 3380 | ``		 * group that is not itself typed (php rejects `use function A\{const C}`). */`` |
|      28 | 3381 | `		if( iUseType == 0 && pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       5 | 3382 | `			sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|       5 | 3383 | `			if( nKey == PH7_TKWRD_FUNCTION ){` |
|       3 | 3384 | `				iMemberType = 1;` |
|       3 | 3385 | `				pGen->pIn++;` |
|       4 | 3386 | `			}else if( nKey == PH7_TKWRD_CONST ){` |
|       3 | 3387 | `				iMemberType = 2;` |
|       3 | 3388 | `				pGen->pIn++;` |
|       1 | 3389 | `			}` |
|       2 | 3390 | `		}` |
|      28 | 3391 | `		SyBlobReset(&sPath);` |
|      28 | 3392 | `		SyBlobAppend(&sPath,SyBlobData(pPrefix),SyBlobLength(pPrefix));` |
|      28 | 3393 | `		if( GenStateUseRejectNsRelName(&(*pGen),nLine,` |
|       - | 3394 | `				", expecting identifier or namespaced name or \"function\" or \"const\"",&rc) ){` |
|     ! 0 | 3395 | `			GenStateSkipToGroupEnd(&(*pGen));` |
|     ! 0 | 3396 | `			break;` |
|       - | 3397 | `		}` |
|      28 | 3398 | `		pLast = GenStateCollectNsPath(pGen,&sPath);` |
|      28 | 3399 | `		if( pLast == 0 ){` |
|       - | 3400 | ``			/* No member name: `use A\{};` or a stray token.  Report once, then`` |
|       - | 3401 | `			 * skip to the end of the group so the statement does not cascade. */` |
|     ! 0 | 3402 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - | 3403 | `				"syntax error, unexpected %s \"%z\", expecting identifier",` |
|     ! 0 | 3404 | `				TokenTypeName(pGen->pIn < pGen->pEnd ? pGen->pIn->nType : 0),` |
|     ! 0 | 3405 | `				pGen->pIn < pGen->pEnd ? &pGen->pIn->sData : 0);` |
|     ! 0 | 3406 | `			GenStateSkipToGroupEnd(&(*pGen));` |
|     ! 0 | 3407 | `			break;` |
|       - | 3408 | `		}` |
|      28 | 3409 | `		sAlias = pLast->sData; /* Default alias is the member's last component */` |
|      28 | 3410 | `		if( GenStateCollectImportAlias(pGen,&sAlias,&rc) ){` |
|     ! 0 | 3411 | `			GenStateSkipToGroupEnd(&(*pGen));` |
|     ! 0 | 3412 | `			break;` |
|       - | 3413 | `		}` |
|      28 | 3414 | `		rc = GenStateAddImport(&(*pGen),iMemberType,&sPath,&sAlias,nLine);` |
|      28 | 3415 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3416 | `			break;` |
|       - | 3417 | `		}` |
|      28 | 3418 | `		rc = SXRET_OK;` |
|      28 | 3419 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|      18 | 3420 | `			pGen->pIn++;` |
|      18 | 3421 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) ){` |
|       3 | 3422 | `				break; /* Trailing comma before the closing brace */` |
|       - | 3423 | `			}` |
|      16 | 3424 | `			continue;` |
|       - | 3425 | `		}` |
|      12 | 3426 | `		break;` |
|     ! 0 | 3427 | `	}` |
|      14 | 3428 | `	SyBlobRelease(&sPath);` |
|      14 | 3429 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 3430 | `		return SXERR_ABORT;` |
|       - | 3431 | `	}` |
|      14 | 3432 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) ){` |
|      14 | 3433 | `		pGen->pIn++; /* Jump '}' */` |
|       6 | 3434 | `	}` |
|      14 | 3435 | `	return SXRET_OK;` |
|       8 | 3436 | `}` |
|       - | 3437 | `/*` |
|       - | 3438 | ` * Compile the 'use' statement` |
|       - | 3439 | ` * According to the PHP language reference manual` |
|       - | 3440 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - | 3441 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - | 3442 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - | 3443 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - | 3444 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - | 3445 | ` *  a function or constant is not supported.` |
|       - | 3446 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - | 3447 | ` * NOTE` |
|       - | 3448 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - | 3449 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - | 3450 | ` */` |
|     280 | 3451 | `PH7_PRIVATE sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       5 | 3452 | `{` |
|       - | 3453 | `	sxu32 nLine;` |
|       - | 3454 | `	sxi32 rc;` |
|       - | 3455 | `	SyBlob sPath;` |
|       - | 3456 | `	SyString sAlias;` |
|       - | 3457 | `	SyToken *pLast;` |
|       - | 3458 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|     285 | 3459 | `	nLine = pGen->pIn->nLine;` |
|     285 | 3460 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|       - | 3461 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|     285 | 3462 | `	iUseType = 0;` |
|     285 | 3463 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      73 | 3464 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|      73 | 3465 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|      39 | 3466 | `			iUseType = 1;` |
|      39 | 3467 | `			pGen->pIn++;` |
|      56 | 3468 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|      37 | 3469 | `			iUseType = 2;` |
|      37 | 3470 | `			pGen->pIn++;` |
|      16 | 3471 | `		}` |
|      34 | 3472 | `	}` |
|     285 | 3473 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - | 3474 | `	/* Process one or more use declarations separated by commas */` |
|     141 | 3475 | `	for(;;){` |
|     287 | 3476 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 3477 | `			break;` |
|       - | 3478 | `		}` |
|     287 | 3479 | `		SyBlobReset(&sPath);` |
|     287 | 3480 | `		if( GenStateUseRejectNsRelName(&(*pGen),nLine,"",&rc) ){` |
|       3 | 3481 | `			SyBlobRelease(&sPath);` |
|       3 | 3482 | `			GenStateSkipToStatementEnd(&(*pGen));` |
|       3 | 3483 | `			return ( rc == SXERR_ABORT ) ? SXERR_ABORT : SXRET_OK;` |
|       - | 3484 | `		}` |
|       - | 3485 | `		/* Collect the full namespace path */` |
|     285 | 3486 | `		pLast = GenStateCollectNsPath(pGen,&sPath);` |
|     285 | 3487 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) && SyBlobLength(&sPath) > 0 ){` |
|       - | 3488 | `			/* GROUP declaration: what was collected is the shared prefix.  php` |
|       - | 3489 | `			 * does not let a group be comma-combined with another declaration,` |
|       - | 3490 | `			 * so the members close the statement. */` |
|      14 | 3491 | `			rc = GenStateCompileGroupUse(&(*pGen),&sPath,iUseType,nLine);` |
|      14 | 3492 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3493 | `				SyBlobRelease(&sPath);` |
|     ! 0 | 3494 | `				return SXERR_ABORT;` |
|       - | 3495 | `			}` |
|      14 | 3496 | `			break;` |
|       - | 3497 | `		}` |
|     273 | 3498 | `		if( pLast == 0 ){` |
|       - | 3499 | `			/* Empty path */` |
|       6 | 3500 | `			break;` |
|       - | 3501 | `		}` |
|       - | 3502 | `		/* Default alias is the last component of the path */` |
|     269 | 3503 | `		sAlias = pLast->sData;` |
|       - | 3504 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|     269 | 3505 | `		if( GenStateCollectImportAlias(pGen,&sAlias,&rc) ){` |
|       3 | 3506 | `			SyBlobRelease(&sPath);` |
|       3 | 3507 | `			GenStateSkipToStatementEnd(&(*pGen));` |
|       3 | 3508 | `			return ( rc == SXERR_ABORT ) ? SXERR_ABORT : SXRET_OK;` |
|       - | 3509 | `		}` |
|     267 | 3510 | `		rc = GenStateAddImport(&(*pGen),iUseType,&sPath,&sAlias,nLine);` |
|     267 | 3511 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3512 | `			SyBlobRelease(&sPath);` |
|     ! 0 | 3513 | `			return SXERR_ABORT;` |
|       - | 3514 | `		}` |
|       - | 3515 | `		/* Check for comma (multiple use declarations) */` |
|     267 | 3516 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 | 3517 | `			pGen->pIn++;` |
|       2 | 3518 | `		}else{` |
|     135 | 3519 | `			break;` |
|       - | 3520 | `		}` |
|       1 | 3521 | `	}` |
|     281 | 3522 | `	SyBlobRelease(&sPath);` |
|     281 | 3523 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 | 3524 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 | 3525 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 | 3526 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3527 | `			return SXERR_ABORT;` |
|       - | 3528 | `		}` |
|       1 | 3529 | `	}` |
|     281 | 3530 | `	return SXRET_OK;` |
|     145 | 3531 | `}` |
|       - | 3532 | `/*` |
|       - | 3533 | ` * Compile the stupid 'declare' language construct.` |
|       - | 3534 | ` *` |
|       - | 3535 | ` * According to the PHP language reference manual.` |
|       - | 3536 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - | 3537 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - | 3538 | ` *  declare (directive)` |
|       - | 3539 | ` *   statement` |
|       - | 3540 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - | 3541 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - | 3542 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - | 3543 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - | 3544 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - | 3545 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - | 3546 | ` * <?php` |
|       - | 3547 | ` * // these are the same:` |
|       - | 3548 | ` * // you can use this:` |
|       - | 3549 | ` * declare(ticks=1) {` |
|       - | 3550 | ` *   // entire script here` |
|       - | 3551 | ` * }` |
|       - | 3552 | ` * // or you can use this:` |
|       - | 3553 | ` * declare(ticks=1);` |
|       - | 3554 | ` * // entire script here` |
|       - | 3555 | ` * ?>` |
|       - | 3556 | ` *` |
|       - | 3557 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - | 3558 | ` */` |
|       - | 3559 | `/*` |
|       - | 3560 | ` * Match a directive name against a known literal (case-insensitive).` |
|       - | 3561 | ` */` |
|     108 | 3562 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|       5 | 3563 | `{` |
|     162 | 3564 | `	return SyStringLength(pName) == nWant` |
|     108 | 3565 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|       5 | 3566 | `}` |
|       - | 3567 |  |
|      58 | 3568 | `PH7_PRIVATE sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       5 | 3569 | `{` |
|      63 | 3570 | `	sxu32 nLine = pGen->pIn->nLine;` |
|      63 | 3571 | `	SyToken *pBodyEnd = 0;` |
|       - | 3572 | `	SyToken *pBodyStart;` |
|       - | 3573 | `	SyToken *pCursor;` |
|       - | 3574 | `	int bHasStrictTypes;` |
|       - | 3575 | `	int bBlockForm;` |
|       - | 3576 | `	int bPlacementOk;` |
|       - | 3577 | `	sxi32 rc;` |
|      63 | 3578 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|      63 | 3579 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       6 | 3580 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|       6 | 3581 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3582 | `			return SXERR_ABORT;` |
|       - | 3583 | `		}` |
|       6 | 3584 | `		goto Synchro;` |
|       - | 3585 | `	}` |
|      59 | 3586 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|      59 | 3587 | `	pBodyStart = pGen->pIn;` |
|       - | 3588 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|      59 | 3589 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|      59 | 3590 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 | 3591 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\")\"");` |
|     ! 0 | 3592 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3593 | `			return SXERR_ABORT;` |
|       - | 3594 | `		}` |
|     ! 0 | 3595 | `		return SXRET_OK;` |
|       - | 3596 | `	}` |
|       - | 3597 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|       - | 3598 | `	 * now delimits the comma-separated directive list. */` |
|      59 | 3599 | `	pGen->pIn = &pBodyEnd[1];` |
|      59 | 3600 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|     ! 0 | 3601 | `		if( pGen->pIn >= pGen->pEnd && GenStateAtChunkEofStmt(pGen) ){` |
|       - | 3602 | `			/* Ran out of input: php's parser has an unfinished statement and names` |
|       - | 3603 | ``			 * that, not a sentence about `declare` (the shared end-of-input check`` |
|       - | 3604 | `			 * in compile.c words every other statement the same way). */` |
|     ! 0 | 3605 | `			rc = PH7_GenSyntaxError(&(*pGen),0,0);` |
|     ! 0 | 3606 | `		}else{` |
|     ! 0 | 3607 | `			rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"declare: Expecting ';' or '{' after directive");` |
|       - | 3608 | `		}` |
|     ! 0 | 3609 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3610 | `			return SXERR_ABORT;` |
|       - | 3611 | `		}` |
|     ! 0 | 3612 | `	}` |
|      59 | 3613 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|      59 | 3614 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|      59 | 3615 | `	bHasStrictTypes = 0;` |
|       - | 3616 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|       - | 3617 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|       - | 3618 | `	 * directive appears anywhere in the list, before validating values. */` |
|      59 | 3619 | `	pCursor = pBodyStart;` |
|      71 | 3620 | `	while( pCursor < pBodyEnd ){` |
|      67 | 3621 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      59 | 3622 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|      55 | 3623 | `				bHasStrictTypes = 1;` |
|      55 | 3624 | `				break;` |
|       - | 3625 | `			}` |
|       2 | 3626 | `		}` |
|      14 | 3627 | `		pCursor++;` |
|       2 | 3628 | `	}` |
|      59 | 3629 | `	if( bHasStrictTypes && bBlockForm ){` |
|       3 | 3630 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3631 | `			"strict_types declaration must not use block mode");` |
|       3 | 3632 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 | 3633 | `		return SXRET_OK;` |
|       - | 3634 | `	}` |
|      57 | 3635 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|       6 | 3636 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3637 | `			"strict_types declaration must be the very first statement in the script");` |
|       6 | 3638 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       6 | 3639 | `		return SXRET_OK;` |
|       - | 3640 | `	}` |
|       - | 3641 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|      53 | 3642 | `	pCursor = pBodyStart;` |
|     101 | 3643 | `	while( pCursor < pBodyEnd ){` |
|       - | 3644 | `		SyToken *pNameTok;` |
|       - | 3645 | `		SyToken *pEqTok;` |
|       - | 3646 | `		SyToken *pValTok;` |
|       - | 3647 | `		SyString *pDirName;` |
|       - | 3648 | `		int bIsStrict;` |
|       - | 3649 | `		int iStrictValue;` |
|      55 | 3650 | `		pNameTok = pCursor;` |
|      55 | 3651 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 3652 | `			rc = PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|       - | 3653 | `				"declare: Expecting a directive name");` |
|     ! 0 | 3654 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 | 3655 | `			return SXRET_OK;` |
|       - | 3656 | `		}` |
|      55 | 3657 | `		pEqTok = pNameTok + 1;` |
|      55 | 3658 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|     ! 0 | 3659 | `			rc = PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|       - | 3660 | `				"declare: Expecting '=' after directive name");` |
|     ! 0 | 3661 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 | 3662 | `			return SXRET_OK;` |
|       - | 3663 | `		}` |
|      55 | 3664 | `		pValTok = pEqTok + 1;` |
|      55 | 3665 | `		if( pValTok >= pBodyEnd ){` |
|     ! 0 | 3666 | `			rc = PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|       - | 3667 | `				"declare: Expecting value after '='");` |
|     ! 0 | 3668 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 | 3669 | `			return SXRET_OK;` |
|       - | 3670 | `		}` |
|      55 | 3671 | `		pDirName = &pNameTok->sData;` |
|      55 | 3672 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|      55 | 3673 | `		if( bIsStrict ){` |
|       - | 3674 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|       - | 3675 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|      51 | 3676 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|     ! 0 | 3677 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3678 | `					"declare(strict_types) value must be a literal");` |
|     ! 0 | 3679 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 | 3680 | `				return SXRET_OK;` |
|       - | 3681 | `			}` |
|      51 | 3682 | `			iStrictValue = -1;` |
|      51 | 3683 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|      51 | 3684 | `				const char *zv = SyStringData(&pValTok->sData);` |
|      51 | 3685 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|      51 | 3686 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|      49 | 3687 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|      23 | 3688 | `			}` |
|      51 | 3689 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|       3 | 3690 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3691 | `					"strict_types declaration must have 0 or 1 as its value");` |
|       3 | 3692 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 | 3693 | `				return SXRET_OK;` |
|       - | 3694 | `			}` |
|      49 | 3695 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|      28 | 3696 | `		}else if( DeclareNameIs(pDirName, "encoding", sizeof("encoding")-1) ){` |
|       - | 3697 | `			/* php always ignores declare(encoding=...) unless it was built with` |
|       - | 3698 | `			 * Zend multibyte, and says so in these exact words. */` |
|       3 | 3699 | `			PH7_GenCompileError(&(*pGen),E_WARNING,nLine,` |
|       - | 3700 | `				"declare(encoding=...) ignored because Zend multibyte feature is turned off by settings");` |
|       1 | 3701 | `		}else{` |
|       - | 3702 | `			/* Other directives (ticks and friends) are accepted as no-ops.` |
|       - | 3703 | `			 * This used to emit a NOTICE naming the upstream engine and its` |
|       - | 3704 | `			 * version ("the declare construct is a no-op in the current release` |
|       - | 3705 | `			 * of the PH7(2.1.4) engine") — php prints nothing at all for` |
|       - | 3706 | ``			 * `declare(ticks=1)`, and leaking the old engine's branding into`` |
|       - | 3707 | `			 * user-visible diagnostics was wrong on its own. */` |
|       - | 3708 | `		}` |
|      53 | 3709 | `		pCursor = pValTok + 1;` |
|       - | 3710 | `		/* Consume separating comma (or end). */` |
|      53 | 3711 | `		if( pCursor < pBodyEnd ){` |
|       3 | 3712 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 | 3713 | `				rc = PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|       - | 3714 | `					"declare: Expecting ',' or ')' after directive value");` |
|     ! 0 | 3715 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 | 3716 | `				return SXRET_OK;` |
|       - | 3717 | `			}` |
|       3 | 3718 | `			pCursor++;` |
|       1 | 3719 | `		}` |
|       5 | 3720 | `	}` |
|       - | 3721 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|       - | 3722 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|       - | 3723 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|      51 | 3724 | `	return SXRET_OK;` |
|       2 | 3725 | `Synchro:` |
|       - | 3726 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      16 | 3727 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      12 | 3728 | `		pGen->pIn++;` |
|       2 | 3729 | `	}` |
|       6 | 3730 | `	return SXRET_OK;` |
|      34 | 3731 | `}` |
|       - | 3732 | `/*` |
|       - | 3733 | ` * Compile a class constant.` |
|       - | 3734 | ` * According to the PHP language reference manual` |
|       - | 3735 | ` *  Class Constants` |
|       - | 3736 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - | 3737 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - | 3738 | ` *   you don't use the $ symbol to declare or use them.` |
|       - | 3739 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - | 3740 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - | 3741 | ` *   It's also possible for interfaces to have constants.` |
|       - | 3742 | ` * Symisc eXtension.` |
|       - | 3743 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - | 3744 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 3745 | ` *  Example:` |
|       - | 3746 | ` *   class Test{` |
|       - | 3747 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 3748 | ` *   };` |
|       - | 3749 | ` *   var_dump(TEST::MyConst);` |
|       - | 3750 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 3751 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 3752 | ` */` |
|       - | 3753 | `/*` |
|       - | 3754 | ` * Exception handling.` |
|       - | 3755 | ` *  According to the PHP language reference manual` |
|       - | 3756 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - | 3757 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - | 3758 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - | 3759 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - | 3760 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - | 3761 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - | 3762 | ` *    (or re-thrown) within a catch block.` |
|       - | 3763 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - | 3764 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - | 3765 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - | 3766 | ` *    been defined with set_exception_handler().` |
|       - | 3767 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - | 3768 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - | 3769 | ` */` |
|       - | 3770 | `/*` |
|       - | 3771 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - | 3772 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - | 3773 | ` * indicates failure.` |
|       - | 3774 | ` */` |
|   88153 | 3775 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 | 3776 | `{` |
|       - | 3777 | ``	/* php decides throwability at RUNTIME: `throw 5` compiles and raises the`` |
|       - | 3778 | `	 * catchable Error "Can only throw objects" when it executes, and a` |
|       - | 3779 | `	 * non-Throwable object raises "Cannot throw objects that do not implement` |
|       - | 3780 | `	 * Throwable". This used to whitelist a handful of expression shapes and` |
|       - | 3781 | `	 * reject the rest at compile time as a "friendlier" error, which meant the` |
|       - | 3782 | `	 * program never ran and no catch could ever see it — and it was inconsistent` |
|       - | 3783 | ``	 * anyway, since `throw $v` (a variable holding an int) always compiled and`` |
|       - | 3784 | `	 * fell through to the same runtime path. OP_THROW now owns the whole rule,` |
|       - | 3785 | ``	 * so accept any expression here; `throw;` with no operand is still rejected`` |
|       - | 3786 | `	 * by PH7_CompileThrow's SXERR_EMPTY branch. */` |
|   44017 | 3787 | `	SXUNUSED(pGen);` |
|   44017 | 3788 | `	SXUNUSED(pRoot);` |
|   88158 | 3789 | `	return SXRET_OK;` |
|       5 | 3790 | `}` |
|       - | 3791 | `/*` |
|       - | 3792 | ` * Compile a 'throw' statement.` |
|       - | 3793 | ` * throw: This is how you trigger an exception.` |
|       - | 3794 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - | 3795 | ` */` |
|   88107 | 3796 | `PH7_PRIVATE sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       5 | 3797 | `{` |
|   88112 | 3798 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 3799 | `	GenBlock *pBlock;` |
|       - | 3800 | `	sxu32 nIdx;` |
|       - | 3801 | `	sxi32 rc;` |
|   88112 | 3802 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - | 3803 | `	/* Compile the expression */` |
|   88112 | 3804 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|   88112 | 3805 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 3806 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 | 3807 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3808 | `			return SXERR_ABORT;` |
|       - | 3809 | `		}` |
|     ! 0 | 3810 | `		return SXRET_OK;` |
|       - | 3811 | `	}` |
|   88112 | 3812 | `	pBlock = pGen->pCurrent;` |
|       - | 3813 | `	/* Point to the top most function or try block and emit the forward jump */` |
|  404799 | 3814 | `	while(pBlock->pParent){` |
|  404775 | 3815 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|   88088 | 3816 | `			break;` |
|       - | 3817 | `		}` |
|       - | 3818 | `		/* Point to the parent block */` |
|  316692 | 3819 | `		pBlock = pBlock->pParent;` |
|       5 | 3820 | `	}` |
|       - | 3821 | `	/* Emit the throw instruction */` |
|   88112 | 3822 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - | 3823 | `	/* Emit the jump */` |
|   88112 | 3824 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|   88112 | 3825 | `	return SXRET_OK;` |
|   43999 | 3826 | `}` |
|       - | 3827 | `/*` |
|       - | 3828 | ` * Compile a PHP 8.0 'throw' expression.` |
|       - | 3829 | ` * Called from the expression code generator when a 'throw' keyword is` |
|       - | 3830 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|       - | 3831 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|       - | 3832 | ` * the validator guarantees the operand is a valid exception target.` |
|       - | 3833 | ` */` |
|      46 | 3834 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       4 | 3835 | `{` |
|      50 | 3836 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 3837 | `	GenBlock *pBlock;` |
|       - | 3838 | `	sxu32 nIdx;` |
|       - | 3839 | `	sxi32 rc;` |
|      23 | 3840 | `	(void)iCompileFlag;` |
|      50 | 3841 | `	pGen->pIn++; /* Skip 'throw' */` |
|      50 | 3842 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 3843 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 3844 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 3845 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3846 | `			return SXERR_ABORT;` |
|       - | 3847 | `		}` |
|     ! 0 | 3848 | `		return SXRET_OK;` |
|       - | 3849 | `	}` |
|      50 | 3850 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|      50 | 3851 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 3852 | `		return SXERR_ABORT;` |
|       - | 3853 | `	}` |
|      50 | 3854 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 3855 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 3856 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 3857 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3858 | `			return SXERR_ABORT;` |
|       - | 3859 | `		}` |
|     ! 0 | 3860 | `		return SXRET_OK;` |
|       - | 3861 | `	}` |
|       - | 3862 | `	/* Walk up to nearest exception/function block for the jump target */` |
|      50 | 3863 | `	pBlock = pGen->pCurrent;` |
|      76 | 3864 | `	while( pBlock->pParent ){` |
|      66 | 3865 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|      40 | 3866 | `			break;` |
|       - | 3867 | `		}` |
|      28 | 3868 | `		pBlock = pBlock->pParent;` |
|       2 | 3869 | `	}` |
|      50 | 3870 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|      50 | 3871 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|      50 | 3872 | `	return SXRET_OK;` |
|      27 | 3873 | `}` |
|       - | 3874 | `/*` |
|       - | 3875 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|       - | 3876 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|       - | 3877 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|       - | 3878 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|       - | 3879 | ` * compile error propagated from the parser.` |
|       - | 3880 | ` */` |
|      72 | 3881 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|       5 | 3882 | `{` |
|       - | 3883 | `	SyString sClassName;` |
|       - | 3884 | `	SyToken *pToken;` |
|       - | 3885 | `	SyString *pName;` |
|       - | 3886 | `	char *zDup;` |
|       - | 3887 | `	sxi32 rc;` |
|      77 | 3888 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|      77 | 3889 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|      77 | 3890 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|       - | 3891 | `	/* Inline catches compile into the function's own container; pByteCode stays NULL. */` |
|      77 | 3892 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 | 3893 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 3894 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 3895 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 3896 | `		return SXERR_INVALID;` |
|       - | 3897 | `	}` |
|      77 | 3898 | `	pGen->pIn++; /* '(' */` |
|      36 | 3899 | `	for(;;){` |
|       - | 3900 | `		SyBlob sResolved;` |
|      77 | 3901 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      77 | 3902 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 | 3903 | `			SyBlobRelease(&sResolved);` |
|     ! 0 | 3904 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 3905 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 3906 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 3907 | `			return SXERR_INVALID;` |
|       - | 3908 | `		}` |
|     113 | 3909 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      72 | 3910 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|      77 | 3911 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|      77 | 3912 | `		SyBlobRelease(&sResolved);` |
|      77 | 3913 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|      77 | 3914 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|      77 | 3915 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      72 | 3916 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|       5 | 3917 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|     ! 0 | 3918 | `			pGen->pIn++; continue;` |
|       - | 3919 | `		}` |
|      77 | 3920 | `		break;` |
|     ! 0 | 3921 | `	}` |
|       - | 3922 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|       - | 3923 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding. */` |
|      77 | 3924 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|       3 | 3925 | `		pGen->pIn++; /* ')' */` |
|       3 | 3926 | `		return SXRET_OK;` |
|       - | 3927 | `	}` |
|      70 | 3928 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|      75 | 3929 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 3930 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 3931 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 3932 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 3933 | `		return SXERR_INVALID;` |
|       - | 3934 | `	}` |
|      75 | 3935 | `	pGen->pIn++; /* '$' */` |
|      75 | 3936 | `	pName = &pGen->pIn->sData;` |
|      75 | 3937 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      75 | 3938 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|      75 | 3939 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|      75 | 3940 | `	pGen->pIn++;` |
|      75 | 3941 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 | 3942 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 3943 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 3944 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 3945 | `		return SXERR_INVALID;` |
|       - | 3946 | `	}` |
|      75 | 3947 | `	pGen->pIn++; /* ')' */` |
|      75 | 3948 | `	return SXRET_OK;` |
|      41 | 3949 | `}` |
|       - | 3950 | `/*` |
|       - | 3951 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|       - | 3952 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|       - | 3953 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|       - | 3954 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|       - | 3955 | ` * VmThrowException):` |
|       - | 3956 | ` *` |
|       - | 3957 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|       - | 3958 | ` *    <try body>` |
|       - | 3959 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|       - | 3960 | ` *    JMP  -> finally\|end` |
|       - | 3961 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|       - | 3962 | ` *    <catch body>` |
|       - | 3963 | ` *    JMP  -> finally\|end` |
|       - | 3964 | ` *    ... more catches ...` |
|       - | 3965 | ` *  Lfin: <finally body>` |
|       - | 3966 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|       - | 3967 | ` *  Lend:` |
|       - | 3968 | ` */` |
|     130 | 3969 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|       5 | 3970 | `{` |
|     135 | 3971 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 3972 | `	GenBlock *pTry;` |
|       - | 3973 | `	VmInstr *pInstr;` |
|     135 | 3974 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|       - | 3975 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|       - | 3976 | `	sxi32 rc;` |
|     135 | 3977 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|       - | 3978 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION; passed at` |
|       - | 3979 | `	 * ENTRY so GenStateEnterBlock can classify the scope with it) */` |
|     200 | 3980 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),` |
|      65 | 3981 | `		pException,&pTry);` |
|     135 | 3982 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|     135 | 3983 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|     135 | 3984 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|     135 | 3985 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     135 | 3986 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|     135 | 3987 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|     135 | 3988 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|       - | 3989 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|     135 | 3990 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|     135 | 3991 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|     135 | 3992 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|     135 | 3993 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 3994 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|     135 | 3995 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|       - | 3996 | `	/* Catch clauses (inline) */` |
|     135 | 3997 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     130 | 3998 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|      77 | 3999 | `		sxu32 k = 0;` |
|     108 | 4000 | `		for(;;){` |
|       - | 4001 | `			ph7_exception_block sCatch;` |
|       - | 4002 | `			GenBlock *pCatchBlk;` |
|     149 | 4003 | `			sxu32 idxJmp = 0;` |
|     144 | 4004 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     137 | 4005 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|      41 | 4006 | `				break;` |
|       - | 4007 | `			}` |
|      77 | 4008 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|      77 | 4009 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      77 | 4010 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|      77 | 4011 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|      77 | 4012 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|       - | 4013 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|       - | 4014 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|       - | 4015 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump).` |
|       - | 4016 | `			 * Passed at ENTRY: GenStateEnterBlock reads it to classify the block's scope. */` |
|     113 | 4017 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),` |
|      36 | 4018 | `				pException,&pCatchBlk);` |
|      77 | 4019 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      77 | 4020 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      77 | 4021 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      77 | 4022 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|      77 | 4023 | `			GenStateLeaveBlock(&(*pGen),0);` |
|       - | 4024 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|       - | 4025 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|      77 | 4026 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|      77 | 4027 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|      77 | 4028 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|      77 | 4029 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|      77 | 4030 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      77 | 4031 | `			k++;` |
|       5 | 4032 | `		}` |
|      36 | 4033 | `	}` |
|       - | 4034 | `	/* Finally (inline) */` |
|     135 | 4035 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     106 | 4036 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 4037 | `		GenBlock *pFinBlk;` |
|      69 | 4038 | `		pGen->pIn++; /* Jump 'finally' */` |
|      69 | 4039 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|     101 | 4040 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FINALLY,` |
|      32 | 4041 | `			PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|      69 | 4042 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      69 | 4043 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      69 | 4044 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      69 | 4045 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|      69 | 4046 | `		GenStateLeaveBlock(&(*pGen),0);` |
|      69 | 4047 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|      69 | 4048 | `		pException->iHasFinally = 1;` |
|      32 | 4049 | `	}` |
|     135 | 4050 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|     135 | 4051 | `	pException->iInlined = 1;` |
|       - | 4052 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|       - | 4053 | `	{` |
|     135 | 4054 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|       - | 4055 | `		sxu32 *aJ; sxu32 n;` |
|     135 | 4056 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|     135 | 4057 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|     135 | 4058 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|     207 | 4059 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|      77 | 4060 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|      77 | 4061 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|      41 | 4062 | `		}` |
|       - | 4063 | `	}` |
|     135 | 4064 | `	SySetRelease(&aCatchJmp);` |
|     135 | 4065 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|     ! 0 | 4066 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|     ! 0 | 4067 | `	}` |
|     135 | 4068 | `	return SXRET_OK;` |
|      70 | 4069 | `}` |
|       - | 4070 | `/*` |
|       - | 4071 | ` * Compile a 'catch' block.` |
|       - | 4072 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - | 4073 | ` * an object containing the exception information.` |
|       - | 4074 | ` */` |
|    4702 | 4075 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       5 | 4076 | `{` |
|    4707 | 4077 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4078 | `	ph7_exception_block sCatch;` |
|       - | 4079 | `	SySet *pInstrContainer;` |
|       - | 4080 | `	SyString sClassName;` |
|       - | 4081 | `	GenBlock *pCatch;` |
|       - | 4082 | `	SyToken *pToken;` |
|       - | 4083 | `	SyString *pName;` |
|       - | 4084 | `	char *zDup;` |
|       - | 4085 | `	sxi32 rc;` |
|    4707 | 4086 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - | 4087 | `	/* Zero the structure */` |
|    4707 | 4088 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - | 4089 | `	/* Initialize fields */` |
|    4707 | 4090 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|       - | 4091 | `	/* The catch body gets its own bytecode array, allocated (not embedded) so its address` |
|       - | 4092 | `	 * survives both this stack frame and any later growth of pException->sEntry — a` |
|       - | 4093 | `	 * break/continue inside the body records it in its JumpFixup (see JumpFixup). */` |
|    4707 | 4094 | `	sCatch.pByteCode = (SySet *)SyMemBackendAlloc(&pException->pVm->sAllocator,sizeof(SySet));` |
|    4707 | 4095 | `	if( sCatch.pByteCode == 0 ){` |
|     ! 0 | 4096 | `		goto Mem;` |
|       - | 4097 | `	}` |
|    4707 | 4098 | `	SySetInit(sCatch.pByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|    4707 | 4099 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|       - | 4100 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 4101 | `			pToken = pGen->pIn;` |
|     ! 0 | 4102 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 4103 | `				pToken--;` |
|     ! 0 | 4104 | `			}` |
|     ! 0 | 4105 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 4106 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 4107 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 4108 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4109 | `				return SXERR_ABORT;` |
|       - | 4110 | `			}` |
|     ! 0 | 4111 | `			return SXERR_INVALID;` |
|       - | 4112 | `	}` |
|       - | 4113 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|    4707 | 4114 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|    2364 | 4115 | `	for(;;){` |
|       - | 4116 | `		SyBlob sResolved;` |
|    4741 | 4117 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    4741 | 4118 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       6 | 4119 | `			SyBlobRelease(&sResolved);` |
|       6 | 4120 | `			pToken = pGen->pIn;` |
|       6 | 4121 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 4122 | `				pToken--;` |
|     ! 0 | 4123 | `			}` |
|       8 | 4124 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 4125 | `				"syntax error, unexpected %s \"%z\"",` |
|       2 | 4126 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       6 | 4127 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4128 | `				return SXERR_ABORT;` |
|       - | 4129 | `			}` |
|       6 | 4130 | `			return SXERR_INVALID;` |
|       - | 4131 | `		}` |
|       - | 4132 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|       - | 4133 | `		 * transient SyBlob allocation. */` |
|    7099 | 4134 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    4732 | 4135 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|    4737 | 4136 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|    4737 | 4137 | `		SyBlobRelease(&sResolved);` |
|    4737 | 4138 | `		if( zDup == 0 ){` |
|     ! 0 | 4139 | `			goto Mem;` |
|       - | 4140 | `		}` |
|    4737 | 4141 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|    4737 | 4142 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 4143 | `			goto Mem;` |
|       - | 4144 | `		}` |
|       - | 4145 | `		/* Check for '\|' (multi-catch separator) */` |
|    4732 | 4146 | `		if( pGen->pIn < pGen->pEnd &&` |
|    4732 | 4147 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|      39 | 4148 | `			pGen->pIn->sData.nByte == 1 &&` |
|      34 | 4149 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|      37 | 4150 | `			pGen->pIn++; /* Consume the '\|' */` |
|      37 | 4151 | `			continue;` |
|       - | 4152 | `		}` |
|    4703 | 4153 | `		break;` |
|     ! 0 | 4154 | `	}` |
|       - | 4155 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|       - | 4156 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding;` |
|       - | 4157 | `	 * jump straight to compiling the block below. */` |
|    4703 | 4158 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) /*)*/ ){` |
|       7 | 4159 | `		goto CatchBody;` |
|       - | 4160 | `	}` |
|    4692 | 4161 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|    4697 | 4162 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 4163 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 4164 | `			pToken = pGen->pIn;` |
|     ! 0 | 4165 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 4166 | `				pToken--;` |
|     ! 0 | 4167 | `			}` |
|     ! 0 | 4168 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 4169 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 4170 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 4171 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4172 | `				return SXERR_ABORT;` |
|       - | 4173 | `			}` |
|     ! 0 | 4174 | `			return SXERR_INVALID;` |
|       - | 4175 | `	}` |
|    4697 | 4176 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - | 4177 | `	/* Duplicate instance name */` |
|    4697 | 4178 | `	pName = &pGen->pIn->sData;` |
|    4697 | 4179 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    4697 | 4180 | `	if( zDup == 0 ){` |
|     ! 0 | 4181 | `		goto Mem;` |
|       - | 4182 | `	}` |
|    4697 | 4183 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|    4697 | 4184 | `	pGen->pIn++;` |
|    2353 | 4185 | `CatchBody:` |
|    4703 | 4186 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - | 4187 | `		/* Unexpected token,break immediately */` |
|     ! 0 | 4188 | `		pToken = pGen->pIn;` |
|     ! 0 | 4189 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 | 4190 | `			pToken--;` |
|     ! 0 | 4191 | `		}` |
|     ! 0 | 4192 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 4193 | `			"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 4194 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 4195 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4196 | `			return SXERR_ABORT;` |
|       - | 4197 | `		}` |
|     ! 0 | 4198 | `		return SXERR_INVALID;` |
|       - | 4199 | `	}` |
|       - | 4200 | `	/* Compile the block */` |
|    4703 | 4201 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - | 4202 | `	/* Create the catch block. GEN_BLOCK_DETACHED: the body below compiles into` |
|       - | 4203 | `	 * sCatch.pByteCode, not into the enclosing function's array. */` |
|    4703 | 4204 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION\|GEN_BLOCK_DETACHED,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|    4703 | 4205 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4206 | `		return SXERR_ABORT;` |
|       - | 4207 | `	}` |
|       - | 4208 | `	/* Swap bytecode container */` |
|    4703 | 4209 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    4703 | 4210 | `	PH7_VmSetByteCodeContainer(pGen->pVm,sCatch.pByteCode);` |
|       - | 4211 | `	/* Compile the block */` |
|    4703 | 4212 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 4213 | `	/* Fix forward jumps now the destination is resolved  */` |
|    4703 | 4214 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 4215 | `	/* Emit the DONE instruction */` |
|    4703 | 4216 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 4217 | `	/* Leave the block */` |
|    4703 | 4218 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 4219 | `	/* Restore the default container */` |
|    4703 | 4220 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 4221 | `	/* Install the catch block */` |
|    4703 | 4222 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|    4703 | 4223 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4224 | `		goto Mem;` |
|       - | 4225 | `	}` |
|    4703 | 4226 | `	return SXRET_OK;` |
|     ! 0 | 4227 | `Mem:` |
|     ! 0 | 4228 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 4229 | `	return SXERR_ABORT;` |
|    2352 | 4230 | `}` |
|       - | 4231 | `/*` |
|       - | 4232 | ` * Compile a 'try' block.` |
|       - | 4233 | ` * A function using an exception should be in a "try" block.` |
|       - | 4234 | ` * If the exception does not trigger, the code will continue` |
|       - | 4235 | ` * as normal. However if the exception triggers, an exception` |
|       - | 4236 | ` * is "thrown".` |
|       - | 4237 | ` */` |
|    4996 | 4238 | `PH7_PRIVATE sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       5 | 4239 | `{` |
|       - | 4240 | `	ph7_exception *pException;` |
|    5001 | 4241 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4242 | `	GenBlock *pTry;` |
|       - | 4243 | `	sxu32 nJmpIdx;` |
|       - | 4244 | `	sxi32 rc;` |
|       - | 4245 | `	/* Create the exception container */` |
|    5001 | 4246 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|    5001 | 4247 | `	if( pException == 0 ){` |
|     ! 0 | 4248 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 | 4249 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 4250 | `		return SXERR_ABORT;` |
|       - | 4251 | `	}` |
|       - | 4252 | `	/* Zero the structure */` |
|    5001 | 4253 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - | 4254 | `	/* Initialize fields */` |
|    5001 | 4255 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|    5001 | 4256 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|    5001 | 4257 | `	pException->iHasFinally = 0;` |
|    5001 | 4258 | `	pException->iFinallyDone = 0;` |
|    5001 | 4259 | `	pException->pVm = pGen->pVm;` |
|       - | 4260 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|       - | 4261 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the DETACHED path`` |
|       - | 4262 | `	 * below — deliberately, not pending migration: it is the proven one, and inlining was` |
|       - | 4263 | ``	 * scoped to generators so no other code path changed. `bInlineTryCatch` is 1 since the`` |
|       - | 4264 | ``	 * inline VM handlers landed, so `bInGenerator` is what actually selects here. */`` |
|    5001 | 4265 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|     135 | 4266 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|       - | 4267 | `	}` |
|       - | 4268 | `	/* Create the try block */` |
|       - | 4269 | `	/* pUserData is the exception context, passed at ENTRY (not assigned after) because` |
|       - | 4270 | `	 * GenStateEnterBlock reads it to classify the block's try/catch scope — see aScope.` |
|       - | 4271 | `	 * It is also what a break/continue crossing this try emits its POP_EXCEPTION with. */` |
|    7300 | 4272 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),` |
|    2429 | 4273 | `		pException,&pTry);` |
|    4871 | 4274 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4275 | `		return SXERR_ABORT;` |
|       - | 4276 | `	}` |
|       - | 4277 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|    4871 | 4278 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - | 4279 | `	/* Fix the jump later when the destination is resolved */` |
|    4871 | 4280 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|    4871 | 4281 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - | 4282 | `	/* Compile the block */` |
|    4871 | 4283 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|    4871 | 4284 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 4285 | `		return SXERR_ABORT;` |
|       - | 4286 | `	}` |
|       - | 4287 | `	/* Fix forward jumps now the destination is resolved */` |
|    4871 | 4288 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 4289 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|    4871 | 4290 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - | 4291 | `	/* Leave the block */` |
|    4871 | 4292 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 4293 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|    4871 | 4294 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|    4864 | 4295 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - | 4296 | `		/* Compile one or more catch blocks */` |
|    4689 | 4297 | `		for(;;){` |
|    9394 | 4298 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    7765 | 4299 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|    2347 | 4300 | `					break;` |
|       - | 4301 | `			}` |
|    4707 | 4302 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|    4707 | 4303 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4304 | `				return SXERR_ABORT;` |
|       - | 4305 | `			}` |
|       5 | 4306 | `		}` |
|    2342 | 4307 | `	}` |
|       - | 4308 | `	/* Compile optional finally block */` |
|    4871 | 4309 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|    2350 | 4310 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 4311 | `		SySet *pInstrContainer;` |
|       - | 4312 | `		GenBlock *pFinBlock;` |
|     259 | 4313 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - | 4314 | `		/* Create the finally block for jump fixup bookkeeping (detached: the body` |
|       - | 4315 | `		 * compiles into pException->sFinally, see GEN_BLOCK_DETACHED). */` |
|     386 | 4316 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION\|GEN_BLOCK_DETACHED\|GEN_BLOCK_FINALLY,` |
|     127 | 4317 | `			PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|     259 | 4318 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 4319 | `			return SXERR_ABORT;` |
|       - | 4320 | `		}` |
|       - | 4321 | `		/* Swap bytecode container */` |
|     259 | 4322 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     259 | 4323 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - | 4324 | `		/* Compile the finally body */` |
|     259 | 4325 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     259 | 4326 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4327 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 4328 | `			return SXERR_ABORT;` |
|       - | 4329 | `		}` |
|       - | 4330 | `		/* Fix forward jumps now the destination is resolved */` |
|     259 | 4331 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 4332 | `		/* Emit DONE to terminate the finally block */` |
|     259 | 4333 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 4334 | `		/* Leave the block */` |
|     259 | 4335 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - | 4336 | `		/* Restore the default container */` |
|     259 | 4337 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     259 | 4338 | `		pException->iHasFinally = 1;` |
|     127 | 4339 | `	}` |
|       - | 4340 | `	/* Must have at least one catch or finally */` |
|    4871 | 4341 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       9 | 4342 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 4343 | `			"Cannot use try without catch or finally");` |
|       9 | 4344 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4345 | `			return SXERR_ABORT;` |
|       - | 4346 | `		}` |
|       3 | 4347 | `	}` |
|    4871 | 4348 | `	return SXRET_OK;` |
|    2499 | 4349 | `}` |
|       - | 4350 | `/*` |
|       - | 4351 | ` * Compile a switch block.` |
|       - | 4352 | ` *  (See block-comment below for more information)` |
|       - | 4353 | ` */` |
|     228 | 4354 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       5 | 4355 | `{` |
|     233 | 4356 | `	sxi32 rc = SXRET_OK;` |
|     233 | 4357 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - | 4358 | `		/* Unexpected token */` |
|     ! 0 | 4359 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|     ! 0 | 4360 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4361 | `			return SXERR_ABORT;` |
|       - | 4362 | `		}` |
|     ! 0 | 4363 | `		pGen->pIn++;` |
|     ! 0 | 4364 | `	}` |
|     233 | 4365 | `	pGen->pIn++;` |
|       - | 4366 | `	/* First instruction to execute in this block. */` |
|     233 | 4367 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - | 4368 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - | 4369 | `	 * or the '}' token */` |
|     351 | 4370 | `	for(;;){` |
|     707 | 4371 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 4372 | `			/* No more input to process */` |
|     ! 0 | 4373 | `			break;` |
|       - | 4374 | `		}` |
|     707 | 4375 | `		rc = SXRET_OK;` |
|     707 | 4376 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     161 | 4377 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      69 | 4378 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - | 4379 | `					/* Unexpected token */` |
|     ! 0 | 4380 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 4381 | `						&pGen->pIn->sData);` |
|     ! 0 | 4382 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 4383 | `						return SXERR_ABORT;` |
|       - | 4384 | `					}` |
|       - | 4385 | `					/* FALL THROUGH */` |
|     ! 0 | 4386 | `				}` |
|      69 | 4387 | `				rc = SXERR_EOF;` |
|      69 | 4388 | `				break;` |
|       - | 4389 | `			}` |
|      51 | 4390 | `		}else{` |
|       - | 4391 | `			sxi32 nKwrd;` |
|       - | 4392 | `			/* Extract the keyword */` |
|     551 | 4393 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     551 | 4394 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      84 | 4395 | `				break;` |
|       - | 4396 | `			}` |
|     393 | 4397 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       7 | 4398 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - | 4399 | `					/* Unexpected token */` |
|     ! 0 | 4400 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 4401 | `						&pGen->pIn->sData);` |
|     ! 0 | 4402 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 4403 | `						return SXERR_ABORT;` |
|       - | 4404 | `					}` |
|       - | 4405 | `					/* FALL THROUGH */` |
|     ! 0 | 4406 | `				}` |
|       - | 4407 | `				/* Block compiled */` |
|       7 | 4408 | `				break;` |
|       - | 4409 | `			}` |
|       - | 4410 | `		}` |
|       - | 4411 | `		/* Compile block */` |
|     479 | 4412 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     479 | 4413 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4414 | `			return SXERR_ABORT;` |
|       - | 4415 | `		}` |
|       5 | 4416 | `	}` |
|     233 | 4417 | `	return rc;` |
|     119 | 4418 | `}` |
|       - | 4419 | `/*` |
|       - | 4420 | ` * Compile a case eXpression.` |
|       - | 4421 | ` *  (See block-comment below for more information)` |
|       - | 4422 | ` */` |
|     178 | 4423 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       5 | 4424 | `{` |
|       - | 4425 | `	SySet *pInstrContainer;` |
|       - | 4426 | `	SyToken *pEnd,*pTmp;` |
|     183 | 4427 | `	sxi32 iNest = 0;` |
|     183 | 4428 | ``	sxi32 iQuesty = 0;  /* `?`s opened in the case expression and still unclosed */`` |
|       - | 4429 | `	sxi32 rc;` |
|       - | 4430 | ``	/* Delimit the expression. The `:` that ends a case label is the one standing`` |
|       - | 4431 | `	 * outside every paren AND outside every open ternary: php reads the whole` |
|       - | 4432 | `` 	 * expression first, so `case \PHP_VERSION_ID < 80100 ? \T_CLASS : \T_ENUM:` `` |
|       - | 4433 | `	 * is one label with three colons' worth of punctuation in it. Stopping at the` |
|       - | 4434 | `` 	 * first colon cut that label at the ternary's, and the leftover `\T_ENUM:` `` |
|       - | 4435 | ``	 * came back as `syntax error, unexpected token ":"` -- it is how nette/utils`` |
|       - | 4436 | `	 * spells its token switch, so no phpstan run got past its own bootstrap. A` |
|       - | 4437 | ``	 * `?:` closes itself here (its two tokens are adjacent), and a named`` |
|       - | 4438 | ``	 * argument's `:` sits at iNest >= 1 where neither test can see it. */`` |
|     183 | 4439 | `	pEnd = pGen->pIn;` |
|     467 | 4440 | `	while( pEnd < pGen->pEnd ){` |
|     467 | 4441 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - | 4442 | `			/* Increment nesting level */` |
|      16 | 4443 | `			iNest++;` |
|     460 | 4444 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - | 4445 | `			/* Decrement nesting level */` |
|      16 | 4446 | `			iNest--;` |
|     443 | 4447 | `		}else if( iNest < 1 && (pEnd->nType & PH7_TK_OP)` |
|     210 | 4448 | `			&& pEnd->sData.nByte == 1 && pEnd->sData.zString[0] == '?' ){` |
|       - | 4449 | ``			/* `??` and `?->` are tokens of their own, so this cannot see them */`` |
|      11 | 4450 | `			iQuesty++;` |
|     434 | 4451 | `		}else if( (pEnd->nType & PH7_TK_COLON) && iNest < 1 ){` |
|     193 | 4452 | `			if( iQuesty < 1 ){` |
|     183 | 4453 | `				break;` |
|       - | 4454 | `			}` |
|      11 | 4455 | `			iQuesty--;` |
|     246 | 4456 | `		}else if( (pEnd->nType & PH7_TK_SEMI/*';'*/) && iNest < 1 ){` |
|     ! 0 | 4457 | `			break;` |
|       - | 4458 | `		}` |
|     289 | 4459 | `		pEnd++;` |
|       5 | 4460 | `	}` |
|     183 | 4461 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 4462 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 | 4463 | `		if( rc == SXERR_ABORT ){` |
|       - | 4464 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4465 | `			return SXERR_ABORT;` |
|       - | 4466 | `		}` |
|     ! 0 | 4467 | `	}` |
|       - | 4468 | `	/* Swap token stream */` |
|     183 | 4469 | `	pTmp = pGen->pEnd;` |
|     183 | 4470 | `	pGen->pEnd = pEnd;` |
|     183 | 4471 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     183 | 4472 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|     183 | 4473 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 4474 | `	/* Emit the done instruction */` |
|     183 | 4475 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     183 | 4476 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 4477 | `	/* Update token stream */` |
|     183 | 4478 | `	pGen->pIn  = pEnd;` |
|     183 | 4479 | `	pGen->pEnd = pTmp;` |
|     183 | 4480 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 4481 | `		return SXERR_ABORT;` |
|       - | 4482 | `	}` |
|     183 | 4483 | `	return SXRET_OK;` |
|      94 | 4484 | `}` |
|       - | 4485 | `/*` |
|       - | 4486 | ` * Compile the smart switch statement.` |
|       - | 4487 | ` * According to the PHP language reference manual` |
|       - | 4488 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - | 4489 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - | 4490 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - | 4491 | ` *  This is exactly what the switch statement is for.` |
|       - | 4492 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - | 4493 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - | 4494 | ` *  of the outer loop, use continue 2.` |
|       - | 4495 | ` *  Note that switch/case does loose comparision.` |
|       - | 4496 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - | 4497 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - | 4498 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - | 4499 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - | 4500 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - | 4501 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - | 4502 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - | 4503 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - | 4504 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - | 4505 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - | 4506 | ` *  list for the next case.` |
|       - | 4507 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - | 4508 | ` *  or floating-point numbers and strings.` |
|       - | 4509 | ` */` |
|      70 | 4510 | `PH7_PRIVATE sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       5 | 4511 | `{` |
|       - | 4512 | `	GenBlock *pSwitchBlock;` |
|       - | 4513 | `	SyToken *pTmp,*pEnd;` |
|       - | 4514 | `	ph7_switch *pSwitch;` |
|       - | 4515 | `	sxu32 nToken;` |
|       - | 4516 | `	sxu32 nLine;` |
|       - | 4517 | `	sxi32 rc;` |
|      75 | 4518 | `	nLine = pGen->pIn->nLine;` |
|       - | 4519 | `	/* Jump the 'switch' keyword */` |
|      75 | 4520 | `	pGen->pIn++;` |
|      75 | 4521 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 4522 | `		/* Syntax error */` |
|     ! 0 | 4523 | `		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 | 4524 | `		if( rc == SXERR_ABORT ){` |
|       - | 4525 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4526 | `			return SXERR_ABORT;` |
|       - | 4527 | `		}` |
|     ! 0 | 4528 | `		goto Synchronize;` |
|       - | 4529 | `	}` |
|       - | 4530 | `	/* Jump the left parenthesis '(' */` |
|      75 | 4531 | `	pGen->pIn++;` |
|      75 | 4532 | `	pEnd = 0; /* cc warning */` |
|       - | 4533 | `	/* Create the loop block */` |
|     110 | 4534 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      35 | 4535 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      75 | 4536 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4537 | `		return SXERR_ABORT;` |
|       - | 4538 | `	}` |
|       - | 4539 | `	/* Delimit the condition */` |
|      75 | 4540 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      75 | 4541 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 4542 | `		/* Empty expression */` |
|     ! 0 | 4543 | `		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 | 4544 | `		if( rc == SXERR_ABORT ){` |
|       - | 4545 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4546 | `			return SXERR_ABORT;` |
|       - | 4547 | `		}` |
|     ! 0 | 4548 | `	}` |
|       - | 4549 | `	/* Swap token streams */` |
|      75 | 4550 | `	pTmp = pGen->pEnd;` |
|      75 | 4551 | `	pGen->pEnd = pEnd;` |
|       - | 4552 | `	/* Compile the expression */` |
|      75 | 4553 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      75 | 4554 | `	if( rc == SXERR_ABORT ){` |
|       - | 4555 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 4556 | `		return SXERR_ABORT;` |
|       - | 4557 | `	}` |
|       - | 4558 | `	/* Update token stream */` |
|      75 | 4559 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 4560 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4561 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 4562 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4563 | `			return SXERR_ABORT;` |
|       - | 4564 | `		}` |
|     ! 0 | 4565 | `		pGen->pIn++;` |
|     ! 0 | 4566 | `	}` |
|      75 | 4567 | `	pGen->pIn  = &pEnd[1];` |
|      75 | 4568 | `	pGen->pEnd = pTmp;` |
|      75 | 4569 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      70 | 4570 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 | 4571 | `			pTmp = pGen->pIn;` |
|     ! 0 | 4572 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 | 4573 | `				pTmp--;` |
|     ! 0 | 4574 | `			}` |
|       - | 4575 | `			/* Unexpected token */` |
|     ! 0 | 4576 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 | 4577 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4578 | `				return SXERR_ABORT;` |
|       - | 4579 | `			}` |
|     ! 0 | 4580 | `			goto Synchronize;` |
|       - | 4581 | `	}` |
|       - | 4582 | `	/* Set the delimiter token */` |
|      75 | 4583 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       7 | 4584 | `		nToken = PH7_TK_KEYWORD;` |
|       - | 4585 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       4 | 4586 | `	}else{` |
|      69 | 4587 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - | 4588 | `	}` |
|      75 | 4589 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - | 4590 | `	/* Create the switch blocks container */` |
|      75 | 4591 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      75 | 4592 | `	if( pSwitch == 0 ){` |
|       - | 4593 | `		/* Abort compilation */` |
|     ! 0 | 4594 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4595 | `		return SXERR_ABORT;` |
|       - | 4596 | `	}` |
|       - | 4597 | `	/* Zero the structure */` |
|      75 | 4598 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - | 4599 | `	/* Initialize fields */` |
|      75 | 4600 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - | 4601 | `	/* Emit the switch instruction */` |
|      75 | 4602 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - | 4603 | `	/* Compile case blocks */` |
|     199 | 4604 | `	for(;;){` |
|       - | 4605 | `		sxu32 nKwrd;` |
|     239 | 4606 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 4607 | `			/* No more input to process */` |
|     ! 0 | 4608 | `			break;` |
|       - | 4609 | `		}` |
|     239 | 4610 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 4611 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - | 4612 | `				/* Unexpected token */` |
|     ! 0 | 4613 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 4614 | `					&pGen->pIn->sData);` |
|     ! 0 | 4615 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 4616 | `					return SXERR_ABORT;` |
|       - | 4617 | `				}` |
|       - | 4618 | `				/* FALL THROUGH */` |
|     ! 0 | 4619 | `			}` |
|       - | 4620 | `			/* Block compiled */` |
|     ! 0 | 4621 | `			break;` |
|       - | 4622 | `		}` |
|       - | 4623 | `		/* Extract the keyword */` |
|     239 | 4624 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     239 | 4625 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       7 | 4626 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - | 4627 | `				/* Unexpected token */` |
|     ! 0 | 4628 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 4629 | `					&pGen->pIn->sData);` |
|     ! 0 | 4630 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 4631 | `					return SXERR_ABORT;` |
|       - | 4632 | `				}` |
|       - | 4633 | `				/* FALL THROUGH */` |
|     ! 0 | 4634 | `			}` |
|       - | 4635 | `			/* Block compiled */` |
|       7 | 4636 | `			break;` |
|       - | 4637 | `		}` |
|     233 | 4638 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - | 4639 | `			/*` |
|       - | 4640 | `			 * Accroding to the PHP language reference manual` |
|       - | 4641 | `			 *  A special case is the default case. This case matches anything` |
|       - | 4642 | `			 *  that wasn't matched by the other cases.` |
|       - | 4643 | `			 */` |
|      55 | 4644 | `			if( pSwitch->nDefault > 0 ){` |
|       - | 4645 | `				/* Default case already compiled */` |
|     ! 0 | 4646 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 | 4647 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 4648 | `					return SXERR_ABORT;` |
|       - | 4649 | `				}` |
|     ! 0 | 4650 | `			}` |
|      55 | 4651 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - | 4652 | `			/* Compile the default block */` |
|      55 | 4653 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      55 | 4654 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 4655 | `				return SXERR_ABORT;` |
|      55 | 4656 | `			}else if( rc == SXERR_EOF ){` |
|      53 | 4657 | `				break;` |
|       1 | 4658 | `			}` |
|     184 | 4659 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - | 4660 | `			ph7_case_expr sCase;` |
|       - | 4661 | `			/* Standard case block */` |
|     183 | 4662 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - | 4663 | `			/* initialize the structure */` |
|     183 | 4664 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 4665 | `			/* Compile the case expression */` |
|     183 | 4666 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|     183 | 4667 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4668 | `				return SXERR_ABORT;` |
|       - | 4669 | `			}` |
|       - | 4670 | `			/* Compile the case block */` |
|     183 | 4671 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - | 4672 | `			/* Insert in the switch container */` |
|     183 | 4673 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|     183 | 4674 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 4675 | `				return SXERR_ABORT;` |
|     183 | 4676 | `			}else if( rc == SXERR_EOF ){` |
|      18 | 4677 | `				break;` |
|       - | 4678 | `			}` |
|      86 | 4679 | `		}else{` |
|       - | 4680 | `			/* Unexpected token */` |
|     ! 0 | 4681 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 4682 | `				&pGen->pIn->sData);` |
|     ! 0 | 4683 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4684 | `				return SXERR_ABORT;` |
|       - | 4685 | `			}` |
|     ! 0 | 4686 | `			break;` |
|       - | 4687 | `		}` |
|       5 | 4688 | `	}` |
|       - | 4689 | `	/* Fix all jumps now the destination is resolved */` |
|      75 | 4690 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      75 | 4691 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 4692 | `	/* Release the loop block */` |
|      75 | 4693 | `	GenStateLeaveBlock(pGen,0);` |
|      75 | 4694 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - | 4695 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      75 | 4696 | `		pGen->pIn++;` |
|      35 | 4697 | `	}` |
|       - | 4698 | `	/* Statement successfully compiled */` |
|      75 | 4699 | `	return SXRET_OK;` |
|     ! 0 | 4700 | `Synchronize:` |
|       - | 4701 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 4702 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 | 4703 | `		pGen->pIn++;` |
|     ! 0 | 4704 | `	}` |
|     ! 0 | 4705 | `	return SXRET_OK;` |
|      40 | 4706 | `}` |
|       - | 4707 |  |
