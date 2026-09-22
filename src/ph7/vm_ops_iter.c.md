# src/ph7/vm_ops_iter.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 274/342 lines (80.12%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|      - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    5 | ` */` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `/*` |
|      - |    8 | ` * Section:` |
|      - |    9 | ` *    Iteration opcode handlers extracted from vm.c's dispatch loop. Each` |
|      - |   10 | ` *    handler runs one opcode arm against the caller's VmExecState: the loop` |
|      - |   11 | ` *    syncs pTos/pc in, calls the handler, reloads them and routes the` |
|      - |   12 | ` *    returned VmOpRc onto its labels (same idiom as VmCallFinish).` |
|      - |   13 | ` * Status:` |
|      - |   14 | ` *    Stable.` |
|      - |   15 | ` */` |
|      - |   16 | `/* Bind the dispatch-routing exits to handler semantics (see vm_dispatch.h),` |
|      - |   17 | ` * and map the macros' sState references onto our state parameter. */` |
|      - |   18 | `#define VM_EXIT_BREAK      { pState->pTos = pTos; pState->pc = pc; return VM_OP_NEXT; }` |
|      - |   19 | `#define VM_EXIT_ABORT      { pState->pTos = pTos; pState->pc = pc; return VM_OP_ABORT; }` |
|      - |   20 | `#define VM_EXIT_EXCEPTION  { pState->pTos = pTos; pState->pc = pc; return VM_OP_EXCEPTION; }` |
|      - |   21 | `#include "vm_dispatch.h"` |
|      - |   22 | `#define sState (*pState)` |
|      - |   23 |  |
|      - |   24 | `/*` |
|      - |   25 | ` * OP_FOREACH_STEP: advance one foreach iteration (hashmap cursor, Iterator` |
|      - |   26 | ` * protocol, or object-attribute walk). Body moved verbatim from the` |
|      - |   27 | ` * OP_FOREACH_STEP arm of VmByteCodeExecBody; arm-terminal breaks became` |
|      - |   28 | ` * VM_EXIT_BREAK.` |
|      - |   29 | ` */` |
| 298632 |   30 | `PH7_PRIVATE VmOpRc VmExecOpForeachStep(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |   31 | `{` |
| 298637 |   32 | `	ph7_value *pTos = pState->pTos;` |
| 298637 |   33 | `	ph7_value *pStack = pState->pStack;` |
| 298637 |   34 | `	VmInstr *aInstr = pState->aInstr;` |
| 298637 |   35 | `	sxi32 pc = pState->pc;` |
|      - |   36 | `	sxi32 rc;` |
| 298637 |   37 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|      - |   38 | `	ph7_foreach_step **apStep,*pStep;` |
|      - |   39 | `	ph7_value *pValue;` |
|      - |   40 | `	VmFrame *pFrameLocal;` |
|      - |   41 | `	sxu32 nStep;` |
| 298637 |   42 | `	pFrameLocal = pVm->pFrame;` |
| 298637 |   43 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      - |   44 | `	/* Select THIS activation's step. aStep is per-STATEMENT and shared by every` |
|      - |   45 | `	 * activation, so peeking the last entry resumes onto a sibling's cursor when` |
|      - |   46 | `	 * two instances of one generator/fiber are suspended in the same textual` |
|      - |   47 | `	 * foreach. Scan from the top (most-recent push) for the step whose owning` |
|      - |   48 | `	 * frame matches the running activation; top-down makes the current push win` |
|      - |   49 | `	 * over any leaked older step that happens to share a recycled frame address. */` |
| 298637 |   50 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
| 298637 |   51 | `	nStep = SySetUsed(&pInfo->aStep);` |
| 298637 |   52 | `	if( nStep < 1 ){` |
|      - |   53 | `		/* Defensive: OP_FOREACH_INIT always pushes this activation's step before` |
|      - |   54 | `		 * STEP runs (and jumps past the loop when the push fails), so an empty` |
|      - |   55 | `		 * set is unreachable — guard the apStep[-1] read anyway. Jump out. */` |
|    ! 0 |   56 | `		pc = pInstr->iP2 - 1;` |
|    ! 0 |   57 | `		VM_EXIT_BREAK;` |
|      - |   58 | `	}` |
| 298637 |   59 | `	pStep = apStep[nStep - 1];` |
| 298663 |   60 | `	while( nStep > 0 ){` |
| 298663 |   61 | `		if( apStep[nStep - 1]->pFrame == pFrameLocal ){` |
| 298637 |   62 | `			pStep = apStep[nStep - 1];` |
| 298637 |   63 | `			break;` |
|      - |   64 | `		}` |
|     27 |   65 | `		nStep--;` |
|      1 |   66 | `	}` |
| 298637 |   67 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|      - |   68 | `		ph7_hashmap_node *pNode;` |
|      - |   69 | `		/* Extract the current node via this loop's PRIVATE cursor (php:` |
|      - |   70 | `		 * nested foreach over the same array are independent iterations) */` |
| 297171 |   71 | `		pNode = pStep->pCursor;` |
| 297171 |   72 | `		if( pNode == 0 ){` |
|      - |   73 | `			/* No more entry to process */` |
|  26983 |   74 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|  26983 |   75 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|      - |   76 | `				/* Break the reference with the last element */` |
|     27 |   77 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|     12 |   78 | `			}` |
|      - |   79 | `			/* Cleanup the mess left behind */` |
|  26983 |   80 | `			VmForeachHashmapStepRelease(&(*pVm),pInfo,pStep,TRUE);` |
|  13494 |   81 | `		}else{` |
|      - |   82 | `			/* Advance the private cursor */` |
| 270193 |   83 | `			pStep->pCursor = pNode->pPrev; /* Reverse link */` |
|      - |   84 | `			/* Bind the VALUE before the KEY: on the first iteration this is where` |
|      - |   85 | `			 * both locals are created in the frame table, and php's symbol table` |
|      - |   86 | `			 * lists the value ahead of the key (get_defined_vars() order). Only the` |
|      - |   87 | `			 * creation ORDER matters here; the stored values are independent. */` |
| 270193 |   88 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|      - |   89 | `				SyHashEntry *pEntry;` |
|      - |   90 | `				/* Pass by reference */` |
|     73 |   91 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|     73 |   92 | `				if( pEntry ){` |
|     55 |   93 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|     29 |   94 | `				}else{` |
|     30 |   95 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|     18 |   96 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|      - |   97 | `				}` |
|     38 |   98 | `			}else{` |
|      - |   99 | `				/* Make a copy of the entry value */` |
| 270123 |  100 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
| 270123 |  101 | `				if( pValue ){` |
| 270123 |  102 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
| 135059 |  103 | `				}` |
|      - |  104 | `			}` |
| 270193 |  105 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|  16617 |  106 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|  16617 |  107 | `				if( pKey ){` |
|  16617 |  108 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|   8306 |  109 | `				}` |
|   8306 |  110 | `			}` |
|      5 |  111 | `		}` |
| 150054 |  112 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|      - |  113 | `		/* Iterator-based iteration.` |
|      - |  114 | `		 * Sequence: on first call just check valid/current/key.` |
|      - |  115 | `		 * On subsequent calls, advance with next() first, then check.` |
|      - |  116 | `		 */` |
|   1389 |  117 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|      - |  118 | `		ph7_class_method *pMethod;` |
|      - |  119 | `		ph7_value sResult;` |
|   1389 |  120 | `		int isValid = 0;` |
|      - |  121 | `		/* Call next() to advance — but skip on the first iteration */` |
|   1389 |  122 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|    263 |  123 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|    134 |  124 | `		}else{` |
|   1131 |  125 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|   1131 |  126 | `			if( pMethod ){` |
|   1131 |  127 | `				rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|   1131 |  128 | `				if( VmIterCallThrew(rc) ){` |
|      - |  129 | `					/* next() threw (generator body / userland Iterator): tear the` |
|      - |  130 | `					 * step down like exhaustion does, then route the exception —` |
|      - |  131 | `					 * the loop must not silently end with execution continuing. */` |
|     15 |  132 | `					VmForeachStepAbandon(pVm,pInfo,pStep,pThis);` |
|     15 |  133 | `					PH7_DISPATCH_ITER_RC(rc,0)` |
|    ! 0 |  134 | `				}` |
|    557 |  135 | `			}` |
|      - |  136 | `		}` |
|      - |  137 | `		/* Call valid() */` |
|   1377 |  138 | `		PH7_MemObjInit(pVm,&sResult);` |
|   1377 |  139 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|   1377 |  140 | `		if( pMethod ){` |
|   1377 |  141 | `			rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|   1377 |  142 | `			if( VmIterCallThrew(rc) ){` |
|      - |  143 | `				/* valid() threw: same teardown-and-route as next() above. */` |
|    ! 0 |  144 | `				PH7_MemObjRelease(&sResult);` |
|    ! 0 |  145 | `				VmForeachStepAbandon(pVm,pInfo,pStep,pThis);` |
|    ! 0 |  146 | `				PH7_DISPATCH_ITER_RC(rc,0)` |
|    ! 0 |  147 | `			}` |
|   1377 |  148 | `			PH7_MemObjToBool(&sResult);` |
|   1377 |  149 | `			isValid = (sResult.x.iVal != 0);` |
|    686 |  150 | `		}` |
|   1377 |  151 | `		PH7_MemObjRelease(&sResult);` |
|   1377 |  152 | `		if( !isValid ){` |
|      - |  153 | `			/* Iterator exhausted */` |
|    239 |  154 | `			pc = pInstr->iP2 - 1;` |
|      - |  155 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|    239 |  156 | `			VmForeachStepAbandon(pVm,pInfo,pStep,pThis);` |
|    122 |  157 | `		}else{` |
|      - |  158 | `			/* Call current() to get value */` |
|   1143 |  159 | `			PH7_MemObjInit(pVm,&sResult);` |
|   1143 |  160 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|   1143 |  161 | `			if( pMethod ){` |
|   1143 |  162 | `				rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|   1143 |  163 | `				if( VmIterCallThrew(rc) ){` |
|      - |  164 | `					/* current() threw: same teardown-and-route as next() above. */` |
|    ! 0 |  165 | `					PH7_MemObjRelease(&sResult);` |
|    ! 0 |  166 | `					VmForeachStepAbandon(pVm,pInfo,pStep,pThis);` |
|    ! 0 |  167 | `					PH7_DISPATCH_ITER_RC(rc,0)` |
|    ! 0 |  168 | `				}` |
|    569 |  169 | `			}` |
|   1143 |  170 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   1143 |  171 | `			if( pValue ){` |
|   1143 |  172 | `				PH7_MemObjStore(&sResult,pValue);` |
|    569 |  173 | `			}` |
|   1143 |  174 | `			PH7_MemObjRelease(&sResult);` |
|      - |  175 | `			/* Call key() if needed */` |
|   1143 |  176 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      - |  177 | `				ph7_value sKey;` |
|    216 |  178 | `				PH7_MemObjInit(pVm,&sKey);` |
|    216 |  179 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|    216 |  180 | `				if( pMethod ){` |
|    216 |  181 | `					rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|    216 |  182 | `					if( VmIterCallThrew(rc) ){` |
|      - |  183 | `						/* key() threw: same teardown-and-route as next() above. */` |
|    ! 0 |  184 | `						PH7_MemObjRelease(&sKey);` |
|    ! 0 |  185 | `						VmForeachStepAbandon(pVm,pInfo,pStep,pThis);` |
|    ! 0 |  186 | `						PH7_DISPATCH_ITER_RC(rc,0)` |
|    ! 0 |  187 | `					}` |
|    106 |  188 | `				}` |
|    216 |  189 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|    216 |  190 | `				if( pValue ){` |
|    216 |  191 | `					PH7_MemObjStore(&sKey,pValue);` |
|    106 |  192 | `				}` |
|    216 |  193 | `				PH7_MemObjRelease(&sKey);` |
|    106 |  194 | `			}` |
|      - |  195 | `		}` |
|    691 |  196 | `	}else{` |
|     86 |  197 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|     86 |  198 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|      - |  199 | `		SyHashEntry *pEntry;` |
|      - |  200 | `		/* Point to the next attribute */` |
|    102 |  201 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|     76 |  202 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     76 |  203 | `			if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|      - |  204 | `				/* A static property belongs to the CLASS, never to an object: php` |
|      - |  205 | `				 * iterates only the instance's own properties. PHL's instance` |
|      - |  206 | `				 * attribute table carries an entry for every declared member` |
|      - |  207 | `				 * (statics share the class slot), so it has to filter here — the` |
|      - |  208 | `				 * same test var_dump/get_object_vars/json/serialize already make. */` |
|      3 |  209 | `				continue;` |
|      - |  210 | `			}` |
|     70 |  211 | `			if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|     39 |  212 | `			 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|      3 |  213 | `				continue; /* virtual set-only property: iteration skips it (php) */` |
|      - |  214 | `			}` |
|      - |  215 | `			/* Check access permission */` |
|    106 |  216 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|     68 |  217 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|     60 |  218 | `					break; /* Access is granted */` |
|      - |  219 | `			}` |
|      3 |  220 | `		}` |
|     86 |  221 | `		if( pEntry == 0 ){` |
|      - |  222 | `			/* Clean up the mess left behind */` |
|     30 |  223 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|     30 |  224 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|      - |  225 | `				/* Break the reference with the last element */` |
|      3 |  226 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|      1 |  227 | `			}` |
|     30 |  228 | `			VmForeachStepUnlink(pInfo,pStep);` |
|     30 |  229 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     30 |  230 | `			PH7_ClassInstanceUnref(pThis);` |
|     17 |  231 | `		}else{` |
|     60 |  232 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|      - |  233 | `			ph7_value *pAttrValue;` |
|     60 |  234 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|      - |  235 | `				/* Fill with the current attribute name */` |
|     60 |  236 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|     60 |  237 | `				if( pKey ){` |
|     60 |  238 | `					SyBlobReset(&pKey->sBlob);` |
|     60 |  239 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|     60 |  240 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|     28 |  241 | `				}` |
|     28 |  242 | `			}` |
|     56 |  243 | `			if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET))` |
|     34 |  244 | `			 && (pStep->iFlags & PH7_4EACH_STEP_REF)` |
|     11 |  245 | `			 && !VmHookGuardHeld(pVm,(void *)pThis,&pVmAttr->pAttr->sName) ){` |
|      - |  246 | `				/* php: a hooked property (virtual or backed) cannot be iterated` |
|      - |  247 | `				 * by reference — catchable Error. Tear the step down like the` |
|      - |  248 | `				 * exhausted-iteration path (break the by-ref binding, unlink,` |
|      - |  249 | `				 * free, drop the instance retain) so nothing leaks and a` |
|      - |  250 | `				 * re-entered foreach starts fresh; the fetch-point router lands` |
|      - |  251 | `				 * the parked throw right after this op. Inside the property's` |
|      - |  252 | `				 * own hook body the guard keeps raw semantics (no Error). */` |
|      - |  253 | `				SyBlob sErrMsg;` |
|      3 |  254 | `				SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      3 |  255 | `				SyBlobFormat(&sErrMsg,"Cannot create reference to property %z::$%z",` |
|      2 |  256 | `					&pThis->pClass->sName,&pVmAttr->pAttr->sName);` |
|      3 |  257 | `				VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      3 |  258 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|      3 |  259 | `				VmForeachStepUnlink(pInfo,pStep);` |
|      3 |  260 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      3 |  261 | `				PH7_ClassInstanceUnref(pThis);` |
|      3 |  262 | `				VM_EXIT_BREAK;` |
|      - |  263 | `			}` |
|     54 |  264 | `			if( (pStep->iFlags & PH7_4EACH_STEP_REF) == 0` |
|     57 |  265 | `			 && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET) != 0 ){` |
|      - |  266 | `				/* PHP 8.4 property hooks: object iteration reads through the` |
|      - |  267 | `				 * get hook (virtual properties included; the flag gate keeps` |
|      - |  268 | `				 * hook-free classes on the raw zero-copy path below). The` |
|      - |  269 | `				 * object step walks hAttr with the hash's single EMBEDDED` |
|      - |  270 | `				 * cursor — save/restore it around the dispatch so a hook that` |
|      - |  271 | `				 * re-enters an hAttr walk on this instance (get_object_vars,` |
|      - |  272 | `				 * json_encode of $this) can't truncate THIS iteration. A hook` |
|      - |  273 | `				 * that unset()s the property the saved cursor points at stays` |
|      - |  274 | `				 * a recorded hazard (php's own semantics there are murky). */` |
|      - |  275 | `				ph7_value sHookVal;` |
|      - |  276 | `				sxi32 rcHk;` |
|     11 |  277 | `				SyHashEntry_Pr *pSavedCur = pThis->hAttr.pCurrent;` |
|     11 |  278 | `				PH7_MemObjInit(pVm,&sHookVal);` |
|     11 |  279 | `				rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);` |
|     11 |  280 | `				pThis->hAttr.pCurrent = pSavedCur;` |
|     11 |  281 | `				if( rcHk != SXERR_NOTFOUND ){` |
|     11 |  282 | `					if( rcHk == SXRET_OK ){` |
|     11 |  283 | `						pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|     11 |  284 | `						if( pValue ){` |
|     11 |  285 | `							PH7_MemObjStore(&sHookVal,pValue);` |
|      5 |  286 | `						}` |
|      5 |  287 | `					}` |
|      - |  288 | `					/* a throw parked on the boundary rail: the fetch-point` |
|      - |  289 | `					 * router lands it right after this op */` |
|     11 |  290 | `					PH7_MemObjRelease(&sHookVal);` |
|     11 |  291 | `					VM_EXIT_BREAK;` |
|      - |  292 | `				}` |
|    ! 0 |  293 | `				PH7_MemObjRelease(&sHookVal);` |
|    ! 0 |  294 | `			}` |
|      - |  295 | `			/* Extract attribute value */` |
|     48 |  296 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|     48 |  297 | `			if( pAttrValue ){` |
|     48 |  298 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|      - |  299 | `					/* Pass by reference */` |
|      3 |  300 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|      3 |  301 | `					if( pEntry ){` |
|      3 |  302 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|      2 |  303 | `					}else{` |
|    ! 0 |  304 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|    ! 0 |  305 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|      - |  306 | `					}` |
|      2 |  307 | `				}else{` |
|      - |  308 | `					/* Make a copy of the attribute value */` |
|     46 |  309 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|     46 |  310 | `					if( pValue ){` |
|     46 |  311 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|     21 |  312 | `					}` |
|      - |  313 | `				}` |
|     22 |  314 | `			}` |
|      - |  315 | `		}` |
|      - |  316 | `	}` |
| 298613 |  317 | `	VM_EXIT_BREAK;` |
| 149321 |  318 | `}` |
|      - |  319 |  |
|      - |  320 | `/*` |
|      - |  321 | ` * OP_FOREACH_INIT: body moved verbatim from the OP_FOREACH_INIT arm of` |
|      - |  322 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  323 | ` */` |
|  27520 |  324 | `PH7_PRIVATE VmOpRc VmExecOpForeachInit(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  325 | `{` |
|  27525 |  326 | `	ph7_value *pTos = pState->pTos;` |
|  27525 |  327 | `	ph7_value *pStack = pState->pStack;` |
|  27525 |  328 | `	VmInstr *aInstr = pState->aInstr;` |
|  27525 |  329 | `	sxi32 pc = pState->pc;` |
|      - |  330 | `	sxi32 rc;` |
|  27525 |  331 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|      - |  332 | `	void *pName;` |
|      - |  333 | `#ifdef UNTRUST` |
|      - |  334 | `	if( pTos < pStack ){` |
|      - |  335 | `		VM_EXIT_ABORT;` |
|      - |  336 | `	}` |
|      - |  337 | `#endif` |
|  27525 |  338 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|      - |  339 | `		/* Take the variable name from the top of the stack */` |
|    ! 0 |  340 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      - |  341 | `			/* Force a string cast */` |
|    ! 0 |  342 | `			PH7_MemObjToString(pTos);` |
|    ! 0 |  343 | `		}` |
|      - |  344 | `		/* Duplicate name */` |
|    ! 0 |  345 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|    ! 0 |  346 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|    ! 0 |  347 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|    ! 0 |  348 | `		}` |
|    ! 0 |  349 | `		VmPopOperand(&pTos,1);` |
|    ! 0 |  350 | `	}` |
|  27525 |  351 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|    ! 0 |  352 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      - |  353 | `			/* Force a string cast */` |
|    ! 0 |  354 | `			PH7_MemObjToString(pTos);` |
|    ! 0 |  355 | `		}` |
|      - |  356 | `		/* Duplicate name */` |
|    ! 0 |  357 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|    ! 0 |  358 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|    ! 0 |  359 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|    ! 0 |  360 | `		}` |
|    ! 0 |  361 | `		VmPopOperand(&pTos,1);` |
|    ! 0 |  362 | `	}` |
|      - |  363 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|  27525 |  364 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|      - |  365 | `		/* Jump out of the loop */` |
|      5 |  366 | `		if( SyStringLength(&pInfo->sValue) > 0 ){` |
|      - |  367 | `			/* php warns for EVERY non-iterable, null included (PH7 exempted null). */` |
|      7 |  368 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|      - |  369 | `				"foreach() argument must be of type array\|object, %s given",` |
|      2 |  370 | `				VmArithTypeName(pTos));` |
|      2 |  371 | `		}` |
|      5 |  372 | `		pc = pInstr->iP2 - 1;` |
|      3 |  373 | `	}else{` |
|      - |  374 | `		ph7_foreach_step *pStep;` |
|  27521 |  375 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|  27521 |  376 | `		if( pStep == 0 ){` |
|    ! 0 |  377 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      - |  378 | `			/* Jump out of the loop */` |
|    ! 0 |  379 | `			pc = pInstr->iP2 - 1;` |
|    ! 0 |  380 | `		}else{` |
|      - |  381 | `			/* Zero the structure */` |
|  27521 |  382 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|      - |  383 | `			/* Prepare the step */` |
|  27521 |  384 | `			pStep->iFlags = pInfo->iFlags;` |
|      - |  385 | `			/* Record the owning activation so OP_FOREACH_STEP can pick THIS` |
|      - |  386 | `			 * activation's step out of the per-statement stack — two suspended` |
|      - |  387 | `			 * generator/fiber instances (or a recursive call) paused in the same` |
|      - |  388 | `			 * textual foreach otherwise resume onto each other's cursor. */` |
|  27521 |  389 | `			pStep->pFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|  27521 |  390 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|      - |  391 | `				ph7_hashmap *pMap,*pIterMap;` |
|      - |  392 | `				/* COW: For by-reference foreach, eagerly separate the` |
|      - |  393 | `				 * source array so mutations don't affect other sharers. */` |
|  27229 |  394 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|     29 |  395 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|     29 |  396 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|     29 |  397 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|      - |  398 | `						/* Only adjust refcounts/separate if the backing` |
|      - |  399 | `						 * variable still points at the same hashmap as` |
|      - |  400 | `						 * the stack value. */` |
|     29 |  401 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|     29 |  402 | `							pCur->iRef--;` |
|      - |  403 | `							/* Use the returned map, not pBacking->x.pOther: PH7_HashmapDup` |
|      - |  404 | `							 * inside CowSeparate can reallocate (move) pVm->aMemObj and leave` |
|      - |  405 | `							 * pBacking dangling. The return value is the post-separation map. */` |
|     29 |  406 | `							pTos->x.pOther = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|     29 |  407 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|     13 |  408 | `						}` |
|     13 |  409 | `					}` |
|     13 |  410 | `				}` |
|  27229 |  411 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|  27229 |  412 | `				pIterMap = pMap;` |
|  27229 |  413 | `				if( pMap == pVm->pGlobal && (pStep->iFlags & PH7_4EACH_STEP_REF) == 0 ){` |
|      - |  414 | `					/* php 8.1: foreach ($GLOBALS as ...) by value iterates a` |
|      - |  415 | `					 * SNAPSHOT of the symbol table — globals created inside` |
|      - |  416 | `					 * the loop body must not be visited (the live map would` |
|      - |  417 | `					 * grow under the cursor). By-ref foreach keeps the live` |
|      - |  418 | `					 * map, like php. On OOM fall back to the live map. */` |
|      5 |  419 | `					ph7_hashmap *pSnap = PH7_NewHashmap(&(*pVm),0,0);` |
|      5 |  420 | `					if( pSnap && PH7_HashmapDupMaterialized(pMap,pSnap) == SXRET_OK ){` |
|      - |  421 | `						/* The step consumes the snapshot's initial reference */` |
|      5 |  422 | `						pIterMap = pSnap;` |
|      2 |  423 | `					}else if( pSnap ){` |
|    ! 0 |  424 | `						PH7_HashmapUnref(pSnap);` |
|    ! 0 |  425 | `					}` |
|      2 |  426 | `				}` |
|  27229 |  427 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|  27229 |  428 | `				pStep->xIter.pMap = pIterMap;` |
|  27229 |  429 | `				if( pIterMap == pMap ){` |
|  27225 |  430 | `					pMap->iRef++;` |
|  13610 |  431 | `				}` |
|      - |  432 | `				/* Private cursor + registry (php: nested foreach over one` |
|      - |  433 | `				 * array are independent; foreach never moves the internal` |
|      - |  434 | `				 * pointer — see PH7_HashmapRegisterForeachStep) */` |
|  27229 |  435 | `				PH7_HashmapRegisterForeachStep(pIterMap,pStep);` |
|  13617 |  436 | `			}else{` |
|    297 |  437 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|      - |  438 | `				ph7_class *pIteratorClass;` |
|      - |  439 | `				/* Check if the object implements Iterator */` |
|    297 |  440 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|    414 |  441 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|      - |  442 | `					/* Iterator-based iteration: call rewind() */` |
|      - |  443 | `					ph7_class_method *pRewind;` |
|    245 |  444 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|    245 |  445 | `					pStep->xIter.pThis = pThis;` |
|    245 |  446 | `					pThis->iRef++;` |
|    245 |  447 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|    245 |  448 | `					if( pRewind ){` |
|    245 |  449 | `						rc = PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|    245 |  450 | `						if( VmIterCallThrew(rc) ){` |
|      - |  451 | `							/* rewind() threw (a generator body or userland Iterator):` |
|      - |  452 | `							 * undo this step's retain, drop the step, and route the` |
|      - |  453 | `							 * exception instead of silently starting the loop. */` |
|      8 |  454 | `							pThis->iRef--;` |
|      8 |  455 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      8 |  456 | `							pStep = 0;` |
|      8 |  457 | `							PH7_DISPATCH_ITER_RC(rc,1)` |
|    ! 0 |  458 | `						}` |
|    117 |  459 | `					}` |
|    122 |  460 | `				}else{` |
|      - |  461 | `					/* Check if the object implements IteratorAggregate */` |
|      - |  462 | `					ph7_class *pIterAggClass;` |
|     56 |  463 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|      - |  464 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|     68 |  465 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|      - |  466 | `						/* Call getIterator() and use the returned Iterator object */` |
|      - |  467 | `						ph7_class_method *pGetIter;` |
|     26 |  468 | `						int iterAggOk = 0;` |
|     26 |  469 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|     26 |  470 | `						if( pGetIter ){` |
|      - |  471 | `							ph7_value sResult;` |
|     26 |  472 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|     26 |  473 | `							rc = PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|     26 |  474 | `							if( VmIterCallThrew(rc) ){` |
|      - |  475 | `								/* getIterator() threw: drop the step and route the` |
|      - |  476 | `								 * exception (don't pile the "must implement Iterator"` |
|      - |  477 | `								 * error on top of it). */` |
|    ! 0 |  478 | `								PH7_MemObjRelease(&sResult);` |
|    ! 0 |  479 | `								SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    ! 0 |  480 | `								pStep = 0;` |
|    ! 0 |  481 | `								PH7_DISPATCH_ITER_RC(rc,1)` |
|    ! 0 |  482 | `							}` |
|     26 |  483 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|     26 |  484 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|     26 |  485 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|      - |  486 | `									ph7_class_method *pRewind;` |
|     26 |  487 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|     26 |  488 | `									pStep->xIter.pThis = pIterObj;` |
|     26 |  489 | `									pIterObj->iRef++;` |
|      - |  490 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|     26 |  491 | `									pStep->pOwner = pThis;` |
|     26 |  492 | `									pThis->iRef++;` |
|     26 |  493 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|     26 |  494 | `									if( pRewind ){` |
|     26 |  495 | `										rc = PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|     26 |  496 | `										if( VmIterCallThrew(rc) ){` |
|      - |  497 | `											/* The aggregate's iterator rewind() threw: undo` |
|      - |  498 | `											 * both retains, drop the step, route the exception. */` |
|    ! 0 |  499 | `											pIterObj->iRef--;` |
|    ! 0 |  500 | `											pThis->iRef--;` |
|    ! 0 |  501 | `											PH7_MemObjRelease(&sResult);` |
|    ! 0 |  502 | `											SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    ! 0 |  503 | `											pStep = 0;` |
|    ! 0 |  504 | `											PH7_DISPATCH_ITER_RC(rc,1)` |
|    ! 0 |  505 | `										}` |
|     12 |  506 | `									}` |
|     26 |  507 | `									iterAggOk = 1;` |
|     12 |  508 | `								}` |
|     12 |  509 | `							}` |
|     26 |  510 | `							PH7_MemObjRelease(&sResult);` |
|     12 |  511 | `						}` |
|     26 |  512 | `						if( !iterAggOk ){` |
|      - |  513 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|    ! 0 |  514 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|      - |  515 | `								"Object returned by getIterator() must implement Iterator");` |
|    ! 0 |  516 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    ! 0 |  517 | `							pStep = 0; /* Signal: do not store this step */` |
|    ! 0 |  518 | `							pc = pInstr->iP2 - 1;` |
|    ! 0 |  519 | `						}` |
|     14 |  520 | `					}else{` |
|      - |  521 | `						/* Plain object iteration via hAttr */` |
|     32 |  522 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|     32 |  523 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|     32 |  524 | `						pStep->xIter.pThis = pThis;` |
|     32 |  525 | `						pThis->iRef++;` |
|      - |  526 | `					}` |
|      - |  527 | `				}` |
|      - |  528 | `			}` |
|      - |  529 | `		}` |
|  27515 |  530 | `		if( pStep ){` |
|  27515 |  531 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|    ! 0 |  532 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|    ! 0 |  533 | `				if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|    ! 0 |  534 | `					VmForeachHashmapStepRelease(&(*pVm),pInfo,pStep,FALSE/*never made it onto aStep*/);` |
|    ! 0 |  535 | `				}else{` |
|    ! 0 |  536 | `					SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      - |  537 | `				}` |
|      - |  538 | `				/* Jump out of the loop */` |
|    ! 0 |  539 | `				pc = pInstr->iP2 - 1;` |
|    ! 0 |  540 | `			}` |
|  13755 |  541 | `		}` |
|      - |  542 | `	}` |
|  27519 |  543 | `	VmPopOperand(&pTos,1);` |
|  27519 |  544 | `	VM_EXIT_BREAK;` |
|    ! 0 |  545 | `	VM_EXIT_BREAK;` |
|  13765 |  546 | `}` |
|      - |  547 |  |
