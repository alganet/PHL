# src/ph7/vm_ops_iter.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 337/389 lines (86.63%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h"` |
|       - |    7 | `/*` |
|       - |    8 | ` * Section:` |
|       - |    9 | ` *    Iteration opcode handlers extracted from vm.c's dispatch loop. Each` |
|       - |   10 | ` *    handler runs one opcode arm against the caller's VmExecState: the loop` |
|       - |   11 | ` *    syncs pTos/pc in, calls the handler, reloads them and routes the` |
|       - |   12 | ` *    returned VmOpRc onto its labels (same idiom as VmCallFinish).` |
|       - |   13 | ` * Status:` |
|       - |   14 | ` *    Stable.` |
|       - |   15 | ` */` |
|       - |   16 | `/* Bind the dispatch-routing exits to handler semantics (see vm_dispatch.h),` |
|       - |   17 | ` * and map the macros' sState references onto our state parameter. */` |
|       - |   18 | `#define VM_EXIT_BREAK      { pState->pTos = pTos; pState->pc = pc; return VM_OP_NEXT; }` |
|       - |   19 | `#define VM_EXIT_ABORT      { pState->pTos = pTos; pState->pc = pc; return VM_OP_ABORT; }` |
|       - |   20 | `#define VM_EXIT_EXCEPTION  { pState->pTos = pTos; pState->pc = pc; return VM_OP_EXCEPTION; }` |
|       - |   21 | `#include "vm_dispatch.h"` |
|       - |   22 | `#define sState (*pState)` |
|       - |   23 |  |
|       - |   24 | `/*` |
|       - |   25 | ` * OP_FOREACH_STEP: advance one foreach iteration (hashmap cursor, Iterator` |
|       - |   26 | ` * protocol, or object-attribute walk). Body moved verbatim from the` |
|       - |   27 | ` * OP_FOREACH_STEP arm of VmByteCodeExecBody; arm-terminal breaks became` |
|       - |   28 | ` * VM_EXIT_BREAK.` |
|       - |   29 | ` */` |
| 5164828 |   30 | `PH7_PRIVATE VmOpRc VmExecOpForeachStep(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |   31 | `{` |
| 5164833 |   32 | `	ph7_value *pTos = pState->pTos;` |
| 5164833 |   33 | `	ph7_value *pStack = pState->pStack;` |
| 5164833 |   34 | `	VmInstr *aInstr = pState->aInstr;` |
| 5164833 |   35 | `	sxi32 pc = pState->pc;` |
|       - |   36 | `	sxi32 rc;` |
| 5164833 |   37 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|       - |   38 | `	ph7_foreach_step **apStep,*pStep;` |
|       - |   39 | `	ph7_value *pValue;` |
|       - |   40 | `	VmFrame *pFrameLocal;` |
|       - |   41 | `	sxu32 nStep;` |
| 5164833 |   42 | `	pFrameLocal = pVm->pFrame;` |
| 5164833 |   43 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       - |   44 | `	/* Select THIS activation's step. aStep is per-STATEMENT and shared by every` |
|       - |   45 | `	 * activation, so peeking the last entry resumes onto a sibling's cursor when` |
|       - |   46 | `	 * two instances of one generator/fiber are suspended in the same textual` |
|       - |   47 | `	 * foreach. Scan from the top (most-recent push) for the step whose owning` |
|       - |   48 | `	 * frame matches the running activation; top-down makes the current push win` |
|       - |   49 | `	 * over any leaked older step that happens to share a recycled frame address. */` |
| 5164833 |   50 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
| 5164833 |   51 | `	nStep = SySetUsed(&pInfo->aStep);` |
| 5164833 |   52 | `	if( nStep < 1 ){` |
|       - |   53 | `		/* Defensive: OP_FOREACH_INIT always pushes this activation's step before` |
|       - |   54 | `		 * STEP runs (and jumps past the loop when the push fails), so an empty` |
|       - |   55 | `		 * set is unreachable — guard the apStep[-1] read anyway. Jump out. */` |
|       5 |   56 | `		pc = pInstr->iP2 - 1;` |
|       5 |   57 | `		VM_EXIT_BREAK;` |
|       - |   58 | `	}` |
| 5164829 |   59 | `	pStep = apStep[nStep - 1];` |
| 5164853 |   60 | `	while( nStep > 0 ){` |
| 5164853 |   61 | `		if( apStep[nStep - 1]->pFrame == pFrameLocal ){` |
| 5164829 |   62 | `			pStep = apStep[nStep - 1];` |
| 5164829 |   63 | `			break;` |
|       - |   64 | `		}` |
|      25 |   65 | `		nStep--;` |
|       1 |   66 | `	}` |
| 5164829 |   67 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|       - |   68 | `		ph7_hashmap_node *pNode;` |
|       - |   69 | `		/* Extract the current node via this loop's PRIVATE cursor (php:` |
|       - |   70 | `		 * nested foreach over the same array are independent iterations) */` |
| 5160074 |   71 | `		pNode = pStep->pCursor;` |
| 5160074 |   72 | `		if( pNode == 0 ){` |
|       - |   73 | `			/* No more entry to process */` |
|   46364 |   74 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|       - |   75 | `			/* php does NOT break the binding: the value variable stays a reference to the` |
|       - |   76 | `` 			 * LAST element after the loop — that is what makes a second `foreach ($a as $v)` `` |
|       - |   77 | ``			 * write through it (the famous gotcha), and what the `unset($v)` idiom exists to`` |
|       - |   78 | `			 * undo. Deleting the name here left $v undefined instead. */` |
|       - |   79 | `			/* Cleanup the mess left behind */` |
|   46364 |   80 | `			VmForeachHashmapStepRelease(&(*pVm),pInfo,pStep,TRUE);` |
|   23150 |   81 | `		}else{` |
|       - |   82 | `			/* Advance the private cursor */` |
| 5113715 |   83 | `			pStep->pCursor = pNode->pPrev; /* Reverse link */` |
|       - |   84 | `			/* Bind the VALUE before the KEY: on the first iteration this is where` |
|       - |   85 | `			 * both locals are created in the frame table, and php's symbol table` |
|       - |   86 | `			 * lists the value ahead of the key (get_defined_vars() order). Only the` |
|       - |   87 | `			 * creation ORDER matters here; the stored values are independent. */` |
| 5113715 |   88 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|       - |   89 | `				/* Pass by reference — a REGISTERED binding (PH7_VmBindVarSlot), so the element` |
|       - |   90 | `				 * counts the loop variable as a holder for as long as it is bound, exactly as` |
|       - |   91 | `				 * php's reference does. */` |
|     234 |   92 | `				PH7_VmBindVarSlot(&(*pVm),pFrameLocal,SyStringData(&pInfo->sValue),` |
|      77 |   93 | `					SyStringLength(&pInfo->sValue),pNode->nValIdx);` |
|      80 |   94 | `			}else{` |
|       - |   95 | `				/* Make a copy of the entry value */` |
| 5113561 |   96 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
| 5113561 |   97 | `				if( pValue ){` |
| 5113561 |   98 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
| 2556398 |   99 | `				}` |
|       - |  100 | `			}` |
| 5113715 |  101 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|   27699 |  102 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|   27699 |  103 | `				if( pKey ){` |
|   27699 |  104 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|   13585 |  105 | `				}` |
|   13585 |  106 | `			}` |
|       5 |  107 | `		}` |
| 2584380 |  108 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|       - |  109 | `		/* Iterator-based iteration.` |
|       - |  110 | `		 * Sequence: on first call just check valid/current/key.` |
|       - |  111 | `		 * On subsequent calls, advance with next() first, then check.` |
|       - |  112 | `		 */` |
|    4466 |  113 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       - |  114 | `		ph7_class_method *pMethod;` |
|       - |  115 | `		ph7_value sResult;` |
|    4466 |  116 | `		int isValid = 0;` |
|       - |  117 | `		/* Call next() to advance — but skip on the first iteration */` |
|    4466 |  118 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|    1329 |  119 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|     667 |  120 | `		}else{` |
|    3142 |  121 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|    3142 |  122 | `			if( pMethod ){` |
|    3142 |  123 | `				rc = PH7_VmCallIteratorMethod(&(*pVm),pThis,pMethod,0);` |
|    3142 |  124 | `				if( VmIterCallThrew(rc) ){` |
|       - |  125 | `					/* next() threw (generator body / userland Iterator): tear the` |
|       - |  126 | `					 * step down like exhaustion does, then route the exception —` |
|       - |  127 | `					 * the loop must not silently end with execution continuing. */` |
|      25 |  128 | `					VmForeachStepAbandon(pVm,pInfo,pStep,pThis);` |
|      25 |  129 | `					PH7_DISPATCH_ITER_RC(rc,0)` |
|     ! 0 |  130 | `				}` |
|    1559 |  131 | `			}` |
|       - |  132 | `		}` |
|       - |  133 | `		/* Call valid() */` |
|    4446 |  134 | `		PH7_MemObjInit(pVm,&sResult);` |
|    4446 |  135 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|    4446 |  136 | `		if( pMethod ){` |
|    4446 |  137 | `			rc = PH7_VmCallIteratorMethod(&(*pVm),pThis,pMethod,&sResult);` |
|    4446 |  138 | `			if( VmIterCallThrew(rc) ){` |
|       - |  139 | `				/* valid() threw: same teardown-and-route as next() above. */` |
|     ! 0 |  140 | `				PH7_MemObjRelease(&sResult);` |
|     ! 0 |  141 | `				VmForeachStepAbandon(pVm,pInfo,pStep,pThis);` |
|     ! 0 |  142 | `				PH7_DISPATCH_ITER_RC(rc,0)` |
|     ! 0 |  143 | `			}` |
|    4446 |  144 | `			PH7_MemObjToBool(&sResult);` |
|    4446 |  145 | `			isValid = (sResult.x.iVal != 0);` |
|    2221 |  146 | `		}` |
|    4446 |  147 | `		PH7_MemObjRelease(&sResult);` |
|    4446 |  148 | `		if( !isValid ){` |
|       - |  149 | `			/* Iterator exhausted */` |
|    1287 |  150 | `			pc = pInstr->iP2 - 1;` |
|       - |  151 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|    1287 |  152 | `			VmForeachStepAbandon(pVm,pInfo,pStep,pThis);` |
|     646 |  153 | `		}else{` |
|       - |  154 | `			/* Call current() to get value */` |
|    3164 |  155 | `			PH7_MemObjInit(pVm,&sResult);` |
|    3164 |  156 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|    3164 |  157 | `			if( pMethod ){` |
|    3164 |  158 | `				rc = PH7_VmCallIteratorMethod(&(*pVm),pThis,pMethod,&sResult);` |
|    3164 |  159 | `				if( VmIterCallThrew(rc) ){` |
|       - |  160 | `					/* current() threw: same teardown-and-route as next() above. */` |
|     ! 0 |  161 | `					PH7_MemObjRelease(&sResult);` |
|     ! 0 |  162 | `					VmForeachStepAbandon(pVm,pInfo,pStep,pThis);` |
|     ! 0 |  163 | `					PH7_DISPATCH_ITER_RC(rc,0)` |
|     ! 0 |  164 | `				}` |
|    1580 |  165 | `			}` |
|    3164 |  166 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|    3164 |  167 | `			if( pValue ){` |
|    3164 |  168 | `				PH7_MemObjStore(&sResult,pValue);` |
|    1580 |  169 | `			}` |
|    3164 |  170 | `			PH7_MemObjRelease(&sResult);` |
|       - |  171 | `			/* Call key() if needed */` |
|    3164 |  172 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|       - |  173 | `				ph7_value sKey;` |
|     807 |  174 | `				PH7_MemObjInit(pVm,&sKey);` |
|     807 |  175 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|     807 |  176 | `				if( pMethod ){` |
|     807 |  177 | `					rc = PH7_VmCallIteratorMethod(&(*pVm),pThis,pMethod,&sKey);` |
|     807 |  178 | `					if( VmIterCallThrew(rc) ){` |
|       - |  179 | `						/* key() threw: same teardown-and-route as next() above. */` |
|     ! 0 |  180 | `						PH7_MemObjRelease(&sKey);` |
|     ! 0 |  181 | `						VmForeachStepAbandon(pVm,pInfo,pStep,pThis);` |
|     ! 0 |  182 | `						PH7_DISPATCH_ITER_RC(rc,0)` |
|     ! 0 |  183 | `					}` |
|     401 |  184 | `				}` |
|     807 |  185 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|     807 |  186 | `				if( pValue ){` |
|     807 |  187 | `					PH7_MemObjStore(&sKey,pValue);` |
|     401 |  188 | `				}` |
|     807 |  189 | `				PH7_MemObjRelease(&sKey);` |
|     401 |  190 | `			}` |
|       - |  191 | `		}` |
|    2226 |  192 | `	}else{` |
|     298 |  193 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|     298 |  194 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|       - |  195 | `		SyHashEntry *pEntry;` |
|       - |  196 | `		/* Point to the next attribute (this loop's own cursor) */` |
|     486 |  197 | `		while((pEntry = PH7_ClassInstanceIterNext(&pStep->sAttrIter)) != 0 ){` |
|     380 |  198 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     380 |  199 | `			if( PH7_ATTR_UNPRESENTED(pVmAttr) ){` |
|       - |  200 | `				/* A static property belongs to the CLASS, never to an object: php` |
|       - |  201 | `				 * iterates only the instance's own properties. PHL's instance` |
|       - |  202 | `				 * attribute table carries an entry for every declared member` |
|       - |  203 | `				 * (statics share the class slot), so it has to filter here — the` |
|       - |  204 | `				 * same test var_dump/get_object_vars/json/serialize already make. */` |
|     154 |  205 | `				continue;` |
|       - |  206 | `			}` |
|     226 |  207 | `			if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|     117 |  208 | `			 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|       3 |  209 | `				continue; /* virtual set-only property: iteration skips it (php) */` |
|       - |  210 | `			}` |
|     228 |  211 | `			if( PH7_ClassAttrUninitializedForRead(pVmAttr) ){` |
|      24 |  212 | `				continue; /* typed, never written: not there yet (php) */` |
|       - |  213 | `			}` |
|       - |  214 | `			/* Check access permission */` |
|     206 |  215 | `			if( PH7_ClassInstanceAttrShadowed(&(*pVm),pThis,pEntry) ){` |
|       3 |  216 | `				continue; /* an earlier accessible slot already answers for this name */` |
|       - |  217 | `			}` |
|     204 |  218 | `			if( PH7_VmClassAttrAccess(&(*pVm),pThis->pClass,pVmAttr->pAttr,FALSE) ){` |
|     192 |  219 | `					break; /* Access is granted */` |
|       - |  220 | `			}` |
|       3 |  221 | `		}` |
|     298 |  222 | `		if( pEntry == 0 ){` |
|       - |  223 | `			/* Clean up the mess left behind */` |
|     110 |  224 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|       - |  225 | `			/* The binding survives the loop (see the hashmap step) */` |
|     110 |  226 | `			PH7_ClassInstanceIterClose(pThis,&pStep->sAttrIter);` |
|     110 |  227 | `			VmForeachStepUnlink(pInfo,pStep);` |
|     110 |  228 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     110 |  229 | `			PH7_ClassInstanceUnref(pThis);` |
|      57 |  230 | `		}else{` |
|     192 |  231 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|       - |  232 | `			ph7_value *pAttrValue;` |
|     192 |  233 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|       - |  234 | `				/* Fill with the current attribute name. A MANGLED name — only the` |
|       - |  235 | `				 * __PHP_Incomplete_Class carrier stores those — yields its plain` |
|       - |  236 | `				 * part: php's iterator unmangles the key it hands out. */` |
|     188 |  237 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|     188 |  238 | `				if( pKey ){` |
|       - |  239 | `					SyString sUnmCls, sUnmName;` |
|     188 |  240 | `					SyStringInitFromBuf(&sUnmName,pAttrName->zString,pAttrName->nByte);` |
|     188 |  241 | `					if( pAttrName->nByte > 0 && pAttrName->zString[0] == 0 ){` |
|       5 |  242 | `						PH7_UnmangleAttrName(pAttrName->zString,pAttrName->nByte,&sUnmCls,&sUnmName);` |
|       2 |  243 | `					}` |
|     188 |  244 | `					SyBlobReset(&pKey->sBlob);` |
|     188 |  245 | `					SyBlobAppend(&pKey->sBlob,sUnmName.zString,sUnmName.nByte);` |
|     188 |  246 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|      92 |  247 | `				}` |
|      92 |  248 | `			}` |
|     188 |  249 | `			if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET))` |
|     100 |  250 | `			 && (pStep->iFlags & PH7_4EACH_STEP_REF)` |
|      11 |  251 | `			 && !VmHookGuardHeld(pVm,(void *)pThis,&pVmAttr->pAttr->sName) ){` |
|       - |  252 | `				/* php: a hooked property (virtual or backed) cannot be iterated` |
|       - |  253 | `				 * by reference — catchable Error. Tear the step down like the` |
|       - |  254 | `				 * exhausted-iteration path (break the by-ref binding, unlink,` |
|       - |  255 | `				 * free, drop the instance retain) so nothing leaks and a` |
|       - |  256 | `				 * re-entered foreach starts fresh; the fetch-point router lands` |
|       - |  257 | `				 * the parked throw right after this op. Inside the property's` |
|       - |  258 | `				 * own hook body the guard keeps raw semantics (no Error). */` |
|       - |  259 | `				SyBlob sErrMsg;` |
|       3 |  260 | `				SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       3 |  261 | `				SyBlobFormat(&sErrMsg,"Cannot create reference to property %z::$%z",` |
|       2 |  262 | `					&pThis->pClass->sDisp,&pVmAttr->pAttr->sName);` |
|       3 |  263 | `				VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       3 |  264 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|       - |  265 | `				/* ...and with the binding gone, the frame's memo of it (see VmFrame). */` |
|       3 |  266 | `				VmVarMemoFlush(pFrameLocal);` |
|       3 |  267 | `				PH7_ClassInstanceIterClose(pThis,&pStep->sAttrIter);` |
|       3 |  268 | `				VmForeachStepUnlink(pInfo,pStep);` |
|       3 |  269 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       3 |  270 | `				PH7_ClassInstanceUnref(pThis);` |
|       3 |  271 | `				VM_EXIT_BREAK;` |
|       - |  272 | `			}` |
|     186 |  273 | `			if( (pStep->iFlags & PH7_4EACH_STEP_REF) == 0` |
|     184 |  274 | `			 && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET) != 0 ){` |
|       - |  275 | `				/* PHP 8.4 property hooks: object iteration reads through the` |
|       - |  276 | `				 * get hook (virtual properties included; the flag gate keeps` |
|       - |  277 | `				 * hook-free classes on the raw zero-copy path below). The` |
|       - |  278 | `				 * step carries its own registered cursor, so a hook that` |
|       - |  279 | `				 * re-enters an hAttr walk on this instance (get_object_vars,` |
|       - |  280 | `				 * json_encode of $this) cannot truncate THIS iteration, and one` |
|       - |  281 | `				 * that unset()s the property the cursor is parked on has that` |
|       - |  282 | `				 * cursor advanced under it. */` |
|       - |  283 | `				ph7_value sHookVal;` |
|       - |  284 | `				sxi32 rcHk;` |
|      11 |  285 | `				PH7_MemObjInit(pVm,&sHookVal);` |
|      11 |  286 | `				rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);` |
|      11 |  287 | `				if( rcHk != SXERR_NOTFOUND ){` |
|      11 |  288 | `					if( rcHk == SXRET_OK ){` |
|      11 |  289 | `						pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|      11 |  290 | `						if( pValue ){` |
|      11 |  291 | `							PH7_MemObjStore(&sHookVal,pValue);` |
|       5 |  292 | `						}` |
|       5 |  293 | `					}` |
|       - |  294 | `					/* a throw parked on the boundary rail: the fetch-point` |
|       - |  295 | `					 * router lands it right after this op */` |
|      11 |  296 | `					PH7_MemObjRelease(&sHookVal);` |
|      11 |  297 | `					VM_EXIT_BREAK;` |
|       - |  298 | `				}` |
|     ! 0 |  299 | `				PH7_MemObjRelease(&sHookVal);` |
|     ! 0 |  300 | `			}` |
|       - |  301 | `			/* Extract attribute value */` |
|     180 |  302 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|     180 |  303 | `			if( pAttrValue ){` |
|     180 |  304 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|       - |  305 | `					/* Pass by reference (registered — see the hashmap step) */` |
|      19 |  306 | `					PH7_VmBindVarSlot(&(*pVm),pFrameLocal,SyStringData(&pInfo->sValue),` |
|       6 |  307 | `						SyStringLength(&pInfo->sValue),pVmAttr->nIdx);` |
|       7 |  308 | `				}else{` |
|       - |  309 | `					/* Make a copy of the attribute value */` |
|     168 |  310 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|     168 |  311 | `					if( pValue ){` |
|     168 |  312 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|      82 |  313 | `					}` |
|       - |  314 | `				}` |
|      88 |  315 | `			}` |
|       - |  316 | `		}` |
|       - |  317 | `	}` |
| 5164797 |  318 | `	VM_EXIT_BREAK;` |
| 2582005 |  319 | `}` |
|       - |  320 |  |
|       - |  321 | `/*` |
|       - |  322 | ` * OP_FOREACH_INIT: body moved verbatim from the OP_FOREACH_INIT arm of` |
|       - |  323 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  324 | ` */` |
|   48289 |  325 | `PH7_PRIVATE VmOpRc VmExecOpForeachInit(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  326 | `{` |
|   48294 |  327 | `	ph7_value *pTos = pState->pTos;` |
|   48294 |  328 | `	ph7_value *pStack = pState->pStack;` |
|   48294 |  329 | `	VmInstr *aInstr = pState->aInstr;` |
|   48294 |  330 | `	sxi32 pc = pState->pc;` |
|       - |  331 | `	sxi32 rc;` |
|   48294 |  332 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|       - |  333 | `	void *pName;` |
|       - |  334 | `#ifdef UNTRUST` |
|       - |  335 | `	if( pTos < pStack ){` |
|       - |  336 | `		VM_EXIT_ABORT;` |
|       - |  337 | `	}` |
|       - |  338 | `#endif` |
|   48294 |  339 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|       - |  340 | `		/* Take the variable name from the top of the stack */` |
|     ! 0 |  341 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  342 | `			/* Force a string cast */` |
|     ! 0 |  343 | `			PH7_MemObjToString(pTos);` |
|     ! 0 |  344 | `		}` |
|       - |  345 | `		/* Duplicate name */` |
|     ! 0 |  346 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|     ! 0 |  347 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     ! 0 |  348 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|     ! 0 |  349 | `		}` |
|     ! 0 |  350 | `		VmPopOperand(&pTos,1);` |
|     ! 0 |  351 | `	}` |
|   48294 |  352 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|     ! 0 |  353 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  354 | `			/* Force a string cast */` |
|     ! 0 |  355 | `			PH7_MemObjToString(pTos);` |
|     ! 0 |  356 | `		}` |
|       - |  357 | `		/* Duplicate name */` |
|     ! 0 |  358 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|     ! 0 |  359 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     ! 0 |  360 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|     ! 0 |  361 | `		}` |
|     ! 0 |  362 | `		VmPopOperand(&pTos,1);` |
|     ! 0 |  363 | `	}` |
|   48294 |  364 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_REF) && (pTos->iFlags & MEMOBJ_AUX_STROFFSET) ){` |
|       - |  365 | ``		/* `foreach ($s[0] as &$v)`: the subject is a string OFFSET and the loop wants`` |
|       - |  366 | `		 * to ALIAS its elements. php screens that before it asks whether the subject is` |
|       - |  367 | `		 * iterable at all, and a string offset is never a reference — same Error the` |
|       - |  368 | ``		 * `=&` and by-ref-argument paths raise, not the not-iterable warning PHL`` |
|       - |  369 | `		 * answered (whose slot index is the BASE STRING's, so binding would have` |
|       - |  370 | `		 * aliased the whole string). */` |
|       3 |  371 | `		sxi32 rcSo = VmThrowFromVm(&(*pVm),"Error","Cannot create references to/from string offsets",` |
|       - |  372 | `			sizeof("Cannot create references to/from string offsets")-1);` |
|       3 |  373 | `		if( rcSo == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       3 |  374 | `		PH7_THROW_ROUTE_MIDEXPR(rcSo)` |
|       - |  375 | `	}` |
|   48292 |  376 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_REF) && SyStringLength(&pInfo->sValue) > 0 ){` |
|       - |  377 | ``		/* `foreach ($o->p as &$v)` on a property no write may reach: the loop hands`` |
|       - |  378 | `		 * out an ALIAS of every element, so php screens the property where it` |
|       - |  379 | `		 * screens a store -- and it screens it BEFORE deciding the subject is` |
|       - |  380 | `		 * iterable at all, which is why an int or a string property is this Error` |
|       - |  381 | `		 * there and a "must be of type array\|object" warning here. PHL walked an` |
|       - |  382 | `		 * array one and let the body rewrite it. */` |
|      83 |  383 | `		sxi32 rcInd = PH7_VmCheckIndirectModify(&(*pVm),pTos->nIdx);` |
|      83 |  384 | `		if( rcInd != SXRET_OK ){` |
|       7 |  385 | `			if( rcInd == PH7_ABORT ){ VM_EXIT_ABORT; }` |
|       7 |  386 | `			PH7_THROW_ROUTE_MIDEXPR(rcInd)` |
|       - |  387 | `		}` |
|      37 |  388 | `	}` |
|       - |  389 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|   48286 |  390 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|       - |  391 | `		/* Jump out of the loop */` |
|       9 |  392 | `		if( SyStringLength(&pInfo->sValue) > 0 ){` |
|       - |  393 | `			/* php warns for EVERY non-iterable, null included (PH7 exempted null). */` |
|      13 |  394 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - |  395 | `				"foreach() argument must be of type array\|object, %s given",` |
|       4 |  396 | `				VmArithValueName(pTos));` |
|       4 |  397 | `		}` |
|       9 |  398 | `		pc = pInstr->iP2 - 1;` |
|       5 |  399 | `	}else{` |
|       - |  400 | `		ph7_foreach_step *pStep;` |
|   48278 |  401 | `		VmFrame *pInitFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|       - |  402 | `		/* Reclaim this activation's LEFTOVER step for this same foreach statement.` |
|       - |  403 | `		 * A loop left through break/return/goto/an exception never reaches the` |
|       - |  404 | `		 * "no more entries" arm that frees its step, so the step, its retain of` |
|       - |  405 | `		 * the subject and — for an object walk — its registered cursor all` |
|       - |  406 | `		 * survived until the VM died. The running frame cannot be inside that` |
|       - |  407 | `		 * loop's body while it is executing INIT, so the step is stale by` |
|       - |  408 | `		 * construction. */` |
|       - |  409 | `		{` |
|   48278 |  410 | `			ph7_foreach_step **apOld = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   48278 |  411 | `			sxu32 nOld = SySetUsed(&pInfo->aStep);` |
|   75661 |  412 | `			while( nOld > 0 ){` |
|    3217 |  413 | `				ph7_foreach_step *pOld = apOld[--nOld];` |
|    3217 |  414 | `				if( pOld->pFrame == pInitFrame ){` |
|      20 |  415 | `					VmForeachStepRelease(&(*pVm),pInfo,pOld);` |
|       - |  416 | `					/* The set shifted under us: restart the scan. */` |
|      20 |  417 | `					apOld = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|      20 |  418 | `					nOld = SySetUsed(&pInfo->aStep);` |
|       8 |  419 | `				}` |
|       5 |  420 | `			}` |
|       - |  421 | `		}` |
|   48278 |  422 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|   48278 |  423 | `		if( pStep == 0 ){` |
|     ! 0 |  424 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|       - |  425 | `			/* Jump out of the loop */` |
|     ! 0 |  426 | `			pc = pInstr->iP2 - 1;` |
|     ! 0 |  427 | `		}else{` |
|       - |  428 | `			/* Zero the structure */` |
|   48278 |  429 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|       - |  430 | `			/* Prepare the step */` |
|   48278 |  431 | `			pStep->iFlags = pInfo->iFlags;` |
|       - |  432 | `			/* Record the owning activation so OP_FOREACH_STEP can pick THIS` |
|       - |  433 | `			 * activation's step out of the per-statement stack — two suspended` |
|       - |  434 | `			 * generator/fiber instances (or a recursive call) paused in the same` |
|       - |  435 | `			 * textual foreach otherwise resume onto each other's cursor. */` |
|   48278 |  436 | `			pStep->pFrame = pInitFrame;` |
|   48278 |  437 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       - |  438 | `				ph7_hashmap *pMap,*pIterMap;` |
|       - |  439 | `				/* COW: For by-reference foreach, eagerly separate the` |
|       - |  440 | `				 * source array so mutations don't affect other sharers. */` |
|   46778 |  441 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|      61 |  442 | `					ph7_value *pBacking = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pTos->nIdx);` |
|      61 |  443 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|      61 |  444 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|       - |  445 | `						/* Only adjust refcounts/separate if the backing` |
|       - |  446 | `						 * variable still points at the same hashmap as` |
|       - |  447 | `						 * the stack value. */` |
|      61 |  448 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|      61 |  449 | `							pCur->iRef--;` |
|       - |  450 | `							/* Use the returned map, not pBacking->x.pOther: CowSeparate answers` |
|       - |  451 | `							 * a DIFFERENT hashmap, and reading it back out of pBacking assumes` |
|       - |  452 | `							 * the separation wrote there. (It also used to move the pool under` |
|       - |  453 | `							 * pBacking, which P1's fixed segments retired.) */` |
|      61 |  454 | `							pTos->x.pOther = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|      61 |  455 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|      29 |  456 | `						}` |
|      29 |  457 | `					}` |
|      29 |  458 | `				}` |
|   46778 |  459 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   46778 |  460 | `				pIterMap = pMap;` |
|   46778 |  461 | `				if( pMap == pVm->pGlobal && (pStep->iFlags & PH7_4EACH_STEP_REF) == 0 ){` |
|       - |  462 | `					/* php 8.1: foreach ($GLOBALS as ...) by value iterates a` |
|       - |  463 | `					 * SNAPSHOT of the symbol table — globals created inside` |
|       - |  464 | `					 * the loop body must not be visited (the live map would` |
|       - |  465 | `					 * grow under the cursor). By-ref foreach keeps the live` |
|       - |  466 | `					 * map, like php. On OOM fall back to the live map. */` |
|       5 |  467 | `					ph7_hashmap *pSnap = PH7_NewHashmap(&(*pVm),0,0);` |
|       5 |  468 | `					if( pSnap && PH7_HashmapDupMaterialized(pMap,pSnap) == SXRET_OK ){` |
|       - |  469 | `						/* The step consumes the snapshot's initial reference */` |
|       5 |  470 | `						pIterMap = pSnap;` |
|       2 |  471 | `					}else if( pSnap ){` |
|     ! 0 |  472 | `						PH7_HashmapUnref(pSnap);` |
|     ! 0 |  473 | `					}` |
|       2 |  474 | `				}` |
|   46778 |  475 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|   46778 |  476 | `				pStep->xIter.pMap = pIterMap;` |
|   46778 |  477 | `				if( pIterMap == pMap ){` |
|   46774 |  478 | `					pMap->iRef++;` |
|   23350 |  479 | `				}` |
|       - |  480 | `				/* Private cursor + registry (php: nested foreach over one` |
|       - |  481 | `				 * array are independent; foreach never moves the internal` |
|       - |  482 | `				 * pointer — see PH7_HashmapRegisterForeachStep) */` |
|   46778 |  483 | `				PH7_HashmapRegisterForeachStep(pIterMap,pStep);` |
|   23357 |  484 | `			}else{` |
|    1505 |  485 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|       - |  486 | `				ph7_class *pIteratorClass;` |
|       - |  487 | `				/* Check if the object implements Iterator */` |
|    1505 |  488 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|    1505 |  489 | `				if( PH7_VmGeneratorIsClosed(&(*pVm),pThis) ){` |
|       - |  490 | `					/* php refuses to START a foreach over a generator that has already` |
|       - |  491 | `					 * run to its end, and says so BEFORE the rewind that would report` |
|       - |  492 | `					 * the coarser "already run". PHL walked an EMPTY loop instead, so a` |
|       - |  493 | `					 * second foreach over the same generator silently did nothing. */` |
|       3 |  494 | `					SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       3 |  495 | `					pStep = 0;` |
|       3 |  496 | `					rc = VmThrowFromVm(&(*pVm),"Exception",` |
|       - |  497 | `						"Cannot traverse an already closed generator",` |
|       - |  498 | `						(sxu32)sizeof("Cannot traverse an already closed generator")-1);` |
|      34 |  499 | `					PH7_DISPATCH_ITER_RC(rc,1)` |
|    1778 |  500 | `				}else if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|       - |  501 | `					/* Iterator-based iteration: call rewind() */` |
|       - |  502 | `					ph7_class_method *pRewind;` |
|     591 |  503 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|     591 |  504 | `					pStep->xIter.pThis = pThis;` |
|     591 |  505 | `					pThis->iRef++;` |
|     591 |  506 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|     591 |  507 | `					if( pRewind ){` |
|     591 |  508 | `						rc = PH7_VmCallIteratorMethod(&(*pVm),pThis,pRewind,0);` |
|     591 |  509 | `						if( VmIterCallThrew(rc) ){` |
|       - |  510 | `							/* rewind() threw (a generator body or userland Iterator):` |
|       - |  511 | `							 * undo this step's retain, drop the step, and route the` |
|       - |  512 | `							 * exception instead of silently starting the loop. */` |
|      42 |  513 | `							pThis->iRef--;` |
|      42 |  514 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      42 |  515 | `							pStep = 0;` |
|      48 |  516 | `							PH7_DISPATCH_ITER_RC(rc,1)` |
|     ! 0 |  517 | `						}` |
|     274 |  518 | `					}` |
|     279 |  519 | `				}else{` |
|       - |  520 | `					/* Check if the object implements IteratorAggregate */` |
|       - |  521 | `					ph7_class *pIterAggClass;` |
|     917 |  522 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|       - |  523 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|    1306 |  524 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|       - |  525 | `						/* php resolves a CHAIN, not one hop: whatever getIterator()` |
|       - |  526 | `						 * hands back is asked the same question, so an` |
|       - |  527 | `						 * IteratorAggregate may return another one and only the` |
|       - |  528 | `						 * Iterator at the end drives the loop. PHL stopped at the` |
|       - |  529 | `						 * first hop and refused everything else, which is what` |
|       - |  530 | ``						 * `foreach` over any aggregate-of-an-aggregate hit -- twig's`` |
|       - |  531 | ``						 * `in` filter over one is two rows of its suite. The two`` |
|       - |  532 | `						 * ends php still refuses are a receiver that returns ITSELF` |
|       - |  533 | `						 * and a return that is not Traversable at all; a cycle` |
|       - |  534 | `						 * between two aggregates is php's stack overflow, and a` |
|       - |  535 | `						 * bounded walk here (nothing real nests) so the engine` |
|       - |  536 | `						 * answers instead of spinning. */` |
|     807 |  537 | `						ph7_class_instance *pAgg = pThis;  /* whose getIterator() to ask */` |
|     807 |  538 | `						ph7_class_instance *pAggHold = 0;  /* an intermediate this walk owns */` |
|     807 |  539 | `						ph7_class_instance *pIterObj = 0;` |
|       - |  540 | `						ph7_class_method *pGetIter;` |
|     807 |  541 | `						int iterAggOk = 0, nHop = 0;` |
|     417 |  542 | `						for(;;){` |
|       - |  543 | `							ph7_value sResult;` |
|     823 |  544 | `							pGetIter = PH7_ClassExtractMethod(pAgg->pClass,"getIterator",sizeof("getIterator")-1);` |
|     823 |  545 | `							if( pGetIter == 0 ){` |
|     ! 0 |  546 | `								break;` |
|       - |  547 | `							}` |
|     823 |  548 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|     823 |  549 | `							rc = PH7_VmCallClassMethod(&(*pVm),pAgg,pGetIter,&sResult,0,0);` |
|     823 |  550 | `							if( VmIterCallThrew(rc) ){` |
|       - |  551 | `								/* getIterator() threw: drop the step and route the` |
|       - |  552 | `								 * exception (don't pile the "must be traversable"` |
|       - |  553 | `								 * error on top of it). */` |
|      15 |  554 | `								PH7_MemObjRelease(&sResult);` |
|      15 |  555 | `								if( pAggHold ){ PH7_ClassInstanceUnref(pAggHold); }` |
|      15 |  556 | `								SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      15 |  557 | `								pStep = 0;` |
|      15 |  558 | `								PH7_DISPATCH_ITER_RC(rc,1)` |
|     ! 0 |  559 | `							}` |
|     809 |  560 | `							pIterObj = ((sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther)` |
|    1206 |  561 | `								? (ph7_class_instance *)sResult.x.pOther : 0;` |
|     804 |  562 | `							if( pIterObj && pIteratorClass` |
|     809 |  563 | `							 && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|     791 |  564 | `								pIterObj->iRef++; /* survive the release below */` |
|     791 |  565 | `								PH7_MemObjRelease(&sResult);` |
|     791 |  566 | `								iterAggOk = 1;` |
|     791 |  567 | `								break;` |
|       - |  568 | `							}` |
|      18 |  569 | `							if( pIterObj == 0 \|\| pIterObj == pAgg` |
|      17 |  570 | `							 \|\| !PH7_VmInstanceOf(pIterObj->pClass,pIterAggClass)` |
|      17 |  571 | `							 \|\| ++nHop > 256 ){` |
|       3 |  572 | `								PH7_MemObjRelease(&sResult);` |
|       3 |  573 | `								break;` |
|       - |  574 | `							}` |
|       - |  575 | `							/* Another aggregate: own it across the next call, exactly` |
|       - |  576 | `							 * as php's own retval does, and drop the one before it. */` |
|      17 |  577 | `							pIterObj->iRef++;` |
|      17 |  578 | `							if( pAggHold ){ PH7_ClassInstanceUnref(pAggHold); }` |
|      17 |  579 | `							pAggHold = pIterObj;` |
|      17 |  580 | `							pAgg = pIterObj;` |
|      17 |  581 | `							PH7_MemObjRelease(&sResult);` |
|       1 |  582 | `						}` |
|     793 |  583 | `						if( iterAggOk ){` |
|       - |  584 | `							ph7_class_method *pRewind;` |
|     791 |  585 | `							pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|     791 |  586 | `							pStep->xIter.pThis = pIterObj; /* ref taken in the walk */` |
|       - |  587 | `							/* Retain the aggregate so it lives for the duration of the foreach */` |
|     791 |  588 | `							pStep->pOwner = pThis;` |
|     791 |  589 | `							pThis->iRef++;` |
|     791 |  590 | `							pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|     791 |  591 | `							if( pRewind ){` |
|     791 |  592 | `								rc = PH7_VmCallIteratorMethod(&(*pVm),pIterObj,pRewind,0);` |
|     791 |  593 | `								if( VmIterCallThrew(rc) ){` |
|       - |  594 | `									/* The aggregate's iterator rewind() threw: undo` |
|       - |  595 | `									 * both retains, drop the step, route the exception. */` |
|      11 |  596 | `									pIterObj->iRef--;` |
|      11 |  597 | `									pThis->iRef--;` |
|      11 |  598 | `									if( pAggHold ){ PH7_ClassInstanceUnref(pAggHold); }` |
|      11 |  599 | `									SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      11 |  600 | `									pStep = 0;` |
|      11 |  601 | `									PH7_DISPATCH_ITER_RC(rc,1)` |
|     ! 0 |  602 | `								}` |
|     388 |  603 | `							}` |
|     781 |  604 | `							if( pAggHold ){ PH7_ClassInstanceUnref(pAggHold); }` |
|     393 |  605 | `						}else{` |
|       - |  606 | `							/* php's own wording, and php's own THROW: a catchable` |
|       - |  607 | `							 * Exception naming the receiver that answered wrong, where` |
|       - |  608 | `							 * PHL raised an uncatchable E_ERROR diagnostic. */` |
|       - |  609 | `							char zMsg[256];` |
|       - |  610 | `							int nMsg;` |
|       3 |  611 | `							ph7_class *pBad = pAgg->pClass;` |
|       5 |  612 | `							nMsg = (int)SyBufferFormat(zMsg,sizeof(zMsg),` |
|       - |  613 | `								"Objects returned by %.*s::getIterator() must be traversable or implement interface Iterator",` |
|       2 |  614 | `								(int)SyStringLength(&pBad->sDisp),SyStringData(&pBad->sDisp));` |
|       3 |  615 | `							if( pAggHold ){ PH7_ClassInstanceUnref(pAggHold); }` |
|       3 |  616 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       3 |  617 | `							pStep = 0; /* Signal: do not store this step */` |
|       3 |  618 | `							rc = VmThrowFromVm(&(*pVm),"Exception",zMsg,(sxu32)nMsg);` |
|       3 |  619 | `							PH7_DISPATCH_ITER_RC(rc,1)` |
|       - |  620 | `						}` |
|     394 |  621 | `					}else{` |
|       - |  622 | `						/* Plain object iteration via hAttr. A PRIVATE cursor,` |
|       - |  623 | `						 * registered on the instance -- the table's embedded one` |
|       - |  624 | `						 * is shared, so a nested loop over the same object rewound` |
|       - |  625 | `						 * this one (an infinite loop) and an unset() in the body` |
|       - |  626 | `						 * freed the entry it was parked on. */` |
|     114 |  627 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|     114 |  628 | `						pStep->xIter.pThis = pThis;` |
|     114 |  629 | `						pThis->iRef++;` |
|     114 |  630 | `						PH7_ClassInstanceIterOpen(pThis,&pStep->sAttrIter);` |
|       - |  631 | `					}` |
|       - |  632 | `				}` |
|       - |  633 | `			}` |
|       - |  634 | `		}` |
|   48216 |  635 | `		if( pStep ){` |
|   48212 |  636 | `			if( SXRET_OK == SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|       - |  637 | ``				/* Hand the step to its owning activation. Whatever a `break`, a`` |
|       - |  638 | ``				 * `return`, a `goto` or an exception leaves behind is released when`` |
|       - |  639 | `				 * that frame dies, so aStep only ever holds LIVE walks -- the scan` |
|       - |  640 | `				 * above stays short instead of growing for the life of the VM. Linked` |
|       - |  641 | `				 * only once the step is really on aStep, so the failure arm below has` |
|       - |  642 | `				 * nothing to unlink. */` |
|   48212 |  643 | `				pStep->pInfo = pInfo;` |
|   48212 |  644 | `				pStep->pNextFrameStep = pInitFrame->pForeachSteps;` |
|   48212 |  645 | `				pInitFrame->pForeachSteps = pStep;` |
|   24074 |  646 | `			}else{` |
|     ! 0 |  647 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|     ! 0 |  648 | `				if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|     ! 0 |  649 | `					VmForeachHashmapStepRelease(&(*pVm),pInfo,pStep,FALSE/*never made it onto aStep*/);` |
|     ! 0 |  650 | `				}else{` |
|     ! 0 |  651 | `					if( pStep->iFlags & PH7_4EACH_STEP_OBJECT ){` |
|       - |  652 | `						/* Unhook the cursor before the pool slot is recycled: a` |
|       - |  653 | `						 * registered walker left behind is what the next unset()` |
|       - |  654 | `						 * on this object would step through. */` |
|     ! 0 |  655 | `						PH7_ClassInstanceIterClose(pStep->xIter.pThis,&pStep->sAttrIter);` |
|     ! 0 |  656 | `					}` |
|     ! 0 |  657 | `					SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       - |  658 | `				}` |
|       - |  659 | `				/* Jump out of the loop */` |
|     ! 0 |  660 | `				pc = pInstr->iP2 - 1;` |
|       - |  661 | `			}` |
|   24069 |  662 | `		}` |
|       - |  663 | `	}` |
|   48224 |  664 | `	VmPopOperand(&pTos,1);` |
|   48224 |  665 | `	VM_EXIT_BREAK;` |
|     ! 0 |  666 | `	VM_EXIT_BREAK;` |
|   24115 |  667 | `}` |
|       - |  668 |  |
