# src/ph7/oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 439/501 lines (87.62%)

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
|  53704 |   14 | `PH7_PRIVATE ph7_class * PH7_NewRawClass(ph7_vm *pVm,const SyString *pName,sxu32 nLine)` |
|      2 |   15 |  |
|      - |   16 | `	ph7_class *pClass;` |
|      - |   17 | `	char *zName;` |
|      - |   18 | `	/* Allocate a new instance */` |
|  53706 |   19 | `	pClass = (ph7_class *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class));` |
|  53706 |   20 | `	if( pClass == 0 ){` |
|    ! 0 |   21 | `		return 0;` |
|      - |   22 | `	}` |
|      - |   23 | `	/* Zero the structure */` |
|  53706 |   24 | `	SyZero(pClass,sizeof(ph7_class));` |
|      - |   25 | `	/* Duplicate class name */` |
|  53706 |   26 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  53706 |   27 | `	if( zName == 0 ){` |
|    ! 0 |   28 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClass);` |
|    ! 0 |   29 | `		return 0;` |
|      - |   30 | `	}` |
|      - |   31 | `	/* Initialize fields */` |
|  53706 |   32 | `	SyStringInitFromBuf(&pClass->sName,zName,pName->nByte);` |
|  53706 |   33 | `	SyHashInit(&pClass->hMethod,&pVm->sAllocator,0,0);` |
|  53706 |   34 | `	SyHashInit(&pClass->hAttr,&pVm->sAllocator,0,0);` |
|  53706 |   35 | `	SyHashInit(&pClass->hDerived,&pVm->sAllocator,0,0);` |
|  53706 |   36 | `	SySetInit(&pClass->aInterface,&pVm->sAllocator,sizeof(ph7_class *));` |
|  53706 |   37 | `	SySetInit(&pClass->aTrait,&pVm->sAllocator,sizeof(ph7_class *));` |
|  53706 |   38 | `	pClass->nLine = nLine;` |
|      - |   39 | `	/* All done */` |
|  53706 |   40 | `	return pClass;` |
|  26854 |   41 |  |
|      - |   42 | `/*` |
|      - |   43 | ` * Allocate and initialize a new class attribute.` |
|      - |   44 | ` * Return a pointer to the class attribute on success. NULL otherwise.` |
|      - |   45 | ` */` |
|  56274 |   46 | `PH7_PRIVATE ph7_class_attr * PH7_NewClassAttr(ph7_vm *pVm,const SyString *pName,sxu32 nLine,sxi32 iProtection,sxi32 iFlags)` |
|      2 |   47 |  |
|      - |   48 | `	ph7_class_attr *pAttr;` |
|      - |   49 | `	char *zName;` |
|  56276 |   50 | `	pAttr = (ph7_class_attr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_attr));` |
|  56276 |   51 | `	if( pAttr == 0 ){` |
|    ! 0 |   52 | `		return 0;` |
|      - |   53 | `	}` |
|      - |   54 | `	/* Zero the structure */` |
|  56276 |   55 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|      - |   56 | `	/* Duplicate attribute name */` |
|  56276 |   57 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  56276 |   58 | `	if( zName == 0 ){` |
|    ! 0 |   59 | `		SyMemBackendPoolFree(&pVm->sAllocator,pAttr);` |
|    ! 0 |   60 | `		return 0;` |
|      - |   61 | `	}` |
|      - |   62 | `	/* Initialize fields */` |
|  56276 |   63 | `	SySetInit(&pAttr->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|  56276 |   64 | `	SySetInit(&pAttr->aUnionAlts,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|  56276 |   65 | `	SyStringInitFromBuf(&pAttr->sName,zName,pName->nByte);` |
|  56276 |   66 | `	pAttr->iProtection = iProtection;` |
|  56276 |   67 | `	pAttr->nIdx = SXU32_HIGH;` |
|  56276 |   68 | `	pAttr->iFlags = iFlags;` |
|  56276 |   69 | `	pAttr->nLine = nLine;` |
|  56276 |   70 | `	return pAttr;` |
|  28139 |   71 |  |
|      - |   72 | `/*` |
|      - |   73 | ` * Allocate and initialize a new class method.` |
|      - |   74 | ` * Return a pointer to the class method on success. NULL otherwise` |
|      - |   75 | ` * This function associate with the newly created method an automatically generated` |
|      - |   76 | ` * random unique name.` |
|      - |   77 | ` */` |
| 191410 |   78 | `PH7_PRIVATE ph7_class_method * PH7_NewClassMethod(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,sxu32 nLine,` |
|      - |   79 | `	sxi32 iProtection,sxi32 iFlags,sxi32 iFuncFlags)` |
|      2 |   80 |  |
|      - |   81 | `	ph7_class_method *pMeth;` |
|      - |   82 | `	SyHashEntry *pEntry;` |
|      - |   83 | `	SyString *pNamePtr;` |
|      - |   84 | `	char zSalt[10];` |
|      - |   85 | `	char *zName;` |
|      - |   86 | `	sxu32 nByte;` |
|      - |   87 | `	/* Allocate a new class method instance */` |
| 191412 |   88 | `	pMeth = (ph7_class_method *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_method));` |
| 191412 |   89 | `	if( pMeth == 0 ){` |
|    ! 0 |   90 | `		return 0;` |
|      - |   91 | `	}` |
|      - |   92 | `	/* Zero the structure */` |
| 191412 |   93 | `	SyZero(pMeth,sizeof(ph7_class_method));` |
|      - |   94 | `	/* Check for an already installed method with the same name */` |
| 191412 |   95 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
| 191412 |   96 | `	if( pEntry == 0 ){` |
|      - |   97 | `		/* Associate an unique VM name to this method */` |
| 191410 |   98 | `		nByte = sizeof(zSalt) + pName->nByte + SyStringLength(&pClass->sName)+sizeof(char)*7/*[[__'\0'*/;` |
| 191410 |   99 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,nByte);` |
| 191410 |  100 | `		if( zName == 0 ){` |
|    ! 0 |  101 | `			SyMemBackendPoolFree(&pVm->sAllocator,pMeth);` |
|    ! 0 |  102 | `			return 0;` |
|      - |  103 | `		}` |
| 191410 |  104 | `		pNamePtr = &pMeth->sVmName;` |
|      - |  105 | `		/* Generate a random string */` |
| 191410 |  106 | `		PH7_VmRandomString(&(*pVm),zSalt,sizeof(zSalt));` |
| 191410 |  107 | `		pNamePtr->nByte = SyBufferFormat(zName,nByte,"[__%z@%z_%.*s]",&pClass->sName,pName,sizeof(zSalt),zSalt);` |
| 191410 |  108 | `		pNamePtr->zString = zName;` |
|  95706 |  109 | `	}else{` |
|      - |  110 | `		/* Method is condidate for 'overloading' */` |
|      3 |  111 | `		ph7_class_method *pCurrent = (ph7_class_method *)pEntry->pUserData;` |
|      3 |  112 | `		pNamePtr = &pMeth->sVmName;` |
|      - |  113 | `		/* Use the same VM name */` |
|      3 |  114 | `		SyStringDupPtr(pNamePtr,&pCurrent->sVmName);` |
|      3 |  115 | `		zName = (char *)pNamePtr->zString;` |
|      - |  116 | `	}` |
| 191412 |  117 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|     32 |  118 | `		if( (pName->nByte == sizeof("__construct") - 1 && SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1 ) == 0)` |
|     21 |  119 | `			\|\| (pName->nByte == sizeof("__destruct") - 1 && SyMemcmp(pName->zString,"__destruct",sizeof("__destruct") - 1 ) == 0)` |
|     22 |  120 | `			\|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|      - |  121 | `				/* Switch to public visibility when dealing with constructor/destructor */` |
|      5 |  122 | `				iProtection = PH7_CLASS_PROT_PUBLIC;` |
|      2 |  123 | `		}` |
|     12 |  124 | `	}` |
|      - |  125 | `	/* Initialize method fields */` |
| 191414 |  126 | `	pMeth->iProtection = iProtection;` |
| 191414 |  127 | `	pMeth->iFlags = iFlags;` |
| 191414 |  128 | `	pMeth->nLine = nLine;` |
| 287121 |  129 | `	PH7_VmInitFuncState(&(*pVm),&pMeth->sFunc,&zName[sizeof(char)*4/*[__@*/+SyStringLength(&pClass->sName)],` |
| 191412 |  130 | `		pName->nByte,iFuncFlags\|VM_FUNC_CLASS_METHOD,pClass);` |
| 191414 |  131 | `	return pMeth;` |
|  95709 |  132 |  |
|      - |  133 | `/*` |
|      - |  134 | ` * Check if the given name have a class method associated with it.` |
|      - |  135 | ` * Return the desired method [i.e: ph7_class_method instance] on success. NULL otherwise.` |
|      - |  136 | ` */` |
|  69766 |  137 | `PH7_PRIVATE ph7_class_method * PH7_ClassExtractMethod(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|      2 |  138 |  |
|      - |  139 | `	SyHashEntry *pEntry;` |
|      - |  140 | `	/* Perform a hash lookup */` |
|  69768 |  141 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)zName,nByte);` |
|  69768 |  142 | `	if( pEntry == 0 ){` |
|      - |  143 | `		/* No such entry */` |
|   2774 |  144 | `		return 0;` |
|      - |  145 | `	}` |
|      - |  146 | `	/* Point to the desired method */` |
|  66996 |  147 | `	return (ph7_class_method *)pEntry->pUserData;` |
|  34885 |  148 |  |
|      - |  149 | `/*` |
|      - |  150 | ` * Check if the given name is a class attribute.` |
|      - |  151 | ` * Return the desired attribute [i.e: ph7_class_attr instance] on success.NULL otherwise.` |
|      - |  152 | ` */` |
|  56334 |  153 | `PH7_PRIVATE ph7_class_attr * PH7_ClassExtractAttribute(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|      2 |  154 |  |
|      - |  155 | `	SyHashEntry *pEntry;` |
|      - |  156 | `	/* Perform a hash lookup */` |
|  56336 |  157 | `	pEntry = SyHashGet(&pClass->hAttr,(const void *)zName,nByte);` |
|  56336 |  158 | `	if( pEntry == 0 ){` |
|      - |  159 | `		/* No such entry */` |
|  56246 |  160 | `		return 0;` |
|      - |  161 | `	}` |
|      - |  162 | `	/* Point to the desierd method */` |
|     92 |  163 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|  28169 |  164 |  |
|      - |  165 | `/*` |
|      - |  166 | ` * Install a class attribute in the corresponding container.` |
|      - |  167 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|      - |  168 | ` */` |
|  56274 |  169 | `PH7_PRIVATE sxi32 PH7_ClassInstallAttr(ph7_class *pClass,ph7_class_attr *pAttr)` |
|      2 |  170 |  |
|  56276 |  171 | `	SyString *pName = &pAttr->sName;` |
|      - |  172 | `	sxi32 rc;` |
|      - |  173 | `	/* Remember where this attribute was originally declared so that later` |
|      - |  174 | `	 * inheritance/trait copies still know the declaring class (needed for` |
|      - |  175 | `	 * PHP-compatible error messages on typed properties). */` |
|  56276 |  176 | `	if( pAttr->pDeclClass == 0 ){` |
|  56276 |  177 | `		pAttr->pDeclClass = pClass;` |
|  28137 |  178 | `	}` |
|  56276 |  179 | `	rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|  56276 |  180 | `	return rc;` |
|      2 |  181 |  |
|      - |  182 | `/*` |
|      - |  183 | ` * Install a class method in the corresponding container.` |
|      - |  184 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|      - |  185 | ` */` |
| 191400 |  186 | `PH7_PRIVATE sxi32 PH7_ClassInstallMethod(ph7_class *pClass,ph7_class_method *pMeth)` |
|      2 |  187 |  |
| 191402 |  188 | `	SyString *pName = &pMeth->sFunc.sName;` |
|      - |  189 | `	sxi32 rc;` |
| 191402 |  190 | `	rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 191402 |  191 | `	return rc;` |
|      2 |  192 |  |
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
|  23624 |  234 | `PH7_PRIVATE sxi32 PH7_ClassInherit(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase)` |
|      2 |  235 |  |
|      - |  236 | `	ph7_class_method *pMeth;` |
|      - |  237 | `	ph7_class_attr *pAttr;` |
|      - |  238 | `	SyHashEntry *pEntry;` |
|      - |  239 | `	SyString *pName;` |
|      - |  240 | `	sxi32 rc;` |
|      - |  241 | `	/* Install in the derived hashtable */` |
|  23626 |  242 | `	rc = SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|  23626 |  243 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  244 | `		return rc;` |
|      - |  245 | `	}` |
|      - |  246 | `	/* Copy public/protected attributes from the base class */` |
|  23626 |  247 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
| 164922 |  248 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|      - |  249 | `		/* Make sure the private attributes are not redeclared in the subclass */` |
| 141298 |  250 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
| 141298 |  251 | `		pName = &pAttr->sName;` |
| 141298 |  252 | `		if( (pEntry = SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte)) != 0 ){` |
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
| 141294 |  264 | `		if( pAttr->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
| 141290 |  265 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
| 141290 |  266 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  267 | `				return rc;` |
|      - |  268 | `			}` |
|  70644 |  269 | `		}` |
|      2 |  270 | `	}` |
|  23626 |  271 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
| 235630 |  272 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|      - |  273 | `		/* Make sure the private/final methods are not redeclared in the subclass */` |
| 212006 |  274 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
| 212006 |  275 | `		pName = &pMeth->sFunc.sName;` |
| 212006 |  276 | `		if( (pEntry = SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|   2970 |  277 | `			 if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|      - |  278 | `				/* Cannot Overwrite final method */` |
|      7 |  279 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_method *)pEntry->pUserData)->nLine,` |
|      - |  280 | `					"Cannot Overwrite final method '%z:%z' inside child class '%z'",` |
|      2 |  281 | `					&pBase->sName,pName,&pSub->sName);` |
|      5 |  282 | `				if( rc == SXERR_ABORT ){` |
|    ! 0 |  283 | `					return SXERR_ABORT;` |
|      - |  284 | `				}` |
|      2 |  285 | `			}` |
|   2970 |  286 | `			continue;` |
|      - |  287 | `		}` |
|      - |  288 | `		/* Install the method */` |
| 209038 |  289 | `		if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
| 209036 |  290 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 209036 |  291 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  292 | `				return rc;` |
|      - |  293 | `			}` |
| 104517 |  294 | `		}` |
|      2 |  295 | `	}` |
|      - |  296 | `	/* Mark as subclass */` |
|  23626 |  297 | `	pSub->pBase = pBase;` |
|      - |  298 | `	/* All done */` |
|  23626 |  299 | `	return SXRET_OK;` |
|  11814 |  300 |  |
|      - |  301 | `/*` |
|      - |  302 | ` * Apply a trait to a class: copy all methods and attributes from the trait` |
|      - |  303 | ` * into the target class. Unlike inheritance, traits copy ALL members including` |
|      - |  304 | ` * private ones. Members already defined in the class take precedence.` |
|      - |  305 | ` */` |
|     42 |  306 | `PH7_PRIVATE sxi32 PH7_ClassUseTrait(ph7_gen_state *pGen,ph7_class *pClass,ph7_class *pTrait)` |
|      2 |  307 |  |
|      - |  308 | `	ph7_class_method *pMeth;` |
|      - |  309 | `	ph7_class_attr *pAttr;` |
|      - |  310 | `	SyHashEntry *pEntry;` |
|      - |  311 | `	SyString *pName;` |
|      - |  312 | `	sxi32 rc;` |
|      - |  313 | `	/* Detect cyclic trait composition (e.g. trait A { use B; } trait B { use A; }) */` |
|     44 |  314 | `	if( pTrait->iFlags & PH7_CLASS_TRAIT_VISITING ){` |
|    ! 0 |  315 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|    ! 0 |  316 | `			"Trait circular reference detected: %z is already being applied",&pTrait->sName);` |
|    ! 0 |  317 | `		if( rc == SXERR_ABORT ){` |
|    ! 0 |  318 | `			return SXERR_ABORT;` |
|      - |  319 | `		}` |
|    ! 0 |  320 | `		return SXRET_OK;` |
|      - |  321 | `	}` |
|     44 |  322 | `	pTrait->iFlags \|= PH7_CLASS_TRAIT_VISITING;` |
|     44 |  323 | `	rc = SXRET_OK;` |
|      - |  324 | `	/* Copy attributes from the trait */` |
|     44 |  325 | `	SyHashResetLoopCursor(&pTrait->hAttr);` |
|     60 |  326 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hAttr)) != 0 ){` |
|      - |  327 | `		SyHashEntry *pExisting;` |
|     18 |  328 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     18 |  329 | `		pName = &pAttr->sName;` |
|     18 |  330 | `		pExisting = SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte);` |
|     18 |  331 | `		if( pExisting != 0 ){` |
|      - |  332 | `			/* Attribute already exists. Check if it came from another trait` |
|      - |  333 | `			 * and whether the definitions are compatible (same defaults).` |
|      - |  334 | `			 */` |
|      - |  335 | `			ph7_class **apUsedTraits;` |
|      - |  336 | `			sxu32 nUsed,k;` |
|      5 |  337 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|      5 |  338 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|      5 |  339 | `			for(k = 0; k < nUsed; k++){` |
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
|      5 |  360 | `			continue;` |
|      - |  361 | `		}` |
|     14 |  362 | `		rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|     14 |  363 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  364 | `			goto cleanup;` |
|      - |  365 | `		}` |
|      2 |  366 | `	}` |
|      - |  367 | `	/* Copy methods from the trait */` |
|     44 |  368 | `	SyHashResetLoopCursor(&pTrait->hMethod);` |
|     82 |  369 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hMethod)) != 0 ){` |
|     39 |  370 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|     39 |  371 | `		pName = &pMeth->sFunc.sName;` |
|     39 |  372 | `		if( SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte) != 0 ){` |
|      - |  373 | `			/* Method already exists in the class. Check if it came from another trait` |
|      - |  374 | `			 * (unresolved conflict) vs being defined by the class itself.` |
|      - |  375 | `			 */` |
|      - |  376 | `			ph7_class **apUsedTraits;` |
|      - |  377 | `			sxu32 nUsed,k;` |
|      9 |  378 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|      9 |  379 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|      9 |  380 | `			for(k = 0; k < nUsed; k++){` |
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
|      9 |  396 | `			continue;` |
|      - |  397 | `		}` |
|     31 |  398 | `		rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|     31 |  399 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  400 | `			goto cleanup;` |
|      - |  401 | `		}` |
|      1 |  402 | `	}` |
|      - |  403 | `	/* Record trait in the class */` |
|     44 |  404 | `	SySetPut(&pClass->aTrait,(const void *)&pTrait);` |
|     21 |  405 | `cleanup:` |
|      - |  406 | `	/* Always clear visiting flag, even on error paths */` |
|     44 |  407 | `	pTrait->iFlags &= ~PH7_CLASS_TRAIT_VISITING;` |
|     21 |  408 | `	SXUNUSED(pGen);` |
|     44 |  409 | `	return rc;` |
|     23 |  410 |  |
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
|      6 |  424 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceInherit(ph7_class *pSub,ph7_class *pBase)` |
|      2 |  425 |  |
|      - |  426 | `	ph7_class_method *pMeth;` |
|      - |  427 | `	ph7_class_attr *pAttr;` |
|      - |  428 | `	SyHashEntry *pEntry;` |
|      - |  429 | `	SyString *pName;` |
|      - |  430 | `	sxi32 rc;` |
|      - |  431 | `	/* Install in the derived hashtable */` |
|      8 |  432 | `	SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|      8 |  433 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
|      - |  434 | `	/* Copy constants */` |
|     13 |  435 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
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
|      8 |  447 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
|      - |  448 | `	/* Copy methods signature */` |
|     45 |  449 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|      - |  450 | `		/* Make sure the method are not redeclared in the subclass */` |
|     36 |  451 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|     36 |  452 | `		pName = &pMeth->sFunc.sName;` |
|     36 |  453 | `		if( SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte) == 0 ){` |
|      - |  454 | `			/* Install the method */` |
|     36 |  455 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|     36 |  456 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  457 | `				return rc;` |
|      - |  458 | `			}` |
|     17 |  459 | `		}` |
|      2 |  460 | `	}` |
|      - |  461 | `	/* Mark as subclass */` |
|      8 |  462 | `	pSub->pBase = pBase;` |
|      - |  463 | `	/* All done */` |
|      8 |  464 | `	return SXRET_OK;` |
|      5 |  465 |  |
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
|   8850 |  479 | `PH7_PRIVATE sxi32 PH7_ClassImplement(ph7_class *pMain,ph7_class *pInterface)` |
|      2 |  480 |  |
|      - |  481 | `	ph7_class_attr *pAttr;` |
|      - |  482 | `	SyHashEntry *pEntry;` |
|      - |  483 | `	SyString *pName;` |
|      - |  484 | `	sxi32 rc;` |
|      - |  485 | `	/* First off,copy all constants declared inside the interface */` |
|   8852 |  486 | `	SyHashResetLoopCursor(&pInterface->hAttr);` |
|  13279 |  487 | `	while((pEntry = SyHashGetNextEntry(&pInterface->hAttr)) != 0 ){` |
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
|   8852 |  501 | `	SySetPut(&pMain->aInterface,(const void *)&pInterface);` |
|      - |  502 | `	/* Install interface method stubs into the implementing class.` |
|      - |  503 | `	 * Methods already defined in the class take precedence (they satisfy` |
|      - |  504 | `	 * the interface contract). Stubs retain PH7_CLASS_ATTR_ABSTRACT so` |
|      - |  505 | `	 * the unified check in GenStateCheckAbstractMethods catches missing ones.` |
|      - |  506 | `	 */` |
|      - |  507 | `	{` |
|      - |  508 | `		ph7_class_method *pMeth;` |
|      - |  509 | `		SyHashEntry *pMEntry;` |
|      - |  510 | `		SyString *pMName;` |
|   8852 |  511 | `		SyHashResetLoopCursor(&pInterface->hMethod);` |
|  75043 |  512 | `		while((pMEntry = SyHashGetNextEntry(&pInterface->hMethod)) != 0 ){` |
|  61768 |  513 | `			pMeth = (ph7_class_method *)pMEntry->pUserData;` |
|  61768 |  514 | `			pMName = &pMeth->sFunc.sName;` |
|  61768 |  515 | `			if( SyHashGet(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte) == 0 ){` |
|     15 |  516 | `				rc = SyHashInsert(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte,pMeth);` |
|     15 |  517 | `				if( rc != SXRET_OK ){` |
|    ! 0 |  518 | `					return rc;` |
|      - |  519 | `				}` |
|      7 |  520 | `			}` |
|      2 |  521 | `		}` |
|      - |  522 | `	}` |
|   8852 |  523 | `	return SXRET_OK;` |
|   4427 |  524 |  |
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
|   1712 |  604 | `static ph7_class_instance * NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|      2 |  605 |  |
|      - |  606 | `	ph7_class_instance *pThis;` |
|      - |  607 | `	/* Allocate a new instance */` |
|   1714 |  608 | `	pThis = (ph7_class_instance *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_instance));` |
|   1714 |  609 | `	if( pThis == 0 ){` |
|    ! 0 |  610 | `		return 0;` |
|      - |  611 | `	}` |
|      - |  612 | `	/* Zero the structure */` |
|   1714 |  613 | `	SyZero(pThis,sizeof(ph7_class_instance));` |
|      - |  614 | `	/* Initialize fields */` |
|   1714 |  615 | `	pThis->iRef = 1;` |
|   1714 |  616 | `	pThis->pVm = pVm;` |
|   1714 |  617 | `	pThis->pClass = pClass;` |
|   1714 |  618 | `	SyHashInit(&pThis->hAttr,&pVm->sAllocator,0,0);` |
|   1714 |  619 | `	return pThis;` |
|    858 |  620 |  |
|      - |  621 | `/*` |
|      - |  622 | ` * Wrapper around the NewClassInstance() function defined above.` |
|      - |  623 | ` * See the block comment above for more information.` |
|      - |  624 | ` */` |
|   1668 |  625 | `PH7_PRIVATE ph7_class_instance * PH7_NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|      2 |  626 |  |
|      - |  627 | `	ph7_class_instance *pNew;` |
|      - |  628 | `	sxi32 rc;` |
|   1670 |  629 | `	pNew = NewClassInstance(&(*pVm),&(*pClass));` |
|   1670 |  630 | `	if( pNew == 0 ){` |
|    ! 0 |  631 | `		return 0;` |
|      - |  632 | `	}` |
|      - |  633 | `	/* Associate a private VM frame with this class instance */` |
|   1670 |  634 | `	rc = PH7_VmCreateClassInstanceFrame(&(*pVm),pNew);` |
|   1670 |  635 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  636 | `		SyMemBackendPoolFree(&pVm->sAllocator,pNew);` |
|    ! 0 |  637 | `		return 0;` |
|      - |  638 | `	}` |
|   1670 |  639 | `	return pNew;` |
|    836 |  640 |  |
|      - |  641 | `/*` |
|      - |  642 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon] attribute.` |
|      - |  643 | ` * This function never fail.` |
|      - |  644 | ` */` |
|    968 |  645 | `static ph7_value * ExtractClassAttrValue(ph7_vm *pVm,VmClassAttr *pAttr)` |
|      2 |  646 |  |
|      - |  647 | `	/* Extract the value */` |
|      - |  648 | `	ph7_value *pValue;` |
|    970 |  649 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|    970 |  650 | `	return pValue;` |
|      2 |  651 |  |
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
|   1242 |  794 | `static void PH7_ClassInstanceRelease(ph7_class_instance *pThis)` |
|      2 |  795 |  |
|      - |  796 | `	ph7_class_method *pDestr;` |
|      - |  797 | `	SyHashEntry *pEntry;` |
|      - |  798 | `	ph7_class *pClass;` |
|      - |  799 | `	ph7_vm *pVm;` |
|   1244 |  800 | `	if( pThis->iFlags & CLASS_INSTANCE_DESTROYED ){` |
|      - |  801 | `		/*` |
|      - |  802 | `		 * Already destroyed,return immediately.` |
|      - |  803 | `		 * This could happend if someone perform unset($this) in the destructor body.` |
|      - |  804 | `		 */` |
|    ! 0 |  805 | `		return;` |
|      - |  806 | `	}` |
|      - |  807 | `	/* Mark as destroyed */` |
|   1244 |  808 | `	pThis->iFlags \|= CLASS_INSTANCE_DESTROYED;` |
|      - |  809 | `	/* Invoke any defined destructor if available */` |
|   1244 |  810 | `	pVm = pThis->pVm;` |
|   1244 |  811 | `	pClass = pThis->pClass;` |
|   1244 |  812 | `	pDestr = PH7_ClassExtractMethod(pClass,"__destruct",sizeof("__destruct")-1);` |
|   1244 |  813 | `	if( pDestr ){` |
|      - |  814 | `		/* Invoke the destructor */` |
|      9 |  815 | `		pThis->iRef = 2; /* Prevent garbage collection */` |
|      9 |  816 | `		PH7_VmCallClassMethod(pVm,pThis,pDestr,0,0,0);` |
|      4 |  817 | `	}` |
|      - |  818 | `	/* Release non-static attributes */` |
|   1244 |  819 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|   6088 |  820 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|   4846 |  821 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|   4846 |  822 | `		if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|      - |  823 | `			/* Drop any typed-property enforcement slot registered for this` |
|      - |  824 | `			 * memobj. Must happen before the memobj is returned to the free` |
|      - |  825 | `			 * list so a future recycled slot does not inherit the stale entry. */` |
|   4830 |  826 | `			if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|    184 |  827 | `				SyHashDeleteEntry(&pVm->hTypedSlot,` |
|    122 |  828 | `					(const void *)&pVmAttr->nIdx,sizeof(sxu32),0);` |
|     61 |  829 | `			}` |
|   4830 |  830 | `			PH7_VmUnsetMemObj(pVm,pVmAttr->nIdx,TRUE);` |
|   2414 |  831 | `		}` |
|   4846 |  832 | `		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      2 |  833 | `	}` |
|      - |  834 | `	/* Release the whole structure */` |
|   1244 |  835 | `	SyHashRelease(&pThis->hAttr);` |
|   1244 |  836 | `	SyMemBackendPoolFree(&pVm->sAllocator,pThis);` |
|    623 |  837 |  |
|      - |  838 | `/*` |
|      - |  839 | ` * Decrement the reference count of a class instance [i.e Object in the PHP jargon].` |
|      - |  840 | ` * If the reference count reaches zero,release the whole instance.` |
|      - |  841 | ` */` |
|  22080 |  842 | `PH7_PRIVATE void PH7_ClassInstanceUnref(ph7_class_instance *pThis)` |
|      2 |  843 |  |
|  22082 |  844 | `	pThis->iRef--;` |
|  22082 |  845 | `	if( pThis->iRef < 1 ){` |
|      - |  846 | `		/* No more reference to this instance */` |
|   1244 |  847 | `		PH7_ClassInstanceRelease(&(*pThis));` |
|    621 |  848 | `	}` |
|  22082 |  849 |  |
|      - |  850 | `/*` |
|      - |  851 | ` * Compare two class instances [i.e: Objects in the PHP jargon]` |
|      - |  852 | ` * Note on objects comparison:` |
|      - |  853 | ` *  According to the PHP langauge reference manual` |
|      - |  854 | ` *  When using the comparison operator (==), object variables are compared in a simple manner` |
|      - |  855 | ` *  namely: Two object instances are equal if they have the same attributes and values, and are` |
|      - |  856 | ` *  instances of the same class.` |
|      - |  857 | ` *  On the other hand, when using the identity operator (===), object variables are identical` |
|      - |  858 | ` *  if and only if they refer to the same instance of the same class.` |
|      - |  859 | ` *  An example will clarify these rules.` |
|      - |  860 | ` *  Example #1 Example of object comparison` |
|      - |  861 | ` *  <?php` |
|      - |  862 | ` *    function bool2str($bool)` |
|      - |  863 | ` * {` |
|      - |  864 | ` *   if ($bool === false) {` |
|      - |  865 | ` *       return 'FALSE';` |
|      - |  866 | ` *   } else {` |
|      - |  867 | ` *       return 'TRUE';` |
|      - |  868 | ` *   }` |
|      - |  869 | ` * }` |
|      - |  870 | ` * function compareObjects(&$o1, &$o2)` |
|      - |  871 | ` * {` |
|      - |  872 | ` *   echo 'o1 == o2 : ' . bool2str($o1 == $o2) . "\n";` |
|      - |  873 | ` *   echo 'o1 != o2 : ' . bool2str($o1 != $o2) . "\n";` |
|      - |  874 | ` *   echo 'o1 === o2 : ' . bool2str($o1 === $o2) . "\n";` |
|      - |  875 | ` *   echo 'o1 !== o2 : ' . bool2str($o1 !== $o2) . "\n";` |
|      - |  876 | ` * }` |
|      - |  877 | ` * class Flag` |
|      - |  878 | ` * {` |
|      - |  879 | ` *   public $flag;` |
|      - |  880 | ` *` |
|      - |  881 | ` *   function Flag($flag = true) {` |
|      - |  882 | ` *       $this->flag = $flag;` |
|      - |  883 | ` *   }` |
|      - |  884 | ` * }` |
|      - |  885 | ` *` |
|      - |  886 | ` * class OtherFlag` |
|      - |  887 | ` * {` |
|      - |  888 | ` *   public $flag;` |
|      - |  889 | ` *` |
|      - |  890 | ` *   function OtherFlag($flag = true) {` |
|      - |  891 | ` *       $this->flag = $flag;` |
|      - |  892 | ` *   }` |
|      - |  893 | ` * }` |
|      - |  894 | ` *` |
|      - |  895 | ` * $o = new Flag();` |
|      - |  896 | ` * $p = new Flag();` |
|      - |  897 | ` * $q = $o;` |
|      - |  898 | ` * $r = new OtherFlag();` |
|      - |  899 | ` *` |
|      - |  900 | ` * echo "Two instances of the same class\n";` |
|      - |  901 | ` * compareObjects($o, $p);` |
|      - |  902 | ` * echo "\nTwo references to the same instance\n";` |
|      - |  903 | ` * compareObjects($o, $q);` |
|      - |  904 | ` * echo "\nInstances of two different classes\n";` |
|      - |  905 | ` * compareObjects($o, $r);` |
|      - |  906 | ` * ?>` |
|      - |  907 | ` * The above example will output:` |
|      - |  908 | ` * Two instances of the same class` |
|      - |  909 | ` * o1 == o2 : TRUE` |
|      - |  910 | ` * o1 != o2 : FALSE` |
|      - |  911 | ` * o1 === o2 : FALSE` |
|      - |  912 | ` * o1 !== o2 : TRUE` |
|      - |  913 | ` * Two references to the same instance` |
|      - |  914 | ` * o1 == o2 : TRUE` |
|      - |  915 | ` * o1 != o2 : FALSE` |
|      - |  916 | ` * o1 === o2 : TRUE` |
|      - |  917 | ` * o1 !== o2 : FALSE` |
|      - |  918 | ` * Instances of two different classes` |
|      - |  919 | ` * o1 == o2 : FALSE` |
|      - |  920 | ` * o1 != o2 : TRUE` |
|      - |  921 | ` * o1 === o2 : FALSE` |
|      - |  922 | ` * o1 !== o2 : TRUE` |
|      - |  923 | ` *` |
|      - |  924 | ` * This function return 0 if the objects are equals according to the comprison rules defined above.` |
|      - |  925 | ` * Any other return values indicates difference.` |
|      - |  926 | ` */` |
|    174 |  927 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest)` |
|      2 |  928 |  |
|      - |  929 | `	SyHashEntry *pEntry,*pEntry2;` |
|      - |  930 | `	ph7_value sV1,sV2;` |
|      - |  931 | `	sxi32 rc;` |
|    176 |  932 | `	if( iNest > 31 ){` |
|      - |  933 | `		/* Nesting limit reached */` |
|      5 |  934 | `		PH7_VmThrowError(pLeft->pVm,0,PH7_CTX_ERR,"Nesting limit reached: Infinite recursion?");` |
|      5 |  935 | `		return 1;` |
|      - |  936 | `	}` |
|      - |  937 | `	/* Comparison is performed only if the objects are instance of the same class */` |
|    172 |  938 | `	if( pLeft->pClass != pRight->pClass ){` |
|      7 |  939 | `		return 1;` |
|      - |  940 | `	}` |
|    166 |  941 | `	if( bStrict ){` |
|      - |  942 | `		/*` |
|      - |  943 | `		 * According to the PHP language reference manual:` |
|      - |  944 | `		 *  when using the identity operator (===), object variables` |
|      - |  945 | `		 *  are identical if and only if they refer to the same instance` |
|      - |  946 | `		 *  of the same class.` |
|      - |  947 | `		 */` |
|     25 |  948 | `		return !(pLeft == pRight);` |
|      - |  949 | `	}` |
|      - |  950 | `	/*` |
|      - |  951 | `	 * Attribute comparison.` |
|      - |  952 | `	 * According to the PHP reference manual:` |
|      - |  953 | `	 *  When using the comparison operator (==), object variables are compared` |
|      - |  954 | `	 *  in a simple manner, namely: Two object instances are equal if they have` |
|      - |  955 | `	 *  the same attributes and values, and are instances of the same class.` |
|      - |  956 | `	 */` |
|    142 |  957 | `	if( pLeft == pRight ){` |
|      - |  958 | `		/* Same instance,don't bother processing,object are equals */` |
|      3 |  959 | `		return 0;` |
|      - |  960 | `	}` |
|    140 |  961 | `	SyHashResetLoopCursor(&pLeft->hAttr);` |
|    140 |  962 | `	SyHashResetLoopCursor(&pRight->hAttr);` |
|    140 |  963 | `	PH7_MemObjInit(pLeft->pVm,&sV1);` |
|    140 |  964 | `	PH7_MemObjInit(pLeft->pVm,&sV2);` |
|    140 |  965 | `	sV1.nIdx = sV2.nIdx = SXU32_HIGH;` |
|    223 |  966 | `	while((pEntry = SyHashGetNextEntry(&pLeft->hAttr)) != 0 && (pEntry2 = SyHashGetNextEntry(&pRight->hAttr)) != 0 ){` |
|    146 |  967 | `		VmClassAttr *p1 = (VmClassAttr *)pEntry->pUserData;` |
|    146 |  968 | `		VmClassAttr *p2 = (VmClassAttr *)pEntry2->pUserData;` |
|      - |  969 | `		/* Compare only non-static attribute */` |
|    146 |  970 | `		if( (p1->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|      - |  971 | `			ph7_value *pL,*pR;` |
|    146 |  972 | `			pL = ExtractClassAttrValue(pLeft->pVm,p1);` |
|    146 |  973 | `			pR = ExtractClassAttrValue(pRight->pVm,p2);` |
|    146 |  974 | `			if( pL && pR ){` |
|    146 |  975 | `				PH7_MemObjLoad(pL,&sV1);` |
|    146 |  976 | `				PH7_MemObjLoad(pR,&sV2);` |
|      - |  977 | `				/* Compare the two values now */` |
|    146 |  978 | `				rc = PH7_MemObjCmp(&sV1,&sV2,bStrict,iNest+1);` |
|    146 |  979 | `				PH7_MemObjRelease(&sV1);` |
|    146 |  980 | `				PH7_MemObjRelease(&sV2);` |
|    146 |  981 | `				if( rc != 0 ){` |
|      - |  982 | `					/* Not equals */` |
|    132 |  983 | `					return rc;` |
|      - |  984 | `				}` |
|      7 |  985 | `			}` |
|      7 |  986 | `		}` |
|      1 |  987 | `	}` |
|      - |  988 | `	/* Object are equals */` |
|      9 |  989 | `	return 0;` |
|     89 |  990 |  |
|      - |  991 | `/*` |
|      - |  992 | ` * Dump a class instance and the store the dump in the BLOB given` |
|      - |  993 | ` * as the first argument.` |
|      - |  994 | ` * Note that only non-static/non-constants attribute are dumped.` |
|      - |  995 | ` * This function is typically invoked when the user issue a call` |
|      - |  996 | ` * to [var_dump(),var_export(),print_r(),...].` |
|      - |  997 | ` * This function SXRET_OK on success. Any other return value including` |
|      - |  998 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|      - |  999 | ` */` |
|    132 | 1000 | `PH7_PRIVATE sxi32 PH7_ClassInstanceDump(SyBlob *pOut,ph7_class_instance *pThis,int ShowType,int nTab,int nDepth)` |
|      1 | 1001 |  |
|      - | 1002 | `	SyHashEntry *pEntry;` |
|      - | 1003 | `	ph7_value *pValue;` |
|      - | 1004 | `	sxi32 rc;` |
|      - | 1005 | `	int i;` |
|    133 | 1006 | `	if( nDepth > 31 ){` |
|      - | 1007 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|      - | 1008 | `		/* Nesting limit reached..halt immediately*/` |
|      5 | 1009 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|      5 | 1010 | `		if( ShowType ){` |
|      5 | 1011 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|      2 | 1012 | `		}` |
|      5 | 1013 | `		return SXERR_LIMIT;` |
|      - | 1014 | `	}` |
|    129 | 1015 | `	rc = SXRET_OK;` |
|    129 | 1016 | `	if( !ShowType ){` |
|      3 | 1017 | `		SyBlobAppend(&(*pOut),"Object(",sizeof("Object(")-1);` |
|      1 | 1018 | `	}` |
|      - | 1019 | `	/* Append class name */` |
|    129 | 1020 | `	SyBlobFormat(&(*pOut),"%z) {",&pThis->pClass->sName);` |
|      - | 1021 | `#ifdef __WINNT__` |
|      1 | 1022 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|      - | 1023 | `#else` |
|    128 | 1024 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      - | 1025 | `#endif` |
|      - | 1026 | `	/* Dump object attributes */` |
|    129 | 1027 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|    201 | 1028 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|    133 | 1029 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    133 | 1030 | `		if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|      - | 1031 | `			/* Dump non-static/constant attribute only */` |
|   3985 | 1032 | `			for( i = 0 ; i < nTab ; i++ ){` |
|   3853 | 1033 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|   1927 | 1034 | `			}` |
|    133 | 1035 | `			pValue = ExtractClassAttrValue(pThis->pVm,pVmAttr);` |
|    133 | 1036 | `			if( pValue ){` |
|    133 | 1037 | `				SyBlobFormat(&(*pOut),"['%z'] =>",&pVmAttr->pAttr->sName);` |
|      - | 1038 | `#ifdef __WINNT__` |
|      1 | 1039 | `				SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|      - | 1040 | `#else` |
|    132 | 1041 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      - | 1042 | `#endif` |
|    133 | 1043 | `				rc = PH7_MemObjDump(&(*pOut),pValue,ShowType,nTab+1,nDepth,0);` |
|    133 | 1044 | `				if( rc == SXERR_LIMIT ){` |
|    125 | 1045 | `					break;` |
|      - | 1046 | `				}` |
|      4 | 1047 | `			}` |
|      4 | 1048 | `		}` |
|      1 | 1049 | `	}` |
|   3977 | 1050 | `	for( i = 0 ; i < nTab ; i++ ){` |
|   3849 | 1051 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|   1925 | 1052 | `	}` |
|    129 | 1053 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|    129 | 1054 | `	return rc;` |
|     67 | 1055 |  |
|      - | 1056 | `/*` |
|      - | 1057 | ` * Call a magic method [i.e: __toString(),__toBool(),__Invoke()...]` |
|      - | 1058 | ` * Return SXRET_OK on successfull call. Any other return value indicates failure.` |
|      - | 1059 | ` * Notes on magic methods.` |
|      - | 1060 | ` * According to the PHP language reference manual.` |
|      - | 1061 | ` *  The function names __construct(), __destruct(), __call(), __callStatic()` |
|      - | 1062 | ` *  __get(),  __toString(), __invoke(), __clone() are magical in PHP classes.` |
|      - | 1063 | ` * You cannot have functions with these names in any of your classes unless` |
|      - | 1064 | ` * you want the magic functionality associated with them.` |
|      - | 1065 | ` * Example of magical methods:` |
|      - | 1066 | ` * __toString()` |
|      - | 1067 | ` *  The __toString() method allows a class to decide how it will react when it is treated like` |
|      - | 1068 | ` *  a string. For example, what echo $obj; will print. This method must return a string.` |
|      - | 1069 | ` *  Example #2 Simple example` |
|      - | 1070 | ` * <?php` |
|      - | 1071 | ` * // Declare a simple class` |
|      - | 1072 | ` * class TestClass` |
|      - | 1073 | ` * {` |
|      - | 1074 | ` *   public $foo;` |
|      - | 1075 | ` *` |
|      - | 1076 | ` *   public function __construct($foo)` |
|      - | 1077 | ` *   {` |
|      - | 1078 | ` *       $this->foo = $foo;` |
|      - | 1079 | ` *   }` |
|      - | 1080 | ` *` |
|      - | 1081 | ` *   public function __toString()` |
|      - | 1082 | ` *   {` |
|      - | 1083 | ` *       return $this->foo;` |
|      - | 1084 | ` *   }` |
|      - | 1085 | ` * }` |
|      - | 1086 | ` * $class = new TestClass('Hello');` |
|      - | 1087 | ` * echo $class;` |
|      - | 1088 | ` * ?>` |
|      - | 1089 | ` * The above example will output:` |
|      - | 1090 | ` *  Hello` |
|      - | 1091 | ` *` |
|      - | 1092 | ` * Note that PH7 does not support all the magical method and introudces __toFloat(),__toInt()` |
|      - | 1093 | ` * which have the same behaviour as __toString() but for float and integer types` |
|      - | 1094 | ` * respectively.` |
|      - | 1095 | ` * Refer to the official documentation for more information.` |
|      - | 1096 | ` */` |
|      4 | 1097 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallMagicMethod(` |
|      - | 1098 | `	ph7_vm *pVm,               /* VM that own all this stuff */` |
|      - | 1099 | `	ph7_class *pClass,         /* Target class */` |
|      - | 1100 | `	ph7_class_instance *pThis, /* Target object */` |
|      - | 1101 | `	const char *zMethod,       /* Magic method name [i.e: __toString()]*/` |
|      - | 1102 | `	sxu32 nByte,               /* zMethod length*/` |
|      - | 1103 | `	const SyString *pAttrName  /* Attribute name */` |
|      - | 1104 | `	)` |
|      2 | 1105 |  |
|      6 | 1106 | `	ph7_value *apArg[2] = { 0 , 0 };` |
|      - | 1107 | `	ph7_class_method *pMeth;` |
|      - | 1108 | `	ph7_value sAttr; /* cc warning */` |
|      - | 1109 | `	sxi32 rc;` |
|      - | 1110 | `	int nArg;` |
|      - | 1111 | `	/* Make sure the magic method is available */` |
|      6 | 1112 | `	pMeth = PH7_ClassExtractMethod(&(*pClass),zMethod,nByte);` |
|      6 | 1113 | `	if( pMeth == 0 ){` |
|      - | 1114 | `		/* No such method,return immediately */` |
|      3 | 1115 | `		return SXERR_NOTFOUND;` |
|      - | 1116 | `	}` |
|      3 | 1117 | `	nArg = 0;` |
|      - | 1118 | `	/* Copy arguments */` |
|      3 | 1119 | `	if( pAttrName ){` |
|    ! 0 | 1120 | `		PH7_MemObjInitFromString(pVm,&sAttr,pAttrName);` |
|    ! 0 | 1121 | `		sAttr.nIdx = SXU32_HIGH; /* Mark as constant */` |
|    ! 0 | 1122 | `		apArg[0] = &sAttr;` |
|    ! 0 | 1123 | `		nArg = 1;` |
|    ! 0 | 1124 | `	}` |
|      - | 1125 | `	/* Call the magic method now */` |
|      3 | 1126 | `	rc = PH7_VmCallClassMethod(pVm,&(*pThis),pMeth,0,nArg,apArg);` |
|      - | 1127 | `	/* Clean up */` |
|      3 | 1128 | `	if( pAttrName ){` |
|    ! 0 | 1129 | `		PH7_MemObjRelease(&sAttr);` |
|    ! 0 | 1130 | `	}` |
|      3 | 1131 | `	return rc;` |
|      4 | 1132 |  |
|      - | 1133 | `/*` |
|      - | 1134 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon].` |
|      - | 1135 | ` * This function is simply a wrapper on ExtractClassAttrValue().` |
|      - | 1136 | ` */` |
|     18 | 1137 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceExtractAttrValue(ph7_class_instance *pThis,VmClassAttr *pAttr)` |
|      1 | 1138 |  |
|      - | 1139 | `   /* Extract the attribute value */` |
|      - | 1140 | `	ph7_value *pValue;` |
|     19 | 1141 | `	pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     19 | 1142 | `	return pValue;` |
|      1 | 1143 |  |
|      - | 1144 | `/*` |
|      - | 1145 | ` * Convert a class instance [i.e: Object in the PHP jargon] into a hashmap [i.e: array in the PHP jargon].` |
|      - | 1146 | ` * Return SXRET_OK on success. Any other value indicates failure.` |
|      - | 1147 | ` * Note on object conversion to array:` |
|      - | 1148 | ` *  Acccording to the PHP language reference manual` |
|      - | 1149 | ` *  If an object is converted to an array, the result is an array whose elements are the object's properties.` |
|      - | 1150 | ` *  The keys are the member variable names.` |
|      - | 1151 | ` *` |
|      - | 1152 | ` *  The following example:` |
|      - | 1153 | ` *  class Test {` |
|      - | 1154 | ` *   public $A = 25<<1;  // 50` |
|      - | 1155 | ` *	 public $c = rand_str(3);   // Random string of length 3` |
|      - | 1156 | ` *	 public $d = rand() & 1023; // Random number between 0..1023` |
|      - | 1157 | ` *  }` |
|      - | 1158 | ` *  var_dump((array) new Test());` |
|      - | 1159 | ` *	Will output:` |
|      - | 1160 | ` *  array(3) {` |
|      - | 1161 | ` *   [A] =>` |
|      - | 1162 | ` *      int(50)` |
|      - | 1163 | ` *   [c] =>` |
|      - | 1164 | ` *     string(3 'aps')` |
|      - | 1165 | ` *   [d] =>` |
|      - | 1166 | ` *     int(991)` |
|      - | 1167 | ` *  }` |
|      - | 1168 | ` * You have noticed that PH7 allow class attributes [i.e: $a,$c,$d in the example above]` |
|      - | 1169 | ` * have any complex expression (even function calls/Annonymous functions) as their default` |
|      - | 1170 | ` * value unlike the standard PHP engine.` |
|      - | 1171 | ` * This is a very powerful feature that you have to look at.` |
|      - | 1172 | ` */` |
|      6 | 1173 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|      1 | 1174 |  |
|      - | 1175 | `	SyHashEntry *pEntry;` |
|      - | 1176 | `	SyString *pAttrName;` |
|      - | 1177 | `	VmClassAttr *pAttr;` |
|      - | 1178 | `	ph7_value *pValue;` |
|      - | 1179 | `	ph7_value sName;` |
|      - | 1180 | `	/* Reset the loop cursor */` |
|      7 | 1181 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      7 | 1182 | `	PH7_MemObjInitFromString(pThis->pVm,&sName,0);` |
|     20 | 1183 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|      - | 1184 | `		/* Point to the current attribute */` |
|     11 | 1185 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1186 | `		/* Extract attribute value */` |
|     11 | 1187 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     11 | 1188 | `		if( pValue ){` |
|      - | 1189 | `			/* Build attribute name */` |
|     11 | 1190 | `			pAttrName = &pAttr->pAttr->sName;` |
|     11 | 1191 | `			PH7_MemObjStringAppend(&sName,pAttrName->zString,pAttrName->nByte);` |
|      - | 1192 | `			/* Perform the insertion */` |
|     11 | 1193 | `			PH7_HashmapInsert(pMap,&sName,pValue);` |
|      - | 1194 | `			/* Reset the string cursor */` |
|     11 | 1195 | `			SyBlobReset(&sName.sBlob);` |
|      5 | 1196 | `		}` |
|      1 | 1197 | `	}` |
|      7 | 1198 | `	PH7_MemObjRelease(&sName);` |
|      7 | 1199 | `	return SXRET_OK;` |
|      1 | 1200 |  |
|      - | 1201 | `/*` |
|      - | 1202 | ` * Iterate throw class attributes and invoke the given callback [i.e: xWalk()] for each` |
|      - | 1203 | ` * retrieved attribute.` |
|      - | 1204 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|      - | 1205 | ` * the attribute value in the callback body will not alter the real attribute value.` |
|      - | 1206 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|      - | 1207 | ` * a value different from PH7_OK.` |
|      - | 1208 | ` * Refer to [ph7_object_walk()] for more information.` |
|      - | 1209 | ` */` |
|    ! 0 | 1210 | `PH7_PRIVATE sxi32 PH7_ClassInstanceWalk(` |
|      - | 1211 | `	ph7_class_instance *pThis, /* Target object */` |
|      - | 1212 | `	int (*xWalk)(const char *,ph7_value *,void *), /* Walker callback */` |
|      - | 1213 | `	void *pUserData /* Last argument to xWalk() */` |
|      - | 1214 | `	)` |
|    ! 0 | 1215 |  |
|      - | 1216 | `	SyHashEntry *pEntry; /* Hash entry */` |
|      - | 1217 | `	VmClassAttr *pAttr;  /* Pointer to the attribute */` |
|      - | 1218 | `	ph7_value *pValue;   /* Attribute value */` |
|      - | 1219 | `	ph7_value sValue;    /* Copy of the attribute value */` |
|      - | 1220 | `	int rc;` |
|      - | 1221 | `	/* Reset the loop cursor */` |
|    ! 0 | 1222 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|    ! 0 | 1223 | `	PH7_MemObjInit(pThis->pVm,&sValue);` |
|      - | 1224 | `	/* Start the walk process */` |
|    ! 0 | 1225 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|      - | 1226 | `		/* Point to the current attribute */` |
|    ! 0 | 1227 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1228 | `		/* Extract attribute value */` |
|    ! 0 | 1229 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|    ! 0 | 1230 | `		if( pValue ){` |
|    ! 0 | 1231 | `			PH7_MemObjLoad(pValue,&sValue);` |
|      - | 1232 | `			/* Invoke the supplied callback */` |
|    ! 0 | 1233 | `			rc =  xWalk(SyStringData(&pAttr->pAttr->sName),&sValue,pUserData);` |
|    ! 0 | 1234 | `			PH7_MemObjRelease(&sValue);` |
|    ! 0 | 1235 | `			if( rc != PH7_OK){` |
|      - | 1236 | `				/* User callback request an operation abort */` |
|    ! 0 | 1237 | `				return SXERR_ABORT;` |
|      - | 1238 | `			}` |
|    ! 0 | 1239 | `		}` |
|    ! 0 | 1240 | `	}` |
|      - | 1241 | `	/* All done */` |
|    ! 0 | 1242 | `	return SXRET_OK;` |
|    ! 0 | 1243 |  |
|      - | 1244 | `/*` |
|      - | 1245 | ` * Extract a class atrribute value.` |
|      - | 1246 | ` * Return a pointer to the attribute value on success. Otherwise NULL.` |
|      - | 1247 | ` * Note:` |
|      - | 1248 | ` *  Access to static and constant attribute is not allowed. That is,the function` |
|      - | 1249 | ` *  will return NULL in case someone (host-application code) try to extract` |
|      - | 1250 | ` *  a static/constant attribute.` |
|      - | 1251 | ` */` |
|    424 | 1252 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceFetchAttr(ph7_class_instance *pThis,const SyString *pName)` |
|      2 | 1253 |  |
|      - | 1254 | `	SyHashEntry *pEntry;` |
|      - | 1255 | `	VmClassAttr *pAttr;` |
|      - | 1256 | `	/* Query the attribute hashtable */` |
|    426 | 1257 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);` |
|    426 | 1258 | `	if( pEntry == 0 ){` |
|      - | 1259 | `		/* No such attribute */` |
|    ! 0 | 1260 | `		return 0;` |
|      - | 1261 | `	}` |
|      - | 1262 | `	/* Point to the class atrribute */` |
|    426 | 1263 | `	pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1264 | `	/* Check if we are dealing with a static/constant attribute */` |
|    426 | 1265 | `	if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|      - | 1266 | `		/* Access is forbidden */` |
|    ! 0 | 1267 | `		return 0;` |
|      - | 1268 | `	}` |
|      - | 1269 | `	/* Return the attribute value */` |
|    426 | 1270 | `	return ExtractClassAttrValue(pThis->pVm,pAttr);` |
|    214 | 1271 |  |
|      - | 1272 |  |
