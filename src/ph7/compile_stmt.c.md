# src/ph7/compile_stmt.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1827/2351 lines (77.71%)

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
|      16 |   47 | `static int GenStateConstNameKeywordOk(SyString *pName)` |
|       1 |   48 | `{` |
|       - |   49 | `	static const char *azOk[] = { "bool", "float", "int", "object", "string",` |
|       - |   50 | `	                              "self", "parent" };` |
|       - |   51 | `	sxu32 i;` |
|      73 |   52 | `	for( i = 0 ; i < SX_ARRAYSIZE(azOk) ; ++i ){` |
|      71 |   53 | `		sxu32 n = (sxu32)SyStrlen(azOk[i]);` |
|      71 |   54 | `		if( pName->nByte == n && SyStrnicmp(pName->zString,azOk[i],n) == 0 ){` |
|      15 |   55 | `			return 1;` |
|       - |   56 | `		}` |
|      29 |   57 | `	}` |
|       3 |   58 | `	return 0;` |
|       9 |   59 | `}` |
|     174 |   60 | `PH7_PRIVATE sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       5 |   61 | `{` |
|       - |   62 | `	SySet *pConsCode,*pInstrContainer;` |
|       - |   63 | `	sxu32 nLineLocal;` |
|       - |   64 | `	SyString *pName;` |
|       - |   65 | `	sxi32 rc;` |
|       - |   66 | `	/* php forbids attributes on a comma-separated const list. Snapshot whether the` |
|       - |   67 | `	 * statement carries any now, before the first constant consumes them. */` |
|     179 |   68 | `	int bHadAttrs = SySetUsed(&pGen->aPendingAttrs) > 0;` |
|       - |   69 | ``	/* php attributes every const-statement compile error to the `const` keyword's`` |
|       - |   70 | ``	 * line, not the offending list element's own line (`const A=1,\nB=strlen()` blames`` |
|       - |   71 | `	 * line 1). Capture it once here, before jumping the keyword. */` |
|     179 |   72 | `	nLineLocal = pGen->pIn->nLine;` |
|     179 |   73 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       - |   74 | ``	/* php allows a single `const` statement to declare several constants at once`` |
|       - |   75 | ``	 * (`const A = 1, B = 2;`). Loop over the comma-separated name = value pairs;`` |
|       - |   76 | `	 * PH7_CompileExpr(EXPR_FLAG_COMMA_STATEMENT) stops each value at the first` |
|       - |   77 | `	 * top-level comma so the next pair starts cleanly. */` |
|      92 |   78 | `Loop:` |
|     189 |   79 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |   80 | `		/* Invalid constant name */` |
|       9 |   81 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"identifier");` |
|       9 |   82 | `		if( rc == SXERR_ABORT ){` |
|       - |   83 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |   84 | `			return SXERR_ABORT;` |
|       - |   85 | `		}` |
|       9 |   86 | `		goto Synchronize;` |
|       - |   87 | `	}` |
|       - |   88 | `	/* Peek constant name */` |
|     183 |   89 | `	pName = &pGen->pIn->sData;` |
|       - |   90 | ``	/* php's global `const` takes an IDENTIFIER: a reserved word is a parse error`` |
|       - |   91 | `` 	 * there, and only a CLASS constant may carry one (`class C { const list = 5; }` `` |
|       - |   92 | ``	 * is php-legal, `const list = 5;` at file scope is not). PHL accepted both, so`` |
|       - |   93 | ``	 * `const LIST = 1;` compiled and READ back — source php refuses to parse.`` |
|       - |   94 | `	 * The words php still allows are the ones its lexer does not reserve: the type` |
|       - |   95 | ``	 * names and the scope words. The word OPERATORS (`and`, `or`, `xor`, `new`,`` |
|       - |   96 | ``	 * `clone`, `instanceof`) are reserved too — the lexer types those ID\|OP rather`` |
|       - |   97 | `	 * than KEYWORD, which is why the test reads both bits. */` |
|     178 |   98 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_OP))` |
|     102 |   99 | `	 && !GenStateConstNameKeywordOk(pName) ){` |
|       3 |  100 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn,"identifier");` |
|       3 |  101 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  102 | `			return SXERR_ABORT;` |
|       - |  103 | `		}` |
|       3 |  104 | `		goto Synchronize;` |
|       - |  105 | `	}` |
|       - |  106 | `	/* Make sure the constant name isn't reserved */` |
|     181 |  107 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  108 | `		/* Reserved constant */` |
|      10 |  109 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Cannot redeclare constant '%z'",pName);` |
|      10 |  110 | `		if( rc == SXERR_ABORT ){` |
|       - |  111 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  112 | `			return SXERR_ABORT;` |
|       - |  113 | `		}` |
|      10 |  114 | `		goto Synchronize;` |
|       - |  115 | `	}` |
|     173 |  116 | `	pGen->pIn++;` |
|     173 |  117 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  118 | `		/* Invalid statement*/` |
|       6 |  119 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"=\"");` |
|       6 |  120 | `		if( rc == SXERR_ABORT ){` |
|       - |  121 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  122 | `			return SXERR_ABORT;` |
|       - |  123 | `		}` |
|       6 |  124 | `		goto Synchronize;` |
|       - |  125 | `	}` |
|     169 |  126 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - |  127 | ``	/* php: a closure in a constant expression must be `static function`; a`` |
|       - |  128 | `	 * non-static closure and any arrow fn are compile-time fatals with distinct` |
|       - |  129 | `	 * messages. Checked ahead of the call scan (it skips closure bodies). */` |
|       - |  130 | `	{` |
|     169 |  131 | `		int iClo = PH7_GenStateInitClosureError(pGen);` |
|     169 |  132 | `		if( iClo ){` |
|       8 |  133 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       2 |  134 | `				iClo == 2 ? "Closures in constant expressions must be static"` |
|       - |  135 | `				          : "Constant expression contains invalid operations");` |
|       6 |  136 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  137 | `				return SXERR_ABORT;` |
|       - |  138 | `			}` |
|       6 |  139 | `			goto Synchronize;` |
|       - |  140 | `		}` |
|       - |  141 | `	}` |
|       - |  142 | `	/* php: a constant expression may not CALL anything --` |
|       - |  143 | `	 * "Constant expression contains invalid operations". PHL used to evaluate the` |
|       - |  144 | ``	 * call happily, so `const X = strlen("ab");` defined X as 2. */`` |
|     165 |  145 | `	if( PH7_GenStateInitHasCallExpr(pGen) ){` |
|       5 |  146 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - |  147 | `			"Constant expression contains invalid operations");` |
|       5 |  148 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  149 | `			return SXERR_ABORT;` |
|       - |  150 | `		}` |
|       5 |  151 | `		goto Synchronize;` |
|       - |  152 | `	}` |
|       - |  153 | `	/* Allocate a new constant value container */` |
|     161 |  154 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|     161 |  155 | `	if( pConsCode == 0 ){` |
|     ! 0 |  156 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  157 | `		return SXERR_ABORT;` |
|       - |  158 | `	}` |
|     161 |  159 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  160 | `	/* Swap bytecode container */` |
|     161 |  161 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     161 |  162 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - |  163 | ``	/* Compile constant value. php: a stray token after `const X = EXPR` is`` |
|       - |  164 | ``	 * `... expecting "," or ";"` (const supports a comma-separated list).`` |
|       - |  165 | `	 * EXPR_FLAG_COMMA_STATEMENT stops this value at the first top-level comma so a` |
|       - |  166 | `	 * following declaration is left for the loop below. */` |
|       - |  167 | `	{` |
|     161 |  168 | `		const char *zSaveConst = pGen->zClauseCloser;` |
|     161 |  169 | `		pGen->zClauseCloser = "\",\" or \";\"";` |
|     161 |  170 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|     161 |  171 | `		pGen->zClauseCloser = zSaveConst;` |
|       - |  172 | `	}` |
|       - |  173 | `	/* Emit the done instruction */` |
|     161 |  174 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     161 |  175 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     161 |  176 | `	if( rc == SXERR_ABORT ){` |
|       - |  177 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  178 | `		return SXERR_ABORT;` |
|       - |  179 | `	}` |
|       - |  180 | ``	/* php parse-errors an empty initializer (`const A = ;`, or a `,B=` list hole);`` |
|       - |  181 | `	 * the class-const path rejects it too. Reject loudly rather than silently` |
|       - |  182 | `	 * defining a NULL constant. (php's error KIND -- a parse error -- differs from` |
|       - |  183 | `	 * PHL's compile fatal, the recursive-descent-vs-bison family; both refuse.) */` |
|     161 |  184 | `	if( rc == SXERR_EMPTY && PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       2 |  185 | `			"Empty constant '%z' value",pName) == SXERR_ABORT ){` |
|     ! 0 |  186 | `		return SXERR_ABORT;` |
|       - |  187 | `	}` |
|     161 |  188 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - |  189 | `	/* Register the constant with namespace-qualified name */` |
|       - |  190 | `	{` |
|       - |  191 | `		SyBlob sFQN;` |
|       - |  192 | `		SyString sFQNStr;` |
|     161 |  193 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     161 |  194 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|     161 |  195 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|       - |  196 | ``		/* php refuses a `const` whose name a local `use const` already took. */`` |
|     161 |  197 | `		if( GenStateGuardImportRedeclare(pGen,2,pName,&sFQNStr,nLineLocal) == SXERR_ABORT ){` |
|     ! 0 |  198 | `			SyBlobRelease(&sFQN);` |
|     ! 0 |  199 | `			return SXERR_ABORT;` |
|       - |  200 | `		}` |
|     239 |  201 | `		rc = PH7_VmRegisterConstantEx(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode,` |
|     156 |  202 | `			(SyString *)SySetPeek(&pGen->pVm->aFiles),nLineLocal,1);` |
|     161 |  203 | `		if( rc == SXRET_OK && SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|       - |  204 | ``			/* php 8.5: attributes on `const` statements — attach the pending`` |
|       - |  205 | `			 * groups to the registered constant record for Reflection. */` |
|      11 |  206 | `			SyHashEntry *pCEntry = SyHashGet(&pGen->pVm->hConstant,` |
|       6 |  207 | `				SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|       8 |  208 | `			if( pCEntry ){` |
|       8 |  209 | `				ph7_constant *pRegCons = (ph7_constant *)pCEntry->pUserData;` |
|       8 |  210 | `				if( GenStateConsumeAttrs(&(*pGen),&pRegCons->aAttrs) == SXERR_ABORT ){` |
|     ! 0 |  211 | `					SyBlobRelease(&sFQN);` |
|     ! 0 |  212 | `					return SXERR_ABORT;` |
|       - |  213 | `				}` |
|       3 |  214 | `			}` |
|       3 |  215 | `		}` |
|     161 |  216 | `		SyBlobRelease(&sFQN);` |
|       - |  217 | `	}` |
|     161 |  218 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  219 | `		SySetRelease(pConsCode);` |
|     ! 0 |  220 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 |  221 | `	}` |
|       - |  222 | ``	/* Another declaration in the same statement: `const A = 1, B = 2;`. */`` |
|     161 |  223 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /* ',' */) ){` |
|      14 |  224 | `		if( bHadAttrs ){` |
|       - |  225 | ``			/* php compile-fatals `#[Attr] const A = 1, B = 2;` outright. */`` |
|       3 |  226 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - |  227 | `				"Cannot apply attributes to multiple constants at once");` |
|       3 |  228 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  229 | `				return SXERR_ABORT;` |
|       - |  230 | `			}` |
|       3 |  231 | `			goto Synchronize;` |
|       - |  232 | `		}` |
|      12 |  233 | `		pGen->pIn++; /* Jump the comma */` |
|      12 |  234 | `		goto Loop;` |
|       - |  235 | `	}` |
|     149 |  236 | `	return SXRET_OK;` |
|      15 |  237 | `Synchronize:` |
|       - |  238 | `	/* Synchronize with the next top-level semicolon and avoid compiling this` |
|       - |  239 | ``	 * erroneous statement. BRACE-aware (only `{`...`}`): a rejected closure`` |
|       - |  240 | ``	 * initializer's body holds inner `;` that are not statement terminators, so`` |
|       - |  241 | ``	 * without this its `;` and `}` dangle into a spurious second error where php`` |
|       - |  242 | `	 * halts at the first fatal. Parentheses and brackets are deliberately NOT` |
|       - |  243 | ``	 * tracked -- a lone unbalanced `(`/`[` in erroneous input must not swallow the`` |
|       - |  244 | ``	 * following statements (which would drop later error reports); a `{` never`` |
|       - |  245 | `	 * appears in a valid global-const initializer except as a closure body. */` |
|       - |  246 | `	{` |
|      34 |  247 | `		int iBrace = 0;` |
|     130 |  248 | `		while( pGen->pIn < pGen->pEnd ){` |
|     130 |  249 | `			if( iBrace == 0 && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      34 |  250 | `				break;` |
|       - |  251 | `			}` |
|     100 |  252 | `			if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       3 |  253 | `				iBrace++;` |
|      99 |  254 | `			}else if( pGen->pIn->nType & PH7_TK_CCB ){` |
|       3 |  255 | `				if( iBrace > 0 ){ iBrace--; }` |
|       1 |  256 | `			}` |
|     100 |  257 | `			pGen->pIn++;` |
|       4 |  258 | `		}` |
|       - |  259 | `	}` |
|      34 |  260 | `	return SXRET_OK;` |
|      92 |  261 | `}` |
|       - |  262 | `/*` |
|       - |  263 | ` * Compile the 'continue' statement.` |
|       - |  264 | ` * According to the PHP language reference` |
|       - |  265 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|       - |  266 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|       - |  267 | ` *  iteration.` |
|       - |  268 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|       - |  269 | ` *  the purposes of continue.` |
|       - |  270 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|       - |  271 | ` *  of enclosing loops it should skip to the end of.` |
|       - |  272 | ` *  Note:` |
|       - |  273 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|       - |  274 | ` */` |
|       - |  275 | `/*` |
|       - |  276 | `` * Pick the jump opcode for a `break`/`continue` targeting pLoop and fill in its iP1,`` |
|       - |  277 | ` * emitting the POP_EXCEPTIONs for the crossed trys this statement can resolve here and` |
|       - |  278 | `` * now. All the classification lives in GenStateJumpScope, which `goto` shares.`` |
|       - |  279 | ` */` |
|   41444 |  280 | `static sxi32 GenStateLoopJumpOp(ph7_gen_state *pGen,GenBlock *pLoop,sxi32 *piP1,` |
|       - |  281 | `	GenJumpScope *pCross)` |
|       5 |  282 | `{` |
|       - |  283 | `	/* The loop encloses the break by construction, so the walk always reaches it. Note the` |
|       - |  284 | `	 * walk EMITS the crossed trys' POP_EXCEPTIONs as it goes, before the caller can see` |
|       - |  285 | `	 * pCross->nFinally and reject: a statement about to be fatal therefore leaves a few` |
|       - |  286 | `	 * dead instructions behind. Harmless — the compile error stops the program from` |
|       - |  287 | `	 * running at all — and the alternative is walking the chain twice on every jump. */` |
|   41449 |  288 | `	GenStateJumpScope(&(*pGen),pGen->nCurScopeId,pLoop->nScopeId,TRUE,pCross);` |
|   41449 |  289 | `	return GenStateScopeJumpOp(pCross,piP1);` |
|       5 |  290 | `}` |
|       - |  291 | `/*` |
|       - |  292 | `` * php compile-rejects a `break`/`continue`/`goto` that leaves a `finally` body (a`` |
|       - |  293 | `` * `return` is fine). One wording, one place, for all three statements.`` |
|       - |  294 | ` */` |
|      10 |  295 | `PH7_PRIVATE sxi32 GenStateJumpOutOfFinally(ph7_gen_state *pGen,sxu32 nLine)` |
|       4 |  296 | `{` |
|      14 |  297 | `	return PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  298 | `		"jump out of a finally block is disallowed");` |
|       4 |  299 | `}` |
|   30960 |  300 | `PH7_PRIVATE sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       5 |  301 | `{` |
|       - |  302 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  303 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  304 | `	sxi32 iRawLevel; /* The level as WRITTEN, kept for php's diagnostics */` |
|       - |  305 | `	sxu32 nLineLocal;` |
|       - |  306 | `	sxi32 rc;` |
|   30965 |  307 | `	iRawLevel = 1;` |
|   30965 |  308 | `	nLineLocal = pGen->pIn->nLine;` |
|   30965 |  309 | `	iLevel = 0;` |
|       - |  310 | `	/* Jump the 'continue' keyword */` |
|   30965 |  311 | `	pGen->pIn++;` |
|   30965 |  312 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  313 | `		/* optional numeric argument which tells us how many levels` |
|       - |  314 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  315 | `		 */` |
|       - |  316 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      19 |  317 | `		char *zAlloc = 0;` |
|       - |  318 | `		SyString sNum;` |
|      19 |  319 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      19 |  320 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  321 | `			return SXERR_ABORT;` |
|       - |  322 | `		}` |
|      19 |  323 | `		if( rc == SXRET_OK ){` |
|      24 |  324 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      14 |  325 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      17 |  326 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  327 | `				return SXERR_ABORT;` |
|       - |  328 | `			}` |
|      17 |  329 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      17 |  330 | `			iRawLevel = iLevel;` |
|      17 |  331 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       7 |  332 | `		}` |
|      19 |  333 | `		if( iLevel < 2 ){` |
|       3 |  334 | `			iLevel = 0;` |
|       1 |  335 | `		}` |
|      19 |  336 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       8 |  337 | `	}` |
|       - |  338 | `	/* php rejects a non-positive level outright, before asking where it lands. */` |
|   30965 |  339 | `	if( iRawLevel < 1 ){` |
|     ! 0 |  340 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - |  341 | `			"'continue' operator accepts only positive integers");` |
|     ! 0 |  342 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  343 | `			return SXERR_ABORT;` |
|       - |  344 | `		}` |
|     ! 0 |  345 | `		return SXRET_OK;` |
|       - |  346 | `	}` |
|       - |  347 | `	/* Point to the target loop */` |
|   30965 |  348 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|   30965 |  349 | `	if( pLoop == 0 ){` |
|       - |  350 | ``		/* Same split php makes for `break`: no loop at all keeps the not-in-context`` |
|       - |  351 | ``		 * wording, too FEW loops is `Cannot 'continue' N levels`. */`` |
|      13 |  352 | `		if( GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,0) != 0 ){` |
|     ! 0 |  353 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|     ! 0 |  354 | `				"Cannot 'continue' %d level%s",iRawLevel,iRawLevel == 1 ? "" : "s");` |
|     ! 0 |  355 | `		}else{` |
|      13 |  356 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"'continue' not in the 'loop' or 'switch' context");` |
|       - |  357 | `		}` |
|      13 |  358 | `		if( rc == SXERR_ABORT ){` |
|       - |  359 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  360 | `			return SXERR_ABORT;` |
|       - |  361 | `		}` |
|       8 |  362 | `	}else{` |
|   30955 |  363 | `		sxu32 nInstrIdx = 0;` |
|   30955 |  364 | `		sxi32 iP1 = 0;` |
|       - |  365 | `		GenJumpScope sCross;` |
|   30955 |  366 | `		sxi32 iJmpOp = GenStateLoopJumpOp(&(*pGen),pLoop,&iP1,&sCross);` |
|   30955 |  367 | `		if( sCross.nFinally > 0 ){` |
|       3 |  368 | `			if( GenStateJumpOutOfFinally(&(*pGen),nLineLocal) == SXERR_ABORT ){` |
|     ! 0 |  369 | `				return SXERR_ABORT;` |
|       1 |  370 | `			}` |
|   30954 |  371 | `		}else if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|       - |  372 | ``			/* `continue` inside a switch acts like `break` — which is almost never what`` |
|       - |  373 | `			 * the author meant, so php 7.3+ says so at compile time. The generated jump` |
|       - |  374 | `			 * is unchanged; only the diagnostic was missing. An explicit level` |
|       - |  375 | ``			 * (`continue 2`) targets the enclosing loop and stays silent. */`` |
|       5 |  376 | `			if( iLevel < 1 ){` |
|       5 |  377 | `				PH7_GenCompileError(&(*pGen),E_WARNING,nLineLocal,` |
|       - |  378 | `					"\"continue\" targeting switch is equivalent to \"break\"."` |
|       - |  379 | `					" Did you mean to use \"continue 2\"?");` |
|       2 |  380 | `			}` |
|       5 |  381 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,iP1,0,0,&nInstrIdx);` |
|       5 |  382 | `			if( rc == SXRET_OK ){` |
|       5 |  383 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|       2 |  384 | `			}` |
|       3 |  385 | `		}else{` |
|       - |  386 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|   30949 |  387 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,iP1,pLoop->nFirstInstr,0,&nInstrIdx);` |
|   30949 |  388 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - |  389 | `				JumpFixup sJumpFix;` |
|       - |  390 | `				/* Post-continue */` |
|   15463 |  391 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|   15463 |  392 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|   15463 |  393 | `				sJumpFix.pContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   15463 |  394 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|    7729 |  395 | `			}` |
|       - |  396 | `		}` |
|       - |  397 | `	}` |
|   30965 |  398 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  399 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  400 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 |  401 | `	}` |
|       - |  402 | `	/* Statement successfully compiled */` |
|   30965 |  403 | `	return SXRET_OK;` |
|   15485 |  404 | `}` |
|       - |  405 | `/*` |
|       - |  406 | ` * Compile the 'break' statement.` |
|       - |  407 | ` * According to the PHP language reference` |
|       - |  408 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - |  409 | ` *  structure.` |
|       - |  410 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - |  411 | ` *  enclosing structures are to be broken out of.` |
|       - |  412 | ` */` |
|   10510 |  413 | `PH7_PRIVATE sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       5 |  414 | `{` |
|       - |  415 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  416 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  417 | `	sxi32 iRawLevel; /* The level as WRITTEN, kept for php's diagnostics */` |
|       - |  418 | `	sxu32 nLineLocal;` |
|       - |  419 | `	sxi32 rc;` |
|   10515 |  420 | `	iLevel = 0;` |
|   10515 |  421 | `	iRawLevel = 1;` |
|   10515 |  422 | `	nLineLocal = pGen->pIn->nLine;` |
|       - |  423 | `	/* Jump the 'break' keyword */` |
|   10515 |  424 | `	pGen->pIn++;` |
|   10515 |  425 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  426 | `		/* optional numeric argument which tells us how many levels` |
|       - |  427 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  428 | `		 */` |
|       - |  429 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      22 |  430 | `		char *zAlloc = 0;` |
|       - |  431 | `		SyString sNum;` |
|      22 |  432 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      22 |  433 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  434 | `			return SXERR_ABORT;` |
|       - |  435 | `		}` |
|      22 |  436 | `		if( rc == SXRET_OK ){` |
|      28 |  437 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      16 |  438 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      20 |  439 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  440 | `				return SXERR_ABORT;` |
|       - |  441 | `			}` |
|      20 |  442 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      20 |  443 | `			iRawLevel = iLevel;` |
|      20 |  444 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       8 |  445 | `		}` |
|      22 |  446 | `		if( iLevel < 2 ){` |
|       3 |  447 | `			iLevel = 0;` |
|       1 |  448 | `		}` |
|      22 |  449 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       9 |  450 | `	}` |
|       - |  451 | `	/* php rejects a non-positive level outright, before asking where it lands. */` |
|   10515 |  452 | `	if( iRawLevel < 1 ){` |
|     ! 0 |  453 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  454 | `			"'break' operator accepts only positive integers");` |
|     ! 0 |  455 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  456 | `			return SXERR_ABORT;` |
|       - |  457 | `		}` |
|     ! 0 |  458 | `		goto BreakLevelDone;` |
|       - |  459 | `	}` |
|       - |  460 | `	/* Extract the target loop */` |
|   10515 |  461 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|   15770 |  462 | `	if( pLoop == 0 ){` |
|       - |  463 | `		/* php distinguishes "there is no loop at all here" from "there is one, but` |
|       - |  464 | `		 * not N of them": the first keeps the not-in-context wording, the second is` |
|       - |  465 | ``		 * `Cannot 'break' N levels`. */`` |
|      19 |  466 | `		if( GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,0) != 0 ){` |
|       4 |  467 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 |  468 | `				"Cannot 'break' %d level%s",iRawLevel,iRawLevel == 1 ? "" : "s");` |
|       2 |  469 | `		}else{` |
|      16 |  470 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"'break' not in the 'loop' or 'switch' context");` |
|       - |  471 | `		}` |
|      19 |  472 | `		if( rc == SXERR_ABORT ){` |
|       - |  473 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  474 | `			return SXERR_ABORT;` |
|       - |  475 | `		}` |
|      11 |  476 | `	}else{` |
|       - |  477 | `		sxu32 nInstrIdx;` |
|   10499 |  478 | `		sxi32 iP1 = 0;` |
|       - |  479 | `		GenJumpScope sCross;` |
|   10499 |  480 | `		sxi32 iJmpOp = GenStateLoopJumpOp(&(*pGen),pLoop,&iP1,&sCross);` |
|   10499 |  481 | `		if( sCross.nFinally > 0 ){` |
|       9 |  482 | `			if( GenStateJumpOutOfFinally(&(*pGen),nLineLocal) == SXERR_ABORT ){` |
|     ! 0 |  483 | `				return SXERR_ABORT;` |
|       - |  484 | `			}` |
|       6 |  485 | `		}else{` |
|   10493 |  486 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,iP1,0,0,&nInstrIdx);` |
|   10493 |  487 | `			if( rc == SXRET_OK ){` |
|       - |  488 | `				/* Fix the jump later when the jump destination is resolved */` |
|   10493 |  489 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|    5244 |  490 | `			}` |
|       - |  491 | `		}` |
|       - |  492 | `	}` |
|    5255 |  493 | `BreakLevelDone:` |
|   10515 |  494 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  495 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  496 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 |  497 | `	}` |
|       - |  498 | `	/* Statement successfully compiled */` |
|   10515 |  499 | `	return SXRET_OK;` |
|    5260 |  500 | `}` |
|       - |  501 | `/*` |
|       - |  502 | ` * The function body a goto or a label sits in, or NULL at file scope. A goto may not` |
|       - |  503 | ` * cross functions, so GenStateFixGoto pairs the two on this.` |
|       - |  504 | ` */` |
|     432 |  505 | `static ph7_vm_func * GenStateOwningFunc(ph7_gen_state *pGen)` |
|       5 |  506 | `{` |
|     437 |  507 | `	GenBlock *pBlock = pGen->pCurrent;` |
|    1105 |  508 | `	while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|     673 |  509 | `		pBlock = pBlock->pParent;` |
|       5 |  510 | `	}` |
|     437 |  511 | `	return pBlock ? (ph7_vm_func *)pBlock->pUserData : 0;` |
|       5 |  512 | `}` |
|       - |  513 | `/*` |
|       - |  514 | ` * Compile or record a label.` |
|       - |  515 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|       - |  516 | ` * Example` |
|       - |  517 | ` *  goto LABEL;` |
|       - |  518 | ` *   echo 'Foo';` |
|       - |  519 | ` *  LABEL:` |
|       - |  520 | ` *   echo 'Bar';` |
|       - |  521 | ` */` |
|     212 |  522 | `PH7_PRIVATE sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|       5 |  523 | `{` |
|       - |  524 | `	Label sLabel;` |
|       - |  525 | `	/* php places almost NO restriction on where a label may be DEFINED — inside a loop, a` |
|       - |  526 | `	 * switch or a try{} is all fine; the one rule is that a name may not be declared twice` |
|       - |  527 | `	 * in the same function (below). The rest is on the jump: you may not goto INTO a loop` |
|       - |  528 | `	 * or switch from outside it, which is checked once the labels are all known (see` |
|       - |  529 | `	 * GenStateFixJumps). Record the loop this label sits in so that check can run. */` |
|       - |  530 | `	{` |
|     217 |  531 | `		SyString *pTarget = &pGen->pIn->sData;` |
|     217 |  532 | `		ph7_vm_func *pFunc = GenStateOwningFunc(&(*pGen));` |
|       - |  533 | `		char *zDup;` |
|       - |  534 | `		/* One name, one destination: php compile-rejects a label its function already` |
|       - |  535 | ``		 * declares, wherever the two sit (`L: L:`, one per branch of an if, one in a loop`` |
|       - |  536 | `		 * and one after it). PHL used to accept the redeclaration and silently give every` |
|       - |  537 | `		 * goto the FIRST one. The owning function is part of the key, so the same name in` |
|       - |  538 | `		 * another function — or at file scope beside it — is untouched by this. On the` |
|       - |  539 | `		 * duplicate, keep the first declaration and record nothing: the compile has already` |
|       - |  540 | `		 * failed, and a second entry under the same key would only shadow it. */` |
|     217 |  541 | `		if( SXRET_OK == GenStateGetLabel(&(*pGen),pTarget,pFunc,0) ){` |
|       8 |  542 | `			if( SXERR_ABORT == PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       2 |  543 | `				"Label '%z' already defined",pTarget) ){` |
|     ! 0 |  544 | `				return SXERR_ABORT;` |
|       - |  545 | `			}` |
|       6 |  546 | `			pGen->pIn += 2; /* Jump the label name and the semi-colon */` |
|       6 |  547 | `			return SXRET_OK;` |
|       - |  548 | `		}` |
|       - |  549 | `		/* Initialize label fields */` |
|     213 |  550 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       - |  551 | `		/* Duplicate label name */` |
|     213 |  552 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     213 |  553 | `		if( zDup == 0 ){` |
|     ! 0 |  554 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  555 | `			return SXERR_ABORT;` |
|       - |  556 | `		}` |
|     213 |  557 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|     213 |  558 | `		sLabel.nLine = pGen->pIn->nLine;` |
|     213 |  559 | `		sLabel.nLoopId = pGen->nCurLoopId;` |
|       - |  560 | `		/* Where the label sits, so a goto from a detached catch/finally body can be told` |
|       - |  561 | `		 * what it has to cross to reach it (GenStateJumpScope). */` |
|     213 |  562 | `		sLabel.pContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     213 |  563 | `		sLabel.nScopeId = pGen->nCurScopeId;` |
|       - |  564 | `		/* The owning FUNCTION, matched against the goto's in GenStateFixGoto. This used` |
|       - |  565 | `		 * to stop at GEN_BLOCK_EXCEPTION as well, which attributed every label inside a` |
|       - |  566 | `		 * try or catch body to "no function" — so from anywhere in a function such a` |
|       - |  567 | `		 * label read as undefined, including from the very catch body declaring it.` |
|       - |  568 | `		 * Whether a label may be jumped TO is decided by its container, not by this. */` |
|     213 |  569 | `		sLabel.pFunc = pFunc;` |
|       - |  570 | `		/* Insert in label set */` |
|     213 |  571 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|       - |  572 | `	}` |
|     213 |  573 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|     213 |  574 | `	return SXRET_OK;` |
|     111 |  575 | `}` |
|       - |  576 | `/*` |
|       - |  577 | ` * Compile the so hated 'goto' statement.` |
|       - |  578 | ` * You've probably been taught that gotos are bad, but this sort` |
|       - |  579 | ` * of rewriting  happens all the time, in fact every time you run` |
|       - |  580 | ` * a compiler it has to do this.` |
|       - |  581 | ` * According to the PHP language reference manual` |
|       - |  582 | ` *   The goto operator can be used to jump to another section in the program.` |
|       - |  583 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|       - |  584 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|       - |  585 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|       - |  586 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|       - |  587 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|       - |  588 | ` *   of a multi-level break` |
|       - |  589 | ` */` |
|     224 |  590 | `PH7_PRIVATE sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|       5 |  591 | `{` |
|       - |  592 | `	JumpFixup sJump;` |
|       - |  593 | `	sxi32 rc;` |
|     229 |  594 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|     229 |  595 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  596 | `		/* Missing label */` |
|     ! 0 |  597 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|     ! 0 |  598 | `		if( rc == SXERR_ABORT ){` |
|       - |  599 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  600 | `			return SXERR_ABORT;` |
|       - |  601 | `		}` |
|     ! 0 |  602 | `		return SXRET_OK;` |
|       - |  603 | `	}` |
|     229 |  604 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       6 |  605 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn,"identifier");` |
|       6 |  606 | `		if( rc == SXERR_ABORT ){` |
|       - |  607 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  608 | `			return SXERR_ABORT;` |
|       - |  609 | `		}` |
|       4 |  610 | `	}else{` |
|     225 |  611 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  612 | `		char *zDup;` |
|       - |  613 | `		/* Prepare the jump destination */` |
|     225 |  614 | `		sJump.nJumpType = PH7_OP_JMP;` |
|     225 |  615 | `		sJump.nLine = pGen->pIn->nLine;` |
|       - |  616 | `		/* Gotos resolve at end of compilation, well after any container swap. */` |
|     225 |  617 | `		sJump.pContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     225 |  618 | `		sJump.nScopeId = pGen->nCurScopeId;` |
|       - |  619 | `		/* Duplicate label name */` |
|     225 |  620 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     225 |  621 | `		if( zDup == 0 ){` |
|     ! 0 |  622 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  623 | `			return SXERR_ABORT;` |
|       - |  624 | `		}` |
|     225 |  625 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|       - |  626 | `		/* The loop/switch this goto sits in, for the "goto into a loop" check later. */` |
|     225 |  627 | `		sJump.nLoopId = pGen->nCurLoopId;` |
|       - |  628 | `		/* A goto inside a try{}/catch{} is legal php (jumping OUT of the block is fine);` |
|       - |  629 | `		 * only the owning function matters here, since a goto may not cross functions. */` |
|     225 |  630 | `		sJump.pFunc = GenStateOwningFunc(&(*pGen));` |
|       - |  631 | `		/* Emit the unconditional jump. Inside a DETACHED catch/finally body the target may` |
|       - |  632 | `		 * lie in another bytecode array, which a plain OP_JMP cannot address; enclosing` |
|       - |  633 | `		 * trys likewise need their finally run on the way out, which a plain jump would` |
|       - |  634 | `		 * skip. Emit a structure-crossing jump whenever either is possible — the label is` |
|       - |  635 | `		 * not known yet, so GenStateFixGoto picks the final opcode (and may downgrade it` |
|       - |  636 | `		 * back to OP_JMP once the counts prove to cancel). */` |
|     335 |  637 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,` |
|     220 |  638 | `			sJump.nScopeId > 0 ? PH7_OP_CATCH_JMP : PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|     225 |  639 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|     110 |  640 | `		}` |
|       - |  641 | `	}` |
|     229 |  642 | `	pGen->pIn++; /* Jump the label name */` |
|     229 |  643 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       3 |  644 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|       1 |  645 | `	}` |
|       - |  646 | `	/* Statement successfully compiled */` |
|     229 |  647 | `	return SXRET_OK;` |
|     117 |  648 | `}` |
|       - |  649 | `/*` |
|       - |  650 | ` * Point to the next PHP chunk that will be processed shortly.` |
|       - |  651 | ` * Return SXRET_OK on success. Any other return value indicates` |
|       - |  652 | ` * failure.` |
|       - |  653 | ` */` |
|      20 |  654 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|       1 |  655 | `{` |
|       - |  656 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - |  657 | `	sxu32 nRawObj;` |
|      10 |  658 | `	sxu32 nObjIdx;` |
|       - |  659 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - |  660 | `	 * a PHP block.` |
|       - |  661 | `	 */` |
|      10 |  662 | `Consume:` |
|      21 |  663 | `	nRawObj = nObjIdx = 0;` |
|      21 |  664 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|     ! 0 |  665 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|     ! 0 |  666 | `		if( pRawObj == 0 ){` |
|     ! 0 |  667 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  668 | `			return SXERR_ABORT;` |
|       - |  669 | `		}` |
|       - |  670 | `		/* Mark as constant and emit the load constant instruction */` |
|     ! 0 |  671 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|     ! 0 |  672 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     ! 0 |  673 | `		++nRawObj;` |
|     ! 0 |  674 | `		pGen->pRawIn++; /* Next chunk */` |
|     ! 0 |  675 | `	}` |
|      21 |  676 | `	if( nRawObj > 0 ){` |
|       - |  677 | `		/* Emit the consume instruction */` |
|     ! 0 |  678 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     ! 0 |  679 | `	}` |
|      21 |  680 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|     ! 0 |  681 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|       - |  682 | `		/* Reset the token set (and its trivia sidecar) */` |
|     ! 0 |  683 | `		SySetReset(pTokenSet);` |
|     ! 0 |  684 | `		SySetReset(&pGen->aTrivia);` |
|       - |  685 | `		/* Tokenize input */` |
|     ! 0 |  686 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|     ! 0 |  687 | `			pGen->pRawIn->nLine,pTokenSet,&pGen->aTrivia);` |
|       - |  688 | `		/* Point to the fresh token stream */` |
|     ! 0 |  689 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     ! 0 |  690 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|       - |  691 | `		/* Advance the stream cursor */` |
|     ! 0 |  692 | `		pGen->pRawIn++;` |
|       - |  693 | `		/* TICKET 1433-011 */` |
|     ! 0 |  694 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - |  695 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - |  696 | `			sxi32 rc;` |
|       - |  697 | `			/* Refer to TICKET 1433-009  */` |
|     ! 0 |  698 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|     ! 0 |  699 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|     ! 0 |  700 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       - |  701 | `			/* Synthesized short-tag echo: legitimately an expression here. */` |
|     ! 0 |  702 | `			pGen->nExprEchoOk++;` |
|     ! 0 |  703 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 |  704 | `			pGen->nExprEchoOk--;` |
|     ! 0 |  705 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  706 | `				return SXERR_ABORT;` |
|     ! 0 |  707 | `			}else if( rc != SXERR_EMPTY ){` |
|     ! 0 |  708 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  709 | `			}` |
|     ! 0 |  710 | `			goto Consume;` |
|       - |  711 | `		}` |
|     ! 0 |  712 | `	}else{` |
|       - |  713 | `		/* No more chunks to process */` |
|      21 |  714 | `		pGen->pIn = pGen->pEnd;` |
|      21 |  715 | `		return SXERR_EOF;` |
|       - |  716 | `	}` |
|     ! 0 |  717 | `	return SXRET_OK;` |
|      11 |  718 | `}` |
|       - |  719 | `/*` |
|       - |  720 | ` * Compile a PHP block.` |
|       - |  721 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - |  722 | ` * optionally delimited by braces {}.` |
|       - |  723 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  724 | ` * and this function takes care of generating the appropriate error` |
|       - |  725 | ` * message.` |
|       - |  726 | ` */` |
|  675280 |  727 | `PH7_PRIVATE sxi32 PH7_CompileBlock(` |
|       - |  728 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  729 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - |  730 | `	)` |
|       5 |  731 | `{` |
|       - |  732 | `	sxi32 rc;` |
|       - |  733 | `	sxu32 nLine;` |
|  675285 |  734 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  674259 |  735 | `		nLine = pGen->pIn->nLine;` |
|  674259 |  736 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  674259 |  737 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  738 | `			return SXERR_ABORT;` |
|       - |  739 | `		}` |
|  674259 |  740 | `		pGen->pIn++;` |
|       - |  741 | `		/* Compile until we hit the closing braces '}' */` |
| 1097930 |  742 | `		for(;;){` |
| 2195865 |  743 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      21 |  744 | `				rc = GenStateNextChunk(&(*pGen));` |
|      21 |  745 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  746 | `			 	   return SXERR_ABORT;` |
|       - |  747 | `				}` |
|      21 |  748 | `				if( rc == SXERR_EOF ){` |
|       - |  749 | `					/* No more token to process: the block was never closed. php reports` |
|       - |  750 | `					 * the line the '{' was opened on, not where the input ran out. */` |
|      21 |  751 | `					PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"Unclosed '{' on line %u",nLine);` |
|      21 |  752 | `					break;` |
|       - |  753 | `				}` |
|     ! 0 |  754 | `			}` |
| 2195845 |  755 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - |  756 | `				/* Closing braces found,break immediately*/` |
|  674239 |  757 | `				pGen->pIn++;` |
|  674239 |  758 | `				break;` |
|       - |  759 | `			}` |
|       - |  760 | `			/* Compile a single statement */` |
| 1521611 |  761 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
| 1521611 |  762 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  763 | `				return SXERR_ABORT;` |
|       - |  764 | `			}` |
|       5 |  765 | `		}` |
|  674259 |  766 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  338158 |  767 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|      11 |  768 | `		pGen->pIn++;` |
|      11 |  769 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|      11 |  770 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  771 | `			return SXERR_ABORT;` |
|       - |  772 | `		}` |
|       - |  773 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|      11 |  774 | `		for(;;){` |
|      23 |  775 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  776 | `				rc = GenStateNextChunk(&(*pGen));` |
|     ! 0 |  777 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  778 | `			 	   return SXERR_ABORT;` |
|       - |  779 | `				}` |
|     ! 0 |  780 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|       - |  781 | `					/* No more token to process */` |
|     ! 0 |  782 | `					if( rc == SXERR_EOF ){` |
|     ! 0 |  783 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|       - |  784 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|     ! 0 |  785 | `					}` |
|     ! 0 |  786 | `					break;` |
|       - |  787 | `				}` |
|     ! 0 |  788 | `			}` |
|      23 |  789 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - |  790 | `				sxi32 nKwrd;` |
|       - |  791 | `				/* Keyword found */` |
|      21 |  792 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      22 |  793 | `				if( nKwrd == nKeywordEnd \|\|` |
|       6 |  794 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|       - |  795 | `						/* Delimiter keyword found,break */` |
|      11 |  796 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|      11 |  797 | `							pGen->pIn++; /*  endif;endswitch... */` |
|       5 |  798 | `						}` |
|      11 |  799 | `						break;` |
|       - |  800 | `				}` |
|       5 |  801 | `			}` |
|       - |  802 | `			/* Compile a single statement */` |
|      13 |  803 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|      13 |  804 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  805 | `				return SXERR_ABORT;` |
|       - |  806 | `			}` |
|       1 |  807 | `		}` |
|      11 |  808 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       6 |  809 | `	}else{` |
|       - |  810 | `		/* Compile a single statement */` |
|    1021 |  811 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1021 |  812 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  813 | `			return SXERR_ABORT;` |
|       - |  814 | `		}` |
|       - |  815 | `	}` |
|       - |  816 | `	/* Jump trailing semi-colons ';' */` |
|  675295 |  817 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      11 |  818 | `		pGen->pIn++;` |
|       1 |  819 | `	}` |
|  675285 |  820 | `	return SXRET_OK;` |
|  337645 |  821 | `}` |
|       - |  822 | `/*` |
|       - |  823 | ` * Compile the gentle 'while' statement.` |
|       - |  824 | ` * According to the PHP language reference` |
|       - |  825 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|       - |  826 | ` *  The basic form of a while statement is:` |
|       - |  827 | ` *  while (expr)` |
|       - |  828 | ` *   statement` |
|       - |  829 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|       - |  830 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|       - |  831 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|       - |  832 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|       - |  833 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|       - |  834 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|       - |  835 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|       - |  836 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|       - |  837 | ` *  while (expr):` |
|       - |  838 | ` *    statement` |
|       - |  839 | ` *   endwhile;` |
|       - |  840 | ` */` |
|   15652 |  841 | `PH7_PRIVATE sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       5 |  842 | `{` |
|   15657 |  843 | `	GenBlock *pWhileBlock = 0;` |
|   15657 |  844 | `	SyToken *pTmp,*pEnd = 0;` |
|       - |  845 | `	sxu32 nFalseJump;` |
|       - |  846 | `	sxu32 nLine;` |
|       - |  847 | `	sxi32 rc;` |
|   15657 |  848 | `	nLine = pGen->pIn->nLine;` |
|       - |  849 | `	/* Jump the 'while' keyword */` |
|   15657 |  850 | `	pGen->pIn++;` |
|   15657 |  851 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  852 | `		/* Syntax error */` |
|     ! 0 |  853 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  854 | `		if( rc == SXERR_ABORT ){` |
|       - |  855 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  856 | `			return SXERR_ABORT;` |
|       - |  857 | `		}` |
|     ! 0 |  858 | `		goto Synchronize;` |
|       - |  859 | `	}` |
|       - |  860 | `	/* Jump the left parenthesis '(' */` |
|   15657 |  861 | `	pGen->pIn++;` |
|       - |  862 | `	/* Create the loop block */` |
|   15657 |  863 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   15657 |  864 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  865 | `		return SXERR_ABORT;` |
|       - |  866 | `	}` |
|       - |  867 | `	/* Delimit the condition */` |
|   15657 |  868 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   15657 |  869 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  870 | `		/* Empty condition. php reports the token that actually stopped it -- for` |
|       - |  871 | ``		 * `while ()` that is the ')' -- not a hand-written "expected expression". */`` |
|       3 |  872 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,0);` |
|       3 |  873 | `		if( rc == SXERR_ABORT ){` |
|       - |  874 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  875 | `			return SXERR_ABORT;` |
|       - |  876 | `		}` |
|       1 |  877 | `	}` |
|       - |  878 | `	/* Swap token streams */` |
|   15657 |  879 | `	pTmp = pGen->pEnd;` |
|   15657 |  880 | `	pGen->pEnd = pEnd;` |
|       - |  881 | `	/* Compile the expression */` |
|   15657 |  882 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   15657 |  883 | `	if( rc == SXERR_ABORT ){` |
|       - |  884 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  885 | `		return SXERR_ABORT;` |
|       - |  886 | `	}` |
|       - |  887 | `	/* Update token stream */` |
|   15657 |  888 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  889 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|     ! 0 |  890 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  891 | `			return SXERR_ABORT;` |
|       - |  892 | `		}` |
|     ! 0 |  893 | `		pGen->pIn++;` |
|     ! 0 |  894 | `	}` |
|       - |  895 | `	/* Synchronize pointers */` |
|   15657 |  896 | `	pGen->pIn  = &pEnd[1];` |
|   15657 |  897 | `	pGen->pEnd = pTmp;` |
|       - |  898 | `	/* Emit the false jump */` |
|   15657 |  899 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  900 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   15657 |  901 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - |  902 | `	/* Compile the loop body */` |
|   15657 |  903 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   15657 |  904 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  905 | `		return SXERR_ABORT;` |
|       - |  906 | `	}` |
|       - |  907 | `	/* Emit the unconditional jump to the start of the loop */` |
|   15657 |  908 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - |  909 | `	/* Fix all jumps now the destination is resolved */` |
|   15657 |  910 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  911 | `	/* Release the loop block */` |
|   15657 |  912 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  913 | `	/* Statement successfully compiled */` |
|   15657 |  914 | `	return SXRET_OK;` |
|     ! 0 |  915 | `Synchronize:` |
|       - |  916 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  917 | `	 * compiling this erroneous block.` |
|       - |  918 | `	 */` |
|     ! 0 |  919 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  920 | `		pGen->pIn++;` |
|     ! 0 |  921 | `	}` |
|     ! 0 |  922 | `	return SXRET_OK;` |
|    7831 |  923 | `}` |
|       - |  924 | `/*` |
|       - |  925 | ` * Compile the ugly do..while() statement.` |
|       - |  926 | ` * According to the PHP language reference` |
|       - |  927 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|       - |  928 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|       - |  929 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|       - |  930 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|       - |  931 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|       - |  932 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|       - |  933 | ` *  would end immediately).` |
|       - |  934 | ` *  There is just one syntax for do-while loops:` |
|       - |  935 | ` *  <?php` |
|       - |  936 | ` *  $i = 0;` |
|       - |  937 | ` *  do {` |
|       - |  938 | ` *   echo $i;` |
|       - |  939 | ` *  } while ($i > 0);` |
|       - |  940 | ` * ?>` |
|       - |  941 | ` */` |
|       8 |  942 | `PH7_PRIVATE sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|       3 |  943 | `{` |
|      11 |  944 | `	SyToken *pTmp,*pEnd = 0;` |
|      11 |  945 | `	GenBlock *pDoBlock = 0;` |
|       - |  946 | `	sxu32 nLine;` |
|       - |  947 | `	sxi32 rc;` |
|      11 |  948 | `	nLine = pGen->pIn->nLine;` |
|       - |  949 | `	/* Jump the 'do' keyword */` |
|      11 |  950 | `	pGen->pIn++;` |
|       - |  951 | `	/* Create the loop block */` |
|      11 |  952 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|      11 |  953 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  954 | `		return SXERR_ABORT;` |
|       - |  955 | `	}` |
|       - |  956 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|      11 |  957 | `	pDoBlock->bPostContinue = TRUE;` |
|      11 |  958 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      11 |  959 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  960 | `		return SXERR_ABORT;` |
|       - |  961 | `	}` |
|      11 |  962 | `	if( pGen->pIn < pGen->pEnd ){` |
|       9 |  963 | `		nLine = pGen->pIn->nLine;` |
|       3 |  964 | `	}` |
|      11 |  965 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|       6 |  966 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|       - |  967 | `			/* Missing 'while' statement */` |
|       - |  968 | ``			/* php: `do {} ;` names the ';', while `do {}` at EOF is "unexpected end of`` |
|       - |  969 | `			 * file". The do-block's own slice stops before its terminator, so look at` |
|       - |  970 | `			 * the whole CHUNK stream: a token still there is the one php names; nothing` |
|       - |  971 | `			 * left means end of file (NULL). */` |
|       - |  972 | `			{` |
|       3 |  973 | `				SyToken *pBad = pGen->pIn < pGen->pEnd ? pGen->pIn : 0;` |
|       3 |  974 | `				if( pBad == 0 && pGen->pTokenSet ){` |
|       - |  975 | `					/* The do-block consumed its terminator, so the token php names is` |
|       - |  976 | `					 * the one just behind the cursor -- but only when it is a real` |
|       - |  977 | ``					 * terminator (`do {} ;` -> the ';'). If the block simply ended on`` |
|       - |  978 | `					 * its '}' with nothing after it, php reports end of file. */` |
|       3 |  979 | `					SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|       3 |  980 | `					if( pGen->pIn > pBase && (pGen->pIn[-1].nType & PH7_TK_SEMI) ){` |
|     ! 0 |  981 | `						pBad = &pGen->pIn[-1];` |
|     ! 0 |  982 | `					}` |
|       1 |  983 | `				}` |
|       3 |  984 | `				rc = PH7_GenSyntaxError(pGen,pBad,"\"while\"");` |
|       - |  985 | `			}` |
|       3 |  986 | `			if( rc == SXERR_ABORT ){` |
|       - |  987 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  988 | `				return SXERR_ABORT;` |
|       - |  989 | `			}` |
|       3 |  990 | `			goto Synchronize;` |
|       - |  991 | `	}` |
|       - |  992 | `	/* Jump the 'while' keyword */` |
|       9 |  993 | `	pGen->pIn++;` |
|       9 |  994 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  995 | `		/* Syntax error */` |
|     ! 0 |  996 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  997 | `		if( rc == SXERR_ABORT ){` |
|       - |  998 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  999 | `			return SXERR_ABORT;` |
|       - | 1000 | `		}` |
|     ! 0 | 1001 | `		goto Synchronize;` |
|       - | 1002 | `	}` |
|       - | 1003 | `	/* Jump the left parenthesis '(' */` |
|       9 | 1004 | `	pGen->pIn++;` |
|       - | 1005 | `	/* Delimit the condition */` |
|       9 | 1006 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|       9 | 1007 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 1008 | `		/* Empty condition. php reports the token that actually stopped it -- for` |
|       - | 1009 | ``		 * `while ()` that is the ')' -- not a hand-written "expected expression". */`` |
|     ! 0 | 1010 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,0);` |
|     ! 0 | 1011 | `		if( rc == SXERR_ABORT ){` |
|       - | 1012 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1013 | `			return SXERR_ABORT;` |
|       - | 1014 | `		}` |
|     ! 0 | 1015 | `		goto Synchronize;` |
|       - | 1016 | `	}` |
|       - | 1017 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|       9 | 1018 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|       - | 1019 | `		JumpFixup *aPost;` |
|       - | 1020 | `		VmInstr *pInstr;` |
|       - | 1021 | `		sxu32 nJumpDest;` |
|       - | 1022 | `		sxu32 n;` |
|       3 | 1023 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|       3 | 1024 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       5 | 1025 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|       3 | 1026 | `			pInstr = GenStateFixupInstr(&aPost[n]);` |
|       3 | 1027 | `			if( pInstr ){` |
|       - | 1028 | `				/* Fix */` |
|       3 | 1029 | `				pInstr->iP2 = nJumpDest;` |
|       1 | 1030 | `			}` |
|       2 | 1031 | `		}` |
|       1 | 1032 | `	}` |
|       - | 1033 | `	/* Swap token streams */` |
|       9 | 1034 | `	pTmp = pGen->pEnd;` |
|       9 | 1035 | `	pGen->pEnd = pEnd;` |
|       - | 1036 | `	/* Compile the expression */` |
|       9 | 1037 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       9 | 1038 | `	if( rc == SXERR_ABORT ){` |
|       - | 1039 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 1040 | `		return SXERR_ABORT;` |
|       - | 1041 | `	}` |
|       - | 1042 | `	/* Update token stream */` |
|       9 | 1043 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 1044 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|     ! 0 | 1045 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1046 | `			return SXERR_ABORT;` |
|       - | 1047 | `		}` |
|     ! 0 | 1048 | `		pGen->pIn++;` |
|     ! 0 | 1049 | `	}` |
|       9 | 1050 | `	pGen->pIn  = &pEnd[1];` |
|       9 | 1051 | `	pGen->pEnd = pTmp;` |
|       - | 1052 | `	/* Emit the true jump to the beginning of the loop */` |
|       9 | 1053 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|       - | 1054 | `	/* Fix all jumps now the destination is resolved */` |
|       9 | 1055 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 1056 | `	/* Release the loop block */` |
|       9 | 1057 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 1058 | `	/* Statement successfully compiled */` |
|       9 | 1059 | `	return SXRET_OK;` |
|       1 | 1060 | `Synchronize:` |
|       - | 1061 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 1062 | `	 * compiling this erroneous block.` |
|       - | 1063 | `	 */` |
|       3 | 1064 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 1065 | `		pGen->pIn++;` |
|     ! 0 | 1066 | `	}` |
|       3 | 1067 | `	return SXRET_OK;` |
|       7 | 1068 | `}` |
|       - | 1069 | `/*` |
|       - | 1070 | ` * Compile the complex and powerful 'for' statement.` |
|       - | 1071 | ` * According to the PHP language reference` |
|       - | 1072 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|       - | 1073 | ` *  The syntax of a for loop is:` |
|       - | 1074 | ` *  for (expr1; expr2; expr3)` |
|       - | 1075 | ` *   statement` |
|       - | 1076 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|       - | 1077 | ` *  the beginning of the loop.` |
|       - | 1078 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|       - | 1079 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|       - | 1080 | ` *  to FALSE, the execution of the loop ends.` |
|       - | 1081 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|       - | 1082 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|       - | 1083 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|       - | 1084 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|       - | 1085 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|       - | 1086 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|       - | 1087 | ` *  of using the for truth expression.` |
|       - | 1088 | ` */` |
|   31158 | 1089 | `PH7_PRIVATE sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       5 | 1090 | `{` |
|   31163 | 1091 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   31163 | 1092 | `	GenBlock *pForBlock = 0;` |
|       - | 1093 | `	sxu32 nFalseJump;` |
|       - | 1094 | `	sxu32 nLine;` |
|       - | 1095 | `	sxi32 rc;` |
|   31163 | 1096 | `	nLine = pGen->pIn->nLine;` |
|       - | 1097 | `	/* Jump the 'for' keyword */` |
|   31163 | 1098 | `	pGen->pIn++;` |
|   31163 | 1099 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 1100 | `		/* Syntax error */` |
|     ! 0 | 1101 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 | 1102 | `		if( rc == SXERR_ABORT ){` |
|       - | 1103 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1104 | `			return SXERR_ABORT;` |
|       - | 1105 | `		}` |
|     ! 0 | 1106 | `		return SXRET_OK;` |
|       - | 1107 | `	}` |
|       - | 1108 | `	/* Jump the left parenthesis '(' */` |
|   31163 | 1109 | `	pGen->pIn++;` |
|       - | 1110 | `	/* Delimit the init-expr;condition;post-expr */` |
|   31163 | 1111 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   31163 | 1112 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 1113 | `		/* Empty expression */` |
|     ! 0 | 1114 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|     ! 0 | 1115 | `		if( rc == SXERR_ABORT ){` |
|       - | 1116 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1117 | `			return SXERR_ABORT;` |
|       - | 1118 | `		}` |
|       - | 1119 | `		/* Synchronize */` |
|     ! 0 | 1120 | `		pGen->pIn = pEnd;` |
|     ! 0 | 1121 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 1122 | `			pGen->pIn++;` |
|     ! 0 | 1123 | `		}` |
|     ! 0 | 1124 | `		return SXRET_OK;` |
|       - | 1125 | `	}` |
|       - | 1126 | `	/* Swap token streams */` |
|   31163 | 1127 | `	pTmp = pGen->pEnd;` |
|   31163 | 1128 | `	pGen->pEnd = pEnd;` |
|       - | 1129 | `	/* for() clauses are the ONLY place php's grammar allows a comma-separated` |
|       - | 1130 | `	 * expression list, so the comma operator is permitted for their duration` |
|       - | 1131 | `	 * (see GenStateTreeHasComma). A closure body nested inside a clause is` |
|       - | 1132 | `	 * compiled through this same window — recorded as a known leniency. */` |
|   31163 | 1133 | `	pGen->nCommaExprOk++;` |
|   31163 | 1134 | `	pGen->zClauseCloser = "\";\""; /* init/condition clauses close on ';' */` |
|       - | 1135 | `	/* Compile initialization expressions if available */` |
|   31163 | 1136 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 1137 | `	/* Pop operand lvalues */` |
|   31163 | 1138 | `	if( rc == SXERR_ABORT ){` |
|       - | 1139 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 1140 | `		return SXERR_ABORT;` |
|   31163 | 1141 | `	}else if( rc != SXERR_EMPTY ){` |
|   31161 | 1142 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|   15578 | 1143 | `	}` |
|   31163 | 1144 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 1145 | `		/* Syntax error */` |
|     ! 0 | 1146 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|     ! 0 | 1147 | `		if( rc == SXERR_ABORT ){` |
|       - | 1148 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1149 | `			return SXERR_ABORT;` |
|       - | 1150 | `		}` |
|     ! 0 | 1151 | `		return SXRET_OK;` |
|       - | 1152 | `	}` |
|       - | 1153 | `	/* Jump the trailing ';' */` |
|   31163 | 1154 | `	pGen->pIn++;` |
|       - | 1155 | `	/* Create the loop block */` |
|   31163 | 1156 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   31163 | 1157 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1158 | `		return SXERR_ABORT;` |
|       - | 1159 | `	}` |
|       - | 1160 | `	/* Deffer continue jumps */` |
|   31163 | 1161 | `	pForBlock->bPostContinue = TRUE;` |
|       - | 1162 | `	/* Compile the condition */` |
|   31163 | 1163 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   31163 | 1164 | `	if( rc == SXERR_ABORT ){` |
|       - | 1165 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 1166 | `		return SXERR_ABORT;` |
|   31163 | 1167 | `	}else if( rc != SXERR_EMPTY ){` |
|       - | 1168 | `		/* Emit the false jump */` |
|   31161 | 1169 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 1170 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   31161 | 1171 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|   15578 | 1172 | `	}` |
|   31163 | 1173 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 1174 | `		/* Syntax error */` |
|       6 | 1175 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|       6 | 1176 | `		if( rc == SXERR_ABORT ){` |
|       - | 1177 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1178 | `			return SXERR_ABORT;` |
|       - | 1179 | `		}` |
|       6 | 1180 | `		return SXRET_OK;` |
|       - | 1181 | `	}` |
|       - | 1182 | `	/* Jump the trailing ';' */` |
|   31159 | 1183 | `	pGen->pIn++;` |
|       - | 1184 | `	/* Save the post condition stream */` |
|   31159 | 1185 | `	pPostStart = pGen->pIn;` |
|       - | 1186 | `	/* Compile the loop body — OUTSIDE the comma window (the body is ordinary` |
|       - | 1187 | ``	 * php, so `(1, 2)` inside it is the parse error it should be). */`` |
|   31159 | 1188 | `	pGen->nCommaExprOk--;` |
|   31159 | 1189 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   31159 | 1190 | `	pGen->pEnd = pTmp;` |
|   31159 | 1191 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   31159 | 1192 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 1193 | `		return SXERR_ABORT;` |
|       - | 1194 | `	}` |
|       - | 1195 | `	/* Fix post-continue jumps */` |
|   31159 | 1196 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - | 1197 | `		JumpFixup *aPost;` |
|       - | 1198 | `		VmInstr *pInstr;` |
|       - | 1199 | `		sxu32 nJumpDest;` |
|       - | 1200 | `		sxu32 n;` |
|   10315 | 1201 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|   10315 | 1202 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|   25771 | 1203 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|   15461 | 1204 | `			pInstr = GenStateFixupInstr(&aPost[n]);` |
|   15461 | 1205 | `			if( pInstr ){` |
|       - | 1206 | `				/* Fix jump */` |
|   15461 | 1207 | `				pInstr->iP2 = nJumpDest;` |
|    7728 | 1208 | `			}` |
|    7733 | 1209 | `		}` |
|    5155 | 1210 | `	}` |
|       - | 1211 | `	/* compile the post-expressions if available */` |
|   31159 | 1212 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 1213 | `		pPostStart++;` |
|     ! 0 | 1214 | `	}` |
|   31159 | 1215 | `	if( pPostStart < pEnd ){` |
|       - | 1216 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   31159 | 1217 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   31159 | 1218 | `		pGen->nCommaExprOk++; /* post-expressions are a clause list again */` |
|   31159 | 1219 | `		pGen->zClauseCloser = "\")\""; /* the post clause closes on ')' */` |
|   31159 | 1220 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   31159 | 1221 | `		pGen->nCommaExprOk--;` |
|   31159 | 1222 | `		pGen->zClauseCloser = 0;` |
|   31159 | 1223 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 1224 | `			/* php names the token and expects ')'; the post-clause runs to the ')'. */` |
|     ! 0 | 1225 | `			rc = PH7_GenSyntaxError(pGen,pGen->pIn,"\")\"");` |
|     ! 0 | 1226 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1227 | `				return SXERR_ABORT;` |
|       - | 1228 | `			}` |
|     ! 0 | 1229 | `			return SXRET_OK;` |
|       - | 1230 | `		}` |
|   31159 | 1231 | `		RE_SWAP_DELIMITER(pGen);` |
|   31159 | 1232 | `		if( rc == SXERR_ABORT ){` |
|       - | 1233 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 1234 | `			return SXERR_ABORT;` |
|   31159 | 1235 | `		}else if( rc != SXERR_EMPTY){` |
|       - | 1236 | `			/* Pop operand lvalue */` |
|   31159 | 1237 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|   15577 | 1238 | `		}` |
|   15577 | 1239 | `	}` |
|       - | 1240 | `	/* Emit the unconditional jump to the start of the loop */` |
|   31159 | 1241 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - | 1242 | `	/* Fix all jumps now the destination is resolved */` |
|   31159 | 1243 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 1244 | `	/* Release the loop block */` |
|   31159 | 1245 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 1246 | `	/* Statement successfully compiled */` |
|   31159 | 1247 | `	return SXRET_OK;` |
|   15584 | 1248 | `}` |
|       - | 1249 | `/*` |
|       - | 1250 | ` * Is this keyword token part of a NAME rather than a keyword in its own right?` |
|       - | 1251 | ` *` |
|       - | 1252 | `` * The lexer marks `as` a keyword wherever it appears, and php lets it BE a name`` |
|       - | 1253 | ` * in three places the token stream reaches through a preceding operator: a` |
|       - | 1254 | ` * variable ($as lexes as PH7_TK_DOLLAR followed by the identifier), a property` |
|       - | 1255 | ` * ($o->as, $o?->as) and a class constant (A::as). foreach's separator scan has` |
|       - | 1256 | ` * to look at what PRECEDES the keyword, or it splits inside the name and hands` |
|       - | 1257 | `` * the subject compiler a bare `$`.`` |
|       - | 1258 | ` */` |
|   58554 | 1259 | `static int GenStateKeywordIsName(SyToken *pStart,SyToken *pCur)` |
|       5 | 1260 | `{` |
|       - | 1261 | `	SyToken *pPrev;` |
|   58559 | 1262 | `	if( pCur <= pStart ){` |
|     ! 0 | 1263 | `		return 0;` |
|       - | 1264 | `	}` |
|   58559 | 1265 | `	pPrev = pCur - 1;` |
|   58559 | 1266 | `	if( pPrev->nType & PH7_TK_DOLLAR ){` |
|       6 | 1267 | `		return 1;` |
|       - | 1268 | `	}` |
|   58555 | 1269 | `	if( (pPrev->nType & PH7_TK_OP) == 0 ){` |
|   58547 | 1270 | `		return 0;` |
|       - | 1271 | `	}` |
|      13 | 1272 | `	return (pPrev->sData.nByte == sizeof("->")-1` |
|       6 | 1273 | `			&& SyMemcmp(pPrev->sData.zString,"->",sizeof("->")-1) == 0)` |
|       7 | 1274 | `		\|\| (pPrev->sData.nByte == sizeof("::")-1` |
|       3 | 1275 | `			&& SyMemcmp(pPrev->sData.zString,"::",sizeof("::")-1) == 0)` |
|      16 | 1276 | `		\|\| (pPrev->sData.nByte == sizeof("?->")-1` |
|       4 | 1277 | `			&& SyMemcmp(pPrev->sData.zString,"?->",sizeof("?->")-1) == 0);` |
|   29282 | 1278 | `}` |
|       - | 1279 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - | 1280 | ` *` |
|       - | 1281 | `` * php's `as` target is any WRITABLE expression, not just a variable: a property`` |
|       - | 1282 | ` * ($o->p), a static property (C::$s), an array element ($a['k']) and an append` |
|       - | 1283 | ` * ($a[]) are all accepted, on the key side as much as on the value side. The` |
|       - | 1284 | ` * three shapes php rejects get php's own wording; everything else keeps PH7's.` |
|       - | 1285 | ` */` |
|   84718 | 1286 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 | 1287 | `{` |
|   84723 | 1288 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|   84723 | 1289 | `	const char *zMsg = 0;` |
|       - | 1290 | ``	/* A loop target is a write target: `as $this` and `as (new A)->p` are php's`` |
|       - | 1291 | `	 * own compile fatals. */` |
|   84723 | 1292 | `	rc = GenStateWriteTargetCheck(&(*pGen),pRoot,0);` |
|   84723 | 1293 | `	if( rc != SXRET_OK ){` |
|       3 | 1294 | `		return rc;` |
|       - | 1295 | `	}` |
|   84721 | 1296 | `	if( pRoot->pOp ){` |
|      35 | 1297 | `		switch( pRoot->pOp->iOp ){` |
|      17 | 1298 | `		case EXPR_OP_ARROW:     /* $o->p */` |
|       - | 1299 | `		case EXPR_OP_DC:        /* C::$s */` |
|       - | 1300 | `		case EXPR_OP_SUBSCRIPT: /* $a['k'], $a[] */` |
|      35 | 1301 | `			return SXRET_OK;` |
|     ! 0 | 1302 | `		case EXPR_OP_NULLSAFE_ARROW:` |
|     ! 0 | 1303 | `			zMsg = "Can't use nullsafe operator in write context";` |
|     ! 0 | 1304 | `			break;` |
|     ! 0 | 1305 | `		case EXPR_OP_FUNC_CALL:` |
|     ! 0 | 1306 | `			zMsg = "Can't use function return value in write context";` |
|     ! 0 | 1307 | `			break;` |
|     ! 0 | 1308 | `		default:` |
|     ! 0 | 1309 | `			break;` |
|       - | 1310 | `		}` |
|   84682 | 1311 | `	}else if( pRoot->xCode == PH7_CompileVariable ){` |
|   84687 | 1312 | `		return SXRET_OK;` |
|       - | 1313 | `	}` |
|       - | 1314 | `	/* Unexpected expression */` |
|     ! 0 | 1315 | `	rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|     ! 0 | 1316 | `		zMsg ? zMsg : "foreach: Expecting a variable name");` |
|     ! 0 | 1317 | `	if( rc != SXERR_ABORT ){` |
|     ! 0 | 1318 | `		rc = SXERR_INVALID;` |
|     ! 0 | 1319 | `	}` |
|     ! 0 | 1320 | `	return rc;` |
|   42364 | 1321 | `}` |
|       - | 1322 | `/*` |
|       - | 1323 | `` * Is this `as` target the plain `$name` shape?`` |
|       - | 1324 | ` *` |
|       - | 1325 | ` * Only that shape can be installed by NAME the way ph7_foreach_info records it` |
|       - | 1326 | ` * (the step writes straight into the frame's symbol table). Every other writable` |
|       - | 1327 | ` * target — including a variable-variable, whose name php re-evaluates per step —` |
|       - | 1328 | ` * goes through a synthetic temporary plus a real store (GenStateForeachStoreTarget).` |
|       - | 1329 | ` */` |
|   84718 | 1330 | `static int GenStateForeachTargetIsPlainVar(SyToken *pStart,SyToken *pEnd)` |
|       5 | 1331 | `{` |
|  127064 | 1332 | `	return (pEnd == &pStart[2])` |
|   84700 | 1333 | `		&& (pStart[0].nType & PH7_TK_DOLLAR)` |
|  127059 | 1334 | `		&& (pStart[1].nType & PH7_TK_ID);` |
|       5 | 1335 | `}` |
|       - | 1336 | `/*` |
|       - | 1337 | `` * Reserve the synthetic temporary a complex `as` target's step value lands in.`` |
|       - | 1338 | ` * The bracketed name cannot collide with a user variable — the same trick the` |
|       - | 1339 | ` * list()/[...] destructuring path uses.` |
|       - | 1340 | ` */` |
|      72 | 1341 | `static sxi32 GenStateForeachTempName(ph7_gen_state *pGen,const char *zTag,SyString *pOut)` |
|       4 | 1342 | `{` |
|       - | 1343 | `	static int iForeachTargetCnt = 0;` |
|       - | 1344 | `	char zTmp[128];` |
|       - | 1345 | `	sxu32 nLen;` |
|       - | 1346 | `	char *zDup;` |
|      76 | 1347 | `	nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_%s_%d__]",zTag,iForeachTargetCnt++);` |
|      76 | 1348 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      76 | 1349 | `	if( zDup == 0 ){` |
|     ! 0 | 1350 | `		return SXERR_ABORT;` |
|       - | 1351 | `	}` |
|      76 | 1352 | `	SyStringInitFromBuf(pOut,zDup,nLen);` |
|      76 | 1353 | `	return SXRET_OK;` |
|      40 | 1354 | `}` |
|       - | 1355 | `/*` |
|       - | 1356 | `` * Emit `<target> = <temp>` (or `<target> =& <temp>` for a by-reference value) for`` |
|       - | 1357 | `` * one complex `as` target, at the top of the loop body — where php performs the`` |
|       - | 1358 | ` * assignment, once per step.` |
|       - | 1359 | ` *` |
|       - | 1360 | ` * The store is folded exactly as the assignment operator's own codegen folds it` |
|       - | 1361 | ` * (compile.c, precedence-18 site): a member LHS keeps its OP_MEMBER, a subscript` |
|       - | 1362 | ` * becomes STORE_IDX, and a plain name folds into the STORE's p3.` |
|       - | 1363 | ` */` |
|      72 | 1364 | `static sxi32 GenStateForeachStoreTarget(` |
|       - | 1365 | `	ph7_gen_state *pGen,` |
|       - | 1366 | `	SyString *pTemp,   /* Synthetic variable holding this step's value/key */` |
|       - | 1367 | `	SyToken *pStart,   /* Target expression token range */` |
|       - | 1368 | `	SyToken *pEnd,` |
|       - | 1369 | `	int bRef           /* True for a by-reference value target */` |
|       - | 1370 | `	)` |
|       4 | 1371 | `{` |
|      76 | 1372 | `	SyToken *pSavedIn = pGen->pIn;` |
|      76 | 1373 | `	SyToken *pSavedEnd = pGen->pEnd;` |
|      76 | 1374 | `	sxi32 iVmOp = bRef ? PH7_OP_STORE_REF : PH7_OP_STORE;` |
|       - | 1375 | `	VmInstr *pInstr;` |
|      76 | 1376 | `	sxi32 iP1 = 0;` |
|      76 | 1377 | `	sxi32 iP2 = 0;` |
|      76 | 1378 | `	void *p3 = 0;` |
|       - | 1379 | `	sxi32 rc;` |
|       - | 1380 | `	/* The value being stored, below the target — the operand order OP_STORE expects. */` |
|      76 | 1381 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(pTemp),0);` |
|      76 | 1382 | `	pGen->pIn = pStart;` |
|      76 | 1383 | `	pGen->pEnd = pEnd;` |
|      76 | 1384 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE,` |
|       - | 1385 | `		GenStateForEachNodeValidator);` |
|      76 | 1386 | `	pGen->pIn = pSavedIn;` |
|      76 | 1387 | `	pGen->pEnd = pSavedEnd;` |
|      76 | 1388 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 1389 | `		return SXERR_ABORT;` |
|      76 | 1390 | `	}else if( rc != SXRET_OK ){` |
|       - | 1391 | `		/* The validator already reported it; drop the pushed value and carry on. */` |
|     ! 0 | 1392 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 | 1393 | `		return SXRET_OK;` |
|       - | 1394 | `	}` |
|      76 | 1395 | `	pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      76 | 1396 | `	if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 1397 | `		/* A member target resolves (and, for a reference, stashes) its own slot. */` |
|      19 | 1398 | `		if( bRef ){` |
|       3 | 1399 | `			pInstr->iP2 = PH7_MEMBER_REF_TARGET;` |
|       1 | 1400 | `		}` |
|      19 | 1401 | `		iP2 = 1;` |
|      67 | 1402 | `	}else if( pInstr ){` |
|      58 | 1403 | `		(void)PH7_VmPopInstr(pGen->pVm);` |
|      58 | 1404 | `		if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|      17 | 1405 | `			iVmOp = bRef ? PH7_OP_STORE_IDX_REF : PH7_OP_STORE_IDX;` |
|      17 | 1406 | `			iP1 = pInstr->iP1;` |
|      17 | 1407 | `			if( bRef ){` |
|       3 | 1408 | `				iP2 = pInstr->iP2;` |
|       3 | 1409 | `				p3 = pInstr->p3;` |
|       1 | 1410 | `			}` |
|       9 | 1411 | `		}else{` |
|      42 | 1412 | `			p3 = pInstr->p3;` |
|       - | 1413 | `		}` |
|      27 | 1414 | `	}` |
|      76 | 1415 | `	PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|       - | 1416 | `	/* Discard the stored value the store leaves behind */` |
|      76 | 1417 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      76 | 1418 | `	if( bRef ){` |
|       - | 1419 | `		/* The target now holds the element; drop the temporary's own hold, or it` |
|       - | 1420 | `		 * would keep the element a REFERENCE for the rest of the script — an extra` |
|       - | 1421 | ``		 * holder no `unset()` the program can write is able to reach. Dropping the`` |
|       - | 1422 | `		 * NAME never releases the slot the target still refers to. */` |
|       5 | 1423 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UNSET_VAR,0,0,(void *)pTemp,0);` |
|       2 | 1424 | `	}` |
|      76 | 1425 | `	return SXRET_OK;` |
|      40 | 1426 | `}` |
|       - | 1427 | `/*` |
|       - | 1428 | ` * Compile the 'foreach' statement.` |
|       - | 1429 | ` * According to the PHP language reference` |
|       - | 1430 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|       - | 1431 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|       - | 1432 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|       - | 1433 | ` *  is a minor but useful extension of the first:` |
|       - | 1434 | ` *  foreach (array_expression as $value)` |
|       - | 1435 | ` *    statement` |
|       - | 1436 | ` *  foreach (array_expression as $key => $value)` |
|       - | 1437 | ` *   statement` |
|       - | 1438 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|       - | 1439 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|       - | 1440 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|       - | 1441 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|       - | 1442 | ` *  to the variable $key on each loop.` |
|       - | 1443 | ` *  Note:` |
|       - | 1444 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|       - | 1445 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|       - | 1446 | ` *  Note:` |
|       - | 1447 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|       - | 1448 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|       - | 1449 | ` *  or after the foreach without resetting it.` |
|       - | 1450 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|       - | 1451 | ` *  of copying the value.` |
|       - | 1452 | ` */` |
|   58542 | 1453 | `PH7_PRIVATE sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       5 | 1454 | `{` |
|   58547 | 1455 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|   58547 | 1456 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|       - | 1457 | ``	/* Token ranges of a KEY / VALUE target that is not a plain `$name`: it is`` |
|       - | 1458 | `	 * compiled as a real store at the top of the loop body (php assigns the value` |
|       - | 1459 | `	 * first, then the key), against a synthetic temporary the step writes. */` |
|   58547 | 1460 | `	SyToken *pKeyStart = 0,*pKeyEnd = 0;` |
|   58547 | 1461 | `	SyToken *pValStart = 0,*pValEnd = 0;` |
|   58547 | 1462 | `	GenBlock *pForeachBlock = 0;` |
|       - | 1463 | `	ph7_foreach_info *pInfo;` |
|       - | 1464 | `	sxu32 nFalseJump;` |
|       - | 1465 | `	VmInstr *pInstr;` |
|       - | 1466 | `	sxu32 nLine;` |
|       - | 1467 | `	sxi32 rc;` |
|   58547 | 1468 | `	nLine = pGen->pIn->nLine;` |
|       - | 1469 | `	/* Jump the 'foreach' keyword */` |
|   58547 | 1470 | `	pGen->pIn++;` |
|   58547 | 1471 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 1472 | `		/* Syntax error */` |
|     ! 0 | 1473 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 | 1474 | `		if( rc == SXERR_ABORT ){` |
|       - | 1475 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1476 | `			return SXERR_ABORT;` |
|       - | 1477 | `		}` |
|     ! 0 | 1478 | `		goto Synchronize;` |
|       - | 1479 | `	}` |
|       - | 1480 | `	/* Jump the left parenthesis '(' */` |
|   58547 | 1481 | `	pGen->pIn++;` |
|       - | 1482 | `	/* Create the loop block */` |
|   58547 | 1483 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|   58547 | 1484 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1485 | `		return SXERR_ABORT;` |
|       - | 1486 | `	}` |
|       - | 1487 | `	/* Delimit the expression */` |
|   58547 | 1488 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   58547 | 1489 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 1490 | `		/* Empty expression */` |
|     ! 0 | 1491 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|     ! 0 | 1492 | `		if( rc == SXERR_ABORT ){` |
|       - | 1493 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1494 | `			return SXERR_ABORT;` |
|       - | 1495 | `		}` |
|       - | 1496 | `		/* Synchronize */` |
|     ! 0 | 1497 | `		pGen->pIn = pEnd;` |
|     ! 0 | 1498 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 1499 | `			pGen->pIn++;` |
|     ! 0 | 1500 | `		}` |
|     ! 0 | 1501 | `		return SXRET_OK;` |
|       - | 1502 | `	}` |
|       - | 1503 | `	/* Compile the array expression.` |
|       - | 1504 | `	 *` |
|       - | 1505 | ``	 * The separator is the first TOP-LEVEL `as`: one nested inside brackets, parens`` |
|       - | 1506 | `	 * or braces belongs to something else the iterated expression contains — a` |
|       - | 1507 | ``	 * closure with a `foreach` of its own is the shape that finds this, and cutting`` |
|       - | 1508 | ``	 * at its inner `as` left the outer expression with an unclosed bracket and made`` |
|       - | 1509 | ``	 * `foreach ([function(){ foreach ([1] as $k) … }] as $f)` a compile fatal on`` |
|       - | 1510 | ``	 * source php runs. An `as` that is part of a NAME ($as, $o->as, A::as) is not a`` |
|       - | 1511 | `	 * separator either. */` |
|   58547 | 1512 | `	pCur = pGen->pIn;` |
|       - | 1513 | `	{` |
|   58547 | 1514 | `		sxi32 iNest = 0;` |
|  208943 | 1515 | `		while( pCur < pEnd ){` |
|  208943 | 1516 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|    7077 | 1517 | `				iNest++;` |
|  205407 | 1518 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       - | 1519 | `				/* A mismatch here is the expression parser's to report. */` |
|    7077 | 1520 | `				iNest--;` |
|  198335 | 1521 | `			}else if( iNest <= 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|   74017 | 1522 | `				sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|   74017 | 1523 | `				if( nKeywrd == PH7_TKWRD_AS && !GenStateKeywordIsName(pGen->pIn,pCur) ){` |
|   58547 | 1524 | `					break;` |
|       - | 1525 | `				}` |
|    7735 | 1526 | `			}` |
|       - | 1527 | `			/* Advance the stream cursor */` |
|  150401 | 1528 | `			pCur++;` |
|       5 | 1529 | `		}` |
|       - | 1530 | `	}` |
|   58547 | 1531 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 | 1532 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - | 1533 | `			"foreach: Missing array/object expression");` |
|     ! 0 | 1534 | `		if( rc == SXERR_ABORT ){` |
|       - | 1535 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 1536 | `			return SXERR_ABORT;` |
|       - | 1537 | `		}` |
|     ! 0 | 1538 | `		goto Synchronize;` |
|       - | 1539 | `	}` |
|       - | 1540 | `	/* Swap token streams */` |
|   58547 | 1541 | `	pTmp = pGen->pEnd;` |
|   58547 | 1542 | `	pGen->pEnd = pCur;` |
|   58547 | 1543 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   58547 | 1544 | `	if( rc == SXERR_ABORT ){` |
|       - | 1545 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 1546 | `		return SXERR_ABORT;` |
|       - | 1547 | `	}` |
|       - | 1548 | `	/* Update token stream */` |
|   58547 | 1549 | `	while(pGen->pIn < pCur ){` |
|     ! 0 | 1550 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 1551 | `		if( rc == SXERR_ABORT ){` |
|       - | 1552 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 1553 | `			return SXERR_ABORT;` |
|       - | 1554 | `		}` |
|     ! 0 | 1555 | `		pGen->pIn++;` |
|     ! 0 | 1556 | `	}` |
|   58547 | 1557 | `	pCur++; /* Jump the 'as' keyword */` |
|   58547 | 1558 | `	pGen->pIn = pCur;` |
|   58547 | 1559 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 1560 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 | 1561 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1562 | `			return SXERR_ABORT;` |
|       - | 1563 | `		}` |
|     ! 0 | 1564 | `	}` |
|       - | 1565 | `	/* Create the foreach context */` |
|   58547 | 1566 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|   58547 | 1567 | `	if( pInfo == 0 ){` |
|     ! 0 | 1568 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 1569 | `		return SXERR_ABORT;` |
|       - | 1570 | `	}` |
|       - | 1571 | `	/* Zero the structure */` |
|   58547 | 1572 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - | 1573 | `	/* Initialize structure fields */` |
|   58547 | 1574 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - | 1575 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|       - | 1576 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|       - | 1577 | `	 * '=>'. */` |
|   58547 | 1578 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|   58547 | 1579 | `	if( pCur < pEnd ){` |
|       - | 1580 | `		/* Compile the expression holding the key name */` |
|   26263 | 1581 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 | 1582 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 | 1583 | `			if( rc == SXERR_ABORT ){` |
|       - | 1584 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 1585 | `				return SXERR_ABORT;` |
|     ! 0 | 1586 | `			}` |
|   26263 | 1587 | `		}else if( !GenStateForeachTargetIsPlainVar(pGen->pIn,pCur) ){` |
|       - | 1588 | `			/* A writable but non-name key target ($o->k, C::$s, $a['k'], $$n): the` |
|       - | 1589 | `			 * step lands in a temporary and the store runs in the loop body. */` |
|      13 | 1590 | `			pKeyStart = pGen->pIn;` |
|      13 | 1591 | `			pKeyEnd = pCur;` |
|      13 | 1592 | `			if( GenStateForeachTempName(&(*pGen),"key",&pInfo->sKey) != SXRET_OK ){` |
|     ! 0 | 1593 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1594 | `				return SXERR_ABORT;` |
|       - | 1595 | `			}` |
|      13 | 1596 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       7 | 1597 | `		}else{` |
|   26251 | 1598 | `			pGen->pEnd = pCur;` |
|   26251 | 1599 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|   26251 | 1600 | `			if( rc == SXERR_ABORT ){` |
|       - | 1601 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 1602 | `				return SXERR_ABORT;` |
|       - | 1603 | `			}` |
|   26251 | 1604 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|   26251 | 1605 | `			if( pInstr->p3 ){` |
|       - | 1606 | `				/* Record key name */` |
|   26251 | 1607 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|   13123 | 1608 | `			}` |
|   26251 | 1609 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - | 1610 | `		}` |
|   26263 | 1611 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|   13129 | 1612 | `	}` |
|   58547 | 1613 | `	pGen->pEnd = pEnd;` |
|   58547 | 1614 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 1615 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 | 1616 | `		if( rc == SXERR_ABORT ){` |
|       - | 1617 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 1618 | `			return SXERR_ABORT;` |
|       - | 1619 | `		}` |
|     ! 0 | 1620 | `		goto Synchronize;` |
|       - | 1621 | `	}` |
|   58547 | 1622 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      63 | 1623 | `		pGen->pIn++;` |
|       - | 1624 | `		/* Pass by reference  */` |
|      63 | 1625 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|      30 | 1626 | `	}` |
|       - | 1627 | `	/* Check if the value target is list() */` |
|   58547 | 1628 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 | 1629 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - | 1630 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|       - | 1631 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|       - | 1632 | `		 */` |
|       - | 1633 | `		static int iForeachListCnt = 0;` |
|       - | 1634 | `		char zTmp[128];` |
|       - | 1635 | `		sxu32 nLen;` |
|       - | 1636 | `		char *zDup;` |
|      10 | 1637 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|      10 | 1638 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      10 | 1639 | `		if( zDup == 0 ){` |
|     ! 0 | 1640 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1641 | `			return SXERR_ABORT;` |
|       - | 1642 | `		}` |
|      10 | 1643 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - | 1644 | `		/* Save list() token boundaries */` |
|      10 | 1645 | `		pListStart = pGen->pIn;` |
|       - | 1646 | `		/* Advance past list(...) — validate parentheses */` |
|      10 | 1647 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|      10 | 1648 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 1649 | `			/* php: syntax error, unexpected variable "$x", expecting "(" */` |
|       3 | 1650 | `			rc = PH7_GenSyntaxError(pGen,pGen->pIn < pEnd ? pGen->pIn : 0,"\"(\"");` |
|       3 | 1651 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1652 | `				return SXERR_ABORT;` |
|       - | 1653 | `			}` |
|       3 | 1654 | `			goto Synchronize;` |
|       - | 1655 | `		}` |
|       7 | 1656 | `		pGen->pIn++; /* Jump '(' */` |
|       7 | 1657 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       7 | 1658 | `		if( pListEnd >= pEnd ){` |
|     ! 0 | 1659 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 1660 | `				"foreach: Missing closing ')' after list");` |
|     ! 0 | 1661 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1662 | `				return SXERR_ABORT;` |
|       - | 1663 | `			}` |
|     ! 0 | 1664 | `			goto Synchronize;` |
|       - | 1665 | `		}` |
|       7 | 1666 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|       7 | 1667 | `		pListEnd = pGen->pIn;` |
|       7 | 1668 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|   58542 | 1669 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - | 1670 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|       - | 1671 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|       - | 1672 | `		 */` |
|       - | 1673 | `		static int iForeachShortListCnt = 0;` |
|       - | 1674 | `		char zTmp[128];` |
|       - | 1675 | `		sxu32 nLen;` |
|       - | 1676 | `		char *zDup;` |
|      77 | 1677 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|      77 | 1678 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      77 | 1679 | `		if( zDup == 0 ){` |
|     ! 0 | 1680 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1681 | `			return SXERR_ABORT;` |
|       - | 1682 | `		}` |
|      77 | 1683 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - | 1684 | `		/* Save [...] token boundaries */` |
|      77 | 1685 | `		pListStart = pGen->pIn;` |
|       - | 1686 | `		/* Advance past [...] */` |
|      77 | 1687 | `		pGen->pIn++; /* Jump '[' */` |
|      77 | 1688 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|      77 | 1689 | `		if( pListEnd >= pEnd ){` |
|     ! 0 | 1690 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 1691 | `				"foreach: Missing closing ']' after short list");` |
|     ! 0 | 1692 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1693 | `				return SXERR_ABORT;` |
|       - | 1694 | `			}` |
|     ! 0 | 1695 | `			goto Synchronize;` |
|       - | 1696 | `		}` |
|      77 | 1697 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|      77 | 1698 | `		pListEnd = pGen->pIn;` |
|      77 | 1699 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|   58502 | 1700 | `	}else if( !GenStateForeachTargetIsPlainVar(pGen->pIn,pEnd) ){` |
|       - | 1701 | `		/* A writable but non-name value target — same treatment as the key above. */` |
|      64 | 1702 | `		pValStart = pGen->pIn;` |
|      64 | 1703 | `		pValEnd = pEnd;` |
|      64 | 1704 | `		if( GenStateForeachTempName(&(*pGen),"val",&pInfo->sValue) != SXRET_OK ){` |
|     ! 0 | 1705 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1706 | `			return SXERR_ABORT;` |
|       - | 1707 | `		}` |
|      34 | 1708 | `	}else{` |
|       - | 1709 | `		/* Compile the expression holding the value name */` |
|   58405 | 1710 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|   58405 | 1711 | `		if( rc == SXERR_ABORT ){` |
|       - | 1712 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 1713 | `			return SXERR_ABORT;` |
|       - | 1714 | `		}` |
|   58405 | 1715 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|   58405 | 1716 | `		if( pInstr->p3 ){` |
|       - | 1717 | `			/* Record value name */` |
|   58405 | 1718 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|   29200 | 1719 | `		}` |
|       - | 1720 | `	}` |
|       - | 1721 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|   58545 | 1722 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - | 1723 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   58545 | 1724 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - | 1725 | `	/* Record the first instruction to execute */` |
|   58545 | 1726 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - | 1727 | `	/* Emit the FOREACH_STEP instruction */` |
|   58545 | 1728 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - | 1729 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   58545 | 1730 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - | 1731 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|   58545 | 1732 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|       - | 1733 | `		SyToken *pSavedIn,*pSavedEnd;` |
|       - | 1734 | `		/* Load the temporary variable holding the current value onto the stack.` |
|       - | 1735 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|       - | 1736 | `		 */` |
|      83 | 1737 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|       - | 1738 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|       - | 1739 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|       - | 1740 | `		 * picks up the delimiter and the variable names inside.` |
|       - | 1741 | `		 */` |
|      83 | 1742 | `		pSavedIn = pGen->pIn;` |
|      83 | 1743 | `		pSavedEnd = pGen->pEnd;` |
|      83 | 1744 | `		pGen->pIn = pListStart;` |
|      83 | 1745 | `		pGen->pEnd = pListEnd;` |
|      83 | 1746 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|      77 | 1747 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|      40 | 1748 | `		}else{` |
|       7 | 1749 | `			rc = PH7_CompileList(&(*pGen),0);` |
|       - | 1750 | `		}` |
|      83 | 1751 | `		pGen->pIn = pSavedIn;` |
|      83 | 1752 | `		pGen->pEnd = pSavedEnd;` |
|      83 | 1753 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1754 | `			return SXERR_ABORT;` |
|       - | 1755 | `		}` |
|       - | 1756 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|      83 | 1757 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      40 | 1758 | `	}` |
|       - | 1759 | `	/* Store this step's value and key into their non-name targets. php performs the` |
|       - | 1760 | `	 * VALUE assignment first — visible through a __set() pair, and the order the` |
|       - | 1761 | `	 * symbol table records the two locals in for the plain-name shape. */` |
|   58545 | 1762 | `	if( pValStart ){` |
|      94 | 1763 | `		rc = GenStateForeachStoreTarget(&(*pGen),&pInfo->sValue,pValStart,pValEnd,` |
|      60 | 1764 | `			(pInfo->iFlags & PH7_4EACH_STEP_REF) != 0);` |
|      64 | 1765 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1766 | `			return SXERR_ABORT;` |
|       - | 1767 | `		}` |
|      30 | 1768 | `	}` |
|   58545 | 1769 | `	if( pKeyStart ){` |
|      13 | 1770 | `		rc = GenStateForeachStoreTarget(&(*pGen),&pInfo->sKey,pKeyStart,pKeyEnd,0);` |
|      13 | 1771 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1772 | `			return SXERR_ABORT;` |
|       - | 1773 | `		}` |
|       6 | 1774 | `	}` |
|       - | 1775 | `	/* Compile the loop body */` |
|   58545 | 1776 | `	pGen->pIn = &pEnd[1];` |
|   58545 | 1777 | `	pGen->pEnd = pTmp;` |
|   58545 | 1778 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|   58545 | 1779 | `	if( rc == SXERR_ABORT ){` |
|       - | 1780 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 1781 | `		return SXERR_ABORT;` |
|       - | 1782 | `	}` |
|       - | 1783 | `	/* Emit the unconditional jump to the start of the loop */` |
|   58545 | 1784 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - | 1785 | `	/* Fix all jumps now the destination is resolved */` |
|   58545 | 1786 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 1787 | `	/* Release the loop block */` |
|   58545 | 1788 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 1789 | `	/* Statement successfully compiled */` |
|   58545 | 1790 | `	return SXRET_OK;` |
|       1 | 1791 | `Synchronize:` |
|       - | 1792 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 1793 | `	 * compiling this erroneous block.` |
|       - | 1794 | `	 */` |
|       3 | 1795 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 1796 | `		pGen->pIn++;` |
|     ! 0 | 1797 | `	}` |
|       3 | 1798 | `	return SXRET_OK;` |
|   29276 | 1799 | `}` |
|       - | 1800 | `/*` |
|       - | 1801 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - | 1802 | ` * According to the PHP language reference` |
|       - | 1803 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - | 1804 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - | 1805 | ` *  that is similar to that of C:` |
|       - | 1806 | ` *  if (expr)` |
|       - | 1807 | ` *   statement` |
|       - | 1808 | ` *  else construct:` |
|       - | 1809 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - | 1810 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - | 1811 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - | 1812 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - | 1813 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - | 1814 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - | 1815 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - | 1816 | ` *  elseif` |
|       - | 1817 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - | 1818 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - | 1819 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - | 1820 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - | 1821 | ` *   than b, a equal to b or a is smaller than b:` |
|       - | 1822 | ` *   <?php` |
|       - | 1823 | ` *    if ($a > $b) {` |
|       - | 1824 | ` *     echo "a is bigger than b";` |
|       - | 1825 | ` *    } elseif ($a == $b) {` |
|       - | 1826 | ` *     echo "a is equal to b";` |
|       - | 1827 | ` *    } else {` |
|       - | 1828 | ` *     echo "a is smaller than b";` |
|       - | 1829 | ` *    }` |
|       - | 1830 | ` *    ?>` |
|       - | 1831 | ` */` |
|  336466 | 1832 | `PH7_PRIVATE sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       5 | 1833 | `{` |
|  336471 | 1834 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  336471 | 1835 | `	GenBlock *pCondBlock = 0;` |
|       - | 1836 | `	sxu32 nJumpIdx;` |
|       - | 1837 | `	sxu32 nKeyID;` |
|       - | 1838 | `	sxi32 rc;` |
|       - | 1839 | `	/* Jump the 'if' keyword */` |
|  336471 | 1840 | `	pGen->pIn++;` |
|  336471 | 1841 | `	pToken = pGen->pIn;` |
|       - | 1842 | `	/* Create the conditional block */` |
|  336471 | 1843 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  336471 | 1844 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1845 | `		return SXERR_ABORT;` |
|       - | 1846 | `	}` |
|       - | 1847 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|  188899 | 1848 | `	for(;;){` |
|  377803 | 1849 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 1850 | `			/* Syntax error */` |
|     ! 0 | 1851 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 1852 | `				pToken--;` |
|     ! 0 | 1853 | `			}` |
|     ! 0 | 1854 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 | 1855 | `			if( rc == SXERR_ABORT ){` |
|       - | 1856 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 1857 | `				return SXERR_ABORT;` |
|       - | 1858 | `			}` |
|     ! 0 | 1859 | `			goto Synchronize;` |
|       - | 1860 | `		}` |
|       - | 1861 | `		/* Jump the left parenthesis '(' */` |
|  377803 | 1862 | `		pToken++;` |
|       - | 1863 | `		/* Delimit the condition */` |
|  377803 | 1864 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  377803 | 1865 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - | 1866 | `			/* Syntax error */` |
|     ! 0 | 1867 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 1868 | `				pToken--;` |
|     ! 0 | 1869 | `			}` |
|     ! 0 | 1870 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 | 1871 | `			if( rc == SXERR_ABORT ){` |
|       - | 1872 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 1873 | `				return SXERR_ABORT;` |
|       - | 1874 | `			}` |
|     ! 0 | 1875 | `			goto Synchronize;` |
|       - | 1876 | `		}` |
|       - | 1877 | `		/* Swap token streams */` |
|  377803 | 1878 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - | 1879 | `		/* Compile the condition */` |
|  377803 | 1880 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 1881 | `		/* Update token stream */` |
|  377803 | 1882 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 | 1883 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|     ! 0 | 1884 | `			pGen->pIn++;` |
|     ! 0 | 1885 | `		}` |
|  377803 | 1886 | `		pGen->pIn  = &pEnd[1];` |
|  377803 | 1887 | `		pGen->pEnd = pTmp;` |
|  377803 | 1888 | `		if( rc == SXERR_ABORT ){` |
|       - | 1889 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       3 | 1890 | `			return SXERR_ABORT;` |
|       - | 1891 | `		}` |
|       - | 1892 | `		/* Emit the false jump */` |
|  377801 | 1893 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - | 1894 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  377801 | 1895 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - | 1896 | `		/* Compile the body */` |
|  377801 | 1897 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  377801 | 1898 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1899 | `			return SXERR_ABORT;` |
|       - | 1900 | `		}` |
|  377801 | 1901 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   88056 | 1902 | `			break;` |
|       - | 1903 | `		}` |
|       - | 1904 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|  201699 | 1905 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  201699 | 1906 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|  129051 | 1907 | `			break;` |
|       - | 1908 | `		}` |
|       - | 1909 | `		/* Emit the unconditional jump */` |
|   72653 | 1910 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - | 1911 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   72653 | 1912 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   72653 | 1913 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   46759 | 1914 | `			pToken = &pGen->pIn[1];` |
|   46759 | 1915 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|   15476 | 1916 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|   15663 | 1917 | `					break;` |
|       - | 1918 | `			}` |
|   15443 | 1919 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    7719 | 1920 | `		}` |
|   41337 | 1921 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - | 1922 | `		/* Synchronize cursors */` |
|   41337 | 1923 | `		pToken = pGen->pIn;` |
|       - | 1924 | `		/* Fix the false jump */` |
|   41337 | 1925 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       5 | 1926 | `	} /* For(;;) */` |
|       - | 1927 | `	/* Fix the false jump */` |
|  336469 | 1928 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  336469 | 1929 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|  160362 | 1930 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - | 1931 | `			/* Compile the else block */` |
|   31321 | 1932 | `			pGen->pIn++;` |
|   31321 | 1933 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   31321 | 1934 | `			if( rc == SXERR_ABORT ){` |
|       - | 1935 |  |
|     ! 0 | 1936 | `				return SXERR_ABORT;` |
|       - | 1937 | `			}` |
|   15658 | 1938 | `	}` |
|  336469 | 1939 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - | 1940 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  336469 | 1941 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - | 1942 | `	/* Release the conditional block */` |
|  336469 | 1943 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 1944 | `	/* Statement successfully compiled */` |
|  336469 | 1945 | `	return SXRET_OK;` |
|     ! 0 | 1946 | `Synchronize:` |
|       - | 1947 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - | 1948 | `	 */` |
|     ! 0 | 1949 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 1950 | `		pGen->pIn++;` |
|     ! 0 | 1951 | `	}` |
|     ! 0 | 1952 | `	return SXRET_OK;` |
|  168238 | 1953 | `}` |
|       - | 1954 | `/*` |
|       - | 1955 | ` * Compile the global construct.` |
|       - | 1956 | ` * According to the PHP language reference` |
|       - | 1957 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - | 1958 | ` *  to be used in that function.` |
|       - | 1959 | ` *  Example #1 Using global` |
|       - | 1960 | ` *  <?php` |
|       - | 1961 | ` *   $a = 1;` |
|       - | 1962 | ` *   $b = 2;` |
|       - | 1963 | ` *   function Sum()` |
|       - | 1964 | ` *   {` |
|       - | 1965 | ` *    global $a, $b;` |
|       - | 1966 | ` *    $b = $a + $b;` |
|       - | 1967 | ` *   }` |
|       - | 1968 | ` *   Sum();` |
|       - | 1969 | ` *   echo $b;` |
|       - | 1970 | ` *  ?>` |
|       - | 1971 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - | 1972 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - | 1973 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - | 1974 | ` */` |
|      72 | 1975 | `PH7_PRIVATE sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       5 | 1976 | `{` |
|      77 | 1977 | `	SyToken *pTmp,*pNext = 0;` |
|       - | 1978 | `	sxi32 nExpr;` |
|       - | 1979 | `	sxi32 rc;` |
|       - | 1980 | `	/* Jump the 'global' keyword */` |
|      77 | 1981 | `	pGen->pIn++;` |
|      77 | 1982 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - | 1983 | `		/* Nothing to process */` |
|     ! 0 | 1984 | `		return SXRET_OK;` |
|       - | 1985 | `	}` |
|      77 | 1986 | `	pTmp = pGen->pEnd;` |
|      77 | 1987 | `	nExpr = 0;` |
|     167 | 1988 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      95 | 1989 | `		if( pGen->pIn < pNext ){` |
|      95 | 1990 | `			pGen->pEnd = pNext;` |
|      95 | 1991 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 1992 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 | 1993 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1994 | `					return SXERR_ABORT;` |
|       - | 1995 | `				}` |
|      90 | 1996 | `			}else if( &pGen->pIn[1] < pGen->pEnd` |
|      90 | 1997 | `			 && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|      85 | 1998 | `			 && pGen->pIn[1].sData.nByte == sizeof("this")-1` |
|      48 | 1999 | `			 && SyMemcmp((const void *)pGen->pIn[1].sData.zString,` |
|       3 | 2000 | `			             (const void *)"this",sizeof("this")-1) == 0 ){` |
|       - | 2001 | ``				/* php refuses `global $this;` at the declaration. */`` |
|       3 | 2002 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - | 2003 | `					"Cannot use $this as global variable");` |
|       3 | 2004 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 2005 | `					return SXERR_ABORT;` |
|       - | 2006 | `				}` |
|       2 | 2007 | `			}else{` |
|      93 | 2008 | `				pGen->pIn++;` |
|      93 | 2009 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - | 2010 | `					/* Emit a warning */` |
|     ! 0 | 2011 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 | 2012 | `				}else{` |
|      93 | 2013 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      93 | 2014 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 2015 | `						return SXERR_ABORT;` |
|      93 | 2016 | `					}else if(rc != SXERR_EMPTY ){` |
|      93 | 2017 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      93 | 2018 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|       - | 2019 | `							/* Variable name, not a constant */` |
|      83 | 2020 | `							pLast->iP1 = 0;` |
|      39 | 2021 | `						}` |
|      93 | 2022 | `						nExpr++;` |
|      44 | 2023 | `					}` |
|       - | 2024 | `				}` |
|       - | 2025 | `			}` |
|      45 | 2026 | `		}` |
|       - | 2027 | `		/* Next expression in the stream */` |
|      95 | 2028 | `		pGen->pIn = pNext;` |
|       - | 2029 | `		/* Jump trailing commas */` |
|     113 | 2030 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|      23 | 2031 | `			pGen->pIn++;` |
|       5 | 2032 | `		}` |
|       5 | 2033 | `	}` |
|       - | 2034 | `	/* Restore token stream */` |
|      77 | 2035 | `	pGen->pEnd = pTmp;` |
|      77 | 2036 | `	if( nExpr > 0 ){` |
|       - | 2037 | `		/* Emit the uplink instruction */` |
|      75 | 2038 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|      35 | 2039 | `	}` |
|      77 | 2040 | `	return SXRET_OK;` |
|      41 | 2041 | `}` |
|       - | 2042 | `/*` |
|       - | 2043 | ` * php's NOUN for a compile-time return diagnostic is the LEXICAL scope, not the` |
|       - | 2044 | `` * kind of function the `return` sits in: zend reads CG(active_class_entry), so a`` |
|       - | 2045 | ` * closure written inside a class body reports "method" and the very same closure` |
|       - | 2046 | ` * written at file scope reports "function". pCurClass is the compiler's exact` |
|       - | 2047 | ` * counterpart (the class/interface/trait/enum whose BODY is being compiled).` |
|       - | 2048 | ` */` |
|      26 | 2049 | `static const char * GenStateReturnNoun(ph7_gen_state *pGen)` |
|       4 | 2050 | `{` |
|      30 | 2051 | `	return pGen->pCurClass ? "method" : "function";` |
|       4 | 2052 | `}` |
|       - | 2053 | `/*` |
|       - | 2054 | ` * TRUE when a declared return type ACCEPTS null — the condition under which php` |
|       - | 2055 | `` * appends its `did you mean "return null;"` hint to the missing-value error.`` |
|       - | 2056 | ``  * That is every nullable declaration (`?T`, `T\|null`, and the standalone `null` `` |
|       - | 2057 | `` * type, all of which set VM_FUNC_RETURN_NULLABLE) plus `mixed`, which includes`` |
|       - | 2058 | ` * null but is stored as a pseudo-CLASS atom rather than through the flag.` |
|       - | 2059 | ` */` |
|      10 | 2060 | `static int GenStateReturnTypeAllowsNull(ph7_vm_func *pFunc)` |
|       4 | 2061 | `{` |
|       - | 2062 | `	SyString *pCls;` |
|      14 | 2063 | `	if( pFunc->iFlags & VM_FUNC_RETURN_NULLABLE ){` |
|       3 | 2064 | `		return 1;` |
|       - | 2065 | `	}` |
|      11 | 2066 | `	pCls = &pFunc->sReturnClass;` |
|       8 | 2067 | `	if( pFunc->nReturnType == SXU32_HIGH && pCls->nByte == sizeof("mixed")-1` |
|       3 | 2068 | `	 && SyStrnicmp(pCls->zString,"mixed",sizeof("mixed")-1) == 0 ){` |
|     ! 0 | 2069 | `		return 1;` |
|       - | 2070 | `	}` |
|      11 | 2071 | `	return 0;` |
|       9 | 2072 | `}` |
|       - | 2073 | `/*` |
|       - | 2074 | ` * Compile the return statement.` |
|       - | 2075 | ` * According to the PHP language reference` |
|       - | 2076 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - | 2077 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - | 2078 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - | 2079 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - | 2080 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - | 2081 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - | 2082 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - | 2083 | ` *  from within the main script file, then script execution end.` |
|       - | 2084 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - | 2085 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - | 2086 | ` *  should do so as PHP has less work to do in this case.` |
|       - | 2087 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - | 2088 | ` */` |
|  248006 | 2089 | `PH7_PRIVATE sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       5 | 2090 | `{` |
|  248011 | 2091 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - | 2092 | `	sxi32 rc;` |
|  248011 | 2093 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  248011 | 2094 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|       - | 2095 | `	ph7_vm_func *pFunc;` |
|       - | 2096 | `	sxu32 nInstrBefore;` |
|       - | 2097 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|       - | 2098 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|       - | 2099 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|       - | 2100 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|       - | 2101 | `	 * normally below so token processing stays consistent. */` |
|  764823 | 2102 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|  516817 | 2103 | `		pFuncBlock = pFuncBlock->pParent;` |
|       5 | 2104 | `	}` |
|  248011 | 2105 | `	pFunc = pFuncBlock ? (ph7_vm_func *)pFuncBlock->pUserData : 0;` |
|  248011 | 2106 | `	if( pFunc && pFunc->nReturnType == MEMOBJ_NEVER ){` |
|       8 | 2107 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       2 | 2108 | `			"A never-returning %s must not return", GenStateReturnNoun(pGen));` |
|       6 | 2109 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2110 | `			return SXERR_ABORT;` |
|       - | 2111 | `		}` |
|       2 | 2112 | `	}` |
|       - | 2113 | `	/* Jump the 'return' keyword */` |
|  248011 | 2114 | `	pGen->pIn++;` |
|  248011 | 2115 | `	nInstrBefore = PH7_VmInstrLength(pGen->pVm);` |
|  248011 | 2116 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2117 | ``		/* php: a stray token after `return EXPR` is `... expecting ";"`. */`` |
|  247953 | 2118 | `		const char *zSave = pGen->zClauseCloser;` |
|  247953 | 2119 | `		pGen->zClauseCloser = "\";\"";` |
|       - | 2120 | ``		/* A `return` READS its operand (the value is consumed), so compile it`` |
|       - | 2121 | ``		 * read-only: a lone undefined variable `return $z` must warn at the read`` |
|       - | 2122 | `		 * exactly like echo/interpolation, not be loaded quietly (the same quiet` |
|       - | 2123 | ``		 * load that correctly keeps a bare `$z;` statement silent). Matches the`` |
|       - | 2124 | `		 * arrow-fn implicit-return body fix. */` |
|  247953 | 2125 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|  247953 | 2126 | `		pGen->zClauseCloser = zSave;` |
|  247953 | 2127 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2128 | `			return SXERR_ABORT;` |
|  247953 | 2129 | `		}else if(rc != SXERR_EMPTY ){` |
|  247953 | 2130 | `			nRet = 1;` |
|  123974 | 2131 | `		}` |
|  123974 | 2132 | `	}` |
|       - | 2133 | ``	/* A bare `return;` inside a function that DECLARES a return type is a php`` |
|       - | 2134 | `	 * COMPILE error, not the runtime TypeError PHL used to raise on the way out:` |
|       - | 2135 | `` 	 * php rejects the program before it runs. `void` (which is what `return;` `` |
|       - | 2136 | ``	 * means) and `never` (handled above) are the two declarations exempt from it,`` |
|       - | 2137 | ``	 * and a GENERATOR is exempt whatever it declares — there `return;` ends the`` |
|       - | 2138 | `	 * generator, and the declared type describes the Generator object the call` |
|       - | 2139 | `	 * produced, never the returned value. */` |
|  248006 | 2140 | `	if( nRet == 0 && pFunc && !pGen->bInGenerator && VmFuncHasReturnType(pFunc)` |
|      36 | 2141 | `	 && pFunc->nReturnType != MEMOBJ_VOID && pFunc->nReturnType != MEMOBJ_NEVER ){` |
|      19 | 2142 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - | 2143 | `			"A %s with return type must return a value%s",` |
|       5 | 2144 | `			GenStateReturnNoun(pGen),` |
|      10 | 2145 | `			GenStateReturnTypeAllowsNull(pFunc)` |
|       - | 2146 | `				? " (did you mean \"return null;\" instead of \"return;\"?)" : "");` |
|      14 | 2147 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2148 | `			return SXERR_ABORT;` |
|       - | 2149 | `		}` |
|       5 | 2150 | `	}` |
|       - | 2151 | ``	/* The mirror rule: a `void` function must not return a VALUE, and php stops`` |
|       - | 2152 | `	 * the program at the return statement rather than throwing on the way out.` |
|       - | 2153 | `	 * Generators keep their own diagnostic (a void generator is rejected as` |
|       - | 2154 | ``	 * `Generator return type must be a supertype of Generator`), so they are`` |
|       - | 2155 | `	 * skipped here exactly as above. php's hint fires when the operand is a` |
|       - | 2156 | ``	 * compile-time constant null; PHL folds the `null` KEYWORD (constant index 0,`` |
|       - | 2157 | `	 * emitted as a lone OP_LOADC and unchanged by any wrapping parens), which is` |
|       - | 2158 | `	 * every shape real code writes. */` |
|  248006 | 2159 | `	if( nRet != 0 && pFunc && !pGen->bInGenerator` |
|  247902 | 2160 | `	 && pFunc->nReturnType == MEMOBJ_VOID ){` |
|      16 | 2161 | `		VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      22 | 2162 | `		int bNullLiteral = (PH7_VmInstrLength(pGen->pVm) == nInstrBefore + 1)` |
|      12 | 2163 | `			&& pLast && pLast->iOp == PH7_OP_LOADC` |
|      18 | 2164 | `			&& pLast->iP1 == 0 && pLast->iP2 == 0;` |
|      22 | 2165 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - | 2166 | `			"A void %s must not return a value%s",` |
|       6 | 2167 | `			GenStateReturnNoun(pGen),` |
|       6 | 2168 | `			bNullLiteral` |
|       - | 2169 | `				? " (did you mean \"return;\" instead of \"return null;\"?)" : "");` |
|      16 | 2170 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2171 | `			return SXERR_ABORT;` |
|       - | 2172 | `		}` |
|       6 | 2173 | `	}` |
|       - | 2174 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|       - | 2175 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|       - | 2176 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|       - | 2177 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|  248011 | 2178 | `	if( pGen->bInGenerator ){` |
|      47 | 2179 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|      47 | 2180 | `		return SXRET_OK;` |
|       - | 2181 | `	}` |
|       - | 2182 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|       - | 2183 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|       - | 2184 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|       - | 2185 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|       - | 2186 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|  247969 | 2187 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|  247969 | 2188 | `	return SXRET_OK;` |
|  124008 | 2189 | `}` |
|       - | 2190 | `/*` |
|       - | 2191 | ` * Compile a yield expression.` |
|       - | 2192 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - | 2193 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - | 2194 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - | 2195 | ` */` |
|     476 | 2196 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       5 | 2197 | `{` |
|       - | 2198 | `	SyToken *pTmp, *pSplit;` |
|     481 | 2199 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|     481 | 2200 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - | 2201 | `	sxi32 rc;` |
|     238 | 2202 | `	(void)iCompileFlag;` |
|       - | 2203 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|     481 | 2204 | `	pGen->pIn++;` |
|       - | 2205 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - | 2206 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|       - | 2207 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|       - | 2208 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|       - | 2209 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|     476 | 2210 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|     275 | 2211 | `		&& pGen->pIn->sData.nByte == 4` |
|      75 | 2212 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|      69 | 2213 | `		pGen->pIn++; /* Skip 'from' */` |
|      69 | 2214 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      69 | 2215 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2216 | `			return SXERR_ABORT;` |
|       - | 2217 | `		}` |
|      69 | 2218 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 | 2219 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|     ! 0 | 2220 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|       - | 2221 | `				"Missing expression after 'yield from'");` |
|     ! 0 | 2222 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2223 | `				return SXERR_ABORT;` |
|       - | 2224 | `			}` |
|     ! 0 | 2225 | `		}` |
|      69 | 2226 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|      69 | 2227 | `		return SXRET_OK;` |
|       - | 2228 | `	}` |
|     417 | 2229 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 2230 | `		/* Bare yield — no value */` |
|       3 | 2231 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|       3 | 2232 | `		return SXRET_OK;` |
|       - | 2233 | `	}` |
|       - | 2234 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|     415 | 2235 | `	pSplit = 0;` |
|       - | 2236 | `	{` |
|     415 | 2237 | `		SyToken *pCur = pGen->pIn;` |
|     415 | 2238 | `		sxi32 nNest = 0;` |
|    1021 | 2239 | `		while( pCur < pGen->pEnd ){` |
|     625 | 2240 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      29 | 2241 | `				nNest++;` |
|     612 | 2242 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      29 | 2243 | `				nNest--;` |
|     586 | 2244 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|      16 | 2245 | `				pSplit = pCur;` |
|      16 | 2246 | `				break;` |
|       - | 2247 | `			}` |
|     611 | 2248 | `			pCur++;` |
|       5 | 2249 | `		}` |
|       - | 2250 | `	}` |
|     415 | 2251 | `	pTmp = pGen->pEnd;` |
|     415 | 2252 | `	if( pSplit ){` |
|       - | 2253 | `		/* yield $key => $value */` |
|      16 | 2254 | `		pGen->pEnd = pSplit;` |
|      16 | 2255 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      16 | 2256 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      16 | 2257 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|      16 | 2258 | `		pGen->pEnd = pTmp;` |
|      16 | 2259 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      16 | 2260 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      16 | 2261 | `		iP1 = 1;` |
|      16 | 2262 | `		iP2 = 1;` |
|       9 | 2263 | `	}else{` |
|       - | 2264 | `		/* yield $value */` |
|     401 | 2265 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     401 | 2266 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     401 | 2267 | `		if( rc != SXERR_EMPTY ){` |
|     401 | 2268 | `			iP1 = 1;` |
|     198 | 2269 | `		}` |
|       - | 2270 | `	}` |
|     415 | 2271 | `	pGen->pEnd = pTmp;` |
|     415 | 2272 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|     415 | 2273 | `	return SXRET_OK;` |
|     243 | 2274 | `}` |
|       - | 2275 | `/*` |
|       - | 2276 | ` * Compile the die/exit language construct.` |
|       - | 2277 | ` * The role of these constructs is to terminate execution of the script.` |
|       - | 2278 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - | 2279 | ` */` |
|     154 | 2280 | `PH7_PRIVATE sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       5 | 2281 | `{` |
|     159 | 2282 | `	sxi32 nExpr = 0;` |
|       - | 2283 | `	sxi32 rc;` |
|       - | 2284 | `	/* Jump the die/exit keyword */` |
|     159 | 2285 | `	pGen->pIn++;` |
|     159 | 2286 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2287 | `		/* Compile the expression */` |
|     159 | 2288 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     159 | 2289 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2290 | `			return SXERR_ABORT;` |
|     159 | 2291 | `		}else if(rc != SXERR_EMPTY ){` |
|     159 | 2292 | `			nExpr = 1;` |
|      77 | 2293 | `		}` |
|      77 | 2294 | `	}` |
|       - | 2295 | `	/* Emit the HALT instruction */` |
|     159 | 2296 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|     159 | 2297 | `	return SXRET_OK;` |
|      82 | 2298 | `}` |
|       - | 2299 | `/*` |
|       - | 2300 | ` * Compile the 'echo' language construct.` |
|       - | 2301 | ` */` |
|   24678 | 2302 | `PH7_PRIVATE sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       5 | 2303 | `{` |
|   24683 | 2304 | `	SyToken *pTmp,*pNext = 0;` |
|   24683 | 2305 | `	sxu32 nLine = pGen->pIn->nLine;` |
|   24683 | 2306 | `	int nExpr = 0;      /* expressions actually compiled */` |
|   24683 | 2307 | `	int bExpectMore = 1;/* after 'echo' or a comma an expression is REQUIRED */` |
|       - | 2308 | `	sxi32 rc;` |
|       - | 2309 | `	/* Jump the 'echo' keyword */` |
|   24683 | 2310 | `	pGen->pIn++;` |
|       - | 2311 | `	/* Compile arguments one after one. php: a stray token in an echo list is` |
|       - | 2312 | ``	 * `... expecting "," or ";"` — set the closer for each argument's compile. */`` |
|   24683 | 2313 | `	pTmp = pGen->pEnd;` |
|       - | 2314 | `	{` |
|   24683 | 2315 | `	const char *zSaveEcho = pGen->zClauseCloser;` |
|   69565 | 2316 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   44893 | 2317 | `		if( pGen->pIn < pNext ){` |
|   44893 | 2318 | `			pGen->pEnd = pNext;` |
|   44893 | 2319 | `			pGen->zClauseCloser = "\",\" or \";\"";` |
|   44893 | 2320 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   44893 | 2321 | `			pGen->zClauseCloser = zSaveEcho;` |
|   44893 | 2322 | `			if( rc == SXERR_ABORT ){` |
|       6 | 2323 | `				return SXERR_ABORT;` |
|   44889 | 2324 | `			}else if( rc != SXERR_EMPTY ){` |
|       - | 2325 | `				/* Emit the consume instruction */` |
|   44865 | 2326 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|   44865 | 2327 | `				nExpr++;` |
|   44865 | 2328 | `				bExpectMore = 0;` |
|   22430 | 2329 | `			}` |
|   22442 | 2330 | `		}` |
|       - | 2331 | `		/* Jump trailing commas (php: exactly one between expressions; a` |
|       - | 2332 | `		 * dangling or doubled comma is a parse error, enforced below) */` |
|   65105 | 2333 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|   20223 | 2334 | `			if( bExpectMore ){` |
|       - | 2335 | `				/* two commas in a row */` |
|       3 | 2336 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,pNext->nLine,` |
|       - | 2337 | `					"syntax error, unexpected token \",\"");` |
|       3 | 2338 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - | 2339 | `			}` |
|   20221 | 2340 | `			bExpectMore = 1;` |
|   20221 | 2341 | `			pNext++;` |
|       5 | 2342 | `		}` |
|   44887 | 2343 | `		pGen->pIn = pNext;` |
|       5 | 2344 | `	}` |
|       - | 2345 | `	}` |
|       - | 2346 | `	/* Restore token stream */` |
|   24677 | 2347 | `	pGen->pEnd = pTmp;` |
|   24677 | 2348 | `	if( nExpr == 0 \|\| bExpectMore ){` |
|       - | 2349 | ``		/* `echo ;` or `echo expr, ;` — php rejects both */`` |
|      32 | 2350 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - | 2351 | `			"syntax error, unexpected token \";\"");` |
|      32 | 2352 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - | 2353 | `	}` |
|   24649 | 2354 | `	return SXRET_OK;` |
|   12344 | 2355 | `}` |
|       - | 2356 | `/*` |
|       - | 2357 | ` * Compile the static statement.` |
|       - | 2358 | ` * According to the PHP language reference` |
|       - | 2359 | ` *  Another important feature of variable scoping is the static variable.` |
|       - | 2360 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - | 2361 | ` *  when program execution leaves this scope.` |
|       - | 2362 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - | 2363 | ` * Symisc eXtension.` |
|       - | 2364 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - | 2365 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 2366 | ` *  Example` |
|       - | 2367 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - | 2368 | ` *    Refer to the official documentation for more information on this feature.` |
|       - | 2369 | ` */` |
|      30 | 2370 | `PH7_PRIVATE sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       4 | 2371 | `{` |
|       - | 2372 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - | 2373 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - | 2374 | `	GenBlock *pBlock;` |
|       - | 2375 | `	SyString *pName;` |
|       - | 2376 | `	char *zDup;` |
|       - | 2377 | `	sxu32 nLine;` |
|       - | 2378 | `	sxi32 rc;` |
|       - | 2379 | ``	/* `static function () {}` / `static fn () =>` at statement position is an`` |
|       - | 2380 | `	 * EXPRESSION statement (a bare static closure), not a static-variable` |
|       - | 2381 | `	 * declaration — hand it to the expression compiler (php accepts it). */` |
|      30 | 2382 | `	if( &pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & PH7_TK_KEYWORD)` |
|      20 | 2383 | `	 && (SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FUNCTION` |
|       1 | 2384 | `	  \|\| SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FN) ){` |
|       3 | 2385 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       3 | 2386 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2387 | `			return SXERR_ABORT;` |
|       3 | 2388 | `		}else if( rc != SXERR_EMPTY ){` |
|       3 | 2389 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 2390 | `		}` |
|       3 | 2391 | `		return SXRET_OK;` |
|       - | 2392 | `	}` |
|       - | 2393 | `	/* Jump the static keyword */` |
|      32 | 2394 | `	nLine = pGen->pIn->nLine;` |
|      32 | 2395 | `	pGen->pIn++;` |
|       - | 2396 | `	/* Extract the enclosing function if any */` |
|      32 | 2397 | `	pBlock = pGen->pCurrent;` |
|      60 | 2398 | `	while( pBlock ){` |
|      60 | 2399 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|      32 | 2400 | `			break;` |
|       - | 2401 | `		}` |
|       - | 2402 | `		/* Point to the upper block */` |
|      32 | 2403 | `		pBlock = pBlock->pParent;` |
|       4 | 2404 | `	}` |
|      32 | 2405 | `	if( pBlock == 0 ){` |
|       - | 2406 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 | 2407 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       - | 2408 | ``			/* php: `static FOO;` is a syntax error naming FOO and expecting "::"`` |
|       - | 2409 | ``			 * (the parser is still open to `static::` at that point). */`` |
|     ! 0 | 2410 | `			rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"::\"");` |
|     ! 0 | 2411 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2412 | `				return SXERR_ABORT;` |
|       - | 2413 | `			}` |
|     ! 0 | 2414 | `			goto Synchronize;` |
|       - | 2415 | `		}` |
|       - | 2416 | `		/* Compile the expression holding the variable */` |
|     ! 0 | 2417 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 | 2418 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2419 | `			return SXERR_ABORT;` |
|     ! 0 | 2420 | `		}else if( rc != SXERR_EMPTY ){` |
|       - | 2421 | `			/* Emit the POP instruction */` |
|     ! 0 | 2422 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 | 2423 | `		}` |
|     ! 0 | 2424 | `		return SXRET_OK;` |
|       - | 2425 | `	}` |
|      32 | 2426 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - | 2427 | `	/* Make sure we are dealing with a valid statement */` |
|      32 | 2428 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      26 | 2429 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 2430 | ``			/* php: `static FOO;` is a syntax error naming FOO and expecting "::"`` |
|       - | 2431 | ``			 * (the parser is still open to `static::` at that point). */`` |
|       3 | 2432 | `			rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"::\"");` |
|       3 | 2433 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2434 | `				return SXERR_ABORT;` |
|       - | 2435 | `			}` |
|       3 | 2436 | `			goto Synchronize;` |
|       - | 2437 | `	}` |
|      30 | 2438 | `	pGen->pIn++;` |
|       - | 2439 | `	/* Extract variable name */` |
|      30 | 2440 | `	pName = &pGen->pIn->sData;` |
|       - | 2441 | ``	/* php refuses `static $this;` at the declaration — the name is not a slot a`` |
|       - | 2442 | `	 * function may own. */` |
|      26 | 2443 | `	if( pName->nByte == sizeof("this")-1` |
|      20 | 2444 | `	 && SyMemcmp((const void *)pName->zString,(const void *)"this",sizeof("this")-1) == 0 ){` |
|       3 | 2445 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - | 2446 | `			"Cannot use $this as static variable");` |
|       3 | 2447 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2448 | `			return SXERR_ABORT;` |
|       - | 2449 | `		}` |
|       3 | 2450 | `		goto Synchronize;` |
|       - | 2451 | `	}` |
|      28 | 2452 | `	pGen->pIn++; /* Jump the var name */` |
|      28 | 2453 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 | 2454 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2455 | `		goto Synchronize;` |
|       - | 2456 | `	}` |
|       - | 2457 | `	/* Initialize the structure describing the static variable */` |
|      28 | 2458 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      28 | 2459 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - | 2460 | `	/* Duplicate variable name */` |
|      28 | 2461 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      28 | 2462 | `	if( zDup == 0 ){` |
|     ! 0 | 2463 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2464 | `		return SXERR_ABORT;` |
|       - | 2465 | `	}` |
|      28 | 2466 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - | 2467 | `	/* Check if we have an expression to compile */` |
|      28 | 2468 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - | 2469 | `		SySet *pInstrContainer;` |
|       - | 2470 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - | 2471 | `		 * Static variable can take any complex expression including function` |
|       - | 2472 | `		 * call as their initialization value.` |
|       - | 2473 | `		 * Example:` |
|       - | 2474 | `		 *		static $var = foo(1,4+5,bar());` |
|       - | 2475 | `		 */` |
|      28 | 2476 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - | 2477 | `		/* Swap bytecode container */` |
|      28 | 2478 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      28 | 2479 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - | 2480 | `		/* Compile the expression */` |
|      28 | 2481 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 2482 | `		/* Emit the done instruction */` |
|      28 | 2483 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - | 2484 | `		/* Restore default bytecode container */` |
|      28 | 2485 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      12 | 2486 | `	}` |
|       - | 2487 | `	/* Finally save the compiled static variable in the appropriate container */` |
|      28 | 2488 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|      28 | 2489 | `	return SXRET_OK;` |
|       2 | 2490 | `Synchronize:` |
|       - | 2491 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - | 2492 | `	 * statement.` |
|       - | 2493 | `	 */` |
|      10 | 2494 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       6 | 2495 | `		pGen->pIn++;` |
|       2 | 2496 | `	}` |
|       6 | 2497 | `	return SXRET_OK;` |
|      19 | 2498 | `}` |
|       - | 2499 | `/*` |
|       - | 2500 | ` * Compile the var statement.` |
|       - | 2501 | ` * Symisc Extension:` |
|       - | 2502 | ` *      var statement can be used outside of a class definition.` |
|       - | 2503 | ` */` |
|       2 | 2504 | `PH7_PRIVATE sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 | 2505 | `{` |
|       - | 2506 | `` 	/* `var` is a PROPERTY modifier and nothing else. A class body's `var $x;` `` |
|       - | 2507 | `	 * is compiled by compile_class.c, never here, so reaching this statement` |
|       - | 2508 | ``	 * handler means `var` appeared outside a class — which php rejects as a`` |
|       - | 2509 | `	 * parse error. PHL used to compile it as an ordinary expression statement,` |
|       - | 2510 | ``	 * so top-level `var $x = 1;` silently ran (a Symisc extension). */`` |
|       3 | 2511 | `	PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       3 | 2512 | `	return SXERR_ABORT;` |
|       1 | 2513 | `}` |
|       - | 2514 | `/*` |
|       - | 2515 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - | 2516 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - | 2517 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - | 2518 | ` */` |
|       - | 2519 | `/*` |
|       - | 2520 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|       - | 2521 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - | 2522 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - | 2523 | ` * qualified name and updates the instruction's operand index.` |
|       - | 2524 | ` *` |
|       - | 2525 | ` * Resolution order:` |
|       - | 2526 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|       - | 2527 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|       - | 2528 | ` *   3. Otherwise return the original literal index unchanged.` |
|       - | 2529 | ` *` |
|       - | 2530 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|       - | 2531 | ` * came from an import (step 1) and 0 otherwise.` |
|       - | 2532 | ` * Returns the (possibly new) literal index.` |
|       - | 2533 | ` */` |
|  741188 | 2534 | `PH7_PRIVATE sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|       5 | 2535 | `{` |
|       - | 2536 | `	ph7_value *pLit;` |
|       - | 2537 | `	const char *zLit;` |
|       - | 2538 | `	SyString sQualified;` |
|       - | 2539 | `	sxu32 nLit;` |
|       - | 2540 | `	sxu32 k;` |
|       - | 2541 | `	sxu32 nNewIdx;` |
|       - | 2542 | `	int hasNsSep;` |
|       - | 2543 | `	SyHashEntry *pImport;` |
|       - | 2544 | `	ph7_value *pNew;` |
|  741193 | 2545 | `	if( pFromImport ){` |
|  644395 | 2546 | `		*pFromImport = 0;` |
|  322195 | 2547 | `	}` |
|  741193 | 2548 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  741193 | 2549 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 | 2550 | `		return nOrigIdx;` |
|       - | 2551 | `	}` |
|  741193 | 2552 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  741193 | 2553 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - | 2554 | `	/* Skip if already qualified (contains backslash) */` |
|  741193 | 2555 | `	hasNsSep = 0;` |
| 7083467 | 2556 | `	for( k = 0; k < nLit; k++ ){` |
| 6342329 | 2557 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 3171142 | 2558 | `	}` |
|  741193 | 2559 | `	if( hasNsSep ){` |
|      54 | 2560 | `		return nOrigIdx;` |
|       - | 2561 | `	}` |
|       - | 2562 | `	/* Check use imports first (works even outside namespaces) */` |
|  741143 | 2563 | `	SyBlobReset(&pGen->sWorker);` |
|  741143 | 2564 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  741143 | 2565 | `	if( pImport ){` |
|      82 | 2566 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      82 | 2567 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      82 | 2568 | `		if( pFromImport ){` |
|      34 | 2569 | `			*pFromImport = 1;` |
|      15 | 2570 | `		}` |
|      43 | 2571 | `	}else{` |
|  741065 | 2572 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  740845 | 2573 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|       - | 2574 | `		}` |
|       - | 2575 | `		/* Prepend current namespace */` |
|     225 | 2576 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     225 | 2577 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|     225 | 2578 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - | 2579 | `	}` |
|       - | 2580 | `	/* Look up or create a new literal for the qualified name */` |
|     303 | 2581 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|     303 | 2582 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|     121 | 2583 | `		return nNewIdx; /* Already interned */` |
|       - | 2584 | `	}` |
|     187 | 2585 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|     187 | 2586 | `	if( pNew == 0 ){` |
|     ! 0 | 2587 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - | 2588 | `	}` |
|     187 | 2589 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|     187 | 2590 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|     187 | 2591 | `	return nNewIdx;` |
|  370599 | 2592 | `}` |
|       - | 2593 | `/*` |
|       - | 2594 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - | 2595 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - | 2596 | ` */` |
|    5062 | 2597 | `PH7_PRIVATE void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 | 2598 | `{` |
|       - | 2599 | `	SyHashEntry *pImport;` |
|    5067 | 2600 | `	const char *zName = pName->zString;` |
|    5067 | 2601 | `	sxu32 nName = pName->nByte;` |
|    5067 | 2602 | `	sxu32 nFirst = 0;` |
|       - | 2603 | `	/* php resolves a name through use-imports on its LEADING segment: an` |
|       - | 2604 | ``	 * unqualified `C` maps the whole name (`use X\C;` -> X\C), while a QUALIFIED`` |
|       - | 2605 | ``	 * `A\B\C` maps just `A` (`use X\A;` -> X\A\B\C) and keeps the `\B\C` tail.`` |
|       - | 2606 | `	 * The old code looked up the whole qualified string (which never matches a` |
|       - | 2607 | `	 * single-segment import alias) and then blindly prefixed the current` |
|       - | 2608 | ``	 * namespace, so `use PHPUnit\Framework; extends Framework\TestCase` resolved`` |
|       - | 2609 | `	 * to Ns\Framework\TestCase. Mirror GenStateResolveNamespaceLiteral here. */` |
|   49749 | 2610 | `	while( nFirst < nName && zName[nFirst] != '\\' ){ nFirst++; }` |
|    5067 | 2611 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)zName,nFirst);` |
|    5067 | 2612 | `	if( pImport ){` |
|      73 | 2613 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      73 | 2614 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|      73 | 2615 | `		SyBlobAppend(pOut,zName + nFirst,nName - nFirst); /* the \B\C tail, if any */` |
|      73 | 2616 | `		return;` |
|       - | 2617 | `	}` |
|       - | 2618 | `	/* Prepend current namespace if active */` |
|    4999 | 2619 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      27 | 2620 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      27 | 2621 | `		SyBlobAppend(pOut,"\\",1);` |
|      12 | 2622 | `	}` |
|    4999 | 2623 | `	SyBlobAppend(pOut,zName,nName);` |
|    2536 | 2624 | `}` |
|       - | 2625 | `/*` |
|       - | 2626 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - | 2627 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - | 2628 | ` * The caller must release pOut when done.` |
|       - | 2629 | ` */` |
|    7920 | 2630 | `PH7_PRIVATE void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 | 2631 | `{` |
|    7925 | 2632 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     477 | 2633 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     477 | 2634 | `		SyBlobAppend(pOut,"\\",1);` |
|     236 | 2635 | `	}` |
|    7925 | 2636 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    7925 | 2637 | `}` |
|       - | 2638 | `/*` |
|       - | 2639 | `` * php's `namespace\X` NAME OPERATOR (5.3): a leading `namespace` keyword glued to`` |
|       - | 2640 | `` * a `\` names the CURRENT namespace, and the whole name is then FULLY QUALIFIED —`` |
|       - | 2641 | `` * `namespace\X` inside `namespace B;` is `\B\X`, and plain `\X` at global scope.`` |
|       - | 2642 | `` * php's lexer matches it as one token (T_NAME_RELATIVE, `"namespace"("\\"{LABEL})+`,`` |
|       - | 2643 | `` * case-insensitively), so the `\` must be GLUED to the keyword: `namespace \X` is a`` |
|       - | 2644 | ` * php parse error, and this mirrors that by comparing source offsets.` |
|       - | 2645 | ` *` |
|       - | 2646 | ` * This predicate only RECOGNIZES the operator (it consumes nothing), which is what` |
|       - | 2647 | `` * the statement dispatcher needs to tell `namespace\X::m();` from a namespace`` |
|       - | 2648 | ` * DECLARATION; GenStateNsRelPrefix below is what the name collectors call.` |
|       - | 2649 | ` */` |
| 1784680 | 2650 | `PH7_PRIVATE int GenStateIsNsRelName(SyToken *pIn,SyToken *pEnd)` |
|       5 | 2651 | `{` |
| 1784680 | 2652 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
| 1407915 | 2653 | `	 \|\| (sxu32)SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_NAMESPACE ){` |
| 1784383 | 2654 | `		return 0;` |
|       - | 2655 | `	}` |
|     307 | 2656 | `	if( &pIn[1] >= pEnd \|\| (pIn[1].nType & PH7_TK_NSSEP) == 0 ){` |
|     213 | 2657 | `		return 0;` |
|       - | 2658 | `	}` |
|      97 | 2659 | `	if( &pIn[2] >= pEnd \|\| (pIn[2].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 2660 | ``		return 0; /* php's T_NAME_RELATIVE needs at least one segment after the `\` */`` |
|       - | 2661 | `	}` |
|       - | 2662 | `	/* Glued? The tokenizer drops whitespace, so adjacency is the source offsets. */` |
|      97 | 2663 | `	return pIn->sData.zString + pIn->sData.nByte == pIn[1].sData.zString;` |
|  892345 | 2664 | `}` |
|       - | 2665 | `/*` |
|       - | 2666 | `` * Consume a leading `namespace\` (see GenStateIsNsRelName) at *ppIn and seed pOut`` |
|       - | 2667 | ` * with the current namespace plus its separator — nothing at global scope, where` |
|       - | 2668 | ` * the bare name already IS the FQN. Returns TRUE when it fired, and the caller` |
|       - | 2669 | `` * must then treat the name it goes on to collect as ABSOLUTE: no `use` import may`` |
|       - | 2670 | ` * apply to it, and the current namespace is already in place.` |
|       - | 2671 | ` */` |
|   54840 | 2672 | `PH7_PRIVATE int GenStateNsRelPrefix(ph7_gen_state *pGen,SyToken **ppIn,SyToken *pEnd,SyBlob *pOut)` |
|       5 | 2673 | `{` |
|   54845 | 2674 | `	SyToken *pIn = *ppIn;` |
|   54845 | 2675 | `	if( !GenStateIsNsRelName(pIn,pEnd) ){` |
|   54757 | 2676 | `		return 0;` |
|       - | 2677 | `	}` |
|      91 | 2678 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      81 | 2679 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      81 | 2680 | `		SyBlobAppend(pOut,"\\",1);` |
|      39 | 2681 | `	}` |
|      91 | 2682 | `	*ppIn = &pIn[2];` |
|      91 | 2683 | `	return 1;` |
|   27425 | 2684 | `}` |
|       - | 2685 | `/*` |
|       - | 2686 | ` * Compile a namespace statement` |
|       - | 2687 | ` * According to the PHP language reference manual` |
|       - | 2688 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - | 2689 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - | 2690 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - | 2691 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - | 2692 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - | 2693 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - | 2694 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - | 2695 | ` *  programming world.` |
|       - | 2696 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - | 2697 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - | 2698 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - | 2699 | ` *  classes/functions/constants.` |
|       - | 2700 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - | 2701 | ` *  readability of source code.` |
|       - | 2702 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - | 2703 | ` *  Here is an example of namespace syntax in PHP:` |
|       - | 2704 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - | 2705 | ` *       class MyClass {}` |
|       - | 2706 | ` *       function myfunction() {}` |
|       - | 2707 | ` *       const MYCONST = 1;` |
|       - | 2708 | ` *       $a = new MyClass;` |
|       - | 2709 | ` *       $c = new \my\name\MyClass;` |
|       - | 2710 | ` *       $a = strlen('hi');` |
|       - | 2711 | ` *       $d = namespace\MYCONST;` |
|       - | 2712 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - | 2713 | ` *       echo constant($d);` |
|       - | 2714 | ` * NOTE` |
|       - | 2715 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - | 2716 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - | 2717 | ` */` |
|       - | 2718 | `/*` |
|       - | 2719 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - | 2720 | ` */` |
|      14 | 2721 | `PH7_PRIVATE const char * TokenTypeName(sxu32 nType)` |
|       4 | 2722 | `{` |
|      18 | 2723 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|      12 | 2724 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|      12 | 2725 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|      12 | 2726 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|      12 | 2727 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|      12 | 2728 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|       3 | 2729 | `	return "token";` |
|      11 | 2730 | `}` |
|     208 | 2731 | `PH7_PRIVATE sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       5 | 2732 | `{` |
|       - | 2733 | `	sxu32 nLine;` |
|       - | 2734 | `	sxi32 rc;` |
|     213 | 2735 | `	nLine = pGen->pIn->nLine;` |
|     213 | 2736 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - | 2737 | `	/* Reset namespace and clear previous use imports */` |
|     213 | 2738 | `	SyBlobReset(&pGen->sNamespace);` |
|     213 | 2739 | `	GenStateResetUseImports(&(*pGen),pGen->pVm);` |
|     213 | 2740 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 2741 | `		return SXRET_OK; /* Global namespace (bare "namespace;") */` |
|       - | 2742 | `	}` |
|     213 | 2743 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|     ! 0 | 2744 | `		return SXRET_OK; /* namespace; — switch to global namespace */` |
|       - | 2745 | `	}` |
|     213 | 2746 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|      10 | 2747 | `		return SXRET_OK; /* namespace { } — global namespace block */` |
|       - | 2748 | `	}` |
|       - | 2749 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     495 | 2750 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     295 | 2751 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - | 2752 | `			/* Append backslash separator */` |
|      51 | 2753 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      51 | 2754 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|      23 | 2755 | `			}` |
|      28 | 2756 | `		}else{` |
|       - | 2757 | `			/* Append identifier */` |
|     249 | 2758 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - | 2759 | `		}` |
|     295 | 2760 | `		pGen->pIn++;` |
|       5 | 2761 | `	}` |
|     205 | 2762 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       8 | 2763 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - | 2764 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 | 2765 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       6 | 2766 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2767 | `			return SXERR_ABORT;` |
|       - | 2768 | `		}` |
|       2 | 2769 | `	}` |
|     205 | 2770 | `	return SXRET_OK;` |
|     109 | 2771 | `}` |
|       - | 2772 | `/*` |
|       - | 2773 | ` * Initialize the three use-import tables of a code generator.` |
|       - | 2774 | ` *` |
|       - | 2775 | ` * php resolves CLASS and FUNCTION imports case-INSENSITIVELY, like every other` |
|       - | 2776 | ``  * name in those two families: `use A\Cee;` then `CEE::K`, `use A\Cee as Alias;` `` |
|       - | 2777 | `` * then `ALIAS::K`, `use function A\eff;` then `EFF()`, and a wrong-case leading`` |
|       - | 2778 | `` * segment of an imported namespace (`use A\B;` then `b\Cee::K`) all resolve.`` |
|       - | 2779 | ` * So both tables fold through SyStrHash/SyStrnmicmp, exactly like hClass /` |
|       - | 2780 | ` * hMethod / hFunction.` |
|       - | 2781 | ` *` |
|       - | 2782 | ` * The CONST table stays BYTE-EXACT: php keeps constant names case-sensitive,` |
|       - | 2783 | `` * so `use const A\KAY;` followed by `kay` must remain an undefined constant.`` |
|       - | 2784 | ` * That asymmetry is why the three tables exist separately.` |
|       - | 2785 | ` */` |
|   30628 | 2786 | `PH7_PRIVATE void GenStateInitUseImports(ph7_gen_state *pGen,ph7_vm *pVm)` |
|       5 | 2787 | `{` |
|   30633 | 2788 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|   30633 | 2789 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|   30633 | 2790 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|   30633 | 2791 | `}` |
|       - | 2792 | `/*` |
|       - | 2793 | ` * Drop every import currently in scope and start a fresh set (a namespace` |
|       - | 2794 | ` * switch clears imports).  Keeps the case rules of GenStateInitUseImports.` |
|       - | 2795 | ` */` |
|   25478 | 2796 | `PH7_PRIVATE void GenStateResetUseImports(ph7_gen_state *pGen,ph7_vm *pVm)` |
|       5 | 2797 | `{` |
|   25483 | 2798 | `	SyHashRelease(&pGen->hUseImports);` |
|   25483 | 2799 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   25483 | 2800 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   25483 | 2801 | `	GenStateInitUseImports(&(*pGen),&(*pVm));` |
|   25483 | 2802 | `}` |
|       - | 2803 | `/*` |
|       - | 2804 | ` * The two DECLARED-name tables (classes and functions declared so far in this` |
|       - | 2805 | ` * compile unit). php refuses an import whose name a declaration already took, and` |
|       - | 2806 | ` * the check is per COMPILE UNIT and case-INSENSITIVE — a class declared by a file` |
|       - | 2807 | `` * this one later `require`s is invisible to it, because that file compiles after`` |
|       - | 2808 | ` * this one has finished. Both tables key on the FQN, so they survive a namespace` |
|       - | 2809 | ` * switch (which clears only the imports).` |
|       - | 2810 | ` */` |
|   30420 | 2811 | `PH7_PRIVATE void GenStateInitSeenSymbols(ph7_gen_state *pGen,ph7_vm *pVm)` |
|       5 | 2812 | `{` |
|   30425 | 2813 | `	SyHashInit(&pGen->hSeenClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|   30425 | 2814 | `	SyHashInit(&pGen->hSeenFunc,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|   30425 | 2815 | `}` |
|   25274 | 2816 | `PH7_PRIVATE void GenStateReleaseSeenSymbols(ph7_gen_state *pGen)` |
|       5 | 2817 | `{` |
|   25279 | 2818 | `	SyHashRelease(&pGen->hSeenClass);` |
|   25279 | 2819 | `	SyHashRelease(&pGen->hSeenFunc);` |
|   25279 | 2820 | `}` |
|   25270 | 2821 | `PH7_PRIVATE void GenStateResetSeenSymbols(ph7_gen_state *pGen,ph7_vm *pVm)` |
|       5 | 2822 | `{` |
|   25275 | 2823 | `	GenStateReleaseSeenSymbols(&(*pGen));` |
|   25275 | 2824 | `	GenStateInitSeenSymbols(&(*pGen),&(*pVm));` |
|   25275 | 2825 | `}` |
|       - | 2826 | `/*` |
|       - | 2827 | ` * Record one declared CLASS (bFunc = 0) or FUNCTION (bFunc = 1) FQN so a later` |
|       - | 2828 | `` * `use` in this compile unit can see that the name is taken.`` |
|       - | 2829 | ` */` |
|  150400 | 2830 | `PH7_PRIVATE void GenStateRecordDeclaredName(ph7_gen_state *pGen,int bFunc,const SyString *pFqn)` |
|       5 | 2831 | `{` |
|  150405 | 2832 | `	SyHash *pHash = bFunc ? &pGen->hSeenFunc : &pGen->hSeenClass;` |
|       - | 2833 | `	char *zDup;` |
|  150405 | 2834 | `	if( pFqn->nByte < 1 \|\| SyHashGet(pHash,pFqn->zString,pFqn->nByte) != 0 ){` |
|      23 | 2835 | `		return;` |
|       - | 2836 | `	}` |
|  150387 | 2837 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pFqn->zString,pFqn->nByte);` |
|  150387 | 2838 | `	if( zDup ){` |
|       - | 2839 | `		/* The blob the caller built is released on its way out, so the table owns` |
|       - | 2840 | `		 * a pool copy (freed in bulk with the VM, like the import FQNs). */` |
|  150387 | 2841 | `		SyHashInsert(pHash,zDup,pFqn->nByte,zDup);` |
|   75191 | 2842 | `	}` |
|   75205 | 2843 | `}` |
|       - | 2844 | `/*` |
|       - | 2845 | `` * php refuses a DECLARATION whose short name a local `use` import already took:`` |
|       - | 2846 | ` *` |
|       - | 2847 | ` *   use A\Cee;  class Cee {}    Cannot redeclare class B\Cee (previously declared as local import)` |
|       - | 2848 | ` *   use function A\eff;  function eff(){}` |
|       - | 2849 | ` *                               Cannot redeclare function B\eff() (previously declared as local import)` |
|       - | 2850 | ` *   use const A\KAY;  const KAY = 1;` |
|       - | 2851 | ` *                               Cannot declare const B\KAY because the name is already in use` |
|       - | 2852 | ` *` |
|       - | 2853 | `` * A SELF-import (`use B\Cee;` inside `namespace B;`) names this very declaration`` |
|       - | 2854 | ` * and is a no-op, so it is exempt. iKind: 0 = class family (interface/trait/enum` |
|       - | 2855 | ` * included — php says "class" for all four), 1 = function, 2 = const.` |
|       - | 2856 | ` */` |
|  150556 | 2857 | `PH7_PRIVATE sxi32 GenStateGuardImportRedeclare(ph7_gen_state *pGen,int iKind,` |
|       - | 2858 | `	const SyString *pShort,const SyString *pFqn,sxu32 nLine)` |
|       5 | 2859 | `{` |
|       - | 2860 | `	SyHash *pImports;` |
|       - | 2861 | `	SyHashEntry *pEntry;` |
|       - | 2862 | `	const char *zImported;` |
|       - | 2863 | `	sxu32 nImported;` |
|  150561 | 2864 | `	switch( iKind ){` |
|  146597 | 2865 | `		case 1:  pImports = &pGen->hUseFuncImports; break;` |
|     161 | 2866 | `		case 2:  pImports = &pGen->hUseConstImports; break;` |
|    3813 | 2867 | `		default: pImports = &pGen->hUseImports; break;` |
|       - | 2868 | `	}` |
|  150561 | 2869 | `	pEntry = SyHashGet(pImports,(const void *)pShort->zString,pShort->nByte);` |
|  150561 | 2870 | `	if( pEntry == 0 ){` |
|  150547 | 2871 | `		return SXRET_OK;` |
|       - | 2872 | `	}` |
|      18 | 2873 | `	zImported = (const char *)pEntry->pUserData;` |
|      18 | 2874 | `	nImported = SyStrlen(zImported);` |
|      14 | 2875 | `	if( nImported == pFqn->nByte` |
|      22 | 2876 | `	 && (iKind == 2 ? SyMemcmp((const void *)zImported,(const void *)pFqn->zString,nImported) == 0` |
|       8 | 2877 | `	                : SyStrnicmp(zImported,pFqn->zString,nImported) == 0) ){` |
|       7 | 2878 | `		return SXRET_OK; /* the import IS this declaration */` |
|       - | 2879 | `	}` |
|      13 | 2880 | `	if( iKind == 2 ){` |
|       4 | 2881 | `		return PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       1 | 2882 | `			"Cannot declare const %z because the name is already in use",pFqn);` |
|       - | 2883 | `	}` |
|       8 | 2884 | `	return PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       2 | 2885 | `		iKind == 1 ? "Cannot redeclare function %z() (previously declared as local import)"` |
|       2 | 2886 | `		           : "Cannot redeclare class %z (previously declared as local import)",pFqn);` |
|   75281 | 2887 | `}` |
|       - | 2888 | `/*` |
|       - | 2889 | `` * Register one resolved `use` import: alias -> FQN, in the table its KIND owns`` |
|       - | 2890 | ` * (iUseType: 0 = class, 1 = function, 2 = const).  Shared by the plain form` |
|       - | 2891 | `` * (`use A\Cee;`) and by each member of a group (`use A\{Cee, Dee};`).`` |
|       - | 2892 | ` */` |
|     158 | 2893 | `static sxi32 GenStateAddImport(` |
|       - | 2894 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 2895 | `	int iUseType,         /* 0=class, 1=function, 2=const */` |
|       - | 2896 | `	SyBlob *pPath,        /* Fully qualified name being imported */` |
|       - | 2897 | `	SyString *pAlias,     /* Short name it is imported under */` |
|       - | 2898 | `	sxu32 nLine           /* Line of the 'use' keyword (for diagnostics) */` |
|       - | 2899 | `	)` |
|       5 | 2900 | `{` |
|       - | 2901 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|       - | 2902 | `	const char *zKind;  /* php's kind word in the "already in use" message */` |
|       - | 2903 | `	char *zDup;` |
|       - | 2904 | `	sxi32 rc;` |
|       - | 2905 | `	/* Select the target hash table based on import type. */` |
|     163 | 2906 | `	switch( iUseType ){` |
|      36 | 2907 | `		case 1:  pGenHash = &pGen->hUseFuncImports; break;` |
|      37 | 2908 | `		case 2:  pGenHash = &pGen->hUseConstImports; break;` |
|      99 | 2909 | `		default: pGenHash = &pGen->hUseImports; break;` |
|       - | 2910 | `	}` |
|       - | 2911 | `	/* php names the KIND of a non-class import in this message: "Cannot use` |
|       - | 2912 | `	 * function A\eff as eff …" / "Cannot use const A\KAY as KAY …". */` |
|     163 | 2913 | `	zKind = iUseType == 1 ? "function " : iUseType == 2 ? "const " : "";` |
|       - | 2914 | `	/* Check for duplicate import alias (per-type) */` |
|     163 | 2915 | `	if( SyHashGet(pGenHash,pAlias->zString,pAlias->nByte) != 0 ){` |
|      12 | 2916 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 2917 | `			"Cannot use %s%.*s as %z because the name is already in use",` |
|       6 | 2918 | `			zKind,(int)SyBlobLength(pPath),(const char *)SyBlobData(pPath),pAlias);` |
|       9 | 2919 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2920 | `			return SXERR_ABORT;` |
|       - | 2921 | `		}` |
|       3 | 2922 | `	}` |
|       - | 2923 | `	/* …and refuses one whose name a DECLARATION in this compile unit already took` |
|       - | 2924 | ``	 * (`class Cee {} use A\Cee;`), unless the import names that very declaration.`` |
|       - | 2925 | `	 * The name an import occupies is the alias in the CURRENT namespace, which is` |
|       - | 2926 | `	 * what the seen tables key on. php runs this check for classes and functions` |
|       - | 2927 | ``	 * only — a `const` declaration followed by its own `use const` is accepted. */`` |
|     163 | 2928 | `	if( iUseType != 2 ){` |
|       - | 2929 | `		SyBlob sTaken;` |
|     131 | 2930 | `		SyBlobInit(&sTaken,&pGen->pVm->sAllocator);` |
|     131 | 2931 | `		GenStateBuildFQN(&(*pGen),pAlias,&sTaken);` |
|     126 | 2932 | `		if( SyHashGet(iUseType == 1 ? &pGen->hSeenFunc : &pGen->hSeenClass,` |
|     189 | 2933 | `				SyBlobData(&sTaken),SyBlobLength(&sTaken)) != 0` |
|      70 | 2934 | `		 && (SyBlobLength(&sTaken) != SyBlobLength(pPath)` |
|       4 | 2935 | `			\|\| SyStrnicmp((const char *)SyBlobData(&sTaken),(const char *)SyBlobData(pPath),` |
|       6 | 2936 | `				(sxu32)SyBlobLength(&sTaken)) != 0) ){` |
|       8 | 2937 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 2938 | `				"Cannot use %s%.*s as %z because the name is already in use",` |
|       4 | 2939 | `				zKind,(int)SyBlobLength(pPath),(const char *)SyBlobData(pPath),pAlias);` |
|       6 | 2940 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2941 | `				SyBlobRelease(&sTaken);` |
|     ! 0 | 2942 | `				return SXERR_ABORT;` |
|       - | 2943 | `			}` |
|       2 | 2944 | `		}` |
|     131 | 2945 | `		SyBlobRelease(&sTaken);` |
|      63 | 2946 | `	}` |
|       - | 2947 | `	/* Register the import: alias -> FQN.` |
|       - | 2948 | `	 * Strings are allocated from the VM pool allocator and freed` |
|       - | 2949 | `	 * when the entire VM is released. SyHashRelease does not free` |
|       - | 2950 | `	 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|     242 | 2951 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     158 | 2952 | `		(const char *)SyBlobData(pPath),SyBlobLength(pPath));` |
|     163 | 2953 | `	if( zDup ){` |
|       - | 2954 | `		/* All three kinds resolve entirely at COMPILE time — a const import is read` |
|       - | 2955 | `		 * by the OP_LOADC candidate builder (compile_node.c), so no runtime table` |
|       - | 2956 | `		 * is needed for it either. */` |
|     163 | 2957 | `		SyHashInsert(pGenHash,pAlias->zString,pAlias->nByte,zDup);` |
|      79 | 2958 | `	}` |
|     163 | 2959 | `	return SXRET_OK;` |
|      84 | 2960 | `}` |
|       - | 2961 | `/*` |
|       - | 2962 | `` * Collect one `\`-separated name into pOut (appending to whatever it holds, with`` |
|       - | 2963 | ` * a separator when needed) and return its LAST segment token, or 0 when the` |
|       - | 2964 | ` * cursor is not on a name at all.` |
|       - | 2965 | ` */` |
|     172 | 2966 | `static SyToken * GenStateCollectNsPath(ph7_gen_state *pGen,SyBlob *pOut)` |
|       5 | 2967 | `{` |
|     177 | 2968 | `	SyToken *pLast = 0;` |
|     611 | 2969 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|     439 | 2970 | `		if( pGen->pIn->nType & PH7_TK_ID ){` |
|     301 | 2971 | `			pLast = pGen->pIn;` |
|     301 | 2972 | `			if( SyBlobLength(pOut) > 0 ){` |
|     155 | 2973 | `				SyBlobAppend(pOut,"\\",1);` |
|      75 | 2974 | `			}` |
|     301 | 2975 | `			SyBlobAppend(pOut,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|     148 | 2976 | `		}` |
|     439 | 2977 | `		pGen->pIn++;` |
|       5 | 2978 | `	}` |
|     177 | 2979 | `	return pLast;` |
|       5 | 2980 | `}` |
|       - | 2981 | `/*` |
|       - | 2982 | `` * Consume the optional `as Alias` clause, leaving *pAlias untouched when absent.`` |
|       - | 2983 | ` */` |
|     158 | 2984 | `static void GenStateCollectImportAlias(ph7_gen_state *pGen,SyString *pAlias)` |
|       5 | 2985 | `{` |
|     158 | 2986 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     103 | 2987 | `		&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|      41 | 2988 | `		pGen->pIn++; /* Jump 'as' */` |
|      41 | 2989 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|      41 | 2990 | `			*pAlias = pGen->pIn->sData;` |
|      41 | 2991 | `			pGen->pIn++;` |
|      19 | 2992 | `		}` |
|      19 | 2993 | `	}` |
|     163 | 2994 | `}` |
|       - | 2995 | `/*` |
|       - | 2996 | ` * Compile the members of a GROUP use declaration (php 7.0):` |
|       - | 2997 | ` *` |
|       - | 2998 | ` *      use A\{Cee, Dee as D2, Sub\Eee};` |
|       - | 2999 | ` *      use function A\{f, g as h};` |
|       - | 3000 | ` *      use A\{function f, const K, Cee};   // per-member kind, untyped group only` |
|       - | 3001 | ` *` |
|       - | 3002 | `` * pPrefix holds the path before the brace; the cursor sits on `{`.  Each member`` |
|       - | 3003 | `` * is the prefix, a `\`, and the member's own (possibly multi-segment) name.  A`` |
|       - | 3004 | ` * trailing comma is allowed, an empty group is not.` |
|       - | 3005 | ` */` |
|      10 | 3006 | `static sxi32 GenStateCompileGroupUse(ph7_gen_state *pGen,SyBlob *pPrefix,int iUseType,sxu32 nLine)` |
|       1 | 3007 | `{` |
|       - | 3008 | `	SyBlob sPath;` |
|      11 | 3009 | `	sxi32 rc = SXRET_OK;` |
|      11 | 3010 | `	pGen->pIn++; /* Jump '{' */` |
|      11 | 3011 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|      11 | 3012 | `	for(;;){` |
|      23 | 3013 | `		int iMemberType = iUseType;` |
|       - | 3014 | `		SyString sAlias;` |
|       - | 3015 | `		SyToken *pLast;` |
|       - | 3016 | ``		/* `function`/`const` may qualify a single member, but only inside a`` |
|       - | 3017 | ``		 * group that is not itself typed (php rejects `use function A\{const C}`). */`` |
|      23 | 3018 | `		if( iUseType == 0 && pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       5 | 3019 | `			sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|       5 | 3020 | `			if( nKey == PH7_TKWRD_FUNCTION ){` |
|       3 | 3021 | `				iMemberType = 1;` |
|       3 | 3022 | `				pGen->pIn++;` |
|       4 | 3023 | `			}else if( nKey == PH7_TKWRD_CONST ){` |
|       3 | 3024 | `				iMemberType = 2;` |
|       3 | 3025 | `				pGen->pIn++;` |
|       1 | 3026 | `			}` |
|       2 | 3027 | `		}` |
|      23 | 3028 | `		SyBlobReset(&sPath);` |
|      23 | 3029 | `		SyBlobAppend(&sPath,SyBlobData(pPrefix),SyBlobLength(pPrefix));` |
|      23 | 3030 | `		pLast = GenStateCollectNsPath(pGen,&sPath);` |
|      23 | 3031 | `		if( pLast == 0 ){` |
|       - | 3032 | ``			/* No member name: `use A\{};` or a stray token.  Report once, then`` |
|       - | 3033 | `			 * skip to the end of the group so the statement does not cascade. */` |
|     ! 0 | 3034 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - | 3035 | `				"syntax error, unexpected %s \"%z\", expecting identifier",` |
|     ! 0 | 3036 | `				TokenTypeName(pGen->pIn < pGen->pEnd ? pGen->pIn->nType : 0),` |
|     ! 0 | 3037 | `				pGen->pIn < pGen->pEnd ? &pGen->pIn->sData : 0);` |
|     ! 0 | 3038 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_CCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 | 3039 | `				pGen->pIn++;` |
|     ! 0 | 3040 | `			}` |
|     ! 0 | 3041 | `			break;` |
|       - | 3042 | `		}` |
|      23 | 3043 | `		sAlias = pLast->sData; /* Default alias is the member's last component */` |
|      23 | 3044 | `		GenStateCollectImportAlias(pGen,&sAlias);` |
|      23 | 3045 | `		rc = GenStateAddImport(&(*pGen),iMemberType,&sPath,&sAlias,nLine);` |
|      23 | 3046 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3047 | `			break;` |
|       - | 3048 | `		}` |
|      23 | 3049 | `		rc = SXRET_OK;` |
|      23 | 3050 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|      15 | 3051 | `			pGen->pIn++;` |
|      15 | 3052 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) ){` |
|       3 | 3053 | `				break; /* Trailing comma before the closing brace */` |
|       - | 3054 | `			}` |
|      13 | 3055 | `			continue;` |
|       - | 3056 | `		}` |
|       9 | 3057 | `		break;` |
|     ! 0 | 3058 | `	}` |
|      11 | 3059 | `	SyBlobRelease(&sPath);` |
|      11 | 3060 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 3061 | `		return SXERR_ABORT;` |
|       - | 3062 | `	}` |
|      11 | 3063 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) ){` |
|      11 | 3064 | `		pGen->pIn++; /* Jump '}' */` |
|       5 | 3065 | `	}` |
|      11 | 3066 | `	return SXRET_OK;` |
|       6 | 3067 | `}` |
|       - | 3068 | `/*` |
|       - | 3069 | ` * Compile the 'use' statement` |
|       - | 3070 | ` * According to the PHP language reference manual` |
|       - | 3071 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - | 3072 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - | 3073 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - | 3074 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - | 3075 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - | 3076 | ` *  a function or constant is not supported.` |
|       - | 3077 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - | 3078 | ` * NOTE` |
|       - | 3079 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - | 3080 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - | 3081 | ` */` |
|     148 | 3082 | `PH7_PRIVATE sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       5 | 3083 | `{` |
|       - | 3084 | `	sxu32 nLine;` |
|       - | 3085 | `	sxi32 rc;` |
|       - | 3086 | `	SyBlob sPath;` |
|       - | 3087 | `	SyString sAlias;` |
|       - | 3088 | `	SyToken *pLast;` |
|       - | 3089 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|     153 | 3090 | `	nLine = pGen->pIn->nLine;` |
|     153 | 3091 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|       - | 3092 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|     153 | 3093 | `	iUseType = 0;` |
|     153 | 3094 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      61 | 3095 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|      61 | 3096 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|      32 | 3097 | `			iUseType = 1;` |
|      32 | 3098 | `			pGen->pIn++;` |
|      47 | 3099 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|      33 | 3100 | `			iUseType = 2;` |
|      33 | 3101 | `			pGen->pIn++;` |
|      14 | 3102 | `		}` |
|      28 | 3103 | `	}` |
|     153 | 3104 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - | 3105 | `	/* Process one or more use declarations separated by commas */` |
|      75 | 3106 | `	for(;;){` |
|     155 | 3107 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 3108 | `			break;` |
|       - | 3109 | `		}` |
|     155 | 3110 | `		SyBlobReset(&sPath);` |
|       - | 3111 | `		/* Collect the full namespace path */` |
|     155 | 3112 | `		pLast = GenStateCollectNsPath(pGen,&sPath);` |
|     155 | 3113 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) && SyBlobLength(&sPath) > 0 ){` |
|       - | 3114 | `			/* GROUP declaration: what was collected is the shared prefix.  php` |
|       - | 3115 | `			 * does not let a group be comma-combined with another declaration,` |
|       - | 3116 | `			 * so the members close the statement. */` |
|      11 | 3117 | `			rc = GenStateCompileGroupUse(&(*pGen),&sPath,iUseType,nLine);` |
|      11 | 3118 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3119 | `				SyBlobRelease(&sPath);` |
|     ! 0 | 3120 | `				return SXERR_ABORT;` |
|       - | 3121 | `			}` |
|      11 | 3122 | `			break;` |
|       - | 3123 | `		}` |
|     145 | 3124 | `		if( pLast == 0 ){` |
|       - | 3125 | `			/* Empty path */` |
|       6 | 3126 | `			break;` |
|       - | 3127 | `		}` |
|       - | 3128 | `		/* Default alias is the last component of the path */` |
|     141 | 3129 | `		sAlias = pLast->sData;` |
|       - | 3130 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|     141 | 3131 | `		GenStateCollectImportAlias(pGen,&sAlias);` |
|     141 | 3132 | `		rc = GenStateAddImport(&(*pGen),iUseType,&sPath,&sAlias,nLine);` |
|     141 | 3133 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3134 | `			SyBlobRelease(&sPath);` |
|     ! 0 | 3135 | `			return SXERR_ABORT;` |
|       - | 3136 | `		}` |
|       - | 3137 | `		/* Check for comma (multiple use declarations) */` |
|     141 | 3138 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 | 3139 | `			pGen->pIn++;` |
|       2 | 3140 | `		}else{` |
|      72 | 3141 | `			break;` |
|       - | 3142 | `		}` |
|       1 | 3143 | `	}` |
|     153 | 3144 | `	SyBlobRelease(&sPath);` |
|     153 | 3145 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 | 3146 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 | 3147 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 | 3148 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3149 | `			return SXERR_ABORT;` |
|       - | 3150 | `		}` |
|       1 | 3151 | `	}` |
|     153 | 3152 | `	return SXRET_OK;` |
|      79 | 3153 | `}` |
|       - | 3154 | `/*` |
|       - | 3155 | ` * Compile the stupid 'declare' language construct.` |
|       - | 3156 | ` *` |
|       - | 3157 | ` * According to the PHP language reference manual.` |
|       - | 3158 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - | 3159 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - | 3160 | ` *  declare (directive)` |
|       - | 3161 | ` *   statement` |
|       - | 3162 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - | 3163 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - | 3164 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - | 3165 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - | 3166 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - | 3167 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - | 3168 | ` * <?php` |
|       - | 3169 | ` * // these are the same:` |
|       - | 3170 | ` * // you can use this:` |
|       - | 3171 | ` * declare(ticks=1) {` |
|       - | 3172 | ` *   // entire script here` |
|       - | 3173 | ` * }` |
|       - | 3174 | ` * // or you can use this:` |
|       - | 3175 | ` * declare(ticks=1);` |
|       - | 3176 | ` * // entire script here` |
|       - | 3177 | ` * ?>` |
|       - | 3178 | ` *` |
|       - | 3179 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - | 3180 | ` */` |
|       - | 3181 | `/*` |
|       - | 3182 | ` * Match a directive name against a known literal (case-insensitive).` |
|       - | 3183 | ` */` |
|     104 | 3184 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|       5 | 3185 | `{` |
|     156 | 3186 | `	return SyStringLength(pName) == nWant` |
|     104 | 3187 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|       5 | 3188 | `}` |
|       - | 3189 |  |
|      56 | 3190 | `PH7_PRIVATE sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       5 | 3191 | `{` |
|      61 | 3192 | `	sxu32 nLine = pGen->pIn->nLine;` |
|      61 | 3193 | `	SyToken *pBodyEnd = 0;` |
|       - | 3194 | `	SyToken *pBodyStart;` |
|       - | 3195 | `	SyToken *pCursor;` |
|       - | 3196 | `	int bHasStrictTypes;` |
|       - | 3197 | `	int bBlockForm;` |
|       - | 3198 | `	int bPlacementOk;` |
|       - | 3199 | `	sxi32 rc;` |
|      61 | 3200 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|      61 | 3201 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       5 | 3202 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|       5 | 3203 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3204 | `			return SXERR_ABORT;` |
|       - | 3205 | `		}` |
|       5 | 3206 | `		goto Synchro;` |
|       - | 3207 | `	}` |
|      57 | 3208 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|      57 | 3209 | `	pBodyStart = pGen->pIn;` |
|       - | 3210 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|      57 | 3211 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|      57 | 3212 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 | 3213 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\")\"");` |
|     ! 0 | 3214 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3215 | `			return SXERR_ABORT;` |
|       - | 3216 | `		}` |
|     ! 0 | 3217 | `		return SXRET_OK;` |
|       - | 3218 | `	}` |
|       - | 3219 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|       - | 3220 | `	 * now delimits the comma-separated directive list. */` |
|      57 | 3221 | `	pGen->pIn = &pBodyEnd[1];` |
|      57 | 3222 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|     ! 0 | 3223 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 | 3224 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3225 | `			return SXERR_ABORT;` |
|       - | 3226 | `		}` |
|     ! 0 | 3227 | `	}` |
|      57 | 3228 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|      57 | 3229 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|      57 | 3230 | `	bHasStrictTypes = 0;` |
|       - | 3231 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|       - | 3232 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|       - | 3233 | `	 * directive appears anywhere in the list, before validating values. */` |
|      57 | 3234 | `	pCursor = pBodyStart;` |
|      69 | 3235 | `	while( pCursor < pBodyEnd ){` |
|      65 | 3236 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      57 | 3237 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|      53 | 3238 | `				bHasStrictTypes = 1;` |
|      53 | 3239 | `				break;` |
|       - | 3240 | `			}` |
|       2 | 3241 | `		}` |
|      14 | 3242 | `		pCursor++;` |
|       2 | 3243 | `	}` |
|      57 | 3244 | `	if( bHasStrictTypes && bBlockForm ){` |
|       3 | 3245 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3246 | `			"strict_types declaration must not use block mode");` |
|       3 | 3247 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 | 3248 | `		return SXRET_OK;` |
|       - | 3249 | `	}` |
|      55 | 3250 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|       6 | 3251 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3252 | `			"strict_types declaration must be the very first statement in the script");` |
|       6 | 3253 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       6 | 3254 | `		return SXRET_OK;` |
|       - | 3255 | `	}` |
|       - | 3256 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|      51 | 3257 | `	pCursor = pBodyStart;` |
|      97 | 3258 | `	while( pCursor < pBodyEnd ){` |
|       - | 3259 | `		SyToken *pNameTok;` |
|       - | 3260 | `		SyToken *pEqTok;` |
|       - | 3261 | `		SyToken *pValTok;` |
|       - | 3262 | `		SyString *pDirName;` |
|       - | 3263 | `		int bIsStrict;` |
|       - | 3264 | `		int iStrictValue;` |
|      53 | 3265 | `		pNameTok = pCursor;` |
|      53 | 3266 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 3267 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3268 | `				"declare: Expecting a directive name");` |
|     ! 0 | 3269 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 | 3270 | `			return SXRET_OK;` |
|       - | 3271 | `		}` |
|      53 | 3272 | `		pEqTok = pNameTok + 1;` |
|      53 | 3273 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|     ! 0 | 3274 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3275 | `				"declare: Expecting '=' after directive name");` |
|     ! 0 | 3276 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 | 3277 | `			return SXRET_OK;` |
|       - | 3278 | `		}` |
|      53 | 3279 | `		pValTok = pEqTok + 1;` |
|      53 | 3280 | `		if( pValTok >= pBodyEnd ){` |
|     ! 0 | 3281 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3282 | `				"declare: Expecting value after '='");` |
|     ! 0 | 3283 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 | 3284 | `			return SXRET_OK;` |
|       - | 3285 | `		}` |
|      53 | 3286 | `		pDirName = &pNameTok->sData;` |
|      53 | 3287 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|      53 | 3288 | `		if( bIsStrict ){` |
|       - | 3289 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|       - | 3290 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|      49 | 3291 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|     ! 0 | 3292 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3293 | `					"declare(strict_types) value must be a literal");` |
|     ! 0 | 3294 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 | 3295 | `				return SXRET_OK;` |
|       - | 3296 | `			}` |
|      49 | 3297 | `			iStrictValue = -1;` |
|      49 | 3298 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|      49 | 3299 | `				const char *zv = SyStringData(&pValTok->sData);` |
|      49 | 3300 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|      49 | 3301 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|      47 | 3302 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|      22 | 3303 | `			}` |
|      49 | 3304 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|       3 | 3305 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3306 | `					"strict_types declaration must have 0 or 1 as its value");` |
|       3 | 3307 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 | 3308 | `				return SXRET_OK;` |
|       - | 3309 | `			}` |
|      46 | 3310 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|      27 | 3311 | `		}else if( DeclareNameIs(pDirName, "encoding", sizeof("encoding")-1) ){` |
|       - | 3312 | `			/* php always ignores declare(encoding=...) unless it was built with` |
|       - | 3313 | `			 * Zend multibyte, and says so in these exact words. */` |
|       3 | 3314 | `			PH7_GenCompileError(&(*pGen),E_WARNING,nLine,` |
|       - | 3315 | `				"declare(encoding=...) ignored because Zend multibyte feature is turned off by settings");` |
|       1 | 3316 | `		}else{` |
|       - | 3317 | `			/* Other directives (ticks and friends) are accepted as no-ops.` |
|       - | 3318 | `			 * This used to emit a NOTICE naming the upstream engine and its` |
|       - | 3319 | `			 * version ("the declare construct is a no-op in the current release` |
|       - | 3320 | `			 * of the PH7(2.1.4) engine") — php prints nothing at all for` |
|       - | 3321 | ``			 * `declare(ticks=1)`, and leaking the old engine's branding into`` |
|       - | 3322 | `			 * user-visible diagnostics was wrong on its own. */` |
|       - | 3323 | `		}` |
|      51 | 3324 | `		pCursor = pValTok + 1;` |
|       - | 3325 | `		/* Consume separating comma (or end). */` |
|      51 | 3326 | `		if( pCursor < pBodyEnd ){` |
|       3 | 3327 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 | 3328 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3329 | `					"declare: Expecting ',' or ')' after directive value");` |
|     ! 0 | 3330 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 | 3331 | `				return SXRET_OK;` |
|       - | 3332 | `			}` |
|       3 | 3333 | `			pCursor++;` |
|       1 | 3334 | `		}` |
|       5 | 3335 | `	}` |
|       - | 3336 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|       - | 3337 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|       - | 3338 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|      49 | 3339 | `	return SXRET_OK;` |
|       2 | 3340 | `Synchro:` |
|       - | 3341 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      15 | 3342 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      11 | 3343 | `		pGen->pIn++;` |
|       1 | 3344 | `	}` |
|       5 | 3345 | `	return SXRET_OK;` |
|      33 | 3346 | `}` |
|       - | 3347 | `/*` |
|       - | 3348 | ` * Compile a class constant.` |
|       - | 3349 | ` * According to the PHP language reference manual` |
|       - | 3350 | ` *  Class Constants` |
|       - | 3351 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - | 3352 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - | 3353 | ` *   you don't use the $ symbol to declare or use them.` |
|       - | 3354 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - | 3355 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - | 3356 | ` *   It's also possible for interfaces to have constants.` |
|       - | 3357 | ` * Symisc eXtension.` |
|       - | 3358 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - | 3359 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 3360 | ` *  Example:` |
|       - | 3361 | ` *   class Test{` |
|       - | 3362 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 3363 | ` *   };` |
|       - | 3364 | ` *   var_dump(TEST::MyConst);` |
|       - | 3365 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 3366 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 3367 | ` */` |
|       - | 3368 | `/*` |
|       - | 3369 | ` * Exception handling.` |
|       - | 3370 | ` *  According to the PHP language reference manual` |
|       - | 3371 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - | 3372 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - | 3373 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - | 3374 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - | 3375 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - | 3376 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - | 3377 | ` *    (or re-thrown) within a catch block.` |
|       - | 3378 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - | 3379 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - | 3380 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - | 3381 | ` *    been defined with set_exception_handler().` |
|       - | 3382 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - | 3383 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - | 3384 | ` */` |
|       - | 3385 | `/*` |
|       - | 3386 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - | 3387 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - | 3388 | ` * indicates failure.` |
|       - | 3389 | ` */` |
|   67554 | 3390 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 | 3391 | `{` |
|       - | 3392 | ``	/* php decides throwability at RUNTIME: `throw 5` compiles and raises the`` |
|       - | 3393 | `	 * catchable Error "Can only throw objects" when it executes, and a` |
|       - | 3394 | `	 * non-Throwable object raises "Cannot throw objects that do not implement` |
|       - | 3395 | `	 * Throwable". This used to whitelist a handful of expression shapes and` |
|       - | 3396 | `	 * reject the rest at compile time as a "friendlier" error, which meant the` |
|       - | 3397 | `	 * program never ran and no catch could ever see it — and it was inconsistent` |
|       - | 3398 | ``	 * anyway, since `throw $v` (a variable holding an int) always compiled and`` |
|       - | 3399 | `	 * fell through to the same runtime path. OP_THROW now owns the whole rule,` |
|       - | 3400 | ``	 * so accept any expression here; `throw;` with no operand is still rejected`` |
|       - | 3401 | `	 * by PH7_CompileThrow's SXERR_EMPTY branch. */` |
|   33777 | 3402 | `	SXUNUSED(pGen);` |
|   33777 | 3403 | `	SXUNUSED(pRoot);` |
|   67559 | 3404 | `	return SXRET_OK;` |
|       5 | 3405 | `}` |
|       - | 3406 | `/*` |
|       - | 3407 | ` * Compile a 'throw' statement.` |
|       - | 3408 | ` * throw: This is how you trigger an exception.` |
|       - | 3409 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - | 3410 | ` */` |
|   67510 | 3411 | `PH7_PRIVATE sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       5 | 3412 | `{` |
|   67515 | 3413 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 3414 | `	GenBlock *pBlock;` |
|       - | 3415 | `	sxu32 nIdx;` |
|       - | 3416 | `	sxi32 rc;` |
|   67515 | 3417 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - | 3418 | `	/* Compile the expression */` |
|   67515 | 3419 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|   67515 | 3420 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 3421 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 | 3422 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3423 | `			return SXERR_ABORT;` |
|       - | 3424 | `		}` |
|     ! 0 | 3425 | `		return SXRET_OK;` |
|       - | 3426 | `	}` |
|   67515 | 3427 | `	pBlock = pGen->pCurrent;` |
|       - | 3428 | `	/* Point to the top most function or try block and emit the forward jump */` |
|  310019 | 3429 | `	while(pBlock->pParent){` |
|  309997 | 3430 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|   67493 | 3431 | `			break;` |
|       - | 3432 | `		}` |
|       - | 3433 | `		/* Point to the parent block */` |
|  242509 | 3434 | `		pBlock = pBlock->pParent;` |
|       5 | 3435 | `	}` |
|       - | 3436 | `	/* Emit the throw instruction */` |
|   67515 | 3437 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - | 3438 | `	/* Emit the jump */` |
|   67515 | 3439 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|   67515 | 3440 | `	return SXRET_OK;` |
|   33760 | 3441 | `}` |
|       - | 3442 | `/*` |
|       - | 3443 | ` * Compile a PHP 8.0 'throw' expression.` |
|       - | 3444 | ` * Called from the expression code generator when a 'throw' keyword is` |
|       - | 3445 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|       - | 3446 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|       - | 3447 | ` * the validator guarantees the operand is a valid exception target.` |
|       - | 3448 | ` */` |
|      44 | 3449 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       4 | 3450 | `{` |
|      48 | 3451 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 3452 | `	GenBlock *pBlock;` |
|       - | 3453 | `	sxu32 nIdx;` |
|       - | 3454 | `	sxi32 rc;` |
|      22 | 3455 | `	(void)iCompileFlag;` |
|      48 | 3456 | `	pGen->pIn++; /* Skip 'throw' */` |
|      48 | 3457 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 3458 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 3459 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 3460 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3461 | `			return SXERR_ABORT;` |
|       - | 3462 | `		}` |
|     ! 0 | 3463 | `		return SXRET_OK;` |
|       - | 3464 | `	}` |
|      48 | 3465 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|      48 | 3466 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 3467 | `		return SXERR_ABORT;` |
|       - | 3468 | `	}` |
|      48 | 3469 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 3470 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 3471 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 3472 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3473 | `			return SXERR_ABORT;` |
|       - | 3474 | `		}` |
|     ! 0 | 3475 | `		return SXRET_OK;` |
|       - | 3476 | `	}` |
|       - | 3477 | `	/* Walk up to nearest exception/function block for the jump target */` |
|      48 | 3478 | `	pBlock = pGen->pCurrent;` |
|      72 | 3479 | `	while( pBlock->pParent ){` |
|      62 | 3480 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|      38 | 3481 | `			break;` |
|       - | 3482 | `		}` |
|      26 | 3483 | `		pBlock = pBlock->pParent;` |
|       2 | 3484 | `	}` |
|      48 | 3485 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|      48 | 3486 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|      48 | 3487 | `	return SXRET_OK;` |
|      26 | 3488 | `}` |
|       - | 3489 | `/*` |
|       - | 3490 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|       - | 3491 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|       - | 3492 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|       - | 3493 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|       - | 3494 | ` * compile error propagated from the parser.` |
|       - | 3495 | ` */` |
|      66 | 3496 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|       5 | 3497 | `{` |
|       - | 3498 | `	SyString sClassName;` |
|       - | 3499 | `	SyToken *pToken;` |
|       - | 3500 | `	SyString *pName;` |
|       - | 3501 | `	char *zDup;` |
|       - | 3502 | `	sxi32 rc;` |
|      71 | 3503 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|      71 | 3504 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|      71 | 3505 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|       - | 3506 | `	/* Inline catches compile into the function's own container; pByteCode stays NULL. */` |
|      71 | 3507 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 | 3508 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 3509 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 3510 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 3511 | `		return SXERR_INVALID;` |
|       - | 3512 | `	}` |
|      71 | 3513 | `	pGen->pIn++; /* '(' */` |
|      33 | 3514 | `	for(;;){` |
|       - | 3515 | `		SyBlob sResolved;` |
|      71 | 3516 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      71 | 3517 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 | 3518 | `			SyBlobRelease(&sResolved);` |
|     ! 0 | 3519 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 3520 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 3521 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 3522 | `			return SXERR_INVALID;` |
|       - | 3523 | `		}` |
|     104 | 3524 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      66 | 3525 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|      71 | 3526 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|      71 | 3527 | `		SyBlobRelease(&sResolved);` |
|      71 | 3528 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|      71 | 3529 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|      71 | 3530 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      66 | 3531 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|       5 | 3532 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|     ! 0 | 3533 | `			pGen->pIn++; continue;` |
|       - | 3534 | `		}` |
|      71 | 3535 | `		break;` |
|     ! 0 | 3536 | `	}` |
|       - | 3537 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|       - | 3538 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding. */` |
|      71 | 3539 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|       3 | 3540 | `		pGen->pIn++; /* ')' */` |
|       3 | 3541 | `		return SXRET_OK;` |
|       - | 3542 | `	}` |
|      64 | 3543 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|      69 | 3544 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 3545 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 3546 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 3547 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 3548 | `		return SXERR_INVALID;` |
|       - | 3549 | `	}` |
|      69 | 3550 | `	pGen->pIn++; /* '$' */` |
|      69 | 3551 | `	pName = &pGen->pIn->sData;` |
|      69 | 3552 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      69 | 3553 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|      69 | 3554 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|      69 | 3555 | `	pGen->pIn++;` |
|      69 | 3556 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 | 3557 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 3558 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 3559 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 3560 | `		return SXERR_INVALID;` |
|       - | 3561 | `	}` |
|      69 | 3562 | `	pGen->pIn++; /* ')' */` |
|      69 | 3563 | `	return SXRET_OK;` |
|      38 | 3564 | `}` |
|       - | 3565 | `/*` |
|       - | 3566 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|       - | 3567 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|       - | 3568 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|       - | 3569 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|       - | 3570 | ` * VmThrowException):` |
|       - | 3571 | ` *` |
|       - | 3572 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|       - | 3573 | ` *    <try body>` |
|       - | 3574 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|       - | 3575 | ` *    JMP  -> finally\|end` |
|       - | 3576 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|       - | 3577 | ` *    <catch body>` |
|       - | 3578 | ` *    JMP  -> finally\|end` |
|       - | 3579 | ` *    ... more catches ...` |
|       - | 3580 | ` *  Lfin: <finally body>` |
|       - | 3581 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|       - | 3582 | ` *  Lend:` |
|       - | 3583 | ` */` |
|     122 | 3584 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|       5 | 3585 | `{` |
|     127 | 3586 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 3587 | `	GenBlock *pTry;` |
|       - | 3588 | `	VmInstr *pInstr;` |
|     127 | 3589 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|       - | 3590 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|       - | 3591 | `	sxi32 rc;` |
|     127 | 3592 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|       - | 3593 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION; passed at` |
|       - | 3594 | `	 * ENTRY so GenStateEnterBlock can classify the scope with it) */` |
|     188 | 3595 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),` |
|      61 | 3596 | `		pException,&pTry);` |
|     127 | 3597 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|     127 | 3598 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|     127 | 3599 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|     127 | 3600 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     127 | 3601 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|     127 | 3602 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|     127 | 3603 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|       - | 3604 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|     127 | 3605 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|     127 | 3606 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|     127 | 3607 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|     127 | 3608 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 3609 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|     127 | 3610 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|       - | 3611 | `	/* Catch clauses (inline) */` |
|     127 | 3612 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     122 | 3613 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|      71 | 3614 | `		sxu32 k = 0;` |
|      99 | 3615 | `		for(;;){` |
|       - | 3616 | `			ph7_exception_block sCatch;` |
|       - | 3617 | `			GenBlock *pCatchBlk;` |
|     137 | 3618 | `			sxu32 idxJmp = 0;` |
|     132 | 3619 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     127 | 3620 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|      38 | 3621 | `				break;` |
|       - | 3622 | `			}` |
|      71 | 3623 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|      71 | 3624 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      71 | 3625 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|      71 | 3626 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|      71 | 3627 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|       - | 3628 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|       - | 3629 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|       - | 3630 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump).` |
|       - | 3631 | `			 * Passed at ENTRY: GenStateEnterBlock reads it to classify the block's scope. */` |
|     104 | 3632 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),` |
|      33 | 3633 | `				pException,&pCatchBlk);` |
|      71 | 3634 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      71 | 3635 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      71 | 3636 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      71 | 3637 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|      71 | 3638 | `			GenStateLeaveBlock(&(*pGen),0);` |
|       - | 3639 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|       - | 3640 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|      71 | 3641 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|      71 | 3642 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|      71 | 3643 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|      71 | 3644 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|      71 | 3645 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      71 | 3646 | `			k++;` |
|       5 | 3647 | `		}` |
|      33 | 3648 | `	}` |
|       - | 3649 | `	/* Finally (inline) */` |
|     127 | 3650 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     102 | 3651 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 3652 | `		GenBlock *pFinBlk;` |
|      65 | 3653 | `		pGen->pIn++; /* Jump 'finally' */` |
|      65 | 3654 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|      95 | 3655 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FINALLY,` |
|      30 | 3656 | `			PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|      65 | 3657 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      65 | 3658 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      65 | 3659 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      65 | 3660 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|      65 | 3661 | `		GenStateLeaveBlock(&(*pGen),0);` |
|      65 | 3662 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|      65 | 3663 | `		pException->iHasFinally = 1;` |
|      30 | 3664 | `	}` |
|     127 | 3665 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|     127 | 3666 | `	pException->iInlined = 1;` |
|       - | 3667 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|       - | 3668 | `	{` |
|     127 | 3669 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|       - | 3670 | `		sxu32 *aJ; sxu32 n;` |
|     127 | 3671 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|     127 | 3672 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|     127 | 3673 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|     193 | 3674 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|      71 | 3675 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|      71 | 3676 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|      38 | 3677 | `		}` |
|       - | 3678 | `	}` |
|     127 | 3679 | `	SySetRelease(&aCatchJmp);` |
|     127 | 3680 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|     ! 0 | 3681 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|     ! 0 | 3682 | `	}` |
|     127 | 3683 | `	return SXRET_OK;` |
|      66 | 3684 | `}` |
|       - | 3685 | `/*` |
|       - | 3686 | ` * Compile a 'catch' block.` |
|       - | 3687 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - | 3688 | ` * an object containing the exception information.` |
|       - | 3689 | ` */` |
|    3274 | 3690 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       5 | 3691 | `{` |
|    3279 | 3692 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 3693 | `	ph7_exception_block sCatch;` |
|       - | 3694 | `	SySet *pInstrContainer;` |
|       - | 3695 | `	SyString sClassName;` |
|       - | 3696 | `	GenBlock *pCatch;` |
|       - | 3697 | `	SyToken *pToken;` |
|       - | 3698 | `	SyString *pName;` |
|       - | 3699 | `	char *zDup;` |
|       - | 3700 | `	sxi32 rc;` |
|    3279 | 3701 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - | 3702 | `	/* Zero the structure */` |
|    3279 | 3703 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - | 3704 | `	/* Initialize fields */` |
|    3279 | 3705 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|       - | 3706 | `	/* The catch body gets its own bytecode array, allocated (not embedded) so its address` |
|       - | 3707 | `	 * survives both this stack frame and any later growth of pException->sEntry — a` |
|       - | 3708 | `	 * break/continue inside the body records it in its JumpFixup (see JumpFixup). */` |
|    3279 | 3709 | `	sCatch.pByteCode = (SySet *)SyMemBackendAlloc(&pException->pVm->sAllocator,sizeof(SySet));` |
|    3279 | 3710 | `	if( sCatch.pByteCode == 0 ){` |
|     ! 0 | 3711 | `		goto Mem;` |
|       - | 3712 | `	}` |
|    3279 | 3713 | `	SySetInit(sCatch.pByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|    3279 | 3714 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|       - | 3715 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 3716 | `			pToken = pGen->pIn;` |
|     ! 0 | 3717 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 3718 | `				pToken--;` |
|     ! 0 | 3719 | `			}` |
|     ! 0 | 3720 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 3721 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 3722 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 3723 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3724 | `				return SXERR_ABORT;` |
|       - | 3725 | `			}` |
|     ! 0 | 3726 | `			return SXERR_INVALID;` |
|       - | 3727 | `	}` |
|       - | 3728 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|    3279 | 3729 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|    1654 | 3730 | `	for(;;){` |
|       - | 3731 | `		SyBlob sResolved;` |
|    3313 | 3732 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    3313 | 3733 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       6 | 3734 | `			SyBlobRelease(&sResolved);` |
|       6 | 3735 | `			pToken = pGen->pIn;` |
|       6 | 3736 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 3737 | `				pToken--;` |
|     ! 0 | 3738 | `			}` |
|       8 | 3739 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 3740 | `				"syntax error, unexpected %s \"%z\"",` |
|       2 | 3741 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       6 | 3742 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3743 | `				return SXERR_ABORT;` |
|       - | 3744 | `			}` |
|       6 | 3745 | `			return SXERR_INVALID;` |
|       - | 3746 | `		}` |
|       - | 3747 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|       - | 3748 | `		 * transient SyBlob allocation. */` |
|    4961 | 3749 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    3304 | 3750 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|    3309 | 3751 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|    3309 | 3752 | `		SyBlobRelease(&sResolved);` |
|    3309 | 3753 | `		if( zDup == 0 ){` |
|     ! 0 | 3754 | `			goto Mem;` |
|       - | 3755 | `		}` |
|    3309 | 3756 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|    3309 | 3757 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3758 | `			goto Mem;` |
|       - | 3759 | `		}` |
|       - | 3760 | `		/* Check for '\|' (multi-catch separator) */` |
|    3304 | 3761 | `		if( pGen->pIn < pGen->pEnd &&` |
|    3304 | 3762 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|      39 | 3763 | `			pGen->pIn->sData.nByte == 1 &&` |
|      34 | 3764 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|      37 | 3765 | `			pGen->pIn++; /* Consume the '\|' */` |
|      37 | 3766 | `			continue;` |
|       - | 3767 | `		}` |
|    3275 | 3768 | `		break;` |
|     ! 0 | 3769 | `	}` |
|       - | 3770 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|       - | 3771 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding;` |
|       - | 3772 | `	 * jump straight to compiling the block below. */` |
|    3275 | 3773 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) /*)*/ ){` |
|       7 | 3774 | `		goto CatchBody;` |
|       - | 3775 | `	}` |
|    3264 | 3776 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|    3269 | 3777 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 3778 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 3779 | `			pToken = pGen->pIn;` |
|     ! 0 | 3780 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 3781 | `				pToken--;` |
|     ! 0 | 3782 | `			}` |
|     ! 0 | 3783 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 3784 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 3785 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 3786 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3787 | `				return SXERR_ABORT;` |
|       - | 3788 | `			}` |
|     ! 0 | 3789 | `			return SXERR_INVALID;` |
|       - | 3790 | `	}` |
|    3269 | 3791 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - | 3792 | `	/* Duplicate instance name */` |
|    3269 | 3793 | `	pName = &pGen->pIn->sData;` |
|    3269 | 3794 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    3269 | 3795 | `	if( zDup == 0 ){` |
|     ! 0 | 3796 | `		goto Mem;` |
|       - | 3797 | `	}` |
|    3269 | 3798 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|    3269 | 3799 | `	pGen->pIn++;` |
|    1635 | 3800 | `CatchBody:` |
|    3275 | 3801 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - | 3802 | `		/* Unexpected token,break immediately */` |
|     ! 0 | 3803 | `		pToken = pGen->pIn;` |
|     ! 0 | 3804 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 | 3805 | `			pToken--;` |
|     ! 0 | 3806 | `		}` |
|     ! 0 | 3807 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 3808 | `			"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 3809 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 3810 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3811 | `			return SXERR_ABORT;` |
|       - | 3812 | `		}` |
|     ! 0 | 3813 | `		return SXERR_INVALID;` |
|       - | 3814 | `	}` |
|       - | 3815 | `	/* Compile the block */` |
|    3275 | 3816 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - | 3817 | `	/* Create the catch block. GEN_BLOCK_DETACHED: the body below compiles into` |
|       - | 3818 | `	 * sCatch.pByteCode, not into the enclosing function's array. */` |
|    3275 | 3819 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION\|GEN_BLOCK_DETACHED,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|    3275 | 3820 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 3821 | `		return SXERR_ABORT;` |
|       - | 3822 | `	}` |
|       - | 3823 | `	/* Swap bytecode container */` |
|    3275 | 3824 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    3275 | 3825 | `	PH7_VmSetByteCodeContainer(pGen->pVm,sCatch.pByteCode);` |
|       - | 3826 | `	/* Compile the block */` |
|    3275 | 3827 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 3828 | `	/* Fix forward jumps now the destination is resolved  */` |
|    3275 | 3829 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 3830 | `	/* Emit the DONE instruction */` |
|    3275 | 3831 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 3832 | `	/* Leave the block */` |
|    3275 | 3833 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 3834 | `	/* Restore the default container */` |
|    3275 | 3835 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 3836 | `	/* Install the catch block */` |
|    3275 | 3837 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|    3275 | 3838 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 3839 | `		goto Mem;` |
|       - | 3840 | `	}` |
|    3275 | 3841 | `	return SXRET_OK;` |
|     ! 0 | 3842 | `Mem:` |
|     ! 0 | 3843 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 3844 | `	return SXERR_ABORT;` |
|    1642 | 3845 | `}` |
|       - | 3846 | `/*` |
|       - | 3847 | ` * Compile a 'try' block.` |
|       - | 3848 | ` * A function using an exception should be in a "try" block.` |
|       - | 3849 | ` * If the exception does not trigger, the code will continue` |
|       - | 3850 | ` * as normal. However if the exception triggers, an exception` |
|       - | 3851 | ` * is "thrown".` |
|       - | 3852 | ` */` |
|    3540 | 3853 | `PH7_PRIVATE sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       5 | 3854 | `{` |
|       - | 3855 | `	ph7_exception *pException;` |
|    3545 | 3856 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 3857 | `	GenBlock *pTry;` |
|       - | 3858 | `	sxu32 nJmpIdx;` |
|       - | 3859 | `	sxi32 rc;` |
|       - | 3860 | `	/* Create the exception container */` |
|    3545 | 3861 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|    3545 | 3862 | `	if( pException == 0 ){` |
|     ! 0 | 3863 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 | 3864 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 3865 | `		return SXERR_ABORT;` |
|       - | 3866 | `	}` |
|       - | 3867 | `	/* Zero the structure */` |
|    3545 | 3868 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - | 3869 | `	/* Initialize fields */` |
|    3545 | 3870 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|    3545 | 3871 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|    3545 | 3872 | `	pException->iHasFinally = 0;` |
|    3545 | 3873 | `	pException->iFinallyDone = 0;` |
|    3545 | 3874 | `	pException->pVm = pGen->pVm;` |
|       - | 3875 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|       - | 3876 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the DETACHED path`` |
|       - | 3877 | `	 * below — deliberately, not pending migration: it is the proven one, and inlining was` |
|       - | 3878 | ``	 * scoped to generators so no other code path changed. `bInlineTryCatch` is 1 since the`` |
|       - | 3879 | ``	 * inline VM handlers landed, so `bInGenerator` is what actually selects here. */`` |
|    3545 | 3880 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|     127 | 3881 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|       - | 3882 | `	}` |
|       - | 3883 | `	/* Create the try block */` |
|       - | 3884 | `	/* pUserData is the exception context, passed at ENTRY (not assigned after) because` |
|       - | 3885 | `	 * GenStateEnterBlock reads it to classify the block's try/catch scope — see aScope.` |
|       - | 3886 | `	 * It is also what a break/continue crossing this try emits its POP_EXCEPTION with. */` |
|    5132 | 3887 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),` |
|    1709 | 3888 | `		pException,&pTry);` |
|    3423 | 3889 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 3890 | `		return SXERR_ABORT;` |
|       - | 3891 | `	}` |
|       - | 3892 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|    3423 | 3893 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - | 3894 | `	/* Fix the jump later when the destination is resolved */` |
|    3423 | 3895 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|    3423 | 3896 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - | 3897 | `	/* Compile the block */` |
|    3423 | 3898 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|    3423 | 3899 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 3900 | `		return SXERR_ABORT;` |
|       - | 3901 | `	}` |
|       - | 3902 | `	/* Fix forward jumps now the destination is resolved */` |
|    3423 | 3903 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 3904 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|    3423 | 3905 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - | 3906 | `	/* Leave the block */` |
|    3423 | 3907 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 3908 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|    3423 | 3909 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|    3416 | 3910 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - | 3911 | `		/* Compile one or more catch blocks */` |
|    3269 | 3912 | `		for(;;){` |
|    6538 | 3913 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    5421 | 3914 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|    1637 | 3915 | `					break;` |
|       - | 3916 | `			}` |
|    3279 | 3917 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|    3279 | 3918 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3919 | `				return SXERR_ABORT;` |
|       - | 3920 | `			}` |
|       5 | 3921 | `		}` |
|    1632 | 3922 | `	}` |
|       - | 3923 | `	/* Compile optional finally block */` |
|    3423 | 3924 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|    1842 | 3925 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 3926 | `		SySet *pInstrContainer;` |
|       - | 3927 | `		GenBlock *pFinBlock;` |
|     239 | 3928 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - | 3929 | `		/* Create the finally block for jump fixup bookkeeping (detached: the body` |
|       - | 3930 | `		 * compiles into pException->sFinally, see GEN_BLOCK_DETACHED). */` |
|     356 | 3931 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION\|GEN_BLOCK_DETACHED\|GEN_BLOCK_FINALLY,` |
|     117 | 3932 | `			PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|     239 | 3933 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3934 | `			return SXERR_ABORT;` |
|       - | 3935 | `		}` |
|       - | 3936 | `		/* Swap bytecode container */` |
|     239 | 3937 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     239 | 3938 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - | 3939 | `		/* Compile the finally body */` |
|     239 | 3940 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     239 | 3941 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3942 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 3943 | `			return SXERR_ABORT;` |
|       - | 3944 | `		}` |
|       - | 3945 | `		/* Fix forward jumps now the destination is resolved */` |
|     239 | 3946 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 3947 | `		/* Emit DONE to terminate the finally block */` |
|     239 | 3948 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 3949 | `		/* Leave the block */` |
|     239 | 3950 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - | 3951 | `		/* Restore the default container */` |
|     239 | 3952 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     239 | 3953 | `		pException->iHasFinally = 1;` |
|     117 | 3954 | `	}` |
|       - | 3955 | `	/* Must have at least one catch or finally */` |
|    3423 | 3956 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       9 | 3957 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 3958 | `			"Cannot use try without catch or finally");` |
|       9 | 3959 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3960 | `			return SXERR_ABORT;` |
|       - | 3961 | `		}` |
|       3 | 3962 | `	}` |
|    3423 | 3963 | `	return SXRET_OK;` |
|    1775 | 3964 | `}` |
|       - | 3965 | `/*` |
|       - | 3966 | ` * Compile a switch block.` |
|       - | 3967 | ` *  (See block-comment below for more information)` |
|       - | 3968 | ` */` |
|     158 | 3969 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       5 | 3970 | `{` |
|     163 | 3971 | `	sxi32 rc = SXRET_OK;` |
|     163 | 3972 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - | 3973 | `		/* Unexpected token */` |
|     ! 0 | 3974 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|     ! 0 | 3975 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3976 | `			return SXERR_ABORT;` |
|       - | 3977 | `		}` |
|     ! 0 | 3978 | `		pGen->pIn++;` |
|     ! 0 | 3979 | `	}` |
|     163 | 3980 | `	pGen->pIn++;` |
|       - | 3981 | `	/* First instruction to execute in this block. */` |
|     163 | 3982 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - | 3983 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - | 3984 | `	 * or the '}' token */` |
|     268 | 3985 | `	for(;;){` |
|     541 | 3986 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 3987 | `			/* No more input to process */` |
|     ! 0 | 3988 | `			break;` |
|       - | 3989 | `		}` |
|     541 | 3990 | `		rc = SXRET_OK;` |
|     541 | 3991 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     123 | 3992 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      49 | 3993 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - | 3994 | `					/* Unexpected token */` |
|     ! 0 | 3995 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 3996 | `						&pGen->pIn->sData);` |
|     ! 0 | 3997 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 3998 | `						return SXERR_ABORT;` |
|       - | 3999 | `					}` |
|       - | 4000 | `					/* FALL THROUGH */` |
|     ! 0 | 4001 | `				}` |
|      49 | 4002 | `				rc = SXERR_EOF;` |
|      49 | 4003 | `				break;` |
|       - | 4004 | `			}` |
|      42 | 4005 | `		}else{` |
|       - | 4006 | `			sxi32 nKwrd;` |
|       - | 4007 | `			/* Extract the keyword */` |
|     423 | 4008 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     423 | 4009 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      60 | 4010 | `				break;` |
|       - | 4011 | `			}` |
|     313 | 4012 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       5 | 4013 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - | 4014 | `					/* Unexpected token */` |
|     ! 0 | 4015 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 4016 | `						&pGen->pIn->sData);` |
|     ! 0 | 4017 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 4018 | `						return SXERR_ABORT;` |
|       - | 4019 | `					}` |
|       - | 4020 | `					/* FALL THROUGH */` |
|     ! 0 | 4021 | `				}` |
|       - | 4022 | `				/* Block compiled */` |
|       5 | 4023 | `				break;` |
|       - | 4024 | `			}` |
|       - | 4025 | `		}` |
|       - | 4026 | `		/* Compile block */` |
|     383 | 4027 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     383 | 4028 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4029 | `			return SXERR_ABORT;` |
|       - | 4030 | `		}` |
|       5 | 4031 | `	}` |
|     163 | 4032 | `	return rc;` |
|      84 | 4033 | `}` |
|       - | 4034 | `/*` |
|       - | 4035 | ` * Compile a case eXpression.` |
|       - | 4036 | ` *  (See block-comment below for more information)` |
|       - | 4037 | ` */` |
|     124 | 4038 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       5 | 4039 | `{` |
|       - | 4040 | `	SySet *pInstrContainer;` |
|       - | 4041 | `	SyToken *pEnd,*pTmp;` |
|     129 | 4042 | `	sxi32 iNest = 0;` |
|       - | 4043 | `	sxi32 rc;` |
|       - | 4044 | `	/* Delimit the expression */` |
|     129 | 4045 | `	pEnd = pGen->pIn;` |
|     269 | 4046 | `	while( pEnd < pGen->pEnd ){` |
|     269 | 4047 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - | 4048 | `			/* Increment nesting level */` |
|       8 | 4049 | `			iNest++;` |
|     266 | 4050 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - | 4051 | `			/* Decrement nesting level */` |
|       8 | 4052 | `			iNest--;` |
|     260 | 4053 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|     129 | 4054 | `			break;` |
|       - | 4055 | `		}` |
|     145 | 4056 | `		pEnd++;` |
|       5 | 4057 | `	}` |
|     129 | 4058 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 4059 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 | 4060 | `		if( rc == SXERR_ABORT ){` |
|       - | 4061 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4062 | `			return SXERR_ABORT;` |
|       - | 4063 | `		}` |
|     ! 0 | 4064 | `	}` |
|       - | 4065 | `	/* Swap token stream */` |
|     129 | 4066 | `	pTmp = pGen->pEnd;` |
|     129 | 4067 | `	pGen->pEnd = pEnd;` |
|     129 | 4068 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     129 | 4069 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|     129 | 4070 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 4071 | `	/* Emit the done instruction */` |
|     129 | 4072 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     129 | 4073 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 4074 | `	/* Update token stream */` |
|     129 | 4075 | `	pGen->pIn  = pEnd;` |
|     129 | 4076 | `	pGen->pEnd = pTmp;` |
|     129 | 4077 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 4078 | `		return SXERR_ABORT;` |
|       - | 4079 | `	}` |
|     129 | 4080 | `	return SXRET_OK;` |
|      67 | 4081 | `}` |
|       - | 4082 | `/*` |
|       - | 4083 | ` * Compile the smart switch statement.` |
|       - | 4084 | ` * According to the PHP language reference manual` |
|       - | 4085 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - | 4086 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - | 4087 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - | 4088 | ` *  This is exactly what the switch statement is for.` |
|       - | 4089 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - | 4090 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - | 4091 | ` *  of the outer loop, use continue 2.` |
|       - | 4092 | ` *  Note that switch/case does loose comparision.` |
|       - | 4093 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - | 4094 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - | 4095 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - | 4096 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - | 4097 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - | 4098 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - | 4099 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - | 4100 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - | 4101 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - | 4102 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - | 4103 | ` *  list for the next case.` |
|       - | 4104 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - | 4105 | ` *  or floating-point numbers and strings.` |
|       - | 4106 | ` */` |
|      48 | 4107 | `PH7_PRIVATE sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       5 | 4108 | `{` |
|       - | 4109 | `	GenBlock *pSwitchBlock;` |
|       - | 4110 | `	SyToken *pTmp,*pEnd;` |
|       - | 4111 | `	ph7_switch *pSwitch;` |
|       - | 4112 | `	sxu32 nToken;` |
|       - | 4113 | `	sxu32 nLine;` |
|       - | 4114 | `	sxi32 rc;` |
|      53 | 4115 | `	nLine = pGen->pIn->nLine;` |
|       - | 4116 | `	/* Jump the 'switch' keyword */` |
|      53 | 4117 | `	pGen->pIn++;` |
|      53 | 4118 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 4119 | `		/* Syntax error */` |
|     ! 0 | 4120 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 | 4121 | `		if( rc == SXERR_ABORT ){` |
|       - | 4122 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4123 | `			return SXERR_ABORT;` |
|       - | 4124 | `		}` |
|     ! 0 | 4125 | `		goto Synchronize;` |
|       - | 4126 | `	}` |
|       - | 4127 | `	/* Jump the left parenthesis '(' */` |
|      53 | 4128 | `	pGen->pIn++;` |
|      53 | 4129 | `	pEnd = 0; /* cc warning */` |
|       - | 4130 | `	/* Create the loop block */` |
|      77 | 4131 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      24 | 4132 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      53 | 4133 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4134 | `		return SXERR_ABORT;` |
|       - | 4135 | `	}` |
|       - | 4136 | `	/* Delimit the condition */` |
|      53 | 4137 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      53 | 4138 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 4139 | `		/* Empty expression */` |
|     ! 0 | 4140 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 | 4141 | `		if( rc == SXERR_ABORT ){` |
|       - | 4142 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4143 | `			return SXERR_ABORT;` |
|       - | 4144 | `		}` |
|     ! 0 | 4145 | `	}` |
|       - | 4146 | `	/* Swap token streams */` |
|      53 | 4147 | `	pTmp = pGen->pEnd;` |
|      53 | 4148 | `	pGen->pEnd = pEnd;` |
|       - | 4149 | `	/* Compile the expression */` |
|      53 | 4150 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      53 | 4151 | `	if( rc == SXERR_ABORT ){` |
|       - | 4152 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 4153 | `		return SXERR_ABORT;` |
|       - | 4154 | `	}` |
|       - | 4155 | `	/* Update token stream */` |
|      53 | 4156 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 4157 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4158 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 4159 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4160 | `			return SXERR_ABORT;` |
|       - | 4161 | `		}` |
|     ! 0 | 4162 | `		pGen->pIn++;` |
|     ! 0 | 4163 | `	}` |
|      53 | 4164 | `	pGen->pIn  = &pEnd[1];` |
|      53 | 4165 | `	pGen->pEnd = pTmp;` |
|      53 | 4166 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      48 | 4167 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 | 4168 | `			pTmp = pGen->pIn;` |
|     ! 0 | 4169 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 | 4170 | `				pTmp--;` |
|     ! 0 | 4171 | `			}` |
|       - | 4172 | `			/* Unexpected token */` |
|     ! 0 | 4173 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 | 4174 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4175 | `				return SXERR_ABORT;` |
|       - | 4176 | `			}` |
|     ! 0 | 4177 | `			goto Synchronize;` |
|       - | 4178 | `	}` |
|       - | 4179 | `	/* Set the delimiter token */` |
|      53 | 4180 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       5 | 4181 | `		nToken = PH7_TK_KEYWORD;` |
|       - | 4182 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       3 | 4183 | `	}else{` |
|      49 | 4184 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - | 4185 | `	}` |
|      53 | 4186 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - | 4187 | `	/* Create the switch blocks container */` |
|      53 | 4188 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      53 | 4189 | `	if( pSwitch == 0 ){` |
|       - | 4190 | `		/* Abort compilation */` |
|     ! 0 | 4191 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4192 | `		return SXERR_ABORT;` |
|       - | 4193 | `	}` |
|       - | 4194 | `	/* Zero the structure */` |
|      53 | 4195 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - | 4196 | `	/* Initialize fields */` |
|      53 | 4197 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - | 4198 | `	/* Emit the switch instruction */` |
|      53 | 4199 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - | 4200 | `	/* Compile case blocks */` |
|     138 | 4201 | `	for(;;){` |
|       - | 4202 | `		sxu32 nKwrd;` |
|     167 | 4203 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 4204 | `			/* No more input to process */` |
|     ! 0 | 4205 | `			break;` |
|       - | 4206 | `		}` |
|     167 | 4207 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 4208 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - | 4209 | `				/* Unexpected token */` |
|     ! 0 | 4210 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 4211 | `					&pGen->pIn->sData);` |
|     ! 0 | 4212 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 4213 | `					return SXERR_ABORT;` |
|       - | 4214 | `				}` |
|       - | 4215 | `				/* FALL THROUGH */` |
|     ! 0 | 4216 | `			}` |
|       - | 4217 | `			/* Block compiled */` |
|     ! 0 | 4218 | `			break;` |
|       - | 4219 | `		}` |
|       - | 4220 | `		/* Extract the keyword */` |
|     167 | 4221 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     167 | 4222 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       5 | 4223 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - | 4224 | `				/* Unexpected token */` |
|     ! 0 | 4225 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 4226 | `					&pGen->pIn->sData);` |
|     ! 0 | 4227 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 4228 | `					return SXERR_ABORT;` |
|       - | 4229 | `				}` |
|       - | 4230 | `				/* FALL THROUGH */` |
|     ! 0 | 4231 | `			}` |
|       - | 4232 | `			/* Block compiled */` |
|       5 | 4233 | `			break;` |
|       - | 4234 | `		}` |
|     163 | 4235 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - | 4236 | `			/*` |
|       - | 4237 | `			 * Accroding to the PHP language reference manual` |
|       - | 4238 | `			 *  A special case is the default case. This case matches anything` |
|       - | 4239 | `			 *  that wasn't matched by the other cases.` |
|       - | 4240 | `			 */` |
|      39 | 4241 | `			if( pSwitch->nDefault > 0 ){` |
|       - | 4242 | `				/* Default case already compiled */` |
|     ! 0 | 4243 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 | 4244 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 4245 | `					return SXERR_ABORT;` |
|       - | 4246 | `				}` |
|     ! 0 | 4247 | `			}` |
|      39 | 4248 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - | 4249 | `			/* Compile the default block */` |
|      39 | 4250 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      39 | 4251 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 4252 | `				return SXERR_ABORT;` |
|      39 | 4253 | `			}else if( rc == SXERR_EOF ){` |
|      37 | 4254 | `				break;` |
|       1 | 4255 | `			}` |
|     130 | 4256 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - | 4257 | `			ph7_case_expr sCase;` |
|       - | 4258 | `			/* Standard case block */` |
|     129 | 4259 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - | 4260 | `			/* initialize the structure */` |
|     129 | 4261 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 4262 | `			/* Compile the case expression */` |
|     129 | 4263 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|     129 | 4264 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4265 | `				return SXERR_ABORT;` |
|       - | 4266 | `			}` |
|       - | 4267 | `			/* Compile the case block */` |
|     129 | 4268 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - | 4269 | `			/* Insert in the switch container */` |
|     129 | 4270 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|     129 | 4271 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 4272 | `				return SXERR_ABORT;` |
|     129 | 4273 | `			}else if( rc == SXERR_EOF ){` |
|      15 | 4274 | `				break;` |
|       - | 4275 | `			}` |
|      61 | 4276 | `		}else{` |
|       - | 4277 | `			/* Unexpected token */` |
|     ! 0 | 4278 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 4279 | `				&pGen->pIn->sData);` |
|     ! 0 | 4280 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4281 | `				return SXERR_ABORT;` |
|       - | 4282 | `			}` |
|     ! 0 | 4283 | `			break;` |
|       - | 4284 | `		}` |
|       5 | 4285 | `	}` |
|       - | 4286 | `	/* Fix all jumps now the destination is resolved */` |
|      53 | 4287 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      53 | 4288 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 4289 | `	/* Release the loop block */` |
|      53 | 4290 | `	GenStateLeaveBlock(pGen,0);` |
|      53 | 4291 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - | 4292 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      53 | 4293 | `		pGen->pIn++;` |
|      24 | 4294 | `	}` |
|       - | 4295 | `	/* Statement successfully compiled */` |
|      53 | 4296 | `	return SXRET_OK;` |
|     ! 0 | 4297 | `Synchronize:` |
|       - | 4298 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 4299 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 | 4300 | `		pGen->pIn++;` |
|     ! 0 | 4301 | `	}` |
|     ! 0 | 4302 | `	return SXRET_OK;` |
|      29 | 4303 | `}` |
|       - | 4304 |  |
