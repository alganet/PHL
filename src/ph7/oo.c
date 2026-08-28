/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
/*
 * This file implement an Object Oriented (OO) subsystem for the PH7 engine.
 */
/*
 * Create an empty class.
 * Return a pointer to a raw class (ph7_class instance) on success. NULL otherwise.
 */
PH7_PRIVATE ph7_class * PH7_NewRawClass(ph7_vm *pVm,const SyString *pName,sxu32 nLine)
{
	ph7_class *pClass;
	char *zName;
	/* Allocate a new instance */
	pClass = (ph7_class *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class));
	if( pClass == 0 ){
		return 0;
	}
	/* Zero the structure */
	SyZero(pClass,sizeof(ph7_class));
	/* Duplicate class name */
	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);
	if( zName == 0 ){
		SyMemBackendPoolFree(&pVm->sAllocator,pClass);
		return 0;
	}
	/* Initialize fields */
	SyStringInitFromBuf(&pClass->sName,zName,pName->nByte);
	SyHashInit(&pClass->hMethod,&pVm->sAllocator,0,0);
	SyHashInit(&pClass->hAttr,&pVm->sAllocator,0,0);
	SyHashInit(&pClass->hDerived,&pVm->sAllocator,0,0);
	SySetInit(&pClass->aInterface,&pVm->sAllocator,sizeof(ph7_class *));
	SySetInit(&pClass->aTrait,&pVm->sAllocator,sizeof(ph7_class *));
	SySetInit(&pClass->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));
	SySetInit(&pClass->aEnumCases,&pVm->sAllocator,sizeof(ph7_class_attr *));
	pClass->nLine = nLine;
	if( pVm->bCompilingBuiltin ){
		/* Defined by an embedded builtin chunk: internal, no defining file.
		 * Class compilers merge further flags with |= so this survives. */
		pClass->iFlags |= PH7_CLASS_INTERNAL;
	}else{
		/* Alias the VM-lifetime path dup on top of the include stack */
		SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);
		if( pFile ){
			SyStringDupPtr(&pClass->sFile,pFile);
		}
	}
	/* All done */
	return pClass;
}
/*
 * Allocate and initialize a new class attribute.
 * Return a pointer to the class attribute on success. NULL otherwise.
 */
PH7_PRIVATE ph7_class_attr * PH7_NewClassAttr(ph7_vm *pVm,const SyString *pName,sxu32 nLine,sxi32 iProtection,sxi32 iFlags)
{
	ph7_class_attr *pAttr;
	char *zName;
	pAttr = (ph7_class_attr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_attr));
	if( pAttr == 0 ){
		return 0;
	}
	/* Zero the structure */
	SyZero(pAttr,sizeof(ph7_class_attr));
	SySetInit(&pAttr->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));
	/* Duplicate attribute name */
	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);
	if( zName == 0 ){
		SyMemBackendPoolFree(&pVm->sAllocator,pAttr);
		return 0;
	}
	/* Initialize fields */
	SySetInit(&pAttr->aByteCode,&pVm->sAllocator,sizeof(VmInstr));
	SySetInit(&pAttr->aUnionAlts,&pVm->sAllocator,sizeof(ph7_type_alt));
	SyStringInitFromBuf(&pAttr->sName,zName,pName->nByte);
	pAttr->iProtection = iProtection;
	pAttr->nIdx = SXU32_HIGH;
	pAttr->iFlags = iFlags;
	pAttr->nLine = nLine;
	return pAttr;
}
/*
 * Allocate and initialize a new class method.
 * Return a pointer to the class method on success. NULL otherwise
 * This function associate with the newly created method an automatically generated
 * random unique name.
 */
PH7_PRIVATE ph7_class_method * PH7_NewClassMethod(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,sxu32 nLine,
	sxi32 iProtection,sxi32 iFlags,sxi32 iFuncFlags)
{
	ph7_class_method *pMeth;
	SyHashEntry *pEntry;
	SyString *pNamePtr;
	char zSalt[10];
	char *zName;
	sxu32 nByte;
	/* Allocate a new class method instance */
	pMeth = (ph7_class_method *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_method));
	if( pMeth == 0 ){
		return 0;
	}
	/* Zero the structure */
	SyZero(pMeth,sizeof(ph7_class_method));
	/* Check for an already installed method with the same name */
	pEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);
	if( pEntry == 0 ){
		/* Associate an unique VM name to this method */
		nByte = sizeof(zSalt) + pName->nByte + SyStringLength(&pClass->sName)+sizeof(char)*7/*[[__'\0'*/;
		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,nByte);
		if( zName == 0 ){
			SyMemBackendPoolFree(&pVm->sAllocator,pMeth);
			return 0;
		}
		pNamePtr = &pMeth->sVmName;
		/* Generate a random string */
		PH7_VmRandomString(&(*pVm),zSalt,sizeof(zSalt));
		pNamePtr->nByte = SyBufferFormat(zName,nByte,"[__%z@%z_%.*s]",&pClass->sName,pName,sizeof(zSalt),zSalt);
		pNamePtr->zString = zName;
	}else{
		/* Method is condidate for 'overloading' */
		ph7_class_method *pCurrent = (ph7_class_method *)pEntry->pUserData;
		pNamePtr = &pMeth->sVmName;
		/* Use the same VM name */
		SyStringDupPtr(pNamePtr,&pCurrent->sVmName);
		zName = (char *)pNamePtr->zString;
	}
	if( iProtection != PH7_CLASS_PROT_PUBLIC ){
		if( (pName->nByte == sizeof("__destruct") - 1 && SyMemcmp(pName->zString,"__destruct",sizeof("__destruct") - 1 ) == 0)
			|| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){
				/* Switch to public visibility for destructors and legacy class-name
				 * constructors (the engine invokes destructors internally, bypassing
				 * visibility either way). __construct KEEPS its declared visibility
				 * (band A #4): php enforces it at `new` — a private/protected ctor
				 * from the wrong scope is a catchable Error, checked at OP_NEW —
				 * and ReflectionClass::isInstantiable()/newInstance() now see it. */
				iProtection = PH7_CLASS_PROT_PUBLIC;
		}
	}
	/* Initialize method fields */
	pMeth->iProtection = iProtection;
	pMeth->iFlags = iFlags;
	pMeth->nLine = nLine;
	PH7_VmInitFuncState(&(*pVm),&pMeth->sFunc,&zName[sizeof(char)*4/*[__@*/+SyStringLength(&pClass->sName)],
		pName->nByte,iFuncFlags|VM_FUNC_CLASS_METHOD,pClass);
	return pMeth;
}
/*
 * Check if the given name have a class method associated with it.
 * Return the desired method [i.e: ph7_class_method instance] on success. NULL otherwise.
 */
PH7_PRIVATE ph7_class_method * PH7_ClassExtractMethod(ph7_class *pClass,const char *zName,sxu32 nByte)
{
	SyHashEntry *pEntry;
	/* Perform a hash lookup */
	pEntry = SyHashGet(&pClass->hMethod,(const void *)zName,nByte);
	if( pEntry == 0 ){
		/* No such entry */
		return 0;
	}
	/* Point to the desired method */
	return (ph7_class_method *)pEntry->pUserData;
}
/*
 * Check if the given name is a class attribute.
 * Return the desired attribute [i.e: ph7_class_attr instance] on success.NULL otherwise.
 */
PH7_PRIVATE ph7_class_attr * PH7_ClassExtractAttribute(ph7_class *pClass,const char *zName,sxu32 nByte)
{
	SyHashEntry *pEntry;
	/* Perform a hash lookup */
	pEntry = SyHashGet(&pClass->hAttr,(const void *)zName,nByte);
	if( pEntry == 0 ){
		/* No such entry */
		return 0;
	}
	/* Point to the desierd method */
	return (ph7_class_attr *)pEntry->pUserData;
}
/*
 * Install a class attribute in the corresponding container.
 * Return SXRET_OK on success. Any other return value indicates failure.
 */
PH7_PRIVATE sxi32 PH7_ClassInstallAttr(ph7_class *pClass,ph7_class_attr *pAttr)
{
	SyString *pName = &pAttr->sName;
	sxi32 rc;
	/* Remember where this attribute was originally declared so that later
	 * inheritance/trait copies still know the declaring class (needed for
	 * PHP-compatible error messages on typed properties). */
	if( pAttr->pDeclClass == 0 ){
		pAttr->pDeclClass = pClass;
	}
	rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);
	return rc;
}
/*
 * Install a class method in the corresponding container.
 * Return SXRET_OK on success. Any other return value indicates failure.
 */
PH7_PRIVATE sxi32 PH7_ClassInstallMethod(ph7_class *pClass,ph7_class_method *pMeth)
{
	SyString *pName = &pMeth->sFunc.sName;
	sxi32 rc;
	rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);
	return rc;
}
/*
 * Method-override compatibility (variance) checking.
 *
 * PHP rejects an override whose signature is incompatible with the parent's:
 * return types are covariant (child may only narrow), parameter types are
 * contravariant (child may only widen), and a child may not add a required
 * parameter. We add the diagnostic — but conservatively: PHL must keep running
 * valid PHP, so the comparator below is SKIP-BY-DEFAULT. It flags only cases that
 * are unambiguously invalid and silently accepts anything subtle (unions,
 * intersections, pseudo-types, self/parent/static, object, unresolved classes,
 * or a missing type), so it can never reject valid code.
 */
