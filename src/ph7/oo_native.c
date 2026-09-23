/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
/*
 * Declaring a class from C.
 *
 * Every built-in class subsystem used to be an embedded PHP source string compiled
 * at VM init, reaching the engine through a GLOBAL C thunk per operation
 * (`__reflect_class_info()`, `__gen_next()`, `__dom_*`, ~110 of them). The reason
 * was structural: a ph7_class_method carries a ph7_vm_func, whose only body is
 * bytecode, so C code could only ever be a global function.
 *
 * VM_FUNC_NATIVE removed that restriction (a method body may be a C routine), and
 * this file is the front door to it: a declarative table describing a class —
 * parent, interfaces, constants, methods — that PH7_InstallNativeClasses() turns
 * into a real, mounted ph7_class. The thunks become what they always were,
 * methods, and stop being visible in the global namespace.
 *
 * Nothing here is new machinery. It drives the same builders the COMPILER drives
 * for `class Foo {}` — PH7_NewRawClass, PH7_NewClassMethod, PH7_ClassInstallMethod,
 * PH7_ClassInherit, PH7_ClassImplement, PH7_VmInstallClass, VmMountUserClass — so a
 * native class is not a second kind of class: Reflection, instanceof, inheritance,
 * visibility and autoloading all see an ordinary one.
 */
/*
 * Materialize a native declaration's literal initializer into a value slot.
 *
 * A compiled declaration expresses its default as byte-code evaluated at mount
 * (constants, statics) or at `new` (instance properties). The C builder has no
 * compiler to emit that, so it carries the literal on the attribute
 * (ph7_class_attr::pNativeValue) and both of those sites call this instead --
 * which is what a literal initializer's byte-code would have produced anyway.
 */
PH7_PRIVATE void PH7_NativeLiteralValue(ph7_vm *pVm,const void *pLiteral,ph7_value *pOut)
{
	const PH7_NativeConstDef *pLit = (const PH7_NativeConstDef *)pLiteral;
	/* Every PH7_MemObjInitFrom* below SyZeros the whole value, nIdx included — and
	 * a CONSTANT's or STATIC's slot is addressed by that index afterwards
	 * (`pAttr->nIdx = pMemObj->nIdx`). Losing it pointed every native class
	 * constant at slot 0, which reads as $GLOBALS. The instance-property path was
	 * unaffected (it records the index before calling this), which is why nothing
	 * saw it until the date family declared the first native constants. */
	sxu32 nSlot = pOut->nIdx;
	switch( pLit->iType ){
		case PH7_NATIVE_VAL_INT:
			PH7_MemObjInitFromInt(&(*pVm),pOut,pLit->iValue);
			break;
		case PH7_NATIVE_VAL_BOOL:
			PH7_MemObjInitFromBool(&(*pVm),pOut,(sxi32)pLit->iValue);
			break;
		case PH7_NATIVE_VAL_STRING: {
			SyString sLit;
			SyStringInitFromBuf(&sLit,pLit->zValue,SyStrlen(pLit->zValue));
			PH7_MemObjInitFromString(&(*pVm),pOut,&sLit);
			break;
		}
#ifndef PH7_OMIT_FLOATING_POINT
		case PH7_NATIVE_VAL_DOUBLE:
			PH7_MemObjInitFromReal(&(*pVm),pOut,pLit->rValue);
			break;
#endif
		default:
			PH7_MemObjInit(&(*pVm),pOut);
			break;
	}
	pOut->nIdx = nSlot;
}
/*
 * Write a declared property of an instance from C.
 *
 * Every native class that hands an OBJECT back to PHP has to fill one in, and the
 * write has to go through the instance's own slot table (hAttr -> VmClassAttr ->
 * pVm->aMemObj) rather than the class declaration, or the value lands nowhere.
 * Silently does nothing for a name the class does not declare -- callers pass
 * literals from their own spec table, so a miss is a build error, not input.
 */
