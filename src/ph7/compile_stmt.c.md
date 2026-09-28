# src/ph7/compile_stmt.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1838/2362 lines (77.82%)

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
|       2 |   48 | `{` |
|       - |   49 | `	static const char *azOk[] = { "bool", "float", "int", "object", "string",` |
|       - |   50 | `	                              "self", "parent" };` |
|       - |   51 | `	sxu32 i;` |
|      74 |   52 | `	for( i = 0 ; i < SX_ARRAYSIZE(azOk) ; ++i ){` |
|      72 |   53 | `		sxu32 n = (sxu32)SyStrlen(azOk[i]);` |
|      72 |   54 | `		if( pName->nByte == n && SyStrnicmp(pName->zString,azOk[i],n) == 0 ){` |
|      15 |   55 | `			return 1;` |
|       - |   56 | `		}` |
|      30 |   57 | `	}` |
|       3 |   58 | `	return 0;` |
|      10 |   59 | `}` |
|     182 |   60 | `PH7_PRIVATE sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       5 |   61 | `{` |
|       - |   62 | `	SySet *pConsCode,*pInstrContainer;` |
|       - |   63 | `	sxu32 nLineLocal;` |
|       - |   64 | `	SyString *pName;` |
|       - |   65 | `	sxi32 rc;` |
|       - |   66 | `	/* php forbids attributes on a comma-separated const list. Snapshot whether the` |
|       - |   67 | `	 * statement carries any now, before the first constant consumes them. */` |
|     187 |   68 | `	int bHadAttrs = SySetUsed(&pGen->aPendingAttrs) > 0;` |
|       - |   69 | ``	/* php attributes every const-statement compile error to the `const` keyword's`` |
|       - |   70 | ``	 * line, not the offending list element's own line (`const A=1,\nB=strlen()` blames`` |
|       - |   71 | `	 * line 1). Capture it once here, before jumping the keyword. */` |
|     187 |   72 | `	nLineLocal = pGen->pIn->nLine;` |
|     187 |   73 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       - |   74 | ``	/* php allows a single `const` statement to declare several constants at once`` |
|       - |   75 | ``	 * (`const A = 1, B = 2;`). Loop over the comma-separated name = value pairs;`` |
|       - |   76 | `	 * PH7_CompileExpr(EXPR_FLAG_COMMA_STATEMENT) stops each value at the first` |
|       - |   77 | `	 * top-level comma so the next pair starts cleanly. */` |
|      96 |   78 | `Loop:` |
|     197 |   79 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |   80 | `		/* Invalid constant name */` |
|       9 |   81 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"identifier");` |
|       9 |   82 | `		if( rc == SXERR_ABORT ){` |
|       - |   83 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |   84 | `			return SXERR_ABORT;` |
|       - |   85 | `		}` |
|       9 |   86 | `		goto Synchronize;` |
|       - |   87 | `	}` |
|       - |   88 | `	/* Peek constant name */` |
|     191 |   89 | `	pName = &pGen->pIn->sData;` |
|       - |   90 | ``	/* php's global `const` takes an IDENTIFIER: a reserved word is a parse error`` |
|       - |   91 | `` 	 * there, and only a CLASS constant may carry one (`class C { const list = 5; }` `` |
|       - |   92 | ``	 * is php-legal, `const list = 5;` at file scope is not). PHL accepted both, so`` |
|       - |   93 | ``	 * `const LIST = 1;` compiled and READ back — source php refuses to parse.`` |
|       - |   94 | `	 * The words php still allows are the ones its lexer does not reserve: the type` |
|       - |   95 | ``	 * names and the scope words. The word OPERATORS (`and`, `or`, `xor`, `new`,`` |
|       - |   96 | ``	 * `clone`, `instanceof`) are reserved too — the lexer types those ID\|OP rather`` |
|       - |   97 | `	 * than KEYWORD, which is why the test reads both bits. */` |
|     186 |   98 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_OP))` |
|     106 |   99 | `	 && !GenStateConstNameKeywordOk(pName) ){` |
|       3 |  100 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn,"identifier");` |
|       3 |  101 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  102 | `			return SXERR_ABORT;` |
|       - |  103 | `		}` |
|       3 |  104 | `		goto Synchronize;` |
|       - |  105 | `	}` |
|       - |  106 | `	/* Make sure the constant name isn't reserved */` |
|     189 |  107 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  108 | `		/* Reserved constant */` |
|      10 |  109 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Cannot redeclare constant '%z'",pName);` |
|      10 |  110 | `		if( rc == SXERR_ABORT ){` |
|       - |  111 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  112 | `			return SXERR_ABORT;` |
|       - |  113 | `		}` |
|      10 |  114 | `		goto Synchronize;` |
|       - |  115 | `	}` |
|     181 |  116 | `	pGen->pIn++;` |
|     181 |  117 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  118 | `		/* Invalid statement*/` |
|       6 |  119 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"=\"");` |
|       6 |  120 | `		if( rc == SXERR_ABORT ){` |
|       - |  121 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  122 | `			return SXERR_ABORT;` |
|       - |  123 | `		}` |
|       6 |  124 | `		goto Synchronize;` |
|       - |  125 | `	}` |
|     177 |  126 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - |  127 | ``	/* php: a closure in a constant expression must be `static function`; a`` |
|       - |  128 | `	 * non-static closure and any arrow fn are compile-time fatals with distinct` |
|       - |  129 | `	 * messages. Checked ahead of the call scan (it skips closure bodies). */` |
|       - |  130 | `	{` |
|     177 |  131 | `		int iClo = PH7_GenStateInitClosureError(pGen);` |
|     177 |  132 | `		if( iClo ){` |
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
|     173 |  145 | `	if( PH7_GenStateInitHasCallExpr(pGen) ){` |
|       5 |  146 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - |  147 | `			"Constant expression contains invalid operations");` |
|       5 |  148 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  149 | `			return SXERR_ABORT;` |
|       - |  150 | `		}` |
|       5 |  151 | `		goto Synchronize;` |
|       - |  152 | `	}` |
|       - |  153 | `	/* Allocate a new constant value container */` |
|     169 |  154 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|     169 |  155 | `	if( pConsCode == 0 ){` |
|     ! 0 |  156 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  157 | `		return SXERR_ABORT;` |
|       - |  158 | `	}` |
|     169 |  159 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  160 | `	/* Swap bytecode container */` |
|     169 |  161 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     169 |  162 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - |  163 | ``	/* Compile constant value. php: a stray token after `const X = EXPR` is`` |
|       - |  164 | ``	 * `... expecting "," or ";"` (const supports a comma-separated list).`` |
|       - |  165 | `	 * EXPR_FLAG_COMMA_STATEMENT stops this value at the first top-level comma so a` |
|       - |  166 | `	 * following declaration is left for the loop below. */` |
|       - |  167 | `	{` |
|     169 |  168 | `		const char *zSaveConst = pGen->zClauseCloser;` |
|     169 |  169 | `		pGen->zClauseCloser = "\",\" or \";\"";` |
|     169 |  170 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|     169 |  171 | `		pGen->zClauseCloser = zSaveConst;` |
|       - |  172 | `	}` |
|       - |  173 | `	/* Emit the done instruction */` |
|     169 |  174 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     169 |  175 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     169 |  176 | `	if( rc == SXERR_ABORT ){` |
|       - |  177 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  178 | `		return SXERR_ABORT;` |
|       - |  179 | `	}` |
|       - |  180 | ``	/* php parse-errors an empty initializer (`const A = ;`, or a `,B=` list hole);`` |
|       - |  181 | `	 * the class-const path rejects it too. Reject loudly rather than silently` |
|       - |  182 | `	 * defining a NULL constant. (php's error KIND -- a parse error -- differs from` |
|       - |  183 | `	 * PHL's compile fatal, the recursive-descent-vs-bison family; both refuse.) */` |
|     169 |  184 | `	if( rc == SXERR_EMPTY && PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       2 |  185 | `			"Empty constant '%z' value",pName) == SXERR_ABORT ){` |
|     ! 0 |  186 | `		return SXERR_ABORT;` |
|       - |  187 | `	}` |
|     169 |  188 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - |  189 | `	/* Register the constant with namespace-qualified name */` |
|       - |  190 | `	{` |
|       - |  191 | `		SyBlob sFQN;` |
|       - |  192 | `		SyString sFQNStr;` |
|     169 |  193 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     169 |  194 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|     169 |  195 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|       - |  196 | ``		/* php refuses a `const` whose name a local `use const` already took. */`` |
|     169 |  197 | `		if( GenStateGuardImportRedeclare(pGen,2,pName,&sFQNStr,nLineLocal) == SXERR_ABORT ){` |
|     ! 0 |  198 | `			SyBlobRelease(&sFQN);` |
|     ! 0 |  199 | `			return SXERR_ABORT;` |
|       - |  200 | `		}` |
|     251 |  201 | `		rc = PH7_VmRegisterConstantEx(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode,` |
|     164 |  202 | `			(SyString *)SySetPeek(&pGen->pVm->aFiles),nLineLocal,1);` |
|     169 |  203 | `		if( rc == SXRET_OK && SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|       - |  204 | ``			/* php 8.5: attributes on `const` statements — attach the pending`` |
|       - |  205 | `			 * groups to the registered constant record for Reflection. */` |
|      15 |  206 | `			SyHashEntry *pCEntry = SyHashGet(&pGen->pVm->hConstant,` |
|       8 |  207 | `				SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      11 |  208 | `			if( pCEntry ){` |
|      11 |  209 | `				ph7_constant *pRegCons = (ph7_constant *)pCEntry->pUserData;` |
|      11 |  210 | `				if( GenStateConsumeAttrs(&(*pGen),&pRegCons->aAttrs) == SXERR_ABORT ){` |
|     ! 0 |  211 | `					SyBlobRelease(&sFQN);` |
|     ! 0 |  212 | `					return SXERR_ABORT;` |
|       - |  213 | `				}` |
|       8 |  214 | `				if( GenStateCheckAttrPlacement(&(*pGen),&pRegCons->aAttrs,64,64,0,0)` |
|       7 |  215 | `					== SXERR_ABORT ){` |
|     ! 0 |  216 | `					SyBlobRelease(&sFQN);` |
|     ! 0 |  217 | `					return SXERR_ABORT;` |
|       - |  218 | `				}` |
|       4 |  219 | `			}` |
|       4 |  220 | `		}` |
|     169 |  221 | `		SyBlobRelease(&sFQN);` |
|       - |  222 | `	}` |
|     169 |  223 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  224 | `		SySetRelease(pConsCode);` |
|     ! 0 |  225 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 |  226 | `	}` |
|       - |  227 | ``	/* Another declaration in the same statement: `const A = 1, B = 2;`. */`` |
|     169 |  228 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /* ',' */) ){` |
|      14 |  229 | `		if( bHadAttrs ){` |
|       - |  230 | ``			/* php compile-fatals `#[Attr] const A = 1, B = 2;` outright. */`` |
|       3 |  231 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - |  232 | `				"Cannot apply attributes to multiple constants at once");` |
|       3 |  233 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  234 | `				return SXERR_ABORT;` |
|       - |  235 | `			}` |
|       3 |  236 | `			goto Synchronize;` |
|       - |  237 | `		}` |
|      12 |  238 | `		pGen->pIn++; /* Jump the comma */` |
|      12 |  239 | `		goto Loop;` |
|       - |  240 | `	}` |
|     157 |  241 | `	return SXRET_OK;` |
|      15 |  242 | `Synchronize:` |
|       - |  243 | `	/* Synchronize with the next top-level semicolon and avoid compiling this` |
|       - |  244 | ``	 * erroneous statement. BRACE-aware (only `{`...`}`): a rejected closure`` |
|       - |  245 | ``	 * initializer's body holds inner `;` that are not statement terminators, so`` |
|       - |  246 | ``	 * without this its `;` and `}` dangle into a spurious second error where php`` |
|       - |  247 | `	 * halts at the first fatal. Parentheses and brackets are deliberately NOT` |
|       - |  248 | ``	 * tracked -- a lone unbalanced `(`/`[` in erroneous input must not swallow the`` |
|       - |  249 | ``	 * following statements (which would drop later error reports); a `{` never`` |
|       - |  250 | `	 * appears in a valid global-const initializer except as a closure body. */` |
|       - |  251 | `	{` |
|      34 |  252 | `		int iBrace = 0;` |
|     130 |  253 | `		while( pGen->pIn < pGen->pEnd ){` |
|     130 |  254 | `			if( iBrace == 0 && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      34 |  255 | `				break;` |
|       - |  256 | `			}` |
|     100 |  257 | `			if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       3 |  258 | `				iBrace++;` |
|      99 |  259 | `			}else if( pGen->pIn->nType & PH7_TK_CCB ){` |
|       3 |  260 | `				if( iBrace > 0 ){ iBrace--; }` |
|       1 |  261 | `			}` |
|     100 |  262 | `			pGen->pIn++;` |
|       4 |  263 | `		}` |
|       - |  264 | `	}` |
|      34 |  265 | `	return SXRET_OK;` |
|      96 |  266 | `}` |
|       - |  267 | `/*` |
|       - |  268 | ` * Compile the 'continue' statement.` |
|       - |  269 | ` * According to the PHP language reference` |
|       - |  270 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|       - |  271 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|       - |  272 | ` *  iteration.` |
|       - |  273 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|       - |  274 | ` *  the purposes of continue.` |
|       - |  275 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|       - |  276 | ` *  of enclosing loops it should skip to the end of.` |
|       - |  277 | ` *  Note:` |
|       - |  278 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|       - |  279 | ` */` |
|       - |  280 | `/*` |
|       - |  281 | `` * Pick the jump opcode for a `break`/`continue` targeting pLoop and fill in its iP1,`` |
|       - |  282 | ` * emitting the POP_EXCEPTIONs for the crossed trys this statement can resolve here and` |
|       - |  283 | `` * now. All the classification lives in GenStateJumpScope, which `goto` shares.`` |
|       - |  284 | ` */` |
|   46364 |  285 | `static sxi32 GenStateLoopJumpOp(ph7_gen_state *pGen,GenBlock *pLoop,sxi32 *piP1,` |
|       - |  286 | `	GenJumpScope *pCross)` |
|       5 |  287 | `{` |
|       - |  288 | `	/* The loop encloses the break by construction, so the walk always reaches it. Note the` |
|       - |  289 | `	 * walk EMITS the crossed trys' POP_EXCEPTIONs as it goes, before the caller can see` |
|       - |  290 | `	 * pCross->nFinally and reject: a statement about to be fatal therefore leaves a few` |
|       - |  291 | `	 * dead instructions behind. Harmless — the compile error stops the program from` |
|       - |  292 | `	 * running at all — and the alternative is walking the chain twice on every jump. */` |
|   46369 |  293 | `	GenStateJumpScope(&(*pGen),pGen->nCurScopeId,pLoop->nScopeId,TRUE,pCross);` |
|   46369 |  294 | `	return GenStateScopeJumpOp(pCross,piP1);` |
|       5 |  295 | `}` |
|       - |  296 | `/*` |
|       - |  297 | `` * php compile-rejects a `break`/`continue`/`goto` that leaves a `finally` body (a`` |
|       - |  298 | `` * `return` is fine). One wording, one place, for all three statements.`` |
|       - |  299 | ` */` |
|      10 |  300 | `PH7_PRIVATE sxi32 GenStateJumpOutOfFinally(ph7_gen_state *pGen,sxu32 nLine)` |
|       4 |  301 | `{` |
|      14 |  302 | `	return PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  303 | `		"jump out of a finally block is disallowed");` |
|       4 |  304 | `}` |
|   34544 |  305 | `PH7_PRIVATE sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       5 |  306 | `{` |
|       - |  307 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  308 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  309 | `	sxi32 iRawLevel; /* The level as WRITTEN, kept for php's diagnostics */` |
|       - |  310 | `	sxu32 nLineLocal;` |
|       - |  311 | `	sxi32 rc;` |
|   34549 |  312 | `	iRawLevel = 1;` |
|   34549 |  313 | `	nLineLocal = pGen->pIn->nLine;` |
|   34549 |  314 | `	iLevel = 0;` |
|       - |  315 | `	/* Jump the 'continue' keyword */` |
|   34549 |  316 | `	pGen->pIn++;` |
|   34549 |  317 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  318 | `		/* optional numeric argument which tells us how many levels` |
|       - |  319 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  320 | `		 */` |
|       - |  321 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      19 |  322 | `		char *zAlloc = 0;` |
|       - |  323 | `		SyString sNum;` |
|      19 |  324 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      19 |  325 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  326 | `			return SXERR_ABORT;` |
|       - |  327 | `		}` |
|      19 |  328 | `		if( rc == SXRET_OK ){` |
|      24 |  329 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      14 |  330 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      17 |  331 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  332 | `				return SXERR_ABORT;` |
|       - |  333 | `			}` |
|      17 |  334 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      17 |  335 | `			iRawLevel = iLevel;` |
|      17 |  336 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       7 |  337 | `		}` |
|      19 |  338 | `		if( iLevel < 2 ){` |
|       3 |  339 | `			iLevel = 0;` |
|       1 |  340 | `		}` |
|      19 |  341 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       8 |  342 | `	}` |
|       - |  343 | `	/* php rejects a non-positive level outright, before asking where it lands. */` |
|   34549 |  344 | `	if( iRawLevel < 1 ){` |
|     ! 0 |  345 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - |  346 | `			"'continue' operator accepts only positive integers");` |
|     ! 0 |  347 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  348 | `			return SXERR_ABORT;` |
|       - |  349 | `		}` |
|     ! 0 |  350 | `		return SXRET_OK;` |
|       - |  351 | `	}` |
|       - |  352 | `	/* Point to the target loop */` |
|   34549 |  353 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|   34549 |  354 | `	if( pLoop == 0 ){` |
|       - |  355 | ``		/* Same split php makes for `break`: no loop at all keeps the not-in-context`` |
|       - |  356 | ``		 * wording, too FEW loops is `Cannot 'continue' N levels`. */`` |
|      12 |  357 | `		if( GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,0) != 0 ){` |
|     ! 0 |  358 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|     ! 0 |  359 | `				"Cannot 'continue' %d level%s",iRawLevel,iRawLevel == 1 ? "" : "s");` |
|     ! 0 |  360 | `		}else{` |
|      12 |  361 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"'continue' not in the 'loop' or 'switch' context");` |
|       - |  362 | `		}` |
|      12 |  363 | `		if( rc == SXERR_ABORT ){` |
|       - |  364 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  365 | `			return SXERR_ABORT;` |
|       - |  366 | `		}` |
|       7 |  367 | `	}else{` |
|   34539 |  368 | `		sxu32 nInstrIdx = 0;` |
|   34539 |  369 | `		sxi32 iP1 = 0;` |
|       - |  370 | `		GenJumpScope sCross;` |
|   34539 |  371 | `		sxi32 iJmpOp = GenStateLoopJumpOp(&(*pGen),pLoop,&iP1,&sCross);` |
|   34539 |  372 | `		if( sCross.nFinally > 0 ){` |
|       3 |  373 | `			if( GenStateJumpOutOfFinally(&(*pGen),nLineLocal) == SXERR_ABORT ){` |
|     ! 0 |  374 | `				return SXERR_ABORT;` |
|       1 |  375 | `			}` |
|   34538 |  376 | `		}else if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|       - |  377 | ``			/* `continue` inside a switch acts like `break` — which is almost never what`` |
|       - |  378 | `			 * the author meant, so php 7.3+ says so at compile time. The generated jump` |
|       - |  379 | `			 * is unchanged; only the diagnostic was missing. An explicit level` |
|       - |  380 | ``			 * (`continue 2`) targets the enclosing loop and stays silent. */`` |
|       5 |  381 | `			if( iLevel < 1 ){` |
|       5 |  382 | `				PH7_GenCompileError(&(*pGen),E_WARNING,nLineLocal,` |
|       - |  383 | `					"\"continue\" targeting switch is equivalent to \"break\"."` |
|       - |  384 | `					" Did you mean to use \"continue 2\"?");` |
|       2 |  385 | `			}` |
|       5 |  386 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,iP1,0,0,&nInstrIdx);` |
|       5 |  387 | `			if( rc == SXRET_OK ){` |
|       5 |  388 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|       2 |  389 | `			}` |
|       3 |  390 | `		}else{` |
|       - |  391 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|   34533 |  392 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,iP1,pLoop->nFirstInstr,0,&nInstrIdx);` |
|   34533 |  393 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - |  394 | `				JumpFixup sJumpFix;` |
|       - |  395 | `				/* Post-continue */` |
|   17247 |  396 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|   17247 |  397 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|   17247 |  398 | `				sJumpFix.pContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   17247 |  399 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|    8621 |  400 | `			}` |
|       - |  401 | `		}` |
|       - |  402 | `	}` |
|   34549 |  403 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  404 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  405 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 |  406 | `	}` |
|       - |  407 | `	/* Statement successfully compiled */` |
|   34549 |  408 | `	return SXRET_OK;` |
|   17277 |  409 | `}` |
|       - |  410 | `/*` |
|       - |  411 | ` * Compile the 'break' statement.` |
|       - |  412 | ` * According to the PHP language reference` |
|       - |  413 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - |  414 | ` *  structure.` |
|       - |  415 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - |  416 | ` *  enclosing structures are to be broken out of.` |
|       - |  417 | ` */` |
|   11846 |  418 | `PH7_PRIVATE sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       5 |  419 | `{` |
|       - |  420 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  421 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  422 | `	sxi32 iRawLevel; /* The level as WRITTEN, kept for php's diagnostics */` |
|       - |  423 | `	sxu32 nLineLocal;` |
|       - |  424 | `	sxi32 rc;` |
|   11851 |  425 | `	iLevel = 0;` |
|   11851 |  426 | `	iRawLevel = 1;` |
|   11851 |  427 | `	nLineLocal = pGen->pIn->nLine;` |
|       - |  428 | `	/* Jump the 'break' keyword */` |
|   11851 |  429 | `	pGen->pIn++;` |
|   11851 |  430 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  431 | `		/* optional numeric argument which tells us how many levels` |
|       - |  432 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  433 | `		 */` |
|       - |  434 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      22 |  435 | `		char *zAlloc = 0;` |
|       - |  436 | `		SyString sNum;` |
|      22 |  437 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      22 |  438 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  439 | `			return SXERR_ABORT;` |
|       - |  440 | `		}` |
|      22 |  441 | `		if( rc == SXRET_OK ){` |
|      28 |  442 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      16 |  443 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      20 |  444 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  445 | `				return SXERR_ABORT;` |
|       - |  446 | `			}` |
|      20 |  447 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      20 |  448 | `			iRawLevel = iLevel;` |
|      20 |  449 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       8 |  450 | `		}` |
|      22 |  451 | `		if( iLevel < 2 ){` |
|       3 |  452 | `			iLevel = 0;` |
|       1 |  453 | `		}` |
|      22 |  454 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       9 |  455 | `	}` |
|       - |  456 | `	/* php rejects a non-positive level outright, before asking where it lands. */` |
|   11851 |  457 | `	if( iRawLevel < 1 ){` |
|     ! 0 |  458 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  459 | `			"'break' operator accepts only positive integers");` |
|     ! 0 |  460 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  461 | `			return SXERR_ABORT;` |
|       - |  462 | `		}` |
|     ! 0 |  463 | `		goto BreakLevelDone;` |
|       - |  464 | `	}` |
|       - |  465 | `	/* Extract the target loop */` |
|   11851 |  466 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|   17774 |  467 | `	if( pLoop == 0 ){` |
|       - |  468 | `		/* php distinguishes "there is no loop at all here" from "there is one, but` |
|       - |  469 | `		 * not N of them": the first keeps the not-in-context wording, the second is` |
|       - |  470 | ``		 * `Cannot 'break' N levels`. */`` |
|      19 |  471 | `		if( GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,0) != 0 ){` |
|       4 |  472 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 |  473 | `				"Cannot 'break' %d level%s",iRawLevel,iRawLevel == 1 ? "" : "s");` |
|       2 |  474 | `		}else{` |
|      17 |  475 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"'break' not in the 'loop' or 'switch' context");` |
|       - |  476 | `		}` |
|      19 |  477 | `		if( rc == SXERR_ABORT ){` |
|       - |  478 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  479 | `			return SXERR_ABORT;` |
|       - |  480 | `		}` |
|      11 |  481 | `	}else{` |
|       - |  482 | `		sxu32 nInstrIdx;` |
|   11835 |  483 | `		sxi32 iP1 = 0;` |
|       - |  484 | `		GenJumpScope sCross;` |
|   11835 |  485 | `		sxi32 iJmpOp = GenStateLoopJumpOp(&(*pGen),pLoop,&iP1,&sCross);` |
|   11835 |  486 | `		if( sCross.nFinally > 0 ){` |
|       9 |  487 | `			if( GenStateJumpOutOfFinally(&(*pGen),nLineLocal) == SXERR_ABORT ){` |
|     ! 0 |  488 | `				return SXERR_ABORT;` |
|       - |  489 | `			}` |
|       6 |  490 | `		}else{` |
|   11829 |  491 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,iP1,0,0,&nInstrIdx);` |
|   11829 |  492 | `			if( rc == SXRET_OK ){` |
|       - |  493 | `				/* Fix the jump later when the jump destination is resolved */` |
|   11829 |  494 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|    5912 |  495 | `			}` |
|       - |  496 | `		}` |
|       - |  497 | `	}` |
|    5923 |  498 | `BreakLevelDone:` |
|   11851 |  499 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  500 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  501 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 |  502 | `	}` |
|       - |  503 | `	/* Statement successfully compiled */` |
|   11851 |  504 | `	return SXRET_OK;` |
|    5928 |  505 | `}` |
|       - |  506 | `/*` |
|       - |  507 | ` * The function body a goto or a label sits in, or NULL at file scope. A goto may not` |
|       - |  508 | ` * cross functions, so GenStateFixGoto pairs the two on this.` |
|       - |  509 | ` */` |
|     436 |  510 | `static ph7_vm_func * GenStateOwningFunc(ph7_gen_state *pGen)` |
|       5 |  511 | `{` |
|     441 |  512 | `	GenBlock *pBlock = pGen->pCurrent;` |
|    1117 |  513 | `	while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|     681 |  514 | `		pBlock = pBlock->pParent;` |
|       5 |  515 | `	}` |
|     441 |  516 | `	return pBlock ? (ph7_vm_func *)pBlock->pUserData : 0;` |
|       5 |  517 | `}` |
|       - |  518 | `/*` |
|       - |  519 | ` * Compile or record a label.` |
|       - |  520 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|       - |  521 | ` * Example` |
|       - |  522 | ` *  goto LABEL;` |
|       - |  523 | ` *   echo 'Foo';` |
|       - |  524 | ` *  LABEL:` |
|       - |  525 | ` *   echo 'Bar';` |
|       - |  526 | ` */` |
|     214 |  527 | `PH7_PRIVATE sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|       5 |  528 | `{` |
|       - |  529 | `	Label sLabel;` |
|       - |  530 | `	/* php places almost NO restriction on where a label may be DEFINED — inside a loop, a` |
|       - |  531 | `	 * switch or a try{} is all fine; the one rule is that a name may not be declared twice` |
|       - |  532 | `	 * in the same function (below). The rest is on the jump: you may not goto INTO a loop` |
|       - |  533 | `	 * or switch from outside it, which is checked once the labels are all known (see` |
|       - |  534 | `	 * GenStateFixJumps). Record the loop this label sits in so that check can run. */` |
|       - |  535 | `	{` |
|     219 |  536 | `		SyString *pTarget = &pGen->pIn->sData;` |
|     219 |  537 | `		ph7_vm_func *pFunc = GenStateOwningFunc(&(*pGen));` |
|       - |  538 | `		char *zDup;` |
|       - |  539 | `		/* One name, one destination: php compile-rejects a label its function already` |
|       - |  540 | ``		 * declares, wherever the two sit (`L: L:`, one per branch of an if, one in a loop`` |
|       - |  541 | `		 * and one after it). PHL used to accept the redeclaration and silently give every` |
|       - |  542 | `		 * goto the FIRST one. The owning function is part of the key, so the same name in` |
|       - |  543 | `		 * another function — or at file scope beside it — is untouched by this. On the` |
|       - |  544 | `		 * duplicate, keep the first declaration and record nothing: the compile has already` |
|       - |  545 | `		 * failed, and a second entry under the same key would only shadow it. */` |
|     219 |  546 | `		if( SXRET_OK == GenStateGetLabel(&(*pGen),pTarget,pFunc,0) ){` |
|       8 |  547 | `			if( SXERR_ABORT == PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       2 |  548 | `				"Label '%z' already defined",pTarget) ){` |
|     ! 0 |  549 | `				return SXERR_ABORT;` |
|       - |  550 | `			}` |
|       6 |  551 | `			pGen->pIn += 2; /* Jump the label name and the semi-colon */` |
|       6 |  552 | `			return SXRET_OK;` |
|       - |  553 | `		}` |
|       - |  554 | `		/* Initialize label fields */` |
|     215 |  555 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       - |  556 | `		/* Duplicate label name */` |
|     215 |  557 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     215 |  558 | `		if( zDup == 0 ){` |
|     ! 0 |  559 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  560 | `			return SXERR_ABORT;` |
|       - |  561 | `		}` |
|     215 |  562 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|     215 |  563 | `		sLabel.nLine = pGen->pIn->nLine;` |
|     215 |  564 | `		sLabel.nLoopId = pGen->nCurLoopId;` |
|       - |  565 | `		/* Where the label sits, so a goto from a detached catch/finally body can be told` |
|       - |  566 | `		 * what it has to cross to reach it (GenStateJumpScope). */` |
|     215 |  567 | `		sLabel.pContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     215 |  568 | `		sLabel.nScopeId = pGen->nCurScopeId;` |
|       - |  569 | `		/* The owning FUNCTION, matched against the goto's in GenStateFixGoto. This used` |
|       - |  570 | `		 * to stop at GEN_BLOCK_EXCEPTION as well, which attributed every label inside a` |
|       - |  571 | `		 * try or catch body to "no function" — so from anywhere in a function such a` |
|       - |  572 | `		 * label read as undefined, including from the very catch body declaring it.` |
|       - |  573 | `		 * Whether a label may be jumped TO is decided by its container, not by this. */` |
|     215 |  574 | `		sLabel.pFunc = pFunc;` |
|       - |  575 | `		/* Insert in label set */` |
|     215 |  576 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|       - |  577 | `	}` |
|     215 |  578 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|     215 |  579 | `	return SXRET_OK;` |
|     112 |  580 | `}` |
|       - |  581 | `/*` |
|       - |  582 | ` * Compile the so hated 'goto' statement.` |
|       - |  583 | ` * You've probably been taught that gotos are bad, but this sort` |
|       - |  584 | ` * of rewriting  happens all the time, in fact every time you run` |
|       - |  585 | ` * a compiler it has to do this.` |
|       - |  586 | ` * According to the PHP language reference manual` |
|       - |  587 | ` *   The goto operator can be used to jump to another section in the program.` |
|       - |  588 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|       - |  589 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|       - |  590 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|       - |  591 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|       - |  592 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|       - |  593 | ` *   of a multi-level break` |
|       - |  594 | ` */` |
|     226 |  595 | `PH7_PRIVATE sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|       5 |  596 | `{` |
|       - |  597 | `	JumpFixup sJump;` |
|       - |  598 | `	sxi32 rc;` |
|     231 |  599 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|     231 |  600 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  601 | `		/* Missing label */` |
|     ! 0 |  602 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|     ! 0 |  603 | `		if( rc == SXERR_ABORT ){` |
|       - |  604 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  605 | `			return SXERR_ABORT;` |
|       - |  606 | `		}` |
|     ! 0 |  607 | `		return SXRET_OK;` |
|       - |  608 | `	}` |
|     231 |  609 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       6 |  610 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn,"identifier");` |
|       6 |  611 | `		if( rc == SXERR_ABORT ){` |
|       - |  612 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  613 | `			return SXERR_ABORT;` |
|       - |  614 | `		}` |
|       4 |  615 | `	}else{` |
|     227 |  616 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  617 | `		char *zDup;` |
|       - |  618 | `		/* Prepare the jump destination */` |
|     227 |  619 | `		sJump.nJumpType = PH7_OP_JMP;` |
|     227 |  620 | `		sJump.nLine = pGen->pIn->nLine;` |
|       - |  621 | `		/* Gotos resolve at end of compilation, well after any container swap. */` |
|     227 |  622 | `		sJump.pContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     227 |  623 | `		sJump.nScopeId = pGen->nCurScopeId;` |
|       - |  624 | `		/* Duplicate label name */` |
|     227 |  625 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     227 |  626 | `		if( zDup == 0 ){` |
|     ! 0 |  627 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  628 | `			return SXERR_ABORT;` |
|       - |  629 | `		}` |
|     227 |  630 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|       - |  631 | `		/* The loop/switch this goto sits in, for the "goto into a loop" check later. */` |
|     227 |  632 | `		sJump.nLoopId = pGen->nCurLoopId;` |
|       - |  633 | `		/* A goto inside a try{}/catch{} is legal php (jumping OUT of the block is fine);` |
|       - |  634 | `		 * only the owning function matters here, since a goto may not cross functions. */` |
|     227 |  635 | `		sJump.pFunc = GenStateOwningFunc(&(*pGen));` |
|       - |  636 | `		/* Emit the unconditional jump. Inside a DETACHED catch/finally body the target may` |
|       - |  637 | `		 * lie in another bytecode array, which a plain OP_JMP cannot address; enclosing` |
|       - |  638 | `		 * trys likewise need their finally run on the way out, which a plain jump would` |
|       - |  639 | `		 * skip. Emit a structure-crossing jump whenever either is possible — the label is` |
|       - |  640 | `		 * not known yet, so GenStateFixGoto picks the final opcode (and may downgrade it` |
|       - |  641 | `		 * back to OP_JMP once the counts prove to cancel). */` |
|     338 |  642 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,` |
|     222 |  643 | `			sJump.nScopeId > 0 ? PH7_OP_CATCH_JMP : PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|     227 |  644 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|     111 |  645 | `		}` |
|       - |  646 | `	}` |
|     231 |  647 | `	pGen->pIn++; /* Jump the label name */` |
|     231 |  648 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       3 |  649 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|       1 |  650 | `	}` |
|       - |  651 | `	/* Statement successfully compiled */` |
|     231 |  652 | `	return SXRET_OK;` |
|     118 |  653 | `}` |
|       - |  654 | `/*` |
|       - |  655 | ` * Point to the next PHP chunk that will be processed shortly.` |
|       - |  656 | ` * Return SXRET_OK on success. Any other return value indicates` |
|       - |  657 | ` * failure.` |
|       - |  658 | ` */` |
|      20 |  659 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|       2 |  660 | `{` |
|       - |  661 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - |  662 | `	sxu32 nRawObj;` |
|      10 |  663 | `	sxu32 nObjIdx;` |
|       - |  664 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - |  665 | `	 * a PHP block.` |
|       - |  666 | `	 */` |
|      10 |  667 | `Consume:` |
|      22 |  668 | `	nRawObj = nObjIdx = 0;` |
|      22 |  669 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|     ! 0 |  670 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|     ! 0 |  671 | `		if( pRawObj == 0 ){` |
|     ! 0 |  672 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  673 | `			return SXERR_ABORT;` |
|       - |  674 | `		}` |
|       - |  675 | `		/* Mark as constant and emit the load constant instruction */` |
|     ! 0 |  676 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|     ! 0 |  677 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     ! 0 |  678 | `		++nRawObj;` |
|     ! 0 |  679 | `		pGen->pRawIn++; /* Next chunk */` |
|     ! 0 |  680 | `	}` |
|      22 |  681 | `	if( nRawObj > 0 ){` |
|       - |  682 | `		/* Emit the consume instruction */` |
|     ! 0 |  683 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     ! 0 |  684 | `	}` |
|      22 |  685 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|     ! 0 |  686 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|       - |  687 | `		/* Reset the token set (and its trivia sidecar) */` |
|     ! 0 |  688 | `		SySetReset(pTokenSet);` |
|     ! 0 |  689 | `		SySetReset(&pGen->aTrivia);` |
|       - |  690 | `		/* Tokenize input */` |
|     ! 0 |  691 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|     ! 0 |  692 | `			pGen->pRawIn->nLine,pTokenSet,&pGen->aTrivia);` |
|       - |  693 | `		/* Point to the fresh token stream */` |
|     ! 0 |  694 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     ! 0 |  695 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|       - |  696 | `		/* Advance the stream cursor */` |
|     ! 0 |  697 | `		pGen->pRawIn++;` |
|       - |  698 | `		/* TICKET 1433-011 */` |
|     ! 0 |  699 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - |  700 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - |  701 | `			sxi32 rc;` |
|       - |  702 | `			/* Refer to TICKET 1433-009  */` |
|     ! 0 |  703 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|     ! 0 |  704 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|     ! 0 |  705 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       - |  706 | `			/* Synthesized short-tag echo: legitimately an expression here. */` |
|     ! 0 |  707 | `			pGen->nExprEchoOk++;` |
|     ! 0 |  708 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 |  709 | `			pGen->nExprEchoOk--;` |
|     ! 0 |  710 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  711 | `				return SXERR_ABORT;` |
|     ! 0 |  712 | `			}else if( rc != SXERR_EMPTY ){` |
|     ! 0 |  713 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  714 | `			}` |
|     ! 0 |  715 | `			goto Consume;` |
|       - |  716 | `		}` |
|     ! 0 |  717 | `	}else{` |
|       - |  718 | `		/* No more chunks to process */` |
|      22 |  719 | `		pGen->pIn = pGen->pEnd;` |
|      22 |  720 | `		return SXERR_EOF;` |
|       - |  721 | `	}` |
|     ! 0 |  722 | `	return SXRET_OK;` |
|      12 |  723 | `}` |
|       - |  724 | `/*` |
|       - |  725 | ` * Compile a PHP block.` |
|       - |  726 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - |  727 | ` * optionally delimited by braces {}.` |
|       - |  728 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  729 | ` * and this function takes care of generating the appropriate error` |
|       - |  730 | ` * message.` |
|       - |  731 | ` */` |
|  809476 |  732 | `PH7_PRIVATE sxi32 PH7_CompileBlock(` |
|       - |  733 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  734 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - |  735 | `	)` |
|       5 |  736 | `{` |
|       - |  737 | `	sxi32 rc;` |
|       - |  738 | `	sxu32 nLine;` |
|  809481 |  739 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  808335 |  740 | `		nLine = pGen->pIn->nLine;` |
|  808335 |  741 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  808335 |  742 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  743 | `			return SXERR_ABORT;` |
|       - |  744 | `		}` |
|  808335 |  745 | `		pGen->pIn++;` |
|       - |  746 | `		/* Compile until we hit the closing braces '}' */` |
| 1306608 |  747 | `		for(;;){` |
| 2613221 |  748 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      22 |  749 | `				rc = GenStateNextChunk(&(*pGen));` |
|      22 |  750 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  751 | `			 	   return SXERR_ABORT;` |
|       - |  752 | `				}` |
|      22 |  753 | `				if( rc == SXERR_EOF ){` |
|       - |  754 | `					/* No more token to process: the block was never closed. php reports` |
|       - |  755 | `					 * the line the '{' was opened on, not where the input ran out. */` |
|      22 |  756 | `					PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"Unclosed '{' on line %u",nLine);` |
|      22 |  757 | `					break;` |
|       - |  758 | `				}` |
|     ! 0 |  759 | `			}` |
| 2613201 |  760 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - |  761 | `				/* Closing braces found,break immediately*/` |
|  808315 |  762 | `				pGen->pIn++;` |
|  808315 |  763 | `				break;` |
|       - |  764 | `			}` |
|       - |  765 | `			/* Compile a single statement */` |
| 1804891 |  766 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
| 1804891 |  767 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  768 | `				return SXERR_ABORT;` |
|       - |  769 | `			}` |
|       5 |  770 | `		}` |
|  808335 |  771 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  405316 |  772 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|      15 |  773 | `		pGen->pIn++;` |
|      15 |  774 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|      15 |  775 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  776 | `			return SXERR_ABORT;` |
|       - |  777 | `		}` |
|       - |  778 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|      15 |  779 | `		for(;;){` |
|      31 |  780 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  781 | `				rc = GenStateNextChunk(&(*pGen));` |
|     ! 0 |  782 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  783 | `			 	   return SXERR_ABORT;` |
|       - |  784 | `				}` |
|     ! 0 |  785 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|       - |  786 | `					/* No more token to process */` |
|     ! 0 |  787 | `					if( rc == SXERR_EOF ){` |
|     ! 0 |  788 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|       - |  789 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|     ! 0 |  790 | `					}` |
|     ! 0 |  791 | `					break;` |
|       - |  792 | `				}` |
|     ! 0 |  793 | `			}` |
|      31 |  794 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - |  795 | `				sxi32 nKwrd;` |
|       - |  796 | `				/* Keyword found */` |
|      29 |  797 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      32 |  798 | `				if( nKwrd == nKeywordEnd \|\|` |
|      12 |  799 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|       - |  800 | `						/* Delimiter keyword found,break */` |
|      15 |  801 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|      13 |  802 | `							pGen->pIn++; /*  endif;endswitch... */` |
|       6 |  803 | `						}` |
|      15 |  804 | `						break;` |
|       - |  805 | `				}` |
|       7 |  806 | `			}` |
|       - |  807 | `			/* Compile a single statement */` |
|      17 |  808 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|      17 |  809 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  810 | `				return SXERR_ABORT;` |
|       - |  811 | `			}` |
|       1 |  812 | `		}` |
|      15 |  813 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       8 |  814 | `	}else{` |
|       - |  815 | `		/* Compile a single statement */` |
|    1137 |  816 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1137 |  817 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  818 | `			return SXERR_ABORT;` |
|       - |  819 | `		}` |
|       - |  820 | `	}` |
|       - |  821 | `	/* Jump trailing semi-colons ';' */` |
|  809493 |  822 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      13 |  823 | `		pGen->pIn++;` |
|       1 |  824 | `	}` |
|  809481 |  825 | `	return SXRET_OK;` |
|  404743 |  826 | `}` |
|       - |  827 | `/*` |
|       - |  828 | ` * Compile the gentle 'while' statement.` |
|       - |  829 | ` * According to the PHP language reference` |
|       - |  830 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|       - |  831 | ` *  The basic form of a while statement is:` |
|       - |  832 | ` *  while (expr)` |
|       - |  833 | ` *   statement` |
|       - |  834 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|       - |  835 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|       - |  836 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|       - |  837 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|       - |  838 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|       - |  839 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|       - |  840 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|       - |  841 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|       - |  842 | ` *  while (expr):` |
|       - |  843 | ` *    statement` |
|       - |  844 | ` *   endwhile;` |
|       - |  845 | ` */` |
|   17528 |  846 | `PH7_PRIVATE sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       5 |  847 | `{` |
|   17533 |  848 | `	GenBlock *pWhileBlock = 0;` |
|   17533 |  849 | `	SyToken *pTmp,*pEnd = 0;` |
|       - |  850 | `	sxu32 nFalseJump;` |
|       - |  851 | `	sxu32 nLine;` |
|       - |  852 | `	sxi32 rc;` |
|   17533 |  853 | `	nLine = pGen->pIn->nLine;` |
|       - |  854 | `	/* Jump the 'while' keyword */` |
|   17533 |  855 | `	pGen->pIn++;` |
|   17533 |  856 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  857 | `		/* Syntax error */` |
|     ! 0 |  858 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  859 | `		if( rc == SXERR_ABORT ){` |
|       - |  860 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  861 | `			return SXERR_ABORT;` |
|       - |  862 | `		}` |
|     ! 0 |  863 | `		goto Synchronize;` |
|       - |  864 | `	}` |
|       - |  865 | `	/* Jump the left parenthesis '(' */` |
|   17533 |  866 | `	pGen->pIn++;` |
|       - |  867 | `	/* Create the loop block */` |
|   17533 |  868 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   17533 |  869 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  870 | `		return SXERR_ABORT;` |
|       - |  871 | `	}` |
|       - |  872 | `	/* Delimit the condition */` |
|   17533 |  873 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   17533 |  874 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  875 | `		/* Empty condition. php reports the token that actually stopped it -- for` |
|       - |  876 | ``		 * `while ()` that is the ')' -- not a hand-written "expected expression". */`` |
|       3 |  877 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,0);` |
|       3 |  878 | `		if( rc == SXERR_ABORT ){` |
|       - |  879 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  880 | `			return SXERR_ABORT;` |
|       - |  881 | `		}` |
|       1 |  882 | `	}` |
|       - |  883 | `	/* Swap token streams */` |
|   17533 |  884 | `	pTmp = pGen->pEnd;` |
|   17533 |  885 | `	pGen->pEnd = pEnd;` |
|       - |  886 | `	/* Compile the expression */` |
|   17533 |  887 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   17533 |  888 | `	if( rc == SXERR_ABORT ){` |
|       - |  889 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  890 | `		return SXERR_ABORT;` |
|       - |  891 | `	}` |
|       - |  892 | `	/* Update token stream */` |
|   17533 |  893 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  894 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|     ! 0 |  895 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  896 | `			return SXERR_ABORT;` |
|       - |  897 | `		}` |
|     ! 0 |  898 | `		pGen->pIn++;` |
|     ! 0 |  899 | `	}` |
|       - |  900 | `	/* Synchronize pointers */` |
|   17533 |  901 | `	pGen->pIn  = &pEnd[1];` |
|   17533 |  902 | `	pGen->pEnd = pTmp;` |
|       - |  903 | `	/* Emit the false jump */` |
|   17533 |  904 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  905 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   17533 |  906 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - |  907 | `	/* Compile the loop body */` |
|   17533 |  908 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   17533 |  909 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  910 | `		return SXERR_ABORT;` |
|       - |  911 | `	}` |
|       - |  912 | `	/* Emit the unconditional jump to the start of the loop */` |
|   17533 |  913 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - |  914 | `	/* Fix all jumps now the destination is resolved */` |
|   17533 |  915 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  916 | `	/* Release the loop block */` |
|   17533 |  917 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  918 | `	/* Statement successfully compiled */` |
|   17533 |  919 | `	return SXRET_OK;` |
|     ! 0 |  920 | `Synchronize:` |
|       - |  921 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  922 | `	 * compiling this erroneous block.` |
|       - |  923 | `	 */` |
|     ! 0 |  924 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  925 | `		pGen->pIn++;` |
|     ! 0 |  926 | `	}` |
|     ! 0 |  927 | `	return SXRET_OK;` |
|    8769 |  928 | `}` |
|       - |  929 | `/*` |
|       - |  930 | ` * Compile the ugly do..while() statement.` |
|       - |  931 | ` * According to the PHP language reference` |
|       - |  932 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|       - |  933 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|       - |  934 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|       - |  935 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|       - |  936 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|       - |  937 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|       - |  938 | ` *  would end immediately).` |
|       - |  939 | ` *  There is just one syntax for do-while loops:` |
|       - |  940 | ` *  <?php` |
|       - |  941 | ` *  $i = 0;` |
|       - |  942 | ` *  do {` |
|       - |  943 | ` *   echo $i;` |
|       - |  944 | ` *  } while ($i > 0);` |
|       - |  945 | ` * ?>` |
|       - |  946 | ` */` |
|      10 |  947 | `PH7_PRIVATE sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|       3 |  948 | `{` |
|      13 |  949 | `	SyToken *pTmp,*pEnd = 0;` |
|      13 |  950 | `	GenBlock *pDoBlock = 0;` |
|       - |  951 | `	sxu32 nLine;` |
|       - |  952 | `	sxi32 rc;` |
|      13 |  953 | `	nLine = pGen->pIn->nLine;` |
|       - |  954 | `	/* Jump the 'do' keyword */` |
|      13 |  955 | `	pGen->pIn++;` |
|       - |  956 | `	/* Create the loop block */` |
|      13 |  957 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|      13 |  958 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  959 | `		return SXERR_ABORT;` |
|       - |  960 | `	}` |
|       - |  961 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|      13 |  962 | `	pDoBlock->bPostContinue = TRUE;` |
|      13 |  963 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      13 |  964 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  965 | `		return SXERR_ABORT;` |
|       - |  966 | `	}` |
|      13 |  967 | `	if( pGen->pIn < pGen->pEnd ){` |
|      11 |  968 | `		nLine = pGen->pIn->nLine;` |
|       4 |  969 | `	}` |
|      13 |  970 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|       8 |  971 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|       - |  972 | `			/* Missing 'while' statement */` |
|       - |  973 | ``			/* php: `do {} ;` names the ';', while `do {}` at EOF is "unexpected end of`` |
|       - |  974 | `			 * file". The do-block's own slice stops before its terminator, so look at` |
|       - |  975 | `			 * the whole CHUNK stream: a token still there is the one php names; nothing` |
|       - |  976 | `			 * left means end of file (NULL). */` |
|       - |  977 | `			{` |
|       3 |  978 | `				SyToken *pBad = pGen->pIn < pGen->pEnd ? pGen->pIn : 0;` |
|       3 |  979 | `				if( pBad == 0 && pGen->pTokenSet ){` |
|       - |  980 | `					/* The do-block consumed its terminator, so the token php names is` |
|       - |  981 | `					 * the one just behind the cursor -- but only when it is a real` |
|       - |  982 | ``					 * terminator (`do {} ;` -> the ';'). If the block simply ended on`` |
|       - |  983 | `					 * its '}' with nothing after it, php reports end of file. */` |
|       3 |  984 | `					SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|       3 |  985 | `					if( pGen->pIn > pBase && (pGen->pIn[-1].nType & PH7_TK_SEMI) ){` |
|     ! 0 |  986 | `						pBad = &pGen->pIn[-1];` |
|     ! 0 |  987 | `					}` |
|       1 |  988 | `				}` |
|       3 |  989 | `				rc = PH7_GenSyntaxError(pGen,pBad,"\"while\"");` |
|       - |  990 | `			}` |
|       3 |  991 | `			if( rc == SXERR_ABORT ){` |
|       - |  992 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  993 | `				return SXERR_ABORT;` |
|       - |  994 | `			}` |
|       3 |  995 | `			goto Synchronize;` |
|       - |  996 | `	}` |
|       - |  997 | `	/* Jump the 'while' keyword */` |
|      11 |  998 | `	pGen->pIn++;` |
|      11 |  999 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 1000 | `		/* Syntax error */` |
|     ! 0 | 1001 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 | 1002 | `		if( rc == SXERR_ABORT ){` |
|       - | 1003 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1004 | `			return SXERR_ABORT;` |
|       - | 1005 | `		}` |
|     ! 0 | 1006 | `		goto Synchronize;` |
|       - | 1007 | `	}` |
|       - | 1008 | `	/* Jump the left parenthesis '(' */` |
|      11 | 1009 | `	pGen->pIn++;` |
|       - | 1010 | `	/* Delimit the condition */` |
|      11 | 1011 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      11 | 1012 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 1013 | `		/* Empty condition. php reports the token that actually stopped it -- for` |
|       - | 1014 | ``		 * `while ()` that is the ')' -- not a hand-written "expected expression". */`` |
|     ! 0 | 1015 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,0);` |
|     ! 0 | 1016 | `		if( rc == SXERR_ABORT ){` |
|       - | 1017 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1018 | `			return SXERR_ABORT;` |
|       - | 1019 | `		}` |
|     ! 0 | 1020 | `		goto Synchronize;` |
|       - | 1021 | `	}` |
|       - | 1022 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|      11 | 1023 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|       - | 1024 | `		JumpFixup *aPost;` |
|       - | 1025 | `		VmInstr *pInstr;` |
|       - | 1026 | `		sxu32 nJumpDest;` |
|       - | 1027 | `		sxu32 n;` |
|       3 | 1028 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|       3 | 1029 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       5 | 1030 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|       3 | 1031 | `			pInstr = GenStateFixupInstr(&aPost[n]);` |
|       3 | 1032 | `			if( pInstr ){` |
|       - | 1033 | `				/* Fix */` |
|       3 | 1034 | `				pInstr->iP2 = nJumpDest;` |
|       1 | 1035 | `			}` |
|       2 | 1036 | `		}` |
|       1 | 1037 | `	}` |
|       - | 1038 | `	/* Swap token streams */` |
|      11 | 1039 | `	pTmp = pGen->pEnd;` |
|      11 | 1040 | `	pGen->pEnd = pEnd;` |
|       - | 1041 | `	/* Compile the expression */` |
|      11 | 1042 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      11 | 1043 | `	if( rc == SXERR_ABORT ){` |
|       - | 1044 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 1045 | `		return SXERR_ABORT;` |
|       - | 1046 | `	}` |
|       - | 1047 | `	/* Update token stream */` |
|      11 | 1048 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 1049 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|     ! 0 | 1050 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1051 | `			return SXERR_ABORT;` |
|       - | 1052 | `		}` |
|     ! 0 | 1053 | `		pGen->pIn++;` |
|     ! 0 | 1054 | `	}` |
|      11 | 1055 | `	pGen->pIn  = &pEnd[1];` |
|      11 | 1056 | `	pGen->pEnd = pTmp;` |
|       - | 1057 | `	/* Emit the true jump to the beginning of the loop */` |
|      11 | 1058 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|       - | 1059 | `	/* Fix all jumps now the destination is resolved */` |
|      11 | 1060 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 1061 | `	/* Release the loop block */` |
|      11 | 1062 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 1063 | `	/* Statement successfully compiled */` |
|      11 | 1064 | `	return SXRET_OK;` |
|       1 | 1065 | `Synchronize:` |
|       - | 1066 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 1067 | `	 * compiling this erroneous block.` |
|       - | 1068 | `	 */` |
|       3 | 1069 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 1070 | `		pGen->pIn++;` |
|     ! 0 | 1071 | `	}` |
|       3 | 1072 | `	return SXRET_OK;` |
|       8 | 1073 | `}` |
|       - | 1074 | `/*` |
|       - | 1075 | ` * Compile the complex and powerful 'for' statement.` |
|       - | 1076 | ` * According to the PHP language reference` |
|       - | 1077 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|       - | 1078 | ` *  The syntax of a for loop is:` |
|       - | 1079 | ` *  for (expr1; expr2; expr3)` |
|       - | 1080 | ` *   statement` |
|       - | 1081 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|       - | 1082 | ` *  the beginning of the loop.` |
|       - | 1083 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|       - | 1084 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|       - | 1085 | ` *  to FALSE, the execution of the loop ends.` |
|       - | 1086 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|       - | 1087 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|       - | 1088 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|       - | 1089 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|       - | 1090 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|       - | 1091 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|       - | 1092 | ` *  of using the for truth expression.` |
|       - | 1093 | ` */` |
|   34840 | 1094 | `PH7_PRIVATE sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       5 | 1095 | `{` |
|   34845 | 1096 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   34845 | 1097 | `	GenBlock *pForBlock = 0;` |
|       - | 1098 | `	sxu32 nFalseJump;` |
|       - | 1099 | `	sxu32 nLine;` |
|       - | 1100 | `	sxi32 rc;` |
|   34845 | 1101 | `	nLine = pGen->pIn->nLine;` |
|       - | 1102 | `	/* Jump the 'for' keyword */` |
|   34845 | 1103 | `	pGen->pIn++;` |
|   34845 | 1104 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 1105 | `		/* Syntax error */` |
|     ! 0 | 1106 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 | 1107 | `		if( rc == SXERR_ABORT ){` |
|       - | 1108 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1109 | `			return SXERR_ABORT;` |
|       - | 1110 | `		}` |
|     ! 0 | 1111 | `		return SXRET_OK;` |
|       - | 1112 | `	}` |
|       - | 1113 | `	/* Jump the left parenthesis '(' */` |
|   34845 | 1114 | `	pGen->pIn++;` |
|       - | 1115 | `	/* Delimit the init-expr;condition;post-expr */` |
|   34845 | 1116 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   34845 | 1117 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 1118 | `		/* Empty expression */` |
|     ! 0 | 1119 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|     ! 0 | 1120 | `		if( rc == SXERR_ABORT ){` |
|       - | 1121 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1122 | `			return SXERR_ABORT;` |
|       - | 1123 | `		}` |
|       - | 1124 | `		/* Synchronize */` |
|     ! 0 | 1125 | `		pGen->pIn = pEnd;` |
|     ! 0 | 1126 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 1127 | `			pGen->pIn++;` |
|     ! 0 | 1128 | `		}` |
|     ! 0 | 1129 | `		return SXRET_OK;` |
|       - | 1130 | `	}` |
|       - | 1131 | `	/* Swap token streams */` |
|   34845 | 1132 | `	pTmp = pGen->pEnd;` |
|   34845 | 1133 | `	pGen->pEnd = pEnd;` |
|       - | 1134 | `	/* for() clauses are the ONLY place php's grammar allows a comma-separated` |
|       - | 1135 | `	 * expression list, so the comma operator is permitted for their duration` |
|       - | 1136 | `	 * (see GenStateTreeHasComma). A closure body nested inside a clause is` |
|       - | 1137 | `	 * compiled through this same window — recorded as a known leniency. */` |
|   34845 | 1138 | `	pGen->nCommaExprOk++;` |
|   34845 | 1139 | `	pGen->zClauseCloser = "\";\""; /* init/condition clauses close on ';' */` |
|       - | 1140 | `` 	/* Compile initialization expressions if available. Every element of a `for` `` |
|       - | 1141 | ``	 * clause is a statement position in php, `(void)` cast included. */`` |
|   34845 | 1142 | `	GenStateEnableClauseVoidCasts(&(*pGen),1);` |
|   34845 | 1143 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 1144 | `	/* Pop operand lvalues */` |
|   34845 | 1145 | `	if( rc == SXERR_ABORT ){` |
|       - | 1146 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 1147 | `		return SXERR_ABORT;` |
|   34845 | 1148 | `	}else if( rc != SXERR_EMPTY ){` |
|   34843 | 1149 | `		GenStateMarkDiscardedCall(&(*pGen));` |
|   34843 | 1150 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|   17419 | 1151 | `	}` |
|   34845 | 1152 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 1153 | `		/* Syntax error */` |
|     ! 0 | 1154 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|     ! 0 | 1155 | `		if( rc == SXERR_ABORT ){` |
|       - | 1156 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1157 | `			return SXERR_ABORT;` |
|       - | 1158 | `		}` |
|     ! 0 | 1159 | `		return SXRET_OK;` |
|       - | 1160 | `	}` |
|       - | 1161 | `	/* Jump the trailing ';' */` |
|   34845 | 1162 | `	pGen->pIn++;` |
|       - | 1163 | `	/* Create the loop block */` |
|   34845 | 1164 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   34845 | 1165 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1166 | `		return SXERR_ABORT;` |
|       - | 1167 | `	}` |
|       - | 1168 | `	/* Deffer continue jumps */` |
|   34845 | 1169 | `	pForBlock->bPostContinue = TRUE;` |
|       - | 1170 | `	/* Compile the condition */` |
|   34845 | 1171 | `	if( GenStateEnableClauseVoidCasts(&(*pGen),0) ){` |
|       - | 1172 | `		/* php names the clause terminator, not the cast, when the offending` |
|       - | 1173 | ``		 * `(void)` is on the element that has to BE the condition. */`` |
|       3 | 1174 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|       - | 1175 | `			"syntax error, unexpected token \";\", expecting \",\"");` |
|       3 | 1176 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1177 | `			return SXERR_ABORT;` |
|       - | 1178 | `		}` |
|       3 | 1179 | `		return SXRET_OK;` |
|       - | 1180 | `	}` |
|   34843 | 1181 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   34843 | 1182 | `	if( rc == SXERR_ABORT ){` |
|       - | 1183 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 1184 | `		return SXERR_ABORT;` |
|   34843 | 1185 | `	}else if( rc != SXERR_EMPTY ){` |
|       - | 1186 | `		/* Emit the false jump */` |
|   34841 | 1187 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 1188 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   34841 | 1189 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|   17418 | 1190 | `	}` |
|   34843 | 1191 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 1192 | `		/* Syntax error */` |
|       6 | 1193 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|       6 | 1194 | `		if( rc == SXERR_ABORT ){` |
|       - | 1195 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1196 | `			return SXERR_ABORT;` |
|       - | 1197 | `		}` |
|       6 | 1198 | `		return SXRET_OK;` |
|       - | 1199 | `	}` |
|       - | 1200 | `	/* Jump the trailing ';' */` |
|   34839 | 1201 | `	pGen->pIn++;` |
|       - | 1202 | `	/* Save the post condition stream */` |
|   34839 | 1203 | `	pPostStart = pGen->pIn;` |
|       - | 1204 | `	/* Compile the loop body — OUTSIDE the comma window (the body is ordinary` |
|       - | 1205 | ``	 * php, so `(1, 2)` inside it is the parse error it should be). */`` |
|   34839 | 1206 | `	pGen->nCommaExprOk--;` |
|   34839 | 1207 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   34839 | 1208 | `	pGen->pEnd = pTmp;` |
|   34839 | 1209 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   34839 | 1210 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 1211 | `		return SXERR_ABORT;` |
|       - | 1212 | `	}` |
|       - | 1213 | `	/* Fix post-continue jumps */` |
|   34839 | 1214 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - | 1215 | `		JumpFixup *aPost;` |
|       - | 1216 | `		VmInstr *pInstr;` |
|       - | 1217 | `		sxu32 nJumpDest;` |
|       - | 1218 | `		sxu32 n;` |
|   11505 | 1219 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|   11505 | 1220 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|   28745 | 1221 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|   17245 | 1222 | `			pInstr = GenStateFixupInstr(&aPost[n]);` |
|   17245 | 1223 | `			if( pInstr ){` |
|       - | 1224 | `				/* Fix jump */` |
|   17245 | 1225 | `				pInstr->iP2 = nJumpDest;` |
|    8620 | 1226 | `			}` |
|    8625 | 1227 | `		}` |
|    5750 | 1228 | `	}` |
|       - | 1229 | `	/* compile the post-expressions if available */` |
|   34839 | 1230 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 1231 | `		pPostStart++;` |
|     ! 0 | 1232 | `	}` |
|   34839 | 1233 | `	if( pPostStart < pEnd ){` |
|       - | 1234 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   34839 | 1235 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   34839 | 1236 | `		pGen->nCommaExprOk++; /* post-expressions are a clause list again */` |
|   34839 | 1237 | `		pGen->zClauseCloser = "\")\""; /* the post clause closes on ')' */` |
|   34839 | 1238 | `		GenStateEnableClauseVoidCasts(&(*pGen),1);` |
|   34839 | 1239 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   34839 | 1240 | `		pGen->nCommaExprOk--;` |
|   34839 | 1241 | `		pGen->zClauseCloser = 0;` |
|   34839 | 1242 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 1243 | `			/* php names the token and expects ')'; the post-clause runs to the ')'. */` |
|     ! 0 | 1244 | `			rc = PH7_GenSyntaxError(pGen,pGen->pIn,"\")\"");` |
|     ! 0 | 1245 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1246 | `				return SXERR_ABORT;` |
|       - | 1247 | `			}` |
|     ! 0 | 1248 | `			return SXRET_OK;` |
|       - | 1249 | `		}` |
|   34839 | 1250 | `		RE_SWAP_DELIMITER(pGen);` |
|   34839 | 1251 | `		if( rc == SXERR_ABORT ){` |
|       - | 1252 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 1253 | `			return SXERR_ABORT;` |
|   34839 | 1254 | `		}else if( rc != SXERR_EMPTY){` |
|   34839 | 1255 | `			GenStateMarkDiscardedCall(&(*pGen));` |
|       - | 1256 | `			/* Pop operand lvalue */` |
|   34839 | 1257 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|   17417 | 1258 | `		}` |
|   17417 | 1259 | `	}` |
|       - | 1260 | `	/* Emit the unconditional jump to the start of the loop */` |
|   34839 | 1261 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - | 1262 | `	/* Fix all jumps now the destination is resolved */` |
|   34839 | 1263 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 1264 | `	/* Release the loop block */` |
|   34839 | 1265 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 1266 | `	/* Statement successfully compiled */` |
|   34839 | 1267 | `	return SXRET_OK;` |
|   17425 | 1268 | `}` |
|       - | 1269 | `/*` |
|       - | 1270 | ` * Is this keyword token part of a NAME rather than a keyword in its own right?` |
|       - | 1271 | ` *` |
|       - | 1272 | `` * The lexer marks `as` a keyword wherever it appears, and php lets it BE a name`` |
|       - | 1273 | ` * in three places the token stream reaches through a preceding operator: a` |
|       - | 1274 | ` * variable ($as lexes as PH7_TK_DOLLAR followed by the identifier), a property` |
|       - | 1275 | ` * ($o->as, $o?->as) and a class constant (A::as). foreach's separator scan has` |
|       - | 1276 | ` * to look at what PRECEDES the keyword, or it splits inside the name and hands` |
|       - | 1277 | `` * the subject compiler a bare `$`.`` |
|       - | 1278 | ` */` |
|   83640 | 1279 | `static int GenStateKeywordIsName(SyToken *pStart,SyToken *pCur)` |
|       5 | 1280 | `{` |
|       - | 1281 | `	SyToken *pPrev;` |
|   83645 | 1282 | `	if( pCur <= pStart ){` |
|     ! 0 | 1283 | `		return 0;` |
|       - | 1284 | `	}` |
|   83645 | 1285 | `	pPrev = pCur - 1;` |
|   83645 | 1286 | `	if( pPrev->nType & PH7_TK_DOLLAR ){` |
|       6 | 1287 | `		return 1;` |
|       - | 1288 | `	}` |
|   83641 | 1289 | `	if( (pPrev->nType & PH7_TK_OP) == 0 ){` |
|   83633 | 1290 | `		return 0;` |
|       - | 1291 | `	}` |
|      13 | 1292 | `	return (pPrev->sData.nByte == sizeof("->")-1` |
|       6 | 1293 | `			&& SyMemcmp(pPrev->sData.zString,"->",sizeof("->")-1) == 0)` |
|       7 | 1294 | `		\|\| (pPrev->sData.nByte == sizeof("::")-1` |
|       3 | 1295 | `			&& SyMemcmp(pPrev->sData.zString,"::",sizeof("::")-1) == 0)` |
|      16 | 1296 | `		\|\| (pPrev->sData.nByte == sizeof("?->")-1` |
|       4 | 1297 | `			&& SyMemcmp(pPrev->sData.zString,"?->",sizeof("?->")-1) == 0);` |
|   41825 | 1298 | `}` |
|       - | 1299 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - | 1300 | ` *` |
|       - | 1301 | `` * php's `as` target is any WRITABLE expression, not just a variable: a property`` |
|       - | 1302 | ` * ($o->p), a static property (C::$s), an array element ($a['k']) and an append` |
|       - | 1303 | ` * ($a[]) are all accepted, on the key side as much as on the value side. The` |
|       - | 1304 | ` * three shapes php rejects get php's own wording; everything else keeps PH7's.` |
|       - | 1305 | ` */` |
|  112830 | 1306 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 | 1307 | `{` |
|  112835 | 1308 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|  112835 | 1309 | `	const char *zMsg = 0;` |
|       - | 1310 | ``	/* A loop target is a write target: `as $this` and `as (new A)->p` are php's`` |
|       - | 1311 | `	 * own compile fatals. */` |
|  112835 | 1312 | `	rc = GenStateWriteTargetCheck(&(*pGen),pRoot,0);` |
|  112835 | 1313 | `	if( rc != SXRET_OK ){` |
|       3 | 1314 | `		return rc;` |
|       - | 1315 | `	}` |
|  112833 | 1316 | `	if( pRoot->pOp ){` |
|      40 | 1317 | `		switch( pRoot->pOp->iOp ){` |
|      17 | 1318 | `		case EXPR_OP_ARROW:     /* $o->p */` |
|       - | 1319 | `		case EXPR_OP_SUBSCRIPT: /* $a['k'], $a[] */` |
|      35 | 1320 | `			return SXRET_OK;` |
|       2 | 1321 | ``		case EXPR_OP_DC:        /* C::$s — but `as C::K` is php's parse error */`` |
|       6 | 1322 | `			if( !PH7_ExprNodeIsClassConst(pRoot) ){` |
|       3 | 1323 | `				return SXRET_OK;` |
|       - | 1324 | `			}` |
|       3 | 1325 | `			break;` |
|     ! 0 | 1326 | `		case EXPR_OP_NULLSAFE_ARROW:` |
|     ! 0 | 1327 | `			zMsg = "Can't use nullsafe operator in write context";` |
|     ! 0 | 1328 | `			break;` |
|       - | 1329 | ``		/* A CALL target (`as f()`) never reaches here — GenStateWriteTargetCheck`` |
|       - | 1330 | `		 * above already refused it, and with php's function/method distinction. */` |
|     ! 0 | 1331 | `		default:` |
|     ! 0 | 1332 | `			break;` |
|       - | 1333 | `		}` |
|  112792 | 1334 | `	}else if( pRoot->xCode == PH7_CompileVariable ){` |
|  112795 | 1335 | `		return SXRET_OK;` |
|       - | 1336 | `	}` |
|       3 | 1337 | `	if( zMsg == 0 ){` |
|       - | 1338 | ``		/* Not a `variable` in php's grammar: php's own syntax error, which names`` |
|       - | 1339 | `		 * the token that follows the target. */` |
|       3 | 1340 | `		return PH7_ExprOperandNotAVariable(&(*pGen),pRoot);` |
|       - | 1341 | `	}` |
|     ! 0 | 1342 | `	rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,zMsg);` |
|     ! 0 | 1343 | `	if( rc != SXERR_ABORT ){` |
|     ! 0 | 1344 | `		rc = SXERR_INVALID;` |
|     ! 0 | 1345 | `	}` |
|     ! 0 | 1346 | `	return rc;` |
|   56420 | 1347 | `}` |
|       - | 1348 | `/*` |
|       - | 1349 | `` * Is this `as` target the plain `$name` shape?`` |
|       - | 1350 | ` *` |
|       - | 1351 | ` * Only that shape can be installed by NAME the way ph7_foreach_info records it` |
|       - | 1352 | ` * (the step writes straight into the frame's symbol table). Every other writable` |
|       - | 1353 | ` * target — including a variable-variable, whose name php re-evaluates per step —` |
|       - | 1354 | ` * goes through a synthetic temporary plus a real store (GenStateForeachStoreTarget).` |
|       - | 1355 | ` */` |
|  112830 | 1356 | `static int GenStateForeachTargetIsPlainVar(SyToken *pStart,SyToken *pEnd)` |
|       5 | 1357 | `{` |
|  169230 | 1358 | `	return (pEnd == &pStart[2])` |
|  112810 | 1359 | `		&& (pStart[0].nType & PH7_TK_DOLLAR)` |
|  169225 | 1360 | `		&& (pStart[1].nType & PH7_TK_ID);` |
|       5 | 1361 | `}` |
|       - | 1362 | `/*` |
|       - | 1363 | `` * Reserve the synthetic temporary a complex `as` target's step value lands in.`` |
|       - | 1364 | ` * The bracketed name cannot collide with a user variable — the same trick the` |
|       - | 1365 | ` * list()/[...] destructuring path uses.` |
|       - | 1366 | ` */` |
|     102 | 1367 | `static sxi32 GenStateForeachTempName(ph7_gen_state *pGen,const char *zTag,SyString *pOut)` |
|       5 | 1368 | `{` |
|       - | 1369 | `	static int iForeachTargetCnt = 0;` |
|       - | 1370 | `	char zTmp[128];` |
|       - | 1371 | `	sxu32 nLen;` |
|       - | 1372 | `	char *zDup;` |
|     107 | 1373 | `	nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_%s_%d__]",zTag,iForeachTargetCnt++);` |
|     107 | 1374 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|     107 | 1375 | `	if( zDup == 0 ){` |
|     ! 0 | 1376 | `		return SXERR_ABORT;` |
|       - | 1377 | `	}` |
|     107 | 1378 | `	SyStringInitFromBuf(pOut,zDup,nLen);` |
|     107 | 1379 | `	return SXRET_OK;` |
|      56 | 1380 | `}` |
|       - | 1381 | `/*` |
|       - | 1382 | `` * Emit `<target> = <temp>` (or `<target> =& <temp>` for a by-reference value) for`` |
|       - | 1383 | `` * one complex `as` target, at the top of the loop body — where php performs the`` |
|       - | 1384 | ` * assignment, once per step.` |
|       - | 1385 | ` *` |
|       - | 1386 | ` * The store is folded exactly as the assignment operator's own codegen folds it` |
|       - | 1387 | ` * (compile.c, precedence-18 site): a member LHS keeps its OP_MEMBER, a subscript` |
|       - | 1388 | ` * becomes STORE_IDX, and a plain name folds into the STORE's p3.` |
|       - | 1389 | ` */` |
|     102 | 1390 | `static sxi32 GenStateForeachStoreTarget(` |
|       - | 1391 | `	ph7_gen_state *pGen,` |
|       - | 1392 | `	SyString *pTemp,   /* Synthetic variable holding this step's value/key */` |
|       - | 1393 | `	SyToken *pStart,   /* Target expression token range */` |
|       - | 1394 | `	SyToken *pEnd,` |
|       - | 1395 | `	int bRef           /* True for a by-reference value target */` |
|       - | 1396 | `	)` |
|       5 | 1397 | `{` |
|     107 | 1398 | `	SyToken *pSavedIn = pGen->pIn;` |
|     107 | 1399 | `	SyToken *pSavedEnd = pGen->pEnd;` |
|     107 | 1400 | `	sxi32 iVmOp = bRef ? PH7_OP_STORE_REF : PH7_OP_STORE;` |
|       - | 1401 | `	VmInstr *pInstr;` |
|     107 | 1402 | `	sxi32 iP1 = 0;` |
|     107 | 1403 | `	sxi32 iP2 = 0;` |
|     107 | 1404 | `	void *p3 = 0;` |
|       - | 1405 | `	sxi32 rc;` |
|       - | 1406 | `	/* The value being stored, below the target — the operand order OP_STORE expects. */` |
|     107 | 1407 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(pTemp),0);` |
|     107 | 1408 | `	pGen->pIn = pStart;` |
|     107 | 1409 | `	pGen->pEnd = pEnd;` |
|     107 | 1410 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE,` |
|       - | 1411 | `		GenStateForEachNodeValidator);` |
|     107 | 1412 | `	pGen->pIn = pSavedIn;` |
|     107 | 1413 | `	pGen->pEnd = pSavedEnd;` |
|     107 | 1414 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 1415 | `		return SXERR_ABORT;` |
|     107 | 1416 | `	}else if( rc != SXRET_OK ){` |
|       - | 1417 | `		/* The validator already reported it; drop the pushed value and carry on. */` |
|     ! 0 | 1418 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 | 1419 | `		return SXRET_OK;` |
|       - | 1420 | `	}` |
|     107 | 1421 | `	pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     107 | 1422 | `	if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 1423 | `		/* A member target resolves (and, for a reference, stashes) its own slot. */` |
|      22 | 1424 | `		if( bRef ){` |
|       3 | 1425 | `			pInstr->iP2 = PH7_MEMBER_REF_TARGET;` |
|       1 | 1426 | `		}` |
|      22 | 1427 | `		iP2 = 1;` |
|      97 | 1428 | `	}else if( pInstr ){` |
|      87 | 1429 | `		(void)PH7_VmPopInstr(pGen->pVm);` |
|      87 | 1430 | `		if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|      19 | 1431 | `			iVmOp = bRef ? PH7_OP_STORE_IDX_REF : PH7_OP_STORE_IDX;` |
|      19 | 1432 | `			iP1 = pInstr->iP1;` |
|      19 | 1433 | `			if( bRef ){` |
|       3 | 1434 | `				iP2 = pInstr->iP2;` |
|       3 | 1435 | `				p3 = pInstr->p3;` |
|       1 | 1436 | `			}` |
|      10 | 1437 | `		}else{` |
|      69 | 1438 | `			p3 = pInstr->p3;` |
|       - | 1439 | `		}` |
|      41 | 1440 | `	}` |
|     107 | 1441 | `	PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|       - | 1442 | `	/* Discard the stored value the store leaves behind */` |
|     107 | 1443 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     107 | 1444 | `	if( bRef ){` |
|       - | 1445 | `		/* The target now holds the element; drop the temporary's own hold, or it` |
|       - | 1446 | `		 * would keep the element a REFERENCE for the rest of the script — an extra` |
|       - | 1447 | ``		 * holder no `unset()` the program can write is able to reach. Dropping the`` |
|       - | 1448 | `		 * NAME never releases the slot the target still refers to. */` |
|       5 | 1449 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UNSET_VAR,0,0,(void *)pTemp,0);` |
|       2 | 1450 | `	}` |
|     107 | 1451 | `	return SXRET_OK;` |
|      56 | 1452 | `}` |
|       - | 1453 | `/*` |
|       - | 1454 | ` * Compile the 'foreach' statement.` |
|       - | 1455 | ` * According to the PHP language reference` |
|       - | 1456 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|       - | 1457 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|       - | 1458 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|       - | 1459 | ` *  is a minor but useful extension of the first:` |
|       - | 1460 | ` *  foreach (array_expression as $value)` |
|       - | 1461 | ` *    statement` |
|       - | 1462 | ` *  foreach (array_expression as $key => $value)` |
|       - | 1463 | ` *   statement` |
|       - | 1464 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|       - | 1465 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|       - | 1466 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|       - | 1467 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|       - | 1468 | ` *  to the variable $key on each loop.` |
|       - | 1469 | ` *  Note:` |
|       - | 1470 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|       - | 1471 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|       - | 1472 | ` *  Note:` |
|       - | 1473 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|       - | 1474 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|       - | 1475 | ` *  or after the foreach without resetting it.` |
|       - | 1476 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|       - | 1477 | ` *  of copying the value.` |
|       - | 1478 | ` */` |
|   83628 | 1479 | `PH7_PRIVATE sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       5 | 1480 | `{` |
|   83633 | 1481 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|   83633 | 1482 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|       - | 1483 | ``	/* Token ranges of a KEY / VALUE target that is not a plain `$name`: it is`` |
|       - | 1484 | `	 * compiled as a real store at the top of the loop body (php assigns the value` |
|       - | 1485 | `	 * first, then the key), against a synthetic temporary the step writes. */` |
|   83633 | 1486 | `	SyToken *pKeyStart = 0,*pKeyEnd = 0;` |
|   83633 | 1487 | `	SyToken *pValStart = 0,*pValEnd = 0;` |
|   83633 | 1488 | `	GenBlock *pForeachBlock = 0;` |
|       - | 1489 | `	ph7_foreach_info *pInfo;` |
|       - | 1490 | `	sxu32 nFalseJump;` |
|       - | 1491 | `	VmInstr *pInstr;` |
|       - | 1492 | `	sxu32 nLine;` |
|       - | 1493 | `	sxi32 rc;` |
|   83633 | 1494 | `	nLine = pGen->pIn->nLine;` |
|       - | 1495 | `	/* Jump the 'foreach' keyword */` |
|   83633 | 1496 | `	pGen->pIn++;` |
|   83633 | 1497 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 1498 | `		/* Syntax error */` |
|     ! 0 | 1499 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 | 1500 | `		if( rc == SXERR_ABORT ){` |
|       - | 1501 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1502 | `			return SXERR_ABORT;` |
|       - | 1503 | `		}` |
|     ! 0 | 1504 | `		goto Synchronize;` |
|       - | 1505 | `	}` |
|       - | 1506 | `	/* Jump the left parenthesis '(' */` |
|   83633 | 1507 | `	pGen->pIn++;` |
|       - | 1508 | `	/* Create the loop block */` |
|   83633 | 1509 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|   83633 | 1510 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1511 | `		return SXERR_ABORT;` |
|       - | 1512 | `	}` |
|       - | 1513 | `	/* Delimit the expression */` |
|   83633 | 1514 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   83633 | 1515 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 1516 | `		/* Empty expression */` |
|     ! 0 | 1517 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|     ! 0 | 1518 | `		if( rc == SXERR_ABORT ){` |
|       - | 1519 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1520 | `			return SXERR_ABORT;` |
|       - | 1521 | `		}` |
|       - | 1522 | `		/* Synchronize */` |
|     ! 0 | 1523 | `		pGen->pIn = pEnd;` |
|     ! 0 | 1524 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 1525 | `			pGen->pIn++;` |
|     ! 0 | 1526 | `		}` |
|     ! 0 | 1527 | `		return SXRET_OK;` |
|       - | 1528 | `	}` |
|       - | 1529 | `	/* Compile the array expression.` |
|       - | 1530 | `	 *` |
|       - | 1531 | ``	 * The separator is the first TOP-LEVEL `as`: one nested inside brackets, parens`` |
|       - | 1532 | `	 * or braces belongs to something else the iterated expression contains — a` |
|       - | 1533 | ``	 * closure with a `foreach` of its own is the shape that finds this, and cutting`` |
|       - | 1534 | ``	 * at its inner `as` left the outer expression with an unclosed bracket and made`` |
|       - | 1535 | ``	 * `foreach ([function(){ foreach ([1] as $k) … }] as $f)` a compile fatal on`` |
|       - | 1536 | ``	 * source php runs. An `as` that is part of a NAME ($as, $o->as, A::as) is not a`` |
|       - | 1537 | `	 * separator either. */` |
|   83633 | 1538 | `	pCur = pGen->pIn;` |
|       - | 1539 | `	{` |
|   83633 | 1540 | `		sxi32 iNest = 0;` |
|  546951 | 1541 | `		while( pCur < pEnd ){` |
|  546951 | 1542 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|   44685 | 1543 | `				iNest++;` |
|  524611 | 1544 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       - | 1545 | `				/* A mismatch here is the expression parser's to report. */` |
|   44685 | 1546 | `				iNest--;` |
|  479931 | 1547 | `			}else if( iNest <= 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|  100897 | 1548 | `				sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|  100897 | 1549 | `				if( nKeywrd == PH7_TKWRD_AS && !GenStateKeywordIsName(pGen->pIn,pCur) ){` |
|   83633 | 1550 | `					break;` |
|       - | 1551 | `				}` |
|    8632 | 1552 | `			}` |
|       - | 1553 | `			/* Advance the stream cursor */` |
|  463323 | 1554 | `			pCur++;` |
|       5 | 1555 | `		}` |
|       - | 1556 | `	}` |
|   83633 | 1557 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 | 1558 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - | 1559 | `			"foreach: Missing array/object expression");` |
|     ! 0 | 1560 | `		if( rc == SXERR_ABORT ){` |
|       - | 1561 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 1562 | `			return SXERR_ABORT;` |
|       - | 1563 | `		}` |
|     ! 0 | 1564 | `		goto Synchronize;` |
|       - | 1565 | `	}` |
|       - | 1566 | `	/* Swap token streams */` |
|   83633 | 1567 | `	pTmp = pGen->pEnd;` |
|   83633 | 1568 | `	pGen->pEnd = pCur;` |
|   83633 | 1569 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   83633 | 1570 | `	if( rc == SXERR_ABORT ){` |
|       - | 1571 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 1572 | `		return SXERR_ABORT;` |
|       - | 1573 | `	}` |
|       - | 1574 | `	/* Update token stream */` |
|   83633 | 1575 | `	while(pGen->pIn < pCur ){` |
|     ! 0 | 1576 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 1577 | `		if( rc == SXERR_ABORT ){` |
|       - | 1578 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 1579 | `			return SXERR_ABORT;` |
|       - | 1580 | `		}` |
|     ! 0 | 1581 | `		pGen->pIn++;` |
|     ! 0 | 1582 | `	}` |
|   83633 | 1583 | `	pCur++; /* Jump the 'as' keyword */` |
|   83633 | 1584 | `	pGen->pIn = pCur;` |
|   83633 | 1585 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 1586 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 | 1587 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1588 | `			return SXERR_ABORT;` |
|       - | 1589 | `		}` |
|     ! 0 | 1590 | `	}` |
|       - | 1591 | `	/* Create the foreach context */` |
|   83633 | 1592 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|   83633 | 1593 | `	if( pInfo == 0 ){` |
|     ! 0 | 1594 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 1595 | `		return SXERR_ABORT;` |
|       - | 1596 | `	}` |
|       - | 1597 | `	/* Zero the structure */` |
|   83633 | 1598 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - | 1599 | `	/* Initialize structure fields */` |
|   83633 | 1600 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - | 1601 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|       - | 1602 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|       - | 1603 | `	 * '=>'. */` |
|   83633 | 1604 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|   83633 | 1605 | `	if( pCur < pEnd ){` |
|       - | 1606 | `		/* Compile the expression holding the key name */` |
|   29397 | 1607 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 | 1608 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 | 1609 | `			if( rc == SXERR_ABORT ){` |
|       - | 1610 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 1611 | `				return SXERR_ABORT;` |
|     ! 0 | 1612 | `			}` |
|   29397 | 1613 | `		}else if( !GenStateForeachTargetIsPlainVar(pGen->pIn,pCur) ){` |
|       - | 1614 | `			/* A writable but non-name key target ($o->k, C::$s, $a['k'], $$n): the` |
|       - | 1615 | `			 * step lands in a temporary and the store runs in the loop body. */` |
|      13 | 1616 | `			pKeyStart = pGen->pIn;` |
|      13 | 1617 | `			pKeyEnd = pCur;` |
|      13 | 1618 | `			if( GenStateForeachTempName(&(*pGen),"key",&pInfo->sKey) != SXRET_OK ){` |
|     ! 0 | 1619 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1620 | `				return SXERR_ABORT;` |
|       - | 1621 | `			}` |
|      13 | 1622 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       7 | 1623 | `		}else{` |
|   29385 | 1624 | `			pGen->pEnd = pCur;` |
|   29385 | 1625 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|   29385 | 1626 | `			if( rc == SXERR_ABORT ){` |
|       - | 1627 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 1628 | `				return SXERR_ABORT;` |
|       - | 1629 | `			}` |
|   29385 | 1630 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|   29385 | 1631 | `			if( pInstr->p3 ){` |
|       - | 1632 | `				/* Record key name */` |
|   29385 | 1633 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|   14690 | 1634 | `			}` |
|   29385 | 1635 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - | 1636 | `		}` |
|   29397 | 1637 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|   14696 | 1638 | `	}` |
|   83633 | 1639 | `	pGen->pEnd = pEnd;` |
|   83633 | 1640 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 1641 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 | 1642 | `		if( rc == SXERR_ABORT ){` |
|       - | 1643 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 1644 | `			return SXERR_ABORT;` |
|       - | 1645 | `		}` |
|     ! 0 | 1646 | `		goto Synchronize;` |
|       - | 1647 | `	}` |
|   83633 | 1648 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      75 | 1649 | `		pGen->pIn++;` |
|       - | 1650 | `		/* Pass by reference  */` |
|      75 | 1651 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|      36 | 1652 | `	}` |
|       - | 1653 | `	/* Check if the value target is list() */` |
|   83633 | 1654 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 | 1655 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - | 1656 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|       - | 1657 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|       - | 1658 | `		 */` |
|       - | 1659 | `		static int iForeachListCnt = 0;` |
|       - | 1660 | `		char zTmp[128];` |
|       - | 1661 | `		sxu32 nLen;` |
|       - | 1662 | `		char *zDup;` |
|      10 | 1663 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|      10 | 1664 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      10 | 1665 | `		if( zDup == 0 ){` |
|     ! 0 | 1666 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1667 | `			return SXERR_ABORT;` |
|       - | 1668 | `		}` |
|      10 | 1669 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - | 1670 | `		/* Save list() token boundaries */` |
|      10 | 1671 | `		pListStart = pGen->pIn;` |
|       - | 1672 | `		/* Advance past list(...) — validate parentheses */` |
|      10 | 1673 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|      10 | 1674 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 1675 | `			/* php: syntax error, unexpected variable "$x", expecting "(" */` |
|       3 | 1676 | `			rc = PH7_GenSyntaxError(pGen,pGen->pIn < pEnd ? pGen->pIn : 0,"\"(\"");` |
|       3 | 1677 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1678 | `				return SXERR_ABORT;` |
|       - | 1679 | `			}` |
|       3 | 1680 | `			goto Synchronize;` |
|       - | 1681 | `		}` |
|       7 | 1682 | `		pGen->pIn++; /* Jump '(' */` |
|       7 | 1683 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       7 | 1684 | `		if( pListEnd >= pEnd ){` |
|     ! 0 | 1685 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 1686 | `				"foreach: Missing closing ')' after list");` |
|     ! 0 | 1687 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1688 | `				return SXERR_ABORT;` |
|       - | 1689 | `			}` |
|     ! 0 | 1690 | `			goto Synchronize;` |
|       - | 1691 | `		}` |
|       7 | 1692 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|       7 | 1693 | `		pListEnd = pGen->pIn;` |
|       7 | 1694 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|   83628 | 1695 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - | 1696 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|       - | 1697 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|       - | 1698 | `		 */` |
|       - | 1699 | `		static int iForeachShortListCnt = 0;` |
|       - | 1700 | `		char zTmp[128];` |
|       - | 1701 | `		sxu32 nLen;` |
|       - | 1702 | `		char *zDup;` |
|     187 | 1703 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|     187 | 1704 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|     187 | 1705 | `		if( zDup == 0 ){` |
|     ! 0 | 1706 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1707 | `			return SXERR_ABORT;` |
|       - | 1708 | `		}` |
|     187 | 1709 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - | 1710 | `		/* Save [...] token boundaries */` |
|     187 | 1711 | `		pListStart = pGen->pIn;` |
|       - | 1712 | `		/* Advance past [...] */` |
|     187 | 1713 | `		pGen->pIn++; /* Jump '[' */` |
|     187 | 1714 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|     187 | 1715 | `		if( pListEnd >= pEnd ){` |
|     ! 0 | 1716 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 1717 | `				"foreach: Missing closing ']' after short list");` |
|     ! 0 | 1718 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1719 | `				return SXERR_ABORT;` |
|       - | 1720 | `			}` |
|     ! 0 | 1721 | `			goto Synchronize;` |
|       - | 1722 | `		}` |
|     187 | 1723 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|     187 | 1724 | `		pListEnd = pGen->pIn;` |
|     187 | 1725 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|   83534 | 1726 | `	}else if( !GenStateForeachTargetIsPlainVar(pGen->pIn,pEnd) ){` |
|       - | 1727 | `		/* A writable but non-name value target — same treatment as the key above. */` |
|      95 | 1728 | `		pValStart = pGen->pIn;` |
|      95 | 1729 | `		pValEnd = pEnd;` |
|      95 | 1730 | `		if( GenStateForeachTempName(&(*pGen),"val",&pInfo->sValue) != SXRET_OK ){` |
|     ! 0 | 1731 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1732 | `			return SXERR_ABORT;` |
|       - | 1733 | `		}` |
|      50 | 1734 | `	}else{` |
|       - | 1735 | `		/* Compile the expression holding the value name */` |
|   83353 | 1736 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|   83353 | 1737 | `		if( rc == SXERR_ABORT ){` |
|       - | 1738 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 1739 | `			return SXERR_ABORT;` |
|       - | 1740 | `		}` |
|   83353 | 1741 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|   83353 | 1742 | `		if( pInstr->p3 ){` |
|       - | 1743 | `			/* Record value name */` |
|   83353 | 1744 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|   41674 | 1745 | `		}` |
|       - | 1746 | `	}` |
|       - | 1747 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|   83631 | 1748 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - | 1749 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   83631 | 1750 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - | 1751 | `	/* Record the first instruction to execute */` |
|   83631 | 1752 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - | 1753 | `	/* Emit the FOREACH_STEP instruction */` |
|   83631 | 1754 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - | 1755 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   83631 | 1756 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - | 1757 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|   83631 | 1758 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|       - | 1759 | `		SyToken *pSavedIn,*pSavedEnd;` |
|       - | 1760 | `		/* Load the temporary variable holding the current value onto the stack.` |
|       - | 1761 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|       - | 1762 | `		 */` |
|     193 | 1763 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|       - | 1764 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|       - | 1765 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|       - | 1766 | `		 * picks up the delimiter and the variable names inside.` |
|       - | 1767 | `		 */` |
|     193 | 1768 | `		pSavedIn = pGen->pIn;` |
|     193 | 1769 | `		pSavedEnd = pGen->pEnd;` |
|     193 | 1770 | `		pGen->pIn = pListStart;` |
|     193 | 1771 | `		pGen->pEnd = pListEnd;` |
|     193 | 1772 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|     187 | 1773 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|      96 | 1774 | `		}else{` |
|       7 | 1775 | `			rc = PH7_CompileList(&(*pGen),0);` |
|       - | 1776 | `		}` |
|     193 | 1777 | `		pGen->pIn = pSavedIn;` |
|     193 | 1778 | `		pGen->pEnd = pSavedEnd;` |
|     193 | 1779 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1780 | `			return SXERR_ABORT;` |
|       - | 1781 | `		}` |
|       - | 1782 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|     193 | 1783 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      94 | 1784 | `	}` |
|       - | 1785 | `	/* Store this step's value and key into their non-name targets. php performs the` |
|       - | 1786 | `	 * VALUE assignment first — visible through a __set() pair, and the order the` |
|       - | 1787 | `	 * symbol table records the two locals in for the plain-name shape. */` |
|   83631 | 1788 | `	if( pValStart ){` |
|     140 | 1789 | `		rc = GenStateForeachStoreTarget(&(*pGen),&pInfo->sValue,pValStart,pValEnd,` |
|      90 | 1790 | `			(pInfo->iFlags & PH7_4EACH_STEP_REF) != 0);` |
|      95 | 1791 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1792 | `			return SXERR_ABORT;` |
|       - | 1793 | `		}` |
|      45 | 1794 | `	}` |
|   83631 | 1795 | `	if( pKeyStart ){` |
|      13 | 1796 | `		rc = GenStateForeachStoreTarget(&(*pGen),&pInfo->sKey,pKeyStart,pKeyEnd,0);` |
|      13 | 1797 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1798 | `			return SXERR_ABORT;` |
|       - | 1799 | `		}` |
|       6 | 1800 | `	}` |
|       - | 1801 | `	/* Compile the loop body */` |
|   83631 | 1802 | `	pGen->pIn = &pEnd[1];` |
|   83631 | 1803 | `	pGen->pEnd = pTmp;` |
|   83631 | 1804 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|   83631 | 1805 | `	if( rc == SXERR_ABORT ){` |
|       - | 1806 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 1807 | `		return SXERR_ABORT;` |
|       - | 1808 | `	}` |
|       - | 1809 | `	/* Emit the unconditional jump to the start of the loop */` |
|   83631 | 1810 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - | 1811 | `	/* Fix all jumps now the destination is resolved */` |
|   83631 | 1812 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 1813 | `	/* Release the loop block */` |
|   83631 | 1814 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 1815 | `	/* Statement successfully compiled */` |
|   83631 | 1816 | `	return SXRET_OK;` |
|       1 | 1817 | `Synchronize:` |
|       - | 1818 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 1819 | `	 * compiling this erroneous block.` |
|       - | 1820 | `	 */` |
|       3 | 1821 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 1822 | `		pGen->pIn++;` |
|     ! 0 | 1823 | `	}` |
|       3 | 1824 | `	return SXRET_OK;` |
|   41819 | 1825 | `}` |
|       - | 1826 | `/*` |
|       - | 1827 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - | 1828 | ` * According to the PHP language reference` |
|       - | 1829 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - | 1830 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - | 1831 | ` *  that is similar to that of C:` |
|       - | 1832 | ` *  if (expr)` |
|       - | 1833 | ` *   statement` |
|       - | 1834 | ` *  else construct:` |
|       - | 1835 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - | 1836 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - | 1837 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - | 1838 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - | 1839 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - | 1840 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - | 1841 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - | 1842 | ` *  elseif` |
|       - | 1843 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - | 1844 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - | 1845 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - | 1846 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - | 1847 | ` *   than b, a equal to b or a is smaller than b:` |
|       - | 1848 | ` *   <?php` |
|       - | 1849 | ` *    if ($a > $b) {` |
|       - | 1850 | ` *     echo "a is bigger than b";` |
|       - | 1851 | ` *    } elseif ($a == $b) {` |
|       - | 1852 | ` *     echo "a is equal to b";` |
|       - | 1853 | ` *    } else {` |
|       - | 1854 | ` *     echo "a is smaller than b";` |
|       - | 1855 | ` *    }` |
|       - | 1856 | ` *    ?>` |
|       - | 1857 | ` */` |
|  410398 | 1858 | `PH7_PRIVATE sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       5 | 1859 | `{` |
|  410403 | 1860 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  410403 | 1861 | `	GenBlock *pCondBlock = 0;` |
|       - | 1862 | `	sxu32 nJumpIdx;` |
|       - | 1863 | `	sxu32 nKeyID;` |
|       - | 1864 | `	sxi32 rc;` |
|       - | 1865 | `	/* Jump the 'if' keyword */` |
|  410403 | 1866 | `	pGen->pIn++;` |
|  410403 | 1867 | `	pToken = pGen->pIn;` |
|       - | 1868 | `	/* Create the conditional block */` |
|  410403 | 1869 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  410403 | 1870 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1871 | `		return SXERR_ABORT;` |
|       - | 1872 | `	}` |
|       - | 1873 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|  228277 | 1874 | `	for(;;){` |
|  456559 | 1875 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 1876 | `			/* Syntax error */` |
|     ! 0 | 1877 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 1878 | `				pToken--;` |
|     ! 0 | 1879 | `			}` |
|     ! 0 | 1880 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 | 1881 | `			if( rc == SXERR_ABORT ){` |
|       - | 1882 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 1883 | `				return SXERR_ABORT;` |
|       - | 1884 | `			}` |
|     ! 0 | 1885 | `			goto Synchronize;` |
|       - | 1886 | `		}` |
|       - | 1887 | `		/* Jump the left parenthesis '(' */` |
|  456559 | 1888 | `		pToken++;` |
|       - | 1889 | `		/* Delimit the condition */` |
|  456559 | 1890 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  456559 | 1891 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - | 1892 | `			/* Syntax error */` |
|     ! 0 | 1893 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 1894 | `				pToken--;` |
|     ! 0 | 1895 | `			}` |
|     ! 0 | 1896 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 | 1897 | `			if( rc == SXERR_ABORT ){` |
|       - | 1898 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 1899 | `				return SXERR_ABORT;` |
|       - | 1900 | `			}` |
|     ! 0 | 1901 | `			goto Synchronize;` |
|       - | 1902 | `		}` |
|       - | 1903 | `		/* Swap token streams */` |
|  456559 | 1904 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - | 1905 | `		/* Compile the condition */` |
|  456559 | 1906 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 1907 | `		/* Update token stream */` |
|  456559 | 1908 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 | 1909 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|     ! 0 | 1910 | `			pGen->pIn++;` |
|     ! 0 | 1911 | `		}` |
|  456559 | 1912 | `		pGen->pIn  = &pEnd[1];` |
|  456559 | 1913 | `		pGen->pEnd = pTmp;` |
|  456559 | 1914 | `		if( rc == SXERR_ABORT ){` |
|       - | 1915 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       3 | 1916 | `			return SXERR_ABORT;` |
|       - | 1917 | `		}` |
|       - | 1918 | `		/* Emit the false jump */` |
|  456557 | 1919 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - | 1920 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  456557 | 1921 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - | 1922 | `		/* Compile the body */` |
|  456557 | 1923 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  456557 | 1924 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1925 | `			return SXERR_ABORT;` |
|       - | 1926 | `		}` |
|  456557 | 1927 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|  107038 | 1928 | `			break;` |
|       - | 1929 | `		}` |
|       - | 1930 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|  242491 | 1931 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  242491 | 1932 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|  155685 | 1933 | `			break;` |
|       - | 1934 | `		}` |
|       - | 1935 | `		/* Emit the unconditional jump */` |
|   86811 | 1936 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - | 1937 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   86811 | 1938 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   86811 | 1939 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   57875 | 1940 | `			pToken = &pGen->pIn[1];` |
|   57875 | 1941 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|   17258 | 1942 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|   20330 | 1943 | `					break;` |
|       - | 1944 | `			}` |
|   17225 | 1945 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    8610 | 1946 | `		}` |
|   46161 | 1947 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - | 1948 | `		/* Synchronize cursors */` |
|   46161 | 1949 | `		pToken = pGen->pIn;` |
|       - | 1950 | `		/* Fix the false jump */` |
|   46161 | 1951 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       5 | 1952 | `	} /* For(;;) */` |
|       - | 1953 | `	/* Fix the false jump */` |
|  410401 | 1954 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  410401 | 1955 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|  196330 | 1956 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - | 1957 | `			/* Compile the else block */` |
|   40655 | 1958 | `			pGen->pIn++;` |
|   40655 | 1959 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   40655 | 1960 | `			if( rc == SXERR_ABORT ){` |
|       - | 1961 |  |
|     ! 0 | 1962 | `				return SXERR_ABORT;` |
|       - | 1963 | `			}` |
|   20325 | 1964 | `	}` |
|  410401 | 1965 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - | 1966 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  410401 | 1967 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - | 1968 | `	/* Release the conditional block */` |
|  410401 | 1969 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 1970 | `	/* Statement successfully compiled */` |
|  410401 | 1971 | `	return SXRET_OK;` |
|     ! 0 | 1972 | `Synchronize:` |
|       - | 1973 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - | 1974 | `	 */` |
|     ! 0 | 1975 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 1976 | `		pGen->pIn++;` |
|     ! 0 | 1977 | `	}` |
|     ! 0 | 1978 | `	return SXRET_OK;` |
|  205204 | 1979 | `}` |
|       - | 1980 | `/*` |
|       - | 1981 | ` * Compile the global construct.` |
|       - | 1982 | ` * According to the PHP language reference` |
|       - | 1983 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - | 1984 | ` *  to be used in that function.` |
|       - | 1985 | ` *  Example #1 Using global` |
|       - | 1986 | ` *  <?php` |
|       - | 1987 | ` *   $a = 1;` |
|       - | 1988 | ` *   $b = 2;` |
|       - | 1989 | ` *   function Sum()` |
|       - | 1990 | ` *   {` |
|       - | 1991 | ` *    global $a, $b;` |
|       - | 1992 | ` *    $b = $a + $b;` |
|       - | 1993 | ` *   }` |
|       - | 1994 | ` *   Sum();` |
|       - | 1995 | ` *   echo $b;` |
|       - | 1996 | ` *  ?>` |
|       - | 1997 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - | 1998 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - | 1999 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - | 2000 | ` */` |
|      88 | 2001 | `PH7_PRIVATE sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       5 | 2002 | `{` |
|      93 | 2003 | `	SyToken *pTmp,*pNext = 0;` |
|       - | 2004 | `	sxi32 nExpr;` |
|       - | 2005 | `	sxi32 rc;` |
|       - | 2006 | `	/* Jump the 'global' keyword */` |
|      93 | 2007 | `	pGen->pIn++;` |
|      93 | 2008 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - | 2009 | `		/* Nothing to process */` |
|     ! 0 | 2010 | `		return SXRET_OK;` |
|       - | 2011 | `	}` |
|      93 | 2012 | `	pTmp = pGen->pEnd;` |
|      93 | 2013 | `	nExpr = 0;` |
|     199 | 2014 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|     111 | 2015 | `		if( pGen->pIn < pNext ){` |
|     111 | 2016 | `			pGen->pEnd = pNext;` |
|     111 | 2017 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 2018 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 | 2019 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 2020 | `					return SXERR_ABORT;` |
|       - | 2021 | `				}` |
|     106 | 2022 | `			}else if( &pGen->pIn[1] < pGen->pEnd` |
|     106 | 2023 | `			 && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|     101 | 2024 | `			 && pGen->pIn[1].sData.nByte == sizeof("this")-1` |
|      56 | 2025 | `			 && SyMemcmp((const void *)pGen->pIn[1].sData.zString,` |
|       3 | 2026 | `			             (const void *)"this",sizeof("this")-1) == 0 ){` |
|       - | 2027 | ``				/* php refuses `global $this;` at the declaration. */`` |
|       3 | 2028 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - | 2029 | `					"Cannot use $this as global variable");` |
|       3 | 2030 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 2031 | `					return SXERR_ABORT;` |
|       - | 2032 | `				}` |
|       2 | 2033 | `			}else{` |
|     109 | 2034 | `				pGen->pIn++;` |
|     109 | 2035 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - | 2036 | `					/* Emit a warning */` |
|     ! 0 | 2037 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 | 2038 | `				}else{` |
|     109 | 2039 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     109 | 2040 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 2041 | `						return SXERR_ABORT;` |
|     109 | 2042 | `					}else if(rc != SXERR_EMPTY ){` |
|     109 | 2043 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|     109 | 2044 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|       - | 2045 | `							/* Variable name, not a constant */` |
|      99 | 2046 | `							pLast->iP1 = 0;` |
|      47 | 2047 | `						}` |
|     109 | 2048 | `						nExpr++;` |
|      52 | 2049 | `					}` |
|       - | 2050 | `				}` |
|       - | 2051 | `			}` |
|      53 | 2052 | `		}` |
|       - | 2053 | `		/* Next expression in the stream */` |
|     111 | 2054 | `		pGen->pIn = pNext;` |
|       - | 2055 | `		/* Jump trailing commas */` |
|     129 | 2056 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|      23 | 2057 | `			pGen->pIn++;` |
|       5 | 2058 | `		}` |
|       5 | 2059 | `	}` |
|       - | 2060 | `	/* Restore token stream */` |
|      93 | 2061 | `	pGen->pEnd = pTmp;` |
|      93 | 2062 | `	if( nExpr > 0 ){` |
|       - | 2063 | `		/* Emit the uplink instruction */` |
|      91 | 2064 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|      43 | 2065 | `	}` |
|      93 | 2066 | `	return SXRET_OK;` |
|      49 | 2067 | `}` |
|       - | 2068 | `/*` |
|       - | 2069 | ` * php's NOUN for a compile-time return diagnostic is the LEXICAL scope, not the` |
|       - | 2070 | `` * kind of function the `return` sits in: zend reads CG(active_class_entry), so a`` |
|       - | 2071 | ` * closure written inside a class body reports "method" and the very same closure` |
|       - | 2072 | ` * written at file scope reports "function". pCurClass is the compiler's exact` |
|       - | 2073 | ` * counterpart (the class/interface/trait/enum whose BODY is being compiled).` |
|       - | 2074 | ` */` |
|      26 | 2075 | `static const char * GenStateReturnNoun(ph7_gen_state *pGen)` |
|       4 | 2076 | `{` |
|      30 | 2077 | `	return pGen->pCurClass ? "method" : "function";` |
|       4 | 2078 | `}` |
|       - | 2079 | `/*` |
|       - | 2080 | ` * TRUE when a declared return type ACCEPTS null — the condition under which php` |
|       - | 2081 | `` * appends its `did you mean "return null;"` hint to the missing-value error.`` |
|       - | 2082 | ``  * That is every nullable declaration (`?T`, `T\|null`, and the standalone `null` `` |
|       - | 2083 | `` * type, all of which set VM_FUNC_RETURN_NULLABLE) plus `mixed`, which includes`` |
|       - | 2084 | ` * null but is stored as a pseudo-CLASS atom rather than through the flag.` |
|       - | 2085 | ` */` |
|      10 | 2086 | `static int GenStateReturnTypeAllowsNull(ph7_vm_func *pFunc)` |
|       4 | 2087 | `{` |
|       - | 2088 | `	SyString *pCls;` |
|      14 | 2089 | `	if( pFunc->iFlags & VM_FUNC_RETURN_NULLABLE ){` |
|       3 | 2090 | `		return 1;` |
|       - | 2091 | `	}` |
|      11 | 2092 | `	pCls = &pFunc->sReturnClass;` |
|       8 | 2093 | `	if( pFunc->nReturnType == SXU32_HIGH && pCls->nByte == sizeof("mixed")-1` |
|       3 | 2094 | `	 && SyStrnicmp(pCls->zString,"mixed",sizeof("mixed")-1) == 0 ){` |
|     ! 0 | 2095 | `		return 1;` |
|       - | 2096 | `	}` |
|      11 | 2097 | `	return 0;` |
|       9 | 2098 | `}` |
|       - | 2099 | `/*` |
|       - | 2100 | ` * Compile the return statement.` |
|       - | 2101 | ` * According to the PHP language reference` |
|       - | 2102 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - | 2103 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - | 2104 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - | 2105 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - | 2106 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - | 2107 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - | 2108 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - | 2109 | ` *  from within the main script file, then script execution end.` |
|       - | 2110 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - | 2111 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - | 2112 | ` *  should do so as PHP has less work to do in this case.` |
|       - | 2113 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - | 2114 | ` */` |
|  287626 | 2115 | `PH7_PRIVATE sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       5 | 2116 | `{` |
|  287631 | 2117 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - | 2118 | `	sxi32 rc;` |
|  287631 | 2119 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  287631 | 2120 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|       - | 2121 | `	ph7_vm_func *pFunc;` |
|       - | 2122 | `	sxu32 nInstrBefore;` |
|       - | 2123 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|       - | 2124 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|       - | 2125 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|       - | 2126 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|       - | 2127 | `	 * normally below so token processing stays consistent. */` |
|  898755 | 2128 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|  611129 | 2129 | `		pFuncBlock = pFuncBlock->pParent;` |
|       5 | 2130 | `	}` |
|  287631 | 2131 | `	pFunc = pFuncBlock ? (ph7_vm_func *)pFuncBlock->pUserData : 0;` |
|  287631 | 2132 | `	if( pFunc && pFunc->nReturnType == MEMOBJ_NEVER ){` |
|       8 | 2133 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       2 | 2134 | `			"A never-returning %s must not return", GenStateReturnNoun(pGen));` |
|       6 | 2135 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2136 | `			return SXERR_ABORT;` |
|       - | 2137 | `		}` |
|       2 | 2138 | `	}` |
|       - | 2139 | `	/* Jump the 'return' keyword */` |
|  287631 | 2140 | `	pGen->pIn++;` |
|  287631 | 2141 | `	nInstrBefore = PH7_VmInstrLength(pGen->pVm);` |
|  287631 | 2142 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_VOID_CAST) ){` |
|       - | 2143 | ``		/* php's `(void)` cast is a STATEMENT prefix, so `return (void) f();` is a`` |
|       - | 2144 | ``		 * parse error there — and the expected-token set is the one `return` has,`` |
|       - | 2145 | ``		 * which is why php names `";"` here and nothing after `$x = (void)`. */`` |
|       3 | 2146 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|       - | 2147 | `			"syntax error, unexpected token \"(void)\", expecting \";\"");` |
|       3 | 2148 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2149 | `			return SXERR_ABORT;` |
|       - | 2150 | `		}` |
|       3 | 2151 | `		return SXRET_OK;` |
|       - | 2152 | `	}` |
|  287629 | 2153 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2154 | ``		/* php: a stray token after `return EXPR` is `... expecting ";"`. */`` |
|  287547 | 2155 | `		const char *zSave = pGen->zClauseCloser;` |
|  287547 | 2156 | `		pGen->zClauseCloser = "\";\"";` |
|       - | 2157 | ``		/* A `return` READS its operand (the value is consumed), so compile it`` |
|       - | 2158 | ``		 * read-only: a lone undefined variable `return $z` must warn at the read`` |
|       - | 2159 | `		 * exactly like echo/interpolation, not be loaded quietly (the same quiet` |
|       - | 2160 | ``		 * load that correctly keeps a bare `$z;` statement silent). Matches the`` |
|       - | 2161 | `		 * arrow-fn implicit-return body fix. */` |
|  287547 | 2162 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|  287547 | 2163 | `		pGen->zClauseCloser = zSave;` |
|  287547 | 2164 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2165 | `			return SXERR_ABORT;` |
|  287547 | 2166 | `		}else if(rc != SXERR_EMPTY ){` |
|  287547 | 2167 | `			nRet = 1;` |
|  143771 | 2168 | `		}` |
|  143771 | 2169 | `	}` |
|       - | 2170 | ``	/* A bare `return;` inside a function that DECLARES a return type is a php`` |
|       - | 2171 | `	 * COMPILE error, not the runtime TypeError PHL used to raise on the way out:` |
|       - | 2172 | `` 	 * php rejects the program before it runs. `void` (which is what `return;` `` |
|       - | 2173 | ``	 * means) and `never` (handled above) are the two declarations exempt from it,`` |
|       - | 2174 | ``	 * and a GENERATOR is exempt whatever it declares — there `return;` ends the`` |
|       - | 2175 | `	 * generator, and the declared type describes the Generator object the call` |
|       - | 2176 | `	 * produced, never the returned value. */` |
|  287624 | 2177 | `	if( nRet == 0 && pFunc && !pGen->bInGenerator && VmFuncHasReturnType(pFunc)` |
|      39 | 2178 | `	 && pFunc->nReturnType != MEMOBJ_VOID && pFunc->nReturnType != MEMOBJ_NEVER ){` |
|      19 | 2179 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - | 2180 | `			"A %s with return type must return a value%s",` |
|       5 | 2181 | `			GenStateReturnNoun(pGen),` |
|      10 | 2182 | `			GenStateReturnTypeAllowsNull(pFunc)` |
|       - | 2183 | `				? " (did you mean \"return null;\" instead of \"return;\"?)" : "");` |
|      14 | 2184 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2185 | `			return SXERR_ABORT;` |
|       - | 2186 | `		}` |
|       5 | 2187 | `	}` |
|       - | 2188 | ``	/* The mirror rule: a `void` function must not return a VALUE, and php stops`` |
|       - | 2189 | `	 * the program at the return statement rather than throwing on the way out.` |
|       - | 2190 | `	 * Generators keep their own diagnostic (a void generator is rejected as` |
|       - | 2191 | ``	 * `Generator return type must be a supertype of Generator`), so they are`` |
|       - | 2192 | `	 * skipped here exactly as above. php's hint fires when the operand is a` |
|       - | 2193 | ``	 * compile-time constant null; PHL folds the `null` KEYWORD (constant index 0,`` |
|       - | 2194 | `	 * emitted as a lone OP_LOADC and unchanged by any wrapping parens), which is` |
|       - | 2195 | `	 * every shape real code writes. */` |
|  287624 | 2196 | `	if( nRet != 0 && pFunc && !pGen->bInGenerator` |
|  277927 | 2197 | `	 && pFunc->nReturnType == MEMOBJ_VOID ){` |
|      16 | 2198 | `		VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      22 | 2199 | `		int bNullLiteral = (PH7_VmInstrLength(pGen->pVm) == nInstrBefore + 1)` |
|      12 | 2200 | `			&& pLast && pLast->iOp == PH7_OP_LOADC` |
|      18 | 2201 | `			&& pLast->iP1 == 0 && pLast->iP2 == 0;` |
|      22 | 2202 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - | 2203 | `			"A void %s must not return a value%s",` |
|       6 | 2204 | `			GenStateReturnNoun(pGen),` |
|       6 | 2205 | `			bNullLiteral` |
|       - | 2206 | `				? " (did you mean \"return;\" instead of \"return null;\"?)" : "");` |
|      16 | 2207 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2208 | `			return SXERR_ABORT;` |
|       - | 2209 | `		}` |
|       6 | 2210 | `	}` |
|       - | 2211 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|       - | 2212 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|       - | 2213 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|       - | 2214 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|  287629 | 2215 | `	if( pGen->bInGenerator ){` |
|      53 | 2216 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|      53 | 2217 | `		return SXRET_OK;` |
|       - | 2218 | `	}` |
|       - | 2219 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|       - | 2220 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|       - | 2221 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|       - | 2222 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|       - | 2223 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|  287581 | 2224 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|  287581 | 2225 | `	return SXRET_OK;` |
|  143818 | 2226 | `}` |
|       - | 2227 | `/*` |
|       - | 2228 | ` * Compile a yield expression.` |
|       - | 2229 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - | 2230 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - | 2231 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - | 2232 | ` */` |
|     518 | 2233 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       5 | 2234 | `{` |
|       - | 2235 | `	SyToken *pTmp, *pSplit;` |
|     523 | 2236 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|     523 | 2237 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - | 2238 | `	sxi32 rc;` |
|     259 | 2239 | `	(void)iCompileFlag;` |
|       - | 2240 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|     523 | 2241 | `	pGen->pIn++;` |
|       - | 2242 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - | 2243 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|       - | 2244 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|       - | 2245 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|       - | 2246 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|     518 | 2247 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|     298 | 2248 | `		&& pGen->pIn->sData.nByte == 4` |
|      83 | 2249 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|      77 | 2250 | `		pGen->pIn++; /* Skip 'from' */` |
|      77 | 2251 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      77 | 2252 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2253 | `			return SXERR_ABORT;` |
|       - | 2254 | `		}` |
|      77 | 2255 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 | 2256 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|     ! 0 | 2257 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|       - | 2258 | `				"Missing expression after 'yield from'");` |
|     ! 0 | 2259 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2260 | `				return SXERR_ABORT;` |
|       - | 2261 | `			}` |
|     ! 0 | 2262 | `		}` |
|      77 | 2263 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|      77 | 2264 | `		return SXRET_OK;` |
|       - | 2265 | `	}` |
|     451 | 2266 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 2267 | `		/* Bare yield — no value */` |
|       8 | 2268 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|       8 | 2269 | `		return SXRET_OK;` |
|       - | 2270 | `	}` |
|       - | 2271 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax. The shared` |
|       - | 2272 | `	 * array-entry scanner is the one that already knows which '=>' are NOT` |
|       - | 2273 | ``	 * separators — an arrow function's body and a match arm's — so `yield fn($x)`` |
|       - | 2274 | `` 	 * => $x` and `yield match($k){ 1 => 2 }` stop being read as `key => value` `` |
|       - | 2275 | `	 * pairs (the first was a parse error, the second yielded the wrong pair). */` |
|     445 | 2276 | `	pSplit = GenStateFindTopLevelArrow(pGen->pIn,pGen->pEnd);` |
|     445 | 2277 | `	if( pSplit >= pGen->pEnd ){` |
|     421 | 2278 | `		pSplit = 0;` |
|     208 | 2279 | `	}` |
|     445 | 2280 | `	pTmp = pGen->pEnd;` |
|     445 | 2281 | `	if( pSplit ){` |
|       - | 2282 | `		/* yield $key => $value */` |
|      26 | 2283 | `		pGen->pEnd = pSplit;` |
|      26 | 2284 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      26 | 2285 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      26 | 2286 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|      26 | 2287 | `		pGen->pEnd = pTmp;` |
|      26 | 2288 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      26 | 2289 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      26 | 2290 | `		iP1 = 1;` |
|      26 | 2291 | `		iP2 = 1;` |
|      14 | 2292 | `	}else{` |
|       - | 2293 | `		/* yield $value */` |
|     421 | 2294 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     421 | 2295 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     421 | 2296 | `		if( rc != SXERR_EMPTY ){` |
|     421 | 2297 | `			iP1 = 1;` |
|     208 | 2298 | `		}` |
|       - | 2299 | `	}` |
|     445 | 2300 | `	pGen->pEnd = pTmp;` |
|     445 | 2301 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|     445 | 2302 | `	return SXRET_OK;` |
|     264 | 2303 | `}` |
|       - | 2304 | `/*` |
|       - | 2305 | ` * Compile the die/exit language construct.` |
|       - | 2306 | ` * The role of these constructs is to terminate execution of the script.` |
|       - | 2307 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - | 2308 | ` */` |
|     190 | 2309 | `PH7_PRIVATE sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       5 | 2310 | `{` |
|     195 | 2311 | `	sxi32 nExpr = 0;` |
|       - | 2312 | `	sxi32 rc;` |
|       - | 2313 | `	/* Jump the die/exit keyword */` |
|     195 | 2314 | `	pGen->pIn++;` |
|     195 | 2315 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2316 | `		/* Compile the expression */` |
|     195 | 2317 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     195 | 2318 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2319 | `			return SXERR_ABORT;` |
|     195 | 2320 | `		}else if(rc != SXERR_EMPTY ){` |
|     195 | 2321 | `			nExpr = 1;` |
|      95 | 2322 | `		}` |
|      95 | 2323 | `	}` |
|       - | 2324 | `	/* Emit the HALT instruction */` |
|     195 | 2325 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|     195 | 2326 | `	return SXRET_OK;` |
|     100 | 2327 | `}` |
|       - | 2328 | `/*` |
|       - | 2329 | ` * Compile the 'echo' language construct.` |
|       - | 2330 | ` */` |
|   27628 | 2331 | `PH7_PRIVATE sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       5 | 2332 | `{` |
|   27633 | 2333 | `	SyToken *pTmp,*pNext = 0;` |
|   27633 | 2334 | `	sxu32 nLine = pGen->pIn->nLine;` |
|   27633 | 2335 | `	int nExpr = 0;      /* expressions actually compiled */` |
|   27633 | 2336 | `	int bExpectMore = 1;/* after 'echo' or a comma an expression is REQUIRED */` |
|       - | 2337 | `	sxi32 rc;` |
|       - | 2338 | `	/* Jump the 'echo' keyword */` |
|   27633 | 2339 | `	pGen->pIn++;` |
|       - | 2340 | `	/* Compile arguments one after one. php: a stray token in an echo list is` |
|       - | 2341 | ``	 * `... expecting "," or ";"` — set the closer for each argument's compile. */`` |
|   27633 | 2342 | `	pTmp = pGen->pEnd;` |
|       - | 2343 | `	{` |
|   27633 | 2344 | `	const char *zSaveEcho = pGen->zClauseCloser;` |
|   79509 | 2345 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   51887 | 2346 | `		if( pGen->pIn < pNext ){` |
|   51887 | 2347 | `			pGen->pEnd = pNext;` |
|   51887 | 2348 | `			pGen->zClauseCloser = "\",\" or \";\"";` |
|   51887 | 2349 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   51887 | 2350 | `			pGen->zClauseCloser = zSaveEcho;` |
|   51887 | 2351 | `			if( rc == SXERR_ABORT ){` |
|       6 | 2352 | `				return SXERR_ABORT;` |
|   51883 | 2353 | `			}else if( rc != SXERR_EMPTY ){` |
|       - | 2354 | `				/* Emit the consume instruction */` |
|   51857 | 2355 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|   51857 | 2356 | `				nExpr++;` |
|   51857 | 2357 | `				bExpectMore = 0;` |
|   25926 | 2358 | `			}` |
|   25939 | 2359 | `		}` |
|       - | 2360 | `		/* Jump trailing commas (php: exactly one between expressions; a` |
|       - | 2361 | `		 * dangling or doubled comma is a parse error, enforced below) */` |
|   76143 | 2362 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|   24267 | 2363 | `			if( bExpectMore ){` |
|       - | 2364 | `				/* two commas in a row */` |
|       3 | 2365 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,pNext->nLine,` |
|       - | 2366 | `					"syntax error, unexpected token \",\"");` |
|       3 | 2367 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - | 2368 | `			}` |
|   24265 | 2369 | `			bExpectMore = 1;` |
|   24265 | 2370 | `			pNext++;` |
|       5 | 2371 | `		}` |
|   51881 | 2372 | `		pGen->pIn = pNext;` |
|       5 | 2373 | `	}` |
|       - | 2374 | `	}` |
|       - | 2375 | `	/* Restore token stream */` |
|   27627 | 2376 | `	pGen->pEnd = pTmp;` |
|   27627 | 2377 | `	if( nExpr == 0 \|\| bExpectMore ){` |
|       - | 2378 | ``		/* `echo ;` or `echo expr, ;` — php rejects both */`` |
|      34 | 2379 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - | 2380 | `			"syntax error, unexpected token \";\"");` |
|      34 | 2381 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - | 2382 | `	}` |
|   27597 | 2383 | `	return SXRET_OK;` |
|   13819 | 2384 | `}` |
|       - | 2385 | `/*` |
|       - | 2386 | ` * Compile the static statement.` |
|       - | 2387 | ` * According to the PHP language reference` |
|       - | 2388 | ` *  Another important feature of variable scoping is the static variable.` |
|       - | 2389 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - | 2390 | ` *  when program execution leaves this scope.` |
|       - | 2391 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - | 2392 | ` * Symisc eXtension.` |
|       - | 2393 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - | 2394 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 2395 | ` *  Example` |
|       - | 2396 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - | 2397 | ` *    Refer to the official documentation for more information on this feature.` |
|       - | 2398 | ` */` |
|      56 | 2399 | `PH7_PRIVATE sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       5 | 2400 | `{` |
|       - | 2401 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - | 2402 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - | 2403 | `	GenBlock *pBlock;` |
|       - | 2404 | `	SyString *pName;` |
|       - | 2405 | `	char *zDup;` |
|       - | 2406 | `	sxu32 nLine;` |
|       - | 2407 | `	sxi32 rc;` |
|       - | 2408 | ``	/* `static function () {}` / `static fn () =>` at statement position is an`` |
|       - | 2409 | `	 * EXPRESSION statement (a bare static closure), not a static-variable` |
|       - | 2410 | `	 * declaration — hand it to the expression compiler (php accepts it). */` |
|      56 | 2411 | `	if( &pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & PH7_TK_KEYWORD)` |
|      34 | 2412 | `	 && (SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FUNCTION` |
|       1 | 2413 | `	  \|\| SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FN) ){` |
|       3 | 2414 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       3 | 2415 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2416 | `			return SXERR_ABORT;` |
|       3 | 2417 | `		}else if( rc != SXERR_EMPTY ){` |
|       3 | 2418 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 2419 | `		}` |
|       3 | 2420 | `		return SXRET_OK;` |
|       - | 2421 | `	}` |
|       - | 2422 | `	/* Jump the static keyword */` |
|      59 | 2423 | `	nLine = pGen->pIn->nLine;` |
|      59 | 2424 | `	pGen->pIn++;` |
|       - | 2425 | `	/* Extract the enclosing function if any */` |
|      59 | 2426 | `	pBlock = pGen->pCurrent;` |
|     113 | 2427 | `	while( pBlock ){` |
|     113 | 2428 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|      59 | 2429 | `			break;` |
|       - | 2430 | `		}` |
|       - | 2431 | `		/* Point to the upper block */` |
|      59 | 2432 | `		pBlock = pBlock->pParent;` |
|       5 | 2433 | `	}` |
|      59 | 2434 | `	if( pBlock == 0 ){` |
|       - | 2435 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 | 2436 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       - | 2437 | ``			/* php: `static FOO;` is a syntax error naming FOO and expecting "::"`` |
|       - | 2438 | ``			 * (the parser is still open to `static::` at that point). */`` |
|     ! 0 | 2439 | `			rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"::\"");` |
|     ! 0 | 2440 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2441 | `				return SXERR_ABORT;` |
|       - | 2442 | `			}` |
|     ! 0 | 2443 | `			goto Synchronize;` |
|       - | 2444 | `		}` |
|       - | 2445 | `		/* Compile the expression holding the variable */` |
|     ! 0 | 2446 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 | 2447 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2448 | `			return SXERR_ABORT;` |
|     ! 0 | 2449 | `		}else if( rc != SXERR_EMPTY ){` |
|       - | 2450 | `			/* Emit the POP instruction */` |
|     ! 0 | 2451 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 | 2452 | `		}` |
|     ! 0 | 2453 | `		return SXRET_OK;` |
|       - | 2454 | `	}` |
|      59 | 2455 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - | 2456 | `	/* Make sure we are dealing with a valid statement */` |
|      59 | 2457 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      52 | 2458 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 2459 | ``			/* php: `static FOO;` is a syntax error naming FOO and expecting "::"`` |
|       - | 2460 | ``			 * (the parser is still open to `static::` at that point). */`` |
|       3 | 2461 | `			rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"::\"");` |
|       3 | 2462 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2463 | `				return SXERR_ABORT;` |
|       - | 2464 | `			}` |
|       3 | 2465 | `			goto Synchronize;` |
|       - | 2466 | `	}` |
|      56 | 2467 | `	pGen->pIn++;` |
|       - | 2468 | `	/* Extract variable name */` |
|      56 | 2469 | `	pName = &pGen->pIn->sData;` |
|       - | 2470 | ``	/* php refuses `static $this;` at the declaration — the name is not a slot a`` |
|       - | 2471 | `	 * function may own. */` |
|      52 | 2472 | `	if( pName->nByte == sizeof("this")-1` |
|      33 | 2473 | `	 && SyMemcmp((const void *)pName->zString,(const void *)"this",sizeof("this")-1) == 0 ){` |
|       3 | 2474 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - | 2475 | `			"Cannot use $this as static variable");` |
|       3 | 2476 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2477 | `			return SXERR_ABORT;` |
|       - | 2478 | `		}` |
|       3 | 2479 | `		goto Synchronize;` |
|       - | 2480 | `	}` |
|      53 | 2481 | `	pGen->pIn++; /* Jump the var name */` |
|      53 | 2482 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 | 2483 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2484 | `		goto Synchronize;` |
|       - | 2485 | `	}` |
|       - | 2486 | `	/* Initialize the structure describing the static variable */` |
|      53 | 2487 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      53 | 2488 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - | 2489 | `	/* Duplicate variable name */` |
|      53 | 2490 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      53 | 2491 | `	if( zDup == 0 ){` |
|     ! 0 | 2492 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2493 | `		return SXERR_ABORT;` |
|       - | 2494 | `	}` |
|      53 | 2495 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - | 2496 | `	/* Check if we have an expression to compile */` |
|      53 | 2497 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - | 2498 | `		SySet *pInstrContainer;` |
|       - | 2499 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - | 2500 | `		 * Static variable can take any complex expression including function` |
|       - | 2501 | `		 * call as their initialization value.` |
|       - | 2502 | `		 * Example:` |
|       - | 2503 | `		 *		static $var = foo(1,4+5,bar());` |
|       - | 2504 | `		 */` |
|      47 | 2505 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - | 2506 | `		/* Swap bytecode container */` |
|      47 | 2507 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      47 | 2508 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - | 2509 | `		/* Compile the expression */` |
|      47 | 2510 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 2511 | `		/* Emit the done instruction */` |
|      47 | 2512 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - | 2513 | `		/* Restore default bytecode container */` |
|      47 | 2514 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      22 | 2515 | `	}` |
|       - | 2516 | `	/* Finally save the compiled static variable in the appropriate container */` |
|      53 | 2517 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|      53 | 2518 | `	return SXRET_OK;` |
|       2 | 2519 | `Synchronize:` |
|       - | 2520 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - | 2521 | `	 * statement.` |
|       - | 2522 | `	 */` |
|      10 | 2523 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       6 | 2524 | `		pGen->pIn++;` |
|       2 | 2525 | `	}` |
|       6 | 2526 | `	return SXRET_OK;` |
|      33 | 2527 | `}` |
|       - | 2528 | `/*` |
|       - | 2529 | ` * Compile the var statement.` |
|       - | 2530 | ` * Symisc Extension:` |
|       - | 2531 | ` *      var statement can be used outside of a class definition.` |
|       - | 2532 | ` */` |
|       2 | 2533 | `PH7_PRIVATE sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 | 2534 | `{` |
|       - | 2535 | `` 	/* `var` is a PROPERTY modifier and nothing else. A class body's `var $x;` `` |
|       - | 2536 | `	 * is compiled by compile_class.c, never here, so reaching this statement` |
|       - | 2537 | ``	 * handler means `var` appeared outside a class — which php rejects as a`` |
|       - | 2538 | `	 * parse error. PHL used to compile it as an ordinary expression statement,` |
|       - | 2539 | ``	 * so top-level `var $x = 1;` silently ran (a Symisc extension). */`` |
|       3 | 2540 | `	PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       3 | 2541 | `	return SXERR_ABORT;` |
|       1 | 2542 | `}` |
|       - | 2543 | `/*` |
|       - | 2544 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - | 2545 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - | 2546 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - | 2547 | ` */` |
|       - | 2548 | `/*` |
|       - | 2549 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|       - | 2550 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - | 2551 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - | 2552 | ` * qualified name and updates the instruction's operand index.` |
|       - | 2553 | ` *` |
|       - | 2554 | ` * Resolution order:` |
|       - | 2555 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|       - | 2556 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|       - | 2557 | ` *   3. Otherwise return the original literal index unchanged.` |
|       - | 2558 | ` *` |
|       - | 2559 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|       - | 2560 | ` * came from an import (step 1) and 0 otherwise.` |
|       - | 2561 | ` * Returns the (possibly new) literal index.` |
|       - | 2562 | ` */` |
|  918534 | 2563 | `PH7_PRIVATE sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|       5 | 2564 | `{` |
|       - | 2565 | `	ph7_value *pLit;` |
|       - | 2566 | `	const char *zLit;` |
|       - | 2567 | `	SyString sQualified;` |
|       - | 2568 | `	sxu32 nLit;` |
|       - | 2569 | `	sxu32 k;` |
|       - | 2570 | `	sxu32 nNewIdx;` |
|       - | 2571 | `	int hasNsSep;` |
|       - | 2572 | `	SyHashEntry *pImport;` |
|       - | 2573 | `	ph7_value *pNew;` |
|  918539 | 2574 | `	if( pFromImport ){` |
|  813225 | 2575 | `		*pFromImport = 0;` |
|  406610 | 2576 | `	}` |
|  918539 | 2577 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  918539 | 2578 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 | 2579 | `		return nOrigIdx;` |
|       - | 2580 | `	}` |
|  918539 | 2581 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  918539 | 2582 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - | 2583 | `	/* Skip if already qualified (contains backslash) */` |
|  918539 | 2584 | `	hasNsSep = 0;` |
| 8528045 | 2585 | `	for( k = 0; k < nLit; k++ ){` |
| 7609873 | 2586 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 3804758 | 2587 | `	}` |
|  918539 | 2588 | `	if( hasNsSep ){` |
|     367 | 2589 | `		return nOrigIdx;` |
|       - | 2590 | `	}` |
|       - | 2591 | `	/* Check use imports first (works even outside namespaces) */` |
|  918177 | 2592 | `	SyBlobReset(&pGen->sWorker);` |
|  918177 | 2593 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  918177 | 2594 | `	if( pImport ){` |
|     186 | 2595 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|     186 | 2596 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|     186 | 2597 | `		if( pFromImport ){` |
|      34 | 2598 | `			*pFromImport = 1;` |
|      15 | 2599 | `		}` |
|      95 | 2600 | `	}else{` |
|  917995 | 2601 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  917775 | 2602 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|       - | 2603 | `		}` |
|       - | 2604 | `		/* Prepend current namespace */` |
|     225 | 2605 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     225 | 2606 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|     225 | 2607 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - | 2608 | `	}` |
|       - | 2609 | `	/* Look up or create a new literal for the qualified name */` |
|     407 | 2610 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|     407 | 2611 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|     221 | 2612 | `		return nNewIdx; /* Already interned */` |
|       - | 2613 | `	}` |
|     191 | 2614 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|     191 | 2615 | `	if( pNew == 0 ){` |
|     ! 0 | 2616 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - | 2617 | `	}` |
|     191 | 2618 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|     191 | 2619 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|     191 | 2620 | `	return nNewIdx;` |
|  459272 | 2621 | `}` |
|       - | 2622 | `/*` |
|       - | 2623 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - | 2624 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - | 2625 | ` */` |
|    6518 | 2626 | `PH7_PRIVATE void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 | 2627 | `{` |
|       - | 2628 | `	SyHashEntry *pImport;` |
|    6523 | 2629 | `	const char *zName = pName->zString;` |
|    6523 | 2630 | `	sxu32 nName = pName->nByte;` |
|    6523 | 2631 | `	sxu32 nFirst = 0;` |
|       - | 2632 | `	/* php resolves a name through use-imports on its LEADING segment: an` |
|       - | 2633 | ``	 * unqualified `C` maps the whole name (`use X\C;` -> X\C), while a QUALIFIED`` |
|       - | 2634 | ``	 * `A\B\C` maps just `A` (`use X\A;` -> X\A\B\C) and keeps the `\B\C` tail.`` |
|       - | 2635 | `	 * The old code looked up the whole qualified string (which never matches a` |
|       - | 2636 | `	 * single-segment import alias) and then blindly prefixed the current` |
|       - | 2637 | ``	 * namespace, so `use PHPUnit\Framework; extends Framework\TestCase` resolved`` |
|       - | 2638 | `	 * to Ns\Framework\TestCase. Mirror GenStateResolveNamespaceLiteral here. */` |
|   63795 | 2639 | `	while( nFirst < nName && zName[nFirst] != '\\' ){ nFirst++; }` |
|    6523 | 2640 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)zName,nFirst);` |
|    6523 | 2641 | `	if( pImport ){` |
|      72 | 2642 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      72 | 2643 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|      72 | 2644 | `		SyBlobAppend(pOut,zName + nFirst,nName - nFirst); /* the \B\C tail, if any */` |
|      72 | 2645 | `		return;` |
|       - | 2646 | `	}` |
|       - | 2647 | `	/* Prepend current namespace if active */` |
|    6455 | 2648 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      28 | 2649 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      28 | 2650 | `		SyBlobAppend(pOut,"\\",1);` |
|      12 | 2651 | `	}` |
|    6455 | 2652 | `	SyBlobAppend(pOut,zName,nName);` |
|    3264 | 2653 | `}` |
|       - | 2654 | `/*` |
|       - | 2655 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - | 2656 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - | 2657 | ` * The caller must release pOut when done.` |
|       - | 2658 | ` */` |
|    9124 | 2659 | `PH7_PRIVATE void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 | 2660 | `{` |
|    9129 | 2661 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     477 | 2662 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     477 | 2663 | `		SyBlobAppend(pOut,"\\",1);` |
|     236 | 2664 | `	}` |
|    9129 | 2665 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    9129 | 2666 | `}` |
|       - | 2667 | `/*` |
|       - | 2668 | `` * php's `namespace\X` NAME OPERATOR (5.3): a leading `namespace` keyword glued to`` |
|       - | 2669 | `` * a `\` names the CURRENT namespace, and the whole name is then FULLY QUALIFIED —`` |
|       - | 2670 | `` * `namespace\X` inside `namespace B;` is `\B\X`, and plain `\X` at global scope.`` |
|       - | 2671 | `` * php's lexer matches it as one token (T_NAME_RELATIVE, `"namespace"("\\"{LABEL})+`,`` |
|       - | 2672 | `` * case-insensitively), so the `\` must be GLUED to the keyword: `namespace \X` is a`` |
|       - | 2673 | ` * php parse error, and this mirrors that by comparing source offsets.` |
|       - | 2674 | ` *` |
|       - | 2675 | ` * This predicate only RECOGNIZES the operator (it consumes nothing), which is what` |
|       - | 2676 | `` * the statement dispatcher needs to tell `namespace\X::m();` from a namespace`` |
|       - | 2677 | ` * DECLARATION; GenStateNsRelPrefix below is what the name collectors call.` |
|       - | 2678 | ` */` |
| 2120960 | 2679 | `PH7_PRIVATE int GenStateIsNsRelName(SyToken *pIn,SyToken *pEnd)` |
|       5 | 2680 | `{` |
| 2120960 | 2681 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
| 1662691 | 2682 | `	 \|\| (sxu32)SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_NAMESPACE ){` |
| 2120659 | 2683 | `		return 0;` |
|       - | 2684 | `	}` |
|     311 | 2685 | `	if( &pIn[1] >= pEnd \|\| (pIn[1].nType & PH7_TK_NSSEP) == 0 ){` |
|     217 | 2686 | `		return 0;` |
|       - | 2687 | `	}` |
|      96 | 2688 | `	if( &pIn[2] >= pEnd \|\| (pIn[2].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 2689 | ``		return 0; /* php's T_NAME_RELATIVE needs at least one segment after the `\` */`` |
|       - | 2690 | `	}` |
|       - | 2691 | `	/* Glued? The tokenizer drops whitespace, so adjacency is the source offsets. */` |
|      96 | 2692 | `	return pIn->sData.zString + pIn->sData.nByte == pIn[1].sData.zString;` |
| 1060485 | 2693 | `}` |
|       - | 2694 | `/*` |
|       - | 2695 | `` * Consume a leading `namespace\` (see GenStateIsNsRelName) at *ppIn and seed pOut`` |
|       - | 2696 | ` * with the current namespace plus its separator — nothing at global scope, where` |
|       - | 2697 | ` * the bare name already IS the FQN. Returns TRUE when it fired, and the caller` |
|       - | 2698 | `` * must then treat the name it goes on to collect as ABSOLUTE: no `use` import may`` |
|       - | 2699 | ` * apply to it, and the current namespace is already in place.` |
|       - | 2700 | ` */` |
|   62604 | 2701 | `PH7_PRIVATE int GenStateNsRelPrefix(ph7_gen_state *pGen,SyToken **ppIn,SyToken *pEnd,SyBlob *pOut)` |
|       5 | 2702 | `{` |
|   62609 | 2703 | `	SyToken *pIn = *ppIn;` |
|   62609 | 2704 | `	if( !GenStateIsNsRelName(pIn,pEnd) ){` |
|   62521 | 2705 | `		return 0;` |
|       - | 2706 | `	}` |
|      90 | 2707 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      80 | 2708 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      80 | 2709 | `		SyBlobAppend(pOut,"\\",1);` |
|      39 | 2710 | `	}` |
|      90 | 2711 | `	*ppIn = &pIn[2];` |
|      90 | 2712 | `	return 1;` |
|   31307 | 2713 | `}` |
|       - | 2714 | `/*` |
|       - | 2715 | ` * Compile a namespace statement` |
|       - | 2716 | ` * According to the PHP language reference manual` |
|       - | 2717 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - | 2718 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - | 2719 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - | 2720 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - | 2721 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - | 2722 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - | 2723 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - | 2724 | ` *  programming world.` |
|       - | 2725 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - | 2726 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - | 2727 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - | 2728 | ` *  classes/functions/constants.` |
|       - | 2729 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - | 2730 | ` *  readability of source code.` |
|       - | 2731 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - | 2732 | ` *  Here is an example of namespace syntax in PHP:` |
|       - | 2733 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - | 2734 | ` *       class MyClass {}` |
|       - | 2735 | ` *       function myfunction() {}` |
|       - | 2736 | ` *       const MYCONST = 1;` |
|       - | 2737 | ` *       $a = new MyClass;` |
|       - | 2738 | ` *       $c = new \my\name\MyClass;` |
|       - | 2739 | ` *       $a = strlen('hi');` |
|       - | 2740 | ` *       $d = namespace\MYCONST;` |
|       - | 2741 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - | 2742 | ` *       echo constant($d);` |
|       - | 2743 | ` * NOTE` |
|       - | 2744 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - | 2745 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - | 2746 | ` */` |
|       - | 2747 | `/*` |
|       - | 2748 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - | 2749 | ` */` |
|      14 | 2750 | `PH7_PRIVATE const char * TokenTypeName(sxu32 nType)` |
|       4 | 2751 | `{` |
|      18 | 2752 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|      11 | 2753 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|      11 | 2754 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|      11 | 2755 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|      11 | 2756 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|      11 | 2757 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|       3 | 2758 | `	return "token";` |
|      11 | 2759 | `}` |
|     212 | 2760 | `PH7_PRIVATE sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       5 | 2761 | `{` |
|       - | 2762 | `	sxu32 nLine;` |
|       - | 2763 | `	sxi32 rc;` |
|     217 | 2764 | `	nLine = pGen->pIn->nLine;` |
|     217 | 2765 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - | 2766 | `	/* Reset namespace and clear previous use imports */` |
|     217 | 2767 | `	SyBlobReset(&pGen->sNamespace);` |
|     217 | 2768 | `	GenStateResetUseImports(&(*pGen),pGen->pVm);` |
|     217 | 2769 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 2770 | `		return SXRET_OK; /* Global namespace (bare "namespace;") */` |
|       - | 2771 | `	}` |
|     217 | 2772 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|     ! 0 | 2773 | `		return SXRET_OK; /* namespace; — switch to global namespace */` |
|       - | 2774 | `	}` |
|     217 | 2775 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|      10 | 2776 | `		return SXRET_OK; /* namespace { } — global namespace block */` |
|       - | 2777 | `	}` |
|       - | 2778 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     503 | 2779 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     299 | 2780 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - | 2781 | `			/* Append backslash separator */` |
|      50 | 2782 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      50 | 2783 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|      23 | 2784 | `			}` |
|      27 | 2785 | `		}else{` |
|       - | 2786 | `			/* Append identifier */` |
|     253 | 2787 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - | 2788 | `		}` |
|     299 | 2789 | `		pGen->pIn++;` |
|       5 | 2790 | `	}` |
|     209 | 2791 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       8 | 2792 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - | 2793 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 | 2794 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       6 | 2795 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2796 | `			return SXERR_ABORT;` |
|       - | 2797 | `		}` |
|       2 | 2798 | `	}` |
|     209 | 2799 | `	return SXRET_OK;` |
|     111 | 2800 | `}` |
|       - | 2801 | `/*` |
|       - | 2802 | ` * Initialize the three use-import tables of a code generator.` |
|       - | 2803 | ` *` |
|       - | 2804 | ` * php resolves CLASS and FUNCTION imports case-INSENSITIVELY, like every other` |
|       - | 2805 | ``  * name in those two families: `use A\Cee;` then `CEE::K`, `use A\Cee as Alias;` `` |
|       - | 2806 | `` * then `ALIAS::K`, `use function A\eff;` then `EFF()`, and a wrong-case leading`` |
|       - | 2807 | `` * segment of an imported namespace (`use A\B;` then `b\Cee::K`) all resolve.`` |
|       - | 2808 | ` * So both tables fold through SyStrHash/SyStrnmicmp, exactly like hClass /` |
|       - | 2809 | ` * hMethod / hFunction.` |
|       - | 2810 | ` *` |
|       - | 2811 | ` * The CONST table stays BYTE-EXACT: php keeps constant names case-sensitive,` |
|       - | 2812 | `` * so `use const A\KAY;` followed by `kay` must remain an undefined constant.`` |
|       - | 2813 | ` * That asymmetry is why the three tables exist separately.` |
|       - | 2814 | ` */` |
|   43066 | 2815 | `PH7_PRIVATE void GenStateInitUseImports(ph7_gen_state *pGen,ph7_vm *pVm)` |
|       5 | 2816 | `{` |
|   43071 | 2817 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|   43071 | 2818 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|   43071 | 2819 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|   43071 | 2820 | `}` |
|       - | 2821 | `/*` |
|       - | 2822 | ` * Drop every import currently in scope and start a fresh set (a namespace` |
|       - | 2823 | ` * switch clears imports).  Keeps the case rules of GenStateInitUseImports.` |
|       - | 2824 | ` */` |
|   37322 | 2825 | `PH7_PRIVATE void GenStateResetUseImports(ph7_gen_state *pGen,ph7_vm *pVm)` |
|       5 | 2826 | `{` |
|   37327 | 2827 | `	SyHashRelease(&pGen->hUseImports);` |
|   37327 | 2828 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   37327 | 2829 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   37327 | 2830 | `	GenStateInitUseImports(&(*pGen),&(*pVm));` |
|   37327 | 2831 | `}` |
|       - | 2832 | `/*` |
|       - | 2833 | ` * The two DECLARED-name tables (classes and functions declared so far in this` |
|       - | 2834 | ` * compile unit). php refuses an import whose name a declaration already took, and` |
|       - | 2835 | ` * the check is per COMPILE UNIT and case-INSENSITIVE — a class declared by a file` |
|       - | 2836 | `` * this one later `require`s is invisible to it, because that file compiles after`` |
|       - | 2837 | ` * this one has finished. Both tables key on the FQN, so they survive a namespace` |
|       - | 2838 | ` * switch (which clears only the imports).` |
|       - | 2839 | ` */` |
|   42854 | 2840 | `PH7_PRIVATE void GenStateInitSeenSymbols(ph7_gen_state *pGen,ph7_vm *pVm)` |
|       5 | 2841 | `{` |
|   42859 | 2842 | `	SyHashInit(&pGen->hSeenClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|   42859 | 2843 | `	SyHashInit(&pGen->hSeenFunc,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|   42859 | 2844 | `}` |
|   37114 | 2845 | `PH7_PRIVATE void GenStateReleaseSeenSymbols(ph7_gen_state *pGen)` |
|       5 | 2846 | `{` |
|   37119 | 2847 | `	SyHashRelease(&pGen->hSeenClass);` |
|   37119 | 2848 | `	SyHashRelease(&pGen->hSeenFunc);` |
|   37119 | 2849 | `}` |
|   37110 | 2850 | `PH7_PRIVATE void GenStateResetSeenSymbols(ph7_gen_state *pGen,ph7_vm *pVm)` |
|       5 | 2851 | `{` |
|   37115 | 2852 | `	GenStateReleaseSeenSymbols(&(*pGen));` |
|   37115 | 2853 | `	GenStateInitSeenSymbols(&(*pGen),&(*pVm));` |
|   37115 | 2854 | `}` |
|       - | 2855 | `/*` |
|       - | 2856 | ` * Record one declared CLASS (bFunc = 0) or FUNCTION (bFunc = 1) FQN so a later` |
|       - | 2857 | `` * `use` in this compile unit can see that the name is taken.`` |
|       - | 2858 | ` */` |
|  162552 | 2859 | `PH7_PRIVATE void GenStateRecordDeclaredName(ph7_gen_state *pGen,int bFunc,const SyString *pFqn)` |
|       5 | 2860 | `{` |
|  162557 | 2861 | `	SyHash *pHash = bFunc ? &pGen->hSeenFunc : &pGen->hSeenClass;` |
|       - | 2862 | `	char *zDup;` |
|  162557 | 2863 | `	if( pFqn->nByte < 1 \|\| SyHashGet(pHash,pFqn->zString,pFqn->nByte) != 0 ){` |
|      23 | 2864 | `		return;` |
|       - | 2865 | `	}` |
|  162539 | 2866 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pFqn->zString,pFqn->nByte);` |
|  162539 | 2867 | `	if( zDup ){` |
|       - | 2868 | `		/* The blob the caller built is released on its way out, so the table owns` |
|       - | 2869 | `		 * a pool copy (freed in bulk with the VM, like the import FQNs). */` |
|  162539 | 2870 | `		SyHashInsert(pHash,zDup,pFqn->nByte,zDup);` |
|   81267 | 2871 | `	}` |
|   81281 | 2872 | `}` |
|       - | 2873 | `/*` |
|       - | 2874 | `` * php refuses a DECLARATION whose short name a local `use` import already took:`` |
|       - | 2875 | ` *` |
|       - | 2876 | ` *   use A\Cee;  class Cee {}    Cannot redeclare class B\Cee (previously declared as local import)` |
|       - | 2877 | ` *   use function A\eff;  function eff(){}` |
|       - | 2878 | ` *                               Cannot redeclare function B\eff() (previously declared as local import)` |
|       - | 2879 | ` *   use const A\KAY;  const KAY = 1;` |
|       - | 2880 | ` *                               Cannot declare const B\KAY because the name is already in use` |
|       - | 2881 | ` *` |
|       - | 2882 | `` * A SELF-import (`use B\Cee;` inside `namespace B;`) names this very declaration`` |
|       - | 2883 | ` * and is a no-op, so it is exempt. iKind: 0 = class family (interface/trait/enum` |
|       - | 2884 | ` * included — php says "class" for all four), 1 = function, 2 = const.` |
|       - | 2885 | ` */` |
|  162716 | 2886 | `PH7_PRIVATE sxi32 GenStateGuardImportRedeclare(ph7_gen_state *pGen,int iKind,` |
|       - | 2887 | `	const SyString *pShort,const SyString *pFqn,sxu32 nLine)` |
|       5 | 2888 | `{` |
|       - | 2889 | `	SyHash *pImports;` |
|       - | 2890 | `	SyHashEntry *pEntry;` |
|       - | 2891 | `	const char *zImported;` |
|       - | 2892 | `	sxu32 nImported;` |
|  162721 | 2893 | `	switch( iKind ){` |
|  158153 | 2894 | `		case 1:  pImports = &pGen->hUseFuncImports; break;` |
|     169 | 2895 | `		case 2:  pImports = &pGen->hUseConstImports; break;` |
|    4409 | 2896 | `		default: pImports = &pGen->hUseImports; break;` |
|       - | 2897 | `	}` |
|  162721 | 2898 | `	pEntry = SyHashGet(pImports,(const void *)pShort->zString,pShort->nByte);` |
|  162721 | 2899 | `	if( pEntry == 0 ){` |
|  162707 | 2900 | `		return SXRET_OK;` |
|       - | 2901 | `	}` |
|      18 | 2902 | `	zImported = (const char *)pEntry->pUserData;` |
|      18 | 2903 | `	nImported = SyStrlen(zImported);` |
|      14 | 2904 | `	if( nImported == pFqn->nByte` |
|      22 | 2905 | `	 && (iKind == 2 ? SyMemcmp((const void *)zImported,(const void *)pFqn->zString,nImported) == 0` |
|       8 | 2906 | `	                : SyStrnicmp(zImported,pFqn->zString,nImported) == 0) ){` |
|       7 | 2907 | `		return SXRET_OK; /* the import IS this declaration */` |
|       - | 2908 | `	}` |
|      13 | 2909 | `	if( iKind == 2 ){` |
|       4 | 2910 | `		return PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       1 | 2911 | `			"Cannot declare const %z because the name is already in use",pFqn);` |
|       - | 2912 | `	}` |
|       8 | 2913 | `	return PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       2 | 2914 | `		iKind == 1 ? "Cannot redeclare function %z() (previously declared as local import)"` |
|       2 | 2915 | `		           : "Cannot redeclare class %z (previously declared as local import)",pFqn);` |
|   81361 | 2916 | `}` |
|       - | 2917 | `/*` |
|       - | 2918 | `` * Register one resolved `use` import: alias -> FQN, in the table its KIND owns`` |
|       - | 2919 | ` * (iUseType: 0 = class, 1 = function, 2 = const).  Shared by the plain form` |
|       - | 2920 | `` * (`use A\Cee;`) and by each member of a group (`use A\{Cee, Dee};`).`` |
|       - | 2921 | ` */` |
|     162 | 2922 | `static sxi32 GenStateAddImport(` |
|       - | 2923 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 2924 | `	int iUseType,         /* 0=class, 1=function, 2=const */` |
|       - | 2925 | `	SyBlob *pPath,        /* Fully qualified name being imported */` |
|       - | 2926 | `	SyString *pAlias,     /* Short name it is imported under */` |
|       - | 2927 | `	sxu32 nLine           /* Line of the 'use' keyword (for diagnostics) */` |
|       - | 2928 | `	)` |
|       5 | 2929 | `{` |
|       - | 2930 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|       - | 2931 | `	const char *zKind;  /* php's kind word in the "already in use" message */` |
|       - | 2932 | `	char *zDup;` |
|       - | 2933 | `	sxi32 rc;` |
|       - | 2934 | `	/* Select the target hash table based on import type. */` |
|     167 | 2935 | `	switch( iUseType ){` |
|      36 | 2936 | `		case 1:  pGenHash = &pGen->hUseFuncImports; break;` |
|      37 | 2937 | `		case 2:  pGenHash = &pGen->hUseConstImports; break;` |
|     103 | 2938 | `		default: pGenHash = &pGen->hUseImports; break;` |
|       - | 2939 | `	}` |
|       - | 2940 | `	/* php names the KIND of a non-class import in this message: "Cannot use` |
|       - | 2941 | `	 * function A\eff as eff …" / "Cannot use const A\KAY as KAY …". */` |
|     167 | 2942 | `	zKind = iUseType == 1 ? "function " : iUseType == 2 ? "const " : "";` |
|       - | 2943 | `	/* Check for duplicate import alias (per-type) */` |
|     167 | 2944 | `	if( SyHashGet(pGenHash,pAlias->zString,pAlias->nByte) != 0 ){` |
|      12 | 2945 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 2946 | `			"Cannot use %s%.*s as %z because the name is already in use",` |
|       6 | 2947 | `			zKind,(int)SyBlobLength(pPath),(const char *)SyBlobData(pPath),pAlias);` |
|       9 | 2948 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2949 | `			return SXERR_ABORT;` |
|       - | 2950 | `		}` |
|       3 | 2951 | `	}` |
|       - | 2952 | `	/* …and refuses one whose name a DECLARATION in this compile unit already took` |
|       - | 2953 | ``	 * (`class Cee {} use A\Cee;`), unless the import names that very declaration.`` |
|       - | 2954 | `	 * The name an import occupies is the alias in the CURRENT namespace, which is` |
|       - | 2955 | `	 * what the seen tables key on. php runs this check for classes and functions` |
|       - | 2956 | ``	 * only — a `const` declaration followed by its own `use const` is accepted. */`` |
|     167 | 2957 | `	if( iUseType != 2 ){` |
|       - | 2958 | `		SyBlob sTaken;` |
|     135 | 2959 | `		SyBlobInit(&sTaken,&pGen->pVm->sAllocator);` |
|     135 | 2960 | `		GenStateBuildFQN(&(*pGen),pAlias,&sTaken);` |
|     130 | 2961 | `		if( SyHashGet(iUseType == 1 ? &pGen->hSeenFunc : &pGen->hSeenClass,` |
|     195 | 2962 | `				SyBlobData(&sTaken),SyBlobLength(&sTaken)) != 0` |
|      72 | 2963 | `		 && (SyBlobLength(&sTaken) != SyBlobLength(pPath)` |
|       4 | 2964 | `			\|\| SyStrnicmp((const char *)SyBlobData(&sTaken),(const char *)SyBlobData(pPath),` |
|       6 | 2965 | `				(sxu32)SyBlobLength(&sTaken)) != 0) ){` |
|       8 | 2966 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 2967 | `				"Cannot use %s%.*s as %z because the name is already in use",` |
|       4 | 2968 | `				zKind,(int)SyBlobLength(pPath),(const char *)SyBlobData(pPath),pAlias);` |
|       6 | 2969 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2970 | `				SyBlobRelease(&sTaken);` |
|     ! 0 | 2971 | `				return SXERR_ABORT;` |
|       - | 2972 | `			}` |
|       2 | 2973 | `		}` |
|     135 | 2974 | `		SyBlobRelease(&sTaken);` |
|      65 | 2975 | `	}` |
|       - | 2976 | `	/* Register the import: alias -> FQN.` |
|       - | 2977 | `	 * Strings are allocated from the VM pool allocator and freed` |
|       - | 2978 | `	 * when the entire VM is released. SyHashRelease does not free` |
|       - | 2979 | `	 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|     248 | 2980 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     162 | 2981 | `		(const char *)SyBlobData(pPath),SyBlobLength(pPath));` |
|     167 | 2982 | `	if( zDup ){` |
|       - | 2983 | `		/* All three kinds resolve entirely at COMPILE time — a const import is read` |
|       - | 2984 | `		 * by the OP_LOADC candidate builder (compile_node.c), so no runtime table` |
|       - | 2985 | `		 * is needed for it either. */` |
|     167 | 2986 | `		SyHashInsert(pGenHash,pAlias->zString,pAlias->nByte,zDup);` |
|      81 | 2987 | `	}` |
|     167 | 2988 | `	return SXRET_OK;` |
|      86 | 2989 | `}` |
|       - | 2990 | `/*` |
|       - | 2991 | `` * Collect one `\`-separated name into pOut (appending to whatever it holds, with`` |
|       - | 2992 | ` * a separator when needed) and return its LAST segment token, or 0 when the` |
|       - | 2993 | ` * cursor is not on a name at all.` |
|       - | 2994 | ` */` |
|     176 | 2995 | `static SyToken * GenStateCollectNsPath(ph7_gen_state *pGen,SyBlob *pOut)` |
|       5 | 2996 | `{` |
|     181 | 2997 | `	SyToken *pLast = 0;` |
|     627 | 2998 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|     451 | 2999 | `		if( pGen->pIn->nType & PH7_TK_ID ){` |
|     309 | 3000 | `			pLast = pGen->pIn;` |
|     309 | 3001 | `			if( SyBlobLength(pOut) > 0 ){` |
|     159 | 3002 | `				SyBlobAppend(pOut,"\\",1);` |
|      77 | 3003 | `			}` |
|     309 | 3004 | `			SyBlobAppend(pOut,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|     152 | 3005 | `		}` |
|     451 | 3006 | `		pGen->pIn++;` |
|       5 | 3007 | `	}` |
|     181 | 3008 | `	return pLast;` |
|       5 | 3009 | `}` |
|       - | 3010 | `/*` |
|       - | 3011 | `` * Consume the optional `as Alias` clause, leaving *pAlias untouched when absent.`` |
|       - | 3012 | ` */` |
|     162 | 3013 | `static void GenStateCollectImportAlias(ph7_gen_state *pGen,SyString *pAlias)` |
|       5 | 3014 | `{` |
|     162 | 3015 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     105 | 3016 | `		&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|      41 | 3017 | `		pGen->pIn++; /* Jump 'as' */` |
|      41 | 3018 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|      41 | 3019 | `			*pAlias = pGen->pIn->sData;` |
|      41 | 3020 | `			pGen->pIn++;` |
|      19 | 3021 | `		}` |
|      19 | 3022 | `	}` |
|     167 | 3023 | `}` |
|       - | 3024 | `/*` |
|       - | 3025 | ` * Compile the members of a GROUP use declaration (php 7.0):` |
|       - | 3026 | ` *` |
|       - | 3027 | ` *      use A\{Cee, Dee as D2, Sub\Eee};` |
|       - | 3028 | ` *      use function A\{f, g as h};` |
|       - | 3029 | ` *      use A\{function f, const K, Cee};   // per-member kind, untyped group only` |
|       - | 3030 | ` *` |
|       - | 3031 | `` * pPrefix holds the path before the brace; the cursor sits on `{`.  Each member`` |
|       - | 3032 | `` * is the prefix, a `\`, and the member's own (possibly multi-segment) name.  A`` |
|       - | 3033 | ` * trailing comma is allowed, an empty group is not.` |
|       - | 3034 | ` */` |
|      10 | 3035 | `static sxi32 GenStateCompileGroupUse(ph7_gen_state *pGen,SyBlob *pPrefix,int iUseType,sxu32 nLine)` |
|       1 | 3036 | `{` |
|       - | 3037 | `	SyBlob sPath;` |
|      11 | 3038 | `	sxi32 rc = SXRET_OK;` |
|      11 | 3039 | `	pGen->pIn++; /* Jump '{' */` |
|      11 | 3040 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|      11 | 3041 | `	for(;;){` |
|      23 | 3042 | `		int iMemberType = iUseType;` |
|       - | 3043 | `		SyString sAlias;` |
|       - | 3044 | `		SyToken *pLast;` |
|       - | 3045 | ``		/* `function`/`const` may qualify a single member, but only inside a`` |
|       - | 3046 | ``		 * group that is not itself typed (php rejects `use function A\{const C}`). */`` |
|      23 | 3047 | `		if( iUseType == 0 && pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       5 | 3048 | `			sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|       5 | 3049 | `			if( nKey == PH7_TKWRD_FUNCTION ){` |
|       3 | 3050 | `				iMemberType = 1;` |
|       3 | 3051 | `				pGen->pIn++;` |
|       4 | 3052 | `			}else if( nKey == PH7_TKWRD_CONST ){` |
|       3 | 3053 | `				iMemberType = 2;` |
|       3 | 3054 | `				pGen->pIn++;` |
|       1 | 3055 | `			}` |
|       2 | 3056 | `		}` |
|      23 | 3057 | `		SyBlobReset(&sPath);` |
|      23 | 3058 | `		SyBlobAppend(&sPath,SyBlobData(pPrefix),SyBlobLength(pPrefix));` |
|      23 | 3059 | `		pLast = GenStateCollectNsPath(pGen,&sPath);` |
|      23 | 3060 | `		if( pLast == 0 ){` |
|       - | 3061 | ``			/* No member name: `use A\{};` or a stray token.  Report once, then`` |
|       - | 3062 | `			 * skip to the end of the group so the statement does not cascade. */` |
|     ! 0 | 3063 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - | 3064 | `				"syntax error, unexpected %s \"%z\", expecting identifier",` |
|     ! 0 | 3065 | `				TokenTypeName(pGen->pIn < pGen->pEnd ? pGen->pIn->nType : 0),` |
|     ! 0 | 3066 | `				pGen->pIn < pGen->pEnd ? &pGen->pIn->sData : 0);` |
|     ! 0 | 3067 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_CCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 | 3068 | `				pGen->pIn++;` |
|     ! 0 | 3069 | `			}` |
|     ! 0 | 3070 | `			break;` |
|       - | 3071 | `		}` |
|      23 | 3072 | `		sAlias = pLast->sData; /* Default alias is the member's last component */` |
|      23 | 3073 | `		GenStateCollectImportAlias(pGen,&sAlias);` |
|      23 | 3074 | `		rc = GenStateAddImport(&(*pGen),iMemberType,&sPath,&sAlias,nLine);` |
|      23 | 3075 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3076 | `			break;` |
|       - | 3077 | `		}` |
|      23 | 3078 | `		rc = SXRET_OK;` |
|      23 | 3079 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|      15 | 3080 | `			pGen->pIn++;` |
|      15 | 3081 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) ){` |
|       3 | 3082 | `				break; /* Trailing comma before the closing brace */` |
|       - | 3083 | `			}` |
|      13 | 3084 | `			continue;` |
|       - | 3085 | `		}` |
|       9 | 3086 | `		break;` |
|     ! 0 | 3087 | `	}` |
|      11 | 3088 | `	SyBlobRelease(&sPath);` |
|      11 | 3089 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 3090 | `		return SXERR_ABORT;` |
|       - | 3091 | `	}` |
|      11 | 3092 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) ){` |
|      11 | 3093 | `		pGen->pIn++; /* Jump '}' */` |
|       5 | 3094 | `	}` |
|      11 | 3095 | `	return SXRET_OK;` |
|       6 | 3096 | `}` |
|       - | 3097 | `/*` |
|       - | 3098 | ` * Compile the 'use' statement` |
|       - | 3099 | ` * According to the PHP language reference manual` |
|       - | 3100 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - | 3101 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - | 3102 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - | 3103 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - | 3104 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - | 3105 | ` *  a function or constant is not supported.` |
|       - | 3106 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - | 3107 | ` * NOTE` |
|       - | 3108 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - | 3109 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - | 3110 | ` */` |
|     152 | 3111 | `PH7_PRIVATE sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       5 | 3112 | `{` |
|       - | 3113 | `	sxu32 nLine;` |
|       - | 3114 | `	sxi32 rc;` |
|       - | 3115 | `	SyBlob sPath;` |
|       - | 3116 | `	SyString sAlias;` |
|       - | 3117 | `	SyToken *pLast;` |
|       - | 3118 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|     157 | 3119 | `	nLine = pGen->pIn->nLine;` |
|     157 | 3120 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|       - | 3121 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|     157 | 3122 | `	iUseType = 0;` |
|     157 | 3123 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      61 | 3124 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|      61 | 3125 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|      32 | 3126 | `			iUseType = 1;` |
|      32 | 3127 | `			pGen->pIn++;` |
|      47 | 3128 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|      33 | 3129 | `			iUseType = 2;` |
|      33 | 3130 | `			pGen->pIn++;` |
|      14 | 3131 | `		}` |
|      28 | 3132 | `	}` |
|     157 | 3133 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - | 3134 | `	/* Process one or more use declarations separated by commas */` |
|      77 | 3135 | `	for(;;){` |
|     159 | 3136 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 3137 | `			break;` |
|       - | 3138 | `		}` |
|     159 | 3139 | `		SyBlobReset(&sPath);` |
|       - | 3140 | `		/* Collect the full namespace path */` |
|     159 | 3141 | `		pLast = GenStateCollectNsPath(pGen,&sPath);` |
|     159 | 3142 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) && SyBlobLength(&sPath) > 0 ){` |
|       - | 3143 | `			/* GROUP declaration: what was collected is the shared prefix.  php` |
|       - | 3144 | `			 * does not let a group be comma-combined with another declaration,` |
|       - | 3145 | `			 * so the members close the statement. */` |
|      11 | 3146 | `			rc = GenStateCompileGroupUse(&(*pGen),&sPath,iUseType,nLine);` |
|      11 | 3147 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3148 | `				SyBlobRelease(&sPath);` |
|     ! 0 | 3149 | `				return SXERR_ABORT;` |
|       - | 3150 | `			}` |
|      11 | 3151 | `			break;` |
|       - | 3152 | `		}` |
|     149 | 3153 | `		if( pLast == 0 ){` |
|       - | 3154 | `			/* Empty path */` |
|       6 | 3155 | `			break;` |
|       - | 3156 | `		}` |
|       - | 3157 | `		/* Default alias is the last component of the path */` |
|     145 | 3158 | `		sAlias = pLast->sData;` |
|       - | 3159 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|     145 | 3160 | `		GenStateCollectImportAlias(pGen,&sAlias);` |
|     145 | 3161 | `		rc = GenStateAddImport(&(*pGen),iUseType,&sPath,&sAlias,nLine);` |
|     145 | 3162 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3163 | `			SyBlobRelease(&sPath);` |
|     ! 0 | 3164 | `			return SXERR_ABORT;` |
|       - | 3165 | `		}` |
|       - | 3166 | `		/* Check for comma (multiple use declarations) */` |
|     145 | 3167 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 | 3168 | `			pGen->pIn++;` |
|       2 | 3169 | `		}else{` |
|      74 | 3170 | `			break;` |
|       - | 3171 | `		}` |
|       1 | 3172 | `	}` |
|     157 | 3173 | `	SyBlobRelease(&sPath);` |
|     157 | 3174 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 | 3175 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 | 3176 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 | 3177 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3178 | `			return SXERR_ABORT;` |
|       - | 3179 | `		}` |
|       1 | 3180 | `	}` |
|     157 | 3181 | `	return SXRET_OK;` |
|      81 | 3182 | `}` |
|       - | 3183 | `/*` |
|       - | 3184 | ` * Compile the stupid 'declare' language construct.` |
|       - | 3185 | ` *` |
|       - | 3186 | ` * According to the PHP language reference manual.` |
|       - | 3187 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - | 3188 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - | 3189 | ` *  declare (directive)` |
|       - | 3190 | ` *   statement` |
|       - | 3191 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - | 3192 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - | 3193 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - | 3194 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - | 3195 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - | 3196 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - | 3197 | ` * <?php` |
|       - | 3198 | ` * // these are the same:` |
|       - | 3199 | ` * // you can use this:` |
|       - | 3200 | ` * declare(ticks=1) {` |
|       - | 3201 | ` *   // entire script here` |
|       - | 3202 | ` * }` |
|       - | 3203 | ` * // or you can use this:` |
|       - | 3204 | ` * declare(ticks=1);` |
|       - | 3205 | ` * // entire script here` |
|       - | 3206 | ` * ?>` |
|       - | 3207 | ` *` |
|       - | 3208 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - | 3209 | ` */` |
|       - | 3210 | `/*` |
|       - | 3211 | ` * Match a directive name against a known literal (case-insensitive).` |
|       - | 3212 | ` */` |
|     104 | 3213 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|       5 | 3214 | `{` |
|     156 | 3215 | `	return SyStringLength(pName) == nWant` |
|     104 | 3216 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|       5 | 3217 | `}` |
|       - | 3218 |  |
|      56 | 3219 | `PH7_PRIVATE sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       5 | 3220 | `{` |
|      61 | 3221 | `	sxu32 nLine = pGen->pIn->nLine;` |
|      61 | 3222 | `	SyToken *pBodyEnd = 0;` |
|       - | 3223 | `	SyToken *pBodyStart;` |
|       - | 3224 | `	SyToken *pCursor;` |
|       - | 3225 | `	int bHasStrictTypes;` |
|       - | 3226 | `	int bBlockForm;` |
|       - | 3227 | `	int bPlacementOk;` |
|       - | 3228 | `	sxi32 rc;` |
|      61 | 3229 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|      61 | 3230 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       6 | 3231 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|       6 | 3232 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3233 | `			return SXERR_ABORT;` |
|       - | 3234 | `		}` |
|       6 | 3235 | `		goto Synchro;` |
|       - | 3236 | `	}` |
|      57 | 3237 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|      57 | 3238 | `	pBodyStart = pGen->pIn;` |
|       - | 3239 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|      57 | 3240 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|      57 | 3241 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 | 3242 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\")\"");` |
|     ! 0 | 3243 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3244 | `			return SXERR_ABORT;` |
|       - | 3245 | `		}` |
|     ! 0 | 3246 | `		return SXRET_OK;` |
|       - | 3247 | `	}` |
|       - | 3248 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|       - | 3249 | `	 * now delimits the comma-separated directive list. */` |
|      57 | 3250 | `	pGen->pIn = &pBodyEnd[1];` |
|      57 | 3251 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|     ! 0 | 3252 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 | 3253 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3254 | `			return SXERR_ABORT;` |
|       - | 3255 | `		}` |
|     ! 0 | 3256 | `	}` |
|      57 | 3257 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|      57 | 3258 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|      57 | 3259 | `	bHasStrictTypes = 0;` |
|       - | 3260 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|       - | 3261 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|       - | 3262 | `	 * directive appears anywhere in the list, before validating values. */` |
|      57 | 3263 | `	pCursor = pBodyStart;` |
|      69 | 3264 | `	while( pCursor < pBodyEnd ){` |
|      65 | 3265 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      57 | 3266 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|      53 | 3267 | `				bHasStrictTypes = 1;` |
|      53 | 3268 | `				break;` |
|       - | 3269 | `			}` |
|       2 | 3270 | `		}` |
|      14 | 3271 | `		pCursor++;` |
|       2 | 3272 | `	}` |
|      57 | 3273 | `	if( bHasStrictTypes && bBlockForm ){` |
|       3 | 3274 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3275 | `			"strict_types declaration must not use block mode");` |
|       3 | 3276 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 | 3277 | `		return SXRET_OK;` |
|       - | 3278 | `	}` |
|      55 | 3279 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|       6 | 3280 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3281 | `			"strict_types declaration must be the very first statement in the script");` |
|       6 | 3282 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       6 | 3283 | `		return SXRET_OK;` |
|       - | 3284 | `	}` |
|       - | 3285 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|      51 | 3286 | `	pCursor = pBodyStart;` |
|      97 | 3287 | `	while( pCursor < pBodyEnd ){` |
|       - | 3288 | `		SyToken *pNameTok;` |
|       - | 3289 | `		SyToken *pEqTok;` |
|       - | 3290 | `		SyToken *pValTok;` |
|       - | 3291 | `		SyString *pDirName;` |
|       - | 3292 | `		int bIsStrict;` |
|       - | 3293 | `		int iStrictValue;` |
|      53 | 3294 | `		pNameTok = pCursor;` |
|      53 | 3295 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 3296 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3297 | `				"declare: Expecting a directive name");` |
|     ! 0 | 3298 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 | 3299 | `			return SXRET_OK;` |
|       - | 3300 | `		}` |
|      53 | 3301 | `		pEqTok = pNameTok + 1;` |
|      53 | 3302 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|     ! 0 | 3303 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3304 | `				"declare: Expecting '=' after directive name");` |
|     ! 0 | 3305 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 | 3306 | `			return SXRET_OK;` |
|       - | 3307 | `		}` |
|      53 | 3308 | `		pValTok = pEqTok + 1;` |
|      53 | 3309 | `		if( pValTok >= pBodyEnd ){` |
|     ! 0 | 3310 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3311 | `				"declare: Expecting value after '='");` |
|     ! 0 | 3312 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 | 3313 | `			return SXRET_OK;` |
|       - | 3314 | `		}` |
|      53 | 3315 | `		pDirName = &pNameTok->sData;` |
|      53 | 3316 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|      53 | 3317 | `		if( bIsStrict ){` |
|       - | 3318 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|       - | 3319 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|      49 | 3320 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|     ! 0 | 3321 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3322 | `					"declare(strict_types) value must be a literal");` |
|     ! 0 | 3323 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 | 3324 | `				return SXRET_OK;` |
|       - | 3325 | `			}` |
|      49 | 3326 | `			iStrictValue = -1;` |
|      49 | 3327 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|      49 | 3328 | `				const char *zv = SyStringData(&pValTok->sData);` |
|      49 | 3329 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|      49 | 3330 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|      47 | 3331 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|      22 | 3332 | `			}` |
|      49 | 3333 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|       3 | 3334 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3335 | `					"strict_types declaration must have 0 or 1 as its value");` |
|       3 | 3336 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 | 3337 | `				return SXRET_OK;` |
|       - | 3338 | `			}` |
|      46 | 3339 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|      27 | 3340 | `		}else if( DeclareNameIs(pDirName, "encoding", sizeof("encoding")-1) ){` |
|       - | 3341 | `			/* php always ignores declare(encoding=...) unless it was built with` |
|       - | 3342 | `			 * Zend multibyte, and says so in these exact words. */` |
|       3 | 3343 | `			PH7_GenCompileError(&(*pGen),E_WARNING,nLine,` |
|       - | 3344 | `				"declare(encoding=...) ignored because Zend multibyte feature is turned off by settings");` |
|       1 | 3345 | `		}else{` |
|       - | 3346 | `			/* Other directives (ticks and friends) are accepted as no-ops.` |
|       - | 3347 | `			 * This used to emit a NOTICE naming the upstream engine and its` |
|       - | 3348 | `			 * version ("the declare construct is a no-op in the current release` |
|       - | 3349 | `			 * of the PH7(2.1.4) engine") — php prints nothing at all for` |
|       - | 3350 | ``			 * `declare(ticks=1)`, and leaking the old engine's branding into`` |
|       - | 3351 | `			 * user-visible diagnostics was wrong on its own. */` |
|       - | 3352 | `		}` |
|      51 | 3353 | `		pCursor = pValTok + 1;` |
|       - | 3354 | `		/* Consume separating comma (or end). */` |
|      51 | 3355 | `		if( pCursor < pBodyEnd ){` |
|       3 | 3356 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 | 3357 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3358 | `					"declare: Expecting ',' or ')' after directive value");` |
|     ! 0 | 3359 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 | 3360 | `				return SXRET_OK;` |
|       - | 3361 | `			}` |
|       3 | 3362 | `			pCursor++;` |
|       1 | 3363 | `		}` |
|       5 | 3364 | `	}` |
|       - | 3365 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|       - | 3366 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|       - | 3367 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|      49 | 3368 | `	return SXRET_OK;` |
|       2 | 3369 | `Synchro:` |
|       - | 3370 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      16 | 3371 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      12 | 3372 | `		pGen->pIn++;` |
|       2 | 3373 | `	}` |
|       6 | 3374 | `	return SXRET_OK;` |
|      33 | 3375 | `}` |
|       - | 3376 | `/*` |
|       - | 3377 | ` * Compile a class constant.` |
|       - | 3378 | ` * According to the PHP language reference manual` |
|       - | 3379 | ` *  Class Constants` |
|       - | 3380 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - | 3381 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - | 3382 | ` *   you don't use the $ symbol to declare or use them.` |
|       - | 3383 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - | 3384 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - | 3385 | ` *   It's also possible for interfaces to have constants.` |
|       - | 3386 | ` * Symisc eXtension.` |
|       - | 3387 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - | 3388 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 3389 | ` *  Example:` |
|       - | 3390 | ` *   class Test{` |
|       - | 3391 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 3392 | ` *   };` |
|       - | 3393 | ` *   var_dump(TEST::MyConst);` |
|       - | 3394 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 3395 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 3396 | ` */` |
|       - | 3397 | `/*` |
|       - | 3398 | ` * Exception handling.` |
|       - | 3399 | ` *  According to the PHP language reference manual` |
|       - | 3400 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - | 3401 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - | 3402 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - | 3403 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - | 3404 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - | 3405 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - | 3406 | ` *    (or re-thrown) within a catch block.` |
|       - | 3407 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - | 3408 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - | 3409 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - | 3410 | ` *    been defined with set_exception_handler().` |
|       - | 3411 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - | 3412 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - | 3413 | ` */` |
|       - | 3414 | `/*` |
|       - | 3415 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - | 3416 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - | 3417 | ` * indicates failure.` |
|       - | 3418 | ` */` |
|   69574 | 3419 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 | 3420 | `{` |
|       - | 3421 | ``	/* php decides throwability at RUNTIME: `throw 5` compiles and raises the`` |
|       - | 3422 | `	 * catchable Error "Can only throw objects" when it executes, and a` |
|       - | 3423 | `	 * non-Throwable object raises "Cannot throw objects that do not implement` |
|       - | 3424 | `	 * Throwable". This used to whitelist a handful of expression shapes and` |
|       - | 3425 | `	 * reject the rest at compile time as a "friendlier" error, which meant the` |
|       - | 3426 | `	 * program never ran and no catch could ever see it — and it was inconsistent` |
|       - | 3427 | ``	 * anyway, since `throw $v` (a variable holding an int) always compiled and`` |
|       - | 3428 | `	 * fell through to the same runtime path. OP_THROW now owns the whole rule,` |
|       - | 3429 | ``	 * so accept any expression here; `throw;` with no operand is still rejected`` |
|       - | 3430 | `	 * by PH7_CompileThrow's SXERR_EMPTY branch. */` |
|   34787 | 3431 | `	SXUNUSED(pGen);` |
|   34787 | 3432 | `	SXUNUSED(pRoot);` |
|   69579 | 3433 | `	return SXRET_OK;` |
|       5 | 3434 | `}` |
|       - | 3435 | `/*` |
|       - | 3436 | ` * Compile a 'throw' statement.` |
|       - | 3437 | ` * throw: This is how you trigger an exception.` |
|       - | 3438 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - | 3439 | ` */` |
|   69530 | 3440 | `PH7_PRIVATE sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       5 | 3441 | `{` |
|   69535 | 3442 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 3443 | `	GenBlock *pBlock;` |
|       - | 3444 | `	sxu32 nIdx;` |
|       - | 3445 | `	sxi32 rc;` |
|   69535 | 3446 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - | 3447 | `	/* Compile the expression */` |
|   69535 | 3448 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|   69535 | 3449 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 3450 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 | 3451 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3452 | `			return SXERR_ABORT;` |
|       - | 3453 | `		}` |
|     ! 0 | 3454 | `		return SXRET_OK;` |
|       - | 3455 | `	}` |
|   69535 | 3456 | `	pBlock = pGen->pCurrent;` |
|       - | 3457 | `	/* Point to the top most function or try block and emit the forward jump */` |
|  322783 | 3458 | `	while(pBlock->pParent){` |
|  322761 | 3459 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|   69513 | 3460 | `			break;` |
|       - | 3461 | `		}` |
|       - | 3462 | `		/* Point to the parent block */` |
|  253253 | 3463 | `		pBlock = pBlock->pParent;` |
|       5 | 3464 | `	}` |
|       - | 3465 | `	/* Emit the throw instruction */` |
|   69535 | 3466 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - | 3467 | `	/* Emit the jump */` |
|   69535 | 3468 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|   69535 | 3469 | `	return SXRET_OK;` |
|   34770 | 3470 | `}` |
|       - | 3471 | `/*` |
|       - | 3472 | ` * Compile a PHP 8.0 'throw' expression.` |
|       - | 3473 | ` * Called from the expression code generator when a 'throw' keyword is` |
|       - | 3474 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|       - | 3475 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|       - | 3476 | ` * the validator guarantees the operand is a valid exception target.` |
|       - | 3477 | ` */` |
|      44 | 3478 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       4 | 3479 | `{` |
|      48 | 3480 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 3481 | `	GenBlock *pBlock;` |
|       - | 3482 | `	sxu32 nIdx;` |
|       - | 3483 | `	sxi32 rc;` |
|      22 | 3484 | `	(void)iCompileFlag;` |
|      48 | 3485 | `	pGen->pIn++; /* Skip 'throw' */` |
|      48 | 3486 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 3487 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 3488 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 3489 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3490 | `			return SXERR_ABORT;` |
|       - | 3491 | `		}` |
|     ! 0 | 3492 | `		return SXRET_OK;` |
|       - | 3493 | `	}` |
|      48 | 3494 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|      48 | 3495 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 3496 | `		return SXERR_ABORT;` |
|       - | 3497 | `	}` |
|      48 | 3498 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 3499 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 3500 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 3501 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3502 | `			return SXERR_ABORT;` |
|       - | 3503 | `		}` |
|     ! 0 | 3504 | `		return SXRET_OK;` |
|       - | 3505 | `	}` |
|       - | 3506 | `	/* Walk up to nearest exception/function block for the jump target */` |
|      48 | 3507 | `	pBlock = pGen->pCurrent;` |
|      72 | 3508 | `	while( pBlock->pParent ){` |
|      62 | 3509 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|      38 | 3510 | `			break;` |
|       - | 3511 | `		}` |
|      26 | 3512 | `		pBlock = pBlock->pParent;` |
|       2 | 3513 | `	}` |
|      48 | 3514 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|      48 | 3515 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|      48 | 3516 | `	return SXRET_OK;` |
|      26 | 3517 | `}` |
|       - | 3518 | `/*` |
|       - | 3519 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|       - | 3520 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|       - | 3521 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|       - | 3522 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|       - | 3523 | ` * compile error propagated from the parser.` |
|       - | 3524 | ` */` |
|      68 | 3525 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|       5 | 3526 | `{` |
|       - | 3527 | `	SyString sClassName;` |
|       - | 3528 | `	SyToken *pToken;` |
|       - | 3529 | `	SyString *pName;` |
|       - | 3530 | `	char *zDup;` |
|       - | 3531 | `	sxi32 rc;` |
|      73 | 3532 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|      73 | 3533 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|      73 | 3534 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|       - | 3535 | `	/* Inline catches compile into the function's own container; pByteCode stays NULL. */` |
|      73 | 3536 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 | 3537 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 3538 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 3539 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 3540 | `		return SXERR_INVALID;` |
|       - | 3541 | `	}` |
|      73 | 3542 | `	pGen->pIn++; /* '(' */` |
|      34 | 3543 | `	for(;;){` |
|       - | 3544 | `		SyBlob sResolved;` |
|      73 | 3545 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      73 | 3546 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 | 3547 | `			SyBlobRelease(&sResolved);` |
|     ! 0 | 3548 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 3549 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 3550 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 3551 | `			return SXERR_INVALID;` |
|       - | 3552 | `		}` |
|     107 | 3553 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      68 | 3554 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|      73 | 3555 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|      73 | 3556 | `		SyBlobRelease(&sResolved);` |
|      73 | 3557 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|      73 | 3558 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|      73 | 3559 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      68 | 3560 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|       5 | 3561 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|     ! 0 | 3562 | `			pGen->pIn++; continue;` |
|       - | 3563 | `		}` |
|      73 | 3564 | `		break;` |
|     ! 0 | 3565 | `	}` |
|       - | 3566 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|       - | 3567 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding. */` |
|      73 | 3568 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|       3 | 3569 | `		pGen->pIn++; /* ')' */` |
|       3 | 3570 | `		return SXRET_OK;` |
|       - | 3571 | `	}` |
|      66 | 3572 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|      71 | 3573 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 3574 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 3575 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 3576 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 3577 | `		return SXERR_INVALID;` |
|       - | 3578 | `	}` |
|      71 | 3579 | `	pGen->pIn++; /* '$' */` |
|      71 | 3580 | `	pName = &pGen->pIn->sData;` |
|      71 | 3581 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      71 | 3582 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|      71 | 3583 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|      71 | 3584 | `	pGen->pIn++;` |
|      71 | 3585 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 | 3586 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 3587 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 3588 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 3589 | `		return SXERR_INVALID;` |
|       - | 3590 | `	}` |
|      71 | 3591 | `	pGen->pIn++; /* ')' */` |
|      71 | 3592 | `	return SXRET_OK;` |
|      39 | 3593 | `}` |
|       - | 3594 | `/*` |
|       - | 3595 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|       - | 3596 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|       - | 3597 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|       - | 3598 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|       - | 3599 | ` * VmThrowException):` |
|       - | 3600 | ` *` |
|       - | 3601 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|       - | 3602 | ` *    <try body>` |
|       - | 3603 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|       - | 3604 | ` *    JMP  -> finally\|end` |
|       - | 3605 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|       - | 3606 | ` *    <catch body>` |
|       - | 3607 | ` *    JMP  -> finally\|end` |
|       - | 3608 | ` *    ... more catches ...` |
|       - | 3609 | ` *  Lfin: <finally body>` |
|       - | 3610 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|       - | 3611 | ` *  Lend:` |
|       - | 3612 | ` */` |
|     124 | 3613 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|       5 | 3614 | `{` |
|     129 | 3615 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 3616 | `	GenBlock *pTry;` |
|       - | 3617 | `	VmInstr *pInstr;` |
|     129 | 3618 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|       - | 3619 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|       - | 3620 | `	sxi32 rc;` |
|     129 | 3621 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|       - | 3622 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION; passed at` |
|       - | 3623 | `	 * ENTRY so GenStateEnterBlock can classify the scope with it) */` |
|     191 | 3624 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),` |
|      62 | 3625 | `		pException,&pTry);` |
|     129 | 3626 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|     129 | 3627 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|     129 | 3628 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|     129 | 3629 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     129 | 3630 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|     129 | 3631 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|     129 | 3632 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|       - | 3633 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|     129 | 3634 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|     129 | 3635 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|     129 | 3636 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|     129 | 3637 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 3638 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|     129 | 3639 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|       - | 3640 | `	/* Catch clauses (inline) */` |
|     129 | 3641 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     124 | 3642 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|      73 | 3643 | `		sxu32 k = 0;` |
|     102 | 3644 | `		for(;;){` |
|       - | 3645 | `			ph7_exception_block sCatch;` |
|       - | 3646 | `			GenBlock *pCatchBlk;` |
|     141 | 3647 | `			sxu32 idxJmp = 0;` |
|     136 | 3648 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     130 | 3649 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|      39 | 3650 | `				break;` |
|       - | 3651 | `			}` |
|      73 | 3652 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|      73 | 3653 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      73 | 3654 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|      73 | 3655 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|      73 | 3656 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|       - | 3657 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|       - | 3658 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|       - | 3659 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump).` |
|       - | 3660 | `			 * Passed at ENTRY: GenStateEnterBlock reads it to classify the block's scope. */` |
|     107 | 3661 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),` |
|      34 | 3662 | `				pException,&pCatchBlk);` |
|      73 | 3663 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      73 | 3664 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      73 | 3665 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      73 | 3666 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|      73 | 3667 | `			GenStateLeaveBlock(&(*pGen),0);` |
|       - | 3668 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|       - | 3669 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|      73 | 3670 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|      73 | 3671 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|      73 | 3672 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|      73 | 3673 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|      73 | 3674 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      73 | 3675 | `			k++;` |
|       5 | 3676 | `		}` |
|      34 | 3677 | `	}` |
|       - | 3678 | `	/* Finally (inline) */` |
|     129 | 3679 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     102 | 3680 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 3681 | `		GenBlock *pFinBlk;` |
|      65 | 3682 | `		pGen->pIn++; /* Jump 'finally' */` |
|      65 | 3683 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|      95 | 3684 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FINALLY,` |
|      30 | 3685 | `			PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|      65 | 3686 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      65 | 3687 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      65 | 3688 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      65 | 3689 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|      65 | 3690 | `		GenStateLeaveBlock(&(*pGen),0);` |
|      65 | 3691 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|      65 | 3692 | `		pException->iHasFinally = 1;` |
|      30 | 3693 | `	}` |
|     129 | 3694 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|     129 | 3695 | `	pException->iInlined = 1;` |
|       - | 3696 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|       - | 3697 | `	{` |
|     129 | 3698 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|       - | 3699 | `		sxu32 *aJ; sxu32 n;` |
|     129 | 3700 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|     129 | 3701 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|     129 | 3702 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|     197 | 3703 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|      73 | 3704 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|      73 | 3705 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|      39 | 3706 | `		}` |
|       - | 3707 | `	}` |
|     129 | 3708 | `	SySetRelease(&aCatchJmp);` |
|     129 | 3709 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|     ! 0 | 3710 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|     ! 0 | 3711 | `	}` |
|     129 | 3712 | `	return SXRET_OK;` |
|      67 | 3713 | `}` |
|       - | 3714 | `/*` |
|       - | 3715 | ` * Compile a 'catch' block.` |
|       - | 3716 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - | 3717 | ` * an object containing the exception information.` |
|       - | 3718 | ` */` |
|    4180 | 3719 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       5 | 3720 | `{` |
|    4185 | 3721 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 3722 | `	ph7_exception_block sCatch;` |
|       - | 3723 | `	SySet *pInstrContainer;` |
|       - | 3724 | `	SyString sClassName;` |
|       - | 3725 | `	GenBlock *pCatch;` |
|       - | 3726 | `	SyToken *pToken;` |
|       - | 3727 | `	SyString *pName;` |
|       - | 3728 | `	char *zDup;` |
|       - | 3729 | `	sxi32 rc;` |
|    4185 | 3730 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - | 3731 | `	/* Zero the structure */` |
|    4185 | 3732 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - | 3733 | `	/* Initialize fields */` |
|    4185 | 3734 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|       - | 3735 | `	/* The catch body gets its own bytecode array, allocated (not embedded) so its address` |
|       - | 3736 | `	 * survives both this stack frame and any later growth of pException->sEntry — a` |
|       - | 3737 | `	 * break/continue inside the body records it in its JumpFixup (see JumpFixup). */` |
|    4185 | 3738 | `	sCatch.pByteCode = (SySet *)SyMemBackendAlloc(&pException->pVm->sAllocator,sizeof(SySet));` |
|    4185 | 3739 | `	if( sCatch.pByteCode == 0 ){` |
|     ! 0 | 3740 | `		goto Mem;` |
|       - | 3741 | `	}` |
|    4185 | 3742 | `	SySetInit(sCatch.pByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|    4185 | 3743 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|       - | 3744 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 3745 | `			pToken = pGen->pIn;` |
|     ! 0 | 3746 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 3747 | `				pToken--;` |
|     ! 0 | 3748 | `			}` |
|     ! 0 | 3749 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 3750 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 3751 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 3752 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3753 | `				return SXERR_ABORT;` |
|       - | 3754 | `			}` |
|     ! 0 | 3755 | `			return SXERR_INVALID;` |
|       - | 3756 | `	}` |
|       - | 3757 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|    4185 | 3758 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|    2107 | 3759 | `	for(;;){` |
|       - | 3760 | `		SyBlob sResolved;` |
|    4219 | 3761 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    4219 | 3762 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       6 | 3763 | `			SyBlobRelease(&sResolved);` |
|       6 | 3764 | `			pToken = pGen->pIn;` |
|       6 | 3765 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 3766 | `				pToken--;` |
|     ! 0 | 3767 | `			}` |
|       8 | 3768 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 3769 | `				"syntax error, unexpected %s \"%z\"",` |
|       2 | 3770 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       6 | 3771 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3772 | `				return SXERR_ABORT;` |
|       - | 3773 | `			}` |
|       6 | 3774 | `			return SXERR_INVALID;` |
|       - | 3775 | `		}` |
|       - | 3776 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|       - | 3777 | `		 * transient SyBlob allocation. */` |
|    6320 | 3778 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    4210 | 3779 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|    4215 | 3780 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|    4215 | 3781 | `		SyBlobRelease(&sResolved);` |
|    4215 | 3782 | `		if( zDup == 0 ){` |
|     ! 0 | 3783 | `			goto Mem;` |
|       - | 3784 | `		}` |
|    4215 | 3785 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|    4215 | 3786 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3787 | `			goto Mem;` |
|       - | 3788 | `		}` |
|       - | 3789 | `		/* Check for '\|' (multi-catch separator) */` |
|    4210 | 3790 | `		if( pGen->pIn < pGen->pEnd &&` |
|    4210 | 3791 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|      39 | 3792 | `			pGen->pIn->sData.nByte == 1 &&` |
|      34 | 3793 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|      37 | 3794 | `			pGen->pIn++; /* Consume the '\|' */` |
|      37 | 3795 | `			continue;` |
|       - | 3796 | `		}` |
|    4181 | 3797 | `		break;` |
|     ! 0 | 3798 | `	}` |
|       - | 3799 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|       - | 3800 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding;` |
|       - | 3801 | `	 * jump straight to compiling the block below. */` |
|    4181 | 3802 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) /*)*/ ){` |
|       7 | 3803 | `		goto CatchBody;` |
|       - | 3804 | `	}` |
|    4170 | 3805 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|    4175 | 3806 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 3807 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 3808 | `			pToken = pGen->pIn;` |
|     ! 0 | 3809 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 3810 | `				pToken--;` |
|     ! 0 | 3811 | `			}` |
|     ! 0 | 3812 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 3813 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 3814 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 3815 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3816 | `				return SXERR_ABORT;` |
|       - | 3817 | `			}` |
|     ! 0 | 3818 | `			return SXERR_INVALID;` |
|       - | 3819 | `	}` |
|    4175 | 3820 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - | 3821 | `	/* Duplicate instance name */` |
|    4175 | 3822 | `	pName = &pGen->pIn->sData;` |
|    4175 | 3823 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    4175 | 3824 | `	if( zDup == 0 ){` |
|     ! 0 | 3825 | `		goto Mem;` |
|       - | 3826 | `	}` |
|    4175 | 3827 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|    4175 | 3828 | `	pGen->pIn++;` |
|    2088 | 3829 | `CatchBody:` |
|    4181 | 3830 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - | 3831 | `		/* Unexpected token,break immediately */` |
|     ! 0 | 3832 | `		pToken = pGen->pIn;` |
|     ! 0 | 3833 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 | 3834 | `			pToken--;` |
|     ! 0 | 3835 | `		}` |
|     ! 0 | 3836 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 3837 | `			"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 3838 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 3839 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3840 | `			return SXERR_ABORT;` |
|       - | 3841 | `		}` |
|     ! 0 | 3842 | `		return SXERR_INVALID;` |
|       - | 3843 | `	}` |
|       - | 3844 | `	/* Compile the block */` |
|    4181 | 3845 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - | 3846 | `	/* Create the catch block. GEN_BLOCK_DETACHED: the body below compiles into` |
|       - | 3847 | `	 * sCatch.pByteCode, not into the enclosing function's array. */` |
|    4181 | 3848 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION\|GEN_BLOCK_DETACHED,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|    4181 | 3849 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 3850 | `		return SXERR_ABORT;` |
|       - | 3851 | `	}` |
|       - | 3852 | `	/* Swap bytecode container */` |
|    4181 | 3853 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    4181 | 3854 | `	PH7_VmSetByteCodeContainer(pGen->pVm,sCatch.pByteCode);` |
|       - | 3855 | `	/* Compile the block */` |
|    4181 | 3856 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 3857 | `	/* Fix forward jumps now the destination is resolved  */` |
|    4181 | 3858 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 3859 | `	/* Emit the DONE instruction */` |
|    4181 | 3860 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 3861 | `	/* Leave the block */` |
|    4181 | 3862 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 3863 | `	/* Restore the default container */` |
|    4181 | 3864 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 3865 | `	/* Install the catch block */` |
|    4181 | 3866 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|    4181 | 3867 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 3868 | `		goto Mem;` |
|       - | 3869 | `	}` |
|    4181 | 3870 | `	return SXRET_OK;` |
|     ! 0 | 3871 | `Mem:` |
|     ! 0 | 3872 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 3873 | `	return SXERR_ABORT;` |
|    2095 | 3874 | `}` |
|       - | 3875 | `/*` |
|       - | 3876 | ` * Compile a 'try' block.` |
|       - | 3877 | ` * A function using an exception should be in a "try" block.` |
|       - | 3878 | ` * If the exception does not trigger, the code will continue` |
|       - | 3879 | ` * as normal. However if the exception triggers, an exception` |
|       - | 3880 | ` * is "thrown".` |
|       - | 3881 | ` */` |
|    4458 | 3882 | `PH7_PRIVATE sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       5 | 3883 | `{` |
|       - | 3884 | `	ph7_exception *pException;` |
|    4463 | 3885 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 3886 | `	GenBlock *pTry;` |
|       - | 3887 | `	sxu32 nJmpIdx;` |
|       - | 3888 | `	sxi32 rc;` |
|       - | 3889 | `	/* Create the exception container */` |
|    4463 | 3890 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|    4463 | 3891 | `	if( pException == 0 ){` |
|     ! 0 | 3892 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 | 3893 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 3894 | `		return SXERR_ABORT;` |
|       - | 3895 | `	}` |
|       - | 3896 | `	/* Zero the structure */` |
|    4463 | 3897 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - | 3898 | `	/* Initialize fields */` |
|    4463 | 3899 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|    4463 | 3900 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|    4463 | 3901 | `	pException->iHasFinally = 0;` |
|    4463 | 3902 | `	pException->iFinallyDone = 0;` |
|    4463 | 3903 | `	pException->pVm = pGen->pVm;` |
|       - | 3904 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|       - | 3905 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the DETACHED path`` |
|       - | 3906 | `	 * below — deliberately, not pending migration: it is the proven one, and inlining was` |
|       - | 3907 | ``	 * scoped to generators so no other code path changed. `bInlineTryCatch` is 1 since the`` |
|       - | 3908 | ``	 * inline VM handlers landed, so `bInGenerator` is what actually selects here. */`` |
|    4463 | 3909 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|     129 | 3910 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|       - | 3911 | `	}` |
|       - | 3912 | `	/* Create the try block */` |
|       - | 3913 | `	/* pUserData is the exception context, passed at ENTRY (not assigned after) because` |
|       - | 3914 | `	 * GenStateEnterBlock reads it to classify the block's try/catch scope — see aScope.` |
|       - | 3915 | `	 * It is also what a break/continue crossing this try emits its POP_EXCEPTION with. */` |
|    6506 | 3916 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),` |
|    2167 | 3917 | `		pException,&pTry);` |
|    4339 | 3918 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 3919 | `		return SXERR_ABORT;` |
|       - | 3920 | `	}` |
|       - | 3921 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|    4339 | 3922 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - | 3923 | `	/* Fix the jump later when the destination is resolved */` |
|    4339 | 3924 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|    4339 | 3925 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - | 3926 | `	/* Compile the block */` |
|    4339 | 3927 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|    4339 | 3928 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 3929 | `		return SXERR_ABORT;` |
|       - | 3930 | `	}` |
|       - | 3931 | `	/* Fix forward jumps now the destination is resolved */` |
|    4339 | 3932 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 3933 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|    4339 | 3934 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - | 3935 | `	/* Leave the block */` |
|    4339 | 3936 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 3937 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|    4339 | 3938 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|    4332 | 3939 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - | 3940 | `		/* Compile one or more catch blocks */` |
|    4175 | 3941 | `		for(;;){` |
|    8350 | 3942 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    6897 | 3943 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|    2090 | 3944 | `					break;` |
|       - | 3945 | `			}` |
|    4185 | 3946 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|    4185 | 3947 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3948 | `				return SXERR_ABORT;` |
|       - | 3949 | `			}` |
|       5 | 3950 | `		}` |
|    2085 | 3951 | `	}` |
|       - | 3952 | `	/* Compile optional finally block */` |
|    4339 | 3953 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|    2124 | 3954 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 3955 | `		SySet *pInstrContainer;` |
|       - | 3956 | `		GenBlock *pFinBlock;` |
|     249 | 3957 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - | 3958 | `		/* Create the finally block for jump fixup bookkeeping (detached: the body` |
|       - | 3959 | `		 * compiles into pException->sFinally, see GEN_BLOCK_DETACHED). */` |
|     371 | 3960 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION\|GEN_BLOCK_DETACHED\|GEN_BLOCK_FINALLY,` |
|     122 | 3961 | `			PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|     249 | 3962 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3963 | `			return SXERR_ABORT;` |
|       - | 3964 | `		}` |
|       - | 3965 | `		/* Swap bytecode container */` |
|     249 | 3966 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     249 | 3967 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - | 3968 | `		/* Compile the finally body */` |
|     249 | 3969 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     249 | 3970 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3971 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 3972 | `			return SXERR_ABORT;` |
|       - | 3973 | `		}` |
|       - | 3974 | `		/* Fix forward jumps now the destination is resolved */` |
|     249 | 3975 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 3976 | `		/* Emit DONE to terminate the finally block */` |
|     249 | 3977 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 3978 | `		/* Leave the block */` |
|     249 | 3979 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - | 3980 | `		/* Restore the default container */` |
|     249 | 3981 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     249 | 3982 | `		pException->iHasFinally = 1;` |
|     122 | 3983 | `	}` |
|       - | 3984 | `	/* Must have at least one catch or finally */` |
|    4339 | 3985 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       8 | 3986 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 3987 | `			"Cannot use try without catch or finally");` |
|       8 | 3988 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3989 | `			return SXERR_ABORT;` |
|       - | 3990 | `		}` |
|       3 | 3991 | `	}` |
|    4339 | 3992 | `	return SXRET_OK;` |
|    2234 | 3993 | `}` |
|       - | 3994 | `/*` |
|       - | 3995 | ` * Compile a switch block.` |
|       - | 3996 | ` *  (See block-comment below for more information)` |
|       - | 3997 | ` */` |
|     206 | 3998 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       5 | 3999 | `{` |
|     211 | 4000 | `	sxi32 rc = SXRET_OK;` |
|     211 | 4001 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - | 4002 | `		/* Unexpected token */` |
|     ! 0 | 4003 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|     ! 0 | 4004 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4005 | `			return SXERR_ABORT;` |
|       - | 4006 | `		}` |
|     ! 0 | 4007 | `		pGen->pIn++;` |
|     ! 0 | 4008 | `	}` |
|     211 | 4009 | `	pGen->pIn++;` |
|       - | 4010 | `	/* First instruction to execute in this block. */` |
|     211 | 4011 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - | 4012 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - | 4013 | `	 * or the '}' token */` |
|     327 | 4014 | `	for(;;){` |
|     659 | 4015 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 4016 | `			/* No more input to process */` |
|     ! 0 | 4017 | `			break;` |
|       - | 4018 | `		}` |
|     659 | 4019 | `		rc = SXRET_OK;` |
|     659 | 4020 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     155 | 4021 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      63 | 4022 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - | 4023 | `					/* Unexpected token */` |
|     ! 0 | 4024 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 4025 | `						&pGen->pIn->sData);` |
|     ! 0 | 4026 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 4027 | `						return SXERR_ABORT;` |
|       - | 4028 | `					}` |
|       - | 4029 | `					/* FALL THROUGH */` |
|     ! 0 | 4030 | `				}` |
|      63 | 4031 | `				rc = SXERR_EOF;` |
|      63 | 4032 | `				break;` |
|       - | 4033 | `			}` |
|      51 | 4034 | `		}else{` |
|       - | 4035 | `			sxi32 nKwrd;` |
|       - | 4036 | `			/* Extract the keyword */` |
|     509 | 4037 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     509 | 4038 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      77 | 4039 | `				break;` |
|       - | 4040 | `			}` |
|     365 | 4041 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       5 | 4042 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - | 4043 | `					/* Unexpected token */` |
|     ! 0 | 4044 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 4045 | `						&pGen->pIn->sData);` |
|     ! 0 | 4046 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 4047 | `						return SXERR_ABORT;` |
|       - | 4048 | `					}` |
|       - | 4049 | `					/* FALL THROUGH */` |
|     ! 0 | 4050 | `				}` |
|       - | 4051 | `				/* Block compiled */` |
|       5 | 4052 | `				break;` |
|       - | 4053 | `			}` |
|       - | 4054 | `		}` |
|       - | 4055 | `		/* Compile block */` |
|     453 | 4056 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     453 | 4057 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4058 | `			return SXERR_ABORT;` |
|       - | 4059 | `		}` |
|       5 | 4060 | `	}` |
|     211 | 4061 | `	return rc;` |
|     108 | 4062 | `}` |
|       - | 4063 | `/*` |
|       - | 4064 | ` * Compile a case eXpression.` |
|       - | 4065 | ` *  (See block-comment below for more information)` |
|       - | 4066 | ` */` |
|     160 | 4067 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       5 | 4068 | `{` |
|       - | 4069 | `	SySet *pInstrContainer;` |
|       - | 4070 | `	SyToken *pEnd,*pTmp;` |
|     165 | 4071 | `	sxi32 iNest = 0;` |
|       - | 4072 | `	sxi32 rc;` |
|       - | 4073 | `	/* Delimit the expression */` |
|     165 | 4074 | `	pEnd = pGen->pIn;` |
|     351 | 4075 | `	while( pEnd < pGen->pEnd ){` |
|     351 | 4076 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - | 4077 | `			/* Increment nesting level */` |
|      10 | 4078 | `			iNest++;` |
|     347 | 4079 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - | 4080 | `			/* Decrement nesting level */` |
|      10 | 4081 | `			iNest--;` |
|     339 | 4082 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|     165 | 4083 | `			break;` |
|       - | 4084 | `		}` |
|     191 | 4085 | `		pEnd++;` |
|       5 | 4086 | `	}` |
|     165 | 4087 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 4088 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 | 4089 | `		if( rc == SXERR_ABORT ){` |
|       - | 4090 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4091 | `			return SXERR_ABORT;` |
|       - | 4092 | `		}` |
|     ! 0 | 4093 | `	}` |
|       - | 4094 | `	/* Swap token stream */` |
|     165 | 4095 | `	pTmp = pGen->pEnd;` |
|     165 | 4096 | `	pGen->pEnd = pEnd;` |
|     165 | 4097 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     165 | 4098 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|     165 | 4099 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 4100 | `	/* Emit the done instruction */` |
|     165 | 4101 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     165 | 4102 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 4103 | `	/* Update token stream */` |
|     165 | 4104 | `	pGen->pIn  = pEnd;` |
|     165 | 4105 | `	pGen->pEnd = pTmp;` |
|     165 | 4106 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 4107 | `		return SXERR_ABORT;` |
|       - | 4108 | `	}` |
|     165 | 4109 | `	return SXRET_OK;` |
|      85 | 4110 | `}` |
|       - | 4111 | `/*` |
|       - | 4112 | ` * Compile the smart switch statement.` |
|       - | 4113 | ` * According to the PHP language reference manual` |
|       - | 4114 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - | 4115 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - | 4116 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - | 4117 | ` *  This is exactly what the switch statement is for.` |
|       - | 4118 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - | 4119 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - | 4120 | ` *  of the outer loop, use continue 2.` |
|       - | 4121 | ` *  Note that switch/case does loose comparision.` |
|       - | 4122 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - | 4123 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - | 4124 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - | 4125 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - | 4126 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - | 4127 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - | 4128 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - | 4129 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - | 4130 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - | 4131 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - | 4132 | ` *  list for the next case.` |
|       - | 4133 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - | 4134 | ` *  or floating-point numbers and strings.` |
|       - | 4135 | ` */` |
|      62 | 4136 | `PH7_PRIVATE sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       5 | 4137 | `{` |
|       - | 4138 | `	GenBlock *pSwitchBlock;` |
|       - | 4139 | `	SyToken *pTmp,*pEnd;` |
|       - | 4140 | `	ph7_switch *pSwitch;` |
|       - | 4141 | `	sxu32 nToken;` |
|       - | 4142 | `	sxu32 nLine;` |
|       - | 4143 | `	sxi32 rc;` |
|      67 | 4144 | `	nLine = pGen->pIn->nLine;` |
|       - | 4145 | `	/* Jump the 'switch' keyword */` |
|      67 | 4146 | `	pGen->pIn++;` |
|      67 | 4147 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 4148 | `		/* Syntax error */` |
|     ! 0 | 4149 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 | 4150 | `		if( rc == SXERR_ABORT ){` |
|       - | 4151 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4152 | `			return SXERR_ABORT;` |
|       - | 4153 | `		}` |
|     ! 0 | 4154 | `		goto Synchronize;` |
|       - | 4155 | `	}` |
|       - | 4156 | `	/* Jump the left parenthesis '(' */` |
|      67 | 4157 | `	pGen->pIn++;` |
|      67 | 4158 | `	pEnd = 0; /* cc warning */` |
|       - | 4159 | `	/* Create the loop block */` |
|      98 | 4160 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      31 | 4161 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      67 | 4162 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4163 | `		return SXERR_ABORT;` |
|       - | 4164 | `	}` |
|       - | 4165 | `	/* Delimit the condition */` |
|      67 | 4166 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      67 | 4167 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 4168 | `		/* Empty expression */` |
|     ! 0 | 4169 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 | 4170 | `		if( rc == SXERR_ABORT ){` |
|       - | 4171 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4172 | `			return SXERR_ABORT;` |
|       - | 4173 | `		}` |
|     ! 0 | 4174 | `	}` |
|       - | 4175 | `	/* Swap token streams */` |
|      67 | 4176 | `	pTmp = pGen->pEnd;` |
|      67 | 4177 | `	pGen->pEnd = pEnd;` |
|       - | 4178 | `	/* Compile the expression */` |
|      67 | 4179 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      67 | 4180 | `	if( rc == SXERR_ABORT ){` |
|       - | 4181 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 4182 | `		return SXERR_ABORT;` |
|       - | 4183 | `	}` |
|       - | 4184 | `	/* Update token stream */` |
|      67 | 4185 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 4186 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4187 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 4188 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4189 | `			return SXERR_ABORT;` |
|       - | 4190 | `		}` |
|     ! 0 | 4191 | `		pGen->pIn++;` |
|     ! 0 | 4192 | `	}` |
|      67 | 4193 | `	pGen->pIn  = &pEnd[1];` |
|      67 | 4194 | `	pGen->pEnd = pTmp;` |
|      67 | 4195 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      62 | 4196 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 | 4197 | `			pTmp = pGen->pIn;` |
|     ! 0 | 4198 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 | 4199 | `				pTmp--;` |
|     ! 0 | 4200 | `			}` |
|       - | 4201 | `			/* Unexpected token */` |
|     ! 0 | 4202 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 | 4203 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4204 | `				return SXERR_ABORT;` |
|       - | 4205 | `			}` |
|     ! 0 | 4206 | `			goto Synchronize;` |
|       - | 4207 | `	}` |
|       - | 4208 | `	/* Set the delimiter token */` |
|      67 | 4209 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       5 | 4210 | `		nToken = PH7_TK_KEYWORD;` |
|       - | 4211 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       3 | 4212 | `	}else{` |
|      63 | 4213 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - | 4214 | `	}` |
|      67 | 4215 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - | 4216 | `	/* Create the switch blocks container */` |
|      67 | 4217 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      67 | 4218 | `	if( pSwitch == 0 ){` |
|       - | 4219 | `		/* Abort compilation */` |
|     ! 0 | 4220 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4221 | `		return SXERR_ABORT;` |
|       - | 4222 | `	}` |
|       - | 4223 | `	/* Zero the structure */` |
|      67 | 4224 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - | 4225 | `	/* Initialize fields */` |
|      67 | 4226 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - | 4227 | `	/* Emit the switch instruction */` |
|      67 | 4228 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - | 4229 | `	/* Compile case blocks */` |
|     179 | 4230 | `	for(;;){` |
|       - | 4231 | `		sxu32 nKwrd;` |
|     215 | 4232 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 4233 | `			/* No more input to process */` |
|     ! 0 | 4234 | `			break;` |
|       - | 4235 | `		}` |
|     215 | 4236 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 4237 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - | 4238 | `				/* Unexpected token */` |
|     ! 0 | 4239 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 4240 | `					&pGen->pIn->sData);` |
|     ! 0 | 4241 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 4242 | `					return SXERR_ABORT;` |
|       - | 4243 | `				}` |
|       - | 4244 | `				/* FALL THROUGH */` |
|     ! 0 | 4245 | `			}` |
|       - | 4246 | `			/* Block compiled */` |
|     ! 0 | 4247 | `			break;` |
|       - | 4248 | `		}` |
|       - | 4249 | `		/* Extract the keyword */` |
|     215 | 4250 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     215 | 4251 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       5 | 4252 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - | 4253 | `				/* Unexpected token */` |
|     ! 0 | 4254 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 4255 | `					&pGen->pIn->sData);` |
|     ! 0 | 4256 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 4257 | `					return SXERR_ABORT;` |
|       - | 4258 | `				}` |
|       - | 4259 | `				/* FALL THROUGH */` |
|     ! 0 | 4260 | `			}` |
|       - | 4261 | `			/* Block compiled */` |
|       5 | 4262 | `			break;` |
|       - | 4263 | `		}` |
|     211 | 4264 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - | 4265 | `			/*` |
|       - | 4266 | `			 * Accroding to the PHP language reference manual` |
|       - | 4267 | `			 *  A special case is the default case. This case matches anything` |
|       - | 4268 | `			 *  that wasn't matched by the other cases.` |
|       - | 4269 | `			 */` |
|      51 | 4270 | `			if( pSwitch->nDefault > 0 ){` |
|       - | 4271 | `				/* Default case already compiled */` |
|     ! 0 | 4272 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 | 4273 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 4274 | `					return SXERR_ABORT;` |
|       - | 4275 | `				}` |
|     ! 0 | 4276 | `			}` |
|      51 | 4277 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - | 4278 | `			/* Compile the default block */` |
|      51 | 4279 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      51 | 4280 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 4281 | `				return SXERR_ABORT;` |
|      51 | 4282 | `			}else if( rc == SXERR_EOF ){` |
|      49 | 4283 | `				break;` |
|       1 | 4284 | `			}` |
|     166 | 4285 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - | 4286 | `			ph7_case_expr sCase;` |
|       - | 4287 | `			/* Standard case block */` |
|     165 | 4288 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - | 4289 | `			/* initialize the structure */` |
|     165 | 4290 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 4291 | `			/* Compile the case expression */` |
|     165 | 4292 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|     165 | 4293 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4294 | `				return SXERR_ABORT;` |
|       - | 4295 | `			}` |
|       - | 4296 | `			/* Compile the case block */` |
|     165 | 4297 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - | 4298 | `			/* Insert in the switch container */` |
|     165 | 4299 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|     165 | 4300 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 4301 | `				return SXERR_ABORT;` |
|     165 | 4302 | `			}else if( rc == SXERR_EOF ){` |
|      17 | 4303 | `				break;` |
|       - | 4304 | `			}` |
|      78 | 4305 | `		}else{` |
|       - | 4306 | `			/* Unexpected token */` |
|     ! 0 | 4307 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 4308 | `				&pGen->pIn->sData);` |
|     ! 0 | 4309 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4310 | `				return SXERR_ABORT;` |
|       - | 4311 | `			}` |
|     ! 0 | 4312 | `			break;` |
|       - | 4313 | `		}` |
|       5 | 4314 | `	}` |
|       - | 4315 | `	/* Fix all jumps now the destination is resolved */` |
|      67 | 4316 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      67 | 4317 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 4318 | `	/* Release the loop block */` |
|      67 | 4319 | `	GenStateLeaveBlock(pGen,0);` |
|      67 | 4320 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - | 4321 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      67 | 4322 | `		pGen->pIn++;` |
|      31 | 4323 | `	}` |
|       - | 4324 | `	/* Statement successfully compiled */` |
|      67 | 4325 | `	return SXRET_OK;` |
|     ! 0 | 4326 | `Synchronize:` |
|       - | 4327 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 4328 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 | 4329 | `		pGen->pIn++;` |
|     ! 0 | 4330 | `	}` |
|     ! 0 | 4331 | `	return SXRET_OK;` |
|      36 | 4332 | `}` |
|       - | 4333 |  |