#define OVT_NONE   0  /* no declared type */
#define OVT_SCALAR 1  /* a concrete invariant scalar: int/float/string/bool/array */
#define OVT_CLASS  2  /* a real, already-loaded class/interface */
#define OVT_SKIP   3  /* union/intersection/pseudo/self/object/unresolved — never flag */

/*
 * Classify one declared type (nType + class name + union flag) for override
 * comparison. On OVT_CLASS, *ppClass receives the resolved class. Class names are
 * resolved by a direct, autoload-free hClass lookup: a miss (forward reference,
 * namespaced, or not-yet-loaded) yields OVT_SKIP, which the caller accepts.
 */
static int OoClassifyOverrideType(ph7_vm *pVm, sxu32 nType, const SyString *pClass,
	int bUnion, ph7_class **ppClass)
{
	*ppClass = 0;
	if( bUnion ){
		return OVT_SKIP; /* union/intersection — full lattice, skip */
	}
	if( nType == 0 ){
		return OVT_NONE; /* no declared type */
	}
	if( nType == SXU32_HIGH ){
		/* A class name OR a pseudo-type stored as a name atom. Skip every pseudo
		 * (incl. self/parent/static, which are context-relative). */
		static const struct { const char *z; sxu32 n; } aPseudo[] = {
			{"mixed",5}, {"never",5}, {"iterable",8}, {"callable",8}, {"true",4},
			{"false",5}, {"self",4}, {"parent",6}, {"static",6}
		};
		const char *z = pClass->zString;
		sxu32 n = pClass->nByte;
		SyHashEntry *pE;
		sxu32 i;
		for( i = 0; i < SX_ARRAYSIZE(aPseudo); i++ ){
			if( n == aPseudo[i].n && SyStrnmicmp(z,aPseudo[i].z,n) == 0 ){
				return OVT_SKIP;
			}
		}
		pE = SyHashGet(&pVm->hClass,(const void *)z,n);
		if( pE == 0 ){
			return OVT_SKIP; /* not loaded / forward ref / namespaced — accept */
		}
		*ppClass = (ph7_class *)pE->pUserData;
		return OVT_CLASS;
	}
	if( nType == MEMOBJ_STRING || nType == MEMOBJ_INT || nType == MEMOBJ_REAL
	 || nType == MEMOBJ_BOOL || nType == MEMOBJ_HASHMAP ){
		return OVT_SCALAR;
	}
	/* MEMOBJ_OBJ (object — subtypes against classes), MEMOBJ_VOID/NULL/RES,
	 * or anything unexpected: skip. */
	return OVT_SKIP;
}

/*
 * A declared type normalized for override comparison: the raw type code, the
 * class-name string (when a class), and the union/nullable flags. Extracted once
 * from each side so the comparator takes two of these instead of eight scalars.
 */
typedef struct OvType OvType;
struct OvType {
	sxu32 nType;
	const SyString *pClass;
	int bUnion;
	int bNullable;
};
static OvType OoTypeFromReturn(ph7_vm_func *pF)
{
	OvType t;
	t.nType = pF->nReturnType;
	t.pClass = &pF->sReturnClass;
	t.bUnion = SySetUsed(&pF->aReturnUnion) > 0;
	t.bNullable = (pF->iFlags & VM_FUNC_RETURN_NULLABLE) != 0;
	return t;
}
static OvType OoTypeFromArg(ph7_vm_func_arg *pA)
{
	OvType t;
	t.nType = pA->nType;
	t.pClass = &pA->sClass;
	t.bUnion = (pA->iFlags & VM_FUNC_ARG_UNION) != 0;
	t.bNullable = (pA->iFlags & VM_FUNC_ARG_NULLABLE) != 0;
	return t;
}
/*
 * Return TRUE if the child type is an unambiguously-invalid override of the
 * parent type. bCovariant=1 for a return type (child must be ⊆ parent),
 * 0 for a parameter (child must be ⊇ parent). Returns FALSE (accept) on any
 * skipped/ambiguous shape.
 */
static int OoOverrideTypeBad(ph7_vm *pVm, OvType parent, OvType child, int bCovariant)
{
	ph7_class *pParentCls, *pChildCls;
	int kP = OoClassifyOverrideType(pVm, parent.nType, parent.pClass, parent.bUnion, &pParentCls);
	int kC = OoClassifyOverrideType(pVm, child.nType, child.pClass, child.bUnion, &pChildCls);
	if( kP == OVT_SKIP || kC == OVT_SKIP ){
		return 0; /* ambiguous shape — conservatively accept */
	}
	/* A missing type is the TOP type. covariant (return): a concrete child is a
	 * subtype of top, fine; a top child over a concrete parent WIDENS → bad.
	 * contravariant (param): a top child is a supertype of anything, fine; a
	 * concrete child over a top parent NARROWS → bad. (A union/intersection child
	 * already fell into OVT_SKIP above, so a flagged child here is scalar/class.) */
	if( kP == OVT_NONE || kC == OVT_NONE ){
		if( bCovariant && kC == OVT_NONE && kP != OVT_NONE ) return 1;
		if( !bCovariant && kP == OVT_NONE && kC != OVT_NONE ) return 1;
		return 0;
	}
	/* Nullability: a covariant return may not ADD null; a contravariant param may
	 * not REMOVE null. */
	if( bCovariant ){
		if( child.bNullable && !parent.bNullable ) return 1;
	}else{
		if( parent.bNullable && !child.bNullable ) return 1;
	}
	if( kP == OVT_SCALAR && kC == OVT_SCALAR ){
		/* Scalars are invariant — they must match exactly. */
		return (parent.nType != child.nType) ? 1 : 0;
	}
	if( kP == OVT_CLASS && kC == OVT_CLASS ){
		if( bCovariant ){
			return PH7_VmInstanceOf(pChildCls, pParentCls) ? 0 : 1;  /* child ⊆ parent */
		}
		return PH7_VmInstanceOf(pParentCls, pChildCls) ? 0 : 1;      /* child ⊇ parent */
	}
	/* One scalar and one class — disjoint. */
	return 1;
}

/*
 * Check a child method's signature against the parent method it overrides.
 * Emits a PHP-style "Declaration of … must be compatible …" fatal on a clear
 * incompatibility. `__construct` is exempt (PHP does not apply variance to it).
 */
