# src/ph7/compile_stmt.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1532/2021 lines (75.80%)

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
|      108 |   40 | `PH7_PRIVATE sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|        5 |   41 | `{` |
|        - |   42 | `	SySet *pConsCode,*pInstrContainer;` |
|        - |   43 | `	sxu32 nLineLocal;` |
|        - |   44 | `	SyString *pName;` |
|        - |   45 | `	sxi32 rc;` |
|        - |   46 | `	/* php forbids attributes on a comma-separated const list. Snapshot whether the` |
|        - |   47 | `	 * statement carries any now, before the first constant consumes them. */` |
|      113 |   48 | `	int bHadAttrs = SySetUsed(&pGen->aPendingAttrs) > 0;` |
|        - |   49 | ``	/* php attributes every const-statement compile error to the `const` keyword's`` |
|        - |   50 | ``	 * line, not the offending list element's own line (`const A=1,\nB=strlen()` blames`` |
|        - |   51 | `	 * line 1). Capture it once here, before jumping the keyword. */` |
|      113 |   52 | `	nLineLocal = pGen->pIn->nLine;` |
|      113 |   53 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|        - |   54 | ``	/* php allows a single `const` statement to declare several constants at once`` |
|        - |   55 | ``	 * (`const A = 1, B = 2;`). Loop over the comma-separated name = value pairs;`` |
|        - |   56 | `	 * PH7_CompileExpr(EXPR_FLAG_COMMA_STATEMENT) stops each value at the first` |
|        - |   57 | `	 * top-level comma so the next pair starts cleanly. */` |
|       59 |   58 | `Loop:` |
|      123 |   59 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |   60 | `		/* Invalid constant name */` |
|        8 |   61 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"identifier");` |
|        8 |   62 | `		if( rc == SXERR_ABORT ){` |
|        - |   63 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |   64 | `			return SXERR_ABORT;` |
|        - |   65 | `		}` |
|        8 |   66 | `		goto Synchronize;` |
|        - |   67 | `	}` |
|        - |   68 | `	/* Peek constant name */` |
|      117 |   69 | `	pName = &pGen->pIn->sData;` |
|        - |   70 | `	/* Make sure the constant name isn't reserved */` |
|      117 |   71 | `	if( GenStateIsReservedConstant(pName) ){` |
|        - |   72 | `		/* Reserved constant */` |
|       10 |   73 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Cannot redeclare constant '%z'",pName);` |
|       10 |   74 | `		if( rc == SXERR_ABORT ){` |
|        - |   75 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |   76 | `			return SXERR_ABORT;` |
|        - |   77 | `		}` |
|       10 |   78 | `		goto Synchronize;` |
|        - |   79 | `	}` |
|      109 |   80 | `	pGen->pIn++;` |
|      109 |   81 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|        - |   82 | `		/* Invalid statement*/` |
|        6 |   83 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"=\"");` |
|        6 |   84 | `		if( rc == SXERR_ABORT ){` |
|        - |   85 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |   86 | `			return SXERR_ABORT;` |
|        - |   87 | `		}` |
|        6 |   88 | `		goto Synchronize;` |
|        - |   89 | `	}` |
|      105 |   90 | `	pGen->pIn++; /*Jump the equal sign */` |
|        - |   91 | ``	/* php: a closure in a constant expression must be `static function`; a`` |
|        - |   92 | `	 * non-static closure and any arrow fn are compile-time fatals with distinct` |
|        - |   93 | `	 * messages. Checked ahead of the call scan (it skips closure bodies). */` |
|        - |   94 | `	{` |
|      105 |   95 | `		int iClo = PH7_GenStateInitClosureError(pGen);` |
|      105 |   96 | `		if( iClo ){` |
|        8 |   97 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|        2 |   98 | `				iClo == 2 ? "Closures in constant expressions must be static"` |
|        - |   99 | `				          : "Constant expression contains invalid operations");` |
|        6 |  100 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  101 | `				return SXERR_ABORT;` |
|        - |  102 | `			}` |
|        6 |  103 | `			goto Synchronize;` |
|        - |  104 | `		}` |
|        - |  105 | `	}` |
|        - |  106 | `	/* php: a constant expression may not CALL anything --` |
|        - |  107 | `	 * "Constant expression contains invalid operations". PHL used to evaluate the` |
|        - |  108 | ``	 * call happily, so `const X = strlen("ab");` defined X as 2. */`` |
|      101 |  109 | `	if( PH7_GenStateInitHasCallExpr(pGen) ){` |
|        6 |  110 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|        - |  111 | `			"Constant expression contains invalid operations");` |
|        6 |  112 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  113 | `			return SXERR_ABORT;` |
|        - |  114 | `		}` |
|        6 |  115 | `		goto Synchronize;` |
|        - |  116 | `	}` |
|        - |  117 | `	/* Allocate a new constant value container */` |
|       97 |  118 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|       97 |  119 | `	if( pConsCode == 0 ){` |
|      ! 0 |  120 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  121 | `		return SXERR_ABORT;` |
|        - |  122 | `	}` |
|       97 |  123 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - |  124 | `	/* Swap bytecode container */` |
|       97 |  125 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       97 |  126 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|        - |  127 | ``	/* Compile constant value. php: a stray token after `const X = EXPR` is`` |
|        - |  128 | ``	 * `... expecting "," or ";"` (const supports a comma-separated list).`` |
|        - |  129 | `	 * EXPR_FLAG_COMMA_STATEMENT stops this value at the first top-level comma so a` |
|        - |  130 | `	 * following declaration is left for the loop below. */` |
|        - |  131 | `	{` |
|       97 |  132 | `		const char *zSaveConst = pGen->zClauseCloser;` |
|       97 |  133 | `		pGen->zClauseCloser = "\",\" or \";\"";` |
|       97 |  134 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|       97 |  135 | `		pGen->zClauseCloser = zSaveConst;` |
|        - |  136 | `	}` |
|        - |  137 | `	/* Emit the done instruction */` |
|       97 |  138 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       97 |  139 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       97 |  140 | `	if( rc == SXERR_ABORT ){` |
|        - |  141 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  142 | `		return SXERR_ABORT;` |
|        - |  143 | `	}` |
|        - |  144 | ``	/* php parse-errors an empty initializer (`const A = ;`, or a `,B=` list hole);`` |
|        - |  145 | `	 * the class-const path rejects it too. Reject loudly rather than silently` |
|        - |  146 | `	 * defining a NULL constant. (php's error KIND -- a parse error -- differs from` |
|        - |  147 | `	 * PHL's compile fatal, the recursive-descent-vs-bison family; both refuse.) */` |
|       97 |  148 | `	if( rc == SXERR_EMPTY && PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|        2 |  149 | `			"Empty constant '%z' value",pName) == SXERR_ABORT ){` |
|      ! 0 |  150 | `		return SXERR_ABORT;` |
|        - |  151 | `	}` |
|       97 |  152 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|        - |  153 | `	/* Register the constant with namespace-qualified name */` |
|        - |  154 | `	{` |
|        - |  155 | `		SyBlob sFQN;` |
|        - |  156 | `		SyString sFQNStr;` |
|       97 |  157 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       97 |  158 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|       97 |  159 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      143 |  160 | `		rc = PH7_VmRegisterConstantEx(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode,` |
|       92 |  161 | `			(SyString *)SySetPeek(&pGen->pVm->aFiles),nLineLocal,1);` |
|       97 |  162 | `		if( rc == SXRET_OK && SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|        - |  163 | ``			/* php 8.5: attributes on `const` statements — attach the pending`` |
|        - |  164 | `			 * groups to the registered constant record for Reflection. */` |
|       11 |  165 | `			SyHashEntry *pCEntry = SyHashGet(&pGen->pVm->hConstant,` |
|        6 |  166 | `				SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|        8 |  167 | `			if( pCEntry ){` |
|        8 |  168 | `				ph7_constant *pRegCons = (ph7_constant *)pCEntry->pUserData;` |
|        8 |  169 | `				if( GenStateConsumeAttrs(&(*pGen),&pRegCons->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  170 | `					SyBlobRelease(&sFQN);` |
|      ! 0 |  171 | `					return SXERR_ABORT;` |
|        - |  172 | `				}` |
|        3 |  173 | `			}` |
|        3 |  174 | `		}` |
|       97 |  175 | `		SyBlobRelease(&sFQN);` |
|        - |  176 | `	}` |
|       97 |  177 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  178 | `		SySetRelease(pConsCode);` |
|      ! 0 |  179 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|      ! 0 |  180 | `	}` |
|        - |  181 | ``	/* Another declaration in the same statement: `const A = 1, B = 2;`. */`` |
|       97 |  182 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /* ',' */) ){` |
|       14 |  183 | `		if( bHadAttrs ){` |
|        - |  184 | ``			/* php compile-fatals `#[Attr] const A = 1, B = 2;` outright. */`` |
|        3 |  185 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|        - |  186 | `				"Cannot apply attributes to multiple constants at once");` |
|        3 |  187 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  188 | `				return SXERR_ABORT;` |
|        - |  189 | `			}` |
|        3 |  190 | `			goto Synchronize;` |
|        - |  191 | `		}` |
|       12 |  192 | `		pGen->pIn++; /* Jump the comma */` |
|       12 |  193 | `		goto Loop;` |
|        - |  194 | `	}` |
|       85 |  195 | `	return SXRET_OK;` |
|       14 |  196 | `Synchronize:` |
|        - |  197 | `	/* Synchronize with the next top-level semicolon and avoid compiling this` |
|        - |  198 | ``	 * erroneous statement. BRACE-aware (only `{`...`}`): a rejected closure`` |
|        - |  199 | ``	 * initializer's body holds inner `;` that are not statement terminators, so`` |
|        - |  200 | ``	 * without this its `;` and `}` dangle into a spurious second error where php`` |
|        - |  201 | `	 * halts at the first fatal. Parentheses and brackets are deliberately NOT` |
|        - |  202 | ``	 * tracked -- a lone unbalanced `(`/`[` in erroneous input must not swallow the`` |
|        - |  203 | ``	 * following statements (which would drop later error reports); a `{` never`` |
|        - |  204 | `	 * appears in a valid global-const initializer except as a closure body. */` |
|        - |  205 | `	{` |
|       32 |  206 | `		int iBrace = 0;` |
|      122 |  207 | `		while( pGen->pIn < pGen->pEnd ){` |
|      122 |  208 | `			if( iBrace == 0 && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       32 |  209 | `				break;` |
|        - |  210 | `			}` |
|       94 |  211 | `			if( pGen->pIn->nType & PH7_TK_OCB ){` |
|        3 |  212 | `				iBrace++;` |
|       93 |  213 | `			}else if( pGen->pIn->nType & PH7_TK_CCB ){` |
|        3 |  214 | `				if( iBrace > 0 ){ iBrace--; }` |
|        1 |  215 | `			}` |
|       94 |  216 | `			pGen->pIn++;` |
|        4 |  217 | `		}` |
|        - |  218 | `	}` |
|       32 |  219 | `	return SXRET_OK;` |
|       59 |  220 | `}` |
|        - |  221 | `/*` |
|        - |  222 | ` * Compile the 'continue' statement.` |
|        - |  223 | ` * According to the PHP language reference` |
|        - |  224 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|        - |  225 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|        - |  226 | ` *  iteration.` |
|        - |  227 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|        - |  228 | ` *  the purposes of continue.` |
|        - |  229 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|        - |  230 | ` *  of enclosing loops it should skip to the end of.` |
|        - |  231 | ` *  Note:` |
|        - |  232 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|        - |  233 | ` */` |
|        - |  234 | `/*` |
|        - |  235 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|        - |  236 | ` * block and the target loop block. This ensures finally blocks run when` |
|        - |  237 | ` * break/continue crosses a try boundary.` |
|        - |  238 | ` *` |
|        - |  239 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|        - |  240 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|        - |  241 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|        - |  242 | ` */` |
|   194746 |  243 | `static int GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|        5 |  244 | `{` |
|   194751 |  245 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   194751 |  246 | `	int nInlineTry = 0;` |
|   787043 |  247 | `	while( pBlock && pBlock != pTarget ){` |
|   592297 |  248 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|        6 |  249 | `			if( pBlock->pUserData ){` |
|        - |  250 | `				/* A try block with an exception context. In a generator its catch/finally` |
|        - |  251 | `				 * are inlined: count it so the caller emits a single OP_SET_FINALLY_JMP that` |
|        - |  252 | `				 * runs each crossed finally (VmFinallyAdvance) before taking the loop jump.` |
|        - |  253 | `				 * Legacy path: emit POP_EXCEPTION per crossed try as before. */` |
|        6 |  254 | `				if( pGen->bInGenerator ){` |
|        3 |  255 | `					nInlineTry++;` |
|        2 |  256 | `				}else{` |
|        3 |  257 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|        - |  258 | `				}` |
|        4 |  259 | `			}else{` |
|        - |  260 | `				/* A catch/finally block compiled into a separate bytecode container` |
|        - |  261 | `				 * (legacy). Stop — cannot cross into the parent try from a sub-execution. */` |
|      ! 0 |  262 | `				break;` |
|        - |  263 | `			}` |
|        2 |  264 | `		}` |
|   592297 |  265 | `		pBlock = pBlock->pParent;` |
|        5 |  266 | `	}` |
|   194751 |  267 | `	return nInlineTry;` |
|        5 |  268 | `}` |
|    99414 |  269 | `PH7_PRIVATE sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|        5 |  270 | `{` |
|        - |  271 | `	GenBlock *pLoop; /* Target loop */` |
|        - |  272 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|        - |  273 | `	sxi32 iRawLevel; /* The level as WRITTEN, kept for php's diagnostics */` |
|        - |  274 | `	sxu32 nLineLocal;` |
|        - |  275 | `	sxi32 rc;` |
|    99419 |  276 | `	iRawLevel = 1;` |
|    99419 |  277 | `	nLineLocal = pGen->pIn->nLine;` |
|    99419 |  278 | `	iLevel = 0;` |
|        - |  279 | `	/* Jump the 'continue' keyword */` |
|    99419 |  280 | `	pGen->pIn++;` |
|    99419 |  281 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|        - |  282 | `		/* optional numeric argument which tells us how many levels` |
|        - |  283 | `		 * of enclosing loops we should skip to the end of.` |
|        - |  284 | `		 */` |
|        - |  285 | `		char zScratch[GEN_NUM_SCRATCH];` |
|       17 |  286 | `		char *zAlloc = 0;` |
|        - |  287 | `		SyString sNum;` |
|       17 |  288 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|       17 |  289 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  290 | `			return SXERR_ABORT;` |
|        - |  291 | `		}` |
|       17 |  292 | `		if( rc == SXRET_OK ){` |
|       20 |  293 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|       12 |  294 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|       14 |  295 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  296 | `				return SXERR_ABORT;` |
|        - |  297 | `			}` |
|       14 |  298 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|       14 |  299 | `			iRawLevel = iLevel;` |
|       14 |  300 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        6 |  301 | `		}` |
|       17 |  302 | `		if( iLevel < 2 ){` |
|        3 |  303 | `			iLevel = 0;` |
|        1 |  304 | `		}` |
|       17 |  305 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|        7 |  306 | `	}` |
|        - |  307 | `	/* php rejects a non-positive level outright, before asking where it lands. */` |
|    99419 |  308 | `	if( iRawLevel < 1 ){` |
|      ! 0 |  309 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|        - |  310 | `			"'continue' operator accepts only positive integers");` |
|      ! 0 |  311 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  312 | `			return SXERR_ABORT;` |
|        - |  313 | `		}` |
|      ! 0 |  314 | `		return SXRET_OK;` |
|        - |  315 | `	}` |
|        - |  316 | `	/* Point to the target loop */` |
|    99419 |  317 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    99419 |  318 | `	if( pLoop == 0 ){` |
|        - |  319 | ``		/* Same split php makes for `break`: no loop at all keeps the not-in-context`` |
|        - |  320 | ``		 * wording, too FEW loops is `Cannot 'continue' N levels`. */`` |
|       12 |  321 | `		if( GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,0) != 0 ){` |
|      ! 0 |  322 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|      ! 0 |  323 | `				"Cannot 'continue' %d level%s",iRawLevel,iRawLevel == 1 ? "" : "s");` |
|      ! 0 |  324 | `		}else{` |
|       12 |  325 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"'continue' not in the 'loop' or 'switch' context");` |
|        - |  326 | `		}` |
|       12 |  327 | `		if( rc == SXERR_ABORT ){` |
|        - |  328 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  329 | `			return SXERR_ABORT;` |
|        - |  330 | `		}` |
|        7 |  331 | `	}else{` |
|    99409 |  332 | `		sxu32 nInstrIdx = 0;` |
|        - |  333 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|    99409 |  334 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|        - |  335 | `		/* ROOT C: in a generator, a break/continue crossing inline trys must run their` |
|        - |  336 | `		 * finallys first. OP_SET_FINALLY_JMP(iP1=count) does that then takes the loop jump. */` |
|    99409 |  337 | `		sxi32 iJmpOp = nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP;` |
|    99409 |  338 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|        - |  339 | ``			/* `continue` inside a switch acts like `break` — which is almost never what`` |
|        - |  340 | `			 * the author meant, so php 7.3+ says so at compile time. The generated jump` |
|        - |  341 | `			 * is unchanged; only the diagnostic was missing. An explicit level` |
|        - |  342 | ``			 * (`continue 2`) targets the enclosing loop and stays silent. */`` |
|        5 |  343 | `			if( iLevel < 1 ){` |
|        5 |  344 | `				PH7_GenCompileError(&(*pGen),E_WARNING,nLineLocal,` |
|        - |  345 | `					"\"continue\" targeting switch is equivalent to \"break\"."` |
|        - |  346 | `					" Did you mean to use \"continue 2\"?");` |
|        2 |  347 | `			}` |
|        5 |  348 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,0,0,&nInstrIdx);` |
|        5 |  349 | `			if( rc == SXRET_OK ){` |
|        5 |  350 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|        2 |  351 | `			}` |
|        3 |  352 | `		}else{` |
|        - |  353 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    99405 |  354 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    99405 |  355 | `			if( pLoop->bPostContinue == TRUE ){` |
|        - |  356 | `				JumpFixup sJumpFix;` |
|        - |  357 | `				/* Post-continue */` |
|    28997 |  358 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|    28997 |  359 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|    28997 |  360 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|    14496 |  361 | `			}` |
|        - |  362 | `		}` |
|        - |  363 | `	}` |
|    99419 |  364 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  365 | `		/* Not so fatal,emit a warning only */` |
|      ! 0 |  366 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|      ! 0 |  367 | `	}` |
|        - |  368 | `	/* Statement successfully compiled */` |
|    99419 |  369 | `	return SXRET_OK;` |
|    49712 |  370 | `}` |
|        - |  371 | `/*` |
|        - |  372 | ` * Compile the 'break' statement.` |
|        - |  373 | ` * According to the PHP language reference` |
|        - |  374 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|        - |  375 | ` *  structure.` |
|        - |  376 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|        - |  377 | ` *  enclosing structures are to be broken out of.` |
|        - |  378 | ` */` |
|    95358 |  379 | `PH7_PRIVATE sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|        5 |  380 | `{` |
|        - |  381 | `	GenBlock *pLoop; /* Target loop */` |
|        - |  382 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|        - |  383 | `	sxi32 iRawLevel; /* The level as WRITTEN, kept for php's diagnostics */` |
|        - |  384 | `	sxi32 rc;` |
|    95363 |  385 | `	iLevel = 0;` |
|    95363 |  386 | `	iRawLevel = 1;` |
|        - |  387 | `	/* Jump the 'break' keyword */` |
|    95363 |  388 | `	pGen->pIn++;` |
|    95363 |  389 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|        - |  390 | `		/* optional numeric argument which tells us how many levels` |
|        - |  391 | `		 * of enclosing loops we should skip to the end of.` |
|        - |  392 | `		 */` |
|        - |  393 | `		char zScratch[GEN_NUM_SCRATCH];` |
|       17 |  394 | `		char *zAlloc = 0;` |
|        - |  395 | `		SyString sNum;` |
|       17 |  396 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|       17 |  397 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  398 | `			return SXERR_ABORT;` |
|        - |  399 | `		}` |
|       17 |  400 | `		if( rc == SXRET_OK ){` |
|       21 |  401 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|       12 |  402 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|       15 |  403 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  404 | `				return SXERR_ABORT;` |
|        - |  405 | `			}` |
|       15 |  406 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|       15 |  407 | `			iRawLevel = iLevel;` |
|       15 |  408 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        6 |  409 | `		}` |
|       17 |  410 | `		if( iLevel < 2 ){` |
|        3 |  411 | `			iLevel = 0;` |
|        1 |  412 | `		}` |
|       17 |  413 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|        7 |  414 | `	}` |
|        - |  415 | `	/* php rejects a non-positive level outright, before asking where it lands. */` |
|    95363 |  416 | `	if( iRawLevel < 1 ){` |
|      ! 0 |  417 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  418 | `			"'break' operator accepts only positive integers");` |
|      ! 0 |  419 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  420 | `			return SXERR_ABORT;` |
|        - |  421 | `		}` |
|      ! 0 |  422 | `		goto BreakLevelDone;` |
|        - |  423 | `	}` |
|        - |  424 | `	/* Extract the target loop */` |
|    95363 |  425 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|   143042 |  426 | `	if( pLoop == 0 ){` |
|        - |  427 | `		/* php distinguishes "there is no loop at all here" from "there is one, but` |
|        - |  428 | `		 * not N of them": the first keeps the not-in-context wording, the second is` |
|        - |  429 | ``		 * `Cannot 'break' N levels`. */`` |
|       19 |  430 | `		if( GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,0) != 0 ){` |
|        4 |  431 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        1 |  432 | `				"Cannot 'break' %d level%s",iRawLevel,iRawLevel == 1 ? "" : "s");` |
|        2 |  433 | `		}else{` |
|       16 |  434 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"'break' not in the 'loop' or 'switch' context");` |
|        - |  435 | `		}` |
|       19 |  436 | `		if( rc == SXERR_ABORT ){` |
|        - |  437 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  438 | `			return SXERR_ABORT;` |
|        - |  439 | `		}` |
|       11 |  440 | `	}else{` |
|        - |  441 | `		sxu32 nInstrIdx;` |
|        - |  442 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|    95347 |  443 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|        - |  444 | `		/* ROOT C: OP_SET_FINALLY_JMP runs the crossed inline finallys before the break jump. */` |
|    95347 |  445 | `		rc = PH7_VmEmitInstr(pGen->pVm,nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP,nCross,0,0,&nInstrIdx);` |
|    95347 |  446 | `		if( rc == SXRET_OK ){` |
|        - |  447 | `			/* Fix the jump later when the jump destination is resolved */` |
|    95347 |  448 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|    47671 |  449 | `		}` |
|        - |  450 | `	}` |
|    47679 |  451 | `BreakLevelDone:` |
|    95363 |  452 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  453 | `		/* Not so fatal,emit a warning only */` |
|      ! 0 |  454 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|      ! 0 |  455 | `	}` |
|        - |  456 | `	/* Statement successfully compiled */` |
|    95363 |  457 | `	return SXRET_OK;` |
|    47684 |  458 | `}` |
|        - |  459 | `/*` |
|        - |  460 | ` * Compile or record a label.` |
|        - |  461 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|        - |  462 | ` * Example` |
|        - |  463 | ` *  goto LABEL;` |
|        - |  464 | ` *   echo 'Foo';` |
|        - |  465 | ` *  LABEL:` |
|        - |  466 | ` *   echo 'Bar';` |
|        - |  467 | ` */` |
|      112 |  468 | `PH7_PRIVATE sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|        5 |  469 | `{` |
|        - |  470 | `	GenBlock *pBlock;` |
|        - |  471 | `	Label sLabel;` |
|        - |  472 | `	/* php places NO restriction on where a label may be DEFINED — inside a loop, a switch` |
|        - |  473 | `	 * or a try{} is all fine. The only rule is on the jump: you may not goto INTO a loop` |
|        - |  474 | `	 * or switch from outside it, which is checked once the labels are all known (see` |
|        - |  475 | `	 * GenStateFixJumps). Record the loop this label sits in so that check can run. */` |
|        - |  476 | `	{` |
|      117 |  477 | `		SyString *pTarget = &pGen->pIn->sData;` |
|        - |  478 | `		char *zDup;` |
|        - |  479 | `		/* Initialize label fields */` |
|      117 |  480 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|        - |  481 | `		/* Duplicate label name */` |
|      117 |  482 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|      117 |  483 | `		if( zDup == 0 ){` |
|      ! 0 |  484 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  485 | `			return SXERR_ABORT;` |
|        - |  486 | `		}` |
|      117 |  487 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|      117 |  488 | `		sLabel.bRef  = FALSE;` |
|      117 |  489 | `		sLabel.nLine = pGen->pIn->nLine;` |
|      117 |  490 | `		sLabel.nLoopId = pGen->nCurLoopId;` |
|      117 |  491 | `		pBlock = pGen->pCurrent;` |
|      233 |  492 | `		while( pBlock ){` |
|      143 |  493 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|       26 |  494 | `				break;` |
|        - |  495 | `			}` |
|        - |  496 | `			/* Point to the upper block */` |
|      121 |  497 | `			pBlock = pBlock->pParent;` |
|        5 |  498 | `		}` |
|      117 |  499 | `		if( pBlock ){` |
|       26 |  500 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       15 |  501 | `		}else{` |
|       95 |  502 | `			sLabel.pFunc = 0;` |
|        - |  503 | `		}` |
|        - |  504 | `		/* Insert in label set */` |
|      117 |  505 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|        - |  506 | `	}` |
|      117 |  507 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|      117 |  508 | `	return SXRET_OK;` |
|       61 |  509 | `}` |
|        - |  510 | `/*` |
|        - |  511 | ` * Compile the so hated 'goto' statement.` |
|        - |  512 | ` * You've probably been taught that gotos are bad, but this sort` |
|        - |  513 | ` * of rewriting  happens all the time, in fact every time you run` |
|        - |  514 | ` * a compiler it has to do this.` |
|        - |  515 | ` * According to the PHP language reference manual` |
|        - |  516 | ` *   The goto operator can be used to jump to another section in the program.` |
|        - |  517 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|        - |  518 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|        - |  519 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|        - |  520 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|        - |  521 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|        - |  522 | ` *   of a multi-level break` |
|        - |  523 | ` */` |
|      152 |  524 | `PH7_PRIVATE sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|        5 |  525 | `{` |
|        - |  526 | `	JumpFixup sJump;` |
|        - |  527 | `	sxi32 rc;` |
|      157 |  528 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|      157 |  529 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - |  530 | `		/* Missing label */` |
|      ! 0 |  531 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|      ! 0 |  532 | `		if( rc == SXERR_ABORT ){` |
|        - |  533 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  534 | `			return SXERR_ABORT;` |
|        - |  535 | `		}` |
|      ! 0 |  536 | `		return SXRET_OK;` |
|        - |  537 | `	}` |
|      157 |  538 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|        5 |  539 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn,"identifier");` |
|        5 |  540 | `		if( rc == SXERR_ABORT ){` |
|        - |  541 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  542 | `			return SXERR_ABORT;` |
|        - |  543 | `		}` |
|        3 |  544 | `	}else{` |
|      153 |  545 | `		SyString *pTarget = &pGen->pIn->sData;` |
|        - |  546 | `		GenBlock *pBlock;` |
|        - |  547 | `		char *zDup;` |
|        - |  548 | `		/* Prepare the jump destination */` |
|      153 |  549 | `		sJump.nJumpType = PH7_OP_JMP;` |
|      153 |  550 | `		sJump.nLine = pGen->pIn->nLine;` |
|        - |  551 | `		/* Duplicate label name */` |
|      153 |  552 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|      153 |  553 | `		if( zDup == 0 ){` |
|      ! 0 |  554 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  555 | `			return SXERR_ABORT;` |
|        - |  556 | `		}` |
|      153 |  557 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|        - |  558 | `		/* The loop/switch this goto sits in, for the "goto into a loop" check later. */` |
|      153 |  559 | `		sJump.nLoopId = pGen->nCurLoopId;` |
|        - |  560 | `		/* A goto inside a try{}/catch{} is legal php (jumping OUT of the block is fine);` |
|        - |  561 | `		 * only the owning function matters here, since a goto may not cross functions. */` |
|      153 |  562 | `		pBlock = pGen->pCurrent;` |
|      327 |  563 | `		while( pBlock ){` |
|      205 |  564 | `			if( pBlock->iFlags & GEN_BLOCK_FUNC ){` |
|       30 |  565 | `				break;` |
|        - |  566 | `			}` |
|        - |  567 | `			/* Point to the upper block */` |
|      179 |  568 | `			pBlock = pBlock->pParent;` |
|        5 |  569 | `		}` |
|      153 |  570 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|       30 |  571 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       17 |  572 | `		}else{` |
|      127 |  573 | `			sJump.pFunc = 0;` |
|        - |  574 | `		}` |
|        - |  575 | `		/* Emit the unconditional jump */` |
|      153 |  576 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|      153 |  577 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|       74 |  578 | `		}` |
|        - |  579 | `	}` |
|      157 |  580 | `	pGen->pIn++; /* Jump the label name */` |
|      157 |  581 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        3 |  582 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|        1 |  583 | `	}` |
|        - |  584 | `	/* Statement successfully compiled */` |
|      157 |  585 | `	return SXRET_OK;` |
|       81 |  586 | `}` |
|        - |  587 | `/*` |
|        - |  588 | ` * Point to the next PHP chunk that will be processed shortly.` |
|        - |  589 | ` * Return SXRET_OK on success. Any other return value indicates` |
|        - |  590 | ` * failure.` |
|        - |  591 | ` */` |
|       20 |  592 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|        1 |  593 | `{` |
|        - |  594 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|        - |  595 | `	sxu32 nRawObj;` |
|       10 |  596 | `	sxu32 nObjIdx;` |
|        - |  597 | `	/* Consume raw chunks verbatim without any processing until we get` |
|        - |  598 | `	 * a PHP block.` |
|        - |  599 | `	 */` |
|       10 |  600 | `Consume:` |
|       21 |  601 | `	nRawObj = nObjIdx = 0;` |
|       21 |  602 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|      ! 0 |  603 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|      ! 0 |  604 | `		if( pRawObj == 0 ){` |
|      ! 0 |  605 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  606 | `			return SXERR_ABORT;` |
|        - |  607 | `		}` |
|        - |  608 | `		/* Mark as constant and emit the load constant instruction */` |
|      ! 0 |  609 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|      ! 0 |  610 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|      ! 0 |  611 | `		++nRawObj;` |
|      ! 0 |  612 | `		pGen->pRawIn++; /* Next chunk */` |
|      ! 0 |  613 | `	}` |
|       21 |  614 | `	if( nRawObj > 0 ){` |
|        - |  615 | `		/* Emit the consume instruction */` |
|      ! 0 |  616 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|      ! 0 |  617 | `	}` |
|       21 |  618 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|      ! 0 |  619 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|        - |  620 | `		/* Reset the token set (and its trivia sidecar) */` |
|      ! 0 |  621 | `		SySetReset(pTokenSet);` |
|      ! 0 |  622 | `		SySetReset(&pGen->aTrivia);` |
|        - |  623 | `		/* Tokenize input */` |
|      ! 0 |  624 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|      ! 0 |  625 | `			pGen->pRawIn->nLine,pTokenSet,&pGen->aTrivia);` |
|        - |  626 | `		/* Point to the fresh token stream */` |
|      ! 0 |  627 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|      ! 0 |  628 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|        - |  629 | `		/* Advance the stream cursor */` |
|      ! 0 |  630 | `		pGen->pRawIn++;` |
|        - |  631 | `		/* TICKET 1433-011 */` |
|      ! 0 |  632 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|        - |  633 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|        - |  634 | `			sxi32 rc;` |
|        - |  635 | `			/* Refer to TICKET 1433-009  */` |
|      ! 0 |  636 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|      ! 0 |  637 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|      ! 0 |  638 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|        - |  639 | `			/* Synthesized short-tag echo: legitimately an expression here. */` |
|      ! 0 |  640 | `			pGen->nExprEchoOk++;` |
|      ! 0 |  641 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|      ! 0 |  642 | `			pGen->nExprEchoOk--;` |
|      ! 0 |  643 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  644 | `				return SXERR_ABORT;` |
|      ! 0 |  645 | `			}else if( rc != SXERR_EMPTY ){` |
|      ! 0 |  646 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      ! 0 |  647 | `			}` |
|      ! 0 |  648 | `			goto Consume;` |
|        - |  649 | `		}` |
|      ! 0 |  650 | `	}else{` |
|        - |  651 | `		/* No more chunks to process */` |
|       21 |  652 | `		pGen->pIn = pGen->pEnd;` |
|       21 |  653 | `		return SXERR_EOF;` |
|        - |  654 | `	}` |
|      ! 0 |  655 | `	return SXRET_OK;` |
|       11 |  656 | `}` |
|        - |  657 | `/*` |
|        - |  658 | ` * Compile a PHP block.` |
|        - |  659 | ` * A block is simply one or more PHP statements and expressions to compile` |
|        - |  660 | ` * optionally delimited by braces {}.` |
|        - |  661 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|        - |  662 | ` * and this function takes care of generating the appropriate error` |
|        - |  663 | ` * message.` |
|        - |  664 | ` */` |
|  7402756 |  665 | `PH7_PRIVATE sxi32 PH7_CompileBlock(` |
|        - |  666 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |  667 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|        - |  668 | `	)` |
|        5 |  669 | `{` |
|        - |  670 | `	sxi32 rc;` |
|        - |  671 | `	sxu32 nLine;` |
|  7402761 |  672 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  7269339 |  673 | `		nLine = pGen->pIn->nLine;` |
|  7269339 |  674 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  7269339 |  675 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  676 | `			return SXERR_ABORT;` |
|        - |  677 | `		}` |
|  7269339 |  678 | `		pGen->pIn++;` |
|        - |  679 | `		/* Compile until we hit the closing braces '}' */` |
| 10647025 |  680 | `		for(;;){` |
| 21294055 |  681 | `			if( pGen->pIn >= pGen->pEnd ){` |
|       21 |  682 | `				rc = GenStateNextChunk(&(*pGen));` |
|       21 |  683 | `				if (rc == SXERR_ABORT ){` |
|      ! 0 |  684 | `			 	   return SXERR_ABORT;` |
|        - |  685 | `				}` |
|       21 |  686 | `				if( rc == SXERR_EOF ){` |
|        - |  687 | `					/* No more token to process: the block was never closed. php reports` |
|        - |  688 | `					 * the line the '{' was opened on, not where the input ran out. */` |
|       21 |  689 | `					PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"Unclosed '{' on line %u",nLine);` |
|       21 |  690 | `					break;` |
|        - |  691 | `				}` |
|      ! 0 |  692 | `			}` |
| 21294035 |  693 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|        - |  694 | `				/* Closing braces found,break immediately*/` |
|  7269319 |  695 | `				pGen->pIn++;` |
|  7269319 |  696 | `				break;` |
|        - |  697 | `			}` |
|        - |  698 | `			/* Compile a single statement */` |
| 14024721 |  699 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
| 14024721 |  700 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  701 | `				return SXERR_ABORT;` |
|        - |  702 | `			}` |
|        5 |  703 | `		}` |
|  7269339 |  704 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  3768094 |  705 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|        9 |  706 | `		pGen->pIn++;` |
|        9 |  707 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|        9 |  708 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  709 | `			return SXERR_ABORT;` |
|        - |  710 | `		}` |
|        - |  711 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|        9 |  712 | `		for(;;){` |
|       19 |  713 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  714 | `				rc = GenStateNextChunk(&(*pGen));` |
|      ! 0 |  715 | `				if (rc == SXERR_ABORT ){` |
|      ! 0 |  716 | `			 	   return SXERR_ABORT;` |
|        - |  717 | `				}` |
|      ! 0 |  718 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|        - |  719 | `					/* No more token to process */` |
|      ! 0 |  720 | `					if( rc == SXERR_EOF ){` |
|      ! 0 |  721 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|        - |  722 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|      ! 0 |  723 | `					}` |
|      ! 0 |  724 | `					break;` |
|        - |  725 | `				}` |
|      ! 0 |  726 | `			}` |
|       19 |  727 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|        - |  728 | `				sxi32 nKwrd;` |
|        - |  729 | `				/* Keyword found */` |
|       17 |  730 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       18 |  731 | `				if( nKwrd == nKeywordEnd \|\|` |
|        5 |  732 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|        - |  733 | `						/* Delimiter keyword found,break */` |
|        9 |  734 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|        9 |  735 | `							pGen->pIn++; /*  endif;endswitch... */` |
|        4 |  736 | `						}` |
|        9 |  737 | `						break;` |
|        - |  738 | `				}` |
|        4 |  739 | `			}` |
|        - |  740 | `			/* Compile a single statement */` |
|       11 |  741 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|       11 |  742 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  743 | `				return SXERR_ABORT;` |
|        - |  744 | `			}` |
|        1 |  745 | `		}` |
|        9 |  746 | `		GenStateLeaveBlock(&(*pGen),0);` |
|        5 |  747 | `	}else{` |
|        - |  748 | `		/* Compile a single statement */` |
|   133419 |  749 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|   133419 |  750 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  751 | `			return SXERR_ABORT;` |
|        - |  752 | `		}` |
|        - |  753 | `	}` |
|        - |  754 | `	/* Jump trailing semi-colons ';' */` |
|  7402769 |  755 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|        9 |  756 | `		pGen->pIn++;` |
|        1 |  757 | `	}` |
|  7402761 |  758 | `	return SXRET_OK;` |
|  3701383 |  759 | `}` |
|        - |  760 | `/*` |
|        - |  761 | ` * Compile the gentle 'while' statement.` |
|        - |  762 | ` * According to the PHP language reference` |
|        - |  763 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|        - |  764 | ` *  The basic form of a while statement is:` |
|        - |  765 | ` *  while (expr)` |
|        - |  766 | ` *   statement` |
|        - |  767 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|        - |  768 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|        - |  769 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|        - |  770 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|        - |  771 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|        - |  772 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|        - |  773 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|        - |  774 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|        - |  775 | ` *  while (expr):` |
|        - |  776 | ` *    statement` |
|        - |  777 | ` *   endwhile;` |
|        - |  778 | ` */` |
|    78814 |  779 | `PH7_PRIVATE sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|        5 |  780 | `{` |
|    78819 |  781 | `	GenBlock *pWhileBlock = 0;` |
|    78819 |  782 | `	SyToken *pTmp,*pEnd = 0;` |
|        - |  783 | `	sxu32 nFalseJump;` |
|        - |  784 | `	sxu32 nLine;` |
|        - |  785 | `	sxi32 rc;` |
|    78819 |  786 | `	nLine = pGen->pIn->nLine;` |
|        - |  787 | `	/* Jump the 'while' keyword */` |
|    78819 |  788 | `	pGen->pIn++;` |
|    78819 |  789 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  790 | `		/* Syntax error */` |
|      ! 0 |  791 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|      ! 0 |  792 | `		if( rc == SXERR_ABORT ){` |
|        - |  793 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  794 | `			return SXERR_ABORT;` |
|        - |  795 | `		}` |
|      ! 0 |  796 | `		goto Synchronize;` |
|        - |  797 | `	}` |
|        - |  798 | `	/* Jump the left parenthesis '(' */` |
|    78819 |  799 | `	pGen->pIn++;` |
|        - |  800 | `	/* Create the loop block */` |
|    78819 |  801 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|    78819 |  802 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  803 | `		return SXERR_ABORT;` |
|        - |  804 | `	}` |
|        - |  805 | `	/* Delimit the condition */` |
|    78819 |  806 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    78819 |  807 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  808 | `		/* Empty condition. php reports the token that actually stopped it -- for` |
|        - |  809 | ``		 * `while ()` that is the ')' -- not a hand-written "expected expression". */`` |
|        3 |  810 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,0);` |
|        3 |  811 | `		if( rc == SXERR_ABORT ){` |
|        - |  812 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  813 | `			return SXERR_ABORT;` |
|        - |  814 | `		}` |
|        1 |  815 | `	}` |
|        - |  816 | `	/* Swap token streams */` |
|    78819 |  817 | `	pTmp = pGen->pEnd;` |
|    78819 |  818 | `	pGen->pEnd = pEnd;` |
|        - |  819 | `	/* Compile the expression */` |
|    78819 |  820 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    78819 |  821 | `	if( rc == SXERR_ABORT ){` |
|        - |  822 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  823 | `		return SXERR_ABORT;` |
|        - |  824 | `	}` |
|        - |  825 | `	/* Update token stream */` |
|    78819 |  826 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 |  827 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 |  828 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  829 | `			return SXERR_ABORT;` |
|        - |  830 | `		}` |
|      ! 0 |  831 | `		pGen->pIn++;` |
|      ! 0 |  832 | `	}` |
|        - |  833 | `	/* Synchronize pointers */` |
|    78819 |  834 | `	pGen->pIn  = &pEnd[1];` |
|    78819 |  835 | `	pGen->pEnd = pTmp;` |
|        - |  836 | `	/* Emit the false jump */` |
|    78819 |  837 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|        - |  838 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    78819 |  839 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|        - |  840 | `	/* Compile the loop body */` |
|    78819 |  841 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|    78819 |  842 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  843 | `		return SXERR_ABORT;` |
|        - |  844 | `	}` |
|        - |  845 | `	/* Emit the unconditional jump to the start of the loop */` |
|    78819 |  846 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|        - |  847 | `	/* Fix all jumps now the destination is resolved */` |
|    78819 |  848 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  849 | `	/* Release the loop block */` |
|    78819 |  850 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  851 | `	/* Statement successfully compiled */` |
|    78819 |  852 | `	return SXRET_OK;` |
|      ! 0 |  853 | `Synchronize:` |
|        - |  854 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - |  855 | `	 * compiling this erroneous block.` |
|        - |  856 | `	 */` |
|      ! 0 |  857 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  858 | `		pGen->pIn++;` |
|      ! 0 |  859 | `	}` |
|      ! 0 |  860 | `	return SXRET_OK;` |
|    39412 |  861 | `}` |
|        - |  862 | `/*` |
|        - |  863 | ` * Compile the ugly do..while() statement.` |
|        - |  864 | ` * According to the PHP language reference` |
|        - |  865 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|        - |  866 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|        - |  867 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|        - |  868 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|        - |  869 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|        - |  870 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|        - |  871 | ` *  would end immediately).` |
|        - |  872 | ` *  There is just one syntax for do-while loops:` |
|        - |  873 | ` *  <?php` |
|        - |  874 | ` *  $i = 0;` |
|        - |  875 | ` *  do {` |
|        - |  876 | ` *   echo $i;` |
|        - |  877 | ` *  } while ($i > 0);` |
|        - |  878 | ` * ?>` |
|        - |  879 | ` */` |
|        4 |  880 | `PH7_PRIVATE sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|        2 |  881 | `{` |
|        6 |  882 | `	SyToken *pTmp,*pEnd = 0;` |
|        6 |  883 | `	GenBlock *pDoBlock = 0;` |
|        - |  884 | `	sxu32 nLine;` |
|        - |  885 | `	sxi32 rc;` |
|        6 |  886 | `	nLine = pGen->pIn->nLine;` |
|        - |  887 | `	/* Jump the 'do' keyword */` |
|        6 |  888 | `	pGen->pIn++;` |
|        - |  889 | `	/* Create the loop block */` |
|        6 |  890 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|        6 |  891 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  892 | `		return SXERR_ABORT;` |
|        - |  893 | `	}` |
|        - |  894 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|        6 |  895 | `	pDoBlock->bPostContinue = TRUE;` |
|        6 |  896 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|        6 |  897 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  898 | `		return SXERR_ABORT;` |
|        - |  899 | `	}` |
|        6 |  900 | `	if( pGen->pIn < pGen->pEnd ){` |
|        3 |  901 | `		nLine = pGen->pIn->nLine;` |
|        1 |  902 | `	}` |
|        6 |  903 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|        2 |  904 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|        - |  905 | `			/* Missing 'while' statement */` |
|        - |  906 | ``			/* php: `do {} ;` names the ';', while `do {}` at EOF is "unexpected end of`` |
|        - |  907 | `			 * file". The do-block's own slice stops before its terminator, so look at` |
|        - |  908 | `			 * the whole CHUNK stream: a token still there is the one php names; nothing` |
|        - |  909 | `			 * left means end of file (NULL). */` |
|        - |  910 | `			{` |
|        3 |  911 | `				SyToken *pBad = pGen->pIn < pGen->pEnd ? pGen->pIn : 0;` |
|        3 |  912 | `				if( pBad == 0 && pGen->pTokenSet ){` |
|        - |  913 | `					/* The do-block consumed its terminator, so the token php names is` |
|        - |  914 | `					 * the one just behind the cursor -- but only when it is a real` |
|        - |  915 | ``					 * terminator (`do {} ;` -> the ';'). If the block simply ended on`` |
|        - |  916 | `					 * its '}' with nothing after it, php reports end of file. */` |
|        3 |  917 | `					SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        3 |  918 | `					if( pGen->pIn > pBase && (pGen->pIn[-1].nType & PH7_TK_SEMI) ){` |
|      ! 0 |  919 | `						pBad = &pGen->pIn[-1];` |
|      ! 0 |  920 | `					}` |
|        1 |  921 | `				}` |
|        3 |  922 | `				rc = PH7_GenSyntaxError(pGen,pBad,"\"while\"");` |
|        - |  923 | `			}` |
|        3 |  924 | `			if( rc == SXERR_ABORT ){` |
|        - |  925 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  926 | `				return SXERR_ABORT;` |
|        - |  927 | `			}` |
|        3 |  928 | `			goto Synchronize;` |
|        - |  929 | `	}` |
|        - |  930 | `	/* Jump the 'while' keyword */` |
|        3 |  931 | `	pGen->pIn++;` |
|        3 |  932 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  933 | `		/* Syntax error */` |
|      ! 0 |  934 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|      ! 0 |  935 | `		if( rc == SXERR_ABORT ){` |
|        - |  936 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  937 | `			return SXERR_ABORT;` |
|        - |  938 | `		}` |
|      ! 0 |  939 | `		goto Synchronize;` |
|        - |  940 | `	}` |
|        - |  941 | `	/* Jump the left parenthesis '(' */` |
|        3 |  942 | `	pGen->pIn++;` |
|        - |  943 | `	/* Delimit the condition */` |
|        3 |  944 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|        3 |  945 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  946 | `		/* Empty condition. php reports the token that actually stopped it -- for` |
|        - |  947 | ``		 * `while ()` that is the ')' -- not a hand-written "expected expression". */`` |
|      ! 0 |  948 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,0);` |
|      ! 0 |  949 | `		if( rc == SXERR_ABORT ){` |
|        - |  950 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  951 | `			return SXERR_ABORT;` |
|        - |  952 | `		}` |
|      ! 0 |  953 | `		goto Synchronize;` |
|        - |  954 | `	}` |
|        - |  955 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|        3 |  956 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|        - |  957 | `		JumpFixup *aPost;` |
|        - |  958 | `		VmInstr *pInstr;` |
|        - |  959 | `		sxu32 nJumpDest;` |
|        - |  960 | `		sxu32 n;` |
|      ! 0 |  961 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|      ! 0 |  962 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      ! 0 |  963 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|      ! 0 |  964 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      ! 0 |  965 | `			if( pInstr ){` |
|        - |  966 | `				/* Fix */` |
|      ! 0 |  967 | `				pInstr->iP2 = nJumpDest;` |
|      ! 0 |  968 | `			}` |
|      ! 0 |  969 | `		}` |
|      ! 0 |  970 | `	}` |
|        - |  971 | `	/* Swap token streams */` |
|        3 |  972 | `	pTmp = pGen->pEnd;` |
|        3 |  973 | `	pGen->pEnd = pEnd;` |
|        - |  974 | `	/* Compile the expression */` |
|        3 |  975 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        3 |  976 | `	if( rc == SXERR_ABORT ){` |
|        - |  977 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  978 | `		return SXERR_ABORT;` |
|        - |  979 | `	}` |
|        - |  980 | `	/* Update token stream */` |
|        3 |  981 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 |  982 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 |  983 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  984 | `			return SXERR_ABORT;` |
|        - |  985 | `		}` |
|      ! 0 |  986 | `		pGen->pIn++;` |
|      ! 0 |  987 | `	}` |
|        3 |  988 | `	pGen->pIn  = &pEnd[1];` |
|        3 |  989 | `	pGen->pEnd = pTmp;` |
|        - |  990 | `	/* Emit the true jump to the beginning of the loop */` |
|        3 |  991 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|        - |  992 | `	/* Fix all jumps now the destination is resolved */` |
|        3 |  993 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  994 | `	/* Release the loop block */` |
|        3 |  995 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  996 | `	/* Statement successfully compiled */` |
|        3 |  997 | `	return SXRET_OK;` |
|        1 |  998 | `Synchronize:` |
|        - |  999 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - | 1000 | `	 * compiling this erroneous block.` |
|        - | 1001 | `	 */` |
|        3 | 1002 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 | 1003 | `		pGen->pIn++;` |
|      ! 0 | 1004 | `	}` |
|        3 | 1005 | `	return SXRET_OK;` |
|        4 | 1006 | `}` |
|        - | 1007 | `/*` |
|        - | 1008 | ` * Compile the complex and powerful 'for' statement.` |
|        - | 1009 | ` * According to the PHP language reference` |
|        - | 1010 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|        - | 1011 | ` *  The syntax of a for loop is:` |
|        - | 1012 | ` *  for (expr1; expr2; expr3)` |
|        - | 1013 | ` *   statement` |
|        - | 1014 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|        - | 1015 | ` *  the beginning of the loop.` |
|        - | 1016 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|        - | 1017 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|        - | 1018 | ` *  to FALSE, the execution of the loop ends.` |
|        - | 1019 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|        - | 1020 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|        - | 1021 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|        - | 1022 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|        - | 1023 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|        - | 1024 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|        - | 1025 | ` *  of using the for truth expression.` |
|        - | 1026 | ` */` |
|   136798 | 1027 | `PH7_PRIVATE sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|        5 | 1028 | `{` |
|   136803 | 1029 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   136803 | 1030 | `	GenBlock *pForBlock = 0;` |
|        - | 1031 | `	sxu32 nFalseJump;` |
|        - | 1032 | `	sxu32 nLine;` |
|        - | 1033 | `	sxi32 rc;` |
|   136803 | 1034 | `	nLine = pGen->pIn->nLine;` |
|        - | 1035 | `	/* Jump the 'for' keyword */` |
|   136803 | 1036 | `	pGen->pIn++;` |
|   136803 | 1037 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1038 | `		/* Syntax error */` |
|      ! 0 | 1039 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|      ! 0 | 1040 | `		if( rc == SXERR_ABORT ){` |
|        - | 1041 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1042 | `			return SXERR_ABORT;` |
|        - | 1043 | `		}` |
|      ! 0 | 1044 | `		return SXRET_OK;` |
|        - | 1045 | `	}` |
|        - | 1046 | `	/* Jump the left parenthesis '(' */` |
|   136803 | 1047 | `	pGen->pIn++;` |
|        - | 1048 | `	/* Delimit the init-expr;condition;post-expr */` |
|   136803 | 1049 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   136803 | 1050 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - | 1051 | `		/* Empty expression */` |
|      ! 0 | 1052 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|      ! 0 | 1053 | `		if( rc == SXERR_ABORT ){` |
|        - | 1054 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1055 | `			return SXERR_ABORT;` |
|        - | 1056 | `		}` |
|        - | 1057 | `		/* Synchronize */` |
|      ! 0 | 1058 | `		pGen->pIn = pEnd;` |
|      ! 0 | 1059 | `		if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 | 1060 | `			pGen->pIn++;` |
|      ! 0 | 1061 | `		}` |
|      ! 0 | 1062 | `		return SXRET_OK;` |
|        - | 1063 | `	}` |
|        - | 1064 | `	/* Swap token streams */` |
|   136803 | 1065 | `	pTmp = pGen->pEnd;` |
|   136803 | 1066 | `	pGen->pEnd = pEnd;` |
|        - | 1067 | `	/* for() clauses are the ONLY place php's grammar allows a comma-separated` |
|        - | 1068 | `	 * expression list, so the comma operator is permitted for their duration` |
|        - | 1069 | `	 * (see GenStateTreeHasComma). A closure body nested inside a clause is` |
|        - | 1070 | `	 * compiled through this same window — recorded as a known leniency. */` |
|   136803 | 1071 | `	pGen->nCommaExprOk++;` |
|   136803 | 1072 | `	pGen->zClauseCloser = "\";\""; /* init/condition clauses close on ';' */` |
|        - | 1073 | `	/* Compile initialization expressions if available */` |
|   136803 | 1074 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 1075 | `	/* Pop operand lvalues */` |
|   136803 | 1076 | `	if( rc == SXERR_ABORT ){` |
|        - | 1077 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1078 | `		return SXERR_ABORT;` |
|   136803 | 1079 | `	}else if( rc != SXERR_EMPTY ){` |
|   124381 | 1080 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    62188 | 1081 | `	}` |
|   136803 | 1082 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - | 1083 | `		/* Syntax error */` |
|      ! 0 | 1084 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|      ! 0 | 1085 | `		if( rc == SXERR_ABORT ){` |
|        - | 1086 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1087 | `			return SXERR_ABORT;` |
|        - | 1088 | `		}` |
|      ! 0 | 1089 | `		return SXRET_OK;` |
|        - | 1090 | `	}` |
|        - | 1091 | `	/* Jump the trailing ';' */` |
|   136803 | 1092 | `	pGen->pIn++;` |
|        - | 1093 | `	/* Create the loop block */` |
|   136803 | 1094 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   136803 | 1095 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1096 | `		return SXERR_ABORT;` |
|        - | 1097 | `	}` |
|        - | 1098 | `	/* Deffer continue jumps */` |
|   136803 | 1099 | `	pForBlock->bPostContinue = TRUE;` |
|        - | 1100 | `	/* Compile the condition */` |
|   136803 | 1101 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   136803 | 1102 | `	if( rc == SXERR_ABORT ){` |
|        - | 1103 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1104 | `		return SXERR_ABORT;` |
|   136803 | 1105 | `	}else if( rc != SXERR_EMPTY ){` |
|        - | 1106 | `		/* Emit the false jump */` |
|   124381 | 1107 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|        - | 1108 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   124381 | 1109 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    62188 | 1110 | `	}` |
|   136803 | 1111 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - | 1112 | `		/* Syntax error */` |
|        6 | 1113 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|        6 | 1114 | `		if( rc == SXERR_ABORT ){` |
|        - | 1115 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1116 | `			return SXERR_ABORT;` |
|        - | 1117 | `		}` |
|        6 | 1118 | `		return SXRET_OK;` |
|        - | 1119 | `	}` |
|        - | 1120 | `	/* Jump the trailing ';' */` |
|   136799 | 1121 | `	pGen->pIn++;` |
|        - | 1122 | `	/* Save the post condition stream */` |
|   136799 | 1123 | `	pPostStart = pGen->pIn;` |
|        - | 1124 | `	/* Compile the loop body — OUTSIDE the comma window (the body is ordinary` |
|        - | 1125 | ``	 * php, so `(1, 2)` inside it is the parse error it should be). */`` |
|   136799 | 1126 | `	pGen->nCommaExprOk--;` |
|   136799 | 1127 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   136799 | 1128 | `	pGen->pEnd = pTmp;` |
|   136799 | 1129 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   136799 | 1130 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 1131 | `		return SXERR_ABORT;` |
|        - | 1132 | `	}` |
|        - | 1133 | `	/* Fix post-continue jumps */` |
|   136799 | 1134 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|        - | 1135 | `		JumpFixup *aPost;` |
|        - | 1136 | `		VmInstr *pInstr;` |
|        - | 1137 | `		sxu32 nJumpDest;` |
|        - | 1138 | `		sxu32 n;` |
|    12437 | 1139 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|    12437 | 1140 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|    41429 | 1141 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|    28997 | 1142 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|    28997 | 1143 | `			if( pInstr ){` |
|        - | 1144 | `				/* Fix jump */` |
|    28997 | 1145 | `				pInstr->iP2 = nJumpDest;` |
|    14496 | 1146 | `			}` |
|    14501 | 1147 | `		}` |
|     6216 | 1148 | `	}` |
|        - | 1149 | `	/* compile the post-expressions if available */` |
|   136799 | 1150 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|      ! 0 | 1151 | `		pPostStart++;` |
|      ! 0 | 1152 | `	}` |
|   136799 | 1153 | `	if( pPostStart < pEnd ){` |
|        - | 1154 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   124379 | 1155 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   124379 | 1156 | `		pGen->nCommaExprOk++; /* post-expressions are a clause list again */` |
|   124379 | 1157 | `		pGen->zClauseCloser = "\")\""; /* the post clause closes on ')' */` |
|   124379 | 1158 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   124379 | 1159 | `		pGen->nCommaExprOk--;` |
|   124379 | 1160 | `		pGen->zClauseCloser = 0;` |
|   124379 | 1161 | `		if( pGen->pIn < pGen->pEnd ){` |
|        - | 1162 | `			/* php names the token and expects ')'; the post-clause runs to the ')'. */` |
|      ! 0 | 1163 | `			rc = PH7_GenSyntaxError(pGen,pGen->pIn,"\")\"");` |
|      ! 0 | 1164 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1165 | `				return SXERR_ABORT;` |
|        - | 1166 | `			}` |
|      ! 0 | 1167 | `			return SXRET_OK;` |
|        - | 1168 | `		}` |
|   124379 | 1169 | `		RE_SWAP_DELIMITER(pGen);` |
|   124379 | 1170 | `		if( rc == SXERR_ABORT ){` |
|        - | 1171 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1172 | `			return SXERR_ABORT;` |
|   124379 | 1173 | `		}else if( rc != SXERR_EMPTY){` |
|        - | 1174 | `			/* Pop operand lvalue */` |
|   124379 | 1175 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    62187 | 1176 | `		}` |
|    62187 | 1177 | `	}` |
|        - | 1178 | `	/* Emit the unconditional jump to the start of the loop */` |
|   136799 | 1179 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|        - | 1180 | `	/* Fix all jumps now the destination is resolved */` |
|   136799 | 1181 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 1182 | `	/* Release the loop block */` |
|   136799 | 1183 | `	GenStateLeaveBlock(pGen,0);` |
|        - | 1184 | `	/* Statement successfully compiled */` |
|   136799 | 1185 | `	return SXRET_OK;` |
|    68404 | 1186 | `}` |
|        - | 1187 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|        - | 1188 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|        - | 1189 | ` * are allowed.` |
|        - | 1190 | ` */` |
|   485654 | 1191 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 1192 | `{` |
|   485659 | 1193 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|   485659 | 1194 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|        - | 1195 | `		/* Unexpected expression */` |
|      ! 0 | 1196 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 1197 | `			"foreach: Expecting a variable name");` |
|      ! 0 | 1198 | `		if( rc != SXERR_ABORT ){` |
|      ! 0 | 1199 | `			rc = SXERR_INVALID;` |
|      ! 0 | 1200 | `		}` |
|      ! 0 | 1201 | `	}` |
|   485659 | 1202 | `	return rc;` |
|        5 | 1203 | `}` |
|        - | 1204 | `/*` |
|        - | 1205 | ` * Compile the 'foreach' statement.` |
|        - | 1206 | ` * According to the PHP language reference` |
|        - | 1207 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|        - | 1208 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|        - | 1209 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|        - | 1210 | ` *  is a minor but useful extension of the first:` |
|        - | 1211 | ` *  foreach (array_expression as $value)` |
|        - | 1212 | ` *    statement` |
|        - | 1213 | ` *  foreach (array_expression as $key => $value)` |
|        - | 1214 | ` *   statement` |
|        - | 1215 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|        - | 1216 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|        - | 1217 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|        - | 1218 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|        - | 1219 | ` *  to the variable $key on each loop.` |
|        - | 1220 | ` *  Note:` |
|        - | 1221 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|        - | 1222 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|        - | 1223 | ` *  Note:` |
|        - | 1224 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|        - | 1225 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|        - | 1226 | ` *  or after the foreach without resetting it.` |
|        - | 1227 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|        - | 1228 | ` *  of copying the value.` |
|        - | 1229 | ` */` |
|   344574 | 1230 | `PH7_PRIVATE sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|        5 | 1231 | `{` |
|   344579 | 1232 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|   344579 | 1233 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|   344579 | 1234 | `	GenBlock *pForeachBlock = 0;` |
|        - | 1235 | `	ph7_foreach_info *pInfo;` |
|        - | 1236 | `	sxu32 nFalseJump;` |
|        - | 1237 | `	VmInstr *pInstr;` |
|        - | 1238 | `	sxu32 nLine;` |
|        - | 1239 | `	sxi32 rc;` |
|   344579 | 1240 | `	nLine = pGen->pIn->nLine;` |
|        - | 1241 | `	/* Jump the 'foreach' keyword */` |
|   344579 | 1242 | `	pGen->pIn++;` |
|   344579 | 1243 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1244 | `		/* Syntax error */` |
|      ! 0 | 1245 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|      ! 0 | 1246 | `		if( rc == SXERR_ABORT ){` |
|        - | 1247 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1248 | `			return SXERR_ABORT;` |
|        - | 1249 | `		}` |
|      ! 0 | 1250 | `		goto Synchronize;` |
|        - | 1251 | `	}` |
|        - | 1252 | `	/* Jump the left parenthesis '(' */` |
|   344579 | 1253 | `	pGen->pIn++;` |
|        - | 1254 | `	/* Create the loop block */` |
|   344579 | 1255 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|   344579 | 1256 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1257 | `		return SXERR_ABORT;` |
|        - | 1258 | `	}` |
|        - | 1259 | `	/* Delimit the expression */` |
|   344579 | 1260 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   344579 | 1261 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - | 1262 | `		/* Empty expression */` |
|      ! 0 | 1263 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|      ! 0 | 1264 | `		if( rc == SXERR_ABORT ){` |
|        - | 1265 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1266 | `			return SXERR_ABORT;` |
|        - | 1267 | `		}` |
|        - | 1268 | `		/* Synchronize */` |
|      ! 0 | 1269 | `		pGen->pIn = pEnd;` |
|      ! 0 | 1270 | `		if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 | 1271 | `			pGen->pIn++;` |
|      ! 0 | 1272 | `		}` |
|      ! 0 | 1273 | `		return SXRET_OK;` |
|        - | 1274 | `	}` |
|        - | 1275 | `	/* Compile the array expression */` |
|   344579 | 1276 | `	pCur = pGen->pIn;` |
|  1964899 | 1277 | `	while( pCur < pEnd ){` |
|  1964899 | 1278 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|   377761 | 1279 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|   377761 | 1280 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|        - | 1281 | `				/* Break with the first 'as' found */` |
|   344579 | 1282 | `				break;` |
|        - | 1283 | `			}` |
|    16591 | 1284 | `		}` |
|        - | 1285 | `		/* Advance the stream cursor */` |
|  1620325 | 1286 | `		pCur++;` |
|        5 | 1287 | `	}` |
|   344579 | 1288 | `	if( pCur <= pGen->pIn ){` |
|      ! 0 | 1289 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 1290 | `			"foreach: Missing array/object expression");` |
|      ! 0 | 1291 | `		if( rc == SXERR_ABORT ){` |
|        - | 1292 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1293 | `			return SXERR_ABORT;` |
|        - | 1294 | `		}` |
|      ! 0 | 1295 | `		goto Synchronize;` |
|        - | 1296 | `	}` |
|        - | 1297 | `	/* Swap token streams */` |
|   344579 | 1298 | `	pTmp = pGen->pEnd;` |
|   344579 | 1299 | `	pGen->pEnd = pCur;` |
|   344579 | 1300 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   344579 | 1301 | `	if( rc == SXERR_ABORT ){` |
|        - | 1302 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1303 | `		return SXERR_ABORT;` |
|        - | 1304 | `	}` |
|        - | 1305 | `	/* Update token stream */` |
|   344579 | 1306 | `	while(pGen->pIn < pCur ){` |
|      ! 0 | 1307 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 1308 | `		if( rc == SXERR_ABORT ){` |
|        - | 1309 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1310 | `			return SXERR_ABORT;` |
|        - | 1311 | `		}` |
|      ! 0 | 1312 | `		pGen->pIn++;` |
|      ! 0 | 1313 | `	}` |
|   344579 | 1314 | `	pCur++; /* Jump the 'as' keyword */` |
|   344579 | 1315 | `	pGen->pIn = pCur;` |
|   344579 | 1316 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 1317 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|      ! 0 | 1318 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1319 | `			return SXERR_ABORT;` |
|        - | 1320 | `		}` |
|      ! 0 | 1321 | `	}` |
|        - | 1322 | `	/* Create the foreach context */` |
|   344579 | 1323 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|   344579 | 1324 | `	if( pInfo == 0 ){` |
|      ! 0 | 1325 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|      ! 0 | 1326 | `		return SXERR_ABORT;` |
|        - | 1327 | `	}` |
|        - | 1328 | `	/* Zero the structure */` |
|   344579 | 1329 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|        - | 1330 | `	/* Initialize structure fields */` |
|   344579 | 1331 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|        - | 1332 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|        - | 1333 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|        - | 1334 | `	 * '=>'. */` |
|   344579 | 1335 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|   344579 | 1336 | `	if( pCur < pEnd ){` |
|        - | 1337 | `		/* Compile the expression holding the key name */` |
|   141113 | 1338 | `		if( pGen->pIn >= pCur ){` |
|      ! 0 | 1339 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|      ! 0 | 1340 | `			if( rc == SXERR_ABORT ){` |
|        - | 1341 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1342 | `				return SXERR_ABORT;` |
|        - | 1343 | `			}` |
|      ! 0 | 1344 | `		}else{` |
|   141113 | 1345 | `			pGen->pEnd = pCur;` |
|   141113 | 1346 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|   141113 | 1347 | `			if( rc == SXERR_ABORT ){` |
|        - | 1348 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1349 | `				return SXERR_ABORT;` |
|        - | 1350 | `			}` |
|   141113 | 1351 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|   141113 | 1352 | `			if( pInstr->p3 ){` |
|        - | 1353 | `				/* Record key name */` |
|   141113 | 1354 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    70554 | 1355 | `			}` |
|   141113 | 1356 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|        - | 1357 | `		}` |
|   141113 | 1358 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    70554 | 1359 | `	}` |
|   344579 | 1360 | `	pGen->pEnd = pEnd;` |
|   344579 | 1361 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 1362 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|      ! 0 | 1363 | `		if( rc == SXERR_ABORT ){` |
|        - | 1364 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1365 | `			return SXERR_ABORT;` |
|        - | 1366 | `		}` |
|      ! 0 | 1367 | `		goto Synchronize;` |
|        - | 1368 | `	}` |
|   344579 | 1369 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|       33 | 1370 | `		pGen->pIn++;` |
|        - | 1371 | `		/* Pass by reference  */` |
|       33 | 1372 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       15 | 1373 | `	}` |
|        - | 1374 | `	/* Check if the value target is list() */` |
|   344579 | 1375 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        8 | 1376 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|        - | 1377 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|        - | 1378 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|        - | 1379 | `		 */` |
|        - | 1380 | `		static int iForeachListCnt = 0;` |
|        - | 1381 | `		char zTmp[128];` |
|        - | 1382 | `		sxu32 nLen;` |
|        - | 1383 | `		char *zDup;` |
|       10 | 1384 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|       10 | 1385 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       10 | 1386 | `		if( zDup == 0 ){` |
|      ! 0 | 1387 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1388 | `			return SXERR_ABORT;` |
|        - | 1389 | `		}` |
|       10 | 1390 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|        - | 1391 | `		/* Save list() token boundaries */` |
|       10 | 1392 | `		pListStart = pGen->pIn;` |
|        - | 1393 | `		/* Advance past list(...) — validate parentheses */` |
|       10 | 1394 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|       10 | 1395 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1396 | `			/* php: syntax error, unexpected variable "$x", expecting "(" */` |
|        3 | 1397 | `			rc = PH7_GenSyntaxError(pGen,pGen->pIn < pEnd ? pGen->pIn : 0,"\"(\"");` |
|        3 | 1398 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1399 | `				return SXERR_ABORT;` |
|        - | 1400 | `			}` |
|        3 | 1401 | `			goto Synchronize;` |
|        - | 1402 | `		}` |
|        7 | 1403 | `		pGen->pIn++; /* Jump '(' */` |
|        7 | 1404 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|        7 | 1405 | `		if( pListEnd >= pEnd ){` |
|      ! 0 | 1406 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1407 | `				"foreach: Missing closing ')' after list");` |
|      ! 0 | 1408 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1409 | `				return SXERR_ABORT;` |
|        - | 1410 | `			}` |
|      ! 0 | 1411 | `			goto Synchronize;` |
|        - | 1412 | `		}` |
|        7 | 1413 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|        7 | 1414 | `		pListEnd = pGen->pIn;` |
|        7 | 1415 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|   344574 | 1416 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|        - | 1417 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|        - | 1418 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|        - | 1419 | `		 */` |
|        - | 1420 | `		static int iForeachShortListCnt = 0;` |
|        - | 1421 | `		char zTmp[128];` |
|        - | 1422 | `		sxu32 nLen;` |
|        - | 1423 | `		char *zDup;` |
|       22 | 1424 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       22 | 1425 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       22 | 1426 | `		if( zDup == 0 ){` |
|      ! 0 | 1427 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1428 | `			return SXERR_ABORT;` |
|        - | 1429 | `		}` |
|       22 | 1430 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|        - | 1431 | `		/* Save [...] token boundaries */` |
|       22 | 1432 | `		pListStart = pGen->pIn;` |
|        - | 1433 | `		/* Advance past [...] */` |
|       22 | 1434 | `		pGen->pIn++; /* Jump '[' */` |
|       22 | 1435 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       22 | 1436 | `		if( pListEnd >= pEnd ){` |
|      ! 0 | 1437 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1438 | `				"foreach: Missing closing ']' after short list");` |
|      ! 0 | 1439 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1440 | `				return SXERR_ABORT;` |
|        - | 1441 | `			}` |
|      ! 0 | 1442 | `			goto Synchronize;` |
|        - | 1443 | `		}` |
|       22 | 1444 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       22 | 1445 | `		pListEnd = pGen->pIn;` |
|       22 | 1446 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|       12 | 1447 | `	}else{` |
|        - | 1448 | `		/* Compile the expression holding the value name */` |
|   344551 | 1449 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|   344551 | 1450 | `		if( rc == SXERR_ABORT ){` |
|        - | 1451 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1452 | `			return SXERR_ABORT;` |
|        - | 1453 | `		}` |
|   344551 | 1454 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|   344551 | 1455 | `		if( pInstr->p3 ){` |
|        - | 1456 | `			/* Record value name */` |
|   344551 | 1457 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|   172273 | 1458 | `		}` |
|        - | 1459 | `	}` |
|        - | 1460 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|   344577 | 1461 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|        - | 1462 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   344577 | 1463 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|        - | 1464 | `	/* Record the first instruction to execute */` |
|   344577 | 1465 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|        - | 1466 | `	/* Emit the FOREACH_STEP instruction */` |
|   344577 | 1467 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|        - | 1468 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   344577 | 1469 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|        - | 1470 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|   344577 | 1471 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|        - | 1472 | `		SyToken *pSavedIn,*pSavedEnd;` |
|        - | 1473 | `		/* Load the temporary variable holding the current value onto the stack.` |
|        - | 1474 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|        - | 1475 | `		 */` |
|       28 | 1476 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|        - | 1477 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|        - | 1478 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|        - | 1479 | `		 * picks up the delimiter and the variable names inside.` |
|        - | 1480 | `		 */` |
|       28 | 1481 | `		pSavedIn = pGen->pIn;` |
|       28 | 1482 | `		pSavedEnd = pGen->pEnd;` |
|       28 | 1483 | `		pGen->pIn = pListStart;` |
|       28 | 1484 | `		pGen->pEnd = pListEnd;` |
|       28 | 1485 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       22 | 1486 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|       12 | 1487 | `		}else{` |
|        7 | 1488 | `			rc = PH7_CompileList(&(*pGen),0);` |
|        - | 1489 | `		}` |
|       28 | 1490 | `		pGen->pIn = pSavedIn;` |
|       28 | 1491 | `		pGen->pEnd = pSavedEnd;` |
|       28 | 1492 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1493 | `			return SXERR_ABORT;` |
|        - | 1494 | `		}` |
|        - | 1495 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|       28 | 1496 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       13 | 1497 | `	}` |
|        - | 1498 | `	/* Compile the loop body */` |
|   344577 | 1499 | `	pGen->pIn = &pEnd[1];` |
|   344577 | 1500 | `	pGen->pEnd = pTmp;` |
|   344577 | 1501 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|   344577 | 1502 | `	if( rc == SXERR_ABORT ){` |
|        - | 1503 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1504 | `		return SXERR_ABORT;` |
|        - | 1505 | `	}` |
|        - | 1506 | `	/* Emit the unconditional jump to the start of the loop */` |
|   344577 | 1507 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|        - | 1508 | `	/* Fix all jumps now the destination is resolved */` |
|   344577 | 1509 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 1510 | `	/* Release the loop block */` |
|   344577 | 1511 | `	GenStateLeaveBlock(pGen,0);` |
|        - | 1512 | `	/* Statement successfully compiled */` |
|   344577 | 1513 | `	return SXRET_OK;` |
|        1 | 1514 | `Synchronize:` |
|        - | 1515 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - | 1516 | `	 * compiling this erroneous block.` |
|        - | 1517 | `	 */` |
|        3 | 1518 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 | 1519 | `		pGen->pIn++;` |
|      ! 0 | 1520 | `	}` |
|        3 | 1521 | `	return SXRET_OK;` |
|   172292 | 1522 | `}` |
|        - | 1523 | `/*` |
|        - | 1524 | ` * Compile the infamous if/elseif/else if/else statements.` |
|        - | 1525 | ` * According to the PHP language reference` |
|        - | 1526 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|        - | 1527 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|        - | 1528 | ` *  that is similar to that of C:` |
|        - | 1529 | ` *  if (expr)` |
|        - | 1530 | ` *   statement` |
|        - | 1531 | ` *  else construct:` |
|        - | 1532 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|        - | 1533 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|        - | 1534 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|        - | 1535 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|        - | 1536 | ` *   $b, and a is NOT greater than b otherwise.` |
|        - | 1537 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|        - | 1538 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|        - | 1539 | ` *  elseif` |
|        - | 1540 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|        - | 1541 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|        - | 1542 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|        - | 1543 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|        - | 1544 | ` *   than b, a equal to b or a is smaller than b:` |
|        - | 1545 | ` *   <?php` |
|        - | 1546 | ` *    if ($a > $b) {` |
|        - | 1547 | ` *     echo "a is bigger than b";` |
|        - | 1548 | ` *    } elseif ($a == $b) {` |
|        - | 1549 | ` *     echo "a is equal to b";` |
|        - | 1550 | ` *    } else {` |
|        - | 1551 | ` *     echo "a is smaller than b";` |
|        - | 1552 | ` *    }` |
|        - | 1553 | ` *    ?>` |
|        - | 1554 | ` */` |
|  2605648 | 1555 | `PH7_PRIVATE sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|        5 | 1556 | `{` |
|  2605653 | 1557 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  2605653 | 1558 | `	GenBlock *pCondBlock = 0;` |
|        - | 1559 | `	sxu32 nJumpIdx;` |
|        - | 1560 | `	sxu32 nKeyID;` |
|        - | 1561 | `	sxi32 rc;` |
|        - | 1562 | `	/* Jump the 'if' keyword */` |
|  2605653 | 1563 | `	pGen->pIn++;` |
|  2605653 | 1564 | `	pToken = pGen->pIn;` |
|        - | 1565 | `	/* Create the conditional block */` |
|  2605653 | 1566 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  2605653 | 1567 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1568 | `		return SXERR_ABORT;` |
|        - | 1569 | `	}` |
|        - | 1570 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|  1454010 | 1571 | `	for(;;){` |
|  2908025 | 1572 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1573 | `			/* Syntax error */` |
|      ! 0 | 1574 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 1575 | `				pToken--;` |
|      ! 0 | 1576 | `			}` |
|      ! 0 | 1577 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|      ! 0 | 1578 | `			if( rc == SXERR_ABORT ){` |
|        - | 1579 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 1580 | `				return SXERR_ABORT;` |
|        - | 1581 | `			}` |
|      ! 0 | 1582 | `			goto Synchronize;` |
|        - | 1583 | `		}` |
|        - | 1584 | `		/* Jump the left parenthesis '(' */` |
|  2908025 | 1585 | `		pToken++;` |
|        - | 1586 | `		/* Delimit the condition */` |
|  2908025 | 1587 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  2908025 | 1588 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|        - | 1589 | `			/* Syntax error */` |
|      ! 0 | 1590 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 1591 | `				pToken--;` |
|      ! 0 | 1592 | `			}` |
|      ! 0 | 1593 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|      ! 0 | 1594 | `			if( rc == SXERR_ABORT ){` |
|        - | 1595 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 1596 | `				return SXERR_ABORT;` |
|        - | 1597 | `			}` |
|      ! 0 | 1598 | `			goto Synchronize;` |
|        - | 1599 | `		}` |
|        - | 1600 | `		/* Swap token streams */` |
|  2908025 | 1601 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|        - | 1602 | `		/* Compile the condition */` |
|  2908025 | 1603 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 1604 | `		/* Update token stream */` |
|  2908025 | 1605 | `		while(pGen->pIn < pEnd ){` |
|      ! 0 | 1606 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 | 1607 | `			pGen->pIn++;` |
|      ! 0 | 1608 | `		}` |
|  2908025 | 1609 | `		pGen->pIn  = &pEnd[1];` |
|  2908025 | 1610 | `		pGen->pEnd = pTmp;` |
|  2908025 | 1611 | `		if( rc == SXERR_ABORT ){` |
|        - | 1612 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|        3 | 1613 | `			return SXERR_ABORT;` |
|        - | 1614 | `		}` |
|        - | 1615 | `		/* Emit the false jump */` |
|  2908023 | 1616 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|        - | 1617 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  2908023 | 1618 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|        - | 1619 | `		/* Compile the body */` |
|  2908023 | 1620 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  2908023 | 1621 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1622 | `			return SXERR_ABORT;` |
|        - | 1623 | `		}` |
|  2908023 | 1624 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   559326 | 1625 | `			break;` |
|        - | 1626 | `		}` |
|        - | 1627 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|  1789381 | 1628 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  1789381 | 1629 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|  1242325 | 1630 | `			break;` |
|        - | 1631 | `		}` |
|        - | 1632 | `		/* Emit the unconditional jump */` |
|   547061 | 1633 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|        - | 1634 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   547061 | 1635 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   547061 | 1636 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   331629 | 1637 | `			pToken = &pGen->pIn[1];` |
|   331629 | 1638 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    86978 | 1639 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|   122347 | 1640 | `					break;` |
|        - | 1641 | `			}` |
|    86945 | 1642 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    43470 | 1643 | `		}` |
|   302377 | 1644 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|        - | 1645 | `		/* Synchronize cursors */` |
|   302377 | 1646 | `		pToken = pGen->pIn;` |
|        - | 1647 | `		/* Fix the false jump */` |
|   302377 | 1648 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|        5 | 1649 | `	} /* For(;;) */` |
|        - | 1650 | `	/* Fix the false jump */` |
|  2605651 | 1651 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  2605651 | 1652 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|  1487004 | 1653 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|        - | 1654 | `			/* Compile the else block */` |
|   244689 | 1655 | `			pGen->pIn++;` |
|   244689 | 1656 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   244689 | 1657 | `			if( rc == SXERR_ABORT ){` |
|        - | 1658 |  |
|      ! 0 | 1659 | `				return SXERR_ABORT;` |
|        - | 1660 | `			}` |
|   122342 | 1661 | `	}` |
|  2605651 | 1662 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|        - | 1663 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  2605651 | 1664 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|        - | 1665 | `	/* Release the conditional block */` |
|  2605651 | 1666 | `	GenStateLeaveBlock(pGen,0);` |
|        - | 1667 | `	/* Statement successfully compiled */` |
|  2605651 | 1668 | `	return SXRET_OK;` |
|      ! 0 | 1669 | `Synchronize:` |
|        - | 1670 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|        - | 1671 | `	 */` |
|      ! 0 | 1672 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 | 1673 | `		pGen->pIn++;` |
|      ! 0 | 1674 | `	}` |
|      ! 0 | 1675 | `	return SXRET_OK;` |
|  1302829 | 1676 | `}` |
|        - | 1677 | `/*` |
|        - | 1678 | ` * Compile the global construct.` |
|        - | 1679 | ` * According to the PHP language reference` |
|        - | 1680 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|        - | 1681 | ` *  to be used in that function.` |
|        - | 1682 | ` *  Example #1 Using global` |
|        - | 1683 | ` *  <?php` |
|        - | 1684 | ` *   $a = 1;` |
|        - | 1685 | ` *   $b = 2;` |
|        - | 1686 | ` *   function Sum()` |
|        - | 1687 | ` *   {` |
|        - | 1688 | ` *    global $a, $b;` |
|        - | 1689 | ` *    $b = $a + $b;` |
|        - | 1690 | ` *   }` |
|        - | 1691 | ` *   Sum();` |
|        - | 1692 | ` *   echo $b;` |
|        - | 1693 | ` *  ?>` |
|        - | 1694 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|        - | 1695 | ` *  all references to either variable will refer to the global version. There is no limit` |
|        - | 1696 | ` *  to the number of global variables that can be manipulated by a function.` |
|        - | 1697 | ` */` |
|       40 | 1698 | `PH7_PRIVATE sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|        5 | 1699 | `{` |
|       45 | 1700 | `	SyToken *pTmp,*pNext = 0;` |
|        - | 1701 | `	sxi32 nExpr;` |
|        - | 1702 | `	sxi32 rc;` |
|        - | 1703 | `	/* Jump the 'global' keyword */` |
|       45 | 1704 | `	pGen->pIn++;` |
|       45 | 1705 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|        - | 1706 | `		/* Nothing to process */` |
|      ! 0 | 1707 | `		return SXRET_OK;` |
|        - | 1708 | `	}` |
|       45 | 1709 | `	pTmp = pGen->pEnd;` |
|       45 | 1710 | `	nExpr = 0;` |
|       95 | 1711 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       55 | 1712 | `		if( pGen->pIn < pNext ){` |
|       55 | 1713 | `			pGen->pEnd = pNext;` |
|       55 | 1714 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 | 1715 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|      ! 0 | 1716 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1717 | `					return SXERR_ABORT;` |
|        - | 1718 | `				}` |
|      ! 0 | 1719 | `			}else{` |
|       55 | 1720 | `				pGen->pIn++;` |
|       55 | 1721 | `				if( pGen->pIn >= pGen->pEnd ){` |
|        - | 1722 | `					/* Emit a warning */` |
|      ! 0 | 1723 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|      ! 0 | 1724 | `				}else{` |
|       55 | 1725 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       55 | 1726 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 1727 | `						return SXERR_ABORT;` |
|       55 | 1728 | `					}else if(rc != SXERR_EMPTY ){` |
|       55 | 1729 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|       55 | 1730 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|        - | 1731 | `							/* Variable name, not a constant */` |
|       55 | 1732 | `							pLast->iP1 = 0;` |
|       25 | 1733 | `						}` |
|       55 | 1734 | `						nExpr++;` |
|       25 | 1735 | `					}` |
|        - | 1736 | `				}` |
|        - | 1737 | `			}` |
|       25 | 1738 | `		}` |
|        - | 1739 | `		/* Next expression in the stream */` |
|       55 | 1740 | `		pGen->pIn = pNext;` |
|        - | 1741 | `		/* Jump trailing commas */` |
|       65 | 1742 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       15 | 1743 | `			pGen->pIn++;` |
|        5 | 1744 | `		}` |
|        5 | 1745 | `	}` |
|        - | 1746 | `	/* Restore token stream */` |
|       45 | 1747 | `	pGen->pEnd = pTmp;` |
|       45 | 1748 | `	if( nExpr > 0 ){` |
|        - | 1749 | `		/* Emit the uplink instruction */` |
|       45 | 1750 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|       20 | 1751 | `	}` |
|       45 | 1752 | `	return SXRET_OK;` |
|       25 | 1753 | `}` |
|        - | 1754 | `/*` |
|        - | 1755 | ` * Compile the return statement.` |
|        - | 1756 | ` * According to the PHP language reference` |
|        - | 1757 | ` *  If called from within a function, the return() statement immediately ends execution` |
|        - | 1758 | ` *  of the current function, and returns its argument as the value of the function call.` |
|        - | 1759 | ` *  return() will also end the execution of an eval() statement or script file.` |
|        - | 1760 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|        - | 1761 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|        - | 1762 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|        - | 1763 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|        - | 1764 | ` *  from within the main script file, then script execution end.` |
|        - | 1765 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|        - | 1766 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|        - | 1767 | ` *  should do so as PHP has less work to do in this case.` |
|        - | 1768 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|        - | 1769 | ` */` |
|  3869592 | 1770 | `PH7_PRIVATE sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|        5 | 1771 | `{` |
|  3869597 | 1772 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|        - | 1773 | `	sxi32 rc;` |
|  3869597 | 1774 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  3869597 | 1775 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|        - | 1776 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|        - | 1777 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|        - | 1778 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|        - | 1779 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|        - | 1780 | `	 * normally below so token processing stays consistent. */` |
| 10199243 | 1781 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|  6329651 | 1782 | `		pFuncBlock = pFuncBlock->pParent;` |
|        5 | 1783 | `	}` |
|  3869592 | 1784 | `	if( pFuncBlock && pFuncBlock->pUserData` |
|  3869581 | 1785 | `	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){` |
|        3 | 1786 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1787 | `			"A never-returning function must not return");` |
|        3 | 1788 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1789 | `			return SXERR_ABORT;` |
|        - | 1790 | `		}` |
|        1 | 1791 | `	}` |
|        - | 1792 | `	/* Jump the 'return' keyword */` |
|  3869597 | 1793 | `	pGen->pIn++;` |
|  3869597 | 1794 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - | 1795 | ``		/* php: a stray token after `return EXPR` is `... expecting ";"`. */`` |
|  3761937 | 1796 | `		const char *zSave = pGen->zClauseCloser;` |
|  3761937 | 1797 | `		pGen->zClauseCloser = "\";\"";` |
|        - | 1798 | ``		/* A `return` READS its operand (the value is consumed), so compile it`` |
|        - | 1799 | ``		 * read-only: a lone undefined variable `return $z` must warn at the read`` |
|        - | 1800 | `		 * exactly like echo/interpolation, not be loaded quietly (the same quiet` |
|        - | 1801 | ``		 * load that correctly keeps a bare `$z;` statement silent). Matches the`` |
|        - | 1802 | `		 * arrow-fn implicit-return body fix. */` |
|  3761937 | 1803 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|  3761937 | 1804 | `		pGen->zClauseCloser = zSave;` |
|  3761937 | 1805 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1806 | `			return SXERR_ABORT;` |
|  3761937 | 1807 | `		}else if(rc != SXERR_EMPTY ){` |
|  3761937 | 1808 | `			nRet = 1;` |
|  1880966 | 1809 | `		}` |
|  1880966 | 1810 | `	}` |
|        - | 1811 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|        - | 1812 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|        - | 1813 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|        - | 1814 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|  3869597 | 1815 | `	if( pGen->bInGenerator ){` |
|     4181 | 1816 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|     4181 | 1817 | `		return SXRET_OK;` |
|        - | 1818 | `	}` |
|        - | 1819 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|        - | 1820 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|        - | 1821 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|        - | 1822 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|        - | 1823 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|  3865421 | 1824 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|  3865421 | 1825 | `	return SXRET_OK;` |
|  1934801 | 1826 | `}` |
|        - | 1827 | `/*` |
|        - | 1828 | ` * Compile a yield expression.` |
|        - | 1829 | ` * Called from the expression code generator when a yield node is encountered.` |
|        - | 1830 | ` * Handles: yield, yield $value, yield $key => $value` |
|        - | 1831 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|        - | 1832 | ` */` |
|    16980 | 1833 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|        5 | 1834 | `{` |
|        - | 1835 | `	SyToken *pTmp, *pSplit;` |
|    16985 | 1836 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|    16985 | 1837 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|        - | 1838 | `	sxi32 rc;` |
|     8490 | 1839 | `	(void)iCompileFlag;` |
|        - | 1840 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|    16985 | 1841 | `	pGen->pIn++;` |
|        - | 1842 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|        - | 1843 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|        - | 1844 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|        - | 1845 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|        - | 1846 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|    16980 | 1847 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|     8527 | 1848 | `		&& pGen->pIn->sData.nByte == 4` |
|       75 | 1849 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|       69 | 1850 | `		pGen->pIn++; /* Skip 'from' */` |
|       69 | 1851 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       69 | 1852 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1853 | `			return SXERR_ABORT;` |
|        - | 1854 | `		}` |
|       69 | 1855 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 | 1856 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|      ! 0 | 1857 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|        - | 1858 | `				"Missing expression after 'yield from'");` |
|      ! 0 | 1859 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1860 | `				return SXERR_ABORT;` |
|        - | 1861 | `			}` |
|      ! 0 | 1862 | `		}` |
|       69 | 1863 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|       69 | 1864 | `		return SXRET_OK;` |
|        - | 1865 | `	}` |
|    16921 | 1866 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - | 1867 | `		/* Bare yield — no value */` |
|        3 | 1868 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|        3 | 1869 | `		return SXRET_OK;` |
|        - | 1870 | `	}` |
|        - | 1871 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|    16919 | 1872 | `	pSplit = 0;` |
|        - | 1873 | `	{` |
|    16919 | 1874 | `		SyToken *pCur = pGen->pIn;` |
|    16919 | 1875 | `		sxi32 nNest = 0;` |
|    50547 | 1876 | `		while( pCur < pGen->pEnd ){` |
|    50207 | 1877 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       20 | 1878 | `				nNest++;` |
|    50198 | 1879 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       20 | 1880 | `				nNest--;` |
|    50180 | 1881 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|    16579 | 1882 | `				pSplit = pCur;` |
|    16579 | 1883 | `				break;` |
|        - | 1884 | `			}` |
|    33633 | 1885 | `			pCur++;` |
|        5 | 1886 | `		}` |
|        - | 1887 | `	}` |
|    16919 | 1888 | `	pTmp = pGen->pEnd;` |
|    16919 | 1889 | `	if( pSplit ){` |
|        - | 1890 | `		/* yield $key => $value */` |
|    16579 | 1891 | `		pGen->pEnd = pSplit;` |
|    16579 | 1892 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|    16579 | 1893 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|    16579 | 1894 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|    16579 | 1895 | `		pGen->pEnd = pTmp;` |
|    16579 | 1896 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|    16579 | 1897 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|    16579 | 1898 | `		iP1 = 1;` |
|    16579 | 1899 | `		iP2 = 1;` |
|     8292 | 1900 | `	}else{` |
|        - | 1901 | `		/* yield $value */` |
|      345 | 1902 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      345 | 1903 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      345 | 1904 | `		if( rc != SXERR_EMPTY ){` |
|      345 | 1905 | `			iP1 = 1;` |
|      170 | 1906 | `		}` |
|        - | 1907 | `	}` |
|    16919 | 1908 | `	pGen->pEnd = pTmp;` |
|    16919 | 1909 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|    16919 | 1910 | `	return SXRET_OK;` |
|     8495 | 1911 | `}` |
|        - | 1912 | `/*` |
|        - | 1913 | ` * Compile the die/exit language construct.` |
|        - | 1914 | ` * The role of these constructs is to terminate execution of the script.` |
|        - | 1915 | ` * Shutdown functions will always be executed even if exit() is called.` |
|        - | 1916 | ` */` |
|       94 | 1917 | `PH7_PRIVATE sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|        5 | 1918 | `{` |
|       99 | 1919 | `	sxi32 nExpr = 0;` |
|        - | 1920 | `	sxi32 rc;` |
|        - | 1921 | `	/* Jump the die/exit keyword */` |
|       99 | 1922 | `	pGen->pIn++;` |
|       99 | 1923 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - | 1924 | `		/* Compile the expression */` |
|       99 | 1925 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       99 | 1926 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1927 | `			return SXERR_ABORT;` |
|       99 | 1928 | `		}else if(rc != SXERR_EMPTY ){` |
|       99 | 1929 | `			nExpr = 1;` |
|       47 | 1930 | `		}` |
|       47 | 1931 | `	}` |
|        - | 1932 | `	/* Emit the HALT instruction */` |
|       99 | 1933 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|       99 | 1934 | `	return SXRET_OK;` |
|       52 | 1935 | `}` |
|        - | 1936 | `/*` |
|        - | 1937 | ` * Compile the 'echo' language construct.` |
|        - | 1938 | ` */` |
|    18800 | 1939 | `PH7_PRIVATE sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|        5 | 1940 | `{` |
|    18805 | 1941 | `	SyToken *pTmp,*pNext = 0;` |
|    18805 | 1942 | `	sxu32 nLine = pGen->pIn->nLine;` |
|    18805 | 1943 | `	int nExpr = 0;      /* expressions actually compiled */` |
|    18805 | 1944 | `	int bExpectMore = 1;/* after 'echo' or a comma an expression is REQUIRED */` |
|        - | 1945 | `	sxi32 rc;` |
|        - | 1946 | `	/* Jump the 'echo' keyword */` |
|    18805 | 1947 | `	pGen->pIn++;` |
|        - | 1948 | `	/* Compile arguments one after one. php: a stray token in an echo list is` |
|        - | 1949 | ``	 * `... expecting "," or ";"` — set the closer for each argument's compile. */`` |
|    18805 | 1950 | `	pTmp = pGen->pEnd;` |
|        - | 1951 | `	{` |
|    18805 | 1952 | `	const char *zSaveEcho = pGen->zClauseCloser;` |
|    49649 | 1953 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|    30855 | 1954 | `		if( pGen->pIn < pNext ){` |
|    30855 | 1955 | `			pGen->pEnd = pNext;` |
|    30855 | 1956 | `			pGen->zClauseCloser = "\",\" or \";\"";` |
|    30855 | 1957 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|    30855 | 1958 | `			pGen->zClauseCloser = zSaveEcho;` |
|    30855 | 1959 | `			if( rc == SXERR_ABORT ){` |
|        5 | 1960 | `				return SXERR_ABORT;` |
|    30851 | 1961 | `			}else if( rc != SXERR_EMPTY ){` |
|        - | 1962 | `				/* Emit the consume instruction */` |
|    30827 | 1963 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    30827 | 1964 | `				nExpr++;` |
|    30827 | 1965 | `				bExpectMore = 0;` |
|    15411 | 1966 | `			}` |
|    15423 | 1967 | `		}` |
|        - | 1968 | `		/* Jump trailing commas (php: exactly one between expressions; a` |
|        - | 1969 | `		 * dangling or doubled comma is a parse error, enforced below) */` |
|    42907 | 1970 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|    12063 | 1971 | `			if( bExpectMore ){` |
|        - | 1972 | `				/* two commas in a row */` |
|        3 | 1973 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,pNext->nLine,` |
|        - | 1974 | `					"syntax error, unexpected token \",\"");` |
|        3 | 1975 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1976 | `			}` |
|    12061 | 1977 | `			bExpectMore = 1;` |
|    12061 | 1978 | `			pNext++;` |
|        5 | 1979 | `		}` |
|    30849 | 1980 | `		pGen->pIn = pNext;` |
|        5 | 1981 | `	}` |
|        - | 1982 | `	}` |
|        - | 1983 | `	/* Restore token stream */` |
|    18799 | 1984 | `	pGen->pEnd = pTmp;` |
|    18799 | 1985 | `	if( nExpr == 0 \|\| bExpectMore ){` |
|        - | 1986 | ``		/* `echo ;` or `echo expr, ;` — php rejects both */`` |
|       32 | 1987 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - | 1988 | `			"syntax error, unexpected token \";\"");` |
|       32 | 1989 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1990 | `	}` |
|    18771 | 1991 | `	return SXRET_OK;` |
|     9405 | 1992 | `}` |
|        - | 1993 | `/*` |
|        - | 1994 | ` * Compile the static statement.` |
|        - | 1995 | ` * According to the PHP language reference` |
|        - | 1996 | ` *  Another important feature of variable scoping is the static variable.` |
|        - | 1997 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|        - | 1998 | ` *  when program execution leaves this scope.` |
|        - | 1999 | ` *  Static variables also provide one way to deal with recursive functions.` |
|        - | 2000 | ` * Symisc eXtension.` |
|        - | 2001 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|        - | 2002 | ` *  the zend engine would allow only simple scalar value.` |
|        - | 2003 | ` *  Example` |
|        - | 2004 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|        - | 2005 | ` *    Refer to the official documentation for more information on this feature.` |
|        - | 2006 | ` */` |
|    12434 | 2007 | `PH7_PRIVATE sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|        5 | 2008 | `{` |
|        - | 2009 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|        - | 2010 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|        - | 2011 | `	GenBlock *pBlock;` |
|        - | 2012 | `	SyString *pName;` |
|        - | 2013 | `	char *zDup;` |
|        - | 2014 | `	sxu32 nLine;` |
|        - | 2015 | `	sxi32 rc;` |
|        - | 2016 | ``	/* `static function () {}` / `static fn () =>` at statement position is an`` |
|        - | 2017 | `	 * EXPRESSION statement (a bare static closure), not a static-variable` |
|        - | 2018 | `	 * declaration — hand it to the expression compiler (php accepts it). */` |
|    12434 | 2019 | `	if( &pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & PH7_TK_KEYWORD)` |
|     6223 | 2020 | `	 && (SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FUNCTION` |
|        1 | 2021 | `	  \|\| SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FN) ){` |
|        3 | 2022 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        3 | 2023 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2024 | `			return SXERR_ABORT;` |
|        3 | 2025 | `		}else if( rc != SXERR_EMPTY ){` |
|        3 | 2026 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        1 | 2027 | `		}` |
|        3 | 2028 | `		return SXRET_OK;` |
|        - | 2029 | `	}` |
|        - | 2030 | `	/* Jump the static keyword */` |
|    12437 | 2031 | `	nLine = pGen->pIn->nLine;` |
|    12437 | 2032 | `	pGen->pIn++;` |
|        - | 2033 | `	/* Extract the enclosing function if any */` |
|    12437 | 2034 | `	pBlock = pGen->pCurrent;` |
|    24869 | 2035 | `	while( pBlock ){` |
|    24869 | 2036 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|    12437 | 2037 | `			break;` |
|        - | 2038 | `		}` |
|        - | 2039 | `		/* Point to the upper block */` |
|    12437 | 2040 | `		pBlock = pBlock->pParent;` |
|        5 | 2041 | `	}` |
|    12437 | 2042 | `	if( pBlock == 0 ){` |
|        - | 2043 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|      ! 0 | 2044 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|        - | 2045 | ``			/* php: `static FOO;` is a syntax error naming FOO and expecting "::"`` |
|        - | 2046 | ``			 * (the parser is still open to `static::` at that point). */`` |
|      ! 0 | 2047 | `			rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"::\"");` |
|      ! 0 | 2048 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2049 | `				return SXERR_ABORT;` |
|        - | 2050 | `			}` |
|      ! 0 | 2051 | `			goto Synchronize;` |
|        - | 2052 | `		}` |
|        - | 2053 | `		/* Compile the expression holding the variable */` |
|      ! 0 | 2054 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      ! 0 | 2055 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2056 | `			return SXERR_ABORT;` |
|      ! 0 | 2057 | `		}else if( rc != SXERR_EMPTY ){` |
|        - | 2058 | `			/* Emit the POP instruction */` |
|      ! 0 | 2059 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      ! 0 | 2060 | `		}` |
|      ! 0 | 2061 | `		return SXRET_OK;` |
|        - | 2062 | `	}` |
|    12437 | 2063 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        - | 2064 | `	/* Make sure we are dealing with a valid statement */` |
|    12437 | 2065 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|    12430 | 2066 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - | 2067 | ``			/* php: `static FOO;` is a syntax error naming FOO and expecting "::"`` |
|        - | 2068 | ``			 * (the parser is still open to `static::` at that point). */`` |
|        3 | 2069 | `			rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"::\"");` |
|        3 | 2070 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2071 | `				return SXERR_ABORT;` |
|        - | 2072 | `			}` |
|        3 | 2073 | `			goto Synchronize;` |
|        - | 2074 | `	}` |
|    12435 | 2075 | `	pGen->pIn++;` |
|        - | 2076 | `	/* Extract variable name */` |
|    12435 | 2077 | `	pName = &pGen->pIn->sData;` |
|    12435 | 2078 | `	pGen->pIn++; /* Jump the var name */` |
|    12435 | 2079 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|      ! 0 | 2080 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 2081 | `		goto Synchronize;` |
|        - | 2082 | `	}` |
|        - | 2083 | `	/* Initialize the structure describing the static variable */` |
|    12435 | 2084 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|    12435 | 2085 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|        - | 2086 | `	/* Duplicate variable name */` |
|    12435 | 2087 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    12435 | 2088 | `	if( zDup == 0 ){` |
|      ! 0 | 2089 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 2090 | `		return SXERR_ABORT;` |
|        - | 2091 | `	}` |
|    12435 | 2092 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|        - | 2093 | `	/* Check if we have an expression to compile */` |
|    12435 | 2094 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|        - | 2095 | `		SySet *pInstrContainer;` |
|        - | 2096 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|        - | 2097 | `		 * Static variable can take any complex expression including function` |
|        - | 2098 | `		 * call as their initialization value.` |
|        - | 2099 | `		 * Example:` |
|        - | 2100 | `		 *		static $var = foo(1,4+5,bar());` |
|        - | 2101 | `		 */` |
|    12435 | 2102 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|        - | 2103 | `		/* Swap bytecode container */` |
|    12435 | 2104 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    12435 | 2105 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|        - | 2106 | `		/* Compile the expression */` |
|    12435 | 2107 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 2108 | `		/* Emit the done instruction */` |
|    12435 | 2109 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        - | 2110 | `		/* Restore default bytecode container */` |
|    12435 | 2111 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     6215 | 2112 | `	}` |
|        - | 2113 | `	/* Finally save the compiled static variable in the appropriate container */` |
|    12435 | 2114 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|    12435 | 2115 | `	return SXRET_OK;` |
|        1 | 2116 | `Synchronize:` |
|        - | 2117 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|        - | 2118 | `	 * statement.` |
|        - | 2119 | `	 */` |
|        5 | 2120 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|        3 | 2121 | `		pGen->pIn++;` |
|        1 | 2122 | `	}` |
|        3 | 2123 | `	return SXRET_OK;` |
|     6222 | 2124 | `}` |
|        - | 2125 | `/*` |
|        - | 2126 | ` * Compile the var statement.` |
|        - | 2127 | ` * Symisc Extension:` |
|        - | 2128 | ` *      var statement can be used outside of a class definition.` |
|        - | 2129 | ` */` |
|        2 | 2130 | `PH7_PRIVATE sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|        1 | 2131 | `{` |
|        - | 2132 | `` 	/* `var` is a PROPERTY modifier and nothing else. A class body's `var $x;` `` |
|        - | 2133 | `	 * is compiled by compile_class.c, never here, so reaching this statement` |
|        - | 2134 | ``	 * handler means `var` appeared outside a class — which php rejects as a`` |
|        - | 2135 | `	 * parse error. PHL used to compile it as an ordinary expression statement,` |
|        - | 2136 | ``	 * so top-level `var $x = 1;` silently ran (a Symisc extension). */`` |
|        3 | 2137 | `	PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|        3 | 2138 | `	return SXERR_ABORT;` |
|        1 | 2139 | `}` |
|        - | 2140 | `/*` |
|        - | 2141 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|        - | 2142 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|        - | 2143 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|        - | 2144 | ` */` |
|        - | 2145 | `/*` |
|        - | 2146 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|        - | 2147 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|        - | 2148 | ` * hash and any shared references), this creates a new literal entry with the` |
|        - | 2149 | ` * qualified name and updates the instruction's operand index.` |
|        - | 2150 | ` *` |
|        - | 2151 | ` * Resolution order:` |
|        - | 2152 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|        - | 2153 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|        - | 2154 | ` *   3. Otherwise return the original literal index unchanged.` |
|        - | 2155 | ` *` |
|        - | 2156 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|        - | 2157 | ` * came from an import (step 1) and 0 otherwise.` |
|        - | 2158 | ` * Returns the (possibly new) literal index.` |
|        - | 2159 | ` */` |
|  6918960 | 2160 | `PH7_PRIVATE sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|        5 | 2161 | `{` |
|        - | 2162 | `	ph7_value *pLit;` |
|        - | 2163 | `	const char *zLit;` |
|        - | 2164 | `	SyString sQualified;` |
|        - | 2165 | `	sxu32 nLit;` |
|        - | 2166 | `	sxu32 k;` |
|        - | 2167 | `	sxu32 nNewIdx;` |
|        - | 2168 | `	int hasNsSep;` |
|        - | 2169 | `	SyHashEntry *pImport;` |
|        - | 2170 | `	ph7_value *pNew;` |
|  6918965 | 2171 | `	if( pFromImport ){` |
|  5647339 | 2172 | `		*pFromImport = 0;` |
|  2823667 | 2173 | `	}` |
|  6918965 | 2174 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  6918965 | 2175 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|      ! 0 | 2176 | `		return nOrigIdx;` |
|        - | 2177 | `	}` |
|  6918965 | 2178 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  6918965 | 2179 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|        - | 2180 | `	/* Skip if already qualified (contains backslash) */` |
|  6918965 | 2181 | `	hasNsSep = 0;` |
| 83704823 | 2182 | `	for( k = 0; k < nLit; k++ ){` |
| 76785889 | 2183 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 38392934 | 2184 | `	}` |
|  6918965 | 2185 | `	if( hasNsSep ){` |
|       28 | 2186 | `		return nOrigIdx;` |
|        - | 2187 | `	}` |
|        - | 2188 | `	/* Check use imports first (works even outside namespaces) */` |
|  6918939 | 2189 | `	SyBlobReset(&pGen->sWorker);` |
|  6918939 | 2190 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  6918939 | 2191 | `	if( pImport ){` |
|       41 | 2192 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       41 | 2193 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|       41 | 2194 | `		if( pFromImport ){` |
|       18 | 2195 | `			*pFromImport = 1;` |
|        8 | 2196 | `		}` |
|       23 | 2197 | `	}else{` |
|  6918903 | 2198 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  6918763 | 2199 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|        - | 2200 | `		}` |
|        - | 2201 | `		/* Prepend current namespace */` |
|      145 | 2202 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      145 | 2203 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      145 | 2204 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|        - | 2205 | `	}` |
|        - | 2206 | `	/* Look up or create a new literal for the qualified name */` |
|      181 | 2207 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|      181 | 2208 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|       79 | 2209 | `		return nNewIdx; /* Already interned */` |
|        - | 2210 | `	}` |
|      107 | 2211 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      107 | 2212 | `	if( pNew == 0 ){` |
|      ! 0 | 2213 | `		return nOrigIdx; /* OOM, fall back to original */` |
|        - | 2214 | `	}` |
|      107 | 2215 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      107 | 2216 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      107 | 2217 | `	return nNewIdx;` |
|  3459485 | 2218 | `}` |
|        - | 2219 | `/*` |
|        - | 2220 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|        - | 2221 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|        - | 2222 | ` */` |
|  1108504 | 2223 | `PH7_PRIVATE void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|        5 | 2224 | `{` |
|        - | 2225 | `	SyHashEntry *pImport;` |
|  1108509 | 2226 | `	const char *zName = pName->zString;` |
|  1108509 | 2227 | `	sxu32 nName = pName->nByte;` |
|  1108509 | 2228 | `	sxu32 nFirst = 0;` |
|        - | 2229 | `	/* php resolves a name through use-imports on its LEADING segment: an` |
|        - | 2230 | ``	 * unqualified `C` maps the whole name (`use X\C;` -> X\C), while a QUALIFIED`` |
|        - | 2231 | ``	 * `A\B\C` maps just `A` (`use X\A;` -> X\A\B\C) and keeps the `\B\C` tail.`` |
|        - | 2232 | `	 * The old code looked up the whole qualified string (which never matches a` |
|        - | 2233 | `	 * single-segment import alias) and then blindly prefixed the current` |
|        - | 2234 | ``	 * namespace, so `use PHPUnit\Framework; extends Framework\TestCase` resolved`` |
|        - | 2235 | `	 * to Ns\Framework\TestCase. Mirror GenStateResolveNamespaceLiteral here. */` |
| 14192631 | 2236 | `	while( nFirst < nName && zName[nFirst] != '\\' ){ nFirst++; }` |
|  1108509 | 2237 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)zName,nFirst);` |
|  1108509 | 2238 | `	if( pImport ){` |
|       46 | 2239 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       46 | 2240 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|       46 | 2241 | `		SyBlobAppend(pOut,zName + nFirst,nName - nFirst); /* the \B\C tail, if any */` |
|       46 | 2242 | `		return;` |
|        - | 2243 | `	}` |
|        - | 2244 | `	/* Prepend current namespace if active */` |
|  1108467 | 2245 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       23 | 2246 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       23 | 2247 | `		SyBlobAppend(pOut,"\\",1);` |
|       10 | 2248 | `	}` |
|  1108467 | 2249 | `	SyBlobAppend(pOut,zName,nName);` |
|   554257 | 2250 | `}` |
|        - | 2251 | `/*` |
|        - | 2252 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|        - | 2253 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|        - | 2254 | ` * The caller must release pOut when done.` |
|        - | 2255 | ` */` |
|  1131226 | 2256 | `PH7_PRIVATE void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|        5 | 2257 | `{` |
|  1131231 | 2258 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     8489 | 2259 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     8489 | 2260 | `		SyBlobAppend(pOut,"\\",1);` |
|     4242 | 2261 | `	}` |
|  1131231 | 2262 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|  1131231 | 2263 | `}` |
|        - | 2264 | `/*` |
|        - | 2265 | ` * Compile a namespace statement` |
|        - | 2266 | ` * According to the PHP language reference manual` |
|        - | 2267 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|        - | 2268 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|        - | 2269 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|        - | 2270 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|        - | 2271 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|        - | 2272 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|        - | 2273 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|        - | 2274 | ` *  programming world.` |
|        - | 2275 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|        - | 2276 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|        - | 2277 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|        - | 2278 | ` *  classes/functions/constants.` |
|        - | 2279 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|        - | 2280 | ` *  readability of source code.` |
|        - | 2281 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|        - | 2282 | ` *  Here is an example of namespace syntax in PHP:` |
|        - | 2283 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|        - | 2284 | ` *       class MyClass {}` |
|        - | 2285 | ` *       function myfunction() {}` |
|        - | 2286 | ` *       const MYCONST = 1;` |
|        - | 2287 | ` *       $a = new MyClass;` |
|        - | 2288 | ` *       $c = new \my\name\MyClass;` |
|        - | 2289 | ` *       $a = strlen('hi');` |
|        - | 2290 | ` *       $d = namespace\MYCONST;` |
|        - | 2291 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|        - | 2292 | ` *       echo constant($d);` |
|        - | 2293 | ` * NOTE` |
|        - | 2294 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|        - | 2295 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|        - | 2296 | ` */` |
|        - | 2297 | `/*` |
|        - | 2298 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|        - | 2299 | ` */` |
|       14 | 2300 | `PH7_PRIVATE const char * TokenTypeName(sxu32 nType)` |
|        4 | 2301 | `{` |
|       18 | 2302 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|       12 | 2303 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|       12 | 2304 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|       12 | 2305 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|       12 | 2306 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|       12 | 2307 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|        3 | 2308 | `	return "token";` |
|       11 | 2309 | `}` |
|     4286 | 2310 | `PH7_PRIVATE sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|        5 | 2311 | `{` |
|        - | 2312 | `	sxu32 nLine;` |
|        - | 2313 | `	sxi32 rc;` |
|     4291 | 2314 | `	nLine = pGen->pIn->nLine;` |
|     4291 | 2315 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|        - | 2316 | `	/* Reset namespace and clear previous use imports */` |
|     4291 | 2317 | `	SyBlobReset(&pGen->sNamespace);` |
|     4291 | 2318 | `	SyHashRelease(&pGen->hUseImports);` |
|     4291 | 2319 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|     4291 | 2320 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     4291 | 2321 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|     4291 | 2322 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     4291 | 2323 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|     4291 | 2324 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - | 2325 | `		/* Global namespace (bare "namespace;") */` |
|      ! 0 | 2326 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|      ! 0 | 2327 | `		return SXRET_OK;` |
|        - | 2328 | `	}` |
|     4291 | 2329 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|        - | 2330 | `		/* namespace; — switch to global namespace */` |
|      ! 0 | 2331 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|      ! 0 | 2332 | `		return SXRET_OK;` |
|        - | 2333 | `	}` |
|     4291 | 2334 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|        - | 2335 | `		/* namespace { } — global namespace block */` |
|        5 | 2336 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|        5 | 2337 | `		return SXRET_OK;` |
|        - | 2338 | `	}` |
|        - | 2339 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     8651 | 2340 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     4369 | 2341 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|        - | 2342 | `			/* Append backslash separator */` |
|       47 | 2343 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       47 | 2344 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|       21 | 2345 | `			}` |
|       26 | 2346 | `		}else{` |
|        - | 2347 | `			/* Append identifier */` |
|     4327 | 2348 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        - | 2349 | `		}` |
|     4369 | 2350 | `		pGen->pIn++;` |
|        5 | 2351 | `	}` |
|        - | 2352 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|        - | 2353 | `	 * at the correct program counter, not just the last one compiled. */` |
|        - | 2354 | `	{` |
|     4287 | 2355 | `		char *zNsDup = 0;` |
|     4287 | 2356 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     6425 | 2357 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     4280 | 2358 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     2140 | 2359 | `		}` |
|     4287 | 2360 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|        - | 2361 | `	}` |
|     4287 | 2362 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        8 | 2363 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - | 2364 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|        4 | 2365 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        6 | 2366 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2367 | `			return SXERR_ABORT;` |
|        - | 2368 | `		}` |
|        2 | 2369 | `	}` |
|     4287 | 2370 | `	return SXRET_OK;` |
|     2148 | 2371 | `}` |
|        - | 2372 | `/*` |
|        - | 2373 | ` * Compile the 'use' statement` |
|        - | 2374 | ` * According to the PHP language reference manual` |
|        - | 2375 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|        - | 2376 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|        - | 2377 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|        - | 2378 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|        - | 2379 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|        - | 2380 | ` *  a function or constant is not supported.` |
|        - | 2381 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|        - | 2382 | ` * NOTE` |
|        - | 2383 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|        - | 2384 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|        - | 2385 | ` */` |
|       82 | 2386 | `PH7_PRIVATE sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|        5 | 2387 | `{` |
|        - | 2388 | `	sxu32 nLine;` |
|        - | 2389 | `	sxi32 rc;` |
|        - | 2390 | `	SyBlob sPath;` |
|        - | 2391 | `	SyString sAlias;` |
|        - | 2392 | `	SyToken *pLast;` |
|        - | 2393 | `	char *zDup;` |
|        - | 2394 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|        - | 2395 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|        - | 2396 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|       87 | 2397 | `	nLine = pGen->pIn->nLine;` |
|       87 | 2398 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|        - | 2399 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|       87 | 2400 | `	iUseType = 0;` |
|       87 | 2401 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       30 | 2402 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|       30 | 2403 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|       16 | 2404 | `			iUseType = 1;` |
|       16 | 2405 | `			pGen->pIn++;` |
|       23 | 2406 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|       16 | 2407 | `			iUseType = 2;` |
|       16 | 2408 | `			pGen->pIn++;` |
|        7 | 2409 | `		}` |
|       14 | 2410 | `	}` |
|        - | 2411 | `	/* Select target hash tables based on import type */` |
|       87 | 2412 | `	switch( iUseType ){` |
|        7 | 2413 | `		case 1:` |
|       16 | 2414 | `			pGenHash = &pGen->hUseFuncImports;` |
|       16 | 2415 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|       16 | 2416 | `			break;` |
|        7 | 2417 | `		case 2:` |
|       16 | 2418 | `			pGenHash = &pGen->hUseConstImports;` |
|       16 | 2419 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|       16 | 2420 | `			break;` |
|       27 | 2421 | `		default:` |
|       59 | 2422 | `			pGenHash = &pGen->hUseImports;` |
|       59 | 2423 | `			pVmHash = &pGen->pVm->hUseImports;` |
|       54 | 2424 | `			break;` |
|        - | 2425 | `	}` |
|       87 | 2426 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|        - | 2427 | `	/* Process one or more use declarations separated by commas */` |
|       42 | 2428 | `	for(;;){` |
|       89 | 2429 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 2430 | `			break;` |
|        - | 2431 | `		}` |
|       89 | 2432 | `		SyBlobReset(&sPath);` |
|       89 | 2433 | `		pLast = 0;` |
|        - | 2434 | `		/* Collect the full namespace path */` |
|      309 | 2435 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|      225 | 2436 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|      155 | 2437 | `				pLast = pGen->pIn;` |
|      155 | 2438 | `				if( SyBlobLength(&sPath) > 0 ){` |
|       75 | 2439 | `					SyBlobAppend(&sPath,"\\",1);` |
|       35 | 2440 | `				}` |
|      155 | 2441 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       75 | 2442 | `			}` |
|      225 | 2443 | `			pGen->pIn++;` |
|        5 | 2444 | `		}` |
|       89 | 2445 | `		if( pLast == 0 ){` |
|        - | 2446 | `			/* Empty path */` |
|        5 | 2447 | `			break;` |
|        - | 2448 | `		}` |
|        - | 2449 | `		/* Default alias is the last component of the path */` |
|       85 | 2450 | `		sAlias = pLast->sData;` |
|        - | 2451 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|       80 | 2452 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       57 | 2453 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|       28 | 2454 | `			pGen->pIn++; /* Jump 'as' */` |
|       28 | 2455 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|       28 | 2456 | `				sAlias = pGen->pIn->sData;` |
|       28 | 2457 | `				pGen->pIn++;` |
|       12 | 2458 | `			}` |
|       12 | 2459 | `		}` |
|        - | 2460 | `		/* Check for duplicate import alias (per-type) */` |
|       85 | 2461 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|        8 | 2462 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 2463 | `				"Cannot use %.*s as %z because the name is already in use",` |
|        4 | 2464 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|        6 | 2465 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2466 | `				SyBlobRelease(&sPath);` |
|      ! 0 | 2467 | `				return SXERR_ABORT;` |
|        - | 2468 | `			}` |
|        2 | 2469 | `		}` |
|        - | 2470 | `		/* Register the import: alias -> FQN.` |
|        - | 2471 | `		 * Strings are allocated from the VM pool allocator and freed` |
|        - | 2472 | `		 * when the entire VM is released. SyHashRelease does not free` |
|        - | 2473 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|      125 | 2474 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       80 | 2475 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|       85 | 2476 | `		if( zDup ){` |
|       85 | 2477 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|       85 | 2478 | `			if( pVmHash ){` |
|        - | 2479 | `				/* Class imports: populate VM table directly (class resolution` |
|        - | 2480 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|       57 | 2481 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       57 | 2482 | `				if( zAliasDup ){` |
|       57 | 2483 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|       26 | 2484 | `				}` |
|       26 | 2485 | `			}` |
|       85 | 2486 | `			if( iUseType == 2 ){` |
|        - | 2487 | `				/* Const imports: emit a runtime instruction so imports are` |
|        - | 2488 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|       16 | 2489 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       16 | 2490 | `				if( zAliasDup ){` |
|        - | 2491 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|        - | 2492 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|        - | 2493 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|       16 | 2494 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|       16 | 2495 | `					if( azPair ){` |
|       16 | 2496 | `						azPair[0] = zAliasDup;` |
|       16 | 2497 | `						azPair[1] = zDup;` |
|       16 | 2498 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|        7 | 2499 | `					}` |
|        7 | 2500 | `				}` |
|        7 | 2501 | `			}` |
|       40 | 2502 | `		}` |
|        - | 2503 | `		/* Check for comma (multiple use declarations) */` |
|       85 | 2504 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|        3 | 2505 | `			pGen->pIn++;` |
|        2 | 2506 | `		}else{` |
|       44 | 2507 | `			break;` |
|        - | 2508 | `		}` |
|        1 | 2509 | `	}` |
|       87 | 2510 | `	SyBlobRelease(&sPath);` |
|       87 | 2511 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        4 | 2512 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|        2 | 2513 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        3 | 2514 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2515 | `			return SXERR_ABORT;` |
|        - | 2516 | `		}` |
|        1 | 2517 | `	}` |
|       87 | 2518 | `	return SXRET_OK;` |
|       46 | 2519 | `}` |
|        - | 2520 | `/*` |
|        - | 2521 | ` * Compile the stupid 'declare' language construct.` |
|        - | 2522 | ` *` |
|        - | 2523 | ` * According to the PHP language reference manual.` |
|        - | 2524 | ` *  The declare construct is used to set execution directives for a block of code.` |
|        - | 2525 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|        - | 2526 | ` *  declare (directive)` |
|        - | 2527 | ` *   statement` |
|        - | 2528 | ` * The directive section allows the behavior of the declare block to be set.` |
|        - | 2529 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|        - | 2530 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|        - | 2531 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|        - | 2532 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|        - | 2533 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|        - | 2534 | ` * <?php` |
|        - | 2535 | ` * // these are the same:` |
|        - | 2536 | ` * // you can use this:` |
|        - | 2537 | ` * declare(ticks=1) {` |
|        - | 2538 | ` *   // entire script here` |
|        - | 2539 | ` * }` |
|        - | 2540 | ` * // or you can use this:` |
|        - | 2541 | ` * declare(ticks=1);` |
|        - | 2542 | ` * // entire script here` |
|        - | 2543 | ` * ?>` |
|        - | 2544 | ` *` |
|        - | 2545 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|        - | 2546 | ` */` |
|        - | 2547 | `/*` |
|        - | 2548 | ` * Match a directive name against a known literal (case-insensitive).` |
|        - | 2549 | ` */` |
|       88 | 2550 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|        5 | 2551 | `{` |
|      132 | 2552 | `	return SyStringLength(pName) == nWant` |
|       88 | 2553 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|        5 | 2554 | `}` |
|        - | 2555 |  |
|       48 | 2556 | `PH7_PRIVATE sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|        5 | 2557 | `{` |
|       53 | 2558 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       53 | 2559 | `	SyToken *pBodyEnd = 0;` |
|        - | 2560 | `	SyToken *pBodyStart;` |
|        - | 2561 | `	SyToken *pCursor;` |
|        - | 2562 | `	int bHasStrictTypes;` |
|        - | 2563 | `	int bBlockForm;` |
|        - | 2564 | `	int bPlacementOk;` |
|        - | 2565 | `	sxi32 rc;` |
|       53 | 2566 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|       53 | 2567 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|        6 | 2568 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|        6 | 2569 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2570 | `			return SXERR_ABORT;` |
|        - | 2571 | `		}` |
|        6 | 2572 | `		goto Synchro;` |
|        - | 2573 | `	}` |
|       49 | 2574 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|       49 | 2575 | `	pBodyStart = pGen->pIn;` |
|        - | 2576 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|       49 | 2577 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|       49 | 2578 | `	if( pBodyEnd >= pGen->pEnd ){` |
|      ! 0 | 2579 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\")\"");` |
|      ! 0 | 2580 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2581 | `			return SXERR_ABORT;` |
|        - | 2582 | `		}` |
|      ! 0 | 2583 | `		return SXRET_OK;` |
|        - | 2584 | `	}` |
|        - | 2585 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|        - | 2586 | `	 * now delimits the comma-separated directive list. */` |
|       49 | 2587 | `	pGen->pIn = &pBodyEnd[1];` |
|       49 | 2588 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      ! 0 | 2589 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|      ! 0 | 2590 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2591 | `			return SXERR_ABORT;` |
|        - | 2592 | `		}` |
|      ! 0 | 2593 | `	}` |
|       49 | 2594 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|       49 | 2595 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|       49 | 2596 | `	bHasStrictTypes = 0;` |
|        - | 2597 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|        - | 2598 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|        - | 2599 | `	 * directive appears anywhere in the list, before validating values. */` |
|       49 | 2600 | `	pCursor = pBodyStart;` |
|       61 | 2601 | `	while( pCursor < pBodyEnd ){` |
|       57 | 2602 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|       49 | 2603 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|       45 | 2604 | `				bHasStrictTypes = 1;` |
|       45 | 2605 | `				break;` |
|        - | 2606 | `			}` |
|        2 | 2607 | `		}` |
|       14 | 2608 | `		pCursor++;` |
|        2 | 2609 | `	}` |
|       49 | 2610 | `	if( bHasStrictTypes && bBlockForm ){` |
|        3 | 2611 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2612 | `			"strict_types declaration must not use block mode");` |
|        3 | 2613 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        3 | 2614 | `		return SXRET_OK;` |
|        - | 2615 | `	}` |
|       47 | 2616 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|        6 | 2617 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2618 | `			"strict_types declaration must be the very first statement in the script");` |
|        6 | 2619 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        6 | 2620 | `		return SXRET_OK;` |
|        - | 2621 | `	}` |
|        - | 2622 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|       43 | 2623 | `	pCursor = pBodyStart;` |
|       81 | 2624 | `	while( pCursor < pBodyEnd ){` |
|        - | 2625 | `		SyToken *pNameTok;` |
|        - | 2626 | `		SyToken *pEqTok;` |
|        - | 2627 | `		SyToken *pValTok;` |
|        - | 2628 | `		SyString *pDirName;` |
|        - | 2629 | `		int bIsStrict;` |
|        - | 2630 | `		int iStrictValue;` |
|       45 | 2631 | `		pNameTok = pCursor;` |
|       45 | 2632 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 2633 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2634 | `				"declare: Expecting a directive name");` |
|      ! 0 | 2635 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2636 | `			return SXRET_OK;` |
|        - | 2637 | `		}` |
|       45 | 2638 | `		pEqTok = pNameTok + 1;` |
|       45 | 2639 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|      ! 0 | 2640 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2641 | `				"declare: Expecting '=' after directive name");` |
|      ! 0 | 2642 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2643 | `			return SXRET_OK;` |
|        - | 2644 | `		}` |
|       45 | 2645 | `		pValTok = pEqTok + 1;` |
|       45 | 2646 | `		if( pValTok >= pBodyEnd ){` |
|      ! 0 | 2647 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2648 | `				"declare: Expecting value after '='");` |
|      ! 0 | 2649 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2650 | `			return SXRET_OK;` |
|        - | 2651 | `		}` |
|       45 | 2652 | `		pDirName = &pNameTok->sData;` |
|       45 | 2653 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|       45 | 2654 | `		if( bIsStrict ){` |
|        - | 2655 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|        - | 2656 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|       41 | 2657 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      ! 0 | 2658 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2659 | `					"declare(strict_types) value must be a literal");` |
|      ! 0 | 2660 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2661 | `				return SXRET_OK;` |
|        - | 2662 | `			}` |
|       41 | 2663 | `			iStrictValue = -1;` |
|       41 | 2664 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|       41 | 2665 | `				const char *zv = SyStringData(&pValTok->sData);` |
|       41 | 2666 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|       41 | 2667 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|       39 | 2668 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|       18 | 2669 | `			}` |
|       41 | 2670 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|        3 | 2671 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2672 | `					"strict_types declaration must have 0 or 1 as its value");` |
|        3 | 2673 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        3 | 2674 | `				return SXRET_OK;` |
|        - | 2675 | `			}` |
|       39 | 2676 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|       23 | 2677 | `		}else if( DeclareNameIs(pDirName, "encoding", sizeof("encoding")-1) ){` |
|        - | 2678 | `			/* php always ignores declare(encoding=...) unless it was built with` |
|        - | 2679 | `			 * Zend multibyte, and says so in these exact words. */` |
|        3 | 2680 | `			PH7_GenCompileError(&(*pGen),E_WARNING,nLine,` |
|        - | 2681 | `				"declare(encoding=...) ignored because Zend multibyte feature is turned off by settings");` |
|        1 | 2682 | `		}else{` |
|        - | 2683 | `			/* Other directives (ticks and friends) are accepted as no-ops.` |
|        - | 2684 | `			 * This used to emit a NOTICE naming the upstream engine and its` |
|        - | 2685 | `			 * version ("the declare construct is a no-op in the current release` |
|        - | 2686 | `			 * of the PH7(2.1.4) engine") — php prints nothing at all for` |
|        - | 2687 | ``			 * `declare(ticks=1)`, and leaking the old engine's branding into`` |
|        - | 2688 | `			 * user-visible diagnostics was wrong on its own. */` |
|        - | 2689 | `		}` |
|       43 | 2690 | `		pCursor = pValTok + 1;` |
|        - | 2691 | `		/* Consume separating comma (or end). */` |
|       43 | 2692 | `		if( pCursor < pBodyEnd ){` |
|        3 | 2693 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|      ! 0 | 2694 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2695 | `					"declare: Expecting ',' or ')' after directive value");` |
|      ! 0 | 2696 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2697 | `				return SXRET_OK;` |
|        - | 2698 | `			}` |
|        3 | 2699 | `			pCursor++;` |
|        1 | 2700 | `		}` |
|        5 | 2701 | `	}` |
|        - | 2702 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|        - | 2703 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|        - | 2704 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|       41 | 2705 | `	return SXRET_OK;` |
|        2 | 2706 | `Synchro:` |
|        - | 2707 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|       16 | 2708 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       12 | 2709 | `		pGen->pIn++;` |
|        2 | 2710 | `	}` |
|        6 | 2711 | `	return SXRET_OK;` |
|       29 | 2712 | `}` |
|        - | 2713 | `/*` |
|        - | 2714 | ` * Compile a class constant.` |
|        - | 2715 | ` * According to the PHP language reference manual` |
|        - | 2716 | ` *  Class Constants` |
|        - | 2717 | ` *   It is possible to define constant values on a per-class basis remaining` |
|        - | 2718 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|        - | 2719 | ` *   you don't use the $ symbol to declare or use them.` |
|        - | 2720 | ` *   The value must be a constant expression, not (for example) a variable,` |
|        - | 2721 | ` *   a property, a result of a mathematical operation, or a function call.` |
|        - | 2722 | ` *   It's also possible for interfaces to have constants.` |
|        - | 2723 | ` * Symisc eXtension.` |
|        - | 2724 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|        - | 2725 | ` *  the zend engine would allow only simple scalar value.` |
|        - | 2726 | ` *  Example:` |
|        - | 2727 | ` *   class Test{` |
|        - | 2728 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|        - | 2729 | ` *   };` |
|        - | 2730 | ` *   var_dump(TEST::MyConst);` |
|        - | 2731 | ` *   Refer to the official documentation for more information on the powerful extension` |
|        - | 2732 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|        - | 2733 | ` */` |
|        - | 2734 | `/*` |
|        - | 2735 | ` * Exception handling.` |
|        - | 2736 | ` *  According to the PHP language reference manual` |
|        - | 2737 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|        - | 2738 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|        - | 2739 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|        - | 2740 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|        - | 2741 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|        - | 2742 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|        - | 2743 | ` *    (or re-thrown) within a catch block.` |
|        - | 2744 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|        - | 2745 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|        - | 2746 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|        - | 2747 | ` *    been defined with set_exception_handler().` |
|        - | 2748 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|        - | 2749 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|        - | 2750 | ` */` |
|        - | 2751 | `/*` |
|        - | 2752 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|        - | 2753 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|        - | 2754 | ` * indicates failure.` |
|        - | 2755 | ` */` |
|   592492 | 2756 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 2757 | `{` |
|        - | 2758 | ``	/* php decides throwability at RUNTIME: `throw 5` compiles and raises the`` |
|        - | 2759 | `	 * catchable Error "Can only throw objects" when it executes, and a` |
|        - | 2760 | `	 * non-Throwable object raises "Cannot throw objects that do not implement` |
|        - | 2761 | `	 * Throwable". This used to whitelist a handful of expression shapes and` |
|        - | 2762 | `	 * reject the rest at compile time as a "friendlier" error, which meant the` |
|        - | 2763 | `	 * program never ran and no catch could ever see it — and it was inconsistent` |
|        - | 2764 | ``	 * anyway, since `throw $v` (a variable holding an int) always compiled and`` |
|        - | 2765 | `	 * fell through to the same runtime path. OP_THROW now owns the whole rule,` |
|        - | 2766 | ``	 * so accept any expression here; `throw;` with no operand is still rejected`` |
|        - | 2767 | `	 * by PH7_CompileThrow's SXERR_EMPTY branch. */` |
|   296246 | 2768 | `	SXUNUSED(pGen);` |
|   296246 | 2769 | `	SXUNUSED(pRoot);` |
|   592497 | 2770 | `	return SXRET_OK;` |
|        5 | 2771 | `}` |
|        - | 2772 | `/*` |
|        - | 2773 | ` * Compile a 'throw' statement.` |
|        - | 2774 | ` * throw: This is how you trigger an exception.` |
|        - | 2775 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|        - | 2776 | ` */` |
|   592454 | 2777 | `PH7_PRIVATE sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|        5 | 2778 | `{` |
|   592459 | 2779 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2780 | `	GenBlock *pBlock;` |
|        - | 2781 | `	sxu32 nIdx;` |
|        - | 2782 | `	sxi32 rc;` |
|   592459 | 2783 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|        - | 2784 | `	/* Compile the expression */` |
|   592459 | 2785 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|   592459 | 2786 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 | 2787 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|      ! 0 | 2788 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2789 | `			return SXERR_ABORT;` |
|        - | 2790 | `		}` |
|      ! 0 | 2791 | `		return SXRET_OK;` |
|        - | 2792 | `	}` |
|   592459 | 2793 | `	pBlock = pGen->pCurrent;` |
|        - | 2794 | `	/* Point to the top most function or try block and emit the forward jump */` |
|  2360689 | 2795 | `	while(pBlock->pParent){` |
|  2360681 | 2796 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|   592451 | 2797 | `			break;` |
|        - | 2798 | `		}` |
|        - | 2799 | `		/* Point to the parent block */` |
|  1768235 | 2800 | `		pBlock = pBlock->pParent;` |
|        5 | 2801 | `	}` |
|        - | 2802 | `	/* Emit the throw instruction */` |
|   592459 | 2803 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|        - | 2804 | `	/* Emit the jump */` |
|   592459 | 2805 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|   592459 | 2806 | `	return SXRET_OK;` |
|   296232 | 2807 | `}` |
|        - | 2808 | `/*` |
|        - | 2809 | ` * Compile a PHP 8.0 'throw' expression.` |
|        - | 2810 | ` * Called from the expression code generator when a 'throw' keyword is` |
|        - | 2811 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|        - | 2812 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|        - | 2813 | ` * the validator guarantees the operand is a valid exception target.` |
|        - | 2814 | ` */` |
|       38 | 2815 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|        2 | 2816 | `{` |
|       40 | 2817 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2818 | `	GenBlock *pBlock;` |
|        - | 2819 | `	sxu32 nIdx;` |
|        - | 2820 | `	sxi32 rc;` |
|       19 | 2821 | `	(void)iCompileFlag;` |
|       40 | 2822 | `	pGen->pIn++; /* Skip 'throw' */` |
|       40 | 2823 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 2824 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 2825 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 2826 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2827 | `			return SXERR_ABORT;` |
|        - | 2828 | `		}` |
|      ! 0 | 2829 | `		return SXRET_OK;` |
|        - | 2830 | `	}` |
|       40 | 2831 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|       40 | 2832 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 2833 | `		return SXERR_ABORT;` |
|        - | 2834 | `	}` |
|       40 | 2835 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 | 2836 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 2837 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 2838 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2839 | `			return SXERR_ABORT;` |
|        - | 2840 | `		}` |
|      ! 0 | 2841 | `		return SXRET_OK;` |
|        - | 2842 | `	}` |
|        - | 2843 | `	/* Walk up to nearest exception/function block for the jump target */` |
|       40 | 2844 | `	pBlock = pGen->pCurrent;` |
|       64 | 2845 | `	while( pBlock->pParent ){` |
|       54 | 2846 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|       30 | 2847 | `			break;` |
|        - | 2848 | `		}` |
|       26 | 2849 | `		pBlock = pBlock->pParent;` |
|        2 | 2850 | `	}` |
|       40 | 2851 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       40 | 2852 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|       40 | 2853 | `	return SXRET_OK;` |
|       21 | 2854 | `}` |
|        - | 2855 | `/*` |
|        - | 2856 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|        - | 2857 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|        - | 2858 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|        - | 2859 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|        - | 2860 | ` * compile error propagated from the parser.` |
|        - | 2861 | ` */` |
|       60 | 2862 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|        5 | 2863 | `{` |
|        - | 2864 | `	SyString sClassName;` |
|        - | 2865 | `	SyToken *pToken;` |
|        - | 2866 | `	SyString *pName;` |
|        - | 2867 | `	char *zDup;` |
|        - | 2868 | `	sxi32 rc;` |
|       65 | 2869 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       65 | 2870 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|       65 | 2871 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|       65 | 2872 | `	SySetInit(&pCatch->sByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       65 | 2873 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 | 2874 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 2875 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 2876 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 2877 | `		return SXERR_INVALID;` |
|        - | 2878 | `	}` |
|       65 | 2879 | `	pGen->pIn++; /* '(' */` |
|       30 | 2880 | `	for(;;){` |
|        - | 2881 | `		SyBlob sResolved;` |
|       65 | 2882 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       65 | 2883 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 2884 | `			SyBlobRelease(&sResolved);` |
|      ! 0 | 2885 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 2886 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 2887 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 2888 | `			return SXERR_INVALID;` |
|        - | 2889 | `		}` |
|       95 | 2890 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       60 | 2891 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       65 | 2892 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|       65 | 2893 | `		SyBlobRelease(&sResolved);` |
|       65 | 2894 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|       65 | 2895 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|       65 | 2896 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       60 | 2897 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|        5 | 2898 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|      ! 0 | 2899 | `			pGen->pIn++; continue;` |
|        - | 2900 | `		}` |
|       65 | 2901 | `		break;` |
|      ! 0 | 2902 | `	}` |
|        - | 2903 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|        - | 2904 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding. */` |
|       65 | 2905 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|        3 | 2906 | `		pGen->pIn++; /* ')' */` |
|        3 | 2907 | `		return SXRET_OK;` |
|        - | 2908 | `	}` |
|       58 | 2909 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|       63 | 2910 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 2911 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 2912 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 2913 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 2914 | `		return SXERR_INVALID;` |
|        - | 2915 | `	}` |
|       63 | 2916 | `	pGen->pIn++; /* '$' */` |
|       63 | 2917 | `	pName = &pGen->pIn->sData;` |
|       63 | 2918 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       63 | 2919 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|       63 | 2920 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|       63 | 2921 | `	pGen->pIn++;` |
|       63 | 2922 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 | 2923 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 2924 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 2925 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 2926 | `		return SXERR_INVALID;` |
|        - | 2927 | `	}` |
|       63 | 2928 | `	pGen->pIn++; /* ')' */` |
|       63 | 2929 | `	return SXRET_OK;` |
|       35 | 2930 | `}` |
|        - | 2931 | `/*` |
|        - | 2932 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|        - | 2933 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|        - | 2934 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|        - | 2935 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|        - | 2936 | ` * VmThrowException):` |
|        - | 2937 | ` *` |
|        - | 2938 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|        - | 2939 | ` *    <try body>` |
|        - | 2940 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|        - | 2941 | ` *    JMP  -> finally\|end` |
|        - | 2942 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|        - | 2943 | ` *    <catch body>` |
|        - | 2944 | ` *    JMP  -> finally\|end` |
|        - | 2945 | ` *    ... more catches ...` |
|        - | 2946 | ` *  Lfin: <finally body>` |
|        - | 2947 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|        - | 2948 | ` *  Lend:` |
|        - | 2949 | ` */` |
|      112 | 2950 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|        5 | 2951 | `{` |
|      117 | 2952 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2953 | `	GenBlock *pTry;` |
|        - | 2954 | `	VmInstr *pInstr;` |
|      117 | 2955 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|        - | 2956 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|        - | 2957 | `	sxi32 rc;` |
|      117 | 2958 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|        - | 2959 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION) */` |
|      117 | 2960 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|      117 | 2961 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      117 | 2962 | `	pTry->pUserData = pException;` |
|      117 | 2963 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|      117 | 2964 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|      117 | 2965 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      117 | 2966 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      117 | 2967 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|      117 | 2968 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|        - | 2969 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|      117 | 2970 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|      117 | 2971 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|      117 | 2972 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|      117 | 2973 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 2974 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|      117 | 2975 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|        - | 2976 | `	/* Catch clauses (inline) */` |
|      117 | 2977 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      112 | 2978 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       65 | 2979 | `		sxu32 k = 0;` |
|       90 | 2980 | `		for(;;){` |
|        - | 2981 | `			ph7_exception_block sCatch;` |
|        - | 2982 | `			GenBlock *pCatchBlk;` |
|      125 | 2983 | `			sxu32 idxJmp = 0;` |
|      120 | 2984 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      115 | 2985 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|       35 | 2986 | `				break;` |
|        - | 2987 | `			}` |
|       65 | 2988 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|       65 | 2989 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       65 | 2990 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|       65 | 2991 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|       65 | 2992 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|       65 | 2993 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatchBlk);` |
|       65 | 2994 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        - | 2995 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|        - | 2996 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|        - | 2997 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump). */` |
|       65 | 2998 | `			pCatchBlk->pUserData = pException;` |
|       65 | 2999 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|       65 | 3000 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       65 | 3001 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|       65 | 3002 | `			GenStateLeaveBlock(&(*pGen),0);` |
|        - | 3003 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|        - | 3004 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|       65 | 3005 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       65 | 3006 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|       65 | 3007 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|       65 | 3008 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|       65 | 3009 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       65 | 3010 | `			k++;` |
|        5 | 3011 | `		}` |
|       30 | 3012 | `	}` |
|        - | 3013 | `	/* Finally (inline) */` |
|      117 | 3014 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       92 | 3015 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|        - | 3016 | `		GenBlock *pFinBlk;` |
|       61 | 3017 | `		pGen->pIn++; /* Jump 'finally' */` |
|       61 | 3018 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|       61 | 3019 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|       61 | 3020 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       61 | 3021 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       61 | 3022 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       61 | 3023 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|       61 | 3024 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       61 | 3025 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|       61 | 3026 | `		pException->iHasFinally = 1;` |
|       28 | 3027 | `	}` |
|      117 | 3028 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|      117 | 3029 | `	pException->iInlined = 1;` |
|        - | 3030 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|        - | 3031 | `	{` |
|      117 | 3032 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|        - | 3033 | `		sxu32 *aJ; sxu32 n;` |
|      117 | 3034 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|      117 | 3035 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|      117 | 3036 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|      177 | 3037 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|       65 | 3038 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|       65 | 3039 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|       35 | 3040 | `		}` |
|        - | 3041 | `	}` |
|      117 | 3042 | `	SySetRelease(&aCatchJmp);` |
|      117 | 3043 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|      ! 0 | 3044 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|      ! 0 | 3045 | `	}` |
|      117 | 3046 | `	return SXRET_OK;` |
|       61 | 3047 | `}` |
|        - | 3048 | `/*` |
|        - | 3049 | ` * Compile a 'catch' block.` |
|        - | 3050 | ` * Catch: A "catch" block retrieves an exception and creates` |
|        - | 3051 | ` * an object containing the exception information.` |
|        - | 3052 | ` */` |
|    26914 | 3053 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|        5 | 3054 | `{` |
|    26919 | 3055 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 3056 | `	ph7_exception_block sCatch;` |
|        - | 3057 | `	SySet *pInstrContainer;` |
|        - | 3058 | `	SyString sClassName;` |
|        - | 3059 | `	GenBlock *pCatch;` |
|        - | 3060 | `	SyToken *pToken;` |
|        - | 3061 | `	SyString *pName;` |
|        - | 3062 | `	char *zDup;` |
|        - | 3063 | `	sxi32 rc;` |
|    26919 | 3064 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|        - | 3065 | `	/* Zero the structure */` |
|    26919 | 3066 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|        - | 3067 | `	/* Initialize fields */` |
|    26919 | 3068 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|    26919 | 3069 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|    26919 | 3070 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|        - | 3071 | `			/* Unexpected token,break immediately */` |
|      ! 0 | 3072 | `			pToken = pGen->pIn;` |
|      ! 0 | 3073 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 3074 | `				pToken--;` |
|      ! 0 | 3075 | `			}` |
|      ! 0 | 3076 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 3077 | `				"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 3078 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 3079 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3080 | `				return SXERR_ABORT;` |
|        - | 3081 | `			}` |
|      ! 0 | 3082 | `			return SXERR_INVALID;` |
|        - | 3083 | `	}` |
|        - | 3084 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|    26919 | 3085 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|    13473 | 3086 | `	for(;;){` |
|        - | 3087 | `		SyBlob sResolved;` |
|    26951 | 3088 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    26951 | 3089 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|        6 | 3090 | `			SyBlobRelease(&sResolved);` |
|        6 | 3091 | `			pToken = pGen->pIn;` |
|        6 | 3092 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 3093 | `				pToken--;` |
|      ! 0 | 3094 | `			}` |
|        8 | 3095 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 3096 | `				"syntax error, unexpected %s \"%z\"",` |
|        2 | 3097 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|        6 | 3098 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3099 | `				return SXERR_ABORT;` |
|        - | 3100 | `			}` |
|        6 | 3101 | `			return SXERR_INVALID;` |
|        - | 3102 | `		}` |
|        - | 3103 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|        - | 3104 | `		 * transient SyBlob allocation. */` |
|    40418 | 3105 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    26942 | 3106 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|    26947 | 3107 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|    26947 | 3108 | `		SyBlobRelease(&sResolved);` |
|    26947 | 3109 | `		if( zDup == 0 ){` |
|      ! 0 | 3110 | `			goto Mem;` |
|        - | 3111 | `		}` |
|    26947 | 3112 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|    26947 | 3113 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 3114 | `			goto Mem;` |
|        - | 3115 | `		}` |
|        - | 3116 | `		/* Check for '\|' (multi-catch separator) */` |
|    26942 | 3117 | `		if( pGen->pIn < pGen->pEnd &&` |
|    26942 | 3118 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|       37 | 3119 | `			pGen->pIn->sData.nByte == 1 &&` |
|       32 | 3120 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|       34 | 3121 | `			pGen->pIn++; /* Consume the '\|' */` |
|       34 | 3122 | `			continue;` |
|        - | 3123 | `		}` |
|    26915 | 3124 | `		break;` |
|      ! 0 | 3125 | `	}` |
|        - | 3126 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|        - | 3127 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding;` |
|        - | 3128 | `	 * jump straight to compiling the block below. */` |
|    26915 | 3129 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) /*)*/ ){` |
|        7 | 3130 | `		goto CatchBody;` |
|        - | 3131 | `	}` |
|    26904 | 3132 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|    26909 | 3133 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - | 3134 | `			/* Unexpected token,break immediately */` |
|      ! 0 | 3135 | `			pToken = pGen->pIn;` |
|      ! 0 | 3136 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 3137 | `				pToken--;` |
|      ! 0 | 3138 | `			}` |
|      ! 0 | 3139 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 3140 | `				"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 3141 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 3142 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3143 | `				return SXERR_ABORT;` |
|        - | 3144 | `			}` |
|      ! 0 | 3145 | `			return SXERR_INVALID;` |
|        - | 3146 | `	}` |
|    26909 | 3147 | `	pGen->pIn++; /* Jump the dollar sign */` |
|        - | 3148 | `	/* Duplicate instance name */` |
|    26909 | 3149 | `	pName = &pGen->pIn->sData;` |
|    26909 | 3150 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    26909 | 3151 | `	if( zDup == 0 ){` |
|      ! 0 | 3152 | `		goto Mem;` |
|        - | 3153 | `	}` |
|    26909 | 3154 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|    26909 | 3155 | `	pGen->pIn++;` |
|    13455 | 3156 | `CatchBody:` |
|    26915 | 3157 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|        - | 3158 | `		/* Unexpected token,break immediately */` |
|      ! 0 | 3159 | `		pToken = pGen->pIn;` |
|      ! 0 | 3160 | `		if( pToken >= pGen->pEnd ){` |
|      ! 0 | 3161 | `			pToken--;` |
|      ! 0 | 3162 | `		}` |
|      ! 0 | 3163 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 3164 | `			"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 3165 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 3166 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3167 | `			return SXERR_ABORT;` |
|        - | 3168 | `		}` |
|      ! 0 | 3169 | `		return SXERR_INVALID;` |
|        - | 3170 | `	}` |
|        - | 3171 | `	/* Compile the block */` |
|    26915 | 3172 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|        - | 3173 | `	/* Create the catch block */` |
|    26915 | 3174 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|    26915 | 3175 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3176 | `		return SXERR_ABORT;` |
|        - | 3177 | `	}` |
|        - | 3178 | `	/* Swap bytecode container */` |
|    26915 | 3179 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    26915 | 3180 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|        - | 3181 | `	/* Compile the block */` |
|    26915 | 3182 | `	PH7_CompileBlock(&(*pGen),0);` |
|        - | 3183 | `	/* Fix forward jumps now the destination is resolved  */` |
|    26915 | 3184 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 3185 | `	/* Emit the DONE instruction */` |
|    26915 | 3186 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - | 3187 | `	/* Leave the block */` |
|    26915 | 3188 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 3189 | `	/* Restore the default container */` |
|    26915 | 3190 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - | 3191 | `	/* Install the catch block */` |
|    26915 | 3192 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|    26915 | 3193 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3194 | `		goto Mem;` |
|        - | 3195 | `	}` |
|    26915 | 3196 | `	return SXRET_OK;` |
|      ! 0 | 3197 | `Mem:` |
|      ! 0 | 3198 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 3199 | `	return SXERR_ABORT;` |
|    13462 | 3200 | `}` |
|        - | 3201 | `/*` |
|        - | 3202 | ` * Compile a 'try' block.` |
|        - | 3203 | ` * A function using an exception should be in a "try" block.` |
|        - | 3204 | ` * If the exception does not trigger, the code will continue` |
|        - | 3205 | ` * as normal. However if the exception triggers, an exception` |
|        - | 3206 | ` * is "thrown".` |
|        - | 3207 | ` */` |
|    27124 | 3208 | `PH7_PRIVATE sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|        5 | 3209 | `{` |
|        - | 3210 | `	ph7_exception *pException;` |
|    27129 | 3211 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 3212 | `	GenBlock *pTry;` |
|        - | 3213 | `	sxu32 nJmpIdx;` |
|        - | 3214 | `	sxi32 rc;` |
|        - | 3215 | `	/* Create the exception container */` |
|    27129 | 3216 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|    27129 | 3217 | `	if( pException == 0 ){` |
|      ! 0 | 3218 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|      ! 0 | 3219 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 3220 | `		return SXERR_ABORT;` |
|        - | 3221 | `	}` |
|        - | 3222 | `	/* Zero the structure */` |
|    27129 | 3223 | `	SyZero(pException,sizeof(ph7_exception));` |
|        - | 3224 | `	/* Initialize fields */` |
|    27129 | 3225 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|    27129 | 3226 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|    27129 | 3227 | `	pException->iHasFinally = 0;` |
|    27129 | 3228 | `	pException->iFinallyDone = 0;` |
|    27129 | 3229 | `	pException->pVm = pGen->pVm;` |
|        - | 3230 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|        - | 3231 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the legacy path.`` |
|        - | 3232 | `	 * DORMANT until the inline VM handlers (OP_CATCH / OP_END_FINALLY dispatch,` |
|        - | 3233 | `	 * VmThrowException pc-redirect, return/break-through-finally threading, generator` |
|        - | 3234 | `	 * park of aFinallyAction) land — the compiler emits the layout but the VM cannot yet` |
|        - | 3235 | `	 * execute it. Guarded by pVm->bInlineTryCatch (default 0) so the tree stays green. */` |
|    27129 | 3236 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|      117 | 3237 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|        - | 3238 | `	}` |
|        - | 3239 | `	/* Create the try block */` |
|    27017 | 3240 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|    27017 | 3241 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3242 | `		return SXERR_ABORT;` |
|        - | 3243 | `	}` |
|        - | 3244 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|    27017 | 3245 | `	pTry->pUserData = pException;` |
|        - | 3246 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|    27017 | 3247 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|        - | 3248 | `	/* Fix the jump later when the destination is resolved */` |
|    27017 | 3249 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|    27017 | 3250 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|        - | 3251 | `	/* Compile the block */` |
|    27017 | 3252 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|    27017 | 3253 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 3254 | `		return SXERR_ABORT;` |
|        - | 3255 | `	}` |
|        - | 3256 | `	/* Fix forward jumps now the destination is resolved */` |
|    27017 | 3257 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 3258 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|    27017 | 3259 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|        - | 3260 | `	/* Leave the block */` |
|    27017 | 3261 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 3262 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|    27017 | 3263 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|    27010 | 3264 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|        - | 3265 | `		/* Compile one or more catch blocks */` |
|    26909 | 3266 | `		for(;;){` |
|    53818 | 3267 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    40586 | 3268 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|    13457 | 3269 | `					break;` |
|        - | 3270 | `			}` |
|    26919 | 3271 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|    26919 | 3272 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3273 | `				return SXERR_ABORT;` |
|        - | 3274 | `			}` |
|        5 | 3275 | `		}` |
|    13452 | 3276 | `	}` |
|        - | 3277 | `	/* Compile optional finally block */` |
|    27017 | 3278 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     1144 | 3279 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|        - | 3280 | `		SySet *pInstrContainer;` |
|        - | 3281 | `		GenBlock *pFinBlock;` |
|      179 | 3282 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|        - | 3283 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      179 | 3284 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      179 | 3285 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 3286 | `			return SXERR_ABORT;` |
|        - | 3287 | `		}` |
|        - | 3288 | `		/* Swap bytecode container */` |
|      179 | 3289 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      179 | 3290 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|        - | 3291 | `		/* Compile the finally body */` |
|      179 | 3292 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      179 | 3293 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3294 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 | 3295 | `			return SXERR_ABORT;` |
|        - | 3296 | `		}` |
|        - | 3297 | `		/* Fix forward jumps now the destination is resolved */` |
|      179 | 3298 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 3299 | `		/* Emit DONE to terminate the finally block */` |
|      179 | 3300 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - | 3301 | `		/* Leave the block */` |
|      179 | 3302 | `		GenStateLeaveBlock(&(*pGen),0);` |
|        - | 3303 | `		/* Restore the default container */` |
|      179 | 3304 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      179 | 3305 | `		pException->iHasFinally = 1;` |
|       87 | 3306 | `	}` |
|        - | 3307 | `	/* Must have at least one catch or finally */` |
|    27017 | 3308 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|        9 | 3309 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 3310 | `			"Cannot use try without catch or finally");` |
|        9 | 3311 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3312 | `			return SXERR_ABORT;` |
|        - | 3313 | `		}` |
|        3 | 3314 | `	}` |
|    27017 | 3315 | `	return SXRET_OK;` |
|    13567 | 3316 | `}` |
|        - | 3317 | `/*` |
|        - | 3318 | ` * Compile a switch block.` |
|        - | 3319 | ` *  (See block-comment below for more information)` |
|        - | 3320 | ` */` |
|   145018 | 3321 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|        5 | 3322 | `{` |
|   145023 | 3323 | `	sxi32 rc = SXRET_OK;` |
|   145023 | 3324 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|        - | 3325 | `		/* Unexpected token */` |
|      ! 0 | 3326 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 | 3327 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3328 | `			return SXERR_ABORT;` |
|        - | 3329 | `		}` |
|      ! 0 | 3330 | `		pGen->pIn++;` |
|      ! 0 | 3331 | `	}` |
|   145023 | 3332 | `	pGen->pIn++;` |
|        - | 3333 | `	/* First instruction to execute in this block. */` |
|   145023 | 3334 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|        - | 3335 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|        - | 3336 | `	 * or the '}' token */` |
|   138903 | 3337 | `	for(;;){` |
|   277811 | 3338 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 3339 | `			/* No more input to process */` |
|      ! 0 | 3340 | `			break;` |
|        - | 3341 | `		}` |
|   277811 | 3342 | `		rc = SXRET_OK;` |
|   277811 | 3343 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|    37347 | 3344 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|    12453 | 3345 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|        - | 3346 | `					/* Unexpected token */` |
|      ! 0 | 3347 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|      ! 0 | 3348 | `						&pGen->pIn->sData);` |
|      ! 0 | 3349 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3350 | `						return SXERR_ABORT;` |
|        - | 3351 | `					}` |
|        - | 3352 | `					/* FALL THROUGH */` |
|      ! 0 | 3353 | `				}` |
|    12453 | 3354 | `				rc = SXERR_EOF;` |
|    12453 | 3355 | `				break;` |
|        - | 3356 | `			}` |
|    12452 | 3357 | `		}else{` |
|        - | 3358 | `			sxi32 nKwrd;` |
|        - | 3359 | `			/* Extract the keyword */` |
|   240469 | 3360 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   240469 | 3361 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|    66288 | 3362 | `				break;` |
|        - | 3363 | `			}` |
|   107903 | 3364 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|        5 | 3365 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|        - | 3366 | `					/* Unexpected token */` |
|      ! 0 | 3367 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|      ! 0 | 3368 | `						&pGen->pIn->sData);` |
|      ! 0 | 3369 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3370 | `						return SXERR_ABORT;` |
|        - | 3371 | `					}` |
|        - | 3372 | `					/* FALL THROUGH */` |
|      ! 0 | 3373 | `				}` |
|        - | 3374 | `				/* Block compiled */` |
|        5 | 3375 | `				break;` |
|        - | 3376 | `			}` |
|        - | 3377 | `		}` |
|        - | 3378 | `		/* Compile block */` |
|   132793 | 3379 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|   132793 | 3380 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3381 | `			return SXERR_ABORT;` |
|        - | 3382 | `		}` |
|        5 | 3383 | `	}` |
|   145023 | 3384 | `	return rc;` |
|    72514 | 3385 | `}` |
|        - | 3386 | `/*` |
|        - | 3387 | ` * Compile a case eXpression.` |
|        - | 3388 | ` *  (See block-comment below for more information)` |
|        - | 3389 | ` */` |
|   140856 | 3390 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|        5 | 3391 | `{` |
|        - | 3392 | `	SySet *pInstrContainer;` |
|        - | 3393 | `	SyToken *pEnd,*pTmp;` |
|   140861 | 3394 | `	sxi32 iNest = 0;` |
|        - | 3395 | `	sxi32 rc;` |
|        - | 3396 | `	/* Delimit the expression */` |
|   140861 | 3397 | `	pEnd = pGen->pIn;` |
|   281725 | 3398 | `	while( pEnd < pGen->pEnd ){` |
|   281725 | 3399 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|        - | 3400 | `			/* Increment nesting level */` |
|        3 | 3401 | `			iNest++;` |
|   281724 | 3402 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|        - | 3403 | `			/* Decrement nesting level */` |
|        3 | 3404 | `			iNest--;` |
|   281722 | 3405 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|   140861 | 3406 | `			break;` |
|        - | 3407 | `		}` |
|   140869 | 3408 | `		pEnd++;` |
|        5 | 3409 | `	}` |
|   140861 | 3410 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 3411 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|      ! 0 | 3412 | `		if( rc == SXERR_ABORT ){` |
|        - | 3413 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 3414 | `			return SXERR_ABORT;` |
|        - | 3415 | `		}` |
|      ! 0 | 3416 | `	}` |
|        - | 3417 | `	/* Swap token stream */` |
|   140861 | 3418 | `	pTmp = pGen->pEnd;` |
|   140861 | 3419 | `	pGen->pEnd = pEnd;` |
|   140861 | 3420 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   140861 | 3421 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|   140861 | 3422 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 3423 | `	/* Emit the done instruction */` |
|   140861 | 3424 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   140861 | 3425 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - | 3426 | `	/* Update token stream */` |
|   140861 | 3427 | `	pGen->pIn  = pEnd;` |
|   140861 | 3428 | `	pGen->pEnd = pTmp;` |
|   140861 | 3429 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 3430 | `		return SXERR_ABORT;` |
|        - | 3431 | `	}` |
|   140861 | 3432 | `	return SXRET_OK;` |
|    70433 | 3433 | `}` |
|        - | 3434 | `/*` |
|        - | 3435 | ` * Compile the smart switch statement.` |
|        - | 3436 | ` * According to the PHP language reference manual` |
|        - | 3437 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|        - | 3438 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|        - | 3439 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|        - | 3440 | ` *  This is exactly what the switch statement is for.` |
|        - | 3441 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|        - | 3442 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|        - | 3443 | ` *  of the outer loop, use continue 2.` |
|        - | 3444 | ` *  Note that switch/case does loose comparision.` |
|        - | 3445 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|        - | 3446 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|        - | 3447 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|        - | 3448 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|        - | 3449 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|        - | 3450 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|        - | 3451 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|        - | 3452 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|        - | 3453 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|        - | 3454 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|        - | 3455 | ` *  list for the next case.` |
|        - | 3456 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|        - | 3457 | ` *  or floating-point numbers and strings.` |
|        - | 3458 | ` */` |
|    12452 | 3459 | `PH7_PRIVATE sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|        5 | 3460 | `{` |
|        - | 3461 | `	GenBlock *pSwitchBlock;` |
|        - | 3462 | `	SyToken *pTmp,*pEnd;` |
|        - | 3463 | `	ph7_switch *pSwitch;` |
|        - | 3464 | `	sxu32 nToken;` |
|        - | 3465 | `	sxu32 nLine;` |
|        - | 3466 | `	sxi32 rc;` |
|    12457 | 3467 | `	nLine = pGen->pIn->nLine;` |
|        - | 3468 | `	/* Jump the 'switch' keyword */` |
|    12457 | 3469 | `	pGen->pIn++;` |
|    12457 | 3470 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 3471 | `		/* Syntax error */` |
|      ! 0 | 3472 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|      ! 0 | 3473 | `		if( rc == SXERR_ABORT ){` |
|        - | 3474 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 3475 | `			return SXERR_ABORT;` |
|        - | 3476 | `		}` |
|      ! 0 | 3477 | `		goto Synchronize;` |
|        - | 3478 | `	}` |
|        - | 3479 | `	/* Jump the left parenthesis '(' */` |
|    12457 | 3480 | `	pGen->pIn++;` |
|    12457 | 3481 | `	pEnd = 0; /* cc warning */` |
|        - | 3482 | `	/* Create the loop block */` |
|    18683 | 3483 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|     6226 | 3484 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|    12457 | 3485 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3486 | `		return SXERR_ABORT;` |
|        - | 3487 | `	}` |
|        - | 3488 | `	/* Delimit the condition */` |
|    12457 | 3489 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    12457 | 3490 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - | 3491 | `		/* Empty expression */` |
|      ! 0 | 3492 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|      ! 0 | 3493 | `		if( rc == SXERR_ABORT ){` |
|        - | 3494 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 3495 | `			return SXERR_ABORT;` |
|        - | 3496 | `		}` |
|      ! 0 | 3497 | `	}` |
|        - | 3498 | `	/* Swap token streams */` |
|    12457 | 3499 | `	pTmp = pGen->pEnd;` |
|    12457 | 3500 | `	pGen->pEnd = pEnd;` |
|        - | 3501 | `	/* Compile the expression */` |
|    12457 | 3502 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    12457 | 3503 | `	if( rc == SXERR_ABORT ){` |
|        - | 3504 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 3505 | `		return SXERR_ABORT;` |
|        - | 3506 | `	}` |
|        - | 3507 | `	/* Update token stream */` |
|    12457 | 3508 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 | 3509 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 3510 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 3511 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3512 | `			return SXERR_ABORT;` |
|        - | 3513 | `		}` |
|      ! 0 | 3514 | `		pGen->pIn++;` |
|      ! 0 | 3515 | `	}` |
|    12457 | 3516 | `	pGen->pIn  = &pEnd[1];` |
|    12457 | 3517 | `	pGen->pEnd = pTmp;` |
|    12457 | 3518 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|    12452 | 3519 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|      ! 0 | 3520 | `			pTmp = pGen->pIn;` |
|      ! 0 | 3521 | `			if( pTmp >= pGen->pEnd ){` |
|      ! 0 | 3522 | `				pTmp--;` |
|      ! 0 | 3523 | `			}` |
|        - | 3524 | `			/* Unexpected token */` |
|      ! 0 | 3525 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|      ! 0 | 3526 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3527 | `				return SXERR_ABORT;` |
|        - | 3528 | `			}` |
|      ! 0 | 3529 | `			goto Synchronize;` |
|        - | 3530 | `	}` |
|        - | 3531 | `	/* Set the delimiter token */` |
|    12457 | 3532 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|        5 | 3533 | `		nToken = PH7_TK_KEYWORD;` |
|        - | 3534 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|        3 | 3535 | `	}else{` |
|    12453 | 3536 | `		nToken = PH7_TK_CCB; /* '}' */` |
|        - | 3537 | `	}` |
|    12457 | 3538 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|        - | 3539 | `	/* Create the switch blocks container */` |
|    12457 | 3540 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|    12457 | 3541 | `	if( pSwitch == 0 ){` |
|        - | 3542 | `		/* Abort compilation */` |
|      ! 0 | 3543 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 3544 | `		return SXERR_ABORT;` |
|        - | 3545 | `	}` |
|        - | 3546 | `	/* Zero the structure */` |
|    12457 | 3547 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|        - | 3548 | `	/* Initialize fields */` |
|    12457 | 3549 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|        - | 3550 | `	/* Emit the switch instruction */` |
|    12457 | 3551 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|        - | 3552 | `	/* Compile case blocks */` |
|   138796 | 3553 | `	for(;;){` |
|        - | 3554 | `		sxu32 nKwrd;` |
|   145027 | 3555 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 3556 | `			/* No more input to process */` |
|      ! 0 | 3557 | `			break;` |
|        - | 3558 | `		}` |
|   145027 | 3559 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 3560 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|        - | 3561 | `				/* Unexpected token */` |
|      ! 0 | 3562 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 3563 | `					&pGen->pIn->sData);` |
|      ! 0 | 3564 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3565 | `					return SXERR_ABORT;` |
|        - | 3566 | `				}` |
|        - | 3567 | `				/* FALL THROUGH */` |
|      ! 0 | 3568 | `			}` |
|        - | 3569 | `			/* Block compiled */` |
|      ! 0 | 3570 | `			break;` |
|        - | 3571 | `		}` |
|        - | 3572 | `		/* Extract the keyword */` |
|   145027 | 3573 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   145027 | 3574 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|        5 | 3575 | `			if( nToken != PH7_TK_KEYWORD ){` |
|        - | 3576 | `				/* Unexpected token */` |
|      ! 0 | 3577 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 3578 | `					&pGen->pIn->sData);` |
|      ! 0 | 3579 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3580 | `					return SXERR_ABORT;` |
|        - | 3581 | `				}` |
|        - | 3582 | `				/* FALL THROUGH */` |
|      ! 0 | 3583 | `			}` |
|        - | 3584 | `			/* Block compiled */` |
|        5 | 3585 | `			break;` |
|        - | 3586 | `		}` |
|   145023 | 3587 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|        - | 3588 | `			/*` |
|        - | 3589 | `			 * Accroding to the PHP language reference manual` |
|        - | 3590 | `			 *  A special case is the default case. This case matches anything` |
|        - | 3591 | `			 *  that wasn't matched by the other cases.` |
|        - | 3592 | `			 */` |
|     4167 | 3593 | `			if( pSwitch->nDefault > 0 ){` |
|        - | 3594 | `				/* Default case already compiled */` |
|      ! 0 | 3595 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|      ! 0 | 3596 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3597 | `					return SXERR_ABORT;` |
|        - | 3598 | `				}` |
|      ! 0 | 3599 | `			}` |
|     4167 | 3600 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|        - | 3601 | `			/* Compile the default block */` |
|     4167 | 3602 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|     4167 | 3603 | `			if( rc == SXERR_ABORT){` |
|      ! 0 | 3604 | `				return SXERR_ABORT;` |
|     4167 | 3605 | `			}else if( rc == SXERR_EOF ){` |
|     4165 | 3606 | `				break;` |
|        1 | 3607 | `			}` |
|   140862 | 3608 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|        - | 3609 | `			ph7_case_expr sCase;` |
|        - | 3610 | `			/* Standard case block */` |
|   140861 | 3611 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|        - | 3612 | `			/* initialize the structure */` |
|   140861 | 3613 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - | 3614 | `			/* Compile the case expression */` |
|   140861 | 3615 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|   140861 | 3616 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3617 | `				return SXERR_ABORT;` |
|        - | 3618 | `			}` |
|        - | 3619 | `			/* Compile the case block */` |
|   140861 | 3620 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|        - | 3621 | `			/* Insert in the switch container */` |
|   140861 | 3622 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|   140861 | 3623 | `			if( rc == SXERR_ABORT){` |
|      ! 0 | 3624 | `				return SXERR_ABORT;` |
|   140861 | 3625 | `			}else if( rc == SXERR_EOF ){` |
|     8293 | 3626 | `				break;` |
|        - | 3627 | `			}` |
|    66289 | 3628 | `		}else{` |
|        - | 3629 | `			/* Unexpected token */` |
|      ! 0 | 3630 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 3631 | `				&pGen->pIn->sData);` |
|      ! 0 | 3632 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3633 | `				return SXERR_ABORT;` |
|        - | 3634 | `			}` |
|      ! 0 | 3635 | `			break;` |
|        - | 3636 | `		}` |
|        5 | 3637 | `	}` |
|        - | 3638 | `	/* Fix all jumps now the destination is resolved */` |
|    12457 | 3639 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|    12457 | 3640 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 3641 | `	/* Release the loop block */` |
|    12457 | 3642 | `	GenStateLeaveBlock(pGen,0);` |
|    12457 | 3643 | `	if( pGen->pIn < pGen->pEnd ){` |
|        - | 3644 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|    12457 | 3645 | `		pGen->pIn++;` |
|     6226 | 3646 | `	}` |
|        - | 3647 | `	/* Statement successfully compiled */` |
|    12457 | 3648 | `	return SXRET_OK;` |
|      ! 0 | 3649 | `Synchronize:` |
|        - | 3650 | `	/* Synchronize with the first semi-colon */` |
|      ! 0 | 3651 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      ! 0 | 3652 | `		pGen->pIn++;` |
|      ! 0 | 3653 | `	}` |
|      ! 0 | 3654 | `	return SXRET_OK;` |
|     6231 | 3655 | `}` |
|        - | 3656 |  |
