/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
PH7_PRIVATE int vm_builtin_get_class(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class *pClass;
	SyString *pName;
	if( nArg < 1 ){
		/* Check if we are inside a class */
		pClass = PH7_VmPeekTopClass(pCtx->pVm);
		if( pClass ){
			/* Point to the class name */
			pName = &pClass->sName;
			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);
		}else{
			/* Not inside class,return FALSE */
			ph7_result_bool(pCtx,0);
		}
	}else{
		/* Extract the target class */
		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);
		if( pClass ){
			pName = &pClass->sName;
			/* Return the class name */
			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);
		}else{
			/* Not a class instance,return FALSE */
			ph7_result_bool(pCtx,0);
		}
	}
	return PH7_OK;
}
/*
 * string get_parent_class([object $object = NULL ] )
 *   Returns the name of the parent class of an object
 * Parameters
 *  object
 *   The tested object. This parameter may be omitted when inside a class.
 * Return
 *  The name of the parent class of which object is an instance.
 *  Returns FALSE if object is not an object or if the object does
 *  not have a parent.
 *  If object is omitted when inside a class, the name of that class is returned.
 */
PH7_PRIVATE int vm_builtin_get_parent_class(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class *pClass;
	SyString *pName;
	if( nArg < 1 ){
		/* Check if we are inside a class [i.e: a method call]*/
		pClass = PH7_VmPeekTopClass(pCtx->pVm);
		if( pClass && pClass->pBase ){
			/* Point to the class name */
			pName = &pClass->pBase->sName;
			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);
		}else{
			/* Not inside class,return FALSE */
			ph7_result_bool(pCtx,0);
		}
	}else{
		/* Extract the target class */
		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);
		if( pClass ){
			if( pClass->pBase ){
				pName = &pClass->pBase->sName;
				/* Return the parent class name */
				ph7_result_string(pCtx,pName->zString,(int)pName->nByte);
			}else{
				/* Object does not have a parent class */
				ph7_result_bool(pCtx,0);
			}
		}else{
			/* Not a class instance,return FALSE */
			ph7_result_bool(pCtx,0);
		}
	}
	return PH7_OK;
}
/*
 * string get_called_class(void)
 *   Gets the name of the class the static method is called in.
 * Parameters
 *  None.
 * Return
 *  Returns the class name. Returns FALSE if called from outside a class.
 */
PH7_PRIVATE int vm_builtin_get_called_class(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class *pClass;
	/* Check if we are inside a class [i.e: a method call] */
	pClass = PH7_VmPeekTopClass(pCtx->pVm);
	if( pClass ){
		SyString *pName;
		/* Point to the class name */
		pName = &pClass->sName;
		ph7_result_string(pCtx,pName->zString,(int)pName->nByte);
	}else{
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		/* Not inside class,return FALSE */
		ph7_result_bool(pCtx,0);
	}
	return PH7_OK;
}
/*
 * Extract a ph7_class from the given ph7_value.
 * The given value must be of type object [i.e: class instance] or
 * string which hold the class name.
 */
PH7_PRIVATE ph7_class * PH7_VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg)
{
	ph7_class *pClass = 0;
	if( ph7_value_is_object(pArg) ){
		/* Class instance already loaded,no need to perform a lookup */
		pClass = ((ph7_class_instance *)pArg->x.pOther)->pClass;
	}else if( ph7_value_is_string(pArg) ){
		const char *zClass;
		int nLen;
		/* Extract class name */
		zClass = ph7_value_to_string(pArg,&nLen);
		/* A leading '\' (the global-namespace anchor) is stripped by
		 * PH7_VmExtractClass itself now (PH7_VmClassNameAnchor), so do not
		 * strip here too — a second strip would wrongly resolve "\\Foo". */
		if( nLen > 0 ){
			/* Resolve through PH7_VmExtractClass so a class named by STRING is
			 * autoloaded on a miss — php autoloads the class of a [class,method]
			 * callable (is_callable/array_map/call_user_func), and of the class
			 * argument to method_exists()/property_exists(). iLoadable=FALSE keeps
			 * abstract classes and interfaces (a static method on an abstract class
			 * is a valid callable). */
			pClass = PH7_VmExtractClass(pVm,zClass,(sxu32)nLen,FALSE,0);
		}
	}
	return pClass;
}
/*
 * bool property_exists(mixed $class,string $property)
 *   Checks if the object or class has a property.
 * Parameters
 *  class
 *   The class name or an object of the class to test for
 * property
 *  The name of the property
 * Return
 *   Returns TRUE if the property exists,FALSE otherwise.
 */
PH7_PRIVATE int vm_builtin_property_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume attribute does not exists */
	if( nArg > 1 ){
		ph7_class *pClass;
		if( (apArg[0]->iFlags & MEMOBJ_OBJ)
		 && PH7_VmIsIncompleteClass(pCtx->pVm,((ph7_class_instance *)apArg[0]->x.pOther)->pClass) ){
			/* An incomplete OBJECT: php's has_property probe is the access warning
			 * (qualified with this builtin's own name) answering false. The class
			 * asked about by NAME stays an ordinary lookup. */
			SyBlob sIncMsg;
			SyBlobInit(&sIncMsg,&pCtx->pVm->sAllocator);
			PH7_VmIncompleteMsg(pCtx->pVm,(ph7_class_instance *)apArg[0]->x.pOther,
				"access a property",&sIncMsg);
			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%.*s",
				(int)SyBlobLength(&sIncMsg),(const char *)SyBlobData(&sIncMsg));
			SyBlobRelease(&sIncMsg);
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);
		if( pClass ){
			const char *zName;
			int nLen;
			/* Extract attribute name */
			zName = ph7_value_to_string(apArg[1],&nLen);
			if( nLen > 0 ){
				/* php looks in ce->properties_info and NOWHERE else: a METHOD of this
				 * name is not a property (`property_exists('C','someMethod')` is false),
				 * and neither is a class CONSTANT. PHL searched the method table too and
				 * answered true for both. */
				SyHashEntry *pAttrE = SyHashGet(&pClass->hAttr,(const void *)zName,(sxu32)nLen);
				ph7_class_attr *pAttr = pAttrE ? (ph7_class_attr *)pAttrE->pUserData : 0;
				if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){
					/* A base's PRIVATE property is invisible to the child it was asked
					 * about, php's `property_info->ce == ce` rule: PHL copies one down
					 * onto every child (its own methods read it through $this), so the
					 * table alone said true where php says false. Static or instance,
					 * the rule is the same; protected and public are inherited outright.
					 * A trait's property belongs to the class that COMPOSED it. */
					res = 1;
					if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE
					 && pAttr->pDeclClass != 0
					 && PH7_VmComposingClass(pClass,pAttr->pDeclClass) != pClass ){
						res = 0;
					}
				}
				/* A DYNAMIC (runtime-added) property lives on the INSTANCE's
				 * attribute table, not the class's — php reports those too
				 * (band A #3b; pre-fix property_exists() was blind to them). */
				if( res == 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){
					ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;
					SyHashEntry *pObjE = pThis
						? SyHashGet(&pThis->hAttr,(const void *)zName,(sxu32)nLen) : 0;
					VmClassAttr *pObjAttr = pObjE ? (VmClassAttr *)pObjE->pUserData : 0;
					/* Only a genuinely DYNAMIC one: every instance carries a slot for every
					 * DECLARED member too, so an unqualified instance lookup answered true
					 * for the base private the class-table rule above had just refused. */
					if( pObjAttr && pObjAttr->pAttr
					 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC) ){
						res = 1;
					}
				}
			}
		}
	}
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * bool method_exists(mixed $class,string $method)
 *   Checks if the given method is a class member.
 * Parameters
 *  class
 *   The class name or an object of the class to test for
 * property
 *  The name of the method
 * Return
 *   Returns TRUE if the method exists,FALSE otherwise.
 */
PH7_PRIVATE int vm_builtin_method_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume method does not exists */
	if( nArg > 1 ){
		ph7_class *pClass;
		if( (apArg[0]->iFlags & MEMOBJ_OBJ)
		 && PH7_VmIsIncompleteClass(pCtx->pVm,((ph7_class_instance *)apArg[0]->x.pOther)->pClass) ){
			/* An incomplete OBJECT consults its method resolution, which the
			 * carrier refuses with php's catchable call Error (probe-verified;
			 * the class asked about by NAME answers false the ordinary way). */
			SyBlob sIncMsg;
			sxi32 rcInc;
			SyBlobInit(&sIncMsg,&pCtx->pVm->sAllocator);
			PH7_VmIncompleteMsg(pCtx->pVm,(ph7_class_instance *)apArg[0]->x.pOther,
				"call a method",&sIncMsg);
			rcInc = PH7_VmThrowException(pCtx,"Error","%.*s",
				(int)SyBlobLength(&sIncMsg),(const char *)SyBlobData(&sIncMsg));
			SyBlobRelease(&sIncMsg);
			return rcInc;
		}
		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);
		if( pClass ){
			const char *zName;
			int nLen;
			/* Extract method name */
			zName = ph7_value_to_string(apArg[1],&nLen);
			if( nLen > 0 ){
				/* Perform the lookup in the method table */
				SyHashEntry *pEntry = SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen);
				if( pEntry ){
					/* ...and apply php's one visibility rule here (`func->common.scope
					 * == ce`): a PRIVATE method is only a method of the class that
					 * declares it. PHL copies a base's private down so an inherited
					 * public method can still dispatch it, which made
					 * `method_exists('Child','basePrivate')` answer true where php
					 * answers false — the same shape property_exists() had. Nothing
					 * about the CALLING scope enters into it: php answers false for the
					 * child from inside the BASE too. A trait's method belongs to the
					 * class that COMPOSED it. */
					ph7_class_method *pMeth = (ph7_class_method *)pEntry->pUserData;
					res = 1;
					if( pMeth->iProtection == PH7_CLASS_PROT_PRIVATE
					 && PH7_VmMethodScopeName(pCtx->pVm,pClass,pMeth) != pClass ){
						res = 0;
					}
				}
			}
		}
	}
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * bool class_exists(string $class_name [, bool $autoload = true ] )
 *   Checks if the class has been defined.
 * Parameters
 *  class_name
 *   The class name. The name is matched in a case-sensitive manner
 *   unlinke the standard PHP engine.
 *  autoload
 *   Whether or not to call __autoload by default.
 * Return
 *   TRUE if class_name is a defined class, FALSE otherwise.
 */
