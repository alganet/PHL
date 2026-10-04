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
		case PH7_NATIVE_VAL_ARRAY: {
			/* The empty array — php's `private array $trace = [];`. Each instance
			 * needs its OWN map (the exception's trace is written per throw), so
			 * this allocates rather than sharing one. */
			ph7_hashmap *pMap = PH7_NewHashmap(&(*pVm),0,0);
			if( pMap == 0 ){
				PH7_MemObjInit(&(*pVm),pOut);
			}else{
				PH7_MemObjInitFromArray(&(*pVm),pOut,pMap);
			}
			break;
		}
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
	SyHashEntry *pEntry;
	VmClassAttr *pVmAttr;
	ph7_value *pSlot;
	PH7_NativeMaterializeLazy(&(*pVm),pObj);
	pEntry = SyHashGet(&pObj->hAttr,(const void *)zProp,nProp);
	if( pEntry == 0 ){
		return;
	}
	pVmAttr = (VmClassAttr *)pEntry->pUserData;
	pSlot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pVmAttr->nIdx);
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
/*
 * Has this declared slot never been written? A PH7_NATIVE_VAL_NONE property is
 * php's `public int $id;` — typed, with no default — and reading one before the
 * class has filled it is php's "must not be accessed before initialization".
 * The property-read opcode raises that itself; a C body reading the slot
 * directly has to ask.
 */
PH7_PRIVATE int PH7_NativeAttrIsUninit(ph7_class_instance *pObj,const char *zName)
{
	SyHashEntry *pEntry = pObj ? SyHashGet(&pObj->hAttr,(const void *)zName,SyStrlen(zName)) : 0;
	return pEntry != 0
		&& (((VmClassAttr *)pEntry->pUserData)->iState & VM_CLASS_ATTR_UNINIT) != 0;
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
/*
 * Clear the not-yet-initialized mark a TYPED slot without a default carries.
 *
 * There are two families of writer here: PH7_NativeSetProp, which looks the
 * VmClassAttr up and clears the bit, and the four typed shortcuts below, which
 * write the ph7_value through PH7_NativeAttr and used to leave it set. That was
 * invisible while nothing native declared a default-less typed property; the
 * moment `public string $name` arrived on the reflectors, every one of them
 * threw "must not be accessed before initialization" from a constructor that HAD
 * written the slot. Rule 44's family: the bookkeeping has to live with the write,
 * not with one of the two ways of writing.
 */
static void NativeAttrMarkInit(ph7_class_instance *pObj,const char *zName)
{
	SyHashEntry *pEntry = SyHashGet(&pObj->hAttr,(const void *)zName,SyStrlen(zName));
	if( pEntry ){
		((VmClassAttr *)pEntry->pUserData)->iState &= ~VM_CLASS_ATTR_UNINIT;
	}
}
/*
 * The slot fetch on the WRITE side. A class whose php-visible properties are LAZY
 * has none of them on the object until a C body fills one, and that first write is
 * what installs the set -- which is php's constructor writing its struct into the
 * property table. Every native writer goes through here so the bookkeeping lives
 * with the write rather than with one of the ways of writing.
 */
static ph7_value * NativeAttrForWrite(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName)
{
	ph7_value *pSlot;
	PH7_NativeMaterializeLazy(&(*pVm),pObj);
	pSlot = PH7_NativeAttr(pObj,zName);
	if( pSlot == 0 && pObj ){
		/* An ON-DEMAND property is installed by the write that names it and by
		 * nothing else, so an object nobody wrote one on does not carry the name
		 * (php's `date_string`, which exists on a from-string DateInterval alone). */
		SyHashEntry *pEntry = SyHashGet(&pObj->pClass->hAttr,zName,(sxu32)SyStrlen(zName));
		if( pEntry ){
			ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;
			if( pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_ONDEMAND ){
				VmClassAttr *pVmAttr = 0;
				VmRecreateDeclaredAttr(&(*pVm),pObj,pAttr,&pVmAttr);
				if( pVmAttr ){
					pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;
					pSlot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pVmAttr->nIdx);
				}
			}
		}
	}
	return pSlot;
}
/*
 * Keep one of this OBJECT's slots out of every surface that shows it, while it
 * goes on reading, writing and answering isset() as it did
 * (VM_CLASS_ATTR_UNSEEN).
 */
PH7_PRIVATE void PH7_NativeHideAttr(ph7_class_instance *pObj,const char *zName)
{
	SyHashEntry *pEntry;
	if( pObj == 0 ){
		return;
	}
	pEntry = SyHashGet(&pObj->hAttr,zName,(sxu32)SyStrlen(zName));
	if( pEntry ){
		((VmClassAttr *)pEntry->pUserData)->iState |= VM_CLASS_ATTR_UNSEEN;
	}
}
PH7_PRIVATE void PH7_NativeSetAttrInt(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,sxi64 iVal)
{
	ph7_value *pSlot = NativeAttrForWrite(&(*pVm),pObj,zName);
	ph7_value sVal;
	if( pSlot == 0 ){
		return;
	}
	PH7_MemObjInitFromInt(&(*pVm),&sVal,iVal);
	PH7_MemObjStore(&sVal,pSlot);
	PH7_MemObjRelease(&sVal);
	NativeAttrMarkInit(pObj,zName);
}
#ifndef PH7_OMIT_FLOATING_POINT
PH7_PRIVATE void PH7_NativeSetAttrReal(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,ph7_real rVal)
{
	ph7_value *pSlot = NativeAttrForWrite(&(*pVm),pObj,zName);
	ph7_value sVal;
	if( pSlot == 0 ){
		return;
	}
	PH7_MemObjInitFromReal(&(*pVm),&sVal,rVal);
	PH7_MemObjStore(&sVal,pSlot);
	PH7_MemObjRelease(&sVal);
	NativeAttrMarkInit(pObj,zName);
}
#endif /* PH7_OMIT_FLOATING_POINT */
PH7_PRIVATE void PH7_NativeSetAttrStr(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,
	const char *zVal,int nVal)
{
	ph7_value *pSlot = NativeAttrForWrite(&(*pVm),pObj,zName);
	ph7_value sVal;
	SyString sStr;
	if( pSlot == 0 ){
		return;
	}
	SyStringInitFromBuf(&sStr,zVal,nVal);
	PH7_MemObjInitFromString(&(*pVm),&sVal,&sStr);
	PH7_MemObjStore(&sVal,pSlot);
	PH7_MemObjRelease(&sVal);	NativeAttrMarkInit(pObj,zName);
}
PH7_PRIVATE void PH7_NativeSetAttrBool(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,int bVal)
{
	ph7_value *pSlot = NativeAttrForWrite(&(*pVm),pObj,zName);
	ph7_value sVal;
	if( pSlot == 0 ){
		return;
	}
	PH7_MemObjInitFromBool(&(*pVm),&sVal,bVal);
	PH7_MemObjStore(&sVal,pSlot);
	PH7_MemObjRelease(&sVal);	NativeAttrMarkInit(pObj,zName);
}
/* Store an object in a slot, or NULL to clear it. PH7_MemObjStore takes the
 * reference the slot needs, so the temp never holds one of its own. */
