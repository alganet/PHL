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
		rc = (pClass->iFlags & PH7_CLASS_INTERFACE)
			? PH7_ClassInterfaceInherit(pClass,pBase)
			: PH7_ClassInherit(0,pClass,pBase);
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