PH7_PRIVATE int vm_builtin_class_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume class does not exist */
	if( nArg > 0 ){
		SyHashEntry *pEntry = 0;
		const char *zName;
		int nLen;
		int iAutoload = 1; /* Default: autoload enabled */
		sxu32 nName;
		/* Extract given name */
		zName = ph7_value_to_string(apArg[0],&nLen);
		if( nArg >= 2 ){
			iAutoload = ph7_value_to_bool(apArg[1]);
		}
		/* Strip a leading '\' (global-namespace anchor) — this builtin hashes
		 * hClass directly, bypassing PH7_VmExtractClass, so anchor here. */
		nName = (sxu32)nLen;
		PH7_VmClassNameAnchor(&zName,&nName);
		if( nName > 0 ){
			/* Perform a hash lookup first */
			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);
		}
		/* A lone "\" strips to no name, and php hands no name to the autoloader. */
		if( pEntry == 0 && nName > 0 && iAutoload ){
			/* Try autoload, then re-check */
			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,nName,FALSE);
			if( pClass ){
				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);
			}
		}
		if( pEntry ){
			/* Walk the collision chain: return TRUE only for concrete or abstract classes,
			 * not for interfaces or traits (matching PHP behavior). */
			ph7_class *pClass = (ph7_class *)pEntry->pUserData;
			while( pClass ){
				if( (pClass->iFlags & (PH7_CLASS_INTERFACE|PH7_CLASS_TRAIT)) == 0 ){
					res = 1;
					break;
				}
				pClass = pClass->pNextName;
			}
		}
	}
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * bool interface_exists(string $class_name [, bool $autoload = true ] )
 *   Checks if the interface has been defined.
 * Parameters
 *  class_name
 *   The class name. The name is matched in a case-sensitive manner
 *   unlinke the standard PHP engine.
 *  autoload
 *   Whether or not to call __autoload by default.
 * Return
 *   TRUE if class_name is a defined class, FALSE otherwise.
 */
PH7_PRIVATE int vm_builtin_interface_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume interface does not exist */
	if( nArg > 0 ){
		SyHashEntry *pEntry = 0;
		const char *zName;
		int nLen;
		int iAutoload = 1; /* Default: autoload enabled */
		sxu32 nName;
		/* Extract given name */
		zName = ph7_value_to_string(apArg[0],&nLen);
		if( nArg >= 2 ){
			iAutoload = ph7_value_to_bool(apArg[1]);
		}
		/* Strip a leading '\' (global-namespace anchor); this builtin bypasses
		 * PH7_VmExtractClass and hashes hClass directly. */
		nName = (sxu32)nLen;
		PH7_VmClassNameAnchor(&zName,&nName);
		/* Perform a hash lookup */
		if( nName > 0 ){
			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);
		}
		/* A lone "\" strips to no name, and php hands no name to the autoloader. */
		if( pEntry == 0 && nName > 0 && iAutoload ){
			/* Try autoload — pass iLoadable=FALSE so we get interfaces too */
			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,nName,FALSE);
			if( pClass ){
				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);
			}
		}
		if( pEntry ){
			ph7_class *pClass = (ph7_class *)pEntry->pUserData;
			while( pClass ){
				if( pClass->iFlags & PH7_CLASS_INTERFACE ){
					/* interface is available */
					res = 1;
					break;
				}
				/* Next with the same name */
				pClass = pClass->pNextName;
			}
		}
	}
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * bool trait_exists(string $trait [, bool $autoload = true ] )
 *   Checks if the trait has been defined.
 * Parameters
 *  trait
 *   The trait name (case-sensitive here, unlike the standard PHP engine).
 *  autoload
 *   Whether to invoke autoloading if the trait is not yet defined.
 * Return
 *   TRUE if trait is a defined trait, FALSE otherwise.
 */
PH7_PRIVATE int vm_builtin_trait_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume trait does not exist */
	if( nArg > 0 ){
		SyHashEntry *pEntry = 0;
		const char *zName;
		int nLen;
		int iAutoload = 1; /* Default: autoload enabled */
		sxu32 nName;
		/* Extract given name */
		zName = ph7_value_to_string(apArg[0],&nLen);
		if( nArg >= 2 ){
			iAutoload = ph7_value_to_bool(apArg[1]);
		}
		/* Strip a leading '\' (global-namespace anchor); this builtin bypasses
		 * PH7_VmExtractClass and hashes hClass directly. */
		nName = (sxu32)nLen;
		PH7_VmClassNameAnchor(&zName,&nName);
		/* Perform a hash lookup */
		if( nName > 0 ){
			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);
		}
		/* A lone "\" strips to no name, and php hands no name to the autoloader. */
		if( pEntry == 0 && nName > 0 && iAutoload ){
			/* Try autoload — pass iLoadable=FALSE so we get traits too */
			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,nName,FALSE);
			if( pClass ){
				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);
			}
		}
		if( pEntry ){
			ph7_class *pClass = (ph7_class *)pEntry->pUserData;
			while( pClass ){
				if( pClass->iFlags & PH7_CLASS_TRAIT ){
					/* trait is available */
					res = 1;
					break;
				}
				/* Next with the same name */
				pClass = pClass->pNextName;
			}
		}
	}
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * bool class_alias([string $original[,string $alias ]])
 *   Creates an alias for a class.
 * Parameters
 *  original
 *    The original class.
 *  alias
 *   The alias name for the class.
 * Return
 *   Returns TRUE on success or FALSE on failure.
 */