PH7_PRIVATE void PH7_NativeSetAttrObj(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,
	ph7_class_instance *pVal)
{
	ph7_value *pSlot = NativeAttrForWrite(&(*pVm),pObj,zName);
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
	NativeAttrMarkInit(pObj,zName);
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
		((pDef->iMods & PH7_MOD_FINAL) ? PH7_CLASS_ATTR_FINAL : 0)
		| ((pDef->iMods & PH7_MOD_ABSTRACT) ? PH7_CLASS_ATTR_ABSTRACT : 0)
		| ((pDef->iMods & PH7_MOD_FABRICATED) ? PH7_CLASS_ATTR_FABRICATED : 0),
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
	/* An ABSTRACT method has no body to install — an INTERFACE's methods are the
	 * reason the builder needs the case at all (`SeekableIterator::seek()`), and
	 * php reports them with their declared signature like any other. The dispatch
	 * never reaches one: OP_CALL and OP_MEMBER both refuse an abstract method
	 * ("Cannot call abstract method C::m()") before they look at a body. */
	if( pDef->iMods & PH7_MOD_ABSTRACT ){
		if( pDef->zSig ){
			rc = PH7_NewForeignFunction(&(*pVm),&sName,pDef->xFunc,pUserData,&pNative);
			if( rc != SXRET_OK ){
				return rc;
			}
			pNative->zSig = pDef->zSig;
			if( pDef->zRet && pDef->zRet[0] ){
				pNative->zRet = pDef->zRet;
			}
			pMeth->sFunc.pNative = pNative;
		}
		return PH7_ClassInstallMethod(pClass,pMeth);
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
 * Fill in a declared property TYPE from the text a spec row states, exactly as
 * GenStateCopyTypeToAttr fills it from a parsed declaration: nType is the
 * MEMOBJ_* the atom names (SXU32_HIGH plus sClass for a class name, which is
 * also how the compiler carries `mixed` and `iterable`), the `?` becomes the
 * NULLABLE flag rather than a type bit, and sTypeName keeps the text VERBATIM --
 * both the TypeError and Reflection print what the declaration said.
 *
 * Single atoms only. A union needs the alternative SET the compiler builds, and
 * nothing native declares one; a spec that tries reads as the class name it is
 * spelled with, which is why the parse stays this literal.
 */
static void NativeAttrType(ph7_class_attr *pAttr,const char *zType)
{
	static const struct { const char *zName; sxu32 nType; } aScalar[] = {
		{ "int",    MEMOBJ_INT },
		{ "float",  MEMOBJ_REAL },
		{ "string", MEMOBJ_STRING },
		{ "bool",   MEMOBJ_BOOL },
		{ "array",  MEMOBJ_HASHMAP },
		{ "object", MEMOBJ_OBJ },
	};
	const char *zAtom = zType;
	sxu32 nAtom, n;
	if( zAtom[0] == '?' ){
		pAttr->iFlags |= PH7_CLASS_ATTR_NULLABLE;
		zAtom++;
	}
	nAtom = SyStrlen(zAtom);
	pAttr->iFlags |= PH7_CLASS_ATTR_TYPED;
	SyStringInitFromBuf(&pAttr->sTypeName,zType,SyStrlen(zType));
	for( n = 0 ; n < SX_ARRAYSIZE(aScalar) ; ++n ){
		if( nAtom == SyStrlen(aScalar[n].zName)
		 && SyMemcmp(zAtom,aScalar[n].zName,(sxu32)nAtom) == 0 ){
			pAttr->nType = aScalar[n].nType;
			return;
		}
	}
	pAttr->nType = SXU32_HIGH;
	SyStringInitFromBuf(&pAttr->sClass,zAtom,nAtom);
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
	if( pDef->iMods & PH7_MOD_HIDDEN ){
		iFlags |= PH7_CLASS_ATTR_HIDDEN;
	}
	if( pDef->iMods & PH7_MOD_ONDEMAND ){
		iFlags |= PH7_CLASS_ATTR_NATIVE_ONDEMAND;
	}
	/* php's lazily-filled REAL slot: the object carries the name from `new`, in its
	 * declared position and uninitialized, and the class's handler fills it on the
	 * first read. NOWRITE rides with it because the handler owns the write too --
	 * these are the names php refuses with the readonly WORDING while Reflection
	 * still reports them writable. */
	if( pDef->iMods & PH7_MOD_LAZYSLOT ){
		iFlags |= PH7_CLASS_ATTR_NATIVE_LAZYSLOT|PH7_CLASS_ATTR_NATIVE_NOWRITE;
	}
	/* php's VIRTUAL property: declared, and answered by the class's own handlers
	 * rather than by a slot. NATIVE_VIRTUAL rides with it because that is exactly
	 * what the name means to Reflection (modifiers 512) and to the object
	 * comparator -- there is no real property behind it to compare. */
	if( pDef->iMods & PH7_MOD_VIRTUAL ){
		iFlags |= PH7_CLASS_ATTR_NATIVE_NOSLOT|PH7_CLASS_ATTR_NATIVE_VIRTUAL;
	}
	/* php declares several native slots readonly and asymmetrically visible
	 * (`public protected(set) readonly string $path` on Directory), and both are
	 * php-visible twice over: the write refusal and Reflection's modifier list. */
	if( pDef->iMods & PH7_MOD_READONLY ){
		iFlags |= PH7_CLASS_ATTR_READONLY;
	}
	if( pDef->iMods & PH7_MOD_PROT_SET ){
		iFlags |= PH7_CLASS_ATTR_PROTECTED_SET;
	}
	if( pDef->iMods & PH7_MOD_PRIV_SET ){
		iFlags |= PH7_CLASS_ATTR_PRIVATE_SET;
	}
	pAttr = PH7_NewClassAttr(&(*pVm),&sName,0,NativeProtection(pDef->iMods),iFlags);
	if( pAttr == 0 ){
		return SXERR_MEM;
	}
	pAttr->pDeclClass = pClass;
	/* NO default is not the same as a NULL one, and the difference is php-visible:
	 * a typed slot without a default is UNINITIALIZED at `new` (reading it before
	 * the class writes it is an Error, hasDefaultValue() is false), which is what
	 * php declares for LibXMLError's six fields and every reflector's $name. The
	 * machinery is already there for a compiled `public string $p;` — leaving
	 * pNativeValue at 0 is what selects it. */
	if( pDef->sDefault.iType != PH7_NATIVE_VAL_NONE ){
		pAttr->pNativeValue = &pDef->sDefault;
	}
	if( pDef->zType && pDef->zType[0] ){
		NativeAttrType(pAttr,pDef->zType);
	}
	return PH7_ClassInstallAttr(pClass,pAttr);
}
/*
 * Attach an `#[Attr(...)]` to something declared from C.
 *
 * php declares `#[Attribute(Attribute::TARGET_CLASS)]` on Attribute itself, a
 * target mask on every other attribute class, and `#[NoDiscard(message: …)]` on
 * nine DateTimeImmutable methods — and those records are LOAD-BEARING: the
 * compiler reads them to decide whether a user's `#[Deprecated]` may sit where
 * it does, the NoDiscard warning reads its message from them, and
 * ReflectionAttribute answers them all. A compiled attribute holds its argument
 * as byte-code; there is no compiler here, so the argument rides as the same
 * literal record a native constant or property default uses and every reader
 * takes that branch when the byte-code is empty.
 *
 * NativeBuildAttr is the shared half; the two entry points below hang the record
 * on a class or on one of its methods. aArg is BORROWED, so callers state their
 * rows `static const`.
 */
static sxi32 NativeBuildAttr(ph7_vm *pVm,ph7_attribute *pAttr,const char *zAttr,
	const PH7_NativeAttrArg *aArg,sxu32 nArg)
{
	char *zDup;
	sxu32 n;
	SyZero(pAttr,sizeof(*pAttr));
	zDup = SyMemBackendStrDup(&pVm->sAllocator,zAttr,SyStrlen(zAttr));
	if( zDup == 0 ){
		return SXERR_MEM;
	}
	SyStringInitFromBuf(&pAttr->sName,zDup,SyStrlen(zAttr));
	SySetInit(&pAttr->aArgs,&pVm->sAllocator,sizeof(ph7_attr_arg));
	for( n = 0 ; n < nArg ; n++ ){
		ph7_attr_arg sArgRec;
		SyZero(&sArgRec,sizeof(sArgRec));
		SySetInit(&sArgRec.aByteCode,&pVm->sAllocator,sizeof(VmInstr));
		if( aArg[n].zName ){
			char *zN = SyMemBackendStrDup(&pVm->sAllocator,aArg[n].zName,SyStrlen(aArg[n].zName));
			if( zN ){
				SyStringInitFromBuf(&sArgRec.sName,zN,SyStrlen(aArg[n].zName));
			}
		}
		/* The literal is BORROWED, not copied: aArg must have static storage
		 * duration (every caller states its rows as `static const`). */
		sArgRec.pNativeValue = (const void *)&aArg[n].sValue;
		SySetPut(&pAttr->aArgs,(const void *)&sArgRec);
	}
	return SXRET_OK;
}
/*
 * Declare one of a native class's METHODS php 8.5's `#[\NoDiscard]`, argument
 * and all — the same record a compiled declaration carries, so Reflection
 * reports the attribute and the warning reads its message from the one place a
 * userland one is read from. Assigned by the owning installer after
 * PH7_InstallNativeClasses, like xClone/xDim/xSet: a spec-row field would have
 * to be left empty by every other table.
 */
PH7_PRIVATE sxi32 PH7_NativeMethodSetNoDiscard(ph7_vm *pVm,ph7_class *pClass,
	const char *zMethod,const PH7_NativeAttrArg *aArg,sxu32 nArg)
{
	ph7_class_method *pMeth;
	ph7_attribute sAttr;
	sxi32 rc;
	if( pClass == 0 ){
		return SXERR_NOTFOUND;
	}
	pMeth = PH7_ClassExtractMethod(pClass,zMethod,(sxu32)SyStrlen(zMethod));
	if( pMeth == 0 ){
		return SXERR_NOTFOUND;
	}
	rc = NativeBuildAttr(&(*pVm),&sAttr,"NoDiscard",aArg,nArg);
	if( rc != SXRET_OK ){
		return rc;
	}
	pMeth->sFunc.iFlags |= VM_FUNC_NODISCARD;
	return SySetPut(&pMeth->sFunc.aAttrs,(const void *)&sAttr);
}
PH7_PRIVATE sxi32 PH7_NativeClassAddAttribute(ph7_vm *pVm,ph7_class *pClass,
	const char *zAttr,const PH7_NativeAttrArg *aArg,sxu32 nArg)
{
	ph7_attribute sAttr;
	sxi32 rc;
	if( pClass == 0 ){
		return SXERR_NOTFOUND;
	}
	rc = NativeBuildAttr(&(*pVm),&sAttr,zAttr,aArg,nArg);
	if( rc != SXRET_OK ){
		return rc;
	}
	return SySetPut(&pClass->aAttrs,(const void *)&sAttr);
}
/*
 * php's get_properties / get_debug_info handlers: the SHAPE a class SHOWS.
 *
 * Several native classes present something that is not their storage. php shows a
 * DateTime as date/timezone_type/timezone (the state is a timestamp, an offset and
 * a zone name), a DateTimeZone as timezone_type/timezone, and a WeakReference as
 * ["object"]. Those slots carry PH7_MOD_HIDDEN so no presentation surface sees the
 * engine state; this fills an array with what php shows instead.
 *
 * Answers 1 when the class (or an ancestor) declared a hook and pOut was filled.
 * bDebug distinguishes php's two handlers: 1 for var_dump/print_r (get_debug_info),
 * 0 for var_export and the (array) cast (get_properties). They disagree — a
 * WeakReference shows ["object"] to var_dump and nothing to (array) — so the
 * callback is told which is asking rather than each caller guessing.
 * get_object_vars() and foreach are NOT callers: php answers those from the real
 * properties with the caller's scope applied, which for every class here is empty.
 */
PH7_PRIVATE int PH7_ClassInstancePresent(ph7_class_instance *pThis,ph7_value *pOut,int bDebug)
{
	ph7_class *pClass;
	if( pThis == 0 || pOut == 0 ){
		return 0;
	}
	for( pClass = pThis->pClass ; pClass ; pClass = pClass->pBase ){
		if( pClass->xPresent ){
			return pClass->xPresent(pThis->pVm,pThis,pOut,bDebug) == SXRET_OK;
		}
	}
	return 0;
}
/*
 * The nearest ph7_class::xDim in a class's base chain -- php's handler
 * inheritance, so a user subclass of DOMNodeList reads dimensions the way its
 * parent does.
 */
static ph7_class * NativeDimClass(ph7_class *pClass)
{
	while( pClass ){
		if( pClass->xDim ){
			return pClass;
		}
		pClass = pClass->pBase;
	}
	return 0;
}
/*
 * Does `$o[$k]` mean anything for an instance of this class? The subscript
 * opcode asks BEFORE it commits to php's `Cannot use object of type C as
 * array`, which is still the answer for every class that has no hook.
 */
PH7_PRIVATE int PH7_ClassHasNativeDim(ph7_class *pClass)
{
	return NativeDimClass(pClass) != 0;
}
/*
 * Run the hook. Answers 0 when the class has none (nothing in pCtx is touched);
 * 1 when it answered, which includes a REFUSAL -- the caller reads zThrowClass
 * to tell the two apart.
 */
PH7_PRIVATE int PH7_ClassNativeDim(ph7_class_instance *pThis,PH7_NativeDimCtx *pCtx)
{
	ph7_class *pClass = pThis ? NativeDimClass(pThis->pClass) : 0;
	if( pClass == 0 ){
		return 0;
	}
	pClass->xDim(pThis->pVm,pThis,pCtx);
	return 1;
}
/*
 * The refusal a dimension WRITE, APPEND or UNSET takes on an object. php's own
 * sentence for a class that is not an ArrayAccess is
 * `Cannot use object of type C as array`; a class whose read handler answers
 * something words its own (php's PDORow names the operation and the class),
 * which the hook supplies through the same refusal fields a read uses.
 */
PH7_PRIVATE sxu32 PH7_ClassNativeDimRefusal(ph7_class_instance *pThis,int iMode,
	char *zMsg,sxu32 nMsg)
{
	ph7_class *pClass = pThis ? NativeDimClass(pThis->pClass) : 0;
	if( pClass ){
		PH7_NativeDimCtx sDim;
		sDim.iMode = iMode;
		sDim.pOffset = 0;
		sDim.pResult = 0;
		sDim.zThrowClass = 0;
		sDim.zThrowMsg[0] = 0;
		sDim.bStored = 0;
		pClass->xDim(pThis->pVm,pThis,&sDim);
		if( sDim.zThrowClass ){
			return SyBufferFormat(zMsg,nMsg,"%s",sDim.zThrowMsg);
		}
	}
	return SyBufferFormat(zMsg,nMsg,"Cannot use object of type %.*s as array",
		pThis ? (int)pThis->pClass->sDisp.nByte : 0,
		pThis ? pThis->pClass->sDisp.zString : "");
}
/*
 * Offer a dimension WRITE, APPEND or UNSET to the class's own handler, with the
 * offset and the value the refusal-only form does not carry.
 *
 * Answers 1 when the handler took the access -- either by STORING (bStored,
 * with zThrowClass still 0) or by wording its own refusal in
 * zThrowClass/zThrowMsg -- and 0 when the class has no handler or its handler
 * declined, which puts the access on the ordinary `Cannot use object of type C
 * as array` path. pCtx is the caller's scratch: it is initialized here and left
 * filled for the caller to read.
 */
PH7_PRIVATE int PH7_ClassNativeDimStore(ph7_class_instance *pThis,int iMode,
	ph7_value *pOffset,ph7_value *pValue,PH7_NativeDimCtx *pCtx)
{
	ph7_class *pClass = pThis ? NativeDimClass(pThis->pClass) : 0;
	pCtx->iMode = iMode;
	pCtx->pOffset = pOffset;
	pCtx->pResult = pValue;
	pCtx->zThrowClass = 0;
	pCtx->zThrowMsg[0] = 0;
	pCtx->bStored = 0;
	if( pClass == 0 ){
		return 0;
	}
	pClass->xDim(pThis->pVm,pThis,pCtx);
	return pCtx->bStored || pCtx->zThrowClass ? 1 : 0;
}
/*
 * The nearest ph7_class::xProp in a class's base chain -- the same handler
 * inheritance xDim and xSet get, and php's own: a subclass of a class whose
 * properties are not storage reads them through the parent's handler.
 */
static ph7_class * NativePropClass(ph7_class *pClass)
{
	while( pClass ){
		if( pClass->xProp ){
			return pClass;
		}
		pClass = pClass->pBase;
	}
	return 0;
}
/*
 * Does `$o->p` MEAN something this class answers for itself? Asked before the
 * miss path commits to creating a property, warning about an undefined one or
 * dispatching __get.
 */
PH7_PRIVATE int PH7_ClassHasNativeProp(ph7_class *pClass)
{
	return NativePropClass(pClass) != 0;
}
/*
 * Run the hook. Answers 0 when the class has none, or when the hook DECLINED
 * the name (bAnswered left at 0); 1 when it answered, which includes a
 * REFUSAL -- the caller reads zThrowClass to tell the two apart.
 */
PH7_PRIVATE int PH7_ClassNativeProp(ph7_class_instance *pThis,PH7_NativePropCtx *pCtx)
{
	ph7_class *pClass = pThis ? NativePropClass(pThis->pClass) : 0;
	if( pClass == 0 ){
		return 0;
	}
	pClass->xProp(pThis->pVm,pThis,pCtx);
	return pCtx->bAnswered || pCtx->zThrowClass != 0;
}
/*
 * Fill a caller-owned context and run the hook, for the callers that ask
 * OUTSIDE the member opcode: Reflection's getValue()/setValue() and
 * property_exists(), each of which reaches php's handlers by its own door.
 */
PH7_PRIVATE int PH7_ClassNativePropAsk(ph7_class_instance *pThis,PH7_NativePropCtx *pCtx,
	int iMode,const SyString *pName,ph7_value *pResult)
{
	pCtx->iMode = iMode;
	pCtx->pName = pName;
	pCtx->pResult = pResult;
	pCtx->bAnswered = 0;
	pCtx->zThrowClass = 0;
	pCtx->zThrowMsg[0] = 0;
	pCtx->iThrowCode = 0;
	pCtx->bQuiet = 0;
	pCtx->bWriteCtx = 0;
	pCtx->nSlot = SXU32_HIGH;
	return PH7_ClassNativeProp(pThis,pCtx);
}
/*
 * Does this class answer `$o->p` through a HANDLER of its own -- php's question
 * "is the name in the class's property-handler table"?
 *
 * The hook answers for itself, because what the answer depends on differs per
 * class: ext/dom reads the class's VIRTUAL declarations (PH7_MOD_VIRTUAL -- a
 * name declared with no slot of any kind), ArrayObject reads the object's
 * ARRAY_AS_PROPS flag and owns every name once it is set. Whether a REAL property
 * of that name is in the way is not the hook's question: every caller asks only
 * after the instance's own table missed, which is php's order too.
 *
 * It is what makes the handler beat a subclass's own `__get`: a name the table
 * carries never reaches the magic layer, and a name it does not carry falls
 * through to it, which is php's handler order. Asked at the write shapes, where
 * the member opcode has no value yet and the write half cannot be run for an
 * answer.
 */
PH7_PRIVATE int PH7_ClassNativePropOwns(ph7_class_instance *pThis,const SyString *pName)
{
	PH7_NativePropCtx sCtx;
	if( pThis == 0 || NativePropClass(pThis->pClass) == 0 ){
		return 0;
	}
	return PH7_ClassNativePropAsk(pThis,&sCtx,PH7_NATIVE_PROP_OWNS,pName,0)
	    && sCtx.zThrowClass == 0;
}
/*
 * Install a property handler on a mounted native class. Called by the owning
 * installer right after PH7_InstallNativeClasses, for the same reason xClone,
 * xDim and xSet are: the spec table has no field for a hook.
 */
PH7_PRIVATE sxi32 PH7_NativeClassInstallPropHook(ph7_vm *pVm,const char *zClass,
	void (*xProp)(ph7_vm *,ph7_class_instance *,PH7_NativePropCtx *))
{
	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);
	if( pClass == 0 ){
		return SXERR_NOTFOUND;
	}
	pClass->xProp = xProp;
	return SXRET_OK;
}
/*
 * The nearest ph7_class::xSet in a class's base chain -- the same handler
 * inheritance the dimension hook gets, so a user subclass of DateInterval
 * converts its writes the way its parent does.
 */
static ph7_class * NativeSetClass(ph7_class *pClass)
{
	while( pClass ){
		if( pClass->xSet ){
			return pClass;
		}
		pClass = pClass->pBase;
	}
	return 0;
}
/*
 * Run the write handler for a property store. Answers 0 when no class in the
 * chain has one (nothing in pCtx is touched); 1 when it ran, which includes a
 * REFUSAL -- the caller reads zThrowClass to tell the two apart.
 */
PH7_PRIVATE int PH7_ClassNativeSet(ph7_class_instance *pThis,PH7_NativeSetCtx *pCtx)
{
	ph7_class *pClass = pThis ? NativeSetClass(pThis->pClass) : 0;
	if( pClass == 0 ){
		return 0;
	}
	pClass->xSet(pThis->pVm,pThis,pCtx);
	return 1;
}
/*
 * php's create_object handler for a mounted native class: the C routine that
 * runs once the instance frame exists and before any constructor.
 *
 * It exists for one shape -- a class whose properties php DECLARES and then
 * answers through a read_property handler rather than out of the slots. Seeding
 * the slots reproduces every surface of that at once (the read, `var_dump`, the
 * `(array)` cast, `json_encode`, `foreach`, `serialize`, `get_object_vars`),
 * and leaves the DECLARATION alone: the properties still have no default, so
 * Reflection's hasDefaultValue() answers false the way php's does.
 */
PH7_PRIVATE sxi32 PH7_NativeClassInstallNewHook(ph7_vm *pVm,const char *zClass,
	void (*xNew)(ph7_vm *,ph7_class_instance *))
{
	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);
	if( pClass == 0 ){
		return SXERR_NOTFOUND;
	}
	pClass->xNew = xNew;
	return SXRET_OK;
}
/*
 * Install a write handler on a mounted native class and mark every INSTANCE
 * property it declares as filtered, which is what makes instantiation register
 * the slots the filter looks up. Called by the owning installer right after
 * PH7_InstallNativeClasses, for the same reason xClone and xDim are: the spec
 * table has no field for a hook.
 */
