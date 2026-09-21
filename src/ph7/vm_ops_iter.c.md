# src/ph7/vm_ops_iter.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 272/340 lines (80.00%)

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
| 268562 |   30 | `PH7_PRIVATE VmOpRc VmExecOpForeachStep(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |   31 | `{` |
| 268567 |   32 | `	ph7_value *pTos = pState->pTos;` |
| 268567 |   33 | `	ph7_value *pStack = pState->pStack;` |
| 268567 |   34 | `	VmInstr *aInstr = pState->aInstr;` |
| 268567 |   35 | `	sxi32 pc = pState->pc;` |
|      - |   36 | `	sxi32 rc;` |
| 268567 |   37 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|      - |   38 | `	ph7_foreach_step **apStep,*pStep;` |
|      - |   39 | `	ph7_value *pValue;` |
|      - |   40 | `	VmFrame *pFrameLocal;` |
|      - |   41 | `	sxu32 nStep;` |
| 268567 |   42 | `	pFrameLocal = pVm->pFrame;` |
| 268567 |   43 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      - |   44 | `	/* Select THIS activation's step. aStep is per-STATEMENT and shared by every` |
|      - |   45 | `	 * activation, so peeking the last entry resumes onto a sibling's cursor when` |
|      - |   46 | `	 * two instances of one generator/fiber are suspended in the same textual` |
|      - |   47 | `	 * foreach. Scan from the top (most-recent push) for the step whose owning` |
|      - |   48 | `	 * frame matches the running activation; top-down makes the current push win` |
|      - |   49 | `	 * over any leaked older step that happens to share a recycled frame address. */` |
| 268567 |   50 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
| 268567 |   51 | `	nStep = SySetUsed(&pInfo->aStep);` |
| 268567 |   52 | `	if( nStep < 1 ){` |
|      - |   53 | `		/* Defensive: OP_FOREACH_INIT always pushes this activation's step before` |
|      - |   54 | `		 * STEP runs (and jumps past the loop when the push fails), so an empty` |
|      - |   55 | `		 * set is unreachable — guard the apStep[-1] read anyway. Jump out. */` |
|    ! 0 |   56 | `		pc = pInstr->iP2 - 1;` |
|    ! 0 |   57 | `		VM_EXIT_BREAK;` |
|      - |   58 | `	}` |
| 268567 |   59 | `	pStep = apStep[nStep - 1];` |
| 268593 |   60 | `	while( nStep > 0 ){` |
| 268593 |   61 | `		if( apStep[nStep - 1]->pFrame == pFrameLocal ){` |
| 268567 |   62 | `			pStep = apStep[nStep - 1];` |
| 268567 |   63 | `			break;` |
|      - |   64 | `		}` |
|     27 |   65 | `		nStep--;` |
|      1 |   66 | `	}` |
| 268567 |   67 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|      - |   68 | `		ph7_hashmap_node *pNode;` |
|      - |   69 | `		/* Extract the current node via this loop's PRIVATE cursor (php:` |
|      - |   70 | `		 * nested foreach over the same array are independent iterations) */` |
| 267143 |   71 | `		pNode = pStep->pCursor;` |
| 267143 |   72 | `		if( pNode == 0 ){` |
|      - |   73 | `			/* No more entry to process */` |
|  25001 |   74 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|  25001 |   75 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|      - |   76 | `				/* Break the reference with the last element */` |
|     27 |   77 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|     12 |   78 | `			}` |
|      - |   79 | `			/* Cleanup the mess left behind */` |
|  25001 |   80 | `			VmForeachHashmapStepRelease(&(*pVm),pInfo,pStep,TRUE);` |
|  12503 |   81 | `		}else{` |
|      - |   82 | `			/* Advance the private cursor */` |
| 242147 |   83 | `			pStep->pCursor = pNode->pPrev; /* Reverse link */` |
|      - |   84 | `			/* Bind the VALUE before the KEY: on the first iteration this is where` |
|      - |   85 | `			 * both locals are created in the frame table, and php's symbol table` |
|      - |   86 | `			 * lists the value ahead of the key (get_defined_vars() order). Only the` |
|      - |   87 | `			 * creation ORDER matters here; the stored values are independent. */` |
| 242147 |   88 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|      - |   89 | `				SyHashEntry *pEntry;` |
|      - |   90 | `				/* Pass by reference */` |
|     73 |   91 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|     73 |   92 | `				if( pEntry ){` |
|     53 |   93 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|     28 |   94 | `				}else{` |
|     33 |   95 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|     20 |   96 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|      - |   97 | `				}` |
|     38 |   98 | `			}else{` |
|      - |   99 | `				/* Make a copy of the entry value */` |
| 242077 |  100 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
| 242077 |  101 | `				if( pValue ){` |
| 242077 |  102 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
| 121036 |  103 | `				}` |
|      - |  104 | `			}` |
| 242147 |  105 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|  14737 |  106 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|  14737 |  107 | `				if( pKey ){` |
|  14737 |  108 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|   7366 |  109 | `				}` |
|   7366 |  110 | `			}` |
|      5 |  111 | `		}` |
| 134998 |  112 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|      - |  113 | `		/* Iterator-based iteration.` |
|      - |  114 | `		 * Sequence: on first call just check valid/current/key.` |
|      - |  115 | `		 * On subsequent calls, advance with next() first, then check.` |
|      - |  116 | `		 */` |
|   1351 |  117 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|      - |  118 | `		ph7_class_method *pMethod;` |
|      - |  119 | `		ph7_value sResult;` |
|   1351 |  120 | `		int isValid = 0;` |
|      - |  121 | `		/* Call next() to advance — but skip on the first iteration */` |
|   1351 |  122 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|    245 |  123 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|    125 |  124 | `		}else{` |
|   1111 |  125 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|   1111 |  126 | `			if( pMethod ){` |
|   1111 |  127 | `				rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|   1111 |  128 | `				if( VmIterCallThrew(rc) ){` |
|      - |  129 | `					/* next() threw (generator body / userland Iterator): tear the` |
|      - |  130 | `					 * step down like exhaustion does, then route the exception —` |
|      - |  131 | `					 * the loop must not silently end with execution continuing. */` |
|     15 |  132 | `					VmForeachStepAbandon(pVm,pInfo,pStep,pThis);` |
|     15 |  133 | `					PH7_DISPATCH_ITER_RC(rc,0)` |
|    ! 0 |  134 | `				}` |
|    547 |  135 | `			}` |
|      - |  136 | `		}` |
|      - |  137 | `		/* Call valid() */` |
|   1339 |  138 | `		PH7_MemObjInit(pVm,&sResult);` |
|   1339 |  139 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|   1339 |  140 | `		if( pMethod ){` |
|   1339 |  141 | `			rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|   1339 |  142 | `			if( VmIterCallThrew(rc) ){` |
|      - |  143 | `				/* valid() threw: same teardown-and-route as next() above. */` |
|    ! 0 |  144 | `				PH7_MemObjRelease(&sResult);` |
|    ! 0 |  145 | `				VmForeachStepAbandon(pVm,pInfo,pStep,pThis);` |
|    ! 0 |  146 | `				PH7_DISPATCH_ITER_RC(rc,0)` |
|    ! 0 |  147 | `			}` |
|   1339 |  148 | `			PH7_MemObjToBool(&sResult);` |
|   1339 |  149 | `			isValid = (sResult.x.iVal != 0);` |
|    667 |  150 | `		}` |
|   1339 |  151 | `		PH7_MemObjRelease(&sResult);` |
|   1339 |  152 | `		if( !isValid ){` |
|      - |  153 | `			/* Iterator exhausted */` |
|    223 |  154 | `			pc = pInstr->iP2 - 1;` |
|      - |  155 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|    223 |  156 | `			VmForeachStepAbandon(pVm,pInfo,pStep,pThis);` |
|    114 |  157 | `		}else{` |
|      - |  158 | `			/* Call current() to get value */` |
|   1121 |  159 | `			PH7_MemObjInit(pVm,&sResult);` |
|   1121 |  160 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|   1121 |  161 | `			if( pMethod ){` |
|   1121 |  162 | `				rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|   1121 |  163 | `				if( VmIterCallThrew(rc) ){` |
|      - |  164 | `					/* current() threw: same teardown-and-route as next() above. */` |
|    ! 0 |  165 | `					PH7_MemObjRelease(&sResult);` |
|    ! 0 |  166 | `					VmForeachStepAbandon(pVm,pInfo,pStep,pThis);` |
|    ! 0 |  167 | `					PH7_DISPATCH_ITER_RC(rc,0)` |
|    ! 0 |  168 | `				}` |
|    558 |  169 | `			}` |
|   1121 |  170 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   1121 |  171 | `			if( pValue ){` |
|   1121 |  172 | `				PH7_MemObjStore(&sResult,pValue);` |
|    558 |  173 | `			}` |
|   1121 |  174 | `			PH7_MemObjRelease(&sResult);` |
|      - |  175 | `			/* Call key() if needed */` |
|   1121 |  176 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      - |  177 | `				ph7_value sKey;` |
|    213 |  178 | `				PH7_MemObjInit(pVm,&sKey);` |
|    213 |  179 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|    213 |  180 | `				if( pMethod ){` |
|    213 |  181 | `					rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|    213 |  182 | `					if( VmIterCallThrew(rc) ){` |
|      - |  183 | `						/* key() threw: same teardown-and-route as next() above. */` |
|    ! 0 |  184 | `						PH7_MemObjRelease(&sKey);` |
|    ! 0 |  185 | `						VmForeachStepAbandon(pVm,pInfo,pStep,pThis);` |
|    ! 0 |  186 | `						PH7_DISPATCH_ITER_RC(rc,0)` |
|    ! 0 |  187 | `					}` |
|    104 |  188 | `				}` |
|    213 |  189 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|    213 |  190 | `				if( pValue ){` |
|    213 |  191 | `					PH7_MemObjStore(&sKey,pValue);` |
|    104 |  192 | `				}` |
|    213 |  193 | `				PH7_MemObjRelease(&sKey);` |
|    104 |  194 | `			}` |
|      - |  195 | `		}` |
|    672 |  196 | `	}else{` |
|     82 |  197 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|     82 |  198 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|      - |  199 | `		SyHashEntry *pEntry;` |
|      - |  200 | `		/* Point to the next attribute */` |
|     94 |  201 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|     70 |  202 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     66 |  203 | `			if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|     37 |  204 | `			 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|      3 |  205 | `				continue; /* virtual set-only property: iteration skips it (php) */` |
|      - |  206 | `			}` |
|      - |  207 | `			/* Check access permission */` |
|    100 |  208 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|     64 |  209 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|     58 |  210 | `					break; /* Access is granted */` |
|      - |  211 | `			}` |
|      2 |  212 | `		}` |
|     82 |  213 | `		if( pEntry == 0 ){` |
|      - |  214 | `			/* Clean up the mess left behind */` |
|     28 |  215 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|     28 |  216 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|      - |  217 | `				/* Break the reference with the last element */` |
|      3 |  218 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|      1 |  219 | `			}` |
|     28 |  220 | `			VmForeachStepUnlink(pInfo,pStep);` |
|     28 |  221 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     28 |  222 | `			PH7_ClassInstanceUnref(pThis);` |
|     16 |  223 | `		}else{` |
|     58 |  224 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|      - |  225 | `			ph7_value *pAttrValue;` |
|     58 |  226 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|      - |  227 | `				/* Fill with the current attribute name */` |
|     58 |  228 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|     58 |  229 | `				if( pKey ){` |
|     58 |  230 | `					SyBlobReset(&pKey->sBlob);` |
|     58 |  231 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|     58 |  232 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|     27 |  233 | `				}` |
|     27 |  234 | `			}` |
|     54 |  235 | `			if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET))` |
|     33 |  236 | `			 && (pStep->iFlags & PH7_4EACH_STEP_REF)` |
|     11 |  237 | `			 && !VmHookGuardHeld(pVm,(void *)pThis,&pVmAttr->pAttr->sName) ){` |
|      - |  238 | `				/* php: a hooked property (virtual or backed) cannot be iterated` |
|      - |  239 | `				 * by reference — catchable Error. Tear the step down like the` |
|      - |  240 | `				 * exhausted-iteration path (break the by-ref binding, unlink,` |
|      - |  241 | `				 * free, drop the instance retain) so nothing leaks and a` |
|      - |  242 | `				 * re-entered foreach starts fresh; the fetch-point router lands` |
|      - |  243 | `				 * the parked throw right after this op. Inside the property's` |
|      - |  244 | `				 * own hook body the guard keeps raw semantics (no Error). */` |
|      - |  245 | `				SyBlob sErrMsg;` |
|      3 |  246 | `				SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      3 |  247 | `				SyBlobFormat(&sErrMsg,"Cannot create reference to property %z::$%z",` |
|      2 |  248 | `					&pThis->pClass->sName,&pVmAttr->pAttr->sName);` |
|      3 |  249 | `				VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      3 |  250 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|      3 |  251 | `				VmForeachStepUnlink(pInfo,pStep);` |
|      3 |  252 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      3 |  253 | `				PH7_ClassInstanceUnref(pThis);` |
|      3 |  254 | `				VM_EXIT_BREAK;` |
|      - |  255 | `			}` |
|     52 |  256 | `			if( (pStep->iFlags & PH7_4EACH_STEP_REF) == 0` |
|     55 |  257 | `			 && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET) != 0 ){` |
|      - |  258 | `				/* PHP 8.4 property hooks: object iteration reads through the` |
|      - |  259 | `				 * get hook (virtual properties included; the flag gate keeps` |
|      - |  260 | `				 * hook-free classes on the raw zero-copy path below). The` |
|      - |  261 | `				 * object step walks hAttr with the hash's single EMBEDDED` |
|      - |  262 | `				 * cursor — save/restore it around the dispatch so a hook that` |
|      - |  263 | `				 * re-enters an hAttr walk on this instance (get_object_vars,` |
|      - |  264 | `				 * json_encode of $this) can't truncate THIS iteration. A hook` |
|      - |  265 | `				 * that unset()s the property the saved cursor points at stays` |
|      - |  266 | `				 * a recorded hazard (php's own semantics there are murky). */` |
|      - |  267 | `				ph7_value sHookVal;` |
|      - |  268 | `				sxi32 rcHk;` |
|     11 |  269 | `				SyHashEntry_Pr *pSavedCur = pThis->hAttr.pCurrent;` |
|     11 |  270 | `				PH7_MemObjInit(pVm,&sHookVal);` |
|     11 |  271 | `				rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);` |
|     11 |  272 | `				pThis->hAttr.pCurrent = pSavedCur;` |
|     11 |  273 | `				if( rcHk != SXERR_NOTFOUND ){` |
|     11 |  274 | `					if( rcHk == SXRET_OK ){` |
|     11 |  275 | `						pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|     11 |  276 | `						if( pValue ){` |
|     11 |  277 | `							PH7_MemObjStore(&sHookVal,pValue);` |
|      5 |  278 | `						}` |
|      5 |  279 | `					}` |
|      - |  280 | `					/* a throw parked on the boundary rail: the fetch-point` |
|      - |  281 | `					 * router lands it right after this op */` |
|     11 |  282 | `					PH7_MemObjRelease(&sHookVal);` |
|     11 |  283 | `					VM_EXIT_BREAK;` |
|      - |  284 | `				}` |
|    ! 0 |  285 | `				PH7_MemObjRelease(&sHookVal);` |
|    ! 0 |  286 | `			}` |
|      - |  287 | `			/* Extract attribute value */` |
|     46 |  288 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|     46 |  289 | `			if( pAttrValue ){` |
|     46 |  290 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|      - |  291 | `					/* Pass by reference */` |
|      3 |  292 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|      3 |  293 | `					if( pEntry ){` |
|      3 |  294 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|      2 |  295 | `					}else{` |
|    ! 0 |  296 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|    ! 0 |  297 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|      - |  298 | `					}` |
|      2 |  299 | `				}else{` |
|      - |  300 | `					/* Make a copy of the attribute value */` |
|     44 |  301 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|     44 |  302 | `					if( pValue ){` |
|     44 |  303 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|     20 |  304 | `					}` |
|      - |  305 | `				}` |
|     21 |  306 | `			}` |
|      - |  307 | `		}` |
|      - |  308 | `	}` |
| 268543 |  309 | `	VM_EXIT_BREAK;` |
| 134286 |  310 | `}` |
|      - |  311 |  |
|      - |  312 | `/*` |
|      - |  313 | ` * OP_FOREACH_INIT: body moved verbatim from the OP_FOREACH_INIT arm of` |
|      - |  314 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  315 | ` */` |
|  25452 |  316 | `PH7_PRIVATE VmOpRc VmExecOpForeachInit(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  317 | `{` |
|  25457 |  318 | `	ph7_value *pTos = pState->pTos;` |
|  25457 |  319 | `	ph7_value *pStack = pState->pStack;` |
|  25457 |  320 | `	VmInstr *aInstr = pState->aInstr;` |
|  25457 |  321 | `	sxi32 pc = pState->pc;` |
|      - |  322 | `	sxi32 rc;` |
|  25457 |  323 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|      - |  324 | `	void *pName;` |
|      - |  325 | `#ifdef UNTRUST` |
|      - |  326 | `	if( pTos < pStack ){` |
|      - |  327 | `		VM_EXIT_ABORT;` |
|      - |  328 | `	}` |
|      - |  329 | `#endif` |
|  25457 |  330 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|      - |  331 | `		/* Take the variable name from the top of the stack */` |
|    ! 0 |  332 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      - |  333 | `			/* Force a string cast */` |
|    ! 0 |  334 | `			PH7_MemObjToString(pTos);` |
|    ! 0 |  335 | `		}` |
|      - |  336 | `		/* Duplicate name */` |
|    ! 0 |  337 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|    ! 0 |  338 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|    ! 0 |  339 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|    ! 0 |  340 | `		}` |
|    ! 0 |  341 | `		VmPopOperand(&pTos,1);` |
|    ! 0 |  342 | `	}` |
|  25457 |  343 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|    ! 0 |  344 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      - |  345 | `			/* Force a string cast */` |
|    ! 0 |  346 | `			PH7_MemObjToString(pTos);` |
|    ! 0 |  347 | `		}` |
|      - |  348 | `		/* Duplicate name */` |
|    ! 0 |  349 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|    ! 0 |  350 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|    ! 0 |  351 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|    ! 0 |  352 | `		}` |
|    ! 0 |  353 | `		VmPopOperand(&pTos,1);` |
|    ! 0 |  354 | `	}` |
|      - |  355 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|  25457 |  356 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|      - |  357 | `		/* Jump out of the loop */` |
|      5 |  358 | `		if( SyStringLength(&pInfo->sValue) > 0 ){` |
|      - |  359 | `			/* php warns for EVERY non-iterable, null included (PH7 exempted null). */` |
|      7 |  360 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|      - |  361 | `				"foreach() argument must be of type array\|object, %s given",` |
|      2 |  362 | `				VmArithTypeName(pTos));` |
|      2 |  363 | `		}` |
|      5 |  364 | `		pc = pInstr->iP2 - 1;` |
|      3 |  365 | `	}else{` |
|      - |  366 | `		ph7_foreach_step *pStep;` |
|  25453 |  367 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|  25453 |  368 | `		if( pStep == 0 ){` |
|    ! 0 |  369 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      - |  370 | `			/* Jump out of the loop */` |
|    ! 0 |  371 | `			pc = pInstr->iP2 - 1;` |
|    ! 0 |  372 | `		}else{` |
|      - |  373 | `			/* Zero the structure */` |
|  25453 |  374 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|      - |  375 | `			/* Prepare the step */` |
|  25453 |  376 | `			pStep->iFlags = pInfo->iFlags;` |
|      - |  377 | `			/* Record the owning activation so OP_FOREACH_STEP can pick THIS` |
|      - |  378 | `			 * activation's step out of the per-statement stack — two suspended` |
|      - |  379 | `			 * generator/fiber instances (or a recursive call) paused in the same` |
|      - |  380 | `			 * textual foreach otherwise resume onto each other's cursor. */` |
|  25453 |  381 | `			pStep->pFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|  25453 |  382 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|      - |  383 | `				ph7_hashmap *pMap,*pIterMap;` |
|      - |  384 | `				/* COW: For by-reference foreach, eagerly separate the` |
|      - |  385 | `				 * source array so mutations don't affect other sharers. */` |
|  25181 |  386 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|     29 |  387 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|     29 |  388 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|     29 |  389 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|      - |  390 | `						/* Only adjust refcounts/separate if the backing` |
|      - |  391 | `						 * variable still points at the same hashmap as` |
|      - |  392 | `						 * the stack value. */` |
|     29 |  393 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|     29 |  394 | `							pCur->iRef--;` |
|      - |  395 | `							/* Use the returned map, not pBacking->x.pOther: PH7_HashmapDup` |
|      - |  396 | `							 * inside CowSeparate can reallocate (move) pVm->aMemObj and leave` |
|      - |  397 | `							 * pBacking dangling. The return value is the post-separation map. */` |
|     29 |  398 | `							pTos->x.pOther = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|     29 |  399 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|     13 |  400 | `						}` |
|     13 |  401 | `					}` |
|     13 |  402 | `				}` |
|  25181 |  403 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|  25181 |  404 | `				pIterMap = pMap;` |
|  25181 |  405 | `				if( pMap == pVm->pGlobal && (pStep->iFlags & PH7_4EACH_STEP_REF) == 0 ){` |
|      - |  406 | `					/* php 8.1: foreach ($GLOBALS as ...) by value iterates a` |
|      - |  407 | `					 * SNAPSHOT of the symbol table — globals created inside` |
|      - |  408 | `					 * the loop body must not be visited (the live map would` |
|      - |  409 | `					 * grow under the cursor). By-ref foreach keeps the live` |
|      - |  410 | `					 * map, like php. On OOM fall back to the live map. */` |
|      5 |  411 | `					ph7_hashmap *pSnap = PH7_NewHashmap(&(*pVm),0,0);` |
|      5 |  412 | `					if( pSnap && PH7_HashmapDupMaterialized(pMap,pSnap) == SXRET_OK ){` |
|      - |  413 | `						/* The step consumes the snapshot's initial reference */` |
|      5 |  414 | `						pIterMap = pSnap;` |
|      2 |  415 | `					}else if( pSnap ){` |
|    ! 0 |  416 | `						PH7_HashmapUnref(pSnap);` |
|    ! 0 |  417 | `					}` |
|      2 |  418 | `				}` |
|  25181 |  419 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|  25181 |  420 | `				pStep->xIter.pMap = pIterMap;` |
|  25181 |  421 | `				if( pIterMap == pMap ){` |
|  25177 |  422 | `					pMap->iRef++;` |
|  12586 |  423 | `				}` |
|      - |  424 | `				/* Private cursor + registry (php: nested foreach over one` |
|      - |  425 | `				 * array are independent; foreach never moves the internal` |
|      - |  426 | `				 * pointer — see PH7_HashmapRegisterForeachStep) */` |
|  25181 |  427 | `				PH7_HashmapRegisterForeachStep(pIterMap,pStep);` |
|  12593 |  428 | `			}else{` |
|    277 |  429 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|      - |  430 | `				ph7_class *pIteratorClass;` |
|      - |  431 | `				/* Check if the object implements Iterator */` |
|    277 |  432 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|    386 |  433 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|      - |  434 | `					/* Iterator-based iteration: call rewind() */` |
|      - |  435 | `					ph7_class_method *pRewind;` |
|    229 |  436 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|    229 |  437 | `					pStep->xIter.pThis = pThis;` |
|    229 |  438 | `					pThis->iRef++;` |
|    229 |  439 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|    229 |  440 | `					if( pRewind ){` |
|    229 |  441 | `						rc = PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|    229 |  442 | `						if( VmIterCallThrew(rc) ){` |
|      - |  443 | `							/* rewind() threw (a generator body or userland Iterator):` |
|      - |  444 | `							 * undo this step's retain, drop the step, and route the` |
|      - |  445 | `							 * exception instead of silently starting the loop. */` |
|      9 |  446 | `							pThis->iRef--;` |
|      9 |  447 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      9 |  448 | `							pStep = 0;` |
|      9 |  449 | `							PH7_DISPATCH_ITER_RC(rc,1)` |
|    ! 0 |  450 | `						}` |
|    109 |  451 | `					}` |
|    114 |  452 | `				}else{` |
|      - |  453 | `					/* Check if the object implements IteratorAggregate */` |
|      - |  454 | `					ph7_class *pIterAggClass;` |
|     52 |  455 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|      - |  456 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|     63 |  457 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|      - |  458 | `						/* Call getIterator() and use the returned Iterator object */` |
|      - |  459 | `						ph7_class_method *pGetIter;` |
|     24 |  460 | `						int iterAggOk = 0;` |
|     24 |  461 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|     24 |  462 | `						if( pGetIter ){` |
|      - |  463 | `							ph7_value sResult;` |
|     24 |  464 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|     24 |  465 | `							rc = PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|     24 |  466 | `							if( VmIterCallThrew(rc) ){` |
|      - |  467 | `								/* getIterator() threw: drop the step and route the` |
|      - |  468 | `								 * exception (don't pile the "must implement Iterator"` |
|      - |  469 | `								 * error on top of it). */` |
|    ! 0 |  470 | `								PH7_MemObjRelease(&sResult);` |
|    ! 0 |  471 | `								SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    ! 0 |  472 | `								pStep = 0;` |
|    ! 0 |  473 | `								PH7_DISPATCH_ITER_RC(rc,1)` |
|    ! 0 |  474 | `							}` |
|     24 |  475 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|     24 |  476 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|     24 |  477 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|      - |  478 | `									ph7_class_method *pRewind;` |
|     24 |  479 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|     24 |  480 | `									pStep->xIter.pThis = pIterObj;` |
|     24 |  481 | `									pIterObj->iRef++;` |
|      - |  482 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|     24 |  483 | `									pStep->pOwner = pThis;` |
|     24 |  484 | `									pThis->iRef++;` |
|     24 |  485 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|     24 |  486 | `									if( pRewind ){` |
|     24 |  487 | `										rc = PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|     24 |  488 | `										if( VmIterCallThrew(rc) ){` |
|      - |  489 | `											/* The aggregate's iterator rewind() threw: undo` |
|      - |  490 | `											 * both retains, drop the step, route the exception. */` |
|    ! 0 |  491 | `											pIterObj->iRef--;` |
|    ! 0 |  492 | `											pThis->iRef--;` |
|    ! 0 |  493 | `											PH7_MemObjRelease(&sResult);` |
|    ! 0 |  494 | `											SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    ! 0 |  495 | `											pStep = 0;` |
|    ! 0 |  496 | `											PH7_DISPATCH_ITER_RC(rc,1)` |
|    ! 0 |  497 | `										}` |
|     11 |  498 | `									}` |
|     24 |  499 | `									iterAggOk = 1;` |
|     11 |  500 | `								}` |
|     11 |  501 | `							}` |
|     24 |  502 | `							PH7_MemObjRelease(&sResult);` |
|     11 |  503 | `						}` |
|     24 |  504 | `						if( !iterAggOk ){` |
|      - |  505 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|    ! 0 |  506 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|      - |  507 | `								"Object returned by getIterator() must implement Iterator");` |
|    ! 0 |  508 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    ! 0 |  509 | `							pStep = 0; /* Signal: do not store this step */` |
|    ! 0 |  510 | `							pc = pInstr->iP2 - 1;` |
|    ! 0 |  511 | `						}` |
|     13 |  512 | `					}else{` |
|      - |  513 | `						/* Plain object iteration via hAttr */` |
|     30 |  514 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|     30 |  515 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|     30 |  516 | `						pStep->xIter.pThis = pThis;` |
|     30 |  517 | `						pThis->iRef++;` |
|      - |  518 | `					}` |
|      - |  519 | `				}` |
|      - |  520 | `			}` |
|      - |  521 | `		}` |
|  25447 |  522 | `		if( pStep ){` |
|  25447 |  523 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|    ! 0 |  524 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|    ! 0 |  525 | `				if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|    ! 0 |  526 | `					VmForeachHashmapStepRelease(&(*pVm),pInfo,pStep,FALSE/*never made it onto aStep*/);` |
|    ! 0 |  527 | `				}else{` |
|    ! 0 |  528 | `					SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      - |  529 | `				}` |
|      - |  530 | `				/* Jump out of the loop */` |
|    ! 0 |  531 | `				pc = pInstr->iP2 - 1;` |
|    ! 0 |  532 | `			}` |
|  12721 |  533 | `		}` |
|      - |  534 | `	}` |
|  25451 |  535 | `	VmPopOperand(&pTos,1);` |
|  25451 |  536 | `	VM_EXIT_BREAK;` |
|    ! 0 |  537 | `	VM_EXIT_BREAK;` |
|  12731 |  538 | `}` |
|      - |  539 |  |
