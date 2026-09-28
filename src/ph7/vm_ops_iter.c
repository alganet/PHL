/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
/*
 * Section:
 *    Iteration opcode handlers extracted from vm.c's dispatch loop. Each
 *    handler runs one opcode arm against the caller's VmExecState: the loop
 *    syncs pTos/pc in, calls the handler, reloads them and routes the
 *    returned VmOpRc onto its labels (same idiom as VmCallFinish).
 * Status:
 *    Stable.
 */
/* Bind the dispatch-routing exits to handler semantics (see vm_dispatch.h),
 * and map the macros' sState references onto our state parameter. */
#define VM_EXIT_BREAK      { pState->pTos = pTos; pState->pc = pc; return VM_OP_NEXT; }
#define VM_EXIT_ABORT      { pState->pTos = pTos; pState->pc = pc; return VM_OP_ABORT; }
#define VM_EXIT_EXCEPTION  { pState->pTos = pTos; pState->pc = pc; return VM_OP_EXCEPTION; }
#include "vm_dispatch.h"
#define sState (*pState)

/*
 * OP_FOREACH_STEP: advance one foreach iteration (hashmap cursor, Iterator
 * protocol, or object-attribute walk). Body moved verbatim from the
 * OP_FOREACH_STEP arm of VmByteCodeExecBody; arm-terminal breaks became
 * VM_EXIT_BREAK.
 */
