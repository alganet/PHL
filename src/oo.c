/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef PH7_AMALGAMATION
#include "ph7int.h"
#endif
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
	pClass->nLine = nLine;
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
	/* Duplicate attribute name */
	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);
	if( zName == 0 ){
		SyMemBackendPoolFree(&pVm->sAllocator,pAttr);
		return 0;
	}
	/* Initialize fields */
	SySetInit(&pAttr->aByteCode,&pVm->sAllocator,sizeof(VmInstr));
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
		if( (pName->nByte == sizeof("__construct") - 1 && SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1 ) == 0)
			|| (pName->nByte == sizeof("__destruct") - 1 && SyMemcmp(pName->zString,"__destruct",sizeof("__destruct") - 1 ) == 0)
			|| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){
				/* Switch to public visibility when dealing with constructor/destructor */
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
	/* Copy public/protected attributes from the base class */
	SyHashResetLoopCursor(&pBase->hAttr);
	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){
		/* Make sure the private attributes are not redeclared in the subclass */
		pAttr = (ph7_class_attr *)pEntry->pUserData;
		pName = &pAttr->sName;
		if( (pEntry = SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte)) != 0 ){
			if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE && 
				((ph7_class_attr *)pEntry->pUserData)->iProtection != PH7_CLASS_PROT_PUBLIC ){
					/* Cannot redeclare private attribute */
					PH7_GenCompileError(&(*pGen),E_WARNING,((ph7_class_attr *)pEntry->pUserData)->nLine,
						"Private attribute '%z::%z' redeclared inside child class '%z'",
						&pBase->sName,pName,&pSub->sName);
					
			}
			continue;
		}
		/* Install the attribute */
		if( pAttr->iProtection != PH7_CLASS_PROT_PRIVATE ){
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
			}
			continue;
		}else{
			if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){
				/* Abstract method must be defined in the child class */
				PH7_GenCompileError(&(*pGen),E_WARNING,pMeth->nLine,
					"Abstract method '%z:%z' must be defined inside child class '%z'",
					&pBase->sName,pName,&pSub->sName);
				continue;
			}
		}
		/* Install the method */
		if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE ){
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
	/* TICKET 1433-49/1: Symisc eXtension 
	 *  A class may not implemnt all declared interface methods,so there
	 *  is no need for a method installer loop here.
	 */
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
	/* Duplicate object values */
	SyHashResetLoopCursor(&pSrc->hAttr);
	SyHashResetLoopCursor(&pClone->hAttr);
	while((pEntry = SyHashGetNextEntry(&pSrc->hAttr)) != 0 && (pEntry2 = SyHashGetNextEntry(&pClone->hAttr)) != 0 ){
		VmClassAttr *pSrcAttr = (VmClassAttr *)pEntry->pUserData;
		VmClassAttr *pDestAttr = (VmClassAttr *)pEntry2->pUserData;
		/* Duplicate non-static attribute */
		if( (pSrcAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT)) == 0 ){
			ph7_value *pvSrc,*pvDest;
			pvSrc = ExtractClassAttrValue(pVm,pSrcAttr);
			pvDest = ExtractClassAttrValue(pVm,pDestAttr);
			if( pvSrc && pvDest ){
				PH7_MemObjStore(pvSrc,pvDest);
			}
		}
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
	if( pDestr ){
		/* Invoke the destructor */
		pThis->iRef = 2; /* Prevent garbage collection */
		PH7_VmCallClassMethod(pVm,pThis,pDestr,0,0,0);
	}
	/* Release non-static attributes */
	SyHashResetLoopCursor(&pThis->hAttr);
	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){
		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;
		if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT)) == 0 ){
			PH7_VmUnsetMemObj(pVm,pVmAttr->nIdx,TRUE);
		}
		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);
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
	SyHashResetLoopCursor(&pLeft->hAttr);
	SyHashResetLoopCursor(&pRight->hAttr);
	PH7_MemObjInit(pLeft->pVm,&sV1);
	PH7_MemObjInit(pLeft->pVm,&sV2);
	sV1.nIdx = sV2.nIdx = SXU32_HIGH;
	while((pEntry = SyHashGetNextEntry(&pLeft->hAttr)) != 0 && (pEntry2 = SyHashGetNextEntry(&pRight->hAttr)) != 0 ){
		VmClassAttr *p1 = (VmClassAttr *)pEntry->pUserData;
		VmClassAttr *p2 = (VmClassAttr *)pEntry2->pUserData;
		/* Compare only non-static attribute */
		if( (p1->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT|PH7_CLASS_ATTR_STATIC)) == 0 ){
			ph7_value *pL,*pR;
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
		if( ShowType ){
			SyBlobAppend(&(*pOut),")",sizeof(char));
		}
		return SXERR_LIMIT;
	}
	rc = SXRET_OK;
	if( !ShowType ){
		SyBlobAppend(&(*pOut),"Object(",sizeof("Object(")-1);
	}
	/* Append class name */
	SyBlobFormat(&(*pOut),"%z) {",&pThis->pClass->sName);
#ifdef __WINNT__
	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);
#else
	SyBlobAppend(&(*pOut),"\n",sizeof(char));
#endif
	/* Dump object attributes */
	SyHashResetLoopCursor(&pThis->hAttr);
	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){
		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;
		if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT|PH7_CLASS_ATTR_STATIC)) == 0 ){
			/* Dump non-static/constant attribute only */
			for( i = 0 ; i < nTab ; i++ ){
				SyBlobAppend(&(*pOut)," ",sizeof(char));
			}
			pValue = ExtractClassAttrValue(pThis->pVm,pVmAttr);
			if( pValue ){
				SyBlobFormat(&(*pOut),"['%z'] =>",&pVmAttr->pAttr->sName);
#ifdef __WINNT__
				SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);
#else
				SyBlobAppend(&(*pOut),"\n",sizeof(char));
#endif
				rc = PH7_MemObjDump(&(*pOut),pValue,ShowType,nTab+1,nDepth,0);
				if( rc == SXERR_LIMIT ){
					break;
				}
			}
		}
	}
	for( i = 0 ; i < nTab ; i++ ){
		SyBlobAppend(&(*pOut)," ",sizeof(char));
	}
	SyBlobAppend(&(*pOut),"}",sizeof(char));
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
	const SyString *pAttrName  /* Attribute name */
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
	rc = PH7_VmCallClassMethod(pVm,&(*pThis),pMeth,0,nArg,apArg);
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