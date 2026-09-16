# src/ph7/compile_stmt.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1446/1951 lines (74.12%)

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
|   179376 |  168 | `static int GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|        5 |  169 | `{` |
|   179381 |  170 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   179381 |  171 | `	int nInlineTry = 0;` |
|   725085 |  172 | `	while( pBlock && pBlock != pTarget ){` |
|   545709 |  173 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
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
|   545709 |  190 | `		pBlock = pBlock->pParent;` |
|        5 |  191 | `	}` |
|   179381 |  192 | `	return nInlineTry;` |
|        5 |  193 | `}` |
|    89660 |  194 | `PH7_PRIVATE sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|        5 |  195 | `{` |
|        - |  196 | `	GenBlock *pLoop; /* Target loop */` |
|        - |  197 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|        - |  198 | `	sxu32 nLineLocal;` |
|        - |  199 | `	sxi32 rc;` |
|    89665 |  200 | `	nLineLocal = pGen->pIn->nLine;` |
|    89665 |  201 | `	iLevel = 0;` |
|        - |  202 | `	/* Jump the 'continue' keyword */` |
|    89665 |  203 | `	pGen->pIn++;` |
|    89665 |  204 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|        - |  205 | `		/* optional numeric argument which tells us how many levels` |
|        - |  206 | `		 * of enclosing loops we should skip to the end of.` |
|        - |  207 | `		 */` |
|        - |  208 | `		char zScratch[GEN_NUM_SCRATCH];` |
|       16 |  209 | `		char *zAlloc = 0;` |
|        - |  210 | `		SyString sNum;` |
|       16 |  211 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|       16 |  212 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  213 | `			return SXERR_ABORT;` |
|        - |  214 | `		}` |
|       16 |  215 | `		if( rc == SXRET_OK ){` |
|       20 |  216 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|       12 |  217 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|       14 |  218 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  219 | `				return SXERR_ABORT;` |
|        - |  220 | `			}` |
|       14 |  221 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|       14 |  222 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        6 |  223 | `		}` |
|       16 |  224 | `		if( iLevel < 2 ){` |
|        3 |  225 | `			iLevel = 0;` |
|        1 |  226 | `		}` |
|       16 |  227 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|        7 |  228 | `	}` |
|        - |  229 | `	/* Point to the target loop */` |
|    89665 |  230 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    89665 |  231 | `	if( pLoop == 0 ){` |
|        - |  232 | `		/* Illegal continue */` |
|       13 |  233 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"'continue' not in the 'loop' or 'switch' context");` |
|       13 |  234 | `		if( rc == SXERR_ABORT ){` |
|        - |  235 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  236 | `			return SXERR_ABORT;` |
|        - |  237 | `		}` |
|        8 |  238 | `	}else{` |
|    89655 |  239 | `		sxu32 nInstrIdx = 0;` |
|        - |  240 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|    89655 |  241 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|        - |  242 | `		/* ROOT C: in a generator, a break/continue crossing inline trys must run their` |
|        - |  243 | `		 * finallys first. OP_SET_FINALLY_JMP(iP1=count) does that then takes the loop jump. */` |
|    89655 |  244 | `		sxi32 iJmpOp = nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP;` |
|    89655 |  245 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|        - |  246 | ``			/* `continue` inside a switch acts like `break` — which is almost never what`` |
|        - |  247 | `			 * the author meant, so php 7.3+ says so at compile time. The generated jump` |
|        - |  248 | `			 * is unchanged; only the diagnostic was missing. An explicit level` |
|        - |  249 | ``			 * (`continue 2`) targets the enclosing loop and stays silent. */`` |
|        5 |  250 | `			if( iLevel < 1 ){` |
|        5 |  251 | `				PH7_GenCompileError(&(*pGen),E_WARNING,nLineLocal,` |
|        - |  252 | `					"\"continue\" targeting switch is equivalent to \"break\"."` |
|        - |  253 | `					" Did you mean to use \"continue 2\"?");` |
|        2 |  254 | `			}` |
|        5 |  255 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,0,0,&nInstrIdx);` |
|        5 |  256 | `			if( rc == SXRET_OK ){` |
|        5 |  257 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|        2 |  258 | `			}` |
|        3 |  259 | `		}else{` |
|        - |  260 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    89651 |  261 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    89651 |  262 | `			if( pLoop->bPostContinue == TRUE ){` |
|        - |  263 | `				JumpFixup sJumpFix;` |
|        - |  264 | `				/* Post-continue */` |
|    27289 |  265 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|    27289 |  266 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|    27289 |  267 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|    13642 |  268 | `			}` |
|        - |  269 | `		}` |
|        - |  270 | `	}` |
|    89665 |  271 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  272 | `		/* Not so fatal,emit a warning only */` |
|      ! 0 |  273 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|      ! 0 |  274 | `	}` |
|        - |  275 | `	/* Statement successfully compiled */` |
|    89665 |  276 | `	return SXRET_OK;` |
|    44835 |  277 | `}` |
|        - |  278 | `/*` |
|        - |  279 | ` * Compile the 'break' statement.` |
|        - |  280 | ` * According to the PHP language reference` |
|        - |  281 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|        - |  282 | ` *  structure.` |
|        - |  283 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|        - |  284 | ` *  enclosing structures are to be broken out of.` |
|        - |  285 | ` */` |
|    89742 |  286 | `PH7_PRIVATE sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|        5 |  287 | `{` |
|        - |  288 | `	GenBlock *pLoop; /* Target loop */` |
|        - |  289 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|        - |  290 | `	sxi32 rc;` |
|    89747 |  291 | `	iLevel = 0;` |
|        - |  292 | `	/* Jump the 'break' keyword */` |
|    89747 |  293 | `	pGen->pIn++;` |
|    89747 |  294 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|        - |  295 | `		/* optional numeric argument which tells us how many levels` |
|        - |  296 | `		 * of enclosing loops we should skip to the end of.` |
|        - |  297 | `		 */` |
|        - |  298 | `		char zScratch[GEN_NUM_SCRATCH];` |
|       17 |  299 | `		char *zAlloc = 0;` |
|        - |  300 | `		SyString sNum;` |
|       17 |  301 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|       17 |  302 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  303 | `			return SXERR_ABORT;` |
|        - |  304 | `		}` |
|       17 |  305 | `		if( rc == SXRET_OK ){` |
|       21 |  306 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|       12 |  307 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|       15 |  308 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  309 | `				return SXERR_ABORT;` |
|        - |  310 | `			}` |
|       15 |  311 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|       15 |  312 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        6 |  313 | `		}` |
|       17 |  314 | `		if( iLevel < 2 ){` |
|        3 |  315 | `			iLevel = 0;` |
|        1 |  316 | `		}` |
|       17 |  317 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|        7 |  318 | `	}` |
|        - |  319 | `	/* Extract the target loop */` |
|    89747 |  320 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    89747 |  321 | `	if( pLoop == 0 ){` |
|        - |  322 | `		/* Illegal break */` |
|       19 |  323 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"'break' not in the 'loop' or 'switch' context");` |
|       19 |  324 | `		if( rc == SXERR_ABORT ){` |
|        - |  325 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  326 | `			return SXERR_ABORT;` |
|        - |  327 | `		}` |
|       11 |  328 | `	}else{` |
|        - |  329 | `		sxu32 nInstrIdx;` |
|        - |  330 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|    89731 |  331 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|        - |  332 | `		/* ROOT C: OP_SET_FINALLY_JMP runs the crossed inline finallys before the break jump. */` |
|    89731 |  333 | `		rc = PH7_VmEmitInstr(pGen->pVm,nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP,nCross,0,0,&nInstrIdx);` |
|    89731 |  334 | `		if( rc == SXRET_OK ){` |
|        - |  335 | `			/* Fix the jump later when the jump destination is resolved */` |
|    89731 |  336 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|    44863 |  337 | `		}` |
|        - |  338 | `	}` |
|    89747 |  339 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  340 | `		/* Not so fatal,emit a warning only */` |
|      ! 0 |  341 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|      ! 0 |  342 | `	}` |
|        - |  343 | `	/* Statement successfully compiled */` |
|    89747 |  344 | `	return SXRET_OK;` |
|    44876 |  345 | `}` |
|        - |  346 | `/*` |
|        - |  347 | ` * Compile or record a label.` |
|        - |  348 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|        - |  349 | ` * Example` |
|        - |  350 | ` *  goto LABEL;` |
|        - |  351 | ` *   echo 'Foo';` |
|        - |  352 | ` *  LABEL:` |
|        - |  353 | ` *   echo 'Bar';` |
|        - |  354 | ` */` |
|      112 |  355 | `PH7_PRIVATE sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|        5 |  356 | `{` |
|        - |  357 | `	GenBlock *pBlock;` |
|        - |  358 | `	Label sLabel;` |
|        - |  359 | `	/* php places NO restriction on where a label may be DEFINED — inside a loop, a switch` |
|        - |  360 | `	 * or a try{} is all fine. The only rule is on the jump: you may not goto INTO a loop` |
|        - |  361 | `	 * or switch from outside it, which is checked once the labels are all known (see` |
|        - |  362 | `	 * GenStateFixJumps). Record the loop this label sits in so that check can run. */` |
|        - |  363 | `	{` |
|      117 |  364 | `		SyString *pTarget = &pGen->pIn->sData;` |
|        - |  365 | `		char *zDup;` |
|        - |  366 | `		/* Initialize label fields */` |
|      117 |  367 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|        - |  368 | `		/* Duplicate label name */` |
|      117 |  369 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|      117 |  370 | `		if( zDup == 0 ){` |
|      ! 0 |  371 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  372 | `			return SXERR_ABORT;` |
|        - |  373 | `		}` |
|      117 |  374 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|      117 |  375 | `		sLabel.bRef  = FALSE;` |
|      117 |  376 | `		sLabel.nLine = pGen->pIn->nLine;` |
|      117 |  377 | `		sLabel.nLoopId = pGen->nCurLoopId;` |
|      117 |  378 | `		pBlock = pGen->pCurrent;` |
|      233 |  379 | `		while( pBlock ){` |
|      143 |  380 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|       27 |  381 | `				break;` |
|        - |  382 | `			}` |
|        - |  383 | `			/* Point to the upper block */` |
|      121 |  384 | `			pBlock = pBlock->pParent;` |
|        5 |  385 | `		}` |
|      117 |  386 | `		if( pBlock ){` |
|       27 |  387 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       16 |  388 | `		}else{` |
|       95 |  389 | `			sLabel.pFunc = 0;` |
|        - |  390 | `		}` |
|        - |  391 | `		/* Insert in label set */` |
|      117 |  392 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|        - |  393 | `	}` |
|      117 |  394 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|      117 |  395 | `	return SXRET_OK;` |
|       61 |  396 | `}` |
|        - |  397 | `/*` |
|        - |  398 | ` * Compile the so hated 'goto' statement.` |
|        - |  399 | ` * You've probably been taught that gotos are bad, but this sort` |
|        - |  400 | ` * of rewriting  happens all the time, in fact every time you run` |
|        - |  401 | ` * a compiler it has to do this.` |
|        - |  402 | ` * According to the PHP language reference manual` |
|        - |  403 | ` *   The goto operator can be used to jump to another section in the program.` |
|        - |  404 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|        - |  405 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|        - |  406 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|        - |  407 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|        - |  408 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|        - |  409 | ` *   of a multi-level break` |
|        - |  410 | ` */` |
|      152 |  411 | `PH7_PRIVATE sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|        5 |  412 | `{` |
|        - |  413 | `	JumpFixup sJump;` |
|        - |  414 | `	sxi32 rc;` |
|      157 |  415 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|      157 |  416 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - |  417 | `		/* Missing label */` |
|      ! 0 |  418 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|      ! 0 |  419 | `		if( rc == SXERR_ABORT ){` |
|        - |  420 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  421 | `			return SXERR_ABORT;` |
|        - |  422 | `		}` |
|      ! 0 |  423 | `		return SXRET_OK;` |
|        - |  424 | `	}` |
|      157 |  425 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|        5 |  426 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn,"identifier");` |
|        5 |  427 | `		if( rc == SXERR_ABORT ){` |
|        - |  428 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  429 | `			return SXERR_ABORT;` |
|        - |  430 | `		}` |
|        3 |  431 | `	}else{` |
|      153 |  432 | `		SyString *pTarget = &pGen->pIn->sData;` |
|        - |  433 | `		GenBlock *pBlock;` |
|        - |  434 | `		char *zDup;` |
|        - |  435 | `		/* Prepare the jump destination */` |
|      153 |  436 | `		sJump.nJumpType = PH7_OP_JMP;` |
|      153 |  437 | `		sJump.nLine = pGen->pIn->nLine;` |
|        - |  438 | `		/* Duplicate label name */` |
|      153 |  439 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|      153 |  440 | `		if( zDup == 0 ){` |
|      ! 0 |  441 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  442 | `			return SXERR_ABORT;` |
|        - |  443 | `		}` |
|      153 |  444 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|        - |  445 | `		/* The loop/switch this goto sits in, for the "goto into a loop" check later. */` |
|      153 |  446 | `		sJump.nLoopId = pGen->nCurLoopId;` |
|        - |  447 | `		/* A goto inside a try{}/catch{} is legal php (jumping OUT of the block is fine);` |
|        - |  448 | `		 * only the owning function matters here, since a goto may not cross functions. */` |
|      153 |  449 | `		pBlock = pGen->pCurrent;` |
|      327 |  450 | `		while( pBlock ){` |
|      205 |  451 | `			if( pBlock->iFlags & GEN_BLOCK_FUNC ){` |
|       29 |  452 | `				break;` |
|        - |  453 | `			}` |
|        - |  454 | `			/* Point to the upper block */` |
|      179 |  455 | `			pBlock = pBlock->pParent;` |
|        5 |  456 | `		}` |
|      153 |  457 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|       29 |  458 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       16 |  459 | `		}else{` |
|      127 |  460 | `			sJump.pFunc = 0;` |
|        - |  461 | `		}` |
|        - |  462 | `		/* Emit the unconditional jump */` |
|      153 |  463 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|      153 |  464 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|       74 |  465 | `		}` |
|        - |  466 | `	}` |
|      157 |  467 | `	pGen->pIn++; /* Jump the label name */` |
|      157 |  468 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        3 |  469 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|        1 |  470 | `	}` |
|        - |  471 | `	/* Statement successfully compiled */` |
|      157 |  472 | `	return SXRET_OK;` |
|       81 |  473 | `}` |
|        - |  474 | `/*` |
|        - |  475 | ` * Point to the next PHP chunk that will be processed shortly.` |
|        - |  476 | ` * Return SXRET_OK on success. Any other return value indicates` |
|        - |  477 | ` * failure.` |
|        - |  478 | ` */` |
|       20 |  479 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|        2 |  480 | `{` |
|        - |  481 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|        - |  482 | `	sxu32 nRawObj;` |
|       10 |  483 | `	sxu32 nObjIdx;` |
|        - |  484 | `	/* Consume raw chunks verbatim without any processing until we get` |
|        - |  485 | `	 * a PHP block.` |
|        - |  486 | `	 */` |
|       10 |  487 | `Consume:` |
|       22 |  488 | `	nRawObj = nObjIdx = 0;` |
|       22 |  489 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|      ! 0 |  490 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|      ! 0 |  491 | `		if( pRawObj == 0 ){` |
|      ! 0 |  492 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  493 | `			return SXERR_ABORT;` |
|        - |  494 | `		}` |
|        - |  495 | `		/* Mark as constant and emit the load constant instruction */` |
|      ! 0 |  496 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|      ! 0 |  497 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|      ! 0 |  498 | `		++nRawObj;` |
|      ! 0 |  499 | `		pGen->pRawIn++; /* Next chunk */` |
|      ! 0 |  500 | `	}` |
|       22 |  501 | `	if( nRawObj > 0 ){` |
|        - |  502 | `		/* Emit the consume instruction */` |
|      ! 0 |  503 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|      ! 0 |  504 | `	}` |
|       22 |  505 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|      ! 0 |  506 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|        - |  507 | `		/* Reset the token set (and its trivia sidecar) */` |
|      ! 0 |  508 | `		SySetReset(pTokenSet);` |
|      ! 0 |  509 | `		SySetReset(&pGen->aTrivia);` |
|        - |  510 | `		/* Tokenize input */` |
|      ! 0 |  511 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|      ! 0 |  512 | `			pGen->pRawIn->nLine,pTokenSet,&pGen->aTrivia);` |
|        - |  513 | `		/* Point to the fresh token stream */` |
|      ! 0 |  514 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|      ! 0 |  515 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|        - |  516 | `		/* Advance the stream cursor */` |
|      ! 0 |  517 | `		pGen->pRawIn++;` |
|        - |  518 | `		/* TICKET 1433-011 */` |
|      ! 0 |  519 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|        - |  520 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|        - |  521 | `			sxi32 rc;` |
|        - |  522 | `			/* Refer to TICKET 1433-009  */` |
|      ! 0 |  523 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|      ! 0 |  524 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|      ! 0 |  525 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|        - |  526 | `			/* Synthesized short-tag echo: legitimately an expression here. */` |
|      ! 0 |  527 | `			pGen->nExprEchoOk++;` |
|      ! 0 |  528 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|      ! 0 |  529 | `			pGen->nExprEchoOk--;` |
|      ! 0 |  530 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  531 | `				return SXERR_ABORT;` |
|      ! 0 |  532 | `			}else if( rc != SXERR_EMPTY ){` |
|      ! 0 |  533 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      ! 0 |  534 | `			}` |
|      ! 0 |  535 | `			goto Consume;` |
|        - |  536 | `		}` |
|      ! 0 |  537 | `	}else{` |
|        - |  538 | `		/* No more chunks to process */` |
|       22 |  539 | `		pGen->pIn = pGen->pEnd;` |
|       22 |  540 | `		return SXERR_EOF;` |
|        - |  541 | `	}` |
|      ! 0 |  542 | `	return SXRET_OK;` |
|       12 |  543 | `}` |
|        - |  544 | `/*` |
|        - |  545 | ` * Compile a PHP block.` |
|        - |  546 | ` * A block is simply one or more PHP statements and expressions to compile` |
|        - |  547 | ` * optionally delimited by braces {}.` |
|        - |  548 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|        - |  549 | ` * and this function takes care of generating the appropriate error` |
|        - |  550 | ` * message.` |
|        - |  551 | ` */` |
|  6950016 |  552 | `PH7_PRIVATE sxi32 PH7_CompileBlock(` |
|        - |  553 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |  554 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|        - |  555 | `	)` |
|        5 |  556 | `{` |
|        - |  557 | `	sxi32 rc;` |
|        - |  558 | `	sxu32 nLine;` |
|  6950021 |  559 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  6824341 |  560 | `		nLine = pGen->pIn->nLine;` |
|  6824341 |  561 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  6824341 |  562 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  563 | `			return SXERR_ABORT;` |
|        - |  564 | `		}` |
|  6824341 |  565 | `		pGen->pIn++;` |
|        - |  566 | `		/* Compile until we hit the closing braces '}' */` |
|  9985347 |  567 | `		for(;;){` |
| 19970699 |  568 | `			if( pGen->pIn >= pGen->pEnd ){` |
|       22 |  569 | `				rc = GenStateNextChunk(&(*pGen));` |
|       22 |  570 | `				if (rc == SXERR_ABORT ){` |
|      ! 0 |  571 | `			 	   return SXERR_ABORT;` |
|        - |  572 | `				}` |
|       22 |  573 | `				if( rc == SXERR_EOF ){` |
|        - |  574 | `					/* No more token to process: the block was never closed. php reports` |
|        - |  575 | `					 * the line the '{' was opened on, not where the input ran out. */` |
|       22 |  576 | `					PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"Unclosed '{' on line %u",nLine);` |
|       22 |  577 | `					break;` |
|        - |  578 | `				}` |
|      ! 0 |  579 | `			}` |
| 19970679 |  580 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|        - |  581 | `				/* Closing braces found,break immediately*/` |
|  6824321 |  582 | `				pGen->pIn++;` |
|  6824321 |  583 | `				break;` |
|        - |  584 | `			}` |
|        - |  585 | `			/* Compile a single statement */` |
| 13146363 |  586 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
| 13146363 |  587 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  588 | `				return SXERR_ABORT;` |
|        - |  589 | `			}` |
|        5 |  590 | `		}` |
|  6824341 |  591 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  3537853 |  592 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|      ! 0 |  593 | `		pGen->pIn++;` |
|      ! 0 |  594 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|      ! 0 |  595 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  596 | `			return SXERR_ABORT;` |
|        - |  597 | `		}` |
|        - |  598 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|      ! 0 |  599 | `		for(;;){` |
|      ! 0 |  600 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  601 | `				rc = GenStateNextChunk(&(*pGen));` |
|      ! 0 |  602 | `				if (rc == SXERR_ABORT ){` |
|      ! 0 |  603 | `			 	   return SXERR_ABORT;` |
|        - |  604 | `				}` |
|      ! 0 |  605 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|        - |  606 | `					/* No more token to process */` |
|      ! 0 |  607 | `					if( rc == SXERR_EOF ){` |
|      ! 0 |  608 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|        - |  609 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|      ! 0 |  610 | `					}` |
|      ! 0 |  611 | `					break;` |
|        - |  612 | `				}` |
|      ! 0 |  613 | `			}` |
|      ! 0 |  614 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|        - |  615 | `				sxi32 nKwrd;` |
|        - |  616 | `				/* Keyword found */` |
|      ! 0 |  617 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      ! 0 |  618 | `				if( nKwrd == nKeywordEnd \|\|` |
|      ! 0 |  619 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|        - |  620 | `						/* Delimiter keyword found,break */` |
|      ! 0 |  621 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|      ! 0 |  622 | `							pGen->pIn++; /*  endif;endswitch... */` |
|      ! 0 |  623 | `						}` |
|      ! 0 |  624 | `						break;` |
|        - |  625 | `				}` |
|      ! 0 |  626 | `			}` |
|        - |  627 | `			/* Compile a single statement */` |
|      ! 0 |  628 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|      ! 0 |  629 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  630 | `				return SXERR_ABORT;` |
|        - |  631 | `			}` |
|      ! 0 |  632 | `		}` |
|      ! 0 |  633 | `		GenStateLeaveBlock(&(*pGen),0);` |
|      ! 0 |  634 | `	}else{` |
|        - |  635 | `		/* Compile a single statement */` |
|   125685 |  636 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|   125685 |  637 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  638 | `			return SXERR_ABORT;` |
|        - |  639 | `		}` |
|        - |  640 | `	}` |
|        - |  641 | `	/* Jump trailing semi-colons ';' */` |
|  6950021 |  642 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      ! 0 |  643 | `		pGen->pIn++;` |
|      ! 0 |  644 | `	}` |
|  6950021 |  645 | `	return SXRET_OK;` |
|  3475013 |  646 | `}` |
|        - |  647 | `/*` |
|        - |  648 | ` * Compile the gentle 'while' statement.` |
|        - |  649 | ` * According to the PHP language reference` |
|        - |  650 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|        - |  651 | ` *  The basic form of a while statement is:` |
|        - |  652 | ` *  while (expr)` |
|        - |  653 | ` *   statement` |
|        - |  654 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|        - |  655 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|        - |  656 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|        - |  657 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|        - |  658 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|        - |  659 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|        - |  660 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|        - |  661 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|        - |  662 | ` *  while (expr):` |
|        - |  663 | ` *    statement` |
|        - |  664 | ` *   endwhile;` |
|        - |  665 | ` */` |
|    74168 |  666 | `PH7_PRIVATE sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|        5 |  667 | `{` |
|    74173 |  668 | `	GenBlock *pWhileBlock = 0;` |
|    74173 |  669 | `	SyToken *pTmp,*pEnd = 0;` |
|        - |  670 | `	sxu32 nFalseJump;` |
|        - |  671 | `	sxu32 nLine;` |
|        - |  672 | `	sxi32 rc;` |
|    74173 |  673 | `	nLine = pGen->pIn->nLine;` |
|        - |  674 | `	/* Jump the 'while' keyword */` |
|    74173 |  675 | `	pGen->pIn++;` |
|    74173 |  676 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  677 | `		/* Syntax error */` |
|      ! 0 |  678 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|      ! 0 |  679 | `		if( rc == SXERR_ABORT ){` |
|        - |  680 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  681 | `			return SXERR_ABORT;` |
|        - |  682 | `		}` |
|      ! 0 |  683 | `		goto Synchronize;` |
|        - |  684 | `	}` |
|        - |  685 | `	/* Jump the left parenthesis '(' */` |
|    74173 |  686 | `	pGen->pIn++;` |
|        - |  687 | `	/* Create the loop block */` |
|    74173 |  688 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|    74173 |  689 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  690 | `		return SXERR_ABORT;` |
|        - |  691 | `	}` |
|        - |  692 | `	/* Delimit the condition */` |
|    74173 |  693 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    74173 |  694 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  695 | `		/* Empty expression */` |
|        3 |  696 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|        3 |  697 | `		if( rc == SXERR_ABORT ){` |
|        - |  698 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  699 | `			return SXERR_ABORT;` |
|        - |  700 | `		}` |
|        1 |  701 | `	}` |
|        - |  702 | `	/* Swap token streams */` |
|    74173 |  703 | `	pTmp = pGen->pEnd;` |
|    74173 |  704 | `	pGen->pEnd = pEnd;` |
|        - |  705 | `	/* Compile the expression */` |
|    74173 |  706 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    74173 |  707 | `	if( rc == SXERR_ABORT ){` |
|        - |  708 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  709 | `		return SXERR_ABORT;` |
|        - |  710 | `	}` |
|        - |  711 | `	/* Update token stream */` |
|    74173 |  712 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 |  713 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 |  714 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  715 | `			return SXERR_ABORT;` |
|        - |  716 | `		}` |
|      ! 0 |  717 | `		pGen->pIn++;` |
|      ! 0 |  718 | `	}` |
|        - |  719 | `	/* Synchronize pointers */` |
|    74173 |  720 | `	pGen->pIn  = &pEnd[1];` |
|    74173 |  721 | `	pGen->pEnd = pTmp;` |
|        - |  722 | `	/* Emit the false jump */` |
|    74173 |  723 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|        - |  724 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    74173 |  725 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|        - |  726 | `	/* Compile the loop body */` |
|    74173 |  727 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|    74173 |  728 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  729 | `		return SXERR_ABORT;` |
|        - |  730 | `	}` |
|        - |  731 | `	/* Emit the unconditional jump to the start of the loop */` |
|    74173 |  732 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|        - |  733 | `	/* Fix all jumps now the destination is resolved */` |
|    74173 |  734 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  735 | `	/* Release the loop block */` |
|    74173 |  736 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  737 | `	/* Statement successfully compiled */` |
|    74173 |  738 | `	return SXRET_OK;` |
|      ! 0 |  739 | `Synchronize:` |
|        - |  740 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - |  741 | `	 * compiling this erroneous block.` |
|        - |  742 | `	 */` |
|      ! 0 |  743 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  744 | `		pGen->pIn++;` |
|      ! 0 |  745 | `	}` |
|      ! 0 |  746 | `	return SXRET_OK;` |
|    37089 |  747 | `}` |
|        - |  748 | `/*` |
|        - |  749 | ` * Compile the ugly do..while() statement.` |
|        - |  750 | ` * According to the PHP language reference` |
|        - |  751 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|        - |  752 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|        - |  753 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|        - |  754 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|        - |  755 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|        - |  756 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|        - |  757 | ` *  would end immediately).` |
|        - |  758 | ` *  There is just one syntax for do-while loops:` |
|        - |  759 | ` *  <?php` |
|        - |  760 | ` *  $i = 0;` |
|        - |  761 | ` *  do {` |
|        - |  762 | ` *   echo $i;` |
|        - |  763 | ` *  } while ($i > 0);` |
|        - |  764 | ` * ?>` |
|        - |  765 | ` */` |
|        2 |  766 | `PH7_PRIVATE sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|        1 |  767 | `{` |
|        3 |  768 | `	SyToken *pTmp,*pEnd = 0;` |
|        3 |  769 | `	GenBlock *pDoBlock = 0;` |
|        - |  770 | `	sxu32 nLine;` |
|        - |  771 | `	sxi32 rc;` |
|        3 |  772 | `	nLine = pGen->pIn->nLine;` |
|        - |  773 | `	/* Jump the 'do' keyword */` |
|        3 |  774 | `	pGen->pIn++;` |
|        - |  775 | `	/* Create the loop block */` |
|        3 |  776 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|        3 |  777 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  778 | `		return SXERR_ABORT;` |
|        - |  779 | `	}` |
|        - |  780 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|        3 |  781 | `	pDoBlock->bPostContinue = TRUE;` |
|        3 |  782 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|        3 |  783 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  784 | `		return SXERR_ABORT;` |
|        - |  785 | `	}` |
|        3 |  786 | `	if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 |  787 | `		nLine = pGen->pIn->nLine;` |
|      ! 0 |  788 | `	}` |
|        3 |  789 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|      ! 0 |  790 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|        - |  791 | `			/* Missing 'while' statement */` |
|        3 |  792 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|        3 |  793 | `			if( rc == SXERR_ABORT ){` |
|        - |  794 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  795 | `				return SXERR_ABORT;` |
|        - |  796 | `			}` |
|        3 |  797 | `			goto Synchronize;` |
|        - |  798 | `	}` |
|        - |  799 | `	/* Jump the 'while' keyword */` |
|      ! 0 |  800 | `	pGen->pIn++;` |
|      ! 0 |  801 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  802 | `		/* Syntax error */` |
|      ! 0 |  803 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|      ! 0 |  804 | `		if( rc == SXERR_ABORT ){` |
|        - |  805 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  806 | `			return SXERR_ABORT;` |
|        - |  807 | `		}` |
|      ! 0 |  808 | `		goto Synchronize;` |
|        - |  809 | `	}` |
|        - |  810 | `	/* Jump the left parenthesis '(' */` |
|      ! 0 |  811 | `	pGen->pIn++;` |
|        - |  812 | `	/* Delimit the condition */` |
|      ! 0 |  813 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      ! 0 |  814 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  815 | `		/* Empty expression */` |
|      ! 0 |  816 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|      ! 0 |  817 | `		if( rc == SXERR_ABORT ){` |
|        - |  818 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  819 | `			return SXERR_ABORT;` |
|        - |  820 | `		}` |
|      ! 0 |  821 | `		goto Synchronize;` |
|        - |  822 | `	}` |
|        - |  823 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|      ! 0 |  824 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|        - |  825 | `		JumpFixup *aPost;` |
|        - |  826 | `		VmInstr *pInstr;` |
|        - |  827 | `		sxu32 nJumpDest;` |
|        - |  828 | `		sxu32 n;` |
|      ! 0 |  829 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|      ! 0 |  830 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      ! 0 |  831 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|      ! 0 |  832 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      ! 0 |  833 | `			if( pInstr ){` |
|        - |  834 | `				/* Fix */` |
|      ! 0 |  835 | `				pInstr->iP2 = nJumpDest;` |
|      ! 0 |  836 | `			}` |
|      ! 0 |  837 | `		}` |
|      ! 0 |  838 | `	}` |
|        - |  839 | `	/* Swap token streams */` |
|      ! 0 |  840 | `	pTmp = pGen->pEnd;` |
|      ! 0 |  841 | `	pGen->pEnd = pEnd;` |
|        - |  842 | `	/* Compile the expression */` |
|      ! 0 |  843 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      ! 0 |  844 | `	if( rc == SXERR_ABORT ){` |
|        - |  845 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  846 | `		return SXERR_ABORT;` |
|        - |  847 | `	}` |
|        - |  848 | `	/* Update token stream */` |
|      ! 0 |  849 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 |  850 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 |  851 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  852 | `			return SXERR_ABORT;` |
|        - |  853 | `		}` |
|      ! 0 |  854 | `		pGen->pIn++;` |
|      ! 0 |  855 | `	}` |
|      ! 0 |  856 | `	pGen->pIn  = &pEnd[1];` |
|      ! 0 |  857 | `	pGen->pEnd = pTmp;` |
|        - |  858 | `	/* Emit the true jump to the beginning of the loop */` |
|      ! 0 |  859 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|        - |  860 | `	/* Fix all jumps now the destination is resolved */` |
|      ! 0 |  861 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  862 | `	/* Release the loop block */` |
|      ! 0 |  863 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  864 | `	/* Statement successfully compiled */` |
|      ! 0 |  865 | `	return SXRET_OK;` |
|        1 |  866 | `Synchronize:` |
|        - |  867 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - |  868 | `	 * compiling this erroneous block.` |
|        - |  869 | `	 */` |
|        3 |  870 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  871 | `		pGen->pIn++;` |
|      ! 0 |  872 | `	}` |
|        3 |  873 | `	return SXRET_OK;` |
|        2 |  874 | `}` |
|        - |  875 | `/*` |
|        - |  876 | ` * Compile the complex and powerful 'for' statement.` |
|        - |  877 | ` * According to the PHP language reference` |
|        - |  878 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|        - |  879 | ` *  The syntax of a for loop is:` |
|        - |  880 | ` *  for (expr1; expr2; expr3)` |
|        - |  881 | ` *   statement` |
|        - |  882 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|        - |  883 | ` *  the beginning of the loop.` |
|        - |  884 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|        - |  885 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|        - |  886 | ` *  to FALSE, the execution of the loop ends.` |
|        - |  887 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|        - |  888 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|        - |  889 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|        - |  890 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|        - |  891 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|        - |  892 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|        - |  893 | ` *  of using the for truth expression.` |
|        - |  894 | ` */` |
|   128720 |  895 | `PH7_PRIVATE sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|        5 |  896 | `{` |
|   128725 |  897 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   128725 |  898 | `	GenBlock *pForBlock = 0;` |
|        - |  899 | `	sxu32 nFalseJump;` |
|        - |  900 | `	sxu32 nLine;` |
|        - |  901 | `	sxi32 rc;` |
|   128725 |  902 | `	nLine = pGen->pIn->nLine;` |
|        - |  903 | `	/* Jump the 'for' keyword */` |
|   128725 |  904 | `	pGen->pIn++;` |
|   128725 |  905 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  906 | `		/* Syntax error */` |
|      ! 0 |  907 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|      ! 0 |  908 | `		if( rc == SXERR_ABORT ){` |
|        - |  909 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  910 | `			return SXERR_ABORT;` |
|        - |  911 | `		}` |
|      ! 0 |  912 | `		return SXRET_OK;` |
|        - |  913 | `	}` |
|        - |  914 | `	/* Jump the left parenthesis '(' */` |
|   128725 |  915 | `	pGen->pIn++;` |
|        - |  916 | `	/* Delimit the init-expr;condition;post-expr */` |
|   128725 |  917 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   128725 |  918 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  919 | `		/* Empty expression */` |
|      ! 0 |  920 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|      ! 0 |  921 | `		if( rc == SXERR_ABORT ){` |
|        - |  922 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  923 | `			return SXERR_ABORT;` |
|        - |  924 | `		}` |
|        - |  925 | `		/* Synchronize */` |
|      ! 0 |  926 | `		pGen->pIn = pEnd;` |
|      ! 0 |  927 | `		if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 |  928 | `			pGen->pIn++;` |
|      ! 0 |  929 | `		}` |
|      ! 0 |  930 | `		return SXRET_OK;` |
|        - |  931 | `	}` |
|        - |  932 | `	/* Swap token streams */` |
|   128725 |  933 | `	pTmp = pGen->pEnd;` |
|   128725 |  934 | `	pGen->pEnd = pEnd;` |
|        - |  935 | `	/* for() clauses are the ONLY place php's grammar allows a comma-separated` |
|        - |  936 | `	 * expression list, so the comma operator is permitted for their duration` |
|        - |  937 | `	 * (see GenStateTreeHasComma). A closure body nested inside a clause is` |
|        - |  938 | `	 * compiled through this same window — recorded as a known leniency. */` |
|   128725 |  939 | `	pGen->nCommaExprOk++;` |
|        - |  940 | `	/* Compile initialization expressions if available */` |
|   128725 |  941 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  942 | `	/* Pop operand lvalues */` |
|   128725 |  943 | `	if( rc == SXERR_ABORT ){` |
|        - |  944 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  945 | `		return SXERR_ABORT;` |
|   128725 |  946 | `	}else if( rc != SXERR_EMPTY ){` |
|   117035 |  947 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    58515 |  948 | `	}` |
|   128725 |  949 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  950 | `		/* Syntax error */` |
|      ! 0 |  951 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|      ! 0 |  952 | `		if( rc == SXERR_ABORT ){` |
|        - |  953 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  954 | `			return SXERR_ABORT;` |
|        - |  955 | `		}` |
|      ! 0 |  956 | `		return SXRET_OK;` |
|        - |  957 | `	}` |
|        - |  958 | `	/* Jump the trailing ';' */` |
|   128725 |  959 | `	pGen->pIn++;` |
|        - |  960 | `	/* Create the loop block */` |
|   128725 |  961 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   128725 |  962 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  963 | `		return SXERR_ABORT;` |
|        - |  964 | `	}` |
|        - |  965 | `	/* Deffer continue jumps */` |
|   128725 |  966 | `	pForBlock->bPostContinue = TRUE;` |
|        - |  967 | `	/* Compile the condition */` |
|   128725 |  968 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   128725 |  969 | `	if( rc == SXERR_ABORT ){` |
|        - |  970 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  971 | `		return SXERR_ABORT;` |
|   128725 |  972 | `	}else if( rc != SXERR_EMPTY ){` |
|        - |  973 | `		/* Emit the false jump */` |
|   117035 |  974 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|        - |  975 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   117035 |  976 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    58515 |  977 | `	}` |
|   128725 |  978 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  979 | `		/* Syntax error */` |
|        6 |  980 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|        6 |  981 | `		if( rc == SXERR_ABORT ){` |
|        - |  982 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  983 | `			return SXERR_ABORT;` |
|        - |  984 | `		}` |
|        6 |  985 | `		return SXRET_OK;` |
|        - |  986 | `	}` |
|        - |  987 | `	/* Jump the trailing ';' */` |
|   128721 |  988 | `	pGen->pIn++;` |
|        - |  989 | `	/* Save the post condition stream */` |
|   128721 |  990 | `	pPostStart = pGen->pIn;` |
|        - |  991 | `	/* Compile the loop body — OUTSIDE the comma window (the body is ordinary` |
|        - |  992 | ``	 * php, so `(1, 2)` inside it is the parse error it should be). */`` |
|   128721 |  993 | `	pGen->nCommaExprOk--;` |
|   128721 |  994 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   128721 |  995 | `	pGen->pEnd = pTmp;` |
|   128721 |  996 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   128721 |  997 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  998 | `		return SXERR_ABORT;` |
|        - |  999 | `	}` |
|        - | 1000 | `	/* Fix post-continue jumps */` |
|   128721 | 1001 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|        - | 1002 | `		JumpFixup *aPost;` |
|        - | 1003 | `		VmInstr *pInstr;` |
|        - | 1004 | `		sxu32 nJumpDest;` |
|        - | 1005 | `		sxu32 n;` |
|    11705 | 1006 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|    11705 | 1007 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|    38989 | 1008 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|    27289 | 1009 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|    27289 | 1010 | `			if( pInstr ){` |
|        - | 1011 | `				/* Fix jump */` |
|    27289 | 1012 | `				pInstr->iP2 = nJumpDest;` |
|    13642 | 1013 | `			}` |
|    13647 | 1014 | `		}` |
|     5850 | 1015 | `	}` |
|        - | 1016 | `	/* compile the post-expressions if available */` |
|   128721 | 1017 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|      ! 0 | 1018 | `		pPostStart++;` |
|      ! 0 | 1019 | `	}` |
|   128721 | 1020 | `	if( pPostStart < pEnd ){` |
|        - | 1021 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   117033 | 1022 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   117033 | 1023 | `		pGen->nCommaExprOk++; /* post-expressions are a clause list again */` |
|   117033 | 1024 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   117033 | 1025 | `		pGen->nCommaExprOk--;` |
|   117033 | 1026 | `		if( pGen->pIn < pGen->pEnd ){` |
|        - | 1027 | `			/* Syntax error */` |
|      ! 0 | 1028 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|      ! 0 | 1029 | `			if( rc == SXERR_ABORT ){` |
|        - | 1030 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 1031 | `				return SXERR_ABORT;` |
|        - | 1032 | `			}` |
|      ! 0 | 1033 | `			return SXRET_OK;` |
|        - | 1034 | `		}` |
|   117033 | 1035 | `		RE_SWAP_DELIMITER(pGen);` |
|   117033 | 1036 | `		if( rc == SXERR_ABORT ){` |
|        - | 1037 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1038 | `			return SXERR_ABORT;` |
|   117033 | 1039 | `		}else if( rc != SXERR_EMPTY){` |
|        - | 1040 | `			/* Pop operand lvalue */` |
|   117033 | 1041 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    58514 | 1042 | `		}` |
|    58514 | 1043 | `	}` |
|        - | 1044 | `	/* Emit the unconditional jump to the start of the loop */` |
|   128721 | 1045 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|        - | 1046 | `	/* Fix all jumps now the destination is resolved */` |
|   128721 | 1047 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 1048 | `	/* Release the loop block */` |
|   128721 | 1049 | `	GenStateLeaveBlock(pGen,0);` |
|        - | 1050 | `	/* Statement successfully compiled */` |
|   128721 | 1051 | `	return SXRET_OK;` |
|    64365 | 1052 | `}` |
|        - | 1053 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|        - | 1054 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|        - | 1055 | ` * are allowed.` |
|        - | 1056 | ` */` |
|   464768 | 1057 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 1058 | `{` |
|   464773 | 1059 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|   464773 | 1060 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|        - | 1061 | `		/* Unexpected expression */` |
|      ! 0 | 1062 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 1063 | `			"foreach: Expecting a variable name");` |
|      ! 0 | 1064 | `		if( rc != SXERR_ABORT ){` |
|      ! 0 | 1065 | `			rc = SXERR_INVALID;` |
|      ! 0 | 1066 | `		}` |
|      ! 0 | 1067 | `	}` |
|   464773 | 1068 | `	return rc;` |
|        5 | 1069 | `}` |
|        - | 1070 | `/*` |
|        - | 1071 | ` * Compile the 'foreach' statement.` |
|        - | 1072 | ` * According to the PHP language reference` |
|        - | 1073 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|        - | 1074 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|        - | 1075 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|        - | 1076 | ` *  is a minor but useful extension of the first:` |
|        - | 1077 | ` *  foreach (array_expression as $value)` |
|        - | 1078 | ` *    statement` |
|        - | 1079 | ` *  foreach (array_expression as $key => $value)` |
|        - | 1080 | ` *   statement` |
|        - | 1081 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|        - | 1082 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|        - | 1083 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|        - | 1084 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|        - | 1085 | ` *  to the variable $key on each loop.` |
|        - | 1086 | ` *  Note:` |
|        - | 1087 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|        - | 1088 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|        - | 1089 | ` *  Note:` |
|        - | 1090 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|        - | 1091 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|        - | 1092 | ` *  or after the foreach without resetting it.` |
|        - | 1093 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|        - | 1094 | ` *  of copying the value.` |
|        - | 1095 | ` */` |
|   328108 | 1096 | `PH7_PRIVATE sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|        5 | 1097 | `{` |
|   328113 | 1098 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|   328113 | 1099 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|   328113 | 1100 | `	GenBlock *pForeachBlock = 0;` |
|        - | 1101 | `	ph7_foreach_info *pInfo;` |
|        - | 1102 | `	sxu32 nFalseJump;` |
|        - | 1103 | `	VmInstr *pInstr;` |
|        - | 1104 | `	sxu32 nLine;` |
|        - | 1105 | `	sxi32 rc;` |
|   328113 | 1106 | `	nLine = pGen->pIn->nLine;` |
|        - | 1107 | `	/* Jump the 'foreach' keyword */` |
|   328113 | 1108 | `	pGen->pIn++;` |
|   328113 | 1109 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1110 | `		/* Syntax error */` |
|      ! 0 | 1111 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|      ! 0 | 1112 | `		if( rc == SXERR_ABORT ){` |
|        - | 1113 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1114 | `			return SXERR_ABORT;` |
|        - | 1115 | `		}` |
|      ! 0 | 1116 | `		goto Synchronize;` |
|        - | 1117 | `	}` |
|        - | 1118 | `	/* Jump the left parenthesis '(' */` |
|   328113 | 1119 | `	pGen->pIn++;` |
|        - | 1120 | `	/* Create the loop block */` |
|   328113 | 1121 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|   328113 | 1122 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1123 | `		return SXERR_ABORT;` |
|        - | 1124 | `	}` |
|        - | 1125 | `	/* Delimit the expression */` |
|   328113 | 1126 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   328113 | 1127 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - | 1128 | `		/* Empty expression */` |
|      ! 0 | 1129 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|      ! 0 | 1130 | `		if( rc == SXERR_ABORT ){` |
|        - | 1131 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1132 | `			return SXERR_ABORT;` |
|        - | 1133 | `		}` |
|        - | 1134 | `		/* Synchronize */` |
|      ! 0 | 1135 | `		pGen->pIn = pEnd;` |
|      ! 0 | 1136 | `		if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 | 1137 | `			pGen->pIn++;` |
|      ! 0 | 1138 | `		}` |
|      ! 0 | 1139 | `		return SXRET_OK;` |
|        - | 1140 | `	}` |
|        - | 1141 | `	/* Compile the array expression */` |
|   328113 | 1142 | `	pCur = pGen->pIn;` |
|  1859673 | 1143 | `	while( pCur < pEnd ){` |
|  1859673 | 1144 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|   359303 | 1145 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|   359303 | 1146 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|        - | 1147 | `				/* Break with the first 'as' found */` |
|   328113 | 1148 | `				break;` |
|        - | 1149 | `			}` |
|    15595 | 1150 | `		}` |
|        - | 1151 | `		/* Advance the stream cursor */` |
|  1531565 | 1152 | `		pCur++;` |
|        5 | 1153 | `	}` |
|   328113 | 1154 | `	if( pCur <= pGen->pIn ){` |
|      ! 0 | 1155 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 1156 | `			"foreach: Missing array/object expression");` |
|      ! 0 | 1157 | `		if( rc == SXERR_ABORT ){` |
|        - | 1158 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1159 | `			return SXERR_ABORT;` |
|        - | 1160 | `		}` |
|      ! 0 | 1161 | `		goto Synchronize;` |
|        - | 1162 | `	}` |
|        - | 1163 | `	/* Swap token streams */` |
|   328113 | 1164 | `	pTmp = pGen->pEnd;` |
|   328113 | 1165 | `	pGen->pEnd = pCur;` |
|   328113 | 1166 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   328113 | 1167 | `	if( rc == SXERR_ABORT ){` |
|        - | 1168 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1169 | `		return SXERR_ABORT;` |
|        - | 1170 | `	}` |
|        - | 1171 | `	/* Update token stream */` |
|   328113 | 1172 | `	while(pGen->pIn < pCur ){` |
|      ! 0 | 1173 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 1174 | `		if( rc == SXERR_ABORT ){` |
|        - | 1175 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1176 | `			return SXERR_ABORT;` |
|        - | 1177 | `		}` |
|      ! 0 | 1178 | `		pGen->pIn++;` |
|      ! 0 | 1179 | `	}` |
|   328113 | 1180 | `	pCur++; /* Jump the 'as' keyword */` |
|   328113 | 1181 | `	pGen->pIn = pCur;` |
|   328113 | 1182 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 1183 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|      ! 0 | 1184 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1185 | `			return SXERR_ABORT;` |
|        - | 1186 | `		}` |
|      ! 0 | 1187 | `	}` |
|        - | 1188 | `	/* Create the foreach context */` |
|   328113 | 1189 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|   328113 | 1190 | `	if( pInfo == 0 ){` |
|      ! 0 | 1191 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|      ! 0 | 1192 | `		return SXERR_ABORT;` |
|        - | 1193 | `	}` |
|        - | 1194 | `	/* Zero the structure */` |
|   328113 | 1195 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|        - | 1196 | `	/* Initialize structure fields */` |
|   328113 | 1197 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|        - | 1198 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|        - | 1199 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|        - | 1200 | `	 * '=>'. */` |
|   328113 | 1201 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|   328113 | 1202 | `	if( pCur < pEnd ){` |
|        - | 1203 | `		/* Compile the expression holding the key name */` |
|   136689 | 1204 | `		if( pGen->pIn >= pCur ){` |
|      ! 0 | 1205 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|      ! 0 | 1206 | `			if( rc == SXERR_ABORT ){` |
|        - | 1207 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1208 | `				return SXERR_ABORT;` |
|        - | 1209 | `			}` |
|      ! 0 | 1210 | `		}else{` |
|   136689 | 1211 | `			pGen->pEnd = pCur;` |
|   136689 | 1212 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|   136689 | 1213 | `			if( rc == SXERR_ABORT ){` |
|        - | 1214 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1215 | `				return SXERR_ABORT;` |
|        - | 1216 | `			}` |
|   136689 | 1217 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|   136689 | 1218 | `			if( pInstr->p3 ){` |
|        - | 1219 | `				/* Record key name */` |
|   136689 | 1220 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    68342 | 1221 | `			}` |
|   136689 | 1222 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|        - | 1223 | `		}` |
|   136689 | 1224 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    68342 | 1225 | `	}` |
|   328113 | 1226 | `	pGen->pEnd = pEnd;` |
|   328113 | 1227 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 1228 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|      ! 0 | 1229 | `		if( rc == SXERR_ABORT ){` |
|        - | 1230 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1231 | `			return SXERR_ABORT;` |
|        - | 1232 | `		}` |
|      ! 0 | 1233 | `		goto Synchronize;` |
|        - | 1234 | `	}` |
|   328113 | 1235 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|       33 | 1236 | `		pGen->pIn++;` |
|        - | 1237 | `		/* Pass by reference  */` |
|       33 | 1238 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       15 | 1239 | `	}` |
|        - | 1240 | `	/* Check if the value target is list() */` |
|   328113 | 1241 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        8 | 1242 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|        - | 1243 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|        - | 1244 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|        - | 1245 | `		 */` |
|        - | 1246 | `		static int iForeachListCnt = 0;` |
|        - | 1247 | `		char zTmp[128];` |
|        - | 1248 | `		sxu32 nLen;` |
|        - | 1249 | `		char *zDup;` |
|       10 | 1250 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|       10 | 1251 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       10 | 1252 | `		if( zDup == 0 ){` |
|      ! 0 | 1253 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1254 | `			return SXERR_ABORT;` |
|        - | 1255 | `		}` |
|       10 | 1256 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|        - | 1257 | `		/* Save list() token boundaries */` |
|       10 | 1258 | `		pListStart = pGen->pIn;` |
|        - | 1259 | `		/* Advance past list(...) — validate parentheses */` |
|       10 | 1260 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|       10 | 1261 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        3 | 1262 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|        - | 1263 | `				"foreach: Expected '(' after 'list'");` |
|        3 | 1264 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1265 | `				return SXERR_ABORT;` |
|        - | 1266 | `			}` |
|        3 | 1267 | `			goto Synchronize;` |
|        - | 1268 | `		}` |
|        7 | 1269 | `		pGen->pIn++; /* Jump '(' */` |
|        7 | 1270 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|        7 | 1271 | `		if( pListEnd >= pEnd ){` |
|      ! 0 | 1272 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1273 | `				"foreach: Missing closing ')' after list");` |
|      ! 0 | 1274 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1275 | `				return SXERR_ABORT;` |
|        - | 1276 | `			}` |
|      ! 0 | 1277 | `			goto Synchronize;` |
|        - | 1278 | `		}` |
|        7 | 1279 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|        7 | 1280 | `		pListEnd = pGen->pIn;` |
|        7 | 1281 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|   328108 | 1282 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|        - | 1283 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|        - | 1284 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|        - | 1285 | `		 */` |
|        - | 1286 | `		static int iForeachShortListCnt = 0;` |
|        - | 1287 | `		char zTmp[128];` |
|        - | 1288 | `		sxu32 nLen;` |
|        - | 1289 | `		char *zDup;` |
|       17 | 1290 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       17 | 1291 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       17 | 1292 | `		if( zDup == 0 ){` |
|      ! 0 | 1293 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1294 | `			return SXERR_ABORT;` |
|        - | 1295 | `		}` |
|       17 | 1296 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|        - | 1297 | `		/* Save [...] token boundaries */` |
|       17 | 1298 | `		pListStart = pGen->pIn;` |
|        - | 1299 | `		/* Advance past [...] */` |
|       17 | 1300 | `		pGen->pIn++; /* Jump '[' */` |
|       17 | 1301 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       17 | 1302 | `		if( pListEnd >= pEnd ){` |
|      ! 0 | 1303 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1304 | `				"foreach: Missing closing ']' after short list");` |
|      ! 0 | 1305 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1306 | `				return SXERR_ABORT;` |
|        - | 1307 | `			}` |
|      ! 0 | 1308 | `			goto Synchronize;` |
|        - | 1309 | `		}` |
|       17 | 1310 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       17 | 1311 | `		pListEnd = pGen->pIn;` |
|       17 | 1312 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|        9 | 1313 | `	}else{` |
|        - | 1314 | `		/* Compile the expression holding the value name */` |
|   328089 | 1315 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|   328089 | 1316 | `		if( rc == SXERR_ABORT ){` |
|        - | 1317 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1318 | `			return SXERR_ABORT;` |
|        - | 1319 | `		}` |
|   328089 | 1320 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|   328089 | 1321 | `		if( pInstr->p3 ){` |
|        - | 1322 | `			/* Record value name */` |
|   328089 | 1323 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|   164042 | 1324 | `		}` |
|        - | 1325 | `	}` |
|        - | 1326 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|   328111 | 1327 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|        - | 1328 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   328111 | 1329 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|        - | 1330 | `	/* Record the first instruction to execute */` |
|   328111 | 1331 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|        - | 1332 | `	/* Emit the FOREACH_STEP instruction */` |
|   328111 | 1333 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|        - | 1334 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   328111 | 1335 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|        - | 1336 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|   328111 | 1337 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|        - | 1338 | `		SyToken *pSavedIn,*pSavedEnd;` |
|        - | 1339 | `		/* Load the temporary variable holding the current value onto the stack.` |
|        - | 1340 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|        - | 1341 | `		 */` |
|       23 | 1342 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|        - | 1343 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|        - | 1344 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|        - | 1345 | `		 * picks up the delimiter and the variable names inside.` |
|        - | 1346 | `		 */` |
|       23 | 1347 | `		pSavedIn = pGen->pIn;` |
|       23 | 1348 | `		pSavedEnd = pGen->pEnd;` |
|       23 | 1349 | `		pGen->pIn = pListStart;` |
|       23 | 1350 | `		pGen->pEnd = pListEnd;` |
|       23 | 1351 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       17 | 1352 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|        9 | 1353 | `		}else{` |
|        7 | 1354 | `			rc = PH7_CompileList(&(*pGen),0);` |
|        - | 1355 | `		}` |
|       23 | 1356 | `		pGen->pIn = pSavedIn;` |
|       23 | 1357 | `		pGen->pEnd = pSavedEnd;` |
|       23 | 1358 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1359 | `			return SXERR_ABORT;` |
|        - | 1360 | `		}` |
|        - | 1361 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|       23 | 1362 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       11 | 1363 | `	}` |
|        - | 1364 | `	/* Compile the loop body */` |
|   328111 | 1365 | `	pGen->pIn = &pEnd[1];` |
|   328111 | 1366 | `	pGen->pEnd = pTmp;` |
|   328111 | 1367 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|   328111 | 1368 | `	if( rc == SXERR_ABORT ){` |
|        - | 1369 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1370 | `		return SXERR_ABORT;` |
|        - | 1371 | `	}` |
|        - | 1372 | `	/* Emit the unconditional jump to the start of the loop */` |
|   328111 | 1373 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|        - | 1374 | `	/* Fix all jumps now the destination is resolved */` |
|   328111 | 1375 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 1376 | `	/* Release the loop block */` |
|   328111 | 1377 | `	GenStateLeaveBlock(pGen,0);` |
|        - | 1378 | `	/* Statement successfully compiled */` |
|   328111 | 1379 | `	return SXRET_OK;` |
|        1 | 1380 | `Synchronize:` |
|        - | 1381 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - | 1382 | `	 * compiling this erroneous block.` |
|        - | 1383 | `	 */` |
|        3 | 1384 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 | 1385 | `		pGen->pIn++;` |
|      ! 0 | 1386 | `	}` |
|        3 | 1387 | `	return SXRET_OK;` |
|   164059 | 1388 | `}` |
|        - | 1389 | `/*` |
|        - | 1390 | ` * Compile the infamous if/elseif/else if/else statements.` |
|        - | 1391 | ` * According to the PHP language reference` |
|        - | 1392 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|        - | 1393 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|        - | 1394 | ` *  that is similar to that of C:` |
|        - | 1395 | ` *  if (expr)` |
|        - | 1396 | ` *   statement` |
|        - | 1397 | ` *  else construct:` |
|        - | 1398 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|        - | 1399 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|        - | 1400 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|        - | 1401 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|        - | 1402 | ` *   $b, and a is NOT greater than b otherwise.` |
|        - | 1403 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|        - | 1404 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|        - | 1405 | ` *  elseif` |
|        - | 1406 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|        - | 1407 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|        - | 1408 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|        - | 1409 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|        - | 1410 | ` *   than b, a equal to b or a is smaller than b:` |
|        - | 1411 | ` *   <?php` |
|        - | 1412 | ` *    if ($a > $b) {` |
|        - | 1413 | ` *     echo "a is bigger than b";` |
|        - | 1414 | ` *    } elseif ($a == $b) {` |
|        - | 1415 | ` *     echo "a is equal to b";` |
|        - | 1416 | ` *    } else {` |
|        - | 1417 | ` *     echo "a is smaller than b";` |
|        - | 1418 | ` *    }` |
|        - | 1419 | ` *    ?>` |
|        - | 1420 | ` */` |
|  2436686 | 1421 | `PH7_PRIVATE sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|        5 | 1422 | `{` |
|  2436691 | 1423 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  2436691 | 1424 | `	GenBlock *pCondBlock = 0;` |
|        - | 1425 | `	sxu32 nJumpIdx;` |
|        - | 1426 | `	sxu32 nKeyID;` |
|        - | 1427 | `	sxi32 rc;` |
|        - | 1428 | `	/* Jump the 'if' keyword */` |
|  2436691 | 1429 | `	pGen->pIn++;` |
|  2436691 | 1430 | `	pToken = pGen->pIn;` |
|        - | 1431 | `	/* Create the conditional block */` |
|  2436691 | 1432 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  2436691 | 1433 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1434 | `		return SXERR_ABORT;` |
|        - | 1435 | `	}` |
|        - | 1436 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|  1362566 | 1437 | `	for(;;){` |
|  2725137 | 1438 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1439 | `			/* Syntax error */` |
|      ! 0 | 1440 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 1441 | `				pToken--;` |
|      ! 0 | 1442 | `			}` |
|      ! 0 | 1443 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|      ! 0 | 1444 | `			if( rc == SXERR_ABORT ){` |
|        - | 1445 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 1446 | `				return SXERR_ABORT;` |
|        - | 1447 | `			}` |
|      ! 0 | 1448 | `			goto Synchronize;` |
|        - | 1449 | `		}` |
|        - | 1450 | `		/* Jump the left parenthesis '(' */` |
|  2725137 | 1451 | `		pToken++;` |
|        - | 1452 | `		/* Delimit the condition */` |
|  2725137 | 1453 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  2725137 | 1454 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|        - | 1455 | `			/* Syntax error */` |
|        6 | 1456 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 1457 | `				pToken--;` |
|      ! 0 | 1458 | `			}` |
|        6 | 1459 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|        6 | 1460 | `			if( rc == SXERR_ABORT ){` |
|        - | 1461 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 1462 | `				return SXERR_ABORT;` |
|        - | 1463 | `			}` |
|        6 | 1464 | `			goto Synchronize;` |
|        - | 1465 | `		}` |
|        - | 1466 | `		/* Swap token streams */` |
|  2725133 | 1467 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|        - | 1468 | `		/* Compile the condition */` |
|  2725133 | 1469 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 1470 | `		/* Update token stream */` |
|  2725133 | 1471 | `		while(pGen->pIn < pEnd ){` |
|      ! 0 | 1472 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 | 1473 | `			pGen->pIn++;` |
|      ! 0 | 1474 | `		}` |
|  2725133 | 1475 | `		pGen->pIn  = &pEnd[1];` |
|  2725133 | 1476 | `		pGen->pEnd = pTmp;` |
|  2725133 | 1477 | `		if( rc == SXERR_ABORT ){` |
|        - | 1478 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|        3 | 1479 | `			return SXERR_ABORT;` |
|        - | 1480 | `		}` |
|        - | 1481 | `		/* Emit the false jump */` |
|  2725131 | 1482 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|        - | 1483 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  2725131 | 1484 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|        - | 1485 | `		/* Compile the body */` |
|  2725131 | 1486 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  2725131 | 1487 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1488 | `			return SXERR_ABORT;` |
|        - | 1489 | `		}` |
|  2725131 | 1490 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   522551 | 1491 | `			break;` |
|        - | 1492 | `		}` |
|        - | 1493 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|  1680039 | 1494 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  1680039 | 1495 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|  1169103 | 1496 | `			break;` |
|        - | 1497 | `		}` |
|        - | 1498 | `		/* Emit the unconditional jump */` |
|   510941 | 1499 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|        - | 1500 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   510941 | 1501 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   510941 | 1502 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   308207 | 1503 | `			pToken = &pGen->pIn[1];` |
|   308207 | 1504 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    85750 | 1505 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|   111250 | 1506 | `					break;` |
|        - | 1507 | `			}` |
|    85717 | 1508 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    42856 | 1509 | `		}` |
|   288451 | 1510 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|        - | 1511 | `		/* Synchronize cursors */` |
|   288451 | 1512 | `		pToken = pGen->pIn;` |
|        - | 1513 | `		/* Fix the false jump */` |
|   288451 | 1514 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|        5 | 1515 | `	} /* For(;;) */` |
|        - | 1516 | `	/* Fix the false jump */` |
|  2436685 | 1517 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  2436685 | 1518 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|  1391588 | 1519 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|        - | 1520 | `			/* Compile the else block */` |
|   222495 | 1521 | `			pGen->pIn++;` |
|   222495 | 1522 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   222495 | 1523 | `			if( rc == SXERR_ABORT ){` |
|        - | 1524 |  |
|      ! 0 | 1525 | `				return SXERR_ABORT;` |
|        - | 1526 | `			}` |
|   111245 | 1527 | `	}` |
|  2436685 | 1528 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|        - | 1529 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  2436685 | 1530 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|        - | 1531 | `	/* Release the conditional block */` |
|  2436685 | 1532 | `	GenStateLeaveBlock(pGen,0);` |
|        - | 1533 | `	/* Statement successfully compiled */` |
|  2436685 | 1534 | `	return SXRET_OK;` |
|        2 | 1535 | `Synchronize:` |
|        - | 1536 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|        - | 1537 | `	 */` |
|       34 | 1538 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       30 | 1539 | `		pGen->pIn++;` |
|        2 | 1540 | `	}` |
|        6 | 1541 | `	return SXRET_OK;` |
|  1218348 | 1542 | `}` |
|        - | 1543 | `/*` |
|        - | 1544 | ` * Compile the global construct.` |
|        - | 1545 | ` * According to the PHP language reference` |
|        - | 1546 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|        - | 1547 | ` *  to be used in that function.` |
|        - | 1548 | ` *  Example #1 Using global` |
|        - | 1549 | ` *  <?php` |
|        - | 1550 | ` *   $a = 1;` |
|        - | 1551 | ` *   $b = 2;` |
|        - | 1552 | ` *   function Sum()` |
|        - | 1553 | ` *   {` |
|        - | 1554 | ` *    global $a, $b;` |
|        - | 1555 | ` *    $b = $a + $b;` |
|        - | 1556 | ` *   }` |
|        - | 1557 | ` *   Sum();` |
|        - | 1558 | ` *   echo $b;` |
|        - | 1559 | ` *  ?>` |
|        - | 1560 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|        - | 1561 | ` *  all references to either variable will refer to the global version. There is no limit` |
|        - | 1562 | ` *  to the number of global variables that can be manipulated by a function.` |
|        - | 1563 | ` */` |
|       36 | 1564 | `PH7_PRIVATE sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|        5 | 1565 | `{` |
|       41 | 1566 | `	SyToken *pTmp,*pNext = 0;` |
|        - | 1567 | `	sxi32 nExpr;` |
|        - | 1568 | `	sxi32 rc;` |
|        - | 1569 | `	/* Jump the 'global' keyword */` |
|       41 | 1570 | `	pGen->pIn++;` |
|       41 | 1571 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|        - | 1572 | `		/* Nothing to process */` |
|      ! 0 | 1573 | `		return SXRET_OK;` |
|        - | 1574 | `	}` |
|       41 | 1575 | `	pTmp = pGen->pEnd;` |
|       41 | 1576 | `	nExpr = 0;` |
|       87 | 1577 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       51 | 1578 | `		if( pGen->pIn < pNext ){` |
|       51 | 1579 | `			pGen->pEnd = pNext;` |
|       51 | 1580 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 | 1581 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|      ! 0 | 1582 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1583 | `					return SXERR_ABORT;` |
|        - | 1584 | `				}` |
|      ! 0 | 1585 | `			}else{` |
|       51 | 1586 | `				pGen->pIn++;` |
|       51 | 1587 | `				if( pGen->pIn >= pGen->pEnd ){` |
|        - | 1588 | `					/* Emit a warning */` |
|      ! 0 | 1589 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|      ! 0 | 1590 | `				}else{` |
|       51 | 1591 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       51 | 1592 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 1593 | `						return SXERR_ABORT;` |
|       51 | 1594 | `					}else if(rc != SXERR_EMPTY ){` |
|       51 | 1595 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|       51 | 1596 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|        - | 1597 | `							/* Variable name, not a constant */` |
|       51 | 1598 | `							pLast->iP1 = 0;` |
|       23 | 1599 | `						}` |
|       51 | 1600 | `						nExpr++;` |
|       23 | 1601 | `					}` |
|        - | 1602 | `				}` |
|        - | 1603 | `			}` |
|       23 | 1604 | `		}` |
|        - | 1605 | `		/* Next expression in the stream */` |
|       51 | 1606 | `		pGen->pIn = pNext;` |
|        - | 1607 | `		/* Jump trailing commas */` |
|       61 | 1608 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       15 | 1609 | `			pGen->pIn++;` |
|        5 | 1610 | `		}` |
|        5 | 1611 | `	}` |
|        - | 1612 | `	/* Restore token stream */` |
|       41 | 1613 | `	pGen->pEnd = pTmp;` |
|       41 | 1614 | `	if( nExpr > 0 ){` |
|        - | 1615 | `		/* Emit the uplink instruction */` |
|       41 | 1616 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|       18 | 1617 | `	}` |
|       41 | 1618 | `	return SXRET_OK;` |
|       23 | 1619 | `}` |
|        - | 1620 | `/*` |
|        - | 1621 | ` * Compile the return statement.` |
|        - | 1622 | ` * According to the PHP language reference` |
|        - | 1623 | ` *  If called from within a function, the return() statement immediately ends execution` |
|        - | 1624 | ` *  of the current function, and returns its argument as the value of the function call.` |
|        - | 1625 | ` *  return() will also end the execution of an eval() statement or script file.` |
|        - | 1626 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|        - | 1627 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|        - | 1628 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|        - | 1629 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|        - | 1630 | ` *  from within the main script file, then script execution end.` |
|        - | 1631 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|        - | 1632 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|        - | 1633 | ` *  should do so as PHP has less work to do in this case.` |
|        - | 1634 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|        - | 1635 | ` */` |
|  3633542 | 1636 | `PH7_PRIVATE sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|        5 | 1637 | `{` |
|  3633547 | 1638 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|        - | 1639 | `	sxi32 rc;` |
|  3633547 | 1640 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  3633547 | 1641 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|        - | 1642 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|        - | 1643 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|        - | 1644 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|        - | 1645 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|        - | 1646 | `	 * normally below so token processing stays consistent. */` |
|  9566551 | 1647 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|  5933009 | 1648 | `		pFuncBlock = pFuncBlock->pParent;` |
|        5 | 1649 | `	}` |
|  3633542 | 1650 | `	if( pFuncBlock && pFuncBlock->pUserData` |
|  3633531 | 1651 | `	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){` |
|        3 | 1652 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1653 | `			"A never-returning function must not return");` |
|        3 | 1654 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1655 | `			return SXERR_ABORT;` |
|        - | 1656 | `		}` |
|        1 | 1657 | `	}` |
|        - | 1658 | `	/* Jump the 'return' keyword */` |
|  3633547 | 1659 | `	pGen->pIn++;` |
|  3633547 | 1660 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - | 1661 | `		/* Compile the expression */` |
|  3536135 | 1662 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  3536135 | 1663 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1664 | `			return SXERR_ABORT;` |
|  3536135 | 1665 | `		}else if(rc != SXERR_EMPTY ){` |
|  3536135 | 1666 | `			nRet = 1;` |
|  1768065 | 1667 | `		}` |
|  1768065 | 1668 | `	}` |
|        - | 1669 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|        - | 1670 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|        - | 1671 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|        - | 1672 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|  3633547 | 1673 | `	if( pGen->bInGenerator ){` |
|     3929 | 1674 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|     3929 | 1675 | `		return SXRET_OK;` |
|        - | 1676 | `	}` |
|        - | 1677 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|        - | 1678 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|        - | 1679 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|        - | 1680 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|        - | 1681 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|  3629623 | 1682 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|  3629623 | 1683 | `	return SXRET_OK;` |
|  1816776 | 1684 | `}` |
|        - | 1685 | `/*` |
|        - | 1686 | ` * Compile a yield expression.` |
|        - | 1687 | ` * Called from the expression code generator when a yield node is encountered.` |
|        - | 1688 | ` * Handles: yield, yield $value, yield $key => $value` |
|        - | 1689 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|        - | 1690 | ` */` |
|    15972 | 1691 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|        5 | 1692 | `{` |
|        - | 1693 | `	SyToken *pTmp, *pSplit;` |
|    15977 | 1694 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|    15977 | 1695 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|        - | 1696 | `	sxi32 rc;` |
|     7986 | 1697 | `	(void)iCompileFlag;` |
|        - | 1698 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|    15977 | 1699 | `	pGen->pIn++;` |
|        - | 1700 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|        - | 1701 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|        - | 1702 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|        - | 1703 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|        - | 1704 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|    15972 | 1705 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|     8021 | 1706 | `		&& pGen->pIn->sData.nByte == 4` |
|       72 | 1707 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|       67 | 1708 | `		pGen->pIn++; /* Skip 'from' */` |
|       67 | 1709 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       67 | 1710 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1711 | `			return SXERR_ABORT;` |
|        - | 1712 | `		}` |
|       67 | 1713 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 | 1714 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|      ! 0 | 1715 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|        - | 1716 | `				"Missing expression after 'yield from'");` |
|      ! 0 | 1717 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1718 | `				return SXERR_ABORT;` |
|        - | 1719 | `			}` |
|      ! 0 | 1720 | `		}` |
|       67 | 1721 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|       67 | 1722 | `		return SXRET_OK;` |
|        - | 1723 | `	}` |
|    15915 | 1724 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - | 1725 | `		/* Bare yield — no value */` |
|        3 | 1726 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|        3 | 1727 | `		return SXRET_OK;` |
|        - | 1728 | `	}` |
|        - | 1729 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|    15913 | 1730 | `	pSplit = 0;` |
|        - | 1731 | `	{` |
|    15913 | 1732 | `		SyToken *pCur = pGen->pIn;` |
|    15913 | 1733 | `		sxi32 nNest = 0;` |
|    47543 | 1734 | `		while( pCur < pGen->pEnd ){` |
|    47233 | 1735 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       18 | 1736 | `				nNest++;` |
|    47225 | 1737 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       18 | 1738 | `				nNest--;` |
|    47209 | 1739 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|    15603 | 1740 | `				pSplit = pCur;` |
|    15603 | 1741 | `				break;` |
|        - | 1742 | `			}` |
|    31635 | 1743 | `			pCur++;` |
|        5 | 1744 | `		}` |
|        - | 1745 | `	}` |
|    15913 | 1746 | `	pTmp = pGen->pEnd;` |
|    15913 | 1747 | `	if( pSplit ){` |
|        - | 1748 | `		/* yield $key => $value */` |
|    15603 | 1749 | `		pGen->pEnd = pSplit;` |
|    15603 | 1750 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|    15603 | 1751 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|    15603 | 1752 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|    15603 | 1753 | `		pGen->pEnd = pTmp;` |
|    15603 | 1754 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|    15603 | 1755 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|    15603 | 1756 | `		iP1 = 1;` |
|    15603 | 1757 | `		iP2 = 1;` |
|     7804 | 1758 | `	}else{` |
|        - | 1759 | `		/* yield $value */` |
|      315 | 1760 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      315 | 1761 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      315 | 1762 | `		if( rc != SXERR_EMPTY ){` |
|      315 | 1763 | `			iP1 = 1;` |
|      155 | 1764 | `		}` |
|        - | 1765 | `	}` |
|    15913 | 1766 | `	pGen->pEnd = pTmp;` |
|    15913 | 1767 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|    15913 | 1768 | `	return SXRET_OK;` |
|     7991 | 1769 | `}` |
|        - | 1770 | `/*` |
|        - | 1771 | ` * Compile the die/exit language construct.` |
|        - | 1772 | ` * The role of these constructs is to terminate execution of the script.` |
|        - | 1773 | ` * Shutdown functions will always be executed even if exit() is called.` |
|        - | 1774 | ` */` |
|       94 | 1775 | `PH7_PRIVATE sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|        5 | 1776 | `{` |
|       99 | 1777 | `	sxi32 nExpr = 0;` |
|        - | 1778 | `	sxi32 rc;` |
|        - | 1779 | `	/* Jump the die/exit keyword */` |
|       99 | 1780 | `	pGen->pIn++;` |
|       99 | 1781 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - | 1782 | `		/* Compile the expression */` |
|       99 | 1783 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       99 | 1784 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1785 | `			return SXERR_ABORT;` |
|       99 | 1786 | `		}else if(rc != SXERR_EMPTY ){` |
|       99 | 1787 | `			nExpr = 1;` |
|       47 | 1788 | `		}` |
|       47 | 1789 | `	}` |
|        - | 1790 | `	/* Emit the HALT instruction */` |
|       99 | 1791 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|       99 | 1792 | `	return SXRET_OK;` |
|       52 | 1793 | `}` |
|        - | 1794 | `/*` |
|        - | 1795 | ` * Compile the 'echo' language construct.` |
|        - | 1796 | ` */` |
|    17554 | 1797 | `PH7_PRIVATE sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|        5 | 1798 | `{` |
|    17559 | 1799 | `	SyToken *pTmp,*pNext = 0;` |
|    17559 | 1800 | `	sxu32 nLine = pGen->pIn->nLine;` |
|    17559 | 1801 | `	int nExpr = 0;      /* expressions actually compiled */` |
|    17559 | 1802 | `	int bExpectMore = 1;/* after 'echo' or a comma an expression is REQUIRED */` |
|        - | 1803 | `	sxi32 rc;` |
|        - | 1804 | `	/* Jump the 'echo' keyword */` |
|    17559 | 1805 | `	pGen->pIn++;` |
|        - | 1806 | `	/* Compile arguments one after one */` |
|    17559 | 1807 | `	pTmp = pGen->pEnd;` |
|    45507 | 1808 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|    27959 | 1809 | `		if( pGen->pIn < pNext ){` |
|    27959 | 1810 | `			pGen->pEnd = pNext;` |
|    27959 | 1811 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|    27959 | 1812 | `			if( rc == SXERR_ABORT ){` |
|        5 | 1813 | `				return SXERR_ABORT;` |
|    27955 | 1814 | `			}else if( rc != SXERR_EMPTY ){` |
|        - | 1815 | `				/* Emit the consume instruction */` |
|    27931 | 1816 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    27931 | 1817 | `				nExpr++;` |
|    27931 | 1818 | `				bExpectMore = 0;` |
|    13963 | 1819 | `			}` |
|    13975 | 1820 | `		}` |
|        - | 1821 | `		/* Jump trailing commas (php: exactly one between expressions; a` |
|        - | 1822 | `		 * dangling or doubled comma is a parse error, enforced below) */` |
|    38361 | 1823 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|    10413 | 1824 | `			if( bExpectMore ){` |
|        - | 1825 | `				/* two commas in a row */` |
|        3 | 1826 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,pNext->nLine,` |
|        - | 1827 | `					"syntax error, unexpected token \",\"");` |
|        3 | 1828 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1829 | `			}` |
|    10411 | 1830 | `			bExpectMore = 1;` |
|    10411 | 1831 | `			pNext++;` |
|        5 | 1832 | `		}` |
|    27953 | 1833 | `		pGen->pIn = pNext;` |
|        5 | 1834 | `	}` |
|        - | 1835 | `	/* Restore token stream */` |
|    17553 | 1836 | `	pGen->pEnd = pTmp;` |
|    17553 | 1837 | `	if( nExpr == 0 \|\| bExpectMore ){` |
|        - | 1838 | ``		/* `echo ;` or `echo expr, ;` — php rejects both */`` |
|       32 | 1839 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - | 1840 | `			"syntax error, unexpected token \";\"");` |
|       32 | 1841 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1842 | `	}` |
|    17525 | 1843 | `	return SXRET_OK;` |
|     8782 | 1844 | `}` |
|        - | 1845 | `/*` |
|        - | 1846 | ` * Compile the static statement.` |
|        - | 1847 | ` * According to the PHP language reference` |
|        - | 1848 | ` *  Another important feature of variable scoping is the static variable.` |
|        - | 1849 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|        - | 1850 | ` *  when program execution leaves this scope.` |
|        - | 1851 | ` *  Static variables also provide one way to deal with recursive functions.` |
|        - | 1852 | ` * Symisc eXtension.` |
|        - | 1853 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|        - | 1854 | ` *  the zend engine would allow only simple scalar value.` |
|        - | 1855 | ` *  Example` |
|        - | 1856 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|        - | 1857 | ` *    Refer to the official documentation for more information on this feature.` |
|        - | 1858 | ` */` |
|    11700 | 1859 | `PH7_PRIVATE sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|        5 | 1860 | `{` |
|        - | 1861 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|        - | 1862 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|        - | 1863 | `	GenBlock *pBlock;` |
|        - | 1864 | `	SyString *pName;` |
|        - | 1865 | `	char *zDup;` |
|        - | 1866 | `	sxu32 nLine;` |
|        - | 1867 | `	sxi32 rc;` |
|        - | 1868 | ``	/* `static function () {}` / `static fn () =>` at statement position is an`` |
|        - | 1869 | `	 * EXPRESSION statement (a bare static closure), not a static-variable` |
|        - | 1870 | `	 * declaration — hand it to the expression compiler (php accepts it). */` |
|    11700 | 1871 | `	if( &pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & PH7_TK_KEYWORD)` |
|     5856 | 1872 | `	 && (SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FUNCTION` |
|        1 | 1873 | `	  \|\| SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FN) ){` |
|        3 | 1874 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        3 | 1875 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1876 | `			return SXERR_ABORT;` |
|        3 | 1877 | `		}else if( rc != SXERR_EMPTY ){` |
|        3 | 1878 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        1 | 1879 | `		}` |
|        3 | 1880 | `		return SXRET_OK;` |
|        - | 1881 | `	}` |
|        - | 1882 | `	/* Jump the static keyword */` |
|    11703 | 1883 | `	nLine = pGen->pIn->nLine;` |
|    11703 | 1884 | `	pGen->pIn++;` |
|        - | 1885 | `	/* Extract the enclosing function if any */` |
|    11703 | 1886 | `	pBlock = pGen->pCurrent;` |
|    23401 | 1887 | `	while( pBlock ){` |
|    23401 | 1888 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|    11703 | 1889 | `			break;` |
|        - | 1890 | `		}` |
|        - | 1891 | `		/* Point to the upper block */` |
|    11703 | 1892 | `		pBlock = pBlock->pParent;` |
|        5 | 1893 | `	}` |
|    11703 | 1894 | `	if( pBlock == 0 ){` |
|        - | 1895 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|      ! 0 | 1896 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 | 1897 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|      ! 0 | 1898 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1899 | `				return SXERR_ABORT;` |
|        - | 1900 | `			}` |
|      ! 0 | 1901 | `			goto Synchronize;` |
|        - | 1902 | `		}` |
|        - | 1903 | `		/* Compile the expression holding the variable */` |
|      ! 0 | 1904 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      ! 0 | 1905 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1906 | `			return SXERR_ABORT;` |
|      ! 0 | 1907 | `		}else if( rc != SXERR_EMPTY ){` |
|        - | 1908 | `			/* Emit the POP instruction */` |
|      ! 0 | 1909 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      ! 0 | 1910 | `		}` |
|      ! 0 | 1911 | `		return SXRET_OK;` |
|        - | 1912 | `	}` |
|    11703 | 1913 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        - | 1914 | `	/* Make sure we are dealing with a valid statement */` |
|    11703 | 1915 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|    11696 | 1916 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        3 | 1917 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|        3 | 1918 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1919 | `				return SXERR_ABORT;` |
|        - | 1920 | `			}` |
|        3 | 1921 | `			goto Synchronize;` |
|        - | 1922 | `	}` |
|    11701 | 1923 | `	pGen->pIn++;` |
|        - | 1924 | `	/* Extract variable name */` |
|    11701 | 1925 | `	pName = &pGen->pIn->sData;` |
|    11701 | 1926 | `	pGen->pIn++; /* Jump the var name */` |
|    11701 | 1927 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|      ! 0 | 1928 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 1929 | `		goto Synchronize;` |
|        - | 1930 | `	}` |
|        - | 1931 | `	/* Initialize the structure describing the static variable */` |
|    11701 | 1932 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|    11701 | 1933 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|        - | 1934 | `	/* Duplicate variable name */` |
|    11701 | 1935 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    11701 | 1936 | `	if( zDup == 0 ){` |
|      ! 0 | 1937 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1938 | `		return SXERR_ABORT;` |
|        - | 1939 | `	}` |
|    11701 | 1940 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|        - | 1941 | `	/* Check if we have an expression to compile */` |
|    11701 | 1942 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|        - | 1943 | `		SySet *pInstrContainer;` |
|        - | 1944 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|        - | 1945 | `		 * Static variable can take any complex expression including function` |
|        - | 1946 | `		 * call as their initialization value.` |
|        - | 1947 | `		 * Example:` |
|        - | 1948 | `		 *		static $var = foo(1,4+5,bar());` |
|        - | 1949 | `		 */` |
|    11701 | 1950 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|        - | 1951 | `		/* Swap bytecode container */` |
|    11701 | 1952 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    11701 | 1953 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|        - | 1954 | `		/* Compile the expression */` |
|    11701 | 1955 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 1956 | `		/* Emit the done instruction */` |
|    11701 | 1957 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        - | 1958 | `		/* Restore default bytecode container */` |
|    11701 | 1959 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     5848 | 1960 | `	}` |
|        - | 1961 | `	/* Finally save the compiled static variable in the appropriate container */` |
|    11701 | 1962 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|    11701 | 1963 | `	return SXRET_OK;` |
|        1 | 1964 | `Synchronize:` |
|        - | 1965 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|        - | 1966 | `	 * statement.` |
|        - | 1967 | `	 */` |
|        5 | 1968 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|        3 | 1969 | `		pGen->pIn++;` |
|        1 | 1970 | `	}` |
|        3 | 1971 | `	return SXRET_OK;` |
|     5855 | 1972 | `}` |
|        - | 1973 | `/*` |
|        - | 1974 | ` * Compile the var statement.` |
|        - | 1975 | ` * Symisc Extension:` |
|        - | 1976 | ` *      var statement can be used outside of a class definition.` |
|        - | 1977 | ` */` |
|        2 | 1978 | `PH7_PRIVATE sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|        1 | 1979 | `{` |
|        - | 1980 | `` 	/* `var` is a PROPERTY modifier and nothing else. A class body's `var $x;` `` |
|        - | 1981 | `	 * is compiled by compile_class.c, never here, so reaching this statement` |
|        - | 1982 | ``	 * handler means `var` appeared outside a class — which php rejects as a`` |
|        - | 1983 | `	 * parse error. PHL used to compile it as an ordinary expression statement,` |
|        - | 1984 | ``	 * so top-level `var $x = 1;` silently ran (a Symisc extension). */`` |
|        3 | 1985 | `	PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|        3 | 1986 | `	return SXERR_ABORT;` |
|        1 | 1987 | `}` |
|        - | 1988 | `/*` |
|        - | 1989 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|        - | 1990 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|        - | 1991 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|        - | 1992 | ` */` |
|        - | 1993 | `/*` |
|        - | 1994 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|        - | 1995 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|        - | 1996 | ` * hash and any shared references), this creates a new literal entry with the` |
|        - | 1997 | ` * qualified name and updates the instruction's operand index.` |
|        - | 1998 | ` *` |
|        - | 1999 | ` * Resolution order:` |
|        - | 2000 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|        - | 2001 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|        - | 2002 | ` *   3. Otherwise return the original literal index unchanged.` |
|        - | 2003 | ` *` |
|        - | 2004 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|        - | 2005 | ` * came from an import (step 1) and 0 otherwise.` |
|        - | 2006 | ` * Returns the (possibly new) literal index.` |
|        - | 2007 | ` */` |
|  6458536 | 2008 | `PH7_PRIVATE sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|        5 | 2009 | `{` |
|        - | 2010 | `	ph7_value *pLit;` |
|        - | 2011 | `	const char *zLit;` |
|        - | 2012 | `	SyString sQualified;` |
|        - | 2013 | `	sxu32 nLit;` |
|        - | 2014 | `	sxu32 k;` |
|        - | 2015 | `	sxu32 nNewIdx;` |
|        - | 2016 | `	int hasNsSep;` |
|        - | 2017 | `	SyHashEntry *pImport;` |
|        - | 2018 | `	ph7_value *pNew;` |
|  6458541 | 2019 | `	if( pFromImport ){` |
|  5266009 | 2020 | `		*pFromImport = 0;` |
|  2633002 | 2021 | `	}` |
|  6458541 | 2022 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  6458541 | 2023 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|      ! 0 | 2024 | `		return nOrigIdx;` |
|        - | 2025 | `	}` |
|  6458541 | 2026 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  6458541 | 2027 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|        - | 2028 | `	/* Skip if already qualified (contains backslash) */` |
|  6458541 | 2029 | `	hasNsSep = 0;` |
| 78096353 | 2030 | `	for( k = 0; k < nLit; k++ ){` |
| 71637843 | 2031 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 35818911 | 2032 | `	}` |
|  6458541 | 2033 | `	if( hasNsSep ){` |
|       28 | 2034 | `		return nOrigIdx;` |
|        - | 2035 | `	}` |
|        - | 2036 | `	/* Check use imports first (works even outside namespaces) */` |
|  6458515 | 2037 | `	SyBlobReset(&pGen->sWorker);` |
|  6458515 | 2038 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  6458515 | 2039 | `	if( pImport ){` |
|       41 | 2040 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       41 | 2041 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|       41 | 2042 | `		if( pFromImport ){` |
|       18 | 2043 | `			*pFromImport = 1;` |
|        8 | 2044 | `		}` |
|       23 | 2045 | `	}else{` |
|  6458479 | 2046 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  6458341 | 2047 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|        - | 2048 | `		}` |
|        - | 2049 | `		/* Prepend current namespace */` |
|      143 | 2050 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      143 | 2051 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      143 | 2052 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|        - | 2053 | `	}` |
|        - | 2054 | `	/* Look up or create a new literal for the qualified name */` |
|      179 | 2055 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|      179 | 2056 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|       79 | 2057 | `		return nNewIdx; /* Already interned */` |
|        - | 2058 | `	}` |
|      105 | 2059 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      105 | 2060 | `	if( pNew == 0 ){` |
|      ! 0 | 2061 | `		return nOrigIdx; /* OOM, fall back to original */` |
|        - | 2062 | `	}` |
|      105 | 2063 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      105 | 2064 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      105 | 2065 | `	return nNewIdx;` |
|  3229273 | 2066 | `}` |
|        - | 2067 | `/*` |
|        - | 2068 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|        - | 2069 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|        - | 2070 | ` */` |
|   555168 | 2071 | `PH7_PRIVATE void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|        5 | 2072 | `{` |
|        - | 2073 | `	SyHashEntry *pImport;` |
|   555173 | 2074 | `	const char *zName = pName->zString;` |
|   555173 | 2075 | `	sxu32 nName = pName->nByte;` |
|   555173 | 2076 | `	sxu32 nFirst = 0;` |
|        - | 2077 | `	/* php resolves a name through use-imports on its LEADING segment: an` |
|        - | 2078 | ``	 * unqualified `C` maps the whole name (`use X\C;` -> X\C), while a QUALIFIED`` |
|        - | 2079 | ``	 * `A\B\C` maps just `A` (`use X\A;` -> X\A\B\C) and keeps the `\B\C` tail.`` |
|        - | 2080 | `	 * The old code looked up the whole qualified string (which never matches a` |
|        - | 2081 | `	 * single-segment import alias) and then blindly prefixed the current` |
|        - | 2082 | ``	 * namespace, so `use PHPUnit\Framework; extends Framework\TestCase` resolved`` |
|        - | 2083 | `	 * to Ns\Framework\TestCase. Mirror GenStateResolveNamespaceLiteral here. */` |
|  7059771 | 2084 | `	while( nFirst < nName && zName[nFirst] != '\\' ){ nFirst++; }` |
|   555173 | 2085 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)zName,nFirst);` |
|   555173 | 2086 | `	if( pImport ){` |
|       26 | 2087 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       26 | 2088 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|       26 | 2089 | `		SyBlobAppend(pOut,zName + nFirst,nName - nFirst); /* the \B\C tail, if any */` |
|       26 | 2090 | `		return;` |
|        - | 2091 | `	}` |
|        - | 2092 | `	/* Prepend current namespace if active */` |
|   555151 | 2093 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       14 | 2094 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       14 | 2095 | `		SyBlobAppend(pOut,"\\",1);` |
|        6 | 2096 | `	}` |
|   555151 | 2097 | `	SyBlobAppend(pOut,zName,nName);` |
|   277589 | 2098 | `}` |
|        - | 2099 | `/*` |
|        - | 2100 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|        - | 2101 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|        - | 2102 | ` * The caller must release pOut when done.` |
|        - | 2103 | ` */` |
|   532088 | 2104 | `PH7_PRIVATE void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|        5 | 2105 | `{` |
|   532093 | 2106 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     3989 | 2107 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     3989 | 2108 | `		SyBlobAppend(pOut,"\\",1);` |
|     1992 | 2109 | `	}` |
|   532093 | 2110 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   532093 | 2111 | `}` |
|        - | 2112 | `/*` |
|        - | 2113 | ` * Compile a namespace statement` |
|        - | 2114 | ` * According to the PHP language reference manual` |
|        - | 2115 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|        - | 2116 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|        - | 2117 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|        - | 2118 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|        - | 2119 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|        - | 2120 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|        - | 2121 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|        - | 2122 | ` *  programming world.` |
|        - | 2123 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|        - | 2124 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|        - | 2125 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|        - | 2126 | ` *  classes/functions/constants.` |
|        - | 2127 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|        - | 2128 | ` *  readability of source code.` |
|        - | 2129 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|        - | 2130 | ` *  Here is an example of namespace syntax in PHP:` |
|        - | 2131 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|        - | 2132 | ` *       class MyClass {}` |
|        - | 2133 | ` *       function myfunction() {}` |
|        - | 2134 | ` *       const MYCONST = 1;` |
|        - | 2135 | ` *       $a = new MyClass;` |
|        - | 2136 | ` *       $c = new \my\name\MyClass;` |
|        - | 2137 | ` *       $a = strlen('hi');` |
|        - | 2138 | ` *       $d = namespace\MYCONST;` |
|        - | 2139 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|        - | 2140 | ` *       echo constant($d);` |
|        - | 2141 | ` * NOTE` |
|        - | 2142 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|        - | 2143 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|        - | 2144 | ` */` |
|        - | 2145 | `/*` |
|        - | 2146 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|        - | 2147 | ` */` |
|       14 | 2148 | `PH7_PRIVATE const char * TokenTypeName(sxu32 nType)` |
|        4 | 2149 | `{` |
|       18 | 2150 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|       11 | 2151 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|       11 | 2152 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|       11 | 2153 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|       11 | 2154 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|       11 | 2155 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|        3 | 2156 | `	return "token";` |
|       11 | 2157 | `}` |
|     4028 | 2158 | `PH7_PRIVATE sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|        5 | 2159 | `{` |
|        - | 2160 | `	sxu32 nLine;` |
|        - | 2161 | `	sxi32 rc;` |
|     4033 | 2162 | `	nLine = pGen->pIn->nLine;` |
|     4033 | 2163 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|        - | 2164 | `	/* Reset namespace and clear previous use imports */` |
|     4033 | 2165 | `	SyBlobReset(&pGen->sNamespace);` |
|     4033 | 2166 | `	SyHashRelease(&pGen->hUseImports);` |
|     4033 | 2167 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|     4033 | 2168 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     4033 | 2169 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|     4033 | 2170 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     4033 | 2171 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|     4033 | 2172 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - | 2173 | `		/* Global namespace (bare "namespace;") */` |
|      ! 0 | 2174 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|      ! 0 | 2175 | `		return SXRET_OK;` |
|        - | 2176 | `	}` |
|     4033 | 2177 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|        - | 2178 | `		/* namespace; — switch to global namespace */` |
|      ! 0 | 2179 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|      ! 0 | 2180 | `		return SXRET_OK;` |
|        - | 2181 | `	}` |
|     4033 | 2182 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|        - | 2183 | `		/* namespace { } — global namespace block */` |
|        5 | 2184 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|        5 | 2185 | `		return SXRET_OK;` |
|        - | 2186 | `	}` |
|        - | 2187 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     8135 | 2188 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     4111 | 2189 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|        - | 2190 | `			/* Append backslash separator */` |
|       46 | 2191 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       46 | 2192 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|       21 | 2193 | `			}` |
|       25 | 2194 | `		}else{` |
|        - | 2195 | `			/* Append identifier */` |
|     4069 | 2196 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        - | 2197 | `		}` |
|     4111 | 2198 | `		pGen->pIn++;` |
|        5 | 2199 | `	}` |
|        - | 2200 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|        - | 2201 | `	 * at the correct program counter, not just the last one compiled. */` |
|        - | 2202 | `	{` |
|     4029 | 2203 | `		char *zNsDup = 0;` |
|     4029 | 2204 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     6038 | 2205 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     4022 | 2206 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     2011 | 2207 | `		}` |
|     4029 | 2208 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|        - | 2209 | `	}` |
|     4029 | 2210 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        8 | 2211 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - | 2212 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|        4 | 2213 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        6 | 2214 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2215 | `			return SXERR_ABORT;` |
|        - | 2216 | `		}` |
|        2 | 2217 | `	}` |
|     4029 | 2218 | `	return SXRET_OK;` |
|     2019 | 2219 | `}` |
|        - | 2220 | `/*` |
|        - | 2221 | ` * Compile the 'use' statement` |
|        - | 2222 | ` * According to the PHP language reference manual` |
|        - | 2223 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|        - | 2224 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|        - | 2225 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|        - | 2226 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|        - | 2227 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|        - | 2228 | ` *  a function or constant is not supported.` |
|        - | 2229 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|        - | 2230 | ` * NOTE` |
|        - | 2231 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|        - | 2232 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|        - | 2233 | ` */` |
|       80 | 2234 | `PH7_PRIVATE sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|        5 | 2235 | `{` |
|        - | 2236 | `	sxu32 nLine;` |
|        - | 2237 | `	sxi32 rc;` |
|        - | 2238 | `	SyBlob sPath;` |
|        - | 2239 | `	SyString sAlias;` |
|        - | 2240 | `	SyToken *pLast;` |
|        - | 2241 | `	char *zDup;` |
|        - | 2242 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|        - | 2243 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|        - | 2244 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|       85 | 2245 | `	nLine = pGen->pIn->nLine;` |
|       85 | 2246 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|        - | 2247 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|       85 | 2248 | `	iUseType = 0;` |
|       85 | 2249 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       30 | 2250 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|       30 | 2251 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|       16 | 2252 | `			iUseType = 1;` |
|       16 | 2253 | `			pGen->pIn++;` |
|       23 | 2254 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|       16 | 2255 | `			iUseType = 2;` |
|       16 | 2256 | `			pGen->pIn++;` |
|        7 | 2257 | `		}` |
|       14 | 2258 | `	}` |
|        - | 2259 | `	/* Select target hash tables based on import type */` |
|       85 | 2260 | `	switch( iUseType ){` |
|        7 | 2261 | `		case 1:` |
|       16 | 2262 | `			pGenHash = &pGen->hUseFuncImports;` |
|       16 | 2263 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|       16 | 2264 | `			break;` |
|        7 | 2265 | `		case 2:` |
|       16 | 2266 | `			pGenHash = &pGen->hUseConstImports;` |
|       16 | 2267 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|       16 | 2268 | `			break;` |
|       26 | 2269 | `		default:` |
|       57 | 2270 | `			pGenHash = &pGen->hUseImports;` |
|       57 | 2271 | `			pVmHash = &pGen->pVm->hUseImports;` |
|       52 | 2272 | `			break;` |
|        - | 2273 | `	}` |
|       85 | 2274 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|        - | 2275 | `	/* Process one or more use declarations separated by commas */` |
|       41 | 2276 | `	for(;;){` |
|       87 | 2277 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 2278 | `			break;` |
|        - | 2279 | `		}` |
|       87 | 2280 | `		SyBlobReset(&sPath);` |
|       87 | 2281 | `		pLast = 0;` |
|        - | 2282 | `		/* Collect the full namespace path */` |
|      301 | 2283 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|      219 | 2284 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|      151 | 2285 | `				pLast = pGen->pIn;` |
|      151 | 2286 | `				if( SyBlobLength(&sPath) > 0 ){` |
|       73 | 2287 | `					SyBlobAppend(&sPath,"\\",1);` |
|       34 | 2288 | `				}` |
|      151 | 2289 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       73 | 2290 | `			}` |
|      219 | 2291 | `			pGen->pIn++;` |
|        5 | 2292 | `		}` |
|       87 | 2293 | `		if( pLast == 0 ){` |
|        - | 2294 | `			/* Empty path */` |
|        6 | 2295 | `			break;` |
|        - | 2296 | `		}` |
|        - | 2297 | `		/* Default alias is the last component of the path */` |
|       83 | 2298 | `		sAlias = pLast->sData;` |
|        - | 2299 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|       78 | 2300 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       56 | 2301 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|       27 | 2302 | `			pGen->pIn++; /* Jump 'as' */` |
|       27 | 2303 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|       27 | 2304 | `				sAlias = pGen->pIn->sData;` |
|       27 | 2305 | `				pGen->pIn++;` |
|       12 | 2306 | `			}` |
|       12 | 2307 | `		}` |
|        - | 2308 | `		/* Check for duplicate import alias (per-type) */` |
|       83 | 2309 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|        8 | 2310 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 2311 | `				"Cannot use %.*s as %z because the name is already in use",` |
|        4 | 2312 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|        6 | 2313 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2314 | `				SyBlobRelease(&sPath);` |
|      ! 0 | 2315 | `				return SXERR_ABORT;` |
|        - | 2316 | `			}` |
|        2 | 2317 | `		}` |
|        - | 2318 | `		/* Register the import: alias -> FQN.` |
|        - | 2319 | `		 * Strings are allocated from the VM pool allocator and freed` |
|        - | 2320 | `		 * when the entire VM is released. SyHashRelease does not free` |
|        - | 2321 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|      122 | 2322 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       78 | 2323 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|       83 | 2324 | `		if( zDup ){` |
|       83 | 2325 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|       83 | 2326 | `			if( pVmHash ){` |
|        - | 2327 | `				/* Class imports: populate VM table directly (class resolution` |
|        - | 2328 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|       55 | 2329 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       55 | 2330 | `				if( zAliasDup ){` |
|       55 | 2331 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|       25 | 2332 | `				}` |
|       25 | 2333 | `			}` |
|       83 | 2334 | `			if( iUseType == 2 ){` |
|        - | 2335 | `				/* Const imports: emit a runtime instruction so imports are` |
|        - | 2336 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|       16 | 2337 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       16 | 2338 | `				if( zAliasDup ){` |
|        - | 2339 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|        - | 2340 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|        - | 2341 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|       16 | 2342 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|       16 | 2343 | `					if( azPair ){` |
|       16 | 2344 | `						azPair[0] = zAliasDup;` |
|       16 | 2345 | `						azPair[1] = zDup;` |
|       16 | 2346 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|        7 | 2347 | `					}` |
|        7 | 2348 | `				}` |
|        7 | 2349 | `			}` |
|       39 | 2350 | `		}` |
|        - | 2351 | `		/* Check for comma (multiple use declarations) */` |
|       83 | 2352 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|        3 | 2353 | `			pGen->pIn++;` |
|        2 | 2354 | `		}else{` |
|       43 | 2355 | `			break;` |
|        - | 2356 | `		}` |
|        1 | 2357 | `	}` |
|       85 | 2358 | `	SyBlobRelease(&sPath);` |
|       85 | 2359 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        4 | 2360 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|        2 | 2361 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        3 | 2362 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2363 | `			return SXERR_ABORT;` |
|        - | 2364 | `		}` |
|        1 | 2365 | `	}` |
|       85 | 2366 | `	return SXRET_OK;` |
|       45 | 2367 | `}` |
|        - | 2368 | `/*` |
|        - | 2369 | ` * Compile the stupid 'declare' language construct.` |
|        - | 2370 | ` *` |
|        - | 2371 | ` * According to the PHP language reference manual.` |
|        - | 2372 | ` *  The declare construct is used to set execution directives for a block of code.` |
|        - | 2373 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|        - | 2374 | ` *  declare (directive)` |
|        - | 2375 | ` *   statement` |
|        - | 2376 | ` * The directive section allows the behavior of the declare block to be set.` |
|        - | 2377 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|        - | 2378 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|        - | 2379 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|        - | 2380 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|        - | 2381 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|        - | 2382 | ` * <?php` |
|        - | 2383 | ` * // these are the same:` |
|        - | 2384 | ` * // you can use this:` |
|        - | 2385 | ` * declare(ticks=1) {` |
|        - | 2386 | ` *   // entire script here` |
|        - | 2387 | ` * }` |
|        - | 2388 | ` * // or you can use this:` |
|        - | 2389 | ` * declare(ticks=1);` |
|        - | 2390 | ` * // entire script here` |
|        - | 2391 | ` * ?>` |
|        - | 2392 | ` *` |
|        - | 2393 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|        - | 2394 | ` */` |
|        - | 2395 | `/*` |
|        - | 2396 | ` * Match a directive name against a known literal (case-insensitive).` |
|        - | 2397 | ` */` |
|       80 | 2398 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|        5 | 2399 | `{` |
|      120 | 2400 | `	return SyStringLength(pName) == nWant` |
|       80 | 2401 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|        5 | 2402 | `}` |
|        - | 2403 |  |
|       44 | 2404 | `PH7_PRIVATE sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|        5 | 2405 | `{` |
|       49 | 2406 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       49 | 2407 | `	SyToken *pBodyEnd = 0;` |
|        - | 2408 | `	SyToken *pBodyStart;` |
|        - | 2409 | `	SyToken *pCursor;` |
|        - | 2410 | `	int bHasStrictTypes;` |
|        - | 2411 | `	int bBlockForm;` |
|        - | 2412 | `	int bPlacementOk;` |
|        - | 2413 | `	sxi32 rc;` |
|       49 | 2414 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|       49 | 2415 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|        5 | 2416 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|        5 | 2417 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2418 | `			return SXERR_ABORT;` |
|        - | 2419 | `		}` |
|        5 | 2420 | `		goto Synchro;` |
|        - | 2421 | `	}` |
|       45 | 2422 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|       45 | 2423 | `	pBodyStart = pGen->pIn;` |
|        - | 2424 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|       45 | 2425 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|       45 | 2426 | `	if( pBodyEnd >= pGen->pEnd ){` |
|      ! 0 | 2427 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|      ! 0 | 2428 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2429 | `			return SXERR_ABORT;` |
|        - | 2430 | `		}` |
|      ! 0 | 2431 | `		return SXRET_OK;` |
|        - | 2432 | `	}` |
|        - | 2433 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|        - | 2434 | `	 * now delimits the comma-separated directive list. */` |
|       45 | 2435 | `	pGen->pIn = &pBodyEnd[1];` |
|       45 | 2436 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      ! 0 | 2437 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|      ! 0 | 2438 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2439 | `			return SXERR_ABORT;` |
|        - | 2440 | `		}` |
|      ! 0 | 2441 | `	}` |
|       45 | 2442 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|       45 | 2443 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|       45 | 2444 | `	bHasStrictTypes = 0;` |
|        - | 2445 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|        - | 2446 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|        - | 2447 | `	 * directive appears anywhere in the list, before validating values. */` |
|       45 | 2448 | `	pCursor = pBodyStart;` |
|       57 | 2449 | `	while( pCursor < pBodyEnd ){` |
|       53 | 2450 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|       45 | 2451 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|       41 | 2452 | `				bHasStrictTypes = 1;` |
|       41 | 2453 | `				break;` |
|        - | 2454 | `			}` |
|        2 | 2455 | `		}` |
|       14 | 2456 | `		pCursor++;` |
|        2 | 2457 | `	}` |
|       45 | 2458 | `	if( bHasStrictTypes && bBlockForm ){` |
|        3 | 2459 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2460 | `			"strict_types declaration must not use block mode");` |
|        3 | 2461 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        3 | 2462 | `		return SXRET_OK;` |
|        - | 2463 | `	}` |
|       43 | 2464 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|        6 | 2465 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2466 | `			"strict_types declaration must be the very first statement in the script");` |
|        6 | 2467 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        6 | 2468 | `		return SXRET_OK;` |
|        - | 2469 | `	}` |
|        - | 2470 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|       39 | 2471 | `	pCursor = pBodyStart;` |
|       73 | 2472 | `	while( pCursor < pBodyEnd ){` |
|        - | 2473 | `		SyToken *pNameTok;` |
|        - | 2474 | `		SyToken *pEqTok;` |
|        - | 2475 | `		SyToken *pValTok;` |
|        - | 2476 | `		SyString *pDirName;` |
|        - | 2477 | `		int bIsStrict;` |
|        - | 2478 | `		int iStrictValue;` |
|       41 | 2479 | `		pNameTok = pCursor;` |
|       41 | 2480 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 2481 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2482 | `				"declare: Expecting a directive name");` |
|      ! 0 | 2483 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2484 | `			return SXRET_OK;` |
|        - | 2485 | `		}` |
|       41 | 2486 | `		pEqTok = pNameTok + 1;` |
|       41 | 2487 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|      ! 0 | 2488 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2489 | `				"declare: Expecting '=' after directive name");` |
|      ! 0 | 2490 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2491 | `			return SXRET_OK;` |
|        - | 2492 | `		}` |
|       41 | 2493 | `		pValTok = pEqTok + 1;` |
|       41 | 2494 | `		if( pValTok >= pBodyEnd ){` |
|      ! 0 | 2495 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2496 | `				"declare: Expecting value after '='");` |
|      ! 0 | 2497 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2498 | `			return SXRET_OK;` |
|        - | 2499 | `		}` |
|       41 | 2500 | `		pDirName = &pNameTok->sData;` |
|       41 | 2501 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|       41 | 2502 | `		if( bIsStrict ){` |
|        - | 2503 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|        - | 2504 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|       37 | 2505 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      ! 0 | 2506 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2507 | `					"declare(strict_types) value must be a literal");` |
|      ! 0 | 2508 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2509 | `				return SXRET_OK;` |
|        - | 2510 | `			}` |
|       37 | 2511 | `			iStrictValue = -1;` |
|       37 | 2512 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|       37 | 2513 | `				const char *zv = SyStringData(&pValTok->sData);` |
|       37 | 2514 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|       37 | 2515 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|       35 | 2516 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|       16 | 2517 | `			}` |
|       37 | 2518 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|        3 | 2519 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2520 | `					"strict_types declaration must have 0 or 1 as its value");` |
|        3 | 2521 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        3 | 2522 | `				return SXRET_OK;` |
|        - | 2523 | `			}` |
|       35 | 2524 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|       21 | 2525 | `		}else if( DeclareNameIs(pDirName, "encoding", sizeof("encoding")-1) ){` |
|        - | 2526 | `			/* php always ignores declare(encoding=...) unless it was built with` |
|        - | 2527 | `			 * Zend multibyte, and says so in these exact words. */` |
|        3 | 2528 | `			PH7_GenCompileError(&(*pGen),E_WARNING,nLine,` |
|        - | 2529 | `				"declare(encoding=...) ignored because Zend multibyte feature is turned off by settings");` |
|        1 | 2530 | `		}else{` |
|        - | 2531 | `			/* Other directives (ticks and friends) are accepted as no-ops.` |
|        - | 2532 | `			 * This used to emit a NOTICE naming the upstream engine and its` |
|        - | 2533 | `			 * version ("the declare construct is a no-op in the current release` |
|        - | 2534 | `			 * of the PH7(2.1.4) engine") — php prints nothing at all for` |
|        - | 2535 | ``			 * `declare(ticks=1)`, and leaking the old engine's branding into`` |
|        - | 2536 | `			 * user-visible diagnostics was wrong on its own. */` |
|        - | 2537 | `		}` |
|       39 | 2538 | `		pCursor = pValTok + 1;` |
|        - | 2539 | `		/* Consume separating comma (or end). */` |
|       39 | 2540 | `		if( pCursor < pBodyEnd ){` |
|        3 | 2541 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|      ! 0 | 2542 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2543 | `					"declare: Expecting ',' or ')' after directive value");` |
|      ! 0 | 2544 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2545 | `				return SXRET_OK;` |
|        - | 2546 | `			}` |
|        3 | 2547 | `			pCursor++;` |
|        1 | 2548 | `		}` |
|        5 | 2549 | `	}` |
|        - | 2550 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|        - | 2551 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|        - | 2552 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|       37 | 2553 | `	return SXRET_OK;` |
|        2 | 2554 | `Synchro:` |
|        - | 2555 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|       15 | 2556 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       11 | 2557 | `		pGen->pIn++;` |
|        1 | 2558 | `	}` |
|        5 | 2559 | `	return SXRET_OK;` |
|       27 | 2560 | `}` |
|        - | 2561 | `/*` |
|        - | 2562 | ` * Compile a class constant.` |
|        - | 2563 | ` * According to the PHP language reference manual` |
|        - | 2564 | ` *  Class Constants` |
|        - | 2565 | ` *   It is possible to define constant values on a per-class basis remaining` |
|        - | 2566 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|        - | 2567 | ` *   you don't use the $ symbol to declare or use them.` |
|        - | 2568 | ` *   The value must be a constant expression, not (for example) a variable,` |
|        - | 2569 | ` *   a property, a result of a mathematical operation, or a function call.` |
|        - | 2570 | ` *   It's also possible for interfaces to have constants.` |
|        - | 2571 | ` * Symisc eXtension.` |
|        - | 2572 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|        - | 2573 | ` *  the zend engine would allow only simple scalar value.` |
|        - | 2574 | ` *  Example:` |
|        - | 2575 | ` *   class Test{` |
|        - | 2576 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|        - | 2577 | ` *   };` |
|        - | 2578 | ` *   var_dump(TEST::MyConst);` |
|        - | 2579 | ` *   Refer to the official documentation for more information on the powerful extension` |
|        - | 2580 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|        - | 2581 | ` */` |
|        - | 2582 | `/*` |
|        - | 2583 | ` * Exception handling.` |
|        - | 2584 | ` *  According to the PHP language reference manual` |
|        - | 2585 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|        - | 2586 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|        - | 2587 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|        - | 2588 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|        - | 2589 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|        - | 2590 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|        - | 2591 | ` *    (or re-thrown) within a catch block.` |
|        - | 2592 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|        - | 2593 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|        - | 2594 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|        - | 2595 | ` *    been defined with set_exception_handler().` |
|        - | 2596 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|        - | 2597 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|        - | 2598 | ` */` |
|        - | 2599 | `/*` |
|        - | 2600 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|        - | 2601 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|        - | 2602 | ` * indicates failure.` |
|        - | 2603 | ` */` |
|   549772 | 2604 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 2605 | `{` |
|        - | 2606 | ``	/* php decides throwability at RUNTIME: `throw 5` compiles and raises the`` |
|        - | 2607 | `	 * catchable Error "Can only throw objects" when it executes, and a` |
|        - | 2608 | `	 * non-Throwable object raises "Cannot throw objects that do not implement` |
|        - | 2609 | `	 * Throwable". This used to whitelist a handful of expression shapes and` |
|        - | 2610 | `	 * reject the rest at compile time as a "friendlier" error, which meant the` |
|        - | 2611 | `	 * program never ran and no catch could ever see it — and it was inconsistent` |
|        - | 2612 | ``	 * anyway, since `throw $v` (a variable holding an int) always compiled and`` |
|        - | 2613 | `	 * fell through to the same runtime path. OP_THROW now owns the whole rule,` |
|        - | 2614 | ``	 * so accept any expression here; `throw;` with no operand is still rejected`` |
|        - | 2615 | `	 * by PH7_CompileThrow's SXERR_EMPTY branch. */` |
|   274886 | 2616 | `	SXUNUSED(pGen);` |
|   274886 | 2617 | `	SXUNUSED(pRoot);` |
|   549777 | 2618 | `	return SXRET_OK;` |
|        5 | 2619 | `}` |
|        - | 2620 | `/*` |
|        - | 2621 | ` * Compile a 'throw' statement.` |
|        - | 2622 | ` * throw: This is how you trigger an exception.` |
|        - | 2623 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|        - | 2624 | ` */` |
|   549736 | 2625 | `PH7_PRIVATE sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|        5 | 2626 | `{` |
|   549741 | 2627 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2628 | `	GenBlock *pBlock;` |
|        - | 2629 | `	sxu32 nIdx;` |
|        - | 2630 | `	sxi32 rc;` |
|   549741 | 2631 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|        - | 2632 | `	/* Compile the expression */` |
|   549741 | 2633 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|   549741 | 2634 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 | 2635 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|      ! 0 | 2636 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2637 | `			return SXERR_ABORT;` |
|        - | 2638 | `		}` |
|      ! 0 | 2639 | `		return SXRET_OK;` |
|        - | 2640 | `	}` |
|   549741 | 2641 | `	pBlock = pGen->pCurrent;` |
|        - | 2642 | `	/* Point to the top most function or try block and emit the forward jump */` |
|  2190375 | 2643 | `	while(pBlock->pParent){` |
|  2190369 | 2644 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|   549735 | 2645 | `			break;` |
|        - | 2646 | `		}` |
|        - | 2647 | `		/* Point to the parent block */` |
|  1640639 | 2648 | `		pBlock = pBlock->pParent;` |
|        5 | 2649 | `	}` |
|        - | 2650 | `	/* Emit the throw instruction */` |
|   549741 | 2651 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|        - | 2652 | `	/* Emit the jump */` |
|   549741 | 2653 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|   549741 | 2654 | `	return SXRET_OK;` |
|   274873 | 2655 | `}` |
|        - | 2656 | `/*` |
|        - | 2657 | ` * Compile a PHP 8.0 'throw' expression.` |
|        - | 2658 | ` * Called from the expression code generator when a 'throw' keyword is` |
|        - | 2659 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|        - | 2660 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|        - | 2661 | ` * the validator guarantees the operand is a valid exception target.` |
|        - | 2662 | ` */` |
|       36 | 2663 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|        2 | 2664 | `{` |
|       38 | 2665 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2666 | `	GenBlock *pBlock;` |
|        - | 2667 | `	sxu32 nIdx;` |
|        - | 2668 | `	sxi32 rc;` |
|       18 | 2669 | `	(void)iCompileFlag;` |
|       38 | 2670 | `	pGen->pIn++; /* Skip 'throw' */` |
|       38 | 2671 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 2672 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 2673 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 2674 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2675 | `			return SXERR_ABORT;` |
|        - | 2676 | `		}` |
|      ! 0 | 2677 | `		return SXRET_OK;` |
|        - | 2678 | `	}` |
|       38 | 2679 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|       38 | 2680 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 2681 | `		return SXERR_ABORT;` |
|        - | 2682 | `	}` |
|       38 | 2683 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 | 2684 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 2685 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 2686 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2687 | `			return SXERR_ABORT;` |
|        - | 2688 | `		}` |
|      ! 0 | 2689 | `		return SXRET_OK;` |
|        - | 2690 | `	}` |
|        - | 2691 | `	/* Walk up to nearest exception/function block for the jump target */` |
|       38 | 2692 | `	pBlock = pGen->pCurrent;` |
|       60 | 2693 | `	while( pBlock->pParent ){` |
|       49 | 2694 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|       27 | 2695 | `			break;` |
|        - | 2696 | `		}` |
|       23 | 2697 | `		pBlock = pBlock->pParent;` |
|        1 | 2698 | `	}` |
|       38 | 2699 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       38 | 2700 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|       38 | 2701 | `	return SXRET_OK;` |
|       20 | 2702 | `}` |
|        - | 2703 | `/*` |
|        - | 2704 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|        - | 2705 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|        - | 2706 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|        - | 2707 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|        - | 2708 | ` * compile error propagated from the parser.` |
|        - | 2709 | ` */` |
|       56 | 2710 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|        5 | 2711 | `{` |
|        - | 2712 | `	SyString sClassName;` |
|        - | 2713 | `	SyToken *pToken;` |
|        - | 2714 | `	SyString *pName;` |
|        - | 2715 | `	char *zDup;` |
|        - | 2716 | `	sxi32 rc;` |
|       61 | 2717 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       61 | 2718 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|       61 | 2719 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|       61 | 2720 | `	SySetInit(&pCatch->sByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       61 | 2721 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 | 2722 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 2723 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 2724 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 2725 | `		return SXERR_INVALID;` |
|        - | 2726 | `	}` |
|       61 | 2727 | `	pGen->pIn++; /* '(' */` |
|       28 | 2728 | `	for(;;){` |
|        - | 2729 | `		SyBlob sResolved;` |
|       61 | 2730 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       61 | 2731 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 2732 | `			SyBlobRelease(&sResolved);` |
|      ! 0 | 2733 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 2734 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 2735 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 2736 | `			return SXERR_INVALID;` |
|        - | 2737 | `		}` |
|       89 | 2738 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       56 | 2739 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       61 | 2740 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|       61 | 2741 | `		SyBlobRelease(&sResolved);` |
|       61 | 2742 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|       61 | 2743 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|       61 | 2744 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       56 | 2745 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|        5 | 2746 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|      ! 0 | 2747 | `			pGen->pIn++; continue;` |
|        - | 2748 | `		}` |
|       61 | 2749 | `		break;` |
|      ! 0 | 2750 | `	}` |
|        - | 2751 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|        - | 2752 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding. */` |
|       61 | 2753 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|        3 | 2754 | `		pGen->pIn++; /* ')' */` |
|        3 | 2755 | `		return SXRET_OK;` |
|        - | 2756 | `	}` |
|       54 | 2757 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|       59 | 2758 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 2759 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 2760 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 2761 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 2762 | `		return SXERR_INVALID;` |
|        - | 2763 | `	}` |
|       59 | 2764 | `	pGen->pIn++; /* '$' */` |
|       59 | 2765 | `	pName = &pGen->pIn->sData;` |
|       59 | 2766 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       59 | 2767 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|       59 | 2768 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|       59 | 2769 | `	pGen->pIn++;` |
|       59 | 2770 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 | 2771 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 2772 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 2773 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 2774 | `		return SXERR_INVALID;` |
|        - | 2775 | `	}` |
|       59 | 2776 | `	pGen->pIn++; /* ')' */` |
|       59 | 2777 | `	return SXRET_OK;` |
|       33 | 2778 | `}` |
|        - | 2779 | `/*` |
|        - | 2780 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|        - | 2781 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|        - | 2782 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|        - | 2783 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|        - | 2784 | ` * VmThrowException):` |
|        - | 2785 | ` *` |
|        - | 2786 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|        - | 2787 | ` *    <try body>` |
|        - | 2788 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|        - | 2789 | ` *    JMP  -> finally\|end` |
|        - | 2790 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|        - | 2791 | ` *    <catch body>` |
|        - | 2792 | ` *    JMP  -> finally\|end` |
|        - | 2793 | ` *    ... more catches ...` |
|        - | 2794 | ` *  Lfin: <finally body>` |
|        - | 2795 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|        - | 2796 | ` *  Lend:` |
|        - | 2797 | ` */` |
|      100 | 2798 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|        5 | 2799 | `{` |
|      105 | 2800 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2801 | `	GenBlock *pTry;` |
|        - | 2802 | `	VmInstr *pInstr;` |
|      105 | 2803 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|        - | 2804 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|        - | 2805 | `	sxi32 rc;` |
|      105 | 2806 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|        - | 2807 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION) */` |
|      105 | 2808 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|      105 | 2809 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      105 | 2810 | `	pTry->pUserData = pException;` |
|      105 | 2811 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|      105 | 2812 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|      105 | 2813 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      105 | 2814 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      105 | 2815 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|      105 | 2816 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|        - | 2817 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|      105 | 2818 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|      105 | 2819 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|      105 | 2820 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|      105 | 2821 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 2822 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|      105 | 2823 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|        - | 2824 | `	/* Catch clauses (inline) */` |
|      105 | 2825 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      100 | 2826 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       61 | 2827 | `		sxu32 k = 0;` |
|       84 | 2828 | `		for(;;){` |
|        - | 2829 | `			ph7_exception_block sCatch;` |
|        - | 2830 | `			GenBlock *pCatchBlk;` |
|      117 | 2831 | `			sxu32 idxJmp = 0;` |
|      112 | 2832 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      107 | 2833 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|       33 | 2834 | `				break;` |
|        - | 2835 | `			}` |
|       61 | 2836 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|       61 | 2837 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       61 | 2838 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|       61 | 2839 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|       61 | 2840 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|       61 | 2841 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatchBlk);` |
|       61 | 2842 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        - | 2843 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|        - | 2844 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|        - | 2845 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump). */` |
|       61 | 2846 | `			pCatchBlk->pUserData = pException;` |
|       61 | 2847 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|       61 | 2848 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       61 | 2849 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|       61 | 2850 | `			GenStateLeaveBlock(&(*pGen),0);` |
|        - | 2851 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|        - | 2852 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|       61 | 2853 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       61 | 2854 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|       61 | 2855 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|       61 | 2856 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|       61 | 2857 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       61 | 2858 | `			k++;` |
|        5 | 2859 | `		}` |
|       28 | 2860 | `	}` |
|        - | 2861 | `	/* Finally (inline) */` |
|      105 | 2862 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       80 | 2863 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|        - | 2864 | `		GenBlock *pFinBlk;` |
|       52 | 2865 | `		pGen->pIn++; /* Jump 'finally' */` |
|       52 | 2866 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|       52 | 2867 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|       52 | 2868 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       52 | 2869 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       52 | 2870 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       52 | 2871 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|       52 | 2872 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       52 | 2873 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|       52 | 2874 | `		pException->iHasFinally = 1;` |
|       24 | 2875 | `	}` |
|      105 | 2876 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|      105 | 2877 | `	pException->iInlined = 1;` |
|        - | 2878 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|        - | 2879 | `	{` |
|      105 | 2880 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|        - | 2881 | `		sxu32 *aJ; sxu32 n;` |
|      105 | 2882 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|      105 | 2883 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|      105 | 2884 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|      161 | 2885 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|       61 | 2886 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|       61 | 2887 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|       33 | 2888 | `		}` |
|        - | 2889 | `	}` |
|      105 | 2890 | `	SySetRelease(&aCatchJmp);` |
|      105 | 2891 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|      ! 0 | 2892 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|      ! 0 | 2893 | `	}` |
|      105 | 2894 | `	return SXRET_OK;` |
|       55 | 2895 | `}` |
|        - | 2896 | `/*` |
|        - | 2897 | ` * Compile a 'catch' block.` |
|        - | 2898 | ` * Catch: A "catch" block retrieves an exception and creates` |
|        - | 2899 | ` * an object containing the exception information.` |
|        - | 2900 | ` */` |
|    25014 | 2901 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|        5 | 2902 | `{` |
|    25019 | 2903 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2904 | `	ph7_exception_block sCatch;` |
|        - | 2905 | `	SySet *pInstrContainer;` |
|        - | 2906 | `	SyString sClassName;` |
|        - | 2907 | `	GenBlock *pCatch;` |
|        - | 2908 | `	SyToken *pToken;` |
|        - | 2909 | `	SyString *pName;` |
|        - | 2910 | `	char *zDup;` |
|        - | 2911 | `	sxi32 rc;` |
|    25019 | 2912 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|        - | 2913 | `	/* Zero the structure */` |
|    25019 | 2914 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|        - | 2915 | `	/* Initialize fields */` |
|    25019 | 2916 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|    25019 | 2917 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|    25019 | 2918 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|        - | 2919 | `			/* Unexpected token,break immediately */` |
|      ! 0 | 2920 | `			pToken = pGen->pIn;` |
|      ! 0 | 2921 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 2922 | `				pToken--;` |
|      ! 0 | 2923 | `			}` |
|      ! 0 | 2924 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 2925 | `				"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 2926 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 2927 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2928 | `				return SXERR_ABORT;` |
|        - | 2929 | `			}` |
|      ! 0 | 2930 | `			return SXERR_INVALID;` |
|        - | 2931 | `	}` |
|        - | 2932 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|    25019 | 2933 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|    12522 | 2934 | `	for(;;){` |
|        - | 2935 | `		SyBlob sResolved;` |
|    25049 | 2936 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    25049 | 2937 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|        6 | 2938 | `			SyBlobRelease(&sResolved);` |
|        6 | 2939 | `			pToken = pGen->pIn;` |
|        6 | 2940 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 2941 | `				pToken--;` |
|      ! 0 | 2942 | `			}` |
|        8 | 2943 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 2944 | `				"syntax error, unexpected %s \"%z\"",` |
|        2 | 2945 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|        6 | 2946 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2947 | `				return SXERR_ABORT;` |
|        - | 2948 | `			}` |
|        6 | 2949 | `			return SXERR_INVALID;` |
|        - | 2950 | `		}` |
|        - | 2951 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|        - | 2952 | `		 * transient SyBlob allocation. */` |
|    37565 | 2953 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    25040 | 2954 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|    25045 | 2955 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|    25045 | 2956 | `		SyBlobRelease(&sResolved);` |
|    25045 | 2957 | `		if( zDup == 0 ){` |
|      ! 0 | 2958 | `			goto Mem;` |
|        - | 2959 | `		}` |
|    25045 | 2960 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|    25045 | 2961 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 2962 | `			goto Mem;` |
|        - | 2963 | `		}` |
|        - | 2964 | `		/* Check for '\|' (multi-catch separator) */` |
|    25040 | 2965 | `		if( pGen->pIn < pGen->pEnd &&` |
|    25040 | 2966 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|       35 | 2967 | `			pGen->pIn->sData.nByte == 1 &&` |
|       30 | 2968 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|       32 | 2969 | `			pGen->pIn++; /* Consume the '\|' */` |
|       32 | 2970 | `			continue;` |
|        - | 2971 | `		}` |
|    25015 | 2972 | `		break;` |
|      ! 0 | 2973 | `	}` |
|        - | 2974 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|        - | 2975 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding;` |
|        - | 2976 | `	 * jump straight to compiling the block below. */` |
|    25015 | 2977 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) /*)*/ ){` |
|        7 | 2978 | `		goto CatchBody;` |
|        - | 2979 | `	}` |
|    25004 | 2980 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|    25009 | 2981 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
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
|    25009 | 2995 | `	pGen->pIn++; /* Jump the dollar sign */` |
|        - | 2996 | `	/* Duplicate instance name */` |
|    25009 | 2997 | `	pName = &pGen->pIn->sData;` |
|    25009 | 2998 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    25009 | 2999 | `	if( zDup == 0 ){` |
|      ! 0 | 3000 | `		goto Mem;` |
|        - | 3001 | `	}` |
|    25009 | 3002 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|    25009 | 3003 | `	pGen->pIn++;` |
|    12505 | 3004 | `CatchBody:` |
|    25015 | 3005 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|        - | 3006 | `		/* Unexpected token,break immediately */` |
|      ! 0 | 3007 | `		pToken = pGen->pIn;` |
|      ! 0 | 3008 | `		if( pToken >= pGen->pEnd ){` |
|      ! 0 | 3009 | `			pToken--;` |
|      ! 0 | 3010 | `		}` |
|      ! 0 | 3011 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 3012 | `			"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 3013 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 3014 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3015 | `			return SXERR_ABORT;` |
|        - | 3016 | `		}` |
|      ! 0 | 3017 | `		return SXERR_INVALID;` |
|        - | 3018 | `	}` |
|        - | 3019 | `	/* Compile the block */` |
|    25015 | 3020 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|        - | 3021 | `	/* Create the catch block */` |
|    25015 | 3022 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|    25015 | 3023 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3024 | `		return SXERR_ABORT;` |
|        - | 3025 | `	}` |
|        - | 3026 | `	/* Swap bytecode container */` |
|    25015 | 3027 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    25015 | 3028 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|        - | 3029 | `	/* Compile the block */` |
|    25015 | 3030 | `	PH7_CompileBlock(&(*pGen),0);` |
|        - | 3031 | `	/* Fix forward jumps now the destination is resolved  */` |
|    25015 | 3032 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 3033 | `	/* Emit the DONE instruction */` |
|    25015 | 3034 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - | 3035 | `	/* Leave the block */` |
|    25015 | 3036 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 3037 | `	/* Restore the default container */` |
|    25015 | 3038 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - | 3039 | `	/* Install the catch block */` |
|    25015 | 3040 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|    25015 | 3041 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3042 | `		goto Mem;` |
|        - | 3043 | `	}` |
|    25015 | 3044 | `	return SXRET_OK;` |
|      ! 0 | 3045 | `Mem:` |
|      ! 0 | 3046 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 3047 | `	return SXERR_ABORT;` |
|    12512 | 3048 | `}` |
|        - | 3049 | `/*` |
|        - | 3050 | ` * Compile a 'try' block.` |
|        - | 3051 | ` * A function using an exception should be in a "try" block.` |
|        - | 3052 | ` * If the exception does not trigger, the code will continue` |
|        - | 3053 | ` * as normal. However if the exception triggers, an exception` |
|        - | 3054 | ` * is "thrown".` |
|        - | 3055 | ` */` |
|    25172 | 3056 | `PH7_PRIVATE sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|        5 | 3057 | `{` |
|        - | 3058 | `	ph7_exception *pException;` |
|    25177 | 3059 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 3060 | `	GenBlock *pTry;` |
|        - | 3061 | `	sxu32 nJmpIdx;` |
|        - | 3062 | `	sxi32 rc;` |
|        - | 3063 | `	/* Create the exception container */` |
|    25177 | 3064 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|    25177 | 3065 | `	if( pException == 0 ){` |
|      ! 0 | 3066 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|      ! 0 | 3067 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 3068 | `		return SXERR_ABORT;` |
|        - | 3069 | `	}` |
|        - | 3070 | `	/* Zero the structure */` |
|    25177 | 3071 | `	SyZero(pException,sizeof(ph7_exception));` |
|        - | 3072 | `	/* Initialize fields */` |
|    25177 | 3073 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|    25177 | 3074 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|    25177 | 3075 | `	pException->iHasFinally = 0;` |
|    25177 | 3076 | `	pException->iFinallyDone = 0;` |
|    25177 | 3077 | `	pException->pVm = pGen->pVm;` |
|        - | 3078 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|        - | 3079 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the legacy path.`` |
|        - | 3080 | `	 * DORMANT until the inline VM handlers (OP_CATCH / OP_END_FINALLY dispatch,` |
|        - | 3081 | `	 * VmThrowException pc-redirect, return/break-through-finally threading, generator` |
|        - | 3082 | `	 * park of aFinallyAction) land — the compiler emits the layout but the VM cannot yet` |
|        - | 3083 | `	 * execute it. Guarded by pVm->bInlineTryCatch (default 0) so the tree stays green. */` |
|    25177 | 3084 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|      105 | 3085 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|        - | 3086 | `	}` |
|        - | 3087 | `	/* Create the try block */` |
|    25077 | 3088 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|    25077 | 3089 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3090 | `		return SXERR_ABORT;` |
|        - | 3091 | `	}` |
|        - | 3092 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|    25077 | 3093 | `	pTry->pUserData = pException;` |
|        - | 3094 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|    25077 | 3095 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|        - | 3096 | `	/* Fix the jump later when the destination is resolved */` |
|    25077 | 3097 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|    25077 | 3098 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|        - | 3099 | `	/* Compile the block */` |
|    25077 | 3100 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|    25077 | 3101 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 3102 | `		return SXERR_ABORT;` |
|        - | 3103 | `	}` |
|        - | 3104 | `	/* Fix forward jumps now the destination is resolved */` |
|    25077 | 3105 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 3106 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|    25077 | 3107 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|        - | 3108 | `	/* Leave the block */` |
|    25077 | 3109 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 3110 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|    25077 | 3111 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|    25070 | 3112 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|        - | 3113 | `		/* Compile one or more catch blocks */` |
|    25010 | 3114 | `		for(;;){` |
|    50020 | 3115 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    37597 | 3116 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|    12508 | 3117 | `					break;` |
|        - | 3118 | `			}` |
|    25019 | 3119 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|    25019 | 3120 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3121 | `				return SXERR_ABORT;` |
|        - | 3122 | `			}` |
|        5 | 3123 | `		}` |
|    12503 | 3124 | `	}` |
|        - | 3125 | `	/* Compile optional finally block */` |
|    25077 | 3126 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      804 | 3127 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|        - | 3128 | `		SySet *pInstrContainer;` |
|        - | 3129 | `		GenBlock *pFinBlock;` |
|      129 | 3130 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|        - | 3131 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      129 | 3132 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      129 | 3133 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 3134 | `			return SXERR_ABORT;` |
|        - | 3135 | `		}` |
|        - | 3136 | `		/* Swap bytecode container */` |
|      129 | 3137 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      129 | 3138 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|        - | 3139 | `		/* Compile the finally body */` |
|      129 | 3140 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      129 | 3141 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3142 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 | 3143 | `			return SXERR_ABORT;` |
|        - | 3144 | `		}` |
|        - | 3145 | `		/* Fix forward jumps now the destination is resolved */` |
|      129 | 3146 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 3147 | `		/* Emit DONE to terminate the finally block */` |
|      129 | 3148 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - | 3149 | `		/* Leave the block */` |
|      129 | 3150 | `		GenStateLeaveBlock(&(*pGen),0);` |
|        - | 3151 | `		/* Restore the default container */` |
|      129 | 3152 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      129 | 3153 | `		pException->iHasFinally = 1;` |
|       62 | 3154 | `	}` |
|        - | 3155 | `	/* Must have at least one catch or finally */` |
|    25077 | 3156 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|        9 | 3157 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 3158 | `			"Cannot use try without catch or finally");` |
|        9 | 3159 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3160 | `			return SXERR_ABORT;` |
|        - | 3161 | `		}` |
|        3 | 3162 | `	}` |
|    25077 | 3163 | `	return SXRET_OK;` |
|    12591 | 3164 | `}` |
|        - | 3165 | `/*` |
|        - | 3166 | ` * Compile a switch block.` |
|        - | 3167 | ` *  (See block-comment below for more information)` |
|        - | 3168 | ` */` |
|   136472 | 3169 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|        5 | 3170 | `{` |
|   136477 | 3171 | `	sxi32 rc = SXRET_OK;` |
|   136477 | 3172 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|        - | 3173 | `		/* Unexpected token */` |
|      ! 0 | 3174 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 | 3175 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3176 | `			return SXERR_ABORT;` |
|        - | 3177 | `		}` |
|      ! 0 | 3178 | `		pGen->pIn++;` |
|      ! 0 | 3179 | `	}` |
|   136477 | 3180 | `	pGen->pIn++;` |
|        - | 3181 | `	/* First instruction to execute in this block. */` |
|   136477 | 3182 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|        - | 3183 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|        - | 3184 | `	 * or the '}' token */` |
|   130722 | 3185 | `	for(;;){` |
|   261449 | 3186 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 3187 | `			/* No more input to process */` |
|      ! 0 | 3188 | `			break;` |
|        - | 3189 | `		}` |
|   261449 | 3190 | `		rc = SXRET_OK;` |
|   261449 | 3191 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|    35149 | 3192 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|    11719 | 3193 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|        - | 3194 | `					/* Unexpected token */` |
|      ! 0 | 3195 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|      ! 0 | 3196 | `						&pGen->pIn->sData);` |
|      ! 0 | 3197 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3198 | `						return SXERR_ABORT;` |
|        - | 3199 | `					}` |
|        - | 3200 | `					/* FALL THROUGH */` |
|      ! 0 | 3201 | `				}` |
|    11719 | 3202 | `				rc = SXERR_EOF;` |
|    11719 | 3203 | `				break;` |
|        - | 3204 | `			}` |
|    11720 | 3205 | `		}else{` |
|        - | 3206 | `			sxi32 nKwrd;` |
|        - | 3207 | `			/* Extract the keyword */` |
|   226305 | 3208 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   226305 | 3209 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|    62383 | 3210 | `				break;` |
|        - | 3211 | `			}` |
|   101549 | 3212 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|        3 | 3213 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|        - | 3214 | `					/* Unexpected token */` |
|      ! 0 | 3215 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|      ! 0 | 3216 | `						&pGen->pIn->sData);` |
|      ! 0 | 3217 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3218 | `						return SXERR_ABORT;` |
|        - | 3219 | `					}` |
|        - | 3220 | `					/* FALL THROUGH */` |
|      ! 0 | 3221 | `				}` |
|        - | 3222 | `				/* Block compiled */` |
|        3 | 3223 | `				break;` |
|        - | 3224 | `			}` |
|        - | 3225 | `		}` |
|        - | 3226 | `		/* Compile block */` |
|   124977 | 3227 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|   124977 | 3228 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3229 | `			return SXERR_ABORT;` |
|        - | 3230 | `		}` |
|        5 | 3231 | `	}` |
|   136477 | 3232 | `	return rc;` |
|    68241 | 3233 | `}` |
|        - | 3234 | `/*` |
|        - | 3235 | ` * Compile a case eXpression.` |
|        - | 3236 | ` *  (See block-comment below for more information)` |
|        - | 3237 | ` */` |
|   132556 | 3238 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|        5 | 3239 | `{` |
|        - | 3240 | `	SySet *pInstrContainer;` |
|        - | 3241 | `	SyToken *pEnd,*pTmp;` |
|   132561 | 3242 | `	sxi32 iNest = 0;` |
|        - | 3243 | `	sxi32 rc;` |
|        - | 3244 | `	/* Delimit the expression */` |
|   132561 | 3245 | `	pEnd = pGen->pIn;` |
|   265125 | 3246 | `	while( pEnd < pGen->pEnd ){` |
|   265125 | 3247 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|        - | 3248 | `			/* Increment nesting level */` |
|        3 | 3249 | `			iNest++;` |
|   265124 | 3250 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|        - | 3251 | `			/* Decrement nesting level */` |
|        3 | 3252 | `			iNest--;` |
|   265122 | 3253 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|   132561 | 3254 | `			break;` |
|        - | 3255 | `		}` |
|   132569 | 3256 | `		pEnd++;` |
|        5 | 3257 | `	}` |
|   132561 | 3258 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 3259 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|      ! 0 | 3260 | `		if( rc == SXERR_ABORT ){` |
|        - | 3261 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 3262 | `			return SXERR_ABORT;` |
|        - | 3263 | `		}` |
|      ! 0 | 3264 | `	}` |
|        - | 3265 | `	/* Swap token stream */` |
|   132561 | 3266 | `	pTmp = pGen->pEnd;` |
|   132561 | 3267 | `	pGen->pEnd = pEnd;` |
|   132561 | 3268 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   132561 | 3269 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|   132561 | 3270 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 3271 | `	/* Emit the done instruction */` |
|   132561 | 3272 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   132561 | 3273 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - | 3274 | `	/* Update token stream */` |
|   132561 | 3275 | `	pGen->pIn  = pEnd;` |
|   132561 | 3276 | `	pGen->pEnd = pTmp;` |
|   132561 | 3277 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 3278 | `		return SXERR_ABORT;` |
|        - | 3279 | `	}` |
|   132561 | 3280 | `	return SXRET_OK;` |
|    66283 | 3281 | `}` |
|        - | 3282 | `/*` |
|        - | 3283 | ` * Compile the smart switch statement.` |
|        - | 3284 | ` * According to the PHP language reference manual` |
|        - | 3285 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|        - | 3286 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|        - | 3287 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|        - | 3288 | ` *  This is exactly what the switch statement is for.` |
|        - | 3289 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|        - | 3290 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|        - | 3291 | ` *  of the outer loop, use continue 2.` |
|        - | 3292 | ` *  Note that switch/case does loose comparision.` |
|        - | 3293 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|        - | 3294 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|        - | 3295 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|        - | 3296 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|        - | 3297 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|        - | 3298 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|        - | 3299 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|        - | 3300 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|        - | 3301 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|        - | 3302 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|        - | 3303 | ` *  list for the next case.` |
|        - | 3304 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|        - | 3305 | ` *  or floating-point numbers and strings.` |
|        - | 3306 | ` */` |
|    11716 | 3307 | `PH7_PRIVATE sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|        5 | 3308 | `{` |
|        - | 3309 | `	GenBlock *pSwitchBlock;` |
|        - | 3310 | `	SyToken *pTmp,*pEnd;` |
|        - | 3311 | `	ph7_switch *pSwitch;` |
|        - | 3312 | `	sxu32 nToken;` |
|        - | 3313 | `	sxu32 nLine;` |
|        - | 3314 | `	sxi32 rc;` |
|    11721 | 3315 | `	nLine = pGen->pIn->nLine;` |
|        - | 3316 | `	/* Jump the 'switch' keyword */` |
|    11721 | 3317 | `	pGen->pIn++;` |
|    11721 | 3318 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 3319 | `		/* Syntax error */` |
|      ! 0 | 3320 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|      ! 0 | 3321 | `		if( rc == SXERR_ABORT ){` |
|        - | 3322 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 3323 | `			return SXERR_ABORT;` |
|        - | 3324 | `		}` |
|      ! 0 | 3325 | `		goto Synchronize;` |
|        - | 3326 | `	}` |
|        - | 3327 | `	/* Jump the left parenthesis '(' */` |
|    11721 | 3328 | `	pGen->pIn++;` |
|    11721 | 3329 | `	pEnd = 0; /* cc warning */` |
|        - | 3330 | `	/* Create the loop block */` |
|    17579 | 3331 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|     5858 | 3332 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|    11721 | 3333 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3334 | `		return SXERR_ABORT;` |
|        - | 3335 | `	}` |
|        - | 3336 | `	/* Delimit the condition */` |
|    11721 | 3337 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    11721 | 3338 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - | 3339 | `		/* Empty expression */` |
|      ! 0 | 3340 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|      ! 0 | 3341 | `		if( rc == SXERR_ABORT ){` |
|        - | 3342 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 3343 | `			return SXERR_ABORT;` |
|        - | 3344 | `		}` |
|      ! 0 | 3345 | `	}` |
|        - | 3346 | `	/* Swap token streams */` |
|    11721 | 3347 | `	pTmp = pGen->pEnd;` |
|    11721 | 3348 | `	pGen->pEnd = pEnd;` |
|        - | 3349 | `	/* Compile the expression */` |
|    11721 | 3350 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    11721 | 3351 | `	if( rc == SXERR_ABORT ){` |
|        - | 3352 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 3353 | `		return SXERR_ABORT;` |
|        - | 3354 | `	}` |
|        - | 3355 | `	/* Update token stream */` |
|    11721 | 3356 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 | 3357 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 3358 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 3359 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3360 | `			return SXERR_ABORT;` |
|        - | 3361 | `		}` |
|      ! 0 | 3362 | `		pGen->pIn++;` |
|      ! 0 | 3363 | `	}` |
|    11721 | 3364 | `	pGen->pIn  = &pEnd[1];` |
|    11721 | 3365 | `	pGen->pEnd = pTmp;` |
|    11721 | 3366 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|    11716 | 3367 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|      ! 0 | 3368 | `			pTmp = pGen->pIn;` |
|      ! 0 | 3369 | `			if( pTmp >= pGen->pEnd ){` |
|      ! 0 | 3370 | `				pTmp--;` |
|      ! 0 | 3371 | `			}` |
|        - | 3372 | `			/* Unexpected token */` |
|      ! 0 | 3373 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|      ! 0 | 3374 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3375 | `				return SXERR_ABORT;` |
|        - | 3376 | `			}` |
|      ! 0 | 3377 | `			goto Synchronize;` |
|        - | 3378 | `	}` |
|        - | 3379 | `	/* Set the delimiter token */` |
|    11721 | 3380 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|        3 | 3381 | `		nToken = PH7_TK_KEYWORD;` |
|        - | 3382 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|        2 | 3383 | `	}else{` |
|    11719 | 3384 | `		nToken = PH7_TK_CCB; /* '}' */` |
|        - | 3385 | `	}` |
|    11721 | 3386 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|        - | 3387 | `	/* Create the switch blocks container */` |
|    11721 | 3388 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|    11721 | 3389 | `	if( pSwitch == 0 ){` |
|        - | 3390 | `		/* Abort compilation */` |
|      ! 0 | 3391 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 3392 | `		return SXERR_ABORT;` |
|        - | 3393 | `	}` |
|        - | 3394 | `	/* Zero the structure */` |
|    11721 | 3395 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|        - | 3396 | `	/* Initialize fields */` |
|    11721 | 3397 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|        - | 3398 | `	/* Emit the switch instruction */` |
|    11721 | 3399 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|        - | 3400 | `	/* Compile case blocks */` |
|   130616 | 3401 | `	for(;;){` |
|        - | 3402 | `		sxu32 nKwrd;` |
|   136479 | 3403 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 3404 | `			/* No more input to process */` |
|      ! 0 | 3405 | `			break;` |
|        - | 3406 | `		}` |
|   136479 | 3407 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 3408 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|        - | 3409 | `				/* Unexpected token */` |
|      ! 0 | 3410 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 3411 | `					&pGen->pIn->sData);` |
|      ! 0 | 3412 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3413 | `					return SXERR_ABORT;` |
|        - | 3414 | `				}` |
|        - | 3415 | `				/* FALL THROUGH */` |
|      ! 0 | 3416 | `			}` |
|        - | 3417 | `			/* Block compiled */` |
|      ! 0 | 3418 | `			break;` |
|        - | 3419 | `		}` |
|        - | 3420 | `		/* Extract the keyword */` |
|   136479 | 3421 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   136479 | 3422 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|        3 | 3423 | `			if( nToken != PH7_TK_KEYWORD ){` |
|        - | 3424 | `				/* Unexpected token */` |
|      ! 0 | 3425 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 3426 | `					&pGen->pIn->sData);` |
|      ! 0 | 3427 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3428 | `					return SXERR_ABORT;` |
|        - | 3429 | `				}` |
|        - | 3430 | `				/* FALL THROUGH */` |
|      ! 0 | 3431 | `			}` |
|        - | 3432 | `			/* Block compiled */` |
|        3 | 3433 | `			break;` |
|        - | 3434 | `		}` |
|   136477 | 3435 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|        - | 3436 | `			/*` |
|        - | 3437 | `			 * Accroding to the PHP language reference manual` |
|        - | 3438 | `			 *  A special case is the default case. This case matches anything` |
|        - | 3439 | `			 *  that wasn't matched by the other cases.` |
|        - | 3440 | `			 */` |
|     3921 | 3441 | `			if( pSwitch->nDefault > 0 ){` |
|        - | 3442 | `				/* Default case already compiled */` |
|      ! 0 | 3443 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|      ! 0 | 3444 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3445 | `					return SXERR_ABORT;` |
|        - | 3446 | `				}` |
|      ! 0 | 3447 | `			}` |
|     3921 | 3448 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|        - | 3449 | `			/* Compile the default block */` |
|     3921 | 3450 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|     3921 | 3451 | `			if( rc == SXERR_ABORT){` |
|      ! 0 | 3452 | `				return SXERR_ABORT;` |
|     3921 | 3453 | `			}else if( rc == SXERR_EOF ){` |
|     3919 | 3454 | `				break;` |
|        1 | 3455 | `			}` |
|   132562 | 3456 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|        - | 3457 | `			ph7_case_expr sCase;` |
|        - | 3458 | `			/* Standard case block */` |
|   132561 | 3459 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|        - | 3460 | `			/* initialize the structure */` |
|   132561 | 3461 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - | 3462 | `			/* Compile the case expression */` |
|   132561 | 3463 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|   132561 | 3464 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3465 | `				return SXERR_ABORT;` |
|        - | 3466 | `			}` |
|        - | 3467 | `			/* Compile the case block */` |
|   132561 | 3468 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|        - | 3469 | `			/* Insert in the switch container */` |
|   132561 | 3470 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|   132561 | 3471 | `			if( rc == SXERR_ABORT){` |
|      ! 0 | 3472 | `				return SXERR_ABORT;` |
|   132561 | 3473 | `			}else if( rc == SXERR_EOF ){` |
|     7805 | 3474 | `				break;` |
|        - | 3475 | `			}` |
|    62383 | 3476 | `		}else{` |
|        - | 3477 | `			/* Unexpected token */` |
|      ! 0 | 3478 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 3479 | `				&pGen->pIn->sData);` |
|      ! 0 | 3480 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3481 | `				return SXERR_ABORT;` |
|        - | 3482 | `			}` |
|      ! 0 | 3483 | `			break;` |
|        - | 3484 | `		}` |
|        5 | 3485 | `	}` |
|        - | 3486 | `	/* Fix all jumps now the destination is resolved */` |
|    11721 | 3487 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|    11721 | 3488 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 3489 | `	/* Release the loop block */` |
|    11721 | 3490 | `	GenStateLeaveBlock(pGen,0);` |
|    11721 | 3491 | `	if( pGen->pIn < pGen->pEnd ){` |
|        - | 3492 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|    11721 | 3493 | `		pGen->pIn++;` |
|     5858 | 3494 | `	}` |
|        - | 3495 | `	/* Statement successfully compiled */` |
|    11721 | 3496 | `	return SXRET_OK;` |
|      ! 0 | 3497 | `Synchronize:` |
|        - | 3498 | `	/* Synchronize with the first semi-colon */` |
|      ! 0 | 3499 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      ! 0 | 3500 | `		pGen->pIn++;` |
|      ! 0 | 3501 | `	}` |
|      ! 0 | 3502 | `	return SXRET_OK;` |
|     5863 | 3503 | `}` |
|        - | 3504 |  |
