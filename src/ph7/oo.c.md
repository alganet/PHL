# src/ph7/oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 422/493 lines (85.60%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|      - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    5 | ` */` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `/*` |
|      - |    8 | ` * This file implement an Object Oriented (OO) subsystem for the PH7 engine.` |
|      - |    9 | ` */` |
|      - |   10 | `/*` |
|      - |   11 | ` * Create an empty class.` |
|      - |   12 | ` * Return a pointer to a raw class (ph7_class instance) on success. NULL otherwise.` |
|      - |   13 | ` */` |
|  30514 |   14 | `PH7_PRIVATE ph7_class * PH7_NewRawClass(ph7_vm *pVm,const SyString *pName,sxu32 nLine)` |
|      2 |   15 |  |
|      - |   16 | `	ph7_class *pClass;` |
|      - |   17 | `	char *zName;` |
|      - |   18 | `	/* Allocate a new instance */` |
|  30516 |   19 | `	pClass = (ph7_class *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class));` |
|  30516 |   20 | `	if( pClass == 0 ){` |
|    ! 0 |   21 | `		return 0;` |
|      - |   22 | `	}` |
|      - |   23 | `	/* Zero the structure */` |
|  30516 |   24 | `	SyZero(pClass,sizeof(ph7_class));` |
|      - |   25 | `	/* Duplicate class name */` |
|  30516 |   26 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  30516 |   27 | `	if( zName == 0 ){` |
|    ! 0 |   28 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClass);` |
|    ! 0 |   29 | `		return 0;` |
|      - |   30 | `	}` |
|      - |   31 | `	/* Initialize fields */` |
|  30516 |   32 | `	SyStringInitFromBuf(&pClass->sName,zName,pName->nByte);` |
|  30516 |   33 | `	SyHashInit(&pClass->hMethod,&pVm->sAllocator,0,0);` |
|  30516 |   34 | `	SyHashInit(&pClass->hAttr,&pVm->sAllocator,0,0);` |
|  30516 |   35 | `	SyHashInit(&pClass->hDerived,&pVm->sAllocator,0,0);` |
|  30516 |   36 | `	SySetInit(&pClass->aInterface,&pVm->sAllocator,sizeof(ph7_class *));` |
|  30516 |   37 | `	SySetInit(&pClass->aTrait,&pVm->sAllocator,sizeof(ph7_class *));` |
|  30516 |   38 | `	pClass->nLine = nLine;` |
|      - |   39 | `	/* All done */` |
|  30516 |   40 | `	return pClass;` |
|  15259 |   41 |  |
|      - |   42 | `/*` |
|      - |   43 | ` * Allocate and initialize a new class attribute.` |
|      - |   44 | ` * Return a pointer to the class attribute on success. NULL otherwise.` |
|      - |   45 | ` */` |
|  25278 |   46 | `PH7_PRIVATE ph7_class_attr * PH7_NewClassAttr(ph7_vm *pVm,const SyString *pName,sxu32 nLine,sxi32 iProtection,sxi32 iFlags)` |
|      2 |   47 |  |
|      - |   48 | `	ph7_class_attr *pAttr;` |
|      - |   49 | `	char *zName;` |
|  25280 |   50 | `	pAttr = (ph7_class_attr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_attr));` |
|  25280 |   51 | `	if( pAttr == 0 ){` |
|    ! 0 |   52 | `		return 0;` |
|      - |   53 | `	}` |
|      - |   54 | `	/* Zero the structure */` |
|  25280 |   55 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|      - |   56 | `	/* Duplicate attribute name */` |
|  25280 |   57 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  25280 |   58 | `	if( zName == 0 ){` |
|    ! 0 |   59 | `		SyMemBackendPoolFree(&pVm->sAllocator,pAttr);` |
|    ! 0 |   60 | `		return 0;` |
|      - |   61 | `	}` |
|      - |   62 | `	/* Initialize fields */` |
|  25280 |   63 | `	SySetInit(&pAttr->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|  25280 |   64 | `	SyStringInitFromBuf(&pAttr->sName,zName,pName->nByte);` |
|  25280 |   65 | `	pAttr->iProtection = iProtection;` |
|  25280 |   66 | `	pAttr->nIdx = SXU32_HIGH;` |
|  25280 |   67 | `	pAttr->iFlags = iFlags;` |
|  25280 |   68 | `	pAttr->nLine = nLine;` |
|  25280 |   69 | `	return pAttr;` |
|  12641 |   70 |  |
|      - |   71 | `/*` |
|      - |   72 | ` * Allocate and initialize a new class method.` |
|      - |   73 | ` * Return a pointer to the class method on success. NULL otherwise` |
|      - |   74 | ` * This function associate with the newly created method an automatically generated` |
|      - |   75 | ` * random unique name.` |
|      - |   76 | ` */` |
|  73090 |   77 | `PH7_PRIVATE ph7_class_method * PH7_NewClassMethod(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,sxu32 nLine,` |
|      - |   78 | `	sxi32 iProtection,sxi32 iFlags,sxi32 iFuncFlags)` |
|      2 |   79 |  |
|      - |   80 | `	ph7_class_method *pMeth;` |
|      - |   81 | `	SyHashEntry *pEntry;` |
|      - |   82 | `	SyString *pNamePtr;` |
|      - |   83 | `	char zSalt[10];` |
|      - |   84 | `	char *zName;` |
|      - |   85 | `	sxu32 nByte;` |
|      - |   86 | `	/* Allocate a new class method instance */` |
|  73092 |   87 | `	pMeth = (ph7_class_method *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_method));` |
|  73092 |   88 | `	if( pMeth == 0 ){` |
|    ! 0 |   89 | `		return 0;` |
|      - |   90 | `	}` |
|      - |   91 | `	/* Zero the structure */` |
|  73092 |   92 | `	SyZero(pMeth,sizeof(ph7_class_method));` |
|      - |   93 | `	/* Check for an already installed method with the same name */` |
|  73092 |   94 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
|  73092 |   95 | `	if( pEntry == 0 ){` |
|      - |   96 | `		/* Associate an unique VM name to this method */` |
|  73090 |   97 | `		nByte = sizeof(zSalt) + pName->nByte + SyStringLength(&pClass->sName)+sizeof(char)*7/*[[__'\0'*/;` |
|  73090 |   98 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,nByte);` |
|  73090 |   99 | `		if( zName == 0 ){` |
|    ! 0 |  100 | `			SyMemBackendPoolFree(&pVm->sAllocator,pMeth);` |
|    ! 0 |  101 | `			return 0;` |
|      - |  102 | `		}` |
|  73090 |  103 | `		pNamePtr = &pMeth->sVmName;` |
|      - |  104 | `		/* Generate a random string */` |
|  73090 |  105 | `		PH7_VmRandomString(&(*pVm),zSalt,sizeof(zSalt));` |
|  73090 |  106 | `		pNamePtr->nByte = SyBufferFormat(zName,nByte,"[__%z@%z_%.*s]",&pClass->sName,pName,sizeof(zSalt),zSalt);` |
|  73090 |  107 | `		pNamePtr->zString = zName;` |
|  36546 |  108 | `	}else{` |
|      - |  109 | `		/* Method is condidate for 'overloading' */` |
|      3 |  110 | `		ph7_class_method *pCurrent = (ph7_class_method *)pEntry->pUserData;` |
|      3 |  111 | `		pNamePtr = &pMeth->sVmName;` |
|      - |  112 | `		/* Use the same VM name */` |
|      3 |  113 | `		SyStringDupPtr(pNamePtr,&pCurrent->sVmName);` |
|      3 |  114 | `		zName = (char *)pNamePtr->zString;` |
|      - |  115 | `	}` |
|  73092 |  116 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|     26 |  117 | `		if( (pName->nByte == sizeof("__construct") - 1 && SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1 ) == 0)` |
|     17 |  118 | `			\|\| (pName->nByte == sizeof("__destruct") - 1 && SyMemcmp(pName->zString,"__destruct",sizeof("__destruct") - 1 ) == 0)` |
|     18 |  119 | `			\|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|      - |  120 | `				/* Switch to public visibility when dealing with constructor/destructor */` |
|      5 |  121 | `				iProtection = PH7_CLASS_PROT_PUBLIC;` |
|      2 |  122 | `		}` |
|     10 |  123 | `	}` |
|      - |  124 | `	/* Initialize method fields */` |
|  73094 |  125 | `	pMeth->iProtection = iProtection;` |
|  73094 |  126 | `	pMeth->iFlags = iFlags;` |
|  73094 |  127 | `	pMeth->nLine = nLine;` |
| 109641 |  128 | `	PH7_VmInitFuncState(&(*pVm),&pMeth->sFunc,&zName[sizeof(char)*4/*[__@*/+SyStringLength(&pClass->sName)],` |
|  73092 |  129 | `		pName->nByte,iFuncFlags\|VM_FUNC_CLASS_METHOD,pClass);` |
|  73094 |  130 | `	return pMeth;` |
|  36549 |  131 |  |
|      - |  132 | `/*` |
|      - |  133 | ` * Check if the given name have a class method associated with it.` |
|      - |  134 | ` * Return the desired method [i.e: ph7_class_method instance] on success. NULL otherwise.` |
|      - |  135 | ` */` |
|   5130 |  136 | `PH7_PRIVATE ph7_class_method * PH7_ClassExtractMethod(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|      2 |  137 |  |
|      - |  138 | `	SyHashEntry *pEntry;` |
|      - |  139 | `	/* Perform a hash lookup */` |
|   5132 |  140 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)zName,nByte);` |
|   5132 |  141 | `	if( pEntry == 0 ){` |
|      - |  142 | `		/* No such entry */` |
|   1828 |  143 | `		return 0;` |
|      - |  144 | `	}` |
|      - |  145 | `	/* Point to the desired method */` |
|   3306 |  146 | `	return (ph7_class_method *)pEntry->pUserData;` |
|   2567 |  147 |  |
|      - |  148 | `/*` |
|      - |  149 | ` * Check if the given name is a class attribute.` |
|      - |  150 | ` * Return the desired attribute [i.e: ph7_class_attr instance] on success.NULL otherwise.` |
|      - |  151 | ` */` |
|     20 |  152 | `PH7_PRIVATE ph7_class_attr * PH7_ClassExtractAttribute(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|      2 |  153 |  |
|      - |  154 | `	SyHashEntry *pEntry;` |
|      - |  155 | `	/* Perform a hash lookup */` |
|     22 |  156 | `	pEntry = SyHashGet(&pClass->hAttr,(const void *)zName,nByte);` |
|     22 |  157 | `	if( pEntry == 0 ){` |
|      - |  158 | `		/* No such entry */` |
|    ! 0 |  159 | `		return 0;` |
|      - |  160 | `	}` |
|      - |  161 | `	/* Point to the desierd method */` |
|     22 |  162 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|     12 |  163 |  |
|      - |  164 | `/*` |
|      - |  165 | ` * Install a class attribute in the corresponding container.` |
|      - |  166 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|      - |  167 | ` */` |
|  25278 |  168 | `PH7_PRIVATE sxi32 PH7_ClassInstallAttr(ph7_class *pClass,ph7_class_attr *pAttr)` |
|      2 |  169 |  |
|  25280 |  170 | `	SyString *pName = &pAttr->sName;` |
|      - |  171 | `	sxi32 rc;` |
|  25280 |  172 | `	rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|  25280 |  173 | `	return rc;` |
|      2 |  174 |  |
|      - |  175 | `/*` |
|      - |  176 | ` * Install a class method in the corresponding container.` |
|      - |  177 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|      - |  178 | ` */` |
|  73088 |  179 | `PH7_PRIVATE sxi32 PH7_ClassInstallMethod(ph7_class *pClass,ph7_class_method *pMeth)` |
|      2 |  180 |  |
|  73090 |  181 | `	SyString *pName = &pMeth->sFunc.sName;` |
|      - |  182 | `	sxi32 rc;` |
|  73090 |  183 | `	rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|  73090 |  184 | `	return rc;` |
|      2 |  185 |  |
|      - |  186 | `/*` |
|      - |  187 | ` * Perform an inheritance operation.` |
|      - |  188 | ` * According to the PHP language reference manual` |
|      - |  189 | ` *  When you extend a class, the subclass inherits all of the public and protected methods` |
|      - |  190 | ` *  from the parent class. Unless a class Overwrites those methods, they will retain their original` |
|      - |  191 | ` *  functionality.` |
|      - |  192 | ` *  This is useful for defining and abstracting functionality, and permits the implementation` |
|      - |  193 | ` *  of additional functionality in similar objects without the need to reimplement all of the shared` |
|      - |  194 | ` *  functionality.` |
|      - |  195 | ` *  Example #1 Inheritance Example` |
|      - |  196 | ` * <?php` |
|      - |  197 | ` * class foo` |
|      - |  198 | ` * {` |
|      - |  199 | ` *   public function printItem($string)` |
|      - |  200 | ` *   {` |
|      - |  201 | ` *       echo 'Foo: ' . $string . PHP_EOL;` |
|      - |  202 | ` *   }` |
|      - |  203 | ` *` |
|      - |  204 | ` *   public function printPHP()` |
|      - |  205 | ` *   {` |
|      - |  206 | ` *       echo 'PHP is great.' . PHP_EOL;` |
|      - |  207 | ` *   }` |
|      - |  208 | ` * }` |
|      - |  209 | ` * class bar extends foo` |
|      - |  210 | ` * {` |
|      - |  211 | ` *   public function printItem($string)` |
|      - |  212 | ` *   {` |
|      - |  213 | ` *       echo 'Bar: ' . $string . PHP_EOL;` |
|      - |  214 | ` *   }` |
|      - |  215 | ` * }` |
|      - |  216 | ` * $foo = new foo();` |
|      - |  217 | ` * $bar = new bar();` |
|      - |  218 | ` * $foo->printItem('baz'); // Output: 'Foo: baz'` |
|      - |  219 | ` * $foo->printPHP();       // Output: 'PHP is great'` |
|      - |  220 | ` * $bar->printItem('baz'); // Output: 'Bar: baz'` |
|      - |  221 | ` * $bar->printPHP();       // Output: 'PHP is great'` |
|      - |  222 | ` *` |
|      - |  223 | ` * This function return SXRET_OK if the inheritance operation was successfully performed.` |
|      - |  224 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|      - |  225 | ` * error message.` |
|      - |  226 | ` */` |
|  15096 |  227 | `PH7_PRIVATE sxi32 PH7_ClassInherit(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase)` |
|      2 |  228 |  |
|      - |  229 | `	ph7_class_method *pMeth;` |
|      - |  230 | `	ph7_class_attr *pAttr;` |
|      - |  231 | `	SyHashEntry *pEntry;` |
|      - |  232 | `	SyString *pName;` |
|      - |  233 | `	sxi32 rc;` |
|      - |  234 | `	/* Install in the derived hashtable */` |
|  15098 |  235 | `	rc = SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|  15098 |  236 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  237 | `		return rc;` |
|      - |  238 | `	}` |
|      - |  239 | `	/* Copy public/protected attributes from the base class */` |
|  15098 |  240 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
| 105338 |  241 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|      - |  242 | `		/* Make sure the private attributes are not redeclared in the subclass */` |
|  90242 |  243 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  90242 |  244 | `		pName = &pAttr->sName;` |
|  90242 |  245 | `		if( (pEntry = SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|      3 |  246 | `			if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE &&` |
|      2 |  247 | `				((ph7_class_attr *)pEntry->pUserData)->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|      - |  248 | `					/* Cannot redeclare private attribute */` |
|      4 |  249 | `					PH7_GenCompileError(&(*pGen),E_WARNING,((ph7_class_attr *)pEntry->pUserData)->nLine,` |
|      - |  250 | `						"Private attribute '%z::%z' redeclared inside child class '%z'",` |
|      1 |  251 | `						&pBase->sName,pName,&pSub->sName);` |
|      - |  252 |  |
|      1 |  253 | `			}` |
|      3 |  254 | `			continue;` |
|      - |  255 | `		}` |
|      - |  256 | `		/* Install the attribute */` |
|  90240 |  257 | `		if( pAttr->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
|  90238 |  258 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|  90238 |  259 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  260 | `				return rc;` |
|      - |  261 | `			}` |
|  45118 |  262 | `		}` |
|      2 |  263 | `	}` |
|  15098 |  264 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
| 150528 |  265 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|      - |  266 | `		/* Make sure the private/final methods are not redeclared in the subclass */` |
| 135432 |  267 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
| 135432 |  268 | `		pName = &pMeth->sFunc.sName;` |
| 135432 |  269 | `		if( (pEntry = SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|   2536 |  270 | `			 if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|      - |  271 | `				/* Cannot Overwrite final method */` |
|      7 |  272 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_method *)pEntry->pUserData)->nLine,` |
|      - |  273 | `					"Cannot Overwrite final method '%z:%z' inside child class '%z'",` |
|      2 |  274 | `					&pBase->sName,pName,&pSub->sName);` |
|      5 |  275 | `				if( rc == SXERR_ABORT ){` |
|    ! 0 |  276 | `					return SXERR_ABORT;` |
|      - |  277 | `				}` |
|      2 |  278 | `			}` |
|   2536 |  279 | `			continue;` |
|      - |  280 | `		}` |
|      - |  281 | `		/* Install the method */` |
| 132898 |  282 | `		if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
| 132896 |  283 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 132896 |  284 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  285 | `				return rc;` |
|      - |  286 | `			}` |
|  66447 |  287 | `		}` |
|      2 |  288 | `	}` |
|      - |  289 | `	/* Mark as subclass */` |
|  15098 |  290 | `	pSub->pBase = pBase;` |
|      - |  291 | `	/* All done */` |
|  15098 |  292 | `	return SXRET_OK;` |
|   7550 |  293 |  |
|      - |  294 | `/*` |
|      - |  295 | ` * Apply a trait to a class: copy all methods and attributes from the trait` |
|      - |  296 | ` * into the target class. Unlike inheritance, traits copy ALL members including` |
|      - |  297 | ` * private ones. Members already defined in the class take precedence.` |
|      - |  298 | ` */` |
|     34 |  299 | `PH7_PRIVATE sxi32 PH7_ClassUseTrait(ph7_gen_state *pGen,ph7_class *pClass,ph7_class *pTrait)` |
|      1 |  300 |  |
|      - |  301 | `	ph7_class_method *pMeth;` |
|      - |  302 | `	ph7_class_attr *pAttr;` |
|      - |  303 | `	SyHashEntry *pEntry;` |
|      - |  304 | `	SyString *pName;` |
|      - |  305 | `	sxi32 rc;` |
|      - |  306 | `	/* Detect cyclic trait composition (e.g. trait A { use B; } trait B { use A; }) */` |
|     35 |  307 | `	if( pTrait->iFlags & PH7_CLASS_TRAIT_VISITING ){` |
|    ! 0 |  308 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|    ! 0 |  309 | `			"Trait circular reference detected: %z is already being applied",&pTrait->sName);` |
|    ! 0 |  310 | `		if( rc == SXERR_ABORT ){` |
|    ! 0 |  311 | `			return SXERR_ABORT;` |
|      - |  312 | `		}` |
|    ! 0 |  313 | `		return SXRET_OK;` |
|      - |  314 | `	}` |
|     35 |  315 | `	pTrait->iFlags \|= PH7_CLASS_TRAIT_VISITING;` |
|     35 |  316 | `	rc = SXRET_OK;` |
|      - |  317 | `	/* Copy attributes from the trait */` |
|     35 |  318 | `	SyHashResetLoopCursor(&pTrait->hAttr);` |
|     47 |  319 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hAttr)) != 0 ){` |
|      - |  320 | `		SyHashEntry *pExisting;` |
|     13 |  321 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     13 |  322 | `		pName = &pAttr->sName;` |
|     13 |  323 | `		pExisting = SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte);` |
|     13 |  324 | `		if( pExisting != 0 ){` |
|      - |  325 | `			/* Attribute already exists. Check if it came from another trait` |
|      - |  326 | `			 * and whether the definitions are compatible (same defaults).` |
|      - |  327 | `			 */` |
|      - |  328 | `			ph7_class **apUsedTraits;` |
|      - |  329 | `			sxu32 nUsed,k;` |
|      5 |  330 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|      5 |  331 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|      5 |  332 | `			for(k = 0; k < nUsed; k++){` |
|      - |  333 | `				ph7_class_attr *pOther;` |
|      3 |  334 | `				pOther = PH7_ClassExtractAttribute(apUsedTraits[k],pName->zString,pName->nByte);` |
|      3 |  335 | `				if( pOther ){` |
|      - |  336 | `					/* Two traits define the same property — check if defaults differ */` |
|      3 |  337 | `					ph7_class_attr *pClassAttr = (ph7_class_attr *)pExisting->pUserData;` |
|      4 |  338 | `					if( SySetUsed(&pAttr->aByteCode) != SySetUsed(&pClassAttr->aByteCode) \|\|` |
|      3 |  339 | `						(SySetUsed(&pAttr->aByteCode) > 0 &&` |
|      3 |  340 | `						 SyMemcmp(SySetBasePtr(&pAttr->aByteCode),SySetBasePtr(&pClassAttr->aByteCode),` |
|      3 |  341 | `							SySetUsed(&pAttr->aByteCode) * SySetElemSize(&pAttr->aByteCode)) != 0) ){` |
|      4 |  342 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine,` |
|      - |  343 | `							"%z and %z define the same property ($%z) in the composition of %z. "` |
|      - |  344 | `							"However, the definition differs and is considered incompatible",` |
|      2 |  345 | `							&apUsedTraits[k]->sName,&pTrait->sName,pName,&pClass->sName);` |
|      3 |  346 | `						if( rc == SXERR_ABORT ){` |
|    ! 0 |  347 | `							goto cleanup;` |
|      - |  348 | `						}` |
|      1 |  349 | `					}` |
|      3 |  350 | `					break;` |
|      - |  351 | `				}` |
|    ! 0 |  352 | `			}` |
|      5 |  353 | `			continue;` |
|      - |  354 | `		}` |
|      9 |  355 | `		rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|      9 |  356 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  357 | `			goto cleanup;` |
|      - |  358 | `		}` |
|      1 |  359 | `	}` |
|      - |  360 | `	/* Copy methods from the trait */` |
|     35 |  361 | `	SyHashResetLoopCursor(&pTrait->hMethod);` |
|     67 |  362 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hMethod)) != 0 ){` |
|     33 |  363 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|     33 |  364 | `		pName = &pMeth->sFunc.sName;` |
|     33 |  365 | `		if( SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte) != 0 ){` |
|      - |  366 | `			/* Method already exists in the class. Check if it came from another trait` |
|      - |  367 | `			 * (unresolved conflict) vs being defined by the class itself.` |
|      - |  368 | `			 */` |
|      - |  369 | `			ph7_class **apUsedTraits;` |
|      - |  370 | `			sxu32 nUsed,k;` |
|      7 |  371 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|      7 |  372 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|      7 |  373 | `			for(k = 0; k < nUsed; k++){` |
|      3 |  374 | `				if( PH7_ClassExtractMethod(apUsedTraits[k],pName->zString,pName->nByte) != 0 ){` |
|      - |  375 | `					/* Two different traits define the same method with no resolution */` |
|      4 |  376 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|      - |  377 | `						"Trait method %z::%z has not been applied as %z::%z, "` |
|      - |  378 | `						"because of collision with %z::%z",` |
|      2 |  379 | `						&pTrait->sName,pName,` |
|      1 |  380 | `						&pClass->sName,pName,` |
|      2 |  381 | `						&apUsedTraits[k]->sName,pName);` |
|      3 |  382 | `					if( rc == SXERR_ABORT ){` |
|    ! 0 |  383 | `						goto cleanup;` |
|      - |  384 | `					}` |
|      3 |  385 | `					break;` |
|      - |  386 | `				}` |
|    ! 0 |  387 | `			}` |
|      - |  388 | `			/* Class-defined method takes precedence */` |
|      7 |  389 | `			continue;` |
|      - |  390 | `		}` |
|     27 |  391 | `		rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|     27 |  392 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  393 | `			goto cleanup;` |
|      - |  394 | `		}` |
|      1 |  395 | `	}` |
|      - |  396 | `	/* Record trait in the class */` |
|     35 |  397 | `	SySetPut(&pClass->aTrait,(const void *)&pTrait);` |
|     17 |  398 | `cleanup:` |
|      - |  399 | `	/* Always clear visiting flag, even on error paths */` |
|     35 |  400 | `	pTrait->iFlags &= ~PH7_CLASS_TRAIT_VISITING;` |
|     17 |  401 | `	SXUNUSED(pGen);` |
|     35 |  402 | `	return rc;` |
|     18 |  403 |  |
|      - |  404 | `/*` |
|      - |  405 | ` * Inherit an object interface from another object interface.` |
|      - |  406 | ` * According to the PHP language reference manual.` |
|      - |  407 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|      - |  408 | ` *  must implement, without having to define how these methods are handled.` |
|      - |  409 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|      - |  410 | ` *  class, but without any of the methods having their contents defined.` |
|      - |  411 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|      - |  412 | ` *` |
|      - |  413 | ` * This function return SXRET_OK if the interface inheritance operation was successfully performed.` |
|      - |  414 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|      - |  415 | ` * error message.` |
|      - |  416 | ` */` |
|      2 |  417 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceInherit(ph7_class *pSub,ph7_class *pBase)` |
|      1 |  418 |  |
|      - |  419 | `	ph7_class_method *pMeth;` |
|      - |  420 | `	ph7_class_attr *pAttr;` |
|      - |  421 | `	SyHashEntry *pEntry;` |
|      - |  422 | `	SyString *pName;` |
|      - |  423 | `	sxi32 rc;` |
|      - |  424 | `	/* Install in the derived hashtable */` |
|      3 |  425 | `	SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|      3 |  426 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
|      - |  427 | `	/* Copy constants */` |
|      6 |  428 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|      - |  429 | `		/* Make sure the constants are not redeclared in the subclass */` |
|      3 |  430 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      3 |  431 | `		pName = &pAttr->sName;` |
|      3 |  432 | `		if( SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte) == 0 ){` |
|      - |  433 | `			/* Install the constant in the subclass */` |
|      3 |  434 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|      3 |  435 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  436 | `				return rc;` |
|      - |  437 | `			}` |
|      1 |  438 | `		}` |
|      1 |  439 | `	}` |
|      3 |  440 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
|      - |  441 | `	/* Copy methods signature */` |
|      6 |  442 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|      - |  443 | `		/* Make sure the method are not redeclared in the subclass */` |
|      3 |  444 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      3 |  445 | `		pName = &pMeth->sFunc.sName;` |
|      3 |  446 | `		if( SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte) == 0 ){` |
|      - |  447 | `			/* Install the method */` |
|      3 |  448 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|      3 |  449 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  450 | `				return rc;` |
|      - |  451 | `			}` |
|      1 |  452 | `		}` |
|      1 |  453 | `	}` |
|      - |  454 | `	/* Mark as subclass */` |
|      3 |  455 | `	pSub->pBase = pBase;` |
|      - |  456 | `	/* All done */` |
|      3 |  457 | `	return SXRET_OK;` |
|      2 |  458 |  |
|      - |  459 | `/*` |
|      - |  460 | ` * Implements an object interface in the given main class.` |
|      - |  461 | ` * According to the PHP language reference manual.` |
|      - |  462 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|      - |  463 | ` *  must implement, without having to define how these methods are handled.` |
|      - |  464 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|      - |  465 | ` *  class, but without any of the methods having their contents defined.` |
|      - |  466 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|      - |  467 | ` *` |
|      - |  468 | ` * This function return SXRET_OK if the interface was successfully implemented.` |
|      - |  469 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|      - |  470 | ` * error message.` |
|      - |  471 | ` */` |
|     32 |  472 | `PH7_PRIVATE sxi32 PH7_ClassImplement(ph7_class *pMain,ph7_class *pInterface)` |
|      2 |  473 |  |
|      - |  474 | `	ph7_class_attr *pAttr;` |
|      - |  475 | `	SyHashEntry *pEntry;` |
|      - |  476 | `	SyString *pName;` |
|      - |  477 | `	sxi32 rc;` |
|      - |  478 | `	/* First off,copy all constants declared inside the interface */` |
|     34 |  479 | `	SyHashResetLoopCursor(&pInterface->hAttr);` |
|     52 |  480 | `	while((pEntry = SyHashGetNextEntry(&pInterface->hAttr)) != 0 ){` |
|      - |  481 | `		/* Point to the constant declaration */` |
|      3 |  482 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      3 |  483 | `		pName = &pAttr->sName;` |
|      - |  484 | `		/* Make sure the attribute is not redeclared in the main class */` |
|      3 |  485 | `		if( SyHashGet(&pMain->hAttr,pName->zString,pName->nByte) == 0 ){` |
|      - |  486 | `			/* Install the attribute */` |
|      3 |  487 | `			rc = SyHashInsert(&pMain->hAttr,pName->zString,pName->nByte,pAttr);` |
|      3 |  488 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  489 | `				return rc;` |
|      - |  490 | `			}` |
|      1 |  491 | `		}` |
|      1 |  492 | `	}` |
|      - |  493 | `	/* Install in the interface container */` |
|     34 |  494 | `	SySetPut(&pMain->aInterface,(const void *)&pInterface);` |
|      - |  495 | `	/* Install interface method stubs into the implementing class.` |
|      - |  496 | `	 * Methods already defined in the class take precedence (they satisfy` |
|      - |  497 | `	 * the interface contract). Stubs retain PH7_CLASS_ATTR_ABSTRACT so` |
|      - |  498 | `	 * the unified check in GenStateCheckAbstractMethods catches missing ones.` |
|      - |  499 | `	 */` |
|      - |  500 | `	{` |
|      - |  501 | `		ph7_class_method *pMeth;` |
|      - |  502 | `		SyHashEntry *pMEntry;` |
|      - |  503 | `		SyString *pMName;` |
|     34 |  504 | `		SyHashResetLoopCursor(&pInterface->hMethod);` |
|    122 |  505 | `		while((pMEntry = SyHashGetNextEntry(&pInterface->hMethod)) != 0 ){` |
|     74 |  506 | `			pMeth = (ph7_class_method *)pMEntry->pUserData;` |
|     74 |  507 | `			pMName = &pMeth->sFunc.sName;` |
|     74 |  508 | `			if( SyHashGet(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte) == 0 ){` |
|     15 |  509 | `				rc = SyHashInsert(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte,pMeth);` |
|     15 |  510 | `				if( rc != SXRET_OK ){` |
|    ! 0 |  511 | `					return rc;` |
|      - |  512 | `				}` |
|      7 |  513 | `			}` |
|      2 |  514 | `		}` |
|      - |  515 | `	}` |
|     34 |  516 | `	return SXRET_OK;` |
|     18 |  517 |  |
|      - |  518 | `/*` |
|      - |  519 | ` * Create a class instance [i.e: Object in the PHP jargon] at run-time.` |
|      - |  520 | ` * The following function is called when an object is created at run-time` |
|      - |  521 | ` * typically when the PH7_OP_NEW/PH7_OP_CLONE instructions are executed.` |
|      - |  522 | ` * Notes on object creation.` |
|      - |  523 | ` *` |
|      - |  524 | ` * According to PHP language reference manual.` |
|      - |  525 | ` * To create an instance of a class, the new keyword must be used. An object will always` |
|      - |  526 | ` * be created unless the object has a constructor defined that throws an exception on error.` |
|      - |  527 | ` * Classes should be defined before instantiation (and in some cases this is a requirement).` |
|      - |  528 | ` * If a string containing the name of a class is used with new, a new instance of that class` |
|      - |  529 | ` * will be created. If the class is in a namespace, its fully qualified name must be used when` |
|      - |  530 | ` * doing this.` |
|      - |  531 | ` * Example #3 Creating an instance` |
|      - |  532 | ` * <?php` |
|      - |  533 | ` *  $instance = new SimpleClass();` |
|      - |  534 | ` *   // This can also be done with a variable:` |
|      - |  535 | ` * $className = 'Foo';` |
|      - |  536 | ` * $instance = new $className(); // Foo()` |
|      - |  537 | ` * ?>` |
|      - |  538 | ` * In the class context, it is possible to create a new object by new self and new parent.` |
|      - |  539 | ` * When assigning an already created instance of a class to a new variable, the new variable` |
|      - |  540 | ` * will access the same instance as the object that was assigned. This behaviour is the same` |
|      - |  541 | ` * when passing instances to a function. A copy of an already created object can be made by` |
|      - |  542 | ` * cloning it.` |
|      - |  543 | ` * Example #4 Object Assignment` |
|      - |  544 | ` * <?php` |
|      - |  545 | ` *  class SimpleClass(){` |
|      - |  546 | ` *    public $var;` |
|      - |  547 | ` *  };` |
|      - |  548 | ` *  $instance = new SimpleClass();` |
|      - |  549 | ` *  $assigned   =  $instance;` |
|      - |  550 | ` *  $reference  =& $instance;` |
|      - |  551 | ` *  $instance->var = '$assigned will have this value';` |
|      - |  552 | ` *  $instance = null; // $instance and $reference become null` |
|      - |  553 | ` *  var_dump($instance);` |
|      - |  554 | ` *  var_dump($reference);` |
|      - |  555 | ` *  var_dump($assigned);` |
|      - |  556 | ` * ?>` |
|      - |  557 | ` * The above example will output:` |
|      - |  558 | ` * NULL` |
|      - |  559 | ` * NULL` |
|      - |  560 | ` * object(SimpleClass)#1 (1) {` |
|      - |  561 | ` *  ["var"]=>` |
|      - |  562 | ` *    string(30) "$assigned will have this value"` |
|      - |  563 | ` * }` |
|      - |  564 | ` * Example #5 Creating new objects` |
|      - |  565 | ` * <?php` |
|      - |  566 | ` * class Test` |
|      - |  567 | ` * {` |
|      - |  568 | ` *   static public function getNew()` |
|      - |  569 | ` *   {` |
|      - |  570 | ` *       return new static;` |
|      - |  571 | ` *   }` |
|      - |  572 | ` * }` |
|      - |  573 | ` * class Child extends Test` |
|      - |  574 | ` * {}` |
|      - |  575 | ` * $obj1 = new Test();` |
|      - |  576 | ` * $obj2 = new $obj1;` |
|      - |  577 | ` * var_dump($obj1 !== $obj2);` |
|      - |  578 | ` * $obj3 = Test::getNew();` |
|      - |  579 | ` * var_dump($obj3 instanceof Test);` |
|      - |  580 | ` * $obj4 = Child::getNew();` |
|      - |  581 | ` * var_dump($obj4 instanceof Child);` |
|      - |  582 | ` * ?>` |
|      - |  583 | ` * The above example will output:` |
|      - |  584 | ` * bool(true)` |
|      - |  585 | ` * bool(true)` |
|      - |  586 | ` * bool(true)` |
|      - |  587 | ` * Note that Symisc Systems have introduced powerfull extension to` |
|      - |  588 | ` * OO subsystem. For example a class attribute may have any complex` |
|      - |  589 | ` * expression associated with it when declaring the attribute unlike` |
|      - |  590 | ` * the standard PHP engine which would allow a single value.` |
|      - |  591 | ` * Example:` |
|      - |  592 | ` *  class myClass{` |
|      - |  593 | ` *    public $var = 25<<1+foo()/bar();` |
|      - |  594 | ` *  };` |
|      - |  595 | ` * Refer to the official documentation for more information.` |
|      - |  596 | ` */` |
|   1124 |  597 | `static ph7_class_instance * NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|      2 |  598 |  |
|      - |  599 | `	ph7_class_instance *pThis;` |
|      - |  600 | `	/* Allocate a new instance */` |
|   1126 |  601 | `	pThis = (ph7_class_instance *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_instance));` |
|   1126 |  602 | `	if( pThis == 0 ){` |
|    ! 0 |  603 | `		return 0;` |
|      - |  604 | `	}` |
|      - |  605 | `	/* Zero the structure */` |
|   1126 |  606 | `	SyZero(pThis,sizeof(ph7_class_instance));` |
|      - |  607 | `	/* Initialize fields */` |
|   1126 |  608 | `	pThis->iRef = 1;` |
|   1126 |  609 | `	pThis->pVm = pVm;` |
|   1126 |  610 | `	pThis->pClass = pClass;` |
|   1126 |  611 | `	SyHashInit(&pThis->hAttr,&pVm->sAllocator,0,0);` |
|   1126 |  612 | `	return pThis;` |
|    564 |  613 |  |
|      - |  614 | `/*` |
|      - |  615 | ` * Wrapper around the NewClassInstance() function defined above.` |
|      - |  616 | ` * See the block comment above for more information.` |
|      - |  617 | ` */` |
|   1082 |  618 | `PH7_PRIVATE ph7_class_instance * PH7_NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|      2 |  619 |  |
|      - |  620 | `	ph7_class_instance *pNew;` |
|      - |  621 | `	sxi32 rc;` |
|   1084 |  622 | `	pNew = NewClassInstance(&(*pVm),&(*pClass));` |
|   1084 |  623 | `	if( pNew == 0 ){` |
|    ! 0 |  624 | `		return 0;` |
|      - |  625 | `	}` |
|      - |  626 | `	/* Associate a private VM frame with this class instance */` |
|   1084 |  627 | `	rc = PH7_VmCreateClassInstanceFrame(&(*pVm),pNew);` |
|   1084 |  628 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  629 | `		SyMemBackendPoolFree(&pVm->sAllocator,pNew);` |
|    ! 0 |  630 | `		return 0;` |
|      - |  631 | `	}` |
|   1084 |  632 | `	return pNew;` |
|    543 |  633 |  |
|      - |  634 | `/*` |
|      - |  635 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon] attribute.` |
|      - |  636 | ` * This function never fail.` |
|      - |  637 | ` */` |
|    540 |  638 | `static ph7_value * ExtractClassAttrValue(ph7_vm *pVm,VmClassAttr *pAttr)` |
|      2 |  639 |  |
|      - |  640 | `	/* Extract the value */` |
|      - |  641 | `	ph7_value *pValue;` |
|    542 |  642 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|    542 |  643 | `	return pValue;` |
|      2 |  644 |  |
|      - |  645 | `/*` |
|      - |  646 | ` * Perform a clone operation on a class instance [i.e: Object in the PHP jargon].` |
|      - |  647 | ` * The following function is called when an object is cloned at run-time` |
|      - |  648 | ` * typically when the PH7_OP_CLONE instruction is executed.` |
|      - |  649 | ` * Notes on object cloning.` |
|      - |  650 | ` *` |
|      - |  651 | ` * According to PHP language reference manual.` |
|      - |  652 | ` * Creating a copy of an object with fully replicated properties is not always the wanted behavior.` |
|      - |  653 | ` * A good example of the need for copy constructors. Another example is if your object holds a reference` |
|      - |  654 | ` * to another object which it uses and when you replicate the parent object you want to create` |
|      - |  655 | ` * a new instance of this other object so that the replica has its own separate copy.` |
|      - |  656 | ` * An object copy is created by using the clone keyword (which calls the object's __clone() method if possible).` |
|      - |  657 | ` * An object's __clone() method cannot be called directly.` |
|      - |  658 | ` * $copy_of_object = clone $object;` |
|      - |  659 | ` * When an object is cloned, PHP 5 will perform a shallow copy of all of the object's properties.` |
|      - |  660 | ` * Any properties that are references to other variables, will remain references.` |
|      - |  661 | ` * Once the cloning is complete, if a __clone() method is defined, then the newly created object's __clone() method` |
|      - |  662 | ` * will be called, to allow any necessary properties that need to be changed.` |
|      - |  663 | ` * Example #1 Cloning an object` |
|      - |  664 | ` * <?php` |
|      - |  665 | ` * class SubObject` |
|      - |  666 | ` * {` |
|      - |  667 | ` *   static $instances = 0;` |
|      - |  668 | ` *   public $instance;` |
|      - |  669 | ` *` |
|      - |  670 | ` *   public function __construct() {` |
|      - |  671 | ` *       $this->instance = ++self::$instances;` |
|      - |  672 | ` *   }` |
|      - |  673 | ` *` |
|      - |  674 | ` *   public function __clone() {` |
|      - |  675 | ` *       $this->instance = ++self::$instances;` |
|      - |  676 | ` *   }` |
|      - |  677 | ` * }` |
|      - |  678 | ` *` |
|      - |  679 | ` * class MyCloneable` |
|      - |  680 | ` * {` |
|      - |  681 | ` *   public $object1;` |
|      - |  682 | ` *   public $object2;` |
|      - |  683 | ` *` |
|      - |  684 | ` *   function __clone()` |
|      - |  685 | ` *   {` |
|      - |  686 | ` *       // Force a copy of this->object, otherwise` |
|      - |  687 | ` *       // it will point to same object.` |
|      - |  688 | ` *       $this->object1 = clone $this->object1;` |
|      - |  689 | ` *   }` |
|      - |  690 | ` * }` |
|      - |  691 | ` * $obj = new MyCloneable();` |
|      - |  692 | ` * $obj->object1 = new SubObject();` |
|      - |  693 | ` * $obj->object2 = new SubObject();` |
|      - |  694 | ` * $obj2 = clone $obj;` |
|      - |  695 | ` * print("Original Object:\n");` |
|      - |  696 | ` * print_r($obj);` |
|      - |  697 | ` * print("Cloned Object:\n");` |
|      - |  698 | ` * print_r($obj2);` |
|      - |  699 | ` * ?>` |
|      - |  700 | ` * The above example will output:` |
|      - |  701 | ` * Original Object:` |
|      - |  702 | ` * MyCloneable Object` |
|      - |  703 | ` * (` |
|      - |  704 | ` *   [object1] => SubObject Object` |
|      - |  705 | ` *       (` |
|      - |  706 | ` *           [instance] => 1` |
|      - |  707 | ` *       )` |
|      - |  708 | ` *` |
|      - |  709 | ` *   [object2] => SubObject Object` |
|      - |  710 | ` *       (` |
|      - |  711 | ` *           [instance] => 2` |
|      - |  712 | ` *       )` |
|      - |  713 | ` *` |
|      - |  714 | ` * )` |
|      - |  715 | ` * Cloned Object:` |
|      - |  716 | ` * MyCloneable Object` |
|      - |  717 | ` * (` |
|      - |  718 | ` *   [object1] => SubObject Object` |
|      - |  719 | ` *       (` |
|      - |  720 | ` *           [instance] => 3` |
|      - |  721 | ` *       )` |
|      - |  722 | ` *` |
|      - |  723 | ` *   [object2] => SubObject Object` |
|      - |  724 | ` *       (` |
|      - |  725 | ` *           [instance] => 2` |
|      - |  726 | ` *       )` |
|      - |  727 | ` * )` |
|      - |  728 | ` */` |
|     42 |  729 | `PH7_PRIVATE ph7_class_instance * PH7_CloneClassInstance(ph7_class_instance *pSrc)` |
|      2 |  730 |  |
|      - |  731 | `	ph7_class_instance *pClone;` |
|      - |  732 | `	ph7_class_method *pMethod;` |
|      - |  733 | `	SyHashEntry *pEntry2;` |
|      - |  734 | `	SyHashEntry *pEntry;` |
|      - |  735 | `	ph7_vm *pVm;` |
|      - |  736 | `	sxi32 rc;` |
|      - |  737 | `	/* Allocate a new instance */` |
|     44 |  738 | `	pVm = pSrc->pVm;` |
|     44 |  739 | `	pClone = NewClassInstance(pVm,pSrc->pClass);` |
|     44 |  740 | `	if( pClone == 0 ){` |
|    ! 0 |  741 | `		return 0;` |
|      - |  742 | `	}` |
|      - |  743 | `	/* Associate a private VM frame with this class instance */` |
|     44 |  744 | `	rc = PH7_VmCreateClassInstanceFrame(pVm,pClone);` |
|     44 |  745 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  746 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClone);` |
|    ! 0 |  747 | `		return 0;` |
|      - |  748 | `	}` |
|      - |  749 | `	/* Duplicate object values */` |
|     44 |  750 | `	SyHashResetLoopCursor(&pSrc->hAttr);` |
|     44 |  751 | `	SyHashResetLoopCursor(&pClone->hAttr);` |
|    111 |  752 | `	while((pEntry = SyHashGetNextEntry(&pSrc->hAttr)) != 0 && (pEntry2 = SyHashGetNextEntry(&pClone->hAttr)) != 0 ){` |
|     48 |  753 | `		VmClassAttr *pSrcAttr = (VmClassAttr *)pEntry->pUserData;` |
|     48 |  754 | `		VmClassAttr *pDestAttr = (VmClassAttr *)pEntry2->pUserData;` |
|      - |  755 | `		/* Duplicate non-static attribute */` |
|     48 |  756 | `		if( (pSrcAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|      - |  757 | `			ph7_value *pvSrc,*pvDest;` |
|     48 |  758 | `			pvSrc = ExtractClassAttrValue(pVm,pSrcAttr);` |
|     48 |  759 | `			pvDest = ExtractClassAttrValue(pVm,pDestAttr);` |
|     48 |  760 | `			if( pvSrc && pvDest ){` |
|     48 |  761 | `				PH7_MemObjStore(pvSrc,pvDest);` |
|     23 |  762 | `			}` |
|     23 |  763 | `		}` |
|      2 |  764 | `	}` |
|      - |  765 | `	/* call the __clone method on the cloned object if available */` |
|     44 |  766 | `	pMethod = PH7_ClassExtractMethod(pClone->pClass,"__clone",sizeof("__clone")-1);` |
|     44 |  767 | `	if( pMethod ){` |
|     38 |  768 | `		if( pMethod->iCloneDepth < 16 ){` |
|     36 |  769 | `			pMethod->iCloneDepth++;` |
|     36 |  770 | `			PH7_VmCallClassMethod(pVm,pClone,pMethod,0,0,0);` |
|     19 |  771 | `		}else{` |
|      - |  772 | `			/* Nesting limit reached */` |
|      3 |  773 | `			PH7_VmThrowError(pVm,0,PH7_CTX_ERR,"Object clone limit reached,no more call to __clone()");` |
|      - |  774 | `		}` |
|      - |  775 | `		/* Reset the cursor */` |
|     38 |  776 | `		pMethod->iCloneDepth = 0;` |
|     18 |  777 | `	}` |
|      - |  778 | `	/* Return the cloned object */` |
|     44 |  779 | `	return pClone;` |
|     23 |  780 |  |
|      - |  781 | `#define CLASS_INSTANCE_DESTROYED 0x001 /* Instance is released */` |
|      - |  782 | `/*` |
|      - |  783 | ` * Release a class instance [i.e: Object in the PHP jargon] and invoke any defined destructor.` |
|      - |  784 | ` * This routine is invoked as soon as there are no other references to a particular` |
|      - |  785 | ` * class instance.` |
|      - |  786 | ` */` |
|    780 |  787 | `static void PH7_ClassInstanceRelease(ph7_class_instance *pThis)` |
|      2 |  788 |  |
|      - |  789 | `	ph7_class_method *pDestr;` |
|      - |  790 | `	SyHashEntry *pEntry;` |
|      - |  791 | `	ph7_class *pClass;` |
|      - |  792 | `	ph7_vm *pVm;` |
|    782 |  793 | `	if( pThis->iFlags & CLASS_INSTANCE_DESTROYED ){` |
|      - |  794 | `		/*` |
|      - |  795 | `		 * Already destroyed,return immediately.` |
|      - |  796 | `		 * This could happend if someone perform unset($this) in the destructor body.` |
|      - |  797 | `		 */` |
|    ! 0 |  798 | `		return;` |
|      - |  799 | `	}` |
|      - |  800 | `	/* Mark as destroyed */` |
|    782 |  801 | `	pThis->iFlags \|= CLASS_INSTANCE_DESTROYED;` |
|      - |  802 | `	/* Invoke any defined destructor if available */` |
|    782 |  803 | `	pVm = pThis->pVm;` |
|    782 |  804 | `	pClass = pThis->pClass;` |
|    782 |  805 | `	pDestr = PH7_ClassExtractMethod(pClass,"__destruct",sizeof("__destruct")-1);` |
|    782 |  806 | `	if( pDestr ){` |
|      - |  807 | `		/* Invoke the destructor */` |
|      5 |  808 | `		pThis->iRef = 2; /* Prevent garbage collection */` |
|      5 |  809 | `		PH7_VmCallClassMethod(pVm,pThis,pDestr,0,0,0);` |
|      2 |  810 | `	}` |
|      - |  811 | `	/* Release non-static attributes */` |
|    782 |  812 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|   4052 |  813 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|   3272 |  814 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|   3272 |  815 | `		if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|   3268 |  816 | `			PH7_VmUnsetMemObj(pVm,pVmAttr->nIdx,TRUE);` |
|   1633 |  817 | `		}` |
|   3272 |  818 | `		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      2 |  819 | `	}` |
|      - |  820 | `	/* Release the whole structure */` |
|    782 |  821 | `	SyHashRelease(&pThis->hAttr);` |
|    782 |  822 | `	SyMemBackendPoolFree(&pVm->sAllocator,pThis);` |
|    392 |  823 |  |
|      - |  824 | `/*` |
|      - |  825 | ` * Decrement the reference count of a class instance [i.e Object in the PHP jargon].` |
|      - |  826 | ` * If the reference count reaches zero,release the whole instance.` |
|      - |  827 | ` */` |
|  14494 |  828 | `PH7_PRIVATE void PH7_ClassInstanceUnref(ph7_class_instance *pThis)` |
|      2 |  829 |  |
|  14496 |  830 | `	pThis->iRef--;` |
|  14496 |  831 | `	if( pThis->iRef < 1 ){` |
|      - |  832 | `		/* No more reference to this instance */` |
|    782 |  833 | `		PH7_ClassInstanceRelease(&(*pThis));` |
|    390 |  834 | `	}` |
|  14496 |  835 |  |
|      - |  836 | `/*` |
|      - |  837 | ` * Compare two class instances [i.e: Objects in the PHP jargon]` |
|      - |  838 | ` * Note on objects comparison:` |
|      - |  839 | ` *  According to the PHP langauge reference manual` |
|      - |  840 | ` *  When using the comparison operator (==), object variables are compared in a simple manner` |
|      - |  841 | ` *  namely: Two object instances are equal if they have the same attributes and values, and are` |
|      - |  842 | ` *  instances of the same class.` |
|      - |  843 | ` *  On the other hand, when using the identity operator (===), object variables are identical` |
|      - |  844 | ` *  if and only if they refer to the same instance of the same class.` |
|      - |  845 | ` *  An example will clarify these rules.` |
|      - |  846 | ` *  Example #1 Example of object comparison` |
|      - |  847 | ` *  <?php` |
|      - |  848 | ` *    function bool2str($bool)` |
|      - |  849 | ` * {` |
|      - |  850 | ` *   if ($bool === false) {` |
|      - |  851 | ` *       return 'FALSE';` |
|      - |  852 | ` *   } else {` |
|      - |  853 | ` *       return 'TRUE';` |
|      - |  854 | ` *   }` |
|      - |  855 | ` * }` |
|      - |  856 | ` * function compareObjects(&$o1, &$o2)` |
|      - |  857 | ` * {` |
|      - |  858 | ` *   echo 'o1 == o2 : ' . bool2str($o1 == $o2) . "\n";` |
|      - |  859 | ` *   echo 'o1 != o2 : ' . bool2str($o1 != $o2) . "\n";` |
|      - |  860 | ` *   echo 'o1 === o2 : ' . bool2str($o1 === $o2) . "\n";` |
|      - |  861 | ` *   echo 'o1 !== o2 : ' . bool2str($o1 !== $o2) . "\n";` |
|      - |  862 | ` * }` |
|      - |  863 | ` * class Flag` |
|      - |  864 | ` * {` |
|      - |  865 | ` *   public $flag;` |
|      - |  866 | ` *` |
|      - |  867 | ` *   function Flag($flag = true) {` |
|      - |  868 | ` *       $this->flag = $flag;` |
|      - |  869 | ` *   }` |
|      - |  870 | ` * }` |
|      - |  871 | ` *` |
|      - |  872 | ` * class OtherFlag` |
|      - |  873 | ` * {` |
|      - |  874 | ` *   public $flag;` |
|      - |  875 | ` *` |
|      - |  876 | ` *   function OtherFlag($flag = true) {` |
|      - |  877 | ` *       $this->flag = $flag;` |
|      - |  878 | ` *   }` |
|      - |  879 | ` * }` |
|      - |  880 | ` *` |
|      - |  881 | ` * $o = new Flag();` |
|      - |  882 | ` * $p = new Flag();` |
|      - |  883 | ` * $q = $o;` |
|      - |  884 | ` * $r = new OtherFlag();` |
|      - |  885 | ` *` |
|      - |  886 | ` * echo "Two instances of the same class\n";` |
|      - |  887 | ` * compareObjects($o, $p);` |
|      - |  888 | ` * echo "\nTwo references to the same instance\n";` |
|      - |  889 | ` * compareObjects($o, $q);` |
|      - |  890 | ` * echo "\nInstances of two different classes\n";` |
|      - |  891 | ` * compareObjects($o, $r);` |
|      - |  892 | ` * ?>` |
|      - |  893 | ` * The above example will output:` |
|      - |  894 | ` * Two instances of the same class` |
|      - |  895 | ` * o1 == o2 : TRUE` |
|      - |  896 | ` * o1 != o2 : FALSE` |
|      - |  897 | ` * o1 === o2 : FALSE` |
|      - |  898 | ` * o1 !== o2 : TRUE` |
|      - |  899 | ` * Two references to the same instance` |
|      - |  900 | ` * o1 == o2 : TRUE` |
|      - |  901 | ` * o1 != o2 : FALSE` |
|      - |  902 | ` * o1 === o2 : TRUE` |
|      - |  903 | ` * o1 !== o2 : FALSE` |
|      - |  904 | ` * Instances of two different classes` |
|      - |  905 | ` * o1 == o2 : FALSE` |
|      - |  906 | ` * o1 != o2 : TRUE` |
|      - |  907 | ` * o1 === o2 : FALSE` |
|      - |  908 | ` * o1 !== o2 : TRUE` |
|      - |  909 | ` *` |
|      - |  910 | ` * This function return 0 if the objects are equals according to the comprison rules defined above.` |
|      - |  911 | ` * Any other return values indicates difference.` |
|      - |  912 | ` */` |
|    160 |  913 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest)` |
|      2 |  914 |  |
|      - |  915 | `	SyHashEntry *pEntry,*pEntry2;` |
|      - |  916 | `	ph7_value sV1,sV2;` |
|      - |  917 | `	sxi32 rc;` |
|    162 |  918 | `	if( iNest > 31 ){` |
|      - |  919 | `		/* Nesting limit reached */` |
|      5 |  920 | `		PH7_VmThrowError(pLeft->pVm,0,PH7_CTX_ERR,"Nesting limit reached: Infinite recursion?");` |
|      5 |  921 | `		return 1;` |
|      - |  922 | `	}` |
|      - |  923 | `	/* Comparison is performed only if the objects are instance of the same class */` |
|    158 |  924 | `	if( pLeft->pClass != pRight->pClass ){` |
|      7 |  925 | `		return 1;` |
|      - |  926 | `	}` |
|    152 |  927 | `	if( bStrict ){` |
|      - |  928 | `		/*` |
|      - |  929 | `		 * According to the PHP language reference manual:` |
|      - |  930 | `		 *  when using the identity operator (===), object variables` |
|      - |  931 | `		 *  are identical if and only if they refer to the same instance` |
|      - |  932 | `		 *  of the same class.` |
|      - |  933 | `		 */` |
|     11 |  934 | `		return !(pLeft == pRight);` |
|      - |  935 | `	}` |
|      - |  936 | `	/*` |
|      - |  937 | `	 * Attribute comparison.` |
|      - |  938 | `	 * According to the PHP reference manual:` |
|      - |  939 | `	 *  When using the comparison operator (==), object variables are compared` |
|      - |  940 | `	 *  in a simple manner, namely: Two object instances are equal if they have` |
|      - |  941 | `	 *  the same attributes and values, and are instances of the same class.` |
|      - |  942 | `	 */` |
|    142 |  943 | `	if( pLeft == pRight ){` |
|      - |  944 | `		/* Same instance,don't bother processing,object are equals */` |
|      3 |  945 | `		return 0;` |
|      - |  946 | `	}` |
|    140 |  947 | `	SyHashResetLoopCursor(&pLeft->hAttr);` |
|    140 |  948 | `	SyHashResetLoopCursor(&pRight->hAttr);` |
|    140 |  949 | `	PH7_MemObjInit(pLeft->pVm,&sV1);` |
|    140 |  950 | `	PH7_MemObjInit(pLeft->pVm,&sV2);` |
|    140 |  951 | `	sV1.nIdx = sV2.nIdx = SXU32_HIGH;` |
|    223 |  952 | `	while((pEntry = SyHashGetNextEntry(&pLeft->hAttr)) != 0 && (pEntry2 = SyHashGetNextEntry(&pRight->hAttr)) != 0 ){` |
|    146 |  953 | `		VmClassAttr *p1 = (VmClassAttr *)pEntry->pUserData;` |
|    146 |  954 | `		VmClassAttr *p2 = (VmClassAttr *)pEntry2->pUserData;` |
|      - |  955 | `		/* Compare only non-static attribute */` |
|    146 |  956 | `		if( (p1->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|      - |  957 | `			ph7_value *pL,*pR;` |
|    146 |  958 | `			pL = ExtractClassAttrValue(pLeft->pVm,p1);` |
|    146 |  959 | `			pR = ExtractClassAttrValue(pRight->pVm,p2);` |
|    146 |  960 | `			if( pL && pR ){` |
|    146 |  961 | `				PH7_MemObjLoad(pL,&sV1);` |
|    146 |  962 | `				PH7_MemObjLoad(pR,&sV2);` |
|      - |  963 | `				/* Compare the two values now */` |
|    146 |  964 | `				rc = PH7_MemObjCmp(&sV1,&sV2,bStrict,iNest+1);` |
|    146 |  965 | `				PH7_MemObjRelease(&sV1);` |
|    146 |  966 | `				PH7_MemObjRelease(&sV2);` |
|    146 |  967 | `				if( rc != 0 ){` |
|      - |  968 | `					/* Not equals */` |
|    132 |  969 | `					return rc;` |
|      - |  970 | `				}` |
|      7 |  971 | `			}` |
|      7 |  972 | `		}` |
|      1 |  973 | `	}` |
|      - |  974 | `	/* Object are equals */` |
|      9 |  975 | `	return 0;` |
|     82 |  976 |  |
|      - |  977 | `/*` |
|      - |  978 | ` * Dump a class instance and the store the dump in the BLOB given` |
|      - |  979 | ` * as the first argument.` |
|      - |  980 | ` * Note that only non-static/non-constants attribute are dumped.` |
|      - |  981 | ` * This function is typically invoked when the user issue a call` |
|      - |  982 | ` * to [var_dump(),var_export(),print_r(),...].` |
|      - |  983 | ` * This function SXRET_OK on success. Any other return value including` |
|      - |  984 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|      - |  985 | ` */` |
|    132 |  986 | `PH7_PRIVATE sxi32 PH7_ClassInstanceDump(SyBlob *pOut,ph7_class_instance *pThis,int ShowType,int nTab,int nDepth)` |
|      1 |  987 |  |
|      - |  988 | `	SyHashEntry *pEntry;` |
|      - |  989 | `	ph7_value *pValue;` |
|      - |  990 | `	sxi32 rc;` |
|      - |  991 | `	int i;` |
|    133 |  992 | `	if( nDepth > 31 ){` |
|      - |  993 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|      - |  994 | `		/* Nesting limit reached..halt immediately*/` |
|      5 |  995 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|      5 |  996 | `		if( ShowType ){` |
|      5 |  997 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|      2 |  998 | `		}` |
|      5 |  999 | `		return SXERR_LIMIT;` |
|      - | 1000 | `	}` |
|    129 | 1001 | `	rc = SXRET_OK;` |
|    129 | 1002 | `	if( !ShowType ){` |
|      3 | 1003 | `		SyBlobAppend(&(*pOut),"Object(",sizeof("Object(")-1);` |
|      1 | 1004 | `	}` |
|      - | 1005 | `	/* Append class name */` |
|    129 | 1006 | `	SyBlobFormat(&(*pOut),"%z) {",&pThis->pClass->sName);` |
|      - | 1007 | `#ifdef __WINNT__` |
|      1 | 1008 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|      - | 1009 | `#else` |
|    128 | 1010 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      - | 1011 | `#endif` |
|      - | 1012 | `	/* Dump object attributes */` |
|    129 | 1013 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|    201 | 1014 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|    133 | 1015 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    133 | 1016 | `		if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|      - | 1017 | `			/* Dump non-static/constant attribute only */` |
|   3985 | 1018 | `			for( i = 0 ; i < nTab ; i++ ){` |
|   3853 | 1019 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|   1927 | 1020 | `			}` |
|    133 | 1021 | `			pValue = ExtractClassAttrValue(pThis->pVm,pVmAttr);` |
|    133 | 1022 | `			if( pValue ){` |
|    133 | 1023 | `				SyBlobFormat(&(*pOut),"['%z'] =>",&pVmAttr->pAttr->sName);` |
|      - | 1024 | `#ifdef __WINNT__` |
|      1 | 1025 | `				SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|      - | 1026 | `#else` |
|    132 | 1027 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      - | 1028 | `#endif` |
|    133 | 1029 | `				rc = PH7_MemObjDump(&(*pOut),pValue,ShowType,nTab+1,nDepth,0);` |
|    133 | 1030 | `				if( rc == SXERR_LIMIT ){` |
|    125 | 1031 | `					break;` |
|      - | 1032 | `				}` |
|      4 | 1033 | `			}` |
|      4 | 1034 | `		}` |
|      1 | 1035 | `	}` |
|   3977 | 1036 | `	for( i = 0 ; i < nTab ; i++ ){` |
|   3849 | 1037 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|   1925 | 1038 | `	}` |
|    129 | 1039 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|    129 | 1040 | `	return rc;` |
|     67 | 1041 |  |
|      - | 1042 | `/*` |
|      - | 1043 | ` * Call a magic method [i.e: __toString(),__toBool(),__Invoke()...]` |
|      - | 1044 | ` * Return SXRET_OK on successfull call. Any other return value indicates failure.` |
|      - | 1045 | ` * Notes on magic methods.` |
|      - | 1046 | ` * According to the PHP language reference manual.` |
|      - | 1047 | ` *  The function names __construct(), __destruct(), __call(), __callStatic()` |
|      - | 1048 | ` *  __get(),  __toString(), __invoke(), __clone() are magical in PHP classes.` |
|      - | 1049 | ` * You cannot have functions with these names in any of your classes unless` |
|      - | 1050 | ` * you want the magic functionality associated with them.` |
|      - | 1051 | ` * Example of magical methods:` |
|      - | 1052 | ` * __toString()` |
|      - | 1053 | ` *  The __toString() method allows a class to decide how it will react when it is treated like` |
|      - | 1054 | ` *  a string. For example, what echo $obj; will print. This method must return a string.` |
|      - | 1055 | ` *  Example #2 Simple example` |
|      - | 1056 | ` * <?php` |
|      - | 1057 | ` * // Declare a simple class` |
|      - | 1058 | ` * class TestClass` |
|      - | 1059 | ` * {` |
|      - | 1060 | ` *   public $foo;` |
|      - | 1061 | ` *` |
|      - | 1062 | ` *   public function __construct($foo)` |
|      - | 1063 | ` *   {` |
|      - | 1064 | ` *       $this->foo = $foo;` |
|      - | 1065 | ` *   }` |
|      - | 1066 | ` *` |
|      - | 1067 | ` *   public function __toString()` |
|      - | 1068 | ` *   {` |
|      - | 1069 | ` *       return $this->foo;` |
|      - | 1070 | ` *   }` |
|      - | 1071 | ` * }` |
|      - | 1072 | ` * $class = new TestClass('Hello');` |
|      - | 1073 | ` * echo $class;` |
|      - | 1074 | ` * ?>` |
|      - | 1075 | ` * The above example will output:` |
|      - | 1076 | ` *  Hello` |
|      - | 1077 | ` *` |
|      - | 1078 | ` * Note that PH7 does not support all the magical method and introudces __toFloat(),__toInt()` |
|      - | 1079 | ` * which have the same behaviour as __toString() but for float and integer types` |
|      - | 1080 | ` * respectively.` |
|      - | 1081 | ` * Refer to the official documentation for more information.` |
|      - | 1082 | ` */` |
|      4 | 1083 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallMagicMethod(` |
|      - | 1084 | `	ph7_vm *pVm,               /* VM that own all this stuff */` |
|      - | 1085 | `	ph7_class *pClass,         /* Target class */` |
|      - | 1086 | `	ph7_class_instance *pThis, /* Target object */` |
|      - | 1087 | `	const char *zMethod,       /* Magic method name [i.e: __toString()]*/` |
|      - | 1088 | `	sxu32 nByte,               /* zMethod length*/` |
|      - | 1089 | `	const SyString *pAttrName  /* Attribute name */` |
|      - | 1090 | `	)` |
|      2 | 1091 |  |
|      6 | 1092 | `	ph7_value *apArg[2] = { 0 , 0 };` |
|      - | 1093 | `	ph7_class_method *pMeth;` |
|      - | 1094 | `	ph7_value sAttr; /* cc warning */` |
|      - | 1095 | `	sxi32 rc;` |
|      - | 1096 | `	int nArg;` |
|      - | 1097 | `	/* Make sure the magic method is available */` |
|      6 | 1098 | `	pMeth = PH7_ClassExtractMethod(&(*pClass),zMethod,nByte);` |
|      6 | 1099 | `	if( pMeth == 0 ){` |
|      - | 1100 | `		/* No such method,return immediately */` |
|      3 | 1101 | `		return SXERR_NOTFOUND;` |
|      - | 1102 | `	}` |
|      3 | 1103 | `	nArg = 0;` |
|      - | 1104 | `	/* Copy arguments */` |
|      3 | 1105 | `	if( pAttrName ){` |
|    ! 0 | 1106 | `		PH7_MemObjInitFromString(pVm,&sAttr,pAttrName);` |
|    ! 0 | 1107 | `		sAttr.nIdx = SXU32_HIGH; /* Mark as constant */` |
|    ! 0 | 1108 | `		apArg[0] = &sAttr;` |
|    ! 0 | 1109 | `		nArg = 1;` |
|    ! 0 | 1110 | `	}` |
|      - | 1111 | `	/* Call the magic method now */` |
|      3 | 1112 | `	rc = PH7_VmCallClassMethod(pVm,&(*pThis),pMeth,0,nArg,apArg);` |
|      - | 1113 | `	/* Clean up */` |
|      3 | 1114 | `	if( pAttrName ){` |
|    ! 0 | 1115 | `		PH7_MemObjRelease(&sAttr);` |
|    ! 0 | 1116 | `	}` |
|      3 | 1117 | `	return rc;` |
|      4 | 1118 |  |
|      - | 1119 | `/*` |
|      - | 1120 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon].` |
|      - | 1121 | ` * This function is simply a wrapper on ExtractClassAttrValue().` |
|      - | 1122 | ` */` |
|     18 | 1123 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceExtractAttrValue(ph7_class_instance *pThis,VmClassAttr *pAttr)` |
|      1 | 1124 |  |
|      - | 1125 | `   /* Extract the attribute value */` |
|      - | 1126 | `	ph7_value *pValue;` |
|     19 | 1127 | `	pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     19 | 1128 | `	return pValue;` |
|      1 | 1129 |  |
|      - | 1130 | `/*` |
|      - | 1131 | ` * Convert a class instance [i.e: Object in the PHP jargon] into a hashmap [i.e: array in the PHP jargon].` |
|      - | 1132 | ` * Return SXRET_OK on success. Any other value indicates failure.` |
|      - | 1133 | ` * Note on object conversion to array:` |
|      - | 1134 | ` *  Acccording to the PHP language reference manual` |
|      - | 1135 | ` *  If an object is converted to an array, the result is an array whose elements are the object's properties.` |
|      - | 1136 | ` *  The keys are the member variable names.` |
|      - | 1137 | ` *` |
|      - | 1138 | ` *  The following example:` |
|      - | 1139 | ` *  class Test {` |
|      - | 1140 | ` *   public $A = 25<<1;  // 50` |
|      - | 1141 | ` *	 public $c = rand_str(3);   // Random string of length 3` |
|      - | 1142 | ` *	 public $d = rand() & 1023; // Random number between 0..1023` |
|      - | 1143 | ` *  }` |
|      - | 1144 | ` *  var_dump((array) new Test());` |
|      - | 1145 | ` *	Will output:` |
|      - | 1146 | ` *  array(3) {` |
|      - | 1147 | ` *   [A] =>` |
|      - | 1148 | ` *      int(50)` |
|      - | 1149 | ` *   [c] =>` |
|      - | 1150 | ` *     string(3 'aps')` |
|      - | 1151 | ` *   [d] =>` |
|      - | 1152 | ` *     int(991)` |
|      - | 1153 | ` *  }` |
|      - | 1154 | ` * You have noticed that PH7 allow class attributes [i.e: $a,$c,$d in the example above]` |
|      - | 1155 | ` * have any complex expression (even function calls/Annonymous functions) as their default` |
|      - | 1156 | ` * value unlike the standard PHP engine.` |
|      - | 1157 | ` * This is a very powerful feature that you have to look at.` |
|      - | 1158 | ` */` |
|      6 | 1159 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|      1 | 1160 |  |
|      - | 1161 | `	SyHashEntry *pEntry;` |
|      - | 1162 | `	SyString *pAttrName;` |
|      - | 1163 | `	VmClassAttr *pAttr;` |
|      - | 1164 | `	ph7_value *pValue;` |
|      - | 1165 | `	ph7_value sName;` |
|      - | 1166 | `	/* Reset the loop cursor */` |
|      7 | 1167 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      7 | 1168 | `	PH7_MemObjInitFromString(pThis->pVm,&sName,0);` |
|     20 | 1169 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|      - | 1170 | `		/* Point to the current attribute */` |
|     11 | 1171 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1172 | `		/* Extract attribute value */` |
|     11 | 1173 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     11 | 1174 | `		if( pValue ){` |
|      - | 1175 | `			/* Build attribute name */` |
|     11 | 1176 | `			pAttrName = &pAttr->pAttr->sName;` |
|     11 | 1177 | `			PH7_MemObjStringAppend(&sName,pAttrName->zString,pAttrName->nByte);` |
|      - | 1178 | `			/* Perform the insertion */` |
|     11 | 1179 | `			PH7_HashmapInsert(pMap,&sName,pValue);` |
|      - | 1180 | `			/* Reset the string cursor */` |
|     11 | 1181 | `			SyBlobReset(&sName.sBlob);` |
|      5 | 1182 | `		}` |
|      1 | 1183 | `	}` |
|      7 | 1184 | `	PH7_MemObjRelease(&sName);` |
|      7 | 1185 | `	return SXRET_OK;` |
|      1 | 1186 |  |
|      - | 1187 | `/*` |
|      - | 1188 | ` * Iterate throw class attributes and invoke the given callback [i.e: xWalk()] for each` |
|      - | 1189 | ` * retrieved attribute.` |
|      - | 1190 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|      - | 1191 | ` * the attribute value in the callback body will not alter the real attribute value.` |
|      - | 1192 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|      - | 1193 | ` * a value different from PH7_OK.` |
|      - | 1194 | ` * Refer to [ph7_object_walk()] for more information.` |
|      - | 1195 | ` */` |
|    ! 0 | 1196 | `PH7_PRIVATE sxi32 PH7_ClassInstanceWalk(` |
|      - | 1197 | `	ph7_class_instance *pThis, /* Target object */` |
|      - | 1198 | `	int (*xWalk)(const char *,ph7_value *,void *), /* Walker callback */` |
|      - | 1199 | `	void *pUserData /* Last argument to xWalk() */` |
|      - | 1200 | `	)` |
|    ! 0 | 1201 |  |
|      - | 1202 | `	SyHashEntry *pEntry; /* Hash entry */` |
|      - | 1203 | `	VmClassAttr *pAttr;  /* Pointer to the attribute */` |
|      - | 1204 | `	ph7_value *pValue;   /* Attribute value */` |
|      - | 1205 | `	ph7_value sValue;    /* Copy of the attribute value */` |
|      - | 1206 | `	int rc;` |
|      - | 1207 | `	/* Reset the loop cursor */` |
|    ! 0 | 1208 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|    ! 0 | 1209 | `	PH7_MemObjInit(pThis->pVm,&sValue);` |
|      - | 1210 | `	/* Start the walk process */` |
|    ! 0 | 1211 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|      - | 1212 | `		/* Point to the current attribute */` |
|    ! 0 | 1213 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1214 | `		/* Extract attribute value */` |
|    ! 0 | 1215 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|    ! 0 | 1216 | `		if( pValue ){` |
|    ! 0 | 1217 | `			PH7_MemObjLoad(pValue,&sValue);` |
|      - | 1218 | `			/* Invoke the supplied callback */` |
|    ! 0 | 1219 | `			rc =  xWalk(SyStringData(&pAttr->pAttr->sName),&sValue,pUserData);` |
|    ! 0 | 1220 | `			PH7_MemObjRelease(&sValue);` |
|    ! 0 | 1221 | `			if( rc != PH7_OK){` |
|      - | 1222 | `				/* User callback request an operation abort */` |
|    ! 0 | 1223 | `				return SXERR_ABORT;` |
|      - | 1224 | `			}` |
|    ! 0 | 1225 | `		}` |
|    ! 0 | 1226 | `	}` |
|      - | 1227 | `	/* All done */` |
|    ! 0 | 1228 | `	return SXRET_OK;` |
|    ! 0 | 1229 |  |
|      - | 1230 | `/*` |
|      - | 1231 | ` * Extract a class atrribute value.` |
|      - | 1232 | ` * Return a pointer to the attribute value on success. Otherwise NULL.` |
|      - | 1233 | ` * Note:` |
|      - | 1234 | ` *  Access to static and constant attribute is not allowed. That is,the function` |
|      - | 1235 | ` *  will return NULL in case someone (host-application code) try to extract` |
|      - | 1236 | ` *  a static/constant attribute.` |
|      - | 1237 | ` */` |
|    ! 0 | 1238 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceFetchAttr(ph7_class_instance *pThis,const SyString *pName)` |
|    ! 0 | 1239 |  |
|      - | 1240 | `	SyHashEntry *pEntry;` |
|      - | 1241 | `	VmClassAttr *pAttr;` |
|      - | 1242 | `	/* Query the attribute hashtable */` |
|    ! 0 | 1243 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);` |
|    ! 0 | 1244 | `	if( pEntry == 0 ){` |
|      - | 1245 | `		/* No such attribute */` |
|    ! 0 | 1246 | `		return 0;` |
|      - | 1247 | `	}` |
|      - | 1248 | `	/* Point to the class atrribute */` |
|    ! 0 | 1249 | `	pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1250 | `	/* Check if we are dealing with a static/constant attribute */` |
|    ! 0 | 1251 | `	if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|      - | 1252 | `		/* Access is forbidden */` |
|    ! 0 | 1253 | `		return 0;` |
|      - | 1254 | `	}` |
|      - | 1255 | `	/* Return the attribute value */` |
|    ! 0 | 1256 | `	return ExtractClassAttrValue(pThis->pVm,pAttr);` |
|    ! 0 | 1257 |  |
|      - | 1258 |  |