PH7_PRIVATE sxi32 PH7_NativeClassInstallSetHook(ph7_vm *pVm,const char *zClass,
	void (*xSet)(ph7_vm *,ph7_class_instance *,PH7_NativeSetCtx *))
{
	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);
	SyHashEntry *pEntry;
	if( pClass == 0 ){
		return SXERR_NOTFOUND;
	}
	pClass->xSet = xSet;
	SyHashResetLoopCursor(&pClass->hAttr);
	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){
		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;
		if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT)) == 0 ){
			pAttr->iFlags |= PH7_CLASS_ATTR_NATIVE_SET;
		}
	}
	return SXRET_OK;
}
/*
 * The nearest ph7_class::xCmp in a class's base chain -- the same handler
 * inheritance xDim and xSet get, and php's own: a subclass of DateTime still
 * compares as an instant, whatever properties it adds.
 */
static ph7_class * NativeCmpClass(ph7_class *pClass)
{
	while( pClass ){
		if( pClass->xCmp ){
			return pClass;
		}
		pClass = pClass->pBase;
	}
	return 0;
}
/*
 * Ask the LEFT operand's compare handler, php-style. Answers 0 when no class in
 * its chain has one (the caller falls back to the property walk); 1 when the
 * handler decided, and *pResult is then the ordering -- which includes a
 * REFUSAL, recorded on the VM for the nearest throw boundary to raise, with the
 * uncomparable 1 standing in as the answer meanwhile.
 */