PH7_PRIVATE void PH7_NativeSetProp(ph7_vm *pVm,ph7_class_instance *pObj,
	const char *zProp,sxu32 nProp,ph7_value *pSrcVal)
{
	SyHashEntry *pEntry = SyHashGet(&pObj->hAttr,(const void *)zProp,nProp);
	VmClassAttr *pVmAttr;
	ph7_value *pSlot;
	if( pEntry == 0 ){
		return;
	}
	pVmAttr = (VmClassAttr *)pEntry->pUserData;
	pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,pVmAttr->nIdx);
	if( pSlot == 0 ){
		return;
	}
	PH7_MemObjStore(pSrcVal,pSlot);
	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;
}
/*
 * ---------------------------------------------------------------------------
 * Reading and writing a native instance's own declared slots.
 *
 * A compiled method reaches `$this->p` through the byte-code that resolves the
 * attribute; a C body has to walk the instance's slot table itself. Every native
 * class needs the same six or seven moves, so they live here rather than being
 * re-declared per subsystem (the date family carried a private copy of the whole
 * set, which is what these replace).
 * ---------------------------------------------------------------------------
 */
/* Fetch a declared INSTANCE slot by name (never a static or a constant). */
PH7_PRIVATE ph7_value * PH7_NativeAttr(ph7_class_instance *pObj,const char *zName)
{
	SyString sName;
	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));
	return PH7_ClassInstanceFetchAttr(pObj,&sName);
}
/* Read an int slot WITHOUT converting it: ph7_value_to_int64() converts the
 * attribute in place, which would rewrite the object's own state. */
PH7_PRIVATE sxi64 PH7_NativeAttrInt(ph7_class_instance *pObj,const char *zName)
{
	ph7_value *pVal = PH7_NativeAttr(pObj,zName);
	if( pVal && (pVal->iFlags & MEMOBJ_INT) ){
		return pVal->x.iVal;
	}
	return 0;
}
/* Borrow a string slot's bytes (empty when it holds anything else). */
PH7_PRIVATE void PH7_NativeAttrStr(ph7_class_instance *pObj,const char *zName,
	const char **pzOut,int *pnOut)
{
	ph7_value *pVal = PH7_NativeAttr(pObj,zName);
	*pzOut = "";
	*pnOut = 0;
	if( pVal && (pVal->iFlags & MEMOBJ_STRING) ){
		*pzOut = (const char *)SyBlobData(&pVal->sBlob);
		*pnOut = (int)SyBlobLength(&pVal->sBlob);
	}
}
/* The object stored in a slot, or NULL when it holds anything else. */
PH7_PRIVATE ph7_class_instance * PH7_NativeAttrObj(ph7_class_instance *pObj,const char *zName)
{
	ph7_value *pVal = PH7_NativeAttr(pObj,zName);
	if( pVal == 0 || (pVal->iFlags & MEMOBJ_OBJ) == 0 ){
		return 0;
	}
	return (ph7_class_instance *)pVal->x.pOther;
}
/* Truth of a bool/int slot, again without converting it. */
PH7_PRIVATE int PH7_NativeAttrTruthy(ph7_class_instance *pObj,const char *zName)
{
	ph7_value *pVal = PH7_NativeAttr(pObj,zName);
	if( pVal && (pVal->iFlags & (MEMOBJ_BOOL|MEMOBJ_INT)) ){
		return pVal->x.iVal != 0;
	}
	return 0;
}
PH7_PRIVATE void PH7_NativeSetAttrInt(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,sxi64 iVal)
{
	ph7_value *pSlot = PH7_NativeAttr(pObj,zName);
	ph7_value sVal;
	if( pSlot == 0 ){
		return;
	}
	PH7_MemObjInitFromInt(&(*pVm),&sVal,iVal);
	PH7_MemObjStore(&sVal,pSlot);
	PH7_MemObjRelease(&sVal);
}
PH7_PRIVATE void PH7_NativeSetAttrStr(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,
	const char *zVal,int nVal)
{
	ph7_value *pSlot = PH7_NativeAttr(pObj,zName);
	ph7_value sVal;
	SyString sStr;
	if( pSlot == 0 ){
		return;
	}
	SyStringInitFromBuf(&sStr,zVal,nVal);
	PH7_MemObjInitFromString(&(*pVm),&sVal,&sStr);
	PH7_MemObjStore(&sVal,pSlot);
	PH7_MemObjRelease(&sVal);
}
PH7_PRIVATE void PH7_NativeSetAttrBool(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,int bVal)
{
	ph7_value *pSlot = PH7_NativeAttr(pObj,zName);
	ph7_value sVal;
	if( pSlot == 0 ){
		return;
	}
	PH7_MemObjInitFromBool(&(*pVm),&sVal,bVal);
	PH7_MemObjStore(&sVal,pSlot);
	PH7_MemObjRelease(&sVal);
}
/* Store an object in a slot, or NULL to clear it. PH7_MemObjStore takes the
 * reference the slot needs, so the temp never holds one of its own. */
