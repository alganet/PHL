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
	/* php method names are CASE-INSENSITIVE ($o->FOO() finds foo(), and declaring
	 * both is a redeclaration), so the method table matches on them the same way
	 * hClass does for class names. Properties and class constants ARE case
	 * sensitive in php, so hAttr keeps the default exact comparator. */
	SyHashInit(&pClass->hMethod,&pVm->sAllocator,SyStrHash,SyStrnmicmp);
	SyHashInit(&pClass->hAttr,&pVm->sAllocator,0,0);
	SyHashInit(&pClass->hConst,&pVm->sAllocator,0,0);
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
	/* Every method keeps the visibility it was DECLARED with, `__destruct`
	 * included. It used to be forced public here "because the engine invokes it
	 * internally" -- but the engine's teardown reaches it through
	 * PH7_VmCallClassMethod, which never consults the visibility, so the force
	 * bought nothing and cost the declaration: a private destructor reflected as
	 * public (isPrivate() false, modifiers 1), was listed by get_class_methods()
	 * from outside the class, printed `private` nowhere in its Reflection export,
	 * and could be called as `$o->__destruct()` from any scope. __construct has
	 * kept its declared visibility since band A #4, and php enforces that one at
	 * `new`; a method named like the class is a PLAIN method (PHP-4 constructors
	 * removed in 8.0) and keeps its own too. */
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
 * php's MANGLED storage name for one property, as an instance of pClass files it.
 *
 * php makes a split this engine did not: `ce->properties_info` is keyed by the
 * PLAIN name and is where every visibility decision is made, while the object's
 * own slot table is keyed by a MANGLED one -- "\0DeclaringClass\0name" for a
 * private property, the bare name for everything else. Two things follow from it,
 * and both were wrong here.
 *
 * `class A { private $q; } class B extends A { private $q; }` has TWO slots on one
 * object, each reachable only from its own declaring class, where PHL had one --
 * so a base method reading its own private got the CHILD's value, with nothing to
 * announce it. And a base's private is INVISIBLE from outside rather than merely
 * inaccessible: a lookup by the plain name finds nothing at all, which is why php
 * answers `$b->q` with "Undefined property: B::$q" and not with the visibility
 * refusal it words for `$a->q`.
 *
 * The declaring class is fixed per attribute, so the mangled name is built once
 * and cached on it. A property this class DECLARED keeps its plain name -- php
 * mangles the storage name there too, but nothing else in this engine ever sees
 * the difference, and the plain key is what every lookup that does not know about
 * a scope already asks for.
 */
PH7_PRIVATE const SyString * PH7_ClassAttrStorageName(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)
{
	ph7_class *pDecl;
	sxu32 nCls,nName;
	char *zKey;
	if( pAttr->iProtection != PH7_CLASS_PROT_PRIVATE
	 || (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT|PH7_CLASS_ATTR_DYNAMIC)) != 0 ){
		return &pAttr->sName;
	}
	pDecl = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);
	if( pDecl == 0 || pDecl == pClass ){
		return &pAttr->sName;
	}
	if( pDecl->iFlags & PH7_CLASS_INTERNAL ){
		/* An ENGINE class's slot keeps its plain name on every object below it:
		 * the C bodies that own that storage address it by name (a DateTime's
		 * timestamp, a PDOStatement's handle), and a subclass instance whose slots
		 * were renamed read as an object whose parent constructor never ran. php
		 * mangles an internal private too; nothing here can see the difference,
		 * because an engine class's private is not a name user code declares. */
		return &pAttr->sName;
	}
	if( SyStringLength(&pAttr->sStoreName) > 0 ){
		return &pAttr->sStoreName;
	}
	nCls = SyStringLength(&pDecl->sName);
	nName = SyStringLength(&pAttr->sName);
	/* Class-lifetime, like sName's own dup: an attribute outlives every instance
	 * whose table points at this key. */
	zKey = (char *)SyMemBackendAlloc(&pVm->sAllocator,nCls + nName + 3);
	if( zKey == 0 ){
		return &pAttr->sName;
	}
	zKey[0] = 0;
	SyMemcpy((const void *)SyStringData(&pDecl->sName),(void *)&zKey[1],nCls);
	zKey[1+nCls] = 0;
	SyMemcpy((const void *)SyStringData(&pAttr->sName),(void *)&zKey[nCls+2],nName);
	zKey[nCls+nName+2] = 0;
	SyStringInitFromBuf(&pAttr->sStoreName,zKey,nCls + nName + 2);
	return &pAttr->sStoreName;
}
/*
 * Which PROPERTY does `name` mean, seen from the class whose code is RUNNING?
 *
 * php's zend_get_parent_private_property: a scope that declares a private of this
 * name owns a slot of its own on every instance below it, and that slot -- not
 * whatever the object's class holds under the plain name -- is what its code
 * means. Answers 0 when the executing scope has no such private, which leaves the
 * caller on the ordinary plain-name path.
 */
static ph7_class_attr * OoScopePrivateAttr(ph7_vm *pVm,ph7_class *pClass,const char *zName,sxu32 nName)
{
	ph7_class *pScope;
	SyHashEntry *pEntry;
	ph7_class_attr *pOwn;
	if( nName < 1 ){
		return 0;
	}
	if( (pClass->iFlags & PH7_CLASS_SHADOW_PROP) == 0 ){
		/* No property of this class is filed under a mangled name, so it holds no
		 * slot the plain probe cannot reach. Reaching one at all takes a scope this
		 * class DESCENDS from, and inheriting that scope's private is exactly what
		 * sets the flag -- so this is the whole test, and every ordinary property
		 * access skips the frame walk below on it. */
		return 0;
	}
	pScope = PH7_VmCallerScope(&(*pVm));
	if( pScope == 0 || pScope == pClass ){
		return 0;   /* global scope, or the object's own class: the plain name IS the slot */
	}
	pEntry = SyHashGet(&pScope->hAttr,(const void *)zName,nName);
	pOwn = pEntry ? (ph7_class_attr *)pEntry->pUserData : 0;
	if( pOwn == 0
	 || pOwn->iProtection != PH7_CLASS_PROT_PRIVATE
	 || (pOwn->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT)) != 0
	 || PH7_VmMemberOwnerClass(pOwn->pDeclClass,pScope) != pScope
	 || !PH7_VmInstanceOf(pClass,pScope) ){
		return 0;
	}
	return pOwn;
}
/*
 * php presents an object's properties by their PLAIN names, so two slots that
 * unmangle to the same one -- a base's private and the subclass's own property --
 * collide on every surface that walks the object BY NAME: `foreach` and
 * get_object_vars(). php keeps the FIRST accessible one in storage order and drops
 * the rest, which is base-first, so a base method iterating a subclass instance
 * sees its OWN `$q` and never the child's. Only the RAW surfaces show both --
 * (array), serialize(), var_dump(), get_mangled_object_vars() -- and those key by
 * the mangled name, where nothing collides.
 *
 * TRUE when an EARLIER entry of this object's table carries the same plain name
 * and is itself accessible from here.
 */
PH7_PRIVATE int PH7_ClassInstanceAttrShadowed(ph7_vm *pVm,ph7_class_instance *pThis,SyHashEntry *pEntry)
{
	VmClassAttr *pMe = (VmClassAttr *)pEntry->pUserData;
	SyString *pName;
	SyHashEntry *pWalk;
	if( (pThis->pClass->iFlags & PH7_CLASS_SHADOW_PROP) == 0 || pMe == 0 ){
		return 0;   /* no mangled slot on this class: no name can collide */
	}
	pName = &pMe->pAttr->sName;
	for( pWalk = SyHashFirstEntry(&pThis->hAttr) ; pWalk && pWalk != pEntry ;
	     pWalk = SyHashEntryNext(pWalk) ){
		VmClassAttr *pOther = (VmClassAttr *)pWalk->pUserData;
		if( pOther == 0 || pOther->pAttr == pMe->pAttr ){
			continue;
		}
		if( SyStringLength(&pOther->pAttr->sName) != SyStringLength(pName)
		 || SyMemcmp((const void *)SyStringData(&pOther->pAttr->sName),
			(const void *)SyStringData(pName),SyStringLength(pName)) != 0 ){
			continue;
		}
		if( PH7_ClassInstanceAttrPresented(pOther)
		 && PH7_VmClassAttrAccess(&(*pVm),pThis->pClass,pOther->pAttr,FALSE) ){
			return 1;
		}
	}
	return 0;
}
/*
 * PH7_ClassExtractAttribute, told which scope is asking: the executing class's own
 * private wins over the same name declared further down the chain.
 */
PH7_PRIVATE ph7_class_attr * PH7_ClassScopedAttribute(ph7_vm *pVm,ph7_class *pClass,const char *zName,sxu32 nName)
{
	ph7_class_attr *pOwn = OoScopePrivateAttr(&(*pVm),pClass,zName,nName);
	if( pOwn ){
		return pOwn;
	}
	return PH7_ClassExtractAttribute(pClass,zName,nName);
}
/*
 * PH7_ClassInstanceAttrEntry, told which scope is asking. When the executing class
 * declares a private of this name, its MANGLED slot is the only one it can mean --
 * so a miss there is a miss, and never falls back to the plain name (php's fetch
 * stops at the property_info it resolved; an `unset()` of that slot reads as
 * undefined even when a public property of the same name sits beside it).
 */
PH7_PRIVATE SyHashEntry * PH7_ClassInstanceScopedAttrEntry(ph7_vm *pVm,ph7_class_instance *pThis,
	const char *zName,sxu32 nName)
{
	ph7_class_attr *pOwn = OoScopePrivateAttr(&(*pVm),pThis->pClass,zName,nName);
	if( pOwn ){
		const SyString *pKey = PH7_ClassAttrStorageName(&(*pVm),pThis->pClass,pOwn);
		return SyHashGet(&pThis->hAttr,(const void *)SyStringData(pKey),SyStringLength(pKey));
	}
	return PH7_ClassInstanceAttrEntry(pThis,zName,nName);
}
/*
 * Check if the given name is a class CONSTANT (or enum case).
 * php keeps constants and properties in separate namespaces, so constants live
 * in a dedicated table (hConst) and never collide with a same-named property.
 * Return the desired constant [ph7_class_attr with PH7_CLASS_ATTR_CONSTANT] on
 * success, NULL otherwise.
 */
PH7_PRIVATE ph7_class_attr * PH7_ClassExtractConstant(ph7_class *pClass,const char *zName,sxu32 nByte)
{
	SyHashEntry *pEntry;
	pEntry = SyHashGet(&pClass->hConst,(const void *)zName,nByte);
	if( pEntry == 0 ){
		return 0;
	}
	return (ph7_class_attr *)pEntry->pUserData;
}
/*
 * Install a class attribute in the corresponding container.
 * A constant (or enum case) goes to hConst, a property to hAttr — php's two
 * separate member namespaces, so `const C` and `public $C` coexist.
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
	if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){
		rc = SyHashInsertTail(&pClass->hConst,(const void *)pName->zString,pName->nByte,pAttr);
	}else{
		rc = SyHashInsertTail(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);
	}
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
 * ---------------------------------------------------------------------------
 * php's rendering of a USER function's DECLARATION.
 *
 * The text an incompatible-override fatal prints on either side of "must be
 * compatible with": `B::f(int $a, ?string $b = null): string`. It is php's
 * zend_get_function_declaration, and until this shipped both sides of that
 * sentence were bare names -- `B::f() must be compatible with A::f()` -- which
 * says the declarations disagree without saying how.
 *
 * A parameter is `[type ][&][...]$name[ = default]`; the return type follows as
 * `: T` and is omitted entirely when the declaration has none.
 * ---------------------------------------------------------------------------
 */
/* A character that belongs to a type NAME, as opposed to the punctuation that
 * separates the parts of a declared type (`?A`, `A|B`, `(A&B)|null`). */
static int OoDeclNameChar(int c)
{
	return !(c == '|' || c == '&' || c == '?' || c == '(' || c == ')'
		|| c == ' ' || c == '\t');
}
/*
 * A declared type as php prints it in a declaration.
 *
 * The stored text is already php's canonical order (`string|int` for both
 * spellings of it, `?A` for `A|null`, an intersection as written), so only the
 * names that are relative to WHERE the declaration was written move:
 *
 *   self / parent  resolved against the DECLARING class -- so the same trait
 *                  method reads `A $a` in one composing class and `B $a` in the
 *                  next, which is what php prints.
 *   static         left as written; php has no class to resolve it to at link
 *                  time either.
 *   iterable       expanded to the two types it stands for. Only the standalone
 *                  spellings reach here (a COMPOUND type stored it expanded
 *                  already), so `?iterable` is `Traversable|array|null` rather
 *                  than `?Traversable|array` -- the whole text, not a token.
 */
