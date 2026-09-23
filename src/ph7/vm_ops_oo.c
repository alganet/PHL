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
	/* Resolving the class below can run an AUTOLOADER that throws. Snapshot the boundary
	 * rail so the "not found" report can tell a missing class from an autoloader that
	 * raised — php propagates the autoloader's exception and reports nothing else, while
	 * this arm used to throw its own Error on top of it (uncaught, killing the script
	 * right after the real exception had been handled). */
	sxi32 nNewBrc = pVm->nBoundaryRc;
	const void *pNewRes = (const void *)pVm->pResumeFrame;
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
		if( PH7_VmClassLookupRaised(&(*pVm),nNewBrc,pNewRes) ){
			/* The autoloader threw: land THAT (an in-place catch leaves 0 behind plus a
			 * recorded resume frame, which the router picks up). */
			sxi32 rcAuto = pVm->nBoundaryRc;
			pVm->nBoundaryRc = 0;
			if( nCtorArgs > 0 ){
				VmPopOperand(&pTos,nCtorArgs);
			}
			PH7_MemObjRelease(pTos);
			MemObjSetType(pTos,MEMOBJ_NULL);
			pTos->nIdx = SXU32_HIGH;
			if( rcAuto == PH7_ABORT ){
				VM_EXIT_ABORT;
			}
			rc = PH7_EXCEPTION;
			PH7_THROW_ROUTE_MIDEXPR(rc)
		}
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
	}else if( pClass->iFlags & PH7_CLASS_NOINSTANTIATE ){
		/* php refuses this one in the create_object handler, so the refusal names the
		 * INSTANTIATION and never reaches the constructor — which matters because the
		 * class may still declare a private __construct that Reflection prints. Same
		 * shape as the enum reject below: no instance, no __destruct. */
		SyBlob sErrMsg;
		SyBlobInit(&sErrMsg,&pVm->sAllocator);
		if( pClass->zNewRefusal ){
			/* php words a few of these per class — Directory's names dir() as the
			 * way to get one — so the spec's own sentence wins when it has one. */
			SyBlobAppend(&sErrMsg,pClass->zNewRefusal,SyStrlen(pClass->zNewRefusal));
		}else{
			SyBlobFormat(&sErrMsg,"Instantiation of class %z is not allowed",&pClass->sName);
		}
		VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));
		if( nCtorArgs > 0 ){
			VmPopOperand(&pTos,nCtorArgs);
		}
		PH7_MemObjRelease(pTos);
		pTos->nIdx = SXU32_HIGH;
		VM_EXIT_BREAK;
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
	}else if( VmClassStaticDeferPending(pClass)
	       && (rc = PH7_VmMaterializeClassStatics(&(*pVm),pClass)) != SXRET_OK ){
		/* php materializes the static table at instantiation too, so a default
		 * that THREW at the declaration (or failed its type check) raises BEFORE
		 * any construction (no instance, no __destruct) — same shape as the enum
		 * reject above: park the status, settle the stack, and let the
		 * fetch-point router land it. */
		VmBoundaryPark(&(*pVm),rc);
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
		/* Only an explicit __construct is the constructor. PHP-4-style class-name
		 * constructors were removed in PHP 8.0 — a method named like the class is a
		 * plain method, so no same-name fallback here. */
		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);
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
				/* php NAMES the calling scope when there is one (see the method twin in
				 * OP_CALL); "global scope" is only for code outside every class. */
				ph7_class *pCtorScope = PH7_VmCallerScopeName(&(*pVm));
				SyBlobInit(&sErrMsg,&pVm->sAllocator);
				if( pCtorScope ){
					SyBlobFormat(&sErrMsg,"Call to %s %z::__construct() from scope %z",
						zVis,&pClass->sName,&pCtorScope->sName);
				}else{
					SyBlobFormat(&sErrMsg,"Call to %s %z::__construct() from global scope",
						zVis,&pClass->sName);
				}
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
		if( pVm->nBoundaryRc != 0 ){
			/* A typed property DEFAULT failed its type check during instance-
			 * frame creation (parked catchable TypeError): php aborts the
			 * construction — no constructor call, no object. Consume the
			 * parked status here and route exactly like a constructor throw
			 * (the exception object is already registered). */
			sxi32 rcDef = pVm->nBoundaryRc;
			sxi32 iDefResumePc;
			pVm->nBoundaryRc = 0;
			PH7_ClassInstanceUnref(pNew);
			if( rcDef == PH7_ABORT ){
				VM_EXIT_ABORT;
			}
			if( VmRecordedResume(pVm,&iDefResumePc,pState->pEntryFrame,aInstr) ){
				/* This frame's own try caught it in-place: tidy the stack
				 * (pop ctor args + release the class-name slot, then drain any
				 * abandoned outer-expression operands to the try's base — the
				 * class-name slot itself sits above it) and resume. */
				if( nCtorArgs > 0 ){
					VmPopOperand(&pTos,nCtorArgs);
				}
				PH7_MemObjRelease(pTos);
				PH7_RESUME_DRAIN()
				pc = iDefResumePc;
				VM_EXIT_BREAK;
			}
			VM_EXIT_EXCEPTION;
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
					 * (pop ctor args + release the class-name slot, then drain any
					 * abandoned outer-expression operands to the try's base — the
					 * class-name slot itself sits above it) and resume. */
					if( nCtorArgs > 0 ){
						VmPopOperand(&pTos,nCtorArgs);
					}
					PH7_MemObjRelease(pTos);
					PH7_RESUME_DRAIN()
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
 * Is this OP_MEMBER fetching the property for WRITING — php's BP_VAR_W / BP_VAR_RW
 * fetch, the one that asks the object for something to MODIFY? The compiler tags
 * the base of a subscript-write / `??=` PH7_MEMBER_WRITE, so that answers most of
 * it once the shapes that share the tag are excluded: a plain store and a `??=`
 * write through their own paths, and a direct read-modify-write is the accessor
 * pair (VmMagicRmwArm), not a write fetch. The two php compiles as plain READS —
 * binding a reference (`$r = &$o->p`) and iterating by reference — are told apart
 * by the instruction that follows. `$o->p =& $x` is NOT one of them: the member is
 * the reference TARGET there and carries its own iP2.
 */
static int VmMemberFetchForWrite(const VmInstr *pInstr)
{
	const VmInstr *pNext = pInstr + 1;
	if( pInstr->iP2 == PH7_MEMBER_WRITE ){
		int bPlainStore = (pNext->iOp == PH7_OP_STORE && pNext->iP2 != 0);
		int bCoalesceW = (pNext->iOp == PH7_OP_NULLC_JMP);
		return !bPlainStore && !bCoalesceW && !VmMemberNextIsRmw(pNext);
	}
	if( pInstr->iP2 == PH7_MEMBER_READ ){
		if( pNext->iOp == PH7_OP_STORE_REF ){
			return 1;
		}
		if( pNext->iOp == PH7_OP_FOREACH_INIT && pNext->p3 ){
			return (((ph7_foreach_info *)pNext->p3)->iFlags & PH7_4EACH_STEP_REF) != 0;
		}
	}
	return 0;
}
/*
 * php's `Indirect modification of overloaded property C::$p has no effect`: the
 * write-context fetch above landed on a property only __get answers for, so what
 * comes back is a VALUE and whatever the rest of the expression writes into it is
 * thrown away. php says so and carries on.
 *
 * PHL had the notice at one site only (a by-reference ARGUMENT, VmBindPropByRef)
 * and, worse, the value was not a copy: __get's return still shared the object's
 * own nested hashmap by COW, so `$o->a['b'] = 9`, `$o->a[] = 5` and a by-reference
 * foreach modified the object php leaves untouched. Separating it here is what
 * makes the write land nowhere.
 *
 * An ENGINE __get is not this — php routes those through the class's own property
 * handler, which hands back the real element (ArrayObject's ARRAY_AS_PROPS reads
 * and writes through, in both engines). php stays silent for an OBJECT value too:
 * a handle is shared, so nothing about the write is indirect.
 */
static void VmOverloadedPropNotice(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,ph7_value *pVal)
{
	ph7_class_method *pGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);
	if( pGet == 0 || (pGet->sFunc.iFlags & VM_FUNC_NATIVE) ){
		return;
	}
	if( pVal->iFlags & MEMOBJ_OBJ ){
		return;
	}
	VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,
		"Indirect modification of overloaded property %z::$%z has no effect",
		&pClass->sName,pName);
	if( pVal->iFlags & MEMOBJ_HASHMAP ){
		PH7_HashmapCowSeparate(&(*pVm),pVal);
	}
}
/*
 * Does an OVERLOADED read-modify-write apply to this property access? php runs
 * one only when the class answers BOTH sides; the guard means we are already
 * inside this property's own __get, where php behaves as if the accessor were
 * absent.
 */
