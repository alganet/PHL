# src/ph7/oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 468/520 lines (90.00%)

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
| 126786 |   14 | `PH7_PRIVATE ph7_class * PH7_NewRawClass(ph7_vm *pVm,const SyString *pName,sxu32 nLine)` |
|      5 |   15 |  |
|      - |   16 | `	ph7_class *pClass;` |
|      - |   17 | `	char *zName;` |
|      - |   18 | `	/* Allocate a new instance */` |
| 126791 |   19 | `	pClass = (ph7_class *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class));` |
| 126791 |   20 | `	if( pClass == 0 ){` |
|    ! 0 |   21 | `		return 0;` |
|      - |   22 | `	}` |
|      - |   23 | `	/* Zero the structure */` |
| 126791 |   24 | `	SyZero(pClass,sizeof(ph7_class));` |
|      - |   25 | `	/* Duplicate class name */` |
| 126791 |   26 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
| 126791 |   27 | `	if( zName == 0 ){` |
|    ! 0 |   28 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClass);` |
|    ! 0 |   29 | `		return 0;` |
|      - |   30 | `	}` |
|      - |   31 | `	/* Initialize fields */` |
| 126791 |   32 | `	SyStringInitFromBuf(&pClass->sName,zName,pName->nByte);` |
| 126791 |   33 | `	SyHashInit(&pClass->hMethod,&pVm->sAllocator,0,0);` |
| 126791 |   34 | `	SyHashInit(&pClass->hAttr,&pVm->sAllocator,0,0);` |
| 126791 |   35 | `	SyHashInit(&pClass->hDerived,&pVm->sAllocator,0,0);` |
| 126791 |   36 | `	SySetInit(&pClass->aInterface,&pVm->sAllocator,sizeof(ph7_class *));` |
| 126791 |   37 | `	SySetInit(&pClass->aTrait,&pVm->sAllocator,sizeof(ph7_class *));` |
| 126791 |   38 | `	pClass->nLine = nLine;` |
|      - |   39 | `	/* All done */` |
| 126791 |   40 | `	return pClass;` |
|  63398 |   41 |  |
|      - |   42 | `/*` |
|      - |   43 | ` * Allocate and initialize a new class attribute.` |
|      - |   44 | ` * Return a pointer to the class attribute on success. NULL otherwise.` |
|      - |   45 | ` */` |
|  63506 |   46 | `PH7_PRIVATE ph7_class_attr * PH7_NewClassAttr(ph7_vm *pVm,const SyString *pName,sxu32 nLine,sxi32 iProtection,sxi32 iFlags)` |
|      5 |   47 |  |
|      - |   48 | `	ph7_class_attr *pAttr;` |
|      - |   49 | `	char *zName;` |
|  63511 |   50 | `	pAttr = (ph7_class_attr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_attr));` |
|  63511 |   51 | `	if( pAttr == 0 ){` |
|    ! 0 |   52 | `		return 0;` |
|      - |   53 | `	}` |
|      - |   54 | `	/* Zero the structure */` |
|  63511 |   55 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|      - |   56 | `	/* Duplicate attribute name */` |
|  63511 |   57 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  63511 |   58 | `	if( zName == 0 ){` |
|    ! 0 |   59 | `		SyMemBackendPoolFree(&pVm->sAllocator,pAttr);` |
|    ! 0 |   60 | `		return 0;` |
|      - |   61 | `	}` |
|      - |   62 | `	/* Initialize fields */` |
|  63511 |   63 | `	SySetInit(&pAttr->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|  63511 |   64 | `	SySetInit(&pAttr->aUnionAlts,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|  63511 |   65 | `	SyStringInitFromBuf(&pAttr->sName,zName,pName->nByte);` |
|  63511 |   66 | `	pAttr->iProtection = iProtection;` |
|  63511 |   67 | `	pAttr->nIdx = SXU32_HIGH;` |
|  63511 |   68 | `	pAttr->iFlags = iFlags;` |
|  63511 |   69 | `	pAttr->nLine = nLine;` |
|  63511 |   70 | `	return pAttr;` |
|  31758 |   71 |  |
|      - |   72 | `/*` |
|      - |   73 | ` * Allocate and initialize a new class method.` |
|      - |   74 | ` * Return a pointer to the class method on success. NULL otherwise` |
|      - |   75 | ` * This function associate with the newly created method an automatically generated` |
|      - |   76 | ` * random unique name.` |
|      - |   77 | ` */` |
| 248880 |   78 | `PH7_PRIVATE ph7_class_method * PH7_NewClassMethod(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,sxu32 nLine,` |
|      - |   79 | `	sxi32 iProtection,sxi32 iFlags,sxi32 iFuncFlags)` |
|      5 |   80 |  |
|      - |   81 | `	ph7_class_method *pMeth;` |
|      - |   82 | `	SyHashEntry *pEntry;` |
|      - |   83 | `	SyString *pNamePtr;` |
|      - |   84 | `	char zSalt[10];` |
|      - |   85 | `	char *zName;` |
|      - |   86 | `	sxu32 nByte;` |
|      - |   87 | `	/* Allocate a new class method instance */` |
| 248885 |   88 | `	pMeth = (ph7_class_method *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_method));` |
| 248885 |   89 | `	if( pMeth == 0 ){` |
|    ! 0 |   90 | `		return 0;` |
|      - |   91 | `	}` |
|      - |   92 | `	/* Zero the structure */` |
| 248885 |   93 | `	SyZero(pMeth,sizeof(ph7_class_method));` |
|      - |   94 | `	/* Check for an already installed method with the same name */` |
| 248885 |   95 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
| 248885 |   96 | `	if( pEntry == 0 ){` |
|      - |   97 | `		/* Associate an unique VM name to this method */` |
| 248883 |   98 | `		nByte = sizeof(zSalt) + pName->nByte + SyStringLength(&pClass->sName)+sizeof(char)*7/*[[__'\0'*/;` |
| 248883 |   99 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,nByte);` |
| 248883 |  100 | `		if( zName == 0 ){` |
|    ! 0 |  101 | `			SyMemBackendPoolFree(&pVm->sAllocator,pMeth);` |
|    ! 0 |  102 | `			return 0;` |
|      - |  103 | `		}` |
| 248883 |  104 | `		pNamePtr = &pMeth->sVmName;` |
|      - |  105 | `		/* Generate a random string */` |
| 248883 |  106 | `		PH7_VmRandomString(&(*pVm),zSalt,sizeof(zSalt));` |
| 248883 |  107 | `		pNamePtr->nByte = SyBufferFormat(zName,nByte,"[__%z@%z_%.*s]",&pClass->sName,pName,sizeof(zSalt),zSalt);` |
| 248883 |  108 | `		pNamePtr->zString = zName;` |
| 124444 |  109 | `	}else{` |
|      - |  110 | `		/* Method is condidate for 'overloading' */` |
|      3 |  111 | `		ph7_class_method *pCurrent = (ph7_class_method *)pEntry->pUserData;` |
|      3 |  112 | `		pNamePtr = &pMeth->sVmName;` |
|      - |  113 | `		/* Use the same VM name */` |
|      3 |  114 | `		SyStringDupPtr(pNamePtr,&pCurrent->sVmName);` |
|      3 |  115 | `		zName = (char *)pNamePtr->zString;` |
|      - |  116 | `	}` |
| 248885 |  117 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|     32 |  118 | `		if( (pName->nByte == sizeof("__construct") - 1 && SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1 ) == 0)` |
|     21 |  119 | `			\|\| (pName->nByte == sizeof("__destruct") - 1 && SyMemcmp(pName->zString,"__destruct",sizeof("__destruct") - 1 ) == 0)` |
|     22 |  120 | `			\|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|      - |  121 | `				/* Switch to public visibility when dealing with constructor/destructor */` |
|      5 |  122 | `				iProtection = PH7_CLASS_PROT_PUBLIC;` |
|      2 |  123 | `		}` |
|     12 |  124 | `	}` |
|      - |  125 | `	/* Initialize method fields */` |
| 248887 |  126 | `	pMeth->iProtection = iProtection;` |
| 248887 |  127 | `	pMeth->iFlags = iFlags;` |
| 248887 |  128 | `	pMeth->nLine = nLine;` |
| 373329 |  129 | `	PH7_VmInitFuncState(&(*pVm),&pMeth->sFunc,&zName[sizeof(char)*4/*[__@*/+SyStringLength(&pClass->sName)],` |
| 248882 |  130 | `		pName->nByte,iFuncFlags\|VM_FUNC_CLASS_METHOD,pClass);` |
| 248887 |  131 | `	return pMeth;` |
| 124447 |  132 |  |
|      - |  133 | `/*` |
|      - |  134 | ` * Check if the given name have a class method associated with it.` |
|      - |  135 | ` * Return the desired method [i.e: ph7_class_method instance] on success. NULL otherwise.` |
|      - |  136 | ` */` |
| 160792 |  137 | `PH7_PRIVATE ph7_class_method * PH7_ClassExtractMethod(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|      5 |  138 |  |
|      - |  139 | `	SyHashEntry *pEntry;` |
|      - |  140 | `	/* Perform a hash lookup */` |
| 160797 |  141 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)zName,nByte);` |
| 160797 |  142 | `	if( pEntry == 0 ){` |
|      - |  143 | `		/* No such entry */` |
|   3721 |  144 | `		return 0;` |
|      - |  145 | `	}` |
|      - |  146 | `	/* Point to the desired method */` |
| 157081 |  147 | `	return (ph7_class_method *)pEntry->pUserData;` |
|  80401 |  148 |  |
|      - |  149 | `/*` |
|      - |  150 | ` * Check if the given name is a class attribute.` |
|      - |  151 | ` * Return the desired attribute [i.e: ph7_class_attr instance] on success.NULL otherwise.` |
|      - |  152 | ` */` |
|  63578 |  153 | `PH7_PRIVATE ph7_class_attr * PH7_ClassExtractAttribute(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|      5 |  154 |  |
|      - |  155 | `	SyHashEntry *pEntry;` |
|      - |  156 | `	/* Perform a hash lookup */` |
|  63583 |  157 | `	pEntry = SyHashGet(&pClass->hAttr,(const void *)zName,nByte);` |
|  63583 |  158 | `	if( pEntry == 0 ){` |
|      - |  159 | `		/* No such entry */` |
|  63437 |  160 | `		return 0;` |
|      - |  161 | `	}` |
|      - |  162 | `	/* Point to the desierd method */` |
|    151 |  163 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|  31794 |  164 |  |
|      - |  165 | `/*` |
|      - |  166 | ` * Install a class attribute in the corresponding container.` |
|      - |  167 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|      - |  168 | ` */` |
|  63506 |  169 | `PH7_PRIVATE sxi32 PH7_ClassInstallAttr(ph7_class *pClass,ph7_class_attr *pAttr)` |
|      5 |  170 |  |
|  63511 |  171 | `	SyString *pName = &pAttr->sName;` |
|      - |  172 | `	sxi32 rc;` |
|      - |  173 | `	/* Remember where this attribute was originally declared so that later` |
|      - |  174 | `	 * inheritance/trait copies still know the declaring class (needed for` |
|      - |  175 | `	 * PHP-compatible error messages on typed properties). */` |
|  63511 |  176 | `	if( pAttr->pDeclClass == 0 ){` |
|  63511 |  177 | `		pAttr->pDeclClass = pClass;` |
|  31753 |  178 | `	}` |
|  63511 |  179 | `	rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|  63511 |  180 | `	return rc;` |
|      5 |  181 |  |
|      - |  182 | `/*` |
|      - |  183 | ` * Install a class method in the corresponding container.` |
|      - |  184 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|      - |  185 | ` */` |
| 248868 |  186 | `PH7_PRIVATE sxi32 PH7_ClassInstallMethod(ph7_class *pClass,ph7_class_method *pMeth)` |
|      5 |  187 |  |
| 248873 |  188 | `	SyString *pName = &pMeth->sFunc.sName;` |
|      - |  189 | `	sxi32 rc;` |
| 248873 |  190 | `	rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 248873 |  191 | `	return rc;` |
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
|  69576 |  234 | `PH7_PRIVATE sxi32 PH7_ClassInherit(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase)` |
|      5 |  235 |  |
|      - |  236 | `	ph7_class_method *pMeth;` |
|      - |  237 | `	ph7_class_attr *pAttr;` |
|      - |  238 | `	SyHashEntry *pEntry;` |
|      - |  239 | `	SyString *pName;` |
|      - |  240 | `	sxi32 rc;` |
|      - |  241 | `	/* Install in the derived hashtable */` |
|  69581 |  242 | `	rc = SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|  69581 |  243 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  244 | `		return rc;` |
|      - |  245 | `	}` |
|      - |  246 | `	/* readonly class inheritance (PHP 8.2): a readonly class may only extend a` |
|      - |  247 | `	 * readonly class, and a non-readonly class may not extend a readonly one. */` |
|  69581 |  248 | `	if( (pBase->iFlags & PH7_CLASS_READONLY) != (pSub->iFlags & PH7_CLASS_READONLY) ){` |
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
|  69581 |  263 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
| 486511 |  264 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|      - |  265 | `		/* Make sure the private attributes are not redeclared in the subclass */` |
| 416935 |  266 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
| 416935 |  267 | `		pName = &pAttr->sName;` |
| 416935 |  268 | `		if( (pEntry = SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|      9 |  269 | `			if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_FINAL))` |
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
| 416929 |  293 | `		if( pAttr->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
| 416925 |  294 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
| 416925 |  295 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  296 | `				return rc;` |
|      - |  297 | `			}` |
| 208460 |  298 | `		}` |
|      5 |  299 | `	}` |
|  69581 |  300 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
| 695041 |  301 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|      - |  302 | `		/* Make sure the private/final methods are not redeclared in the subclass */` |
| 625465 |  303 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
| 625465 |  304 | `		pName = &pMeth->sFunc.sName;` |
| 625465 |  305 | `		if( (pEntry = SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|   3343 |  306 | `			 if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|      - |  307 | `				/* Cannot Overwrite final method */` |
|      8 |  308 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_method *)pEntry->pUserData)->nLine,` |
|      - |  309 | `					"Cannot Overwrite final method '%z:%z' inside child class '%z'",` |
|      2 |  310 | `					&pBase->sName,pName,&pSub->sName);` |
|      6 |  311 | `				if( rc == SXERR_ABORT ){` |
|    ! 0 |  312 | `					return SXERR_ABORT;` |
|      - |  313 | `				}` |
|      2 |  314 | `			}` |
|   3343 |  315 | `			continue;` |
|      - |  316 | `		}` |
|      - |  317 | `		/* Install the method */` |
| 622127 |  318 | `		if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
| 622125 |  319 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 622125 |  320 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  321 | `				return rc;` |
|      - |  322 | `			}` |
| 311060 |  323 | `		}` |
|      5 |  324 | `	}` |
|      - |  325 | `	/* Mark as subclass */` |
|  69581 |  326 | `	pSub->pBase = pBase;` |
|      - |  327 | `	/* All done */` |
|  69581 |  328 | `	return SXRET_OK;` |
|  34793 |  329 |  |
|      - |  330 | `/*` |
|      - |  331 | ` * Apply a trait to a class: copy all methods and attributes from the trait` |
|      - |  332 | ` * into the target class. Unlike inheritance, traits copy ALL members including` |
|      - |  333 | ` * private ones. Members already defined in the class take precedence.` |
|      - |  334 | ` */` |
|     44 |  335 | `PH7_PRIVATE sxi32 PH7_ClassUseTrait(ph7_gen_state *pGen,ph7_class *pClass,ph7_class *pTrait)` |
|      5 |  336 |  |
|      - |  337 | `	ph7_class_method *pMeth;` |
|      - |  338 | `	ph7_class_attr *pAttr;` |
|      - |  339 | `	SyHashEntry *pEntry;` |
|      - |  340 | `	SyString *pName;` |
|      - |  341 | `	sxi32 rc;` |
|      - |  342 | `	/* Detect cyclic trait composition (e.g. trait A { use B; } trait B { use A; }) */` |
|     49 |  343 | `	if( pTrait->iFlags & PH7_CLASS_TRAIT_VISITING ){` |
|    ! 0 |  344 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|    ! 0 |  345 | `			"Trait circular reference detected: %z is already being applied",&pTrait->sName);` |
|    ! 0 |  346 | `		if( rc == SXERR_ABORT ){` |
|    ! 0 |  347 | `			return SXERR_ABORT;` |
|      - |  348 | `		}` |
|    ! 0 |  349 | `		return SXRET_OK;` |
|      - |  350 | `	}` |
|     49 |  351 | `	pTrait->iFlags \|= PH7_CLASS_TRAIT_VISITING;` |
|     49 |  352 | `	rc = SXRET_OK;` |
|      - |  353 | `	/* Copy attributes from the trait */` |
|     49 |  354 | `	SyHashResetLoopCursor(&pTrait->hAttr);` |
|     65 |  355 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hAttr)) != 0 ){` |
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
|     49 |  397 | `	SyHashResetLoopCursor(&pTrait->hMethod);` |
|     89 |  398 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hMethod)) != 0 ){` |
|     44 |  399 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|     44 |  400 | `		pName = &pMeth->sFunc.sName;` |
|     44 |  401 | `		if( SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte) != 0 ){` |
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
|     36 |  427 | `		rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|     36 |  428 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  429 | `			goto cleanup;` |
|      - |  430 | `		}` |
|      4 |  431 | `	}` |
|      - |  432 | `	/* Record trait in the class */` |
|     49 |  433 | `	SySetPut(&pClass->aTrait,(const void *)&pTrait);` |
|     22 |  434 | `cleanup:` |
|      - |  435 | `	/* Always clear visiting flag, even on error paths */` |
|     49 |  436 | `	pTrait->iFlags &= ~PH7_CLASS_TRAIT_VISITING;` |
|     22 |  437 | `	SXUNUSED(pGen);` |
|     49 |  438 | `	return rc;` |
|     27 |  439 |  |
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
|   9926 |  453 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceInherit(ph7_class *pSub,ph7_class *pBase)` |
|      5 |  454 |  |
|      - |  455 | `	ph7_class_method *pMeth;` |
|      - |  456 | `	ph7_class_attr *pAttr;` |
|      - |  457 | `	SyHashEntry *pEntry;` |
|      - |  458 | `	SyString *pName;` |
|      - |  459 | `	sxi32 rc;` |
|      - |  460 | `	/* Install in the derived hashtable */` |
|   9931 |  461 | `	SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|   9931 |  462 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
|      - |  463 | `	/* Copy constants */` |
|  14896 |  464 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
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
|   9931 |  476 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
|      - |  477 | `	/* Copy methods signature */` |
|  18242 |  478 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|      - |  479 | `		/* Make sure the method are not redeclared in the subclass */` |
|   3353 |  480 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   3353 |  481 | `		pName = &pMeth->sFunc.sName;` |
|   3353 |  482 | `		if( SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte) == 0 ){` |
|      - |  483 | `			/* Install the method */` |
|   3353 |  484 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|   3353 |  485 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  486 | `				return rc;` |
|      - |  487 | `			}` |
|   1674 |  488 | `		}` |
|      5 |  489 | `	}` |
|      - |  490 | `	/* Mark as subclass */` |
|   9931 |  491 | `	pSub->pBase = pBase;` |
|      - |  492 | `	/* All done */` |
|   9931 |  493 | `	return SXRET_OK;` |
|   4968 |  494 |  |
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
|  89458 |  508 | `PH7_PRIVATE sxi32 PH7_ClassImplement(ph7_class *pMain,ph7_class *pInterface)` |
|      5 |  509 |  |
|      - |  510 | `	ph7_class_attr *pAttr;` |
|      - |  511 | `	SyHashEntry *pEntry;` |
|      - |  512 | `	SyString *pName;` |
|      - |  513 | `	sxi32 rc;` |
|      - |  514 | `	/* First off,copy all constants declared inside the interface */` |
|  89463 |  515 | `	SyHashResetLoopCursor(&pInterface->hAttr);` |
| 134198 |  516 | `	while((pEntry = SyHashGetNextEntry(&pInterface->hAttr)) != 0 ){` |
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
|  89463 |  530 | `	SySetPut(&pMain->aInterface,(const void *)&pInterface);` |
|      - |  531 | `	/* Install interface method stubs into the implementing class.` |
|      - |  532 | `	 * Methods already defined in the class take precedence (they satisfy` |
|      - |  533 | `	 * the interface contract). Stubs retain PH7_CLASS_ATTR_ABSTRACT so` |
|      - |  534 | `	 * the unified check in GenStateCheckAbstractMethods catches missing ones.` |
|      - |  535 | `	 */` |
|      - |  536 | `	{` |
|      - |  537 | `		ph7_class_method *pMeth;` |
|      - |  538 | `		SyHashEntry *pMEntry;` |
|      - |  539 | `		SyString *pMName;` |
|  89463 |  540 | `		SyHashResetLoopCursor(&pInterface->hMethod);` |
| 283364 |  541 | `		while((pMEntry = SyHashGetNextEntry(&pInterface->hMethod)) != 0 ){` |
| 149177 |  542 | `			pMeth = (ph7_class_method *)pMEntry->pUserData;` |
| 149177 |  543 | `			pMName = &pMeth->sFunc.sName;` |
| 149177 |  544 | `			if( SyHashGet(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte) == 0 ){` |
|     18 |  545 | `				rc = SyHashInsert(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte,pMeth);` |
|     18 |  546 | `				if( rc != SXRET_OK ){` |
|    ! 0 |  547 | `					return rc;` |
|      - |  548 | `				}` |
|      7 |  549 | `			}` |
|      5 |  550 | `		}` |
|      - |  551 | `	}` |
|  89463 |  552 | `	return SXRET_OK;` |
|  44734 |  553 |  |
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
|   2326 |  633 | `static ph7_class_instance * NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|      5 |  634 |  |
|      - |  635 | `	ph7_class_instance *pThis;` |
|      - |  636 | `	/* Allocate a new instance */` |
|   2331 |  637 | `	pThis = (ph7_class_instance *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_instance));` |
|   2331 |  638 | `	if( pThis == 0 ){` |
|    ! 0 |  639 | `		return 0;` |
|      - |  640 | `	}` |
|      - |  641 | `	/* Zero the structure */` |
|   2331 |  642 | `	SyZero(pThis,sizeof(ph7_class_instance));` |
|      - |  643 | `	/* Initialize fields */` |
|   2331 |  644 | `	pThis->iRef = 1;` |
|   2331 |  645 | `	pThis->pVm = pVm;` |
|   2331 |  646 | `	pThis->pClass = pClass;` |
|   2331 |  647 | `	SyHashInit(&pThis->hAttr,&pVm->sAllocator,0,0);` |
|   2331 |  648 | `	return pThis;` |
|   1168 |  649 |  |
|      - |  650 | `/*` |
|      - |  651 | ` * Wrapper around the NewClassInstance() function defined above.` |
|      - |  652 | ` * See the block comment above for more information.` |
|      - |  653 | ` */` |
|   2276 |  654 | `PH7_PRIVATE ph7_class_instance * PH7_NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|      5 |  655 |  |
|      - |  656 | `	ph7_class_instance *pNew;` |
|      - |  657 | `	sxi32 rc;` |
|   2281 |  658 | `	pNew = NewClassInstance(&(*pVm),&(*pClass));` |
|   2281 |  659 | `	if( pNew == 0 ){` |
|    ! 0 |  660 | `		return 0;` |
|      - |  661 | `	}` |
|      - |  662 | `	/* Associate a private VM frame with this class instance */` |
|   2281 |  663 | `	rc = PH7_VmCreateClassInstanceFrame(&(*pVm),pNew);` |
|   2281 |  664 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  665 | `		SyMemBackendPoolFree(&pVm->sAllocator,pNew);` |
|    ! 0 |  666 | `		return 0;` |
|      - |  667 | `	}` |
|   2281 |  668 | `	return pNew;` |
|   1143 |  669 |  |
|      - |  670 | `/*` |
|      - |  671 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon] attribute.` |
|      - |  672 | ` * This function never fail.` |
|      - |  673 | ` */` |
|   1312 |  674 | `static ph7_value * ExtractClassAttrValue(ph7_vm *pVm,VmClassAttr *pAttr)` |
|      5 |  675 |  |
|      - |  676 | `	/* Extract the value */` |
|      - |  677 | `	ph7_value *pValue;` |
|   1317 |  678 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|   1317 |  679 | `	return pValue;` |
|      5 |  680 |  |
|      - |  681 | `/*` |
|      - |  682 | ` * Perform a clone operation on a class instance [i.e: Object in the PHP jargon].` |
|      - |  683 | ` * The following function is called when an object is cloned at run-time` |
|      - |  684 | ` * typically when the PH7_OP_CLONE instruction is executed.` |
|      - |  685 | ` * Notes on object cloning.` |
|      - |  686 | ` *` |
|      - |  687 | ` * According to PHP language reference manual.` |
|      - |  688 | ` * Creating a copy of an object with fully replicated properties is not always the wanted behavior.` |
|      - |  689 | ` * A good example of the need for copy constructors. Another example is if your object holds a reference` |
|      - |  690 | ` * to another object which it uses and when you replicate the parent object you want to create` |
|      - |  691 | ` * a new instance of this other object so that the replica has its own separate copy.` |
|      - |  692 | ` * An object copy is created by using the clone keyword (which calls the object's __clone() method if possible).` |
|      - |  693 | ` * An object's __clone() method cannot be called directly.` |
|      - |  694 | ` * $copy_of_object = clone $object;` |
|      - |  695 | ` * When an object is cloned, PHP 5 will perform a shallow copy of all of the object's properties.` |
|      - |  696 | ` * Any properties that are references to other variables, will remain references.` |
|      - |  697 | ` * Once the cloning is complete, if a __clone() method is defined, then the newly created object's __clone() method` |
|      - |  698 | ` * will be called, to allow any necessary properties that need to be changed.` |
|      - |  699 | ` * Example #1 Cloning an object` |
|      - |  700 | ` * <?php` |
|      - |  701 | ` * class SubObject` |
|      - |  702 | ` * {` |
|      - |  703 | ` *   static $instances = 0;` |
|      - |  704 | ` *   public $instance;` |
|      - |  705 | ` *` |
|      - |  706 | ` *   public function __construct() {` |
|      - |  707 | ` *       $this->instance = ++self::$instances;` |
|      - |  708 | ` *   }` |
|      - |  709 | ` *` |
|      - |  710 | ` *   public function __clone() {` |
|      - |  711 | ` *       $this->instance = ++self::$instances;` |
|      - |  712 | ` *   }` |
|      - |  713 | ` * }` |
|      - |  714 | ` *` |
|      - |  715 | ` * class MyCloneable` |
|      - |  716 | ` * {` |
|      - |  717 | ` *   public $object1;` |
|      - |  718 | ` *   public $object2;` |
|      - |  719 | ` *` |
|      - |  720 | ` *   function __clone()` |
|      - |  721 | ` *   {` |
|      - |  722 | ` *       // Force a copy of this->object, otherwise` |
|      - |  723 | ` *       // it will point to same object.` |
|      - |  724 | ` *       $this->object1 = clone $this->object1;` |
|      - |  725 | ` *   }` |
|      - |  726 | ` * }` |
|      - |  727 | ` * $obj = new MyCloneable();` |
|      - |  728 | ` * $obj->object1 = new SubObject();` |
|      - |  729 | ` * $obj->object2 = new SubObject();` |
|      - |  730 | ` * $obj2 = clone $obj;` |
|      - |  731 | ` * print("Original Object:\n");` |
|      - |  732 | ` * print_r($obj);` |
|      - |  733 | ` * print("Cloned Object:\n");` |
|      - |  734 | ` * print_r($obj2);` |
|      - |  735 | ` * ?>` |
|      - |  736 | ` * The above example will output:` |
|      - |  737 | ` * Original Object:` |
|      - |  738 | ` * MyCloneable Object` |
|      - |  739 | ` * (` |
|      - |  740 | ` *   [object1] => SubObject Object` |
|      - |  741 | ` *       (` |
|      - |  742 | ` *           [instance] => 1` |
|      - |  743 | ` *       )` |
|      - |  744 | ` *` |
|      - |  745 | ` *   [object2] => SubObject Object` |
|      - |  746 | ` *       (` |
|      - |  747 | ` *           [instance] => 2` |
|      - |  748 | ` *       )` |
|      - |  749 | ` *` |
|      - |  750 | ` * )` |
|      - |  751 | ` * Cloned Object:` |
|      - |  752 | ` * MyCloneable Object` |
|      - |  753 | ` * (` |
|      - |  754 | ` *   [object1] => SubObject Object` |
|      - |  755 | ` *       (` |
|      - |  756 | ` *           [instance] => 3` |
|      - |  757 | ` *       )` |
|      - |  758 | ` *` |
|      - |  759 | ` *   [object2] => SubObject Object` |
|      - |  760 | ` *       (` |
|      - |  761 | ` *           [instance] => 2` |
|      - |  762 | ` *       )` |
|      - |  763 | ` * )` |
|      - |  764 | ` */` |
|     50 |  765 | `PH7_PRIVATE ph7_class_instance * PH7_CloneClassInstance(ph7_class_instance *pSrc)` |
|      4 |  766 |  |
|      - |  767 | `	ph7_class_instance *pClone;` |
|      - |  768 | `	ph7_class_method *pMethod;` |
|      - |  769 | `	SyHashEntry *pEntry2;` |
|      - |  770 | `	SyHashEntry *pEntry;` |
|      - |  771 | `	ph7_vm *pVm;` |
|      - |  772 | `	sxi32 rc;` |
|      - |  773 | `	/* Allocate a new instance */` |
|     54 |  774 | `	pVm = pSrc->pVm;` |
|     54 |  775 | `	pClone = NewClassInstance(pVm,pSrc->pClass);` |
|     54 |  776 | `	if( pClone == 0 ){` |
|    ! 0 |  777 | `		return 0;` |
|      - |  778 | `	}` |
|      - |  779 | `	/* Associate a private VM frame with this class instance */` |
|     54 |  780 | `	rc = PH7_VmCreateClassInstanceFrame(pVm,pClone);` |
|     54 |  781 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  782 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClone);` |
|    ! 0 |  783 | `		return 0;` |
|      - |  784 | `	}` |
|      - |  785 | `	/* Duplicate object values */` |
|     54 |  786 | `	SyHashResetLoopCursor(&pSrc->hAttr);` |
|     54 |  787 | `	SyHashResetLoopCursor(&pClone->hAttr);` |
|    133 |  788 | `	while((pEntry = SyHashGetNextEntry(&pSrc->hAttr)) != 0 && (pEntry2 = SyHashGetNextEntry(&pClone->hAttr)) != 0 ){` |
|     58 |  789 | `		VmClassAttr *pSrcAttr = (VmClassAttr *)pEntry->pUserData;` |
|     58 |  790 | `		VmClassAttr *pDestAttr = (VmClassAttr *)pEntry2->pUserData;` |
|      - |  791 | `		/* Duplicate non-static attribute */` |
|     58 |  792 | `		if( (pSrcAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|      - |  793 | `			ph7_value *pvSrc,*pvDest;` |
|     58 |  794 | `			pvSrc = ExtractClassAttrValue(pVm,pSrcAttr);` |
|     58 |  795 | `			pvDest = ExtractClassAttrValue(pVm,pDestAttr);` |
|     58 |  796 | `			if( pvSrc && pvDest ){` |
|     58 |  797 | `				PH7_MemObjStore(pvSrc,pvDest);` |
|     27 |  798 | `			}` |
|      - |  799 | `			/* Carry over the per-instance state so the clone matches the source:` |
|      - |  800 | `			 * VM_CLASS_ATTR_UNINIT marks a typed property as not-yet-initialized` |
|      - |  801 | `			 * and doubles as the readonly write-once latch — without this a clone` |
|      - |  802 | `			 * would reset to uninitialized (losing the value's readiness) and a` |
|      - |  803 | `			 * readonly property would become writable again. */` |
|     58 |  804 | `			pDestAttr->iState = pSrcAttr->iState;` |
|     27 |  805 | `		}` |
|      4 |  806 | `	}` |
|      - |  807 | `	/* call the __clone method on the cloned object if available */` |
|     54 |  808 | `	pMethod = PH7_ClassExtractMethod(pClone->pClass,"__clone",sizeof("__clone")-1);` |
|     54 |  809 | `	if( pMethod ){` |
|     38 |  810 | `		if( pMethod->iCloneDepth < 16 ){` |
|     36 |  811 | `			pMethod->iCloneDepth++;` |
|     36 |  812 | `			PH7_VmCallClassMethod(pVm,pClone,pMethod,0,0,0);` |
|     19 |  813 | `		}else{` |
|      - |  814 | `			/* Nesting limit reached */` |
|      3 |  815 | `			PH7_VmThrowError(pVm,0,PH7_CTX_ERR,"Object clone limit reached,no more call to __clone()");` |
|      - |  816 | `		}` |
|      - |  817 | `		/* Reset the cursor */` |
|     38 |  818 | `		pMethod->iCloneDepth = 0;` |
|     18 |  819 | `	}` |
|      - |  820 | `	/* Return the cloned object */` |
|     54 |  821 | `	return pClone;` |
|     29 |  822 |  |
|      - |  823 | `#define CLASS_INSTANCE_DESTROYED 0x001 /* Instance is released */` |
|      - |  824 | `/*` |
|      - |  825 | ` * Release a class instance [i.e: Object in the PHP jargon] and invoke any defined destructor.` |
|      - |  826 | ` * This routine is invoked as soon as there are no other references to a particular` |
|      - |  827 | ` * class instance.` |
|      - |  828 | ` */` |
|   1708 |  829 | `static void PH7_ClassInstanceRelease(ph7_class_instance *pThis)` |
|      5 |  830 |  |
|      - |  831 | `	ph7_class_method *pDestr;` |
|      - |  832 | `	SyHashEntry *pEntry;` |
|      - |  833 | `	ph7_class *pClass;` |
|      - |  834 | `	ph7_vm *pVm;` |
|   1713 |  835 | `	if( pThis->iFlags & CLASS_INSTANCE_DESTROYED ){` |
|      - |  836 | `		/*` |
|      - |  837 | `		 * Already destroyed,return immediately.` |
|      - |  838 | `		 * This could happend if someone perform unset($this) in the destructor body.` |
|      - |  839 | `		 */` |
|      9 |  840 | `		return;` |
|      - |  841 | `	}` |
|      - |  842 | `	/* Mark as destroyed */` |
|   1705 |  843 | `	pThis->iFlags \|= CLASS_INSTANCE_DESTROYED;` |
|      - |  844 | `	/* Invoke any defined destructor if available */` |
|   1705 |  845 | `	pVm = pThis->pVm;` |
|   1705 |  846 | `	pClass = pThis->pClass;` |
|   1705 |  847 | `	pDestr = PH7_ClassExtractMethod(pClass,"__destruct",sizeof("__destruct")-1);` |
|   1705 |  848 | `	if( pDestr && !pVm->bInReset ){` |
|      - |  849 | `		/* Invoke the destructor. Skipped during ph7_vm_reset() bulk teardown:` |
|      - |  850 | `		 * running user PHP against a half-reset VM is unsafe (see bInReset). */` |
|     13 |  851 | `		pThis->iRef = 2; /* Prevent garbage collection */` |
|     13 |  852 | `		PH7_VmCallClassMethod(pVm,pThis,pDestr,0,0,0);` |
|      6 |  853 | `	}` |
|      - |  854 | `	/* Release non-static attributes */` |
|   1705 |  855 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|   8187 |  856 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|   6487 |  857 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|   6487 |  858 | `		if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|      - |  859 | `			/* Drop any typed-property enforcement slot registered for this` |
|      - |  860 | `			 * memobj. Must happen before the memobj is returned to the free` |
|      - |  861 | `			 * list so a future recycled slot does not inherit the stale entry. */` |
|   6469 |  862 | `			if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|    262 |  863 | `				SyHashDeleteEntry(&pVm->hTypedSlot,` |
|    172 |  864 | `					(const void *)&pVmAttr->nIdx,sizeof(sxu32),0);` |
|     86 |  865 | `			}` |
|   6469 |  866 | `			PH7_VmUnsetMemObj(pVm,pVmAttr->nIdx,TRUE);` |
|   3232 |  867 | `		}` |
|   6487 |  868 | `		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      5 |  869 | `	}` |
|      - |  870 | `	/* Release the whole structure */` |
|   1705 |  871 | `	SyHashRelease(&pThis->hAttr);` |
|   1705 |  872 | `	SyMemBackendPoolFree(&pVm->sAllocator,pThis);` |
|    859 |  873 |  |
|      - |  874 | `/*` |
|      - |  875 | ` * Decrement the reference count of a class instance [i.e Object in the PHP jargon].` |
|      - |  876 | ` * If the reference count reaches zero,release the whole instance.` |
|      - |  877 | ` */` |
|  30668 |  878 | `PH7_PRIVATE void PH7_ClassInstanceUnref(ph7_class_instance *pThis)` |
|      5 |  879 |  |
|  30673 |  880 | `	pThis->iRef--;` |
|  30673 |  881 | `	if( pThis->iRef < 1 ){` |
|      - |  882 | `		/* No more reference to this instance */` |
|   1713 |  883 | `		PH7_ClassInstanceRelease(&(*pThis));` |
|    854 |  884 | `	}` |
|  30673 |  885 |  |
|      - |  886 | `/*` |
|      - |  887 | ` * Compare two class instances [i.e: Objects in the PHP jargon]` |
|      - |  888 | ` * Note on objects comparison:` |
|      - |  889 | ` *  According to the PHP langauge reference manual` |
|      - |  890 | ` *  When using the comparison operator (==), object variables are compared in a simple manner` |
|      - |  891 | ` *  namely: Two object instances are equal if they have the same attributes and values, and are` |
|      - |  892 | ` *  instances of the same class.` |
|      - |  893 | ` *  On the other hand, when using the identity operator (===), object variables are identical` |
|      - |  894 | ` *  if and only if they refer to the same instance of the same class.` |
|      - |  895 | ` *  An example will clarify these rules.` |
|      - |  896 | ` *  Example #1 Example of object comparison` |
|      - |  897 | ` *  <?php` |
|      - |  898 | ` *    function bool2str($bool)` |
|      - |  899 | ` * {` |
|      - |  900 | ` *   if ($bool === false) {` |
|      - |  901 | ` *       return 'FALSE';` |
|      - |  902 | ` *   } else {` |
|      - |  903 | ` *       return 'TRUE';` |
|      - |  904 | ` *   }` |
|      - |  905 | ` * }` |
|      - |  906 | ` * function compareObjects(&$o1, &$o2)` |
|      - |  907 | ` * {` |
|      - |  908 | ` *   echo 'o1 == o2 : ' . bool2str($o1 == $o2) . "\n";` |
|      - |  909 | ` *   echo 'o1 != o2 : ' . bool2str($o1 != $o2) . "\n";` |
|      - |  910 | ` *   echo 'o1 === o2 : ' . bool2str($o1 === $o2) . "\n";` |
|      - |  911 | ` *   echo 'o1 !== o2 : ' . bool2str($o1 !== $o2) . "\n";` |
|      - |  912 | ` * }` |
|      - |  913 | ` * class Flag` |
|      - |  914 | ` * {` |
|      - |  915 | ` *   public $flag;` |
|      - |  916 | ` *` |
|      - |  917 | ` *   function Flag($flag = true) {` |
|      - |  918 | ` *       $this->flag = $flag;` |
|      - |  919 | ` *   }` |
|      - |  920 | ` * }` |
|      - |  921 | ` *` |
|      - |  922 | ` * class OtherFlag` |
|      - |  923 | ` * {` |
|      - |  924 | ` *   public $flag;` |
|      - |  925 | ` *` |
|      - |  926 | ` *   function OtherFlag($flag = true) {` |
|      - |  927 | ` *       $this->flag = $flag;` |
|      - |  928 | ` *   }` |
|      - |  929 | ` * }` |
|      - |  930 | ` *` |
|      - |  931 | ` * $o = new Flag();` |
|      - |  932 | ` * $p = new Flag();` |
|      - |  933 | ` * $q = $o;` |
|      - |  934 | ` * $r = new OtherFlag();` |
|      - |  935 | ` *` |
|      - |  936 | ` * echo "Two instances of the same class\n";` |
|      - |  937 | ` * compareObjects($o, $p);` |
|      - |  938 | ` * echo "\nTwo references to the same instance\n";` |
|      - |  939 | ` * compareObjects($o, $q);` |
|      - |  940 | ` * echo "\nInstances of two different classes\n";` |
|      - |  941 | ` * compareObjects($o, $r);` |
|      - |  942 | ` * ?>` |
|      - |  943 | ` * The above example will output:` |
|      - |  944 | ` * Two instances of the same class` |
|      - |  945 | ` * o1 == o2 : TRUE` |
|      - |  946 | ` * o1 != o2 : FALSE` |
|      - |  947 | ` * o1 === o2 : FALSE` |
|      - |  948 | ` * o1 !== o2 : TRUE` |
|      - |  949 | ` * Two references to the same instance` |
|      - |  950 | ` * o1 == o2 : TRUE` |
|      - |  951 | ` * o1 != o2 : FALSE` |
|      - |  952 | ` * o1 === o2 : TRUE` |
|      - |  953 | ` * o1 !== o2 : FALSE` |
|      - |  954 | ` * Instances of two different classes` |
|      - |  955 | ` * o1 == o2 : FALSE` |
|      - |  956 | ` * o1 != o2 : TRUE` |
|      - |  957 | ` * o1 === o2 : FALSE` |
|      - |  958 | ` * o1 !== o2 : TRUE` |
|      - |  959 | ` *` |
|      - |  960 | ` * This function return 0 if the objects are equals according to the comprison rules defined above.` |
|      - |  961 | ` * Any other return values indicates difference.` |
|      - |  962 | ` */` |
|    174 |  963 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest)` |
|      3 |  964 |  |
|      - |  965 | `	SyHashEntry *pEntry,*pEntry2;` |
|      - |  966 | `	ph7_value sV1,sV2;` |
|      - |  967 | `	sxi32 rc;` |
|    177 |  968 | `	if( iNest > 31 ){` |
|      - |  969 | `		/* Nesting limit reached */` |
|      6 |  970 | `		PH7_VmThrowError(pLeft->pVm,0,PH7_CTX_ERR,"Nesting limit reached: Infinite recursion?");` |
|      6 |  971 | `		return 1;` |
|      - |  972 | `	}` |
|      - |  973 | `	/* Comparison is performed only if the objects are instance of the same class */` |
|    173 |  974 | `	if( pLeft->pClass != pRight->pClass ){` |
|      7 |  975 | `		return 1;` |
|      - |  976 | `	}` |
|    167 |  977 | `	if( bStrict ){` |
|      - |  978 | `		/*` |
|      - |  979 | `		 * According to the PHP language reference manual:` |
|      - |  980 | `		 *  when using the identity operator (===), object variables` |
|      - |  981 | `		 *  are identical if and only if they refer to the same instance` |
|      - |  982 | `		 *  of the same class.` |
|      - |  983 | `		 */` |
|     25 |  984 | `		return !(pLeft == pRight);` |
|      - |  985 | `	}` |
|      - |  986 | `	/*` |
|      - |  987 | `	 * Attribute comparison.` |
|      - |  988 | `	 * According to the PHP reference manual:` |
|      - |  989 | `	 *  When using the comparison operator (==), object variables are compared` |
|      - |  990 | `	 *  in a simple manner, namely: Two object instances are equal if they have` |
|      - |  991 | `	 *  the same attributes and values, and are instances of the same class.` |
|      - |  992 | `	 */` |
|    143 |  993 | `	if( pLeft == pRight ){` |
|      - |  994 | `		/* Same instance,don't bother processing,object are equals */` |
|      3 |  995 | `		return 0;` |
|      - |  996 | `	}` |
|    141 |  997 | `	SyHashResetLoopCursor(&pLeft->hAttr);` |
|    141 |  998 | `	SyHashResetLoopCursor(&pRight->hAttr);` |
|    141 |  999 | `	PH7_MemObjInit(pLeft->pVm,&sV1);` |
|    141 | 1000 | `	PH7_MemObjInit(pLeft->pVm,&sV2);` |
|    141 | 1001 | `	sV1.nIdx = sV2.nIdx = SXU32_HIGH;` |
|    224 | 1002 | `	while((pEntry = SyHashGetNextEntry(&pLeft->hAttr)) != 0 && (pEntry2 = SyHashGetNextEntry(&pRight->hAttr)) != 0 ){` |
|    147 | 1003 | `		VmClassAttr *p1 = (VmClassAttr *)pEntry->pUserData;` |
|    147 | 1004 | `		VmClassAttr *p2 = (VmClassAttr *)pEntry2->pUserData;` |
|      - | 1005 | `		/* Compare only non-static attribute */` |
|    147 | 1006 | `		if( (p1->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|      - | 1007 | `			ph7_value *pL,*pR;` |
|    147 | 1008 | `			pL = ExtractClassAttrValue(pLeft->pVm,p1);` |
|    147 | 1009 | `			pR = ExtractClassAttrValue(pRight->pVm,p2);` |
|    147 | 1010 | `			if( pL && pR ){` |
|    147 | 1011 | `				PH7_MemObjLoad(pL,&sV1);` |
|    147 | 1012 | `				PH7_MemObjLoad(pR,&sV2);` |
|      - | 1013 | `				/* Compare the two values now */` |
|    147 | 1014 | `				rc = PH7_MemObjCmp(&sV1,&sV2,bStrict,iNest+1);` |
|    147 | 1015 | `				PH7_MemObjRelease(&sV1);` |
|    147 | 1016 | `				PH7_MemObjRelease(&sV2);` |
|    147 | 1017 | `				if( rc != 0 ){` |
|      - | 1018 | `					/* Not equals */` |
|    133 | 1019 | `					return rc;` |
|      - | 1020 | `				}` |
|      7 | 1021 | `			}` |
|      7 | 1022 | `		}` |
|      1 | 1023 | `	}` |
|      - | 1024 | `	/* Object are equals */` |
|      9 | 1025 | `	return 0;` |
|     90 | 1026 |  |
|      - | 1027 | `/*` |
|      - | 1028 | ` * Dump a class instance and the store the dump in the BLOB given` |
|      - | 1029 | ` * as the first argument.` |
|      - | 1030 | ` * Note that only non-static/non-constants attribute are dumped.` |
|      - | 1031 | ` * This function is typically invoked when the user issue a call` |
|      - | 1032 | ` * to [var_dump(),var_export(),print_r(),...].` |
|      - | 1033 | ` * This function SXRET_OK on success. Any other return value including` |
|      - | 1034 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|      - | 1035 | ` */` |
|    132 | 1036 | `PH7_PRIVATE sxi32 PH7_ClassInstanceDump(SyBlob *pOut,ph7_class_instance *pThis,int ShowType,int nTab,int nDepth)` |
|      1 | 1037 |  |
|      - | 1038 | `	SyHashEntry *pEntry;` |
|      - | 1039 | `	ph7_value *pValue;` |
|      - | 1040 | `	sxi32 rc;` |
|      - | 1041 | `	int i;` |
|    133 | 1042 | `	if( nDepth > 31 ){` |
|      - | 1043 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|      - | 1044 | `		/* Nesting limit reached..halt immediately*/` |
|      5 | 1045 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|      5 | 1046 | `		if( ShowType ){` |
|      5 | 1047 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|      2 | 1048 | `		}` |
|      5 | 1049 | `		return SXERR_LIMIT;` |
|      - | 1050 | `	}` |
|    129 | 1051 | `	rc = SXRET_OK;` |
|    129 | 1052 | `	if( !ShowType ){` |
|      3 | 1053 | `		SyBlobAppend(&(*pOut),"Object(",sizeof("Object(")-1);` |
|      1 | 1054 | `	}` |
|      - | 1055 | `	/* Append class name */` |
|    129 | 1056 | `	SyBlobFormat(&(*pOut),"%z) {",&pThis->pClass->sName);` |
|      - | 1057 | `#ifdef __WINNT__` |
|      1 | 1058 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|      - | 1059 | `#else` |
|    128 | 1060 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      - | 1061 | `#endif` |
|      - | 1062 | `	/* Dump object attributes */` |
|    129 | 1063 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|    201 | 1064 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|    133 | 1065 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    133 | 1066 | `		if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|      - | 1067 | `			/* Dump non-static/constant attribute only */` |
|   3985 | 1068 | `			for( i = 0 ; i < nTab ; i++ ){` |
|   3853 | 1069 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|   1927 | 1070 | `			}` |
|    133 | 1071 | `			pValue = ExtractClassAttrValue(pThis->pVm,pVmAttr);` |
|    133 | 1072 | `			if( pValue ){` |
|    133 | 1073 | `				SyBlobFormat(&(*pOut),"['%z'] =>",&pVmAttr->pAttr->sName);` |
|      - | 1074 | `#ifdef __WINNT__` |
|      1 | 1075 | `				SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|      - | 1076 | `#else` |
|    132 | 1077 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      - | 1078 | `#endif` |
|    133 | 1079 | `				rc = PH7_MemObjDump(&(*pOut),pValue,ShowType,nTab+1,nDepth,0);` |
|    133 | 1080 | `				if( rc == SXERR_LIMIT ){` |
|    125 | 1081 | `					break;` |
|      - | 1082 | `				}` |
|      4 | 1083 | `			}` |
|      4 | 1084 | `		}` |
|      1 | 1085 | `	}` |
|   3977 | 1086 | `	for( i = 0 ; i < nTab ; i++ ){` |
|   3849 | 1087 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|   1925 | 1088 | `	}` |
|    129 | 1089 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|    129 | 1090 | `	return rc;` |
|     67 | 1091 |  |
|      - | 1092 | `/*` |
|      - | 1093 | ` * Call a magic method [i.e: __toString(),__toBool(),__Invoke()...]` |
|      - | 1094 | ` * Return SXRET_OK on successfull call. Any other return value indicates failure.` |
|      - | 1095 | ` * Notes on magic methods.` |
|      - | 1096 | ` * According to the PHP language reference manual.` |
|      - | 1097 | ` *  The function names __construct(), __destruct(), __call(), __callStatic()` |
|      - | 1098 | ` *  __get(),  __toString(), __invoke(), __clone() are magical in PHP classes.` |
|      - | 1099 | ` * You cannot have functions with these names in any of your classes unless` |
|      - | 1100 | ` * you want the magic functionality associated with them.` |
|      - | 1101 | ` * Example of magical methods:` |
|      - | 1102 | ` * __toString()` |
|      - | 1103 | ` *  The __toString() method allows a class to decide how it will react when it is treated like` |
|      - | 1104 | ` *  a string. For example, what echo $obj; will print. This method must return a string.` |
|      - | 1105 | ` *  Example #2 Simple example` |
|      - | 1106 | ` * <?php` |
|      - | 1107 | ` * // Declare a simple class` |
|      - | 1108 | ` * class TestClass` |
|      - | 1109 | ` * {` |
|      - | 1110 | ` *   public $foo;` |
|      - | 1111 | ` *` |
|      - | 1112 | ` *   public function __construct($foo)` |
|      - | 1113 | ` *   {` |
|      - | 1114 | ` *       $this->foo = $foo;` |
|      - | 1115 | ` *   }` |
|      - | 1116 | ` *` |
|      - | 1117 | ` *   public function __toString()` |
|      - | 1118 | ` *   {` |
|      - | 1119 | ` *       return $this->foo;` |
|      - | 1120 | ` *   }` |
|      - | 1121 | ` * }` |
|      - | 1122 | ` * $class = new TestClass('Hello');` |
|      - | 1123 | ` * echo $class;` |
|      - | 1124 | ` * ?>` |
|      - | 1125 | ` * The above example will output:` |
|      - | 1126 | ` *  Hello` |
|      - | 1127 | ` *` |
|      - | 1128 | ` * Note that PH7 does not support all the magical method and introudces __toFloat(),__toInt()` |
|      - | 1129 | ` * which have the same behaviour as __toString() but for float and integer types` |
|      - | 1130 | ` * respectively.` |
|      - | 1131 | ` * Refer to the official documentation for more information.` |
|      - | 1132 | ` */` |
|      2 | 1133 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallMagicMethod(` |
|      - | 1134 | `	ph7_vm *pVm,               /* VM that own all this stuff */` |
|      - | 1135 | `	ph7_class *pClass,         /* Target class */` |
|      - | 1136 | `	ph7_class_instance *pThis, /* Target object */` |
|      - | 1137 | `	const char *zMethod,       /* Magic method name [i.e: __toString()]*/` |
|      - | 1138 | `	sxu32 nByte,               /* zMethod length*/` |
|      - | 1139 | `	const SyString *pAttrName  /* Attribute name */` |
|      - | 1140 | `	)` |
|      1 | 1141 |  |
|      3 | 1142 | `	ph7_value *apArg[2] = { 0 , 0 };` |
|      - | 1143 | `	ph7_class_method *pMeth;` |
|      - | 1144 | `	ph7_value sAttr; /* cc warning */` |
|      - | 1145 | `	sxi32 rc;` |
|      - | 1146 | `	int nArg;` |
|      - | 1147 | `	/* Make sure the magic method is available */` |
|      3 | 1148 | `	pMeth = PH7_ClassExtractMethod(&(*pClass),zMethod,nByte);` |
|      3 | 1149 | `	if( pMeth == 0 ){` |
|      - | 1150 | `		/* No such method,return immediately */` |
|      3 | 1151 | `		return SXERR_NOTFOUND;` |
|      - | 1152 | `	}` |
|    ! 0 | 1153 | `	nArg = 0;` |
|      - | 1154 | `	/* Copy arguments */` |
|    ! 0 | 1155 | `	if( pAttrName ){` |
|    ! 0 | 1156 | `		PH7_MemObjInitFromString(pVm,&sAttr,pAttrName);` |
|    ! 0 | 1157 | `		sAttr.nIdx = SXU32_HIGH; /* Mark as constant */` |
|    ! 0 | 1158 | `		apArg[0] = &sAttr;` |
|    ! 0 | 1159 | `		nArg = 1;` |
|    ! 0 | 1160 | `	}` |
|      - | 1161 | `	/* Call the magic method now */` |
|    ! 0 | 1162 | `	rc = PH7_VmCallClassMethod(pVm,&(*pThis),pMeth,0,nArg,apArg);` |
|      - | 1163 | `	/* Clean up */` |
|    ! 0 | 1164 | `	if( pAttrName ){` |
|    ! 0 | 1165 | `		PH7_MemObjRelease(&sAttr);` |
|    ! 0 | 1166 | `	}` |
|    ! 0 | 1167 | `	return rc;` |
|      2 | 1168 |  |
|      - | 1169 | `/*` |
|      - | 1170 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon].` |
|      - | 1171 | ` * This function is simply a wrapper on ExtractClassAttrValue().` |
|      - | 1172 | ` */` |
|     44 | 1173 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceExtractAttrValue(ph7_class_instance *pThis,VmClassAttr *pAttr)` |
|      2 | 1174 |  |
|      - | 1175 | `   /* Extract the attribute value */` |
|      - | 1176 | `	ph7_value *pValue;` |
|     46 | 1177 | `	pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     46 | 1178 | `	return pValue;` |
|      2 | 1179 |  |
|      - | 1180 | `/*` |
|      - | 1181 | ` * Convert a class instance [i.e: Object in the PHP jargon] into a hashmap [i.e: array in the PHP jargon].` |
|      - | 1182 | ` * Return SXRET_OK on success. Any other value indicates failure.` |
|      - | 1183 | ` * Note on object conversion to array:` |
|      - | 1184 | ` *  Acccording to the PHP language reference manual` |
|      - | 1185 | ` *  If an object is converted to an array, the result is an array whose elements are the object's properties.` |
|      - | 1186 | ` *  The keys are the member variable names.` |
|      - | 1187 | ` *` |
|      - | 1188 | ` *  The following example:` |
|      - | 1189 | ` *  class Test {` |
|      - | 1190 | ` *   public $A = 25<<1;  // 50` |
|      - | 1191 | ` *	 public $c = rand_str(3);   // Random string of length 3` |
|      - | 1192 | ` *	 public $d = rand() & 1023; // Random number between 0..1023` |
|      - | 1193 | ` *  }` |
|      - | 1194 | ` *  var_dump((array) new Test());` |
|      - | 1195 | ` *	Will output:` |
|      - | 1196 | ` *  array(3) {` |
|      - | 1197 | ` *   [A] =>` |
|      - | 1198 | ` *      int(50)` |
|      - | 1199 | ` *   [c] =>` |
|      - | 1200 | ` *     string(3 'aps')` |
|      - | 1201 | ` *   [d] =>` |
|      - | 1202 | ` *     int(991)` |
|      - | 1203 | ` *  }` |
|      - | 1204 | ` * You have noticed that PH7 allow class attributes [i.e: $a,$c,$d in the example above]` |
|      - | 1205 | ` * have any complex expression (even function calls/Annonymous functions) as their default` |
|      - | 1206 | ` * value unlike the standard PHP engine.` |
|      - | 1207 | ` * This is a very powerful feature that you have to look at.` |
|      - | 1208 | ` */` |
|      6 | 1209 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|      1 | 1210 |  |
|      - | 1211 | `	SyHashEntry *pEntry;` |
|      - | 1212 | `	SyString *pAttrName;` |
|      - | 1213 | `	VmClassAttr *pAttr;` |
|      - | 1214 | `	ph7_value *pValue;` |
|      - | 1215 | `	ph7_value sName;` |
|      - | 1216 | `	/* Reset the loop cursor */` |
|      7 | 1217 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      7 | 1218 | `	PH7_MemObjInitFromString(pThis->pVm,&sName,0);` |
|     20 | 1219 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|      - | 1220 | `		/* Point to the current attribute */` |
|     11 | 1221 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1222 | `		/* Extract attribute value */` |
|     11 | 1223 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     11 | 1224 | `		if( pValue ){` |
|      - | 1225 | `			/* Build attribute name */` |
|     11 | 1226 | `			pAttrName = &pAttr->pAttr->sName;` |
|     11 | 1227 | `			PH7_MemObjStringAppend(&sName,pAttrName->zString,pAttrName->nByte);` |
|      - | 1228 | `			/* Perform the insertion */` |
|     11 | 1229 | `			PH7_HashmapInsert(pMap,&sName,pValue);` |
|      - | 1230 | `			/* Reset the string cursor */` |
|     11 | 1231 | `			SyBlobReset(&sName.sBlob);` |
|      5 | 1232 | `		}` |
|      1 | 1233 | `	}` |
|      7 | 1234 | `	PH7_MemObjRelease(&sName);` |
|      7 | 1235 | `	return SXRET_OK;` |
|      1 | 1236 |  |
|      - | 1237 | `/*` |
|      - | 1238 | ` * Iterate throw class attributes and invoke the given callback [i.e: xWalk()] for each` |
|      - | 1239 | ` * retrieved attribute.` |
|      - | 1240 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|      - | 1241 | ` * the attribute value in the callback body will not alter the real attribute value.` |
|      - | 1242 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|      - | 1243 | ` * a value different from PH7_OK.` |
|      - | 1244 | ` * Refer to [ph7_object_walk()] for more information.` |
|      - | 1245 | ` */` |
|      2 | 1246 | `PH7_PRIVATE sxi32 PH7_ClassInstanceWalk(` |
|      - | 1247 | `	ph7_class_instance *pThis, /* Target object */` |
|      - | 1248 | `	int (*xWalk)(const char *,ph7_value *,void *), /* Walker callback */` |
|      - | 1249 | `	void *pUserData /* Last argument to xWalk() */` |
|      - | 1250 | `	)` |
|      1 | 1251 |  |
|      - | 1252 | `	SyHashEntry *pEntry; /* Hash entry */` |
|      - | 1253 | `	VmClassAttr *pAttr;  /* Pointer to the attribute */` |
|      - | 1254 | `	ph7_value *pValue;   /* Attribute value */` |
|      - | 1255 | `	ph7_value sValue;    /* Copy of the attribute value */` |
|      - | 1256 | `	int rc;` |
|      - | 1257 | `	/* Reset the loop cursor */` |
|      3 | 1258 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      3 | 1259 | `	PH7_MemObjInit(pThis->pVm,&sValue);` |
|      - | 1260 | `	/* Start the walk process */` |
|      8 | 1261 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|      - | 1262 | `		/* Point to the current attribute */` |
|      5 | 1263 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1264 | `		/* Extract attribute value */` |
|      5 | 1265 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|      5 | 1266 | `		if( pValue ){` |
|      5 | 1267 | `			PH7_MemObjLoad(pValue,&sValue);` |
|      - | 1268 | `			/* Invoke the supplied callback */` |
|      5 | 1269 | `			rc =  xWalk(SyStringData(&pAttr->pAttr->sName),&sValue,pUserData);` |
|      5 | 1270 | `			PH7_MemObjRelease(&sValue);` |
|      5 | 1271 | `			if( rc != PH7_OK){` |
|      - | 1272 | `				/* User callback request an operation abort */` |
|    ! 0 | 1273 | `				return SXERR_ABORT;` |
|      - | 1274 | `			}` |
|      2 | 1275 | `		}` |
|      1 | 1276 | `	}` |
|      - | 1277 | `	/* All done */` |
|      3 | 1278 | `	return SXRET_OK;` |
|      2 | 1279 |  |
|      - | 1280 | `/*` |
|      - | 1281 | ` * Extract a class atrribute value.` |
|      - | 1282 | ` * Return a pointer to the attribute value on success. Otherwise NULL.` |
|      - | 1283 | ` * Note:` |
|      - | 1284 | ` *  Access to static and constant attribute is not allowed. That is,the function` |
|      - | 1285 | ` *  will return NULL in case someone (host-application code) try to extract` |
|      - | 1286 | ` *  a static/constant attribute.` |
|      - | 1287 | ` */` |
|    726 | 1288 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceFetchAttr(ph7_class_instance *pThis,const SyString *pName)` |
|      5 | 1289 |  |
|      - | 1290 | `	SyHashEntry *pEntry;` |
|      - | 1291 | `	VmClassAttr *pAttr;` |
|      - | 1292 | `	/* Query the attribute hashtable */` |
|    731 | 1293 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);` |
|    731 | 1294 | `	if( pEntry == 0 ){` |
|      - | 1295 | `		/* No such attribute */` |
|    ! 0 | 1296 | `		return 0;` |
|      - | 1297 | `	}` |
|      - | 1298 | `	/* Point to the class atrribute */` |
|    731 | 1299 | `	pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1300 | `	/* Check if we are dealing with a static/constant attribute */` |
|    731 | 1301 | `	if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|      - | 1302 | `		/* Access is forbidden */` |
|    ! 0 | 1303 | `		return 0;` |
|      - | 1304 | `	}` |
|      - | 1305 | `	/* Return the attribute value */` |
|    731 | 1306 | `	return ExtractClassAttrValue(pThis->pVm,pAttr);` |
|    368 | 1307 |  |
|      - | 1308 |  |
