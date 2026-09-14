# src/ph7/compile_stmt.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1452/1974 lines (73.56%)

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
|       50 |   40 | `PH7_PRIVATE sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|        5 |   41 | `{` |
|        - |   42 | `	SySet *pConsCode,*pInstrContainer;` |
|       55 |   43 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|        - |   44 | `	SyString *pName;` |
|        - |   45 | `	sxi32 rc;` |
|       55 |   46 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       55 |   47 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |   48 | `		/* Invalid constant name */` |
|        8 |   49 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"identifier");` |
|        8 |   50 | `		if( rc == SXERR_ABORT ){` |
|        - |   51 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |   52 | `			return SXERR_ABORT;` |
|        - |   53 | `		}` |
|        8 |   54 | `		goto Synchronize;` |
|        - |   55 | `	}` |
|        - |   56 | `	/* Peek constant name */` |
|       48 |   57 | `	pName = &pGen->pIn->sData;` |
|        - |   58 | `	/* Make sure the constant name isn't reserved */` |
|       48 |   59 | `	if( GenStateIsReservedConstant(pName) ){` |
|        - |   60 | `		/* Reserved constant */` |
|       10 |   61 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Cannot redeclare constant '%z'",pName);` |
|       10 |   62 | `		if( rc == SXERR_ABORT ){` |
|        - |   63 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |   64 | `			return SXERR_ABORT;` |
|        - |   65 | `		}` |
|       10 |   66 | `		goto Synchronize;` |
|        - |   67 | `	}` |
|       40 |   68 | `	pGen->pIn++;` |
|       40 |   69 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|        - |   70 | `		/* Invalid statement*/` |
|        6 |   71 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"=\"");` |
|        6 |   72 | `		if( rc == SXERR_ABORT ){` |
|        - |   73 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |   74 | `			return SXERR_ABORT;` |
|        - |   75 | `		}` |
|        6 |   76 | `		goto Synchronize;` |
|        - |   77 | `	}` |
|       34 |   78 | `	pGen->pIn++; /*Jump the equal sign */` |
|        - |   79 | `	/* Allocate a new constant value container */` |
|       34 |   80 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|       34 |   81 | `	if( pConsCode == 0 ){` |
|      ! 0 |   82 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |   83 | `		return SXERR_ABORT;` |
|        - |   84 | `	}` |
|       34 |   85 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - |   86 | `	/* Swap bytecode container */` |
|       34 |   87 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       34 |   88 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|        - |   89 | `	/* Compile constant value */` |
|       34 |   90 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |   91 | `	/* Emit the done instruction */` |
|       34 |   92 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       34 |   93 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       34 |   94 | `	if( rc == SXERR_ABORT ){` |
|        - |   95 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |   96 | `		return SXERR_ABORT;` |
|        - |   97 | `	}` |
|       34 |   98 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|        - |   99 | `	/* Register the constant with namespace-qualified name */` |
|        - |  100 | `	{` |
|        - |  101 | `		SyBlob sFQN;` |
|        - |  102 | `		SyString sFQNStr;` |
|       34 |  103 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       34 |  104 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|       34 |  105 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|       50 |  106 | `		rc = PH7_VmRegisterConstantEx(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode,` |
|       32 |  107 | `			(SyString *)SySetPeek(&pGen->pVm->aFiles),nLineLocal,1);` |
|       34 |  108 | `		if( rc == SXRET_OK && SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
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
|       34 |  121 | `		SyBlobRelease(&sFQN);` |
|        - |  122 | `	}` |
|       34 |  123 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  124 | `		SySetRelease(pConsCode);` |
|      ! 0 |  125 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|      ! 0 |  126 | `	}` |
|       34 |  127 | `	return SXRET_OK;` |
|        9 |  128 | `Synchronize:` |
|        - |  129 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|       60 |  130 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       42 |  131 | `		pGen->pIn++;` |
|        4 |  132 | `	}` |
|       22 |  133 | `	return SXRET_OK;` |
|       30 |  134 | `}` |
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
|   178542 |  157 | `static int GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|        5 |  158 | `{` |
|   178547 |  159 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   178547 |  160 | `	int nInlineTry = 0;` |
|   721713 |  161 | `	while( pBlock && pBlock != pTarget ){` |
|   543171 |  162 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
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
|   543171 |  179 | `		pBlock = pBlock->pParent;` |
|        5 |  180 | `	}` |
|   178547 |  181 | `	return nInlineTry;` |
|        5 |  182 | `}` |
|    89244 |  183 | `PH7_PRIVATE sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|        5 |  184 | `{` |
|        - |  185 | `	GenBlock *pLoop; /* Target loop */` |
|        - |  186 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|        - |  187 | `	sxu32 nLineLocal;` |
|        - |  188 | `	sxi32 rc;` |
|    89249 |  189 | `	nLineLocal = pGen->pIn->nLine;` |
|    89249 |  190 | `	iLevel = 0;` |
|        - |  191 | `	/* Jump the 'continue' keyword */` |
|    89249 |  192 | `	pGen->pIn++;` |
|    89249 |  193 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
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
|    89249 |  219 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    89249 |  220 | `	if( pLoop == 0 ){` |
|        - |  221 | `		/* Illegal continue */` |
|       12 |  222 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"'continue' not in the 'loop' or 'switch' context");` |
|       12 |  223 | `		if( rc == SXERR_ABORT ){` |
|        - |  224 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  225 | `			return SXERR_ABORT;` |
|        - |  226 | `		}` |
|        7 |  227 | `	}else{` |
|    89239 |  228 | `		sxu32 nInstrIdx = 0;` |
|        - |  229 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|    89239 |  230 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|        - |  231 | `		/* ROOT C: in a generator, a break/continue crossing inline trys must run their` |
|        - |  232 | `		 * finallys first. OP_SET_FINALLY_JMP(iP1=count) does that then takes the loop jump. */` |
|    89239 |  233 | `		sxi32 iJmpOp = nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP;` |
|    89239 |  234 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
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
|    89235 |  250 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    89235 |  251 | `			if( pLoop->bPostContinue == TRUE ){` |
|        - |  252 | `				JumpFixup sJumpFix;` |
|        - |  253 | `				/* Post-continue */` |
|    27163 |  254 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|    27163 |  255 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|    27163 |  256 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|    13579 |  257 | `			}` |
|        - |  258 | `		}` |
|        - |  259 | `	}` |
|    89249 |  260 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  261 | `		/* Not so fatal,emit a warning only */` |
|      ! 0 |  262 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|      ! 0 |  263 | `	}` |
|        - |  264 | `	/* Statement successfully compiled */` |
|    89249 |  265 | `	return SXRET_OK;` |
|    44627 |  266 | `}` |
|        - |  267 | `/*` |
|        - |  268 | ` * Compile the 'break' statement.` |
|        - |  269 | ` * According to the PHP language reference` |
|        - |  270 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|        - |  271 | ` *  structure.` |
|        - |  272 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|        - |  273 | ` *  enclosing structures are to be broken out of.` |
|        - |  274 | ` */` |
|    89324 |  275 | `PH7_PRIVATE sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|        5 |  276 | `{` |
|        - |  277 | `	GenBlock *pLoop; /* Target loop */` |
|        - |  278 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|        - |  279 | `	sxi32 rc;` |
|    89329 |  280 | `	iLevel = 0;` |
|        - |  281 | `	/* Jump the 'break' keyword */` |
|    89329 |  282 | `	pGen->pIn++;` |
|    89329 |  283 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|        - |  284 | `		/* optional numeric argument which tells us how many levels` |
|        - |  285 | `		 * of enclosing loops we should skip to the end of.` |
|        - |  286 | `		 */` |
|        - |  287 | `		char zScratch[GEN_NUM_SCRATCH];` |
|       17 |  288 | `		char *zAlloc = 0;` |
|        - |  289 | `		SyString sNum;` |
|       17 |  290 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|       17 |  291 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  292 | `			return SXERR_ABORT;` |
|        - |  293 | `		}` |
|       17 |  294 | `		if( rc == SXRET_OK ){` |
|       20 |  295 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|       12 |  296 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|       14 |  297 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  298 | `				return SXERR_ABORT;` |
|        - |  299 | `			}` |
|       14 |  300 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|       14 |  301 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        6 |  302 | `		}` |
|       17 |  303 | `		if( iLevel < 2 ){` |
|        3 |  304 | `			iLevel = 0;` |
|        1 |  305 | `		}` |
|       17 |  306 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|        7 |  307 | `	}` |
|        - |  308 | `	/* Extract the target loop */` |
|    89329 |  309 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    89329 |  310 | `	if( pLoop == 0 ){` |
|        - |  311 | `		/* Illegal break */` |
|       18 |  312 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"'break' not in the 'loop' or 'switch' context");` |
|       18 |  313 | `		if( rc == SXERR_ABORT ){` |
|        - |  314 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  315 | `			return SXERR_ABORT;` |
|        - |  316 | `		}` |
|       10 |  317 | `	}else{` |
|        - |  318 | `		sxu32 nInstrIdx;` |
|        - |  319 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|    89313 |  320 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|        - |  321 | `		/* ROOT C: OP_SET_FINALLY_JMP runs the crossed inline finallys before the break jump. */` |
|    89313 |  322 | `		rc = PH7_VmEmitInstr(pGen->pVm,nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP,nCross,0,0,&nInstrIdx);` |
|    89313 |  323 | `		if( rc == SXRET_OK ){` |
|        - |  324 | `			/* Fix the jump later when the jump destination is resolved */` |
|    89313 |  325 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|    44654 |  326 | `		}` |
|        - |  327 | `	}` |
|    89329 |  328 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  329 | `		/* Not so fatal,emit a warning only */` |
|      ! 0 |  330 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|      ! 0 |  331 | `	}` |
|        - |  332 | `	/* Statement successfully compiled */` |
|    89329 |  333 | `	return SXRET_OK;` |
|    44667 |  334 | `}` |
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
|       26 |  370 | `				break;` |
|        - |  371 | `			}` |
|        - |  372 | `			/* Point to the upper block */` |
|      121 |  373 | `			pBlock = pBlock->pParent;` |
|        5 |  374 | `		}` |
|      117 |  375 | `		if( pBlock ){` |
|       26 |  376 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       15 |  377 | `		}else{` |
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
|       29 |  441 | `				break;` |
|        - |  442 | `			}` |
|        - |  443 | `			/* Point to the upper block */` |
|      179 |  444 | `			pBlock = pBlock->pParent;` |
|        5 |  445 | `		}` |
|      153 |  446 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|       29 |  447 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       16 |  448 | `		}else{` |
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
|      ! 0 |  515 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|      ! 0 |  516 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  517 | `				return SXERR_ABORT;` |
|      ! 0 |  518 | `			}else if( rc != SXERR_EMPTY ){` |
|      ! 0 |  519 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      ! 0 |  520 | `			}` |
|      ! 0 |  521 | `			goto Consume;` |
|        - |  522 | `		}` |
|      ! 0 |  523 | `	}else{` |
|        - |  524 | `		/* No more chunks to process */` |
|       22 |  525 | `		pGen->pIn = pGen->pEnd;` |
|       22 |  526 | `		return SXERR_EOF;` |
|        - |  527 | `	}` |
|      ! 0 |  528 | `	return SXRET_OK;` |
|       12 |  529 | `}` |
|        - |  530 | `/*` |
|        - |  531 | ` * Compile a PHP block.` |
|        - |  532 | ` * A block is simply one or more PHP statements and expressions to compile` |
|        - |  533 | ` * optionally delimited by braces {}.` |
|        - |  534 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|        - |  535 | ` * and this function takes care of generating the appropriate error` |
|        - |  536 | ` * message.` |
|        - |  537 | ` */` |
|  6917832 |  538 | `PH7_PRIVATE sxi32 PH7_CompileBlock(` |
|        - |  539 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |  540 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|        - |  541 | `	)` |
|        5 |  542 | `{` |
|        - |  543 | `	sxi32 rc;` |
|        - |  544 | `	sxu32 nLine;` |
|  6917837 |  545 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  6792675 |  546 | `		nLine = pGen->pIn->nLine;` |
|  6792675 |  547 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  6792675 |  548 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  549 | `			return SXERR_ABORT;` |
|        - |  550 | `		}` |
|  6792675 |  551 | `		pGen->pIn++;` |
|        - |  552 | `		/* Compile until we hit the closing braces '}' */` |
|  9939024 |  553 | `		for(;;){` |
| 19878053 |  554 | `			if( pGen->pIn >= pGen->pEnd ){` |
|       22 |  555 | `				rc = GenStateNextChunk(&(*pGen));` |
|       22 |  556 | `				if (rc == SXERR_ABORT ){` |
|      ! 0 |  557 | `			 	   return SXERR_ABORT;` |
|        - |  558 | `				}` |
|       22 |  559 | `				if( rc == SXERR_EOF ){` |
|        - |  560 | `					/* No more token to process: the block was never closed. php reports` |
|        - |  561 | `					 * the line the '{' was opened on, not where the input ran out. */` |
|       22 |  562 | `					PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"Unclosed '{' on line %u",nLine);` |
|       22 |  563 | `					break;` |
|        - |  564 | `				}` |
|      ! 0 |  565 | `			}` |
| 19878033 |  566 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|        - |  567 | `				/* Closing braces found,break immediately*/` |
|  6792655 |  568 | `				pGen->pIn++;` |
|  6792655 |  569 | `				break;` |
|        - |  570 | `			}` |
|        - |  571 | `			/* Compile a single statement */` |
| 13085383 |  572 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
| 13085383 |  573 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  574 | `				return SXERR_ABORT;` |
|        - |  575 | `			}` |
|        5 |  576 | `		}` |
|  6792675 |  577 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  3521502 |  578 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|      ! 0 |  579 | `		pGen->pIn++;` |
|      ! 0 |  580 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|      ! 0 |  581 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  582 | `			return SXERR_ABORT;` |
|        - |  583 | `		}` |
|        - |  584 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|      ! 0 |  585 | `		for(;;){` |
|      ! 0 |  586 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  587 | `				rc = GenStateNextChunk(&(*pGen));` |
|      ! 0 |  588 | `				if (rc == SXERR_ABORT ){` |
|      ! 0 |  589 | `			 	   return SXERR_ABORT;` |
|        - |  590 | `				}` |
|      ! 0 |  591 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|        - |  592 | `					/* No more token to process */` |
|      ! 0 |  593 | `					if( rc == SXERR_EOF ){` |
|      ! 0 |  594 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|        - |  595 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|      ! 0 |  596 | `					}` |
|      ! 0 |  597 | `					break;` |
|        - |  598 | `				}` |
|      ! 0 |  599 | `			}` |
|      ! 0 |  600 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|        - |  601 | `				sxi32 nKwrd;` |
|        - |  602 | `				/* Keyword found */` |
|      ! 0 |  603 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      ! 0 |  604 | `				if( nKwrd == nKeywordEnd \|\|` |
|      ! 0 |  605 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|        - |  606 | `						/* Delimiter keyword found,break */` |
|      ! 0 |  607 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|      ! 0 |  608 | `							pGen->pIn++; /*  endif;endswitch... */` |
|      ! 0 |  609 | `						}` |
|      ! 0 |  610 | `						break;` |
|        - |  611 | `				}` |
|      ! 0 |  612 | `			}` |
|        - |  613 | `			/* Compile a single statement */` |
|      ! 0 |  614 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|      ! 0 |  615 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  616 | `				return SXERR_ABORT;` |
|        - |  617 | `			}` |
|      ! 0 |  618 | `		}` |
|      ! 0 |  619 | `		GenStateLeaveBlock(&(*pGen),0);` |
|      ! 0 |  620 | `	}else{` |
|        - |  621 | `		/* Compile a single statement */` |
|   125167 |  622 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|   125167 |  623 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  624 | `			return SXERR_ABORT;` |
|        - |  625 | `		}` |
|        - |  626 | `	}` |
|        - |  627 | `	/* Jump trailing semi-colons ';' */` |
|  6917837 |  628 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      ! 0 |  629 | `		pGen->pIn++;` |
|      ! 0 |  630 | `	}` |
|  6917837 |  631 | `	return SXRET_OK;` |
|  3458921 |  632 | `}` |
|        - |  633 | `/*` |
|        - |  634 | ` * Compile the gentle 'while' statement.` |
|        - |  635 | ` * According to the PHP language reference` |
|        - |  636 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|        - |  637 | ` *  The basic form of a while statement is:` |
|        - |  638 | ` *  while (expr)` |
|        - |  639 | ` *   statement` |
|        - |  640 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|        - |  641 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|        - |  642 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|        - |  643 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|        - |  644 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|        - |  645 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|        - |  646 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|        - |  647 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|        - |  648 | ` *  while (expr):` |
|        - |  649 | ` *    statement` |
|        - |  650 | ` *   endwhile;` |
|        - |  651 | ` */` |
|    73830 |  652 | `PH7_PRIVATE sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|        5 |  653 | `{` |
|    73835 |  654 | `	GenBlock *pWhileBlock = 0;` |
|    73835 |  655 | `	SyToken *pTmp,*pEnd = 0;` |
|        - |  656 | `	sxu32 nFalseJump;` |
|        - |  657 | `	sxu32 nLine;` |
|        - |  658 | `	sxi32 rc;` |
|    73835 |  659 | `	nLine = pGen->pIn->nLine;` |
|        - |  660 | `	/* Jump the 'while' keyword */` |
|    73835 |  661 | `	pGen->pIn++;` |
|    73835 |  662 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  663 | `		/* Syntax error */` |
|      ! 0 |  664 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|      ! 0 |  665 | `		if( rc == SXERR_ABORT ){` |
|        - |  666 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  667 | `			return SXERR_ABORT;` |
|        - |  668 | `		}` |
|      ! 0 |  669 | `		goto Synchronize;` |
|        - |  670 | `	}` |
|        - |  671 | `	/* Jump the left parenthesis '(' */` |
|    73835 |  672 | `	pGen->pIn++;` |
|        - |  673 | `	/* Create the loop block */` |
|    73835 |  674 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|    73835 |  675 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  676 | `		return SXERR_ABORT;` |
|        - |  677 | `	}` |
|        - |  678 | `	/* Delimit the condition */` |
|    73835 |  679 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    73835 |  680 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  681 | `		/* Empty expression */` |
|        3 |  682 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|        3 |  683 | `		if( rc == SXERR_ABORT ){` |
|        - |  684 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  685 | `			return SXERR_ABORT;` |
|        - |  686 | `		}` |
|        1 |  687 | `	}` |
|        - |  688 | `	/* Swap token streams */` |
|    73835 |  689 | `	pTmp = pGen->pEnd;` |
|    73835 |  690 | `	pGen->pEnd = pEnd;` |
|        - |  691 | `	/* Compile the expression */` |
|    73835 |  692 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    73835 |  693 | `	if( rc == SXERR_ABORT ){` |
|        - |  694 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  695 | `		return SXERR_ABORT;` |
|        - |  696 | `	}` |
|        - |  697 | `	/* Update token stream */` |
|    73835 |  698 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 |  699 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 |  700 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  701 | `			return SXERR_ABORT;` |
|        - |  702 | `		}` |
|      ! 0 |  703 | `		pGen->pIn++;` |
|      ! 0 |  704 | `	}` |
|        - |  705 | `	/* Synchronize pointers */` |
|    73835 |  706 | `	pGen->pIn  = &pEnd[1];` |
|    73835 |  707 | `	pGen->pEnd = pTmp;` |
|        - |  708 | `	/* Emit the false jump */` |
|    73835 |  709 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|        - |  710 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    73835 |  711 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|        - |  712 | `	/* Compile the loop body */` |
|    73835 |  713 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|    73835 |  714 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  715 | `		return SXERR_ABORT;` |
|        - |  716 | `	}` |
|        - |  717 | `	/* Emit the unconditional jump to the start of the loop */` |
|    73835 |  718 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|        - |  719 | `	/* Fix all jumps now the destination is resolved */` |
|    73835 |  720 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  721 | `	/* Release the loop block */` |
|    73835 |  722 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  723 | `	/* Statement successfully compiled */` |
|    73835 |  724 | `	return SXRET_OK;` |
|      ! 0 |  725 | `Synchronize:` |
|        - |  726 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - |  727 | `	 * compiling this erroneous block.` |
|        - |  728 | `	 */` |
|      ! 0 |  729 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  730 | `		pGen->pIn++;` |
|      ! 0 |  731 | `	}` |
|      ! 0 |  732 | `	return SXRET_OK;` |
|    36920 |  733 | `}` |
|        - |  734 | `/*` |
|        - |  735 | ` * Compile the ugly do..while() statement.` |
|        - |  736 | ` * According to the PHP language reference` |
|        - |  737 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|        - |  738 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|        - |  739 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|        - |  740 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|        - |  741 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|        - |  742 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|        - |  743 | ` *  would end immediately).` |
|        - |  744 | ` *  There is just one syntax for do-while loops:` |
|        - |  745 | ` *  <?php` |
|        - |  746 | ` *  $i = 0;` |
|        - |  747 | ` *  do {` |
|        - |  748 | ` *   echo $i;` |
|        - |  749 | ` *  } while ($i > 0);` |
|        - |  750 | ` * ?>` |
|        - |  751 | ` */` |
|        2 |  752 | `PH7_PRIVATE sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|        1 |  753 | `{` |
|        3 |  754 | `	SyToken *pTmp,*pEnd = 0;` |
|        3 |  755 | `	GenBlock *pDoBlock = 0;` |
|        - |  756 | `	sxu32 nLine;` |
|        - |  757 | `	sxi32 rc;` |
|        3 |  758 | `	nLine = pGen->pIn->nLine;` |
|        - |  759 | `	/* Jump the 'do' keyword */` |
|        3 |  760 | `	pGen->pIn++;` |
|        - |  761 | `	/* Create the loop block */` |
|        3 |  762 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|        3 |  763 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  764 | `		return SXERR_ABORT;` |
|        - |  765 | `	}` |
|        - |  766 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|        3 |  767 | `	pDoBlock->bPostContinue = TRUE;` |
|        3 |  768 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|        3 |  769 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  770 | `		return SXERR_ABORT;` |
|        - |  771 | `	}` |
|        3 |  772 | `	if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 |  773 | `		nLine = pGen->pIn->nLine;` |
|      ! 0 |  774 | `	}` |
|        3 |  775 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|      ! 0 |  776 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|        - |  777 | `			/* Missing 'while' statement */` |
|        3 |  778 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|        3 |  779 | `			if( rc == SXERR_ABORT ){` |
|        - |  780 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  781 | `				return SXERR_ABORT;` |
|        - |  782 | `			}` |
|        3 |  783 | `			goto Synchronize;` |
|        - |  784 | `	}` |
|        - |  785 | `	/* Jump the 'while' keyword */` |
|      ! 0 |  786 | `	pGen->pIn++;` |
|      ! 0 |  787 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  788 | `		/* Syntax error */` |
|      ! 0 |  789 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|      ! 0 |  790 | `		if( rc == SXERR_ABORT ){` |
|        - |  791 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  792 | `			return SXERR_ABORT;` |
|        - |  793 | `		}` |
|      ! 0 |  794 | `		goto Synchronize;` |
|        - |  795 | `	}` |
|        - |  796 | `	/* Jump the left parenthesis '(' */` |
|      ! 0 |  797 | `	pGen->pIn++;` |
|        - |  798 | `	/* Delimit the condition */` |
|      ! 0 |  799 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      ! 0 |  800 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  801 | `		/* Empty expression */` |
|      ! 0 |  802 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|      ! 0 |  803 | `		if( rc == SXERR_ABORT ){` |
|        - |  804 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  805 | `			return SXERR_ABORT;` |
|        - |  806 | `		}` |
|      ! 0 |  807 | `		goto Synchronize;` |
|        - |  808 | `	}` |
|        - |  809 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|      ! 0 |  810 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|        - |  811 | `		JumpFixup *aPost;` |
|        - |  812 | `		VmInstr *pInstr;` |
|        - |  813 | `		sxu32 nJumpDest;` |
|        - |  814 | `		sxu32 n;` |
|      ! 0 |  815 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|      ! 0 |  816 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      ! 0 |  817 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|      ! 0 |  818 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      ! 0 |  819 | `			if( pInstr ){` |
|        - |  820 | `				/* Fix */` |
|      ! 0 |  821 | `				pInstr->iP2 = nJumpDest;` |
|      ! 0 |  822 | `			}` |
|      ! 0 |  823 | `		}` |
|      ! 0 |  824 | `	}` |
|        - |  825 | `	/* Swap token streams */` |
|      ! 0 |  826 | `	pTmp = pGen->pEnd;` |
|      ! 0 |  827 | `	pGen->pEnd = pEnd;` |
|        - |  828 | `	/* Compile the expression */` |
|      ! 0 |  829 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      ! 0 |  830 | `	if( rc == SXERR_ABORT ){` |
|        - |  831 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  832 | `		return SXERR_ABORT;` |
|        - |  833 | `	}` |
|        - |  834 | `	/* Update token stream */` |
|      ! 0 |  835 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 |  836 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 |  837 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  838 | `			return SXERR_ABORT;` |
|        - |  839 | `		}` |
|      ! 0 |  840 | `		pGen->pIn++;` |
|      ! 0 |  841 | `	}` |
|      ! 0 |  842 | `	pGen->pIn  = &pEnd[1];` |
|      ! 0 |  843 | `	pGen->pEnd = pTmp;` |
|        - |  844 | `	/* Emit the true jump to the beginning of the loop */` |
|      ! 0 |  845 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|        - |  846 | `	/* Fix all jumps now the destination is resolved */` |
|      ! 0 |  847 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  848 | `	/* Release the loop block */` |
|      ! 0 |  849 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  850 | `	/* Statement successfully compiled */` |
|      ! 0 |  851 | `	return SXRET_OK;` |
|        1 |  852 | `Synchronize:` |
|        - |  853 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - |  854 | `	 * compiling this erroneous block.` |
|        - |  855 | `	 */` |
|        3 |  856 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  857 | `		pGen->pIn++;` |
|      ! 0 |  858 | `	}` |
|        3 |  859 | `	return SXRET_OK;` |
|        2 |  860 | `}` |
|        - |  861 | `/*` |
|        - |  862 | ` * Compile the complex and powerful 'for' statement.` |
|        - |  863 | ` * According to the PHP language reference` |
|        - |  864 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|        - |  865 | ` *  The syntax of a for loop is:` |
|        - |  866 | ` *  for (expr1; expr2; expr3)` |
|        - |  867 | ` *   statement` |
|        - |  868 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|        - |  869 | ` *  the beginning of the loop.` |
|        - |  870 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|        - |  871 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|        - |  872 | ` *  to FALSE, the execution of the loop ends.` |
|        - |  873 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|        - |  874 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|        - |  875 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|        - |  876 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|        - |  877 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|        - |  878 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|        - |  879 | ` *  of using the for truth expression.` |
|        - |  880 | ` */` |
|   128122 |  881 | `PH7_PRIVATE sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|        5 |  882 | `{` |
|   128127 |  883 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   128127 |  884 | `	GenBlock *pForBlock = 0;` |
|        - |  885 | `	sxu32 nFalseJump;` |
|        - |  886 | `	sxu32 nLine;` |
|        - |  887 | `	sxi32 rc;` |
|   128127 |  888 | `	nLine = pGen->pIn->nLine;` |
|        - |  889 | `	/* Jump the 'for' keyword */` |
|   128127 |  890 | `	pGen->pIn++;` |
|   128127 |  891 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  892 | `		/* Syntax error */` |
|      ! 0 |  893 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|      ! 0 |  894 | `		if( rc == SXERR_ABORT ){` |
|        - |  895 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  896 | `			return SXERR_ABORT;` |
|        - |  897 | `		}` |
|      ! 0 |  898 | `		return SXRET_OK;` |
|        - |  899 | `	}` |
|        - |  900 | `	/* Jump the left parenthesis '(' */` |
|   128127 |  901 | `	pGen->pIn++;` |
|        - |  902 | `	/* Delimit the init-expr;condition;post-expr */` |
|   128127 |  903 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   128127 |  904 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  905 | `		/* Empty expression */` |
|      ! 0 |  906 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|      ! 0 |  907 | `		if( rc == SXERR_ABORT ){` |
|        - |  908 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  909 | `			return SXERR_ABORT;` |
|        - |  910 | `		}` |
|        - |  911 | `		/* Synchronize */` |
|      ! 0 |  912 | `		pGen->pIn = pEnd;` |
|      ! 0 |  913 | `		if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 |  914 | `			pGen->pIn++;` |
|      ! 0 |  915 | `		}` |
|      ! 0 |  916 | `		return SXRET_OK;` |
|        - |  917 | `	}` |
|        - |  918 | `	/* Swap token streams */` |
|   128127 |  919 | `	pTmp = pGen->pEnd;` |
|   128127 |  920 | `	pGen->pEnd = pEnd;` |
|        - |  921 | `	/* for() clauses are the ONLY place php's grammar allows a comma-separated` |
|        - |  922 | `	 * expression list, so the comma operator is permitted for their duration` |
|        - |  923 | `	 * (see GenStateTreeHasComma). A closure body nested inside a clause is` |
|        - |  924 | `	 * compiled through this same window — recorded as a known leniency. */` |
|   128127 |  925 | `	pGen->nCommaExprOk++;` |
|        - |  926 | `	/* Compile initialization expressions if available */` |
|   128127 |  927 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  928 | `	/* Pop operand lvalues */` |
|   128127 |  929 | `	if( rc == SXERR_ABORT ){` |
|        - |  930 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  931 | `		return SXERR_ABORT;` |
|   128127 |  932 | `	}else if( rc != SXERR_EMPTY ){` |
|   116491 |  933 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    58243 |  934 | `	}` |
|   128127 |  935 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  936 | `		/* Syntax error */` |
|      ! 0 |  937 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|      ! 0 |  938 | `		if( rc == SXERR_ABORT ){` |
|        - |  939 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  940 | `			return SXERR_ABORT;` |
|        - |  941 | `		}` |
|      ! 0 |  942 | `		return SXRET_OK;` |
|        - |  943 | `	}` |
|        - |  944 | `	/* Jump the trailing ';' */` |
|   128127 |  945 | `	pGen->pIn++;` |
|        - |  946 | `	/* Create the loop block */` |
|   128127 |  947 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   128127 |  948 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  949 | `		return SXERR_ABORT;` |
|        - |  950 | `	}` |
|        - |  951 | `	/* Deffer continue jumps */` |
|   128127 |  952 | `	pForBlock->bPostContinue = TRUE;` |
|        - |  953 | `	/* Compile the condition */` |
|   128127 |  954 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   128127 |  955 | `	if( rc == SXERR_ABORT ){` |
|        - |  956 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  957 | `		return SXERR_ABORT;` |
|   128127 |  958 | `	}else if( rc != SXERR_EMPTY ){` |
|        - |  959 | `		/* Emit the false jump */` |
|   116491 |  960 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|        - |  961 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   116491 |  962 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    58243 |  963 | `	}` |
|   128127 |  964 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  965 | `		/* Syntax error */` |
|        6 |  966 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|        6 |  967 | `		if( rc == SXERR_ABORT ){` |
|        - |  968 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  969 | `			return SXERR_ABORT;` |
|        - |  970 | `		}` |
|        6 |  971 | `		return SXRET_OK;` |
|        - |  972 | `	}` |
|        - |  973 | `	/* Jump the trailing ';' */` |
|   128123 |  974 | `	pGen->pIn++;` |
|        - |  975 | `	/* Save the post condition stream */` |
|   128123 |  976 | `	pPostStart = pGen->pIn;` |
|        - |  977 | `	/* Compile the loop body — OUTSIDE the comma window (the body is ordinary` |
|        - |  978 | ``	 * php, so `(1, 2)` inside it is the parse error it should be). */`` |
|   128123 |  979 | `	pGen->nCommaExprOk--;` |
|   128123 |  980 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   128123 |  981 | `	pGen->pEnd = pTmp;` |
|   128123 |  982 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   128123 |  983 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  984 | `		return SXERR_ABORT;` |
|        - |  985 | `	}` |
|        - |  986 | `	/* Fix post-continue jumps */` |
|   128123 |  987 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|        - |  988 | `		JumpFixup *aPost;` |
|        - |  989 | `		VmInstr *pInstr;` |
|        - |  990 | `		sxu32 nJumpDest;` |
|        - |  991 | `		sxu32 n;` |
|    11651 |  992 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|    11651 |  993 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|    38809 |  994 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|    27163 |  995 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|    27163 |  996 | `			if( pInstr ){` |
|        - |  997 | `				/* Fix jump */` |
|    27163 |  998 | `				pInstr->iP2 = nJumpDest;` |
|    13579 |  999 | `			}` |
|    13584 | 1000 | `		}` |
|     5823 | 1001 | `	}` |
|        - | 1002 | `	/* compile the post-expressions if available */` |
|   128123 | 1003 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|      ! 0 | 1004 | `		pPostStart++;` |
|      ! 0 | 1005 | `	}` |
|   128123 | 1006 | `	if( pPostStart < pEnd ){` |
|        - | 1007 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   116489 | 1008 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   116489 | 1009 | `		pGen->nCommaExprOk++; /* post-expressions are a clause list again */` |
|   116489 | 1010 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   116489 | 1011 | `		pGen->nCommaExprOk--;` |
|   116489 | 1012 | `		if( pGen->pIn < pGen->pEnd ){` |
|        - | 1013 | `			/* Syntax error */` |
|      ! 0 | 1014 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|      ! 0 | 1015 | `			if( rc == SXERR_ABORT ){` |
|        - | 1016 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 1017 | `				return SXERR_ABORT;` |
|        - | 1018 | `			}` |
|      ! 0 | 1019 | `			return SXRET_OK;` |
|        - | 1020 | `		}` |
|   116489 | 1021 | `		RE_SWAP_DELIMITER(pGen);` |
|   116489 | 1022 | `		if( rc == SXERR_ABORT ){` |
|        - | 1023 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1024 | `			return SXERR_ABORT;` |
|   116489 | 1025 | `		}else if( rc != SXERR_EMPTY){` |
|        - | 1026 | `			/* Pop operand lvalue */` |
|   116489 | 1027 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    58242 | 1028 | `		}` |
|    58242 | 1029 | `	}` |
|        - | 1030 | `	/* Emit the unconditional jump to the start of the loop */` |
|   128123 | 1031 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|        - | 1032 | `	/* Fix all jumps now the destination is resolved */` |
|   128123 | 1033 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 1034 | `	/* Release the loop block */` |
|   128123 | 1035 | `	GenStateLeaveBlock(pGen,0);` |
|        - | 1036 | `	/* Statement successfully compiled */` |
|   128123 | 1037 | `	return SXRET_OK;` |
|    64066 | 1038 | `}` |
|        - | 1039 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|        - | 1040 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|        - | 1041 | ` * are allowed.` |
|        - | 1042 | ` */` |
|   462586 | 1043 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 1044 | `{` |
|   462591 | 1045 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|   462591 | 1046 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|        - | 1047 | `		/* Unexpected expression */` |
|      ! 0 | 1048 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 1049 | `			"foreach: Expecting a variable name");` |
|      ! 0 | 1050 | `		if( rc != SXERR_ABORT ){` |
|      ! 0 | 1051 | `			rc = SXERR_INVALID;` |
|      ! 0 | 1052 | `		}` |
|      ! 0 | 1053 | `	}` |
|   462591 | 1054 | `	return rc;` |
|        5 | 1055 | `}` |
|        - | 1056 | `/*` |
|        - | 1057 | ` * Compile the 'foreach' statement.` |
|        - | 1058 | ` * According to the PHP language reference` |
|        - | 1059 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|        - | 1060 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|        - | 1061 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|        - | 1062 | ` *  is a minor but useful extension of the first:` |
|        - | 1063 | ` *  foreach (array_expression as $value)` |
|        - | 1064 | ` *    statement` |
|        - | 1065 | ` *  foreach (array_expression as $key => $value)` |
|        - | 1066 | ` *   statement` |
|        - | 1067 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|        - | 1068 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|        - | 1069 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|        - | 1070 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|        - | 1071 | ` *  to the variable $key on each loop.` |
|        - | 1072 | ` *  Note:` |
|        - | 1073 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|        - | 1074 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|        - | 1075 | ` *  Note:` |
|        - | 1076 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|        - | 1077 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|        - | 1078 | ` *  or after the foreach without resetting it.` |
|        - | 1079 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|        - | 1080 | ` *  of copying the value.` |
|        - | 1081 | ` */` |
|   326566 | 1082 | `PH7_PRIVATE sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|        5 | 1083 | `{` |
|   326571 | 1084 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|   326571 | 1085 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|   326571 | 1086 | `	GenBlock *pForeachBlock = 0;` |
|        - | 1087 | `	ph7_foreach_info *pInfo;` |
|        - | 1088 | `	sxu32 nFalseJump;` |
|        - | 1089 | `	VmInstr *pInstr;` |
|        - | 1090 | `	sxu32 nLine;` |
|        - | 1091 | `	sxi32 rc;` |
|   326571 | 1092 | `	nLine = pGen->pIn->nLine;` |
|        - | 1093 | `	/* Jump the 'foreach' keyword */` |
|   326571 | 1094 | `	pGen->pIn++;` |
|   326571 | 1095 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1096 | `		/* Syntax error */` |
|      ! 0 | 1097 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|      ! 0 | 1098 | `		if( rc == SXERR_ABORT ){` |
|        - | 1099 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1100 | `			return SXERR_ABORT;` |
|        - | 1101 | `		}` |
|      ! 0 | 1102 | `		goto Synchronize;` |
|        - | 1103 | `	}` |
|        - | 1104 | `	/* Jump the left parenthesis '(' */` |
|   326571 | 1105 | `	pGen->pIn++;` |
|        - | 1106 | `	/* Create the loop block */` |
|   326571 | 1107 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|   326571 | 1108 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1109 | `		return SXERR_ABORT;` |
|        - | 1110 | `	}` |
|        - | 1111 | `	/* Delimit the expression */` |
|   326571 | 1112 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   326571 | 1113 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - | 1114 | `		/* Empty expression */` |
|      ! 0 | 1115 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|      ! 0 | 1116 | `		if( rc == SXERR_ABORT ){` |
|        - | 1117 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1118 | `			return SXERR_ABORT;` |
|        - | 1119 | `		}` |
|        - | 1120 | `		/* Synchronize */` |
|      ! 0 | 1121 | `		pGen->pIn = pEnd;` |
|      ! 0 | 1122 | `		if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 | 1123 | `			pGen->pIn++;` |
|      ! 0 | 1124 | `		}` |
|      ! 0 | 1125 | `		return SXRET_OK;` |
|        - | 1126 | `	}` |
|        - | 1127 | `	/* Compile the array expression */` |
|   326571 | 1128 | `	pCur = pGen->pIn;` |
|  1850655 | 1129 | `	while( pCur < pEnd ){` |
|  1850655 | 1130 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|   357609 | 1131 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|   357609 | 1132 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|        - | 1133 | `				/* Break with the first 'as' found */` |
|   326571 | 1134 | `				break;` |
|        - | 1135 | `			}` |
|    15519 | 1136 | `		}` |
|        - | 1137 | `		/* Advance the stream cursor */` |
|  1524089 | 1138 | `		pCur++;` |
|        5 | 1139 | `	}` |
|   326571 | 1140 | `	if( pCur <= pGen->pIn ){` |
|      ! 0 | 1141 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 1142 | `			"foreach: Missing array/object expression");` |
|      ! 0 | 1143 | `		if( rc == SXERR_ABORT ){` |
|        - | 1144 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1145 | `			return SXERR_ABORT;` |
|        - | 1146 | `		}` |
|      ! 0 | 1147 | `		goto Synchronize;` |
|        - | 1148 | `	}` |
|        - | 1149 | `	/* Swap token streams */` |
|   326571 | 1150 | `	pTmp = pGen->pEnd;` |
|   326571 | 1151 | `	pGen->pEnd = pCur;` |
|   326571 | 1152 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   326571 | 1153 | `	if( rc == SXERR_ABORT ){` |
|        - | 1154 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1155 | `		return SXERR_ABORT;` |
|        - | 1156 | `	}` |
|        - | 1157 | `	/* Update token stream */` |
|   326571 | 1158 | `	while(pGen->pIn < pCur ){` |
|      ! 0 | 1159 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 1160 | `		if( rc == SXERR_ABORT ){` |
|        - | 1161 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1162 | `			return SXERR_ABORT;` |
|        - | 1163 | `		}` |
|      ! 0 | 1164 | `		pGen->pIn++;` |
|      ! 0 | 1165 | `	}` |
|   326571 | 1166 | `	pCur++; /* Jump the 'as' keyword */` |
|   326571 | 1167 | `	pGen->pIn = pCur;` |
|   326571 | 1168 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 1169 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|      ! 0 | 1170 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1171 | `			return SXERR_ABORT;` |
|        - | 1172 | `		}` |
|      ! 0 | 1173 | `	}` |
|        - | 1174 | `	/* Create the foreach context */` |
|   326571 | 1175 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|   326571 | 1176 | `	if( pInfo == 0 ){` |
|      ! 0 | 1177 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|      ! 0 | 1178 | `		return SXERR_ABORT;` |
|        - | 1179 | `	}` |
|        - | 1180 | `	/* Zero the structure */` |
|   326571 | 1181 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|        - | 1182 | `	/* Initialize structure fields */` |
|   326571 | 1183 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|        - | 1184 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|        - | 1185 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|        - | 1186 | `	 * '=>'. */` |
|   326571 | 1187 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|   326571 | 1188 | `	if( pCur < pEnd ){` |
|        - | 1189 | `		/* Compile the expression holding the key name */` |
|   136049 | 1190 | `		if( pGen->pIn >= pCur ){` |
|      ! 0 | 1191 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|      ! 0 | 1192 | `			if( rc == SXERR_ABORT ){` |
|        - | 1193 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1194 | `				return SXERR_ABORT;` |
|        - | 1195 | `			}` |
|      ! 0 | 1196 | `		}else{` |
|   136049 | 1197 | `			pGen->pEnd = pCur;` |
|   136049 | 1198 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|   136049 | 1199 | `			if( rc == SXERR_ABORT ){` |
|        - | 1200 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1201 | `				return SXERR_ABORT;` |
|        - | 1202 | `			}` |
|   136049 | 1203 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|   136049 | 1204 | `			if( pInstr->p3 ){` |
|        - | 1205 | `				/* Record key name */` |
|   136049 | 1206 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    68022 | 1207 | `			}` |
|   136049 | 1208 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|        - | 1209 | `		}` |
|   136049 | 1210 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    68022 | 1211 | `	}` |
|   326571 | 1212 | `	pGen->pEnd = pEnd;` |
|   326571 | 1213 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 1214 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|      ! 0 | 1215 | `		if( rc == SXERR_ABORT ){` |
|        - | 1216 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1217 | `			return SXERR_ABORT;` |
|        - | 1218 | `		}` |
|      ! 0 | 1219 | `		goto Synchronize;` |
|        - | 1220 | `	}` |
|   326571 | 1221 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|       33 | 1222 | `		pGen->pIn++;` |
|        - | 1223 | `		/* Pass by reference  */` |
|       33 | 1224 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       15 | 1225 | `	}` |
|        - | 1226 | `	/* Check if the value target is list() */` |
|   326571 | 1227 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        8 | 1228 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|        - | 1229 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|        - | 1230 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|        - | 1231 | `		 */` |
|        - | 1232 | `		static int iForeachListCnt = 0;` |
|        - | 1233 | `		char zTmp[128];` |
|        - | 1234 | `		sxu32 nLen;` |
|        - | 1235 | `		char *zDup;` |
|       10 | 1236 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|       10 | 1237 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       10 | 1238 | `		if( zDup == 0 ){` |
|      ! 0 | 1239 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1240 | `			return SXERR_ABORT;` |
|        - | 1241 | `		}` |
|       10 | 1242 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|        - | 1243 | `		/* Save list() token boundaries */` |
|       10 | 1244 | `		pListStart = pGen->pIn;` |
|        - | 1245 | `		/* Advance past list(...) — validate parentheses */` |
|       10 | 1246 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|       10 | 1247 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        3 | 1248 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|        - | 1249 | `				"foreach: Expected '(' after 'list'");` |
|        3 | 1250 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1251 | `				return SXERR_ABORT;` |
|        - | 1252 | `			}` |
|        3 | 1253 | `			goto Synchronize;` |
|        - | 1254 | `		}` |
|        7 | 1255 | `		pGen->pIn++; /* Jump '(' */` |
|        7 | 1256 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|        7 | 1257 | `		if( pListEnd >= pEnd ){` |
|      ! 0 | 1258 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1259 | `				"foreach: Missing closing ')' after list");` |
|      ! 0 | 1260 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1261 | `				return SXERR_ABORT;` |
|        - | 1262 | `			}` |
|      ! 0 | 1263 | `			goto Synchronize;` |
|        - | 1264 | `		}` |
|        7 | 1265 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|        7 | 1266 | `		pListEnd = pGen->pIn;` |
|        7 | 1267 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|   326566 | 1268 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|        - | 1269 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|        - | 1270 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|        - | 1271 | `		 */` |
|        - | 1272 | `		static int iForeachShortListCnt = 0;` |
|        - | 1273 | `		char zTmp[128];` |
|        - | 1274 | `		sxu32 nLen;` |
|        - | 1275 | `		char *zDup;` |
|       17 | 1276 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       17 | 1277 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       17 | 1278 | `		if( zDup == 0 ){` |
|      ! 0 | 1279 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1280 | `			return SXERR_ABORT;` |
|        - | 1281 | `		}` |
|       17 | 1282 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|        - | 1283 | `		/* Save [...] token boundaries */` |
|       17 | 1284 | `		pListStart = pGen->pIn;` |
|        - | 1285 | `		/* Advance past [...] */` |
|       17 | 1286 | `		pGen->pIn++; /* Jump '[' */` |
|       17 | 1287 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       17 | 1288 | `		if( pListEnd >= pEnd ){` |
|      ! 0 | 1289 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1290 | `				"foreach: Missing closing ']' after short list");` |
|      ! 0 | 1291 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1292 | `				return SXERR_ABORT;` |
|        - | 1293 | `			}` |
|      ! 0 | 1294 | `			goto Synchronize;` |
|        - | 1295 | `		}` |
|       17 | 1296 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       17 | 1297 | `		pListEnd = pGen->pIn;` |
|       17 | 1298 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|        9 | 1299 | `	}else{` |
|        - | 1300 | `		/* Compile the expression holding the value name */` |
|   326547 | 1301 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|   326547 | 1302 | `		if( rc == SXERR_ABORT ){` |
|        - | 1303 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1304 | `			return SXERR_ABORT;` |
|        - | 1305 | `		}` |
|   326547 | 1306 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|   326547 | 1307 | `		if( pInstr->p3 ){` |
|        - | 1308 | `			/* Record value name */` |
|   326547 | 1309 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|   163271 | 1310 | `		}` |
|        - | 1311 | `	}` |
|        - | 1312 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|   326569 | 1313 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|        - | 1314 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   326569 | 1315 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|        - | 1316 | `	/* Record the first instruction to execute */` |
|   326569 | 1317 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|        - | 1318 | `	/* Emit the FOREACH_STEP instruction */` |
|   326569 | 1319 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|        - | 1320 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   326569 | 1321 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|        - | 1322 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|   326569 | 1323 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|        - | 1324 | `		SyToken *pSavedIn,*pSavedEnd;` |
|        - | 1325 | `		/* Load the temporary variable holding the current value onto the stack.` |
|        - | 1326 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|        - | 1327 | `		 */` |
|       23 | 1328 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|        - | 1329 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|        - | 1330 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|        - | 1331 | `		 * picks up the delimiter and the variable names inside.` |
|        - | 1332 | `		 */` |
|       23 | 1333 | `		pSavedIn = pGen->pIn;` |
|       23 | 1334 | `		pSavedEnd = pGen->pEnd;` |
|       23 | 1335 | `		pGen->pIn = pListStart;` |
|       23 | 1336 | `		pGen->pEnd = pListEnd;` |
|       23 | 1337 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       17 | 1338 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|        9 | 1339 | `		}else{` |
|        7 | 1340 | `			rc = PH7_CompileList(&(*pGen),0);` |
|        - | 1341 | `		}` |
|       23 | 1342 | `		pGen->pIn = pSavedIn;` |
|       23 | 1343 | `		pGen->pEnd = pSavedEnd;` |
|       23 | 1344 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1345 | `			return SXERR_ABORT;` |
|        - | 1346 | `		}` |
|        - | 1347 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|       23 | 1348 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       11 | 1349 | `	}` |
|        - | 1350 | `	/* Compile the loop body */` |
|   326569 | 1351 | `	pGen->pIn = &pEnd[1];` |
|   326569 | 1352 | `	pGen->pEnd = pTmp;` |
|   326569 | 1353 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|   326569 | 1354 | `	if( rc == SXERR_ABORT ){` |
|        - | 1355 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1356 | `		return SXERR_ABORT;` |
|        - | 1357 | `	}` |
|        - | 1358 | `	/* Emit the unconditional jump to the start of the loop */` |
|   326569 | 1359 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|        - | 1360 | `	/* Fix all jumps now the destination is resolved */` |
|   326569 | 1361 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 1362 | `	/* Release the loop block */` |
|   326569 | 1363 | `	GenStateLeaveBlock(pGen,0);` |
|        - | 1364 | `	/* Statement successfully compiled */` |
|   326569 | 1365 | `	return SXRET_OK;` |
|        1 | 1366 | `Synchronize:` |
|        - | 1367 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - | 1368 | `	 * compiling this erroneous block.` |
|        - | 1369 | `	 */` |
|        3 | 1370 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 | 1371 | `		pGen->pIn++;` |
|      ! 0 | 1372 | `	}` |
|        3 | 1373 | `	return SXRET_OK;` |
|   163288 | 1374 | `}` |
|        - | 1375 | `/*` |
|        - | 1376 | ` * Compile the infamous if/elseif/else if/else statements.` |
|        - | 1377 | ` * According to the PHP language reference` |
|        - | 1378 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|        - | 1379 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|        - | 1380 | ` *  that is similar to that of C:` |
|        - | 1381 | ` *  if (expr)` |
|        - | 1382 | ` *   statement` |
|        - | 1383 | ` *  else construct:` |
|        - | 1384 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|        - | 1385 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|        - | 1386 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|        - | 1387 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|        - | 1388 | ` *   $b, and a is NOT greater than b otherwise.` |
|        - | 1389 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|        - | 1390 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|        - | 1391 | ` *  elseif` |
|        - | 1392 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|        - | 1393 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|        - | 1394 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|        - | 1395 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|        - | 1396 | ` *   than b, a equal to b or a is smaller than b:` |
|        - | 1397 | ` *   <?php` |
|        - | 1398 | ` *    if ($a > $b) {` |
|        - | 1399 | ` *     echo "a is bigger than b";` |
|        - | 1400 | ` *    } elseif ($a == $b) {` |
|        - | 1401 | ` *     echo "a is equal to b";` |
|        - | 1402 | ` *    } else {` |
|        - | 1403 | ` *     echo "a is smaller than b";` |
|        - | 1404 | ` *    }` |
|        - | 1405 | ` *    ?>` |
|        - | 1406 | ` */` |
|  2425530 | 1407 | `PH7_PRIVATE sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|        5 | 1408 | `{` |
|  2425535 | 1409 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  2425535 | 1410 | `	GenBlock *pCondBlock = 0;` |
|        - | 1411 | `	sxu32 nJumpIdx;` |
|        - | 1412 | `	sxu32 nKeyID;` |
|        - | 1413 | `	sxi32 rc;` |
|        - | 1414 | `	/* Jump the 'if' keyword */` |
|  2425535 | 1415 | `	pGen->pIn++;` |
|  2425535 | 1416 | `	pToken = pGen->pIn;` |
|        - | 1417 | `	/* Create the conditional block */` |
|  2425535 | 1418 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  2425535 | 1419 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1420 | `		return SXERR_ABORT;` |
|        - | 1421 | `	}` |
|        - | 1422 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|  1356312 | 1423 | `	for(;;){` |
|  2712629 | 1424 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1425 | `			/* Syntax error */` |
|      ! 0 | 1426 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 1427 | `				pToken--;` |
|      ! 0 | 1428 | `			}` |
|      ! 0 | 1429 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|      ! 0 | 1430 | `			if( rc == SXERR_ABORT ){` |
|        - | 1431 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 1432 | `				return SXERR_ABORT;` |
|        - | 1433 | `			}` |
|      ! 0 | 1434 | `			goto Synchronize;` |
|        - | 1435 | `		}` |
|        - | 1436 | `		/* Jump the left parenthesis '(' */` |
|  2712629 | 1437 | `		pToken++;` |
|        - | 1438 | `		/* Delimit the condition */` |
|  2712629 | 1439 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  2712629 | 1440 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|        - | 1441 | `			/* Syntax error */` |
|        6 | 1442 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 1443 | `				pToken--;` |
|      ! 0 | 1444 | `			}` |
|        6 | 1445 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|        6 | 1446 | `			if( rc == SXERR_ABORT ){` |
|        - | 1447 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 1448 | `				return SXERR_ABORT;` |
|        - | 1449 | `			}` |
|        6 | 1450 | `			goto Synchronize;` |
|        - | 1451 | `		}` |
|        - | 1452 | `		/* Swap token streams */` |
|  2712625 | 1453 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|        - | 1454 | `		/* Compile the condition */` |
|  2712625 | 1455 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 1456 | `		/* Update token stream */` |
|  2712625 | 1457 | `		while(pGen->pIn < pEnd ){` |
|      ! 0 | 1458 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 | 1459 | `			pGen->pIn++;` |
|      ! 0 | 1460 | `		}` |
|  2712625 | 1461 | `		pGen->pIn  = &pEnd[1];` |
|  2712625 | 1462 | `		pGen->pEnd = pTmp;` |
|  2712625 | 1463 | `		if( rc == SXERR_ABORT ){` |
|        - | 1464 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1465 | `			return SXERR_ABORT;` |
|        - | 1466 | `		}` |
|        - | 1467 | `		/* Emit the false jump */` |
|  2712625 | 1468 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|        - | 1469 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  2712625 | 1470 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|        - | 1471 | `		/* Compile the body */` |
|  2712625 | 1472 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  2712625 | 1473 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1474 | `			return SXERR_ABORT;` |
|        - | 1475 | `		}` |
|  2712625 | 1476 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   520183 | 1477 | `			break;` |
|        - | 1478 | `		}` |
|        - | 1479 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|  1672269 | 1480 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  1672269 | 1481 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|  1163697 | 1482 | `			break;` |
|        - | 1483 | `		}` |
|        - | 1484 | `		/* Emit the unconditional jump */` |
|   508577 | 1485 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|        - | 1486 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   508577 | 1487 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   508577 | 1488 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   306799 | 1489 | `			pToken = &pGen->pIn[1];` |
|   306799 | 1490 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    85354 | 1491 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|   110744 | 1492 | `					break;` |
|        - | 1493 | `			}` |
|    85321 | 1494 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    42658 | 1495 | `		}` |
|   287099 | 1496 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|        - | 1497 | `		/* Synchronize cursors */` |
|   287099 | 1498 | `		pToken = pGen->pIn;` |
|        - | 1499 | `		/* Fix the false jump */` |
|   287099 | 1500 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|        5 | 1501 | `	} /* For(;;) */` |
|        - | 1502 | `	/* Fix the false jump */` |
|  2425531 | 1503 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  2425531 | 1504 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|  1385170 | 1505 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|        - | 1506 | `			/* Compile the else block */` |
|   221483 | 1507 | `			pGen->pIn++;` |
|   221483 | 1508 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   221483 | 1509 | `			if( rc == SXERR_ABORT ){` |
|        - | 1510 |  |
|      ! 0 | 1511 | `				return SXERR_ABORT;` |
|        - | 1512 | `			}` |
|   110739 | 1513 | `	}` |
|  2425531 | 1514 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|        - | 1515 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  2425531 | 1516 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|        - | 1517 | `	/* Release the conditional block */` |
|  2425531 | 1518 | `	GenStateLeaveBlock(pGen,0);` |
|        - | 1519 | `	/* Statement successfully compiled */` |
|  2425531 | 1520 | `	return SXRET_OK;` |
|        2 | 1521 | `Synchronize:` |
|        - | 1522 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|        - | 1523 | `	 */` |
|       34 | 1524 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       30 | 1525 | `		pGen->pIn++;` |
|        2 | 1526 | `	}` |
|        6 | 1527 | `	return SXRET_OK;` |
|  1212770 | 1528 | `}` |
|        - | 1529 | `/*` |
|        - | 1530 | ` * Compile the global construct.` |
|        - | 1531 | ` * According to the PHP language reference` |
|        - | 1532 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|        - | 1533 | ` *  to be used in that function.` |
|        - | 1534 | ` *  Example #1 Using global` |
|        - | 1535 | ` *  <?php` |
|        - | 1536 | ` *   $a = 1;` |
|        - | 1537 | ` *   $b = 2;` |
|        - | 1538 | ` *   function Sum()` |
|        - | 1539 | ` *   {` |
|        - | 1540 | ` *    global $a, $b;` |
|        - | 1541 | ` *    $b = $a + $b;` |
|        - | 1542 | ` *   }` |
|        - | 1543 | ` *   Sum();` |
|        - | 1544 | ` *   echo $b;` |
|        - | 1545 | ` *  ?>` |
|        - | 1546 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|        - | 1547 | ` *  all references to either variable will refer to the global version. There is no limit` |
|        - | 1548 | ` *  to the number of global variables that can be manipulated by a function.` |
|        - | 1549 | ` */` |
|       36 | 1550 | `PH7_PRIVATE sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|        5 | 1551 | `{` |
|       41 | 1552 | `	SyToken *pTmp,*pNext = 0;` |
|        - | 1553 | `	sxi32 nExpr;` |
|        - | 1554 | `	sxi32 rc;` |
|        - | 1555 | `	/* Jump the 'global' keyword */` |
|       41 | 1556 | `	pGen->pIn++;` |
|       41 | 1557 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|        - | 1558 | `		/* Nothing to process */` |
|      ! 0 | 1559 | `		return SXRET_OK;` |
|        - | 1560 | `	}` |
|       41 | 1561 | `	pTmp = pGen->pEnd;` |
|       41 | 1562 | `	nExpr = 0;` |
|       87 | 1563 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       51 | 1564 | `		if( pGen->pIn < pNext ){` |
|       51 | 1565 | `			pGen->pEnd = pNext;` |
|       51 | 1566 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 | 1567 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|      ! 0 | 1568 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1569 | `					return SXERR_ABORT;` |
|        - | 1570 | `				}` |
|      ! 0 | 1571 | `			}else{` |
|       51 | 1572 | `				pGen->pIn++;` |
|       51 | 1573 | `				if( pGen->pIn >= pGen->pEnd ){` |
|        - | 1574 | `					/* Emit a warning */` |
|      ! 0 | 1575 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|      ! 0 | 1576 | `				}else{` |
|       51 | 1577 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       51 | 1578 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 1579 | `						return SXERR_ABORT;` |
|       51 | 1580 | `					}else if(rc != SXERR_EMPTY ){` |
|       51 | 1581 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|       51 | 1582 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|        - | 1583 | `							/* Variable name, not a constant */` |
|       51 | 1584 | `							pLast->iP1 = 0;` |
|       23 | 1585 | `						}` |
|       51 | 1586 | `						nExpr++;` |
|       23 | 1587 | `					}` |
|        - | 1588 | `				}` |
|        - | 1589 | `			}` |
|       23 | 1590 | `		}` |
|        - | 1591 | `		/* Next expression in the stream */` |
|       51 | 1592 | `		pGen->pIn = pNext;` |
|        - | 1593 | `		/* Jump trailing commas */` |
|       61 | 1594 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       15 | 1595 | `			pGen->pIn++;` |
|        5 | 1596 | `		}` |
|        5 | 1597 | `	}` |
|        - | 1598 | `	/* Restore token stream */` |
|       41 | 1599 | `	pGen->pEnd = pTmp;` |
|       41 | 1600 | `	if( nExpr > 0 ){` |
|        - | 1601 | `		/* Emit the uplink instruction */` |
|       41 | 1602 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|       18 | 1603 | `	}` |
|       41 | 1604 | `	return SXRET_OK;` |
|       23 | 1605 | `}` |
|        - | 1606 | `/*` |
|        - | 1607 | ` * Compile the return statement.` |
|        - | 1608 | ` * According to the PHP language reference` |
|        - | 1609 | ` *  If called from within a function, the return() statement immediately ends execution` |
|        - | 1610 | ` *  of the current function, and returns its argument as the value of the function call.` |
|        - | 1611 | ` *  return() will also end the execution of an eval() statement or script file.` |
|        - | 1612 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|        - | 1613 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|        - | 1614 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|        - | 1615 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|        - | 1616 | ` *  from within the main script file, then script execution end.` |
|        - | 1617 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|        - | 1618 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|        - | 1619 | ` *  should do so as PHP has less work to do in this case.` |
|        - | 1620 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|        - | 1621 | ` */` |
|  3616718 | 1622 | `PH7_PRIVATE sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|        5 | 1623 | `{` |
|  3616723 | 1624 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|        - | 1625 | `	sxi32 rc;` |
|  3616723 | 1626 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  3616723 | 1627 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|        - | 1628 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|        - | 1629 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|        - | 1630 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|        - | 1631 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|        - | 1632 | `	 * normally below so token processing stays consistent. */` |
|  9522243 | 1633 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|  5905525 | 1634 | `		pFuncBlock = pFuncBlock->pParent;` |
|        5 | 1635 | `	}` |
|  3616718 | 1636 | `	if( pFuncBlock && pFuncBlock->pUserData` |
|  3616709 | 1637 | `	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){` |
|        3 | 1638 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1639 | `			"A never-returning function must not return");` |
|        3 | 1640 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1641 | `			return SXERR_ABORT;` |
|        - | 1642 | `		}` |
|        1 | 1643 | `	}` |
|        - | 1644 | `	/* Jump the 'return' keyword */` |
|  3616723 | 1645 | `	pGen->pIn++;` |
|  3616723 | 1646 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - | 1647 | `		/* Compile the expression */` |
|  3519761 | 1648 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  3519761 | 1649 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1650 | `			return SXERR_ABORT;` |
|  3519761 | 1651 | `		}else if(rc != SXERR_EMPTY ){` |
|  3519761 | 1652 | `			nRet = 1;` |
|  1759878 | 1653 | `		}` |
|  1759878 | 1654 | `	}` |
|        - | 1655 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|        - | 1656 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|        - | 1657 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|        - | 1658 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|  3616723 | 1659 | `	if( pGen->bInGenerator ){` |
|     3911 | 1660 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|     3911 | 1661 | `		return SXRET_OK;` |
|        - | 1662 | `	}` |
|        - | 1663 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|        - | 1664 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|        - | 1665 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|        - | 1666 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|        - | 1667 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|  3612817 | 1668 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|  3612817 | 1669 | `	return SXRET_OK;` |
|  1808364 | 1670 | `}` |
|        - | 1671 | `/*` |
|        - | 1672 | ` * Compile a yield expression.` |
|        - | 1673 | ` * Called from the expression code generator when a yield node is encountered.` |
|        - | 1674 | ` * Handles: yield, yield $value, yield $key => $value` |
|        - | 1675 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|        - | 1676 | ` */` |
|    15898 | 1677 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|        5 | 1678 | `{` |
|        - | 1679 | `	SyToken *pTmp, *pSplit;` |
|    15903 | 1680 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|    15903 | 1681 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|        - | 1682 | `	sxi32 rc;` |
|     7949 | 1683 | `	(void)iCompileFlag;` |
|        - | 1684 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|    15903 | 1685 | `	pGen->pIn++;` |
|        - | 1686 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|        - | 1687 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|        - | 1688 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|        - | 1689 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|        - | 1690 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|    15898 | 1691 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|     7984 | 1692 | `		&& pGen->pIn->sData.nByte == 4` |
|       72 | 1693 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|       67 | 1694 | `		pGen->pIn++; /* Skip 'from' */` |
|       67 | 1695 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       67 | 1696 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1697 | `			return SXERR_ABORT;` |
|        - | 1698 | `		}` |
|       67 | 1699 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 | 1700 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|      ! 0 | 1701 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|        - | 1702 | `				"Missing expression after 'yield from'");` |
|      ! 0 | 1703 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1704 | `				return SXERR_ABORT;` |
|        - | 1705 | `			}` |
|      ! 0 | 1706 | `		}` |
|       67 | 1707 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|       67 | 1708 | `		return SXRET_OK;` |
|        - | 1709 | `	}` |
|    15841 | 1710 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - | 1711 | `		/* Bare yield — no value */` |
|        3 | 1712 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|        3 | 1713 | `		return SXRET_OK;` |
|        - | 1714 | `	}` |
|        - | 1715 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|    15839 | 1716 | `	pSplit = 0;` |
|        - | 1717 | `	{` |
|    15839 | 1718 | `		SyToken *pCur = pGen->pIn;` |
|    15839 | 1719 | `		sxi32 nNest = 0;` |
|    47321 | 1720 | `		while( pCur < pGen->pEnd ){` |
|    47013 | 1721 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       18 | 1722 | `				nNest++;` |
|    47005 | 1723 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       18 | 1724 | `				nNest--;` |
|    46989 | 1725 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|    15531 | 1726 | `				pSplit = pCur;` |
|    15531 | 1727 | `				break;` |
|        - | 1728 | `			}` |
|    31487 | 1729 | `			pCur++;` |
|        5 | 1730 | `		}` |
|        - | 1731 | `	}` |
|    15839 | 1732 | `	pTmp = pGen->pEnd;` |
|    15839 | 1733 | `	if( pSplit ){` |
|        - | 1734 | `		/* yield $key => $value */` |
|    15531 | 1735 | `		pGen->pEnd = pSplit;` |
|    15531 | 1736 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|    15531 | 1737 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|    15531 | 1738 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|    15531 | 1739 | `		pGen->pEnd = pTmp;` |
|    15531 | 1740 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|    15531 | 1741 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|    15531 | 1742 | `		iP1 = 1;` |
|    15531 | 1743 | `		iP2 = 1;` |
|     7768 | 1744 | `	}else{` |
|        - | 1745 | `		/* yield $value */` |
|      313 | 1746 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      313 | 1747 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      313 | 1748 | `		if( rc != SXERR_EMPTY ){` |
|      313 | 1749 | `			iP1 = 1;` |
|      154 | 1750 | `		}` |
|        - | 1751 | `	}` |
|    15839 | 1752 | `	pGen->pEnd = pTmp;` |
|    15839 | 1753 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|    15839 | 1754 | `	return SXRET_OK;` |
|     7954 | 1755 | `}` |
|        - | 1756 | `/*` |
|        - | 1757 | ` * Compile the die/exit language construct.` |
|        - | 1758 | ` * The role of these constructs is to terminate execution of the script.` |
|        - | 1759 | ` * Shutdown functions will always be executed even if exit() is called.` |
|        - | 1760 | ` */` |
|      124 | 1761 | `PH7_PRIVATE sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|        5 | 1762 | `{` |
|      129 | 1763 | `	sxi32 nExpr = 0;` |
|        - | 1764 | `	sxi32 rc;` |
|        - | 1765 | `	/* Jump the die/exit keyword */` |
|      129 | 1766 | `	pGen->pIn++;` |
|      129 | 1767 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - | 1768 | `		/* Compile the expression */` |
|      129 | 1769 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      129 | 1770 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1771 | `			return SXERR_ABORT;` |
|      129 | 1772 | `		}else if(rc != SXERR_EMPTY ){` |
|      129 | 1773 | `			nExpr = 1;` |
|       62 | 1774 | `		}` |
|       62 | 1775 | `	}` |
|        - | 1776 | `	/* Emit the HALT instruction */` |
|      129 | 1777 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|      129 | 1778 | `	return SXRET_OK;` |
|       67 | 1779 | `}` |
|        - | 1780 | `/*` |
|        - | 1781 | ` * Compile the 'echo' language construct.` |
|        - | 1782 | ` */` |
|    17538 | 1783 | `PH7_PRIVATE sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|        5 | 1784 | `{` |
|    17543 | 1785 | `	SyToken *pTmp,*pNext = 0;` |
|    17543 | 1786 | `	sxu32 nLine = pGen->pIn->nLine;` |
|    17543 | 1787 | `	int nExpr = 0;      /* expressions actually compiled */` |
|    17543 | 1788 | `	int bExpectMore = 1;/* after 'echo' or a comma an expression is REQUIRED */` |
|        - | 1789 | `	sxi32 rc;` |
|        - | 1790 | `	/* Jump the 'echo' keyword */` |
|    17543 | 1791 | `	pGen->pIn++;` |
|        - | 1792 | `	/* Compile arguments one after one */` |
|    17543 | 1793 | `	pTmp = pGen->pEnd;` |
|    45101 | 1794 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|    27569 | 1795 | `		if( pGen->pIn < pNext ){` |
|    27569 | 1796 | `			pGen->pEnd = pNext;` |
|    27569 | 1797 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|    27569 | 1798 | `			if( rc == SXERR_ABORT ){` |
|        6 | 1799 | `				return SXERR_ABORT;` |
|    27565 | 1800 | `			}else if( rc != SXERR_EMPTY ){` |
|        - | 1801 | `				/* Emit the consume instruction */` |
|    27539 | 1802 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    27539 | 1803 | `				nExpr++;` |
|    27539 | 1804 | `				bExpectMore = 0;` |
|    13767 | 1805 | `			}` |
|    13780 | 1806 | `		}` |
|        - | 1807 | `		/* Jump trailing commas (php: exactly one between expressions; a` |
|        - | 1808 | `		 * dangling or doubled comma is a parse error, enforced below) */` |
|    37597 | 1809 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|    10039 | 1810 | `			if( bExpectMore ){` |
|        - | 1811 | `				/* two commas in a row */` |
|        3 | 1812 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,pNext->nLine,` |
|        - | 1813 | `					"syntax error, unexpected token \",\"");` |
|        3 | 1814 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1815 | `			}` |
|    10037 | 1816 | `			bExpectMore = 1;` |
|    10037 | 1817 | `			pNext++;` |
|        5 | 1818 | `		}` |
|    27563 | 1819 | `		pGen->pIn = pNext;` |
|        5 | 1820 | `	}` |
|        - | 1821 | `	/* Restore token stream */` |
|    17537 | 1822 | `	pGen->pEnd = pTmp;` |
|    17537 | 1823 | `	if( nExpr == 0 \|\| bExpectMore ){` |
|        - | 1824 | ``		/* `echo ;` or `echo expr, ;` — php rejects both */`` |
|       34 | 1825 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - | 1826 | `			"syntax error, unexpected token \";\"");` |
|       34 | 1827 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1828 | `	}` |
|    17507 | 1829 | `	return SXRET_OK;` |
|     8774 | 1830 | `}` |
|        - | 1831 | `/*` |
|        - | 1832 | ` * Compile the static statement.` |
|        - | 1833 | ` * According to the PHP language reference` |
|        - | 1834 | ` *  Another important feature of variable scoping is the static variable.` |
|        - | 1835 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|        - | 1836 | ` *  when program execution leaves this scope.` |
|        - | 1837 | ` *  Static variables also provide one way to deal with recursive functions.` |
|        - | 1838 | ` * Symisc eXtension.` |
|        - | 1839 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|        - | 1840 | ` *  the zend engine would allow only simple scalar value.` |
|        - | 1841 | ` *  Example` |
|        - | 1842 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|        - | 1843 | ` *    Refer to the official documentation for more information on this feature.` |
|        - | 1844 | ` */` |
|    11646 | 1845 | `PH7_PRIVATE sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|        5 | 1846 | `{` |
|        - | 1847 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|        - | 1848 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|        - | 1849 | `	GenBlock *pBlock;` |
|        - | 1850 | `	SyString *pName;` |
|        - | 1851 | `	char *zDup;` |
|        - | 1852 | `	sxu32 nLine;` |
|        - | 1853 | `	sxi32 rc;` |
|        - | 1854 | ``	/* `static function () {}` / `static fn () =>` at statement position is an`` |
|        - | 1855 | `	 * EXPRESSION statement (a bare static closure), not a static-variable` |
|        - | 1856 | `	 * declaration — hand it to the expression compiler (php accepts it). */` |
|    11646 | 1857 | `	if( &pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & PH7_TK_KEYWORD)` |
|     5829 | 1858 | `	 && (SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FUNCTION` |
|        1 | 1859 | `	  \|\| SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FN) ){` |
|        3 | 1860 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        3 | 1861 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1862 | `			return SXERR_ABORT;` |
|        3 | 1863 | `		}else if( rc != SXERR_EMPTY ){` |
|        3 | 1864 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        1 | 1865 | `		}` |
|        3 | 1866 | `		return SXRET_OK;` |
|        - | 1867 | `	}` |
|        - | 1868 | `	/* Jump the static keyword */` |
|    11649 | 1869 | `	nLine = pGen->pIn->nLine;` |
|    11649 | 1870 | `	pGen->pIn++;` |
|        - | 1871 | `	/* Extract the enclosing function if any */` |
|    11649 | 1872 | `	pBlock = pGen->pCurrent;` |
|    23293 | 1873 | `	while( pBlock ){` |
|    23293 | 1874 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|    11649 | 1875 | `			break;` |
|        - | 1876 | `		}` |
|        - | 1877 | `		/* Point to the upper block */` |
|    11649 | 1878 | `		pBlock = pBlock->pParent;` |
|        5 | 1879 | `	}` |
|    11649 | 1880 | `	if( pBlock == 0 ){` |
|        - | 1881 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|      ! 0 | 1882 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 | 1883 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|      ! 0 | 1884 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1885 | `				return SXERR_ABORT;` |
|        - | 1886 | `			}` |
|      ! 0 | 1887 | `			goto Synchronize;` |
|        - | 1888 | `		}` |
|        - | 1889 | `		/* Compile the expression holding the variable */` |
|      ! 0 | 1890 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      ! 0 | 1891 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1892 | `			return SXERR_ABORT;` |
|      ! 0 | 1893 | `		}else if( rc != SXERR_EMPTY ){` |
|        - | 1894 | `			/* Emit the POP instruction */` |
|      ! 0 | 1895 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      ! 0 | 1896 | `		}` |
|      ! 0 | 1897 | `		return SXRET_OK;` |
|        - | 1898 | `	}` |
|    11649 | 1899 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        - | 1900 | `	/* Make sure we are dealing with a valid statement */` |
|    11649 | 1901 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|    11642 | 1902 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        3 | 1903 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|        3 | 1904 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1905 | `				return SXERR_ABORT;` |
|        - | 1906 | `			}` |
|        3 | 1907 | `			goto Synchronize;` |
|        - | 1908 | `	}` |
|    11647 | 1909 | `	pGen->pIn++;` |
|        - | 1910 | `	/* Extract variable name */` |
|    11647 | 1911 | `	pName = &pGen->pIn->sData;` |
|    11647 | 1912 | `	pGen->pIn++; /* Jump the var name */` |
|    11647 | 1913 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|      ! 0 | 1914 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 1915 | `		goto Synchronize;` |
|        - | 1916 | `	}` |
|        - | 1917 | `	/* Initialize the structure describing the static variable */` |
|    11647 | 1918 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|    11647 | 1919 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|        - | 1920 | `	/* Duplicate variable name */` |
|    11647 | 1921 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    11647 | 1922 | `	if( zDup == 0 ){` |
|      ! 0 | 1923 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1924 | `		return SXERR_ABORT;` |
|        - | 1925 | `	}` |
|    11647 | 1926 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|        - | 1927 | `	/* Check if we have an expression to compile */` |
|    11647 | 1928 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|        - | 1929 | `		SySet *pInstrContainer;` |
|        - | 1930 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|        - | 1931 | `		 * Static variable can take any complex expression including function` |
|        - | 1932 | `		 * call as their initialization value.` |
|        - | 1933 | `		 * Example:` |
|        - | 1934 | `		 *		static $var = foo(1,4+5,bar());` |
|        - | 1935 | `		 */` |
|    11647 | 1936 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|        - | 1937 | `		/* Swap bytecode container */` |
|    11647 | 1938 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    11647 | 1939 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|        - | 1940 | `		/* Compile the expression */` |
|    11647 | 1941 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 1942 | `		/* Emit the done instruction */` |
|    11647 | 1943 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        - | 1944 | `		/* Restore default bytecode container */` |
|    11647 | 1945 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     5821 | 1946 | `	}` |
|        - | 1947 | `	/* Finally save the compiled static variable in the appropriate container */` |
|    11647 | 1948 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|    11647 | 1949 | `	return SXRET_OK;` |
|        1 | 1950 | `Synchronize:` |
|        - | 1951 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|        - | 1952 | `	 * statement.` |
|        - | 1953 | `	 */` |
|        5 | 1954 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|        3 | 1955 | `		pGen->pIn++;` |
|        1 | 1956 | `	}` |
|        3 | 1957 | `	return SXRET_OK;` |
|     5828 | 1958 | `}` |
|        - | 1959 | `/*` |
|        - | 1960 | ` * Compile the var statement.` |
|        - | 1961 | ` * Symisc Extension:` |
|        - | 1962 | ` *      var statement can be used outside of a class definition.` |
|        - | 1963 | ` */` |
|        4 | 1964 | `PH7_PRIVATE sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|        1 | 1965 | `{` |
|        - | 1966 | `	sxu32 nLine;` |
|        - | 1967 | `	sxi32 rc;` |
|        5 | 1968 | `	nLine = pGen->pIn->nLine;` |
|        - | 1969 | `	/* Jump the 'var' keyword */` |
|        5 | 1970 | `	pGen->pIn++;` |
|        5 | 1971 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|      ! 0 | 1972 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|        - | 1973 | `		/* Synchronize with the first semi-colon */` |
|      ! 0 | 1974 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|      ! 0 | 1975 | `			pGen->pIn++;` |
|      ! 0 | 1976 | `		}` |
|      ! 0 | 1977 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1978 | `			return SXERR_ABORT;` |
|        - | 1979 | `		}` |
|      ! 0 | 1980 | `	}else{` |
|        - | 1981 | `		/* Compile the expression */` |
|        5 | 1982 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        5 | 1983 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1984 | `			return SXERR_ABORT;` |
|        5 | 1985 | `		}else if( rc != SXERR_EMPTY ){` |
|        5 | 1986 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        2 | 1987 | `		}` |
|        - | 1988 | `	}` |
|        5 | 1989 | `	return SXRET_OK;` |
|        3 | 1990 | `}` |
|        - | 1991 | `/*` |
|        - | 1992 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|        - | 1993 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|        - | 1994 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|        - | 1995 | ` */` |
|        - | 1996 | `/*` |
|        - | 1997 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|        - | 1998 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|        - | 1999 | ` * hash and any shared references), this creates a new literal entry with the` |
|        - | 2000 | ` * qualified name and updates the instruction's operand index.` |
|        - | 2001 | ` *` |
|        - | 2002 | ` * Resolution order:` |
|        - | 2003 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|        - | 2004 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|        - | 2005 | ` *   3. Otherwise return the original literal index unchanged.` |
|        - | 2006 | ` *` |
|        - | 2007 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|        - | 2008 | ` * came from an import (step 1) and 0 otherwise.` |
|        - | 2009 | ` * Returns the (possibly new) literal index.` |
|        - | 2010 | ` */` |
|  6428294 | 2011 | `PH7_PRIVATE sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|        5 | 2012 | `{` |
|        - | 2013 | `	ph7_value *pLit;` |
|        - | 2014 | `	const char *zLit;` |
|        - | 2015 | `	SyString sQualified;` |
|        - | 2016 | `	sxu32 nLit;` |
|        - | 2017 | `	sxu32 k;` |
|        - | 2018 | `	sxu32 nNewIdx;` |
|        - | 2019 | `	int hasNsSep;` |
|        - | 2020 | `	SyHashEntry *pImport;` |
|        - | 2021 | `	ph7_value *pNew;` |
|  6428299 | 2022 | `	if( pFromImport ){` |
|  5241309 | 2023 | `		*pFromImport = 0;` |
|  2620652 | 2024 | `	}` |
|  6428299 | 2025 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  6428299 | 2026 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|      ! 0 | 2027 | `		return nOrigIdx;` |
|        - | 2028 | `	}` |
|  6428299 | 2029 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  6428299 | 2030 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|        - | 2031 | `	/* Skip if already qualified (contains backslash) */` |
|  6428299 | 2032 | `	hasNsSep = 0;` |
| 77732783 | 2033 | `	for( k = 0; k < nLit; k++ ){` |
| 71304515 | 2034 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 35652247 | 2035 | `	}` |
|  6428299 | 2036 | `	if( hasNsSep ){` |
|       28 | 2037 | `		return nOrigIdx;` |
|        - | 2038 | `	}` |
|        - | 2039 | `	/* Check use imports first (works even outside namespaces) */` |
|  6428273 | 2040 | `	SyBlobReset(&pGen->sWorker);` |
|  6428273 | 2041 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  6428273 | 2042 | `	if( pImport ){` |
|       41 | 2043 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       41 | 2044 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|       41 | 2045 | `		if( pFromImport ){` |
|       18 | 2046 | `			*pFromImport = 1;` |
|        8 | 2047 | `		}` |
|       23 | 2048 | `	}else{` |
|  6428237 | 2049 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  6428099 | 2050 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|        - | 2051 | `		}` |
|        - | 2052 | `		/* Prepend current namespace */` |
|      143 | 2053 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      143 | 2054 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      143 | 2055 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|        - | 2056 | `	}` |
|        - | 2057 | `	/* Look up or create a new literal for the qualified name */` |
|      179 | 2058 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|      179 | 2059 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|       79 | 2060 | `		return nNewIdx; /* Already interned */` |
|        - | 2061 | `	}` |
|      105 | 2062 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      105 | 2063 | `	if( pNew == 0 ){` |
|      ! 0 | 2064 | `		return nOrigIdx; /* OOM, fall back to original */` |
|        - | 2065 | `	}` |
|      105 | 2066 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      105 | 2067 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      105 | 2068 | `	return nNewIdx;` |
|  3214152 | 2069 | `}` |
|        - | 2070 | `/*` |
|        - | 2071 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|        - | 2072 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|        - | 2073 | ` */` |
|   540930 | 2074 | `PH7_PRIVATE void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|        5 | 2075 | `{` |
|        - | 2076 | `	SyHashEntry *pImport;` |
|   540935 | 2077 | `	const char *zName = pName->zString;` |
|   540935 | 2078 | `	sxu32 nName = pName->nByte;` |
|   540935 | 2079 | `	sxu32 nFirst = 0;` |
|        - | 2080 | `	/* php resolves a name through use-imports on its LEADING segment: an` |
|        - | 2081 | ``	 * unqualified `C` maps the whole name (`use X\C;` -> X\C), while a QUALIFIED`` |
|        - | 2082 | ``	 * `A\B\C` maps just `A` (`use X\A;` -> X\A\B\C) and keeps the `\B\C` tail.`` |
|        - | 2083 | `	 * The old code looked up the whole qualified string (which never matches a` |
|        - | 2084 | `	 * single-segment import alias) and then blindly prefixed the current` |
|        - | 2085 | ``	 * namespace, so `use PHPUnit\Framework; extends Framework\TestCase` resolved`` |
|        - | 2086 | `	 * to Ns\Framework\TestCase. Mirror GenStateResolveNamespaceLiteral here. */` |
|  6929921 | 2087 | `	while( nFirst < nName && zName[nFirst] != '\\' ){ nFirst++; }` |
|   540935 | 2088 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)zName,nFirst);` |
|   540935 | 2089 | `	if( pImport ){` |
|       26 | 2090 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       26 | 2091 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|       26 | 2092 | `		SyBlobAppend(pOut,zName + nFirst,nName - nFirst); /* the \B\C tail, if any */` |
|       26 | 2093 | `		return;` |
|        - | 2094 | `	}` |
|        - | 2095 | `	/* Prepend current namespace if active */` |
|   540913 | 2096 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       14 | 2097 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       14 | 2098 | `		SyBlobAppend(pOut,"\\",1);` |
|        6 | 2099 | `	}` |
|   540913 | 2100 | `	SyBlobAppend(pOut,zName,nName);` |
|   270470 | 2101 | `}` |
|        - | 2102 | `/*` |
|        - | 2103 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|        - | 2104 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|        - | 2105 | ` * The caller must release pOut when done.` |
|        - | 2106 | ` */` |
|   517952 | 2107 | `PH7_PRIVATE void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|        5 | 2108 | `{` |
|   517957 | 2109 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     3971 | 2110 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     3971 | 2111 | `		SyBlobAppend(pOut,"\\",1);` |
|     1983 | 2112 | `	}` |
|   517957 | 2113 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   517957 | 2114 | `}` |
|        - | 2115 | `/*` |
|        - | 2116 | ` * Compile a namespace statement` |
|        - | 2117 | ` * According to the PHP language reference manual` |
|        - | 2118 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|        - | 2119 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|        - | 2120 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|        - | 2121 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|        - | 2122 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|        - | 2123 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|        - | 2124 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|        - | 2125 | ` *  programming world.` |
|        - | 2126 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|        - | 2127 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|        - | 2128 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|        - | 2129 | ` *  classes/functions/constants.` |
|        - | 2130 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|        - | 2131 | ` *  readability of source code.` |
|        - | 2132 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|        - | 2133 | ` *  Here is an example of namespace syntax in PHP:` |
|        - | 2134 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|        - | 2135 | ` *       class MyClass {}` |
|        - | 2136 | ` *       function myfunction() {}` |
|        - | 2137 | ` *       const MYCONST = 1;` |
|        - | 2138 | ` *       $a = new MyClass;` |
|        - | 2139 | ` *       $c = new \my\name\MyClass;` |
|        - | 2140 | ` *       $a = strlen('hi');` |
|        - | 2141 | ` *       $d = namespace\MYCONST;` |
|        - | 2142 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|        - | 2143 | ` *       echo constant($d);` |
|        - | 2144 | ` * NOTE` |
|        - | 2145 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|        - | 2146 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|        - | 2147 | ` */` |
|        - | 2148 | `/*` |
|        - | 2149 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|        - | 2150 | ` */` |
|       14 | 2151 | `PH7_PRIVATE const char * TokenTypeName(sxu32 nType)` |
|        4 | 2152 | `{` |
|       18 | 2153 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|       11 | 2154 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|       11 | 2155 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|       11 | 2156 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|       11 | 2157 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|       11 | 2158 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|        3 | 2159 | `	return "token";` |
|       11 | 2160 | `}` |
|     4010 | 2161 | `PH7_PRIVATE sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|        5 | 2162 | `{` |
|        - | 2163 | `	sxu32 nLine;` |
|        - | 2164 | `	sxi32 rc;` |
|     4015 | 2165 | `	nLine = pGen->pIn->nLine;` |
|     4015 | 2166 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|        - | 2167 | `	/* Reset namespace and clear previous use imports */` |
|     4015 | 2168 | `	SyBlobReset(&pGen->sNamespace);` |
|     4015 | 2169 | `	SyHashRelease(&pGen->hUseImports);` |
|     4015 | 2170 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|     4015 | 2171 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     4015 | 2172 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|     4015 | 2173 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     4015 | 2174 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|     4015 | 2175 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - | 2176 | `		/* Global namespace (bare "namespace;") */` |
|      ! 0 | 2177 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|      ! 0 | 2178 | `		return SXRET_OK;` |
|        - | 2179 | `	}` |
|     4015 | 2180 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|        - | 2181 | `		/* namespace; — switch to global namespace */` |
|      ! 0 | 2182 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|      ! 0 | 2183 | `		return SXRET_OK;` |
|        - | 2184 | `	}` |
|     4015 | 2185 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|        - | 2186 | `		/* namespace { } — global namespace block */` |
|        5 | 2187 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|        5 | 2188 | `		return SXRET_OK;` |
|        - | 2189 | `	}` |
|        - | 2190 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     8099 | 2191 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     4093 | 2192 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|        - | 2193 | `			/* Append backslash separator */` |
|       47 | 2194 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       47 | 2195 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|       21 | 2196 | `			}` |
|       26 | 2197 | `		}else{` |
|        - | 2198 | `			/* Append identifier */` |
|     4051 | 2199 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        - | 2200 | `		}` |
|     4093 | 2201 | `		pGen->pIn++;` |
|        5 | 2202 | `	}` |
|        - | 2203 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|        - | 2204 | `	 * at the correct program counter, not just the last one compiled. */` |
|        - | 2205 | `	{` |
|     4011 | 2206 | `		char *zNsDup = 0;` |
|     4011 | 2207 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     6011 | 2208 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     4004 | 2209 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     2002 | 2210 | `		}` |
|     4011 | 2211 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|        - | 2212 | `	}` |
|     4011 | 2213 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        8 | 2214 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - | 2215 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|        4 | 2216 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        6 | 2217 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2218 | `			return SXERR_ABORT;` |
|        - | 2219 | `		}` |
|        2 | 2220 | `	}` |
|     4011 | 2221 | `	return SXRET_OK;` |
|     2010 | 2222 | `}` |
|        - | 2223 | `/*` |
|        - | 2224 | ` * Compile the 'use' statement` |
|        - | 2225 | ` * According to the PHP language reference manual` |
|        - | 2226 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|        - | 2227 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|        - | 2228 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|        - | 2229 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|        - | 2230 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|        - | 2231 | ` *  a function or constant is not supported.` |
|        - | 2232 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|        - | 2233 | ` * NOTE` |
|        - | 2234 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|        - | 2235 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|        - | 2236 | ` */` |
|       80 | 2237 | `PH7_PRIVATE sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|        5 | 2238 | `{` |
|        - | 2239 | `	sxu32 nLine;` |
|        - | 2240 | `	sxi32 rc;` |
|        - | 2241 | `	SyBlob sPath;` |
|        - | 2242 | `	SyString sAlias;` |
|        - | 2243 | `	SyToken *pLast;` |
|        - | 2244 | `	char *zDup;` |
|        - | 2245 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|        - | 2246 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|        - | 2247 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|       85 | 2248 | `	nLine = pGen->pIn->nLine;` |
|       85 | 2249 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|        - | 2250 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|       85 | 2251 | `	iUseType = 0;` |
|       85 | 2252 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       30 | 2253 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|       30 | 2254 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|       16 | 2255 | `			iUseType = 1;` |
|       16 | 2256 | `			pGen->pIn++;` |
|       23 | 2257 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|       16 | 2258 | `			iUseType = 2;` |
|       16 | 2259 | `			pGen->pIn++;` |
|        7 | 2260 | `		}` |
|       14 | 2261 | `	}` |
|        - | 2262 | `	/* Select target hash tables based on import type */` |
|       85 | 2263 | `	switch( iUseType ){` |
|        7 | 2264 | `		case 1:` |
|       16 | 2265 | `			pGenHash = &pGen->hUseFuncImports;` |
|       16 | 2266 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|       16 | 2267 | `			break;` |
|        7 | 2268 | `		case 2:` |
|       16 | 2269 | `			pGenHash = &pGen->hUseConstImports;` |
|       16 | 2270 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|       16 | 2271 | `			break;` |
|       26 | 2272 | `		default:` |
|       57 | 2273 | `			pGenHash = &pGen->hUseImports;` |
|       57 | 2274 | `			pVmHash = &pGen->pVm->hUseImports;` |
|       52 | 2275 | `			break;` |
|        - | 2276 | `	}` |
|       85 | 2277 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|        - | 2278 | `	/* Process one or more use declarations separated by commas */` |
|       41 | 2279 | `	for(;;){` |
|       87 | 2280 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 2281 | `			break;` |
|        - | 2282 | `		}` |
|       87 | 2283 | `		SyBlobReset(&sPath);` |
|       87 | 2284 | `		pLast = 0;` |
|        - | 2285 | `		/* Collect the full namespace path */` |
|      301 | 2286 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|      219 | 2287 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|      151 | 2288 | `				pLast = pGen->pIn;` |
|      151 | 2289 | `				if( SyBlobLength(&sPath) > 0 ){` |
|       73 | 2290 | `					SyBlobAppend(&sPath,"\\",1);` |
|       34 | 2291 | `				}` |
|      151 | 2292 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       73 | 2293 | `			}` |
|      219 | 2294 | `			pGen->pIn++;` |
|        5 | 2295 | `		}` |
|       87 | 2296 | `		if( pLast == 0 ){` |
|        - | 2297 | `			/* Empty path */` |
|        6 | 2298 | `			break;` |
|        - | 2299 | `		}` |
|        - | 2300 | `		/* Default alias is the last component of the path */` |
|       83 | 2301 | `		sAlias = pLast->sData;` |
|        - | 2302 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|       78 | 2303 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       56 | 2304 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|       27 | 2305 | `			pGen->pIn++; /* Jump 'as' */` |
|       27 | 2306 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|       27 | 2307 | `				sAlias = pGen->pIn->sData;` |
|       27 | 2308 | `				pGen->pIn++;` |
|       12 | 2309 | `			}` |
|       12 | 2310 | `		}` |
|        - | 2311 | `		/* Check for duplicate import alias (per-type) */` |
|       83 | 2312 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|        8 | 2313 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 2314 | `				"Cannot use %.*s as %z because the name is already in use",` |
|        4 | 2315 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|        6 | 2316 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2317 | `				SyBlobRelease(&sPath);` |
|      ! 0 | 2318 | `				return SXERR_ABORT;` |
|        - | 2319 | `			}` |
|        2 | 2320 | `		}` |
|        - | 2321 | `		/* Register the import: alias -> FQN.` |
|        - | 2322 | `		 * Strings are allocated from the VM pool allocator and freed` |
|        - | 2323 | `		 * when the entire VM is released. SyHashRelease does not free` |
|        - | 2324 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|      122 | 2325 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       78 | 2326 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|       83 | 2327 | `		if( zDup ){` |
|       83 | 2328 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|       83 | 2329 | `			if( pVmHash ){` |
|        - | 2330 | `				/* Class imports: populate VM table directly (class resolution` |
|        - | 2331 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|       55 | 2332 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       55 | 2333 | `				if( zAliasDup ){` |
|       55 | 2334 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|       25 | 2335 | `				}` |
|       25 | 2336 | `			}` |
|       83 | 2337 | `			if( iUseType == 2 ){` |
|        - | 2338 | `				/* Const imports: emit a runtime instruction so imports are` |
|        - | 2339 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|       16 | 2340 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       16 | 2341 | `				if( zAliasDup ){` |
|        - | 2342 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|        - | 2343 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|        - | 2344 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|       16 | 2345 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|       16 | 2346 | `					if( azPair ){` |
|       16 | 2347 | `						azPair[0] = zAliasDup;` |
|       16 | 2348 | `						azPair[1] = zDup;` |
|       16 | 2349 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|        7 | 2350 | `					}` |
|        7 | 2351 | `				}` |
|        7 | 2352 | `			}` |
|       39 | 2353 | `		}` |
|        - | 2354 | `		/* Check for comma (multiple use declarations) */` |
|       83 | 2355 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|        3 | 2356 | `			pGen->pIn++;` |
|        2 | 2357 | `		}else{` |
|       43 | 2358 | `			break;` |
|        - | 2359 | `		}` |
|        1 | 2360 | `	}` |
|       85 | 2361 | `	SyBlobRelease(&sPath);` |
|       85 | 2362 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        4 | 2363 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|        2 | 2364 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        3 | 2365 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2366 | `			return SXERR_ABORT;` |
|        - | 2367 | `		}` |
|        1 | 2368 | `	}` |
|       85 | 2369 | `	return SXRET_OK;` |
|       45 | 2370 | `}` |
|        - | 2371 | `/*` |
|        - | 2372 | ` * Compile the stupid 'declare' language construct.` |
|        - | 2373 | ` *` |
|        - | 2374 | ` * According to the PHP language reference manual.` |
|        - | 2375 | ` *  The declare construct is used to set execution directives for a block of code.` |
|        - | 2376 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|        - | 2377 | ` *  declare (directive)` |
|        - | 2378 | ` *   statement` |
|        - | 2379 | ` * The directive section allows the behavior of the declare block to be set.` |
|        - | 2380 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|        - | 2381 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|        - | 2382 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|        - | 2383 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|        - | 2384 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|        - | 2385 | ` * <?php` |
|        - | 2386 | ` * // these are the same:` |
|        - | 2387 | ` * // you can use this:` |
|        - | 2388 | ` * declare(ticks=1) {` |
|        - | 2389 | ` *   // entire script here` |
|        - | 2390 | ` * }` |
|        - | 2391 | ` * // or you can use this:` |
|        - | 2392 | ` * declare(ticks=1);` |
|        - | 2393 | ` * // entire script here` |
|        - | 2394 | ` * ?>` |
|        - | 2395 | ` *` |
|        - | 2396 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|        - | 2397 | ` */` |
|        - | 2398 | `/*` |
|        - | 2399 | ` * Match a directive name against a known literal (case-insensitive).` |
|        - | 2400 | ` */` |
|       72 | 2401 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|        5 | 2402 | `{` |
|      109 | 2403 | `	return SyStringLength(pName) == nWant` |
|       72 | 2404 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|        5 | 2405 | `}` |
|        - | 2406 |  |
|       42 | 2407 | `PH7_PRIVATE sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|        5 | 2408 | `{` |
|       47 | 2409 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       47 | 2410 | `	SyToken *pBodyEnd = 0;` |
|        - | 2411 | `	SyToken *pBodyStart;` |
|        - | 2412 | `	SyToken *pCursor;` |
|        - | 2413 | `	int bHasStrictTypes;` |
|        - | 2414 | `	int bBlockForm;` |
|        - | 2415 | `	int bPlacementOk;` |
|        - | 2416 | `	sxi32 rc;` |
|       47 | 2417 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|       47 | 2418 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|        6 | 2419 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|        6 | 2420 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2421 | `			return SXERR_ABORT;` |
|        - | 2422 | `		}` |
|        6 | 2423 | `		goto Synchro;` |
|        - | 2424 | `	}` |
|       43 | 2425 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|       43 | 2426 | `	pBodyStart = pGen->pIn;` |
|        - | 2427 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|       43 | 2428 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|       43 | 2429 | `	if( pBodyEnd >= pGen->pEnd ){` |
|      ! 0 | 2430 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|      ! 0 | 2431 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2432 | `			return SXERR_ABORT;` |
|        - | 2433 | `		}` |
|      ! 0 | 2434 | `		return SXRET_OK;` |
|        - | 2435 | `	}` |
|        - | 2436 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|        - | 2437 | `	 * now delimits the comma-separated directive list. */` |
|       43 | 2438 | `	pGen->pIn = &pBodyEnd[1];` |
|       43 | 2439 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      ! 0 | 2440 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|      ! 0 | 2441 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2442 | `			return SXERR_ABORT;` |
|        - | 2443 | `		}` |
|      ! 0 | 2444 | `	}` |
|       43 | 2445 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|       43 | 2446 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|       43 | 2447 | `	bHasStrictTypes = 0;` |
|        - | 2448 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|        - | 2449 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|        - | 2450 | `	 * directive appears anywhere in the list, before validating values. */` |
|       43 | 2451 | `	pCursor = pBodyStart;` |
|       55 | 2452 | `	while( pCursor < pBodyEnd ){` |
|       51 | 2453 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|       43 | 2454 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|       39 | 2455 | `				bHasStrictTypes = 1;` |
|       39 | 2456 | `				break;` |
|        - | 2457 | `			}` |
|        2 | 2458 | `		}` |
|       14 | 2459 | `		pCursor++;` |
|        2 | 2460 | `	}` |
|       43 | 2461 | `	if( bHasStrictTypes && bBlockForm ){` |
|        3 | 2462 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2463 | `			"strict_types declaration must not use block mode");` |
|        3 | 2464 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        3 | 2465 | `		return SXRET_OK;` |
|        - | 2466 | `	}` |
|       41 | 2467 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|        6 | 2468 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2469 | `			"strict_types declaration must be the very first statement in the script");` |
|        6 | 2470 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        6 | 2471 | `		return SXRET_OK;` |
|        - | 2472 | `	}` |
|        - | 2473 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|       37 | 2474 | `	pCursor = pBodyStart;` |
|       69 | 2475 | `	while( pCursor < pBodyEnd ){` |
|        - | 2476 | `		SyToken *pNameTok;` |
|        - | 2477 | `		SyToken *pEqTok;` |
|        - | 2478 | `		SyToken *pValTok;` |
|        - | 2479 | `		SyString *pDirName;` |
|        - | 2480 | `		int bIsStrict;` |
|        - | 2481 | `		int iStrictValue;` |
|       39 | 2482 | `		pNameTok = pCursor;` |
|       39 | 2483 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 2484 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2485 | `				"declare: Expecting a directive name");` |
|      ! 0 | 2486 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2487 | `			return SXRET_OK;` |
|        - | 2488 | `		}` |
|       39 | 2489 | `		pEqTok = pNameTok + 1;` |
|       39 | 2490 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|      ! 0 | 2491 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2492 | `				"declare: Expecting '=' after directive name");` |
|      ! 0 | 2493 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2494 | `			return SXRET_OK;` |
|        - | 2495 | `		}` |
|       39 | 2496 | `		pValTok = pEqTok + 1;` |
|       39 | 2497 | `		if( pValTok >= pBodyEnd ){` |
|      ! 0 | 2498 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2499 | `				"declare: Expecting value after '='");` |
|      ! 0 | 2500 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2501 | `			return SXRET_OK;` |
|        - | 2502 | `		}` |
|       39 | 2503 | `		pDirName = &pNameTok->sData;` |
|       39 | 2504 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|       39 | 2505 | `		if( bIsStrict ){` |
|        - | 2506 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|        - | 2507 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|       35 | 2508 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      ! 0 | 2509 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2510 | `					"declare(strict_types) value must be a literal");` |
|      ! 0 | 2511 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2512 | `				return SXRET_OK;` |
|        - | 2513 | `			}` |
|       35 | 2514 | `			iStrictValue = -1;` |
|       35 | 2515 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|       35 | 2516 | `				const char *zv = SyStringData(&pValTok->sData);` |
|       35 | 2517 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|       35 | 2518 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|       33 | 2519 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|       15 | 2520 | `			}` |
|       35 | 2521 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|        3 | 2522 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2523 | `					"strict_types declaration must have 0 or 1 as its value");` |
|        3 | 2524 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        3 | 2525 | `				return SXRET_OK;` |
|        - | 2526 | `			}` |
|       32 | 2527 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|       18 | 2528 | `		}else{` |
|        - | 2529 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|        - | 2530 | `			 * preserve the legacy notice so callers relying on the old` |
|        - | 2531 | `			 * behavior don't regress. */` |
|        8 | 2532 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|        - | 2533 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|        2 | 2534 | `				ph7_lib_version()` |
|        - | 2535 | `				);` |
|        - | 2536 | `		}` |
|       36 | 2537 | `		pCursor = pValTok + 1;` |
|        - | 2538 | `		/* Consume separating comma (or end). */` |
|       36 | 2539 | `		if( pCursor < pBodyEnd ){` |
|        3 | 2540 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|      ! 0 | 2541 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2542 | `					"declare: Expecting ',' or ')' after directive value");` |
|      ! 0 | 2543 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2544 | `				return SXRET_OK;` |
|        - | 2545 | `			}` |
|        3 | 2546 | `			pCursor++;` |
|        1 | 2547 | `		}` |
|        4 | 2548 | `	}` |
|        - | 2549 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|        - | 2550 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|        - | 2551 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|       34 | 2552 | `	return SXRET_OK;` |
|        2 | 2553 | `Synchro:` |
|        - | 2554 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|       16 | 2555 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       12 | 2556 | `		pGen->pIn++;` |
|        2 | 2557 | `	}` |
|        6 | 2558 | `	return SXRET_OK;` |
|       26 | 2559 | `}` |
|        - | 2560 | `/*` |
|        - | 2561 | ` * Compile a class constant.` |
|        - | 2562 | ` * According to the PHP language reference manual` |
|        - | 2563 | ` *  Class Constants` |
|        - | 2564 | ` *   It is possible to define constant values on a per-class basis remaining` |
|        - | 2565 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|        - | 2566 | ` *   you don't use the $ symbol to declare or use them.` |
|        - | 2567 | ` *   The value must be a constant expression, not (for example) a variable,` |
|        - | 2568 | ` *   a property, a result of a mathematical operation, or a function call.` |
|        - | 2569 | ` *   It's also possible for interfaces to have constants.` |
|        - | 2570 | ` * Symisc eXtension.` |
|        - | 2571 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|        - | 2572 | ` *  the zend engine would allow only simple scalar value.` |
|        - | 2573 | ` *  Example:` |
|        - | 2574 | ` *   class Test{` |
|        - | 2575 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|        - | 2576 | ` *   };` |
|        - | 2577 | ` *   var_dump(TEST::MyConst);` |
|        - | 2578 | ` *   Refer to the official documentation for more information on the powerful extension` |
|        - | 2579 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|        - | 2580 | ` */` |
|        - | 2581 | `/*` |
|        - | 2582 | ` * Exception handling.` |
|        - | 2583 | ` *  According to the PHP language reference manual` |
|        - | 2584 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|        - | 2585 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|        - | 2586 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|        - | 2587 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|        - | 2588 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|        - | 2589 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|        - | 2590 | ` *    (or re-thrown) within a catch block.` |
|        - | 2591 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|        - | 2592 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|        - | 2593 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|        - | 2594 | ` *    been defined with set_exception_handler().` |
|        - | 2595 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|        - | 2596 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|        - | 2597 | ` */` |
|        - | 2598 | `/*` |
|        - | 2599 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|        - | 2600 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|        - | 2601 | ` * indicates failure.` |
|        - | 2602 | ` */` |
|   547222 | 2603 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 2604 | `{` |
|   547227 | 2605 | `	sxi32 rc = SXRET_OK;` |
|   547227 | 2606 | `	if( pRoot->pOp ){` |
|   547215 | 2607 | `		switch( pRoot->pOp->iOp ){` |
|   273605 | 2608 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|        - | 2609 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|        - | 2610 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|        - | 2611 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|        - | 2612 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|        - | 2613 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|   547215 | 2614 | `			break;` |
|      ! 0 | 2615 | `		default:` |
|        - | 2616 | `			/* Runtime will still reject non-Throwable values; the set above` |
|        - | 2617 | `			 * covers the common shapes and gives a friendlier compile error` |
|        - | 2618 | ``			 * for obvious mistakes like `throw 5`. */`` |
|      ! 0 | 2619 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 2620 | `				"throw: Expecting an exception class instance");` |
|      ! 0 | 2621 | `			if( rc != SXERR_ABORT ){` |
|      ! 0 | 2622 | `				rc = SXERR_INVALID;` |
|      ! 0 | 2623 | `			}` |
|      ! 0 | 2624 | `			break;` |
|        - | 2625 | `		}` |
|   273622 | 2626 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|        - | 2627 | `		/* Unexpected expression */` |
|      ! 0 | 2628 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 2629 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 2630 | `		if( rc != SXERR_ABORT ){` |
|      ! 0 | 2631 | `			rc = SXERR_INVALID;` |
|      ! 0 | 2632 | `		}` |
|      ! 0 | 2633 | `	}` |
|   547227 | 2634 | `	return rc;` |
|        5 | 2635 | `}` |
|        - | 2636 | `/*` |
|        - | 2637 | ` * Compile a 'throw' statement.` |
|        - | 2638 | ` * throw: This is how you trigger an exception.` |
|        - | 2639 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|        - | 2640 | ` */` |
|   547186 | 2641 | `PH7_PRIVATE sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|        5 | 2642 | `{` |
|   547191 | 2643 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2644 | `	GenBlock *pBlock;` |
|        - | 2645 | `	sxu32 nIdx;` |
|        - | 2646 | `	sxi32 rc;` |
|   547191 | 2647 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|        - | 2648 | `	/* Compile the expression */` |
|   547191 | 2649 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|   547191 | 2650 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 | 2651 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|      ! 0 | 2652 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2653 | `			return SXERR_ABORT;` |
|        - | 2654 | `		}` |
|      ! 0 | 2655 | `		return SXRET_OK;` |
|        - | 2656 | `	}` |
|   547191 | 2657 | `	pBlock = pGen->pCurrent;` |
|        - | 2658 | `	/* Point to the top most function or try block and emit the forward jump */` |
|  2180237 | 2659 | `	while(pBlock->pParent){` |
|  2180233 | 2660 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|   547187 | 2661 | `			break;` |
|        - | 2662 | `		}` |
|        - | 2663 | `		/* Point to the parent block */` |
|  1633051 | 2664 | `		pBlock = pBlock->pParent;` |
|        5 | 2665 | `	}` |
|        - | 2666 | `	/* Emit the throw instruction */` |
|   547191 | 2667 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|        - | 2668 | `	/* Emit the jump */` |
|   547191 | 2669 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|   547191 | 2670 | `	return SXRET_OK;` |
|   273598 | 2671 | `}` |
|        - | 2672 | `/*` |
|        - | 2673 | ` * Compile a PHP 8.0 'throw' expression.` |
|        - | 2674 | ` * Called from the expression code generator when a 'throw' keyword is` |
|        - | 2675 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|        - | 2676 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|        - | 2677 | ` * the validator guarantees the operand is a valid exception target.` |
|        - | 2678 | ` */` |
|       36 | 2679 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|        2 | 2680 | `{` |
|       38 | 2681 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2682 | `	GenBlock *pBlock;` |
|        - | 2683 | `	sxu32 nIdx;` |
|        - | 2684 | `	sxi32 rc;` |
|       18 | 2685 | `	(void)iCompileFlag;` |
|       38 | 2686 | `	pGen->pIn++; /* Skip 'throw' */` |
|       38 | 2687 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 2688 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 2689 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 2690 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2691 | `			return SXERR_ABORT;` |
|        - | 2692 | `		}` |
|      ! 0 | 2693 | `		return SXRET_OK;` |
|        - | 2694 | `	}` |
|       38 | 2695 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|       38 | 2696 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 2697 | `		return SXERR_ABORT;` |
|        - | 2698 | `	}` |
|       38 | 2699 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 | 2700 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 2701 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 2702 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2703 | `			return SXERR_ABORT;` |
|        - | 2704 | `		}` |
|      ! 0 | 2705 | `		return SXRET_OK;` |
|        - | 2706 | `	}` |
|        - | 2707 | `	/* Walk up to nearest exception/function block for the jump target */` |
|       38 | 2708 | `	pBlock = pGen->pCurrent;` |
|       60 | 2709 | `	while( pBlock->pParent ){` |
|       49 | 2710 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|       27 | 2711 | `			break;` |
|        - | 2712 | `		}` |
|       23 | 2713 | `		pBlock = pBlock->pParent;` |
|        1 | 2714 | `	}` |
|       38 | 2715 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       38 | 2716 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|       38 | 2717 | `	return SXRET_OK;` |
|       20 | 2718 | `}` |
|        - | 2719 | `/*` |
|        - | 2720 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|        - | 2721 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|        - | 2722 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|        - | 2723 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|        - | 2724 | ` * compile error propagated from the parser.` |
|        - | 2725 | ` */` |
|       56 | 2726 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|        5 | 2727 | `{` |
|        - | 2728 | `	SyString sClassName;` |
|        - | 2729 | `	SyToken *pToken;` |
|        - | 2730 | `	SyString *pName;` |
|        - | 2731 | `	char *zDup;` |
|        - | 2732 | `	sxi32 rc;` |
|       61 | 2733 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       61 | 2734 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|       61 | 2735 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|       61 | 2736 | `	SySetInit(&pCatch->sByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       61 | 2737 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 | 2738 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 2739 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 2740 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 2741 | `		return SXERR_INVALID;` |
|        - | 2742 | `	}` |
|       61 | 2743 | `	pGen->pIn++; /* '(' */` |
|       28 | 2744 | `	for(;;){` |
|        - | 2745 | `		SyBlob sResolved;` |
|       61 | 2746 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       61 | 2747 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 2748 | `			SyBlobRelease(&sResolved);` |
|      ! 0 | 2749 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 2750 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 2751 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 2752 | `			return SXERR_INVALID;` |
|        - | 2753 | `		}` |
|       89 | 2754 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       56 | 2755 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       61 | 2756 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|       61 | 2757 | `		SyBlobRelease(&sResolved);` |
|       61 | 2758 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|       61 | 2759 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|       61 | 2760 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       56 | 2761 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|        5 | 2762 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|      ! 0 | 2763 | `			pGen->pIn++; continue;` |
|        - | 2764 | `		}` |
|       61 | 2765 | `		break;` |
|      ! 0 | 2766 | `	}` |
|        - | 2767 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|        - | 2768 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding. */` |
|       61 | 2769 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|        3 | 2770 | `		pGen->pIn++; /* ')' */` |
|        3 | 2771 | `		return SXRET_OK;` |
|        - | 2772 | `	}` |
|       54 | 2773 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|       59 | 2774 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 2775 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 2776 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 2777 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 2778 | `		return SXERR_INVALID;` |
|        - | 2779 | `	}` |
|       59 | 2780 | `	pGen->pIn++; /* '$' */` |
|       59 | 2781 | `	pName = &pGen->pIn->sData;` |
|       59 | 2782 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       59 | 2783 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|       59 | 2784 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|       59 | 2785 | `	pGen->pIn++;` |
|       59 | 2786 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 | 2787 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 2788 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 2789 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 2790 | `		return SXERR_INVALID;` |
|        - | 2791 | `	}` |
|       59 | 2792 | `	pGen->pIn++; /* ')' */` |
|       59 | 2793 | `	return SXRET_OK;` |
|       33 | 2794 | `}` |
|        - | 2795 | `/*` |
|        - | 2796 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|        - | 2797 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|        - | 2798 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|        - | 2799 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|        - | 2800 | ` * VmThrowException):` |
|        - | 2801 | ` *` |
|        - | 2802 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|        - | 2803 | ` *    <try body>` |
|        - | 2804 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|        - | 2805 | ` *    JMP  -> finally\|end` |
|        - | 2806 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|        - | 2807 | ` *    <catch body>` |
|        - | 2808 | ` *    JMP  -> finally\|end` |
|        - | 2809 | ` *    ... more catches ...` |
|        - | 2810 | ` *  Lfin: <finally body>` |
|        - | 2811 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|        - | 2812 | ` *  Lend:` |
|        - | 2813 | ` */` |
|      100 | 2814 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|        5 | 2815 | `{` |
|      105 | 2816 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2817 | `	GenBlock *pTry;` |
|        - | 2818 | `	VmInstr *pInstr;` |
|      105 | 2819 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|        - | 2820 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|        - | 2821 | `	sxi32 rc;` |
|      105 | 2822 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|        - | 2823 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION) */` |
|      105 | 2824 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|      105 | 2825 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      105 | 2826 | `	pTry->pUserData = pException;` |
|      105 | 2827 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|      105 | 2828 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|      105 | 2829 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      105 | 2830 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      105 | 2831 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|      105 | 2832 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|        - | 2833 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|      105 | 2834 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|      105 | 2835 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|      105 | 2836 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|      105 | 2837 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 2838 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|      105 | 2839 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|        - | 2840 | `	/* Catch clauses (inline) */` |
|      105 | 2841 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      100 | 2842 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       61 | 2843 | `		sxu32 k = 0;` |
|       84 | 2844 | `		for(;;){` |
|        - | 2845 | `			ph7_exception_block sCatch;` |
|        - | 2846 | `			GenBlock *pCatchBlk;` |
|      117 | 2847 | `			sxu32 idxJmp = 0;` |
|      112 | 2848 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      107 | 2849 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|       33 | 2850 | `				break;` |
|        - | 2851 | `			}` |
|       61 | 2852 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|       61 | 2853 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       61 | 2854 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|       61 | 2855 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|       61 | 2856 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|       61 | 2857 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatchBlk);` |
|       61 | 2858 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        - | 2859 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|        - | 2860 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|        - | 2861 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump). */` |
|       61 | 2862 | `			pCatchBlk->pUserData = pException;` |
|       61 | 2863 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|       61 | 2864 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       61 | 2865 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|       61 | 2866 | `			GenStateLeaveBlock(&(*pGen),0);` |
|        - | 2867 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|        - | 2868 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|       61 | 2869 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       61 | 2870 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|       61 | 2871 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|       61 | 2872 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|       61 | 2873 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       61 | 2874 | `			k++;` |
|        5 | 2875 | `		}` |
|       28 | 2876 | `	}` |
|        - | 2877 | `	/* Finally (inline) */` |
|      105 | 2878 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       80 | 2879 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|        - | 2880 | `		GenBlock *pFinBlk;` |
|       52 | 2881 | `		pGen->pIn++; /* Jump 'finally' */` |
|       52 | 2882 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|       52 | 2883 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|       52 | 2884 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       52 | 2885 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       52 | 2886 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       52 | 2887 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|       52 | 2888 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       52 | 2889 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|       52 | 2890 | `		pException->iHasFinally = 1;` |
|       24 | 2891 | `	}` |
|      105 | 2892 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|      105 | 2893 | `	pException->iInlined = 1;` |
|        - | 2894 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|        - | 2895 | `	{` |
|      105 | 2896 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|        - | 2897 | `		sxu32 *aJ; sxu32 n;` |
|      105 | 2898 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|      105 | 2899 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|      105 | 2900 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|      161 | 2901 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|       61 | 2902 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|       61 | 2903 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|       33 | 2904 | `		}` |
|        - | 2905 | `	}` |
|      105 | 2906 | `	SySetRelease(&aCatchJmp);` |
|      105 | 2907 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|      ! 0 | 2908 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|      ! 0 | 2909 | `	}` |
|      105 | 2910 | `	return SXRET_OK;` |
|       55 | 2911 | `}` |
|        - | 2912 | `/*` |
|        - | 2913 | ` * Compile a 'catch' block.` |
|        - | 2914 | ` * Catch: A "catch" block retrieves an exception and creates` |
|        - | 2915 | ` * an object containing the exception information.` |
|        - | 2916 | ` */` |
|    24840 | 2917 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|        5 | 2918 | `{` |
|    24845 | 2919 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2920 | `	ph7_exception_block sCatch;` |
|        - | 2921 | `	SySet *pInstrContainer;` |
|        - | 2922 | `	SyString sClassName;` |
|        - | 2923 | `	GenBlock *pCatch;` |
|        - | 2924 | `	SyToken *pToken;` |
|        - | 2925 | `	SyString *pName;` |
|        - | 2926 | `	char *zDup;` |
|        - | 2927 | `	sxi32 rc;` |
|    24845 | 2928 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|        - | 2929 | `	/* Zero the structure */` |
|    24845 | 2930 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|        - | 2931 | `	/* Initialize fields */` |
|    24845 | 2932 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|    24845 | 2933 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|    24845 | 2934 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|        - | 2935 | `			/* Unexpected token,break immediately */` |
|      ! 0 | 2936 | `			pToken = pGen->pIn;` |
|      ! 0 | 2937 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 2938 | `				pToken--;` |
|      ! 0 | 2939 | `			}` |
|      ! 0 | 2940 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 2941 | `				"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 2942 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 2943 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2944 | `				return SXERR_ABORT;` |
|        - | 2945 | `			}` |
|      ! 0 | 2946 | `			return SXERR_INVALID;` |
|        - | 2947 | `	}` |
|        - | 2948 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|    24845 | 2949 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|    12435 | 2950 | `	for(;;){` |
|        - | 2951 | `		SyBlob sResolved;` |
|    24875 | 2952 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    24875 | 2953 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|        6 | 2954 | `			SyBlobRelease(&sResolved);` |
|        6 | 2955 | `			pToken = pGen->pIn;` |
|        6 | 2956 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 2957 | `				pToken--;` |
|      ! 0 | 2958 | `			}` |
|        8 | 2959 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 2960 | `				"syntax error, unexpected %s \"%z\"",` |
|        2 | 2961 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|        6 | 2962 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2963 | `				return SXERR_ABORT;` |
|        - | 2964 | `			}` |
|        6 | 2965 | `			return SXERR_INVALID;` |
|        - | 2966 | `		}` |
|        - | 2967 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|        - | 2968 | `		 * transient SyBlob allocation. */` |
|    37304 | 2969 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    24866 | 2970 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|    24871 | 2971 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|    24871 | 2972 | `		SyBlobRelease(&sResolved);` |
|    24871 | 2973 | `		if( zDup == 0 ){` |
|      ! 0 | 2974 | `			goto Mem;` |
|        - | 2975 | `		}` |
|    24871 | 2976 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|    24871 | 2977 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 2978 | `			goto Mem;` |
|        - | 2979 | `		}` |
|        - | 2980 | `		/* Check for '\|' (multi-catch separator) */` |
|    24866 | 2981 | `		if( pGen->pIn < pGen->pEnd &&` |
|    24866 | 2982 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|       35 | 2983 | `			pGen->pIn->sData.nByte == 1 &&` |
|       30 | 2984 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|       32 | 2985 | `			pGen->pIn++; /* Consume the '\|' */` |
|       32 | 2986 | `			continue;` |
|        - | 2987 | `		}` |
|    24841 | 2988 | `		break;` |
|      ! 0 | 2989 | `	}` |
|        - | 2990 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|        - | 2991 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding;` |
|        - | 2992 | `	 * jump straight to compiling the block below. */` |
|    24841 | 2993 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) /*)*/ ){` |
|        7 | 2994 | `		goto CatchBody;` |
|        - | 2995 | `	}` |
|    24830 | 2996 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|    24835 | 2997 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - | 2998 | `			/* Unexpected token,break immediately */` |
|      ! 0 | 2999 | `			pToken = pGen->pIn;` |
|      ! 0 | 3000 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 3001 | `				pToken--;` |
|      ! 0 | 3002 | `			}` |
|      ! 0 | 3003 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 3004 | `				"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 3005 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 3006 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3007 | `				return SXERR_ABORT;` |
|        - | 3008 | `			}` |
|      ! 0 | 3009 | `			return SXERR_INVALID;` |
|        - | 3010 | `	}` |
|    24835 | 3011 | `	pGen->pIn++; /* Jump the dollar sign */` |
|        - | 3012 | `	/* Duplicate instance name */` |
|    24835 | 3013 | `	pName = &pGen->pIn->sData;` |
|    24835 | 3014 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    24835 | 3015 | `	if( zDup == 0 ){` |
|      ! 0 | 3016 | `		goto Mem;` |
|        - | 3017 | `	}` |
|    24835 | 3018 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|    24835 | 3019 | `	pGen->pIn++;` |
|    12418 | 3020 | `CatchBody:` |
|    24841 | 3021 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|        - | 3022 | `		/* Unexpected token,break immediately */` |
|      ! 0 | 3023 | `		pToken = pGen->pIn;` |
|      ! 0 | 3024 | `		if( pToken >= pGen->pEnd ){` |
|      ! 0 | 3025 | `			pToken--;` |
|      ! 0 | 3026 | `		}` |
|      ! 0 | 3027 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 3028 | `			"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 3029 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 3030 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3031 | `			return SXERR_ABORT;` |
|        - | 3032 | `		}` |
|      ! 0 | 3033 | `		return SXERR_INVALID;` |
|        - | 3034 | `	}` |
|        - | 3035 | `	/* Compile the block */` |
|    24841 | 3036 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|        - | 3037 | `	/* Create the catch block */` |
|    24841 | 3038 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|    24841 | 3039 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3040 | `		return SXERR_ABORT;` |
|        - | 3041 | `	}` |
|        - | 3042 | `	/* Swap bytecode container */` |
|    24841 | 3043 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    24841 | 3044 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|        - | 3045 | `	/* Compile the block */` |
|    24841 | 3046 | `	PH7_CompileBlock(&(*pGen),0);` |
|        - | 3047 | `	/* Fix forward jumps now the destination is resolved  */` |
|    24841 | 3048 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 3049 | `	/* Emit the DONE instruction */` |
|    24841 | 3050 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - | 3051 | `	/* Leave the block */` |
|    24841 | 3052 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 3053 | `	/* Restore the default container */` |
|    24841 | 3054 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - | 3055 | `	/* Install the catch block */` |
|    24841 | 3056 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|    24841 | 3057 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3058 | `		goto Mem;` |
|        - | 3059 | `	}` |
|    24841 | 3060 | `	return SXRET_OK;` |
|      ! 0 | 3061 | `Mem:` |
|      ! 0 | 3062 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 3063 | `	return SXERR_ABORT;` |
|    12425 | 3064 | `}` |
|        - | 3065 | `/*` |
|        - | 3066 | ` * Compile a 'try' block.` |
|        - | 3067 | ` * A function using an exception should be in a "try" block.` |
|        - | 3068 | ` * If the exception does not trigger, the code will continue` |
|        - | 3069 | ` * as normal. However if the exception triggers, an exception` |
|        - | 3070 | ` * is "thrown".` |
|        - | 3071 | ` */` |
|    24998 | 3072 | `PH7_PRIVATE sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|        5 | 3073 | `{` |
|        - | 3074 | `	ph7_exception *pException;` |
|    25003 | 3075 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 3076 | `	GenBlock *pTry;` |
|        - | 3077 | `	sxu32 nJmpIdx;` |
|        - | 3078 | `	sxi32 rc;` |
|        - | 3079 | `	/* Create the exception container */` |
|    25003 | 3080 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|    25003 | 3081 | `	if( pException == 0 ){` |
|      ! 0 | 3082 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|      ! 0 | 3083 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 3084 | `		return SXERR_ABORT;` |
|        - | 3085 | `	}` |
|        - | 3086 | `	/* Zero the structure */` |
|    25003 | 3087 | `	SyZero(pException,sizeof(ph7_exception));` |
|        - | 3088 | `	/* Initialize fields */` |
|    25003 | 3089 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|    25003 | 3090 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|    25003 | 3091 | `	pException->iHasFinally = 0;` |
|    25003 | 3092 | `	pException->iFinallyDone = 0;` |
|    25003 | 3093 | `	pException->pVm = pGen->pVm;` |
|        - | 3094 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|        - | 3095 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the legacy path.`` |
|        - | 3096 | `	 * DORMANT until the inline VM handlers (OP_CATCH / OP_END_FINALLY dispatch,` |
|        - | 3097 | `	 * VmThrowException pc-redirect, return/break-through-finally threading, generator` |
|        - | 3098 | `	 * park of aFinallyAction) land — the compiler emits the layout but the VM cannot yet` |
|        - | 3099 | `	 * execute it. Guarded by pVm->bInlineTryCatch (default 0) so the tree stays green. */` |
|    25003 | 3100 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|      105 | 3101 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|        - | 3102 | `	}` |
|        - | 3103 | `	/* Create the try block */` |
|    24903 | 3104 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|    24903 | 3105 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3106 | `		return SXERR_ABORT;` |
|        - | 3107 | `	}` |
|        - | 3108 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|    24903 | 3109 | `	pTry->pUserData = pException;` |
|        - | 3110 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|    24903 | 3111 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|        - | 3112 | `	/* Fix the jump later when the destination is resolved */` |
|    24903 | 3113 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|    24903 | 3114 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|        - | 3115 | `	/* Compile the block */` |
|    24903 | 3116 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|    24903 | 3117 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 3118 | `		return SXERR_ABORT;` |
|        - | 3119 | `	}` |
|        - | 3120 | `	/* Fix forward jumps now the destination is resolved */` |
|    24903 | 3121 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 3122 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|    24903 | 3123 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|        - | 3124 | `	/* Leave the block */` |
|    24903 | 3125 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 3126 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|    24903 | 3127 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|    24896 | 3128 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|        - | 3129 | `		/* Compile one or more catch blocks */` |
|    24836 | 3130 | `		for(;;){` |
|    49672 | 3131 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    37319 | 3132 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|    12421 | 3133 | `					break;` |
|        - | 3134 | `			}` |
|    24845 | 3135 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|    24845 | 3136 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3137 | `				return SXERR_ABORT;` |
|        - | 3138 | `			}` |
|        5 | 3139 | `		}` |
|    12416 | 3140 | `	}` |
|        - | 3141 | `	/* Compile optional finally block */` |
|    24903 | 3142 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      762 | 3143 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|        - | 3144 | `		SySet *pInstrContainer;` |
|        - | 3145 | `		GenBlock *pFinBlock;` |
|      129 | 3146 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|        - | 3147 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      129 | 3148 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      129 | 3149 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 3150 | `			return SXERR_ABORT;` |
|        - | 3151 | `		}` |
|        - | 3152 | `		/* Swap bytecode container */` |
|      129 | 3153 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      129 | 3154 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|        - | 3155 | `		/* Compile the finally body */` |
|      129 | 3156 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      129 | 3157 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3158 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 | 3159 | `			return SXERR_ABORT;` |
|        - | 3160 | `		}` |
|        - | 3161 | `		/* Fix forward jumps now the destination is resolved */` |
|      129 | 3162 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 3163 | `		/* Emit DONE to terminate the finally block */` |
|      129 | 3164 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - | 3165 | `		/* Leave the block */` |
|      129 | 3166 | `		GenStateLeaveBlock(&(*pGen),0);` |
|        - | 3167 | `		/* Restore the default container */` |
|      129 | 3168 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      129 | 3169 | `		pException->iHasFinally = 1;` |
|       62 | 3170 | `	}` |
|        - | 3171 | `	/* Must have at least one catch or finally */` |
|    24903 | 3172 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|        8 | 3173 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 3174 | `			"Cannot use try without catch or finally");` |
|        8 | 3175 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3176 | `			return SXERR_ABORT;` |
|        - | 3177 | `		}` |
|        3 | 3178 | `	}` |
|    24903 | 3179 | `	return SXRET_OK;` |
|    12504 | 3180 | `}` |
|        - | 3181 | `/*` |
|        - | 3182 | ` * Compile a switch block.` |
|        - | 3183 | ` *  (See block-comment below for more information)` |
|        - | 3184 | ` */` |
|   135842 | 3185 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|        5 | 3186 | `{` |
|   135847 | 3187 | `	sxi32 rc = SXRET_OK;` |
|   135847 | 3188 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|        - | 3189 | `		/* Unexpected token */` |
|      ! 0 | 3190 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 | 3191 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3192 | `			return SXERR_ABORT;` |
|        - | 3193 | `		}` |
|      ! 0 | 3194 | `		pGen->pIn++;` |
|      ! 0 | 3195 | `	}` |
|   135847 | 3196 | `	pGen->pIn++;` |
|        - | 3197 | `	/* First instruction to execute in this block. */` |
|   135847 | 3198 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|        - | 3199 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|        - | 3200 | `	 * or the '}' token */` |
|   130119 | 3201 | `	for(;;){` |
|   260243 | 3202 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 3203 | `			/* No more input to process */` |
|      ! 0 | 3204 | `			break;` |
|        - | 3205 | `		}` |
|   260243 | 3206 | `		rc = SXRET_OK;` |
|   260243 | 3207 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|    34987 | 3208 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|    11665 | 3209 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|        - | 3210 | `					/* Unexpected token */` |
|      ! 0 | 3211 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|      ! 0 | 3212 | `						&pGen->pIn->sData);` |
|      ! 0 | 3213 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3214 | `						return SXERR_ABORT;` |
|        - | 3215 | `					}` |
|        - | 3216 | `					/* FALL THROUGH */` |
|      ! 0 | 3217 | `				}` |
|    11665 | 3218 | `				rc = SXERR_EOF;` |
|    11665 | 3219 | `				break;` |
|        - | 3220 | `			}` |
|    11666 | 3221 | `		}else{` |
|        - | 3222 | `			sxi32 nKwrd;` |
|        - | 3223 | `			/* Extract the keyword */` |
|   225261 | 3224 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   225261 | 3225 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|    62095 | 3226 | `				break;` |
|        - | 3227 | `			}` |
|   101081 | 3228 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|        3 | 3229 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|        - | 3230 | `					/* Unexpected token */` |
|      ! 0 | 3231 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|      ! 0 | 3232 | `						&pGen->pIn->sData);` |
|      ! 0 | 3233 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3234 | `						return SXERR_ABORT;` |
|        - | 3235 | `					}` |
|        - | 3236 | `					/* FALL THROUGH */` |
|      ! 0 | 3237 | `				}` |
|        - | 3238 | `				/* Block compiled */` |
|        3 | 3239 | `				break;` |
|        - | 3240 | `			}` |
|        - | 3241 | `		}` |
|        - | 3242 | `		/* Compile block */` |
|   124401 | 3243 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|   124401 | 3244 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3245 | `			return SXERR_ABORT;` |
|        - | 3246 | `		}` |
|        5 | 3247 | `	}` |
|   135847 | 3248 | `	return rc;` |
|    67926 | 3249 | `}` |
|        - | 3250 | `/*` |
|        - | 3251 | ` * Compile a case eXpression.` |
|        - | 3252 | ` *  (See block-comment below for more information)` |
|        - | 3253 | ` */` |
|   131944 | 3254 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|        5 | 3255 | `{` |
|        - | 3256 | `	SySet *pInstrContainer;` |
|        - | 3257 | `	SyToken *pEnd,*pTmp;` |
|   131949 | 3258 | `	sxi32 iNest = 0;` |
|        - | 3259 | `	sxi32 rc;` |
|        - | 3260 | `	/* Delimit the expression */` |
|   131949 | 3261 | `	pEnd = pGen->pIn;` |
|   263901 | 3262 | `	while( pEnd < pGen->pEnd ){` |
|   263901 | 3263 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|        - | 3264 | `			/* Increment nesting level */` |
|        3 | 3265 | `			iNest++;` |
|   263900 | 3266 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|        - | 3267 | `			/* Decrement nesting level */` |
|        3 | 3268 | `			iNest--;` |
|   263898 | 3269 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|   131949 | 3270 | `			break;` |
|        - | 3271 | `		}` |
|   131957 | 3272 | `		pEnd++;` |
|        5 | 3273 | `	}` |
|   131949 | 3274 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 3275 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|      ! 0 | 3276 | `		if( rc == SXERR_ABORT ){` |
|        - | 3277 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 3278 | `			return SXERR_ABORT;` |
|        - | 3279 | `		}` |
|      ! 0 | 3280 | `	}` |
|        - | 3281 | `	/* Swap token stream */` |
|   131949 | 3282 | `	pTmp = pGen->pEnd;` |
|   131949 | 3283 | `	pGen->pEnd = pEnd;` |
|   131949 | 3284 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   131949 | 3285 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|   131949 | 3286 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 3287 | `	/* Emit the done instruction */` |
|   131949 | 3288 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   131949 | 3289 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - | 3290 | `	/* Update token stream */` |
|   131949 | 3291 | `	pGen->pIn  = pEnd;` |
|   131949 | 3292 | `	pGen->pEnd = pTmp;` |
|   131949 | 3293 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 3294 | `		return SXERR_ABORT;` |
|        - | 3295 | `	}` |
|   131949 | 3296 | `	return SXRET_OK;` |
|    65977 | 3297 | `}` |
|        - | 3298 | `/*` |
|        - | 3299 | ` * Compile the smart switch statement.` |
|        - | 3300 | ` * According to the PHP language reference manual` |
|        - | 3301 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|        - | 3302 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|        - | 3303 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|        - | 3304 | ` *  This is exactly what the switch statement is for.` |
|        - | 3305 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|        - | 3306 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|        - | 3307 | ` *  of the outer loop, use continue 2.` |
|        - | 3308 | ` *  Note that switch/case does loose comparision.` |
|        - | 3309 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|        - | 3310 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|        - | 3311 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|        - | 3312 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|        - | 3313 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|        - | 3314 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|        - | 3315 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|        - | 3316 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|        - | 3317 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|        - | 3318 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|        - | 3319 | ` *  list for the next case.` |
|        - | 3320 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|        - | 3321 | ` *  or floating-point numbers and strings.` |
|        - | 3322 | ` */` |
|    11662 | 3323 | `PH7_PRIVATE sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|        5 | 3324 | `{` |
|        - | 3325 | `	GenBlock *pSwitchBlock;` |
|        - | 3326 | `	SyToken *pTmp,*pEnd;` |
|        - | 3327 | `	ph7_switch *pSwitch;` |
|        - | 3328 | `	sxu32 nToken;` |
|        - | 3329 | `	sxu32 nLine;` |
|        - | 3330 | `	sxi32 rc;` |
|    11667 | 3331 | `	nLine = pGen->pIn->nLine;` |
|        - | 3332 | `	/* Jump the 'switch' keyword */` |
|    11667 | 3333 | `	pGen->pIn++;` |
|    11667 | 3334 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 3335 | `		/* Syntax error */` |
|      ! 0 | 3336 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|      ! 0 | 3337 | `		if( rc == SXERR_ABORT ){` |
|        - | 3338 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 3339 | `			return SXERR_ABORT;` |
|        - | 3340 | `		}` |
|      ! 0 | 3341 | `		goto Synchronize;` |
|        - | 3342 | `	}` |
|        - | 3343 | `	/* Jump the left parenthesis '(' */` |
|    11667 | 3344 | `	pGen->pIn++;` |
|    11667 | 3345 | `	pEnd = 0; /* cc warning */` |
|        - | 3346 | `	/* Create the loop block */` |
|    17498 | 3347 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|     5831 | 3348 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|    11667 | 3349 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3350 | `		return SXERR_ABORT;` |
|        - | 3351 | `	}` |
|        - | 3352 | `	/* Delimit the condition */` |
|    11667 | 3353 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    11667 | 3354 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - | 3355 | `		/* Empty expression */` |
|      ! 0 | 3356 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|      ! 0 | 3357 | `		if( rc == SXERR_ABORT ){` |
|        - | 3358 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 3359 | `			return SXERR_ABORT;` |
|        - | 3360 | `		}` |
|      ! 0 | 3361 | `	}` |
|        - | 3362 | `	/* Swap token streams */` |
|    11667 | 3363 | `	pTmp = pGen->pEnd;` |
|    11667 | 3364 | `	pGen->pEnd = pEnd;` |
|        - | 3365 | `	/* Compile the expression */` |
|    11667 | 3366 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    11667 | 3367 | `	if( rc == SXERR_ABORT ){` |
|        - | 3368 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 3369 | `		return SXERR_ABORT;` |
|        - | 3370 | `	}` |
|        - | 3371 | `	/* Update token stream */` |
|    11667 | 3372 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 | 3373 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 3374 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 3375 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3376 | `			return SXERR_ABORT;` |
|        - | 3377 | `		}` |
|      ! 0 | 3378 | `		pGen->pIn++;` |
|      ! 0 | 3379 | `	}` |
|    11667 | 3380 | `	pGen->pIn  = &pEnd[1];` |
|    11667 | 3381 | `	pGen->pEnd = pTmp;` |
|    11667 | 3382 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|    11662 | 3383 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|      ! 0 | 3384 | `			pTmp = pGen->pIn;` |
|      ! 0 | 3385 | `			if( pTmp >= pGen->pEnd ){` |
|      ! 0 | 3386 | `				pTmp--;` |
|      ! 0 | 3387 | `			}` |
|        - | 3388 | `			/* Unexpected token */` |
|      ! 0 | 3389 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|      ! 0 | 3390 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3391 | `				return SXERR_ABORT;` |
|        - | 3392 | `			}` |
|      ! 0 | 3393 | `			goto Synchronize;` |
|        - | 3394 | `	}` |
|        - | 3395 | `	/* Set the delimiter token */` |
|    11667 | 3396 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|        3 | 3397 | `		nToken = PH7_TK_KEYWORD;` |
|        - | 3398 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|        2 | 3399 | `	}else{` |
|    11665 | 3400 | `		nToken = PH7_TK_CCB; /* '}' */` |
|        - | 3401 | `	}` |
|    11667 | 3402 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|        - | 3403 | `	/* Create the switch blocks container */` |
|    11667 | 3404 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|    11667 | 3405 | `	if( pSwitch == 0 ){` |
|        - | 3406 | `		/* Abort compilation */` |
|      ! 0 | 3407 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 3408 | `		return SXERR_ABORT;` |
|        - | 3409 | `	}` |
|        - | 3410 | `	/* Zero the structure */` |
|    11667 | 3411 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|        - | 3412 | `	/* Initialize fields */` |
|    11667 | 3413 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|        - | 3414 | `	/* Emit the switch instruction */` |
|    11667 | 3415 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|        - | 3416 | `	/* Compile case blocks */` |
|   130013 | 3417 | `	for(;;){` |
|        - | 3418 | `		sxu32 nKwrd;` |
|   135849 | 3419 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 3420 | `			/* No more input to process */` |
|      ! 0 | 3421 | `			break;` |
|        - | 3422 | `		}` |
|   135849 | 3423 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 3424 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|        - | 3425 | `				/* Unexpected token */` |
|      ! 0 | 3426 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 3427 | `					&pGen->pIn->sData);` |
|      ! 0 | 3428 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3429 | `					return SXERR_ABORT;` |
|        - | 3430 | `				}` |
|        - | 3431 | `				/* FALL THROUGH */` |
|      ! 0 | 3432 | `			}` |
|        - | 3433 | `			/* Block compiled */` |
|      ! 0 | 3434 | `			break;` |
|        - | 3435 | `		}` |
|        - | 3436 | `		/* Extract the keyword */` |
|   135849 | 3437 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   135849 | 3438 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|        3 | 3439 | `			if( nToken != PH7_TK_KEYWORD ){` |
|        - | 3440 | `				/* Unexpected token */` |
|      ! 0 | 3441 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 3442 | `					&pGen->pIn->sData);` |
|      ! 0 | 3443 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3444 | `					return SXERR_ABORT;` |
|        - | 3445 | `				}` |
|        - | 3446 | `				/* FALL THROUGH */` |
|      ! 0 | 3447 | `			}` |
|        - | 3448 | `			/* Block compiled */` |
|        3 | 3449 | `			break;` |
|        - | 3450 | `		}` |
|   135847 | 3451 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|        - | 3452 | `			/*` |
|        - | 3453 | `			 * Accroding to the PHP language reference manual` |
|        - | 3454 | `			 *  A special case is the default case. This case matches anything` |
|        - | 3455 | `			 *  that wasn't matched by the other cases.` |
|        - | 3456 | `			 */` |
|     3903 | 3457 | `			if( pSwitch->nDefault > 0 ){` |
|        - | 3458 | `				/* Default case already compiled */` |
|      ! 0 | 3459 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|      ! 0 | 3460 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3461 | `					return SXERR_ABORT;` |
|        - | 3462 | `				}` |
|      ! 0 | 3463 | `			}` |
|     3903 | 3464 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|        - | 3465 | `			/* Compile the default block */` |
|     3903 | 3466 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|     3903 | 3467 | `			if( rc == SXERR_ABORT){` |
|      ! 0 | 3468 | `				return SXERR_ABORT;` |
|     3903 | 3469 | `			}else if( rc == SXERR_EOF ){` |
|     3901 | 3470 | `				break;` |
|        1 | 3471 | `			}` |
|   131950 | 3472 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|        - | 3473 | `			ph7_case_expr sCase;` |
|        - | 3474 | `			/* Standard case block */` |
|   131949 | 3475 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|        - | 3476 | `			/* initialize the structure */` |
|   131949 | 3477 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - | 3478 | `			/* Compile the case expression */` |
|   131949 | 3479 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|   131949 | 3480 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3481 | `				return SXERR_ABORT;` |
|        - | 3482 | `			}` |
|        - | 3483 | `			/* Compile the case block */` |
|   131949 | 3484 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|        - | 3485 | `			/* Insert in the switch container */` |
|   131949 | 3486 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|   131949 | 3487 | `			if( rc == SXERR_ABORT){` |
|      ! 0 | 3488 | `				return SXERR_ABORT;` |
|   131949 | 3489 | `			}else if( rc == SXERR_EOF ){` |
|     7769 | 3490 | `				break;` |
|        - | 3491 | `			}` |
|    62095 | 3492 | `		}else{` |
|        - | 3493 | `			/* Unexpected token */` |
|      ! 0 | 3494 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 3495 | `				&pGen->pIn->sData);` |
|      ! 0 | 3496 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3497 | `				return SXERR_ABORT;` |
|        - | 3498 | `			}` |
|      ! 0 | 3499 | `			break;` |
|        - | 3500 | `		}` |
|        5 | 3501 | `	}` |
|        - | 3502 | `	/* Fix all jumps now the destination is resolved */` |
|    11667 | 3503 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|    11667 | 3504 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 3505 | `	/* Release the loop block */` |
|    11667 | 3506 | `	GenStateLeaveBlock(pGen,0);` |
|    11667 | 3507 | `	if( pGen->pIn < pGen->pEnd ){` |
|        - | 3508 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|    11667 | 3509 | `		pGen->pIn++;` |
|     5831 | 3510 | `	}` |
|        - | 3511 | `	/* Statement successfully compiled */` |
|    11667 | 3512 | `	return SXRET_OK;` |
|      ! 0 | 3513 | `Synchronize:` |
|        - | 3514 | `	/* Synchronize with the first semi-colon */` |
|      ! 0 | 3515 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      ! 0 | 3516 | `		pGen->pIn++;` |
|      ! 0 | 3517 | `	}` |
|      ! 0 | 3518 | `	return SXRET_OK;` |
|     5836 | 3519 | `}` |
|        - | 3520 |  |
