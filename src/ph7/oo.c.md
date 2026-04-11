# src/ph7/oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 437/500 lines (87.40%)

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
|  47378 |   14 | `PH7_PRIVATE ph7_class * PH7_NewRawClass(ph7_vm *pVm,const SyString *pName,sxu32 nLine)` |
|      2 |   15 |  |
|      - |   16 | `	ph7_class *pClass;` |
|      - |   17 | `	char *zName;` |
|      - |   18 | `	/* Allocate a new instance */` |
|  47380 |   19 | `	pClass = (ph7_class *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class));` |
|  47380 |   20 | `	if( pClass == 0 ){` |
|    ! 0 |   21 | `		return 0;` |
|      - |   22 | `	}` |
|      - |   23 | `	/* Zero the structure */` |
|  47380 |   24 | `	SyZero(pClass,sizeof(ph7_class));` |
|      - |   25 | `	/* Duplicate class name */` |
|  47380 |   26 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  47380 |   27 | `	if( zName == 0 ){` |
|    ! 0 |   28 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClass);` |
|    ! 0 |   29 | `		return 0;` |
|      - |   30 | `	}` |
|      - |   31 | `	/* Initialize fields */` |
|  47380 |   32 | `	SyStringInitFromBuf(&pClass->sName,zName,pName->nByte);` |
|  47380 |   33 | `	SyHashInit(&pClass->hMethod,&pVm->sAllocator,0,0);` |
|  47380 |   34 | `	SyHashInit(&pClass->hAttr,&pVm->sAllocator,0,0);` |
|  47380 |   35 | `	SyHashInit(&pClass->hDerived,&pVm->sAllocator,0,0);` |
|  47380 |   36 | `	SySetInit(&pClass->aInterface,&pVm->sAllocator,sizeof(ph7_class *));` |
|  47380 |   37 | `	SySetInit(&pClass->aTrait,&pVm->sAllocator,sizeof(ph7_class *));` |
|  47380 |   38 | `	pClass->nLine = nLine;` |
|      - |   39 | `	/* All done */` |
|  47380 |   40 | `	return pClass;` |
|  23691 |   41 |  |
|      - |   42 | `/*` |
|      - |   43 | ` * Allocate and initialize a new class attribute.` |
|      - |   44 | ` * Return a pointer to the class attribute on success. NULL otherwise.` |
|      - |   45 | ` */` |
|  36076 |   46 | `PH7_PRIVATE ph7_class_attr * PH7_NewClassAttr(ph7_vm *pVm,const SyString *pName,sxu32 nLine,sxi32 iProtection,sxi32 iFlags)` |
|      2 |   47 |  |
|      - |   48 | `	ph7_class_attr *pAttr;` |
|      - |   49 | `	char *zName;` |
|  36078 |   50 | `	pAttr = (ph7_class_attr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_attr));` |
|  36078 |   51 | `	if( pAttr == 0 ){` |
|    ! 0 |   52 | `		return 0;` |
|      - |   53 | `	}` |
|      - |   54 | `	/* Zero the structure */` |
|  36078 |   55 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|      - |   56 | `	/* Duplicate attribute name */` |
|  36078 |   57 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  36078 |   58 | `	if( zName == 0 ){` |
|    ! 0 |   59 | `		SyMemBackendPoolFree(&pVm->sAllocator,pAttr);` |
|    ! 0 |   60 | `		return 0;` |
|      - |   61 | `	}` |
|      - |   62 | `	/* Initialize fields */` |
|  36078 |   63 | `	SySetInit(&pAttr->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|  36078 |   64 | `	SyStringInitFromBuf(&pAttr->sName,zName,pName->nByte);` |
|  36078 |   65 | `	pAttr->iProtection = iProtection;` |
|  36078 |   66 | `	pAttr->nIdx = SXU32_HIGH;` |
|  36078 |   67 | `	pAttr->iFlags = iFlags;` |
|  36078 |   68 | `	pAttr->nLine = nLine;` |
|  36078 |   69 | `	return pAttr;` |
|  18040 |   70 |  |
|      - |   71 | `/*` |
|      - |   72 | ` * Allocate and initialize a new class method.` |
|      - |   73 | ` * Return a pointer to the class method on success. NULL otherwise` |
|      - |   74 | ` * This function associate with the newly created method an automatically generated` |
|      - |   75 | ` * random unique name.` |
|      - |   76 | ` */` |
| 132402 |   77 | `PH7_PRIVATE ph7_class_method * PH7_NewClassMethod(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,sxu32 nLine,` |
|      - |   78 | `	sxi32 iProtection,sxi32 iFlags,sxi32 iFuncFlags)` |
|      2 |   79 |  |
|      - |   80 | `	ph7_class_method *pMeth;` |
|      - |   81 | `	SyHashEntry *pEntry;` |
|      - |   82 | `	SyString *pNamePtr;` |
|      - |   83 | `	char zSalt[10];` |
|      - |   84 | `	char *zName;` |
|      - |   85 | `	sxu32 nByte;` |
|      - |   86 | `	/* Allocate a new class method instance */` |
| 132404 |   87 | `	pMeth = (ph7_class_method *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_method));` |
| 132404 |   88 | `	if( pMeth == 0 ){` |
|    ! 0 |   89 | `		return 0;` |
|      - |   90 | `	}` |
|      - |   91 | `	/* Zero the structure */` |
| 132404 |   92 | `	SyZero(pMeth,sizeof(ph7_class_method));` |
|      - |   93 | `	/* Check for an already installed method with the same name */` |
| 132404 |   94 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
| 132404 |   95 | `	if( pEntry == 0 ){` |
|      - |   96 | `		/* Associate an unique VM name to this method */` |
| 132402 |   97 | `		nByte = sizeof(zSalt) + pName->nByte + SyStringLength(&pClass->sName)+sizeof(char)*7/*[[__'\0'*/;` |
| 132402 |   98 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,nByte);` |
| 132402 |   99 | `		if( zName == 0 ){` |
|    ! 0 |  100 | `			SyMemBackendPoolFree(&pVm->sAllocator,pMeth);` |
|    ! 0 |  101 | `			return 0;` |
|      - |  102 | `		}` |
| 132402 |  103 | `		pNamePtr = &pMeth->sVmName;` |
|      - |  104 | `		/* Generate a random string */` |
| 132402 |  105 | `		PH7_VmRandomString(&(*pVm),zSalt,sizeof(zSalt));` |
| 132402 |  106 | `		pNamePtr->nByte = SyBufferFormat(zName,nByte,"[__%z@%z_%.*s]",&pClass->sName,pName,sizeof(zSalt),zSalt);` |
| 132402 |  107 | `		pNamePtr->zString = zName;` |
|  66202 |  108 | `	}else{` |
|      - |  109 | `		/* Method is condidate for 'overloading' */` |
|      3 |  110 | `		ph7_class_method *pCurrent = (ph7_class_method *)pEntry->pUserData;` |
|      3 |  111 | `		pNamePtr = &pMeth->sVmName;` |
|      - |  112 | `		/* Use the same VM name */` |
|      3 |  113 | `		SyStringDupPtr(pNamePtr,&pCurrent->sVmName);` |
|      3 |  114 | `		zName = (char *)pNamePtr->zString;` |
|      - |  115 | `	}` |
| 132404 |  116 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|     32 |  117 | `		if( (pName->nByte == sizeof("__construct") - 1 && SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1 ) == 0)` |
|     21 |  118 | `			\|\| (pName->nByte == sizeof("__destruct") - 1 && SyMemcmp(pName->zString,"__destruct",sizeof("__destruct") - 1 ) == 0)` |
|     22 |  119 | `			\|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|      - |  120 | `				/* Switch to public visibility when dealing with constructor/destructor */` |
|      5 |  121 | `				iProtection = PH7_CLASS_PROT_PUBLIC;` |
|      2 |  122 | `		}` |
|     12 |  123 | `	}` |
|      - |  124 | `	/* Initialize method fields */` |
| 132406 |  125 | `	pMeth->iProtection = iProtection;` |
| 132406 |  126 | `	pMeth->iFlags = iFlags;` |
| 132406 |  127 | `	pMeth->nLine = nLine;` |
| 198609 |  128 | `	PH7_VmInitFuncState(&(*pVm),&pMeth->sFunc,&zName[sizeof(char)*4/*[__@*/+SyStringLength(&pClass->sName)],` |
| 132404 |  129 | `		pName->nByte,iFuncFlags\|VM_FUNC_CLASS_METHOD,pClass);` |
| 132406 |  130 | `	return pMeth;` |
|  66205 |  131 |  |
|      - |  132 | `/*` |
|      - |  133 | ` * Check if the given name have a class method associated with it.` |
|      - |  134 | ` * Return the desired method [i.e: ph7_class_method instance] on success. NULL otherwise.` |
|      - |  135 | ` */` |
|  20562 |  136 | `PH7_PRIVATE ph7_class_method * PH7_ClassExtractMethod(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|      2 |  137 |  |
|      - |  138 | `	SyHashEntry *pEntry;` |
|      - |  139 | `	/* Perform a hash lookup */` |
|  20564 |  140 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)zName,nByte);` |
|  20564 |  141 | `	if( pEntry == 0 ){` |
|      - |  142 | `		/* No such entry */` |
|   2294 |  143 | `		return 0;` |
|      - |  144 | `	}` |
|      - |  145 | `	/* Point to the desired method */` |
|  18272 |  146 | `	return (ph7_class_method *)pEntry->pUserData;` |
|  10283 |  147 |  |
|      - |  148 | `/*` |
|      - |  149 | ` * Check if the given name is a class attribute.` |
|      - |  150 | ` * Return the desired attribute [i.e: ph7_class_attr instance] on success.NULL otherwise.` |
|      - |  151 | ` */` |
|     82 |  152 | `PH7_PRIVATE ph7_class_attr * PH7_ClassExtractAttribute(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|      2 |  153 |  |
|      - |  154 | `	SyHashEntry *pEntry;` |
|      - |  155 | `	/* Perform a hash lookup */` |
|     84 |  156 | `	pEntry = SyHashGet(&pClass->hAttr,(const void *)zName,nByte);` |
|     84 |  157 | `	if( pEntry == 0 ){` |
|      - |  158 | `		/* No such entry */` |
|    ! 0 |  159 | `		return 0;` |
|      - |  160 | `	}` |
|      - |  161 | `	/* Point to the desierd method */` |
|     84 |  162 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|     43 |  163 |  |
|      - |  164 | `/*` |
|      - |  165 | ` * Install a class attribute in the corresponding container.` |
|      - |  166 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|      - |  167 | ` */` |
|  36076 |  168 | `PH7_PRIVATE sxi32 PH7_ClassInstallAttr(ph7_class *pClass,ph7_class_attr *pAttr)` |
|      2 |  169 |  |
|  36078 |  170 | `	SyString *pName = &pAttr->sName;` |
|      - |  171 | `	sxi32 rc;` |
|      - |  172 | `	/* Remember where this attribute was originally declared so that later` |
|      - |  173 | `	 * inheritance/trait copies still know the declaring class (needed for` |
|      - |  174 | `	 * PHP-compatible error messages on typed properties). */` |
|  36078 |  175 | `	if( pAttr->pDeclClass == 0 ){` |
|  36078 |  176 | `		pAttr->pDeclClass = pClass;` |
|  18038 |  177 | `	}` |
|  36078 |  178 | `	rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|  36078 |  179 | `	return rc;` |
|      2 |  180 |  |
|      - |  181 | `/*` |
|      - |  182 | ` * Install a class method in the corresponding container.` |
|      - |  183 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|      - |  184 | ` */` |
| 132400 |  185 | `PH7_PRIVATE sxi32 PH7_ClassInstallMethod(ph7_class *pClass,ph7_class_method *pMeth)` |
|      2 |  186 |  |
| 132402 |  187 | `	SyString *pName = &pMeth->sFunc.sName;` |
|      - |  188 | `	sxi32 rc;` |
| 132402 |  189 | `	rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 132402 |  190 | `	return rc;` |
|      2 |  191 |  |
|      - |  192 | `/*` |
|      - |  193 | ` * Perform an inheritance operation.` |
|      - |  194 | ` * According to the PHP language reference manual` |
|      - |  195 | ` *  When you extend a class, the subclass inherits all of the public and protected methods` |
|      - |  196 | ` *  from the parent class. Unless a class Overwrites those methods, they will retain their original` |
|      - |  197 | ` *  functionality.` |
|      - |  198 | ` *  This is useful for defining and abstracting functionality, and permits the implementation` |
|      - |  199 | ` *  of additional functionality in similar objects without the need to reimplement all of the shared` |
|      - |  200 | ` *  functionality.` |
|      - |  201 | ` *  Example #1 Inheritance Example` |
|      - |  202 | ` * <?php` |
|      - |  203 | ` * class foo` |
|      - |  204 | ` * {` |
|      - |  205 | ` *   public function printItem($string)` |
|      - |  206 | ` *   {` |
|      - |  207 | ` *       echo 'Foo: ' . $string . PHP_EOL;` |
|      - |  208 | ` *   }` |
|      - |  209 | ` *` |
|      - |  210 | ` *   public function printPHP()` |
|      - |  211 | ` *   {` |
|      - |  212 | ` *       echo 'PHP is great.' . PHP_EOL;` |
|      - |  213 | ` *   }` |
|      - |  214 | ` * }` |
|      - |  215 | ` * class bar extends foo` |
|      - |  216 | ` * {` |
|      - |  217 | ` *   public function printItem($string)` |
|      - |  218 | ` *   {` |
|      - |  219 | ` *       echo 'Bar: ' . $string . PHP_EOL;` |
|      - |  220 | ` *   }` |
|      - |  221 | ` * }` |
|      - |  222 | ` * $foo = new foo();` |
|      - |  223 | ` * $bar = new bar();` |
|      - |  224 | ` * $foo->printItem('baz'); // Output: 'Foo: baz'` |
|      - |  225 | ` * $foo->printPHP();       // Output: 'PHP is great'` |
|      - |  226 | ` * $bar->printItem('baz'); // Output: 'Bar: baz'` |
|      - |  227 | ` * $bar->printPHP();       // Output: 'PHP is great'` |
|      - |  228 | ` *` |
|      - |  229 | ` * This function return SXRET_OK if the inheritance operation was successfully performed.` |
|      - |  230 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|      - |  231 | ` * error message.` |
|      - |  232 | ` */` |
|  24848 |  233 | `PH7_PRIVATE sxi32 PH7_ClassInherit(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase)` |
|      2 |  234 |  |
|      - |  235 | `	ph7_class_method *pMeth;` |
|      - |  236 | `	ph7_class_attr *pAttr;` |
|      - |  237 | `	SyHashEntry *pEntry;` |
|      - |  238 | `	SyString *pName;` |
|      - |  239 | `	sxi32 rc;` |
|      - |  240 | `	/* Install in the derived hashtable */` |
|  24850 |  241 | `	rc = SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|  24850 |  242 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  243 | `		return rc;` |
|      - |  244 | `	}` |
|      - |  245 | `	/* Copy public/protected attributes from the base class */` |
|  24850 |  246 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
| 173510 |  247 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|      - |  248 | `		/* Make sure the private attributes are not redeclared in the subclass */` |
| 148662 |  249 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
| 148662 |  250 | `		pName = &pAttr->sName;` |
| 148662 |  251 | `		if( (pEntry = SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|      6 |  252 | `			if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE &&` |
|      2 |  253 | `				((ph7_class_attr *)pEntry->pUserData)->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|      - |  254 | `					/* Cannot redeclare private attribute */` |
|      4 |  255 | `					PH7_GenCompileError(&(*pGen),E_WARNING,((ph7_class_attr *)pEntry->pUserData)->nLine,` |
|      - |  256 | `						"Private attribute '%z::%z' redeclared inside child class '%z'",` |
|      1 |  257 | `						&pBase->sName,pName,&pSub->sName);` |
|      - |  258 |  |
|      1 |  259 | `			}` |
|      6 |  260 | `			continue;` |
|      - |  261 | `		}` |
|      - |  262 | `		/* Install the attribute */` |
| 148658 |  263 | `		if( pAttr->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
| 148654 |  264 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
| 148654 |  265 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  266 | `				return rc;` |
|      - |  267 | `			}` |
|  74326 |  268 | `		}` |
|      2 |  269 | `	}` |
|  24850 |  270 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
| 247902 |  271 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|      - |  272 | `		/* Make sure the private/final methods are not redeclared in the subclass */` |
| 223054 |  273 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
| 223054 |  274 | `		pName = &pMeth->sFunc.sName;` |
| 223054 |  275 | `		if( (pEntry = SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|   2780 |  276 | `			 if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|      - |  277 | `				/* Cannot Overwrite final method */` |
|      7 |  278 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_method *)pEntry->pUserData)->nLine,` |
|      - |  279 | `					"Cannot Overwrite final method '%z:%z' inside child class '%z'",` |
|      2 |  280 | `					&pBase->sName,pName,&pSub->sName);` |
|      5 |  281 | `				if( rc == SXERR_ABORT ){` |
|    ! 0 |  282 | `					return SXERR_ABORT;` |
|      - |  283 | `				}` |
|      2 |  284 | `			}` |
|   2780 |  285 | `			continue;` |
|      - |  286 | `		}` |
|      - |  287 | `		/* Install the method */` |
| 220276 |  288 | `		if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
| 220274 |  289 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 220274 |  290 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  291 | `				return rc;` |
|      - |  292 | `			}` |
| 110136 |  293 | `		}` |
|      2 |  294 | `	}` |
|      - |  295 | `	/* Mark as subclass */` |
|  24850 |  296 | `	pSub->pBase = pBase;` |
|      - |  297 | `	/* All done */` |
|  24850 |  298 | `	return SXRET_OK;` |
|  12426 |  299 |  |
|      - |  300 | `/*` |
|      - |  301 | ` * Apply a trait to a class: copy all methods and attributes from the trait` |
|      - |  302 | ` * into the target class. Unlike inheritance, traits copy ALL members including` |
|      - |  303 | ` * private ones. Members already defined in the class take precedence.` |
|      - |  304 | ` */` |
|     42 |  305 | `PH7_PRIVATE sxi32 PH7_ClassUseTrait(ph7_gen_state *pGen,ph7_class *pClass,ph7_class *pTrait)` |
|      2 |  306 |  |
|      - |  307 | `	ph7_class_method *pMeth;` |
|      - |  308 | `	ph7_class_attr *pAttr;` |
|      - |  309 | `	SyHashEntry *pEntry;` |
|      - |  310 | `	SyString *pName;` |
|      - |  311 | `	sxi32 rc;` |
|      - |  312 | `	/* Detect cyclic trait composition (e.g. trait A { use B; } trait B { use A; }) */` |
|     44 |  313 | `	if( pTrait->iFlags & PH7_CLASS_TRAIT_VISITING ){` |
|    ! 0 |  314 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|    ! 0 |  315 | `			"Trait circular reference detected: %z is already being applied",&pTrait->sName);` |
|    ! 0 |  316 | `		if( rc == SXERR_ABORT ){` |
|    ! 0 |  317 | `			return SXERR_ABORT;` |
|      - |  318 | `		}` |
|    ! 0 |  319 | `		return SXRET_OK;` |
|      - |  320 | `	}` |
|     44 |  321 | `	pTrait->iFlags \|= PH7_CLASS_TRAIT_VISITING;` |
|     44 |  322 | `	rc = SXRET_OK;` |
|      - |  323 | `	/* Copy attributes from the trait */` |
|     44 |  324 | `	SyHashResetLoopCursor(&pTrait->hAttr);` |
|     60 |  325 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hAttr)) != 0 ){` |
|      - |  326 | `		SyHashEntry *pExisting;` |
|     18 |  327 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     18 |  328 | `		pName = &pAttr->sName;` |
|     18 |  329 | `		pExisting = SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte);` |
|     18 |  330 | `		if( pExisting != 0 ){` |
|      - |  331 | `			/* Attribute already exists. Check if it came from another trait` |
|      - |  332 | `			 * and whether the definitions are compatible (same defaults).` |
|      - |  333 | `			 */` |
|      - |  334 | `			ph7_class **apUsedTraits;` |
|      - |  335 | `			sxu32 nUsed,k;` |
|      5 |  336 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|      5 |  337 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|      5 |  338 | `			for(k = 0; k < nUsed; k++){` |
|      - |  339 | `				ph7_class_attr *pOther;` |
|      3 |  340 | `				pOther = PH7_ClassExtractAttribute(apUsedTraits[k],pName->zString,pName->nByte);` |
|      3 |  341 | `				if( pOther ){` |
|      - |  342 | `					/* Two traits define the same property — check if defaults differ */` |
|      3 |  343 | `					ph7_class_attr *pClassAttr = (ph7_class_attr *)pExisting->pUserData;` |
|      4 |  344 | `					if( SySetUsed(&pAttr->aByteCode) != SySetUsed(&pClassAttr->aByteCode) \|\|` |
|      3 |  345 | `						(SySetUsed(&pAttr->aByteCode) > 0 &&` |
|      3 |  346 | `						 SyMemcmp(SySetBasePtr(&pAttr->aByteCode),SySetBasePtr(&pClassAttr->aByteCode),` |
|      3 |  347 | `							SySetUsed(&pAttr->aByteCode) * SySetElemSize(&pAttr->aByteCode)) != 0) ){` |
|      4 |  348 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine,` |
|      - |  349 | `							"%z and %z define the same property ($%z) in the composition of %z. "` |
|      - |  350 | `							"However, the definition differs and is considered incompatible",` |
|      2 |  351 | `							&apUsedTraits[k]->sName,&pTrait->sName,pName,&pClass->sName);` |
|      3 |  352 | `						if( rc == SXERR_ABORT ){` |
|    ! 0 |  353 | `							goto cleanup;` |
|      - |  354 | `						}` |
|      1 |  355 | `					}` |
|      3 |  356 | `					break;` |
|      - |  357 | `				}` |
|    ! 0 |  358 | `			}` |
|      5 |  359 | `			continue;` |
|      - |  360 | `		}` |
|     14 |  361 | `		rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|     14 |  362 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  363 | `			goto cleanup;` |
|      - |  364 | `		}` |
|      2 |  365 | `	}` |
|      - |  366 | `	/* Copy methods from the trait */` |
|     44 |  367 | `	SyHashResetLoopCursor(&pTrait->hMethod);` |
|     82 |  368 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hMethod)) != 0 ){` |
|     39 |  369 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|     39 |  370 | `		pName = &pMeth->sFunc.sName;` |
|     39 |  371 | `		if( SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte) != 0 ){` |
|      - |  372 | `			/* Method already exists in the class. Check if it came from another trait` |
|      - |  373 | `			 * (unresolved conflict) vs being defined by the class itself.` |
|      - |  374 | `			 */` |
|      - |  375 | `			ph7_class **apUsedTraits;` |
|      - |  376 | `			sxu32 nUsed,k;` |
|      9 |  377 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|      9 |  378 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|      9 |  379 | `			for(k = 0; k < nUsed; k++){` |
|      3 |  380 | `				if( PH7_ClassExtractMethod(apUsedTraits[k],pName->zString,pName->nByte) != 0 ){` |
|      - |  381 | `					/* Two different traits define the same method with no resolution */` |
|      4 |  382 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|      - |  383 | `						"Trait method %z::%z has not been applied as %z::%z, "` |
|      - |  384 | `						"because of collision with %z::%z",` |
|      2 |  385 | `						&pTrait->sName,pName,` |
|      1 |  386 | `						&pClass->sName,pName,` |
|      2 |  387 | `						&apUsedTraits[k]->sName,pName);` |
|      3 |  388 | `					if( rc == SXERR_ABORT ){` |
|    ! 0 |  389 | `						goto cleanup;` |
|      - |  390 | `					}` |
|      3 |  391 | `					break;` |
|      - |  392 | `				}` |
|    ! 0 |  393 | `			}` |
|      - |  394 | `			/* Class-defined method takes precedence */` |
|      9 |  395 | `			continue;` |
|      - |  396 | `		}` |
|     31 |  397 | `		rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|     31 |  398 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  399 | `			goto cleanup;` |
|      - |  400 | `		}` |
|      1 |  401 | `	}` |
|      - |  402 | `	/* Record trait in the class */` |
|     44 |  403 | `	SySetPut(&pClass->aTrait,(const void *)&pTrait);` |
|     21 |  404 | `cleanup:` |
|      - |  405 | `	/* Always clear visiting flag, even on error paths */` |
|     44 |  406 | `	pTrait->iFlags &= ~PH7_CLASS_TRAIT_VISITING;` |
|     21 |  407 | `	SXUNUSED(pGen);` |
|     44 |  408 | `	return rc;` |
|     23 |  409 |  |
|      - |  410 | `/*` |
|      - |  411 | ` * Inherit an object interface from another object interface.` |
|      - |  412 | ` * According to the PHP language reference manual.` |
|      - |  413 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|      - |  414 | ` *  must implement, without having to define how these methods are handled.` |
|      - |  415 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|      - |  416 | ` *  class, but without any of the methods having their contents defined.` |
|      - |  417 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|      - |  418 | ` *` |
|      - |  419 | ` * This function return SXRET_OK if the interface inheritance operation was successfully performed.` |
|      - |  420 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|      - |  421 | ` * error message.` |
|      - |  422 | ` */` |
|      2 |  423 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceInherit(ph7_class *pSub,ph7_class *pBase)` |
|      1 |  424 |  |
|      - |  425 | `	ph7_class_method *pMeth;` |
|      - |  426 | `	ph7_class_attr *pAttr;` |
|      - |  427 | `	SyHashEntry *pEntry;` |
|      - |  428 | `	SyString *pName;` |
|      - |  429 | `	sxi32 rc;` |
|      - |  430 | `	/* Install in the derived hashtable */` |
|      3 |  431 | `	SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|      3 |  432 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
|      - |  433 | `	/* Copy constants */` |
|      6 |  434 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|      - |  435 | `		/* Make sure the constants are not redeclared in the subclass */` |
|      3 |  436 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      3 |  437 | `		pName = &pAttr->sName;` |
|      3 |  438 | `		if( SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte) == 0 ){` |
|      - |  439 | `			/* Install the constant in the subclass */` |
|      3 |  440 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|      3 |  441 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  442 | `				return rc;` |
|      - |  443 | `			}` |
|      1 |  444 | `		}` |
|      1 |  445 | `	}` |
|      3 |  446 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
|      - |  447 | `	/* Copy methods signature */` |
|      6 |  448 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|      - |  449 | `		/* Make sure the method are not redeclared in the subclass */` |
|      3 |  450 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      3 |  451 | `		pName = &pMeth->sFunc.sName;` |
|      3 |  452 | `		if( SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte) == 0 ){` |
|      - |  453 | `			/* Install the method */` |
|      3 |  454 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|      3 |  455 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  456 | `				return rc;` |
|      - |  457 | `			}` |
|      1 |  458 | `		}` |
|      1 |  459 | `	}` |
|      - |  460 | `	/* Mark as subclass */` |
|      3 |  461 | `	pSub->pBase = pBase;` |
|      - |  462 | `	/* All done */` |
|      3 |  463 | `	return SXRET_OK;` |
|      2 |  464 |  |
|      - |  465 | `/*` |
|      - |  466 | ` * Implements an object interface in the given main class.` |
|      - |  467 | ` * According to the PHP language reference manual.` |
|      - |  468 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|      - |  469 | ` *  must implement, without having to define how these methods are handled.` |
|      - |  470 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|      - |  471 | ` *  class, but without any of the methods having their contents defined.` |
|      - |  472 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|      - |  473 | ` *` |
|      - |  474 | ` * This function return SXRET_OK if the interface was successfully implemented.` |
|      - |  475 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|      - |  476 | ` * error message.` |
|      - |  477 | ` */` |
|   2786 |  478 | `PH7_PRIVATE sxi32 PH7_ClassImplement(ph7_class *pMain,ph7_class *pInterface)` |
|      2 |  479 |  |
|      - |  480 | `	ph7_class_attr *pAttr;` |
|      - |  481 | `	SyHashEntry *pEntry;` |
|      - |  482 | `	SyString *pName;` |
|      - |  483 | `	sxi32 rc;` |
|      - |  484 | `	/* First off,copy all constants declared inside the interface */` |
|   2788 |  485 | `	SyHashResetLoopCursor(&pInterface->hAttr);` |
|   4183 |  486 | `	while((pEntry = SyHashGetNextEntry(&pInterface->hAttr)) != 0 ){` |
|      - |  487 | `		/* Point to the constant declaration */` |
|      3 |  488 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      3 |  489 | `		pName = &pAttr->sName;` |
|      - |  490 | `		/* Make sure the attribute is not redeclared in the main class */` |
|      3 |  491 | `		if( SyHashGet(&pMain->hAttr,pName->zString,pName->nByte) == 0 ){` |
|      - |  492 | `			/* Install the attribute */` |
|      3 |  493 | `			rc = SyHashInsert(&pMain->hAttr,pName->zString,pName->nByte,pAttr);` |
|      3 |  494 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  495 | `				return rc;` |
|      - |  496 | `			}` |
|      1 |  497 | `		}` |
|      1 |  498 | `	}` |
|      - |  499 | `	/* Install in the interface container */` |
|   2788 |  500 | `	SySetPut(&pMain->aInterface,(const void *)&pInterface);` |
|      - |  501 | `	/* Install interface method stubs into the implementing class.` |
|      - |  502 | `	 * Methods already defined in the class take precedence (they satisfy` |
|      - |  503 | `	 * the interface contract). Stubs retain PH7_CLASS_ATTR_ABSTRACT so` |
|      - |  504 | `	 * the unified check in GenStateCheckAbstractMethods catches missing ones.` |
|      - |  505 | `	 */` |
|      - |  506 | `	{` |
|      - |  507 | `		ph7_class_method *pMeth;` |
|      - |  508 | `		SyHashEntry *pMEntry;` |
|      - |  509 | `		SyString *pMName;` |
|   2788 |  510 | `		SyHashResetLoopCursor(&pInterface->hMethod);` |
|  17999 |  511 | `		while((pMEntry = SyHashGetNextEntry(&pInterface->hMethod)) != 0 ){` |
|  13820 |  512 | `			pMeth = (ph7_class_method *)pMEntry->pUserData;` |
|  13820 |  513 | `			pMName = &pMeth->sFunc.sName;` |
|  13820 |  514 | `			if( SyHashGet(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte) == 0 ){` |
|     15 |  515 | `				rc = SyHashInsert(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte,pMeth);` |
|     15 |  516 | `				if( rc != SXRET_OK ){` |
|    ! 0 |  517 | `					return rc;` |
|      - |  518 | `				}` |
|      7 |  519 | `			}` |
|      2 |  520 | `		}` |
|      - |  521 | `	}` |
|   2788 |  522 | `	return SXRET_OK;` |
|   1395 |  523 |  |
|      - |  524 | `/*` |
|      - |  525 | ` * Create a class instance [i.e: Object in the PHP jargon] at run-time.` |
|      - |  526 | ` * The following function is called when an object is created at run-time` |
|      - |  527 | ` * typically when the PH7_OP_NEW/PH7_OP_CLONE instructions are executed.` |
|      - |  528 | ` * Notes on object creation.` |
|      - |  529 | ` *` |
|      - |  530 | ` * According to PHP language reference manual.` |
|      - |  531 | ` * To create an instance of a class, the new keyword must be used. An object will always` |
|      - |  532 | ` * be created unless the object has a constructor defined that throws an exception on error.` |
|      - |  533 | ` * Classes should be defined before instantiation (and in some cases this is a requirement).` |
|      - |  534 | ` * If a string containing the name of a class is used with new, a new instance of that class` |
|      - |  535 | ` * will be created. If the class is in a namespace, its fully qualified name must be used when` |
|      - |  536 | ` * doing this.` |
|      - |  537 | ` * Example #3 Creating an instance` |
|      - |  538 | ` * <?php` |
|      - |  539 | ` *  $instance = new SimpleClass();` |
|      - |  540 | ` *   // This can also be done with a variable:` |
|      - |  541 | ` * $className = 'Foo';` |
|      - |  542 | ` * $instance = new $className(); // Foo()` |
|      - |  543 | ` * ?>` |
|      - |  544 | ` * In the class context, it is possible to create a new object by new self and new parent.` |
|      - |  545 | ` * When assigning an already created instance of a class to a new variable, the new variable` |
|      - |  546 | ` * will access the same instance as the object that was assigned. This behaviour is the same` |
|      - |  547 | ` * when passing instances to a function. A copy of an already created object can be made by` |
|      - |  548 | ` * cloning it.` |
|      - |  549 | ` * Example #4 Object Assignment` |
|      - |  550 | ` * <?php` |
|      - |  551 | ` *  class SimpleClass(){` |
|      - |  552 | ` *    public $var;` |
|      - |  553 | ` *  };` |
|      - |  554 | ` *  $instance = new SimpleClass();` |
|      - |  555 | ` *  $assigned   =  $instance;` |
|      - |  556 | ` *  $reference  =& $instance;` |
|      - |  557 | ` *  $instance->var = '$assigned will have this value';` |
|      - |  558 | ` *  $instance = null; // $instance and $reference become null` |
|      - |  559 | ` *  var_dump($instance);` |
|      - |  560 | ` *  var_dump($reference);` |
|      - |  561 | ` *  var_dump($assigned);` |
|      - |  562 | ` * ?>` |
|      - |  563 | ` * The above example will output:` |
|      - |  564 | ` * NULL` |
|      - |  565 | ` * NULL` |
|      - |  566 | ` * object(SimpleClass)#1 (1) {` |
|      - |  567 | ` *  ["var"]=>` |
|      - |  568 | ` *    string(30) "$assigned will have this value"` |
|      - |  569 | ` * }` |
|      - |  570 | ` * Example #5 Creating new objects` |
|      - |  571 | ` * <?php` |
|      - |  572 | ` * class Test` |
|      - |  573 | ` * {` |
|      - |  574 | ` *   static public function getNew()` |
|      - |  575 | ` *   {` |
|      - |  576 | ` *       return new static;` |
|      - |  577 | ` *   }` |
|      - |  578 | ` * }` |
|      - |  579 | ` * class Child extends Test` |
|      - |  580 | ` * {}` |
|      - |  581 | ` * $obj1 = new Test();` |
|      - |  582 | ` * $obj2 = new $obj1;` |
|      - |  583 | ` * var_dump($obj1 !== $obj2);` |
|      - |  584 | ` * $obj3 = Test::getNew();` |
|      - |  585 | ` * var_dump($obj3 instanceof Test);` |
|      - |  586 | ` * $obj4 = Child::getNew();` |
|      - |  587 | ` * var_dump($obj4 instanceof Child);` |
|      - |  588 | ` * ?>` |
|      - |  589 | ` * The above example will output:` |
|      - |  590 | ` * bool(true)` |
|      - |  591 | ` * bool(true)` |
|      - |  592 | ` * bool(true)` |
|      - |  593 | ` * Note that Symisc Systems have introduced powerfull extension to` |
|      - |  594 | ` * OO subsystem. For example a class attribute may have any complex` |
|      - |  595 | ` * expression associated with it when declaring the attribute unlike` |
|      - |  596 | ` * the standard PHP engine which would allow a single value.` |
|      - |  597 | ` * Example:` |
|      - |  598 | ` *  class myClass{` |
|      - |  599 | ` *    public $var = 25<<1+foo()/bar();` |
|      - |  600 | ` *  };` |
|      - |  601 | ` * Refer to the official documentation for more information.` |
|      - |  602 | ` */` |
|   1402 |  603 | `static ph7_class_instance * NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|      2 |  604 |  |
|      - |  605 | `	ph7_class_instance *pThis;` |
|      - |  606 | `	/* Allocate a new instance */` |
|   1404 |  607 | `	pThis = (ph7_class_instance *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_instance));` |
|   1404 |  608 | `	if( pThis == 0 ){` |
|    ! 0 |  609 | `		return 0;` |
|      - |  610 | `	}` |
|      - |  611 | `	/* Zero the structure */` |
|   1404 |  612 | `	SyZero(pThis,sizeof(ph7_class_instance));` |
|      - |  613 | `	/* Initialize fields */` |
|   1404 |  614 | `	pThis->iRef = 1;` |
|   1404 |  615 | `	pThis->pVm = pVm;` |
|   1404 |  616 | `	pThis->pClass = pClass;` |
|   1404 |  617 | `	SyHashInit(&pThis->hAttr,&pVm->sAllocator,0,0);` |
|   1404 |  618 | `	return pThis;` |
|    703 |  619 |  |
|      - |  620 | `/*` |
|      - |  621 | ` * Wrapper around the NewClassInstance() function defined above.` |
|      - |  622 | ` * See the block comment above for more information.` |
|      - |  623 | ` */` |
|   1360 |  624 | `PH7_PRIVATE ph7_class_instance * PH7_NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|      2 |  625 |  |
|      - |  626 | `	ph7_class_instance *pNew;` |
|      - |  627 | `	sxi32 rc;` |
|   1362 |  628 | `	pNew = NewClassInstance(&(*pVm),&(*pClass));` |
|   1362 |  629 | `	if( pNew == 0 ){` |
|    ! 0 |  630 | `		return 0;` |
|      - |  631 | `	}` |
|      - |  632 | `	/* Associate a private VM frame with this class instance */` |
|   1362 |  633 | `	rc = PH7_VmCreateClassInstanceFrame(&(*pVm),pNew);` |
|   1362 |  634 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  635 | `		SyMemBackendPoolFree(&pVm->sAllocator,pNew);` |
|    ! 0 |  636 | `		return 0;` |
|      - |  637 | `	}` |
|   1362 |  638 | `	return pNew;` |
|    682 |  639 |  |
|      - |  640 | `/*` |
|      - |  641 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon] attribute.` |
|      - |  642 | ` * This function never fail.` |
|      - |  643 | ` */` |
|    916 |  644 | `static ph7_value * ExtractClassAttrValue(ph7_vm *pVm,VmClassAttr *pAttr)` |
|      2 |  645 |  |
|      - |  646 | `	/* Extract the value */` |
|      - |  647 | `	ph7_value *pValue;` |
|    918 |  648 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|    918 |  649 | `	return pValue;` |
|      2 |  650 |  |
|      - |  651 | `/*` |
|      - |  652 | ` * Perform a clone operation on a class instance [i.e: Object in the PHP jargon].` |
|      - |  653 | ` * The following function is called when an object is cloned at run-time` |
|      - |  654 | ` * typically when the PH7_OP_CLONE instruction is executed.` |
|      - |  655 | ` * Notes on object cloning.` |
|      - |  656 | ` *` |
|      - |  657 | ` * According to PHP language reference manual.` |
|      - |  658 | ` * Creating a copy of an object with fully replicated properties is not always the wanted behavior.` |
|      - |  659 | ` * A good example of the need for copy constructors. Another example is if your object holds a reference` |
|      - |  660 | ` * to another object which it uses and when you replicate the parent object you want to create` |
|      - |  661 | ` * a new instance of this other object so that the replica has its own separate copy.` |
|      - |  662 | ` * An object copy is created by using the clone keyword (which calls the object's __clone() method if possible).` |
|      - |  663 | ` * An object's __clone() method cannot be called directly.` |
|      - |  664 | ` * $copy_of_object = clone $object;` |
|      - |  665 | ` * When an object is cloned, PHP 5 will perform a shallow copy of all of the object's properties.` |
|      - |  666 | ` * Any properties that are references to other variables, will remain references.` |
|      - |  667 | ` * Once the cloning is complete, if a __clone() method is defined, then the newly created object's __clone() method` |
|      - |  668 | ` * will be called, to allow any necessary properties that need to be changed.` |
|      - |  669 | ` * Example #1 Cloning an object` |
|      - |  670 | ` * <?php` |
|      - |  671 | ` * class SubObject` |
|      - |  672 | ` * {` |
|      - |  673 | ` *   static $instances = 0;` |
|      - |  674 | ` *   public $instance;` |
|      - |  675 | ` *` |
|      - |  676 | ` *   public function __construct() {` |
|      - |  677 | ` *       $this->instance = ++self::$instances;` |
|      - |  678 | ` *   }` |
|      - |  679 | ` *` |
|      - |  680 | ` *   public function __clone() {` |
|      - |  681 | ` *       $this->instance = ++self::$instances;` |
|      - |  682 | ` *   }` |
|      - |  683 | ` * }` |
|      - |  684 | ` *` |
|      - |  685 | ` * class MyCloneable` |
|      - |  686 | ` * {` |
|      - |  687 | ` *   public $object1;` |
|      - |  688 | ` *   public $object2;` |
|      - |  689 | ` *` |
|      - |  690 | ` *   function __clone()` |
|      - |  691 | ` *   {` |
|      - |  692 | ` *       // Force a copy of this->object, otherwise` |
|      - |  693 | ` *       // it will point to same object.` |
|      - |  694 | ` *       $this->object1 = clone $this->object1;` |
|      - |  695 | ` *   }` |
|      - |  696 | ` * }` |
|      - |  697 | ` * $obj = new MyCloneable();` |
|      - |  698 | ` * $obj->object1 = new SubObject();` |
|      - |  699 | ` * $obj->object2 = new SubObject();` |
|      - |  700 | ` * $obj2 = clone $obj;` |
|      - |  701 | ` * print("Original Object:\n");` |
|      - |  702 | ` * print_r($obj);` |
|      - |  703 | ` * print("Cloned Object:\n");` |
|      - |  704 | ` * print_r($obj2);` |
|      - |  705 | ` * ?>` |
|      - |  706 | ` * The above example will output:` |
|      - |  707 | ` * Original Object:` |
|      - |  708 | ` * MyCloneable Object` |
|      - |  709 | ` * (` |
|      - |  710 | ` *   [object1] => SubObject Object` |
|      - |  711 | ` *       (` |
|      - |  712 | ` *           [instance] => 1` |
|      - |  713 | ` *       )` |
|      - |  714 | ` *` |
|      - |  715 | ` *   [object2] => SubObject Object` |
|      - |  716 | ` *       (` |
|      - |  717 | ` *           [instance] => 2` |
|      - |  718 | ` *       )` |
|      - |  719 | ` *` |
|      - |  720 | ` * )` |
|      - |  721 | ` * Cloned Object:` |
|      - |  722 | ` * MyCloneable Object` |
|      - |  723 | ` * (` |
|      - |  724 | ` *   [object1] => SubObject Object` |
|      - |  725 | ` *       (` |
|      - |  726 | ` *           [instance] => 3` |
|      - |  727 | ` *       )` |
|      - |  728 | ` *` |
|      - |  729 | ` *   [object2] => SubObject Object` |
|      - |  730 | ` *       (` |
|      - |  731 | ` *           [instance] => 2` |
|      - |  732 | ` *       )` |
|      - |  733 | ` * )` |
|      - |  734 | ` */` |
|     42 |  735 | `PH7_PRIVATE ph7_class_instance * PH7_CloneClassInstance(ph7_class_instance *pSrc)` |
|      2 |  736 |  |
|      - |  737 | `	ph7_class_instance *pClone;` |
|      - |  738 | `	ph7_class_method *pMethod;` |
|      - |  739 | `	SyHashEntry *pEntry2;` |
|      - |  740 | `	SyHashEntry *pEntry;` |
|      - |  741 | `	ph7_vm *pVm;` |
|      - |  742 | `	sxi32 rc;` |
|      - |  743 | `	/* Allocate a new instance */` |
|     44 |  744 | `	pVm = pSrc->pVm;` |
|     44 |  745 | `	pClone = NewClassInstance(pVm,pSrc->pClass);` |
|     44 |  746 | `	if( pClone == 0 ){` |
|    ! 0 |  747 | `		return 0;` |
|      - |  748 | `	}` |
|      - |  749 | `	/* Associate a private VM frame with this class instance */` |
|     44 |  750 | `	rc = PH7_VmCreateClassInstanceFrame(pVm,pClone);` |
|     44 |  751 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  752 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClone);` |
|    ! 0 |  753 | `		return 0;` |
|      - |  754 | `	}` |
|      - |  755 | `	/* Duplicate object values */` |
|     44 |  756 | `	SyHashResetLoopCursor(&pSrc->hAttr);` |
|     44 |  757 | `	SyHashResetLoopCursor(&pClone->hAttr);` |
|    111 |  758 | `	while((pEntry = SyHashGetNextEntry(&pSrc->hAttr)) != 0 && (pEntry2 = SyHashGetNextEntry(&pClone->hAttr)) != 0 ){` |
|     48 |  759 | `		VmClassAttr *pSrcAttr = (VmClassAttr *)pEntry->pUserData;` |
|     48 |  760 | `		VmClassAttr *pDestAttr = (VmClassAttr *)pEntry2->pUserData;` |
|      - |  761 | `		/* Duplicate non-static attribute */` |
|     48 |  762 | `		if( (pSrcAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|      - |  763 | `			ph7_value *pvSrc,*pvDest;` |
|     48 |  764 | `			pvSrc = ExtractClassAttrValue(pVm,pSrcAttr);` |
|     48 |  765 | `			pvDest = ExtractClassAttrValue(pVm,pDestAttr);` |
|     48 |  766 | `			if( pvSrc && pvDest ){` |
|     48 |  767 | `				PH7_MemObjStore(pvSrc,pvDest);` |
|     23 |  768 | `			}` |
|     23 |  769 | `		}` |
|      2 |  770 | `	}` |
|      - |  771 | `	/* call the __clone method on the cloned object if available */` |
|     44 |  772 | `	pMethod = PH7_ClassExtractMethod(pClone->pClass,"__clone",sizeof("__clone")-1);` |
|     44 |  773 | `	if( pMethod ){` |
|     38 |  774 | `		if( pMethod->iCloneDepth < 16 ){` |
|     36 |  775 | `			pMethod->iCloneDepth++;` |
|     36 |  776 | `			PH7_VmCallClassMethod(pVm,pClone,pMethod,0,0,0);` |
|     19 |  777 | `		}else{` |
|      - |  778 | `			/* Nesting limit reached */` |
|      3 |  779 | `			PH7_VmThrowError(pVm,0,PH7_CTX_ERR,"Object clone limit reached,no more call to __clone()");` |
|      - |  780 | `		}` |
|      - |  781 | `		/* Reset the cursor */` |
|     38 |  782 | `		pMethod->iCloneDepth = 0;` |
|     18 |  783 | `	}` |
|      - |  784 | `	/* Return the cloned object */` |
|     44 |  785 | `	return pClone;` |
|     23 |  786 |  |
|      - |  787 | `#define CLASS_INSTANCE_DESTROYED 0x001 /* Instance is released */` |
|      - |  788 | `/*` |
|      - |  789 | ` * Release a class instance [i.e: Object in the PHP jargon] and invoke any defined destructor.` |
|      - |  790 | ` * This routine is invoked as soon as there are no other references to a particular` |
|      - |  791 | ` * class instance.` |
|      - |  792 | ` */` |
|    974 |  793 | `static void PH7_ClassInstanceRelease(ph7_class_instance *pThis)` |
|      2 |  794 |  |
|      - |  795 | `	ph7_class_method *pDestr;` |
|      - |  796 | `	SyHashEntry *pEntry;` |
|      - |  797 | `	ph7_class *pClass;` |
|      - |  798 | `	ph7_vm *pVm;` |
|    976 |  799 | `	if( pThis->iFlags & CLASS_INSTANCE_DESTROYED ){` |
|      - |  800 | `		/*` |
|      - |  801 | `		 * Already destroyed,return immediately.` |
|      - |  802 | `		 * This could happend if someone perform unset($this) in the destructor body.` |
|      - |  803 | `		 */` |
|    ! 0 |  804 | `		return;` |
|      - |  805 | `	}` |
|      - |  806 | `	/* Mark as destroyed */` |
|    976 |  807 | `	pThis->iFlags \|= CLASS_INSTANCE_DESTROYED;` |
|      - |  808 | `	/* Invoke any defined destructor if available */` |
|    976 |  809 | `	pVm = pThis->pVm;` |
|    976 |  810 | `	pClass = pThis->pClass;` |
|    976 |  811 | `	pDestr = PH7_ClassExtractMethod(pClass,"__destruct",sizeof("__destruct")-1);` |
|    976 |  812 | `	if( pDestr ){` |
|      - |  813 | `		/* Invoke the destructor */` |
|      9 |  814 | `		pThis->iRef = 2; /* Prevent garbage collection */` |
|      9 |  815 | `		PH7_VmCallClassMethod(pVm,pThis,pDestr,0,0,0);` |
|      4 |  816 | `	}` |
|      - |  817 | `	/* Release non-static attributes */` |
|    976 |  818 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|   4868 |  819 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|   3894 |  820 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|   3894 |  821 | `		if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|      - |  822 | `			/* Drop any typed-property enforcement slot registered for this` |
|      - |  823 | `			 * memobj. Must happen before the memobj is returned to the free` |
|      - |  824 | `			 * list so a future recycled slot does not inherit the stale entry. */` |
|   3878 |  825 | `			if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|    115 |  826 | `				SyHashDeleteEntry(&pVm->hTypedSlot,` |
|     76 |  827 | `					(const void *)&pVmAttr->nIdx,sizeof(sxu32),0);` |
|     38 |  828 | `			}` |
|   3878 |  829 | `			PH7_VmUnsetMemObj(pVm,pVmAttr->nIdx,TRUE);` |
|   1938 |  830 | `		}` |
|   3894 |  831 | `		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      2 |  832 | `	}` |
|      - |  833 | `	/* Release the whole structure */` |
|    976 |  834 | `	SyHashRelease(&pThis->hAttr);` |
|    976 |  835 | `	SyMemBackendPoolFree(&pVm->sAllocator,pThis);` |
|    489 |  836 |  |
|      - |  837 | `/*` |
|      - |  838 | ` * Decrement the reference count of a class instance [i.e Object in the PHP jargon].` |
|      - |  839 | ` * If the reference count reaches zero,release the whole instance.` |
|      - |  840 | ` */` |
|  18222 |  841 | `PH7_PRIVATE void PH7_ClassInstanceUnref(ph7_class_instance *pThis)` |
|      2 |  842 |  |
|  18224 |  843 | `	pThis->iRef--;` |
|  18224 |  844 | `	if( pThis->iRef < 1 ){` |
|      - |  845 | `		/* No more reference to this instance */` |
|    976 |  846 | `		PH7_ClassInstanceRelease(&(*pThis));` |
|    487 |  847 | `	}` |
|  18224 |  848 |  |
|      - |  849 | `/*` |
|      - |  850 | ` * Compare two class instances [i.e: Objects in the PHP jargon]` |
|      - |  851 | ` * Note on objects comparison:` |
|      - |  852 | ` *  According to the PHP langauge reference manual` |
|      - |  853 | ` *  When using the comparison operator (==), object variables are compared in a simple manner` |
|      - |  854 | ` *  namely: Two object instances are equal if they have the same attributes and values, and are` |
|      - |  855 | ` *  instances of the same class.` |
|      - |  856 | ` *  On the other hand, when using the identity operator (===), object variables are identical` |
|      - |  857 | ` *  if and only if they refer to the same instance of the same class.` |
|      - |  858 | ` *  An example will clarify these rules.` |
|      - |  859 | ` *  Example #1 Example of object comparison` |
|      - |  860 | ` *  <?php` |
|      - |  861 | ` *    function bool2str($bool)` |
|      - |  862 | ` * {` |
|      - |  863 | ` *   if ($bool === false) {` |
|      - |  864 | ` *       return 'FALSE';` |
|      - |  865 | ` *   } else {` |
|      - |  866 | ` *       return 'TRUE';` |
|      - |  867 | ` *   }` |
|      - |  868 | ` * }` |
|      - |  869 | ` * function compareObjects(&$o1, &$o2)` |
|      - |  870 | ` * {` |
|      - |  871 | ` *   echo 'o1 == o2 : ' . bool2str($o1 == $o2) . "\n";` |
|      - |  872 | ` *   echo 'o1 != o2 : ' . bool2str($o1 != $o2) . "\n";` |
|      - |  873 | ` *   echo 'o1 === o2 : ' . bool2str($o1 === $o2) . "\n";` |
|      - |  874 | ` *   echo 'o1 !== o2 : ' . bool2str($o1 !== $o2) . "\n";` |
|      - |  875 | ` * }` |
|      - |  876 | ` * class Flag` |
|      - |  877 | ` * {` |
|      - |  878 | ` *   public $flag;` |
|      - |  879 | ` *` |
|      - |  880 | ` *   function Flag($flag = true) {` |
|      - |  881 | ` *       $this->flag = $flag;` |
|      - |  882 | ` *   }` |
|      - |  883 | ` * }` |
|      - |  884 | ` *` |
|      - |  885 | ` * class OtherFlag` |
|      - |  886 | ` * {` |
|      - |  887 | ` *   public $flag;` |
|      - |  888 | ` *` |
|      - |  889 | ` *   function OtherFlag($flag = true) {` |
|      - |  890 | ` *       $this->flag = $flag;` |
|      - |  891 | ` *   }` |
|      - |  892 | ` * }` |
|      - |  893 | ` *` |
|      - |  894 | ` * $o = new Flag();` |
|      - |  895 | ` * $p = new Flag();` |
|      - |  896 | ` * $q = $o;` |
|      - |  897 | ` * $r = new OtherFlag();` |
|      - |  898 | ` *` |
|      - |  899 | ` * echo "Two instances of the same class\n";` |
|      - |  900 | ` * compareObjects($o, $p);` |
|      - |  901 | ` * echo "\nTwo references to the same instance\n";` |
|      - |  902 | ` * compareObjects($o, $q);` |
|      - |  903 | ` * echo "\nInstances of two different classes\n";` |
|      - |  904 | ` * compareObjects($o, $r);` |
|      - |  905 | ` * ?>` |
|      - |  906 | ` * The above example will output:` |
|      - |  907 | ` * Two instances of the same class` |
|      - |  908 | ` * o1 == o2 : TRUE` |
|      - |  909 | ` * o1 != o2 : FALSE` |
|      - |  910 | ` * o1 === o2 : FALSE` |
|      - |  911 | ` * o1 !== o2 : TRUE` |
|      - |  912 | ` * Two references to the same instance` |
|      - |  913 | ` * o1 == o2 : TRUE` |
|      - |  914 | ` * o1 != o2 : FALSE` |
|      - |  915 | ` * o1 === o2 : TRUE` |
|      - |  916 | ` * o1 !== o2 : FALSE` |
|      - |  917 | ` * Instances of two different classes` |
|      - |  918 | ` * o1 == o2 : FALSE` |
|      - |  919 | ` * o1 != o2 : TRUE` |
|      - |  920 | ` * o1 === o2 : FALSE` |
|      - |  921 | ` * o1 !== o2 : TRUE` |
|      - |  922 | ` *` |
|      - |  923 | ` * This function return 0 if the objects are equals according to the comprison rules defined above.` |
|      - |  924 | ` * Any other return values indicates difference.` |
|      - |  925 | ` */` |
|    160 |  926 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest)` |
|      2 |  927 |  |
|      - |  928 | `	SyHashEntry *pEntry,*pEntry2;` |
|      - |  929 | `	ph7_value sV1,sV2;` |
|      - |  930 | `	sxi32 rc;` |
|    162 |  931 | `	if( iNest > 31 ){` |
|      - |  932 | `		/* Nesting limit reached */` |
|      5 |  933 | `		PH7_VmThrowError(pLeft->pVm,0,PH7_CTX_ERR,"Nesting limit reached: Infinite recursion?");` |
|      5 |  934 | `		return 1;` |
|      - |  935 | `	}` |
|      - |  936 | `	/* Comparison is performed only if the objects are instance of the same class */` |
|    158 |  937 | `	if( pLeft->pClass != pRight->pClass ){` |
|      7 |  938 | `		return 1;` |
|      - |  939 | `	}` |
|    152 |  940 | `	if( bStrict ){` |
|      - |  941 | `		/*` |
|      - |  942 | `		 * According to the PHP language reference manual:` |
|      - |  943 | `		 *  when using the identity operator (===), object variables` |
|      - |  944 | `		 *  are identical if and only if they refer to the same instance` |
|      - |  945 | `		 *  of the same class.` |
|      - |  946 | `		 */` |
|     11 |  947 | `		return !(pLeft == pRight);` |
|      - |  948 | `	}` |
|      - |  949 | `	/*` |
|      - |  950 | `	 * Attribute comparison.` |
|      - |  951 | `	 * According to the PHP reference manual:` |
|      - |  952 | `	 *  When using the comparison operator (==), object variables are compared` |
|      - |  953 | `	 *  in a simple manner, namely: Two object instances are equal if they have` |
|      - |  954 | `	 *  the same attributes and values, and are instances of the same class.` |
|      - |  955 | `	 */` |
|    142 |  956 | `	if( pLeft == pRight ){` |
|      - |  957 | `		/* Same instance,don't bother processing,object are equals */` |
|      3 |  958 | `		return 0;` |
|      - |  959 | `	}` |
|    140 |  960 | `	SyHashResetLoopCursor(&pLeft->hAttr);` |
|    140 |  961 | `	SyHashResetLoopCursor(&pRight->hAttr);` |
|    140 |  962 | `	PH7_MemObjInit(pLeft->pVm,&sV1);` |
|    140 |  963 | `	PH7_MemObjInit(pLeft->pVm,&sV2);` |
|    140 |  964 | `	sV1.nIdx = sV2.nIdx = SXU32_HIGH;` |
|    223 |  965 | `	while((pEntry = SyHashGetNextEntry(&pLeft->hAttr)) != 0 && (pEntry2 = SyHashGetNextEntry(&pRight->hAttr)) != 0 ){` |
|    146 |  966 | `		VmClassAttr *p1 = (VmClassAttr *)pEntry->pUserData;` |
|    146 |  967 | `		VmClassAttr *p2 = (VmClassAttr *)pEntry2->pUserData;` |
|      - |  968 | `		/* Compare only non-static attribute */` |
|    146 |  969 | `		if( (p1->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|      - |  970 | `			ph7_value *pL,*pR;` |
|    146 |  971 | `			pL = ExtractClassAttrValue(pLeft->pVm,p1);` |
|    146 |  972 | `			pR = ExtractClassAttrValue(pRight->pVm,p2);` |
|    146 |  973 | `			if( pL && pR ){` |
|    146 |  974 | `				PH7_MemObjLoad(pL,&sV1);` |
|    146 |  975 | `				PH7_MemObjLoad(pR,&sV2);` |
|      - |  976 | `				/* Compare the two values now */` |
|    146 |  977 | `				rc = PH7_MemObjCmp(&sV1,&sV2,bStrict,iNest+1);` |
|    146 |  978 | `				PH7_MemObjRelease(&sV1);` |
|    146 |  979 | `				PH7_MemObjRelease(&sV2);` |
|    146 |  980 | `				if( rc != 0 ){` |
|      - |  981 | `					/* Not equals */` |
|    132 |  982 | `					return rc;` |
|      - |  983 | `				}` |
|      7 |  984 | `			}` |
|      7 |  985 | `		}` |
|      1 |  986 | `	}` |
|      - |  987 | `	/* Object are equals */` |
|      9 |  988 | `	return 0;` |
|     82 |  989 |  |
|      - |  990 | `/*` |
|      - |  991 | ` * Dump a class instance and the store the dump in the BLOB given` |
|      - |  992 | ` * as the first argument.` |
|      - |  993 | ` * Note that only non-static/non-constants attribute are dumped.` |
|      - |  994 | ` * This function is typically invoked when the user issue a call` |
|      - |  995 | ` * to [var_dump(),var_export(),print_r(),...].` |
|      - |  996 | ` * This function SXRET_OK on success. Any other return value including` |
|      - |  997 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|      - |  998 | ` */` |
|    132 |  999 | `PH7_PRIVATE sxi32 PH7_ClassInstanceDump(SyBlob *pOut,ph7_class_instance *pThis,int ShowType,int nTab,int nDepth)` |
|      1 | 1000 |  |
|      - | 1001 | `	SyHashEntry *pEntry;` |
|      - | 1002 | `	ph7_value *pValue;` |
|      - | 1003 | `	sxi32 rc;` |
|      - | 1004 | `	int i;` |
|    133 | 1005 | `	if( nDepth > 31 ){` |
|      - | 1006 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|      - | 1007 | `		/* Nesting limit reached..halt immediately*/` |
|      5 | 1008 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|      5 | 1009 | `		if( ShowType ){` |
|      5 | 1010 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|      2 | 1011 | `		}` |
|      5 | 1012 | `		return SXERR_LIMIT;` |
|      - | 1013 | `	}` |
|    129 | 1014 | `	rc = SXRET_OK;` |
|    129 | 1015 | `	if( !ShowType ){` |
|      3 | 1016 | `		SyBlobAppend(&(*pOut),"Object(",sizeof("Object(")-1);` |
|      1 | 1017 | `	}` |
|      - | 1018 | `	/* Append class name */` |
|    129 | 1019 | `	SyBlobFormat(&(*pOut),"%z) {",&pThis->pClass->sName);` |
|      - | 1020 | `#ifdef __WINNT__` |
|      1 | 1021 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|      - | 1022 | `#else` |
|    128 | 1023 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      - | 1024 | `#endif` |
|      - | 1025 | `	/* Dump object attributes */` |
|    129 | 1026 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|    201 | 1027 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|    133 | 1028 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    133 | 1029 | `		if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|      - | 1030 | `			/* Dump non-static/constant attribute only */` |
|   3985 | 1031 | `			for( i = 0 ; i < nTab ; i++ ){` |
|   3853 | 1032 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|   1927 | 1033 | `			}` |
|    133 | 1034 | `			pValue = ExtractClassAttrValue(pThis->pVm,pVmAttr);` |
|    133 | 1035 | `			if( pValue ){` |
|    133 | 1036 | `				SyBlobFormat(&(*pOut),"['%z'] =>",&pVmAttr->pAttr->sName);` |
|      - | 1037 | `#ifdef __WINNT__` |
|      1 | 1038 | `				SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|      - | 1039 | `#else` |
|    132 | 1040 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      - | 1041 | `#endif` |
|    133 | 1042 | `				rc = PH7_MemObjDump(&(*pOut),pValue,ShowType,nTab+1,nDepth,0);` |
|    133 | 1043 | `				if( rc == SXERR_LIMIT ){` |
|    125 | 1044 | `					break;` |
|      - | 1045 | `				}` |
|      4 | 1046 | `			}` |
|      4 | 1047 | `		}` |
|      1 | 1048 | `	}` |
|   3977 | 1049 | `	for( i = 0 ; i < nTab ; i++ ){` |
|   3849 | 1050 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|   1925 | 1051 | `	}` |
|    129 | 1052 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|    129 | 1053 | `	return rc;` |
|     67 | 1054 |  |
|      - | 1055 | `/*` |
|      - | 1056 | ` * Call a magic method [i.e: __toString(),__toBool(),__Invoke()...]` |
|      - | 1057 | ` * Return SXRET_OK on successfull call. Any other return value indicates failure.` |
|      - | 1058 | ` * Notes on magic methods.` |
|      - | 1059 | ` * According to the PHP language reference manual.` |
|      - | 1060 | ` *  The function names __construct(), __destruct(), __call(), __callStatic()` |
|      - | 1061 | ` *  __get(),  __toString(), __invoke(), __clone() are magical in PHP classes.` |
|      - | 1062 | ` * You cannot have functions with these names in any of your classes unless` |
|      - | 1063 | ` * you want the magic functionality associated with them.` |
|      - | 1064 | ` * Example of magical methods:` |
|      - | 1065 | ` * __toString()` |
|      - | 1066 | ` *  The __toString() method allows a class to decide how it will react when it is treated like` |
|      - | 1067 | ` *  a string. For example, what echo $obj; will print. This method must return a string.` |
|      - | 1068 | ` *  Example #2 Simple example` |
|      - | 1069 | ` * <?php` |
|      - | 1070 | ` * // Declare a simple class` |
|      - | 1071 | ` * class TestClass` |
|      - | 1072 | ` * {` |
|      - | 1073 | ` *   public $foo;` |
|      - | 1074 | ` *` |
|      - | 1075 | ` *   public function __construct($foo)` |
|      - | 1076 | ` *   {` |
|      - | 1077 | ` *       $this->foo = $foo;` |
|      - | 1078 | ` *   }` |
|      - | 1079 | ` *` |
|      - | 1080 | ` *   public function __toString()` |
|      - | 1081 | ` *   {` |
|      - | 1082 | ` *       return $this->foo;` |
|      - | 1083 | ` *   }` |
|      - | 1084 | ` * }` |
|      - | 1085 | ` * $class = new TestClass('Hello');` |
|      - | 1086 | ` * echo $class;` |
|      - | 1087 | ` * ?>` |
|      - | 1088 | ` * The above example will output:` |
|      - | 1089 | ` *  Hello` |
|      - | 1090 | ` *` |
|      - | 1091 | ` * Note that PH7 does not support all the magical method and introudces __toFloat(),__toInt()` |
|      - | 1092 | ` * which have the same behaviour as __toString() but for float and integer types` |
|      - | 1093 | ` * respectively.` |
|      - | 1094 | ` * Refer to the official documentation for more information.` |
|      - | 1095 | ` */` |
|      4 | 1096 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallMagicMethod(` |
|      - | 1097 | `	ph7_vm *pVm,               /* VM that own all this stuff */` |
|      - | 1098 | `	ph7_class *pClass,         /* Target class */` |
|      - | 1099 | `	ph7_class_instance *pThis, /* Target object */` |
|      - | 1100 | `	const char *zMethod,       /* Magic method name [i.e: __toString()]*/` |
|      - | 1101 | `	sxu32 nByte,               /* zMethod length*/` |
|      - | 1102 | `	const SyString *pAttrName  /* Attribute name */` |
|      - | 1103 | `	)` |
|      2 | 1104 |  |
|      6 | 1105 | `	ph7_value *apArg[2] = { 0 , 0 };` |
|      - | 1106 | `	ph7_class_method *pMeth;` |
|      - | 1107 | `	ph7_value sAttr; /* cc warning */` |
|      - | 1108 | `	sxi32 rc;` |
|      - | 1109 | `	int nArg;` |
|      - | 1110 | `	/* Make sure the magic method is available */` |
|      6 | 1111 | `	pMeth = PH7_ClassExtractMethod(&(*pClass),zMethod,nByte);` |
|      6 | 1112 | `	if( pMeth == 0 ){` |
|      - | 1113 | `		/* No such method,return immediately */` |
|      3 | 1114 | `		return SXERR_NOTFOUND;` |
|      - | 1115 | `	}` |
|      3 | 1116 | `	nArg = 0;` |
|      - | 1117 | `	/* Copy arguments */` |
|      3 | 1118 | `	if( pAttrName ){` |
|    ! 0 | 1119 | `		PH7_MemObjInitFromString(pVm,&sAttr,pAttrName);` |
|    ! 0 | 1120 | `		sAttr.nIdx = SXU32_HIGH; /* Mark as constant */` |
|    ! 0 | 1121 | `		apArg[0] = &sAttr;` |
|    ! 0 | 1122 | `		nArg = 1;` |
|    ! 0 | 1123 | `	}` |
|      - | 1124 | `	/* Call the magic method now */` |
|      3 | 1125 | `	rc = PH7_VmCallClassMethod(pVm,&(*pThis),pMeth,0,nArg,apArg);` |
|      - | 1126 | `	/* Clean up */` |
|      3 | 1127 | `	if( pAttrName ){` |
|    ! 0 | 1128 | `		PH7_MemObjRelease(&sAttr);` |
|    ! 0 | 1129 | `	}` |
|      3 | 1130 | `	return rc;` |
|      4 | 1131 |  |
|      - | 1132 | `/*` |
|      - | 1133 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon].` |
|      - | 1134 | ` * This function is simply a wrapper on ExtractClassAttrValue().` |
|      - | 1135 | ` */` |
|     18 | 1136 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceExtractAttrValue(ph7_class_instance *pThis,VmClassAttr *pAttr)` |
|      1 | 1137 |  |
|      - | 1138 | `   /* Extract the attribute value */` |
|      - | 1139 | `	ph7_value *pValue;` |
|     19 | 1140 | `	pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     19 | 1141 | `	return pValue;` |
|      1 | 1142 |  |
|      - | 1143 | `/*` |
|      - | 1144 | ` * Convert a class instance [i.e: Object in the PHP jargon] into a hashmap [i.e: array in the PHP jargon].` |
|      - | 1145 | ` * Return SXRET_OK on success. Any other value indicates failure.` |
|      - | 1146 | ` * Note on object conversion to array:` |
|      - | 1147 | ` *  Acccording to the PHP language reference manual` |
|      - | 1148 | ` *  If an object is converted to an array, the result is an array whose elements are the object's properties.` |
|      - | 1149 | ` *  The keys are the member variable names.` |
|      - | 1150 | ` *` |
|      - | 1151 | ` *  The following example:` |
|      - | 1152 | ` *  class Test {` |
|      - | 1153 | ` *   public $A = 25<<1;  // 50` |
|      - | 1154 | ` *	 public $c = rand_str(3);   // Random string of length 3` |
|      - | 1155 | ` *	 public $d = rand() & 1023; // Random number between 0..1023` |
|      - | 1156 | ` *  }` |
|      - | 1157 | ` *  var_dump((array) new Test());` |
|      - | 1158 | ` *	Will output:` |
|      - | 1159 | ` *  array(3) {` |
|      - | 1160 | ` *   [A] =>` |
|      - | 1161 | ` *      int(50)` |
|      - | 1162 | ` *   [c] =>` |
|      - | 1163 | ` *     string(3 'aps')` |
|      - | 1164 | ` *   [d] =>` |
|      - | 1165 | ` *     int(991)` |
|      - | 1166 | ` *  }` |
|      - | 1167 | ` * You have noticed that PH7 allow class attributes [i.e: $a,$c,$d in the example above]` |
|      - | 1168 | ` * have any complex expression (even function calls/Annonymous functions) as their default` |
|      - | 1169 | ` * value unlike the standard PHP engine.` |
|      - | 1170 | ` * This is a very powerful feature that you have to look at.` |
|      - | 1171 | ` */` |
|      6 | 1172 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|      1 | 1173 |  |
|      - | 1174 | `	SyHashEntry *pEntry;` |
|      - | 1175 | `	SyString *pAttrName;` |
|      - | 1176 | `	VmClassAttr *pAttr;` |
|      - | 1177 | `	ph7_value *pValue;` |
|      - | 1178 | `	ph7_value sName;` |
|      - | 1179 | `	/* Reset the loop cursor */` |
|      7 | 1180 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      7 | 1181 | `	PH7_MemObjInitFromString(pThis->pVm,&sName,0);` |
|     20 | 1182 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|      - | 1183 | `		/* Point to the current attribute */` |
|     11 | 1184 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1185 | `		/* Extract attribute value */` |
|     11 | 1186 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     11 | 1187 | `		if( pValue ){` |
|      - | 1188 | `			/* Build attribute name */` |
|     11 | 1189 | `			pAttrName = &pAttr->pAttr->sName;` |
|     11 | 1190 | `			PH7_MemObjStringAppend(&sName,pAttrName->zString,pAttrName->nByte);` |
|      - | 1191 | `			/* Perform the insertion */` |
|     11 | 1192 | `			PH7_HashmapInsert(pMap,&sName,pValue);` |
|      - | 1193 | `			/* Reset the string cursor */` |
|     11 | 1194 | `			SyBlobReset(&sName.sBlob);` |
|      5 | 1195 | `		}` |
|      1 | 1196 | `	}` |
|      7 | 1197 | `	PH7_MemObjRelease(&sName);` |
|      7 | 1198 | `	return SXRET_OK;` |
|      1 | 1199 |  |
|      - | 1200 | `/*` |
|      - | 1201 | ` * Iterate throw class attributes and invoke the given callback [i.e: xWalk()] for each` |
|      - | 1202 | ` * retrieved attribute.` |
|      - | 1203 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|      - | 1204 | ` * the attribute value in the callback body will not alter the real attribute value.` |
|      - | 1205 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|      - | 1206 | ` * a value different from PH7_OK.` |
|      - | 1207 | ` * Refer to [ph7_object_walk()] for more information.` |
|      - | 1208 | ` */` |
|    ! 0 | 1209 | `PH7_PRIVATE sxi32 PH7_ClassInstanceWalk(` |
|      - | 1210 | `	ph7_class_instance *pThis, /* Target object */` |
|      - | 1211 | `	int (*xWalk)(const char *,ph7_value *,void *), /* Walker callback */` |
|      - | 1212 | `	void *pUserData /* Last argument to xWalk() */` |
|      - | 1213 | `	)` |
|    ! 0 | 1214 |  |
|      - | 1215 | `	SyHashEntry *pEntry; /* Hash entry */` |
|      - | 1216 | `	VmClassAttr *pAttr;  /* Pointer to the attribute */` |
|      - | 1217 | `	ph7_value *pValue;   /* Attribute value */` |
|      - | 1218 | `	ph7_value sValue;    /* Copy of the attribute value */` |
|      - | 1219 | `	int rc;` |
|      - | 1220 | `	/* Reset the loop cursor */` |
|    ! 0 | 1221 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|    ! 0 | 1222 | `	PH7_MemObjInit(pThis->pVm,&sValue);` |
|      - | 1223 | `	/* Start the walk process */` |
|    ! 0 | 1224 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|      - | 1225 | `		/* Point to the current attribute */` |
|    ! 0 | 1226 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1227 | `		/* Extract attribute value */` |
|    ! 0 | 1228 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|    ! 0 | 1229 | `		if( pValue ){` |
|    ! 0 | 1230 | `			PH7_MemObjLoad(pValue,&sValue);` |
|      - | 1231 | `			/* Invoke the supplied callback */` |
|    ! 0 | 1232 | `			rc =  xWalk(SyStringData(&pAttr->pAttr->sName),&sValue,pUserData);` |
|    ! 0 | 1233 | `			PH7_MemObjRelease(&sValue);` |
|    ! 0 | 1234 | `			if( rc != PH7_OK){` |
|      - | 1235 | `				/* User callback request an operation abort */` |
|    ! 0 | 1236 | `				return SXERR_ABORT;` |
|      - | 1237 | `			}` |
|    ! 0 | 1238 | `		}` |
|    ! 0 | 1239 | `	}` |
|      - | 1240 | `	/* All done */` |
|    ! 0 | 1241 | `	return SXRET_OK;` |
|    ! 0 | 1242 |  |
|      - | 1243 | `/*` |
|      - | 1244 | ` * Extract a class atrribute value.` |
|      - | 1245 | ` * Return a pointer to the attribute value on success. Otherwise NULL.` |
|      - | 1246 | ` * Note:` |
|      - | 1247 | ` *  Access to static and constant attribute is not allowed. That is,the function` |
|      - | 1248 | ` *  will return NULL in case someone (host-application code) try to extract` |
|      - | 1249 | ` *  a static/constant attribute.` |
|      - | 1250 | ` */` |
|    376 | 1251 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceFetchAttr(ph7_class_instance *pThis,const SyString *pName)` |
|      2 | 1252 |  |
|      - | 1253 | `	SyHashEntry *pEntry;` |
|      - | 1254 | `	VmClassAttr *pAttr;` |
|      - | 1255 | `	/* Query the attribute hashtable */` |
|    378 | 1256 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);` |
|    378 | 1257 | `	if( pEntry == 0 ){` |
|      - | 1258 | `		/* No such attribute */` |
|    ! 0 | 1259 | `		return 0;` |
|      - | 1260 | `	}` |
|      - | 1261 | `	/* Point to the class atrribute */` |
|    378 | 1262 | `	pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1263 | `	/* Check if we are dealing with a static/constant attribute */` |
|    378 | 1264 | `	if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|      - | 1265 | `		/* Access is forbidden */` |
|    ! 0 | 1266 | `		return 0;` |
|      - | 1267 | `	}` |
|      - | 1268 | `	/* Return the attribute value */` |
|    378 | 1269 | `	return ExtractClassAttrValue(pThis->pVm,pAttr);` |
|    190 | 1270 |  |
|      - | 1271 |  |
