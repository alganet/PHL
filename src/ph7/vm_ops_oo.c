/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
/*
 * Section:
 *    Opcode handlers extracted from vm.c's dispatch loop. Each handler runs
 *    one opcode arm against the caller's VmExecState: the loop syncs pTos/pc
 *    in, calls the handler, reloads them and routes the returned VmOpRc onto
 *    its labels (same idiom as VmCallFinish).
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
 * OP_CLONE_APPLY: body moved verbatim from the OP_CLONE_APPLY arm of
 * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.
 */
PH7_PRIVATE VmOpRc VmExecOpCloneApply(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)
{
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	SXUNUSED(pInstr);
	SXUNUSED(pStack);
	ph7_value *pUpdates,*pObj;
	ph7_class_instance *pClone;
	ph7_hashmap *pMap;
	ph7_hashmap_node *pNode;
	sxi32 rcApply = SXRET_OK;
	sxu32 n;
#ifdef UNTRUST
	if( pTos < &pStack[1] ){
		VM_EXIT_ABORT;
	}
#endif
	pUpdates = pTos;
	pObj = &pTos[-1];
	/* $withProperties must be an array (PHP: TypeError otherwise). */
	if( (pUpdates->iFlags & MEMOBJ_HASHMAP) == 0 ){
		SyBlob sMsg;
		SyBlobInit(&sMsg,&pVm->sAllocator);
		SyBlobFormat(&sMsg,"clone(): Argument #2 ($withProperties) must be of type array, %s given",
			ph7_type_name(pUpdates));
		rc = VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);
		if( rc == PH7_ABORT ){
			VM_EXIT_ABORT;
		}
		/* Pop the (bad) updates argument and dispatch to the nearest catch. */
		VmPopOperand(&pTos,1);
		{
			sxi32 iRp;
			if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){
				pc = iRp;
				VM_EXIT_BREAK;
			}
		}
		VM_EXIT_EXCEPTION;
	}
	/* The clone must be an object; OP_CLONE leaves NULL only on a prior failure
	 * (already reported) — in that case just drop the updates and carry the NULL. */
	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 ){
		VmPopOperand(&pTos,1);
		VM_EXIT_BREAK;
	}
	pClone = (ph7_class_instance *)pObj->x.pOther;
	pMap = (ph7_hashmap *)pUpdates->x.pOther;
	/* Apply each update in insertion order (pFirst -> pPrev is the forward link). */
	pNode = pMap->pFirst;
	for( n = pMap->nEntry ; n > 0 && rcApply == SXRET_OK ; --n ){
		ph7_value *pVal;
		ph7_value sVal;
		const char *zName;
		sxu32 nName;
		char zKeyBuf[64];
		if( pNode == 0 ){
			break;
		}
		pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);
		if( pNode->iType == HASHMAP_INT_NODE ){
			/* An int key becomes the property name (PHP: `$5`). */
			nName = SyBufferFormat(zKeyBuf,sizeof(zKeyBuf),"%qd",pNode->xKey.iKey);
			zName = zKeyBuf;
		}else{
			zName = (const char *)SyBlobData(&pNode->xKey.sKey);
			nName = SyBlobLength(&pNode->xKey.sKey);
		}
		if( pVal ){
			/* Snapshot the update value into a stack local FIRST: applying it may
			 * create a dynamic property, whose slot reservation can reallocate
			 * pVm->aMemObj and dangle pVal (a pointer into it). The name is safe
			 * (it lives in the node's key blob / zKeyBuf, not in aMemObj). */
			PH7_MemObjInit(pVm,&sVal);
			PH7_MemObjLoad(pVal,&sVal);
			rcApply = VmCloneApplyUpdate(pVm,pClone,zName,nName,&sVal);
			PH7_MemObjRelease(&sVal);
		}
		pNode = pNode->pPrev;
	}
	if( rcApply == PH7_ABORT ){
		VM_EXIT_ABORT;
	}
	if( rcApply == PH7_EXCEPTION ){
		/* An update threw (visibility / readonly / type). Pop the updates array
		 * and hand control to the nearest catch, else propagate out of the loop. */
		VmPopOperand(&pTos,1);
		{
			sxi32 iRp;
			if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){
				pc = iRp;
				VM_EXIT_BREAK;
			}
		}
		VM_EXIT_EXCEPTION;
	}
	/* Success: drop the updates array, leaving the clone on the stack. */
	VmPopOperand(&pTos,1);
	VM_EXIT_BREAK;
	VM_EXIT_BREAK;
}

/*
 * OP_NEW: body moved verbatim from the OP_NEW arm of
 * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.
 */