PH7_PRIVATE int vm_builtin_class_alias(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zOld,*zNew;
	int nOldLen,nNewLen;
	sxu32 nOld,nNew;
	SyHashEntry *pEntry;
	ph7_class *pClass;
	char *zDup;
	sxi32 rc;
	if( nArg < 2 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract old class name */
	zOld = ph7_value_to_string(apArg[0],&nOldLen);
	/* Extract alias name */
	zNew = ph7_value_to_string(apArg[1],&nNewLen);
	/* Strip a leading '\' (global-namespace anchor) from BOTH names: php
	 * resolves the target and stores the alias without it, so class_exists()
	 * on the plain name then matches. */
	nOld = (sxu32)nOldLen;
	nNew = (sxu32)nNewLen;
	PH7_VmClassNameAnchor(&zOld,&nOld);
	PH7_VmClassNameAnchor(&zNew,&nNew);
	if( nNew < 1 ){
		/* Invalid alias name,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform a hash lookup */
	pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zOld,nOld);
	if( pEntry ==  0 ){
		/* No such class,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the class */
	pClass = (ph7_class *)pEntry->pUserData;
	/* Duplicate alias name */
	zDup = SyMemBackendStrDup(&pCtx->pVm->sAllocator,zNew,nNew);
	if( zDup == 0 ){
		/* Out of memory,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Create the alias */
	rc = SyHashInsert(&pCtx->pVm->hClass,(const void *)zDup,nNew,pClass);
	if( rc != SXRET_OK ){
		SyMemBackendFree(&pCtx->pVm->sAllocator,zDup);
	}
	ph7_result_bool(pCtx,rc == SXRET_OK);
	return PH7_OK;
}
/*
 * The three KINDS hClass holds. php keeps classes, interfaces, traits and enums in one
 * table too and each of its three list builtins filters that table down to its own kind:
 * an ENUM is a class (`get_declared_classes()` reports it), an interface and a trait are
 * not.
 */
#define VM_DECLARED_CLASS      0
#define VM_DECLARED_INTERFACE  1
#define VM_DECLARED_TRAIT      2

static int VmDeclaredEntryKind(ph7_class *pClass)
{
	if( pClass->iFlags & PH7_CLASS_INTERFACE ){
		return VM_DECLARED_INTERFACE;
	}
	if( pClass->iFlags & PH7_CLASS_TRAIT ){
		return VM_DECLARED_TRAIT;
	}
	return VM_DECLARED_CLASS;
}
struct VmDeclaredList {
	int iKind;           /* Which VM_DECLARED_* kind this list wants */
	ph7_value *pArray;   /* The array being built */
	ph7_value *pName;    /* Scratch name */
};
/*
 * One row of a get_declared_*() answer.
 *
 * The NAME reported is the table KEY, not the class struct's own name — a distinction
 * only `class_alias()` makes visible, since it puts a second key over the same class.
 * php reports the class's declared spelling for the key that IS its name and the ALIAS
 * for the other, so `class_alias('C1','C1Alias')` answers both `C1` and `c1alias`,
 * lower-cased because that is the spelling php's own alias key is stored under. PHL
 * keeps the declared spelling in every key, so the fold is applied here.
 */
static sxi32 VmDeclaredNameStep(SyHashEntry *pEntry,void *pUserData)
{
	struct VmDeclaredList *pList = (struct VmDeclaredList *)pUserData;
	ph7_class *pClass = (ph7_class *)pEntry->pUserData;
	SyString *pDecl = &pClass->sName;
	if( VmDeclaredEntryKind(pClass) != pList->iKind ){
		return SXRET_OK;
	}
	if( pEntry->nKeyLen == pDecl->nByte
		&& SyStrnmicmp((const char *)pEntry->pKey,pDecl->zString,pEntry->nKeyLen) == 0 ){
		/* The key this class was DECLARED under */
		ph7_value_string(pList->pName,pDecl->zString,(int)pDecl->nByte);
	}else{
		/* A class_alias() key: php reports it folded */
		const char *zKey = (const char *)pEntry->pKey;
		sxu32 n;
		for( n = 0 ; n < pEntry->nKeyLen ; ++n ){
			char c = (char)SyToLower(zKey[n]);
			ph7_value_string(pList->pName,&c,1);
		}
	}
	ph7_array_add_elem(pList->pArray,0/*Automatic index assign*/,pList->pName); /* Will make it's own copy */
	ph7_value_reset_string_cursor(pList->pName);
	return SXRET_OK;
}

/*
 * Build php's get_declared_classes()/get_declared_interfaces()/get_declared_traits()
 * answer for one kind.
 */
static int VmDeclaredNameList(ph7_context *pCtx,int iKind)
{
	struct VmDeclaredList sList;
	/* Create a new array first */
	sList.iKind = iKind;
	sList.pArray = ph7_context_new_array(pCtx);
	sList.pName = ph7_context_new_scalar(pCtx);
	if( sList.pArray == 0 || sList.pName == 0 ){
		/* Out of memory,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* hClass is head-pushed, so its forward order is reverse-insertion; php reports
	 * these lists in DECLARATION order (its own class table is append-ordered), which
	 * is what the backward walk yields. */
	SyHashForEachReverse(&pCtx->pVm->hClass,VmDeclaredNameStep,(void *)&sList);
	/* Return the created array */
	ph7_result_value(pCtx,sList.pArray);
	return PH7_OK;
}
/*
 * array get_declared_classes(void)
 *   Returns an array with the name of the defined classes
 * Parameters
 *  None
 * Return
 *   Returns an array of the names of the declared classes
 *   in the current script.
 * Note:
 *   NULL is returned on failure.
 */
PH7_PRIVATE int vm_builtin_get_declared_classes(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); /* cc warning */
	SXUNUSED(apArg);
	return VmDeclaredNameList(pCtx,VM_DECLARED_CLASS);
}
/*
 * array get_declared_interfaces(void)
 *   Returns an array with the name of the defined interfaces
 * Parameters
 *  None
 * Return
 *   Returns an array of the names of the declared interfaces
 *   in the current script.
 * Note:
 *   NULL is returned on failure.
 */
PH7_PRIVATE int vm_builtin_get_declared_interfaces(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); /* cc warning */
	SXUNUSED(apArg);
	return VmDeclaredNameList(pCtx,VM_DECLARED_INTERFACE);
}
/*
 * array get_declared_traits(void)
 *   Returns an array with the name of the defined traits.
 */
PH7_PRIVATE int vm_builtin_get_declared_traits(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); /* cc warning */
	SXUNUSED(apArg);
	return VmDeclaredNameList(pCtx,VM_DECLARED_TRAIT);
}
/*
 * Does this method-table entry answer to the method's OWN name (rather than to an
 * adaptation alias made from it)? Method names fold case, so the comparison does too.
 */
static int VmMethodEntryIsOwnName(SyHashEntry *pEntry,ph7_class_method *pMeth)
{
	return pEntry->nKeyLen == pMeth->sFunc.sName.nByte
		&& SyStrnmicmp(pEntry->pKey,pMeth->sFunc.sName.zString,pEntry->nKeyLen) == 0;
}
/*
 * Which inheritance LEVEL does this method-table entry belong to — the class php would
 * have added it under? A method declared in a class body is its own; a trait's is the
 * class that COMPOSED it, which is two different questions depending on the entry. An
 * adaptation ALIAS is a copy no other class made, so the HIGHEST class in the chain still
 * holding this key over this very struct is the one whose `use` block wrote it. A trait
 * method under its own name is the same struct in every class that uses the trait, and
 * php's own table shows the LOWEST one: a subclass that re-uses its parent's trait
 * composes its own copy, and the parent's inherited entry never replaces it.
 */
static ph7_class * VmMethodListLevel(ph7_class *pClass,SyHashEntry *pEntry,ph7_class_method *pMeth)
{
	ph7_class *pDecl = (ph7_class *)pMeth->sFunc.pUserData;
	ph7_class *pWalk,*pHigh = 0;
	if( pDecl == 0 || (pDecl->iFlags & PH7_CLASS_TRAIT) == 0 ){
		return pDecl;
	}
	if( !VmMethodEntryIsOwnName(pEntry,pMeth) ){
		for( pWalk = pClass ; pWalk ; pWalk = pWalk->pBase ){
			SyHashEntry *pE = SyHashGet(&pWalk->hMethod,pEntry->pKey,pEntry->nKeyLen);
			if( pE && pE->pUserData == (void *)pMeth ){
				pHigh = pWalk;
			}
		}
		if( pHigh ){
			return pHigh;
		}
	}
	return PH7_VmComposingClass(pClass,pDecl);
}
/*
 * Append one method-table entry's name to the result array. The name is the entry's HASH
 * KEY, not sFunc.sName: a trait adaptation alias (`hi as bHi`) keeps the original name in
 * its method struct while the key carries the alias — php lists the alias.
 */
static void VmEmitMethodName(ph7_value *pArray,ph7_value *pName,SyHashEntry *pEntry)
{
	ph7_value_string(pName,(const char *)pEntry->pKey,(int)pEntry->nKeyLen);
	ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */
	ph7_value_reset_string_cursor(pName);
}
/*
 * array get_class_methods(object|string $object_or_class)
 *   Returns an array with the names of the class methods the CALLING SCOPE can reach,
 *   in php's order: each class's own body methods, then its trait composition, then the
 *   same again for every ancestor.
 * Parameters
 *  object_or_class
 *   The class name or a class instance. Anything that does not resolve to a class is a
 *   TypeError naming the type given — this builtin never answers NULL.
 */
PH7_PRIVATE int vm_builtin_get_class_methods(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pName,*pArray;
	SyHashEntry *pEntry;
	ph7_class *pClass;
	/* Extract the target class first */
	pClass = 0;
	if( nArg > 0 ){
		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);
	}
	if( pClass == 0 ){
		/* php screens the VALUE, not the type: anything that does not resolve to a
		 * class — a name nothing declares, an int, an array, null — is ONE TypeError
		 * naming the type given. PHL answered NULL for most of them (and the shared
		 * ZPP screen's `must be of type object|string` for the rest), so a typo in a
		 * class name silently listed nothing. This is why get_class_methods() joins
		 * get_class_vars() on the self-checked list in vm_arg_check.c. */
		char zGiven[64];
		return PH7_VmThrowException(pCtx,"TypeError",
			"get_class_methods(): Argument #1 ($object_or_class) must be an object "
			"or a valid class name, %s given",
			nArg > 0 ? VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)) : "no value");
	}
	/* Create a new array  */
	pArray = ph7_context_new_array(pCtx);
	pName = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pName == 0){
		/* Out of memory,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Fill the array with the defined methods, in php's order: the class's own
	 * methods in DECLARATION order, then each ancestor's in ITS declaration
	 * order (band A #4 — the raw hash walk returned reverse-insertion/LIFO
	 * order). SyHash iterates newest-first, so a reversed walk restores
	 * insertion order; grouping by declaring class (sFunc.pUserData, the class
	 * a method was compiled into) walks own-then-parent like php. An override
	 * lives once in the hash under the subclass, so no dedup is needed. */
	{
		SySet aTmp;
		SyHashEntry **apEntry;
		ph7_class *pLevel;
		sxu32 n;
		/* Names are emitted from each entry's HASH KEY, not sFunc.sName: a trait
		 * adaptation alias (`hi as bHi`) keeps the original name in its method
		 * struct while the key carries the alias — php lists the alias. */
		SySetInit(&aTmp,&pCtx->pVm->sAllocator,sizeof(SyHashEntry *));
		SyHashResetLoopCursor(&pClass->hMethod);
		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){
			SySetPut(&aTmp,(const void *)&pEntry);
		}
		apEntry = (SyHashEntry **)SySetBasePtr(&aTmp);
		for( pLevel = pClass; pLevel; pLevel = pLevel->pBase ){
			/* Collect this level's methods, then emit in DECLARATION order
			 * (sorted by nLine — same-level methods share a source file; a
			 * hash-order fallback covers line-less internal methods). */
			SySet aLvl;
			SyHashEntry **apLvl;
			sxu32 i,j;
			SySetInit(&aLvl,&pCtx->pVm->sAllocator,sizeof(SyHashEntry *));
			/* Hash-order fallback for same-line methods: the class's OWN entries
			 * come out in declaration order when walked newest-first, while
			 * inherited copies (inserted by PH7_ClassInherit's walk of the base
			 * hash) come out in declaration order walked oldest-first. */
			for( n = 0; n < SySetUsed(&aTmp); n++ ){
				sxu32 nPick = (pLevel == pClass) ? (SySetUsed(&aTmp) - 1 - n) : n;
				ph7_class_method *pMethod = (ph7_class_method *)apEntry[nPick]->pUserData;
				/* The level a method belongs to is the class that OWNS it — for a trait
				 * method the class that composed it, not the trait. Reading sFunc.pUserData
				 * raw put every trait method on the CLASS's own level even when a BASE was
				 * the one that used the trait, so a subclass listed its inherited trait
				 * methods before its own. */
				ph7_class *pDecl = VmMethodListLevel(pClass,apEntry[nPick],pMethod);
				/* php lists only what the CALLING scope could reach: public always,
				 * protected within the hierarchy, private only from the class that
				 * declares it. PHL listed the whole table, so global-scope code was handed
				 * every private and protected name a class holds. Same decision
				 * get_class_vars() already makes for properties. */
				if( pMethod->iProtection != PH7_CLASS_PROT_PUBLIC ){
					SyString sMName;
					SyStringInitFromBuf(&sMName,(const char *)apEntry[nPick]->pKey,
						apEntry[nPick]->nKeyLen);
					/* The DECISION is the owning class's, which is not always the LEVEL
					 * above: an inherited alias is listed with the class whose `use` block
					 * wrote it, and judged against the class that composed the method. */
					if( !PH7_VmClassMemberAccess(pCtx->pVm,
							PH7_VmMethodScopeName(pCtx->pVm,pClass,pMethod),&sMName,
							pMethod->iProtection,FALSE) ){
						continue;
					}
				}
				if( pDecl != pLevel ){
					/* A declarer outside the base chain (a used trait, or none)
					 * counts as the class's own level, like php. */
					ph7_class *pWalk;
					if( pLevel != pClass || pDecl == pClass ){
						continue;
					}
					for( pWalk = pClass; pWalk; pWalk = pWalk->pBase ){
						if( pWalk == pDecl ){
							break;
						}
					}
					if( pWalk != 0 ){
						continue; /* in-chain: its own level emits it */
					}
				}
				SySetPut(&aLvl,(const void *)&apEntry[nPick]);
			}
			apLvl = (SyHashEntry **)SySetBasePtr(&aLvl);
			/* Insertion sort by declaration line (stable) */
			for( i = 1; i < SySetUsed(&aLvl); i++ ){
				SyHashEntry *pKey = apLvl[i];
				for( j = i; j > 0 && ((ph7_class_method *)apLvl[j-1]->pUserData)->nLine
						> ((ph7_class_method *)pKey->pUserData)->nLine; j-- ){
					apLvl[j] = apLvl[j-1];
				}
				apLvl[j] = pKey;
			}
			/* php's order INSIDE a level is not the line order: the class's own BODY methods
			 * come first, then each USED trait in `use` order, and within a trait each of its
			 * methods in the TRAIT's declaration order, preceded by the aliases made from it —
			 * `class C { function own(){} use T { m1 as z1; m1 as y1; } }` answers own, z1, y1,
			 * m1, m2. Sorting the level by line cannot say that: a trait method's line is the
			 * TRAIT's, so a whole composition sorted ahead of the class's own body. The line
			 * sort above still decides the body's order and, being stable, leaves two aliases
			 * of the same method in their adaptation-block order for the walk below. An emitted
			 * entry is cleared, so each name is listed once and anything these walks do not
			 * claim still goes out at the end. */
			for( i = 0; i < SySetUsed(&aLvl); i++ ){
				ph7_class_method *pM = (ph7_class_method *)apLvl[i]->pUserData;
				ph7_class *pOwn = (ph7_class *)pM->sFunc.pUserData;
				if( pOwn == 0 || pOwn == pLevel ){
					VmEmitMethodName(pArray,pName,apLvl[i]);
					apLvl[i] = 0;
				}
			}
			{
				ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pLevel->aTrait);
				sxu32 nTrait = SySetUsed(&pLevel->aTrait);
				sxu32 k;
				for( k = 0 ; k < nTrait ; ++k ){
					ph7_class *pTrait = apTrait[k];
					SySet aTr;
					SyHashEntry **apTr;
					SyHashEntry *pTrE;
					sxu32 t;
					SySetInit(&aTr,&pCtx->pVm->sAllocator,sizeof(SyHashEntry *));
					SyHashResetLoopCursor(&pTrait->hMethod);
					while((pTrE = SyHashGetNextEntry(&pTrait->hMethod)) != 0 ){
						SySetPut(&aTr,(const void *)&pTrE);
					}
					apTr = (SyHashEntry **)SySetBasePtr(&aTr);
					/* The trait's own table walks newest-first, so backwards is its
					 * declaration order. */
					for( t = SySetUsed(&aTr) ; t > 0 ; --t ){
						ph7_class_method *pOrigin = (ph7_class_method *)apTr[t-1]->pUserData;
						int bWantAlias;
						/* First pass emits the aliases made from this method, second the
						 * method itself — php's order for `m1 as z1`. */
						for( bWantAlias = 1 ; bWantAlias >= 0 ; --bWantAlias ){
							for( i = 0; i < SySetUsed(&aLvl); i++ ){
								ph7_class_method *pM;
								if( apLvl[i] == 0 ){
									continue;
								}
								pM = (ph7_class_method *)apLvl[i]->pUserData;
								if( (ph7_class *)pM->sFunc.pUserData != pTrait
								 || pM->sFunc.sName.nByte != pOrigin->sFunc.sName.nByte
								 || SyStrnmicmp(pM->sFunc.sName.zString,
										pOrigin->sFunc.sName.zString,
										pM->sFunc.sName.nByte) != 0 ){
									continue;
								}
								if( VmMethodEntryIsOwnName(apLvl[i],pM) == bWantAlias ){
									continue;
								}
								VmEmitMethodName(pArray,pName,apLvl[i]);
								apLvl[i] = 0;
							}
						}
					}
					SySetRelease(&aTr);
				}
			}
			for( i = 0; i < SySetUsed(&aLvl); i++ ){
				/* Whatever the two walks above did not claim — an alias made inside a trait
				 * that another trait then composed, say — keeps the line order. */
				if( apLvl[i] != 0 ){
					VmEmitMethodName(pArray,pName,apLvl[i]);
				}
			}
			SySetRelease(&aLvl);
		}
		SySetRelease(&aTmp);
	}
	/* Return the created array */
	ph7_result_value(pCtx,pArray);
	/*
	 * Don't worry about freeing memory here,everything will be relased
	 * automatically as soon we return from this foreign function.
	 */
	return PH7_OK;
}
/*
 * php's zend_get_executed_scope(): the class whose code is running, which is what every
 * visibility decision is made against — and what php NAMES in the Error when it refuses
 * ("... from scope C", or "from global scope" when this answers 0).
 *
 * Extracted from PH7_VmClassMemberAccess, which used to be the only reader; the message
 * sites hardcoded "from global scope" and so reported the wrong scope for every
 * private/protected refusal raised from inside a class.
 */