PH7_PRIVATE VmOpRc VmExecOpForeachStep(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)
{
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;
	ph7_foreach_step **apStep,*pStep;
	ph7_value *pValue;
	VmFrame *pFrameLocal;
	sxu32 nStep;
	pFrameLocal = pVm->pFrame;
	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);
	/* Select THIS activation's step. aStep is per-STATEMENT and shared by every
	 * activation, so peeking the last entry resumes onto a sibling's cursor when
	 * two instances of one generator/fiber are suspended in the same textual
	 * foreach. Scan from the top (most-recent push) for the step whose owning
	 * frame matches the running activation; top-down makes the current push win
	 * over any leaked older step that happens to share a recycled frame address. */
	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);
	nStep = SySetUsed(&pInfo->aStep);
	if( nStep < 1 ){
		/* Defensive: OP_FOREACH_INIT always pushes this activation's step before
		 * STEP runs (and jumps past the loop when the push fails), so an empty
		 * set is unreachable — guard the apStep[-1] read anyway. Jump out. */
		pc = pInstr->iP2 - 1;
		VM_EXIT_BREAK;
	}
	pStep = apStep[nStep - 1];
	while( nStep > 0 ){
		if( apStep[nStep - 1]->pFrame == pFrameLocal ){
			pStep = apStep[nStep - 1];
			break;
		}
		nStep--;
	}
	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){
		ph7_hashmap_node *pNode;
		/* Extract the current node via this loop's PRIVATE cursor (php:
		 * nested foreach over the same array are independent iterations) */
		pNode = pStep->pCursor;
		if( pNode == 0 ){
			/* No more entry to process */
			pc = pInstr->iP2 - 1; /* Jump to this destination */
			/* php does NOT break the binding: the value variable stays a reference to the
			 * LAST element after the loop — that is what makes a second `foreach ($a as $v)`
			 * write through it (the famous gotcha), and what the `unset($v)` idiom exists to
			 * undo. Deleting the name here left $v undefined instead. */
			/* Cleanup the mess left behind */
			VmForeachHashmapStepRelease(&(*pVm),pInfo,pStep,TRUE);
		}else{
			/* Advance the private cursor */
			pStep->pCursor = pNode->pPrev; /* Reverse link */
			/* Bind the VALUE before the KEY: on the first iteration this is where
			 * both locals are created in the frame table, and php's symbol table
			 * lists the value ahead of the key (get_defined_vars() order). Only the
			 * creation ORDER matters here; the stored values are independent. */
			if( pStep->iFlags & PH7_4EACH_STEP_REF ){
				/* Pass by reference — a REGISTERED binding (PH7_VmBindVarSlot), so the element
				 * counts the loop variable as a holder for as long as it is bound, exactly as
				 * php's reference does. */
				PH7_VmBindVarSlot(&(*pVm),pFrameLocal,SyStringData(&pInfo->sValue),
					SyStringLength(&pInfo->sValue),pNode->nValIdx);
			}else{
				/* Make a copy of the entry value */
				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);
				if( pValue ){
					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);
				}
			}
			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){
				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);
				if( pKey ){
					PH7_HashmapExtractNodeKey(pNode,pKey);
				}
			}
		}
	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){
		/* Iterator-based iteration.
		 * Sequence: on first call just check valid/current/key.
		 * On subsequent calls, advance with next() first, then check.
		 */
		ph7_class_instance *pThis = pStep->xIter.pThis;
		ph7_class_method *pMethod;
		ph7_value sResult;
		int isValid = 0;
		/* Call next() to advance — but skip on the first iteration */
		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){
			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;
		}else{
			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);
			if( pMethod ){
				rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);
				if( VmIterCallThrew(rc) ){
					/* next() threw (generator body / userland Iterator): tear the
					 * step down like exhaustion does, then route the exception —
					 * the loop must not silently end with execution continuing. */
					VmForeachStepAbandon(pVm,pInfo,pStep,pThis);
					PH7_DISPATCH_ITER_RC(rc,0)
				}
			}
		}
		/* Call valid() */
		PH7_MemObjInit(pVm,&sResult);
		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);
		if( pMethod ){
			rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);
			if( VmIterCallThrew(rc) ){
				/* valid() threw: same teardown-and-route as next() above. */
				PH7_MemObjRelease(&sResult);
				VmForeachStepAbandon(pVm,pInfo,pStep,pThis);
				PH7_DISPATCH_ITER_RC(rc,0)
			}
			PH7_MemObjToBool(&sResult);
			isValid = (sResult.x.iVal != 0);
		}
		PH7_MemObjRelease(&sResult);
		if( !isValid ){
			/* Iterator exhausted */
			pc = pInstr->iP2 - 1;
			/* Release the aggregate owner if this was an IteratorAggregate foreach */
			VmForeachStepAbandon(pVm,pInfo,pStep,pThis);
		}else{
			/* Call current() to get value */
			PH7_MemObjInit(pVm,&sResult);
			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);
			if( pMethod ){
				rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);
				if( VmIterCallThrew(rc) ){
					/* current() threw: same teardown-and-route as next() above. */
					PH7_MemObjRelease(&sResult);
					VmForeachStepAbandon(pVm,pInfo,pStep,pThis);
					PH7_DISPATCH_ITER_RC(rc,0)
				}
			}
			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);
			if( pValue ){
				PH7_MemObjStore(&sResult,pValue);
			}
			PH7_MemObjRelease(&sResult);
			/* Call key() if needed */
			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){
				ph7_value sKey;
				PH7_MemObjInit(pVm,&sKey);
				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);
				if( pMethod ){
					rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);
					if( VmIterCallThrew(rc) ){
						/* key() threw: same teardown-and-route as next() above. */
						PH7_MemObjRelease(&sKey);
						VmForeachStepAbandon(pVm,pInfo,pStep,pThis);
						PH7_DISPATCH_ITER_RC(rc,0)
					}
				}
				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);
				if( pValue ){
					PH7_MemObjStore(&sKey,pValue);
				}
				PH7_MemObjRelease(&sKey);
			}
		}
	}else{
		ph7_class_instance *pThis = pStep->xIter.pThis;
		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */
		SyHashEntry *pEntry;
		/* Point to the next attribute (this loop's own cursor) */
		while((pEntry = PH7_ClassInstanceIterNext(&pStep->sAttrIter)) != 0 ){
			pVmAttr = (VmClassAttr *)pEntry->pUserData;
			if( PH7_ATTR_UNPRESENTED(pVmAttr) ){
				/* A static property belongs to the CLASS, never to an object: php
				 * iterates only the instance's own properties. PHL's instance
				 * attribute table carries an entry for every declared member
				 * (statics share the class slot), so it has to filter here — the
				 * same test var_dump/get_object_vars/json/serialize already make. */
				continue;
			}
			if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET|PH7_CLASS_ATTR_HOOK_VIRTUAL))
			 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){
				continue; /* virtual set-only property: iteration skips it (php) */
			}
			if( PH7_ClassAttrUninitializedForRead(pVmAttr) ){
				continue; /* typed, never written: not there yet (php) */
			}
			/* Check access permission */
			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,
				pVmAttr->pAttr->iProtection,FALSE) ){
					break; /* Access is granted */
			}
		}
		if( pEntry == 0 ){
			/* Clean up the mess left behind */
			pc = pInstr->iP2 - 1; /* Jump to this destination */
			/* The binding survives the loop (see the hashmap step) */
			PH7_ClassInstanceIterClose(pThis,&pStep->sAttrIter);
			VmForeachStepUnlink(pInfo,pStep);
			SyMemBackendPoolFree(&pVm->sAllocator,pStep);
			PH7_ClassInstanceUnref(pThis);
		}else{
			SyString *pAttrName = &pVmAttr->pAttr->sName;
			ph7_value *pAttrValue;
			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){
				/* Fill with the current attribute name. A MANGLED name — only the
				 * __PHP_Incomplete_Class carrier stores those — yields its plain
				 * part: php's iterator unmangles the key it hands out. */
				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);
				if( pKey ){
					SyString sUnmCls, sUnmName;
					SyStringInitFromBuf(&sUnmName,pAttrName->zString,pAttrName->nByte);
					if( pAttrName->nByte > 0 && pAttrName->zString[0] == 0 ){
						PH7_UnmangleAttrName(pAttrName->zString,pAttrName->nByte,&sUnmCls,&sUnmName);
					}
					SyBlobReset(&pKey->sBlob);
					SyBlobAppend(&pKey->sBlob,sUnmName.zString,sUnmName.nByte);
					MemObjSetType(pKey,MEMOBJ_STRING);
				}
			}
			if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET|PH7_CLASS_ATTR_HOOK_SET))
			 && (pStep->iFlags & PH7_4EACH_STEP_REF)
			 && !VmHookGuardHeld(pVm,(void *)pThis,&pVmAttr->pAttr->sName) ){
				/* php: a hooked property (virtual or backed) cannot be iterated
				 * by reference — catchable Error. Tear the step down like the
				 * exhausted-iteration path (break the by-ref binding, unlink,
				 * free, drop the instance retain) so nothing leaks and a
				 * re-entered foreach starts fresh; the fetch-point router lands
				 * the parked throw right after this op. Inside the property's
				 * own hook body the guard keeps raw semantics (no Error). */
				SyBlob sErrMsg;
				SyBlobInit(&sErrMsg,&pVm->sAllocator);
				SyBlobFormat(&sErrMsg,"Cannot create reference to property %z::$%z",
					&pThis->pClass->sName,&pVmAttr->pAttr->sName);
				VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));
				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);
				PH7_ClassInstanceIterClose(pThis,&pStep->sAttrIter);
				VmForeachStepUnlink(pInfo,pStep);
				SyMemBackendPoolFree(&pVm->sAllocator,pStep);
				PH7_ClassInstanceUnref(pThis);
				VM_EXIT_BREAK;
			}
			if( (pStep->iFlags & PH7_4EACH_STEP_REF) == 0
			 && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET) != 0 ){
				/* PHP 8.4 property hooks: object iteration reads through the
				 * get hook (virtual properties included; the flag gate keeps
				 * hook-free classes on the raw zero-copy path below). The
				 * step carries its own registered cursor, so a hook that
				 * re-enters an hAttr walk on this instance (get_object_vars,
				 * json_encode of $this) cannot truncate THIS iteration, and one
				 * that unset()s the property the cursor is parked on has that
				 * cursor advanced under it. */
				ph7_value sHookVal;
				sxi32 rcHk;
				PH7_MemObjInit(pVm,&sHookVal);
				rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);
				if( rcHk != SXERR_NOTFOUND ){
					if( rcHk == SXRET_OK ){
						pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);
						if( pValue ){
							PH7_MemObjStore(&sHookVal,pValue);
						}
					}
					/* a throw parked on the boundary rail: the fetch-point
					 * router lands it right after this op */
					PH7_MemObjRelease(&sHookVal);
					VM_EXIT_BREAK;
				}
				PH7_MemObjRelease(&sHookVal);
			}
			/* Extract attribute value */
			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);
			if( pAttrValue ){
				if( pStep->iFlags & PH7_4EACH_STEP_REF ){
					/* Pass by reference (registered — see the hashmap step) */
					PH7_VmBindVarSlot(&(*pVm),pFrameLocal,SyStringData(&pInfo->sValue),
						SyStringLength(&pInfo->sValue),pVmAttr->nIdx);
				}else{
					/* Make a copy of the attribute value */
					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);
					if( pValue ){
						PH7_MemObjStore(pAttrValue,pValue);
					}
				}
			}
		}
	}
	VM_EXIT_BREAK;
}