static void OoDeclType(ph7_class *pScope,const SyString *pDeclared,SyBlob *pOut)
{
	const char *z = pDeclared ? SyStringData(pDeclared) : 0;
	sxu32 n = z ? SyStringLength(pDeclared) : 0;
	sxu32 i = 0;
	if( n < 1 ){
		return;
	}
	if( n == 8 && SyStrnicmp(z,"iterable",8) == 0 ){
		SyBlobAppend(pOut,"Traversable|array",sizeof("Traversable|array")-1);
		return;
	}
	if( n == 9 && z[0] == '?' && SyStrnicmp(&z[1],"iterable",8) == 0 ){
		SyBlobAppend(pOut,"Traversable|array|null",sizeof("Traversable|array|null")-1);
		return;
	}
	while( i < n ){
		sxu32 nStart;
		const SyString *pWrite;
		SyString sTok;
		if( !OoDeclNameChar(z[i]) ){
			SyBlobAppend(pOut,&z[i],sizeof(char));
			i++;
			continue;
		}
		nStart = i;
		while( i < n && OoDeclNameChar(z[i]) ){
			i++;
		}
		SyStringInitFromBuf(&sTok,&z[nStart],i - nStart);
		pWrite = &sTok;
		if( pScope ){
			if( sTok.nByte == sizeof("self")-1
			 && SyStrnicmp(sTok.zString,"self",sizeof("self")-1) == 0 ){
				pWrite = &pScope->sName;
			}else if( sTok.nByte == sizeof("parent")-1
			 && SyStrnicmp(sTok.zString,"parent",sizeof("parent")-1) == 0
			 && pScope->pBase ){
				pWrite = &pScope->pBase->sName;
			}
		}
		SyBlobAppend(pOut,SyStringData(pWrite),SyStringLength(pWrite));
	}
}
/* The instructions of a compiled default, without the OP_DONE the compiler
 * terminates every one of them with. */
static sxu32 OoDeclDefLength(SySet *pByteCode)
{
	sxu32 n = SySetUsed(pByteCode);
	while( n > 0 ){
		VmInstr *pIn = (VmInstr *)SySetAt(pByteCode,n - 1);
		if( pIn == 0 || (pIn->iOp != PH7_OP_DONE && pIn->iOp != PH7_OP_NOOP) ){
			break;
		}
		n--;
	}
	return n;
}
/*
 * TRUE when every instruction of a compiled default is a LITERAL load or a pure
 * value operator -- the run php's constant folder would try to reduce. A LOADC
 * still carrying PH7_LOADC_EXPAND is a constant NAME, which php deliberately
 * does NOT fold (it prints the name); anything that reads a variable, calls
 * something or builds an object is not a constant expression at all.
 *
 * This is the screen PH7_VmEvalConstExpr's block comment requires. It is the
 * SySet twin of the instanceof folder's GenStateInstanceofFoldsLhs, which asks
 * the same question of the generator's live stream.
 */
static int OoDeclDefFoldable(SySet *pByteCode,sxu32 nLen)
{
	sxu32 n;
	for( n = 0 ; n < nLen ; ++n ){
		VmInstr *pIn = (VmInstr *)SySetAt(pByteCode,n);
		if( pIn == 0 ){
			return 0;
		}
		switch( pIn->iOp ){
		case PH7_OP_LOADC:
			if( pIn->iP1 & PH7_LOADC_EXPAND ){
				return 0; /* a constant NAME -- php keeps it unfolded */
			}
			break;
		case PH7_OP_LOAD_MAP: case PH7_OP_LOAD_IDX:
		case PH7_OP_CAT:
		case PH7_OP_CVT_INT: case PH7_OP_CVT_STR: case PH7_OP_CVT_REAL:
		case PH7_OP_CVT_BOOL: case PH7_OP_CVT_NUMC: case PH7_OP_CVT_NULL:
		case PH7_OP_UMINUS: case PH7_OP_UPLUS: case PH7_OP_BITNOT: case PH7_OP_LNOT:
		case PH7_OP_MUL: case PH7_OP_DIV: case PH7_OP_MOD: case PH7_OP_POW:
		case PH7_OP_ADD: case PH7_OP_SUB: case PH7_OP_SHL: case PH7_OP_SHR:
		case PH7_OP_LT: case PH7_OP_LE: case PH7_OP_GT: case PH7_OP_GE:
		case PH7_OP_SPACESHIP: case PH7_OP_EQ: case PH7_OP_NEQ:
		case PH7_OP_TEQ: case PH7_OP_TNE:
		case PH7_OP_BAND: case PH7_OP_BXOR: case PH7_OP_BOR:
		case PH7_OP_LAND: case PH7_OP_LOR: case PH7_OP_LXOR:
		case PH7_OP_JMP: case PH7_OP_JZ: case PH7_OP_JNZ:
		case PH7_OP_POP: case PH7_OP_DUP: case PH7_OP_NOOP:
			break;
		default:
			return 0;
		}
	}
	return nLen > 0;
}
/* The literal a LOADC pushes, or 0 when the operand is not a string one. */
static const SyString * OoDeclLiteral(ph7_vm *pVm,VmInstr *pIn,SyString *pOut)
{
	ph7_value *pLit;
	if( pIn == 0 || pIn->iOp != PH7_OP_LOADC ){
		return 0;
	}
	pLit = (ph7_value *)SySetAt(&pVm->aLitObj,(sxu32)pIn->iP2);
	if( pLit == 0 || (pLit->iFlags & MEMOBJ_STRING) == 0 ){
		return 0;
	}
	SyStringInitFromBuf(pOut,SyBlobData(&pLit->sBlob),SyBlobLength(&pLit->sBlob));
	return pOut;
}
/*
 * A FOLDED default value, spelled php's way.
 *
 * php's own spellings, and they are not the export's: a string is SINGLE-quoted,
 * printed RAW (no escaping at all) and TRUNCATED to ten bytes with `...` inside
 * the quotes; `null` is lower-case; an array shows only whether it is empty.
 */
static void OoDeclValue(SyBlob *pOut,ph7_value *pVal)
{
	if( pVal->iFlags & MEMOBJ_NULL ){
		SyBlobAppend(pOut,"null",sizeof("null")-1);
		return;
	}
	if( pVal->iFlags & MEMOBJ_BOOL ){
		SyBlobAppend(pOut,pVal->x.iVal ? "true" : "false",pVal->x.iVal ? 4 : 5);
		return;
	}
	if( pVal->iFlags & MEMOBJ_STRING ){
		sxu32 nStr = SyBlobLength(&pVal->sBlob);
		SyBlobAppend(pOut,"'",sizeof(char));
		if( nStr > 0 ){
			SyBlobAppend(pOut,SyBlobData(&pVal->sBlob),(nStr > 10 ? (sxu32)10 : nStr));
		}
		if( nStr > 10 ){
			SyBlobAppend(pOut,"...",sizeof("...")-1);
		}
		SyBlobAppend(pOut,"'",sizeof(char));
		return;
	}
	if( pVal->iFlags & MEMOBJ_HASHMAP ){
		ph7_hashmap *pMap = (ph7_hashmap *)pVal->x.pOther;
		SyBlobAppend(pOut,
			(pMap && pMap->nEntry > 0) ? "[...]" : "[]",
			(pMap && pMap->nEntry > 0) ? sizeof("[...]")-1 : sizeof("[]")-1);
		return;
	}
	if( pVal->iFlags & (MEMOBJ_INT|MEMOBJ_REAL) ){
		/* php prints the value's own string cast, which is where `1.0` reads `1`,
		 * `1e100` reads `1.0E+100` and INF reads `INF`. */
		PH7_MemObjToString(pVal);
		SyBlobAppend(pOut,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));
		return;
	}
	SyBlobAppend(pOut,"<expression>",sizeof("<expression>")-1);
}
/*
 * The text after `= ` in a parameter default.
 *
 * php prints what its compiler FOLDED the expression to, with two deliberate
 * exceptions it leaves unfolded and prints as source: a lone constant reference
 * keeps its NAME (`= M_PI`, `= PHP_INT_MAX`) and a class constant keeps
 * `Class::NAME` as written (`= self::K`, `= MyEnum::Foo`). `X::class` is not
 * one of those -- it folds to the class-name STRING, so it prints `'X'`.
 * Everything it could not reduce is php's `<expression>`.
 */
static void OoDeclDefault(ph7_vm *pVm,ph7_class *pScope,SySet *pByteCode,SyBlob *pOut)
{
	sxu32 nLen = OoDeclDefLength(pByteCode);
	SyString sOne, sTwo;
	if( nLen == 1 ){
		VmInstr *pIn = (VmInstr *)SySetAt(pByteCode,0);
		if( pIn && pIn->iOp == PH7_OP_LOADC && (pIn->iP1 & PH7_LOADC_EXPAND)
		 && OoDeclLiteral(pVm,pIn,&sOne) ){
			/* A constant NAME, exactly as the source wrote it. */
			SyBlobAppend(pOut,SyStringData(&sOne),SyStringLength(&sOne));
			return;
		}
	}
	if( nLen == 3 ){
		VmInstr *pCls = (VmInstr *)SySetAt(pByteCode,0);
		VmInstr *pMem = (VmInstr *)SySetAt(pByteCode,1);
		VmInstr *pOp  = (VmInstr *)SySetAt(pByteCode,2);
		if( pOp && pOp->iOp == PH7_OP_MEMBER && pOp->iP1 == 1
		 && pOp->iP2 == PH7_MEMBER_READ
		 && OoDeclLiteral(pVm,pCls,&sOne) && OoDeclLiteral(pVm,pMem,&sTwo) ){
			if( sTwo.nByte == sizeof("class")-1
			 && SyStrnicmp(sTwo.zString,"class",sizeof("class")-1) == 0 ){
				/* `self::class` -- the only ::class spelling the compiler leaves for
				 * the runtime (a named class folds to its own literal, and lands on
				 * the value path below). php folded it too, to the STRING. */
				SyBlob sName;
				ph7_class *pCurr = 0;
				if( pScope ){
					pCurr = (sOne.nByte == sizeof("parent")-1
						&& SyStrnicmp(sOne.zString,"parent",sizeof("parent")-1) == 0)
						? pScope->pBase : pScope;
				}
				if( pCurr ){
					SyBlobInit(&sName,&pVm->sAllocator);
					SyBlobAppend(&sName,"'",sizeof(char));
					SyBlobAppend(&sName,SyStringData(&pCurr->sName),SyStringLength(&pCurr->sName));
					SyBlobAppend(&sName,"'",sizeof(char));
					SyBlobAppend(pOut,SyBlobData(&sName),SyBlobLength(&sName));
					SyBlobRelease(&sName);
					return;
				}
			}else{
				SyBlobAppend(pOut,SyStringData(&sOne),SyStringLength(&sOne));
				SyBlobAppend(pOut,"::",sizeof("::")-1);
				SyBlobAppend(pOut,SyStringData(&sTwo),SyStringLength(&sTwo));
				return;
			}
		}
	}
	if( OoDeclDefFoldable(pByteCode,nLen) ){
		ph7_value sVal;
		int bFolded;
		PH7_MemObjInit(pVm,&sVal);
		bFolded = PH7_VmEvalConstExpr(pVm,pByteCode,&sVal);
		if( bFolded ){
			OoDeclValue(pOut,&sVal);
		}
		PH7_MemObjRelease(&sVal);
		if( bFolded ){
			return;
		}
	}
	SyBlobAppend(pOut,"<expression>",sizeof("<expression>")-1);
}
/*
 * php hands each rendered declaration to its error formatter as a C STRING, so a
 * declaration carrying a NUL byte -- `function f($a = "\0")` -- is cut there and
 * the sentence carries on with what follows it (`A::f($a = '` and then ` in ... on
 * line N`). Reproduced rather than left as a whole-blob write, which is the one
 * shape where the two engines would disagree byte for byte.
 */
