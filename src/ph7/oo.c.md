# src/ph7/oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 412/478 lines (86.19%)

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
|  29326 |   14 | `PH7_PRIVATE ph7_class * PH7_NewRawClass(ph7_vm *pVm,const SyString *pName,sxu32 nLine)` |
|      2 |   15 |  |
|      - |   16 | `	ph7_class *pClass;` |
|      - |   17 | `	char *zName;` |
|      - |   18 | `	/* Allocate a new instance */` |
|  29328 |   19 | `	pClass = (ph7_class *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class));` |
|  29328 |   20 | `	if( pClass == 0 ){` |
|    ! 0 |   21 | `		return 0;` |
|      - |   22 | `	}` |
|      - |   23 | `	/* Zero the structure */` |
|  29328 |   24 | `	SyZero(pClass,sizeof(ph7_class));` |
|      - |   25 | `	/* Duplicate class name */` |
|  29328 |   26 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  29328 |   27 | `	if( zName == 0 ){` |
|    ! 0 |   28 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClass);` |
|    ! 0 |   29 | `		return 0;` |
|      - |   30 | `	}` |
|      - |   31 | `	/* Initialize fields */` |
|  29328 |   32 | `	SyStringInitFromBuf(&pClass->sName,zName,pName->nByte);` |
|  29328 |   33 | `	SyHashInit(&pClass->hMethod,&pVm->sAllocator,0,0);` |
|  29328 |   34 | `	SyHashInit(&pClass->hAttr,&pVm->sAllocator,0,0);` |
|  29328 |   35 | `	SyHashInit(&pClass->hDerived,&pVm->sAllocator,0,0);` |
|  29328 |   36 | `	SySetInit(&pClass->aInterface,&pVm->sAllocator,sizeof(ph7_class *));` |
|  29328 |   37 | `	SySetInit(&pClass->aTrait,&pVm->sAllocator,sizeof(ph7_class *));` |
|  29328 |   38 | `	pClass->nLine = nLine;` |
|      - |   39 | `	/* All done */` |
|  29328 |   40 | `	return pClass;` |
|  14665 |   41 |  |
|      - |   42 | `/*` |
|      - |   43 | ` * Allocate and initialize a new class attribute.` |
|      - |   44 | ` * Return a pointer to the class attribute on success. NULL otherwise.` |
|      - |   45 | ` */` |
|  24320 |   46 | `PH7_PRIVATE ph7_class_attr * PH7_NewClassAttr(ph7_vm *pVm,const SyString *pName,sxu32 nLine,sxi32 iProtection,sxi32 iFlags)` |
|      2 |   47 |  |
|      - |   48 | `	ph7_class_attr *pAttr;` |
|      - |   49 | `	char *zName;` |
|  24322 |   50 | `	pAttr = (ph7_class_attr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_attr));` |
|  24322 |   51 | `	if( pAttr == 0 ){` |
|    ! 0 |   52 | `		return 0;` |
|      - |   53 | `	}` |
|      - |   54 | `	/* Zero the structure */` |
|  24322 |   55 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|      - |   56 | `	/* Duplicate attribute name */` |
|  24322 |   57 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  24322 |   58 | `	if( zName == 0 ){` |
|    ! 0 |   59 | `		SyMemBackendPoolFree(&pVm->sAllocator,pAttr);` |
|    ! 0 |   60 | `		return 0;` |
|      - |   61 | `	}` |
|      - |   62 | `	/* Initialize fields */` |
|  24322 |   63 | `	SySetInit(&pAttr->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|  24322 |   64 | `	SyStringInitFromBuf(&pAttr->sName,zName,pName->nByte);` |
|  24322 |   65 | `	pAttr->iProtection = iProtection;` |
|  24322 |   66 | `	pAttr->nIdx = SXU32_HIGH;` |
|  24322 |   67 | `	pAttr->iFlags = iFlags;` |
|  24322 |   68 | `	pAttr->nLine = nLine;` |
|  24322 |   69 | `	return pAttr;` |
|  12162 |   70 |  |
|      - |   71 | `/*` |
|      - |   72 | ` * Allocate and initialize a new class method.` |
|      - |   73 | ` * Return a pointer to the class method on success. NULL otherwise` |
|      - |   74 | ` * This function associate with the newly created method an automatically generated` |
|      - |   75 | ` * random unique name.` |
|      - |   76 | ` */` |
|  70278 |   77 | `PH7_PRIVATE ph7_class_method * PH7_NewClassMethod(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,sxu32 nLine,` |
|      - |   78 | `	sxi32 iProtection,sxi32 iFlags,sxi32 iFuncFlags)` |
|      2 |   79 |  |
|      - |   80 | `	ph7_class_method *pMeth;` |
|      - |   81 | `	SyHashEntry *pEntry;` |
|      - |   82 | `	SyString *pNamePtr;` |
|      - |   83 | `	char zSalt[10];` |
|      - |   84 | `	char *zName;` |
|      - |   85 | `	sxu32 nByte;` |
|      - |   86 | `	/* Allocate a new class method instance */` |
|  70280 |   87 | `	pMeth = (ph7_class_method *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_method));` |
|  70280 |   88 | `	if( pMeth == 0 ){` |
|    ! 0 |   89 | `		return 0;` |
|      - |   90 | `	}` |
|      - |   91 | `	/* Zero the structure */` |
|  70280 |   92 | `	SyZero(pMeth,sizeof(ph7_class_method));` |
|      - |   93 | `	/* Check for an already installed method with the same name */` |
|  70280 |   94 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
|  70280 |   95 | `	if( pEntry == 0 ){` |
|      - |   96 | `		/* Associate an unique VM name to this method */` |
|  70278 |   97 | `		nByte = sizeof(zSalt) + pName->nByte + SyStringLength(&pClass->sName)+sizeof(char)*7/*[[__'\0'*/;` |
|  70278 |   98 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,nByte);` |
|  70278 |   99 | `		if( zName == 0 ){` |
|    ! 0 |  100 | `			SyMemBackendPoolFree(&pVm->sAllocator,pMeth);` |
|    ! 0 |  101 | `			return 0;` |
|      - |  102 | `		}` |
|  70278 |  103 | `		pNamePtr = &pMeth->sVmName;` |
|      - |  104 | `		/* Generate a random string */` |
|  70278 |  105 | `		PH7_VmRandomString(&(*pVm),zSalt,sizeof(zSalt));` |
|  70278 |  106 | `		pNamePtr->nByte = SyBufferFormat(zName,nByte,"[__%z@%z_%.*s]",&pClass->sName,pName,sizeof(zSalt),zSalt);` |
|  70278 |  107 | `		pNamePtr->zString = zName;` |
|  35140 |  108 | `	}else{` |
|      - |  109 | `		/* Method is condidate for 'overloading' */` |
|      3 |  110 | `		ph7_class_method *pCurrent = (ph7_class_method *)pEntry->pUserData;` |
|      3 |  111 | `		pNamePtr = &pMeth->sVmName;` |
|      - |  112 | `		/* Use the same VM name */` |
|      3 |  113 | `		SyStringDupPtr(pNamePtr,&pCurrent->sVmName);` |
|      3 |  114 | `		zName = (char *)pNamePtr->zString;` |
|      - |  115 | `	}` |
|  70280 |  116 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|     23 |  117 | `		if( (pName->nByte == sizeof("__construct") - 1 && SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1 ) == 0)` |
|     15 |  118 | `			\|\| (pName->nByte == sizeof("__destruct") - 1 && SyMemcmp(pName->zString,"__destruct",sizeof("__destruct") - 1 ) == 0)` |
|     15 |  119 | `			\|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|      - |  120 | `				/* Switch to public visibility when dealing with constructor/destructor */` |
|      5 |  121 | `				iProtection = PH7_CLASS_PROT_PUBLIC;` |
|      2 |  122 | `		}` |
|      9 |  123 | `	}` |
|      - |  124 | `	/* Initialize method fields */` |
|  70282 |  125 | `	pMeth->iProtection = iProtection;` |
|  70282 |  126 | `	pMeth->iFlags = iFlags;` |
|  70282 |  127 | `	pMeth->nLine = nLine;` |
| 105423 |  128 | `	PH7_VmInitFuncState(&(*pVm),&pMeth->sFunc,&zName[sizeof(char)*4/*[__@*/+SyStringLength(&pClass->sName)],` |
|  70280 |  129 | `		pName->nByte,iFuncFlags\|VM_FUNC_CLASS_METHOD,pClass);` |
|  70282 |  130 | `	return pMeth;` |
|  35143 |  131 |  |
|      - |  132 | `/*` |
|      - |  133 | ` * Check if the given name have a class method associated with it.` |
|      - |  134 | ` * Return the desired method [i.e: ph7_class_method instance] on success. NULL otherwise.` |
|      - |  135 | ` */` |
|   4694 |  136 | `PH7_PRIVATE ph7_class_method * PH7_ClassExtractMethod(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|      2 |  137 |  |
|      - |  138 | `	SyHashEntry *pEntry;` |
|      - |  139 | `	/* Perform a hash lookup */` |
|   4696 |  140 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)zName,nByte);` |
|   4696 |  141 | `	if( pEntry == 0 ){` |
|      - |  142 | `		/* No such entry */` |
|   1816 |  143 | `		return 0;` |
|      - |  144 | `	}` |
|      - |  145 | `	/* Point to the desired method */` |
|   2882 |  146 | `	return (ph7_class_method *)pEntry->pUserData;` |
|   2349 |  147 |  |
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
|  24320 |  168 | `PH7_PRIVATE sxi32 PH7_ClassInstallAttr(ph7_class *pClass,ph7_class_attr *pAttr)` |
|      2 |  169 |  |
|  24322 |  170 | `	SyString *pName = &pAttr->sName;` |
|      - |  171 | `	sxi32 rc;` |
|  24322 |  172 | `	rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|  24322 |  173 | `	return rc;` |
|      2 |  174 |  |
|      - |  175 | `/*` |
|      - |  176 | ` * Install a class method in the corresponding container.` |
|      - |  177 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|      - |  178 | ` */` |
|  70276 |  179 | `PH7_PRIVATE sxi32 PH7_ClassInstallMethod(ph7_class *pClass,ph7_class_method *pMeth)` |
|      2 |  180 |  |
|  70278 |  181 | `	SyString *pName = &pMeth->sFunc.sName;` |
|      - |  182 | `	sxi32 rc;` |
|  70278 |  183 | `	rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|  70278 |  184 | `	return rc;` |
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
|  14522 |  227 | `PH7_PRIVATE sxi32 PH7_ClassInherit(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase)` |
|      2 |  228 |  |
|      - |  229 | `	ph7_class_method *pMeth;` |
|      - |  230 | `	ph7_class_attr *pAttr;` |
|      - |  231 | `	SyHashEntry *pEntry;` |
|      - |  232 | `	SyString *pName;` |
|      - |  233 | `	sxi32 rc;` |
|      - |  234 | `	/* Install in the derived hashtable */` |
|  14524 |  235 | `	rc = SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|  14524 |  236 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  237 | `		return rc;` |
|      - |  238 | `	}` |
|      - |  239 | `	/* Copy public/protected attributes from the base class */` |
|  14524 |  240 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
| 101380 |  241 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|      - |  242 | `		/* Make sure the private attributes are not redeclared in the subclass */` |
|  86858 |  243 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  86858 |  244 | `		pName = &pAttr->sName;` |
|  86858 |  245 | `		if( (pEntry = SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte)) != 0 ){` |
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
|  86856 |  257 | `		if( pAttr->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
|  86854 |  258 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|  86854 |  259 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  260 | `				return rc;` |
|      - |  261 | `			}` |
|  43426 |  262 | `		}` |
|      2 |  263 | `	}` |
|  14524 |  264 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
| 144864 |  265 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|      - |  266 | `		/* Make sure the private/final methods are not redeclared in the subclass */` |
| 130342 |  267 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
| 130342 |  268 | `		pName = &pMeth->sFunc.sName;` |
| 130342 |  269 | `		if( (pEntry = SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|   2440 |  270 | `			 if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|      - |  271 | `				/* Cannot Overwrite final method */` |
|      7 |  272 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_method *)pEntry->pUserData)->nLine,` |
|      - |  273 | `					"Cannot Overwrite final method '%z:%z' inside child class '%z'",` |
|      2 |  274 | `					&pBase->sName,pName,&pSub->sName);` |
|      5 |  275 | `				if( rc == SXERR_ABORT ){` |
|    ! 0 |  276 | `					return SXERR_ABORT;` |
|      - |  277 | `				}` |
|      2 |  278 | `			}` |
|   2440 |  279 | `			continue;` |
|    ! 0 |  280 | `		}else{` |
| 127904 |  281 | `			if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      - |  282 | `				/* Abstract method must be defined in the child class */` |
|      4 |  283 | `				PH7_GenCompileError(&(*pGen),E_WARNING,pMeth->nLine,` |
|      - |  284 | `					"Abstract method '%z:%z' must be defined inside child class '%z'",` |
|      1 |  285 | `					&pBase->sName,pName,&pSub->sName);` |
|      3 |  286 | `				continue;` |
|      - |  287 | `			}` |
|      - |  288 | `		}` |
|      - |  289 | `		/* Install the method */` |
| 127902 |  290 | `		if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
| 127900 |  291 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 127900 |  292 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  293 | `				return rc;` |
|      - |  294 | `			}` |
|  63949 |  295 | `		}` |
|      2 |  296 | `	}` |
|      - |  297 | `	/* Mark as subclass */` |
|  14524 |  298 | `	pSub->pBase = pBase;` |
|      - |  299 | `	/* All done */` |
|  14524 |  300 | `	return SXRET_OK;` |
|   7263 |  301 |  |
|      - |  302 | `/*` |
|      - |  303 | ` * Apply a trait to a class: copy all methods and attributes from the trait` |
|      - |  304 | ` * into the target class. Unlike inheritance, traits copy ALL members including` |
|      - |  305 | ` * private ones. Members already defined in the class take precedence.` |
|      - |  306 | ` */` |
|     30 |  307 | `PH7_PRIVATE sxi32 PH7_ClassUseTrait(ph7_gen_state *pGen,ph7_class *pClass,ph7_class *pTrait)` |
|      1 |  308 |  |
|      - |  309 | `	ph7_class_method *pMeth;` |
|      - |  310 | `	ph7_class_attr *pAttr;` |
|      - |  311 | `	SyHashEntry *pEntry;` |
|      - |  312 | `	SyString *pName;` |
|      - |  313 | `	sxi32 rc;` |
|      - |  314 | `	/* Copy attributes from the trait */` |
|     31 |  315 | `	SyHashResetLoopCursor(&pTrait->hAttr);` |
|     43 |  316 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hAttr)) != 0 ){` |
|      - |  317 | `		SyHashEntry *pExisting;` |
|     13 |  318 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     13 |  319 | `		pName = &pAttr->sName;` |
|     13 |  320 | `		pExisting = SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte);` |
|     13 |  321 | `		if( pExisting != 0 ){` |
|      - |  322 | `			/* Attribute already exists. Check if it came from another trait` |
|      - |  323 | `			 * and whether the definitions are compatible (same defaults).` |
|      - |  324 | `			 */` |
|      - |  325 | `			ph7_class **apUsedTraits;` |
|      - |  326 | `			sxu32 nUsed,k;` |
|      5 |  327 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|      5 |  328 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|      5 |  329 | `			for(k = 0; k < nUsed; k++){` |
|      - |  330 | `				ph7_class_attr *pOther;` |
|      3 |  331 | `				pOther = PH7_ClassExtractAttribute(apUsedTraits[k],pName->zString,pName->nByte);` |
|      3 |  332 | `				if( pOther ){` |
|      - |  333 | `					/* Two traits define the same property — check if defaults differ */` |
|      3 |  334 | `					ph7_class_attr *pClassAttr = (ph7_class_attr *)pExisting->pUserData;` |
|      4 |  335 | `					if( SySetUsed(&pAttr->aByteCode) != SySetUsed(&pClassAttr->aByteCode) \|\|` |
|      3 |  336 | `						(SySetUsed(&pAttr->aByteCode) > 0 &&` |
|      3 |  337 | `						 SyMemcmp(SySetBasePtr(&pAttr->aByteCode),SySetBasePtr(&pClassAttr->aByteCode),` |
|      3 |  338 | `							SySetUsed(&pAttr->aByteCode) * SySetElemSize(&pAttr->aByteCode)) != 0) ){` |
|      4 |  339 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine,` |
|      - |  340 | `							"%z and %z define the same property ($%z) in the composition of %z. "` |
|      - |  341 | `							"However, the definition differs and is considered incompatible",` |
|      2 |  342 | `							&apUsedTraits[k]->sName,&pTrait->sName,pName,&pClass->sName);` |
|      3 |  343 | `						if( rc == SXERR_ABORT ){` |
|    ! 0 |  344 | `							return SXERR_ABORT;` |
|      - |  345 | `						}` |
|      1 |  346 | `					}` |
|      3 |  347 | `					break;` |
|      - |  348 | `				}` |
|    ! 0 |  349 | `			}` |
|      5 |  350 | `			continue;` |
|      - |  351 | `		}` |
|      9 |  352 | `		rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|      9 |  353 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  354 | `			return rc;` |
|      - |  355 | `		}` |
|      1 |  356 | `	}` |
|      - |  357 | `	/* Copy methods from the trait */` |
|     31 |  358 | `	SyHashResetLoopCursor(&pTrait->hMethod);` |
|     59 |  359 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hMethod)) != 0 ){` |
|     29 |  360 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|     29 |  361 | `		pName = &pMeth->sFunc.sName;` |
|     29 |  362 | `		if( SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte) != 0 ){` |
|      - |  363 | `			/* Method already exists in the class. Check if it came from another trait` |
|      - |  364 | `			 * (unresolved conflict) vs being defined by the class itself.` |
|      - |  365 | `			 */` |
|      - |  366 | `			ph7_class **apUsedTraits;` |
|      - |  367 | `			sxu32 nUsed,k;` |
|      7 |  368 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|      7 |  369 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|      7 |  370 | `			for(k = 0; k < nUsed; k++){` |
|      3 |  371 | `				if( PH7_ClassExtractMethod(apUsedTraits[k],pName->zString,pName->nByte) != 0 ){` |
|      - |  372 | `					/* Two different traits define the same method with no resolution */` |
|      4 |  373 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|      - |  374 | `						"Trait method %z::%z has not been applied as %z::%z, "` |
|      - |  375 | `						"because of collision with %z::%z",` |
|      2 |  376 | `						&pTrait->sName,pName,` |
|      1 |  377 | `						&pClass->sName,pName,` |
|      2 |  378 | `						&apUsedTraits[k]->sName,pName);` |
|      3 |  379 | `					if( rc == SXERR_ABORT ){` |
|    ! 0 |  380 | `						return SXERR_ABORT;` |
|      - |  381 | `					}` |
|      3 |  382 | `					break;` |
|      - |  383 | `				}` |
|    ! 0 |  384 | `			}` |
|      - |  385 | `			/* Class-defined method takes precedence */` |
|      7 |  386 | `			continue;` |
|      - |  387 | `		}` |
|     23 |  388 | `		rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|     23 |  389 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  390 | `			return rc;` |
|      - |  391 | `		}` |
|      1 |  392 | `	}` |
|      - |  393 | `	/* Record trait in the class */` |
|     31 |  394 | `	SySetPut(&pClass->aTrait,(const void *)&pTrait);` |
|     15 |  395 | `	SXUNUSED(pGen);` |
|     31 |  396 | `	return SXRET_OK;` |
|     16 |  397 |  |
|      - |  398 | `/*` |
|      - |  399 | ` * Inherit an object interface from another object interface.` |
|      - |  400 | ` * According to the PHP language reference manual.` |
|      - |  401 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|      - |  402 | ` *  must implement, without having to define how these methods are handled.` |
|      - |  403 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|      - |  404 | ` *  class, but without any of the methods having their contents defined.` |
|      - |  405 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|      - |  406 | ` *` |
|      - |  407 | ` * This function return SXRET_OK if the interface inheritance operation was successfully performed.` |
|      - |  408 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|      - |  409 | ` * error message.` |
|      - |  410 | ` */` |
|      2 |  411 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceInherit(ph7_class *pSub,ph7_class *pBase)` |
|      1 |  412 |  |
|      - |  413 | `	ph7_class_method *pMeth;` |
|      - |  414 | `	ph7_class_attr *pAttr;` |
|      - |  415 | `	SyHashEntry *pEntry;` |
|      - |  416 | `	SyString *pName;` |
|      - |  417 | `	sxi32 rc;` |
|      - |  418 | `	/* Install in the derived hashtable */` |
|      3 |  419 | `	SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|      3 |  420 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
|      - |  421 | `	/* Copy constants */` |
|      6 |  422 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|      - |  423 | `		/* Make sure the constants are not redeclared in the subclass */` |
|      3 |  424 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      3 |  425 | `		pName = &pAttr->sName;` |
|      3 |  426 | `		if( SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte) == 0 ){` |
|      - |  427 | `			/* Install the constant in the subclass */` |
|      3 |  428 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|      3 |  429 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  430 | `				return rc;` |
|      - |  431 | `			}` |
|      1 |  432 | `		}` |
|      1 |  433 | `	}` |
|      3 |  434 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
|      - |  435 | `	/* Copy methods signature */` |
|      6 |  436 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|      - |  437 | `		/* Make sure the method are not redeclared in the subclass */` |
|      3 |  438 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      3 |  439 | `		pName = &pMeth->sFunc.sName;` |
|      3 |  440 | `		if( SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte) == 0 ){` |
|      - |  441 | `			/* Install the method */` |
|      3 |  442 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|      3 |  443 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  444 | `				return rc;` |
|      - |  445 | `			}` |
|      1 |  446 | `		}` |
|      1 |  447 | `	}` |
|      - |  448 | `	/* Mark as subclass */` |
|      3 |  449 | `	pSub->pBase = pBase;` |
|      - |  450 | `	/* All done */` |
|      3 |  451 | `	return SXRET_OK;` |
|      2 |  452 |  |
|      - |  453 | `/*` |
|      - |  454 | ` * Implements an object interface in the given main class.` |
|      - |  455 | ` * According to the PHP language reference manual.` |
|      - |  456 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|      - |  457 | ` *  must implement, without having to define how these methods are handled.` |
|      - |  458 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|      - |  459 | ` *  class, but without any of the methods having their contents defined.` |
|      - |  460 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|      - |  461 | ` *` |
|      - |  462 | ` * This function return SXRET_OK if the interface was successfully implemented.` |
|      - |  463 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|      - |  464 | ` * error message.` |
|      - |  465 | ` */` |
|      6 |  466 | `PH7_PRIVATE sxi32 PH7_ClassImplement(ph7_class *pMain,ph7_class *pInterface)` |
|      2 |  467 |  |
|      - |  468 | `	ph7_class_attr *pAttr;` |
|      - |  469 | `	SyHashEntry *pEntry;` |
|      - |  470 | `	SyString *pName;` |
|      - |  471 | `	sxi32 rc;` |
|      - |  472 | `	/* First off,copy all constants declared inside the interface */` |
|      8 |  473 | `	SyHashResetLoopCursor(&pInterface->hAttr);` |
|     13 |  474 | `	while((pEntry = SyHashGetNextEntry(&pInterface->hAttr)) != 0 ){` |
|      - |  475 | `		/* Point to the constant declaration */` |
|      3 |  476 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      3 |  477 | `		pName = &pAttr->sName;` |
|      - |  478 | `		/* Make sure the attribute is not redeclared in the main class */` |
|      3 |  479 | `		if( SyHashGet(&pMain->hAttr,pName->zString,pName->nByte) == 0 ){` |
|      - |  480 | `			/* Install the attribute */` |
|      3 |  481 | `			rc = SyHashInsert(&pMain->hAttr,pName->zString,pName->nByte,pAttr);` |
|      3 |  482 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  483 | `				return rc;` |
|      - |  484 | `			}` |
|      1 |  485 | `		}` |
|      1 |  486 | `	}` |
|      - |  487 | `	/* Install in the interface container */` |
|      8 |  488 | `	SySetPut(&pMain->aInterface,(const void *)&pInterface);` |
|      - |  489 | `	/* TICKET 1433-49/1: Symisc eXtension` |
|      - |  490 | `	 *  A class may not implemnt all declared interface methods,so there` |
|      - |  491 | `	 *  is no need for a method installer loop here.` |
|      - |  492 | `	 */` |
|      8 |  493 | `	return SXRET_OK;` |
|      5 |  494 |  |
|      - |  495 | `/*` |
|      - |  496 | ` * Create a class instance [i.e: Object in the PHP jargon] at run-time.` |
|      - |  497 | ` * The following function is called when an object is created at run-time` |
|      - |  498 | ` * typically when the PH7_OP_NEW/PH7_OP_CLONE instructions are executed.` |
|      - |  499 | ` * Notes on object creation.` |
|      - |  500 | ` *` |
|      - |  501 | ` * According to PHP language reference manual.` |
|      - |  502 | ` * To create an instance of a class, the new keyword must be used. An object will always` |
|      - |  503 | ` * be created unless the object has a constructor defined that throws an exception on error.` |
|      - |  504 | ` * Classes should be defined before instantiation (and in some cases this is a requirement).` |
|      - |  505 | ` * If a string containing the name of a class is used with new, a new instance of that class` |
|      - |  506 | ` * will be created. If the class is in a namespace, its fully qualified name must be used when` |
|      - |  507 | ` * doing this.` |
|      - |  508 | ` * Example #3 Creating an instance` |
|      - |  509 | ` * <?php` |
|      - |  510 | ` *  $instance = new SimpleClass();` |
|      - |  511 | ` *   // This can also be done with a variable:` |
|      - |  512 | ` * $className = 'Foo';` |
|      - |  513 | ` * $instance = new $className(); // Foo()` |
|      - |  514 | ` * ?>` |
|      - |  515 | ` * In the class context, it is possible to create a new object by new self and new parent.` |
|      - |  516 | ` * When assigning an already created instance of a class to a new variable, the new variable` |
|      - |  517 | ` * will access the same instance as the object that was assigned. This behaviour is the same` |
|      - |  518 | ` * when passing instances to a function. A copy of an already created object can be made by` |
|      - |  519 | ` * cloning it.` |
|      - |  520 | ` * Example #4 Object Assignment` |
|      - |  521 | ` * <?php` |
|      - |  522 | ` *  class SimpleClass(){` |
|      - |  523 | ` *    public $var;` |
|      - |  524 | ` *  };` |
|      - |  525 | ` *  $instance = new SimpleClass();` |
|      - |  526 | ` *  $assigned   =  $instance;` |
|      - |  527 | ` *  $reference  =& $instance;` |
|      - |  528 | ` *  $instance->var = '$assigned will have this value';` |
|      - |  529 | ` *  $instance = null; // $instance and $reference become null` |
|      - |  530 | ` *  var_dump($instance);` |
|      - |  531 | ` *  var_dump($reference);` |
|      - |  532 | ` *  var_dump($assigned);` |
|      - |  533 | ` * ?>` |
|      - |  534 | ` * The above example will output:` |
|      - |  535 | ` * NULL` |
|      - |  536 | ` * NULL` |
|      - |  537 | ` * object(SimpleClass)#1 (1) {` |
|      - |  538 | ` *  ["var"]=>` |
|      - |  539 | ` *    string(30) "$assigned will have this value"` |
|      - |  540 | ` * }` |
|      - |  541 | ` * Example #5 Creating new objects` |
|      - |  542 | ` * <?php` |
|      - |  543 | ` * class Test` |
|      - |  544 | ` * {` |
|      - |  545 | ` *   static public function getNew()` |
|      - |  546 | ` *   {` |
|      - |  547 | ` *       return new static;` |
|      - |  548 | ` *   }` |
|      - |  549 | ` * }` |
|      - |  550 | ` * class Child extends Test` |
|      - |  551 | ` * {}` |
|      - |  552 | ` * $obj1 = new Test();` |
|      - |  553 | ` * $obj2 = new $obj1;` |
|      - |  554 | ` * var_dump($obj1 !== $obj2);` |
|      - |  555 | ` * $obj3 = Test::getNew();` |
|      - |  556 | ` * var_dump($obj3 instanceof Test);` |
|      - |  557 | ` * $obj4 = Child::getNew();` |
|      - |  558 | ` * var_dump($obj4 instanceof Child);` |
|      - |  559 | ` * ?>` |
|      - |  560 | ` * The above example will output:` |
|      - |  561 | ` * bool(true)` |
|      - |  562 | ` * bool(true)` |
|      - |  563 | ` * bool(true)` |
|      - |  564 | ` * Note that Symisc Systems have introduced powerfull extension to` |
|      - |  565 | ` * OO subsystem. For example a class attribute may have any complex` |
|      - |  566 | ` * expression associated with it when declaring the attribute unlike` |
|      - |  567 | ` * the standard PHP engine which would allow a single value.` |
|      - |  568 | ` * Example:` |
|      - |  569 | ` *  class myClass{` |
|      - |  570 | ` *    public $var = 25<<1+foo()/bar();` |
|      - |  571 | ` *  };` |
|      - |  572 | ` * Refer to the official documentation for more information.` |
|      - |  573 | ` */` |
|   1104 |  574 | `static ph7_class_instance * NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|      2 |  575 |  |
|      - |  576 | `	ph7_class_instance *pThis;` |
|      - |  577 | `	/* Allocate a new instance */` |
|   1106 |  578 | `	pThis = (ph7_class_instance *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_instance));` |
|   1106 |  579 | `	if( pThis == 0 ){` |
|    ! 0 |  580 | `		return 0;` |
|      - |  581 | `	}` |
|      - |  582 | `	/* Zero the structure */` |
|   1106 |  583 | `	SyZero(pThis,sizeof(ph7_class_instance));` |
|      - |  584 | `	/* Initialize fields */` |
|   1106 |  585 | `	pThis->iRef = 1;` |
|   1106 |  586 | `	pThis->pVm = pVm;` |
|   1106 |  587 | `	pThis->pClass = pClass;` |
|   1106 |  588 | `	SyHashInit(&pThis->hAttr,&pVm->sAllocator,0,0);` |
|   1106 |  589 | `	return pThis;` |
|    554 |  590 |  |
|      - |  591 | `/*` |
|      - |  592 | ` * Wrapper around the NewClassInstance() function defined above.` |
|      - |  593 | ` * See the block comment above for more information.` |
|      - |  594 | ` */` |
|   1062 |  595 | `PH7_PRIVATE ph7_class_instance * PH7_NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|      2 |  596 |  |
|      - |  597 | `	ph7_class_instance *pNew;` |
|      - |  598 | `	sxi32 rc;` |
|   1064 |  599 | `	pNew = NewClassInstance(&(*pVm),&(*pClass));` |
|   1064 |  600 | `	if( pNew == 0 ){` |
|    ! 0 |  601 | `		return 0;` |
|      - |  602 | `	}` |
|      - |  603 | `	/* Associate a private VM frame with this class instance */` |
|   1064 |  604 | `	rc = PH7_VmCreateClassInstanceFrame(&(*pVm),pNew);` |
|   1064 |  605 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  606 | `		SyMemBackendPoolFree(&pVm->sAllocator,pNew);` |
|    ! 0 |  607 | `		return 0;` |
|      - |  608 | `	}` |
|   1064 |  609 | `	return pNew;` |
|    533 |  610 |  |
|      - |  611 | `/*` |
|      - |  612 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon] attribute.` |
|      - |  613 | ` * This function never fail.` |
|      - |  614 | ` */` |
|    540 |  615 | `static ph7_value * ExtractClassAttrValue(ph7_vm *pVm,VmClassAttr *pAttr)` |
|      2 |  616 |  |
|      - |  617 | `	/* Extract the value */` |
|      - |  618 | `	ph7_value *pValue;` |
|    542 |  619 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|    542 |  620 | `	return pValue;` |
|      2 |  621 |  |
|      - |  622 | `/*` |
|      - |  623 | ` * Perform a clone operation on a class instance [i.e: Object in the PHP jargon].` |
|      - |  624 | ` * The following function is called when an object is cloned at run-time` |
|      - |  625 | ` * typically when the PH7_OP_CLONE instruction is executed.` |
|      - |  626 | ` * Notes on object cloning.` |
|      - |  627 | ` *` |
|      - |  628 | ` * According to PHP language reference manual.` |
|      - |  629 | ` * Creating a copy of an object with fully replicated properties is not always the wanted behavior.` |
|      - |  630 | ` * A good example of the need for copy constructors. Another example is if your object holds a reference` |
|      - |  631 | ` * to another object which it uses and when you replicate the parent object you want to create` |
|      - |  632 | ` * a new instance of this other object so that the replica has its own separate copy.` |
|      - |  633 | ` * An object copy is created by using the clone keyword (which calls the object's __clone() method if possible).` |
|      - |  634 | ` * An object's __clone() method cannot be called directly.` |
|      - |  635 | ` * $copy_of_object = clone $object;` |
|      - |  636 | ` * When an object is cloned, PHP 5 will perform a shallow copy of all of the object's properties.` |
|      - |  637 | ` * Any properties that are references to other variables, will remain references.` |
|      - |  638 | ` * Once the cloning is complete, if a __clone() method is defined, then the newly created object's __clone() method` |
|      - |  639 | ` * will be called, to allow any necessary properties that need to be changed.` |
|      - |  640 | ` * Example #1 Cloning an object` |
|      - |  641 | ` * <?php` |
|      - |  642 | ` * class SubObject` |
|      - |  643 | ` * {` |
|      - |  644 | ` *   static $instances = 0;` |
|      - |  645 | ` *   public $instance;` |
|      - |  646 | ` *` |
|      - |  647 | ` *   public function __construct() {` |
|      - |  648 | ` *       $this->instance = ++self::$instances;` |
|      - |  649 | ` *   }` |
|      - |  650 | ` *` |
|      - |  651 | ` *   public function __clone() {` |
|      - |  652 | ` *       $this->instance = ++self::$instances;` |
|      - |  653 | ` *   }` |
|      - |  654 | ` * }` |
|      - |  655 | ` *` |
|      - |  656 | ` * class MyCloneable` |
|      - |  657 | ` * {` |
|      - |  658 | ` *   public $object1;` |
|      - |  659 | ` *   public $object2;` |
|      - |  660 | ` *` |
|      - |  661 | ` *   function __clone()` |
|      - |  662 | ` *   {` |
|      - |  663 | ` *       // Force a copy of this->object, otherwise` |
|      - |  664 | ` *       // it will point to same object.` |
|      - |  665 | ` *       $this->object1 = clone $this->object1;` |
|      - |  666 | ` *   }` |
|      - |  667 | ` * }` |
|      - |  668 | ` * $obj = new MyCloneable();` |
|      - |  669 | ` * $obj->object1 = new SubObject();` |
|      - |  670 | ` * $obj->object2 = new SubObject();` |
|      - |  671 | ` * $obj2 = clone $obj;` |
|      - |  672 | ` * print("Original Object:\n");` |
|      - |  673 | ` * print_r($obj);` |
|      - |  674 | ` * print("Cloned Object:\n");` |
|      - |  675 | ` * print_r($obj2);` |
|      - |  676 | ` * ?>` |
|      - |  677 | ` * The above example will output:` |
|      - |  678 | ` * Original Object:` |
|      - |  679 | ` * MyCloneable Object` |
|      - |  680 | ` * (` |
|      - |  681 | ` *   [object1] => SubObject Object` |
|      - |  682 | ` *       (` |
|      - |  683 | ` *           [instance] => 1` |
|      - |  684 | ` *       )` |
|      - |  685 | ` *` |
|      - |  686 | ` *   [object2] => SubObject Object` |
|      - |  687 | ` *       (` |
|      - |  688 | ` *           [instance] => 2` |
|      - |  689 | ` *       )` |
|      - |  690 | ` *` |
|      - |  691 | ` * )` |
|      - |  692 | ` * Cloned Object:` |
|      - |  693 | ` * MyCloneable Object` |
|      - |  694 | ` * (` |
|      - |  695 | ` *   [object1] => SubObject Object` |
|      - |  696 | ` *       (` |
|      - |  697 | ` *           [instance] => 3` |
|      - |  698 | ` *       )` |
|      - |  699 | ` *` |
|      - |  700 | ` *   [object2] => SubObject Object` |
|      - |  701 | ` *       (` |
|      - |  702 | ` *           [instance] => 2` |
|      - |  703 | ` *       )` |
|      - |  704 | ` * )` |
|      - |  705 | ` */` |
|     42 |  706 | `PH7_PRIVATE ph7_class_instance * PH7_CloneClassInstance(ph7_class_instance *pSrc)` |
|      2 |  707 |  |
|      - |  708 | `	ph7_class_instance *pClone;` |
|      - |  709 | `	ph7_class_method *pMethod;` |
|      - |  710 | `	SyHashEntry *pEntry2;` |
|      - |  711 | `	SyHashEntry *pEntry;` |
|      - |  712 | `	ph7_vm *pVm;` |
|      - |  713 | `	sxi32 rc;` |
|      - |  714 | `	/* Allocate a new instance */` |
|     44 |  715 | `	pVm = pSrc->pVm;` |
|     44 |  716 | `	pClone = NewClassInstance(pVm,pSrc->pClass);` |
|     44 |  717 | `	if( pClone == 0 ){` |
|    ! 0 |  718 | `		return 0;` |
|      - |  719 | `	}` |
|      - |  720 | `	/* Associate a private VM frame with this class instance */` |
|     44 |  721 | `	rc = PH7_VmCreateClassInstanceFrame(pVm,pClone);` |
|     44 |  722 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  723 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClone);` |
|    ! 0 |  724 | `		return 0;` |
|      - |  725 | `	}` |
|      - |  726 | `	/* Duplicate object values */` |
|     44 |  727 | `	SyHashResetLoopCursor(&pSrc->hAttr);` |
|     44 |  728 | `	SyHashResetLoopCursor(&pClone->hAttr);` |
|    111 |  729 | `	while((pEntry = SyHashGetNextEntry(&pSrc->hAttr)) != 0 && (pEntry2 = SyHashGetNextEntry(&pClone->hAttr)) != 0 ){` |
|     48 |  730 | `		VmClassAttr *pSrcAttr = (VmClassAttr *)pEntry->pUserData;` |
|     48 |  731 | `		VmClassAttr *pDestAttr = (VmClassAttr *)pEntry2->pUserData;` |
|      - |  732 | `		/* Duplicate non-static attribute */` |
|     48 |  733 | `		if( (pSrcAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|      - |  734 | `			ph7_value *pvSrc,*pvDest;` |
|     48 |  735 | `			pvSrc = ExtractClassAttrValue(pVm,pSrcAttr);` |
|     48 |  736 | `			pvDest = ExtractClassAttrValue(pVm,pDestAttr);` |
|     48 |  737 | `			if( pvSrc && pvDest ){` |
|     48 |  738 | `				PH7_MemObjStore(pvSrc,pvDest);` |
|     23 |  739 | `			}` |
|     23 |  740 | `		}` |
|      2 |  741 | `	}` |
|      - |  742 | `	/* call the __clone method on the cloned object if available */` |
|     44 |  743 | `	pMethod = PH7_ClassExtractMethod(pClone->pClass,"__clone",sizeof("__clone")-1);` |
|     44 |  744 | `	if( pMethod ){` |
|     38 |  745 | `		if( pMethod->iCloneDepth < 16 ){` |
|     36 |  746 | `			pMethod->iCloneDepth++;` |
|     36 |  747 | `			PH7_VmCallClassMethod(pVm,pClone,pMethod,0,0,0);` |
|     19 |  748 | `		}else{` |
|      - |  749 | `			/* Nesting limit reached */` |
|      3 |  750 | `			PH7_VmThrowError(pVm,0,PH7_CTX_ERR,"Object clone limit reached,no more call to __clone()");` |
|      - |  751 | `		}` |
|      - |  752 | `		/* Reset the cursor */` |
|     38 |  753 | `		pMethod->iCloneDepth = 0;` |
|     18 |  754 | `	}` |
|      - |  755 | `	/* Return the cloned object */` |
|     44 |  756 | `	return pClone;` |
|     23 |  757 |  |
|      - |  758 | `#define CLASS_INSTANCE_DESTROYED 0x001 /* Instance is released */` |
|      - |  759 | `/*` |
|      - |  760 | ` * Release a class instance [i.e: Object in the PHP jargon] and invoke any defined destructor.` |
|      - |  761 | ` * This routine is invoked as soon as there are no other references to a particular` |
|      - |  762 | ` * class instance.` |
|      - |  763 | ` */` |
|    772 |  764 | `static void PH7_ClassInstanceRelease(ph7_class_instance *pThis)` |
|      2 |  765 |  |
|      - |  766 | `	ph7_class_method *pDestr;` |
|      - |  767 | `	SyHashEntry *pEntry;` |
|      - |  768 | `	ph7_class *pClass;` |
|      - |  769 | `	ph7_vm *pVm;` |
|    774 |  770 | `	if( pThis->iFlags & CLASS_INSTANCE_DESTROYED ){` |
|      - |  771 | `		/*` |
|      - |  772 | `		 * Already destroyed,return immediately.` |
|      - |  773 | `		 * This could happend if someone perform unset($this) in the destructor body.` |
|      - |  774 | `		 */` |
|    ! 0 |  775 | `		return;` |
|      - |  776 | `	}` |
|      - |  777 | `	/* Mark as destroyed */` |
|    774 |  778 | `	pThis->iFlags \|= CLASS_INSTANCE_DESTROYED;` |
|      - |  779 | `	/* Invoke any defined destructor if available */` |
|    774 |  780 | `	pVm = pThis->pVm;` |
|    774 |  781 | `	pClass = pThis->pClass;` |
|    774 |  782 | `	pDestr = PH7_ClassExtractMethod(pClass,"__destruct",sizeof("__destruct")-1);` |
|    774 |  783 | `	if( pDestr ){` |
|      - |  784 | `		/* Invoke the destructor */` |
|      5 |  785 | `		pThis->iRef = 2; /* Prevent garbage collection */` |
|      5 |  786 | `		PH7_VmCallClassMethod(pVm,pThis,pDestr,0,0,0);` |
|      2 |  787 | `	}` |
|      - |  788 | `	/* Release non-static attributes */` |
|    774 |  789 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|   3998 |  790 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|   3226 |  791 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|   3226 |  792 | `		if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|   3222 |  793 | `			PH7_VmUnsetMemObj(pVm,pVmAttr->nIdx,TRUE);` |
|   1610 |  794 | `		}` |
|   3226 |  795 | `		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      2 |  796 | `	}` |
|      - |  797 | `	/* Release the whole structure */` |
|    774 |  798 | `	SyHashRelease(&pThis->hAttr);` |
|    774 |  799 | `	SyMemBackendPoolFree(&pVm->sAllocator,pThis);` |
|    388 |  800 |  |
|      - |  801 | `/*` |
|      - |  802 | ` * Decrement the reference count of a class instance [i.e Object in the PHP jargon].` |
|      - |  803 | ` * If the reference count reaches zero,release the whole instance.` |
|      - |  804 | ` */` |
|  13640 |  805 | `PH7_PRIVATE void PH7_ClassInstanceUnref(ph7_class_instance *pThis)` |
|      2 |  806 |  |
|  13642 |  807 | `	pThis->iRef--;` |
|  13642 |  808 | `	if( pThis->iRef < 1 ){` |
|      - |  809 | `		/* No more reference to this instance */` |
|    774 |  810 | `		PH7_ClassInstanceRelease(&(*pThis));` |
|    386 |  811 | `	}` |
|  13642 |  812 |  |
|      - |  813 | `/*` |
|      - |  814 | ` * Compare two class instances [i.e: Objects in the PHP jargon]` |
|      - |  815 | ` * Note on objects comparison:` |
|      - |  816 | ` *  According to the PHP langauge reference manual` |
|      - |  817 | ` *  When using the comparison operator (==), object variables are compared in a simple manner` |
|      - |  818 | ` *  namely: Two object instances are equal if they have the same attributes and values, and are` |
|      - |  819 | ` *  instances of the same class.` |
|      - |  820 | ` *  On the other hand, when using the identity operator (===), object variables are identical` |
|      - |  821 | ` *  if and only if they refer to the same instance of the same class.` |
|      - |  822 | ` *  An example will clarify these rules.` |
|      - |  823 | ` *  Example #1 Example of object comparison` |
|      - |  824 | ` *  <?php` |
|      - |  825 | ` *    function bool2str($bool)` |
|      - |  826 | ` * {` |
|      - |  827 | ` *   if ($bool === false) {` |
|      - |  828 | ` *       return 'FALSE';` |
|      - |  829 | ` *   } else {` |
|      - |  830 | ` *       return 'TRUE';` |
|      - |  831 | ` *   }` |
|      - |  832 | ` * }` |
|      - |  833 | ` * function compareObjects(&$o1, &$o2)` |
|      - |  834 | ` * {` |
|      - |  835 | ` *   echo 'o1 == o2 : ' . bool2str($o1 == $o2) . "\n";` |
|      - |  836 | ` *   echo 'o1 != o2 : ' . bool2str($o1 != $o2) . "\n";` |
|      - |  837 | ` *   echo 'o1 === o2 : ' . bool2str($o1 === $o2) . "\n";` |
|      - |  838 | ` *   echo 'o1 !== o2 : ' . bool2str($o1 !== $o2) . "\n";` |
|      - |  839 | ` * }` |
|      - |  840 | ` * class Flag` |
|      - |  841 | ` * {` |
|      - |  842 | ` *   public $flag;` |
|      - |  843 | ` *` |
|      - |  844 | ` *   function Flag($flag = true) {` |
|      - |  845 | ` *       $this->flag = $flag;` |
|      - |  846 | ` *   }` |
|      - |  847 | ` * }` |
|      - |  848 | ` *` |
|      - |  849 | ` * class OtherFlag` |
|      - |  850 | ` * {` |
|      - |  851 | ` *   public $flag;` |
|      - |  852 | ` *` |
|      - |  853 | ` *   function OtherFlag($flag = true) {` |
|      - |  854 | ` *       $this->flag = $flag;` |
|      - |  855 | ` *   }` |
|      - |  856 | ` * }` |
|      - |  857 | ` *` |
|      - |  858 | ` * $o = new Flag();` |
|      - |  859 | ` * $p = new Flag();` |
|      - |  860 | ` * $q = $o;` |
|      - |  861 | ` * $r = new OtherFlag();` |
|      - |  862 | ` *` |
|      - |  863 | ` * echo "Two instances of the same class\n";` |
|      - |  864 | ` * compareObjects($o, $p);` |
|      - |  865 | ` * echo "\nTwo references to the same instance\n";` |
|      - |  866 | ` * compareObjects($o, $q);` |
|      - |  867 | ` * echo "\nInstances of two different classes\n";` |
|      - |  868 | ` * compareObjects($o, $r);` |
|      - |  869 | ` * ?>` |
|      - |  870 | ` * The above example will output:` |
|      - |  871 | ` * Two instances of the same class` |
|      - |  872 | ` * o1 == o2 : TRUE` |
|      - |  873 | ` * o1 != o2 : FALSE` |
|      - |  874 | ` * o1 === o2 : FALSE` |
|      - |  875 | ` * o1 !== o2 : TRUE` |
|      - |  876 | ` * Two references to the same instance` |
|      - |  877 | ` * o1 == o2 : TRUE` |
|      - |  878 | ` * o1 != o2 : FALSE` |
|      - |  879 | ` * o1 === o2 : TRUE` |
|      - |  880 | ` * o1 !== o2 : FALSE` |
|      - |  881 | ` * Instances of two different classes` |
|      - |  882 | ` * o1 == o2 : FALSE` |
|      - |  883 | ` * o1 != o2 : TRUE` |
|      - |  884 | ` * o1 === o2 : FALSE` |
|      - |  885 | ` * o1 !== o2 : TRUE` |
|      - |  886 | ` *` |
|      - |  887 | ` * This function return 0 if the objects are equals according to the comprison rules defined above.` |
|      - |  888 | ` * Any other return values indicates difference.` |
|      - |  889 | ` */` |
|    160 |  890 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest)` |
|      2 |  891 |  |
|      - |  892 | `	SyHashEntry *pEntry,*pEntry2;` |
|      - |  893 | `	ph7_value sV1,sV2;` |
|      - |  894 | `	sxi32 rc;` |
|    162 |  895 | `	if( iNest > 31 ){` |
|      - |  896 | `		/* Nesting limit reached */` |
|      5 |  897 | `		PH7_VmThrowError(pLeft->pVm,0,PH7_CTX_ERR,"Nesting limit reached: Infinite recursion?");` |
|      5 |  898 | `		return 1;` |
|      - |  899 | `	}` |
|      - |  900 | `	/* Comparison is performed only if the objects are instance of the same class */` |
|    158 |  901 | `	if( pLeft->pClass != pRight->pClass ){` |
|      7 |  902 | `		return 1;` |
|      - |  903 | `	}` |
|    152 |  904 | `	if( bStrict ){` |
|      - |  905 | `		/*` |
|      - |  906 | `		 * According to the PHP language reference manual:` |
|      - |  907 | `		 *  when using the identity operator (===), object variables` |
|      - |  908 | `		 *  are identical if and only if they refer to the same instance` |
|      - |  909 | `		 *  of the same class.` |
|      - |  910 | `		 */` |
|     11 |  911 | `		return !(pLeft == pRight);` |
|      - |  912 | `	}` |
|      - |  913 | `	/*` |
|      - |  914 | `	 * Attribute comparison.` |
|      - |  915 | `	 * According to the PHP reference manual:` |
|      - |  916 | `	 *  When using the comparison operator (==), object variables are compared` |
|      - |  917 | `	 *  in a simple manner, namely: Two object instances are equal if they have` |
|      - |  918 | `	 *  the same attributes and values, and are instances of the same class.` |
|      - |  919 | `	 */` |
|    142 |  920 | `	if( pLeft == pRight ){` |
|      - |  921 | `		/* Same instance,don't bother processing,object are equals */` |
|      3 |  922 | `		return 0;` |
|      - |  923 | `	}` |
|    140 |  924 | `	SyHashResetLoopCursor(&pLeft->hAttr);` |
|    140 |  925 | `	SyHashResetLoopCursor(&pRight->hAttr);` |
|    140 |  926 | `	PH7_MemObjInit(pLeft->pVm,&sV1);` |
|    140 |  927 | `	PH7_MemObjInit(pLeft->pVm,&sV2);` |
|    140 |  928 | `	sV1.nIdx = sV2.nIdx = SXU32_HIGH;` |
|    223 |  929 | `	while((pEntry = SyHashGetNextEntry(&pLeft->hAttr)) != 0 && (pEntry2 = SyHashGetNextEntry(&pRight->hAttr)) != 0 ){` |
|    146 |  930 | `		VmClassAttr *p1 = (VmClassAttr *)pEntry->pUserData;` |
|    146 |  931 | `		VmClassAttr *p2 = (VmClassAttr *)pEntry2->pUserData;` |
|      - |  932 | `		/* Compare only non-static attribute */` |
|    146 |  933 | `		if( (p1->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|      - |  934 | `			ph7_value *pL,*pR;` |
|    146 |  935 | `			pL = ExtractClassAttrValue(pLeft->pVm,p1);` |
|    146 |  936 | `			pR = ExtractClassAttrValue(pRight->pVm,p2);` |
|    146 |  937 | `			if( pL && pR ){` |
|    146 |  938 | `				PH7_MemObjLoad(pL,&sV1);` |
|    146 |  939 | `				PH7_MemObjLoad(pR,&sV2);` |
|      - |  940 | `				/* Compare the two values now */` |
|    146 |  941 | `				rc = PH7_MemObjCmp(&sV1,&sV2,bStrict,iNest+1);` |
|    146 |  942 | `				PH7_MemObjRelease(&sV1);` |
|    146 |  943 | `				PH7_MemObjRelease(&sV2);` |
|    146 |  944 | `				if( rc != 0 ){` |
|      - |  945 | `					/* Not equals */` |
|    132 |  946 | `					return rc;` |
|      - |  947 | `				}` |
|      7 |  948 | `			}` |
|      7 |  949 | `		}` |
|      1 |  950 | `	}` |
|      - |  951 | `	/* Object are equals */` |
|      9 |  952 | `	return 0;` |
|     82 |  953 |  |
|      - |  954 | `/*` |
|      - |  955 | ` * Dump a class instance and the store the dump in the BLOB given` |
|      - |  956 | ` * as the first argument.` |
|      - |  957 | ` * Note that only non-static/non-constants attribute are dumped.` |
|      - |  958 | ` * This function is typically invoked when the user issue a call` |
|      - |  959 | ` * to [var_dump(),var_export(),print_r(),...].` |
|      - |  960 | ` * This function SXRET_OK on success. Any other return value including` |
|      - |  961 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|      - |  962 | ` */` |
|    132 |  963 | `PH7_PRIVATE sxi32 PH7_ClassInstanceDump(SyBlob *pOut,ph7_class_instance *pThis,int ShowType,int nTab,int nDepth)` |
|      1 |  964 |  |
|      - |  965 | `	SyHashEntry *pEntry;` |
|      - |  966 | `	ph7_value *pValue;` |
|      - |  967 | `	sxi32 rc;` |
|      - |  968 | `	int i;` |
|    133 |  969 | `	if( nDepth > 31 ){` |
|      - |  970 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|      - |  971 | `		/* Nesting limit reached..halt immediately*/` |
|      5 |  972 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|      5 |  973 | `		if( ShowType ){` |
|      5 |  974 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|      2 |  975 | `		}` |
|      5 |  976 | `		return SXERR_LIMIT;` |
|      - |  977 | `	}` |
|    129 |  978 | `	rc = SXRET_OK;` |
|    129 |  979 | `	if( !ShowType ){` |
|      3 |  980 | `		SyBlobAppend(&(*pOut),"Object(",sizeof("Object(")-1);` |
|      1 |  981 | `	}` |
|      - |  982 | `	/* Append class name */` |
|    129 |  983 | `	SyBlobFormat(&(*pOut),"%z) {",&pThis->pClass->sName);` |
|      - |  984 | `#ifdef __WINNT__` |
|      1 |  985 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|      - |  986 | `#else` |
|    128 |  987 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      - |  988 | `#endif` |
|      - |  989 | `	/* Dump object attributes */` |
|    129 |  990 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|    201 |  991 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|    133 |  992 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    133 |  993 | `		if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|      - |  994 | `			/* Dump non-static/constant attribute only */` |
|   3985 |  995 | `			for( i = 0 ; i < nTab ; i++ ){` |
|   3853 |  996 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|   1927 |  997 | `			}` |
|    133 |  998 | `			pValue = ExtractClassAttrValue(pThis->pVm,pVmAttr);` |
|    133 |  999 | `			if( pValue ){` |
|    133 | 1000 | `				SyBlobFormat(&(*pOut),"['%z'] =>",&pVmAttr->pAttr->sName);` |
|      - | 1001 | `#ifdef __WINNT__` |
|      1 | 1002 | `				SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|      - | 1003 | `#else` |
|    132 | 1004 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      - | 1005 | `#endif` |
|    133 | 1006 | `				rc = PH7_MemObjDump(&(*pOut),pValue,ShowType,nTab+1,nDepth,0);` |
|    133 | 1007 | `				if( rc == SXERR_LIMIT ){` |
|    125 | 1008 | `					break;` |
|      - | 1009 | `				}` |
|      4 | 1010 | `			}` |
|      4 | 1011 | `		}` |
|      1 | 1012 | `	}` |
|   3977 | 1013 | `	for( i = 0 ; i < nTab ; i++ ){` |
|   3849 | 1014 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|   1925 | 1015 | `	}` |
|    129 | 1016 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|    129 | 1017 | `	return rc;` |
|     67 | 1018 |  |
|      - | 1019 | `/*` |
|      - | 1020 | ` * Call a magic method [i.e: __toString(),__toBool(),__Invoke()...]` |
|      - | 1021 | ` * Return SXRET_OK on successfull call. Any other return value indicates failure.` |
|      - | 1022 | ` * Notes on magic methods.` |
|      - | 1023 | ` * According to the PHP language reference manual.` |
|      - | 1024 | ` *  The function names __construct(), __destruct(), __call(), __callStatic()` |
|      - | 1025 | ` *  __get(),  __toString(), __invoke(), __clone() are magical in PHP classes.` |
|      - | 1026 | ` * You cannot have functions with these names in any of your classes unless` |
|      - | 1027 | ` * you want the magic functionality associated with them.` |
|      - | 1028 | ` * Example of magical methods:` |
|      - | 1029 | ` * __toString()` |
|      - | 1030 | ` *  The __toString() method allows a class to decide how it will react when it is treated like` |
|      - | 1031 | ` *  a string. For example, what echo $obj; will print. This method must return a string.` |
|      - | 1032 | ` *  Example #2 Simple example` |
|      - | 1033 | ` * <?php` |
|      - | 1034 | ` * // Declare a simple class` |
|      - | 1035 | ` * class TestClass` |
|      - | 1036 | ` * {` |
|      - | 1037 | ` *   public $foo;` |
|      - | 1038 | ` *` |
|      - | 1039 | ` *   public function __construct($foo)` |
|      - | 1040 | ` *   {` |
|      - | 1041 | ` *       $this->foo = $foo;` |
|      - | 1042 | ` *   }` |
|      - | 1043 | ` *` |
|      - | 1044 | ` *   public function __toString()` |
|      - | 1045 | ` *   {` |
|      - | 1046 | ` *       return $this->foo;` |
|      - | 1047 | ` *   }` |
|      - | 1048 | ` * }` |
|      - | 1049 | ` * $class = new TestClass('Hello');` |
|      - | 1050 | ` * echo $class;` |
|      - | 1051 | ` * ?>` |
|      - | 1052 | ` * The above example will output:` |
|      - | 1053 | ` *  Hello` |
|      - | 1054 | ` *` |
|      - | 1055 | ` * Note that PH7 does not support all the magical method and introudces __toFloat(),__toInt()` |
|      - | 1056 | ` * which have the same behaviour as __toString() but for float and integer types` |
|      - | 1057 | ` * respectively.` |
|      - | 1058 | ` * Refer to the official documentation for more information.` |
|      - | 1059 | ` */` |
|      4 | 1060 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallMagicMethod(` |
|      - | 1061 | `	ph7_vm *pVm,               /* VM that own all this stuff */` |
|      - | 1062 | `	ph7_class *pClass,         /* Target class */` |
|      - | 1063 | `	ph7_class_instance *pThis, /* Target object */` |
|      - | 1064 | `	const char *zMethod,       /* Magic method name [i.e: __toString()]*/` |
|      - | 1065 | `	sxu32 nByte,               /* zMethod length*/` |
|      - | 1066 | `	const SyString *pAttrName  /* Attribute name */` |
|      - | 1067 | `	)` |
|      2 | 1068 |  |
|      6 | 1069 | `	ph7_value *apArg[2] = { 0 , 0 };` |
|      - | 1070 | `	ph7_class_method *pMeth;` |
|      - | 1071 | `	ph7_value sAttr; /* cc warning */` |
|      - | 1072 | `	sxi32 rc;` |
|      - | 1073 | `	int nArg;` |
|      - | 1074 | `	/* Make sure the magic method is available */` |
|      6 | 1075 | `	pMeth = PH7_ClassExtractMethod(&(*pClass),zMethod,nByte);` |
|      6 | 1076 | `	if( pMeth == 0 ){` |
|      - | 1077 | `		/* No such method,return immediately */` |
|      3 | 1078 | `		return SXERR_NOTFOUND;` |
|      - | 1079 | `	}` |
|      3 | 1080 | `	nArg = 0;` |
|      - | 1081 | `	/* Copy arguments */` |
|      3 | 1082 | `	if( pAttrName ){` |
|    ! 0 | 1083 | `		PH7_MemObjInitFromString(pVm,&sAttr,pAttrName);` |
|    ! 0 | 1084 | `		sAttr.nIdx = SXU32_HIGH; /* Mark as constant */` |
|    ! 0 | 1085 | `		apArg[0] = &sAttr;` |
|    ! 0 | 1086 | `		nArg = 1;` |
|    ! 0 | 1087 | `	}` |
|      - | 1088 | `	/* Call the magic method now */` |
|      3 | 1089 | `	rc = PH7_VmCallClassMethod(pVm,&(*pThis),pMeth,0,nArg,apArg);` |
|      - | 1090 | `	/* Clean up */` |
|      3 | 1091 | `	if( pAttrName ){` |
|    ! 0 | 1092 | `		PH7_MemObjRelease(&sAttr);` |
|    ! 0 | 1093 | `	}` |
|      3 | 1094 | `	return rc;` |
|      4 | 1095 |  |
|      - | 1096 | `/*` |
|      - | 1097 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon].` |
|      - | 1098 | ` * This function is simply a wrapper on ExtractClassAttrValue().` |
|      - | 1099 | ` */` |
|     18 | 1100 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceExtractAttrValue(ph7_class_instance *pThis,VmClassAttr *pAttr)` |
|      1 | 1101 |  |
|      - | 1102 | `   /* Extract the attribute value */` |
|      - | 1103 | `	ph7_value *pValue;` |
|     19 | 1104 | `	pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     19 | 1105 | `	return pValue;` |
|      1 | 1106 |  |
|      - | 1107 | `/*` |
|      - | 1108 | ` * Convert a class instance [i.e: Object in the PHP jargon] into a hashmap [i.e: array in the PHP jargon].` |
|      - | 1109 | ` * Return SXRET_OK on success. Any other value indicates failure.` |
|      - | 1110 | ` * Note on object conversion to array:` |
|      - | 1111 | ` *  Acccording to the PHP language reference manual` |
|      - | 1112 | ` *  If an object is converted to an array, the result is an array whose elements are the object's properties.` |
|      - | 1113 | ` *  The keys are the member variable names.` |
|      - | 1114 | ` *` |
|      - | 1115 | ` *  The following example:` |
|      - | 1116 | ` *  class Test {` |
|      - | 1117 | ` *   public $A = 25<<1;  // 50` |
|      - | 1118 | ` *	 public $c = rand_str(3);   // Random string of length 3` |
|      - | 1119 | ` *	 public $d = rand() & 1023; // Random number between 0..1023` |
|      - | 1120 | ` *  }` |
|      - | 1121 | ` *  var_dump((array) new Test());` |
|      - | 1122 | ` *	Will output:` |
|      - | 1123 | ` *  array(3) {` |
|      - | 1124 | ` *   [A] =>` |
|      - | 1125 | ` *      int(50)` |
|      - | 1126 | ` *   [c] =>` |
|      - | 1127 | ` *     string(3 'aps')` |
|      - | 1128 | ` *   [d] =>` |
|      - | 1129 | ` *     int(991)` |
|      - | 1130 | ` *  }` |
|      - | 1131 | ` * You have noticed that PH7 allow class attributes [i.e: $a,$c,$d in the example above]` |
|      - | 1132 | ` * have any complex expression (even function calls/Annonymous functions) as their default` |
|      - | 1133 | ` * value unlike the standard PHP engine.` |
|      - | 1134 | ` * This is a very powerful feature that you have to look at.` |
|      - | 1135 | ` */` |
|      6 | 1136 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|      1 | 1137 |  |
|      - | 1138 | `	SyHashEntry *pEntry;` |
|      - | 1139 | `	SyString *pAttrName;` |
|      - | 1140 | `	VmClassAttr *pAttr;` |
|      - | 1141 | `	ph7_value *pValue;` |
|      - | 1142 | `	ph7_value sName;` |
|      - | 1143 | `	/* Reset the loop cursor */` |
|      7 | 1144 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      7 | 1145 | `	PH7_MemObjInitFromString(pThis->pVm,&sName,0);` |
|     20 | 1146 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|      - | 1147 | `		/* Point to the current attribute */` |
|     11 | 1148 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1149 | `		/* Extract attribute value */` |
|     11 | 1150 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     11 | 1151 | `		if( pValue ){` |
|      - | 1152 | `			/* Build attribute name */` |
|     11 | 1153 | `			pAttrName = &pAttr->pAttr->sName;` |
|     11 | 1154 | `			PH7_MemObjStringAppend(&sName,pAttrName->zString,pAttrName->nByte);` |
|      - | 1155 | `			/* Perform the insertion */` |
|     11 | 1156 | `			PH7_HashmapInsert(pMap,&sName,pValue);` |
|      - | 1157 | `			/* Reset the string cursor */` |
|     11 | 1158 | `			SyBlobReset(&sName.sBlob);` |
|      5 | 1159 | `		}` |
|      1 | 1160 | `	}` |
|      7 | 1161 | `	PH7_MemObjRelease(&sName);` |
|      7 | 1162 | `	return SXRET_OK;` |
|      1 | 1163 |  |
|      - | 1164 | `/*` |
|      - | 1165 | ` * Iterate throw class attributes and invoke the given callback [i.e: xWalk()] for each` |
|      - | 1166 | ` * retrieved attribute.` |
|      - | 1167 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|      - | 1168 | ` * the attribute value in the callback body will not alter the real attribute value.` |
|      - | 1169 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|      - | 1170 | ` * a value different from PH7_OK.` |
|      - | 1171 | ` * Refer to [ph7_object_walk()] for more information.` |
|      - | 1172 | ` */` |
|    ! 0 | 1173 | `PH7_PRIVATE sxi32 PH7_ClassInstanceWalk(` |
|      - | 1174 | `	ph7_class_instance *pThis, /* Target object */` |
|      - | 1175 | `	int (*xWalk)(const char *,ph7_value *,void *), /* Walker callback */` |
|      - | 1176 | `	void *pUserData /* Last argument to xWalk() */` |
|      - | 1177 | `	)` |
|    ! 0 | 1178 |  |
|      - | 1179 | `	SyHashEntry *pEntry; /* Hash entry */` |
|      - | 1180 | `	VmClassAttr *pAttr;  /* Pointer to the attribute */` |
|      - | 1181 | `	ph7_value *pValue;   /* Attribute value */` |
|      - | 1182 | `	ph7_value sValue;    /* Copy of the attribute value */` |
|      - | 1183 | `	int rc;` |
|      - | 1184 | `	/* Reset the loop cursor */` |
|    ! 0 | 1185 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|    ! 0 | 1186 | `	PH7_MemObjInit(pThis->pVm,&sValue);` |
|      - | 1187 | `	/* Start the walk process */` |
|    ! 0 | 1188 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|      - | 1189 | `		/* Point to the current attribute */` |
|    ! 0 | 1190 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1191 | `		/* Extract attribute value */` |
|    ! 0 | 1192 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|    ! 0 | 1193 | `		if( pValue ){` |
|    ! 0 | 1194 | `			PH7_MemObjLoad(pValue,&sValue);` |
|      - | 1195 | `			/* Invoke the supplied callback */` |
|    ! 0 | 1196 | `			rc =  xWalk(SyStringData(&pAttr->pAttr->sName),&sValue,pUserData);` |
|    ! 0 | 1197 | `			PH7_MemObjRelease(&sValue);` |
|    ! 0 | 1198 | `			if( rc != PH7_OK){` |
|      - | 1199 | `				/* User callback request an operation abort */` |
|    ! 0 | 1200 | `				return SXERR_ABORT;` |
|      - | 1201 | `			}` |
|    ! 0 | 1202 | `		}` |
|    ! 0 | 1203 | `	}` |
|      - | 1204 | `	/* All done */` |
|    ! 0 | 1205 | `	return SXRET_OK;` |
|    ! 0 | 1206 |  |
|      - | 1207 | `/*` |
|      - | 1208 | ` * Extract a class atrribute value.` |
|      - | 1209 | ` * Return a pointer to the attribute value on success. Otherwise NULL.` |
|      - | 1210 | ` * Note:` |
|      - | 1211 | ` *  Access to static and constant attribute is not allowed. That is,the function` |
|      - | 1212 | ` *  will return NULL in case someone (host-application code) try to extract` |
|      - | 1213 | ` *  a static/constant attribute.` |
|      - | 1214 | ` */` |
|    ! 0 | 1215 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceFetchAttr(ph7_class_instance *pThis,const SyString *pName)` |
|    ! 0 | 1216 |  |
|      - | 1217 | `	SyHashEntry *pEntry;` |
|      - | 1218 | `	VmClassAttr *pAttr;` |
|      - | 1219 | `	/* Query the attribute hashtable */` |
|    ! 0 | 1220 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);` |
|    ! 0 | 1221 | `	if( pEntry == 0 ){` |
|      - | 1222 | `		/* No such attribute */` |
|    ! 0 | 1223 | `		return 0;` |
|      - | 1224 | `	}` |
|      - | 1225 | `	/* Point to the class atrribute */` |
|    ! 0 | 1226 | `	pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1227 | `	/* Check if we are dealing with a static/constant attribute */` |
|    ! 0 | 1228 | `	if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|      - | 1229 | `		/* Access is forbidden */` |
|    ! 0 | 1230 | `		return 0;` |
|      - | 1231 | `	}` |
|      - | 1232 | `	/* Return the attribute value */` |
|    ! 0 | 1233 | `	return ExtractClassAttrValue(pThis->pVm,pAttr);` |
|    ! 0 | 1234 |  |
|      - | 1235 |  |
