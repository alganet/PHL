# src/ph7/compile_stmt.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1466/1993 lines (73.56%)

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
|       68 |   40 | `PH7_PRIVATE sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|        5 |   41 | `{` |
|        - |   42 | `	SySet *pConsCode,*pInstrContainer;` |
|       73 |   43 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|        - |   44 | `	SyString *pName;` |
|        - |   45 | `	sxi32 rc;` |
|       73 |   46 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       73 |   47 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |   48 | `		/* Invalid constant name */` |
|        8 |   49 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"identifier");` |
|        8 |   50 | `		if( rc == SXERR_ABORT ){` |
|        - |   51 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |   52 | `			return SXERR_ABORT;` |
|        - |   53 | `		}` |
|        8 |   54 | `		goto Synchronize;` |
|        - |   55 | `	}` |
|        - |   56 | `	/* Peek constant name */` |
|       66 |   57 | `	pName = &pGen->pIn->sData;` |
|        - |   58 | `	/* Make sure the constant name isn't reserved */` |
|       66 |   59 | `	if( GenStateIsReservedConstant(pName) ){` |
|        - |   60 | `		/* Reserved constant */` |
|        9 |   61 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Cannot redeclare constant '%z'",pName);` |
|        9 |   62 | `		if( rc == SXERR_ABORT ){` |
|        - |   63 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |   64 | `			return SXERR_ABORT;` |
|        - |   65 | `		}` |
|        9 |   66 | `		goto Synchronize;` |
|        - |   67 | `	}` |
|       57 |   68 | `	pGen->pIn++;` |
|       57 |   69 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|        - |   70 | `		/* Invalid statement*/` |
|        6 |   71 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"=\"");` |
|        6 |   72 | `		if( rc == SXERR_ABORT ){` |
|        - |   73 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |   74 | `			return SXERR_ABORT;` |
|        - |   75 | `		}` |
|        6 |   76 | `		goto Synchronize;` |
|        - |   77 | `	}` |
|       53 |   78 | `	pGen->pIn++; /*Jump the equal sign */` |
|        - |   79 | `	/* php: a constant expression may not CALL anything --` |
|        - |   80 | `	 * "Constant expression contains invalid operations". PHL used to evaluate the` |
|        - |   81 | ``	 * call happily, so `const X = strlen("ab");` defined X as 2. */`` |
|       53 |   82 | `	if( PH7_GenStateInitHasCallExpr(pGen) ){` |
|        3 |   83 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|        - |   84 | `			"Constant expression contains invalid operations");` |
|        3 |   85 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |   86 | `			return SXERR_ABORT;` |
|        - |   87 | `		}` |
|        3 |   88 | `		goto Synchronize;` |
|        - |   89 | `	}` |
|        - |   90 | `	/* Allocate a new constant value container */` |
|       51 |   91 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|       51 |   92 | `	if( pConsCode == 0 ){` |
|      ! 0 |   93 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |   94 | `		return SXERR_ABORT;` |
|        - |   95 | `	}` |
|       51 |   96 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - |   97 | `	/* Swap bytecode container */` |
|       51 |   98 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       51 |   99 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|        - |  100 | ``	/* Compile constant value. php: a stray token after `const X = EXPR` is`` |
|        - |  101 | ``	 * `... expecting "," or ";"` (const supports a comma-separated list). */`` |
|        - |  102 | `	{` |
|       51 |  103 | `		const char *zSaveConst = pGen->zClauseCloser;` |
|       51 |  104 | `		pGen->zClauseCloser = "\",\" or \";\"";` |
|       51 |  105 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       51 |  106 | `		pGen->zClauseCloser = zSaveConst;` |
|        - |  107 | `	}` |
|        - |  108 | `	/* Emit the done instruction */` |
|       51 |  109 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       51 |  110 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       51 |  111 | `	if( rc == SXERR_ABORT ){` |
|        - |  112 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  113 | `		return SXERR_ABORT;` |
|        - |  114 | `	}` |
|       51 |  115 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|        - |  116 | `	/* Register the constant with namespace-qualified name */` |
|        - |  117 | `	{` |
|        - |  118 | `		SyBlob sFQN;` |
|        - |  119 | `		SyString sFQNStr;` |
|       51 |  120 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       51 |  121 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|       51 |  122 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|       75 |  123 | `		rc = PH7_VmRegisterConstantEx(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode,` |
|       48 |  124 | `			(SyString *)SySetPeek(&pGen->pVm->aFiles),nLineLocal,1);` |
|       51 |  125 | `		if( rc == SXRET_OK && SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|        - |  126 | ``			/* php 8.5: attributes on `const` statements — attach the pending`` |
|        - |  127 | `			 * groups to the registered constant record for Reflection. */` |
|        7 |  128 | `			SyHashEntry *pCEntry = SyHashGet(&pGen->pVm->hConstant,` |
|        4 |  129 | `				SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|        5 |  130 | `			if( pCEntry ){` |
|        5 |  131 | `				ph7_constant *pRegCons = (ph7_constant *)pCEntry->pUserData;` |
|        5 |  132 | `				if( GenStateConsumeAttrs(&(*pGen),&pRegCons->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  133 | `					SyBlobRelease(&sFQN);` |
|      ! 0 |  134 | `					return SXERR_ABORT;` |
|        - |  135 | `				}` |
|        2 |  136 | `			}` |
|        2 |  137 | `		}` |
|       51 |  138 | `		SyBlobRelease(&sFQN);` |
|        - |  139 | `	}` |
|       51 |  140 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  141 | `		SySetRelease(pConsCode);` |
|      ! 0 |  142 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|      ! 0 |  143 | `	}` |
|       51 |  144 | `	return SXRET_OK;` |
|       10 |  145 | `Synchronize:` |
|        - |  146 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|       70 |  147 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       49 |  148 | `		pGen->pIn++;` |
|        3 |  149 | `	}` |
|       24 |  150 | `	return SXRET_OK;` |
|       39 |  151 | `}` |
|        - |  152 | `/*` |
|        - |  153 | ` * Compile the 'continue' statement.` |
|        - |  154 | ` * According to the PHP language reference` |
|        - |  155 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|        - |  156 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|        - |  157 | ` *  iteration.` |
|        - |  158 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|        - |  159 | ` *  the purposes of continue.` |
|        - |  160 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|        - |  161 | ` *  of enclosing loops it should skip to the end of.` |
|        - |  162 | ` *  Note:` |
|        - |  163 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|        - |  164 | ` */` |
|        - |  165 | `/*` |
|        - |  166 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|        - |  167 | ` * block and the target loop block. This ensures finally blocks run when` |
|        - |  168 | ` * break/continue crosses a try boundary.` |
|        - |  169 | ` *` |
|        - |  170 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|        - |  171 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|        - |  172 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|        - |  173 | ` */` |
|   177996 |  174 | `static int GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|        5 |  175 | `{` |
|   178001 |  176 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   178001 |  177 | `	int nInlineTry = 0;` |
|   719505 |  178 | `	while( pBlock && pBlock != pTarget ){` |
|   541509 |  179 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|        6 |  180 | `			if( pBlock->pUserData ){` |
|        - |  181 | `				/* A try block with an exception context. In a generator its catch/finally` |
|        - |  182 | `				 * are inlined: count it so the caller emits a single OP_SET_FINALLY_JMP that` |
|        - |  183 | `				 * runs each crossed finally (VmFinallyAdvance) before taking the loop jump.` |
|        - |  184 | `				 * Legacy path: emit POP_EXCEPTION per crossed try as before. */` |
|        6 |  185 | `				if( pGen->bInGenerator ){` |
|        3 |  186 | `					nInlineTry++;` |
|        2 |  187 | `				}else{` |
|        3 |  188 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|        - |  189 | `				}` |
|        4 |  190 | `			}else{` |
|        - |  191 | `				/* A catch/finally block compiled into a separate bytecode container` |
|        - |  192 | `				 * (legacy). Stop — cannot cross into the parent try from a sub-execution. */` |
|      ! 0 |  193 | `				break;` |
|        - |  194 | `			}` |
|        2 |  195 | `		}` |
|   541509 |  196 | `		pBlock = pBlock->pParent;` |
|        5 |  197 | `	}` |
|   178001 |  198 | `	return nInlineTry;` |
|        5 |  199 | `}` |
|    88970 |  200 | `PH7_PRIVATE sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|        5 |  201 | `{` |
|        - |  202 | `	GenBlock *pLoop; /* Target loop */` |
|        - |  203 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|        - |  204 | `	sxi32 iRawLevel; /* The level as WRITTEN, kept for php's diagnostics */` |
|        - |  205 | `	sxu32 nLineLocal;` |
|        - |  206 | `	sxi32 rc;` |
|    88975 |  207 | `	iRawLevel = 1;` |
|    88975 |  208 | `	nLineLocal = pGen->pIn->nLine;` |
|    88975 |  209 | `	iLevel = 0;` |
|        - |  210 | `	/* Jump the 'continue' keyword */` |
|    88975 |  211 | `	pGen->pIn++;` |
|    88975 |  212 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|        - |  213 | `		/* optional numeric argument which tells us how many levels` |
|        - |  214 | `		 * of enclosing loops we should skip to the end of.` |
|        - |  215 | `		 */` |
|        - |  216 | `		char zScratch[GEN_NUM_SCRATCH];` |
|       16 |  217 | `		char *zAlloc = 0;` |
|        - |  218 | `		SyString sNum;` |
|       16 |  219 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|       16 |  220 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  221 | `			return SXERR_ABORT;` |
|        - |  222 | `		}` |
|       16 |  223 | `		if( rc == SXRET_OK ){` |
|       20 |  224 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|       12 |  225 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|       14 |  226 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  227 | `				return SXERR_ABORT;` |
|        - |  228 | `			}` |
|       14 |  229 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|       14 |  230 | `			iRawLevel = iLevel;` |
|       14 |  231 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        6 |  232 | `		}` |
|       16 |  233 | `		if( iLevel < 2 ){` |
|        3 |  234 | `			iLevel = 0;` |
|        1 |  235 | `		}` |
|       16 |  236 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|        7 |  237 | `	}` |
|        - |  238 | `	/* php rejects a non-positive level outright, before asking where it lands. */` |
|    88975 |  239 | `	if( iRawLevel < 1 ){` |
|      ! 0 |  240 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|        - |  241 | `			"'continue' operator accepts only positive integers");` |
|      ! 0 |  242 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  243 | `			return SXERR_ABORT;` |
|        - |  244 | `		}` |
|      ! 0 |  245 | `		return SXRET_OK;` |
|        - |  246 | `	}` |
|        - |  247 | `	/* Point to the target loop */` |
|    88975 |  248 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    88975 |  249 | `	if( pLoop == 0 ){` |
|        - |  250 | ``		/* Same split php makes for `break`: no loop at all keeps the not-in-context`` |
|        - |  251 | ``		 * wording, too FEW loops is `Cannot 'continue' N levels`. */`` |
|       11 |  252 | `		if( GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,0) != 0 ){` |
|      ! 0 |  253 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|      ! 0 |  254 | `				"Cannot 'continue' %d level%s",iRawLevel,iRawLevel == 1 ? "" : "s");` |
|      ! 0 |  255 | `		}else{` |
|       11 |  256 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"'continue' not in the 'loop' or 'switch' context");` |
|        - |  257 | `		}` |
|       11 |  258 | `		if( rc == SXERR_ABORT ){` |
|        - |  259 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  260 | `			return SXERR_ABORT;` |
|        - |  261 | `		}` |
|        6 |  262 | `	}else{` |
|    88965 |  263 | `		sxu32 nInstrIdx = 0;` |
|        - |  264 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|    88965 |  265 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|        - |  266 | `		/* ROOT C: in a generator, a break/continue crossing inline trys must run their` |
|        - |  267 | `		 * finallys first. OP_SET_FINALLY_JMP(iP1=count) does that then takes the loop jump. */` |
|    88965 |  268 | `		sxi32 iJmpOp = nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP;` |
|    88965 |  269 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|        - |  270 | ``			/* `continue` inside a switch acts like `break` — which is almost never what`` |
|        - |  271 | `			 * the author meant, so php 7.3+ says so at compile time. The generated jump` |
|        - |  272 | `			 * is unchanged; only the diagnostic was missing. An explicit level` |
|        - |  273 | ``			 * (`continue 2`) targets the enclosing loop and stays silent. */`` |
|        5 |  274 | `			if( iLevel < 1 ){` |
|        5 |  275 | `				PH7_GenCompileError(&(*pGen),E_WARNING,nLineLocal,` |
|        - |  276 | `					"\"continue\" targeting switch is equivalent to \"break\"."` |
|        - |  277 | `					" Did you mean to use \"continue 2\"?");` |
|        2 |  278 | `			}` |
|        5 |  279 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,0,0,&nInstrIdx);` |
|        5 |  280 | `			if( rc == SXRET_OK ){` |
|        5 |  281 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|        2 |  282 | `			}` |
|        3 |  283 | `		}else{` |
|        - |  284 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    88961 |  285 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    88961 |  286 | `			if( pLoop->bPostContinue == TRUE ){` |
|        - |  287 | `				JumpFixup sJumpFix;` |
|        - |  288 | `				/* Post-continue */` |
|    27079 |  289 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|    27079 |  290 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|    27079 |  291 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|    13537 |  292 | `			}` |
|        - |  293 | `		}` |
|        - |  294 | `	}` |
|    88975 |  295 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  296 | `		/* Not so fatal,emit a warning only */` |
|      ! 0 |  297 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|      ! 0 |  298 | `	}` |
|        - |  299 | `	/* Statement successfully compiled */` |
|    88975 |  300 | `	return SXRET_OK;` |
|    44490 |  301 | `}` |
|        - |  302 | `/*` |
|        - |  303 | ` * Compile the 'break' statement.` |
|        - |  304 | ` * According to the PHP language reference` |
|        - |  305 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|        - |  306 | ` *  structure.` |
|        - |  307 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|        - |  308 | ` *  enclosing structures are to be broken out of.` |
|        - |  309 | ` */` |
|    89052 |  310 | `PH7_PRIVATE sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|        5 |  311 | `{` |
|        - |  312 | `	GenBlock *pLoop; /* Target loop */` |
|        - |  313 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|        - |  314 | `	sxi32 iRawLevel; /* The level as WRITTEN, kept for php's diagnostics */` |
|        - |  315 | `	sxi32 rc;` |
|    89057 |  316 | `	iLevel = 0;` |
|    89057 |  317 | `	iRawLevel = 1;` |
|        - |  318 | `	/* Jump the 'break' keyword */` |
|    89057 |  319 | `	pGen->pIn++;` |
|    89057 |  320 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|        - |  321 | `		/* optional numeric argument which tells us how many levels` |
|        - |  322 | `		 * of enclosing loops we should skip to the end of.` |
|        - |  323 | `		 */` |
|        - |  324 | `		char zScratch[GEN_NUM_SCRATCH];` |
|       17 |  325 | `		char *zAlloc = 0;` |
|        - |  326 | `		SyString sNum;` |
|       17 |  327 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|       17 |  328 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  329 | `			return SXERR_ABORT;` |
|        - |  330 | `		}` |
|       17 |  331 | `		if( rc == SXRET_OK ){` |
|       20 |  332 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|       12 |  333 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|       14 |  334 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  335 | `				return SXERR_ABORT;` |
|        - |  336 | `			}` |
|       14 |  337 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|       14 |  338 | `			iRawLevel = iLevel;` |
|       14 |  339 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        6 |  340 | `		}` |
|       17 |  341 | `		if( iLevel < 2 ){` |
|        3 |  342 | `			iLevel = 0;` |
|        1 |  343 | `		}` |
|       17 |  344 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|        7 |  345 | `	}` |
|        - |  346 | `	/* php rejects a non-positive level outright, before asking where it lands. */` |
|    89057 |  347 | `	if( iRawLevel < 1 ){` |
|      ! 0 |  348 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  349 | `			"'break' operator accepts only positive integers");` |
|      ! 0 |  350 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  351 | `			return SXERR_ABORT;` |
|        - |  352 | `		}` |
|      ! 0 |  353 | `		goto BreakLevelDone;` |
|        - |  354 | `	}` |
|        - |  355 | `	/* Extract the target loop */` |
|    89057 |  356 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|   133583 |  357 | `	if( pLoop == 0 ){` |
|        - |  358 | `		/* php distinguishes "there is no loop at all here" from "there is one, but` |
|        - |  359 | `		 * not N of them": the first keeps the not-in-context wording, the second is` |
|        - |  360 | ``		 * `Cannot 'break' N levels`. */`` |
|       19 |  361 | `		if( GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,0) != 0 ){` |
|        4 |  362 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        1 |  363 | `				"Cannot 'break' %d level%s",iRawLevel,iRawLevel == 1 ? "" : "s");` |
|        2 |  364 | `		}else{` |
|       17 |  365 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"'break' not in the 'loop' or 'switch' context");` |
|        - |  366 | `		}` |
|       19 |  367 | `		if( rc == SXERR_ABORT ){` |
|        - |  368 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  369 | `			return SXERR_ABORT;` |
|        - |  370 | `		}` |
|       11 |  371 | `	}else{` |
|        - |  372 | `		sxu32 nInstrIdx;` |
|        - |  373 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|    89041 |  374 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|        - |  375 | `		/* ROOT C: OP_SET_FINALLY_JMP runs the crossed inline finallys before the break jump. */` |
|    89041 |  376 | `		rc = PH7_VmEmitInstr(pGen->pVm,nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP,nCross,0,0,&nInstrIdx);` |
|    89041 |  377 | `		if( rc == SXRET_OK ){` |
|        - |  378 | `			/* Fix the jump later when the jump destination is resolved */` |
|    89041 |  379 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|    44518 |  380 | `		}` |
|        - |  381 | `	}` |
|    44526 |  382 | `BreakLevelDone:` |
|    89057 |  383 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  384 | `		/* Not so fatal,emit a warning only */` |
|      ! 0 |  385 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|      ! 0 |  386 | `	}` |
|        - |  387 | `	/* Statement successfully compiled */` |
|    89057 |  388 | `	return SXRET_OK;` |
|    44531 |  389 | `}` |
|        - |  390 | `/*` |
|        - |  391 | ` * Compile or record a label.` |
|        - |  392 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|        - |  393 | ` * Example` |
|        - |  394 | ` *  goto LABEL;` |
|        - |  395 | ` *   echo 'Foo';` |
|        - |  396 | ` *  LABEL:` |
|        - |  397 | ` *   echo 'Bar';` |
|        - |  398 | ` */` |
|      112 |  399 | `PH7_PRIVATE sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|        5 |  400 | `{` |
|        - |  401 | `	GenBlock *pBlock;` |
|        - |  402 | `	Label sLabel;` |
|        - |  403 | `	/* php places NO restriction on where a label may be DEFINED — inside a loop, a switch` |
|        - |  404 | `	 * or a try{} is all fine. The only rule is on the jump: you may not goto INTO a loop` |
|        - |  405 | `	 * or switch from outside it, which is checked once the labels are all known (see` |
|        - |  406 | `	 * GenStateFixJumps). Record the loop this label sits in so that check can run. */` |
|        - |  407 | `	{` |
|      117 |  408 | `		SyString *pTarget = &pGen->pIn->sData;` |
|        - |  409 | `		char *zDup;` |
|        - |  410 | `		/* Initialize label fields */` |
|      117 |  411 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|        - |  412 | `		/* Duplicate label name */` |
|      117 |  413 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|      117 |  414 | `		if( zDup == 0 ){` |
|      ! 0 |  415 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  416 | `			return SXERR_ABORT;` |
|        - |  417 | `		}` |
|      117 |  418 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|      117 |  419 | `		sLabel.bRef  = FALSE;` |
|      117 |  420 | `		sLabel.nLine = pGen->pIn->nLine;` |
|      117 |  421 | `		sLabel.nLoopId = pGen->nCurLoopId;` |
|      117 |  422 | `		pBlock = pGen->pCurrent;` |
|      233 |  423 | `		while( pBlock ){` |
|      143 |  424 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|       26 |  425 | `				break;` |
|        - |  426 | `			}` |
|        - |  427 | `			/* Point to the upper block */` |
|      121 |  428 | `			pBlock = pBlock->pParent;` |
|        5 |  429 | `		}` |
|      117 |  430 | `		if( pBlock ){` |
|       26 |  431 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       15 |  432 | `		}else{` |
|       95 |  433 | `			sLabel.pFunc = 0;` |
|        - |  434 | `		}` |
|        - |  435 | `		/* Insert in label set */` |
|      117 |  436 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|        - |  437 | `	}` |
|      117 |  438 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|      117 |  439 | `	return SXRET_OK;` |
|       61 |  440 | `}` |
|        - |  441 | `/*` |
|        - |  442 | ` * Compile the so hated 'goto' statement.` |
|        - |  443 | ` * You've probably been taught that gotos are bad, but this sort` |
|        - |  444 | ` * of rewriting  happens all the time, in fact every time you run` |
|        - |  445 | ` * a compiler it has to do this.` |
|        - |  446 | ` * According to the PHP language reference manual` |
|        - |  447 | ` *   The goto operator can be used to jump to another section in the program.` |
|        - |  448 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|        - |  449 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|        - |  450 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|        - |  451 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|        - |  452 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|        - |  453 | ` *   of a multi-level break` |
|        - |  454 | ` */` |
|      152 |  455 | `PH7_PRIVATE sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|        5 |  456 | `{` |
|        - |  457 | `	JumpFixup sJump;` |
|        - |  458 | `	sxi32 rc;` |
|      157 |  459 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|      157 |  460 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - |  461 | `		/* Missing label */` |
|      ! 0 |  462 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|      ! 0 |  463 | `		if( rc == SXERR_ABORT ){` |
|        - |  464 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  465 | `			return SXERR_ABORT;` |
|        - |  466 | `		}` |
|      ! 0 |  467 | `		return SXRET_OK;` |
|        - |  468 | `	}` |
|      157 |  469 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|        6 |  470 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn,"identifier");` |
|        6 |  471 | `		if( rc == SXERR_ABORT ){` |
|        - |  472 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  473 | `			return SXERR_ABORT;` |
|        - |  474 | `		}` |
|        4 |  475 | `	}else{` |
|      153 |  476 | `		SyString *pTarget = &pGen->pIn->sData;` |
|        - |  477 | `		GenBlock *pBlock;` |
|        - |  478 | `		char *zDup;` |
|        - |  479 | `		/* Prepare the jump destination */` |
|      153 |  480 | `		sJump.nJumpType = PH7_OP_JMP;` |
|      153 |  481 | `		sJump.nLine = pGen->pIn->nLine;` |
|        - |  482 | `		/* Duplicate label name */` |
|      153 |  483 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|      153 |  484 | `		if( zDup == 0 ){` |
|      ! 0 |  485 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  486 | `			return SXERR_ABORT;` |
|        - |  487 | `		}` |
|      153 |  488 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|        - |  489 | `		/* The loop/switch this goto sits in, for the "goto into a loop" check later. */` |
|      153 |  490 | `		sJump.nLoopId = pGen->nCurLoopId;` |
|        - |  491 | `		/* A goto inside a try{}/catch{} is legal php (jumping OUT of the block is fine);` |
|        - |  492 | `		 * only the owning function matters here, since a goto may not cross functions. */` |
|      153 |  493 | `		pBlock = pGen->pCurrent;` |
|      327 |  494 | `		while( pBlock ){` |
|      205 |  495 | `			if( pBlock->iFlags & GEN_BLOCK_FUNC ){` |
|       31 |  496 | `				break;` |
|        - |  497 | `			}` |
|        - |  498 | `			/* Point to the upper block */` |
|      179 |  499 | `			pBlock = pBlock->pParent;` |
|        5 |  500 | `		}` |
|      153 |  501 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|       31 |  502 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       18 |  503 | `		}else{` |
|      127 |  504 | `			sJump.pFunc = 0;` |
|        - |  505 | `		}` |
|        - |  506 | `		/* Emit the unconditional jump */` |
|      153 |  507 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|      153 |  508 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|       74 |  509 | `		}` |
|        - |  510 | `	}` |
|      157 |  511 | `	pGen->pIn++; /* Jump the label name */` |
|      157 |  512 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        3 |  513 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|        1 |  514 | `	}` |
|        - |  515 | `	/* Statement successfully compiled */` |
|      157 |  516 | `	return SXRET_OK;` |
|       81 |  517 | `}` |
|        - |  518 | `/*` |
|        - |  519 | ` * Point to the next PHP chunk that will be processed shortly.` |
|        - |  520 | ` * Return SXRET_OK on success. Any other return value indicates` |
|        - |  521 | ` * failure.` |
|        - |  522 | ` */` |
|       20 |  523 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|        2 |  524 | `{` |
|        - |  525 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|        - |  526 | `	sxu32 nRawObj;` |
|       10 |  527 | `	sxu32 nObjIdx;` |
|        - |  528 | `	/* Consume raw chunks verbatim without any processing until we get` |
|        - |  529 | `	 * a PHP block.` |
|        - |  530 | `	 */` |
|       10 |  531 | `Consume:` |
|       22 |  532 | `	nRawObj = nObjIdx = 0;` |
|       22 |  533 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|      ! 0 |  534 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|      ! 0 |  535 | `		if( pRawObj == 0 ){` |
|      ! 0 |  536 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  537 | `			return SXERR_ABORT;` |
|        - |  538 | `		}` |
|        - |  539 | `		/* Mark as constant and emit the load constant instruction */` |
|      ! 0 |  540 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|      ! 0 |  541 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|      ! 0 |  542 | `		++nRawObj;` |
|      ! 0 |  543 | `		pGen->pRawIn++; /* Next chunk */` |
|      ! 0 |  544 | `	}` |
|       22 |  545 | `	if( nRawObj > 0 ){` |
|        - |  546 | `		/* Emit the consume instruction */` |
|      ! 0 |  547 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|      ! 0 |  548 | `	}` |
|       22 |  549 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|      ! 0 |  550 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|        - |  551 | `		/* Reset the token set (and its trivia sidecar) */` |
|      ! 0 |  552 | `		SySetReset(pTokenSet);` |
|      ! 0 |  553 | `		SySetReset(&pGen->aTrivia);` |
|        - |  554 | `		/* Tokenize input */` |
|      ! 0 |  555 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|      ! 0 |  556 | `			pGen->pRawIn->nLine,pTokenSet,&pGen->aTrivia);` |
|        - |  557 | `		/* Point to the fresh token stream */` |
|      ! 0 |  558 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|      ! 0 |  559 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|        - |  560 | `		/* Advance the stream cursor */` |
|      ! 0 |  561 | `		pGen->pRawIn++;` |
|        - |  562 | `		/* TICKET 1433-011 */` |
|      ! 0 |  563 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|        - |  564 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|        - |  565 | `			sxi32 rc;` |
|        - |  566 | `			/* Refer to TICKET 1433-009  */` |
|      ! 0 |  567 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|      ! 0 |  568 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|      ! 0 |  569 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|        - |  570 | `			/* Synthesized short-tag echo: legitimately an expression here. */` |
|      ! 0 |  571 | `			pGen->nExprEchoOk++;` |
|      ! 0 |  572 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|      ! 0 |  573 | `			pGen->nExprEchoOk--;` |
|      ! 0 |  574 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  575 | `				return SXERR_ABORT;` |
|      ! 0 |  576 | `			}else if( rc != SXERR_EMPTY ){` |
|      ! 0 |  577 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      ! 0 |  578 | `			}` |
|      ! 0 |  579 | `			goto Consume;` |
|        - |  580 | `		}` |
|      ! 0 |  581 | `	}else{` |
|        - |  582 | `		/* No more chunks to process */` |
|       22 |  583 | `		pGen->pIn = pGen->pEnd;` |
|       22 |  584 | `		return SXERR_EOF;` |
|        - |  585 | `	}` |
|      ! 0 |  586 | `	return SXRET_OK;` |
|       12 |  587 | `}` |
|        - |  588 | `/*` |
|        - |  589 | ` * Compile a PHP block.` |
|        - |  590 | ` * A block is simply one or more PHP statements and expressions to compile` |
|        - |  591 | ` * optionally delimited by braces {}.` |
|        - |  592 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|        - |  593 | ` * and this function takes care of generating the appropriate error` |
|        - |  594 | ` * message.` |
|        - |  595 | ` */` |
|  6896476 |  596 | `PH7_PRIVATE sxi32 PH7_CompileBlock(` |
|        - |  597 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |  598 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|        - |  599 | `	)` |
|        5 |  600 | `{` |
|        - |  601 | `	sxi32 rc;` |
|        - |  602 | `	sxu32 nLine;` |
|  6896481 |  603 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  6771867 |  604 | `		nLine = pGen->pIn->nLine;` |
|  6771867 |  605 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  6771867 |  606 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  607 | `			return SXERR_ABORT;` |
|        - |  608 | `		}` |
|  6771867 |  609 | `		pGen->pIn++;` |
|        - |  610 | `		/* Compile until we hit the closing braces '}' */` |
|  9908545 |  611 | `		for(;;){` |
| 19817095 |  612 | `			if( pGen->pIn >= pGen->pEnd ){` |
|       22 |  613 | `				rc = GenStateNextChunk(&(*pGen));` |
|       22 |  614 | `				if (rc == SXERR_ABORT ){` |
|      ! 0 |  615 | `			 	   return SXERR_ABORT;` |
|        - |  616 | `				}` |
|       22 |  617 | `				if( rc == SXERR_EOF ){` |
|        - |  618 | `					/* No more token to process: the block was never closed. php reports` |
|        - |  619 | `					 * the line the '{' was opened on, not where the input ran out. */` |
|       22 |  620 | `					PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"Unclosed '{' on line %u",nLine);` |
|       22 |  621 | `					break;` |
|        - |  622 | `				}` |
|      ! 0 |  623 | `			}` |
| 19817075 |  624 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|        - |  625 | `				/* Closing braces found,break immediately*/` |
|  6771847 |  626 | `				pGen->pIn++;` |
|  6771847 |  627 | `				break;` |
|        - |  628 | `			}` |
|        - |  629 | `			/* Compile a single statement */` |
| 13045233 |  630 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
| 13045233 |  631 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  632 | `				return SXERR_ABORT;` |
|        - |  633 | `			}` |
|        5 |  634 | `		}` |
|  6771867 |  635 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  3510550 |  636 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|      ! 0 |  637 | `		pGen->pIn++;` |
|      ! 0 |  638 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|      ! 0 |  639 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  640 | `			return SXERR_ABORT;` |
|        - |  641 | `		}` |
|        - |  642 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|      ! 0 |  643 | `		for(;;){` |
|      ! 0 |  644 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  645 | `				rc = GenStateNextChunk(&(*pGen));` |
|      ! 0 |  646 | `				if (rc == SXERR_ABORT ){` |
|      ! 0 |  647 | `			 	   return SXERR_ABORT;` |
|        - |  648 | `				}` |
|      ! 0 |  649 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|        - |  650 | `					/* No more token to process */` |
|      ! 0 |  651 | `					if( rc == SXERR_EOF ){` |
|      ! 0 |  652 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|        - |  653 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|      ! 0 |  654 | `					}` |
|      ! 0 |  655 | `					break;` |
|        - |  656 | `				}` |
|      ! 0 |  657 | `			}` |
|      ! 0 |  658 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|        - |  659 | `				sxi32 nKwrd;` |
|        - |  660 | `				/* Keyword found */` |
|      ! 0 |  661 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      ! 0 |  662 | `				if( nKwrd == nKeywordEnd \|\|` |
|      ! 0 |  663 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|        - |  664 | `						/* Delimiter keyword found,break */` |
|      ! 0 |  665 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|      ! 0 |  666 | `							pGen->pIn++; /*  endif;endswitch... */` |
|      ! 0 |  667 | `						}` |
|      ! 0 |  668 | `						break;` |
|        - |  669 | `				}` |
|      ! 0 |  670 | `			}` |
|        - |  671 | `			/* Compile a single statement */` |
|      ! 0 |  672 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|      ! 0 |  673 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  674 | `				return SXERR_ABORT;` |
|        - |  675 | `			}` |
|      ! 0 |  676 | `		}` |
|      ! 0 |  677 | `		GenStateLeaveBlock(&(*pGen),0);` |
|      ! 0 |  678 | `	}else{` |
|        - |  679 | `		/* Compile a single statement */` |
|   124619 |  680 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|   124619 |  681 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  682 | `			return SXERR_ABORT;` |
|        - |  683 | `		}` |
|        - |  684 | `	}` |
|        - |  685 | `	/* Jump trailing semi-colons ';' */` |
|  6896481 |  686 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      ! 0 |  687 | `		pGen->pIn++;` |
|      ! 0 |  688 | `	}` |
|  6896481 |  689 | `	return SXRET_OK;` |
|  3448243 |  690 | `}` |
|        - |  691 | `/*` |
|        - |  692 | ` * Compile the gentle 'while' statement.` |
|        - |  693 | ` * According to the PHP language reference` |
|        - |  694 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|        - |  695 | ` *  The basic form of a while statement is:` |
|        - |  696 | ` *  while (expr)` |
|        - |  697 | ` *   statement` |
|        - |  698 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|        - |  699 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|        - |  700 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|        - |  701 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|        - |  702 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|        - |  703 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|        - |  704 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|        - |  705 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|        - |  706 | ` *  while (expr):` |
|        - |  707 | ` *    statement` |
|        - |  708 | ` *   endwhile;` |
|        - |  709 | ` */` |
|    73598 |  710 | `PH7_PRIVATE sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|        5 |  711 | `{` |
|    73603 |  712 | `	GenBlock *pWhileBlock = 0;` |
|    73603 |  713 | `	SyToken *pTmp,*pEnd = 0;` |
|        - |  714 | `	sxu32 nFalseJump;` |
|        - |  715 | `	sxu32 nLine;` |
|        - |  716 | `	sxi32 rc;` |
|    73603 |  717 | `	nLine = pGen->pIn->nLine;` |
|        - |  718 | `	/* Jump the 'while' keyword */` |
|    73603 |  719 | `	pGen->pIn++;` |
|    73603 |  720 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  721 | `		/* Syntax error */` |
|      ! 0 |  722 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|      ! 0 |  723 | `		if( rc == SXERR_ABORT ){` |
|        - |  724 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  725 | `			return SXERR_ABORT;` |
|        - |  726 | `		}` |
|      ! 0 |  727 | `		goto Synchronize;` |
|        - |  728 | `	}` |
|        - |  729 | `	/* Jump the left parenthesis '(' */` |
|    73603 |  730 | `	pGen->pIn++;` |
|        - |  731 | `	/* Create the loop block */` |
|    73603 |  732 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|    73603 |  733 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  734 | `		return SXERR_ABORT;` |
|        - |  735 | `	}` |
|        - |  736 | `	/* Delimit the condition */` |
|    73603 |  737 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    73603 |  738 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  739 | `		/* Empty condition. php reports the token that actually stopped it -- for` |
|        - |  740 | ``		 * `while ()` that is the ')' -- not a hand-written "expected expression". */`` |
|        3 |  741 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,0);` |
|        3 |  742 | `		if( rc == SXERR_ABORT ){` |
|        - |  743 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  744 | `			return SXERR_ABORT;` |
|        - |  745 | `		}` |
|        1 |  746 | `	}` |
|        - |  747 | `	/* Swap token streams */` |
|    73603 |  748 | `	pTmp = pGen->pEnd;` |
|    73603 |  749 | `	pGen->pEnd = pEnd;` |
|        - |  750 | `	/* Compile the expression */` |
|    73603 |  751 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    73603 |  752 | `	if( rc == SXERR_ABORT ){` |
|        - |  753 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  754 | `		return SXERR_ABORT;` |
|        - |  755 | `	}` |
|        - |  756 | `	/* Update token stream */` |
|    73603 |  757 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 |  758 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 |  759 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  760 | `			return SXERR_ABORT;` |
|        - |  761 | `		}` |
|      ! 0 |  762 | `		pGen->pIn++;` |
|      ! 0 |  763 | `	}` |
|        - |  764 | `	/* Synchronize pointers */` |
|    73603 |  765 | `	pGen->pIn  = &pEnd[1];` |
|    73603 |  766 | `	pGen->pEnd = pTmp;` |
|        - |  767 | `	/* Emit the false jump */` |
|    73603 |  768 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|        - |  769 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    73603 |  770 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|        - |  771 | `	/* Compile the loop body */` |
|    73603 |  772 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|    73603 |  773 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  774 | `		return SXERR_ABORT;` |
|        - |  775 | `	}` |
|        - |  776 | `	/* Emit the unconditional jump to the start of the loop */` |
|    73603 |  777 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|        - |  778 | `	/* Fix all jumps now the destination is resolved */` |
|    73603 |  779 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  780 | `	/* Release the loop block */` |
|    73603 |  781 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  782 | `	/* Statement successfully compiled */` |
|    73603 |  783 | `	return SXRET_OK;` |
|      ! 0 |  784 | `Synchronize:` |
|        - |  785 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - |  786 | `	 * compiling this erroneous block.` |
|        - |  787 | `	 */` |
|      ! 0 |  788 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  789 | `		pGen->pIn++;` |
|      ! 0 |  790 | `	}` |
|      ! 0 |  791 | `	return SXRET_OK;` |
|    36804 |  792 | `}` |
|        - |  793 | `/*` |
|        - |  794 | ` * Compile the ugly do..while() statement.` |
|        - |  795 | ` * According to the PHP language reference` |
|        - |  796 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|        - |  797 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|        - |  798 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|        - |  799 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|        - |  800 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|        - |  801 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|        - |  802 | ` *  would end immediately).` |
|        - |  803 | ` *  There is just one syntax for do-while loops:` |
|        - |  804 | ` *  <?php` |
|        - |  805 | ` *  $i = 0;` |
|        - |  806 | ` *  do {` |
|        - |  807 | ` *   echo $i;` |
|        - |  808 | ` *  } while ($i > 0);` |
|        - |  809 | ` * ?>` |
|        - |  810 | ` */` |
|        2 |  811 | `PH7_PRIVATE sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|        1 |  812 | `{` |
|        3 |  813 | `	SyToken *pTmp,*pEnd = 0;` |
|        3 |  814 | `	GenBlock *pDoBlock = 0;` |
|        - |  815 | `	sxu32 nLine;` |
|        - |  816 | `	sxi32 rc;` |
|        3 |  817 | `	nLine = pGen->pIn->nLine;` |
|        - |  818 | `	/* Jump the 'do' keyword */` |
|        3 |  819 | `	pGen->pIn++;` |
|        - |  820 | `	/* Create the loop block */` |
|        3 |  821 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|        3 |  822 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  823 | `		return SXERR_ABORT;` |
|        - |  824 | `	}` |
|        - |  825 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|        3 |  826 | `	pDoBlock->bPostContinue = TRUE;` |
|        3 |  827 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|        3 |  828 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  829 | `		return SXERR_ABORT;` |
|        - |  830 | `	}` |
|        3 |  831 | `	if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 |  832 | `		nLine = pGen->pIn->nLine;` |
|      ! 0 |  833 | `	}` |
|        3 |  834 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|      ! 0 |  835 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|        - |  836 | `			/* Missing 'while' statement */` |
|        - |  837 | ``			/* php: `do {} ;` names the ';', while `do {}` at EOF is "unexpected end of`` |
|        - |  838 | `			 * file". The do-block's own slice stops before its terminator, so look at` |
|        - |  839 | `			 * the whole CHUNK stream: a token still there is the one php names; nothing` |
|        - |  840 | `			 * left means end of file (NULL). */` |
|        - |  841 | `			{` |
|        3 |  842 | `				SyToken *pBad = pGen->pIn < pGen->pEnd ? pGen->pIn : 0;` |
|        3 |  843 | `				if( pBad == 0 && pGen->pTokenSet ){` |
|        - |  844 | `					/* The do-block consumed its terminator, so the token php names is` |
|        - |  845 | `					 * the one just behind the cursor -- but only when it is a real` |
|        - |  846 | ``					 * terminator (`do {} ;` -> the ';'). If the block simply ended on`` |
|        - |  847 | `					 * its '}' with nothing after it, php reports end of file. */` |
|        3 |  848 | `					SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        3 |  849 | `					if( pGen->pIn > pBase && (pGen->pIn[-1].nType & PH7_TK_SEMI) ){` |
|      ! 0 |  850 | `						pBad = &pGen->pIn[-1];` |
|      ! 0 |  851 | `					}` |
|        1 |  852 | `				}` |
|        3 |  853 | `				rc = PH7_GenSyntaxError(pGen,pBad,"\"while\"");` |
|        - |  854 | `			}` |
|        3 |  855 | `			if( rc == SXERR_ABORT ){` |
|        - |  856 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  857 | `				return SXERR_ABORT;` |
|        - |  858 | `			}` |
|        3 |  859 | `			goto Synchronize;` |
|        - |  860 | `	}` |
|        - |  861 | `	/* Jump the 'while' keyword */` |
|      ! 0 |  862 | `	pGen->pIn++;` |
|      ! 0 |  863 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  864 | `		/* Syntax error */` |
|      ! 0 |  865 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|      ! 0 |  866 | `		if( rc == SXERR_ABORT ){` |
|        - |  867 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  868 | `			return SXERR_ABORT;` |
|        - |  869 | `		}` |
|      ! 0 |  870 | `		goto Synchronize;` |
|        - |  871 | `	}` |
|        - |  872 | `	/* Jump the left parenthesis '(' */` |
|      ! 0 |  873 | `	pGen->pIn++;` |
|        - |  874 | `	/* Delimit the condition */` |
|      ! 0 |  875 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      ! 0 |  876 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  877 | `		/* Empty condition. php reports the token that actually stopped it -- for` |
|        - |  878 | ``		 * `while ()` that is the ')' -- not a hand-written "expected expression". */`` |
|      ! 0 |  879 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,0);` |
|      ! 0 |  880 | `		if( rc == SXERR_ABORT ){` |
|        - |  881 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  882 | `			return SXERR_ABORT;` |
|        - |  883 | `		}` |
|      ! 0 |  884 | `		goto Synchronize;` |
|        - |  885 | `	}` |
|        - |  886 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|      ! 0 |  887 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|        - |  888 | `		JumpFixup *aPost;` |
|        - |  889 | `		VmInstr *pInstr;` |
|        - |  890 | `		sxu32 nJumpDest;` |
|        - |  891 | `		sxu32 n;` |
|      ! 0 |  892 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|      ! 0 |  893 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      ! 0 |  894 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|      ! 0 |  895 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      ! 0 |  896 | `			if( pInstr ){` |
|        - |  897 | `				/* Fix */` |
|      ! 0 |  898 | `				pInstr->iP2 = nJumpDest;` |
|      ! 0 |  899 | `			}` |
|      ! 0 |  900 | `		}` |
|      ! 0 |  901 | `	}` |
|        - |  902 | `	/* Swap token streams */` |
|      ! 0 |  903 | `	pTmp = pGen->pEnd;` |
|      ! 0 |  904 | `	pGen->pEnd = pEnd;` |
|        - |  905 | `	/* Compile the expression */` |
|      ! 0 |  906 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      ! 0 |  907 | `	if( rc == SXERR_ABORT ){` |
|        - |  908 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  909 | `		return SXERR_ABORT;` |
|        - |  910 | `	}` |
|        - |  911 | `	/* Update token stream */` |
|      ! 0 |  912 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 |  913 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 |  914 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  915 | `			return SXERR_ABORT;` |
|        - |  916 | `		}` |
|      ! 0 |  917 | `		pGen->pIn++;` |
|      ! 0 |  918 | `	}` |
|      ! 0 |  919 | `	pGen->pIn  = &pEnd[1];` |
|      ! 0 |  920 | `	pGen->pEnd = pTmp;` |
|        - |  921 | `	/* Emit the true jump to the beginning of the loop */` |
|      ! 0 |  922 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|        - |  923 | `	/* Fix all jumps now the destination is resolved */` |
|      ! 0 |  924 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  925 | `	/* Release the loop block */` |
|      ! 0 |  926 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  927 | `	/* Statement successfully compiled */` |
|      ! 0 |  928 | `	return SXRET_OK;` |
|        1 |  929 | `Synchronize:` |
|        - |  930 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - |  931 | `	 * compiling this erroneous block.` |
|        - |  932 | `	 */` |
|        3 |  933 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  934 | `		pGen->pIn++;` |
|      ! 0 |  935 | `	}` |
|        3 |  936 | `	return SXRET_OK;` |
|        2 |  937 | `}` |
|        - |  938 | `/*` |
|        - |  939 | ` * Compile the complex and powerful 'for' statement.` |
|        - |  940 | ` * According to the PHP language reference` |
|        - |  941 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|        - |  942 | ` *  The syntax of a for loop is:` |
|        - |  943 | ` *  for (expr1; expr2; expr3)` |
|        - |  944 | ` *   statement` |
|        - |  945 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|        - |  946 | ` *  the beginning of the loop.` |
|        - |  947 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|        - |  948 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|        - |  949 | ` *  to FALSE, the execution of the loop ends.` |
|        - |  950 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|        - |  951 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|        - |  952 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|        - |  953 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|        - |  954 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|        - |  955 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|        - |  956 | ` *  of using the for truth expression.` |
|        - |  957 | ` */` |
|   127742 |  958 | `PH7_PRIVATE sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|        5 |  959 | `{` |
|   127747 |  960 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   127747 |  961 | `	GenBlock *pForBlock = 0;` |
|        - |  962 | `	sxu32 nFalseJump;` |
|        - |  963 | `	sxu32 nLine;` |
|        - |  964 | `	sxi32 rc;` |
|   127747 |  965 | `	nLine = pGen->pIn->nLine;` |
|        - |  966 | `	/* Jump the 'for' keyword */` |
|   127747 |  967 | `	pGen->pIn++;` |
|   127747 |  968 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  969 | `		/* Syntax error */` |
|      ! 0 |  970 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|      ! 0 |  971 | `		if( rc == SXERR_ABORT ){` |
|        - |  972 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  973 | `			return SXERR_ABORT;` |
|        - |  974 | `		}` |
|      ! 0 |  975 | `		return SXRET_OK;` |
|        - |  976 | `	}` |
|        - |  977 | `	/* Jump the left parenthesis '(' */` |
|   127747 |  978 | `	pGen->pIn++;` |
|        - |  979 | `	/* Delimit the init-expr;condition;post-expr */` |
|   127747 |  980 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   127747 |  981 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  982 | `		/* Empty expression */` |
|      ! 0 |  983 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|      ! 0 |  984 | `		if( rc == SXERR_ABORT ){` |
|        - |  985 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  986 | `			return SXERR_ABORT;` |
|        - |  987 | `		}` |
|        - |  988 | `		/* Synchronize */` |
|      ! 0 |  989 | `		pGen->pIn = pEnd;` |
|      ! 0 |  990 | `		if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 |  991 | `			pGen->pIn++;` |
|      ! 0 |  992 | `		}` |
|      ! 0 |  993 | `		return SXRET_OK;` |
|        - |  994 | `	}` |
|        - |  995 | `	/* Swap token streams */` |
|   127747 |  996 | `	pTmp = pGen->pEnd;` |
|   127747 |  997 | `	pGen->pEnd = pEnd;` |
|        - |  998 | `	/* for() clauses are the ONLY place php's grammar allows a comma-separated` |
|        - |  999 | `	 * expression list, so the comma operator is permitted for their duration` |
|        - | 1000 | `	 * (see GenStateTreeHasComma). A closure body nested inside a clause is` |
|        - | 1001 | `	 * compiled through this same window — recorded as a known leniency. */` |
|   127747 | 1002 | `	pGen->nCommaExprOk++;` |
|   127747 | 1003 | `	pGen->zClauseCloser = "\";\""; /* init/condition clauses close on ';' */` |
|        - | 1004 | `	/* Compile initialization expressions if available */` |
|   127747 | 1005 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 1006 | `	/* Pop operand lvalues */` |
|   127747 | 1007 | `	if( rc == SXERR_ABORT ){` |
|        - | 1008 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1009 | `		return SXERR_ABORT;` |
|   127747 | 1010 | `	}else if( rc != SXERR_EMPTY ){` |
|   116147 | 1011 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    58071 | 1012 | `	}` |
|   127747 | 1013 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - | 1014 | `		/* Syntax error */` |
|      ! 0 | 1015 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|      ! 0 | 1016 | `		if( rc == SXERR_ABORT ){` |
|        - | 1017 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1018 | `			return SXERR_ABORT;` |
|        - | 1019 | `		}` |
|      ! 0 | 1020 | `		return SXRET_OK;` |
|        - | 1021 | `	}` |
|        - | 1022 | `	/* Jump the trailing ';' */` |
|   127747 | 1023 | `	pGen->pIn++;` |
|        - | 1024 | `	/* Create the loop block */` |
|   127747 | 1025 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   127747 | 1026 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1027 | `		return SXERR_ABORT;` |
|        - | 1028 | `	}` |
|        - | 1029 | `	/* Deffer continue jumps */` |
|   127747 | 1030 | `	pForBlock->bPostContinue = TRUE;` |
|        - | 1031 | `	/* Compile the condition */` |
|   127747 | 1032 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   127747 | 1033 | `	if( rc == SXERR_ABORT ){` |
|        - | 1034 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1035 | `		return SXERR_ABORT;` |
|   127747 | 1036 | `	}else if( rc != SXERR_EMPTY ){` |
|        - | 1037 | `		/* Emit the false jump */` |
|   116147 | 1038 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|        - | 1039 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   116147 | 1040 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    58071 | 1041 | `	}` |
|   127747 | 1042 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - | 1043 | `		/* Syntax error */` |
|        6 | 1044 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|        6 | 1045 | `		if( rc == SXERR_ABORT ){` |
|        - | 1046 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1047 | `			return SXERR_ABORT;` |
|        - | 1048 | `		}` |
|        6 | 1049 | `		return SXRET_OK;` |
|        - | 1050 | `	}` |
|        - | 1051 | `	/* Jump the trailing ';' */` |
|   127743 | 1052 | `	pGen->pIn++;` |
|        - | 1053 | `	/* Save the post condition stream */` |
|   127743 | 1054 | `	pPostStart = pGen->pIn;` |
|        - | 1055 | `	/* Compile the loop body — OUTSIDE the comma window (the body is ordinary` |
|        - | 1056 | ``	 * php, so `(1, 2)` inside it is the parse error it should be). */`` |
|   127743 | 1057 | `	pGen->nCommaExprOk--;` |
|   127743 | 1058 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   127743 | 1059 | `	pGen->pEnd = pTmp;` |
|   127743 | 1060 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   127743 | 1061 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 1062 | `		return SXERR_ABORT;` |
|        - | 1063 | `	}` |
|        - | 1064 | `	/* Fix post-continue jumps */` |
|   127743 | 1065 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|        - | 1066 | `		JumpFixup *aPost;` |
|        - | 1067 | `		VmInstr *pInstr;` |
|        - | 1068 | `		sxu32 nJumpDest;` |
|        - | 1069 | `		sxu32 n;` |
|    11615 | 1070 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|    11615 | 1071 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|    38689 | 1072 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|    27079 | 1073 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|    27079 | 1074 | `			if( pInstr ){` |
|        - | 1075 | `				/* Fix jump */` |
|    27079 | 1076 | `				pInstr->iP2 = nJumpDest;` |
|    13537 | 1077 | `			}` |
|    13542 | 1078 | `		}` |
|     5805 | 1079 | `	}` |
|        - | 1080 | `	/* compile the post-expressions if available */` |
|   127743 | 1081 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|      ! 0 | 1082 | `		pPostStart++;` |
|      ! 0 | 1083 | `	}` |
|   127743 | 1084 | `	if( pPostStart < pEnd ){` |
|        - | 1085 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   116145 | 1086 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   116145 | 1087 | `		pGen->nCommaExprOk++; /* post-expressions are a clause list again */` |
|   116145 | 1088 | `		pGen->zClauseCloser = "\")\""; /* the post clause closes on ')' */` |
|   116145 | 1089 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   116145 | 1090 | `		pGen->nCommaExprOk--;` |
|   116145 | 1091 | `		pGen->zClauseCloser = 0;` |
|   116145 | 1092 | `		if( pGen->pIn < pGen->pEnd ){` |
|        - | 1093 | `			/* php names the token and expects ')'; the post-clause runs to the ')'. */` |
|      ! 0 | 1094 | `			rc = PH7_GenSyntaxError(pGen,pGen->pIn,"\")\"");` |
|      ! 0 | 1095 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1096 | `				return SXERR_ABORT;` |
|        - | 1097 | `			}` |
|      ! 0 | 1098 | `			return SXRET_OK;` |
|        - | 1099 | `		}` |
|   116145 | 1100 | `		RE_SWAP_DELIMITER(pGen);` |
|   116145 | 1101 | `		if( rc == SXERR_ABORT ){` |
|        - | 1102 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1103 | `			return SXERR_ABORT;` |
|   116145 | 1104 | `		}else if( rc != SXERR_EMPTY){` |
|        - | 1105 | `			/* Pop operand lvalue */` |
|   116145 | 1106 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    58070 | 1107 | `		}` |
|    58070 | 1108 | `	}` |
|        - | 1109 | `	/* Emit the unconditional jump to the start of the loop */` |
|   127743 | 1110 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|        - | 1111 | `	/* Fix all jumps now the destination is resolved */` |
|   127743 | 1112 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 1113 | `	/* Release the loop block */` |
|   127743 | 1114 | `	GenStateLeaveBlock(pGen,0);` |
|        - | 1115 | `	/* Statement successfully compiled */` |
|   127743 | 1116 | `	return SXRET_OK;` |
|    63876 | 1117 | `}` |
|        - | 1118 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|        - | 1119 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|        - | 1120 | ` * are allowed.` |
|        - | 1121 | ` */` |
|   461200 | 1122 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 1123 | `{` |
|   461205 | 1124 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|   461205 | 1125 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|        - | 1126 | `		/* Unexpected expression */` |
|      ! 0 | 1127 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 1128 | `			"foreach: Expecting a variable name");` |
|      ! 0 | 1129 | `		if( rc != SXERR_ABORT ){` |
|      ! 0 | 1130 | `			rc = SXERR_INVALID;` |
|      ! 0 | 1131 | `		}` |
|      ! 0 | 1132 | `	}` |
|   461205 | 1133 | `	return rc;` |
|        5 | 1134 | `}` |
|        - | 1135 | `/*` |
|        - | 1136 | ` * Compile the 'foreach' statement.` |
|        - | 1137 | ` * According to the PHP language reference` |
|        - | 1138 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|        - | 1139 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|        - | 1140 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|        - | 1141 | ` *  is a minor but useful extension of the first:` |
|        - | 1142 | ` *  foreach (array_expression as $value)` |
|        - | 1143 | ` *    statement` |
|        - | 1144 | ` *  foreach (array_expression as $key => $value)` |
|        - | 1145 | ` *   statement` |
|        - | 1146 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|        - | 1147 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|        - | 1148 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|        - | 1149 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|        - | 1150 | ` *  to the variable $key on each loop.` |
|        - | 1151 | ` *  Note:` |
|        - | 1152 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|        - | 1153 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|        - | 1154 | ` *  Note:` |
|        - | 1155 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|        - | 1156 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|        - | 1157 | ` *  or after the foreach without resetting it.` |
|        - | 1158 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|        - | 1159 | ` *  of copying the value.` |
|        - | 1160 | ` */` |
|   325590 | 1161 | `PH7_PRIVATE sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|        5 | 1162 | `{` |
|   325595 | 1163 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|   325595 | 1164 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|   325595 | 1165 | `	GenBlock *pForeachBlock = 0;` |
|        - | 1166 | `	ph7_foreach_info *pInfo;` |
|        - | 1167 | `	sxu32 nFalseJump;` |
|        - | 1168 | `	VmInstr *pInstr;` |
|        - | 1169 | `	sxu32 nLine;` |
|        - | 1170 | `	sxi32 rc;` |
|   325595 | 1171 | `	nLine = pGen->pIn->nLine;` |
|        - | 1172 | `	/* Jump the 'foreach' keyword */` |
|   325595 | 1173 | `	pGen->pIn++;` |
|   325595 | 1174 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1175 | `		/* Syntax error */` |
|      ! 0 | 1176 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|      ! 0 | 1177 | `		if( rc == SXERR_ABORT ){` |
|        - | 1178 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1179 | `			return SXERR_ABORT;` |
|        - | 1180 | `		}` |
|      ! 0 | 1181 | `		goto Synchronize;` |
|        - | 1182 | `	}` |
|        - | 1183 | `	/* Jump the left parenthesis '(' */` |
|   325595 | 1184 | `	pGen->pIn++;` |
|        - | 1185 | `	/* Create the loop block */` |
|   325595 | 1186 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|   325595 | 1187 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1188 | `		return SXERR_ABORT;` |
|        - | 1189 | `	}` |
|        - | 1190 | `	/* Delimit the expression */` |
|   325595 | 1191 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   325595 | 1192 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - | 1193 | `		/* Empty expression */` |
|      ! 0 | 1194 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|      ! 0 | 1195 | `		if( rc == SXERR_ABORT ){` |
|        - | 1196 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1197 | `			return SXERR_ABORT;` |
|        - | 1198 | `		}` |
|        - | 1199 | `		/* Synchronize */` |
|      ! 0 | 1200 | `		pGen->pIn = pEnd;` |
|      ! 0 | 1201 | `		if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 | 1202 | `			pGen->pIn++;` |
|      ! 0 | 1203 | `		}` |
|      ! 0 | 1204 | `		return SXRET_OK;` |
|        - | 1205 | `	}` |
|        - | 1206 | `	/* Compile the array expression */` |
|   325595 | 1207 | `	pCur = pGen->pIn;` |
|  1845409 | 1208 | `	while( pCur < pEnd ){` |
|  1845409 | 1209 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|   356545 | 1210 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|   356545 | 1211 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|        - | 1212 | `				/* Break with the first 'as' found */` |
|   325595 | 1213 | `				break;` |
|        - | 1214 | `			}` |
|    15475 | 1215 | `		}` |
|        - | 1216 | `		/* Advance the stream cursor */` |
|  1519819 | 1217 | `		pCur++;` |
|        5 | 1218 | `	}` |
|   325595 | 1219 | `	if( pCur <= pGen->pIn ){` |
|      ! 0 | 1220 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 1221 | `			"foreach: Missing array/object expression");` |
|      ! 0 | 1222 | `		if( rc == SXERR_ABORT ){` |
|        - | 1223 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1224 | `			return SXERR_ABORT;` |
|        - | 1225 | `		}` |
|      ! 0 | 1226 | `		goto Synchronize;` |
|        - | 1227 | `	}` |
|        - | 1228 | `	/* Swap token streams */` |
|   325595 | 1229 | `	pTmp = pGen->pEnd;` |
|   325595 | 1230 | `	pGen->pEnd = pCur;` |
|   325595 | 1231 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   325595 | 1232 | `	if( rc == SXERR_ABORT ){` |
|        - | 1233 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1234 | `		return SXERR_ABORT;` |
|        - | 1235 | `	}` |
|        - | 1236 | `	/* Update token stream */` |
|   325595 | 1237 | `	while(pGen->pIn < pCur ){` |
|      ! 0 | 1238 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 1239 | `		if( rc == SXERR_ABORT ){` |
|        - | 1240 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1241 | `			return SXERR_ABORT;` |
|        - | 1242 | `		}` |
|      ! 0 | 1243 | `		pGen->pIn++;` |
|      ! 0 | 1244 | `	}` |
|   325595 | 1245 | `	pCur++; /* Jump the 'as' keyword */` |
|   325595 | 1246 | `	pGen->pIn = pCur;` |
|   325595 | 1247 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 1248 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|      ! 0 | 1249 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1250 | `			return SXERR_ABORT;` |
|        - | 1251 | `		}` |
|      ! 0 | 1252 | `	}` |
|        - | 1253 | `	/* Create the foreach context */` |
|   325595 | 1254 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|   325595 | 1255 | `	if( pInfo == 0 ){` |
|      ! 0 | 1256 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|      ! 0 | 1257 | `		return SXERR_ABORT;` |
|        - | 1258 | `	}` |
|        - | 1259 | `	/* Zero the structure */` |
|   325595 | 1260 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|        - | 1261 | `	/* Initialize structure fields */` |
|   325595 | 1262 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|        - | 1263 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|        - | 1264 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|        - | 1265 | `	 * '=>'. */` |
|   325595 | 1266 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|   325595 | 1267 | `	if( pCur < pEnd ){` |
|        - | 1268 | `		/* Compile the expression holding the key name */` |
|   135639 | 1269 | `		if( pGen->pIn >= pCur ){` |
|      ! 0 | 1270 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|      ! 0 | 1271 | `			if( rc == SXERR_ABORT ){` |
|        - | 1272 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1273 | `				return SXERR_ABORT;` |
|        - | 1274 | `			}` |
|      ! 0 | 1275 | `		}else{` |
|   135639 | 1276 | `			pGen->pEnd = pCur;` |
|   135639 | 1277 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|   135639 | 1278 | `			if( rc == SXERR_ABORT ){` |
|        - | 1279 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1280 | `				return SXERR_ABORT;` |
|        - | 1281 | `			}` |
|   135639 | 1282 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|   135639 | 1283 | `			if( pInstr->p3 ){` |
|        - | 1284 | `				/* Record key name */` |
|   135639 | 1285 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    67817 | 1286 | `			}` |
|   135639 | 1287 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|        - | 1288 | `		}` |
|   135639 | 1289 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    67817 | 1290 | `	}` |
|   325595 | 1291 | `	pGen->pEnd = pEnd;` |
|   325595 | 1292 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 1293 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|      ! 0 | 1294 | `		if( rc == SXERR_ABORT ){` |
|        - | 1295 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1296 | `			return SXERR_ABORT;` |
|        - | 1297 | `		}` |
|      ! 0 | 1298 | `		goto Synchronize;` |
|        - | 1299 | `	}` |
|   325595 | 1300 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|       33 | 1301 | `		pGen->pIn++;` |
|        - | 1302 | `		/* Pass by reference  */` |
|       33 | 1303 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       15 | 1304 | `	}` |
|        - | 1305 | `	/* Check if the value target is list() */` |
|   325595 | 1306 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        8 | 1307 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|        - | 1308 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|        - | 1309 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|        - | 1310 | `		 */` |
|        - | 1311 | `		static int iForeachListCnt = 0;` |
|        - | 1312 | `		char zTmp[128];` |
|        - | 1313 | `		sxu32 nLen;` |
|        - | 1314 | `		char *zDup;` |
|       10 | 1315 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|       10 | 1316 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       10 | 1317 | `		if( zDup == 0 ){` |
|      ! 0 | 1318 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1319 | `			return SXERR_ABORT;` |
|        - | 1320 | `		}` |
|       10 | 1321 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|        - | 1322 | `		/* Save list() token boundaries */` |
|       10 | 1323 | `		pListStart = pGen->pIn;` |
|        - | 1324 | `		/* Advance past list(...) — validate parentheses */` |
|       10 | 1325 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|       10 | 1326 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1327 | `			/* php: syntax error, unexpected variable "$x", expecting "(" */` |
|        3 | 1328 | `			rc = PH7_GenSyntaxError(pGen,pGen->pIn < pEnd ? pGen->pIn : 0,"\"(\"");` |
|        3 | 1329 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1330 | `				return SXERR_ABORT;` |
|        - | 1331 | `			}` |
|        3 | 1332 | `			goto Synchronize;` |
|        - | 1333 | `		}` |
|        7 | 1334 | `		pGen->pIn++; /* Jump '(' */` |
|        7 | 1335 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|        7 | 1336 | `		if( pListEnd >= pEnd ){` |
|      ! 0 | 1337 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1338 | `				"foreach: Missing closing ')' after list");` |
|      ! 0 | 1339 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1340 | `				return SXERR_ABORT;` |
|        - | 1341 | `			}` |
|      ! 0 | 1342 | `			goto Synchronize;` |
|        - | 1343 | `		}` |
|        7 | 1344 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|        7 | 1345 | `		pListEnd = pGen->pIn;` |
|        7 | 1346 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|   325590 | 1347 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|        - | 1348 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|        - | 1349 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|        - | 1350 | `		 */` |
|        - | 1351 | `		static int iForeachShortListCnt = 0;` |
|        - | 1352 | `		char zTmp[128];` |
|        - | 1353 | `		sxu32 nLen;` |
|        - | 1354 | `		char *zDup;` |
|       17 | 1355 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       17 | 1356 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       17 | 1357 | `		if( zDup == 0 ){` |
|      ! 0 | 1358 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1359 | `			return SXERR_ABORT;` |
|        - | 1360 | `		}` |
|       17 | 1361 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|        - | 1362 | `		/* Save [...] token boundaries */` |
|       17 | 1363 | `		pListStart = pGen->pIn;` |
|        - | 1364 | `		/* Advance past [...] */` |
|       17 | 1365 | `		pGen->pIn++; /* Jump '[' */` |
|       17 | 1366 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       17 | 1367 | `		if( pListEnd >= pEnd ){` |
|      ! 0 | 1368 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1369 | `				"foreach: Missing closing ']' after short list");` |
|      ! 0 | 1370 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1371 | `				return SXERR_ABORT;` |
|        - | 1372 | `			}` |
|      ! 0 | 1373 | `			goto Synchronize;` |
|        - | 1374 | `		}` |
|       17 | 1375 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       17 | 1376 | `		pListEnd = pGen->pIn;` |
|       17 | 1377 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|        9 | 1378 | `	}else{` |
|        - | 1379 | `		/* Compile the expression holding the value name */` |
|   325571 | 1380 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|   325571 | 1381 | `		if( rc == SXERR_ABORT ){` |
|        - | 1382 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1383 | `			return SXERR_ABORT;` |
|        - | 1384 | `		}` |
|   325571 | 1385 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|   325571 | 1386 | `		if( pInstr->p3 ){` |
|        - | 1387 | `			/* Record value name */` |
|   325571 | 1388 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|   162783 | 1389 | `		}` |
|        - | 1390 | `	}` |
|        - | 1391 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|   325593 | 1392 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|        - | 1393 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   325593 | 1394 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|        - | 1395 | `	/* Record the first instruction to execute */` |
|   325593 | 1396 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|        - | 1397 | `	/* Emit the FOREACH_STEP instruction */` |
|   325593 | 1398 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|        - | 1399 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   325593 | 1400 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|        - | 1401 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|   325593 | 1402 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|        - | 1403 | `		SyToken *pSavedIn,*pSavedEnd;` |
|        - | 1404 | `		/* Load the temporary variable holding the current value onto the stack.` |
|        - | 1405 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|        - | 1406 | `		 */` |
|       23 | 1407 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|        - | 1408 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|        - | 1409 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|        - | 1410 | `		 * picks up the delimiter and the variable names inside.` |
|        - | 1411 | `		 */` |
|       23 | 1412 | `		pSavedIn = pGen->pIn;` |
|       23 | 1413 | `		pSavedEnd = pGen->pEnd;` |
|       23 | 1414 | `		pGen->pIn = pListStart;` |
|       23 | 1415 | `		pGen->pEnd = pListEnd;` |
|       23 | 1416 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       17 | 1417 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|        9 | 1418 | `		}else{` |
|        7 | 1419 | `			rc = PH7_CompileList(&(*pGen),0);` |
|        - | 1420 | `		}` |
|       23 | 1421 | `		pGen->pIn = pSavedIn;` |
|       23 | 1422 | `		pGen->pEnd = pSavedEnd;` |
|       23 | 1423 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1424 | `			return SXERR_ABORT;` |
|        - | 1425 | `		}` |
|        - | 1426 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|       23 | 1427 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       11 | 1428 | `	}` |
|        - | 1429 | `	/* Compile the loop body */` |
|   325593 | 1430 | `	pGen->pIn = &pEnd[1];` |
|   325593 | 1431 | `	pGen->pEnd = pTmp;` |
|   325593 | 1432 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|   325593 | 1433 | `	if( rc == SXERR_ABORT ){` |
|        - | 1434 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1435 | `		return SXERR_ABORT;` |
|        - | 1436 | `	}` |
|        - | 1437 | `	/* Emit the unconditional jump to the start of the loop */` |
|   325593 | 1438 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|        - | 1439 | `	/* Fix all jumps now the destination is resolved */` |
|   325593 | 1440 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 1441 | `	/* Release the loop block */` |
|   325593 | 1442 | `	GenStateLeaveBlock(pGen,0);` |
|        - | 1443 | `	/* Statement successfully compiled */` |
|   325593 | 1444 | `	return SXRET_OK;` |
|        1 | 1445 | `Synchronize:` |
|        - | 1446 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - | 1447 | `	 * compiling this erroneous block.` |
|        - | 1448 | `	 */` |
|        3 | 1449 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 | 1450 | `		pGen->pIn++;` |
|      ! 0 | 1451 | `	}` |
|        3 | 1452 | `	return SXRET_OK;` |
|   162800 | 1453 | `}` |
|        - | 1454 | `/*` |
|        - | 1455 | ` * Compile the infamous if/elseif/else if/else statements.` |
|        - | 1456 | ` * According to the PHP language reference` |
|        - | 1457 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|        - | 1458 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|        - | 1459 | ` *  that is similar to that of C:` |
|        - | 1460 | ` *  if (expr)` |
|        - | 1461 | ` *   statement` |
|        - | 1462 | ` *  else construct:` |
|        - | 1463 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|        - | 1464 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|        - | 1465 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|        - | 1466 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|        - | 1467 | ` *   $b, and a is NOT greater than b otherwise.` |
|        - | 1468 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|        - | 1469 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|        - | 1470 | ` *  elseif` |
|        - | 1471 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|        - | 1472 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|        - | 1473 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|        - | 1474 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|        - | 1475 | ` *   than b, a equal to b or a is smaller than b:` |
|        - | 1476 | ` *   <?php` |
|        - | 1477 | ` *    if ($a > $b) {` |
|        - | 1478 | ` *     echo "a is bigger than b";` |
|        - | 1479 | ` *    } elseif ($a == $b) {` |
|        - | 1480 | ` *     echo "a is equal to b";` |
|        - | 1481 | ` *    } else {` |
|        - | 1482 | ` *     echo "a is smaller than b";` |
|        - | 1483 | ` *    }` |
|        - | 1484 | ` *    ?>` |
|        - | 1485 | ` */` |
|  2417764 | 1486 | `PH7_PRIVATE sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|        5 | 1487 | `{` |
|  2417769 | 1488 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  2417769 | 1489 | `	GenBlock *pCondBlock = 0;` |
|        - | 1490 | `	sxu32 nJumpIdx;` |
|        - | 1491 | `	sxu32 nKeyID;` |
|        - | 1492 | `	sxi32 rc;` |
|        - | 1493 | `	/* Jump the 'if' keyword */` |
|  2417769 | 1494 | `	pGen->pIn++;` |
|  2417769 | 1495 | `	pToken = pGen->pIn;` |
|        - | 1496 | `	/* Create the conditional block */` |
|  2417769 | 1497 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  2417769 | 1498 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1499 | `		return SXERR_ABORT;` |
|        - | 1500 | `	}` |
|        - | 1501 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|  1351996 | 1502 | `	for(;;){` |
|  2703997 | 1503 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1504 | `			/* Syntax error */` |
|      ! 0 | 1505 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 1506 | `				pToken--;` |
|      ! 0 | 1507 | `			}` |
|      ! 0 | 1508 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|      ! 0 | 1509 | `			if( rc == SXERR_ABORT ){` |
|        - | 1510 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 1511 | `				return SXERR_ABORT;` |
|        - | 1512 | `			}` |
|      ! 0 | 1513 | `			goto Synchronize;` |
|        - | 1514 | `		}` |
|        - | 1515 | `		/* Jump the left parenthesis '(' */` |
|  2703997 | 1516 | `		pToken++;` |
|        - | 1517 | `		/* Delimit the condition */` |
|  2703997 | 1518 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  2703997 | 1519 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|        - | 1520 | `			/* Syntax error */` |
|      ! 0 | 1521 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 1522 | `				pToken--;` |
|      ! 0 | 1523 | `			}` |
|      ! 0 | 1524 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|      ! 0 | 1525 | `			if( rc == SXERR_ABORT ){` |
|        - | 1526 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 1527 | `				return SXERR_ABORT;` |
|        - | 1528 | `			}` |
|      ! 0 | 1529 | `			goto Synchronize;` |
|        - | 1530 | `		}` |
|        - | 1531 | `		/* Swap token streams */` |
|  2703997 | 1532 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|        - | 1533 | `		/* Compile the condition */` |
|  2703997 | 1534 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 1535 | `		/* Update token stream */` |
|  2703997 | 1536 | `		while(pGen->pIn < pEnd ){` |
|      ! 0 | 1537 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 | 1538 | `			pGen->pIn++;` |
|      ! 0 | 1539 | `		}` |
|  2703997 | 1540 | `		pGen->pIn  = &pEnd[1];` |
|  2703997 | 1541 | `		pGen->pEnd = pTmp;` |
|  2703997 | 1542 | `		if( rc == SXERR_ABORT ){` |
|        - | 1543 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|        3 | 1544 | `			return SXERR_ABORT;` |
|        - | 1545 | `		}` |
|        - | 1546 | `		/* Emit the false jump */` |
|  2703995 | 1547 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|        - | 1548 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  2703995 | 1549 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|        - | 1550 | `		/* Compile the body */` |
|  2703995 | 1551 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  2703995 | 1552 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1553 | `			return SXERR_ABORT;` |
|        - | 1554 | `		}` |
|  2703995 | 1555 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   518454 | 1556 | `			break;` |
|        - | 1557 | `		}` |
|        - | 1558 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|  1667097 | 1559 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  1667097 | 1560 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|  1160089 | 1561 | `			break;` |
|        - | 1562 | `		}` |
|        - | 1563 | `		/* Emit the unconditional jump */` |
|   507013 | 1564 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|        - | 1565 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   507013 | 1566 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   507013 | 1567 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   305837 | 1568 | `			pToken = &pGen->pIn[1];` |
|   305837 | 1569 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    85090 | 1570 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|   110395 | 1571 | `					break;` |
|        - | 1572 | `			}` |
|    85057 | 1573 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    42526 | 1574 | `		}` |
|   286233 | 1575 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|        - | 1576 | `		/* Synchronize cursors */` |
|   286233 | 1577 | `		pToken = pGen->pIn;` |
|        - | 1578 | `		/* Fix the false jump */` |
|   286233 | 1579 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|        5 | 1580 | `	} /* For(;;) */` |
|        - | 1581 | `	/* Fix the false jump */` |
|  2417767 | 1582 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  2417767 | 1583 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|  1380864 | 1584 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|        - | 1585 | `			/* Compile the else block */` |
|   220785 | 1586 | `			pGen->pIn++;` |
|   220785 | 1587 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   220785 | 1588 | `			if( rc == SXERR_ABORT ){` |
|        - | 1589 |  |
|      ! 0 | 1590 | `				return SXERR_ABORT;` |
|        - | 1591 | `			}` |
|   110390 | 1592 | `	}` |
|  2417767 | 1593 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|        - | 1594 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  2417767 | 1595 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|        - | 1596 | `	/* Release the conditional block */` |
|  2417767 | 1597 | `	GenStateLeaveBlock(pGen,0);` |
|        - | 1598 | `	/* Statement successfully compiled */` |
|  2417767 | 1599 | `	return SXRET_OK;` |
|      ! 0 | 1600 | `Synchronize:` |
|        - | 1601 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|        - | 1602 | `	 */` |
|      ! 0 | 1603 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 | 1604 | `		pGen->pIn++;` |
|      ! 0 | 1605 | `	}` |
|      ! 0 | 1606 | `	return SXRET_OK;` |
|  1208887 | 1607 | `}` |
|        - | 1608 | `/*` |
|        - | 1609 | ` * Compile the global construct.` |
|        - | 1610 | ` * According to the PHP language reference` |
|        - | 1611 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|        - | 1612 | ` *  to be used in that function.` |
|        - | 1613 | ` *  Example #1 Using global` |
|        - | 1614 | ` *  <?php` |
|        - | 1615 | ` *   $a = 1;` |
|        - | 1616 | ` *   $b = 2;` |
|        - | 1617 | ` *   function Sum()` |
|        - | 1618 | ` *   {` |
|        - | 1619 | ` *    global $a, $b;` |
|        - | 1620 | ` *    $b = $a + $b;` |
|        - | 1621 | ` *   }` |
|        - | 1622 | ` *   Sum();` |
|        - | 1623 | ` *   echo $b;` |
|        - | 1624 | ` *  ?>` |
|        - | 1625 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|        - | 1626 | ` *  all references to either variable will refer to the global version. There is no limit` |
|        - | 1627 | ` *  to the number of global variables that can be manipulated by a function.` |
|        - | 1628 | ` */` |
|       36 | 1629 | `PH7_PRIVATE sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|        5 | 1630 | `{` |
|       41 | 1631 | `	SyToken *pTmp,*pNext = 0;` |
|        - | 1632 | `	sxi32 nExpr;` |
|        - | 1633 | `	sxi32 rc;` |
|        - | 1634 | `	/* Jump the 'global' keyword */` |
|       41 | 1635 | `	pGen->pIn++;` |
|       41 | 1636 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|        - | 1637 | `		/* Nothing to process */` |
|      ! 0 | 1638 | `		return SXRET_OK;` |
|        - | 1639 | `	}` |
|       41 | 1640 | `	pTmp = pGen->pEnd;` |
|       41 | 1641 | `	nExpr = 0;` |
|       87 | 1642 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       51 | 1643 | `		if( pGen->pIn < pNext ){` |
|       51 | 1644 | `			pGen->pEnd = pNext;` |
|       51 | 1645 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 | 1646 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|      ! 0 | 1647 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1648 | `					return SXERR_ABORT;` |
|        - | 1649 | `				}` |
|      ! 0 | 1650 | `			}else{` |
|       51 | 1651 | `				pGen->pIn++;` |
|       51 | 1652 | `				if( pGen->pIn >= pGen->pEnd ){` |
|        - | 1653 | `					/* Emit a warning */` |
|      ! 0 | 1654 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|      ! 0 | 1655 | `				}else{` |
|       51 | 1656 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       51 | 1657 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 1658 | `						return SXERR_ABORT;` |
|       51 | 1659 | `					}else if(rc != SXERR_EMPTY ){` |
|       51 | 1660 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|       51 | 1661 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|        - | 1662 | `							/* Variable name, not a constant */` |
|       51 | 1663 | `							pLast->iP1 = 0;` |
|       23 | 1664 | `						}` |
|       51 | 1665 | `						nExpr++;` |
|       23 | 1666 | `					}` |
|        - | 1667 | `				}` |
|        - | 1668 | `			}` |
|       23 | 1669 | `		}` |
|        - | 1670 | `		/* Next expression in the stream */` |
|       51 | 1671 | `		pGen->pIn = pNext;` |
|        - | 1672 | `		/* Jump trailing commas */` |
|       61 | 1673 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       15 | 1674 | `			pGen->pIn++;` |
|        5 | 1675 | `		}` |
|        5 | 1676 | `	}` |
|        - | 1677 | `	/* Restore token stream */` |
|       41 | 1678 | `	pGen->pEnd = pTmp;` |
|       41 | 1679 | `	if( nExpr > 0 ){` |
|        - | 1680 | `		/* Emit the uplink instruction */` |
|       41 | 1681 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|       18 | 1682 | `	}` |
|       41 | 1683 | `	return SXRET_OK;` |
|       23 | 1684 | `}` |
|        - | 1685 | `/*` |
|        - | 1686 | ` * Compile the return statement.` |
|        - | 1687 | ` * According to the PHP language reference` |
|        - | 1688 | ` *  If called from within a function, the return() statement immediately ends execution` |
|        - | 1689 | ` *  of the current function, and returns its argument as the value of the function call.` |
|        - | 1690 | ` *  return() will also end the execution of an eval() statement or script file.` |
|        - | 1691 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|        - | 1692 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|        - | 1693 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|        - | 1694 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|        - | 1695 | ` *  from within the main script file, then script execution end.` |
|        - | 1696 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|        - | 1697 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|        - | 1698 | ` *  should do so as PHP has less work to do in this case.` |
|        - | 1699 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|        - | 1700 | ` */` |
|  3605596 | 1701 | `PH7_PRIVATE sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|        5 | 1702 | `{` |
|  3605601 | 1703 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|        - | 1704 | `	sxi32 rc;` |
|  3605601 | 1705 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  3605601 | 1706 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|        - | 1707 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|        - | 1708 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|        - | 1709 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|        - | 1710 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|        - | 1711 | `	 * normally below so token processing stays consistent. */` |
|  9492959 | 1712 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|  5887363 | 1713 | `		pFuncBlock = pFuncBlock->pParent;` |
|        5 | 1714 | `	}` |
|  3605596 | 1715 | `	if( pFuncBlock && pFuncBlock->pUserData` |
|  3605585 | 1716 | `	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){` |
|        3 | 1717 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1718 | `			"A never-returning function must not return");` |
|        3 | 1719 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1720 | `			return SXERR_ABORT;` |
|        - | 1721 | `		}` |
|        1 | 1722 | `	}` |
|        - | 1723 | `	/* Jump the 'return' keyword */` |
|  3605601 | 1724 | `	pGen->pIn++;` |
|  3605601 | 1725 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - | 1726 | ``		/* php: a stray token after `return EXPR` is `... expecting ";"`. */`` |
|  3508939 | 1727 | `		const char *zSave = pGen->zClauseCloser;` |
|  3508939 | 1728 | `		pGen->zClauseCloser = "\";\"";` |
|  3508939 | 1729 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  3508939 | 1730 | `		pGen->zClauseCloser = zSave;` |
|  3508939 | 1731 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1732 | `			return SXERR_ABORT;` |
|  3508939 | 1733 | `		}else if(rc != SXERR_EMPTY ){` |
|  3508939 | 1734 | `			nRet = 1;` |
|  1754467 | 1735 | `		}` |
|  1754467 | 1736 | `	}` |
|        - | 1737 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|        - | 1738 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|        - | 1739 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|        - | 1740 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|  3605601 | 1741 | `	if( pGen->bInGenerator ){` |
|     3899 | 1742 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|     3899 | 1743 | `		return SXRET_OK;` |
|        - | 1744 | `	}` |
|        - | 1745 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|        - | 1746 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|        - | 1747 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|        - | 1748 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|        - | 1749 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|  3601707 | 1750 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|  3601707 | 1751 | `	return SXRET_OK;` |
|  1802803 | 1752 | `}` |
|        - | 1753 | `/*` |
|        - | 1754 | ` * Compile a yield expression.` |
|        - | 1755 | ` * Called from the expression code generator when a yield node is encountered.` |
|        - | 1756 | ` * Handles: yield, yield $value, yield $key => $value` |
|        - | 1757 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|        - | 1758 | ` */` |
|    15852 | 1759 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|        5 | 1760 | `{` |
|        - | 1761 | `	SyToken *pTmp, *pSplit;` |
|    15857 | 1762 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|    15857 | 1763 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|        - | 1764 | `	sxi32 rc;` |
|     7926 | 1765 | `	(void)iCompileFlag;` |
|        - | 1766 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|    15857 | 1767 | `	pGen->pIn++;` |
|        - | 1768 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|        - | 1769 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|        - | 1770 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|        - | 1771 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|        - | 1772 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|    15852 | 1773 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|     7961 | 1774 | `		&& pGen->pIn->sData.nByte == 4` |
|       72 | 1775 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|       67 | 1776 | `		pGen->pIn++; /* Skip 'from' */` |
|       67 | 1777 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       67 | 1778 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1779 | `			return SXERR_ABORT;` |
|        - | 1780 | `		}` |
|       67 | 1781 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 | 1782 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|      ! 0 | 1783 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|        - | 1784 | `				"Missing expression after 'yield from'");` |
|      ! 0 | 1785 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1786 | `				return SXERR_ABORT;` |
|        - | 1787 | `			}` |
|      ! 0 | 1788 | `		}` |
|       67 | 1789 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|       67 | 1790 | `		return SXRET_OK;` |
|        - | 1791 | `	}` |
|    15795 | 1792 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - | 1793 | `		/* Bare yield — no value */` |
|        3 | 1794 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|        3 | 1795 | `		return SXRET_OK;` |
|        - | 1796 | `	}` |
|        - | 1797 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|    15793 | 1798 | `	pSplit = 0;` |
|        - | 1799 | `	{` |
|    15793 | 1800 | `		SyToken *pCur = pGen->pIn;` |
|    15793 | 1801 | `		sxi32 nNest = 0;` |
|    47183 | 1802 | `		while( pCur < pGen->pEnd ){` |
|    46873 | 1803 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       18 | 1804 | `				nNest++;` |
|    46865 | 1805 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       18 | 1806 | `				nNest--;` |
|    46849 | 1807 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|    15483 | 1808 | `				pSplit = pCur;` |
|    15483 | 1809 | `				break;` |
|        - | 1810 | `			}` |
|    31395 | 1811 | `			pCur++;` |
|        5 | 1812 | `		}` |
|        - | 1813 | `	}` |
|    15793 | 1814 | `	pTmp = pGen->pEnd;` |
|    15793 | 1815 | `	if( pSplit ){` |
|        - | 1816 | `		/* yield $key => $value */` |
|    15483 | 1817 | `		pGen->pEnd = pSplit;` |
|    15483 | 1818 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|    15483 | 1819 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|    15483 | 1820 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|    15483 | 1821 | `		pGen->pEnd = pTmp;` |
|    15483 | 1822 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|    15483 | 1823 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|    15483 | 1824 | `		iP1 = 1;` |
|    15483 | 1825 | `		iP2 = 1;` |
|     7744 | 1826 | `	}else{` |
|        - | 1827 | `		/* yield $value */` |
|      315 | 1828 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      315 | 1829 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      315 | 1830 | `		if( rc != SXERR_EMPTY ){` |
|      315 | 1831 | `			iP1 = 1;` |
|      155 | 1832 | `		}` |
|        - | 1833 | `	}` |
|    15793 | 1834 | `	pGen->pEnd = pTmp;` |
|    15793 | 1835 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|    15793 | 1836 | `	return SXRET_OK;` |
|     7931 | 1837 | `}` |
|        - | 1838 | `/*` |
|        - | 1839 | ` * Compile the die/exit language construct.` |
|        - | 1840 | ` * The role of these constructs is to terminate execution of the script.` |
|        - | 1841 | ` * Shutdown functions will always be executed even if exit() is called.` |
|        - | 1842 | ` */` |
|       94 | 1843 | `PH7_PRIVATE sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|        5 | 1844 | `{` |
|       99 | 1845 | `	sxi32 nExpr = 0;` |
|        - | 1846 | `	sxi32 rc;` |
|        - | 1847 | `	/* Jump the die/exit keyword */` |
|       99 | 1848 | `	pGen->pIn++;` |
|       99 | 1849 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - | 1850 | `		/* Compile the expression */` |
|       99 | 1851 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       99 | 1852 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1853 | `			return SXERR_ABORT;` |
|       99 | 1854 | `		}else if(rc != SXERR_EMPTY ){` |
|       99 | 1855 | `			nExpr = 1;` |
|       47 | 1856 | `		}` |
|       47 | 1857 | `	}` |
|        - | 1858 | `	/* Emit the HALT instruction */` |
|       99 | 1859 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|       99 | 1860 | `	return SXRET_OK;` |
|       52 | 1861 | `}` |
|        - | 1862 | `/*` |
|        - | 1863 | ` * Compile the 'echo' language construct.` |
|        - | 1864 | ` */` |
|    17482 | 1865 | `PH7_PRIVATE sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|        5 | 1866 | `{` |
|    17487 | 1867 | `	SyToken *pTmp,*pNext = 0;` |
|    17487 | 1868 | `	sxu32 nLine = pGen->pIn->nLine;` |
|    17487 | 1869 | `	int nExpr = 0;      /* expressions actually compiled */` |
|    17487 | 1870 | `	int bExpectMore = 1;/* after 'echo' or a comma an expression is REQUIRED */` |
|        - | 1871 | `	sxi32 rc;` |
|        - | 1872 | `	/* Jump the 'echo' keyword */` |
|    17487 | 1873 | `	pGen->pIn++;` |
|        - | 1874 | `	/* Compile arguments one after one. php: a stray token in an echo list is` |
|        - | 1875 | ``	 * `... expecting "," or ";"` — set the closer for each argument's compile. */`` |
|    17487 | 1876 | `	pTmp = pGen->pEnd;` |
|        - | 1877 | `	{` |
|    17487 | 1878 | `	const char *zSaveEcho = pGen->zClauseCloser;` |
|    45469 | 1879 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|    27993 | 1880 | `		if( pGen->pIn < pNext ){` |
|    27993 | 1881 | `			pGen->pEnd = pNext;` |
|    27993 | 1882 | `			pGen->zClauseCloser = "\",\" or \";\"";` |
|    27993 | 1883 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|    27993 | 1884 | `			pGen->zClauseCloser = zSaveEcho;` |
|    27993 | 1885 | `			if( rc == SXERR_ABORT ){` |
|        6 | 1886 | `				return SXERR_ABORT;` |
|    27989 | 1887 | `			}else if( rc != SXERR_EMPTY ){` |
|        - | 1888 | `				/* Emit the consume instruction */` |
|    27965 | 1889 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    27965 | 1890 | `				nExpr++;` |
|    27965 | 1891 | `				bExpectMore = 0;` |
|    13980 | 1892 | `			}` |
|    13992 | 1893 | `		}` |
|        - | 1894 | `		/* Jump trailing commas (php: exactly one between expressions; a` |
|        - | 1895 | `		 * dangling or doubled comma is a parse error, enforced below) */` |
|    38501 | 1896 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|    10519 | 1897 | `			if( bExpectMore ){` |
|        - | 1898 | `				/* two commas in a row */` |
|        3 | 1899 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,pNext->nLine,` |
|        - | 1900 | `					"syntax error, unexpected token \",\"");` |
|        3 | 1901 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1902 | `			}` |
|    10517 | 1903 | `			bExpectMore = 1;` |
|    10517 | 1904 | `			pNext++;` |
|        5 | 1905 | `		}` |
|    27987 | 1906 | `		pGen->pIn = pNext;` |
|        5 | 1907 | `	}` |
|        - | 1908 | `	}` |
|        - | 1909 | `	/* Restore token stream */` |
|    17481 | 1910 | `	pGen->pEnd = pTmp;` |
|    17481 | 1911 | `	if( nExpr == 0 \|\| bExpectMore ){` |
|        - | 1912 | ``		/* `echo ;` or `echo expr, ;` — php rejects both */`` |
|       32 | 1913 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - | 1914 | `			"syntax error, unexpected token \";\"");` |
|       32 | 1915 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1916 | `	}` |
|    17453 | 1917 | `	return SXRET_OK;` |
|     8746 | 1918 | `}` |
|        - | 1919 | `/*` |
|        - | 1920 | ` * Compile the static statement.` |
|        - | 1921 | ` * According to the PHP language reference` |
|        - | 1922 | ` *  Another important feature of variable scoping is the static variable.` |
|        - | 1923 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|        - | 1924 | ` *  when program execution leaves this scope.` |
|        - | 1925 | ` *  Static variables also provide one way to deal with recursive functions.` |
|        - | 1926 | ` * Symisc eXtension.` |
|        - | 1927 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|        - | 1928 | ` *  the zend engine would allow only simple scalar value.` |
|        - | 1929 | ` *  Example` |
|        - | 1930 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|        - | 1931 | ` *    Refer to the official documentation for more information on this feature.` |
|        - | 1932 | ` */` |
|    11610 | 1933 | `PH7_PRIVATE sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|        5 | 1934 | `{` |
|        - | 1935 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|        - | 1936 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|        - | 1937 | `	GenBlock *pBlock;` |
|        - | 1938 | `	SyString *pName;` |
|        - | 1939 | `	char *zDup;` |
|        - | 1940 | `	sxu32 nLine;` |
|        - | 1941 | `	sxi32 rc;` |
|        - | 1942 | ``	/* `static function () {}` / `static fn () =>` at statement position is an`` |
|        - | 1943 | `	 * EXPRESSION statement (a bare static closure), not a static-variable` |
|        - | 1944 | `	 * declaration — hand it to the expression compiler (php accepts it). */` |
|    11610 | 1945 | `	if( &pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & PH7_TK_KEYWORD)` |
|     5811 | 1946 | `	 && (SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FUNCTION` |
|        1 | 1947 | `	  \|\| SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FN) ){` |
|        3 | 1948 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        3 | 1949 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1950 | `			return SXERR_ABORT;` |
|        3 | 1951 | `		}else if( rc != SXERR_EMPTY ){` |
|        3 | 1952 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        1 | 1953 | `		}` |
|        3 | 1954 | `		return SXRET_OK;` |
|        - | 1955 | `	}` |
|        - | 1956 | `	/* Jump the static keyword */` |
|    11613 | 1957 | `	nLine = pGen->pIn->nLine;` |
|    11613 | 1958 | `	pGen->pIn++;` |
|        - | 1959 | `	/* Extract the enclosing function if any */` |
|    11613 | 1960 | `	pBlock = pGen->pCurrent;` |
|    23221 | 1961 | `	while( pBlock ){` |
|    23221 | 1962 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|    11613 | 1963 | `			break;` |
|        - | 1964 | `		}` |
|        - | 1965 | `		/* Point to the upper block */` |
|    11613 | 1966 | `		pBlock = pBlock->pParent;` |
|        5 | 1967 | `	}` |
|    11613 | 1968 | `	if( pBlock == 0 ){` |
|        - | 1969 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|      ! 0 | 1970 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|        - | 1971 | ``			/* php: `static FOO;` is a syntax error naming FOO and expecting "::"`` |
|        - | 1972 | ``			 * (the parser is still open to `static::` at that point). */`` |
|      ! 0 | 1973 | `			rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"::\"");` |
|      ! 0 | 1974 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1975 | `				return SXERR_ABORT;` |
|        - | 1976 | `			}` |
|      ! 0 | 1977 | `			goto Synchronize;` |
|        - | 1978 | `		}` |
|        - | 1979 | `		/* Compile the expression holding the variable */` |
|      ! 0 | 1980 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      ! 0 | 1981 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1982 | `			return SXERR_ABORT;` |
|      ! 0 | 1983 | `		}else if( rc != SXERR_EMPTY ){` |
|        - | 1984 | `			/* Emit the POP instruction */` |
|      ! 0 | 1985 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      ! 0 | 1986 | `		}` |
|      ! 0 | 1987 | `		return SXRET_OK;` |
|        - | 1988 | `	}` |
|    11613 | 1989 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        - | 1990 | `	/* Make sure we are dealing with a valid statement */` |
|    11613 | 1991 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|    11606 | 1992 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - | 1993 | ``			/* php: `static FOO;` is a syntax error naming FOO and expecting "::"`` |
|        - | 1994 | ``			 * (the parser is still open to `static::` at that point). */`` |
|        3 | 1995 | `			rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"::\"");` |
|        3 | 1996 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1997 | `				return SXERR_ABORT;` |
|        - | 1998 | `			}` |
|        3 | 1999 | `			goto Synchronize;` |
|        - | 2000 | `	}` |
|    11611 | 2001 | `	pGen->pIn++;` |
|        - | 2002 | `	/* Extract variable name */` |
|    11611 | 2003 | `	pName = &pGen->pIn->sData;` |
|    11611 | 2004 | `	pGen->pIn++; /* Jump the var name */` |
|    11611 | 2005 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|      ! 0 | 2006 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 2007 | `		goto Synchronize;` |
|        - | 2008 | `	}` |
|        - | 2009 | `	/* Initialize the structure describing the static variable */` |
|    11611 | 2010 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|    11611 | 2011 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|        - | 2012 | `	/* Duplicate variable name */` |
|    11611 | 2013 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    11611 | 2014 | `	if( zDup == 0 ){` |
|      ! 0 | 2015 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 2016 | `		return SXERR_ABORT;` |
|        - | 2017 | `	}` |
|    11611 | 2018 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|        - | 2019 | `	/* Check if we have an expression to compile */` |
|    11611 | 2020 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|        - | 2021 | `		SySet *pInstrContainer;` |
|        - | 2022 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|        - | 2023 | `		 * Static variable can take any complex expression including function` |
|        - | 2024 | `		 * call as their initialization value.` |
|        - | 2025 | `		 * Example:` |
|        - | 2026 | `		 *		static $var = foo(1,4+5,bar());` |
|        - | 2027 | `		 */` |
|    11611 | 2028 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|        - | 2029 | `		/* Swap bytecode container */` |
|    11611 | 2030 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    11611 | 2031 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|        - | 2032 | `		/* Compile the expression */` |
|    11611 | 2033 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 2034 | `		/* Emit the done instruction */` |
|    11611 | 2035 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        - | 2036 | `		/* Restore default bytecode container */` |
|    11611 | 2037 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     5803 | 2038 | `	}` |
|        - | 2039 | `	/* Finally save the compiled static variable in the appropriate container */` |
|    11611 | 2040 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|    11611 | 2041 | `	return SXRET_OK;` |
|        1 | 2042 | `Synchronize:` |
|        - | 2043 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|        - | 2044 | `	 * statement.` |
|        - | 2045 | `	 */` |
|        5 | 2046 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|        3 | 2047 | `		pGen->pIn++;` |
|        1 | 2048 | `	}` |
|        3 | 2049 | `	return SXRET_OK;` |
|     5810 | 2050 | `}` |
|        - | 2051 | `/*` |
|        - | 2052 | ` * Compile the var statement.` |
|        - | 2053 | ` * Symisc Extension:` |
|        - | 2054 | ` *      var statement can be used outside of a class definition.` |
|        - | 2055 | ` */` |
|        2 | 2056 | `PH7_PRIVATE sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|        1 | 2057 | `{` |
|        - | 2058 | `` 	/* `var` is a PROPERTY modifier and nothing else. A class body's `var $x;` `` |
|        - | 2059 | `	 * is compiled by compile_class.c, never here, so reaching this statement` |
|        - | 2060 | ``	 * handler means `var` appeared outside a class — which php rejects as a`` |
|        - | 2061 | `	 * parse error. PHL used to compile it as an ordinary expression statement,` |
|        - | 2062 | ``	 * so top-level `var $x = 1;` silently ran (a Symisc extension). */`` |
|        3 | 2063 | `	PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|        3 | 2064 | `	return SXERR_ABORT;` |
|        1 | 2065 | `}` |
|        - | 2066 | `/*` |
|        - | 2067 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|        - | 2068 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|        - | 2069 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|        - | 2070 | ` */` |
|        - | 2071 | `/*` |
|        - | 2072 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|        - | 2073 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|        - | 2074 | ` * hash and any shared references), this creates a new literal entry with the` |
|        - | 2075 | ` * qualified name and updates the instruction's operand index.` |
|        - | 2076 | ` *` |
|        - | 2077 | ` * Resolution order:` |
|        - | 2078 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|        - | 2079 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|        - | 2080 | ` *   3. Otherwise return the original literal index unchanged.` |
|        - | 2081 | ` *` |
|        - | 2082 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|        - | 2083 | ` * came from an import (step 1) and 0 otherwise.` |
|        - | 2084 | ` * Returns the (possibly new) literal index.` |
|        - | 2085 | ` */` |
|  6409208 | 2086 | `PH7_PRIVATE sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|        5 | 2087 | `{` |
|        - | 2088 | `	ph7_value *pLit;` |
|        - | 2089 | `	const char *zLit;` |
|        - | 2090 | `	SyString sQualified;` |
|        - | 2091 | `	sxu32 nLit;` |
|        - | 2092 | `	sxu32 k;` |
|        - | 2093 | `	sxu32 nNewIdx;` |
|        - | 2094 | `	int hasNsSep;` |
|        - | 2095 | `	SyHashEntry *pImport;` |
|        - | 2096 | `	ph7_value *pNew;` |
|  6409213 | 2097 | `	if( pFromImport ){` |
|  5225811 | 2098 | `		*pFromImport = 0;` |
|  2612903 | 2099 | `	}` |
|  6409213 | 2100 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  6409213 | 2101 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|      ! 0 | 2102 | `		return nOrigIdx;` |
|        - | 2103 | `	}` |
|  6409213 | 2104 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  6409213 | 2105 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|        - | 2106 | `	/* Skip if already qualified (contains backslash) */` |
|  6409213 | 2107 | `	hasNsSep = 0;` |
| 77497987 | 2108 | `	for( k = 0; k < nLit; k++ ){` |
| 71088805 | 2109 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 35544392 | 2110 | `	}` |
|  6409213 | 2111 | `	if( hasNsSep ){` |
|       28 | 2112 | `		return nOrigIdx;` |
|        - | 2113 | `	}` |
|        - | 2114 | `	/* Check use imports first (works even outside namespaces) */` |
|  6409187 | 2115 | `	SyBlobReset(&pGen->sWorker);` |
|  6409187 | 2116 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  6409187 | 2117 | `	if( pImport ){` |
|       41 | 2118 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       41 | 2119 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|       41 | 2120 | `		if( pFromImport ){` |
|       18 | 2121 | `			*pFromImport = 1;` |
|        8 | 2122 | `		}` |
|       23 | 2123 | `	}else{` |
|  6409151 | 2124 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  6409013 | 2125 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|        - | 2126 | `		}` |
|        - | 2127 | `		/* Prepend current namespace */` |
|      143 | 2128 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      143 | 2129 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      143 | 2130 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|        - | 2131 | `	}` |
|        - | 2132 | `	/* Look up or create a new literal for the qualified name */` |
|      179 | 2133 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|      179 | 2134 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|       79 | 2135 | `		return nNewIdx; /* Already interned */` |
|        - | 2136 | `	}` |
|      105 | 2137 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      105 | 2138 | `	if( pNew == 0 ){` |
|      ! 0 | 2139 | `		return nOrigIdx; /* OOM, fall back to original */` |
|        - | 2140 | `	}` |
|      105 | 2141 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      105 | 2142 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      105 | 2143 | `	return nNewIdx;` |
|  3204609 | 2144 | `}` |
|        - | 2145 | `/*` |
|        - | 2146 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|        - | 2147 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|        - | 2148 | ` */` |
|   550922 | 2149 | `PH7_PRIVATE void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|        5 | 2150 | `{` |
|        - | 2151 | `	SyHashEntry *pImport;` |
|   550927 | 2152 | `	const char *zName = pName->zString;` |
|   550927 | 2153 | `	sxu32 nName = pName->nByte;` |
|   550927 | 2154 | `	sxu32 nFirst = 0;` |
|        - | 2155 | `	/* php resolves a name through use-imports on its LEADING segment: an` |
|        - | 2156 | ``	 * unqualified `C` maps the whole name (`use X\C;` -> X\C), while a QUALIFIED`` |
|        - | 2157 | ``	 * `A\B\C` maps just `A` (`use X\A;` -> X\A\B\C) and keeps the `\B\C` tail.`` |
|        - | 2158 | `	 * The old code looked up the whole qualified string (which never matches a` |
|        - | 2159 | `	 * single-segment import alias) and then blindly prefixed the current` |
|        - | 2160 | ``	 * namespace, so `use PHPUnit\Framework; extends Framework\TestCase` resolved`` |
|        - | 2161 | `	 * to Ns\Framework\TestCase. Mirror GenStateResolveNamespaceLiteral here. */` |
|  7005659 | 2162 | `	while( nFirst < nName && zName[nFirst] != '\\' ){ nFirst++; }` |
|   550927 | 2163 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)zName,nFirst);` |
|   550927 | 2164 | `	if( pImport ){` |
|       26 | 2165 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       26 | 2166 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|       26 | 2167 | `		SyBlobAppend(pOut,zName + nFirst,nName - nFirst); /* the \B\C tail, if any */` |
|       26 | 2168 | `		return;` |
|        - | 2169 | `	}` |
|        - | 2170 | `	/* Prepend current namespace if active */` |
|   550905 | 2171 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       14 | 2172 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       14 | 2173 | `		SyBlobAppend(pOut,"\\",1);` |
|        6 | 2174 | `	}` |
|   550905 | 2175 | `	SyBlobAppend(pOut,zName,nName);` |
|   275466 | 2176 | `}` |
|        - | 2177 | `/*` |
|        - | 2178 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|        - | 2179 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|        - | 2180 | ` * The caller must release pOut when done.` |
|        - | 2181 | ` */` |
|   528032 | 2182 | `PH7_PRIVATE void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|        5 | 2183 | `{` |
|   528037 | 2184 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     3959 | 2185 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     3959 | 2186 | `		SyBlobAppend(pOut,"\\",1);` |
|     1977 | 2187 | `	}` |
|   528037 | 2188 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   528037 | 2189 | `}` |
|        - | 2190 | `/*` |
|        - | 2191 | ` * Compile a namespace statement` |
|        - | 2192 | ` * According to the PHP language reference manual` |
|        - | 2193 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|        - | 2194 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|        - | 2195 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|        - | 2196 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|        - | 2197 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|        - | 2198 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|        - | 2199 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|        - | 2200 | ` *  programming world.` |
|        - | 2201 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|        - | 2202 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|        - | 2203 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|        - | 2204 | ` *  classes/functions/constants.` |
|        - | 2205 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|        - | 2206 | ` *  readability of source code.` |
|        - | 2207 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|        - | 2208 | ` *  Here is an example of namespace syntax in PHP:` |
|        - | 2209 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|        - | 2210 | ` *       class MyClass {}` |
|        - | 2211 | ` *       function myfunction() {}` |
|        - | 2212 | ` *       const MYCONST = 1;` |
|        - | 2213 | ` *       $a = new MyClass;` |
|        - | 2214 | ` *       $c = new \my\name\MyClass;` |
|        - | 2215 | ` *       $a = strlen('hi');` |
|        - | 2216 | ` *       $d = namespace\MYCONST;` |
|        - | 2217 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|        - | 2218 | ` *       echo constant($d);` |
|        - | 2219 | ` * NOTE` |
|        - | 2220 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|        - | 2221 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|        - | 2222 | ` */` |
|        - | 2223 | `/*` |
|        - | 2224 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|        - | 2225 | ` */` |
|       14 | 2226 | `PH7_PRIVATE const char * TokenTypeName(sxu32 nType)` |
|        4 | 2227 | `{` |
|       18 | 2228 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|       11 | 2229 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|       11 | 2230 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|       11 | 2231 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|       11 | 2232 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|       11 | 2233 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|        3 | 2234 | `	return "token";` |
|       11 | 2235 | `}` |
|     3998 | 2236 | `PH7_PRIVATE sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|        5 | 2237 | `{` |
|        - | 2238 | `	sxu32 nLine;` |
|        - | 2239 | `	sxi32 rc;` |
|     4003 | 2240 | `	nLine = pGen->pIn->nLine;` |
|     4003 | 2241 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|        - | 2242 | `	/* Reset namespace and clear previous use imports */` |
|     4003 | 2243 | `	SyBlobReset(&pGen->sNamespace);` |
|     4003 | 2244 | `	SyHashRelease(&pGen->hUseImports);` |
|     4003 | 2245 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|     4003 | 2246 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     4003 | 2247 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|     4003 | 2248 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     4003 | 2249 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|     4003 | 2250 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - | 2251 | `		/* Global namespace (bare "namespace;") */` |
|      ! 0 | 2252 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|      ! 0 | 2253 | `		return SXRET_OK;` |
|        - | 2254 | `	}` |
|     4003 | 2255 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|        - | 2256 | `		/* namespace; — switch to global namespace */` |
|      ! 0 | 2257 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|      ! 0 | 2258 | `		return SXRET_OK;` |
|        - | 2259 | `	}` |
|     4003 | 2260 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|        - | 2261 | `		/* namespace { } — global namespace block */` |
|        5 | 2262 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|        5 | 2263 | `		return SXRET_OK;` |
|        - | 2264 | `	}` |
|        - | 2265 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     8075 | 2266 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     4081 | 2267 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|        - | 2268 | `			/* Append backslash separator */` |
|       46 | 2269 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       46 | 2270 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|       21 | 2271 | `			}` |
|       25 | 2272 | `		}else{` |
|        - | 2273 | `			/* Append identifier */` |
|     4039 | 2274 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        - | 2275 | `		}` |
|     4081 | 2276 | `		pGen->pIn++;` |
|        5 | 2277 | `	}` |
|        - | 2278 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|        - | 2279 | `	 * at the correct program counter, not just the last one compiled. */` |
|        - | 2280 | `	{` |
|     3999 | 2281 | `		char *zNsDup = 0;` |
|     3999 | 2282 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     5993 | 2283 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     3992 | 2284 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     1996 | 2285 | `		}` |
|     3999 | 2286 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|        - | 2287 | `	}` |
|     3999 | 2288 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        8 | 2289 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - | 2290 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|        4 | 2291 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        6 | 2292 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2293 | `			return SXERR_ABORT;` |
|        - | 2294 | `		}` |
|        2 | 2295 | `	}` |
|     3999 | 2296 | `	return SXRET_OK;` |
|     2004 | 2297 | `}` |
|        - | 2298 | `/*` |
|        - | 2299 | ` * Compile the 'use' statement` |
|        - | 2300 | ` * According to the PHP language reference manual` |
|        - | 2301 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|        - | 2302 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|        - | 2303 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|        - | 2304 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|        - | 2305 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|        - | 2306 | ` *  a function or constant is not supported.` |
|        - | 2307 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|        - | 2308 | ` * NOTE` |
|        - | 2309 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|        - | 2310 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|        - | 2311 | ` */` |
|       80 | 2312 | `PH7_PRIVATE sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|        5 | 2313 | `{` |
|        - | 2314 | `	sxu32 nLine;` |
|        - | 2315 | `	sxi32 rc;` |
|        - | 2316 | `	SyBlob sPath;` |
|        - | 2317 | `	SyString sAlias;` |
|        - | 2318 | `	SyToken *pLast;` |
|        - | 2319 | `	char *zDup;` |
|        - | 2320 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|        - | 2321 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|        - | 2322 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|       85 | 2323 | `	nLine = pGen->pIn->nLine;` |
|       85 | 2324 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|        - | 2325 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|       85 | 2326 | `	iUseType = 0;` |
|       85 | 2327 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       30 | 2328 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|       30 | 2329 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|       16 | 2330 | `			iUseType = 1;` |
|       16 | 2331 | `			pGen->pIn++;` |
|       23 | 2332 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|       16 | 2333 | `			iUseType = 2;` |
|       16 | 2334 | `			pGen->pIn++;` |
|        7 | 2335 | `		}` |
|       14 | 2336 | `	}` |
|        - | 2337 | `	/* Select target hash tables based on import type */` |
|       85 | 2338 | `	switch( iUseType ){` |
|        7 | 2339 | `		case 1:` |
|       16 | 2340 | `			pGenHash = &pGen->hUseFuncImports;` |
|       16 | 2341 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|       16 | 2342 | `			break;` |
|        7 | 2343 | `		case 2:` |
|       16 | 2344 | `			pGenHash = &pGen->hUseConstImports;` |
|       16 | 2345 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|       16 | 2346 | `			break;` |
|       26 | 2347 | `		default:` |
|       57 | 2348 | `			pGenHash = &pGen->hUseImports;` |
|       57 | 2349 | `			pVmHash = &pGen->pVm->hUseImports;` |
|       52 | 2350 | `			break;` |
|        - | 2351 | `	}` |
|       85 | 2352 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|        - | 2353 | `	/* Process one or more use declarations separated by commas */` |
|       41 | 2354 | `	for(;;){` |
|       87 | 2355 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 2356 | `			break;` |
|        - | 2357 | `		}` |
|       87 | 2358 | `		SyBlobReset(&sPath);` |
|       87 | 2359 | `		pLast = 0;` |
|        - | 2360 | `		/* Collect the full namespace path */` |
|      301 | 2361 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|      219 | 2362 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|      151 | 2363 | `				pLast = pGen->pIn;` |
|      151 | 2364 | `				if( SyBlobLength(&sPath) > 0 ){` |
|       73 | 2365 | `					SyBlobAppend(&sPath,"\\",1);` |
|       34 | 2366 | `				}` |
|      151 | 2367 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       73 | 2368 | `			}` |
|      219 | 2369 | `			pGen->pIn++;` |
|        5 | 2370 | `		}` |
|       87 | 2371 | `		if( pLast == 0 ){` |
|        - | 2372 | `			/* Empty path */` |
|        6 | 2373 | `			break;` |
|        - | 2374 | `		}` |
|        - | 2375 | `		/* Default alias is the last component of the path */` |
|       83 | 2376 | `		sAlias = pLast->sData;` |
|        - | 2377 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|       78 | 2378 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       56 | 2379 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|       27 | 2380 | `			pGen->pIn++; /* Jump 'as' */` |
|       27 | 2381 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|       27 | 2382 | `				sAlias = pGen->pIn->sData;` |
|       27 | 2383 | `				pGen->pIn++;` |
|       12 | 2384 | `			}` |
|       12 | 2385 | `		}` |
|        - | 2386 | `		/* Check for duplicate import alias (per-type) */` |
|       83 | 2387 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|        8 | 2388 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 2389 | `				"Cannot use %.*s as %z because the name is already in use",` |
|        4 | 2390 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|        6 | 2391 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2392 | `				SyBlobRelease(&sPath);` |
|      ! 0 | 2393 | `				return SXERR_ABORT;` |
|        - | 2394 | `			}` |
|        2 | 2395 | `		}` |
|        - | 2396 | `		/* Register the import: alias -> FQN.` |
|        - | 2397 | `		 * Strings are allocated from the VM pool allocator and freed` |
|        - | 2398 | `		 * when the entire VM is released. SyHashRelease does not free` |
|        - | 2399 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|      122 | 2400 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       78 | 2401 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|       83 | 2402 | `		if( zDup ){` |
|       83 | 2403 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|       83 | 2404 | `			if( pVmHash ){` |
|        - | 2405 | `				/* Class imports: populate VM table directly (class resolution` |
|        - | 2406 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|       55 | 2407 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       55 | 2408 | `				if( zAliasDup ){` |
|       55 | 2409 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|       25 | 2410 | `				}` |
|       25 | 2411 | `			}` |
|       83 | 2412 | `			if( iUseType == 2 ){` |
|        - | 2413 | `				/* Const imports: emit a runtime instruction so imports are` |
|        - | 2414 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|       16 | 2415 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       16 | 2416 | `				if( zAliasDup ){` |
|        - | 2417 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|        - | 2418 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|        - | 2419 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|       16 | 2420 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|       16 | 2421 | `					if( azPair ){` |
|       16 | 2422 | `						azPair[0] = zAliasDup;` |
|       16 | 2423 | `						azPair[1] = zDup;` |
|       16 | 2424 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|        7 | 2425 | `					}` |
|        7 | 2426 | `				}` |
|        7 | 2427 | `			}` |
|       39 | 2428 | `		}` |
|        - | 2429 | `		/* Check for comma (multiple use declarations) */` |
|       83 | 2430 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|        3 | 2431 | `			pGen->pIn++;` |
|        2 | 2432 | `		}else{` |
|       43 | 2433 | `			break;` |
|        - | 2434 | `		}` |
|        1 | 2435 | `	}` |
|       85 | 2436 | `	SyBlobRelease(&sPath);` |
|       85 | 2437 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        4 | 2438 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|        2 | 2439 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        3 | 2440 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2441 | `			return SXERR_ABORT;` |
|        - | 2442 | `		}` |
|        1 | 2443 | `	}` |
|       85 | 2444 | `	return SXRET_OK;` |
|       45 | 2445 | `}` |
|        - | 2446 | `/*` |
|        - | 2447 | ` * Compile the stupid 'declare' language construct.` |
|        - | 2448 | ` *` |
|        - | 2449 | ` * According to the PHP language reference manual.` |
|        - | 2450 | ` *  The declare construct is used to set execution directives for a block of code.` |
|        - | 2451 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|        - | 2452 | ` *  declare (directive)` |
|        - | 2453 | ` *   statement` |
|        - | 2454 | ` * The directive section allows the behavior of the declare block to be set.` |
|        - | 2455 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|        - | 2456 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|        - | 2457 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|        - | 2458 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|        - | 2459 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|        - | 2460 | ` * <?php` |
|        - | 2461 | ` * // these are the same:` |
|        - | 2462 | ` * // you can use this:` |
|        - | 2463 | ` * declare(ticks=1) {` |
|        - | 2464 | ` *   // entire script here` |
|        - | 2465 | ` * }` |
|        - | 2466 | ` * // or you can use this:` |
|        - | 2467 | ` * declare(ticks=1);` |
|        - | 2468 | ` * // entire script here` |
|        - | 2469 | ` * ?>` |
|        - | 2470 | ` *` |
|        - | 2471 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|        - | 2472 | ` */` |
|        - | 2473 | `/*` |
|        - | 2474 | ` * Match a directive name against a known literal (case-insensitive).` |
|        - | 2475 | ` */` |
|       80 | 2476 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|        5 | 2477 | `{` |
|      120 | 2478 | `	return SyStringLength(pName) == nWant` |
|       80 | 2479 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|        5 | 2480 | `}` |
|        - | 2481 |  |
|       44 | 2482 | `PH7_PRIVATE sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|        5 | 2483 | `{` |
|       49 | 2484 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       49 | 2485 | `	SyToken *pBodyEnd = 0;` |
|        - | 2486 | `	SyToken *pBodyStart;` |
|        - | 2487 | `	SyToken *pCursor;` |
|        - | 2488 | `	int bHasStrictTypes;` |
|        - | 2489 | `	int bBlockForm;` |
|        - | 2490 | `	int bPlacementOk;` |
|        - | 2491 | `	sxi32 rc;` |
|       49 | 2492 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|       49 | 2493 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|        6 | 2494 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|        6 | 2495 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2496 | `			return SXERR_ABORT;` |
|        - | 2497 | `		}` |
|        6 | 2498 | `		goto Synchro;` |
|        - | 2499 | `	}` |
|       45 | 2500 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|       45 | 2501 | `	pBodyStart = pGen->pIn;` |
|        - | 2502 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|       45 | 2503 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|       45 | 2504 | `	if( pBodyEnd >= pGen->pEnd ){` |
|      ! 0 | 2505 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\")\"");` |
|      ! 0 | 2506 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2507 | `			return SXERR_ABORT;` |
|        - | 2508 | `		}` |
|      ! 0 | 2509 | `		return SXRET_OK;` |
|        - | 2510 | `	}` |
|        - | 2511 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|        - | 2512 | `	 * now delimits the comma-separated directive list. */` |
|       45 | 2513 | `	pGen->pIn = &pBodyEnd[1];` |
|       45 | 2514 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      ! 0 | 2515 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|      ! 0 | 2516 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2517 | `			return SXERR_ABORT;` |
|        - | 2518 | `		}` |
|      ! 0 | 2519 | `	}` |
|       45 | 2520 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|       45 | 2521 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|       45 | 2522 | `	bHasStrictTypes = 0;` |
|        - | 2523 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|        - | 2524 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|        - | 2525 | `	 * directive appears anywhere in the list, before validating values. */` |
|       45 | 2526 | `	pCursor = pBodyStart;` |
|       57 | 2527 | `	while( pCursor < pBodyEnd ){` |
|       53 | 2528 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|       45 | 2529 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|       41 | 2530 | `				bHasStrictTypes = 1;` |
|       41 | 2531 | `				break;` |
|        - | 2532 | `			}` |
|        2 | 2533 | `		}` |
|       14 | 2534 | `		pCursor++;` |
|        2 | 2535 | `	}` |
|       45 | 2536 | `	if( bHasStrictTypes && bBlockForm ){` |
|        3 | 2537 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2538 | `			"strict_types declaration must not use block mode");` |
|        3 | 2539 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        3 | 2540 | `		return SXRET_OK;` |
|        - | 2541 | `	}` |
|       43 | 2542 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|        6 | 2543 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2544 | `			"strict_types declaration must be the very first statement in the script");` |
|        6 | 2545 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        6 | 2546 | `		return SXRET_OK;` |
|        - | 2547 | `	}` |
|        - | 2548 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|       39 | 2549 | `	pCursor = pBodyStart;` |
|       73 | 2550 | `	while( pCursor < pBodyEnd ){` |
|        - | 2551 | `		SyToken *pNameTok;` |
|        - | 2552 | `		SyToken *pEqTok;` |
|        - | 2553 | `		SyToken *pValTok;` |
|        - | 2554 | `		SyString *pDirName;` |
|        - | 2555 | `		int bIsStrict;` |
|        - | 2556 | `		int iStrictValue;` |
|       41 | 2557 | `		pNameTok = pCursor;` |
|       41 | 2558 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 2559 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2560 | `				"declare: Expecting a directive name");` |
|      ! 0 | 2561 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2562 | `			return SXRET_OK;` |
|        - | 2563 | `		}` |
|       41 | 2564 | `		pEqTok = pNameTok + 1;` |
|       41 | 2565 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|      ! 0 | 2566 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2567 | `				"declare: Expecting '=' after directive name");` |
|      ! 0 | 2568 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2569 | `			return SXRET_OK;` |
|        - | 2570 | `		}` |
|       41 | 2571 | `		pValTok = pEqTok + 1;` |
|       41 | 2572 | `		if( pValTok >= pBodyEnd ){` |
|      ! 0 | 2573 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2574 | `				"declare: Expecting value after '='");` |
|      ! 0 | 2575 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2576 | `			return SXRET_OK;` |
|        - | 2577 | `		}` |
|       41 | 2578 | `		pDirName = &pNameTok->sData;` |
|       41 | 2579 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|       41 | 2580 | `		if( bIsStrict ){` |
|        - | 2581 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|        - | 2582 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|       37 | 2583 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      ! 0 | 2584 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2585 | `					"declare(strict_types) value must be a literal");` |
|      ! 0 | 2586 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2587 | `				return SXRET_OK;` |
|        - | 2588 | `			}` |
|       37 | 2589 | `			iStrictValue = -1;` |
|       37 | 2590 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|       37 | 2591 | `				const char *zv = SyStringData(&pValTok->sData);` |
|       37 | 2592 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|       37 | 2593 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|       35 | 2594 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|       16 | 2595 | `			}` |
|       37 | 2596 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|        3 | 2597 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2598 | `					"strict_types declaration must have 0 or 1 as its value");` |
|        3 | 2599 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        3 | 2600 | `				return SXRET_OK;` |
|        - | 2601 | `			}` |
|       35 | 2602 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|       21 | 2603 | `		}else if( DeclareNameIs(pDirName, "encoding", sizeof("encoding")-1) ){` |
|        - | 2604 | `			/* php always ignores declare(encoding=...) unless it was built with` |
|        - | 2605 | `			 * Zend multibyte, and says so in these exact words. */` |
|        3 | 2606 | `			PH7_GenCompileError(&(*pGen),E_WARNING,nLine,` |
|        - | 2607 | `				"declare(encoding=...) ignored because Zend multibyte feature is turned off by settings");` |
|        1 | 2608 | `		}else{` |
|        - | 2609 | `			/* Other directives (ticks and friends) are accepted as no-ops.` |
|        - | 2610 | `			 * This used to emit a NOTICE naming the upstream engine and its` |
|        - | 2611 | `			 * version ("the declare construct is a no-op in the current release` |
|        - | 2612 | `			 * of the PH7(2.1.4) engine") — php prints nothing at all for` |
|        - | 2613 | ``			 * `declare(ticks=1)`, and leaking the old engine's branding into`` |
|        - | 2614 | `			 * user-visible diagnostics was wrong on its own. */` |
|        - | 2615 | `		}` |
|       39 | 2616 | `		pCursor = pValTok + 1;` |
|        - | 2617 | `		/* Consume separating comma (or end). */` |
|       39 | 2618 | `		if( pCursor < pBodyEnd ){` |
|        3 | 2619 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|      ! 0 | 2620 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2621 | `					"declare: Expecting ',' or ')' after directive value");` |
|      ! 0 | 2622 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2623 | `				return SXRET_OK;` |
|        - | 2624 | `			}` |
|        3 | 2625 | `			pCursor++;` |
|        1 | 2626 | `		}` |
|        5 | 2627 | `	}` |
|        - | 2628 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|        - | 2629 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|        - | 2630 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|       37 | 2631 | `	return SXRET_OK;` |
|        2 | 2632 | `Synchro:` |
|        - | 2633 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|       16 | 2634 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       12 | 2635 | `		pGen->pIn++;` |
|        2 | 2636 | `	}` |
|        6 | 2637 | `	return SXRET_OK;` |
|       27 | 2638 | `}` |
|        - | 2639 | `/*` |
|        - | 2640 | ` * Compile a class constant.` |
|        - | 2641 | ` * According to the PHP language reference manual` |
|        - | 2642 | ` *  Class Constants` |
|        - | 2643 | ` *   It is possible to define constant values on a per-class basis remaining` |
|        - | 2644 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|        - | 2645 | ` *   you don't use the $ symbol to declare or use them.` |
|        - | 2646 | ` *   The value must be a constant expression, not (for example) a variable,` |
|        - | 2647 | ` *   a property, a result of a mathematical operation, or a function call.` |
|        - | 2648 | ` *   It's also possible for interfaces to have constants.` |
|        - | 2649 | ` * Symisc eXtension.` |
|        - | 2650 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|        - | 2651 | ` *  the zend engine would allow only simple scalar value.` |
|        - | 2652 | ` *  Example:` |
|        - | 2653 | ` *   class Test{` |
|        - | 2654 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|        - | 2655 | ` *   };` |
|        - | 2656 | ` *   var_dump(TEST::MyConst);` |
|        - | 2657 | ` *   Refer to the official documentation for more information on the powerful extension` |
|        - | 2658 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|        - | 2659 | ` */` |
|        - | 2660 | `/*` |
|        - | 2661 | ` * Exception handling.` |
|        - | 2662 | ` *  According to the PHP language reference manual` |
|        - | 2663 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|        - | 2664 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|        - | 2665 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|        - | 2666 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|        - | 2667 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|        - | 2668 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|        - | 2669 | ` *    (or re-thrown) within a catch block.` |
|        - | 2670 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|        - | 2671 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|        - | 2672 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|        - | 2673 | ` *    been defined with set_exception_handler().` |
|        - | 2674 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|        - | 2675 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|        - | 2676 | ` */` |
|        - | 2677 | `/*` |
|        - | 2678 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|        - | 2679 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|        - | 2680 | ` * indicates failure.` |
|        - | 2681 | ` */` |
|   545542 | 2682 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 2683 | `{` |
|        - | 2684 | ``	/* php decides throwability at RUNTIME: `throw 5` compiles and raises the`` |
|        - | 2685 | `	 * catchable Error "Can only throw objects" when it executes, and a` |
|        - | 2686 | `	 * non-Throwable object raises "Cannot throw objects that do not implement` |
|        - | 2687 | `	 * Throwable". This used to whitelist a handful of expression shapes and` |
|        - | 2688 | `	 * reject the rest at compile time as a "friendlier" error, which meant the` |
|        - | 2689 | `	 * program never ran and no catch could ever see it — and it was inconsistent` |
|        - | 2690 | ``	 * anyway, since `throw $v` (a variable holding an int) always compiled and`` |
|        - | 2691 | `	 * fell through to the same runtime path. OP_THROW now owns the whole rule,` |
|        - | 2692 | ``	 * so accept any expression here; `throw;` with no operand is still rejected`` |
|        - | 2693 | `	 * by PH7_CompileThrow's SXERR_EMPTY branch. */` |
|   272771 | 2694 | `	SXUNUSED(pGen);` |
|   272771 | 2695 | `	SXUNUSED(pRoot);` |
|   545547 | 2696 | `	return SXRET_OK;` |
|        5 | 2697 | `}` |
|        - | 2698 | `/*` |
|        - | 2699 | ` * Compile a 'throw' statement.` |
|        - | 2700 | ` * throw: This is how you trigger an exception.` |
|        - | 2701 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|        - | 2702 | ` */` |
|   545506 | 2703 | `PH7_PRIVATE sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|        5 | 2704 | `{` |
|   545511 | 2705 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2706 | `	GenBlock *pBlock;` |
|        - | 2707 | `	sxu32 nIdx;` |
|        - | 2708 | `	sxi32 rc;` |
|   545511 | 2709 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|        - | 2710 | `	/* Compile the expression */` |
|   545511 | 2711 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|   545511 | 2712 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 | 2713 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|      ! 0 | 2714 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2715 | `			return SXERR_ABORT;` |
|        - | 2716 | `		}` |
|      ! 0 | 2717 | `		return SXRET_OK;` |
|        - | 2718 | `	}` |
|   545511 | 2719 | `	pBlock = pGen->pCurrent;` |
|        - | 2720 | `	/* Point to the top most function or try block and emit the forward jump */` |
|  2173515 | 2721 | `	while(pBlock->pParent){` |
|  2173509 | 2722 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|   545505 | 2723 | `			break;` |
|        - | 2724 | `		}` |
|        - | 2725 | `		/* Point to the parent block */` |
|  1628009 | 2726 | `		pBlock = pBlock->pParent;` |
|        5 | 2727 | `	}` |
|        - | 2728 | `	/* Emit the throw instruction */` |
|   545511 | 2729 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|        - | 2730 | `	/* Emit the jump */` |
|   545511 | 2731 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|   545511 | 2732 | `	return SXRET_OK;` |
|   272758 | 2733 | `}` |
|        - | 2734 | `/*` |
|        - | 2735 | ` * Compile a PHP 8.0 'throw' expression.` |
|        - | 2736 | ` * Called from the expression code generator when a 'throw' keyword is` |
|        - | 2737 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|        - | 2738 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|        - | 2739 | ` * the validator guarantees the operand is a valid exception target.` |
|        - | 2740 | ` */` |
|       36 | 2741 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|        2 | 2742 | `{` |
|       38 | 2743 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2744 | `	GenBlock *pBlock;` |
|        - | 2745 | `	sxu32 nIdx;` |
|        - | 2746 | `	sxi32 rc;` |
|       18 | 2747 | `	(void)iCompileFlag;` |
|       38 | 2748 | `	pGen->pIn++; /* Skip 'throw' */` |
|       38 | 2749 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 2750 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 2751 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 2752 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2753 | `			return SXERR_ABORT;` |
|        - | 2754 | `		}` |
|      ! 0 | 2755 | `		return SXRET_OK;` |
|        - | 2756 | `	}` |
|       38 | 2757 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|       38 | 2758 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 2759 | `		return SXERR_ABORT;` |
|        - | 2760 | `	}` |
|       38 | 2761 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 | 2762 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 2763 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 2764 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2765 | `			return SXERR_ABORT;` |
|        - | 2766 | `		}` |
|      ! 0 | 2767 | `		return SXRET_OK;` |
|        - | 2768 | `	}` |
|        - | 2769 | `	/* Walk up to nearest exception/function block for the jump target */` |
|       38 | 2770 | `	pBlock = pGen->pCurrent;` |
|       60 | 2771 | `	while( pBlock->pParent ){` |
|       49 | 2772 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|       27 | 2773 | `			break;` |
|        - | 2774 | `		}` |
|       23 | 2775 | `		pBlock = pBlock->pParent;` |
|        1 | 2776 | `	}` |
|       38 | 2777 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       38 | 2778 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|       38 | 2779 | `	return SXRET_OK;` |
|       20 | 2780 | `}` |
|        - | 2781 | `/*` |
|        - | 2782 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|        - | 2783 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|        - | 2784 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|        - | 2785 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|        - | 2786 | ` * compile error propagated from the parser.` |
|        - | 2787 | ` */` |
|       56 | 2788 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|        5 | 2789 | `{` |
|        - | 2790 | `	SyString sClassName;` |
|        - | 2791 | `	SyToken *pToken;` |
|        - | 2792 | `	SyString *pName;` |
|        - | 2793 | `	char *zDup;` |
|        - | 2794 | `	sxi32 rc;` |
|       61 | 2795 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       61 | 2796 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|       61 | 2797 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|       61 | 2798 | `	SySetInit(&pCatch->sByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       61 | 2799 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 | 2800 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 2801 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 2802 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 2803 | `		return SXERR_INVALID;` |
|        - | 2804 | `	}` |
|       61 | 2805 | `	pGen->pIn++; /* '(' */` |
|       28 | 2806 | `	for(;;){` |
|        - | 2807 | `		SyBlob sResolved;` |
|       61 | 2808 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       61 | 2809 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 2810 | `			SyBlobRelease(&sResolved);` |
|      ! 0 | 2811 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 2812 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 2813 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 2814 | `			return SXERR_INVALID;` |
|        - | 2815 | `		}` |
|       89 | 2816 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       56 | 2817 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       61 | 2818 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|       61 | 2819 | `		SyBlobRelease(&sResolved);` |
|       61 | 2820 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|       61 | 2821 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|       61 | 2822 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       56 | 2823 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|        5 | 2824 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|      ! 0 | 2825 | `			pGen->pIn++; continue;` |
|        - | 2826 | `		}` |
|       61 | 2827 | `		break;` |
|      ! 0 | 2828 | `	}` |
|        - | 2829 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|        - | 2830 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding. */` |
|       61 | 2831 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|        3 | 2832 | `		pGen->pIn++; /* ')' */` |
|        3 | 2833 | `		return SXRET_OK;` |
|        - | 2834 | `	}` |
|       54 | 2835 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|       59 | 2836 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 2837 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 2838 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 2839 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 2840 | `		return SXERR_INVALID;` |
|        - | 2841 | `	}` |
|       59 | 2842 | `	pGen->pIn++; /* '$' */` |
|       59 | 2843 | `	pName = &pGen->pIn->sData;` |
|       59 | 2844 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       59 | 2845 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|       59 | 2846 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|       59 | 2847 | `	pGen->pIn++;` |
|       59 | 2848 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 | 2849 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 2850 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 2851 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 2852 | `		return SXERR_INVALID;` |
|        - | 2853 | `	}` |
|       59 | 2854 | `	pGen->pIn++; /* ')' */` |
|       59 | 2855 | `	return SXRET_OK;` |
|       33 | 2856 | `}` |
|        - | 2857 | `/*` |
|        - | 2858 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|        - | 2859 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|        - | 2860 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|        - | 2861 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|        - | 2862 | ` * VmThrowException):` |
|        - | 2863 | ` *` |
|        - | 2864 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|        - | 2865 | ` *    <try body>` |
|        - | 2866 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|        - | 2867 | ` *    JMP  -> finally\|end` |
|        - | 2868 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|        - | 2869 | ` *    <catch body>` |
|        - | 2870 | ` *    JMP  -> finally\|end` |
|        - | 2871 | ` *    ... more catches ...` |
|        - | 2872 | ` *  Lfin: <finally body>` |
|        - | 2873 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|        - | 2874 | ` *  Lend:` |
|        - | 2875 | ` */` |
|      100 | 2876 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|        5 | 2877 | `{` |
|      105 | 2878 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2879 | `	GenBlock *pTry;` |
|        - | 2880 | `	VmInstr *pInstr;` |
|      105 | 2881 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|        - | 2882 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|        - | 2883 | `	sxi32 rc;` |
|      105 | 2884 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|        - | 2885 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION) */` |
|      105 | 2886 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|      105 | 2887 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      105 | 2888 | `	pTry->pUserData = pException;` |
|      105 | 2889 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|      105 | 2890 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|      105 | 2891 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      105 | 2892 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      105 | 2893 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|      105 | 2894 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|        - | 2895 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|      105 | 2896 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|      105 | 2897 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|      105 | 2898 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|      105 | 2899 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 2900 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|      105 | 2901 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|        - | 2902 | `	/* Catch clauses (inline) */` |
|      105 | 2903 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      100 | 2904 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       61 | 2905 | `		sxu32 k = 0;` |
|       84 | 2906 | `		for(;;){` |
|        - | 2907 | `			ph7_exception_block sCatch;` |
|        - | 2908 | `			GenBlock *pCatchBlk;` |
|      117 | 2909 | `			sxu32 idxJmp = 0;` |
|      112 | 2910 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      107 | 2911 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|       33 | 2912 | `				break;` |
|        - | 2913 | `			}` |
|       61 | 2914 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|       61 | 2915 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       61 | 2916 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|       61 | 2917 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|       61 | 2918 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|       61 | 2919 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatchBlk);` |
|       61 | 2920 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        - | 2921 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|        - | 2922 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|        - | 2923 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump). */` |
|       61 | 2924 | `			pCatchBlk->pUserData = pException;` |
|       61 | 2925 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|       61 | 2926 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       61 | 2927 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|       61 | 2928 | `			GenStateLeaveBlock(&(*pGen),0);` |
|        - | 2929 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|        - | 2930 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|       61 | 2931 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       61 | 2932 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|       61 | 2933 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|       61 | 2934 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|       61 | 2935 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       61 | 2936 | `			k++;` |
|        5 | 2937 | `		}` |
|       28 | 2938 | `	}` |
|        - | 2939 | `	/* Finally (inline) */` |
|      105 | 2940 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       80 | 2941 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|        - | 2942 | `		GenBlock *pFinBlk;` |
|       52 | 2943 | `		pGen->pIn++; /* Jump 'finally' */` |
|       52 | 2944 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|       52 | 2945 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|       52 | 2946 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       52 | 2947 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       52 | 2948 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       52 | 2949 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|       52 | 2950 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       52 | 2951 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|       52 | 2952 | `		pException->iHasFinally = 1;` |
|       24 | 2953 | `	}` |
|      105 | 2954 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|      105 | 2955 | `	pException->iInlined = 1;` |
|        - | 2956 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|        - | 2957 | `	{` |
|      105 | 2958 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|        - | 2959 | `		sxu32 *aJ; sxu32 n;` |
|      105 | 2960 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|      105 | 2961 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|      105 | 2962 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|      161 | 2963 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|       61 | 2964 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|       61 | 2965 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|       33 | 2966 | `		}` |
|        - | 2967 | `	}` |
|      105 | 2968 | `	SySetRelease(&aCatchJmp);` |
|      105 | 2969 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|      ! 0 | 2970 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|      ! 0 | 2971 | `	}` |
|      105 | 2972 | `	return SXRET_OK;` |
|       55 | 2973 | `}` |
|        - | 2974 | `/*` |
|        - | 2975 | ` * Compile a 'catch' block.` |
|        - | 2976 | ` * Catch: A "catch" block retrieves an exception and creates` |
|        - | 2977 | ` * an object containing the exception information.` |
|        - | 2978 | ` */` |
|    24844 | 2979 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|        5 | 2980 | `{` |
|    24849 | 2981 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2982 | `	ph7_exception_block sCatch;` |
|        - | 2983 | `	SySet *pInstrContainer;` |
|        - | 2984 | `	SyString sClassName;` |
|        - | 2985 | `	GenBlock *pCatch;` |
|        - | 2986 | `	SyToken *pToken;` |
|        - | 2987 | `	SyString *pName;` |
|        - | 2988 | `	char *zDup;` |
|        - | 2989 | `	sxi32 rc;` |
|    24849 | 2990 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|        - | 2991 | `	/* Zero the structure */` |
|    24849 | 2992 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|        - | 2993 | `	/* Initialize fields */` |
|    24849 | 2994 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|    24849 | 2995 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|    24849 | 2996 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|        - | 2997 | `			/* Unexpected token,break immediately */` |
|      ! 0 | 2998 | `			pToken = pGen->pIn;` |
|      ! 0 | 2999 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 3000 | `				pToken--;` |
|      ! 0 | 3001 | `			}` |
|      ! 0 | 3002 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 3003 | `				"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 3004 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 3005 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3006 | `				return SXERR_ABORT;` |
|        - | 3007 | `			}` |
|      ! 0 | 3008 | `			return SXERR_INVALID;` |
|        - | 3009 | `	}` |
|        - | 3010 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|    24849 | 3011 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|    12437 | 3012 | `	for(;;){` |
|        - | 3013 | `		SyBlob sResolved;` |
|    24879 | 3014 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    24879 | 3015 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|        6 | 3016 | `			SyBlobRelease(&sResolved);` |
|        6 | 3017 | `			pToken = pGen->pIn;` |
|        6 | 3018 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 3019 | `				pToken--;` |
|      ! 0 | 3020 | `			}` |
|        8 | 3021 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 3022 | `				"syntax error, unexpected %s \"%z\"",` |
|        2 | 3023 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|        6 | 3024 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3025 | `				return SXERR_ABORT;` |
|        - | 3026 | `			}` |
|        6 | 3027 | `			return SXERR_INVALID;` |
|        - | 3028 | `		}` |
|        - | 3029 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|        - | 3030 | `		 * transient SyBlob allocation. */` |
|    37310 | 3031 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    24870 | 3032 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|    24875 | 3033 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|    24875 | 3034 | `		SyBlobRelease(&sResolved);` |
|    24875 | 3035 | `		if( zDup == 0 ){` |
|      ! 0 | 3036 | `			goto Mem;` |
|        - | 3037 | `		}` |
|    24875 | 3038 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|    24875 | 3039 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 3040 | `			goto Mem;` |
|        - | 3041 | `		}` |
|        - | 3042 | `		/* Check for '\|' (multi-catch separator) */` |
|    24870 | 3043 | `		if( pGen->pIn < pGen->pEnd &&` |
|    24870 | 3044 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|       35 | 3045 | `			pGen->pIn->sData.nByte == 1 &&` |
|       30 | 3046 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|       32 | 3047 | `			pGen->pIn++; /* Consume the '\|' */` |
|       32 | 3048 | `			continue;` |
|        - | 3049 | `		}` |
|    24845 | 3050 | `		break;` |
|      ! 0 | 3051 | `	}` |
|        - | 3052 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|        - | 3053 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding;` |
|        - | 3054 | `	 * jump straight to compiling the block below. */` |
|    24845 | 3055 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) /*)*/ ){` |
|        7 | 3056 | `		goto CatchBody;` |
|        - | 3057 | `	}` |
|    24834 | 3058 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|    24839 | 3059 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - | 3060 | `			/* Unexpected token,break immediately */` |
|      ! 0 | 3061 | `			pToken = pGen->pIn;` |
|      ! 0 | 3062 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 3063 | `				pToken--;` |
|      ! 0 | 3064 | `			}` |
|      ! 0 | 3065 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 3066 | `				"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 3067 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 3068 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3069 | `				return SXERR_ABORT;` |
|        - | 3070 | `			}` |
|      ! 0 | 3071 | `			return SXERR_INVALID;` |
|        - | 3072 | `	}` |
|    24839 | 3073 | `	pGen->pIn++; /* Jump the dollar sign */` |
|        - | 3074 | `	/* Duplicate instance name */` |
|    24839 | 3075 | `	pName = &pGen->pIn->sData;` |
|    24839 | 3076 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    24839 | 3077 | `	if( zDup == 0 ){` |
|      ! 0 | 3078 | `		goto Mem;` |
|        - | 3079 | `	}` |
|    24839 | 3080 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|    24839 | 3081 | `	pGen->pIn++;` |
|    12420 | 3082 | `CatchBody:` |
|    24845 | 3083 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|        - | 3084 | `		/* Unexpected token,break immediately */` |
|      ! 0 | 3085 | `		pToken = pGen->pIn;` |
|      ! 0 | 3086 | `		if( pToken >= pGen->pEnd ){` |
|      ! 0 | 3087 | `			pToken--;` |
|      ! 0 | 3088 | `		}` |
|      ! 0 | 3089 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 3090 | `			"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 3091 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 3092 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3093 | `			return SXERR_ABORT;` |
|        - | 3094 | `		}` |
|      ! 0 | 3095 | `		return SXERR_INVALID;` |
|        - | 3096 | `	}` |
|        - | 3097 | `	/* Compile the block */` |
|    24845 | 3098 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|        - | 3099 | `	/* Create the catch block */` |
|    24845 | 3100 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|    24845 | 3101 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3102 | `		return SXERR_ABORT;` |
|        - | 3103 | `	}` |
|        - | 3104 | `	/* Swap bytecode container */` |
|    24845 | 3105 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    24845 | 3106 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|        - | 3107 | `	/* Compile the block */` |
|    24845 | 3108 | `	PH7_CompileBlock(&(*pGen),0);` |
|        - | 3109 | `	/* Fix forward jumps now the destination is resolved  */` |
|    24845 | 3110 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 3111 | `	/* Emit the DONE instruction */` |
|    24845 | 3112 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - | 3113 | `	/* Leave the block */` |
|    24845 | 3114 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 3115 | `	/* Restore the default container */` |
|    24845 | 3116 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - | 3117 | `	/* Install the catch block */` |
|    24845 | 3118 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|    24845 | 3119 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3120 | `		goto Mem;` |
|        - | 3121 | `	}` |
|    24845 | 3122 | `	return SXRET_OK;` |
|      ! 0 | 3123 | `Mem:` |
|      ! 0 | 3124 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 3125 | `	return SXERR_ABORT;` |
|    12427 | 3126 | `}` |
|        - | 3127 | `/*` |
|        - | 3128 | ` * Compile a 'try' block.` |
|        - | 3129 | ` * A function using an exception should be in a "try" block.` |
|        - | 3130 | ` * If the exception does not trigger, the code will continue` |
|        - | 3131 | ` * as normal. However if the exception triggers, an exception` |
|        - | 3132 | ` * is "thrown".` |
|        - | 3133 | ` */` |
|    25002 | 3134 | `PH7_PRIVATE sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|        5 | 3135 | `{` |
|        - | 3136 | `	ph7_exception *pException;` |
|    25007 | 3137 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 3138 | `	GenBlock *pTry;` |
|        - | 3139 | `	sxu32 nJmpIdx;` |
|        - | 3140 | `	sxi32 rc;` |
|        - | 3141 | `	/* Create the exception container */` |
|    25007 | 3142 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|    25007 | 3143 | `	if( pException == 0 ){` |
|      ! 0 | 3144 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|      ! 0 | 3145 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 3146 | `		return SXERR_ABORT;` |
|        - | 3147 | `	}` |
|        - | 3148 | `	/* Zero the structure */` |
|    25007 | 3149 | `	SyZero(pException,sizeof(ph7_exception));` |
|        - | 3150 | `	/* Initialize fields */` |
|    25007 | 3151 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|    25007 | 3152 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|    25007 | 3153 | `	pException->iHasFinally = 0;` |
|    25007 | 3154 | `	pException->iFinallyDone = 0;` |
|    25007 | 3155 | `	pException->pVm = pGen->pVm;` |
|        - | 3156 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|        - | 3157 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the legacy path.`` |
|        - | 3158 | `	 * DORMANT until the inline VM handlers (OP_CATCH / OP_END_FINALLY dispatch,` |
|        - | 3159 | `	 * VmThrowException pc-redirect, return/break-through-finally threading, generator` |
|        - | 3160 | `	 * park of aFinallyAction) land — the compiler emits the layout but the VM cannot yet` |
|        - | 3161 | `	 * execute it. Guarded by pVm->bInlineTryCatch (default 0) so the tree stays green. */` |
|    25007 | 3162 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|      105 | 3163 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|        - | 3164 | `	}` |
|        - | 3165 | `	/* Create the try block */` |
|    24907 | 3166 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|    24907 | 3167 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3168 | `		return SXERR_ABORT;` |
|        - | 3169 | `	}` |
|        - | 3170 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|    24907 | 3171 | `	pTry->pUserData = pException;` |
|        - | 3172 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|    24907 | 3173 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|        - | 3174 | `	/* Fix the jump later when the destination is resolved */` |
|    24907 | 3175 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|    24907 | 3176 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|        - | 3177 | `	/* Compile the block */` |
|    24907 | 3178 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|    24907 | 3179 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 3180 | `		return SXERR_ABORT;` |
|        - | 3181 | `	}` |
|        - | 3182 | `	/* Fix forward jumps now the destination is resolved */` |
|    24907 | 3183 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 3184 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|    24907 | 3185 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|        - | 3186 | `	/* Leave the block */` |
|    24907 | 3187 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 3188 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|    24907 | 3189 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|    24900 | 3190 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|        - | 3191 | `		/* Compile one or more catch blocks */` |
|    24840 | 3192 | `		for(;;){` |
|    49680 | 3193 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    37344 | 3194 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|    12423 | 3195 | `					break;` |
|        - | 3196 | `			}` |
|    24849 | 3197 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|    24849 | 3198 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3199 | `				return SXERR_ABORT;` |
|        - | 3200 | `			}` |
|        5 | 3201 | `		}` |
|    12418 | 3202 | `	}` |
|        - | 3203 | `	/* Compile optional finally block */` |
|    24907 | 3204 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      810 | 3205 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|        - | 3206 | `		SySet *pInstrContainer;` |
|        - | 3207 | `		GenBlock *pFinBlock;` |
|      129 | 3208 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|        - | 3209 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      129 | 3210 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      129 | 3211 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 3212 | `			return SXERR_ABORT;` |
|        - | 3213 | `		}` |
|        - | 3214 | `		/* Swap bytecode container */` |
|      129 | 3215 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      129 | 3216 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|        - | 3217 | `		/* Compile the finally body */` |
|      129 | 3218 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      129 | 3219 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3220 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 | 3221 | `			return SXERR_ABORT;` |
|        - | 3222 | `		}` |
|        - | 3223 | `		/* Fix forward jumps now the destination is resolved */` |
|      129 | 3224 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 3225 | `		/* Emit DONE to terminate the finally block */` |
|      129 | 3226 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - | 3227 | `		/* Leave the block */` |
|      129 | 3228 | `		GenStateLeaveBlock(&(*pGen),0);` |
|        - | 3229 | `		/* Restore the default container */` |
|      129 | 3230 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      129 | 3231 | `		pException->iHasFinally = 1;` |
|       62 | 3232 | `	}` |
|        - | 3233 | `	/* Must have at least one catch or finally */` |
|    24907 | 3234 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|        9 | 3235 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 3236 | `			"Cannot use try without catch or finally");` |
|        9 | 3237 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3238 | `			return SXERR_ABORT;` |
|        - | 3239 | `		}` |
|        3 | 3240 | `	}` |
|    24907 | 3241 | `	return SXRET_OK;` |
|    12506 | 3242 | `}` |
|        - | 3243 | `/*` |
|        - | 3244 | ` * Compile a switch block.` |
|        - | 3245 | ` *  (See block-comment below for more information)` |
|        - | 3246 | ` */` |
|   135422 | 3247 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|        5 | 3248 | `{` |
|   135427 | 3249 | `	sxi32 rc = SXRET_OK;` |
|   135427 | 3250 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|        - | 3251 | `		/* Unexpected token */` |
|      ! 0 | 3252 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 | 3253 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3254 | `			return SXERR_ABORT;` |
|        - | 3255 | `		}` |
|      ! 0 | 3256 | `		pGen->pIn++;` |
|      ! 0 | 3257 | `	}` |
|   135427 | 3258 | `	pGen->pIn++;` |
|        - | 3259 | `	/* First instruction to execute in this block. */` |
|   135427 | 3260 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|        - | 3261 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|        - | 3262 | `	 * or the '}' token */` |
|   129717 | 3263 | `	for(;;){` |
|   259439 | 3264 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 3265 | `			/* No more input to process */` |
|      ! 0 | 3266 | `			break;` |
|        - | 3267 | `		}` |
|   259439 | 3268 | `		rc = SXRET_OK;` |
|   259439 | 3269 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|    34879 | 3270 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|    11629 | 3271 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|        - | 3272 | `					/* Unexpected token */` |
|      ! 0 | 3273 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|      ! 0 | 3274 | `						&pGen->pIn->sData);` |
|      ! 0 | 3275 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3276 | `						return SXERR_ABORT;` |
|        - | 3277 | `					}` |
|        - | 3278 | `					/* FALL THROUGH */` |
|      ! 0 | 3279 | `				}` |
|    11629 | 3280 | `				rc = SXERR_EOF;` |
|    11629 | 3281 | `				break;` |
|        - | 3282 | `			}` |
|    11630 | 3283 | `		}else{` |
|        - | 3284 | `			sxi32 nKwrd;` |
|        - | 3285 | `			/* Extract the keyword */` |
|   224565 | 3286 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   224565 | 3287 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|    61903 | 3288 | `				break;` |
|        - | 3289 | `			}` |
|   100769 | 3290 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|        3 | 3291 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|        - | 3292 | `					/* Unexpected token */` |
|      ! 0 | 3293 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|      ! 0 | 3294 | `						&pGen->pIn->sData);` |
|      ! 0 | 3295 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3296 | `						return SXERR_ABORT;` |
|        - | 3297 | `					}` |
|        - | 3298 | `					/* FALL THROUGH */` |
|      ! 0 | 3299 | `				}` |
|        - | 3300 | `				/* Block compiled */` |
|        3 | 3301 | `				break;` |
|        - | 3302 | `			}` |
|        - | 3303 | `		}` |
|        - | 3304 | `		/* Compile block */` |
|   124017 | 3305 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|   124017 | 3306 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3307 | `			return SXERR_ABORT;` |
|        - | 3308 | `		}` |
|        5 | 3309 | `	}` |
|   135427 | 3310 | `	return rc;` |
|    67716 | 3311 | `}` |
|        - | 3312 | `/*` |
|        - | 3313 | ` * Compile a case eXpression.` |
|        - | 3314 | ` *  (See block-comment below for more information)` |
|        - | 3315 | ` */` |
|   131536 | 3316 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|        5 | 3317 | `{` |
|        - | 3318 | `	SySet *pInstrContainer;` |
|        - | 3319 | `	SyToken *pEnd,*pTmp;` |
|   131541 | 3320 | `	sxi32 iNest = 0;` |
|        - | 3321 | `	sxi32 rc;` |
|        - | 3322 | `	/* Delimit the expression */` |
|   131541 | 3323 | `	pEnd = pGen->pIn;` |
|   263085 | 3324 | `	while( pEnd < pGen->pEnd ){` |
|   263085 | 3325 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|        - | 3326 | `			/* Increment nesting level */` |
|        3 | 3327 | `			iNest++;` |
|   263084 | 3328 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|        - | 3329 | `			/* Decrement nesting level */` |
|        3 | 3330 | `			iNest--;` |
|   263082 | 3331 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|   131541 | 3332 | `			break;` |
|        - | 3333 | `		}` |
|   131549 | 3334 | `		pEnd++;` |
|        5 | 3335 | `	}` |
|   131541 | 3336 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 3337 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|      ! 0 | 3338 | `		if( rc == SXERR_ABORT ){` |
|        - | 3339 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 3340 | `			return SXERR_ABORT;` |
|        - | 3341 | `		}` |
|      ! 0 | 3342 | `	}` |
|        - | 3343 | `	/* Swap token stream */` |
|   131541 | 3344 | `	pTmp = pGen->pEnd;` |
|   131541 | 3345 | `	pGen->pEnd = pEnd;` |
|   131541 | 3346 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   131541 | 3347 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|   131541 | 3348 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 3349 | `	/* Emit the done instruction */` |
|   131541 | 3350 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   131541 | 3351 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - | 3352 | `	/* Update token stream */` |
|   131541 | 3353 | `	pGen->pIn  = pEnd;` |
|   131541 | 3354 | `	pGen->pEnd = pTmp;` |
|   131541 | 3355 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 3356 | `		return SXERR_ABORT;` |
|        - | 3357 | `	}` |
|   131541 | 3358 | `	return SXRET_OK;` |
|    65773 | 3359 | `}` |
|        - | 3360 | `/*` |
|        - | 3361 | ` * Compile the smart switch statement.` |
|        - | 3362 | ` * According to the PHP language reference manual` |
|        - | 3363 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|        - | 3364 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|        - | 3365 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|        - | 3366 | ` *  This is exactly what the switch statement is for.` |
|        - | 3367 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|        - | 3368 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|        - | 3369 | ` *  of the outer loop, use continue 2.` |
|        - | 3370 | ` *  Note that switch/case does loose comparision.` |
|        - | 3371 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|        - | 3372 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|        - | 3373 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|        - | 3374 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|        - | 3375 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|        - | 3376 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|        - | 3377 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|        - | 3378 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|        - | 3379 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|        - | 3380 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|        - | 3381 | ` *  list for the next case.` |
|        - | 3382 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|        - | 3383 | ` *  or floating-point numbers and strings.` |
|        - | 3384 | ` */` |
|    11626 | 3385 | `PH7_PRIVATE sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|        5 | 3386 | `{` |
|        - | 3387 | `	GenBlock *pSwitchBlock;` |
|        - | 3388 | `	SyToken *pTmp,*pEnd;` |
|        - | 3389 | `	ph7_switch *pSwitch;` |
|        - | 3390 | `	sxu32 nToken;` |
|        - | 3391 | `	sxu32 nLine;` |
|        - | 3392 | `	sxi32 rc;` |
|    11631 | 3393 | `	nLine = pGen->pIn->nLine;` |
|        - | 3394 | `	/* Jump the 'switch' keyword */` |
|    11631 | 3395 | `	pGen->pIn++;` |
|    11631 | 3396 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 3397 | `		/* Syntax error */` |
|      ! 0 | 3398 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|      ! 0 | 3399 | `		if( rc == SXERR_ABORT ){` |
|        - | 3400 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 3401 | `			return SXERR_ABORT;` |
|        - | 3402 | `		}` |
|      ! 0 | 3403 | `		goto Synchronize;` |
|        - | 3404 | `	}` |
|        - | 3405 | `	/* Jump the left parenthesis '(' */` |
|    11631 | 3406 | `	pGen->pIn++;` |
|    11631 | 3407 | `	pEnd = 0; /* cc warning */` |
|        - | 3408 | `	/* Create the loop block */` |
|    17444 | 3409 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|     5813 | 3410 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|    11631 | 3411 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3412 | `		return SXERR_ABORT;` |
|        - | 3413 | `	}` |
|        - | 3414 | `	/* Delimit the condition */` |
|    11631 | 3415 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    11631 | 3416 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - | 3417 | `		/* Empty expression */` |
|      ! 0 | 3418 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|      ! 0 | 3419 | `		if( rc == SXERR_ABORT ){` |
|        - | 3420 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 3421 | `			return SXERR_ABORT;` |
|        - | 3422 | `		}` |
|      ! 0 | 3423 | `	}` |
|        - | 3424 | `	/* Swap token streams */` |
|    11631 | 3425 | `	pTmp = pGen->pEnd;` |
|    11631 | 3426 | `	pGen->pEnd = pEnd;` |
|        - | 3427 | `	/* Compile the expression */` |
|    11631 | 3428 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    11631 | 3429 | `	if( rc == SXERR_ABORT ){` |
|        - | 3430 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 3431 | `		return SXERR_ABORT;` |
|        - | 3432 | `	}` |
|        - | 3433 | `	/* Update token stream */` |
|    11631 | 3434 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 | 3435 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 3436 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 3437 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3438 | `			return SXERR_ABORT;` |
|        - | 3439 | `		}` |
|      ! 0 | 3440 | `		pGen->pIn++;` |
|      ! 0 | 3441 | `	}` |
|    11631 | 3442 | `	pGen->pIn  = &pEnd[1];` |
|    11631 | 3443 | `	pGen->pEnd = pTmp;` |
|    11631 | 3444 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|    11626 | 3445 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|      ! 0 | 3446 | `			pTmp = pGen->pIn;` |
|      ! 0 | 3447 | `			if( pTmp >= pGen->pEnd ){` |
|      ! 0 | 3448 | `				pTmp--;` |
|      ! 0 | 3449 | `			}` |
|        - | 3450 | `			/* Unexpected token */` |
|      ! 0 | 3451 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|      ! 0 | 3452 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3453 | `				return SXERR_ABORT;` |
|        - | 3454 | `			}` |
|      ! 0 | 3455 | `			goto Synchronize;` |
|        - | 3456 | `	}` |
|        - | 3457 | `	/* Set the delimiter token */` |
|    11631 | 3458 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|        3 | 3459 | `		nToken = PH7_TK_KEYWORD;` |
|        - | 3460 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|        2 | 3461 | `	}else{` |
|    11629 | 3462 | `		nToken = PH7_TK_CCB; /* '}' */` |
|        - | 3463 | `	}` |
|    11631 | 3464 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|        - | 3465 | `	/* Create the switch blocks container */` |
|    11631 | 3466 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|    11631 | 3467 | `	if( pSwitch == 0 ){` |
|        - | 3468 | `		/* Abort compilation */` |
|      ! 0 | 3469 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 3470 | `		return SXERR_ABORT;` |
|        - | 3471 | `	}` |
|        - | 3472 | `	/* Zero the structure */` |
|    11631 | 3473 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|        - | 3474 | `	/* Initialize fields */` |
|    11631 | 3475 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|        - | 3476 | `	/* Emit the switch instruction */` |
|    11631 | 3477 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|        - | 3478 | `	/* Compile case blocks */` |
|   129611 | 3479 | `	for(;;){` |
|        - | 3480 | `		sxu32 nKwrd;` |
|   135429 | 3481 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 3482 | `			/* No more input to process */` |
|      ! 0 | 3483 | `			break;` |
|        - | 3484 | `		}` |
|   135429 | 3485 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 3486 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|        - | 3487 | `				/* Unexpected token */` |
|      ! 0 | 3488 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 3489 | `					&pGen->pIn->sData);` |
|      ! 0 | 3490 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3491 | `					return SXERR_ABORT;` |
|        - | 3492 | `				}` |
|        - | 3493 | `				/* FALL THROUGH */` |
|      ! 0 | 3494 | `			}` |
|        - | 3495 | `			/* Block compiled */` |
|      ! 0 | 3496 | `			break;` |
|        - | 3497 | `		}` |
|        - | 3498 | `		/* Extract the keyword */` |
|   135429 | 3499 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   135429 | 3500 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|        3 | 3501 | `			if( nToken != PH7_TK_KEYWORD ){` |
|        - | 3502 | `				/* Unexpected token */` |
|      ! 0 | 3503 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 3504 | `					&pGen->pIn->sData);` |
|      ! 0 | 3505 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3506 | `					return SXERR_ABORT;` |
|        - | 3507 | `				}` |
|        - | 3508 | `				/* FALL THROUGH */` |
|      ! 0 | 3509 | `			}` |
|        - | 3510 | `			/* Block compiled */` |
|        3 | 3511 | `			break;` |
|        - | 3512 | `		}` |
|   135427 | 3513 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|        - | 3514 | `			/*` |
|        - | 3515 | `			 * Accroding to the PHP language reference manual` |
|        - | 3516 | `			 *  A special case is the default case. This case matches anything` |
|        - | 3517 | `			 *  that wasn't matched by the other cases.` |
|        - | 3518 | `			 */` |
|     3891 | 3519 | `			if( pSwitch->nDefault > 0 ){` |
|        - | 3520 | `				/* Default case already compiled */` |
|      ! 0 | 3521 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|      ! 0 | 3522 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3523 | `					return SXERR_ABORT;` |
|        - | 3524 | `				}` |
|      ! 0 | 3525 | `			}` |
|     3891 | 3526 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|        - | 3527 | `			/* Compile the default block */` |
|     3891 | 3528 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|     3891 | 3529 | `			if( rc == SXERR_ABORT){` |
|      ! 0 | 3530 | `				return SXERR_ABORT;` |
|     3891 | 3531 | `			}else if( rc == SXERR_EOF ){` |
|     3889 | 3532 | `				break;` |
|        1 | 3533 | `			}` |
|   131542 | 3534 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|        - | 3535 | `			ph7_case_expr sCase;` |
|        - | 3536 | `			/* Standard case block */` |
|   131541 | 3537 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|        - | 3538 | `			/* initialize the structure */` |
|   131541 | 3539 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - | 3540 | `			/* Compile the case expression */` |
|   131541 | 3541 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|   131541 | 3542 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3543 | `				return SXERR_ABORT;` |
|        - | 3544 | `			}` |
|        - | 3545 | `			/* Compile the case block */` |
|   131541 | 3546 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|        - | 3547 | `			/* Insert in the switch container */` |
|   131541 | 3548 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|   131541 | 3549 | `			if( rc == SXERR_ABORT){` |
|      ! 0 | 3550 | `				return SXERR_ABORT;` |
|   131541 | 3551 | `			}else if( rc == SXERR_EOF ){` |
|     7745 | 3552 | `				break;` |
|        - | 3553 | `			}` |
|    61903 | 3554 | `		}else{` |
|        - | 3555 | `			/* Unexpected token */` |
|      ! 0 | 3556 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 3557 | `				&pGen->pIn->sData);` |
|      ! 0 | 3558 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3559 | `				return SXERR_ABORT;` |
|        - | 3560 | `			}` |
|      ! 0 | 3561 | `			break;` |
|        - | 3562 | `		}` |
|        5 | 3563 | `	}` |
|        - | 3564 | `	/* Fix all jumps now the destination is resolved */` |
|    11631 | 3565 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|    11631 | 3566 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 3567 | `	/* Release the loop block */` |
|    11631 | 3568 | `	GenStateLeaveBlock(pGen,0);` |
|    11631 | 3569 | `	if( pGen->pIn < pGen->pEnd ){` |
|        - | 3570 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|    11631 | 3571 | `		pGen->pIn++;` |
|     5813 | 3572 | `	}` |
|        - | 3573 | `	/* Statement successfully compiled */` |
|    11631 | 3574 | `	return SXRET_OK;` |
|      ! 0 | 3575 | `Synchronize:` |
|        - | 3576 | `	/* Synchronize with the first semi-colon */` |
|      ! 0 | 3577 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      ! 0 | 3578 | `		pGen->pIn++;` |
|      ! 0 | 3579 | `	}` |
|      ! 0 | 3580 | `	return SXRET_OK;` |
|     5818 | 3581 | `}` |
|        - | 3582 |  |