static int OoDeclCLen(SyBlob *pDecl)
{
	const char *z = (const char *)SyBlobData(pDecl);
	sxu32 n = SyBlobLength(pDecl), i;
	for( i = 0 ; i < n ; ++i ){
		if( z[i] == 0 ){
			return (int)i;
		}
	}
	return (int)n;
}
/*
 * `(int $a, ?string $b = null): string` -- everything php prints after the
 * method's name. pScope is the class the declaration was written FOR (a trait
 * method's composing class, not the trait), which is what `self` and `parent`
 * resolve against.
 */
PH7_PRIVATE void PH7_ClassRenderDecl(ph7_vm *pVm,ph7_class *pScope,ph7_vm_func *pFunc,SyBlob *pOut)
{
	ph7_vm_func_arg *aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);
	sxu32 nArg = SySetUsed(&pFunc->aArgs);
	sxu32 i;
	SyBlobAppend(pOut,"(",sizeof(char));
	for( i = 0 ; i < nArg ; ++i ){
		if( i > 0 ){
			SyBlobAppend(pOut,", ",sizeof(", ")-1);
		}
		if( SyStringLength(&aArgs[i].sTypeName) > 0 ){
			OoDeclType(pScope,&aArgs[i].sTypeName,pOut);
			SyBlobAppend(pOut," ",sizeof(char));
		}
		if( aArgs[i].iFlags & VM_FUNC_ARG_BY_REF ){
			SyBlobAppend(pOut,"&",sizeof(char));
		}
		if( aArgs[i].iFlags & VM_FUNC_ARG_VARIADIC ){
			SyBlobAppend(pOut,"...",sizeof("...")-1);
		}
		SyBlobAppend(pOut,"$",sizeof(char));
		SyBlobAppend(pOut,SyStringData(&aArgs[i].sName),SyStringLength(&aArgs[i].sName));
		if( SySetUsed(&aArgs[i].aByteCode) > 0 ){
			SyBlobAppend(pOut," = ",sizeof(" = ")-1);
			OoDeclDefault(pVm,pScope,&aArgs[i].aByteCode,pOut);
		}
	}
	SyBlobAppend(pOut,")",sizeof(char));
	if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){
		SyBlobAppend(pOut,": ",sizeof(": ")-1);
		OoDeclType(pScope,&pFunc->sReturnTypeName,pOut);
	}
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
 * incompatibility.
 *
 * bCtorExempt tells the two regimes php has for `__construct` apart: an
 * INHERITED constructor is exempt from variance entirely (a child may declare
 * whatever it likes), while one an INTERFACE declares is checked like any other
 * method -- `interface I { __construct(int $a); }` really does constrain every
 * implementor's constructor.
 */
PH7_PRIVATE sxi32 PH7_ClassCheckOverrideCompat(ph7_gen_state *pGen, ph7_class *pBase, ph7_class *pSub,
	ph7_class_method *pParent, ph7_class_method *pChild, int bCtorExempt)
{
	ph7_vm *pVm = pGen->pVm;
	ph7_vm_func *pPF = &pParent->sFunc;
	ph7_vm_func *pCF = &pChild->sFunc;
	SyString *pMName = &pCF->sName;
	ph7_vm_func_arg *aP, *aC;
	sxu32 nPArg, nCArg, k;
	int bBad = 0;
	if( bCtorExempt
	 && pMName->nByte == sizeof("__construct")-1
	 && SyStrnmicmp(pMName->zString,"__construct",pMName->nByte) == 0 ){
		return SXRET_OK;
	}
	/*
	 * A NATIVE method declares its parameters in a zSig STRING, so its aArgs set
	 * is empty and there is nothing here to compare against. Reading that as
	 * "declares no parameters" made every override of one incompatible: a user
	 * class extending DOMDocument, a Reflection class or a native enum's own
	 * cases()/from() all fataled on a declaration php accepts. An
	 * engine-declared signature is compatible by construction, on either side.
	 */
	if( ((pPF->iFlags | pCF->iFlags) & VM_FUNC_NATIVE) != 0 ){
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
		/* php names the class that DECLARED each side, not the one the walk reached
		 * it through: `class A { f() } class B extends A {} class C extends B { f() }`
		 * is `C::f() must be compatible with A::f()`, and a trait method belongs to
		 * the class that composed it. That owner is also what `self` in either
		 * declaration resolves to. */
		ph7_class *pChildOwner = PH7_VmMemberOwnerClass((ph7_class *)pCF->pUserData,pSub);
		ph7_class *pParentOwner = PH7_VmMemberOwnerClass((ph7_class *)pPF->pUserData,pBase);
		SyBlob sChild, sParent;
		sxi32 rc;
		if( pChildOwner == 0 ){
			pChildOwner = pSub;
		}
		if( pParentOwner == 0 ){
			pParentOwner = pBase;
		}
		SyBlobInit(&sChild,&pVm->sAllocator);
		SyBlobInit(&sParent,&pVm->sAllocator);
		PH7_ClassRenderDecl(pVm,pChildOwner,pCF,&sChild);
		PH7_ClassRenderDecl(pVm,pParentOwner,pPF,&sParent);
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pChild->nLine,
			"Declaration of %z::%z%.*s must be compatible with %z::%z%.*s",
			&pChildOwner->sName,pMName,
			OoDeclCLen(&sChild),(const char *)SyBlobData(&sChild),
			&pParentOwner->sName,&pParent->sFunc.sName,
			OoDeclCLen(&sParent),(const char *)SyBlobData(&sParent));
		SyBlobRelease(&sChild);
		SyBlobRelease(&sParent);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	return SXRET_OK;
}
/*
 * Every method the sub-INTERFACE declares ITSELF, judged against the same name in
 * one parent. php checks a restated interface method exactly as it checks an
 * overriding class method, and words the refusal the same way -- PHL checked
 * neither, and instead refused the restatement outright when it came from a
 * parent past the first (see the collected-parents comment in the interface
 * compiler).
 *
 * Called while hMethod still holds only this interface's own declarations, which
 * is the one moment the two sets are separable.
 */
