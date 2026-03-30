# src/ph7/oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 417/483 lines (86.34%)

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
|  30258 |   14 | `PH7_PRIVATE ph7_class * PH7_NewRawClass(ph7_vm *pVm,const SyString *pName,sxu32 nLine)` |
|      2 |   15 |  |
|      - |   16 | `	ph7_class *pClass;` |
|      - |   17 | `	char *zName;` |
|      - |   18 | `	/* Allocate a new instance */` |
|  30260 |   19 | `	pClass = (ph7_class *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class));` |
|  30260 |   20 | `	if( pClass == 0 ){` |
|    ! 0 |   21 | `		return 0;` |
|      - |   22 | `	}` |
|      - |   23 | `	/* Zero the structure */` |
|  30260 |   24 | `	SyZero(pClass,sizeof(ph7_class));` |
|      - |   25 | `	/* Duplicate class name */` |
|  30260 |   26 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  30260 |   27 | `	if( zName == 0 ){` |
|    ! 0 |   28 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClass);` |
|    ! 0 |   29 | `		return 0;` |
|      - |   30 | `	}` |
|      - |   31 | `	/* Initialize fields */` |
|  30260 |   32 | `	SyStringInitFromBuf(&pClass->sName,zName,pName->nByte);` |
|  30260 |   33 | `	SyHashInit(&pClass->hMethod,&pVm->sAllocator,0,0);` |
|  30260 |   34 | `	SyHashInit(&pClass->hAttr,&pVm->sAllocator,0,0);` |
|  30260 |   35 | `	SyHashInit(&pClass->hDerived,&pVm->sAllocator,0,0);` |
|  30260 |   36 | `	SySetInit(&pClass->aInterface,&pVm->sAllocator,sizeof(ph7_class *));` |
|  30260 |   37 | `	SySetInit(&pClass->aTrait,&pVm->sAllocator,sizeof(ph7_class *));` |
|  30260 |   38 | `	pClass->nLine = nLine;` |
|      - |   39 | `	/* All done */` |
|  30260 |   40 | `	return pClass;` |
|  15131 |   41 |  |
|      - |   42 | `/*` |
|      - |   43 | ` * Allocate and initialize a new class attribute.` |
|      - |   44 | ` * Return a pointer to the class attribute on success. NULL otherwise.` |
|      - |   45 | ` */` |
|  25072 |   46 | `PH7_PRIVATE ph7_class_attr * PH7_NewClassAttr(ph7_vm *pVm,const SyString *pName,sxu32 nLine,sxi32 iProtection,sxi32 iFlags)` |
|      2 |   47 |  |
|      - |   48 | `	ph7_class_attr *pAttr;` |
|      - |   49 | `	char *zName;` |
|  25074 |   50 | `	pAttr = (ph7_class_attr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_attr));` |
|  25074 |   51 | `	if( pAttr == 0 ){` |
|    ! 0 |   52 | `		return 0;` |
|      - |   53 | `	}` |
|      - |   54 | `	/* Zero the structure */` |
|  25074 |   55 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|      - |   56 | `	/* Duplicate attribute name */` |
|  25074 |   57 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  25074 |   58 | `	if( zName == 0 ){` |
|    ! 0 |   59 | `		SyMemBackendPoolFree(&pVm->sAllocator,pAttr);` |
|    ! 0 |   60 | `		return 0;` |
|      - |   61 | `	}` |
|      - |   62 | `	/* Initialize fields */` |
|  25074 |   63 | `	SySetInit(&pAttr->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|  25074 |   64 | `	SyStringInitFromBuf(&pAttr->sName,zName,pName->nByte);` |
|  25074 |   65 | `	pAttr->iProtection = iProtection;` |
|  25074 |   66 | `	pAttr->nIdx = SXU32_HIGH;` |
|  25074 |   67 | `	pAttr->iFlags = iFlags;` |
|  25074 |   68 | `	pAttr->nLine = nLine;` |
|  25074 |   69 | `	return pAttr;` |
|  12538 |   70 |  |
|      - |   71 | `/*` |
|      - |   72 | ` * Allocate and initialize a new class method.` |
|      - |   73 | ` * Return a pointer to the class method on success. NULL otherwise` |
|      - |   74 | ` * This function associate with the newly created method an automatically generated` |
|      - |   75 | ` * random unique name.` |
|      - |   76 | ` */` |
|  72484 |   77 | `PH7_PRIVATE ph7_class_method * PH7_NewClassMethod(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,sxu32 nLine,` |
|      - |   78 | `	sxi32 iProtection,sxi32 iFlags,sxi32 iFuncFlags)` |
|      2 |   79 |  |
|      - |   80 | `	ph7_class_method *pMeth;` |
|      - |   81 | `	SyHashEntry *pEntry;` |
|      - |   82 | `	SyString *pNamePtr;` |
|      - |   83 | `	char zSalt[10];` |
|      - |   84 | `	char *zName;` |
|      - |   85 | `	sxu32 nByte;` |
|      - |   86 | `	/* Allocate a new class method instance */` |
|  72486 |   87 | `	pMeth = (ph7_class_method *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_method));` |
|  72486 |   88 | `	if( pMeth == 0 ){` |
|    ! 0 |   89 | `		return 0;` |
|      - |   90 | `	}` |
|      - |   91 | `	/* Zero the structure */` |
|  72486 |   92 | `	SyZero(pMeth,sizeof(ph7_class_method));` |
|      - |   93 | `	/* Check for an already installed method with the same name */` |
|  72486 |   94 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
|  72486 |   95 | `	if( pEntry == 0 ){` |
|      - |   96 | `		/* Associate an unique VM name to this method */` |
|  72484 |   97 | `		nByte = sizeof(zSalt) + pName->nByte + SyStringLength(&pClass->sName)+sizeof(char)*7/*[[__'\0'*/;` |
|  72484 |   98 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,nByte);` |
|  72484 |   99 | `		if( zName == 0 ){` |
|    ! 0 |  100 | `			SyMemBackendPoolFree(&pVm->sAllocator,pMeth);` |
|    ! 0 |  101 | `			return 0;` |
|      - |  102 | `		}` |
|  72484 |  103 | `		pNamePtr = &pMeth->sVmName;` |
|      - |  104 | `		/* Generate a random string */` |
|  72484 |  105 | `		PH7_VmRandomString(&(*pVm),zSalt,sizeof(zSalt));` |
|  72484 |  106 | `		pNamePtr->nByte = SyBufferFormat(zName,nByte,"[__%z@%z_%.*s]",&pClass->sName,pName,sizeof(zSalt),zSalt);` |
|  72484 |  107 | `		pNamePtr->zString = zName;` |
|  36243 |  108 | `	}else{` |
|      - |  109 | `		/* Method is condidate for 'overloading' */` |
|      3 |  110 | `		ph7_class_method *pCurrent = (ph7_class_method *)pEntry->pUserData;` |
|      3 |  111 | `		pNamePtr = &pMeth->sVmName;` |
|      - |  112 | `		/* Use the same VM name */` |
|      3 |  113 | `		SyStringDupPtr(pNamePtr,&pCurrent->sVmName);` |
|      3 |  114 | `		zName = (char *)pNamePtr->zString;` |
|      - |  115 | `	}` |
|  72486 |  116 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|     23 |  117 | `		if( (pName->nByte == sizeof("__construct") - 1 && SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1 ) == 0)` |
|     15 |  118 | `			\|\| (pName->nByte == sizeof("__destruct") - 1 && SyMemcmp(pName->zString,"__destruct",sizeof("__destruct") - 1 ) == 0)` |
|     15 |  119 | `			\|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|      - |  120 | `				/* Switch to public visibility when dealing with constructor/destructor */` |
|      5 |  121 | `				iProtection = PH7_CLASS_PROT_PUBLIC;` |
|      2 |  122 | `		}` |
|      9 |  123 | `	}` |
|      - |  124 | `	/* Initialize method fields */` |
|  72488 |  125 | `	pMeth->iProtection = iProtection;` |
|  72488 |  126 | `	pMeth->iFlags = iFlags;` |
|  72488 |  127 | `	pMeth->nLine = nLine;` |
| 108732 |  128 | `	PH7_VmInitFuncState(&(*pVm),&pMeth->sFunc,&zName[sizeof(char)*4/*[__@*/+SyStringLength(&pClass->sName)],` |
|  72486 |  129 | `		pName->nByte,iFuncFlags\|VM_FUNC_CLASS_METHOD,pClass);` |
|  72488 |  130 | `	return pMeth;` |
|  36246 |  131 |  |
|      - |  132 | `/*` |
|      - |  133 | ` * Check if the given name have a class method associated with it.` |
|      - |  134 | ` * Return the desired method [i.e: ph7_class_method instance] on success. NULL otherwise.` |
|      - |  135 | ` */` |
|   4988 |  136 | `PH7_PRIVATE ph7_class_method * PH7_ClassExtractMethod(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|      2 |  137 |  |
|      - |  138 | `	SyHashEntry *pEntry;` |
|      - |  139 | `	/* Perform a hash lookup */` |
|   4990 |  140 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)zName,nByte);` |
|   4990 |  141 | `	if( pEntry == 0 ){` |
|      - |  142 | `		/* No such entry */` |
|   1826 |  143 | `		return 0;` |
|      - |  144 | `	}` |
|      - |  145 | `	/* Point to the desired method */` |
|   3166 |  146 | `	return (ph7_class_method *)pEntry->pUserData;` |
|   2496 |  147 |  |
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
|  25072 |  168 | `PH7_PRIVATE sxi32 PH7_ClassInstallAttr(ph7_class *pClass,ph7_class_attr *pAttr)` |
|      2 |  169 |  |
|  25074 |  170 | `	SyString *pName = &pAttr->sName;` |
|      - |  171 | `	sxi32 rc;` |
|  25074 |  172 | `	rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|  25074 |  173 | `	return rc;` |
|      2 |  174 |  |
|      - |  175 | `/*` |
|      - |  176 | ` * Install a class method in the corresponding container.` |
|      - |  177 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|      - |  178 | ` */` |
|  72482 |  179 | `PH7_PRIVATE sxi32 PH7_ClassInstallMethod(ph7_class *pClass,ph7_class_method *pMeth)` |
|      2 |  180 |  |
|  72484 |  181 | `	SyString *pName = &pMeth->sFunc.sName;` |
|      - |  182 | `	sxi32 rc;` |
|  72484 |  183 | `	rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|  72484 |  184 | `	return rc;` |
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
|  14976 |  227 | `PH7_PRIVATE sxi32 PH7_ClassInherit(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase)` |
|      2 |  228 |  |
|      - |  229 | `	ph7_class_method *pMeth;` |
|      - |  230 | `	ph7_class_attr *pAttr;` |
|      - |  231 | `	SyHashEntry *pEntry;` |
|      - |  232 | `	SyString *pName;` |
|      - |  233 | `	sxi32 rc;` |
|      - |  234 | `	/* Install in the derived hashtable */` |
|  14978 |  235 | `	rc = SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|  14978 |  236 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  237 | `		return rc;` |
|      - |  238 | `	}` |
|      - |  239 | `	/* Copy public/protected attributes from the base class */` |
|  14978 |  240 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
| 104498 |  241 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|      - |  242 | `		/* Make sure the private attributes are not redeclared in the subclass */` |
|  89522 |  243 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  89522 |  244 | `		pName = &pAttr->sName;` |
|  89522 |  245 | `		if( (pEntry = SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte)) != 0 ){` |
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
|  89520 |  257 | `		if( pAttr->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
|  89518 |  258 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|  89518 |  259 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  260 | `				return rc;` |
|      - |  261 | `			}` |
|  44758 |  262 | `		}` |
|      2 |  263 | `	}` |
|  14978 |  264 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
| 149328 |  265 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|      - |  266 | `		/* Make sure the private/final methods are not redeclared in the subclass */` |
| 134352 |  267 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
| 134352 |  268 | `		pName = &pMeth->sFunc.sName;` |
| 134352 |  269 | `		if( (pEntry = SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|   2516 |  270 | `			 if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|      - |  271 | `				/* Cannot Overwrite final method */` |
|      7 |  272 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_method *)pEntry->pUserData)->nLine,` |
|      - |  273 | `					"Cannot Overwrite final method '%z:%z' inside child class '%z'",` |
|      2 |  274 | `					&pBase->sName,pName,&pSub->sName);` |
|      5 |  275 | `				if( rc == SXERR_ABORT ){` |
|    ! 0 |  276 | `					return SXERR_ABORT;` |
|      - |  277 | `				}` |
|      2 |  278 | `			}` |
|   2516 |  279 | `			continue;` |
|      - |  280 | `		}` |
|      - |  281 | `		/* Install the method */` |
| 131838 |  282 | `		if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
| 131836 |  283 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 131836 |  284 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  285 | `				return rc;` |
|      - |  286 | `			}` |
|  65917 |  287 | `		}` |
|      2 |  288 | `	}` |
|      - |  289 | `	/* Mark as subclass */` |
|  14978 |  290 | `	pSub->pBase = pBase;` |
|      - |  291 | `	/* All done */` |
|  14978 |  292 | `	return SXRET_OK;` |
|   7490 |  293 |  |
|      - |  294 | `/*` |
|      - |  295 | ` * Apply a trait to a class: copy all methods and attributes from the trait` |
|      - |  296 | ` * into the target class. Unlike inheritance, traits copy ALL members including` |
|      - |  297 | ` * private ones. Members already defined in the class take precedence.` |
|      - |  298 | ` */` |
|     32 |  299 | `PH7_PRIVATE sxi32 PH7_ClassUseTrait(ph7_gen_state *pGen,ph7_class *pClass,ph7_class *pTrait)` |
|      1 |  300 |  |
|      - |  301 | `	ph7_class_method *pMeth;` |
|      - |  302 | `	ph7_class_attr *pAttr;` |
|      - |  303 | `	SyHashEntry *pEntry;` |
|      - |  304 | `	SyString *pName;` |
|      - |  305 | `	sxi32 rc;` |
|      - |  306 | `	/* Copy attributes from the trait */` |
|     33 |  307 | `	SyHashResetLoopCursor(&pTrait->hAttr);` |
|     45 |  308 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hAttr)) != 0 ){` |
|      - |  309 | `		SyHashEntry *pExisting;` |
|     13 |  310 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     13 |  311 | `		pName = &pAttr->sName;` |
|     13 |  312 | `		pExisting = SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte);` |
|     13 |  313 | `		if( pExisting != 0 ){` |
|      - |  314 | `			/* Attribute already exists. Check if it came from another trait` |
|      - |  315 | `			 * and whether the definitions are compatible (same defaults).` |
|      - |  316 | `			 */` |
|      - |  317 | `			ph7_class **apUsedTraits;` |
|      - |  318 | `			sxu32 nUsed,k;` |
|      5 |  319 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|      5 |  320 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|      5 |  321 | `			for(k = 0; k < nUsed; k++){` |
|      - |  322 | `				ph7_class_attr *pOther;` |
|      3 |  323 | `				pOther = PH7_ClassExtractAttribute(apUsedTraits[k],pName->zString,pName->nByte);` |
|      3 |  324 | `				if( pOther ){` |
|      - |  325 | `					/* Two traits define the same property — check if defaults differ */` |
|      3 |  326 | `					ph7_class_attr *pClassAttr = (ph7_class_attr *)pExisting->pUserData;` |
|      4 |  327 | `					if( SySetUsed(&pAttr->aByteCode) != SySetUsed(&pClassAttr->aByteCode) \|\|` |
|      3 |  328 | `						(SySetUsed(&pAttr->aByteCode) > 0 &&` |
|      3 |  329 | `						 SyMemcmp(SySetBasePtr(&pAttr->aByteCode),SySetBasePtr(&pClassAttr->aByteCode),` |
|      3 |  330 | `							SySetUsed(&pAttr->aByteCode) * SySetElemSize(&pAttr->aByteCode)) != 0) ){` |
|      4 |  331 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine,` |
|      - |  332 | `							"%z and %z define the same property ($%z) in the composition of %z. "` |
|      - |  333 | `							"However, the definition differs and is considered incompatible",` |
|      2 |  334 | `							&apUsedTraits[k]->sName,&pTrait->sName,pName,&pClass->sName);` |
|      3 |  335 | `						if( rc == SXERR_ABORT ){` |
|    ! 0 |  336 | `							return SXERR_ABORT;` |
|      - |  337 | `						}` |
|      1 |  338 | `					}` |
|      3 |  339 | `					break;` |
|      - |  340 | `				}` |
|    ! 0 |  341 | `			}` |
|      5 |  342 | `			continue;` |
|      - |  343 | `		}` |
|      9 |  344 | `		rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|      9 |  345 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  346 | `			return rc;` |
|      - |  347 | `		}` |
|      1 |  348 | `	}` |
|      - |  349 | `	/* Copy methods from the trait */` |
|     33 |  350 | `	SyHashResetLoopCursor(&pTrait->hMethod);` |
|     63 |  351 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hMethod)) != 0 ){` |
|     31 |  352 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|     31 |  353 | `		pName = &pMeth->sFunc.sName;` |
|     31 |  354 | `		if( SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte) != 0 ){` |
|      - |  355 | `			/* Method already exists in the class. Check if it came from another trait` |
|      - |  356 | `			 * (unresolved conflict) vs being defined by the class itself.` |
|      - |  357 | `			 */` |
|      - |  358 | `			ph7_class **apUsedTraits;` |
|      - |  359 | `			sxu32 nUsed,k;` |
|      7 |  360 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|      7 |  361 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|      7 |  362 | `			for(k = 0; k < nUsed; k++){` |
|      3 |  363 | `				if( PH7_ClassExtractMethod(apUsedTraits[k],pName->zString,pName->nByte) != 0 ){` |
|      - |  364 | `					/* Two different traits define the same method with no resolution */` |
|      4 |  365 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|      - |  366 | `						"Trait method %z::%z has not been applied as %z::%z, "` |
|      - |  367 | `						"because of collision with %z::%z",` |
|      2 |  368 | `						&pTrait->sName,pName,` |
|      1 |  369 | `						&pClass->sName,pName,` |
|      2 |  370 | `						&apUsedTraits[k]->sName,pName);` |
|      3 |  371 | `					if( rc == SXERR_ABORT ){` |
|    ! 0 |  372 | `						return SXERR_ABORT;` |
|      - |  373 | `					}` |
|      3 |  374 | `					break;` |
|      - |  375 | `				}` |
|    ! 0 |  376 | `			}` |
|      - |  377 | `			/* Class-defined method takes precedence */` |
|      7 |  378 | `			continue;` |
|      - |  379 | `		}` |
|     25 |  380 | `		rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|     25 |  381 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  382 | `			return rc;` |
|      - |  383 | `		}` |
|      1 |  384 | `	}` |
|      - |  385 | `	/* Record trait in the class */` |
|     33 |  386 | `	SySetPut(&pClass->aTrait,(const void *)&pTrait);` |
|     16 |  387 | `	SXUNUSED(pGen);` |
|     33 |  388 | `	return SXRET_OK;` |
|     17 |  389 |  |
|      - |  390 | `/*` |
|      - |  391 | ` * Inherit an object interface from another object interface.` |
|      - |  392 | ` * According to the PHP language reference manual.` |
|      - |  393 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|      - |  394 | ` *  must implement, without having to define how these methods are handled.` |
|      - |  395 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|      - |  396 | ` *  class, but without any of the methods having their contents defined.` |
|      - |  397 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|      - |  398 | ` *` |
|      - |  399 | ` * This function return SXRET_OK if the interface inheritance operation was successfully performed.` |
|      - |  400 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|      - |  401 | ` * error message.` |
|      - |  402 | ` */` |
|      2 |  403 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceInherit(ph7_class *pSub,ph7_class *pBase)` |
|      1 |  404 |  |
|      - |  405 | `	ph7_class_method *pMeth;` |
|      - |  406 | `	ph7_class_attr *pAttr;` |
|      - |  407 | `	SyHashEntry *pEntry;` |
|      - |  408 | `	SyString *pName;` |
|      - |  409 | `	sxi32 rc;` |
|      - |  410 | `	/* Install in the derived hashtable */` |
|      3 |  411 | `	SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|      3 |  412 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
|      - |  413 | `	/* Copy constants */` |
|      6 |  414 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|      - |  415 | `		/* Make sure the constants are not redeclared in the subclass */` |
|      3 |  416 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      3 |  417 | `		pName = &pAttr->sName;` |
|      3 |  418 | `		if( SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte) == 0 ){` |
|      - |  419 | `			/* Install the constant in the subclass */` |
|      3 |  420 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|      3 |  421 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  422 | `				return rc;` |
|      - |  423 | `			}` |
|      1 |  424 | `		}` |
|      1 |  425 | `	}` |
|      3 |  426 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
|      - |  427 | `	/* Copy methods signature */` |
|      6 |  428 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|      - |  429 | `		/* Make sure the method are not redeclared in the subclass */` |
|      3 |  430 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      3 |  431 | `		pName = &pMeth->sFunc.sName;` |
|      3 |  432 | `		if( SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte) == 0 ){` |
|      - |  433 | `			/* Install the method */` |
|      3 |  434 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|      3 |  435 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  436 | `				return rc;` |
|      - |  437 | `			}` |
|      1 |  438 | `		}` |
|      1 |  439 | `	}` |
|      - |  440 | `	/* Mark as subclass */` |
|      3 |  441 | `	pSub->pBase = pBase;` |
|      - |  442 | `	/* All done */` |
|      3 |  443 | `	return SXRET_OK;` |
|      2 |  444 |  |
|      - |  445 | `/*` |
|      - |  446 | ` * Implements an object interface in the given main class.` |
|      - |  447 | ` * According to the PHP language reference manual.` |
|      - |  448 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|      - |  449 | ` *  must implement, without having to define how these methods are handled.` |
|      - |  450 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|      - |  451 | ` *  class, but without any of the methods having their contents defined.` |
|      - |  452 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|      - |  453 | ` *` |
|      - |  454 | ` * This function return SXRET_OK if the interface was successfully implemented.` |
|      - |  455 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|      - |  456 | ` * error message.` |
|      - |  457 | ` */` |
|     24 |  458 | `PH7_PRIVATE sxi32 PH7_ClassImplement(ph7_class *pMain,ph7_class *pInterface)` |
|      2 |  459 |  |
|      - |  460 | `	ph7_class_attr *pAttr;` |
|      - |  461 | `	SyHashEntry *pEntry;` |
|      - |  462 | `	SyString *pName;` |
|      - |  463 | `	sxi32 rc;` |
|      - |  464 | `	/* First off,copy all constants declared inside the interface */` |
|     26 |  465 | `	SyHashResetLoopCursor(&pInterface->hAttr);` |
|     40 |  466 | `	while((pEntry = SyHashGetNextEntry(&pInterface->hAttr)) != 0 ){` |
|      - |  467 | `		/* Point to the constant declaration */` |
|      3 |  468 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      3 |  469 | `		pName = &pAttr->sName;` |
|      - |  470 | `		/* Make sure the attribute is not redeclared in the main class */` |
|      3 |  471 | `		if( SyHashGet(&pMain->hAttr,pName->zString,pName->nByte) == 0 ){` |
|      - |  472 | `			/* Install the attribute */` |
|      3 |  473 | `			rc = SyHashInsert(&pMain->hAttr,pName->zString,pName->nByte,pAttr);` |
|      3 |  474 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  475 | `				return rc;` |
|      - |  476 | `			}` |
|      1 |  477 | `		}` |
|      1 |  478 | `	}` |
|      - |  479 | `	/* Install in the interface container */` |
|     26 |  480 | `	SySetPut(&pMain->aInterface,(const void *)&pInterface);` |
|      - |  481 | `	/* Install interface method stubs into the implementing class.` |
|      - |  482 | `	 * Methods already defined in the class take precedence (they satisfy` |
|      - |  483 | `	 * the interface contract). Stubs retain PH7_CLASS_ATTR_ABSTRACT so` |
|      - |  484 | `	 * the unified check in GenStateCheckAbstractMethods catches missing ones.` |
|      - |  485 | `	 */` |
|      - |  486 | `	{` |
|      - |  487 | `		ph7_class_method *pMeth;` |
|      - |  488 | `		SyHashEntry *pMEntry;` |
|      - |  489 | `		SyString *pMName;` |
|     26 |  490 | `		SyHashResetLoopCursor(&pInterface->hMethod);` |
|     94 |  491 | `		while((pMEntry = SyHashGetNextEntry(&pInterface->hMethod)) != 0 ){` |
|     58 |  492 | `			pMeth = (ph7_class_method *)pMEntry->pUserData;` |
|     58 |  493 | `			pMName = &pMeth->sFunc.sName;` |
|     58 |  494 | `			if( SyHashGet(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte) == 0 ){` |
|     15 |  495 | `				rc = SyHashInsert(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte,pMeth);` |
|     15 |  496 | `				if( rc != SXRET_OK ){` |
|    ! 0 |  497 | `					return rc;` |
|      - |  498 | `				}` |
|      7 |  499 | `			}` |
|      2 |  500 | `		}` |
|      - |  501 | `	}` |
|     26 |  502 | `	return SXRET_OK;` |
|     14 |  503 |  |
|      - |  504 | `/*` |
|      - |  505 | ` * Create a class instance [i.e: Object in the PHP jargon] at run-time.` |
|      - |  506 | ` * The following function is called when an object is created at run-time` |
|      - |  507 | ` * typically when the PH7_OP_NEW/PH7_OP_CLONE instructions are executed.` |
|      - |  508 | ` * Notes on object creation.` |
|      - |  509 | ` *` |
|      - |  510 | ` * According to PHP language reference manual.` |
|      - |  511 | ` * To create an instance of a class, the new keyword must be used. An object will always` |
|      - |  512 | ` * be created unless the object has a constructor defined that throws an exception on error.` |
|      - |  513 | ` * Classes should be defined before instantiation (and in some cases this is a requirement).` |
|      - |  514 | ` * If a string containing the name of a class is used with new, a new instance of that class` |
|      - |  515 | ` * will be created. If the class is in a namespace, its fully qualified name must be used when` |
|      - |  516 | ` * doing this.` |
|      - |  517 | ` * Example #3 Creating an instance` |
|      - |  518 | ` * <?php` |
|      - |  519 | ` *  $instance = new SimpleClass();` |
|      - |  520 | ` *   // This can also be done with a variable:` |
|      - |  521 | ` * $className = 'Foo';` |
|      - |  522 | ` * $instance = new $className(); // Foo()` |
|      - |  523 | ` * ?>` |
|      - |  524 | ` * In the class context, it is possible to create a new object by new self and new parent.` |
|      - |  525 | ` * When assigning an already created instance of a class to a new variable, the new variable` |
|      - |  526 | ` * will access the same instance as the object that was assigned. This behaviour is the same` |
|      - |  527 | ` * when passing instances to a function. A copy of an already created object can be made by` |
|      - |  528 | ` * cloning it.` |
|      - |  529 | ` * Example #4 Object Assignment` |
|      - |  530 | ` * <?php` |
|      - |  531 | ` *  class SimpleClass(){` |
|      - |  532 | ` *    public $var;` |
|      - |  533 | ` *  };` |
|      - |  534 | ` *  $instance = new SimpleClass();` |
|      - |  535 | ` *  $assigned   =  $instance;` |
|      - |  536 | ` *  $reference  =& $instance;` |
|      - |  537 | ` *  $instance->var = '$assigned will have this value';` |
|      - |  538 | ` *  $instance = null; // $instance and $reference become null` |
|      - |  539 | ` *  var_dump($instance);` |
|      - |  540 | ` *  var_dump($reference);` |
|      - |  541 | ` *  var_dump($assigned);` |
|      - |  542 | ` * ?>` |
|      - |  543 | ` * The above example will output:` |
|      - |  544 | ` * NULL` |
|      - |  545 | ` * NULL` |
|      - |  546 | ` * object(SimpleClass)#1 (1) {` |
|      - |  547 | ` *  ["var"]=>` |
|      - |  548 | ` *    string(30) "$assigned will have this value"` |
|      - |  549 | ` * }` |
|      - |  550 | ` * Example #5 Creating new objects` |
|      - |  551 | ` * <?php` |
|      - |  552 | ` * class Test` |
|      - |  553 | ` * {` |
|      - |  554 | ` *   static public function getNew()` |
|      - |  555 | ` *   {` |
|      - |  556 | ` *       return new static;` |
|      - |  557 | ` *   }` |
|      - |  558 | ` * }` |
|      - |  559 | ` * class Child extends Test` |
|      - |  560 | ` * {}` |
|      - |  561 | ` * $obj1 = new Test();` |
|      - |  562 | ` * $obj2 = new $obj1;` |
|      - |  563 | ` * var_dump($obj1 !== $obj2);` |
|      - |  564 | ` * $obj3 = Test::getNew();` |
|      - |  565 | ` * var_dump($obj3 instanceof Test);` |
|      - |  566 | ` * $obj4 = Child::getNew();` |
|      - |  567 | ` * var_dump($obj4 instanceof Child);` |
|      - |  568 | ` * ?>` |
|      - |  569 | ` * The above example will output:` |
|      - |  570 | ` * bool(true)` |
|      - |  571 | ` * bool(true)` |
|      - |  572 | ` * bool(true)` |
|      - |  573 | ` * Note that Symisc Systems have introduced powerfull extension to` |
|      - |  574 | ` * OO subsystem. For example a class attribute may have any complex` |
|      - |  575 | ` * expression associated with it when declaring the attribute unlike` |
|      - |  576 | ` * the standard PHP engine which would allow a single value.` |
|      - |  577 | ` * Example:` |
|      - |  578 | ` *  class myClass{` |
|      - |  579 | ` *    public $var = 25<<1+foo()/bar();` |
|      - |  580 | ` *  };` |
|      - |  581 | ` * Refer to the official documentation for more information.` |
|      - |  582 | ` */` |
|   1120 |  583 | `static ph7_class_instance * NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|      2 |  584 |  |
|      - |  585 | `	ph7_class_instance *pThis;` |
|      - |  586 | `	/* Allocate a new instance */` |
|   1122 |  587 | `	pThis = (ph7_class_instance *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_instance));` |
|   1122 |  588 | `	if( pThis == 0 ){` |
|    ! 0 |  589 | `		return 0;` |
|      - |  590 | `	}` |
|      - |  591 | `	/* Zero the structure */` |
|   1122 |  592 | `	SyZero(pThis,sizeof(ph7_class_instance));` |
|      - |  593 | `	/* Initialize fields */` |
|   1122 |  594 | `	pThis->iRef = 1;` |
|   1122 |  595 | `	pThis->pVm = pVm;` |
|   1122 |  596 | `	pThis->pClass = pClass;` |
|   1122 |  597 | `	SyHashInit(&pThis->hAttr,&pVm->sAllocator,0,0);` |
|   1122 |  598 | `	return pThis;` |
|    562 |  599 |  |
|      - |  600 | `/*` |
|      - |  601 | ` * Wrapper around the NewClassInstance() function defined above.` |
|      - |  602 | ` * See the block comment above for more information.` |
|      - |  603 | ` */` |
|   1078 |  604 | `PH7_PRIVATE ph7_class_instance * PH7_NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|      2 |  605 |  |
|      - |  606 | `	ph7_class_instance *pNew;` |
|      - |  607 | `	sxi32 rc;` |
|   1080 |  608 | `	pNew = NewClassInstance(&(*pVm),&(*pClass));` |
|   1080 |  609 | `	if( pNew == 0 ){` |
|    ! 0 |  610 | `		return 0;` |
|      - |  611 | `	}` |
|      - |  612 | `	/* Associate a private VM frame with this class instance */` |
|   1080 |  613 | `	rc = PH7_VmCreateClassInstanceFrame(&(*pVm),pNew);` |
|   1080 |  614 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  615 | `		SyMemBackendPoolFree(&pVm->sAllocator,pNew);` |
|    ! 0 |  616 | `		return 0;` |
|      - |  617 | `	}` |
|   1080 |  618 | `	return pNew;` |
|    541 |  619 |  |
|      - |  620 | `/*` |
|      - |  621 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon] attribute.` |
|      - |  622 | ` * This function never fail.` |
|      - |  623 | ` */` |
|    540 |  624 | `static ph7_value * ExtractClassAttrValue(ph7_vm *pVm,VmClassAttr *pAttr)` |
|      2 |  625 |  |
|      - |  626 | `	/* Extract the value */` |
|      - |  627 | `	ph7_value *pValue;` |
|    542 |  628 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|    542 |  629 | `	return pValue;` |
|      2 |  630 |  |
|      - |  631 | `/*` |
|      - |  632 | ` * Perform a clone operation on a class instance [i.e: Object in the PHP jargon].` |
|      - |  633 | ` * The following function is called when an object is cloned at run-time` |
|      - |  634 | ` * typically when the PH7_OP_CLONE instruction is executed.` |
|      - |  635 | ` * Notes on object cloning.` |
|      - |  636 | ` *` |
|      - |  637 | ` * According to PHP language reference manual.` |
|      - |  638 | ` * Creating a copy of an object with fully replicated properties is not always the wanted behavior.` |
|      - |  639 | ` * A good example of the need for copy constructors. Another example is if your object holds a reference` |
|      - |  640 | ` * to another object which it uses and when you replicate the parent object you want to create` |
|      - |  641 | ` * a new instance of this other object so that the replica has its own separate copy.` |
|      - |  642 | ` * An object copy is created by using the clone keyword (which calls the object's __clone() method if possible).` |
|      - |  643 | ` * An object's __clone() method cannot be called directly.` |
|      - |  644 | ` * $copy_of_object = clone $object;` |
|      - |  645 | ` * When an object is cloned, PHP 5 will perform a shallow copy of all of the object's properties.` |
|      - |  646 | ` * Any properties that are references to other variables, will remain references.` |
|      - |  647 | ` * Once the cloning is complete, if a __clone() method is defined, then the newly created object's __clone() method` |
|      - |  648 | ` * will be called, to allow any necessary properties that need to be changed.` |
|      - |  649 | ` * Example #1 Cloning an object` |
|      - |  650 | ` * <?php` |
|      - |  651 | ` * class SubObject` |
|      - |  652 | ` * {` |
|      - |  653 | ` *   static $instances = 0;` |
|      - |  654 | ` *   public $instance;` |
|      - |  655 | ` *` |
|      - |  656 | ` *   public function __construct() {` |
|      - |  657 | ` *       $this->instance = ++self::$instances;` |
|      - |  658 | ` *   }` |
|      - |  659 | ` *` |
|      - |  660 | ` *   public function __clone() {` |
|      - |  661 | ` *       $this->instance = ++self::$instances;` |
|      - |  662 | ` *   }` |
|      - |  663 | ` * }` |
|      - |  664 | ` *` |
|      - |  665 | ` * class MyCloneable` |
|      - |  666 | ` * {` |
|      - |  667 | ` *   public $object1;` |
|      - |  668 | ` *   public $object2;` |
|      - |  669 | ` *` |
|      - |  670 | ` *   function __clone()` |
|      - |  671 | ` *   {` |
|      - |  672 | ` *       // Force a copy of this->object, otherwise` |
|      - |  673 | ` *       // it will point to same object.` |
|      - |  674 | ` *       $this->object1 = clone $this->object1;` |
|      - |  675 | ` *   }` |
|      - |  676 | ` * }` |
|      - |  677 | ` * $obj = new MyCloneable();` |
|      - |  678 | ` * $obj->object1 = new SubObject();` |
|      - |  679 | ` * $obj->object2 = new SubObject();` |
|      - |  680 | ` * $obj2 = clone $obj;` |
|      - |  681 | ` * print("Original Object:\n");` |
|      - |  682 | ` * print_r($obj);` |
|      - |  683 | ` * print("Cloned Object:\n");` |
|      - |  684 | ` * print_r($obj2);` |
|      - |  685 | ` * ?>` |
|      - |  686 | ` * The above example will output:` |
|      - |  687 | ` * Original Object:` |
|      - |  688 | ` * MyCloneable Object` |
|      - |  689 | ` * (` |
|      - |  690 | ` *   [object1] => SubObject Object` |
|      - |  691 | ` *       (` |
|      - |  692 | ` *           [instance] => 1` |
|      - |  693 | ` *       )` |
|      - |  694 | ` *` |
|      - |  695 | ` *   [object2] => SubObject Object` |
|      - |  696 | ` *       (` |
|      - |  697 | ` *           [instance] => 2` |
|      - |  698 | ` *       )` |
|      - |  699 | ` *` |
|      - |  700 | ` * )` |
|      - |  701 | ` * Cloned Object:` |
|      - |  702 | ` * MyCloneable Object` |
|      - |  703 | ` * (` |
|      - |  704 | ` *   [object1] => SubObject Object` |
|      - |  705 | ` *       (` |
|      - |  706 | ` *           [instance] => 3` |
|      - |  707 | ` *       )` |
|      - |  708 | ` *` |
|      - |  709 | ` *   [object2] => SubObject Object` |
|      - |  710 | ` *       (` |
|      - |  711 | ` *           [instance] => 2` |
|      - |  712 | ` *       )` |
|      - |  713 | ` * )` |
|      - |  714 | ` */` |
|     42 |  715 | `PH7_PRIVATE ph7_class_instance * PH7_CloneClassInstance(ph7_class_instance *pSrc)` |
|      2 |  716 |  |
|      - |  717 | `	ph7_class_instance *pClone;` |
|      - |  718 | `	ph7_class_method *pMethod;` |
|      - |  719 | `	SyHashEntry *pEntry2;` |
|      - |  720 | `	SyHashEntry *pEntry;` |
|      - |  721 | `	ph7_vm *pVm;` |
|      - |  722 | `	sxi32 rc;` |
|      - |  723 | `	/* Allocate a new instance */` |
|     44 |  724 | `	pVm = pSrc->pVm;` |
|     44 |  725 | `	pClone = NewClassInstance(pVm,pSrc->pClass);` |
|     44 |  726 | `	if( pClone == 0 ){` |
|    ! 0 |  727 | `		return 0;` |
|      - |  728 | `	}` |
|      - |  729 | `	/* Associate a private VM frame with this class instance */` |
|     44 |  730 | `	rc = PH7_VmCreateClassInstanceFrame(pVm,pClone);` |
|     44 |  731 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  732 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClone);` |
|    ! 0 |  733 | `		return 0;` |
|      - |  734 | `	}` |
|      - |  735 | `	/* Duplicate object values */` |
|     44 |  736 | `	SyHashResetLoopCursor(&pSrc->hAttr);` |
|     44 |  737 | `	SyHashResetLoopCursor(&pClone->hAttr);` |
|    111 |  738 | `	while((pEntry = SyHashGetNextEntry(&pSrc->hAttr)) != 0 && (pEntry2 = SyHashGetNextEntry(&pClone->hAttr)) != 0 ){` |
|     48 |  739 | `		VmClassAttr *pSrcAttr = (VmClassAttr *)pEntry->pUserData;` |
|     48 |  740 | `		VmClassAttr *pDestAttr = (VmClassAttr *)pEntry2->pUserData;` |
|      - |  741 | `		/* Duplicate non-static attribute */` |
|     48 |  742 | `		if( (pSrcAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|      - |  743 | `			ph7_value *pvSrc,*pvDest;` |
|     48 |  744 | `			pvSrc = ExtractClassAttrValue(pVm,pSrcAttr);` |
|     48 |  745 | `			pvDest = ExtractClassAttrValue(pVm,pDestAttr);` |
|     48 |  746 | `			if( pvSrc && pvDest ){` |
|     48 |  747 | `				PH7_MemObjStore(pvSrc,pvDest);` |
|     23 |  748 | `			}` |
|     23 |  749 | `		}` |
|      2 |  750 | `	}` |
|      - |  751 | `	/* call the __clone method on the cloned object if available */` |
|     44 |  752 | `	pMethod = PH7_ClassExtractMethod(pClone->pClass,"__clone",sizeof("__clone")-1);` |
|     44 |  753 | `	if( pMethod ){` |
|     38 |  754 | `		if( pMethod->iCloneDepth < 16 ){` |
|     36 |  755 | `			pMethod->iCloneDepth++;` |
|     36 |  756 | `			PH7_VmCallClassMethod(pVm,pClone,pMethod,0,0,0);` |
|     19 |  757 | `		}else{` |
|      - |  758 | `			/* Nesting limit reached */` |
|      3 |  759 | `			PH7_VmThrowError(pVm,0,PH7_CTX_ERR,"Object clone limit reached,no more call to __clone()");` |
|      - |  760 | `		}` |
|      - |  761 | `		/* Reset the cursor */` |
|     38 |  762 | `		pMethod->iCloneDepth = 0;` |
|     18 |  763 | `	}` |
|      - |  764 | `	/* Return the cloned object */` |
|     44 |  765 | `	return pClone;` |
|     23 |  766 |  |
|      - |  767 | `#define CLASS_INSTANCE_DESTROYED 0x001 /* Instance is released */` |
|      - |  768 | `/*` |
|      - |  769 | ` * Release a class instance [i.e: Object in the PHP jargon] and invoke any defined destructor.` |
|      - |  770 | ` * This routine is invoked as soon as there are no other references to a particular` |
|      - |  771 | ` * class instance.` |
|      - |  772 | ` */` |
|    778 |  773 | `static void PH7_ClassInstanceRelease(ph7_class_instance *pThis)` |
|      2 |  774 |  |
|      - |  775 | `	ph7_class_method *pDestr;` |
|      - |  776 | `	SyHashEntry *pEntry;` |
|      - |  777 | `	ph7_class *pClass;` |
|      - |  778 | `	ph7_vm *pVm;` |
|    780 |  779 | `	if( pThis->iFlags & CLASS_INSTANCE_DESTROYED ){` |
|      - |  780 | `		/*` |
|      - |  781 | `		 * Already destroyed,return immediately.` |
|      - |  782 | `		 * This could happend if someone perform unset($this) in the destructor body.` |
|      - |  783 | `		 */` |
|    ! 0 |  784 | `		return;` |
|      - |  785 | `	}` |
|      - |  786 | `	/* Mark as destroyed */` |
|    780 |  787 | `	pThis->iFlags \|= CLASS_INSTANCE_DESTROYED;` |
|      - |  788 | `	/* Invoke any defined destructor if available */` |
|    780 |  789 | `	pVm = pThis->pVm;` |
|    780 |  790 | `	pClass = pThis->pClass;` |
|    780 |  791 | `	pDestr = PH7_ClassExtractMethod(pClass,"__destruct",sizeof("__destruct")-1);` |
|    780 |  792 | `	if( pDestr ){` |
|      - |  793 | `		/* Invoke the destructor */` |
|      5 |  794 | `		pThis->iRef = 2; /* Prevent garbage collection */` |
|      5 |  795 | `		PH7_VmCallClassMethod(pVm,pThis,pDestr,0,0,0);` |
|      2 |  796 | `	}` |
|      - |  797 | `	/* Release non-static attributes */` |
|    780 |  798 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|   4046 |  799 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|   3268 |  800 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|   3268 |  801 | `		if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|   3264 |  802 | `			PH7_VmUnsetMemObj(pVm,pVmAttr->nIdx,TRUE);` |
|   1631 |  803 | `		}` |
|   3268 |  804 | `		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      2 |  805 | `	}` |
|      - |  806 | `	/* Release the whole structure */` |
|    780 |  807 | `	SyHashRelease(&pThis->hAttr);` |
|    780 |  808 | `	SyMemBackendPoolFree(&pVm->sAllocator,pThis);` |
|    391 |  809 |  |
|      - |  810 | `/*` |
|      - |  811 | ` * Decrement the reference count of a class instance [i.e Object in the PHP jargon].` |
|      - |  812 | ` * If the reference count reaches zero,release the whole instance.` |
|      - |  813 | ` */` |
|  14316 |  814 | `PH7_PRIVATE void PH7_ClassInstanceUnref(ph7_class_instance *pThis)` |
|      2 |  815 |  |
|  14318 |  816 | `	pThis->iRef--;` |
|  14318 |  817 | `	if( pThis->iRef < 1 ){` |
|      - |  818 | `		/* No more reference to this instance */` |
|    780 |  819 | `		PH7_ClassInstanceRelease(&(*pThis));` |
|    389 |  820 | `	}` |
|  14318 |  821 |  |
|      - |  822 | `/*` |
|      - |  823 | ` * Compare two class instances [i.e: Objects in the PHP jargon]` |
|      - |  824 | ` * Note on objects comparison:` |
|      - |  825 | ` *  According to the PHP langauge reference manual` |
|      - |  826 | ` *  When using the comparison operator (==), object variables are compared in a simple manner` |
|      - |  827 | ` *  namely: Two object instances are equal if they have the same attributes and values, and are` |
|      - |  828 | ` *  instances of the same class.` |
|      - |  829 | ` *  On the other hand, when using the identity operator (===), object variables are identical` |
|      - |  830 | ` *  if and only if they refer to the same instance of the same class.` |
|      - |  831 | ` *  An example will clarify these rules.` |
|      - |  832 | ` *  Example #1 Example of object comparison` |
|      - |  833 | ` *  <?php` |
|      - |  834 | ` *    function bool2str($bool)` |
|      - |  835 | ` * {` |
|      - |  836 | ` *   if ($bool === false) {` |
|      - |  837 | ` *       return 'FALSE';` |
|      - |  838 | ` *   } else {` |
|      - |  839 | ` *       return 'TRUE';` |
|      - |  840 | ` *   }` |
|      - |  841 | ` * }` |
|      - |  842 | ` * function compareObjects(&$o1, &$o2)` |
|      - |  843 | ` * {` |
|      - |  844 | ` *   echo 'o1 == o2 : ' . bool2str($o1 == $o2) . "\n";` |
|      - |  845 | ` *   echo 'o1 != o2 : ' . bool2str($o1 != $o2) . "\n";` |
|      - |  846 | ` *   echo 'o1 === o2 : ' . bool2str($o1 === $o2) . "\n";` |
|      - |  847 | ` *   echo 'o1 !== o2 : ' . bool2str($o1 !== $o2) . "\n";` |
|      - |  848 | ` * }` |
|      - |  849 | ` * class Flag` |
|      - |  850 | ` * {` |
|      - |  851 | ` *   public $flag;` |
|      - |  852 | ` *` |
|      - |  853 | ` *   function Flag($flag = true) {` |
|      - |  854 | ` *       $this->flag = $flag;` |
|      - |  855 | ` *   }` |
|      - |  856 | ` * }` |
|      - |  857 | ` *` |
|      - |  858 | ` * class OtherFlag` |
|      - |  859 | ` * {` |
|      - |  860 | ` *   public $flag;` |
|      - |  861 | ` *` |
|      - |  862 | ` *   function OtherFlag($flag = true) {` |
|      - |  863 | ` *       $this->flag = $flag;` |
|      - |  864 | ` *   }` |
|      - |  865 | ` * }` |
|      - |  866 | ` *` |
|      - |  867 | ` * $o = new Flag();` |
|      - |  868 | ` * $p = new Flag();` |
|      - |  869 | ` * $q = $o;` |
|      - |  870 | ` * $r = new OtherFlag();` |
|      - |  871 | ` *` |
|      - |  872 | ` * echo "Two instances of the same class\n";` |
|      - |  873 | ` * compareObjects($o, $p);` |
|      - |  874 | ` * echo "\nTwo references to the same instance\n";` |
|      - |  875 | ` * compareObjects($o, $q);` |
|      - |  876 | ` * echo "\nInstances of two different classes\n";` |
|      - |  877 | ` * compareObjects($o, $r);` |
|      - |  878 | ` * ?>` |
|      - |  879 | ` * The above example will output:` |
|      - |  880 | ` * Two instances of the same class` |
|      - |  881 | ` * o1 == o2 : TRUE` |
|      - |  882 | ` * o1 != o2 : FALSE` |
|      - |  883 | ` * o1 === o2 : FALSE` |
|      - |  884 | ` * o1 !== o2 : TRUE` |
|      - |  885 | ` * Two references to the same instance` |
|      - |  886 | ` * o1 == o2 : TRUE` |
|      - |  887 | ` * o1 != o2 : FALSE` |
|      - |  888 | ` * o1 === o2 : TRUE` |
|      - |  889 | ` * o1 !== o2 : FALSE` |
|      - |  890 | ` * Instances of two different classes` |
|      - |  891 | ` * o1 == o2 : FALSE` |
|      - |  892 | ` * o1 != o2 : TRUE` |
|      - |  893 | ` * o1 === o2 : FALSE` |
|      - |  894 | ` * o1 !== o2 : TRUE` |
|      - |  895 | ` *` |
|      - |  896 | ` * This function return 0 if the objects are equals according to the comprison rules defined above.` |
|      - |  897 | ` * Any other return values indicates difference.` |
|      - |  898 | ` */` |
|    160 |  899 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest)` |
|      2 |  900 |  |
|      - |  901 | `	SyHashEntry *pEntry,*pEntry2;` |
|      - |  902 | `	ph7_value sV1,sV2;` |
|      - |  903 | `	sxi32 rc;` |
|    162 |  904 | `	if( iNest > 31 ){` |
|      - |  905 | `		/* Nesting limit reached */` |
|      5 |  906 | `		PH7_VmThrowError(pLeft->pVm,0,PH7_CTX_ERR,"Nesting limit reached: Infinite recursion?");` |
|      5 |  907 | `		return 1;` |
|      - |  908 | `	}` |
|      - |  909 | `	/* Comparison is performed only if the objects are instance of the same class */` |
|    158 |  910 | `	if( pLeft->pClass != pRight->pClass ){` |
|      7 |  911 | `		return 1;` |
|      - |  912 | `	}` |
|    152 |  913 | `	if( bStrict ){` |
|      - |  914 | `		/*` |
|      - |  915 | `		 * According to the PHP language reference manual:` |
|      - |  916 | `		 *  when using the identity operator (===), object variables` |
|      - |  917 | `		 *  are identical if and only if they refer to the same instance` |
|      - |  918 | `		 *  of the same class.` |
|      - |  919 | `		 */` |
|     11 |  920 | `		return !(pLeft == pRight);` |
|      - |  921 | `	}` |
|      - |  922 | `	/*` |
|      - |  923 | `	 * Attribute comparison.` |
|      - |  924 | `	 * According to the PHP reference manual:` |
|      - |  925 | `	 *  When using the comparison operator (==), object variables are compared` |
|      - |  926 | `	 *  in a simple manner, namely: Two object instances are equal if they have` |
|      - |  927 | `	 *  the same attributes and values, and are instances of the same class.` |
|      - |  928 | `	 */` |
|    142 |  929 | `	if( pLeft == pRight ){` |
|      - |  930 | `		/* Same instance,don't bother processing,object are equals */` |
|      3 |  931 | `		return 0;` |
|      - |  932 | `	}` |
|    140 |  933 | `	SyHashResetLoopCursor(&pLeft->hAttr);` |
|    140 |  934 | `	SyHashResetLoopCursor(&pRight->hAttr);` |
|    140 |  935 | `	PH7_MemObjInit(pLeft->pVm,&sV1);` |
|    140 |  936 | `	PH7_MemObjInit(pLeft->pVm,&sV2);` |
|    140 |  937 | `	sV1.nIdx = sV2.nIdx = SXU32_HIGH;` |
|    223 |  938 | `	while((pEntry = SyHashGetNextEntry(&pLeft->hAttr)) != 0 && (pEntry2 = SyHashGetNextEntry(&pRight->hAttr)) != 0 ){` |
|    146 |  939 | `		VmClassAttr *p1 = (VmClassAttr *)pEntry->pUserData;` |
|    146 |  940 | `		VmClassAttr *p2 = (VmClassAttr *)pEntry2->pUserData;` |
|      - |  941 | `		/* Compare only non-static attribute */` |
|    146 |  942 | `		if( (p1->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|      - |  943 | `			ph7_value *pL,*pR;` |
|    146 |  944 | `			pL = ExtractClassAttrValue(pLeft->pVm,p1);` |
|    146 |  945 | `			pR = ExtractClassAttrValue(pRight->pVm,p2);` |
|    146 |  946 | `			if( pL && pR ){` |
|    146 |  947 | `				PH7_MemObjLoad(pL,&sV1);` |
|    146 |  948 | `				PH7_MemObjLoad(pR,&sV2);` |
|      - |  949 | `				/* Compare the two values now */` |
|    146 |  950 | `				rc = PH7_MemObjCmp(&sV1,&sV2,bStrict,iNest+1);` |
|    146 |  951 | `				PH7_MemObjRelease(&sV1);` |
|    146 |  952 | `				PH7_MemObjRelease(&sV2);` |
|    146 |  953 | `				if( rc != 0 ){` |
|      - |  954 | `					/* Not equals */` |
|    132 |  955 | `					return rc;` |
|      - |  956 | `				}` |
|      7 |  957 | `			}` |
|      7 |  958 | `		}` |
|      1 |  959 | `	}` |
|      - |  960 | `	/* Object are equals */` |
|      9 |  961 | `	return 0;` |
|     82 |  962 |  |
|      - |  963 | `/*` |
|      - |  964 | ` * Dump a class instance and the store the dump in the BLOB given` |
|      - |  965 | ` * as the first argument.` |
|      - |  966 | ` * Note that only non-static/non-constants attribute are dumped.` |
|      - |  967 | ` * This function is typically invoked when the user issue a call` |
|      - |  968 | ` * to [var_dump(),var_export(),print_r(),...].` |
|      - |  969 | ` * This function SXRET_OK on success. Any other return value including` |
|      - |  970 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|      - |  971 | ` */` |
|    132 |  972 | `PH7_PRIVATE sxi32 PH7_ClassInstanceDump(SyBlob *pOut,ph7_class_instance *pThis,int ShowType,int nTab,int nDepth)` |
|      1 |  973 |  |
|      - |  974 | `	SyHashEntry *pEntry;` |
|      - |  975 | `	ph7_value *pValue;` |
|      - |  976 | `	sxi32 rc;` |
|      - |  977 | `	int i;` |
|    133 |  978 | `	if( nDepth > 31 ){` |
|      - |  979 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|      - |  980 | `		/* Nesting limit reached..halt immediately*/` |
|      5 |  981 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|      5 |  982 | `		if( ShowType ){` |
|      5 |  983 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|      2 |  984 | `		}` |
|      5 |  985 | `		return SXERR_LIMIT;` |
|      - |  986 | `	}` |
|    129 |  987 | `	rc = SXRET_OK;` |
|    129 |  988 | `	if( !ShowType ){` |
|      3 |  989 | `		SyBlobAppend(&(*pOut),"Object(",sizeof("Object(")-1);` |
|      1 |  990 | `	}` |
|      - |  991 | `	/* Append class name */` |
|    129 |  992 | `	SyBlobFormat(&(*pOut),"%z) {",&pThis->pClass->sName);` |
|      - |  993 | `#ifdef __WINNT__` |
|      1 |  994 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|      - |  995 | `#else` |
|    128 |  996 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      - |  997 | `#endif` |
|      - |  998 | `	/* Dump object attributes */` |
|    129 |  999 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|    201 | 1000 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|    133 | 1001 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    133 | 1002 | `		if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|      - | 1003 | `			/* Dump non-static/constant attribute only */` |
|   3985 | 1004 | `			for( i = 0 ; i < nTab ; i++ ){` |
|   3853 | 1005 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|   1927 | 1006 | `			}` |
|    133 | 1007 | `			pValue = ExtractClassAttrValue(pThis->pVm,pVmAttr);` |
|    133 | 1008 | `			if( pValue ){` |
|    133 | 1009 | `				SyBlobFormat(&(*pOut),"['%z'] =>",&pVmAttr->pAttr->sName);` |
|      - | 1010 | `#ifdef __WINNT__` |
|      1 | 1011 | `				SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|      - | 1012 | `#else` |
|    132 | 1013 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      - | 1014 | `#endif` |
|    133 | 1015 | `				rc = PH7_MemObjDump(&(*pOut),pValue,ShowType,nTab+1,nDepth,0);` |
|    133 | 1016 | `				if( rc == SXERR_LIMIT ){` |
|    125 | 1017 | `					break;` |
|      - | 1018 | `				}` |
|      4 | 1019 | `			}` |
|      4 | 1020 | `		}` |
|      1 | 1021 | `	}` |
|   3977 | 1022 | `	for( i = 0 ; i < nTab ; i++ ){` |
|   3849 | 1023 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|   1925 | 1024 | `	}` |
|    129 | 1025 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|    129 | 1026 | `	return rc;` |
|     67 | 1027 |  |
|      - | 1028 | `/*` |
|      - | 1029 | ` * Call a magic method [i.e: __toString(),__toBool(),__Invoke()...]` |
|      - | 1030 | ` * Return SXRET_OK on successfull call. Any other return value indicates failure.` |
|      - | 1031 | ` * Notes on magic methods.` |
|      - | 1032 | ` * According to the PHP language reference manual.` |
|      - | 1033 | ` *  The function names __construct(), __destruct(), __call(), __callStatic()` |
|      - | 1034 | ` *  __get(),  __toString(), __invoke(), __clone() are magical in PHP classes.` |
|      - | 1035 | ` * You cannot have functions with these names in any of your classes unless` |
|      - | 1036 | ` * you want the magic functionality associated with them.` |
|      - | 1037 | ` * Example of magical methods:` |
|      - | 1038 | ` * __toString()` |
|      - | 1039 | ` *  The __toString() method allows a class to decide how it will react when it is treated like` |
|      - | 1040 | ` *  a string. For example, what echo $obj; will print. This method must return a string.` |
|      - | 1041 | ` *  Example #2 Simple example` |
|      - | 1042 | ` * <?php` |
|      - | 1043 | ` * // Declare a simple class` |
|      - | 1044 | ` * class TestClass` |
|      - | 1045 | ` * {` |
|      - | 1046 | ` *   public $foo;` |
|      - | 1047 | ` *` |
|      - | 1048 | ` *   public function __construct($foo)` |
|      - | 1049 | ` *   {` |
|      - | 1050 | ` *       $this->foo = $foo;` |
|      - | 1051 | ` *   }` |
|      - | 1052 | ` *` |
|      - | 1053 | ` *   public function __toString()` |
|      - | 1054 | ` *   {` |
|      - | 1055 | ` *       return $this->foo;` |
|      - | 1056 | ` *   }` |
|      - | 1057 | ` * }` |
|      - | 1058 | ` * $class = new TestClass('Hello');` |
|      - | 1059 | ` * echo $class;` |
|      - | 1060 | ` * ?>` |
|      - | 1061 | ` * The above example will output:` |
|      - | 1062 | ` *  Hello` |
|      - | 1063 | ` *` |
|      - | 1064 | ` * Note that PH7 does not support all the magical method and introudces __toFloat(),__toInt()` |
|      - | 1065 | ` * which have the same behaviour as __toString() but for float and integer types` |
|      - | 1066 | ` * respectively.` |
|      - | 1067 | ` * Refer to the official documentation for more information.` |
|      - | 1068 | ` */` |
|      4 | 1069 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallMagicMethod(` |
|      - | 1070 | `	ph7_vm *pVm,               /* VM that own all this stuff */` |
|      - | 1071 | `	ph7_class *pClass,         /* Target class */` |
|      - | 1072 | `	ph7_class_instance *pThis, /* Target object */` |
|      - | 1073 | `	const char *zMethod,       /* Magic method name [i.e: __toString()]*/` |
|      - | 1074 | `	sxu32 nByte,               /* zMethod length*/` |
|      - | 1075 | `	const SyString *pAttrName  /* Attribute name */` |
|      - | 1076 | `	)` |
|      2 | 1077 |  |
|      6 | 1078 | `	ph7_value *apArg[2] = { 0 , 0 };` |
|      - | 1079 | `	ph7_class_method *pMeth;` |
|      - | 1080 | `	ph7_value sAttr; /* cc warning */` |
|      - | 1081 | `	sxi32 rc;` |
|      - | 1082 | `	int nArg;` |
|      - | 1083 | `	/* Make sure the magic method is available */` |
|      6 | 1084 | `	pMeth = PH7_ClassExtractMethod(&(*pClass),zMethod,nByte);` |
|      6 | 1085 | `	if( pMeth == 0 ){` |
|      - | 1086 | `		/* No such method,return immediately */` |
|      3 | 1087 | `		return SXERR_NOTFOUND;` |
|      - | 1088 | `	}` |
|      3 | 1089 | `	nArg = 0;` |
|      - | 1090 | `	/* Copy arguments */` |
|      3 | 1091 | `	if( pAttrName ){` |
|    ! 0 | 1092 | `		PH7_MemObjInitFromString(pVm,&sAttr,pAttrName);` |
|    ! 0 | 1093 | `		sAttr.nIdx = SXU32_HIGH; /* Mark as constant */` |
|    ! 0 | 1094 | `		apArg[0] = &sAttr;` |
|    ! 0 | 1095 | `		nArg = 1;` |
|    ! 0 | 1096 | `	}` |
|      - | 1097 | `	/* Call the magic method now */` |
|      3 | 1098 | `	rc = PH7_VmCallClassMethod(pVm,&(*pThis),pMeth,0,nArg,apArg);` |
|      - | 1099 | `	/* Clean up */` |
|      3 | 1100 | `	if( pAttrName ){` |
|    ! 0 | 1101 | `		PH7_MemObjRelease(&sAttr);` |
|    ! 0 | 1102 | `	}` |
|      3 | 1103 | `	return rc;` |
|      4 | 1104 |  |
|      - | 1105 | `/*` |
|      - | 1106 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon].` |
|      - | 1107 | ` * This function is simply a wrapper on ExtractClassAttrValue().` |
|      - | 1108 | ` */` |
|     18 | 1109 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceExtractAttrValue(ph7_class_instance *pThis,VmClassAttr *pAttr)` |
|      1 | 1110 |  |
|      - | 1111 | `   /* Extract the attribute value */` |
|      - | 1112 | `	ph7_value *pValue;` |
|     19 | 1113 | `	pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     19 | 1114 | `	return pValue;` |
|      1 | 1115 |  |
|      - | 1116 | `/*` |
|      - | 1117 | ` * Convert a class instance [i.e: Object in the PHP jargon] into a hashmap [i.e: array in the PHP jargon].` |
|      - | 1118 | ` * Return SXRET_OK on success. Any other value indicates failure.` |
|      - | 1119 | ` * Note on object conversion to array:` |
|      - | 1120 | ` *  Acccording to the PHP language reference manual` |
|      - | 1121 | ` *  If an object is converted to an array, the result is an array whose elements are the object's properties.` |
|      - | 1122 | ` *  The keys are the member variable names.` |
|      - | 1123 | ` *` |
|      - | 1124 | ` *  The following example:` |
|      - | 1125 | ` *  class Test {` |
|      - | 1126 | ` *   public $A = 25<<1;  // 50` |
|      - | 1127 | ` *	 public $c = rand_str(3);   // Random string of length 3` |
|      - | 1128 | ` *	 public $d = rand() & 1023; // Random number between 0..1023` |
|      - | 1129 | ` *  }` |
|      - | 1130 | ` *  var_dump((array) new Test());` |
|      - | 1131 | ` *	Will output:` |
|      - | 1132 | ` *  array(3) {` |
|      - | 1133 | ` *   [A] =>` |
|      - | 1134 | ` *      int(50)` |
|      - | 1135 | ` *   [c] =>` |
|      - | 1136 | ` *     string(3 'aps')` |
|      - | 1137 | ` *   [d] =>` |
|      - | 1138 | ` *     int(991)` |
|      - | 1139 | ` *  }` |
|      - | 1140 | ` * You have noticed that PH7 allow class attributes [i.e: $a,$c,$d in the example above]` |
|      - | 1141 | ` * have any complex expression (even function calls/Annonymous functions) as their default` |
|      - | 1142 | ` * value unlike the standard PHP engine.` |
|      - | 1143 | ` * This is a very powerful feature that you have to look at.` |
|      - | 1144 | ` */` |
|      6 | 1145 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|      1 | 1146 |  |
|      - | 1147 | `	SyHashEntry *pEntry;` |
|      - | 1148 | `	SyString *pAttrName;` |
|      - | 1149 | `	VmClassAttr *pAttr;` |
|      - | 1150 | `	ph7_value *pValue;` |
|      - | 1151 | `	ph7_value sName;` |
|      - | 1152 | `	/* Reset the loop cursor */` |
|      7 | 1153 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      7 | 1154 | `	PH7_MemObjInitFromString(pThis->pVm,&sName,0);` |
|     20 | 1155 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|      - | 1156 | `		/* Point to the current attribute */` |
|     11 | 1157 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1158 | `		/* Extract attribute value */` |
|     11 | 1159 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     11 | 1160 | `		if( pValue ){` |
|      - | 1161 | `			/* Build attribute name */` |
|     11 | 1162 | `			pAttrName = &pAttr->pAttr->sName;` |
|     11 | 1163 | `			PH7_MemObjStringAppend(&sName,pAttrName->zString,pAttrName->nByte);` |
|      - | 1164 | `			/* Perform the insertion */` |
|     11 | 1165 | `			PH7_HashmapInsert(pMap,&sName,pValue);` |
|      - | 1166 | `			/* Reset the string cursor */` |
|     11 | 1167 | `			SyBlobReset(&sName.sBlob);` |
|      5 | 1168 | `		}` |
|      1 | 1169 | `	}` |
|      7 | 1170 | `	PH7_MemObjRelease(&sName);` |
|      7 | 1171 | `	return SXRET_OK;` |
|      1 | 1172 |  |
|      - | 1173 | `/*` |
|      - | 1174 | ` * Iterate throw class attributes and invoke the given callback [i.e: xWalk()] for each` |
|      - | 1175 | ` * retrieved attribute.` |
|      - | 1176 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|      - | 1177 | ` * the attribute value in the callback body will not alter the real attribute value.` |
|      - | 1178 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|      - | 1179 | ` * a value different from PH7_OK.` |
|      - | 1180 | ` * Refer to [ph7_object_walk()] for more information.` |
|      - | 1181 | ` */` |
|    ! 0 | 1182 | `PH7_PRIVATE sxi32 PH7_ClassInstanceWalk(` |
|      - | 1183 | `	ph7_class_instance *pThis, /* Target object */` |
|      - | 1184 | `	int (*xWalk)(const char *,ph7_value *,void *), /* Walker callback */` |
|      - | 1185 | `	void *pUserData /* Last argument to xWalk() */` |
|      - | 1186 | `	)` |
|    ! 0 | 1187 |  |
|      - | 1188 | `	SyHashEntry *pEntry; /* Hash entry */` |
|      - | 1189 | `	VmClassAttr *pAttr;  /* Pointer to the attribute */` |
|      - | 1190 | `	ph7_value *pValue;   /* Attribute value */` |
|      - | 1191 | `	ph7_value sValue;    /* Copy of the attribute value */` |
|      - | 1192 | `	int rc;` |
|      - | 1193 | `	/* Reset the loop cursor */` |
|    ! 0 | 1194 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|    ! 0 | 1195 | `	PH7_MemObjInit(pThis->pVm,&sValue);` |
|      - | 1196 | `	/* Start the walk process */` |
|    ! 0 | 1197 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|      - | 1198 | `		/* Point to the current attribute */` |
|    ! 0 | 1199 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1200 | `		/* Extract attribute value */` |
|    ! 0 | 1201 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|    ! 0 | 1202 | `		if( pValue ){` |
|    ! 0 | 1203 | `			PH7_MemObjLoad(pValue,&sValue);` |
|      - | 1204 | `			/* Invoke the supplied callback */` |
|    ! 0 | 1205 | `			rc =  xWalk(SyStringData(&pAttr->pAttr->sName),&sValue,pUserData);` |
|    ! 0 | 1206 | `			PH7_MemObjRelease(&sValue);` |
|    ! 0 | 1207 | `			if( rc != PH7_OK){` |
|      - | 1208 | `				/* User callback request an operation abort */` |
|    ! 0 | 1209 | `				return SXERR_ABORT;` |
|      - | 1210 | `			}` |
|    ! 0 | 1211 | `		}` |
|    ! 0 | 1212 | `	}` |
|      - | 1213 | `	/* All done */` |
|    ! 0 | 1214 | `	return SXRET_OK;` |
|    ! 0 | 1215 |  |
|      - | 1216 | `/*` |
|      - | 1217 | ` * Extract a class atrribute value.` |
|      - | 1218 | ` * Return a pointer to the attribute value on success. Otherwise NULL.` |
|      - | 1219 | ` * Note:` |
|      - | 1220 | ` *  Access to static and constant attribute is not allowed. That is,the function` |
|      - | 1221 | ` *  will return NULL in case someone (host-application code) try to extract` |
|      - | 1222 | ` *  a static/constant attribute.` |
|      - | 1223 | ` */` |
|    ! 0 | 1224 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceFetchAttr(ph7_class_instance *pThis,const SyString *pName)` |
|    ! 0 | 1225 |  |
|      - | 1226 | `	SyHashEntry *pEntry;` |
|      - | 1227 | `	VmClassAttr *pAttr;` |
|      - | 1228 | `	/* Query the attribute hashtable */` |
|    ! 0 | 1229 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);` |
|    ! 0 | 1230 | `	if( pEntry == 0 ){` |
|      - | 1231 | `		/* No such attribute */` |
|    ! 0 | 1232 | `		return 0;` |
|      - | 1233 | `	}` |
|      - | 1234 | `	/* Point to the class atrribute */` |
|    ! 0 | 1235 | `	pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1236 | `	/* Check if we are dealing with a static/constant attribute */` |
|    ! 0 | 1237 | `	if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|      - | 1238 | `		/* Access is forbidden */` |
|    ! 0 | 1239 | `		return 0;` |
|      - | 1240 | `	}` |
|      - | 1241 | `	/* Return the attribute value */` |
|    ! 0 | 1242 | `	return ExtractClassAttrValue(pThis->pVm,pAttr);` |
|    ! 0 | 1243 |  |
|      - | 1244 |  |