PH7_PRIVATE void PH7_NativeSetAttrObj(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,
	ph7_class_instance *pVal)
{
	ph7_value *pSlot = PH7_NativeAttr(pObj,zName);
	ph7_value sVal;
	if( pSlot == 0 ){
		return;
	}
	PH7_MemObjInit(&(*pVm),&sVal);
	if( pVal ){
		sVal.x.pOther = pVal;
		sVal.iFlags = MEMOBJ_OBJ;
	}
	PH7_MemObjStore(&sVal,pSlot);
}
/*
 * Hand an instance back as a native call's result, dropping the reference
 * PH7_NewClassInstance/PH7_CloneClassInstance handed the caller.
 */
PH7_PRIVATE void PH7_NativeResultObject(ph7_context *pCtx,ph7_class_instance *pObj)
{
	ph7_value sRes;
	PH7_MemObjInit(pCtx->pVm,&sRes);
	sRes.x.pOther = pObj;
	sRes.iFlags = MEMOBJ_OBJ;
	ph7_result_value(pCtx,&sRes);   /* takes its own reference */
	PH7_ClassInstanceUnref(pObj);
}
/*
 * Resolve a class by name for the builder's own use (parents and interfaces).
 * Autoload is deliberately NOT triggered: these run at VM init, where the only
 * classes that can exist are the ones installed before this call, and a missing
 * name is a build-order bug in the engine, not a userland lookup.
 */
static ph7_class * NativeLookupClass(ph7_vm *pVm,const char *zName)
{
	return PH7_VmExtractClass(&(*pVm),zName,(sxu32)SyStrlen(zName),FALSE,0);
}
/*
 * Translate the builder's PH7_MOD_* modifier bits into the protection level and
 * the attribute/method flag word the class structures actually store.
 */
static sxi32 NativeProtection(sxi32 iMods)
{
	if( iMods & PH7_MOD_PRIVATE ){
		return PH7_CLASS_PROT_PRIVATE;
	}
	if( iMods & PH7_MOD_PROTECTED ){
		return PH7_CLASS_PROT_PROTECTED;
	}
	return PH7_CLASS_PROT_PUBLIC;
}
/*
 * Attach one C-bodied method to an already-created class.
 *
 * The ph7_user_func built here is NOT registered in pVm->hHostFunction: it hangs
 * off the method and is reachable only by dispatching the method, which is exactly
 * the property that retires the global thunks. Its sName is the php-facing
 * `Class::method`, so an ArgumentCountError raised at the OP_CALL choke point
 * names what a php user would recognise.
 */