PH7_PRIVATE ph7_class * PH7_VmCallerScope(ph7_vm *pVm)
{
	VmFrame *pFrame = pVm->pFrame;
	ph7_vm_func *pVmFunc;
	while( pFrame && pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION|VM_FRAME_CATCH) ) ){
		/* Safely ignore the exception frame */
		pFrame = pFrame->pParent;
	}
	if( pFrame == 0 ){
		return 0;
	}
	pVmFunc = (ph7_vm_func *)pFrame->pUserData;
	/* The calling scope is the executing method's declaring class — OR, for a bound closure
	 * (Closure::bindTo/call), the explicit scope override carried on the frame (Increment 2). */
	if( pFrame->pBoundScope ){
		return pFrame->pBoundScope;
	}
	if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){
		return (ph7_class *)pVmFunc->pUserData;
	}
	if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLOSURE) && pVmFunc->pUserData ){
		/* A closure/arrow-fn defined inside a class carries its creation-site
		 * class in pUserData (stamped by OP_LOAD_CLOSURE via
		 * PH7_VmPeekDeclaringClass, the same scope `self::`/`parent::` resolve
		 * against inside the body). php binds that class as the closure's scope,
		 * so `$this->privateMethod()` / `self::$private` inside the closure are
		 * allowed — an explicit Closure::bindTo/bind rebind still wins above via
		 * pBoundScope. */
		return (ph7_class *)pVmFunc->pUserData;
	}
	if( pVm->pConstEvalClass ){
		/* Constant/property initializer bytecode runs without a method
		 * frame; its scope is the class being initialized (php: a private
		 * constant is reachable from its own class's initializers). */
		return pVm->pConstEvalClass;
	}
	return 0;
}
/*
 * The scope php NAMES in a visibility Error. PH7_VmCallerScope with one adjustment: php
 * flattens a TRAIT into the class that uses it, so code running in a trait method reports
 * the USING class ("from scope Base"), never the trait — and not the RECEIVER's class
 * either, so `class Kid extends Base` (Base being the one that composed the trait) still
 * reports Base. Walk the receiver's ancestry to the first class that uses this trait; the
 * trait itself stands when nothing does (nothing php would print, but better than a lie).
 *
 * Kept apart from PH7_VmCallerScope because the ACCESS decision genuinely wants the trait:
 * its private/protected branches grant on "the caller is a trait used by the target class"
 * and on the reverse, and both compare against the trait itself.
 */
PH7_PRIVATE ph7_class * PH7_VmCallerScopeName(ph7_vm *pVm)
{
	ph7_class *pScope = PH7_VmCallerScope(&(*pVm));
	VmFrame *pFrame;
	ph7_class *pWalk;
	if( pScope == 0 || (pScope->iFlags & PH7_CLASS_TRAIT) == 0 ){
		return pScope;
	}
	pFrame = pVm->pFrame;
	while( pFrame && pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION|VM_FRAME_CATCH)) ){
		pFrame = pFrame->pParent;
	}
	pWalk = (pFrame && pFrame->pThis) ? pFrame->pThis->pClass : VmCurrentSelf(&(*pVm));
	for( ; pWalk ; pWalk = pWalk->pBase ){
		ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pWalk->aTrait);
		sxu32 nTrait = SySetUsed(&pWalk->aTrait);
		sxu32 k;
		for( k = 0 ; k < nTrait ; ++k ){
			if( apTrait[k] == pScope ){
				return pWalk;
			}
		}
	}
	return pScope;
}
/*
 * The DECLARING-side twin of PH7_VmCallerScopeName: the class php NAMES as a method's
 * owner. php composes a trait INTO the class that uses it — the composed method's scope
 * IS that class — so `trait T { private function p(){} } class C { use T; }` refuses with
 * "Call to private method C::p()", from a subclass instance too, and never says T. PHL
 * keeps the trait in sFunc.pUserData (the ACCESS decision wants it there — see
 * PH7_VmClassMemberAccess's trait grants), so the noun is derived here instead: walk the
 * class the lookup went through up its ancestry to the first one that uses this trait.
 *
 * pClass is the class the method was reached through (the receiver's, or the named one).
 * A non-trait declarer is returned unchanged, which is php too: a base's private method
 * refused on a child instance names the BASE.
 */