static sxi32 OoCheckOverrideCompat(ph7_gen_state *pGen, ph7_class *pBase, ph7_class *pSub,
	ph7_class_method *pParent, ph7_class_method *pChild)
{
	ph7_vm *pVm = pGen->pVm;
	ph7_vm_func *pPF = &pParent->sFunc;
	ph7_vm_func *pCF = &pChild->sFunc;
	SyString *pMName = &pCF->sName;
	ph7_vm_func_arg *aP, *aC;
	sxu32 nPArg, nCArg, k;
	int bBad = 0;
	if( pMName->nByte == sizeof("__construct")-1
	 && SyStrnmicmp(pMName->zString,"__construct",pMName->nByte) == 0 ){
		return SXRET_OK;
	}
	/* Return type — covariant. */
	bBad = OoOverrideTypeBad(pVm, OoTypeFromReturn(pPF), OoTypeFromReturn(pCF), /* bCovariant */ 1);
	/* Each overlapping parameter — contravariant. */
	nPArg = SySetUsed(&pPF->aArgs);
	nCArg = SySetUsed(&pCF->aArgs);
	aP = (ph7_vm_func_arg *)SySetBasePtr(&pPF->aArgs);
	aC = (ph7_vm_func_arg *)SySetBasePtr(&pCF->aArgs);
	for( k = 0; !bBad && k < nPArg && k < nCArg; k++ ){
		bBad = OoOverrideTypeBad(pVm, OoTypeFromArg(&aP[k]), OoTypeFromArg(&aC[k]), /* bCovariant */ 0);
	}
	/* Parameter arity: the child must declare at least the parent's parameters and
	 * may add only OPTIONAL ones — PHP rejects dropping any param (even an optional
	 * one) or adding a required one. Skip the rule if either signature is variadic
	 * (arity semantics differ). */
	if( !bBad ){
		int bVariadic = 0;
		for( k = 0; k < nPArg; k++ ){ if( aP[k].iFlags & VM_FUNC_ARG_VARIADIC ) bVariadic = 1; }
		for( k = 0; k < nCArg; k++ ){ if( aC[k].iFlags & VM_FUNC_ARG_VARIADIC ) bVariadic = 1; }
		if( !bVariadic ){
			if( nCArg < nPArg ){
				bBad = 1; /* dropped a parent parameter */
			}else{
				for( k = nPArg; k < nCArg; k++ ){
					if( SySetUsed(&aC[k].aByteCode) == 0 ){ bBad = 1; break; } /* new required */
				}
			}
		}
	}
	if( bBad ){
		sxi32 rc = PH7_GenCompileError(&(*pGen),E_ERROR,pChild->nLine,
			"Declaration of %z::%z() must be compatible with %z::%z()",
			&pSub->sName,pMName,&pBase->sName,&pParent->sFunc.sName);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	return SXRET_OK;
}
/*
 * Perform an inheritance operation.
 * According to the PHP language reference manual
 *  When you extend a class, the subclass inherits all of the public and protected methods
 *  from the parent class. Unless a class Overwrites those methods, they will retain their original
 *  functionality.
 *  This is useful for defining and abstracting functionality, and permits the implementation
 *  of additional functionality in similar objects without the need to reimplement all of the shared
 *  functionality.
 *  Example #1 Inheritance Example
 * <?php
 * class foo
 * {
 *   public function printItem($string)
 *   {
 *       echo 'Foo: ' . $string . PHP_EOL;
 *   }
 *
 *   public function printPHP()
 *   {
 *       echo 'PHP is great.' . PHP_EOL;
 *   }
 * }
 * class bar extends foo
 * {
 *   public function printItem($string)
 *   {
 *       echo 'Bar: ' . $string . PHP_EOL;
 *   }
 * }
 * $foo = new foo();
 * $bar = new bar();
 * $foo->printItem('baz'); // Output: 'Foo: baz'
 * $foo->printPHP();       // Output: 'PHP is great'
 * $bar->printItem('baz'); // Output: 'Bar: baz'
 * $bar->printPHP();       // Output: 'PHP is great'
 *
 * This function return SXRET_OK if the inheritance operation was successfully performed.
 * Any other return value indicates failure and the upper layer must generate an appropriate
 * error message.
 */
PH7_PRIVATE sxi32 PH7_ClassInherit(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase)
{
	ph7_class_method *pMeth;
	ph7_class_attr *pAttr;
	SyHashEntry *pEntry;
	SyString *pName;
	sxi32 rc;
	/* Install in the derived hashtable */
	rc = SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);
	if( rc != SXRET_OK ){
		return rc;
	}
	/* readonly class inheritance (PHP 8.2): a readonly class may only extend a
	 * readonly class, and a non-readonly class may not extend a readonly one. */
	if( (pBase->iFlags & PH7_CLASS_READONLY) != (pSub->iFlags & PH7_CLASS_READONLY) ){
		if( pBase->iFlags & PH7_CLASS_READONLY ){
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,
				"Non-readonly class %z cannot extend readonly class %z",
				&pSub->sName,&pBase->sName);
		}else{
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,
				"Readonly class %z cannot extend non-readonly class %z",
				&pSub->sName,&pBase->sName);
		}
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	/* Copy public/protected attributes from the base class */
	SyHashResetLoopCursor(&pBase->hAttr);
	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){
		/* Make sure the private attributes are not redeclared in the subclass */
		pAttr = (ph7_class_attr *)pEntry->pUserData;
		pName = &pAttr->sName;
		if( (pEntry = SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte)) != 0 ){
			if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT|PH7_CLASS_ATTR_FINAL))
				== (PH7_CLASS_ATTR_CONSTANT|PH7_CLASS_ATTR_FINAL) ){
				/* Cannot override a final class constant (PHP 8.1). Report the
				 * class that originally declared it (pDeclClass) rather than the
				 * immediate base, so a multi-level chain matches PHP. */
				ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pBase;
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_attr *)pEntry->pUserData)->nLine,
					"%z::%z cannot override final constant %z::%z",
					&pSub->sName,pName,&pOwner->sName,pName);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}
			if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE &&
				((ph7_class_attr *)pEntry->pUserData)->iProtection != PH7_CLASS_PROT_PUBLIC ){
					/* Cannot redeclare private attribute */
					PH7_GenCompileError(&(*pGen),E_WARNING,((ph7_class_attr *)pEntry->pUserData)->nLine,
						"Private attribute '%z::%z' redeclared inside child class '%z'",
						&pBase->sName,pName,&pSub->sName);

			}
			continue;
		}
		/* Install the attribute. php: a base class's private INSTANCE property
		 * lives on every child instance too (its own methods read/write it
		 * through $this on the child; the access check grants private access by
		 * DECLARING class, so child methods and outsiders still can't touch it).
		 * Private STATICS/CONSTANTS stay uncopied — base methods reach those
		 * through self:: against the declaring class directly. */
		if( pAttr->iProtection != PH7_CLASS_PROT_PRIVATE
		 || (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT)) == 0 ){
			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);
			if( rc != SXRET_OK ){
				return rc;
			}
		}
	}
	SyHashResetLoopCursor(&pBase->hMethod);
	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){
		/* Make sure the private/final methods are not redeclared in the subclass */
		pMeth = (ph7_class_method *)pEntry->pUserData;
		pName = &pMeth->sFunc.sName;
		if( (pEntry = SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte)) != 0 ){
			 if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){
				/* Cannot Overwrite final method */
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_method *)pEntry->pUserData)->nLine,
					"Cannot Overwrite final method '%z:%z' inside child class '%z'",
					&pBase->sName,pName,&pSub->sName);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}else{
				/* Check the override's signature is compatible with the parent's. */
				rc = OoCheckOverrideCompat(&(*pGen),pBase,pSub,pMeth,
					(ph7_class_method *)pEntry->pUserData);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}
			continue;
		}
		/* Install the method. php: a base class's private INSTANCE method is
		 * dispatchable on child instances too — an inherited public method
		 * calling $this->priv() must find it (the call-site visibility check
		 * binds by DECLARING class, sFunc.pUserData, so child code and
		 * outsiders still can't call it; a private ctor copied down also
		 * blocks `new Child` from outside like php). Private STATICS stay
		 * uncopied — base methods reach those through self:: against the
		 * declaring class directly. */
		if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE
		 || (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){
			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);
			if( rc != SXRET_OK ){
				return rc;
			}
		}
	}
	/* Mark as subclass */
	pSub->pBase = pBase;
	/* All done */
	return SXRET_OK;
}
/*
 * Apply a trait to a class: copy all methods and attributes from the trait
 * into the target class. Unlike inheritance, traits copy ALL members including
 * private ones. Members already defined in the class take precedence.
 */
PH7_PRIVATE sxi32 PH7_ClassUseTrait(ph7_gen_state *pGen,ph7_class *pClass,ph7_class *pTrait)
{
	ph7_class_method *pMeth;
	ph7_class_attr *pAttr;
	SyHashEntry *pEntry;
	SyString *pName;
	sxi32 rc;
	/* Detect cyclic trait composition (e.g. trait A { use B; } trait B { use A; }) */
	if( pTrait->iFlags & PH7_CLASS_TRAIT_VISITING ){
		rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,
			"Trait circular reference detected: %z is already being applied",&pTrait->sName);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	pTrait->iFlags |= PH7_CLASS_TRAIT_VISITING;
	rc = SXRET_OK;
	/* Copy attributes from the trait */
	SyHashResetLoopCursor(&pTrait->hAttr);
	while((pEntry = SyHashGetNextEntry(&pTrait->hAttr)) != 0 ){
		SyHashEntry *pExisting;
		pAttr = (ph7_class_attr *)pEntry->pUserData;
		pName = &pAttr->sName;
		pExisting = SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte);
		if( pExisting != 0 ){
			/* Attribute already exists. Check if it came from another trait
			 * and whether the definitions are compatible (same defaults).
			 */
			ph7_class **apUsedTraits;
			sxu32 nUsed,k;
			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);
			nUsed = SySetUsed(&pClass->aTrait);
			for(k = 0; k < nUsed; k++){
				ph7_class_attr *pOther;
				pOther = PH7_ClassExtractAttribute(apUsedTraits[k],pName->zString,pName->nByte);
				if( pOther ){
					/* Two traits define the same property — check if defaults differ */
					ph7_class_attr *pClassAttr = (ph7_class_attr *)pExisting->pUserData;
					if( SySetUsed(&pAttr->aByteCode) != SySetUsed(&pClassAttr->aByteCode) ||
						(SySetUsed(&pAttr->aByteCode) > 0 &&
						 SyMemcmp(SySetBasePtr(&pAttr->aByteCode),SySetBasePtr(&pClassAttr->aByteCode),
							SySetUsed(&pAttr->aByteCode) * SySetElemSize(&pAttr->aByteCode)) != 0) ){
						rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine,
							"%z and %z define the same property ($%z) in the composition of %z. "
							"However, the definition differs and is considered incompatible",
							&apUsedTraits[k]->sName,&pTrait->sName,pName,&pClass->sName);
						if( rc == SXERR_ABORT ){
							goto cleanup;
						}
					}
					break;
				}
			}
			continue;
		}
		rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);
		if( rc != SXRET_OK ){
			goto cleanup;
		}
	}
	/* Copy methods from the trait */
	SyHashResetLoopCursor(&pTrait->hMethod);
	while((pEntry = SyHashGetNextEntry(&pTrait->hMethod)) != 0 ){
		pMeth = (ph7_class_method *)pEntry->pUserData;
		pName = &pMeth->sFunc.sName;
		if( SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte) != 0 ){
			/* Method already exists in the class. Check if it came from another trait
			 * (unresolved conflict) vs being defined by the class itself.
			 */
			ph7_class **apUsedTraits;
			sxu32 nUsed,k;
			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);
			nUsed = SySetUsed(&pClass->aTrait);
			for(k = 0; k < nUsed; k++){
				if( PH7_ClassExtractMethod(apUsedTraits[k],pName->zString,pName->nByte) != 0 ){
					/* Two different traits define the same method with no resolution */
					rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,
						"Trait method %z::%z has not been applied as %z::%z, "
						"because of collision with %z::%z",
						&pTrait->sName,pName,
						&pClass->sName,pName,
						&apUsedTraits[k]->sName,pName);
					if( rc == SXERR_ABORT ){
						goto cleanup;
					}
					break;
				}
			}
			/* Class-defined method takes precedence */
			continue;
		}
		rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);
		if( rc != SXRET_OK ){
			goto cleanup;
		}
	}
	/* Record trait in the class */
	SySetPut(&pClass->aTrait,(const void *)&pTrait);