PH7_PRIVATE sxi32 PH7_ClassInterfaceCheckRedeclare(ph7_gen_state *pGen,ph7_class *pSub,
	ph7_class *pParent)
{
	SyHashEntry *pEntry;
	SyHashResetLoopCursor(&pSub->hMethod);
	while((pEntry = SyHashGetNextEntry(&pSub->hMethod)) != 0 ){
		ph7_class_method *pOwn = (ph7_class_method *)pEntry->pUserData;
		SyString *pName = &pOwn->sFunc.sName;
		SyHashEntry *pUp = SyHashGet(&pParent->hMethod,
			(const void *)pName->zString,pName->nByte);
		if( pUp && PH7_ClassCheckOverrideCompat(&(*pGen),pParent,pSub,
			(ph7_class_method *)pUp->pUserData,pOwn,0) == SXERR_ABORT ){
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
	SySet aInherited; /* base attributes to prepend (see the copy loop below) */
	sxi32 rc;
	SySetInit(&aInherited,&pGen->pVm->sAllocator,sizeof(ph7_class_attr *));
	/* Install in the derived hashtable */
	rc = SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);
	if( rc != SXRET_OK ){
		SySetRelease(&aInherited);
		return rc;
	}
	/* readonly class inheritance (PHP 8.2): a readonly class may only extend a
	 * readonly class, and a non-readonly class may not extend a readonly one.
	 * A FINAL base is not one of these cases at all -- it cannot be extended by
	 * anything, and php reports only that. Both diagnostics used to fire for a
	 * `final readonly` base and the readonly one was reported, which is the wrong
	 * reason; BcMath\Number is the engine's first such class. */
	if( (pBase->iFlags & PH7_CLASS_FINAL) == 0
	 && (pBase->iFlags & PH7_CLASS_READONLY) != (pSub->iFlags & PH7_CLASS_READONLY) ){
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
			SySetRelease(&aInherited);
			return SXERR_ABORT;
		}
	}
	/* Mark as subclass BEFORE the members are copied. php's mangled storage name
	 * for a TRAIT-composed private names the class that composed it, found by
	 * walking the subclass's ANCESTRY (PH7_VmMemberOwnerClass) -- with pBase still
	 * unset the walk stopped at the trait, cached that answer on the attribute,
	 * and every later lookup then asked for a key the object's table did not hold. */
	pSub->pBase = pBase;
	/* A native class whose php-visible properties are LAZY passes that on: the
	 * attributes copied below keep their flags, so a subclass of DateInterval has
	 * the same ten to install, and the O(1) gate in front of the materialization
	 * walk has to see it on the SUBCLASS or the constructor's writes land nowhere. */
	if( pBase->iFlags & PH7_CLASS_LAZY_ATTR ){
		pSub->iFlags |= PH7_CLASS_LAZY_ATTR;
	}
	/* Copy public/protected attributes from the base class */
	SyHashResetLoopCursor(&pBase->hAttr);
	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){
		/* Make sure the private attributes are not redeclared in the subclass */
		pAttr = (ph7_class_attr *)pEntry->pUserData;
		pName = &pAttr->sName;
		if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE
		 && (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT)) == 0 ){
			/* A base's private INSTANCE property is a slot of its own on every
			 * object below it, filed under php's mangled storage name -- so it can
			 * never collide with a subclass member of the same name, and the
			 * redeclaration rules below have nothing to say about it. The subclass
			 * keeps its own declaration exactly where it wrote it. */
			rc = SySetPut(&aInherited,(const void *)&pAttr);
			if( rc != SXRET_OK ){
				SySetRelease(&aInherited);
				return rc;
			}
			continue;
		}
		if( (pEntry = SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte)) != 0 ){
			if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT|PH7_CLASS_ATTR_FINAL))
				== (PH7_CLASS_ATTR_CONSTANT|PH7_CLASS_ATTR_FINAL) ){
				/* Cannot override a final class constant (PHP 8.1). Report the
				 * class that originally declared it (pDeclClass) rather than the
				 * immediate base, so a multi-level chain matches PHP -- and a
				 * TRAIT-declared one belongs to the class that composed it.
				 * php reports it on the SUBCLASS's declaration line, not on the
				 * line the offending member sits on: the refusal is inheritance
				 * talking, and inheritance happens where `extends` is written.
				 * (Its final-METHOD twin below is the other rule -- php reports
				 * THAT one at the method.) */
				ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pBase);
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,
					"%z::%z cannot override final constant %z::%z",
					&pSub->sName,pName,&pOwner->sName,pName);
				if( rc == SXERR_ABORT ){
					SySetRelease(&aInherited);
					return SXERR_ABORT;
				}
			}else if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT|PH7_CLASS_ATTR_FINAL))
				== PH7_CLASS_ATTR_FINAL ){
				/* PHP 8.4's final PROPERTY: no subclass may redeclare it, however the
				 * redeclaration is spelled -- a plain or static property of its own, a
				 * PROMOTED constructor parameter, or a trait it composes -- because all
				 * three land in the subclass's attribute table before inheritance runs.
				 * Same class-line rule and same declaring-class naming as the constant
				 * above. */
				ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pBase);
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,
					"Cannot override final property %z::$%z",&pOwner->sName,pName);
				if( rc == SXERR_ABORT ){
					SySetRelease(&aInherited);
					return SXERR_ABORT;
				}
			}
			/* A child MAY redeclare a base's private property: php treats the two
			 * as independent members (each private to its declaring class), with no
			 * diagnostic. PH7 warned here, which is wrong — the child's entry simply
			 * shadows the base's in the by-name attribute table.
			 *
			 * Ordering: php keeps an overridden INSTANCE property at the position
			 * the BASE declared it (`class G{$g1;$g2;} class H extends G{$h;$g1;}`
			 * iterates g1,g2,h — not g2,h,g1). Re-collect the CHILD's definition,
			 * whose default value wins, and drop its current entry so the prepend
			 * below re-inserts it in base order. Statics/constants are not part of
			 * instance iteration, so they keep their existing slot. */
			if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT)) == 0 ){
				ph7_class_attr *pOwn = (ph7_class_attr *)pEntry->pUserData;
				SyHashDeleteEntry(&pSub->hAttr,(const void *)pName->zString,pName->nByte,0);
				rc = SySetPut(&aInherited,(const void *)&pOwn);
				if( rc != SXRET_OK ){
					SySetRelease(&aInherited);
					return rc;
				}
			}
			continue;
		}
		/* Collect the attribute. A private STATIC comes down too: php keeps one
		 * in the child's property table -- `B::$s` on `class A { private static
		 * $s; }` is "Cannot access private property B::$s", the visibility
		 * refusal, and not the undeclared-static one -- and nothing else could
		 * find it, so `static::$s` from a base method with the subclass as its
		 * late-static-binding target reported its own static as undeclared. Its
		 * storage is the DECLARING class's slot either way (nIdx is shared), so
		 * this is a second name for one static, exactly as php has it.
		 *
		 * These are gathered rather than installed here because php orders an
		 * instance's properties BASE-DECLARED FIRST, then the subclass's own,
		 * then trait members — while inheritance runs AFTER the subclass body
		 * has already filled hAttr. They are prepended below. */
		rc = SySetPut(&aInherited,(const void *)&pAttr);
		if( rc != SXRET_OK ){
			SySetRelease(&aInherited);
			return rc;
		}
	}
	/* Prepend the collected base attributes so hAttr reads base-first. The
	 * table's iteration list is what every object-iteration consumer walks
	 * (var_dump/print_r, get_object_vars, foreach, (array) casts, json_encode),
	 * so this ordering is user-visible — json_encode emits its keys in exactly
	 * this order. SyHashInsert is a HEAD insert, so walking the collected set
	 * backwards leaves the base's own declaration order at the front. */
	if( SySetUsed(&aInherited) > 0 ){
		ph7_class_attr **apInherited = (ph7_class_attr **)SySetBasePtr(&aInherited);
		sxu32 n = SySetUsed(&aInherited);
		while( n > 0 ){
			ph7_class_attr *pIn = apInherited[--n];
			/* Under php's STORAGE name, which is the plain one for everything but
			 * an inherited private instance property. */
			const SyString *pKey = PH7_ClassAttrStorageName(pGen->pVm,pSub,pIn);
			if( pKey != &pIn->sName ){
				pSub->iFlags |= PH7_CLASS_SHADOW_PROP;
			}
			rc = SyHashInsert(&pSub->hAttr,(const void *)pKey->zString,pKey->nByte,pIn);
			if( rc != SXRET_OK ){
				SySetRelease(&aInherited);
				return rc;
			}
		}
	}
	/* Inherit the base's CONSTANTS (separate namespace: hConst). Constants are not
	 * part of instance iteration, so no base-first ordering dance is needed — a plain
	 * copy of every constant the subclass did not itself redeclare. A subclass that
	 * redeclares a base FINAL constant is a fatal (PHP 8.1). */
	SyHashResetLoopCursor(&pBase->hConst);
	while((pEntry = SyHashGetNextEntry(&pBase->hConst)) != 0 ){
		SyHashEntry *pOwn;
		pAttr = (ph7_class_attr *)pEntry->pUserData;
		pName = &pAttr->sName;
		if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ){
			/* A private CONSTANT is not inherited at all: php answers `B::K` with
			 * "Undefined constant B::K", never with the visibility refusal it words
			 * for `A::K`. Copying it down said "Cannot access private constant
			 * B::K" -- and let `static::K` from a base method find one php does
			 * not. A base method's own `self::K` resolves against A directly. */
			continue;
		}
		if( (pOwn = SyHashGet(&pSub->hConst,(const void *)pName->zString,pName->nByte)) != 0 ){
			if( (pAttr->iFlags & PH7_CLASS_ATTR_FINAL) ){
				/* Cannot override a final class constant. Report the class that
				 * originally declared it (pDeclClass) for a multi-level chain -- and a
				 * TRAIT-declared one belongs to the class that composed it. */
				ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pBase);
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,
					"%z::%z cannot override final constant %z::%z",
					&pSub->sName,pName,&pOwner->sName,pName);
				if( rc == SXERR_ABORT ){
					SySetRelease(&aInherited);
					return SXERR_ABORT;
				}
			}
			continue;
		}
		rc = SyHashInsertTail(&pSub->hConst,(const void *)pName->zString,pName->nByte,pAttr);
		if( rc != SXRET_OK ){
			SySetRelease(&aInherited);
			return rc;
		}
	}
	SySetRelease(&aInherited);
	SyHashResetLoopCursor(&pBase->hMethod);
	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){
		SyHashEntry *pOwn;
		SyString sKey;
		/* Make sure the private/final methods are not redeclared in the subclass.
		 * The identity inherited is the base's HASH KEY, not sFunc.sName — the same
		 * rule PH7_ClassUseTrait copies a trait by. A trait adaptation leaves the
		 * composed class holding entries whose key is the name the class ANSWERS to
		 * while the method struct keeps its original name: `B::m as mB` is the key
		 * `mB` over a struct still called `m`, and `A::m insteadof B` is the key `m`
		 * over A's struct. Keying the copy off sFunc.sName re-filed both under `m`,
		 * so a subclass of the composing class lost the alias entirely and took
		 * whichever of the two the hash walk reached last as its `m` — the insteadof
		 * choice, silently reversed. */
		pMeth = (ph7_class_method *)pEntry->pUserData;
		SyStringInitFromBuf(&sKey,(const char *)pEntry->pKey,pEntry->nKeyLen);
		pName = &sKey;
		if( (pOwn = SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte)) != 0 ){
			ph7_class_method *pOwnMeth = (ph7_class_method *)pOwn->pUserData;
			ph7_class *pOwnDecl = (ph7_class *)pOwnMeth->sFunc.pUserData;
			if( (pOwnMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0
			 && (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0
			 && pOwnDecl && (pOwnDecl->iFlags & PH7_CLASS_TRAIT) != 0 ){
				/* A trait's `abstract` is a REQUIREMENT, not a member, and php lets an
				 * INHERITED method satisfy it: `trait T { abstract function need(); }
				 * class P { function need(){} } class C extends P { use T; }` composes
				 * there and was "Class C contains 1 abstract method" here, because the
				 * trait is applied before the base is inherited and the requirement then
				 * shadowed the very method that answers it. The satisfying declaration
				 * still has to be COMPATIBLE with the requirement -- and php words that
				 * one the other way round, naming the class that PROVIDES the method and
				 * the trait that asked for it. */
				rc = PH7_ClassCheckOverrideCompat(&(*pGen),pOwnDecl,pBase,pOwnMeth,pMeth,1);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				pOwn->pUserData = (void *)pMeth;
				continue;
			}
			if( pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ){
				/* php: a base's PRIVATE method is never overridden — the child's
				 * declaration is an independent member of the same name, so neither
				 * the final rule nor the signature-compatibility rule applies to it
				 * (zend's do_inherit_method skips both for a private parent). PHL
				 * ran both: `class A { private function m($a){} } class B extends A
				 * { private function m($a,$b,$c){} }` was a fatal php compiles, and
				 * `final private` in the base fataled every child that reused the
				 * name — php only WARNS at the final-private DECLARATION and lets
				 * the child have the name. */
			}else if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){
				/* php: "Cannot override final method A::test()" */
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_method *)pOwn->pUserData)->nLine,
					"Cannot override final method %z::%z()",
					&pBase->sName,pName);
				(void)pSub;
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}else{
				/* Check the override's signature is compatible with the parent's. */
				rc = PH7_ClassCheckOverrideCompat(&(*pGen),pBase,pSub,pMeth,
					(ph7_class_method *)pOwn->pUserData,1);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}
			continue;
		}
		/* Install the method. php: a base class's private method is in the child's
		 * table too — an inherited public method calling $this->priv() must find it,
		 * and the LOOKUP has to find it for php's answer to `B::p()` to be
		 * "Call to private method A::p() from global scope" rather than
		 * "Call to undefined method B::p()". The call-site visibility check binds by
		 * DECLARING class (sFunc.pUserData), so child code and outsiders still cannot
		 * reach it; a private ctor copied down blocks `new Child` from outside like
		 * php's; and the surfaces that must NOT show an inherited private say so
		 * themselves (method_exists, get_class_methods, ReflectionClass::getMethods).
		 *
		 * STATIC privates used to be skipped here, on the reasoning that base methods
		 * reach them through self:: against the declaring class anyway. They do — but
		 * nothing else could: every spelling of `B::p()` (the call, `['B','p']()`,
		 * call_user_func, a first-class callable) reported the name as UNDEFINED, and
		 * `static::p()` from the base with a subclass as the late-static-binding
		 * target could not find its own method. */
		rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);
		if( rc != SXRET_OK ){
			return rc;
		}
	}
	/* All done */
	return SXRET_OK;
}
/*
 * Do these two compiled property defaults say the same thing? A raw memcmp of the
 * two instruction buffers is not that question: an instruction carries the LINE it
 * was compiled from and a literal travels as an INDEX into the VM's constant table,
 * so `public $p = 1` written in a trait and the same `public $p = 1` written in the
 * composing class compare as different bytes and made php's incompatible-property
 * fatal fire on a class php composes without a word. Compare what the instructions
 * MEAN instead: the opcode, its operands, and for a constant load the VALUE behind
 * the index.
 */
static int VmTraitLiteralSame(ph7_vm *pVm,sxu32 nLeft,sxu32 nRight)
{
	ph7_value *pLeft,*pRight;
	if( nLeft == nRight ){
		return 1;
	}
	pLeft  = (ph7_value *)SySetAt(&pVm->aLitObj,nLeft);
	pRight = (ph7_value *)SySetAt(&pVm->aLitObj,nRight);
	if( pLeft == 0 || pRight == 0 ){
		return 0;
	}
	if( (pLeft->iFlags & ~MEMOBJ_AUX) != (pRight->iFlags & ~MEMOBJ_AUX) ){
		return 0;
	}
	if( SyBlobLength(&pLeft->sBlob) != SyBlobLength(&pRight->sBlob)
	 || (SyBlobLength(&pLeft->sBlob) > 0
	     && SyMemcmp(SyBlobData(&pLeft->sBlob),SyBlobData(&pRight->sBlob),
	                 SyBlobLength(&pLeft->sBlob)) != 0) ){
		return 0;
	}
	if( (pLeft->iFlags & MEMOBJ_INT) && pLeft->x.iVal != pRight->x.iVal ){
		return 0;
	}
	if( (pLeft->iFlags & MEMOBJ_REAL) && pLeft->rVal != pRight->rVal ){
		return 0;
	}
	return 1;
}
static int VmTraitDefaultsMatch(ph7_vm *pVm,SySet *pLeft,SySet *pRight)
{
	VmInstr *aLeft,*aRight;
	sxu32 n,nUsed;
	nUsed = SySetUsed(pLeft);
	if( nUsed != SySetUsed(pRight) ){
		return 0;
	}
	if( nUsed < 1 ){
		return 1;
	}
	aLeft  = (VmInstr *)SySetBasePtr(pLeft);
	aRight = (VmInstr *)SySetBasePtr(pRight);
	for( n = 0 ; n < nUsed ; ++n ){
		if( aLeft[n].iOp != aRight[n].iOp || aLeft[n].iP1 != aRight[n].iP1 ){
			return 0;
		}
		if( aLeft[n].iOp == PH7_OP_LOADC ){
			if( !VmTraitLiteralSame(pVm,aLeft[n].iP2,aRight[n].iP2) ){
				return 0;
			}
			continue;
		}
		if( aLeft[n].iP2 != aRight[n].iP2 || aLeft[n].p3 != aRight[n].p3 ){
			return 0;
		}
	}
	return 1;
}
/*
 * Two constant declarations php considers the SAME declaration. Composing a trait over a
 * name that is already taken is only a conflict when the definition differs, and php's
 * notion of "differs" covers the whole declaration, not just the value: `final const K='x'`
 * against `const K='x'` conflicts, and so does `public const K` against `private const K`
 * and `const int K=1` against `const K=1`.
 */
static int VmTraitConstDefsMatch(ph7_vm *pVm,ph7_class_attr *pLeft,ph7_class_attr *pRight)
{
	sxi32 iMask = PH7_CLASS_ATTR_FINAL|PH7_CLASS_ATTR_TYPED|PH7_CLASS_ATTR_ABSTRACT;
	if( pLeft->iProtection != pRight->iProtection ){
		return 0;
	}
	if( (pLeft->iFlags & iMask) != (pRight->iFlags & iMask) ){
		return 0;
	}
	if( SyStringCmp(&pLeft->sTypeName,&pRight->sTypeName,SyMemcmp) != 0 ){
		return 0;
	}
	return VmTraitDefaultsMatch(pVm,&pLeft->aByteCode,&pRight->aByteCode);
}
/*
 * A private copy of a trait member's record for one composing class. php composes a trait
 * into each using class SEPARATELY, so a trait's STATIC property is one slot per class --
 * `trait T { public static $c = 0; } class A { use T; } class B { use T; }` gives A and B a
 * counter each -- and a trait CONSTANT is evaluated per class, so `const K = self::J` reads
 * the J of whichever class composed it. Copying the record by POINTER gave every using class
 * the same storage slot and the same memoized value.
 *
 * The copy shares its source's compiled byte-code and attribute sets, which are read-only
 * once compilation is past; what it does NOT share is nIdx, the storage slot, and the
 * per-evaluation flags. pDeclClass stays the TRAIT, so every scope and naming rule still
 * finds the composing class through it.
 */