PH7_PRIVATE int PH7_ClassNativeCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,sxi32 *pResult)
{
	ph7_class *pClass = pLeft ? NativeCmpClass(pLeft->pClass) : 0;
	PH7_NativeCmpCtx sCtx;
	ph7_vm *pVm;
	if( pClass == 0 ){
		return 0;
	}
	pVm = pLeft->pVm;
	SyZero(&sCtx,sizeof(sCtx));
	sCtx.pOther = pRight;
	sCtx.iResult = 1;   /* php's ZEND_UNCOMPARABLE: what a hook that recognizes nothing answers */
	pClass->xCmp(pVm,pLeft,&sCtx);
	if( sCtx.zThrowClass && pVm->zCmpRefusalClass == 0 ){
		/* First refusal wins: a driver that keeps comparing after one (sort() does)
		 * must not overwrite the message the script will actually see. */
		pVm->zCmpRefusalClass = sCtx.zThrowClass;
		SyMemcpy(sCtx.zThrowMsg,pVm->zCmpRefusalMsg,sizeof(pVm->zCmpRefusalMsg));
		pVm->zCmpRefusalMsg[sizeof(pVm->zCmpRefusalMsg)-1] = 0;
	}
	*pResult = sCtx.iResult;
	return 1;
}
/*
 * The same handler, asked about a SCALAR partner -- php's compare handler is one
 * door and `$n == 2` reaches it exactly as `$n == $m` does. Answers 1 only when
 * the hook RECOGNIZED the value; otherwise the caller falls back to php's
 * cast-the-object rule, which is what every class without a handler gets.
 */