static int VmMagicRmwEligible(ph7_vm *pVm,ph7_class *pClass,ph7_class_instance *pThis,const SyString *pName)
{
	return PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1) != 0
	    && PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1) != 0
	    && !VmMagicGuardHeld(pVm,(void *)pThis,pName,'g');
}
/*
 * php's read-modify-write of an OVERLOADED property — `$o->n++`, `--$o->n`,
 * `$o->n += 2`, `$o->n .= 'x'` on a name the instance does not expose. php reads
 * the current value through __get, lets the modify op compute on it, and writes
 * the result back through __set; PHL fell through to dynamic-property creation
 * instead, so the ordinary shape (a class declaring BOTH accessors) died on
 * `Cannot create dynamic property C::$n` — or, when the name IS declared but
 * inaccessible from this scope, on `Cannot access private property C::$n` —
 * for a statement php runs.
 *
 * The write-back rides the same pending-entry rail property HOOKS use: the
 * current value goes into a fresh SCRATCH memobj the modify op mutates in
 * place, and the entry makes that op's tail (PH7_HOOK_RMW_WRITEBACK) dispatch
 * __set with the computed value.
 *
 * BOTH accessors are required, which is php's own split: with only __get php
 * goes on to CREATE the property (PHL's §10 policy refuses a dynamic property),
 * and with only __set it warns `Undefined property` and reads null — neither is
 * this path.
 *
 * *pOut takes __get's value and *pnScratch the slot the modify op must address;
 * the caller does its own stack surgery afterwards, because pName still aliases
 * the NAME operand when the name is dynamic (`$o->$k++`) and releasing that
 * operand first would leave it dangling. *pnScratch stays SXU32_HIGH when __get
 * threw (nothing is armed — the fetch-point router lands the parked throw and
 * php's __set never runs) or when the scratch reservation failed.
 */