cleanup:
	/* Always clear visiting flag, even on error paths */
	pTrait->iFlags &= ~PH7_CLASS_TRAIT_VISITING;
	SXUNUSED(pGen);
	return rc;
}
/*
 * Inherit an object interface from another object interface.
 * According to the PHP language reference manual.
 *  Object interfaces allow you to create code which specifies which methods a class
 *  must implement, without having to define how these methods are handled.
 *  Interfaces are defined using the interface keyword, in the same way as a standard
 *  class, but without any of the methods having their contents defined.
 *  All methods declared in an interface must be public, this is the nature of an interface.
 *
 * This function return SXRET_OK if the interface inheritance operation was successfully performed.
 * Any other return value indicates failure and the upper layer must generate an appropriate
 * error message.
 */
PH7_PRIVATE sxi32 PH7_ClassInterfaceInherit(ph7_class *pSub,ph7_class *pBase)
{
	ph7_class_method *pMeth;
	ph7_class_attr *pAttr;
	SyHashEntry *pEntry;
	SyString *pName;
	sxi32 rc;
	/* Install in the derived hashtable */
	SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);
	SyHashResetLoopCursor(&pBase->hAttr);
	/* Copy constants */
	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){
		/* Make sure the constants are not redeclared in the subclass */
		pAttr = (ph7_class_attr *)pEntry->pUserData;
		pName = &pAttr->sName;
		if( SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte) == 0 ){
			/* Install the constant in the subclass */
			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);
			if( rc != SXRET_OK ){
				return rc;
			}
		}
	}
	SyHashResetLoopCursor(&pBase->hMethod);
	/* Copy methods signature */
	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){
		/* Make sure the method are not redeclared in the subclass */
		pMeth = (ph7_class_method *)pEntry->pUserData;
		pName = &pMeth->sFunc.sName;
		if( SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte) == 0 ){
			/* Install the method */
			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);
			if( rc != SXRET_OK ){
				return rc;
			}
		}
	}
	/* Mark as subclass */
	pSub->pBase = pBase;
	/* All done */
	return SXRET_OK;
}
/*
 * Implements an object interface in the given main class.
 * According to the PHP language reference manual.
 *  Object interfaces allow you to create code which specifies which methods a class
 *  must implement, without having to define how these methods are handled.
 *  Interfaces are defined using the interface keyword, in the same way as a standard
 *  class, but without any of the methods having their contents defined.
 *  All methods declared in an interface must be public, this is the nature of an interface.
 *
 * This function return SXRET_OK if the interface was successfully implemented.
 * Any other return value indicates failure and the upper layer must generate an appropriate
 * error message.
 */
PH7_PRIVATE sxi32 PH7_ClassImplement(ph7_class *pMain,ph7_class *pInterface)
{
	ph7_class_attr *pAttr;
	SyHashEntry *pEntry;
	SyString *pName;
	sxi32 rc;
	/* First off,copy all constants declared inside the interface */
	SyHashResetLoopCursor(&pInterface->hAttr);
	while((pEntry = SyHashGetNextEntry(&pInterface->hAttr)) != 0 ){
		/* Point to the constant declaration */
		pAttr = (ph7_class_attr *)pEntry->pUserData;
		pName = &pAttr->sName;
		/* Make sure the attribute is not redeclared in the main class */
		if( SyHashGet(&pMain->hAttr,pName->zString,pName->nByte) == 0 ){
			/* Install the attribute */
			rc = SyHashInsert(&pMain->hAttr,pName->zString,pName->nByte,pAttr);
			if( rc != SXRET_OK ){
				return rc;
			}
		}
	}
	/* Install in the interface container */
	SySetPut(&pMain->aInterface,(const void *)&pInterface);
	/* Install interface method stubs into the implementing class.
	 * Methods already defined in the class take precedence (they satisfy
	 * the interface contract). Stubs retain PH7_CLASS_ATTR_ABSTRACT so
	 * the unified check in GenStateCheckAbstractMethods catches missing ones.
	 */
	{
		ph7_class_method *pMeth;
		SyHashEntry *pMEntry;
		SyString *pMName;
		SyHashResetLoopCursor(&pInterface->hMethod);
		while((pMEntry = SyHashGetNextEntry(&pInterface->hMethod)) != 0 ){
			pMeth = (ph7_class_method *)pMEntry->pUserData;
			pMName = &pMeth->sFunc.sName;
			if( SyHashGet(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte) == 0 ){
				rc = SyHashInsert(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte,pMeth);
				if( rc != SXRET_OK ){
					return rc;
				}
			}
		}
	}
	return SXRET_OK;
}
/*
 * Create a class instance [i.e: Object in the PHP jargon] at run-time.
 * The following function is called when an object is created at run-time
 * typically when the PH7_OP_NEW/PH7_OP_CLONE instructions are executed.
 * Notes on object creation.
 *
 * According to PHP language reference manual.
 * To create an instance of a class, the new keyword must be used. An object will always
 * be created unless the object has a constructor defined that throws an exception on error.
 * Classes should be defined before instantiation (and in some cases this is a requirement).
 * If a string containing the name of a class is used with new, a new instance of that class
 * will be created. If the class is in a namespace, its fully qualified name must be used when
 * doing this.
 * Example #3 Creating an instance
 * <?php
 *  $instance = new SimpleClass();
 *   // This can also be done with a variable:
 * $className = 'Foo';
 * $instance = new $className(); // Foo()
 * ?>
 * In the class context, it is possible to create a new object by new self and new parent.
 * When assigning an already created instance of a class to a new variable, the new variable
 * will access the same instance as the object that was assigned. This behaviour is the same
 * when passing instances to a function. A copy of an already created object can be made by
 * cloning it.
 * Example #4 Object Assignment
 * <?php
 *  class SimpleClass(){
 *    public $var;
 *  };
 *  $instance = new SimpleClass();
 *  $assigned   =  $instance;
 *  $reference  =& $instance;
 *  $instance->var = '$assigned will have this value';
 *  $instance = null; // $instance and $reference become null
 *  var_dump($instance);
 *  var_dump($reference);
 *  var_dump($assigned);
 * ?>
 * The above example will output:
 * NULL
 * NULL
 * object(SimpleClass)#1 (1) {
 *  ["var"]=>
 *    string(30) "$assigned will have this value"
 * }
 * Example #5 Creating new objects
 * <?php
 * class Test
 * {
 *   static public function getNew()
 *   {
 *       return new static;
 *   }
 * }
 * class Child extends Test
 * {}
 * $obj1 = new Test();
 * $obj2 = new $obj1;
 * var_dump($obj1 !== $obj2);
 * $obj3 = Test::getNew();
 * var_dump($obj3 instanceof Test);
 * $obj4 = Child::getNew();
 * var_dump($obj4 instanceof Child);
 * ?>
 * The above example will output:
 * bool(true)
 * bool(true)
 * bool(true)
 * Note that Symisc Systems have introduced powerfull extension to
 * OO subsystem. For example a class attribute may have any complex
 * expression associated with it when declaring the attribute unlike
 * the standard PHP engine which would allow a single value.
 * Example:
 *  class myClass{
 *    public $var = 25<<1+foo()/bar();
 *  };
 * Refer to the official documentation for more information.
 */
static ph7_class_instance * NewClassInstance(ph7_vm *pVm,ph7_class *pClass)
{
	ph7_class_instance *pThis;
	/* Allocate a new instance */
	pThis = (ph7_class_instance *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_instance));
	if( pThis == 0 ){
		return 0;
	}
	/* Zero the structure */
	SyZero(pThis,sizeof(ph7_class_instance));
	/* Initialize fields */
	pThis->iRef = 1;
	pThis->pVm = pVm;
	pThis->pClass = pClass;
	/* Assign a fresh monotonic object handle id (clones get their own, like PHP). */
	pThis->nObjId = pVm->nNextObjId++;
	SyHashInit(&pThis->hAttr,&pVm->sAllocator,0,0);
	return pThis;
}
/*
 * Wrapper around the NewClassInstance() function defined above.
 * See the block comment above for more information.
 */
