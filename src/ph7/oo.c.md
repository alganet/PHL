# src/ph7/oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 451/501 lines (90.02%)

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
| 120676 |   14 | `PH7_PRIVATE ph7_class * PH7_NewRawClass(ph7_vm *pVm,const SyString *pName,sxu32 nLine)` |
|      5 |   15 |  |
|      - |   16 | `	ph7_class *pClass;` |
|      - |   17 | `	char *zName;` |
|      - |   18 | `	/* Allocate a new instance */` |
| 120681 |   19 | `	pClass = (ph7_class *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class));` |
| 120681 |   20 | `	if( pClass == 0 ){` |
|    ! 0 |   21 | `		return 0;` |
|      - |   22 | `	}` |
|      - |   23 | `	/* Zero the structure */` |
| 120681 |   24 | `	SyZero(pClass,sizeof(ph7_class));` |
|      - |   25 | `	/* Duplicate class name */` |
| 120681 |   26 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
| 120681 |   27 | `	if( zName == 0 ){` |
|    ! 0 |   28 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClass);` |
|    ! 0 |   29 | `		return 0;` |
|      - |   30 | `	}` |
|      - |   31 | `	/* Initialize fields */` |
| 120681 |   32 | `	SyStringInitFromBuf(&pClass->sName,zName,pName->nByte);` |
| 120681 |   33 | `	SyHashInit(&pClass->hMethod,&pVm->sAllocator,0,0);` |
| 120681 |   34 | `	SyHashInit(&pClass->hAttr,&pVm->sAllocator,0,0);` |
| 120681 |   35 | `	SyHashInit(&pClass->hDerived,&pVm->sAllocator,0,0);` |
| 120681 |   36 | `	SySetInit(&pClass->aInterface,&pVm->sAllocator,sizeof(ph7_class *));` |
| 120681 |   37 | `	SySetInit(&pClass->aTrait,&pVm->sAllocator,sizeof(ph7_class *));` |
| 120681 |   38 | `	pClass->nLine = nLine;` |
|      - |   39 | `	/* All done */` |
| 120681 |   40 | `	return pClass;` |
|  60343 |   41 |  |
|      - |   42 | `/*` |
|      - |   43 | ` * Allocate and initialize a new class attribute.` |
|      - |   44 | ` * Return a pointer to the class attribute on success. NULL otherwise.` |
|      - |   45 | ` */` |
|  60386 |   46 | `PH7_PRIVATE ph7_class_attr * PH7_NewClassAttr(ph7_vm *pVm,const SyString *pName,sxu32 nLine,sxi32 iProtection,sxi32 iFlags)` |
|      5 |   47 |  |
|      - |   48 | `	ph7_class_attr *pAttr;` |
|      - |   49 | `	char *zName;` |
|  60391 |   50 | `	pAttr = (ph7_class_attr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_attr));` |
|  60391 |   51 | `	if( pAttr == 0 ){` |
|    ! 0 |   52 | `		return 0;` |
|      - |   53 | `	}` |
|      - |   54 | `	/* Zero the structure */` |
|  60391 |   55 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|      - |   56 | `	/* Duplicate attribute name */` |
|  60391 |   57 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  60391 |   58 | `	if( zName == 0 ){` |
|    ! 0 |   59 | `		SyMemBackendPoolFree(&pVm->sAllocator,pAttr);` |
|    ! 0 |   60 | `		return 0;` |
|      - |   61 | `	}` |
|      - |   62 | `	/* Initialize fields */` |
|  60391 |   63 | `	SySetInit(&pAttr->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|  60391 |   64 | `	SySetInit(&pAttr->aUnionAlts,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|  60391 |   65 | `	SyStringInitFromBuf(&pAttr->sName,zName,pName->nByte);` |
|  60391 |   66 | `	pAttr->iProtection = iProtection;` |
|  60391 |   67 | `	pAttr->nIdx = SXU32_HIGH;` |
|  60391 |   68 | `	pAttr->iFlags = iFlags;` |
|  60391 |   69 | `	pAttr->nLine = nLine;` |
|  60391 |   70 | `	return pAttr;` |
|  30198 |   71 |  |
|      - |   72 | `/*` |
|      - |   73 | ` * Allocate and initialize a new class method.` |
|      - |   74 | ` * Return a pointer to the class method on success. NULL otherwise` |
|      - |   75 | ` * This function associate with the newly created method an automatically generated` |
|      - |   76 | ` * random unique name.` |
|      - |   77 | ` */` |
| 236972 |   78 | `PH7_PRIVATE ph7_class_method * PH7_NewClassMethod(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,sxu32 nLine,` |
|      - |   79 | `	sxi32 iProtection,sxi32 iFlags,sxi32 iFuncFlags)` |
|      5 |   80 |  |
|      - |   81 | `	ph7_class_method *pMeth;` |
|      - |   82 | `	SyHashEntry *pEntry;` |
|      - |   83 | `	SyString *pNamePtr;` |
|      - |   84 | `	char zSalt[10];` |
|      - |   85 | `	char *zName;` |
|      - |   86 | `	sxu32 nByte;` |
|      - |   87 | `	/* Allocate a new class method instance */` |
| 236977 |   88 | `	pMeth = (ph7_class_method *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_method));` |
| 236977 |   89 | `	if( pMeth == 0 ){` |
|    ! 0 |   90 | `		return 0;` |
|      - |   91 | `	}` |
|      - |   92 | `	/* Zero the structure */` |
| 236977 |   93 | `	SyZero(pMeth,sizeof(ph7_class_method));` |
|      - |   94 | `	/* Check for an already installed method with the same name */` |
| 236977 |   95 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
| 236977 |   96 | `	if( pEntry == 0 ){` |
|      - |   97 | `		/* Associate an unique VM name to this method */` |
| 236975 |   98 | `		nByte = sizeof(zSalt) + pName->nByte + SyStringLength(&pClass->sName)+sizeof(char)*7/*[[__'\0'*/;` |
| 236975 |   99 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,nByte);` |
| 236975 |  100 | `		if( zName == 0 ){` |
|    ! 0 |  101 | `			SyMemBackendPoolFree(&pVm->sAllocator,pMeth);` |
|    ! 0 |  102 | `			return 0;` |
|      - |  103 | `		}` |
| 236975 |  104 | `		pNamePtr = &pMeth->sVmName;` |
|      - |  105 | `		/* Generate a random string */` |
| 236975 |  106 | `		PH7_VmRandomString(&(*pVm),zSalt,sizeof(zSalt));` |
| 236975 |  107 | `		pNamePtr->nByte = SyBufferFormat(zName,nByte,"[__%z@%z_%.*s]",&pClass->sName,pName,sizeof(zSalt),zSalt);` |
| 236975 |  108 | `		pNamePtr->zString = zName;` |
| 118490 |  109 | `	}else{` |
|      - |  110 | `		/* Method is condidate for 'overloading' */` |
|      3 |  111 | `		ph7_class_method *pCurrent = (ph7_class_method *)pEntry->pUserData;` |
|      3 |  112 | `		pNamePtr = &pMeth->sVmName;` |
|      - |  113 | `		/* Use the same VM name */` |
|      3 |  114 | `		SyStringDupPtr(pNamePtr,&pCurrent->sVmName);` |
|      3 |  115 | `		zName = (char *)pNamePtr->zString;` |
|      - |  116 | `	}` |
| 236977 |  117 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|     32 |  118 | `		if( (pName->nByte == sizeof("__construct") - 1 && SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1 ) == 0)` |
|     21 |  119 | `			\|\| (pName->nByte == sizeof("__destruct") - 1 && SyMemcmp(pName->zString,"__destruct",sizeof("__destruct") - 1 ) == 0)` |
|     22 |  120 | `			\|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|      - |  121 | `				/* Switch to public visibility when dealing with constructor/destructor */` |
|      5 |  122 | `				iProtection = PH7_CLASS_PROT_PUBLIC;` |
|      2 |  123 | `		}` |
|     12 |  124 | `	}` |
|      - |  125 | `	/* Initialize method fields */` |
| 236979 |  126 | `	pMeth->iProtection = iProtection;` |
| 236979 |  127 | `	pMeth->iFlags = iFlags;` |
| 236979 |  128 | `	pMeth->nLine = nLine;` |
| 355467 |  129 | `	PH7_VmInitFuncState(&(*pVm),&pMeth->sFunc,&zName[sizeof(char)*4/*[__@*/+SyStringLength(&pClass->sName)],` |
| 236974 |  130 | `		pName->nByte,iFuncFlags\|VM_FUNC_CLASS_METHOD,pClass);` |
| 236979 |  131 | `	return pMeth;` |
| 118493 |  132 |  |
|      - |  133 | `/*` |
|      - |  134 | ` * Check if the given name have a class method associated with it.` |
|      - |  135 | ` * Return the desired method [i.e: ph7_class_method instance] on success. NULL otherwise.` |
|      - |  136 | ` */` |
| 153246 |  137 | `PH7_PRIVATE ph7_class_method * PH7_ClassExtractMethod(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|      5 |  138 |  |
|      - |  139 | `	SyHashEntry *pEntry;` |
|      - |  140 | `	/* Perform a hash lookup */` |
| 153251 |  141 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)zName,nByte);` |
| 153251 |  142 | `	if( pEntry == 0 ){` |
|      - |  143 | `		/* No such entry */` |
|   3557 |  144 | `		return 0;` |
|      - |  145 | `	}` |
|      - |  146 | `	/* Point to the desired method */` |
| 149699 |  147 | `	return (ph7_class_method *)pEntry->pUserData;` |
|  76628 |  148 |  |
|      - |  149 | `/*` |
|      - |  150 | ` * Check if the given name is a class attribute.` |
|      - |  151 | ` * Return the desired attribute [i.e: ph7_class_attr instance] on success.NULL otherwise.` |
|      - |  152 | ` */` |
|  60458 |  153 | `PH7_PRIVATE ph7_class_attr * PH7_ClassExtractAttribute(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|      5 |  154 |  |
|      - |  155 | `	SyHashEntry *pEntry;` |
|      - |  156 | `	/* Perform a hash lookup */` |
|  60463 |  157 | `	pEntry = SyHashGet(&pClass->hAttr,(const void *)zName,nByte);` |
|  60463 |  158 | `	if( pEntry == 0 ){` |
|      - |  159 | `		/* No such entry */` |
|  60359 |  160 | `		return 0;` |
|      - |  161 | `	}` |
|      - |  162 | `	/* Point to the desierd method */` |
|    109 |  163 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|  30234 |  164 |  |
|      - |  165 | `/*` |
|      - |  166 | ` * Install a class attribute in the corresponding container.` |
|      - |  167 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|      - |  168 | ` */` |
|  60386 |  169 | `PH7_PRIVATE sxi32 PH7_ClassInstallAttr(ph7_class *pClass,ph7_class_attr *pAttr)` |
|      5 |  170 |  |
|  60391 |  171 | `	SyString *pName = &pAttr->sName;` |
|      - |  172 | `	sxi32 rc;` |
|      - |  173 | `	/* Remember where this attribute was originally declared so that later` |
|      - |  174 | `	 * inheritance/trait copies still know the declaring class (needed for` |
|      - |  175 | `	 * PHP-compatible error messages on typed properties). */` |
|  60391 |  176 | `	if( pAttr->pDeclClass == 0 ){` |
|  60391 |  177 | `		pAttr->pDeclClass = pClass;` |
|  30193 |  178 | `	}` |
|  60391 |  179 | `	rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|  60391 |  180 | `	return rc;` |
|      5 |  181 |  |
|      - |  182 | `/*` |
|      - |  183 | ` * Install a class method in the corresponding container.` |
|      - |  184 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|      - |  185 | ` */` |
| 236962 |  186 | `PH7_PRIVATE sxi32 PH7_ClassInstallMethod(ph7_class *pClass,ph7_class_method *pMeth)` |
|      5 |  187 |  |
| 236967 |  188 | `	SyString *pName = &pMeth->sFunc.sName;` |
|      - |  189 | `	sxi32 rc;` |
| 236967 |  190 | `	rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 236967 |  191 | `	return rc;` |
|      5 |  192 |  |
|      - |  193 | `/*` |
|      - |  194 | ` * Perform an inheritance operation.` |
|      - |  195 | ` * According to the PHP language reference manual` |
|      - |  196 | ` *  When you extend a class, the subclass inherits all of the public and protected methods` |
|      - |  197 | ` *  from the parent class. Unless a class Overwrites those methods, they will retain their original` |
|      - |  198 | ` *  functionality.` |
|      - |  199 | ` *  This is useful for defining and abstracting functionality, and permits the implementation` |
|      - |  200 | ` *  of additional functionality in similar objects without the need to reimplement all of the shared` |
|      - |  201 | ` *  functionality.` |
|      - |  202 | ` *  Example #1 Inheritance Example` |
|      - |  203 | ` * <?php` |
|      - |  204 | ` * class foo` |
|      - |  205 | ` * {` |
|      - |  206 | ` *   public function printItem($string)` |
|      - |  207 | ` *   {` |
|      - |  208 | ` *       echo 'Foo: ' . $string . PHP_EOL;` |
|      - |  209 | ` *   }` |
|      - |  210 | ` *` |
|      - |  211 | ` *   public function printPHP()` |
|      - |  212 | ` *   {` |
|      - |  213 | ` *       echo 'PHP is great.' . PHP_EOL;` |
|      - |  214 | ` *   }` |
|      - |  215 | ` * }` |
|      - |  216 | ` * class bar extends foo` |
|      - |  217 | ` * {` |
|      - |  218 | ` *   public function printItem($string)` |
|      - |  219 | ` *   {` |
|      - |  220 | ` *       echo 'Bar: ' . $string . PHP_EOL;` |
|      - |  221 | ` *   }` |
|      - |  222 | ` * }` |
|      - |  223 | ` * $foo = new foo();` |
|      - |  224 | ` * $bar = new bar();` |
|      - |  225 | ` * $foo->printItem('baz'); // Output: 'Foo: baz'` |
|      - |  226 | ` * $foo->printPHP();       // Output: 'PHP is great'` |
|      - |  227 | ` * $bar->printItem('baz'); // Output: 'Bar: baz'` |
|      - |  228 | ` * $bar->printPHP();       // Output: 'PHP is great'` |
|      - |  229 | ` *` |
|      - |  230 | ` * This function return SXRET_OK if the inheritance operation was successfully performed.` |
|      - |  231 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|      - |  232 | ` * error message.` |
|      - |  233 | ` */` |
|  66250 |  234 | `PH7_PRIVATE sxi32 PH7_ClassInherit(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase)` |
|      5 |  235 |  |
|      - |  236 | `	ph7_class_method *pMeth;` |
|      - |  237 | `	ph7_class_attr *pAttr;` |
|      - |  238 | `	SyHashEntry *pEntry;` |
|      - |  239 | `	SyString *pName;` |
|      - |  240 | `	sxi32 rc;` |
|      - |  241 | `	/* Install in the derived hashtable */` |
|  66255 |  242 | `	rc = SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|  66255 |  243 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  244 | `		return rc;` |
|      - |  245 | `	}` |
|      - |  246 | `	/* Copy public/protected attributes from the base class */` |
|  66255 |  247 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
| 463273 |  248 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|      - |  249 | `		/* Make sure the private attributes are not redeclared in the subclass */` |
| 397023 |  250 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
| 397023 |  251 | `		pName = &pAttr->sName;` |
| 397023 |  252 | `		if( (pEntry = SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|      6 |  253 | `			if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE &&` |
|      2 |  254 | `				((ph7_class_attr *)pEntry->pUserData)->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|      - |  255 | `					/* Cannot redeclare private attribute */` |
|      4 |  256 | `					PH7_GenCompileError(&(*pGen),E_WARNING,((ph7_class_attr *)pEntry->pUserData)->nLine,` |
|      - |  257 | `						"Private attribute '%z::%z' redeclared inside child class '%z'",` |
|      1 |  258 | `						&pBase->sName,pName,&pSub->sName);` |
|      - |  259 |  |
|      1 |  260 | `			}` |
|      6 |  261 | `			continue;` |
|      - |  262 | `		}` |
|      - |  263 | `		/* Install the attribute */` |
| 397019 |  264 | `		if( pAttr->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
| 397015 |  265 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
| 397015 |  266 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  267 | `				return rc;` |
|      - |  268 | `			}` |
| 198505 |  269 | `		}` |
|      5 |  270 | `	}` |
|  66255 |  271 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
| 661851 |  272 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|      - |  273 | `		/* Make sure the private/final methods are not redeclared in the subclass */` |
| 595601 |  274 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
| 595601 |  275 | `		pName = &pMeth->sFunc.sName;` |
| 595601 |  276 | `		if( (pEntry = SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|   3185 |  277 | `			 if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|      - |  278 | `				/* Cannot Overwrite final method */` |
|      8 |  279 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_method *)pEntry->pUserData)->nLine,` |
|      - |  280 | `					"Cannot Overwrite final method '%z:%z' inside child class '%z'",` |
|      2 |  281 | `					&pBase->sName,pName,&pSub->sName);` |
|      6 |  282 | `				if( rc == SXERR_ABORT ){` |
|    ! 0 |  283 | `					return SXERR_ABORT;` |
|      - |  284 | `				}` |
|      2 |  285 | `			}` |
|   3185 |  286 | `			continue;` |
|      - |  287 | `		}` |
|      - |  288 | `		/* Install the method */` |
| 592421 |  289 | `		if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
| 592419 |  290 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 592419 |  291 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  292 | `				return rc;` |
|      - |  293 | `			}` |
| 296207 |  294 | `		}` |
|      5 |  295 | `	}` |
|      - |  296 | `	/* Mark as subclass */` |
|  66255 |  297 | `	pSub->pBase = pBase;` |
|      - |  298 | `	/* All done */` |
|  66255 |  299 | `	return SXRET_OK;` |
|  33130 |  300 |  |
|      - |  301 | `/*` |
|      - |  302 | ` * Apply a trait to a class: copy all methods and attributes from the trait` |
|      - |  303 | ` * into the target class. Unlike inheritance, traits copy ALL members including` |
|      - |  304 | ` * private ones. Members already defined in the class take precedence.` |
|      - |  305 | ` */` |
|     44 |  306 | `PH7_PRIVATE sxi32 PH7_ClassUseTrait(ph7_gen_state *pGen,ph7_class *pClass,ph7_class *pTrait)` |
|      5 |  307 |  |
|      - |  308 | `	ph7_class_method *pMeth;` |
|      - |  309 | `	ph7_class_attr *pAttr;` |
|      - |  310 | `	SyHashEntry *pEntry;` |
|      - |  311 | `	SyString *pName;` |
|      - |  312 | `	sxi32 rc;` |
|      - |  313 | `	/* Detect cyclic trait composition (e.g. trait A { use B; } trait B { use A; }) */` |
|     49 |  314 | `	if( pTrait->iFlags & PH7_CLASS_TRAIT_VISITING ){` |
|    ! 0 |  315 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|    ! 0 |  316 | `			"Trait circular reference detected: %z is already being applied",&pTrait->sName);` |
|    ! 0 |  317 | `		if( rc == SXERR_ABORT ){` |
|    ! 0 |  318 | `			return SXERR_ABORT;` |
|      - |  319 | `		}` |
|    ! 0 |  320 | `		return SXRET_OK;` |
|      - |  321 | `	}` |
|     49 |  322 | `	pTrait->iFlags \|= PH7_CLASS_TRAIT_VISITING;` |
|     49 |  323 | `	rc = SXRET_OK;` |
|      - |  324 | `	/* Copy attributes from the trait */` |
|     49 |  325 | `	SyHashResetLoopCursor(&pTrait->hAttr);` |
|     65 |  326 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hAttr)) != 0 ){` |
|      - |  327 | `		SyHashEntry *pExisting;` |
|     20 |  328 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     20 |  329 | `		pName = &pAttr->sName;` |
|     20 |  330 | `		pExisting = SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte);` |
|     20 |  331 | `		if( pExisting != 0 ){` |
|      - |  332 | `			/* Attribute already exists. Check if it came from another trait` |
|      - |  333 | `			 * and whether the definitions are compatible (same defaults).` |
|      - |  334 | `			 */` |
|      - |  335 | `			ph7_class **apUsedTraits;` |
|      - |  336 | `			sxu32 nUsed,k;` |
|      6 |  337 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|      6 |  338 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|      6 |  339 | `			for(k = 0; k < nUsed; k++){` |
|      - |  340 | `				ph7_class_attr *pOther;` |
|      3 |  341 | `				pOther = PH7_ClassExtractAttribute(apUsedTraits[k],pName->zString,pName->nByte);` |
|      3 |  342 | `				if( pOther ){` |
|      - |  343 | `					/* Two traits define the same property — check if defaults differ */` |
|      3 |  344 | `					ph7_class_attr *pClassAttr = (ph7_class_attr *)pExisting->pUserData;` |
|      4 |  345 | `					if( SySetUsed(&pAttr->aByteCode) != SySetUsed(&pClassAttr->aByteCode) \|\|` |
|      3 |  346 | `						(SySetUsed(&pAttr->aByteCode) > 0 &&` |
|      3 |  347 | `						 SyMemcmp(SySetBasePtr(&pAttr->aByteCode),SySetBasePtr(&pClassAttr->aByteCode),` |
|      3 |  348 | `							SySetUsed(&pAttr->aByteCode) * SySetElemSize(&pAttr->aByteCode)) != 0) ){` |
|      4 |  349 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine,` |
|      - |  350 | `							"%z and %z define the same property ($%z) in the composition of %z. "` |
|      - |  351 | `							"However, the definition differs and is considered incompatible",` |
|      2 |  352 | `							&apUsedTraits[k]->sName,&pTrait->sName,pName,&pClass->sName);` |
|      3 |  353 | `						if( rc == SXERR_ABORT ){` |
|    ! 0 |  354 | `							goto cleanup;` |
|      - |  355 | `						}` |
|      1 |  356 | `					}` |
|      3 |  357 | `					break;` |
|      - |  358 | `				}` |
|    ! 0 |  359 | `			}` |
|      6 |  360 | `			continue;` |
|      - |  361 | `		}` |
|     16 |  362 | `		rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|     16 |  363 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  364 | `			goto cleanup;` |
|      - |  365 | `		}` |
|      4 |  366 | `	}` |
|      - |  367 | `	/* Copy methods from the trait */` |
|     49 |  368 | `	SyHashResetLoopCursor(&pTrait->hMethod);` |
|     89 |  369 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hMethod)) != 0 ){` |
|     44 |  370 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|     44 |  371 | `		pName = &pMeth->sFunc.sName;` |
|     44 |  372 | `		if( SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte) != 0 ){` |
|      - |  373 | `			/* Method already exists in the class. Check if it came from another trait` |
|      - |  374 | `			 * (unresolved conflict) vs being defined by the class itself.` |
|      - |  375 | `			 */` |
|      - |  376 | `			ph7_class **apUsedTraits;` |
|      - |  377 | `			sxu32 nUsed,k;` |
|     11 |  378 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|     11 |  379 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|     11 |  380 | `			for(k = 0; k < nUsed; k++){` |
|      3 |  381 | `				if( PH7_ClassExtractMethod(apUsedTraits[k],pName->zString,pName->nByte) != 0 ){` |
|      - |  382 | `					/* Two different traits define the same method with no resolution */` |
|      4 |  383 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|      - |  384 | `						"Trait method %z::%z has not been applied as %z::%z, "` |
|      - |  385 | `						"because of collision with %z::%z",` |
|      2 |  386 | `						&pTrait->sName,pName,` |
|      1 |  387 | `						&pClass->sName,pName,` |
|      2 |  388 | `						&apUsedTraits[k]->sName,pName);` |
|      3 |  389 | `					if( rc == SXERR_ABORT ){` |
|    ! 0 |  390 | `						goto cleanup;` |
|      - |  391 | `					}` |
|      3 |  392 | `					break;` |
|      - |  393 | `				}` |
|    ! 0 |  394 | `			}` |
|      - |  395 | `			/* Class-defined method takes precedence */` |
|     11 |  396 | `			continue;` |
|      - |  397 | `		}` |
|     36 |  398 | `		rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|     36 |  399 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  400 | `			goto cleanup;` |
|      - |  401 | `		}` |
|      4 |  402 | `	}` |
|      - |  403 | `	/* Record trait in the class */` |
|     49 |  404 | `	SySetPut(&pClass->aTrait,(const void *)&pTrait);` |
|     22 |  405 | `cleanup:` |
|      - |  406 | `	/* Always clear visiting flag, even on error paths */` |
|     49 |  407 | `	pTrait->iFlags &= ~PH7_CLASS_TRAIT_VISITING;` |
|     22 |  408 | `	SXUNUSED(pGen);` |
|     49 |  409 | `	return rc;` |
|     27 |  410 |  |
|      - |  411 | `/*` |
|      - |  412 | ` * Inherit an object interface from another object interface.` |
|      - |  413 | ` * According to the PHP language reference manual.` |
|      - |  414 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|      - |  415 | ` *  must implement, without having to define how these methods are handled.` |
|      - |  416 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|      - |  417 | ` *  class, but without any of the methods having their contents defined.` |
|      - |  418 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|      - |  419 | ` *` |
|      - |  420 | ` * This function return SXRET_OK if the interface inheritance operation was successfully performed.` |
|      - |  421 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|      - |  422 | ` * error message.` |
|      - |  423 | ` */` |
|   9452 |  424 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceInherit(ph7_class *pSub,ph7_class *pBase)` |
|      5 |  425 |  |
|      - |  426 | `	ph7_class_method *pMeth;` |
|      - |  427 | `	ph7_class_attr *pAttr;` |
|      - |  428 | `	SyHashEntry *pEntry;` |
|      - |  429 | `	SyString *pName;` |
|      - |  430 | `	sxi32 rc;` |
|      - |  431 | `	/* Install in the derived hashtable */` |
|   9457 |  432 | `	SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|   9457 |  433 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
|      - |  434 | `	/* Copy constants */` |
|  14185 |  435 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|      - |  436 | `		/* Make sure the constants are not redeclared in the subclass */` |
|      3 |  437 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      3 |  438 | `		pName = &pAttr->sName;` |
|      3 |  439 | `		if( SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte) == 0 ){` |
|      - |  440 | `			/* Install the constant in the subclass */` |
|      3 |  441 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|      3 |  442 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  443 | `				return rc;` |
|      - |  444 | `			}` |
|      1 |  445 | `		}` |
|      1 |  446 | `	}` |
|   9457 |  447 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
|      - |  448 | `	/* Copy methods signature */` |
|  17373 |  449 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|      - |  450 | `		/* Make sure the method are not redeclared in the subclass */` |
|   3195 |  451 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   3195 |  452 | `		pName = &pMeth->sFunc.sName;` |
|   3195 |  453 | `		if( SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte) == 0 ){` |
|      - |  454 | `			/* Install the method */` |
|   3195 |  455 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|   3195 |  456 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  457 | `				return rc;` |
|      - |  458 | `			}` |
|   1595 |  459 | `		}` |
|      5 |  460 | `	}` |
|      - |  461 | `	/* Mark as subclass */` |
|   9457 |  462 | `	pSub->pBase = pBase;` |
|      - |  463 | `	/* All done */` |
|   9457 |  464 | `	return SXRET_OK;` |
|   4731 |  465 |  |
|      - |  466 | `/*` |
|      - |  467 | ` * Implements an object interface in the given main class.` |
|      - |  468 | ` * According to the PHP language reference manual.` |
|      - |  469 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|      - |  470 | ` *  must implement, without having to define how these methods are handled.` |
|      - |  471 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|      - |  472 | ` *  class, but without any of the methods having their contents defined.` |
|      - |  473 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|      - |  474 | ` *` |
|      - |  475 | ` * This function return SXRET_OK if the interface was successfully implemented.` |
|      - |  476 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|      - |  477 | ` * error message.` |
|      - |  478 | ` */` |
|  85188 |  479 | `PH7_PRIVATE sxi32 PH7_ClassImplement(ph7_class *pMain,ph7_class *pInterface)` |
|      5 |  480 |  |
|      - |  481 | `	ph7_class_attr *pAttr;` |
|      - |  482 | `	SyHashEntry *pEntry;` |
|      - |  483 | `	SyString *pName;` |
|      - |  484 | `	sxi32 rc;` |
|      - |  485 | `	/* First off,copy all constants declared inside the interface */` |
|  85193 |  486 | `	SyHashResetLoopCursor(&pInterface->hAttr);` |
| 127789 |  487 | `	while((pEntry = SyHashGetNextEntry(&pInterface->hAttr)) != 0 ){` |
|      - |  488 | `		/* Point to the constant declaration */` |
|      3 |  489 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      3 |  490 | `		pName = &pAttr->sName;` |
|      - |  491 | `		/* Make sure the attribute is not redeclared in the main class */` |
|      3 |  492 | `		if( SyHashGet(&pMain->hAttr,pName->zString,pName->nByte) == 0 ){` |
|      - |  493 | `			/* Install the attribute */` |
|      3 |  494 | `			rc = SyHashInsert(&pMain->hAttr,pName->zString,pName->nByte,pAttr);` |
|      3 |  495 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  496 | `				return rc;` |
|      - |  497 | `			}` |
|      1 |  498 | `		}` |
|      1 |  499 | `	}` |
|      - |  500 | `	/* Install in the interface container */` |
|  85193 |  501 | `	SySetPut(&pMain->aInterface,(const void *)&pInterface);` |
|      - |  502 | `	/* Install interface method stubs into the implementing class.` |
|      - |  503 | `	 * Methods already defined in the class take precedence (they satisfy` |
|      - |  504 | `	 * the interface contract). Stubs retain PH7_CLASS_ATTR_ABSTRACT so` |
|      - |  505 | `	 * the unified check in GenStateCheckAbstractMethods catches missing ones.` |
|      - |  506 | `	 */` |
|      - |  507 | `	{` |
|      - |  508 | `		ph7_class_method *pMeth;` |
|      - |  509 | `		SyHashEntry *pMEntry;` |
|      - |  510 | `		SyString *pMName;` |
|  85193 |  511 | `		SyHashResetLoopCursor(&pInterface->hMethod);` |
| 269849 |  512 | `		while((pMEntry = SyHashGetNextEntry(&pInterface->hMethod)) != 0 ){` |
| 142067 |  513 | `			pMeth = (ph7_class_method *)pMEntry->pUserData;` |
| 142067 |  514 | `			pMName = &pMeth->sFunc.sName;` |
| 142067 |  515 | `			if( SyHashGet(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte) == 0 ){` |
|     18 |  516 | `				rc = SyHashInsert(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte,pMeth);` |
|     18 |  517 | `				if( rc != SXRET_OK ){` |
|    ! 0 |  518 | `					return rc;` |
|      - |  519 | `				}` |
|      7 |  520 | `			}` |
|      5 |  521 | `		}` |
|      - |  522 | `	}` |
|  85193 |  523 | `	return SXRET_OK;` |
|  42599 |  524 |  |
|      - |  525 | `/*` |
|      - |  526 | ` * Create a class instance [i.e: Object in the PHP jargon] at run-time.` |
|      - |  527 | ` * The following function is called when an object is created at run-time` |
|      - |  528 | ` * typically when the PH7_OP_NEW/PH7_OP_CLONE instructions are executed.` |
|      - |  529 | ` * Notes on object creation.` |
|      - |  530 | ` *` |
|      - |  531 | ` * According to PHP language reference manual.` |
|      - |  532 | ` * To create an instance of a class, the new keyword must be used. An object will always` |
|      - |  533 | ` * be created unless the object has a constructor defined that throws an exception on error.` |
|      - |  534 | ` * Classes should be defined before instantiation (and in some cases this is a requirement).` |
|      - |  535 | ` * If a string containing the name of a class is used with new, a new instance of that class` |
|      - |  536 | ` * will be created. If the class is in a namespace, its fully qualified name must be used when` |
|      - |  537 | ` * doing this.` |
|      - |  538 | ` * Example #3 Creating an instance` |
|      - |  539 | ` * <?php` |
|      - |  540 | ` *  $instance = new SimpleClass();` |
|      - |  541 | ` *   // This can also be done with a variable:` |
|      - |  542 | ` * $className = 'Foo';` |
|      - |  543 | ` * $instance = new $className(); // Foo()` |
|      - |  544 | ` * ?>` |
|      - |  545 | ` * In the class context, it is possible to create a new object by new self and new parent.` |
|      - |  546 | ` * When assigning an already created instance of a class to a new variable, the new variable` |
|      - |  547 | ` * will access the same instance as the object that was assigned. This behaviour is the same` |
|      - |  548 | ` * when passing instances to a function. A copy of an already created object can be made by` |
|      - |  549 | ` * cloning it.` |
|      - |  550 | ` * Example #4 Object Assignment` |
|      - |  551 | ` * <?php` |
|      - |  552 | ` *  class SimpleClass(){` |
|      - |  553 | ` *    public $var;` |
|      - |  554 | ` *  };` |
|      - |  555 | ` *  $instance = new SimpleClass();` |
|      - |  556 | ` *  $assigned   =  $instance;` |
|      - |  557 | ` *  $reference  =& $instance;` |
|      - |  558 | ` *  $instance->var = '$assigned will have this value';` |
|      - |  559 | ` *  $instance = null; // $instance and $reference become null` |
|      - |  560 | ` *  var_dump($instance);` |
|      - |  561 | ` *  var_dump($reference);` |
|      - |  562 | ` *  var_dump($assigned);` |
|      - |  563 | ` * ?>` |
|      - |  564 | ` * The above example will output:` |
|      - |  565 | ` * NULL` |
|      - |  566 | ` * NULL` |
|      - |  567 | ` * object(SimpleClass)#1 (1) {` |
|      - |  568 | ` *  ["var"]=>` |
|      - |  569 | ` *    string(30) "$assigned will have this value"` |
|      - |  570 | ` * }` |
|      - |  571 | ` * Example #5 Creating new objects` |
|      - |  572 | ` * <?php` |
|      - |  573 | ` * class Test` |
|      - |  574 | ` * {` |
|      - |  575 | ` *   static public function getNew()` |
|      - |  576 | ` *   {` |
|      - |  577 | ` *       return new static;` |
|      - |  578 | ` *   }` |
|      - |  579 | ` * }` |
|      - |  580 | ` * class Child extends Test` |
|      - |  581 | ` * {}` |
|      - |  582 | ` * $obj1 = new Test();` |
|      - |  583 | ` * $obj2 = new $obj1;` |
|      - |  584 | ` * var_dump($obj1 !== $obj2);` |
|      - |  585 | ` * $obj3 = Test::getNew();` |
|      - |  586 | ` * var_dump($obj3 instanceof Test);` |
|      - |  587 | ` * $obj4 = Child::getNew();` |
|      - |  588 | ` * var_dump($obj4 instanceof Child);` |
|      - |  589 | ` * ?>` |
|      - |  590 | ` * The above example will output:` |
|      - |  591 | ` * bool(true)` |
|      - |  592 | ` * bool(true)` |
|      - |  593 | ` * bool(true)` |
|      - |  594 | ` * Note that Symisc Systems have introduced powerfull extension to` |
|      - |  595 | ` * OO subsystem. For example a class attribute may have any complex` |
|      - |  596 | ` * expression associated with it when declaring the attribute unlike` |
|      - |  597 | ` * the standard PHP engine which would allow a single value.` |
|      - |  598 | ` * Example:` |
|      - |  599 | ` *  class myClass{` |
|      - |  600 | ` *    public $var = 25<<1+foo()/bar();` |
|      - |  601 | ` *  };` |
|      - |  602 | ` * Refer to the official documentation for more information.` |
|      - |  603 | ` */` |
|   2218 |  604 | `static ph7_class_instance * NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|      5 |  605 |  |
|      - |  606 | `	ph7_class_instance *pThis;` |
|      - |  607 | `	/* Allocate a new instance */` |
|   2223 |  608 | `	pThis = (ph7_class_instance *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_instance));` |
|   2223 |  609 | `	if( pThis == 0 ){` |
|    ! 0 |  610 | `		return 0;` |
|      - |  611 | `	}` |
|      - |  612 | `	/* Zero the structure */` |
|   2223 |  613 | `	SyZero(pThis,sizeof(ph7_class_instance));` |
|      - |  614 | `	/* Initialize fields */` |
|   2223 |  615 | `	pThis->iRef = 1;` |
|   2223 |  616 | `	pThis->pVm = pVm;` |
|   2223 |  617 | `	pThis->pClass = pClass;` |
|   2223 |  618 | `	SyHashInit(&pThis->hAttr,&pVm->sAllocator,0,0);` |
|   2223 |  619 | `	return pThis;` |
|   1114 |  620 |  |
|      - |  621 | `/*` |
|      - |  622 | ` * Wrapper around the NewClassInstance() function defined above.` |
|      - |  623 | ` * See the block comment above for more information.` |
|      - |  624 | ` */` |
|   2174 |  625 | `PH7_PRIVATE ph7_class_instance * PH7_NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|      5 |  626 |  |
|      - |  627 | `	ph7_class_instance *pNew;` |
|      - |  628 | `	sxi32 rc;` |
|   2179 |  629 | `	pNew = NewClassInstance(&(*pVm),&(*pClass));` |
|   2179 |  630 | `	if( pNew == 0 ){` |
|    ! 0 |  631 | `		return 0;` |
|      - |  632 | `	}` |
|      - |  633 | `	/* Associate a private VM frame with this class instance */` |
|   2179 |  634 | `	rc = PH7_VmCreateClassInstanceFrame(&(*pVm),pNew);` |
|   2179 |  635 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  636 | `		SyMemBackendPoolFree(&pVm->sAllocator,pNew);` |
|    ! 0 |  637 | `		return 0;` |
|      - |  638 | `	}` |
|   2179 |  639 | `	return pNew;` |
|   1092 |  640 |  |
|      - |  641 | `/*` |
|      - |  642 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon] attribute.` |
|      - |  643 | ` * This function never fail.` |
|      - |  644 | ` */` |
|   1270 |  645 | `static ph7_value * ExtractClassAttrValue(ph7_vm *pVm,VmClassAttr *pAttr)` |
|      5 |  646 |  |
|      - |  647 | `	/* Extract the value */` |
|      - |  648 | `	ph7_value *pValue;` |
|   1275 |  649 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|   1275 |  650 | `	return pValue;` |
|      5 |  651 |  |
|      - |  652 | `/*` |
|      - |  653 | ` * Perform a clone operation on a class instance [i.e: Object in the PHP jargon].` |
|      - |  654 | ` * The following function is called when an object is cloned at run-time` |
|      - |  655 | ` * typically when the PH7_OP_CLONE instruction is executed.` |
|      - |  656 | ` * Notes on object cloning.` |
|      - |  657 | ` *` |
|      - |  658 | ` * According to PHP language reference manual.` |
|      - |  659 | ` * Creating a copy of an object with fully replicated properties is not always the wanted behavior.` |
|      - |  660 | ` * A good example of the need for copy constructors. Another example is if your object holds a reference` |
|      - |  661 | ` * to another object which it uses and when you replicate the parent object you want to create` |
|      - |  662 | ` * a new instance of this other object so that the replica has its own separate copy.` |
|      - |  663 | ` * An object copy is created by using the clone keyword (which calls the object's __clone() method if possible).` |
|      - |  664 | ` * An object's __clone() method cannot be called directly.` |
|      - |  665 | ` * $copy_of_object = clone $object;` |
|      - |  666 | ` * When an object is cloned, PHP 5 will perform a shallow copy of all of the object's properties.` |
|      - |  667 | ` * Any properties that are references to other variables, will remain references.` |
|      - |  668 | ` * Once the cloning is complete, if a __clone() method is defined, then the newly created object's __clone() method` |
|      - |  669 | ` * will be called, to allow any necessary properties that need to be changed.` |
|      - |  670 | ` * Example #1 Cloning an object` |
|      - |  671 | ` * <?php` |
|      - |  672 | ` * class SubObject` |
|      - |  673 | ` * {` |
|      - |  674 | ` *   static $instances = 0;` |
|      - |  675 | ` *   public $instance;` |
|      - |  676 | ` *` |
|      - |  677 | ` *   public function __construct() {` |
|      - |  678 | ` *       $this->instance = ++self::$instances;` |
|      - |  679 | ` *   }` |
|      - |  680 | ` *` |
|      - |  681 | ` *   public function __clone() {` |
|      - |  682 | ` *       $this->instance = ++self::$instances;` |
|      - |  683 | ` *   }` |
|      - |  684 | ` * }` |
|      - |  685 | ` *` |
|      - |  686 | ` * class MyCloneable` |
|      - |  687 | ` * {` |
|      - |  688 | ` *   public $object1;` |
|      - |  689 | ` *   public $object2;` |
|      - |  690 | ` *` |
|      - |  691 | ` *   function __clone()` |
|      - |  692 | ` *   {` |
|      - |  693 | ` *       // Force a copy of this->object, otherwise` |
|      - |  694 | ` *       // it will point to same object.` |
|      - |  695 | ` *       $this->object1 = clone $this->object1;` |
|      - |  696 | ` *   }` |
|      - |  697 | ` * }` |
|      - |  698 | ` * $obj = new MyCloneable();` |
|      - |  699 | ` * $obj->object1 = new SubObject();` |
|      - |  700 | ` * $obj->object2 = new SubObject();` |
|      - |  701 | ` * $obj2 = clone $obj;` |
|      - |  702 | ` * print("Original Object:\n");` |
|      - |  703 | ` * print_r($obj);` |
|      - |  704 | ` * print("Cloned Object:\n");` |
|      - |  705 | ` * print_r($obj2);` |
|      - |  706 | ` * ?>` |
|      - |  707 | ` * The above example will output:` |
|      - |  708 | ` * Original Object:` |
|      - |  709 | ` * MyCloneable Object` |
|      - |  710 | ` * (` |
|      - |  711 | ` *   [object1] => SubObject Object` |
|      - |  712 | ` *       (` |
|      - |  713 | ` *           [instance] => 1` |
|      - |  714 | ` *       )` |
|      - |  715 | ` *` |
|      - |  716 | ` *   [object2] => SubObject Object` |
|      - |  717 | ` *       (` |
|      - |  718 | ` *           [instance] => 2` |
|      - |  719 | ` *       )` |
|      - |  720 | ` *` |
|      - |  721 | ` * )` |
|      - |  722 | ` * Cloned Object:` |
|      - |  723 | ` * MyCloneable Object` |
|      - |  724 | ` * (` |
|      - |  725 | ` *   [object1] => SubObject Object` |
|      - |  726 | ` *       (` |
|      - |  727 | ` *           [instance] => 3` |
|      - |  728 | ` *       )` |
|      - |  729 | ` *` |
|      - |  730 | ` *   [object2] => SubObject Object` |
|      - |  731 | ` *       (` |
|      - |  732 | ` *           [instance] => 2` |
|      - |  733 | ` *       )` |
|      - |  734 | ` * )` |
|      - |  735 | ` */` |
|     44 |  736 | `PH7_PRIVATE ph7_class_instance * PH7_CloneClassInstance(ph7_class_instance *pSrc)` |
|      2 |  737 |  |
|      - |  738 | `	ph7_class_instance *pClone;` |
|      - |  739 | `	ph7_class_method *pMethod;` |
|      - |  740 | `	SyHashEntry *pEntry2;` |
|      - |  741 | `	SyHashEntry *pEntry;` |
|      - |  742 | `	ph7_vm *pVm;` |
|      - |  743 | `	sxi32 rc;` |
|      - |  744 | `	/* Allocate a new instance */` |
|     46 |  745 | `	pVm = pSrc->pVm;` |
|     46 |  746 | `	pClone = NewClassInstance(pVm,pSrc->pClass);` |
|     46 |  747 | `	if( pClone == 0 ){` |
|    ! 0 |  748 | `		return 0;` |
|      - |  749 | `	}` |
|      - |  750 | `	/* Associate a private VM frame with this class instance */` |
|     46 |  751 | `	rc = PH7_VmCreateClassInstanceFrame(pVm,pClone);` |
|     46 |  752 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  753 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClone);` |
|    ! 0 |  754 | `		return 0;` |
|      - |  755 | `	}` |
|      - |  756 | `	/* Duplicate object values */` |
|     46 |  757 | `	SyHashResetLoopCursor(&pSrc->hAttr);` |
|     46 |  758 | `	SyHashResetLoopCursor(&pClone->hAttr);` |
|    116 |  759 | `	while((pEntry = SyHashGetNextEntry(&pSrc->hAttr)) != 0 && (pEntry2 = SyHashGetNextEntry(&pClone->hAttr)) != 0 ){` |
|     50 |  760 | `		VmClassAttr *pSrcAttr = (VmClassAttr *)pEntry->pUserData;` |
|     50 |  761 | `		VmClassAttr *pDestAttr = (VmClassAttr *)pEntry2->pUserData;` |
|      - |  762 | `		/* Duplicate non-static attribute */` |
|     50 |  763 | `		if( (pSrcAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|      - |  764 | `			ph7_value *pvSrc,*pvDest;` |
|     50 |  765 | `			pvSrc = ExtractClassAttrValue(pVm,pSrcAttr);` |
|     50 |  766 | `			pvDest = ExtractClassAttrValue(pVm,pDestAttr);` |
|     50 |  767 | `			if( pvSrc && pvDest ){` |
|     50 |  768 | `				PH7_MemObjStore(pvSrc,pvDest);` |
|     24 |  769 | `			}` |
|     24 |  770 | `		}` |
|      2 |  771 | `	}` |
|      - |  772 | `	/* call the __clone method on the cloned object if available */` |
|     46 |  773 | `	pMethod = PH7_ClassExtractMethod(pClone->pClass,"__clone",sizeof("__clone")-1);` |
|     46 |  774 | `	if( pMethod ){` |
|     38 |  775 | `		if( pMethod->iCloneDepth < 16 ){` |
|     36 |  776 | `			pMethod->iCloneDepth++;` |
|     36 |  777 | `			PH7_VmCallClassMethod(pVm,pClone,pMethod,0,0,0);` |
|     19 |  778 | `		}else{` |
|      - |  779 | `			/* Nesting limit reached */` |
|      3 |  780 | `			PH7_VmThrowError(pVm,0,PH7_CTX_ERR,"Object clone limit reached,no more call to __clone()");` |
|      - |  781 | `		}` |
|      - |  782 | `		/* Reset the cursor */` |
|     38 |  783 | `		pMethod->iCloneDepth = 0;` |
|     18 |  784 | `	}` |
|      - |  785 | `	/* Return the cloned object */` |
|     46 |  786 | `	return pClone;` |
|     24 |  787 |  |
|      - |  788 | `#define CLASS_INSTANCE_DESTROYED 0x001 /* Instance is released */` |
|      - |  789 | `/*` |
|      - |  790 | ` * Release a class instance [i.e: Object in the PHP jargon] and invoke any defined destructor.` |
|      - |  791 | ` * This routine is invoked as soon as there are no other references to a particular` |
|      - |  792 | ` * class instance.` |
|      - |  793 | ` */` |
|   1628 |  794 | `static void PH7_ClassInstanceRelease(ph7_class_instance *pThis)` |
|      5 |  795 |  |
|      - |  796 | `	ph7_class_method *pDestr;` |
|      - |  797 | `	SyHashEntry *pEntry;` |
|      - |  798 | `	ph7_class *pClass;` |
|      - |  799 | `	ph7_vm *pVm;` |
|   1633 |  800 | `	if( pThis->iFlags & CLASS_INSTANCE_DESTROYED ){` |
|      - |  801 | `		/*` |
|      - |  802 | `		 * Already destroyed,return immediately.` |
|      - |  803 | `		 * This could happend if someone perform unset($this) in the destructor body.` |
|      - |  804 | `		 */` |
|      9 |  805 | `		return;` |
|      - |  806 | `	}` |
|      - |  807 | `	/* Mark as destroyed */` |
|   1625 |  808 | `	pThis->iFlags \|= CLASS_INSTANCE_DESTROYED;` |
|      - |  809 | `	/* Invoke any defined destructor if available */` |
|   1625 |  810 | `	pVm = pThis->pVm;` |
|   1625 |  811 | `	pClass = pThis->pClass;` |
|   1625 |  812 | `	pDestr = PH7_ClassExtractMethod(pClass,"__destruct",sizeof("__destruct")-1);` |
|   1625 |  813 | `	if( pDestr && !pVm->bInReset ){` |
|      - |  814 | `		/* Invoke the destructor. Skipped during ph7_vm_reset() bulk teardown:` |
|      - |  815 | `		 * running user PHP against a half-reset VM is unsafe (see bInReset). */` |
|     13 |  816 | `		pThis->iRef = 2; /* Prevent garbage collection */` |
|     13 |  817 | `		PH7_VmCallClassMethod(pVm,pThis,pDestr,0,0,0);` |
|      6 |  818 | `	}` |
|      - |  819 | `	/* Release non-static attributes */` |
|   1625 |  820 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|   7825 |  821 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|   6205 |  822 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|   6205 |  823 | `		if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|      - |  824 | `			/* Drop any typed-property enforcement slot registered for this` |
|      - |  825 | `			 * memobj. Must happen before the memobj is returned to the free` |
|      - |  826 | `			 * list so a future recycled slot does not inherit the stale entry. */` |
|   6187 |  827 | `			if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|    219 |  828 | `				SyHashDeleteEntry(&pVm->hTypedSlot,` |
|    144 |  829 | `					(const void *)&pVmAttr->nIdx,sizeof(sxu32),0);` |
|     72 |  830 | `			}` |
|   6187 |  831 | `			PH7_VmUnsetMemObj(pVm,pVmAttr->nIdx,TRUE);` |
|   3091 |  832 | `		}` |
|   6205 |  833 | `		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      5 |  834 | `	}` |
|      - |  835 | `	/* Release the whole structure */` |
|   1625 |  836 | `	SyHashRelease(&pThis->hAttr);` |
|   1625 |  837 | `	SyMemBackendPoolFree(&pVm->sAllocator,pThis);` |
|    819 |  838 |  |
|      - |  839 | `/*` |
|      - |  840 | ` * Decrement the reference count of a class instance [i.e Object in the PHP jargon].` |
|      - |  841 | ` * If the reference count reaches zero,release the whole instance.` |
|      - |  842 | ` */` |
|  29490 |  843 | `PH7_PRIVATE void PH7_ClassInstanceUnref(ph7_class_instance *pThis)` |
|      5 |  844 |  |
|  29495 |  845 | `	pThis->iRef--;` |
|  29495 |  846 | `	if( pThis->iRef < 1 ){` |
|      - |  847 | `		/* No more reference to this instance */` |
|   1633 |  848 | `		PH7_ClassInstanceRelease(&(*pThis));` |
|    814 |  849 | `	}` |
|  29495 |  850 |  |
|      - |  851 | `/*` |
|      - |  852 | ` * Compare two class instances [i.e: Objects in the PHP jargon]` |
|      - |  853 | ` * Note on objects comparison:` |
|      - |  854 | ` *  According to the PHP langauge reference manual` |
|      - |  855 | ` *  When using the comparison operator (==), object variables are compared in a simple manner` |
|      - |  856 | ` *  namely: Two object instances are equal if they have the same attributes and values, and are` |
|      - |  857 | ` *  instances of the same class.` |
|      - |  858 | ` *  On the other hand, when using the identity operator (===), object variables are identical` |
|      - |  859 | ` *  if and only if they refer to the same instance of the same class.` |
|      - |  860 | ` *  An example will clarify these rules.` |
|      - |  861 | ` *  Example #1 Example of object comparison` |
|      - |  862 | ` *  <?php` |
|      - |  863 | ` *    function bool2str($bool)` |
|      - |  864 | ` * {` |
|      - |  865 | ` *   if ($bool === false) {` |
|      - |  866 | ` *       return 'FALSE';` |
|      - |  867 | ` *   } else {` |
|      - |  868 | ` *       return 'TRUE';` |
|      - |  869 | ` *   }` |
|      - |  870 | ` * }` |
|      - |  871 | ` * function compareObjects(&$o1, &$o2)` |
|      - |  872 | ` * {` |
|      - |  873 | ` *   echo 'o1 == o2 : ' . bool2str($o1 == $o2) . "\n";` |
|      - |  874 | ` *   echo 'o1 != o2 : ' . bool2str($o1 != $o2) . "\n";` |
|      - |  875 | ` *   echo 'o1 === o2 : ' . bool2str($o1 === $o2) . "\n";` |
|      - |  876 | ` *   echo 'o1 !== o2 : ' . bool2str($o1 !== $o2) . "\n";` |
|      - |  877 | ` * }` |
|      - |  878 | ` * class Flag` |
|      - |  879 | ` * {` |
|      - |  880 | ` *   public $flag;` |
|      - |  881 | ` *` |
|      - |  882 | ` *   function Flag($flag = true) {` |
|      - |  883 | ` *       $this->flag = $flag;` |
|      - |  884 | ` *   }` |
|      - |  885 | ` * }` |
|      - |  886 | ` *` |
|      - |  887 | ` * class OtherFlag` |
|      - |  888 | ` * {` |
|      - |  889 | ` *   public $flag;` |
|      - |  890 | ` *` |
|      - |  891 | ` *   function OtherFlag($flag = true) {` |
|      - |  892 | ` *       $this->flag = $flag;` |
|      - |  893 | ` *   }` |
|      - |  894 | ` * }` |
|      - |  895 | ` *` |
|      - |  896 | ` * $o = new Flag();` |
|      - |  897 | ` * $p = new Flag();` |
|      - |  898 | ` * $q = $o;` |
|      - |  899 | ` * $r = new OtherFlag();` |
|      - |  900 | ` *` |
|      - |  901 | ` * echo "Two instances of the same class\n";` |
|      - |  902 | ` * compareObjects($o, $p);` |
|      - |  903 | ` * echo "\nTwo references to the same instance\n";` |
|      - |  904 | ` * compareObjects($o, $q);` |
|      - |  905 | ` * echo "\nInstances of two different classes\n";` |
|      - |  906 | ` * compareObjects($o, $r);` |
|      - |  907 | ` * ?>` |
|      - |  908 | ` * The above example will output:` |
|      - |  909 | ` * Two instances of the same class` |
|      - |  910 | ` * o1 == o2 : TRUE` |
|      - |  911 | ` * o1 != o2 : FALSE` |
|      - |  912 | ` * o1 === o2 : FALSE` |
|      - |  913 | ` * o1 !== o2 : TRUE` |
|      - |  914 | ` * Two references to the same instance` |
|      - |  915 | ` * o1 == o2 : TRUE` |
|      - |  916 | ` * o1 != o2 : FALSE` |
|      - |  917 | ` * o1 === o2 : TRUE` |
|      - |  918 | ` * o1 !== o2 : FALSE` |
|      - |  919 | ` * Instances of two different classes` |
|      - |  920 | ` * o1 == o2 : FALSE` |
|      - |  921 | ` * o1 != o2 : TRUE` |
|      - |  922 | ` * o1 === o2 : FALSE` |
|      - |  923 | ` * o1 !== o2 : TRUE` |
|      - |  924 | ` *` |
|      - |  925 | ` * This function return 0 if the objects are equals according to the comprison rules defined above.` |
|      - |  926 | ` * Any other return values indicates difference.` |
|      - |  927 | ` */` |
|    174 |  928 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest)` |
|      3 |  929 |  |
|      - |  930 | `	SyHashEntry *pEntry,*pEntry2;` |
|      - |  931 | `	ph7_value sV1,sV2;` |
|      - |  932 | `	sxi32 rc;` |
|    177 |  933 | `	if( iNest > 31 ){` |
|      - |  934 | `		/* Nesting limit reached */` |
|      6 |  935 | `		PH7_VmThrowError(pLeft->pVm,0,PH7_CTX_ERR,"Nesting limit reached: Infinite recursion?");` |
|      6 |  936 | `		return 1;` |
|      - |  937 | `	}` |
|      - |  938 | `	/* Comparison is performed only if the objects are instance of the same class */` |
|    173 |  939 | `	if( pLeft->pClass != pRight->pClass ){` |
|      7 |  940 | `		return 1;` |
|      - |  941 | `	}` |
|    167 |  942 | `	if( bStrict ){` |
|      - |  943 | `		/*` |
|      - |  944 | `		 * According to the PHP language reference manual:` |
|      - |  945 | `		 *  when using the identity operator (===), object variables` |
|      - |  946 | `		 *  are identical if and only if they refer to the same instance` |
|      - |  947 | `		 *  of the same class.` |
|      - |  948 | `		 */` |
|     25 |  949 | `		return !(pLeft == pRight);` |
|      - |  950 | `	}` |
|      - |  951 | `	/*` |
|      - |  952 | `	 * Attribute comparison.` |
|      - |  953 | `	 * According to the PHP reference manual:` |
|      - |  954 | `	 *  When using the comparison operator (==), object variables are compared` |
|      - |  955 | `	 *  in a simple manner, namely: Two object instances are equal if they have` |
|      - |  956 | `	 *  the same attributes and values, and are instances of the same class.` |
|      - |  957 | `	 */` |
|    143 |  958 | `	if( pLeft == pRight ){` |
|      - |  959 | `		/* Same instance,don't bother processing,object are equals */` |
|      3 |  960 | `		return 0;` |
|      - |  961 | `	}` |
|    141 |  962 | `	SyHashResetLoopCursor(&pLeft->hAttr);` |
|    141 |  963 | `	SyHashResetLoopCursor(&pRight->hAttr);` |
|    141 |  964 | `	PH7_MemObjInit(pLeft->pVm,&sV1);` |
|    141 |  965 | `	PH7_MemObjInit(pLeft->pVm,&sV2);` |
|    141 |  966 | `	sV1.nIdx = sV2.nIdx = SXU32_HIGH;` |
|    224 |  967 | `	while((pEntry = SyHashGetNextEntry(&pLeft->hAttr)) != 0 && (pEntry2 = SyHashGetNextEntry(&pRight->hAttr)) != 0 ){` |
|    147 |  968 | `		VmClassAttr *p1 = (VmClassAttr *)pEntry->pUserData;` |
|    147 |  969 | `		VmClassAttr *p2 = (VmClassAttr *)pEntry2->pUserData;` |
|      - |  970 | `		/* Compare only non-static attribute */` |
|    147 |  971 | `		if( (p1->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|      - |  972 | `			ph7_value *pL,*pR;` |
|    147 |  973 | `			pL = ExtractClassAttrValue(pLeft->pVm,p1);` |
|    147 |  974 | `			pR = ExtractClassAttrValue(pRight->pVm,p2);` |
|    147 |  975 | `			if( pL && pR ){` |
|    147 |  976 | `				PH7_MemObjLoad(pL,&sV1);` |
|    147 |  977 | `				PH7_MemObjLoad(pR,&sV2);` |
|      - |  978 | `				/* Compare the two values now */` |
|    147 |  979 | `				rc = PH7_MemObjCmp(&sV1,&sV2,bStrict,iNest+1);` |
|    147 |  980 | `				PH7_MemObjRelease(&sV1);` |
|    147 |  981 | `				PH7_MemObjRelease(&sV2);` |
|    147 |  982 | `				if( rc != 0 ){` |
|      - |  983 | `					/* Not equals */` |
|    133 |  984 | `					return rc;` |
|      - |  985 | `				}` |
|      7 |  986 | `			}` |
|      7 |  987 | `		}` |
|      1 |  988 | `	}` |
|      - |  989 | `	/* Object are equals */` |
|      9 |  990 | `	return 0;` |
|     90 |  991 |  |
|      - |  992 | `/*` |
|      - |  993 | ` * Dump a class instance and the store the dump in the BLOB given` |
|      - |  994 | ` * as the first argument.` |
|      - |  995 | ` * Note that only non-static/non-constants attribute are dumped.` |
|      - |  996 | ` * This function is typically invoked when the user issue a call` |
|      - |  997 | ` * to [var_dump(),var_export(),print_r(),...].` |
|      - |  998 | ` * This function SXRET_OK on success. Any other return value including` |
|      - |  999 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|      - | 1000 | ` */` |
|    132 | 1001 | `PH7_PRIVATE sxi32 PH7_ClassInstanceDump(SyBlob *pOut,ph7_class_instance *pThis,int ShowType,int nTab,int nDepth)` |
|      1 | 1002 |  |
|      - | 1003 | `	SyHashEntry *pEntry;` |
|      - | 1004 | `	ph7_value *pValue;` |
|      - | 1005 | `	sxi32 rc;` |
|      - | 1006 | `	int i;` |
|    133 | 1007 | `	if( nDepth > 31 ){` |
|      - | 1008 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|      - | 1009 | `		/* Nesting limit reached..halt immediately*/` |
|      5 | 1010 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|      5 | 1011 | `		if( ShowType ){` |
|      5 | 1012 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|      2 | 1013 | `		}` |
|      5 | 1014 | `		return SXERR_LIMIT;` |
|      - | 1015 | `	}` |
|    129 | 1016 | `	rc = SXRET_OK;` |
|    129 | 1017 | `	if( !ShowType ){` |
|      3 | 1018 | `		SyBlobAppend(&(*pOut),"Object(",sizeof("Object(")-1);` |
|      1 | 1019 | `	}` |
|      - | 1020 | `	/* Append class name */` |
|    129 | 1021 | `	SyBlobFormat(&(*pOut),"%z) {",&pThis->pClass->sName);` |
|      - | 1022 | `#ifdef __WINNT__` |
|      1 | 1023 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|      - | 1024 | `#else` |
|    128 | 1025 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      - | 1026 | `#endif` |
|      - | 1027 | `	/* Dump object attributes */` |
|    129 | 1028 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|    201 | 1029 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|    133 | 1030 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    133 | 1031 | `		if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|      - | 1032 | `			/* Dump non-static/constant attribute only */` |
|   3985 | 1033 | `			for( i = 0 ; i < nTab ; i++ ){` |
|   3853 | 1034 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|   1927 | 1035 | `			}` |
|    133 | 1036 | `			pValue = ExtractClassAttrValue(pThis->pVm,pVmAttr);` |
|    133 | 1037 | `			if( pValue ){` |
|    133 | 1038 | `				SyBlobFormat(&(*pOut),"['%z'] =>",&pVmAttr->pAttr->sName);` |
|      - | 1039 | `#ifdef __WINNT__` |
|      1 | 1040 | `				SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|      - | 1041 | `#else` |
|    132 | 1042 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      - | 1043 | `#endif` |
|    133 | 1044 | `				rc = PH7_MemObjDump(&(*pOut),pValue,ShowType,nTab+1,nDepth,0);` |
|    133 | 1045 | `				if( rc == SXERR_LIMIT ){` |
|    125 | 1046 | `					break;` |
|      - | 1047 | `				}` |
|      4 | 1048 | `			}` |
|      4 | 1049 | `		}` |
|      1 | 1050 | `	}` |
|   3977 | 1051 | `	for( i = 0 ; i < nTab ; i++ ){` |
|   3849 | 1052 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|   1925 | 1053 | `	}` |
|    129 | 1054 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|    129 | 1055 | `	return rc;` |
|     67 | 1056 |  |
|      - | 1057 | `/*` |
|      - | 1058 | ` * Call a magic method [i.e: __toString(),__toBool(),__Invoke()...]` |
|      - | 1059 | ` * Return SXRET_OK on successfull call. Any other return value indicates failure.` |
|      - | 1060 | ` * Notes on magic methods.` |
|      - | 1061 | ` * According to the PHP language reference manual.` |
|      - | 1062 | ` *  The function names __construct(), __destruct(), __call(), __callStatic()` |
|      - | 1063 | ` *  __get(),  __toString(), __invoke(), __clone() are magical in PHP classes.` |
|      - | 1064 | ` * You cannot have functions with these names in any of your classes unless` |
|      - | 1065 | ` * you want the magic functionality associated with them.` |
|      - | 1066 | ` * Example of magical methods:` |
|      - | 1067 | ` * __toString()` |
|      - | 1068 | ` *  The __toString() method allows a class to decide how it will react when it is treated like` |
|      - | 1069 | ` *  a string. For example, what echo $obj; will print. This method must return a string.` |
|      - | 1070 | ` *  Example #2 Simple example` |
|      - | 1071 | ` * <?php` |
|      - | 1072 | ` * // Declare a simple class` |
|      - | 1073 | ` * class TestClass` |
|      - | 1074 | ` * {` |
|      - | 1075 | ` *   public $foo;` |
|      - | 1076 | ` *` |
|      - | 1077 | ` *   public function __construct($foo)` |
|      - | 1078 | ` *   {` |
|      - | 1079 | ` *       $this->foo = $foo;` |
|      - | 1080 | ` *   }` |
|      - | 1081 | ` *` |
|      - | 1082 | ` *   public function __toString()` |
|      - | 1083 | ` *   {` |
|      - | 1084 | ` *       return $this->foo;` |
|      - | 1085 | ` *   }` |
|      - | 1086 | ` * }` |
|      - | 1087 | ` * $class = new TestClass('Hello');` |
|      - | 1088 | ` * echo $class;` |
|      - | 1089 | ` * ?>` |
|      - | 1090 | ` * The above example will output:` |
|      - | 1091 | ` *  Hello` |
|      - | 1092 | ` *` |
|      - | 1093 | ` * Note that PH7 does not support all the magical method and introudces __toFloat(),__toInt()` |
|      - | 1094 | ` * which have the same behaviour as __toString() but for float and integer types` |
|      - | 1095 | ` * respectively.` |
|      - | 1096 | ` * Refer to the official documentation for more information.` |
|      - | 1097 | ` */` |
|      2 | 1098 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallMagicMethod(` |
|      - | 1099 | `	ph7_vm *pVm,               /* VM that own all this stuff */` |
|      - | 1100 | `	ph7_class *pClass,         /* Target class */` |
|      - | 1101 | `	ph7_class_instance *pThis, /* Target object */` |
|      - | 1102 | `	const char *zMethod,       /* Magic method name [i.e: __toString()]*/` |
|      - | 1103 | `	sxu32 nByte,               /* zMethod length*/` |
|      - | 1104 | `	const SyString *pAttrName  /* Attribute name */` |
|      - | 1105 | `	)` |
|      1 | 1106 |  |
|      3 | 1107 | `	ph7_value *apArg[2] = { 0 , 0 };` |
|      - | 1108 | `	ph7_class_method *pMeth;` |
|      - | 1109 | `	ph7_value sAttr; /* cc warning */` |
|      - | 1110 | `	sxi32 rc;` |
|      - | 1111 | `	int nArg;` |
|      - | 1112 | `	/* Make sure the magic method is available */` |
|      3 | 1113 | `	pMeth = PH7_ClassExtractMethod(&(*pClass),zMethod,nByte);` |
|      3 | 1114 | `	if( pMeth == 0 ){` |
|      - | 1115 | `		/* No such method,return immediately */` |
|      3 | 1116 | `		return SXERR_NOTFOUND;` |
|      - | 1117 | `	}` |
|    ! 0 | 1118 | `	nArg = 0;` |
|      - | 1119 | `	/* Copy arguments */` |
|    ! 0 | 1120 | `	if( pAttrName ){` |
|    ! 0 | 1121 | `		PH7_MemObjInitFromString(pVm,&sAttr,pAttrName);` |
|    ! 0 | 1122 | `		sAttr.nIdx = SXU32_HIGH; /* Mark as constant */` |
|    ! 0 | 1123 | `		apArg[0] = &sAttr;` |
|    ! 0 | 1124 | `		nArg = 1;` |
|    ! 0 | 1125 | `	}` |
|      - | 1126 | `	/* Call the magic method now */` |
|    ! 0 | 1127 | `	rc = PH7_VmCallClassMethod(pVm,&(*pThis),pMeth,0,nArg,apArg);` |
|      - | 1128 | `	/* Clean up */` |
|    ! 0 | 1129 | `	if( pAttrName ){` |
|    ! 0 | 1130 | `		PH7_MemObjRelease(&sAttr);` |
|    ! 0 | 1131 | `	}` |
|    ! 0 | 1132 | `	return rc;` |
|      2 | 1133 |  |
|      - | 1134 | `/*` |
|      - | 1135 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon].` |
|      - | 1136 | ` * This function is simply a wrapper on ExtractClassAttrValue().` |
|      - | 1137 | ` */` |
|     22 | 1138 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceExtractAttrValue(ph7_class_instance *pThis,VmClassAttr *pAttr)` |
|      2 | 1139 |  |
|      - | 1140 | `   /* Extract the attribute value */` |
|      - | 1141 | `	ph7_value *pValue;` |
|     24 | 1142 | `	pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     24 | 1143 | `	return pValue;` |
|      2 | 1144 |  |
|      - | 1145 | `/*` |
|      - | 1146 | ` * Convert a class instance [i.e: Object in the PHP jargon] into a hashmap [i.e: array in the PHP jargon].` |
|      - | 1147 | ` * Return SXRET_OK on success. Any other value indicates failure.` |
|      - | 1148 | ` * Note on object conversion to array:` |
|      - | 1149 | ` *  Acccording to the PHP language reference manual` |
|      - | 1150 | ` *  If an object is converted to an array, the result is an array whose elements are the object's properties.` |
|      - | 1151 | ` *  The keys are the member variable names.` |
|      - | 1152 | ` *` |
|      - | 1153 | ` *  The following example:` |
|      - | 1154 | ` *  class Test {` |
|      - | 1155 | ` *   public $A = 25<<1;  // 50` |
|      - | 1156 | ` *	 public $c = rand_str(3);   // Random string of length 3` |
|      - | 1157 | ` *	 public $d = rand() & 1023; // Random number between 0..1023` |
|      - | 1158 | ` *  }` |
|      - | 1159 | ` *  var_dump((array) new Test());` |
|      - | 1160 | ` *	Will output:` |
|      - | 1161 | ` *  array(3) {` |
|      - | 1162 | ` *   [A] =>` |
|      - | 1163 | ` *      int(50)` |
|      - | 1164 | ` *   [c] =>` |
|      - | 1165 | ` *     string(3 'aps')` |
|      - | 1166 | ` *   [d] =>` |
|      - | 1167 | ` *     int(991)` |
|      - | 1168 | ` *  }` |
|      - | 1169 | ` * You have noticed that PH7 allow class attributes [i.e: $a,$c,$d in the example above]` |
|      - | 1170 | ` * have any complex expression (even function calls/Annonymous functions) as their default` |
|      - | 1171 | ` * value unlike the standard PHP engine.` |
|      - | 1172 | ` * This is a very powerful feature that you have to look at.` |
|      - | 1173 | ` */` |
|      6 | 1174 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|      1 | 1175 |  |
|      - | 1176 | `	SyHashEntry *pEntry;` |
|      - | 1177 | `	SyString *pAttrName;` |
|      - | 1178 | `	VmClassAttr *pAttr;` |
|      - | 1179 | `	ph7_value *pValue;` |
|      - | 1180 | `	ph7_value sName;` |
|      - | 1181 | `	/* Reset the loop cursor */` |
|      7 | 1182 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      7 | 1183 | `	PH7_MemObjInitFromString(pThis->pVm,&sName,0);` |
|     20 | 1184 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|      - | 1185 | `		/* Point to the current attribute */` |
|     11 | 1186 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1187 | `		/* Extract attribute value */` |
|     11 | 1188 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     11 | 1189 | `		if( pValue ){` |
|      - | 1190 | `			/* Build attribute name */` |
|     11 | 1191 | `			pAttrName = &pAttr->pAttr->sName;` |
|     11 | 1192 | `			PH7_MemObjStringAppend(&sName,pAttrName->zString,pAttrName->nByte);` |
|      - | 1193 | `			/* Perform the insertion */` |
|     11 | 1194 | `			PH7_HashmapInsert(pMap,&sName,pValue);` |
|      - | 1195 | `			/* Reset the string cursor */` |
|     11 | 1196 | `			SyBlobReset(&sName.sBlob);` |
|      5 | 1197 | `		}` |
|      1 | 1198 | `	}` |
|      7 | 1199 | `	PH7_MemObjRelease(&sName);` |
|      7 | 1200 | `	return SXRET_OK;` |
|      1 | 1201 |  |
|      - | 1202 | `/*` |
|      - | 1203 | ` * Iterate throw class attributes and invoke the given callback [i.e: xWalk()] for each` |
|      - | 1204 | ` * retrieved attribute.` |
|      - | 1205 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|      - | 1206 | ` * the attribute value in the callback body will not alter the real attribute value.` |
|      - | 1207 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|      - | 1208 | ` * a value different from PH7_OK.` |
|      - | 1209 | ` * Refer to [ph7_object_walk()] for more information.` |
|      - | 1210 | ` */` |
|      2 | 1211 | `PH7_PRIVATE sxi32 PH7_ClassInstanceWalk(` |
|      - | 1212 | `	ph7_class_instance *pThis, /* Target object */` |
|      - | 1213 | `	int (*xWalk)(const char *,ph7_value *,void *), /* Walker callback */` |
|      - | 1214 | `	void *pUserData /* Last argument to xWalk() */` |
|      - | 1215 | `	)` |
|      1 | 1216 |  |
|      - | 1217 | `	SyHashEntry *pEntry; /* Hash entry */` |
|      - | 1218 | `	VmClassAttr *pAttr;  /* Pointer to the attribute */` |
|      - | 1219 | `	ph7_value *pValue;   /* Attribute value */` |
|      - | 1220 | `	ph7_value sValue;    /* Copy of the attribute value */` |
|      - | 1221 | `	int rc;` |
|      - | 1222 | `	/* Reset the loop cursor */` |
|      3 | 1223 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      3 | 1224 | `	PH7_MemObjInit(pThis->pVm,&sValue);` |
|      - | 1225 | `	/* Start the walk process */` |
|      8 | 1226 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|      - | 1227 | `		/* Point to the current attribute */` |
|      5 | 1228 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1229 | `		/* Extract attribute value */` |
|      5 | 1230 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|      5 | 1231 | `		if( pValue ){` |
|      5 | 1232 | `			PH7_MemObjLoad(pValue,&sValue);` |
|      - | 1233 | `			/* Invoke the supplied callback */` |
|      5 | 1234 | `			rc =  xWalk(SyStringData(&pAttr->pAttr->sName),&sValue,pUserData);` |
|      5 | 1235 | `			PH7_MemObjRelease(&sValue);` |
|      5 | 1236 | `			if( rc != PH7_OK){` |
|      - | 1237 | `				/* User callback request an operation abort */` |
|    ! 0 | 1238 | `				return SXERR_ABORT;` |
|      - | 1239 | `			}` |
|      2 | 1240 | `		}` |
|      1 | 1241 | `	}` |
|      - | 1242 | `	/* All done */` |
|      3 | 1243 | `	return SXRET_OK;` |
|      2 | 1244 |  |
|      - | 1245 | `/*` |
|      - | 1246 | ` * Extract a class atrribute value.` |
|      - | 1247 | ` * Return a pointer to the attribute value on success. Otherwise NULL.` |
|      - | 1248 | ` * Note:` |
|      - | 1249 | ` *  Access to static and constant attribute is not allowed. That is,the function` |
|      - | 1250 | ` *  will return NULL in case someone (host-application code) try to extract` |
|      - | 1251 | ` *  a static/constant attribute.` |
|      - | 1252 | ` */` |
|    718 | 1253 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceFetchAttr(ph7_class_instance *pThis,const SyString *pName)` |
|      5 | 1254 |  |
|      - | 1255 | `	SyHashEntry *pEntry;` |
|      - | 1256 | `	VmClassAttr *pAttr;` |
|      - | 1257 | `	/* Query the attribute hashtable */` |
|    723 | 1258 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);` |
|    723 | 1259 | `	if( pEntry == 0 ){` |
|      - | 1260 | `		/* No such attribute */` |
|    ! 0 | 1261 | `		return 0;` |
|      - | 1262 | `	}` |
|      - | 1263 | `	/* Point to the class atrribute */` |
|    723 | 1264 | `	pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1265 | `	/* Check if we are dealing with a static/constant attribute */` |
|    723 | 1266 | `	if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|      - | 1267 | `		/* Access is forbidden */` |
|    ! 0 | 1268 | `		return 0;` |
|      - | 1269 | `	}` |
|      - | 1270 | `	/* Return the attribute value */` |
|    723 | 1271 | `	return ExtractClassAttrValue(pThis->pVm,pAttr);` |
|    364 | 1272 |  |
|      - | 1273 |  |