PH7_PRIVATE int PH7_ClassNativeCmpValue(ph7_class_instance *pLeft,ph7_value *pOther,
	int bReversed,sxi32 *pResult)
{
	ph7_class *pClass = pLeft ? NativeCmpClass(pLeft->pClass) : 0;
	PH7_NativeCmpCtx sCtx;
	ph7_vm *pVm;
	if( pClass == 0 ){
		return 0;
	}
	pVm = pLeft->pVm;
	SyZero(&sCtx,sizeof(sCtx));
	sCtx.pOtherValue = pOther;
	sCtx.bReversed = bReversed;
	sCtx.iResult = 1;   /* php's ZEND_UNCOMPARABLE */
	pClass->xCmp(pVm,pLeft,&sCtx);
	if( sCtx.zThrowClass && pVm->zCmpRefusalClass == 0 ){
		pVm->zCmpRefusalClass = sCtx.zThrowClass;
		SyMemcpy(sCtx.zThrowMsg,pVm->zCmpRefusalMsg,sizeof(pVm->zCmpRefusalMsg));
		pVm->zCmpRefusalMsg[sizeof(pVm->zCmpRefusalMsg)-1] = 0;
	}
	if( !sCtx.bAnswered ){
		return 0;
	}
	*pResult = sCtx.iResult;
	return 1;
}
/*
 * Install a compare handler on a mounted native class. Called by the owning
 * installer right after PH7_InstallNativeClasses, for the reason xClone, xDim
 * and xSet are: the spec table has no field for a hook.
 */
/*
 * php's cast_object handler for _IS_BOOL, which is the one conversion an object
 * may decide for itself. Answers 1 when the class HAS a handler, with the truth
 * value in *pOut; every other class keeps php's rule that an object is truthy.
 */
PH7_PRIVATE int PH7_ClassNativeBool(ph7_class_instance *pThis,int *pOut)
{
	ph7_class *pClass;
	for( pClass = pThis ? pThis->pClass : 0 ; pClass ; pClass = pClass->pBase ){
		if( pClass->xBool ){
			*pOut = pClass->xBool(pThis->pVm,pThis) ? 1 : 0;
			return 1;
		}
	}
	return 0;
}
PH7_PRIVATE sxi32 PH7_NativeClassInstallBoolHook(ph7_vm *pVm,const char *zClass,
	int (*xBool)(ph7_vm *,ph7_class_instance *))
{
	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);
	if( pClass == 0 ){
		return SXERR_NOTFOUND;
	}
	pClass->xBool = xBool;
	return SXRET_OK;
}
PH7_PRIVATE sxi32 PH7_NativeClassInstallArithHook(ph7_vm *pVm,const char *zClass,
	void (*xArith)(ph7_vm *,ph7_class_instance *,PH7_NativeArithCtx *))
{
	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);
	if( pClass == 0 ){
		return SXERR_NOTFOUND;
	}
	pClass->xArith = xArith;
	return SXRET_OK;
}
/*
 * php's compare handler for an OPAQUE HANDLE class -- one whose object stands for
 * something outside the engine (a curl easy/multi/share handle, a PDO connection,
 * a statement, a lazy row). php gives each of them a handler that recognizes
 * NOTHING, so every comparison that is not the identity shortcut is
 * ZEND_UNCOMPARABLE: `$h == 1`, `$h < 2`, `$h > 0` and `$h == $other` are all
 * false, `$h <=> $x` is 1 from either direction, and none of it says a word.
 * Without it these fell through to php's cast-the-object rule, which warns
 * `could not be converted to int` and then calls the handle equal to 1 -- so
 * in_array($h, [1,2,3]) was TRUE, and sorting a list that held one was noise.
 *
 * The uncomparable 1 is deliberately not flipped for bReversed: php answers it
 * from both sides alike, which is what leaves every relational spelling false.
 */
PH7_PRIVATE void PH7_NativeCmpOpaqueHandle(ph7_vm *pVm,ph7_class_instance *pThis,
	PH7_NativeCmpCtx *pCtx)
{
	SXUNUSED(pVm);
	SXUNUSED(pThis);
	if( pCtx->pOtherValue && (pCtx->pOtherValue->iFlags & MEMOBJ_BOOL) ){
		return;   /* declined: php's cast rule decides an object against a bool */
	}
	pCtx->bAnswered = 1;
	pCtx->iResult = 1;   /* php's ZEND_UNCOMPARABLE */
}
/*
 * TRUE when `(int)` on an instance of this class answers the object handle
 * (PH7_CLASS_HANDLE_ID). Resolved through the ANCESTORS, like every other native
 * hook: php installs the cast on the class's object handlers, and a subclass
 * inherits the whole handler table.
 */
PH7_PRIVATE int PH7_ClassCastsToHandleId(ph7_class *pClass)
{
	while( pClass ){
		if( pClass->iFlags & PH7_CLASS_HANDLE_ID ){
			return 1;
		}
		pClass = pClass->pBase;
	}
	return 0;
}
/* The same base-chain question for the two flags beside it: a subclass of
 * SimpleXMLElement casts to a number and answers get_object_vars the way its
 * parent does, which is php's handler inheritance. */
PH7_PRIVATE int PH7_ClassNumberIsString(ph7_class *pClass)
{
	while( pClass ){
		if( pClass->iFlags & PH7_CLASS_NUM_AS_STRING ){
			return 1;
		}
		pClass = pClass->pBase;
	}
	return 0;
}
PH7_PRIVATE int PH7_ClassVarsFromPresent(ph7_class *pClass)
{
	while( pClass ){
		if( pClass->iFlags & PH7_CLASS_VARS_PRESENT ){
			return 1;
		}
		pClass = pClass->pBase;
	}
	return 0;
}
PH7_PRIVATE sxi32 PH7_NativeClassInstallCmpHook(ph7_vm *pVm,const char *zClass,
	void (*xCmp)(ph7_vm *,ph7_class_instance *,PH7_NativeCmpCtx *))
{
	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);
	if( pClass == 0 ){
		return SXERR_NOTFOUND;
	}
	pClass->xCmp = xCmp;
	return SXRET_OK;
}
/*
 * Mark every INSTANCE property a mounted native class declares as one php
 * FABRICATES rather than stores (PH7_CLASS_ATTR_NATIVE_VIRTUAL), which is what
 * keeps the object comparator from seeing it. DatePeriod is the whole caller
 * list: php's object has an EMPTY real property table, so two of them are equal
 * whatever they contain, while a subclass's own property still decides.
 */