PH7_PRIVATE ph7_class_instance * PH7_NewClassInstance(ph7_vm *pVm,ph7_class *pClass)
{
	ph7_class_instance *pNew;
	sxi32 rc;
	pNew = NewClassInstance(&(*pVm),&(*pClass));
	if( pNew == 0 ){
		return 0;
	}
	/* Associate a private VM frame with this class instance */
	rc = PH7_VmCreateClassInstanceFrame(&(*pVm),pNew);
	if( rc != SXRET_OK ){
		SyMemBackendPoolFree(&pVm->sAllocator,pNew);
		return 0;
	}
	return pNew;
}
/*
 * Extract the value of a class instance [i.e: Object in the PHP jargon] attribute.
 * This function never fail.
 */
static ph7_value * ExtractClassAttrValue(ph7_vm *pVm,VmClassAttr *pAttr)
{
	/* Extract the value */
	ph7_value *pValue;
	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);
	return pValue;
}
/*
 * Perform a clone operation on a class instance [i.e: Object in the PHP jargon].
 * The following function is called when an object is cloned at run-time
 * typically when the PH7_OP_CLONE instruction is executed.
 * Notes on object cloning.
 *
 * According to PHP language reference manual.
 * Creating a copy of an object with fully replicated properties is not always the wanted behavior.
 * A good example of the need for copy constructors. Another example is if your object holds a reference
 * to another object which it uses and when you replicate the parent object you want to create
 * a new instance of this other object so that the replica has its own separate copy.
 * An object copy is created by using the clone keyword (which calls the object's __clone() method if possible).
 * An object's __clone() method cannot be called directly.
 * $copy_of_object = clone $object;
 * When an object is cloned, PHP 5 will perform a shallow copy of all of the object's properties.
 * Any properties that are references to other variables, will remain references.
 * Once the cloning is complete, if a __clone() method is defined, then the newly created object's __clone() method
 * will be called, to allow any necessary properties that need to be changed.
 * Example #1 Cloning an object
 * <?php
 * class SubObject
 * {
 *   static $instances = 0;
 *   public $instance;
 *
 *   public function __construct() {
 *       $this->instance = ++self::$instances;
 *   }
 *
 *   public function __clone() {
 *       $this->instance = ++self::$instances;
 *   }
 * }
 *
 * class MyCloneable
 * {
 *   public $object1;
 *   public $object2;
 *
 *   function __clone()
 *   {
 *       // Force a copy of this->object, otherwise
 *       // it will point to same object.
 *       $this->object1 = clone $this->object1;
 *   }
 * }
 * $obj = new MyCloneable();
 * $obj->object1 = new SubObject();
 * $obj->object2 = new SubObject();
 * $obj2 = clone $obj;
 * print("Original Object:\n");
 * print_r($obj);
 * print("Cloned Object:\n");
 * print_r($obj2);
 * ?>
 * The above example will output:
 * Original Object:
 * MyCloneable Object
 * (
 *   [object1] => SubObject Object
 *       (
 *           [instance] => 1
 *       )
 *
 *   [object2] => SubObject Object
 *       (
 *           [instance] => 2
 *       )
 *
 * )
 * Cloned Object:
 * MyCloneable Object
 * (
 *   [object1] => SubObject Object
 *       (
 *           [instance] => 3
 *       )
 *
 *   [object2] => SubObject Object
 *       (
 *           [instance] => 2
 *       )
 * )
 */
PH7_PRIVATE ph7_class_instance * PH7_CloneClassInstance(ph7_class_instance *pSrc)
{
	ph7_class_instance *pClone;
	ph7_class_method *pMethod;
	SyHashEntry *pEntry2;
	SyHashEntry *pEntry;
	ph7_vm *pVm;
	sxi32 rc;
	/* Allocate a new instance */
	pVm = pSrc->pVm;
	pClone = NewClassInstance(pVm,pSrc->pClass);
	if( pClone == 0 ){
		return 0;
	}
	/* Associate a private VM frame with this class instance */
	rc = PH7_VmCreateClassInstanceFrame(pVm,pClone);
	if( rc != SXRET_OK ){
		SyMemBackendPoolFree(&pVm->sAllocator,pClone);
		return 0;
	}
	/* Duplicate object values. Iterate the SOURCE attributes and copy each into
	 * the clone's same-named slot (looked up by name, so order/count differences
	 * from dynamic properties don't matter). A dynamic (runtime-added) property
	 * has no declared counterpart in the clone, so synthesize it first — without
	 * this, a clone of a stdClass would silently lose all its dynamic properties. */
	SyHashResetLoopCursor(&pSrc->hAttr);
	while((pEntry = SyHashGetNextEntry(&pSrc->hAttr)) != 0 ){
		VmClassAttr *pSrcAttr = (VmClassAttr *)pEntry->pUserData;
		VmClassAttr *pDestAttr = 0;
		ph7_value *pvSrc,*pvDest = 0;
		/* Duplicate non-static attribute */
		if( pSrcAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT) ){
			continue;
		}
		pEntry2 = SyHashGet(&pClone->hAttr,SyStringData(&pSrcAttr->pAttr->sName),SyStringLength(&pSrcAttr->pAttr->sName));
		if( pEntry2 ){
			pDestAttr = (VmClassAttr *)pEntry2->pUserData;
			pvDest = ExtractClassAttrValue(pVm,pDestAttr);
		}else if( pSrcAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){
			/* Dynamic property: synthesize the matching slot on the clone. */
			pvDest = PH7_VmCreateDynamicAttr(pVm,pClone,
				SyStringData(&pSrcAttr->pAttr->sName),SyStringLength(&pSrcAttr->pAttr->sName),&pDestAttr);
		}
		/* Fetch the source value LAST: PH7_VmCreateDynamicAttr above may have
		 * reserved a slot and reallocated pVm->aMemObj, which would dangle any
		 * ph7_value* obtained before it. pvDest from the synth path already points
		 * into the post-realloc aMemObj; resolve pvSrc now so both are current. */
		pvSrc = ExtractClassAttrValue(pVm,pSrcAttr);
		if( pvSrc && pvDest ){
			PH7_MemObjStore(pvSrc,pvDest);
		}
		/* Carry over the per-instance state so the clone matches the source:
		 * VM_CLASS_ATTR_UNINIT marks a typed property as not-yet-initialized
		 * and doubles as the readonly write-once latch — without this a clone
		 * would reset to uninitialized (losing the value's readiness) and a
		 * readonly property would become writable again. */
		if( pDestAttr ){
			pDestAttr->iState = pSrcAttr->iState;
		}
	}
	/* A declared property unset() on the source is absent from the clone too (PHP). But the clone
	 * frame above materialized ALL declared attrs (with their defaults), so drop any clone attr whose
	 * name is not present on the source. Collect first, then delete — removing an entry mid-walk would
	 * free the node the SyHash loop cursor points at. */
	{
		SySet sDrop;
		SySetInit(&sDrop,&pVm->sAllocator,sizeof(VmClassAttr *));
		SyHashResetLoopCursor(&pClone->hAttr);
		while((pEntry = SyHashGetNextEntry(&pClone->hAttr)) != 0 ){
			VmClassAttr *pCloneAttr = (VmClassAttr *)pEntry->pUserData;
			if( pCloneAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT) ){
				continue;
			}
			if( SyHashGet(&pSrc->hAttr,SyStringData(&pCloneAttr->pAttr->sName),
					SyStringLength(&pCloneAttr->pAttr->sName)) == 0 ){
				SySetPut(&sDrop,(const void *)&pCloneAttr);
			}
		}
		if( SySetUsed(&sDrop) > 0 ){
			VmClassAttr **apDrop = (VmClassAttr **)SySetBasePtr(&sDrop);
			sxu32 i;
			for( i = 0 ; i < SySetUsed(&sDrop) ; ++i ){
				VmClassAttr *pVmAttr = apDrop[i];
				SyHashDeleteEntry(&pClone->hAttr,SyStringData(&pVmAttr->pAttr->sName),
					SyStringLength(&pVmAttr->pAttr->sName),0);
				PH7_VmReleaseInstanceAttr(pVm,pVmAttr);
			}
		}
		SySetRelease(&sDrop);
	}
	/* call the __clone method on the cloned object if available */
	pMethod = PH7_ClassExtractMethod(pClone->pClass,"__clone",sizeof("__clone")-1);
	if( pMethod ){
		if( pMethod->iCloneDepth < 16 ){
			pMethod->iCloneDepth++;
			PH7_VmCallClassMethod(pVm,pClone,pMethod,0,0,0);
		}else{
			/* Nesting limit reached */
			PH7_VmThrowError(pVm,0,PH7_CTX_ERR,"Object clone limit reached,no more call to __clone()");
		}
		/* Reset the cursor */
		pMethod->iCloneDepth = 0;
	}
	/* Return the cloned object */
	return pClone;
}
#define CLASS_INSTANCE_DESTROYED 0x001 /* Instance is released */
/*
 * Free the per-instance allocations owned by ONE object attribute: its value slot (+ the typed-slot
 * enforcement entry), the synthesized ph7_class_attr for a dynamic (runtime-added) property, and the
 * VmClassAttr wrapper itself. Does NOT touch the hAttr entry node — the caller removes it
 * (`unset($o->p)` via SyHashDeleteEntry2; instance teardown via the wholesale SyHashRelease, so it must
 * not delete entries mid-walk). Shared by PH7_ClassInstanceRelease and the OP_MEMBER unset path.
 */