PH7_PRIVATE sxi32 PH7_NativeClassInstallMethod(
	ph7_vm *pVm,
	ph7_class *pClass,
	const PH7_NativeMethodDef *pDef,
	void *pUserData
	)
{
	ph7_class_method *pMeth;
	ph7_user_func *pNative;
	SyString sName;
	SyString sVmName;
	char zQual[128];
	sxi32 iFuncFlags;
	sxi32 rc;
	SyStringInitFromBuf(&sName,pDef->zName,SyStrlen(pDef->zName));
	iFuncFlags = VM_FUNC_NATIVE;
	if( pVm->bCompilingBuiltin ){
		/* Same stamp the embedded chunks got, so Reflection keeps reporting these
		 * as internal: isInternal() true, getFileName() false. */
		iFuncFlags |= VM_FUNC_INTERNAL;
	}
	pMeth = PH7_NewClassMethod(&(*pVm),pClass,&sName,0,
		NativeProtection(pDef->iMods),
		(pDef->iMods & PH7_MOD_FINAL) ? PH7_CLASS_ATTR_FINAL : 0,
		iFuncFlags);
	if( pMeth == 0 ){
		return SXERR_MEM;
	}
	/* Staticness has to be recorded in BOTH places: on the method (where the class
	 * machinery and Reflection read it) and on the function (where the OP_CALL
	 * dispatcher reads it, to keep the caller's $this from leaking in as a receiver
	 * — see VM_FUNC_NATIVE_STATIC). */
	if( pDef->iMods & PH7_MOD_STATIC ){
		pMeth->iFlags |= PH7_CLASS_ATTR_STATIC;
		pMeth->sFunc.iFlags |= VM_FUNC_NATIVE_STATIC;
	}
	/* The C body. The diagnostic name is the qualified one php would print. */
	SyBufferFormat(zQual,sizeof(zQual),"%z::%s",&pClass->sName,pDef->zName);
	SyStringInitFromBuf(&sVmName,zQual,SyStrlen(zQual));
	rc = PH7_NewForeignFunction(&(*pVm),&sVmName,pDef->xFunc,pUserData,&pNative);
	if( rc != SXRET_OK ){
		return rc;
	}
	/* Arity bounds and by-ref positions come from the declared signature, exactly
	 * as they do for a builtin (VmSetBuiltinSignatures) -- one string is the single
	 * source of truth, and it doubles as the Reflection parameter list.
	 *
	 * NULL and "" mean different things here, and the difference is load-bearing.
	 * A builtin without a row in aBuiltinSig[] is simply undescribed, so it stays
	 * UNENFORCED (the SyZero'd bHasMaxArg == 0 reads as "unchecked", never as
	 * "accepts at most zero"). A native method, by contrast, always states its
	 * signature deliberately, so "" is a positive declaration of ZERO parameters
	 * and must enforce a maximum of zero -- otherwise `$gen->current(1)` and
	 * `$fiber->isStarted(1)` are swallowed where php raises "expects exactly 0
	 * arguments, 1 given". VmDeriveArityFromSig("") already answers min 0 / max 0 /
	 * enforced; only NULL opts out. */
	if( pDef->zSig ){
		sxi16 nMin = 0, nMax = 0;
		sxu8 bAtLeast = 0, bHasMax = 0;
		pNative->zSig = pDef->zSig;
		pNative->nByRefMask = VmDeriveByRefMaskFromSig(pDef->zSig);
		VmDeriveArityFromSig(pDef->zSig,&nMin,&bAtLeast,&nMax,&bHasMax);
		pNative->nMinArg = nMin;
		pNative->bAtLeast = bAtLeast;
		pNative->nMaxArg = nMax;
		pNative->bHasMaxArg = bHasMax;
	}
	if( pDef->zRet && pDef->zRet[0] ){
		pNative->zRet = pDef->zRet;
	}
	pMeth->sFunc.pNative = pNative;
	rc = PH7_ClassInstallMethod(pClass,pMeth);
	if( rc != SXRET_OK ){
		return rc;
	}
	if( pClass->bMounted ){
		/* The class is already live (a method attached after installation): mount
		 * this one method now, since VmMountUserClass will not run again. */
		return PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);
	}
	return SXRET_OK;
}
/*
 * Install one class constant carrying a scalar value.
 *
 * A declared default is normally a compiled initializer (ph7_class_attr::aByteCode)
 * evaluated at mount. The builder has no compiler to hand, so it evaluates the
 * value NOW into the constant's reserved slot and leaves the byte-code empty --
 * which is what a literal initializer would have produced anyway.
 */
static sxi32 NativeInstallConstant(ph7_vm *pVm,ph7_class *pClass,const PH7_NativeConstDef *pDef)
{
	ph7_class_attr *pAttr;
	SyString sName;
	SyStringInitFromBuf(&sName,pDef->zName,SyStrlen(pDef->zName));
	pAttr = PH7_NewClassAttr(&(*pVm),&sName,0,NativeProtection(pDef->iMods),
		PH7_CLASS_ATTR_CONSTANT);
	if( pAttr == 0 ){
		return SXERR_MEM;
	}
	pAttr->pDeclClass = pClass;
	/* Stash the literal; VmMountUserClassAttrs materializes it into the slot. */
	pAttr->pNativeValue = pDef;
	return PH7_ClassInstallAttr(pClass,pAttr);
}
/*
 * Install one declared property.
 *
 * The default is carried as a literal rather than compiled byte-code (see
 * PH7_NativeLiteralValue); an instance property materializes it at `new`, a static
 * one at mount. A PH7_NATIVE_VAL_NULL default is the plain `public $p;` case.
 */
