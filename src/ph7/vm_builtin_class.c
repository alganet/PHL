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
		if( nLen > 0 ){
			SyHashEntry *pEntry;
			/* Perform a lookup */
			pEntry = SyHashGet(&pVm->hClass,(const void *)zClass,(sxu32)nLen);
			if( pEntry ){
				/* Point to the desired class */
				pClass = (ph7_class *)pEntry->pUserData;
			}
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
		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);
		if( pClass ){
			const char *zName;
			int nLen;
			/* Extract attribute name */
			zName = ph7_value_to_string(apArg[1],&nLen);
			if( nLen > 0 ){
				/* Perform the lookup in the attribute and method table */
				if( SyHashGet(&pClass->hAttr,(const void *)zName,(sxu32)nLen) != 0
					|| SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen) != 0 ){
						/* property exists,flag that */
						res = 1;
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
		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);
		if( pClass ){
			const char *zName;
			int nLen;
			/* Extract method name */
			zName = ph7_value_to_string(apArg[1],&nLen);
			if( nLen > 0 ){
				/* Perform the lookup in the method table */
				if( SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen) != 0 ){
					/* method exists,flag that */
					res = 1;
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
		/* Extract given name */
		zName = ph7_value_to_string(apArg[0],&nLen);
		if( nArg >= 2 ){
			iAutoload = ph7_value_to_bool(apArg[1]);
		}
		if( nLen > 0 ){
			/* Perform a hash lookup first */
			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);
		}
		if( pEntry == 0 && nLen > 0 && iAutoload ){
			/* Try autoload, then re-check */
			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,(sxu32)nLen,FALSE);
			if( pClass ){
				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);
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
		/* Extract given name */
		zName = ph7_value_to_string(apArg[0],&nLen);
		if( nArg >= 2 ){
			iAutoload = ph7_value_to_bool(apArg[1]);
		}
		/* Perform a hash lookup */
		if( nLen > 0 ){
			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);
		}
		if( pEntry == 0 && nLen > 0 && iAutoload ){
			/* Try autoload — pass iLoadable=FALSE so we get interfaces too */
			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,(sxu32)nLen,FALSE);
			if( pClass ){
				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);
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
	if( nNewLen < 1 ){
		/* Invalid alias name,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform a hash lookup */
	pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zOld,(sxu32)nOldLen);
	if( pEntry ==  0 ){
		/* No such class,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the class */
	pClass = (ph7_class *)pEntry->pUserData;
	/* Duplicate alias name */
	zDup = SyMemBackendStrDup(&pCtx->pVm->sAllocator,zNew,(sxu32)nNewLen);
	if( zDup == 0 ){
		/* Out of memory,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Create the alias */
	rc = SyHashInsert(&pCtx->pVm->hClass,(const void *)zDup,(sxu32)nNewLen,pClass);
	if( rc != SXRET_OK ){
		SyMemBackendFree(&pCtx->pVm->sAllocator,zDup);
	}
	ph7_result_bool(pCtx,rc == SXRET_OK);
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
	ph7_value *pName,*pArray;
	SyHashEntry *pEntry;
	/* Create a new array first */
	pArray = ph7_context_new_array(pCtx);
	pName = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pName == 0){
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		/* Out of memory,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Fill the array with the defined classes */
	SyHashResetLoopCursor(&pCtx->pVm->hClass);
	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){
		ph7_class *pClass = (ph7_class *)pEntry->pUserData;
		/* Do not register classes defined as interfaces */
		if( (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){
			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));
			/* insert class name */
			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */
			/* Reset the cursor */
			ph7_value_reset_string_cursor(pName);
		}
	}
	/* Return the created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
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
	ph7_value *pName,*pArray;
	SyHashEntry *pEntry;
	/* Create a new array first */
	pArray = ph7_context_new_array(pCtx);
	pName = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pName == 0 ){
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		/* Out of memory,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Fill the array with the defined classes */
	SyHashResetLoopCursor(&pCtx->pVm->hClass);
	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){
		ph7_class *pClass = (ph7_class *)pEntry->pUserData;
		/* Register classes defined as interfaces only */
		if( pClass->iFlags & PH7_CLASS_INTERFACE ){
			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));
			/* insert interface name */
			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */
			/* Reset the cursor */
			ph7_value_reset_string_cursor(pName);
		}
	}
	/* Return the created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array get_class_methods(string/object $class_name)
 *   Returns an array with the name of the class methods
 * Parameters
 *  class_name
 *  The class name or class instance
 * Return
 *  Returns an array of method names defined for the class specified by class_name.
 *  In case of an error, it returns NULL.
 * Note:
 *   NULL is returned on failure.
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
		/* No such class,return NULL */
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
	/* Fill the array with the defined methods */
	SyHashResetLoopCursor(&pClass->hMethod);
	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){
		ph7_class_method *pMethod = (ph7_class_method *)pEntry->pUserData;
		/* Insert method name */
		ph7_value_string(pName,SyStringData(&pMethod->sFunc.sName),(int)SyStringLength(&pMethod->sFunc.sName));
		ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */
		/* Reset the cursor */
		ph7_value_reset_string_cursor(pName);
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
		VmFrame *pFrame = pVm->pFrame;
		ph7_vm_func *pVmFunc;
		while( pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION|VM_FRAME_CATCH) ) ){
			/* Safely ignore the exception frame */
			pFrame = pFrame->pParent;
		}
		pVmFunc = (ph7_vm_func *)pFrame->pUserData;
		if( pVmFunc == 0 || (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){
			goto dis; /* Access is forbidden */
		}
		if( iProtection == PH7_CLASS_PROT_PRIVATE ){
			/* Must be the same instance or a trait used by the class */
			ph7_class *pCaller = (ph7_class *)pVmFunc->pUserData;
			if( pCaller != pClass ){
				/* Check if the caller is a trait used by pClass */
				ph7_class **apTrait;
				sxu32 nTrait,k;
				int iFound = 0;
				apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);
				nTrait = SySetUsed(&pClass->aTrait);
				for(k = 0; k < nTrait; k++){
					if( apTrait[k] == pCaller ){
						iFound = 1;
						break;
					}
				}
				if( !iFound ){
					goto dis; /* Access is forbidden */
				}
			}
		}else{
			/* Protected */
			ph7_class *pBase = (ph7_class *)pVmFunc->pUserData;
			/* Must be in the same class hierarchy */
			if( !PH7_VmInstanceOf(pClass,pBase) && !PH7_VmInstanceOf(pBase,pClass) ){
				goto dis; /* Access is forbidden */
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
		/* No such class,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
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
		/* Check if the access is allowed */
		if( PH7_VmClassMemberAccess(pCtx->pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){
			SyString *pAttrName = &pAttr->sName;
			ph7_value *pValue = 0;
			if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT|PH7_CLASS_ATTR_STATIC) ){
				/* Extract static attribute value which is always computed */
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
	/* Fill the array with the defined attribute visible from the current scope */
	SyHashResetLoopCursor(&pThis->hAttr);
	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){
		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;
		SyString *pAttrName;
		if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT) ){
			/* Only non-static/constant attributes are extracted */
			continue;
		}
		pAttrName = &pVmAttr->pAttr->sName;
		/* Check if the access is allowed */
		if( PH7_VmClassMemberAccess(pCtx->pVm,pThis->pClass,pAttrName,pVmAttr->pAttr->iProtection,FALSE) ){
			ph7_value *pValue = 0;
			/* Extract attribute */
			pValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);
			if( pValue ){
				/* Insert attribute name in the array */
				ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);
				ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */
			}
			/* Reset the cursor */
			ph7_value_reset_string_cursor(pName);
		}
	}
	/* Return the created array */
	ph7_result_value(pCtx,pArray);
	/*
	 * Don't worry about freeing memory here,everything will be relased
	 * automatically as soon we return from this foreign function.
	 */
	return PH7_OK;
}
/* Bound on `extends` chain depth — matches PH7_THROWABLE_WALK_MAX_DEPTH in
 * compile.c. Defends against compiler cycles even though interface cycle
 * detection should reject them up front. */