PH7_PRIVATE void PH7_VmReleaseInstanceAttr(ph7_vm *pVm, VmClassAttr *pVmAttr)
{
	if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT)) == 0 ){
		/* Drop any typed-property enforcement slot registered for this memobj, before the memobj
		 * is returned to the free list, so a future recycled slot does not inherit the stale entry. */
		if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){
			SyHashDeleteEntry(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),0);
		}
		PH7_VmUnsetMemObj(pVm,pVmAttr->nIdx,TRUE);
	}
	/* A dynamic property owns its synthesized ph7_class_attr (struct + inline name in one block) —
	 * free it here (the only place a per-instance pAttr is freed; declared attrs are class-owned). */
	if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){
		SyMemBackendFree(&pVm->sAllocator,pVmAttr->pAttr);
	}
	SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);
}
/*
 * Release a class instance [i.e: Object in the PHP jargon] and invoke any defined destructor.
 * This routine is invoked as soon as there are no other references to a particular
 * class instance.
 */
static void PH7_ClassInstanceRelease(ph7_class_instance *pThis)
{
	ph7_class_method *pDestr;
	SyHashEntry *pEntry;
	ph7_class *pClass;
	ph7_vm *pVm;
	if( pThis->iFlags & CLASS_INSTANCE_DESTROYED ){
		/*
		 * Already destroyed,return immediately.
		 * This could happend if someone perform unset($this) in the destructor body.
		 */
		return;
	}
	/* Mark as destroyed */
	pThis->iFlags |= CLASS_INSTANCE_DESTROYED;
	/* Invoke any defined destructor if available */
	pVm = pThis->pVm;
	pClass = pThis->pClass;
	pDestr = PH7_ClassExtractMethod(pClass,"__destruct",sizeof("__destruct")-1);
	if( pDestr && !pVm->bInReset ){
		/* Invoke the destructor. Skipped during ph7_vm_reset() bulk teardown:
		 * running user PHP against a half-reset VM is unsafe (see bInReset). */
		pThis->iRef = 2; /* Prevent garbage collection */
		PH7_VmCallClassMethod(pVm,pThis,pDestr,0,0,0);
	}
	/* Release non-static attributes (the wholesale SyHashRelease below frees the entry nodes,
	 * so the helper must not delete them mid-walk). */
	SyHashResetLoopCursor(&pThis->hAttr);
	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){
		PH7_VmReleaseInstanceAttr(pVm,(VmClassAttr *)pEntry->pUserData);
	}
	/* Release the whole structure */
	SyHashRelease(&pThis->hAttr);
	SyMemBackendPoolFree(&pVm->sAllocator,pThis);
}
/*
 * Decrement the reference count of a class instance [i.e Object in the PHP jargon].
 * If the reference count reaches zero,release the whole instance.
 */
PH7_PRIVATE void PH7_ClassInstanceUnref(ph7_class_instance *pThis)
{
	pThis->iRef--;
	if( pThis->iRef < 1 ){
		/* No more reference to this instance */
		PH7_ClassInstanceRelease(&(*pThis));
	}
}
/*
 * Compare two class instances [i.e: Objects in the PHP jargon]
 * Note on objects comparison:
 *  According to the PHP langauge reference manual
 *  When using the comparison operator (==), object variables are compared in a simple manner
 *  namely: Two object instances are equal if they have the same attributes and values, and are
 *  instances of the same class.
 *  On the other hand, when using the identity operator (===), object variables are identical
 *  if and only if they refer to the same instance of the same class.
 *  An example will clarify these rules.
 *  Example #1 Example of object comparison
 *  <?php
 *    function bool2str($bool)
 * {
 *   if ($bool === false) {
 *       return 'FALSE';
 *   } else {
 *       return 'TRUE';
 *   }
 * }
 * function compareObjects(&$o1, &$o2)
 * {
 *   echo 'o1 == o2 : ' . bool2str($o1 == $o2) . "\n";
 *   echo 'o1 != o2 : ' . bool2str($o1 != $o2) . "\n";
 *   echo 'o1 === o2 : ' . bool2str($o1 === $o2) . "\n";
 *   echo 'o1 !== o2 : ' . bool2str($o1 !== $o2) . "\n";
 * }
 * class Flag
 * {
 *   public $flag;
 *
 *   function Flag($flag = true) {
 *       $this->flag = $flag;
 *   }
 * }
 *
 * class OtherFlag
 * {
 *   public $flag;
 *
 *   function OtherFlag($flag = true) {
 *       $this->flag = $flag;
 *   }
 * }
 *
 * $o = new Flag();
 * $p = new Flag();
 * $q = $o;
 * $r = new OtherFlag();
 *
 * echo "Two instances of the same class\n";
 * compareObjects($o, $p);
 * echo "\nTwo references to the same instance\n";
 * compareObjects($o, $q);
 * echo "\nInstances of two different classes\n";
 * compareObjects($o, $r);
 * ?>
 * The above example will output:
 * Two instances of the same class
 * o1 == o2 : TRUE
 * o1 != o2 : FALSE
 * o1 === o2 : FALSE
 * o1 !== o2 : TRUE
 * Two references to the same instance
 * o1 == o2 : TRUE
 * o1 != o2 : FALSE
 * o1 === o2 : TRUE
 * o1 !== o2 : FALSE
 * Instances of two different classes
 * o1 == o2 : FALSE
 * o1 != o2 : TRUE
 * o1 === o2 : FALSE
 * o1 !== o2 : TRUE
 *
 * This function return 0 if the objects are equals according to the comprison rules defined above.
 * Any other return values indicates difference.
 */
PH7_PRIVATE sxi32 PH7_ClassInstanceCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest)
{
	SyHashEntry *pEntry,*pEntry2;
	ph7_value sV1,sV2;
	sxi32 rc;
	if( iNest > 31 ){
		/* Nesting limit reached */
		PH7_VmThrowError(pLeft->pVm,0,PH7_CTX_ERR,"Nesting limit reached: Infinite recursion?");
		return 1;
	}
	/* Comparison is performed only if the objects are instance of the same class */
	if( pLeft->pClass != pRight->pClass ){
		return 1;
	}
	if( bStrict ){
		/*
		 * According to the PHP language reference manual:
		 *  when using the identity operator (===), object variables
		 *  are identical if and only if they refer to the same instance
		 *  of the same class.
		 */
		return !(pLeft == pRight);
	}
	/*
	 * Attribute comparison.
	 * According to the PHP reference manual:
	 *  When using the comparison operator (==), object variables are compared
	 *  in a simple manner, namely: Two object instances are equal if they have
	 *  the same attributes and values, and are instances of the same class.
	 */
	if( pLeft == pRight ){
		/* Same instance,don't bother processing,object are equals */
		return 0;
	}
	/* Closures compare by IDENTITY under == as well (not by attributes): two distinct
	 * Closure instances are never equal, even when they wrap the same underlying function
	 * (PHP semantics). pLeft != pRight here, so a Closure pair is unequal. Without this,
	 * two capture-less lambdas of the same `function(){}` share the template's `$__fn`
	 * name and would compare equal. */
	if( pLeft->pVm->pClosureClass && pLeft->pClass == pLeft->pVm->pClosureClass ){
		return 1;
	}
	/* Same class but a different number of attributes ⇒ different property sets
	 * (dynamic properties can give two same-class instances different counts). */
	if( pLeft->hAttr.nEntry != pRight->hAttr.nEntry ){
		return 1;
	}
	PH7_MemObjInit(pLeft->pVm,&sV1);
	PH7_MemObjInit(pLeft->pVm,&sV2);
	sV1.nIdx = sV2.nIdx = SXU32_HIGH;
	/* Compare each left attribute against the RIGHT attribute of the SAME NAME
	 * (not in lockstep): dynamic properties may be stored in a different order
	 * on the two instances. Counts already match, so if every left attribute has
	 * an equal-valued same-named right attribute the property sets are equal. */
	SyHashResetLoopCursor(&pLeft->hAttr);
	while((pEntry = SyHashGetNextEntry(&pLeft->hAttr)) != 0 ){
		VmClassAttr *p1 = (VmClassAttr *)pEntry->pUserData;
		VmClassAttr *p2;
		ph7_value *pL,*pR;
		/* Compare only non-static attribute */
		if( p1->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT|PH7_CLASS_ATTR_STATIC) ){
			continue;
		}
		pEntry2 = SyHashGet(&pRight->hAttr,SyStringData(&p1->pAttr->sName),SyStringLength(&p1->pAttr->sName));
		if( pEntry2 == 0 ){
			/* Left has a property the right lacks ⇒ not equal. */
			return 1;
		}
		p2 = (VmClassAttr *)pEntry2->pUserData;
		pL = ExtractClassAttrValue(pLeft->pVm,p1);
		pR = ExtractClassAttrValue(pRight->pVm,p2);
		if( pL && pR ){
			PH7_MemObjLoad(pL,&sV1);
			PH7_MemObjLoad(pR,&sV2);
			/* Compare the two values now */
			rc = PH7_MemObjCmp(&sV1,&sV2,bStrict,iNest+1);
			PH7_MemObjRelease(&sV1);
			PH7_MemObjRelease(&sV2);
			if( rc != 0 ){
				/* Not equals */
				return rc;
			}
		}
	}
	/* Object are equals */
	return 0;
}
/*
 * Dump a class instance and the store the dump in the BLOB given
 * as the first argument.
 * Note that only non-static/non-constants attribute are dumped.
 * This function is typically invoked when the user issue a call
 * to [var_dump(),var_export(),print_r(),...].
 * This function SXRET_OK on success. Any other return value including
 * SXERR_LIMIT(infinite recursion) indicates failure.
 */