static ph7_class_attr * VmCloneTraitAttr(ph7_vm *pVm,ph7_class_attr *pSrc)
{
	ph7_class_attr *pNew = (ph7_class_attr *)SyMemBackendPoolAlloc(&pVm->sAllocator,
		sizeof(ph7_class_attr));
	if( pNew == 0 ){
		return 0;
	}
	SyMemcpy((const void *)pSrc,(void *)pNew,sizeof(ph7_class_attr));
	pNew->nIdx = SXU32_HIGH; /* its own storage slot, reserved at this class's mount */
	SyZero(&pNew->sStoreName,sizeof(SyString)); /* ...and its own mangled name, which
	                          * names the class that COMPOSED it and not the source's */
	pNew->iFlags &= ~(PH7_CLASS_ATTR_EVALING|PH7_CLASS_ATTR_STATIC_DEFER);
	return pNew;
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
			/* The name is taken. What decides is the definition ALREADY standing --
			 * the class's own body just as much as an earlier trait's -- and whether
			 * its default is the same one. Looking the name up in the traits applied
			 * so far and comparing only THEN let a class-body property through:
			 * `class M { use TA, TB; public $p = 3; }` said nothing when TA arrived
			 * (no trait held the name yet) and then blamed the wrong pair when TB did. */
			ph7_class_attr *pClassAttr = (ph7_class_attr *)pExisting->pUserData;
			if( !VmTraitDefaultsMatch(pGen->pVm,&pAttr->aByteCode,&pClassAttr->aByteCode) ){
				/* php names the FIRST definition rather than the standing one: when the
				 * holder is the composing class itself, it walks the traits applied so
				 * far and names the first that declares the property, so the same class
				 * body reads "M and TA" with one trait behind it and "TA and TB" with
				 * two. The sentence ends with a clause of its own and lets the fatal's
				 * " in %s on line %u" finish it -- the line is the composing class's. */
				ph7_class *pHolder = pClassAttr->pDeclClass;
				if( pHolder == 0 || pHolder == pClass ){
					ph7_class **apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);
					sxu32 nUsed = SySetUsed(&pClass->aTrait);
					sxu32 k;
					pHolder = pClass;
					for(k = 0; k < nUsed; k++){
						if( PH7_ClassExtractAttribute(apUsedTraits[k],pName->zString,pName->nByte) ){
							pHolder = apUsedTraits[k];
							break;
						}
					}
				}
				rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,
					"%z and %z define the same property ($%z) in the composition of %z. "
					"However, the definition differs and is considered incompatible. "
					"Class was composed",
					&pHolder->sName,&pTrait->sName,pName,&pClass->sName);
				if( rc == SXERR_ABORT ){
					goto cleanup;
				}
			}
			continue;
		}
		if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){
			/* One slot per composing class (see VmCloneTraitAttr). */
			ph7_class_attr *pOwnCopy = VmCloneTraitAttr(pGen->pVm,pAttr);
			if( pOwnCopy == 0 ){
				rc = SXERR_MEM;
				goto cleanup;
			}
			pAttr = pOwnCopy;
		}
		rc = SyHashInsertTail(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);
		if( rc != SXRET_OK ){
			goto cleanup;
		}
	}
	/* Copy constants from the trait (PHP 8.2 trait constants; separate hConst
	 * namespace). The name being taken is only a conflict when the DEFINITION differs,
	 * exactly as for a property above -- php compares the value, the visibility, the
	 * `final` flag and the declared type, and lets two identical declarations through
	 * (`trait A { const K='x'; } trait B { const K='x'; }` composes fine). A definition
	 * inherited from a BASE class is not part of the comparison: a trait constant
	 * overrides one, silently, the way a class-body constant does. */
	SyHashResetLoopCursor(&pTrait->hConst);
	while((pEntry = SyHashGetNextEntry(&pTrait->hConst)) != 0 ){
		SyHashEntry *pExisting;
		pAttr = (ph7_class_attr *)pEntry->pUserData;
		pName = &pAttr->sName;
		pExisting = SyHashGet(&pClass->hConst,(const void *)pName->zString,pName->nByte);
		if( pExisting != 0 ){
			ph7_class_attr *pHave = (ph7_class_attr *)pExisting->pUserData;
			ph7_class *pHolder = pHave->pDeclClass;
			if( pHolder && pHolder != pClass
			 && (pHolder->iFlags & PH7_CLASS_TRAIT) == 0
			 && PH7_VmInstanceOf(pClass,pHolder) ){
				/* Inherited from a base class: the trait's definition replaces it. */
				ph7_class_attr *pOwnCopy = VmCloneTraitAttr(pGen->pVm,pAttr);
				if( pOwnCopy == 0 ){
					rc = SXERR_MEM;
					goto cleanup;
				}
				SyHashDeleteEntry2(pExisting);
				rc = SyHashInsertTail(&pClass->hConst,(const void *)pName->zString,pName->nByte,pOwnCopy);
				if( rc != SXRET_OK ){
					goto cleanup;
				}
				continue;
			}
			if( !VmTraitConstDefsMatch(pGen->pVm,pAttr,pHave) ){
				/* php names the FIRST definition, as the property path does. */
				if( pHolder == 0 || pHolder == pClass ){
					ph7_class **apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);
					sxu32 nUsed = SySetUsed(&pClass->aTrait);
					sxu32 k;
					pHolder = pClass;
					for(k = 0; k < nUsed; k++){
						if( PH7_ClassExtractConstant(apUsedTraits[k],pName->zString,pName->nByte) ){
							pHolder = apUsedTraits[k];
							break;
						}
					}
				}
				rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,
					"%z and %z define the same constant (%z) in the composition of %z. "
					"However, the definition differs and is considered incompatible. "
					"Class was composed",
					&pHolder->sName,&pTrait->sName,pName,&pClass->sName);
				if( rc == SXERR_ABORT ){
					goto cleanup;
				}
			}
			continue;
		}
		{
			/* Evaluated per composing class (`const K = self::J`), so one record each. */
			ph7_class_attr *pOwnCopy = VmCloneTraitAttr(pGen->pVm,pAttr);
			if( pOwnCopy == 0 ){
				rc = SXERR_MEM;
				goto cleanup;
			}
			rc = SyHashInsertTail(&pClass->hConst,(const void *)pName->zString,pName->nByte,pOwnCopy);
		}
		if( rc != SXRET_OK ){
			goto cleanup;
		}
	}
	/* Copy methods from the trait. The identity copied is the trait's HASH KEY,
	 * not sFunc.sName: an adaptation alias (`hi as bHi`) is a hash entry whose
	 * key is the alias while the method struct keeps its original name — keying
	 * the copy off sFunc.sName silently re-filed `bHi` under `hi`, so an alias
	 * made inside a TRAIT vanished when that trait was composed into a class. */
	SyHashResetLoopCursor(&pTrait->hMethod);
	while((pEntry = SyHashGetNextEntry(&pTrait->hMethod)) != 0 ){
		SyHashEntry *pClassMethEntry;
		SyString sKey;
		pMeth = (ph7_class_method *)pEntry->pUserData;
		SyStringInitFromBuf(&sKey,(const char *)pEntry->pKey,pEntry->nKeyLen);
		pName = &sKey;
		pClassMethEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);
		if( pClassMethEntry != 0 ){
			/* Method already exists in the class. An ABSTRACT trait method is a
			 * REQUIREMENT, not an implementation: php satisfies it with any concrete
			 * method of the same name (from the class body or another trait) — no
			 * collision. Only two CONCRETE trait methods actually conflict. */
			ph7_class_method *pExistingMeth = (ph7_class_method *)pClassMethEntry->pUserData;
			int bIncomingAbstract = (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0;
			int bExistingAbstract = (pExistingMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0;
			ph7_class **apUsedTraits;
			sxu32 nUsed,k;
			if( bIncomingAbstract ){
				/* Incoming abstract requirement: the existing (concrete or abstract)
				 * method already covers this name — keep it. */
				continue;
			}
			if( bExistingAbstract ){
				/* Existing entry is only an abstract requirement (from an earlier
				 * trait): the incoming concrete method satisfies and replaces it. */
				pClassMethEntry->pUserData = (void *)pMeth;
				continue;
			}
			/* Two names are not two METHODS. A trait that `use`s another trait
			 * flattens it by sharing the very ph7_class_method the origin trait
			 * compiled, so a method reaching the class down two composition paths
			 * arrives as the SAME struct both times -- which is php's own test
			 * (zend compares the two functions' op_array.opcodes) and why
			 * `trait TB { use TA; } class M { use TB, TA; }` composes there and
			 * fatalled here. Only two genuinely different definitions collide. */
			if( pExistingMeth == pMeth ){
				continue;
			}
			/* A method the class declares ITSELF wins over every trait, however many
			 * of them offer the name: php reports no collision at all for
			 * `class M { use TA, TB; public function m(){} }`, where PHL raised one
			 * as soon as the second trait arrived. */
			if( (ph7_class *)pExistingMeth->sFunc.pUserData == pClass ){
				continue;
			}
			/* Both concrete: a genuine collision only when the OTHER definition came
			 * from another trait (a concrete one). A class-body method wins silently. */
			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);
			nUsed = SySetUsed(&pClass->aTrait);
			for(k = 0; k < nUsed; k++){
				ph7_class_method *pOtherMeth = PH7_ClassExtractMethod(apUsedTraits[k],pName->zString,pName->nByte);
				if( pOtherMeth != 0 && pOtherMeth != pMeth
				 && (pOtherMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){
					/* Two different traits define the same CONCRETE method with no
					 * resolution. php reports the line of the COMPOSING class, not
					 * the one the losing definition was written on. */
					rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,
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
	SyHashResetLoopCursor(&pBase->hConst);
	/* Copy constants (interfaces carry only constants + method signatures) */
	while((pEntry = SyHashGetNextEntry(&pBase->hConst)) != 0 ){
		/* Make sure the constants are not redeclared in the subclass */
		pAttr = (ph7_class_attr *)pEntry->pUserData;
		pName = &pAttr->sName;
		if( SyHashGet(&pSub->hConst,(const void *)pName->zString,pName->nByte) == 0 ){
			/* Install the constant in the subclass */
			rc = SyHashInsertTail(&pSub->hConst,(const void *)pName->zString,pName->nByte,pAttr);
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
	/* First off,copy all constants declared inside the interface (hConst namespace) */
	SyHashResetLoopCursor(&pInterface->hConst);
	while((pEntry = SyHashGetNextEntry(&pInterface->hConst)) != 0 ){
		/* Point to the constant declaration */
		pAttr = (ph7_class_attr *)pEntry->pUserData;
		pName = &pAttr->sName;
		/* Make sure the constant is not redeclared in the main class */
		if( SyHashGet(&pMain->hConst,pName->zString,pName->nByte) == 0 ){
			/* Install the constant */
			rc = SyHashInsertTail(&pMain->hConst,pName->zString,pName->nByte,pAttr);
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
	/* php stamps a Throwable's file/line at CREATION, not in its constructor, so a
	 * subclass that overrides __construct without calling parent::__construct still
	 * reports the right site. Every instantiation path lands here. */
	PH7_VmStampThrowableSite(&(*pVm),pNew);
	return pNew;
}
/*
 * Open a private walk of this object's property table.
 *
 * Every consumer that hands PHP code the control flow between two attributes --
 * `foreach ($o as $k => $v)`, `array_walk($o, $fn)` -- must own its position
 * rather than share the SyHash's embedded cursor: php iterates each walk
 * independently (nested loops over one object do not rewind each other), and the
 * body it runs in between can add or remove a property. The instance keeps the
 * list of open walks so those two mutations can fix the cursors up; the walker
 * MUST close it on every exit path, or the next mutation walks a recycled slot.
 */
PH7_PRIVATE void PH7_ClassInstanceIterOpen(ph7_class_instance *pThis,PH7_AttrIter *pIter)
{
	pIter->pCursor = SyHashFirstEntry(&pThis->hAttr);
	pIter->pNextIter = pThis->pActiveIters;
	pThis->pActiveIters = pIter;
}
/*
 * The next attribute entry, or 0 when the walk is exhausted. The cursor is
 * advanced BEFORE the entry is handed out, exactly like SyHashGetNextEntry:
 * php's own iteration standing on an entry is free to unset() it.
 */
PH7_PRIVATE SyHashEntry * PH7_ClassInstanceIterNext(PH7_AttrIter *pIter)
{
	SyHashEntry *pEntry = pIter->pCursor;
	if( pEntry ){
		pIter->pCursor = SyHashEntryNext(pEntry);
	}
	return pEntry;
}
PH7_PRIVATE void PH7_ClassInstanceIterClose(ph7_class_instance *pThis,PH7_AttrIter *pIter)
{
	PH7_AttrIter **ppLink = &pThis->pActiveIters;
	while( *ppLink ){
		if( *ppLink == pIter ){
			*ppLink = pIter->pNextIter;
			pIter->pNextIter = 0;
			pIter->pCursor = 0;
			return;
		}
		ppLink = &(*ppLink)->pNextIter;
	}
}
/*
 * Remove one attribute entry from the instance, advancing any open walk parked
 * on it first. The ONLY door for an `unset($o->p)`-shaped removal: the entry is
 * freed here, so a walker still holding it would read a recycled pool slot on
 * its next step (php visits the properties AFTER the deleted one, and so does
 * this).
 */
PH7_PRIVATE void PH7_ClassInstanceDeleteAttrEntry(ph7_class_instance *pThis,SyHashEntry *pEntry)
{
	PH7_AttrIter *pIter;
	for( pIter = pThis->pActiveIters ; pIter ; pIter = pIter->pNextIter ){
		if( pIter->pCursor == pEntry ){
			pIter->pCursor = SyHashEntryNext(pEntry);
		}
	}
	SyHashDeleteEntry2(pEntry);
}
/*
 * The mirror: an attribute APPENDED to the table (a dynamic property created by
 * the loop body, a declared one re-created after unset()) re-arms any walk that
 * has run off the end -- php walks the LIVE table, so `foreach ($o as ...)` over
 * a stdClass whose body keeps adding properties keeps visiting them. A walker
 * with a NULL cursor is always mid-walk: it unregisters as soon as it stops.
 */
PH7_PRIVATE void PH7_ClassInstanceAttrAppended(ph7_class_instance *pThis,SyHashEntry *pEntry)
{
	PH7_AttrIter *pIter;
	if( pEntry == 0 ){
		return;
	}
	for( pIter = pThis->pActiveIters ; pIter ; pIter = pIter->pNextIter ){
		if( pIter->pCursor == 0 ){
			pIter->pCursor = pEntry;
		}
	}
}
/*
 * Spell ONE attribute the way php names it wherever an object's property table is
 * handed out as keys: a private property is "\0DeclaringClass\0name", a protected
 * one "\0*\0name", a public one its bare name. The NULs are real bytes (these
 * appends are length-based), which is what keeps two same-named members from
 * different visibility levels distinct. `pKey` must already be a STRING value; its
 * buffer is reset first, so one carrier serves a whole walk.
 */
PH7_PRIVATE void PH7_ClassInstanceAttrKey(ph7_class_instance *pThis,VmClassAttr *pAttr,ph7_value *pKey)
{
	SyString *pAttrName = &pAttr->pAttr->sName;
	SyBlobReset(&pKey->sBlob);
	if( pAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ){
		/* php mangles a private key with the class that OWNS the property, and a
		 * trait's members are owned by the class that composed them -- so the key,
		 * and every wire format built on it (serialize, the (array) cast), names
		 * the class and never the trait. */
		ph7_class *pDecl = PH7_VmMemberOwnerClass(pAttr->pAttr->pDeclClass,pThis->pClass);
		PH7_MemObjStringAppend(pKey,"\0",1);
		PH7_MemObjStringAppend(pKey,SyStringData(&pDecl->sName),SyStringLength(&pDecl->sName));
		PH7_MemObjStringAppend(pKey,"\0",1);
	}else if( pAttr->pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){
		PH7_MemObjStringAppend(pKey,"\0*\0",3);
	}
	PH7_MemObjStringAppend(pKey,pAttrName->zString,pAttrName->nByte);
}
/*
 * Is this slot part of the RAW property table php hands a walker -- the (array)
 * cast's slot walk, get_mangled_object_vars(), array_walk() over an object? A
 * class-level member is not the object's, a typed property never written is not
 * there yet, and a php 8.4 VIRTUAL hooked property has no backing store at all.
 */
PH7_PRIVATE int PH7_ClassInstanceAttrPresented(VmClassAttr *pAttr)
{
	return !PH7_ATTR_UNPRESENTED(pAttr)
		&& !PH7_ClassAttrUninitialized(pAttr)
		&& (pAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL) == 0;
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
/*
 * Is `clone` refused for this class? php's uncloneable internal classes refuse
 * for their USER SUBCLASSES too -- `class M extends IteratorIterator {}` makes
 * `clone $m` the same catchable Error, named after M -- because the refusal is
 * the inherited clone_obj handler, not the class's own row. So the flag is
 * consulted up the base chain, not on the instance's class alone. (A subclass
 * declaring its own __clone() changes nothing there either: php never reaches
 * it, and neither does this engine -- the refusal answers first.)
 */
PH7_PRIVATE int PH7_ClassIsUncloneable(ph7_class *pClass)
{
	ph7_class *pC;
	for( pC = pClass ; pC ; pC = pC->pBase ){
		if( pC->iFlags & PH7_CLASS_NOCLONE ){
			return 1;
		}
	}
	return 0;
}
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
	/* A clone of an object whose LAZY native properties are installed has them
	 * too: php clones the C struct the table is written from, so the copy shows
	 * what the original shows. The frame above skipped them (as it does at `new`),
	 * so install them before the value copy below looks for the same-named slots. */
	if( pSrc->iFlags & VM_INSTANCE_LAZY_DONE ){
		PH7_NativeMaterializeLazy(pVm,pClone);
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
		/* By the source's own KEY: a private property of a BASE class is filed under
		 * php's mangled storage name, and matching on the attribute's plain name
		 * would copy it over the same-named slot of the object's own class. */
		pEntry2 = SyHashGet(&pClone->hAttr,pEntry->pKey,pEntry->nKeyLen);
		if( pEntry2 ){
			pDestAttr = (VmClassAttr *)pEntry2->pUserData;
			pvDest = ExtractClassAttrValue(pVm,pDestAttr);
		}else if( pSrcAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){
			/* Dynamic property: synthesize the matching slot on the clone. */
			pvDest = PH7_VmCreateDynamicAttr(pVm,pClone,
				SyStringData(&pSrcAttr->pAttr->sName),SyStringLength(&pSrcAttr->pAttr->sName),&pDestAttr);
		}else if( pSrcAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_ONDEMAND ){
			/* An ON-DEMAND property is installed by the write that names it, so
			 * the clone's frame has no slot for one -- and php's copy carries it
			 * (a cloned from-string DateInterval keeps its `date_string`). */
			VmRecreateDeclaredAttr(pVm,pClone,pSrcAttr->pAttr,&pDestAttr);
			if( pDestAttr ){
				pvDest = ExtractClassAttrValue(pVm,pDestAttr);
			}
		}
		/* Fetch the source value LAST: PH7_VmCreateDynamicAttr above may have
		 * reserved a slot and reallocated pVm->aMemObj, which would dangle any
		 * ph7_value* obtained before it. pvDest from the synth path already points
		 * into the post-realloc aMemObj; resolve pvSrc now so both are current. */
		pvSrc = ExtractClassAttrValue(pVm,pSrcAttr);
		if( (pSrcAttr->iState & VM_CLASS_ATTR_REFBOUND) && pDestAttr ){
			/* php preserves references across clone: the clone shares the SAME slot
			 * as the source property (both alias the referenced variable), rather
			 * than getting an independent value copy. Drop the clone's fresh private
			 * slot and repoint at the (already-pinned) shared slot. The REFBOUND flag
			 * is carried over by the iState copy below, so the clone's release also
			 * leaves the shared slot alone. */
			if( pDestAttr->nIdx != pSrcAttr->nIdx ){
				PH7_VmStoreFilterDrop(pVm,pDestAttr->pAttr,pDestAttr->nIdx);
				PH7_VmUnsetMemObj(pVm,pDestAttr->nIdx,TRUE);
				pDestAttr->nIdx = pSrcAttr->nIdx;
				/* The clone is a holder of the shared slot in its own right — take a pin
				 * for it, since its own release will give one back. */
				VmPinMemObjSlotCounted(pVm,pDestAttr->nIdx);
			}
		}else if( pvSrc && pvDest ){
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
		SySetInit(&sDrop,&pVm->sAllocator,sizeof(SyHashEntry *));
		SyHashResetLoopCursor(&pClone->hAttr);
		while((pEntry = SyHashGetNextEntry(&pClone->hAttr)) != 0 ){
			VmClassAttr *pCloneAttr = (VmClassAttr *)pEntry->pUserData;
			if( pCloneAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT) ){
				continue;
			}
			if( SyHashGet(&pSrc->hAttr,pEntry->pKey,pEntry->nKeyLen) == 0 ){
				SySetPut(&sDrop,(const void *)&pEntry);
			}
		}
		if( SySetUsed(&sDrop) > 0 ){
			SyHashEntry **apDrop = (SyHashEntry **)SySetBasePtr(&sDrop);
			sxu32 i;
			for( i = 0 ; i < SySetUsed(&sDrop) ; ++i ){
				VmClassAttr *pVmAttr = (VmClassAttr *)apDrop[i]->pUserData;
				SyHashDeleteEntry(&pClone->hAttr,apDrop[i]->pKey,apDrop[i]->nKeyLen,0);
				PH7_VmReleaseInstanceAttr(pVm,pVmAttr);
			}
		}
		SySetRelease(&sDrop);
	}
	/* The native clone hook (php's clone_obj handler): what the copy MEANS for a
	 * class whose instances stand for engine-side state -- a DOM wrapper's copy
	 * is a copy of the node. The nearest ancestor's hook serves a user subclass,
	 * which is php's handler inheritance. Runs before any __clone(), as php's
	 * handler does. */
	{
		ph7_class *pHook;
		for( pHook = pClone->pClass ; pHook ; pHook = pHook->pBase ){
			if( pHook->xClone ){
				pHook->xClone(pVm,pClone,pSrc);
				break;
			}
		}
	}
	/* call the __clone method on the cloned object if available */
	pMethod = PH7_ClassExtractMethod(pClone->pClass,"__clone",sizeof("__clone")-1);
	if( pMethod ){
		if( pMethod->iCloneDepth < 16 ){
			pMethod->iCloneDepth++;
			/* PHP 8.3: __clone() may re-initialize the clone's readonly
			 * properties. Flag the instance so the readonly store guard allows
			 * it for the duration of the call. */
			pClone->iFlags |= VM_INSTANCE_CLONING;
			PH7_VmCallClassMethod(pVm,pClone,pMethod,0,0,0);
			pClone->iFlags &= ~VM_INSTANCE_CLONING;
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
	if( pVmAttr->iState & VM_CLASS_ATTR_REFBOUND ){
		/* Reference-bound property (`$o->p =& $x`): its value slot is SHARED, so it must
		 * not be released here — but the property WAS one of its holders, so give the pin
		 * back. The slot (and its value, which used to stay alive for the rest of the
		 * script) goes if the property was the last thing holding it. */
		VmUnpinMemObjSlot(pVm,pVmAttr->nIdx);
	}else if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT)) == 0 ){
		/* Drop any typed-property enforcement slot registered for this memobj, before the memobj
		 * is returned to the free list, so a future recycled slot does not inherit the stale entry. */
		PH7_VmStoreFilterDrop(pVm,pVmAttr->pAttr,pVmAttr->nIdx);
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
		/* php checks a non-public destructor's visibility HERE, against the scope
		 * the destruction happened in, and refuses with a sentence of its own: the
		 * engine reached for the method, so the message names the OBJECT's class
		 * and drops the word "method" the ordinary call refusal carries
		 * (`Call to private B::__destruct() from global scope` for a `class B
		 * extends A` whose base declared it). Screening here rather than letting
		 * the dispatcher speak is what keeps that wording; the call is then made
		 * unchecked, since this IS the check. */
		ph7_class *pDestrDecl = pDestr->sFunc.pUserData
			? (ph7_class *)pDestr->sFunc.pUserData : pClass;
		if( pDestr->iProtection != PH7_CLASS_PROT_PUBLIC
		 && !PH7_VmClassMemberAccess(&(*pVm),pDestrDecl,&pDestr->sFunc.sName,
			pDestr->iProtection,FALSE) ){
			SyBlob sErrMsg;
			const char *zVis = pDestr->iProtection == PH7_CLASS_PROT_PRIVATE
				? "private" : "protected";
			ph7_class *pScope = PH7_VmCallerScope(&(*pVm));
			SyBlobInit(&sErrMsg,&pVm->sAllocator);
			if( pScope ){
				SyBlobFormat(&sErrMsg,"Call to %s %z::__destruct() from scope %z",
					zVis,&pClass->sName,&pScope->sName);
			}else{
				SyBlobFormat(&sErrMsg,"Call to %s %z::__destruct() from global scope",
					zVis,&pClass->sName);
			}
			/* Parked, not returned: this release has no channel back to the
			 * executor (nothing "called" the destruct), and the dispatcher's own
			 * screen used to do the parking for us through
			 * VmCallClassMethodWithMap. Without it the uncaught Error is printed
			 * and the program carries on past a statement php never reaches. */
			VmBoundaryPark(&(*pVm),
				VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));
			SyBlobRelease(&sErrMsg);
		}else{
			/* Invoke the destructor. Skipped during ph7_vm_reset() bulk teardown:
			 * running user PHP against a half-reset VM is unsafe (see bInReset). */
			pThis->iRef = 2; /* Prevent garbage collection */
			PH7_VmCallMethodUnchecked(pVm,pThis,pDestr,0,0,0);
		}
	}
	/* A native class's own teardown, while its slots are still readable. Not a
	 * __destruct: the classes that need this (WeakReference) declare none in php,
	 * and Reflection must not grow one.
	 *
	 * Resolved through the ANCESTORS, like php's own free_obj handler: a
	 * subclass inherits it unless it declares one of its own. Reading it off
	 * this class alone left every subclass of a handle-owning native class
	 * without teardown -- `Pdo\Sqlite` (which is how PDO::connect() answers) and
	 * any userland `extends PDO` alike, both of which then died holding engine
	 * state that believed it was still reachable. */
	{
		ph7_class *pOwner = pClass;
		while( pOwner && pOwner->xRelease == 0 ){
			pOwner = pOwner->pBase;
		}
		if( pOwner && pOwner->xRelease ){
			pOwner->xRelease(pVm,pThis);
		}
	}
	/* Weak-reference registry: kill the cell for this instance so every
	 * WeakReference/WeakMap handle observes the death (the cell outlives the
	 * instance until its own handles drop; removing the hash entry here keeps
	 * a pool-reused address from resurrecting a dead cell). */
	if( SyHashTotalEntry(&pVm->hWeakCell) > 0 ){
		void *pCellData = 0;
		if( SyHashDeleteEntry(&pVm->hWeakCell,(const void *)&pThis,sizeof(void *),&pCellData) == SXRET_OK
		 && pCellData ){
			((VmWeakCell *)pCellData)->pObj = 0;
		}
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
	/*
	 * php's identity shortcut, and it comes FIRST -- before the same-class screen
	 * and before any handler: `$i == $i` is 0 for a DateInterval, the one pair of
	 * intervals php will compare at all.
	 */
	if( pLeft == pRight ){
		/* Same instance,don't bother processing,object are equals */
		return 0;
	}
	if( bStrict ){
		/*
		 * According to the PHP language reference manual:
		 *  when using the identity operator (===), object variables
		 *  are identical if and only if they refer to the same instance
		 *  of the same class.
		 * Two DISTINCT instances, so this is never identical -- and no compare
		 * handler is asked, because php's `===` is pointer identity and never
		 * reaches one.
		 */
		return 1;
	}
	/*
	 * php's compare handler (ph7_class::xCmp), asked of the LEFT operand and
	 * ABOVE the same-class screen: a DateTime and a DateTimeImmutable of the same
	 * instant are equal there, which no property walk between two different
	 * classes could ever answer. A class with no handler falls through to the
	 * walk, which is php's zend_std_compare_objects.
	 */
	if( PH7_ClassNativeCmp(pLeft,pRight,&rc) ){
		return rc;
	}
	/* Comparison is performed only if the objects are instance of the same class */
	if( pLeft->pClass != pRight->pClass ){
		return 1;
	}
	/*
	 * Attribute comparison.
	 * According to the PHP reference manual:
	 *  When using the comparison operator (==), object variables are compared
	 *  in a simple manner, namely: Two object instances are equal if they have
	 *  the same attributes and values, and are instances of the same class.
	 */
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
		/* Compare only non-static attribute. A native class's VIRTUAL property is
		 * skipped too: php fabricates DatePeriod's seven on demand and its real
		 * property table is empty, so any two DatePeriods are equal there whatever
		 * they contain -- while a subclass's own property, which IS in the table,
		 * still decides. */
		if( p1->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT|PH7_CLASS_ATTR_STATIC
		                        |PH7_CLASS_ATTR_NATIVE_VIRTUAL) ){
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
	 * copies share the pointer, so the field survives the chain) -- and a TRAIT's
	 * members belong to the class that composed them, which is what php names. */
	return PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);
}
/*
 * php's zend_unmangle_property_name_ex: a property key carries its own
 * visibility when it is MANGLED — "\0*\0name" is protected and "\0Class\0name"
 * is private to Class, which is how the (array) cast, __debugInfo() and every
 * get_debug_info handler say what a plain array key cannot. A key that does not
 * begin with a NUL is a public name and comes back unchanged.
 *
 * Answers 0 for a key that begins with a NUL and is NOT a well-formed mangled
 * name — php's "Illegal member variable name" (nothing after the NUL, or an
 * empty class part) and "Corrupt member variable name" (no second NUL, or
 * nothing after it). php renders those raw, notice aside, and so does the caller.
 */
PH7_PRIVATE int PH7_UnmangleAttrName(const char *zKey,sxu32 nKey,SyString *pClass,SyString *pName)
{
	sxu32 nCls,nSrc;
	SyStringInitFromBuf(pClass,0,0);
	SyStringInitFromBuf(pName,zKey,nKey);
	if( nKey < 1 || zKey[0] != 0 ){
		return 1;   /* a plain public name */
	}
	if( nKey < 3 || zKey[1] == 0 ){
		return 0;   /* php: "Illegal member variable name" */
	}
	nCls = 0;
	while( nCls < nKey - 2 && zKey[1+nCls] != 0 ){
		nCls++;
	}
	if( nCls >= nKey - 2 ){
		return 0;   /* php: "Corrupt member variable name" */
	}
	/* An ANONYMOUS class mangles its source location in as a SECOND NUL-separated
	 * part, so the property name is what follows the LAST NUL rather than the
	 * second one (php's anonclass_src_len step). The class STRING php shows is
	 * still only the first part — it prints that one as a C string. */
	nSrc = 0;
	while( nCls + 2 + nSrc < nKey && zKey[nCls+2+nSrc] != 0 ){
		nSrc++;
	}
	SyStringInitFromBuf(pClass,&zKey[1],nCls);
	if( nCls + nSrc + 2 != nKey ){
		nCls += nSrc + 1;
	}
	SyStringInitFromBuf(pName,&zKey[nCls+2],nKey - nCls - 2);
	return 1;
}
/*
 * Emit a property's dump key: var_dump `["x"]=>` / `["p":"C":private]=>` /
 * `["q":protected]=>`; print_r `[x] => ` / `[p:C:private] => ` /
 * `[q:protected] => ` (php's exact annotations).
 */
static void OoDumpPropKey(SyBlob *pOut,ph7_class_instance *pThis,ph7_class_attr *pAttr,int ShowType)
{
	const char *zQ = ShowType ? "\"" : "";
	if( SyStringLength(&pAttr->sName) > 0 && SyStringData(&pAttr->sName)[0] == 0 ){
		/* A MANGLED name stored raw — the __PHP_Incomplete_Class carrier's
		 * private/protected payload keys. php's dump unmangles them exactly as
		 * it does an (array) cast's: `["bp":"PB":private]` / `["pp":protected]`. */
		SyString sUnmCls, sUnmName;
		if( PH7_UnmangleAttrName(SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),
			&sUnmCls,&sUnmName) ){
			SyBlobFormat(&(*pOut),"[%s%z%s",zQ,&sUnmName,zQ);
			if( sUnmCls.nByte == 1 && sUnmCls.zString[0] == '*' ){
				SyBlobAppend(&(*pOut),":protected",sizeof(":protected")-1);
			}else{
				SyBlobFormat(&(*pOut),":%s%z%s:private",zQ,&sUnmCls,zQ);
			}
			SyBlobAppend(&(*pOut),ShowType ? "]=>" : "] => ",ShowType ? 3 : 5);
			return;
		}
	}
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
		/* A native class's PRESENTATION (php's get_debug_info): the shape it shows
		 * is not its storage — a DateTime shows date/timezone_type/timezone, a
		 * WeakReference shows ["object"] — and the slots underneath are hidden.
		 * Rendered exactly like a __debugInfo() array, which is what php does with
		 * it, and consulted first because php's handler wins over a userland
		 * method a native class cannot declare anyway. */
		ph7_value sPresent;
		PH7_MemObjInit(pThis->pVm,&sPresent);
		if( ph7_value_is_array(&sPresent) == 0 ){
			ph7_hashmap *pPresent = PH7_NewHashmap(pThis->pVm,0,0);
			if( pPresent ){
				sPresent.x.pOther = pPresent;
				MemObjSetType(&sPresent,MEMOBJ_HASHMAP);
			}
		}
		if( (sPresent.iFlags & MEMOBJ_HASHMAP)
		 && PH7_ClassInstancePresent(pThis,&sPresent,1) ){
			ph7_hashmap *pMap = (ph7_hashmap *)sPresent.x.pOther;
			DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,pMap->nEntry);
			if( !ShowType ){
				for( i = 0 ; i < nTab ; i++ ){
					SyBlobAppend(&(*pOut)," ",sizeof(char));
				}
				SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);
			}
			rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth,1);
			for( i = 0 ; i < nTab ; i++ ){
				SyBlobAppend(&(*pOut)," ",sizeof(char));
			}
			if( ShowType ){
				SyBlobAppend(&(*pOut),"}",sizeof(char));
			}else{
				SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);
			}
			PH7_MemObjRelease(&sPresent);
			return rc;
		}
		PH7_MemObjRelease(&sPresent);
	}
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
			PH7_VmCallMagicMethod(pThis->pVm,pThis,pDbg,&sResult,0,0);
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
				rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth,1);
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
		 * non-static/non-constant attributes (matching the dump loop below).
		 * An UNINITIALIZED typed property is still LISTED by var_dump — with php's
		 * `uninitialized(T)` marker in place of a value — but it does not COUNT,
		 * which is how php's `object(C)#1 (0) { ["a"]=> uninitialized(int) }`
		 * reads. */
		sxu32 nProp = 0;
		if( ShowType ){
			SyHashResetLoopCursor(&pThis->hAttr);
			while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){
				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;
				if( !PH7_ATTR_UNPRESENTED(pVmAttr)
				 && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL) == 0
				 && !PH7_ClassAttrUninitialized(pVmAttr) ){
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
		if( !PH7_ATTR_UNPRESENTED(pVmAttr)
		 && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL) == 0 ){
			if( PH7_ClassAttrUninitialized(pVmAttr) ){
				/* var_dump names the property and prints php's marker in place of
				 * the value it has not got; print_r has no such marker and leaves
				 * the property out entirely. */
				if( ShowType ){
					char zType[192];
					const char *zText = VmHintTextResolved(pThis->pVm,&pVmAttr->pAttr->sTypeName,
						VmHintScopeClass(pThis->pVm,pVmAttr->pAttr->pDeclClass,pVmAttr->pOwner),
						zType,sizeof(zType));
					for( i = 0 ; i < nTab + 2 ; i++ ){
						SyBlobAppend(&(*pOut)," ",sizeof(char));
					}
					OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,TRUE);
					SyBlobAppend(&(*pOut),"\n",sizeof(char));
					for( i = 0 ; i < nTab + 2 ; i++ ){
						SyBlobAppend(&(*pOut)," ",sizeof(char));
					}
					SyBlobFormat(&(*pOut),"uninitialized(%s)\n",zText);
				}
				continue;
			}
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
	rc = PH7_VmCallMagicMethod(pVm,&(*pThis),pMeth,pResult,nArg,apArg);
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
	{
		/* php's get_properties handler, which is what the (array) cast reads: a
		 * DateTime casts to date/timezone_type/timezone, not to the hidden slots
		 * holding its timestamp. A class whose hook answers only the DEBUG surface
		 * (WeakReference) fills nothing here, and that EMPTY answer is php's — the
		 * hook is authoritative, so there is no falling back to the slot walk. It
		 * used to fall back when the hook filled nothing, which was invisible while
		 * every hooked class also had no visible property: an ArrayObject SUBCLASS
		 * with an empty storage would have cast to its own `p` where php casts to
		 * the (empty) storage. */
		ph7_value sPresent;
		PH7_MemObjInit(pThis->pVm,&sPresent);
		sPresent.x.pOther = pMap;
		MemObjSetType(&sPresent,MEMOBJ_HASHMAP);
		if( PH7_ClassInstancePresent(pThis,&sPresent,0) ){
			/* The map IS the destination; do not release the carrier's hashmap. */
			sPresent.x.pOther = 0;
			sPresent.iFlags = MEMOBJ_NULL;
			return SXRET_OK;
		}
		sPresent.x.pOther = 0;
		sPresent.iFlags = MEMOBJ_NULL;
		PH7_MemObjRelease(&sPresent);
	}
	return PH7_ClassInstanceToHashmapRaw(pThis,pMap);
}
/*
 * Is this property NOT THERE YET?
 *
 * php 7.4's typed properties have a third state beside "holds a value" and "does not
 * exist": a typed property with no default is UNINITIALIZED at `new`, reading it is an
 * Error rather than a null, and every surface that presents an object's properties
 * leaves it out — get_object_vars(), get_mangled_object_vars(), the (array) cast,
 * foreach, json_encode(), serialize(), print_r() and var_export(). var_dump() is the one
 * exception and only half of one: it NAMES the property and prints `uninitialized(T)`
 * where the value would be, and does not count it in the header.
 *
 * The state is already tracked (it is what makes the read throw); this asks it by name
 * so the eight surfaces agree. VM_CLASS_ATTR_UNINIT doubles as the readonly write-once
 * latch, which wants the same answer: a readonly property must be typed and cannot have
 * a default, so before its first write php calls it uninitialized too.
 */