/*
 * OP_FOREACH_INIT: body moved verbatim from the OP_FOREACH_INIT arm of
 * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.
 */
PH7_PRIVATE VmOpRc VmExecOpForeachInit(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)
{
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;
	void *pName;
#ifdef UNTRUST
	if( pTos < pStack ){
		VM_EXIT_ABORT;
	}
#endif
	if( SyStringLength(&pInfo->sValue) < 1 ){
		/* Take the variable name from the top of the stack */
		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){
			/* Force a string cast */
			PH7_MemObjToString(pTos);
		}
		/* Duplicate name */
		if( SyBlobLength(&pTos->sBlob) > 0 ){
			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));
			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));
		}
		VmPopOperand(&pTos,1);
	}
	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){
		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){
			/* Force a string cast */
			PH7_MemObjToString(pTos);
		}
		/* Duplicate name */
		if( SyBlobLength(&pTos->sBlob) > 0 ){
			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));
			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));
		}
		VmPopOperand(&pTos,1);
	}
	if( (pInfo->iFlags & PH7_4EACH_STEP_REF) && (pTos->iFlags & MEMOBJ_AUX_STROFFSET) ){
		/* `foreach ($s[0] as &$v)`: the subject is a string OFFSET and the loop wants
		 * to ALIAS its elements. php screens that before it asks whether the subject is
		 * iterable at all, and a string offset is never a reference — same Error the
		 * `=&` and by-ref-argument paths raise, not the not-iterable warning PHL
		 * answered (whose slot index is the BASE STRING's, so binding would have
		 * aliased the whole string). */
		sxi32 rcSo = VmThrowFromVm(&(*pVm),"Error","Cannot create references to/from string offsets",
			sizeof("Cannot create references to/from string offsets")-1);
		if( rcSo == SXERR_ABORT ){ VM_EXIT_ABORT; }
		PH7_THROW_ROUTE_MIDEXPR(rcSo)
	}
	if( (pInfo->iFlags & PH7_4EACH_STEP_REF) && SyStringLength(&pInfo->sValue) > 0 ){
		/* `foreach ($o->p as &$v)` on a property no write may reach: the loop hands
		 * out an ALIAS of every element, so php screens the property where it
		 * screens a store -- and it screens it BEFORE deciding the subject is
		 * iterable at all, which is why an int or a string property is this Error
		 * there and a "must be of type array|object" warning here. PHL walked an
		 * array one and let the body rewrite it. */
		sxi32 rcInd = PH7_VmCheckIndirectModify(&(*pVm),pTos->nIdx);
		if( rcInd != SXRET_OK ){
			if( rcInd == PH7_ABORT ){ VM_EXIT_ABORT; }
			PH7_THROW_ROUTE_MIDEXPR(rcInd)
		}
	}
	/* Make sure we are dealing with a hashmap aka 'array' or an object */
	if( (pTos->iFlags & (MEMOBJ_HASHMAP|MEMOBJ_OBJ)) == 0 || SyStringLength(&pInfo->sValue) < 1 ){
		/* Jump out of the loop */
		if( SyStringLength(&pInfo->sValue) > 0 ){
			/* php warns for EVERY non-iterable, null included (PH7 exempted null). */
			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,
				"foreach() argument must be of type array|object, %s given",
				VmArithValueName(pTos));
		}
		pc = pInstr->iP2 - 1;
	}else{
		ph7_foreach_step *pStep;
		VmFrame *pInitFrame = VmSkipExceptionFrames(pVm->pFrame);
		/* Reclaim this activation's LEFTOVER step for this same foreach statement.
		 * A loop left through break/return/goto/an exception never reaches the
		 * "no more entries" arm that frees its step, so the step, its retain of
		 * the subject and — for an object walk — its registered cursor all
		 * survived until the VM died. The running frame cannot be inside that
		 * loop's body while it is executing INIT, so the step is stale by
		 * construction. */
		{
			ph7_foreach_step **apOld = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);
			sxu32 nOld = SySetUsed(&pInfo->aStep);
			while( nOld > 0 ){
				ph7_foreach_step *pOld = apOld[--nOld];
				if( pOld->pFrame == pInitFrame ){
					VmForeachStepRelease(&(*pVm),pInfo,pOld);
					/* The set shifted under us: restart the scan. */
					apOld = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);
					nOld = SySetUsed(&pInfo->aStep);
				}
			}
		}
		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));
		if( pStep == 0 ){
			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");
			/* Jump out of the loop */
			pc = pInstr->iP2 - 1;
		}else{
			/* Zero the structure */
			SyZero(pStep,sizeof(ph7_foreach_step));
			/* Prepare the step */
			pStep->iFlags = pInfo->iFlags;
			/* Record the owning activation so OP_FOREACH_STEP can pick THIS
			 * activation's step out of the per-statement stack — two suspended
			 * generator/fiber instances (or a recursive call) paused in the same
			 * textual foreach otherwise resume onto each other's cursor. */
			pStep->pFrame = pInitFrame;
			if( pTos->iFlags & MEMOBJ_HASHMAP ){
				ph7_hashmap *pMap,*pIterMap;
				/* COW: For by-reference foreach, eagerly separate the
				 * source array so mutations don't affect other sharers. */
				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){
					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);
					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){
						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;
						/* Only adjust refcounts/separate if the backing
						 * variable still points at the same hashmap as
						 * the stack value. */
						if( pBacking->x.pOther == (void *)pCur ){
							pCur->iRef--;
							/* Use the returned map, not pBacking->x.pOther: PH7_HashmapDup
							 * inside CowSeparate can reallocate (move) pVm->aMemObj and leave
							 * pBacking dangling. The return value is the post-separation map. */
							pTos->x.pOther = PH7_HashmapCowSeparate(&(*pVm),pBacking);
							((ph7_hashmap *)pTos->x.pOther)->iRef++;
						}
					}
				}
				pMap = (ph7_hashmap *)pTos->x.pOther;
				pIterMap = pMap;
				if( pMap == pVm->pGlobal && (pStep->iFlags & PH7_4EACH_STEP_REF) == 0 ){
					/* php 8.1: foreach ($GLOBALS as ...) by value iterates a
					 * SNAPSHOT of the symbol table — globals created inside
					 * the loop body must not be visited (the live map would
					 * grow under the cursor). By-ref foreach keeps the live
					 * map, like php. On OOM fall back to the live map. */
					ph7_hashmap *pSnap = PH7_NewHashmap(&(*pVm),0,0);
					if( pSnap && PH7_HashmapDupMaterialized(pMap,pSnap) == SXRET_OK ){
						/* The step consumes the snapshot's initial reference */
						pIterMap = pSnap;
					}else if( pSnap ){
						PH7_HashmapUnref(pSnap);
					}
				}
				pStep->iFlags |= PH7_4EACH_STEP_HASHMAP;
				pStep->xIter.pMap = pIterMap;
				if( pIterMap == pMap ){
					pMap->iRef++;
				}
				/* Private cursor + registry (php: nested foreach over one
				 * array are independent; foreach never moves the internal
				 * pointer — see PH7_HashmapRegisterForeachStep) */
				PH7_HashmapRegisterForeachStep(pIterMap,pStep);
			}else{
				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;
				ph7_class *pIteratorClass;
				/* Check if the object implements Iterator */
				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);
				if( PH7_VmGeneratorIsClosed(&(*pVm),pThis) ){
					/* php refuses to START a foreach over a generator that has already
					 * run to its end, and says so BEFORE the rewind that would report
					 * the coarser "already run". PHL walked an EMPTY loop instead, so a
					 * second foreach over the same generator silently did nothing. */
					SyMemBackendPoolFree(&pVm->sAllocator,pStep);
					pStep = 0;
					rc = VmThrowFromVm(&(*pVm),"Exception",
						"Cannot traverse an already closed generator",
						(sxu32)sizeof("Cannot traverse an already closed generator")-1);
					PH7_DISPATCH_ITER_RC(rc,1)
				}else if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){
					/* Iterator-based iteration: call rewind() */
					ph7_class_method *pRewind;
					pStep->iFlags |= PH7_4EACH_STEP_ITERATOR|PH7_4EACH_STEP_FIRST;
					pStep->xIter.pThis = pThis;
					pThis->iRef++;
					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);
					if( pRewind ){
						rc = PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);
						if( VmIterCallThrew(rc) ){
							/* rewind() threw (a generator body or userland Iterator):
							 * undo this step's retain, drop the step, and route the
							 * exception instead of silently starting the loop. */
							pThis->iRef--;
							SyMemBackendPoolFree(&pVm->sAllocator,pStep);
							pStep = 0;
							PH7_DISPATCH_ITER_RC(rc,1)
						}
					}
				}else{
					/* Check if the object implements IteratorAggregate */
					ph7_class *pIterAggClass;
					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",
						sizeof("IteratorAggregate")-1,FALSE,0);
					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){
						/* Call getIterator() and use the returned Iterator object */
						ph7_class_method *pGetIter;
						int iterAggOk = 0;
						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);
						if( pGetIter ){
							ph7_value sResult;
							PH7_MemObjInit(&(*pVm),&sResult);
							rc = PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);
							if( VmIterCallThrew(rc) ){
								/* getIterator() threw: drop the step and route the
								 * exception (don't pile the "must implement Iterator"
								 * error on top of it). */
								PH7_MemObjRelease(&sResult);
								SyMemBackendPoolFree(&pVm->sAllocator,pStep);
								pStep = 0;
								PH7_DISPATCH_ITER_RC(rc,1)
							}
							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){
								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;
								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){
									ph7_class_method *pRewind;
									pStep->iFlags |= PH7_4EACH_STEP_ITERATOR|PH7_4EACH_STEP_FIRST;
									pStep->xIter.pThis = pIterObj;
									pIterObj->iRef++;
									/* Retain the aggregate so it lives for the duration of the foreach */
									pStep->pOwner = pThis;
									pThis->iRef++;
									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);
									if( pRewind ){
										rc = PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);
										if( VmIterCallThrew(rc) ){
											/* The aggregate's iterator rewind() threw: undo
											 * both retains, drop the step, route the exception. */
											pIterObj->iRef--;
											pThis->iRef--;
											PH7_MemObjRelease(&sResult);
											SyMemBackendPoolFree(&pVm->sAllocator,pStep);
											pStep = 0;
											PH7_DISPATCH_ITER_RC(rc,1)
										}
									}
									iterAggOk = 1;
								}
							}
							PH7_MemObjRelease(&sResult);
						}
						if( !iterAggOk ){
							/* getIterator() failed or returned non-Iterator: abort this foreach */
							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,
								"Object returned by getIterator() must implement Iterator");
							SyMemBackendPoolFree(&pVm->sAllocator,pStep);
							pStep = 0; /* Signal: do not store this step */
							pc = pInstr->iP2 - 1;
						}
					}else{
						/* Plain object iteration via hAttr. A PRIVATE cursor,
						 * registered on the instance -- the table's embedded one
						 * is shared, so a nested loop over the same object rewound
						 * this one (an infinite loop) and an unset() in the body
						 * freed the entry it was parked on. */
						pStep->iFlags |= PH7_4EACH_STEP_OBJECT;
						pStep->xIter.pThis = pThis;
						pThis->iRef++;
						PH7_ClassInstanceIterOpen(pThis,&pStep->sAttrIter);
					}
				}
			}
		}
		if( pStep ){
			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){
				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");
				if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){
					VmForeachHashmapStepRelease(&(*pVm),pInfo,pStep,FALSE/*never made it onto aStep*/);
				}else{
					if( pStep->iFlags & PH7_4EACH_STEP_OBJECT ){
						/* Unhook the cursor before the pool slot is recycled: a
						 * registered walker left behind is what the next unset()
						 * on this object would step through. */
						PH7_ClassInstanceIterClose(pStep->xIter.pThis,&pStep->sAttrIter);
					}
					SyMemBackendPoolFree(&pVm->sAllocator,pStep);
				}
				/* Jump out of the loop */
				pc = pInstr->iP2 - 1;
			}
		}
	}
	VmPopOperand(&pTos,1);
	VM_EXIT_BREAK;
	VM_EXIT_BREAK;
}