/*
 * Return the `name` property value of an enum case instance (the case name),
 * or 0 when unavailable. Shared by the var_dump/var_export/json/serialize
 * renderers, which all print enum cases as Class::CaseName forms.
 */
PH7_PRIVATE ph7_value * PH7_EnumCaseNameValue(ph7_class_instance *pThis)
{
	SyHashEntry *pEntry;
	if( (pThis->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){
		return 0;
	}
	pEntry = SyHashGet(&pThis->hAttr,(const void *)"name",sizeof("name")-1);
	if( pEntry == 0 ){
		return 0;
	}
	return PH7_ClassInstanceExtractAttrValue(pThis,(VmClassAttr *)pEntry->pUserData);
}
/*
 * Return the `value` property value (the backing value) of an enum case
 * instance, or 0 when unavailable (pure enums have none).
 */
PH7_PRIVATE ph7_value * PH7_EnumCaseBackingValueOf(ph7_class_instance *pThis)
{
	SyHashEntry *pEntry;
	if( (pThis->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){
		return 0;
	}
	pEntry = SyHashGet(&pThis->hAttr,(const void *)"value",sizeof("value")-1);
	if( pEntry == 0 ){
		return 0;
	}
	return PH7_ClassInstanceExtractAttrValue(pThis,(VmClassAttr *)pEntry->pUserData);
}
/*
 * Emit a class-instance dump header plus its trailing newline. For var_dump
 * (ShowType) it completes the "object(" prefix the caller already emitted as
 *   ClassName)#<id> (<count>) {
 * for print_r it emits the legacy PHL  Object(ClassName) {  (count/id unused).
 * Enum cases print php's `ClassName Enum {` print_r header (var_dump never
 * reaches here for enums — PH7_MemObjDump prints `enum(S::A)` directly).
 */
static void DumpClassInstanceHeader(SyBlob *pOut,ph7_class *pClass,sxu32 nObjId,int ShowType,sxu32 nCount)
{
	if( ShowType ){
		/* var_dump: `object(C)#id (n) {` */
		SyBlobFormat(&(*pOut),"object(%z)#%u (%u) {",&pClass->sName,nObjId,nCount);
		SyBlobAppend(&(*pOut),"\n",sizeof(char));
		return;
	}
	/* print_r: `C Object` / `E Enum[:backing]` — the '(' line is emitted by
	 * the body renderer at the container indent. */
	if( pClass->iFlags & PH7_CLASS_ENUM ){
		SyBlobFormat(&(*pOut),"%z Enum",&pClass->sName);
		if( pClass->nEnumBacking == MEMOBJ_INT ){
			SyBlobAppend(&(*pOut),":int",sizeof(":int")-1);
		}else if( pClass->nEnumBacking == MEMOBJ_STRING ){
			SyBlobAppend(&(*pOut),":string",sizeof(":string")-1);
		}
	}else{
		SyBlobFormat(&(*pOut),"%z Object",&pClass->sName);
	}
	SyBlobAppend(&(*pOut),"\n",sizeof(char));
}
/*
 * The class that DECLARED pAttr: inheritance shares attr pointers down the
 * chain, so the declaring class is the most ANCESTRAL class whose hAttr still
 * maps the name to this exact pointer. php's var_dump/print_r use it for the
 * `["p":"Decl":private]` annotation.
 */
static ph7_class * OoAttrDeclaringClass(ph7_class *pClass,ph7_class_attr *pAttr)
{
	/* Attrs record their declaring class at install time (inheritance/trait
	 * copies share the pointer, so the field survives the chain). */
	return pAttr->pDeclClass ? pAttr->pDeclClass : pClass;
}
/*
 * Emit a property's dump key: var_dump `["x"]=>` / `["p":"C":private]=>` /
 * `["q":protected]=>`; print_r `[x] => ` / `[p:C:private] => ` /
 * `[q:protected] => ` (php's exact annotations).
 */
static void OoDumpPropKey(SyBlob *pOut,ph7_class_instance *pThis,ph7_class_attr *pAttr,int ShowType)
{
	const char *zQ = ShowType ? "\"" : "";
	SyBlobFormat(&(*pOut),"[%s%z%s",zQ,&pAttr->sName,zQ);
	if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ){
		ph7_class *pDecl = OoAttrDeclaringClass(pThis->pClass,pAttr);
		SyBlobFormat(&(*pOut),":%s%z%s:private",zQ,&pDecl->sName,zQ);
	}else if( pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){
		SyBlobAppend(&(*pOut),":protected",sizeof(":protected")-1);
	}
	SyBlobAppend(&(*pOut),ShowType ? "]=>" : "] => ",ShowType ? 3 : 5);
}
PH7_PRIVATE sxi32 PH7_ClassInstanceDump(SyBlob *pOut,ph7_class_instance *pThis,int ShowType,int nTab,int nDepth)
{
	SyHashEntry *pEntry;
	ph7_value *pValue;
	sxi32 rc;
	int i;
	if( nDepth > 31 ){
		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";
		/* Nesting limit reached..halt immediately*/
		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);
		return SXERR_LIMIT;
	}
	rc = SXRET_OK;
	{
		/* Both var_dump and print_r consult __debugInfo() (PHP behavior);
		 * var_export uses a separate renderer and never reaches here. When the
		 * method is present and returns an array, render that array's entries as
		 * the object body, with the header showing the debug array's count. The
		 * nDepth guard above protects against a __debugInfo returning the object
		 * itself. */
		ph7_class_method *pDbg = PH7_ClassExtractMethod(pThis->pClass,"__debugInfo",sizeof("__debugInfo")-1);
		if( pDbg ){
			ph7_value sResult;
			PH7_MemObjInit(pThis->pVm,&sResult);
			PH7_VmCallClassMethod(pThis->pVm,pThis,pDbg,&sResult,0,0);
			if( sResult.iFlags & MEMOBJ_HASHMAP ){
				ph7_hashmap *pMap = (ph7_hashmap *)sResult.x.pOther;
				/* Header count is the debug array's entry count. */
				DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,pMap->nEntry);
				if( !ShowType ){
					for( i = 0 ; i < nTab ; i++ ){
						SyBlobAppend(&(*pOut)," ",sizeof(char));
					}
					SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);
				}
				rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth);
				for( i = 0 ; i < nTab ; i++ ){
					SyBlobAppend(&(*pOut)," ",sizeof(char));
				}
				if( ShowType ){
					SyBlobAppend(&(*pOut),"}",sizeof(char));
				}else{
					SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);
				}
				PH7_MemObjRelease(&sResult);
				return rc;
			}
			/* Non-array return: behave as if __debugInfo were absent. */
			PH7_MemObjRelease(&sResult);
		}
	}
	{
		/* var_dump's header needs the property count up front, so pre-count the
		 * non-static/non-constant attributes (matching the dump loop below). */
		sxu32 nProp = 0;
		if( ShowType ){
			SyHashResetLoopCursor(&pThis->hAttr);
			while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){
				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;
				if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT|PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_HOOK_VIRTUAL)) == 0 ){
					nProp++;
				}
			}
		}
		DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,nProp);
	}
	if( !ShowType ){
		/* print_r body opener: '(' at the container indent */
		for( i = 0 ; i < nTab ; i++ ){
			SyBlobAppend(&(*pOut)," ",sizeof(char));
		}
		SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);
	}
	/* Dump object attributes (php 8.4: VIRTUAL hooked properties have no
	 * backing store — excluded from var_dump/print_r) */
	SyHashResetLoopCursor(&pThis->hAttr);
	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){
		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;
		if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT|PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_HOOK_VIRTUAL)) == 0 ){
			/* Dump non-static/constant attribute only */
			pValue = ExtractClassAttrValue(pThis->pVm,pVmAttr);
			if( pValue == 0 ){
				continue;
			}
			if( ShowType ){
				/* var_dump prop: `["x"(:…)]=>` at nTab+2, the value on the next
				 * line at the same indent (php). */
				for( i = 0 ; i < nTab + 2 ; i++ ){
					SyBlobAppend(&(*pOut)," ",sizeof(char));
				}
				OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,TRUE);
				SyBlobAppend(&(*pOut),"\n",sizeof(char));
				rc = PH7_MemObjDump(&(*pOut),pValue,TRUE,nTab+2,nDepth,0);
				if( rc == SXERR_LIMIT ){
					break;
				}
			}else{
				/* print_r prop: `[x(:…)] => value` at nTab+4; container values
				 * render their block at nTab+8 followed by php's blank line. */
				for( i = 0 ; i < nTab + 4 ; i++ ){
					SyBlobAppend(&(*pOut)," ",sizeof(char));
				}
				OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,FALSE);
				if( (pValue->iFlags & (MEMOBJ_HASHMAP|MEMOBJ_OBJ))
				 && (pValue->iFlags & MEMOBJ_NULL) == 0 ){
					rc = PH7_MemObjDump(&(*pOut),pValue,FALSE,nTab+8,nDepth,0);
					SyBlobAppend(&(*pOut),"\n",sizeof(char));
					if( rc == SXERR_LIMIT ){
						break;
					}
				}else{
					PH7_MemObjPrintRInline(&(*pOut),pValue);
					SyBlobAppend(&(*pOut),"\n",sizeof(char));
				}
			}
		}
	}
	for( i = 0 ; i < nTab ; i++ ){
		SyBlobAppend(&(*pOut)," ",sizeof(char));
	}
	if( ShowType ){
		SyBlobAppend(&(*pOut),"}",sizeof(char));
	}else{
		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);
	}
	return rc;
}
/*
 * Call a magic method [i.e: __toString(),__toBool(),__Invoke()...]
 * Return SXRET_OK on successfull call. Any other return value indicates failure.
 * Notes on magic methods.
 * According to the PHP language reference manual.
 *  The function names __construct(), __destruct(), __call(), __callStatic()
 *  __get(),  __toString(), __invoke(), __clone() are magical in PHP classes.
 * You cannot have functions with these names in any of your classes unless
 * you want the magic functionality associated with them.
 * Example of magical methods:
 * __toString()
 *  The __toString() method allows a class to decide how it will react when it is treated like
 *  a string. For example, what echo $obj; will print. This method must return a string.
 *  Example #2 Simple example
 * <?php
 * // Declare a simple class
 * class TestClass
 * {
 *   public $foo;
 *
 *   public function __construct($foo)
 *   {
 *       $this->foo = $foo;
 *   }
 *
 *   public function __toString()
 *   {
 *       return $this->foo;
 *   }
 * }
 * $class = new TestClass('Hello');
 * echo $class;
 * ?>
 * The above example will output:
 *  Hello
 *
 * Note that PH7 does not support all the magical method and introudces __toFloat(),__toInt()
 * which have the same behaviour as __toString() but for float and integer types
 * respectively.
 * Refer to the official documentation for more information.
 */