PH7_PRIVATE int PH7_ClassAttrUninitialized(VmClassAttr *pVmAttr)
{
	return pVmAttr != 0 && (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) != 0;
}
/*
 * The same question asked by the four surfaces that read through a php 8.4 `get` HOOK —
 * get_object_vars(), json_encode(), foreach and var_export().
 *
 * A hooked property's value is whatever its hook answers, so an empty backing slot is
 * not an absence there: a VIRTUAL property has no slot at all and still has a value
 * (php lists `virt` in all four), and a BACKED one whose hook reads its own slot raises
 * the uninitialized Error from inside the hook — which is php's answer for these four
 * and not something to pre-empt by skipping the property. Only a property with no `get`
 * at all is absent.
 */
PH7_PRIVATE int PH7_ClassAttrUninitializedForRead(VmClassAttr *pVmAttr)
{
	return PH7_ClassAttrUninitialized(pVmAttr)
		&& (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET) == 0;
}
/*
 * The SLOT walk under the cast above, with php's get_properties handler left out:
 * the instance's own property table, mangled, and nothing else.
 *
 * This is what `get_mangled_object_vars()` answers — php asks the same handler with a
 * different PURPOSE there, and every class here that has a handler answers the raw
 * table for it: `get_mangled_object_vars(new ArrayObject([1,2]))` is the EMPTY array
 * where the cast is `[1,2]`, and a DateTime's is empty where the cast has three keys.
 * Since PHL's engine slots are hidden (and php's equivalents live outside the property
 * table entirely), dropping the handler is the whole difference.
 */
