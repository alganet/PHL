# src/ph7/vm_ops_iter.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 266/332 lines (80.12%)

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
| 333576 |   30 | `PH7_PRIVATE VmOpRc VmExecOpForeachStep(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |   31 | `{` |
| 333581 |   32 | `	ph7_value *pTos = pState->pTos;` |
| 333581 |   33 | `	ph7_value *pStack = pState->pStack;` |
| 333581 |   34 | `	VmInstr *aInstr = pState->aInstr;` |
| 333581 |   35 | `	sxi32 pc = pState->pc;` |
|      - |   36 | `	sxi32 rc;` |
| 333581 |   37 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|      - |   38 | `	ph7_foreach_step **apStep,*pStep;` |
|      - |   39 | `	ph7_value *pValue;` |
|      - |   40 | `	VmFrame *pFrameLocal;` |
|      - |   41 | `	sxu32 nStep;` |
| 333581 |   42 | `	pFrameLocal = pVm->pFrame;` |
| 333581 |   43 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      - |   44 | `	/* Select THIS activation's step. aStep is per-STATEMENT and shared by every` |
|      - |   45 | `	 * activation, so peeking the last entry resumes onto a sibling's cursor when` |
|      - |   46 | `	 * two instances of one generator/fiber are suspended in the same textual` |
|      - |   47 | `	 * foreach. Scan from the top (most-recent push) for the step whose owning` |
|      - |   48 | `	 * frame matches the running activation; top-down makes the current push win` |
|      - |   49 | `	 * over any leaked older step that happens to share a recycled frame address. */` |
| 333581 |   50 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
| 333581 |   51 | `	nStep = SySetUsed(&pInfo->aStep);` |
| 333581 |   52 | `	if( nStep < 1 ){` |
|      - |   53 | `		/* Defensive: OP_FOREACH_INIT always pushes this activation's step before` |
|      - |   54 | `		 * STEP runs (and jumps past the loop when the push fails), so an empty` |
|      - |   55 | `		 * set is unreachable — guard the apStep[-1] read anyway. Jump out. */` |
|    ! 0 |   56 | `		pc = pInstr->iP2 - 1;` |
|    ! 0 |   57 | `		VM_EXIT_BREAK;` |
|      - |   58 | `	}` |
| 333581 |   59 | `	pStep = apStep[nStep - 1];` |
| 333607 |   60 | `	while( nStep > 0 ){` |
| 333607 |   61 | `		if( apStep[nStep - 1]->pFrame == pFrameLocal ){` |
| 333581 |   62 | `			pStep = apStep[nStep - 1];` |
| 333581 |   63 | `			break;` |
|      - |   64 | `		}` |
|     27 |   65 | `		nStep--;` |
|      1 |   66 | `	}` |
| 333581 |   67 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|      - |   68 | `		ph7_hashmap_node *pNode;` |
|      - |   69 | `		/* Extract the current node via this loop's PRIVATE cursor (php:` |
|      - |   70 | `		 * nested foreach over the same array are independent iterations) */` |
| 331629 |   71 | `		pNode = pStep->pCursor;` |
| 331629 |   72 | `		if( pNode == 0 ){` |
|      - |   73 | `			/* No more entry to process */` |
|  27531 |   74 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|      - |   75 | `			/* php does NOT break the binding: the value variable stays a reference to the` |
|      - |   76 | `` 			 * LAST element after the loop — that is what makes a second `foreach ($a as $v)` `` |
|      - |   77 | ``			 * write through it (the famous gotcha), and what the `unset($v)` idiom exists to`` |
|      - |   78 | `			 * undo. Deleting the name here left $v undefined instead. */` |
|      - |   79 | `			/* Cleanup the mess left behind */` |
|  27531 |   80 | `			VmForeachHashmapStepRelease(&(*pVm),pInfo,pStep,TRUE);` |
|  13768 |   81 | `		}else{` |
|      - |   82 | `			/* Advance the private cursor */` |
| 304103 |   83 | `			pStep->pCursor = pNode->pPrev; /* Reverse link */` |
|      - |   84 | `			/* Bind the VALUE before the KEY: on the first iteration this is where` |
|      - |   85 | `			 * both locals are created in the frame table, and php's symbol table` |
|      - |   86 | `			 * lists the value ahead of the key (get_defined_vars() order). Only the` |
|      - |   87 | `			 * creation ORDER matters here; the stored values are independent. */` |
| 304103 |   88 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|      - |   89 | `				/* Pass by reference — a REGISTERED binding (PH7_VmBindVarSlot), so the element` |
|      - |   90 | `				 * counts the loop variable as a holder for as long as it is bound, exactly as` |
|      - |   91 | `				 * php's reference does. */` |
|    198 |   92 | `				PH7_VmBindVarSlot(&(*pVm),pFrameLocal,SyStringData(&pInfo->sValue),` |
|     65 |   93 | `					SyStringLength(&pInfo->sValue),pNode->nValIdx);` |
|     68 |   94 | `			}else{` |
|      - |   95 | `				/* Make a copy of the entry value */` |
| 303973 |   96 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
| 303973 |   97 | `				if( pValue ){` |
| 303973 |   98 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
| 151984 |   99 | `				}` |
|      - |  100 | `			}` |
| 304103 |  101 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|   6815 |  102 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|   6815 |  103 | `				if( pKey ){` |
|   6815 |  104 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|   3405 |  105 | `				}` |
|   3405 |  106 | `			}` |
|      5 |  107 | `		}` |
| 167769 |  108 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|      - |  109 | `		/* Iterator-based iteration.` |
|      - |  110 | `		 * Sequence: on first call just check valid/current/key.` |
|      - |  111 | `		 * On subsequent calls, advance with next() first, then check.` |
|      - |  112 | `		 */` |
|   1831 |  113 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|      - |  114 | `		ph7_class_method *pMethod;` |
|      - |  115 | `		ph7_value sResult;` |
|   1831 |  116 | `		int isValid = 0;` |
|      - |  117 | `		/* Call next() to advance — but skip on the first iteration */` |
|   1831 |  118 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|    385 |  119 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|    195 |  120 | `		}else{` |
|   1451 |  121 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|   1451 |  122 | `			if( pMethod ){` |
|   1451 |  123 | `				rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|   1451 |  124 | `				if( VmIterCallThrew(rc) ){` |
|      - |  125 | `					/* next() threw (generator body / userland Iterator): tear the` |
|      - |  126 | `					 * step down like exhaustion does, then route the exception —` |
|      - |  127 | `					 * the loop must not silently end with execution continuing. */` |
|     18 |  128 | `					VmForeachStepAbandon(pVm,pInfo,pStep,pThis);` |
|     18 |  129 | `					PH7_DISPATCH_ITER_RC(rc,0)` |
|    ! 0 |  130 | `				}` |
|    716 |  131 | `			}` |
|      - |  132 | `		}` |
|      - |  133 | `		/* Call valid() */` |
|   1817 |  134 | `		PH7_MemObjInit(pVm,&sResult);` |
|   1817 |  135 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|   1817 |  136 | `		if( pMethod ){` |
|   1817 |  137 | `			rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|   1817 |  138 | `			if( VmIterCallThrew(rc) ){` |
|      - |  139 | `				/* valid() threw: same teardown-and-route as next() above. */` |
|    ! 0 |  140 | `				PH7_MemObjRelease(&sResult);` |
|    ! 0 |  141 | `				VmForeachStepAbandon(pVm,pInfo,pStep,pThis);` |
|    ! 0 |  142 | `				PH7_DISPATCH_ITER_RC(rc,0)` |
|    ! 0 |  143 | `			}` |
|   1817 |  144 | `			PH7_MemObjToBool(&sResult);` |
|   1817 |  145 | `			isValid = (sResult.x.iVal != 0);` |
|    906 |  146 | `		}` |
|   1817 |  147 | `		PH7_MemObjRelease(&sResult);` |
|   1817 |  148 | `		if( !isValid ){` |
|      - |  149 | `			/* Iterator exhausted */` |
|    357 |  150 | `			pc = pInstr->iP2 - 1;` |
|      - |  151 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|    357 |  152 | `			VmForeachStepAbandon(pVm,pInfo,pStep,pThis);` |
|    181 |  153 | `		}else{` |
|      - |  154 | `			/* Call current() to get value */` |
|   1465 |  155 | `			PH7_MemObjInit(pVm,&sResult);` |
|   1465 |  156 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|   1465 |  157 | `			if( pMethod ){` |
|   1465 |  158 | `				rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|   1465 |  159 | `				if( VmIterCallThrew(rc) ){` |
|      - |  160 | `					/* current() threw: same teardown-and-route as next() above. */` |
|    ! 0 |  161 | `					PH7_MemObjRelease(&sResult);` |
|    ! 0 |  162 | `					VmForeachStepAbandon(pVm,pInfo,pStep,pThis);` |
|    ! 0 |  163 | `					PH7_DISPATCH_ITER_RC(rc,0)` |
|    ! 0 |  164 | `				}` |
|    730 |  165 | `			}` |
|   1465 |  166 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   1465 |  167 | `			if( pValue ){` |
|   1465 |  168 | `				PH7_MemObjStore(&sResult,pValue);` |
|    730 |  169 | `			}` |
|   1465 |  170 | `			PH7_MemObjRelease(&sResult);` |
|      - |  171 | `			/* Call key() if needed */` |
|   1465 |  172 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      - |  173 | `				ph7_value sKey;` |
|    444 |  174 | `				PH7_MemObjInit(pVm,&sKey);` |
|    444 |  175 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|    444 |  176 | `				if( pMethod ){` |
|    444 |  177 | `					rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|    444 |  178 | `					if( VmIterCallThrew(rc) ){` |
|      - |  179 | `						/* key() threw: same teardown-and-route as next() above. */` |
|    ! 0 |  180 | `						PH7_MemObjRelease(&sKey);` |
|    ! 0 |  181 | `						VmForeachStepAbandon(pVm,pInfo,pStep,pThis);` |
|    ! 0 |  182 | `						PH7_DISPATCH_ITER_RC(rc,0)` |
|    ! 0 |  183 | `					}` |
|    220 |  184 | `				}` |
|    444 |  185 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|    444 |  186 | `				if( pValue ){` |
|    444 |  187 | `					PH7_MemObjStore(&sKey,pValue);` |
|    220 |  188 | `				}` |
|    444 |  189 | `				PH7_MemObjRelease(&sKey);` |
|    220 |  190 | `			}` |
|      - |  191 | `		}` |
|    911 |  192 | `	}else{` |
|    131 |  193 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|    131 |  194 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|      - |  195 | `		SyHashEntry *pEntry;` |
|      - |  196 | `		/* Point to the next attribute */` |
|    207 |  197 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|    151 |  198 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    151 |  199 | `			if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_HIDDEN) ){` |
|      - |  200 | `				/* A static property belongs to the CLASS, never to an object: php` |
|      - |  201 | `				 * iterates only the instance's own properties. PHL's instance` |
|      - |  202 | `				 * attribute table carries an entry for every declared member` |
|      - |  203 | `				 * (statics share the class slot), so it has to filter here — the` |
|      - |  204 | `				 * same test var_dump/get_object_vars/json/serialize already make. */` |
|     64 |  205 | `				continue;` |
|      - |  206 | `			}` |
|     84 |  207 | `			if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|     47 |  208 | `			 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|      3 |  209 | `				continue; /* virtual set-only property: iteration skips it (php) */` |
|      - |  210 | `			}` |
|      - |  211 | `			/* Check access permission */` |
|    128 |  212 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|     82 |  213 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|     75 |  214 | `					break; /* Access is granted */` |
|      - |  215 | `			}` |
|      3 |  216 | `		}` |
|    131 |  217 | `		if( pEntry == 0 ){` |
|      - |  218 | `			/* Clean up the mess left behind */` |
|     61 |  219 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|      - |  220 | `			/* The binding survives the loop (see the hashmap step) */` |
|     61 |  221 | `			VmForeachStepUnlink(pInfo,pStep);` |
|     61 |  222 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     61 |  223 | `			PH7_ClassInstanceUnref(pThis);` |
|     33 |  224 | `		}else{` |
|     75 |  225 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|      - |  226 | `			ph7_value *pAttrValue;` |
|     75 |  227 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|      - |  228 | `				/* Fill with the current attribute name. A MANGLED name — only the` |
|      - |  229 | `				 * __PHP_Incomplete_Class carrier stores those — yields its plain` |
|      - |  230 | `				 * part: php's iterator unmangles the key it hands out. */` |
|     71 |  231 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|     71 |  232 | `				if( pKey ){` |
|      - |  233 | `					SyString sUnmCls, sUnmName;` |
|     71 |  234 | `					SyStringInitFromBuf(&sUnmName,pAttrName->zString,pAttrName->nByte);` |
|     71 |  235 | `					if( pAttrName->nByte > 0 && pAttrName->zString[0] == 0 ){` |
|      5 |  236 | `						PH7_UnmangleAttrName(pAttrName->zString,pAttrName->nByte,&sUnmCls,&sUnmName);` |
|      2 |  237 | `					}` |
|     71 |  238 | `					SyBlobReset(&pKey->sBlob);` |
|     71 |  239 | `					SyBlobAppend(&pKey->sBlob,sUnmName.zString,sUnmName.nByte);` |
|     71 |  240 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|     33 |  241 | `				}` |
|     33 |  242 | `			}` |
|     70 |  243 | `			if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET))` |
|     41 |  244 | `			 && (pStep->iFlags & PH7_4EACH_STEP_REF)` |
|     12 |  245 | `			 && !VmHookGuardHeld(pVm,(void *)pThis,&pVmAttr->pAttr->sName) ){` |
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
|     68 |  264 | `			if( (pStep->iFlags & PH7_4EACH_STEP_REF) == 0` |
|     70 |  265 | `			 && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET) != 0 ){` |
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
|     63 |  296 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|     63 |  297 | `			if( pAttrValue ){` |
|     63 |  298 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|      - |  299 | `					/* Pass by reference (registered — see the hashmap step) */` |
|     10 |  300 | `					PH7_VmBindVarSlot(&(*pVm),pFrameLocal,SyStringData(&pInfo->sValue),` |
|      3 |  301 | `						SyStringLength(&pInfo->sValue),pVmAttr->nIdx);` |
|      4 |  302 | `				}else{` |
|      - |  303 | `					/* Make a copy of the attribute value */` |
|     57 |  304 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|     57 |  305 | `					if( pValue ){` |
|     57 |  306 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|     26 |  307 | `					}` |
|      - |  308 | `				}` |
|     29 |  309 | `			}` |
|      - |  310 | `		}` |
|      - |  311 | `	}` |
| 333555 |  312 | `	VM_EXIT_BREAK;` |
| 166793 |  313 | `}` |
|      - |  314 |  |
|      - |  315 | `/*` |
|      - |  316 | ` * OP_FOREACH_INIT: body moved verbatim from the OP_FOREACH_INIT arm of` |
|      - |  317 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  318 | ` */` |
|  28092 |  319 | `PH7_PRIVATE VmOpRc VmExecOpForeachInit(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  320 | `{` |
|  28097 |  321 | `	ph7_value *pTos = pState->pTos;` |
|  28097 |  322 | `	ph7_value *pStack = pState->pStack;` |
|  28097 |  323 | `	VmInstr *aInstr = pState->aInstr;` |
|  28097 |  324 | `	sxi32 pc = pState->pc;` |
|      - |  325 | `	sxi32 rc;` |
|  28097 |  326 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|      - |  327 | `	void *pName;` |
|      - |  328 | `#ifdef UNTRUST` |
|      - |  329 | `	if( pTos < pStack ){` |
|      - |  330 | `		VM_EXIT_ABORT;` |
|      - |  331 | `	}` |
|      - |  332 | `#endif` |
|  28097 |  333 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|      - |  334 | `		/* Take the variable name from the top of the stack */` |
|    ! 0 |  335 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      - |  336 | `			/* Force a string cast */` |
|    ! 0 |  337 | `			PH7_MemObjToString(pTos);` |
|    ! 0 |  338 | `		}` |
|      - |  339 | `		/* Duplicate name */` |
|    ! 0 |  340 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|    ! 0 |  341 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|    ! 0 |  342 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|    ! 0 |  343 | `		}` |
|    ! 0 |  344 | `		VmPopOperand(&pTos,1);` |
|    ! 0 |  345 | `	}` |
|  28097 |  346 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|    ! 0 |  347 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      - |  348 | `			/* Force a string cast */` |
|    ! 0 |  349 | `			PH7_MemObjToString(pTos);` |
|    ! 0 |  350 | `		}` |
|      - |  351 | `		/* Duplicate name */` |
|    ! 0 |  352 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|    ! 0 |  353 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|    ! 0 |  354 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|    ! 0 |  355 | `		}` |
|    ! 0 |  356 | `		VmPopOperand(&pTos,1);` |
|    ! 0 |  357 | `	}` |
|      - |  358 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|  28097 |  359 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|      - |  360 | `		/* Jump out of the loop */` |
|      7 |  361 | `		if( SyStringLength(&pInfo->sValue) > 0 ){` |
|      - |  362 | `			/* php warns for EVERY non-iterable, null included (PH7 exempted null). */` |
|     10 |  363 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|      - |  364 | `				"foreach() argument must be of type array\|object, %s given",` |
|      3 |  365 | `				VmArithValueName(pTos));` |
|      3 |  366 | `		}` |
|      7 |  367 | `		pc = pInstr->iP2 - 1;` |
|      4 |  368 | `	}else{` |
|      - |  369 | `		ph7_foreach_step *pStep;` |
|  28091 |  370 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|  28091 |  371 | `		if( pStep == 0 ){` |
|    ! 0 |  372 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      - |  373 | `			/* Jump out of the loop */` |
|    ! 0 |  374 | `			pc = pInstr->iP2 - 1;` |
|    ! 0 |  375 | `		}else{` |
|      - |  376 | `			/* Zero the structure */` |
|  28091 |  377 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|      - |  378 | `			/* Prepare the step */` |
|  28091 |  379 | `			pStep->iFlags = pInfo->iFlags;` |
|      - |  380 | `			/* Record the owning activation so OP_FOREACH_STEP can pick THIS` |
|      - |  381 | `			 * activation's step out of the per-statement stack — two suspended` |
|      - |  382 | `			 * generator/fiber instances (or a recursive call) paused in the same` |
|      - |  383 | `			 * textual foreach otherwise resume onto each other's cursor. */` |
|  28091 |  384 | `			pStep->pFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|  28091 |  385 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|      - |  386 | `				ph7_hashmap *pMap,*pIterMap;` |
|      - |  387 | `				/* COW: For by-reference foreach, eagerly separate the` |
|      - |  388 | `				 * source array so mutations don't affect other sharers. */` |
|  27643 |  389 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|     51 |  390 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|     51 |  391 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|     51 |  392 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|      - |  393 | `						/* Only adjust refcounts/separate if the backing` |
|      - |  394 | `						 * variable still points at the same hashmap as` |
|      - |  395 | `						 * the stack value. */` |
|     51 |  396 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|     51 |  397 | `							pCur->iRef--;` |
|      - |  398 | `							/* Use the returned map, not pBacking->x.pOther: PH7_HashmapDup` |
|      - |  399 | `							 * inside CowSeparate can reallocate (move) pVm->aMemObj and leave` |
|      - |  400 | `							 * pBacking dangling. The return value is the post-separation map. */` |
|     51 |  401 | `							pTos->x.pOther = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|     51 |  402 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|     24 |  403 | `						}` |
|     24 |  404 | `					}` |
|     24 |  405 | `				}` |
|  27643 |  406 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|  27643 |  407 | `				pIterMap = pMap;` |
|  27643 |  408 | `				if( pMap == pVm->pGlobal && (pStep->iFlags & PH7_4EACH_STEP_REF) == 0 ){` |
|      - |  409 | `					/* php 8.1: foreach ($GLOBALS as ...) by value iterates a` |
|      - |  410 | `					 * SNAPSHOT of the symbol table — globals created inside` |
|      - |  411 | `					 * the loop body must not be visited (the live map would` |
|      - |  412 | `					 * grow under the cursor). By-ref foreach keeps the live` |
|      - |  413 | `					 * map, like php. On OOM fall back to the live map. */` |
|      5 |  414 | `					ph7_hashmap *pSnap = PH7_NewHashmap(&(*pVm),0,0);` |
|      5 |  415 | `					if( pSnap && PH7_HashmapDupMaterialized(pMap,pSnap) == SXRET_OK ){` |
|      - |  416 | `						/* The step consumes the snapshot's initial reference */` |
|      5 |  417 | `						pIterMap = pSnap;` |
|      2 |  418 | `					}else if( pSnap ){` |
|    ! 0 |  419 | `						PH7_HashmapUnref(pSnap);` |
|    ! 0 |  420 | `					}` |
|      2 |  421 | `				}` |
|  27643 |  422 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|  27643 |  423 | `				pStep->xIter.pMap = pIterMap;` |
|  27643 |  424 | `				if( pIterMap == pMap ){` |
|  27639 |  425 | `					pMap->iRef++;` |
|  13817 |  426 | `				}` |
|      - |  427 | `				/* Private cursor + registry (php: nested foreach over one` |
|      - |  428 | `				 * array are independent; foreach never moves the internal` |
|      - |  429 | `				 * pointer — see PH7_HashmapRegisterForeachStep) */` |
|  27643 |  430 | `				PH7_HashmapRegisterForeachStep(pIterMap,pStep);` |
|  13824 |  431 | `			}else{` |
|    453 |  432 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|      - |  433 | `				ph7_class *pIteratorClass;` |
|      - |  434 | `				/* Check if the object implements Iterator */` |
|    453 |  435 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|    605 |  436 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|      - |  437 | `					/* Iterator-based iteration: call rewind() */` |
|      - |  438 | `					ph7_class_method *pRewind;` |
|    319 |  439 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|    319 |  440 | `					pStep->xIter.pThis = pThis;` |
|    319 |  441 | `					pThis->iRef++;` |
|    319 |  442 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|    319 |  443 | `					if( pRewind ){` |
|    319 |  444 | `						rc = PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|    319 |  445 | `						if( VmIterCallThrew(rc) ){` |
|      - |  446 | `							/* rewind() threw (a generator body or userland Iterator):` |
|      - |  447 | `							 * undo this step's retain, drop the step, and route the` |
|      - |  448 | `							 * exception instead of silently starting the loop. */` |
|     14 |  449 | `							pThis->iRef--;` |
|     14 |  450 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     14 |  451 | `							pStep = 0;` |
|     14 |  452 | `							PH7_DISPATCH_ITER_RC(rc,1)` |
|    ! 0 |  453 | `						}` |
|    152 |  454 | `					}` |
|    157 |  455 | `				}else{` |
|      - |  456 | `					/* Check if the object implements IteratorAggregate */` |
|      - |  457 | `					ph7_class *pIterAggClass;` |
|    139 |  458 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|      - |  459 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|    177 |  460 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|      - |  461 | `						/* Call getIterator() and use the returned Iterator object */` |
|      - |  462 | `						ph7_class_method *pGetIter;` |
|     79 |  463 | `						int iterAggOk = 0;` |
|     79 |  464 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|     79 |  465 | `						if( pGetIter ){` |
|      - |  466 | `							ph7_value sResult;` |
|     79 |  467 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|     79 |  468 | `							rc = PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|     79 |  469 | `							if( VmIterCallThrew(rc) ){` |
|      - |  470 | `								/* getIterator() threw: drop the step and route the` |
|      - |  471 | `								 * exception (don't pile the "must implement Iterator"` |
|      - |  472 | `								 * error on top of it). */` |
|    ! 0 |  473 | `								PH7_MemObjRelease(&sResult);` |
|    ! 0 |  474 | `								SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    ! 0 |  475 | `								pStep = 0;` |
|    ! 0 |  476 | `								PH7_DISPATCH_ITER_RC(rc,1)` |
|    ! 0 |  477 | `							}` |
|     79 |  478 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|     79 |  479 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|     79 |  480 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|      - |  481 | `									ph7_class_method *pRewind;` |
|     79 |  482 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|     79 |  483 | `									pStep->xIter.pThis = pIterObj;` |
|     79 |  484 | `									pIterObj->iRef++;` |
|      - |  485 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|     79 |  486 | `									pStep->pOwner = pThis;` |
|     79 |  487 | `									pThis->iRef++;` |
|     79 |  488 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|     79 |  489 | `									if( pRewind ){` |
|     79 |  490 | `										rc = PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|     79 |  491 | `										if( VmIterCallThrew(rc) ){` |
|      - |  492 | `											/* The aggregate's iterator rewind() threw: undo` |
|      - |  493 | `											 * both retains, drop the step, route the exception. */` |
|    ! 0 |  494 | `											pIterObj->iRef--;` |
|    ! 0 |  495 | `											pThis->iRef--;` |
|    ! 0 |  496 | `											PH7_MemObjRelease(&sResult);` |
|    ! 0 |  497 | `											SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    ! 0 |  498 | `											pStep = 0;` |
|    ! 0 |  499 | `											PH7_DISPATCH_ITER_RC(rc,1)` |
|    ! 0 |  500 | `										}` |
|     38 |  501 | `									}` |
|     79 |  502 | `									iterAggOk = 1;` |
|     38 |  503 | `								}` |
|     38 |  504 | `							}` |
|     79 |  505 | `							PH7_MemObjRelease(&sResult);` |
|     38 |  506 | `						}` |
|     79 |  507 | `						if( !iterAggOk ){` |
|      - |  508 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|    ! 0 |  509 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|      - |  510 | `								"Object returned by getIterator() must implement Iterator");` |
|    ! 0 |  511 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    ! 0 |  512 | `							pStep = 0; /* Signal: do not store this step */` |
|    ! 0 |  513 | `							pc = pInstr->iP2 - 1;` |
|    ! 0 |  514 | `						}` |
|     41 |  515 | `					}else{` |
|      - |  516 | `						/* Plain object iteration via hAttr */` |
|     63 |  517 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|     63 |  518 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|     63 |  519 | `						pStep->xIter.pThis = pThis;` |
|     63 |  520 | `						pThis->iRef++;` |
|      - |  521 | `					}` |
|      - |  522 | `				}` |
|      - |  523 | `			}` |
|      - |  524 | `		}` |
|  28081 |  525 | `		if( pStep ){` |
|  28081 |  526 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|    ! 0 |  527 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|    ! 0 |  528 | `				if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|    ! 0 |  529 | `					VmForeachHashmapStepRelease(&(*pVm),pInfo,pStep,FALSE/*never made it onto aStep*/);` |
|    ! 0 |  530 | `				}else{` |
|    ! 0 |  531 | `					SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      - |  532 | `				}` |
|      - |  533 | `				/* Jump out of the loop */` |
|    ! 0 |  534 | `				pc = pInstr->iP2 - 1;` |
|    ! 0 |  535 | `			}` |
|  14038 |  536 | `		}` |
|      - |  537 | `	}` |
|  28087 |  538 | `	VmPopOperand(&pTos,1);` |
|  28087 |  539 | `	VM_EXIT_BREAK;` |
|    ! 0 |  540 | `	VM_EXIT_BREAK;` |
|  14051 |  541 | `}` |
|      - |  542 |  |
