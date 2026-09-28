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
	/* iP1 < 0 is the SCREEN pass: php's NEW resolves the class, refuses everything a
	 * `new` can be refused for and allocates the object BEFORE the constructor arguments
	 * are evaluated — only the constructor body runs after them. PHL evaluated the whole
	 * argument list first, so `new NoSuchClass(s(1))`, `new AbstractC(s(1))` and
	 * `new PrivateCtorC(s(1))` all ran `s(1)` on a `new` php never performs. The screen
	 * is this same handler with no arguments on the stack, returning just before the
	 * allocation and LEAVING the class name for the real pass that follows it — one code
	 * path, so the two can never disagree about what a refusal is. */
	int bScreenOnly = pInstr->iP1 < 0;
	sxi32 nCtorArgs = bScreenOnly ? 0
		: pInstr->iP1 + (pInstr->iP2 ? VmSpreadOwnExtra(pVm,pInstr->iP1,pTos) : 0);
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
	/* The screen pass has no arguments and no runs of its own: building (and
	 * truncating) an effective map there would speak for the enclosing call. */
	VmCallArgMap *pEffNewMap = bScreenOnly ? 0 : VmEffCallArgMap(pVm,pInstr,pArg,
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
			/* The extract above only accepts NEW-able classes, so an interface, an
			 * abstract class or a trait comes back as 0 and used to be reported as
			 * "not found". Look again without that filter so php's real message can be
			 * given — but through the table DIRECTLY, never PH7_VmExtractClass: that
			 * one fires the autoloader when the name is absent, so a genuinely missing
			 * class ran every registered autoloader TWICE where php runs them once. */
			const char *zNotNew = (const char *)SyBlobData(&pTos->sBlob);
			sxu32 nNotNew = SyBlobLength(&pTos->sBlob);
			SyHashEntry *pNotNewEntry;
			PH7_VmClassNameAnchor(&zNotNew,&nNotNew);
			pNotNewEntry = nNotNew > 0 ? SyHashGet(&pVm->hClass,(const void *)zNotNew,nNotNew) : 0;
			pNotNew = pNotNewEntry ? (ph7_class *)pNotNewEntry->pUserData : 0;
		}
		if( pNotNew && (pNotNew->iFlags & (PH7_CLASS_INTERFACE|PH7_CLASS_ABSTRACT|PH7_CLASS_TRAIT)) ){
			/* php names WHAT it will not instantiate; a trait is one of the three, and
			 * PH7's loadable-only extract rejected it as "not found" — the one shape of
			 * `new` whose refusal did not say why. */
			const char *zKind = (pNotNew->iFlags & PH7_CLASS_INTERFACE) ? "interface"
				: (pNotNew->iFlags & PH7_CLASS_TRAIT) ? "trait" : "abstract class";
			SyBlobFormat(&sErrM,"Cannot instantiate %s %z",zKind,&pNotNew->sName);
		}else if( (pTos->iFlags & (MEMOBJ_STRING|MEMOBJ_OBJ)) == 0 ){
			/* php refuses the OPERAND before it ever has a name to look up: a `new`
			 * takes an object or a string and nothing else, and every other value --
			 * int, float, bool, null, array, resource -- is
			 * `Class name must be a valid object or a string`. PHL string-cast the
			 * slot's raw blob instead, so all six answered `Class "" not found`, a
			 * sentence that names a class the program never wrote. (An EMPTY string
			 * really is `Class "" not found` in php, so the test is the TYPE.) The
			 * `::` twin of this refusal is already at the bottom of VmExecOpMember. */
			SyBlobAppend(&sErrM,"Class name must be a valid object or a string",
				sizeof("Class name must be a valid object or a string")-1);
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
		{
			const char *zRefCls = pClass->zNewRefusalClass ? pClass->zNewRefusalClass : "Error";
			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),zRefCls,
				(sxu32)SyStrlen(zRefCls),&sErrMsg));
		}
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
				ph7_class *pCtorScope = PH7_VmCallerScope(&(*pVm));
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
		if( bScreenOnly ){
			/* Every refusal above has been asked. Leave the class name standing for
			 * the real pass and let the arguments run. */
			VM_EXIT_BREAK;
		}
		if( nCtorArgs > 0 ){
			/* D1: a `new`'s arguments are ARGUMENTS. An element/property one whose
			 * target was absent at load time rides a deferred carrier that only OP_CALL
			 * resolved, and the ctor call reaches its callee by pointer rather than
			 * through that opcode — so `new C($a['missing'])` passed a silent NULL where
			 * php warns, and a by-REFERENCE ctor parameter (`new C($a['fresh'])` with
			 * `__construct(&$x)`) never vivified the target at all: php writes the
			 * element, PHL left it uncreated. Resolve here, where the constructor is
			 * known and pVm->pFrame is still the caller, exactly as every OP_CALL
			 * dispatch branch does. A class with NO constructor still resolves — the
			 * arguments were evaluated and php reports what reading them found. */
			ph7_class_method *pCtorArgs = pCons;
			sxi32 rcDA;
			if( pCtorArgs == 0 ){
				rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,0,0,0,pEffNewMap);
			}else if( pCtorArgs->sFunc.iFlags & VM_FUNC_NATIVE ){
				/* A native constructor has no compiled formals; its by-ref positions
				 * come from the signature-derived mask, the builtin way. */
				rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,
					pCtorArgs->sFunc.pNative ? pCtorArgs->sFunc.pNative->nByRefMask : 0,0,0,pEffNewMap);
			}else{
				rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,
					(ph7_vm_func_arg *)SySetBasePtr(&pCtorArgs->sFunc.aArgs),
					SySetUsed(&pCtorArgs->sFunc.aArgs),0,0,0,pEffNewMap);
			}
			if( rcDA == PH7_ABORT || rcDA == PH7_EXCEPTION ){
				/* Reading an argument raised (a string-offset Error, a magic accessor's
				 * throw): no object is created, and the stack is tidied exactly as the
				 * constructor-throw path below tidies it. */
				sxi32 iResumeDA;
				if( rcDA == PH7_ABORT ){
					VM_EXIT_ABORT;
				}
				if( VmRecordedResume(pVm,&iResumeDA,pState->pEntryFrame,aInstr) ){
					VmPopOperand(&pTos,nCtorArgs);
					PH7_MemObjRelease(pTos);
					PH7_RESUME_DRAIN()
					pc = iResumeDA;
					VM_EXIT_BREAK;
				}
				VM_EXIT_EXCEPTION;
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
 * A native class's property is a field of php's own C struct, and the only writes
 * that reach one are the writes the store filter converts (ph7_class::xSet). So
 * the fetched value keeps its slot index for exactly the shapes that end in such
 * a store -- a plain assignment, `??=`, a destructuring target and the
 * read-modify-write forms -- and is a TEMPORARY for every other use. That is
 * php's own answer: it has no ptr_ptr handler for such a property, so a reference
 * bind (`$r = &$i->f`), a by-reference argument (`preg_match($p,$s,$i->s)`) and a
 * by-reference foreach all get a copy whose writes are SILENTLY lost -- silently,
 * unlike the overloaded case, which php has a notice for.
 */
static int VmMemberNativeSetKeepsSlot(const VmInstr *pInstr)
{
	if( pInstr->iP2 == PH7_MEMBER_WRITE ){
		/* Every fetch the compiler tags for writing: a plain store, `??=`, a
		 * compound assign, and the base of a subscript write — that last one has
		 * to reach the real value so `$i->y[0] = 5` is php's "Cannot use a scalar
		 * value as an array" rather than a write nobody notices. */
		return 1;
	}
	if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){
		return 1;   /* list()/foreach destructuring target */
	}
	return VmMemberNextIsWrite(pInstr + 1);   /* ++/-- and the compound assigns */
}
/*
 * `$doc->encoding ??= 'UTF-8'` on a name a native class's own property-handler
 * table carries.
 *
 * The member opcode's handler gate answers a READ and can only REFUSE a write,
 * because the value does not exist yet -- and a coalesce that finds null must
 * make one. Only the overloaded-coalesce rail carries a pending store to the
 * point the value arrives, so this shape is left to it: the read it makes goes
 * through the same handler anyway.
 */
