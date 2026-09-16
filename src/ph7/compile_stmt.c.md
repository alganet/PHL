# src/ph7/compile_stmt.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1442/1946 lines (74.10%)

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
|       56 |   40 | `PH7_PRIVATE sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|        5 |   41 | `{` |
|        - |   42 | `	SySet *pConsCode,*pInstrContainer;` |
|       61 |   43 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|        - |   44 | `	SyString *pName;` |
|        - |   45 | `	sxi32 rc;` |
|       61 |   46 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       61 |   47 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |   48 | `		/* Invalid constant name */` |
|        9 |   49 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"identifier");` |
|        9 |   50 | `		if( rc == SXERR_ABORT ){` |
|        - |   51 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |   52 | `			return SXERR_ABORT;` |
|        - |   53 | `		}` |
|        9 |   54 | `		goto Synchronize;` |
|        - |   55 | `	}` |
|        - |   56 | `	/* Peek constant name */` |
|       54 |   57 | `	pName = &pGen->pIn->sData;` |
|        - |   58 | `	/* Make sure the constant name isn't reserved */` |
|       54 |   59 | `	if( GenStateIsReservedConstant(pName) ){` |
|        - |   60 | `		/* Reserved constant */` |
|       10 |   61 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Cannot redeclare constant '%z'",pName);` |
|       10 |   62 | `		if( rc == SXERR_ABORT ){` |
|        - |   63 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |   64 | `			return SXERR_ABORT;` |
|        - |   65 | `		}` |
|       10 |   66 | `		goto Synchronize;` |
|        - |   67 | `	}` |
|       46 |   68 | `	pGen->pIn++;` |
|       46 |   69 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|        - |   70 | `		/* Invalid statement*/` |
|        6 |   71 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"=\"");` |
|        6 |   72 | `		if( rc == SXERR_ABORT ){` |
|        - |   73 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |   74 | `			return SXERR_ABORT;` |
|        - |   75 | `		}` |
|        6 |   76 | `		goto Synchronize;` |
|        - |   77 | `	}` |
|       40 |   78 | `	pGen->pIn++; /*Jump the equal sign */` |
|        - |   79 | `	/* Allocate a new constant value container */` |
|       40 |   80 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|       40 |   81 | `	if( pConsCode == 0 ){` |
|      ! 0 |   82 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |   83 | `		return SXERR_ABORT;` |
|        - |   84 | `	}` |
|       40 |   85 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - |   86 | `	/* Swap bytecode container */` |
|       40 |   87 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       40 |   88 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|        - |   89 | `	/* Compile constant value */` |
|       40 |   90 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |   91 | `	/* Emit the done instruction */` |
|       40 |   92 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       40 |   93 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       40 |   94 | `	if( rc == SXERR_ABORT ){` |
|        - |   95 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |   96 | `		return SXERR_ABORT;` |
|        - |   97 | `	}` |
|       40 |   98 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|        - |   99 | `	/* Register the constant with namespace-qualified name */` |
|        - |  100 | `	{` |
|        - |  101 | `		SyBlob sFQN;` |
|        - |  102 | `		SyString sFQNStr;` |
|       40 |  103 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       40 |  104 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|       40 |  105 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|       59 |  106 | `		rc = PH7_VmRegisterConstantEx(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode,` |
|       38 |  107 | `			(SyString *)SySetPeek(&pGen->pVm->aFiles),nLineLocal,1);` |
|       40 |  108 | `		if( rc == SXRET_OK && SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|        - |  109 | ``			/* php 8.5: attributes on `const` statements — attach the pending`` |
|        - |  110 | `			 * groups to the registered constant record for Reflection. */` |
|        7 |  111 | `			SyHashEntry *pCEntry = SyHashGet(&pGen->pVm->hConstant,` |
|        4 |  112 | `				SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|        5 |  113 | `			if( pCEntry ){` |
|        5 |  114 | `				ph7_constant *pRegCons = (ph7_constant *)pCEntry->pUserData;` |
|        5 |  115 | `				if( GenStateConsumeAttrs(&(*pGen),&pRegCons->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  116 | `					SyBlobRelease(&sFQN);` |
|      ! 0 |  117 | `					return SXERR_ABORT;` |
|        - |  118 | `				}` |
|        2 |  119 | `			}` |
|        2 |  120 | `		}` |
|       40 |  121 | `		SyBlobRelease(&sFQN);` |
|        - |  122 | `	}` |
|       40 |  123 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  124 | `		SySetRelease(pConsCode);` |
|      ! 0 |  125 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|      ! 0 |  126 | `	}` |
|       40 |  127 | `	return SXRET_OK;` |
|        9 |  128 | `Synchronize:` |
|        - |  129 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|       60 |  130 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       42 |  131 | `		pGen->pIn++;` |
|        4 |  132 | `	}` |
|       22 |  133 | `	return SXRET_OK;` |
|       33 |  134 | `}` |
|        - |  135 | `/*` |
|        - |  136 | ` * Compile the 'continue' statement.` |
|        - |  137 | ` * According to the PHP language reference` |
|        - |  138 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|        - |  139 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|        - |  140 | ` *  iteration.` |
|        - |  141 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|        - |  142 | ` *  the purposes of continue.` |
|        - |  143 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|        - |  144 | ` *  of enclosing loops it should skip to the end of.` |
|        - |  145 | ` *  Note:` |
|        - |  146 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|        - |  147 | ` */` |
|        - |  148 | `/*` |
|        - |  149 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|        - |  150 | ` * block and the target loop block. This ensures finally blocks run when` |
|        - |  151 | ` * break/continue crosses a try boundary.` |
|        - |  152 | ` *` |
|        - |  153 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|        - |  154 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|        - |  155 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|        - |  156 | ` */` |
|   179008 |  157 | `static int GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|        5 |  158 | `{` |
|   179013 |  159 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   179013 |  160 | `	int nInlineTry = 0;` |
|   723597 |  161 | `	while( pBlock && pBlock != pTarget ){` |
|   544589 |  162 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|        6 |  163 | `			if( pBlock->pUserData ){` |
|        - |  164 | `				/* A try block with an exception context. In a generator its catch/finally` |
|        - |  165 | `				 * are inlined: count it so the caller emits a single OP_SET_FINALLY_JMP that` |
|        - |  166 | `				 * runs each crossed finally (VmFinallyAdvance) before taking the loop jump.` |
|        - |  167 | `				 * Legacy path: emit POP_EXCEPTION per crossed try as before. */` |
|        6 |  168 | `				if( pGen->bInGenerator ){` |
|        3 |  169 | `					nInlineTry++;` |
|        2 |  170 | `				}else{` |
|        3 |  171 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|        - |  172 | `				}` |
|        4 |  173 | `			}else{` |
|        - |  174 | `				/* A catch/finally block compiled into a separate bytecode container` |
|        - |  175 | `				 * (legacy). Stop — cannot cross into the parent try from a sub-execution. */` |
|      ! 0 |  176 | `				break;` |
|        - |  177 | `			}` |
|        2 |  178 | `		}` |
|   544589 |  179 | `		pBlock = pBlock->pParent;` |
|        5 |  180 | `	}` |
|   179013 |  181 | `	return nInlineTry;` |
|        5 |  182 | `}` |
|    89476 |  183 | `PH7_PRIVATE sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|        5 |  184 | `{` |
|        - |  185 | `	GenBlock *pLoop; /* Target loop */` |
|        - |  186 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|        - |  187 | `	sxu32 nLineLocal;` |
|        - |  188 | `	sxi32 rc;` |
|    89481 |  189 | `	nLineLocal = pGen->pIn->nLine;` |
|    89481 |  190 | `	iLevel = 0;` |
|        - |  191 | `	/* Jump the 'continue' keyword */` |
|    89481 |  192 | `	pGen->pIn++;` |
|    89481 |  193 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|        - |  194 | `		/* optional numeric argument which tells us how many levels` |
|        - |  195 | `		 * of enclosing loops we should skip to the end of.` |
|        - |  196 | `		 */` |
|        - |  197 | `		char zScratch[GEN_NUM_SCRATCH];` |
|       17 |  198 | `		char *zAlloc = 0;` |
|        - |  199 | `		SyString sNum;` |
|       17 |  200 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|       17 |  201 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  202 | `			return SXERR_ABORT;` |
|        - |  203 | `		}` |
|       17 |  204 | `		if( rc == SXRET_OK ){` |
|       20 |  205 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|       12 |  206 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|       14 |  207 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  208 | `				return SXERR_ABORT;` |
|        - |  209 | `			}` |
|       14 |  210 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|       14 |  211 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        6 |  212 | `		}` |
|       17 |  213 | `		if( iLevel < 2 ){` |
|        3 |  214 | `			iLevel = 0;` |
|        1 |  215 | `		}` |
|       17 |  216 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|        7 |  217 | `	}` |
|        - |  218 | `	/* Point to the target loop */` |
|    89481 |  219 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    89481 |  220 | `	if( pLoop == 0 ){` |
|        - |  221 | `		/* Illegal continue */` |
|       13 |  222 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"'continue' not in the 'loop' or 'switch' context");` |
|       13 |  223 | `		if( rc == SXERR_ABORT ){` |
|        - |  224 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  225 | `			return SXERR_ABORT;` |
|        - |  226 | `		}` |
|        8 |  227 | `	}else{` |
|    89471 |  228 | `		sxu32 nInstrIdx = 0;` |
|        - |  229 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|    89471 |  230 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|        - |  231 | `		/* ROOT C: in a generator, a break/continue crossing inline trys must run their` |
|        - |  232 | `		 * finallys first. OP_SET_FINALLY_JMP(iP1=count) does that then takes the loop jump. */` |
|    89471 |  233 | `		sxi32 iJmpOp = nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP;` |
|    89471 |  234 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|        - |  235 | ``			/* `continue` inside a switch acts like `break` — which is almost never what`` |
|        - |  236 | `			 * the author meant, so php 7.3+ says so at compile time. The generated jump` |
|        - |  237 | `			 * is unchanged; only the diagnostic was missing. An explicit level` |
|        - |  238 | ``			 * (`continue 2`) targets the enclosing loop and stays silent. */`` |
|        5 |  239 | `			if( iLevel < 1 ){` |
|        5 |  240 | `				PH7_GenCompileError(&(*pGen),E_WARNING,nLineLocal,` |
|        - |  241 | `					"\"continue\" targeting switch is equivalent to \"break\"."` |
|        - |  242 | `					" Did you mean to use \"continue 2\"?");` |
|        2 |  243 | `			}` |
|        5 |  244 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,0,0,&nInstrIdx);` |
|        5 |  245 | `			if( rc == SXRET_OK ){` |
|        5 |  246 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|        2 |  247 | `			}` |
|        3 |  248 | `		}else{` |
|        - |  249 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    89467 |  250 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    89467 |  251 | `			if( pLoop->bPostContinue == TRUE ){` |
|        - |  252 | `				JumpFixup sJumpFix;` |
|        - |  253 | `				/* Post-continue */` |
|    27233 |  254 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|    27233 |  255 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|    27233 |  256 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|    13614 |  257 | `			}` |
|        - |  258 | `		}` |
|        - |  259 | `	}` |
|    89481 |  260 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  261 | `		/* Not so fatal,emit a warning only */` |
|      ! 0 |  262 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|      ! 0 |  263 | `	}` |
|        - |  264 | `	/* Statement successfully compiled */` |
|    89481 |  265 | `	return SXRET_OK;` |
|    44743 |  266 | `}` |
|        - |  267 | `/*` |
|        - |  268 | ` * Compile the 'break' statement.` |
|        - |  269 | ` * According to the PHP language reference` |
|        - |  270 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|        - |  271 | ` *  structure.` |
|        - |  272 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|        - |  273 | ` *  enclosing structures are to be broken out of.` |
|        - |  274 | ` */` |
|    89558 |  275 | `PH7_PRIVATE sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|        5 |  276 | `{` |
|        - |  277 | `	GenBlock *pLoop; /* Target loop */` |
|        - |  278 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|        - |  279 | `	sxi32 rc;` |
|    89563 |  280 | `	iLevel = 0;` |
|        - |  281 | `	/* Jump the 'break' keyword */` |
|    89563 |  282 | `	pGen->pIn++;` |
|    89563 |  283 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|        - |  284 | `		/* optional numeric argument which tells us how many levels` |
|        - |  285 | `		 * of enclosing loops we should skip to the end of.` |
|        - |  286 | `		 */` |
|        - |  287 | `		char zScratch[GEN_NUM_SCRATCH];` |
|       18 |  288 | `		char *zAlloc = 0;` |
|        - |  289 | `		SyString sNum;` |
|       18 |  290 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|       18 |  291 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  292 | `			return SXERR_ABORT;` |
|        - |  293 | `		}` |
|       18 |  294 | `		if( rc == SXRET_OK ){` |
|       21 |  295 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|       12 |  296 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|       15 |  297 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  298 | `				return SXERR_ABORT;` |
|        - |  299 | `			}` |
|       15 |  300 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|       15 |  301 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        6 |  302 | `		}` |
|       18 |  303 | `		if( iLevel < 2 ){` |
|        3 |  304 | `			iLevel = 0;` |
|        1 |  305 | `		}` |
|       18 |  306 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|        7 |  307 | `	}` |
|        - |  308 | `	/* Extract the target loop */` |
|    89563 |  309 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    89563 |  310 | `	if( pLoop == 0 ){` |
|        - |  311 | `		/* Illegal break */` |
|       19 |  312 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"'break' not in the 'loop' or 'switch' context");` |
|       19 |  313 | `		if( rc == SXERR_ABORT ){` |
|        - |  314 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  315 | `			return SXERR_ABORT;` |
|        - |  316 | `		}` |
|       11 |  317 | `	}else{` |
|        - |  318 | `		sxu32 nInstrIdx;` |
|        - |  319 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|    89547 |  320 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|        - |  321 | `		/* ROOT C: OP_SET_FINALLY_JMP runs the crossed inline finallys before the break jump. */` |
|    89547 |  322 | `		rc = PH7_VmEmitInstr(pGen->pVm,nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP,nCross,0,0,&nInstrIdx);` |
|    89547 |  323 | `		if( rc == SXRET_OK ){` |
|        - |  324 | `			/* Fix the jump later when the jump destination is resolved */` |
|    89547 |  325 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|    44771 |  326 | `		}` |
|        - |  327 | `	}` |
|    89563 |  328 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  329 | `		/* Not so fatal,emit a warning only */` |
|      ! 0 |  330 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|      ! 0 |  331 | `	}` |
|        - |  332 | `	/* Statement successfully compiled */` |
|    89563 |  333 | `	return SXRET_OK;` |
|    44784 |  334 | `}` |
|        - |  335 | `/*` |
|        - |  336 | ` * Compile or record a label.` |
|        - |  337 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|        - |  338 | ` * Example` |
|        - |  339 | ` *  goto LABEL;` |
|        - |  340 | ` *   echo 'Foo';` |
|        - |  341 | ` *  LABEL:` |
|        - |  342 | ` *   echo 'Bar';` |
|        - |  343 | ` */` |
|      112 |  344 | `PH7_PRIVATE sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|        5 |  345 | `{` |
|        - |  346 | `	GenBlock *pBlock;` |
|        - |  347 | `	Label sLabel;` |
|        - |  348 | `	/* php places NO restriction on where a label may be DEFINED — inside a loop, a switch` |
|        - |  349 | `	 * or a try{} is all fine. The only rule is on the jump: you may not goto INTO a loop` |
|        - |  350 | `	 * or switch from outside it, which is checked once the labels are all known (see` |
|        - |  351 | `	 * GenStateFixJumps). Record the loop this label sits in so that check can run. */` |
|        - |  352 | `	{` |
|      117 |  353 | `		SyString *pTarget = &pGen->pIn->sData;` |
|        - |  354 | `		char *zDup;` |
|        - |  355 | `		/* Initialize label fields */` |
|      117 |  356 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|        - |  357 | `		/* Duplicate label name */` |
|      117 |  358 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|      117 |  359 | `		if( zDup == 0 ){` |
|      ! 0 |  360 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  361 | `			return SXERR_ABORT;` |
|        - |  362 | `		}` |
|      117 |  363 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|      117 |  364 | `		sLabel.bRef  = FALSE;` |
|      117 |  365 | `		sLabel.nLine = pGen->pIn->nLine;` |
|      117 |  366 | `		sLabel.nLoopId = pGen->nCurLoopId;` |
|      117 |  367 | `		pBlock = pGen->pCurrent;` |
|      233 |  368 | `		while( pBlock ){` |
|      143 |  369 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|       27 |  370 | `				break;` |
|        - |  371 | `			}` |
|        - |  372 | `			/* Point to the upper block */` |
|      121 |  373 | `			pBlock = pBlock->pParent;` |
|        5 |  374 | `		}` |
|      117 |  375 | `		if( pBlock ){` |
|       27 |  376 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       16 |  377 | `		}else{` |
|       95 |  378 | `			sLabel.pFunc = 0;` |
|        - |  379 | `		}` |
|        - |  380 | `		/* Insert in label set */` |
|      117 |  381 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|        - |  382 | `	}` |
|      117 |  383 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|      117 |  384 | `	return SXRET_OK;` |
|       61 |  385 | `}` |
|        - |  386 | `/*` |
|        - |  387 | ` * Compile the so hated 'goto' statement.` |
|        - |  388 | ` * You've probably been taught that gotos are bad, but this sort` |
|        - |  389 | ` * of rewriting  happens all the time, in fact every time you run` |
|        - |  390 | ` * a compiler it has to do this.` |
|        - |  391 | ` * According to the PHP language reference manual` |
|        - |  392 | ` *   The goto operator can be used to jump to another section in the program.` |
|        - |  393 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|        - |  394 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|        - |  395 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|        - |  396 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|        - |  397 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|        - |  398 | ` *   of a multi-level break` |
|        - |  399 | ` */` |
|      152 |  400 | `PH7_PRIVATE sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|        5 |  401 | `{` |
|        - |  402 | `	JumpFixup sJump;` |
|        - |  403 | `	sxi32 rc;` |
|      157 |  404 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|      157 |  405 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - |  406 | `		/* Missing label */` |
|      ! 0 |  407 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|      ! 0 |  408 | `		if( rc == SXERR_ABORT ){` |
|        - |  409 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  410 | `			return SXERR_ABORT;` |
|        - |  411 | `		}` |
|      ! 0 |  412 | `		return SXRET_OK;` |
|        - |  413 | `	}` |
|      157 |  414 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|        6 |  415 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn,"identifier");` |
|        6 |  416 | `		if( rc == SXERR_ABORT ){` |
|        - |  417 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  418 | `			return SXERR_ABORT;` |
|        - |  419 | `		}` |
|        4 |  420 | `	}else{` |
|      153 |  421 | `		SyString *pTarget = &pGen->pIn->sData;` |
|        - |  422 | `		GenBlock *pBlock;` |
|        - |  423 | `		char *zDup;` |
|        - |  424 | `		/* Prepare the jump destination */` |
|      153 |  425 | `		sJump.nJumpType = PH7_OP_JMP;` |
|      153 |  426 | `		sJump.nLine = pGen->pIn->nLine;` |
|        - |  427 | `		/* Duplicate label name */` |
|      153 |  428 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|      153 |  429 | `		if( zDup == 0 ){` |
|      ! 0 |  430 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  431 | `			return SXERR_ABORT;` |
|        - |  432 | `		}` |
|      153 |  433 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|        - |  434 | `		/* The loop/switch this goto sits in, for the "goto into a loop" check later. */` |
|      153 |  435 | `		sJump.nLoopId = pGen->nCurLoopId;` |
|        - |  436 | `		/* A goto inside a try{}/catch{} is legal php (jumping OUT of the block is fine);` |
|        - |  437 | `		 * only the owning function matters here, since a goto may not cross functions. */` |
|      153 |  438 | `		pBlock = pGen->pCurrent;` |
|      327 |  439 | `		while( pBlock ){` |
|      205 |  440 | `			if( pBlock->iFlags & GEN_BLOCK_FUNC ){` |
|       30 |  441 | `				break;` |
|        - |  442 | `			}` |
|        - |  443 | `			/* Point to the upper block */` |
|      179 |  444 | `			pBlock = pBlock->pParent;` |
|        5 |  445 | `		}` |
|      153 |  446 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|       30 |  447 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       17 |  448 | `		}else{` |
|      127 |  449 | `			sJump.pFunc = 0;` |
|        - |  450 | `		}` |
|        - |  451 | `		/* Emit the unconditional jump */` |
|      153 |  452 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|      153 |  453 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|       74 |  454 | `		}` |
|        - |  455 | `	}` |
|      157 |  456 | `	pGen->pIn++; /* Jump the label name */` |
|      157 |  457 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        3 |  458 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|        1 |  459 | `	}` |
|        - |  460 | `	/* Statement successfully compiled */` |
|      157 |  461 | `	return SXRET_OK;` |
|       81 |  462 | `}` |
|        - |  463 | `/*` |
|        - |  464 | ` * Point to the next PHP chunk that will be processed shortly.` |
|        - |  465 | ` * Return SXRET_OK on success. Any other return value indicates` |
|        - |  466 | ` * failure.` |
|        - |  467 | ` */` |
|       20 |  468 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|        2 |  469 | `{` |
|        - |  470 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|        - |  471 | `	sxu32 nRawObj;` |
|       10 |  472 | `	sxu32 nObjIdx;` |
|        - |  473 | `	/* Consume raw chunks verbatim without any processing until we get` |
|        - |  474 | `	 * a PHP block.` |
|        - |  475 | `	 */` |
|       10 |  476 | `Consume:` |
|       22 |  477 | `	nRawObj = nObjIdx = 0;` |
|       22 |  478 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|      ! 0 |  479 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|      ! 0 |  480 | `		if( pRawObj == 0 ){` |
|      ! 0 |  481 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  482 | `			return SXERR_ABORT;` |
|        - |  483 | `		}` |
|        - |  484 | `		/* Mark as constant and emit the load constant instruction */` |
|      ! 0 |  485 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|      ! 0 |  486 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|      ! 0 |  487 | `		++nRawObj;` |
|      ! 0 |  488 | `		pGen->pRawIn++; /* Next chunk */` |
|      ! 0 |  489 | `	}` |
|       22 |  490 | `	if( nRawObj > 0 ){` |
|        - |  491 | `		/* Emit the consume instruction */` |
|      ! 0 |  492 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|      ! 0 |  493 | `	}` |
|       22 |  494 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|      ! 0 |  495 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|        - |  496 | `		/* Reset the token set (and its trivia sidecar) */` |
|      ! 0 |  497 | `		SySetReset(pTokenSet);` |
|      ! 0 |  498 | `		SySetReset(&pGen->aTrivia);` |
|        - |  499 | `		/* Tokenize input */` |
|      ! 0 |  500 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|      ! 0 |  501 | `			pGen->pRawIn->nLine,pTokenSet,&pGen->aTrivia);` |
|        - |  502 | `		/* Point to the fresh token stream */` |
|      ! 0 |  503 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|      ! 0 |  504 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|        - |  505 | `		/* Advance the stream cursor */` |
|      ! 0 |  506 | `		pGen->pRawIn++;` |
|        - |  507 | `		/* TICKET 1433-011 */` |
|      ! 0 |  508 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|        - |  509 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|        - |  510 | `			sxi32 rc;` |
|        - |  511 | `			/* Refer to TICKET 1433-009  */` |
|      ! 0 |  512 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|      ! 0 |  513 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|      ! 0 |  514 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|        - |  515 | `			/* Synthesized short-tag echo: legitimately an expression here. */` |
|      ! 0 |  516 | `			pGen->nExprEchoOk++;` |
|      ! 0 |  517 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|      ! 0 |  518 | `			pGen->nExprEchoOk--;` |
|      ! 0 |  519 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  520 | `				return SXERR_ABORT;` |
|      ! 0 |  521 | `			}else if( rc != SXERR_EMPTY ){` |
|      ! 0 |  522 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      ! 0 |  523 | `			}` |
|      ! 0 |  524 | `			goto Consume;` |
|        - |  525 | `		}` |
|      ! 0 |  526 | `	}else{` |
|        - |  527 | `		/* No more chunks to process */` |
|       22 |  528 | `		pGen->pIn = pGen->pEnd;` |
|       22 |  529 | `		return SXERR_EOF;` |
|        - |  530 | `	}` |
|      ! 0 |  531 | `	return SXRET_OK;` |
|       12 |  532 | `}` |
|        - |  533 | `/*` |
|        - |  534 | ` * Compile a PHP block.` |
|        - |  535 | ` * A block is simply one or more PHP statements and expressions to compile` |
|        - |  536 | ` * optionally delimited by braces {}.` |
|        - |  537 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|        - |  538 | ` * and this function takes care of generating the appropriate error` |
|        - |  539 | ` * message.` |
|        - |  540 | ` */` |
|  6935768 |  541 | `PH7_PRIVATE sxi32 PH7_CompileBlock(` |
|        - |  542 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |  543 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|        - |  544 | `	)` |
|        5 |  545 | `{` |
|        - |  546 | `	sxi32 rc;` |
|        - |  547 | `	sxu32 nLine;` |
|  6935773 |  548 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  6810347 |  549 | `		nLine = pGen->pIn->nLine;` |
|  6810347 |  550 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  6810347 |  551 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  552 | `			return SXERR_ABORT;` |
|        - |  553 | `		}` |
|  6810347 |  554 | `		pGen->pIn++;` |
|        - |  555 | `		/* Compile until we hit the closing braces '}' */` |
|  9964865 |  556 | `		for(;;){` |
| 19929735 |  557 | `			if( pGen->pIn >= pGen->pEnd ){` |
|       22 |  558 | `				rc = GenStateNextChunk(&(*pGen));` |
|       22 |  559 | `				if (rc == SXERR_ABORT ){` |
|      ! 0 |  560 | `			 	   return SXERR_ABORT;` |
|        - |  561 | `				}` |
|       22 |  562 | `				if( rc == SXERR_EOF ){` |
|        - |  563 | `					/* No more token to process: the block was never closed. php reports` |
|        - |  564 | `					 * the line the '{' was opened on, not where the input ran out. */` |
|       22 |  565 | `					PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"Unclosed '{' on line %u",nLine);` |
|       22 |  566 | `					break;` |
|        - |  567 | `				}` |
|      ! 0 |  568 | `			}` |
| 19929715 |  569 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|        - |  570 | `				/* Closing braces found,break immediately*/` |
|  6810327 |  571 | `				pGen->pIn++;` |
|  6810327 |  572 | `				break;` |
|        - |  573 | `			}` |
|        - |  574 | `			/* Compile a single statement */` |
| 13119393 |  575 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
| 13119393 |  576 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  577 | `				return SXERR_ABORT;` |
|        - |  578 | `			}` |
|        5 |  579 | `		}` |
|  6810347 |  580 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  3530602 |  581 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|      ! 0 |  582 | `		pGen->pIn++;` |
|      ! 0 |  583 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|      ! 0 |  584 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  585 | `			return SXERR_ABORT;` |
|        - |  586 | `		}` |
|        - |  587 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|      ! 0 |  588 | `		for(;;){` |
|      ! 0 |  589 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  590 | `				rc = GenStateNextChunk(&(*pGen));` |
|      ! 0 |  591 | `				if (rc == SXERR_ABORT ){` |
|      ! 0 |  592 | `			 	   return SXERR_ABORT;` |
|        - |  593 | `				}` |
|      ! 0 |  594 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|        - |  595 | `					/* No more token to process */` |
|      ! 0 |  596 | `					if( rc == SXERR_EOF ){` |
|      ! 0 |  597 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|        - |  598 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|      ! 0 |  599 | `					}` |
|      ! 0 |  600 | `					break;` |
|        - |  601 | `				}` |
|      ! 0 |  602 | `			}` |
|      ! 0 |  603 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|        - |  604 | `				sxi32 nKwrd;` |
|        - |  605 | `				/* Keyword found */` |
|      ! 0 |  606 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      ! 0 |  607 | `				if( nKwrd == nKeywordEnd \|\|` |
|      ! 0 |  608 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|        - |  609 | `						/* Delimiter keyword found,break */` |
|      ! 0 |  610 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|      ! 0 |  611 | `							pGen->pIn++; /*  endif;endswitch... */` |
|      ! 0 |  612 | `						}` |
|      ! 0 |  613 | `						break;` |
|        - |  614 | `				}` |
|      ! 0 |  615 | `			}` |
|        - |  616 | `			/* Compile a single statement */` |
|      ! 0 |  617 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|      ! 0 |  618 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  619 | `				return SXERR_ABORT;` |
|        - |  620 | `			}` |
|      ! 0 |  621 | `		}` |
|      ! 0 |  622 | `		GenStateLeaveBlock(&(*pGen),0);` |
|      ! 0 |  623 | `	}else{` |
|        - |  624 | `		/* Compile a single statement */` |
|   125431 |  625 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|   125431 |  626 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  627 | `			return SXERR_ABORT;` |
|        - |  628 | `		}` |
|        - |  629 | `	}` |
|        - |  630 | `	/* Jump trailing semi-colons ';' */` |
|  6935773 |  631 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      ! 0 |  632 | `		pGen->pIn++;` |
|      ! 0 |  633 | `	}` |
|  6935773 |  634 | `	return SXRET_OK;` |
|  3467889 |  635 | `}` |
|        - |  636 | `/*` |
|        - |  637 | ` * Compile the gentle 'while' statement.` |
|        - |  638 | ` * According to the PHP language reference` |
|        - |  639 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|        - |  640 | ` *  The basic form of a while statement is:` |
|        - |  641 | ` *  while (expr)` |
|        - |  642 | ` *   statement` |
|        - |  643 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|        - |  644 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|        - |  645 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|        - |  646 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|        - |  647 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|        - |  648 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|        - |  649 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|        - |  650 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|        - |  651 | ` *  while (expr):` |
|        - |  652 | ` *    statement` |
|        - |  653 | ` *   endwhile;` |
|        - |  654 | ` */` |
|    74016 |  655 | `PH7_PRIVATE sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|        5 |  656 | `{` |
|    74021 |  657 | `	GenBlock *pWhileBlock = 0;` |
|    74021 |  658 | `	SyToken *pTmp,*pEnd = 0;` |
|        - |  659 | `	sxu32 nFalseJump;` |
|        - |  660 | `	sxu32 nLine;` |
|        - |  661 | `	sxi32 rc;` |
|    74021 |  662 | `	nLine = pGen->pIn->nLine;` |
|        - |  663 | `	/* Jump the 'while' keyword */` |
|    74021 |  664 | `	pGen->pIn++;` |
|    74021 |  665 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  666 | `		/* Syntax error */` |
|      ! 0 |  667 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|      ! 0 |  668 | `		if( rc == SXERR_ABORT ){` |
|        - |  669 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  670 | `			return SXERR_ABORT;` |
|        - |  671 | `		}` |
|      ! 0 |  672 | `		goto Synchronize;` |
|        - |  673 | `	}` |
|        - |  674 | `	/* Jump the left parenthesis '(' */` |
|    74021 |  675 | `	pGen->pIn++;` |
|        - |  676 | `	/* Create the loop block */` |
|    74021 |  677 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|    74021 |  678 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  679 | `		return SXERR_ABORT;` |
|        - |  680 | `	}` |
|        - |  681 | `	/* Delimit the condition */` |
|    74021 |  682 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    74021 |  683 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  684 | `		/* Empty expression */` |
|        3 |  685 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|        3 |  686 | `		if( rc == SXERR_ABORT ){` |
|        - |  687 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  688 | `			return SXERR_ABORT;` |
|        - |  689 | `		}` |
|        1 |  690 | `	}` |
|        - |  691 | `	/* Swap token streams */` |
|    74021 |  692 | `	pTmp = pGen->pEnd;` |
|    74021 |  693 | `	pGen->pEnd = pEnd;` |
|        - |  694 | `	/* Compile the expression */` |
|    74021 |  695 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    74021 |  696 | `	if( rc == SXERR_ABORT ){` |
|        - |  697 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  698 | `		return SXERR_ABORT;` |
|        - |  699 | `	}` |
|        - |  700 | `	/* Update token stream */` |
|    74021 |  701 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 |  702 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 |  703 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  704 | `			return SXERR_ABORT;` |
|        - |  705 | `		}` |
|      ! 0 |  706 | `		pGen->pIn++;` |
|      ! 0 |  707 | `	}` |
|        - |  708 | `	/* Synchronize pointers */` |
|    74021 |  709 | `	pGen->pIn  = &pEnd[1];` |
|    74021 |  710 | `	pGen->pEnd = pTmp;` |
|        - |  711 | `	/* Emit the false jump */` |
|    74021 |  712 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|        - |  713 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    74021 |  714 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|        - |  715 | `	/* Compile the loop body */` |
|    74021 |  716 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|    74021 |  717 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  718 | `		return SXERR_ABORT;` |
|        - |  719 | `	}` |
|        - |  720 | `	/* Emit the unconditional jump to the start of the loop */` |
|    74021 |  721 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|        - |  722 | `	/* Fix all jumps now the destination is resolved */` |
|    74021 |  723 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  724 | `	/* Release the loop block */` |
|    74021 |  725 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  726 | `	/* Statement successfully compiled */` |
|    74021 |  727 | `	return SXRET_OK;` |
|      ! 0 |  728 | `Synchronize:` |
|        - |  729 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - |  730 | `	 * compiling this erroneous block.` |
|        - |  731 | `	 */` |
|      ! 0 |  732 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  733 | `		pGen->pIn++;` |
|      ! 0 |  734 | `	}` |
|      ! 0 |  735 | `	return SXRET_OK;` |
|    37013 |  736 | `}` |
|        - |  737 | `/*` |
|        - |  738 | ` * Compile the ugly do..while() statement.` |
|        - |  739 | ` * According to the PHP language reference` |
|        - |  740 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|        - |  741 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|        - |  742 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|        - |  743 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|        - |  744 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|        - |  745 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|        - |  746 | ` *  would end immediately).` |
|        - |  747 | ` *  There is just one syntax for do-while loops:` |
|        - |  748 | ` *  <?php` |
|        - |  749 | ` *  $i = 0;` |
|        - |  750 | ` *  do {` |
|        - |  751 | ` *   echo $i;` |
|        - |  752 | ` *  } while ($i > 0);` |
|        - |  753 | ` * ?>` |
|        - |  754 | ` */` |
|        2 |  755 | `PH7_PRIVATE sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|        1 |  756 | `{` |
|        3 |  757 | `	SyToken *pTmp,*pEnd = 0;` |
|        3 |  758 | `	GenBlock *pDoBlock = 0;` |
|        - |  759 | `	sxu32 nLine;` |
|        - |  760 | `	sxi32 rc;` |
|        3 |  761 | `	nLine = pGen->pIn->nLine;` |
|        - |  762 | `	/* Jump the 'do' keyword */` |
|        3 |  763 | `	pGen->pIn++;` |
|        - |  764 | `	/* Create the loop block */` |
|        3 |  765 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|        3 |  766 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  767 | `		return SXERR_ABORT;` |
|        - |  768 | `	}` |
|        - |  769 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|        3 |  770 | `	pDoBlock->bPostContinue = TRUE;` |
|        3 |  771 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|        3 |  772 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  773 | `		return SXERR_ABORT;` |
|        - |  774 | `	}` |
|        3 |  775 | `	if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 |  776 | `		nLine = pGen->pIn->nLine;` |
|      ! 0 |  777 | `	}` |
|        3 |  778 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|      ! 0 |  779 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|        - |  780 | `			/* Missing 'while' statement */` |
|        3 |  781 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|        3 |  782 | `			if( rc == SXERR_ABORT ){` |
|        - |  783 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  784 | `				return SXERR_ABORT;` |
|        - |  785 | `			}` |
|        3 |  786 | `			goto Synchronize;` |
|        - |  787 | `	}` |
|        - |  788 | `	/* Jump the 'while' keyword */` |
|      ! 0 |  789 | `	pGen->pIn++;` |
|      ! 0 |  790 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  791 | `		/* Syntax error */` |
|      ! 0 |  792 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|      ! 0 |  793 | `		if( rc == SXERR_ABORT ){` |
|        - |  794 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  795 | `			return SXERR_ABORT;` |
|        - |  796 | `		}` |
|      ! 0 |  797 | `		goto Synchronize;` |
|        - |  798 | `	}` |
|        - |  799 | `	/* Jump the left parenthesis '(' */` |
|      ! 0 |  800 | `	pGen->pIn++;` |
|        - |  801 | `	/* Delimit the condition */` |
|      ! 0 |  802 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      ! 0 |  803 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  804 | `		/* Empty expression */` |
|      ! 0 |  805 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|      ! 0 |  806 | `		if( rc == SXERR_ABORT ){` |
|        - |  807 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  808 | `			return SXERR_ABORT;` |
|        - |  809 | `		}` |
|      ! 0 |  810 | `		goto Synchronize;` |
|        - |  811 | `	}` |
|        - |  812 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|      ! 0 |  813 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|        - |  814 | `		JumpFixup *aPost;` |
|        - |  815 | `		VmInstr *pInstr;` |
|        - |  816 | `		sxu32 nJumpDest;` |
|        - |  817 | `		sxu32 n;` |
|      ! 0 |  818 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|      ! 0 |  819 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      ! 0 |  820 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|      ! 0 |  821 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      ! 0 |  822 | `			if( pInstr ){` |
|        - |  823 | `				/* Fix */` |
|      ! 0 |  824 | `				pInstr->iP2 = nJumpDest;` |
|      ! 0 |  825 | `			}` |
|      ! 0 |  826 | `		}` |
|      ! 0 |  827 | `	}` |
|        - |  828 | `	/* Swap token streams */` |
|      ! 0 |  829 | `	pTmp = pGen->pEnd;` |
|      ! 0 |  830 | `	pGen->pEnd = pEnd;` |
|        - |  831 | `	/* Compile the expression */` |
|      ! 0 |  832 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      ! 0 |  833 | `	if( rc == SXERR_ABORT ){` |
|        - |  834 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  835 | `		return SXERR_ABORT;` |
|        - |  836 | `	}` |
|        - |  837 | `	/* Update token stream */` |
|      ! 0 |  838 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 |  839 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 |  840 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  841 | `			return SXERR_ABORT;` |
|        - |  842 | `		}` |
|      ! 0 |  843 | `		pGen->pIn++;` |
|      ! 0 |  844 | `	}` |
|      ! 0 |  845 | `	pGen->pIn  = &pEnd[1];` |
|      ! 0 |  846 | `	pGen->pEnd = pTmp;` |
|        - |  847 | `	/* Emit the true jump to the beginning of the loop */` |
|      ! 0 |  848 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|        - |  849 | `	/* Fix all jumps now the destination is resolved */` |
|      ! 0 |  850 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  851 | `	/* Release the loop block */` |
|      ! 0 |  852 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  853 | `	/* Statement successfully compiled */` |
|      ! 0 |  854 | `	return SXRET_OK;` |
|        1 |  855 | `Synchronize:` |
|        - |  856 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - |  857 | `	 * compiling this erroneous block.` |
|        - |  858 | `	 */` |
|        3 |  859 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  860 | `		pGen->pIn++;` |
|      ! 0 |  861 | `	}` |
|        3 |  862 | `	return SXRET_OK;` |
|        2 |  863 | `}` |
|        - |  864 | `/*` |
|        - |  865 | ` * Compile the complex and powerful 'for' statement.` |
|        - |  866 | ` * According to the PHP language reference` |
|        - |  867 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|        - |  868 | ` *  The syntax of a for loop is:` |
|        - |  869 | ` *  for (expr1; expr2; expr3)` |
|        - |  870 | ` *   statement` |
|        - |  871 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|        - |  872 | ` *  the beginning of the loop.` |
|        - |  873 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|        - |  874 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|        - |  875 | ` *  to FALSE, the execution of the loop ends.` |
|        - |  876 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|        - |  877 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|        - |  878 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|        - |  879 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|        - |  880 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|        - |  881 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|        - |  882 | ` *  of using the for truth expression.` |
|        - |  883 | ` */` |
|   128456 |  884 | `PH7_PRIVATE sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|        5 |  885 | `{` |
|   128461 |  886 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   128461 |  887 | `	GenBlock *pForBlock = 0;` |
|        - |  888 | `	sxu32 nFalseJump;` |
|        - |  889 | `	sxu32 nLine;` |
|        - |  890 | `	sxi32 rc;` |
|   128461 |  891 | `	nLine = pGen->pIn->nLine;` |
|        - |  892 | `	/* Jump the 'for' keyword */` |
|   128461 |  893 | `	pGen->pIn++;` |
|   128461 |  894 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  895 | `		/* Syntax error */` |
|      ! 0 |  896 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|      ! 0 |  897 | `		if( rc == SXERR_ABORT ){` |
|        - |  898 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  899 | `			return SXERR_ABORT;` |
|        - |  900 | `		}` |
|      ! 0 |  901 | `		return SXRET_OK;` |
|        - |  902 | `	}` |
|        - |  903 | `	/* Jump the left parenthesis '(' */` |
|   128461 |  904 | `	pGen->pIn++;` |
|        - |  905 | `	/* Delimit the init-expr;condition;post-expr */` |
|   128461 |  906 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   128461 |  907 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  908 | `		/* Empty expression */` |
|      ! 0 |  909 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|      ! 0 |  910 | `		if( rc == SXERR_ABORT ){` |
|        - |  911 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  912 | `			return SXERR_ABORT;` |
|        - |  913 | `		}` |
|        - |  914 | `		/* Synchronize */` |
|      ! 0 |  915 | `		pGen->pIn = pEnd;` |
|      ! 0 |  916 | `		if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 |  917 | `			pGen->pIn++;` |
|      ! 0 |  918 | `		}` |
|      ! 0 |  919 | `		return SXRET_OK;` |
|        - |  920 | `	}` |
|        - |  921 | `	/* Swap token streams */` |
|   128461 |  922 | `	pTmp = pGen->pEnd;` |
|   128461 |  923 | `	pGen->pEnd = pEnd;` |
|        - |  924 | `	/* for() clauses are the ONLY place php's grammar allows a comma-separated` |
|        - |  925 | `	 * expression list, so the comma operator is permitted for their duration` |
|        - |  926 | `	 * (see GenStateTreeHasComma). A closure body nested inside a clause is` |
|        - |  927 | `	 * compiled through this same window — recorded as a known leniency. */` |
|   128461 |  928 | `	pGen->nCommaExprOk++;` |
|        - |  929 | `	/* Compile initialization expressions if available */` |
|   128461 |  930 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  931 | `	/* Pop operand lvalues */` |
|   128461 |  932 | `	if( rc == SXERR_ABORT ){` |
|        - |  933 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  934 | `		return SXERR_ABORT;` |
|   128461 |  935 | `	}else if( rc != SXERR_EMPTY ){` |
|   116795 |  936 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    58395 |  937 | `	}` |
|   128461 |  938 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  939 | `		/* Syntax error */` |
|      ! 0 |  940 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|      ! 0 |  941 | `		if( rc == SXERR_ABORT ){` |
|        - |  942 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  943 | `			return SXERR_ABORT;` |
|        - |  944 | `		}` |
|      ! 0 |  945 | `		return SXRET_OK;` |
|        - |  946 | `	}` |
|        - |  947 | `	/* Jump the trailing ';' */` |
|   128461 |  948 | `	pGen->pIn++;` |
|        - |  949 | `	/* Create the loop block */` |
|   128461 |  950 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   128461 |  951 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  952 | `		return SXERR_ABORT;` |
|        - |  953 | `	}` |
|        - |  954 | `	/* Deffer continue jumps */` |
|   128461 |  955 | `	pForBlock->bPostContinue = TRUE;` |
|        - |  956 | `	/* Compile the condition */` |
|   128461 |  957 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   128461 |  958 | `	if( rc == SXERR_ABORT ){` |
|        - |  959 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  960 | `		return SXERR_ABORT;` |
|   128461 |  961 | `	}else if( rc != SXERR_EMPTY ){` |
|        - |  962 | `		/* Emit the false jump */` |
|   116795 |  963 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|        - |  964 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   116795 |  965 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    58395 |  966 | `	}` |
|   128461 |  967 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  968 | `		/* Syntax error */` |
|        6 |  969 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|        6 |  970 | `		if( rc == SXERR_ABORT ){` |
|        - |  971 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  972 | `			return SXERR_ABORT;` |
|        - |  973 | `		}` |
|        6 |  974 | `		return SXRET_OK;` |
|        - |  975 | `	}` |
|        - |  976 | `	/* Jump the trailing ';' */` |
|   128457 |  977 | `	pGen->pIn++;` |
|        - |  978 | `	/* Save the post condition stream */` |
|   128457 |  979 | `	pPostStart = pGen->pIn;` |
|        - |  980 | `	/* Compile the loop body — OUTSIDE the comma window (the body is ordinary` |
|        - |  981 | ``	 * php, so `(1, 2)` inside it is the parse error it should be). */`` |
|   128457 |  982 | `	pGen->nCommaExprOk--;` |
|   128457 |  983 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   128457 |  984 | `	pGen->pEnd = pTmp;` |
|   128457 |  985 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   128457 |  986 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  987 | `		return SXERR_ABORT;` |
|        - |  988 | `	}` |
|        - |  989 | `	/* Fix post-continue jumps */` |
|   128457 |  990 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|        - |  991 | `		JumpFixup *aPost;` |
|        - |  992 | `		VmInstr *pInstr;` |
|        - |  993 | `		sxu32 nJumpDest;` |
|        - |  994 | `		sxu32 n;` |
|    11681 |  995 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|    11681 |  996 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|    38909 |  997 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|    27233 |  998 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|    27233 |  999 | `			if( pInstr ){` |
|        - | 1000 | `				/* Fix jump */` |
|    27233 | 1001 | `				pInstr->iP2 = nJumpDest;` |
|    13614 | 1002 | `			}` |
|    13619 | 1003 | `		}` |
|     5838 | 1004 | `	}` |
|        - | 1005 | `	/* compile the post-expressions if available */` |
|   128457 | 1006 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|      ! 0 | 1007 | `		pPostStart++;` |
|      ! 0 | 1008 | `	}` |
|   128457 | 1009 | `	if( pPostStart < pEnd ){` |
|        - | 1010 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   116793 | 1011 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   116793 | 1012 | `		pGen->nCommaExprOk++; /* post-expressions are a clause list again */` |
|   116793 | 1013 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   116793 | 1014 | `		pGen->nCommaExprOk--;` |
|   116793 | 1015 | `		if( pGen->pIn < pGen->pEnd ){` |
|        - | 1016 | `			/* Syntax error */` |
|      ! 0 | 1017 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|      ! 0 | 1018 | `			if( rc == SXERR_ABORT ){` |
|        - | 1019 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 1020 | `				return SXERR_ABORT;` |
|        - | 1021 | `			}` |
|      ! 0 | 1022 | `			return SXRET_OK;` |
|        - | 1023 | `		}` |
|   116793 | 1024 | `		RE_SWAP_DELIMITER(pGen);` |
|   116793 | 1025 | `		if( rc == SXERR_ABORT ){` |
|        - | 1026 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1027 | `			return SXERR_ABORT;` |
|   116793 | 1028 | `		}else if( rc != SXERR_EMPTY){` |
|        - | 1029 | `			/* Pop operand lvalue */` |
|   116793 | 1030 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    58394 | 1031 | `		}` |
|    58394 | 1032 | `	}` |
|        - | 1033 | `	/* Emit the unconditional jump to the start of the loop */` |
|   128457 | 1034 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|        - | 1035 | `	/* Fix all jumps now the destination is resolved */` |
|   128457 | 1036 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 1037 | `	/* Release the loop block */` |
|   128457 | 1038 | `	GenStateLeaveBlock(pGen,0);` |
|        - | 1039 | `	/* Statement successfully compiled */` |
|   128457 | 1040 | `	return SXRET_OK;` |
|    64233 | 1041 | `}` |
|        - | 1042 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|        - | 1043 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|        - | 1044 | ` * are allowed.` |
|        - | 1045 | ` */` |
|   463816 | 1046 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 1047 | `{` |
|   463821 | 1048 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|   463821 | 1049 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|        - | 1050 | `		/* Unexpected expression */` |
|      ! 0 | 1051 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 1052 | `			"foreach: Expecting a variable name");` |
|      ! 0 | 1053 | `		if( rc != SXERR_ABORT ){` |
|      ! 0 | 1054 | `			rc = SXERR_INVALID;` |
|      ! 0 | 1055 | `		}` |
|      ! 0 | 1056 | `	}` |
|   463821 | 1057 | `	return rc;` |
|        5 | 1058 | `}` |
|        - | 1059 | `/*` |
|        - | 1060 | ` * Compile the 'foreach' statement.` |
|        - | 1061 | ` * According to the PHP language reference` |
|        - | 1062 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|        - | 1063 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|        - | 1064 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|        - | 1065 | ` *  is a minor but useful extension of the first:` |
|        - | 1066 | ` *  foreach (array_expression as $value)` |
|        - | 1067 | ` *    statement` |
|        - | 1068 | ` *  foreach (array_expression as $key => $value)` |
|        - | 1069 | ` *   statement` |
|        - | 1070 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|        - | 1071 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|        - | 1072 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|        - | 1073 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|        - | 1074 | ` *  to the variable $key on each loop.` |
|        - | 1075 | ` *  Note:` |
|        - | 1076 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|        - | 1077 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|        - | 1078 | ` *  Note:` |
|        - | 1079 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|        - | 1080 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|        - | 1081 | ` *  or after the foreach without resetting it.` |
|        - | 1082 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|        - | 1083 | ` *  of copying the value.` |
|        - | 1084 | ` */` |
|   327436 | 1085 | `PH7_PRIVATE sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|        5 | 1086 | `{` |
|   327441 | 1087 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|   327441 | 1088 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|   327441 | 1089 | `	GenBlock *pForeachBlock = 0;` |
|        - | 1090 | `	ph7_foreach_info *pInfo;` |
|        - | 1091 | `	sxu32 nFalseJump;` |
|        - | 1092 | `	VmInstr *pInstr;` |
|        - | 1093 | `	sxu32 nLine;` |
|        - | 1094 | `	sxi32 rc;` |
|   327441 | 1095 | `	nLine = pGen->pIn->nLine;` |
|        - | 1096 | `	/* Jump the 'foreach' keyword */` |
|   327441 | 1097 | `	pGen->pIn++;` |
|   327441 | 1098 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1099 | `		/* Syntax error */` |
|      ! 0 | 1100 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|      ! 0 | 1101 | `		if( rc == SXERR_ABORT ){` |
|        - | 1102 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1103 | `			return SXERR_ABORT;` |
|        - | 1104 | `		}` |
|      ! 0 | 1105 | `		goto Synchronize;` |
|        - | 1106 | `	}` |
|        - | 1107 | `	/* Jump the left parenthesis '(' */` |
|   327441 | 1108 | `	pGen->pIn++;` |
|        - | 1109 | `	/* Create the loop block */` |
|   327441 | 1110 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|   327441 | 1111 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1112 | `		return SXERR_ABORT;` |
|        - | 1113 | `	}` |
|        - | 1114 | `	/* Delimit the expression */` |
|   327441 | 1115 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   327441 | 1116 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - | 1117 | `		/* Empty expression */` |
|      ! 0 | 1118 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|      ! 0 | 1119 | `		if( rc == SXERR_ABORT ){` |
|        - | 1120 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1121 | `			return SXERR_ABORT;` |
|        - | 1122 | `		}` |
|        - | 1123 | `		/* Synchronize */` |
|      ! 0 | 1124 | `		pGen->pIn = pEnd;` |
|      ! 0 | 1125 | `		if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 | 1126 | `			pGen->pIn++;` |
|      ! 0 | 1127 | `		}` |
|      ! 0 | 1128 | `		return SXRET_OK;` |
|        - | 1129 | `	}` |
|        - | 1130 | `	/* Compile the array expression */` |
|   327441 | 1131 | `	pCur = pGen->pIn;` |
|  1855865 | 1132 | `	while( pCur < pEnd ){` |
|  1855865 | 1133 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|   358567 | 1134 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|   358567 | 1135 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|        - | 1136 | `				/* Break with the first 'as' found */` |
|   327441 | 1137 | `				break;` |
|        - | 1138 | `			}` |
|    15563 | 1139 | `		}` |
|        - | 1140 | `		/* Advance the stream cursor */` |
|  1528429 | 1141 | `		pCur++;` |
|        5 | 1142 | `	}` |
|   327441 | 1143 | `	if( pCur <= pGen->pIn ){` |
|      ! 0 | 1144 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 1145 | `			"foreach: Missing array/object expression");` |
|      ! 0 | 1146 | `		if( rc == SXERR_ABORT ){` |
|        - | 1147 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1148 | `			return SXERR_ABORT;` |
|        - | 1149 | `		}` |
|      ! 0 | 1150 | `		goto Synchronize;` |
|        - | 1151 | `	}` |
|        - | 1152 | `	/* Swap token streams */` |
|   327441 | 1153 | `	pTmp = pGen->pEnd;` |
|   327441 | 1154 | `	pGen->pEnd = pCur;` |
|   327441 | 1155 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   327441 | 1156 | `	if( rc == SXERR_ABORT ){` |
|        - | 1157 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1158 | `		return SXERR_ABORT;` |
|        - | 1159 | `	}` |
|        - | 1160 | `	/* Update token stream */` |
|   327441 | 1161 | `	while(pGen->pIn < pCur ){` |
|      ! 0 | 1162 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 1163 | `		if( rc == SXERR_ABORT ){` |
|        - | 1164 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1165 | `			return SXERR_ABORT;` |
|        - | 1166 | `		}` |
|      ! 0 | 1167 | `		pGen->pIn++;` |
|      ! 0 | 1168 | `	}` |
|   327441 | 1169 | `	pCur++; /* Jump the 'as' keyword */` |
|   327441 | 1170 | `	pGen->pIn = pCur;` |
|   327441 | 1171 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 1172 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|      ! 0 | 1173 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1174 | `			return SXERR_ABORT;` |
|        - | 1175 | `		}` |
|      ! 0 | 1176 | `	}` |
|        - | 1177 | `	/* Create the foreach context */` |
|   327441 | 1178 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|   327441 | 1179 | `	if( pInfo == 0 ){` |
|      ! 0 | 1180 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|      ! 0 | 1181 | `		return SXERR_ABORT;` |
|        - | 1182 | `	}` |
|        - | 1183 | `	/* Zero the structure */` |
|   327441 | 1184 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|        - | 1185 | `	/* Initialize structure fields */` |
|   327441 | 1186 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|        - | 1187 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|        - | 1188 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|        - | 1189 | `	 * '=>'. */` |
|   327441 | 1190 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|   327441 | 1191 | `	if( pCur < pEnd ){` |
|        - | 1192 | `		/* Compile the expression holding the key name */` |
|   136409 | 1193 | `		if( pGen->pIn >= pCur ){` |
|      ! 0 | 1194 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|      ! 0 | 1195 | `			if( rc == SXERR_ABORT ){` |
|        - | 1196 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1197 | `				return SXERR_ABORT;` |
|        - | 1198 | `			}` |
|      ! 0 | 1199 | `		}else{` |
|   136409 | 1200 | `			pGen->pEnd = pCur;` |
|   136409 | 1201 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|   136409 | 1202 | `			if( rc == SXERR_ABORT ){` |
|        - | 1203 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1204 | `				return SXERR_ABORT;` |
|        - | 1205 | `			}` |
|   136409 | 1206 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|   136409 | 1207 | `			if( pInstr->p3 ){` |
|        - | 1208 | `				/* Record key name */` |
|   136409 | 1209 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    68202 | 1210 | `			}` |
|   136409 | 1211 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|        - | 1212 | `		}` |
|   136409 | 1213 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    68202 | 1214 | `	}` |
|   327441 | 1215 | `	pGen->pEnd = pEnd;` |
|   327441 | 1216 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 1217 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|      ! 0 | 1218 | `		if( rc == SXERR_ABORT ){` |
|        - | 1219 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1220 | `			return SXERR_ABORT;` |
|        - | 1221 | `		}` |
|      ! 0 | 1222 | `		goto Synchronize;` |
|        - | 1223 | `	}` |
|   327441 | 1224 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|       33 | 1225 | `		pGen->pIn++;` |
|        - | 1226 | `		/* Pass by reference  */` |
|       33 | 1227 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       15 | 1228 | `	}` |
|        - | 1229 | `	/* Check if the value target is list() */` |
|   327441 | 1230 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        8 | 1231 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|        - | 1232 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|        - | 1233 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|        - | 1234 | `		 */` |
|        - | 1235 | `		static int iForeachListCnt = 0;` |
|        - | 1236 | `		char zTmp[128];` |
|        - | 1237 | `		sxu32 nLen;` |
|        - | 1238 | `		char *zDup;` |
|       10 | 1239 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|       10 | 1240 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       10 | 1241 | `		if( zDup == 0 ){` |
|      ! 0 | 1242 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1243 | `			return SXERR_ABORT;` |
|        - | 1244 | `		}` |
|       10 | 1245 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|        - | 1246 | `		/* Save list() token boundaries */` |
|       10 | 1247 | `		pListStart = pGen->pIn;` |
|        - | 1248 | `		/* Advance past list(...) — validate parentheses */` |
|       10 | 1249 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|       10 | 1250 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        3 | 1251 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|        - | 1252 | `				"foreach: Expected '(' after 'list'");` |
|        3 | 1253 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1254 | `				return SXERR_ABORT;` |
|        - | 1255 | `			}` |
|        3 | 1256 | `			goto Synchronize;` |
|        - | 1257 | `		}` |
|        7 | 1258 | `		pGen->pIn++; /* Jump '(' */` |
|        7 | 1259 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|        7 | 1260 | `		if( pListEnd >= pEnd ){` |
|      ! 0 | 1261 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1262 | `				"foreach: Missing closing ')' after list");` |
|      ! 0 | 1263 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1264 | `				return SXERR_ABORT;` |
|        - | 1265 | `			}` |
|      ! 0 | 1266 | `			goto Synchronize;` |
|        - | 1267 | `		}` |
|        7 | 1268 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|        7 | 1269 | `		pListEnd = pGen->pIn;` |
|        7 | 1270 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|   327436 | 1271 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|        - | 1272 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|        - | 1273 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|        - | 1274 | `		 */` |
|        - | 1275 | `		static int iForeachShortListCnt = 0;` |
|        - | 1276 | `		char zTmp[128];` |
|        - | 1277 | `		sxu32 nLen;` |
|        - | 1278 | `		char *zDup;` |
|       17 | 1279 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       17 | 1280 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       17 | 1281 | `		if( zDup == 0 ){` |
|      ! 0 | 1282 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1283 | `			return SXERR_ABORT;` |
|        - | 1284 | `		}` |
|       17 | 1285 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|        - | 1286 | `		/* Save [...] token boundaries */` |
|       17 | 1287 | `		pListStart = pGen->pIn;` |
|        - | 1288 | `		/* Advance past [...] */` |
|       17 | 1289 | `		pGen->pIn++; /* Jump '[' */` |
|       17 | 1290 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       17 | 1291 | `		if( pListEnd >= pEnd ){` |
|      ! 0 | 1292 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1293 | `				"foreach: Missing closing ']' after short list");` |
|      ! 0 | 1294 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1295 | `				return SXERR_ABORT;` |
|        - | 1296 | `			}` |
|      ! 0 | 1297 | `			goto Synchronize;` |
|        - | 1298 | `		}` |
|       17 | 1299 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       17 | 1300 | `		pListEnd = pGen->pIn;` |
|       17 | 1301 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|        9 | 1302 | `	}else{` |
|        - | 1303 | `		/* Compile the expression holding the value name */` |
|   327417 | 1304 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|   327417 | 1305 | `		if( rc == SXERR_ABORT ){` |
|        - | 1306 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1307 | `			return SXERR_ABORT;` |
|        - | 1308 | `		}` |
|   327417 | 1309 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|   327417 | 1310 | `		if( pInstr->p3 ){` |
|        - | 1311 | `			/* Record value name */` |
|   327417 | 1312 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|   163706 | 1313 | `		}` |
|        - | 1314 | `	}` |
|        - | 1315 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|   327439 | 1316 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|        - | 1317 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   327439 | 1318 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|        - | 1319 | `	/* Record the first instruction to execute */` |
|   327439 | 1320 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|        - | 1321 | `	/* Emit the FOREACH_STEP instruction */` |
|   327439 | 1322 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|        - | 1323 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   327439 | 1324 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|        - | 1325 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|   327439 | 1326 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|        - | 1327 | `		SyToken *pSavedIn,*pSavedEnd;` |
|        - | 1328 | `		/* Load the temporary variable holding the current value onto the stack.` |
|        - | 1329 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|        - | 1330 | `		 */` |
|       23 | 1331 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|        - | 1332 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|        - | 1333 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|        - | 1334 | `		 * picks up the delimiter and the variable names inside.` |
|        - | 1335 | `		 */` |
|       23 | 1336 | `		pSavedIn = pGen->pIn;` |
|       23 | 1337 | `		pSavedEnd = pGen->pEnd;` |
|       23 | 1338 | `		pGen->pIn = pListStart;` |
|       23 | 1339 | `		pGen->pEnd = pListEnd;` |
|       23 | 1340 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       17 | 1341 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|        9 | 1342 | `		}else{` |
|        7 | 1343 | `			rc = PH7_CompileList(&(*pGen),0);` |
|        - | 1344 | `		}` |
|       23 | 1345 | `		pGen->pIn = pSavedIn;` |
|       23 | 1346 | `		pGen->pEnd = pSavedEnd;` |
|       23 | 1347 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1348 | `			return SXERR_ABORT;` |
|        - | 1349 | `		}` |
|        - | 1350 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|       23 | 1351 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       11 | 1352 | `	}` |
|        - | 1353 | `	/* Compile the loop body */` |
|   327439 | 1354 | `	pGen->pIn = &pEnd[1];` |
|   327439 | 1355 | `	pGen->pEnd = pTmp;` |
|   327439 | 1356 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|   327439 | 1357 | `	if( rc == SXERR_ABORT ){` |
|        - | 1358 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1359 | `		return SXERR_ABORT;` |
|        - | 1360 | `	}` |
|        - | 1361 | `	/* Emit the unconditional jump to the start of the loop */` |
|   327439 | 1362 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|        - | 1363 | `	/* Fix all jumps now the destination is resolved */` |
|   327439 | 1364 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 1365 | `	/* Release the loop block */` |
|   327439 | 1366 | `	GenStateLeaveBlock(pGen,0);` |
|        - | 1367 | `	/* Statement successfully compiled */` |
|   327439 | 1368 | `	return SXRET_OK;` |
|        1 | 1369 | `Synchronize:` |
|        - | 1370 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - | 1371 | `	 * compiling this erroneous block.` |
|        - | 1372 | `	 */` |
|        3 | 1373 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 | 1374 | `		pGen->pIn++;` |
|      ! 0 | 1375 | `	}` |
|        3 | 1376 | `	return SXRET_OK;` |
|   163723 | 1377 | `}` |
|        - | 1378 | `/*` |
|        - | 1379 | ` * Compile the infamous if/elseif/else if/else statements.` |
|        - | 1380 | ` * According to the PHP language reference` |
|        - | 1381 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|        - | 1382 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|        - | 1383 | ` *  that is similar to that of C:` |
|        - | 1384 | ` *  if (expr)` |
|        - | 1385 | ` *   statement` |
|        - | 1386 | ` *  else construct:` |
|        - | 1387 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|        - | 1388 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|        - | 1389 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|        - | 1390 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|        - | 1391 | ` *   $b, and a is NOT greater than b otherwise.` |
|        - | 1392 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|        - | 1393 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|        - | 1394 | ` *  elseif` |
|        - | 1395 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|        - | 1396 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|        - | 1397 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|        - | 1398 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|        - | 1399 | ` *   than b, a equal to b or a is smaller than b:` |
|        - | 1400 | ` *   <?php` |
|        - | 1401 | ` *    if ($a > $b) {` |
|        - | 1402 | ` *     echo "a is bigger than b";` |
|        - | 1403 | ` *    } elseif ($a == $b) {` |
|        - | 1404 | ` *     echo "a is equal to b";` |
|        - | 1405 | ` *    } else {` |
|        - | 1406 | ` *     echo "a is smaller than b";` |
|        - | 1407 | ` *    }` |
|        - | 1408 | ` *    ?>` |
|        - | 1409 | ` */` |
|  2431690 | 1410 | `PH7_PRIVATE sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|        5 | 1411 | `{` |
|  2431695 | 1412 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  2431695 | 1413 | `	GenBlock *pCondBlock = 0;` |
|        - | 1414 | `	sxu32 nJumpIdx;` |
|        - | 1415 | `	sxu32 nKeyID;` |
|        - | 1416 | `	sxi32 rc;` |
|        - | 1417 | `	/* Jump the 'if' keyword */` |
|  2431695 | 1418 | `	pGen->pIn++;` |
|  2431695 | 1419 | `	pToken = pGen->pIn;` |
|        - | 1420 | `	/* Create the conditional block */` |
|  2431695 | 1421 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  2431695 | 1422 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1423 | `		return SXERR_ABORT;` |
|        - | 1424 | `	}` |
|        - | 1425 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|  1359772 | 1426 | `	for(;;){` |
|  2719549 | 1427 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1428 | `			/* Syntax error */` |
|      ! 0 | 1429 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 1430 | `				pToken--;` |
|      ! 0 | 1431 | `			}` |
|      ! 0 | 1432 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|      ! 0 | 1433 | `			if( rc == SXERR_ABORT ){` |
|        - | 1434 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 1435 | `				return SXERR_ABORT;` |
|        - | 1436 | `			}` |
|      ! 0 | 1437 | `			goto Synchronize;` |
|        - | 1438 | `		}` |
|        - | 1439 | `		/* Jump the left parenthesis '(' */` |
|  2719549 | 1440 | `		pToken++;` |
|        - | 1441 | `		/* Delimit the condition */` |
|  2719549 | 1442 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  2719549 | 1443 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|        - | 1444 | `			/* Syntax error */` |
|        6 | 1445 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 1446 | `				pToken--;` |
|      ! 0 | 1447 | `			}` |
|        6 | 1448 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|        6 | 1449 | `			if( rc == SXERR_ABORT ){` |
|        - | 1450 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 1451 | `				return SXERR_ABORT;` |
|        - | 1452 | `			}` |
|        6 | 1453 | `			goto Synchronize;` |
|        - | 1454 | `		}` |
|        - | 1455 | `		/* Swap token streams */` |
|  2719545 | 1456 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|        - | 1457 | `		/* Compile the condition */` |
|  2719545 | 1458 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 1459 | `		/* Update token stream */` |
|  2719545 | 1460 | `		while(pGen->pIn < pEnd ){` |
|      ! 0 | 1461 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 | 1462 | `			pGen->pIn++;` |
|      ! 0 | 1463 | `		}` |
|  2719545 | 1464 | `		pGen->pIn  = &pEnd[1];` |
|  2719545 | 1465 | `		pGen->pEnd = pTmp;` |
|  2719545 | 1466 | `		if( rc == SXERR_ABORT ){` |
|        - | 1467 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|        3 | 1468 | `			return SXERR_ABORT;` |
|        - | 1469 | `		}` |
|        - | 1470 | `		/* Emit the false jump */` |
|  2719543 | 1471 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|        - | 1472 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  2719543 | 1473 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|        - | 1474 | `		/* Compile the body */` |
|  2719543 | 1475 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  2719543 | 1476 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1477 | `			return SXERR_ABORT;` |
|        - | 1478 | `		}` |
|  2719543 | 1479 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   521481 | 1480 | `			break;` |
|        - | 1481 | `		}` |
|        - | 1482 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|  1676591 | 1483 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  1676591 | 1484 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|  1166703 | 1485 | `			break;` |
|        - | 1486 | `		}` |
|        - | 1487 | `		/* Emit the unconditional jump */` |
|   509893 | 1488 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|        - | 1489 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   509893 | 1490 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   509893 | 1491 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   307575 | 1492 | `			pToken = &pGen->pIn[1];` |
|   307575 | 1493 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    85574 | 1494 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|   111022 | 1495 | `					break;` |
|        - | 1496 | `			}` |
|    85541 | 1497 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    42768 | 1498 | `		}` |
|   287859 | 1499 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|        - | 1500 | `		/* Synchronize cursors */` |
|   287859 | 1501 | `		pToken = pGen->pIn;` |
|        - | 1502 | `		/* Fix the false jump */` |
|   287859 | 1503 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|        5 | 1504 | `	} /* For(;;) */` |
|        - | 1505 | `	/* Fix the false jump */` |
|  2431689 | 1506 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  2431689 | 1507 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|  1388732 | 1508 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|        - | 1509 | `			/* Compile the else block */` |
|   222039 | 1510 | `			pGen->pIn++;` |
|   222039 | 1511 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   222039 | 1512 | `			if( rc == SXERR_ABORT ){` |
|        - | 1513 |  |
|      ! 0 | 1514 | `				return SXERR_ABORT;` |
|        - | 1515 | `			}` |
|   111017 | 1516 | `	}` |
|  2431689 | 1517 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|        - | 1518 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  2431689 | 1519 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|        - | 1520 | `	/* Release the conditional block */` |
|  2431689 | 1521 | `	GenStateLeaveBlock(pGen,0);` |
|        - | 1522 | `	/* Statement successfully compiled */` |
|  2431689 | 1523 | `	return SXRET_OK;` |
|        2 | 1524 | `Synchronize:` |
|        - | 1525 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|        - | 1526 | `	 */` |
|       34 | 1527 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       30 | 1528 | `		pGen->pIn++;` |
|        2 | 1529 | `	}` |
|        6 | 1530 | `	return SXRET_OK;` |
|  1215850 | 1531 | `}` |
|        - | 1532 | `/*` |
|        - | 1533 | ` * Compile the global construct.` |
|        - | 1534 | ` * According to the PHP language reference` |
|        - | 1535 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|        - | 1536 | ` *  to be used in that function.` |
|        - | 1537 | ` *  Example #1 Using global` |
|        - | 1538 | ` *  <?php` |
|        - | 1539 | ` *   $a = 1;` |
|        - | 1540 | ` *   $b = 2;` |
|        - | 1541 | ` *   function Sum()` |
|        - | 1542 | ` *   {` |
|        - | 1543 | ` *    global $a, $b;` |
|        - | 1544 | ` *    $b = $a + $b;` |
|        - | 1545 | ` *   }` |
|        - | 1546 | ` *   Sum();` |
|        - | 1547 | ` *   echo $b;` |
|        - | 1548 | ` *  ?>` |
|        - | 1549 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|        - | 1550 | ` *  all references to either variable will refer to the global version. There is no limit` |
|        - | 1551 | ` *  to the number of global variables that can be manipulated by a function.` |
|        - | 1552 | ` */` |
|       36 | 1553 | `PH7_PRIVATE sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|        5 | 1554 | `{` |
|       41 | 1555 | `	SyToken *pTmp,*pNext = 0;` |
|        - | 1556 | `	sxi32 nExpr;` |
|        - | 1557 | `	sxi32 rc;` |
|        - | 1558 | `	/* Jump the 'global' keyword */` |
|       41 | 1559 | `	pGen->pIn++;` |
|       41 | 1560 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|        - | 1561 | `		/* Nothing to process */` |
|      ! 0 | 1562 | `		return SXRET_OK;` |
|        - | 1563 | `	}` |
|       41 | 1564 | `	pTmp = pGen->pEnd;` |
|       41 | 1565 | `	nExpr = 0;` |
|       87 | 1566 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       51 | 1567 | `		if( pGen->pIn < pNext ){` |
|       51 | 1568 | `			pGen->pEnd = pNext;` |
|       51 | 1569 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 | 1570 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|      ! 0 | 1571 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1572 | `					return SXERR_ABORT;` |
|        - | 1573 | `				}` |
|      ! 0 | 1574 | `			}else{` |
|       51 | 1575 | `				pGen->pIn++;` |
|       51 | 1576 | `				if( pGen->pIn >= pGen->pEnd ){` |
|        - | 1577 | `					/* Emit a warning */` |
|      ! 0 | 1578 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|      ! 0 | 1579 | `				}else{` |
|       51 | 1580 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       51 | 1581 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 1582 | `						return SXERR_ABORT;` |
|       51 | 1583 | `					}else if(rc != SXERR_EMPTY ){` |
|       51 | 1584 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|       51 | 1585 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|        - | 1586 | `							/* Variable name, not a constant */` |
|       51 | 1587 | `							pLast->iP1 = 0;` |
|       23 | 1588 | `						}` |
|       51 | 1589 | `						nExpr++;` |
|       23 | 1590 | `					}` |
|        - | 1591 | `				}` |
|        - | 1592 | `			}` |
|       23 | 1593 | `		}` |
|        - | 1594 | `		/* Next expression in the stream */` |
|       51 | 1595 | `		pGen->pIn = pNext;` |
|        - | 1596 | `		/* Jump trailing commas */` |
|       61 | 1597 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       15 | 1598 | `			pGen->pIn++;` |
|        5 | 1599 | `		}` |
|        5 | 1600 | `	}` |
|        - | 1601 | `	/* Restore token stream */` |
|       41 | 1602 | `	pGen->pEnd = pTmp;` |
|       41 | 1603 | `	if( nExpr > 0 ){` |
|        - | 1604 | `		/* Emit the uplink instruction */` |
|       41 | 1605 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|       18 | 1606 | `	}` |
|       41 | 1607 | `	return SXRET_OK;` |
|       23 | 1608 | `}` |
|        - | 1609 | `/*` |
|        - | 1610 | ` * Compile the return statement.` |
|        - | 1611 | ` * According to the PHP language reference` |
|        - | 1612 | ` *  If called from within a function, the return() statement immediately ends execution` |
|        - | 1613 | ` *  of the current function, and returns its argument as the value of the function call.` |
|        - | 1614 | ` *  return() will also end the execution of an eval() statement or script file.` |
|        - | 1615 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|        - | 1616 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|        - | 1617 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|        - | 1618 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|        - | 1619 | ` *  from within the main script file, then script execution end.` |
|        - | 1620 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|        - | 1621 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|        - | 1622 | ` *  should do so as PHP has less work to do in this case.` |
|        - | 1623 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|        - | 1624 | ` */` |
|  3626082 | 1625 | `PH7_PRIVATE sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|        5 | 1626 | `{` |
|  3626087 | 1627 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|        - | 1628 | `	sxi32 rc;` |
|  3626087 | 1629 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  3626087 | 1630 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|        - | 1631 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|        - | 1632 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|        - | 1633 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|        - | 1634 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|        - | 1635 | `	 * normally below so token processing stays consistent. */` |
|  9546911 | 1636 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|  5920829 | 1637 | `		pFuncBlock = pFuncBlock->pParent;` |
|        5 | 1638 | `	}` |
|  3626082 | 1639 | `	if( pFuncBlock && pFuncBlock->pUserData` |
|  3626071 | 1640 | `	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){` |
|        3 | 1641 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1642 | `			"A never-returning function must not return");` |
|        3 | 1643 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1644 | `			return SXERR_ABORT;` |
|        - | 1645 | `		}` |
|        1 | 1646 | `	}` |
|        - | 1647 | `	/* Jump the 'return' keyword */` |
|  3626087 | 1648 | `	pGen->pIn++;` |
|  3626087 | 1649 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - | 1650 | `		/* Compile the expression */` |
|  3528875 | 1651 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  3528875 | 1652 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1653 | `			return SXERR_ABORT;` |
|  3528875 | 1654 | `		}else if(rc != SXERR_EMPTY ){` |
|  3528875 | 1655 | `			nRet = 1;` |
|  1764435 | 1656 | `		}` |
|  1764435 | 1657 | `	}` |
|        - | 1658 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|        - | 1659 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|        - | 1660 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|        - | 1661 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|  3626087 | 1662 | `	if( pGen->bInGenerator ){` |
|     3921 | 1663 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|     3921 | 1664 | `		return SXRET_OK;` |
|        - | 1665 | `	}` |
|        - | 1666 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|        - | 1667 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|        - | 1668 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|        - | 1669 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|        - | 1670 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|  3622171 | 1671 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|  3622171 | 1672 | `	return SXRET_OK;` |
|  1813046 | 1673 | `}` |
|        - | 1674 | `/*` |
|        - | 1675 | ` * Compile a yield expression.` |
|        - | 1676 | ` * Called from the expression code generator when a yield node is encountered.` |
|        - | 1677 | ` * Handles: yield, yield $value, yield $key => $value` |
|        - | 1678 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|        - | 1679 | ` */` |
|    15940 | 1680 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|        5 | 1681 | `{` |
|        - | 1682 | `	SyToken *pTmp, *pSplit;` |
|    15945 | 1683 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|    15945 | 1684 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|        - | 1685 | `	sxi32 rc;` |
|     7970 | 1686 | `	(void)iCompileFlag;` |
|        - | 1687 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|    15945 | 1688 | `	pGen->pIn++;` |
|        - | 1689 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|        - | 1690 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|        - | 1691 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|        - | 1692 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|        - | 1693 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|    15940 | 1694 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|     8005 | 1695 | `		&& pGen->pIn->sData.nByte == 4` |
|       72 | 1696 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|       67 | 1697 | `		pGen->pIn++; /* Skip 'from' */` |
|       67 | 1698 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       67 | 1699 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1700 | `			return SXERR_ABORT;` |
|        - | 1701 | `		}` |
|       67 | 1702 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 | 1703 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|      ! 0 | 1704 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|        - | 1705 | `				"Missing expression after 'yield from'");` |
|      ! 0 | 1706 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1707 | `				return SXERR_ABORT;` |
|        - | 1708 | `			}` |
|      ! 0 | 1709 | `		}` |
|       67 | 1710 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|       67 | 1711 | `		return SXRET_OK;` |
|        - | 1712 | `	}` |
|    15883 | 1713 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - | 1714 | `		/* Bare yield — no value */` |
|        3 | 1715 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|        3 | 1716 | `		return SXRET_OK;` |
|        - | 1717 | `	}` |
|        - | 1718 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|    15881 | 1719 | `	pSplit = 0;` |
|        - | 1720 | `	{` |
|    15881 | 1721 | `		SyToken *pCur = pGen->pIn;` |
|    15881 | 1722 | `		sxi32 nNest = 0;` |
|    47447 | 1723 | `		while( pCur < pGen->pEnd ){` |
|    47137 | 1724 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       18 | 1725 | `				nNest++;` |
|    47129 | 1726 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       18 | 1727 | `				nNest--;` |
|    47113 | 1728 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|    15571 | 1729 | `				pSplit = pCur;` |
|    15571 | 1730 | `				break;` |
|        - | 1731 | `			}` |
|    31571 | 1732 | `			pCur++;` |
|        5 | 1733 | `		}` |
|        - | 1734 | `	}` |
|    15881 | 1735 | `	pTmp = pGen->pEnd;` |
|    15881 | 1736 | `	if( pSplit ){` |
|        - | 1737 | `		/* yield $key => $value */` |
|    15571 | 1738 | `		pGen->pEnd = pSplit;` |
|    15571 | 1739 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|    15571 | 1740 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|    15571 | 1741 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|    15571 | 1742 | `		pGen->pEnd = pTmp;` |
|    15571 | 1743 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|    15571 | 1744 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|    15571 | 1745 | `		iP1 = 1;` |
|    15571 | 1746 | `		iP2 = 1;` |
|     7788 | 1747 | `	}else{` |
|        - | 1748 | `		/* yield $value */` |
|      315 | 1749 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      315 | 1750 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      315 | 1751 | `		if( rc != SXERR_EMPTY ){` |
|      315 | 1752 | `			iP1 = 1;` |
|      155 | 1753 | `		}` |
|        - | 1754 | `	}` |
|    15881 | 1755 | `	pGen->pEnd = pTmp;` |
|    15881 | 1756 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|    15881 | 1757 | `	return SXRET_OK;` |
|     7975 | 1758 | `}` |
|        - | 1759 | `/*` |
|        - | 1760 | ` * Compile the die/exit language construct.` |
|        - | 1761 | ` * The role of these constructs is to terminate execution of the script.` |
|        - | 1762 | ` * Shutdown functions will always be executed even if exit() is called.` |
|        - | 1763 | ` */` |
|       94 | 1764 | `PH7_PRIVATE sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|        5 | 1765 | `{` |
|       99 | 1766 | `	sxi32 nExpr = 0;` |
|        - | 1767 | `	sxi32 rc;` |
|        - | 1768 | `	/* Jump the die/exit keyword */` |
|       99 | 1769 | `	pGen->pIn++;` |
|       99 | 1770 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - | 1771 | `		/* Compile the expression */` |
|       99 | 1772 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       99 | 1773 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1774 | `			return SXERR_ABORT;` |
|       99 | 1775 | `		}else if(rc != SXERR_EMPTY ){` |
|       99 | 1776 | `			nExpr = 1;` |
|       47 | 1777 | `		}` |
|       47 | 1778 | `	}` |
|        - | 1779 | `	/* Emit the HALT instruction */` |
|       99 | 1780 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|       99 | 1781 | `	return SXRET_OK;` |
|       52 | 1782 | `}` |
|        - | 1783 | `/*` |
|        - | 1784 | ` * Compile the 'echo' language construct.` |
|        - | 1785 | ` */` |
|    17540 | 1786 | `PH7_PRIVATE sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|        5 | 1787 | `{` |
|    17545 | 1788 | `	SyToken *pTmp,*pNext = 0;` |
|    17545 | 1789 | `	sxu32 nLine = pGen->pIn->nLine;` |
|    17545 | 1790 | `	int nExpr = 0;      /* expressions actually compiled */` |
|    17545 | 1791 | `	int bExpectMore = 1;/* after 'echo' or a comma an expression is REQUIRED */` |
|        - | 1792 | `	sxi32 rc;` |
|        - | 1793 | `	/* Jump the 'echo' keyword */` |
|    17545 | 1794 | `	pGen->pIn++;` |
|        - | 1795 | `	/* Compile arguments one after one */` |
|    17545 | 1796 | `	pTmp = pGen->pEnd;` |
|    45441 | 1797 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|    27907 | 1798 | `		if( pGen->pIn < pNext ){` |
|    27907 | 1799 | `			pGen->pEnd = pNext;` |
|    27907 | 1800 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|    27907 | 1801 | `			if( rc == SXERR_ABORT ){` |
|        6 | 1802 | `				return SXERR_ABORT;` |
|    27903 | 1803 | `			}else if( rc != SXERR_EMPTY ){` |
|        - | 1804 | `				/* Emit the consume instruction */` |
|    27879 | 1805 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    27879 | 1806 | `				nExpr++;` |
|    27879 | 1807 | `				bExpectMore = 0;` |
|    13937 | 1808 | `			}` |
|    13949 | 1809 | `		}` |
|        - | 1810 | `		/* Jump trailing commas (php: exactly one between expressions; a` |
|        - | 1811 | `		 * dangling or doubled comma is a parse error, enforced below) */` |
|    38271 | 1812 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|    10375 | 1813 | `			if( bExpectMore ){` |
|        - | 1814 | `				/* two commas in a row */` |
|        3 | 1815 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,pNext->nLine,` |
|        - | 1816 | `					"syntax error, unexpected token \",\"");` |
|        3 | 1817 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1818 | `			}` |
|    10373 | 1819 | `			bExpectMore = 1;` |
|    10373 | 1820 | `			pNext++;` |
|        5 | 1821 | `		}` |
|    27901 | 1822 | `		pGen->pIn = pNext;` |
|        5 | 1823 | `	}` |
|        - | 1824 | `	/* Restore token stream */` |
|    17539 | 1825 | `	pGen->pEnd = pTmp;` |
|    17539 | 1826 | `	if( nExpr == 0 \|\| bExpectMore ){` |
|        - | 1827 | ``		/* `echo ;` or `echo expr, ;` — php rejects both */`` |
|       32 | 1828 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - | 1829 | `			"syntax error, unexpected token \";\"");` |
|       32 | 1830 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1831 | `	}` |
|    17511 | 1832 | `	return SXRET_OK;` |
|     8775 | 1833 | `}` |
|        - | 1834 | `/*` |
|        - | 1835 | ` * Compile the static statement.` |
|        - | 1836 | ` * According to the PHP language reference` |
|        - | 1837 | ` *  Another important feature of variable scoping is the static variable.` |
|        - | 1838 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|        - | 1839 | ` *  when program execution leaves this scope.` |
|        - | 1840 | ` *  Static variables also provide one way to deal with recursive functions.` |
|        - | 1841 | ` * Symisc eXtension.` |
|        - | 1842 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|        - | 1843 | ` *  the zend engine would allow only simple scalar value.` |
|        - | 1844 | ` *  Example` |
|        - | 1845 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|        - | 1846 | ` *    Refer to the official documentation for more information on this feature.` |
|        - | 1847 | ` */` |
|    11676 | 1848 | `PH7_PRIVATE sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|        5 | 1849 | `{` |
|        - | 1850 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|        - | 1851 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|        - | 1852 | `	GenBlock *pBlock;` |
|        - | 1853 | `	SyString *pName;` |
|        - | 1854 | `	char *zDup;` |
|        - | 1855 | `	sxu32 nLine;` |
|        - | 1856 | `	sxi32 rc;` |
|        - | 1857 | ``	/* `static function () {}` / `static fn () =>` at statement position is an`` |
|        - | 1858 | `	 * EXPRESSION statement (a bare static closure), not a static-variable` |
|        - | 1859 | `	 * declaration — hand it to the expression compiler (php accepts it). */` |
|    11676 | 1860 | `	if( &pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & PH7_TK_KEYWORD)` |
|     5844 | 1861 | `	 && (SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FUNCTION` |
|        1 | 1862 | `	  \|\| SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FN) ){` |
|        3 | 1863 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        3 | 1864 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1865 | `			return SXERR_ABORT;` |
|        3 | 1866 | `		}else if( rc != SXERR_EMPTY ){` |
|        3 | 1867 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        1 | 1868 | `		}` |
|        3 | 1869 | `		return SXRET_OK;` |
|        - | 1870 | `	}` |
|        - | 1871 | `	/* Jump the static keyword */` |
|    11679 | 1872 | `	nLine = pGen->pIn->nLine;` |
|    11679 | 1873 | `	pGen->pIn++;` |
|        - | 1874 | `	/* Extract the enclosing function if any */` |
|    11679 | 1875 | `	pBlock = pGen->pCurrent;` |
|    23353 | 1876 | `	while( pBlock ){` |
|    23353 | 1877 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|    11679 | 1878 | `			break;` |
|        - | 1879 | `		}` |
|        - | 1880 | `		/* Point to the upper block */` |
|    11679 | 1881 | `		pBlock = pBlock->pParent;` |
|        5 | 1882 | `	}` |
|    11679 | 1883 | `	if( pBlock == 0 ){` |
|        - | 1884 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|      ! 0 | 1885 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 | 1886 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|      ! 0 | 1887 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1888 | `				return SXERR_ABORT;` |
|        - | 1889 | `			}` |
|      ! 0 | 1890 | `			goto Synchronize;` |
|        - | 1891 | `		}` |
|        - | 1892 | `		/* Compile the expression holding the variable */` |
|      ! 0 | 1893 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      ! 0 | 1894 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1895 | `			return SXERR_ABORT;` |
|      ! 0 | 1896 | `		}else if( rc != SXERR_EMPTY ){` |
|        - | 1897 | `			/* Emit the POP instruction */` |
|      ! 0 | 1898 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      ! 0 | 1899 | `		}` |
|      ! 0 | 1900 | `		return SXRET_OK;` |
|        - | 1901 | `	}` |
|    11679 | 1902 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        - | 1903 | `	/* Make sure we are dealing with a valid statement */` |
|    11679 | 1904 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|    11672 | 1905 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        3 | 1906 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|        3 | 1907 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1908 | `				return SXERR_ABORT;` |
|        - | 1909 | `			}` |
|        3 | 1910 | `			goto Synchronize;` |
|        - | 1911 | `	}` |
|    11677 | 1912 | `	pGen->pIn++;` |
|        - | 1913 | `	/* Extract variable name */` |
|    11677 | 1914 | `	pName = &pGen->pIn->sData;` |
|    11677 | 1915 | `	pGen->pIn++; /* Jump the var name */` |
|    11677 | 1916 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|      ! 0 | 1917 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 1918 | `		goto Synchronize;` |
|        - | 1919 | `	}` |
|        - | 1920 | `	/* Initialize the structure describing the static variable */` |
|    11677 | 1921 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|    11677 | 1922 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|        - | 1923 | `	/* Duplicate variable name */` |
|    11677 | 1924 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    11677 | 1925 | `	if( zDup == 0 ){` |
|      ! 0 | 1926 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1927 | `		return SXERR_ABORT;` |
|        - | 1928 | `	}` |
|    11677 | 1929 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|        - | 1930 | `	/* Check if we have an expression to compile */` |
|    11677 | 1931 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|        - | 1932 | `		SySet *pInstrContainer;` |
|        - | 1933 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|        - | 1934 | `		 * Static variable can take any complex expression including function` |
|        - | 1935 | `		 * call as their initialization value.` |
|        - | 1936 | `		 * Example:` |
|        - | 1937 | `		 *		static $var = foo(1,4+5,bar());` |
|        - | 1938 | `		 */` |
|    11677 | 1939 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|        - | 1940 | `		/* Swap bytecode container */` |
|    11677 | 1941 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    11677 | 1942 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|        - | 1943 | `		/* Compile the expression */` |
|    11677 | 1944 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 1945 | `		/* Emit the done instruction */` |
|    11677 | 1946 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        - | 1947 | `		/* Restore default bytecode container */` |
|    11677 | 1948 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     5836 | 1949 | `	}` |
|        - | 1950 | `	/* Finally save the compiled static variable in the appropriate container */` |
|    11677 | 1951 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|    11677 | 1952 | `	return SXRET_OK;` |
|        1 | 1953 | `Synchronize:` |
|        - | 1954 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|        - | 1955 | `	 * statement.` |
|        - | 1956 | `	 */` |
|        5 | 1957 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|        3 | 1958 | `		pGen->pIn++;` |
|        1 | 1959 | `	}` |
|        3 | 1960 | `	return SXRET_OK;` |
|     5843 | 1961 | `}` |
|        - | 1962 | `/*` |
|        - | 1963 | ` * Compile the var statement.` |
|        - | 1964 | ` * Symisc Extension:` |
|        - | 1965 | ` *      var statement can be used outside of a class definition.` |
|        - | 1966 | ` */` |
|        2 | 1967 | `PH7_PRIVATE sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|        1 | 1968 | `{` |
|        - | 1969 | `` 	/* `var` is a PROPERTY modifier and nothing else. A class body's `var $x;` `` |
|        - | 1970 | `	 * is compiled by compile_class.c, never here, so reaching this statement` |
|        - | 1971 | ``	 * handler means `var` appeared outside a class — which php rejects as a`` |
|        - | 1972 | `	 * parse error. PHL used to compile it as an ordinary expression statement,` |
|        - | 1973 | ``	 * so top-level `var $x = 1;` silently ran (a Symisc extension). */`` |
|        3 | 1974 | `	PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|        3 | 1975 | `	return SXERR_ABORT;` |
|        1 | 1976 | `}` |
|        - | 1977 | `/*` |
|        - | 1978 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|        - | 1979 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|        - | 1980 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|        - | 1981 | ` */` |
|        - | 1982 | `/*` |
|        - | 1983 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|        - | 1984 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|        - | 1985 | ` * hash and any shared references), this creates a new literal entry with the` |
|        - | 1986 | ` * qualified name and updates the instruction's operand index.` |
|        - | 1987 | ` *` |
|        - | 1988 | ` * Resolution order:` |
|        - | 1989 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|        - | 1990 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|        - | 1991 | ` *   3. Otherwise return the original literal index unchanged.` |
|        - | 1992 | ` *` |
|        - | 1993 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|        - | 1994 | ` * came from an import (step 1) and 0 otherwise.` |
|        - | 1995 | ` * Returns the (possibly new) literal index.` |
|        - | 1996 | ` */` |
|  6445308 | 1997 | `PH7_PRIVATE sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|        5 | 1998 | `{` |
|        - | 1999 | `	ph7_value *pLit;` |
|        - | 2000 | `	const char *zLit;` |
|        - | 2001 | `	SyString sQualified;` |
|        - | 2002 | `	sxu32 nLit;` |
|        - | 2003 | `	sxu32 k;` |
|        - | 2004 | `	sxu32 nNewIdx;` |
|        - | 2005 | `	int hasNsSep;` |
|        - | 2006 | `	SyHashEntry *pImport;` |
|        - | 2007 | `	ph7_value *pNew;` |
|  6445313 | 2008 | `	if( pFromImport ){` |
|  5255237 | 2009 | `		*pFromImport = 0;` |
|  2627616 | 2010 | `	}` |
|  6445313 | 2011 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  6445313 | 2012 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|      ! 0 | 2013 | `		return nOrigIdx;` |
|        - | 2014 | `	}` |
|  6445313 | 2015 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  6445313 | 2016 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|        - | 2017 | `	/* Skip if already qualified (contains backslash) */` |
|  6445313 | 2018 | `	hasNsSep = 0;` |
| 77936469 | 2019 | `	for( k = 0; k < nLit; k++ ){` |
| 71491187 | 2020 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 35745583 | 2021 | `	}` |
|  6445313 | 2022 | `	if( hasNsSep ){` |
|       28 | 2023 | `		return nOrigIdx;` |
|        - | 2024 | `	}` |
|        - | 2025 | `	/* Check use imports first (works even outside namespaces) */` |
|  6445287 | 2026 | `	SyBlobReset(&pGen->sWorker);` |
|  6445287 | 2027 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  6445287 | 2028 | `	if( pImport ){` |
|       41 | 2029 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       41 | 2030 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|       41 | 2031 | `		if( pFromImport ){` |
|       18 | 2032 | `			*pFromImport = 1;` |
|        8 | 2033 | `		}` |
|       23 | 2034 | `	}else{` |
|  6445251 | 2035 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  6445113 | 2036 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|        - | 2037 | `		}` |
|        - | 2038 | `		/* Prepend current namespace */` |
|      143 | 2039 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      143 | 2040 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      143 | 2041 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|        - | 2042 | `	}` |
|        - | 2043 | `	/* Look up or create a new literal for the qualified name */` |
|      179 | 2044 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|      179 | 2045 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|       79 | 2046 | `		return nNewIdx; /* Already interned */` |
|        - | 2047 | `	}` |
|      105 | 2048 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      105 | 2049 | `	if( pNew == 0 ){` |
|      ! 0 | 2050 | `		return nOrigIdx; /* OOM, fall back to original */` |
|        - | 2051 | `	}` |
|      105 | 2052 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      105 | 2053 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      105 | 2054 | `	return nNewIdx;` |
|  3222659 | 2055 | `}` |
|        - | 2056 | `/*` |
|        - | 2057 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|        - | 2058 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|        - | 2059 | ` */` |
|   554032 | 2060 | `PH7_PRIVATE void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|        5 | 2061 | `{` |
|        - | 2062 | `	SyHashEntry *pImport;` |
|   554037 | 2063 | `	const char *zName = pName->zString;` |
|   554037 | 2064 | `	sxu32 nName = pName->nByte;` |
|   554037 | 2065 | `	sxu32 nFirst = 0;` |
|        - | 2066 | `	/* php resolves a name through use-imports on its LEADING segment: an` |
|        - | 2067 | ``	 * unqualified `C` maps the whole name (`use X\C;` -> X\C), while a QUALIFIED`` |
|        - | 2068 | ``	 * `A\B\C` maps just `A` (`use X\A;` -> X\A\B\C) and keeps the `\B\C` tail.`` |
|        - | 2069 | `	 * The old code looked up the whole qualified string (which never matches a` |
|        - | 2070 | `	 * single-segment import alias) and then blindly prefixed the current` |
|        - | 2071 | ``	 * namespace, so `use PHPUnit\Framework; extends Framework\TestCase` resolved`` |
|        - | 2072 | `	 * to Ns\Framework\TestCase. Mirror GenStateResolveNamespaceLiteral here. */` |
|  7045315 | 2073 | `	while( nFirst < nName && zName[nFirst] != '\\' ){ nFirst++; }` |
|   554037 | 2074 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)zName,nFirst);` |
|   554037 | 2075 | `	if( pImport ){` |
|       26 | 2076 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       26 | 2077 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|       26 | 2078 | `		SyBlobAppend(pOut,zName + nFirst,nName - nFirst); /* the \B\C tail, if any */` |
|       26 | 2079 | `		return;` |
|        - | 2080 | `	}` |
|        - | 2081 | `	/* Prepend current namespace if active */` |
|   554015 | 2082 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       14 | 2083 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       14 | 2084 | `		SyBlobAppend(pOut,"\\",1);` |
|        6 | 2085 | `	}` |
|   554015 | 2086 | `	SyBlobAppend(pOut,zName,nName);` |
|   277021 | 2087 | `}` |
|        - | 2088 | `/*` |
|        - | 2089 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|        - | 2090 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|        - | 2091 | ` * The caller must release pOut when done.` |
|        - | 2092 | ` */` |
|   530986 | 2093 | `PH7_PRIVATE void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|        5 | 2094 | `{` |
|   530991 | 2095 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     3981 | 2096 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     3981 | 2097 | `		SyBlobAppend(pOut,"\\",1);` |
|     1988 | 2098 | `	}` |
|   530991 | 2099 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   530991 | 2100 | `}` |
|        - | 2101 | `/*` |
|        - | 2102 | ` * Compile a namespace statement` |
|        - | 2103 | ` * According to the PHP language reference manual` |
|        - | 2104 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|        - | 2105 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|        - | 2106 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|        - | 2107 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|        - | 2108 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|        - | 2109 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|        - | 2110 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|        - | 2111 | ` *  programming world.` |
|        - | 2112 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|        - | 2113 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|        - | 2114 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|        - | 2115 | ` *  classes/functions/constants.` |
|        - | 2116 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|        - | 2117 | ` *  readability of source code.` |
|        - | 2118 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|        - | 2119 | ` *  Here is an example of namespace syntax in PHP:` |
|        - | 2120 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|        - | 2121 | ` *       class MyClass {}` |
|        - | 2122 | ` *       function myfunction() {}` |
|        - | 2123 | ` *       const MYCONST = 1;` |
|        - | 2124 | ` *       $a = new MyClass;` |
|        - | 2125 | ` *       $c = new \my\name\MyClass;` |
|        - | 2126 | ` *       $a = strlen('hi');` |
|        - | 2127 | ` *       $d = namespace\MYCONST;` |
|        - | 2128 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|        - | 2129 | ` *       echo constant($d);` |
|        - | 2130 | ` * NOTE` |
|        - | 2131 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|        - | 2132 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|        - | 2133 | ` */` |
|        - | 2134 | `/*` |
|        - | 2135 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|        - | 2136 | ` */` |
|       14 | 2137 | `PH7_PRIVATE const char * TokenTypeName(sxu32 nType)` |
|        4 | 2138 | `{` |
|       18 | 2139 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|       11 | 2140 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|       11 | 2141 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|       11 | 2142 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|       11 | 2143 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|       11 | 2144 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|        3 | 2145 | `	return "token";` |
|       11 | 2146 | `}` |
|     4020 | 2147 | `PH7_PRIVATE sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|        5 | 2148 | `{` |
|        - | 2149 | `	sxu32 nLine;` |
|        - | 2150 | `	sxi32 rc;` |
|     4025 | 2151 | `	nLine = pGen->pIn->nLine;` |
|     4025 | 2152 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|        - | 2153 | `	/* Reset namespace and clear previous use imports */` |
|     4025 | 2154 | `	SyBlobReset(&pGen->sNamespace);` |
|     4025 | 2155 | `	SyHashRelease(&pGen->hUseImports);` |
|     4025 | 2156 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|     4025 | 2157 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     4025 | 2158 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|     4025 | 2159 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     4025 | 2160 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|     4025 | 2161 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - | 2162 | `		/* Global namespace (bare "namespace;") */` |
|      ! 0 | 2163 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|      ! 0 | 2164 | `		return SXRET_OK;` |
|        - | 2165 | `	}` |
|     4025 | 2166 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|        - | 2167 | `		/* namespace; — switch to global namespace */` |
|      ! 0 | 2168 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|      ! 0 | 2169 | `		return SXRET_OK;` |
|        - | 2170 | `	}` |
|     4025 | 2171 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|        - | 2172 | `		/* namespace { } — global namespace block */` |
|        5 | 2173 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|        5 | 2174 | `		return SXRET_OK;` |
|        - | 2175 | `	}` |
|        - | 2176 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     8119 | 2177 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     4103 | 2178 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|        - | 2179 | `			/* Append backslash separator */` |
|       46 | 2180 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       46 | 2181 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|       21 | 2182 | `			}` |
|       25 | 2183 | `		}else{` |
|        - | 2184 | `			/* Append identifier */` |
|     4061 | 2185 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        - | 2186 | `		}` |
|     4103 | 2187 | `		pGen->pIn++;` |
|        5 | 2188 | `	}` |
|        - | 2189 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|        - | 2190 | `	 * at the correct program counter, not just the last one compiled. */` |
|        - | 2191 | `	{` |
|     4021 | 2192 | `		char *zNsDup = 0;` |
|     4021 | 2193 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     6026 | 2194 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     4014 | 2195 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     2007 | 2196 | `		}` |
|     4021 | 2197 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|        - | 2198 | `	}` |
|     4021 | 2199 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        8 | 2200 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - | 2201 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|        4 | 2202 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        6 | 2203 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2204 | `			return SXERR_ABORT;` |
|        - | 2205 | `		}` |
|        2 | 2206 | `	}` |
|     4021 | 2207 | `	return SXRET_OK;` |
|     2015 | 2208 | `}` |
|        - | 2209 | `/*` |
|        - | 2210 | ` * Compile the 'use' statement` |
|        - | 2211 | ` * According to the PHP language reference manual` |
|        - | 2212 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|        - | 2213 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|        - | 2214 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|        - | 2215 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|        - | 2216 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|        - | 2217 | ` *  a function or constant is not supported.` |
|        - | 2218 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|        - | 2219 | ` * NOTE` |
|        - | 2220 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|        - | 2221 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|        - | 2222 | ` */` |
|       80 | 2223 | `PH7_PRIVATE sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|        5 | 2224 | `{` |
|        - | 2225 | `	sxu32 nLine;` |
|        - | 2226 | `	sxi32 rc;` |
|        - | 2227 | `	SyBlob sPath;` |
|        - | 2228 | `	SyString sAlias;` |
|        - | 2229 | `	SyToken *pLast;` |
|        - | 2230 | `	char *zDup;` |
|        - | 2231 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|        - | 2232 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|        - | 2233 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|       85 | 2234 | `	nLine = pGen->pIn->nLine;` |
|       85 | 2235 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|        - | 2236 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|       85 | 2237 | `	iUseType = 0;` |
|       85 | 2238 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       30 | 2239 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|       30 | 2240 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|       16 | 2241 | `			iUseType = 1;` |
|       16 | 2242 | `			pGen->pIn++;` |
|       23 | 2243 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|       16 | 2244 | `			iUseType = 2;` |
|       16 | 2245 | `			pGen->pIn++;` |
|        7 | 2246 | `		}` |
|       14 | 2247 | `	}` |
|        - | 2248 | `	/* Select target hash tables based on import type */` |
|       85 | 2249 | `	switch( iUseType ){` |
|        7 | 2250 | `		case 1:` |
|       16 | 2251 | `			pGenHash = &pGen->hUseFuncImports;` |
|       16 | 2252 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|       16 | 2253 | `			break;` |
|        7 | 2254 | `		case 2:` |
|       16 | 2255 | `			pGenHash = &pGen->hUseConstImports;` |
|       16 | 2256 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|       16 | 2257 | `			break;` |
|       26 | 2258 | `		default:` |
|       57 | 2259 | `			pGenHash = &pGen->hUseImports;` |
|       57 | 2260 | `			pVmHash = &pGen->pVm->hUseImports;` |
|       52 | 2261 | `			break;` |
|        - | 2262 | `	}` |
|       85 | 2263 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|        - | 2264 | `	/* Process one or more use declarations separated by commas */` |
|       41 | 2265 | `	for(;;){` |
|       87 | 2266 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 2267 | `			break;` |
|        - | 2268 | `		}` |
|       87 | 2269 | `		SyBlobReset(&sPath);` |
|       87 | 2270 | `		pLast = 0;` |
|        - | 2271 | `		/* Collect the full namespace path */` |
|      301 | 2272 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|      219 | 2273 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|      151 | 2274 | `				pLast = pGen->pIn;` |
|      151 | 2275 | `				if( SyBlobLength(&sPath) > 0 ){` |
|       73 | 2276 | `					SyBlobAppend(&sPath,"\\",1);` |
|       34 | 2277 | `				}` |
|      151 | 2278 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       73 | 2279 | `			}` |
|      219 | 2280 | `			pGen->pIn++;` |
|        5 | 2281 | `		}` |
|       87 | 2282 | `		if( pLast == 0 ){` |
|        - | 2283 | `			/* Empty path */` |
|        5 | 2284 | `			break;` |
|        - | 2285 | `		}` |
|        - | 2286 | `		/* Default alias is the last component of the path */` |
|       83 | 2287 | `		sAlias = pLast->sData;` |
|        - | 2288 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|       78 | 2289 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       56 | 2290 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|       27 | 2291 | `			pGen->pIn++; /* Jump 'as' */` |
|       27 | 2292 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|       27 | 2293 | `				sAlias = pGen->pIn->sData;` |
|       27 | 2294 | `				pGen->pIn++;` |
|       12 | 2295 | `			}` |
|       12 | 2296 | `		}` |
|        - | 2297 | `		/* Check for duplicate import alias (per-type) */` |
|       83 | 2298 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|        8 | 2299 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 2300 | `				"Cannot use %.*s as %z because the name is already in use",` |
|        4 | 2301 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|        6 | 2302 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2303 | `				SyBlobRelease(&sPath);` |
|      ! 0 | 2304 | `				return SXERR_ABORT;` |
|        - | 2305 | `			}` |
|        2 | 2306 | `		}` |
|        - | 2307 | `		/* Register the import: alias -> FQN.` |
|        - | 2308 | `		 * Strings are allocated from the VM pool allocator and freed` |
|        - | 2309 | `		 * when the entire VM is released. SyHashRelease does not free` |
|        - | 2310 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|      122 | 2311 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       78 | 2312 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|       83 | 2313 | `		if( zDup ){` |
|       83 | 2314 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|       83 | 2315 | `			if( pVmHash ){` |
|        - | 2316 | `				/* Class imports: populate VM table directly (class resolution` |
|        - | 2317 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|       55 | 2318 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       55 | 2319 | `				if( zAliasDup ){` |
|       55 | 2320 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|       25 | 2321 | `				}` |
|       25 | 2322 | `			}` |
|       83 | 2323 | `			if( iUseType == 2 ){` |
|        - | 2324 | `				/* Const imports: emit a runtime instruction so imports are` |
|        - | 2325 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|       16 | 2326 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       16 | 2327 | `				if( zAliasDup ){` |
|        - | 2328 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|        - | 2329 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|        - | 2330 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|       16 | 2331 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|       16 | 2332 | `					if( azPair ){` |
|       16 | 2333 | `						azPair[0] = zAliasDup;` |
|       16 | 2334 | `						azPair[1] = zDup;` |
|       16 | 2335 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|        7 | 2336 | `					}` |
|        7 | 2337 | `				}` |
|        7 | 2338 | `			}` |
|       39 | 2339 | `		}` |
|        - | 2340 | `		/* Check for comma (multiple use declarations) */` |
|       83 | 2341 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|        3 | 2342 | `			pGen->pIn++;` |
|        2 | 2343 | `		}else{` |
|       43 | 2344 | `			break;` |
|        - | 2345 | `		}` |
|        1 | 2346 | `	}` |
|       85 | 2347 | `	SyBlobRelease(&sPath);` |
|       85 | 2348 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        4 | 2349 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|        2 | 2350 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        3 | 2351 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2352 | `			return SXERR_ABORT;` |
|        - | 2353 | `		}` |
|        1 | 2354 | `	}` |
|       85 | 2355 | `	return SXRET_OK;` |
|       45 | 2356 | `}` |
|        - | 2357 | `/*` |
|        - | 2358 | ` * Compile the stupid 'declare' language construct.` |
|        - | 2359 | ` *` |
|        - | 2360 | ` * According to the PHP language reference manual.` |
|        - | 2361 | ` *  The declare construct is used to set execution directives for a block of code.` |
|        - | 2362 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|        - | 2363 | ` *  declare (directive)` |
|        - | 2364 | ` *   statement` |
|        - | 2365 | ` * The directive section allows the behavior of the declare block to be set.` |
|        - | 2366 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|        - | 2367 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|        - | 2368 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|        - | 2369 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|        - | 2370 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|        - | 2371 | ` * <?php` |
|        - | 2372 | ` * // these are the same:` |
|        - | 2373 | ` * // you can use this:` |
|        - | 2374 | ` * declare(ticks=1) {` |
|        - | 2375 | ` *   // entire script here` |
|        - | 2376 | ` * }` |
|        - | 2377 | ` * // or you can use this:` |
|        - | 2378 | ` * declare(ticks=1);` |
|        - | 2379 | ` * // entire script here` |
|        - | 2380 | ` * ?>` |
|        - | 2381 | ` *` |
|        - | 2382 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|        - | 2383 | ` */` |
|        - | 2384 | `/*` |
|        - | 2385 | ` * Match a directive name against a known literal (case-insensitive).` |
|        - | 2386 | ` */` |
|       80 | 2387 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|        5 | 2388 | `{` |
|      120 | 2389 | `	return SyStringLength(pName) == nWant` |
|       80 | 2390 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|        5 | 2391 | `}` |
|        - | 2392 |  |
|       44 | 2393 | `PH7_PRIVATE sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|        5 | 2394 | `{` |
|       49 | 2395 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       49 | 2396 | `	SyToken *pBodyEnd = 0;` |
|        - | 2397 | `	SyToken *pBodyStart;` |
|        - | 2398 | `	SyToken *pCursor;` |
|        - | 2399 | `	int bHasStrictTypes;` |
|        - | 2400 | `	int bBlockForm;` |
|        - | 2401 | `	int bPlacementOk;` |
|        - | 2402 | `	sxi32 rc;` |
|       49 | 2403 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|       49 | 2404 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|        5 | 2405 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|        5 | 2406 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2407 | `			return SXERR_ABORT;` |
|        - | 2408 | `		}` |
|        5 | 2409 | `		goto Synchro;` |
|        - | 2410 | `	}` |
|       45 | 2411 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|       45 | 2412 | `	pBodyStart = pGen->pIn;` |
|        - | 2413 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|       45 | 2414 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|       45 | 2415 | `	if( pBodyEnd >= pGen->pEnd ){` |
|      ! 0 | 2416 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|      ! 0 | 2417 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2418 | `			return SXERR_ABORT;` |
|        - | 2419 | `		}` |
|      ! 0 | 2420 | `		return SXRET_OK;` |
|        - | 2421 | `	}` |
|        - | 2422 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|        - | 2423 | `	 * now delimits the comma-separated directive list. */` |
|       45 | 2424 | `	pGen->pIn = &pBodyEnd[1];` |
|       45 | 2425 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      ! 0 | 2426 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|      ! 0 | 2427 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2428 | `			return SXERR_ABORT;` |
|        - | 2429 | `		}` |
|      ! 0 | 2430 | `	}` |
|       45 | 2431 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|       45 | 2432 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|       45 | 2433 | `	bHasStrictTypes = 0;` |
|        - | 2434 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|        - | 2435 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|        - | 2436 | `	 * directive appears anywhere in the list, before validating values. */` |
|       45 | 2437 | `	pCursor = pBodyStart;` |
|       57 | 2438 | `	while( pCursor < pBodyEnd ){` |
|       53 | 2439 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|       45 | 2440 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|       41 | 2441 | `				bHasStrictTypes = 1;` |
|       41 | 2442 | `				break;` |
|        - | 2443 | `			}` |
|        2 | 2444 | `		}` |
|       14 | 2445 | `		pCursor++;` |
|        2 | 2446 | `	}` |
|       45 | 2447 | `	if( bHasStrictTypes && bBlockForm ){` |
|        3 | 2448 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2449 | `			"strict_types declaration must not use block mode");` |
|        3 | 2450 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        3 | 2451 | `		return SXRET_OK;` |
|        - | 2452 | `	}` |
|       43 | 2453 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|        6 | 2454 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2455 | `			"strict_types declaration must be the very first statement in the script");` |
|        6 | 2456 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        6 | 2457 | `		return SXRET_OK;` |
|        - | 2458 | `	}` |
|        - | 2459 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|       39 | 2460 | `	pCursor = pBodyStart;` |
|       73 | 2461 | `	while( pCursor < pBodyEnd ){` |
|        - | 2462 | `		SyToken *pNameTok;` |
|        - | 2463 | `		SyToken *pEqTok;` |
|        - | 2464 | `		SyToken *pValTok;` |
|        - | 2465 | `		SyString *pDirName;` |
|        - | 2466 | `		int bIsStrict;` |
|        - | 2467 | `		int iStrictValue;` |
|       41 | 2468 | `		pNameTok = pCursor;` |
|       41 | 2469 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 2470 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2471 | `				"declare: Expecting a directive name");` |
|      ! 0 | 2472 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2473 | `			return SXRET_OK;` |
|        - | 2474 | `		}` |
|       41 | 2475 | `		pEqTok = pNameTok + 1;` |
|       41 | 2476 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|      ! 0 | 2477 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2478 | `				"declare: Expecting '=' after directive name");` |
|      ! 0 | 2479 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2480 | `			return SXRET_OK;` |
|        - | 2481 | `		}` |
|       41 | 2482 | `		pValTok = pEqTok + 1;` |
|       41 | 2483 | `		if( pValTok >= pBodyEnd ){` |
|      ! 0 | 2484 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2485 | `				"declare: Expecting value after '='");` |
|      ! 0 | 2486 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2487 | `			return SXRET_OK;` |
|        - | 2488 | `		}` |
|       41 | 2489 | `		pDirName = &pNameTok->sData;` |
|       41 | 2490 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|       41 | 2491 | `		if( bIsStrict ){` |
|        - | 2492 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|        - | 2493 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|       37 | 2494 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      ! 0 | 2495 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2496 | `					"declare(strict_types) value must be a literal");` |
|      ! 0 | 2497 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2498 | `				return SXRET_OK;` |
|        - | 2499 | `			}` |
|       37 | 2500 | `			iStrictValue = -1;` |
|       37 | 2501 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|       37 | 2502 | `				const char *zv = SyStringData(&pValTok->sData);` |
|       37 | 2503 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|       37 | 2504 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|       35 | 2505 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|       16 | 2506 | `			}` |
|       37 | 2507 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|        3 | 2508 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2509 | `					"strict_types declaration must have 0 or 1 as its value");` |
|        3 | 2510 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        3 | 2511 | `				return SXRET_OK;` |
|        - | 2512 | `			}` |
|       35 | 2513 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|       21 | 2514 | `		}else if( DeclareNameIs(pDirName, "encoding", sizeof("encoding")-1) ){` |
|        - | 2515 | `			/* php always ignores declare(encoding=...) unless it was built with` |
|        - | 2516 | `			 * Zend multibyte, and says so in these exact words. */` |
|        3 | 2517 | `			PH7_GenCompileError(&(*pGen),E_WARNING,nLine,` |
|        - | 2518 | `				"declare(encoding=...) ignored because Zend multibyte feature is turned off by settings");` |
|        1 | 2519 | `		}else{` |
|        - | 2520 | `			/* Other directives (ticks and friends) are accepted as no-ops.` |
|        - | 2521 | `			 * This used to emit a NOTICE naming the upstream engine and its` |
|        - | 2522 | `			 * version ("the declare construct is a no-op in the current release` |
|        - | 2523 | `			 * of the PH7(2.1.4) engine") — php prints nothing at all for` |
|        - | 2524 | ``			 * `declare(ticks=1)`, and leaking the old engine's branding into`` |
|        - | 2525 | `			 * user-visible diagnostics was wrong on its own. */` |
|        - | 2526 | `		}` |
|       39 | 2527 | `		pCursor = pValTok + 1;` |
|        - | 2528 | `		/* Consume separating comma (or end). */` |
|       39 | 2529 | `		if( pCursor < pBodyEnd ){` |
|        3 | 2530 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|      ! 0 | 2531 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2532 | `					"declare: Expecting ',' or ')' after directive value");` |
|      ! 0 | 2533 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2534 | `				return SXRET_OK;` |
|        - | 2535 | `			}` |
|        3 | 2536 | `			pCursor++;` |
|        1 | 2537 | `		}` |
|        5 | 2538 | `	}` |
|        - | 2539 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|        - | 2540 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|        - | 2541 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|       37 | 2542 | `	return SXRET_OK;` |
|        2 | 2543 | `Synchro:` |
|        - | 2544 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|       15 | 2545 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       11 | 2546 | `		pGen->pIn++;` |
|        1 | 2547 | `	}` |
|        5 | 2548 | `	return SXRET_OK;` |
|       27 | 2549 | `}` |
|        - | 2550 | `/*` |
|        - | 2551 | ` * Compile a class constant.` |
|        - | 2552 | ` * According to the PHP language reference manual` |
|        - | 2553 | ` *  Class Constants` |
|        - | 2554 | ` *   It is possible to define constant values on a per-class basis remaining` |
|        - | 2555 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|        - | 2556 | ` *   you don't use the $ symbol to declare or use them.` |
|        - | 2557 | ` *   The value must be a constant expression, not (for example) a variable,` |
|        - | 2558 | ` *   a property, a result of a mathematical operation, or a function call.` |
|        - | 2559 | ` *   It's also possible for interfaces to have constants.` |
|        - | 2560 | ` * Symisc eXtension.` |
|        - | 2561 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|        - | 2562 | ` *  the zend engine would allow only simple scalar value.` |
|        - | 2563 | ` *  Example:` |
|        - | 2564 | ` *   class Test{` |
|        - | 2565 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|        - | 2566 | ` *   };` |
|        - | 2567 | ` *   var_dump(TEST::MyConst);` |
|        - | 2568 | ` *   Refer to the official documentation for more information on the powerful extension` |
|        - | 2569 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|        - | 2570 | ` */` |
|        - | 2571 | `/*` |
|        - | 2572 | ` * Exception handling.` |
|        - | 2573 | ` *  According to the PHP language reference manual` |
|        - | 2574 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|        - | 2575 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|        - | 2576 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|        - | 2577 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|        - | 2578 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|        - | 2579 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|        - | 2580 | ` *    (or re-thrown) within a catch block.` |
|        - | 2581 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|        - | 2582 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|        - | 2583 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|        - | 2584 | ` *    been defined with set_exception_handler().` |
|        - | 2585 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|        - | 2586 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|        - | 2587 | ` */` |
|        - | 2588 | `/*` |
|        - | 2589 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|        - | 2590 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|        - | 2591 | ` * indicates failure.` |
|        - | 2592 | ` */` |
|   548644 | 2593 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 2594 | `{` |
|        - | 2595 | ``	/* php decides throwability at RUNTIME: `throw 5` compiles and raises the`` |
|        - | 2596 | `	 * catchable Error "Can only throw objects" when it executes, and a` |
|        - | 2597 | `	 * non-Throwable object raises "Cannot throw objects that do not implement` |
|        - | 2598 | `	 * Throwable". This used to whitelist a handful of expression shapes and` |
|        - | 2599 | `	 * reject the rest at compile time as a "friendlier" error, which meant the` |
|        - | 2600 | `	 * program never ran and no catch could ever see it — and it was inconsistent` |
|        - | 2601 | ``	 * anyway, since `throw $v` (a variable holding an int) always compiled and`` |
|        - | 2602 | `	 * fell through to the same runtime path. OP_THROW now owns the whole rule,` |
|        - | 2603 | ``	 * so accept any expression here; `throw;` with no operand is still rejected`` |
|        - | 2604 | `	 * by PH7_CompileThrow's SXERR_EMPTY branch. */` |
|   274322 | 2605 | `	SXUNUSED(pGen);` |
|   274322 | 2606 | `	SXUNUSED(pRoot);` |
|   548649 | 2607 | `	return SXRET_OK;` |
|        5 | 2608 | `}` |
|        - | 2609 | `/*` |
|        - | 2610 | ` * Compile a 'throw' statement.` |
|        - | 2611 | ` * throw: This is how you trigger an exception.` |
|        - | 2612 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|        - | 2613 | ` */` |
|   548608 | 2614 | `PH7_PRIVATE sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|        5 | 2615 | `{` |
|   548613 | 2616 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2617 | `	GenBlock *pBlock;` |
|        - | 2618 | `	sxu32 nIdx;` |
|        - | 2619 | `	sxi32 rc;` |
|   548613 | 2620 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|        - | 2621 | `	/* Compile the expression */` |
|   548613 | 2622 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|   548613 | 2623 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 | 2624 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|      ! 0 | 2625 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2626 | `			return SXERR_ABORT;` |
|        - | 2627 | `		}` |
|      ! 0 | 2628 | `		return SXRET_OK;` |
|        - | 2629 | `	}` |
|   548613 | 2630 | `	pBlock = pGen->pCurrent;` |
|        - | 2631 | `	/* Point to the top most function or try block and emit the forward jump */` |
|  2185879 | 2632 | `	while(pBlock->pParent){` |
|  2185873 | 2633 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|   548607 | 2634 | `			break;` |
|        - | 2635 | `		}` |
|        - | 2636 | `		/* Point to the parent block */` |
|  1637271 | 2637 | `		pBlock = pBlock->pParent;` |
|        5 | 2638 | `	}` |
|        - | 2639 | `	/* Emit the throw instruction */` |
|   548613 | 2640 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|        - | 2641 | `	/* Emit the jump */` |
|   548613 | 2642 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|   548613 | 2643 | `	return SXRET_OK;` |
|   274309 | 2644 | `}` |
|        - | 2645 | `/*` |
|        - | 2646 | ` * Compile a PHP 8.0 'throw' expression.` |
|        - | 2647 | ` * Called from the expression code generator when a 'throw' keyword is` |
|        - | 2648 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|        - | 2649 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|        - | 2650 | ` * the validator guarantees the operand is a valid exception target.` |
|        - | 2651 | ` */` |
|       36 | 2652 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|        2 | 2653 | `{` |
|       38 | 2654 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2655 | `	GenBlock *pBlock;` |
|        - | 2656 | `	sxu32 nIdx;` |
|        - | 2657 | `	sxi32 rc;` |
|       18 | 2658 | `	(void)iCompileFlag;` |
|       38 | 2659 | `	pGen->pIn++; /* Skip 'throw' */` |
|       38 | 2660 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 2661 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 2662 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 2663 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2664 | `			return SXERR_ABORT;` |
|        - | 2665 | `		}` |
|      ! 0 | 2666 | `		return SXRET_OK;` |
|        - | 2667 | `	}` |
|       38 | 2668 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|       38 | 2669 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 2670 | `		return SXERR_ABORT;` |
|        - | 2671 | `	}` |
|       38 | 2672 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 | 2673 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 2674 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 2675 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2676 | `			return SXERR_ABORT;` |
|        - | 2677 | `		}` |
|      ! 0 | 2678 | `		return SXRET_OK;` |
|        - | 2679 | `	}` |
|        - | 2680 | `	/* Walk up to nearest exception/function block for the jump target */` |
|       38 | 2681 | `	pBlock = pGen->pCurrent;` |
|       60 | 2682 | `	while( pBlock->pParent ){` |
|       49 | 2683 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|       27 | 2684 | `			break;` |
|        - | 2685 | `		}` |
|       23 | 2686 | `		pBlock = pBlock->pParent;` |
|        1 | 2687 | `	}` |
|       38 | 2688 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       38 | 2689 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|       38 | 2690 | `	return SXRET_OK;` |
|       20 | 2691 | `}` |
|        - | 2692 | `/*` |
|        - | 2693 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|        - | 2694 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|        - | 2695 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|        - | 2696 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|        - | 2697 | ` * compile error propagated from the parser.` |
|        - | 2698 | ` */` |
|       56 | 2699 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|        5 | 2700 | `{` |
|        - | 2701 | `	SyString sClassName;` |
|        - | 2702 | `	SyToken *pToken;` |
|        - | 2703 | `	SyString *pName;` |
|        - | 2704 | `	char *zDup;` |
|        - | 2705 | `	sxi32 rc;` |
|       61 | 2706 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       61 | 2707 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|       61 | 2708 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|       61 | 2709 | `	SySetInit(&pCatch->sByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       61 | 2710 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 | 2711 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 2712 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 2713 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 2714 | `		return SXERR_INVALID;` |
|        - | 2715 | `	}` |
|       61 | 2716 | `	pGen->pIn++; /* '(' */` |
|       28 | 2717 | `	for(;;){` |
|        - | 2718 | `		SyBlob sResolved;` |
|       61 | 2719 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       61 | 2720 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 2721 | `			SyBlobRelease(&sResolved);` |
|      ! 0 | 2722 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 2723 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 2724 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 2725 | `			return SXERR_INVALID;` |
|        - | 2726 | `		}` |
|       89 | 2727 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       56 | 2728 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       61 | 2729 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|       61 | 2730 | `		SyBlobRelease(&sResolved);` |
|       61 | 2731 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|       61 | 2732 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|       61 | 2733 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       56 | 2734 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|        5 | 2735 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|      ! 0 | 2736 | `			pGen->pIn++; continue;` |
|        - | 2737 | `		}` |
|       61 | 2738 | `		break;` |
|      ! 0 | 2739 | `	}` |
|        - | 2740 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|        - | 2741 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding. */` |
|       61 | 2742 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|        3 | 2743 | `		pGen->pIn++; /* ')' */` |
|        3 | 2744 | `		return SXRET_OK;` |
|        - | 2745 | `	}` |
|       54 | 2746 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|       59 | 2747 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 2748 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 2749 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 2750 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 2751 | `		return SXERR_INVALID;` |
|        - | 2752 | `	}` |
|       59 | 2753 | `	pGen->pIn++; /* '$' */` |
|       59 | 2754 | `	pName = &pGen->pIn->sData;` |
|       59 | 2755 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       59 | 2756 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|       59 | 2757 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|       59 | 2758 | `	pGen->pIn++;` |
|       59 | 2759 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 | 2760 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 2761 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 2762 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 2763 | `		return SXERR_INVALID;` |
|        - | 2764 | `	}` |
|       59 | 2765 | `	pGen->pIn++; /* ')' */` |
|       59 | 2766 | `	return SXRET_OK;` |
|       33 | 2767 | `}` |
|        - | 2768 | `/*` |
|        - | 2769 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|        - | 2770 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|        - | 2771 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|        - | 2772 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|        - | 2773 | ` * VmThrowException):` |
|        - | 2774 | ` *` |
|        - | 2775 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|        - | 2776 | ` *    <try body>` |
|        - | 2777 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|        - | 2778 | ` *    JMP  -> finally\|end` |
|        - | 2779 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|        - | 2780 | ` *    <catch body>` |
|        - | 2781 | ` *    JMP  -> finally\|end` |
|        - | 2782 | ` *    ... more catches ...` |
|        - | 2783 | ` *  Lfin: <finally body>` |
|        - | 2784 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|        - | 2785 | ` *  Lend:` |
|        - | 2786 | ` */` |
|      100 | 2787 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|        5 | 2788 | `{` |
|      105 | 2789 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2790 | `	GenBlock *pTry;` |
|        - | 2791 | `	VmInstr *pInstr;` |
|      105 | 2792 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|        - | 2793 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|        - | 2794 | `	sxi32 rc;` |
|      105 | 2795 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|        - | 2796 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION) */` |
|      105 | 2797 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|      105 | 2798 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      105 | 2799 | `	pTry->pUserData = pException;` |
|      105 | 2800 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|      105 | 2801 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|      105 | 2802 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      105 | 2803 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      105 | 2804 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|      105 | 2805 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|        - | 2806 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|      105 | 2807 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|      105 | 2808 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|      105 | 2809 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|      105 | 2810 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 2811 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|      105 | 2812 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|        - | 2813 | `	/* Catch clauses (inline) */` |
|      105 | 2814 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      100 | 2815 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       61 | 2816 | `		sxu32 k = 0;` |
|       84 | 2817 | `		for(;;){` |
|        - | 2818 | `			ph7_exception_block sCatch;` |
|        - | 2819 | `			GenBlock *pCatchBlk;` |
|      117 | 2820 | `			sxu32 idxJmp = 0;` |
|      112 | 2821 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      107 | 2822 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|       33 | 2823 | `				break;` |
|        - | 2824 | `			}` |
|       61 | 2825 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|       61 | 2826 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       61 | 2827 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|       61 | 2828 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|       61 | 2829 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|       61 | 2830 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatchBlk);` |
|       61 | 2831 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        - | 2832 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|        - | 2833 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|        - | 2834 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump). */` |
|       61 | 2835 | `			pCatchBlk->pUserData = pException;` |
|       61 | 2836 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|       61 | 2837 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       61 | 2838 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|       61 | 2839 | `			GenStateLeaveBlock(&(*pGen),0);` |
|        - | 2840 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|        - | 2841 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|       61 | 2842 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       61 | 2843 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|       61 | 2844 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|       61 | 2845 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|       61 | 2846 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       61 | 2847 | `			k++;` |
|        5 | 2848 | `		}` |
|       28 | 2849 | `	}` |
|        - | 2850 | `	/* Finally (inline) */` |
|      105 | 2851 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       80 | 2852 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|        - | 2853 | `		GenBlock *pFinBlk;` |
|       52 | 2854 | `		pGen->pIn++; /* Jump 'finally' */` |
|       52 | 2855 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|       52 | 2856 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|       52 | 2857 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       52 | 2858 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       52 | 2859 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       52 | 2860 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|       52 | 2861 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       52 | 2862 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|       52 | 2863 | `		pException->iHasFinally = 1;` |
|       24 | 2864 | `	}` |
|      105 | 2865 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|      105 | 2866 | `	pException->iInlined = 1;` |
|        - | 2867 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|        - | 2868 | `	{` |
|      105 | 2869 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|        - | 2870 | `		sxu32 *aJ; sxu32 n;` |
|      105 | 2871 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|      105 | 2872 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|      105 | 2873 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|      161 | 2874 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|       61 | 2875 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|       61 | 2876 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|       33 | 2877 | `		}` |
|        - | 2878 | `	}` |
|      105 | 2879 | `	SySetRelease(&aCatchJmp);` |
|      105 | 2880 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|      ! 0 | 2881 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|      ! 0 | 2882 | `	}` |
|      105 | 2883 | `	return SXRET_OK;` |
|       55 | 2884 | `}` |
|        - | 2885 | `/*` |
|        - | 2886 | ` * Compile a 'catch' block.` |
|        - | 2887 | ` * Catch: A "catch" block retrieves an exception and creates` |
|        - | 2888 | ` * an object containing the exception information.` |
|        - | 2889 | ` */` |
|    24966 | 2890 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|        5 | 2891 | `{` |
|    24971 | 2892 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2893 | `	ph7_exception_block sCatch;` |
|        - | 2894 | `	SySet *pInstrContainer;` |
|        - | 2895 | `	SyString sClassName;` |
|        - | 2896 | `	GenBlock *pCatch;` |
|        - | 2897 | `	SyToken *pToken;` |
|        - | 2898 | `	SyString *pName;` |
|        - | 2899 | `	char *zDup;` |
|        - | 2900 | `	sxi32 rc;` |
|    24971 | 2901 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|        - | 2902 | `	/* Zero the structure */` |
|    24971 | 2903 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|        - | 2904 | `	/* Initialize fields */` |
|    24971 | 2905 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|    24971 | 2906 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|    24971 | 2907 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|        - | 2908 | `			/* Unexpected token,break immediately */` |
|      ! 0 | 2909 | `			pToken = pGen->pIn;` |
|      ! 0 | 2910 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 2911 | `				pToken--;` |
|      ! 0 | 2912 | `			}` |
|      ! 0 | 2913 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 2914 | `				"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 2915 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 2916 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2917 | `				return SXERR_ABORT;` |
|        - | 2918 | `			}` |
|      ! 0 | 2919 | `			return SXERR_INVALID;` |
|        - | 2920 | `	}` |
|        - | 2921 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|    24971 | 2922 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|    12498 | 2923 | `	for(;;){` |
|        - | 2924 | `		SyBlob sResolved;` |
|    25001 | 2925 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    25001 | 2926 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|        6 | 2927 | `			SyBlobRelease(&sResolved);` |
|        6 | 2928 | `			pToken = pGen->pIn;` |
|        6 | 2929 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 2930 | `				pToken--;` |
|      ! 0 | 2931 | `			}` |
|        8 | 2932 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 2933 | `				"syntax error, unexpected %s \"%z\"",` |
|        2 | 2934 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|        6 | 2935 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2936 | `				return SXERR_ABORT;` |
|        - | 2937 | `			}` |
|        6 | 2938 | `			return SXERR_INVALID;` |
|        - | 2939 | `		}` |
|        - | 2940 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|        - | 2941 | `		 * transient SyBlob allocation. */` |
|    37493 | 2942 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    24992 | 2943 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|    24997 | 2944 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|    24997 | 2945 | `		SyBlobRelease(&sResolved);` |
|    24997 | 2946 | `		if( zDup == 0 ){` |
|      ! 0 | 2947 | `			goto Mem;` |
|        - | 2948 | `		}` |
|    24997 | 2949 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|    24997 | 2950 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 2951 | `			goto Mem;` |
|        - | 2952 | `		}` |
|        - | 2953 | `		/* Check for '\|' (multi-catch separator) */` |
|    24992 | 2954 | `		if( pGen->pIn < pGen->pEnd &&` |
|    24992 | 2955 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|       35 | 2956 | `			pGen->pIn->sData.nByte == 1 &&` |
|       30 | 2957 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|       32 | 2958 | `			pGen->pIn++; /* Consume the '\|' */` |
|       32 | 2959 | `			continue;` |
|        - | 2960 | `		}` |
|    24967 | 2961 | `		break;` |
|      ! 0 | 2962 | `	}` |
|        - | 2963 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|        - | 2964 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding;` |
|        - | 2965 | `	 * jump straight to compiling the block below. */` |
|    24967 | 2966 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) /*)*/ ){` |
|        7 | 2967 | `		goto CatchBody;` |
|        - | 2968 | `	}` |
|    24956 | 2969 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|    24961 | 2970 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - | 2971 | `			/* Unexpected token,break immediately */` |
|      ! 0 | 2972 | `			pToken = pGen->pIn;` |
|      ! 0 | 2973 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 2974 | `				pToken--;` |
|      ! 0 | 2975 | `			}` |
|      ! 0 | 2976 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 2977 | `				"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 2978 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 2979 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2980 | `				return SXERR_ABORT;` |
|        - | 2981 | `			}` |
|      ! 0 | 2982 | `			return SXERR_INVALID;` |
|        - | 2983 | `	}` |
|    24961 | 2984 | `	pGen->pIn++; /* Jump the dollar sign */` |
|        - | 2985 | `	/* Duplicate instance name */` |
|    24961 | 2986 | `	pName = &pGen->pIn->sData;` |
|    24961 | 2987 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    24961 | 2988 | `	if( zDup == 0 ){` |
|      ! 0 | 2989 | `		goto Mem;` |
|        - | 2990 | `	}` |
|    24961 | 2991 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|    24961 | 2992 | `	pGen->pIn++;` |
|    12481 | 2993 | `CatchBody:` |
|    24967 | 2994 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|        - | 2995 | `		/* Unexpected token,break immediately */` |
|      ! 0 | 2996 | `		pToken = pGen->pIn;` |
|      ! 0 | 2997 | `		if( pToken >= pGen->pEnd ){` |
|      ! 0 | 2998 | `			pToken--;` |
|      ! 0 | 2999 | `		}` |
|      ! 0 | 3000 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 3001 | `			"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 3002 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 3003 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3004 | `			return SXERR_ABORT;` |
|        - | 3005 | `		}` |
|      ! 0 | 3006 | `		return SXERR_INVALID;` |
|        - | 3007 | `	}` |
|        - | 3008 | `	/* Compile the block */` |
|    24967 | 3009 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|        - | 3010 | `	/* Create the catch block */` |
|    24967 | 3011 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|    24967 | 3012 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3013 | `		return SXERR_ABORT;` |
|        - | 3014 | `	}` |
|        - | 3015 | `	/* Swap bytecode container */` |
|    24967 | 3016 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    24967 | 3017 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|        - | 3018 | `	/* Compile the block */` |
|    24967 | 3019 | `	PH7_CompileBlock(&(*pGen),0);` |
|        - | 3020 | `	/* Fix forward jumps now the destination is resolved  */` |
|    24967 | 3021 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 3022 | `	/* Emit the DONE instruction */` |
|    24967 | 3023 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - | 3024 | `	/* Leave the block */` |
|    24967 | 3025 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 3026 | `	/* Restore the default container */` |
|    24967 | 3027 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - | 3028 | `	/* Install the catch block */` |
|    24967 | 3029 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|    24967 | 3030 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3031 | `		goto Mem;` |
|        - | 3032 | `	}` |
|    24967 | 3033 | `	return SXRET_OK;` |
|      ! 0 | 3034 | `Mem:` |
|      ! 0 | 3035 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 3036 | `	return SXERR_ABORT;` |
|    12488 | 3037 | `}` |
|        - | 3038 | `/*` |
|        - | 3039 | ` * Compile a 'try' block.` |
|        - | 3040 | ` * A function using an exception should be in a "try" block.` |
|        - | 3041 | ` * If the exception does not trigger, the code will continue` |
|        - | 3042 | ` * as normal. However if the exception triggers, an exception` |
|        - | 3043 | ` * is "thrown".` |
|        - | 3044 | ` */` |
|    25124 | 3045 | `PH7_PRIVATE sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|        5 | 3046 | `{` |
|        - | 3047 | `	ph7_exception *pException;` |
|    25129 | 3048 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 3049 | `	GenBlock *pTry;` |
|        - | 3050 | `	sxu32 nJmpIdx;` |
|        - | 3051 | `	sxi32 rc;` |
|        - | 3052 | `	/* Create the exception container */` |
|    25129 | 3053 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|    25129 | 3054 | `	if( pException == 0 ){` |
|      ! 0 | 3055 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|      ! 0 | 3056 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 3057 | `		return SXERR_ABORT;` |
|        - | 3058 | `	}` |
|        - | 3059 | `	/* Zero the structure */` |
|    25129 | 3060 | `	SyZero(pException,sizeof(ph7_exception));` |
|        - | 3061 | `	/* Initialize fields */` |
|    25129 | 3062 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|    25129 | 3063 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|    25129 | 3064 | `	pException->iHasFinally = 0;` |
|    25129 | 3065 | `	pException->iFinallyDone = 0;` |
|    25129 | 3066 | `	pException->pVm = pGen->pVm;` |
|        - | 3067 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|        - | 3068 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the legacy path.`` |
|        - | 3069 | `	 * DORMANT until the inline VM handlers (OP_CATCH / OP_END_FINALLY dispatch,` |
|        - | 3070 | `	 * VmThrowException pc-redirect, return/break-through-finally threading, generator` |
|        - | 3071 | `	 * park of aFinallyAction) land — the compiler emits the layout but the VM cannot yet` |
|        - | 3072 | `	 * execute it. Guarded by pVm->bInlineTryCatch (default 0) so the tree stays green. */` |
|    25129 | 3073 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|      105 | 3074 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|        - | 3075 | `	}` |
|        - | 3076 | `	/* Create the try block */` |
|    25029 | 3077 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|    25029 | 3078 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3079 | `		return SXERR_ABORT;` |
|        - | 3080 | `	}` |
|        - | 3081 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|    25029 | 3082 | `	pTry->pUserData = pException;` |
|        - | 3083 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|    25029 | 3084 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|        - | 3085 | `	/* Fix the jump later when the destination is resolved */` |
|    25029 | 3086 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|    25029 | 3087 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|        - | 3088 | `	/* Compile the block */` |
|    25029 | 3089 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|    25029 | 3090 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 3091 | `		return SXERR_ABORT;` |
|        - | 3092 | `	}` |
|        - | 3093 | `	/* Fix forward jumps now the destination is resolved */` |
|    25029 | 3094 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 3095 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|    25029 | 3096 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|        - | 3097 | `	/* Leave the block */` |
|    25029 | 3098 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 3099 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|    25029 | 3100 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|    25022 | 3101 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|        - | 3102 | `		/* Compile one or more catch blocks */` |
|    24962 | 3103 | `		for(;;){` |
|    49924 | 3104 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    37525 | 3105 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|    12484 | 3106 | `					break;` |
|        - | 3107 | `			}` |
|    24971 | 3108 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|    24971 | 3109 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3110 | `				return SXERR_ABORT;` |
|        - | 3111 | `			}` |
|        5 | 3112 | `		}` |
|    12479 | 3113 | `	}` |
|        - | 3114 | `	/* Compile optional finally block */` |
|    25029 | 3115 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      804 | 3116 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|        - | 3117 | `		SySet *pInstrContainer;` |
|        - | 3118 | `		GenBlock *pFinBlock;` |
|      129 | 3119 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|        - | 3120 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      129 | 3121 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      129 | 3122 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 3123 | `			return SXERR_ABORT;` |
|        - | 3124 | `		}` |
|        - | 3125 | `		/* Swap bytecode container */` |
|      129 | 3126 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      129 | 3127 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|        - | 3128 | `		/* Compile the finally body */` |
|      129 | 3129 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      129 | 3130 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3131 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 | 3132 | `			return SXERR_ABORT;` |
|        - | 3133 | `		}` |
|        - | 3134 | `		/* Fix forward jumps now the destination is resolved */` |
|      129 | 3135 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 3136 | `		/* Emit DONE to terminate the finally block */` |
|      129 | 3137 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - | 3138 | `		/* Leave the block */` |
|      129 | 3139 | `		GenStateLeaveBlock(&(*pGen),0);` |
|        - | 3140 | `		/* Restore the default container */` |
|      129 | 3141 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      129 | 3142 | `		pException->iHasFinally = 1;` |
|       62 | 3143 | `	}` |
|        - | 3144 | `	/* Must have at least one catch or finally */` |
|    25029 | 3145 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|        9 | 3146 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 3147 | `			"Cannot use try without catch or finally");` |
|        9 | 3148 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3149 | `			return SXERR_ABORT;` |
|        - | 3150 | `		}` |
|        3 | 3151 | `	}` |
|    25029 | 3152 | `	return SXRET_OK;` |
|    12567 | 3153 | `}` |
|        - | 3154 | `/*` |
|        - | 3155 | ` * Compile a switch block.` |
|        - | 3156 | ` *  (See block-comment below for more information)` |
|        - | 3157 | ` */` |
|   136192 | 3158 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|        5 | 3159 | `{` |
|   136197 | 3160 | `	sxi32 rc = SXRET_OK;` |
|   136197 | 3161 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|        - | 3162 | `		/* Unexpected token */` |
|      ! 0 | 3163 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 | 3164 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3165 | `			return SXERR_ABORT;` |
|        - | 3166 | `		}` |
|      ! 0 | 3167 | `		pGen->pIn++;` |
|      ! 0 | 3168 | `	}` |
|   136197 | 3169 | `	pGen->pIn++;` |
|        - | 3170 | `	/* First instruction to execute in this block. */` |
|   136197 | 3171 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|        - | 3172 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|        - | 3173 | `	 * or the '}' token */` |
|   130454 | 3174 | `	for(;;){` |
|   260913 | 3175 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 3176 | `			/* No more input to process */` |
|      ! 0 | 3177 | `			break;` |
|        - | 3178 | `		}` |
|   260913 | 3179 | `		rc = SXRET_OK;` |
|   260913 | 3180 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|    35077 | 3181 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|    11695 | 3182 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|        - | 3183 | `					/* Unexpected token */` |
|      ! 0 | 3184 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|      ! 0 | 3185 | `						&pGen->pIn->sData);` |
|      ! 0 | 3186 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3187 | `						return SXERR_ABORT;` |
|        - | 3188 | `					}` |
|        - | 3189 | `					/* FALL THROUGH */` |
|      ! 0 | 3190 | `				}` |
|    11695 | 3191 | `				rc = SXERR_EOF;` |
|    11695 | 3192 | `				break;` |
|        - | 3193 | `			}` |
|    11696 | 3194 | `		}else{` |
|        - | 3195 | `			sxi32 nKwrd;` |
|        - | 3196 | `			/* Extract the keyword */` |
|   225841 | 3197 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   225841 | 3198 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|    62255 | 3199 | `				break;` |
|        - | 3200 | `			}` |
|   101341 | 3201 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|        3 | 3202 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|        - | 3203 | `					/* Unexpected token */` |
|      ! 0 | 3204 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|      ! 0 | 3205 | `						&pGen->pIn->sData);` |
|      ! 0 | 3206 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3207 | `						return SXERR_ABORT;` |
|        - | 3208 | `					}` |
|        - | 3209 | `					/* FALL THROUGH */` |
|      ! 0 | 3210 | `				}` |
|        - | 3211 | `				/* Block compiled */` |
|        3 | 3212 | `				break;` |
|        - | 3213 | `			}` |
|        - | 3214 | `		}` |
|        - | 3215 | `		/* Compile block */` |
|   124721 | 3216 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|   124721 | 3217 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3218 | `			return SXERR_ABORT;` |
|        - | 3219 | `		}` |
|        5 | 3220 | `	}` |
|   136197 | 3221 | `	return rc;` |
|    68101 | 3222 | `}` |
|        - | 3223 | `/*` |
|        - | 3224 | ` * Compile a case eXpression.` |
|        - | 3225 | ` *  (See block-comment below for more information)` |
|        - | 3226 | ` */` |
|   132284 | 3227 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|        5 | 3228 | `{` |
|        - | 3229 | `	SySet *pInstrContainer;` |
|        - | 3230 | `	SyToken *pEnd,*pTmp;` |
|   132289 | 3231 | `	sxi32 iNest = 0;` |
|        - | 3232 | `	sxi32 rc;` |
|        - | 3233 | `	/* Delimit the expression */` |
|   132289 | 3234 | `	pEnd = pGen->pIn;` |
|   264581 | 3235 | `	while( pEnd < pGen->pEnd ){` |
|   264581 | 3236 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|        - | 3237 | `			/* Increment nesting level */` |
|        3 | 3238 | `			iNest++;` |
|   264580 | 3239 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|        - | 3240 | `			/* Decrement nesting level */` |
|        3 | 3241 | `			iNest--;` |
|   264578 | 3242 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|   132289 | 3243 | `			break;` |
|        - | 3244 | `		}` |
|   132297 | 3245 | `		pEnd++;` |
|        5 | 3246 | `	}` |
|   132289 | 3247 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 3248 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|      ! 0 | 3249 | `		if( rc == SXERR_ABORT ){` |
|        - | 3250 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 3251 | `			return SXERR_ABORT;` |
|        - | 3252 | `		}` |
|      ! 0 | 3253 | `	}` |
|        - | 3254 | `	/* Swap token stream */` |
|   132289 | 3255 | `	pTmp = pGen->pEnd;` |
|   132289 | 3256 | `	pGen->pEnd = pEnd;` |
|   132289 | 3257 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   132289 | 3258 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|   132289 | 3259 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 3260 | `	/* Emit the done instruction */` |
|   132289 | 3261 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   132289 | 3262 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - | 3263 | `	/* Update token stream */` |
|   132289 | 3264 | `	pGen->pIn  = pEnd;` |
|   132289 | 3265 | `	pGen->pEnd = pTmp;` |
|   132289 | 3266 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 3267 | `		return SXERR_ABORT;` |
|        - | 3268 | `	}` |
|   132289 | 3269 | `	return SXRET_OK;` |
|    66147 | 3270 | `}` |
|        - | 3271 | `/*` |
|        - | 3272 | ` * Compile the smart switch statement.` |
|        - | 3273 | ` * According to the PHP language reference manual` |
|        - | 3274 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|        - | 3275 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|        - | 3276 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|        - | 3277 | ` *  This is exactly what the switch statement is for.` |
|        - | 3278 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|        - | 3279 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|        - | 3280 | ` *  of the outer loop, use continue 2.` |
|        - | 3281 | ` *  Note that switch/case does loose comparision.` |
|        - | 3282 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|        - | 3283 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|        - | 3284 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|        - | 3285 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|        - | 3286 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|        - | 3287 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|        - | 3288 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|        - | 3289 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|        - | 3290 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|        - | 3291 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|        - | 3292 | ` *  list for the next case.` |
|        - | 3293 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|        - | 3294 | ` *  or floating-point numbers and strings.` |
|        - | 3295 | ` */` |
|    11692 | 3296 | `PH7_PRIVATE sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|        5 | 3297 | `{` |
|        - | 3298 | `	GenBlock *pSwitchBlock;` |
|        - | 3299 | `	SyToken *pTmp,*pEnd;` |
|        - | 3300 | `	ph7_switch *pSwitch;` |
|        - | 3301 | `	sxu32 nToken;` |
|        - | 3302 | `	sxu32 nLine;` |
|        - | 3303 | `	sxi32 rc;` |
|    11697 | 3304 | `	nLine = pGen->pIn->nLine;` |
|        - | 3305 | `	/* Jump the 'switch' keyword */` |
|    11697 | 3306 | `	pGen->pIn++;` |
|    11697 | 3307 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 3308 | `		/* Syntax error */` |
|      ! 0 | 3309 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|      ! 0 | 3310 | `		if( rc == SXERR_ABORT ){` |
|        - | 3311 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 3312 | `			return SXERR_ABORT;` |
|        - | 3313 | `		}` |
|      ! 0 | 3314 | `		goto Synchronize;` |
|        - | 3315 | `	}` |
|        - | 3316 | `	/* Jump the left parenthesis '(' */` |
|    11697 | 3317 | `	pGen->pIn++;` |
|    11697 | 3318 | `	pEnd = 0; /* cc warning */` |
|        - | 3319 | `	/* Create the loop block */` |
|    17543 | 3320 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|     5846 | 3321 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|    11697 | 3322 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3323 | `		return SXERR_ABORT;` |
|        - | 3324 | `	}` |
|        - | 3325 | `	/* Delimit the condition */` |
|    11697 | 3326 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    11697 | 3327 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - | 3328 | `		/* Empty expression */` |
|      ! 0 | 3329 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|      ! 0 | 3330 | `		if( rc == SXERR_ABORT ){` |
|        - | 3331 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 3332 | `			return SXERR_ABORT;` |
|        - | 3333 | `		}` |
|      ! 0 | 3334 | `	}` |
|        - | 3335 | `	/* Swap token streams */` |
|    11697 | 3336 | `	pTmp = pGen->pEnd;` |
|    11697 | 3337 | `	pGen->pEnd = pEnd;` |
|        - | 3338 | `	/* Compile the expression */` |
|    11697 | 3339 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    11697 | 3340 | `	if( rc == SXERR_ABORT ){` |
|        - | 3341 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 3342 | `		return SXERR_ABORT;` |
|        - | 3343 | `	}` |
|        - | 3344 | `	/* Update token stream */` |
|    11697 | 3345 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 | 3346 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 3347 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 3348 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3349 | `			return SXERR_ABORT;` |
|        - | 3350 | `		}` |
|      ! 0 | 3351 | `		pGen->pIn++;` |
|      ! 0 | 3352 | `	}` |
|    11697 | 3353 | `	pGen->pIn  = &pEnd[1];` |
|    11697 | 3354 | `	pGen->pEnd = pTmp;` |
|    11697 | 3355 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|    11692 | 3356 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|      ! 0 | 3357 | `			pTmp = pGen->pIn;` |
|      ! 0 | 3358 | `			if( pTmp >= pGen->pEnd ){` |
|      ! 0 | 3359 | `				pTmp--;` |
|      ! 0 | 3360 | `			}` |
|        - | 3361 | `			/* Unexpected token */` |
|      ! 0 | 3362 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|      ! 0 | 3363 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3364 | `				return SXERR_ABORT;` |
|        - | 3365 | `			}` |
|      ! 0 | 3366 | `			goto Synchronize;` |
|        - | 3367 | `	}` |
|        - | 3368 | `	/* Set the delimiter token */` |
|    11697 | 3369 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|        3 | 3370 | `		nToken = PH7_TK_KEYWORD;` |
|        - | 3371 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|        2 | 3372 | `	}else{` |
|    11695 | 3373 | `		nToken = PH7_TK_CCB; /* '}' */` |
|        - | 3374 | `	}` |
|    11697 | 3375 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|        - | 3376 | `	/* Create the switch blocks container */` |
|    11697 | 3377 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|    11697 | 3378 | `	if( pSwitch == 0 ){` |
|        - | 3379 | `		/* Abort compilation */` |
|      ! 0 | 3380 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 3381 | `		return SXERR_ABORT;` |
|        - | 3382 | `	}` |
|        - | 3383 | `	/* Zero the structure */` |
|    11697 | 3384 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|        - | 3385 | `	/* Initialize fields */` |
|    11697 | 3386 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|        - | 3387 | `	/* Emit the switch instruction */` |
|    11697 | 3388 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|        - | 3389 | `	/* Compile case blocks */` |
|   130348 | 3390 | `	for(;;){` |
|        - | 3391 | `		sxu32 nKwrd;` |
|   136199 | 3392 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 3393 | `			/* No more input to process */` |
|      ! 0 | 3394 | `			break;` |
|        - | 3395 | `		}` |
|   136199 | 3396 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 3397 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|        - | 3398 | `				/* Unexpected token */` |
|      ! 0 | 3399 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 3400 | `					&pGen->pIn->sData);` |
|      ! 0 | 3401 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3402 | `					return SXERR_ABORT;` |
|        - | 3403 | `				}` |
|        - | 3404 | `				/* FALL THROUGH */` |
|      ! 0 | 3405 | `			}` |
|        - | 3406 | `			/* Block compiled */` |
|      ! 0 | 3407 | `			break;` |
|        - | 3408 | `		}` |
|        - | 3409 | `		/* Extract the keyword */` |
|   136199 | 3410 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   136199 | 3411 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|        3 | 3412 | `			if( nToken != PH7_TK_KEYWORD ){` |
|        - | 3413 | `				/* Unexpected token */` |
|      ! 0 | 3414 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 3415 | `					&pGen->pIn->sData);` |
|      ! 0 | 3416 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3417 | `					return SXERR_ABORT;` |
|        - | 3418 | `				}` |
|        - | 3419 | `				/* FALL THROUGH */` |
|      ! 0 | 3420 | `			}` |
|        - | 3421 | `			/* Block compiled */` |
|        3 | 3422 | `			break;` |
|        - | 3423 | `		}` |
|   136197 | 3424 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|        - | 3425 | `			/*` |
|        - | 3426 | `			 * Accroding to the PHP language reference manual` |
|        - | 3427 | `			 *  A special case is the default case. This case matches anything` |
|        - | 3428 | `			 *  that wasn't matched by the other cases.` |
|        - | 3429 | `			 */` |
|     3913 | 3430 | `			if( pSwitch->nDefault > 0 ){` |
|        - | 3431 | `				/* Default case already compiled */` |
|      ! 0 | 3432 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|      ! 0 | 3433 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3434 | `					return SXERR_ABORT;` |
|        - | 3435 | `				}` |
|      ! 0 | 3436 | `			}` |
|     3913 | 3437 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|        - | 3438 | `			/* Compile the default block */` |
|     3913 | 3439 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|     3913 | 3440 | `			if( rc == SXERR_ABORT){` |
|      ! 0 | 3441 | `				return SXERR_ABORT;` |
|     3913 | 3442 | `			}else if( rc == SXERR_EOF ){` |
|     3911 | 3443 | `				break;` |
|        1 | 3444 | `			}` |
|   132290 | 3445 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|        - | 3446 | `			ph7_case_expr sCase;` |
|        - | 3447 | `			/* Standard case block */` |
|   132289 | 3448 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|        - | 3449 | `			/* initialize the structure */` |
|   132289 | 3450 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - | 3451 | `			/* Compile the case expression */` |
|   132289 | 3452 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|   132289 | 3453 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3454 | `				return SXERR_ABORT;` |
|        - | 3455 | `			}` |
|        - | 3456 | `			/* Compile the case block */` |
|   132289 | 3457 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|        - | 3458 | `			/* Insert in the switch container */` |
|   132289 | 3459 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|   132289 | 3460 | `			if( rc == SXERR_ABORT){` |
|      ! 0 | 3461 | `				return SXERR_ABORT;` |
|   132289 | 3462 | `			}else if( rc == SXERR_EOF ){` |
|     7789 | 3463 | `				break;` |
|        - | 3464 | `			}` |
|    62255 | 3465 | `		}else{` |
|        - | 3466 | `			/* Unexpected token */` |
|      ! 0 | 3467 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 3468 | `				&pGen->pIn->sData);` |
|      ! 0 | 3469 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3470 | `				return SXERR_ABORT;` |
|        - | 3471 | `			}` |
|      ! 0 | 3472 | `			break;` |
|        - | 3473 | `		}` |
|        5 | 3474 | `	}` |
|        - | 3475 | `	/* Fix all jumps now the destination is resolved */` |
|    11697 | 3476 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|    11697 | 3477 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 3478 | `	/* Release the loop block */` |
|    11697 | 3479 | `	GenStateLeaveBlock(pGen,0);` |
|    11697 | 3480 | `	if( pGen->pIn < pGen->pEnd ){` |
|        - | 3481 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|    11697 | 3482 | `		pGen->pIn++;` |
|     5846 | 3483 | `	}` |
|        - | 3484 | `	/* Statement successfully compiled */` |
|    11697 | 3485 | `	return SXRET_OK;` |
|      ! 0 | 3486 | `Synchronize:` |
|        - | 3487 | `	/* Synchronize with the first semi-colon */` |
|      ! 0 | 3488 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      ! 0 | 3489 | `		pGen->pIn++;` |
|      ! 0 | 3490 | `	}` |
|      ! 0 | 3491 | `	return SXRET_OK;` |
|     5851 | 3492 | `}` |
|        - | 3493 |  |
