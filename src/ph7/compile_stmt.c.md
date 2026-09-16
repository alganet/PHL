# src/ph7/compile_stmt.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1463/1981 lines (73.85%)

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
|       67 |   57 | `	pName = &pGen->pIn->sData;` |
|        - |   58 | `	/* Make sure the constant name isn't reserved */` |
|       67 |   59 | `	if( GenStateIsReservedConstant(pName) ){` |
|        - |   60 | `		/* Reserved constant */` |
|       10 |   61 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Cannot redeclare constant '%z'",pName);` |
|       10 |   62 | `		if( rc == SXERR_ABORT ){` |
|        - |   63 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |   64 | `			return SXERR_ABORT;` |
|        - |   65 | `		}` |
|       10 |   66 | `		goto Synchronize;` |
|        - |   67 | `	}` |
|       58 |   68 | `	pGen->pIn++;` |
|       58 |   69 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
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
|        - |  100 | `	/* Compile constant value */` |
|       51 |  101 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  102 | `	/* Emit the done instruction */` |
|       51 |  103 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       51 |  104 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       51 |  105 | `	if( rc == SXERR_ABORT ){` |
|        - |  106 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  107 | `		return SXERR_ABORT;` |
|        - |  108 | `	}` |
|       51 |  109 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|        - |  110 | `	/* Register the constant with namespace-qualified name */` |
|        - |  111 | `	{` |
|        - |  112 | `		SyBlob sFQN;` |
|        - |  113 | `		SyString sFQNStr;` |
|       51 |  114 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       51 |  115 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|       51 |  116 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|       75 |  117 | `		rc = PH7_VmRegisterConstantEx(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode,` |
|       48 |  118 | `			(SyString *)SySetPeek(&pGen->pVm->aFiles),nLineLocal,1);` |
|       51 |  119 | `		if( rc == SXRET_OK && SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|        - |  120 | ``			/* php 8.5: attributes on `const` statements — attach the pending`` |
|        - |  121 | `			 * groups to the registered constant record for Reflection. */` |
|        7 |  122 | `			SyHashEntry *pCEntry = SyHashGet(&pGen->pVm->hConstant,` |
|        4 |  123 | `				SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|        5 |  124 | `			if( pCEntry ){` |
|        5 |  125 | `				ph7_constant *pRegCons = (ph7_constant *)pCEntry->pUserData;` |
|        5 |  126 | `				if( GenStateConsumeAttrs(&(*pGen),&pRegCons->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  127 | `					SyBlobRelease(&sFQN);` |
|      ! 0 |  128 | `					return SXERR_ABORT;` |
|        - |  129 | `				}` |
|        2 |  130 | `			}` |
|        2 |  131 | `		}` |
|       51 |  132 | `		SyBlobRelease(&sFQN);` |
|        - |  133 | `	}` |
|       51 |  134 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  135 | `		SySetRelease(pConsCode);` |
|      ! 0 |  136 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|      ! 0 |  137 | `	}` |
|       51 |  138 | `	return SXRET_OK;` |
|       10 |  139 | `Synchronize:` |
|        - |  140 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|       70 |  141 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       48 |  142 | `		pGen->pIn++;` |
|        2 |  143 | `	}` |
|       24 |  144 | `	return SXRET_OK;` |
|       39 |  145 | `}` |
|        - |  146 | `/*` |
|        - |  147 | ` * Compile the 'continue' statement.` |
|        - |  148 | ` * According to the PHP language reference` |
|        - |  149 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|        - |  150 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|        - |  151 | ` *  iteration.` |
|        - |  152 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|        - |  153 | ` *  the purposes of continue.` |
|        - |  154 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|        - |  155 | ` *  of enclosing loops it should skip to the end of.` |
|        - |  156 | ` *  Note:` |
|        - |  157 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|        - |  158 | ` */` |
|        - |  159 | `/*` |
|        - |  160 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|        - |  161 | ` * block and the target loop block. This ensures finally blocks run when` |
|        - |  162 | ` * break/continue crosses a try boundary.` |
|        - |  163 | ` *` |
|        - |  164 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|        - |  165 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|        - |  166 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|        - |  167 | ` */` |
|   178916 |  168 | `static int GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|        5 |  169 | `{` |
|   178921 |  170 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   178921 |  171 | `	int nInlineTry = 0;` |
|   723225 |  172 | `	while( pBlock && pBlock != pTarget ){` |
|   544309 |  173 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|        6 |  174 | `			if( pBlock->pUserData ){` |
|        - |  175 | `				/* A try block with an exception context. In a generator its catch/finally` |
|        - |  176 | `				 * are inlined: count it so the caller emits a single OP_SET_FINALLY_JMP that` |
|        - |  177 | `				 * runs each crossed finally (VmFinallyAdvance) before taking the loop jump.` |
|        - |  178 | `				 * Legacy path: emit POP_EXCEPTION per crossed try as before. */` |
|        6 |  179 | `				if( pGen->bInGenerator ){` |
|        3 |  180 | `					nInlineTry++;` |
|        2 |  181 | `				}else{` |
|        3 |  182 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|        - |  183 | `				}` |
|        4 |  184 | `			}else{` |
|        - |  185 | `				/* A catch/finally block compiled into a separate bytecode container` |
|        - |  186 | `				 * (legacy). Stop — cannot cross into the parent try from a sub-execution. */` |
|      ! 0 |  187 | `				break;` |
|        - |  188 | `			}` |
|        2 |  189 | `		}` |
|   544309 |  190 | `		pBlock = pBlock->pParent;` |
|        5 |  191 | `	}` |
|   178921 |  192 | `	return nInlineTry;` |
|        5 |  193 | `}` |
|    89430 |  194 | `PH7_PRIVATE sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|        5 |  195 | `{` |
|        - |  196 | `	GenBlock *pLoop; /* Target loop */` |
|        - |  197 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|        - |  198 | `	sxi32 iRawLevel; /* The level as WRITTEN, kept for php's diagnostics */` |
|        - |  199 | `	sxu32 nLineLocal;` |
|        - |  200 | `	sxi32 rc;` |
|    89435 |  201 | `	iRawLevel = 1;` |
|    89435 |  202 | `	nLineLocal = pGen->pIn->nLine;` |
|    89435 |  203 | `	iLevel = 0;` |
|        - |  204 | `	/* Jump the 'continue' keyword */` |
|    89435 |  205 | `	pGen->pIn++;` |
|    89435 |  206 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|        - |  207 | `		/* optional numeric argument which tells us how many levels` |
|        - |  208 | `		 * of enclosing loops we should skip to the end of.` |
|        - |  209 | `		 */` |
|        - |  210 | `		char zScratch[GEN_NUM_SCRATCH];` |
|       16 |  211 | `		char *zAlloc = 0;` |
|        - |  212 | `		SyString sNum;` |
|       16 |  213 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|       16 |  214 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  215 | `			return SXERR_ABORT;` |
|        - |  216 | `		}` |
|       16 |  217 | `		if( rc == SXRET_OK ){` |
|       20 |  218 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|       12 |  219 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|       14 |  220 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  221 | `				return SXERR_ABORT;` |
|        - |  222 | `			}` |
|       14 |  223 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|       14 |  224 | `			iRawLevel = iLevel;` |
|       14 |  225 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        6 |  226 | `		}` |
|       16 |  227 | `		if( iLevel < 2 ){` |
|        3 |  228 | `			iLevel = 0;` |
|        1 |  229 | `		}` |
|       16 |  230 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|        7 |  231 | `	}` |
|        - |  232 | `	/* php rejects a non-positive level outright, before asking where it lands. */` |
|    89435 |  233 | `	if( iRawLevel < 1 ){` |
|      ! 0 |  234 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|        - |  235 | `			"'continue' operator accepts only positive integers");` |
|      ! 0 |  236 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  237 | `			return SXERR_ABORT;` |
|        - |  238 | `		}` |
|      ! 0 |  239 | `		return SXRET_OK;` |
|        - |  240 | `	}` |
|        - |  241 | `	/* Point to the target loop */` |
|    89435 |  242 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    89435 |  243 | `	if( pLoop == 0 ){` |
|        - |  244 | ``		/* Same split php makes for `break`: no loop at all keeps the not-in-context`` |
|        - |  245 | ``		 * wording, too FEW loops is `Cannot 'continue' N levels`. */`` |
|       13 |  246 | `		if( GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,0) != 0 ){` |
|      ! 0 |  247 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|      ! 0 |  248 | `				"Cannot 'continue' %d level%s",iRawLevel,iRawLevel == 1 ? "" : "s");` |
|      ! 0 |  249 | `		}else{` |
|       13 |  250 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"'continue' not in the 'loop' or 'switch' context");` |
|        - |  251 | `		}` |
|       13 |  252 | `		if( rc == SXERR_ABORT ){` |
|        - |  253 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  254 | `			return SXERR_ABORT;` |
|        - |  255 | `		}` |
|        8 |  256 | `	}else{` |
|    89425 |  257 | `		sxu32 nInstrIdx = 0;` |
|        - |  258 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|    89425 |  259 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|        - |  260 | `		/* ROOT C: in a generator, a break/continue crossing inline trys must run their` |
|        - |  261 | `		 * finallys first. OP_SET_FINALLY_JMP(iP1=count) does that then takes the loop jump. */` |
|    89425 |  262 | `		sxi32 iJmpOp = nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP;` |
|    89425 |  263 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|        - |  264 | ``			/* `continue` inside a switch acts like `break` — which is almost never what`` |
|        - |  265 | `			 * the author meant, so php 7.3+ says so at compile time. The generated jump` |
|        - |  266 | `			 * is unchanged; only the diagnostic was missing. An explicit level` |
|        - |  267 | ``			 * (`continue 2`) targets the enclosing loop and stays silent. */`` |
|        5 |  268 | `			if( iLevel < 1 ){` |
|        5 |  269 | `				PH7_GenCompileError(&(*pGen),E_WARNING,nLineLocal,` |
|        - |  270 | `					"\"continue\" targeting switch is equivalent to \"break\"."` |
|        - |  271 | `					" Did you mean to use \"continue 2\"?");` |
|        2 |  272 | `			}` |
|        5 |  273 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,0,0,&nInstrIdx);` |
|        5 |  274 | `			if( rc == SXRET_OK ){` |
|        5 |  275 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|        2 |  276 | `			}` |
|        3 |  277 | `		}else{` |
|        - |  278 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    89421 |  279 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    89421 |  280 | `			if( pLoop->bPostContinue == TRUE ){` |
|        - |  281 | `				JumpFixup sJumpFix;` |
|        - |  282 | `				/* Post-continue */` |
|    27219 |  283 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|    27219 |  284 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|    27219 |  285 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|    13607 |  286 | `			}` |
|        - |  287 | `		}` |
|        - |  288 | `	}` |
|    89435 |  289 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  290 | `		/* Not so fatal,emit a warning only */` |
|      ! 0 |  291 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|      ! 0 |  292 | `	}` |
|        - |  293 | `	/* Statement successfully compiled */` |
|    89435 |  294 | `	return SXRET_OK;` |
|    44720 |  295 | `}` |
|        - |  296 | `/*` |
|        - |  297 | ` * Compile the 'break' statement.` |
|        - |  298 | ` * According to the PHP language reference` |
|        - |  299 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|        - |  300 | ` *  structure.` |
|        - |  301 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|        - |  302 | ` *  enclosing structures are to be broken out of.` |
|        - |  303 | ` */` |
|    89512 |  304 | `PH7_PRIVATE sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|        5 |  305 | `{` |
|        - |  306 | `	GenBlock *pLoop; /* Target loop */` |
|        - |  307 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|        - |  308 | `	sxi32 iRawLevel; /* The level as WRITTEN, kept for php's diagnostics */` |
|        - |  309 | `	sxi32 rc;` |
|    89517 |  310 | `	iLevel = 0;` |
|    89517 |  311 | `	iRawLevel = 1;` |
|        - |  312 | `	/* Jump the 'break' keyword */` |
|    89517 |  313 | `	pGen->pIn++;` |
|    89517 |  314 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|        - |  315 | `		/* optional numeric argument which tells us how many levels` |
|        - |  316 | `		 * of enclosing loops we should skip to the end of.` |
|        - |  317 | `		 */` |
|        - |  318 | `		char zScratch[GEN_NUM_SCRATCH];` |
|       17 |  319 | `		char *zAlloc = 0;` |
|        - |  320 | `		SyString sNum;` |
|       17 |  321 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|       17 |  322 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  323 | `			return SXERR_ABORT;` |
|        - |  324 | `		}` |
|       17 |  325 | `		if( rc == SXRET_OK ){` |
|       21 |  326 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|       12 |  327 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|       15 |  328 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  329 | `				return SXERR_ABORT;` |
|        - |  330 | `			}` |
|       15 |  331 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|       15 |  332 | `			iRawLevel = iLevel;` |
|       15 |  333 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        6 |  334 | `		}` |
|       17 |  335 | `		if( iLevel < 2 ){` |
|        3 |  336 | `			iLevel = 0;` |
|        1 |  337 | `		}` |
|       17 |  338 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|        7 |  339 | `	}` |
|        - |  340 | `	/* php rejects a non-positive level outright, before asking where it lands. */` |
|    89517 |  341 | `	if( iRawLevel < 1 ){` |
|      ! 0 |  342 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  343 | `			"'break' operator accepts only positive integers");` |
|      ! 0 |  344 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  345 | `			return SXERR_ABORT;` |
|        - |  346 | `		}` |
|      ! 0 |  347 | `		goto BreakLevelDone;` |
|        - |  348 | `	}` |
|        - |  349 | `	/* Extract the target loop */` |
|    89517 |  350 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|   134273 |  351 | `	if( pLoop == 0 ){` |
|        - |  352 | `		/* php distinguishes "there is no loop at all here" from "there is one, but` |
|        - |  353 | `		 * not N of them": the first keeps the not-in-context wording, the second is` |
|        - |  354 | ``		 * `Cannot 'break' N levels`. */`` |
|       19 |  355 | `		if( GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,0) != 0 ){` |
|        4 |  356 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        1 |  357 | `				"Cannot 'break' %d level%s",iRawLevel,iRawLevel == 1 ? "" : "s");` |
|        2 |  358 | `		}else{` |
|       16 |  359 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"'break' not in the 'loop' or 'switch' context");` |
|        - |  360 | `		}` |
|       19 |  361 | `		if( rc == SXERR_ABORT ){` |
|        - |  362 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  363 | `			return SXERR_ABORT;` |
|        - |  364 | `		}` |
|       11 |  365 | `	}else{` |
|        - |  366 | `		sxu32 nInstrIdx;` |
|        - |  367 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|    89501 |  368 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|        - |  369 | `		/* ROOT C: OP_SET_FINALLY_JMP runs the crossed inline finallys before the break jump. */` |
|    89501 |  370 | `		rc = PH7_VmEmitInstr(pGen->pVm,nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP,nCross,0,0,&nInstrIdx);` |
|    89501 |  371 | `		if( rc == SXRET_OK ){` |
|        - |  372 | `			/* Fix the jump later when the jump destination is resolved */` |
|    89501 |  373 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|    44748 |  374 | `		}` |
|        - |  375 | `	}` |
|    44756 |  376 | `BreakLevelDone:` |
|    89517 |  377 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  378 | `		/* Not so fatal,emit a warning only */` |
|      ! 0 |  379 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|      ! 0 |  380 | `	}` |
|        - |  381 | `	/* Statement successfully compiled */` |
|    89517 |  382 | `	return SXRET_OK;` |
|    44761 |  383 | `}` |
|        - |  384 | `/*` |
|        - |  385 | ` * Compile or record a label.` |
|        - |  386 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|        - |  387 | ` * Example` |
|        - |  388 | ` *  goto LABEL;` |
|        - |  389 | ` *   echo 'Foo';` |
|        - |  390 | ` *  LABEL:` |
|        - |  391 | ` *   echo 'Bar';` |
|        - |  392 | ` */` |
|      112 |  393 | `PH7_PRIVATE sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|        5 |  394 | `{` |
|        - |  395 | `	GenBlock *pBlock;` |
|        - |  396 | `	Label sLabel;` |
|        - |  397 | `	/* php places NO restriction on where a label may be DEFINED — inside a loop, a switch` |
|        - |  398 | `	 * or a try{} is all fine. The only rule is on the jump: you may not goto INTO a loop` |
|        - |  399 | `	 * or switch from outside it, which is checked once the labels are all known (see` |
|        - |  400 | `	 * GenStateFixJumps). Record the loop this label sits in so that check can run. */` |
|        - |  401 | `	{` |
|      117 |  402 | `		SyString *pTarget = &pGen->pIn->sData;` |
|        - |  403 | `		char *zDup;` |
|        - |  404 | `		/* Initialize label fields */` |
|      117 |  405 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|        - |  406 | `		/* Duplicate label name */` |
|      117 |  407 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|      117 |  408 | `		if( zDup == 0 ){` |
|      ! 0 |  409 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  410 | `			return SXERR_ABORT;` |
|        - |  411 | `		}` |
|      117 |  412 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|      117 |  413 | `		sLabel.bRef  = FALSE;` |
|      117 |  414 | `		sLabel.nLine = pGen->pIn->nLine;` |
|      117 |  415 | `		sLabel.nLoopId = pGen->nCurLoopId;` |
|      117 |  416 | `		pBlock = pGen->pCurrent;` |
|      233 |  417 | `		while( pBlock ){` |
|      143 |  418 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|       27 |  419 | `				break;` |
|        - |  420 | `			}` |
|        - |  421 | `			/* Point to the upper block */` |
|      121 |  422 | `			pBlock = pBlock->pParent;` |
|        5 |  423 | `		}` |
|      117 |  424 | `		if( pBlock ){` |
|       27 |  425 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       16 |  426 | `		}else{` |
|       95 |  427 | `			sLabel.pFunc = 0;` |
|        - |  428 | `		}` |
|        - |  429 | `		/* Insert in label set */` |
|      117 |  430 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|        - |  431 | `	}` |
|      117 |  432 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|      117 |  433 | `	return SXRET_OK;` |
|       61 |  434 | `}` |
|        - |  435 | `/*` |
|        - |  436 | ` * Compile the so hated 'goto' statement.` |
|        - |  437 | ` * You've probably been taught that gotos are bad, but this sort` |
|        - |  438 | ` * of rewriting  happens all the time, in fact every time you run` |
|        - |  439 | ` * a compiler it has to do this.` |
|        - |  440 | ` * According to the PHP language reference manual` |
|        - |  441 | ` *   The goto operator can be used to jump to another section in the program.` |
|        - |  442 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|        - |  443 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|        - |  444 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|        - |  445 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|        - |  446 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|        - |  447 | ` *   of a multi-level break` |
|        - |  448 | ` */` |
|      152 |  449 | `PH7_PRIVATE sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|        5 |  450 | `{` |
|        - |  451 | `	JumpFixup sJump;` |
|        - |  452 | `	sxi32 rc;` |
|      157 |  453 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|      157 |  454 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - |  455 | `		/* Missing label */` |
|      ! 0 |  456 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|      ! 0 |  457 | `		if( rc == SXERR_ABORT ){` |
|        - |  458 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  459 | `			return SXERR_ABORT;` |
|        - |  460 | `		}` |
|      ! 0 |  461 | `		return SXRET_OK;` |
|        - |  462 | `	}` |
|      157 |  463 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|        5 |  464 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn,"identifier");` |
|        5 |  465 | `		if( rc == SXERR_ABORT ){` |
|        - |  466 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  467 | `			return SXERR_ABORT;` |
|        - |  468 | `		}` |
|        3 |  469 | `	}else{` |
|      153 |  470 | `		SyString *pTarget = &pGen->pIn->sData;` |
|        - |  471 | `		GenBlock *pBlock;` |
|        - |  472 | `		char *zDup;` |
|        - |  473 | `		/* Prepare the jump destination */` |
|      153 |  474 | `		sJump.nJumpType = PH7_OP_JMP;` |
|      153 |  475 | `		sJump.nLine = pGen->pIn->nLine;` |
|        - |  476 | `		/* Duplicate label name */` |
|      153 |  477 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|      153 |  478 | `		if( zDup == 0 ){` |
|      ! 0 |  479 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  480 | `			return SXERR_ABORT;` |
|        - |  481 | `		}` |
|      153 |  482 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|        - |  483 | `		/* The loop/switch this goto sits in, for the "goto into a loop" check later. */` |
|      153 |  484 | `		sJump.nLoopId = pGen->nCurLoopId;` |
|        - |  485 | `		/* A goto inside a try{}/catch{} is legal php (jumping OUT of the block is fine);` |
|        - |  486 | `		 * only the owning function matters here, since a goto may not cross functions. */` |
|      153 |  487 | `		pBlock = pGen->pCurrent;` |
|      327 |  488 | `		while( pBlock ){` |
|      205 |  489 | `			if( pBlock->iFlags & GEN_BLOCK_FUNC ){` |
|       29 |  490 | `				break;` |
|        - |  491 | `			}` |
|        - |  492 | `			/* Point to the upper block */` |
|      179 |  493 | `			pBlock = pBlock->pParent;` |
|        5 |  494 | `		}` |
|      153 |  495 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|       29 |  496 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       16 |  497 | `		}else{` |
|      127 |  498 | `			sJump.pFunc = 0;` |
|        - |  499 | `		}` |
|        - |  500 | `		/* Emit the unconditional jump */` |
|      153 |  501 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|      153 |  502 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|       74 |  503 | `		}` |
|        - |  504 | `	}` |
|      157 |  505 | `	pGen->pIn++; /* Jump the label name */` |
|      157 |  506 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        3 |  507 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|        1 |  508 | `	}` |
|        - |  509 | `	/* Statement successfully compiled */` |
|      157 |  510 | `	return SXRET_OK;` |
|       81 |  511 | `}` |
|        - |  512 | `/*` |
|        - |  513 | ` * Point to the next PHP chunk that will be processed shortly.` |
|        - |  514 | ` * Return SXRET_OK on success. Any other return value indicates` |
|        - |  515 | ` * failure.` |
|        - |  516 | ` */` |
|       20 |  517 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|        2 |  518 | `{` |
|        - |  519 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|        - |  520 | `	sxu32 nRawObj;` |
|       10 |  521 | `	sxu32 nObjIdx;` |
|        - |  522 | `	/* Consume raw chunks verbatim without any processing until we get` |
|        - |  523 | `	 * a PHP block.` |
|        - |  524 | `	 */` |
|       10 |  525 | `Consume:` |
|       22 |  526 | `	nRawObj = nObjIdx = 0;` |
|       22 |  527 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|      ! 0 |  528 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|      ! 0 |  529 | `		if( pRawObj == 0 ){` |
|      ! 0 |  530 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  531 | `			return SXERR_ABORT;` |
|        - |  532 | `		}` |
|        - |  533 | `		/* Mark as constant and emit the load constant instruction */` |
|      ! 0 |  534 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|      ! 0 |  535 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|      ! 0 |  536 | `		++nRawObj;` |
|      ! 0 |  537 | `		pGen->pRawIn++; /* Next chunk */` |
|      ! 0 |  538 | `	}` |
|       22 |  539 | `	if( nRawObj > 0 ){` |
|        - |  540 | `		/* Emit the consume instruction */` |
|      ! 0 |  541 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|      ! 0 |  542 | `	}` |
|       22 |  543 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|      ! 0 |  544 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|        - |  545 | `		/* Reset the token set (and its trivia sidecar) */` |
|      ! 0 |  546 | `		SySetReset(pTokenSet);` |
|      ! 0 |  547 | `		SySetReset(&pGen->aTrivia);` |
|        - |  548 | `		/* Tokenize input */` |
|      ! 0 |  549 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|      ! 0 |  550 | `			pGen->pRawIn->nLine,pTokenSet,&pGen->aTrivia);` |
|        - |  551 | `		/* Point to the fresh token stream */` |
|      ! 0 |  552 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|      ! 0 |  553 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|        - |  554 | `		/* Advance the stream cursor */` |
|      ! 0 |  555 | `		pGen->pRawIn++;` |
|        - |  556 | `		/* TICKET 1433-011 */` |
|      ! 0 |  557 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|        - |  558 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|        - |  559 | `			sxi32 rc;` |
|        - |  560 | `			/* Refer to TICKET 1433-009  */` |
|      ! 0 |  561 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|      ! 0 |  562 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|      ! 0 |  563 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|        - |  564 | `			/* Synthesized short-tag echo: legitimately an expression here. */` |
|      ! 0 |  565 | `			pGen->nExprEchoOk++;` |
|      ! 0 |  566 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|      ! 0 |  567 | `			pGen->nExprEchoOk--;` |
|      ! 0 |  568 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  569 | `				return SXERR_ABORT;` |
|      ! 0 |  570 | `			}else if( rc != SXERR_EMPTY ){` |
|      ! 0 |  571 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      ! 0 |  572 | `			}` |
|      ! 0 |  573 | `			goto Consume;` |
|        - |  574 | `		}` |
|      ! 0 |  575 | `	}else{` |
|        - |  576 | `		/* No more chunks to process */` |
|       22 |  577 | `		pGen->pIn = pGen->pEnd;` |
|       22 |  578 | `		return SXERR_EOF;` |
|        - |  579 | `	}` |
|      ! 0 |  580 | `	return SXRET_OK;` |
|       12 |  581 | `}` |
|        - |  582 | `/*` |
|        - |  583 | ` * Compile a PHP block.` |
|        - |  584 | ` * A block is simply one or more PHP statements and expressions to compile` |
|        - |  585 | ` * optionally delimited by braces {}.` |
|        - |  586 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|        - |  587 | ` * and this function takes care of generating the appropriate error` |
|        - |  588 | ` * message.` |
|        - |  589 | ` */` |
|  6932196 |  590 | `PH7_PRIVATE sxi32 PH7_CompileBlock(` |
|        - |  591 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |  592 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|        - |  593 | `	)` |
|        5 |  594 | `{` |
|        - |  595 | `	sxi32 rc;` |
|        - |  596 | `	sxu32 nLine;` |
|  6932201 |  597 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  6806849 |  598 | `		nLine = pGen->pIn->nLine;` |
|  6806849 |  599 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  6806849 |  600 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  601 | `			return SXERR_ABORT;` |
|        - |  602 | `		}` |
|  6806849 |  603 | `		pGen->pIn++;` |
|        - |  604 | `		/* Compile until we hit the closing braces '}' */` |
|  9959745 |  605 | `		for(;;){` |
| 19919495 |  606 | `			if( pGen->pIn >= pGen->pEnd ){` |
|       22 |  607 | `				rc = GenStateNextChunk(&(*pGen));` |
|       22 |  608 | `				if (rc == SXERR_ABORT ){` |
|      ! 0 |  609 | `			 	   return SXERR_ABORT;` |
|        - |  610 | `				}` |
|       22 |  611 | `				if( rc == SXERR_EOF ){` |
|        - |  612 | `					/* No more token to process: the block was never closed. php reports` |
|        - |  613 | `					 * the line the '{' was opened on, not where the input ran out. */` |
|       22 |  614 | `					PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"Unclosed '{' on line %u",nLine);` |
|       22 |  615 | `					break;` |
|        - |  616 | `				}` |
|      ! 0 |  617 | `			}` |
| 19919475 |  618 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|        - |  619 | `				/* Closing braces found,break immediately*/` |
|  6806829 |  620 | `				pGen->pIn++;` |
|  6806829 |  621 | `				break;` |
|        - |  622 | `			}` |
|        - |  623 | `			/* Compile a single statement */` |
| 13112651 |  624 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
| 13112651 |  625 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  626 | `				return SXERR_ABORT;` |
|        - |  627 | `			}` |
|        5 |  628 | `		}` |
|  6806849 |  629 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  3528779 |  630 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|      ! 0 |  631 | `		pGen->pIn++;` |
|      ! 0 |  632 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|      ! 0 |  633 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  634 | `			return SXERR_ABORT;` |
|        - |  635 | `		}` |
|        - |  636 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|      ! 0 |  637 | `		for(;;){` |
|      ! 0 |  638 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  639 | `				rc = GenStateNextChunk(&(*pGen));` |
|      ! 0 |  640 | `				if (rc == SXERR_ABORT ){` |
|      ! 0 |  641 | `			 	   return SXERR_ABORT;` |
|        - |  642 | `				}` |
|      ! 0 |  643 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|        - |  644 | `					/* No more token to process */` |
|      ! 0 |  645 | `					if( rc == SXERR_EOF ){` |
|      ! 0 |  646 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|        - |  647 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|      ! 0 |  648 | `					}` |
|      ! 0 |  649 | `					break;` |
|        - |  650 | `				}` |
|      ! 0 |  651 | `			}` |
|      ! 0 |  652 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|        - |  653 | `				sxi32 nKwrd;` |
|        - |  654 | `				/* Keyword found */` |
|      ! 0 |  655 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      ! 0 |  656 | `				if( nKwrd == nKeywordEnd \|\|` |
|      ! 0 |  657 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|        - |  658 | `						/* Delimiter keyword found,break */` |
|      ! 0 |  659 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|      ! 0 |  660 | `							pGen->pIn++; /*  endif;endswitch... */` |
|      ! 0 |  661 | `						}` |
|      ! 0 |  662 | `						break;` |
|        - |  663 | `				}` |
|      ! 0 |  664 | `			}` |
|        - |  665 | `			/* Compile a single statement */` |
|      ! 0 |  666 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|      ! 0 |  667 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  668 | `				return SXERR_ABORT;` |
|        - |  669 | `			}` |
|      ! 0 |  670 | `		}` |
|      ! 0 |  671 | `		GenStateLeaveBlock(&(*pGen),0);` |
|      ! 0 |  672 | `	}else{` |
|        - |  673 | `		/* Compile a single statement */` |
|   125357 |  674 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|   125357 |  675 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  676 | `			return SXERR_ABORT;` |
|        - |  677 | `		}` |
|        - |  678 | `	}` |
|        - |  679 | `	/* Jump trailing semi-colons ';' */` |
|  6932201 |  680 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      ! 0 |  681 | `		pGen->pIn++;` |
|      ! 0 |  682 | `	}` |
|  6932201 |  683 | `	return SXRET_OK;` |
|  3466103 |  684 | `}` |
|        - |  685 | `/*` |
|        - |  686 | ` * Compile the gentle 'while' statement.` |
|        - |  687 | ` * According to the PHP language reference` |
|        - |  688 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|        - |  689 | ` *  The basic form of a while statement is:` |
|        - |  690 | ` *  while (expr)` |
|        - |  691 | ` *   statement` |
|        - |  692 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|        - |  693 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|        - |  694 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|        - |  695 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|        - |  696 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|        - |  697 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|        - |  698 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|        - |  699 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|        - |  700 | ` *  while (expr):` |
|        - |  701 | ` *    statement` |
|        - |  702 | ` *   endwhile;` |
|        - |  703 | ` */` |
|    73978 |  704 | `PH7_PRIVATE sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|        5 |  705 | `{` |
|    73983 |  706 | `	GenBlock *pWhileBlock = 0;` |
|    73983 |  707 | `	SyToken *pTmp,*pEnd = 0;` |
|        - |  708 | `	sxu32 nFalseJump;` |
|        - |  709 | `	sxu32 nLine;` |
|        - |  710 | `	sxi32 rc;` |
|    73983 |  711 | `	nLine = pGen->pIn->nLine;` |
|        - |  712 | `	/* Jump the 'while' keyword */` |
|    73983 |  713 | `	pGen->pIn++;` |
|    73983 |  714 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  715 | `		/* Syntax error */` |
|      ! 0 |  716 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|      ! 0 |  717 | `		if( rc == SXERR_ABORT ){` |
|        - |  718 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  719 | `			return SXERR_ABORT;` |
|        - |  720 | `		}` |
|      ! 0 |  721 | `		goto Synchronize;` |
|        - |  722 | `	}` |
|        - |  723 | `	/* Jump the left parenthesis '(' */` |
|    73983 |  724 | `	pGen->pIn++;` |
|        - |  725 | `	/* Create the loop block */` |
|    73983 |  726 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|    73983 |  727 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  728 | `		return SXERR_ABORT;` |
|        - |  729 | `	}` |
|        - |  730 | `	/* Delimit the condition */` |
|    73983 |  731 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    73983 |  732 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  733 | `		/* Empty expression */` |
|        3 |  734 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|        3 |  735 | `		if( rc == SXERR_ABORT ){` |
|        - |  736 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  737 | `			return SXERR_ABORT;` |
|        - |  738 | `		}` |
|        1 |  739 | `	}` |
|        - |  740 | `	/* Swap token streams */` |
|    73983 |  741 | `	pTmp = pGen->pEnd;` |
|    73983 |  742 | `	pGen->pEnd = pEnd;` |
|        - |  743 | `	/* Compile the expression */` |
|    73983 |  744 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    73983 |  745 | `	if( rc == SXERR_ABORT ){` |
|        - |  746 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  747 | `		return SXERR_ABORT;` |
|        - |  748 | `	}` |
|        - |  749 | `	/* Update token stream */` |
|    73983 |  750 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 |  751 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 |  752 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  753 | `			return SXERR_ABORT;` |
|        - |  754 | `		}` |
|      ! 0 |  755 | `		pGen->pIn++;` |
|      ! 0 |  756 | `	}` |
|        - |  757 | `	/* Synchronize pointers */` |
|    73983 |  758 | `	pGen->pIn  = &pEnd[1];` |
|    73983 |  759 | `	pGen->pEnd = pTmp;` |
|        - |  760 | `	/* Emit the false jump */` |
|    73983 |  761 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|        - |  762 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    73983 |  763 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|        - |  764 | `	/* Compile the loop body */` |
|    73983 |  765 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|    73983 |  766 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  767 | `		return SXERR_ABORT;` |
|        - |  768 | `	}` |
|        - |  769 | `	/* Emit the unconditional jump to the start of the loop */` |
|    73983 |  770 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|        - |  771 | `	/* Fix all jumps now the destination is resolved */` |
|    73983 |  772 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  773 | `	/* Release the loop block */` |
|    73983 |  774 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  775 | `	/* Statement successfully compiled */` |
|    73983 |  776 | `	return SXRET_OK;` |
|      ! 0 |  777 | `Synchronize:` |
|        - |  778 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - |  779 | `	 * compiling this erroneous block.` |
|        - |  780 | `	 */` |
|      ! 0 |  781 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  782 | `		pGen->pIn++;` |
|      ! 0 |  783 | `	}` |
|      ! 0 |  784 | `	return SXRET_OK;` |
|    36994 |  785 | `}` |
|        - |  786 | `/*` |
|        - |  787 | ` * Compile the ugly do..while() statement.` |
|        - |  788 | ` * According to the PHP language reference` |
|        - |  789 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|        - |  790 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|        - |  791 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|        - |  792 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|        - |  793 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|        - |  794 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|        - |  795 | ` *  would end immediately).` |
|        - |  796 | ` *  There is just one syntax for do-while loops:` |
|        - |  797 | ` *  <?php` |
|        - |  798 | ` *  $i = 0;` |
|        - |  799 | ` *  do {` |
|        - |  800 | ` *   echo $i;` |
|        - |  801 | ` *  } while ($i > 0);` |
|        - |  802 | ` * ?>` |
|        - |  803 | ` */` |
|        2 |  804 | `PH7_PRIVATE sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|        1 |  805 | `{` |
|        3 |  806 | `	SyToken *pTmp,*pEnd = 0;` |
|        3 |  807 | `	GenBlock *pDoBlock = 0;` |
|        - |  808 | `	sxu32 nLine;` |
|        - |  809 | `	sxi32 rc;` |
|        3 |  810 | `	nLine = pGen->pIn->nLine;` |
|        - |  811 | `	/* Jump the 'do' keyword */` |
|        3 |  812 | `	pGen->pIn++;` |
|        - |  813 | `	/* Create the loop block */` |
|        3 |  814 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|        3 |  815 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  816 | `		return SXERR_ABORT;` |
|        - |  817 | `	}` |
|        - |  818 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|        3 |  819 | `	pDoBlock->bPostContinue = TRUE;` |
|        3 |  820 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|        3 |  821 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  822 | `		return SXERR_ABORT;` |
|        - |  823 | `	}` |
|        3 |  824 | `	if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 |  825 | `		nLine = pGen->pIn->nLine;` |
|      ! 0 |  826 | `	}` |
|        3 |  827 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|      ! 0 |  828 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|        - |  829 | `			/* Missing 'while' statement */` |
|        - |  830 | ``			/* php: `do {} ;` names the ';', while `do {}` at EOF is "unexpected end of`` |
|        - |  831 | `			 * file". The do-block's own slice stops before its terminator, so look at` |
|        - |  832 | `			 * the whole CHUNK stream: a token still there is the one php names; nothing` |
|        - |  833 | `			 * left means end of file (NULL). */` |
|        - |  834 | `			{` |
|        3 |  835 | `				SyToken *pBad = pGen->pIn < pGen->pEnd ? pGen->pIn : 0;` |
|        3 |  836 | `				if( pBad == 0 && pGen->pTokenSet ){` |
|        - |  837 | `					/* The do-block consumed its terminator, so the token php names is` |
|        - |  838 | `					 * the one just behind the cursor -- but only when it is a real` |
|        - |  839 | ``					 * terminator (`do {} ;` -> the ';'). If the block simply ended on`` |
|        - |  840 | `					 * its '}' with nothing after it, php reports end of file. */` |
|        3 |  841 | `					SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        3 |  842 | `					if( pGen->pIn > pBase && (pGen->pIn[-1].nType & PH7_TK_SEMI) ){` |
|      ! 0 |  843 | `						pBad = &pGen->pIn[-1];` |
|      ! 0 |  844 | `					}` |
|        1 |  845 | `				}` |
|        3 |  846 | `				rc = PH7_GenSyntaxError(pGen,pBad,"\"while\"");` |
|        - |  847 | `			}` |
|        3 |  848 | `			if( rc == SXERR_ABORT ){` |
|        - |  849 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  850 | `				return SXERR_ABORT;` |
|        - |  851 | `			}` |
|        3 |  852 | `			goto Synchronize;` |
|        - |  853 | `	}` |
|        - |  854 | `	/* Jump the 'while' keyword */` |
|      ! 0 |  855 | `	pGen->pIn++;` |
|      ! 0 |  856 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  857 | `		/* Syntax error */` |
|      ! 0 |  858 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|      ! 0 |  859 | `		if( rc == SXERR_ABORT ){` |
|        - |  860 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  861 | `			return SXERR_ABORT;` |
|        - |  862 | `		}` |
|      ! 0 |  863 | `		goto Synchronize;` |
|        - |  864 | `	}` |
|        - |  865 | `	/* Jump the left parenthesis '(' */` |
|      ! 0 |  866 | `	pGen->pIn++;` |
|        - |  867 | `	/* Delimit the condition */` |
|      ! 0 |  868 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      ! 0 |  869 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  870 | `		/* Empty expression */` |
|      ! 0 |  871 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|      ! 0 |  872 | `		if( rc == SXERR_ABORT ){` |
|        - |  873 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  874 | `			return SXERR_ABORT;` |
|        - |  875 | `		}` |
|      ! 0 |  876 | `		goto Synchronize;` |
|        - |  877 | `	}` |
|        - |  878 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|      ! 0 |  879 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|        - |  880 | `		JumpFixup *aPost;` |
|        - |  881 | `		VmInstr *pInstr;` |
|        - |  882 | `		sxu32 nJumpDest;` |
|        - |  883 | `		sxu32 n;` |
|      ! 0 |  884 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|      ! 0 |  885 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      ! 0 |  886 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|      ! 0 |  887 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      ! 0 |  888 | `			if( pInstr ){` |
|        - |  889 | `				/* Fix */` |
|      ! 0 |  890 | `				pInstr->iP2 = nJumpDest;` |
|      ! 0 |  891 | `			}` |
|      ! 0 |  892 | `		}` |
|      ! 0 |  893 | `	}` |
|        - |  894 | `	/* Swap token streams */` |
|      ! 0 |  895 | `	pTmp = pGen->pEnd;` |
|      ! 0 |  896 | `	pGen->pEnd = pEnd;` |
|        - |  897 | `	/* Compile the expression */` |
|      ! 0 |  898 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      ! 0 |  899 | `	if( rc == SXERR_ABORT ){` |
|        - |  900 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  901 | `		return SXERR_ABORT;` |
|        - |  902 | `	}` |
|        - |  903 | `	/* Update token stream */` |
|      ! 0 |  904 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 |  905 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 |  906 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  907 | `			return SXERR_ABORT;` |
|        - |  908 | `		}` |
|      ! 0 |  909 | `		pGen->pIn++;` |
|      ! 0 |  910 | `	}` |
|      ! 0 |  911 | `	pGen->pIn  = &pEnd[1];` |
|      ! 0 |  912 | `	pGen->pEnd = pTmp;` |
|        - |  913 | `	/* Emit the true jump to the beginning of the loop */` |
|      ! 0 |  914 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|        - |  915 | `	/* Fix all jumps now the destination is resolved */` |
|      ! 0 |  916 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  917 | `	/* Release the loop block */` |
|      ! 0 |  918 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  919 | `	/* Statement successfully compiled */` |
|      ! 0 |  920 | `	return SXRET_OK;` |
|        1 |  921 | `Synchronize:` |
|        - |  922 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - |  923 | `	 * compiling this erroneous block.` |
|        - |  924 | `	 */` |
|        3 |  925 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  926 | `		pGen->pIn++;` |
|      ! 0 |  927 | `	}` |
|        3 |  928 | `	return SXRET_OK;` |
|        2 |  929 | `}` |
|        - |  930 | `/*` |
|        - |  931 | ` * Compile the complex and powerful 'for' statement.` |
|        - |  932 | ` * According to the PHP language reference` |
|        - |  933 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|        - |  934 | ` *  The syntax of a for loop is:` |
|        - |  935 | ` *  for (expr1; expr2; expr3)` |
|        - |  936 | ` *   statement` |
|        - |  937 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|        - |  938 | ` *  the beginning of the loop.` |
|        - |  939 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|        - |  940 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|        - |  941 | ` *  to FALSE, the execution of the loop ends.` |
|        - |  942 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|        - |  943 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|        - |  944 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|        - |  945 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|        - |  946 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|        - |  947 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|        - |  948 | ` *  of using the for truth expression.` |
|        - |  949 | ` */` |
|   128390 |  950 | `PH7_PRIVATE sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|        5 |  951 | `{` |
|   128395 |  952 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   128395 |  953 | `	GenBlock *pForBlock = 0;` |
|        - |  954 | `	sxu32 nFalseJump;` |
|        - |  955 | `	sxu32 nLine;` |
|        - |  956 | `	sxi32 rc;` |
|   128395 |  957 | `	nLine = pGen->pIn->nLine;` |
|        - |  958 | `	/* Jump the 'for' keyword */` |
|   128395 |  959 | `	pGen->pIn++;` |
|   128395 |  960 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  961 | `		/* Syntax error */` |
|      ! 0 |  962 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|      ! 0 |  963 | `		if( rc == SXERR_ABORT ){` |
|        - |  964 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  965 | `			return SXERR_ABORT;` |
|        - |  966 | `		}` |
|      ! 0 |  967 | `		return SXRET_OK;` |
|        - |  968 | `	}` |
|        - |  969 | `	/* Jump the left parenthesis '(' */` |
|   128395 |  970 | `	pGen->pIn++;` |
|        - |  971 | `	/* Delimit the init-expr;condition;post-expr */` |
|   128395 |  972 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   128395 |  973 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  974 | `		/* Empty expression */` |
|      ! 0 |  975 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|      ! 0 |  976 | `		if( rc == SXERR_ABORT ){` |
|        - |  977 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  978 | `			return SXERR_ABORT;` |
|        - |  979 | `		}` |
|        - |  980 | `		/* Synchronize */` |
|      ! 0 |  981 | `		pGen->pIn = pEnd;` |
|      ! 0 |  982 | `		if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 |  983 | `			pGen->pIn++;` |
|      ! 0 |  984 | `		}` |
|      ! 0 |  985 | `		return SXRET_OK;` |
|        - |  986 | `	}` |
|        - |  987 | `	/* Swap token streams */` |
|   128395 |  988 | `	pTmp = pGen->pEnd;` |
|   128395 |  989 | `	pGen->pEnd = pEnd;` |
|        - |  990 | `	/* for() clauses are the ONLY place php's grammar allows a comma-separated` |
|        - |  991 | `	 * expression list, so the comma operator is permitted for their duration` |
|        - |  992 | `	 * (see GenStateTreeHasComma). A closure body nested inside a clause is` |
|        - |  993 | `	 * compiled through this same window — recorded as a known leniency. */` |
|   128395 |  994 | `	pGen->nCommaExprOk++;` |
|        - |  995 | `	/* Compile initialization expressions if available */` |
|   128395 |  996 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  997 | `	/* Pop operand lvalues */` |
|   128395 |  998 | `	if( rc == SXERR_ABORT ){` |
|        - |  999 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1000 | `		return SXERR_ABORT;` |
|   128395 | 1001 | `	}else if( rc != SXERR_EMPTY ){` |
|   116735 | 1002 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    58365 | 1003 | `	}` |
|   128395 | 1004 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - | 1005 | `		/* Syntax error */` |
|      ! 0 | 1006 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|      ! 0 | 1007 | `		if( rc == SXERR_ABORT ){` |
|        - | 1008 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1009 | `			return SXERR_ABORT;` |
|        - | 1010 | `		}` |
|      ! 0 | 1011 | `		return SXRET_OK;` |
|        - | 1012 | `	}` |
|        - | 1013 | `	/* Jump the trailing ';' */` |
|   128395 | 1014 | `	pGen->pIn++;` |
|        - | 1015 | `	/* Create the loop block */` |
|   128395 | 1016 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   128395 | 1017 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1018 | `		return SXERR_ABORT;` |
|        - | 1019 | `	}` |
|        - | 1020 | `	/* Deffer continue jumps */` |
|   128395 | 1021 | `	pForBlock->bPostContinue = TRUE;` |
|        - | 1022 | `	/* Compile the condition */` |
|   128395 | 1023 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   128395 | 1024 | `	if( rc == SXERR_ABORT ){` |
|        - | 1025 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1026 | `		return SXERR_ABORT;` |
|   128395 | 1027 | `	}else if( rc != SXERR_EMPTY ){` |
|        - | 1028 | `		/* Emit the false jump */` |
|   116735 | 1029 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|        - | 1030 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   116735 | 1031 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    58365 | 1032 | `	}` |
|   128395 | 1033 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - | 1034 | `		/* Syntax error */` |
|        6 | 1035 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|        6 | 1036 | `		if( rc == SXERR_ABORT ){` |
|        - | 1037 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1038 | `			return SXERR_ABORT;` |
|        - | 1039 | `		}` |
|        6 | 1040 | `		return SXRET_OK;` |
|        - | 1041 | `	}` |
|        - | 1042 | `	/* Jump the trailing ';' */` |
|   128391 | 1043 | `	pGen->pIn++;` |
|        - | 1044 | `	/* Save the post condition stream */` |
|   128391 | 1045 | `	pPostStart = pGen->pIn;` |
|        - | 1046 | `	/* Compile the loop body — OUTSIDE the comma window (the body is ordinary` |
|        - | 1047 | ``	 * php, so `(1, 2)` inside it is the parse error it should be). */`` |
|   128391 | 1048 | `	pGen->nCommaExprOk--;` |
|   128391 | 1049 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   128391 | 1050 | `	pGen->pEnd = pTmp;` |
|   128391 | 1051 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   128391 | 1052 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 1053 | `		return SXERR_ABORT;` |
|        - | 1054 | `	}` |
|        - | 1055 | `	/* Fix post-continue jumps */` |
|   128391 | 1056 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|        - | 1057 | `		JumpFixup *aPost;` |
|        - | 1058 | `		VmInstr *pInstr;` |
|        - | 1059 | `		sxu32 nJumpDest;` |
|        - | 1060 | `		sxu32 n;` |
|    11675 | 1061 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|    11675 | 1062 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|    38889 | 1063 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|    27219 | 1064 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|    27219 | 1065 | `			if( pInstr ){` |
|        - | 1066 | `				/* Fix jump */` |
|    27219 | 1067 | `				pInstr->iP2 = nJumpDest;` |
|    13607 | 1068 | `			}` |
|    13612 | 1069 | `		}` |
|     5835 | 1070 | `	}` |
|        - | 1071 | `	/* compile the post-expressions if available */` |
|   128391 | 1072 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|      ! 0 | 1073 | `		pPostStart++;` |
|      ! 0 | 1074 | `	}` |
|   128391 | 1075 | `	if( pPostStart < pEnd ){` |
|        - | 1076 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   116733 | 1077 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   116733 | 1078 | `		pGen->nCommaExprOk++; /* post-expressions are a clause list again */` |
|   116733 | 1079 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   116733 | 1080 | `		pGen->nCommaExprOk--;` |
|   116733 | 1081 | `		if( pGen->pIn < pGen->pEnd ){` |
|        - | 1082 | `			/* Syntax error */` |
|      ! 0 | 1083 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|      ! 0 | 1084 | `			if( rc == SXERR_ABORT ){` |
|        - | 1085 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 1086 | `				return SXERR_ABORT;` |
|        - | 1087 | `			}` |
|      ! 0 | 1088 | `			return SXRET_OK;` |
|        - | 1089 | `		}` |
|   116733 | 1090 | `		RE_SWAP_DELIMITER(pGen);` |
|   116733 | 1091 | `		if( rc == SXERR_ABORT ){` |
|        - | 1092 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1093 | `			return SXERR_ABORT;` |
|   116733 | 1094 | `		}else if( rc != SXERR_EMPTY){` |
|        - | 1095 | `			/* Pop operand lvalue */` |
|   116733 | 1096 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    58364 | 1097 | `		}` |
|    58364 | 1098 | `	}` |
|        - | 1099 | `	/* Emit the unconditional jump to the start of the loop */` |
|   128391 | 1100 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|        - | 1101 | `	/* Fix all jumps now the destination is resolved */` |
|   128391 | 1102 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 1103 | `	/* Release the loop block */` |
|   128391 | 1104 | `	GenStateLeaveBlock(pGen,0);` |
|        - | 1105 | `	/* Statement successfully compiled */` |
|   128391 | 1106 | `	return SXRET_OK;` |
|    64200 | 1107 | `}` |
|        - | 1108 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|        - | 1109 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|        - | 1110 | ` * are allowed.` |
|        - | 1111 | ` */` |
|   463578 | 1112 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 1113 | `{` |
|   463583 | 1114 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|   463583 | 1115 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|        - | 1116 | `		/* Unexpected expression */` |
|      ! 0 | 1117 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 1118 | `			"foreach: Expecting a variable name");` |
|      ! 0 | 1119 | `		if( rc != SXERR_ABORT ){` |
|      ! 0 | 1120 | `			rc = SXERR_INVALID;` |
|      ! 0 | 1121 | `		}` |
|      ! 0 | 1122 | `	}` |
|   463583 | 1123 | `	return rc;` |
|        5 | 1124 | `}` |
|        - | 1125 | `/*` |
|        - | 1126 | ` * Compile the 'foreach' statement.` |
|        - | 1127 | ` * According to the PHP language reference` |
|        - | 1128 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|        - | 1129 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|        - | 1130 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|        - | 1131 | ` *  is a minor but useful extension of the first:` |
|        - | 1132 | ` *  foreach (array_expression as $value)` |
|        - | 1133 | ` *    statement` |
|        - | 1134 | ` *  foreach (array_expression as $key => $value)` |
|        - | 1135 | ` *   statement` |
|        - | 1136 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|        - | 1137 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|        - | 1138 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|        - | 1139 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|        - | 1140 | ` *  to the variable $key on each loop.` |
|        - | 1141 | ` *  Note:` |
|        - | 1142 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|        - | 1143 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|        - | 1144 | ` *  Note:` |
|        - | 1145 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|        - | 1146 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|        - | 1147 | ` *  or after the foreach without resetting it.` |
|        - | 1148 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|        - | 1149 | ` *  of copying the value.` |
|        - | 1150 | ` */` |
|   327268 | 1151 | `PH7_PRIVATE sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|        5 | 1152 | `{` |
|   327273 | 1153 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|   327273 | 1154 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|   327273 | 1155 | `	GenBlock *pForeachBlock = 0;` |
|        - | 1156 | `	ph7_foreach_info *pInfo;` |
|        - | 1157 | `	sxu32 nFalseJump;` |
|        - | 1158 | `	VmInstr *pInstr;` |
|        - | 1159 | `	sxu32 nLine;` |
|        - | 1160 | `	sxi32 rc;` |
|   327273 | 1161 | `	nLine = pGen->pIn->nLine;` |
|        - | 1162 | `	/* Jump the 'foreach' keyword */` |
|   327273 | 1163 | `	pGen->pIn++;` |
|   327273 | 1164 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1165 | `		/* Syntax error */` |
|      ! 0 | 1166 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|      ! 0 | 1167 | `		if( rc == SXERR_ABORT ){` |
|        - | 1168 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1169 | `			return SXERR_ABORT;` |
|        - | 1170 | `		}` |
|      ! 0 | 1171 | `		goto Synchronize;` |
|        - | 1172 | `	}` |
|        - | 1173 | `	/* Jump the left parenthesis '(' */` |
|   327273 | 1174 | `	pGen->pIn++;` |
|        - | 1175 | `	/* Create the loop block */` |
|   327273 | 1176 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|   327273 | 1177 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1178 | `		return SXERR_ABORT;` |
|        - | 1179 | `	}` |
|        - | 1180 | `	/* Delimit the expression */` |
|   327273 | 1181 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   327273 | 1182 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - | 1183 | `		/* Empty expression */` |
|      ! 0 | 1184 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|      ! 0 | 1185 | `		if( rc == SXERR_ABORT ){` |
|        - | 1186 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1187 | `			return SXERR_ABORT;` |
|        - | 1188 | `		}` |
|        - | 1189 | `		/* Synchronize */` |
|      ! 0 | 1190 | `		pGen->pIn = pEnd;` |
|      ! 0 | 1191 | `		if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 | 1192 | `			pGen->pIn++;` |
|      ! 0 | 1193 | `		}` |
|      ! 0 | 1194 | `		return SXRET_OK;` |
|        - | 1195 | `	}` |
|        - | 1196 | `	/* Compile the array expression */` |
|   327273 | 1197 | `	pCur = pGen->pIn;` |
|  1854913 | 1198 | `	while( pCur < pEnd ){` |
|  1854913 | 1199 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|   358383 | 1200 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|   358383 | 1201 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|        - | 1202 | `				/* Break with the first 'as' found */` |
|   327273 | 1203 | `				break;` |
|        - | 1204 | `			}` |
|    15555 | 1205 | `		}` |
|        - | 1206 | `		/* Advance the stream cursor */` |
|  1527645 | 1207 | `		pCur++;` |
|        5 | 1208 | `	}` |
|   327273 | 1209 | `	if( pCur <= pGen->pIn ){` |
|      ! 0 | 1210 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 1211 | `			"foreach: Missing array/object expression");` |
|      ! 0 | 1212 | `		if( rc == SXERR_ABORT ){` |
|        - | 1213 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1214 | `			return SXERR_ABORT;` |
|        - | 1215 | `		}` |
|      ! 0 | 1216 | `		goto Synchronize;` |
|        - | 1217 | `	}` |
|        - | 1218 | `	/* Swap token streams */` |
|   327273 | 1219 | `	pTmp = pGen->pEnd;` |
|   327273 | 1220 | `	pGen->pEnd = pCur;` |
|   327273 | 1221 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   327273 | 1222 | `	if( rc == SXERR_ABORT ){` |
|        - | 1223 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1224 | `		return SXERR_ABORT;` |
|        - | 1225 | `	}` |
|        - | 1226 | `	/* Update token stream */` |
|   327273 | 1227 | `	while(pGen->pIn < pCur ){` |
|      ! 0 | 1228 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 1229 | `		if( rc == SXERR_ABORT ){` |
|        - | 1230 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1231 | `			return SXERR_ABORT;` |
|        - | 1232 | `		}` |
|      ! 0 | 1233 | `		pGen->pIn++;` |
|      ! 0 | 1234 | `	}` |
|   327273 | 1235 | `	pCur++; /* Jump the 'as' keyword */` |
|   327273 | 1236 | `	pGen->pIn = pCur;` |
|   327273 | 1237 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 1238 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|      ! 0 | 1239 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1240 | `			return SXERR_ABORT;` |
|        - | 1241 | `		}` |
|      ! 0 | 1242 | `	}` |
|        - | 1243 | `	/* Create the foreach context */` |
|   327273 | 1244 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|   327273 | 1245 | `	if( pInfo == 0 ){` |
|      ! 0 | 1246 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|      ! 0 | 1247 | `		return SXERR_ABORT;` |
|        - | 1248 | `	}` |
|        - | 1249 | `	/* Zero the structure */` |
|   327273 | 1250 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|        - | 1251 | `	/* Initialize structure fields */` |
|   327273 | 1252 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|        - | 1253 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|        - | 1254 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|        - | 1255 | `	 * '=>'. */` |
|   327273 | 1256 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|   327273 | 1257 | `	if( pCur < pEnd ){` |
|        - | 1258 | `		/* Compile the expression holding the key name */` |
|   136339 | 1259 | `		if( pGen->pIn >= pCur ){` |
|      ! 0 | 1260 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|      ! 0 | 1261 | `			if( rc == SXERR_ABORT ){` |
|        - | 1262 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1263 | `				return SXERR_ABORT;` |
|        - | 1264 | `			}` |
|      ! 0 | 1265 | `		}else{` |
|   136339 | 1266 | `			pGen->pEnd = pCur;` |
|   136339 | 1267 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|   136339 | 1268 | `			if( rc == SXERR_ABORT ){` |
|        - | 1269 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1270 | `				return SXERR_ABORT;` |
|        - | 1271 | `			}` |
|   136339 | 1272 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|   136339 | 1273 | `			if( pInstr->p3 ){` |
|        - | 1274 | `				/* Record key name */` |
|   136339 | 1275 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    68167 | 1276 | `			}` |
|   136339 | 1277 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|        - | 1278 | `		}` |
|   136339 | 1279 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    68167 | 1280 | `	}` |
|   327273 | 1281 | `	pGen->pEnd = pEnd;` |
|   327273 | 1282 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 1283 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|      ! 0 | 1284 | `		if( rc == SXERR_ABORT ){` |
|        - | 1285 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1286 | `			return SXERR_ABORT;` |
|        - | 1287 | `		}` |
|      ! 0 | 1288 | `		goto Synchronize;` |
|        - | 1289 | `	}` |
|   327273 | 1290 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|       33 | 1291 | `		pGen->pIn++;` |
|        - | 1292 | `		/* Pass by reference  */` |
|       33 | 1293 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       15 | 1294 | `	}` |
|        - | 1295 | `	/* Check if the value target is list() */` |
|   327273 | 1296 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        8 | 1297 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|        - | 1298 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|        - | 1299 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|        - | 1300 | `		 */` |
|        - | 1301 | `		static int iForeachListCnt = 0;` |
|        - | 1302 | `		char zTmp[128];` |
|        - | 1303 | `		sxu32 nLen;` |
|        - | 1304 | `		char *zDup;` |
|       10 | 1305 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|       10 | 1306 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       10 | 1307 | `		if( zDup == 0 ){` |
|      ! 0 | 1308 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1309 | `			return SXERR_ABORT;` |
|        - | 1310 | `		}` |
|       10 | 1311 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|        - | 1312 | `		/* Save list() token boundaries */` |
|       10 | 1313 | `		pListStart = pGen->pIn;` |
|        - | 1314 | `		/* Advance past list(...) — validate parentheses */` |
|       10 | 1315 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|       10 | 1316 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1317 | `			/* php: syntax error, unexpected variable "$x", expecting "(" */` |
|        3 | 1318 | `			rc = PH7_GenSyntaxError(pGen,pGen->pIn < pEnd ? pGen->pIn : 0,"\"(\"");` |
|        3 | 1319 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1320 | `				return SXERR_ABORT;` |
|        - | 1321 | `			}` |
|        3 | 1322 | `			goto Synchronize;` |
|        - | 1323 | `		}` |
|        7 | 1324 | `		pGen->pIn++; /* Jump '(' */` |
|        7 | 1325 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|        7 | 1326 | `		if( pListEnd >= pEnd ){` |
|      ! 0 | 1327 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1328 | `				"foreach: Missing closing ')' after list");` |
|      ! 0 | 1329 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1330 | `				return SXERR_ABORT;` |
|        - | 1331 | `			}` |
|      ! 0 | 1332 | `			goto Synchronize;` |
|        - | 1333 | `		}` |
|        7 | 1334 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|        7 | 1335 | `		pListEnd = pGen->pIn;` |
|        7 | 1336 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|   327268 | 1337 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|        - | 1338 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|        - | 1339 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|        - | 1340 | `		 */` |
|        - | 1341 | `		static int iForeachShortListCnt = 0;` |
|        - | 1342 | `		char zTmp[128];` |
|        - | 1343 | `		sxu32 nLen;` |
|        - | 1344 | `		char *zDup;` |
|       17 | 1345 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       17 | 1346 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       17 | 1347 | `		if( zDup == 0 ){` |
|      ! 0 | 1348 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1349 | `			return SXERR_ABORT;` |
|        - | 1350 | `		}` |
|       17 | 1351 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|        - | 1352 | `		/* Save [...] token boundaries */` |
|       17 | 1353 | `		pListStart = pGen->pIn;` |
|        - | 1354 | `		/* Advance past [...] */` |
|       17 | 1355 | `		pGen->pIn++; /* Jump '[' */` |
|       17 | 1356 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       17 | 1357 | `		if( pListEnd >= pEnd ){` |
|      ! 0 | 1358 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1359 | `				"foreach: Missing closing ']' after short list");` |
|      ! 0 | 1360 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1361 | `				return SXERR_ABORT;` |
|        - | 1362 | `			}` |
|      ! 0 | 1363 | `			goto Synchronize;` |
|        - | 1364 | `		}` |
|       17 | 1365 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       17 | 1366 | `		pListEnd = pGen->pIn;` |
|       17 | 1367 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|        9 | 1368 | `	}else{` |
|        - | 1369 | `		/* Compile the expression holding the value name */` |
|   327249 | 1370 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|   327249 | 1371 | `		if( rc == SXERR_ABORT ){` |
|        - | 1372 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1373 | `			return SXERR_ABORT;` |
|        - | 1374 | `		}` |
|   327249 | 1375 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|   327249 | 1376 | `		if( pInstr->p3 ){` |
|        - | 1377 | `			/* Record value name */` |
|   327249 | 1378 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|   163622 | 1379 | `		}` |
|        - | 1380 | `	}` |
|        - | 1381 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|   327271 | 1382 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|        - | 1383 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   327271 | 1384 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|        - | 1385 | `	/* Record the first instruction to execute */` |
|   327271 | 1386 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|        - | 1387 | `	/* Emit the FOREACH_STEP instruction */` |
|   327271 | 1388 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|        - | 1389 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   327271 | 1390 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|        - | 1391 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|   327271 | 1392 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|        - | 1393 | `		SyToken *pSavedIn,*pSavedEnd;` |
|        - | 1394 | `		/* Load the temporary variable holding the current value onto the stack.` |
|        - | 1395 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|        - | 1396 | `		 */` |
|       23 | 1397 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|        - | 1398 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|        - | 1399 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|        - | 1400 | `		 * picks up the delimiter and the variable names inside.` |
|        - | 1401 | `		 */` |
|       23 | 1402 | `		pSavedIn = pGen->pIn;` |
|       23 | 1403 | `		pSavedEnd = pGen->pEnd;` |
|       23 | 1404 | `		pGen->pIn = pListStart;` |
|       23 | 1405 | `		pGen->pEnd = pListEnd;` |
|       23 | 1406 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       17 | 1407 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|        9 | 1408 | `		}else{` |
|        7 | 1409 | `			rc = PH7_CompileList(&(*pGen),0);` |
|        - | 1410 | `		}` |
|       23 | 1411 | `		pGen->pIn = pSavedIn;` |
|       23 | 1412 | `		pGen->pEnd = pSavedEnd;` |
|       23 | 1413 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1414 | `			return SXERR_ABORT;` |
|        - | 1415 | `		}` |
|        - | 1416 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|       23 | 1417 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       11 | 1418 | `	}` |
|        - | 1419 | `	/* Compile the loop body */` |
|   327271 | 1420 | `	pGen->pIn = &pEnd[1];` |
|   327271 | 1421 | `	pGen->pEnd = pTmp;` |
|   327271 | 1422 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|   327271 | 1423 | `	if( rc == SXERR_ABORT ){` |
|        - | 1424 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1425 | `		return SXERR_ABORT;` |
|        - | 1426 | `	}` |
|        - | 1427 | `	/* Emit the unconditional jump to the start of the loop */` |
|   327271 | 1428 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|        - | 1429 | `	/* Fix all jumps now the destination is resolved */` |
|   327271 | 1430 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 1431 | `	/* Release the loop block */` |
|   327271 | 1432 | `	GenStateLeaveBlock(pGen,0);` |
|        - | 1433 | `	/* Statement successfully compiled */` |
|   327271 | 1434 | `	return SXRET_OK;` |
|        1 | 1435 | `Synchronize:` |
|        - | 1436 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - | 1437 | `	 * compiling this erroneous block.` |
|        - | 1438 | `	 */` |
|        3 | 1439 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 | 1440 | `		pGen->pIn++;` |
|      ! 0 | 1441 | `	}` |
|        3 | 1442 | `	return SXRET_OK;` |
|   163639 | 1443 | `}` |
|        - | 1444 | `/*` |
|        - | 1445 | ` * Compile the infamous if/elseif/else if/else statements.` |
|        - | 1446 | ` * According to the PHP language reference` |
|        - | 1447 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|        - | 1448 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|        - | 1449 | ` *  that is similar to that of C:` |
|        - | 1450 | ` *  if (expr)` |
|        - | 1451 | ` *   statement` |
|        - | 1452 | ` *  else construct:` |
|        - | 1453 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|        - | 1454 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|        - | 1455 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|        - | 1456 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|        - | 1457 | ` *   $b, and a is NOT greater than b otherwise.` |
|        - | 1458 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|        - | 1459 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|        - | 1460 | ` *  elseif` |
|        - | 1461 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|        - | 1462 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|        - | 1463 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|        - | 1464 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|        - | 1465 | ` *   than b, a equal to b or a is smaller than b:` |
|        - | 1466 | ` *   <?php` |
|        - | 1467 | ` *    if ($a > $b) {` |
|        - | 1468 | ` *     echo "a is bigger than b";` |
|        - | 1469 | ` *    } elseif ($a == $b) {` |
|        - | 1470 | ` *     echo "a is equal to b";` |
|        - | 1471 | ` *    } else {` |
|        - | 1472 | ` *     echo "a is smaller than b";` |
|        - | 1473 | ` *    }` |
|        - | 1474 | ` *    ?>` |
|        - | 1475 | ` */` |
|  2430426 | 1476 | `PH7_PRIVATE sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|        5 | 1477 | `{` |
|  2430431 | 1478 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  2430431 | 1479 | `	GenBlock *pCondBlock = 0;` |
|        - | 1480 | `	sxu32 nJumpIdx;` |
|        - | 1481 | `	sxu32 nKeyID;` |
|        - | 1482 | `	sxi32 rc;` |
|        - | 1483 | `	/* Jump the 'if' keyword */` |
|  2430431 | 1484 | `	pGen->pIn++;` |
|  2430431 | 1485 | `	pToken = pGen->pIn;` |
|        - | 1486 | `	/* Create the conditional block */` |
|  2430431 | 1487 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  2430431 | 1488 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1489 | `		return SXERR_ABORT;` |
|        - | 1490 | `	}` |
|        - | 1491 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|  1359066 | 1492 | `	for(;;){` |
|  2718137 | 1493 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1494 | `			/* Syntax error */` |
|      ! 0 | 1495 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 1496 | `				pToken--;` |
|      ! 0 | 1497 | `			}` |
|      ! 0 | 1498 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|      ! 0 | 1499 | `			if( rc == SXERR_ABORT ){` |
|        - | 1500 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 1501 | `				return SXERR_ABORT;` |
|        - | 1502 | `			}` |
|      ! 0 | 1503 | `			goto Synchronize;` |
|        - | 1504 | `		}` |
|        - | 1505 | `		/* Jump the left parenthesis '(' */` |
|  2718137 | 1506 | `		pToken++;` |
|        - | 1507 | `		/* Delimit the condition */` |
|  2718137 | 1508 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  2718137 | 1509 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|        - | 1510 | `			/* Syntax error */` |
|        6 | 1511 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 1512 | `				pToken--;` |
|      ! 0 | 1513 | `			}` |
|        6 | 1514 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|        6 | 1515 | `			if( rc == SXERR_ABORT ){` |
|        - | 1516 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 1517 | `				return SXERR_ABORT;` |
|        - | 1518 | `			}` |
|        6 | 1519 | `			goto Synchronize;` |
|        - | 1520 | `		}` |
|        - | 1521 | `		/* Swap token streams */` |
|  2718133 | 1522 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|        - | 1523 | `		/* Compile the condition */` |
|  2718133 | 1524 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 1525 | `		/* Update token stream */` |
|  2718133 | 1526 | `		while(pGen->pIn < pEnd ){` |
|      ! 0 | 1527 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 | 1528 | `			pGen->pIn++;` |
|      ! 0 | 1529 | `		}` |
|  2718133 | 1530 | `		pGen->pIn  = &pEnd[1];` |
|  2718133 | 1531 | `		pGen->pEnd = pTmp;` |
|  2718133 | 1532 | `		if( rc == SXERR_ABORT ){` |
|        - | 1533 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|        3 | 1534 | `			return SXERR_ABORT;` |
|        - | 1535 | `		}` |
|        - | 1536 | `		/* Emit the false jump */` |
|  2718131 | 1537 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|        - | 1538 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  2718131 | 1539 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|        - | 1540 | `		/* Compile the body */` |
|  2718131 | 1541 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  2718131 | 1542 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1543 | `			return SXERR_ABORT;` |
|        - | 1544 | `		}` |
|  2718131 | 1545 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   521206 | 1546 | `			break;` |
|        - | 1547 | `		}` |
|        - | 1548 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|  1675729 | 1549 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  1675729 | 1550 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|  1166103 | 1551 | `			break;` |
|        - | 1552 | `		}` |
|        - | 1553 | `		/* Emit the unconditional jump */` |
|   509631 | 1554 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|        - | 1555 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   509631 | 1556 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   509631 | 1557 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   307417 | 1558 | `			pToken = &pGen->pIn[1];` |
|   307417 | 1559 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    85530 | 1560 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|   110965 | 1561 | `					break;` |
|        - | 1562 | `			}` |
|    85497 | 1563 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    42746 | 1564 | `		}` |
|   287711 | 1565 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|        - | 1566 | `		/* Synchronize cursors */` |
|   287711 | 1567 | `		pToken = pGen->pIn;` |
|        - | 1568 | `		/* Fix the false jump */` |
|   287711 | 1569 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|        5 | 1570 | `	} /* For(;;) */` |
|        - | 1571 | `	/* Fix the false jump */` |
|  2430425 | 1572 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  2430425 | 1573 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|  1388018 | 1574 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|        - | 1575 | `			/* Compile the else block */` |
|   221925 | 1576 | `			pGen->pIn++;` |
|   221925 | 1577 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   221925 | 1578 | `			if( rc == SXERR_ABORT ){` |
|        - | 1579 |  |
|      ! 0 | 1580 | `				return SXERR_ABORT;` |
|        - | 1581 | `			}` |
|   110960 | 1582 | `	}` |
|  2430425 | 1583 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|        - | 1584 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  2430425 | 1585 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|        - | 1586 | `	/* Release the conditional block */` |
|  2430425 | 1587 | `	GenStateLeaveBlock(pGen,0);` |
|        - | 1588 | `	/* Statement successfully compiled */` |
|  2430425 | 1589 | `	return SXRET_OK;` |
|        2 | 1590 | `Synchronize:` |
|        - | 1591 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|        - | 1592 | `	 */` |
|       34 | 1593 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       30 | 1594 | `		pGen->pIn++;` |
|        2 | 1595 | `	}` |
|        6 | 1596 | `	return SXRET_OK;` |
|  1215218 | 1597 | `}` |
|        - | 1598 | `/*` |
|        - | 1599 | ` * Compile the global construct.` |
|        - | 1600 | ` * According to the PHP language reference` |
|        - | 1601 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|        - | 1602 | ` *  to be used in that function.` |
|        - | 1603 | ` *  Example #1 Using global` |
|        - | 1604 | ` *  <?php` |
|        - | 1605 | ` *   $a = 1;` |
|        - | 1606 | ` *   $b = 2;` |
|        - | 1607 | ` *   function Sum()` |
|        - | 1608 | ` *   {` |
|        - | 1609 | ` *    global $a, $b;` |
|        - | 1610 | ` *    $b = $a + $b;` |
|        - | 1611 | ` *   }` |
|        - | 1612 | ` *   Sum();` |
|        - | 1613 | ` *   echo $b;` |
|        - | 1614 | ` *  ?>` |
|        - | 1615 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|        - | 1616 | ` *  all references to either variable will refer to the global version. There is no limit` |
|        - | 1617 | ` *  to the number of global variables that can be manipulated by a function.` |
|        - | 1618 | ` */` |
|       36 | 1619 | `PH7_PRIVATE sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|        5 | 1620 | `{` |
|       41 | 1621 | `	SyToken *pTmp,*pNext = 0;` |
|        - | 1622 | `	sxi32 nExpr;` |
|        - | 1623 | `	sxi32 rc;` |
|        - | 1624 | `	/* Jump the 'global' keyword */` |
|       41 | 1625 | `	pGen->pIn++;` |
|       41 | 1626 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|        - | 1627 | `		/* Nothing to process */` |
|      ! 0 | 1628 | `		return SXRET_OK;` |
|        - | 1629 | `	}` |
|       41 | 1630 | `	pTmp = pGen->pEnd;` |
|       41 | 1631 | `	nExpr = 0;` |
|       87 | 1632 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       51 | 1633 | `		if( pGen->pIn < pNext ){` |
|       51 | 1634 | `			pGen->pEnd = pNext;` |
|       51 | 1635 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 | 1636 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|      ! 0 | 1637 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1638 | `					return SXERR_ABORT;` |
|        - | 1639 | `				}` |
|      ! 0 | 1640 | `			}else{` |
|       51 | 1641 | `				pGen->pIn++;` |
|       51 | 1642 | `				if( pGen->pIn >= pGen->pEnd ){` |
|        - | 1643 | `					/* Emit a warning */` |
|      ! 0 | 1644 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|      ! 0 | 1645 | `				}else{` |
|       51 | 1646 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       51 | 1647 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 1648 | `						return SXERR_ABORT;` |
|       51 | 1649 | `					}else if(rc != SXERR_EMPTY ){` |
|       51 | 1650 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|       51 | 1651 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|        - | 1652 | `							/* Variable name, not a constant */` |
|       51 | 1653 | `							pLast->iP1 = 0;` |
|       23 | 1654 | `						}` |
|       51 | 1655 | `						nExpr++;` |
|       23 | 1656 | `					}` |
|        - | 1657 | `				}` |
|        - | 1658 | `			}` |
|       23 | 1659 | `		}` |
|        - | 1660 | `		/* Next expression in the stream */` |
|       51 | 1661 | `		pGen->pIn = pNext;` |
|        - | 1662 | `		/* Jump trailing commas */` |
|       61 | 1663 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       15 | 1664 | `			pGen->pIn++;` |
|        5 | 1665 | `		}` |
|        5 | 1666 | `	}` |
|        - | 1667 | `	/* Restore token stream */` |
|       41 | 1668 | `	pGen->pEnd = pTmp;` |
|       41 | 1669 | `	if( nExpr > 0 ){` |
|        - | 1670 | `		/* Emit the uplink instruction */` |
|       41 | 1671 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|       18 | 1672 | `	}` |
|       41 | 1673 | `	return SXRET_OK;` |
|       23 | 1674 | `}` |
|        - | 1675 | `/*` |
|        - | 1676 | ` * Compile the return statement.` |
|        - | 1677 | ` * According to the PHP language reference` |
|        - | 1678 | ` *  If called from within a function, the return() statement immediately ends execution` |
|        - | 1679 | ` *  of the current function, and returns its argument as the value of the function call.` |
|        - | 1680 | ` *  return() will also end the execution of an eval() statement or script file.` |
|        - | 1681 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|        - | 1682 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|        - | 1683 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|        - | 1684 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|        - | 1685 | ` *  from within the main script file, then script execution end.` |
|        - | 1686 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|        - | 1687 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|        - | 1688 | ` *  should do so as PHP has less work to do in this case.` |
|        - | 1689 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|        - | 1690 | ` */` |
|  3624222 | 1691 | `PH7_PRIVATE sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|        5 | 1692 | `{` |
|  3624227 | 1693 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|        - | 1694 | `	sxi32 rc;` |
|  3624227 | 1695 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  3624227 | 1696 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|        - | 1697 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|        - | 1698 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|        - | 1699 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|        - | 1700 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|        - | 1701 | `	 * normally below so token processing stays consistent. */` |
|  9542011 | 1702 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|  5917789 | 1703 | `		pFuncBlock = pFuncBlock->pParent;` |
|        5 | 1704 | `	}` |
|  3624222 | 1705 | `	if( pFuncBlock && pFuncBlock->pUserData` |
|  3624211 | 1706 | `	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){` |
|        3 | 1707 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1708 | `			"A never-returning function must not return");` |
|        3 | 1709 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1710 | `			return SXERR_ABORT;` |
|        - | 1711 | `		}` |
|        1 | 1712 | `	}` |
|        - | 1713 | `	/* Jump the 'return' keyword */` |
|  3624227 | 1714 | `	pGen->pIn++;` |
|  3624227 | 1715 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - | 1716 | `		/* Compile the expression */` |
|  3527065 | 1717 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  3527065 | 1718 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1719 | `			return SXERR_ABORT;` |
|  3527065 | 1720 | `		}else if(rc != SXERR_EMPTY ){` |
|  3527065 | 1721 | `			nRet = 1;` |
|  1763530 | 1722 | `		}` |
|  1763530 | 1723 | `	}` |
|        - | 1724 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|        - | 1725 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|        - | 1726 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|        - | 1727 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|  3624227 | 1728 | `	if( pGen->bInGenerator ){` |
|     3919 | 1729 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|     3919 | 1730 | `		return SXRET_OK;` |
|        - | 1731 | `	}` |
|        - | 1732 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|        - | 1733 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|        - | 1734 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|        - | 1735 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|        - | 1736 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|  3620313 | 1737 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|  3620313 | 1738 | `	return SXRET_OK;` |
|  1812116 | 1739 | `}` |
|        - | 1740 | `/*` |
|        - | 1741 | ` * Compile a yield expression.` |
|        - | 1742 | ` * Called from the expression code generator when a yield node is encountered.` |
|        - | 1743 | ` * Handles: yield, yield $value, yield $key => $value` |
|        - | 1744 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|        - | 1745 | ` */` |
|    15932 | 1746 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|        5 | 1747 | `{` |
|        - | 1748 | `	SyToken *pTmp, *pSplit;` |
|    15937 | 1749 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|    15937 | 1750 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|        - | 1751 | `	sxi32 rc;` |
|     7966 | 1752 | `	(void)iCompileFlag;` |
|        - | 1753 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|    15937 | 1754 | `	pGen->pIn++;` |
|        - | 1755 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|        - | 1756 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|        - | 1757 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|        - | 1758 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|        - | 1759 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|    15932 | 1760 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|     8001 | 1761 | `		&& pGen->pIn->sData.nByte == 4` |
|       72 | 1762 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|       67 | 1763 | `		pGen->pIn++; /* Skip 'from' */` |
|       67 | 1764 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       67 | 1765 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1766 | `			return SXERR_ABORT;` |
|        - | 1767 | `		}` |
|       67 | 1768 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 | 1769 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|      ! 0 | 1770 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|        - | 1771 | `				"Missing expression after 'yield from'");` |
|      ! 0 | 1772 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1773 | `				return SXERR_ABORT;` |
|        - | 1774 | `			}` |
|      ! 0 | 1775 | `		}` |
|       67 | 1776 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|       67 | 1777 | `		return SXRET_OK;` |
|        - | 1778 | `	}` |
|    15875 | 1779 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - | 1780 | `		/* Bare yield — no value */` |
|        3 | 1781 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|        3 | 1782 | `		return SXRET_OK;` |
|        - | 1783 | `	}` |
|        - | 1784 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|    15873 | 1785 | `	pSplit = 0;` |
|        - | 1786 | `	{` |
|    15873 | 1787 | `		SyToken *pCur = pGen->pIn;` |
|    15873 | 1788 | `		sxi32 nNest = 0;` |
|    47423 | 1789 | `		while( pCur < pGen->pEnd ){` |
|    47113 | 1790 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       18 | 1791 | `				nNest++;` |
|    47105 | 1792 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       18 | 1793 | `				nNest--;` |
|    47089 | 1794 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|    15563 | 1795 | `				pSplit = pCur;` |
|    15563 | 1796 | `				break;` |
|        - | 1797 | `			}` |
|    31555 | 1798 | `			pCur++;` |
|        5 | 1799 | `		}` |
|        - | 1800 | `	}` |
|    15873 | 1801 | `	pTmp = pGen->pEnd;` |
|    15873 | 1802 | `	if( pSplit ){` |
|        - | 1803 | `		/* yield $key => $value */` |
|    15563 | 1804 | `		pGen->pEnd = pSplit;` |
|    15563 | 1805 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|    15563 | 1806 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|    15563 | 1807 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|    15563 | 1808 | `		pGen->pEnd = pTmp;` |
|    15563 | 1809 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|    15563 | 1810 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|    15563 | 1811 | `		iP1 = 1;` |
|    15563 | 1812 | `		iP2 = 1;` |
|     7784 | 1813 | `	}else{` |
|        - | 1814 | `		/* yield $value */` |
|      315 | 1815 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      315 | 1816 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      315 | 1817 | `		if( rc != SXERR_EMPTY ){` |
|      315 | 1818 | `			iP1 = 1;` |
|      155 | 1819 | `		}` |
|        - | 1820 | `	}` |
|    15873 | 1821 | `	pGen->pEnd = pTmp;` |
|    15873 | 1822 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|    15873 | 1823 | `	return SXRET_OK;` |
|     7971 | 1824 | `}` |
|        - | 1825 | `/*` |
|        - | 1826 | ` * Compile the die/exit language construct.` |
|        - | 1827 | ` * The role of these constructs is to terminate execution of the script.` |
|        - | 1828 | ` * Shutdown functions will always be executed even if exit() is called.` |
|        - | 1829 | ` */` |
|       94 | 1830 | `PH7_PRIVATE sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|        5 | 1831 | `{` |
|       99 | 1832 | `	sxi32 nExpr = 0;` |
|        - | 1833 | `	sxi32 rc;` |
|        - | 1834 | `	/* Jump the die/exit keyword */` |
|       99 | 1835 | `	pGen->pIn++;` |
|       99 | 1836 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - | 1837 | `		/* Compile the expression */` |
|       99 | 1838 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       99 | 1839 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1840 | `			return SXERR_ABORT;` |
|       99 | 1841 | `		}else if(rc != SXERR_EMPTY ){` |
|       99 | 1842 | `			nExpr = 1;` |
|       47 | 1843 | `		}` |
|       47 | 1844 | `	}` |
|        - | 1845 | `	/* Emit the HALT instruction */` |
|       99 | 1846 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|       99 | 1847 | `	return SXRET_OK;` |
|       52 | 1848 | `}` |
|        - | 1849 | `/*` |
|        - | 1850 | ` * Compile the 'echo' language construct.` |
|        - | 1851 | ` */` |
|    17544 | 1852 | `PH7_PRIVATE sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|        5 | 1853 | `{` |
|    17549 | 1854 | `	SyToken *pTmp,*pNext = 0;` |
|    17549 | 1855 | `	sxu32 nLine = pGen->pIn->nLine;` |
|    17549 | 1856 | `	int nExpr = 0;      /* expressions actually compiled */` |
|    17549 | 1857 | `	int bExpectMore = 1;/* after 'echo' or a comma an expression is REQUIRED */` |
|        - | 1858 | `	sxi32 rc;` |
|        - | 1859 | `	/* Jump the 'echo' keyword */` |
|    17549 | 1860 | `	pGen->pIn++;` |
|        - | 1861 | `	/* Compile arguments one after one */` |
|    17549 | 1862 | `	pTmp = pGen->pEnd;` |
|    45487 | 1863 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|    27949 | 1864 | `		if( pGen->pIn < pNext ){` |
|    27949 | 1865 | `			pGen->pEnd = pNext;` |
|    27949 | 1866 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|    27949 | 1867 | `			if( rc == SXERR_ABORT ){` |
|        5 | 1868 | `				return SXERR_ABORT;` |
|    27945 | 1869 | `			}else if( rc != SXERR_EMPTY ){` |
|        - | 1870 | `				/* Emit the consume instruction */` |
|    27921 | 1871 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    27921 | 1872 | `				nExpr++;` |
|    27921 | 1873 | `				bExpectMore = 0;` |
|    13958 | 1874 | `			}` |
|    13970 | 1875 | `		}` |
|        - | 1876 | `		/* Jump trailing commas (php: exactly one between expressions; a` |
|        - | 1877 | `		 * dangling or doubled comma is a parse error, enforced below) */` |
|    38351 | 1878 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|    10413 | 1879 | `			if( bExpectMore ){` |
|        - | 1880 | `				/* two commas in a row */` |
|        3 | 1881 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,pNext->nLine,` |
|        - | 1882 | `					"syntax error, unexpected token \",\"");` |
|        3 | 1883 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1884 | `			}` |
|    10411 | 1885 | `			bExpectMore = 1;` |
|    10411 | 1886 | `			pNext++;` |
|        5 | 1887 | `		}` |
|    27943 | 1888 | `		pGen->pIn = pNext;` |
|        5 | 1889 | `	}` |
|        - | 1890 | `	/* Restore token stream */` |
|    17543 | 1891 | `	pGen->pEnd = pTmp;` |
|    17543 | 1892 | `	if( nExpr == 0 \|\| bExpectMore ){` |
|        - | 1893 | ``		/* `echo ;` or `echo expr, ;` — php rejects both */`` |
|       32 | 1894 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - | 1895 | `			"syntax error, unexpected token \";\"");` |
|       32 | 1896 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1897 | `	}` |
|    17515 | 1898 | `	return SXRET_OK;` |
|     8777 | 1899 | `}` |
|        - | 1900 | `/*` |
|        - | 1901 | ` * Compile the static statement.` |
|        - | 1902 | ` * According to the PHP language reference` |
|        - | 1903 | ` *  Another important feature of variable scoping is the static variable.` |
|        - | 1904 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|        - | 1905 | ` *  when program execution leaves this scope.` |
|        - | 1906 | ` *  Static variables also provide one way to deal with recursive functions.` |
|        - | 1907 | ` * Symisc eXtension.` |
|        - | 1908 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|        - | 1909 | ` *  the zend engine would allow only simple scalar value.` |
|        - | 1910 | ` *  Example` |
|        - | 1911 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|        - | 1912 | ` *    Refer to the official documentation for more information on this feature.` |
|        - | 1913 | ` */` |
|    11670 | 1914 | `PH7_PRIVATE sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|        5 | 1915 | `{` |
|        - | 1916 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|        - | 1917 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|        - | 1918 | `	GenBlock *pBlock;` |
|        - | 1919 | `	SyString *pName;` |
|        - | 1920 | `	char *zDup;` |
|        - | 1921 | `	sxu32 nLine;` |
|        - | 1922 | `	sxi32 rc;` |
|        - | 1923 | ``	/* `static function () {}` / `static fn () =>` at statement position is an`` |
|        - | 1924 | `	 * EXPRESSION statement (a bare static closure), not a static-variable` |
|        - | 1925 | `	 * declaration — hand it to the expression compiler (php accepts it). */` |
|    11670 | 1926 | `	if( &pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & PH7_TK_KEYWORD)` |
|     5841 | 1927 | `	 && (SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FUNCTION` |
|        1 | 1928 | `	  \|\| SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FN) ){` |
|        3 | 1929 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        3 | 1930 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1931 | `			return SXERR_ABORT;` |
|        3 | 1932 | `		}else if( rc != SXERR_EMPTY ){` |
|        3 | 1933 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        1 | 1934 | `		}` |
|        3 | 1935 | `		return SXRET_OK;` |
|        - | 1936 | `	}` |
|        - | 1937 | `	/* Jump the static keyword */` |
|    11673 | 1938 | `	nLine = pGen->pIn->nLine;` |
|    11673 | 1939 | `	pGen->pIn++;` |
|        - | 1940 | `	/* Extract the enclosing function if any */` |
|    11673 | 1941 | `	pBlock = pGen->pCurrent;` |
|    23341 | 1942 | `	while( pBlock ){` |
|    23341 | 1943 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|    11673 | 1944 | `			break;` |
|        - | 1945 | `		}` |
|        - | 1946 | `		/* Point to the upper block */` |
|    11673 | 1947 | `		pBlock = pBlock->pParent;` |
|        5 | 1948 | `	}` |
|    11673 | 1949 | `	if( pBlock == 0 ){` |
|        - | 1950 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|      ! 0 | 1951 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 | 1952 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|      ! 0 | 1953 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1954 | `				return SXERR_ABORT;` |
|        - | 1955 | `			}` |
|      ! 0 | 1956 | `			goto Synchronize;` |
|        - | 1957 | `		}` |
|        - | 1958 | `		/* Compile the expression holding the variable */` |
|      ! 0 | 1959 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      ! 0 | 1960 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1961 | `			return SXERR_ABORT;` |
|      ! 0 | 1962 | `		}else if( rc != SXERR_EMPTY ){` |
|        - | 1963 | `			/* Emit the POP instruction */` |
|      ! 0 | 1964 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      ! 0 | 1965 | `		}` |
|      ! 0 | 1966 | `		return SXRET_OK;` |
|        - | 1967 | `	}` |
|    11673 | 1968 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        - | 1969 | `	/* Make sure we are dealing with a valid statement */` |
|    11673 | 1970 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|    11666 | 1971 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        3 | 1972 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|        3 | 1973 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1974 | `				return SXERR_ABORT;` |
|        - | 1975 | `			}` |
|        3 | 1976 | `			goto Synchronize;` |
|        - | 1977 | `	}` |
|    11671 | 1978 | `	pGen->pIn++;` |
|        - | 1979 | `	/* Extract variable name */` |
|    11671 | 1980 | `	pName = &pGen->pIn->sData;` |
|    11671 | 1981 | `	pGen->pIn++; /* Jump the var name */` |
|    11671 | 1982 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|      ! 0 | 1983 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 1984 | `		goto Synchronize;` |
|        - | 1985 | `	}` |
|        - | 1986 | `	/* Initialize the structure describing the static variable */` |
|    11671 | 1987 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|    11671 | 1988 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|        - | 1989 | `	/* Duplicate variable name */` |
|    11671 | 1990 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    11671 | 1991 | `	if( zDup == 0 ){` |
|      ! 0 | 1992 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1993 | `		return SXERR_ABORT;` |
|        - | 1994 | `	}` |
|    11671 | 1995 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|        - | 1996 | `	/* Check if we have an expression to compile */` |
|    11671 | 1997 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|        - | 1998 | `		SySet *pInstrContainer;` |
|        - | 1999 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|        - | 2000 | `		 * Static variable can take any complex expression including function` |
|        - | 2001 | `		 * call as their initialization value.` |
|        - | 2002 | `		 * Example:` |
|        - | 2003 | `		 *		static $var = foo(1,4+5,bar());` |
|        - | 2004 | `		 */` |
|    11671 | 2005 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|        - | 2006 | `		/* Swap bytecode container */` |
|    11671 | 2007 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    11671 | 2008 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|        - | 2009 | `		/* Compile the expression */` |
|    11671 | 2010 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 2011 | `		/* Emit the done instruction */` |
|    11671 | 2012 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        - | 2013 | `		/* Restore default bytecode container */` |
|    11671 | 2014 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     5833 | 2015 | `	}` |
|        - | 2016 | `	/* Finally save the compiled static variable in the appropriate container */` |
|    11671 | 2017 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|    11671 | 2018 | `	return SXRET_OK;` |
|        1 | 2019 | `Synchronize:` |
|        - | 2020 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|        - | 2021 | `	 * statement.` |
|        - | 2022 | `	 */` |
|        5 | 2023 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|        3 | 2024 | `		pGen->pIn++;` |
|        1 | 2025 | `	}` |
|        3 | 2026 | `	return SXRET_OK;` |
|     5840 | 2027 | `}` |
|        - | 2028 | `/*` |
|        - | 2029 | ` * Compile the var statement.` |
|        - | 2030 | ` * Symisc Extension:` |
|        - | 2031 | ` *      var statement can be used outside of a class definition.` |
|        - | 2032 | ` */` |
|        2 | 2033 | `PH7_PRIVATE sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|        1 | 2034 | `{` |
|        - | 2035 | `` 	/* `var` is a PROPERTY modifier and nothing else. A class body's `var $x;` `` |
|        - | 2036 | `	 * is compiled by compile_class.c, never here, so reaching this statement` |
|        - | 2037 | ``	 * handler means `var` appeared outside a class — which php rejects as a`` |
|        - | 2038 | `	 * parse error. PHL used to compile it as an ordinary expression statement,` |
|        - | 2039 | ``	 * so top-level `var $x = 1;` silently ran (a Symisc extension). */`` |
|        3 | 2040 | `	PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|        3 | 2041 | `	return SXERR_ABORT;` |
|        1 | 2042 | `}` |
|        - | 2043 | `/*` |
|        - | 2044 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|        - | 2045 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|        - | 2046 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|        - | 2047 | ` */` |
|        - | 2048 | `/*` |
|        - | 2049 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|        - | 2050 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|        - | 2051 | ` * hash and any shared references), this creates a new literal entry with the` |
|        - | 2052 | ` * qualified name and updates the instruction's operand index.` |
|        - | 2053 | ` *` |
|        - | 2054 | ` * Resolution order:` |
|        - | 2055 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|        - | 2056 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|        - | 2057 | ` *   3. Otherwise return the original literal index unchanged.` |
|        - | 2058 | ` *` |
|        - | 2059 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|        - | 2060 | ` * came from an import (step 1) and 0 otherwise.` |
|        - | 2061 | ` * Returns the (possibly new) literal index.` |
|        - | 2062 | ` */` |
|  6442016 | 2063 | `PH7_PRIVATE sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|        5 | 2064 | `{` |
|        - | 2065 | `	ph7_value *pLit;` |
|        - | 2066 | `	const char *zLit;` |
|        - | 2067 | `	SyString sQualified;` |
|        - | 2068 | `	sxu32 nLit;` |
|        - | 2069 | `	sxu32 k;` |
|        - | 2070 | `	sxu32 nNewIdx;` |
|        - | 2071 | `	int hasNsSep;` |
|        - | 2072 | `	SyHashEntry *pImport;` |
|        - | 2073 | `	ph7_value *pNew;` |
|  6442021 | 2074 | `	if( pFromImport ){` |
|  5252539 | 2075 | `		*pFromImport = 0;` |
|  2626267 | 2076 | `	}` |
|  6442021 | 2077 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  6442021 | 2078 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|      ! 0 | 2079 | `		return nOrigIdx;` |
|        - | 2080 | `	}` |
|  6442021 | 2081 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  6442021 | 2082 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|        - | 2083 | `	/* Skip if already qualified (contains backslash) */` |
|  6442021 | 2084 | `	hasNsSep = 0;` |
| 77896433 | 2085 | `	for( k = 0; k < nLit; k++ ){` |
| 71454443 | 2086 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 35727211 | 2087 | `	}` |
|  6442021 | 2088 | `	if( hasNsSep ){` |
|       28 | 2089 | `		return nOrigIdx;` |
|        - | 2090 | `	}` |
|        - | 2091 | `	/* Check use imports first (works even outside namespaces) */` |
|  6441995 | 2092 | `	SyBlobReset(&pGen->sWorker);` |
|  6441995 | 2093 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  6441995 | 2094 | `	if( pImport ){` |
|       41 | 2095 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       41 | 2096 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|       41 | 2097 | `		if( pFromImport ){` |
|       18 | 2098 | `			*pFromImport = 1;` |
|        8 | 2099 | `		}` |
|       23 | 2100 | `	}else{` |
|  6441959 | 2101 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  6441821 | 2102 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|        - | 2103 | `		}` |
|        - | 2104 | `		/* Prepend current namespace */` |
|      143 | 2105 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      143 | 2106 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      143 | 2107 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|        - | 2108 | `	}` |
|        - | 2109 | `	/* Look up or create a new literal for the qualified name */` |
|      179 | 2110 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|      179 | 2111 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|       79 | 2112 | `		return nNewIdx; /* Already interned */` |
|        - | 2113 | `	}` |
|      105 | 2114 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      105 | 2115 | `	if( pNew == 0 ){` |
|      ! 0 | 2116 | `		return nOrigIdx; /* OOM, fall back to original */` |
|        - | 2117 | `	}` |
|      105 | 2118 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      105 | 2119 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      105 | 2120 | `	return nNewIdx;` |
|  3221013 | 2121 | `}` |
|        - | 2122 | `/*` |
|        - | 2123 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|        - | 2124 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|        - | 2125 | ` */` |
|   553748 | 2126 | `PH7_PRIVATE void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|        5 | 2127 | `{` |
|        - | 2128 | `	SyHashEntry *pImport;` |
|   553753 | 2129 | `	const char *zName = pName->zString;` |
|   553753 | 2130 | `	sxu32 nName = pName->nByte;` |
|   553753 | 2131 | `	sxu32 nFirst = 0;` |
|        - | 2132 | `	/* php resolves a name through use-imports on its LEADING segment: an` |
|        - | 2133 | ``	 * unqualified `C` maps the whole name (`use X\C;` -> X\C), while a QUALIFIED`` |
|        - | 2134 | ``	 * `A\B\C` maps just `A` (`use X\A;` -> X\A\B\C) and keeps the `\B\C` tail.`` |
|        - | 2135 | `	 * The old code looked up the whole qualified string (which never matches a` |
|        - | 2136 | `	 * single-segment import alias) and then blindly prefixed the current` |
|        - | 2137 | ``	 * namespace, so `use PHPUnit\Framework; extends Framework\TestCase` resolved`` |
|        - | 2138 | `	 * to Ns\Framework\TestCase. Mirror GenStateResolveNamespaceLiteral here. */` |
|  7041701 | 2139 | `	while( nFirst < nName && zName[nFirst] != '\\' ){ nFirst++; }` |
|   553753 | 2140 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)zName,nFirst);` |
|   553753 | 2141 | `	if( pImport ){` |
|       26 | 2142 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       26 | 2143 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|       26 | 2144 | `		SyBlobAppend(pOut,zName + nFirst,nName - nFirst); /* the \B\C tail, if any */` |
|       26 | 2145 | `		return;` |
|        - | 2146 | `	}` |
|        - | 2147 | `	/* Prepend current namespace if active */` |
|   553731 | 2148 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       14 | 2149 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       14 | 2150 | `		SyBlobAppend(pOut,"\\",1);` |
|        6 | 2151 | `	}` |
|   553731 | 2152 | `	SyBlobAppend(pOut,zName,nName);` |
|   276879 | 2153 | `}` |
|        - | 2154 | `/*` |
|        - | 2155 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|        - | 2156 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|        - | 2157 | ` * The caller must release pOut when done.` |
|        - | 2158 | ` */` |
|   530728 | 2159 | `PH7_PRIVATE void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|        5 | 2160 | `{` |
|   530733 | 2161 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     3979 | 2162 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     3979 | 2163 | `		SyBlobAppend(pOut,"\\",1);` |
|     1987 | 2164 | `	}` |
|   530733 | 2165 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   530733 | 2166 | `}` |
|        - | 2167 | `/*` |
|        - | 2168 | ` * Compile a namespace statement` |
|        - | 2169 | ` * According to the PHP language reference manual` |
|        - | 2170 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|        - | 2171 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|        - | 2172 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|        - | 2173 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|        - | 2174 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|        - | 2175 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|        - | 2176 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|        - | 2177 | ` *  programming world.` |
|        - | 2178 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|        - | 2179 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|        - | 2180 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|        - | 2181 | ` *  classes/functions/constants.` |
|        - | 2182 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|        - | 2183 | ` *  readability of source code.` |
|        - | 2184 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|        - | 2185 | ` *  Here is an example of namespace syntax in PHP:` |
|        - | 2186 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|        - | 2187 | ` *       class MyClass {}` |
|        - | 2188 | ` *       function myfunction() {}` |
|        - | 2189 | ` *       const MYCONST = 1;` |
|        - | 2190 | ` *       $a = new MyClass;` |
|        - | 2191 | ` *       $c = new \my\name\MyClass;` |
|        - | 2192 | ` *       $a = strlen('hi');` |
|        - | 2193 | ` *       $d = namespace\MYCONST;` |
|        - | 2194 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|        - | 2195 | ` *       echo constant($d);` |
|        - | 2196 | ` * NOTE` |
|        - | 2197 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|        - | 2198 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|        - | 2199 | ` */` |
|        - | 2200 | `/*` |
|        - | 2201 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|        - | 2202 | ` */` |
|       14 | 2203 | `PH7_PRIVATE const char * TokenTypeName(sxu32 nType)` |
|        4 | 2204 | `{` |
|       18 | 2205 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|       11 | 2206 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|       11 | 2207 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|       11 | 2208 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|       11 | 2209 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|       11 | 2210 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|        3 | 2211 | `	return "token";` |
|       11 | 2212 | `}` |
|     4018 | 2213 | `PH7_PRIVATE sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|        5 | 2214 | `{` |
|        - | 2215 | `	sxu32 nLine;` |
|        - | 2216 | `	sxi32 rc;` |
|     4023 | 2217 | `	nLine = pGen->pIn->nLine;` |
|     4023 | 2218 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|        - | 2219 | `	/* Reset namespace and clear previous use imports */` |
|     4023 | 2220 | `	SyBlobReset(&pGen->sNamespace);` |
|     4023 | 2221 | `	SyHashRelease(&pGen->hUseImports);` |
|     4023 | 2222 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|     4023 | 2223 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     4023 | 2224 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|     4023 | 2225 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     4023 | 2226 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|     4023 | 2227 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - | 2228 | `		/* Global namespace (bare "namespace;") */` |
|      ! 0 | 2229 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|      ! 0 | 2230 | `		return SXRET_OK;` |
|        - | 2231 | `	}` |
|     4023 | 2232 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|        - | 2233 | `		/* namespace; — switch to global namespace */` |
|      ! 0 | 2234 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|      ! 0 | 2235 | `		return SXRET_OK;` |
|        - | 2236 | `	}` |
|     4023 | 2237 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|        - | 2238 | `		/* namespace { } — global namespace block */` |
|        5 | 2239 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|        5 | 2240 | `		return SXRET_OK;` |
|        - | 2241 | `	}` |
|        - | 2242 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     8115 | 2243 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     4101 | 2244 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|        - | 2245 | `			/* Append backslash separator */` |
|       46 | 2246 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       46 | 2247 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|       21 | 2248 | `			}` |
|       25 | 2249 | `		}else{` |
|        - | 2250 | `			/* Append identifier */` |
|     4059 | 2251 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        - | 2252 | `		}` |
|     4101 | 2253 | `		pGen->pIn++;` |
|        5 | 2254 | `	}` |
|        - | 2255 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|        - | 2256 | `	 * at the correct program counter, not just the last one compiled. */` |
|        - | 2257 | `	{` |
|     4019 | 2258 | `		char *zNsDup = 0;` |
|     4019 | 2259 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     6023 | 2260 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     4012 | 2261 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     2006 | 2262 | `		}` |
|     4019 | 2263 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|        - | 2264 | `	}` |
|     4019 | 2265 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        8 | 2266 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - | 2267 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|        4 | 2268 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        6 | 2269 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2270 | `			return SXERR_ABORT;` |
|        - | 2271 | `		}` |
|        2 | 2272 | `	}` |
|     4019 | 2273 | `	return SXRET_OK;` |
|     2014 | 2274 | `}` |
|        - | 2275 | `/*` |
|        - | 2276 | ` * Compile the 'use' statement` |
|        - | 2277 | ` * According to the PHP language reference manual` |
|        - | 2278 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|        - | 2279 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|        - | 2280 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|        - | 2281 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|        - | 2282 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|        - | 2283 | ` *  a function or constant is not supported.` |
|        - | 2284 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|        - | 2285 | ` * NOTE` |
|        - | 2286 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|        - | 2287 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|        - | 2288 | ` */` |
|       80 | 2289 | `PH7_PRIVATE sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|        5 | 2290 | `{` |
|        - | 2291 | `	sxu32 nLine;` |
|        - | 2292 | `	sxi32 rc;` |
|        - | 2293 | `	SyBlob sPath;` |
|        - | 2294 | `	SyString sAlias;` |
|        - | 2295 | `	SyToken *pLast;` |
|        - | 2296 | `	char *zDup;` |
|        - | 2297 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|        - | 2298 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|        - | 2299 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|       85 | 2300 | `	nLine = pGen->pIn->nLine;` |
|       85 | 2301 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|        - | 2302 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|       85 | 2303 | `	iUseType = 0;` |
|       85 | 2304 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       30 | 2305 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|       30 | 2306 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|       16 | 2307 | `			iUseType = 1;` |
|       16 | 2308 | `			pGen->pIn++;` |
|       23 | 2309 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|       16 | 2310 | `			iUseType = 2;` |
|       16 | 2311 | `			pGen->pIn++;` |
|        7 | 2312 | `		}` |
|       14 | 2313 | `	}` |
|        - | 2314 | `	/* Select target hash tables based on import type */` |
|       85 | 2315 | `	switch( iUseType ){` |
|        7 | 2316 | `		case 1:` |
|       16 | 2317 | `			pGenHash = &pGen->hUseFuncImports;` |
|       16 | 2318 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|       16 | 2319 | `			break;` |
|        7 | 2320 | `		case 2:` |
|       16 | 2321 | `			pGenHash = &pGen->hUseConstImports;` |
|       16 | 2322 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|       16 | 2323 | `			break;` |
|       26 | 2324 | `		default:` |
|       57 | 2325 | `			pGenHash = &pGen->hUseImports;` |
|       57 | 2326 | `			pVmHash = &pGen->pVm->hUseImports;` |
|       52 | 2327 | `			break;` |
|        - | 2328 | `	}` |
|       85 | 2329 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|        - | 2330 | `	/* Process one or more use declarations separated by commas */` |
|       41 | 2331 | `	for(;;){` |
|       87 | 2332 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 2333 | `			break;` |
|        - | 2334 | `		}` |
|       87 | 2335 | `		SyBlobReset(&sPath);` |
|       87 | 2336 | `		pLast = 0;` |
|        - | 2337 | `		/* Collect the full namespace path */` |
|      301 | 2338 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|      219 | 2339 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|      151 | 2340 | `				pLast = pGen->pIn;` |
|      151 | 2341 | `				if( SyBlobLength(&sPath) > 0 ){` |
|       73 | 2342 | `					SyBlobAppend(&sPath,"\\",1);` |
|       34 | 2343 | `				}` |
|      151 | 2344 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       73 | 2345 | `			}` |
|      219 | 2346 | `			pGen->pIn++;` |
|        5 | 2347 | `		}` |
|       87 | 2348 | `		if( pLast == 0 ){` |
|        - | 2349 | `			/* Empty path */` |
|        6 | 2350 | `			break;` |
|        - | 2351 | `		}` |
|        - | 2352 | `		/* Default alias is the last component of the path */` |
|       83 | 2353 | `		sAlias = pLast->sData;` |
|        - | 2354 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|       78 | 2355 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       56 | 2356 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|       27 | 2357 | `			pGen->pIn++; /* Jump 'as' */` |
|       27 | 2358 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|       27 | 2359 | `				sAlias = pGen->pIn->sData;` |
|       27 | 2360 | `				pGen->pIn++;` |
|       12 | 2361 | `			}` |
|       12 | 2362 | `		}` |
|        - | 2363 | `		/* Check for duplicate import alias (per-type) */` |
|       83 | 2364 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|        8 | 2365 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 2366 | `				"Cannot use %.*s as %z because the name is already in use",` |
|        4 | 2367 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|        6 | 2368 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2369 | `				SyBlobRelease(&sPath);` |
|      ! 0 | 2370 | `				return SXERR_ABORT;` |
|        - | 2371 | `			}` |
|        2 | 2372 | `		}` |
|        - | 2373 | `		/* Register the import: alias -> FQN.` |
|        - | 2374 | `		 * Strings are allocated from the VM pool allocator and freed` |
|        - | 2375 | `		 * when the entire VM is released. SyHashRelease does not free` |
|        - | 2376 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|      122 | 2377 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       78 | 2378 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|       83 | 2379 | `		if( zDup ){` |
|       83 | 2380 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|       83 | 2381 | `			if( pVmHash ){` |
|        - | 2382 | `				/* Class imports: populate VM table directly (class resolution` |
|        - | 2383 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|       55 | 2384 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       55 | 2385 | `				if( zAliasDup ){` |
|       55 | 2386 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|       25 | 2387 | `				}` |
|       25 | 2388 | `			}` |
|       83 | 2389 | `			if( iUseType == 2 ){` |
|        - | 2390 | `				/* Const imports: emit a runtime instruction so imports are` |
|        - | 2391 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|       16 | 2392 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       16 | 2393 | `				if( zAliasDup ){` |
|        - | 2394 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|        - | 2395 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|        - | 2396 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|       16 | 2397 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|       16 | 2398 | `					if( azPair ){` |
|       16 | 2399 | `						azPair[0] = zAliasDup;` |
|       16 | 2400 | `						azPair[1] = zDup;` |
|       16 | 2401 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|        7 | 2402 | `					}` |
|        7 | 2403 | `				}` |
|        7 | 2404 | `			}` |
|       39 | 2405 | `		}` |
|        - | 2406 | `		/* Check for comma (multiple use declarations) */` |
|       83 | 2407 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|        3 | 2408 | `			pGen->pIn++;` |
|        2 | 2409 | `		}else{` |
|       43 | 2410 | `			break;` |
|        - | 2411 | `		}` |
|        1 | 2412 | `	}` |
|       85 | 2413 | `	SyBlobRelease(&sPath);` |
|       85 | 2414 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        4 | 2415 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|        2 | 2416 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        3 | 2417 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2418 | `			return SXERR_ABORT;` |
|        - | 2419 | `		}` |
|        1 | 2420 | `	}` |
|       85 | 2421 | `	return SXRET_OK;` |
|       45 | 2422 | `}` |
|        - | 2423 | `/*` |
|        - | 2424 | ` * Compile the stupid 'declare' language construct.` |
|        - | 2425 | ` *` |
|        - | 2426 | ` * According to the PHP language reference manual.` |
|        - | 2427 | ` *  The declare construct is used to set execution directives for a block of code.` |
|        - | 2428 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|        - | 2429 | ` *  declare (directive)` |
|        - | 2430 | ` *   statement` |
|        - | 2431 | ` * The directive section allows the behavior of the declare block to be set.` |
|        - | 2432 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|        - | 2433 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|        - | 2434 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|        - | 2435 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|        - | 2436 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|        - | 2437 | ` * <?php` |
|        - | 2438 | ` * // these are the same:` |
|        - | 2439 | ` * // you can use this:` |
|        - | 2440 | ` * declare(ticks=1) {` |
|        - | 2441 | ` *   // entire script here` |
|        - | 2442 | ` * }` |
|        - | 2443 | ` * // or you can use this:` |
|        - | 2444 | ` * declare(ticks=1);` |
|        - | 2445 | ` * // entire script here` |
|        - | 2446 | ` * ?>` |
|        - | 2447 | ` *` |
|        - | 2448 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|        - | 2449 | ` */` |
|        - | 2450 | `/*` |
|        - | 2451 | ` * Match a directive name against a known literal (case-insensitive).` |
|        - | 2452 | ` */` |
|       80 | 2453 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|        5 | 2454 | `{` |
|      120 | 2455 | `	return SyStringLength(pName) == nWant` |
|       80 | 2456 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|        5 | 2457 | `}` |
|        - | 2458 |  |
|       44 | 2459 | `PH7_PRIVATE sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|        5 | 2460 | `{` |
|       49 | 2461 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       49 | 2462 | `	SyToken *pBodyEnd = 0;` |
|        - | 2463 | `	SyToken *pBodyStart;` |
|        - | 2464 | `	SyToken *pCursor;` |
|        - | 2465 | `	int bHasStrictTypes;` |
|        - | 2466 | `	int bBlockForm;` |
|        - | 2467 | `	int bPlacementOk;` |
|        - | 2468 | `	sxi32 rc;` |
|       49 | 2469 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|       49 | 2470 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|        5 | 2471 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|        5 | 2472 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2473 | `			return SXERR_ABORT;` |
|        - | 2474 | `		}` |
|        5 | 2475 | `		goto Synchro;` |
|        - | 2476 | `	}` |
|       45 | 2477 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|       45 | 2478 | `	pBodyStart = pGen->pIn;` |
|        - | 2479 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|       45 | 2480 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|       45 | 2481 | `	if( pBodyEnd >= pGen->pEnd ){` |
|      ! 0 | 2482 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|      ! 0 | 2483 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2484 | `			return SXERR_ABORT;` |
|        - | 2485 | `		}` |
|      ! 0 | 2486 | `		return SXRET_OK;` |
|        - | 2487 | `	}` |
|        - | 2488 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|        - | 2489 | `	 * now delimits the comma-separated directive list. */` |
|       45 | 2490 | `	pGen->pIn = &pBodyEnd[1];` |
|       45 | 2491 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      ! 0 | 2492 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|      ! 0 | 2493 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2494 | `			return SXERR_ABORT;` |
|        - | 2495 | `		}` |
|      ! 0 | 2496 | `	}` |
|       45 | 2497 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|       45 | 2498 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|       45 | 2499 | `	bHasStrictTypes = 0;` |
|        - | 2500 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|        - | 2501 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|        - | 2502 | `	 * directive appears anywhere in the list, before validating values. */` |
|       45 | 2503 | `	pCursor = pBodyStart;` |
|       57 | 2504 | `	while( pCursor < pBodyEnd ){` |
|       53 | 2505 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|       45 | 2506 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|       41 | 2507 | `				bHasStrictTypes = 1;` |
|       41 | 2508 | `				break;` |
|        - | 2509 | `			}` |
|        2 | 2510 | `		}` |
|       14 | 2511 | `		pCursor++;` |
|        2 | 2512 | `	}` |
|       45 | 2513 | `	if( bHasStrictTypes && bBlockForm ){` |
|        3 | 2514 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2515 | `			"strict_types declaration must not use block mode");` |
|        3 | 2516 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        3 | 2517 | `		return SXRET_OK;` |
|        - | 2518 | `	}` |
|       43 | 2519 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|        6 | 2520 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2521 | `			"strict_types declaration must be the very first statement in the script");` |
|        6 | 2522 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        6 | 2523 | `		return SXRET_OK;` |
|        - | 2524 | `	}` |
|        - | 2525 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|       39 | 2526 | `	pCursor = pBodyStart;` |
|       73 | 2527 | `	while( pCursor < pBodyEnd ){` |
|        - | 2528 | `		SyToken *pNameTok;` |
|        - | 2529 | `		SyToken *pEqTok;` |
|        - | 2530 | `		SyToken *pValTok;` |
|        - | 2531 | `		SyString *pDirName;` |
|        - | 2532 | `		int bIsStrict;` |
|        - | 2533 | `		int iStrictValue;` |
|       41 | 2534 | `		pNameTok = pCursor;` |
|       41 | 2535 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 2536 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2537 | `				"declare: Expecting a directive name");` |
|      ! 0 | 2538 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2539 | `			return SXRET_OK;` |
|        - | 2540 | `		}` |
|       41 | 2541 | `		pEqTok = pNameTok + 1;` |
|       41 | 2542 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|      ! 0 | 2543 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2544 | `				"declare: Expecting '=' after directive name");` |
|      ! 0 | 2545 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2546 | `			return SXRET_OK;` |
|        - | 2547 | `		}` |
|       41 | 2548 | `		pValTok = pEqTok + 1;` |
|       41 | 2549 | `		if( pValTok >= pBodyEnd ){` |
|      ! 0 | 2550 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2551 | `				"declare: Expecting value after '='");` |
|      ! 0 | 2552 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2553 | `			return SXRET_OK;` |
|        - | 2554 | `		}` |
|       41 | 2555 | `		pDirName = &pNameTok->sData;` |
|       41 | 2556 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|       41 | 2557 | `		if( bIsStrict ){` |
|        - | 2558 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|        - | 2559 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|       37 | 2560 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      ! 0 | 2561 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2562 | `					"declare(strict_types) value must be a literal");` |
|      ! 0 | 2563 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2564 | `				return SXRET_OK;` |
|        - | 2565 | `			}` |
|       37 | 2566 | `			iStrictValue = -1;` |
|       37 | 2567 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|       37 | 2568 | `				const char *zv = SyStringData(&pValTok->sData);` |
|       37 | 2569 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|       37 | 2570 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|       35 | 2571 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|       16 | 2572 | `			}` |
|       37 | 2573 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|        3 | 2574 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2575 | `					"strict_types declaration must have 0 or 1 as its value");` |
|        3 | 2576 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        3 | 2577 | `				return SXRET_OK;` |
|        - | 2578 | `			}` |
|       35 | 2579 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|       21 | 2580 | `		}else if( DeclareNameIs(pDirName, "encoding", sizeof("encoding")-1) ){` |
|        - | 2581 | `			/* php always ignores declare(encoding=...) unless it was built with` |
|        - | 2582 | `			 * Zend multibyte, and says so in these exact words. */` |
|        3 | 2583 | `			PH7_GenCompileError(&(*pGen),E_WARNING,nLine,` |
|        - | 2584 | `				"declare(encoding=...) ignored because Zend multibyte feature is turned off by settings");` |
|        1 | 2585 | `		}else{` |
|        - | 2586 | `			/* Other directives (ticks and friends) are accepted as no-ops.` |
|        - | 2587 | `			 * This used to emit a NOTICE naming the upstream engine and its` |
|        - | 2588 | `			 * version ("the declare construct is a no-op in the current release` |
|        - | 2589 | `			 * of the PH7(2.1.4) engine") — php prints nothing at all for` |
|        - | 2590 | ``			 * `declare(ticks=1)`, and leaking the old engine's branding into`` |
|        - | 2591 | `			 * user-visible diagnostics was wrong on its own. */` |
|        - | 2592 | `		}` |
|       39 | 2593 | `		pCursor = pValTok + 1;` |
|        - | 2594 | `		/* Consume separating comma (or end). */` |
|       39 | 2595 | `		if( pCursor < pBodyEnd ){` |
|        3 | 2596 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|      ! 0 | 2597 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2598 | `					"declare: Expecting ',' or ')' after directive value");` |
|      ! 0 | 2599 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2600 | `				return SXRET_OK;` |
|        - | 2601 | `			}` |
|        3 | 2602 | `			pCursor++;` |
|        1 | 2603 | `		}` |
|        5 | 2604 | `	}` |
|        - | 2605 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|        - | 2606 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|        - | 2607 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|       37 | 2608 | `	return SXRET_OK;` |
|        2 | 2609 | `Synchro:` |
|        - | 2610 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|       15 | 2611 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       11 | 2612 | `		pGen->pIn++;` |
|        1 | 2613 | `	}` |
|        5 | 2614 | `	return SXRET_OK;` |
|       27 | 2615 | `}` |
|        - | 2616 | `/*` |
|        - | 2617 | ` * Compile a class constant.` |
|        - | 2618 | ` * According to the PHP language reference manual` |
|        - | 2619 | ` *  Class Constants` |
|        - | 2620 | ` *   It is possible to define constant values on a per-class basis remaining` |
|        - | 2621 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|        - | 2622 | ` *   you don't use the $ symbol to declare or use them.` |
|        - | 2623 | ` *   The value must be a constant expression, not (for example) a variable,` |
|        - | 2624 | ` *   a property, a result of a mathematical operation, or a function call.` |
|        - | 2625 | ` *   It's also possible for interfaces to have constants.` |
|        - | 2626 | ` * Symisc eXtension.` |
|        - | 2627 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|        - | 2628 | ` *  the zend engine would allow only simple scalar value.` |
|        - | 2629 | ` *  Example:` |
|        - | 2630 | ` *   class Test{` |
|        - | 2631 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|        - | 2632 | ` *   };` |
|        - | 2633 | ` *   var_dump(TEST::MyConst);` |
|        - | 2634 | ` *   Refer to the official documentation for more information on the powerful extension` |
|        - | 2635 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|        - | 2636 | ` */` |
|        - | 2637 | `/*` |
|        - | 2638 | ` * Exception handling.` |
|        - | 2639 | ` *  According to the PHP language reference manual` |
|        - | 2640 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|        - | 2641 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|        - | 2642 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|        - | 2643 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|        - | 2644 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|        - | 2645 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|        - | 2646 | ` *    (or re-thrown) within a catch block.` |
|        - | 2647 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|        - | 2648 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|        - | 2649 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|        - | 2650 | ` *    been defined with set_exception_handler().` |
|        - | 2651 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|        - | 2652 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|        - | 2653 | ` */` |
|        - | 2654 | `/*` |
|        - | 2655 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|        - | 2656 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|        - | 2657 | ` * indicates failure.` |
|        - | 2658 | ` */` |
|   548362 | 2659 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 2660 | `{` |
|        - | 2661 | ``	/* php decides throwability at RUNTIME: `throw 5` compiles and raises the`` |
|        - | 2662 | `	 * catchable Error "Can only throw objects" when it executes, and a` |
|        - | 2663 | `	 * non-Throwable object raises "Cannot throw objects that do not implement` |
|        - | 2664 | `	 * Throwable". This used to whitelist a handful of expression shapes and` |
|        - | 2665 | `	 * reject the rest at compile time as a "friendlier" error, which meant the` |
|        - | 2666 | `	 * program never ran and no catch could ever see it — and it was inconsistent` |
|        - | 2667 | ``	 * anyway, since `throw $v` (a variable holding an int) always compiled and`` |
|        - | 2668 | `	 * fell through to the same runtime path. OP_THROW now owns the whole rule,` |
|        - | 2669 | ``	 * so accept any expression here; `throw;` with no operand is still rejected`` |
|        - | 2670 | `	 * by PH7_CompileThrow's SXERR_EMPTY branch. */` |
|   274181 | 2671 | `	SXUNUSED(pGen);` |
|   274181 | 2672 | `	SXUNUSED(pRoot);` |
|   548367 | 2673 | `	return SXRET_OK;` |
|        5 | 2674 | `}` |
|        - | 2675 | `/*` |
|        - | 2676 | ` * Compile a 'throw' statement.` |
|        - | 2677 | ` * throw: This is how you trigger an exception.` |
|        - | 2678 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|        - | 2679 | ` */` |
|   548326 | 2680 | `PH7_PRIVATE sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|        5 | 2681 | `{` |
|   548331 | 2682 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2683 | `	GenBlock *pBlock;` |
|        - | 2684 | `	sxu32 nIdx;` |
|        - | 2685 | `	sxi32 rc;` |
|   548331 | 2686 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|        - | 2687 | `	/* Compile the expression */` |
|   548331 | 2688 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|   548331 | 2689 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 | 2690 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|      ! 0 | 2691 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2692 | `			return SXERR_ABORT;` |
|        - | 2693 | `		}` |
|      ! 0 | 2694 | `		return SXRET_OK;` |
|        - | 2695 | `	}` |
|   548331 | 2696 | `	pBlock = pGen->pCurrent;` |
|        - | 2697 | `	/* Point to the top most function or try block and emit the forward jump */` |
|  2184755 | 2698 | `	while(pBlock->pParent){` |
|  2184749 | 2699 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|   548325 | 2700 | `			break;` |
|        - | 2701 | `		}` |
|        - | 2702 | `		/* Point to the parent block */` |
|  1636429 | 2703 | `		pBlock = pBlock->pParent;` |
|        5 | 2704 | `	}` |
|        - | 2705 | `	/* Emit the throw instruction */` |
|   548331 | 2706 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|        - | 2707 | `	/* Emit the jump */` |
|   548331 | 2708 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|   548331 | 2709 | `	return SXRET_OK;` |
|   274168 | 2710 | `}` |
|        - | 2711 | `/*` |
|        - | 2712 | ` * Compile a PHP 8.0 'throw' expression.` |
|        - | 2713 | ` * Called from the expression code generator when a 'throw' keyword is` |
|        - | 2714 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|        - | 2715 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|        - | 2716 | ` * the validator guarantees the operand is a valid exception target.` |
|        - | 2717 | ` */` |
|       36 | 2718 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|        2 | 2719 | `{` |
|       38 | 2720 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2721 | `	GenBlock *pBlock;` |
|        - | 2722 | `	sxu32 nIdx;` |
|        - | 2723 | `	sxi32 rc;` |
|       18 | 2724 | `	(void)iCompileFlag;` |
|       38 | 2725 | `	pGen->pIn++; /* Skip 'throw' */` |
|       38 | 2726 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 2727 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 2728 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 2729 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2730 | `			return SXERR_ABORT;` |
|        - | 2731 | `		}` |
|      ! 0 | 2732 | `		return SXRET_OK;` |
|        - | 2733 | `	}` |
|       38 | 2734 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|       38 | 2735 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 2736 | `		return SXERR_ABORT;` |
|        - | 2737 | `	}` |
|       38 | 2738 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 | 2739 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 2740 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 2741 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2742 | `			return SXERR_ABORT;` |
|        - | 2743 | `		}` |
|      ! 0 | 2744 | `		return SXRET_OK;` |
|        - | 2745 | `	}` |
|        - | 2746 | `	/* Walk up to nearest exception/function block for the jump target */` |
|       38 | 2747 | `	pBlock = pGen->pCurrent;` |
|       60 | 2748 | `	while( pBlock->pParent ){` |
|       49 | 2749 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|       27 | 2750 | `			break;` |
|        - | 2751 | `		}` |
|       23 | 2752 | `		pBlock = pBlock->pParent;` |
|        1 | 2753 | `	}` |
|       38 | 2754 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       38 | 2755 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|       38 | 2756 | `	return SXRET_OK;` |
|       20 | 2757 | `}` |
|        - | 2758 | `/*` |
|        - | 2759 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|        - | 2760 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|        - | 2761 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|        - | 2762 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|        - | 2763 | ` * compile error propagated from the parser.` |
|        - | 2764 | ` */` |
|       56 | 2765 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|        5 | 2766 | `{` |
|        - | 2767 | `	SyString sClassName;` |
|        - | 2768 | `	SyToken *pToken;` |
|        - | 2769 | `	SyString *pName;` |
|        - | 2770 | `	char *zDup;` |
|        - | 2771 | `	sxi32 rc;` |
|       61 | 2772 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       61 | 2773 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|       61 | 2774 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|       61 | 2775 | `	SySetInit(&pCatch->sByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       61 | 2776 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 | 2777 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 2778 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 2779 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 2780 | `		return SXERR_INVALID;` |
|        - | 2781 | `	}` |
|       61 | 2782 | `	pGen->pIn++; /* '(' */` |
|       28 | 2783 | `	for(;;){` |
|        - | 2784 | `		SyBlob sResolved;` |
|       61 | 2785 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       61 | 2786 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 2787 | `			SyBlobRelease(&sResolved);` |
|      ! 0 | 2788 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 2789 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 2790 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 2791 | `			return SXERR_INVALID;` |
|        - | 2792 | `		}` |
|       89 | 2793 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       56 | 2794 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       61 | 2795 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|       61 | 2796 | `		SyBlobRelease(&sResolved);` |
|       61 | 2797 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|       61 | 2798 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|       61 | 2799 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       56 | 2800 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|        5 | 2801 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|      ! 0 | 2802 | `			pGen->pIn++; continue;` |
|        - | 2803 | `		}` |
|       61 | 2804 | `		break;` |
|      ! 0 | 2805 | `	}` |
|        - | 2806 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|        - | 2807 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding. */` |
|       61 | 2808 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|        3 | 2809 | `		pGen->pIn++; /* ')' */` |
|        3 | 2810 | `		return SXRET_OK;` |
|        - | 2811 | `	}` |
|       54 | 2812 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|       59 | 2813 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 2814 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 2815 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 2816 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 2817 | `		return SXERR_INVALID;` |
|        - | 2818 | `	}` |
|       59 | 2819 | `	pGen->pIn++; /* '$' */` |
|       59 | 2820 | `	pName = &pGen->pIn->sData;` |
|       59 | 2821 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       59 | 2822 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|       59 | 2823 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|       59 | 2824 | `	pGen->pIn++;` |
|       59 | 2825 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 | 2826 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 2827 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 2828 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 2829 | `		return SXERR_INVALID;` |
|        - | 2830 | `	}` |
|       59 | 2831 | `	pGen->pIn++; /* ')' */` |
|       59 | 2832 | `	return SXRET_OK;` |
|       33 | 2833 | `}` |
|        - | 2834 | `/*` |
|        - | 2835 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|        - | 2836 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|        - | 2837 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|        - | 2838 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|        - | 2839 | ` * VmThrowException):` |
|        - | 2840 | ` *` |
|        - | 2841 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|        - | 2842 | ` *    <try body>` |
|        - | 2843 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|        - | 2844 | ` *    JMP  -> finally\|end` |
|        - | 2845 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|        - | 2846 | ` *    <catch body>` |
|        - | 2847 | ` *    JMP  -> finally\|end` |
|        - | 2848 | ` *    ... more catches ...` |
|        - | 2849 | ` *  Lfin: <finally body>` |
|        - | 2850 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|        - | 2851 | ` *  Lend:` |
|        - | 2852 | ` */` |
|      100 | 2853 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|        5 | 2854 | `{` |
|      105 | 2855 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2856 | `	GenBlock *pTry;` |
|        - | 2857 | `	VmInstr *pInstr;` |
|      105 | 2858 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|        - | 2859 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|        - | 2860 | `	sxi32 rc;` |
|      105 | 2861 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|        - | 2862 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION) */` |
|      105 | 2863 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|      105 | 2864 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      105 | 2865 | `	pTry->pUserData = pException;` |
|      105 | 2866 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|      105 | 2867 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|      105 | 2868 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      105 | 2869 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      105 | 2870 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|      105 | 2871 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|        - | 2872 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|      105 | 2873 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|      105 | 2874 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|      105 | 2875 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|      105 | 2876 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 2877 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|      105 | 2878 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|        - | 2879 | `	/* Catch clauses (inline) */` |
|      105 | 2880 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      100 | 2881 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       61 | 2882 | `		sxu32 k = 0;` |
|       84 | 2883 | `		for(;;){` |
|        - | 2884 | `			ph7_exception_block sCatch;` |
|        - | 2885 | `			GenBlock *pCatchBlk;` |
|      117 | 2886 | `			sxu32 idxJmp = 0;` |
|      112 | 2887 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      107 | 2888 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|       33 | 2889 | `				break;` |
|        - | 2890 | `			}` |
|       61 | 2891 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|       61 | 2892 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       61 | 2893 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|       61 | 2894 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|       61 | 2895 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|       61 | 2896 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatchBlk);` |
|       61 | 2897 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        - | 2898 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|        - | 2899 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|        - | 2900 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump). */` |
|       61 | 2901 | `			pCatchBlk->pUserData = pException;` |
|       61 | 2902 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|       61 | 2903 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       61 | 2904 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|       61 | 2905 | `			GenStateLeaveBlock(&(*pGen),0);` |
|        - | 2906 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|        - | 2907 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|       61 | 2908 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       61 | 2909 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|       61 | 2910 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|       61 | 2911 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|       61 | 2912 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       61 | 2913 | `			k++;` |
|        5 | 2914 | `		}` |
|       28 | 2915 | `	}` |
|        - | 2916 | `	/* Finally (inline) */` |
|      105 | 2917 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       80 | 2918 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|        - | 2919 | `		GenBlock *pFinBlk;` |
|       52 | 2920 | `		pGen->pIn++; /* Jump 'finally' */` |
|       52 | 2921 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|       52 | 2922 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|       52 | 2923 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       52 | 2924 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       52 | 2925 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       52 | 2926 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|       52 | 2927 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       52 | 2928 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|       52 | 2929 | `		pException->iHasFinally = 1;` |
|       24 | 2930 | `	}` |
|      105 | 2931 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|      105 | 2932 | `	pException->iInlined = 1;` |
|        - | 2933 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|        - | 2934 | `	{` |
|      105 | 2935 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|        - | 2936 | `		sxu32 *aJ; sxu32 n;` |
|      105 | 2937 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|      105 | 2938 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|      105 | 2939 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|      161 | 2940 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|       61 | 2941 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|       61 | 2942 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|       33 | 2943 | `		}` |
|        - | 2944 | `	}` |
|      105 | 2945 | `	SySetRelease(&aCatchJmp);` |
|      105 | 2946 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|      ! 0 | 2947 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|      ! 0 | 2948 | `	}` |
|      105 | 2949 | `	return SXRET_OK;` |
|       55 | 2950 | `}` |
|        - | 2951 | `/*` |
|        - | 2952 | ` * Compile a 'catch' block.` |
|        - | 2953 | ` * Catch: A "catch" block retrieves an exception and creates` |
|        - | 2954 | ` * an object containing the exception information.` |
|        - | 2955 | ` */` |
|    24954 | 2956 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|        5 | 2957 | `{` |
|    24959 | 2958 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2959 | `	ph7_exception_block sCatch;` |
|        - | 2960 | `	SySet *pInstrContainer;` |
|        - | 2961 | `	SyString sClassName;` |
|        - | 2962 | `	GenBlock *pCatch;` |
|        - | 2963 | `	SyToken *pToken;` |
|        - | 2964 | `	SyString *pName;` |
|        - | 2965 | `	char *zDup;` |
|        - | 2966 | `	sxi32 rc;` |
|    24959 | 2967 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|        - | 2968 | `	/* Zero the structure */` |
|    24959 | 2969 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|        - | 2970 | `	/* Initialize fields */` |
|    24959 | 2971 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|    24959 | 2972 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|    24959 | 2973 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|        - | 2974 | `			/* Unexpected token,break immediately */` |
|      ! 0 | 2975 | `			pToken = pGen->pIn;` |
|      ! 0 | 2976 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 2977 | `				pToken--;` |
|      ! 0 | 2978 | `			}` |
|      ! 0 | 2979 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 2980 | `				"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 2981 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 2982 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2983 | `				return SXERR_ABORT;` |
|        - | 2984 | `			}` |
|      ! 0 | 2985 | `			return SXERR_INVALID;` |
|        - | 2986 | `	}` |
|        - | 2987 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|    24959 | 2988 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|    12492 | 2989 | `	for(;;){` |
|        - | 2990 | `		SyBlob sResolved;` |
|    24989 | 2991 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    24989 | 2992 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|        6 | 2993 | `			SyBlobRelease(&sResolved);` |
|        6 | 2994 | `			pToken = pGen->pIn;` |
|        6 | 2995 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 2996 | `				pToken--;` |
|      ! 0 | 2997 | `			}` |
|        8 | 2998 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 2999 | `				"syntax error, unexpected %s \"%z\"",` |
|        2 | 3000 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|        6 | 3001 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3002 | `				return SXERR_ABORT;` |
|        - | 3003 | `			}` |
|        6 | 3004 | `			return SXERR_INVALID;` |
|        - | 3005 | `		}` |
|        - | 3006 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|        - | 3007 | `		 * transient SyBlob allocation. */` |
|    37475 | 3008 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    24980 | 3009 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|    24985 | 3010 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|    24985 | 3011 | `		SyBlobRelease(&sResolved);` |
|    24985 | 3012 | `		if( zDup == 0 ){` |
|      ! 0 | 3013 | `			goto Mem;` |
|        - | 3014 | `		}` |
|    24985 | 3015 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|    24985 | 3016 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 3017 | `			goto Mem;` |
|        - | 3018 | `		}` |
|        - | 3019 | `		/* Check for '\|' (multi-catch separator) */` |
|    24980 | 3020 | `		if( pGen->pIn < pGen->pEnd &&` |
|    24980 | 3021 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|       35 | 3022 | `			pGen->pIn->sData.nByte == 1 &&` |
|       30 | 3023 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|       32 | 3024 | `			pGen->pIn++; /* Consume the '\|' */` |
|       32 | 3025 | `			continue;` |
|        - | 3026 | `		}` |
|    24955 | 3027 | `		break;` |
|      ! 0 | 3028 | `	}` |
|        - | 3029 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|        - | 3030 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding;` |
|        - | 3031 | `	 * jump straight to compiling the block below. */` |
|    24955 | 3032 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) /*)*/ ){` |
|        7 | 3033 | `		goto CatchBody;` |
|        - | 3034 | `	}` |
|    24944 | 3035 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|    24949 | 3036 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - | 3037 | `			/* Unexpected token,break immediately */` |
|      ! 0 | 3038 | `			pToken = pGen->pIn;` |
|      ! 0 | 3039 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 3040 | `				pToken--;` |
|      ! 0 | 3041 | `			}` |
|      ! 0 | 3042 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 3043 | `				"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 3044 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 3045 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3046 | `				return SXERR_ABORT;` |
|        - | 3047 | `			}` |
|      ! 0 | 3048 | `			return SXERR_INVALID;` |
|        - | 3049 | `	}` |
|    24949 | 3050 | `	pGen->pIn++; /* Jump the dollar sign */` |
|        - | 3051 | `	/* Duplicate instance name */` |
|    24949 | 3052 | `	pName = &pGen->pIn->sData;` |
|    24949 | 3053 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    24949 | 3054 | `	if( zDup == 0 ){` |
|      ! 0 | 3055 | `		goto Mem;` |
|        - | 3056 | `	}` |
|    24949 | 3057 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|    24949 | 3058 | `	pGen->pIn++;` |
|    12475 | 3059 | `CatchBody:` |
|    24955 | 3060 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|        - | 3061 | `		/* Unexpected token,break immediately */` |
|      ! 0 | 3062 | `		pToken = pGen->pIn;` |
|      ! 0 | 3063 | `		if( pToken >= pGen->pEnd ){` |
|      ! 0 | 3064 | `			pToken--;` |
|      ! 0 | 3065 | `		}` |
|      ! 0 | 3066 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 3067 | `			"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 3068 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 3069 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3070 | `			return SXERR_ABORT;` |
|        - | 3071 | `		}` |
|      ! 0 | 3072 | `		return SXERR_INVALID;` |
|        - | 3073 | `	}` |
|        - | 3074 | `	/* Compile the block */` |
|    24955 | 3075 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|        - | 3076 | `	/* Create the catch block */` |
|    24955 | 3077 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|    24955 | 3078 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3079 | `		return SXERR_ABORT;` |
|        - | 3080 | `	}` |
|        - | 3081 | `	/* Swap bytecode container */` |
|    24955 | 3082 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    24955 | 3083 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|        - | 3084 | `	/* Compile the block */` |
|    24955 | 3085 | `	PH7_CompileBlock(&(*pGen),0);` |
|        - | 3086 | `	/* Fix forward jumps now the destination is resolved  */` |
|    24955 | 3087 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 3088 | `	/* Emit the DONE instruction */` |
|    24955 | 3089 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - | 3090 | `	/* Leave the block */` |
|    24955 | 3091 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 3092 | `	/* Restore the default container */` |
|    24955 | 3093 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - | 3094 | `	/* Install the catch block */` |
|    24955 | 3095 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|    24955 | 3096 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3097 | `		goto Mem;` |
|        - | 3098 | `	}` |
|    24955 | 3099 | `	return SXRET_OK;` |
|      ! 0 | 3100 | `Mem:` |
|      ! 0 | 3101 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 3102 | `	return SXERR_ABORT;` |
|    12482 | 3103 | `}` |
|        - | 3104 | `/*` |
|        - | 3105 | ` * Compile a 'try' block.` |
|        - | 3106 | ` * A function using an exception should be in a "try" block.` |
|        - | 3107 | ` * If the exception does not trigger, the code will continue` |
|        - | 3108 | ` * as normal. However if the exception triggers, an exception` |
|        - | 3109 | ` * is "thrown".` |
|        - | 3110 | ` */` |
|    25112 | 3111 | `PH7_PRIVATE sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|        5 | 3112 | `{` |
|        - | 3113 | `	ph7_exception *pException;` |
|    25117 | 3114 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 3115 | `	GenBlock *pTry;` |
|        - | 3116 | `	sxu32 nJmpIdx;` |
|        - | 3117 | `	sxi32 rc;` |
|        - | 3118 | `	/* Create the exception container */` |
|    25117 | 3119 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|    25117 | 3120 | `	if( pException == 0 ){` |
|      ! 0 | 3121 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|      ! 0 | 3122 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 3123 | `		return SXERR_ABORT;` |
|        - | 3124 | `	}` |
|        - | 3125 | `	/* Zero the structure */` |
|    25117 | 3126 | `	SyZero(pException,sizeof(ph7_exception));` |
|        - | 3127 | `	/* Initialize fields */` |
|    25117 | 3128 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|    25117 | 3129 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|    25117 | 3130 | `	pException->iHasFinally = 0;` |
|    25117 | 3131 | `	pException->iFinallyDone = 0;` |
|    25117 | 3132 | `	pException->pVm = pGen->pVm;` |
|        - | 3133 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|        - | 3134 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the legacy path.`` |
|        - | 3135 | `	 * DORMANT until the inline VM handlers (OP_CATCH / OP_END_FINALLY dispatch,` |
|        - | 3136 | `	 * VmThrowException pc-redirect, return/break-through-finally threading, generator` |
|        - | 3137 | `	 * park of aFinallyAction) land — the compiler emits the layout but the VM cannot yet` |
|        - | 3138 | `	 * execute it. Guarded by pVm->bInlineTryCatch (default 0) so the tree stays green. */` |
|    25117 | 3139 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|      105 | 3140 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|        - | 3141 | `	}` |
|        - | 3142 | `	/* Create the try block */` |
|    25017 | 3143 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|    25017 | 3144 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3145 | `		return SXERR_ABORT;` |
|        - | 3146 | `	}` |
|        - | 3147 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|    25017 | 3148 | `	pTry->pUserData = pException;` |
|        - | 3149 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|    25017 | 3150 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|        - | 3151 | `	/* Fix the jump later when the destination is resolved */` |
|    25017 | 3152 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|    25017 | 3153 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|        - | 3154 | `	/* Compile the block */` |
|    25017 | 3155 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|    25017 | 3156 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 3157 | `		return SXERR_ABORT;` |
|        - | 3158 | `	}` |
|        - | 3159 | `	/* Fix forward jumps now the destination is resolved */` |
|    25017 | 3160 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 3161 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|    25017 | 3162 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|        - | 3163 | `	/* Leave the block */` |
|    25017 | 3164 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 3165 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|    25017 | 3166 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|    25010 | 3167 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|        - | 3168 | `		/* Compile one or more catch blocks */` |
|    24950 | 3169 | `		for(;;){` |
|    49900 | 3170 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    37507 | 3171 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|    12478 | 3172 | `					break;` |
|        - | 3173 | `			}` |
|    24959 | 3174 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|    24959 | 3175 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3176 | `				return SXERR_ABORT;` |
|        - | 3177 | `			}` |
|        5 | 3178 | `		}` |
|    12473 | 3179 | `	}` |
|        - | 3180 | `	/* Compile optional finally block */` |
|    25017 | 3181 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      804 | 3182 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|        - | 3183 | `		SySet *pInstrContainer;` |
|        - | 3184 | `		GenBlock *pFinBlock;` |
|      129 | 3185 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|        - | 3186 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      129 | 3187 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      129 | 3188 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 3189 | `			return SXERR_ABORT;` |
|        - | 3190 | `		}` |
|        - | 3191 | `		/* Swap bytecode container */` |
|      129 | 3192 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      129 | 3193 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|        - | 3194 | `		/* Compile the finally body */` |
|      129 | 3195 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      129 | 3196 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3197 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 | 3198 | `			return SXERR_ABORT;` |
|        - | 3199 | `		}` |
|        - | 3200 | `		/* Fix forward jumps now the destination is resolved */` |
|      129 | 3201 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 3202 | `		/* Emit DONE to terminate the finally block */` |
|      129 | 3203 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - | 3204 | `		/* Leave the block */` |
|      129 | 3205 | `		GenStateLeaveBlock(&(*pGen),0);` |
|        - | 3206 | `		/* Restore the default container */` |
|      129 | 3207 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      129 | 3208 | `		pException->iHasFinally = 1;` |
|       62 | 3209 | `	}` |
|        - | 3210 | `	/* Must have at least one catch or finally */` |
|    25017 | 3211 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|        9 | 3212 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 3213 | `			"Cannot use try without catch or finally");` |
|        9 | 3214 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3215 | `			return SXERR_ABORT;` |
|        - | 3216 | `		}` |
|        3 | 3217 | `	}` |
|    25017 | 3218 | `	return SXRET_OK;` |
|    12561 | 3219 | `}` |
|        - | 3220 | `/*` |
|        - | 3221 | ` * Compile a switch block.` |
|        - | 3222 | ` *  (See block-comment below for more information)` |
|        - | 3223 | ` */` |
|   136122 | 3224 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|        5 | 3225 | `{` |
|   136127 | 3226 | `	sxi32 rc = SXRET_OK;` |
|   136127 | 3227 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|        - | 3228 | `		/* Unexpected token */` |
|      ! 0 | 3229 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 | 3230 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3231 | `			return SXERR_ABORT;` |
|        - | 3232 | `		}` |
|      ! 0 | 3233 | `		pGen->pIn++;` |
|      ! 0 | 3234 | `	}` |
|   136127 | 3235 | `	pGen->pIn++;` |
|        - | 3236 | `	/* First instruction to execute in this block. */` |
|   136127 | 3237 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|        - | 3238 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|        - | 3239 | `	 * or the '}' token */` |
|   130387 | 3240 | `	for(;;){` |
|   260779 | 3241 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 3242 | `			/* No more input to process */` |
|      ! 0 | 3243 | `			break;` |
|        - | 3244 | `		}` |
|   260779 | 3245 | `		rc = SXRET_OK;` |
|   260779 | 3246 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|    35059 | 3247 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|    11689 | 3248 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|        - | 3249 | `					/* Unexpected token */` |
|      ! 0 | 3250 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|      ! 0 | 3251 | `						&pGen->pIn->sData);` |
|      ! 0 | 3252 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3253 | `						return SXERR_ABORT;` |
|        - | 3254 | `					}` |
|        - | 3255 | `					/* FALL THROUGH */` |
|      ! 0 | 3256 | `				}` |
|    11689 | 3257 | `				rc = SXERR_EOF;` |
|    11689 | 3258 | `				break;` |
|        - | 3259 | `			}` |
|    11690 | 3260 | `		}else{` |
|        - | 3261 | `			sxi32 nKwrd;` |
|        - | 3262 | `			/* Extract the keyword */` |
|   225725 | 3263 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   225725 | 3264 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|    62223 | 3265 | `				break;` |
|        - | 3266 | `			}` |
|   101289 | 3267 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|        3 | 3268 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|        - | 3269 | `					/* Unexpected token */` |
|      ! 0 | 3270 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|      ! 0 | 3271 | `						&pGen->pIn->sData);` |
|      ! 0 | 3272 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3273 | `						return SXERR_ABORT;` |
|        - | 3274 | `					}` |
|        - | 3275 | `					/* FALL THROUGH */` |
|      ! 0 | 3276 | `				}` |
|        - | 3277 | `				/* Block compiled */` |
|        3 | 3278 | `				break;` |
|        - | 3279 | `			}` |
|        - | 3280 | `		}` |
|        - | 3281 | `		/* Compile block */` |
|   124657 | 3282 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|   124657 | 3283 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3284 | `			return SXERR_ABORT;` |
|        - | 3285 | `		}` |
|        5 | 3286 | `	}` |
|   136127 | 3287 | `	return rc;` |
|    68066 | 3288 | `}` |
|        - | 3289 | `/*` |
|        - | 3290 | ` * Compile a case eXpression.` |
|        - | 3291 | ` *  (See block-comment below for more information)` |
|        - | 3292 | ` */` |
|   132216 | 3293 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|        5 | 3294 | `{` |
|        - | 3295 | `	SySet *pInstrContainer;` |
|        - | 3296 | `	SyToken *pEnd,*pTmp;` |
|   132221 | 3297 | `	sxi32 iNest = 0;` |
|        - | 3298 | `	sxi32 rc;` |
|        - | 3299 | `	/* Delimit the expression */` |
|   132221 | 3300 | `	pEnd = pGen->pIn;` |
|   264445 | 3301 | `	while( pEnd < pGen->pEnd ){` |
|   264445 | 3302 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|        - | 3303 | `			/* Increment nesting level */` |
|        3 | 3304 | `			iNest++;` |
|   264444 | 3305 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|        - | 3306 | `			/* Decrement nesting level */` |
|        3 | 3307 | `			iNest--;` |
|   264442 | 3308 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|   132221 | 3309 | `			break;` |
|        - | 3310 | `		}` |
|   132229 | 3311 | `		pEnd++;` |
|        5 | 3312 | `	}` |
|   132221 | 3313 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 3314 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|      ! 0 | 3315 | `		if( rc == SXERR_ABORT ){` |
|        - | 3316 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 3317 | `			return SXERR_ABORT;` |
|        - | 3318 | `		}` |
|      ! 0 | 3319 | `	}` |
|        - | 3320 | `	/* Swap token stream */` |
|   132221 | 3321 | `	pTmp = pGen->pEnd;` |
|   132221 | 3322 | `	pGen->pEnd = pEnd;` |
|   132221 | 3323 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   132221 | 3324 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|   132221 | 3325 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 3326 | `	/* Emit the done instruction */` |
|   132221 | 3327 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   132221 | 3328 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - | 3329 | `	/* Update token stream */` |
|   132221 | 3330 | `	pGen->pIn  = pEnd;` |
|   132221 | 3331 | `	pGen->pEnd = pTmp;` |
|   132221 | 3332 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 3333 | `		return SXERR_ABORT;` |
|        - | 3334 | `	}` |
|   132221 | 3335 | `	return SXRET_OK;` |
|    66113 | 3336 | `}` |
|        - | 3337 | `/*` |
|        - | 3338 | ` * Compile the smart switch statement.` |
|        - | 3339 | ` * According to the PHP language reference manual` |
|        - | 3340 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|        - | 3341 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|        - | 3342 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|        - | 3343 | ` *  This is exactly what the switch statement is for.` |
|        - | 3344 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|        - | 3345 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|        - | 3346 | ` *  of the outer loop, use continue 2.` |
|        - | 3347 | ` *  Note that switch/case does loose comparision.` |
|        - | 3348 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|        - | 3349 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|        - | 3350 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|        - | 3351 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|        - | 3352 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|        - | 3353 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|        - | 3354 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|        - | 3355 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|        - | 3356 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|        - | 3357 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|        - | 3358 | ` *  list for the next case.` |
|        - | 3359 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|        - | 3360 | ` *  or floating-point numbers and strings.` |
|        - | 3361 | ` */` |
|    11686 | 3362 | `PH7_PRIVATE sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|        5 | 3363 | `{` |
|        - | 3364 | `	GenBlock *pSwitchBlock;` |
|        - | 3365 | `	SyToken *pTmp,*pEnd;` |
|        - | 3366 | `	ph7_switch *pSwitch;` |
|        - | 3367 | `	sxu32 nToken;` |
|        - | 3368 | `	sxu32 nLine;` |
|        - | 3369 | `	sxi32 rc;` |
|    11691 | 3370 | `	nLine = pGen->pIn->nLine;` |
|        - | 3371 | `	/* Jump the 'switch' keyword */` |
|    11691 | 3372 | `	pGen->pIn++;` |
|    11691 | 3373 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 3374 | `		/* Syntax error */` |
|      ! 0 | 3375 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|      ! 0 | 3376 | `		if( rc == SXERR_ABORT ){` |
|        - | 3377 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 3378 | `			return SXERR_ABORT;` |
|        - | 3379 | `		}` |
|      ! 0 | 3380 | `		goto Synchronize;` |
|        - | 3381 | `	}` |
|        - | 3382 | `	/* Jump the left parenthesis '(' */` |
|    11691 | 3383 | `	pGen->pIn++;` |
|    11691 | 3384 | `	pEnd = 0; /* cc warning */` |
|        - | 3385 | `	/* Create the loop block */` |
|    17534 | 3386 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|     5843 | 3387 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|    11691 | 3388 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3389 | `		return SXERR_ABORT;` |
|        - | 3390 | `	}` |
|        - | 3391 | `	/* Delimit the condition */` |
|    11691 | 3392 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    11691 | 3393 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - | 3394 | `		/* Empty expression */` |
|      ! 0 | 3395 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|      ! 0 | 3396 | `		if( rc == SXERR_ABORT ){` |
|        - | 3397 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 3398 | `			return SXERR_ABORT;` |
|        - | 3399 | `		}` |
|      ! 0 | 3400 | `	}` |
|        - | 3401 | `	/* Swap token streams */` |
|    11691 | 3402 | `	pTmp = pGen->pEnd;` |
|    11691 | 3403 | `	pGen->pEnd = pEnd;` |
|        - | 3404 | `	/* Compile the expression */` |
|    11691 | 3405 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    11691 | 3406 | `	if( rc == SXERR_ABORT ){` |
|        - | 3407 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 3408 | `		return SXERR_ABORT;` |
|        - | 3409 | `	}` |
|        - | 3410 | `	/* Update token stream */` |
|    11691 | 3411 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 | 3412 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 3413 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 3414 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3415 | `			return SXERR_ABORT;` |
|        - | 3416 | `		}` |
|      ! 0 | 3417 | `		pGen->pIn++;` |
|      ! 0 | 3418 | `	}` |
|    11691 | 3419 | `	pGen->pIn  = &pEnd[1];` |
|    11691 | 3420 | `	pGen->pEnd = pTmp;` |
|    11691 | 3421 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|    11686 | 3422 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|      ! 0 | 3423 | `			pTmp = pGen->pIn;` |
|      ! 0 | 3424 | `			if( pTmp >= pGen->pEnd ){` |
|      ! 0 | 3425 | `				pTmp--;` |
|      ! 0 | 3426 | `			}` |
|        - | 3427 | `			/* Unexpected token */` |
|      ! 0 | 3428 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|      ! 0 | 3429 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3430 | `				return SXERR_ABORT;` |
|        - | 3431 | `			}` |
|      ! 0 | 3432 | `			goto Synchronize;` |
|        - | 3433 | `	}` |
|        - | 3434 | `	/* Set the delimiter token */` |
|    11691 | 3435 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|        3 | 3436 | `		nToken = PH7_TK_KEYWORD;` |
|        - | 3437 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|        2 | 3438 | `	}else{` |
|    11689 | 3439 | `		nToken = PH7_TK_CCB; /* '}' */` |
|        - | 3440 | `	}` |
|    11691 | 3441 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|        - | 3442 | `	/* Create the switch blocks container */` |
|    11691 | 3443 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|    11691 | 3444 | `	if( pSwitch == 0 ){` |
|        - | 3445 | `		/* Abort compilation */` |
|      ! 0 | 3446 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 3447 | `		return SXERR_ABORT;` |
|        - | 3448 | `	}` |
|        - | 3449 | `	/* Zero the structure */` |
|    11691 | 3450 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|        - | 3451 | `	/* Initialize fields */` |
|    11691 | 3452 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|        - | 3453 | `	/* Emit the switch instruction */` |
|    11691 | 3454 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|        - | 3455 | `	/* Compile case blocks */` |
|   130281 | 3456 | `	for(;;){` |
|        - | 3457 | `		sxu32 nKwrd;` |
|   136129 | 3458 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 3459 | `			/* No more input to process */` |
|      ! 0 | 3460 | `			break;` |
|        - | 3461 | `		}` |
|   136129 | 3462 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 3463 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|        - | 3464 | `				/* Unexpected token */` |
|      ! 0 | 3465 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 3466 | `					&pGen->pIn->sData);` |
|      ! 0 | 3467 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3468 | `					return SXERR_ABORT;` |
|        - | 3469 | `				}` |
|        - | 3470 | `				/* FALL THROUGH */` |
|      ! 0 | 3471 | `			}` |
|        - | 3472 | `			/* Block compiled */` |
|      ! 0 | 3473 | `			break;` |
|        - | 3474 | `		}` |
|        - | 3475 | `		/* Extract the keyword */` |
|   136129 | 3476 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   136129 | 3477 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|        3 | 3478 | `			if( nToken != PH7_TK_KEYWORD ){` |
|        - | 3479 | `				/* Unexpected token */` |
|      ! 0 | 3480 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 3481 | `					&pGen->pIn->sData);` |
|      ! 0 | 3482 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3483 | `					return SXERR_ABORT;` |
|        - | 3484 | `				}` |
|        - | 3485 | `				/* FALL THROUGH */` |
|      ! 0 | 3486 | `			}` |
|        - | 3487 | `			/* Block compiled */` |
|        3 | 3488 | `			break;` |
|        - | 3489 | `		}` |
|   136127 | 3490 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|        - | 3491 | `			/*` |
|        - | 3492 | `			 * Accroding to the PHP language reference manual` |
|        - | 3493 | `			 *  A special case is the default case. This case matches anything` |
|        - | 3494 | `			 *  that wasn't matched by the other cases.` |
|        - | 3495 | `			 */` |
|     3911 | 3496 | `			if( pSwitch->nDefault > 0 ){` |
|        - | 3497 | `				/* Default case already compiled */` |
|      ! 0 | 3498 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|      ! 0 | 3499 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3500 | `					return SXERR_ABORT;` |
|        - | 3501 | `				}` |
|      ! 0 | 3502 | `			}` |
|     3911 | 3503 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|        - | 3504 | `			/* Compile the default block */` |
|     3911 | 3505 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|     3911 | 3506 | `			if( rc == SXERR_ABORT){` |
|      ! 0 | 3507 | `				return SXERR_ABORT;` |
|     3911 | 3508 | `			}else if( rc == SXERR_EOF ){` |
|     3909 | 3509 | `				break;` |
|        1 | 3510 | `			}` |
|   132222 | 3511 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|        - | 3512 | `			ph7_case_expr sCase;` |
|        - | 3513 | `			/* Standard case block */` |
|   132221 | 3514 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|        - | 3515 | `			/* initialize the structure */` |
|   132221 | 3516 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - | 3517 | `			/* Compile the case expression */` |
|   132221 | 3518 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|   132221 | 3519 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3520 | `				return SXERR_ABORT;` |
|        - | 3521 | `			}` |
|        - | 3522 | `			/* Compile the case block */` |
|   132221 | 3523 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|        - | 3524 | `			/* Insert in the switch container */` |
|   132221 | 3525 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|   132221 | 3526 | `			if( rc == SXERR_ABORT){` |
|      ! 0 | 3527 | `				return SXERR_ABORT;` |
|   132221 | 3528 | `			}else if( rc == SXERR_EOF ){` |
|     7785 | 3529 | `				break;` |
|        - | 3530 | `			}` |
|    62223 | 3531 | `		}else{` |
|        - | 3532 | `			/* Unexpected token */` |
|      ! 0 | 3533 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 3534 | `				&pGen->pIn->sData);` |
|      ! 0 | 3535 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3536 | `				return SXERR_ABORT;` |
|        - | 3537 | `			}` |
|      ! 0 | 3538 | `			break;` |
|        - | 3539 | `		}` |
|        5 | 3540 | `	}` |
|        - | 3541 | `	/* Fix all jumps now the destination is resolved */` |
|    11691 | 3542 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|    11691 | 3543 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 3544 | `	/* Release the loop block */` |
|    11691 | 3545 | `	GenStateLeaveBlock(pGen,0);` |
|    11691 | 3546 | `	if( pGen->pIn < pGen->pEnd ){` |
|        - | 3547 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|    11691 | 3548 | `		pGen->pIn++;` |
|     5843 | 3549 | `	}` |
|        - | 3550 | `	/* Statement successfully compiled */` |
|    11691 | 3551 | `	return SXRET_OK;` |
|      ! 0 | 3552 | `Synchronize:` |
|        - | 3553 | `	/* Synchronize with the first semi-colon */` |
|      ! 0 | 3554 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      ! 0 | 3555 | `		pGen->pIn++;` |
|      ! 0 | 3556 | `	}` |
|      ! 0 | 3557 | `	return SXRET_OK;` |
|     5848 | 3558 | `}` |
|        - | 3559 |  |
