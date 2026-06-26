# src/ph7/oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 498/552 lines (90.22%)

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
| 133662 |   14 | `PH7_PRIVATE ph7_class * PH7_NewRawClass(ph7_vm *pVm,const SyString *pName,sxu32 nLine)` |
|      5 |   15 |  |
|      - |   16 | `	ph7_class *pClass;` |
|      - |   17 | `	char *zName;` |
|      - |   18 | `	/* Allocate a new instance */` |
| 133667 |   19 | `	pClass = (ph7_class *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class));` |
| 133667 |   20 | `	if( pClass == 0 ){` |
|    ! 0 |   21 | `		return 0;` |
|      - |   22 | `	}` |
|      - |   23 | `	/* Zero the structure */` |
| 133667 |   24 | `	SyZero(pClass,sizeof(ph7_class));` |
|      - |   25 | `	/* Duplicate class name */` |
| 133667 |   26 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
| 133667 |   27 | `	if( zName == 0 ){` |
|    ! 0 |   28 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClass);` |
|    ! 0 |   29 | `		return 0;` |
|      - |   30 | `	}` |
|      - |   31 | `	/* Initialize fields */` |
| 133667 |   32 | `	SyStringInitFromBuf(&pClass->sName,zName,pName->nByte);` |
| 133667 |   33 | `	SyHashInit(&pClass->hMethod,&pVm->sAllocator,0,0);` |
| 133667 |   34 | `	SyHashInit(&pClass->hAttr,&pVm->sAllocator,0,0);` |
| 133667 |   35 | `	SyHashInit(&pClass->hDerived,&pVm->sAllocator,0,0);` |
| 133667 |   36 | `	SySetInit(&pClass->aInterface,&pVm->sAllocator,sizeof(ph7_class *));` |
| 133667 |   37 | `	SySetInit(&pClass->aTrait,&pVm->sAllocator,sizeof(ph7_class *));` |
| 133667 |   38 | `	pClass->nLine = nLine;` |
|      - |   39 | `	/* All done */` |
| 133667 |   40 | `	return pClass;` |
|  66836 |   41 |  |
|      - |   42 | `/*` |
|      - |   43 | ` * Allocate and initialize a new class attribute.` |
|      - |   44 | ` * Return a pointer to the class attribute on success. NULL otherwise.` |
|      - |   45 | ` */` |
|  66958 |   46 | `PH7_PRIVATE ph7_class_attr * PH7_NewClassAttr(ph7_vm *pVm,const SyString *pName,sxu32 nLine,sxi32 iProtection,sxi32 iFlags)` |
|      5 |   47 |  |
|      - |   48 | `	ph7_class_attr *pAttr;` |
|      - |   49 | `	char *zName;` |
|  66963 |   50 | `	pAttr = (ph7_class_attr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_attr));` |
|  66963 |   51 | `	if( pAttr == 0 ){` |
|    ! 0 |   52 | `		return 0;` |
|      - |   53 | `	}` |
|      - |   54 | `	/* Zero the structure */` |
|  66963 |   55 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|      - |   56 | `	/* Duplicate attribute name */` |
|  66963 |   57 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  66963 |   58 | `	if( zName == 0 ){` |
|    ! 0 |   59 | `		SyMemBackendPoolFree(&pVm->sAllocator,pAttr);` |
|    ! 0 |   60 | `		return 0;` |
|      - |   61 | `	}` |
|      - |   62 | `	/* Initialize fields */` |
|  66963 |   63 | `	SySetInit(&pAttr->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|  66963 |   64 | `	SySetInit(&pAttr->aUnionAlts,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|  66963 |   65 | `	SyStringInitFromBuf(&pAttr->sName,zName,pName->nByte);` |
|  66963 |   66 | `	pAttr->iProtection = iProtection;` |
|  66963 |   67 | `	pAttr->nIdx = SXU32_HIGH;` |
|  66963 |   68 | `	pAttr->iFlags = iFlags;` |
|  66963 |   69 | `	pAttr->nLine = nLine;` |
|  66963 |   70 | `	return pAttr;` |
|  33484 |   71 |  |
|      - |   72 | `/*` |
|      - |   73 | ` * Allocate and initialize a new class method.` |
|      - |   74 | ` * Return a pointer to the class method on success. NULL otherwise` |
|      - |   75 | ` * This function associate with the newly created method an automatically generated` |
|      - |   76 | ` * random unique name.` |
|      - |   77 | ` */` |
| 262328 |   78 | `PH7_PRIVATE ph7_class_method * PH7_NewClassMethod(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,sxu32 nLine,` |
|      - |   79 | `	sxi32 iProtection,sxi32 iFlags,sxi32 iFuncFlags)` |
|      5 |   80 |  |
|      - |   81 | `	ph7_class_method *pMeth;` |
|      - |   82 | `	SyHashEntry *pEntry;` |
|      - |   83 | `	SyString *pNamePtr;` |
|      - |   84 | `	char zSalt[10];` |
|      - |   85 | `	char *zName;` |
|      - |   86 | `	sxu32 nByte;` |
|      - |   87 | `	/* Allocate a new class method instance */` |
| 262333 |   88 | `	pMeth = (ph7_class_method *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_method));` |
| 262333 |   89 | `	if( pMeth == 0 ){` |
|    ! 0 |   90 | `		return 0;` |
|      - |   91 | `	}` |
|      - |   92 | `	/* Zero the structure */` |
| 262333 |   93 | `	SyZero(pMeth,sizeof(ph7_class_method));` |
|      - |   94 | `	/* Check for an already installed method with the same name */` |
| 262333 |   95 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
| 262333 |   96 | `	if( pEntry == 0 ){` |
|      - |   97 | `		/* Associate an unique VM name to this method */` |
| 262331 |   98 | `		nByte = sizeof(zSalt) + pName->nByte + SyStringLength(&pClass->sName)+sizeof(char)*7/*[[__'\0'*/;` |
| 262331 |   99 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,nByte);` |
| 262331 |  100 | `		if( zName == 0 ){` |
|    ! 0 |  101 | `			SyMemBackendPoolFree(&pVm->sAllocator,pMeth);` |
|    ! 0 |  102 | `			return 0;` |
|      - |  103 | `		}` |
| 262331 |  104 | `		pNamePtr = &pMeth->sVmName;` |
|      - |  105 | `		/* Generate a random string */` |
| 262331 |  106 | `		PH7_VmRandomString(&(*pVm),zSalt,sizeof(zSalt));` |
| 262331 |  107 | `		pNamePtr->nByte = SyBufferFormat(zName,nByte,"[__%z@%z_%.*s]",&pClass->sName,pName,sizeof(zSalt),zSalt);` |
| 262331 |  108 | `		pNamePtr->zString = zName;` |
| 131168 |  109 | `	}else{` |
|      - |  110 | `		/* Method is condidate for 'overloading' */` |
|      3 |  111 | `		ph7_class_method *pCurrent = (ph7_class_method *)pEntry->pUserData;` |
|      3 |  112 | `		pNamePtr = &pMeth->sVmName;` |
|      - |  113 | `		/* Use the same VM name */` |
|      3 |  114 | `		SyStringDupPtr(pNamePtr,&pCurrent->sVmName);` |
|      3 |  115 | `		zName = (char *)pNamePtr->zString;` |
|      - |  116 | `	}` |
| 262333 |  117 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|     22 |  118 | `		if( (pName->nByte == sizeof("__construct") - 1 && SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1 ) == 0)` |
|     21 |  119 | `			\|\| (pName->nByte == sizeof("__destruct") - 1 && SyMemcmp(pName->zString,"__destruct",sizeof("__destruct") - 1 ) == 0)` |
|     22 |  120 | `			\|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|      - |  121 | `				/* Switch to public visibility when dealing with constructor/destructor */` |
|      5 |  122 | `				iProtection = PH7_CLASS_PROT_PUBLIC;` |
|      2 |  123 | `		}` |
|     12 |  124 | `	}` |
|      - |  125 | `	/* Initialize method fields */` |
| 262335 |  126 | `	pMeth->iProtection = iProtection;` |
| 262335 |  127 | `	pMeth->iFlags = iFlags;` |
| 262335 |  128 | `	pMeth->nLine = nLine;` |
| 393501 |  129 | `	PH7_VmInitFuncState(&(*pVm),&pMeth->sFunc,&zName[sizeof(char)*4/*[__@*/+SyStringLength(&pClass->sName)],` |
| 262330 |  130 | `		pName->nByte,iFuncFlags\|VM_FUNC_CLASS_METHOD,pClass);` |
| 262335 |  131 | `	return pMeth;` |
| 131171 |  132 |  |
|      - |  133 | `/*` |
|      - |  134 | ` * Check if the given name have a class method associated with it.` |
|      - |  135 | ` * Return the desired method [i.e: ph7_class_method instance] on success. NULL otherwise.` |
|      - |  136 | ` */` |
| 170608 |  137 | `PH7_PRIVATE ph7_class_method * PH7_ClassExtractMethod(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|      5 |  138 |  |
|      - |  139 | `	SyHashEntry *pEntry;` |
|      - |  140 | `	/* Perform a hash lookup */` |
| 170613 |  141 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)zName,nByte);` |
| 170613 |  142 | `	if( pEntry == 0 ){` |
|      - |  143 | `		/* No such entry */` |
|   4137 |  144 | `		return 0;` |
|      - |  145 | `	}` |
|      - |  146 | `	/* Point to the desired method */` |
| 166481 |  147 | `	return (ph7_class_method *)pEntry->pUserData;` |
|  85309 |  148 |  |
|      - |  149 | `/*` |
|      - |  150 | ` * Check if the given name is a class attribute.` |
|      - |  151 | ` * Return the desired attribute [i.e: ph7_class_attr instance] on success.NULL otherwise.` |
|      - |  152 | ` */` |
|  67028 |  153 | `PH7_PRIVATE ph7_class_attr * PH7_ClassExtractAttribute(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|      5 |  154 |  |
|      - |  155 | `	SyHashEntry *pEntry;` |
|      - |  156 | `	/* Perform a hash lookup */` |
|  67033 |  157 | `	pEntry = SyHashGet(&pClass->hAttr,(const void *)zName,nByte);` |
|  67033 |  158 | `	if( pEntry == 0 ){` |
|      - |  159 | `		/* No such entry */` |
|  66887 |  160 | `		return 0;` |
|      - |  161 | `	}` |
|      - |  162 | `	/* Point to the desierd method */` |
|    151 |  163 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|  33519 |  164 |  |
|      - |  165 | `/*` |
|      - |  166 | ` * Install a class attribute in the corresponding container.` |
|      - |  167 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|      - |  168 | ` */` |
|  66958 |  169 | `PH7_PRIVATE sxi32 PH7_ClassInstallAttr(ph7_class *pClass,ph7_class_attr *pAttr)` |
|      5 |  170 |  |
|  66963 |  171 | `	SyString *pName = &pAttr->sName;` |
|      - |  172 | `	sxi32 rc;` |
|      - |  173 | `	/* Remember where this attribute was originally declared so that later` |
|      - |  174 | `	 * inheritance/trait copies still know the declaring class (needed for` |
|      - |  175 | `	 * PHP-compatible error messages on typed properties). */` |
|  66963 |  176 | `	if( pAttr->pDeclClass == 0 ){` |
|  66963 |  177 | `		pAttr->pDeclClass = pClass;` |
|  33479 |  178 | `	}` |
|  66963 |  179 | `	rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|  66963 |  180 | `	return rc;` |
|      5 |  181 |  |
|      - |  182 | `/*` |
|      - |  183 | ` * Install a class method in the corresponding container.` |
|      - |  184 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|      - |  185 | ` */` |
| 262316 |  186 | `PH7_PRIVATE sxi32 PH7_ClassInstallMethod(ph7_class *pClass,ph7_class_method *pMeth)` |
|      5 |  187 |  |
| 262321 |  188 | `	SyString *pName = &pMeth->sFunc.sName;` |
|      - |  189 | `	sxi32 rc;` |
| 262321 |  190 | `	rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 262321 |  191 | `	return rc;` |
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
|  73318 |  234 | `PH7_PRIVATE sxi32 PH7_ClassInherit(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase)` |
|      5 |  235 |  |
|      - |  236 | `	ph7_class_method *pMeth;` |
|      - |  237 | `	ph7_class_attr *pAttr;` |
|      - |  238 | `	SyHashEntry *pEntry;` |
|      - |  239 | `	SyString *pName;` |
|      - |  240 | `	sxi32 rc;` |
|      - |  241 | `	/* Install in the derived hashtable */` |
|  73323 |  242 | `	rc = SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|  73323 |  243 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  244 | `		return rc;` |
|      - |  245 | `	}` |
|      - |  246 | `	/* readonly class inheritance (PHP 8.2): a readonly class may only extend a` |
|      - |  247 | `	 * readonly class, and a non-readonly class may not extend a readonly one. */` |
|  73323 |  248 | `	if( (pBase->iFlags & PH7_CLASS_READONLY) != (pSub->iFlags & PH7_CLASS_READONLY) ){` |
|      5 |  249 | `		if( pBase->iFlags & PH7_CLASS_READONLY ){` |
|      4 |  250 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|      - |  251 | `				"Non-readonly class %z cannot extend readonly class %z",` |
|      1 |  252 | `				&pSub->sName,&pBase->sName);` |
|      2 |  253 | `		}else{` |
|      4 |  254 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|      - |  255 | `				"Readonly class %z cannot extend non-readonly class %z",` |
|      1 |  256 | `				&pSub->sName,&pBase->sName);` |
|      - |  257 | `		}` |
|      5 |  258 | `		if( rc == SXERR_ABORT ){` |
|    ! 0 |  259 | `			return SXERR_ABORT;` |
|      - |  260 | `		}` |
|      2 |  261 | `	}` |
|      - |  262 | `	/* Copy public/protected attributes from the base class */` |
|  73323 |  263 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
| 512683 |  264 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|      - |  265 | `		/* Make sure the private attributes are not redeclared in the subclass */` |
| 439365 |  266 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
| 439365 |  267 | `		pName = &pAttr->sName;` |
| 439365 |  268 | `		if( (pEntry = SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|      6 |  269 | `			if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_FINAL))` |
|      6 |  270 | `				== (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_FINAL) ){` |
|      - |  271 | `				/* Cannot override a final class constant (PHP 8.1). Report the` |
|      - |  272 | `				 * class that originally declared it (pDeclClass) rather than the` |
|      - |  273 | `				 * immediate base, so a multi-level chain matches PHP. */` |
|      3 |  274 | `				ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pBase;` |
|      4 |  275 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_attr *)pEntry->pUserData)->nLine,` |
|      - |  276 | `					"%z::%z cannot override final constant %z::%z",` |
|      1 |  277 | `					&pSub->sName,pName,&pOwner->sName,pName);` |
|      3 |  278 | `				if( rc == SXERR_ABORT ){` |
|    ! 0 |  279 | `					return SXERR_ABORT;` |
|      - |  280 | `				}` |
|      1 |  281 | `			}` |
|      9 |  282 | `			if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE &&` |
|      2 |  283 | `				((ph7_class_attr *)pEntry->pUserData)->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|      - |  284 | `					/* Cannot redeclare private attribute */` |
|      4 |  285 | `					PH7_GenCompileError(&(*pGen),E_WARNING,((ph7_class_attr *)pEntry->pUserData)->nLine,` |
|      - |  286 | `						"Private attribute '%z::%z' redeclared inside child class '%z'",` |
|      1 |  287 | `						&pBase->sName,pName,&pSub->sName);` |
|      - |  288 |  |
|      1 |  289 | `			}` |
|      9 |  290 | `			continue;` |
|      - |  291 | `		}` |
|      - |  292 | `		/* Install the attribute */` |
| 439359 |  293 | `		if( pAttr->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
| 439355 |  294 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
| 439355 |  295 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  296 | `				return rc;` |
|      - |  297 | `			}` |
| 219675 |  298 | `		}` |
|      5 |  299 | `	}` |
|  73323 |  300 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
| 732427 |  301 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|      - |  302 | `		/* Make sure the private/final methods are not redeclared in the subclass */` |
| 659109 |  303 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
| 659109 |  304 | `		pName = &pMeth->sFunc.sName;` |
| 659109 |  305 | `		if( (pEntry = SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|   3521 |  306 | `			 if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|      - |  307 | `				/* Cannot Overwrite final method */` |
|      8 |  308 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_method *)pEntry->pUserData)->nLine,` |
|      - |  309 | `					"Cannot Overwrite final method '%z:%z' inside child class '%z'",` |
|      2 |  310 | `					&pBase->sName,pName,&pSub->sName);` |
|      6 |  311 | `				if( rc == SXERR_ABORT ){` |
|    ! 0 |  312 | `					return SXERR_ABORT;` |
|      - |  313 | `				}` |
|      2 |  314 | `			}` |
|   3521 |  315 | `			continue;` |
|      - |  316 | `		}` |
|      - |  317 | `		/* Install the method */` |
| 655593 |  318 | `		if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
| 655591 |  319 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 655591 |  320 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  321 | `				return rc;` |
|      - |  322 | `			}` |
| 327793 |  323 | `		}` |
|      5 |  324 | `	}` |
|      - |  325 | `	/* Mark as subclass */` |
|  73323 |  326 | `	pSub->pBase = pBase;` |
|      - |  327 | `	/* All done */` |
|  73323 |  328 | `	return SXRET_OK;` |
|  36664 |  329 |  |
|      - |  330 | `/*` |
|      - |  331 | ` * Apply a trait to a class: copy all methods and attributes from the trait` |
|      - |  332 | ` * into the target class. Unlike inheritance, traits copy ALL members including` |
|      - |  333 | ` * private ones. Members already defined in the class take precedence.` |
|      - |  334 | ` */` |
|     46 |  335 | `PH7_PRIVATE sxi32 PH7_ClassUseTrait(ph7_gen_state *pGen,ph7_class *pClass,ph7_class *pTrait)` |
|      5 |  336 |  |
|      - |  337 | `	ph7_class_method *pMeth;` |
|      - |  338 | `	ph7_class_attr *pAttr;` |
|      - |  339 | `	SyHashEntry *pEntry;` |
|      - |  340 | `	SyString *pName;` |
|      - |  341 | `	sxi32 rc;` |
|      - |  342 | `	/* Detect cyclic trait composition (e.g. trait A { use B; } trait B { use A; }) */` |
|     51 |  343 | `	if( pTrait->iFlags & PH7_CLASS_TRAIT_VISITING ){` |
|    ! 0 |  344 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|    ! 0 |  345 | `			"Trait circular reference detected: %z is already being applied",&pTrait->sName);` |
|    ! 0 |  346 | `		if( rc == SXERR_ABORT ){` |
|    ! 0 |  347 | `			return SXERR_ABORT;` |
|      - |  348 | `		}` |
|    ! 0 |  349 | `		return SXRET_OK;` |
|      - |  350 | `	}` |
|     51 |  351 | `	pTrait->iFlags \|= PH7_CLASS_TRAIT_VISITING;` |
|     51 |  352 | `	rc = SXRET_OK;` |
|      - |  353 | `	/* Copy attributes from the trait */` |
|     51 |  354 | `	SyHashResetLoopCursor(&pTrait->hAttr);` |
|     67 |  355 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hAttr)) != 0 ){` |
|      - |  356 | `		SyHashEntry *pExisting;` |
|     20 |  357 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     20 |  358 | `		pName = &pAttr->sName;` |
|     20 |  359 | `		pExisting = SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte);` |
|     20 |  360 | `		if( pExisting != 0 ){` |
|      - |  361 | `			/* Attribute already exists. Check if it came from another trait` |
|      - |  362 | `			 * and whether the definitions are compatible (same defaults).` |
|      - |  363 | `			 */` |
|      - |  364 | `			ph7_class **apUsedTraits;` |
|      - |  365 | `			sxu32 nUsed,k;` |
|      6 |  366 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|      6 |  367 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|      6 |  368 | `			for(k = 0; k < nUsed; k++){` |
|      - |  369 | `				ph7_class_attr *pOther;` |
|      3 |  370 | `				pOther = PH7_ClassExtractAttribute(apUsedTraits[k],pName->zString,pName->nByte);` |
|      3 |  371 | `				if( pOther ){` |
|      - |  372 | `					/* Two traits define the same property — check if defaults differ */` |
|      3 |  373 | `					ph7_class_attr *pClassAttr = (ph7_class_attr *)pExisting->pUserData;` |
|      4 |  374 | `					if( SySetUsed(&pAttr->aByteCode) != SySetUsed(&pClassAttr->aByteCode) \|\|` |
|      3 |  375 | `						(SySetUsed(&pAttr->aByteCode) > 0 &&` |
|      3 |  376 | `						 SyMemcmp(SySetBasePtr(&pAttr->aByteCode),SySetBasePtr(&pClassAttr->aByteCode),` |
|      3 |  377 | `							SySetUsed(&pAttr->aByteCode) * SySetElemSize(&pAttr->aByteCode)) != 0) ){` |
|      4 |  378 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine,` |
|      - |  379 | `							"%z and %z define the same property ($%z) in the composition of %z. "` |
|      - |  380 | `							"However, the definition differs and is considered incompatible",` |
|      2 |  381 | `							&apUsedTraits[k]->sName,&pTrait->sName,pName,&pClass->sName);` |
|      3 |  382 | `						if( rc == SXERR_ABORT ){` |
|    ! 0 |  383 | `							goto cleanup;` |
|      - |  384 | `						}` |
|      1 |  385 | `					}` |
|      3 |  386 | `					break;` |
|      - |  387 | `				}` |
|    ! 0 |  388 | `			}` |
|      6 |  389 | `			continue;` |
|      - |  390 | `		}` |
|     16 |  391 | `		rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|     16 |  392 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  393 | `			goto cleanup;` |
|      - |  394 | `		}` |
|      4 |  395 | `	}` |
|      - |  396 | `	/* Copy methods from the trait */` |
|     51 |  397 | `	SyHashResetLoopCursor(&pTrait->hMethod);` |
|     93 |  398 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hMethod)) != 0 ){` |
|     46 |  399 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|     46 |  400 | `		pName = &pMeth->sFunc.sName;` |
|     46 |  401 | `		if( SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte) != 0 ){` |
|      - |  402 | `			/* Method already exists in the class. Check if it came from another trait` |
|      - |  403 | `			 * (unresolved conflict) vs being defined by the class itself.` |
|      - |  404 | `			 */` |
|      - |  405 | `			ph7_class **apUsedTraits;` |
|      - |  406 | `			sxu32 nUsed,k;` |
|     11 |  407 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|     11 |  408 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|     11 |  409 | `			for(k = 0; k < nUsed; k++){` |
|      3 |  410 | `				if( PH7_ClassExtractMethod(apUsedTraits[k],pName->zString,pName->nByte) != 0 ){` |
|      - |  411 | `					/* Two different traits define the same method with no resolution */` |
|      4 |  412 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|      - |  413 | `						"Trait method %z::%z has not been applied as %z::%z, "` |
|      - |  414 | `						"because of collision with %z::%z",` |
|      2 |  415 | `						&pTrait->sName,pName,` |
|      1 |  416 | `						&pClass->sName,pName,` |
|      2 |  417 | `						&apUsedTraits[k]->sName,pName);` |
|      3 |  418 | `					if( rc == SXERR_ABORT ){` |
|    ! 0 |  419 | `						goto cleanup;` |
|      - |  420 | `					}` |
|      3 |  421 | `					break;` |
|      - |  422 | `				}` |
|    ! 0 |  423 | `			}` |
|      - |  424 | `			/* Class-defined method takes precedence */` |
|     11 |  425 | `			continue;` |
|      - |  426 | `		}` |
|     38 |  427 | `		rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|     38 |  428 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  429 | `			goto cleanup;` |
|      - |  430 | `		}` |
|      4 |  431 | `	}` |
|      - |  432 | `	/* Record trait in the class */` |
|     51 |  433 | `	SySetPut(&pClass->aTrait,(const void *)&pTrait);` |
|     23 |  434 | `cleanup:` |
|      - |  435 | `	/* Always clear visiting flag, even on error paths */` |
|     51 |  436 | `	pTrait->iFlags &= ~PH7_CLASS_TRAIT_VISITING;` |
|     23 |  437 | `	SXUNUSED(pGen);` |
|     51 |  438 | `	return rc;` |
|     28 |  439 |  |
|      - |  440 | `/*` |
|      - |  441 | ` * Inherit an object interface from another object interface.` |
|      - |  442 | ` * According to the PHP language reference manual.` |
|      - |  443 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|      - |  444 | ` *  must implement, without having to define how these methods are handled.` |
|      - |  445 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|      - |  446 | ` *  class, but without any of the methods having their contents defined.` |
|      - |  447 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|      - |  448 | ` *` |
|      - |  449 | ` * This function return SXRET_OK if the interface inheritance operation was successfully performed.` |
|      - |  450 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|      - |  451 | ` * error message.` |
|      - |  452 | ` */` |
|  10460 |  453 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceInherit(ph7_class *pSub,ph7_class *pBase)` |
|      5 |  454 |  |
|      - |  455 | `	ph7_class_method *pMeth;` |
|      - |  456 | `	ph7_class_attr *pAttr;` |
|      - |  457 | `	SyHashEntry *pEntry;` |
|      - |  458 | `	SyString *pName;` |
|      - |  459 | `	sxi32 rc;` |
|      - |  460 | `	/* Install in the derived hashtable */` |
|  10465 |  461 | `	SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|  10465 |  462 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
|      - |  463 | `	/* Copy constants */` |
|  15697 |  464 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|      - |  465 | `		/* Make sure the constants are not redeclared in the subclass */` |
|      3 |  466 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      3 |  467 | `		pName = &pAttr->sName;` |
|      3 |  468 | `		if( SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte) == 0 ){` |
|      - |  469 | `			/* Install the constant in the subclass */` |
|      3 |  470 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|      3 |  471 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  472 | `				return rc;` |
|      - |  473 | `			}` |
|      1 |  474 | `		}` |
|      1 |  475 | `	}` |
|  10465 |  476 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
|      - |  477 | `	/* Copy methods signature */` |
|  19221 |  478 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|      - |  479 | `		/* Make sure the method are not redeclared in the subclass */` |
|   3531 |  480 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   3531 |  481 | `		pName = &pMeth->sFunc.sName;` |
|   3531 |  482 | `		if( SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte) == 0 ){` |
|      - |  483 | `			/* Install the method */` |
|   3531 |  484 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|   3531 |  485 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  486 | `				return rc;` |
|      - |  487 | `			}` |
|   1763 |  488 | `		}` |
|      5 |  489 | `	}` |
|      - |  490 | `	/* Mark as subclass */` |
|  10465 |  491 | `	pSub->pBase = pBase;` |
|      - |  492 | `	/* All done */` |
|  10465 |  493 | `	return SXRET_OK;` |
|   5235 |  494 |  |
|      - |  495 | `/*` |
|      - |  496 | ` * Implements an object interface in the given main class.` |
|      - |  497 | ` * According to the PHP language reference manual.` |
|      - |  498 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|      - |  499 | ` *  must implement, without having to define how these methods are handled.` |
|      - |  500 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|      - |  501 | ` *  class, but without any of the methods having their contents defined.` |
|      - |  502 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|      - |  503 | ` *` |
|      - |  504 | ` * This function return SXRET_OK if the interface was successfully implemented.` |
|      - |  505 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|      - |  506 | ` * error message.` |
|      - |  507 | ` */` |
|  94296 |  508 | `PH7_PRIVATE sxi32 PH7_ClassImplement(ph7_class *pMain,ph7_class *pInterface)` |
|      5 |  509 |  |
|      - |  510 | `	ph7_class_attr *pAttr;` |
|      - |  511 | `	SyHashEntry *pEntry;` |
|      - |  512 | `	SyString *pName;` |
|      - |  513 | `	sxi32 rc;` |
|      - |  514 | `	/* First off,copy all constants declared inside the interface */` |
|  94301 |  515 | `	SyHashResetLoopCursor(&pInterface->hAttr);` |
| 141455 |  516 | `	while((pEntry = SyHashGetNextEntry(&pInterface->hAttr)) != 0 ){` |
|      - |  517 | `		/* Point to the constant declaration */` |
|      7 |  518 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      7 |  519 | `		pName = &pAttr->sName;` |
|      - |  520 | `		/* Make sure the attribute is not redeclared in the main class */` |
|      7 |  521 | `		if( SyHashGet(&pMain->hAttr,pName->zString,pName->nByte) == 0 ){` |
|      - |  522 | `			/* Install the attribute */` |
|      7 |  523 | `			rc = SyHashInsert(&pMain->hAttr,pName->zString,pName->nByte,pAttr);` |
|      7 |  524 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  525 | `				return rc;` |
|      - |  526 | `			}` |
|      3 |  527 | `		}` |
|      1 |  528 | `	}` |
|      - |  529 | `	/* Install in the interface container */` |
|  94301 |  530 | `	SySetPut(&pMain->aInterface,(const void *)&pInterface);` |
|      - |  531 | `	/* Install interface method stubs into the implementing class.` |
|      - |  532 | `	 * Methods already defined in the class take precedence (they satisfy` |
|      - |  533 | `	 * the interface contract). Stubs retain PH7_CLASS_ATTR_ABSTRACT so` |
|      - |  534 | `	 * the unified check in GenStateCheckAbstractMethods catches missing ones.` |
|      - |  535 | `	 */` |
|      - |  536 | `	{` |
|      - |  537 | `		ph7_class_method *pMeth;` |
|      - |  538 | `		SyHashEntry *pMEntry;` |
|      - |  539 | `		SyString *pMName;` |
|  94301 |  540 | `		SyHashResetLoopCursor(&pInterface->hMethod);` |
| 298675 |  541 | `		while((pMEntry = SyHashGetNextEntry(&pInterface->hMethod)) != 0 ){` |
| 157231 |  542 | `			pMeth = (ph7_class_method *)pMEntry->pUserData;` |
| 157231 |  543 | `			pMName = &pMeth->sFunc.sName;` |
| 157231 |  544 | `			if( SyHashGet(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte) == 0 ){` |
|     18 |  545 | `				rc = SyHashInsert(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte,pMeth);` |
|     18 |  546 | `				if( rc != SXRET_OK ){` |
|    ! 0 |  547 | `					return rc;` |
|      - |  548 | `				}` |
|      7 |  549 | `			}` |
|      5 |  550 | `		}` |
|      - |  551 | `	}` |
|  94301 |  552 | `	return SXRET_OK;` |
|  47153 |  553 |  |
|      - |  554 | `/*` |
|      - |  555 | ` * Create a class instance [i.e: Object in the PHP jargon] at run-time.` |
|      - |  556 | ` * The following function is called when an object is created at run-time` |
|      - |  557 | ` * typically when the PH7_OP_NEW/PH7_OP_CLONE instructions are executed.` |
|      - |  558 | ` * Notes on object creation.` |
|      - |  559 | ` *` |
|      - |  560 | ` * According to PHP language reference manual.` |
|      - |  561 | ` * To create an instance of a class, the new keyword must be used. An object will always` |
|      - |  562 | ` * be created unless the object has a constructor defined that throws an exception on error.` |
|      - |  563 | ` * Classes should be defined before instantiation (and in some cases this is a requirement).` |
|      - |  564 | ` * If a string containing the name of a class is used with new, a new instance of that class` |
|      - |  565 | ` * will be created. If the class is in a namespace, its fully qualified name must be used when` |
|      - |  566 | ` * doing this.` |
|      - |  567 | ` * Example #3 Creating an instance` |
|      - |  568 | ` * <?php` |
|      - |  569 | ` *  $instance = new SimpleClass();` |
|      - |  570 | ` *   // This can also be done with a variable:` |
|      - |  571 | ` * $className = 'Foo';` |
|      - |  572 | ` * $instance = new $className(); // Foo()` |
|      - |  573 | ` * ?>` |
|      - |  574 | ` * In the class context, it is possible to create a new object by new self and new parent.` |
|      - |  575 | ` * When assigning an already created instance of a class to a new variable, the new variable` |
|      - |  576 | ` * will access the same instance as the object that was assigned. This behaviour is the same` |
|      - |  577 | ` * when passing instances to a function. A copy of an already created object can be made by` |
|      - |  578 | ` * cloning it.` |
|      - |  579 | ` * Example #4 Object Assignment` |
|      - |  580 | ` * <?php` |
|      - |  581 | ` *  class SimpleClass(){` |
|      - |  582 | ` *    public $var;` |
|      - |  583 | ` *  };` |
|      - |  584 | ` *  $instance = new SimpleClass();` |
|      - |  585 | ` *  $assigned   =  $instance;` |
|      - |  586 | ` *  $reference  =& $instance;` |
|      - |  587 | ` *  $instance->var = '$assigned will have this value';` |
|      - |  588 | ` *  $instance = null; // $instance and $reference become null` |
|      - |  589 | ` *  var_dump($instance);` |
|      - |  590 | ` *  var_dump($reference);` |
|      - |  591 | ` *  var_dump($assigned);` |
|      - |  592 | ` * ?>` |
|      - |  593 | ` * The above example will output:` |
|      - |  594 | ` * NULL` |
|      - |  595 | ` * NULL` |
|      - |  596 | ` * object(SimpleClass)#1 (1) {` |
|      - |  597 | ` *  ["var"]=>` |
|      - |  598 | ` *    string(30) "$assigned will have this value"` |
|      - |  599 | ` * }` |
|      - |  600 | ` * Example #5 Creating new objects` |
|      - |  601 | ` * <?php` |
|      - |  602 | ` * class Test` |
|      - |  603 | ` * {` |
|      - |  604 | ` *   static public function getNew()` |
|      - |  605 | ` *   {` |
|      - |  606 | ` *       return new static;` |
|      - |  607 | ` *   }` |
|      - |  608 | ` * }` |
|      - |  609 | ` * class Child extends Test` |
|      - |  610 | ` * {}` |
|      - |  611 | ` * $obj1 = new Test();` |
|      - |  612 | ` * $obj2 = new $obj1;` |
|      - |  613 | ` * var_dump($obj1 !== $obj2);` |
|      - |  614 | ` * $obj3 = Test::getNew();` |
|      - |  615 | ` * var_dump($obj3 instanceof Test);` |
|      - |  616 | ` * $obj4 = Child::getNew();` |
|      - |  617 | ` * var_dump($obj4 instanceof Child);` |
|      - |  618 | ` * ?>` |
|      - |  619 | ` * The above example will output:` |
|      - |  620 | ` * bool(true)` |
|      - |  621 | ` * bool(true)` |
|      - |  622 | ` * bool(true)` |
|      - |  623 | ` * Note that Symisc Systems have introduced powerfull extension to` |
|      - |  624 | ` * OO subsystem. For example a class attribute may have any complex` |
|      - |  625 | ` * expression associated with it when declaring the attribute unlike` |
|      - |  626 | ` * the standard PHP engine which would allow a single value.` |
|      - |  627 | ` * Example:` |
|      - |  628 | ` *  class myClass{` |
|      - |  629 | ` *    public $var = 25<<1+foo()/bar();` |
|      - |  630 | ` *  };` |
|      - |  631 | ` * Refer to the official documentation for more information.` |
|      - |  632 | ` */` |
|   2550 |  633 | `static ph7_class_instance * NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|      5 |  634 |  |
|      - |  635 | `	ph7_class_instance *pThis;` |
|      - |  636 | `	/* Allocate a new instance */` |
|   2555 |  637 | `	pThis = (ph7_class_instance *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_instance));` |
|   2555 |  638 | `	if( pThis == 0 ){` |
|    ! 0 |  639 | `		return 0;` |
|      - |  640 | `	}` |
|      - |  641 | `	/* Zero the structure */` |
|   2555 |  642 | `	SyZero(pThis,sizeof(ph7_class_instance));` |
|      - |  643 | `	/* Initialize fields */` |
|   2555 |  644 | `	pThis->iRef = 1;` |
|   2555 |  645 | `	pThis->pVm = pVm;` |
|   2555 |  646 | `	pThis->pClass = pClass;` |
|      - |  647 | `	/* Assign a fresh monotonic object handle id (clones get their own, like PHP). */` |
|   2555 |  648 | `	pThis->nObjId = pVm->nNextObjId++;` |
|   2555 |  649 | `	SyHashInit(&pThis->hAttr,&pVm->sAllocator,0,0);` |
|   2555 |  650 | `	return pThis;` |
|   1280 |  651 |  |
|      - |  652 | `/*` |
|      - |  653 | ` * Wrapper around the NewClassInstance() function defined above.` |
|      - |  654 | ` * See the block comment above for more information.` |
|      - |  655 | ` */` |
|   2498 |  656 | `PH7_PRIVATE ph7_class_instance * PH7_NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|      5 |  657 |  |
|      - |  658 | `	ph7_class_instance *pNew;` |
|      - |  659 | `	sxi32 rc;` |
|   2503 |  660 | `	pNew = NewClassInstance(&(*pVm),&(*pClass));` |
|   2503 |  661 | `	if( pNew == 0 ){` |
|    ! 0 |  662 | `		return 0;` |
|      - |  663 | `	}` |
|      - |  664 | `	/* Associate a private VM frame with this class instance */` |
|   2503 |  665 | `	rc = PH7_VmCreateClassInstanceFrame(&(*pVm),pNew);` |
|   2503 |  666 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  667 | `		SyMemBackendPoolFree(&pVm->sAllocator,pNew);` |
|    ! 0 |  668 | `		return 0;` |
|      - |  669 | `	}` |
|   2503 |  670 | `	return pNew;` |
|   1254 |  671 |  |
|      - |  672 | `/*` |
|      - |  673 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon] attribute.` |
|      - |  674 | ` * This function never fail.` |
|      - |  675 | ` */` |
|   1872 |  676 | `static ph7_value * ExtractClassAttrValue(ph7_vm *pVm,VmClassAttr *pAttr)` |
|      5 |  677 |  |
|      - |  678 | `	/* Extract the value */` |
|      - |  679 | `	ph7_value *pValue;` |
|   1877 |  680 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|   1877 |  681 | `	return pValue;` |
|      5 |  682 |  |
|      - |  683 | `/*` |
|      - |  684 | ` * Perform a clone operation on a class instance [i.e: Object in the PHP jargon].` |
|      - |  685 | ` * The following function is called when an object is cloned at run-time` |
|      - |  686 | ` * typically when the PH7_OP_CLONE instruction is executed.` |
|      - |  687 | ` * Notes on object cloning.` |
|      - |  688 | ` *` |
|      - |  689 | ` * According to PHP language reference manual.` |
|      - |  690 | ` * Creating a copy of an object with fully replicated properties is not always the wanted behavior.` |
|      - |  691 | ` * A good example of the need for copy constructors. Another example is if your object holds a reference` |
|      - |  692 | ` * to another object which it uses and when you replicate the parent object you want to create` |
|      - |  693 | ` * a new instance of this other object so that the replica has its own separate copy.` |
|      - |  694 | ` * An object copy is created by using the clone keyword (which calls the object's __clone() method if possible).` |
|      - |  695 | ` * An object's __clone() method cannot be called directly.` |
|      - |  696 | ` * $copy_of_object = clone $object;` |
|      - |  697 | ` * When an object is cloned, PHP 5 will perform a shallow copy of all of the object's properties.` |
|      - |  698 | ` * Any properties that are references to other variables, will remain references.` |
|      - |  699 | ` * Once the cloning is complete, if a __clone() method is defined, then the newly created object's __clone() method` |
|      - |  700 | ` * will be called, to allow any necessary properties that need to be changed.` |
|      - |  701 | ` * Example #1 Cloning an object` |
|      - |  702 | ` * <?php` |
|      - |  703 | ` * class SubObject` |
|      - |  704 | ` * {` |
|      - |  705 | ` *   static $instances = 0;` |
|      - |  706 | ` *   public $instance;` |
|      - |  707 | ` *` |
|      - |  708 | ` *   public function __construct() {` |
|      - |  709 | ` *       $this->instance = ++self::$instances;` |
|      - |  710 | ` *   }` |
|      - |  711 | ` *` |
|      - |  712 | ` *   public function __clone() {` |
|      - |  713 | ` *       $this->instance = ++self::$instances;` |
|      - |  714 | ` *   }` |
|      - |  715 | ` * }` |
|      - |  716 | ` *` |
|      - |  717 | ` * class MyCloneable` |
|      - |  718 | ` * {` |
|      - |  719 | ` *   public $object1;` |
|      - |  720 | ` *   public $object2;` |
|      - |  721 | ` *` |
|      - |  722 | ` *   function __clone()` |
|      - |  723 | ` *   {` |
|      - |  724 | ` *       // Force a copy of this->object, otherwise` |
|      - |  725 | ` *       // it will point to same object.` |
|      - |  726 | ` *       $this->object1 = clone $this->object1;` |
|      - |  727 | ` *   }` |
|      - |  728 | ` * }` |
|      - |  729 | ` * $obj = new MyCloneable();` |
|      - |  730 | ` * $obj->object1 = new SubObject();` |
|      - |  731 | ` * $obj->object2 = new SubObject();` |
|      - |  732 | ` * $obj2 = clone $obj;` |
|      - |  733 | ` * print("Original Object:\n");` |
|      - |  734 | ` * print_r($obj);` |
|      - |  735 | ` * print("Cloned Object:\n");` |
|      - |  736 | ` * print_r($obj2);` |
|      - |  737 | ` * ?>` |
|      - |  738 | ` * The above example will output:` |
|      - |  739 | ` * Original Object:` |
|      - |  740 | ` * MyCloneable Object` |
|      - |  741 | ` * (` |
|      - |  742 | ` *   [object1] => SubObject Object` |
|      - |  743 | ` *       (` |
|      - |  744 | ` *           [instance] => 1` |
|      - |  745 | ` *       )` |
|      - |  746 | ` *` |
|      - |  747 | ` *   [object2] => SubObject Object` |
|      - |  748 | ` *       (` |
|      - |  749 | ` *           [instance] => 2` |
|      - |  750 | ` *       )` |
|      - |  751 | ` *` |
|      - |  752 | ` * )` |
|      - |  753 | ` * Cloned Object:` |
|      - |  754 | ` * MyCloneable Object` |
|      - |  755 | ` * (` |
|      - |  756 | ` *   [object1] => SubObject Object` |
|      - |  757 | ` *       (` |
|      - |  758 | ` *           [instance] => 3` |
|      - |  759 | ` *       )` |
|      - |  760 | ` *` |
|      - |  761 | ` *   [object2] => SubObject Object` |
|      - |  762 | ` *       (` |
|      - |  763 | ` *           [instance] => 2` |
|      - |  764 | ` *       )` |
|      - |  765 | ` * )` |
|      - |  766 | ` */` |
|     52 |  767 | `PH7_PRIVATE ph7_class_instance * PH7_CloneClassInstance(ph7_class_instance *pSrc)` |
|      4 |  768 |  |
|      - |  769 | `	ph7_class_instance *pClone;` |
|      - |  770 | `	ph7_class_method *pMethod;` |
|      - |  771 | `	SyHashEntry *pEntry2;` |
|      - |  772 | `	SyHashEntry *pEntry;` |
|      - |  773 | `	ph7_vm *pVm;` |
|      - |  774 | `	sxi32 rc;` |
|      - |  775 | `	/* Allocate a new instance */` |
|     56 |  776 | `	pVm = pSrc->pVm;` |
|     56 |  777 | `	pClone = NewClassInstance(pVm,pSrc->pClass);` |
|     56 |  778 | `	if( pClone == 0 ){` |
|    ! 0 |  779 | `		return 0;` |
|      - |  780 | `	}` |
|      - |  781 | `	/* Associate a private VM frame with this class instance */` |
|     56 |  782 | `	rc = PH7_VmCreateClassInstanceFrame(pVm,pClone);` |
|     56 |  783 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  784 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClone);` |
|    ! 0 |  785 | `		return 0;` |
|      - |  786 | `	}` |
|      - |  787 | `	/* Duplicate object values */` |
|     56 |  788 | `	SyHashResetLoopCursor(&pSrc->hAttr);` |
|     56 |  789 | `	SyHashResetLoopCursor(&pClone->hAttr);` |
|    138 |  790 | `	while((pEntry = SyHashGetNextEntry(&pSrc->hAttr)) != 0 && (pEntry2 = SyHashGetNextEntry(&pClone->hAttr)) != 0 ){` |
|     60 |  791 | `		VmClassAttr *pSrcAttr = (VmClassAttr *)pEntry->pUserData;` |
|     60 |  792 | `		VmClassAttr *pDestAttr = (VmClassAttr *)pEntry2->pUserData;` |
|      - |  793 | `		/* Duplicate non-static attribute */` |
|     60 |  794 | `		if( (pSrcAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|      - |  795 | `			ph7_value *pvSrc,*pvDest;` |
|     60 |  796 | `			pvSrc = ExtractClassAttrValue(pVm,pSrcAttr);` |
|     60 |  797 | `			pvDest = ExtractClassAttrValue(pVm,pDestAttr);` |
|     60 |  798 | `			if( pvSrc && pvDest ){` |
|     60 |  799 | `				PH7_MemObjStore(pvSrc,pvDest);` |
|     28 |  800 | `			}` |
|      - |  801 | `			/* Carry over the per-instance state so the clone matches the source:` |
|      - |  802 | `			 * VM_CLASS_ATTR_UNINIT marks a typed property as not-yet-initialized` |
|      - |  803 | `			 * and doubles as the readonly write-once latch — without this a clone` |
|      - |  804 | `			 * would reset to uninitialized (losing the value's readiness) and a` |
|      - |  805 | `			 * readonly property would become writable again. */` |
|     60 |  806 | `			pDestAttr->iState = pSrcAttr->iState;` |
|     28 |  807 | `		}` |
|      4 |  808 | `	}` |
|      - |  809 | `	/* call the __clone method on the cloned object if available */` |
|     56 |  810 | `	pMethod = PH7_ClassExtractMethod(pClone->pClass,"__clone",sizeof("__clone")-1);` |
|     56 |  811 | `	if( pMethod ){` |
|     38 |  812 | `		if( pMethod->iCloneDepth < 16 ){` |
|     36 |  813 | `			pMethod->iCloneDepth++;` |
|     36 |  814 | `			PH7_VmCallClassMethod(pVm,pClone,pMethod,0,0,0);` |
|     19 |  815 | `		}else{` |
|      - |  816 | `			/* Nesting limit reached */` |
|      3 |  817 | `			PH7_VmThrowError(pVm,0,PH7_CTX_ERR,"Object clone limit reached,no more call to __clone()");` |
|      - |  818 | `		}` |
|      - |  819 | `		/* Reset the cursor */` |
|     38 |  820 | `		pMethod->iCloneDepth = 0;` |
|     18 |  821 | `	}` |
|      - |  822 | `	/* Return the cloned object */` |
|     56 |  823 | `	return pClone;` |
|     30 |  824 |  |
|      - |  825 | `#define CLASS_INSTANCE_DESTROYED 0x001 /* Instance is released */` |
|      - |  826 | `/*` |
|      - |  827 | ` * Release a class instance [i.e: Object in the PHP jargon] and invoke any defined destructor.` |
|      - |  828 | ` * This routine is invoked as soon as there are no other references to a particular` |
|      - |  829 | ` * class instance.` |
|      - |  830 | ` */` |
|   1816 |  831 | `static void PH7_ClassInstanceRelease(ph7_class_instance *pThis)` |
|      5 |  832 |  |
|      - |  833 | `	ph7_class_method *pDestr;` |
|      - |  834 | `	SyHashEntry *pEntry;` |
|      - |  835 | `	ph7_class *pClass;` |
|      - |  836 | `	ph7_vm *pVm;` |
|   1821 |  837 | `	if( pThis->iFlags & CLASS_INSTANCE_DESTROYED ){` |
|      - |  838 | `		/*` |
|      - |  839 | `		 * Already destroyed,return immediately.` |
|      - |  840 | `		 * This could happend if someone perform unset($this) in the destructor body.` |
|      - |  841 | `		 */` |
|      9 |  842 | `		return;` |
|      - |  843 | `	}` |
|      - |  844 | `	/* Mark as destroyed */` |
|   1813 |  845 | `	pThis->iFlags \|= CLASS_INSTANCE_DESTROYED;` |
|      - |  846 | `	/* Invoke any defined destructor if available */` |
|   1813 |  847 | `	pVm = pThis->pVm;` |
|   1813 |  848 | `	pClass = pThis->pClass;` |
|   1813 |  849 | `	pDestr = PH7_ClassExtractMethod(pClass,"__destruct",sizeof("__destruct")-1);` |
|   1813 |  850 | `	if( pDestr && !pVm->bInReset ){` |
|      - |  851 | `		/* Invoke the destructor. Skipped during ph7_vm_reset() bulk teardown:` |
|      - |  852 | `		 * running user PHP against a half-reset VM is unsafe (see bInReset). */` |
|     13 |  853 | `		pThis->iRef = 2; /* Prevent garbage collection */` |
|     13 |  854 | `		PH7_VmCallClassMethod(pVm,pThis,pDestr,0,0,0);` |
|      6 |  855 | `	}` |
|      - |  856 | `	/* Release non-static attributes */` |
|   1813 |  857 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|   8619 |  858 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|   6811 |  859 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|   6811 |  860 | `		if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|      - |  861 | `			/* Drop any typed-property enforcement slot registered for this` |
|      - |  862 | `			 * memobj. Must happen before the memobj is returned to the free` |
|      - |  863 | `			 * list so a future recycled slot does not inherit the stale entry. */` |
|   6789 |  864 | `			if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|    268 |  865 | `				SyHashDeleteEntry(&pVm->hTypedSlot,` |
|    176 |  866 | `					(const void *)&pVmAttr->nIdx,sizeof(sxu32),0);` |
|     88 |  867 | `			}` |
|   6789 |  868 | `			PH7_VmUnsetMemObj(pVm,pVmAttr->nIdx,TRUE);` |
|   3392 |  869 | `		}` |
|   6811 |  870 | `		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      5 |  871 | `	}` |
|      - |  872 | `	/* Release the whole structure */` |
|   1813 |  873 | `	SyHashRelease(&pThis->hAttr);` |
|   1813 |  874 | `	SyMemBackendPoolFree(&pVm->sAllocator,pThis);` |
|    913 |  875 |  |
|      - |  876 | `/*` |
|      - |  877 | ` * Decrement the reference count of a class instance [i.e Object in the PHP jargon].` |
|      - |  878 | ` * If the reference count reaches zero,release the whole instance.` |
|      - |  879 | ` */` |
|  33934 |  880 | `PH7_PRIVATE void PH7_ClassInstanceUnref(ph7_class_instance *pThis)` |
|      5 |  881 |  |
|  33939 |  882 | `	pThis->iRef--;` |
|  33939 |  883 | `	if( pThis->iRef < 1 ){` |
|      - |  884 | `		/* No more reference to this instance */` |
|   1821 |  885 | `		PH7_ClassInstanceRelease(&(*pThis));` |
|    908 |  886 | `	}` |
|  33939 |  887 |  |
|      - |  888 | `/*` |
|      - |  889 | ` * Compare two class instances [i.e: Objects in the PHP jargon]` |
|      - |  890 | ` * Note on objects comparison:` |
|      - |  891 | ` *  According to the PHP langauge reference manual` |
|      - |  892 | ` *  When using the comparison operator (==), object variables are compared in a simple manner` |
|      - |  893 | ` *  namely: Two object instances are equal if they have the same attributes and values, and are` |
|      - |  894 | ` *  instances of the same class.` |
|      - |  895 | ` *  On the other hand, when using the identity operator (===), object variables are identical` |
|      - |  896 | ` *  if and only if they refer to the same instance of the same class.` |
|      - |  897 | ` *  An example will clarify these rules.` |
|      - |  898 | ` *  Example #1 Example of object comparison` |
|      - |  899 | ` *  <?php` |
|      - |  900 | ` *    function bool2str($bool)` |
|      - |  901 | ` * {` |
|      - |  902 | ` *   if ($bool === false) {` |
|      - |  903 | ` *       return 'FALSE';` |
|      - |  904 | ` *   } else {` |
|      - |  905 | ` *       return 'TRUE';` |
|      - |  906 | ` *   }` |
|      - |  907 | ` * }` |
|      - |  908 | ` * function compareObjects(&$o1, &$o2)` |
|      - |  909 | ` * {` |
|      - |  910 | ` *   echo 'o1 == o2 : ' . bool2str($o1 == $o2) . "\n";` |
|      - |  911 | ` *   echo 'o1 != o2 : ' . bool2str($o1 != $o2) . "\n";` |
|      - |  912 | ` *   echo 'o1 === o2 : ' . bool2str($o1 === $o2) . "\n";` |
|      - |  913 | ` *   echo 'o1 !== o2 : ' . bool2str($o1 !== $o2) . "\n";` |
|      - |  914 | ` * }` |
|      - |  915 | ` * class Flag` |
|      - |  916 | ` * {` |
|      - |  917 | ` *   public $flag;` |
|      - |  918 | ` *` |
|      - |  919 | ` *   function Flag($flag = true) {` |
|      - |  920 | ` *       $this->flag = $flag;` |
|      - |  921 | ` *   }` |
|      - |  922 | ` * }` |
|      - |  923 | ` *` |
|      - |  924 | ` * class OtherFlag` |
|      - |  925 | ` * {` |
|      - |  926 | ` *   public $flag;` |
|      - |  927 | ` *` |
|      - |  928 | ` *   function OtherFlag($flag = true) {` |
|      - |  929 | ` *       $this->flag = $flag;` |
|      - |  930 | ` *   }` |
|      - |  931 | ` * }` |
|      - |  932 | ` *` |
|      - |  933 | ` * $o = new Flag();` |
|      - |  934 | ` * $p = new Flag();` |
|      - |  935 | ` * $q = $o;` |
|      - |  936 | ` * $r = new OtherFlag();` |
|      - |  937 | ` *` |
|      - |  938 | ` * echo "Two instances of the same class\n";` |
|      - |  939 | ` * compareObjects($o, $p);` |
|      - |  940 | ` * echo "\nTwo references to the same instance\n";` |
|      - |  941 | ` * compareObjects($o, $q);` |
|      - |  942 | ` * echo "\nInstances of two different classes\n";` |
|      - |  943 | ` * compareObjects($o, $r);` |
|      - |  944 | ` * ?>` |
|      - |  945 | ` * The above example will output:` |
|      - |  946 | ` * Two instances of the same class` |
|      - |  947 | ` * o1 == o2 : TRUE` |
|      - |  948 | ` * o1 != o2 : FALSE` |
|      - |  949 | ` * o1 === o2 : FALSE` |
|      - |  950 | ` * o1 !== o2 : TRUE` |
|      - |  951 | ` * Two references to the same instance` |
|      - |  952 | ` * o1 == o2 : TRUE` |
|      - |  953 | ` * o1 != o2 : FALSE` |
|      - |  954 | ` * o1 === o2 : TRUE` |
|      - |  955 | ` * o1 !== o2 : FALSE` |
|      - |  956 | ` * Instances of two different classes` |
|      - |  957 | ` * o1 == o2 : FALSE` |
|      - |  958 | ` * o1 != o2 : TRUE` |
|      - |  959 | ` * o1 === o2 : FALSE` |
|      - |  960 | ` * o1 !== o2 : TRUE` |
|      - |  961 | ` *` |
|      - |  962 | ` * This function return 0 if the objects are equals according to the comprison rules defined above.` |
|      - |  963 | ` * Any other return values indicates difference.` |
|      - |  964 | ` */` |
|    174 |  965 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest)` |
|      3 |  966 |  |
|      - |  967 | `	SyHashEntry *pEntry,*pEntry2;` |
|      - |  968 | `	ph7_value sV1,sV2;` |
|      - |  969 | `	sxi32 rc;` |
|    177 |  970 | `	if( iNest > 31 ){` |
|      - |  971 | `		/* Nesting limit reached */` |
|      6 |  972 | `		PH7_VmThrowError(pLeft->pVm,0,PH7_CTX_ERR,"Nesting limit reached: Infinite recursion?");` |
|      6 |  973 | `		return 1;` |
|      - |  974 | `	}` |
|      - |  975 | `	/* Comparison is performed only if the objects are instance of the same class */` |
|    173 |  976 | `	if( pLeft->pClass != pRight->pClass ){` |
|      7 |  977 | `		return 1;` |
|      - |  978 | `	}` |
|    167 |  979 | `	if( bStrict ){` |
|      - |  980 | `		/*` |
|      - |  981 | `		 * According to the PHP language reference manual:` |
|      - |  982 | `		 *  when using the identity operator (===), object variables` |
|      - |  983 | `		 *  are identical if and only if they refer to the same instance` |
|      - |  984 | `		 *  of the same class.` |
|      - |  985 | `		 */` |
|     25 |  986 | `		return !(pLeft == pRight);` |
|      - |  987 | `	}` |
|      - |  988 | `	/*` |
|      - |  989 | `	 * Attribute comparison.` |
|      - |  990 | `	 * According to the PHP reference manual:` |
|      - |  991 | `	 *  When using the comparison operator (==), object variables are compared` |
|      - |  992 | `	 *  in a simple manner, namely: Two object instances are equal if they have` |
|      - |  993 | `	 *  the same attributes and values, and are instances of the same class.` |
|      - |  994 | `	 */` |
|    143 |  995 | `	if( pLeft == pRight ){` |
|      - |  996 | `		/* Same instance,don't bother processing,object are equals */` |
|      3 |  997 | `		return 0;` |
|      - |  998 | `	}` |
|    141 |  999 | `	SyHashResetLoopCursor(&pLeft->hAttr);` |
|    141 | 1000 | `	SyHashResetLoopCursor(&pRight->hAttr);` |
|    141 | 1001 | `	PH7_MemObjInit(pLeft->pVm,&sV1);` |
|    141 | 1002 | `	PH7_MemObjInit(pLeft->pVm,&sV2);` |
|    141 | 1003 | `	sV1.nIdx = sV2.nIdx = SXU32_HIGH;` |
|    224 | 1004 | `	while((pEntry = SyHashGetNextEntry(&pLeft->hAttr)) != 0 && (pEntry2 = SyHashGetNextEntry(&pRight->hAttr)) != 0 ){` |
|    147 | 1005 | `		VmClassAttr *p1 = (VmClassAttr *)pEntry->pUserData;` |
|    147 | 1006 | `		VmClassAttr *p2 = (VmClassAttr *)pEntry2->pUserData;` |
|      - | 1007 | `		/* Compare only non-static attribute */` |
|    147 | 1008 | `		if( (p1->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|      - | 1009 | `			ph7_value *pL,*pR;` |
|    147 | 1010 | `			pL = ExtractClassAttrValue(pLeft->pVm,p1);` |
|    147 | 1011 | `			pR = ExtractClassAttrValue(pRight->pVm,p2);` |
|    147 | 1012 | `			if( pL && pR ){` |
|    147 | 1013 | `				PH7_MemObjLoad(pL,&sV1);` |
|    147 | 1014 | `				PH7_MemObjLoad(pR,&sV2);` |
|      - | 1015 | `				/* Compare the two values now */` |
|    147 | 1016 | `				rc = PH7_MemObjCmp(&sV1,&sV2,bStrict,iNest+1);` |
|    147 | 1017 | `				PH7_MemObjRelease(&sV1);` |
|    147 | 1018 | `				PH7_MemObjRelease(&sV2);` |
|    147 | 1019 | `				if( rc != 0 ){` |
|      - | 1020 | `					/* Not equals */` |
|    133 | 1021 | `					return rc;` |
|      - | 1022 | `				}` |
|      7 | 1023 | `			}` |
|      7 | 1024 | `		}` |
|      1 | 1025 | `	}` |
|      - | 1026 | `	/* Object are equals */` |
|      9 | 1027 | `	return 0;` |
|     90 | 1028 |  |
|      - | 1029 | `/*` |
|      - | 1030 | ` * Dump a class instance and the store the dump in the BLOB given` |
|      - | 1031 | ` * as the first argument.` |
|      - | 1032 | ` * Note that only non-static/non-constants attribute are dumped.` |
|      - | 1033 | ` * This function is typically invoked when the user issue a call` |
|      - | 1034 | ` * to [var_dump(),var_export(),print_r(),...].` |
|      - | 1035 | ` * This function SXRET_OK on success. Any other return value including` |
|      - | 1036 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|      - | 1037 | ` */` |
|      - | 1038 | `/*` |
|      - | 1039 | ` * Emit a class-instance dump header plus its trailing newline. For var_dump` |
|      - | 1040 | ` * (ShowType) it completes the "object(" prefix the caller already emitted as` |
|      - | 1041 | ` *   ClassName)#<id> (<count>) {` |
|      - | 1042 | ` * for print_r it emits the legacy PHL  Object(ClassName) {  (count/id unused).` |
|      - | 1043 | ` */` |
|    134 | 1044 | `static void DumpClassInstanceHeader(SyBlob *pOut,ph7_class *pClass,sxu32 nObjId,int ShowType,sxu32 nCount)` |
|      3 | 1045 |  |
|    137 | 1046 | `	if( !ShowType ){` |
|      6 | 1047 | `		SyBlobAppend(&(*pOut),"Object(",sizeof("Object(")-1);` |
|      6 | 1048 | `		SyBlobFormat(&(*pOut),"%z) {",&pClass->sName);` |
|      4 | 1049 | `	}else{` |
|    133 | 1050 | `		SyBlobFormat(&(*pOut),"%z)#%u (%u) {",&pClass->sName,nObjId,nCount);` |
|      - | 1051 | `	}` |
|      - | 1052 | `#ifdef __WINNT__` |
|      3 | 1053 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|      - | 1054 | `#else` |
|    134 | 1055 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      - | 1056 | `#endif` |
|    137 | 1057 |  |
|    138 | 1058 | `PH7_PRIVATE sxi32 PH7_ClassInstanceDump(SyBlob *pOut,ph7_class_instance *pThis,int ShowType,int nTab,int nDepth)` |
|      3 | 1059 |  |
|      - | 1060 | `	SyHashEntry *pEntry;` |
|      - | 1061 | `	ph7_value *pValue;` |
|      - | 1062 | `	sxi32 rc;` |
|      - | 1063 | `	int i;` |
|    141 | 1064 | `	if( nDepth > 31 ){` |
|      - | 1065 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|      - | 1066 | `		/* Nesting limit reached..halt immediately*/` |
|      5 | 1067 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|      5 | 1068 | `		if( ShowType ){` |
|      5 | 1069 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|      2 | 1070 | `		}` |
|      5 | 1071 | `		return SXERR_LIMIT;` |
|      - | 1072 | `	}` |
|    137 | 1073 | `	rc = SXRET_OK;` |
|      - | 1074 | `	{` |
|      - | 1075 | `		/* Both var_dump and print_r consult __debugInfo() (PHP behavior);` |
|      - | 1076 | `		 * var_export uses a separate renderer and never reaches here. When the` |
|      - | 1077 | `		 * method is present and returns an array, render that array's entries as` |
|      - | 1078 | `		 * the object body, with the header showing the debug array's count. The` |
|      - | 1079 | `		 * nDepth guard above protects against a __debugInfo returning the object` |
|      - | 1080 | `		 * itself. */` |
|    137 | 1081 | `		ph7_class_method *pDbg = PH7_ClassExtractMethod(pThis->pClass,"__debugInfo",sizeof("__debugInfo")-1);` |
|    137 | 1082 | `		if( pDbg ){` |
|      - | 1083 | `			ph7_value sResult;` |
|      5 | 1084 | `			PH7_MemObjInit(pThis->pVm,&sResult);` |
|      5 | 1085 | `			PH7_VmCallClassMethod(pThis->pVm,pThis,pDbg,&sResult,0,0);` |
|      5 | 1086 | `			if( sResult.iFlags & MEMOBJ_HASHMAP ){` |
|      5 | 1087 | `				ph7_hashmap *pMap = (ph7_hashmap *)sResult.x.pOther;` |
|      - | 1088 | `				/* Header count is the debug array's entry count. */` |
|      5 | 1089 | `				DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,pMap->nEntry);` |
|      5 | 1090 | `				rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth);` |
|      9 | 1091 | `				for( i = 0 ; i < nTab ; i++ ){` |
|      5 | 1092 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      3 | 1093 | `				}` |
|      5 | 1094 | `				SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      5 | 1095 | `				PH7_MemObjRelease(&sResult);` |
|      5 | 1096 | `				return rc;` |
|      - | 1097 | `			}` |
|      - | 1098 | `			/* Non-array return: behave as if __debugInfo were absent. */` |
|    ! 0 | 1099 | `			PH7_MemObjRelease(&sResult);` |
|    ! 0 | 1100 | `		}` |
|      - | 1101 | `	}` |
|      - | 1102 | `	{` |
|      - | 1103 | `		/* var_dump's header needs the property count up front, so pre-count the` |
|      - | 1104 | `		 * non-static/non-constant attributes (matching the dump loop below). */` |
|    132 | 1105 | `		sxu32 nProp = 0;` |
|    132 | 1106 | `		if( ShowType ){` |
|    130 | 1107 | `			SyHashResetLoopCursor(&pThis->hAttr);` |
|    266 | 1108 | `			while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|    138 | 1109 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    138 | 1110 | `				if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|    134 | 1111 | `					nProp++;` |
|     66 | 1112 | `				}` |
|      2 | 1113 | `			}` |
|     64 | 1114 | `		}` |
|    132 | 1115 | `		DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,nProp);` |
|      - | 1116 | `	}` |
|      - | 1117 | `	/* Dump object attributes */` |
|    132 | 1118 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|    213 | 1119 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|    142 | 1120 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    142 | 1121 | `		if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|      - | 1122 | `			/* Dump non-static/constant attribute only */` |
|   3994 | 1123 | `			for( i = 0 ; i < nTab ; i++ ){` |
|   3858 | 1124 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|   1930 | 1125 | `			}` |
|    138 | 1126 | `			pValue = ExtractClassAttrValue(pThis->pVm,pVmAttr);` |
|    138 | 1127 | `			if( pValue ){` |
|    138 | 1128 | `				SyBlobFormat(&(*pOut),"['%z'] =>",&pVmAttr->pAttr->sName);` |
|      - | 1129 | `#ifdef __WINNT__` |
|      2 | 1130 | `				SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|      - | 1131 | `#else` |
|    136 | 1132 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      - | 1133 | `#endif` |
|    138 | 1134 | `				rc = PH7_MemObjDump(&(*pOut),pValue,ShowType,nTab+1,nDepth,0);` |
|    138 | 1135 | `				if( rc == SXERR_LIMIT ){` |
|    125 | 1136 | `					break;` |
|      - | 1137 | `				}` |
|      6 | 1138 | `			}` |
|      6 | 1139 | `		}` |
|      2 | 1140 | `	}` |
|   3982 | 1141 | `	for( i = 0 ; i < nTab ; i++ ){` |
|   3852 | 1142 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|   1927 | 1143 | `	}` |
|    132 | 1144 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|    132 | 1145 | `	return rc;` |
|     72 | 1146 |  |
|      - | 1147 | `/*` |
|      - | 1148 | ` * Call a magic method [i.e: __toString(),__toBool(),__Invoke()...]` |
|      - | 1149 | ` * Return SXRET_OK on successfull call. Any other return value indicates failure.` |
|      - | 1150 | ` * Notes on magic methods.` |
|      - | 1151 | ` * According to the PHP language reference manual.` |
|      - | 1152 | ` *  The function names __construct(), __destruct(), __call(), __callStatic()` |
|      - | 1153 | ` *  __get(),  __toString(), __invoke(), __clone() are magical in PHP classes.` |
|      - | 1154 | ` * You cannot have functions with these names in any of your classes unless` |
|      - | 1155 | ` * you want the magic functionality associated with them.` |
|      - | 1156 | ` * Example of magical methods:` |
|      - | 1157 | ` * __toString()` |
|      - | 1158 | ` *  The __toString() method allows a class to decide how it will react when it is treated like` |
|      - | 1159 | ` *  a string. For example, what echo $obj; will print. This method must return a string.` |
|      - | 1160 | ` *  Example #2 Simple example` |
|      - | 1161 | ` * <?php` |
|      - | 1162 | ` * // Declare a simple class` |
|      - | 1163 | ` * class TestClass` |
|      - | 1164 | ` * {` |
|      - | 1165 | ` *   public $foo;` |
|      - | 1166 | ` *` |
|      - | 1167 | ` *   public function __construct($foo)` |
|      - | 1168 | ` *   {` |
|      - | 1169 | ` *       $this->foo = $foo;` |
|      - | 1170 | ` *   }` |
|      - | 1171 | ` *` |
|      - | 1172 | ` *   public function __toString()` |
|      - | 1173 | ` *   {` |
|      - | 1174 | ` *       return $this->foo;` |
|      - | 1175 | ` *   }` |
|      - | 1176 | ` * }` |
|      - | 1177 | ` * $class = new TestClass('Hello');` |
|      - | 1178 | ` * echo $class;` |
|      - | 1179 | ` * ?>` |
|      - | 1180 | ` * The above example will output:` |
|      - | 1181 | ` *  Hello` |
|      - | 1182 | ` *` |
|      - | 1183 | ` * Note that PH7 does not support all the magical method and introudces __toFloat(),__toInt()` |
|      - | 1184 | ` * which have the same behaviour as __toString() but for float and integer types` |
|      - | 1185 | ` * respectively.` |
|      - | 1186 | ` * Refer to the official documentation for more information.` |
|      - | 1187 | ` */` |
|      2 | 1188 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallMagicMethod(` |
|      - | 1189 | `	ph7_vm *pVm,               /* VM that own all this stuff */` |
|      - | 1190 | `	ph7_class *pClass,         /* Target class */` |
|      - | 1191 | `	ph7_class_instance *pThis, /* Target object */` |
|      - | 1192 | `	const char *zMethod,       /* Magic method name [i.e: __toString()]*/` |
|      - | 1193 | `	sxu32 nByte,               /* zMethod length*/` |
|      - | 1194 | `	const SyString *pAttrName  /* Attribute name */` |
|      - | 1195 | `	)` |
|      1 | 1196 |  |
|      3 | 1197 | `	ph7_value *apArg[2] = { 0 , 0 };` |
|      - | 1198 | `	ph7_class_method *pMeth;` |
|      - | 1199 | `	ph7_value sAttr; /* cc warning */` |
|      - | 1200 | `	sxi32 rc;` |
|      - | 1201 | `	int nArg;` |
|      - | 1202 | `	/* Make sure the magic method is available */` |
|      3 | 1203 | `	pMeth = PH7_ClassExtractMethod(&(*pClass),zMethod,nByte);` |
|      3 | 1204 | `	if( pMeth == 0 ){` |
|      - | 1205 | `		/* No such method,return immediately */` |
|      3 | 1206 | `		return SXERR_NOTFOUND;` |
|      - | 1207 | `	}` |
|    ! 0 | 1208 | `	nArg = 0;` |
|      - | 1209 | `	/* Copy arguments */` |
|    ! 0 | 1210 | `	if( pAttrName ){` |
|    ! 0 | 1211 | `		PH7_MemObjInitFromString(pVm,&sAttr,pAttrName);` |
|    ! 0 | 1212 | `		sAttr.nIdx = SXU32_HIGH; /* Mark as constant */` |
|    ! 0 | 1213 | `		apArg[0] = &sAttr;` |
|    ! 0 | 1214 | `		nArg = 1;` |
|    ! 0 | 1215 | `	}` |
|      - | 1216 | `	/* Call the magic method now */` |
|    ! 0 | 1217 | `	rc = PH7_VmCallClassMethod(pVm,&(*pThis),pMeth,0,nArg,apArg);` |
|      - | 1218 | `	/* Clean up */` |
|    ! 0 | 1219 | `	if( pAttrName ){` |
|    ! 0 | 1220 | `		PH7_MemObjRelease(&sAttr);` |
|    ! 0 | 1221 | `	}` |
|    ! 0 | 1222 | `	return rc;` |
|      2 | 1223 |  |
|      - | 1224 | `/*` |
|      - | 1225 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon].` |
|      - | 1226 | ` * This function is simply a wrapper on ExtractClassAttrValue().` |
|      - | 1227 | ` */` |
|     74 | 1228 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceExtractAttrValue(ph7_class_instance *pThis,VmClassAttr *pAttr)` |
|      3 | 1229 |  |
|      - | 1230 | `   /* Extract the attribute value */` |
|      - | 1231 | `	ph7_value *pValue;` |
|     77 | 1232 | `	pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     77 | 1233 | `	return pValue;` |
|      3 | 1234 |  |
|      - | 1235 | `/*` |
|      - | 1236 | ` * Convert a class instance [i.e: Object in the PHP jargon] into a hashmap [i.e: array in the PHP jargon].` |
|      - | 1237 | ` * Return SXRET_OK on success. Any other value indicates failure.` |
|      - | 1238 | ` * Note on object conversion to array:` |
|      - | 1239 | ` *  Acccording to the PHP language reference manual` |
|      - | 1240 | ` *  If an object is converted to an array, the result is an array whose elements are the object's properties.` |
|      - | 1241 | ` *  The keys are the member variable names.` |
|      - | 1242 | ` *` |
|      - | 1243 | ` *  The following example:` |
|      - | 1244 | ` *  class Test {` |
|      - | 1245 | ` *   public $A = 25<<1;  // 50` |
|      - | 1246 | ` *	 public $c = rand_str(3);   // Random string of length 3` |
|      - | 1247 | ` *	 public $d = rand() & 1023; // Random number between 0..1023` |
|      - | 1248 | ` *  }` |
|      - | 1249 | ` *  var_dump((array) new Test());` |
|      - | 1250 | ` *	Will output:` |
|      - | 1251 | ` *  array(3) {` |
|      - | 1252 | ` *   [A] =>` |
|      - | 1253 | ` *      int(50)` |
|      - | 1254 | ` *   [c] =>` |
|      - | 1255 | ` *     string(3 'aps')` |
|      - | 1256 | ` *   [d] =>` |
|      - | 1257 | ` *     int(991)` |
|      - | 1258 | ` *  }` |
|      - | 1259 | ` * You have noticed that PH7 allow class attributes [i.e: $a,$c,$d in the example above]` |
|      - | 1260 | ` * have any complex expression (even function calls/Annonymous functions) as their default` |
|      - | 1261 | ` * value unlike the standard PHP engine.` |
|      - | 1262 | ` * This is a very powerful feature that you have to look at.` |
|      - | 1263 | ` */` |
|      6 | 1264 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|      1 | 1265 |  |
|      - | 1266 | `	SyHashEntry *pEntry;` |
|      - | 1267 | `	SyString *pAttrName;` |
|      - | 1268 | `	VmClassAttr *pAttr;` |
|      - | 1269 | `	ph7_value *pValue;` |
|      - | 1270 | `	ph7_value sName;` |
|      - | 1271 | `	/* Reset the loop cursor */` |
|      7 | 1272 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      7 | 1273 | `	PH7_MemObjInitFromString(pThis->pVm,&sName,0);` |
|     20 | 1274 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|      - | 1275 | `		/* Point to the current attribute */` |
|     11 | 1276 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1277 | `		/* Extract attribute value */` |
|     11 | 1278 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     11 | 1279 | `		if( pValue ){` |
|      - | 1280 | `			/* Build attribute name */` |
|     11 | 1281 | `			pAttrName = &pAttr->pAttr->sName;` |
|     11 | 1282 | `			PH7_MemObjStringAppend(&sName,pAttrName->zString,pAttrName->nByte);` |
|      - | 1283 | `			/* Perform the insertion */` |
|     11 | 1284 | `			PH7_HashmapInsert(pMap,&sName,pValue);` |
|      - | 1285 | `			/* Reset the string cursor */` |
|     11 | 1286 | `			SyBlobReset(&sName.sBlob);` |
|      5 | 1287 | `		}` |
|      1 | 1288 | `	}` |
|      7 | 1289 | `	PH7_MemObjRelease(&sName);` |
|      7 | 1290 | `	return SXRET_OK;` |
|      1 | 1291 |  |
|      - | 1292 | `/*` |
|      - | 1293 | ` * Iterate throw class attributes and invoke the given callback [i.e: xWalk()] for each` |
|      - | 1294 | ` * retrieved attribute.` |
|      - | 1295 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|      - | 1296 | ` * the attribute value in the callback body will not alter the real attribute value.` |
|      - | 1297 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|      - | 1298 | ` * a value different from PH7_OK.` |
|      - | 1299 | ` * Refer to [ph7_object_walk()] for more information.` |
|      - | 1300 | ` */` |
|      2 | 1301 | `PH7_PRIVATE sxi32 PH7_ClassInstanceWalk(` |
|      - | 1302 | `	ph7_class_instance *pThis, /* Target object */` |
|      - | 1303 | `	int (*xWalk)(const char *,ph7_value *,void *), /* Walker callback */` |
|      - | 1304 | `	void *pUserData /* Last argument to xWalk() */` |
|      - | 1305 | `	)` |
|      1 | 1306 |  |
|      - | 1307 | `	SyHashEntry *pEntry; /* Hash entry */` |
|      - | 1308 | `	VmClassAttr *pAttr;  /* Pointer to the attribute */` |
|      - | 1309 | `	ph7_value *pValue;   /* Attribute value */` |
|      - | 1310 | `	ph7_value sValue;    /* Copy of the attribute value */` |
|      - | 1311 | `	int rc;` |
|      - | 1312 | `	/* Reset the loop cursor */` |
|      3 | 1313 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      3 | 1314 | `	PH7_MemObjInit(pThis->pVm,&sValue);` |
|      - | 1315 | `	/* Start the walk process */` |
|      8 | 1316 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|      - | 1317 | `		/* Point to the current attribute */` |
|      5 | 1318 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1319 | `		/* Extract attribute value */` |
|      5 | 1320 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|      5 | 1321 | `		if( pValue ){` |
|      5 | 1322 | `			PH7_MemObjLoad(pValue,&sValue);` |
|      - | 1323 | `			/* Invoke the supplied callback */` |
|      5 | 1324 | `			rc =  xWalk(SyStringData(&pAttr->pAttr->sName),&sValue,pUserData);` |
|      5 | 1325 | `			PH7_MemObjRelease(&sValue);` |
|      5 | 1326 | `			if( rc != PH7_OK){` |
|      - | 1327 | `				/* User callback request an operation abort */` |
|    ! 0 | 1328 | `				return SXERR_ABORT;` |
|      - | 1329 | `			}` |
|      2 | 1330 | `		}` |
|      1 | 1331 | `	}` |
|      - | 1332 | `	/* All done */` |
|      3 | 1333 | `	return SXRET_OK;` |
|      2 | 1334 |  |
|      - | 1335 | `/*` |
|      - | 1336 | ` * Extract a class atrribute value.` |
|      - | 1337 | ` * Return a pointer to the attribute value on success. Otherwise NULL.` |
|      - | 1338 | ` * Note:` |
|      - | 1339 | ` *  Access to static and constant attribute is not allowed. That is,the function` |
|      - | 1340 | ` *  will return NULL in case someone (host-application code) try to extract` |
|      - | 1341 | ` *  a static/constant attribute.` |
|      - | 1342 | ` */` |
|   1248 | 1343 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceFetchAttr(ph7_class_instance *pThis,const SyString *pName)` |
|      5 | 1344 |  |
|      - | 1345 | `	SyHashEntry *pEntry;` |
|      - | 1346 | `	VmClassAttr *pAttr;` |
|      - | 1347 | `	/* Query the attribute hashtable */` |
|   1253 | 1348 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);` |
|   1253 | 1349 | `	if( pEntry == 0 ){` |
|      - | 1350 | `		/* No such attribute */` |
|    ! 0 | 1351 | `		return 0;` |
|      - | 1352 | `	}` |
|      - | 1353 | `	/* Point to the class atrribute */` |
|   1253 | 1354 | `	pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1355 | `	/* Check if we are dealing with a static/constant attribute */` |
|   1253 | 1356 | `	if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|      - | 1357 | `		/* Access is forbidden */` |
|    ! 0 | 1358 | `		return 0;` |
|      - | 1359 | `	}` |
|      - | 1360 | `	/* Return the attribute value */` |
|   1253 | 1361 | `	return ExtractClassAttrValue(pThis->pVm,pAttr);` |
|    629 | 1362 |  |
|      - | 1363 |  |