PH7_PRIVATE VmOpRc VmExecOpNew(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)
{
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	/* Constructor arg count: compile-time args plus THIS new's own unpack
	 * expansion (iP2 = hasSpread, transferred from the popped OP_CALL —
	 * `new C(...$args)` used to ignore the extras, leaving the expanded
	 * elements ABOVE the class-name slot and fataling "Class ' ' is not
	 * defined"). VmSpreadOwnExtra counts only this new's own runs, so a nested
	 * spread call in the ctor arg list stays scoped to itself. */
	sxi32 nCtorArgs = pInstr->iP1 + (pInstr->iP2 ? VmSpreadOwnExtra(pVm,pInstr->iP1,pTos) : 0);
	ph7_value *pArg;
	ph7_class *pClass = 0;
	ph7_class_instance *pNew;
	pArg = &pTos[-nCtorArgs]; /* Constructor arguments (if available) */
	/* Same PHP 8.1 spread-key realignment as OP_CALL: build a per-actual-slot name
	 * map when the ctor arg list unpacked string-keyed elements. NEW keeps the
	 * class-name slot at pTos (above the args), so pArg is already the correct base;
	 * the build also truncates this call's captured runs. */
	VmCallArgMap sEffNewMap;
	VmCallArgMap *pEffNewMap = VmEffCallArgMap(pVm,pInstr,pArg,
		nCtorArgs > 0 ? (sxu32)nCtorArgs : 0,&sEffNewMap);
	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){
		const char *zCls = (const char *)SyBlobData(&pTos->sBlob);
		sxu32 nCls = SyBlobLength(&pTos->sBlob);
		if( (nCls == sizeof("self")-1 && SyMemcmp(zCls,"self",sizeof("self")-1) == 0)
		 || (nCls == sizeof("static")-1 && SyMemcmp(zCls,"static",sizeof("static")-1) == 0)
		 || (nCls == sizeof("parent")-1 && SyMemcmp(zCls,"parent",sizeof("parent")-1) == 0) ){
			/* new self() / new static() / new parent(): resolve against the live
			 * class context (LSB for static), sharing the FCC resolver. */
			pClass = VmFccResolveScope(&(*pVm),pTos);
			if( pClass && (pClass->iFlags & (PH7_CLASS_INTERFACE|PH7_CLASS_ABSTRACT)) ){
				/* Not new-able (the named path's iLoadable extract excludes these
				 * before it ever gets here): php's wording, with the RESOLVED name. */
				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot instantiate %s %z",
					(pClass->iFlags & PH7_CLASS_INTERFACE) ? "interface" : "abstract class",
					&pClass->sName);
				PH7_MemObjRelease(pTos);
				if( nCtorArgs > 0 ){
					VmPopOperand(&pTos,nCtorArgs);
				}
				VM_EXIT_ABORT;
			}
		}else{
			/* Try to extract the desired class */
			pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,
				TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);
		}
	}else if( pTos->iFlags & MEMOBJ_OBJ ){
		/* Take the base class from the loaded instance */
		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;
	}
	if( pClass == 0 ){
		/* php: `new NoSuch()` is a CATCHABLE Error ('Class "NoSuch" not found'), not a
		 * hard stop. Aborting here killed the script outright -- and with exit status 0,
		 * so a caller could not even tell it had failed. */
		SyBlob sErrM;
		sxi32 rcErr;
		ph7_class *pNotNew = 0;
		SyBlobInit(&sErrM,&pVm->sAllocator);
		if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){
			/* The extract above only accepts NEW-able classes, so an interface or an
			 * abstract class comes back as 0 and used to be reported as "not found".
			 * Look again without that filter so php's real message can be given. */
			pNotNew = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),
				SyBlobLength(&pTos->sBlob),FALSE,0);
		}
		if( pNotNew && (pNotNew->iFlags & (PH7_CLASS_INTERFACE|PH7_CLASS_ABSTRACT)) ){
			SyBlobFormat(&sErrM,"Cannot instantiate %s %z",
				(pNotNew->iFlags & PH7_CLASS_INTERFACE) ? "interface" : "abstract class",
				&pNotNew->sName);
		}else{
			SyBlobFormat(&sErrM,"Class \"%.*s\" not found",
				SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob));
		}
		/* Settle the stack BEFORE throwing: the class-name slot becomes the (NULL)
		 * expression result and the ctor arguments go. */
		if( nCtorArgs > 0 ){
			VmPopOperand(&pTos,nCtorArgs);
		}
		PH7_MemObjRelease(pTos);
		MemObjSetType(pTos,MEMOBJ_NULL);
		pTos->nIdx = SXU32_HIGH;
		rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),
			SyBlobLength(&sErrM));
		SyBlobRelease(&sErrM);
		if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }
		rc = rcErr;
		PH7_THROW_ROUTE_MIDEXPR(rc)
	}else if( pClass->iFlags & PH7_CLASS_ENUM ){
		/* php 8.1: enums cannot be instantiated — a catchable Error, raised
		 * BEFORE any construction (no instance, no __destruct). */
		SyBlob sErrMsg;
		SyBlobInit(&sErrMsg,&pVm->sAllocator);
		SyBlobFormat(&sErrMsg,"Cannot instantiate enum %z",&pClass->sName);
		VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));
		if( nCtorArgs > 0 ){
			VmPopOperand(&pTos,nCtorArgs);
		}
		PH7_MemObjRelease(pTos);
		pTos->nIdx = SXU32_HIGH;
		VM_EXIT_BREAK;
	}else{
		ph7_class_method *pCons;
		/* Check if a constructor is available — BEFORE instantiation: a
		 * visibility-denied `new` must not construct (nor later destruct)
		 * the object (band A #4). */
		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);
		if( pCons == 0 ){
			SyString *pName = &pClass->sName;
			/* Check for a constructor with the same base class name */
			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);
		}
		/* Constructor visibility (band A #4): __construct now KEEPS its
		 * declared protection (PH7_NewClassMethod no longer forces it
		 * public), so `new C()` on a private/protected constructor from
		 * the wrong scope is php's catchable Error. Reflection's
		 * newInstance path sets bReflectBypass like method invoke. */
		if( pCons && pCons->iProtection != PH7_CLASS_PROT_PUBLIC ){
			/* php binds non-public access by the member's DECLARING class, not the
			 * instantiated class: a private constructor declared in a base is
			 * reachable from that base's own methods even when instantiating a
			 * subclass (`new Child()` inside Base::factory()). __construct is a
			 * method, so pass its declaring class (sFunc.pUserData) — mirroring the
			 * method-call visibility check — rather than pClass, whose hAttr lookup
			 * for a method name always misses and falls to a wrong exact-class test. */
			ph7_class *pCtorDecl = pCons->sFunc.pUserData ? (ph7_class *)pCons->sFunc.pUserData : pClass;
			if( pVm->bReflectBypass ){
				pVm->bReflectBypass = 0;
			}else if( !PH7_VmClassMemberAccess(&(*pVm),pCtorDecl,&pCons->sFunc.sName,pCons->iProtection,FALSE) ){
				SyBlob sErrMsg;
				const char *zVis = pCons->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";
				SyBlobInit(&sErrMsg,&pVm->sAllocator);
				SyBlobFormat(&sErrMsg,"Call to %s %z::__construct() from global scope",
					zVis,&pClass->sName);
				VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));
				/* Pop ctor args + release the class-name operand, leave NULL
				 * as the expression value; the fetch-point router lands the
				 * throw. No instance was created. */
				if( nCtorArgs > 0 ){
					VmPopOperand(&pTos,nCtorArgs);
				}
				PH7_MemObjRelease(pTos);
				pTos->nIdx = SXU32_HIGH;
				VM_EXIT_BREAK;
			}
		}
		/* Create a new class instance */
		pNew = PH7_NewClassInstance(&(*pVm),pClass);
		if( pNew == 0 ){
			VmErrorFormat(&(*pVm),PH7_CTX_ERR,
				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",
				&pClass->sName
			);
			PH7_MemObjRelease(pTos);
			if( nCtorArgs > 0 ){
				/* Pop given arguments */
				VmPopOperand(&pTos,nCtorArgs);
			}
			VM_EXIT_BREAK;
		}
		if( pCons ){
			/* Call the class constructor.  Collect args in stack order and
			 * forward any VmCallArgMap from the NEW instruction so the
			 * receiving OP_CALL path runs its named-argument matching
			 * (including variadic string-key packing). */
			VmCallArgMap *pNewMap = pEffNewMap;
			sxi32 rcCons;
			SySet aArg; /* ctor argument scratch (the loop's shared set stays with OP_CALL) */
			SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));
			while( pArg < pTos ){
				SySetPut(&aArg,(const void *)&pArg);
				pArg++;
			}
			/* Too-few-arguments is php's catchable ArgumentCountError, raised
			 * by the shared OP_CALL install path this ctor call routes through
			 * (was a PHL-only notice here). */
			rcCons = VmCallClassMethodWithMap(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pNewMap);
			SySetRelease(&aArg); /* scratch dies here, on every exit path */
			/* TICKET 1433-52: Unsetting $this in the constructor body */
			if( pNew->iRef < 1 ){
				pNew->iRef = 1;
			}
			if( rcCons == PH7_ABORT || rcCons == PH7_EXCEPTION ){
				/* The constructor raised: the half-constructed object must not
				 * become the NEW result. Drop our reference so it is destroyed.
				 * The class-name operand (and any leftover args) are released by
				 * the Abort/Exception unwind, or explicitly on the resume path. */
				sxi32 iResumePc;
				PH7_ClassInstanceUnref(pNew);
				if( rcCons == PH7_ABORT ){
					VM_EXIT_ABORT;
				}
				if( VmRecordedResume(pVm,&iResumePc,pState->pEntryFrame,aInstr) ){
					/* This frame's own try caught it in-place: tidy the stack
					 * (pop ctor args + release the class-name slot) and resume. */
					if( nCtorArgs > 0 ){
						VmPopOperand(&pTos,nCtorArgs);
					}
					PH7_MemObjRelease(pTos);
					pc = iResumePc;
					VM_EXIT_BREAK;
				}
				VM_EXIT_EXCEPTION;
			}
		}
		if( nCtorArgs > 0 ){
			/* Pop given arguments */
			VmPopOperand(&pTos,nCtorArgs);
		}
		PH7_MemObjRelease(pTos);
		pTos->x.pOther = pNew;
		MemObjSetType(pTos,MEMOBJ_OBJ);
	}
	VM_EXIT_BREAK;
	VM_EXIT_BREAK;
}

/*
 * OP_MEMBER: body moved verbatim from the OP_MEMBER arm of
 * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.
 */