static int VmMemberCoalOwned(ph7_class *pClass,const VmInstr *pInstr,const SyString *pName)
{
	return pInstr->iP2 == PH7_MEMBER_WRITE
	    && pInstr[1].iOp == PH7_OP_NULLC_JMP
	    && PH7_ClassNativePropOwns(pClass,pName);
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
PH7_PRIVATE void PH7_VmOverloadedPropNotice(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,ph7_value *pVal)
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
	if( PH7_ClassNativePropOwns(pClass,pName) ){
		/* A native class's own property handler answers BOTH halves -- php reads
		 * `$doc->version .= '.1'` through read_property and writes the result back
		 * through write_property, with no magic accessor involved either way. */
		return 1;
	}
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
 * The property php's `C::$name` form finds. That form reads the class's whole
 * property table -- instance properties and constants included, answering on
 * visibility before static-ness (see the arm that uses it) -- and php's table
 * carries a base's PRIVATE instance property as a SHADOW entry, so `B::$q` on
 * `class A { private $q; }` is "Cannot access private property B::$q" and not the
 * undeclared-static sentence. Here that member lives under its mangled STORAGE
 * name, which the plain probe cannot see, so the ancestry answers for it: the
 * nearest base that declares one under the plain name.
 */
static ph7_class_attr * VmClassAttrWithShadow(ph7_class *pClass,const char *zName,sxu32 nName)
{
	ph7_class *pWalk;
	ph7_class_attr *pAttr = PH7_ClassExtractAttribute(pClass,zName,nName);
	if( pAttr ){
		return pAttr;
	}
	for( pWalk = pClass->pBase ; pWalk ; pWalk = pWalk->pBase ){
		pAttr = PH7_ClassExtractAttribute(pWalk,zName,nName);
		if( pAttr ){
			return (pAttr->iProtection == PH7_CLASS_PROT_PRIVATE
			     && (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT)) == 0)
				? pAttr : 0;
		}
	}
	return 0;
}
/*
 * May the executing scope touch this class-level member reached through an
 * INSTANCE (`$o->s` on a static, or on a class constant)?
 *
 * The ordinary attribute rule, plus php's shadow re-lookup: when the member the
 * object's class holds under this name is a private one the scope may not touch,
 * php asks the SCOPE's own table for the name and uses what it finds there. With
 * `class A { private static $q; } class B extends A { private static $q; }` that
 * is how `$b->q` from inside A reaches A's own -- the as-non-static notice and
 * then the ordinary undefined-property answer, rather than a visibility refusal
 * about B's.
 */