PH7_PRIVATE sxi32 PH7_NativeClassMarkVirtualProps(ph7_vm *pVm,const char *zClass)
{
	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);
	SyHashEntry *pEntry;
	if( pClass == 0 ){
		return SXERR_NOTFOUND;
	}
	SyHashResetLoopCursor(&pClass->hAttr);
	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){
		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;
		if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT)) == 0 ){
			pAttr->iFlags |= PH7_CLASS_ATTR_NATIVE_VIRTUAL;
		}
	}
	return SXRET_OK;
}
/*
 * Mark every php-VISIBLE instance property a mounted native class declares as one
 * the OBJECT does not hold until its constructor fills it
 * (PH7_CLASS_ATTR_NATIVE_LAZY). php's DateInterval and DatePeriod are the caller
 * list: the state is a C struct the constructor allocates and the property table
 * is written FROM it, so an object nobody constructed has no such property at all.
 *
 * The HIDDEN slots are left alone -- they are PHL's own storage, they have to
 * exist from `new` (the initialized FLAG lives in one of them), and php shows
 * nothing for them either way.
 *
 * bDefaultRead selects which of php's two handlers the class has: with it, a read
 * of a still-absent slot answers the DECLARED literal in silence (DatePeriod's
 * read_property over the zeroed struct); without it, the name really is undefined
 * until the constructor runs (DateInterval).
 */
/*
 * Mark every php-VISIBLE instance property a mounted native class declares as one
 * whose WRITE php's handler refuses (PH7_CLASS_ATTR_NATIVE_NOWRITE). DatePeriod is
 * the caller list: php answers `Cannot modify readonly property DatePeriod::$p` to
 * every write form and `Cannot unset DatePeriod::$p` to an unset, while Reflection
 * still reports isReadOnly() false -- the wording is the handler's, not the
 * readonly flag's. The C bodies that fill the seven write their slots directly and
 * never pass the store filter, so the refusal costs the class nothing.
 */
PH7_PRIVATE sxi32 PH7_NativeClassMarkNoWriteProps(ph7_vm *pVm,const char *zClass)
{
	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);
	SyHashEntry *pEntry;
	if( pClass == 0 ){
		return SXERR_NOTFOUND;
	}
	SyHashResetLoopCursor(&pClass->hAttr);
	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){
		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;
		if( pAttr->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT|PH7_CLASS_ATTR_HIDDEN) ){
			continue;
		}
		pAttr->iFlags |= PH7_CLASS_ATTR_NATIVE_NOWRITE;
	}
	return SXRET_OK;
}
/*
 * Mark ONE property of ONE instance as php's read-only kind: a plain store and
 * an unset() refuse with `Property p is read only`, everything that takes a
 * pointer to it goes through. It is marked per OBJECT because php's handler is
 * -- a PDOStatement with no cursor behind it takes the write, and only the one
 * a driver built refuses.
 */
PH7_PRIVATE void PH7_NativeMarkAttrReadOnly(ph7_class_instance *pThis,const char *zProp)
{
	SyHashEntry *pEntry = pThis
		? SyHashGet(&pThis->hAttr,(const void *)zProp,(sxu32)SyStrlen(zProp)) : 0;
	if( pEntry ){
		((VmClassAttr *)pEntry->pUserData)->iState |= VM_CLASS_ATTR_RDONLY;
	}
}
PH7_PRIVATE sxi32 PH7_NativeClassMarkLazyProps(ph7_vm *pVm,const char *zClass,int bDefaultRead)
{
	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);
	SyHashEntry *pEntry;
	if( pClass == 0 ){
		return SXERR_NOTFOUND;
	}
	SyHashResetLoopCursor(&pClass->hAttr);
	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){
		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;
		if( pAttr->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT|PH7_CLASS_ATTR_HIDDEN) ){
			continue;
		}
		pAttr->iFlags |= PH7_CLASS_ATTR_NATIVE_LAZY;
		if( bDefaultRead ){
			pAttr->iFlags |= PH7_CLASS_ATTR_NATIVE_LAZY_DEFAULT;
		}
		pClass->iFlags |= PH7_CLASS_LAZY_ATTR;
	}
	return SXRET_OK;
}
/*
 * Install this object's LAZY properties -- the whole set, in the order the class
 * declares them, skipping any the object already carries.
 *
 * The ORDER is php's: its constructor writes the struct's fields into the property
 * table one after another, so a name the object already has keeps its POSITION and
 * only takes the new value, and the rest are appended in declared order behind it.
 * VmRecreateDeclaredAttr tail-inserts exactly that way.
 *
 * The declared literal goes in as the slot's starting value (php's zeroed struct),
 * and the not-yet-initialized mark a TYPED slot would carry is cleared with it:
 * these are filled by the C body that is about to write them, and a read between
 * the two is php's default, not its Error.
 */
PH7_PRIVATE void PH7_NativeMaterializeLazy(ph7_vm *pVm,ph7_class_instance *pObj)
{
	SyHashEntry *pEntry;
	if( pObj == 0 || (pObj->pClass->iFlags & PH7_CLASS_LAZY_ATTR) == 0
	 || (pObj->iFlags & VM_INSTANCE_LAZY_DONE) ){
		return;
	}
	pObj->iFlags |= VM_INSTANCE_LAZY_DONE;
	SyHashResetLoopCursor(&pObj->pClass->hAttr);
	while( (pEntry = SyHashGetNextEntry(&pObj->pClass->hAttr)) != 0 ){
		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;
		VmClassAttr *pVmAttr = 0;
		ph7_value *pSlot;
		if( (pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_LAZY) == 0
		 || (pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_ONDEMAND) != 0 ){
			continue;   /* ...and an ON-DEMAND one waits for the write that names it */
		}
		if( SyHashGet(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName)) != 0 ){
			continue;
		}
		VmRecreateDeclaredAttr(&(*pVm),pObj,pAttr,&pVmAttr);
		if( pVmAttr == 0 ){
			continue;   /* OOM: the caller's write lands nowhere, as it would have anyway */
		}
		pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;
		if( pAttr->pNativeValue == 0 ){
			continue;
		}
		pSlot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pVmAttr->nIdx);
		if( pSlot ){
			PH7_NativeLiteralValue(&(*pVm),pAttr->pNativeValue,pSlot);
		}
	}
}
/*
 * Create and install ONE class from its spec: constants and properties, but
 * neither methods nor its base chain.
 *
 * Split from the two passes that follow because a spec table may describe
 * classes that extend each other, and PH7_ClassInherit needs the parent to
 * exist -- and to already CARRY ITS METHODS, since inheriting is what copies
 * them down.
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
	/* PH7_CLASS_BOUND: an engine class is declared unconditionally, exactly once,
	 * so `class DateTime {}` in user code is php's "Cannot redeclare class
	 * DateTime". Only the compiler used to set the flag, so EVERY native class
	 * was silently redeclarable — the chunk-declared ones (Exception, stdClass)
	 * fataled and their C replacements did not. */
	pClass->iFlags |= pSpec->iFlags | PH7_CLASS_BOUND;
	pClass->xRelease = pSpec->xRelease;
	pClass->pIterVtab = pSpec->pIterVtab;
	pClass->xPresent = pSpec->xPresent;
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
	*ppOut = pClass;
	return PH7_VmInstallClass(&(*pVm),pClass);
}
/*
 * Wire ONE class's base chain and interfaces.
 *
 * This runs AFTER every class in the table has its own methods, which is the
 * compiler's order too (GenStateCompileClassEx compiles the whole body and only
 * then calls PH7_ClassInherit/PH7_ClassImplement). Two things depend on it:
 * PH7_ClassInherit COPIES the base's methods into the subclass, so a base whose
 * methods were not installed yet hands down an empty table -- which is how
 * `DOMDocument::C14N()` came out undefined, the first time a native class
 * extended another native class that had methods; and PH7_ClassImplement stubs
 * every interface method the class lacks as ABSTRACT, so a spec may now name an
 * interface it implements itself rather than attaching it by hand afterwards.
 */