PH7_PRIVATE ph7_class * PH7_VmMethodScopeName(ph7_vm *pVm,ph7_class *pClass,ph7_class_method *pMeth)
{
	SXUNUSED(pVm);
	return PH7_VmComposingClass(pClass,
		(pMeth && pMeth->sFunc.pUserData) ? (ph7_class *)pMeth->sFunc.pUserData : pClass);
}
/*
 * The rule itself, shared by the method side above and the ATTRIBUTE side (a trait's
 * property is composed into the using class exactly as its methods are, and
 * property_exists() asks the same "is this member's class the one I asked about"
 * question). A declarer that is not a trait is the answer; a trait resolves to the first
 * class in pClass's ancestry that uses it, and stands for itself when nothing does.
 */
PH7_PRIVATE ph7_class * PH7_VmComposingClass(ph7_class *pClass,ph7_class *pDecl)
{
	ph7_class *pWalk;
	if( pDecl == 0 || (pDecl->iFlags & PH7_CLASS_TRAIT) == 0 ){
		return pDecl;
	}
	for( pWalk = pClass ; pWalk ; pWalk = pWalk->pBase ){
		ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pWalk->aTrait);
		sxu32 nTrait = SySetUsed(&pWalk->aTrait);
		sxu32 k;
		for( k = 0 ; k < nTrait ; ++k ){
			if( apTrait[k] == pDecl ){
				return pWalk;
			}
		}
	}
	return pDecl;
}
/*
 * The name php prints for a method: the identity the class REGISTERED it under, not the
 * one in the method struct. A trait adaptation splits the two — `hi as private pHi` files
 * the method under `pHi` while the struct stays `hi` — and php, which compiles the alias
 * into a function of its own, names `pHi`. Falls back to the requested name when the class
 * holds no entry for it.
 */
PH7_PRIVATE void PH7_ClassMethodRegisteredName(ph7_class *pClass,const char *zName,sxu32 nByte,SyString *pOut)
{
	SyHashEntry *pEntry = pClass ? SyHashGet(&pClass->hMethod,(const void *)zName,nByte) : 0;
	if( pEntry ){
		SyStringInitFromBuf(pOut,(const char *)pEntry->pKey,pEntry->nKeyLen);
	}else{
		SyStringInitFromBuf(pOut,zName,nByte);
	}
}
/*
 * This function return TRUE(1) if the given class attribute stored
 * in the pAttrName parameter is visible and thus can be extracted
 * from the current scope.Otherwise FALSE is returned.
 */
PH7_PRIVATE int PH7_VmClassMemberAccess(
	ph7_vm *pVm,               /* Target VM */
	ph7_class *pClass,         /* Target Class */
	const SyString *pAttrName, /* Attribute name */
	sxi32 iProtection,         /* Attribute protection level [i.e: public,protected or private] */
	int bLog                   /* TRUE to log forbidden access. */
	)
{
	if( iProtection != PH7_CLASS_PROT_PUBLIC ){
		ph7_class *pCallerScope = PH7_VmCallerScope(&(*pVm));
		if( pCallerScope == 0 ){
			goto dis; /* Not in a class scope: access is forbidden */
		}
		if( iProtection == PH7_CLASS_PROT_PRIVATE ){
			/* php grants private access by DECLARING class: the caller's own
			 * class must declare a private attribute of this name (a base
			 * method touching its own private on a CHILD instance passes; a
			 * child method touching an inherited base-private fails). An attr
			 * whose declaring "class" is a TRAIT behaves as if declared by the
			 * adopting class. Fallbacks: the caller being a trait used by the
			 * instance's class (legacy trait-body scope), or — when the caller
			 * class carries no such attr entry at all — the legacy exact-class
			 * match (dynamic props and other non-declared shapes). */
			ph7_class *pCaller = pCallerScope;
			SyHashEntry *pOwnE = SyHashGet(&pCaller->hAttr,
				(const void *)pAttrName->zString,pAttrName->nByte);
			ph7_class_attr *pOwn = pOwnE ? (ph7_class_attr *)pOwnE->pUserData : 0;
			int bGranted = 0;
			if( pOwn && pOwn->iProtection == PH7_CLASS_PROT_PRIVATE ){
				if( pOwn->pDeclClass == 0
				 || pOwn->pDeclClass == pCaller
				 || (pOwn->pDeclClass->iFlags & PH7_CLASS_TRAIT) != 0 ){
					bGranted = 1;
				}
			}else if( pOwn == 0 && pCaller == pClass ){
				bGranted = 1;
			}
			if( !bGranted ){
				/* Check if the caller is a trait used by pClass */
				ph7_class **apTrait;
				sxu32 nTrait,k;
				apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);
				nTrait = SySetUsed(&pClass->aTrait);
				for(k = 0; k < nTrait; k++){
					if( apTrait[k] == pCaller ){
						bGranted = 1;
						break;
					}
				}
			}
			if( !bGranted && (pClass->iFlags & PH7_CLASS_TRAIT) != 0 ){
				/* The target "class" is itself a trait: a trait-copied private
				 * member behaves as if declared in the adopting class, so a
				 * caller that USES the trait gets access (php: `self::s()`
				 * from a using class's static method reaching a trait-private
				 * static — the callee resolves via the shared trait VmFunc
				 * whose owner is the trait, not the class). */
				ph7_class **apTrait;
				sxu32 nTrait,k;
				apTrait = (ph7_class **)SySetBasePtr(&pCaller->aTrait);
				nTrait = SySetUsed(&pCaller->aTrait);
				for(k = 0; k < nTrait; k++){
					if( apTrait[k] == pClass ){
						bGranted = 1;
						break;
					}
				}
			}
			if( !bGranted ){
				goto dis; /* Access is forbidden */
			}
		}else{
			/* Protected */
			ph7_class *pBase = pCallerScope;
			/* php checks the hierarchy against the class that INTRODUCES the member,
			 * not the one that (re)declares the override we resolved. A protected
			 * member declared in a common ancestor B and overridden in a child C is
			 * still reachable from a SIBLING scope S (also extending B) — S and C both
			 * descend from B. Walk pClass up to the top-most ancestor that genuinely
			 * declares a member of this name (a method via sFunc.pUserData, or an attr
			 * via pDeclClass — hMethod/hAttr also carry inherited copies, so match on the
			 * true declaring class) and test the hierarchy against that introducing
			 * class. `child_only` (declared solely in C) keeps pClass and stays denied
			 * from a sibling, matching php. */
			ph7_class *pIntro = pClass;
			ph7_class *pAnc;
			for( pAnc = pClass ; pAnc ; pAnc = pAnc->pBase ){
				ph7_class_method *pAncMeth = PH7_ClassExtractMethod(pAnc,pAttrName->zString,pAttrName->nByte);
				SyHashEntry *pAncAttrE = SyHashGet(&pAnc->hAttr,(const void *)pAttrName->zString,pAttrName->nByte);
				ph7_class_attr *pAncAttr = pAncAttrE ? (ph7_class_attr *)pAncAttrE->pUserData : 0;
				int bHere = 0;
				if( pAncMeth && (ph7_class *)pAncMeth->sFunc.pUserData == pAnc ){
					bHere = 1;
				}
				if( pAncAttr && (pAncAttr->pDeclClass == pAnc || pAncAttr->pDeclClass == 0) ){
					bHere = 1;
				}
				if( bHere ){
					pIntro = pAnc; /* keep climbing: the LAST (highest) match wins */
				}
			}
			/* Must be in the same class hierarchy as the introducing class */
			if( !PH7_VmInstanceOf(pIntro,pBase) && !PH7_VmInstanceOf(pBase,pIntro) ){
				int bTraitGrant = 0;
				if( (pClass->iFlags & PH7_CLASS_TRAIT) != 0 ){
					/* Same trait-target rule as the private branch above */
					ph7_class **apTrait;
					sxu32 nTrait,k;
					apTrait = (ph7_class **)SySetBasePtr(&pBase->aTrait);
					nTrait = SySetUsed(&pBase->aTrait);
					for(k = 0; k < nTrait; k++){
						if( apTrait[k] == pClass ){
							bTraitGrant = 1;
							break;
						}
					}
				}
				if( !bTraitGrant ){
					goto dis; /* Access is forbidden */
				}
			}
		}
	}
	return 1; /* Access is granted */
dis:
	if( bLog ){
		VmErrorFormat(&(*pVm),PH7_CTX_ERR,
			"Access to the class attribute '%z->%z' is forbidden",
			&pClass->sName,pAttrName);
	}
	return 0; /* Access is forbidden */
}
/*
 * array get_class_vars(string/object $class_name)
 *   Get the default properties of the class
 * Parameters
 *  class_name
 *   The class name or class instance
 * Return
 *  Returns an associative array of declared properties visible from the current scope
 *  with their default value. The resulting array elements are in the form
 *  of varname => value.
 * Note:
 *   NULL is returned on failure.
 */
