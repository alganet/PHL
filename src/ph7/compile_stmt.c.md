# src/ph7/compile_stmt.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1822/2346 lines (77.66%)

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
|     152 |   60 | `PH7_PRIVATE sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       5 |   61 | `{` |
|       - |   62 | `	SySet *pConsCode,*pInstrContainer;` |
|       - |   63 | `	sxu32 nLineLocal;` |
|       - |   64 | `	SyString *pName;` |
|       - |   65 | `	sxi32 rc;` |
|       - |   66 | `	/* php forbids attributes on a comma-separated const list. Snapshot whether the` |
|       - |   67 | `	 * statement carries any now, before the first constant consumes them. */` |
|     157 |   68 | `	int bHadAttrs = SySetUsed(&pGen->aPendingAttrs) > 0;` |
|       - |   69 | ``	/* php attributes every const-statement compile error to the `const` keyword's`` |
|       - |   70 | ``	 * line, not the offending list element's own line (`const A=1,\nB=strlen()` blames`` |
|       - |   71 | `	 * line 1). Capture it once here, before jumping the keyword. */` |
|     157 |   72 | `	nLineLocal = pGen->pIn->nLine;` |
|     157 |   73 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       - |   74 | ``	/* php allows a single `const` statement to declare several constants at once`` |
|       - |   75 | ``	 * (`const A = 1, B = 2;`). Loop over the comma-separated name = value pairs;`` |
|       - |   76 | `	 * PH7_CompileExpr(EXPR_FLAG_COMMA_STATEMENT) stops each value at the first` |
|       - |   77 | `	 * top-level comma so the next pair starts cleanly. */` |
|      81 |   78 | `Loop:` |
|     167 |   79 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |   80 | `		/* Invalid constant name */` |
|       9 |   81 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"identifier");` |
|       9 |   82 | `		if( rc == SXERR_ABORT ){` |
|       - |   83 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |   84 | `			return SXERR_ABORT;` |
|       - |   85 | `		}` |
|       9 |   86 | `		goto Synchronize;` |
|       - |   87 | `	}` |
|       - |   88 | `	/* Peek constant name */` |
|     161 |   89 | `	pName = &pGen->pIn->sData;` |
|       - |   90 | ``	/* php's global `const` takes an IDENTIFIER: a reserved word is a parse error`` |
|       - |   91 | `` 	 * there, and only a CLASS constant may carry one (`class C { const list = 5; }` `` |
|       - |   92 | ``	 * is php-legal, `const list = 5;` at file scope is not). PHL accepted both, so`` |
|       - |   93 | ``	 * `const LIST = 1;` compiled and READ back — source php refuses to parse.`` |
|       - |   94 | `	 * The words php still allows are the ones its lexer does not reserve: the type` |
|       - |   95 | ``	 * names and the scope words. The word OPERATORS (`and`, `or`, `xor`, `new`,`` |
|       - |   96 | ``	 * `clone`, `instanceof`) are reserved too — the lexer types those ID\|OP rather`` |
|       - |   97 | `	 * than KEYWORD, which is why the test reads both bits. */` |
|     156 |   98 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_OP))` |
|      91 |   99 | `	 && !GenStateConstNameKeywordOk(pName) ){` |
|       3 |  100 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn,"identifier");` |
|       3 |  101 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  102 | `			return SXERR_ABORT;` |
|       - |  103 | `		}` |
|       3 |  104 | `		goto Synchronize;` |
|       - |  105 | `	}` |
|       - |  106 | `	/* Make sure the constant name isn't reserved */` |
|     159 |  107 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  108 | `		/* Reserved constant */` |
|       9 |  109 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Cannot redeclare constant '%z'",pName);` |
|       9 |  110 | `		if( rc == SXERR_ABORT ){` |
|       - |  111 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  112 | `			return SXERR_ABORT;` |
|       - |  113 | `		}` |
|       9 |  114 | `		goto Synchronize;` |
|       - |  115 | `	}` |
|     151 |  116 | `	pGen->pIn++;` |
|     151 |  117 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  118 | `		/* Invalid statement*/` |
|       6 |  119 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"=\"");` |
|       6 |  120 | `		if( rc == SXERR_ABORT ){` |
|       - |  121 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  122 | `			return SXERR_ABORT;` |
|       - |  123 | `		}` |
|       6 |  124 | `		goto Synchronize;` |
|       - |  125 | `	}` |
|     147 |  126 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - |  127 | ``	/* php: a closure in a constant expression must be `static function`; a`` |
|       - |  128 | `	 * non-static closure and any arrow fn are compile-time fatals with distinct` |
|       - |  129 | `	 * messages. Checked ahead of the call scan (it skips closure bodies). */` |
|       - |  130 | `	{` |
|     147 |  131 | `		int iClo = PH7_GenStateInitClosureError(pGen);` |
|     147 |  132 | `		if( iClo ){` |
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
|     143 |  145 | `	if( PH7_GenStateInitHasCallExpr(pGen) ){` |
|       6 |  146 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - |  147 | `			"Constant expression contains invalid operations");` |
|       6 |  148 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  149 | `			return SXERR_ABORT;` |
|       - |  150 | `		}` |
|       6 |  151 | `		goto Synchronize;` |
|       - |  152 | `	}` |
|       - |  153 | `	/* Allocate a new constant value container */` |
|     139 |  154 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|     139 |  155 | `	if( pConsCode == 0 ){` |
|     ! 0 |  156 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  157 | `		return SXERR_ABORT;` |
|       - |  158 | `	}` |
|     139 |  159 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  160 | `	/* Swap bytecode container */` |
|     139 |  161 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     139 |  162 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - |  163 | ``	/* Compile constant value. php: a stray token after `const X = EXPR` is`` |
|       - |  164 | ``	 * `... expecting "," or ";"` (const supports a comma-separated list).`` |
|       - |  165 | `	 * EXPR_FLAG_COMMA_STATEMENT stops this value at the first top-level comma so a` |
|       - |  166 | `	 * following declaration is left for the loop below. */` |
|       - |  167 | `	{` |
|     139 |  168 | `		const char *zSaveConst = pGen->zClauseCloser;` |
|     139 |  169 | `		pGen->zClauseCloser = "\",\" or \";\"";` |
|     139 |  170 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|     139 |  171 | `		pGen->zClauseCloser = zSaveConst;` |
|       - |  172 | `	}` |
|       - |  173 | `	/* Emit the done instruction */` |
|     139 |  174 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     139 |  175 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     139 |  176 | `	if( rc == SXERR_ABORT ){` |
|       - |  177 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  178 | `		return SXERR_ABORT;` |
|       - |  179 | `	}` |
|       - |  180 | ``	/* php parse-errors an empty initializer (`const A = ;`, or a `,B=` list hole);`` |
|       - |  181 | `	 * the class-const path rejects it too. Reject loudly rather than silently` |
|       - |  182 | `	 * defining a NULL constant. (php's error KIND -- a parse error -- differs from` |
|       - |  183 | `	 * PHL's compile fatal, the recursive-descent-vs-bison family; both refuse.) */` |
|     139 |  184 | `	if( rc == SXERR_EMPTY && PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       2 |  185 | `			"Empty constant '%z' value",pName) == SXERR_ABORT ){` |
|     ! 0 |  186 | `		return SXERR_ABORT;` |
|       - |  187 | `	}` |
|     139 |  188 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - |  189 | `	/* Register the constant with namespace-qualified name */` |
|       - |  190 | `	{` |
|       - |  191 | `		SyBlob sFQN;` |
|       - |  192 | `		SyString sFQNStr;` |
|     139 |  193 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     139 |  194 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|     139 |  195 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|       - |  196 | ``		/* php refuses a `const` whose name a local `use const` already took. */`` |
|     139 |  197 | `		if( GenStateGuardImportRedeclare(pGen,2,pName,&sFQNStr,nLineLocal) == SXERR_ABORT ){` |
|     ! 0 |  198 | `			SyBlobRelease(&sFQN);` |
|     ! 0 |  199 | `			return SXERR_ABORT;` |
|       - |  200 | `		}` |
|     206 |  201 | `		rc = PH7_VmRegisterConstantEx(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode,` |
|     134 |  202 | `			(SyString *)SySetPeek(&pGen->pVm->aFiles),nLineLocal,1);` |
|     139 |  203 | `		if( rc == SXRET_OK && SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
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
|     139 |  216 | `		SyBlobRelease(&sFQN);` |
|       - |  217 | `	}` |
|     139 |  218 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  219 | `		SySetRelease(pConsCode);` |
|     ! 0 |  220 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 |  221 | `	}` |
|       - |  222 | ``	/* Another declaration in the same statement: `const A = 1, B = 2;`. */`` |
|     139 |  223 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /* ',' */) ){` |
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
|     127 |  236 | `	return SXRET_OK;` |
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
|      81 |  261 | `}` |
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
|   37612 |  280 | `static sxi32 GenStateLoopJumpOp(ph7_gen_state *pGen,GenBlock *pLoop,sxi32 *piP1,` |
|       - |  281 | `	GenJumpScope *pCross)` |
|       5 |  282 | `{` |
|       - |  283 | `	/* The loop encloses the break by construction, so the walk always reaches it. Note the` |
|       - |  284 | `	 * walk EMITS the crossed trys' POP_EXCEPTIONs as it goes, before the caller can see` |
|       - |  285 | `	 * pCross->nFinally and reject: a statement about to be fatal therefore leaves a few` |
|       - |  286 | `	 * dead instructions behind. Harmless — the compile error stops the program from` |
|       - |  287 | `	 * running at all — and the alternative is walking the chain twice on every jump. */` |
|   37617 |  288 | `	GenStateJumpScope(&(*pGen),pGen->nCurScopeId,pLoop->nScopeId,TRUE,pCross);` |
|   37617 |  289 | `	return GenStateScopeJumpOp(pCross,piP1);` |
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
|   28098 |  300 | `PH7_PRIVATE sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       5 |  301 | `{` |
|       - |  302 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  303 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  304 | `	sxi32 iRawLevel; /* The level as WRITTEN, kept for php's diagnostics */` |
|       - |  305 | `	sxu32 nLineLocal;` |
|       - |  306 | `	sxi32 rc;` |
|   28103 |  307 | `	iRawLevel = 1;` |
|   28103 |  308 | `	nLineLocal = pGen->pIn->nLine;` |
|   28103 |  309 | `	iLevel = 0;` |
|       - |  310 | `	/* Jump the 'continue' keyword */` |
|   28103 |  311 | `	pGen->pIn++;` |
|   28103 |  312 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  313 | `		/* optional numeric argument which tells us how many levels` |
|       - |  314 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  315 | `		 */` |
|       - |  316 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      18 |  317 | `		char *zAlloc = 0;` |
|       - |  318 | `		SyString sNum;` |
|      18 |  319 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      18 |  320 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  321 | `			return SXERR_ABORT;` |
|       - |  322 | `		}` |
|      18 |  323 | `		if( rc == SXRET_OK ){` |
|      23 |  324 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      14 |  325 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      16 |  326 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  327 | `				return SXERR_ABORT;` |
|       - |  328 | `			}` |
|      16 |  329 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      16 |  330 | `			iRawLevel = iLevel;` |
|      16 |  331 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       7 |  332 | `		}` |
|      18 |  333 | `		if( iLevel < 2 ){` |
|       3 |  334 | `			iLevel = 0;` |
|       1 |  335 | `		}` |
|      18 |  336 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       8 |  337 | `	}` |
|       - |  338 | `	/* php rejects a non-positive level outright, before asking where it lands. */` |
|   28103 |  339 | `	if( iRawLevel < 1 ){` |
|     ! 0 |  340 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - |  341 | `			"'continue' operator accepts only positive integers");` |
|     ! 0 |  342 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  343 | `			return SXERR_ABORT;` |
|       - |  344 | `		}` |
|     ! 0 |  345 | `		return SXRET_OK;` |
|       - |  346 | `	}` |
|       - |  347 | `	/* Point to the target loop */` |
|   28103 |  348 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|   28103 |  349 | `	if( pLoop == 0 ){` |
|       - |  350 | ``		/* Same split php makes for `break`: no loop at all keeps the not-in-context`` |
|       - |  351 | ``		 * wording, too FEW loops is `Cannot 'continue' N levels`. */`` |
|      12 |  352 | `		if( GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,0) != 0 ){` |
|     ! 0 |  353 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|     ! 0 |  354 | `				"Cannot 'continue' %d level%s",iRawLevel,iRawLevel == 1 ? "" : "s");` |
|     ! 0 |  355 | `		}else{` |
|      12 |  356 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"'continue' not in the 'loop' or 'switch' context");` |
|       - |  357 | `		}` |
|      12 |  358 | `		if( rc == SXERR_ABORT ){` |
|       - |  359 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  360 | `			return SXERR_ABORT;` |
|       - |  361 | `		}` |
|       7 |  362 | `	}else{` |
|   28093 |  363 | `		sxu32 nInstrIdx = 0;` |
|   28093 |  364 | `		sxi32 iP1 = 0;` |
|       - |  365 | `		GenJumpScope sCross;` |
|   28093 |  366 | `		sxi32 iJmpOp = GenStateLoopJumpOp(&(*pGen),pLoop,&iP1,&sCross);` |
|   28093 |  367 | `		if( sCross.nFinally > 0 ){` |
|       3 |  368 | `			if( GenStateJumpOutOfFinally(&(*pGen),nLineLocal) == SXERR_ABORT ){` |
|     ! 0 |  369 | `				return SXERR_ABORT;` |
|       1 |  370 | `			}` |
|   28092 |  371 | `		}else if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
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
|   28087 |  387 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,iP1,pLoop->nFirstInstr,0,&nInstrIdx);` |
|   28087 |  388 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - |  389 | `				JumpFixup sJumpFix;` |
|       - |  390 | `				/* Post-continue */` |
|   14033 |  391 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|   14033 |  392 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|   14033 |  393 | `				sJumpFix.pContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   14033 |  394 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|    7014 |  395 | `			}` |
|       - |  396 | `		}` |
|       - |  397 | `	}` |
|   28103 |  398 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  399 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  400 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 |  401 | `	}` |
|       - |  402 | `	/* Statement successfully compiled */` |
|   28103 |  403 | `	return SXRET_OK;` |
|   14054 |  404 | `}` |
|       - |  405 | `/*` |
|       - |  406 | ` * Compile the 'break' statement.` |
|       - |  407 | ` * According to the PHP language reference` |
|       - |  408 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - |  409 | ` *  structure.` |
|       - |  410 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - |  411 | ` *  enclosing structures are to be broken out of.` |
|       - |  412 | ` */` |
|    9540 |  413 | `PH7_PRIVATE sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       5 |  414 | `{` |
|       - |  415 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  416 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  417 | `	sxi32 iRawLevel; /* The level as WRITTEN, kept for php's diagnostics */` |
|       - |  418 | `	sxu32 nLineLocal;` |
|       - |  419 | `	sxi32 rc;` |
|    9545 |  420 | `	iLevel = 0;` |
|    9545 |  421 | `	iRawLevel = 1;` |
|    9545 |  422 | `	nLineLocal = pGen->pIn->nLine;` |
|       - |  423 | `	/* Jump the 'break' keyword */` |
|    9545 |  424 | `	pGen->pIn++;` |
|    9545 |  425 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  426 | `		/* optional numeric argument which tells us how many levels` |
|       - |  427 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  428 | `		 */` |
|       - |  429 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      21 |  430 | `		char *zAlloc = 0;` |
|       - |  431 | `		SyString sNum;` |
|      21 |  432 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      21 |  433 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  434 | `			return SXERR_ABORT;` |
|       - |  435 | `		}` |
|      21 |  436 | `		if( rc == SXRET_OK ){` |
|      27 |  437 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      16 |  438 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      19 |  439 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  440 | `				return SXERR_ABORT;` |
|       - |  441 | `			}` |
|      19 |  442 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      19 |  443 | `			iRawLevel = iLevel;` |
|      19 |  444 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       8 |  445 | `		}` |
|      21 |  446 | `		if( iLevel < 2 ){` |
|       3 |  447 | `			iLevel = 0;` |
|       1 |  448 | `		}` |
|      21 |  449 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       9 |  450 | `	}` |
|       - |  451 | `	/* php rejects a non-positive level outright, before asking where it lands. */` |
|    9545 |  452 | `	if( iRawLevel < 1 ){` |
|     ! 0 |  453 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  454 | `			"'break' operator accepts only positive integers");` |
|     ! 0 |  455 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  456 | `			return SXERR_ABORT;` |
|       - |  457 | `		}` |
|     ! 0 |  458 | `		goto BreakLevelDone;` |
|       - |  459 | `	}` |
|       - |  460 | `	/* Extract the target loop */` |
|    9545 |  461 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|   14315 |  462 | `	if( pLoop == 0 ){` |
|       - |  463 | `		/* php distinguishes "there is no loop at all here" from "there is one, but` |
|       - |  464 | `		 * not N of them": the first keeps the not-in-context wording, the second is` |
|       - |  465 | ``		 * `Cannot 'break' N levels`. */`` |
|      19 |  466 | `		if( GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,0) != 0 ){` |
|       4 |  467 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 |  468 | `				"Cannot 'break' %d level%s",iRawLevel,iRawLevel == 1 ? "" : "s");` |
|       2 |  469 | `		}else{` |
|      17 |  470 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"'break' not in the 'loop' or 'switch' context");` |
|       - |  471 | `		}` |
|      19 |  472 | `		if( rc == SXERR_ABORT ){` |
|       - |  473 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  474 | `			return SXERR_ABORT;` |
|       - |  475 | `		}` |
|      11 |  476 | `	}else{` |
|       - |  477 | `		sxu32 nInstrIdx;` |
|    9529 |  478 | `		sxi32 iP1 = 0;` |
|       - |  479 | `		GenJumpScope sCross;` |
|    9529 |  480 | `		sxi32 iJmpOp = GenStateLoopJumpOp(&(*pGen),pLoop,&iP1,&sCross);` |
|    9529 |  481 | `		if( sCross.nFinally > 0 ){` |
|       9 |  482 | `			if( GenStateJumpOutOfFinally(&(*pGen),nLineLocal) == SXERR_ABORT ){` |
|     ! 0 |  483 | `				return SXERR_ABORT;` |
|       - |  484 | `			}` |
|       6 |  485 | `		}else{` |
|    9523 |  486 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,iP1,0,0,&nInstrIdx);` |
|    9523 |  487 | `			if( rc == SXRET_OK ){` |
|       - |  488 | `				/* Fix the jump later when the jump destination is resolved */` |
|    9523 |  489 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|    4759 |  490 | `			}` |
|       - |  491 | `		}` |
|       - |  492 | `	}` |
|    4770 |  493 | `BreakLevelDone:` |
|    9545 |  494 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  495 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  496 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 |  497 | `	}` |
|       - |  498 | `	/* Statement successfully compiled */` |
|    9545 |  499 | `	return SXRET_OK;` |
|    4775 |  500 | `}` |
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
|       2 |  655 | `{` |
|       - |  656 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - |  657 | `	sxu32 nRawObj;` |
|      10 |  658 | `	sxu32 nObjIdx;` |
|       - |  659 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - |  660 | `	 * a PHP block.` |
|       - |  661 | `	 */` |
|      10 |  662 | `Consume:` |
|      22 |  663 | `	nRawObj = nObjIdx = 0;` |
|      22 |  664 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
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
|      22 |  676 | `	if( nRawObj > 0 ){` |
|       - |  677 | `		/* Emit the consume instruction */` |
|     ! 0 |  678 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     ! 0 |  679 | `	}` |
|      22 |  680 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
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
|      22 |  714 | `		pGen->pIn = pGen->pEnd;` |
|      22 |  715 | `		return SXERR_EOF;` |
|       - |  716 | `	}` |
|     ! 0 |  717 | `	return SXRET_OK;` |
|      12 |  718 | `}` |
|       - |  719 | `/*` |
|       - |  720 | ` * Compile a PHP block.` |
|       - |  721 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - |  722 | ` * optionally delimited by braces {}.` |
|       - |  723 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  724 | ` * and this function takes care of generating the appropriate error` |
|       - |  725 | ` * message.` |
|       - |  726 | ` */` |
|  617402 |  727 | `PH7_PRIVATE sxi32 PH7_CompileBlock(` |
|       - |  728 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  729 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - |  730 | `	)` |
|       5 |  731 | `{` |
|       - |  732 | `	sxi32 rc;` |
|       - |  733 | `	sxu32 nLine;` |
|  617407 |  734 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  616417 |  735 | `		nLine = pGen->pIn->nLine;` |
|  616417 |  736 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  616417 |  737 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  738 | `			return SXERR_ABORT;` |
|       - |  739 | `		}` |
|  616417 |  740 | `		pGen->pIn++;` |
|       - |  741 | `		/* Compile until we hit the closing braces '}' */` |
| 1000468 |  742 | `		for(;;){` |
| 2000941 |  743 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      22 |  744 | `				rc = GenStateNextChunk(&(*pGen));` |
|      22 |  745 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  746 | `			 	   return SXERR_ABORT;` |
|       - |  747 | `				}` |
|      22 |  748 | `				if( rc == SXERR_EOF ){` |
|       - |  749 | `					/* No more token to process: the block was never closed. php reports` |
|       - |  750 | `					 * the line the '{' was opened on, not where the input ran out. */` |
|      22 |  751 | `					PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"Unclosed '{' on line %u",nLine);` |
|      22 |  752 | `					break;` |
|       - |  753 | `				}` |
|     ! 0 |  754 | `			}` |
| 2000921 |  755 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - |  756 | `				/* Closing braces found,break immediately*/` |
|  616397 |  757 | `				pGen->pIn++;` |
|  616397 |  758 | `				break;` |
|       - |  759 | `			}` |
|       - |  760 | `			/* Compile a single statement */` |
| 1384529 |  761 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
| 1384529 |  762 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  763 | `				return SXERR_ABORT;` |
|       - |  764 | `			}` |
|       5 |  765 | `		}` |
|  616417 |  766 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  309201 |  767 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
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
|     985 |  811 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     985 |  812 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  813 | `			return SXERR_ABORT;` |
|       - |  814 | `		}` |
|       - |  815 | `	}` |
|       - |  816 | `	/* Jump trailing semi-colons ';' */` |
|  617417 |  817 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      11 |  818 | `		pGen->pIn++;` |
|       1 |  819 | `	}` |
|  617407 |  820 | `	return SXRET_OK;` |
|  308706 |  821 | `}` |
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
|   14184 |  841 | `PH7_PRIVATE sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       5 |  842 | `{` |
|   14189 |  843 | `	GenBlock *pWhileBlock = 0;` |
|   14189 |  844 | `	SyToken *pTmp,*pEnd = 0;` |
|       - |  845 | `	sxu32 nFalseJump;` |
|       - |  846 | `	sxu32 nLine;` |
|       - |  847 | `	sxi32 rc;` |
|   14189 |  848 | `	nLine = pGen->pIn->nLine;` |
|       - |  849 | `	/* Jump the 'while' keyword */` |
|   14189 |  850 | `	pGen->pIn++;` |
|   14189 |  851 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  852 | `		/* Syntax error */` |
|     ! 0 |  853 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  854 | `		if( rc == SXERR_ABORT ){` |
|       - |  855 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  856 | `			return SXERR_ABORT;` |
|       - |  857 | `		}` |
|     ! 0 |  858 | `		goto Synchronize;` |
|       - |  859 | `	}` |
|       - |  860 | `	/* Jump the left parenthesis '(' */` |
|   14189 |  861 | `	pGen->pIn++;` |
|       - |  862 | `	/* Create the loop block */` |
|   14189 |  863 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   14189 |  864 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  865 | `		return SXERR_ABORT;` |
|       - |  866 | `	}` |
|       - |  867 | `	/* Delimit the condition */` |
|   14189 |  868 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   14189 |  869 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  870 | `		/* Empty condition. php reports the token that actually stopped it -- for` |
|       - |  871 | ``		 * `while ()` that is the ')' -- not a hand-written "expected expression". */`` |
|       3 |  872 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,0);` |
|       3 |  873 | `		if( rc == SXERR_ABORT ){` |
|       - |  874 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  875 | `			return SXERR_ABORT;` |
|       - |  876 | `		}` |
|       1 |  877 | `	}` |
|       - |  878 | `	/* Swap token streams */` |
|   14189 |  879 | `	pTmp = pGen->pEnd;` |
|   14189 |  880 | `	pGen->pEnd = pEnd;` |
|       - |  881 | `	/* Compile the expression */` |
|   14189 |  882 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   14189 |  883 | `	if( rc == SXERR_ABORT ){` |
|       - |  884 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  885 | `		return SXERR_ABORT;` |
|       - |  886 | `	}` |
|       - |  887 | `	/* Update token stream */` |
|   14189 |  888 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  889 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|     ! 0 |  890 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  891 | `			return SXERR_ABORT;` |
|       - |  892 | `		}` |
|     ! 0 |  893 | `		pGen->pIn++;` |
|     ! 0 |  894 | `	}` |
|       - |  895 | `	/* Synchronize pointers */` |
|   14189 |  896 | `	pGen->pIn  = &pEnd[1];` |
|   14189 |  897 | `	pGen->pEnd = pTmp;` |
|       - |  898 | `	/* Emit the false jump */` |
|   14189 |  899 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  900 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   14189 |  901 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - |  902 | `	/* Compile the loop body */` |
|   14189 |  903 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   14189 |  904 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  905 | `		return SXERR_ABORT;` |
|       - |  906 | `	}` |
|       - |  907 | `	/* Emit the unconditional jump to the start of the loop */` |
|   14189 |  908 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - |  909 | `	/* Fix all jumps now the destination is resolved */` |
|   14189 |  910 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  911 | `	/* Release the loop block */` |
|   14189 |  912 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  913 | `	/* Statement successfully compiled */` |
|   14189 |  914 | `	return SXRET_OK;` |
|     ! 0 |  915 | `Synchronize:` |
|       - |  916 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  917 | `	 * compiling this erroneous block.` |
|       - |  918 | `	 */` |
|     ! 0 |  919 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  920 | `		pGen->pIn++;` |
|     ! 0 |  921 | `	}` |
|     ! 0 |  922 | `	return SXRET_OK;` |
|    7097 |  923 | `}` |
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
|   28258 | 1089 | `PH7_PRIVATE sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       5 | 1090 | `{` |
|   28263 | 1091 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   28263 | 1092 | `	GenBlock *pForBlock = 0;` |
|       - | 1093 | `	sxu32 nFalseJump;` |
|       - | 1094 | `	sxu32 nLine;` |
|       - | 1095 | `	sxi32 rc;` |
|   28263 | 1096 | `	nLine = pGen->pIn->nLine;` |
|       - | 1097 | `	/* Jump the 'for' keyword */` |
|   28263 | 1098 | `	pGen->pIn++;` |
|   28263 | 1099 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 1100 | `		/* Syntax error */` |
|     ! 0 | 1101 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 | 1102 | `		if( rc == SXERR_ABORT ){` |
|       - | 1103 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1104 | `			return SXERR_ABORT;` |
|       - | 1105 | `		}` |
|     ! 0 | 1106 | `		return SXRET_OK;` |
|       - | 1107 | `	}` |
|       - | 1108 | `	/* Jump the left parenthesis '(' */` |
|   28263 | 1109 | `	pGen->pIn++;` |
|       - | 1110 | `	/* Delimit the init-expr;condition;post-expr */` |
|   28263 | 1111 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   28263 | 1112 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
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
|   28263 | 1127 | `	pTmp = pGen->pEnd;` |
|   28263 | 1128 | `	pGen->pEnd = pEnd;` |
|       - | 1129 | `	/* for() clauses are the ONLY place php's grammar allows a comma-separated` |
|       - | 1130 | `	 * expression list, so the comma operator is permitted for their duration` |
|       - | 1131 | `	 * (see GenStateTreeHasComma). A closure body nested inside a clause is` |
|       - | 1132 | `	 * compiled through this same window — recorded as a known leniency. */` |
|   28263 | 1133 | `	pGen->nCommaExprOk++;` |
|   28263 | 1134 | `	pGen->zClauseCloser = "\";\""; /* init/condition clauses close on ';' */` |
|       - | 1135 | `	/* Compile initialization expressions if available */` |
|   28263 | 1136 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 1137 | `	/* Pop operand lvalues */` |
|   28263 | 1138 | `	if( rc == SXERR_ABORT ){` |
|       - | 1139 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 1140 | `		return SXERR_ABORT;` |
|   28263 | 1141 | `	}else if( rc != SXERR_EMPTY ){` |
|   28261 | 1142 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|   14128 | 1143 | `	}` |
|   28263 | 1144 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 1145 | `		/* Syntax error */` |
|     ! 0 | 1146 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|     ! 0 | 1147 | `		if( rc == SXERR_ABORT ){` |
|       - | 1148 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1149 | `			return SXERR_ABORT;` |
|       - | 1150 | `		}` |
|     ! 0 | 1151 | `		return SXRET_OK;` |
|       - | 1152 | `	}` |
|       - | 1153 | `	/* Jump the trailing ';' */` |
|   28263 | 1154 | `	pGen->pIn++;` |
|       - | 1155 | `	/* Create the loop block */` |
|   28263 | 1156 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   28263 | 1157 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1158 | `		return SXERR_ABORT;` |
|       - | 1159 | `	}` |
|       - | 1160 | `	/* Deffer continue jumps */` |
|   28263 | 1161 | `	pForBlock->bPostContinue = TRUE;` |
|       - | 1162 | `	/* Compile the condition */` |
|   28263 | 1163 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   28263 | 1164 | `	if( rc == SXERR_ABORT ){` |
|       - | 1165 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 1166 | `		return SXERR_ABORT;` |
|   28263 | 1167 | `	}else if( rc != SXERR_EMPTY ){` |
|       - | 1168 | `		/* Emit the false jump */` |
|   28261 | 1169 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 1170 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   28261 | 1171 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|   14128 | 1172 | `	}` |
|   28263 | 1173 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 1174 | `		/* Syntax error */` |
|       6 | 1175 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|       6 | 1176 | `		if( rc == SXERR_ABORT ){` |
|       - | 1177 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1178 | `			return SXERR_ABORT;` |
|       - | 1179 | `		}` |
|       6 | 1180 | `		return SXRET_OK;` |
|       - | 1181 | `	}` |
|       - | 1182 | `	/* Jump the trailing ';' */` |
|   28259 | 1183 | `	pGen->pIn++;` |
|       - | 1184 | `	/* Save the post condition stream */` |
|   28259 | 1185 | `	pPostStart = pGen->pIn;` |
|       - | 1186 | `	/* Compile the loop body — OUTSIDE the comma window (the body is ordinary` |
|       - | 1187 | ``	 * php, so `(1, 2)` inside it is the parse error it should be). */`` |
|   28259 | 1188 | `	pGen->nCommaExprOk--;` |
|   28259 | 1189 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   28259 | 1190 | `	pGen->pEnd = pTmp;` |
|   28259 | 1191 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   28259 | 1192 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 1193 | `		return SXERR_ABORT;` |
|       - | 1194 | `	}` |
|       - | 1195 | `	/* Fix post-continue jumps */` |
|   28259 | 1196 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - | 1197 | `		JumpFixup *aPost;` |
|       - | 1198 | `		VmInstr *pInstr;` |
|       - | 1199 | `		sxu32 nJumpDest;` |
|       - | 1200 | `		sxu32 n;` |
|    9361 | 1201 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|    9361 | 1202 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|   23387 | 1203 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|   14031 | 1204 | `			pInstr = GenStateFixupInstr(&aPost[n]);` |
|   14031 | 1205 | `			if( pInstr ){` |
|       - | 1206 | `				/* Fix jump */` |
|   14031 | 1207 | `				pInstr->iP2 = nJumpDest;` |
|    7013 | 1208 | `			}` |
|    7018 | 1209 | `		}` |
|    4678 | 1210 | `	}` |
|       - | 1211 | `	/* compile the post-expressions if available */` |
|   28259 | 1212 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 1213 | `		pPostStart++;` |
|     ! 0 | 1214 | `	}` |
|   28259 | 1215 | `	if( pPostStart < pEnd ){` |
|       - | 1216 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   28259 | 1217 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   28259 | 1218 | `		pGen->nCommaExprOk++; /* post-expressions are a clause list again */` |
|   28259 | 1219 | `		pGen->zClauseCloser = "\")\""; /* the post clause closes on ')' */` |
|   28259 | 1220 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   28259 | 1221 | `		pGen->nCommaExprOk--;` |
|   28259 | 1222 | `		pGen->zClauseCloser = 0;` |
|   28259 | 1223 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 1224 | `			/* php names the token and expects ')'; the post-clause runs to the ')'. */` |
|     ! 0 | 1225 | `			rc = PH7_GenSyntaxError(pGen,pGen->pIn,"\")\"");` |
|     ! 0 | 1226 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1227 | `				return SXERR_ABORT;` |
|       - | 1228 | `			}` |
|     ! 0 | 1229 | `			return SXRET_OK;` |
|       - | 1230 | `		}` |
|   28259 | 1231 | `		RE_SWAP_DELIMITER(pGen);` |
|   28259 | 1232 | `		if( rc == SXERR_ABORT ){` |
|       - | 1233 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 1234 | `			return SXERR_ABORT;` |
|   28259 | 1235 | `		}else if( rc != SXERR_EMPTY){` |
|       - | 1236 | `			/* Pop operand lvalue */` |
|   28259 | 1237 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|   14127 | 1238 | `		}` |
|   14127 | 1239 | `	}` |
|       - | 1240 | `	/* Emit the unconditional jump to the start of the loop */` |
|   28259 | 1241 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - | 1242 | `	/* Fix all jumps now the destination is resolved */` |
|   28259 | 1243 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 1244 | `	/* Release the loop block */` |
|   28259 | 1245 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 1246 | `	/* Statement successfully compiled */` |
|   28259 | 1247 | `	return SXRET_OK;` |
|   14134 | 1248 | `}` |
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
|   52988 | 1259 | `static int GenStateKeywordIsName(SyToken *pStart,SyToken *pCur)` |
|       5 | 1260 | `{` |
|       - | 1261 | `	SyToken *pPrev;` |
|   52993 | 1262 | `	if( pCur <= pStart ){` |
|     ! 0 | 1263 | `		return 0;` |
|       - | 1264 | `	}` |
|   52993 | 1265 | `	pPrev = pCur - 1;` |
|   52993 | 1266 | `	if( pPrev->nType & PH7_TK_DOLLAR ){` |
|       3 | 1267 | `		return 1;` |
|       - | 1268 | `	}` |
|   52991 | 1269 | `	if( (pPrev->nType & PH7_TK_OP) == 0 ){` |
|   52983 | 1270 | `		return 0;` |
|       - | 1271 | `	}` |
|      13 | 1272 | `	return (pPrev->sData.nByte == sizeof("->")-1` |
|       6 | 1273 | `			&& SyMemcmp(pPrev->sData.zString,"->",sizeof("->")-1) == 0)` |
|       7 | 1274 | `		\|\| (pPrev->sData.nByte == sizeof("::")-1` |
|       3 | 1275 | `			&& SyMemcmp(pPrev->sData.zString,"::",sizeof("::")-1) == 0)` |
|      16 | 1276 | `		\|\| (pPrev->sData.nByte == sizeof("?->")-1` |
|       4 | 1277 | `			&& SyMemcmp(pPrev->sData.zString,"?->",sizeof("?->")-1) == 0);` |
|   26499 | 1278 | `}` |
|       - | 1279 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - | 1280 | ` *` |
|       - | 1281 | `` * php's `as` target is any WRITABLE expression, not just a variable: a property`` |
|       - | 1282 | ` * ($o->p), a static property (C::$s), an array element ($a['k']) and an append` |
|       - | 1283 | ` * ($a[]) are all accepted, on the key side as much as on the value side. The` |
|       - | 1284 | ` * three shapes php rejects get php's own wording; everything else keeps PH7's.` |
|       - | 1285 | ` */` |
|   76746 | 1286 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 | 1287 | `{` |
|   76751 | 1288 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|   76751 | 1289 | `	const char *zMsg = 0;` |
|       - | 1290 | ``	/* A loop target is a write target: `as $this` and `as (new A)->p` are php's`` |
|       - | 1291 | `	 * own compile fatals. */` |
|   76751 | 1292 | `	rc = GenStateWriteTargetCheck(&(*pGen),pRoot,0);` |
|   76751 | 1293 | `	if( rc != SXRET_OK ){` |
|       3 | 1294 | `		return rc;` |
|       - | 1295 | `	}` |
|   76749 | 1296 | `	if( pRoot->pOp ){` |
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
|   76710 | 1311 | `	}else if( pRoot->xCode == PH7_CompileVariable ){` |
|   76715 | 1312 | `		return SXRET_OK;` |
|       - | 1313 | `	}` |
|       - | 1314 | `	/* Unexpected expression */` |
|     ! 0 | 1315 | `	rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|     ! 0 | 1316 | `		zMsg ? zMsg : "foreach: Expecting a variable name");` |
|     ! 0 | 1317 | `	if( rc != SXERR_ABORT ){` |
|     ! 0 | 1318 | `		rc = SXERR_INVALID;` |
|     ! 0 | 1319 | `	}` |
|     ! 0 | 1320 | `	return rc;` |
|   38378 | 1321 | `}` |
|       - | 1322 | `/*` |
|       - | 1323 | `` * Is this `as` target the plain `$name` shape?`` |
|       - | 1324 | ` *` |
|       - | 1325 | ` * Only that shape can be installed by NAME the way ph7_foreach_info records it` |
|       - | 1326 | ` * (the step writes straight into the frame's symbol table). Every other writable` |
|       - | 1327 | ` * target — including a variable-variable, whose name php re-evaluates per step —` |
|       - | 1328 | ` * goes through a synthetic temporary plus a real store (GenStateForeachStoreTarget).` |
|       - | 1329 | ` */` |
|   76746 | 1330 | `static int GenStateForeachTargetIsPlainVar(SyToken *pStart,SyToken *pEnd)` |
|       5 | 1331 | `{` |
|  115106 | 1332 | `	return (pEnd == &pStart[2])` |
|   76728 | 1333 | `		&& (pStart[0].nType & PH7_TK_DOLLAR)` |
|  115101 | 1334 | `		&& (pStart[1].nType & PH7_TK_ID);` |
|       5 | 1335 | `}` |
|       - | 1336 | `/*` |
|       - | 1337 | `` * Reserve the synthetic temporary a complex `as` target's step value lands in.`` |
|       - | 1338 | ` * The bracketed name cannot collide with a user variable — the same trick the` |
|       - | 1339 | ` * list()/[...] destructuring path uses.` |
|       - | 1340 | ` */` |
|      68 | 1341 | `static sxi32 GenStateForeachTempName(ph7_gen_state *pGen,const char *zTag,SyString *pOut)` |
|       3 | 1342 | `{` |
|       - | 1343 | `	static int iForeachTargetCnt = 0;` |
|       - | 1344 | `	char zTmp[128];` |
|       - | 1345 | `	sxu32 nLen;` |
|       - | 1346 | `	char *zDup;` |
|      71 | 1347 | `	nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_%s_%d__]",zTag,iForeachTargetCnt++);` |
|      71 | 1348 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      71 | 1349 | `	if( zDup == 0 ){` |
|     ! 0 | 1350 | `		return SXERR_ABORT;` |
|       - | 1351 | `	}` |
|      71 | 1352 | `	SyStringInitFromBuf(pOut,zDup,nLen);` |
|      71 | 1353 | `	return SXRET_OK;` |
|      37 | 1354 | `}` |
|       - | 1355 | `/*` |
|       - | 1356 | `` * Emit `<target> = <temp>` (or `<target> =& <temp>` for a by-reference value) for`` |
|       - | 1357 | `` * one complex `as` target, at the top of the loop body — where php performs the`` |
|       - | 1358 | ` * assignment, once per step.` |
|       - | 1359 | ` *` |
|       - | 1360 | ` * The store is folded exactly as the assignment operator's own codegen folds it` |
|       - | 1361 | ` * (compile.c, precedence-18 site): a member LHS keeps its OP_MEMBER, a subscript` |
|       - | 1362 | ` * becomes STORE_IDX, and a plain name folds into the STORE's p3.` |
|       - | 1363 | ` */` |
|      68 | 1364 | `static sxi32 GenStateForeachStoreTarget(` |
|       - | 1365 | `	ph7_gen_state *pGen,` |
|       - | 1366 | `	SyString *pTemp,   /* Synthetic variable holding this step's value/key */` |
|       - | 1367 | `	SyToken *pStart,   /* Target expression token range */` |
|       - | 1368 | `	SyToken *pEnd,` |
|       - | 1369 | `	int bRef           /* True for a by-reference value target */` |
|       - | 1370 | `	)` |
|       3 | 1371 | `{` |
|      71 | 1372 | `	SyToken *pSavedIn = pGen->pIn;` |
|      71 | 1373 | `	SyToken *pSavedEnd = pGen->pEnd;` |
|      71 | 1374 | `	sxi32 iVmOp = bRef ? PH7_OP_STORE_REF : PH7_OP_STORE;` |
|       - | 1375 | `	VmInstr *pInstr;` |
|      71 | 1376 | `	sxi32 iP1 = 0;` |
|      71 | 1377 | `	sxi32 iP2 = 0;` |
|      71 | 1378 | `	void *p3 = 0;` |
|       - | 1379 | `	sxi32 rc;` |
|       - | 1380 | `	/* The value being stored, below the target — the operand order OP_STORE expects. */` |
|      71 | 1381 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(pTemp),0);` |
|      71 | 1382 | `	pGen->pIn = pStart;` |
|      71 | 1383 | `	pGen->pEnd = pEnd;` |
|      71 | 1384 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE,` |
|       - | 1385 | `		GenStateForEachNodeValidator);` |
|      71 | 1386 | `	pGen->pIn = pSavedIn;` |
|      71 | 1387 | `	pGen->pEnd = pSavedEnd;` |
|      71 | 1388 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 1389 | `		return SXERR_ABORT;` |
|      71 | 1390 | `	}else if( rc != SXRET_OK ){` |
|       - | 1391 | `		/* The validator already reported it; drop the pushed value and carry on. */` |
|     ! 0 | 1392 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 | 1393 | `		return SXRET_OK;` |
|       - | 1394 | `	}` |
|      71 | 1395 | `	pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      71 | 1396 | `	if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 1397 | `		/* A member target resolves (and, for a reference, stashes) its own slot. */` |
|      19 | 1398 | `		if( bRef ){` |
|       3 | 1399 | `			pInstr->iP2 = PH7_MEMBER_REF_TARGET;` |
|       1 | 1400 | `		}` |
|      19 | 1401 | `		iP2 = 1;` |
|      62 | 1402 | `	}else if( pInstr ){` |
|      53 | 1403 | `		(void)PH7_VmPopInstr(pGen->pVm);` |
|      53 | 1404 | `		if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|      17 | 1405 | `			iVmOp = bRef ? PH7_OP_STORE_IDX_REF : PH7_OP_STORE_IDX;` |
|      17 | 1406 | `			iP1 = pInstr->iP1;` |
|      17 | 1407 | `			if( bRef ){` |
|       3 | 1408 | `				iP2 = pInstr->iP2;` |
|       3 | 1409 | `				p3 = pInstr->p3;` |
|       1 | 1410 | `			}` |
|       9 | 1411 | `		}else{` |
|      37 | 1412 | `			p3 = pInstr->p3;` |
|       - | 1413 | `		}` |
|      25 | 1414 | `	}` |
|      71 | 1415 | `	PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|       - | 1416 | `	/* Discard the stored value the store leaves behind */` |
|      71 | 1417 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      71 | 1418 | `	if( bRef ){` |
|       - | 1419 | `		/* The target now holds the element; drop the temporary's own hold, or it` |
|       - | 1420 | `		 * would keep the element a REFERENCE for the rest of the script — an extra` |
|       - | 1421 | ``		 * holder no `unset()` the program can write is able to reach. Dropping the`` |
|       - | 1422 | `		 * NAME never releases the slot the target still refers to. */` |
|       5 | 1423 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UNSET_VAR,0,0,(void *)pTemp,0);` |
|       2 | 1424 | `	}` |
|      71 | 1425 | `	return SXRET_OK;` |
|      37 | 1426 | `}` |
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
|   52978 | 1453 | `PH7_PRIVATE sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       5 | 1454 | `{` |
|   52983 | 1455 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|   52983 | 1456 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|       - | 1457 | ``	/* Token ranges of a KEY / VALUE target that is not a plain `$name`: it is`` |
|       - | 1458 | `	 * compiled as a real store at the top of the loop body (php assigns the value` |
|       - | 1459 | `	 * first, then the key), against a synthetic temporary the step writes. */` |
|   52983 | 1460 | `	SyToken *pKeyStart = 0,*pKeyEnd = 0;` |
|   52983 | 1461 | `	SyToken *pValStart = 0,*pValEnd = 0;` |
|   52983 | 1462 | `	GenBlock *pForeachBlock = 0;` |
|       - | 1463 | `	ph7_foreach_info *pInfo;` |
|       - | 1464 | `	sxu32 nFalseJump;` |
|       - | 1465 | `	VmInstr *pInstr;` |
|       - | 1466 | `	sxu32 nLine;` |
|       - | 1467 | `	sxi32 rc;` |
|   52983 | 1468 | `	nLine = pGen->pIn->nLine;` |
|       - | 1469 | `	/* Jump the 'foreach' keyword */` |
|   52983 | 1470 | `	pGen->pIn++;` |
|   52983 | 1471 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 1472 | `		/* Syntax error */` |
|     ! 0 | 1473 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 | 1474 | `		if( rc == SXERR_ABORT ){` |
|       - | 1475 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1476 | `			return SXERR_ABORT;` |
|       - | 1477 | `		}` |
|     ! 0 | 1478 | `		goto Synchronize;` |
|       - | 1479 | `	}` |
|       - | 1480 | `	/* Jump the left parenthesis '(' */` |
|   52983 | 1481 | `	pGen->pIn++;` |
|       - | 1482 | `	/* Create the loop block */` |
|   52983 | 1483 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|   52983 | 1484 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1485 | `		return SXERR_ABORT;` |
|       - | 1486 | `	}` |
|       - | 1487 | `	/* Delimit the expression */` |
|   52983 | 1488 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   52983 | 1489 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
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
|       - | 1503 | `	/* Compile the array expression */` |
|   52983 | 1504 | `	pCur = pGen->pIn;` |
|  185537 | 1505 | `	while( pCur < pEnd ){` |
|  185537 | 1506 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|   67113 | 1507 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|   67113 | 1508 | `			if( nKeywrd == PH7_TKWRD_AS && !GenStateKeywordIsName(pGen->pIn,pCur) ){` |
|       - | 1509 | `				/* Break with the first 'as' that is the SEPARATOR — one that is` |
|       - | 1510 | `				 * part of a name ($as, $o->as, A::as) is not one. */` |
|   52983 | 1511 | `				break;` |
|       - | 1512 | `			}` |
|    7065 | 1513 | `		}` |
|       - | 1514 | `		/* Advance the stream cursor */` |
|  132559 | 1515 | `		pCur++;` |
|       5 | 1516 | `	}` |
|   52983 | 1517 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 | 1518 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - | 1519 | `			"foreach: Missing array/object expression");` |
|     ! 0 | 1520 | `		if( rc == SXERR_ABORT ){` |
|       - | 1521 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 1522 | `			return SXERR_ABORT;` |
|       - | 1523 | `		}` |
|     ! 0 | 1524 | `		goto Synchronize;` |
|       - | 1525 | `	}` |
|       - | 1526 | `	/* Swap token streams */` |
|   52983 | 1527 | `	pTmp = pGen->pEnd;` |
|   52983 | 1528 | `	pGen->pEnd = pCur;` |
|   52983 | 1529 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   52983 | 1530 | `	if( rc == SXERR_ABORT ){` |
|       - | 1531 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 1532 | `		return SXERR_ABORT;` |
|       - | 1533 | `	}` |
|       - | 1534 | `	/* Update token stream */` |
|   52983 | 1535 | `	while(pGen->pIn < pCur ){` |
|     ! 0 | 1536 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 1537 | `		if( rc == SXERR_ABORT ){` |
|       - | 1538 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 1539 | `			return SXERR_ABORT;` |
|       - | 1540 | `		}` |
|     ! 0 | 1541 | `		pGen->pIn++;` |
|     ! 0 | 1542 | `	}` |
|   52983 | 1543 | `	pCur++; /* Jump the 'as' keyword */` |
|   52983 | 1544 | `	pGen->pIn = pCur;` |
|   52983 | 1545 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 1546 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 | 1547 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1548 | `			return SXERR_ABORT;` |
|       - | 1549 | `		}` |
|     ! 0 | 1550 | `	}` |
|       - | 1551 | `	/* Create the foreach context */` |
|   52983 | 1552 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|   52983 | 1553 | `	if( pInfo == 0 ){` |
|     ! 0 | 1554 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 1555 | `		return SXERR_ABORT;` |
|       - | 1556 | `	}` |
|       - | 1557 | `	/* Zero the structure */` |
|   52983 | 1558 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - | 1559 | `	/* Initialize structure fields */` |
|   52983 | 1560 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - | 1561 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|       - | 1562 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|       - | 1563 | `	 * '=>'. */` |
|   52983 | 1564 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|   52983 | 1565 | `	if( pCur < pEnd ){` |
|       - | 1566 | `		/* Compile the expression holding the key name */` |
|   23827 | 1567 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 | 1568 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 | 1569 | `			if( rc == SXERR_ABORT ){` |
|       - | 1570 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 1571 | `				return SXERR_ABORT;` |
|     ! 0 | 1572 | `			}` |
|   23827 | 1573 | `		}else if( !GenStateForeachTargetIsPlainVar(pGen->pIn,pCur) ){` |
|       - | 1574 | `			/* A writable but non-name key target ($o->k, C::$s, $a['k'], $$n): the` |
|       - | 1575 | `			 * step lands in a temporary and the store runs in the loop body. */` |
|      13 | 1576 | `			pKeyStart = pGen->pIn;` |
|      13 | 1577 | `			pKeyEnd = pCur;` |
|      13 | 1578 | `			if( GenStateForeachTempName(&(*pGen),"key",&pInfo->sKey) != SXRET_OK ){` |
|     ! 0 | 1579 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1580 | `				return SXERR_ABORT;` |
|       - | 1581 | `			}` |
|      13 | 1582 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       7 | 1583 | `		}else{` |
|   23815 | 1584 | `			pGen->pEnd = pCur;` |
|   23815 | 1585 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|   23815 | 1586 | `			if( rc == SXERR_ABORT ){` |
|       - | 1587 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 1588 | `				return SXERR_ABORT;` |
|       - | 1589 | `			}` |
|   23815 | 1590 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|   23815 | 1591 | `			if( pInstr->p3 ){` |
|       - | 1592 | `				/* Record key name */` |
|   23815 | 1593 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|   11905 | 1594 | `			}` |
|   23815 | 1595 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - | 1596 | `		}` |
|   23827 | 1597 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|   11911 | 1598 | `	}` |
|   52983 | 1599 | `	pGen->pEnd = pEnd;` |
|   52983 | 1600 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 1601 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 | 1602 | `		if( rc == SXERR_ABORT ){` |
|       - | 1603 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 1604 | `			return SXERR_ABORT;` |
|       - | 1605 | `		}` |
|     ! 0 | 1606 | `		goto Synchronize;` |
|       - | 1607 | `	}` |
|   52983 | 1608 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      63 | 1609 | `		pGen->pIn++;` |
|       - | 1610 | `		/* Pass by reference  */` |
|      63 | 1611 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|      30 | 1612 | `	}` |
|       - | 1613 | `	/* Check if the value target is list() */` |
|   52983 | 1614 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 | 1615 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - | 1616 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|       - | 1617 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|       - | 1618 | `		 */` |
|       - | 1619 | `		static int iForeachListCnt = 0;` |
|       - | 1620 | `		char zTmp[128];` |
|       - | 1621 | `		sxu32 nLen;` |
|       - | 1622 | `		char *zDup;` |
|      10 | 1623 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|      10 | 1624 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      10 | 1625 | `		if( zDup == 0 ){` |
|     ! 0 | 1626 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1627 | `			return SXERR_ABORT;` |
|       - | 1628 | `		}` |
|      10 | 1629 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - | 1630 | `		/* Save list() token boundaries */` |
|      10 | 1631 | `		pListStart = pGen->pIn;` |
|       - | 1632 | `		/* Advance past list(...) — validate parentheses */` |
|      10 | 1633 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|      10 | 1634 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 1635 | `			/* php: syntax error, unexpected variable "$x", expecting "(" */` |
|       3 | 1636 | `			rc = PH7_GenSyntaxError(pGen,pGen->pIn < pEnd ? pGen->pIn : 0,"\"(\"");` |
|       3 | 1637 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1638 | `				return SXERR_ABORT;` |
|       - | 1639 | `			}` |
|       3 | 1640 | `			goto Synchronize;` |
|       - | 1641 | `		}` |
|       7 | 1642 | `		pGen->pIn++; /* Jump '(' */` |
|       7 | 1643 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       7 | 1644 | `		if( pListEnd >= pEnd ){` |
|     ! 0 | 1645 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 1646 | `				"foreach: Missing closing ')' after list");` |
|     ! 0 | 1647 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1648 | `				return SXERR_ABORT;` |
|       - | 1649 | `			}` |
|     ! 0 | 1650 | `			goto Synchronize;` |
|       - | 1651 | `		}` |
|       7 | 1652 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|       7 | 1653 | `		pListEnd = pGen->pIn;` |
|       7 | 1654 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|   52978 | 1655 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - | 1656 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|       - | 1657 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|       - | 1658 | `		 */` |
|       - | 1659 | `		static int iForeachShortListCnt = 0;` |
|       - | 1660 | `		char zTmp[128];` |
|       - | 1661 | `		sxu32 nLen;` |
|       - | 1662 | `		char *zDup;` |
|      49 | 1663 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|      49 | 1664 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      49 | 1665 | `		if( zDup == 0 ){` |
|     ! 0 | 1666 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1667 | `			return SXERR_ABORT;` |
|       - | 1668 | `		}` |
|      49 | 1669 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - | 1670 | `		/* Save [...] token boundaries */` |
|      49 | 1671 | `		pListStart = pGen->pIn;` |
|       - | 1672 | `		/* Advance past [...] */` |
|      49 | 1673 | `		pGen->pIn++; /* Jump '[' */` |
|      49 | 1674 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|      49 | 1675 | `		if( pListEnd >= pEnd ){` |
|     ! 0 | 1676 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 1677 | `				"foreach: Missing closing ']' after short list");` |
|     ! 0 | 1678 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1679 | `				return SXERR_ABORT;` |
|       - | 1680 | `			}` |
|     ! 0 | 1681 | `			goto Synchronize;` |
|       - | 1682 | `		}` |
|      49 | 1683 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|      49 | 1684 | `		pListEnd = pGen->pIn;` |
|      49 | 1685 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|   52952 | 1686 | `	}else if( !GenStateForeachTargetIsPlainVar(pGen->pIn,pEnd) ){` |
|       - | 1687 | `		/* A writable but non-name value target — same treatment as the key above. */` |
|      59 | 1688 | `		pValStart = pGen->pIn;` |
|      59 | 1689 | `		pValEnd = pEnd;` |
|      59 | 1690 | `		if( GenStateForeachTempName(&(*pGen),"val",&pInfo->sValue) != SXRET_OK ){` |
|     ! 0 | 1691 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1692 | `			return SXERR_ABORT;` |
|       - | 1693 | `		}` |
|      31 | 1694 | `	}else{` |
|       - | 1695 | `		/* Compile the expression holding the value name */` |
|   52873 | 1696 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|   52873 | 1697 | `		if( rc == SXERR_ABORT ){` |
|       - | 1698 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 1699 | `			return SXERR_ABORT;` |
|       - | 1700 | `		}` |
|   52873 | 1701 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|   52873 | 1702 | `		if( pInstr->p3 ){` |
|       - | 1703 | `			/* Record value name */` |
|   52873 | 1704 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|   26434 | 1705 | `		}` |
|       - | 1706 | `	}` |
|       - | 1707 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|   52981 | 1708 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - | 1709 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   52981 | 1710 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - | 1711 | `	/* Record the first instruction to execute */` |
|   52981 | 1712 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - | 1713 | `	/* Emit the FOREACH_STEP instruction */` |
|   52981 | 1714 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - | 1715 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   52981 | 1716 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - | 1717 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|   52981 | 1718 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|       - | 1719 | `		SyToken *pSavedIn,*pSavedEnd;` |
|       - | 1720 | `		/* Load the temporary variable holding the current value onto the stack.` |
|       - | 1721 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|       - | 1722 | `		 */` |
|      55 | 1723 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|       - | 1724 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|       - | 1725 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|       - | 1726 | `		 * picks up the delimiter and the variable names inside.` |
|       - | 1727 | `		 */` |
|      55 | 1728 | `		pSavedIn = pGen->pIn;` |
|      55 | 1729 | `		pSavedEnd = pGen->pEnd;` |
|      55 | 1730 | `		pGen->pIn = pListStart;` |
|      55 | 1731 | `		pGen->pEnd = pListEnd;` |
|      55 | 1732 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|      49 | 1733 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|      26 | 1734 | `		}else{` |
|       7 | 1735 | `			rc = PH7_CompileList(&(*pGen),0);` |
|       - | 1736 | `		}` |
|      55 | 1737 | `		pGen->pIn = pSavedIn;` |
|      55 | 1738 | `		pGen->pEnd = pSavedEnd;` |
|      55 | 1739 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1740 | `			return SXERR_ABORT;` |
|       - | 1741 | `		}` |
|       - | 1742 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|      55 | 1743 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      26 | 1744 | `	}` |
|       - | 1745 | `	/* Store this step's value and key into their non-name targets. php performs the` |
|       - | 1746 | `	 * VALUE assignment first — visible through a __set() pair, and the order the` |
|       - | 1747 | `	 * symbol table records the two locals in for the plain-name shape. */` |
|   52981 | 1748 | `	if( pValStart ){` |
|      87 | 1749 | `		rc = GenStateForeachStoreTarget(&(*pGen),&pInfo->sValue,pValStart,pValEnd,` |
|      56 | 1750 | `			(pInfo->iFlags & PH7_4EACH_STEP_REF) != 0);` |
|      59 | 1751 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1752 | `			return SXERR_ABORT;` |
|       - | 1753 | `		}` |
|      28 | 1754 | `	}` |
|   52981 | 1755 | `	if( pKeyStart ){` |
|      13 | 1756 | `		rc = GenStateForeachStoreTarget(&(*pGen),&pInfo->sKey,pKeyStart,pKeyEnd,0);` |
|      13 | 1757 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1758 | `			return SXERR_ABORT;` |
|       - | 1759 | `		}` |
|       6 | 1760 | `	}` |
|       - | 1761 | `	/* Compile the loop body */` |
|   52981 | 1762 | `	pGen->pIn = &pEnd[1];` |
|   52981 | 1763 | `	pGen->pEnd = pTmp;` |
|   52981 | 1764 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|   52981 | 1765 | `	if( rc == SXERR_ABORT ){` |
|       - | 1766 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 1767 | `		return SXERR_ABORT;` |
|       - | 1768 | `	}` |
|       - | 1769 | `	/* Emit the unconditional jump to the start of the loop */` |
|   52981 | 1770 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - | 1771 | `	/* Fix all jumps now the destination is resolved */` |
|   52981 | 1772 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 1773 | `	/* Release the loop block */` |
|   52981 | 1774 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 1775 | `	/* Statement successfully compiled */` |
|   52981 | 1776 | `	return SXRET_OK;` |
|       1 | 1777 | `Synchronize:` |
|       - | 1778 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 1779 | `	 * compiling this erroneous block.` |
|       - | 1780 | `	 */` |
|       3 | 1781 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 1782 | `		pGen->pIn++;` |
|     ! 0 | 1783 | `	}` |
|       3 | 1784 | `	return SXRET_OK;` |
|   26494 | 1785 | `}` |
|       - | 1786 | `/*` |
|       - | 1787 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - | 1788 | ` * According to the PHP language reference` |
|       - | 1789 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - | 1790 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - | 1791 | ` *  that is similar to that of C:` |
|       - | 1792 | ` *  if (expr)` |
|       - | 1793 | ` *   statement` |
|       - | 1794 | ` *  else construct:` |
|       - | 1795 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - | 1796 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - | 1797 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - | 1798 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - | 1799 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - | 1800 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - | 1801 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - | 1802 | ` *  elseif` |
|       - | 1803 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - | 1804 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - | 1805 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - | 1806 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - | 1807 | ` *   than b, a equal to b or a is smaller than b:` |
|       - | 1808 | ` *   <?php` |
|       - | 1809 | ` *    if ($a > $b) {` |
|       - | 1810 | ` *     echo "a is bigger than b";` |
|       - | 1811 | ` *    } elseif ($a == $b) {` |
|       - | 1812 | ` *     echo "a is equal to b";` |
|       - | 1813 | ` *    } else {` |
|       - | 1814 | ` *     echo "a is smaller than b";` |
|       - | 1815 | ` *    }` |
|       - | 1816 | ` *    ?>` |
|       - | 1817 | ` */` |
|  300652 | 1818 | `PH7_PRIVATE sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       5 | 1819 | `{` |
|  300657 | 1820 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  300657 | 1821 | `	GenBlock *pCondBlock = 0;` |
|       - | 1822 | `	sxu32 nJumpIdx;` |
|       - | 1823 | `	sxu32 nKeyID;` |
|       - | 1824 | `	sxi32 rc;` |
|       - | 1825 | `	/* Jump the 'if' keyword */` |
|  300657 | 1826 | `	pGen->pIn++;` |
|  300657 | 1827 | `	pToken = pGen->pIn;` |
|       - | 1828 | `	/* Create the conditional block */` |
|  300657 | 1829 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  300657 | 1830 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1831 | `		return SXERR_ABORT;` |
|       - | 1832 | `	}` |
|       - | 1833 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|  169086 | 1834 | `	for(;;){` |
|  338177 | 1835 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 1836 | `			/* Syntax error */` |
|     ! 0 | 1837 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 1838 | `				pToken--;` |
|     ! 0 | 1839 | `			}` |
|     ! 0 | 1840 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 | 1841 | `			if( rc == SXERR_ABORT ){` |
|       - | 1842 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 1843 | `				return SXERR_ABORT;` |
|       - | 1844 | `			}` |
|     ! 0 | 1845 | `			goto Synchronize;` |
|       - | 1846 | `		}` |
|       - | 1847 | `		/* Jump the left parenthesis '(' */` |
|  338177 | 1848 | `		pToken++;` |
|       - | 1849 | `		/* Delimit the condition */` |
|  338177 | 1850 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  338177 | 1851 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - | 1852 | `			/* Syntax error */` |
|     ! 0 | 1853 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 1854 | `				pToken--;` |
|     ! 0 | 1855 | `			}` |
|     ! 0 | 1856 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 | 1857 | `			if( rc == SXERR_ABORT ){` |
|       - | 1858 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 1859 | `				return SXERR_ABORT;` |
|       - | 1860 | `			}` |
|     ! 0 | 1861 | `			goto Synchronize;` |
|       - | 1862 | `		}` |
|       - | 1863 | `		/* Swap token streams */` |
|  338177 | 1864 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - | 1865 | `		/* Compile the condition */` |
|  338177 | 1866 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 1867 | `		/* Update token stream */` |
|  338177 | 1868 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 | 1869 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|     ! 0 | 1870 | `			pGen->pIn++;` |
|     ! 0 | 1871 | `		}` |
|  338177 | 1872 | `		pGen->pIn  = &pEnd[1];` |
|  338177 | 1873 | `		pGen->pEnd = pTmp;` |
|  338177 | 1874 | `		if( rc == SXERR_ABORT ){` |
|       - | 1875 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       3 | 1876 | `			return SXERR_ABORT;` |
|       - | 1877 | `		}` |
|       - | 1878 | `		/* Emit the false jump */` |
|  338175 | 1879 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - | 1880 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  338175 | 1881 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - | 1882 | `		/* Compile the body */` |
|  338175 | 1883 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  338175 | 1884 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1885 | `			return SXERR_ABORT;` |
|       - | 1886 | `		}` |
|  338175 | 1887 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   77551 | 1888 | `			break;` |
|       - | 1889 | `		}` |
|       - | 1890 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|  183083 | 1891 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  183083 | 1892 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|  117109 | 1893 | `			break;` |
|       - | 1894 | `		}` |
|       - | 1895 | `		/* Emit the unconditional jump */` |
|   65979 | 1896 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - | 1897 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   65979 | 1898 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   65979 | 1899 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   42469 | 1900 | `			pToken = &pGen->pIn[1];` |
|   42469 | 1901 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|   14048 | 1902 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|   14232 | 1903 | `					break;` |
|       - | 1904 | `			}` |
|   14015 | 1905 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    7005 | 1906 | `		}` |
|   37525 | 1907 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - | 1908 | `		/* Synchronize cursors */` |
|   37525 | 1909 | `		pToken = pGen->pIn;` |
|       - | 1910 | `		/* Fix the false jump */` |
|   37525 | 1911 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       5 | 1912 | `	} /* For(;;) */` |
|       - | 1913 | `	/* Fix the false jump */` |
|  300655 | 1914 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  300655 | 1915 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|  145558 | 1916 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - | 1917 | `			/* Compile the else block */` |
|   28459 | 1918 | `			pGen->pIn++;` |
|   28459 | 1919 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   28459 | 1920 | `			if( rc == SXERR_ABORT ){` |
|       - | 1921 |  |
|     ! 0 | 1922 | `				return SXERR_ABORT;` |
|       - | 1923 | `			}` |
|   14227 | 1924 | `	}` |
|  300655 | 1925 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - | 1926 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  300655 | 1927 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - | 1928 | `	/* Release the conditional block */` |
|  300655 | 1929 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 1930 | `	/* Statement successfully compiled */` |
|  300655 | 1931 | `	return SXRET_OK;` |
|     ! 0 | 1932 | `Synchronize:` |
|       - | 1933 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - | 1934 | `	 */` |
|     ! 0 | 1935 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 1936 | `		pGen->pIn++;` |
|     ! 0 | 1937 | `	}` |
|     ! 0 | 1938 | `	return SXRET_OK;` |
|  150331 | 1939 | `}` |
|       - | 1940 | `/*` |
|       - | 1941 | ` * Compile the global construct.` |
|       - | 1942 | ` * According to the PHP language reference` |
|       - | 1943 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - | 1944 | ` *  to be used in that function.` |
|       - | 1945 | ` *  Example #1 Using global` |
|       - | 1946 | ` *  <?php` |
|       - | 1947 | ` *   $a = 1;` |
|       - | 1948 | ` *   $b = 2;` |
|       - | 1949 | ` *   function Sum()` |
|       - | 1950 | ` *   {` |
|       - | 1951 | ` *    global $a, $b;` |
|       - | 1952 | ` *    $b = $a + $b;` |
|       - | 1953 | ` *   }` |
|       - | 1954 | ` *   Sum();` |
|       - | 1955 | ` *   echo $b;` |
|       - | 1956 | ` *  ?>` |
|       - | 1957 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - | 1958 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - | 1959 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - | 1960 | ` */` |
|      62 | 1961 | `PH7_PRIVATE sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       5 | 1962 | `{` |
|      67 | 1963 | `	SyToken *pTmp,*pNext = 0;` |
|       - | 1964 | `	sxi32 nExpr;` |
|       - | 1965 | `	sxi32 rc;` |
|       - | 1966 | `	/* Jump the 'global' keyword */` |
|      67 | 1967 | `	pGen->pIn++;` |
|      67 | 1968 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - | 1969 | `		/* Nothing to process */` |
|     ! 0 | 1970 | `		return SXRET_OK;` |
|       - | 1971 | `	}` |
|      67 | 1972 | `	pTmp = pGen->pEnd;` |
|      67 | 1973 | `	nExpr = 0;` |
|     147 | 1974 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      85 | 1975 | `		if( pGen->pIn < pNext ){` |
|      85 | 1976 | `			pGen->pEnd = pNext;` |
|      85 | 1977 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 1978 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 | 1979 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1980 | `					return SXERR_ABORT;` |
|       - | 1981 | `				}` |
|      80 | 1982 | `			}else if( &pGen->pIn[1] < pGen->pEnd` |
|      80 | 1983 | `			 && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|      75 | 1984 | `			 && pGen->pIn[1].sData.nByte == sizeof("this")-1` |
|      43 | 1985 | `			 && SyMemcmp((const void *)pGen->pIn[1].sData.zString,` |
|       3 | 1986 | `			             (const void *)"this",sizeof("this")-1) == 0 ){` |
|       - | 1987 | ``				/* php refuses `global $this;` at the declaration. */`` |
|       3 | 1988 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - | 1989 | `					"Cannot use $this as global variable");` |
|       3 | 1990 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1991 | `					return SXERR_ABORT;` |
|       - | 1992 | `				}` |
|       2 | 1993 | `			}else{` |
|      83 | 1994 | `				pGen->pIn++;` |
|      83 | 1995 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - | 1996 | `					/* Emit a warning */` |
|     ! 0 | 1997 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 | 1998 | `				}else{` |
|      83 | 1999 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      83 | 2000 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 2001 | `						return SXERR_ABORT;` |
|      83 | 2002 | `					}else if(rc != SXERR_EMPTY ){` |
|      83 | 2003 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      83 | 2004 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|       - | 2005 | `							/* Variable name, not a constant */` |
|      73 | 2006 | `							pLast->iP1 = 0;` |
|      34 | 2007 | `						}` |
|      83 | 2008 | `						nExpr++;` |
|      39 | 2009 | `					}` |
|       - | 2010 | `				}` |
|       - | 2011 | `			}` |
|      40 | 2012 | `		}` |
|       - | 2013 | `		/* Next expression in the stream */` |
|      85 | 2014 | `		pGen->pIn = pNext;` |
|       - | 2015 | `		/* Jump trailing commas */` |
|     103 | 2016 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|      23 | 2017 | `			pGen->pIn++;` |
|       5 | 2018 | `		}` |
|       5 | 2019 | `	}` |
|       - | 2020 | `	/* Restore token stream */` |
|      67 | 2021 | `	pGen->pEnd = pTmp;` |
|      67 | 2022 | `	if( nExpr > 0 ){` |
|       - | 2023 | `		/* Emit the uplink instruction */` |
|      65 | 2024 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|      30 | 2025 | `	}` |
|      67 | 2026 | `	return SXRET_OK;` |
|      36 | 2027 | `}` |
|       - | 2028 | `/*` |
|       - | 2029 | ` * php's NOUN for a compile-time return diagnostic is the LEXICAL scope, not the` |
|       - | 2030 | `` * kind of function the `return` sits in: zend reads CG(active_class_entry), so a`` |
|       - | 2031 | ` * closure written inside a class body reports "method" and the very same closure` |
|       - | 2032 | ` * written at file scope reports "function". pCurClass is the compiler's exact` |
|       - | 2033 | ` * counterpart (the class/interface/trait/enum whose BODY is being compiled).` |
|       - | 2034 | ` */` |
|      26 | 2035 | `static const char * GenStateReturnNoun(ph7_gen_state *pGen)` |
|       4 | 2036 | `{` |
|      30 | 2037 | `	return pGen->pCurClass ? "method" : "function";` |
|       4 | 2038 | `}` |
|       - | 2039 | `/*` |
|       - | 2040 | ` * TRUE when a declared return type ACCEPTS null — the condition under which php` |
|       - | 2041 | `` * appends its `did you mean "return null;"` hint to the missing-value error.`` |
|       - | 2042 | ``  * That is every nullable declaration (`?T`, `T\|null`, and the standalone `null` `` |
|       - | 2043 | `` * type, all of which set VM_FUNC_RETURN_NULLABLE) plus `mixed`, which includes`` |
|       - | 2044 | ` * null but is stored as a pseudo-CLASS atom rather than through the flag.` |
|       - | 2045 | ` */` |
|      10 | 2046 | `static int GenStateReturnTypeAllowsNull(ph7_vm_func *pFunc)` |
|       4 | 2047 | `{` |
|       - | 2048 | `	SyString *pCls;` |
|      14 | 2049 | `	if( pFunc->iFlags & VM_FUNC_RETURN_NULLABLE ){` |
|       3 | 2050 | `		return 1;` |
|       - | 2051 | `	}` |
|      11 | 2052 | `	pCls = &pFunc->sReturnClass;` |
|       8 | 2053 | `	if( pFunc->nReturnType == SXU32_HIGH && pCls->nByte == sizeof("mixed")-1` |
|       3 | 2054 | `	 && SyStrnicmp(pCls->zString,"mixed",sizeof("mixed")-1) == 0 ){` |
|     ! 0 | 2055 | `		return 1;` |
|       - | 2056 | `	}` |
|      11 | 2057 | `	return 0;` |
|       9 | 2058 | `}` |
|       - | 2059 | `/*` |
|       - | 2060 | ` * Compile the return statement.` |
|       - | 2061 | ` * According to the PHP language reference` |
|       - | 2062 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - | 2063 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - | 2064 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - | 2065 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - | 2066 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - | 2067 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - | 2068 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - | 2069 | ` *  from within the main script file, then script execution end.` |
|       - | 2070 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - | 2071 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - | 2072 | ` *  should do so as PHP has less work to do in this case.` |
|       - | 2073 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - | 2074 | ` */` |
|  243646 | 2075 | `PH7_PRIVATE sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       5 | 2076 | `{` |
|  243651 | 2077 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - | 2078 | `	sxi32 rc;` |
|  243651 | 2079 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  243651 | 2080 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|       - | 2081 | `	ph7_vm_func *pFunc;` |
|       - | 2082 | `	sxu32 nInstrBefore;` |
|       - | 2083 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|       - | 2084 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|       - | 2085 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|       - | 2086 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|       - | 2087 | `	 * normally below so token processing stays consistent. */` |
|  749975 | 2088 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|  506329 | 2089 | `		pFuncBlock = pFuncBlock->pParent;` |
|       5 | 2090 | `	}` |
|  243651 | 2091 | `	pFunc = pFuncBlock ? (ph7_vm_func *)pFuncBlock->pUserData : 0;` |
|  243651 | 2092 | `	if( pFunc && pFunc->nReturnType == MEMOBJ_NEVER ){` |
|       8 | 2093 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       2 | 2094 | `			"A never-returning %s must not return", GenStateReturnNoun(pGen));` |
|       6 | 2095 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2096 | `			return SXERR_ABORT;` |
|       - | 2097 | `		}` |
|       2 | 2098 | `	}` |
|       - | 2099 | `	/* Jump the 'return' keyword */` |
|  243651 | 2100 | `	pGen->pIn++;` |
|  243651 | 2101 | `	nInstrBefore = PH7_VmInstrLength(pGen->pVm);` |
|  243651 | 2102 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2103 | ``		/* php: a stray token after `return EXPR` is `... expecting ";"`. */`` |
|  243601 | 2104 | `		const char *zSave = pGen->zClauseCloser;` |
|  243601 | 2105 | `		pGen->zClauseCloser = "\";\"";` |
|       - | 2106 | ``		/* A `return` READS its operand (the value is consumed), so compile it`` |
|       - | 2107 | ``		 * read-only: a lone undefined variable `return $z` must warn at the read`` |
|       - | 2108 | `		 * exactly like echo/interpolation, not be loaded quietly (the same quiet` |
|       - | 2109 | ``		 * load that correctly keeps a bare `$z;` statement silent). Matches the`` |
|       - | 2110 | `		 * arrow-fn implicit-return body fix. */` |
|  243601 | 2111 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|  243601 | 2112 | `		pGen->zClauseCloser = zSave;` |
|  243601 | 2113 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2114 | `			return SXERR_ABORT;` |
|  243601 | 2115 | `		}else if(rc != SXERR_EMPTY ){` |
|  243601 | 2116 | `			nRet = 1;` |
|  121798 | 2117 | `		}` |
|  121798 | 2118 | `	}` |
|       - | 2119 | ``	/* A bare `return;` inside a function that DECLARES a return type is a php`` |
|       - | 2120 | `	 * COMPILE error, not the runtime TypeError PHL used to raise on the way out:` |
|       - | 2121 | `` 	 * php rejects the program before it runs. `void` (which is what `return;` `` |
|       - | 2122 | ``	 * means) and `never` (handled above) are the two declarations exempt from it,`` |
|       - | 2123 | ``	 * and a GENERATOR is exempt whatever it declares — there `return;` ends the`` |
|       - | 2124 | `	 * generator, and the declared type describes the Generator object the call` |
|       - | 2125 | `	 * produced, never the returned value. */` |
|  243646 | 2126 | `	if( nRet == 0 && pFunc && !pGen->bInGenerator && VmFuncHasReturnType(pFunc)` |
|      34 | 2127 | `	 && pFunc->nReturnType != MEMOBJ_VOID && pFunc->nReturnType != MEMOBJ_NEVER ){` |
|      19 | 2128 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - | 2129 | `			"A %s with return type must return a value%s",` |
|       5 | 2130 | `			GenStateReturnNoun(pGen),` |
|      10 | 2131 | `			GenStateReturnTypeAllowsNull(pFunc)` |
|       - | 2132 | `				? " (did you mean \"return null;\" instead of \"return;\"?)" : "");` |
|      14 | 2133 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2134 | `			return SXERR_ABORT;` |
|       - | 2135 | `		}` |
|       5 | 2136 | `	}` |
|       - | 2137 | ``	/* The mirror rule: a `void` function must not return a VALUE, and php stops`` |
|       - | 2138 | `	 * the program at the return statement rather than throwing on the way out.` |
|       - | 2139 | `	 * Generators keep their own diagnostic (a void generator is rejected as` |
|       - | 2140 | ``	 * `Generator return type must be a supertype of Generator`), so they are`` |
|       - | 2141 | `	 * skipped here exactly as above. php's hint fires when the operand is a` |
|       - | 2142 | ``	 * compile-time constant null; PHL folds the `null` KEYWORD (constant index 0,`` |
|       - | 2143 | `	 * emitted as a lone OP_LOADC and unchanged by any wrapping parens), which is` |
|       - | 2144 | `	 * every shape real code writes. */` |
|  243646 | 2145 | `	if( nRet != 0 && pFunc && !pGen->bInGenerator` |
|  243560 | 2146 | `	 && pFunc->nReturnType == MEMOBJ_VOID ){` |
|      16 | 2147 | `		VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      22 | 2148 | `		int bNullLiteral = (PH7_VmInstrLength(pGen->pVm) == nInstrBefore + 1)` |
|      12 | 2149 | `			&& pLast && pLast->iOp == PH7_OP_LOADC` |
|      18 | 2150 | `			&& pLast->iP1 == 0 && pLast->iP2 == 0;` |
|      22 | 2151 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - | 2152 | `			"A void %s must not return a value%s",` |
|       6 | 2153 | `			GenStateReturnNoun(pGen),` |
|       6 | 2154 | `			bNullLiteral` |
|       - | 2155 | `				? " (did you mean \"return;\" instead of \"return null;\"?)" : "");` |
|      16 | 2156 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2157 | `			return SXERR_ABORT;` |
|       - | 2158 | `		}` |
|       6 | 2159 | `	}` |
|       - | 2160 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|       - | 2161 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|       - | 2162 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|       - | 2163 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|  243651 | 2164 | `	if( pGen->bInGenerator ){` |
|      46 | 2165 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|      46 | 2166 | `		return SXRET_OK;` |
|       - | 2167 | `	}` |
|       - | 2168 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|       - | 2169 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|       - | 2170 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|       - | 2171 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|       - | 2172 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|  243609 | 2173 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|  243609 | 2174 | `	return SXRET_OK;` |
|  121828 | 2175 | `}` |
|       - | 2176 | `/*` |
|       - | 2177 | ` * Compile a yield expression.` |
|       - | 2178 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - | 2179 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - | 2180 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - | 2181 | ` */` |
|     476 | 2182 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       5 | 2183 | `{` |
|       - | 2184 | `	SyToken *pTmp, *pSplit;` |
|     481 | 2185 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|     481 | 2186 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - | 2187 | `	sxi32 rc;` |
|     238 | 2188 | `	(void)iCompileFlag;` |
|       - | 2189 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|     481 | 2190 | `	pGen->pIn++;` |
|       - | 2191 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - | 2192 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|       - | 2193 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|       - | 2194 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|       - | 2195 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|     476 | 2196 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|     275 | 2197 | `		&& pGen->pIn->sData.nByte == 4` |
|      75 | 2198 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|      69 | 2199 | `		pGen->pIn++; /* Skip 'from' */` |
|      69 | 2200 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      69 | 2201 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2202 | `			return SXERR_ABORT;` |
|       - | 2203 | `		}` |
|      69 | 2204 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 | 2205 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|     ! 0 | 2206 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|       - | 2207 | `				"Missing expression after 'yield from'");` |
|     ! 0 | 2208 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2209 | `				return SXERR_ABORT;` |
|       - | 2210 | `			}` |
|     ! 0 | 2211 | `		}` |
|      69 | 2212 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|      69 | 2213 | `		return SXRET_OK;` |
|       - | 2214 | `	}` |
|     417 | 2215 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 2216 | `		/* Bare yield — no value */` |
|       3 | 2217 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|       3 | 2218 | `		return SXRET_OK;` |
|       - | 2219 | `	}` |
|       - | 2220 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|     415 | 2221 | `	pSplit = 0;` |
|       - | 2222 | `	{` |
|     415 | 2223 | `		SyToken *pCur = pGen->pIn;` |
|     415 | 2224 | `		sxi32 nNest = 0;` |
|    1021 | 2225 | `		while( pCur < pGen->pEnd ){` |
|     625 | 2226 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      29 | 2227 | `				nNest++;` |
|     612 | 2228 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      29 | 2229 | `				nNest--;` |
|     586 | 2230 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|      16 | 2231 | `				pSplit = pCur;` |
|      16 | 2232 | `				break;` |
|       - | 2233 | `			}` |
|     611 | 2234 | `			pCur++;` |
|       5 | 2235 | `		}` |
|       - | 2236 | `	}` |
|     415 | 2237 | `	pTmp = pGen->pEnd;` |
|     415 | 2238 | `	if( pSplit ){` |
|       - | 2239 | `		/* yield $key => $value */` |
|      16 | 2240 | `		pGen->pEnd = pSplit;` |
|      16 | 2241 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      16 | 2242 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      16 | 2243 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|      16 | 2244 | `		pGen->pEnd = pTmp;` |
|      16 | 2245 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      16 | 2246 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      16 | 2247 | `		iP1 = 1;` |
|      16 | 2248 | `		iP2 = 1;` |
|       9 | 2249 | `	}else{` |
|       - | 2250 | `		/* yield $value */` |
|     401 | 2251 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     401 | 2252 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     401 | 2253 | `		if( rc != SXERR_EMPTY ){` |
|     401 | 2254 | `			iP1 = 1;` |
|     198 | 2255 | `		}` |
|       - | 2256 | `	}` |
|     415 | 2257 | `	pGen->pEnd = pTmp;` |
|     415 | 2258 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|     415 | 2259 | `	return SXRET_OK;` |
|     243 | 2260 | `}` |
|       - | 2261 | `/*` |
|       - | 2262 | ` * Compile the die/exit language construct.` |
|       - | 2263 | ` * The role of these constructs is to terminate execution of the script.` |
|       - | 2264 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - | 2265 | ` */` |
|      94 | 2266 | `PH7_PRIVATE sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       5 | 2267 | `{` |
|      99 | 2268 | `	sxi32 nExpr = 0;` |
|       - | 2269 | `	sxi32 rc;` |
|       - | 2270 | `	/* Jump the die/exit keyword */` |
|      99 | 2271 | `	pGen->pIn++;` |
|      99 | 2272 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2273 | `		/* Compile the expression */` |
|      99 | 2274 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      99 | 2275 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2276 | `			return SXERR_ABORT;` |
|      99 | 2277 | `		}else if(rc != SXERR_EMPTY ){` |
|      99 | 2278 | `			nExpr = 1;` |
|      47 | 2279 | `		}` |
|      47 | 2280 | `	}` |
|       - | 2281 | `	/* Emit the HALT instruction */` |
|      99 | 2282 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|      99 | 2283 | `	return SXRET_OK;` |
|      52 | 2284 | `}` |
|       - | 2285 | `/*` |
|       - | 2286 | ` * Compile the 'echo' language construct.` |
|       - | 2287 | ` */` |
|   23116 | 2288 | `PH7_PRIVATE sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       5 | 2289 | `{` |
|   23121 | 2290 | `	SyToken *pTmp,*pNext = 0;` |
|   23121 | 2291 | `	sxu32 nLine = pGen->pIn->nLine;` |
|   23121 | 2292 | `	int nExpr = 0;      /* expressions actually compiled */` |
|   23121 | 2293 | `	int bExpectMore = 1;/* after 'echo' or a comma an expression is REQUIRED */` |
|       - | 2294 | `	sxi32 rc;` |
|       - | 2295 | `	/* Jump the 'echo' keyword */` |
|   23121 | 2296 | `	pGen->pIn++;` |
|       - | 2297 | `	/* Compile arguments one after one. php: a stray token in an echo list is` |
|       - | 2298 | ``	 * `... expecting "," or ";"` — set the closer for each argument's compile. */`` |
|   23121 | 2299 | `	pTmp = pGen->pEnd;` |
|       - | 2300 | `	{` |
|   23121 | 2301 | `	const char *zSaveEcho = pGen->zClauseCloser;` |
|   64063 | 2302 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   40953 | 2303 | `		if( pGen->pIn < pNext ){` |
|   40953 | 2304 | `			pGen->pEnd = pNext;` |
|   40953 | 2305 | `			pGen->zClauseCloser = "\",\" or \";\"";` |
|   40953 | 2306 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   40953 | 2307 | `			pGen->zClauseCloser = zSaveEcho;` |
|   40953 | 2308 | `			if( rc == SXERR_ABORT ){` |
|       6 | 2309 | `				return SXERR_ABORT;` |
|   40949 | 2310 | `			}else if( rc != SXERR_EMPTY ){` |
|       - | 2311 | `				/* Emit the consume instruction */` |
|   40925 | 2312 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|   40925 | 2313 | `				nExpr++;` |
|   40925 | 2314 | `				bExpectMore = 0;` |
|   20460 | 2315 | `			}` |
|   20472 | 2316 | `		}` |
|       - | 2317 | `		/* Jump trailing commas (php: exactly one between expressions; a` |
|       - | 2318 | `		 * dangling or doubled comma is a parse error, enforced below) */` |
|   58787 | 2319 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|   17845 | 2320 | `			if( bExpectMore ){` |
|       - | 2321 | `				/* two commas in a row */` |
|       3 | 2322 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,pNext->nLine,` |
|       - | 2323 | `					"syntax error, unexpected token \",\"");` |
|       3 | 2324 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - | 2325 | `			}` |
|   17843 | 2326 | `			bExpectMore = 1;` |
|   17843 | 2327 | `			pNext++;` |
|       5 | 2328 | `		}` |
|   40947 | 2329 | `		pGen->pIn = pNext;` |
|       5 | 2330 | `	}` |
|       - | 2331 | `	}` |
|       - | 2332 | `	/* Restore token stream */` |
|   23115 | 2333 | `	pGen->pEnd = pTmp;` |
|   23115 | 2334 | `	if( nExpr == 0 \|\| bExpectMore ){` |
|       - | 2335 | ``		/* `echo ;` or `echo expr, ;` — php rejects both */`` |
|      32 | 2336 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - | 2337 | `			"syntax error, unexpected token \";\"");` |
|      32 | 2338 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - | 2339 | `	}` |
|   23087 | 2340 | `	return SXRET_OK;` |
|   11563 | 2341 | `}` |
|       - | 2342 | `/*` |
|       - | 2343 | ` * Compile the static statement.` |
|       - | 2344 | ` * According to the PHP language reference` |
|       - | 2345 | ` *  Another important feature of variable scoping is the static variable.` |
|       - | 2346 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - | 2347 | ` *  when program execution leaves this scope.` |
|       - | 2348 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - | 2349 | ` * Symisc eXtension.` |
|       - | 2350 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - | 2351 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 2352 | ` *  Example` |
|       - | 2353 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - | 2354 | ` *    Refer to the official documentation for more information on this feature.` |
|       - | 2355 | ` */` |
|      30 | 2356 | `PH7_PRIVATE sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       3 | 2357 | `{` |
|       - | 2358 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - | 2359 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - | 2360 | `	GenBlock *pBlock;` |
|       - | 2361 | `	SyString *pName;` |
|       - | 2362 | `	char *zDup;` |
|       - | 2363 | `	sxu32 nLine;` |
|       - | 2364 | `	sxi32 rc;` |
|       - | 2365 | ``	/* `static function () {}` / `static fn () =>` at statement position is an`` |
|       - | 2366 | `	 * EXPRESSION statement (a bare static closure), not a static-variable` |
|       - | 2367 | `	 * declaration — hand it to the expression compiler (php accepts it). */` |
|      30 | 2368 | `	if( &pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & PH7_TK_KEYWORD)` |
|      19 | 2369 | `	 && (SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FUNCTION` |
|       1 | 2370 | `	  \|\| SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FN) ){` |
|       3 | 2371 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       3 | 2372 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2373 | `			return SXERR_ABORT;` |
|       3 | 2374 | `		}else if( rc != SXERR_EMPTY ){` |
|       3 | 2375 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 2376 | `		}` |
|       3 | 2377 | `		return SXRET_OK;` |
|       - | 2378 | `	}` |
|       - | 2379 | `	/* Jump the static keyword */` |
|      31 | 2380 | `	nLine = pGen->pIn->nLine;` |
|      31 | 2381 | `	pGen->pIn++;` |
|       - | 2382 | `	/* Extract the enclosing function if any */` |
|      31 | 2383 | `	pBlock = pGen->pCurrent;` |
|      59 | 2384 | `	while( pBlock ){` |
|      59 | 2385 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|      31 | 2386 | `			break;` |
|       - | 2387 | `		}` |
|       - | 2388 | `		/* Point to the upper block */` |
|      31 | 2389 | `		pBlock = pBlock->pParent;` |
|       3 | 2390 | `	}` |
|      31 | 2391 | `	if( pBlock == 0 ){` |
|       - | 2392 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 | 2393 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       - | 2394 | ``			/* php: `static FOO;` is a syntax error naming FOO and expecting "::"`` |
|       - | 2395 | ``			 * (the parser is still open to `static::` at that point). */`` |
|     ! 0 | 2396 | `			rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"::\"");` |
|     ! 0 | 2397 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2398 | `				return SXERR_ABORT;` |
|       - | 2399 | `			}` |
|     ! 0 | 2400 | `			goto Synchronize;` |
|       - | 2401 | `		}` |
|       - | 2402 | `		/* Compile the expression holding the variable */` |
|     ! 0 | 2403 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 | 2404 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2405 | `			return SXERR_ABORT;` |
|     ! 0 | 2406 | `		}else if( rc != SXERR_EMPTY ){` |
|       - | 2407 | `			/* Emit the POP instruction */` |
|     ! 0 | 2408 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 | 2409 | `		}` |
|     ! 0 | 2410 | `		return SXRET_OK;` |
|       - | 2411 | `	}` |
|      31 | 2412 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - | 2413 | `	/* Make sure we are dealing with a valid statement */` |
|      31 | 2414 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      26 | 2415 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 2416 | ``			/* php: `static FOO;` is a syntax error naming FOO and expecting "::"`` |
|       - | 2417 | ``			 * (the parser is still open to `static::` at that point). */`` |
|       3 | 2418 | `			rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"::\"");` |
|       3 | 2419 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2420 | `				return SXERR_ABORT;` |
|       - | 2421 | `			}` |
|       3 | 2422 | `			goto Synchronize;` |
|       - | 2423 | `	}` |
|      29 | 2424 | `	pGen->pIn++;` |
|       - | 2425 | `	/* Extract variable name */` |
|      29 | 2426 | `	pName = &pGen->pIn->sData;` |
|       - | 2427 | ``	/* php refuses `static $this;` at the declaration — the name is not a slot a`` |
|       - | 2428 | `	 * function may own. */` |
|      26 | 2429 | `	if( pName->nByte == sizeof("this")-1` |
|      19 | 2430 | `	 && SyMemcmp((const void *)pName->zString,(const void *)"this",sizeof("this")-1) == 0 ){` |
|       3 | 2431 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - | 2432 | `			"Cannot use $this as static variable");` |
|       3 | 2433 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2434 | `			return SXERR_ABORT;` |
|       - | 2435 | `		}` |
|       3 | 2436 | `		goto Synchronize;` |
|       - | 2437 | `	}` |
|      27 | 2438 | `	pGen->pIn++; /* Jump the var name */` |
|      27 | 2439 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 | 2440 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2441 | `		goto Synchronize;` |
|       - | 2442 | `	}` |
|       - | 2443 | `	/* Initialize the structure describing the static variable */` |
|      27 | 2444 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      27 | 2445 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - | 2446 | `	/* Duplicate variable name */` |
|      27 | 2447 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      27 | 2448 | `	if( zDup == 0 ){` |
|     ! 0 | 2449 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2450 | `		return SXERR_ABORT;` |
|       - | 2451 | `	}` |
|      27 | 2452 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - | 2453 | `	/* Check if we have an expression to compile */` |
|      27 | 2454 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - | 2455 | `		SySet *pInstrContainer;` |
|       - | 2456 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - | 2457 | `		 * Static variable can take any complex expression including function` |
|       - | 2458 | `		 * call as their initialization value.` |
|       - | 2459 | `		 * Example:` |
|       - | 2460 | `		 *		static $var = foo(1,4+5,bar());` |
|       - | 2461 | `		 */` |
|      27 | 2462 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - | 2463 | `		/* Swap bytecode container */` |
|      27 | 2464 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      27 | 2465 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - | 2466 | `		/* Compile the expression */` |
|      27 | 2467 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 2468 | `		/* Emit the done instruction */` |
|      27 | 2469 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - | 2470 | `		/* Restore default bytecode container */` |
|      27 | 2471 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      12 | 2472 | `	}` |
|       - | 2473 | `	/* Finally save the compiled static variable in the appropriate container */` |
|      27 | 2474 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|      27 | 2475 | `	return SXRET_OK;` |
|       2 | 2476 | `Synchronize:` |
|       - | 2477 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - | 2478 | `	 * statement.` |
|       - | 2479 | `	 */` |
|      10 | 2480 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       6 | 2481 | `		pGen->pIn++;` |
|       2 | 2482 | `	}` |
|       6 | 2483 | `	return SXRET_OK;` |
|      18 | 2484 | `}` |
|       - | 2485 | `/*` |
|       - | 2486 | ` * Compile the var statement.` |
|       - | 2487 | ` * Symisc Extension:` |
|       - | 2488 | ` *      var statement can be used outside of a class definition.` |
|       - | 2489 | ` */` |
|       2 | 2490 | `PH7_PRIVATE sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 | 2491 | `{` |
|       - | 2492 | `` 	/* `var` is a PROPERTY modifier and nothing else. A class body's `var $x;` `` |
|       - | 2493 | `	 * is compiled by compile_class.c, never here, so reaching this statement` |
|       - | 2494 | ``	 * handler means `var` appeared outside a class — which php rejects as a`` |
|       - | 2495 | `	 * parse error. PHL used to compile it as an ordinary expression statement,` |
|       - | 2496 | ``	 * so top-level `var $x = 1;` silently ran (a Symisc extension). */`` |
|       3 | 2497 | `	PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       3 | 2498 | `	return SXERR_ABORT;` |
|       1 | 2499 | `}` |
|       - | 2500 | `/*` |
|       - | 2501 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - | 2502 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - | 2503 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - | 2504 | ` */` |
|       - | 2505 | `/*` |
|       - | 2506 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|       - | 2507 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - | 2508 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - | 2509 | ` * qualified name and updates the instruction's operand index.` |
|       - | 2510 | ` *` |
|       - | 2511 | ` * Resolution order:` |
|       - | 2512 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|       - | 2513 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|       - | 2514 | ` *   3. Otherwise return the original literal index unchanged.` |
|       - | 2515 | ` *` |
|       - | 2516 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|       - | 2517 | ` * came from an import (step 1) and 0 otherwise.` |
|       - | 2518 | ` * Returns the (possibly new) literal index.` |
|       - | 2519 | ` */` |
|  653756 | 2520 | `PH7_PRIVATE sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|       5 | 2521 | `{` |
|       - | 2522 | `	ph7_value *pLit;` |
|       - | 2523 | `	const char *zLit;` |
|       - | 2524 | `	SyString sQualified;` |
|       - | 2525 | `	sxu32 nLit;` |
|       - | 2526 | `	sxu32 k;` |
|       - | 2527 | `	sxu32 nNewIdx;` |
|       - | 2528 | `	int hasNsSep;` |
|       - | 2529 | `	SyHashEntry *pImport;` |
|       - | 2530 | `	ph7_value *pNew;` |
|  653761 | 2531 | `	if( pFromImport ){` |
|  574731 | 2532 | `		*pFromImport = 0;` |
|  287363 | 2533 | `	}` |
|  653761 | 2534 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  653761 | 2535 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 | 2536 | `		return nOrigIdx;` |
|       - | 2537 | `	}` |
|  653761 | 2538 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  653761 | 2539 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - | 2540 | `	/* Skip if already qualified (contains backslash) */` |
|  653761 | 2541 | `	hasNsSep = 0;` |
| 6231135 | 2542 | `	for( k = 0; k < nLit; k++ ){` |
| 5577429 | 2543 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 2788692 | 2544 | `	}` |
|  653761 | 2545 | `	if( hasNsSep ){` |
|      54 | 2546 | `		return nOrigIdx;` |
|       - | 2547 | `	}` |
|       - | 2548 | `	/* Check use imports first (works even outside namespaces) */` |
|  653711 | 2549 | `	SyBlobReset(&pGen->sWorker);` |
|  653711 | 2550 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  653711 | 2551 | `	if( pImport ){` |
|      82 | 2552 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      82 | 2553 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      82 | 2554 | `		if( pFromImport ){` |
|      34 | 2555 | `			*pFromImport = 1;` |
|      15 | 2556 | `		}` |
|      43 | 2557 | `	}else{` |
|  653633 | 2558 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  653413 | 2559 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|       - | 2560 | `		}` |
|       - | 2561 | `		/* Prepend current namespace */` |
|     225 | 2562 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     225 | 2563 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|     225 | 2564 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - | 2565 | `	}` |
|       - | 2566 | `	/* Look up or create a new literal for the qualified name */` |
|     303 | 2567 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|     303 | 2568 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|     121 | 2569 | `		return nNewIdx; /* Already interned */` |
|       - | 2570 | `	}` |
|     187 | 2571 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|     187 | 2572 | `	if( pNew == 0 ){` |
|     ! 0 | 2573 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - | 2574 | `	}` |
|     187 | 2575 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|     187 | 2576 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|     187 | 2577 | `	return nNewIdx;` |
|  326883 | 2578 | `}` |
|       - | 2579 | `/*` |
|       - | 2580 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - | 2581 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - | 2582 | ` */` |
|    4698 | 2583 | `PH7_PRIVATE void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 | 2584 | `{` |
|       - | 2585 | `	SyHashEntry *pImport;` |
|    4703 | 2586 | `	const char *zName = pName->zString;` |
|    4703 | 2587 | `	sxu32 nName = pName->nByte;` |
|    4703 | 2588 | `	sxu32 nFirst = 0;` |
|       - | 2589 | `	/* php resolves a name through use-imports on its LEADING segment: an` |
|       - | 2590 | ``	 * unqualified `C` maps the whole name (`use X\C;` -> X\C), while a QUALIFIED`` |
|       - | 2591 | ``	 * `A\B\C` maps just `A` (`use X\A;` -> X\A\B\C) and keeps the `\B\C` tail.`` |
|       - | 2592 | `	 * The old code looked up the whole qualified string (which never matches a` |
|       - | 2593 | `	 * single-segment import alias) and then blindly prefixed the current` |
|       - | 2594 | ``	 * namespace, so `use PHPUnit\Framework; extends Framework\TestCase` resolved`` |
|       - | 2595 | `	 * to Ns\Framework\TestCase. Mirror GenStateResolveNamespaceLiteral here. */` |
|   45373 | 2596 | `	while( nFirst < nName && zName[nFirst] != '\\' ){ nFirst++; }` |
|    4703 | 2597 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)zName,nFirst);` |
|    4703 | 2598 | `	if( pImport ){` |
|      72 | 2599 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      72 | 2600 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|      72 | 2601 | `		SyBlobAppend(pOut,zName + nFirst,nName - nFirst); /* the \B\C tail, if any */` |
|      72 | 2602 | `		return;` |
|       - | 2603 | `	}` |
|       - | 2604 | `	/* Prepend current namespace if active */` |
|    4635 | 2605 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      27 | 2606 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      27 | 2607 | `		SyBlobAppend(pOut,"\\",1);` |
|      12 | 2608 | `	}` |
|    4635 | 2609 | `	SyBlobAppend(pOut,zName,nName);` |
|    2354 | 2610 | `}` |
|       - | 2611 | `/*` |
|       - | 2612 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - | 2613 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - | 2614 | ` * The caller must release pOut when done.` |
|       - | 2615 | ` */` |
|    7538 | 2616 | `PH7_PRIVATE void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 | 2617 | `{` |
|    7543 | 2618 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     477 | 2619 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     477 | 2620 | `		SyBlobAppend(pOut,"\\",1);` |
|     236 | 2621 | `	}` |
|    7543 | 2622 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    7543 | 2623 | `}` |
|       - | 2624 | `/*` |
|       - | 2625 | `` * php's `namespace\X` NAME OPERATOR (5.3): a leading `namespace` keyword glued to`` |
|       - | 2626 | `` * a `\` names the CURRENT namespace, and the whole name is then FULLY QUALIFIED —`` |
|       - | 2627 | `` * `namespace\X` inside `namespace B;` is `\B\X`, and plain `\X` at global scope.`` |
|       - | 2628 | `` * php's lexer matches it as one token (T_NAME_RELATIVE, `"namespace"("\\"{LABEL})+`,`` |
|       - | 2629 | `` * case-insensitively), so the `\` must be GLUED to the keyword: `namespace \X` is a`` |
|       - | 2630 | ` * php parse error, and this mirrors that by comparing source offsets.` |
|       - | 2631 | ` *` |
|       - | 2632 | ` * This predicate only RECOGNIZES the operator (it consumes nothing), which is what` |
|       - | 2633 | `` * the statement dispatcher needs to tell `namespace\X::m();` from a namespace`` |
|       - | 2634 | ` * DECLARATION; GenStateNsRelPrefix below is what the name collectors call.` |
|       - | 2635 | ` */` |
| 1631698 | 2636 | `PH7_PRIVATE int GenStateIsNsRelName(SyToken *pIn,SyToken *pEnd)` |
|       5 | 2637 | `{` |
| 1631698 | 2638 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
| 1291236 | 2639 | `	 \|\| (sxu32)SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_NAMESPACE ){` |
| 1631401 | 2640 | `		return 0;` |
|       - | 2641 | `	}` |
|     307 | 2642 | `	if( &pIn[1] >= pEnd \|\| (pIn[1].nType & PH7_TK_NSSEP) == 0 ){` |
|     213 | 2643 | `		return 0;` |
|       - | 2644 | `	}` |
|      96 | 2645 | `	if( &pIn[2] >= pEnd \|\| (pIn[2].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 2646 | ``		return 0; /* php's T_NAME_RELATIVE needs at least one segment after the `\` */`` |
|       - | 2647 | `	}` |
|       - | 2648 | `	/* Glued? The tokenizer drops whitespace, so adjacency is the source offsets. */` |
|      96 | 2649 | `	return pIn->sData.zString + pIn->sData.nByte == pIn[1].sData.zString;` |
|  815854 | 2650 | `}` |
|       - | 2651 | `/*` |
|       - | 2652 | `` * Consume a leading `namespace\` (see GenStateIsNsRelName) at *ppIn and seed pOut`` |
|       - | 2653 | ` * with the current namespace plus its separator — nothing at global scope, where` |
|       - | 2654 | ` * the bare name already IS the FQN. Returns TRUE when it fired, and the caller` |
|       - | 2655 | `` * must then treat the name it goes on to collect as ABSOLUTE: no `use` import may`` |
|       - | 2656 | ` * apply to it, and the current namespace is already in place.` |
|       - | 2657 | ` */` |
|   49920 | 2658 | `PH7_PRIVATE int GenStateNsRelPrefix(ph7_gen_state *pGen,SyToken **ppIn,SyToken *pEnd,SyBlob *pOut)` |
|       5 | 2659 | `{` |
|   49925 | 2660 | `	SyToken *pIn = *ppIn;` |
|   49925 | 2661 | `	if( !GenStateIsNsRelName(pIn,pEnd) ){` |
|   49837 | 2662 | `		return 0;` |
|       - | 2663 | `	}` |
|      90 | 2664 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      80 | 2665 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      80 | 2666 | `		SyBlobAppend(pOut,"\\",1);` |
|      39 | 2667 | `	}` |
|      90 | 2668 | `	*ppIn = &pIn[2];` |
|      90 | 2669 | `	return 1;` |
|   24965 | 2670 | `}` |
|       - | 2671 | `/*` |
|       - | 2672 | ` * Compile a namespace statement` |
|       - | 2673 | ` * According to the PHP language reference manual` |
|       - | 2674 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - | 2675 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - | 2676 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - | 2677 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - | 2678 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - | 2679 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - | 2680 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - | 2681 | ` *  programming world.` |
|       - | 2682 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - | 2683 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - | 2684 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - | 2685 | ` *  classes/functions/constants.` |
|       - | 2686 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - | 2687 | ` *  readability of source code.` |
|       - | 2688 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - | 2689 | ` *  Here is an example of namespace syntax in PHP:` |
|       - | 2690 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - | 2691 | ` *       class MyClass {}` |
|       - | 2692 | ` *       function myfunction() {}` |
|       - | 2693 | ` *       const MYCONST = 1;` |
|       - | 2694 | ` *       $a = new MyClass;` |
|       - | 2695 | ` *       $c = new \my\name\MyClass;` |
|       - | 2696 | ` *       $a = strlen('hi');` |
|       - | 2697 | ` *       $d = namespace\MYCONST;` |
|       - | 2698 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - | 2699 | ` *       echo constant($d);` |
|       - | 2700 | ` * NOTE` |
|       - | 2701 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - | 2702 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - | 2703 | ` */` |
|       - | 2704 | `/*` |
|       - | 2705 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - | 2706 | ` */` |
|      14 | 2707 | `PH7_PRIVATE const char * TokenTypeName(sxu32 nType)` |
|       4 | 2708 | `{` |
|      18 | 2709 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|      12 | 2710 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|      12 | 2711 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|      12 | 2712 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|      12 | 2713 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|      12 | 2714 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|       3 | 2715 | `	return "token";` |
|      11 | 2716 | `}` |
|     208 | 2717 | `PH7_PRIVATE sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       5 | 2718 | `{` |
|       - | 2719 | `	sxu32 nLine;` |
|       - | 2720 | `	sxi32 rc;` |
|     213 | 2721 | `	nLine = pGen->pIn->nLine;` |
|     213 | 2722 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - | 2723 | `	/* Reset namespace and clear previous use imports */` |
|     213 | 2724 | `	SyBlobReset(&pGen->sNamespace);` |
|     213 | 2725 | `	GenStateResetUseImports(&(*pGen),pGen->pVm);` |
|     213 | 2726 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 2727 | `		return SXRET_OK; /* Global namespace (bare "namespace;") */` |
|       - | 2728 | `	}` |
|     213 | 2729 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|     ! 0 | 2730 | `		return SXRET_OK; /* namespace; — switch to global namespace */` |
|       - | 2731 | `	}` |
|     213 | 2732 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|      10 | 2733 | `		return SXRET_OK; /* namespace { } — global namespace block */` |
|       - | 2734 | `	}` |
|       - | 2735 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     495 | 2736 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     295 | 2737 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - | 2738 | `			/* Append backslash separator */` |
|      51 | 2739 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      51 | 2740 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|      23 | 2741 | `			}` |
|      28 | 2742 | `		}else{` |
|       - | 2743 | `			/* Append identifier */` |
|     249 | 2744 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - | 2745 | `		}` |
|     295 | 2746 | `		pGen->pIn++;` |
|       5 | 2747 | `	}` |
|     205 | 2748 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       8 | 2749 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - | 2750 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 | 2751 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       6 | 2752 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2753 | `			return SXERR_ABORT;` |
|       - | 2754 | `		}` |
|       2 | 2755 | `	}` |
|     205 | 2756 | `	return SXRET_OK;` |
|     109 | 2757 | `}` |
|       - | 2758 | `/*` |
|       - | 2759 | ` * Initialize the three use-import tables of a code generator.` |
|       - | 2760 | ` *` |
|       - | 2761 | ` * php resolves CLASS and FUNCTION imports case-INSENSITIVELY, like every other` |
|       - | 2762 | ``  * name in those two families: `use A\Cee;` then `CEE::K`, `use A\Cee as Alias;` `` |
|       - | 2763 | `` * then `ALIAS::K`, `use function A\eff;` then `EFF()`, and a wrong-case leading`` |
|       - | 2764 | `` * segment of an imported namespace (`use A\B;` then `b\Cee::K`) all resolve.`` |
|       - | 2765 | ` * So both tables fold through SyStrHash/SyStrnmicmp, exactly like hClass /` |
|       - | 2766 | ` * hMethod / hFunction.` |
|       - | 2767 | ` *` |
|       - | 2768 | ` * The CONST table stays BYTE-EXACT: php keeps constant names case-sensitive,` |
|       - | 2769 | `` * so `use const A\KAY;` followed by `kay` must remain an undefined constant.`` |
|       - | 2770 | ` * That asymmetry is why the three tables exist separately.` |
|       - | 2771 | ` */` |
|   28522 | 2772 | `PH7_PRIVATE void GenStateInitUseImports(ph7_gen_state *pGen,ph7_vm *pVm)` |
|       5 | 2773 | `{` |
|   28527 | 2774 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|   28527 | 2775 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|   28527 | 2776 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|   28527 | 2777 | `}` |
|       - | 2778 | `/*` |
|       - | 2779 | ` * Drop every import currently in scope and start a fresh set (a namespace` |
|       - | 2780 | ` * switch clears imports).  Keeps the case rules of GenStateInitUseImports.` |
|       - | 2781 | ` */` |
|   23848 | 2782 | `PH7_PRIVATE void GenStateResetUseImports(ph7_gen_state *pGen,ph7_vm *pVm)` |
|       5 | 2783 | `{` |
|   23853 | 2784 | `	SyHashRelease(&pGen->hUseImports);` |
|   23853 | 2785 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   23853 | 2786 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   23853 | 2787 | `	GenStateInitUseImports(&(*pGen),&(*pVm));` |
|   23853 | 2788 | `}` |
|       - | 2789 | `/*` |
|       - | 2790 | ` * The two DECLARED-name tables (classes and functions declared so far in this` |
|       - | 2791 | ` * compile unit). php refuses an import whose name a declaration already took, and` |
|       - | 2792 | ` * the check is per COMPILE UNIT and case-INSENSITIVE — a class declared by a file` |
|       - | 2793 | `` * this one later `require`s is invisible to it, because that file compiles after`` |
|       - | 2794 | ` * this one has finished. Both tables key on the FQN, so they survive a namespace` |
|       - | 2795 | ` * switch (which clears only the imports).` |
|       - | 2796 | ` */` |
|   28314 | 2797 | `PH7_PRIVATE void GenStateInitSeenSymbols(ph7_gen_state *pGen,ph7_vm *pVm)` |
|       5 | 2798 | `{` |
|   28319 | 2799 | `	SyHashInit(&pGen->hSeenClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|   28319 | 2800 | `	SyHashInit(&pGen->hSeenFunc,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|   28319 | 2801 | `}` |
|   23644 | 2802 | `PH7_PRIVATE void GenStateReleaseSeenSymbols(ph7_gen_state *pGen)` |
|       5 | 2803 | `{` |
|   23649 | 2804 | `	SyHashRelease(&pGen->hSeenClass);` |
|   23649 | 2805 | `	SyHashRelease(&pGen->hSeenFunc);` |
|   23649 | 2806 | `}` |
|   23640 | 2807 | `PH7_PRIVATE void GenStateResetSeenSymbols(ph7_gen_state *pGen,ph7_vm *pVm)` |
|       5 | 2808 | `{` |
|   23645 | 2809 | `	GenStateReleaseSeenSymbols(&(*pGen));` |
|   23645 | 2810 | `	GenStateInitSeenSymbols(&(*pGen),&(*pVm));` |
|   23645 | 2811 | `}` |
|       - | 2812 | `/*` |
|       - | 2813 | ` * Record one declared CLASS (bFunc = 0) or FUNCTION (bFunc = 1) FQN so a later` |
|       - | 2814 | `` * `use` in this compile unit can see that the name is taken.`` |
|       - | 2815 | ` */` |
|  146110 | 2816 | `PH7_PRIVATE void GenStateRecordDeclaredName(ph7_gen_state *pGen,int bFunc,const SyString *pFqn)` |
|       5 | 2817 | `{` |
|  146115 | 2818 | `	SyHash *pHash = bFunc ? &pGen->hSeenFunc : &pGen->hSeenClass;` |
|       - | 2819 | `	char *zDup;` |
|  146115 | 2820 | `	if( pFqn->nByte < 1 \|\| SyHashGet(pHash,pFqn->zString,pFqn->nByte) != 0 ){` |
|      23 | 2821 | `		return;` |
|       - | 2822 | `	}` |
|  146097 | 2823 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pFqn->zString,pFqn->nByte);` |
|  146097 | 2824 | `	if( zDup ){` |
|       - | 2825 | `		/* The blob the caller built is released on its way out, so the table owns` |
|       - | 2826 | `		 * a pool copy (freed in bulk with the VM, like the import FQNs). */` |
|  146097 | 2827 | `		SyHashInsert(pHash,zDup,pFqn->nByte,zDup);` |
|   73046 | 2828 | `	}` |
|   73060 | 2829 | `}` |
|       - | 2830 | `/*` |
|       - | 2831 | `` * php refuses a DECLARATION whose short name a local `use` import already took:`` |
|       - | 2832 | ` *` |
|       - | 2833 | ` *   use A\Cee;  class Cee {}    Cannot redeclare class B\Cee (previously declared as local import)` |
|       - | 2834 | ` *   use function A\eff;  function eff(){}` |
|       - | 2835 | ` *                               Cannot redeclare function B\eff() (previously declared as local import)` |
|       - | 2836 | ` *   use const A\KAY;  const KAY = 1;` |
|       - | 2837 | ` *                               Cannot declare const B\KAY because the name is already in use` |
|       - | 2838 | ` *` |
|       - | 2839 | `` * A SELF-import (`use B\Cee;` inside `namespace B;`) names this very declaration`` |
|       - | 2840 | ` * and is a no-op, so it is exempt. iKind: 0 = class family (interface/trait/enum` |
|       - | 2841 | ` * included — php says "class" for all four), 1 = function, 2 = const.` |
|       - | 2842 | ` */` |
|  146244 | 2843 | `PH7_PRIVATE sxi32 GenStateGuardImportRedeclare(ph7_gen_state *pGen,int iKind,` |
|       - | 2844 | `	const SyString *pShort,const SyString *pFqn,sxu32 nLine)` |
|       5 | 2845 | `{` |
|       - | 2846 | `	SyHash *pImports;` |
|       - | 2847 | `	SyHashEntry *pEntry;` |
|       - | 2848 | `	const char *zImported;` |
|       - | 2849 | `	sxu32 nImported;` |
|  146249 | 2850 | `	switch( iKind ){` |
|  142487 | 2851 | `		case 1:  pImports = &pGen->hUseFuncImports; break;` |
|     139 | 2852 | `		case 2:  pImports = &pGen->hUseConstImports; break;` |
|    3633 | 2853 | `		default: pImports = &pGen->hUseImports; break;` |
|       - | 2854 | `	}` |
|  146249 | 2855 | `	pEntry = SyHashGet(pImports,(const void *)pShort->zString,pShort->nByte);` |
|  146249 | 2856 | `	if( pEntry == 0 ){` |
|  146235 | 2857 | `		return SXRET_OK;` |
|       - | 2858 | `	}` |
|      18 | 2859 | `	zImported = (const char *)pEntry->pUserData;` |
|      18 | 2860 | `	nImported = SyStrlen(zImported);` |
|      14 | 2861 | `	if( nImported == pFqn->nByte` |
|      22 | 2862 | `	 && (iKind == 2 ? SyMemcmp((const void *)zImported,(const void *)pFqn->zString,nImported) == 0` |
|       8 | 2863 | `	                : SyStrnicmp(zImported,pFqn->zString,nImported) == 0) ){` |
|       7 | 2864 | `		return SXRET_OK; /* the import IS this declaration */` |
|       - | 2865 | `	}` |
|      13 | 2866 | `	if( iKind == 2 ){` |
|       4 | 2867 | `		return PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       1 | 2868 | `			"Cannot declare const %z because the name is already in use",pFqn);` |
|       - | 2869 | `	}` |
|       8 | 2870 | `	return PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       2 | 2871 | `		iKind == 1 ? "Cannot redeclare function %z() (previously declared as local import)"` |
|       2 | 2872 | `		           : "Cannot redeclare class %z (previously declared as local import)",pFqn);` |
|   73125 | 2873 | `}` |
|       - | 2874 | `/*` |
|       - | 2875 | `` * Register one resolved `use` import: alias -> FQN, in the table its KIND owns`` |
|       - | 2876 | ` * (iUseType: 0 = class, 1 = function, 2 = const).  Shared by the plain form` |
|       - | 2877 | `` * (`use A\Cee;`) and by each member of a group (`use A\{Cee, Dee};`).`` |
|       - | 2878 | ` */` |
|     158 | 2879 | `static sxi32 GenStateAddImport(` |
|       - | 2880 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 2881 | `	int iUseType,         /* 0=class, 1=function, 2=const */` |
|       - | 2882 | `	SyBlob *pPath,        /* Fully qualified name being imported */` |
|       - | 2883 | `	SyString *pAlias,     /* Short name it is imported under */` |
|       - | 2884 | `	sxu32 nLine           /* Line of the 'use' keyword (for diagnostics) */` |
|       - | 2885 | `	)` |
|       5 | 2886 | `{` |
|       - | 2887 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|       - | 2888 | `	const char *zKind;  /* php's kind word in the "already in use" message */` |
|       - | 2889 | `	char *zDup;` |
|       - | 2890 | `	sxi32 rc;` |
|       - | 2891 | `	/* Select the target hash table based on import type. */` |
|     163 | 2892 | `	switch( iUseType ){` |
|      36 | 2893 | `		case 1:  pGenHash = &pGen->hUseFuncImports; break;` |
|      37 | 2894 | `		case 2:  pGenHash = &pGen->hUseConstImports; break;` |
|      99 | 2895 | `		default: pGenHash = &pGen->hUseImports; break;` |
|       - | 2896 | `	}` |
|       - | 2897 | `	/* php names the KIND of a non-class import in this message: "Cannot use` |
|       - | 2898 | `	 * function A\eff as eff …" / "Cannot use const A\KAY as KAY …". */` |
|     163 | 2899 | `	zKind = iUseType == 1 ? "function " : iUseType == 2 ? "const " : "";` |
|       - | 2900 | `	/* Check for duplicate import alias (per-type) */` |
|     163 | 2901 | `	if( SyHashGet(pGenHash,pAlias->zString,pAlias->nByte) != 0 ){` |
|      12 | 2902 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 2903 | `			"Cannot use %s%.*s as %z because the name is already in use",` |
|       6 | 2904 | `			zKind,(int)SyBlobLength(pPath),(const char *)SyBlobData(pPath),pAlias);` |
|       9 | 2905 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2906 | `			return SXERR_ABORT;` |
|       - | 2907 | `		}` |
|       3 | 2908 | `	}` |
|       - | 2909 | `	/* …and refuses one whose name a DECLARATION in this compile unit already took` |
|       - | 2910 | ``	 * (`class Cee {} use A\Cee;`), unless the import names that very declaration.`` |
|       - | 2911 | `	 * The name an import occupies is the alias in the CURRENT namespace, which is` |
|       - | 2912 | `	 * what the seen tables key on. php runs this check for classes and functions` |
|       - | 2913 | ``	 * only — a `const` declaration followed by its own `use const` is accepted. */`` |
|     163 | 2914 | `	if( iUseType != 2 ){` |
|       - | 2915 | `		SyBlob sTaken;` |
|     131 | 2916 | `		SyBlobInit(&sTaken,&pGen->pVm->sAllocator);` |
|     131 | 2917 | `		GenStateBuildFQN(&(*pGen),pAlias,&sTaken);` |
|     126 | 2918 | `		if( SyHashGet(iUseType == 1 ? &pGen->hSeenFunc : &pGen->hSeenClass,` |
|     189 | 2919 | `				SyBlobData(&sTaken),SyBlobLength(&sTaken)) != 0` |
|      70 | 2920 | `		 && (SyBlobLength(&sTaken) != SyBlobLength(pPath)` |
|       4 | 2921 | `			\|\| SyStrnicmp((const char *)SyBlobData(&sTaken),(const char *)SyBlobData(pPath),` |
|       6 | 2922 | `				(sxu32)SyBlobLength(&sTaken)) != 0) ){` |
|       8 | 2923 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 2924 | `				"Cannot use %s%.*s as %z because the name is already in use",` |
|       4 | 2925 | `				zKind,(int)SyBlobLength(pPath),(const char *)SyBlobData(pPath),pAlias);` |
|       6 | 2926 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2927 | `				SyBlobRelease(&sTaken);` |
|     ! 0 | 2928 | `				return SXERR_ABORT;` |
|       - | 2929 | `			}` |
|       2 | 2930 | `		}` |
|     131 | 2931 | `		SyBlobRelease(&sTaken);` |
|      63 | 2932 | `	}` |
|       - | 2933 | `	/* Register the import: alias -> FQN.` |
|       - | 2934 | `	 * Strings are allocated from the VM pool allocator and freed` |
|       - | 2935 | `	 * when the entire VM is released. SyHashRelease does not free` |
|       - | 2936 | `	 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|     242 | 2937 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     158 | 2938 | `		(const char *)SyBlobData(pPath),SyBlobLength(pPath));` |
|     163 | 2939 | `	if( zDup ){` |
|       - | 2940 | `		/* All three kinds resolve entirely at COMPILE time — a const import is read` |
|       - | 2941 | `		 * by the OP_LOADC candidate builder (compile_node.c), so no runtime table` |
|       - | 2942 | `		 * is needed for it either. */` |
|     163 | 2943 | `		SyHashInsert(pGenHash,pAlias->zString,pAlias->nByte,zDup);` |
|      79 | 2944 | `	}` |
|     163 | 2945 | `	return SXRET_OK;` |
|      84 | 2946 | `}` |
|       - | 2947 | `/*` |
|       - | 2948 | `` * Collect one `\`-separated name into pOut (appending to whatever it holds, with`` |
|       - | 2949 | ` * a separator when needed) and return its LAST segment token, or 0 when the` |
|       - | 2950 | ` * cursor is not on a name at all.` |
|       - | 2951 | ` */` |
|     172 | 2952 | `static SyToken * GenStateCollectNsPath(ph7_gen_state *pGen,SyBlob *pOut)` |
|       5 | 2953 | `{` |
|     177 | 2954 | `	SyToken *pLast = 0;` |
|     611 | 2955 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|     439 | 2956 | `		if( pGen->pIn->nType & PH7_TK_ID ){` |
|     301 | 2957 | `			pLast = pGen->pIn;` |
|     301 | 2958 | `			if( SyBlobLength(pOut) > 0 ){` |
|     155 | 2959 | `				SyBlobAppend(pOut,"\\",1);` |
|      75 | 2960 | `			}` |
|     301 | 2961 | `			SyBlobAppend(pOut,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|     148 | 2962 | `		}` |
|     439 | 2963 | `		pGen->pIn++;` |
|       5 | 2964 | `	}` |
|     177 | 2965 | `	return pLast;` |
|       5 | 2966 | `}` |
|       - | 2967 | `/*` |
|       - | 2968 | `` * Consume the optional `as Alias` clause, leaving *pAlias untouched when absent.`` |
|       - | 2969 | ` */` |
|     158 | 2970 | `static void GenStateCollectImportAlias(ph7_gen_state *pGen,SyString *pAlias)` |
|       5 | 2971 | `{` |
|     158 | 2972 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     103 | 2973 | `		&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|      42 | 2974 | `		pGen->pIn++; /* Jump 'as' */` |
|      42 | 2975 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|      42 | 2976 | `			*pAlias = pGen->pIn->sData;` |
|      42 | 2977 | `			pGen->pIn++;` |
|      19 | 2978 | `		}` |
|      19 | 2979 | `	}` |
|     163 | 2980 | `}` |
|       - | 2981 | `/*` |
|       - | 2982 | ` * Compile the members of a GROUP use declaration (php 7.0):` |
|       - | 2983 | ` *` |
|       - | 2984 | ` *      use A\{Cee, Dee as D2, Sub\Eee};` |
|       - | 2985 | ` *      use function A\{f, g as h};` |
|       - | 2986 | ` *      use A\{function f, const K, Cee};   // per-member kind, untyped group only` |
|       - | 2987 | ` *` |
|       - | 2988 | `` * pPrefix holds the path before the brace; the cursor sits on `{`.  Each member`` |
|       - | 2989 | `` * is the prefix, a `\`, and the member's own (possibly multi-segment) name.  A`` |
|       - | 2990 | ` * trailing comma is allowed, an empty group is not.` |
|       - | 2991 | ` */` |
|      10 | 2992 | `static sxi32 GenStateCompileGroupUse(ph7_gen_state *pGen,SyBlob *pPrefix,int iUseType,sxu32 nLine)` |
|       1 | 2993 | `{` |
|       - | 2994 | `	SyBlob sPath;` |
|      11 | 2995 | `	sxi32 rc = SXRET_OK;` |
|      11 | 2996 | `	pGen->pIn++; /* Jump '{' */` |
|      11 | 2997 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|      11 | 2998 | `	for(;;){` |
|      23 | 2999 | `		int iMemberType = iUseType;` |
|       - | 3000 | `		SyString sAlias;` |
|       - | 3001 | `		SyToken *pLast;` |
|       - | 3002 | ``		/* `function`/`const` may qualify a single member, but only inside a`` |
|       - | 3003 | ``		 * group that is not itself typed (php rejects `use function A\{const C}`). */`` |
|      23 | 3004 | `		if( iUseType == 0 && pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       5 | 3005 | `			sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|       5 | 3006 | `			if( nKey == PH7_TKWRD_FUNCTION ){` |
|       3 | 3007 | `				iMemberType = 1;` |
|       3 | 3008 | `				pGen->pIn++;` |
|       4 | 3009 | `			}else if( nKey == PH7_TKWRD_CONST ){` |
|       3 | 3010 | `				iMemberType = 2;` |
|       3 | 3011 | `				pGen->pIn++;` |
|       1 | 3012 | `			}` |
|       2 | 3013 | `		}` |
|      23 | 3014 | `		SyBlobReset(&sPath);` |
|      23 | 3015 | `		SyBlobAppend(&sPath,SyBlobData(pPrefix),SyBlobLength(pPrefix));` |
|      23 | 3016 | `		pLast = GenStateCollectNsPath(pGen,&sPath);` |
|      23 | 3017 | `		if( pLast == 0 ){` |
|       - | 3018 | ``			/* No member name: `use A\{};` or a stray token.  Report once, then`` |
|       - | 3019 | `			 * skip to the end of the group so the statement does not cascade. */` |
|     ! 0 | 3020 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - | 3021 | `				"syntax error, unexpected %s \"%z\", expecting identifier",` |
|     ! 0 | 3022 | `				TokenTypeName(pGen->pIn < pGen->pEnd ? pGen->pIn->nType : 0),` |
|     ! 0 | 3023 | `				pGen->pIn < pGen->pEnd ? &pGen->pIn->sData : 0);` |
|     ! 0 | 3024 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_CCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 | 3025 | `				pGen->pIn++;` |
|     ! 0 | 3026 | `			}` |
|     ! 0 | 3027 | `			break;` |
|       - | 3028 | `		}` |
|      23 | 3029 | `		sAlias = pLast->sData; /* Default alias is the member's last component */` |
|      23 | 3030 | `		GenStateCollectImportAlias(pGen,&sAlias);` |
|      23 | 3031 | `		rc = GenStateAddImport(&(*pGen),iMemberType,&sPath,&sAlias,nLine);` |
|      23 | 3032 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3033 | `			break;` |
|       - | 3034 | `		}` |
|      23 | 3035 | `		rc = SXRET_OK;` |
|      23 | 3036 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|      15 | 3037 | `			pGen->pIn++;` |
|      15 | 3038 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) ){` |
|       3 | 3039 | `				break; /* Trailing comma before the closing brace */` |
|       - | 3040 | `			}` |
|      13 | 3041 | `			continue;` |
|       - | 3042 | `		}` |
|       9 | 3043 | `		break;` |
|     ! 0 | 3044 | `	}` |
|      11 | 3045 | `	SyBlobRelease(&sPath);` |
|      11 | 3046 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 3047 | `		return SXERR_ABORT;` |
|       - | 3048 | `	}` |
|      11 | 3049 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) ){` |
|      11 | 3050 | `		pGen->pIn++; /* Jump '}' */` |
|       5 | 3051 | `	}` |
|      11 | 3052 | `	return SXRET_OK;` |
|       6 | 3053 | `}` |
|       - | 3054 | `/*` |
|       - | 3055 | ` * Compile the 'use' statement` |
|       - | 3056 | ` * According to the PHP language reference manual` |
|       - | 3057 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - | 3058 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - | 3059 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - | 3060 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - | 3061 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - | 3062 | ` *  a function or constant is not supported.` |
|       - | 3063 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - | 3064 | ` * NOTE` |
|       - | 3065 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - | 3066 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - | 3067 | ` */` |
|     148 | 3068 | `PH7_PRIVATE sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       5 | 3069 | `{` |
|       - | 3070 | `	sxu32 nLine;` |
|       - | 3071 | `	sxi32 rc;` |
|       - | 3072 | `	SyBlob sPath;` |
|       - | 3073 | `	SyString sAlias;` |
|       - | 3074 | `	SyToken *pLast;` |
|       - | 3075 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|     153 | 3076 | `	nLine = pGen->pIn->nLine;` |
|     153 | 3077 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|       - | 3078 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|     153 | 3079 | `	iUseType = 0;` |
|     153 | 3080 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      61 | 3081 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|      61 | 3082 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|      32 | 3083 | `			iUseType = 1;` |
|      32 | 3084 | `			pGen->pIn++;` |
|      47 | 3085 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|      33 | 3086 | `			iUseType = 2;` |
|      33 | 3087 | `			pGen->pIn++;` |
|      14 | 3088 | `		}` |
|      28 | 3089 | `	}` |
|     153 | 3090 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - | 3091 | `	/* Process one or more use declarations separated by commas */` |
|      75 | 3092 | `	for(;;){` |
|     155 | 3093 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 3094 | `			break;` |
|       - | 3095 | `		}` |
|     155 | 3096 | `		SyBlobReset(&sPath);` |
|       - | 3097 | `		/* Collect the full namespace path */` |
|     155 | 3098 | `		pLast = GenStateCollectNsPath(pGen,&sPath);` |
|     155 | 3099 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) && SyBlobLength(&sPath) > 0 ){` |
|       - | 3100 | `			/* GROUP declaration: what was collected is the shared prefix.  php` |
|       - | 3101 | `			 * does not let a group be comma-combined with another declaration,` |
|       - | 3102 | `			 * so the members close the statement. */` |
|      11 | 3103 | `			rc = GenStateCompileGroupUse(&(*pGen),&sPath,iUseType,nLine);` |
|      11 | 3104 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3105 | `				SyBlobRelease(&sPath);` |
|     ! 0 | 3106 | `				return SXERR_ABORT;` |
|       - | 3107 | `			}` |
|      11 | 3108 | `			break;` |
|       - | 3109 | `		}` |
|     145 | 3110 | `		if( pLast == 0 ){` |
|       - | 3111 | `			/* Empty path */` |
|       6 | 3112 | `			break;` |
|       - | 3113 | `		}` |
|       - | 3114 | `		/* Default alias is the last component of the path */` |
|     141 | 3115 | `		sAlias = pLast->sData;` |
|       - | 3116 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|     141 | 3117 | `		GenStateCollectImportAlias(pGen,&sAlias);` |
|     141 | 3118 | `		rc = GenStateAddImport(&(*pGen),iUseType,&sPath,&sAlias,nLine);` |
|     141 | 3119 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3120 | `			SyBlobRelease(&sPath);` |
|     ! 0 | 3121 | `			return SXERR_ABORT;` |
|       - | 3122 | `		}` |
|       - | 3123 | `		/* Check for comma (multiple use declarations) */` |
|     141 | 3124 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 | 3125 | `			pGen->pIn++;` |
|       2 | 3126 | `		}else{` |
|      72 | 3127 | `			break;` |
|       - | 3128 | `		}` |
|       1 | 3129 | `	}` |
|     153 | 3130 | `	SyBlobRelease(&sPath);` |
|     153 | 3131 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 | 3132 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 | 3133 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 | 3134 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3135 | `			return SXERR_ABORT;` |
|       - | 3136 | `		}` |
|       1 | 3137 | `	}` |
|     153 | 3138 | `	return SXRET_OK;` |
|      79 | 3139 | `}` |
|       - | 3140 | `/*` |
|       - | 3141 | ` * Compile the stupid 'declare' language construct.` |
|       - | 3142 | ` *` |
|       - | 3143 | ` * According to the PHP language reference manual.` |
|       - | 3144 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - | 3145 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - | 3146 | ` *  declare (directive)` |
|       - | 3147 | ` *   statement` |
|       - | 3148 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - | 3149 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - | 3150 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - | 3151 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - | 3152 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - | 3153 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - | 3154 | ` * <?php` |
|       - | 3155 | ` * // these are the same:` |
|       - | 3156 | ` * // you can use this:` |
|       - | 3157 | ` * declare(ticks=1) {` |
|       - | 3158 | ` *   // entire script here` |
|       - | 3159 | ` * }` |
|       - | 3160 | ` * // or you can use this:` |
|       - | 3161 | ` * declare(ticks=1);` |
|       - | 3162 | ` * // entire script here` |
|       - | 3163 | ` * ?>` |
|       - | 3164 | ` *` |
|       - | 3165 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - | 3166 | ` */` |
|       - | 3167 | `/*` |
|       - | 3168 | ` * Match a directive name against a known literal (case-insensitive).` |
|       - | 3169 | ` */` |
|     104 | 3170 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|       5 | 3171 | `{` |
|     156 | 3172 | `	return SyStringLength(pName) == nWant` |
|     104 | 3173 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|       5 | 3174 | `}` |
|       - | 3175 |  |
|      56 | 3176 | `PH7_PRIVATE sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       5 | 3177 | `{` |
|      61 | 3178 | `	sxu32 nLine = pGen->pIn->nLine;` |
|      61 | 3179 | `	SyToken *pBodyEnd = 0;` |
|       - | 3180 | `	SyToken *pBodyStart;` |
|       - | 3181 | `	SyToken *pCursor;` |
|       - | 3182 | `	int bHasStrictTypes;` |
|       - | 3183 | `	int bBlockForm;` |
|       - | 3184 | `	int bPlacementOk;` |
|       - | 3185 | `	sxi32 rc;` |
|      61 | 3186 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|      61 | 3187 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       5 | 3188 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|       5 | 3189 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3190 | `			return SXERR_ABORT;` |
|       - | 3191 | `		}` |
|       5 | 3192 | `		goto Synchro;` |
|       - | 3193 | `	}` |
|      57 | 3194 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|      57 | 3195 | `	pBodyStart = pGen->pIn;` |
|       - | 3196 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|      57 | 3197 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|      57 | 3198 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 | 3199 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\")\"");` |
|     ! 0 | 3200 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3201 | `			return SXERR_ABORT;` |
|       - | 3202 | `		}` |
|     ! 0 | 3203 | `		return SXRET_OK;` |
|       - | 3204 | `	}` |
|       - | 3205 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|       - | 3206 | `	 * now delimits the comma-separated directive list. */` |
|      57 | 3207 | `	pGen->pIn = &pBodyEnd[1];` |
|      57 | 3208 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|     ! 0 | 3209 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 | 3210 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3211 | `			return SXERR_ABORT;` |
|       - | 3212 | `		}` |
|     ! 0 | 3213 | `	}` |
|      57 | 3214 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|      57 | 3215 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|      57 | 3216 | `	bHasStrictTypes = 0;` |
|       - | 3217 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|       - | 3218 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|       - | 3219 | `	 * directive appears anywhere in the list, before validating values. */` |
|      57 | 3220 | `	pCursor = pBodyStart;` |
|      69 | 3221 | `	while( pCursor < pBodyEnd ){` |
|      65 | 3222 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      57 | 3223 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|      53 | 3224 | `				bHasStrictTypes = 1;` |
|      53 | 3225 | `				break;` |
|       - | 3226 | `			}` |
|       2 | 3227 | `		}` |
|      14 | 3228 | `		pCursor++;` |
|       2 | 3229 | `	}` |
|      57 | 3230 | `	if( bHasStrictTypes && bBlockForm ){` |
|       3 | 3231 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3232 | `			"strict_types declaration must not use block mode");` |
|       3 | 3233 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 | 3234 | `		return SXRET_OK;` |
|       - | 3235 | `	}` |
|      55 | 3236 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|       6 | 3237 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3238 | `			"strict_types declaration must be the very first statement in the script");` |
|       6 | 3239 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       6 | 3240 | `		return SXRET_OK;` |
|       - | 3241 | `	}` |
|       - | 3242 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|      51 | 3243 | `	pCursor = pBodyStart;` |
|      97 | 3244 | `	while( pCursor < pBodyEnd ){` |
|       - | 3245 | `		SyToken *pNameTok;` |
|       - | 3246 | `		SyToken *pEqTok;` |
|       - | 3247 | `		SyToken *pValTok;` |
|       - | 3248 | `		SyString *pDirName;` |
|       - | 3249 | `		int bIsStrict;` |
|       - | 3250 | `		int iStrictValue;` |
|      53 | 3251 | `		pNameTok = pCursor;` |
|      53 | 3252 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 3253 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3254 | `				"declare: Expecting a directive name");` |
|     ! 0 | 3255 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 | 3256 | `			return SXRET_OK;` |
|       - | 3257 | `		}` |
|      53 | 3258 | `		pEqTok = pNameTok + 1;` |
|      53 | 3259 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|     ! 0 | 3260 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3261 | `				"declare: Expecting '=' after directive name");` |
|     ! 0 | 3262 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 | 3263 | `			return SXRET_OK;` |
|       - | 3264 | `		}` |
|      53 | 3265 | `		pValTok = pEqTok + 1;` |
|      53 | 3266 | `		if( pValTok >= pBodyEnd ){` |
|     ! 0 | 3267 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3268 | `				"declare: Expecting value after '='");` |
|     ! 0 | 3269 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 | 3270 | `			return SXRET_OK;` |
|       - | 3271 | `		}` |
|      53 | 3272 | `		pDirName = &pNameTok->sData;` |
|      53 | 3273 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|      53 | 3274 | `		if( bIsStrict ){` |
|       - | 3275 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|       - | 3276 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|      49 | 3277 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|     ! 0 | 3278 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3279 | `					"declare(strict_types) value must be a literal");` |
|     ! 0 | 3280 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 | 3281 | `				return SXRET_OK;` |
|       - | 3282 | `			}` |
|      49 | 3283 | `			iStrictValue = -1;` |
|      49 | 3284 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|      49 | 3285 | `				const char *zv = SyStringData(&pValTok->sData);` |
|      49 | 3286 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|      49 | 3287 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|      47 | 3288 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|      22 | 3289 | `			}` |
|      49 | 3290 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|       3 | 3291 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3292 | `					"strict_types declaration must have 0 or 1 as its value");` |
|       3 | 3293 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 | 3294 | `				return SXRET_OK;` |
|       - | 3295 | `			}` |
|      46 | 3296 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|      27 | 3297 | `		}else if( DeclareNameIs(pDirName, "encoding", sizeof("encoding")-1) ){` |
|       - | 3298 | `			/* php always ignores declare(encoding=...) unless it was built with` |
|       - | 3299 | `			 * Zend multibyte, and says so in these exact words. */` |
|       3 | 3300 | `			PH7_GenCompileError(&(*pGen),E_WARNING,nLine,` |
|       - | 3301 | `				"declare(encoding=...) ignored because Zend multibyte feature is turned off by settings");` |
|       1 | 3302 | `		}else{` |
|       - | 3303 | `			/* Other directives (ticks and friends) are accepted as no-ops.` |
|       - | 3304 | `			 * This used to emit a NOTICE naming the upstream engine and its` |
|       - | 3305 | `			 * version ("the declare construct is a no-op in the current release` |
|       - | 3306 | `			 * of the PH7(2.1.4) engine") — php prints nothing at all for` |
|       - | 3307 | ``			 * `declare(ticks=1)`, and leaking the old engine's branding into`` |
|       - | 3308 | `			 * user-visible diagnostics was wrong on its own. */` |
|       - | 3309 | `		}` |
|      51 | 3310 | `		pCursor = pValTok + 1;` |
|       - | 3311 | `		/* Consume separating comma (or end). */` |
|      51 | 3312 | `		if( pCursor < pBodyEnd ){` |
|       3 | 3313 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 | 3314 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3315 | `					"declare: Expecting ',' or ')' after directive value");` |
|     ! 0 | 3316 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 | 3317 | `				return SXRET_OK;` |
|       - | 3318 | `			}` |
|       3 | 3319 | `			pCursor++;` |
|       1 | 3320 | `		}` |
|       5 | 3321 | `	}` |
|       - | 3322 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|       - | 3323 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|       - | 3324 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|      49 | 3325 | `	return SXRET_OK;` |
|       2 | 3326 | `Synchro:` |
|       - | 3327 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      15 | 3328 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      11 | 3329 | `		pGen->pIn++;` |
|       1 | 3330 | `	}` |
|       5 | 3331 | `	return SXRET_OK;` |
|      33 | 3332 | `}` |
|       - | 3333 | `/*` |
|       - | 3334 | ` * Compile a class constant.` |
|       - | 3335 | ` * According to the PHP language reference manual` |
|       - | 3336 | ` *  Class Constants` |
|       - | 3337 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - | 3338 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - | 3339 | ` *   you don't use the $ symbol to declare or use them.` |
|       - | 3340 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - | 3341 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - | 3342 | ` *   It's also possible for interfaces to have constants.` |
|       - | 3343 | ` * Symisc eXtension.` |
|       - | 3344 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - | 3345 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 3346 | ` *  Example:` |
|       - | 3347 | ` *   class Test{` |
|       - | 3348 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 3349 | ` *   };` |
|       - | 3350 | ` *   var_dump(TEST::MyConst);` |
|       - | 3351 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 3352 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 3353 | ` */` |
|       - | 3354 | `/*` |
|       - | 3355 | ` * Exception handling.` |
|       - | 3356 | ` *  According to the PHP language reference manual` |
|       - | 3357 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - | 3358 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - | 3359 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - | 3360 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - | 3361 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - | 3362 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - | 3363 | ` *    (or re-thrown) within a catch block.` |
|       - | 3364 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - | 3365 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - | 3366 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - | 3367 | ` *    been defined with set_exception_handler().` |
|       - | 3368 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - | 3369 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - | 3370 | ` */` |
|       - | 3371 | `/*` |
|       - | 3372 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - | 3373 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - | 3374 | ` * indicates failure.` |
|       - | 3375 | ` */` |
|   51990 | 3376 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 | 3377 | `{` |
|       - | 3378 | ``	/* php decides throwability at RUNTIME: `throw 5` compiles and raises the`` |
|       - | 3379 | `	 * catchable Error "Can only throw objects" when it executes, and a` |
|       - | 3380 | `	 * non-Throwable object raises "Cannot throw objects that do not implement` |
|       - | 3381 | `	 * Throwable". This used to whitelist a handful of expression shapes and` |
|       - | 3382 | `	 * reject the rest at compile time as a "friendlier" error, which meant the` |
|       - | 3383 | `	 * program never ran and no catch could ever see it — and it was inconsistent` |
|       - | 3384 | ``	 * anyway, since `throw $v` (a variable holding an int) always compiled and`` |
|       - | 3385 | `	 * fell through to the same runtime path. OP_THROW now owns the whole rule,` |
|       - | 3386 | ``	 * so accept any expression here; `throw;` with no operand is still rejected`` |
|       - | 3387 | `	 * by PH7_CompileThrow's SXERR_EMPTY branch. */` |
|   25995 | 3388 | `	SXUNUSED(pGen);` |
|   25995 | 3389 | `	SXUNUSED(pRoot);` |
|   51995 | 3390 | `	return SXRET_OK;` |
|       5 | 3391 | `}` |
|       - | 3392 | `/*` |
|       - | 3393 | ` * Compile a 'throw' statement.` |
|       - | 3394 | ` * throw: This is how you trigger an exception.` |
|       - | 3395 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - | 3396 | ` */` |
|   51950 | 3397 | `PH7_PRIVATE sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       5 | 3398 | `{` |
|   51955 | 3399 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 3400 | `	GenBlock *pBlock;` |
|       - | 3401 | `	sxu32 nIdx;` |
|       - | 3402 | `	sxi32 rc;` |
|   51955 | 3403 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - | 3404 | `	/* Compile the expression */` |
|   51955 | 3405 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|   51955 | 3406 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 3407 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 | 3408 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3409 | `			return SXERR_ABORT;` |
|       - | 3410 | `		}` |
|     ! 0 | 3411 | `		return SXRET_OK;` |
|       - | 3412 | `	}` |
|   51955 | 3413 | `	pBlock = pGen->pCurrent;` |
|       - | 3414 | `	/* Point to the top most function or try block and emit the forward jump */` |
|  225355 | 3415 | `	while(pBlock->pParent){` |
|  225333 | 3416 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|   51933 | 3417 | `			break;` |
|       - | 3418 | `		}` |
|       - | 3419 | `		/* Point to the parent block */` |
|  173405 | 3420 | `		pBlock = pBlock->pParent;` |
|       5 | 3421 | `	}` |
|       - | 3422 | `	/* Emit the throw instruction */` |
|   51955 | 3423 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - | 3424 | `	/* Emit the jump */` |
|   51955 | 3425 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|   51955 | 3426 | `	return SXRET_OK;` |
|   25980 | 3427 | `}` |
|       - | 3428 | `/*` |
|       - | 3429 | ` * Compile a PHP 8.0 'throw' expression.` |
|       - | 3430 | ` * Called from the expression code generator when a 'throw' keyword is` |
|       - | 3431 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|       - | 3432 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|       - | 3433 | ` * the validator guarantees the operand is a valid exception target.` |
|       - | 3434 | ` */` |
|      40 | 3435 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       3 | 3436 | `{` |
|      43 | 3437 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 3438 | `	GenBlock *pBlock;` |
|       - | 3439 | `	sxu32 nIdx;` |
|       - | 3440 | `	sxi32 rc;` |
|      20 | 3441 | `	(void)iCompileFlag;` |
|      43 | 3442 | `	pGen->pIn++; /* Skip 'throw' */` |
|      43 | 3443 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 3444 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 3445 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 3446 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3447 | `			return SXERR_ABORT;` |
|       - | 3448 | `		}` |
|     ! 0 | 3449 | `		return SXRET_OK;` |
|       - | 3450 | `	}` |
|      43 | 3451 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|      43 | 3452 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 3453 | `		return SXERR_ABORT;` |
|       - | 3454 | `	}` |
|      43 | 3455 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 3456 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 3457 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 3458 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3459 | `			return SXERR_ABORT;` |
|       - | 3460 | `		}` |
|     ! 0 | 3461 | `		return SXRET_OK;` |
|       - | 3462 | `	}` |
|       - | 3463 | `	/* Walk up to nearest exception/function block for the jump target */` |
|      43 | 3464 | `	pBlock = pGen->pCurrent;` |
|      67 | 3465 | `	while( pBlock->pParent ){` |
|      57 | 3466 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|      33 | 3467 | `			break;` |
|       - | 3468 | `		}` |
|      26 | 3469 | `		pBlock = pBlock->pParent;` |
|       2 | 3470 | `	}` |
|      43 | 3471 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|      43 | 3472 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|      43 | 3473 | `	return SXRET_OK;` |
|      23 | 3474 | `}` |
|       - | 3475 | `/*` |
|       - | 3476 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|       - | 3477 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|       - | 3478 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|       - | 3479 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|       - | 3480 | ` * compile error propagated from the parser.` |
|       - | 3481 | ` */` |
|      66 | 3482 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|       5 | 3483 | `{` |
|       - | 3484 | `	SyString sClassName;` |
|       - | 3485 | `	SyToken *pToken;` |
|       - | 3486 | `	SyString *pName;` |
|       - | 3487 | `	char *zDup;` |
|       - | 3488 | `	sxi32 rc;` |
|      71 | 3489 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|      71 | 3490 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|      71 | 3491 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|       - | 3492 | `	/* Inline catches compile into the function's own container; pByteCode stays NULL. */` |
|      71 | 3493 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 | 3494 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 3495 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 3496 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 3497 | `		return SXERR_INVALID;` |
|       - | 3498 | `	}` |
|      71 | 3499 | `	pGen->pIn++; /* '(' */` |
|      33 | 3500 | `	for(;;){` |
|       - | 3501 | `		SyBlob sResolved;` |
|      71 | 3502 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      71 | 3503 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 | 3504 | `			SyBlobRelease(&sResolved);` |
|     ! 0 | 3505 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 3506 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 3507 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 3508 | `			return SXERR_INVALID;` |
|       - | 3509 | `		}` |
|     104 | 3510 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      66 | 3511 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|      71 | 3512 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|      71 | 3513 | `		SyBlobRelease(&sResolved);` |
|      71 | 3514 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|      71 | 3515 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|      71 | 3516 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      66 | 3517 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|       5 | 3518 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|     ! 0 | 3519 | `			pGen->pIn++; continue;` |
|       - | 3520 | `		}` |
|      71 | 3521 | `		break;` |
|     ! 0 | 3522 | `	}` |
|       - | 3523 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|       - | 3524 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding. */` |
|      71 | 3525 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|       3 | 3526 | `		pGen->pIn++; /* ')' */` |
|       3 | 3527 | `		return SXRET_OK;` |
|       - | 3528 | `	}` |
|      64 | 3529 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|      69 | 3530 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 3531 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 3532 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 3533 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 3534 | `		return SXERR_INVALID;` |
|       - | 3535 | `	}` |
|      69 | 3536 | `	pGen->pIn++; /* '$' */` |
|      69 | 3537 | `	pName = &pGen->pIn->sData;` |
|      69 | 3538 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      69 | 3539 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|      69 | 3540 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|      69 | 3541 | `	pGen->pIn++;` |
|      69 | 3542 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 | 3543 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 3544 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 3545 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 3546 | `		return SXERR_INVALID;` |
|       - | 3547 | `	}` |
|      69 | 3548 | `	pGen->pIn++; /* ')' */` |
|      69 | 3549 | `	return SXRET_OK;` |
|      38 | 3550 | `}` |
|       - | 3551 | `/*` |
|       - | 3552 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|       - | 3553 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|       - | 3554 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|       - | 3555 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|       - | 3556 | ` * VmThrowException):` |
|       - | 3557 | ` *` |
|       - | 3558 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|       - | 3559 | ` *    <try body>` |
|       - | 3560 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|       - | 3561 | ` *    JMP  -> finally\|end` |
|       - | 3562 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|       - | 3563 | ` *    <catch body>` |
|       - | 3564 | ` *    JMP  -> finally\|end` |
|       - | 3565 | ` *    ... more catches ...` |
|       - | 3566 | ` *  Lfin: <finally body>` |
|       - | 3567 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|       - | 3568 | ` *  Lend:` |
|       - | 3569 | ` */` |
|     122 | 3570 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|       5 | 3571 | `{` |
|     127 | 3572 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 3573 | `	GenBlock *pTry;` |
|       - | 3574 | `	VmInstr *pInstr;` |
|     127 | 3575 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|       - | 3576 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|       - | 3577 | `	sxi32 rc;` |
|     127 | 3578 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|       - | 3579 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION; passed at` |
|       - | 3580 | `	 * ENTRY so GenStateEnterBlock can classify the scope with it) */` |
|     188 | 3581 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),` |
|      61 | 3582 | `		pException,&pTry);` |
|     127 | 3583 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|     127 | 3584 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|     127 | 3585 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|     127 | 3586 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     127 | 3587 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|     127 | 3588 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|     127 | 3589 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|       - | 3590 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|     127 | 3591 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|     127 | 3592 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|     127 | 3593 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|     127 | 3594 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 3595 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|     127 | 3596 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|       - | 3597 | `	/* Catch clauses (inline) */` |
|     127 | 3598 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     122 | 3599 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|      71 | 3600 | `		sxu32 k = 0;` |
|      99 | 3601 | `		for(;;){` |
|       - | 3602 | `			ph7_exception_block sCatch;` |
|       - | 3603 | `			GenBlock *pCatchBlk;` |
|     137 | 3604 | `			sxu32 idxJmp = 0;` |
|     132 | 3605 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     127 | 3606 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|      38 | 3607 | `				break;` |
|       - | 3608 | `			}` |
|      71 | 3609 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|      71 | 3610 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      71 | 3611 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|      71 | 3612 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|      71 | 3613 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|       - | 3614 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|       - | 3615 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|       - | 3616 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump).` |
|       - | 3617 | `			 * Passed at ENTRY: GenStateEnterBlock reads it to classify the block's scope. */` |
|     104 | 3618 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),` |
|      33 | 3619 | `				pException,&pCatchBlk);` |
|      71 | 3620 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      71 | 3621 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      71 | 3622 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      71 | 3623 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|      71 | 3624 | `			GenStateLeaveBlock(&(*pGen),0);` |
|       - | 3625 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|       - | 3626 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|      71 | 3627 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|      71 | 3628 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|      71 | 3629 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|      71 | 3630 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|      71 | 3631 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      71 | 3632 | `			k++;` |
|       5 | 3633 | `		}` |
|      33 | 3634 | `	}` |
|       - | 3635 | `	/* Finally (inline) */` |
|     127 | 3636 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     102 | 3637 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 3638 | `		GenBlock *pFinBlk;` |
|      65 | 3639 | `		pGen->pIn++; /* Jump 'finally' */` |
|      65 | 3640 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|      95 | 3641 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FINALLY,` |
|      30 | 3642 | `			PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|      65 | 3643 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      65 | 3644 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      65 | 3645 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      65 | 3646 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|      65 | 3647 | `		GenStateLeaveBlock(&(*pGen),0);` |
|      65 | 3648 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|      65 | 3649 | `		pException->iHasFinally = 1;` |
|      30 | 3650 | `	}` |
|     127 | 3651 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|     127 | 3652 | `	pException->iInlined = 1;` |
|       - | 3653 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|       - | 3654 | `	{` |
|     127 | 3655 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|       - | 3656 | `		sxu32 *aJ; sxu32 n;` |
|     127 | 3657 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|     127 | 3658 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|     127 | 3659 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|     193 | 3660 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|      71 | 3661 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|      71 | 3662 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|      38 | 3663 | `		}` |
|       - | 3664 | `	}` |
|     127 | 3665 | `	SySetRelease(&aCatchJmp);` |
|     127 | 3666 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|     ! 0 | 3667 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|     ! 0 | 3668 | `	}` |
|     127 | 3669 | `	return SXRET_OK;` |
|      66 | 3670 | `}` |
|       - | 3671 | `/*` |
|       - | 3672 | ` * Compile a 'catch' block.` |
|       - | 3673 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - | 3674 | ` * an object containing the exception information.` |
|       - | 3675 | ` */` |
|    3012 | 3676 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       5 | 3677 | `{` |
|    3017 | 3678 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 3679 | `	ph7_exception_block sCatch;` |
|       - | 3680 | `	SySet *pInstrContainer;` |
|       - | 3681 | `	SyString sClassName;` |
|       - | 3682 | `	GenBlock *pCatch;` |
|       - | 3683 | `	SyToken *pToken;` |
|       - | 3684 | `	SyString *pName;` |
|       - | 3685 | `	char *zDup;` |
|       - | 3686 | `	sxi32 rc;` |
|    3017 | 3687 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - | 3688 | `	/* Zero the structure */` |
|    3017 | 3689 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - | 3690 | `	/* Initialize fields */` |
|    3017 | 3691 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|       - | 3692 | `	/* The catch body gets its own bytecode array, allocated (not embedded) so its address` |
|       - | 3693 | `	 * survives both this stack frame and any later growth of pException->sEntry — a` |
|       - | 3694 | `	 * break/continue inside the body records it in its JumpFixup (see JumpFixup). */` |
|    3017 | 3695 | `	sCatch.pByteCode = (SySet *)SyMemBackendAlloc(&pException->pVm->sAllocator,sizeof(SySet));` |
|    3017 | 3696 | `	if( sCatch.pByteCode == 0 ){` |
|     ! 0 | 3697 | `		goto Mem;` |
|       - | 3698 | `	}` |
|    3017 | 3699 | `	SySetInit(sCatch.pByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|    3017 | 3700 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|       - | 3701 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 3702 | `			pToken = pGen->pIn;` |
|     ! 0 | 3703 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 3704 | `				pToken--;` |
|     ! 0 | 3705 | `			}` |
|     ! 0 | 3706 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 3707 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 3708 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 3709 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3710 | `				return SXERR_ABORT;` |
|       - | 3711 | `			}` |
|     ! 0 | 3712 | `			return SXERR_INVALID;` |
|       - | 3713 | `	}` |
|       - | 3714 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|    3017 | 3715 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|    1523 | 3716 | `	for(;;){` |
|       - | 3717 | `		SyBlob sResolved;` |
|    3051 | 3718 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    3051 | 3719 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       6 | 3720 | `			SyBlobRelease(&sResolved);` |
|       6 | 3721 | `			pToken = pGen->pIn;` |
|       6 | 3722 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 3723 | `				pToken--;` |
|     ! 0 | 3724 | `			}` |
|       8 | 3725 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 3726 | `				"syntax error, unexpected %s \"%z\"",` |
|       2 | 3727 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       6 | 3728 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3729 | `				return SXERR_ABORT;` |
|       - | 3730 | `			}` |
|       6 | 3731 | `			return SXERR_INVALID;` |
|       - | 3732 | `		}` |
|       - | 3733 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|       - | 3734 | `		 * transient SyBlob allocation. */` |
|    4568 | 3735 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    3042 | 3736 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|    3047 | 3737 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|    3047 | 3738 | `		SyBlobRelease(&sResolved);` |
|    3047 | 3739 | `		if( zDup == 0 ){` |
|     ! 0 | 3740 | `			goto Mem;` |
|       - | 3741 | `		}` |
|    3047 | 3742 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|    3047 | 3743 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3744 | `			goto Mem;` |
|       - | 3745 | `		}` |
|       - | 3746 | `		/* Check for '\|' (multi-catch separator) */` |
|    3042 | 3747 | `		if( pGen->pIn < pGen->pEnd &&` |
|    3042 | 3748 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|      39 | 3749 | `			pGen->pIn->sData.nByte == 1 &&` |
|      34 | 3750 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|      38 | 3751 | `			pGen->pIn++; /* Consume the '\|' */` |
|      38 | 3752 | `			continue;` |
|       - | 3753 | `		}` |
|    3013 | 3754 | `		break;` |
|     ! 0 | 3755 | `	}` |
|       - | 3756 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|       - | 3757 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding;` |
|       - | 3758 | `	 * jump straight to compiling the block below. */` |
|    3013 | 3759 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) /*)*/ ){` |
|       7 | 3760 | `		goto CatchBody;` |
|       - | 3761 | `	}` |
|    3002 | 3762 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|    3007 | 3763 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 3764 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 3765 | `			pToken = pGen->pIn;` |
|     ! 0 | 3766 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 3767 | `				pToken--;` |
|     ! 0 | 3768 | `			}` |
|     ! 0 | 3769 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 3770 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 3771 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 3772 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3773 | `				return SXERR_ABORT;` |
|       - | 3774 | `			}` |
|     ! 0 | 3775 | `			return SXERR_INVALID;` |
|       - | 3776 | `	}` |
|    3007 | 3777 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - | 3778 | `	/* Duplicate instance name */` |
|    3007 | 3779 | `	pName = &pGen->pIn->sData;` |
|    3007 | 3780 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    3007 | 3781 | `	if( zDup == 0 ){` |
|     ! 0 | 3782 | `		goto Mem;` |
|       - | 3783 | `	}` |
|    3007 | 3784 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|    3007 | 3785 | `	pGen->pIn++;` |
|    1504 | 3786 | `CatchBody:` |
|    3013 | 3787 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - | 3788 | `		/* Unexpected token,break immediately */` |
|     ! 0 | 3789 | `		pToken = pGen->pIn;` |
|     ! 0 | 3790 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 | 3791 | `			pToken--;` |
|     ! 0 | 3792 | `		}` |
|     ! 0 | 3793 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 3794 | `			"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 3795 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 3796 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3797 | `			return SXERR_ABORT;` |
|       - | 3798 | `		}` |
|     ! 0 | 3799 | `		return SXERR_INVALID;` |
|       - | 3800 | `	}` |
|       - | 3801 | `	/* Compile the block */` |
|    3013 | 3802 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - | 3803 | `	/* Create the catch block. GEN_BLOCK_DETACHED: the body below compiles into` |
|       - | 3804 | `	 * sCatch.pByteCode, not into the enclosing function's array. */` |
|    3013 | 3805 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION\|GEN_BLOCK_DETACHED,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|    3013 | 3806 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 3807 | `		return SXERR_ABORT;` |
|       - | 3808 | `	}` |
|       - | 3809 | `	/* Swap bytecode container */` |
|    3013 | 3810 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    3013 | 3811 | `	PH7_VmSetByteCodeContainer(pGen->pVm,sCatch.pByteCode);` |
|       - | 3812 | `	/* Compile the block */` |
|    3013 | 3813 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 3814 | `	/* Fix forward jumps now the destination is resolved  */` |
|    3013 | 3815 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 3816 | `	/* Emit the DONE instruction */` |
|    3013 | 3817 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 3818 | `	/* Leave the block */` |
|    3013 | 3819 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 3820 | `	/* Restore the default container */` |
|    3013 | 3821 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 3822 | `	/* Install the catch block */` |
|    3013 | 3823 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|    3013 | 3824 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 3825 | `		goto Mem;` |
|       - | 3826 | `	}` |
|    3013 | 3827 | `	return SXRET_OK;` |
|     ! 0 | 3828 | `Mem:` |
|     ! 0 | 3829 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 3830 | `	return SXERR_ABORT;` |
|    1511 | 3831 | `}` |
|       - | 3832 | `/*` |
|       - | 3833 | ` * Compile a 'try' block.` |
|       - | 3834 | ` * A function using an exception should be in a "try" block.` |
|       - | 3835 | ` * If the exception does not trigger, the code will continue` |
|       - | 3836 | ` * as normal. However if the exception triggers, an exception` |
|       - | 3837 | ` * is "thrown".` |
|       - | 3838 | ` */` |
|    3276 | 3839 | `PH7_PRIVATE sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       5 | 3840 | `{` |
|       - | 3841 | `	ph7_exception *pException;` |
|    3281 | 3842 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 3843 | `	GenBlock *pTry;` |
|       - | 3844 | `	sxu32 nJmpIdx;` |
|       - | 3845 | `	sxi32 rc;` |
|       - | 3846 | `	/* Create the exception container */` |
|    3281 | 3847 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|    3281 | 3848 | `	if( pException == 0 ){` |
|     ! 0 | 3849 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 | 3850 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 3851 | `		return SXERR_ABORT;` |
|       - | 3852 | `	}` |
|       - | 3853 | `	/* Zero the structure */` |
|    3281 | 3854 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - | 3855 | `	/* Initialize fields */` |
|    3281 | 3856 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|    3281 | 3857 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|    3281 | 3858 | `	pException->iHasFinally = 0;` |
|    3281 | 3859 | `	pException->iFinallyDone = 0;` |
|    3281 | 3860 | `	pException->pVm = pGen->pVm;` |
|       - | 3861 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|       - | 3862 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the DETACHED path`` |
|       - | 3863 | `	 * below — deliberately, not pending migration: it is the proven one, and inlining was` |
|       - | 3864 | ``	 * scoped to generators so no other code path changed. `bInlineTryCatch` is 1 since the`` |
|       - | 3865 | ``	 * inline VM handlers landed, so `bInGenerator` is what actually selects here. */`` |
|    3281 | 3866 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|     127 | 3867 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|       - | 3868 | `	}` |
|       - | 3869 | `	/* Create the try block */` |
|       - | 3870 | `	/* pUserData is the exception context, passed at ENTRY (not assigned after) because` |
|       - | 3871 | `	 * GenStateEnterBlock reads it to classify the block's try/catch scope — see aScope.` |
|       - | 3872 | `	 * It is also what a break/continue crossing this try emits its POP_EXCEPTION with. */` |
|    4736 | 3873 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),` |
|    1577 | 3874 | `		pException,&pTry);` |
|    3159 | 3875 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 3876 | `		return SXERR_ABORT;` |
|       - | 3877 | `	}` |
|       - | 3878 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|    3159 | 3879 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - | 3880 | `	/* Fix the jump later when the destination is resolved */` |
|    3159 | 3881 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|    3159 | 3882 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - | 3883 | `	/* Compile the block */` |
|    3159 | 3884 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|    3159 | 3885 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 3886 | `		return SXERR_ABORT;` |
|       - | 3887 | `	}` |
|       - | 3888 | `	/* Fix forward jumps now the destination is resolved */` |
|    3159 | 3889 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 3890 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|    3159 | 3891 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - | 3892 | `	/* Leave the block */` |
|    3159 | 3893 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 3894 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|    3159 | 3895 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|    3152 | 3896 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - | 3897 | `		/* Compile one or more catch blocks */` |
|    3007 | 3898 | `		for(;;){` |
|    6014 | 3899 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    4975 | 3900 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|    1506 | 3901 | `					break;` |
|       - | 3902 | `			}` |
|    3017 | 3903 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|    3017 | 3904 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3905 | `				return SXERR_ABORT;` |
|       - | 3906 | `			}` |
|       5 | 3907 | `		}` |
|    1501 | 3908 | `	}` |
|       - | 3909 | `	/* Compile optional finally block */` |
|    3159 | 3910 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|    1716 | 3911 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 3912 | `		SySet *pInstrContainer;` |
|       - | 3913 | `		GenBlock *pFinBlock;` |
|     237 | 3914 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - | 3915 | `		/* Create the finally block for jump fixup bookkeeping (detached: the body` |
|       - | 3916 | `		 * compiles into pException->sFinally, see GEN_BLOCK_DETACHED). */` |
|     353 | 3917 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION\|GEN_BLOCK_DETACHED\|GEN_BLOCK_FINALLY,` |
|     116 | 3918 | `			PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|     237 | 3919 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3920 | `			return SXERR_ABORT;` |
|       - | 3921 | `		}` |
|       - | 3922 | `		/* Swap bytecode container */` |
|     237 | 3923 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     237 | 3924 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - | 3925 | `		/* Compile the finally body */` |
|     237 | 3926 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     237 | 3927 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3928 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 3929 | `			return SXERR_ABORT;` |
|       - | 3930 | `		}` |
|       - | 3931 | `		/* Fix forward jumps now the destination is resolved */` |
|     237 | 3932 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 3933 | `		/* Emit DONE to terminate the finally block */` |
|     237 | 3934 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 3935 | `		/* Leave the block */` |
|     237 | 3936 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - | 3937 | `		/* Restore the default container */` |
|     237 | 3938 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     237 | 3939 | `		pException->iHasFinally = 1;` |
|     116 | 3940 | `	}` |
|       - | 3941 | `	/* Must have at least one catch or finally */` |
|    3159 | 3942 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       9 | 3943 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 3944 | `			"Cannot use try without catch or finally");` |
|       9 | 3945 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3946 | `			return SXERR_ABORT;` |
|       - | 3947 | `		}` |
|       3 | 3948 | `	}` |
|    3159 | 3949 | `	return SXRET_OK;` |
|    1643 | 3950 | `}` |
|       - | 3951 | `/*` |
|       - | 3952 | ` * Compile a switch block.` |
|       - | 3953 | ` *  (See block-comment below for more information)` |
|       - | 3954 | ` */` |
|     146 | 3955 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       5 | 3956 | `{` |
|     151 | 3957 | `	sxi32 rc = SXRET_OK;` |
|     151 | 3958 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - | 3959 | `		/* Unexpected token */` |
|     ! 0 | 3960 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|     ! 0 | 3961 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3962 | `			return SXERR_ABORT;` |
|       - | 3963 | `		}` |
|     ! 0 | 3964 | `		pGen->pIn++;` |
|     ! 0 | 3965 | `	}` |
|     151 | 3966 | `	pGen->pIn++;` |
|       - | 3967 | `	/* First instruction to execute in this block. */` |
|     151 | 3968 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - | 3969 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - | 3970 | `	 * or the '}' token */` |
|     247 | 3971 | `	for(;;){` |
|     499 | 3972 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 3973 | `			/* No more input to process */` |
|     ! 0 | 3974 | `			break;` |
|       - | 3975 | `		}` |
|     499 | 3976 | `		rc = SXRET_OK;` |
|     499 | 3977 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     103 | 3978 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      47 | 3979 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - | 3980 | `					/* Unexpected token */` |
|     ! 0 | 3981 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 3982 | `						&pGen->pIn->sData);` |
|     ! 0 | 3983 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 3984 | `						return SXERR_ABORT;` |
|       - | 3985 | `					}` |
|       - | 3986 | `					/* FALL THROUGH */` |
|     ! 0 | 3987 | `				}` |
|      47 | 3988 | `				rc = SXERR_EOF;` |
|      47 | 3989 | `				break;` |
|       - | 3990 | `			}` |
|      33 | 3991 | `		}else{` |
|       - | 3992 | `			sxi32 nKwrd;` |
|       - | 3993 | `			/* Extract the keyword */` |
|     401 | 3994 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     401 | 3995 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      55 | 3996 | `				break;` |
|       - | 3997 | `			}` |
|     301 | 3998 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       5 | 3999 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - | 4000 | `					/* Unexpected token */` |
|     ! 0 | 4001 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 4002 | `						&pGen->pIn->sData);` |
|     ! 0 | 4003 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 4004 | `						return SXERR_ABORT;` |
|       - | 4005 | `					}` |
|       - | 4006 | `					/* FALL THROUGH */` |
|     ! 0 | 4007 | `				}` |
|       - | 4008 | `				/* Block compiled */` |
|       5 | 4009 | `				break;` |
|       - | 4010 | `			}` |
|       - | 4011 | `		}` |
|       - | 4012 | `		/* Compile block */` |
|     353 | 4013 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     353 | 4014 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4015 | `			return SXERR_ABORT;` |
|       - | 4016 | `		}` |
|       5 | 4017 | `	}` |
|     151 | 4018 | `	return rc;` |
|      78 | 4019 | `}` |
|       - | 4020 | `/*` |
|       - | 4021 | ` * Compile a case eXpression.` |
|       - | 4022 | ` *  (See block-comment below for more information)` |
|       - | 4023 | ` */` |
|     112 | 4024 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       5 | 4025 | `{` |
|       - | 4026 | `	SySet *pInstrContainer;` |
|       - | 4027 | `	SyToken *pEnd,*pTmp;` |
|     117 | 4028 | `	sxi32 iNest = 0;` |
|       - | 4029 | `	sxi32 rc;` |
|       - | 4030 | `	/* Delimit the expression */` |
|     117 | 4031 | `	pEnd = pGen->pIn;` |
|     245 | 4032 | `	while( pEnd < pGen->pEnd ){` |
|     245 | 4033 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - | 4034 | `			/* Increment nesting level */` |
|       8 | 4035 | `			iNest++;` |
|     242 | 4036 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - | 4037 | `			/* Decrement nesting level */` |
|       8 | 4038 | `			iNest--;` |
|     236 | 4039 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|     117 | 4040 | `			break;` |
|       - | 4041 | `		}` |
|     133 | 4042 | `		pEnd++;` |
|       5 | 4043 | `	}` |
|     117 | 4044 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 4045 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 | 4046 | `		if( rc == SXERR_ABORT ){` |
|       - | 4047 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4048 | `			return SXERR_ABORT;` |
|       - | 4049 | `		}` |
|     ! 0 | 4050 | `	}` |
|       - | 4051 | `	/* Swap token stream */` |
|     117 | 4052 | `	pTmp = pGen->pEnd;` |
|     117 | 4053 | `	pGen->pEnd = pEnd;` |
|     117 | 4054 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     117 | 4055 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|     117 | 4056 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 4057 | `	/* Emit the done instruction */` |
|     117 | 4058 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     117 | 4059 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 4060 | `	/* Update token stream */` |
|     117 | 4061 | `	pGen->pIn  = pEnd;` |
|     117 | 4062 | `	pGen->pEnd = pTmp;` |
|     117 | 4063 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 4064 | `		return SXERR_ABORT;` |
|       - | 4065 | `	}` |
|     117 | 4066 | `	return SXRET_OK;` |
|      61 | 4067 | `}` |
|       - | 4068 | `/*` |
|       - | 4069 | ` * Compile the smart switch statement.` |
|       - | 4070 | ` * According to the PHP language reference manual` |
|       - | 4071 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - | 4072 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - | 4073 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - | 4074 | ` *  This is exactly what the switch statement is for.` |
|       - | 4075 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - | 4076 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - | 4077 | ` *  of the outer loop, use continue 2.` |
|       - | 4078 | ` *  Note that switch/case does loose comparision.` |
|       - | 4079 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - | 4080 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - | 4081 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - | 4082 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - | 4083 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - | 4084 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - | 4085 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - | 4086 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - | 4087 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - | 4088 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - | 4089 | ` *  list for the next case.` |
|       - | 4090 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - | 4091 | ` *  or floating-point numbers and strings.` |
|       - | 4092 | ` */` |
|      46 | 4093 | `PH7_PRIVATE sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       5 | 4094 | `{` |
|       - | 4095 | `	GenBlock *pSwitchBlock;` |
|       - | 4096 | `	SyToken *pTmp,*pEnd;` |
|       - | 4097 | `	ph7_switch *pSwitch;` |
|       - | 4098 | `	sxu32 nToken;` |
|       - | 4099 | `	sxu32 nLine;` |
|       - | 4100 | `	sxi32 rc;` |
|      51 | 4101 | `	nLine = pGen->pIn->nLine;` |
|       - | 4102 | `	/* Jump the 'switch' keyword */` |
|      51 | 4103 | `	pGen->pIn++;` |
|      51 | 4104 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 4105 | `		/* Syntax error */` |
|     ! 0 | 4106 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 | 4107 | `		if( rc == SXERR_ABORT ){` |
|       - | 4108 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4109 | `			return SXERR_ABORT;` |
|       - | 4110 | `		}` |
|     ! 0 | 4111 | `		goto Synchronize;` |
|       - | 4112 | `	}` |
|       - | 4113 | `	/* Jump the left parenthesis '(' */` |
|      51 | 4114 | `	pGen->pIn++;` |
|      51 | 4115 | `	pEnd = 0; /* cc warning */` |
|       - | 4116 | `	/* Create the loop block */` |
|      74 | 4117 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      23 | 4118 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      51 | 4119 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4120 | `		return SXERR_ABORT;` |
|       - | 4121 | `	}` |
|       - | 4122 | `	/* Delimit the condition */` |
|      51 | 4123 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      51 | 4124 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 4125 | `		/* Empty expression */` |
|     ! 0 | 4126 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 | 4127 | `		if( rc == SXERR_ABORT ){` |
|       - | 4128 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4129 | `			return SXERR_ABORT;` |
|       - | 4130 | `		}` |
|     ! 0 | 4131 | `	}` |
|       - | 4132 | `	/* Swap token streams */` |
|      51 | 4133 | `	pTmp = pGen->pEnd;` |
|      51 | 4134 | `	pGen->pEnd = pEnd;` |
|       - | 4135 | `	/* Compile the expression */` |
|      51 | 4136 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      51 | 4137 | `	if( rc == SXERR_ABORT ){` |
|       - | 4138 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 4139 | `		return SXERR_ABORT;` |
|       - | 4140 | `	}` |
|       - | 4141 | `	/* Update token stream */` |
|      51 | 4142 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 4143 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4144 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 4145 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4146 | `			return SXERR_ABORT;` |
|       - | 4147 | `		}` |
|     ! 0 | 4148 | `		pGen->pIn++;` |
|     ! 0 | 4149 | `	}` |
|      51 | 4150 | `	pGen->pIn  = &pEnd[1];` |
|      51 | 4151 | `	pGen->pEnd = pTmp;` |
|      51 | 4152 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      46 | 4153 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 | 4154 | `			pTmp = pGen->pIn;` |
|     ! 0 | 4155 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 | 4156 | `				pTmp--;` |
|     ! 0 | 4157 | `			}` |
|       - | 4158 | `			/* Unexpected token */` |
|     ! 0 | 4159 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 | 4160 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4161 | `				return SXERR_ABORT;` |
|       - | 4162 | `			}` |
|     ! 0 | 4163 | `			goto Synchronize;` |
|       - | 4164 | `	}` |
|       - | 4165 | `	/* Set the delimiter token */` |
|      51 | 4166 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       5 | 4167 | `		nToken = PH7_TK_KEYWORD;` |
|       - | 4168 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       3 | 4169 | `	}else{` |
|      47 | 4170 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - | 4171 | `	}` |
|      51 | 4172 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - | 4173 | `	/* Create the switch blocks container */` |
|      51 | 4174 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      51 | 4175 | `	if( pSwitch == 0 ){` |
|       - | 4176 | `		/* Abort compilation */` |
|     ! 0 | 4177 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4178 | `		return SXERR_ABORT;` |
|       - | 4179 | `	}` |
|       - | 4180 | `	/* Zero the structure */` |
|      51 | 4181 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - | 4182 | `	/* Initialize fields */` |
|      51 | 4183 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - | 4184 | `	/* Emit the switch instruction */` |
|      51 | 4185 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - | 4186 | `	/* Compile case blocks */` |
|     127 | 4187 | `	for(;;){` |
|       - | 4188 | `		sxu32 nKwrd;` |
|     155 | 4189 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 4190 | `			/* No more input to process */` |
|     ! 0 | 4191 | `			break;` |
|       - | 4192 | `		}` |
|     155 | 4193 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 4194 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - | 4195 | `				/* Unexpected token */` |
|     ! 0 | 4196 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 4197 | `					&pGen->pIn->sData);` |
|     ! 0 | 4198 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 4199 | `					return SXERR_ABORT;` |
|       - | 4200 | `				}` |
|       - | 4201 | `				/* FALL THROUGH */` |
|     ! 0 | 4202 | `			}` |
|       - | 4203 | `			/* Block compiled */` |
|     ! 0 | 4204 | `			break;` |
|       - | 4205 | `		}` |
|       - | 4206 | `		/* Extract the keyword */` |
|     155 | 4207 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     155 | 4208 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       5 | 4209 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - | 4210 | `				/* Unexpected token */` |
|     ! 0 | 4211 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 4212 | `					&pGen->pIn->sData);` |
|     ! 0 | 4213 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 4214 | `					return SXERR_ABORT;` |
|       - | 4215 | `				}` |
|       - | 4216 | `				/* FALL THROUGH */` |
|     ! 0 | 4217 | `			}` |
|       - | 4218 | `			/* Block compiled */` |
|       5 | 4219 | `			break;` |
|       - | 4220 | `		}` |
|     151 | 4221 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - | 4222 | `			/*` |
|       - | 4223 | `			 * Accroding to the PHP language reference manual` |
|       - | 4224 | `			 *  A special case is the default case. This case matches anything` |
|       - | 4225 | `			 *  that wasn't matched by the other cases.` |
|       - | 4226 | `			 */` |
|      39 | 4227 | `			if( pSwitch->nDefault > 0 ){` |
|       - | 4228 | `				/* Default case already compiled */` |
|     ! 0 | 4229 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 | 4230 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 4231 | `					return SXERR_ABORT;` |
|       - | 4232 | `				}` |
|     ! 0 | 4233 | `			}` |
|      39 | 4234 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - | 4235 | `			/* Compile the default block */` |
|      39 | 4236 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      39 | 4237 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 4238 | `				return SXERR_ABORT;` |
|      39 | 4239 | `			}else if( rc == SXERR_EOF ){` |
|      37 | 4240 | `				break;` |
|       1 | 4241 | `			}` |
|     118 | 4242 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - | 4243 | `			ph7_case_expr sCase;` |
|       - | 4244 | `			/* Standard case block */` |
|     117 | 4245 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - | 4246 | `			/* initialize the structure */` |
|     117 | 4247 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 4248 | `			/* Compile the case expression */` |
|     117 | 4249 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|     117 | 4250 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4251 | `				return SXERR_ABORT;` |
|       - | 4252 | `			}` |
|       - | 4253 | `			/* Compile the case block */` |
|     117 | 4254 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - | 4255 | `			/* Insert in the switch container */` |
|     117 | 4256 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|     117 | 4257 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 4258 | `				return SXERR_ABORT;` |
|     117 | 4259 | `			}else if( rc == SXERR_EOF ){` |
|      12 | 4260 | `				break;` |
|       - | 4261 | `			}` |
|      56 | 4262 | `		}else{` |
|       - | 4263 | `			/* Unexpected token */` |
|     ! 0 | 4264 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 4265 | `				&pGen->pIn->sData);` |
|     ! 0 | 4266 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4267 | `				return SXERR_ABORT;` |
|       - | 4268 | `			}` |
|     ! 0 | 4269 | `			break;` |
|       - | 4270 | `		}` |
|       5 | 4271 | `	}` |
|       - | 4272 | `	/* Fix all jumps now the destination is resolved */` |
|      51 | 4273 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      51 | 4274 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 4275 | `	/* Release the loop block */` |
|      51 | 4276 | `	GenStateLeaveBlock(pGen,0);` |
|      51 | 4277 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - | 4278 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      51 | 4279 | `		pGen->pIn++;` |
|      23 | 4280 | `	}` |
|       - | 4281 | `	/* Statement successfully compiled */` |
|      51 | 4282 | `	return SXRET_OK;` |
|     ! 0 | 4283 | `Synchronize:` |
|       - | 4284 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 4285 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 | 4286 | `		pGen->pIn++;` |
|     ! 0 | 4287 | `	}` |
|     ! 0 | 4288 | `	return SXRET_OK;` |
|      28 | 4289 | `}` |
|       - | 4290 |  |
