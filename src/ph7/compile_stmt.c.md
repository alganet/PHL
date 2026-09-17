# src/ph7/compile_stmt.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1454/1981 lines (73.40%)

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
|        - |  997 | `	/* Compile initialization expressions if available */` |
|   127605 |  998 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  999 | `	/* Pop operand lvalues */` |
|   127605 | 1000 | `	if( rc == SXERR_ABORT ){` |
|        - | 1001 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1002 | `		return SXERR_ABORT;` |
|   127605 | 1003 | `	}else if( rc != SXERR_EMPTY ){` |
|   116017 | 1004 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    58006 | 1005 | `	}` |
|   127605 | 1006 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - | 1007 | `		/* Syntax error */` |
|      ! 0 | 1008 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|      ! 0 | 1009 | `		if( rc == SXERR_ABORT ){` |
|        - | 1010 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1011 | `			return SXERR_ABORT;` |
|        - | 1012 | `		}` |
|      ! 0 | 1013 | `		return SXRET_OK;` |
|        - | 1014 | `	}` |
|        - | 1015 | `	/* Jump the trailing ';' */` |
|   127605 | 1016 | `	pGen->pIn++;` |
|        - | 1017 | `	/* Create the loop block */` |
|   127605 | 1018 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   127605 | 1019 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1020 | `		return SXERR_ABORT;` |
|        - | 1021 | `	}` |
|        - | 1022 | `	/* Deffer continue jumps */` |
|   127605 | 1023 | `	pForBlock->bPostContinue = TRUE;` |
|        - | 1024 | `	/* Compile the condition */` |
|   127605 | 1025 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   127605 | 1026 | `	if( rc == SXERR_ABORT ){` |
|        - | 1027 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1028 | `		return SXERR_ABORT;` |
|   127605 | 1029 | `	}else if( rc != SXERR_EMPTY ){` |
|        - | 1030 | `		/* Emit the false jump */` |
|   116017 | 1031 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|        - | 1032 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   116017 | 1033 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    58006 | 1034 | `	}` |
|   127605 | 1035 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - | 1036 | `		/* Syntax error */` |
|        6 | 1037 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|        6 | 1038 | `		if( rc == SXERR_ABORT ){` |
|        - | 1039 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1040 | `			return SXERR_ABORT;` |
|        - | 1041 | `		}` |
|        6 | 1042 | `		return SXRET_OK;` |
|        - | 1043 | `	}` |
|        - | 1044 | `	/* Jump the trailing ';' */` |
|   127601 | 1045 | `	pGen->pIn++;` |
|        - | 1046 | `	/* Save the post condition stream */` |
|   127601 | 1047 | `	pPostStart = pGen->pIn;` |
|        - | 1048 | `	/* Compile the loop body — OUTSIDE the comma window (the body is ordinary` |
|        - | 1049 | ``	 * php, so `(1, 2)` inside it is the parse error it should be). */`` |
|   127601 | 1050 | `	pGen->nCommaExprOk--;` |
|   127601 | 1051 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   127601 | 1052 | `	pGen->pEnd = pTmp;` |
|   127601 | 1053 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   127601 | 1054 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 1055 | `		return SXERR_ABORT;` |
|        - | 1056 | `	}` |
|        - | 1057 | `	/* Fix post-continue jumps */` |
|   127601 | 1058 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|        - | 1059 | `		JumpFixup *aPost;` |
|        - | 1060 | `		VmInstr *pInstr;` |
|        - | 1061 | `		sxu32 nJumpDest;` |
|        - | 1062 | `		sxu32 n;` |
|    11603 | 1063 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|    11603 | 1064 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|    38649 | 1065 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|    27051 | 1066 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|    27051 | 1067 | `			if( pInstr ){` |
|        - | 1068 | `				/* Fix jump */` |
|    27051 | 1069 | `				pInstr->iP2 = nJumpDest;` |
|    13523 | 1070 | `			}` |
|    13528 | 1071 | `		}` |
|     5799 | 1072 | `	}` |
|        - | 1073 | `	/* compile the post-expressions if available */` |
|   127601 | 1074 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|      ! 0 | 1075 | `		pPostStart++;` |
|      ! 0 | 1076 | `	}` |
|   127601 | 1077 | `	if( pPostStart < pEnd ){` |
|        - | 1078 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   116015 | 1079 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   116015 | 1080 | `		pGen->nCommaExprOk++; /* post-expressions are a clause list again */` |
|   116015 | 1081 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   116015 | 1082 | `		pGen->nCommaExprOk--;` |
|   116015 | 1083 | `		if( pGen->pIn < pGen->pEnd ){` |
|        - | 1084 | `			/* Syntax error */` |
|      ! 0 | 1085 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|      ! 0 | 1086 | `			if( rc == SXERR_ABORT ){` |
|        - | 1087 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 1088 | `				return SXERR_ABORT;` |
|        - | 1089 | `			}` |
|      ! 0 | 1090 | `			return SXRET_OK;` |
|        - | 1091 | `		}` |
|   116015 | 1092 | `		RE_SWAP_DELIMITER(pGen);` |
|   116015 | 1093 | `		if( rc == SXERR_ABORT ){` |
|        - | 1094 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1095 | `			return SXERR_ABORT;` |
|   116015 | 1096 | `		}else if( rc != SXERR_EMPTY){` |
|        - | 1097 | `			/* Pop operand lvalue */` |
|   116015 | 1098 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    58005 | 1099 | `		}` |
|    58005 | 1100 | `	}` |
|        - | 1101 | `	/* Emit the unconditional jump to the start of the loop */` |
|   127601 | 1102 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|        - | 1103 | `	/* Fix all jumps now the destination is resolved */` |
|   127601 | 1104 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 1105 | `	/* Release the loop block */` |
|   127601 | 1106 | `	GenStateLeaveBlock(pGen,0);` |
|        - | 1107 | `	/* Statement successfully compiled */` |
|   127601 | 1108 | `	return SXRET_OK;` |
|    63805 | 1109 | `}` |
|        - | 1110 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|        - | 1111 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|        - | 1112 | ` * are allowed.` |
|        - | 1113 | ` */` |
|   460724 | 1114 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 1115 | `{` |
|   460729 | 1116 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|   460729 | 1117 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|        - | 1118 | `		/* Unexpected expression */` |
|      ! 0 | 1119 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 1120 | `			"foreach: Expecting a variable name");` |
|      ! 0 | 1121 | `		if( rc != SXERR_ABORT ){` |
|      ! 0 | 1122 | `			rc = SXERR_INVALID;` |
|      ! 0 | 1123 | `		}` |
|      ! 0 | 1124 | `	}` |
|   460729 | 1125 | `	return rc;` |
|        5 | 1126 | `}` |
|        - | 1127 | `/*` |
|        - | 1128 | ` * Compile the 'foreach' statement.` |
|        - | 1129 | ` * According to the PHP language reference` |
|        - | 1130 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|        - | 1131 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|        - | 1132 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|        - | 1133 | ` *  is a minor but useful extension of the first:` |
|        - | 1134 | ` *  foreach (array_expression as $value)` |
|        - | 1135 | ` *    statement` |
|        - | 1136 | ` *  foreach (array_expression as $key => $value)` |
|        - | 1137 | ` *   statement` |
|        - | 1138 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|        - | 1139 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|        - | 1140 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|        - | 1141 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|        - | 1142 | ` *  to the variable $key on each loop.` |
|        - | 1143 | ` *  Note:` |
|        - | 1144 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|        - | 1145 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|        - | 1146 | ` *  Note:` |
|        - | 1147 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|        - | 1148 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|        - | 1149 | ` *  or after the foreach without resetting it.` |
|        - | 1150 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|        - | 1151 | ` *  of copying the value.` |
|        - | 1152 | ` */` |
|   325254 | 1153 | `PH7_PRIVATE sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|        5 | 1154 | `{` |
|   325259 | 1155 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|   325259 | 1156 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|   325259 | 1157 | `	GenBlock *pForeachBlock = 0;` |
|        - | 1158 | `	ph7_foreach_info *pInfo;` |
|        - | 1159 | `	sxu32 nFalseJump;` |
|        - | 1160 | `	VmInstr *pInstr;` |
|        - | 1161 | `	sxu32 nLine;` |
|        - | 1162 | `	sxi32 rc;` |
|   325259 | 1163 | `	nLine = pGen->pIn->nLine;` |
|        - | 1164 | `	/* Jump the 'foreach' keyword */` |
|   325259 | 1165 | `	pGen->pIn++;` |
|   325259 | 1166 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1167 | `		/* Syntax error */` |
|      ! 0 | 1168 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|      ! 0 | 1169 | `		if( rc == SXERR_ABORT ){` |
|        - | 1170 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1171 | `			return SXERR_ABORT;` |
|        - | 1172 | `		}` |
|      ! 0 | 1173 | `		goto Synchronize;` |
|        - | 1174 | `	}` |
|        - | 1175 | `	/* Jump the left parenthesis '(' */` |
|   325259 | 1176 | `	pGen->pIn++;` |
|        - | 1177 | `	/* Create the loop block */` |
|   325259 | 1178 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|   325259 | 1179 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1180 | `		return SXERR_ABORT;` |
|        - | 1181 | `	}` |
|        - | 1182 | `	/* Delimit the expression */` |
|   325259 | 1183 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   325259 | 1184 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - | 1185 | `		/* Empty expression */` |
|      ! 0 | 1186 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|      ! 0 | 1187 | `		if( rc == SXERR_ABORT ){` |
|        - | 1188 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1189 | `			return SXERR_ABORT;` |
|        - | 1190 | `		}` |
|        - | 1191 | `		/* Synchronize */` |
|      ! 0 | 1192 | `		pGen->pIn = pEnd;` |
|      ! 0 | 1193 | `		if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 | 1194 | `			pGen->pIn++;` |
|      ! 0 | 1195 | `		}` |
|      ! 0 | 1196 | `		return SXRET_OK;` |
|        - | 1197 | `	}` |
|        - | 1198 | `	/* Compile the array expression */` |
|   325259 | 1199 | `	pCur = pGen->pIn;` |
|  1843505 | 1200 | `	while( pCur < pEnd ){` |
|  1843505 | 1201 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|   356177 | 1202 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|   356177 | 1203 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|        - | 1204 | `				/* Break with the first 'as' found */` |
|   325259 | 1205 | `				break;` |
|        - | 1206 | `			}` |
|    15459 | 1207 | `		}` |
|        - | 1208 | `		/* Advance the stream cursor */` |
|  1518251 | 1209 | `		pCur++;` |
|        5 | 1210 | `	}` |
|   325259 | 1211 | `	if( pCur <= pGen->pIn ){` |
|      ! 0 | 1212 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 1213 | `			"foreach: Missing array/object expression");` |
|      ! 0 | 1214 | `		if( rc == SXERR_ABORT ){` |
|        - | 1215 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1216 | `			return SXERR_ABORT;` |
|        - | 1217 | `		}` |
|      ! 0 | 1218 | `		goto Synchronize;` |
|        - | 1219 | `	}` |
|        - | 1220 | `	/* Swap token streams */` |
|   325259 | 1221 | `	pTmp = pGen->pEnd;` |
|   325259 | 1222 | `	pGen->pEnd = pCur;` |
|   325259 | 1223 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   325259 | 1224 | `	if( rc == SXERR_ABORT ){` |
|        - | 1225 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1226 | `		return SXERR_ABORT;` |
|        - | 1227 | `	}` |
|        - | 1228 | `	/* Update token stream */` |
|   325259 | 1229 | `	while(pGen->pIn < pCur ){` |
|      ! 0 | 1230 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 1231 | `		if( rc == SXERR_ABORT ){` |
|        - | 1232 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1233 | `			return SXERR_ABORT;` |
|        - | 1234 | `		}` |
|      ! 0 | 1235 | `		pGen->pIn++;` |
|      ! 0 | 1236 | `	}` |
|   325259 | 1237 | `	pCur++; /* Jump the 'as' keyword */` |
|   325259 | 1238 | `	pGen->pIn = pCur;` |
|   325259 | 1239 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 1240 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|      ! 0 | 1241 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1242 | `			return SXERR_ABORT;` |
|        - | 1243 | `		}` |
|      ! 0 | 1244 | `	}` |
|        - | 1245 | `	/* Create the foreach context */` |
|   325259 | 1246 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|   325259 | 1247 | `	if( pInfo == 0 ){` |
|      ! 0 | 1248 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|      ! 0 | 1249 | `		return SXERR_ABORT;` |
|        - | 1250 | `	}` |
|        - | 1251 | `	/* Zero the structure */` |
|   325259 | 1252 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|        - | 1253 | `	/* Initialize structure fields */` |
|   325259 | 1254 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|        - | 1255 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|        - | 1256 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|        - | 1257 | `	 * '=>'. */` |
|   325259 | 1258 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|   325259 | 1259 | `	if( pCur < pEnd ){` |
|        - | 1260 | `		/* Compile the expression holding the key name */` |
|   135499 | 1261 | `		if( pGen->pIn >= pCur ){` |
|      ! 0 | 1262 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|      ! 0 | 1263 | `			if( rc == SXERR_ABORT ){` |
|        - | 1264 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1265 | `				return SXERR_ABORT;` |
|        - | 1266 | `			}` |
|      ! 0 | 1267 | `		}else{` |
|   135499 | 1268 | `			pGen->pEnd = pCur;` |
|   135499 | 1269 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|   135499 | 1270 | `			if( rc == SXERR_ABORT ){` |
|        - | 1271 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1272 | `				return SXERR_ABORT;` |
|        - | 1273 | `			}` |
|   135499 | 1274 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|   135499 | 1275 | `			if( pInstr->p3 ){` |
|        - | 1276 | `				/* Record key name */` |
|   135499 | 1277 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    67747 | 1278 | `			}` |
|   135499 | 1279 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|        - | 1280 | `		}` |
|   135499 | 1281 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    67747 | 1282 | `	}` |
|   325259 | 1283 | `	pGen->pEnd = pEnd;` |
|   325259 | 1284 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 1285 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|      ! 0 | 1286 | `		if( rc == SXERR_ABORT ){` |
|        - | 1287 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1288 | `			return SXERR_ABORT;` |
|        - | 1289 | `		}` |
|      ! 0 | 1290 | `		goto Synchronize;` |
|        - | 1291 | `	}` |
|   325259 | 1292 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|       33 | 1293 | `		pGen->pIn++;` |
|        - | 1294 | `		/* Pass by reference  */` |
|       33 | 1295 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       15 | 1296 | `	}` |
|        - | 1297 | `	/* Check if the value target is list() */` |
|   325259 | 1298 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        8 | 1299 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|        - | 1300 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|        - | 1301 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|        - | 1302 | `		 */` |
|        - | 1303 | `		static int iForeachListCnt = 0;` |
|        - | 1304 | `		char zTmp[128];` |
|        - | 1305 | `		sxu32 nLen;` |
|        - | 1306 | `		char *zDup;` |
|       10 | 1307 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|       10 | 1308 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       10 | 1309 | `		if( zDup == 0 ){` |
|      ! 0 | 1310 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1311 | `			return SXERR_ABORT;` |
|        - | 1312 | `		}` |
|       10 | 1313 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|        - | 1314 | `		/* Save list() token boundaries */` |
|       10 | 1315 | `		pListStart = pGen->pIn;` |
|        - | 1316 | `		/* Advance past list(...) — validate parentheses */` |
|       10 | 1317 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|       10 | 1318 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1319 | `			/* php: syntax error, unexpected variable "$x", expecting "(" */` |
|        3 | 1320 | `			rc = PH7_GenSyntaxError(pGen,pGen->pIn < pEnd ? pGen->pIn : 0,"\"(\"");` |
|        3 | 1321 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1322 | `				return SXERR_ABORT;` |
|        - | 1323 | `			}` |
|        3 | 1324 | `			goto Synchronize;` |
|        - | 1325 | `		}` |
|        7 | 1326 | `		pGen->pIn++; /* Jump '(' */` |
|        7 | 1327 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|        7 | 1328 | `		if( pListEnd >= pEnd ){` |
|      ! 0 | 1329 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1330 | `				"foreach: Missing closing ')' after list");` |
|      ! 0 | 1331 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1332 | `				return SXERR_ABORT;` |
|        - | 1333 | `			}` |
|      ! 0 | 1334 | `			goto Synchronize;` |
|        - | 1335 | `		}` |
|        7 | 1336 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|        7 | 1337 | `		pListEnd = pGen->pIn;` |
|        7 | 1338 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|   325254 | 1339 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|        - | 1340 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|        - | 1341 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|        - | 1342 | `		 */` |
|        - | 1343 | `		static int iForeachShortListCnt = 0;` |
|        - | 1344 | `		char zTmp[128];` |
|        - | 1345 | `		sxu32 nLen;` |
|        - | 1346 | `		char *zDup;` |
|       17 | 1347 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       17 | 1348 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       17 | 1349 | `		if( zDup == 0 ){` |
|      ! 0 | 1350 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1351 | `			return SXERR_ABORT;` |
|        - | 1352 | `		}` |
|       17 | 1353 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|        - | 1354 | `		/* Save [...] token boundaries */` |
|       17 | 1355 | `		pListStart = pGen->pIn;` |
|        - | 1356 | `		/* Advance past [...] */` |
|       17 | 1357 | `		pGen->pIn++; /* Jump '[' */` |
|       17 | 1358 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       17 | 1359 | `		if( pListEnd >= pEnd ){` |
|      ! 0 | 1360 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1361 | `				"foreach: Missing closing ']' after short list");` |
|      ! 0 | 1362 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1363 | `				return SXERR_ABORT;` |
|        - | 1364 | `			}` |
|      ! 0 | 1365 | `			goto Synchronize;` |
|        - | 1366 | `		}` |
|       17 | 1367 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       17 | 1368 | `		pListEnd = pGen->pIn;` |
|       17 | 1369 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|        9 | 1370 | `	}else{` |
|        - | 1371 | `		/* Compile the expression holding the value name */` |
|   325235 | 1372 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|   325235 | 1373 | `		if( rc == SXERR_ABORT ){` |
|        - | 1374 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1375 | `			return SXERR_ABORT;` |
|        - | 1376 | `		}` |
|   325235 | 1377 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|   325235 | 1378 | `		if( pInstr->p3 ){` |
|        - | 1379 | `			/* Record value name */` |
|   325235 | 1380 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|   162615 | 1381 | `		}` |
|        - | 1382 | `	}` |
|        - | 1383 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|   325257 | 1384 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|        - | 1385 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   325257 | 1386 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|        - | 1387 | `	/* Record the first instruction to execute */` |
|   325257 | 1388 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|        - | 1389 | `	/* Emit the FOREACH_STEP instruction */` |
|   325257 | 1390 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|        - | 1391 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   325257 | 1392 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|        - | 1393 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|   325257 | 1394 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|        - | 1395 | `		SyToken *pSavedIn,*pSavedEnd;` |
|        - | 1396 | `		/* Load the temporary variable holding the current value onto the stack.` |
|        - | 1397 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|        - | 1398 | `		 */` |
|       23 | 1399 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|        - | 1400 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|        - | 1401 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|        - | 1402 | `		 * picks up the delimiter and the variable names inside.` |
|        - | 1403 | `		 */` |
|       23 | 1404 | `		pSavedIn = pGen->pIn;` |
|       23 | 1405 | `		pSavedEnd = pGen->pEnd;` |
|       23 | 1406 | `		pGen->pIn = pListStart;` |
|       23 | 1407 | `		pGen->pEnd = pListEnd;` |
|       23 | 1408 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       17 | 1409 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|        9 | 1410 | `		}else{` |
|        7 | 1411 | `			rc = PH7_CompileList(&(*pGen),0);` |
|        - | 1412 | `		}` |
|       23 | 1413 | `		pGen->pIn = pSavedIn;` |
|       23 | 1414 | `		pGen->pEnd = pSavedEnd;` |
|       23 | 1415 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1416 | `			return SXERR_ABORT;` |
|        - | 1417 | `		}` |
|        - | 1418 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|       23 | 1419 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       11 | 1420 | `	}` |
|        - | 1421 | `	/* Compile the loop body */` |
|   325257 | 1422 | `	pGen->pIn = &pEnd[1];` |
|   325257 | 1423 | `	pGen->pEnd = pTmp;` |
|   325257 | 1424 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|   325257 | 1425 | `	if( rc == SXERR_ABORT ){` |
|        - | 1426 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1427 | `		return SXERR_ABORT;` |
|        - | 1428 | `	}` |
|        - | 1429 | `	/* Emit the unconditional jump to the start of the loop */` |
|   325257 | 1430 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|        - | 1431 | `	/* Fix all jumps now the destination is resolved */` |
|   325257 | 1432 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 1433 | `	/* Release the loop block */` |
|   325257 | 1434 | `	GenStateLeaveBlock(pGen,0);` |
|        - | 1435 | `	/* Statement successfully compiled */` |
|   325257 | 1436 | `	return SXRET_OK;` |
|        1 | 1437 | `Synchronize:` |
|        - | 1438 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - | 1439 | `	 * compiling this erroneous block.` |
|        - | 1440 | `	 */` |
|        3 | 1441 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 | 1442 | `		pGen->pIn++;` |
|      ! 0 | 1443 | `	}` |
|        3 | 1444 | `	return SXRET_OK;` |
|   162632 | 1445 | `}` |
|        - | 1446 | `/*` |
|        - | 1447 | ` * Compile the infamous if/elseif/else if/else statements.` |
|        - | 1448 | ` * According to the PHP language reference` |
|        - | 1449 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|        - | 1450 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|        - | 1451 | ` *  that is similar to that of C:` |
|        - | 1452 | ` *  if (expr)` |
|        - | 1453 | ` *   statement` |
|        - | 1454 | ` *  else construct:` |
|        - | 1455 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|        - | 1456 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|        - | 1457 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|        - | 1458 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|        - | 1459 | ` *   $b, and a is NOT greater than b otherwise.` |
|        - | 1460 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|        - | 1461 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|        - | 1462 | ` *  elseif` |
|        - | 1463 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|        - | 1464 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|        - | 1465 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|        - | 1466 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|        - | 1467 | ` *   than b, a equal to b or a is smaller than b:` |
|        - | 1468 | ` *   <?php` |
|        - | 1469 | ` *    if ($a > $b) {` |
|        - | 1470 | ` *     echo "a is bigger than b";` |
|        - | 1471 | ` *    } elseif ($a == $b) {` |
|        - | 1472 | ` *     echo "a is equal to b";` |
|        - | 1473 | ` *    } else {` |
|        - | 1474 | ` *     echo "a is smaller than b";` |
|        - | 1475 | ` *    }` |
|        - | 1476 | ` *    ?>` |
|        - | 1477 | ` */` |
|  2415268 | 1478 | `PH7_PRIVATE sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|        5 | 1479 | `{` |
|  2415273 | 1480 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  2415273 | 1481 | `	GenBlock *pCondBlock = 0;` |
|        - | 1482 | `	sxu32 nJumpIdx;` |
|        - | 1483 | `	sxu32 nKeyID;` |
|        - | 1484 | `	sxi32 rc;` |
|        - | 1485 | `	/* Jump the 'if' keyword */` |
|  2415273 | 1486 | `	pGen->pIn++;` |
|  2415273 | 1487 | `	pToken = pGen->pIn;` |
|        - | 1488 | `	/* Create the conditional block */` |
|  2415273 | 1489 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  2415273 | 1490 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1491 | `		return SXERR_ABORT;` |
|        - | 1492 | `	}` |
|        - | 1493 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|  1350600 | 1494 | `	for(;;){` |
|  2701205 | 1495 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1496 | `			/* Syntax error */` |
|      ! 0 | 1497 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 1498 | `				pToken--;` |
|      ! 0 | 1499 | `			}` |
|      ! 0 | 1500 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|      ! 0 | 1501 | `			if( rc == SXERR_ABORT ){` |
|        - | 1502 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 1503 | `				return SXERR_ABORT;` |
|        - | 1504 | `			}` |
|      ! 0 | 1505 | `			goto Synchronize;` |
|        - | 1506 | `		}` |
|        - | 1507 | `		/* Jump the left parenthesis '(' */` |
|  2701205 | 1508 | `		pToken++;` |
|        - | 1509 | `		/* Delimit the condition */` |
|  2701205 | 1510 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  2701205 | 1511 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|        - | 1512 | `			/* Syntax error */` |
|      ! 0 | 1513 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 1514 | `				pToken--;` |
|      ! 0 | 1515 | `			}` |
|      ! 0 | 1516 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|      ! 0 | 1517 | `			if( rc == SXERR_ABORT ){` |
|        - | 1518 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 1519 | `				return SXERR_ABORT;` |
|        - | 1520 | `			}` |
|      ! 0 | 1521 | `			goto Synchronize;` |
|        - | 1522 | `		}` |
|        - | 1523 | `		/* Swap token streams */` |
|  2701205 | 1524 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|        - | 1525 | `		/* Compile the condition */` |
|  2701205 | 1526 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 1527 | `		/* Update token stream */` |
|  2701205 | 1528 | `		while(pGen->pIn < pEnd ){` |
|      ! 0 | 1529 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 | 1530 | `			pGen->pIn++;` |
|      ! 0 | 1531 | `		}` |
|  2701205 | 1532 | `		pGen->pIn  = &pEnd[1];` |
|  2701205 | 1533 | `		pGen->pEnd = pTmp;` |
|  2701205 | 1534 | `		if( rc == SXERR_ABORT ){` |
|        - | 1535 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|        3 | 1536 | `			return SXERR_ABORT;` |
|        - | 1537 | `		}` |
|        - | 1538 | `		/* Emit the false jump */` |
|  2701203 | 1539 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|        - | 1540 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  2701203 | 1541 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|        - | 1542 | `		/* Compile the body */` |
|  2701203 | 1543 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  2701203 | 1544 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1545 | `			return SXERR_ABORT;` |
|        - | 1546 | `		}` |
|  2701203 | 1547 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   517920 | 1548 | `			break;` |
|        - | 1549 | `		}` |
|        - | 1550 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|  1665373 | 1551 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  1665373 | 1552 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|  1158889 | 1553 | `			break;` |
|        - | 1554 | `		}` |
|        - | 1555 | `		/* Emit the unconditional jump */` |
|   506489 | 1556 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|        - | 1557 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   506489 | 1558 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   506489 | 1559 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   305521 | 1560 | `			pToken = &pGen->pIn[1];` |
|   305521 | 1561 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    85002 | 1562 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|   110281 | 1563 | `					break;` |
|        - | 1564 | `			}` |
|    84969 | 1565 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    42482 | 1566 | `		}` |
|   285937 | 1567 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|        - | 1568 | `		/* Synchronize cursors */` |
|   285937 | 1569 | `		pToken = pGen->pIn;` |
|        - | 1570 | `		/* Fix the false jump */` |
|   285937 | 1571 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|        5 | 1572 | `	} /* For(;;) */` |
|        - | 1573 | `	/* Fix the false jump */` |
|  2415271 | 1574 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  2415271 | 1575 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|  1379436 | 1576 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|        - | 1577 | `			/* Compile the else block */` |
|   220557 | 1578 | `			pGen->pIn++;` |
|   220557 | 1579 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   220557 | 1580 | `			if( rc == SXERR_ABORT ){` |
|        - | 1581 |  |
|      ! 0 | 1582 | `				return SXERR_ABORT;` |
|        - | 1583 | `			}` |
|   110276 | 1584 | `	}` |
|  2415271 | 1585 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|        - | 1586 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  2415271 | 1587 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|        - | 1588 | `	/* Release the conditional block */` |
|  2415271 | 1589 | `	GenStateLeaveBlock(pGen,0);` |
|        - | 1590 | `	/* Statement successfully compiled */` |
|  2415271 | 1591 | `	return SXRET_OK;` |
|      ! 0 | 1592 | `Synchronize:` |
|        - | 1593 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|        - | 1594 | `	 */` |
|      ! 0 | 1595 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 | 1596 | `		pGen->pIn++;` |
|      ! 0 | 1597 | `	}` |
|      ! 0 | 1598 | `	return SXRET_OK;` |
|  1207639 | 1599 | `}` |
|        - | 1600 | `/*` |
|        - | 1601 | ` * Compile the global construct.` |
|        - | 1602 | ` * According to the PHP language reference` |
|        - | 1603 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|        - | 1604 | ` *  to be used in that function.` |
|        - | 1605 | ` *  Example #1 Using global` |
|        - | 1606 | ` *  <?php` |
|        - | 1607 | ` *   $a = 1;` |
|        - | 1608 | ` *   $b = 2;` |
|        - | 1609 | ` *   function Sum()` |
|        - | 1610 | ` *   {` |
|        - | 1611 | ` *    global $a, $b;` |
|        - | 1612 | ` *    $b = $a + $b;` |
|        - | 1613 | ` *   }` |
|        - | 1614 | ` *   Sum();` |
|        - | 1615 | ` *   echo $b;` |
|        - | 1616 | ` *  ?>` |
|        - | 1617 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|        - | 1618 | ` *  all references to either variable will refer to the global version. There is no limit` |
|        - | 1619 | ` *  to the number of global variables that can be manipulated by a function.` |
|        - | 1620 | ` */` |
|       36 | 1621 | `PH7_PRIVATE sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|        5 | 1622 | `{` |
|       41 | 1623 | `	SyToken *pTmp,*pNext = 0;` |
|        - | 1624 | `	sxi32 nExpr;` |
|        - | 1625 | `	sxi32 rc;` |
|        - | 1626 | `	/* Jump the 'global' keyword */` |
|       41 | 1627 | `	pGen->pIn++;` |
|       41 | 1628 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|        - | 1629 | `		/* Nothing to process */` |
|      ! 0 | 1630 | `		return SXRET_OK;` |
|        - | 1631 | `	}` |
|       41 | 1632 | `	pTmp = pGen->pEnd;` |
|       41 | 1633 | `	nExpr = 0;` |
|       87 | 1634 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       51 | 1635 | `		if( pGen->pIn < pNext ){` |
|       51 | 1636 | `			pGen->pEnd = pNext;` |
|       51 | 1637 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 | 1638 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|      ! 0 | 1639 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1640 | `					return SXERR_ABORT;` |
|        - | 1641 | `				}` |
|      ! 0 | 1642 | `			}else{` |
|       51 | 1643 | `				pGen->pIn++;` |
|       51 | 1644 | `				if( pGen->pIn >= pGen->pEnd ){` |
|        - | 1645 | `					/* Emit a warning */` |
|      ! 0 | 1646 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|      ! 0 | 1647 | `				}else{` |
|       51 | 1648 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       51 | 1649 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 1650 | `						return SXERR_ABORT;` |
|       51 | 1651 | `					}else if(rc != SXERR_EMPTY ){` |
|       51 | 1652 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|       51 | 1653 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|        - | 1654 | `							/* Variable name, not a constant */` |
|       51 | 1655 | `							pLast->iP1 = 0;` |
|       23 | 1656 | `						}` |
|       51 | 1657 | `						nExpr++;` |
|       23 | 1658 | `					}` |
|        - | 1659 | `				}` |
|        - | 1660 | `			}` |
|       23 | 1661 | `		}` |
|        - | 1662 | `		/* Next expression in the stream */` |
|       51 | 1663 | `		pGen->pIn = pNext;` |
|        - | 1664 | `		/* Jump trailing commas */` |
|       61 | 1665 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       15 | 1666 | `			pGen->pIn++;` |
|        5 | 1667 | `		}` |
|        5 | 1668 | `	}` |
|        - | 1669 | `	/* Restore token stream */` |
|       41 | 1670 | `	pGen->pEnd = pTmp;` |
|       41 | 1671 | `	if( nExpr > 0 ){` |
|        - | 1672 | `		/* Emit the uplink instruction */` |
|       41 | 1673 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|       18 | 1674 | `	}` |
|       41 | 1675 | `	return SXRET_OK;` |
|       23 | 1676 | `}` |
|        - | 1677 | `/*` |
|        - | 1678 | ` * Compile the return statement.` |
|        - | 1679 | ` * According to the PHP language reference` |
|        - | 1680 | ` *  If called from within a function, the return() statement immediately ends execution` |
|        - | 1681 | ` *  of the current function, and returns its argument as the value of the function call.` |
|        - | 1682 | ` *  return() will also end the execution of an eval() statement or script file.` |
|        - | 1683 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|        - | 1684 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|        - | 1685 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|        - | 1686 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|        - | 1687 | ` *  from within the main script file, then script execution end.` |
|        - | 1688 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|        - | 1689 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|        - | 1690 | ` *  should do so as PHP has less work to do in this case.` |
|        - | 1691 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|        - | 1692 | ` */` |
|  3601866 | 1693 | `PH7_PRIVATE sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|        5 | 1694 | `{` |
|  3601871 | 1695 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|        - | 1696 | `	sxi32 rc;` |
|  3601871 | 1697 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  3601871 | 1698 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|        - | 1699 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|        - | 1700 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|        - | 1701 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|        - | 1702 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|        - | 1703 | `	 * normally below so token processing stays consistent. */` |
|  9483139 | 1704 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|  5881273 | 1705 | `		pFuncBlock = pFuncBlock->pParent;` |
|        5 | 1706 | `	}` |
|  3601866 | 1707 | `	if( pFuncBlock && pFuncBlock->pUserData` |
|  3601855 | 1708 | `	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){` |
|        3 | 1709 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1710 | `			"A never-returning function must not return");` |
|        3 | 1711 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1712 | `			return SXERR_ABORT;` |
|        - | 1713 | `		}` |
|        1 | 1714 | `	}` |
|        - | 1715 | `	/* Jump the 'return' keyword */` |
|  3601871 | 1716 | `	pGen->pIn++;` |
|  3601871 | 1717 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - | 1718 | `		/* Compile the expression */` |
|  3505309 | 1719 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  3505309 | 1720 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1721 | `			return SXERR_ABORT;` |
|  3505309 | 1722 | `		}else if(rc != SXERR_EMPTY ){` |
|  3505309 | 1723 | `			nRet = 1;` |
|  1752652 | 1724 | `		}` |
|  1752652 | 1725 | `	}` |
|        - | 1726 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|        - | 1727 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|        - | 1728 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|        - | 1729 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|  3601871 | 1730 | `	if( pGen->bInGenerator ){` |
|     3895 | 1731 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|     3895 | 1732 | `		return SXRET_OK;` |
|        - | 1733 | `	}` |
|        - | 1734 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|        - | 1735 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|        - | 1736 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|        - | 1737 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|        - | 1738 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|  3597981 | 1739 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|  3597981 | 1740 | `	return SXRET_OK;` |
|  1800938 | 1741 | `}` |
|        - | 1742 | `/*` |
|        - | 1743 | ` * Compile a yield expression.` |
|        - | 1744 | ` * Called from the expression code generator when a yield node is encountered.` |
|        - | 1745 | ` * Handles: yield, yield $value, yield $key => $value` |
|        - | 1746 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|        - | 1747 | ` */` |
|    15836 | 1748 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|        5 | 1749 | `{` |
|        - | 1750 | `	SyToken *pTmp, *pSplit;` |
|    15841 | 1751 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|    15841 | 1752 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|        - | 1753 | `	sxi32 rc;` |
|     7918 | 1754 | `	(void)iCompileFlag;` |
|        - | 1755 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|    15841 | 1756 | `	pGen->pIn++;` |
|        - | 1757 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|        - | 1758 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|        - | 1759 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|        - | 1760 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|        - | 1761 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|    15836 | 1762 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|     7953 | 1763 | `		&& pGen->pIn->sData.nByte == 4` |
|       72 | 1764 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|       67 | 1765 | `		pGen->pIn++; /* Skip 'from' */` |
|       67 | 1766 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       67 | 1767 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1768 | `			return SXERR_ABORT;` |
|        - | 1769 | `		}` |
|       67 | 1770 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 | 1771 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|      ! 0 | 1772 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|        - | 1773 | `				"Missing expression after 'yield from'");` |
|      ! 0 | 1774 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1775 | `				return SXERR_ABORT;` |
|        - | 1776 | `			}` |
|      ! 0 | 1777 | `		}` |
|       67 | 1778 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|       67 | 1779 | `		return SXRET_OK;` |
|        - | 1780 | `	}` |
|    15779 | 1781 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - | 1782 | `		/* Bare yield — no value */` |
|        3 | 1783 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|        3 | 1784 | `		return SXRET_OK;` |
|        - | 1785 | `	}` |
|        - | 1786 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|    15777 | 1787 | `	pSplit = 0;` |
|        - | 1788 | `	{` |
|    15777 | 1789 | `		SyToken *pCur = pGen->pIn;` |
|    15777 | 1790 | `		sxi32 nNest = 0;` |
|    47135 | 1791 | `		while( pCur < pGen->pEnd ){` |
|    46825 | 1792 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       18 | 1793 | `				nNest++;` |
|    46817 | 1794 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       18 | 1795 | `				nNest--;` |
|    46801 | 1796 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|    15467 | 1797 | `				pSplit = pCur;` |
|    15467 | 1798 | `				break;` |
|        - | 1799 | `			}` |
|    31363 | 1800 | `			pCur++;` |
|        5 | 1801 | `		}` |
|        - | 1802 | `	}` |
|    15777 | 1803 | `	pTmp = pGen->pEnd;` |
|    15777 | 1804 | `	if( pSplit ){` |
|        - | 1805 | `		/* yield $key => $value */` |
|    15467 | 1806 | `		pGen->pEnd = pSplit;` |
|    15467 | 1807 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|    15467 | 1808 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|    15467 | 1809 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|    15467 | 1810 | `		pGen->pEnd = pTmp;` |
|    15467 | 1811 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|    15467 | 1812 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|    15467 | 1813 | `		iP1 = 1;` |
|    15467 | 1814 | `		iP2 = 1;` |
|     7736 | 1815 | `	}else{` |
|        - | 1816 | `		/* yield $value */` |
|      315 | 1817 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      315 | 1818 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      315 | 1819 | `		if( rc != SXERR_EMPTY ){` |
|      315 | 1820 | `			iP1 = 1;` |
|      155 | 1821 | `		}` |
|        - | 1822 | `	}` |
|    15777 | 1823 | `	pGen->pEnd = pTmp;` |
|    15777 | 1824 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|    15777 | 1825 | `	return SXRET_OK;` |
|     7923 | 1826 | `}` |
|        - | 1827 | `/*` |
|        - | 1828 | ` * Compile the die/exit language construct.` |
|        - | 1829 | ` * The role of these constructs is to terminate execution of the script.` |
|        - | 1830 | ` * Shutdown functions will always be executed even if exit() is called.` |
|        - | 1831 | ` */` |
|       94 | 1832 | `PH7_PRIVATE sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|        5 | 1833 | `{` |
|       99 | 1834 | `	sxi32 nExpr = 0;` |
|        - | 1835 | `	sxi32 rc;` |
|        - | 1836 | `	/* Jump the die/exit keyword */` |
|       99 | 1837 | `	pGen->pIn++;` |
|       99 | 1838 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - | 1839 | `		/* Compile the expression */` |
|       99 | 1840 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       99 | 1841 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1842 | `			return SXERR_ABORT;` |
|       99 | 1843 | `		}else if(rc != SXERR_EMPTY ){` |
|       99 | 1844 | `			nExpr = 1;` |
|       47 | 1845 | `		}` |
|       47 | 1846 | `	}` |
|        - | 1847 | `	/* Emit the HALT instruction */` |
|       99 | 1848 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|       99 | 1849 | `	return SXRET_OK;` |
|       52 | 1850 | `}` |
|        - | 1851 | `/*` |
|        - | 1852 | ` * Compile the 'echo' language construct.` |
|        - | 1853 | ` */` |
|    17426 | 1854 | `PH7_PRIVATE sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|        5 | 1855 | `{` |
|    17431 | 1856 | `	SyToken *pTmp,*pNext = 0;` |
|    17431 | 1857 | `	sxu32 nLine = pGen->pIn->nLine;` |
|    17431 | 1858 | `	int nExpr = 0;      /* expressions actually compiled */` |
|    17431 | 1859 | `	int bExpectMore = 1;/* after 'echo' or a comma an expression is REQUIRED */` |
|        - | 1860 | `	sxi32 rc;` |
|        - | 1861 | `	/* Jump the 'echo' keyword */` |
|    17431 | 1862 | `	pGen->pIn++;` |
|        - | 1863 | `	/* Compile arguments one after one */` |
|    17431 | 1864 | `	pTmp = pGen->pEnd;` |
|    45293 | 1865 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|    27873 | 1866 | `		if( pGen->pIn < pNext ){` |
|    27873 | 1867 | `			pGen->pEnd = pNext;` |
|    27873 | 1868 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|    27873 | 1869 | `			if( rc == SXERR_ABORT ){` |
|        6 | 1870 | `				return SXERR_ABORT;` |
|    27869 | 1871 | `			}else if( rc != SXERR_EMPTY ){` |
|        - | 1872 | `				/* Emit the consume instruction */` |
|    27845 | 1873 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    27845 | 1874 | `				nExpr++;` |
|    27845 | 1875 | `				bExpectMore = 0;` |
|    13920 | 1876 | `			}` |
|    13932 | 1877 | `		}` |
|        - | 1878 | `		/* Jump trailing commas (php: exactly one between expressions; a` |
|        - | 1879 | `		 * dangling or doubled comma is a parse error, enforced below) */` |
|    38317 | 1880 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|    10455 | 1881 | `			if( bExpectMore ){` |
|        - | 1882 | `				/* two commas in a row */` |
|        3 | 1883 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,pNext->nLine,` |
|        - | 1884 | `					"syntax error, unexpected token \",\"");` |
|        3 | 1885 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1886 | `			}` |
|    10453 | 1887 | `			bExpectMore = 1;` |
|    10453 | 1888 | `			pNext++;` |
|        5 | 1889 | `		}` |
|    27867 | 1890 | `		pGen->pIn = pNext;` |
|        5 | 1891 | `	}` |
|        - | 1892 | `	/* Restore token stream */` |
|    17425 | 1893 | `	pGen->pEnd = pTmp;` |
|    17425 | 1894 | `	if( nExpr == 0 \|\| bExpectMore ){` |
|        - | 1895 | ``		/* `echo ;` or `echo expr, ;` — php rejects both */`` |
|       32 | 1896 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - | 1897 | `			"syntax error, unexpected token \";\"");` |
|       32 | 1898 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1899 | `	}` |
|    17397 | 1900 | `	return SXRET_OK;` |
|     8718 | 1901 | `}` |
|        - | 1902 | `/*` |
|        - | 1903 | ` * Compile the static statement.` |
|        - | 1904 | ` * According to the PHP language reference` |
|        - | 1905 | ` *  Another important feature of variable scoping is the static variable.` |
|        - | 1906 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|        - | 1907 | ` *  when program execution leaves this scope.` |
|        - | 1908 | ` *  Static variables also provide one way to deal with recursive functions.` |
|        - | 1909 | ` * Symisc eXtension.` |
|        - | 1910 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|        - | 1911 | ` *  the zend engine would allow only simple scalar value.` |
|        - | 1912 | ` *  Example` |
|        - | 1913 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|        - | 1914 | ` *    Refer to the official documentation for more information on this feature.` |
|        - | 1915 | ` */` |
|    11598 | 1916 | `PH7_PRIVATE sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|        5 | 1917 | `{` |
|        - | 1918 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|        - | 1919 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|        - | 1920 | `	GenBlock *pBlock;` |
|        - | 1921 | `	SyString *pName;` |
|        - | 1922 | `	char *zDup;` |
|        - | 1923 | `	sxu32 nLine;` |
|        - | 1924 | `	sxi32 rc;` |
|        - | 1925 | ``	/* `static function () {}` / `static fn () =>` at statement position is an`` |
|        - | 1926 | `	 * EXPRESSION statement (a bare static closure), not a static-variable` |
|        - | 1927 | `	 * declaration — hand it to the expression compiler (php accepts it). */` |
|    11598 | 1928 | `	if( &pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & PH7_TK_KEYWORD)` |
|     5805 | 1929 | `	 && (SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FUNCTION` |
|        1 | 1930 | `	  \|\| SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FN) ){` |
|        3 | 1931 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        3 | 1932 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1933 | `			return SXERR_ABORT;` |
|        3 | 1934 | `		}else if( rc != SXERR_EMPTY ){` |
|        3 | 1935 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        1 | 1936 | `		}` |
|        3 | 1937 | `		return SXRET_OK;` |
|        - | 1938 | `	}` |
|        - | 1939 | `	/* Jump the static keyword */` |
|    11601 | 1940 | `	nLine = pGen->pIn->nLine;` |
|    11601 | 1941 | `	pGen->pIn++;` |
|        - | 1942 | `	/* Extract the enclosing function if any */` |
|    11601 | 1943 | `	pBlock = pGen->pCurrent;` |
|    23197 | 1944 | `	while( pBlock ){` |
|    23197 | 1945 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|    11601 | 1946 | `			break;` |
|        - | 1947 | `		}` |
|        - | 1948 | `		/* Point to the upper block */` |
|    11601 | 1949 | `		pBlock = pBlock->pParent;` |
|        5 | 1950 | `	}` |
|    11601 | 1951 | `	if( pBlock == 0 ){` |
|        - | 1952 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|      ! 0 | 1953 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|        - | 1954 | ``			/* php: `static FOO;` is a syntax error naming FOO and expecting "::"`` |
|        - | 1955 | ``			 * (the parser is still open to `static::` at that point). */`` |
|      ! 0 | 1956 | `			rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"::\"");` |
|      ! 0 | 1957 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1958 | `				return SXERR_ABORT;` |
|        - | 1959 | `			}` |
|      ! 0 | 1960 | `			goto Synchronize;` |
|        - | 1961 | `		}` |
|        - | 1962 | `		/* Compile the expression holding the variable */` |
|      ! 0 | 1963 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      ! 0 | 1964 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1965 | `			return SXERR_ABORT;` |
|      ! 0 | 1966 | `		}else if( rc != SXERR_EMPTY ){` |
|        - | 1967 | `			/* Emit the POP instruction */` |
|      ! 0 | 1968 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      ! 0 | 1969 | `		}` |
|      ! 0 | 1970 | `		return SXRET_OK;` |
|        - | 1971 | `	}` |
|    11601 | 1972 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        - | 1973 | `	/* Make sure we are dealing with a valid statement */` |
|    11601 | 1974 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|    11594 | 1975 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - | 1976 | ``			/* php: `static FOO;` is a syntax error naming FOO and expecting "::"`` |
|        - | 1977 | ``			 * (the parser is still open to `static::` at that point). */`` |
|        3 | 1978 | `			rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"::\"");` |
|        3 | 1979 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1980 | `				return SXERR_ABORT;` |
|        - | 1981 | `			}` |
|        3 | 1982 | `			goto Synchronize;` |
|        - | 1983 | `	}` |
|    11599 | 1984 | `	pGen->pIn++;` |
|        - | 1985 | `	/* Extract variable name */` |
|    11599 | 1986 | `	pName = &pGen->pIn->sData;` |
|    11599 | 1987 | `	pGen->pIn++; /* Jump the var name */` |
|    11599 | 1988 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|      ! 0 | 1989 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 1990 | `		goto Synchronize;` |
|        - | 1991 | `	}` |
|        - | 1992 | `	/* Initialize the structure describing the static variable */` |
|    11599 | 1993 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|    11599 | 1994 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|        - | 1995 | `	/* Duplicate variable name */` |
|    11599 | 1996 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    11599 | 1997 | `	if( zDup == 0 ){` |
|      ! 0 | 1998 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1999 | `		return SXERR_ABORT;` |
|        - | 2000 | `	}` |
|    11599 | 2001 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|        - | 2002 | `	/* Check if we have an expression to compile */` |
|    11599 | 2003 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|        - | 2004 | `		SySet *pInstrContainer;` |
|        - | 2005 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|        - | 2006 | `		 * Static variable can take any complex expression including function` |
|        - | 2007 | `		 * call as their initialization value.` |
|        - | 2008 | `		 * Example:` |
|        - | 2009 | `		 *		static $var = foo(1,4+5,bar());` |
|        - | 2010 | `		 */` |
|    11599 | 2011 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|        - | 2012 | `		/* Swap bytecode container */` |
|    11599 | 2013 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    11599 | 2014 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|        - | 2015 | `		/* Compile the expression */` |
|    11599 | 2016 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 2017 | `		/* Emit the done instruction */` |
|    11599 | 2018 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        - | 2019 | `		/* Restore default bytecode container */` |
|    11599 | 2020 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     5797 | 2021 | `	}` |
|        - | 2022 | `	/* Finally save the compiled static variable in the appropriate container */` |
|    11599 | 2023 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|    11599 | 2024 | `	return SXRET_OK;` |
|        1 | 2025 | `Synchronize:` |
|        - | 2026 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|        - | 2027 | `	 * statement.` |
|        - | 2028 | `	 */` |
|        5 | 2029 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|        3 | 2030 | `		pGen->pIn++;` |
|        1 | 2031 | `	}` |
|        3 | 2032 | `	return SXRET_OK;` |
|     5804 | 2033 | `}` |
|        - | 2034 | `/*` |
|        - | 2035 | ` * Compile the var statement.` |
|        - | 2036 | ` * Symisc Extension:` |
|        - | 2037 | ` *      var statement can be used outside of a class definition.` |
|        - | 2038 | ` */` |
|        2 | 2039 | `PH7_PRIVATE sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|        1 | 2040 | `{` |
|        - | 2041 | `` 	/* `var` is a PROPERTY modifier and nothing else. A class body's `var $x;` `` |
|        - | 2042 | `	 * is compiled by compile_class.c, never here, so reaching this statement` |
|        - | 2043 | ``	 * handler means `var` appeared outside a class — which php rejects as a`` |
|        - | 2044 | `	 * parse error. PHL used to compile it as an ordinary expression statement,` |
|        - | 2045 | ``	 * so top-level `var $x = 1;` silently ran (a Symisc extension). */`` |
|        3 | 2046 | `	PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|        3 | 2047 | `	return SXERR_ABORT;` |
|        1 | 2048 | `}` |
|        - | 2049 | `/*` |
|        - | 2050 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|        - | 2051 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|        - | 2052 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|        - | 2053 | ` */` |
|        - | 2054 | `/*` |
|        - | 2055 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|        - | 2056 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|        - | 2057 | ` * hash and any shared references), this creates a new literal entry with the` |
|        - | 2058 | ` * qualified name and updates the instruction's operand index.` |
|        - | 2059 | ` *` |
|        - | 2060 | ` * Resolution order:` |
|        - | 2061 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|        - | 2062 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|        - | 2063 | ` *   3. Otherwise return the original literal index unchanged.` |
|        - | 2064 | ` *` |
|        - | 2065 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|        - | 2066 | ` * came from an import (step 1) and 0 otherwise.` |
|        - | 2067 | ` * Returns the (possibly new) literal index.` |
|        - | 2068 | ` */` |
|  6402344 | 2069 | `PH7_PRIVATE sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|        5 | 2070 | `{` |
|        - | 2071 | `	ph7_value *pLit;` |
|        - | 2072 | `	const char *zLit;` |
|        - | 2073 | `	SyString sQualified;` |
|        - | 2074 | `	sxu32 nLit;` |
|        - | 2075 | `	sxu32 k;` |
|        - | 2076 | `	sxu32 nNewIdx;` |
|        - | 2077 | `	int hasNsSep;` |
|        - | 2078 | `	SyHashEntry *pImport;` |
|        - | 2079 | `	ph7_value *pNew;` |
|  6402349 | 2080 | `	if( pFromImport ){` |
|  5220183 | 2081 | `		*pFromImport = 0;` |
|  2610089 | 2082 | `	}` |
|  6402349 | 2083 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  6402349 | 2084 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|      ! 0 | 2085 | `		return nOrigIdx;` |
|        - | 2086 | `	}` |
|  6402349 | 2087 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  6402349 | 2088 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|        - | 2089 | `	/* Skip if already qualified (contains backslash) */` |
|  6402349 | 2090 | `	hasNsSep = 0;` |
| 77415637 | 2091 | `	for( k = 0; k < nLit; k++ ){` |
| 71013319 | 2092 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 35506649 | 2093 | `	}` |
|  6402349 | 2094 | `	if( hasNsSep ){` |
|       28 | 2095 | `		return nOrigIdx;` |
|        - | 2096 | `	}` |
|        - | 2097 | `	/* Check use imports first (works even outside namespaces) */` |
|  6402323 | 2098 | `	SyBlobReset(&pGen->sWorker);` |
|  6402323 | 2099 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  6402323 | 2100 | `	if( pImport ){` |
|       41 | 2101 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       41 | 2102 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|       41 | 2103 | `		if( pFromImport ){` |
|       18 | 2104 | `			*pFromImport = 1;` |
|        8 | 2105 | `		}` |
|       23 | 2106 | `	}else{` |
|  6402287 | 2107 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  6402149 | 2108 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|        - | 2109 | `		}` |
|        - | 2110 | `		/* Prepend current namespace */` |
|      143 | 2111 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      143 | 2112 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      143 | 2113 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|        - | 2114 | `	}` |
|        - | 2115 | `	/* Look up or create a new literal for the qualified name */` |
|      179 | 2116 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|      179 | 2117 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|       79 | 2118 | `		return nNewIdx; /* Already interned */` |
|        - | 2119 | `	}` |
|      105 | 2120 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      105 | 2121 | `	if( pNew == 0 ){` |
|      ! 0 | 2122 | `		return nOrigIdx; /* OOM, fall back to original */` |
|        - | 2123 | `	}` |
|      105 | 2124 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      105 | 2125 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      105 | 2126 | `	return nNewIdx;` |
|  3201177 | 2127 | `}` |
|        - | 2128 | `/*` |
|        - | 2129 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|        - | 2130 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|        - | 2131 | ` */` |
|   550344 | 2132 | `PH7_PRIVATE void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|        5 | 2133 | `{` |
|        - | 2134 | `	SyHashEntry *pImport;` |
|   550349 | 2135 | `	const char *zName = pName->zString;` |
|   550349 | 2136 | `	sxu32 nName = pName->nByte;` |
|   550349 | 2137 | `	sxu32 nFirst = 0;` |
|        - | 2138 | `	/* php resolves a name through use-imports on its LEADING segment: an` |
|        - | 2139 | ``	 * unqualified `C` maps the whole name (`use X\C;` -> X\C), while a QUALIFIED`` |
|        - | 2140 | ``	 * `A\B\C` maps just `A` (`use X\A;` -> X\A\B\C) and keeps the `\B\C` tail.`` |
|        - | 2141 | `	 * The old code looked up the whole qualified string (which never matches a` |
|        - | 2142 | `	 * single-segment import alias) and then blindly prefixed the current` |
|        - | 2143 | ``	 * namespace, so `use PHPUnit\Framework; extends Framework\TestCase` resolved`` |
|        - | 2144 | `	 * to Ns\Framework\TestCase. Mirror GenStateResolveNamespaceLiteral here. */` |
|  6998375 | 2145 | `	while( nFirst < nName && zName[nFirst] != '\\' ){ nFirst++; }` |
|   550349 | 2146 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)zName,nFirst);` |
|   550349 | 2147 | `	if( pImport ){` |
|       26 | 2148 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       26 | 2149 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|       26 | 2150 | `		SyBlobAppend(pOut,zName + nFirst,nName - nFirst); /* the \B\C tail, if any */` |
|       26 | 2151 | `		return;` |
|        - | 2152 | `	}` |
|        - | 2153 | `	/* Prepend current namespace if active */` |
|   550327 | 2154 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       14 | 2155 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       14 | 2156 | `		SyBlobAppend(pOut,"\\",1);` |
|        6 | 2157 | `	}` |
|   550327 | 2158 | `	SyBlobAppend(pOut,zName,nName);` |
|   275177 | 2159 | `}` |
|        - | 2160 | `/*` |
|        - | 2161 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|        - | 2162 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|        - | 2163 | ` * The caller must release pOut when done.` |
|        - | 2164 | ` */` |
|   527470 | 2165 | `PH7_PRIVATE void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|        5 | 2166 | `{` |
|   527475 | 2167 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     3955 | 2168 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     3955 | 2169 | `		SyBlobAppend(pOut,"\\",1);` |
|     1975 | 2170 | `	}` |
|   527475 | 2171 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   527475 | 2172 | `}` |
|        - | 2173 | `/*` |
|        - | 2174 | ` * Compile a namespace statement` |
|        - | 2175 | ` * According to the PHP language reference manual` |
|        - | 2176 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|        - | 2177 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|        - | 2178 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|        - | 2179 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|        - | 2180 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|        - | 2181 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|        - | 2182 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|        - | 2183 | ` *  programming world.` |
|        - | 2184 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|        - | 2185 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|        - | 2186 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|        - | 2187 | ` *  classes/functions/constants.` |
|        - | 2188 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|        - | 2189 | ` *  readability of source code.` |
|        - | 2190 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|        - | 2191 | ` *  Here is an example of namespace syntax in PHP:` |
|        - | 2192 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|        - | 2193 | ` *       class MyClass {}` |
|        - | 2194 | ` *       function myfunction() {}` |
|        - | 2195 | ` *       const MYCONST = 1;` |
|        - | 2196 | ` *       $a = new MyClass;` |
|        - | 2197 | ` *       $c = new \my\name\MyClass;` |
|        - | 2198 | ` *       $a = strlen('hi');` |
|        - | 2199 | ` *       $d = namespace\MYCONST;` |
|        - | 2200 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|        - | 2201 | ` *       echo constant($d);` |
|        - | 2202 | ` * NOTE` |
|        - | 2203 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|        - | 2204 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|        - | 2205 | ` */` |
|        - | 2206 | `/*` |
|        - | 2207 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|        - | 2208 | ` */` |
|       14 | 2209 | `PH7_PRIVATE const char * TokenTypeName(sxu32 nType)` |
|        4 | 2210 | `{` |
|       18 | 2211 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|       11 | 2212 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|       11 | 2213 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|       11 | 2214 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|       11 | 2215 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|       11 | 2216 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|        3 | 2217 | `	return "token";` |
|       11 | 2218 | `}` |
|     3994 | 2219 | `PH7_PRIVATE sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|        5 | 2220 | `{` |
|        - | 2221 | `	sxu32 nLine;` |
|        - | 2222 | `	sxi32 rc;` |
|     3999 | 2223 | `	nLine = pGen->pIn->nLine;` |
|     3999 | 2224 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|        - | 2225 | `	/* Reset namespace and clear previous use imports */` |
|     3999 | 2226 | `	SyBlobReset(&pGen->sNamespace);` |
|     3999 | 2227 | `	SyHashRelease(&pGen->hUseImports);` |
|     3999 | 2228 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|     3999 | 2229 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     3999 | 2230 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|     3999 | 2231 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     3999 | 2232 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|     3999 | 2233 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - | 2234 | `		/* Global namespace (bare "namespace;") */` |
|      ! 0 | 2235 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|      ! 0 | 2236 | `		return SXRET_OK;` |
|        - | 2237 | `	}` |
|     3999 | 2238 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|        - | 2239 | `		/* namespace; — switch to global namespace */` |
|      ! 0 | 2240 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|      ! 0 | 2241 | `		return SXRET_OK;` |
|        - | 2242 | `	}` |
|     3999 | 2243 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|        - | 2244 | `		/* namespace { } — global namespace block */` |
|        5 | 2245 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|        5 | 2246 | `		return SXRET_OK;` |
|        - | 2247 | `	}` |
|        - | 2248 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     8067 | 2249 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     4077 | 2250 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|        - | 2251 | `			/* Append backslash separator */` |
|       46 | 2252 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       46 | 2253 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|       21 | 2254 | `			}` |
|       25 | 2255 | `		}else{` |
|        - | 2256 | `			/* Append identifier */` |
|     4035 | 2257 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        - | 2258 | `		}` |
|     4077 | 2259 | `		pGen->pIn++;` |
|        5 | 2260 | `	}` |
|        - | 2261 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|        - | 2262 | `	 * at the correct program counter, not just the last one compiled. */` |
|        - | 2263 | `	{` |
|     3995 | 2264 | `		char *zNsDup = 0;` |
|     3995 | 2265 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     5987 | 2266 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     3988 | 2267 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     1994 | 2268 | `		}` |
|     3995 | 2269 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|        - | 2270 | `	}` |
|     3995 | 2271 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        8 | 2272 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - | 2273 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|        4 | 2274 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        6 | 2275 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2276 | `			return SXERR_ABORT;` |
|        - | 2277 | `		}` |
|        2 | 2278 | `	}` |
|     3995 | 2279 | `	return SXRET_OK;` |
|     2002 | 2280 | `}` |
|        - | 2281 | `/*` |
|        - | 2282 | ` * Compile the 'use' statement` |
|        - | 2283 | ` * According to the PHP language reference manual` |
|        - | 2284 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|        - | 2285 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|        - | 2286 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|        - | 2287 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|        - | 2288 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|        - | 2289 | ` *  a function or constant is not supported.` |
|        - | 2290 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|        - | 2291 | ` * NOTE` |
|        - | 2292 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|        - | 2293 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|        - | 2294 | ` */` |
|       80 | 2295 | `PH7_PRIVATE sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|        5 | 2296 | `{` |
|        - | 2297 | `	sxu32 nLine;` |
|        - | 2298 | `	sxi32 rc;` |
|        - | 2299 | `	SyBlob sPath;` |
|        - | 2300 | `	SyString sAlias;` |
|        - | 2301 | `	SyToken *pLast;` |
|        - | 2302 | `	char *zDup;` |
|        - | 2303 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|        - | 2304 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|        - | 2305 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|       85 | 2306 | `	nLine = pGen->pIn->nLine;` |
|       85 | 2307 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|        - | 2308 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|       85 | 2309 | `	iUseType = 0;` |
|       85 | 2310 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       30 | 2311 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|       30 | 2312 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|       16 | 2313 | `			iUseType = 1;` |
|       16 | 2314 | `			pGen->pIn++;` |
|       23 | 2315 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|       16 | 2316 | `			iUseType = 2;` |
|       16 | 2317 | `			pGen->pIn++;` |
|        7 | 2318 | `		}` |
|       14 | 2319 | `	}` |
|        - | 2320 | `	/* Select target hash tables based on import type */` |
|       85 | 2321 | `	switch( iUseType ){` |
|        7 | 2322 | `		case 1:` |
|       16 | 2323 | `			pGenHash = &pGen->hUseFuncImports;` |
|       16 | 2324 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|       16 | 2325 | `			break;` |
|        7 | 2326 | `		case 2:` |
|       16 | 2327 | `			pGenHash = &pGen->hUseConstImports;` |
|       16 | 2328 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|       16 | 2329 | `			break;` |
|       26 | 2330 | `		default:` |
|       57 | 2331 | `			pGenHash = &pGen->hUseImports;` |
|       57 | 2332 | `			pVmHash = &pGen->pVm->hUseImports;` |
|       52 | 2333 | `			break;` |
|        - | 2334 | `	}` |
|       85 | 2335 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|        - | 2336 | `	/* Process one or more use declarations separated by commas */` |
|       41 | 2337 | `	for(;;){` |
|       87 | 2338 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 2339 | `			break;` |
|        - | 2340 | `		}` |
|       87 | 2341 | `		SyBlobReset(&sPath);` |
|       87 | 2342 | `		pLast = 0;` |
|        - | 2343 | `		/* Collect the full namespace path */` |
|      301 | 2344 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|      219 | 2345 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|      151 | 2346 | `				pLast = pGen->pIn;` |
|      151 | 2347 | `				if( SyBlobLength(&sPath) > 0 ){` |
|       73 | 2348 | `					SyBlobAppend(&sPath,"\\",1);` |
|       34 | 2349 | `				}` |
|      151 | 2350 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       73 | 2351 | `			}` |
|      219 | 2352 | `			pGen->pIn++;` |
|        5 | 2353 | `		}` |
|       87 | 2354 | `		if( pLast == 0 ){` |
|        - | 2355 | `			/* Empty path */` |
|        6 | 2356 | `			break;` |
|        - | 2357 | `		}` |
|        - | 2358 | `		/* Default alias is the last component of the path */` |
|       83 | 2359 | `		sAlias = pLast->sData;` |
|        - | 2360 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|       78 | 2361 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       56 | 2362 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|       27 | 2363 | `			pGen->pIn++; /* Jump 'as' */` |
|       27 | 2364 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|       27 | 2365 | `				sAlias = pGen->pIn->sData;` |
|       27 | 2366 | `				pGen->pIn++;` |
|       12 | 2367 | `			}` |
|       12 | 2368 | `		}` |
|        - | 2369 | `		/* Check for duplicate import alias (per-type) */` |
|       83 | 2370 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|        8 | 2371 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 2372 | `				"Cannot use %.*s as %z because the name is already in use",` |
|        4 | 2373 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|        6 | 2374 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2375 | `				SyBlobRelease(&sPath);` |
|      ! 0 | 2376 | `				return SXERR_ABORT;` |
|        - | 2377 | `			}` |
|        2 | 2378 | `		}` |
|        - | 2379 | `		/* Register the import: alias -> FQN.` |
|        - | 2380 | `		 * Strings are allocated from the VM pool allocator and freed` |
|        - | 2381 | `		 * when the entire VM is released. SyHashRelease does not free` |
|        - | 2382 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|      122 | 2383 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       78 | 2384 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|       83 | 2385 | `		if( zDup ){` |
|       83 | 2386 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|       83 | 2387 | `			if( pVmHash ){` |
|        - | 2388 | `				/* Class imports: populate VM table directly (class resolution` |
|        - | 2389 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|       55 | 2390 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       55 | 2391 | `				if( zAliasDup ){` |
|       55 | 2392 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|       25 | 2393 | `				}` |
|       25 | 2394 | `			}` |
|       83 | 2395 | `			if( iUseType == 2 ){` |
|        - | 2396 | `				/* Const imports: emit a runtime instruction so imports are` |
|        - | 2397 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|       16 | 2398 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       16 | 2399 | `				if( zAliasDup ){` |
|        - | 2400 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|        - | 2401 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|        - | 2402 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|       16 | 2403 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|       16 | 2404 | `					if( azPair ){` |
|       16 | 2405 | `						azPair[0] = zAliasDup;` |
|       16 | 2406 | `						azPair[1] = zDup;` |
|       16 | 2407 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|        7 | 2408 | `					}` |
|        7 | 2409 | `				}` |
|        7 | 2410 | `			}` |
|       39 | 2411 | `		}` |
|        - | 2412 | `		/* Check for comma (multiple use declarations) */` |
|       83 | 2413 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|        3 | 2414 | `			pGen->pIn++;` |
|        2 | 2415 | `		}else{` |
|       43 | 2416 | `			break;` |
|        - | 2417 | `		}` |
|        1 | 2418 | `	}` |
|       85 | 2419 | `	SyBlobRelease(&sPath);` |
|       85 | 2420 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        4 | 2421 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|        2 | 2422 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        3 | 2423 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2424 | `			return SXERR_ABORT;` |
|        - | 2425 | `		}` |
|        1 | 2426 | `	}` |
|       85 | 2427 | `	return SXRET_OK;` |
|       45 | 2428 | `}` |
|        - | 2429 | `/*` |
|        - | 2430 | ` * Compile the stupid 'declare' language construct.` |
|        - | 2431 | ` *` |
|        - | 2432 | ` * According to the PHP language reference manual.` |
|        - | 2433 | ` *  The declare construct is used to set execution directives for a block of code.` |
|        - | 2434 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|        - | 2435 | ` *  declare (directive)` |
|        - | 2436 | ` *   statement` |
|        - | 2437 | ` * The directive section allows the behavior of the declare block to be set.` |
|        - | 2438 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|        - | 2439 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|        - | 2440 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|        - | 2441 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|        - | 2442 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|        - | 2443 | ` * <?php` |
|        - | 2444 | ` * // these are the same:` |
|        - | 2445 | ` * // you can use this:` |
|        - | 2446 | ` * declare(ticks=1) {` |
|        - | 2447 | ` *   // entire script here` |
|        - | 2448 | ` * }` |
|        - | 2449 | ` * // or you can use this:` |
|        - | 2450 | ` * declare(ticks=1);` |
|        - | 2451 | ` * // entire script here` |
|        - | 2452 | ` * ?>` |
|        - | 2453 | ` *` |
|        - | 2454 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|        - | 2455 | ` */` |
|        - | 2456 | `/*` |
|        - | 2457 | ` * Match a directive name against a known literal (case-insensitive).` |
|        - | 2458 | ` */` |
|       80 | 2459 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|        5 | 2460 | `{` |
|      120 | 2461 | `	return SyStringLength(pName) == nWant` |
|       80 | 2462 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|        5 | 2463 | `}` |
|        - | 2464 |  |
|       44 | 2465 | `PH7_PRIVATE sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|        5 | 2466 | `{` |
|       49 | 2467 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       49 | 2468 | `	SyToken *pBodyEnd = 0;` |
|        - | 2469 | `	SyToken *pBodyStart;` |
|        - | 2470 | `	SyToken *pCursor;` |
|        - | 2471 | `	int bHasStrictTypes;` |
|        - | 2472 | `	int bBlockForm;` |
|        - | 2473 | `	int bPlacementOk;` |
|        - | 2474 | `	sxi32 rc;` |
|       49 | 2475 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|       49 | 2476 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|        6 | 2477 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|        6 | 2478 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2479 | `			return SXERR_ABORT;` |
|        - | 2480 | `		}` |
|        6 | 2481 | `		goto Synchro;` |
|        - | 2482 | `	}` |
|       45 | 2483 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|       45 | 2484 | `	pBodyStart = pGen->pIn;` |
|        - | 2485 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|       45 | 2486 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|       45 | 2487 | `	if( pBodyEnd >= pGen->pEnd ){` |
|      ! 0 | 2488 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\")\"");` |
|      ! 0 | 2489 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2490 | `			return SXERR_ABORT;` |
|        - | 2491 | `		}` |
|      ! 0 | 2492 | `		return SXRET_OK;` |
|        - | 2493 | `	}` |
|        - | 2494 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|        - | 2495 | `	 * now delimits the comma-separated directive list. */` |
|       45 | 2496 | `	pGen->pIn = &pBodyEnd[1];` |
|       45 | 2497 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      ! 0 | 2498 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|      ! 0 | 2499 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2500 | `			return SXERR_ABORT;` |
|        - | 2501 | `		}` |
|      ! 0 | 2502 | `	}` |
|       45 | 2503 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|       45 | 2504 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|       45 | 2505 | `	bHasStrictTypes = 0;` |
|        - | 2506 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|        - | 2507 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|        - | 2508 | `	 * directive appears anywhere in the list, before validating values. */` |
|       45 | 2509 | `	pCursor = pBodyStart;` |
|       57 | 2510 | `	while( pCursor < pBodyEnd ){` |
|       53 | 2511 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|       45 | 2512 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|       41 | 2513 | `				bHasStrictTypes = 1;` |
|       41 | 2514 | `				break;` |
|        - | 2515 | `			}` |
|        2 | 2516 | `		}` |
|       14 | 2517 | `		pCursor++;` |
|        2 | 2518 | `	}` |
|       45 | 2519 | `	if( bHasStrictTypes && bBlockForm ){` |
|        3 | 2520 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2521 | `			"strict_types declaration must not use block mode");` |
|        3 | 2522 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        3 | 2523 | `		return SXRET_OK;` |
|        - | 2524 | `	}` |
|       43 | 2525 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|        6 | 2526 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2527 | `			"strict_types declaration must be the very first statement in the script");` |
|        6 | 2528 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        6 | 2529 | `		return SXRET_OK;` |
|        - | 2530 | `	}` |
|        - | 2531 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|       39 | 2532 | `	pCursor = pBodyStart;` |
|       73 | 2533 | `	while( pCursor < pBodyEnd ){` |
|        - | 2534 | `		SyToken *pNameTok;` |
|        - | 2535 | `		SyToken *pEqTok;` |
|        - | 2536 | `		SyToken *pValTok;` |
|        - | 2537 | `		SyString *pDirName;` |
|        - | 2538 | `		int bIsStrict;` |
|        - | 2539 | `		int iStrictValue;` |
|       41 | 2540 | `		pNameTok = pCursor;` |
|       41 | 2541 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 2542 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2543 | `				"declare: Expecting a directive name");` |
|      ! 0 | 2544 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2545 | `			return SXRET_OK;` |
|        - | 2546 | `		}` |
|       41 | 2547 | `		pEqTok = pNameTok + 1;` |
|       41 | 2548 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|      ! 0 | 2549 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2550 | `				"declare: Expecting '=' after directive name");` |
|      ! 0 | 2551 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2552 | `			return SXRET_OK;` |
|        - | 2553 | `		}` |
|       41 | 2554 | `		pValTok = pEqTok + 1;` |
|       41 | 2555 | `		if( pValTok >= pBodyEnd ){` |
|      ! 0 | 2556 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2557 | `				"declare: Expecting value after '='");` |
|      ! 0 | 2558 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2559 | `			return SXRET_OK;` |
|        - | 2560 | `		}` |
|       41 | 2561 | `		pDirName = &pNameTok->sData;` |
|       41 | 2562 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|       41 | 2563 | `		if( bIsStrict ){` |
|        - | 2564 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|        - | 2565 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|       37 | 2566 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      ! 0 | 2567 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2568 | `					"declare(strict_types) value must be a literal");` |
|      ! 0 | 2569 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2570 | `				return SXRET_OK;` |
|        - | 2571 | `			}` |
|       37 | 2572 | `			iStrictValue = -1;` |
|       37 | 2573 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|       37 | 2574 | `				const char *zv = SyStringData(&pValTok->sData);` |
|       37 | 2575 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|       37 | 2576 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|       35 | 2577 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|       16 | 2578 | `			}` |
|       37 | 2579 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|        3 | 2580 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2581 | `					"strict_types declaration must have 0 or 1 as its value");` |
|        3 | 2582 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        3 | 2583 | `				return SXRET_OK;` |
|        - | 2584 | `			}` |
|       35 | 2585 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|       21 | 2586 | `		}else if( DeclareNameIs(pDirName, "encoding", sizeof("encoding")-1) ){` |
|        - | 2587 | `			/* php always ignores declare(encoding=...) unless it was built with` |
|        - | 2588 | `			 * Zend multibyte, and says so in these exact words. */` |
|        3 | 2589 | `			PH7_GenCompileError(&(*pGen),E_WARNING,nLine,` |
|        - | 2590 | `				"declare(encoding=...) ignored because Zend multibyte feature is turned off by settings");` |
|        1 | 2591 | `		}else{` |
|        - | 2592 | `			/* Other directives (ticks and friends) are accepted as no-ops.` |
|        - | 2593 | `			 * This used to emit a NOTICE naming the upstream engine and its` |
|        - | 2594 | `			 * version ("the declare construct is a no-op in the current release` |
|        - | 2595 | `			 * of the PH7(2.1.4) engine") — php prints nothing at all for` |
|        - | 2596 | ``			 * `declare(ticks=1)`, and leaking the old engine's branding into`` |
|        - | 2597 | `			 * user-visible diagnostics was wrong on its own. */` |
|        - | 2598 | `		}` |
|       39 | 2599 | `		pCursor = pValTok + 1;` |
|        - | 2600 | `		/* Consume separating comma (or end). */` |
|       39 | 2601 | `		if( pCursor < pBodyEnd ){` |
|        3 | 2602 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|      ! 0 | 2603 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2604 | `					"declare: Expecting ',' or ')' after directive value");` |
|      ! 0 | 2605 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2606 | `				return SXRET_OK;` |
|        - | 2607 | `			}` |
|        3 | 2608 | `			pCursor++;` |
|        1 | 2609 | `		}` |
|        5 | 2610 | `	}` |
|        - | 2611 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|        - | 2612 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|        - | 2613 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|       37 | 2614 | `	return SXRET_OK;` |
|        2 | 2615 | `Synchro:` |
|        - | 2616 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|       16 | 2617 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       12 | 2618 | `		pGen->pIn++;` |
|        2 | 2619 | `	}` |
|        6 | 2620 | `	return SXRET_OK;` |
|       27 | 2621 | `}` |
|        - | 2622 | `/*` |
|        - | 2623 | ` * Compile a class constant.` |
|        - | 2624 | ` * According to the PHP language reference manual` |
|        - | 2625 | ` *  Class Constants` |
|        - | 2626 | ` *   It is possible to define constant values on a per-class basis remaining` |
|        - | 2627 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|        - | 2628 | ` *   you don't use the $ symbol to declare or use them.` |
|        - | 2629 | ` *   The value must be a constant expression, not (for example) a variable,` |
|        - | 2630 | ` *   a property, a result of a mathematical operation, or a function call.` |
|        - | 2631 | ` *   It's also possible for interfaces to have constants.` |
|        - | 2632 | ` * Symisc eXtension.` |
|        - | 2633 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|        - | 2634 | ` *  the zend engine would allow only simple scalar value.` |
|        - | 2635 | ` *  Example:` |
|        - | 2636 | ` *   class Test{` |
|        - | 2637 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|        - | 2638 | ` *   };` |
|        - | 2639 | ` *   var_dump(TEST::MyConst);` |
|        - | 2640 | ` *   Refer to the official documentation for more information on the powerful extension` |
|        - | 2641 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|        - | 2642 | ` */` |
|        - | 2643 | `/*` |
|        - | 2644 | ` * Exception handling.` |
|        - | 2645 | ` *  According to the PHP language reference manual` |
|        - | 2646 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|        - | 2647 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|        - | 2648 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|        - | 2649 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|        - | 2650 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|        - | 2651 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|        - | 2652 | ` *    (or re-thrown) within a catch block.` |
|        - | 2653 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|        - | 2654 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|        - | 2655 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|        - | 2656 | ` *    been defined with set_exception_handler().` |
|        - | 2657 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|        - | 2658 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|        - | 2659 | ` */` |
|        - | 2660 | `/*` |
|        - | 2661 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|        - | 2662 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|        - | 2663 | ` * indicates failure.` |
|        - | 2664 | ` */` |
|   544978 | 2665 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 2666 | `{` |
|        - | 2667 | ``	/* php decides throwability at RUNTIME: `throw 5` compiles and raises the`` |
|        - | 2668 | `	 * catchable Error "Can only throw objects" when it executes, and a` |
|        - | 2669 | `	 * non-Throwable object raises "Cannot throw objects that do not implement` |
|        - | 2670 | `	 * Throwable". This used to whitelist a handful of expression shapes and` |
|        - | 2671 | `	 * reject the rest at compile time as a "friendlier" error, which meant the` |
|        - | 2672 | `	 * program never ran and no catch could ever see it — and it was inconsistent` |
|        - | 2673 | ``	 * anyway, since `throw $v` (a variable holding an int) always compiled and`` |
|        - | 2674 | `	 * fell through to the same runtime path. OP_THROW now owns the whole rule,` |
|        - | 2675 | ``	 * so accept any expression here; `throw;` with no operand is still rejected`` |
|        - | 2676 | `	 * by PH7_CompileThrow's SXERR_EMPTY branch. */` |
|   272489 | 2677 | `	SXUNUSED(pGen);` |
|   272489 | 2678 | `	SXUNUSED(pRoot);` |
|   544983 | 2679 | `	return SXRET_OK;` |
|        5 | 2680 | `}` |
|        - | 2681 | `/*` |
|        - | 2682 | ` * Compile a 'throw' statement.` |
|        - | 2683 | ` * throw: This is how you trigger an exception.` |
|        - | 2684 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|        - | 2685 | ` */` |
|   544942 | 2686 | `PH7_PRIVATE sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|        5 | 2687 | `{` |
|   544947 | 2688 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2689 | `	GenBlock *pBlock;` |
|        - | 2690 | `	sxu32 nIdx;` |
|        - | 2691 | `	sxi32 rc;` |
|   544947 | 2692 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|        - | 2693 | `	/* Compile the expression */` |
|   544947 | 2694 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|   544947 | 2695 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 | 2696 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|      ! 0 | 2697 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2698 | `			return SXERR_ABORT;` |
|        - | 2699 | `		}` |
|      ! 0 | 2700 | `		return SXRET_OK;` |
|        - | 2701 | `	}` |
|   544947 | 2702 | `	pBlock = pGen->pCurrent;` |
|        - | 2703 | `	/* Point to the top most function or try block and emit the forward jump */` |
|  2171267 | 2704 | `	while(pBlock->pParent){` |
|  2171261 | 2705 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|   544941 | 2706 | `			break;` |
|        - | 2707 | `		}` |
|        - | 2708 | `		/* Point to the parent block */` |
|  1626325 | 2709 | `		pBlock = pBlock->pParent;` |
|        5 | 2710 | `	}` |
|        - | 2711 | `	/* Emit the throw instruction */` |
|   544947 | 2712 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|        - | 2713 | `	/* Emit the jump */` |
|   544947 | 2714 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|   544947 | 2715 | `	return SXRET_OK;` |
|   272476 | 2716 | `}` |
|        - | 2717 | `/*` |
|        - | 2718 | ` * Compile a PHP 8.0 'throw' expression.` |
|        - | 2719 | ` * Called from the expression code generator when a 'throw' keyword is` |
|        - | 2720 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|        - | 2721 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|        - | 2722 | ` * the validator guarantees the operand is a valid exception target.` |
|        - | 2723 | ` */` |
|       36 | 2724 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|        2 | 2725 | `{` |
|       38 | 2726 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2727 | `	GenBlock *pBlock;` |
|        - | 2728 | `	sxu32 nIdx;` |
|        - | 2729 | `	sxi32 rc;` |
|       18 | 2730 | `	(void)iCompileFlag;` |
|       38 | 2731 | `	pGen->pIn++; /* Skip 'throw' */` |
|       38 | 2732 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 2733 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 2734 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 2735 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2736 | `			return SXERR_ABORT;` |
|        - | 2737 | `		}` |
|      ! 0 | 2738 | `		return SXRET_OK;` |
|        - | 2739 | `	}` |
|       38 | 2740 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|       38 | 2741 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 2742 | `		return SXERR_ABORT;` |
|        - | 2743 | `	}` |
|       38 | 2744 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 | 2745 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 2746 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 2747 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2748 | `			return SXERR_ABORT;` |
|        - | 2749 | `		}` |
|      ! 0 | 2750 | `		return SXRET_OK;` |
|        - | 2751 | `	}` |
|        - | 2752 | `	/* Walk up to nearest exception/function block for the jump target */` |
|       38 | 2753 | `	pBlock = pGen->pCurrent;` |
|       60 | 2754 | `	while( pBlock->pParent ){` |
|       49 | 2755 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|       27 | 2756 | `			break;` |
|        - | 2757 | `		}` |
|       23 | 2758 | `		pBlock = pBlock->pParent;` |
|        1 | 2759 | `	}` |
|       38 | 2760 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       38 | 2761 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|       38 | 2762 | `	return SXRET_OK;` |
|       20 | 2763 | `}` |
|        - | 2764 | `/*` |
|        - | 2765 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|        - | 2766 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|        - | 2767 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|        - | 2768 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|        - | 2769 | ` * compile error propagated from the parser.` |
|        - | 2770 | ` */` |
|       56 | 2771 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|        5 | 2772 | `{` |
|        - | 2773 | `	SyString sClassName;` |
|        - | 2774 | `	SyToken *pToken;` |
|        - | 2775 | `	SyString *pName;` |
|        - | 2776 | `	char *zDup;` |
|        - | 2777 | `	sxi32 rc;` |
|       61 | 2778 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       61 | 2779 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|       61 | 2780 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|       61 | 2781 | `	SySetInit(&pCatch->sByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       61 | 2782 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 | 2783 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 2784 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 2785 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 2786 | `		return SXERR_INVALID;` |
|        - | 2787 | `	}` |
|       61 | 2788 | `	pGen->pIn++; /* '(' */` |
|       28 | 2789 | `	for(;;){` |
|        - | 2790 | `		SyBlob sResolved;` |
|       61 | 2791 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       61 | 2792 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 2793 | `			SyBlobRelease(&sResolved);` |
|      ! 0 | 2794 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 2795 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 2796 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 2797 | `			return SXERR_INVALID;` |
|        - | 2798 | `		}` |
|       89 | 2799 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       56 | 2800 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       61 | 2801 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|       61 | 2802 | `		SyBlobRelease(&sResolved);` |
|       61 | 2803 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|       61 | 2804 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|       61 | 2805 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       56 | 2806 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|        5 | 2807 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|      ! 0 | 2808 | `			pGen->pIn++; continue;` |
|        - | 2809 | `		}` |
|       61 | 2810 | `		break;` |
|      ! 0 | 2811 | `	}` |
|        - | 2812 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|        - | 2813 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding. */` |
|       61 | 2814 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|        3 | 2815 | `		pGen->pIn++; /* ')' */` |
|        3 | 2816 | `		return SXRET_OK;` |
|        - | 2817 | `	}` |
|       54 | 2818 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|       59 | 2819 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 2820 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 2821 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 2822 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 2823 | `		return SXERR_INVALID;` |
|        - | 2824 | `	}` |
|       59 | 2825 | `	pGen->pIn++; /* '$' */` |
|       59 | 2826 | `	pName = &pGen->pIn->sData;` |
|       59 | 2827 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       59 | 2828 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|       59 | 2829 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|       59 | 2830 | `	pGen->pIn++;` |
|       59 | 2831 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 | 2832 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 2833 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 2834 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 2835 | `		return SXERR_INVALID;` |
|        - | 2836 | `	}` |
|       59 | 2837 | `	pGen->pIn++; /* ')' */` |
|       59 | 2838 | `	return SXRET_OK;` |
|       33 | 2839 | `}` |
|        - | 2840 | `/*` |
|        - | 2841 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|        - | 2842 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|        - | 2843 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|        - | 2844 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|        - | 2845 | ` * VmThrowException):` |
|        - | 2846 | ` *` |
|        - | 2847 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|        - | 2848 | ` *    <try body>` |
|        - | 2849 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|        - | 2850 | ` *    JMP  -> finally\|end` |
|        - | 2851 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|        - | 2852 | ` *    <catch body>` |
|        - | 2853 | ` *    JMP  -> finally\|end` |
|        - | 2854 | ` *    ... more catches ...` |
|        - | 2855 | ` *  Lfin: <finally body>` |
|        - | 2856 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|        - | 2857 | ` *  Lend:` |
|        - | 2858 | ` */` |
|      100 | 2859 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|        5 | 2860 | `{` |
|      105 | 2861 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2862 | `	GenBlock *pTry;` |
|        - | 2863 | `	VmInstr *pInstr;` |
|      105 | 2864 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|        - | 2865 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|        - | 2866 | `	sxi32 rc;` |
|      105 | 2867 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|        - | 2868 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION) */` |
|      105 | 2869 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|      105 | 2870 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      105 | 2871 | `	pTry->pUserData = pException;` |
|      105 | 2872 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|      105 | 2873 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|      105 | 2874 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      105 | 2875 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      105 | 2876 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|      105 | 2877 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|        - | 2878 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|      105 | 2879 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|      105 | 2880 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|      105 | 2881 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|      105 | 2882 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 2883 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|      105 | 2884 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|        - | 2885 | `	/* Catch clauses (inline) */` |
|      105 | 2886 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      100 | 2887 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       61 | 2888 | `		sxu32 k = 0;` |
|       84 | 2889 | `		for(;;){` |
|        - | 2890 | `			ph7_exception_block sCatch;` |
|        - | 2891 | `			GenBlock *pCatchBlk;` |
|      117 | 2892 | `			sxu32 idxJmp = 0;` |
|      112 | 2893 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      107 | 2894 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|       33 | 2895 | `				break;` |
|        - | 2896 | `			}` |
|       61 | 2897 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|       61 | 2898 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       61 | 2899 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|       61 | 2900 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|       61 | 2901 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|       61 | 2902 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatchBlk);` |
|       61 | 2903 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        - | 2904 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|        - | 2905 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|        - | 2906 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump). */` |
|       61 | 2907 | `			pCatchBlk->pUserData = pException;` |
|       61 | 2908 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|       61 | 2909 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       61 | 2910 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|       61 | 2911 | `			GenStateLeaveBlock(&(*pGen),0);` |
|        - | 2912 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|        - | 2913 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|       61 | 2914 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       61 | 2915 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|       61 | 2916 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|       61 | 2917 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|       61 | 2918 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       61 | 2919 | `			k++;` |
|        5 | 2920 | `		}` |
|       28 | 2921 | `	}` |
|        - | 2922 | `	/* Finally (inline) */` |
|      105 | 2923 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       80 | 2924 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|        - | 2925 | `		GenBlock *pFinBlk;` |
|       52 | 2926 | `		pGen->pIn++; /* Jump 'finally' */` |
|       52 | 2927 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|       52 | 2928 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|       52 | 2929 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       52 | 2930 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       52 | 2931 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       52 | 2932 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|       52 | 2933 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       52 | 2934 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|       52 | 2935 | `		pException->iHasFinally = 1;` |
|       24 | 2936 | `	}` |
|      105 | 2937 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|      105 | 2938 | `	pException->iInlined = 1;` |
|        - | 2939 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|        - | 2940 | `	{` |
|      105 | 2941 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|        - | 2942 | `		sxu32 *aJ; sxu32 n;` |
|      105 | 2943 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|      105 | 2944 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|      105 | 2945 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|      161 | 2946 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|       61 | 2947 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|       61 | 2948 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|       33 | 2949 | `		}` |
|        - | 2950 | `	}` |
|      105 | 2951 | `	SySetRelease(&aCatchJmp);` |
|      105 | 2952 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|      ! 0 | 2953 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|      ! 0 | 2954 | `	}` |
|      105 | 2955 | `	return SXRET_OK;` |
|       55 | 2956 | `}` |
|        - | 2957 | `/*` |
|        - | 2958 | ` * Compile a 'catch' block.` |
|        - | 2959 | ` * Catch: A "catch" block retrieves an exception and creates` |
|        - | 2960 | ` * an object containing the exception information.` |
|        - | 2961 | ` */` |
|    24812 | 2962 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|        5 | 2963 | `{` |
|    24817 | 2964 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2965 | `	ph7_exception_block sCatch;` |
|        - | 2966 | `	SySet *pInstrContainer;` |
|        - | 2967 | `	SyString sClassName;` |
|        - | 2968 | `	GenBlock *pCatch;` |
|        - | 2969 | `	SyToken *pToken;` |
|        - | 2970 | `	SyString *pName;` |
|        - | 2971 | `	char *zDup;` |
|        - | 2972 | `	sxi32 rc;` |
|    24817 | 2973 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|        - | 2974 | `	/* Zero the structure */` |
|    24817 | 2975 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|        - | 2976 | `	/* Initialize fields */` |
|    24817 | 2977 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|    24817 | 2978 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|    24817 | 2979 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|        - | 2980 | `			/* Unexpected token,break immediately */` |
|      ! 0 | 2981 | `			pToken = pGen->pIn;` |
|      ! 0 | 2982 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 2983 | `				pToken--;` |
|      ! 0 | 2984 | `			}` |
|      ! 0 | 2985 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 2986 | `				"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 2987 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 2988 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2989 | `				return SXERR_ABORT;` |
|        - | 2990 | `			}` |
|      ! 0 | 2991 | `			return SXERR_INVALID;` |
|        - | 2992 | `	}` |
|        - | 2993 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|    24817 | 2994 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|    12421 | 2995 | `	for(;;){` |
|        - | 2996 | `		SyBlob sResolved;` |
|    24847 | 2997 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    24847 | 2998 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|        6 | 2999 | `			SyBlobRelease(&sResolved);` |
|        6 | 3000 | `			pToken = pGen->pIn;` |
|        6 | 3001 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 3002 | `				pToken--;` |
|      ! 0 | 3003 | `			}` |
|        8 | 3004 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 3005 | `				"syntax error, unexpected %s \"%z\"",` |
|        2 | 3006 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|        6 | 3007 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3008 | `				return SXERR_ABORT;` |
|        - | 3009 | `			}` |
|        6 | 3010 | `			return SXERR_INVALID;` |
|        - | 3011 | `		}` |
|        - | 3012 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|        - | 3013 | `		 * transient SyBlob allocation. */` |
|    37262 | 3014 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    24838 | 3015 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|    24843 | 3016 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|    24843 | 3017 | `		SyBlobRelease(&sResolved);` |
|    24843 | 3018 | `		if( zDup == 0 ){` |
|      ! 0 | 3019 | `			goto Mem;` |
|        - | 3020 | `		}` |
|    24843 | 3021 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|    24843 | 3022 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 3023 | `			goto Mem;` |
|        - | 3024 | `		}` |
|        - | 3025 | `		/* Check for '\|' (multi-catch separator) */` |
|    24838 | 3026 | `		if( pGen->pIn < pGen->pEnd &&` |
|    24838 | 3027 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|       35 | 3028 | `			pGen->pIn->sData.nByte == 1 &&` |
|       30 | 3029 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|       32 | 3030 | `			pGen->pIn++; /* Consume the '\|' */` |
|       32 | 3031 | `			continue;` |
|        - | 3032 | `		}` |
|    24813 | 3033 | `		break;` |
|      ! 0 | 3034 | `	}` |
|        - | 3035 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|        - | 3036 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding;` |
|        - | 3037 | `	 * jump straight to compiling the block below. */` |
|    24813 | 3038 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) /*)*/ ){` |
|        7 | 3039 | `		goto CatchBody;` |
|        - | 3040 | `	}` |
|    24802 | 3041 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|    24807 | 3042 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - | 3043 | `			/* Unexpected token,break immediately */` |
|      ! 0 | 3044 | `			pToken = pGen->pIn;` |
|      ! 0 | 3045 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 3046 | `				pToken--;` |
|      ! 0 | 3047 | `			}` |
|      ! 0 | 3048 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 3049 | `				"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 3050 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 3051 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3052 | `				return SXERR_ABORT;` |
|        - | 3053 | `			}` |
|      ! 0 | 3054 | `			return SXERR_INVALID;` |
|        - | 3055 | `	}` |
|    24807 | 3056 | `	pGen->pIn++; /* Jump the dollar sign */` |
|        - | 3057 | `	/* Duplicate instance name */` |
|    24807 | 3058 | `	pName = &pGen->pIn->sData;` |
|    24807 | 3059 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    24807 | 3060 | `	if( zDup == 0 ){` |
|      ! 0 | 3061 | `		goto Mem;` |
|        - | 3062 | `	}` |
|    24807 | 3063 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|    24807 | 3064 | `	pGen->pIn++;` |
|    12404 | 3065 | `CatchBody:` |
|    24813 | 3066 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|        - | 3067 | `		/* Unexpected token,break immediately */` |
|      ! 0 | 3068 | `		pToken = pGen->pIn;` |
|      ! 0 | 3069 | `		if( pToken >= pGen->pEnd ){` |
|      ! 0 | 3070 | `			pToken--;` |
|      ! 0 | 3071 | `		}` |
|      ! 0 | 3072 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 3073 | `			"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 3074 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 3075 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3076 | `			return SXERR_ABORT;` |
|        - | 3077 | `		}` |
|      ! 0 | 3078 | `		return SXERR_INVALID;` |
|        - | 3079 | `	}` |
|        - | 3080 | `	/* Compile the block */` |
|    24813 | 3081 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|        - | 3082 | `	/* Create the catch block */` |
|    24813 | 3083 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|    24813 | 3084 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3085 | `		return SXERR_ABORT;` |
|        - | 3086 | `	}` |
|        - | 3087 | `	/* Swap bytecode container */` |
|    24813 | 3088 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    24813 | 3089 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|        - | 3090 | `	/* Compile the block */` |
|    24813 | 3091 | `	PH7_CompileBlock(&(*pGen),0);` |
|        - | 3092 | `	/* Fix forward jumps now the destination is resolved  */` |
|    24813 | 3093 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 3094 | `	/* Emit the DONE instruction */` |
|    24813 | 3095 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - | 3096 | `	/* Leave the block */` |
|    24813 | 3097 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 3098 | `	/* Restore the default container */` |
|    24813 | 3099 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - | 3100 | `	/* Install the catch block */` |
|    24813 | 3101 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|    24813 | 3102 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3103 | `		goto Mem;` |
|        - | 3104 | `	}` |
|    24813 | 3105 | `	return SXRET_OK;` |
|      ! 0 | 3106 | `Mem:` |
|      ! 0 | 3107 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 3108 | `	return SXERR_ABORT;` |
|    12411 | 3109 | `}` |
|        - | 3110 | `/*` |
|        - | 3111 | ` * Compile a 'try' block.` |
|        - | 3112 | ` * A function using an exception should be in a "try" block.` |
|        - | 3113 | ` * If the exception does not trigger, the code will continue` |
|        - | 3114 | ` * as normal. However if the exception triggers, an exception` |
|        - | 3115 | ` * is "thrown".` |
|        - | 3116 | ` */` |
|    24970 | 3117 | `PH7_PRIVATE sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|        5 | 3118 | `{` |
|        - | 3119 | `	ph7_exception *pException;` |
|    24975 | 3120 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 3121 | `	GenBlock *pTry;` |
|        - | 3122 | `	sxu32 nJmpIdx;` |
|        - | 3123 | `	sxi32 rc;` |
|        - | 3124 | `	/* Create the exception container */` |
|    24975 | 3125 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|    24975 | 3126 | `	if( pException == 0 ){` |
|      ! 0 | 3127 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|      ! 0 | 3128 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 3129 | `		return SXERR_ABORT;` |
|        - | 3130 | `	}` |
|        - | 3131 | `	/* Zero the structure */` |
|    24975 | 3132 | `	SyZero(pException,sizeof(ph7_exception));` |
|        - | 3133 | `	/* Initialize fields */` |
|    24975 | 3134 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|    24975 | 3135 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|    24975 | 3136 | `	pException->iHasFinally = 0;` |
|    24975 | 3137 | `	pException->iFinallyDone = 0;` |
|    24975 | 3138 | `	pException->pVm = pGen->pVm;` |
|        - | 3139 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|        - | 3140 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the legacy path.`` |
|        - | 3141 | `	 * DORMANT until the inline VM handlers (OP_CATCH / OP_END_FINALLY dispatch,` |
|        - | 3142 | `	 * VmThrowException pc-redirect, return/break-through-finally threading, generator` |
|        - | 3143 | `	 * park of aFinallyAction) land — the compiler emits the layout but the VM cannot yet` |
|        - | 3144 | `	 * execute it. Guarded by pVm->bInlineTryCatch (default 0) so the tree stays green. */` |
|    24975 | 3145 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|      105 | 3146 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|        - | 3147 | `	}` |
|        - | 3148 | `	/* Create the try block */` |
|    24875 | 3149 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|    24875 | 3150 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3151 | `		return SXERR_ABORT;` |
|        - | 3152 | `	}` |
|        - | 3153 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|    24875 | 3154 | `	pTry->pUserData = pException;` |
|        - | 3155 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|    24875 | 3156 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|        - | 3157 | `	/* Fix the jump later when the destination is resolved */` |
|    24875 | 3158 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|    24875 | 3159 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|        - | 3160 | `	/* Compile the block */` |
|    24875 | 3161 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|    24875 | 3162 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 3163 | `		return SXERR_ABORT;` |
|        - | 3164 | `	}` |
|        - | 3165 | `	/* Fix forward jumps now the destination is resolved */` |
|    24875 | 3166 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 3167 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|    24875 | 3168 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|        - | 3169 | `	/* Leave the block */` |
|    24875 | 3170 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 3171 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|    24875 | 3172 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|    24868 | 3173 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|        - | 3174 | `		/* Compile one or more catch blocks */` |
|    24808 | 3175 | `		for(;;){` |
|    49616 | 3176 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    37294 | 3177 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|    12407 | 3178 | `					break;` |
|        - | 3179 | `			}` |
|    24817 | 3180 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|    24817 | 3181 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3182 | `				return SXERR_ABORT;` |
|        - | 3183 | `			}` |
|        5 | 3184 | `		}` |
|    12402 | 3185 | `	}` |
|        - | 3186 | `	/* Compile optional finally block */` |
|    24875 | 3187 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      804 | 3188 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|        - | 3189 | `		SySet *pInstrContainer;` |
|        - | 3190 | `		GenBlock *pFinBlock;` |
|      129 | 3191 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|        - | 3192 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      129 | 3193 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      129 | 3194 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 3195 | `			return SXERR_ABORT;` |
|        - | 3196 | `		}` |
|        - | 3197 | `		/* Swap bytecode container */` |
|      129 | 3198 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      129 | 3199 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|        - | 3200 | `		/* Compile the finally body */` |
|      129 | 3201 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      129 | 3202 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3203 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 | 3204 | `			return SXERR_ABORT;` |
|        - | 3205 | `		}` |
|        - | 3206 | `		/* Fix forward jumps now the destination is resolved */` |
|      129 | 3207 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 3208 | `		/* Emit DONE to terminate the finally block */` |
|      129 | 3209 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - | 3210 | `		/* Leave the block */` |
|      129 | 3211 | `		GenStateLeaveBlock(&(*pGen),0);` |
|        - | 3212 | `		/* Restore the default container */` |
|      129 | 3213 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      129 | 3214 | `		pException->iHasFinally = 1;` |
|       62 | 3215 | `	}` |
|        - | 3216 | `	/* Must have at least one catch or finally */` |
|    24875 | 3217 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|        8 | 3218 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 3219 | `			"Cannot use try without catch or finally");` |
|        8 | 3220 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3221 | `			return SXERR_ABORT;` |
|        - | 3222 | `		}` |
|        3 | 3223 | `	}` |
|    24875 | 3224 | `	return SXRET_OK;` |
|    12490 | 3225 | `}` |
|        - | 3226 | `/*` |
|        - | 3227 | ` * Compile a switch block.` |
|        - | 3228 | ` *  (See block-comment below for more information)` |
|        - | 3229 | ` */` |
|   135282 | 3230 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|        5 | 3231 | `{` |
|   135287 | 3232 | `	sxi32 rc = SXRET_OK;` |
|   135287 | 3233 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|        - | 3234 | `		/* Unexpected token */` |
|      ! 0 | 3235 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 | 3236 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3237 | `			return SXERR_ABORT;` |
|        - | 3238 | `		}` |
|      ! 0 | 3239 | `		pGen->pIn++;` |
|      ! 0 | 3240 | `	}` |
|   135287 | 3241 | `	pGen->pIn++;` |
|        - | 3242 | `	/* First instruction to execute in this block. */` |
|   135287 | 3243 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|        - | 3244 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|        - | 3245 | `	 * or the '}' token */` |
|   129583 | 3246 | `	for(;;){` |
|   259171 | 3247 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 3248 | `			/* No more input to process */` |
|      ! 0 | 3249 | `			break;` |
|        - | 3250 | `		}` |
|   259171 | 3251 | `		rc = SXRET_OK;` |
|   259171 | 3252 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|    34843 | 3253 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|    11617 | 3254 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|        - | 3255 | `					/* Unexpected token */` |
|      ! 0 | 3256 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|      ! 0 | 3257 | `						&pGen->pIn->sData);` |
|      ! 0 | 3258 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3259 | `						return SXERR_ABORT;` |
|        - | 3260 | `					}` |
|        - | 3261 | `					/* FALL THROUGH */` |
|      ! 0 | 3262 | `				}` |
|    11617 | 3263 | `				rc = SXERR_EOF;` |
|    11617 | 3264 | `				break;` |
|        - | 3265 | `			}` |
|    11618 | 3266 | `		}else{` |
|        - | 3267 | `			sxi32 nKwrd;` |
|        - | 3268 | `			/* Extract the keyword */` |
|   224333 | 3269 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   224333 | 3270 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|    61839 | 3271 | `				break;` |
|        - | 3272 | `			}` |
|   100665 | 3273 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|        3 | 3274 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|        - | 3275 | `					/* Unexpected token */` |
|      ! 0 | 3276 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|      ! 0 | 3277 | `						&pGen->pIn->sData);` |
|      ! 0 | 3278 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3279 | `						return SXERR_ABORT;` |
|        - | 3280 | `					}` |
|        - | 3281 | `					/* FALL THROUGH */` |
|      ! 0 | 3282 | `				}` |
|        - | 3283 | `				/* Block compiled */` |
|        3 | 3284 | `				break;` |
|        - | 3285 | `			}` |
|        - | 3286 | `		}` |
|        - | 3287 | `		/* Compile block */` |
|   123889 | 3288 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|   123889 | 3289 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3290 | `			return SXERR_ABORT;` |
|        - | 3291 | `		}` |
|        5 | 3292 | `	}` |
|   135287 | 3293 | `	return rc;` |
|    67646 | 3294 | `}` |
|        - | 3295 | `/*` |
|        - | 3296 | ` * Compile a case eXpression.` |
|        - | 3297 | ` *  (See block-comment below for more information)` |
|        - | 3298 | ` */` |
|   131400 | 3299 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|        5 | 3300 | `{` |
|        - | 3301 | `	SySet *pInstrContainer;` |
|        - | 3302 | `	SyToken *pEnd,*pTmp;` |
|   131405 | 3303 | `	sxi32 iNest = 0;` |
|        - | 3304 | `	sxi32 rc;` |
|        - | 3305 | `	/* Delimit the expression */` |
|   131405 | 3306 | `	pEnd = pGen->pIn;` |
|   262813 | 3307 | `	while( pEnd < pGen->pEnd ){` |
|   262813 | 3308 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|        - | 3309 | `			/* Increment nesting level */` |
|        3 | 3310 | `			iNest++;` |
|   262812 | 3311 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|        - | 3312 | `			/* Decrement nesting level */` |
|        3 | 3313 | `			iNest--;` |
|   262810 | 3314 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|   131405 | 3315 | `			break;` |
|        - | 3316 | `		}` |
|   131413 | 3317 | `		pEnd++;` |
|        5 | 3318 | `	}` |
|   131405 | 3319 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 3320 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|      ! 0 | 3321 | `		if( rc == SXERR_ABORT ){` |
|        - | 3322 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 3323 | `			return SXERR_ABORT;` |
|        - | 3324 | `		}` |
|      ! 0 | 3325 | `	}` |
|        - | 3326 | `	/* Swap token stream */` |
|   131405 | 3327 | `	pTmp = pGen->pEnd;` |
|   131405 | 3328 | `	pGen->pEnd = pEnd;` |
|   131405 | 3329 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   131405 | 3330 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|   131405 | 3331 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 3332 | `	/* Emit the done instruction */` |
|   131405 | 3333 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   131405 | 3334 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - | 3335 | `	/* Update token stream */` |
|   131405 | 3336 | `	pGen->pIn  = pEnd;` |
|   131405 | 3337 | `	pGen->pEnd = pTmp;` |
|   131405 | 3338 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 3339 | `		return SXERR_ABORT;` |
|        - | 3340 | `	}` |
|   131405 | 3341 | `	return SXRET_OK;` |
|    65705 | 3342 | `}` |
|        - | 3343 | `/*` |
|        - | 3344 | ` * Compile the smart switch statement.` |
|        - | 3345 | ` * According to the PHP language reference manual` |
|        - | 3346 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|        - | 3347 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|        - | 3348 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|        - | 3349 | ` *  This is exactly what the switch statement is for.` |
|        - | 3350 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|        - | 3351 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|        - | 3352 | ` *  of the outer loop, use continue 2.` |
|        - | 3353 | ` *  Note that switch/case does loose comparision.` |
|        - | 3354 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|        - | 3355 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|        - | 3356 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|        - | 3357 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|        - | 3358 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|        - | 3359 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|        - | 3360 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|        - | 3361 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|        - | 3362 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|        - | 3363 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|        - | 3364 | ` *  list for the next case.` |
|        - | 3365 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|        - | 3366 | ` *  or floating-point numbers and strings.` |
|        - | 3367 | ` */` |
|    11614 | 3368 | `PH7_PRIVATE sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|        5 | 3369 | `{` |
|        - | 3370 | `	GenBlock *pSwitchBlock;` |
|        - | 3371 | `	SyToken *pTmp,*pEnd;` |
|        - | 3372 | `	ph7_switch *pSwitch;` |
|        - | 3373 | `	sxu32 nToken;` |
|        - | 3374 | `	sxu32 nLine;` |
|        - | 3375 | `	sxi32 rc;` |
|    11619 | 3376 | `	nLine = pGen->pIn->nLine;` |
|        - | 3377 | `	/* Jump the 'switch' keyword */` |
|    11619 | 3378 | `	pGen->pIn++;` |
|    11619 | 3379 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 3380 | `		/* Syntax error */` |
|      ! 0 | 3381 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|      ! 0 | 3382 | `		if( rc == SXERR_ABORT ){` |
|        - | 3383 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 3384 | `			return SXERR_ABORT;` |
|        - | 3385 | `		}` |
|      ! 0 | 3386 | `		goto Synchronize;` |
|        - | 3387 | `	}` |
|        - | 3388 | `	/* Jump the left parenthesis '(' */` |
|    11619 | 3389 | `	pGen->pIn++;` |
|    11619 | 3390 | `	pEnd = 0; /* cc warning */` |
|        - | 3391 | `	/* Create the loop block */` |
|    17426 | 3392 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|     5807 | 3393 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|    11619 | 3394 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3395 | `		return SXERR_ABORT;` |
|        - | 3396 | `	}` |
|        - | 3397 | `	/* Delimit the condition */` |
|    11619 | 3398 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    11619 | 3399 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - | 3400 | `		/* Empty expression */` |
|      ! 0 | 3401 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|      ! 0 | 3402 | `		if( rc == SXERR_ABORT ){` |
|        - | 3403 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 3404 | `			return SXERR_ABORT;` |
|        - | 3405 | `		}` |
|      ! 0 | 3406 | `	}` |
|        - | 3407 | `	/* Swap token streams */` |
|    11619 | 3408 | `	pTmp = pGen->pEnd;` |
|    11619 | 3409 | `	pGen->pEnd = pEnd;` |
|        - | 3410 | `	/* Compile the expression */` |
|    11619 | 3411 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    11619 | 3412 | `	if( rc == SXERR_ABORT ){` |
|        - | 3413 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 3414 | `		return SXERR_ABORT;` |
|        - | 3415 | `	}` |
|        - | 3416 | `	/* Update token stream */` |
|    11619 | 3417 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 | 3418 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 3419 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 3420 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3421 | `			return SXERR_ABORT;` |
|        - | 3422 | `		}` |
|      ! 0 | 3423 | `		pGen->pIn++;` |
|      ! 0 | 3424 | `	}` |
|    11619 | 3425 | `	pGen->pIn  = &pEnd[1];` |
|    11619 | 3426 | `	pGen->pEnd = pTmp;` |
|    11619 | 3427 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|    11614 | 3428 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|      ! 0 | 3429 | `			pTmp = pGen->pIn;` |
|      ! 0 | 3430 | `			if( pTmp >= pGen->pEnd ){` |
|      ! 0 | 3431 | `				pTmp--;` |
|      ! 0 | 3432 | `			}` |
|        - | 3433 | `			/* Unexpected token */` |
|      ! 0 | 3434 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|      ! 0 | 3435 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3436 | `				return SXERR_ABORT;` |
|        - | 3437 | `			}` |
|      ! 0 | 3438 | `			goto Synchronize;` |
|        - | 3439 | `	}` |
|        - | 3440 | `	/* Set the delimiter token */` |
|    11619 | 3441 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|        3 | 3442 | `		nToken = PH7_TK_KEYWORD;` |
|        - | 3443 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|        2 | 3444 | `	}else{` |
|    11617 | 3445 | `		nToken = PH7_TK_CCB; /* '}' */` |
|        - | 3446 | `	}` |
|    11619 | 3447 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|        - | 3448 | `	/* Create the switch blocks container */` |
|    11619 | 3449 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|    11619 | 3450 | `	if( pSwitch == 0 ){` |
|        - | 3451 | `		/* Abort compilation */` |
|      ! 0 | 3452 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 3453 | `		return SXERR_ABORT;` |
|        - | 3454 | `	}` |
|        - | 3455 | `	/* Zero the structure */` |
|    11619 | 3456 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|        - | 3457 | `	/* Initialize fields */` |
|    11619 | 3458 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|        - | 3459 | `	/* Emit the switch instruction */` |
|    11619 | 3460 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|        - | 3461 | `	/* Compile case blocks */` |
|   129477 | 3462 | `	for(;;){` |
|        - | 3463 | `		sxu32 nKwrd;` |
|   135289 | 3464 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 3465 | `			/* No more input to process */` |
|      ! 0 | 3466 | `			break;` |
|        - | 3467 | `		}` |
|   135289 | 3468 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 3469 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|        - | 3470 | `				/* Unexpected token */` |
|      ! 0 | 3471 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 3472 | `					&pGen->pIn->sData);` |
|      ! 0 | 3473 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3474 | `					return SXERR_ABORT;` |
|        - | 3475 | `				}` |
|        - | 3476 | `				/* FALL THROUGH */` |
|      ! 0 | 3477 | `			}` |
|        - | 3478 | `			/* Block compiled */` |
|      ! 0 | 3479 | `			break;` |
|        - | 3480 | `		}` |
|        - | 3481 | `		/* Extract the keyword */` |
|   135289 | 3482 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   135289 | 3483 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|        3 | 3484 | `			if( nToken != PH7_TK_KEYWORD ){` |
|        - | 3485 | `				/* Unexpected token */` |
|      ! 0 | 3486 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 3487 | `					&pGen->pIn->sData);` |
|      ! 0 | 3488 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3489 | `					return SXERR_ABORT;` |
|        - | 3490 | `				}` |
|        - | 3491 | `				/* FALL THROUGH */` |
|      ! 0 | 3492 | `			}` |
|        - | 3493 | `			/* Block compiled */` |
|        3 | 3494 | `			break;` |
|        - | 3495 | `		}` |
|   135287 | 3496 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|        - | 3497 | `			/*` |
|        - | 3498 | `			 * Accroding to the PHP language reference manual` |
|        - | 3499 | `			 *  A special case is the default case. This case matches anything` |
|        - | 3500 | `			 *  that wasn't matched by the other cases.` |
|        - | 3501 | `			 */` |
|     3887 | 3502 | `			if( pSwitch->nDefault > 0 ){` |
|        - | 3503 | `				/* Default case already compiled */` |
|      ! 0 | 3504 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|      ! 0 | 3505 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3506 | `					return SXERR_ABORT;` |
|        - | 3507 | `				}` |
|      ! 0 | 3508 | `			}` |
|     3887 | 3509 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|        - | 3510 | `			/* Compile the default block */` |
|     3887 | 3511 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|     3887 | 3512 | `			if( rc == SXERR_ABORT){` |
|      ! 0 | 3513 | `				return SXERR_ABORT;` |
|     3887 | 3514 | `			}else if( rc == SXERR_EOF ){` |
|     3885 | 3515 | `				break;` |
|        1 | 3516 | `			}` |
|   131406 | 3517 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|        - | 3518 | `			ph7_case_expr sCase;` |
|        - | 3519 | `			/* Standard case block */` |
|   131405 | 3520 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|        - | 3521 | `			/* initialize the structure */` |
|   131405 | 3522 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - | 3523 | `			/* Compile the case expression */` |
|   131405 | 3524 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|   131405 | 3525 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3526 | `				return SXERR_ABORT;` |
|        - | 3527 | `			}` |
|        - | 3528 | `			/* Compile the case block */` |
|   131405 | 3529 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|        - | 3530 | `			/* Insert in the switch container */` |
|   131405 | 3531 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|   131405 | 3532 | `			if( rc == SXERR_ABORT){` |
|      ! 0 | 3533 | `				return SXERR_ABORT;` |
|   131405 | 3534 | `			}else if( rc == SXERR_EOF ){` |
|     7737 | 3535 | `				break;` |
|        - | 3536 | `			}` |
|    61839 | 3537 | `		}else{` |
|        - | 3538 | `			/* Unexpected token */` |
|      ! 0 | 3539 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 3540 | `				&pGen->pIn->sData);` |
|      ! 0 | 3541 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3542 | `				return SXERR_ABORT;` |
|        - | 3543 | `			}` |
|      ! 0 | 3544 | `			break;` |
|        - | 3545 | `		}` |
|        5 | 3546 | `	}` |
|        - | 3547 | `	/* Fix all jumps now the destination is resolved */` |
|    11619 | 3548 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|    11619 | 3549 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 3550 | `	/* Release the loop block */` |
|    11619 | 3551 | `	GenStateLeaveBlock(pGen,0);` |
|    11619 | 3552 | `	if( pGen->pIn < pGen->pEnd ){` |
|        - | 3553 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|    11619 | 3554 | `		pGen->pIn++;` |
|     5807 | 3555 | `	}` |
|        - | 3556 | `	/* Statement successfully compiled */` |
|    11619 | 3557 | `	return SXRET_OK;` |
|      ! 0 | 3558 | `Synchronize:` |
|        - | 3559 | `	/* Synchronize with the first semi-colon */` |
|      ! 0 | 3560 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      ! 0 | 3561 | `		pGen->pIn++;` |
|      ! 0 | 3562 | `	}` |
|      ! 0 | 3563 | `	return SXRET_OK;` |
|     5812 | 3564 | `}` |
|        - | 3565 |  |