static sxi32 ClassInstanceToHashmapRaw(ph7_class_instance *pThis,ph7_hashmap *pMap,int bOwnOnly)
{
	SyHashEntry *pEntry;
	VmClassAttr *pAttr;
	ph7_value *pValue;
	ph7_value sName;
	/* Reset the loop cursor */
	SyHashResetLoopCursor(&pThis->hAttr);
	PH7_MemObjInitFromString(pThis->pVm,&sName,0);
	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){
		/* Point to the current attribute */
		pAttr = (VmClassAttr *)pEntry->pUserData;
		if( !PH7_ClassInstanceAttrPresented(pAttr) ){
			/* Not part of the raw table: a class-level member, a typed property
			 * never written, or a php 8.4 VIRTUAL hooked one. */
			continue;
		}
		if( bOwnOnly && (pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_NATIVE_SET
			|PH7_CLASS_ATTR_NATIVE_VIRTUAL|PH7_CLASS_ATTR_NATIVE_LAZY)) ){
			/* The STATE of a native class whose state happens to be public
			 * (DateInterval's ten, DatePeriod's seven): the caller is building the
			 * shape those belong to, and wants only what the OBJECT added to it. */
			continue;
		}
		/* Extract attribute value */
		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);
		if( pValue ){
			PH7_ClassInstanceAttrKey(pThis,pAttr,&sName);
			/* Perform the insertion. An OWN-props walk laid beside a shape the
			 * caller already built ADDS rather than updates: php's
			 * add_common_properties is a zend_hash_add, so a subclass property
			 * named like one of the internal keys loses to the internal value
			 * there (`class S extends DateTime { public $date; }` serializes the
			 * DATE). */
			if( bOwnOnly ){
				ph7_hashmap_node *pDup = 0;
				if( PH7_HashmapLookup(pMap,&sName,&pDup) == SXRET_OK ){
					SyBlobReset(&sName.sBlob);
					continue;
				}
			}
			PH7_HashmapInsert(pMap,&sName,pValue);
			/* Reset the string cursor */
			SyBlobReset(&sName.sBlob);
		}
	}
	PH7_MemObjRelease(&sName);
	return SXRET_OK;
}
PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmapRaw(ph7_class_instance *pThis,ph7_hashmap *pMap)
{
	return ClassInstanceToHashmapRaw(pThis,pMap,0);
}
/*
 * The same walk, restricted to what the OBJECT added: a native class's own public
 * STATE is left out, so a subclass's properties can be laid beside the shape that
 * state builds rather than inside it. php's add_common_properties.
 */
