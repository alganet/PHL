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
| 242190 |   30 | `PH7_PRIVATE VmOpRc VmExecOpForeachStep(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |   31 | `{` |
| 242195 |   32 | `	ph7_value *pTos = pState->pTos;` |
| 242195 |   33 | `	ph7_value *pStack = pState->pStack;` |
| 242195 |   34 | `	VmInstr *aInstr = pState->aInstr;` |
| 242195 |   35 | `	sxi32 pc = pState->pc;` |
|      - |   36 | `	sxi32 rc;` |
| 242195 |   37 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|      - |   38 | `	ph7_foreach_step **apStep,*pStep;` |
|      - |   39 | `	ph7_value *pValue;` |
|      - |   40 | `	VmFrame *pFrameLocal;` |
|      - |   41 | `	sxu32 nStep;` |
| 242195 |   42 | `	pFrameLocal = pVm->pFrame;` |
| 242195 |   43 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      - |   44 | `	/* Select THIS activation's step. aStep is per-STATEMENT and shared by every` |
|      - |   45 | `	 * activation, so peeking the last entry resumes onto a sibling's cursor when` |
|      - |   46 | `	 * two instances of one generator/fiber are suspended in the same textual` |
|      - |   47 | `	 * foreach. Scan from the top (most-recent push) for the step whose owning` |
|      - |   48 | `	 * frame matches the running activation; top-down makes the current push win` |
|      - |   49 | `	 * over any leaked older step that happens to share a recycled frame address. */` |
| 242195 |   50 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
| 242195 |   51 | `	nStep = SySetUsed(&pInfo->aStep);` |
| 242195 |   52 | `	if( nStep < 1 ){` |
|      - |   53 | `		/* Defensive: OP_FOREACH_INIT always pushes this activation's step before` |
|      - |   54 | `		 * STEP runs (and jumps past the loop when the push fails), so an empty` |
|      - |   55 | `		 * set is unreachable — guard the apStep[-1] read anyway. Jump out. */` |
|    ! 0 |   56 | `		pc = pInstr->iP2 - 1;` |
|    ! 0 |   57 | `		VM_EXIT_BREAK;` |
|      - |   58 | `	}` |
| 242195 |   59 | `	pStep = apStep[nStep - 1];` |
| 242221 |   60 | `	while( nStep > 0 ){` |
| 242221 |   61 | `		if( apStep[nStep - 1]->pFrame == pFrameLocal ){` |
| 242195 |   62 | `			pStep = apStep[nStep - 1];` |
| 242195 |   63 | `			break;` |
|      - |   64 | `		}` |
|     27 |   65 | `		nStep--;` |
|      1 |   66 | `	}` |
| 242195 |   67 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|      - |   68 | `		ph7_hashmap_node *pNode;` |
|      - |   69 | `		/* Extract the current node via this loop's PRIVATE cursor (php:` |
|      - |   70 | `		 * nested foreach over the same array are independent iterations) */` |
| 240809 |   71 | `		pNode = pStep->pCursor;` |
| 240809 |   72 | `		if( pNode == 0 ){` |
|      - |   73 | `			/* No more entry to process */` |
|  23673 |   74 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|  23673 |   75 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|      - |   76 | `				/* Break the reference with the last element */` |
|     27 |   77 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|     12 |   78 | `			}` |
|      - |   79 | `			/* Cleanup the mess left behind */` |
|  23673 |   80 | `			VmForeachHashmapStepRelease(&(*pVm),pInfo,pStep,TRUE);` |
|  11839 |   81 | `		}else{` |
|      - |   82 | `			/* Advance the private cursor */` |
| 217141 |   83 | `			pStep->pCursor = pNode->pPrev; /* Reverse link */` |
| 217141 |   84 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|   6977 |   85 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|   6977 |   86 | `				if( pKey ){` |
|   6977 |   87 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|   3486 |   88 | `				}` |
|   3486 |   89 | `			}` |
| 217141 |   90 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|      - |   91 | `				SyHashEntry *pEntry;` |
|      - |   92 | `				/* Pass by reference */` |
|     73 |   93 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|     73 |   94 | `				if( pEntry ){` |
|     53 |   95 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|     28 |   96 | `				}else{` |
|     33 |   97 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|     20 |   98 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|      - |   99 | `				}` |
|     38 |  100 | `			}else{` |
|      - |  101 | `				/* Make a copy of the entry value */` |
| 217071 |  102 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
| 217071 |  103 | `				if( pValue ){` |
| 217071 |  104 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
| 108533 |  105 | `				}` |
|      - |  106 | `			}` |
|      5 |  107 | `		}` |
| 121793 |  108 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|      - |  109 | `		/* Iterator-based iteration.` |
|      - |  110 | `		 * Sequence: on first call just check valid/current/key.` |
|      - |  111 | `		 * On subsequent calls, advance with next() first, then check.` |
|      - |  112 | `		 */` |
|   1313 |  113 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|      - |  114 | `		ph7_class_method *pMethod;` |
|      - |  115 | `		ph7_value sResult;` |
|   1313 |  116 | `		int isValid = 0;` |
|      - |  117 | `		/* Call next() to advance — but skip on the first iteration */` |
|   1313 |  118 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|    225 |  119 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|    115 |  120 | `		}else{` |
|   1093 |  121 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|   1093 |  122 | `			if( pMethod ){` |
|   1093 |  123 | `				rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|   1093 |  124 | `				if( VmIterCallThrew(rc) ){` |
|      - |  125 | `					/* next() threw (generator body / userland Iterator): tear the` |
|      - |  126 | `					 * step down like exhaustion does, then route the exception —` |
|      - |  127 | `					 * the loop must not silently end with execution continuing. */` |
|     15 |  128 | `					VmForeachStepAbandon(pVm,pInfo,pStep,pThis);` |
|     15 |  129 | `					PH7_DISPATCH_ITER_RC(rc,0)` |
|    ! 0 |  130 | `				}` |
|    538 |  131 | `			}` |
|      - |  132 | `		}` |
|      - |  133 | `		/* Call valid() */` |
|   1301 |  134 | `		PH7_MemObjInit(pVm,&sResult);` |
|   1301 |  135 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|   1301 |  136 | `		if( pMethod ){` |
|   1301 |  137 | `			rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|   1301 |  138 | `			if( VmIterCallThrew(rc) ){` |
|      - |  139 | `				/* valid() threw: same teardown-and-route as next() above. */` |
|    ! 0 |  140 | `				PH7_MemObjRelease(&sResult);` |
|    ! 0 |  141 | `				VmForeachStepAbandon(pVm,pInfo,pStep,pThis);` |
|    ! 0 |  142 | `				PH7_DISPATCH_ITER_RC(rc,0)` |
|    ! 0 |  143 | `			}` |
|   1301 |  144 | `			PH7_MemObjToBool(&sResult);` |
|   1301 |  145 | `			isValid = (sResult.x.iVal != 0);` |
|    648 |  146 | `		}` |
|   1301 |  147 | `		PH7_MemObjRelease(&sResult);` |
|   1301 |  148 | `		if( !isValid ){` |
|      - |  149 | `			/* Iterator exhausted */` |
|    203 |  150 | `			pc = pInstr->iP2 - 1;` |
|      - |  151 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|    203 |  152 | `			VmForeachStepAbandon(pVm,pInfo,pStep,pThis);` |
|    104 |  153 | `		}else{` |
|      - |  154 | `			/* Call current() to get value */` |
|   1103 |  155 | `			PH7_MemObjInit(pVm,&sResult);` |
|   1103 |  156 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|   1103 |  157 | `			if( pMethod ){` |
|   1103 |  158 | `				rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|   1103 |  159 | `				if( VmIterCallThrew(rc) ){` |
|      - |  160 | `					/* current() threw: same teardown-and-route as next() above. */` |
|    ! 0 |  161 | `					PH7_MemObjRelease(&sResult);` |
|    ! 0 |  162 | `					VmForeachStepAbandon(pVm,pInfo,pStep,pThis);` |
|    ! 0 |  163 | `					PH7_DISPATCH_ITER_RC(rc,0)` |
|    ! 0 |  164 | `				}` |
|    549 |  165 | `			}` |
|   1103 |  166 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   1103 |  167 | `			if( pValue ){` |
|   1103 |  168 | `				PH7_MemObjStore(&sResult,pValue);` |
|    549 |  169 | `			}` |
|   1103 |  170 | `			PH7_MemObjRelease(&sResult);` |
|      - |  171 | `			/* Call key() if needed */` |
|   1103 |  172 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      - |  173 | `				ph7_value sKey;` |
|    212 |  174 | `				PH7_MemObjInit(pVm,&sKey);` |
|    212 |  175 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|    212 |  176 | `				if( pMethod ){` |
|    212 |  177 | `					rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|    212 |  178 | `					if( VmIterCallThrew(rc) ){` |
|      - |  179 | `						/* key() threw: same teardown-and-route as next() above. */` |
|    ! 0 |  180 | `						PH7_MemObjRelease(&sKey);` |
|    ! 0 |  181 | `						VmForeachStepAbandon(pVm,pInfo,pStep,pThis);` |
|    ! 0 |  182 | `						PH7_DISPATCH_ITER_RC(rc,0)` |
|    ! 0 |  183 | `					}` |
|    104 |  184 | `				}` |
|    212 |  185 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|    212 |  186 | `				if( pValue ){` |
|    212 |  187 | `					PH7_MemObjStore(&sKey,pValue);` |
|    104 |  188 | `				}` |
|    212 |  189 | `				PH7_MemObjRelease(&sKey);` |
|    104 |  190 | `			}` |
|      - |  191 | `		}` |
|    653 |  192 | `	}else{` |
|     82 |  193 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|     82 |  194 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|      - |  195 | `		SyHashEntry *pEntry;` |
|      - |  196 | `		/* Point to the next attribute */` |
|     94 |  197 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|     70 |  198 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     66 |  199 | `			if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|     37 |  200 | `			 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|      3 |  201 | `				continue; /* virtual set-only property: iteration skips it (php) */` |
|      - |  202 | `			}` |
|      - |  203 | `			/* Check access permission */` |
|    100 |  204 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|     64 |  205 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|     58 |  206 | `					break; /* Access is granted */` |
|      - |  207 | `			}` |
|      2 |  208 | `		}` |
|     82 |  209 | `		if( pEntry == 0 ){` |
|      - |  210 | `			/* Clean up the mess left behind */` |
|     28 |  211 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|     28 |  212 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|      - |  213 | `				/* Break the reference with the last element */` |
|      3 |  214 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|      1 |  215 | `			}` |
|     28 |  216 | `			VmForeachStepUnlink(pInfo,pStep);` |
|     28 |  217 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     28 |  218 | `			PH7_ClassInstanceUnref(pThis);` |
|     16 |  219 | `		}else{` |
|     58 |  220 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|      - |  221 | `			ph7_value *pAttrValue;` |
|     58 |  222 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|      - |  223 | `				/* Fill with the current attribute name */` |
|     58 |  224 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|     58 |  225 | `				if( pKey ){` |
|     58 |  226 | `					SyBlobReset(&pKey->sBlob);` |
|     58 |  227 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|     58 |  228 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|     27 |  229 | `				}` |
|     27 |  230 | `			}` |
|     54 |  231 | `			if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET))` |
|     33 |  232 | `			 && (pStep->iFlags & PH7_4EACH_STEP_REF)` |
|     11 |  233 | `			 && !VmHookGuardHeld(pVm,(void *)pThis,&pVmAttr->pAttr->sName) ){` |
|      - |  234 | `				/* php: a hooked property (virtual or backed) cannot be iterated` |
|      - |  235 | `				 * by reference — catchable Error. Tear the step down like the` |
|      - |  236 | `				 * exhausted-iteration path (break the by-ref binding, unlink,` |
|      - |  237 | `				 * free, drop the instance retain) so nothing leaks and a` |
|      - |  238 | `				 * re-entered foreach starts fresh; the fetch-point router lands` |
|      - |  239 | `				 * the parked throw right after this op. Inside the property's` |
|      - |  240 | `				 * own hook body the guard keeps raw semantics (no Error). */` |
|      - |  241 | `				SyBlob sErrMsg;` |
|      3 |  242 | `				SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      3 |  243 | `				SyBlobFormat(&sErrMsg,"Cannot create reference to property %z::$%z",` |
|      2 |  244 | `					&pThis->pClass->sName,&pVmAttr->pAttr->sName);` |
|      3 |  245 | `				VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      3 |  246 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|      3 |  247 | `				VmForeachStepUnlink(pInfo,pStep);` |
|      3 |  248 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      3 |  249 | `				PH7_ClassInstanceUnref(pThis);` |
|      3 |  250 | `				VM_EXIT_BREAK;` |
|      - |  251 | `			}` |
|     52 |  252 | `			if( (pStep->iFlags & PH7_4EACH_STEP_REF) == 0` |
|     55 |  253 | `			 && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET) != 0 ){` |
|      - |  254 | `				/* PHP 8.4 property hooks: object iteration reads through the` |
|      - |  255 | `				 * get hook (virtual properties included; the flag gate keeps` |
|      - |  256 | `				 * hook-free classes on the raw zero-copy path below). The` |
|      - |  257 | `				 * object step walks hAttr with the hash's single EMBEDDED` |
|      - |  258 | `				 * cursor — save/restore it around the dispatch so a hook that` |
|      - |  259 | `				 * re-enters an hAttr walk on this instance (get_object_vars,` |
|      - |  260 | `				 * json_encode of $this) can't truncate THIS iteration. A hook` |
|      - |  261 | `				 * that unset()s the property the saved cursor points at stays` |
|      - |  262 | `				 * a recorded hazard (php's own semantics there are murky). */` |
|      - |  263 | `				ph7_value sHookVal;` |
|      - |  264 | `				sxi32 rcHk;` |
|     11 |  265 | `				SyHashEntry_Pr *pSavedCur = pThis->hAttr.pCurrent;` |
|     11 |  266 | `				PH7_MemObjInit(pVm,&sHookVal);` |
|     11 |  267 | `				rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);` |
|     11 |  268 | `				pThis->hAttr.pCurrent = pSavedCur;` |
|     11 |  269 | `				if( rcHk != SXERR_NOTFOUND ){` |
|     11 |  270 | `					if( rcHk == SXRET_OK ){` |
|     11 |  271 | `						pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|     11 |  272 | `						if( pValue ){` |
|     11 |  273 | `							PH7_MemObjStore(&sHookVal,pValue);` |
|      5 |  274 | `						}` |
|      5 |  275 | `					}` |
|      - |  276 | `					/* a throw parked on the boundary rail: the fetch-point` |
|      - |  277 | `					 * router lands it right after this op */` |
|     11 |  278 | `					PH7_MemObjRelease(&sHookVal);` |
|     11 |  279 | `					VM_EXIT_BREAK;` |
|      - |  280 | `				}` |
|    ! 0 |  281 | `				PH7_MemObjRelease(&sHookVal);` |
|    ! 0 |  282 | `			}` |
|      - |  283 | `			/* Extract attribute value */` |
|     46 |  284 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|     46 |  285 | `			if( pAttrValue ){` |
|     46 |  286 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|      - |  287 | `					/* Pass by reference */` |
|      3 |  288 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|      3 |  289 | `					if( pEntry ){` |
|      3 |  290 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|      2 |  291 | `					}else{` |
|    ! 0 |  292 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|    ! 0 |  293 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|      - |  294 | `					}` |
|      2 |  295 | `				}else{` |
|      - |  296 | `					/* Make a copy of the attribute value */` |
|     44 |  297 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|     44 |  298 | `					if( pValue ){` |
|     44 |  299 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|     20 |  300 | `					}` |
|      - |  301 | `				}` |
|     21 |  302 | `			}` |
|      - |  303 | `		}` |
|      - |  304 | `	}` |
| 242171 |  305 | `	VM_EXIT_BREAK;` |
| 121100 |  306 | `}` |
|      - |  307 |  |
|      - |  308 | `/*` |
|      - |  309 | ` * OP_FOREACH_INIT: body moved verbatim from the OP_FOREACH_INIT arm of` |
|      - |  310 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  311 | ` */` |
|  24100 |  312 | `PH7_PRIVATE VmOpRc VmExecOpForeachInit(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  313 | `{` |
|  24105 |  314 | `	ph7_value *pTos = pState->pTos;` |
|  24105 |  315 | `	ph7_value *pStack = pState->pStack;` |
|  24105 |  316 | `	VmInstr *aInstr = pState->aInstr;` |
|  24105 |  317 | `	sxi32 pc = pState->pc;` |
|      - |  318 | `	sxi32 rc;` |
|  24105 |  319 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|      - |  320 | `	void *pName;` |
|      - |  321 | `#ifdef UNTRUST` |
|      - |  322 | `	if( pTos < pStack ){` |
|      - |  323 | `		VM_EXIT_ABORT;` |
|      - |  324 | `	}` |
|      - |  325 | `#endif` |
|  24105 |  326 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|      - |  327 | `		/* Take the variable name from the top of the stack */` |
|    ! 0 |  328 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      - |  329 | `			/* Force a string cast */` |
|    ! 0 |  330 | `			PH7_MemObjToString(pTos);` |
|    ! 0 |  331 | `		}` |
|      - |  332 | `		/* Duplicate name */` |
|    ! 0 |  333 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|    ! 0 |  334 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|    ! 0 |  335 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|    ! 0 |  336 | `		}` |
|    ! 0 |  337 | `		VmPopOperand(&pTos,1);` |
|    ! 0 |  338 | `	}` |
|  24105 |  339 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|    ! 0 |  340 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      - |  341 | `			/* Force a string cast */` |
|    ! 0 |  342 | `			PH7_MemObjToString(pTos);` |
|    ! 0 |  343 | `		}` |
|      - |  344 | `		/* Duplicate name */` |
|    ! 0 |  345 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|    ! 0 |  346 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|    ! 0 |  347 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|    ! 0 |  348 | `		}` |
|    ! 0 |  349 | `		VmPopOperand(&pTos,1);` |
|    ! 0 |  350 | `	}` |
|      - |  351 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|  24105 |  352 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|      - |  353 | `		/* Jump out of the loop */` |
|      5 |  354 | `		if( SyStringLength(&pInfo->sValue) > 0 ){` |
|      - |  355 | `			/* php warns for EVERY non-iterable, null included (PH7 exempted null). */` |
|      7 |  356 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|      - |  357 | `				"foreach() argument must be of type array\|object, %s given",` |
|      2 |  358 | `				VmArithTypeName(pTos));` |
|      2 |  359 | `		}` |
|      5 |  360 | `		pc = pInstr->iP2 - 1;` |
|      3 |  361 | `	}else{` |
|      - |  362 | `		ph7_foreach_step *pStep;` |
|  24101 |  363 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|  24101 |  364 | `		if( pStep == 0 ){` |
|    ! 0 |  365 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      - |  366 | `			/* Jump out of the loop */` |
|    ! 0 |  367 | `			pc = pInstr->iP2 - 1;` |
|    ! 0 |  368 | `		}else{` |
|      - |  369 | `			/* Zero the structure */` |
|  24101 |  370 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|      - |  371 | `			/* Prepare the step */` |
|  24101 |  372 | `			pStep->iFlags = pInfo->iFlags;` |
|      - |  373 | `			/* Record the owning activation so OP_FOREACH_STEP can pick THIS` |
|      - |  374 | `			 * activation's step out of the per-statement stack — two suspended` |
|      - |  375 | `			 * generator/fiber instances (or a recursive call) paused in the same` |
|      - |  376 | `			 * textual foreach otherwise resume onto each other's cursor. */` |
|  24101 |  377 | `			pStep->pFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|  24101 |  378 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|      - |  379 | `				ph7_hashmap *pMap,*pIterMap;` |
|      - |  380 | `				/* COW: For by-reference foreach, eagerly separate the` |
|      - |  381 | `				 * source array so mutations don't affect other sharers. */` |
|  23849 |  382 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|     29 |  383 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|     29 |  384 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|     29 |  385 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|      - |  386 | `						/* Only adjust refcounts/separate if the backing` |
|      - |  387 | `						 * variable still points at the same hashmap as` |
|      - |  388 | `						 * the stack value. */` |
|     29 |  389 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|     29 |  390 | `							pCur->iRef--;` |
|      - |  391 | `							/* Use the returned map, not pBacking->x.pOther: PH7_HashmapDup` |
|      - |  392 | `							 * inside CowSeparate can reallocate (move) pVm->aMemObj and leave` |
|      - |  393 | `							 * pBacking dangling. The return value is the post-separation map. */` |
|     29 |  394 | `							pTos->x.pOther = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|     29 |  395 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|     13 |  396 | `						}` |
|     13 |  397 | `					}` |
|     13 |  398 | `				}` |
|  23849 |  399 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|  23849 |  400 | `				pIterMap = pMap;` |
|  23849 |  401 | `				if( pMap == pVm->pGlobal && (pStep->iFlags & PH7_4EACH_STEP_REF) == 0 ){` |
|      - |  402 | `					/* php 8.1: foreach ($GLOBALS as ...) by value iterates a` |
|      - |  403 | `					 * SNAPSHOT of the symbol table — globals created inside` |
|      - |  404 | `					 * the loop body must not be visited (the live map would` |
|      - |  405 | `					 * grow under the cursor). By-ref foreach keeps the live` |
|      - |  406 | `					 * map, like php. On OOM fall back to the live map. */` |
|      5 |  407 | `					ph7_hashmap *pSnap = PH7_NewHashmap(&(*pVm),0,0);` |
|      5 |  408 | `					if( pSnap && PH7_HashmapDupMaterialized(pMap,pSnap) == SXRET_OK ){` |
|      - |  409 | `						/* The step consumes the snapshot's initial reference */` |
|      5 |  410 | `						pIterMap = pSnap;` |
|      2 |  411 | `					}else if( pSnap ){` |
|    ! 0 |  412 | `						PH7_HashmapUnref(pSnap);` |
|    ! 0 |  413 | `					}` |
|      2 |  414 | `				}` |
|  23849 |  415 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|  23849 |  416 | `				pStep->xIter.pMap = pIterMap;` |
|  23849 |  417 | `				if( pIterMap == pMap ){` |
|  23845 |  418 | `					pMap->iRef++;` |
|  11920 |  419 | `				}` |
|      - |  420 | `				/* Private cursor + registry (php: nested foreach over one` |
|      - |  421 | `				 * array are independent; foreach never moves the internal` |
|      - |  422 | `				 * pointer — see PH7_HashmapRegisterForeachStep) */` |
|  23849 |  423 | `				PH7_HashmapRegisterForeachStep(pIterMap,pStep);` |
|  11927 |  424 | `			}else{` |
|    257 |  425 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|      - |  426 | `				ph7_class *pIteratorClass;` |
|      - |  427 | `				/* Check if the object implements Iterator */` |
|    257 |  428 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|    356 |  429 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|      - |  430 | `					/* Iterator-based iteration: call rewind() */` |
|      - |  431 | `					ph7_class_method *pRewind;` |
|    209 |  432 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|    209 |  433 | `					pStep->xIter.pThis = pThis;` |
|    209 |  434 | `					pThis->iRef++;` |
|    209 |  435 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|    209 |  436 | `					if( pRewind ){` |
|    209 |  437 | `						rc = PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|    209 |  438 | `						if( VmIterCallThrew(rc) ){` |
|      - |  439 | `							/* rewind() threw (a generator body or userland Iterator):` |
|      - |  440 | `							 * undo this step's retain, drop the step, and route the` |
|      - |  441 | `							 * exception instead of silently starting the loop. */` |
|      8 |  442 | `							pThis->iRef--;` |
|      8 |  443 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      8 |  444 | `							pStep = 0;` |
|      8 |  445 | `							PH7_DISPATCH_ITER_RC(rc,1)` |
|    ! 0 |  446 | `						}` |
|     99 |  447 | `					}` |
|    104 |  448 | `				}else{` |
|      - |  449 | `					/* Check if the object implements IteratorAggregate */` |
|      - |  450 | `					ph7_class *pIterAggClass;` |
|     52 |  451 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|      - |  452 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|     63 |  453 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|      - |  454 | `						/* Call getIterator() and use the returned Iterator object */` |
|      - |  455 | `						ph7_class_method *pGetIter;` |
|     24 |  456 | `						int iterAggOk = 0;` |
|     24 |  457 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|     24 |  458 | `						if( pGetIter ){` |
|      - |  459 | `							ph7_value sResult;` |
|     24 |  460 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|     24 |  461 | `							rc = PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|     24 |  462 | `							if( VmIterCallThrew(rc) ){` |
|      - |  463 | `								/* getIterator() threw: drop the step and route the` |
|      - |  464 | `								 * exception (don't pile the "must implement Iterator"` |
|      - |  465 | `								 * error on top of it). */` |
|    ! 0 |  466 | `								PH7_MemObjRelease(&sResult);` |
|    ! 0 |  467 | `								SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    ! 0 |  468 | `								pStep = 0;` |
|    ! 0 |  469 | `								PH7_DISPATCH_ITER_RC(rc,1)` |
|    ! 0 |  470 | `							}` |
|     24 |  471 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|     24 |  472 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|     24 |  473 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|      - |  474 | `									ph7_class_method *pRewind;` |
|     24 |  475 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|     24 |  476 | `									pStep->xIter.pThis = pIterObj;` |
|     24 |  477 | `									pIterObj->iRef++;` |
|      - |  478 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|     24 |  479 | `									pStep->pOwner = pThis;` |
|     24 |  480 | `									pThis->iRef++;` |
|     24 |  481 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|     24 |  482 | `									if( pRewind ){` |
|     24 |  483 | `										rc = PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|     24 |  484 | `										if( VmIterCallThrew(rc) ){` |
|      - |  485 | `											/* The aggregate's iterator rewind() threw: undo` |
|      - |  486 | `											 * both retains, drop the step, route the exception. */` |
|    ! 0 |  487 | `											pIterObj->iRef--;` |
|    ! 0 |  488 | `											pThis->iRef--;` |
|    ! 0 |  489 | `											PH7_MemObjRelease(&sResult);` |
|    ! 0 |  490 | `											SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    ! 0 |  491 | `											pStep = 0;` |
|    ! 0 |  492 | `											PH7_DISPATCH_ITER_RC(rc,1)` |
|    ! 0 |  493 | `										}` |
|     11 |  494 | `									}` |
|     24 |  495 | `									iterAggOk = 1;` |
|     11 |  496 | `								}` |
|     11 |  497 | `							}` |
|     24 |  498 | `							PH7_MemObjRelease(&sResult);` |
|     11 |  499 | `						}` |
|     24 |  500 | `						if( !iterAggOk ){` |
|      - |  501 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|    ! 0 |  502 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|      - |  503 | `								"Object returned by getIterator() must implement Iterator");` |
|    ! 0 |  504 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    ! 0 |  505 | `							pStep = 0; /* Signal: do not store this step */` |
|    ! 0 |  506 | `							pc = pInstr->iP2 - 1;` |
|    ! 0 |  507 | `						}` |
|     13 |  508 | `					}else{` |
|      - |  509 | `						/* Plain object iteration via hAttr */` |
|     30 |  510 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|     30 |  511 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|     30 |  512 | `						pStep->xIter.pThis = pThis;` |
|     30 |  513 | `						pThis->iRef++;` |
|      - |  514 | `					}` |
|      - |  515 | `				}` |
|      - |  516 | `			}` |
|      - |  517 | `		}` |
|  24095 |  518 | `		if( pStep ){` |
|  24095 |  519 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|    ! 0 |  520 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|    ! 0 |  521 | `				if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|    ! 0 |  522 | `					VmForeachHashmapStepRelease(&(*pVm),pInfo,pStep,FALSE/*never made it onto aStep*/);` |
|    ! 0 |  523 | `				}else{` |
|    ! 0 |  524 | `					SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      - |  525 | `				}` |
|      - |  526 | `				/* Jump out of the loop */` |
|    ! 0 |  527 | `				pc = pInstr->iP2 - 1;` |
|    ! 0 |  528 | `			}` |
|  12045 |  529 | `		}` |
|      - |  530 | `	}` |
|  24099 |  531 | `	VmPopOperand(&pTos,1);` |
|  24099 |  532 | `	VM_EXIT_BREAK;` |
|    ! 0 |  533 | `	VM_EXIT_BREAK;` |
|  12055 |  534 | `}` |
|      - |  535 |  |