PH7_PRIVATE int vm_builtin_get_class_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pName,*pArray,sValue;
	SyHashEntry *pEntry;
	ph7_class *pClass;
	/* Extract the target class first */
	pClass = 0;
	if( nArg > 0 ){
		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);
	}
	if( pClass == 0 ){
		/* php screens the VALUE, not the type: anything stringifiable is accepted,
		 * and a name that does not resolve to a class is a TypeError quoting the
		 * stringified argument ("...must be a valid class name, Array given"). This
		 * is why get_class_vars() opts out of the shared ZPP type screen in vm.c. */
		int nLen = 0;
		const char *zVal = "";
		if( nArg > 0 ){
			if( (apArg[0]->iFlags & MEMOBJ_HASHMAP) != 0 ){
				zVal = "Array";
				nLen = (int)sizeof("Array") - 1;
			}else{
				zVal = ph7_value_to_string(apArg[0],&nLen);
			}
		}
		return PH7_VmThrowException(pCtx,"TypeError",
			"get_class_vars(): Argument #1 ($class) must be a valid class name, %.*s given",
			nLen,zVal);
	}
	if( VmClassStaticDeferPending(pClass) ){
		/* Listing the properties reads every static slot, which materializes the
		 * class's static table: a default that threw at the declaration raises
		 * here, as it does in php. */
		sxi32 rcMat = PH7_VmMaterializeClassStatics(pCtx->pVm,pClass);
		if( rcMat != SXRET_OK ){
			return rcMat;
		}
	}
	/* Create a new array  */
	pArray = ph7_context_new_array(pCtx);
	pName = ph7_context_new_scalar(pCtx);
	PH7_MemObjInit(pCtx->pVm,&sValue);
	if( pArray == 0 || pName == 0){
		/* Out of memory,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Fill the array with the defined attribute visible from the current scope */
	SyHashResetLoopCursor(&pClass->hAttr);
	while((pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){
		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;
		if( pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){
			/* php 8.4: VIRTUAL hooked properties have no backing store —
			 * get_class_vars() excludes them (raw surface) */
			continue;
		}
		/* Check if the access is allowed */
		if( PH7_VmClassMemberAccess(pCtx->pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){
			SyString *pAttrName = &pAttr->sName;
			ph7_value *pValue = 0;
			if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT|PH7_CLASS_ATTR_STATIC) ){
				/* Static slots are computed at mount; constants lazily */
				PH7_VmMaterializeClassConst(pCtx->pVm,pClass,pAttr);
				pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pAttr->nIdx);
			}else{
				if( SySetUsed(&pAttr->aByteCode) > 0 ){
					PH7_MemObjRelease(&sValue);
					/* Compute default value (any complex expression) associated with this attribute */
					VmLocalExec(pCtx->pVm,&pAttr->aByteCode,&sValue,FALSE);
					pValue = &sValue;
				}
			}
			/* Fill in the array */
			ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);
			ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */
			/* Reset the cursor */
			ph7_value_reset_string_cursor(pName);
		}
	}
	PH7_MemObjRelease(&sValue);
	/* Return the created array */
	ph7_result_value(pCtx,pArray);
	/*
	 * Don't worry about freeing memory here,everything will be relased
	 * automatically as soon we return from this foreign function.
	 */
	return PH7_OK;
}
/*
 * array get_object_vars(object $this)
 *   Gets the properties of the given object
 * Parameters
 *  this
 *   A class instance
 * Return
 *  Returns an associative array of defined object accessible non-static properties
 *  for the specified object in scope. If a property have not been assigned a value
 *  it will be returned with a NULL value.
 * Note:
 *   NULL is returned on failure.
 */
PH7_PRIVATE int vm_builtin_get_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = 0;
	ph7_value *pName,*pArray;
	SyHashEntry *pEntry;
	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){
		/* Extract the target instance */
		pThis = (ph7_class_instance *)apArg[0]->x.pOther;
	}
	if( pThis == 0 ){
		/* No such instance,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Create a new array  */
	pArray = ph7_context_new_array(pCtx);
	pName = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pName == 0){
		/* Out of memory,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Fill the array with the defined attribute visible from the current scope.
	 * SNAPSHOT the attribute names first: a PHP 8.4 get hook dispatched mid-walk
	 * runs user code that may re-enter an hAttr walk on this instance (resetting
	 * the hash's single embedded loop cursor) or unset()/create properties. The
	 * names point into CLASS-owned attr storage (they outlive instance mutation);
	 * each is re-looked-up before use so an entry unset by an earlier hook is
	 * skipped instead of read after free. */
	{
		SySet sNames;
		SyString *aName;
		sxu32 iName,nName;
		SySetInit(&sNames,&pCtx->pVm->sAllocator,sizeof(SyString));
		SyHashResetLoopCursor(&pThis->hAttr);
		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){
			VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;
			if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT|PH7_CLASS_ATTR_HIDDEN) ){
				/* Only non-static/constant attributes are extracted */
				continue;
			}
			if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET|PH7_CLASS_ATTR_HOOK_VIRTUAL))
			 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){
				continue; /* virtual set-only property: no value to expose (php) */
			}
			SySetPut(&sNames,(const void *)&pVmAttr->pAttr->sName);
		}
		aName = (SyString *)SySetBasePtr(&sNames);
		nName = SySetUsed(&sNames);
		for( iName = 0 ; iName < nName ; ++iName ){
			SyString *pAttrName = &aName[iName];
			VmClassAttr *pVmAttr;
			pEntry = SyHashGet(&pThis->hAttr,(const void *)pAttrName->zString,pAttrName->nByte);
			if( pEntry == 0 ){
				continue; /* unset by an earlier hook */
			}
			pVmAttr = (VmClassAttr *)pEntry->pUserData;
			/* Check if the access is allowed */
			if( PH7_VmClassMemberAccess(pCtx->pVm,pThis->pClass,pAttrName,pVmAttr->pAttr->iProtection,FALSE) ){
				ph7_value *pValue = 0;
				ph7_value sHookVal;
				sxi32 rcHk;
				/* PHP 8.4 property hooks: get_object_vars() reads through the get
				 * hook (virtual properties included); raw slot otherwise. */
				PH7_MemObjInit(pCtx->pVm,&sHookVal);
				rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);
				if( rcHk == SXRET_OK ){
					pValue = &sHookVal;
				}else if( rcHk == SXERR_NOTFOUND ){
					/* Extract attribute */
					pValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);
				}else{
					/* the hook threw — parked on the boundary rail; php aborts the
					 * whole builtin at the first throw (the helper's boundary gate
					 * keeps LATER hooks from running; raw values it falls back to
					 * are discarded when the throw routes) */
					PH7_MemObjRelease(&sHookVal);
					break;
				}
				if( pValue ){
					/* Insert attribute name in the array */
					ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);
					ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */
				}
				PH7_MemObjRelease(&sHookVal);
				/* Reset the cursor */
				ph7_value_reset_string_cursor(pName);
			}
		}
		SySetRelease(&sNames);
	}
	/* Return the created array */
	ph7_result_value(pCtx,pArray);
	/*
	 * Don't worry about freeing memory here,everything will be relased
	 * automatically as soon we return from this foreign function.
	 */
	return PH7_OK;
}
/*
 * array get_mangled_object_vars(object $object)
 *  The object's own property table, with php's visibility MANGLING left on the keys:
 *  a protected `p` is "\0*\0p" and a private one "\0Declaring\0p".
 *
 *  It is get_object_vars()'s opposite in both of that function's decisions — no
 *  visibility screen (every property is reported, from every level of the chain) and
 *  no property HOOK (a `get` is not dispatched; the backing slot is what is reported,
 *  and a VIRTUAL hooked property, having no slot, is not reported at all). It is not
 *  the (array) cast either: the cast asks a native class's own handler, so
 *  `(array) new ArrayObject([1,2])` is `[1,2]` where this answers the EMPTY table.
 */
