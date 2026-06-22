# src/ph7/oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 458/509 lines (89.98%)

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
| 122830 |   14 | `PH7_PRIVATE ph7_class * PH7_NewRawClass(ph7_vm *pVm,const SyString *pName,sxu32 nLine)` |
|      5 |   15 |  |
|      - |   16 | `	ph7_class *pClass;` |
|      - |   17 | `	char *zName;` |
|      - |   18 | `	/* Allocate a new instance */` |
| 122835 |   19 | `	pClass = (ph7_class *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class));` |
| 122835 |   20 | `	if( pClass == 0 ){` |
|    ! 0 |   21 | `		return 0;` |
|      - |   22 | `	}` |
|      - |   23 | `	/* Zero the structure */` |
| 122835 |   24 | `	SyZero(pClass,sizeof(ph7_class));` |
|      - |   25 | `	/* Duplicate class name */` |
| 122835 |   26 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
| 122835 |   27 | `	if( zName == 0 ){` |
|    ! 0 |   28 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClass);` |
|    ! 0 |   29 | `		return 0;` |
|      - |   30 | `	}` |
|      - |   31 | `	/* Initialize fields */` |
| 122835 |   32 | `	SyStringInitFromBuf(&pClass->sName,zName,pName->nByte);` |
| 122835 |   33 | `	SyHashInit(&pClass->hMethod,&pVm->sAllocator,0,0);` |
| 122835 |   34 | `	SyHashInit(&pClass->hAttr,&pVm->sAllocator,0,0);` |
| 122835 |   35 | `	SyHashInit(&pClass->hDerived,&pVm->sAllocator,0,0);` |
| 122835 |   36 | `	SySetInit(&pClass->aInterface,&pVm->sAllocator,sizeof(ph7_class *));` |
| 122835 |   37 | `	SySetInit(&pClass->aTrait,&pVm->sAllocator,sizeof(ph7_class *));` |
| 122835 |   38 | `	pClass->nLine = nLine;` |
|      - |   39 | `	/* All done */` |
| 122835 |   40 | `	return pClass;` |
|  61420 |   41 |  |
|      - |   42 | `/*` |
|      - |   43 | ` * Allocate and initialize a new class attribute.` |
|      - |   44 | ` * Return a pointer to the class attribute on success. NULL otherwise.` |
|      - |   45 | ` */` |
|  61494 |   46 | `PH7_PRIVATE ph7_class_attr * PH7_NewClassAttr(ph7_vm *pVm,const SyString *pName,sxu32 nLine,sxi32 iProtection,sxi32 iFlags)` |
|      5 |   47 |  |
|      - |   48 | `	ph7_class_attr *pAttr;` |
|      - |   49 | `	char *zName;` |
|  61499 |   50 | `	pAttr = (ph7_class_attr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_attr));` |
|  61499 |   51 | `	if( pAttr == 0 ){` |
|    ! 0 |   52 | `		return 0;` |
|      - |   53 | `	}` |
|      - |   54 | `	/* Zero the structure */` |
|  61499 |   55 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|      - |   56 | `	/* Duplicate attribute name */` |
|  61499 |   57 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  61499 |   58 | `	if( zName == 0 ){` |
|    ! 0 |   59 | `		SyMemBackendPoolFree(&pVm->sAllocator,pAttr);` |
|    ! 0 |   60 | `		return 0;` |
|      - |   61 | `	}` |
|      - |   62 | `	/* Initialize fields */` |
|  61499 |   63 | `	SySetInit(&pAttr->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|  61499 |   64 | `	SySetInit(&pAttr->aUnionAlts,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|  61499 |   65 | `	SyStringInitFromBuf(&pAttr->sName,zName,pName->nByte);` |
|  61499 |   66 | `	pAttr->iProtection = iProtection;` |
|  61499 |   67 | `	pAttr->nIdx = SXU32_HIGH;` |
|  61499 |   68 | `	pAttr->iFlags = iFlags;` |
|  61499 |   69 | `	pAttr->nLine = nLine;` |
|  61499 |   70 | `	return pAttr;` |
|  30752 |   71 |  |
|      - |   72 | `/*` |
|      - |   73 | ` * Allocate and initialize a new class method.` |
|      - |   74 | ` * Return a pointer to the class method on success. NULL otherwise` |
|      - |   75 | ` * This function associate with the newly created method an automatically generated` |
|      - |   76 | ` * random unique name.` |
|      - |   77 | ` */` |
| 241172 |   78 | `PH7_PRIVATE ph7_class_method * PH7_NewClassMethod(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,sxu32 nLine,` |
|      - |   79 | `	sxi32 iProtection,sxi32 iFlags,sxi32 iFuncFlags)` |
|      5 |   80 |  |
|      - |   81 | `	ph7_class_method *pMeth;` |
|      - |   82 | `	SyHashEntry *pEntry;` |
|      - |   83 | `	SyString *pNamePtr;` |
|      - |   84 | `	char zSalt[10];` |
|      - |   85 | `	char *zName;` |
|      - |   86 | `	sxu32 nByte;` |
|      - |   87 | `	/* Allocate a new class method instance */` |
| 241177 |   88 | `	pMeth = (ph7_class_method *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_method));` |
| 241177 |   89 | `	if( pMeth == 0 ){` |
|    ! 0 |   90 | `		return 0;` |
|      - |   91 | `	}` |
|      - |   92 | `	/* Zero the structure */` |
| 241177 |   93 | `	SyZero(pMeth,sizeof(ph7_class_method));` |
|      - |   94 | `	/* Check for an already installed method with the same name */` |
| 241177 |   95 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
| 241177 |   96 | `	if( pEntry == 0 ){` |
|      - |   97 | `		/* Associate an unique VM name to this method */` |
| 241175 |   98 | `		nByte = sizeof(zSalt) + pName->nByte + SyStringLength(&pClass->sName)+sizeof(char)*7/*[[__'\0'*/;` |
| 241175 |   99 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,nByte);` |
| 241175 |  100 | `		if( zName == 0 ){` |
|    ! 0 |  101 | `			SyMemBackendPoolFree(&pVm->sAllocator,pMeth);` |
|    ! 0 |  102 | `			return 0;` |
|      - |  103 | `		}` |
| 241175 |  104 | `		pNamePtr = &pMeth->sVmName;` |
|      - |  105 | `		/* Generate a random string */` |
| 241175 |  106 | `		PH7_VmRandomString(&(*pVm),zSalt,sizeof(zSalt));` |
| 241175 |  107 | `		pNamePtr->nByte = SyBufferFormat(zName,nByte,"[__%z@%z_%.*s]",&pClass->sName,pName,sizeof(zSalt),zSalt);` |
| 241175 |  108 | `		pNamePtr->zString = zName;` |
| 120590 |  109 | `	}else{` |
|      - |  110 | `		/* Method is condidate for 'overloading' */` |
|      3 |  111 | `		ph7_class_method *pCurrent = (ph7_class_method *)pEntry->pUserData;` |
|      3 |  112 | `		pNamePtr = &pMeth->sVmName;` |
|      - |  113 | `		/* Use the same VM name */` |
|      3 |  114 | `		SyStringDupPtr(pNamePtr,&pCurrent->sVmName);` |
|      3 |  115 | `		zName = (char *)pNamePtr->zString;` |
|      - |  116 | `	}` |
| 241177 |  117 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|     32 |  118 | `		if( (pName->nByte == sizeof("__construct") - 1 && SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1 ) == 0)` |
|     21 |  119 | `			\|\| (pName->nByte == sizeof("__destruct") - 1 && SyMemcmp(pName->zString,"__destruct",sizeof("__destruct") - 1 ) == 0)` |
|     22 |  120 | `			\|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|      - |  121 | `				/* Switch to public visibility when dealing with constructor/destructor */` |
|      5 |  122 | `				iProtection = PH7_CLASS_PROT_PUBLIC;` |
|      2 |  123 | `		}` |
|     12 |  124 | `	}` |
|      - |  125 | `	/* Initialize method fields */` |
| 241179 |  126 | `	pMeth->iProtection = iProtection;` |
| 241179 |  127 | `	pMeth->iFlags = iFlags;` |
| 241179 |  128 | `	pMeth->nLine = nLine;` |
| 361767 |  129 | `	PH7_VmInitFuncState(&(*pVm),&pMeth->sFunc,&zName[sizeof(char)*4/*[__@*/+SyStringLength(&pClass->sName)],` |
| 241174 |  130 | `		pName->nByte,iFuncFlags\|VM_FUNC_CLASS_METHOD,pClass);` |
| 241179 |  131 | `	return pMeth;` |
| 120593 |  132 |  |
|      - |  133 | `/*` |
|      - |  134 | ` * Check if the given name have a class method associated with it.` |
|      - |  135 | ` * Return the desired method [i.e: ph7_class_method instance] on success. NULL otherwise.` |
|      - |  136 | ` */` |
| 155766 |  137 | `PH7_PRIVATE ph7_class_method * PH7_ClassExtractMethod(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|      5 |  138 |  |
|      - |  139 | `	SyHashEntry *pEntry;` |
|      - |  140 | `	/* Perform a hash lookup */` |
| 155771 |  141 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)zName,nByte);` |
| 155771 |  142 | `	if( pEntry == 0 ){` |
|      - |  143 | `		/* No such entry */` |
|   3557 |  144 | `		return 0;` |
|      - |  145 | `	}` |
|      - |  146 | `	/* Point to the desired method */` |
| 152219 |  147 | `	return (ph7_class_method *)pEntry->pUserData;` |
|  77888 |  148 |  |
|      - |  149 | `/*` |
|      - |  150 | ` * Check if the given name is a class attribute.` |
|      - |  151 | ` * Return the desired attribute [i.e: ph7_class_attr instance] on success.NULL otherwise.` |
|      - |  152 | ` */` |
|  61564 |  153 | `PH7_PRIVATE ph7_class_attr * PH7_ClassExtractAttribute(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|      5 |  154 |  |
|      - |  155 | `	SyHashEntry *pEntry;` |
|      - |  156 | `	/* Perform a hash lookup */` |
|  61569 |  157 | `	pEntry = SyHashGet(&pClass->hAttr,(const void *)zName,nByte);` |
|  61569 |  158 | `	if( pEntry == 0 ){` |
|      - |  159 | `		/* No such entry */` |
|  61423 |  160 | `		return 0;` |
|      - |  161 | `	}` |
|      - |  162 | `	/* Point to the desierd method */` |
|    151 |  163 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|  30787 |  164 |  |
|      - |  165 | `/*` |
|      - |  166 | ` * Install a class attribute in the corresponding container.` |
|      - |  167 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|      - |  168 | ` */` |
|  61494 |  169 | `PH7_PRIVATE sxi32 PH7_ClassInstallAttr(ph7_class *pClass,ph7_class_attr *pAttr)` |
|      5 |  170 |  |
|  61499 |  171 | `	SyString *pName = &pAttr->sName;` |
|      - |  172 | `	sxi32 rc;` |
|      - |  173 | `	/* Remember where this attribute was originally declared so that later` |
|      - |  174 | `	 * inheritance/trait copies still know the declaring class (needed for` |
|      - |  175 | `	 * PHP-compatible error messages on typed properties). */` |
|  61499 |  176 | `	if( pAttr->pDeclClass == 0 ){` |
|  61499 |  177 | `		pAttr->pDeclClass = pClass;` |
|  30747 |  178 | `	}` |
|  61499 |  179 | `	rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|  61499 |  180 | `	return rc;` |
|      5 |  181 |  |
|      - |  182 | `/*` |
|      - |  183 | ` * Install a class method in the corresponding container.` |
|      - |  184 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|      - |  185 | ` */` |
| 241162 |  186 | `PH7_PRIVATE sxi32 PH7_ClassInstallMethod(ph7_class *pClass,ph7_class_method *pMeth)` |
|      5 |  187 |  |
| 241167 |  188 | `	SyString *pName = &pMeth->sFunc.sName;` |
|      - |  189 | `	sxi32 rc;` |
| 241167 |  190 | `	rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 241167 |  191 | `	return rc;` |
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
|  67428 |  234 | `PH7_PRIVATE sxi32 PH7_ClassInherit(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase)` |
|      5 |  235 |  |
|      - |  236 | `	ph7_class_method *pMeth;` |
|      - |  237 | `	ph7_class_attr *pAttr;` |
|      - |  238 | `	SyHashEntry *pEntry;` |
|      - |  239 | `	SyString *pName;` |
|      - |  240 | `	sxi32 rc;` |
|      - |  241 | `	/* Install in the derived hashtable */` |
|  67433 |  242 | `	rc = SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|  67433 |  243 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  244 | `		return rc;` |
|      - |  245 | `	}` |
|      - |  246 | `	/* Copy public/protected attributes from the base class */` |
|  67433 |  247 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
| 471509 |  248 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|      - |  249 | `		/* Make sure the private attributes are not redeclared in the subclass */` |
| 404081 |  250 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
| 404081 |  251 | `		pName = &pAttr->sName;` |
| 404081 |  252 | `		if( (pEntry = SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|      9 |  253 | `			if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_FINAL))` |
|      6 |  254 | `				== (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_FINAL) ){` |
|      - |  255 | `				/* Cannot override a final class constant (PHP 8.1). Report the` |
|      - |  256 | `				 * class that originally declared it (pDeclClass) rather than the` |
|      - |  257 | `				 * immediate base, so a multi-level chain matches PHP. */` |
|      3 |  258 | `				ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pBase;` |
|      4 |  259 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_attr *)pEntry->pUserData)->nLine,` |
|      - |  260 | `					"%z::%z cannot override final constant %z::%z",` |
|      1 |  261 | `					&pSub->sName,pName,&pOwner->sName,pName);` |
|      3 |  262 | `				if( rc == SXERR_ABORT ){` |
|    ! 0 |  263 | `					return SXERR_ABORT;` |
|      - |  264 | `				}` |
|      1 |  265 | `			}` |
|      9 |  266 | `			if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE &&` |
|      2 |  267 | `				((ph7_class_attr *)pEntry->pUserData)->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|      - |  268 | `					/* Cannot redeclare private attribute */` |
|      4 |  269 | `					PH7_GenCompileError(&(*pGen),E_WARNING,((ph7_class_attr *)pEntry->pUserData)->nLine,` |
|      - |  270 | `						"Private attribute '%z::%z' redeclared inside child class '%z'",` |
|      1 |  271 | `						&pBase->sName,pName,&pSub->sName);` |
|      - |  272 |  |
|      1 |  273 | `			}` |
|      9 |  274 | `			continue;` |
|      - |  275 | `		}` |
|      - |  276 | `		/* Install the attribute */` |
| 404075 |  277 | `		if( pAttr->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
| 404071 |  278 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
| 404071 |  279 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  280 | `				return rc;` |
|      - |  281 | `			}` |
| 202033 |  282 | `		}` |
|      5 |  283 | `	}` |
|  67433 |  284 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
| 673613 |  285 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|      - |  286 | `		/* Make sure the private/final methods are not redeclared in the subclass */` |
| 606185 |  287 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
| 606185 |  288 | `		pName = &pMeth->sFunc.sName;` |
| 606185 |  289 | `		if( (pEntry = SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|   3241 |  290 | `			 if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|      - |  291 | `				/* Cannot Overwrite final method */` |
|      8 |  292 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_method *)pEntry->pUserData)->nLine,` |
|      - |  293 | `					"Cannot Overwrite final method '%z:%z' inside child class '%z'",` |
|      2 |  294 | `					&pBase->sName,pName,&pSub->sName);` |
|      6 |  295 | `				if( rc == SXERR_ABORT ){` |
|    ! 0 |  296 | `					return SXERR_ABORT;` |
|      - |  297 | `				}` |
|      2 |  298 | `			}` |
|   3241 |  299 | `			continue;` |
|      - |  300 | `		}` |
|      - |  301 | `		/* Install the method */` |
| 602949 |  302 | `		if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
| 602947 |  303 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 602947 |  304 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  305 | `				return rc;` |
|      - |  306 | `			}` |
| 301471 |  307 | `		}` |
|      5 |  308 | `	}` |
|      - |  309 | `	/* Mark as subclass */` |
|  67433 |  310 | `	pSub->pBase = pBase;` |
|      - |  311 | `	/* All done */` |
|  67433 |  312 | `	return SXRET_OK;` |
|  33719 |  313 |  |
|      - |  314 | `/*` |
|      - |  315 | ` * Apply a trait to a class: copy all methods and attributes from the trait` |
|      - |  316 | ` * into the target class. Unlike inheritance, traits copy ALL members including` |
|      - |  317 | ` * private ones. Members already defined in the class take precedence.` |
|      - |  318 | ` */` |
|     44 |  319 | `PH7_PRIVATE sxi32 PH7_ClassUseTrait(ph7_gen_state *pGen,ph7_class *pClass,ph7_class *pTrait)` |
|      5 |  320 |  |
|      - |  321 | `	ph7_class_method *pMeth;` |
|      - |  322 | `	ph7_class_attr *pAttr;` |
|      - |  323 | `	SyHashEntry *pEntry;` |
|      - |  324 | `	SyString *pName;` |
|      - |  325 | `	sxi32 rc;` |
|      - |  326 | `	/* Detect cyclic trait composition (e.g. trait A { use B; } trait B { use A; }) */` |
|     49 |  327 | `	if( pTrait->iFlags & PH7_CLASS_TRAIT_VISITING ){` |
|    ! 0 |  328 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|    ! 0 |  329 | `			"Trait circular reference detected: %z is already being applied",&pTrait->sName);` |
|    ! 0 |  330 | `		if( rc == SXERR_ABORT ){` |
|    ! 0 |  331 | `			return SXERR_ABORT;` |
|      - |  332 | `		}` |
|    ! 0 |  333 | `		return SXRET_OK;` |
|      - |  334 | `	}` |
|     49 |  335 | `	pTrait->iFlags \|= PH7_CLASS_TRAIT_VISITING;` |
|     49 |  336 | `	rc = SXRET_OK;` |
|      - |  337 | `	/* Copy attributes from the trait */` |
|     49 |  338 | `	SyHashResetLoopCursor(&pTrait->hAttr);` |
|     65 |  339 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hAttr)) != 0 ){` |
|      - |  340 | `		SyHashEntry *pExisting;` |
|     20 |  341 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     20 |  342 | `		pName = &pAttr->sName;` |
|     20 |  343 | `		pExisting = SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte);` |
|     20 |  344 | `		if( pExisting != 0 ){` |
|      - |  345 | `			/* Attribute already exists. Check if it came from another trait` |
|      - |  346 | `			 * and whether the definitions are compatible (same defaults).` |
|      - |  347 | `			 */` |
|      - |  348 | `			ph7_class **apUsedTraits;` |
|      - |  349 | `			sxu32 nUsed,k;` |
|      6 |  350 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|      6 |  351 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|      6 |  352 | `			for(k = 0; k < nUsed; k++){` |
|      - |  353 | `				ph7_class_attr *pOther;` |
|      3 |  354 | `				pOther = PH7_ClassExtractAttribute(apUsedTraits[k],pName->zString,pName->nByte);` |
|      3 |  355 | `				if( pOther ){` |
|      - |  356 | `					/* Two traits define the same property — check if defaults differ */` |
|      3 |  357 | `					ph7_class_attr *pClassAttr = (ph7_class_attr *)pExisting->pUserData;` |
|      4 |  358 | `					if( SySetUsed(&pAttr->aByteCode) != SySetUsed(&pClassAttr->aByteCode) \|\|` |
|      3 |  359 | `						(SySetUsed(&pAttr->aByteCode) > 0 &&` |
|      3 |  360 | `						 SyMemcmp(SySetBasePtr(&pAttr->aByteCode),SySetBasePtr(&pClassAttr->aByteCode),` |
|      3 |  361 | `							SySetUsed(&pAttr->aByteCode) * SySetElemSize(&pAttr->aByteCode)) != 0) ){` |
|      4 |  362 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine,` |
|      - |  363 | `							"%z and %z define the same property ($%z) in the composition of %z. "` |
|      - |  364 | `							"However, the definition differs and is considered incompatible",` |
|      2 |  365 | `							&apUsedTraits[k]->sName,&pTrait->sName,pName,&pClass->sName);` |
|      3 |  366 | `						if( rc == SXERR_ABORT ){` |
|    ! 0 |  367 | `							goto cleanup;` |
|      - |  368 | `						}` |
|      1 |  369 | `					}` |
|      3 |  370 | `					break;` |
|      - |  371 | `				}` |
|    ! 0 |  372 | `			}` |
|      6 |  373 | `			continue;` |
|      - |  374 | `		}` |
|     16 |  375 | `		rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|     16 |  376 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  377 | `			goto cleanup;` |
|      - |  378 | `		}` |
|      4 |  379 | `	}` |
|      - |  380 | `	/* Copy methods from the trait */` |
|     49 |  381 | `	SyHashResetLoopCursor(&pTrait->hMethod);` |
|     89 |  382 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hMethod)) != 0 ){` |
|     44 |  383 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|     44 |  384 | `		pName = &pMeth->sFunc.sName;` |
|     44 |  385 | `		if( SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte) != 0 ){` |
|      - |  386 | `			/* Method already exists in the class. Check if it came from another trait` |
|      - |  387 | `			 * (unresolved conflict) vs being defined by the class itself.` |
|      - |  388 | `			 */` |
|      - |  389 | `			ph7_class **apUsedTraits;` |
|      - |  390 | `			sxu32 nUsed,k;` |
|     11 |  391 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|     11 |  392 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|     11 |  393 | `			for(k = 0; k < nUsed; k++){` |
|      3 |  394 | `				if( PH7_ClassExtractMethod(apUsedTraits[k],pName->zString,pName->nByte) != 0 ){` |
|      - |  395 | `					/* Two different traits define the same method with no resolution */` |
|      4 |  396 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|      - |  397 | `						"Trait method %z::%z has not been applied as %z::%z, "` |
|      - |  398 | `						"because of collision with %z::%z",` |
|      2 |  399 | `						&pTrait->sName,pName,` |
|      1 |  400 | `						&pClass->sName,pName,` |
|      2 |  401 | `						&apUsedTraits[k]->sName,pName);` |
|      3 |  402 | `					if( rc == SXERR_ABORT ){` |
|    ! 0 |  403 | `						goto cleanup;` |
|      - |  404 | `					}` |
|      3 |  405 | `					break;` |
|      - |  406 | `				}` |
|    ! 0 |  407 | `			}` |
|      - |  408 | `			/* Class-defined method takes precedence */` |
|     11 |  409 | `			continue;` |
|      - |  410 | `		}` |
|     36 |  411 | `		rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|     36 |  412 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  413 | `			goto cleanup;` |
|      - |  414 | `		}` |
|      4 |  415 | `	}` |
|      - |  416 | `	/* Record trait in the class */` |
|     49 |  417 | `	SySetPut(&pClass->aTrait,(const void *)&pTrait);` |
|     22 |  418 | `cleanup:` |
|      - |  419 | `	/* Always clear visiting flag, even on error paths */` |
|     49 |  420 | `	pTrait->iFlags &= ~PH7_CLASS_TRAIT_VISITING;` |
|     22 |  421 | `	SXUNUSED(pGen);` |
|     49 |  422 | `	return rc;` |
|     27 |  423 |  |
|      - |  424 | `/*` |
|      - |  425 | ` * Inherit an object interface from another object interface.` |
|      - |  426 | ` * According to the PHP language reference manual.` |
|      - |  427 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|      - |  428 | ` *  must implement, without having to define how these methods are handled.` |
|      - |  429 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|      - |  430 | ` *  class, but without any of the methods having their contents defined.` |
|      - |  431 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|      - |  432 | ` *` |
|      - |  433 | ` * This function return SXRET_OK if the interface inheritance operation was successfully performed.` |
|      - |  434 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|      - |  435 | ` * error message.` |
|      - |  436 | ` */` |
|   9620 |  437 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceInherit(ph7_class *pSub,ph7_class *pBase)` |
|      5 |  438 |  |
|      - |  439 | `	ph7_class_method *pMeth;` |
|      - |  440 | `	ph7_class_attr *pAttr;` |
|      - |  441 | `	SyHashEntry *pEntry;` |
|      - |  442 | `	SyString *pName;` |
|      - |  443 | `	sxi32 rc;` |
|      - |  444 | `	/* Install in the derived hashtable */` |
|   9625 |  445 | `	SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|   9625 |  446 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
|      - |  447 | `	/* Copy constants */` |
|  14437 |  448 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|      - |  449 | `		/* Make sure the constants are not redeclared in the subclass */` |
|      3 |  450 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      3 |  451 | `		pName = &pAttr->sName;` |
|      3 |  452 | `		if( SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte) == 0 ){` |
|      - |  453 | `			/* Install the constant in the subclass */` |
|      3 |  454 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|      3 |  455 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  456 | `				return rc;` |
|      - |  457 | `			}` |
|      1 |  458 | `		}` |
|      1 |  459 | `	}` |
|   9625 |  460 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
|      - |  461 | `	/* Copy methods signature */` |
|  17681 |  462 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|      - |  463 | `		/* Make sure the method are not redeclared in the subclass */` |
|   3251 |  464 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   3251 |  465 | `		pName = &pMeth->sFunc.sName;` |
|   3251 |  466 | `		if( SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte) == 0 ){` |
|      - |  467 | `			/* Install the method */` |
|   3251 |  468 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|   3251 |  469 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  470 | `				return rc;` |
|      - |  471 | `			}` |
|   1623 |  472 | `		}` |
|      5 |  473 | `	}` |
|      - |  474 | `	/* Mark as subclass */` |
|   9625 |  475 | `	pSub->pBase = pBase;` |
|      - |  476 | `	/* All done */` |
|   9625 |  477 | `	return SXRET_OK;` |
|   4815 |  478 |  |
|      - |  479 | `/*` |
|      - |  480 | ` * Implements an object interface in the given main class.` |
|      - |  481 | ` * According to the PHP language reference manual.` |
|      - |  482 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|      - |  483 | ` *  must implement, without having to define how these methods are handled.` |
|      - |  484 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|      - |  485 | ` *  class, but without any of the methods having their contents defined.` |
|      - |  486 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|      - |  487 | ` *` |
|      - |  488 | ` * This function return SXRET_OK if the interface was successfully implemented.` |
|      - |  489 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|      - |  490 | ` * error message.` |
|      - |  491 | ` */` |
|  86704 |  492 | `PH7_PRIVATE sxi32 PH7_ClassImplement(ph7_class *pMain,ph7_class *pInterface)` |
|      5 |  493 |  |
|      - |  494 | `	ph7_class_attr *pAttr;` |
|      - |  495 | `	SyHashEntry *pEntry;` |
|      - |  496 | `	SyString *pName;` |
|      - |  497 | `	sxi32 rc;` |
|      - |  498 | `	/* First off,copy all constants declared inside the interface */` |
|  86709 |  499 | `	SyHashResetLoopCursor(&pInterface->hAttr);` |
| 130067 |  500 | `	while((pEntry = SyHashGetNextEntry(&pInterface->hAttr)) != 0 ){` |
|      - |  501 | `		/* Point to the constant declaration */` |
|      7 |  502 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      7 |  503 | `		pName = &pAttr->sName;` |
|      - |  504 | `		/* Make sure the attribute is not redeclared in the main class */` |
|      7 |  505 | `		if( SyHashGet(&pMain->hAttr,pName->zString,pName->nByte) == 0 ){` |
|      - |  506 | `			/* Install the attribute */` |
|      7 |  507 | `			rc = SyHashInsert(&pMain->hAttr,pName->zString,pName->nByte,pAttr);` |
|      7 |  508 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  509 | `				return rc;` |
|      - |  510 | `			}` |
|      3 |  511 | `		}` |
|      1 |  512 | `	}` |
|      - |  513 | `	/* Install in the interface container */` |
|  86709 |  514 | `	SySetPut(&pMain->aInterface,(const void *)&pInterface);` |
|      - |  515 | `	/* Install interface method stubs into the implementing class.` |
|      - |  516 | `	 * Methods already defined in the class take precedence (they satisfy` |
|      - |  517 | `	 * the interface contract). Stubs retain PH7_CLASS_ATTR_ABSTRACT so` |
|      - |  518 | `	 * the unified check in GenStateCheckAbstractMethods catches missing ones.` |
|      - |  519 | `	 */` |
|      - |  520 | `	{` |
|      - |  521 | `		ph7_class_method *pMeth;` |
|      - |  522 | `		SyHashEntry *pMEntry;` |
|      - |  523 | `		SyString *pMName;` |
|  86709 |  524 | `		SyHashResetLoopCursor(&pInterface->hMethod);` |
| 274643 |  525 | `		while((pMEntry = SyHashGetNextEntry(&pInterface->hMethod)) != 0 ){` |
| 144587 |  526 | `			pMeth = (ph7_class_method *)pMEntry->pUserData;` |
| 144587 |  527 | `			pMName = &pMeth->sFunc.sName;` |
| 144587 |  528 | `			if( SyHashGet(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte) == 0 ){` |
|     18 |  529 | `				rc = SyHashInsert(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte,pMeth);` |
|     18 |  530 | `				if( rc != SXRET_OK ){` |
|    ! 0 |  531 | `					return rc;` |
|      - |  532 | `				}` |
|      7 |  533 | `			}` |
|      5 |  534 | `		}` |
|      - |  535 | `	}` |
|  86709 |  536 | `	return SXRET_OK;` |
|  43357 |  537 |  |
|      - |  538 | `/*` |
|      - |  539 | ` * Create a class instance [i.e: Object in the PHP jargon] at run-time.` |
|      - |  540 | ` * The following function is called when an object is created at run-time` |
|      - |  541 | ` * typically when the PH7_OP_NEW/PH7_OP_CLONE instructions are executed.` |
|      - |  542 | ` * Notes on object creation.` |
|      - |  543 | ` *` |
|      - |  544 | ` * According to PHP language reference manual.` |
|      - |  545 | ` * To create an instance of a class, the new keyword must be used. An object will always` |
|      - |  546 | ` * be created unless the object has a constructor defined that throws an exception on error.` |
|      - |  547 | ` * Classes should be defined before instantiation (and in some cases this is a requirement).` |
|      - |  548 | ` * If a string containing the name of a class is used with new, a new instance of that class` |
|      - |  549 | ` * will be created. If the class is in a namespace, its fully qualified name must be used when` |
|      - |  550 | ` * doing this.` |
|      - |  551 | ` * Example #3 Creating an instance` |
|      - |  552 | ` * <?php` |
|      - |  553 | ` *  $instance = new SimpleClass();` |
|      - |  554 | ` *   // This can also be done with a variable:` |
|      - |  555 | ` * $className = 'Foo';` |
|      - |  556 | ` * $instance = new $className(); // Foo()` |
|      - |  557 | ` * ?>` |
|      - |  558 | ` * In the class context, it is possible to create a new object by new self and new parent.` |
|      - |  559 | ` * When assigning an already created instance of a class to a new variable, the new variable` |
|      - |  560 | ` * will access the same instance as the object that was assigned. This behaviour is the same` |
|      - |  561 | ` * when passing instances to a function. A copy of an already created object can be made by` |
|      - |  562 | ` * cloning it.` |
|      - |  563 | ` * Example #4 Object Assignment` |
|      - |  564 | ` * <?php` |
|      - |  565 | ` *  class SimpleClass(){` |
|      - |  566 | ` *    public $var;` |
|      - |  567 | ` *  };` |
|      - |  568 | ` *  $instance = new SimpleClass();` |
|      - |  569 | ` *  $assigned   =  $instance;` |
|      - |  570 | ` *  $reference  =& $instance;` |
|      - |  571 | ` *  $instance->var = '$assigned will have this value';` |
|      - |  572 | ` *  $instance = null; // $instance and $reference become null` |
|      - |  573 | ` *  var_dump($instance);` |
|      - |  574 | ` *  var_dump($reference);` |
|      - |  575 | ` *  var_dump($assigned);` |
|      - |  576 | ` * ?>` |
|      - |  577 | ` * The above example will output:` |
|      - |  578 | ` * NULL` |
|      - |  579 | ` * NULL` |
|      - |  580 | ` * object(SimpleClass)#1 (1) {` |
|      - |  581 | ` *  ["var"]=>` |
|      - |  582 | ` *    string(30) "$assigned will have this value"` |
|      - |  583 | ` * }` |
|      - |  584 | ` * Example #5 Creating new objects` |
|      - |  585 | ` * <?php` |
|      - |  586 | ` * class Test` |
|      - |  587 | ` * {` |
|      - |  588 | ` *   static public function getNew()` |
|      - |  589 | ` *   {` |
|      - |  590 | ` *       return new static;` |
|      - |  591 | ` *   }` |
|      - |  592 | ` * }` |
|      - |  593 | ` * class Child extends Test` |
|      - |  594 | ` * {}` |
|      - |  595 | ` * $obj1 = new Test();` |
|      - |  596 | ` * $obj2 = new $obj1;` |
|      - |  597 | ` * var_dump($obj1 !== $obj2);` |
|      - |  598 | ` * $obj3 = Test::getNew();` |
|      - |  599 | ` * var_dump($obj3 instanceof Test);` |
|      - |  600 | ` * $obj4 = Child::getNew();` |
|      - |  601 | ` * var_dump($obj4 instanceof Child);` |
|      - |  602 | ` * ?>` |
|      - |  603 | ` * The above example will output:` |
|      - |  604 | ` * bool(true)` |
|      - |  605 | ` * bool(true)` |
|      - |  606 | ` * bool(true)` |
|      - |  607 | ` * Note that Symisc Systems have introduced powerfull extension to` |
|      - |  608 | ` * OO subsystem. For example a class attribute may have any complex` |
|      - |  609 | ` * expression associated with it when declaring the attribute unlike` |
|      - |  610 | ` * the standard PHP engine which would allow a single value.` |
|      - |  611 | ` * Example:` |
|      - |  612 | ` *  class myClass{` |
|      - |  613 | ` *    public $var = 25<<1+foo()/bar();` |
|      - |  614 | ` *  };` |
|      - |  615 | ` * Refer to the official documentation for more information.` |
|      - |  616 | ` */` |
|   2218 |  617 | `static ph7_class_instance * NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|      5 |  618 |  |
|      - |  619 | `	ph7_class_instance *pThis;` |
|      - |  620 | `	/* Allocate a new instance */` |
|   2223 |  621 | `	pThis = (ph7_class_instance *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_instance));` |
|   2223 |  622 | `	if( pThis == 0 ){` |
|    ! 0 |  623 | `		return 0;` |
|      - |  624 | `	}` |
|      - |  625 | `	/* Zero the structure */` |
|   2223 |  626 | `	SyZero(pThis,sizeof(ph7_class_instance));` |
|      - |  627 | `	/* Initialize fields */` |
|   2223 |  628 | `	pThis->iRef = 1;` |
|   2223 |  629 | `	pThis->pVm = pVm;` |
|   2223 |  630 | `	pThis->pClass = pClass;` |
|   2223 |  631 | `	SyHashInit(&pThis->hAttr,&pVm->sAllocator,0,0);` |
|   2223 |  632 | `	return pThis;` |
|   1114 |  633 |  |
|      - |  634 | `/*` |
|      - |  635 | ` * Wrapper around the NewClassInstance() function defined above.` |
|      - |  636 | ` * See the block comment above for more information.` |
|      - |  637 | ` */` |
|   2174 |  638 | `PH7_PRIVATE ph7_class_instance * PH7_NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|      5 |  639 |  |
|      - |  640 | `	ph7_class_instance *pNew;` |
|      - |  641 | `	sxi32 rc;` |
|   2179 |  642 | `	pNew = NewClassInstance(&(*pVm),&(*pClass));` |
|   2179 |  643 | `	if( pNew == 0 ){` |
|    ! 0 |  644 | `		return 0;` |
|      - |  645 | `	}` |
|      - |  646 | `	/* Associate a private VM frame with this class instance */` |
|   2179 |  647 | `	rc = PH7_VmCreateClassInstanceFrame(&(*pVm),pNew);` |
|   2179 |  648 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  649 | `		SyMemBackendPoolFree(&pVm->sAllocator,pNew);` |
|    ! 0 |  650 | `		return 0;` |
|      - |  651 | `	}` |
|   2179 |  652 | `	return pNew;` |
|   1092 |  653 |  |
|      - |  654 | `/*` |
|      - |  655 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon] attribute.` |
|      - |  656 | ` * This function never fail.` |
|      - |  657 | ` */` |
|   1270 |  658 | `static ph7_value * ExtractClassAttrValue(ph7_vm *pVm,VmClassAttr *pAttr)` |
|      5 |  659 |  |
|      - |  660 | `	/* Extract the value */` |
|      - |  661 | `	ph7_value *pValue;` |
|   1275 |  662 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|   1275 |  663 | `	return pValue;` |
|      5 |  664 |  |
|      - |  665 | `/*` |
|      - |  666 | ` * Perform a clone operation on a class instance [i.e: Object in the PHP jargon].` |
|      - |  667 | ` * The following function is called when an object is cloned at run-time` |
|      - |  668 | ` * typically when the PH7_OP_CLONE instruction is executed.` |
|      - |  669 | ` * Notes on object cloning.` |
|      - |  670 | ` *` |
|      - |  671 | ` * According to PHP language reference manual.` |
|      - |  672 | ` * Creating a copy of an object with fully replicated properties is not always the wanted behavior.` |
|      - |  673 | ` * A good example of the need for copy constructors. Another example is if your object holds a reference` |
|      - |  674 | ` * to another object which it uses and when you replicate the parent object you want to create` |
|      - |  675 | ` * a new instance of this other object so that the replica has its own separate copy.` |
|      - |  676 | ` * An object copy is created by using the clone keyword (which calls the object's __clone() method if possible).` |
|      - |  677 | ` * An object's __clone() method cannot be called directly.` |
|      - |  678 | ` * $copy_of_object = clone $object;` |
|      - |  679 | ` * When an object is cloned, PHP 5 will perform a shallow copy of all of the object's properties.` |
|      - |  680 | ` * Any properties that are references to other variables, will remain references.` |
|      - |  681 | ` * Once the cloning is complete, if a __clone() method is defined, then the newly created object's __clone() method` |
|      - |  682 | ` * will be called, to allow any necessary properties that need to be changed.` |
|      - |  683 | ` * Example #1 Cloning an object` |
|      - |  684 | ` * <?php` |
|      - |  685 | ` * class SubObject` |
|      - |  686 | ` * {` |
|      - |  687 | ` *   static $instances = 0;` |
|      - |  688 | ` *   public $instance;` |
|      - |  689 | ` *` |
|      - |  690 | ` *   public function __construct() {` |
|      - |  691 | ` *       $this->instance = ++self::$instances;` |
|      - |  692 | ` *   }` |
|      - |  693 | ` *` |
|      - |  694 | ` *   public function __clone() {` |
|      - |  695 | ` *       $this->instance = ++self::$instances;` |
|      - |  696 | ` *   }` |
|      - |  697 | ` * }` |
|      - |  698 | ` *` |
|      - |  699 | ` * class MyCloneable` |
|      - |  700 | ` * {` |
|      - |  701 | ` *   public $object1;` |
|      - |  702 | ` *   public $object2;` |
|      - |  703 | ` *` |
|      - |  704 | ` *   function __clone()` |
|      - |  705 | ` *   {` |
|      - |  706 | ` *       // Force a copy of this->object, otherwise` |
|      - |  707 | ` *       // it will point to same object.` |
|      - |  708 | ` *       $this->object1 = clone $this->object1;` |
|      - |  709 | ` *   }` |
|      - |  710 | ` * }` |
|      - |  711 | ` * $obj = new MyCloneable();` |
|      - |  712 | ` * $obj->object1 = new SubObject();` |
|      - |  713 | ` * $obj->object2 = new SubObject();` |
|      - |  714 | ` * $obj2 = clone $obj;` |
|      - |  715 | ` * print("Original Object:\n");` |
|      - |  716 | ` * print_r($obj);` |
|      - |  717 | ` * print("Cloned Object:\n");` |
|      - |  718 | ` * print_r($obj2);` |
|      - |  719 | ` * ?>` |
|      - |  720 | ` * The above example will output:` |
|      - |  721 | ` * Original Object:` |
|      - |  722 | ` * MyCloneable Object` |
|      - |  723 | ` * (` |
|      - |  724 | ` *   [object1] => SubObject Object` |
|      - |  725 | ` *       (` |
|      - |  726 | ` *           [instance] => 1` |
|      - |  727 | ` *       )` |
|      - |  728 | ` *` |
|      - |  729 | ` *   [object2] => SubObject Object` |
|      - |  730 | ` *       (` |
|      - |  731 | ` *           [instance] => 2` |
|      - |  732 | ` *       )` |
|      - |  733 | ` *` |
|      - |  734 | ` * )` |
|      - |  735 | ` * Cloned Object:` |
|      - |  736 | ` * MyCloneable Object` |
|      - |  737 | ` * (` |
|      - |  738 | ` *   [object1] => SubObject Object` |
|      - |  739 | ` *       (` |
|      - |  740 | ` *           [instance] => 3` |
|      - |  741 | ` *       )` |
|      - |  742 | ` *` |
|      - |  743 | ` *   [object2] => SubObject Object` |
|      - |  744 | ` *       (` |
|      - |  745 | ` *           [instance] => 2` |
|      - |  746 | ` *       )` |
|      - |  747 | ` * )` |
|      - |  748 | ` */` |
|     44 |  749 | `PH7_PRIVATE ph7_class_instance * PH7_CloneClassInstance(ph7_class_instance *pSrc)` |
|      2 |  750 |  |
|      - |  751 | `	ph7_class_instance *pClone;` |
|      - |  752 | `	ph7_class_method *pMethod;` |
|      - |  753 | `	SyHashEntry *pEntry2;` |
|      - |  754 | `	SyHashEntry *pEntry;` |
|      - |  755 | `	ph7_vm *pVm;` |
|      - |  756 | `	sxi32 rc;` |
|      - |  757 | `	/* Allocate a new instance */` |
|     46 |  758 | `	pVm = pSrc->pVm;` |
|     46 |  759 | `	pClone = NewClassInstance(pVm,pSrc->pClass);` |
|     46 |  760 | `	if( pClone == 0 ){` |
|    ! 0 |  761 | `		return 0;` |
|      - |  762 | `	}` |
|      - |  763 | `	/* Associate a private VM frame with this class instance */` |
|     46 |  764 | `	rc = PH7_VmCreateClassInstanceFrame(pVm,pClone);` |
|     46 |  765 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  766 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClone);` |
|    ! 0 |  767 | `		return 0;` |
|      - |  768 | `	}` |
|      - |  769 | `	/* Duplicate object values */` |
|     46 |  770 | `	SyHashResetLoopCursor(&pSrc->hAttr);` |
|     46 |  771 | `	SyHashResetLoopCursor(&pClone->hAttr);` |
|    116 |  772 | `	while((pEntry = SyHashGetNextEntry(&pSrc->hAttr)) != 0 && (pEntry2 = SyHashGetNextEntry(&pClone->hAttr)) != 0 ){` |
|     50 |  773 | `		VmClassAttr *pSrcAttr = (VmClassAttr *)pEntry->pUserData;` |
|     50 |  774 | `		VmClassAttr *pDestAttr = (VmClassAttr *)pEntry2->pUserData;` |
|      - |  775 | `		/* Duplicate non-static attribute */` |
|     50 |  776 | `		if( (pSrcAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|      - |  777 | `			ph7_value *pvSrc,*pvDest;` |
|     50 |  778 | `			pvSrc = ExtractClassAttrValue(pVm,pSrcAttr);` |
|     50 |  779 | `			pvDest = ExtractClassAttrValue(pVm,pDestAttr);` |
|     50 |  780 | `			if( pvSrc && pvDest ){` |
|     50 |  781 | `				PH7_MemObjStore(pvSrc,pvDest);` |
|     24 |  782 | `			}` |
|     24 |  783 | `		}` |
|      2 |  784 | `	}` |
|      - |  785 | `	/* call the __clone method on the cloned object if available */` |
|     46 |  786 | `	pMethod = PH7_ClassExtractMethod(pClone->pClass,"__clone",sizeof("__clone")-1);` |
|     46 |  787 | `	if( pMethod ){` |
|     38 |  788 | `		if( pMethod->iCloneDepth < 16 ){` |
|     36 |  789 | `			pMethod->iCloneDepth++;` |
|     36 |  790 | `			PH7_VmCallClassMethod(pVm,pClone,pMethod,0,0,0);` |
|     19 |  791 | `		}else{` |
|      - |  792 | `			/* Nesting limit reached */` |
|      3 |  793 | `			PH7_VmThrowError(pVm,0,PH7_CTX_ERR,"Object clone limit reached,no more call to __clone()");` |
|      - |  794 | `		}` |
|      - |  795 | `		/* Reset the cursor */` |
|     38 |  796 | `		pMethod->iCloneDepth = 0;` |
|     18 |  797 | `	}` |
|      - |  798 | `	/* Return the cloned object */` |
|     46 |  799 | `	return pClone;` |
|     24 |  800 |  |
|      - |  801 | `#define CLASS_INSTANCE_DESTROYED 0x001 /* Instance is released */` |
|      - |  802 | `/*` |
|      - |  803 | ` * Release a class instance [i.e: Object in the PHP jargon] and invoke any defined destructor.` |
|      - |  804 | ` * This routine is invoked as soon as there are no other references to a particular` |
|      - |  805 | ` * class instance.` |
|      - |  806 | ` */` |
|   1628 |  807 | `static void PH7_ClassInstanceRelease(ph7_class_instance *pThis)` |
|      5 |  808 |  |
|      - |  809 | `	ph7_class_method *pDestr;` |
|      - |  810 | `	SyHashEntry *pEntry;` |
|      - |  811 | `	ph7_class *pClass;` |
|      - |  812 | `	ph7_vm *pVm;` |
|   1633 |  813 | `	if( pThis->iFlags & CLASS_INSTANCE_DESTROYED ){` |
|      - |  814 | `		/*` |
|      - |  815 | `		 * Already destroyed,return immediately.` |
|      - |  816 | `		 * This could happend if someone perform unset($this) in the destructor body.` |
|      - |  817 | `		 */` |
|      9 |  818 | `		return;` |
|      - |  819 | `	}` |
|      - |  820 | `	/* Mark as destroyed */` |
|   1625 |  821 | `	pThis->iFlags \|= CLASS_INSTANCE_DESTROYED;` |
|      - |  822 | `	/* Invoke any defined destructor if available */` |
|   1625 |  823 | `	pVm = pThis->pVm;` |
|   1625 |  824 | `	pClass = pThis->pClass;` |
|   1625 |  825 | `	pDestr = PH7_ClassExtractMethod(pClass,"__destruct",sizeof("__destruct")-1);` |
|   1625 |  826 | `	if( pDestr && !pVm->bInReset ){` |
|      - |  827 | `		/* Invoke the destructor. Skipped during ph7_vm_reset() bulk teardown:` |
|      - |  828 | `		 * running user PHP against a half-reset VM is unsafe (see bInReset). */` |
|     13 |  829 | `		pThis->iRef = 2; /* Prevent garbage collection */` |
|     13 |  830 | `		PH7_VmCallClassMethod(pVm,pThis,pDestr,0,0,0);` |
|      6 |  831 | `	}` |
|      - |  832 | `	/* Release non-static attributes */` |
|   1625 |  833 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|   7825 |  834 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|   6205 |  835 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|   6205 |  836 | `		if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|      - |  837 | `			/* Drop any typed-property enforcement slot registered for this` |
|      - |  838 | `			 * memobj. Must happen before the memobj is returned to the free` |
|      - |  839 | `			 * list so a future recycled slot does not inherit the stale entry. */` |
|   6187 |  840 | `			if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|    219 |  841 | `				SyHashDeleteEntry(&pVm->hTypedSlot,` |
|    144 |  842 | `					(const void *)&pVmAttr->nIdx,sizeof(sxu32),0);` |
|     72 |  843 | `			}` |
|   6187 |  844 | `			PH7_VmUnsetMemObj(pVm,pVmAttr->nIdx,TRUE);` |
|   3091 |  845 | `		}` |
|   6205 |  846 | `		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      5 |  847 | `	}` |
|      - |  848 | `	/* Release the whole structure */` |
|   1625 |  849 | `	SyHashRelease(&pThis->hAttr);` |
|   1625 |  850 | `	SyMemBackendPoolFree(&pVm->sAllocator,pThis);` |
|    819 |  851 |  |
|      - |  852 | `/*` |
|      - |  853 | ` * Decrement the reference count of a class instance [i.e Object in the PHP jargon].` |
|      - |  854 | ` * If the reference count reaches zero,release the whole instance.` |
|      - |  855 | ` */` |
|  29490 |  856 | `PH7_PRIVATE void PH7_ClassInstanceUnref(ph7_class_instance *pThis)` |
|      5 |  857 |  |
|  29495 |  858 | `	pThis->iRef--;` |
|  29495 |  859 | `	if( pThis->iRef < 1 ){` |
|      - |  860 | `		/* No more reference to this instance */` |
|   1633 |  861 | `		PH7_ClassInstanceRelease(&(*pThis));` |
|    814 |  862 | `	}` |
|  29495 |  863 |  |
|      - |  864 | `/*` |
|      - |  865 | ` * Compare two class instances [i.e: Objects in the PHP jargon]` |
|      - |  866 | ` * Note on objects comparison:` |
|      - |  867 | ` *  According to the PHP langauge reference manual` |
|      - |  868 | ` *  When using the comparison operator (==), object variables are compared in a simple manner` |
|      - |  869 | ` *  namely: Two object instances are equal if they have the same attributes and values, and are` |
|      - |  870 | ` *  instances of the same class.` |
|      - |  871 | ` *  On the other hand, when using the identity operator (===), object variables are identical` |
|      - |  872 | ` *  if and only if they refer to the same instance of the same class.` |
|      - |  873 | ` *  An example will clarify these rules.` |
|      - |  874 | ` *  Example #1 Example of object comparison` |
|      - |  875 | ` *  <?php` |
|      - |  876 | ` *    function bool2str($bool)` |
|      - |  877 | ` * {` |
|      - |  878 | ` *   if ($bool === false) {` |
|      - |  879 | ` *       return 'FALSE';` |
|      - |  880 | ` *   } else {` |
|      - |  881 | ` *       return 'TRUE';` |
|      - |  882 | ` *   }` |
|      - |  883 | ` * }` |
|      - |  884 | ` * function compareObjects(&$o1, &$o2)` |
|      - |  885 | ` * {` |
|      - |  886 | ` *   echo 'o1 == o2 : ' . bool2str($o1 == $o2) . "\n";` |
|      - |  887 | ` *   echo 'o1 != o2 : ' . bool2str($o1 != $o2) . "\n";` |
|      - |  888 | ` *   echo 'o1 === o2 : ' . bool2str($o1 === $o2) . "\n";` |
|      - |  889 | ` *   echo 'o1 !== o2 : ' . bool2str($o1 !== $o2) . "\n";` |
|      - |  890 | ` * }` |
|      - |  891 | ` * class Flag` |
|      - |  892 | ` * {` |
|      - |  893 | ` *   public $flag;` |
|      - |  894 | ` *` |
|      - |  895 | ` *   function Flag($flag = true) {` |
|      - |  896 | ` *       $this->flag = $flag;` |
|      - |  897 | ` *   }` |
|      - |  898 | ` * }` |
|      - |  899 | ` *` |
|      - |  900 | ` * class OtherFlag` |
|      - |  901 | ` * {` |
|      - |  902 | ` *   public $flag;` |
|      - |  903 | ` *` |
|      - |  904 | ` *   function OtherFlag($flag = true) {` |
|      - |  905 | ` *       $this->flag = $flag;` |
|      - |  906 | ` *   }` |
|      - |  907 | ` * }` |
|      - |  908 | ` *` |
|      - |  909 | ` * $o = new Flag();` |
|      - |  910 | ` * $p = new Flag();` |
|      - |  911 | ` * $q = $o;` |
|      - |  912 | ` * $r = new OtherFlag();` |
|      - |  913 | ` *` |
|      - |  914 | ` * echo "Two instances of the same class\n";` |
|      - |  915 | ` * compareObjects($o, $p);` |
|      - |  916 | ` * echo "\nTwo references to the same instance\n";` |
|      - |  917 | ` * compareObjects($o, $q);` |
|      - |  918 | ` * echo "\nInstances of two different classes\n";` |
|      - |  919 | ` * compareObjects($o, $r);` |
|      - |  920 | ` * ?>` |
|      - |  921 | ` * The above example will output:` |
|      - |  922 | ` * Two instances of the same class` |
|      - |  923 | ` * o1 == o2 : TRUE` |
|      - |  924 | ` * o1 != o2 : FALSE` |
|      - |  925 | ` * o1 === o2 : FALSE` |
|      - |  926 | ` * o1 !== o2 : TRUE` |
|      - |  927 | ` * Two references to the same instance` |
|      - |  928 | ` * o1 == o2 : TRUE` |
|      - |  929 | ` * o1 != o2 : FALSE` |
|      - |  930 | ` * o1 === o2 : TRUE` |
|      - |  931 | ` * o1 !== o2 : FALSE` |
|      - |  932 | ` * Instances of two different classes` |
|      - |  933 | ` * o1 == o2 : FALSE` |
|      - |  934 | ` * o1 != o2 : TRUE` |
|      - |  935 | ` * o1 === o2 : FALSE` |
|      - |  936 | ` * o1 !== o2 : TRUE` |
|      - |  937 | ` *` |
|      - |  938 | ` * This function return 0 if the objects are equals according to the comprison rules defined above.` |
|      - |  939 | ` * Any other return values indicates difference.` |
|      - |  940 | ` */` |
|    174 |  941 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest)` |
|      3 |  942 |  |
|      - |  943 | `	SyHashEntry *pEntry,*pEntry2;` |
|      - |  944 | `	ph7_value sV1,sV2;` |
|      - |  945 | `	sxi32 rc;` |
|    177 |  946 | `	if( iNest > 31 ){` |
|      - |  947 | `		/* Nesting limit reached */` |
|      6 |  948 | `		PH7_VmThrowError(pLeft->pVm,0,PH7_CTX_ERR,"Nesting limit reached: Infinite recursion?");` |
|      6 |  949 | `		return 1;` |
|      - |  950 | `	}` |
|      - |  951 | `	/* Comparison is performed only if the objects are instance of the same class */` |
|    173 |  952 | `	if( pLeft->pClass != pRight->pClass ){` |
|      7 |  953 | `		return 1;` |
|      - |  954 | `	}` |
|    167 |  955 | `	if( bStrict ){` |
|      - |  956 | `		/*` |
|      - |  957 | `		 * According to the PHP language reference manual:` |
|      - |  958 | `		 *  when using the identity operator (===), object variables` |
|      - |  959 | `		 *  are identical if and only if they refer to the same instance` |
|      - |  960 | `		 *  of the same class.` |
|      - |  961 | `		 */` |
|     25 |  962 | `		return !(pLeft == pRight);` |
|      - |  963 | `	}` |
|      - |  964 | `	/*` |
|      - |  965 | `	 * Attribute comparison.` |
|      - |  966 | `	 * According to the PHP reference manual:` |
|      - |  967 | `	 *  When using the comparison operator (==), object variables are compared` |
|      - |  968 | `	 *  in a simple manner, namely: Two object instances are equal if they have` |
|      - |  969 | `	 *  the same attributes and values, and are instances of the same class.` |
|      - |  970 | `	 */` |
|    143 |  971 | `	if( pLeft == pRight ){` |
|      - |  972 | `		/* Same instance,don't bother processing,object are equals */` |
|      3 |  973 | `		return 0;` |
|      - |  974 | `	}` |
|    141 |  975 | `	SyHashResetLoopCursor(&pLeft->hAttr);` |
|    141 |  976 | `	SyHashResetLoopCursor(&pRight->hAttr);` |
|    141 |  977 | `	PH7_MemObjInit(pLeft->pVm,&sV1);` |
|    141 |  978 | `	PH7_MemObjInit(pLeft->pVm,&sV2);` |
|    141 |  979 | `	sV1.nIdx = sV2.nIdx = SXU32_HIGH;` |
|    224 |  980 | `	while((pEntry = SyHashGetNextEntry(&pLeft->hAttr)) != 0 && (pEntry2 = SyHashGetNextEntry(&pRight->hAttr)) != 0 ){` |
|    147 |  981 | `		VmClassAttr *p1 = (VmClassAttr *)pEntry->pUserData;` |
|    147 |  982 | `		VmClassAttr *p2 = (VmClassAttr *)pEntry2->pUserData;` |
|      - |  983 | `		/* Compare only non-static attribute */` |
|    147 |  984 | `		if( (p1->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|      - |  985 | `			ph7_value *pL,*pR;` |
|    147 |  986 | `			pL = ExtractClassAttrValue(pLeft->pVm,p1);` |
|    147 |  987 | `			pR = ExtractClassAttrValue(pRight->pVm,p2);` |
|    147 |  988 | `			if( pL && pR ){` |
|    147 |  989 | `				PH7_MemObjLoad(pL,&sV1);` |
|    147 |  990 | `				PH7_MemObjLoad(pR,&sV2);` |
|      - |  991 | `				/* Compare the two values now */` |
|    147 |  992 | `				rc = PH7_MemObjCmp(&sV1,&sV2,bStrict,iNest+1);` |
|    147 |  993 | `				PH7_MemObjRelease(&sV1);` |
|    147 |  994 | `				PH7_MemObjRelease(&sV2);` |
|    147 |  995 | `				if( rc != 0 ){` |
|      - |  996 | `					/* Not equals */` |
|    133 |  997 | `					return rc;` |
|      - |  998 | `				}` |
|      7 |  999 | `			}` |
|      7 | 1000 | `		}` |
|      1 | 1001 | `	}` |
|      - | 1002 | `	/* Object are equals */` |
|      9 | 1003 | `	return 0;` |
|     90 | 1004 |  |
|      - | 1005 | `/*` |
|      - | 1006 | ` * Dump a class instance and the store the dump in the BLOB given` |
|      - | 1007 | ` * as the first argument.` |
|      - | 1008 | ` * Note that only non-static/non-constants attribute are dumped.` |
|      - | 1009 | ` * This function is typically invoked when the user issue a call` |
|      - | 1010 | ` * to [var_dump(),var_export(),print_r(),...].` |
|      - | 1011 | ` * This function SXRET_OK on success. Any other return value including` |
|      - | 1012 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|      - | 1013 | ` */` |
|    132 | 1014 | `PH7_PRIVATE sxi32 PH7_ClassInstanceDump(SyBlob *pOut,ph7_class_instance *pThis,int ShowType,int nTab,int nDepth)` |
|      1 | 1015 |  |
|      - | 1016 | `	SyHashEntry *pEntry;` |
|      - | 1017 | `	ph7_value *pValue;` |
|      - | 1018 | `	sxi32 rc;` |
|      - | 1019 | `	int i;` |
|    133 | 1020 | `	if( nDepth > 31 ){` |
|      - | 1021 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|      - | 1022 | `		/* Nesting limit reached..halt immediately*/` |
|      5 | 1023 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|      5 | 1024 | `		if( ShowType ){` |
|      5 | 1025 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|      2 | 1026 | `		}` |
|      5 | 1027 | `		return SXERR_LIMIT;` |
|      - | 1028 | `	}` |
|    129 | 1029 | `	rc = SXRET_OK;` |
|    129 | 1030 | `	if( !ShowType ){` |
|      3 | 1031 | `		SyBlobAppend(&(*pOut),"Object(",sizeof("Object(")-1);` |
|      1 | 1032 | `	}` |
|      - | 1033 | `	/* Append class name */` |
|    129 | 1034 | `	SyBlobFormat(&(*pOut),"%z) {",&pThis->pClass->sName);` |
|      - | 1035 | `#ifdef __WINNT__` |
|      1 | 1036 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|      - | 1037 | `#else` |
|    128 | 1038 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      - | 1039 | `#endif` |
|      - | 1040 | `	/* Dump object attributes */` |
|    129 | 1041 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|    201 | 1042 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|    133 | 1043 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    133 | 1044 | `		if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|      - | 1045 | `			/* Dump non-static/constant attribute only */` |
|   3985 | 1046 | `			for( i = 0 ; i < nTab ; i++ ){` |
|   3853 | 1047 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|   1927 | 1048 | `			}` |
|    133 | 1049 | `			pValue = ExtractClassAttrValue(pThis->pVm,pVmAttr);` |
|    133 | 1050 | `			if( pValue ){` |
|    133 | 1051 | `				SyBlobFormat(&(*pOut),"['%z'] =>",&pVmAttr->pAttr->sName);` |
|      - | 1052 | `#ifdef __WINNT__` |
|      1 | 1053 | `				SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|      - | 1054 | `#else` |
|    132 | 1055 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      - | 1056 | `#endif` |
|    133 | 1057 | `				rc = PH7_MemObjDump(&(*pOut),pValue,ShowType,nTab+1,nDepth,0);` |
|    133 | 1058 | `				if( rc == SXERR_LIMIT ){` |
|    125 | 1059 | `					break;` |
|      - | 1060 | `				}` |
|      4 | 1061 | `			}` |
|      4 | 1062 | `		}` |
|      1 | 1063 | `	}` |
|   3977 | 1064 | `	for( i = 0 ; i < nTab ; i++ ){` |
|   3849 | 1065 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|   1925 | 1066 | `	}` |
|    129 | 1067 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|    129 | 1068 | `	return rc;` |
|     67 | 1069 |  |
|      - | 1070 | `/*` |
|      - | 1071 | ` * Call a magic method [i.e: __toString(),__toBool(),__Invoke()...]` |
|      - | 1072 | ` * Return SXRET_OK on successfull call. Any other return value indicates failure.` |
|      - | 1073 | ` * Notes on magic methods.` |
|      - | 1074 | ` * According to the PHP language reference manual.` |
|      - | 1075 | ` *  The function names __construct(), __destruct(), __call(), __callStatic()` |
|      - | 1076 | ` *  __get(),  __toString(), __invoke(), __clone() are magical in PHP classes.` |
|      - | 1077 | ` * You cannot have functions with these names in any of your classes unless` |
|      - | 1078 | ` * you want the magic functionality associated with them.` |
|      - | 1079 | ` * Example of magical methods:` |
|      - | 1080 | ` * __toString()` |
|      - | 1081 | ` *  The __toString() method allows a class to decide how it will react when it is treated like` |
|      - | 1082 | ` *  a string. For example, what echo $obj; will print. This method must return a string.` |
|      - | 1083 | ` *  Example #2 Simple example` |
|      - | 1084 | ` * <?php` |
|      - | 1085 | ` * // Declare a simple class` |
|      - | 1086 | ` * class TestClass` |
|      - | 1087 | ` * {` |
|      - | 1088 | ` *   public $foo;` |
|      - | 1089 | ` *` |
|      - | 1090 | ` *   public function __construct($foo)` |
|      - | 1091 | ` *   {` |
|      - | 1092 | ` *       $this->foo = $foo;` |
|      - | 1093 | ` *   }` |
|      - | 1094 | ` *` |
|      - | 1095 | ` *   public function __toString()` |
|      - | 1096 | ` *   {` |
|      - | 1097 | ` *       return $this->foo;` |
|      - | 1098 | ` *   }` |
|      - | 1099 | ` * }` |
|      - | 1100 | ` * $class = new TestClass('Hello');` |
|      - | 1101 | ` * echo $class;` |
|      - | 1102 | ` * ?>` |
|      - | 1103 | ` * The above example will output:` |
|      - | 1104 | ` *  Hello` |
|      - | 1105 | ` *` |
|      - | 1106 | ` * Note that PH7 does not support all the magical method and introudces __toFloat(),__toInt()` |
|      - | 1107 | ` * which have the same behaviour as __toString() but for float and integer types` |
|      - | 1108 | ` * respectively.` |
|      - | 1109 | ` * Refer to the official documentation for more information.` |
|      - | 1110 | ` */` |
|      2 | 1111 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallMagicMethod(` |
|      - | 1112 | `	ph7_vm *pVm,               /* VM that own all this stuff */` |
|      - | 1113 | `	ph7_class *pClass,         /* Target class */` |
|      - | 1114 | `	ph7_class_instance *pThis, /* Target object */` |
|      - | 1115 | `	const char *zMethod,       /* Magic method name [i.e: __toString()]*/` |
|      - | 1116 | `	sxu32 nByte,               /* zMethod length*/` |
|      - | 1117 | `	const SyString *pAttrName  /* Attribute name */` |
|      - | 1118 | `	)` |
|      1 | 1119 |  |
|      3 | 1120 | `	ph7_value *apArg[2] = { 0 , 0 };` |
|      - | 1121 | `	ph7_class_method *pMeth;` |
|      - | 1122 | `	ph7_value sAttr; /* cc warning */` |
|      - | 1123 | `	sxi32 rc;` |
|      - | 1124 | `	int nArg;` |
|      - | 1125 | `	/* Make sure the magic method is available */` |
|      3 | 1126 | `	pMeth = PH7_ClassExtractMethod(&(*pClass),zMethod,nByte);` |
|      3 | 1127 | `	if( pMeth == 0 ){` |
|      - | 1128 | `		/* No such method,return immediately */` |
|      3 | 1129 | `		return SXERR_NOTFOUND;` |
|      - | 1130 | `	}` |
|    ! 0 | 1131 | `	nArg = 0;` |
|      - | 1132 | `	/* Copy arguments */` |
|    ! 0 | 1133 | `	if( pAttrName ){` |
|    ! 0 | 1134 | `		PH7_MemObjInitFromString(pVm,&sAttr,pAttrName);` |
|    ! 0 | 1135 | `		sAttr.nIdx = SXU32_HIGH; /* Mark as constant */` |
|    ! 0 | 1136 | `		apArg[0] = &sAttr;` |
|    ! 0 | 1137 | `		nArg = 1;` |
|    ! 0 | 1138 | `	}` |
|      - | 1139 | `	/* Call the magic method now */` |
|    ! 0 | 1140 | `	rc = PH7_VmCallClassMethod(pVm,&(*pThis),pMeth,0,nArg,apArg);` |
|      - | 1141 | `	/* Clean up */` |
|    ! 0 | 1142 | `	if( pAttrName ){` |
|    ! 0 | 1143 | `		PH7_MemObjRelease(&sAttr);` |
|    ! 0 | 1144 | `	}` |
|    ! 0 | 1145 | `	return rc;` |
|      2 | 1146 |  |
|      - | 1147 | `/*` |
|      - | 1148 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon].` |
|      - | 1149 | ` * This function is simply a wrapper on ExtractClassAttrValue().` |
|      - | 1150 | ` */` |
|     22 | 1151 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceExtractAttrValue(ph7_class_instance *pThis,VmClassAttr *pAttr)` |
|      2 | 1152 |  |
|      - | 1153 | `   /* Extract the attribute value */` |
|      - | 1154 | `	ph7_value *pValue;` |
|     24 | 1155 | `	pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     24 | 1156 | `	return pValue;` |
|      2 | 1157 |  |
|      - | 1158 | `/*` |
|      - | 1159 | ` * Convert a class instance [i.e: Object in the PHP jargon] into a hashmap [i.e: array in the PHP jargon].` |
|      - | 1160 | ` * Return SXRET_OK on success. Any other value indicates failure.` |
|      - | 1161 | ` * Note on object conversion to array:` |
|      - | 1162 | ` *  Acccording to the PHP language reference manual` |
|      - | 1163 | ` *  If an object is converted to an array, the result is an array whose elements are the object's properties.` |
|      - | 1164 | ` *  The keys are the member variable names.` |
|      - | 1165 | ` *` |
|      - | 1166 | ` *  The following example:` |
|      - | 1167 | ` *  class Test {` |
|      - | 1168 | ` *   public $A = 25<<1;  // 50` |
|      - | 1169 | ` *	 public $c = rand_str(3);   // Random string of length 3` |
|      - | 1170 | ` *	 public $d = rand() & 1023; // Random number between 0..1023` |
|      - | 1171 | ` *  }` |
|      - | 1172 | ` *  var_dump((array) new Test());` |
|      - | 1173 | ` *	Will output:` |
|      - | 1174 | ` *  array(3) {` |
|      - | 1175 | ` *   [A] =>` |
|      - | 1176 | ` *      int(50)` |
|      - | 1177 | ` *   [c] =>` |
|      - | 1178 | ` *     string(3 'aps')` |
|      - | 1179 | ` *   [d] =>` |
|      - | 1180 | ` *     int(991)` |
|      - | 1181 | ` *  }` |
|      - | 1182 | ` * You have noticed that PH7 allow class attributes [i.e: $a,$c,$d in the example above]` |
|      - | 1183 | ` * have any complex expression (even function calls/Annonymous functions) as their default` |
|      - | 1184 | ` * value unlike the standard PHP engine.` |
|      - | 1185 | ` * This is a very powerful feature that you have to look at.` |
|      - | 1186 | ` */` |
|      6 | 1187 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|      1 | 1188 |  |
|      - | 1189 | `	SyHashEntry *pEntry;` |
|      - | 1190 | `	SyString *pAttrName;` |
|      - | 1191 | `	VmClassAttr *pAttr;` |
|      - | 1192 | `	ph7_value *pValue;` |
|      - | 1193 | `	ph7_value sName;` |
|      - | 1194 | `	/* Reset the loop cursor */` |
|      7 | 1195 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      7 | 1196 | `	PH7_MemObjInitFromString(pThis->pVm,&sName,0);` |
|     20 | 1197 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|      - | 1198 | `		/* Point to the current attribute */` |
|     11 | 1199 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1200 | `		/* Extract attribute value */` |
|     11 | 1201 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     11 | 1202 | `		if( pValue ){` |
|      - | 1203 | `			/* Build attribute name */` |
|     11 | 1204 | `			pAttrName = &pAttr->pAttr->sName;` |
|     11 | 1205 | `			PH7_MemObjStringAppend(&sName,pAttrName->zString,pAttrName->nByte);` |
|      - | 1206 | `			/* Perform the insertion */` |
|     11 | 1207 | `			PH7_HashmapInsert(pMap,&sName,pValue);` |
|      - | 1208 | `			/* Reset the string cursor */` |
|     11 | 1209 | `			SyBlobReset(&sName.sBlob);` |
|      5 | 1210 | `		}` |
|      1 | 1211 | `	}` |
|      7 | 1212 | `	PH7_MemObjRelease(&sName);` |
|      7 | 1213 | `	return SXRET_OK;` |
|      1 | 1214 |  |
|      - | 1215 | `/*` |
|      - | 1216 | ` * Iterate throw class attributes and invoke the given callback [i.e: xWalk()] for each` |
|      - | 1217 | ` * retrieved attribute.` |
|      - | 1218 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|      - | 1219 | ` * the attribute value in the callback body will not alter the real attribute value.` |
|      - | 1220 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|      - | 1221 | ` * a value different from PH7_OK.` |
|      - | 1222 | ` * Refer to [ph7_object_walk()] for more information.` |
|      - | 1223 | ` */` |
|      2 | 1224 | `PH7_PRIVATE sxi32 PH7_ClassInstanceWalk(` |
|      - | 1225 | `	ph7_class_instance *pThis, /* Target object */` |
|      - | 1226 | `	int (*xWalk)(const char *,ph7_value *,void *), /* Walker callback */` |
|      - | 1227 | `	void *pUserData /* Last argument to xWalk() */` |
|      - | 1228 | `	)` |
|      1 | 1229 |  |
|      - | 1230 | `	SyHashEntry *pEntry; /* Hash entry */` |
|      - | 1231 | `	VmClassAttr *pAttr;  /* Pointer to the attribute */` |
|      - | 1232 | `	ph7_value *pValue;   /* Attribute value */` |
|      - | 1233 | `	ph7_value sValue;    /* Copy of the attribute value */` |
|      - | 1234 | `	int rc;` |
|      - | 1235 | `	/* Reset the loop cursor */` |
|      3 | 1236 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      3 | 1237 | `	PH7_MemObjInit(pThis->pVm,&sValue);` |
|      - | 1238 | `	/* Start the walk process */` |
|      8 | 1239 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|      - | 1240 | `		/* Point to the current attribute */` |
|      5 | 1241 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1242 | `		/* Extract attribute value */` |
|      5 | 1243 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|      5 | 1244 | `		if( pValue ){` |
|      5 | 1245 | `			PH7_MemObjLoad(pValue,&sValue);` |
|      - | 1246 | `			/* Invoke the supplied callback */` |
|      5 | 1247 | `			rc =  xWalk(SyStringData(&pAttr->pAttr->sName),&sValue,pUserData);` |
|      5 | 1248 | `			PH7_MemObjRelease(&sValue);` |
|      5 | 1249 | `			if( rc != PH7_OK){` |
|      - | 1250 | `				/* User callback request an operation abort */` |
|    ! 0 | 1251 | `				return SXERR_ABORT;` |
|      - | 1252 | `			}` |
|      2 | 1253 | `		}` |
|      1 | 1254 | `	}` |
|      - | 1255 | `	/* All done */` |
|      3 | 1256 | `	return SXRET_OK;` |
|      2 | 1257 |  |
|      - | 1258 | `/*` |
|      - | 1259 | ` * Extract a class atrribute value.` |
|      - | 1260 | ` * Return a pointer to the attribute value on success. Otherwise NULL.` |
|      - | 1261 | ` * Note:` |
|      - | 1262 | ` *  Access to static and constant attribute is not allowed. That is,the function` |
|      - | 1263 | ` *  will return NULL in case someone (host-application code) try to extract` |
|      - | 1264 | ` *  a static/constant attribute.` |
|      - | 1265 | ` */` |
|    718 | 1266 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceFetchAttr(ph7_class_instance *pThis,const SyString *pName)` |
|      5 | 1267 |  |
|      - | 1268 | `	SyHashEntry *pEntry;` |
|      - | 1269 | `	VmClassAttr *pAttr;` |
|      - | 1270 | `	/* Query the attribute hashtable */` |
|    723 | 1271 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);` |
|    723 | 1272 | `	if( pEntry == 0 ){` |
|      - | 1273 | `		/* No such attribute */` |
|    ! 0 | 1274 | `		return 0;` |
|      - | 1275 | `	}` |
|      - | 1276 | `	/* Point to the class atrribute */` |
|    723 | 1277 | `	pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1278 | `	/* Check if we are dealing with a static/constant attribute */` |
|    723 | 1279 | `	if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|      - | 1280 | `		/* Access is forbidden */` |
|    ! 0 | 1281 | `		return 0;` |
|      - | 1282 | `	}` |
|      - | 1283 | `	/* Return the attribute value */` |
|    723 | 1284 | `	return ExtractClassAttrValue(pThis->pVm,pAttr);` |
|    364 | 1285 |  |
|      - | 1286 |  |