#define PH7_INTERFACE_WALK_MAX_DEPTH 64
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
		ph7_class *pIface = apInterface[n];
		int iDepth = 0;
		while( pIface && iDepth <= PH7_INTERFACE_WALK_MAX_DEPTH ){
			if( pIface == pClass ){
				return TRUE;
			}
			pIface = pIface->pBase;
			iDepth++;
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
	SySet *pInterface = &pClass->aInterface;
	SyHashEntry *pEntry;
	SyString *pName;
	sxi32 rc;
	while( pClass ){
		pName = &pClass->sName;
		/* Query the derived hashtable */
		pEntry = SyHashGet(&pBase->hDerived,(const void *)pName->zString,pName->nByte);
		if( pEntry ){
			return TRUE;
		}
		pClass = pClass->pBase;
	}
	rc = VmQueryInterfaceSet(pBase,pInterface);
	if( rc ){
		return TRUE;
	}
	/* Not a subclass */
	return FALSE;
}
/*
 * bool is_a(object $object,string $class_name)
 *   Checks if the object is of this class or has this class as one of its parents.
 * Parameters
 *  object
 *   The tested object
 * class_name
 *  The class name
 * Return
 *   Returns TRUE if the object is of this class or has this class as one of its
 *   parents, FALSE otherwise.
 */