static int VmStaticThroughInstanceVisible(ph7_vm *pVm,ph7_class *pClass,
	ph7_class_attr *pAttr,const SyString *pName)
{
	ph7_class *pScope;
	ph7_class_attr *pShadow;
	if( PH7_VmClassAttrAccess(&(*pVm),pClass,pAttr,FALSE) ){
		return 1;
	}
	if( pAttr->iProtection != PH7_CLASS_PROT_PRIVATE || pName->nByte < 1 ){
		return 0;
	}
	pScope = PH7_VmCallerScope(&(*pVm));
	if( pScope == 0 || !PH7_VmInstanceOf(pClass,pScope) ){
		return 0;
	}
	pShadow = PH7_ClassExtractAttribute(pScope,pName->zString,pName->nByte);
	return pShadow != 0
		&& pShadow != pAttr
		&& pShadow->iProtection == PH7_CLASS_PROT_PRIVATE
		&& PH7_VmMemberOwnerClass(pShadow->pDeclClass,pScope) == pScope;
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
	int bStaticHidden = 0; /* a `::$name` already refused for VISIBILITY */
	if( !pInstr->iP1 ){
		pNos = &pTos[-1];
#ifdef UNTRUST
		if( pNos < pStack ){
			VM_EXIT_ABORT;
		}
#endif
		/* What a dynamic NAME may BE, decided before the receiver is looked at --
		 * php settles this in the opcode handler's first lines, and the two name
		 * positions settle it differently.
		 *
		 * A METHOD name must ALREADY be a string: zend's INIT_METHOD_CALL refuses
		 * anything else outright with `Method name must be a string`, before the
		 * receiver's type, before __call, and before the name would be looked up.
		 * A PROPERTY name is COERCED instead, with the user-visible rules -- an
		 * array warns `Array to string conversion` and renders "Array", a float,
		 * bool or null spells itself out, an object hands over its __toString()
		 * and one without it is php's catchable `Object of class X could not be
		 * converted to string`.
		 *
		 * PHL read the name slot's RAW BLOB, which is empty for every value that
		 * is not already a string, so `$o->{5}`, `$o->{1.5}`, `$o->{true}` and
		 * `$o->{$stringable}` all named the property "" -- one shared property per
		 * object, silently, on every access shape (read, write, isset, unset,
		 * increment, by-ref) -- and `$o->{[1]}` on an object with no such property
		 * SEGFAULTED, because an empty blob hands out a NULL pointer that the
		 * dynamic-property path dereferences.
		 *
		 * php's own exception is the one shape that answers before it ever asks
		 * for the name: a lookup (isset/empty/`??`) or an unset() whose receiver
		 * is not an object short-circuits, so no coercion and no diagnostic. A
		 * `?->` on null never reaches here at all -- OP_NULLSAFE_JMP has already
		 * jumped past both the name expression and this op.
		 */
		if( pInstr->iP2 == PH7_MEMBER_METHOD ){
			if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){
				sxi32 rcMn;
				VmPopOperand(&pTos,1);
				PH7_MemObjRelease(pTos);
				MemObjSetType(pTos,MEMOBJ_NULL);
				pTos->nIdx = SXU32_HIGH;
				rcMn = VmThrowFromVm(&(*pVm),"Error","Method name must be a string",
					sizeof("Method name must be a string")-1);
				if( rcMn == SXERR_ABORT ){ VM_EXIT_ABORT; }
				rc = rcMn;
				PH7_THROW_ROUTE_MIDEXPR(rc)
			}
		}else if( (pNos->iFlags & MEMOBJ_OBJ)
		       || (pInstr->iP2 != PH7_MEMBER_UNSET && !VmMemberCtxIsLookup(pInstr->iP2)) ){
			sxi32 rcNm = PH7_MemObjToStringUV(pTos);
			if( rcNm != SXRET_OK ){
				VmPopOperand(&pTos,1);
				PH7_MemObjRelease(pTos);
				MemObjSetType(pTos,MEMOBJ_NULL);
				pTos->nIdx = SXU32_HIGH;
				if( rcNm == PH7_ABORT ){ VM_EXIT_ABORT; }
				rc = rcNm;
				PH7_THROW_ROUTE_MIDEXPR(rc)
			}
		}
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
			if( PH7_VmIsIncompleteClass(&(*pVm),pClass) ){
				/* __PHP_Incomplete_Class: every property access and method call is
				 * php's incomplete-object diagnostic — the carrier's OWN entries are
				 * for the engine's surfaces only. A READ (isset/empty/?? included)
				 * is an E_WARNING answering NULL; every WRITE shape and unset() is
				 * a catchable Error; a method call is its own Error, raised where
				 * the undefined-method twin raises (before any argument runs). The
				 * DEFPATH pre-pass defers like a MISSING property would — the slot
				 * fast path must not hand out the carrier's storage — so the by-ref
				 * resolve (VmBindPropByRef) raises the Error and the by-value
				 * re-drive lands back here in READ context for the warning.
				 * `??=` reads before it refuses, so it takes BOTH, php's order. */
				VmInstr *pIncNext = pInstr + 1;
				if( pInstr->iP2 == PH7_MEMBER_DEFPATH && pNos->nIdx != SXU32_HIGH ){
					SyString sIncProp;
					VmDeferredPath *pIncPath = VmDeferPathNew(&(*pVm),0,pNos->nIdx,0);
					SyStringInitFromBuf(&sIncProp,sName.zString,sName.nByte);
					if( pIncPath && VmDeferPathPushProp(pIncPath,&sIncProp) == SXRET_OK ){
						VmPopOperand(&pTos,1);       /* drop the property name */
						pThis->iRef++;
						PH7_MemObjRelease(pTos);     /* collapse the object slot into the carrier */
						pTos->x.pOther = pIncPath;
						pTos->iFlags = MEMOBJ_NULL | MEMOBJ_AUX_DEFPATH;
						pTos->nIdx = SXU32_HIGH;
						PH7_ClassInstanceUnref(pThis);
						VM_EXIT_BREAK;
					}
					if( pIncPath ){
						VmFreeDeferredPath(pIncPath);
					}
					/* allocation failure (or a temporary base below): the read
					 * verdict is all that is left — fall through to warn + NULL. */
				}
				int bIncCall = (pInstr->iP2 == PH7_MEMBER_METHOD);
				int bIncCoalW = (pInstr->iP2 == PH7_MEMBER_WRITE && pIncNext->iOp == PH7_OP_NULLC_JMP);
				int bIncModify = pInstr->iP2 != PH7_MEMBER_DEFPATH
					&& (pInstr->iP2 == PH7_MEMBER_UNSET
					 || pInstr->iP2 == PH7_MEMBER_WRITE
					 || pInstr->iP2 == PH7_MEMBER_REF_TARGET
					 || pInstr->iP2 == PH7_MEMBER_LIST_TARGET
					 || VmMemberNextIsWrite(pIncNext)
					 || VmMemberFetchForWrite(pInstr));
				if( bIncCall ){
					SyBlob sIncErr;
					sxi32 rcInc;
					SyBlobInit(&sIncErr,&pVm->sAllocator);
					PH7_VmIncompleteMsg(&(*pVm),pThis,"call a method",&sIncErr);
					VmPopOperand(&pTos,1);
					PH7_MemObjRelease(pTos);
					pTos->nIdx = SXU32_HIGH;
					rcInc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sIncErr),
						SyBlobLength(&sIncErr));
					SyBlobRelease(&sIncErr);
					if( rcInc == SXERR_ABORT ){ VM_EXIT_ABORT; }
					rc = rcInc;
					PH7_THROW_ROUTE_MIDEXPR(rc)
				}
				if( bIncCoalW ){
					PH7_VmIncompleteAccessWarn(&(*pVm),pThis,0);
				}
				if( bIncModify ){
					SyBlob sIncErr;
					SyBlobInit(&sIncErr,&pVm->sAllocator);
					PH7_VmIncompleteMsg(&(*pVm),pThis,"modify a property",&sIncErr);
					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sIncErr));
				}else{
					PH7_VmIncompleteAccessWarn(&(*pVm),pThis,0);
				}
				VmPopOperand(&pTos,1);   /* pop the attribute name */
				PH7_MemObjRelease(pTos); /* the object slot becomes the NULL answer */
				pTos->nIdx = SXU32_HIGH;
				VM_EXIT_BREAK;
			}
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
						VmMagicCall *pPend = VmMagicCallNew(&(*pVm),pThis,pClass,&sName);
						if( pPend == 0 ){
							VM_EXIT_ABORT;
						}
						VmPopOperand(&pTos,1);
						PH7_MemObjRelease(pTos);
						pTos->x.pOther = pPend;
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
						ph7_class *pScope = PH7_VmCallerScope(&(*pVm));
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
						VmMagicCall *pPend = VmMagicCallNew(&(*pVm),pThis,pClass,&sName);
						if( pPend == 0 ){
							VM_EXIT_ABORT;
						}
						VmPopOperand(&pTos,1);
						PH7_MemObjRelease(pTos);
						pTos->x.pOther = pPend;
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
				/* A LAZY native property read before anything installed it, whose
				 * class answers such a read from its zeroed struct rather than
				 * calling the name undefined (PH7_CLASS_ATTR_NATIVE_LAZY_DEFAULT).
				 * Set by the miss handling below and answered after the pop. */
				ph7_class_attr *pLazyDefault = 0;
				if( sName.nByte > 0 && sName.zString[0] == 0
				 && !PH7_VmIsIncompleteClass(&(*pVm),pClass) ){
					/* php refuses a property name beginning with a NUL outright:
					 * `Cannot access property starting with "\0"`. Those bytes are
					 * the ENGINE's private mangling — "\0Cls\0p" is C1::$p and
					 * "\0*\0p" is a protected slot — so a script that could write one
					 * would forge a private member of any class it names, and every
					 * display surface would then render the forgery as the real thing.
					 * Two rules ride with it, both probe-verified: a MAGIC accessor
					 * wins (php dispatches __get/__set/__isset/__unset with the raw
					 * name and says nothing), and a LOOKUP context is silent — isset()
					 * is false, empty() true, `??` takes the default — which is what
					 * the existing miss handling below already answers. The refusal
					 * does not depend on whether such a property exists: php raises it
					 * on the NAME, before any lookup, and the one place a NUL-keyed
					 * property legitimately lives is the __PHP_Incomplete_Class
					 * carrier, whose own gate above has already answered for it.
					 * A METHOD name is not covered — php lets `$o->{"\0m"}()` reach
					 * the ordinary undefined-method Error — so this sits in the
					 * attribute branch only. */
					const char *zNulMagic;
					if( pInstr->iP2 == PH7_MEMBER_UNSET ){
						zNulMagic = "__unset";
					}else if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET
					       || VmMemberNextIsWrite(pInstr + 1) ){
						zNulMagic = "__set";
					}else{
						zNulMagic = "__get";
					}
					if( !VmMemberCtxIsLookup(pInstr->iP2)
					 && PH7_ClassExtractMethod(pClass,zNulMagic,(sxu32)SyStrlen(zNulMagic)) == 0 ){
						SyBlob sNulErr;
						sxi32 rcNul;
						SyBlobInit(&sNulErr,&pVm->sAllocator);
						SyBlobAppend(&sNulErr,"Cannot access property starting with \"\\0\"",
							sizeof("Cannot access property starting with \"\\0\"")-1);
						VmPopOperand(&pTos,1);   /* pop the attribute name */
						PH7_MemObjRelease(pTos); /* the object slot becomes the NULL answer */
						pTos->nIdx = SXU32_HIGH;
						rcNul = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sNulErr),
							SyBlobLength(&sNulErr));
						SyBlobRelease(&sNulErr);
						if( rcNul == SXERR_ABORT ){ VM_EXIT_ABORT; }
						rc = rcNul;
						PH7_THROW_ROUTE_MIDEXPR(rc)
					}
				}
				/* Extract the target attribute, as the class whose code is RUNNING
				 * sees it: a scope that declares a private of this name owns a slot
				 * of its own on every instance below it (php's mangled storage name)
				 * and means THAT one, whatever the object's class holds under the
				 * plain name. The EMPTY name is a real property name in php —
				 * `$o->{''} = 1` creates one and `$o->{''}` reads it back — and it is
				 * the one name SyHashGet cannot answer for, so it takes a
				 * list-walking lookup inside. */
				pEntry = PH7_ClassInstanceScopedAttrEntry(&(*pVm),pThis,sName.zString,sName.nByte);
				if( pEntry ){
					/* Point to the attribute value */
					pObjAttr = (VmClassAttr *)pEntry->pUserData;
				}
				if( pObjAttr
				 && (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT))
				 && !VmStaticThroughInstanceVisible(&(*pVm),pClass,pObjAttr->pAttr,&sName) ){
					/* A private static this scope may not touch. When it is an INHERITED
					 * one php's instance path does not see it at all: `$b->s` is an
					 * ordinary MISSING property (warn and null, a dynamic write, a silent
					 * unset), with none of the visibility refusal the DECLARING class's
					 * own instance gets. The declaring class's own keeps the refusal. */
					if( PH7_VmMemberOwnerClass(pObjAttr->pAttr->pDeclClass,pClass) != pClass ){
						pEntry = 0;
						pObjAttr = 0;
					}
				}else if( pObjAttr
				 && (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT)) ){
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
				if( pObjAttr == 0 && PH7_ClassHasNativeProp(pClass)
				 && !VmMemberCoalOwned(pClass,pInstr,&sName) ){
					/* php's read_property / has_property / write_property /
					 * unset_property handlers, for a class whose properties are not
					 * storage at all: PDORow answers every read from the statement's
					 * current ROW, holds no slot for any of them, and refuses every
					 * write. It comes first among the miss paths, and before the
					 * declared-but-absent one: a name this class owns is neither a
					 * dynamic property to create, nor an "Undefined property" to warn
					 * about, nor a __get to dispatch.
					 *
					 * Which handler php asks is what the CONTEXT says: a plain store,
					 * a destructuring target and a read-modify-write all end in a
					 * write; `??=` reads first and writes only when the read answered
					 * null; a subscript-write base (`$o->p[0] = 1`) is a READ whose
					 * value the subscript then refuses; and isset() stops at the
					 * truth while empty() and `??` take the value. */
					VmInstr *pPropNext = pInstr + 1;
					int bPropStore = (pPropNext->iOp == PH7_OP_STORE && pPropNext->iP2 != 0);
					int bPropCoal = (pInstr->iP2 == PH7_MEMBER_WRITE
						&& pPropNext->iOp == PH7_OP_NULLC_JMP);
					PH7_NativePropCtx sProp;
					ph7_value sPropVal;
					PH7_MemObjInit(&(*pVm),&sPropVal);
					if( pInstr->iP2 == PH7_MEMBER_UNSET ){
						sProp.iMode = PH7_NATIVE_PROP_UNSET;
					}else if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET || bPropStore
					       || VmMemberNextIsRmw(pPropNext) ){
						sProp.iMode = PH7_NATIVE_PROP_WRITE;
					}else if( pInstr->iP2 == PH7_MEMBER_ISSET ){
						sProp.iMode = PH7_NATIVE_PROP_ISSET;
					}else if( pInstr->iP2 == PH7_MEMBER_EMPTY ){
						/* php's empty() asks has_property with a non-zero
						 * check_empty and takes THAT for the answer -- it never
						 * reads the value. The two are not the same question:
						 * `empty($row->queryString)` is TRUE on a name the
						 * handler does not know, while reading it works. */
						sProp.iMode = PH7_NATIVE_PROP_EXISTS;
					}else{
						sProp.iMode = PH7_NATIVE_PROP_READ;
					}
					sProp.pName = &sName;
					sProp.pResult = &sPropVal;
					sProp.bAnswered = 0;
					sProp.zThrowClass = 0;
					sProp.zThrowMsg[0] = 0;
					sProp.iThrowCode = 0;
					if( PH7_ClassNativeProp(pThis,&sProp) ){
						if( sProp.zThrowClass == 0 && bPropCoal
						 && sProp.iMode == PH7_NATIVE_PROP_READ
						 && (sPropVal.iFlags & MEMOBJ_NULL) ){
							/* `$o->p ??= v` on a name that reads null: the store php
							 * skips for a non-null one is the one it now makes, so the
							 * refusal is the WRITE handler's. */
							sProp.iMode = PH7_NATIVE_PROP_WRITE;
							sProp.bAnswered = 0;
							PH7_ClassNativeProp(pThis,&sProp);
						}
						if( sProp.zThrowClass ){
							/* Parked, like every other refusal this op makes: the
							 * fetch-point router lands it and abandons this slot. */
							VmBoundaryPark(&(*pVm),
								VmThrowFixedErrorCode(&(*pVm),sProp.zThrowClass,
									sProp.iThrowCode,sProp.zThrowMsg));
							PH7_MemObjRelease(&sPropVal);
							VmPopOperand(&pTos,1);      /* the property name */
							PH7_MemObjRelease(pTos);    /* the object slot is the answer */
							pTos->nIdx = SXU32_HIGH;
							VM_EXIT_BREAK;
						}
						VmPopOperand(&pTos,1);          /* the property name */
						pThis->iRef++;
						PH7_MemObjRelease(pTos);
						if( sProp.iMode == PH7_NATIVE_PROP_ISSET ){
							/* isset() only tests null-ness: a non-null marker for
							 * true, NULL for false — what the __isset arm pushes. */
							if( ph7_value_to_bool(&sPropVal) ){
								pTos->x.iVal = 1;
								MemObjSetType(pTos,MEMOBJ_BOOL);
							}
						}else if( sProp.iMode != PH7_NATIVE_PROP_UNSET ){
							/* EMPTY pushes the handler's BOOL, which the op then
							 * judges for emptiness -- the same answer php takes. */
							PH7_MemObjStore(&sPropVal,pTos);
						}
						pTos->nIdx = SXU32_HIGH;   /* a value, never an lvalue */
						PH7_MemObjRelease(&sPropVal);
						PH7_ClassInstanceUnref(pThis);
						VM_EXIT_BREAK;
					}
					PH7_MemObjRelease(&sPropVal);
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
					int bUnsAccessible = pEntry ? PH7_VmClassAttrAccess(&(*pVm),pClass,pObjAttr->pAttr,FALSE) : 0;
					ph7_class_attr *pUnsNoWrite = pEntry ? pObjAttr->pAttr
						: PH7_ClassScopedAttribute(&(*pVm),pClass,sName.zString,sName.nByte);
					sxi32 rcUnsRo = SXRET_OK;
					if( pEntry && (pObjAttr->iState & VM_CLASS_ATTR_RDONLY) != 0 ){
						/* php's read-only handler answers an unset with the same sentence
						 * it answers a store: `Property p is read only`. */
						VmBoundaryPark(&(*pVm),VmThrowNativeReadOnly(&(*pVm),pObjAttr->pAttr));
					}else if( pUnsNoWrite && (pUnsNoWrite->iFlags & (PH7_CLASS_ATTR_NATIVE_NOWRITE
					                                                |PH7_CLASS_ATTR_NATIVE_NOSLOT)) != 0 ){
						/* php's own unset handler for this class refuses, and words it
						 * without either "readonly" or "property": `Cannot unset C::$p`.
						 * It runs whether the object has a struct or not, so an
						 * unconstructed one -- which holds no slot at all -- refuses too
						 * rather than falling through to the missing-property no-op.
						 * A VIRTUAL property (NOSLOT) is the same answer for the same
						 * reason: php's unset_property handler for one has nothing to
						 * remove, so `unset($doc->preserveWhiteSpace)` is this Error and
						 * not the silent no-op a name the object lacks would take. */
						VmBoundaryPark(&(*pVm),
							VmThrowNativeNoUnset(&(*pVm),pThis->pClass,pUnsNoWrite));
					}else if( pEntry && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET) != 0 ){
						/* A native class's property is php's own C struct field, and
						 * unset() is a std handler that looks for a REAL property and
						 * finds none: nothing happens, nothing is said, and the next
						 * read still answers the struct. Removing the slot here left
						 * `unset($i->y); $i->y` an Undefined property warning and NULL
						 * for a statement php ignores. */
					}else if( pEntry && (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET|PH7_CLASS_ATTR_HOOK_SET)) != 0 ){
						/* php 8.4: a hooked property (virtual or backed) can never be
						 * unset — catchable Error, even from inside its own hook body
						 * (probe-verified). */
						SyBlob sErrMsg;
						SyBlobInit(&sErrMsg,&pVm->sAllocator);
						SyBlobFormat(&sErrMsg,"Cannot unset hooked property %z::$%z",
							&pThis->pClass->sName,&pObjAttr->pAttr->sName);
						VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));
					}else if( pEntry && bUnsAccessible
					       && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_READONLY) != 0
					       && (rcUnsRo = VmCheckReadonlyUnset(&(*pVm),pThis->pClass,pObjAttr)) != SXRET_OK ){
						/* php refuses to destroy a readonly property: an initialized
						 * one from every scope, and an uninitialized one from a scope
						 * that may not write it. Deleting it here re-armed the
						 * write-once latch, so a `readonly` value could be replaced by
						 * anything in two statements. The refusal is thrown inside the
						 * check; parking it is what routes it like every other one
						 * raised from this opcode. */
						VmBoundaryPark(&(*pVm),rcUnsRo);
					}else if( pEntry && bUnsAccessible ){
						if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0
						 && (pObjAttr->iState & VM_CLASS_ATTR_REFBOUND) == 0 ){
							/* A TYPED property keeps its DECLARATION: php's unset makes
							 * it uninitialized, so var_dump still names it
							 * `uninitialized(T)`, a read is "must not be accessed before
							 * initialization" rather than an Undefined property, and a
							 * later write lands back in its declared position instead of
							 * appending a dynamic one at the end. The seven other
							 * presentation surfaces leave an uninitialized property out,
							 * which is what made the delete look right. */
							ph7_value *pUnsSlot = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);
							if( pUnsSlot ){
								PH7_MemObjRelease(pUnsSlot);
								MemObjSetType(pUnsSlot,MEMOBJ_NULL);
							}
							pObjAttr->iState |= VM_CLASS_ATTR_UNINIT;
						}else{
							PH7_VmReleaseInstanceAttr(&(*pVm),pObjAttr);
							/* Through the instance's own door: a `foreach`/`array_walk`
							 * standing one property short of this one is holding the
							 * entry about to be freed. */
							PH7_ClassInstanceDeleteAttrEntry(pThis,pEntry);
						}
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
				if( pObjAttr == 0 ){
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
						ph7_class_attr *pDecl = PH7_ClassScopedAttribute(&(*pVm),pThis->pClass,sName.zString,sName.nByte);
						if( pDecl && PH7_ATTR_LAZY_ABSENT(pDecl,pThis) ){
							/* The object has never held this name. A class whose handler
							 * REFUSES every write answers the same sentence with or
							 * without a struct, and nothing is created; otherwise php's
							 * own write goes to the standard handler and CREATES a
							 * dynamic property beside the struct, which PHL refuses
							 * (§10) -- so fall through to the dynamic branch and let it.
							 * Once the constructor has installed the set, an `unset()`
							 * and a re-write are the ordinary declared path again. */
							if( pDecl->iFlags & PH7_CLASS_ATTR_NATIVE_NOWRITE ){
								VmBoundaryPark(&(*pVm),
									VmThrowNativeNoWrite(&(*pVm),pThis->pClass,pDecl));
								VmPopOperand(&pTos,1);    /* pop the attribute name */
								PH7_MemObjRelease(pTos);  /* the object slot becomes the answer */
								pTos->nIdx = SXU32_HIGH;
								VM_EXIT_BREAK;
							}
							pDecl = 0;
						}
						if( pDecl && (pDecl->iFlags & PH7_CLASS_ATTR_NATIVE_NOSLOT) != 0 ){
							/* A VIRTUAL property: php keeps no slot to re-create, and its
							 * write goes to the class's own write_property handler, which
							 * the rails below reach through PH7_ClassNativePropOwns.
							 * Creating one would give the object a real property php has
							 * none of, and would take the read with it
							 * (`$doc->formatOutput = false` then answered out of the slot
							 * rather than out of the extension's state). */
							pDecl = 0;
						}
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
							/* A name the class's OWN property-handler table carries is
							 * overloaded exactly the way a `__set` class's is -- php's
							 * write_property stands where `__set` would -- so every rail
							 * below takes it, and the one door they all end at
							 * (VmMagicSetDispatch) asks the handler first. */
							int bOwnedSet = PH7_ClassNativePropOwns(pClass,&sName);
							if( bPlainStore || pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){
								pSetMagic = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1);
							}
							if( (pSetMagic || bOwnedSet) && pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){
								/* php dispatches __set($name, element) for a missing
								 * destructuring target, but the element value only exists
								 * at the following OP_LOAD_LIST — the dispatch is
								 * unsupported (recorded residual). Leave the miss: the
								 * property stays uncreated, matching php's observable
								 * state (its __set did not store either). */
							}else if( (bOwnedSet && bPlainStore)
							 || (pSetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'s')) ){
								pThis->iRef++;
								pVm->pMagicSetThis = pThis;
								SyBlobReset(&pVm->sMagicSetName);
								SyBlobAppend(&pVm->sMagicSetName,(const void *)sName.zString,sName.nByte);
								/* pObjAttr stays NULL; the miss path below stays silent. */
							}else if( !bPlainStore && pInstr->iP2 == PH7_MEMBER_WRITE
							 && pNext->iOp == PH7_OP_NULLC_JMP
							 && (bOwnedSet
							  || (pCoalIsset = PH7_ClassExtractMethod(pClass,"__isset",sizeof("__isset")-1)) != 0
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
								if( !bMiss && (pCoalGet || bOwnedSet)
								 && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){
									VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');
									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sTest);
									VmMagicGuardPop(pVm);
								}
								if( (pCoalSet || bOwnedSet)
								 && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'s')
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
							}else if( !VmClassAllowsDynamicProps(pVm,pThis->pClass) ){
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
				if( pObjAttr == 0 && pInstr->iP2 == PH7_MEMBER_DEFPATH && pNos->nIdx != SXU32_HIGH
				 && !(PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1)
				   && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g')) ){
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
				if( pObjAttr == 0
				 && (pInstr->iP2 == PH7_MEMBER_READ || pInstr->iP2 == PH7_MEMBER_COALESCE) ){
					/* A LAZY native property whose class answers a read from its ZEROED
					 * struct (DatePeriod), asked before anything installed the set. php
					 * consults that read handler in the plain read AND in `??` -- its third
					 * accessor level takes the property's VALUE, so `$p->recurrences ?? 'd'`
					 * is 0 there -- while isset()/empty() go to the has_property handler,
					 * which answers false for an object that has no struct at all. */
					ph7_class_attr *pLz = PH7_ClassExtractAttribute(pClass,
						SyStringData(&sName),SyStringLength(&sName));
					if( pLz && (pLz->iFlags & PH7_CLASS_ATTR_NATIVE_LAZY_DEFAULT)
					 && PH7_ATTR_LAZY_ABSENT(pLz,pThis) ){
						pLazyDefault = pLz;
					}
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
							PH7_VmOverloadedPropNotice(&(*pVm),pClass,&sName,&sMagicRet);
						}else if( pInstr->iP2 == PH7_MEMBER_DEFPATH ){
							/* A deferred call ARGUMENT: __get has answered — php calls it
							 * where the property is WRITTEN, whatever the parameter turns
							 * out to be — and only the by-ref verdict is still pending.
							 * Carry the value with the class and name that produced it, so
							 * the notice lands at the call for a by-REFERENCE parameter and
							 * nothing is said for a by-value one, without __get running
							 * twice or a second read arriving after a later argument's side
							 * effects. */
							VmDeferredPath *pPre = VmDeferPathNewPrefetch(&(*pVm),VM_OVER_PROP,
								pClass,&sName,&sMagicRet);
							if( pPre ){
								VmPopOperand(&pTos,1);   /* drop the property name */
								pThis->iRef++;
								PH7_MemObjRelease(pTos); /* collapse the object slot into the carrier */
								pTos->x.pOther = pPre;
								pTos->iFlags = MEMOBJ_NULL | MEMOBJ_AUX_DEFPATH;
								pTos->nIdx = SXU32_HIGH;
								PH7_MemObjRelease(&sMagicRet);
								PH7_ClassInstanceUnref(pThis);
								VM_EXIT_BREAK;
							}
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
						/* A property missing from the INSTANCE may still be DECLARED by the
						 * class — that is what `unset($o->p)` leaves behind, and php keeps
						 * answering for the declaration rather than calling the name
						 * undefined: the visibility screen still applies, and a TYPED slot
						 * is back to uninitialized (the same Error a never-written one
						 * raises). Only a name the class does not declare at all is the
						 * "Undefined property" warning. A destructuring target is silent
						 * either way: php created the property above or dispatched __set. */
						ph7_class_attr *pDeclAttr = PH7_ClassScopedAttribute(&(*pVm),pClass,
							SyStringData(&sName),SyStringLength(&sName));
						/* A static property, a class constant and a native engine slot are
						 * not instance properties; a dynamic one is gone for good once it
						 * is unset, since nothing declares it. */
						if( pDeclAttr
						 && (pDeclAttr->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT
						                          |PH7_CLASS_ATTR_HIDDEN|PH7_CLASS_ATTR_DYNAMIC)) ){
							pDeclAttr = 0;
						}
						if( pDeclAttr && PH7_ATTR_LAZY_ABSENT(pDeclAttr,pThis) ){
							/* The object has never held this name. php has two answers and
							 * the class says which: a read handler over the ZEROED struct
							 * (DatePeriod -- null/0/false, in silence), or nothing at all
							 * (DateInterval), which is the ordinary "Undefined property"
							 * warning. Either way the DECLARATION is not what answers, so
							 * the typed-slot Error below must not fire on a name php keeps
							 * no slot for. */
							if( pDeclAttr->iFlags & PH7_CLASS_ATTR_NATIVE_LAZY_DEFAULT ){
								pLazyDefault = pDeclAttr;
							}
							pDeclAttr = 0;
						}
						if( pDeclAttr && (pDeclAttr->iFlags & PH7_CLASS_ATTR_NATIVE_NOSLOT) != 0 ){
							/* ...and a VIRTUAL property is answered by the class's handler
							 * too, so the DECLARATION must not raise the typed-slot Error
							 * for a name php keeps no slot for either. */
							pDeclAttr = 0;
						}
						if( pLazyDefault ){
							/* php's read handler answered: say nothing. */
						}else if( pDeclAttr
						 && !PH7_VmClassAttrAccess(&(*pVm),pClass,pDeclAttr,FALSE) ){
							SyBlob sErrMsg;
							const char *zVis = pDeclAttr->iProtection == PH7_CLASS_PROT_PRIVATE
								? "private" : "protected";
							SyBlobInit(&sErrMsg,&pVm->sAllocator);
							SyBlobFormat(&sErrMsg,"Cannot access %s property %z::$%z",
								zVis,&pClass->sName,&sName);
							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",
								sizeof("Error")-1,&sErrMsg));
						}else if( pDeclAttr && (pDeclAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){
							VmBoundaryPark(&(*pVm),
								VmThrowUninitializedPropertyError(&(*pVm),pClass,pDeclAttr));
						}else{
							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined property: %z::$%z",
								&pClass->sName,&sName);
						}
					}
				}
				if( pObjAttr && (pObjAttr->iState & VM_CLASS_ATTR_RDONLY)
				 && (pInstr->iP2 == PH7_MEMBER_LIST_TARGET
				  || ((pInstr + 1)->iOp == PH7_OP_STORE && (pInstr + 1)->iP2 != 0)) ){
					/* php's write_property handler refuses the plain store and the
					 * destructuring one that goes through it; everything that takes a
					 * POINTER to the property instead -- a compound assign, `++`, `??=`,
					 * a reference bind -- bypasses the handler in php and is left alone
					 * here too. */
					VmBoundaryPark(&(*pVm),VmThrowNativeReadOnly(&(*pVm),pObjAttr->pAttr));
					VmPopOperand(&pTos,1);
					PH7_MemObjRelease(pTos);
					pTos->nIdx = SXU32_HIGH;
					VM_EXIT_BREAK;
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
					if( pObjAttr && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_NOWRITE) ){
						/* A reference bind is a WRITE, and php's refusing handler answers
						 * it with the same sentence a plain store gets. */
						VmBoundaryPark(&(*pVm),
							VmThrowNativeNoWrite(&(*pVm),pObjAttr->pOwner,pObjAttr->pAttr));
						pVm->pRefTargetAttr = 0;
						pVm->pRefTargetThis = 0;
						pVm->pRefTargetStaticAttr = 0;
						PH7_ClassInstanceUnref(pThis);
					}else if( pObjAttr && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET) ){
						/* `$i->f =& $x`: php REFUSES to make a handler-backed property
						 * the target of a reference — there is no slot to rebind, and
						 * every write through the alias would skip the conversion the
						 * handler is there to do. PHL rebound the slot instead, so
						 * `$x = 2.5` afterwards wrote 2.5 microseconds-free into the
						 * interval. */
						SyBlob sErrMsg;
						SyBlobInit(&sErrMsg,&pVm->sAllocator);
						SyBlobAppend(&sErrMsg,"Cannot assign by reference to overloaded object",
							sizeof("Cannot assign by reference to overloaded object")-1);
						VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));
						pVm->pRefTargetAttr = 0;
						pVm->pRefTargetThis = 0;
						pVm->pRefTargetStaticAttr = 0;
						PH7_ClassInstanceUnref(pThis);
					}else if( pObjAttr ){
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
				if( pLazyDefault && pLazyDefault->pNativeValue ){
					/* A LAZY native property read before its class installed the set,
					 * on a class that answers such a read from its ZEROED struct. The
					 * declared literal IS that struct's field (the 76th session moved
					 * DatePeriod's seven defaults onto it), and the answer is a
					 * TEMPORARY: nothing was installed, so there is no slot to address
					 * and nIdx stays the constant sentinel the pop left. */
					PH7_NativeLiteralValue(&(*pVm),pLazyDefault->pNativeValue,pTos);
				}
				if( pObjAttr ){
					ph7_value *pValue = 0; /* cc warning */
					/* Check attribute access */
					if( PH7_VmClassAttrAccess(&(*pVm),pClass,pObjAttr->pAttr,FALSE) ){
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
									if( pInstr->iP2 == PH7_MEMBER_DEFPATH ){
										/* A deferred call ARGUMENT of a HOOKED property. The
										 * get hook has answered, as php's does; what a
										 * by-REFERENCE parameter then gets is not a notice
										 * but php's refusal — a hook has no slot to alias
										 * and `&get` does not exist here — while a by-VALUE
										 * one simply takes the hook's value. */
										VmDeferredPath *pPre = VmDeferPathNewPrefetch(&(*pVm),
											VM_OVER_HOOK,pThis->pClass,&pObjAttr->pAttr->sName,&sHookRet);
										if( pPre ){
											PH7_MemObjRelease(pTos);
											pTos->x.pOther = pPre;
											pTos->iFlags = MEMOBJ_NULL | MEMOBJ_AUX_DEFPATH;
											pTos->nIdx = SXU32_HIGH;
											PH7_MemObjRelease(&sHookRet);
											PH7_ClassInstanceUnref(pThis);
											VM_EXIT_BREAK;
										}
									}
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
						/* `$o->p[$k] = v`, `$o->p[] = v` and `unset($o->p[$k])`: the
						 * property is the BASE of a subscript write, so the write lands
						 * inside whatever it holds. php screens that where it screens a
						 * store -- `Cannot indirectly modify readonly property C::$p` --
						 * and it screens it BEFORE the uninitialized-typed read below,
						 * which is why this sits in front. PHL wrote through to the
						 * array a readonly property held. */
						{
							VmInstr *pNextI = pInstr + 1;
							int bStoreI = (pNextI->iOp == PH7_OP_STORE && pNextI->iP2 != 0);
							int bCoalI = pNextI->iOp == PH7_OP_NULLC_JMP;
							int bUnsetBase = pInstr->iP2 == PH7_MEMBER_READ
								&& pNextI->iOp == PH7_OP_LOAD_IDX && VM_IDX_IS_UNSET(pNextI->iP2);
							int bBaseW = bUnsetBase
								|| (pInstr->iP2 == PH7_MEMBER_WRITE
								    && !bStoreI && !bCoalI && !VmMemberNextIsWrite(pNextI));
							if( bBaseW ){
								sxi32 rcInd = PH7_VmCheckIndirectModify(&(*pVm),pObjAttr->nIdx);
								if( rcInd != SXRET_OK ){
									VmBoundaryPark(&(*pVm),rcInd);
									PH7_MemObjRelease(pTos);
									pTos->nIdx = SXU32_HIGH;
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
							/* A WRITE base is not a read either. `$o->p ??= v` takes the
							 * slot's NULL and assigns over it, and a DIMENSION write
							 * (`$o->t['n'] = v`) AUTO-INITIALIZES an array when the
							 * declared type has room for one -- php's
							 * zend_handle_fetch_obj_flags -- or refuses with its own
							 * TypeError when it does not. Raising the read Error here
							 * instead is what stopped Doctrine's ClassMetadata, whose
							 * `public array $table;` is filled exactly that way. */
							if( (pInstr + 1)->iOp == PH7_OP_NULLC_JMP ){
								bIsLhs = 1;   /* `$o->p ??= v` assigns over the unset slot */
							}else if( VmMemberFetchForWrite(pInstr) ){
								sxi32 rcAI = VmAutoInitArrayProperty(&(*pVm),pObjAttr,
									(ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx));
								if( rcAI != SXRET_OK ){
									VmBoundaryPark(&(*pVm),rcAI);
									PH7_ClassInstanceUnref(pThis);
									VM_EXIT_BREAK;
								}
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
							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET)
							 && !VmMemberNativeSetKeepsSlot(pInstr) ){
								pTos->nIdx = SXU32_HIGH;
								pTos->iFlags |= MEMOBJ_AUX_NATIVEPROP;
							}
						}
						if( pInstr->iP2 == PH7_MEMBER_ISSET ){
							/* isset() tests null-ness and nothing else, so reduce the loaded
							 * value to the same non-null marker the __isset and get-hook
							 * paths push. A property read off a TEMPORARY receiver
							 * (`isset(f()->p)`, `isset((new C)->p)`, and now every
							 * intermediate link of an accessor chain) leaves no variable
							 * index behind, and the trailing builtin read that as a
							 * CONSTANT and warned -- a diagnostic php has no equivalent of,
							 * its isset() being a language construct rather than a call. */
							int bSet = (pTos->iFlags & MEMOBJ_NULL) == 0;
							PH7_MemObjRelease(pTos);
							if( bSet ){
								pTos->x.iVal = 1;
								MemObjSetType(pTos,MEMOBJ_BOOL);
							}
							pTos->nIdx = SXU32_HIGH;
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
								PH7_VmOverloadedPropNotice(&(*pVm),pClass,&sName,&sMagicRet);
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
						/* A subclass reading a PARENT's PRIVATE property used to be
						 * special-cased into an "Undefined property" warning here. It is
						 * php's answer, but not because of the SCOPE: the base's private
						 * lives under its mangled storage name, so the subclass's plain
						 * lookup never reaches this point at all -- and reading the SAME
						 * property on an instance of the declaring class, which does, is
						 * php's ordinary visibility refusal. Deciding it from the scope
						 * turned that one into a warning too. */
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
			if( (pNos->iFlags & MEMOBJ_AUX_STROFFSET)
			 && (pInstr->iP2 == PH7_MEMBER_WRITE || pInstr->iP2 == PH7_MEMBER_UNSET
			  || pInstr->iP2 == PH7_MEMBER_LIST_TARGET || pInstr->iP2 == PH7_MEMBER_REF_TARGET
			  /* A reference SOURCE (`$r =& $s[0]->p`) is compiled as a READ here on
			   * purpose — php hands back a copy for a handler-backed property — so the
			   * bind that follows is what makes it a reach-inside, exactly as it does
			   * for a subscript. */
			  || (pInstr->iP2 == PH7_MEMBER_READ
			      && ((pInstr + 1)->iOp == PH7_OP_STORE_REF
			       || (pInstr + 1)->iOp == PH7_OP_LOAD_REF
			       || (pInstr + 1)->iOp == PH7_OP_STORE_IDX_REF))) ){
				/* The base is a string OFFSET and this reaches INSIDE it. php refuses every
				 * such reach and words the refusal from what is doing the reaching, so a
				 * PROPERTY is `Cannot use string offset as an object` where a subscript is
				 * `... as an array` — `$s[0]->p = 1`, `$s[0]->p += 1`, `$s[0]->p ??= 1` and
				 * `unset($s[0]->p)` alike. PHL reported the generic non-object write
				 * (`Attempt to assign property "p" on string`) and said nothing at all for
				 * the unset. A METHOD CALL is not one of these: php keeps `Call to a member
				 * function p() on string` there, and so does the path below. */
				sxi32 rcSo;
				VmPopOperand(&pTos,1);
				PH7_MemObjRelease(pTos);
				MemObjSetType(pTos,MEMOBJ_NULL);
				pTos->nIdx = SXU32_HIGH;
				rcSo = VmThrowFromVm(&(*pVm),"Error","Cannot use string offset as an object",
					sizeof("Cannot use string offset as an object")-1);
				if( rcSo == SXERR_ABORT ){ VM_EXIT_ABORT; }
				rc = rcSo;
				PH7_THROW_ROUTE_MIDEXPR(rc)
			}
			/* `->` on a non-object (e.g. a null intermediate). Silent in isset()/empty()/unset()
			 * context (iP2 2/3/4) so `isset($o->missing->x)` / `unset($o->missing->x)` match PHP. */
			if( pInstr->iP2 != PH7_MEMBER_UNSET && !VmMemberCtxIsLookup(pInstr->iP2) ){
				/* php draws a sharp line here, and PH7 drew none: CALLING a method on a
				 * non-object is a catchable Error, while READING a property off one is a
				 * Warning that yields NULL. PH7 raised the same notice for both and
				 * carried on with NULL, so `$null->m()` silently did nothing. */
				SyString sMemb;
				const char *zVerb = 0;
				SyStringInitFromBuf(&sMemb,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));
				if( pInstr->iP2 == PH7_MEMBER_DEFPATH && pNos->nIdx != SXU32_HIGH ){
					/* A deferred call ARGUMENT (`f($u->p)`): which diagnostic php raises
					 * depends on the parameter, and this op does not know it — a by-VALUE
					 * binding is the read warning below, a by-REFERENCE one is php's
					 * `Attempt to modify property` Error. Record the step rooted at the
					 * base and let OP_CALL re-drive it in the mode the callee decides,
					 * exactly as a property MISSING on a real object is recorded. */
					VmDeferredPath *pPath = VmDeferPathNew(&(*pVm),0,pNos->nIdx,0);
					if( pPath && VmDeferPathPushProp(pPath,&sMemb) == SXRET_OK ){
						VmPopOperand(&pTos,1);   /* drop the property name */
						PH7_MemObjRelease(pTos); /* collapse the base slot into the carrier */
						pTos->x.pOther = pPath;
						pTos->iFlags = MEMOBJ_NULL | MEMOBJ_AUX_DEFPATH;
						pTos->nIdx = SXU32_HIGH;
						VM_EXIT_BREAK;
					}
					if( pPath ){
						VmFreeDeferredPath(pPath);
					}
					/* fall through to the immediate warning on allocation failure */
				}
				/* WRITING one is an Error too, and php picks its verb from what the
				 * write actually is. PH7 warned about a READ it never performed and
				 * then let the store fail into its own
				 * "Cannot perform assignment on a constant class attribute", so the
				 * script carried on past a statement php stops it for. The kind is
				 * read off the following instruction, the way every other write
				 * classification in this handler is (VmMemberFetchForWrite). */
				if( pInstr->iP2 == PH7_MEMBER_WRITE ){
					const VmInstr *pNextW = pInstr + 1;
					if( (pNextW->iOp == PH7_OP_STORE && pNextW->iP2 != 0)
					 || pNextW->iOp == PH7_OP_NULLC_JMP /* `$u->p ??= v` */
					 || VmNextIsCompoundAssign(pNextW) ){
						zVerb = "assign";
					}else if( pNextW->iOp == PH7_OP_INCR || pNextW->iOp == PH7_OP_DECR ){
						zVerb = "increment/decrement";
					}else{
						/* The base of a subscript write (`$u->p[] = v`): php asks the
						 * property for something to modify, and there is no property. */
						zVerb = "modify";
					}
				}else if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){
					zVerb = "assign";
				}else if( pInstr->iP2 == PH7_MEMBER_REF_TARGET ){
					zVerb = "modify";
				}
				if( pInstr->iP2 == PH7_MEMBER_METHOD || zVerb ){
					SyBlob sErrM;
					sxi32 rcErr;
					SyBlobInit(&sErrM,&pVm->sAllocator);
					if( zVerb ){
						SyBlobFormat(&sErrM,"Attempt to %s property \"%z\" on %s",
							zVerb,&sMemb,VmArithValueName(pNos));
					}else{
						SyBlobFormat(&sErrM,"Call to a member function %z() on %s",
							&sMemb,VmArithValueName(pNos));
					}
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
					&sMemb,VmArithValueName(pNos));
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
				/* The static twin of the name rule at the top of this handler, and it
				 * runs HERE rather than up there because php resolves the CLASS first:
				 * `NoSuchClass::${$arr}` is the class refusal alone, with no coercion
				 * and no `Array to string conversion` behind it, while the same name on
				 * a class that exists warns and then reports `Access to undeclared
				 * static property C::$Array`. A `::` METHOD name is refused the same way
				 * an instance one is -- once the class is known. */
				if( !pInstr->p3 ){
					if( pInstr->iP2 == PH7_MEMBER_METHOD ){
						if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){
							sxi32 rcMn;
							VmPopOperand(&pTos,1);
							PH7_MemObjRelease(pTos);
							MemObjSetType(pTos,MEMOBJ_NULL);
							pTos->nIdx = SXU32_HIGH;
							if( pThis ){
								PH7_ClassInstanceUnref(pThis);
								pThis = 0;
							}
							rcMn = VmThrowFromVm(&(*pVm),"Error","Method name must be a string",
								sizeof("Method name must be a string")-1);
							if( rcMn == SXERR_ABORT ){ VM_EXIT_ABORT; }
							rc = rcMn;
							PH7_THROW_ROUTE_MIDEXPR(rc)
						}
					}else{
						sxi32 rcNm = PH7_MemObjToStringUV(pTos);
						if( rcNm != SXRET_OK ){
							VmPopOperand(&pTos,1);
							PH7_MemObjRelease(pTos);
							MemObjSetType(pTos,MEMOBJ_NULL);
							pTos->nIdx = SXU32_HIGH;
							if( pThis ){
								PH7_ClassInstanceUnref(pThis);
								pThis = 0;
							}
							if( rcNm == PH7_ABORT ){ VM_EXIT_ABORT; }
							rc = rcNm;
							PH7_THROW_ROUTE_MIDEXPR(rc)
						}
					}
					/* The coercion rewrote the slot the name was read from. */
					SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),
						SyBlobLength(&pTos->sBlob));
				}
				if( pInstr->iP2 == PH7_MEMBER_METHOD ){
					/* Method call */
					ph7_class_method *pMeth = 0;
					if( sName.nByte > 0 ){
						/* An INTERFACE's methods are looked up too: they are all abstract,
						 * so the arm below reports php's "Cannot call abstract method
						 * I::m()" rather than claiming the name does not exist. */
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
							/* The `$obj::m()` form took a reference on the receiver at the
							 * top of this branch; every exit has to give it back, and only
							 * the fall-through at the end of the arm used to. */
							if( pThis ){
								PH7_ClassInstanceUnref(pThis);
								pThis = 0;
							}
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
								VmMagicCall *pPend = VmMagicCallNew(&(*pVm),pMagicThis,
									pMagicThis ? pMagicThis->pClass : pClass,&sName);
								if( pPend == 0 ){
									VM_EXIT_ABORT;
								}
								if( !pInstr->p3 ){
									VmPopOperand(&pTos,1);
								}
								PH7_MemObjRelease(pTos);
								if( pThis ){
									PH7_ClassInstanceUnref(pThis);
									pThis = 0;
								}
								pTos->x.pOther = pPend;
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
								if( pThis ){
									PH7_ClassInstanceUnref(pThis);
									pThis = 0;
								}
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
							ph7_class *pScope = PH7_VmCallerScope(&(*pVm));
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
							if( pThis ){
								PH7_ClassInstanceUnref(pThis);
								pThis = 0;
							}
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
							VmMagicCall *pPend = VmMagicCallNew(&(*pVm),pDeniedThis,
								pDeniedThis ? pDeniedThis->pClass : pClass,&sName);
							if( pPend == 0 ){
								VM_EXIT_ABORT;
							}
							if( !pInstr->p3 ){
								VmPopOperand(&pTos,1);
							}
							PH7_MemObjRelease(pTos);
							if( pThis ){
								PH7_ClassInstanceUnref(pThis);
								pThis = 0;
							}
							pTos->x.pOther = pPend;
							pTos->iFlags = MEMOBJ_NULL|MEMOBJ_AUX_MAGICCALL;
							pTos->nIdx = SXU32_HIGH;
							VM_EXIT_BREAK;
						}
						/* php refuses a NON-STATIC method named through `::` unless the
						 * CALLING frame holds a `$this` the class accepts: that is what
						 * makes `self::`/`parent::`/`static::` and `C::m()` from inside a
						 * C method work, and every other spelling — from global scope,
						 * from a static method, from an unrelated class, and `$obj::m()`
						 * — an Error. PHL ran the body instead, with `$this` unset or
						 * (for the object form) bound to the object the `::` was written
						 * on, which php never uses: it takes the CALLER's. The message
						 * names the DECLARING class, so `D::m()` reports B::m(). */
						if( (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){
							ph7_class_instance *pCallerThis = PH7_VmCallerThisFor(&(*pVm),pClass);
							if( pCallerThis == 0 ){
								SyBlob sErrM;
								sxi32 rcErr;
								ph7_class *pOwner = PH7_VmMethodScopeName(&(*pVm),pClass,pMeth);
								SyBlobInit(&sErrM,&pVm->sAllocator);
								SyBlobFormat(&sErrM,"Non-static method %z::%z() cannot be called statically",
									&pOwner->sName,&pMeth->sFunc.sName);
								if( !pInstr->p3 ){
									VmPopOperand(&pTos,1);
								}
								PH7_MemObjRelease(pTos);
								pTos->nIdx = SXU32_HIGH;
								if( pThis ){
									PH7_ClassInstanceUnref(pThis);
									pThis = 0;
								}
								rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),
									SyBlobLength(&sErrM));
								SyBlobRelease(&sErrM);
								if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }
								rc = rcErr;
								PH7_THROW_ROUTE_MIDEXPR(rc)
							}
							/* The receiver is that `$this` — never the object the `::`
							 * was written on, and never nothing. Naming it here is also
							 * what makes the call work inside a CLOSURE declared in a
							 * method: php gives such a closure a `$this`, PHL carries it
							 * as a frame variable rather than on the frame itself, and
							 * OP_CALL reads only the frame, so `self::m()` in a closure
							 * used to run with no receiver at all. */
							PH7_MemObjRelease(pNos);
							pNos->x.pOther = pCallerThis;
							MemObjSetType(pNos,MEMOBJ_OBJ);
							pCallerThis->iRef++;
							pNos->nIdx = SXU32_HIGH;
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
						/* php gives a class CONSTANT no silent-lookup mode at all: there is
						 * no BP_VAR_IS fetch for one, so `empty(C::K)` and `C::K ?? $d` raise
						 * the same Error a plain read does -- undefined OR inaccessible --
						 * where the same two shapes over a static PROPERTY answer quietly.
						 * PHL silenced both, so a typo'd or private class constant read
						 * through `??` handed back the default. (isset() over one is a php
						 * COMPILE error, so it never reaches this.) */
						int bConstForm = (pInstr->p3 == 0 && pInstr->iP1 != 2);
						if( sName.nByte > 0 ){
							pAttr = bConstForm
								? PH7_ClassExtractConstant(pClass,sName.zString,sName.nByte)
								: VmClassAttrWithShadow(pClass,sName.zString,sName.nByte);
						}
						if( pAttr && bConstForm && (pClass->iFlags & PH7_CLASS_TRAIT) != 0
						 && !VmMemberCtxIsLookup(pInstr->iP2) ){
							/* A trait constant belongs to the classes that COMPOSE the trait
							 * and to no one else: php refuses `T::K` outright, from inside a
							 * trait method as readily as from outside. (PHP 8.2 introduced
							 * trait constants; this refusal came with them.) */
							SyBlob sErrTr;
							SyBlobInit(&sErrTr,&pVm->sAllocator);
							SyBlobFormat(&sErrTr,"Cannot access trait constant %z::%z directly",
								&pClass->sName,&pAttr->sName);
							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",
								sizeof("Error")-1,&sErrTr));
							pAttr = 0;
							bStaticHidden = 1;
						}
						if( pAttr && !bConstForm
						 && (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT))
						     != PH7_CLASS_ATTR_STATIC ){
							/* php's `::$name` reads the class's PROPERTY table -- which holds
							 * the INSTANCE properties and the constants too -- and answers in
							 * a fixed order: visibility first, then static-ness. So a private
							 * instance property is `Cannot access private property H::$pi`,
							 * a public one and a CONSTANT named with the `$` form are
							 * `Access to undeclared static property H::$inst`, and neither
							 * ever yields a value: the static table simply has no such row.
							 * PHL matched only the constant case; an instance property
							 * reached through `::` printed PH7's own uncatchable "Access to a
							 * non-static class attribute ... PH7 is loading NULL" and CARRIED
							 * ON -- reading null, and letting `H::$inst = 'w'` report success
							 * for a write php refuses. Fold it into the not-found arm below,
							 * raising the visibility refusal first when that is what php
							 * answers. */
							if( !VmMemberCtxIsLookup(pInstr->iP2)
							 && !PH7_VmClassAttrAccess(&(*pVm),pClass,pAttr,FALSE) ){
								SyBlob sErrVis;
								SyBlobInit(&sErrVis,&pVm->sAllocator);
								SyBlobFormat(&sErrVis,"Cannot access %s property %z::$%z",
									pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected",
									&pClass->sName,&pAttr->sName);
								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",
									sizeof("Error")-1,&sErrVis));
								bStaticHidden = 1;
							}
							pAttr = 0;
						}
						if( pAttr == 0 && !bStaticHidden ){
							/* No such member. php raises a catchable Error whose wording
							 * depends on the ACCESS form (the same p3/iP1 signal used above):
							 * a bareword `C::MISSING` (constant form) is "Undefined constant
							 * C::MISSING", a `$`-form `C::$missing` is "Access to undeclared
							 * static property C::$missing". Instance magic (__get) is never
							 * consulted for statics (band A #3b). isset()/empty() context
							 * stays silently false. Parked on the boundary rail; the op
							 * completes benignly with NULL and the fetch-point router lands
							 * the throw. */
							if( bConstForm || !VmMemberCtxIsLookup(pInstr->iP2) ){
								SyBlob sErrMsg;
								SyBlobInit(&sErrMsg,&pVm->sAllocator);
								if( bConstForm ){
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
							 && PH7_VmClassAttrAccess(&(*pVm),pClass,pAttr,FALSE) ){
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
							{
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
								if( PH7_VmClassAttrAccess(&(*pVm),pClass,pAttr,FALSE) ){
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
												/* isset()/empty()/`??` ask whether the property
												 * HAS a value and read an uninitialized one as
												 * "not set" -- a silent miss, not the Error a
												 * plain read raises. The instance path has had
												 * this since typed properties landed; the STATIC
												 * one never did, so `isset(C::$n)` threw where php
												 * answers false. (`??=` is the NULLC_JMP branch
												 * below -- it is a write base, not a lookup.) */
												if( VmMemberCtxIsLookup(pInstr->iP2) ){
													bIsLhs = 1;
												}
												/* And a WRITE base is not a read: same
												 * auto-initialize rule as the instance path. A
												 * read-MODIFY-write (`C::$n++`, `.=`) is not one
												 * of these -- it reads the property first, so
												 * php's Error stands. */
												if( (pInstr + 1)->iOp == PH7_OP_NULLC_JMP ){
													bIsLhs = 1;
												}else if( VmMemberFetchForWrite(pInstr) ){
													sxi32 rcAI = VmAutoInitArrayProperty(&(*pVm),pV,
														(ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx));
													if( rcAI != SXRET_OK ){
														VmBoundaryPark(&(*pVm),rcAI);
														if( pThis ){
															PH7_ClassInstanceUnref(pThis);
														}
														VM_EXIT_BREAK;
													}
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
								}else if( bConstForm || !VmMemberCtxIsLookup(pInstr->iP2) ){
									/* Denied by visibility. php's Error is CATCHABLE here
									 * exactly as it is for an instance property, and PHL
									 * reported it uncaught and ABORTED the script -- so a
									 * `try { C::$protectedStatic; } catch` never ran its
									 * catch, and everything after the try was dropped with
									 * exit status 0. Parked on the boundary rail like the
									 * instance twin; the op completes with the NULL already
									 * in the slot and the fetch-point router lands the throw.
									 * A lookup (isset/empty/`??`) stays silent and false, as
									 * php's is. The name is built with the ATTRIBUTE's own
									 * spelling, which is the declaration's. */
									SyBlob sErrVis;
									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE
										? "private" : "protected";
									SyBlobInit(&sErrVis,&pVm->sAllocator);
									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){
										SyBlobFormat(&sErrVis,"Cannot access %s constant %z::%z",
											zVis,&pClass->sName,&pAttr->sName);
									}else{
										SyBlobFormat(&sErrVis,"Cannot access %s property %z::$%z",
											zVis,&pClass->sName,&pAttr->sName);
									}
									VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",
										sizeof("Error")-1,&sErrVis));
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
			/* `$v::X` where $v holds neither an object nor a class NAME. php's
			 * catchable Error, which STOPS the statement; PH7 raised its own
			 * uncatchable wording and carried on with NULL, so a `$obj::$s = 1`
			 * through a null base went on to fail again in OP_STORE. */
			sxi32 rcCn;
			if( !pInstr->p3 ){
				VmPopOperand(&pTos,1);
			}
			PH7_MemObjRelease(pTos);
			MemObjSetType(pTos,MEMOBJ_NULL);
			pTos->nIdx = SXU32_HIGH;
			rcCn = VmThrowFromVm(&(*pVm),"Error","Class name must be a valid object or a string",
				sizeof("Class name must be a valid object or a string")-1);
			if( rcCn == SXERR_ABORT ){ VM_EXIT_ABORT; }
			rc = rcCn;
			PH7_THROW_ROUTE_MIDEXPR(rc)
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
	if( (pSrc->pClass->iFlags & PH7_CLASS_ENUM) || PH7_ClassIsUncloneable(pSrc->pClass)
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