PH7_PRIVATE VmOpRc VmExecOpMember(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)
{
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	ph7_class_instance *pThis;
	ph7_value *pNos;
	SyString sName;
	if( !pInstr->iP1 ){
		pNos = &pTos[-1];
#ifdef UNTRUST
		if( pNos < pStack ){
			VM_EXIT_ABORT;
		}
#endif
		if( pInstr->iP2 == PH7_MEMBER_DEFPATH
		 && (pNos->iFlags & (MEMOBJ_AUX_DEFPATH|MEMOBJ_AUX_DEFERRED)) ){
			/* D1 commit 2: deferred property arg ($o->p) whose base is itself a deferred lvalue
			 * (a nested chain `$a["k"]->p`, or an undefined base object). Append a property step
			 * to the base's captured path; OP_CALL re-walks it. */
			SyString sProp;
			VmDeferredPath *pPath = 0;
			SyStringInitFromBuf(&sProp,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));
			if( pNos->iFlags & MEMOBJ_AUX_DEFPATH ){
				pPath = (VmDeferredPath *)pNos->x.pOther;
				VmDeferPathPushProp(pPath,&sProp);
			}else{
				SyString sRootName;
				SyStringInitFromBuf(&sRootName,(const char *)pNos->x.pOther,
					pNos->x.pOther ? SyStrlen((const char *)pNos->x.pOther) : 0);
				pPath = VmDeferPathNew(&(*pVm),1,SXU32_HIGH,&sRootName);
				if( pPath ){
					pNos->iFlags &= ~MEMOBJ_AUX_DEFERRED; /* borrowed name */
					pNos->x.pOther = pPath;
					pNos->iFlags |= MEMOBJ_AUX_DEFPATH;
					pNos->nIdx = SXU32_HIGH;
					VmDeferPathPushProp(pPath,&sProp);
				}
			}
			VmPopOperand(&pTos,1); /* drop the property name; the carrier stays as pTos */
			VM_EXIT_BREAK;
		}
		if( pNos->iFlags & MEMOBJ_OBJ ){
			ph7_class *pClass;
			/* Class already instantiated */
			pThis = (ph7_class_instance *)pNos->x.pOther;
			/* Point to the instantiated class */
			pClass = pThis->pClass;
			/* Extract attribute name first */
			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));
			if( pInstr->iP2 == PH7_MEMBER_METHOD ){
				/* Method call */
				ph7_class_method *pMeth = 0;
				if( sName.nByte > 0 ){
					/* Extract the target method */
					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);
				}
				if( pMeth == 0 ){
					ph7_class_method *pCallMagic = PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1);
					if( pCallMagic ){
						/* php: a missing method dispatches __call($name, $args) — the
						 * args are only collected by the following OP_CALL, so stash the
						 * receiver + class + original name and redirect the callee to
						 * the hidden packing trampoline (band A #3b; pre-fix the name-
						 * only call discarded everything and the call site failed with
						 * "Invalid function name"). Stack: pop the method name, then
						 * the receiver slot becomes the trampoline's callee name. */
						SyBlobReset(&pVm->sMagicCallName);
						SyBlobAppend(&pVm->sMagicCallName,(const void *)sName.zString,sName.nByte);
						pThis->iRef++;
						pVm->pMagicCallThis = pThis;
						pVm->pMagicCallClass = pClass;
						VmPopOperand(&pTos,1);
						PH7_MemObjRelease(pTos);
						SyBlobAppend(&pTos->sBlob,"__phl_magic_call",sizeof("__phl_magic_call")-1);
						MemObjSetType(pTos,MEMOBJ_STRING);
					}else{
						{
							/* php raises a catchable Error here; PH7 printed a notice and CARRIED ON
							 * with NULL, so the call silently produced nothing. */
							SyBlob sErrM;
							sxi32 rcErr;
							SyBlobInit(&sErrM,&pVm->sAllocator);
							SyBlobFormat(&sErrM,"Call to undefined method %z::%z()",&pClass->sName,&sName);
							VmPopOperand(&pTos,1);
							PH7_MemObjRelease(pTos);
							pTos->nIdx = SXU32_HIGH;
							rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),
								SyBlobLength(&sErrM));
							SyBlobRelease(&sErrM);
							if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }
							rc = rcErr;
							PH7_THROW_ROUTE_MIDEXPR(rc)
						}
					}
				}else{
					ph7_class_method *pDeniedCall = 0;
					if( pMeth->iProtection != PH7_CLASS_PROT_PUBLIC
					 && !PH7_VmClassMemberAccess(&(*pVm),
						pMeth->sFunc.pUserData ? (ph7_class *)pMeth->sFunc.pUserData : pClass,
						&sName,pMeth->iProtection,FALSE) ){
						int bRebound = 0;
						if( pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ){
							/* php: when the instance's class SHADOWS the caller's
							 * private method (child redeclares private m), code in
							 * the caller's class dispatches its OWN private m, not
							 * the shadow. Rebind to the calling scope's method when
							 * that scope declares a private one of this name. */
							VmFrame *pFrameLocal = VmSkipExceptionFrames(pVm->pFrame);
							ph7_vm_func *pCallerFunc = (ph7_vm_func *)pFrameLocal->pUserData;
							if( pCallerFunc && (pCallerFunc->iFlags & VM_FUNC_CLASS_METHOD) && pCallerFunc->pUserData ){
								ph7_class *pScope = (ph7_class *)pCallerFunc->pUserData;
								ph7_class_method *pOwn = PH7_ClassExtractMethod(pScope,sName.zString,sName.nByte);
								if( pOwn && pOwn->iProtection == PH7_CLASS_PROT_PRIVATE
								 && (ph7_class *)pOwn->sFunc.pUserData == pScope ){
									pMeth = pOwn;
									bRebound = 1;
								}
							}
						}
						if( !bRebound ){
							/* Inaccessible from this scope: php routes through __call when
							 * declared (band A #3b); without it OP_CALL raises its
							 * "Call to private/protected method" Error as before. */
							pDeniedCall = PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1);
						}
					}
					if( pDeniedCall ){
						SyBlobReset(&pVm->sMagicCallName);
						SyBlobAppend(&pVm->sMagicCallName,(const void *)sName.zString,sName.nByte);
						pThis->iRef++;
						pVm->pMagicCallThis = pThis;
						pVm->pMagicCallClass = pClass;
						VmPopOperand(&pTos,1);
						PH7_MemObjRelease(pTos);
						SyBlobAppend(&pTos->sBlob,"__phl_magic_call",sizeof("__phl_magic_call")-1);
						MemObjSetType(pTos,MEMOBJ_STRING);
					}else{
						/* Push method name on the stack */
						PH7_MemObjRelease(pTos);
						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));
						MemObjSetType(pTos,MEMOBJ_STRING);
					}
				}
				pTos->nIdx = SXU32_HIGH;
			}else{
				/* Attribute access. iP2: 0 = read, 2 = unset, 3 = isset, 4 = empty. */
				VmClassAttr *pObjAttr = 0;
				SyHashEntry *pEntry = 0;
				/* Extract the target attribute */
				if( sName.nByte > 0 ){
					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);
					if( pEntry ){
						/* Point to the attribute value */
						pObjAttr = (VmClassAttr *)pEntry->pUserData;
					}
				}
				if( pInstr->iP2 == PH7_MEMBER_UNSET ){
					/* unset($o->prop): remove the property entirely so it disappears from
					 * foreach / json_encode / get_object_vars / (array) — matching PHP (a value-only
					 * release would leave a zombie null entry). Leave a NULL constant on the stack so
					 * the trailing generic unset() builtin is a no-op (mirrors LOAD_IDX iP2=5).
					 * php dispatches __unset($name) for a MISSING or INACCESSIBLE property
					 * (band A #3b — pre-fix an inaccessible private was silently DELETED from
					 * outside the class); without __unset, an inaccessible unset is php's
					 * catchable "Cannot access ..." Error and a missing one stays a no-op. */
					int bUnsAccessible = pEntry ? PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) : 0;
					if( pEntry && (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET|PH7_CLASS_ATTR_HOOK_SET)) != 0 ){
						/* php 8.4: a hooked property (virtual or backed) can never be
						 * unset — catchable Error, even from inside its own hook body
						 * (probe-verified). */
						SyBlob sErrMsg;
						SyBlobInit(&sErrMsg,&pVm->sAllocator);
						SyBlobFormat(&sErrMsg,"Cannot unset hooked property %z::$%z",
							&pThis->pClass->sName,&pObjAttr->pAttr->sName);
						VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));
					}else if( pEntry && bUnsAccessible ){
						PH7_VmReleaseInstanceAttr(&(*pVm),pObjAttr);
						SyHashDeleteEntry2(pEntry);
					}else{
						ph7_class_method *pUnsetMagic = PH7_ClassExtractMethod(pClass,"__unset",sizeof("__unset")-1);
						if( pUnsetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'u') ){
							VmMagicGuardPush(pVm,(void *)pThis,&sName,'u');
							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__unset",sizeof("__unset")-1,&sName,0);
							VmMagicGuardPop(pVm);
						}else if( pEntry ){
							/* Inaccessible and no __unset: php's catchable Error. Parked on
							 * the boundary rail; the op completes benignly and the
							 * fetch-point router lands it. */
							SyBlob sErrMsg;
							const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";
							SyBlobInit(&sErrMsg,&pVm->sAllocator);
							SyBlobFormat(&sErrMsg,"Cannot access %s property %z::$%z",zVis,&pClass->sName,&sName);
							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));
						}
						/* Missing property without __unset: silent no-op (php). */
					}
					VmPopOperand(&pTos,1);    /* pop the attribute name */
					PH7_MemObjRelease(pTos);  /* release the object on the stack ($o's stack ref) */
					pTos->nIdx = SXU32_HIGH;  /* NULL constant */
					VM_EXIT_BREAK;
				}
				if( pObjAttr == 0 && sName.nByte > 0 ){
					/* Member not present on the instance and the next instruction writes/modifies it
					 * (store, array-append/keyed-write, `??=`, ++/--, or a compound-assign — see
					 * VmMemberNextIsWrite; the compiler always emits a terminating PH7_OP_DONE so
					 * pInstr+1 is in-bounds). Create the property so the operation lands on a real slot
					 * (PHP auto-vivifies a fresh property for all these forms, not just `=`):
					 *   - a DECLARED prop that was unset() and is re-assigned → recreate it
					 *     (PHP re-appends it at the end), OR
					 *   - a dynamic prop on a dynamic-allowing class (stdClass).
					 * The created slot is NULL with a real nIdx, which the modify-op then vivifies
					 * (NULL→array for [], NULL→0/"" for ++/.=) and writes back in place.
					 * Two signals: the compiler tags the member itself iP2=PH7_MEMBER_WRITE when it is
					 * the base of a write-subscript / `??=` (the modify-op is not the immediately-next
					 * instruction there), and for ++/--/compound-assign/store the next opcode is the
					 * modify-op directly (VmMemberNextIsWrite). */
					VmInstr *pNext = pInstr + 1;
					if( pInstr->iP2 == PH7_MEMBER_WRITE || VmMemberNextIsWrite(pNext) ){
						ph7_class_attr *pDecl = PH7_ClassExtractAttribute(pThis->pClass,sName.zString,sName.nByte);
						if( pDecl && (pDecl->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT)) == 0 ){
							VmRecreateDeclaredAttr(&(*pVm),pThis,pDecl,&pObjAttr);
						}else{
							/* php 8 semantics (band A #3b): a PLAIN store to a missing
							 * property dispatches __set($name,$value) when declared —
							 * the value only exists at the following OP_STORE, so park
							 * the receiver+name for it (one-instruction lifetime; the
							 * guard makes a same-name write inside __set fall through
							 * to dynamic creation, like php). Without __set — or for a
							 * subscript-write base / read-modify-write, which php does
							 * NOT route through __set — create a dynamic property on
							 * ANY class with the 8.2 deprecation (suppressed for
							 * stdClass and #[AllowDynamicProperties]); a readonly
							 * class raises php's catchable Error instead. */
							ph7_class_method *pSetMagic = 0;
							ph7_class_method *pCoalIsset = 0, *pCoalGet = 0, *pCoalSet = 0;
							int bPlainStore = (pNext->iOp == PH7_OP_STORE && pNext->iP2 != 0);
							if( bPlainStore ){
								pSetMagic = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1);
							}
							if( pSetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'s') ){
								pThis->iRef++;
								pVm->pMagicSetThis = pThis;
								SyBlobReset(&pVm->sMagicSetName);
								SyBlobAppend(&pVm->sMagicSetName,(const void *)sName.zString,sName.nByte);
								/* pObjAttr stays NULL; the miss path below stays silent. */
							}else if( !bPlainStore && pInstr->iP2 == PH7_MEMBER_WRITE
							 && pNext->iOp == PH7_OP_NULLC_JMP
							 && ((pCoalIsset = PH7_ClassExtractMethod(pClass,"__isset",sizeof("__isset")-1)) != 0
							  || (pCoalGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1)) != 0
							  || (pCoalSet = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1)) != 0) ){
								/* `$o->p ??= v` on a missing property with magic accessors:
								 * php consults __isset first when declared — false means
								 * assign directly through __set with NO __get call (the
								 * test value stays null); true (or no __isset) means __get
								 * provides the test value. A COAL_MAGIC entry is pushed
								 * for the OP_NULLC_STORE at the jump target; the
								 * fetch-point sweep drops it when the short-circuit jump
								 * skips the assign or a throw abandons the RHS. Without
								 * __set the assign side keeps the pre-existing loud
								 * "Cannot perform assignment" path (php would create a
								 * dynamic property there — recorded residual). Note the
								 * condition's short-circuit assignment chain: a hit on an
								 * earlier method leaves the later pointers unresolved, so
								 * re-resolve the leftovers here (each is one hash probe,
								 * only on this ??=-miss path). */
								ph7_value sTest;
								int bMiss = 0; /* __isset said false: skip __get, test value stays null */
								if( pCoalIsset != 0 ){
									pCoalGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);
								}
								if( pCoalIsset != 0 || pCoalGet != 0 ){
									pCoalSet = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1);
								}
								PH7_MemObjInit(pVm,&sTest);
								if( pCoalIsset && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'i') ){
									ph7_value sIssetRet;
									PH7_MemObjInit(pVm,&sIssetRet);
									VmMagicGuardPush(pVm,(void *)pThis,&sName,'i');
									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__isset",sizeof("__isset")-1,&sName,&sIssetRet);
									VmMagicGuardPop(pVm);
									PH7_MemObjToBool(&sIssetRet);
									bMiss = sIssetRet.x.iVal == 0;
									PH7_MemObjRelease(&sIssetRet);
								}
								if( !bMiss && pCoalGet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){
									VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');
									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sTest);
									VmMagicGuardPop(pVm);
								}
								if( pCoalSet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'s')
								 && pVm->nBoundaryRc == 0 ){
									VmHookRmw sPend;
									sPend.iKind = VM_HOOK_PEND_COAL_MAGIC;
									sPend.pThis = pThis;
									sPend.pAttr = 0;
									sPend.nBackIdx = SXU32_HIGH;
									sPend.nScratchIdx = SXU32_HIGH;
									SyBlobInit(&sPend.sName,&pVm->sAllocator);
									SyBlobAppend(&sPend.sName,(const void *)sName.zString,sName.nByte);
									sPend.pOwnerStack = (void *)pStack;
									sPend.pInstrs = (void *)aInstr;
									sPend.nJmpPc = (sxu32)(pc + 1);        /* the OP_NULLC_JMP */
									sPend.nPc = (sxu32)(pNext->iP2 - 1);   /* the OP_NULLC_STORE */
									pThis->iRef++;
									SySetPut(&pVm->aHookRmw,(const void *)&sPend);
								}
								/* Pop the attribute name; the test value becomes the
								 * expression slot (a temp, not an lvalue). */
								VmPopOperand(&pTos,1);
								pThis->iRef++;
								PH7_MemObjRelease(pTos);
								PH7_MemObjStore(&sTest,pTos);
								pTos->nIdx = SXU32_HIGH;
								PH7_MemObjRelease(&sTest);
								PH7_ClassInstanceUnref(pThis);
								VM_EXIT_BREAK;
							}else if( !bPlainStore && pInstr->iP2 == PH7_MEMBER_WRITE
							 && !VmMemberNextIsWrite(pNext)
							 && PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1) ){
								/* Subscript-write base on a class with __get: php reads
								 * through the magic layer (the write lands on the temp and
								 * is lost, like php's indirect-modification case). A PLAIN
								 * store (bPlainStore — the compiler tags those
								 * PH7_MEMBER_WRITE too) is NOT this case: it falls through
								 * to dynamic creation. A direct ++/--/compound-assign
								 * (VmMemberNextIsWrite — the compiler now tags those
								 * PH7_MEMBER_WRITE as well) is NOT this case either: it
								 * falls through to dynamic creation like before (the
								 * recorded RMW-vivifies-instead-of-__get residual, §7).
								 * Leave the miss path — the read gate below dispatches
								 * __get. */
							}else if( pThis->pClass->iFlags & PH7_CLASS_READONLY ){
								SyBlob sErrMsg;
								SyBlobInit(&sErrMsg,&pVm->sAllocator);
								SyBlobFormat(&sErrMsg,"Cannot create dynamic property %z::$%z",
									&pThis->pClass->sName,&sName);
								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));
							}else if( !VmClassAllowsDynamicProps(pVm,pThis->pClass)
							       && !VmClassHasAttributeNamed(pThis->pClass,"AllowDynamicProperties",sizeof("AllowDynamicProperties")-1) ){
								/* php 8.2 only DEPRECATES creating a dynamic property on a
								 * class without #[AllowDynamicProperties]; PHL rejects it.
								 * stdClass / __set / declared props are unaffected. */
								SyBlob sErrMsg;
								SyBlobInit(&sErrMsg,&pVm->sAllocator);
								SyBlobFormat(&sErrMsg,"Cannot create dynamic property %z::$%z",
									&pThis->pClass->sName,&sName);
								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));
							}else{
								PH7_VmCreateDynamicAttr(&(*pVm),pThis,sName.zString,sName.nByte,&pObjAttr);
							}
						}
					}
				}
				if( pObjAttr == 0 && pInstr->iP2 == PH7_MEMBER_DEFPATH && pNos->nIdx != SXU32_HIGH ){
					/* D1 commit 2: deferred property arg, property absent on a REACHABLE object
					 * base ($o is a real slot). Record a property step rooted at $o so OP_CALL can
					 * vivify+bind (by-ref) or read via __get / warn (by-value). A magic __get/__set
					 * class is recorded the same way; the resolve step emits php's Notice for the
					 * by-ref case and dispatches __get for by-value. */
					SyString sProp;
					VmDeferredPath *pPath = VmDeferPathNew(&(*pVm),0,pNos->nIdx,0);
					SyStringInitFromBuf(&sProp,sName.zString,sName.nByte);
					if( pPath && VmDeferPathPushProp(pPath,&sProp) == SXRET_OK ){
						VmPopOperand(&pTos,1);       /* drop the property name */
						pThis->iRef++;
						PH7_MemObjRelease(pTos);     /* collapse the object slot into the carrier */
						pTos->x.pOther = pPath;
						pTos->iFlags = MEMOBJ_NULL | MEMOBJ_AUX_DEFPATH;
						pTos->nIdx = SXU32_HIGH;
						PH7_ClassInstanceUnref(pThis);
						VM_EXIT_BREAK;
					}
					if( pPath ){
						VmFreeDeferredPath(pPath);
					}
					/* fall through to the normal miss handling on allocation failure */
				}
				if( pObjAttr == 0 ){
					/* Missing property. On a plain READ, php dispatches __get($name) and the
					 * expression takes its RETURN VALUE (band A #3a — pre-fix the result was
					 * discarded and a PHL-native warn fired even when __get existed). isset/
					 * empty context consults __isset first (then __get for empty()'s value
					 * test); a plain-store write context parked a pending __set above. A
					 * self-recursive read of the same property falls back to the
					 * undefined-property path via the guard, like php's property guard. */
					ph7_class_method *pGetMagic = 0;
					if( VmMemberCtxIsLookup(pInstr->iP2) ){
						ph7_class_method *pIssetMagic = PH7_ClassExtractMethod(pClass,"__isset",sizeof("__isset")-1);
						if( pIssetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'i') ){
							ph7_value sIssetRet;
							int bSet;
							PH7_MemObjInit(pVm,&sIssetRet);
							VmMagicGuardPush(pVm,(void *)pThis,&sName,'i');
							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__isset",sizeof("__isset")-1,&sName,&sIssetRet);
							VmMagicGuardPop(pVm);
							PH7_MemObjToBool(&sIssetRet);
							bSet = sIssetRet.x.iVal != 0;
							PH7_MemObjRelease(&sIssetRet);
							if( bSet && pInstr->iP2 == PH7_MEMBER_EMPTY ){
								/* empty(): __isset said set — fetch the value via __get
								 * (php) so emptiness is judged on the real value. */
								ph7_class_method *pEmptyGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);
								ph7_value sEmptyVal;
								PH7_MemObjInit(pVm,&sEmptyVal);
								if( pEmptyGet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){
									VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');
									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sEmptyVal);
									VmMagicGuardPop(pVm);
								}
								VmPopOperand(&pTos,1);
								pThis->iRef++;
								PH7_MemObjRelease(pTos);
								PH7_MemObjStore(&sEmptyVal,pTos);
								pTos->nIdx = SXU32_HIGH;
								PH7_MemObjRelease(&sEmptyVal);
								PH7_ClassInstanceUnref(pThis);
								VM_EXIT_BREAK;
							}
							/* isset(): the truth of __isset IS the answer — push a non-null
							 * marker for true, NULL for false (isset only tests null-ness). */
							VmPopOperand(&pTos,1);
							pThis->iRef++;
							PH7_MemObjRelease(pTos);
							if( bSet ){
								pTos->x.iVal = 1;
								MemObjSetType(pTos,MEMOBJ_BOOL);
							}
							pTos->nIdx = SXU32_HIGH;
							PH7_ClassInstanceUnref(pThis);
							VM_EXIT_BREAK;
						}
					}
					if( !VmMemberCtxIsLookup(pInstr->iP2) && !VmMemberNextIsWrite(pInstr + 1) ){
						/* Plain reads AND the ??=/subscript write-base (PH7_MEMBER_WRITE,
						 * which php reads through __get); read-modify-write forms are
						 * excluded (they vivified above — approximate, recorded). */
						pGetMagic = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);
					}
					if( pGetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){
						ph7_value sMagicRet;
						PH7_MemObjInit(pVm,&sMagicRet);
						VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');
						PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sMagicRet);
						VmMagicGuardPop(pVm);
						/* Pop the attribute name, replace the object slot with the magic
						 * result (a temp, not an lvalue — nIdx stays constant). A throw
						 * from __get parked at the boundary; the fetch-point router lands
						 * it and abandons this slot. */
						VmPopOperand(&pTos,1);
						pThis->iRef++;
						PH7_MemObjRelease(pTos);
						PH7_MemObjStore(&sMagicRet,pTos);
						pTos->nIdx = SXU32_HIGH;
						PH7_MemObjRelease(&sMagicRet);
						PH7_ClassInstanceUnref(pThis);
						VM_EXIT_BREAK;
					}
					/* No __get (or guard held): load null. In isset()/empty() context (iP2 3/4)
					 * PHP returns false/true SILENTLY, so suppress the read-miss warning there
					 * (mirrors the array LOAD_IDX iP2=4/6 suppression); likewise when a
					 * pending __set was parked above (the following OP_STORE dispatches it —
					 * nothing is "undefined" about that write) or a throw is parked on the
					 * boundary rail (e.g. the readonly-class dynamic-property Error — the
					 * fetch-point router lands it right after this op). */
					if( !VmMemberCtxIsLookup(pInstr->iP2) && pVm->pMagicSetThis == 0
					 && pVm->nBoundaryRc == 0 ){
						VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined property: %z::$%z",
							&pClass->sName,&sName);
					}
				}
				VmPopOperand(&pTos,1);
				/* TICKET 1433-49: Deffer garbage collection until attribute loading.
				 * This is due to the following case:
				 *     (new TestClass())->foo;
				 */
				pThis->iRef++;
				PH7_MemObjRelease(pTos);
				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */
				if( pInstr->iP2 == PH7_MEMBER_REF_TARGET ){
					/* `$o->p =& $x`: stash the resolved instance property slot for the
					 * following member-marked OP_STORE_REF, which rebinds it to alias the
					 * source variable. Do NOT run the read/hook/magic/uninit machinery
					 * below — a reference bind neither reads the value nor triggers
					 * get/set hooks or an uninitialized-typed Error. pThis stays retained
					 * (the iRef++ above); OP_STORE_REF releases it. */
					if( pObjAttr ){
						pVm->pRefTargetAttr = pObjAttr;
						pVm->pRefTargetThis = pThis;
						pVm->pRefTargetStaticAttr = 0;
						pTos->nIdx = pObjAttr->nIdx;
					}else{
						/* Missing/inaccessible target: leave nothing stashed; OP_STORE_REF
						 * no-ops. Balance the retain. */
						pVm->pRefTargetAttr = 0;
						pVm->pRefTargetThis = 0;
						pVm->pRefTargetStaticAttr = 0;
						PH7_ClassInstanceUnref(pThis);
					}
					VM_EXIT_BREAK;
				}
				if( pObjAttr ){
					ph7_value *pValue = 0; /* cc warning */
					/* Check attribute access */
					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) ){
						if( (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET|PH7_CLASS_ATTR_HOOK_SET))
						 && !VmHookGuardHeld(pVm,(void *)pThis,&sName) ){
							/* PHP 8.4 property hooks: route reads, writes, and the
							 * read-modify-write forms through the synthesized hook
							 * methods. A held guard (either kind) means we are INSIDE
							 * one of this property's own hook bodies — fall through to
							 * the raw backing slot for BOTH directions (php: `$this->x`
							 * within any of x's hooks addresses the backing store). */
							VmInstr *pNextH = pInstr + 1;
							int bPlainStore = (pNextH->iOp == PH7_OP_STORE && pNextH->iP2 != 0);
							int bCoalesceW = pInstr->iP2 == PH7_MEMBER_WRITE
								&& pNextH->iOp == PH7_OP_NULLC_JMP;
							/* ++/--/compound-assign: the modify op FOLLOWS the member
							 * directly (the compiler tags compound-assign members
							 * PH7_MEMBER_WRITE too, so classify by the next op, not iP2;
							 * the !bPlainStore term is load-bearing — VmMemberNextIsWrite
							 * returns 1 for a member OP_STORE too) */
							int bRmwNext = !bPlainStore && VmMemberNextIsWrite(pNextH);
							int bSubscriptW = !bPlainStore && !bCoalesceW && !bRmwNext
								&& pInstr->iP2 == PH7_MEMBER_WRITE;
							if( bPlainStore ){
								/* Arm the pending hook-set for the following OP_STORE — it
								 * dispatches the set hook, or throws the read-only Error
								 * when the property only has a get hook. (The scalar
								 * transient is safe here: its window is exactly one
								 * instruction, MEMBER -> STORE, nothing runs in between.) */
								pThis->iRef++;
								pVm->pHookSetThis = pThis;
								pVm->pHookSetAttr = pObjAttr->pAttr;
								pVm->nHookSetIdx = pObjAttr->nIdx;
								pTos->nIdx = SXU32_HIGH; /* sentinel slot for OP_STORE */
								PH7_ClassInstanceUnref(pThis);
								VM_EXIT_BREAK;
							}
							if( bSubscriptW ){
								/* Subscript write base ($o->m[] = v, $o->m[$k] = v):
								 * php's catchable Error, with or without a set hook
								 * (only a by-ref `&get` hook would allow it — a loud
								 * compile error in PHL). The op completes benignly with
								 * a null temp; the fetch-point router lands the throw. */
								SyBlob sErrMsg;
								SyBlobInit(&sErrMsg,&pVm->sAllocator);
								SyBlobFormat(&sErrMsg,"Indirect modification of %z::$%z is not allowed",
									&pThis->pClass->sName,&pObjAttr->pAttr->sName);
								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));
								PH7_ClassInstanceUnref(pThis);
								VM_EXIT_BREAK;
							}
							if( (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET|PH7_CLASS_ATTR_HOOK_VIRTUAL))
							 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){
								/* VIRTUAL set-only property: there is no backing store to
								 * fall back to, so EVERY read context — plain read,
								 * isset()/empty() (php throws there too, probe-verified),
								 * the ??= test, an RMW read — is php's catchable
								 * write-only Error. */
								SyBlob sErrMsg;
								SyBlobInit(&sErrMsg,&pVm->sAllocator);
								SyBlobFormat(&sErrMsg,"Property %z::$%z is write-only",
									&pThis->pClass->sName,&pObjAttr->pAttr->sName);
								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));
								PH7_ClassInstanceUnref(pThis);
								VM_EXIT_BREAK;
							}
							if( VmMemberCtxIsLookup(pInstr->iP2) ){
								/* isset()/empty() on a hooked property calls the get hook
								 * (php): isset is "get() !== null", empty tests the value. */
								ph7_value sHookRet;
								PH7_MemObjInit(pVm,&sHookRet);
								if( PH7_VmHookGetAttrValue(pThis,pObjAttr,&sHookRet) != SXERR_NOTFOUND ){
									if( pInstr->iP2 == PH7_MEMBER_ISSET ){
										pTos->x.iVal = (sHookRet.iFlags & MEMOBJ_NULL) == 0;
										MemObjSetType(pTos,MEMOBJ_BOOL);
									}else{
										/* empty(): hand the value to the truthiness test */
										PH7_MemObjStore(&sHookRet,pTos);
									}
									pTos->nIdx = SXU32_HIGH;
									PH7_MemObjRelease(&sHookRet);
									PH7_ClassInstanceUnref(pThis);
									VM_EXIT_BREAK;
								}
								PH7_MemObjRelease(&sHookRet);
							}
							if( !bCoalesceW && !bRmwNext && !VmMemberCtxIsLookup(pInstr->iP2) ){
								/* Read: dispatch __phl_hook_get_NAME (inheritance-aware) */
								ph7_value sHookRet;
								PH7_MemObjInit(pVm,&sHookRet);
								if( PH7_VmHookGetAttrValue(pThis,pObjAttr,&sHookRet) != SXERR_NOTFOUND ){
									PH7_MemObjStore(&sHookRet,pTos);
									pTos->nIdx = SXU32_HIGH;
									PH7_MemObjRelease(&sHookRet);
									PH7_ClassInstanceUnref(pThis);
									VM_EXIT_BREAK;
								}
								PH7_MemObjRelease(&sHookRet);
							}
							if( bCoalesceW || bRmwNext ){
								/* `$o->x ??= v` (bCoalesceW) and the read-modify-write
								 * forms `$o->x++`, `$o->x .= v`, … (bRmwNext): php reads
								 * through the get hook (raw backing when the property is
								 * set-only) and writes through the set hook.
								 *   - ??=: the test value goes on the stack and a
								 *     COAL_HOOK entry is pushed for the OP_NULLC_STORE
								 *     at the jump target; the fetch-point sweep drops it
								 *     when the short-circuit jump skips the assign or a
								 *     throw abandons the RHS (the entry is NOT a scalar
								 *     transient — nested stores/coalesces in the RHS
								 *     stack their own entries above it).
								 *   - RMW: the value goes into a fresh SCRATCH slot the
								 *     modify op mutates in place; the entry makes the
								 *     op's tail dispatch the set side with the computed
								 *     value (PH7_HOOK_RMW_WRITEBACK). */
								ph7_value sCur;
								sxi32 rcCur;
								PH7_MemObjInit(pVm,&sCur);
								rcCur = PH7_VmHookGetAttrValue(pThis,pObjAttr,&sCur);
								if( rcCur == SXERR_NOTFOUND ){
									/* set-only hook (or guard edge): the read side is the
									 * raw backing store (php) */
									ph7_value *pBack = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);
									if( pBack ){
										PH7_MemObjStore(pBack,&sCur);
									}
								}else if( pVm->nBoundaryRc != 0 ){
									/* the get hook threw: leave the null temp; the
									 * fetch-point router lands the parked throw —
									 * nothing is armed. */
									PH7_MemObjRelease(&sCur);
									PH7_ClassInstanceUnref(pThis);
									VM_EXIT_BREAK;
								}
								if( bCoalesceW ){
									VmHookRmw sPend;
									PH7_MemObjStore(&sCur,pTos);
									pTos->nIdx = SXU32_HIGH;
									PH7_MemObjRelease(&sCur);
									sPend.iKind = VM_HOOK_PEND_COAL_HOOK;
									sPend.pThis = pThis;
									sPend.pAttr = pObjAttr->pAttr;
									sPend.nBackIdx = pObjAttr->nIdx;
									sPend.nScratchIdx = SXU32_HIGH;
									SyBlobInit(&sPend.sName,&pVm->sAllocator);
									sPend.pOwnerStack = (void *)pStack;
									sPend.pInstrs = (void *)aInstr;
									sPend.nJmpPc = (sxu32)(pc + 1);            /* the OP_NULLC_JMP */
									sPend.nPc = (sxu32)(pNextH->iP2 - 1);      /* the OP_NULLC_STORE */
									pThis->iRef++;
									SySetPut(&pVm->aHookRmw,(const void *)&sPend);
									PH7_ClassInstanceUnref(pThis);
									VM_EXIT_BREAK;
								}
								{
									ph7_value *pScr = PH7_ReserveMemObj(&(*pVm));
									if( pScr ){
										VmHookRmw sRmw;
										PH7_MemObjStore(&sCur,pScr);
										PH7_MemObjStore(&sCur,pTos);
										pTos->nIdx = pScr->nIdx;
										sRmw.iKind = VM_HOOK_PEND_RMW;
										sRmw.pThis = pThis;
										sRmw.pAttr = pObjAttr->pAttr;
										sRmw.nBackIdx = pObjAttr->nIdx;
										sRmw.nScratchIdx = pScr->nIdx;
										SyBlobInit(&sRmw.sName,&pVm->sAllocator);
										sRmw.pOwnerStack = (void *)pStack;
										sRmw.pInstrs = (void *)aInstr;
										sRmw.nJmpPc = (sxu32)(pc + 1);  /* the modify op ... */
										sRmw.nPc = (sxu32)(pc + 1);     /* ... is the whole window */
										pThis->iRef++;
										SySetPut(&pVm->aHookRmw,(const void *)&sRmw);
									}
									/* OOM: pScr == 0 — leave the null temp (loud allocator
									 * diagnostics already fired) */
									PH7_MemObjRelease(&sCur);
									PH7_ClassInstanceUnref(pThis);
									VM_EXIT_BREAK;
								}
							}
						}
						/* PHP 7.4+: reading an uninitialized typed property is an Error.
						 * We can only raise it on a real read, not when the slot is the
						 * LHS of an assignment — peek at the next instruction to decide.
						 * Safe: the compiler always emits a terminating PH7_OP_DONE, so
						 * pInstr+1 is in-bounds while we are inside a non-DONE opcode. */
						if( (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)
						 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){
							VmInstr *pNext = pInstr + 1;
							int bIsLhs = 0;
							if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){
								bIsLhs = 1;
							}
							/* isset()/empty()/`??` read an uninitialized typed property
							 * as "not set" — a silent miss, NOT the Error a plain read
							 * raises (php). The compiler tags such an access iP2 =
							 * ISSET/EMPTY; treat it like the assignment-LHS case and fall
							 * through to load the slot's NULL. */
							if( VmMemberCtxIsLookup(pInstr->iP2) ){
								bIsLhs = 1;
							}
							if( !bIsLhs ){
								sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pObjAttr->pAttr);
								PH7_ClassInstanceUnref(pThis);
								if( rcU == PH7_ABORT ){
									VM_EXIT_ABORT;
								}
								{
									sxi32 iRp;
									if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){
										pc = iRp;
										VM_EXIT_BREAK;
									}
								}
								VM_EXIT_EXCEPTION;
							}
						}
						/* Load attribute */
						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);
						if( pValue ){
							if( pThis->iRef < 2 ){
								/* Perform a store operation,rather than a load operation since
								 * the class instance '$this' will be deleted shortly.
								 */
								PH7_MemObjStore(pValue,pTos);
							}else{
								/* Simple load */
								PH7_MemObjLoad(pValue,pTos);
							}
							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){
								if( pThis->iRef > 1 ){
									/* Load attribute index */
									pTos->nIdx = pObjAttr->nIdx;
								}
							}
						}
					}else{
						/* Inaccessible (private/protected) from this scope. php consults
						 * __get here exactly like a missing property (band A #3a) before
						 * erroring; the guard keeps a self-recursive read from looping. */
						ph7_class_method *pGetMagic = 0;
						if( pInstr->iP2 != PH7_MEMBER_WRITE && !VmMemberCtxIsLookup(pInstr->iP2)
						 && !VmMemberNextIsWrite(pInstr + 1) ){
							pGetMagic = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);
						}
						if( pGetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){
							ph7_value sMagicRet;
							PH7_MemObjInit(pVm,&sMagicRet);
							VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');
							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sMagicRet);
							VmMagicGuardPop(pVm);
							/* The name was already popped and pTos released above; just
							 * take the magic result as the expression value. */
							PH7_MemObjStore(&sMagicRet,pTos);
							pTos->nIdx = SXU32_HIGH;
							PH7_MemObjRelease(&sMagicRet);
							PH7_ClassInstanceUnref(pThis);
							VM_EXIT_BREAK;
						}
						if( VmMemberCtxIsLookup(pInstr->iP2) ){
							/* isset/empty on an inaccessible property: php consults __isset
							 * (band A #3b), and is silently false without it — never an
							 * Error (pre-fix PHL fataled here). */
							ph7_class_method *pIssetMagic = PH7_ClassExtractMethod(pClass,"__isset",sizeof("__isset")-1);
							int bSet = 0;
							if( pIssetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'i') ){
								ph7_value sIssetRet;
								PH7_MemObjInit(pVm,&sIssetRet);
								VmMagicGuardPush(pVm,(void *)pThis,&sName,'i');
								PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__isset",sizeof("__isset")-1,&sName,&sIssetRet);
								VmMagicGuardPop(pVm);
								PH7_MemObjToBool(&sIssetRet);
								bSet = sIssetRet.x.iVal != 0;
								PH7_MemObjRelease(&sIssetRet);
							}
							if( bSet ){
								if( pInstr->iP2 == PH7_MEMBER_EMPTY ){
									ph7_class_method *pEmptyGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);
									ph7_value sEmptyVal;
									PH7_MemObjInit(pVm,&sEmptyVal);
									if( pEmptyGet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){
										VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');
										PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sEmptyVal);
										VmMagicGuardPop(pVm);
									}
									PH7_MemObjStore(&sEmptyVal,pTos);
									pTos->nIdx = SXU32_HIGH;
									PH7_MemObjRelease(&sEmptyVal);
								}else{
									pTos->x.iVal = 1;
									MemObjSetType(pTos,MEMOBJ_BOOL);
									pTos->nIdx = SXU32_HIGH;
								}
							}
							PH7_ClassInstanceUnref(pThis);
							VM_EXIT_BREAK;
						}
						{
							/* A plain store to an inaccessible property dispatches __set
							 * (band A #3b): park the receiver+name for the following
							 * OP_STORE, exactly like the missing-property case. */
							VmInstr *pNextW = pInstr + 1;
							if( pNextW->iOp == PH7_OP_STORE && pNextW->iP2 != 0 ){
								ph7_class_method *pSetMagic = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1);
								if( pSetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'s') ){
									pThis->iRef++;
									pVm->pMagicSetThis = pThis;
									SyBlobReset(&pVm->sMagicSetName);
									SyBlobAppend(&pVm->sMagicSetName,(const void *)sName.zString,sName.nByte);
									pTos->nIdx = SXU32_HIGH; /* sentinel slot for OP_STORE */
									PH7_ClassInstanceUnref(pThis);
									VM_EXIT_BREAK;
								}
							}
						}
						if( ((pInstr + 1)->iOp == PH7_OP_NULLC || (pInstr + 1)->iOp == PH7_OP_NULLC_JMP)
						 && pInstr->iP2 != PH7_MEMBER_WRITE ){
							/* `$o->priv ?? default` (OP_NULLC; NULLC_JMP is the `??=`
							 * pre-test): php treats `??` as an isset-style lookup — an
							 * inaccessible property yields null SILENTLY (the
							 * __isset/__get consults already ran above), letting the
							 * coalesce pick the default. */
							PH7_ClassInstanceUnref(pThis);
							VM_EXIT_BREAK; /* pTos is already the null temp */
						}
						/* A subclass reading a PARENT's PRIVATE property: php treats it as
						 * an UNDEFINED property (the base-private is invisible to the
						 * subclass scope) — a Warning + null, NOT an access Error. Only
						 * this exact shape warns; every other denied read is the catchable
						 * "Cannot access" Error below. */
						{
						ph7_class *pSelf = VmCurrentSelf(&(*pVm));
						ph7_class *pDecl = pObjAttr->pAttr->pDeclClass;
						if( pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE
						 && pSelf && pDecl && pSelf != pDecl && PH7_VmInstanceOf(pSelf,pDecl) ){
							if( !VmMemberCtxIsLookup(pInstr->iP2) ){
								VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined property: %z::$%z",
									&pClass->sName,&sName);
							}
							PH7_ClassInstanceUnref(pThis);
							VM_EXIT_BREAK; /* pTos is already the null temp */
						}
						}
						/* Genuinely inaccessible: php's CATCHABLE Error (parked on the
						 * boundary rail; the fetch-point router lands it — it was an
						 * uncatchable VmReportUncaughtException+Abort before). */
						{
						SyBlob sErrMsg;
						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";
						SyBlobInit(&sErrMsg,&pVm->sAllocator);
						SyBlobFormat(&sErrMsg,"Cannot access %s property %z::$%z",
							zVis,&pClass->sName,&sName);
						PH7_ClassInstanceUnref(pThis);
						VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));
						SyBlobRelease(&sErrMsg);
						VM_EXIT_BREAK;
						}
					}
				}
				/* Safely unreference the object */
				PH7_ClassInstanceUnref(pThis);
			}
		}else{
			/* `->` on a non-object (e.g. a null intermediate). Silent in isset()/empty()/unset()
			 * context (iP2 2/3/4) so `isset($o->missing->x)` / `unset($o->missing->x)` match PHP. */
			if( pInstr->iP2 != PH7_MEMBER_UNSET && !VmMemberCtxIsLookup(pInstr->iP2) ){
				/* php draws a sharp line here, and PH7 drew none: CALLING a method on a
				 * non-object is a catchable Error, while READING a property off one is a
				 * Warning that yields NULL. PH7 raised the same notice for both and
				 * carried on with NULL, so `$null->m()` silently did nothing. */
				SyString sMemb;
				SyStringInitFromBuf(&sMemb,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));
				if( pInstr->iP2 == PH7_MEMBER_METHOD ){
					SyBlob sErrM;
					sxi32 rcErr;
					SyBlobInit(&sErrM,&pVm->sAllocator);
					SyBlobFormat(&sErrM,"Call to a member function %z() on %s",
						&sMemb,VmArithTypeName(pNos));
					VmPopOperand(&pTos,1);
					PH7_MemObjRelease(pTos);
					MemObjSetType(pTos,MEMOBJ_NULL);
					pTos->nIdx = SXU32_HIGH;
					rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),
						SyBlobLength(&sErrM));
					SyBlobRelease(&sErrM);
					if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }
					rc = rcErr;
					PH7_THROW_ROUTE_MIDEXPR(rc)
				}
				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Attempt to read property \"%z\" on %s",
					&sMemb,VmArithTypeName(pNos));
			}
			VmPopOperand(&pTos,1);
			PH7_MemObjRelease(pTos);
			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */
		}
	}else{
		/* Static member access using class name */
		pNos = pTos;
		pThis = 0;
		if( !pInstr->p3 ){
			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));
			pNos--;