PH7_PRIVATE sxi32 PH7_NativeClassInstallProperty(ph7_vm *pVm,ph7_class *pClass,
	const PH7_NativePropDef *pDef)
{
	ph7_class_attr *pAttr;
	SyString sName;
	sxi32 iFlags = 0;
	SyStringInitFromBuf(&sName,pDef->zName,SyStrlen(pDef->zName));
	if( pDef->iMods & PH7_MOD_STATIC ){
		iFlags |= PH7_CLASS_ATTR_STATIC;
	}
	pAttr = PH7_NewClassAttr(&(*pVm),&sName,0,NativeProtection(pDef->iMods),iFlags);
	if( pAttr == 0 ){
		return SXERR_MEM;
	}
	pAttr->pDeclClass = pClass;
	pAttr->pNativeValue = &pDef->sDefault;
	return PH7_ClassInstallAttr(pClass,pAttr);
}
/*
 * Create and install ONE class from its spec, without its methods.
 *
 * Split from the method pass because a spec table may describe classes that extend
 * each other, and PH7_ClassInherit needs the parent to exist first: callers list
 * bases before subclasses, and PH7_InstallNativeClasses runs this pass over the
 * whole table before touching any method.
 */
static sxi32 NativeDeclareClass(ph7_vm *pVm,const PH7_NativeClassSpec *pSpec,ph7_class **ppOut)
{
	ph7_class *pClass;
	SyString sName;
	sxu32 n;
	sxi32 rc;
	SyStringInitFromBuf(&sName,pSpec->zName,SyStrlen(pSpec->zName));
	pClass = PH7_NewRawClass(&(*pVm),&sName,0);
	if( pClass == 0 ){
		return SXERR_MEM;
	}
	pClass->iFlags |= pSpec->iFlags;
	pClass->xRelease = pSpec->xRelease;
	pClass->pIterVtab = pSpec->pIterVtab;
	for( n = 0 ; n < pSpec->nConst ; n++ ){
		rc = NativeInstallConstant(&(*pVm),pClass,&pSpec->aConst[n]);
		if( rc != SXRET_OK ){
			return rc;
		}
	}
	for( n = 0 ; n < pSpec->nProp ; n++ ){
		rc = PH7_NativeClassInstallProperty(&(*pVm),pClass,&pSpec->aProp[n]);
		if( rc != SXRET_OK ){
			return rc;
		}
	}
	rc = PH7_VmInstallClass(&(*pVm),pClass);
	if( rc != SXRET_OK ){
		return rc;
	}
	/* Inheritance AFTER installation, mirroring the compiler's order in
	 * GenStateCompileClassEx: the class must be findable while its own base chain
	 * is wired, or a self-referential hierarchy cannot resolve. */
	if( pSpec->zParent ){
		ph7_class *pBase = NativeLookupClass(&(*pVm),pSpec->zParent);
		if( pBase == 0 ){
			return SXERR_NOTFOUND;
		}
		/* The VM's OWN generator state, not a NULL one: PH7_ClassInherit takes its
		 * scratch allocator from pGen->pVm and reports every inheritance rule it
		 * enforces through PH7_GenCompileError(pGen, ...). Passing 0 crashed the
		 * moment a native spec first named a parent (the date exceptions). */
		rc = (pClass->iFlags & PH7_CLASS_INTERFACE)
			? PH7_ClassInterfaceInherit(pClass,pBase)
			: PH7_ClassInherit(&pVm->sCodeGen,pClass,pBase);
		if( rc != SXRET_OK ){
			return rc;
		}
	}
	if( pSpec->zImplements ){
		/* Comma-separated list, so one spec row can name several interfaces. */
		const char *zCur = pSpec->zImplements;
		while( zCur[0] != '\0' ){
			const char *zStart;
			char zIface[64];
			sxu32 nLen;
			ph7_class *pIface;
			while( zCur[0] == ' ' || zCur[0] == ',' ){
				zCur++;
			}
			if( zCur[0] == '\0' ){
				break;
			}
			zStart = zCur;
			while( zCur[0] != '\0' && zCur[0] != ',' && zCur[0] != ' ' ){
				zCur++;
			}
			nLen = (sxu32)(zCur - zStart);
			if( nLen >= sizeof(zIface) ){
				return SXERR_SYNTAX;
			}
			SyMemcpy(zStart,zIface,nLen);
			zIface[nLen] = '\0';
			pIface = NativeLookupClass(&(*pVm),zIface);
			if( pIface == 0 ){
				return SXERR_NOTFOUND;
			}
			rc = PH7_ClassImplement(pClass,pIface);
			if( rc != SXRET_OK ){
				return rc;
			}
		}
	}
	*ppOut = pClass;
	return SXRET_OK;
}
/*
 * Install a whole table of native classes: declare them all (so later rows may
 * extend earlier ones), then fill in their methods, then mount.
 *
 * Interfaces are declared but never mounted -- VmMountUserClass skips them anyway,
 * their methods being non-invocable.
 */
