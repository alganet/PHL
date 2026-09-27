# src/ph7/vm_ops_iter.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 268/334 lines (80.24%)

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
| 413664 |   30 | `PH7_PRIVATE VmOpRc VmExecOpForeachStep(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |   31 | `{` |
| 413669 |   32 | `	ph7_value *pTos = pState->pTos;` |
| 413669 |   33 | `	ph7_value *pStack = pState->pStack;` |
| 413669 |   34 | `	VmInstr *aInstr = pState->aInstr;` |
| 413669 |   35 | `	sxi32 pc = pState->pc;` |
|      - |   36 | `	sxi32 rc;` |
| 413669 |   37 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|      - |   38 | `	ph7_foreach_step **apStep,*pStep;` |
|      - |   39 | `	ph7_value *pValue;` |
|      - |   40 | `	VmFrame *pFrameLocal;` |
|      - |   41 | `	sxu32 nStep;` |
| 413669 |   42 | `	pFrameLocal = pVm->pFrame;` |
| 413669 |   43 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      - |   44 | `	/* Select THIS activation's step. aStep is per-STATEMENT and shared by every` |
|      - |   45 | `	 * activation, so peeking the last entry resumes onto a sibling's cursor when` |
|      - |   46 | `	 * two instances of one generator/fiber are suspended in the same textual` |
|      - |   47 | `	 * foreach. Scan from the top (most-recent push) for the step whose owning` |
|      - |   48 | `	 * frame matches the running activation; top-down makes the current push win` |
|      - |   49 | `	 * over any leaked older step that happens to share a recycled frame address. */` |
| 413669 |   50 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
| 413669 |   51 | `	nStep = SySetUsed(&pInfo->aStep);` |
| 413669 |   52 | `	if( nStep < 1 ){` |
|      - |   53 | `		/* Defensive: OP_FOREACH_INIT always pushes this activation's step before` |
|      - |   54 | `		 * STEP runs (and jumps past the loop when the push fails), so an empty` |
|      - |   55 | `		 * set is unreachable — guard the apStep[-1] read anyway. Jump out. */` |
|    ! 0 |   56 | `		pc = pInstr->iP2 - 1;` |
|    ! 0 |   57 | `		VM_EXIT_BREAK;` |
|      - |   58 | `	}` |
| 413669 |   59 | `	pStep = apStep[nStep - 1];` |
| 413695 |   60 | `	while( nStep > 0 ){` |
| 413695 |   61 | `		if( apStep[nStep - 1]->pFrame == pFrameLocal ){` |
| 413669 |   62 | `			pStep = apStep[nStep - 1];` |
| 413669 |   63 | `			break;` |
|      - |   64 | `		}` |
|     27 |   65 | `		nStep--;` |
|      1 |   66 | `	}` |
| 413669 |   67 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|      - |   68 | `		ph7_hashmap_node *pNode;` |
|      - |   69 | `		/* Extract the current node via this loop's PRIVATE cursor (php:` |
|      - |   70 | `		 * nested foreach over the same array are independent iterations) */` |
| 411413 |   71 | `		pNode = pStep->pCursor;` |
| 411413 |   72 | `		if( pNode == 0 ){` |
|      - |   73 | `			/* No more entry to process */` |
|  31493 |   74 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|      - |   75 | `			/* php does NOT break the binding: the value variable stays a reference to the` |
|      - |   76 | `` 			 * LAST element after the loop — that is what makes a second `foreach ($a as $v)` `` |
|      - |   77 | ``			 * write through it (the famous gotcha), and what the `unset($v)` idiom exists to`` |
|      - |   78 | `			 * undo. Deleting the name here left $v undefined instead. */` |
|      - |   79 | `			/* Cleanup the mess left behind */` |
|  31493 |   80 | `			VmForeachHashmapStepRelease(&(*pVm),pInfo,pStep,TRUE);` |
|  15749 |   81 | `		}else{` |
|      - |   82 | `			/* Advance the private cursor */` |
| 379925 |   83 | `			pStep->pCursor = pNode->pPrev; /* Reverse link */` |
|      - |   84 | `			/* Bind the VALUE before the KEY: on the first iteration this is where` |
|      - |   85 | `			 * both locals are created in the frame table, and php's symbol table` |
|      - |   86 | `			 * lists the value ahead of the key (get_defined_vars() order). Only the` |
|      - |   87 | `			 * creation ORDER matters here; the stored values are independent. */` |
| 379925 |   88 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|      - |   89 | `				/* Pass by reference — a REGISTERED binding (PH7_VmBindVarSlot), so the element` |
|      - |   90 | `				 * counts the loop variable as a holder for as long as it is bound, exactly as` |
|      - |   91 | `				 * php's reference does. */` |
|    198 |   92 | `				PH7_VmBindVarSlot(&(*pVm),pFrameLocal,SyStringData(&pInfo->sValue),` |
|     65 |   93 | `					SyStringLength(&pInfo->sValue),pNode->nValIdx);` |
|     68 |   94 | `			}else{` |
|      - |   95 | `				/* Make a copy of the entry value */` |
| 379795 |   96 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
| 379795 |   97 | `				if( pValue ){` |
| 379795 |   98 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
| 189899 |   99 | `				}` |
|      - |  100 | `			}` |
| 379925 |  101 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|  10498 |  102 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|  10498 |  103 | `				if( pKey ){` |
|  10498 |  104 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|   5254 |  105 | `				}` |
|   5254 |  106 | `			}` |
|      5 |  107 | `		}` |
| 207969 |  108 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|      - |  109 | `		/* Iterator-based iteration.` |
|      - |  110 | `		 * Sequence: on first call just check valid/current/key.` |
|      - |  111 | `		 * On subsequent calls, advance with next() first, then check.` |
|      - |  112 | `		 */` |
|   2131 |  113 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|      - |  114 | `		ph7_class_method *pMethod;` |
|      - |  115 | `		ph7_value sResult;` |
|   2131 |  116 | `		int isValid = 0;` |
|      - |  117 | `		/* Call next() to advance — but skip on the first iteration */` |
|   2131 |  118 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|    481 |  119 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|    243 |  120 | `		}else{` |
|   1655 |  121 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|   1655 |  122 | `			if( pMethod ){` |
|   1655 |  123 | `				rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|   1655 |  124 | `				if( VmIterCallThrew(rc) ){` |
|      - |  125 | `					/* next() threw (generator body / userland Iterator): tear the` |
|      - |  126 | `					 * step down like exhaustion does, then route the exception —` |
|      - |  127 | `					 * the loop must not silently end with execution continuing. */` |
|     18 |  128 | `					VmForeachStepAbandon(pVm,pInfo,pStep,pThis);` |
|     18 |  129 | `					PH7_DISPATCH_ITER_RC(rc,0)` |
|    ! 0 |  130 | `				}` |
|    818 |  131 | `			}` |
|      - |  132 | `		}` |
|      - |  133 | `		/* Call valid() */` |
|   2117 |  134 | `		PH7_MemObjInit(pVm,&sResult);` |
|   2117 |  135 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|   2117 |  136 | `		if( pMethod ){` |
|   2117 |  137 | `			rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|   2117 |  138 | `			if( VmIterCallThrew(rc) ){` |
|      - |  139 | `				/* valid() threw: same teardown-and-route as next() above. */` |
|    ! 0 |  140 | `				PH7_MemObjRelease(&sResult);` |
|    ! 0 |  141 | `				VmForeachStepAbandon(pVm,pInfo,pStep,pThis);` |
|    ! 0 |  142 | `				PH7_DISPATCH_ITER_RC(rc,0)` |
|    ! 0 |  143 | `			}` |
|   2117 |  144 | `			PH7_MemObjToBool(&sResult);` |
|   2117 |  145 | `			isValid = (sResult.x.iVal != 0);` |
|   1056 |  146 | `		}` |
|   2117 |  147 | `		PH7_MemObjRelease(&sResult);` |
|   2117 |  148 | `		if( !isValid ){` |
|      - |  149 | `			/* Iterator exhausted */` |
|    453 |  150 | `			pc = pInstr->iP2 - 1;` |
|      - |  151 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|    453 |  152 | `			VmForeachStepAbandon(pVm,pInfo,pStep,pThis);` |
|    229 |  153 | `		}else{` |
|      - |  154 | `			/* Call current() to get value */` |
|   1669 |  155 | `			PH7_MemObjInit(pVm,&sResult);` |
|   1669 |  156 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|   1669 |  157 | `			if( pMethod ){` |
|   1669 |  158 | `				rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|   1669 |  159 | `				if( VmIterCallThrew(rc) ){` |
|      - |  160 | `					/* current() threw: same teardown-and-route as next() above. */` |
|    ! 0 |  161 | `					PH7_MemObjRelease(&sResult);` |
|    ! 0 |  162 | `					VmForeachStepAbandon(pVm,pInfo,pStep,pThis);` |
|    ! 0 |  163 | `					PH7_DISPATCH_ITER_RC(rc,0)` |
|    ! 0 |  164 | `				}` |
|    832 |  165 | `			}` |
|   1669 |  166 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   1669 |  167 | `			if( pValue ){` |
|   1669 |  168 | `				PH7_MemObjStore(&sResult,pValue);` |
|    832 |  169 | `			}` |
|   1669 |  170 | `			PH7_MemObjRelease(&sResult);` |
|      - |  171 | `			/* Call key() if needed */` |
|   1669 |  172 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      - |  173 | `				ph7_value sKey;` |
|    467 |  174 | `				PH7_MemObjInit(pVm,&sKey);` |
|    467 |  175 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|    467 |  176 | `				if( pMethod ){` |
|    467 |  177 | `					rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|    467 |  178 | `					if( VmIterCallThrew(rc) ){` |
|      - |  179 | `						/* key() threw: same teardown-and-route as next() above. */` |
|    ! 0 |  180 | `						PH7_MemObjRelease(&sKey);` |
|    ! 0 |  181 | `						VmForeachStepAbandon(pVm,pInfo,pStep,pThis);` |
|    ! 0 |  182 | `						PH7_DISPATCH_ITER_RC(rc,0)` |
|    ! 0 |  183 | `					}` |
|    231 |  184 | `				}` |
|    467 |  185 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|    467 |  186 | `				if( pValue ){` |
|    467 |  187 | `					PH7_MemObjStore(&sKey,pValue);` |
|    231 |  188 | `				}` |
|    467 |  189 | `				PH7_MemObjRelease(&sKey);` |
|    231 |  190 | `			}` |
|      - |  191 | `		}` |
|   1061 |  192 | `	}else{` |
|    135 |  193 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|    135 |  194 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|      - |  195 | `		SyHashEntry *pEntry;` |
|      - |  196 | `		/* Point to the next attribute */` |
|    231 |  197 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|    173 |  198 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    173 |  199 | `			if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_HIDDEN) ){` |
|      - |  200 | `				/* A static property belongs to the CLASS, never to an object: php` |
|      - |  201 | `				 * iterates only the instance's own properties. PHL's instance` |
|      - |  202 | `				 * attribute table carries an entry for every declared member` |
|      - |  203 | `				 * (statics share the class slot), so it has to filter here — the` |
|      - |  204 | `				 * same test var_dump/get_object_vars/json/serialize already make. */` |
|     66 |  205 | `				continue;` |
|      - |  206 | `			}` |
|    104 |  207 | `			if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|     57 |  208 | `			 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|      3 |  209 | `				continue; /* virtual set-only property: iteration skips it (php) */` |
|      - |  210 | `			}` |
|    107 |  211 | `			if( PH7_ClassAttrUninitializedForRead(pVmAttr) ){` |
|     19 |  212 | `				continue; /* typed, never written: not there yet (php) */` |
|      - |  213 | `			}` |
|      - |  214 | `			/* Check access permission */` |
|    131 |  215 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|     84 |  216 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|     77 |  217 | `					break; /* Access is granted */` |
|      - |  218 | `			}` |
|      3 |  219 | `		}` |
|    135 |  220 | `		if( pEntry == 0 ){` |
|      - |  221 | `			/* Clean up the mess left behind */` |
|     63 |  222 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|      - |  223 | `			/* The binding survives the loop (see the hashmap step) */` |
|     63 |  224 | `			VmForeachStepUnlink(pInfo,pStep);` |
|     63 |  225 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     63 |  226 | `			PH7_ClassInstanceUnref(pThis);` |
|     34 |  227 | `		}else{` |
|     77 |  228 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|      - |  229 | `			ph7_value *pAttrValue;` |
|     77 |  230 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|      - |  231 | `				/* Fill with the current attribute name. A MANGLED name — only the` |
|      - |  232 | `				 * __PHP_Incomplete_Class carrier stores those — yields its plain` |
|      - |  233 | `				 * part: php's iterator unmangles the key it hands out. */` |
|     73 |  234 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|     73 |  235 | `				if( pKey ){` |
|      - |  236 | `					SyString sUnmCls, sUnmName;` |
|     73 |  237 | `					SyStringInitFromBuf(&sUnmName,pAttrName->zString,pAttrName->nByte);` |
|     73 |  238 | `					if( pAttrName->nByte > 0 && pAttrName->zString[0] == 0 ){` |
|      5 |  239 | `						PH7_UnmangleAttrName(pAttrName->zString,pAttrName->nByte,&sUnmCls,&sUnmName);` |
|      2 |  240 | `					}` |
|     73 |  241 | `					SyBlobReset(&pKey->sBlob);` |
|     73 |  242 | `					SyBlobAppend(&pKey->sBlob,sUnmName.zString,sUnmName.nByte);` |
|     73 |  243 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|     34 |  244 | `				}` |
|     34 |  245 | `			}` |
|     72 |  246 | `			if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET))` |
|     42 |  247 | `			 && (pStep->iFlags & PH7_4EACH_STEP_REF)` |
|     12 |  248 | `			 && !VmHookGuardHeld(pVm,(void *)pThis,&pVmAttr->pAttr->sName) ){` |
|      - |  249 | `				/* php: a hooked property (virtual or backed) cannot be iterated` |
|      - |  250 | `				 * by reference — catchable Error. Tear the step down like the` |
|      - |  251 | `				 * exhausted-iteration path (break the by-ref binding, unlink,` |
|      - |  252 | `				 * free, drop the instance retain) so nothing leaks and a` |
|      - |  253 | `				 * re-entered foreach starts fresh; the fetch-point router lands` |
|      - |  254 | `				 * the parked throw right after this op. Inside the property's` |
|      - |  255 | `				 * own hook body the guard keeps raw semantics (no Error). */` |
|      - |  256 | `				SyBlob sErrMsg;` |
|      3 |  257 | `				SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      3 |  258 | `				SyBlobFormat(&sErrMsg,"Cannot create reference to property %z::$%z",` |
|      2 |  259 | `					&pThis->pClass->sName,&pVmAttr->pAttr->sName);` |
|      3 |  260 | `				VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      3 |  261 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|      3 |  262 | `				VmForeachStepUnlink(pInfo,pStep);` |
|      3 |  263 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      3 |  264 | `				PH7_ClassInstanceUnref(pThis);` |
|      3 |  265 | `				VM_EXIT_BREAK;` |
|      - |  266 | `			}` |
|     70 |  267 | `			if( (pStep->iFlags & PH7_4EACH_STEP_REF) == 0` |
|     72 |  268 | `			 && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET) != 0 ){` |
|      - |  269 | `				/* PHP 8.4 property hooks: object iteration reads through the` |
|      - |  270 | `				 * get hook (virtual properties included; the flag gate keeps` |
|      - |  271 | `				 * hook-free classes on the raw zero-copy path below). The` |
|      - |  272 | `				 * object step walks hAttr with the hash's single EMBEDDED` |
|      - |  273 | `				 * cursor — save/restore it around the dispatch so a hook that` |
|      - |  274 | `				 * re-enters an hAttr walk on this instance (get_object_vars,` |
|      - |  275 | `				 * json_encode of $this) can't truncate THIS iteration. A hook` |
|      - |  276 | `				 * that unset()s the property the saved cursor points at stays` |
|      - |  277 | `				 * a recorded hazard (php's own semantics there are murky). */` |
|      - |  278 | `				ph7_value sHookVal;` |
|      - |  279 | `				sxi32 rcHk;` |
|     11 |  280 | `				SyHashEntry_Pr *pSavedCur = pThis->hAttr.pCurrent;` |
|     11 |  281 | `				PH7_MemObjInit(pVm,&sHookVal);` |
|     11 |  282 | `				rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);` |
|     11 |  283 | `				pThis->hAttr.pCurrent = pSavedCur;` |
|     11 |  284 | `				if( rcHk != SXERR_NOTFOUND ){` |
|     11 |  285 | `					if( rcHk == SXRET_OK ){` |
|     11 |  286 | `						pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|     11 |  287 | `						if( pValue ){` |
|     11 |  288 | `							PH7_MemObjStore(&sHookVal,pValue);` |
|      5 |  289 | `						}` |
|      5 |  290 | `					}` |
|      - |  291 | `					/* a throw parked on the boundary rail: the fetch-point` |
|      - |  292 | `					 * router lands it right after this op */` |
|     11 |  293 | `					PH7_MemObjRelease(&sHookVal);` |
|     11 |  294 | `					VM_EXIT_BREAK;` |
|      - |  295 | `				}` |
|    ! 0 |  296 | `				PH7_MemObjRelease(&sHookVal);` |
|    ! 0 |  297 | `			}` |
|      - |  298 | `			/* Extract attribute value */` |
|     65 |  299 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|     65 |  300 | `			if( pAttrValue ){` |
|     65 |  301 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|      - |  302 | `					/* Pass by reference (registered — see the hashmap step) */` |
|     10 |  303 | `					PH7_VmBindVarSlot(&(*pVm),pFrameLocal,SyStringData(&pInfo->sValue),` |
|      3 |  304 | `						SyStringLength(&pInfo->sValue),pVmAttr->nIdx);` |
|      4 |  305 | `				}else{` |
|      - |  306 | `					/* Make a copy of the attribute value */` |
|     59 |  307 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|     59 |  308 | `					if( pValue ){` |
|     59 |  309 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|     27 |  310 | `					}` |
|      - |  311 | `				}` |
|     30 |  312 | `			}` |
|      - |  313 | `		}` |
|      - |  314 | `	}` |
| 413643 |  315 | `	VM_EXIT_BREAK;` |
| 206841 |  316 | `}` |
|      - |  317 |  |
|      - |  318 | `/*` |
|      - |  319 | ` * OP_FOREACH_INIT: body moved verbatim from the OP_FOREACH_INIT arm of` |
|      - |  320 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  321 | ` */` |
|  32154 |  322 | `PH7_PRIVATE VmOpRc VmExecOpForeachInit(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  323 | `{` |
|  32159 |  324 | `	ph7_value *pTos = pState->pTos;` |
|  32159 |  325 | `	ph7_value *pStack = pState->pStack;` |
|  32159 |  326 | `	VmInstr *aInstr = pState->aInstr;` |
|  32159 |  327 | `	sxi32 pc = pState->pc;` |
|      - |  328 | `	sxi32 rc;` |
|  32159 |  329 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|      - |  330 | `	void *pName;` |
|      - |  331 | `#ifdef UNTRUST` |
|      - |  332 | `	if( pTos < pStack ){` |
|      - |  333 | `		VM_EXIT_ABORT;` |
|      - |  334 | `	}` |
|      - |  335 | `#endif` |
|  32159 |  336 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|      - |  337 | `		/* Take the variable name from the top of the stack */` |
|    ! 0 |  338 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      - |  339 | `			/* Force a string cast */` |
|    ! 0 |  340 | `			PH7_MemObjToString(pTos);` |
|    ! 0 |  341 | `		}` |
|      - |  342 | `		/* Duplicate name */` |
|    ! 0 |  343 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|    ! 0 |  344 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|    ! 0 |  345 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|    ! 0 |  346 | `		}` |
|    ! 0 |  347 | `		VmPopOperand(&pTos,1);` |
|    ! 0 |  348 | `	}` |
|  32159 |  349 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|    ! 0 |  350 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      - |  351 | `			/* Force a string cast */` |
|    ! 0 |  352 | `			PH7_MemObjToString(pTos);` |
|    ! 0 |  353 | `		}` |
|      - |  354 | `		/* Duplicate name */` |
|    ! 0 |  355 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|    ! 0 |  356 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|    ! 0 |  357 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|    ! 0 |  358 | `		}` |
|    ! 0 |  359 | `		VmPopOperand(&pTos,1);` |
|    ! 0 |  360 | `	}` |
|      - |  361 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|  32159 |  362 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|      - |  363 | `		/* Jump out of the loop */` |
|      7 |  364 | `		if( SyStringLength(&pInfo->sValue) > 0 ){` |
|      - |  365 | `			/* php warns for EVERY non-iterable, null included (PH7 exempted null). */` |
|     10 |  366 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|      - |  367 | `				"foreach() argument must be of type array\|object, %s given",` |
|      3 |  368 | `				VmArithValueName(pTos));` |
|      3 |  369 | `		}` |
|      7 |  370 | `		pc = pInstr->iP2 - 1;` |
|      4 |  371 | `	}else{` |
|      - |  372 | `		ph7_foreach_step *pStep;` |
|  32153 |  373 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|  32153 |  374 | `		if( pStep == 0 ){` |
|    ! 0 |  375 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      - |  376 | `			/* Jump out of the loop */` |
|    ! 0 |  377 | `			pc = pInstr->iP2 - 1;` |
|    ! 0 |  378 | `		}else{` |
|      - |  379 | `			/* Zero the structure */` |
|  32153 |  380 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|      - |  381 | `			/* Prepare the step */` |
|  32153 |  382 | `			pStep->iFlags = pInfo->iFlags;` |
|      - |  383 | `			/* Record the owning activation so OP_FOREACH_STEP can pick THIS` |
|      - |  384 | `			 * activation's step out of the per-statement stack — two suspended` |
|      - |  385 | `			 * generator/fiber instances (or a recursive call) paused in the same` |
|      - |  386 | `			 * textual foreach otherwise resume onto each other's cursor. */` |
|  32153 |  387 | `			pStep->pFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|  32153 |  388 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|      - |  389 | `				ph7_hashmap *pMap,*pIterMap;` |
|      - |  390 | `				/* COW: For by-reference foreach, eagerly separate the` |
|      - |  391 | `				 * source array so mutations don't affect other sharers. */` |
|  31607 |  392 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|     51 |  393 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|     51 |  394 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|     51 |  395 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|      - |  396 | `						/* Only adjust refcounts/separate if the backing` |
|      - |  397 | `						 * variable still points at the same hashmap as` |
|      - |  398 | `						 * the stack value. */` |
|     51 |  399 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|     51 |  400 | `							pCur->iRef--;` |
|      - |  401 | `							/* Use the returned map, not pBacking->x.pOther: PH7_HashmapDup` |
|      - |  402 | `							 * inside CowSeparate can reallocate (move) pVm->aMemObj and leave` |
|      - |  403 | `							 * pBacking dangling. The return value is the post-separation map. */` |
|     51 |  404 | `							pTos->x.pOther = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|     51 |  405 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|     24 |  406 | `						}` |
|     24 |  407 | `					}` |
|     24 |  408 | `				}` |
|  31607 |  409 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|  31607 |  410 | `				pIterMap = pMap;` |
|  31607 |  411 | `				if( pMap == pVm->pGlobal && (pStep->iFlags & PH7_4EACH_STEP_REF) == 0 ){` |
|      - |  412 | `					/* php 8.1: foreach ($GLOBALS as ...) by value iterates a` |
|      - |  413 | `					 * SNAPSHOT of the symbol table — globals created inside` |
|      - |  414 | `					 * the loop body must not be visited (the live map would` |
|      - |  415 | `					 * grow under the cursor). By-ref foreach keeps the live` |
|      - |  416 | `					 * map, like php. On OOM fall back to the live map. */` |
|      5 |  417 | `					ph7_hashmap *pSnap = PH7_NewHashmap(&(*pVm),0,0);` |
|      5 |  418 | `					if( pSnap && PH7_HashmapDupMaterialized(pMap,pSnap) == SXRET_OK ){` |
|      - |  419 | `						/* The step consumes the snapshot's initial reference */` |
|      5 |  420 | `						pIterMap = pSnap;` |
|      2 |  421 | `					}else if( pSnap ){` |
|    ! 0 |  422 | `						PH7_HashmapUnref(pSnap);` |
|    ! 0 |  423 | `					}` |
|      2 |  424 | `				}` |
|  31607 |  425 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|  31607 |  426 | `				pStep->xIter.pMap = pIterMap;` |
|  31607 |  427 | `				if( pIterMap == pMap ){` |
|  31603 |  428 | `					pMap->iRef++;` |
|  15799 |  429 | `				}` |
|      - |  430 | `				/* Private cursor + registry (php: nested foreach over one` |
|      - |  431 | `				 * array are independent; foreach never moves the internal` |
|      - |  432 | `				 * pointer — see PH7_HashmapRegisterForeachStep) */` |
|  31607 |  433 | `				PH7_HashmapRegisterForeachStep(pIterMap,pStep);` |
|  15806 |  434 | `			}else{` |
|    551 |  435 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|      - |  436 | `				ph7_class *pIteratorClass;` |
|      - |  437 | `				/* Check if the object implements Iterator */` |
|    551 |  438 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|    703 |  439 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|      - |  440 | `					/* Iterator-based iteration: call rewind() */` |
|      - |  441 | `					ph7_class_method *pRewind;` |
|    319 |  442 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|    319 |  443 | `					pStep->xIter.pThis = pThis;` |
|    319 |  444 | `					pThis->iRef++;` |
|    319 |  445 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|    319 |  446 | `					if( pRewind ){` |
|    319 |  447 | `						rc = PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|    319 |  448 | `						if( VmIterCallThrew(rc) ){` |
|      - |  449 | `							/* rewind() threw (a generator body or userland Iterator):` |
|      - |  450 | `							 * undo this step's retain, drop the step, and route the` |
|      - |  451 | `							 * exception instead of silently starting the loop. */` |
|     14 |  452 | `							pThis->iRef--;` |
|     14 |  453 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     14 |  454 | `							pStep = 0;` |
|     14 |  455 | `							PH7_DISPATCH_ITER_RC(rc,1)` |
|    ! 0 |  456 | `						}` |
|    152 |  457 | `					}` |
|    157 |  458 | `				}else{` |
|      - |  459 | `					/* Check if the object implements IteratorAggregate */` |
|      - |  460 | `					ph7_class *pIterAggClass;` |
|    237 |  461 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|      - |  462 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|    323 |  463 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|      - |  464 | `						/* Call getIterator() and use the returned Iterator object */` |
|      - |  465 | `						ph7_class_method *pGetIter;` |
|    176 |  466 | `						int iterAggOk = 0;` |
|    176 |  467 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|    176 |  468 | `						if( pGetIter ){` |
|      - |  469 | `							ph7_value sResult;` |
|    176 |  470 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|    176 |  471 | `							rc = PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|    176 |  472 | `							if( VmIterCallThrew(rc) ){` |
|      - |  473 | `								/* getIterator() threw: drop the step and route the` |
|      - |  474 | `								 * exception (don't pile the "must implement Iterator"` |
|      - |  475 | `								 * error on top of it). */` |
|    ! 0 |  476 | `								PH7_MemObjRelease(&sResult);` |
|    ! 0 |  477 | `								SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    ! 0 |  478 | `								pStep = 0;` |
|    ! 0 |  479 | `								PH7_DISPATCH_ITER_RC(rc,1)` |
|    ! 0 |  480 | `							}` |
|    176 |  481 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|    176 |  482 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|    176 |  483 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|      - |  484 | `									ph7_class_method *pRewind;` |
|    176 |  485 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|    176 |  486 | `									pStep->xIter.pThis = pIterObj;` |
|    176 |  487 | `									pIterObj->iRef++;` |
|      - |  488 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|    176 |  489 | `									pStep->pOwner = pThis;` |
|    176 |  490 | `									pThis->iRef++;` |
|    176 |  491 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|    176 |  492 | `									if( pRewind ){` |
|    176 |  493 | `										rc = PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|    176 |  494 | `										if( VmIterCallThrew(rc) ){` |
|      - |  495 | `											/* The aggregate's iterator rewind() threw: undo` |
|      - |  496 | `											 * both retains, drop the step, route the exception. */` |
|    ! 0 |  497 | `											pIterObj->iRef--;` |
|    ! 0 |  498 | `											pThis->iRef--;` |
|    ! 0 |  499 | `											PH7_MemObjRelease(&sResult);` |
|    ! 0 |  500 | `											SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    ! 0 |  501 | `											pStep = 0;` |
|    ! 0 |  502 | `											PH7_DISPATCH_ITER_RC(rc,1)` |
|    ! 0 |  503 | `										}` |
|     86 |  504 | `									}` |
|    176 |  505 | `									iterAggOk = 1;` |
|     86 |  506 | `								}` |
|     86 |  507 | `							}` |
|    176 |  508 | `							PH7_MemObjRelease(&sResult);` |
|     86 |  509 | `						}` |
|    176 |  510 | `						if( !iterAggOk ){` |
|      - |  511 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|    ! 0 |  512 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|      - |  513 | `								"Object returned by getIterator() must implement Iterator");` |
|    ! 0 |  514 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    ! 0 |  515 | `							pStep = 0; /* Signal: do not store this step */` |
|    ! 0 |  516 | `							pc = pInstr->iP2 - 1;` |
|    ! 0 |  517 | `						}` |
|     90 |  518 | `					}else{` |
|      - |  519 | `						/* Plain object iteration via hAttr */` |
|     65 |  520 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|     65 |  521 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|     65 |  522 | `						pStep->xIter.pThis = pThis;` |
|     65 |  523 | `						pThis->iRef++;` |
|      - |  524 | `					}` |
|      - |  525 | `				}` |
|      - |  526 | `			}` |
|      - |  527 | `		}` |
|  32143 |  528 | `		if( pStep ){` |
|  32143 |  529 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|    ! 0 |  530 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|    ! 0 |  531 | `				if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|    ! 0 |  532 | `					VmForeachHashmapStepRelease(&(*pVm),pInfo,pStep,FALSE/*never made it onto aStep*/);` |
|    ! 0 |  533 | `				}else{` |
|    ! 0 |  534 | `					SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      - |  535 | `				}` |
|      - |  536 | `				/* Jump out of the loop */` |
|    ! 0 |  537 | `				pc = pInstr->iP2 - 1;` |
|    ! 0 |  538 | `			}` |
|  16069 |  539 | `		}` |
|      - |  540 | `	}` |
|  32149 |  541 | `	VmPopOperand(&pTos,1);` |
|  32149 |  542 | `	VM_EXIT_BREAK;` |
|    ! 0 |  543 | `	VM_EXIT_BREAK;` |
|  16082 |  544 | `}` |
|      - |  545 |  |
