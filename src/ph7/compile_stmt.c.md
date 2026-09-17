# src/ph7/compile_stmt.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1457/1984 lines (73.44%)

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
|       49 |  142 | `		pGen->pIn++;` |
|        3 |  143 | `	}` |
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
|   177812 |  168 | `static int GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|        5 |  169 | `{` |
|   177817 |  170 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   177817 |  171 | `	int nInlineTry = 0;` |
|   718761 |  172 | `	while( pBlock && pBlock != pTarget ){` |
|   540949 |  173 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
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
|   540949 |  190 | `		pBlock = pBlock->pParent;` |
|        5 |  191 | `	}` |
|   177817 |  192 | `	return nInlineTry;` |
|        5 |  193 | `}` |
|    88878 |  194 | `PH7_PRIVATE sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|        5 |  195 | `{` |
|        - |  196 | `	GenBlock *pLoop; /* Target loop */` |
|        - |  197 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|        - |  198 | `	sxi32 iRawLevel; /* The level as WRITTEN, kept for php's diagnostics */` |
|        - |  199 | `	sxu32 nLineLocal;` |
|        - |  200 | `	sxi32 rc;` |
|    88883 |  201 | `	iRawLevel = 1;` |
|    88883 |  202 | `	nLineLocal = pGen->pIn->nLine;` |
|    88883 |  203 | `	iLevel = 0;` |
|        - |  204 | `	/* Jump the 'continue' keyword */` |
|    88883 |  205 | `	pGen->pIn++;` |
|    88883 |  206 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
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
|    88883 |  233 | `	if( iRawLevel < 1 ){` |
|      ! 0 |  234 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|        - |  235 | `			"'continue' operator accepts only positive integers");` |
|      ! 0 |  236 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  237 | `			return SXERR_ABORT;` |
|        - |  238 | `		}` |
|      ! 0 |  239 | `		return SXRET_OK;` |
|        - |  240 | `	}` |
|        - |  241 | `	/* Point to the target loop */` |
|    88883 |  242 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    88883 |  243 | `	if( pLoop == 0 ){` |
|        - |  244 | ``		/* Same split php makes for `break`: no loop at all keeps the not-in-context`` |
|        - |  245 | ``		 * wording, too FEW loops is `Cannot 'continue' N levels`. */`` |
|       12 |  246 | `		if( GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,0) != 0 ){` |
|      ! 0 |  247 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|      ! 0 |  248 | `				"Cannot 'continue' %d level%s",iRawLevel,iRawLevel == 1 ? "" : "s");` |
|      ! 0 |  249 | `		}else{` |
|       12 |  250 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"'continue' not in the 'loop' or 'switch' context");` |
|        - |  251 | `		}` |
|       12 |  252 | `		if( rc == SXERR_ABORT ){` |
|        - |  253 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  254 | `			return SXERR_ABORT;` |
|        - |  255 | `		}` |
|        7 |  256 | `	}else{` |
|    88873 |  257 | `		sxu32 nInstrIdx = 0;` |
|        - |  258 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|    88873 |  259 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|        - |  260 | `		/* ROOT C: in a generator, a break/continue crossing inline trys must run their` |
|        - |  261 | `		 * finallys first. OP_SET_FINALLY_JMP(iP1=count) does that then takes the loop jump. */` |
|    88873 |  262 | `		sxi32 iJmpOp = nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP;` |
|    88873 |  263 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
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
|    88869 |  279 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    88869 |  280 | `			if( pLoop->bPostContinue == TRUE ){` |
|        - |  281 | `				JumpFixup sJumpFix;` |
|        - |  282 | `				/* Post-continue */` |
|    27051 |  283 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|    27051 |  284 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|    27051 |  285 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|    13523 |  286 | `			}` |
|        - |  287 | `		}` |
|        - |  288 | `	}` |
|    88883 |  289 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  290 | `		/* Not so fatal,emit a warning only */` |
|      ! 0 |  291 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|      ! 0 |  292 | `	}` |
|        - |  293 | `	/* Statement successfully compiled */` |
|    88883 |  294 | `	return SXRET_OK;` |
|    44444 |  295 | `}` |
|        - |  296 | `/*` |
|        - |  297 | ` * Compile the 'break' statement.` |
|        - |  298 | ` * According to the PHP language reference` |
|        - |  299 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|        - |  300 | ` *  structure.` |
|        - |  301 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|        - |  302 | ` *  enclosing structures are to be broken out of.` |
|        - |  303 | ` */` |
|    88960 |  304 | `PH7_PRIVATE sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|        5 |  305 | `{` |
|        - |  306 | `	GenBlock *pLoop; /* Target loop */` |
|        - |  307 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|        - |  308 | `	sxi32 iRawLevel; /* The level as WRITTEN, kept for php's diagnostics */` |
|        - |  309 | `	sxi32 rc;` |
|    88965 |  310 | `	iLevel = 0;` |
|    88965 |  311 | `	iRawLevel = 1;` |
|        - |  312 | `	/* Jump the 'break' keyword */` |
|    88965 |  313 | `	pGen->pIn++;` |
|    88965 |  314 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
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
|       20 |  326 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|       12 |  327 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|       14 |  328 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  329 | `				return SXERR_ABORT;` |
|        - |  330 | `			}` |
|       14 |  331 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|       14 |  332 | `			iRawLevel = iLevel;` |
|       14 |  333 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        6 |  334 | `		}` |
|       17 |  335 | `		if( iLevel < 2 ){` |
|        3 |  336 | `			iLevel = 0;` |
|        1 |  337 | `		}` |
|       17 |  338 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|        7 |  339 | `	}` |
|        - |  340 | `	/* php rejects a non-positive level outright, before asking where it lands. */` |
|    88965 |  341 | `	if( iRawLevel < 1 ){` |
|      ! 0 |  342 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  343 | `			"'break' operator accepts only positive integers");` |
|      ! 0 |  344 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  345 | `			return SXERR_ABORT;` |
|        - |  346 | `		}` |
|      ! 0 |  347 | `		goto BreakLevelDone;` |
|        - |  348 | `	}` |
|        - |  349 | `	/* Extract the target loop */` |
|    88965 |  350 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|   133445 |  351 | `	if( pLoop == 0 ){` |
|        - |  352 | `		/* php distinguishes "there is no loop at all here" from "there is one, but` |
|        - |  353 | `		 * not N of them": the first keeps the not-in-context wording, the second is` |
|        - |  354 | ``		 * `Cannot 'break' N levels`. */`` |
|       18 |  355 | `		if( GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,0) != 0 ){` |
|        4 |  356 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        1 |  357 | `				"Cannot 'break' %d level%s",iRawLevel,iRawLevel == 1 ? "" : "s");` |
|        2 |  358 | `		}else{` |
|       16 |  359 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"'break' not in the 'loop' or 'switch' context");` |
|        - |  360 | `		}` |
|       18 |  361 | `		if( rc == SXERR_ABORT ){` |
|        - |  362 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  363 | `			return SXERR_ABORT;` |
|        - |  364 | `		}` |
|       10 |  365 | `	}else{` |
|        - |  366 | `		sxu32 nInstrIdx;` |
|        - |  367 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|    88949 |  368 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|        - |  369 | `		/* ROOT C: OP_SET_FINALLY_JMP runs the crossed inline finallys before the break jump. */` |
|    88949 |  370 | `		rc = PH7_VmEmitInstr(pGen->pVm,nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP,nCross,0,0,&nInstrIdx);` |
|    88949 |  371 | `		if( rc == SXRET_OK ){` |
|        - |  372 | `			/* Fix the jump later when the jump destination is resolved */` |
|    88949 |  373 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|    44472 |  374 | `		}` |
|        - |  375 | `	}` |
|    44480 |  376 | `BreakLevelDone:` |
|    88965 |  377 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  378 | `		/* Not so fatal,emit a warning only */` |
|      ! 0 |  379 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|      ! 0 |  380 | `	}` |
|        - |  381 | `	/* Statement successfully compiled */` |
|    88965 |  382 | `	return SXRET_OK;` |
|    44485 |  383 | `}` |
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
|       26 |  419 | `				break;` |
|        - |  420 | `			}` |
|        - |  421 | `			/* Point to the upper block */` |
|      121 |  422 | `			pBlock = pBlock->pParent;` |
|        5 |  423 | `		}` |
|      117 |  424 | `		if( pBlock ){` |
|       26 |  425 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       15 |  426 | `		}else{` |
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
|  6889326 |  590 | `PH7_PRIVATE sxi32 PH7_CompileBlock(` |
|        - |  591 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |  592 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|        - |  593 | `	)` |
|        5 |  594 | `{` |
|        - |  595 | `	sxi32 rc;` |
|        - |  596 | `	sxu32 nLine;` |
|  6889331 |  597 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  6764845 |  598 | `		nLine = pGen->pIn->nLine;` |
|  6764845 |  599 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  6764845 |  600 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  601 | `			return SXERR_ABORT;` |
|        - |  602 | `		}` |
|  6764845 |  603 | `		pGen->pIn++;` |
|        - |  604 | `		/* Compile until we hit the closing braces '}' */` |
|  9898278 |  605 | `		for(;;){` |
| 19796561 |  606 | `			if( pGen->pIn >= pGen->pEnd ){` |
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
| 19796541 |  618 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|        - |  619 | `				/* Closing braces found,break immediately*/` |
|  6764825 |  620 | `				pGen->pIn++;` |
|  6764825 |  621 | `				break;` |
|        - |  622 | `			}` |
|        - |  623 | `			/* Compile a single statement */` |
| 13031721 |  624 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
| 13031721 |  625 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  626 | `				return SXERR_ABORT;` |
|        - |  627 | `			}` |
|        5 |  628 | `		}` |
|  6764845 |  629 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  3506911 |  630 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
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
|   124491 |  674 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|   124491 |  675 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  676 | `			return SXERR_ABORT;` |
|        - |  677 | `		}` |
|        - |  678 | `	}` |
|        - |  679 | `	/* Jump trailing semi-colons ';' */` |
|  6889331 |  680 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      ! 0 |  681 | `		pGen->pIn++;` |
|      ! 0 |  682 | `	}` |
|  6889331 |  683 | `	return SXRET_OK;` |
|  3444668 |  684 | `}` |
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
|    73522 |  704 | `PH7_PRIVATE sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|        5 |  705 | `{` |
|    73527 |  706 | `	GenBlock *pWhileBlock = 0;` |
|    73527 |  707 | `	SyToken *pTmp,*pEnd = 0;` |
|        - |  708 | `	sxu32 nFalseJump;` |
|        - |  709 | `	sxu32 nLine;` |
|        - |  710 | `	sxi32 rc;` |
|    73527 |  711 | `	nLine = pGen->pIn->nLine;` |
|        - |  712 | `	/* Jump the 'while' keyword */` |
|    73527 |  713 | `	pGen->pIn++;` |
|    73527 |  714 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  715 | `		/* Syntax error */` |
|      ! 0 |  716 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|      ! 0 |  717 | `		if( rc == SXERR_ABORT ){` |
|        - |  718 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  719 | `			return SXERR_ABORT;` |
|        - |  720 | `		}` |
|      ! 0 |  721 | `		goto Synchronize;` |
|        - |  722 | `	}` |
|        - |  723 | `	/* Jump the left parenthesis '(' */` |
|    73527 |  724 | `	pGen->pIn++;` |
|        - |  725 | `	/* Create the loop block */` |
|    73527 |  726 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|    73527 |  727 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  728 | `		return SXERR_ABORT;` |
|        - |  729 | `	}` |
|        - |  730 | `	/* Delimit the condition */` |
|    73527 |  731 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    73527 |  732 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  733 | `		/* Empty condition. php reports the token that actually stopped it -- for` |
|        - |  734 | ``		 * `while ()` that is the ')' -- not a hand-written "expected expression". */`` |
|        3 |  735 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,0);` |
|        3 |  736 | `		if( rc == SXERR_ABORT ){` |
|        - |  737 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  738 | `			return SXERR_ABORT;` |
|        - |  739 | `		}` |
|        1 |  740 | `	}` |
|        - |  741 | `	/* Swap token streams */` |
|    73527 |  742 | `	pTmp = pGen->pEnd;` |
|    73527 |  743 | `	pGen->pEnd = pEnd;` |
|        - |  744 | `	/* Compile the expression */` |
|    73527 |  745 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    73527 |  746 | `	if( rc == SXERR_ABORT ){` |
|        - |  747 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  748 | `		return SXERR_ABORT;` |
|        - |  749 | `	}` |
|        - |  750 | `	/* Update token stream */` |
|    73527 |  751 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 |  752 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 |  753 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  754 | `			return SXERR_ABORT;` |
|        - |  755 | `		}` |
|      ! 0 |  756 | `		pGen->pIn++;` |
|      ! 0 |  757 | `	}` |
|        - |  758 | `	/* Synchronize pointers */` |
|    73527 |  759 | `	pGen->pIn  = &pEnd[1];` |
|    73527 |  760 | `	pGen->pEnd = pTmp;` |
|        - |  761 | `	/* Emit the false jump */` |
|    73527 |  762 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|        - |  763 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    73527 |  764 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|        - |  765 | `	/* Compile the loop body */` |
|    73527 |  766 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|    73527 |  767 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  768 | `		return SXERR_ABORT;` |
|        - |  769 | `	}` |
|        - |  770 | `	/* Emit the unconditional jump to the start of the loop */` |
|    73527 |  771 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|        - |  772 | `	/* Fix all jumps now the destination is resolved */` |
|    73527 |  773 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  774 | `	/* Release the loop block */` |
|    73527 |  775 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  776 | `	/* Statement successfully compiled */` |
|    73527 |  777 | `	return SXRET_OK;` |
|      ! 0 |  778 | `Synchronize:` |
|        - |  779 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - |  780 | `	 * compiling this erroneous block.` |
|        - |  781 | `	 */` |
|      ! 0 |  782 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  783 | `		pGen->pIn++;` |
|      ! 0 |  784 | `	}` |
|      ! 0 |  785 | `	return SXRET_OK;` |
|    36766 |  786 | `}` |
|        - |  787 | `/*` |
|        - |  788 | ` * Compile the ugly do..while() statement.` |
|        - |  789 | ` * According to the PHP language reference` |
|        - |  790 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|        - |  791 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|        - |  792 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|        - |  793 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|        - |  794 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|        - |  795 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|        - |  796 | ` *  would end immediately).` |
|        - |  797 | ` *  There is just one syntax for do-while loops:` |
|        - |  798 | ` *  <?php` |
|        - |  799 | ` *  $i = 0;` |
|        - |  800 | ` *  do {` |
|        - |  801 | ` *   echo $i;` |
|        - |  802 | ` *  } while ($i > 0);` |
|        - |  803 | ` * ?>` |
|        - |  804 | ` */` |
|        2 |  805 | `PH7_PRIVATE sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|        1 |  806 | `{` |
|        3 |  807 | `	SyToken *pTmp,*pEnd = 0;` |
|        3 |  808 | `	GenBlock *pDoBlock = 0;` |
|        - |  809 | `	sxu32 nLine;` |
|        - |  810 | `	sxi32 rc;` |
|        3 |  811 | `	nLine = pGen->pIn->nLine;` |
|        - |  812 | `	/* Jump the 'do' keyword */` |
|        3 |  813 | `	pGen->pIn++;` |
|        - |  814 | `	/* Create the loop block */` |
|        3 |  815 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|        3 |  816 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  817 | `		return SXERR_ABORT;` |
|        - |  818 | `	}` |
|        - |  819 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|        3 |  820 | `	pDoBlock->bPostContinue = TRUE;` |
|        3 |  821 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|        3 |  822 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  823 | `		return SXERR_ABORT;` |
|        - |  824 | `	}` |
|        3 |  825 | `	if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 |  826 | `		nLine = pGen->pIn->nLine;` |
|      ! 0 |  827 | `	}` |
|        3 |  828 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|      ! 0 |  829 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|        - |  830 | `			/* Missing 'while' statement */` |
|        - |  831 | ``			/* php: `do {} ;` names the ';', while `do {}` at EOF is "unexpected end of`` |
|        - |  832 | `			 * file". The do-block's own slice stops before its terminator, so look at` |
|        - |  833 | `			 * the whole CHUNK stream: a token still there is the one php names; nothing` |
|        - |  834 | `			 * left means end of file (NULL). */` |
|        - |  835 | `			{` |
|        3 |  836 | `				SyToken *pBad = pGen->pIn < pGen->pEnd ? pGen->pIn : 0;` |
|        3 |  837 | `				if( pBad == 0 && pGen->pTokenSet ){` |
|        - |  838 | `					/* The do-block consumed its terminator, so the token php names is` |
|        - |  839 | `					 * the one just behind the cursor -- but only when it is a real` |
|        - |  840 | ``					 * terminator (`do {} ;` -> the ';'). If the block simply ended on`` |
|        - |  841 | `					 * its '}' with nothing after it, php reports end of file. */` |
|        3 |  842 | `					SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        3 |  843 | `					if( pGen->pIn > pBase && (pGen->pIn[-1].nType & PH7_TK_SEMI) ){` |
|      ! 0 |  844 | `						pBad = &pGen->pIn[-1];` |
|      ! 0 |  845 | `					}` |
|        1 |  846 | `				}` |
|        3 |  847 | `				rc = PH7_GenSyntaxError(pGen,pBad,"\"while\"");` |
|        - |  848 | `			}` |
|        3 |  849 | `			if( rc == SXERR_ABORT ){` |
|        - |  850 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  851 | `				return SXERR_ABORT;` |
|        - |  852 | `			}` |
|        3 |  853 | `			goto Synchronize;` |
|        - |  854 | `	}` |
|        - |  855 | `	/* Jump the 'while' keyword */` |
|      ! 0 |  856 | `	pGen->pIn++;` |
|      ! 0 |  857 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  858 | `		/* Syntax error */` |
|      ! 0 |  859 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|      ! 0 |  860 | `		if( rc == SXERR_ABORT ){` |
|        - |  861 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  862 | `			return SXERR_ABORT;` |
|        - |  863 | `		}` |
|      ! 0 |  864 | `		goto Synchronize;` |
|        - |  865 | `	}` |
|        - |  866 | `	/* Jump the left parenthesis '(' */` |
|      ! 0 |  867 | `	pGen->pIn++;` |
|        - |  868 | `	/* Delimit the condition */` |
|      ! 0 |  869 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      ! 0 |  870 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  871 | `		/* Empty condition. php reports the token that actually stopped it -- for` |
|        - |  872 | ``		 * `while ()` that is the ')' -- not a hand-written "expected expression". */`` |
|      ! 0 |  873 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,0);` |
|      ! 0 |  874 | `		if( rc == SXERR_ABORT ){` |
|        - |  875 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  876 | `			return SXERR_ABORT;` |
|        - |  877 | `		}` |
|      ! 0 |  878 | `		goto Synchronize;` |
|        - |  879 | `	}` |
|        - |  880 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|      ! 0 |  881 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|        - |  882 | `		JumpFixup *aPost;` |
|        - |  883 | `		VmInstr *pInstr;` |
|        - |  884 | `		sxu32 nJumpDest;` |
|        - |  885 | `		sxu32 n;` |
|      ! 0 |  886 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|      ! 0 |  887 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      ! 0 |  888 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|      ! 0 |  889 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      ! 0 |  890 | `			if( pInstr ){` |
|        - |  891 | `				/* Fix */` |
|      ! 0 |  892 | `				pInstr->iP2 = nJumpDest;` |
|      ! 0 |  893 | `			}` |
|      ! 0 |  894 | `		}` |
|      ! 0 |  895 | `	}` |
|        - |  896 | `	/* Swap token streams */` |
|      ! 0 |  897 | `	pTmp = pGen->pEnd;` |
|      ! 0 |  898 | `	pGen->pEnd = pEnd;` |
|        - |  899 | `	/* Compile the expression */` |
|      ! 0 |  900 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      ! 0 |  901 | `	if( rc == SXERR_ABORT ){` |
|        - |  902 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  903 | `		return SXERR_ABORT;` |
|        - |  904 | `	}` |
|        - |  905 | `	/* Update token stream */` |
|      ! 0 |  906 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 |  907 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 |  908 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  909 | `			return SXERR_ABORT;` |
|        - |  910 | `		}` |
|      ! 0 |  911 | `		pGen->pIn++;` |
|      ! 0 |  912 | `	}` |
|      ! 0 |  913 | `	pGen->pIn  = &pEnd[1];` |
|      ! 0 |  914 | `	pGen->pEnd = pTmp;` |
|        - |  915 | `	/* Emit the true jump to the beginning of the loop */` |
|      ! 0 |  916 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|        - |  917 | `	/* Fix all jumps now the destination is resolved */` |
|      ! 0 |  918 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  919 | `	/* Release the loop block */` |
|      ! 0 |  920 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  921 | `	/* Statement successfully compiled */` |
|      ! 0 |  922 | `	return SXRET_OK;` |
|        1 |  923 | `Synchronize:` |
|        - |  924 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - |  925 | `	 * compiling this erroneous block.` |
|        - |  926 | `	 */` |
|        3 |  927 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  928 | `		pGen->pIn++;` |
|      ! 0 |  929 | `	}` |
|        3 |  930 | `	return SXRET_OK;` |
|        2 |  931 | `}` |
|        - |  932 | `/*` |
|        - |  933 | ` * Compile the complex and powerful 'for' statement.` |
|        - |  934 | ` * According to the PHP language reference` |
|        - |  935 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|        - |  936 | ` *  The syntax of a for loop is:` |
|        - |  937 | ` *  for (expr1; expr2; expr3)` |
|        - |  938 | ` *   statement` |
|        - |  939 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|        - |  940 | ` *  the beginning of the loop.` |
|        - |  941 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|        - |  942 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|        - |  943 | ` *  to FALSE, the execution of the loop ends.` |
|        - |  944 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|        - |  945 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|        - |  946 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|        - |  947 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|        - |  948 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|        - |  949 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|        - |  950 | ` *  of using the for truth expression.` |
|        - |  951 | ` */` |
|   127600 |  952 | `PH7_PRIVATE sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|        5 |  953 | `{` |
|   127605 |  954 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   127605 |  955 | `	GenBlock *pForBlock = 0;` |
|        - |  956 | `	sxu32 nFalseJump;` |
|        - |  957 | `	sxu32 nLine;` |
|        - |  958 | `	sxi32 rc;` |
|   127605 |  959 | `	nLine = pGen->pIn->nLine;` |
|        - |  960 | `	/* Jump the 'for' keyword */` |
|   127605 |  961 | `	pGen->pIn++;` |
|   127605 |  962 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  963 | `		/* Syntax error */` |
|      ! 0 |  964 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|      ! 0 |  965 | `		if( rc == SXERR_ABORT ){` |
|        - |  966 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  967 | `			return SXERR_ABORT;` |
|        - |  968 | `		}` |
|      ! 0 |  969 | `		return SXRET_OK;` |
|        - |  970 | `	}` |
|        - |  971 | `	/* Jump the left parenthesis '(' */` |
|   127605 |  972 | `	pGen->pIn++;` |
|        - |  973 | `	/* Delimit the init-expr;condition;post-expr */` |
|   127605 |  974 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   127605 |  975 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  976 | `		/* Empty expression */` |
|      ! 0 |  977 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|      ! 0 |  978 | `		if( rc == SXERR_ABORT ){` |
|        - |  979 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  980 | `			return SXERR_ABORT;` |
|        - |  981 | `		}` |
|        - |  982 | `		/* Synchronize */` |
|      ! 0 |  983 | `		pGen->pIn = pEnd;` |
|      ! 0 |  984 | `		if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 |  985 | `			pGen->pIn++;` |
|      ! 0 |  986 | `		}` |
|      ! 0 |  987 | `		return SXRET_OK;` |
|        - |  988 | `	}` |
|        - |  989 | `	/* Swap token streams */` |
|   127605 |  990 | `	pTmp = pGen->pEnd;` |
|   127605 |  991 | `	pGen->pEnd = pEnd;` |
|        - |  992 | `	/* for() clauses are the ONLY place php's grammar allows a comma-separated` |
|        - |  993 | `	 * expression list, so the comma operator is permitted for their duration` |
|        - |  994 | `	 * (see GenStateTreeHasComma). A closure body nested inside a clause is` |
|        - |  995 | `	 * compiled through this same window — recorded as a known leniency. */` |
|   127605 |  996 | `	pGen->nCommaExprOk++;` |
|   127605 |  997 | `	pGen->zClauseCloser = "\";\""; /* init/condition clauses close on ';' */` |
|        - |  998 | `	/* Compile initialization expressions if available */` |
|   127605 |  999 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 1000 | `	/* Pop operand lvalues */` |
|   127605 | 1001 | `	if( rc == SXERR_ABORT ){` |
|        - | 1002 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1003 | `		return SXERR_ABORT;` |
|   127605 | 1004 | `	}else if( rc != SXERR_EMPTY ){` |
|   116017 | 1005 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    58006 | 1006 | `	}` |
|   127605 | 1007 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - | 1008 | `		/* Syntax error */` |
|      ! 0 | 1009 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|      ! 0 | 1010 | `		if( rc == SXERR_ABORT ){` |
|        - | 1011 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1012 | `			return SXERR_ABORT;` |
|        - | 1013 | `		}` |
|      ! 0 | 1014 | `		return SXRET_OK;` |
|        - | 1015 | `	}` |
|        - | 1016 | `	/* Jump the trailing ';' */` |
|   127605 | 1017 | `	pGen->pIn++;` |
|        - | 1018 | `	/* Create the loop block */` |
|   127605 | 1019 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   127605 | 1020 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1021 | `		return SXERR_ABORT;` |
|        - | 1022 | `	}` |
|        - | 1023 | `	/* Deffer continue jumps */` |
|   127605 | 1024 | `	pForBlock->bPostContinue = TRUE;` |
|        - | 1025 | `	/* Compile the condition */` |
|   127605 | 1026 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   127605 | 1027 | `	if( rc == SXERR_ABORT ){` |
|        - | 1028 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1029 | `		return SXERR_ABORT;` |
|   127605 | 1030 | `	}else if( rc != SXERR_EMPTY ){` |
|        - | 1031 | `		/* Emit the false jump */` |
|   116017 | 1032 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|        - | 1033 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   116017 | 1034 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    58006 | 1035 | `	}` |
|   127605 | 1036 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - | 1037 | `		/* Syntax error */` |
|        6 | 1038 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|        6 | 1039 | `		if( rc == SXERR_ABORT ){` |
|        - | 1040 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1041 | `			return SXERR_ABORT;` |
|        - | 1042 | `		}` |
|        6 | 1043 | `		return SXRET_OK;` |
|        - | 1044 | `	}` |
|        - | 1045 | `	/* Jump the trailing ';' */` |
|   127601 | 1046 | `	pGen->pIn++;` |
|        - | 1047 | `	/* Save the post condition stream */` |
|   127601 | 1048 | `	pPostStart = pGen->pIn;` |
|        - | 1049 | `	/* Compile the loop body — OUTSIDE the comma window (the body is ordinary` |
|        - | 1050 | ``	 * php, so `(1, 2)` inside it is the parse error it should be). */`` |
|   127601 | 1051 | `	pGen->nCommaExprOk--;` |
|   127601 | 1052 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   127601 | 1053 | `	pGen->pEnd = pTmp;` |
|   127601 | 1054 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   127601 | 1055 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 1056 | `		return SXERR_ABORT;` |
|        - | 1057 | `	}` |
|        - | 1058 | `	/* Fix post-continue jumps */` |
|   127601 | 1059 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|        - | 1060 | `		JumpFixup *aPost;` |
|        - | 1061 | `		VmInstr *pInstr;` |
|        - | 1062 | `		sxu32 nJumpDest;` |
|        - | 1063 | `		sxu32 n;` |
|    11603 | 1064 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|    11603 | 1065 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|    38649 | 1066 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|    27051 | 1067 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|    27051 | 1068 | `			if( pInstr ){` |
|        - | 1069 | `				/* Fix jump */` |
|    27051 | 1070 | `				pInstr->iP2 = nJumpDest;` |
|    13523 | 1071 | `			}` |
|    13528 | 1072 | `		}` |
|     5799 | 1073 | `	}` |
|        - | 1074 | `	/* compile the post-expressions if available */` |
|   127601 | 1075 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|      ! 0 | 1076 | `		pPostStart++;` |
|      ! 0 | 1077 | `	}` |
|   127601 | 1078 | `	if( pPostStart < pEnd ){` |
|        - | 1079 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   116015 | 1080 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   116015 | 1081 | `		pGen->nCommaExprOk++; /* post-expressions are a clause list again */` |
|   116015 | 1082 | `		pGen->zClauseCloser = "\")\""; /* the post clause closes on ')' */` |
|   116015 | 1083 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   116015 | 1084 | `		pGen->nCommaExprOk--;` |
|   116015 | 1085 | `		pGen->zClauseCloser = 0;` |
|   116015 | 1086 | `		if( pGen->pIn < pGen->pEnd ){` |
|        - | 1087 | `			/* php names the token and expects ')'; the post-clause runs to the ')'. */` |
|      ! 0 | 1088 | `			rc = PH7_GenSyntaxError(pGen,pGen->pIn,"\")\"");` |
|      ! 0 | 1089 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1090 | `				return SXERR_ABORT;` |
|        - | 1091 | `			}` |
|      ! 0 | 1092 | `			return SXRET_OK;` |
|        - | 1093 | `		}` |
|   116015 | 1094 | `		RE_SWAP_DELIMITER(pGen);` |
|   116015 | 1095 | `		if( rc == SXERR_ABORT ){` |
|        - | 1096 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1097 | `			return SXERR_ABORT;` |
|   116015 | 1098 | `		}else if( rc != SXERR_EMPTY){` |
|        - | 1099 | `			/* Pop operand lvalue */` |
|   116015 | 1100 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    58005 | 1101 | `		}` |
|    58005 | 1102 | `	}` |
|        - | 1103 | `	/* Emit the unconditional jump to the start of the loop */` |
|   127601 | 1104 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|        - | 1105 | `	/* Fix all jumps now the destination is resolved */` |
|   127601 | 1106 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 1107 | `	/* Release the loop block */` |
|   127601 | 1108 | `	GenStateLeaveBlock(pGen,0);` |
|        - | 1109 | `	/* Statement successfully compiled */` |
|   127601 | 1110 | `	return SXRET_OK;` |
|    63805 | 1111 | `}` |
|        - | 1112 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|        - | 1113 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|        - | 1114 | ` * are allowed.` |
|        - | 1115 | ` */` |
|   460724 | 1116 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 1117 | `{` |
|   460729 | 1118 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|   460729 | 1119 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|        - | 1120 | `		/* Unexpected expression */` |
|      ! 0 | 1121 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 1122 | `			"foreach: Expecting a variable name");` |
|      ! 0 | 1123 | `		if( rc != SXERR_ABORT ){` |
|      ! 0 | 1124 | `			rc = SXERR_INVALID;` |
|      ! 0 | 1125 | `		}` |
|      ! 0 | 1126 | `	}` |
|   460729 | 1127 | `	return rc;` |
|        5 | 1128 | `}` |
|        - | 1129 | `/*` |
|        - | 1130 | ` * Compile the 'foreach' statement.` |
|        - | 1131 | ` * According to the PHP language reference` |
|        - | 1132 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|        - | 1133 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|        - | 1134 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|        - | 1135 | ` *  is a minor but useful extension of the first:` |
|        - | 1136 | ` *  foreach (array_expression as $value)` |
|        - | 1137 | ` *    statement` |
|        - | 1138 | ` *  foreach (array_expression as $key => $value)` |
|        - | 1139 | ` *   statement` |
|        - | 1140 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|        - | 1141 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|        - | 1142 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|        - | 1143 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|        - | 1144 | ` *  to the variable $key on each loop.` |
|        - | 1145 | ` *  Note:` |
|        - | 1146 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|        - | 1147 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|        - | 1148 | ` *  Note:` |
|        - | 1149 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|        - | 1150 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|        - | 1151 | ` *  or after the foreach without resetting it.` |
|        - | 1152 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|        - | 1153 | ` *  of copying the value.` |
|        - | 1154 | ` */` |
|   325254 | 1155 | `PH7_PRIVATE sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|        5 | 1156 | `{` |
|   325259 | 1157 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|   325259 | 1158 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|   325259 | 1159 | `	GenBlock *pForeachBlock = 0;` |
|        - | 1160 | `	ph7_foreach_info *pInfo;` |
|        - | 1161 | `	sxu32 nFalseJump;` |
|        - | 1162 | `	VmInstr *pInstr;` |
|        - | 1163 | `	sxu32 nLine;` |
|        - | 1164 | `	sxi32 rc;` |
|   325259 | 1165 | `	nLine = pGen->pIn->nLine;` |
|        - | 1166 | `	/* Jump the 'foreach' keyword */` |
|   325259 | 1167 | `	pGen->pIn++;` |
|   325259 | 1168 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1169 | `		/* Syntax error */` |
|      ! 0 | 1170 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|      ! 0 | 1171 | `		if( rc == SXERR_ABORT ){` |
|        - | 1172 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1173 | `			return SXERR_ABORT;` |
|        - | 1174 | `		}` |
|      ! 0 | 1175 | `		goto Synchronize;` |
|        - | 1176 | `	}` |
|        - | 1177 | `	/* Jump the left parenthesis '(' */` |
|   325259 | 1178 | `	pGen->pIn++;` |
|        - | 1179 | `	/* Create the loop block */` |
|   325259 | 1180 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|   325259 | 1181 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1182 | `		return SXERR_ABORT;` |
|        - | 1183 | `	}` |
|        - | 1184 | `	/* Delimit the expression */` |
|   325259 | 1185 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   325259 | 1186 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - | 1187 | `		/* Empty expression */` |
|      ! 0 | 1188 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|      ! 0 | 1189 | `		if( rc == SXERR_ABORT ){` |
|        - | 1190 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1191 | `			return SXERR_ABORT;` |
|        - | 1192 | `		}` |
|        - | 1193 | `		/* Synchronize */` |
|      ! 0 | 1194 | `		pGen->pIn = pEnd;` |
|      ! 0 | 1195 | `		if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 | 1196 | `			pGen->pIn++;` |
|      ! 0 | 1197 | `		}` |
|      ! 0 | 1198 | `		return SXRET_OK;` |
|        - | 1199 | `	}` |
|        - | 1200 | `	/* Compile the array expression */` |
|   325259 | 1201 | `	pCur = pGen->pIn;` |
|  1843505 | 1202 | `	while( pCur < pEnd ){` |
|  1843505 | 1203 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|   356177 | 1204 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|   356177 | 1205 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|        - | 1206 | `				/* Break with the first 'as' found */` |
|   325259 | 1207 | `				break;` |
|        - | 1208 | `			}` |
|    15459 | 1209 | `		}` |
|        - | 1210 | `		/* Advance the stream cursor */` |
|  1518251 | 1211 | `		pCur++;` |
|        5 | 1212 | `	}` |
|   325259 | 1213 | `	if( pCur <= pGen->pIn ){` |
|      ! 0 | 1214 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 1215 | `			"foreach: Missing array/object expression");` |
|      ! 0 | 1216 | `		if( rc == SXERR_ABORT ){` |
|        - | 1217 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1218 | `			return SXERR_ABORT;` |
|        - | 1219 | `		}` |
|      ! 0 | 1220 | `		goto Synchronize;` |
|        - | 1221 | `	}` |
|        - | 1222 | `	/* Swap token streams */` |
|   325259 | 1223 | `	pTmp = pGen->pEnd;` |
|   325259 | 1224 | `	pGen->pEnd = pCur;` |
|   325259 | 1225 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   325259 | 1226 | `	if( rc == SXERR_ABORT ){` |
|        - | 1227 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1228 | `		return SXERR_ABORT;` |
|        - | 1229 | `	}` |
|        - | 1230 | `	/* Update token stream */` |
|   325259 | 1231 | `	while(pGen->pIn < pCur ){` |
|      ! 0 | 1232 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 1233 | `		if( rc == SXERR_ABORT ){` |
|        - | 1234 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1235 | `			return SXERR_ABORT;` |
|        - | 1236 | `		}` |
|      ! 0 | 1237 | `		pGen->pIn++;` |
|      ! 0 | 1238 | `	}` |
|   325259 | 1239 | `	pCur++; /* Jump the 'as' keyword */` |
|   325259 | 1240 | `	pGen->pIn = pCur;` |
|   325259 | 1241 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 1242 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|      ! 0 | 1243 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1244 | `			return SXERR_ABORT;` |
|        - | 1245 | `		}` |
|      ! 0 | 1246 | `	}` |
|        - | 1247 | `	/* Create the foreach context */` |
|   325259 | 1248 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|   325259 | 1249 | `	if( pInfo == 0 ){` |
|      ! 0 | 1250 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|      ! 0 | 1251 | `		return SXERR_ABORT;` |
|        - | 1252 | `	}` |
|        - | 1253 | `	/* Zero the structure */` |
|   325259 | 1254 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|        - | 1255 | `	/* Initialize structure fields */` |
|   325259 | 1256 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|        - | 1257 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|        - | 1258 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|        - | 1259 | `	 * '=>'. */` |
|   325259 | 1260 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|   325259 | 1261 | `	if( pCur < pEnd ){` |
|        - | 1262 | `		/* Compile the expression holding the key name */` |
|   135499 | 1263 | `		if( pGen->pIn >= pCur ){` |
|      ! 0 | 1264 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|      ! 0 | 1265 | `			if( rc == SXERR_ABORT ){` |
|        - | 1266 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1267 | `				return SXERR_ABORT;` |
|        - | 1268 | `			}` |
|      ! 0 | 1269 | `		}else{` |
|   135499 | 1270 | `			pGen->pEnd = pCur;` |
|   135499 | 1271 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|   135499 | 1272 | `			if( rc == SXERR_ABORT ){` |
|        - | 1273 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1274 | `				return SXERR_ABORT;` |
|        - | 1275 | `			}` |
|   135499 | 1276 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|   135499 | 1277 | `			if( pInstr->p3 ){` |
|        - | 1278 | `				/* Record key name */` |
|   135499 | 1279 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    67747 | 1280 | `			}` |
|   135499 | 1281 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|        - | 1282 | `		}` |
|   135499 | 1283 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    67747 | 1284 | `	}` |
|   325259 | 1285 | `	pGen->pEnd = pEnd;` |
|   325259 | 1286 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 1287 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|      ! 0 | 1288 | `		if( rc == SXERR_ABORT ){` |
|        - | 1289 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1290 | `			return SXERR_ABORT;` |
|        - | 1291 | `		}` |
|      ! 0 | 1292 | `		goto Synchronize;` |
|        - | 1293 | `	}` |
|   325259 | 1294 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|       33 | 1295 | `		pGen->pIn++;` |
|        - | 1296 | `		/* Pass by reference  */` |
|       33 | 1297 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       15 | 1298 | `	}` |
|        - | 1299 | `	/* Check if the value target is list() */` |
|   325259 | 1300 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        8 | 1301 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|        - | 1302 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|        - | 1303 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|        - | 1304 | `		 */` |
|        - | 1305 | `		static int iForeachListCnt = 0;` |
|        - | 1306 | `		char zTmp[128];` |
|        - | 1307 | `		sxu32 nLen;` |
|        - | 1308 | `		char *zDup;` |
|       10 | 1309 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|       10 | 1310 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       10 | 1311 | `		if( zDup == 0 ){` |
|      ! 0 | 1312 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1313 | `			return SXERR_ABORT;` |
|        - | 1314 | `		}` |
|       10 | 1315 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|        - | 1316 | `		/* Save list() token boundaries */` |
|       10 | 1317 | `		pListStart = pGen->pIn;` |
|        - | 1318 | `		/* Advance past list(...) — validate parentheses */` |
|       10 | 1319 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|       10 | 1320 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1321 | `			/* php: syntax error, unexpected variable "$x", expecting "(" */` |
|        3 | 1322 | `			rc = PH7_GenSyntaxError(pGen,pGen->pIn < pEnd ? pGen->pIn : 0,"\"(\"");` |
|        3 | 1323 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1324 | `				return SXERR_ABORT;` |
|        - | 1325 | `			}` |
|        3 | 1326 | `			goto Synchronize;` |
|        - | 1327 | `		}` |
|        7 | 1328 | `		pGen->pIn++; /* Jump '(' */` |
|        7 | 1329 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|        7 | 1330 | `		if( pListEnd >= pEnd ){` |
|      ! 0 | 1331 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1332 | `				"foreach: Missing closing ')' after list");` |
|      ! 0 | 1333 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1334 | `				return SXERR_ABORT;` |
|        - | 1335 | `			}` |
|      ! 0 | 1336 | `			goto Synchronize;` |
|        - | 1337 | `		}` |
|        7 | 1338 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|        7 | 1339 | `		pListEnd = pGen->pIn;` |
|        7 | 1340 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|   325254 | 1341 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|        - | 1342 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|        - | 1343 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|        - | 1344 | `		 */` |
|        - | 1345 | `		static int iForeachShortListCnt = 0;` |
|        - | 1346 | `		char zTmp[128];` |
|        - | 1347 | `		sxu32 nLen;` |
|        - | 1348 | `		char *zDup;` |
|       17 | 1349 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       17 | 1350 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       17 | 1351 | `		if( zDup == 0 ){` |
|      ! 0 | 1352 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1353 | `			return SXERR_ABORT;` |
|        - | 1354 | `		}` |
|       17 | 1355 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|        - | 1356 | `		/* Save [...] token boundaries */` |
|       17 | 1357 | `		pListStart = pGen->pIn;` |
|        - | 1358 | `		/* Advance past [...] */` |
|       17 | 1359 | `		pGen->pIn++; /* Jump '[' */` |
|       17 | 1360 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       17 | 1361 | `		if( pListEnd >= pEnd ){` |
|      ! 0 | 1362 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1363 | `				"foreach: Missing closing ']' after short list");` |
|      ! 0 | 1364 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1365 | `				return SXERR_ABORT;` |
|        - | 1366 | `			}` |
|      ! 0 | 1367 | `			goto Synchronize;` |
|        - | 1368 | `		}` |
|       17 | 1369 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       17 | 1370 | `		pListEnd = pGen->pIn;` |
|       17 | 1371 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|        9 | 1372 | `	}else{` |
|        - | 1373 | `		/* Compile the expression holding the value name */` |
|   325235 | 1374 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|   325235 | 1375 | `		if( rc == SXERR_ABORT ){` |
|        - | 1376 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1377 | `			return SXERR_ABORT;` |
|        - | 1378 | `		}` |
|   325235 | 1379 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|   325235 | 1380 | `		if( pInstr->p3 ){` |
|        - | 1381 | `			/* Record value name */` |
|   325235 | 1382 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|   162615 | 1383 | `		}` |
|        - | 1384 | `	}` |
|        - | 1385 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|   325257 | 1386 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|        - | 1387 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   325257 | 1388 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|        - | 1389 | `	/* Record the first instruction to execute */` |
|   325257 | 1390 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|        - | 1391 | `	/* Emit the FOREACH_STEP instruction */` |
|   325257 | 1392 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|        - | 1393 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   325257 | 1394 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|        - | 1395 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|   325257 | 1396 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|        - | 1397 | `		SyToken *pSavedIn,*pSavedEnd;` |
|        - | 1398 | `		/* Load the temporary variable holding the current value onto the stack.` |
|        - | 1399 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|        - | 1400 | `		 */` |
|       23 | 1401 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|        - | 1402 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|        - | 1403 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|        - | 1404 | `		 * picks up the delimiter and the variable names inside.` |
|        - | 1405 | `		 */` |
|       23 | 1406 | `		pSavedIn = pGen->pIn;` |
|       23 | 1407 | `		pSavedEnd = pGen->pEnd;` |
|       23 | 1408 | `		pGen->pIn = pListStart;` |
|       23 | 1409 | `		pGen->pEnd = pListEnd;` |
|       23 | 1410 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       17 | 1411 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|        9 | 1412 | `		}else{` |
|        7 | 1413 | `			rc = PH7_CompileList(&(*pGen),0);` |
|        - | 1414 | `		}` |
|       23 | 1415 | `		pGen->pIn = pSavedIn;` |
|       23 | 1416 | `		pGen->pEnd = pSavedEnd;` |
|       23 | 1417 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1418 | `			return SXERR_ABORT;` |
|        - | 1419 | `		}` |
|        - | 1420 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|       23 | 1421 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       11 | 1422 | `	}` |
|        - | 1423 | `	/* Compile the loop body */` |
|   325257 | 1424 | `	pGen->pIn = &pEnd[1];` |
|   325257 | 1425 | `	pGen->pEnd = pTmp;` |
|   325257 | 1426 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|   325257 | 1427 | `	if( rc == SXERR_ABORT ){` |
|        - | 1428 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1429 | `		return SXERR_ABORT;` |
|        - | 1430 | `	}` |
|        - | 1431 | `	/* Emit the unconditional jump to the start of the loop */` |
|   325257 | 1432 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|        - | 1433 | `	/* Fix all jumps now the destination is resolved */` |
|   325257 | 1434 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 1435 | `	/* Release the loop block */` |
|   325257 | 1436 | `	GenStateLeaveBlock(pGen,0);` |
|        - | 1437 | `	/* Statement successfully compiled */` |
|   325257 | 1438 | `	return SXRET_OK;` |
|        1 | 1439 | `Synchronize:` |
|        - | 1440 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - | 1441 | `	 * compiling this erroneous block.` |
|        - | 1442 | `	 */` |
|        3 | 1443 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 | 1444 | `		pGen->pIn++;` |
|      ! 0 | 1445 | `	}` |
|        3 | 1446 | `	return SXRET_OK;` |
|   162632 | 1447 | `}` |
|        - | 1448 | `/*` |
|        - | 1449 | ` * Compile the infamous if/elseif/else if/else statements.` |
|        - | 1450 | ` * According to the PHP language reference` |
|        - | 1451 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|        - | 1452 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|        - | 1453 | ` *  that is similar to that of C:` |
|        - | 1454 | ` *  if (expr)` |
|        - | 1455 | ` *   statement` |
|        - | 1456 | ` *  else construct:` |
|        - | 1457 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|        - | 1458 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|        - | 1459 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|        - | 1460 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|        - | 1461 | ` *   $b, and a is NOT greater than b otherwise.` |
|        - | 1462 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|        - | 1463 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|        - | 1464 | ` *  elseif` |
|        - | 1465 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|        - | 1466 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|        - | 1467 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|        - | 1468 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|        - | 1469 | ` *   than b, a equal to b or a is smaller than b:` |
|        - | 1470 | ` *   <?php` |
|        - | 1471 | ` *    if ($a > $b) {` |
|        - | 1472 | ` *     echo "a is bigger than b";` |
|        - | 1473 | ` *    } elseif ($a == $b) {` |
|        - | 1474 | ` *     echo "a is equal to b";` |
|        - | 1475 | ` *    } else {` |
|        - | 1476 | ` *     echo "a is smaller than b";` |
|        - | 1477 | ` *    }` |
|        - | 1478 | ` *    ?>` |
|        - | 1479 | ` */` |
|  2415268 | 1480 | `PH7_PRIVATE sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|        5 | 1481 | `{` |
|  2415273 | 1482 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  2415273 | 1483 | `	GenBlock *pCondBlock = 0;` |
|        - | 1484 | `	sxu32 nJumpIdx;` |
|        - | 1485 | `	sxu32 nKeyID;` |
|        - | 1486 | `	sxi32 rc;` |
|        - | 1487 | `	/* Jump the 'if' keyword */` |
|  2415273 | 1488 | `	pGen->pIn++;` |
|  2415273 | 1489 | `	pToken = pGen->pIn;` |
|        - | 1490 | `	/* Create the conditional block */` |
|  2415273 | 1491 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  2415273 | 1492 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1493 | `		return SXERR_ABORT;` |
|        - | 1494 | `	}` |
|        - | 1495 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|  1350600 | 1496 | `	for(;;){` |
|  2701205 | 1497 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1498 | `			/* Syntax error */` |
|      ! 0 | 1499 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 1500 | `				pToken--;` |
|      ! 0 | 1501 | `			}` |
|      ! 0 | 1502 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|      ! 0 | 1503 | `			if( rc == SXERR_ABORT ){` |
|        - | 1504 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 1505 | `				return SXERR_ABORT;` |
|        - | 1506 | `			}` |
|      ! 0 | 1507 | `			goto Synchronize;` |
|        - | 1508 | `		}` |
|        - | 1509 | `		/* Jump the left parenthesis '(' */` |
|  2701205 | 1510 | `		pToken++;` |
|        - | 1511 | `		/* Delimit the condition */` |
|  2701205 | 1512 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  2701205 | 1513 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|        - | 1514 | `			/* Syntax error */` |
|      ! 0 | 1515 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 1516 | `				pToken--;` |
|      ! 0 | 1517 | `			}` |
|      ! 0 | 1518 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|      ! 0 | 1519 | `			if( rc == SXERR_ABORT ){` |
|        - | 1520 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 1521 | `				return SXERR_ABORT;` |
|        - | 1522 | `			}` |
|      ! 0 | 1523 | `			goto Synchronize;` |
|        - | 1524 | `		}` |
|        - | 1525 | `		/* Swap token streams */` |
|  2701205 | 1526 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|        - | 1527 | `		/* Compile the condition */` |
|  2701205 | 1528 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 1529 | `		/* Update token stream */` |
|  2701205 | 1530 | `		while(pGen->pIn < pEnd ){` |
|      ! 0 | 1531 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 | 1532 | `			pGen->pIn++;` |
|      ! 0 | 1533 | `		}` |
|  2701205 | 1534 | `		pGen->pIn  = &pEnd[1];` |
|  2701205 | 1535 | `		pGen->pEnd = pTmp;` |
|  2701205 | 1536 | `		if( rc == SXERR_ABORT ){` |
|        - | 1537 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|        3 | 1538 | `			return SXERR_ABORT;` |
|        - | 1539 | `		}` |
|        - | 1540 | `		/* Emit the false jump */` |
|  2701203 | 1541 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|        - | 1542 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  2701203 | 1543 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|        - | 1544 | `		/* Compile the body */` |
|  2701203 | 1545 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  2701203 | 1546 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1547 | `			return SXERR_ABORT;` |
|        - | 1548 | `		}` |
|  2701203 | 1549 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   517920 | 1550 | `			break;` |
|        - | 1551 | `		}` |
|        - | 1552 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|  1665373 | 1553 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  1665373 | 1554 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|  1158889 | 1555 | `			break;` |
|        - | 1556 | `		}` |
|        - | 1557 | `		/* Emit the unconditional jump */` |
|   506489 | 1558 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|        - | 1559 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   506489 | 1560 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   506489 | 1561 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   305521 | 1562 | `			pToken = &pGen->pIn[1];` |
|   305521 | 1563 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    85002 | 1564 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|   110281 | 1565 | `					break;` |
|        - | 1566 | `			}` |
|    84969 | 1567 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    42482 | 1568 | `		}` |
|   285937 | 1569 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|        - | 1570 | `		/* Synchronize cursors */` |
|   285937 | 1571 | `		pToken = pGen->pIn;` |
|        - | 1572 | `		/* Fix the false jump */` |
|   285937 | 1573 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|        5 | 1574 | `	} /* For(;;) */` |
|        - | 1575 | `	/* Fix the false jump */` |
|  2415271 | 1576 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  2415271 | 1577 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|  1379436 | 1578 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|        - | 1579 | `			/* Compile the else block */` |
|   220557 | 1580 | `			pGen->pIn++;` |
|   220557 | 1581 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   220557 | 1582 | `			if( rc == SXERR_ABORT ){` |
|        - | 1583 |  |
|      ! 0 | 1584 | `				return SXERR_ABORT;` |
|        - | 1585 | `			}` |
|   110276 | 1586 | `	}` |
|  2415271 | 1587 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|        - | 1588 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  2415271 | 1589 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|        - | 1590 | `	/* Release the conditional block */` |
|  2415271 | 1591 | `	GenStateLeaveBlock(pGen,0);` |
|        - | 1592 | `	/* Statement successfully compiled */` |
|  2415271 | 1593 | `	return SXRET_OK;` |
|      ! 0 | 1594 | `Synchronize:` |
|        - | 1595 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|        - | 1596 | `	 */` |
|      ! 0 | 1597 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 | 1598 | `		pGen->pIn++;` |
|      ! 0 | 1599 | `	}` |
|      ! 0 | 1600 | `	return SXRET_OK;` |
|  1207639 | 1601 | `}` |
|        - | 1602 | `/*` |
|        - | 1603 | ` * Compile the global construct.` |
|        - | 1604 | ` * According to the PHP language reference` |
|        - | 1605 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|        - | 1606 | ` *  to be used in that function.` |
|        - | 1607 | ` *  Example #1 Using global` |
|        - | 1608 | ` *  <?php` |
|        - | 1609 | ` *   $a = 1;` |
|        - | 1610 | ` *   $b = 2;` |
|        - | 1611 | ` *   function Sum()` |
|        - | 1612 | ` *   {` |
|        - | 1613 | ` *    global $a, $b;` |
|        - | 1614 | ` *    $b = $a + $b;` |
|        - | 1615 | ` *   }` |
|        - | 1616 | ` *   Sum();` |
|        - | 1617 | ` *   echo $b;` |
|        - | 1618 | ` *  ?>` |
|        - | 1619 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|        - | 1620 | ` *  all references to either variable will refer to the global version. There is no limit` |
|        - | 1621 | ` *  to the number of global variables that can be manipulated by a function.` |
|        - | 1622 | ` */` |
|       36 | 1623 | `PH7_PRIVATE sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|        5 | 1624 | `{` |
|       41 | 1625 | `	SyToken *pTmp,*pNext = 0;` |
|        - | 1626 | `	sxi32 nExpr;` |
|        - | 1627 | `	sxi32 rc;` |
|        - | 1628 | `	/* Jump the 'global' keyword */` |
|       41 | 1629 | `	pGen->pIn++;` |
|       41 | 1630 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|        - | 1631 | `		/* Nothing to process */` |
|      ! 0 | 1632 | `		return SXRET_OK;` |
|        - | 1633 | `	}` |
|       41 | 1634 | `	pTmp = pGen->pEnd;` |
|       41 | 1635 | `	nExpr = 0;` |
|       87 | 1636 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       51 | 1637 | `		if( pGen->pIn < pNext ){` |
|       51 | 1638 | `			pGen->pEnd = pNext;` |
|       51 | 1639 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 | 1640 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|      ! 0 | 1641 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1642 | `					return SXERR_ABORT;` |
|        - | 1643 | `				}` |
|      ! 0 | 1644 | `			}else{` |
|       51 | 1645 | `				pGen->pIn++;` |
|       51 | 1646 | `				if( pGen->pIn >= pGen->pEnd ){` |
|        - | 1647 | `					/* Emit a warning */` |
|      ! 0 | 1648 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|      ! 0 | 1649 | `				}else{` |
|       51 | 1650 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       51 | 1651 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 1652 | `						return SXERR_ABORT;` |
|       51 | 1653 | `					}else if(rc != SXERR_EMPTY ){` |
|       51 | 1654 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|       51 | 1655 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|        - | 1656 | `							/* Variable name, not a constant */` |
|       51 | 1657 | `							pLast->iP1 = 0;` |
|       23 | 1658 | `						}` |
|       51 | 1659 | `						nExpr++;` |
|       23 | 1660 | `					}` |
|        - | 1661 | `				}` |
|        - | 1662 | `			}` |
|       23 | 1663 | `		}` |
|        - | 1664 | `		/* Next expression in the stream */` |
|       51 | 1665 | `		pGen->pIn = pNext;` |
|        - | 1666 | `		/* Jump trailing commas */` |
|       61 | 1667 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       15 | 1668 | `			pGen->pIn++;` |
|        5 | 1669 | `		}` |
|        5 | 1670 | `	}` |
|        - | 1671 | `	/* Restore token stream */` |
|       41 | 1672 | `	pGen->pEnd = pTmp;` |
|       41 | 1673 | `	if( nExpr > 0 ){` |
|        - | 1674 | `		/* Emit the uplink instruction */` |
|       41 | 1675 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|       18 | 1676 | `	}` |
|       41 | 1677 | `	return SXRET_OK;` |
|       23 | 1678 | `}` |
|        - | 1679 | `/*` |
|        - | 1680 | ` * Compile the return statement.` |
|        - | 1681 | ` * According to the PHP language reference` |
|        - | 1682 | ` *  If called from within a function, the return() statement immediately ends execution` |
|        - | 1683 | ` *  of the current function, and returns its argument as the value of the function call.` |
|        - | 1684 | ` *  return() will also end the execution of an eval() statement or script file.` |
|        - | 1685 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|        - | 1686 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|        - | 1687 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|        - | 1688 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|        - | 1689 | ` *  from within the main script file, then script execution end.` |
|        - | 1690 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|        - | 1691 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|        - | 1692 | ` *  should do so as PHP has less work to do in this case.` |
|        - | 1693 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|        - | 1694 | ` */` |
|  3601866 | 1695 | `PH7_PRIVATE sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|        5 | 1696 | `{` |
|  3601871 | 1697 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|        - | 1698 | `	sxi32 rc;` |
|  3601871 | 1699 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  3601871 | 1700 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|        - | 1701 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|        - | 1702 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|        - | 1703 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|        - | 1704 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|        - | 1705 | `	 * normally below so token processing stays consistent. */` |
|  9483139 | 1706 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|  5881273 | 1707 | `		pFuncBlock = pFuncBlock->pParent;` |
|        5 | 1708 | `	}` |
|  3601866 | 1709 | `	if( pFuncBlock && pFuncBlock->pUserData` |
|  3601855 | 1710 | `	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){` |
|        3 | 1711 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1712 | `			"A never-returning function must not return");` |
|        3 | 1713 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1714 | `			return SXERR_ABORT;` |
|        - | 1715 | `		}` |
|        1 | 1716 | `	}` |
|        - | 1717 | `	/* Jump the 'return' keyword */` |
|  3601871 | 1718 | `	pGen->pIn++;` |
|  3601871 | 1719 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - | 1720 | `		/* Compile the expression */` |
|  3505309 | 1721 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  3505309 | 1722 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1723 | `			return SXERR_ABORT;` |
|  3505309 | 1724 | `		}else if(rc != SXERR_EMPTY ){` |
|  3505309 | 1725 | `			nRet = 1;` |
|  1752652 | 1726 | `		}` |
|  1752652 | 1727 | `	}` |
|        - | 1728 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|        - | 1729 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|        - | 1730 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|        - | 1731 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|  3601871 | 1732 | `	if( pGen->bInGenerator ){` |
|     3895 | 1733 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|     3895 | 1734 | `		return SXRET_OK;` |
|        - | 1735 | `	}` |
|        - | 1736 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|        - | 1737 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|        - | 1738 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|        - | 1739 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|        - | 1740 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|  3597981 | 1741 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|  3597981 | 1742 | `	return SXRET_OK;` |
|  1800938 | 1743 | `}` |
|        - | 1744 | `/*` |
|        - | 1745 | ` * Compile a yield expression.` |
|        - | 1746 | ` * Called from the expression code generator when a yield node is encountered.` |
|        - | 1747 | ` * Handles: yield, yield $value, yield $key => $value` |
|        - | 1748 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|        - | 1749 | ` */` |
|    15836 | 1750 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|        5 | 1751 | `{` |
|        - | 1752 | `	SyToken *pTmp, *pSplit;` |
|    15841 | 1753 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|    15841 | 1754 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|        - | 1755 | `	sxi32 rc;` |
|     7918 | 1756 | `	(void)iCompileFlag;` |
|        - | 1757 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|    15841 | 1758 | `	pGen->pIn++;` |
|        - | 1759 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|        - | 1760 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|        - | 1761 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|        - | 1762 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|        - | 1763 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|    15836 | 1764 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|     7953 | 1765 | `		&& pGen->pIn->sData.nByte == 4` |
|       72 | 1766 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|       67 | 1767 | `		pGen->pIn++; /* Skip 'from' */` |
|       67 | 1768 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       67 | 1769 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1770 | `			return SXERR_ABORT;` |
|        - | 1771 | `		}` |
|       67 | 1772 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 | 1773 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|      ! 0 | 1774 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|        - | 1775 | `				"Missing expression after 'yield from'");` |
|      ! 0 | 1776 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1777 | `				return SXERR_ABORT;` |
|        - | 1778 | `			}` |
|      ! 0 | 1779 | `		}` |
|       67 | 1780 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|       67 | 1781 | `		return SXRET_OK;` |
|        - | 1782 | `	}` |
|    15779 | 1783 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - | 1784 | `		/* Bare yield — no value */` |
|        3 | 1785 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|        3 | 1786 | `		return SXRET_OK;` |
|        - | 1787 | `	}` |
|        - | 1788 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|    15777 | 1789 | `	pSplit = 0;` |
|        - | 1790 | `	{` |
|    15777 | 1791 | `		SyToken *pCur = pGen->pIn;` |
|    15777 | 1792 | `		sxi32 nNest = 0;` |
|    47135 | 1793 | `		while( pCur < pGen->pEnd ){` |
|    46825 | 1794 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       18 | 1795 | `				nNest++;` |
|    46817 | 1796 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       18 | 1797 | `				nNest--;` |
|    46801 | 1798 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|    15467 | 1799 | `				pSplit = pCur;` |
|    15467 | 1800 | `				break;` |
|        - | 1801 | `			}` |
|    31363 | 1802 | `			pCur++;` |
|        5 | 1803 | `		}` |
|        - | 1804 | `	}` |
|    15777 | 1805 | `	pTmp = pGen->pEnd;` |
|    15777 | 1806 | `	if( pSplit ){` |
|        - | 1807 | `		/* yield $key => $value */` |
|    15467 | 1808 | `		pGen->pEnd = pSplit;` |
|    15467 | 1809 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|    15467 | 1810 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|    15467 | 1811 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|    15467 | 1812 | `		pGen->pEnd = pTmp;` |
|    15467 | 1813 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|    15467 | 1814 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|    15467 | 1815 | `		iP1 = 1;` |
|    15467 | 1816 | `		iP2 = 1;` |
|     7736 | 1817 | `	}else{` |
|        - | 1818 | `		/* yield $value */` |
|      315 | 1819 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      315 | 1820 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      315 | 1821 | `		if( rc != SXERR_EMPTY ){` |
|      315 | 1822 | `			iP1 = 1;` |
|      155 | 1823 | `		}` |
|        - | 1824 | `	}` |
|    15777 | 1825 | `	pGen->pEnd = pTmp;` |
|    15777 | 1826 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|    15777 | 1827 | `	return SXRET_OK;` |
|     7923 | 1828 | `}` |
|        - | 1829 | `/*` |
|        - | 1830 | ` * Compile the die/exit language construct.` |
|        - | 1831 | ` * The role of these constructs is to terminate execution of the script.` |
|        - | 1832 | ` * Shutdown functions will always be executed even if exit() is called.` |
|        - | 1833 | ` */` |
|       94 | 1834 | `PH7_PRIVATE sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|        5 | 1835 | `{` |
|       99 | 1836 | `	sxi32 nExpr = 0;` |
|        - | 1837 | `	sxi32 rc;` |
|        - | 1838 | `	/* Jump the die/exit keyword */` |
|       99 | 1839 | `	pGen->pIn++;` |
|       99 | 1840 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - | 1841 | `		/* Compile the expression */` |
|       99 | 1842 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       99 | 1843 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1844 | `			return SXERR_ABORT;` |
|       99 | 1845 | `		}else if(rc != SXERR_EMPTY ){` |
|       99 | 1846 | `			nExpr = 1;` |
|       47 | 1847 | `		}` |
|       47 | 1848 | `	}` |
|        - | 1849 | `	/* Emit the HALT instruction */` |
|       99 | 1850 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|       99 | 1851 | `	return SXRET_OK;` |
|       52 | 1852 | `}` |
|        - | 1853 | `/*` |
|        - | 1854 | ` * Compile the 'echo' language construct.` |
|        - | 1855 | ` */` |
|    17426 | 1856 | `PH7_PRIVATE sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|        5 | 1857 | `{` |
|    17431 | 1858 | `	SyToken *pTmp,*pNext = 0;` |
|    17431 | 1859 | `	sxu32 nLine = pGen->pIn->nLine;` |
|    17431 | 1860 | `	int nExpr = 0;      /* expressions actually compiled */` |
|    17431 | 1861 | `	int bExpectMore = 1;/* after 'echo' or a comma an expression is REQUIRED */` |
|        - | 1862 | `	sxi32 rc;` |
|        - | 1863 | `	/* Jump the 'echo' keyword */` |
|    17431 | 1864 | `	pGen->pIn++;` |
|        - | 1865 | `	/* Compile arguments one after one */` |
|    17431 | 1866 | `	pTmp = pGen->pEnd;` |
|    45293 | 1867 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|    27873 | 1868 | `		if( pGen->pIn < pNext ){` |
|    27873 | 1869 | `			pGen->pEnd = pNext;` |
|    27873 | 1870 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|    27873 | 1871 | `			if( rc == SXERR_ABORT ){` |
|        6 | 1872 | `				return SXERR_ABORT;` |
|    27869 | 1873 | `			}else if( rc != SXERR_EMPTY ){` |
|        - | 1874 | `				/* Emit the consume instruction */` |
|    27845 | 1875 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    27845 | 1876 | `				nExpr++;` |
|    27845 | 1877 | `				bExpectMore = 0;` |
|    13920 | 1878 | `			}` |
|    13932 | 1879 | `		}` |
|        - | 1880 | `		/* Jump trailing commas (php: exactly one between expressions; a` |
|        - | 1881 | `		 * dangling or doubled comma is a parse error, enforced below) */` |
|    38317 | 1882 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|    10455 | 1883 | `			if( bExpectMore ){` |
|        - | 1884 | `				/* two commas in a row */` |
|        3 | 1885 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,pNext->nLine,` |
|        - | 1886 | `					"syntax error, unexpected token \",\"");` |
|        3 | 1887 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1888 | `			}` |
|    10453 | 1889 | `			bExpectMore = 1;` |
|    10453 | 1890 | `			pNext++;` |
|        5 | 1891 | `		}` |
|    27867 | 1892 | `		pGen->pIn = pNext;` |
|        5 | 1893 | `	}` |
|        - | 1894 | `	/* Restore token stream */` |
|    17425 | 1895 | `	pGen->pEnd = pTmp;` |
|    17425 | 1896 | `	if( nExpr == 0 \|\| bExpectMore ){` |
|        - | 1897 | ``		/* `echo ;` or `echo expr, ;` — php rejects both */`` |
|       32 | 1898 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - | 1899 | `			"syntax error, unexpected token \";\"");` |
|       32 | 1900 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1901 | `	}` |
|    17397 | 1902 | `	return SXRET_OK;` |
|     8718 | 1903 | `}` |
|        - | 1904 | `/*` |
|        - | 1905 | ` * Compile the static statement.` |
|        - | 1906 | ` * According to the PHP language reference` |
|        - | 1907 | ` *  Another important feature of variable scoping is the static variable.` |
|        - | 1908 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|        - | 1909 | ` *  when program execution leaves this scope.` |
|        - | 1910 | ` *  Static variables also provide one way to deal with recursive functions.` |
|        - | 1911 | ` * Symisc eXtension.` |
|        - | 1912 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|        - | 1913 | ` *  the zend engine would allow only simple scalar value.` |
|        - | 1914 | ` *  Example` |
|        - | 1915 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|        - | 1916 | ` *    Refer to the official documentation for more information on this feature.` |
|        - | 1917 | ` */` |
|    11598 | 1918 | `PH7_PRIVATE sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|        5 | 1919 | `{` |
|        - | 1920 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|        - | 1921 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|        - | 1922 | `	GenBlock *pBlock;` |
|        - | 1923 | `	SyString *pName;` |
|        - | 1924 | `	char *zDup;` |
|        - | 1925 | `	sxu32 nLine;` |
|        - | 1926 | `	sxi32 rc;` |
|        - | 1927 | ``	/* `static function () {}` / `static fn () =>` at statement position is an`` |
|        - | 1928 | `	 * EXPRESSION statement (a bare static closure), not a static-variable` |
|        - | 1929 | `	 * declaration — hand it to the expression compiler (php accepts it). */` |
|    11598 | 1930 | `	if( &pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & PH7_TK_KEYWORD)` |
|     5805 | 1931 | `	 && (SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FUNCTION` |
|        1 | 1932 | `	  \|\| SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FN) ){` |
|        3 | 1933 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        3 | 1934 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1935 | `			return SXERR_ABORT;` |
|        3 | 1936 | `		}else if( rc != SXERR_EMPTY ){` |
|        3 | 1937 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        1 | 1938 | `		}` |
|        3 | 1939 | `		return SXRET_OK;` |
|        - | 1940 | `	}` |
|        - | 1941 | `	/* Jump the static keyword */` |
|    11601 | 1942 | `	nLine = pGen->pIn->nLine;` |
|    11601 | 1943 | `	pGen->pIn++;` |
|        - | 1944 | `	/* Extract the enclosing function if any */` |
|    11601 | 1945 | `	pBlock = pGen->pCurrent;` |
|    23197 | 1946 | `	while( pBlock ){` |
|    23197 | 1947 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|    11601 | 1948 | `			break;` |
|        - | 1949 | `		}` |
|        - | 1950 | `		/* Point to the upper block */` |
|    11601 | 1951 | `		pBlock = pBlock->pParent;` |
|        5 | 1952 | `	}` |
|    11601 | 1953 | `	if( pBlock == 0 ){` |
|        - | 1954 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|      ! 0 | 1955 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|        - | 1956 | ``			/* php: `static FOO;` is a syntax error naming FOO and expecting "::"`` |
|        - | 1957 | ``			 * (the parser is still open to `static::` at that point). */`` |
|      ! 0 | 1958 | `			rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"::\"");` |
|      ! 0 | 1959 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1960 | `				return SXERR_ABORT;` |
|        - | 1961 | `			}` |
|      ! 0 | 1962 | `			goto Synchronize;` |
|        - | 1963 | `		}` |
|        - | 1964 | `		/* Compile the expression holding the variable */` |
|      ! 0 | 1965 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      ! 0 | 1966 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1967 | `			return SXERR_ABORT;` |
|      ! 0 | 1968 | `		}else if( rc != SXERR_EMPTY ){` |
|        - | 1969 | `			/* Emit the POP instruction */` |
|      ! 0 | 1970 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      ! 0 | 1971 | `		}` |
|      ! 0 | 1972 | `		return SXRET_OK;` |
|        - | 1973 | `	}` |
|    11601 | 1974 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        - | 1975 | `	/* Make sure we are dealing with a valid statement */` |
|    11601 | 1976 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|    11594 | 1977 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - | 1978 | ``			/* php: `static FOO;` is a syntax error naming FOO and expecting "::"`` |
|        - | 1979 | ``			 * (the parser is still open to `static::` at that point). */`` |
|        3 | 1980 | `			rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"::\"");` |
|        3 | 1981 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1982 | `				return SXERR_ABORT;` |
|        - | 1983 | `			}` |
|        3 | 1984 | `			goto Synchronize;` |
|        - | 1985 | `	}` |
|    11599 | 1986 | `	pGen->pIn++;` |
|        - | 1987 | `	/* Extract variable name */` |
|    11599 | 1988 | `	pName = &pGen->pIn->sData;` |
|    11599 | 1989 | `	pGen->pIn++; /* Jump the var name */` |
|    11599 | 1990 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|      ! 0 | 1991 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 1992 | `		goto Synchronize;` |
|        - | 1993 | `	}` |
|        - | 1994 | `	/* Initialize the structure describing the static variable */` |
|    11599 | 1995 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|    11599 | 1996 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|        - | 1997 | `	/* Duplicate variable name */` |
|    11599 | 1998 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    11599 | 1999 | `	if( zDup == 0 ){` |
|      ! 0 | 2000 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 2001 | `		return SXERR_ABORT;` |
|        - | 2002 | `	}` |
|    11599 | 2003 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|        - | 2004 | `	/* Check if we have an expression to compile */` |
|    11599 | 2005 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|        - | 2006 | `		SySet *pInstrContainer;` |
|        - | 2007 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|        - | 2008 | `		 * Static variable can take any complex expression including function` |
|        - | 2009 | `		 * call as their initialization value.` |
|        - | 2010 | `		 * Example:` |
|        - | 2011 | `		 *		static $var = foo(1,4+5,bar());` |
|        - | 2012 | `		 */` |
|    11599 | 2013 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|        - | 2014 | `		/* Swap bytecode container */` |
|    11599 | 2015 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    11599 | 2016 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|        - | 2017 | `		/* Compile the expression */` |
|    11599 | 2018 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 2019 | `		/* Emit the done instruction */` |
|    11599 | 2020 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        - | 2021 | `		/* Restore default bytecode container */` |
|    11599 | 2022 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     5797 | 2023 | `	}` |
|        - | 2024 | `	/* Finally save the compiled static variable in the appropriate container */` |
|    11599 | 2025 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|    11599 | 2026 | `	return SXRET_OK;` |
|        1 | 2027 | `Synchronize:` |
|        - | 2028 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|        - | 2029 | `	 * statement.` |
|        - | 2030 | `	 */` |
|        5 | 2031 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|        3 | 2032 | `		pGen->pIn++;` |
|        1 | 2033 | `	}` |
|        3 | 2034 | `	return SXRET_OK;` |
|     5804 | 2035 | `}` |
|        - | 2036 | `/*` |
|        - | 2037 | ` * Compile the var statement.` |
|        - | 2038 | ` * Symisc Extension:` |
|        - | 2039 | ` *      var statement can be used outside of a class definition.` |
|        - | 2040 | ` */` |
|        2 | 2041 | `PH7_PRIVATE sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|        1 | 2042 | `{` |
|        - | 2043 | `` 	/* `var` is a PROPERTY modifier and nothing else. A class body's `var $x;` `` |
|        - | 2044 | `	 * is compiled by compile_class.c, never here, so reaching this statement` |
|        - | 2045 | ``	 * handler means `var` appeared outside a class — which php rejects as a`` |
|        - | 2046 | `	 * parse error. PHL used to compile it as an ordinary expression statement,` |
|        - | 2047 | ``	 * so top-level `var $x = 1;` silently ran (a Symisc extension). */`` |
|        3 | 2048 | `	PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|        3 | 2049 | `	return SXERR_ABORT;` |
|        1 | 2050 | `}` |
|        - | 2051 | `/*` |
|        - | 2052 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|        - | 2053 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|        - | 2054 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|        - | 2055 | ` */` |
|        - | 2056 | `/*` |
|        - | 2057 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|        - | 2058 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|        - | 2059 | ` * hash and any shared references), this creates a new literal entry with the` |
|        - | 2060 | ` * qualified name and updates the instruction's operand index.` |
|        - | 2061 | ` *` |
|        - | 2062 | ` * Resolution order:` |
|        - | 2063 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|        - | 2064 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|        - | 2065 | ` *   3. Otherwise return the original literal index unchanged.` |
|        - | 2066 | ` *` |
|        - | 2067 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|        - | 2068 | ` * came from an import (step 1) and 0 otherwise.` |
|        - | 2069 | ` * Returns the (possibly new) literal index.` |
|        - | 2070 | ` */` |
|  6402344 | 2071 | `PH7_PRIVATE sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|        5 | 2072 | `{` |
|        - | 2073 | `	ph7_value *pLit;` |
|        - | 2074 | `	const char *zLit;` |
|        - | 2075 | `	SyString sQualified;` |
|        - | 2076 | `	sxu32 nLit;` |
|        - | 2077 | `	sxu32 k;` |
|        - | 2078 | `	sxu32 nNewIdx;` |
|        - | 2079 | `	int hasNsSep;` |
|        - | 2080 | `	SyHashEntry *pImport;` |
|        - | 2081 | `	ph7_value *pNew;` |
|  6402349 | 2082 | `	if( pFromImport ){` |
|  5220183 | 2083 | `		*pFromImport = 0;` |
|  2610089 | 2084 | `	}` |
|  6402349 | 2085 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  6402349 | 2086 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|      ! 0 | 2087 | `		return nOrigIdx;` |
|        - | 2088 | `	}` |
|  6402349 | 2089 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  6402349 | 2090 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|        - | 2091 | `	/* Skip if already qualified (contains backslash) */` |
|  6402349 | 2092 | `	hasNsSep = 0;` |
| 77415637 | 2093 | `	for( k = 0; k < nLit; k++ ){` |
| 71013319 | 2094 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 35506649 | 2095 | `	}` |
|  6402349 | 2096 | `	if( hasNsSep ){` |
|       28 | 2097 | `		return nOrigIdx;` |
|        - | 2098 | `	}` |
|        - | 2099 | `	/* Check use imports first (works even outside namespaces) */` |
|  6402323 | 2100 | `	SyBlobReset(&pGen->sWorker);` |
|  6402323 | 2101 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  6402323 | 2102 | `	if( pImport ){` |
|       41 | 2103 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       41 | 2104 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|       41 | 2105 | `		if( pFromImport ){` |
|       18 | 2106 | `			*pFromImport = 1;` |
|        8 | 2107 | `		}` |
|       23 | 2108 | `	}else{` |
|  6402287 | 2109 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  6402149 | 2110 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|        - | 2111 | `		}` |
|        - | 2112 | `		/* Prepend current namespace */` |
|      143 | 2113 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      143 | 2114 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      143 | 2115 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|        - | 2116 | `	}` |
|        - | 2117 | `	/* Look up or create a new literal for the qualified name */` |
|      179 | 2118 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|      179 | 2119 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|       79 | 2120 | `		return nNewIdx; /* Already interned */` |
|        - | 2121 | `	}` |
|      105 | 2122 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      105 | 2123 | `	if( pNew == 0 ){` |
|      ! 0 | 2124 | `		return nOrigIdx; /* OOM, fall back to original */` |
|        - | 2125 | `	}` |
|      105 | 2126 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      105 | 2127 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      105 | 2128 | `	return nNewIdx;` |
|  3201177 | 2129 | `}` |
|        - | 2130 | `/*` |
|        - | 2131 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|        - | 2132 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|        - | 2133 | ` */` |
|   550344 | 2134 | `PH7_PRIVATE void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|        5 | 2135 | `{` |
|        - | 2136 | `	SyHashEntry *pImport;` |
|   550349 | 2137 | `	const char *zName = pName->zString;` |
|   550349 | 2138 | `	sxu32 nName = pName->nByte;` |
|   550349 | 2139 | `	sxu32 nFirst = 0;` |
|        - | 2140 | `	/* php resolves a name through use-imports on its LEADING segment: an` |
|        - | 2141 | ``	 * unqualified `C` maps the whole name (`use X\C;` -> X\C), while a QUALIFIED`` |
|        - | 2142 | ``	 * `A\B\C` maps just `A` (`use X\A;` -> X\A\B\C) and keeps the `\B\C` tail.`` |
|        - | 2143 | `	 * The old code looked up the whole qualified string (which never matches a` |
|        - | 2144 | `	 * single-segment import alias) and then blindly prefixed the current` |
|        - | 2145 | ``	 * namespace, so `use PHPUnit\Framework; extends Framework\TestCase` resolved`` |
|        - | 2146 | `	 * to Ns\Framework\TestCase. Mirror GenStateResolveNamespaceLiteral here. */` |
|  6998375 | 2147 | `	while( nFirst < nName && zName[nFirst] != '\\' ){ nFirst++; }` |
|   550349 | 2148 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)zName,nFirst);` |
|   550349 | 2149 | `	if( pImport ){` |
|       26 | 2150 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       26 | 2151 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|       26 | 2152 | `		SyBlobAppend(pOut,zName + nFirst,nName - nFirst); /* the \B\C tail, if any */` |
|       26 | 2153 | `		return;` |
|        - | 2154 | `	}` |
|        - | 2155 | `	/* Prepend current namespace if active */` |
|   550327 | 2156 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       14 | 2157 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       14 | 2158 | `		SyBlobAppend(pOut,"\\",1);` |
|        6 | 2159 | `	}` |
|   550327 | 2160 | `	SyBlobAppend(pOut,zName,nName);` |
|   275177 | 2161 | `}` |
|        - | 2162 | `/*` |
|        - | 2163 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|        - | 2164 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|        - | 2165 | ` * The caller must release pOut when done.` |
|        - | 2166 | ` */` |
|   527470 | 2167 | `PH7_PRIVATE void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|        5 | 2168 | `{` |
|   527475 | 2169 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     3955 | 2170 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     3955 | 2171 | `		SyBlobAppend(pOut,"\\",1);` |
|     1975 | 2172 | `	}` |
|   527475 | 2173 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   527475 | 2174 | `}` |
|        - | 2175 | `/*` |
|        - | 2176 | ` * Compile a namespace statement` |
|        - | 2177 | ` * According to the PHP language reference manual` |
|        - | 2178 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|        - | 2179 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|        - | 2180 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|        - | 2181 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|        - | 2182 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|        - | 2183 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|        - | 2184 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|        - | 2185 | ` *  programming world.` |
|        - | 2186 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|        - | 2187 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|        - | 2188 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|        - | 2189 | ` *  classes/functions/constants.` |
|        - | 2190 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|        - | 2191 | ` *  readability of source code.` |
|        - | 2192 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|        - | 2193 | ` *  Here is an example of namespace syntax in PHP:` |
|        - | 2194 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|        - | 2195 | ` *       class MyClass {}` |
|        - | 2196 | ` *       function myfunction() {}` |
|        - | 2197 | ` *       const MYCONST = 1;` |
|        - | 2198 | ` *       $a = new MyClass;` |
|        - | 2199 | ` *       $c = new \my\name\MyClass;` |
|        - | 2200 | ` *       $a = strlen('hi');` |
|        - | 2201 | ` *       $d = namespace\MYCONST;` |
|        - | 2202 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|        - | 2203 | ` *       echo constant($d);` |
|        - | 2204 | ` * NOTE` |
|        - | 2205 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|        - | 2206 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|        - | 2207 | ` */` |
|        - | 2208 | `/*` |
|        - | 2209 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|        - | 2210 | ` */` |
|       14 | 2211 | `PH7_PRIVATE const char * TokenTypeName(sxu32 nType)` |
|        4 | 2212 | `{` |
|       18 | 2213 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|       11 | 2214 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|       11 | 2215 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|       11 | 2216 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|       11 | 2217 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|       11 | 2218 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|        3 | 2219 | `	return "token";` |
|       11 | 2220 | `}` |
|     3994 | 2221 | `PH7_PRIVATE sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|        5 | 2222 | `{` |
|        - | 2223 | `	sxu32 nLine;` |
|        - | 2224 | `	sxi32 rc;` |
|     3999 | 2225 | `	nLine = pGen->pIn->nLine;` |
|     3999 | 2226 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|        - | 2227 | `	/* Reset namespace and clear previous use imports */` |
|     3999 | 2228 | `	SyBlobReset(&pGen->sNamespace);` |
|     3999 | 2229 | `	SyHashRelease(&pGen->hUseImports);` |
|     3999 | 2230 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|     3999 | 2231 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     3999 | 2232 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|     3999 | 2233 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     3999 | 2234 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|     3999 | 2235 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - | 2236 | `		/* Global namespace (bare "namespace;") */` |
|      ! 0 | 2237 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|      ! 0 | 2238 | `		return SXRET_OK;` |
|        - | 2239 | `	}` |
|     3999 | 2240 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|        - | 2241 | `		/* namespace; — switch to global namespace */` |
|      ! 0 | 2242 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|      ! 0 | 2243 | `		return SXRET_OK;` |
|        - | 2244 | `	}` |
|     3999 | 2245 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|        - | 2246 | `		/* namespace { } — global namespace block */` |
|        5 | 2247 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|        5 | 2248 | `		return SXRET_OK;` |
|        - | 2249 | `	}` |
|        - | 2250 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     8067 | 2251 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     4077 | 2252 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|        - | 2253 | `			/* Append backslash separator */` |
|       46 | 2254 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       46 | 2255 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|       21 | 2256 | `			}` |
|       25 | 2257 | `		}else{` |
|        - | 2258 | `			/* Append identifier */` |
|     4035 | 2259 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        - | 2260 | `		}` |
|     4077 | 2261 | `		pGen->pIn++;` |
|        5 | 2262 | `	}` |
|        - | 2263 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|        - | 2264 | `	 * at the correct program counter, not just the last one compiled. */` |
|        - | 2265 | `	{` |
|     3995 | 2266 | `		char *zNsDup = 0;` |
|     3995 | 2267 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     5987 | 2268 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     3988 | 2269 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     1994 | 2270 | `		}` |
|     3995 | 2271 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|        - | 2272 | `	}` |
|     3995 | 2273 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        8 | 2274 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - | 2275 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|        4 | 2276 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        6 | 2277 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2278 | `			return SXERR_ABORT;` |
|        - | 2279 | `		}` |
|        2 | 2280 | `	}` |
|     3995 | 2281 | `	return SXRET_OK;` |
|     2002 | 2282 | `}` |
|        - | 2283 | `/*` |
|        - | 2284 | ` * Compile the 'use' statement` |
|        - | 2285 | ` * According to the PHP language reference manual` |
|        - | 2286 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|        - | 2287 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|        - | 2288 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|        - | 2289 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|        - | 2290 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|        - | 2291 | ` *  a function or constant is not supported.` |
|        - | 2292 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|        - | 2293 | ` * NOTE` |
|        - | 2294 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|        - | 2295 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|        - | 2296 | ` */` |
|       80 | 2297 | `PH7_PRIVATE sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|        5 | 2298 | `{` |
|        - | 2299 | `	sxu32 nLine;` |
|        - | 2300 | `	sxi32 rc;` |
|        - | 2301 | `	SyBlob sPath;` |
|        - | 2302 | `	SyString sAlias;` |
|        - | 2303 | `	SyToken *pLast;` |
|        - | 2304 | `	char *zDup;` |
|        - | 2305 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|        - | 2306 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|        - | 2307 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|       85 | 2308 | `	nLine = pGen->pIn->nLine;` |
|       85 | 2309 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|        - | 2310 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|       85 | 2311 | `	iUseType = 0;` |
|       85 | 2312 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       30 | 2313 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|       30 | 2314 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|       16 | 2315 | `			iUseType = 1;` |
|       16 | 2316 | `			pGen->pIn++;` |
|       23 | 2317 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|       16 | 2318 | `			iUseType = 2;` |
|       16 | 2319 | `			pGen->pIn++;` |
|        7 | 2320 | `		}` |
|       14 | 2321 | `	}` |
|        - | 2322 | `	/* Select target hash tables based on import type */` |
|       85 | 2323 | `	switch( iUseType ){` |
|        7 | 2324 | `		case 1:` |
|       16 | 2325 | `			pGenHash = &pGen->hUseFuncImports;` |
|       16 | 2326 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|       16 | 2327 | `			break;` |
|        7 | 2328 | `		case 2:` |
|       16 | 2329 | `			pGenHash = &pGen->hUseConstImports;` |
|       16 | 2330 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|       16 | 2331 | `			break;` |
|       26 | 2332 | `		default:` |
|       57 | 2333 | `			pGenHash = &pGen->hUseImports;` |
|       57 | 2334 | `			pVmHash = &pGen->pVm->hUseImports;` |
|       52 | 2335 | `			break;` |
|        - | 2336 | `	}` |
|       85 | 2337 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|        - | 2338 | `	/* Process one or more use declarations separated by commas */` |
|       41 | 2339 | `	for(;;){` |
|       87 | 2340 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 2341 | `			break;` |
|        - | 2342 | `		}` |
|       87 | 2343 | `		SyBlobReset(&sPath);` |
|       87 | 2344 | `		pLast = 0;` |
|        - | 2345 | `		/* Collect the full namespace path */` |
|      301 | 2346 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|      219 | 2347 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|      151 | 2348 | `				pLast = pGen->pIn;` |
|      151 | 2349 | `				if( SyBlobLength(&sPath) > 0 ){` |
|       73 | 2350 | `					SyBlobAppend(&sPath,"\\",1);` |
|       34 | 2351 | `				}` |
|      151 | 2352 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       73 | 2353 | `			}` |
|      219 | 2354 | `			pGen->pIn++;` |
|        5 | 2355 | `		}` |
|       87 | 2356 | `		if( pLast == 0 ){` |
|        - | 2357 | `			/* Empty path */` |
|        6 | 2358 | `			break;` |
|        - | 2359 | `		}` |
|        - | 2360 | `		/* Default alias is the last component of the path */` |
|       83 | 2361 | `		sAlias = pLast->sData;` |
|        - | 2362 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|       78 | 2363 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       56 | 2364 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|       27 | 2365 | `			pGen->pIn++; /* Jump 'as' */` |
|       27 | 2366 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|       27 | 2367 | `				sAlias = pGen->pIn->sData;` |
|       27 | 2368 | `				pGen->pIn++;` |
|       12 | 2369 | `			}` |
|       12 | 2370 | `		}` |
|        - | 2371 | `		/* Check for duplicate import alias (per-type) */` |
|       83 | 2372 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|        8 | 2373 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 2374 | `				"Cannot use %.*s as %z because the name is already in use",` |
|        4 | 2375 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|        6 | 2376 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2377 | `				SyBlobRelease(&sPath);` |
|      ! 0 | 2378 | `				return SXERR_ABORT;` |
|        - | 2379 | `			}` |
|        2 | 2380 | `		}` |
|        - | 2381 | `		/* Register the import: alias -> FQN.` |
|        - | 2382 | `		 * Strings are allocated from the VM pool allocator and freed` |
|        - | 2383 | `		 * when the entire VM is released. SyHashRelease does not free` |
|        - | 2384 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|      122 | 2385 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       78 | 2386 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|       83 | 2387 | `		if( zDup ){` |
|       83 | 2388 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|       83 | 2389 | `			if( pVmHash ){` |
|        - | 2390 | `				/* Class imports: populate VM table directly (class resolution` |
|        - | 2391 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|       55 | 2392 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       55 | 2393 | `				if( zAliasDup ){` |
|       55 | 2394 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|       25 | 2395 | `				}` |
|       25 | 2396 | `			}` |
|       83 | 2397 | `			if( iUseType == 2 ){` |
|        - | 2398 | `				/* Const imports: emit a runtime instruction so imports are` |
|        - | 2399 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|       16 | 2400 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       16 | 2401 | `				if( zAliasDup ){` |
|        - | 2402 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|        - | 2403 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|        - | 2404 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|       16 | 2405 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|       16 | 2406 | `					if( azPair ){` |
|       16 | 2407 | `						azPair[0] = zAliasDup;` |
|       16 | 2408 | `						azPair[1] = zDup;` |
|       16 | 2409 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|        7 | 2410 | `					}` |
|        7 | 2411 | `				}` |
|        7 | 2412 | `			}` |
|       39 | 2413 | `		}` |
|        - | 2414 | `		/* Check for comma (multiple use declarations) */` |
|       83 | 2415 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|        3 | 2416 | `			pGen->pIn++;` |
|        2 | 2417 | `		}else{` |
|       43 | 2418 | `			break;` |
|        - | 2419 | `		}` |
|        1 | 2420 | `	}` |
|       85 | 2421 | `	SyBlobRelease(&sPath);` |
|       85 | 2422 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        4 | 2423 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|        2 | 2424 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        3 | 2425 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2426 | `			return SXERR_ABORT;` |
|        - | 2427 | `		}` |
|        1 | 2428 | `	}` |
|       85 | 2429 | `	return SXRET_OK;` |
|       45 | 2430 | `}` |
|        - | 2431 | `/*` |
|        - | 2432 | ` * Compile the stupid 'declare' language construct.` |
|        - | 2433 | ` *` |
|        - | 2434 | ` * According to the PHP language reference manual.` |
|        - | 2435 | ` *  The declare construct is used to set execution directives for a block of code.` |
|        - | 2436 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|        - | 2437 | ` *  declare (directive)` |
|        - | 2438 | ` *   statement` |
|        - | 2439 | ` * The directive section allows the behavior of the declare block to be set.` |
|        - | 2440 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|        - | 2441 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|        - | 2442 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|        - | 2443 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|        - | 2444 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|        - | 2445 | ` * <?php` |
|        - | 2446 | ` * // these are the same:` |
|        - | 2447 | ` * // you can use this:` |
|        - | 2448 | ` * declare(ticks=1) {` |
|        - | 2449 | ` *   // entire script here` |
|        - | 2450 | ` * }` |
|        - | 2451 | ` * // or you can use this:` |
|        - | 2452 | ` * declare(ticks=1);` |
|        - | 2453 | ` * // entire script here` |
|        - | 2454 | ` * ?>` |
|        - | 2455 | ` *` |
|        - | 2456 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|        - | 2457 | ` */` |
|        - | 2458 | `/*` |
|        - | 2459 | ` * Match a directive name against a known literal (case-insensitive).` |
|        - | 2460 | ` */` |
|       80 | 2461 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|        5 | 2462 | `{` |
|      120 | 2463 | `	return SyStringLength(pName) == nWant` |
|       80 | 2464 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|        5 | 2465 | `}` |
|        - | 2466 |  |
|       44 | 2467 | `PH7_PRIVATE sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|        5 | 2468 | `{` |
|       49 | 2469 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       49 | 2470 | `	SyToken *pBodyEnd = 0;` |
|        - | 2471 | `	SyToken *pBodyStart;` |
|        - | 2472 | `	SyToken *pCursor;` |
|        - | 2473 | `	int bHasStrictTypes;` |
|        - | 2474 | `	int bBlockForm;` |
|        - | 2475 | `	int bPlacementOk;` |
|        - | 2476 | `	sxi32 rc;` |
|       49 | 2477 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|       49 | 2478 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|        6 | 2479 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|        6 | 2480 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2481 | `			return SXERR_ABORT;` |
|        - | 2482 | `		}` |
|        6 | 2483 | `		goto Synchro;` |
|        - | 2484 | `	}` |
|       45 | 2485 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|       45 | 2486 | `	pBodyStart = pGen->pIn;` |
|        - | 2487 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|       45 | 2488 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|       45 | 2489 | `	if( pBodyEnd >= pGen->pEnd ){` |
|      ! 0 | 2490 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\")\"");` |
|      ! 0 | 2491 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2492 | `			return SXERR_ABORT;` |
|        - | 2493 | `		}` |
|      ! 0 | 2494 | `		return SXRET_OK;` |
|        - | 2495 | `	}` |
|        - | 2496 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|        - | 2497 | `	 * now delimits the comma-separated directive list. */` |
|       45 | 2498 | `	pGen->pIn = &pBodyEnd[1];` |
|       45 | 2499 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      ! 0 | 2500 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|      ! 0 | 2501 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2502 | `			return SXERR_ABORT;` |
|        - | 2503 | `		}` |
|      ! 0 | 2504 | `	}` |
|       45 | 2505 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|       45 | 2506 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|       45 | 2507 | `	bHasStrictTypes = 0;` |
|        - | 2508 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|        - | 2509 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|        - | 2510 | `	 * directive appears anywhere in the list, before validating values. */` |
|       45 | 2511 | `	pCursor = pBodyStart;` |
|       57 | 2512 | `	while( pCursor < pBodyEnd ){` |
|       53 | 2513 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|       45 | 2514 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|       41 | 2515 | `				bHasStrictTypes = 1;` |
|       41 | 2516 | `				break;` |
|        - | 2517 | `			}` |
|        2 | 2518 | `		}` |
|       14 | 2519 | `		pCursor++;` |
|        2 | 2520 | `	}` |
|       45 | 2521 | `	if( bHasStrictTypes && bBlockForm ){` |
|        3 | 2522 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2523 | `			"strict_types declaration must not use block mode");` |
|        3 | 2524 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        3 | 2525 | `		return SXRET_OK;` |
|        - | 2526 | `	}` |
|       43 | 2527 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|        6 | 2528 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2529 | `			"strict_types declaration must be the very first statement in the script");` |
|        6 | 2530 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        6 | 2531 | `		return SXRET_OK;` |
|        - | 2532 | `	}` |
|        - | 2533 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|       39 | 2534 | `	pCursor = pBodyStart;` |
|       73 | 2535 | `	while( pCursor < pBodyEnd ){` |
|        - | 2536 | `		SyToken *pNameTok;` |
|        - | 2537 | `		SyToken *pEqTok;` |
|        - | 2538 | `		SyToken *pValTok;` |
|        - | 2539 | `		SyString *pDirName;` |
|        - | 2540 | `		int bIsStrict;` |
|        - | 2541 | `		int iStrictValue;` |
|       41 | 2542 | `		pNameTok = pCursor;` |
|       41 | 2543 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 2544 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2545 | `				"declare: Expecting a directive name");` |
|      ! 0 | 2546 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2547 | `			return SXRET_OK;` |
|        - | 2548 | `		}` |
|       41 | 2549 | `		pEqTok = pNameTok + 1;` |
|       41 | 2550 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|      ! 0 | 2551 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2552 | `				"declare: Expecting '=' after directive name");` |
|      ! 0 | 2553 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2554 | `			return SXRET_OK;` |
|        - | 2555 | `		}` |
|       41 | 2556 | `		pValTok = pEqTok + 1;` |
|       41 | 2557 | `		if( pValTok >= pBodyEnd ){` |
|      ! 0 | 2558 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2559 | `				"declare: Expecting value after '='");` |
|      ! 0 | 2560 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2561 | `			return SXRET_OK;` |
|        - | 2562 | `		}` |
|       41 | 2563 | `		pDirName = &pNameTok->sData;` |
|       41 | 2564 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|       41 | 2565 | `		if( bIsStrict ){` |
|        - | 2566 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|        - | 2567 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|       37 | 2568 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      ! 0 | 2569 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2570 | `					"declare(strict_types) value must be a literal");` |
|      ! 0 | 2571 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2572 | `				return SXRET_OK;` |
|        - | 2573 | `			}` |
|       37 | 2574 | `			iStrictValue = -1;` |
|       37 | 2575 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|       37 | 2576 | `				const char *zv = SyStringData(&pValTok->sData);` |
|       37 | 2577 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|       37 | 2578 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|       35 | 2579 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|       16 | 2580 | `			}` |
|       37 | 2581 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|        3 | 2582 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2583 | `					"strict_types declaration must have 0 or 1 as its value");` |
|        3 | 2584 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        3 | 2585 | `				return SXRET_OK;` |
|        - | 2586 | `			}` |
|       35 | 2587 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|       21 | 2588 | `		}else if( DeclareNameIs(pDirName, "encoding", sizeof("encoding")-1) ){` |
|        - | 2589 | `			/* php always ignores declare(encoding=...) unless it was built with` |
|        - | 2590 | `			 * Zend multibyte, and says so in these exact words. */` |
|        3 | 2591 | `			PH7_GenCompileError(&(*pGen),E_WARNING,nLine,` |
|        - | 2592 | `				"declare(encoding=...) ignored because Zend multibyte feature is turned off by settings");` |
|        1 | 2593 | `		}else{` |
|        - | 2594 | `			/* Other directives (ticks and friends) are accepted as no-ops.` |
|        - | 2595 | `			 * This used to emit a NOTICE naming the upstream engine and its` |
|        - | 2596 | `			 * version ("the declare construct is a no-op in the current release` |
|        - | 2597 | `			 * of the PH7(2.1.4) engine") — php prints nothing at all for` |
|        - | 2598 | ``			 * `declare(ticks=1)`, and leaking the old engine's branding into`` |
|        - | 2599 | `			 * user-visible diagnostics was wrong on its own. */` |
|        - | 2600 | `		}` |
|       39 | 2601 | `		pCursor = pValTok + 1;` |
|        - | 2602 | `		/* Consume separating comma (or end). */` |
|       39 | 2603 | `		if( pCursor < pBodyEnd ){` |
|        3 | 2604 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|      ! 0 | 2605 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2606 | `					"declare: Expecting ',' or ')' after directive value");` |
|      ! 0 | 2607 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2608 | `				return SXRET_OK;` |
|        - | 2609 | `			}` |
|        3 | 2610 | `			pCursor++;` |
|        1 | 2611 | `		}` |
|        5 | 2612 | `	}` |
|        - | 2613 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|        - | 2614 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|        - | 2615 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|       37 | 2616 | `	return SXRET_OK;` |
|        2 | 2617 | `Synchro:` |
|        - | 2618 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|       16 | 2619 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       12 | 2620 | `		pGen->pIn++;` |
|        2 | 2621 | `	}` |
|        6 | 2622 | `	return SXRET_OK;` |
|       27 | 2623 | `}` |
|        - | 2624 | `/*` |
|        - | 2625 | ` * Compile a class constant.` |
|        - | 2626 | ` * According to the PHP language reference manual` |
|        - | 2627 | ` *  Class Constants` |
|        - | 2628 | ` *   It is possible to define constant values on a per-class basis remaining` |
|        - | 2629 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|        - | 2630 | ` *   you don't use the $ symbol to declare or use them.` |
|        - | 2631 | ` *   The value must be a constant expression, not (for example) a variable,` |
|        - | 2632 | ` *   a property, a result of a mathematical operation, or a function call.` |
|        - | 2633 | ` *   It's also possible for interfaces to have constants.` |
|        - | 2634 | ` * Symisc eXtension.` |
|        - | 2635 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|        - | 2636 | ` *  the zend engine would allow only simple scalar value.` |
|        - | 2637 | ` *  Example:` |
|        - | 2638 | ` *   class Test{` |
|        - | 2639 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|        - | 2640 | ` *   };` |
|        - | 2641 | ` *   var_dump(TEST::MyConst);` |
|        - | 2642 | ` *   Refer to the official documentation for more information on the powerful extension` |
|        - | 2643 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|        - | 2644 | ` */` |
|        - | 2645 | `/*` |
|        - | 2646 | ` * Exception handling.` |
|        - | 2647 | ` *  According to the PHP language reference manual` |
|        - | 2648 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|        - | 2649 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|        - | 2650 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|        - | 2651 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|        - | 2652 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|        - | 2653 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|        - | 2654 | ` *    (or re-thrown) within a catch block.` |
|        - | 2655 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|        - | 2656 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|        - | 2657 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|        - | 2658 | ` *    been defined with set_exception_handler().` |
|        - | 2659 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|        - | 2660 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|        - | 2661 | ` */` |
|        - | 2662 | `/*` |
|        - | 2663 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|        - | 2664 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|        - | 2665 | ` * indicates failure.` |
|        - | 2666 | ` */` |
|   544978 | 2667 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 2668 | `{` |
|        - | 2669 | ``	/* php decides throwability at RUNTIME: `throw 5` compiles and raises the`` |
|        - | 2670 | `	 * catchable Error "Can only throw objects" when it executes, and a` |
|        - | 2671 | `	 * non-Throwable object raises "Cannot throw objects that do not implement` |
|        - | 2672 | `	 * Throwable". This used to whitelist a handful of expression shapes and` |
|        - | 2673 | `	 * reject the rest at compile time as a "friendlier" error, which meant the` |
|        - | 2674 | `	 * program never ran and no catch could ever see it — and it was inconsistent` |
|        - | 2675 | ``	 * anyway, since `throw $v` (a variable holding an int) always compiled and`` |
|        - | 2676 | `	 * fell through to the same runtime path. OP_THROW now owns the whole rule,` |
|        - | 2677 | ``	 * so accept any expression here; `throw;` with no operand is still rejected`` |
|        - | 2678 | `	 * by PH7_CompileThrow's SXERR_EMPTY branch. */` |
|   272489 | 2679 | `	SXUNUSED(pGen);` |
|   272489 | 2680 | `	SXUNUSED(pRoot);` |
|   544983 | 2681 | `	return SXRET_OK;` |
|        5 | 2682 | `}` |
|        - | 2683 | `/*` |
|        - | 2684 | ` * Compile a 'throw' statement.` |
|        - | 2685 | ` * throw: This is how you trigger an exception.` |
|        - | 2686 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|        - | 2687 | ` */` |
|   544942 | 2688 | `PH7_PRIVATE sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|        5 | 2689 | `{` |
|   544947 | 2690 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2691 | `	GenBlock *pBlock;` |
|        - | 2692 | `	sxu32 nIdx;` |
|        - | 2693 | `	sxi32 rc;` |
|   544947 | 2694 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|        - | 2695 | `	/* Compile the expression */` |
|   544947 | 2696 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|   544947 | 2697 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 | 2698 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|      ! 0 | 2699 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2700 | `			return SXERR_ABORT;` |
|        - | 2701 | `		}` |
|      ! 0 | 2702 | `		return SXRET_OK;` |
|        - | 2703 | `	}` |
|   544947 | 2704 | `	pBlock = pGen->pCurrent;` |
|        - | 2705 | `	/* Point to the top most function or try block and emit the forward jump */` |
|  2171267 | 2706 | `	while(pBlock->pParent){` |
|  2171261 | 2707 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|   544941 | 2708 | `			break;` |
|        - | 2709 | `		}` |
|        - | 2710 | `		/* Point to the parent block */` |
|  1626325 | 2711 | `		pBlock = pBlock->pParent;` |
|        5 | 2712 | `	}` |
|        - | 2713 | `	/* Emit the throw instruction */` |
|   544947 | 2714 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|        - | 2715 | `	/* Emit the jump */` |
|   544947 | 2716 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|   544947 | 2717 | `	return SXRET_OK;` |
|   272476 | 2718 | `}` |
|        - | 2719 | `/*` |
|        - | 2720 | ` * Compile a PHP 8.0 'throw' expression.` |
|        - | 2721 | ` * Called from the expression code generator when a 'throw' keyword is` |
|        - | 2722 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|        - | 2723 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|        - | 2724 | ` * the validator guarantees the operand is a valid exception target.` |
|        - | 2725 | ` */` |
|       36 | 2726 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|        2 | 2727 | `{` |
|       38 | 2728 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2729 | `	GenBlock *pBlock;` |
|        - | 2730 | `	sxu32 nIdx;` |
|        - | 2731 | `	sxi32 rc;` |
|       18 | 2732 | `	(void)iCompileFlag;` |
|       38 | 2733 | `	pGen->pIn++; /* Skip 'throw' */` |
|       38 | 2734 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 2735 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 2736 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 2737 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2738 | `			return SXERR_ABORT;` |
|        - | 2739 | `		}` |
|      ! 0 | 2740 | `		return SXRET_OK;` |
|        - | 2741 | `	}` |
|       38 | 2742 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|       38 | 2743 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 2744 | `		return SXERR_ABORT;` |
|        - | 2745 | `	}` |
|       38 | 2746 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 | 2747 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 2748 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 2749 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2750 | `			return SXERR_ABORT;` |
|        - | 2751 | `		}` |
|      ! 0 | 2752 | `		return SXRET_OK;` |
|        - | 2753 | `	}` |
|        - | 2754 | `	/* Walk up to nearest exception/function block for the jump target */` |
|       38 | 2755 | `	pBlock = pGen->pCurrent;` |
|       60 | 2756 | `	while( pBlock->pParent ){` |
|       49 | 2757 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|       27 | 2758 | `			break;` |
|        - | 2759 | `		}` |
|       23 | 2760 | `		pBlock = pBlock->pParent;` |
|        1 | 2761 | `	}` |
|       38 | 2762 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       38 | 2763 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|       38 | 2764 | `	return SXRET_OK;` |
|       20 | 2765 | `}` |
|        - | 2766 | `/*` |
|        - | 2767 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|        - | 2768 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|        - | 2769 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|        - | 2770 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|        - | 2771 | ` * compile error propagated from the parser.` |
|        - | 2772 | ` */` |
|       56 | 2773 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|        5 | 2774 | `{` |
|        - | 2775 | `	SyString sClassName;` |
|        - | 2776 | `	SyToken *pToken;` |
|        - | 2777 | `	SyString *pName;` |
|        - | 2778 | `	char *zDup;` |
|        - | 2779 | `	sxi32 rc;` |
|       61 | 2780 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       61 | 2781 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|       61 | 2782 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|       61 | 2783 | `	SySetInit(&pCatch->sByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       61 | 2784 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 | 2785 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 2786 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 2787 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 2788 | `		return SXERR_INVALID;` |
|        - | 2789 | `	}` |
|       61 | 2790 | `	pGen->pIn++; /* '(' */` |
|       28 | 2791 | `	for(;;){` |
|        - | 2792 | `		SyBlob sResolved;` |
|       61 | 2793 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       61 | 2794 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 2795 | `			SyBlobRelease(&sResolved);` |
|      ! 0 | 2796 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 2797 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 2798 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 2799 | `			return SXERR_INVALID;` |
|        - | 2800 | `		}` |
|       89 | 2801 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       56 | 2802 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       61 | 2803 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|       61 | 2804 | `		SyBlobRelease(&sResolved);` |
|       61 | 2805 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|       61 | 2806 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|       61 | 2807 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       56 | 2808 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|        5 | 2809 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|      ! 0 | 2810 | `			pGen->pIn++; continue;` |
|        - | 2811 | `		}` |
|       61 | 2812 | `		break;` |
|      ! 0 | 2813 | `	}` |
|        - | 2814 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|        - | 2815 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding. */` |
|       61 | 2816 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|        3 | 2817 | `		pGen->pIn++; /* ')' */` |
|        3 | 2818 | `		return SXRET_OK;` |
|        - | 2819 | `	}` |
|       54 | 2820 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|       59 | 2821 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 2822 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 2823 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 2824 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 2825 | `		return SXERR_INVALID;` |
|        - | 2826 | `	}` |
|       59 | 2827 | `	pGen->pIn++; /* '$' */` |
|       59 | 2828 | `	pName = &pGen->pIn->sData;` |
|       59 | 2829 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       59 | 2830 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|       59 | 2831 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|       59 | 2832 | `	pGen->pIn++;` |
|       59 | 2833 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 | 2834 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 2835 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 2836 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 2837 | `		return SXERR_INVALID;` |
|        - | 2838 | `	}` |
|       59 | 2839 | `	pGen->pIn++; /* ')' */` |
|       59 | 2840 | `	return SXRET_OK;` |
|       33 | 2841 | `}` |
|        - | 2842 | `/*` |
|        - | 2843 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|        - | 2844 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|        - | 2845 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|        - | 2846 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|        - | 2847 | ` * VmThrowException):` |
|        - | 2848 | ` *` |
|        - | 2849 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|        - | 2850 | ` *    <try body>` |
|        - | 2851 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|        - | 2852 | ` *    JMP  -> finally\|end` |
|        - | 2853 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|        - | 2854 | ` *    <catch body>` |
|        - | 2855 | ` *    JMP  -> finally\|end` |
|        - | 2856 | ` *    ... more catches ...` |
|        - | 2857 | ` *  Lfin: <finally body>` |
|        - | 2858 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|        - | 2859 | ` *  Lend:` |
|        - | 2860 | ` */` |
|      100 | 2861 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|        5 | 2862 | `{` |
|      105 | 2863 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2864 | `	GenBlock *pTry;` |
|        - | 2865 | `	VmInstr *pInstr;` |
|      105 | 2866 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|        - | 2867 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|        - | 2868 | `	sxi32 rc;` |
|      105 | 2869 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|        - | 2870 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION) */` |
|      105 | 2871 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|      105 | 2872 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      105 | 2873 | `	pTry->pUserData = pException;` |
|      105 | 2874 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|      105 | 2875 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|      105 | 2876 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      105 | 2877 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      105 | 2878 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|      105 | 2879 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|        - | 2880 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|      105 | 2881 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|      105 | 2882 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|      105 | 2883 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|      105 | 2884 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 2885 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|      105 | 2886 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|        - | 2887 | `	/* Catch clauses (inline) */` |
|      105 | 2888 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      100 | 2889 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       61 | 2890 | `		sxu32 k = 0;` |
|       84 | 2891 | `		for(;;){` |
|        - | 2892 | `			ph7_exception_block sCatch;` |
|        - | 2893 | `			GenBlock *pCatchBlk;` |
|      117 | 2894 | `			sxu32 idxJmp = 0;` |
|      112 | 2895 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      107 | 2896 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|       33 | 2897 | `				break;` |
|        - | 2898 | `			}` |
|       61 | 2899 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|       61 | 2900 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       61 | 2901 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|       61 | 2902 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|       61 | 2903 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|       61 | 2904 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatchBlk);` |
|       61 | 2905 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        - | 2906 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|        - | 2907 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|        - | 2908 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump). */` |
|       61 | 2909 | `			pCatchBlk->pUserData = pException;` |
|       61 | 2910 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|       61 | 2911 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       61 | 2912 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|       61 | 2913 | `			GenStateLeaveBlock(&(*pGen),0);` |
|        - | 2914 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|        - | 2915 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|       61 | 2916 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       61 | 2917 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|       61 | 2918 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|       61 | 2919 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|       61 | 2920 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       61 | 2921 | `			k++;` |
|        5 | 2922 | `		}` |
|       28 | 2923 | `	}` |
|        - | 2924 | `	/* Finally (inline) */` |
|      105 | 2925 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       80 | 2926 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|        - | 2927 | `		GenBlock *pFinBlk;` |
|       52 | 2928 | `		pGen->pIn++; /* Jump 'finally' */` |
|       52 | 2929 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|       52 | 2930 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|       52 | 2931 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       52 | 2932 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       52 | 2933 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       52 | 2934 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|       52 | 2935 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       52 | 2936 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|       52 | 2937 | `		pException->iHasFinally = 1;` |
|       24 | 2938 | `	}` |
|      105 | 2939 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|      105 | 2940 | `	pException->iInlined = 1;` |
|        - | 2941 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|        - | 2942 | `	{` |
|      105 | 2943 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|        - | 2944 | `		sxu32 *aJ; sxu32 n;` |
|      105 | 2945 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|      105 | 2946 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|      105 | 2947 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|      161 | 2948 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|       61 | 2949 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|       61 | 2950 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|       33 | 2951 | `		}` |
|        - | 2952 | `	}` |
|      105 | 2953 | `	SySetRelease(&aCatchJmp);` |
|      105 | 2954 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|      ! 0 | 2955 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|      ! 0 | 2956 | `	}` |
|      105 | 2957 | `	return SXRET_OK;` |
|       55 | 2958 | `}` |
|        - | 2959 | `/*` |
|        - | 2960 | ` * Compile a 'catch' block.` |
|        - | 2961 | ` * Catch: A "catch" block retrieves an exception and creates` |
|        - | 2962 | ` * an object containing the exception information.` |
|        - | 2963 | ` */` |
|    24812 | 2964 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|        5 | 2965 | `{` |
|    24817 | 2966 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2967 | `	ph7_exception_block sCatch;` |
|        - | 2968 | `	SySet *pInstrContainer;` |
|        - | 2969 | `	SyString sClassName;` |
|        - | 2970 | `	GenBlock *pCatch;` |
|        - | 2971 | `	SyToken *pToken;` |
|        - | 2972 | `	SyString *pName;` |
|        - | 2973 | `	char *zDup;` |
|        - | 2974 | `	sxi32 rc;` |
|    24817 | 2975 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|        - | 2976 | `	/* Zero the structure */` |
|    24817 | 2977 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|        - | 2978 | `	/* Initialize fields */` |
|    24817 | 2979 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|    24817 | 2980 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|    24817 | 2981 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|        - | 2982 | `			/* Unexpected token,break immediately */` |
|      ! 0 | 2983 | `			pToken = pGen->pIn;` |
|      ! 0 | 2984 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 2985 | `				pToken--;` |
|      ! 0 | 2986 | `			}` |
|      ! 0 | 2987 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 2988 | `				"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 2989 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 2990 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2991 | `				return SXERR_ABORT;` |
|        - | 2992 | `			}` |
|      ! 0 | 2993 | `			return SXERR_INVALID;` |
|        - | 2994 | `	}` |
|        - | 2995 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|    24817 | 2996 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|    12421 | 2997 | `	for(;;){` |
|        - | 2998 | `		SyBlob sResolved;` |
|    24847 | 2999 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    24847 | 3000 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|        6 | 3001 | `			SyBlobRelease(&sResolved);` |
|        6 | 3002 | `			pToken = pGen->pIn;` |
|        6 | 3003 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 3004 | `				pToken--;` |
|      ! 0 | 3005 | `			}` |
|        8 | 3006 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 3007 | `				"syntax error, unexpected %s \"%z\"",` |
|        2 | 3008 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|        6 | 3009 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3010 | `				return SXERR_ABORT;` |
|        - | 3011 | `			}` |
|        6 | 3012 | `			return SXERR_INVALID;` |
|        - | 3013 | `		}` |
|        - | 3014 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|        - | 3015 | `		 * transient SyBlob allocation. */` |
|    37262 | 3016 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    24838 | 3017 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|    24843 | 3018 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|    24843 | 3019 | `		SyBlobRelease(&sResolved);` |
|    24843 | 3020 | `		if( zDup == 0 ){` |
|      ! 0 | 3021 | `			goto Mem;` |
|        - | 3022 | `		}` |
|    24843 | 3023 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|    24843 | 3024 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 3025 | `			goto Mem;` |
|        - | 3026 | `		}` |
|        - | 3027 | `		/* Check for '\|' (multi-catch separator) */` |
|    24838 | 3028 | `		if( pGen->pIn < pGen->pEnd &&` |
|    24838 | 3029 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|       35 | 3030 | `			pGen->pIn->sData.nByte == 1 &&` |
|       30 | 3031 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|       32 | 3032 | `			pGen->pIn++; /* Consume the '\|' */` |
|       32 | 3033 | `			continue;` |
|        - | 3034 | `		}` |
|    24813 | 3035 | `		break;` |
|      ! 0 | 3036 | `	}` |
|        - | 3037 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|        - | 3038 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding;` |
|        - | 3039 | `	 * jump straight to compiling the block below. */` |
|    24813 | 3040 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) /*)*/ ){` |
|        7 | 3041 | `		goto CatchBody;` |
|        - | 3042 | `	}` |
|    24802 | 3043 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|    24807 | 3044 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - | 3045 | `			/* Unexpected token,break immediately */` |
|      ! 0 | 3046 | `			pToken = pGen->pIn;` |
|      ! 0 | 3047 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 3048 | `				pToken--;` |
|      ! 0 | 3049 | `			}` |
|      ! 0 | 3050 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 3051 | `				"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 3052 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 3053 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3054 | `				return SXERR_ABORT;` |
|        - | 3055 | `			}` |
|      ! 0 | 3056 | `			return SXERR_INVALID;` |
|        - | 3057 | `	}` |
|    24807 | 3058 | `	pGen->pIn++; /* Jump the dollar sign */` |
|        - | 3059 | `	/* Duplicate instance name */` |
|    24807 | 3060 | `	pName = &pGen->pIn->sData;` |
|    24807 | 3061 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    24807 | 3062 | `	if( zDup == 0 ){` |
|      ! 0 | 3063 | `		goto Mem;` |
|        - | 3064 | `	}` |
|    24807 | 3065 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|    24807 | 3066 | `	pGen->pIn++;` |
|    12404 | 3067 | `CatchBody:` |
|    24813 | 3068 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|        - | 3069 | `		/* Unexpected token,break immediately */` |
|      ! 0 | 3070 | `		pToken = pGen->pIn;` |
|      ! 0 | 3071 | `		if( pToken >= pGen->pEnd ){` |
|      ! 0 | 3072 | `			pToken--;` |
|      ! 0 | 3073 | `		}` |
|      ! 0 | 3074 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 3075 | `			"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 3076 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 3077 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3078 | `			return SXERR_ABORT;` |
|        - | 3079 | `		}` |
|      ! 0 | 3080 | `		return SXERR_INVALID;` |
|        - | 3081 | `	}` |
|        - | 3082 | `	/* Compile the block */` |
|    24813 | 3083 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|        - | 3084 | `	/* Create the catch block */` |
|    24813 | 3085 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|    24813 | 3086 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3087 | `		return SXERR_ABORT;` |
|        - | 3088 | `	}` |
|        - | 3089 | `	/* Swap bytecode container */` |
|    24813 | 3090 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    24813 | 3091 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|        - | 3092 | `	/* Compile the block */` |
|    24813 | 3093 | `	PH7_CompileBlock(&(*pGen),0);` |
|        - | 3094 | `	/* Fix forward jumps now the destination is resolved  */` |
|    24813 | 3095 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 3096 | `	/* Emit the DONE instruction */` |
|    24813 | 3097 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - | 3098 | `	/* Leave the block */` |
|    24813 | 3099 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 3100 | `	/* Restore the default container */` |
|    24813 | 3101 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - | 3102 | `	/* Install the catch block */` |
|    24813 | 3103 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|    24813 | 3104 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3105 | `		goto Mem;` |
|        - | 3106 | `	}` |
|    24813 | 3107 | `	return SXRET_OK;` |
|      ! 0 | 3108 | `Mem:` |
|      ! 0 | 3109 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 3110 | `	return SXERR_ABORT;` |
|    12411 | 3111 | `}` |
|        - | 3112 | `/*` |
|        - | 3113 | ` * Compile a 'try' block.` |
|        - | 3114 | ` * A function using an exception should be in a "try" block.` |
|        - | 3115 | ` * If the exception does not trigger, the code will continue` |
|        - | 3116 | ` * as normal. However if the exception triggers, an exception` |
|        - | 3117 | ` * is "thrown".` |
|        - | 3118 | ` */` |
|    24970 | 3119 | `PH7_PRIVATE sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|        5 | 3120 | `{` |
|        - | 3121 | `	ph7_exception *pException;` |
|    24975 | 3122 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 3123 | `	GenBlock *pTry;` |
|        - | 3124 | `	sxu32 nJmpIdx;` |
|        - | 3125 | `	sxi32 rc;` |
|        - | 3126 | `	/* Create the exception container */` |
|    24975 | 3127 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|    24975 | 3128 | `	if( pException == 0 ){` |
|      ! 0 | 3129 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|      ! 0 | 3130 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 3131 | `		return SXERR_ABORT;` |
|        - | 3132 | `	}` |
|        - | 3133 | `	/* Zero the structure */` |
|    24975 | 3134 | `	SyZero(pException,sizeof(ph7_exception));` |
|        - | 3135 | `	/* Initialize fields */` |
|    24975 | 3136 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|    24975 | 3137 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|    24975 | 3138 | `	pException->iHasFinally = 0;` |
|    24975 | 3139 | `	pException->iFinallyDone = 0;` |
|    24975 | 3140 | `	pException->pVm = pGen->pVm;` |
|        - | 3141 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|        - | 3142 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the legacy path.`` |
|        - | 3143 | `	 * DORMANT until the inline VM handlers (OP_CATCH / OP_END_FINALLY dispatch,` |
|        - | 3144 | `	 * VmThrowException pc-redirect, return/break-through-finally threading, generator` |
|        - | 3145 | `	 * park of aFinallyAction) land — the compiler emits the layout but the VM cannot yet` |
|        - | 3146 | `	 * execute it. Guarded by pVm->bInlineTryCatch (default 0) so the tree stays green. */` |
|    24975 | 3147 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|      105 | 3148 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|        - | 3149 | `	}` |
|        - | 3150 | `	/* Create the try block */` |
|    24875 | 3151 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|    24875 | 3152 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3153 | `		return SXERR_ABORT;` |
|        - | 3154 | `	}` |
|        - | 3155 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|    24875 | 3156 | `	pTry->pUserData = pException;` |
|        - | 3157 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|    24875 | 3158 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|        - | 3159 | `	/* Fix the jump later when the destination is resolved */` |
|    24875 | 3160 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|    24875 | 3161 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|        - | 3162 | `	/* Compile the block */` |
|    24875 | 3163 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|    24875 | 3164 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 3165 | `		return SXERR_ABORT;` |
|        - | 3166 | `	}` |
|        - | 3167 | `	/* Fix forward jumps now the destination is resolved */` |
|    24875 | 3168 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 3169 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|    24875 | 3170 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|        - | 3171 | `	/* Leave the block */` |
|    24875 | 3172 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 3173 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|    24875 | 3174 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|    24868 | 3175 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|        - | 3176 | `		/* Compile one or more catch blocks */` |
|    24808 | 3177 | `		for(;;){` |
|    49616 | 3178 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    37294 | 3179 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|    12407 | 3180 | `					break;` |
|        - | 3181 | `			}` |
|    24817 | 3182 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|    24817 | 3183 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3184 | `				return SXERR_ABORT;` |
|        - | 3185 | `			}` |
|        5 | 3186 | `		}` |
|    12402 | 3187 | `	}` |
|        - | 3188 | `	/* Compile optional finally block */` |
|    24875 | 3189 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      804 | 3190 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|        - | 3191 | `		SySet *pInstrContainer;` |
|        - | 3192 | `		GenBlock *pFinBlock;` |
|      129 | 3193 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|        - | 3194 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      129 | 3195 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      129 | 3196 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 3197 | `			return SXERR_ABORT;` |
|        - | 3198 | `		}` |
|        - | 3199 | `		/* Swap bytecode container */` |
|      129 | 3200 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      129 | 3201 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|        - | 3202 | `		/* Compile the finally body */` |
|      129 | 3203 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      129 | 3204 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3205 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 | 3206 | `			return SXERR_ABORT;` |
|        - | 3207 | `		}` |
|        - | 3208 | `		/* Fix forward jumps now the destination is resolved */` |
|      129 | 3209 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 3210 | `		/* Emit DONE to terminate the finally block */` |
|      129 | 3211 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - | 3212 | `		/* Leave the block */` |
|      129 | 3213 | `		GenStateLeaveBlock(&(*pGen),0);` |
|        - | 3214 | `		/* Restore the default container */` |
|      129 | 3215 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      129 | 3216 | `		pException->iHasFinally = 1;` |
|       62 | 3217 | `	}` |
|        - | 3218 | `	/* Must have at least one catch or finally */` |
|    24875 | 3219 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|        8 | 3220 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 3221 | `			"Cannot use try without catch or finally");` |
|        8 | 3222 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3223 | `			return SXERR_ABORT;` |
|        - | 3224 | `		}` |
|        3 | 3225 | `	}` |
|    24875 | 3226 | `	return SXRET_OK;` |
|    12490 | 3227 | `}` |
|        - | 3228 | `/*` |
|        - | 3229 | ` * Compile a switch block.` |
|        - | 3230 | ` *  (See block-comment below for more information)` |
|        - | 3231 | ` */` |
|   135282 | 3232 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|        5 | 3233 | `{` |
|   135287 | 3234 | `	sxi32 rc = SXRET_OK;` |
|   135287 | 3235 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|        - | 3236 | `		/* Unexpected token */` |
|      ! 0 | 3237 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 | 3238 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3239 | `			return SXERR_ABORT;` |
|        - | 3240 | `		}` |
|      ! 0 | 3241 | `		pGen->pIn++;` |
|      ! 0 | 3242 | `	}` |
|   135287 | 3243 | `	pGen->pIn++;` |
|        - | 3244 | `	/* First instruction to execute in this block. */` |
|   135287 | 3245 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|        - | 3246 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|        - | 3247 | `	 * or the '}' token */` |
|   129583 | 3248 | `	for(;;){` |
|   259171 | 3249 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 3250 | `			/* No more input to process */` |
|      ! 0 | 3251 | `			break;` |
|        - | 3252 | `		}` |
|   259171 | 3253 | `		rc = SXRET_OK;` |
|   259171 | 3254 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|    34843 | 3255 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|    11617 | 3256 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|        - | 3257 | `					/* Unexpected token */` |
|      ! 0 | 3258 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|      ! 0 | 3259 | `						&pGen->pIn->sData);` |
|      ! 0 | 3260 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3261 | `						return SXERR_ABORT;` |
|        - | 3262 | `					}` |
|        - | 3263 | `					/* FALL THROUGH */` |
|      ! 0 | 3264 | `				}` |
|    11617 | 3265 | `				rc = SXERR_EOF;` |
|    11617 | 3266 | `				break;` |
|        - | 3267 | `			}` |
|    11618 | 3268 | `		}else{` |
|        - | 3269 | `			sxi32 nKwrd;` |
|        - | 3270 | `			/* Extract the keyword */` |
|   224333 | 3271 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   224333 | 3272 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|    61839 | 3273 | `				break;` |
|        - | 3274 | `			}` |
|   100665 | 3275 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|        3 | 3276 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|        - | 3277 | `					/* Unexpected token */` |
|      ! 0 | 3278 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|      ! 0 | 3279 | `						&pGen->pIn->sData);` |
|      ! 0 | 3280 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3281 | `						return SXERR_ABORT;` |
|        - | 3282 | `					}` |
|        - | 3283 | `					/* FALL THROUGH */` |
|      ! 0 | 3284 | `				}` |
|        - | 3285 | `				/* Block compiled */` |
|        3 | 3286 | `				break;` |
|        - | 3287 | `			}` |
|        - | 3288 | `		}` |
|        - | 3289 | `		/* Compile block */` |
|   123889 | 3290 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|   123889 | 3291 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3292 | `			return SXERR_ABORT;` |
|        - | 3293 | `		}` |
|        5 | 3294 | `	}` |
|   135287 | 3295 | `	return rc;` |
|    67646 | 3296 | `}` |
|        - | 3297 | `/*` |
|        - | 3298 | ` * Compile a case eXpression.` |
|        - | 3299 | ` *  (See block-comment below for more information)` |
|        - | 3300 | ` */` |
|   131400 | 3301 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|        5 | 3302 | `{` |
|        - | 3303 | `	SySet *pInstrContainer;` |
|        - | 3304 | `	SyToken *pEnd,*pTmp;` |
|   131405 | 3305 | `	sxi32 iNest = 0;` |
|        - | 3306 | `	sxi32 rc;` |
|        - | 3307 | `	/* Delimit the expression */` |
|   131405 | 3308 | `	pEnd = pGen->pIn;` |
|   262813 | 3309 | `	while( pEnd < pGen->pEnd ){` |
|   262813 | 3310 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|        - | 3311 | `			/* Increment nesting level */` |
|        3 | 3312 | `			iNest++;` |
|   262812 | 3313 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|        - | 3314 | `			/* Decrement nesting level */` |
|        3 | 3315 | `			iNest--;` |
|   262810 | 3316 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|   131405 | 3317 | `			break;` |
|        - | 3318 | `		}` |
|   131413 | 3319 | `		pEnd++;` |
|        5 | 3320 | `	}` |
|   131405 | 3321 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 3322 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|      ! 0 | 3323 | `		if( rc == SXERR_ABORT ){` |
|        - | 3324 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 3325 | `			return SXERR_ABORT;` |
|        - | 3326 | `		}` |
|      ! 0 | 3327 | `	}` |
|        - | 3328 | `	/* Swap token stream */` |
|   131405 | 3329 | `	pTmp = pGen->pEnd;` |
|   131405 | 3330 | `	pGen->pEnd = pEnd;` |
|   131405 | 3331 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   131405 | 3332 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|   131405 | 3333 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 3334 | `	/* Emit the done instruction */` |
|   131405 | 3335 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   131405 | 3336 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - | 3337 | `	/* Update token stream */` |
|   131405 | 3338 | `	pGen->pIn  = pEnd;` |
|   131405 | 3339 | `	pGen->pEnd = pTmp;` |
|   131405 | 3340 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 3341 | `		return SXERR_ABORT;` |
|        - | 3342 | `	}` |
|   131405 | 3343 | `	return SXRET_OK;` |
|    65705 | 3344 | `}` |
|        - | 3345 | `/*` |
|        - | 3346 | ` * Compile the smart switch statement.` |
|        - | 3347 | ` * According to the PHP language reference manual` |
|        - | 3348 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|        - | 3349 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|        - | 3350 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|        - | 3351 | ` *  This is exactly what the switch statement is for.` |
|        - | 3352 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|        - | 3353 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|        - | 3354 | ` *  of the outer loop, use continue 2.` |
|        - | 3355 | ` *  Note that switch/case does loose comparision.` |
|        - | 3356 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|        - | 3357 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|        - | 3358 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|        - | 3359 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|        - | 3360 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|        - | 3361 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|        - | 3362 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|        - | 3363 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|        - | 3364 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|        - | 3365 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|        - | 3366 | ` *  list for the next case.` |
|        - | 3367 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|        - | 3368 | ` *  or floating-point numbers and strings.` |
|        - | 3369 | ` */` |
|    11614 | 3370 | `PH7_PRIVATE sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|        5 | 3371 | `{` |
|        - | 3372 | `	GenBlock *pSwitchBlock;` |
|        - | 3373 | `	SyToken *pTmp,*pEnd;` |
|        - | 3374 | `	ph7_switch *pSwitch;` |
|        - | 3375 | `	sxu32 nToken;` |
|        - | 3376 | `	sxu32 nLine;` |
|        - | 3377 | `	sxi32 rc;` |
|    11619 | 3378 | `	nLine = pGen->pIn->nLine;` |
|        - | 3379 | `	/* Jump the 'switch' keyword */` |
|    11619 | 3380 | `	pGen->pIn++;` |
|    11619 | 3381 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 3382 | `		/* Syntax error */` |
|      ! 0 | 3383 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|      ! 0 | 3384 | `		if( rc == SXERR_ABORT ){` |
|        - | 3385 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 3386 | `			return SXERR_ABORT;` |
|        - | 3387 | `		}` |
|      ! 0 | 3388 | `		goto Synchronize;` |
|        - | 3389 | `	}` |
|        - | 3390 | `	/* Jump the left parenthesis '(' */` |
|    11619 | 3391 | `	pGen->pIn++;` |
|    11619 | 3392 | `	pEnd = 0; /* cc warning */` |
|        - | 3393 | `	/* Create the loop block */` |
|    17426 | 3394 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|     5807 | 3395 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|    11619 | 3396 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3397 | `		return SXERR_ABORT;` |
|        - | 3398 | `	}` |
|        - | 3399 | `	/* Delimit the condition */` |
|    11619 | 3400 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    11619 | 3401 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - | 3402 | `		/* Empty expression */` |
|      ! 0 | 3403 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|      ! 0 | 3404 | `		if( rc == SXERR_ABORT ){` |
|        - | 3405 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 3406 | `			return SXERR_ABORT;` |
|        - | 3407 | `		}` |
|      ! 0 | 3408 | `	}` |
|        - | 3409 | `	/* Swap token streams */` |
|    11619 | 3410 | `	pTmp = pGen->pEnd;` |
|    11619 | 3411 | `	pGen->pEnd = pEnd;` |
|        - | 3412 | `	/* Compile the expression */` |
|    11619 | 3413 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    11619 | 3414 | `	if( rc == SXERR_ABORT ){` |
|        - | 3415 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 3416 | `		return SXERR_ABORT;` |
|        - | 3417 | `	}` |
|        - | 3418 | `	/* Update token stream */` |
|    11619 | 3419 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 | 3420 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 3421 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 3422 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3423 | `			return SXERR_ABORT;` |
|        - | 3424 | `		}` |
|      ! 0 | 3425 | `		pGen->pIn++;` |
|      ! 0 | 3426 | `	}` |
|    11619 | 3427 | `	pGen->pIn  = &pEnd[1];` |
|    11619 | 3428 | `	pGen->pEnd = pTmp;` |
|    11619 | 3429 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|    11614 | 3430 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|      ! 0 | 3431 | `			pTmp = pGen->pIn;` |
|      ! 0 | 3432 | `			if( pTmp >= pGen->pEnd ){` |
|      ! 0 | 3433 | `				pTmp--;` |
|      ! 0 | 3434 | `			}` |
|        - | 3435 | `			/* Unexpected token */` |
|      ! 0 | 3436 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|      ! 0 | 3437 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3438 | `				return SXERR_ABORT;` |
|        - | 3439 | `			}` |
|      ! 0 | 3440 | `			goto Synchronize;` |
|        - | 3441 | `	}` |
|        - | 3442 | `	/* Set the delimiter token */` |
|    11619 | 3443 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|        3 | 3444 | `		nToken = PH7_TK_KEYWORD;` |
|        - | 3445 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|        2 | 3446 | `	}else{` |
|    11617 | 3447 | `		nToken = PH7_TK_CCB; /* '}' */` |
|        - | 3448 | `	}` |
|    11619 | 3449 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|        - | 3450 | `	/* Create the switch blocks container */` |
|    11619 | 3451 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|    11619 | 3452 | `	if( pSwitch == 0 ){` |
|        - | 3453 | `		/* Abort compilation */` |
|      ! 0 | 3454 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 3455 | `		return SXERR_ABORT;` |
|        - | 3456 | `	}` |
|        - | 3457 | `	/* Zero the structure */` |
|    11619 | 3458 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|        - | 3459 | `	/* Initialize fields */` |
|    11619 | 3460 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|        - | 3461 | `	/* Emit the switch instruction */` |
|    11619 | 3462 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|        - | 3463 | `	/* Compile case blocks */` |
|   129477 | 3464 | `	for(;;){` |
|        - | 3465 | `		sxu32 nKwrd;` |
|   135289 | 3466 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 3467 | `			/* No more input to process */` |
|      ! 0 | 3468 | `			break;` |
|        - | 3469 | `		}` |
|   135289 | 3470 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 3471 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|        - | 3472 | `				/* Unexpected token */` |
|      ! 0 | 3473 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 3474 | `					&pGen->pIn->sData);` |
|      ! 0 | 3475 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3476 | `					return SXERR_ABORT;` |
|        - | 3477 | `				}` |
|        - | 3478 | `				/* FALL THROUGH */` |
|      ! 0 | 3479 | `			}` |
|        - | 3480 | `			/* Block compiled */` |
|      ! 0 | 3481 | `			break;` |
|        - | 3482 | `		}` |
|        - | 3483 | `		/* Extract the keyword */` |
|   135289 | 3484 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   135289 | 3485 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|        3 | 3486 | `			if( nToken != PH7_TK_KEYWORD ){` |
|        - | 3487 | `				/* Unexpected token */` |
|      ! 0 | 3488 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 3489 | `					&pGen->pIn->sData);` |
|      ! 0 | 3490 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3491 | `					return SXERR_ABORT;` |
|        - | 3492 | `				}` |
|        - | 3493 | `				/* FALL THROUGH */` |
|      ! 0 | 3494 | `			}` |
|        - | 3495 | `			/* Block compiled */` |
|        3 | 3496 | `			break;` |
|        - | 3497 | `		}` |
|   135287 | 3498 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|        - | 3499 | `			/*` |
|        - | 3500 | `			 * Accroding to the PHP language reference manual` |
|        - | 3501 | `			 *  A special case is the default case. This case matches anything` |
|        - | 3502 | `			 *  that wasn't matched by the other cases.` |
|        - | 3503 | `			 */` |
|     3887 | 3504 | `			if( pSwitch->nDefault > 0 ){` |
|        - | 3505 | `				/* Default case already compiled */` |
|      ! 0 | 3506 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|      ! 0 | 3507 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3508 | `					return SXERR_ABORT;` |
|        - | 3509 | `				}` |
|      ! 0 | 3510 | `			}` |
|     3887 | 3511 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|        - | 3512 | `			/* Compile the default block */` |
|     3887 | 3513 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|     3887 | 3514 | `			if( rc == SXERR_ABORT){` |
|      ! 0 | 3515 | `				return SXERR_ABORT;` |
|     3887 | 3516 | `			}else if( rc == SXERR_EOF ){` |
|     3885 | 3517 | `				break;` |
|        1 | 3518 | `			}` |
|   131406 | 3519 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|        - | 3520 | `			ph7_case_expr sCase;` |
|        - | 3521 | `			/* Standard case block */` |
|   131405 | 3522 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|        - | 3523 | `			/* initialize the structure */` |
|   131405 | 3524 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - | 3525 | `			/* Compile the case expression */` |
|   131405 | 3526 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|   131405 | 3527 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3528 | `				return SXERR_ABORT;` |
|        - | 3529 | `			}` |
|        - | 3530 | `			/* Compile the case block */` |
|   131405 | 3531 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|        - | 3532 | `			/* Insert in the switch container */` |
|   131405 | 3533 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|   131405 | 3534 | `			if( rc == SXERR_ABORT){` |
|      ! 0 | 3535 | `				return SXERR_ABORT;` |
|   131405 | 3536 | `			}else if( rc == SXERR_EOF ){` |
|     7737 | 3537 | `				break;` |
|        - | 3538 | `			}` |
|    61839 | 3539 | `		}else{` |
|        - | 3540 | `			/* Unexpected token */` |
|      ! 0 | 3541 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 3542 | `				&pGen->pIn->sData);` |
|      ! 0 | 3543 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3544 | `				return SXERR_ABORT;` |
|        - | 3545 | `			}` |
|      ! 0 | 3546 | `			break;` |
|        - | 3547 | `		}` |
|        5 | 3548 | `	}` |
|        - | 3549 | `	/* Fix all jumps now the destination is resolved */` |
|    11619 | 3550 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|    11619 | 3551 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 3552 | `	/* Release the loop block */` |
|    11619 | 3553 | `	GenStateLeaveBlock(pGen,0);` |
|    11619 | 3554 | `	if( pGen->pIn < pGen->pEnd ){` |
|        - | 3555 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|    11619 | 3556 | `		pGen->pIn++;` |
|     5807 | 3557 | `	}` |
|        - | 3558 | `	/* Statement successfully compiled */` |
|    11619 | 3559 | `	return SXRET_OK;` |
|      ! 0 | 3560 | `Synchronize:` |
|        - | 3561 | `	/* Synchronize with the first semi-colon */` |
|      ! 0 | 3562 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      ! 0 | 3563 | `		pGen->pIn++;` |
|      ! 0 | 3564 | `	}` |
|      ! 0 | 3565 | `	return SXRET_OK;` |
|     5812 | 3566 | `}` |
|        - | 3567 |  |