PH7_PRIVATE int vm_builtin_get_mangled_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = 0;
	ph7_value *pArray;
	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){
		/* Extract the target instance */
		pThis = (ph7_class_instance *)apArg[0]->x.pOther;
	}
	if( pThis == 0 ){
		/* The `object $object` signature row refuses everything else before we run */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		/* Out of memory,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	PH7_ClassInstanceToHashmapRaw(pThis,(ph7_hashmap *)pArray->x.pOther);
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/* Bound on `extends` chain depth — matches PH7_THROWABLE_WALK_MAX_DEPTH in
 * compile.c. Defends against compiler cycles even though interface cycle
 * detection should reject them up front. */
#define PH7_INTERFACE_WALK_MAX_DEPTH 64
/*
 * TRUE if pTarget is reachable from pIface as an ancestor interface: walk the
 * `extends` chain (pBase) AND each additional parent-interface set (aInterface),
 * so a multiple-interface `interface C extends A, B` is recognized through BOTH
 * A and B (php allows an interface to extend several interfaces). Recursion is
 * depth-bounded — a malformed cycle cannot run unbounded.
 */
static int VmInterfaceReaches(ph7_class *pIface,ph7_class *pTarget,int iDepth)
{
	while( pIface && iDepth <= PH7_INTERFACE_WALK_MAX_DEPTH ){
		ph7_class **apParent;
		sxu32 n;
		if( pIface == pTarget ){
			return TRUE;
		}
		/* Additional parent interfaces (interface X extends A, B, …) live in
		 * aInterface; the first parent stays on the pBase chain below. */
		apParent = (ph7_class **)SySetBasePtr(&pIface->aInterface);
		for( n = 0 ; n < SySetUsed(&pIface->aInterface) ; n++ ){
			if( VmInterfaceReaches(apParent[n],pTarget,iDepth+1) ){
				return TRUE;
			}
		}
		pIface = pIface->pBase;
		iDepth++;
	}
	return FALSE;
}
/*
 * This function returns TRUE if the given class is an implemented
 * interface.Otherwise FALSE is returned.
 */
static int VmQueryInterfaceSet(ph7_class *pClass,SySet *pSet)
{
	ph7_class **apInterface;
	sxu32 n;
	if( SySetUsed(pSet) < 1 ){
		/* Empty interface container */
		return FALSE;
	}
	/* Point to the set of implemented interfaces */
	apInterface = (ph7_class **)SySetBasePtr(pSet);
	/* Perform the lookup, walking each interface's parent chain so that
	 * Iterator extends Traversable (and similar) is recognized. */
	for( n = 0 ; n < SySetUsed(pSet) ; n++ ){
		if( VmInterfaceReaches(apInterface[n],pClass,0) ){
			return TRUE;
		}
	}
	return FALSE;
}
/*
 * This function returns TRUE if the given class (first argument)
 * is an instance of the main class (second argument).
 * Otherwise FALSE is returned.
 */
PH7_PRIVATE int PH7_VmInstanceOf(ph7_class *pThis,ph7_class *pClass)
{
	ph7_class *pParent;
	sxi32 rc;
	if( pThis == pClass ){
		/* Instance of the same class */
		return TRUE;
	}
	/* Check implemented interfaces */
	rc = VmQueryInterfaceSet(pClass,&pThis->aInterface);
	if( rc ){
		return TRUE;
	}
	/* Check parent classes */
	pParent = pThis->pBase;
	while( pParent ){
		if( pParent == pClass ){
			/* Same instance */
			return TRUE;
		}
		/* Check the implemented interfaces */
		rc = VmQueryInterfaceSet(pClass,&pParent->aInterface);
		if( rc ){
			return TRUE;
		}
		/* Point to the parent class */
		pParent = pParent->pBase;
	}
	/* Not an instance of the the given class */
	return FALSE;
}
/*
 * This function returns TRUE if the given class (first argument)
 * is a subclass of the main class (second argument).
 * Otherwise FALSE is returned.
 */
static int VmSubclassOf(ph7_class *pClass,ph7_class *pBase)
{
	SyHashEntry *pEntry;
	SyString *pName;
	while( pClass ){
		pName = &pClass->sName;
		/* Query the derived hashtable for a class-hierarchy match */
		pEntry = SyHashGet(&pBase->hDerived,(const void *)pName->zString,pName->nByte);
		if( pEntry ){
			return TRUE;
		}
		/* Query the interfaces implemented by THIS class in the chain — so an
		 * interface implemented by a PARENT (B extends A implements I) is found,
		 * mirroring PH7_VmInstanceOf. The original code queried only the first
		 * class's aInterface, missing inherited interfaces. */
		if( VmQueryInterfaceSet(pBase,&pClass->aInterface) ){
			return TRUE;
		}
		pClass = pClass->pBase;
	}
	/* Not a subclass */
	return FALSE;
}
/*
 * bool is_a(object|string $object_or_class,string $class,bool $allow_string = false)
 *   Checks if the object/class is of this class or has this class (or interface)
 *   as one of its parents.
 * Parameters
 *  object_or_class
 *   The tested object, or a class name string (only honored when allow_string is TRUE).
 * class
 *  The class or interface name to test against.
 * allow_string
 *  When TRUE, a string first argument is resolved as a class name; php's default
 *  is FALSE, so is_a() returns FALSE for a string first argument without it. The
 *  flag is IGNORED for an object first argument (php).
 * Return
 *   Returns TRUE if object_or_class is of class class or has class as one of its
 *   parents (or implements it as an interface), FALSE otherwise.
 */
PH7_PRIVATE int vm_builtin_is_a(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume FALSE by default */
	if( nArg > 1 ){
		ph7_class *pThisClass = 0;
		if( ph7_value_is_object(apArg[0]) ){
			/* An object first argument: allow_string is ignored (php). */
			pThisClass = ((ph7_class_instance *)apArg[0]->x.pOther)->pClass;
		}else if( ph7_value_is_string(apArg[0]) && nArg > 2 && ph7_value_to_bool(apArg[2]) ){
			/* A string first argument is resolved to a class ONLY when allow_string
			 * (the 3rd argument) is TRUE — valid php since 5.3.9, and a sibling of
			 * is_subclass_of()'s string form. Autoloads on a miss via
			 * PH7_VmExtractClassFromValue; a leading '\' is anchored there. */
			pThisClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);
		}
		if( pThisClass ){
			/* Extract the given class */
			ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);
			if( pClass ){
				/* Perform the query — instanceof, so the class ITSELF matches
				 * (unlike is_subclass_of, which excludes self). */
				res = PH7_VmInstanceOf(pThisClass,pClass);
			}
		}
	}
	/* Query result */
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * int spl_object_id(object $object)
 *  Return the integer object handle (per-instance id) of the given object.
 * PHL note: PHP 8 throws a TypeError when passed a non-object; PHL returns NULL
 * to stay consistent with the engine's graceful-degradation convention.
 */
PH7_PRIVATE int vm_builtin_spl_object_id(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis;
	if( nArg < 1 || !ph7_value_is_object(apArg[0]) ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pThis = (ph7_class_instance *)apArg[0]->x.pOther;
	ph7_result_int64(pCtx,(ph7_int64)pThis->nObjId);
	return PH7_OK;
}
/*
 * object clone(object $object, array $withProperties = [])
 *  php 8.5's clone-with: `clone` is a real internal function there, so every
 *  indirect spelling reaches it — `clone(...)` as a first-class callable,
 *  `$f = 'clone'; $f($o)`, `call_user_func('clone', $o)` — and the arity/type
 *  refusals are the ordinary runtime ones, not a compile error. The direct
 *  `clone($o, [...])` source form compiles to a CALL of this function, and the
 *  `clone $o` OPERATOR keeps its own opcode.
 *
 *  The property updates are applied AFTER __clone(), each as a scope-aware write
 *  (visibility, readonly re-init, typed coercion) — VmCloneApplyUpdate, shared
 *  with nothing else now. A host function runs on the CALLER's frame, so the
 *  scope those writes are judged against is php's: the scope that called clone().
 */
PH7_PRIVATE int vm_builtin_clone(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pSrc,*pClone;
	char zGiven[64];
	/* Arity and the two parameter types are the aBuiltinSig[] row's; a value that
	 * reaches here is an object (arg #1) and, if given, an array (arg #2). */
	if( nArg < 1 || !ph7_value_is_object(apArg[0]) ){
		return PH7_VmThrowException(pCtx,"TypeError",
			"clone(): Argument #1 ($object) must be of type object, %s given",
			nArg > 0 ? VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)) : "no value");
	}
	pSrc = (ph7_class_instance *)apArg[0]->x.pOther;
	/* The uncloneable classes, same rule and wording as the operator: an enum case
	 * (the singleton identity would break), a class whose instances own a C-side
	 * resource, and Generator/Fiber. */
	if( (pSrc->pClass->iFlags & (PH7_CLASS_ENUM|PH7_CLASS_NOCLONE))
		|| pSrc->pClass == pVm->pGeneratorClass || pSrc->pClass == pVm->pFiberClass ){
		return PH7_VmThrowException(pCtx,"Error",
			"Trying to clone an uncloneable object of class %z",&pSrc->pClass->sName);
	}
	pClone = PH7_CloneClassInstance(pSrc);
	if( pClone == 0 ){
		return PH7_VmMemoryError(pVm);
	}
	/* Hand the clone to the caller BEFORE the updates run: an update that throws
	 * leaves the object owned by the return slot, which releases it. */
	PH7_MemObjRelease(pCtx->pRet);
	pCtx->pRet->x.pOther = pClone;
	MemObjSetType(pCtx->pRet,MEMOBJ_OBJ);
	if( nArg > 1 && ph7_value_is_array(apArg[1]) ){
		ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;
		ph7_hashmap_node *pNode = pMap->pFirst;
		sxu32 n;
		for( n = pMap->nEntry ; n > 0 && pNode ; --n ){
			ph7_value *pVal,sVal;
			const char *zName;
			sxu32 nName;
			char zKeyBuf[64];
			sxi32 rc;
			if( pNode->iType == HASHMAP_INT_NODE ){
				/* An int key becomes the property name (php: `$5`). */
				nName = SyBufferFormat(zKeyBuf,sizeof(zKeyBuf),"%qd",pNode->xKey.iKey);
				zName = zKeyBuf;
			}else{
				zName = (const char *)SyBlobData(&pNode->xKey.sKey);
				nName = SyBlobLength(&pNode->xKey.sKey);
			}
			pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);
			if( pVal ){
				/* Snapshot the update value first: applying it may create a dynamic
				 * property, whose slot reservation can reallocate pVm->aMemObj and
				 * dangle pVal (a pointer into it). */
				PH7_MemObjInit(pVm,&sVal);
				PH7_MemObjLoad(pVal,&sVal);
				rc = VmCloneApplyUpdate(pVm,pClone,zName,nName,&sVal);
				PH7_MemObjRelease(&sVal);
				if( rc != SXRET_OK ){
					return rc;
				}
			}
			pNode = pNode->pPrev; /* pFirst -> pPrev is the forward link */
		}
	}
	return PH7_OK;
}
/*
 * string spl_object_hash(object $object)
 *  Return a 32-char hex identifier, unique and stable per live object.
 * PHL note: PHP derives this from the internal handle plus a per-process key, so
 * the exact value is NOT reproducible. PHL returns the zero-padded object id,
 * which preserves the only guaranteed properties: unique per live object, stable
 * across calls, and distinct objects -> distinct strings. A non-object returns
 * NULL (PHP 8 throws a TypeError; see spl_object_id above).
 */
PH7_PRIVATE int vm_builtin_spl_object_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis;
	if( nArg < 1 || !ph7_value_is_object(apArg[0]) ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pThis = (ph7_class_instance *)apArg[0]->x.pOther;
	ph7_result_string_format(pCtx,"%08x%08x%08x%08x",0,0,0,(unsigned int)pThis->nObjId);
	return PH7_OK;
}
/*
 * bool is_subclass_of(object|string $object_or_class,string $class,bool $allow_string = true)
 *   Checks if the object/class has this class (or interface) as one of its
 *   parents — the subclass relation, which EXCLUDES the class itself.
 * Parameters
 *  object_or_class
 *   The tested object, or a class name string (honored unless allow_string is FALSE).
 * class
 *  The class or interface name to test against.
 * allow_string
 *  When FALSE, a string first argument is NOT resolved and the call returns FALSE.
 *  php's default here is TRUE (unlike is_a's FALSE). The flag is IGNORED for an
 *  object first argument (php).
 * Return
 *  Returns TRUE if object_or_class is a proper subclass of class (or implements it
 *  as an interface, directly or via a parent), FALSE otherwise.
 */