static sxi32 NativeLinkClass(ph7_vm *pVm,const PH7_NativeClassSpec *pSpec,ph7_class *pClass)
{
	sxi32 rc;
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
	return SXRET_OK;
}
/*
 * Install a whole table of native classes, in the compiler's own order: declare
 * them all (so later rows may extend earlier ones), fill in their methods, wire
 * the base chains and interfaces, then mount.
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
		rc = NativeLinkClass(&(*pVm),&aSpec[i],apClass[i]);
		if( rc != SXRET_OK ){
			goto Done;
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
 * Declaring an ENUM from C.
 *
 * An enum is not a class with constants: each `case` is a class constant whose
 * slot holds THE singleton instance of the enum for that case, materialized
 * lazily on first access (VmEnumMaterializeCase). The compiler builds one by
 * declaring the readonly `name`/`value` properties, pushing each case onto
 * ph7_class::aEnumCases, and SYNTHESIZING cases()/from()/tryFrom() as PHP
 * source that forwards to the `__phl_enum_*` thunks.
 *
 * This does the same three things without a compiler: the case's backing value
 * rides as a literal (ph7_class_attr::pNativeValue, which the materializer
 * reads where a compiled case has byte-code), and the three interface methods
 * are C bodies that call the very same engine workers the synthesized PHP
 * forwards to — so a native enum is an ordinary one to `instanceof`,
 * `match`, Reflection and `===` case identity.
 * ---------------------------------------------------------------------------
 */
/* The enum a static native method was called on. */
static ph7_class * NativeEnumSelf(ph7_context *pCtx,ph7_value *pName)
{
	ph7_class *pClass = PH7_ContextCalledClass(pCtx);
	PH7_MemObjInit(pCtx->pVm,pName);
	if( pClass ){
		ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));
	}
	return pClass;
}
/*
 * cases() / from() / tryFrom(): the engine thunks take the enum's FQN as their
 * first argument, exactly as the compiler's synthesized bodies pass it.
 */
static int vm_builtin_NativeEnum_cases(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value sName;
	ph7_value *ap[1];
	int rc;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( NativeEnumSelf(pCtx,&sName) == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	ap[0] = &sName;
	rc = vm_builtin_enum_cases(pCtx,1,ap);
	PH7_MemObjRelease(&sName);
	return rc;
}
static int NativeEnumFrom(ph7_context *pCtx,int nArg,ph7_value **apArg,int bTry)
{
	ph7_value sName;
	ph7_value *ap[2];
	int rc;
	if( nArg < 1 || NativeEnumSelf(pCtx,&sName) == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	ap[0] = &sName;
	ap[1] = apArg[0];
	rc = bTry ? vm_builtin_enum_tryfrom(pCtx,2,ap) : vm_builtin_enum_from(pCtx,2,ap);
	PH7_MemObjRelease(&sName);
	return rc;
}
static int vm_builtin_NativeEnum_from(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return NativeEnumFrom(pCtx,nArg,apArg,0);
}
static int vm_builtin_NativeEnum_tryFrom(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return NativeEnumFrom(pCtx,nArg,apArg,1);
}
/* The readonly `name` (every enum) and `value` (backed only) case properties,
 * declared exactly as GenStateCompileEnum does. */
static sxi32 NativeEnumInstallProp(ph7_vm *pVm,ph7_class *pClass,const char *zName,
	sxu32 nType,const char *zTypeName)
{
	SyString sName;
	ph7_class_attr *pAttr;
	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));
	pAttr = PH7_NewClassAttr(&(*pVm),&sName,0,PH7_CLASS_PROT_PUBLIC,
		PH7_CLASS_ATTR_READONLY|PH7_CLASS_ATTR_TYPED);
	if( pAttr == 0 ){
		return SXERR_MEM;
	}
	pAttr->nType = nType;
	SyStringInitFromBuf(&pAttr->sTypeName,zTypeName,SyStrlen(zTypeName));
	return PH7_ClassInstallAttr(pClass,pAttr);
}
/*
 * cases()/from()/tryFrom(), installed on ANY enum -- one declared from C here
 * and one the compiler just finished reading from source. php declares them on
 * the UnitEnum/BackedEnum prototypes, which is why `from` takes `string|int`
 * rather than the enum's own backing type: the refusal for the wrong one is a
 * VALUE check inside the body, not the parameter's.
 *
 * The compiler used to synthesize PHP source forwarding to three global
 * `__phl_enum_*` thunks. Those names were php-visible, and the methods reported
 * as `<user>` with the enum's file and line where php reports
 * `<internal, prototype BackedEnum>`.
 */
/*
 * Install one of them and stamp it INTERNAL unconditionally. The usual
 * `bCompilingBuiltin` stamp only fires while the engine compiles its OWN
 * sources, and these three are attached to a class the compiler is reading out
 * of a USER file — but php reports them as internal wherever the enum is
 * declared: isInternal() true, getFileName() false, getStartLine() 0.
 */