PH7_PRIVATE sxi32 PH7_ClassInstanceCallMagicMethod(
	ph7_vm *pVm,               /* VM that own all this stuff */
	ph7_class *pClass,         /* Target class */
	ph7_class_instance *pThis, /* Target object */
	const char *zMethod,       /* Magic method name [i.e: __toString()]*/
	sxu32 nByte,               /* zMethod length*/
	const SyString *pAttrName, /* Attribute name */
	ph7_value *pResult         /* OUT: magic method return value. NULL to discard */
	)
{
	ph7_value *apArg[2] = { 0 , 0 };
	ph7_class_method *pMeth;
	ph7_value sAttr; /* cc warning */
	sxi32 rc;
	int nArg;
	/* Make sure the magic method is available */
	pMeth = PH7_ClassExtractMethod(&(*pClass),zMethod,nByte);
	if( pMeth == 0 ){
		/* No such method,return immediately */
		return SXERR_NOTFOUND;
	}
	nArg = 0;
	/* Copy arguments */
	if( pAttrName ){
		PH7_MemObjInitFromString(pVm,&sAttr,pAttrName);
		sAttr.nIdx = SXU32_HIGH; /* Mark as constant */
		apArg[0] = &sAttr;
		nArg = 1;
	}
	/* Call the magic method now */
	rc = PH7_VmCallClassMethod(pVm,&(*pThis),pMeth,pResult,nArg,apArg);
	/* Clean up */
	if( pAttrName ){
		PH7_MemObjRelease(&sAttr);
	}
	return rc;
}
/*
 * Extract the value of a class instance [i.e: Object in the PHP jargon].
 * This function is simply a wrapper on ExtractClassAttrValue().
 */
PH7_PRIVATE ph7_value * PH7_ClassInstanceExtractAttrValue(ph7_class_instance *pThis,VmClassAttr *pAttr)
{
   /* Extract the attribute value */
	ph7_value *pValue;
	pValue = ExtractClassAttrValue(pThis->pVm,pAttr);
	return pValue;
}
/*
 * Convert a class instance [i.e: Object in the PHP jargon] into a hashmap [i.e: array in the PHP jargon].
 * Return SXRET_OK on success. Any other value indicates failure.
 * Note on object conversion to array:
 *  Acccording to the PHP language reference manual
 *  If an object is converted to an array, the result is an array whose elements are the object's properties.
 *  The keys are the member variable names.
 *
 *  The following example:
 *  class Test {
 *   public $A = 25<<1;  // 50
 *	 public $c = rand_str(3);   // Random string of length 3
 *	 public $d = rand() & 1023; // Random number between 0..1023
 *  }
 *  var_dump((array) new Test());
 *	Will output:
 *  array(3) {
 *   [A] =>
 *      int(50)
 *   [c] =>
 *     string(3 'aps')
 *   [d] =>
 *     int(991)
 *  }
 * You have noticed that PH7 allow class attributes [i.e: $a,$c,$d in the example above]
 * have any complex expression (even function calls/Annonymous functions) as their default
 * value unlike the standard PHP engine.
 * This is a very powerful feature that you have to look at.
 */
PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap)
{
	SyHashEntry *pEntry;
	SyString *pAttrName;
	VmClassAttr *pAttr;
	ph7_value *pValue;
	ph7_value sName;
	/* Reset the loop cursor */
	SyHashResetLoopCursor(&pThis->hAttr);
	PH7_MemObjInitFromString(pThis->pVm,&sName,0);
	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){
		/* Point to the current attribute */
		pAttr = (VmClassAttr *)pEntry->pUserData;
		if( pAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){
			/* php 8.4: a VIRTUAL hooked property has no backing store — the
			 * (array) cast excludes it (raw surface, get is NOT dispatched) */
			continue;
		}
		/* Extract attribute value */
		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);
		if( pValue ){
			/* Build attribute name */
			pAttrName = &pAttr->pAttr->sName;
			PH7_MemObjStringAppend(&sName,pAttrName->zString,pAttrName->nByte);
			/* Perform the insertion */
			PH7_HashmapInsert(pMap,&sName,pValue);
			/* Reset the string cursor */
			SyBlobReset(&sName.sBlob);
		}
	}
	PH7_MemObjRelease(&sName);
	return SXRET_OK;
}
/*
 * Iterate throw class attributes and invoke the given callback [i.e: xWalk()] for each
 * retrieved attribute.
 * Note that argument are passed to the callback by copy. That is,any modification to
 * the attribute value in the callback body will not alter the real attribute value.
 * If the callback wishes to abort processing [i.e: it's invocation] it must return
 * a value different from PH7_OK.
 * Refer to [ph7_object_walk()] for more information.
 */
PH7_PRIVATE sxi32 PH7_ClassInstanceWalk(
	ph7_class_instance *pThis, /* Target object */
	int (*xWalk)(const char *,ph7_value *,void *), /* Walker callback */
	void *pUserData /* Last argument to xWalk() */
	)
{
	SyHashEntry *pEntry; /* Hash entry */
	VmClassAttr *pAttr;  /* Pointer to the attribute */
	ph7_value *pValue;   /* Attribute value */
	ph7_value sValue;    /* Copy of the attribute value */
	int rc;
	/* Reset the loop cursor */
	SyHashResetLoopCursor(&pThis->hAttr);
	PH7_MemObjInit(pThis->pVm,&sValue);
	/* Start the walk process */
	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){
		/* Point to the current attribute */
		pAttr = (VmClassAttr *)pEntry->pUserData;
		/* Extract attribute value */
		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);
		if( pValue ){
			PH7_MemObjLoad(pValue,&sValue);
			/* Invoke the supplied callback */
			rc =  xWalk(SyStringData(&pAttr->pAttr->sName),&sValue,pUserData);
			PH7_MemObjRelease(&sValue);
			if( rc != PH7_OK){
				/* User callback request an operation abort */
				return SXERR_ABORT;
			}
		}
	}
	/* All done */
	return SXRET_OK;
}
/*
 * Extract a class atrribute value.
 * Return a pointer to the attribute value on success. Otherwise NULL.
 * Note:
 *  Access to static and constant attribute is not allowed. That is,the function
 *  will return NULL in case someone (host-application code) try to extract
 *  a static/constant attribute.
 */
PH7_PRIVATE ph7_value * PH7_ClassInstanceFetchAttr(ph7_class_instance *pThis,const SyString *pName)
{
	SyHashEntry *pEntry;
	VmClassAttr *pAttr;
	/* Query the attribute hashtable */
	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);
	if( pEntry == 0 ){
		/* No such attribute */
		return 0;
	}
	/* Point to the class atrribute */
	pAttr = (VmClassAttr *)pEntry->pUserData;
	/* Check if we are dealing with a static/constant attribute */
	if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT|PH7_CLASS_ATTR_STATIC) ){
		/* Access is forbidden */
		return 0;
	}
	/* Return the attribute value */
	return ExtractClassAttrValue(pThis->pVm,pAttr);
}