PH7_PRIVATE int vm_builtin_is_subclass_of(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume FALSE by default */
	if( nArg > 1 ){
		ph7_class *pClass = 0;
		if( ph7_value_is_object(apArg[0]) ){
			/* An object first argument: allow_string is ignored (php). */
			pClass = ((ph7_class_instance *)apArg[0]->x.pOther)->pClass;
		}else if( ph7_value_is_string(apArg[0]) && (nArg < 3 || ph7_value_to_bool(apArg[2])) ){
			/* A string first argument is resolved as a class name UNLESS allow_string
			 * (the 3rd argument) is explicitly FALSE — php's default here is TRUE.
			 * Autoloads on a miss via PH7_VmExtractClassFromValue; a leading '\' is
			 * anchored there. Sibling of is_a()'s string form (which defaults OFF). */
			pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);
		}
		if( pClass ){
			/* Extract the target class */
			ph7_class *pMain = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);
			if( pMain ){
				/* Perform the query — subclass-only (excludes self, unlike is_a). */
				res = VmSubclassOf(pClass,pMain);
			}
		}
	}
	/* Query result */
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
PH7_PRIVATE int vm_builtin_call_user_func(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value sResult; /* Store callback return value here */
	sxi32 rc;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	{
		/* php validates the callback BEFORE calling anything; the dispatcher below would
		 * otherwise answer NULL in silence for an unresolvable one. */
		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",0);
		if( rcCb != PH7_OK ){
			return rcCb;
		}
	}
	PH7_MemObjInit(pCtx->pVm,&sResult);
	sResult.nIdx = SXU32_HIGH; /* Mark as constant */
	/* php passes call_user_func()'s arguments BY VALUE: warn on a by-ref formal and copy */
	PH7_VmCufDropByRefArgs(pCtx,apArg[0],nArg - 1,&apArg[1]);
	/* Try to invoke the callback. If the call_user_func() call site used
	 * name: arguments (e.g. call_user_func('f', b: 9)), forward them to the
	 * callback. The inner call's argument i is the outer argument i+1 (outer
	 * argument 0 is the callback), so the inner name array is simply the outer
	 * names shifted by one — no copy needed: VmResolveNamedArgs treats any index
	 * >= nTotal as positional, so a shorter map covers the callback's args. */
	if( pCtx->pArgMap && pCtx->pArgMap->bHasNamed && nArg > 1 ){
		VmCallArgMap *pOuter = pCtx->pArgMap;
		VmCallArgMap sInner;
		/* Zero first: a field added to the map (sAssertSrc, ...) must read as
		 * unset when forwarded, not as stack garbage. */
		SyZero(&sInner,sizeof(sInner));
		sInner.bHasNamed = 1;
		sInner.bIsNamespaced = 0;
		/* Named args to call_user_func coerce in WEAK mode even from a
		 * strict_types=1 caller (verified vs php 8.5.7): a name: argument
		 * collected into the variadic and re-spread loses the strict context.
		 * call_user_func_array does NOT share this quirk (it stays strict). */
		sInner.bStrict = 0;
		sInner.nTotal = pOuter->nTotal > 1 ? pOuter->nTotal - 1 : 0;
		sInner.aNames = sInner.nTotal > 0 ? &pOuter->aNames[1] : 0;
		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult,&sInner);
	}else{
		/* call_user_func is one of php's two FORWARDS: the callback binds under the
		 * mode of the file that wrote the call_user_func, not weakly like every other
		 * internal callback. Carry that one bit on a map of its own — the positional
		 * wrapper would latch the call weak (which is right for array_map and every
		 * other internal invocation, and wrong here). */
		VmCallArgMap sFwd;
		SyZero(&sFwd,sizeof(sFwd));
		sFwd.bStrict = (pCtx->pArgMap && pCtx->pArgMap->bStrict) ? 1 : 0;
		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult,&sFwd);
	}
	if( rc == PH7_EXCEPTION ){
		/* The callback raised: propagate so the OP_CALL dispatcher unwinds
		 * through the nearest try/catch instead of returning FALSE. */
		PH7_MemObjRelease(&sResult);
		return PH7_EXCEPTION;
	}
	if( rc != SXRET_OK ){
		/* An error occured while invoking the given callback [i.e: not defined] */
		ph7_result_bool(pCtx,0); /* return false */
	}else{
		/* Callback result */
		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */
	}
	PH7_MemObjRelease(&sResult);
	return PH7_OK;
}
/*
 * value call_user_func_array(callable $callback,array $param_arr)
 *  Call a callback with an array of parameters.
 * Parameter
 *  $callback
 *   The callable to be called.
 * $param_arr
 *  The parameters to be passed to the callback, as an indexed array.
 * Return
 *  Returns the return value of the callback, or FALSE on error.
 */
PH7_PRIVATE int vm_builtin_call_user_func_array(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pEntry; /* Current hashmap entry */
	ph7_value *pValue,sResult;/* Store callback return value here */
	ph7_hashmap *pMap;        /* Target hashmap */
	SySet aArg;               /* Argument value pointers */
	SyString *aNames = 0;     /* Name map, lazily allocated when a string key appears */
	ph7_hashmap_node **apNode = 0; /* Per-position node: is this element a REFERENCE? */
	sxu32 nSlot = 0;          /* Number of collected arguments */
	sxi32 rc;
	sxu32 n;
	if( nArg < 2 || !ph7_value_is_array(apArg[1]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	{
		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",0);
		if( rcCb != PH7_OK ){
			return rcCb;
		}
	}
	PH7_MemObjInit(pCtx->pVm,&sResult);
	sResult.nIdx = SXU32_HIGH; /* Mark as constant */
	/* Initialize the arguments container */
	SySetInit(&aArg,&pCtx->pVm->sAllocator,sizeof(ph7_value *));
	/* Turn hashmap entries into callback arguments. A string key becomes a
	 * named argument (PHP 8: call_user_func_array($cb, ['b' => 9])), an integer
	 * key stays positional. The name map points straight at each node's key
	 * blob: the source array stays pinned on the operand stack for the whole
	 * call, so the blobs outlive argument binding. A pure list array (no string
	 * keys) never allocates aNames and takes the plain positional path. */
	pMap = (ph7_hashmap *)apArg[1]->x.pOther;
	pEntry = pMap->pFirst; /* First inserted entry */
	for( n = 0 ; n < pMap->nEntry ; n++ ){
		/* Extract node value */
		if( (pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx)) != 0 ){
			if( pEntry->iType == HASHMAP_BLOB_NODE ){
				if( aNames == 0 ){
					/* First string key: allocate the whole map, zeroed so every
					 * not-yet-seen slot defaults to positional. */
					aNames = (SyString *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,pMap->nEntry * sizeof(SyString));
					if( aNames == 0 ){
						SySetRelease(&aArg);
						PH7_MemObjRelease(&sResult);
						if( apNode ){
							SyMemBackendFree(&pCtx->pVm->sAllocator,apNode);
						}
						return PH7_ContextMemoryError(pCtx);
					}
					SyZero(aNames,pMap->nEntry * sizeof(SyString));
				}
				SyStringInitFromBuf(&aNames[nSlot],SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));
			}
			if( apNode == 0 ){
				apNode = (ph7_hashmap_node **)SyMemBackendAlloc(&pCtx->pVm->sAllocator,
					pMap->nEntry * sizeof(ph7_hashmap_node *));
				if( apNode ){
					SyZero(apNode,pMap->nEntry * sizeof(ph7_hashmap_node *));
				}
			}
			if( apNode ){
				apNode[nSlot] = pEntry;
			}
			SySetPut(&aArg,(const void *)&pValue);
			nSlot++;
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
	}
	/* php honours a by-REFERENCE parameter here only when the argument-array ELEMENT is
		 * itself a reference; a plain element is copied, and php says so. The values were
		 * already php-exact (the callee aliases the array's own element) — the diagnostic
		 * was the whole gap. Raised before the invoke, which is where php raises it. */
	if( apNode ){
		PH7_VmWarnByRefArgsGivenValue(pCtx->pVm,apArg[0],(int)nSlot,apNode,aNames);
		SyMemBackendFree(&pCtx->pVm->sAllocator,apNode);
		apNode = 0;
	}
	/* Try to invoke the callback */
	if( aNames ){
		VmCallArgMap sMap;
		SyZero(&sMap,sizeof(sMap)); /* new map fields must read unset, not stack garbage */
		sMap.bHasNamed = 1;
		sMap.bIsNamespaced = 0;
		/* Coercion strictness follows the caller's file; the OP_CALL dispatcher
		 * forwards the call site's map on pArgMap (0 only at non-OP_CALL sites). */
		sMap.bStrict = (pCtx->pArgMap ? pCtx->pArgMap->bStrict : 0);
		sMap.nTotal = nSlot;
		sMap.aNames = aNames;
		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],(int)nSlot,
			(ph7_value **)SySetBasePtr(&aArg),&sResult,&sMap);
		SyMemBackendFree(&pCtx->pVm->sAllocator,aNames);
	}else{
		/* The other FORWARD: same rule as call_user_func above — the caller's file
		 * mode reaches the callback, where every other internal invocation is weak. */
		VmCallArgMap sFwd;
		SyZero(&sFwd,sizeof(sFwd));
		sFwd.bStrict = (pCtx->pArgMap && pCtx->pArgMap->bStrict) ? 1 : 0;
		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],(int)nSlot,
			(ph7_value **)SySetBasePtr(&aArg),&sResult,&sFwd);
	}
	if( rc == PH7_EXCEPTION ){
		/* The callback raised: propagate so the OP_CALL dispatcher unwinds. */
		PH7_MemObjRelease(&sResult);
		SySetRelease(&aArg);
		return PH7_EXCEPTION;
	}
	if( rc != SXRET_OK ){
		/* An error occured while invoking the given callback [i.e: not defined] */
		ph7_result_bool(pCtx,0); /* return false */
	}else{
		/* Callback result */
		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */
	}
	/* Cleanup the mess left behind */
	PH7_MemObjRelease(&sResult);
	SySetRelease(&aArg);
	return PH7_OK;
}