#ifdef UNTRUST
			if( pNos < pStack ){
				VM_EXIT_ABORT;
			}
#endif
		}else{
			/* Attribute name already computed */
			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));
		}
		if( pNos->iFlags & (MEMOBJ_STRING|MEMOBJ_OBJ) ){
			ph7_class *pClass = 0;
			/* A FORWARDING static call — self::/parent::/static:: — preserves the
			 * caller's late-static-binding class (php); a non-forwarding C::m() resets
			 * it to C. The receiver slot below keeps the literal keyword, which OP_CALL
			 * can't resolve, so it would fall back to the callee's DECLARING class and
			 * lose LSB. Remember the live LSB class here and stamp it onto the receiver
			 * after the method name is pushed. */
			int bForwardingCall = 0;
			ph7_class *pForwardLsb = 0;
			if( pNos->iFlags & MEMOBJ_OBJ ){
				/* Class already instantiated */
				pThis = (ph7_class_instance *)pNos->x.pOther;
				pClass = pThis->pClass;
				pThis->iRef++; /* Deffer garbage collection */
			}else{
				/* Try to extract the target class */
				if( SyBlobLength(&pNos->sBlob) > 0 ){
					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);
					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);
					/* Handle self/static/parent keywords */
					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){
						pClass = PH7_VmPeekDeclaringClass(&(*pVm));
						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){
							/* In a trait method, self:: resolves to the using class */
							pClass = PH7_VmPeekTopClass(&(*pVm));
						}
						bForwardingCall = 1;
						pForwardLsb = PH7_VmPeekTopClass(&(*pVm));
					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){
						pClass = PH7_VmPeekTopClass(&(*pVm));
						bForwardingCall = 1;
						pForwardLsb = pClass;
					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){
						pClass = PH7_VmResolveParentClass(&(*pVm));
						bForwardingCall = 1;
						pForwardLsb = PH7_VmPeekTopClass(&(*pVm));
					}else{
						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);
					}
				}
			}
			if( pClass == 0 ){
				/* Undefined class: php throws a catchable Error */
				SyBlob sErrM;
				sxi32 rcErr;
				SyBlobInit(&sErrM,&pVm->sAllocator);
				SyBlobFormat(&sErrM,"Class \"%.*s\" not found",
					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob));
				if( !pInstr->p3 ){
					VmPopOperand(&pTos,1);
				}
				PH7_MemObjRelease(pTos);
				pTos->nIdx = SXU32_HIGH;
				rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),
					SyBlobLength(&sErrM));
				SyBlobRelease(&sErrM);
				if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }
				rc = rcErr;
				PH7_THROW_ROUTE_MIDEXPR(rc)
			}else{
				if( pInstr->iP2 == PH7_MEMBER_METHOD ){
					/* Method call */
					ph7_class_method *pMeth = 0;
					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){
						/* Extract the target method */
						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);
					}
					if( pMeth == 0 || (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){
						if( pMeth ){
							SyBlob sErrM;
							sxi32 rcErr;
							SyBlobInit(&sErrM,&pVm->sAllocator);
							SyBlobFormat(&sErrM,"Cannot call abstract method %z::%z()",
								&pClass->sName,&sName);
							if( !pInstr->p3 ){
								VmPopOperand(&pTos,1);
							}
							PH7_MemObjRelease(pTos);
							pTos->nIdx = SXU32_HIGH;
							rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),
								SyBlobLength(&sErrM));
							SyBlobRelease(&sErrM);
							if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }
							rc = rcErr;
							PH7_THROW_ROUTE_MIDEXPR(rc)
						}else{
							ph7_class_method *pCallStaticMagic = PH7_ClassExtractMethod(pClass,"__callStatic",sizeof("__callStatic")-1);
							if( pCallStaticMagic ){
								/* php: C::missing(...) dispatches __callStatic($name,$args)
								 * via the packing trampoline (see the instance twin). */
								SyBlobReset(&pVm->sMagicCallName);
								SyBlobAppend(&pVm->sMagicCallName,(const void *)sName.zString,sName.nByte);
								pVm->pMagicCallThis = 0;
								pVm->pMagicCallClass = pClass;
								if( !pInstr->p3 ){
									VmPopOperand(&pTos,1);
								}
								PH7_MemObjRelease(pTos);
								SyBlobAppend(&pTos->sBlob,"__phl_magic_call",sizeof("__phl_magic_call")-1);
								MemObjSetType(pTos,MEMOBJ_STRING);
								pTos->nIdx = SXU32_HIGH;
								VM_EXIT_BREAK;
							}
							{
								/* php: the STATIC form reports the same "Call to undefined
								 * method C::m()" as the instance one. */
								SyBlob sErrM;
								sxi32 rcErr;
								SyBlobInit(&sErrM,&pVm->sAllocator);
								SyBlobFormat(&sErrM,"Call to undefined method %z::%z()",
									&pClass->sName,&sName);
								if( !pInstr->p3 ){
									VmPopOperand(&pTos,1);
								}
								PH7_MemObjRelease(pTos);
								pTos->nIdx = SXU32_HIGH;
								rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),
									SyBlobLength(&sErrM));
								SyBlobRelease(&sErrM);
								if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }
								rc = rcErr;
								PH7_THROW_ROUTE_MIDEXPR(rc)
							}
						}
						/* Pop the method name from the stack */
						if( !pInstr->p3 ){
							VmPopOperand(&pTos,1);
						}
						PH7_MemObjRelease(pTos);
					}else{
						/* Push method name on the stack */
						PH7_MemObjRelease(pTos);
						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));
						MemObjSetType(pTos,MEMOBJ_STRING);
					}
					pTos->nIdx = SXU32_HIGH;
					/* Forwarding call (self::/parent::/static::): overwrite the receiver
					 * slot (pNos, one below the method name) with the live LSB class name
					 * so OP_CALL pushes THAT onto aSelf — preserving `static::` inside the
					 * callee. Without this the literal keyword falls through to the
					 * callee's declaring class. Only when an LSB class is actually in
					 * scope (a static call from global scope has none). */
					if( bForwardingCall && pForwardLsb && (pNos->iFlags & MEMOBJ_STRING) ){
						SyBlobReset(&pNos->sBlob);
						SyBlobAppend(&pNos->sBlob,pForwardLsb->sName.zString,pForwardLsb->sName.nByte);
					}
				}else{
					/* Attribute access */
					ph7_class_attr *pAttr = 0;
					if( pInstr->iP2 == PH7_MEMBER_UNSET ){
						/* unset(C::$x): PHP rejects unsetting a static property with a fatal Error.
						 * Without this the iP2=unset tag falls through to a normal static read and the
						 * trailing generic unset() would silently NULL (and de-type) the shared slot. */
						char zMsg[256];
						SyBufferFormat(zMsg,sizeof(zMsg),"Attempt to unset static property %.*s::$%.*s",
							(int)pClass->sName.nByte,pClass->sName.zString,(int)sName.nByte,sName.zString);
						VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);
						VM_EXIT_ABORT;
					}
					/* Check for special ::class pseudo-constant */
					if( sName.nByte == sizeof("class")-1 &&
					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){
						/* ::class returns the fully qualified class name */
						/* Pop the attribute name from the stack */
						if( !pInstr->p3 ){
							VmPopOperand(&pTos,1);
						}
						PH7_MemObjRelease(pTos);
						/* Load the class name */
						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);
						pTos->nIdx = SXU32_HIGH;
					}else{
						/* Extract the target attribute */
						if( sName.nByte > 0 ){
							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);
						}
						if( pAttr == 0 ){
							/* No such STATIC attribute. php raises a catchable Error
							 * ("Access to undeclared static property") — instance magic
							 * (__get) is never consulted for statics (band A #3b; the old
							 * path warned + called __get with a null $this and discarded
							 * it). isset()/empty() context stays silently false. Parked on
							 * the boundary rail; the op completes benignly with NULL and
							 * the fetch-point router lands the throw. */
							if( !VmMemberCtxIsLookup(pInstr->iP2) ){
								SyBlob sErrMsg;
								SyBlobInit(&sErrMsg,&pVm->sAllocator);
								SyBlobFormat(&sErrMsg,"Access to undeclared static property %z::$%z",
									&pClass->sName,&sName);
								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));
							}
						}
						/* Pop the attribute name from the stack */
						if( !pInstr->p3 ){
							VmPopOperand(&pTos,1);
						}
						PH7_MemObjRelease(pTos);
						pTos->nIdx = SXU32_HIGH;
						if( pInstr->iP2 == PH7_MEMBER_REF_TARGET ){
							/* `self::$s =& $x` / `C::$s =& $x`: stash the static property slot
							 * for the following member-marked OP_STORE_REF (class-level, shared
							 * across instances — matches php). Skip the read machinery below. */
							if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_STATIC)
							 && PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){
								pVm->pRefTargetStaticAttr = pAttr;
								pVm->pRefTargetAttr = 0;
								pVm->pRefTargetThis = 0;
								pTos->nIdx = pAttr->nIdx;
							}else{
								pVm->pRefTargetStaticAttr = 0;
								pVm->pRefTargetAttr = 0;
								pVm->pRefTargetThis = 0;
							}
							if( pThis ){
								PH7_ClassInstanceUnref(pThis);
							}
							VM_EXIT_BREAK;
						}
						if( pAttr ){
							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT)) == 0 ){
								/* Access to a non static attribute */
								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",
									&pClass->sName,&pAttr->sName
									);
							}else{
								ph7_value *pValue;
								/* Check if the access to the attribute is allowed */
								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){
									/* PHP 7.4+: uninitialized typed static read.
									 * Same LHS-of-store peek as the instance path. */
									if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0
									 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){
										SyHashEntry *pS = SyHashGet(&pVm->hTypedSlot,
											(const void *)&pAttr->nIdx,sizeof(sxu32));
										if( pS ){
											VmClassAttr *pV = (VmClassAttr *)pS->pUserData;
											if( pV && (pV->iState & VM_CLASS_ATTR_UNINIT) ){
												VmInstr *pNext = pInstr + 1;
												int bIsLhs = 0;
												if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){
													bIsLhs = 1;
												}
												if( !bIsLhs ){
													sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pAttr);
													if( pThis ){
														PH7_ClassInstanceUnref(pThis);
													}
													if( rcU == PH7_ABORT ){
														VM_EXIT_ABORT;
													}
													{
														sxi32 iRp;
														if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){
															pc = iRp;
															VM_EXIT_BREAK;
														}
													}
													VM_EXIT_EXCEPTION;
												}
											}
										}
									}
									if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT)
									 && SySetUsed(&pAttr->aAttrs) > 0 ){
										/* php 8.4 #[\Deprecated] on a class constant:
										 * every access re-warns. */
										VmDeprecatedConstNotice(&(*pVm),pClass,pAttr);
									}
									if( pAttr->nIdx == SXU32_HIGH ){
										/* Unmaterialized slot. Enum case: first access
										 * materializes ALL the singletons. Plain constant: its
										 * initializer hasn't run yet (mount-order-dependent
										 * cross-constant reference) — evaluate on demand. A
										 * raised TypeError/Error (backing mismatch, duplicate
										 * value, self-reference) parks on the boundary rail;
										 * the op completes benignly with NULL and the
										 * fetch-point router lands the throw. */
										sxi32 rcEnum;
										if( pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE ){
											/* php: a DIRECT static access evaluates every case
											 * of the enum (whole-class constant update) — a
											 * broken sibling case throws here too. A reference
											 * from inside another constant's initializer
											 * (nConstEvalDepth > 0) evaluates only the
											 * requested case. */
											if( pVm->nConstEvalDepth > 0 ){
												rcEnum = VmEnumMaterializeCase(&(*pVm),pClass,pAttr);
											}else{
												rcEnum = VmEnumMaterialize(&(*pVm),pClass);
											}
										}else{
											rcEnum = VmClassConstEvalOnDemand(&(*pVm),pClass,pAttr);
										}
										if( rcEnum != SXRET_OK ){
											VmBoundaryPark(&(*pVm),rcEnum);
										}
									}
									/* Load the desired attribute */
									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);
									if( pValue ){
										PH7_MemObjLoad(pValue,pTos);
										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){
											/* Load index number */
											pTos->nIdx = pAttr->nIdx;
										}
									}
								}else{
									/* Throw Error exception (PHP-compatible) */
									char zMsg[256];
									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";
									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){
										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s constant %.*s::%.*s",
											zVis,(int)pClass->sName.nByte,pClass->sName.zString,
											(int)pAttr->sName.nByte,pAttr->sName.zString);
									}else{
										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",
											zVis,(int)pClass->sName.nByte,pClass->sName.zString,
											(int)pAttr->sName.nByte,pAttr->sName.zString);
									}
									VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);
									VM_EXIT_ABORT;
								}
							}
						}
					}
				}
				if( pThis ){
					/* Safely unreference the object */
					PH7_ClassInstanceUnref(pThis);
				}
			}
		}else{
			/* Pop operands */
			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");
			if( !pInstr->p3 ){
				VmPopOperand(&pTos,1);
			}
			PH7_MemObjRelease(pTos);
			pTos->nIdx = SXU32_HIGH;
		}
	}
	VM_EXIT_BREAK;
	VM_EXIT_BREAK;
}