PH7_PRIVATE sxi32 PH7_ClassInstanceOwnPropsToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap)
{
	return ClassInstanceToHashmapRaw(pThis,pMap,1);
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
		if( PH7_ATTR_UNPRESENTED(pAttr) ){
			/* Class-level members are not part of the object (php) */
			continue;
		}
		if( PH7_ClassAttrUninitialized(pAttr) ){
			continue; /* typed, never written: not there yet (php) */
		}
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
 * The instance's attribute entry for a property name, INCLUDING the empty one.
 *
 * SyHashGet answers 0 for a zero-length key engine-wide, so a property named ""
 * — php's own `s:0:""` payload builds one, and every surface that walks hAttr
 * shows it — can only be found by walking the list. Used by the presentation
 * surfaces that snapshot names and re-look-up each one before reading it (a PHP
 * 8.4 get hook may unset a property mid-walk), where the plain hash probe would
 * silently drop it from the output while var_dump, which walks directly, showed
 * it. The walk uses the hash's embedded loop cursor, so it must not run inside
 * another walk of the SAME table — the callers below re-enter only through the
 * hook dispatch, which happens after this returns.
 */
PH7_PRIVATE SyHashEntry * PH7_ClassInstanceAttrEntry(ph7_class_instance *pThis,const char *zName,sxu32 nName)
{
	SyHashEntry *pEntry;
	if( nName > 0 ){
		return SyHashGet(&pThis->hAttr,(const void *)zName,nName);
	}
	SyHashResetLoopCursor(&pThis->hAttr);
	while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){
		if( pEntry->nKeyLen == 0 ){
			return pEntry;
		}
	}
	return 0;
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
/*
 * Does a WRITE through `$obj[k]` land on this class's storage? php answers with
 * the class's read_dimension handler: an internal class whose own handler hands
 * back the real element supports indirect modification, while everything routed
 * through zend_std_read_dimension — every userland ArrayAccess, and the SPL
 * classes that keep the standard handler (SplFixedArray, the SplDoublyLinkedList
 * family, SplObjectStorage) — gets a TEMPORARY back, so the write is lost and php
 * says so. The engine cannot tell those apart from the interface alone: both
 * implement ArrayAccess.
 *
 * PHL's equivalent evidence is the offsetGet that ANSWERS: the native one on a
 * class carrying PH7_CLASS_DIM_WRITABLE means the storage is reachable, and a
 * userland OVERRIDE of it means it is not — which is php's own rule for an
 * ArrayObject subclass (spl_array_read_dimension steps aside for a declared
 * offsetGet). A `&offsetGet` return is php's other yes: it hands back a reference,
 * so the write reaches whatever it aliases.
 */
PH7_PRIVATE int PH7_VmDimFetchWritable(ph7_class *pClass)
{
	ph7_class_method *pGet;
	ph7_class *pCur;
	if( pClass == 0 ){
		return FALSE;
	}
	pGet = PH7_ClassExtractMethod(pClass,"offsetGet",sizeof("offsetGet")-1);
	if( pGet == 0 ){
		return FALSE;
	}
	if( pGet->sFunc.iFlags & VM_FUNC_REF_RETURN ){
		return TRUE;
	}
	if( (pGet->sFunc.iFlags & VM_FUNC_NATIVE) == 0 ){
		return FALSE;
	}
	for( pCur = pClass ; pCur ; pCur = pCur->pBase ){
		if( pCur->iFlags & PH7_CLASS_DIM_WRITABLE ){
			return TRUE;
		}
	}
	return FALSE;
}
/*
 * The three accessors a VM_FUNC_NATIVE method body uses to reach its receiver.
 *
 * A native method is dispatched down the host-function path, so it is handed the
 * plain (ph7_context*, argc, argv) of any builtin — the receiver rides on the
 * context instead of occupying an argument slot. That is the whole point: the C
 * routine that used to be a global `__prefix_verb($target, ...)` thunk taking its
 * target explicitly becomes a method whose target is $this.
 */
/*
 * The receiver, or NULL when the method was called statically (and always NULL in
 * a plain host function). Borrowed: the operand stack holds the reference for the
 * duration of the call, so the body must not unref it.
 */
PH7_PRIVATE ph7_class_instance * PH7_ContextThis(ph7_context *pCtx)
{
	return pCtx->pThis;
}
/*
 * The class the call was made THROUGH — php's late-static-binding target, so an
 * inherited native method sees the SUBCLASS here, not the class that declared it.
 * NULL in a plain host function.
 */
PH7_PRIVATE ph7_class * PH7_ContextCalledClass(ph7_context *pCtx)
{
	return pCtx->pCalledClass;
}
/*
 * The receiver as a ph7_value, so the body can use the ordinary object helpers
 * (ph7_object_fetch_attr, ph7_value_is_object, ph7_result_value, ...) instead of
 * reaching into ph7_class_instance. NULL when there is no receiver.
 *
 * The view ALIASES the instance without bumping its refcount: it lives exactly as
 * long as the call, during which the caller's reference is already keeping the
 * object alive, and VmReleaseCallContext drops the alias without unreffing. Handing
 * it to ph7_result_value() is safe — that copies through PH7_MemObjStore, which
 * takes its own reference.
 */
PH7_PRIVATE ph7_value * PH7_ContextThisValue(ph7_context *pCtx)
{
	if( pCtx->pThis == 0 ){
		return 0;
	}
	if( !pCtx->bThisInit ){
		PH7_MemObjInit(pCtx->pVm,&pCtx->sThis);
		pCtx->sThis.x.pOther = pCtx->pThis;
		pCtx->sThis.iFlags = MEMOBJ_OBJ;
		pCtx->sThis.nIdx = SXU32_HIGH;
		pCtx->bThisInit = 1;
	}
	return &pCtx->sThis;
}