PH7_PRIVATE sxi32 PH7_InstallNativeClasses(ph7_vm *pVm,const PH7_NativeClassSpec *aSpec,sxu32 nSpec)
{
	ph7_class **apClass;
	sxu32 i,j;
	sxi32 rc;
	apClass = (ph7_class **)SyMemBackendAlloc(&pVm->sAllocator,nSpec * sizeof(ph7_class *));
	if( apClass == 0 ){
		return SXERR_MEM;
	}
	for( i = 0 ; i < nSpec ; i++ ){
		apClass[i] = 0;
		rc = NativeDeclareClass(&(*pVm),&aSpec[i],&apClass[i]);
		if( rc != SXRET_OK ){
			goto Done;
		}
	}
	for( i = 0 ; i < nSpec ; i++ ){
		for( j = 0 ; j < aSpec[i].nMethod ; j++ ){
			rc = PH7_NativeClassInstallMethod(&(*pVm),apClass[i],&aSpec[i].aMethod[j],0);
			if( rc != SXRET_OK ){
				goto Done;
			}
		}
	}
	for( i = 0 ; i < nSpec ; i++ ){
		rc = VmMountUserClass(&(*pVm),apClass[i]);
		if( rc != SXRET_OK ){
			goto Done;
		}
	}
	rc = SXRET_OK;
Done:
	SyMemBackendFree(&pVm->sAllocator,apClass);
	return rc;
}
/*
 * ---------------------------------------------------------------------------
 * InternalIterator.
 *
 * A native IteratorAggregate cannot answer a Generator: a PHP generator IS its
 * byte-code, and a C body has none. php never answers one either -- its internal
 * aggregates hand back an InternalIterator wrapping the iterator their class
 * declared -- so PHL declares that class once, here, and every native aggregate
 * reaches it through ph7_class::pIterVtab.
 *
 * The cursor lives entirely in the iterator's own private slots. A vtable states
 * only how to REACH a position; reading it back is the same three methods for
 * everyone.
 * ---------------------------------------------------------------------------
 */
/* The walk THIS iterator was made for: the vtable of the aggregate it holds. */
static const PH7_NativeIterVtab * NativeIterVtab(ph7_class_instance *pIt)
{
	ph7_class_instance *pSrc = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);
	return pSrc ? pSrc->pClass->pIterVtab : 0;
}
static int vm_builtin_InternalIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const PH7_NativeIterVtab *pVtab;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis == 0 ){
		return PH7_OK;
	}
	pVtab = NativeIterVtab(pThis);
	if( pVtab == 0 || pVtab->xRewind == 0 ){
		PH7_NativeSetAttrBool(pCtx->pVm,pThis,PH7_NATIVE_IT_DONE,1);
		return PH7_OK;
	}
	pVtab->xRewind(pCtx->pVm,pThis);
	return PH7_OK;
}
static int vm_builtin_InternalIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const PH7_NativeIterVtab *pVtab;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis == 0 || PH7_NativeAttrTruthy(pThis,PH7_NATIVE_IT_DONE) ){
		return PH7_OK;
	}
	pVtab = NativeIterVtab(pThis);
	if( pVtab == 0 || pVtab->xNext == 0 ){
		PH7_NativeSetAttrBool(pCtx->pVm,pThis,PH7_NATIVE_IT_DONE,1);
		return PH7_OK;
	}
	pVtab->xNext(pCtx->pVm,pThis);
	return PH7_OK;
}
static int vm_builtin_InternalIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_bool(pCtx,pThis != 0 && !PH7_NativeAttrTruthy(pThis,PH7_NATIVE_IT_DONE));
	return PH7_OK;
}
/* current() and key() answer the slots the walk left behind -- and NULL once it
 * is over, which is what php's exhausted InternalIterator answers too. */