/*
 * OP_CLONE: body moved verbatim from the OP_CLONE arm of
 * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.
 */
PH7_PRIVATE VmOpRc VmExecOpClone(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)
{
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);
	ph7_class_instance *pSrc,*pClone;
#ifdef UNTRUST
	if( pTos < pStack ){
		VM_EXIT_ABORT;
	}
#endif
	/* Make sure we are dealing with a class instance. PHP 8 throws a catchable
	 * TypeError for a non-object operand — for both `clone $x` and clone($x). */
	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){
		SyBlob sMsg;
		SyBlobInit(&sMsg,&pVm->sAllocator);
		SyBlobFormat(&sMsg,"clone(): Argument #1 ($object) must be of type object, %s given",
			ph7_type_name(pTos));
		rc = VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);
		PH7_MemObjRelease(pTos);
		pTos->nIdx = SXU32_HIGH;
		if( rc == PH7_ABORT ){
			VM_EXIT_ABORT;
		}
		{
			sxi32 iRp;
			if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){
				pc = iRp;
				VM_EXIT_BREAK;
			}
		}
		VM_EXIT_EXCEPTION;
	}
	/* Point to the source */
	pSrc = (ph7_class_instance *)pTos->x.pOther;
	/* Enum cases are not cloneable — php's catchable Error (the singleton
	 * identity would break). */
	if( pSrc->pClass->iFlags & PH7_CLASS_ENUM ){
		SyBlob sMsg;
		SyBlobInit(&sMsg,&pVm->sAllocator);
		SyBlobFormat(&sMsg,"Trying to clone an uncloneable object of class %z",
			&pSrc->pClass->sName);
		rc = VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);
		PH7_MemObjRelease(pTos);
		pTos->nIdx = SXU32_HIGH;
		if( rc == PH7_ABORT ){
			VM_EXIT_ABORT;
		}
		{
			sxi32 iRp;
			if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){
				pc = iRp;
				VM_EXIT_BREAK;
			}
		}
		VM_EXIT_EXCEPTION;
	}
	/* Generator and Fiber objects are not cloneable (matches PHP) */
	if( pSrc->pClass == pVm->pGeneratorClass || pSrc->pClass == pVm->pFiberClass ){
		VmErrorFormat(&(*pVm),PH7_CTX_ERR,
			"Trying to clone an uncloneable object of class '%z'",
			&pSrc->pClass->sName);
		PH7_MemObjRelease(pTos);
		VM_EXIT_BREAK;
	}
	/* Perform the clone operation */
	pClone = PH7_CloneClassInstance(pSrc);
	PH7_MemObjRelease(pTos);
	if( pClone == 0 ){
		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,
			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");
	}else{
		/* Load the cloned object */
		pTos->x.pOther = pClone;
		MemObjSetType(pTos,MEMOBJ_OBJ);
	}
	VM_EXIT_BREAK;
	VM_EXIT_BREAK;
}