PH7_PRIVATE int vm_builtin_is_a(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume FALSE by default */
	if( nArg > 1 && ph7_value_is_object(apArg[0])  ){
		ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;
		ph7_class *pClass;
		/* Extract the given class */
		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);
		if( pClass ){
			/* Perform the query */
			res = PH7_VmInstanceOf(pThis->pClass,pClass);
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
 * bool is_subclass_of(object/string $object,object/string $class_name)
 *   Checks if the object has this class as one of its parents.
 * Parameters
 *  object
 *   The tested object
 * class_name
 *  The class name
 * Return
 *  This function returns TRUE if the object , belongs to a class
 *  which is a subclass of class_name, FALSE otherwise.
 */
PH7_PRIVATE int vm_builtin_is_subclass_of(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume FALSE by default */
	if( nArg > 1 ){
		ph7_class *pClass,*pMain;
		/* Extract the given classes */
		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);
		pMain = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);
		if( pClass && pMain ){
			/* Perform the query */
			res = VmSubclassOf(pClass,pMain);
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
	PH7_MemObjInit(pCtx->pVm,&sResult);
	sResult.nIdx = SXU32_HIGH; /* Mark as constant */
	/* Try to invoke the callback */
	rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult);
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
	SySet aArg;               /* Arguments containers */
	sxi32 rc;
	sxu32 n;
	if( nArg < 2 || !ph7_value_is_array(apArg[1]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	PH7_MemObjInit(pCtx->pVm,&sResult);
	sResult.nIdx = SXU32_HIGH; /* Mark as constant */
	/* Initialize the arguments container */
	SySetInit(&aArg,&pCtx->pVm->sAllocator,sizeof(ph7_value *));
	/* Turn hashmap entries into callback arguments */
	pMap = (ph7_hashmap *)apArg[1]->x.pOther;
	pEntry = pMap->pFirst; /* First inserted entry */
	for( n = 0 ; n < pMap->nEntry ; n++ ){
		/* Extract node value */
		if( (pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx)) != 0 ){
			SySetPut(&aArg,(const void *)&pValue);
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
	}
	/* Try to invoke the callback */
	rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);
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