static void VmMagicRmwArm(
	ph7_vm *pVm,
	ph7_class_instance *pThis,
	ph7_class *pClass,
	const SyString *pName,
	ph7_value *pOut,
	sxu32 *pnScratch,
	void *pOwnerStack,
	void *pInstrs,
	sxu32 nPc
	)
{
	ph7_value *pScr;
	VmHookRmw sRmw;
	*pnScratch = SXU32_HIGH;
	VmMagicGuardPush(pVm,(void *)pThis,pName,'g');
	PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,pName,pOut);
	VmMagicGuardPop(pVm);
	if( pVm->nBoundaryRc != 0 ){
		PH7_MemObjRelease(pOut);
		return;
	}
	pScr = PH7_ReserveMemObj(&(*pVm));
	if( pScr == 0 ){
		/* OOM: loud allocator diagnostics already fired; the value still stands. */
		return;
	}
	PH7_MemObjStore(pOut,pScr);
	sRmw.iKind = VM_HOOK_PEND_RMW_MAGIC;
	sRmw.pThis = pThis;
	sRmw.pAttr = 0;
	sRmw.nBackIdx = SXU32_HIGH;
	sRmw.nScratchIdx = pScr->nIdx;
	SyBlobInit(&sRmw.sName,&pVm->sAllocator);
	SyBlobAppend(&sRmw.sName,(const void *)pName->zString,pName->nByte);
	sRmw.pOwnerStack = pOwnerStack;
	sRmw.pInstrs = pInstrs;
	sRmw.nJmpPc = nPc;  /* the modify op ... */
	sRmw.nPc = nPc;     /* ... is the whole window */
	pThis->iRef++;
	SySetPut(&pVm->aHookRmw,(const void *)&sRmw);
	*pnScratch = pScr->nIdx;
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
						 * receiver + class + original name and MARK the callee slot
						 * (band A #3b; pre-fix the name-only call discarded everything
						 * and the call site failed with "Invalid function name"). Stack:
						 * pop the method name, then the receiver slot becomes the marked
						 * carrier OP_CALL routes through the packing body. */
						SyBlobReset(&pVm->sMagicCallName);
						SyBlobAppend(&pVm->sMagicCallName,(const void *)sName.zString,sName.nByte);
						pThis->iRef++;
						pVm->pMagicCallThis = pThis;
						pVm->pMagicCallClass = pClass;
						VmPopOperand(&pTos,1);
						PH7_MemObjRelease(pTos);
						pTos->iFlags = MEMOBJ_NULL|MEMOBJ_AUX_MAGICCALL;
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
					int bDenied = 0;
					/* php decides a method's visibility against the class that OWNS it,
					 * which for a trait method is the class that composed it — a trait is
					 * a compile-time construct php has flattened away by now. Deciding
					 * against the trait instead made every rule a question of who USES it:
					 * a protected trait method was denied to a SUBCLASS of the composing
					 * class (not a trait user itself) and granted to an unrelated class
					 * that happened to use the same trait. */
					ph7_class *pOwner = 0;
					if( pMeth->iProtection != PH7_CLASS_PROT_PUBLIC
					 && !PH7_VmClassMemberAccess(&(*pVm),
						(pOwner = PH7_VmMethodScopeName(&(*pVm),pClass,pMeth)),
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
							 * declared (band A #3b); without it the refusal is raised right
							 * here, beside the undefined-method and abstract-method twins. */
							pDeniedCall = PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1);
							bDenied = pDeniedCall == 0;
						}
					}
					if( bDenied ){
						/* php's visibility Error for `$o->m()`. It is raised HERE, at the
						 * member resolution, and not left to OP_CALL: the method OP_CALL
						 * would re-derive is looked up by the FUNCTION's own name against
						 * its declaring class, which a trait adaptation splits away from
						 * the entry this lookup actually chose — `hi as private pHi` gave
						 * OP_CALL the trait's public `hi`, so the private alias ran from
						 * global scope in silence. The entry that answered is right here,
						 * with its composed protection.
						 *
						 * php names the method as the CALL SPELLS it (`$o->PQ()` on a
						 * private `pQ()` reports `PQ`) and the class that OWNS the method,
						 * which for a trait method is the composing class. */
						SyBlob sErrM;
						sxi32 rcErr;
						ph7_class *pScope = PH7_VmCallerScopeName(&(*pVm));
						const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE
							? "private" : "protected";
						SyBlobInit(&sErrM,&pVm->sAllocator);
						if( pScope ){
							SyBlobFormat(&sErrM,"Call to %s method %z::%z() from scope %z",
								zVis,&pOwner->sName,&sName,&pScope->sName);
						}else{
							SyBlobFormat(&sErrM,"Call to %s method %z::%z() from global scope",
								zVis,&pOwner->sName,&sName);
						}
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
					if( pDeniedCall ){
						SyBlobReset(&pVm->sMagicCallName);
						SyBlobAppend(&pVm->sMagicCallName,(const void *)sName.zString,sName.nByte);
						pThis->iRef++;
						pVm->pMagicCallThis = pThis;
						pVm->pMagicCallClass = pClass;
						VmPopOperand(&pTos,1);
						PH7_MemObjRelease(pTos);
						pTos->iFlags = MEMOBJ_NULL|MEMOBJ_AUX_MAGICCALL;
					}else{
						/* Push method name on the stack, MARKED as already screened: the
						 * decision above was made against the entry this lookup chose,
						 * which is the only place a trait adaptation's composed
						 * protection is visible. */
						PH7_MemObjRelease(pTos);
						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));
						MemObjSetType(pTos,MEMOBJ_STRING);
						pTos->iFlags |= MEMOBJ_AUX_MEMBERCALL;
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
				if( pObjAttr
				 && (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT))
				 && PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,
					pObjAttr->pAttr->iProtection,FALSE) ){
					/* A static property (or a constant) belongs to the CLASS: php does
					 * not find it through an instance at all. It notices the attempt —
					 * `Accessing static property C::$s as non static` — and then treats
					 * the name as an ordinary MISSING property: a read warns and answers
					 * null, isset() is false, a write goes to a dynamic property (which
					 * PHL rejects by the §10 policy, like any other undeclared write).
					 * PHL's instance table carries an entry for every declared member
					 * (statics share the class slot), so `$o->s` used to READ and — far
					 * worse — WRITE the class's own static in silence. isset()/empty()
					 * pass silently, as php's do. */
					if( !VmMemberCtxIsLookup(pInstr->iP2)
					 && pInstr->iP2 != PH7_MEMBER_DEFPATH ){
						/* Silent where php is silent: isset()/empty(), and any class
						 * that declares the MAGIC accessor this context would dispatch
						 * (php passes `silent = ce->__get/__set/__unset != NULL` to its
						 * property-offset lookup). Once per access, too: the deferred
						 * call-argument PRE-PASS (PH7_MEMBER_DEFPATH) re-runs this op for
						 * the same source `$o->s` and the value pass carries the notice
						 * — the BY-REF form has no value pass and notices at its own
						 * site (VmBindPropByRef). */
						const char *zMagic;
						if( pInstr->iP2 == PH7_MEMBER_UNSET ){
							zMagic = "__unset";
						}else if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET
						       || VmMemberNextIsWrite(pInstr + 1) ){
							zMagic = "__set";
						}else{
							zMagic = "__get";
						}
						if( PH7_ClassExtractMethod(pClass,zMagic,(sxu32)SyStrlen(zMagic)) == 0 ){
							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,
								"Accessing static property %z::$%z as non static",
								&pClass->sName,&pObjAttr->pAttr->sName);
						}
					}
					pEntry = 0;
					pObjAttr = 0;
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
					if( pInstr->iP2 == PH7_MEMBER_WRITE || pInstr->iP2 == PH7_MEMBER_LIST_TARGET
					 || VmMemberNextIsWrite(pNext) ){
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
							if( bPlainStore || pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){
								pSetMagic = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1);
							}
							if( pSetMagic && pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){
								/* php dispatches __set($name, element) for a missing
								 * destructuring target, but the element value only exists
								 * at the following OP_LOAD_LIST — the dispatch is
								 * unsupported (recorded residual). Leave the miss: the
								 * property stays uncreated, matching php's observable
								 * state (its __set did not store either). */
							}else if( pSetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'s') ){
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
							}else if( VmMemberNextIsRmw(pNext)
							 && VmMagicRmwEligible(pVm,pClass,pThis,&sName) ){
								/* ++/--/compound-assign on an overloaded property: php
								 * reads through __get and writes the computed value back
								 * through __set. Arm the scratch slot the modify op will
								 * mutate and finish the op here — the pre-existing path
								 * vivified a dynamic property instead, which on any
								 * ordinary accessor class is PHL's
								 * "Cannot create dynamic property" Error. */
								ph7_value sRmwVal;
								sxu32 nRmwScratch;
								PH7_MemObjInit(pVm,&sRmwVal);
								VmMagicRmwArm(&(*pVm),pThis,pClass,&sName,&sRmwVal,&nRmwScratch,
									(void *)pStack,(void *)aInstr,(sxu32)(pc + 1));
								VmPopOperand(&pTos,1);   /* drop the property name */
								pThis->iRef++;
								PH7_MemObjRelease(pTos); /* collapse the object slot */
								PH7_MemObjStore(&sRmwVal,pTos);
								pTos->nIdx = nRmwScratch;
								PH7_MemObjRelease(&sRmwVal);
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
								 * to dynamic creation. Leave the miss path — the read gate
								 * below dispatches __get. */
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
						if( pObjAttr && VmMemberNextIsRmw(pNext) ){
							/* A read-modify-write READS the property before it writes it,
							 * so php warns that the name it just created was undefined and
							 * then goes on with null — `$o->hits++` on a fresh object is
							 * `Undefined property: C::$hits` and then int(1). PHL created
							 * the slot in silence. Only the read-modify-write forms:
							 * a subscript-write base (`$o->arr[] = 1`) and a `??=` read
							 * nothing, and php says nothing for either. */
							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined property: %z::$%z",
								&pClass->sName,&sName);
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
							if( bSet && VmMemberCtxWantsValue(pInstr->iP2) ){
								/* empty() and `??`: __isset said set, so php goes on to
								 * __get for the VALUE — emptiness is judged on it, and the
								 * coalesce simply IS it (a null answer then takes the
								 * default, which OP_NULLC does for free). */
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
					if( (!VmMemberCtxIsLookup(pInstr->iP2) || pInstr->iP2 == PH7_MEMBER_COALESCE)
					 && pInstr->iP2 != PH7_MEMBER_LIST_TARGET
					 && !VmMemberNextIsWrite(pInstr + 1) ){
						/* Plain reads AND the ??=/subscript write-base (PH7_MEMBER_WRITE,
						 * which php reads through __get); read-modify-write forms are
						 * excluded (they vivified above — approximate, recorded), and so is
						 * a destructuring target (a pure write — php never reads it).
						 * `??` reaches here only when the class declares NO __isset — the
						 * branch above has returned otherwise — so php's gate is absent and
						 * __get answers on its own. */
						pGetMagic = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);
					}
					if( pGetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){
						ph7_value sMagicRet;
						PH7_MemObjInit(pVm,&sMagicRet);
						VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');
						PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sMagicRet);
						VmMagicGuardPop(pVm);
						if( VmMemberFetchForWrite(pInstr) ){
							/* php's indirect-modification notice, and the separation
							 * that makes the write it describes land nowhere. Raised
							 * BEFORE the pop below: sName still aliases the NAME
							 * operand when the name is dynamic (`$o->$k[0] = v`). */
							VmOverloadedPropNotice(&(*pVm),pClass,&sName,&sMagicRet);
						}
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
					if( !VmMemberCtxIsLookup(pInstr->iP2) && pInstr->iP2 != PH7_MEMBER_LIST_TARGET
					 && pVm->pMagicSetThis == 0
					 && pVm->nBoundaryRc == 0 ){
						/* A destructuring target is also silent: php either created the
						 * property above or dispatched __set — neither warns. */
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
								/* isset()/empty()/`??` on a hooked property all call the get
								 * hook (php): isset is "get() !== null", empty tests the
								 * value, and `??` IS the value. */
								ph7_value sHookRet;
								PH7_MemObjInit(pVm,&sHookRet);
								if( PH7_VmHookGetAttrValue(pThis,pObjAttr,&sHookRet) != SXERR_NOTFOUND ){
									if( !VmMemberCtxWantsValue(pInstr->iP2) ){
										/* isset(): the trailing builtin tests NULL-ness of what
										 * it is handed, so "not set" has to BE null. Storing
										 * bool(FALSE) here made `isset($o->p)` answer TRUE for
										 * a get hook returning null — the same non-null marker
										 * convention the __isset path uses. */
										if( (sHookRet.iFlags & MEMOBJ_NULL) == 0 ){
											pTos->x.iVal = 1;
											MemObjSetType(pTos,MEMOBJ_BOOL);
										}else{
											MemObjSetType(pTos,MEMOBJ_NULL);
										}
									}else{
										/* empty() judges the value; `??` IS the value. */
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
							if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){
								/* Destructuring target: the OP_LOAD_LIST store (typed-
								 * enforced) initializes it — never a read (php). */
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
										PH7_RESUME_DRAIN()
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
						/* A WRITE-context fetch (the base of `$o->p[k] = v`) reads through
						 * __get in php too — the write then lands on the value it answered
						 * and is lost, with php's indirect-modification notice. PHL refused
						 * the whole statement with `Cannot access private property` instead,
						 * a fatal on a program php runs. A plain store and a `??=` keep
						 * their own paths below. */
						if( !VmMemberCtxIsLookup(pInstr->iP2)
						 && (VmMemberFetchForWrite(pInstr)
						  || (pInstr->iP2 != PH7_MEMBER_WRITE && !VmMemberNextIsWrite(pInstr + 1))) ){
							pGetMagic = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);
						}
						/* ++/--/compound-assign on a DECLARED but inaccessible property:
						 * php's accessors answer for it exactly as they do for a missing
						 * one (__get, modify, __set), where PHL raised
						 * `Cannot access private property C::$n`. A plain store keeps its
						 * own __set park below — it is a write, but not a read-modify-write. */
						if( VmMemberNextIsRmw(pInstr + 1)
						 && VmMagicRmwEligible(pVm,pClass,pThis,&sName) ){
							/* The name was already popped and pTos released above, so
							 * sName is the instruction's own literal here. */
							ph7_value sRmwVal;
							sxu32 nRmwScratch;
							PH7_MemObjInit(pVm,&sRmwVal);
							VmMagicRmwArm(&(*pVm),pThis,pClass,&sName,&sRmwVal,&nRmwScratch,
								(void *)pStack,(void *)aInstr,(sxu32)(pc + 1));
							PH7_MemObjStore(&sRmwVal,pTos);
							pTos->nIdx = nRmwScratch;
							PH7_MemObjRelease(&sRmwVal);
							PH7_ClassInstanceUnref(pThis);
							VM_EXIT_BREAK;
						}
						if( pGetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){
							ph7_value sMagicRet;
							PH7_MemObjInit(pVm,&sMagicRet);
							VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');
							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sMagicRet);
							VmMagicGuardPop(pVm);
							if( VmMemberFetchForWrite(pInstr) ){
								VmOverloadedPropNotice(&(*pVm),pClass,&sName,&sMagicRet);
							}
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
							if( pInstr->iP2 == PH7_MEMBER_COALESCE && pIssetMagic == 0 ){
								/* `??` with no __isset to gate it: php reads straight through
								 * __get, exactly as it does for a MISSING property. The
								 * __get dispatch just above this block is gated on a
								 * non-lookup context, so answer here. */
								ph7_class_method *pCoalGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);
								if( pCoalGet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){
									ph7_value sCoalRet;
									PH7_MemObjInit(pVm,&sCoalRet);
									VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');
									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sCoalRet);
									VmMagicGuardPop(pVm);
									PH7_MemObjStore(&sCoalRet,pTos);
									pTos->nIdx = SXU32_HIGH;
									PH7_MemObjRelease(&sCoalRet);
								}
								PH7_ClassInstanceUnref(pThis);
								VM_EXIT_BREAK;
							}
							if( bSet ){
								if( VmMemberCtxWantsValue(pInstr->iP2) ){
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
			/* Same rail snapshot as OP_NEW: the lookup below can run an autoloader that
			 * throws, and php then reports that exception and nothing else. */
			sxi32 nMbBrc = pVm->nBoundaryRc;
			const void *pMbRes = (const void *)pVm->pResumeFrame;
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
						/* In a trait method, self:: resolves to the USING class */
						pClass = PH7_VmPeekSelfClass(&(*pVm));
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
				if( PH7_VmClassLookupRaised(&(*pVm),nMbBrc,pMbRes) ){
					/* ...unless an autoloader raised: land THAT instead of reporting the
					 * class missing on top of it. */
					sxi32 rcAutoM = pVm->nBoundaryRc;
					pVm->nBoundaryRc = 0;
					if( !pInstr->p3 ){
						VmPopOperand(&pTos,1);
					}
					PH7_MemObjRelease(pTos);
					MemObjSetType(pTos,MEMOBJ_NULL);
					pTos->nIdx = SXU32_HIGH;
					if( rcAutoM == PH7_ABORT ){
						VM_EXIT_ABORT;
					}
					rc = PH7_EXCEPTION;
					PH7_THROW_ROUTE_MIDEXPR(rc)
				}
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
							ph7_class_instance *pMagicThis = PH7_VmStaticFallbackThis(&(*pVm),pClass);
							ph7_class_method *pCallStaticMagic = pMagicThis
								? PH7_ClassExtractMethod(pMagicThis->pClass,"__call",sizeof("__call")-1)
								: PH7_ClassExtractMethod(pClass,"__callStatic",sizeof("__callStatic")-1);
							if( pCallStaticMagic ){
								/* php: C::missing(...) dispatches __callStatic($name,$args)
								 * through the packing body (see the instance twin) — or
								 * __call($name,$args) on the calling frame's own $this when
								 * that receiver fits C (VmStaticCallMagicThis). */
								SyBlobReset(&pVm->sMagicCallName);
								SyBlobAppend(&pVm->sMagicCallName,(const void *)sName.zString,sName.nByte);
								if( pMagicThis ){
									pMagicThis->iRef++;
								}
								pVm->pMagicCallThis = pMagicThis;
								pVm->pMagicCallClass = pMagicThis ? pMagicThis->pClass : pClass;
								if( !pInstr->p3 ){
									VmPopOperand(&pTos,1);
								}
								PH7_MemObjRelease(pTos);
								pTos->iFlags = MEMOBJ_NULL|MEMOBJ_AUX_MAGICCALL;
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
						/* Inaccessible from this scope: php routes through the same fallback a
						 * MISSING name takes — __call on a compatible `$this`, else
						 * __callStatic — which is why `C::privateStatic()` runs a catch-all
						 * instead of the "Call to private method" Error OP_CALL would raise
						 * below. */
						ph7_class_method *pDeniedStatic = 0;
						ph7_class_instance *pDeniedThis = 0;
						int bDeniedStatic = 0;
						if( pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){
							/* The OWNING class decides, as in the instance twin above: a
							 * trait method's rules belong to the class that composed it. */
							ph7_class *pOwnerCls = PH7_VmMethodScopeName(&(*pVm),pClass,pMeth);
							if( !PH7_VmClassMemberAccess(&(*pVm),pOwnerCls,&sName,pMeth->iProtection,FALSE) ){
								pDeniedThis = PH7_VmStaticFallbackThis(&(*pVm),pClass);
								pDeniedStatic = pDeniedThis
									? PH7_ClassExtractMethod(pDeniedThis->pClass,"__call",
										sizeof("__call")-1)
									: PH7_ClassExtractMethod(pClass,"__callStatic",
										sizeof("__callStatic")-1);
								bDeniedStatic = pDeniedStatic == 0;
							}
						}
						if( bDeniedStatic ){
							/* No catch-all answers for it: php's visibility Error, raised at
							 * the resolution like the instance twin above. OP_CALL's own
							 * screen re-derives the method from the FUNCTION's name against
							 * its declaring class, which cannot see a trait adaptation's
							 * composed protection — `sHi as private sPriv` ran from global
							 * scope in silence. */
							SyBlob sErrM;
							sxi32 rcErr;
							ph7_class *pOwner = PH7_VmMethodScopeName(&(*pVm),pClass,pMeth);
							ph7_class *pScope = PH7_VmCallerScopeName(&(*pVm));
							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE
								? "private" : "protected";
							SyBlobInit(&sErrM,&pVm->sAllocator);
							if( pScope ){
								SyBlobFormat(&sErrM,"Call to %s method %z::%z() from scope %z",
									zVis,&pOwner->sName,&sName,&pScope->sName);
							}else{
								SyBlobFormat(&sErrM,"Call to %s method %z::%z() from global scope",
									zVis,&pOwner->sName,&sName);
							}
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
						if( pDeniedStatic ){
							/* The packing body takes no receiver slot: drop the method-name
							 * slot so the MARK lands on the receiver slot, exactly as the
							 * missing-method twin above does (leaving the class name in place
							 * would pack it as the first $args entry). */
							SyBlobReset(&pVm->sMagicCallName);
							SyBlobAppend(&pVm->sMagicCallName,(const void *)sName.zString,sName.nByte);
							if( pDeniedThis ){
								pDeniedThis->iRef++;
							}
							pVm->pMagicCallThis = pDeniedThis;
							pVm->pMagicCallClass = pDeniedThis ? pDeniedThis->pClass : pClass;
							if( !pInstr->p3 ){
								VmPopOperand(&pTos,1);
							}
							PH7_MemObjRelease(pTos);
							pTos->iFlags = MEMOBJ_NULL|MEMOBJ_AUX_MAGICCALL;
							pTos->nIdx = SXU32_HIGH;
							VM_EXIT_BREAK;
						}
						/* Push method name on the stack, MARKED as already screened (see the
						 * instance twin: OP_CALL cannot re-derive a composed protection). */
						PH7_MemObjRelease(pTos);
						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));
						MemObjSetType(pTos,MEMOBJ_STRING);
						pTos->iFlags |= MEMOBJ_AUX_MEMBERCALL;
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
						/* Extract the target attribute. php keeps constants and
						 * (static) properties in separate namespaces; the source
						 * form disambiguates (set by the `::` codegen, compile.c):
						 * a `$`-form is a STATIC PROPERTY (hAttr) — either a literal
						 * name folded into pInstr->p3 (`C::$s`) or a dynamic name on
						 * the stack marked iP1==2 (`C::$$x`, `C::${$e}`); a bareword
						 * (p3==0, iP1==1) is a CONSTANT or enum case (hConst). This
						 * is what lets `const C` and `public $C` coexist and resolve
						 * to the right member. */
						if( sName.nByte > 0 ){
							pAttr = (pInstr->p3 || pInstr->iP1 == 2)
								? PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte)
								: PH7_ClassExtractConstant(pClass,sName.zString,sName.nByte);
						}
						if( pAttr == 0 ){
							/* No such member. php raises a catchable Error whose wording
							 * depends on the ACCESS form (the same p3/iP1 signal used above):
							 * a bareword `C::MISSING` (constant form) is "Undefined constant
							 * C::MISSING", a `$`-form `C::$missing` is "Access to undeclared
							 * static property C::$missing". Instance magic (__get) is never
							 * consulted for statics (band A #3b). isset()/empty() context
							 * stays silently false. Parked on the boundary rail; the op
							 * completes benignly with NULL and the fetch-point router lands
							 * the throw. */
							if( !VmMemberCtxIsLookup(pInstr->iP2) ){
								SyBlob sErrMsg;
								SyBlobInit(&sErrMsg,&pVm->sAllocator);
								if( pInstr->p3 == 0 && pInstr->iP1 != 2 ){
									SyBlobFormat(&sErrMsg,"Undefined constant %z::%z",
										&pClass->sName,&sName);
								}else{
									SyBlobFormat(&sErrMsg,"Access to undeclared static property %z::$%z",
										&pClass->sName,&sName);
								}
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
							 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0
							 && VmClassStaticDeferPending(pClass) ){
								/* Binding a reference TO a static touches the table, so it
								 * materializes first, like every other static access. */
								sxi32 rcRt = PH7_VmMaterializeClassStatics(&(*pVm),pClass);
								if( rcRt != SXRET_OK ){
									pVm->pRefTargetStaticAttr = 0;
									pVm->pRefTargetAttr = 0;
									pVm->pRefTargetThis = 0;
									if( pThis ){
										PH7_ClassInstanceUnref(pThis);
									}
									if( rcRt == PH7_ABORT ){
										VM_EXIT_ABORT;
									}
									{
										sxi32 iRpR;
										if( VmRecordedResume(pVm,&iRpR,pState->pEntryFrame,aInstr) ){
											PH7_RESUME_DRAIN()
											pc = iRpR;
											VM_EXIT_BREAK;
										}
									}
									VM_EXIT_EXCEPTION;
								}
							}
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
								/* php materializes the class's static table at the FIRST
								 * static-property access (any property, any context — read,
								 * write, even isset): a default whose evaluation was DEFERRED
								 * (it threw at the declaration) runs here, and a typed one that
								 * failed its check throws its catchable TypeError here. Class
								 * constants and static method calls do not trigger the
								 * materialization (php-exact). */
								if( (pAttr->iFlags & PH7_CLASS_ATTR_STATIC)
								 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0
								 && VmClassStaticDeferPending(pClass) ){
									sxi32 rcD = PH7_VmMaterializeClassStatics(&(*pVm),pClass);
									if( rcD != SXRET_OK ){
										if( pThis ){
											PH7_ClassInstanceUnref(pThis);
										}
										if( rcD == PH7_ABORT ){
											VM_EXIT_ABORT;
										}
										{
											sxi32 iRpD;
											if( VmRecordedResume(pVm,&iRpD,pState->pEntryFrame,aInstr) ){
												PH7_RESUME_DRAIN()
												pc = iRpD;
												VM_EXIT_BREAK;
											}
										}
										VM_EXIT_EXCEPTION;
									}
								}
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
												if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){
													/* Destructuring target ([S::$s] = [...]):
													 * initialized by the OP_LOAD_LIST store. */
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
															PH7_RESUME_DRAIN()
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
		char zGiven[64];
		SyBlobInit(&sMsg,&pVm->sAllocator);
		/* php names the VALUE for a bool ("true given", not "bool given"), which is
		 * what every other argument diagnostic here already says — the shared helper. */
		SyBlobFormat(&sMsg,"clone(): Argument #1 ($object) must be of type object, %s given",
			VmValueGivenName(pTos,zGiven,sizeof(zGiven)));
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
	 * identity would break) — and neither is a class whose instances own a
	 * C-side resource a slot-by-slot copy would double-free (PH7_CLASS_NOCLONE),
	 * nor a Generator or a Fiber (their suspended C state is not copyable). php
	 * words all of them the same way and throws the same catchable Error; the
	 * Generator/Fiber pair used to take an older warn-only path here, so
	 * `clone $gen` PRINTED an uncatchable diagnostic and then carried on with the
	 * ORIGINAL object standing in for the copy — the one shape of `clone` that
	 * answered a value php never lets the program reach. */
	if( (pSrc->pClass->iFlags & (PH7_CLASS_ENUM|PH7_CLASS_NOCLONE))
		|| pSrc->pClass == pVm->pGeneratorClass || pSrc->pClass == pVm->pFiberClass ){
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