static sxi32 NativeEnumInstallMethod(ph7_vm *pVm,ph7_class *pClass,
	const PH7_NativeMethodDef *pDef)
{
	ph7_class_method *pMeth;
	sxi32 rc = PH7_NativeClassInstallMethod(&(*pVm),pClass,pDef,0);
	if( rc != SXRET_OK ){
		return rc;
	}
	pMeth = PH7_ClassExtractMethod(pClass,pDef->zName,(sxu32)SyStrlen(pDef->zName));
	if( pMeth ){
		pMeth->sFunc.iFlags |= VM_FUNC_INTERNAL;
		/* getFileName() reads the recorded source file rather than the flag, and
		 * PH7_NewClassMethod stamped the user file the enum was read from. */
		SyStringInitFromBuf(&pMeth->sFunc.sFile,"",0);
	}
	return SXRET_OK;
}
PH7_PRIVATE sxi32 PH7_InstallEnumInterfaceMethods(ph7_vm *pVm,ph7_class *pClass)
{
	static const PH7_NativeMethodDef sCases =
		{ "cases", PH7_MOD_PUBLIC|PH7_MOD_STATIC, "", "array", vm_builtin_NativeEnum_cases };
	static const PH7_NativeMethodDef aFrom[] = {
		{ "from",    PH7_MOD_PUBLIC|PH7_MOD_STATIC, "string|int $value", "static",
		  vm_builtin_NativeEnum_from },
		{ "tryFrom", PH7_MOD_PUBLIC|PH7_MOD_STATIC, "string|int $value", "?static",
		  vm_builtin_NativeEnum_tryFrom },
	};
	sxu32 n;
	sxi32 rc = NativeEnumInstallMethod(&(*pVm),pClass,&sCases);
	if( rc != SXRET_OK || pClass->nEnumBacking == 0 ){
		return rc;
	}
	for( n = 0 ; n < SX_ARRAYSIZE(aFrom) ; n++ ){
		rc = NativeEnumInstallMethod(&(*pVm),pClass,&aFrom[n]);
		if( rc != SXRET_OK ){
			return rc;
		}
	}
	return SXRET_OK;
}
PH7_PRIVATE sxi32 PH7_InstallNativeEnum(ph7_vm *pVm,const char *zName,sxu32 nBacking,
	const PH7_NativeEnumCase *aCase,sxu32 nCase,
	const PH7_NativeMethodDef *aMethod,sxu32 nMethod)
{
	ph7_class *pClass, *pIface;
	SyString sName;
	sxu32 n;
	sxi32 rc;
	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));
	pClass = PH7_NewRawClass(&(*pVm),&sName,0);
	if( pClass == 0 ){
		return SXERR_MEM;
	}
	/* php: no enum can be extended or instantiated, and the ENUM flag alone says
	 * so -- the `extends` refusal names the enum rather than a final class, and it
	 * is asked first. The FINAL flag is deliberately NOT set: php stamps
	 * ZEND_ACC_FINAL on a COMPILED enum only, so `isFinal()`/`getModifiers()`
	 * answer true/32 for `enum U {}` and false/0 for every enum php declares from
	 * C (RoundingMode, PropertyHookType). Setting it here made an internal enum
	 * report itself as a userland one. */
	pClass->iFlags |= PH7_CLASS_ENUM;
	pClass->nEnumBacking = nBacking;
	rc = NativeEnumInstallProp(&(*pVm),pClass,"name",MEMOBJ_STRING,"string");
	if( rc != SXRET_OK ){
		return rc;
	}
	if( nBacking != 0 ){
		rc = NativeEnumInstallProp(&(*pVm),pClass,"value",nBacking,
			nBacking == MEMOBJ_INT ? "int" : "string");
		if( rc != SXRET_OK ){
			return rc;
		}
	}
	for( n = 0 ; n < nCase ; n++ ){
		ph7_class_attr *pAttr;
		SyString sCase;
		SyStringInitFromBuf(&sCase,aCase[n].zName,SyStrlen(aCase[n].zName));
		pAttr = PH7_NewClassAttr(&(*pVm),&sCase,0,PH7_CLASS_PROT_PUBLIC,
			PH7_CLASS_ATTR_CONSTANT|PH7_CLASS_ATTR_ENUMCASE);
		if( pAttr == 0 ){
			return SXERR_MEM;
		}
		pAttr->pDeclClass = pClass;
		/* The backing literal where a compiled case carries byte-code. */
		if( nBacking != 0 ){
			pAttr->pNativeValue = &aCase[n].sValue;
		}
		rc = PH7_ClassInstallAttr(pClass,pAttr);
		if( rc != SXRET_OK ){
			return rc;
		}
		/* Declaration order, which is the order cases() reports. */
		SySetPut(&pClass->aEnumCases,(const void *)&pAttr);
	}
	for( n = 0 ; n < nMethod ; n++ ){
		rc = PH7_NativeClassInstallMethod(&(*pVm),pClass,&aMethod[n],0);
		if( rc != SXRET_OK ){
			return rc;
		}
	}
	rc = PH7_InstallEnumInterfaceMethods(&(*pVm),pClass);
	if( rc != SXRET_OK ){
		return rc;
	}
	rc = PH7_VmInstallClass(&(*pVm),pClass);
	if( rc != SXRET_OK ){
		return rc;
	}
	/* php 8.1: every enum satisfies `instanceof UnitEnum`, a backed one
	 * `BackedEnum` too. Attached AFTER the methods, so PH7_ClassImplement's
	 * abstract stubbing finds cases()/from()/tryFrom() already declared.
	 * A backed one names only BackedEnum, which BRINGS UnitEnum: php's own
	 * internal enums list `BackedEnum, UnitEnum` in that order, and naming
	 * both here would answer them the other way round. A compiled enum is a
	 * different registration and really does name both (`Ct, UnitEnum,
	 * BackedEnum`), which is what compile_class.c spells. */
	pIface = NativeLookupClass(&(*pVm),nBacking != 0 ? "BackedEnum" : "UnitEnum");
	if( pIface == 0 ){
		return SXERR_NOTFOUND;
	}
	rc = PH7_ClassImplement(pClass,pIface);
	if( rc != SXRET_OK ){
		return rc;
	}
	return VmMountUserClass(&(*pVm),pClass);
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
/*
 * The walk THIS iterator was made for: the vtable of the aggregate it holds,
 * looked up ALONG THE BASE CHAIN. A subclass of a native aggregate inherits the
 * walk the way it inherits the getIterator() that reaches it -- without this a
 * `class P extends DatePeriod {}` (or DOMNodeList, WeakMap, PDOStatement,
 * FilesystemIterator) answered a real InternalIterator that yielded NOTHING, so
 * every foreach over one was silently empty.
 */
static const PH7_NativeIterVtab * NativeIterVtab(ph7_class_instance *pIt)
{
	ph7_class_instance *pSrc = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);
	ph7_class *pClass;
	for( pClass = pSrc ? pSrc->pClass : 0 ; pClass ; pClass = pClass->pBase ){
		if( pClass->pIterVtab ){
			return pClass->pIterVtab;
		}
	}
	return 0;
}
/* Hand the cursor back to the aggregate, for a class that shows its walk as one
 * of its own properties (see PH7_NativeIterVtab::xPublish). Every InternalIterator
 * method calls this -- php's aggregate is written from the iterator's methods, not
 * from the walk, so a getIterator() nobody has touched yet leaves it alone. */
static void NativeIterPublish(ph7_vm *pVm,ph7_class_instance *pIt)
{
	const PH7_NativeIterVtab *pVtab = NativeIterVtab(pIt);
	if( pVtab && pVtab->xPublish ){
		pVtab->xPublish(&(*pVm),pIt);
	}
}
/* May this iterator be walked? See PH7_NativeIterVtab::xGuard -- the aggregate
 * gets to refuse at each of the five methods, which is where php refuses. */
static int NativeIterRefused(ph7_context *pCtx,ph7_class_instance *pIt)
{
	const PH7_NativeIterVtab *pVtab = NativeIterVtab(pIt);
	return (pVtab && pVtab->xGuard) ? pVtab->xGuard(pCtx,pIt) : 0;
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
	if( NativeIterRefused(pCtx,pThis) ){
		return PH7_OK;
	}
	pVtab = NativeIterVtab(pThis);
	if( pVtab == 0 || pVtab->xRewind == 0 ){
		PH7_NativeSetAttrBool(pCtx->pVm,pThis,PH7_NATIVE_IT_DONE,1);
		return PH7_OK;
	}
	pVtab->xRewind(pCtx->pVm,pThis);
	NativeIterPublish(pCtx->pVm,pThis);
	return PH7_OK;
}
static int vm_builtin_InternalIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const PH7_NativeIterVtab *pVtab;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis && NativeIterRefused(pCtx,pThis) ){
		return PH7_OK;
	}
	if( pThis == 0 || PH7_NativeAttrTruthy(pThis,PH7_NATIVE_IT_DONE) ){
		return PH7_OK;
	}
	pVtab = NativeIterVtab(pThis);
	if( pVtab == 0 || pVtab->xNext == 0 ){
		PH7_NativeSetAttrBool(pCtx->pVm,pThis,PH7_NATIVE_IT_DONE,1);
		return PH7_OK;
	}
	pVtab->xNext(pCtx->pVm,pThis);
	NativeIterPublish(pCtx->pVm,pThis);
	return PH7_OK;
}
static int vm_builtin_InternalIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis && NativeIterRefused(pCtx,pThis) ){
		return PH7_OK;
	}
	if( pThis ){
		NativeIterPublish(pCtx->pVm,pThis);
	}
	ph7_result_bool(pCtx,pThis != 0 && !PH7_NativeAttrTruthy(pThis,PH7_NATIVE_IT_DONE));
	return PH7_OK;
}
/* current() and key() answer the slots the walk left behind -- and NULL once it
 * is over, which is what php's exhausted InternalIterator answers too. */
static int NativeIterReadSlot(ph7_context *pCtx,const char *zSlot)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value *pVal;
	if( pThis && NativeIterRefused(pCtx,pThis) ){
		return PH7_OK;
	}
	if( pThis ){
		NativeIterPublish(pCtx->pVm,pThis);
	}
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
	pVtab = NativeIterVtab(pIt);
	if( pVtab && pVtab->xRewind ){
		pVtab->xRewind(&(*pVm),pIt);
	}
	return pIt;
}
PH7_PRIVATE sxi32 PH7_VmInstallNativeIterator(ph7_vm *pVm)
{
	static const PH7_NativePropDef aProp[] = {
		{ PH7_NATIVE_IT_SRC,  PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		{ PH7_NATIVE_IT_CUR,  PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		{ PH7_NATIVE_IT_KEY,  PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		{ PH7_NATIVE_IT_POS,  PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,  0, 0, 0.0 }, 0 },
		{ PH7_NATIVE_IT_AUX,  PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,  0, 0, 0.0 }, 0 },
		{ PH7_NATIVE_IT_DONE, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_BOOL, 1, 0, 0.0 }, 0 },
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
		  aMethod, SX_ARRAYSIZE(aMethod), 0, 0, aProp, SX_ARRAYSIZE(aProp), 0, 0, 0 },
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