static int NativeIterReadSlot(ph7_context *pCtx,const char *zSlot)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value *pVal;
	if( pThis == 0 || PH7_NativeAttrTruthy(pThis,PH7_NATIVE_IT_DONE) ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pVal = PH7_NativeAttr(pThis,zSlot);
	if( pVal ){
		ph7_result_value(pCtx,pVal);
	}
	return PH7_OK;
}
static int vm_builtin_InternalIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return NativeIterReadSlot(pCtx,PH7_NATIVE_IT_CUR);
}
static int vm_builtin_InternalIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return NativeIterReadSlot(pCtx,PH7_NATIVE_IT_KEY);
}
/* InternalIterator::__construct() — private in php, and never reached from PHP:
 * PH7_NativeIteratorNew builds the instance directly. */
static int vm_builtin_InternalIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	SXUNUSED(pCtx);
	return PH7_OK;
}
/*
 * The iterator a native getIterator() answers: bound to its aggregate and already
 * positioned, because php's is valid() before the first rewind().
 */
PH7_PRIVATE ph7_class_instance * PH7_NativeIteratorNew(ph7_vm *pVm,ph7_class_instance *pSrc)
{
	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),"InternalIterator",
		sizeof("InternalIterator")-1,FALSE,0);
	ph7_class_instance *pIt;
	const PH7_NativeIterVtab *pVtab;
	if( pClass == 0 || pSrc == 0 ){
		return 0;
	}
	pIt = PH7_NewClassInstance(&(*pVm),pClass);
	if( pIt == 0 ){
		return 0;
	}
	PH7_NativeSetAttrObj(&(*pVm),pIt,PH7_NATIVE_IT_SRC,pSrc);
	PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);
	pVtab = pSrc->pClass->pIterVtab;
	if( pVtab && pVtab->xRewind ){
		pVtab->xRewind(&(*pVm),pIt);
	}
	return pIt;
}
PH7_PRIVATE sxi32 PH7_VmInstallNativeIterator(ph7_vm *pVm)
{
	static const PH7_NativePropDef aProp[] = {
		{ PH7_NATIVE_IT_SRC,  PH7_MOD_PRIVATE, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 } },
		{ PH7_NATIVE_IT_CUR,  PH7_MOD_PRIVATE, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 } },
		{ PH7_NATIVE_IT_KEY,  PH7_MOD_PRIVATE, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 } },
		{ PH7_NATIVE_IT_POS,  PH7_MOD_PRIVATE, { 0, 0, PH7_NATIVE_VAL_INT,  0, 0, 0.0 } },
		{ PH7_NATIVE_IT_AUX,  PH7_MOD_PRIVATE, { 0, 0, PH7_NATIVE_VAL_INT,  0, 0, 0.0 } },
		{ PH7_NATIVE_IT_DONE, PH7_MOD_PRIVATE, { 0, 0, PH7_NATIVE_VAL_BOOL, 1, 0, 0.0 } },
	};
	static const PH7_NativeMethodDef aMethod[] = {
		{ "__construct", PH7_MOD_PRIVATE, "", "", vm_builtin_InternalIterator_construct },
		{ "current",     PH7_MOD_PUBLIC, "", "mixed", vm_builtin_InternalIterator_current },
		{ "key",         PH7_MOD_PUBLIC, "", "mixed", vm_builtin_InternalIterator_key },
		{ "next",        PH7_MOD_PUBLIC, "", "void",  vm_builtin_InternalIterator_next },
		{ "valid",       PH7_MOD_PUBLIC, "", "bool",  vm_builtin_InternalIterator_valid },
		{ "rewind",      PH7_MOD_PUBLIC, "", "void",  vm_builtin_InternalIterator_rewind },
	};
	static const PH7_NativeClassSpec aSpec[] = {
		{ "InternalIterator", 0, 0, PH7_CLASS_FINAL,
		  aMethod, SX_ARRAYSIZE(aMethod), 0, 0, aProp, SX_ARRAYSIZE(aProp), 0, 0 },
	};
	ph7_class *pIt,*pIterator;
	sxi32 rc;
	rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));
	if( rc != SXRET_OK ){
		return rc;
	}
	/* `implements Iterator` only NOW: PH7_ClassImplement stubs every interface
	 * method the class does not already declare as ABSTRACT, so attaching it
	 * through the spec would have made InternalIterator uninstantiable. */
	pIt = NativeLookupClass(&(*pVm),"InternalIterator");
	pIterator = NativeLookupClass(&(*pVm),"Iterator");
	if( pIt == 0 || pIterator == 0 ){
		return SXERR_NOTFOUND;
	}
	return PH7_ClassImplement(pIt,pIterator);
}
