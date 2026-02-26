# src/ph7/oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 361/421 lines (85.75%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|     - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    5 | ` */` |
|     - |    6 | `#include "ph7int.h"` |
|     - |    7 | `/*` |
|     - |    8 | ` * This file implement an Object Oriented (OO) subsystem for the PH7 engine.` |
|     - |    9 | ` */` |
|     - |   10 | `/*` |
|     - |   11 | ` * Create an empty class.` |
|     - |   12 | ` * Return a pointer to a raw class (ph7_class instance) on success. NULL otherwise.` |
|     - |   13 | ` */` |
| 16098 |   14 | `PH7_PRIVATE ph7_class * PH7_NewRawClass(ph7_vm *pVm,const SyString *pName,sxu32 nLine)` |
|     2 |   15 |  |
|     - |   16 | `	ph7_class *pClass;` |
|     - |   17 | `	char *zName;` |
|     - |   18 | `	/* Allocate a new instance */` |
| 16100 |   19 | `	pClass = (ph7_class *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class));` |
| 16100 |   20 | `	if( pClass == 0 ){` |
|   ! 0 |   21 | `		return 0;` |
|     - |   22 | `	}` |
|     - |   23 | `	/* Zero the structure */` |
| 16100 |   24 | `	SyZero(pClass,sizeof(ph7_class));` |
|     - |   25 | `	/* Duplicate class name */` |
| 16100 |   26 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
| 16100 |   27 | `	if( zName == 0 ){` |
|   ! 0 |   28 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClass);` |
|   ! 0 |   29 | `		return 0;` |
|     - |   30 | `	}` |
|     - |   31 | `	/* Initialize fields */` |
| 16100 |   32 | `	SyStringInitFromBuf(&pClass->sName,zName,pName->nByte);` |
| 16100 |   33 | `	SyHashInit(&pClass->hMethod,&pVm->sAllocator,0,0);` |
| 16100 |   34 | `	SyHashInit(&pClass->hAttr,&pVm->sAllocator,0,0);` |
| 16100 |   35 | `	SyHashInit(&pClass->hDerived,&pVm->sAllocator,0,0);` |
| 16100 |   36 | `	SySetInit(&pClass->aInterface,&pVm->sAllocator,sizeof(ph7_class *));` |
| 16100 |   37 | `	pClass->nLine = nLine;` |
|     - |   38 | `	/* All done */` |
| 16100 |   39 | `	return pClass;` |
|  8051 |   40 |  |
|     - |   41 | `/*` |
|     - |   42 | ` * Allocate and initialize a new class attribute.` |
|     - |   43 | ` * Return a pointer to the class attribute on success. NULL otherwise.` |
|     - |   44 | ` */` |
| 14584 |   45 | `PH7_PRIVATE ph7_class_attr * PH7_NewClassAttr(ph7_vm *pVm,const SyString *pName,sxu32 nLine,sxi32 iProtection,sxi32 iFlags)` |
|     2 |   46 |  |
|     - |   47 | `	ph7_class_attr *pAttr;` |
|     - |   48 | `	char *zName;` |
| 14586 |   49 | `	pAttr = (ph7_class_attr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_attr));` |
| 14586 |   50 | `	if( pAttr == 0 ){` |
|   ! 0 |   51 | `		return 0;` |
|     - |   52 | `	}` |
|     - |   53 | `	/* Zero the structure */` |
| 14586 |   54 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|     - |   55 | `	/* Duplicate attribute name */` |
| 14586 |   56 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
| 14586 |   57 | `	if( zName == 0 ){` |
|   ! 0 |   58 | `		SyMemBackendPoolFree(&pVm->sAllocator,pAttr);` |
|   ! 0 |   59 | `		return 0;` |
|     - |   60 | `	}` |
|     - |   61 | `	/* Initialize fields */` |
| 14586 |   62 | `	SySetInit(&pAttr->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
| 14586 |   63 | `	SyStringInitFromBuf(&pAttr->sName,zName,pName->nByte);` |
| 14586 |   64 | `	pAttr->iProtection = iProtection;` |
| 14586 |   65 | `	pAttr->nIdx = SXU32_HIGH;` |
| 14586 |   66 | `	pAttr->iFlags = iFlags;` |
| 14586 |   67 | `	pAttr->nLine = nLine;` |
| 14586 |   68 | `	return pAttr;` |
|  7294 |   69 |  |
|     - |   70 | `/*` |
|     - |   71 | ` * Allocate and initialize a new class method.` |
|     - |   72 | ` * Return a pointer to the class method on success. NULL otherwise` |
|     - |   73 | ` * This function associate with the newly created method an automatically generated` |
|     - |   74 | ` * random unique name.` |
|     - |   75 | ` */` |
| 41968 |   76 | `PH7_PRIVATE ph7_class_method * PH7_NewClassMethod(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,sxu32 nLine,` |
|     - |   77 | `	sxi32 iProtection,sxi32 iFlags,sxi32 iFuncFlags)` |
|     2 |   78 |  |
|     - |   79 | `	ph7_class_method *pMeth;` |
|     - |   80 | `	SyHashEntry *pEntry;` |
|     - |   81 | `	SyString *pNamePtr;` |
|     - |   82 | `	char zSalt[10];` |
|     - |   83 | `	char *zName;` |
|     - |   84 | `	sxu32 nByte;` |
|     - |   85 | `	/* Allocate a new class method instance */` |
| 41970 |   86 | `	pMeth = (ph7_class_method *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_method));` |
| 41970 |   87 | `	if( pMeth == 0 ){` |
|   ! 0 |   88 | `		return 0;` |
|     - |   89 | `	}` |
|     - |   90 | `	/* Zero the structure */` |
| 41970 |   91 | `	SyZero(pMeth,sizeof(ph7_class_method));` |
|     - |   92 | `	/* Check for an already installed method with the same name */` |
| 41970 |   93 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
| 41970 |   94 | `	if( pEntry == 0 ){` |
|     - |   95 | `		/* Associate an unique VM name to this method */` |
| 41968 |   96 | `		nByte = sizeof(zSalt) + pName->nByte + SyStringLength(&pClass->sName)+sizeof(char)*7/*[[__'\0'*/;` |
| 41968 |   97 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,nByte);` |
| 41968 |   98 | `		if( zName == 0 ){` |
|   ! 0 |   99 | `			SyMemBackendPoolFree(&pVm->sAllocator,pMeth);` |
|   ! 0 |  100 | `			return 0;` |
|     - |  101 | `		}` |
| 41968 |  102 | `		pNamePtr = &pMeth->sVmName;` |
|     - |  103 | `		/* Generate a random string */` |
| 41968 |  104 | `		PH7_VmRandomString(&(*pVm),zSalt,sizeof(zSalt));` |
| 41968 |  105 | `		pNamePtr->nByte = SyBufferFormat(zName,nByte,"[__%z@%z_%.*s]",&pClass->sName,pName,sizeof(zSalt),zSalt);` |
| 41968 |  106 | `		pNamePtr->zString = zName;` |
| 20985 |  107 | `	}else{` |
|     - |  108 | `		/* Method is condidate for 'overloading' */` |
|     3 |  109 | `		ph7_class_method *pCurrent = (ph7_class_method *)pEntry->pUserData;` |
|     3 |  110 | `		pNamePtr = &pMeth->sVmName;` |
|     - |  111 | `		/* Use the same VM name */` |
|     3 |  112 | `		SyStringDupPtr(pNamePtr,&pCurrent->sVmName);` |
|     3 |  113 | `		zName = (char *)pNamePtr->zString;` |
|     - |  114 | `	}` |
| 41970 |  115 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|    23 |  116 | `		if( (pName->nByte == sizeof("__construct") - 1 && SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1 ) == 0)` |
|    15 |  117 | `			\|\| (pName->nByte == sizeof("__destruct") - 1 && SyMemcmp(pName->zString,"__destruct",sizeof("__destruct") - 1 ) == 0)` |
|    15 |  118 | `			\|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|     - |  119 | `				/* Switch to public visibility when dealing with constructor/destructor */` |
|     5 |  120 | `				iProtection = PH7_CLASS_PROT_PUBLIC;` |
|     2 |  121 | `		}` |
|     9 |  122 | `	}` |
|     - |  123 | `	/* Initialize method fields */` |
| 41972 |  124 | `	pMeth->iProtection = iProtection;` |
| 41972 |  125 | `	pMeth->iFlags = iFlags;` |
| 41972 |  126 | `	pMeth->nLine = nLine;` |
| 62958 |  127 | `	PH7_VmInitFuncState(&(*pVm),&pMeth->sFunc,&zName[sizeof(char)*4/*[__@*/+SyStringLength(&pClass->sName)],` |
| 41970 |  128 | `		pName->nByte,iFuncFlags\|VM_FUNC_CLASS_METHOD,pClass);` |
| 41972 |  129 | `	return pMeth;` |
| 20988 |  130 |  |
|     - |  131 | `/*` |
|     - |  132 | ` * Check if the given name have a class method associated with it.` |
|     - |  133 | ` * Return the desired method [i.e: ph7_class_method instance] on success. NULL otherwise.` |
|     - |  134 | ` */` |
|  2488 |  135 | `PH7_PRIVATE ph7_class_method * PH7_ClassExtractMethod(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|     2 |  136 |  |
|     - |  137 | `	SyHashEntry *pEntry;` |
|     - |  138 | `	/* Perform a hash lookup */` |
|  2490 |  139 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)zName,nByte);` |
|  2490 |  140 | `	if( pEntry == 0 ){` |
|     - |  141 | `		/* No such entry */` |
|  1308 |  142 | `		return 0;` |
|     - |  143 | `	}` |
|     - |  144 | `	/* Point to the desired method */` |
|  1184 |  145 | `	return (ph7_class_method *)pEntry->pUserData;` |
|  1246 |  146 |  |
|     - |  147 | `/*` |
|     - |  148 | ` * Check if the given name is a class attribute.` |
|     - |  149 | ` * Return the desired attribute [i.e: ph7_class_attr instance] on success.NULL otherwise.` |
|     - |  150 | ` */` |
|     8 |  151 | `PH7_PRIVATE ph7_class_attr * PH7_ClassExtractAttribute(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|     1 |  152 |  |
|     - |  153 | `	SyHashEntry *pEntry;` |
|     - |  154 | `	/* Perform a hash lookup */` |
|     9 |  155 | `	pEntry = SyHashGet(&pClass->hAttr,(const void *)zName,nByte);` |
|     9 |  156 | `	if( pEntry == 0 ){` |
|     - |  157 | `		/* No such entry */` |
|   ! 0 |  158 | `		return 0;` |
|     - |  159 | `	}` |
|     - |  160 | `	/* Point to the desierd method */` |
|     9 |  161 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|     5 |  162 |  |
|     - |  163 | `/*` |
|     - |  164 | ` * Install a class attribute in the corresponding container.` |
|     - |  165 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|     - |  166 | ` */` |
| 14584 |  167 | `PH7_PRIVATE sxi32 PH7_ClassInstallAttr(ph7_class *pClass,ph7_class_attr *pAttr)` |
|     2 |  168 |  |
| 14586 |  169 | `	SyString *pName = &pAttr->sName;` |
|     - |  170 | `	sxi32 rc;` |
| 14586 |  171 | `	rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
| 14586 |  172 | `	return rc;` |
|     2 |  173 |  |
|     - |  174 | `/*` |
|     - |  175 | ` * Install a class method in the corresponding container.` |
|     - |  176 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|     - |  177 | ` */` |
| 41966 |  178 | `PH7_PRIVATE sxi32 PH7_ClassInstallMethod(ph7_class *pClass,ph7_class_method *pMeth)` |
|     2 |  179 |  |
| 41968 |  180 | `	SyString *pName = &pMeth->sFunc.sName;` |
|     - |  181 | `	sxi32 rc;` |
| 41968 |  182 | `	rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 41968 |  183 | `	return rc;` |
|     2 |  184 |  |
|     - |  185 | `/*` |
|     - |  186 | ` * Perform an inheritance operation.` |
|     - |  187 | ` * According to the PHP language reference manual` |
|     - |  188 | ` *  When you extend a class, the subclass inherits all of the public and protected methods` |
|     - |  189 | ` *  from the parent class. Unless a class Overwrites those methods, they will retain their original` |
|     - |  190 | ` *  functionality.` |
|     - |  191 | ` *  This is useful for defining and abstracting functionality, and permits the implementation` |
|     - |  192 | ` *  of additional functionality in similar objects without the need to reimplement all of the shared` |
|     - |  193 | ` *  functionality.` |
|     - |  194 | ` *  Example #1 Inheritance Example` |
|     - |  195 | ` * <?php` |
|     - |  196 | ` * class foo` |
|     - |  197 | ` * {` |
|     - |  198 | ` *   public function printItem($string)` |
|     - |  199 | ` *   {` |
|     - |  200 | ` *       echo 'Foo: ' . $string . PHP_EOL;` |
|     - |  201 | ` *   }` |
|     - |  202 | ` *` |
|     - |  203 | ` *   public function printPHP()` |
|     - |  204 | ` *   {` |
|     - |  205 | ` *       echo 'PHP is great.' . PHP_EOL;` |
|     - |  206 | ` *   }` |
|     - |  207 | ` * }` |
|     - |  208 | ` * class bar extends foo` |
|     - |  209 | ` * {` |
|     - |  210 | ` *   public function printItem($string)` |
|     - |  211 | ` *   {` |
|     - |  212 | ` *       echo 'Bar: ' . $string . PHP_EOL;` |
|     - |  213 | ` *   }` |
|     - |  214 | ` * }` |
|     - |  215 | ` * $foo = new foo();` |
|     - |  216 | ` * $bar = new bar();` |
|     - |  217 | ` * $foo->printItem('baz'); // Output: 'Foo: baz'` |
|     - |  218 | ` * $foo->printPHP();       // Output: 'PHP is great'` |
|     - |  219 | ` * $bar->printItem('baz'); // Output: 'Bar: baz'` |
|     - |  220 | ` * $bar->printPHP();       // Output: 'PHP is great'` |
|     - |  221 | ` *` |
|     - |  222 | ` * This function return SXRET_OK if the inheritance operation was successfully performed.` |
|     - |  223 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|     - |  224 | ` * error message.` |
|     - |  225 | ` */` |
|  7238 |  226 | `PH7_PRIVATE sxi32 PH7_ClassInherit(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase)` |
|     2 |  227 |  |
|     - |  228 | `	ph7_class_method *pMeth;` |
|     - |  229 | `	ph7_class_attr *pAttr;` |
|     - |  230 | `	SyHashEntry *pEntry;` |
|     - |  231 | `	SyString *pName;` |
|     - |  232 | `	sxi32 rc;` |
|     - |  233 | `	/* Install in the derived hashtable */` |
|  7240 |  234 | `	rc = SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|  7240 |  235 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  236 | `		return rc;` |
|     - |  237 | `	}` |
|     - |  238 | `	/* Copy public/protected attributes from the base class */` |
|  7240 |  239 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
| 50464 |  240 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|     - |  241 | `		/* Make sure the private attributes are not redeclared in the subclass */` |
| 43226 |  242 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
| 43226 |  243 | `		pName = &pAttr->sName;` |
| 43226 |  244 | `		if( (pEntry = SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|     3 |  245 | `			if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE &&` |
|     2 |  246 | `				((ph7_class_attr *)pEntry->pUserData)->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|     - |  247 | `					/* Cannot redeclare private attribute */` |
|     4 |  248 | `					PH7_GenCompileError(&(*pGen),E_WARNING,((ph7_class_attr *)pEntry->pUserData)->nLine,` |
|     - |  249 | `						"Private attribute '%z::%z' redeclared inside child class '%z'",` |
|     1 |  250 | `						&pBase->sName,pName,&pSub->sName);` |
|     - |  251 |  |
|     1 |  252 | `			}` |
|     3 |  253 | `			continue;` |
|     - |  254 | `		}` |
|     - |  255 | `		/* Install the attribute */` |
| 43224 |  256 | `		if( pAttr->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
| 43222 |  257 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
| 43222 |  258 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  259 | `				return rc;` |
|     - |  260 | `			}` |
| 21610 |  261 | `		}` |
|     2 |  262 | `	}` |
|  7240 |  263 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
| 72116 |  264 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|     - |  265 | `		/* Make sure the private/final methods are not redeclared in the subclass */` |
| 64878 |  266 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
| 64878 |  267 | `		pName = &pMeth->sFunc.sName;` |
| 64878 |  268 | `		if( (pEntry = SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|  1468 |  269 | `			 if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|     - |  270 | `				/* Cannot Overwrite final method */` |
|     7 |  271 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_method *)pEntry->pUserData)->nLine,` |
|     - |  272 | `					"Cannot Overwrite final method '%z:%z' inside child class '%z'",` |
|     2 |  273 | `					&pBase->sName,pName,&pSub->sName);` |
|     5 |  274 | `				if( rc == SXERR_ABORT ){` |
|   ! 0 |  275 | `					return SXERR_ABORT;` |
|     - |  276 | `				}` |
|     2 |  277 | `			}` |
|  1468 |  278 | `			continue;` |
|   ! 0 |  279 | `		}else{` |
| 63412 |  280 | `			if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|     - |  281 | `				/* Abstract method must be defined in the child class */` |
|     4 |  282 | `				PH7_GenCompileError(&(*pGen),E_WARNING,pMeth->nLine,` |
|     - |  283 | `					"Abstract method '%z:%z' must be defined inside child class '%z'",` |
|     1 |  284 | `					&pBase->sName,pName,&pSub->sName);` |
|     3 |  285 | `				continue;` |
|     - |  286 | `			}` |
|     - |  287 | `		}` |
|     - |  288 | `		/* Install the method */` |
| 63410 |  289 | `		if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
| 63408 |  290 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 63408 |  291 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  292 | `				return rc;` |
|     - |  293 | `			}` |
| 31703 |  294 | `		}` |
|     2 |  295 | `	}` |
|     - |  296 | `	/* Mark as subclass */` |
|  7240 |  297 | `	pSub->pBase = pBase;` |
|     - |  298 | `	/* All done */` |
|  7240 |  299 | `	return SXRET_OK;` |
|  3621 |  300 |  |
|     - |  301 | `/*` |
|     - |  302 | ` * Inherit an object interface from another object interface.` |
|     - |  303 | ` * According to the PHP language reference manual.` |
|     - |  304 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|     - |  305 | ` *  must implement, without having to define how these methods are handled.` |
|     - |  306 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|     - |  307 | ` *  class, but without any of the methods having their contents defined.` |
|     - |  308 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|     - |  309 | ` *` |
|     - |  310 | ` * This function return SXRET_OK if the interface inheritance operation was successfully performed.` |
|     - |  311 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|     - |  312 | ` * error message.` |
|     - |  313 | ` */` |
|     2 |  314 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceInherit(ph7_class *pSub,ph7_class *pBase)` |
|     1 |  315 |  |
|     - |  316 | `	ph7_class_method *pMeth;` |
|     - |  317 | `	ph7_class_attr *pAttr;` |
|     - |  318 | `	SyHashEntry *pEntry;` |
|     - |  319 | `	SyString *pName;` |
|     - |  320 | `	sxi32 rc;` |
|     - |  321 | `	/* Install in the derived hashtable */` |
|     3 |  322 | `	SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|     3 |  323 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
|     - |  324 | `	/* Copy constants */` |
|     6 |  325 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|     - |  326 | `		/* Make sure the constants are not redeclared in the subclass */` |
|     3 |  327 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     3 |  328 | `		pName = &pAttr->sName;` |
|     3 |  329 | `		if( SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte) == 0 ){` |
|     - |  330 | `			/* Install the constant in the subclass */` |
|     3 |  331 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|     3 |  332 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  333 | `				return rc;` |
|     - |  334 | `			}` |
|     1 |  335 | `		}` |
|     1 |  336 | `	}` |
|     3 |  337 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
|     - |  338 | `	/* Copy methods signature */` |
|     6 |  339 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|     - |  340 | `		/* Make sure the method are not redeclared in the subclass */` |
|     3 |  341 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|     3 |  342 | `		pName = &pMeth->sFunc.sName;` |
|     3 |  343 | `		if( SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte) == 0 ){` |
|     - |  344 | `			/* Install the method */` |
|     3 |  345 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|     3 |  346 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  347 | `				return rc;` |
|     - |  348 | `			}` |
|     1 |  349 | `		}` |
|     1 |  350 | `	}` |
|     - |  351 | `	/* Mark as subclass */` |
|     3 |  352 | `	pSub->pBase = pBase;` |
|     - |  353 | `	/* All done */` |
|     3 |  354 | `	return SXRET_OK;` |
|     2 |  355 |  |
|     - |  356 | `/*` |
|     - |  357 | ` * Implements an object interface in the given main class.` |
|     - |  358 | ` * According to the PHP language reference manual.` |
|     - |  359 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|     - |  360 | ` *  must implement, without having to define how these methods are handled.` |
|     - |  361 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|     - |  362 | ` *  class, but without any of the methods having their contents defined.` |
|     - |  363 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|     - |  364 | ` *` |
|     - |  365 | ` * This function return SXRET_OK if the interface was successfully implemented.` |
|     - |  366 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|     - |  367 | ` * error message.` |
|     - |  368 | ` */` |
|     4 |  369 | `PH7_PRIVATE sxi32 PH7_ClassImplement(ph7_class *pMain,ph7_class *pInterface)` |
|     1 |  370 |  |
|     - |  371 | `	ph7_class_attr *pAttr;` |
|     - |  372 | `	SyHashEntry *pEntry;` |
|     - |  373 | `	SyString *pName;` |
|     - |  374 | `	sxi32 rc;` |
|     - |  375 | `	/* First off,copy all constants declared inside the interface */` |
|     5 |  376 | `	SyHashResetLoopCursor(&pInterface->hAttr);` |
|     9 |  377 | `	while((pEntry = SyHashGetNextEntry(&pInterface->hAttr)) != 0 ){` |
|     - |  378 | `		/* Point to the constant declaration */` |
|     3 |  379 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     3 |  380 | `		pName = &pAttr->sName;` |
|     - |  381 | `		/* Make sure the attribute is not redeclared in the main class */` |
|     3 |  382 | `		if( SyHashGet(&pMain->hAttr,pName->zString,pName->nByte) == 0 ){` |
|     - |  383 | `			/* Install the attribute */` |
|     3 |  384 | `			rc = SyHashInsert(&pMain->hAttr,pName->zString,pName->nByte,pAttr);` |
|     3 |  385 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  386 | `				return rc;` |
|     - |  387 | `			}` |
|     1 |  388 | `		}` |
|     1 |  389 | `	}` |
|     - |  390 | `	/* Install in the interface container */` |
|     5 |  391 | `	SySetPut(&pMain->aInterface,(const void *)&pInterface);` |
|     - |  392 | `	/* TICKET 1433-49/1: Symisc eXtension` |
|     - |  393 | `	 *  A class may not implemnt all declared interface methods,so there` |
|     - |  394 | `	 *  is no need for a method installer loop here.` |
|     - |  395 | `	 */` |
|     5 |  396 | `	return SXRET_OK;` |
|     3 |  397 |  |
|     - |  398 | `/*` |
|     - |  399 | ` * Create a class instance [i.e: Object in the PHP jargon] at run-time.` |
|     - |  400 | ` * The following function is called when an object is created at run-time` |
|     - |  401 | ` * typically when the PH7_OP_NEW/PH7_OP_CLONE instructions are executed.` |
|     - |  402 | ` * Notes on object creation.` |
|     - |  403 | ` *` |
|     - |  404 | ` * According to PHP language reference manual.` |
|     - |  405 | ` * To create an instance of a class, the new keyword must be used. An object will always` |
|     - |  406 | ` * be created unless the object has a constructor defined that throws an exception on error.` |
|     - |  407 | ` * Classes should be defined before instantiation (and in some cases this is a requirement).` |
|     - |  408 | ` * If a string containing the name of a class is used with new, a new instance of that class` |
|     - |  409 | ` * will be created. If the class is in a namespace, its fully qualified name must be used when` |
|     - |  410 | ` * doing this.` |
|     - |  411 | ` * Example #3 Creating an instance` |
|     - |  412 | ` * <?php` |
|     - |  413 | ` *  $instance = new SimpleClass();` |
|     - |  414 | ` *   // This can also be done with a variable:` |
|     - |  415 | ` * $className = 'Foo';` |
|     - |  416 | ` * $instance = new $className(); // Foo()` |
|     - |  417 | ` * ?>` |
|     - |  418 | ` * In the class context, it is possible to create a new object by new self and new parent.` |
|     - |  419 | ` * When assigning an already created instance of a class to a new variable, the new variable` |
|     - |  420 | ` * will access the same instance as the object that was assigned. This behaviour is the same` |
|     - |  421 | ` * when passing instances to a function. A copy of an already created object can be made by` |
|     - |  422 | ` * cloning it.` |
|     - |  423 | ` * Example #4 Object Assignment` |
|     - |  424 | ` * <?php` |
|     - |  425 | ` *  class SimpleClass(){` |
|     - |  426 | ` *    public $var;` |
|     - |  427 | ` *  };` |
|     - |  428 | ` *  $instance = new SimpleClass();` |
|     - |  429 | ` *  $assigned   =  $instance;` |
|     - |  430 | ` *  $reference  =& $instance;` |
|     - |  431 | ` *  $instance->var = '$assigned will have this value';` |
|     - |  432 | ` *  $instance = null; // $instance and $reference become null` |
|     - |  433 | ` *  var_dump($instance);` |
|     - |  434 | ` *  var_dump($reference);` |
|     - |  435 | ` *  var_dump($assigned);` |
|     - |  436 | ` * ?>` |
|     - |  437 | ` * The above example will output:` |
|     - |  438 | ` * NULL` |
|     - |  439 | ` * NULL` |
|     - |  440 | ` * object(SimpleClass)#1 (1) {` |
|     - |  441 | ` *  ["var"]=>` |
|     - |  442 | ` *    string(30) "$assigned will have this value"` |
|     - |  443 | ` * }` |
|     - |  444 | ` * Example #5 Creating new objects` |
|     - |  445 | ` * <?php` |
|     - |  446 | ` * class Test` |
|     - |  447 | ` * {` |
|     - |  448 | ` *   static public function getNew()` |
|     - |  449 | ` *   {` |
|     - |  450 | ` *       return new static;` |
|     - |  451 | ` *   }` |
|     - |  452 | ` * }` |
|     - |  453 | ` * class Child extends Test` |
|     - |  454 | ` * {}` |
|     - |  455 | ` * $obj1 = new Test();` |
|     - |  456 | ` * $obj2 = new $obj1;` |
|     - |  457 | ` * var_dump($obj1 !== $obj2);` |
|     - |  458 | ` * $obj3 = Test::getNew();` |
|     - |  459 | ` * var_dump($obj3 instanceof Test);` |
|     - |  460 | ` * $obj4 = Child::getNew();` |
|     - |  461 | ` * var_dump($obj4 instanceof Child);` |
|     - |  462 | ` * ?>` |
|     - |  463 | ` * The above example will output:` |
|     - |  464 | ` * bool(true)` |
|     - |  465 | ` * bool(true)` |
|     - |  466 | ` * bool(true)` |
|     - |  467 | ` * Note that Symisc Systems have introduced powerfull extension to` |
|     - |  468 | ` * OO subsystem. For example a class attribute may have any complex` |
|     - |  469 | ` * expression associated with it when declaring the attribute unlike` |
|     - |  470 | ` * the standard PHP engine which would allow a single value.` |
|     - |  471 | ` * Example:` |
|     - |  472 | ` *  class myClass{` |
|     - |  473 | ` *    public $var = 25<<1+foo()/bar();` |
|     - |  474 | ` *  };` |
|     - |  475 | ` * Refer to the official documentation for more information.` |
|     - |  476 | ` */` |
|   676 |  477 | `static ph7_class_instance * NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|     2 |  478 |  |
|     - |  479 | `	ph7_class_instance *pThis;` |
|     - |  480 | `	/* Allocate a new instance */` |
|   678 |  481 | `	pThis = (ph7_class_instance *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_instance));` |
|   678 |  482 | `	if( pThis == 0 ){` |
|   ! 0 |  483 | `		return 0;` |
|     - |  484 | `	}` |
|     - |  485 | `	/* Zero the structure */` |
|   678 |  486 | `	SyZero(pThis,sizeof(ph7_class_instance));` |
|     - |  487 | `	/* Initialize fields */` |
|   678 |  488 | `	pThis->iRef = 1;` |
|   678 |  489 | `	pThis->pVm = pVm;` |
|   678 |  490 | `	pThis->pClass = pClass;` |
|   678 |  491 | `	SyHashInit(&pThis->hAttr,&pVm->sAllocator,0,0);` |
|   678 |  492 | `	return pThis;` |
|   340 |  493 |  |
|     - |  494 | `/*` |
|     - |  495 | ` * Wrapper around the NewClassInstance() function defined above.` |
|     - |  496 | ` * See the block comment above for more information.` |
|     - |  497 | ` */` |
|   634 |  498 | `PH7_PRIVATE ph7_class_instance * PH7_NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|     2 |  499 |  |
|     - |  500 | `	ph7_class_instance *pNew;` |
|     - |  501 | `	sxi32 rc;` |
|   636 |  502 | `	pNew = NewClassInstance(&(*pVm),&(*pClass));` |
|   636 |  503 | `	if( pNew == 0 ){` |
|   ! 0 |  504 | `		return 0;` |
|     - |  505 | `	}` |
|     - |  506 | `	/* Associate a private VM frame with this class instance */` |
|   636 |  507 | `	rc = PH7_VmCreateClassInstanceFrame(&(*pVm),pNew);` |
|   636 |  508 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  509 | `		SyMemBackendPoolFree(&pVm->sAllocator,pNew);` |
|   ! 0 |  510 | `		return 0;` |
|     - |  511 | `	}` |
|   636 |  512 | `	return pNew;` |
|   319 |  513 |  |
|     - |  514 | `/*` |
|     - |  515 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon] attribute.` |
|     - |  516 | ` * This function never fail.` |
|     - |  517 | ` */` |
|   540 |  518 | `static ph7_value * ExtractClassAttrValue(ph7_vm *pVm,VmClassAttr *pAttr)` |
|     2 |  519 |  |
|     - |  520 | `	/* Extract the value */` |
|     - |  521 | `	ph7_value *pValue;` |
|   542 |  522 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|   542 |  523 | `	return pValue;` |
|     2 |  524 |  |
|     - |  525 | `/*` |
|     - |  526 | ` * Perform a clone operation on a class instance [i.e: Object in the PHP jargon].` |
|     - |  527 | ` * The following function is called when an object is cloned at run-time` |
|     - |  528 | ` * typically when the PH7_OP_CLONE instruction is executed.` |
|     - |  529 | ` * Notes on object cloning.` |
|     - |  530 | ` *` |
|     - |  531 | ` * According to PHP language reference manual.` |
|     - |  532 | ` * Creating a copy of an object with fully replicated properties is not always the wanted behavior.` |
|     - |  533 | ` * A good example of the need for copy constructors. Another example is if your object holds a reference` |
|     - |  534 | ` * to another object which it uses and when you replicate the parent object you want to create` |
|     - |  535 | ` * a new instance of this other object so that the replica has its own separate copy.` |
|     - |  536 | ` * An object copy is created by using the clone keyword (which calls the object's __clone() method if possible).` |
|     - |  537 | ` * An object's __clone() method cannot be called directly.` |
|     - |  538 | ` * $copy_of_object = clone $object;` |
|     - |  539 | ` * When an object is cloned, PHP 5 will perform a shallow copy of all of the object's properties.` |
|     - |  540 | ` * Any properties that are references to other variables, will remain references.` |
|     - |  541 | ` * Once the cloning is complete, if a __clone() method is defined, then the newly created object's __clone() method` |
|     - |  542 | ` * will be called, to allow any necessary properties that need to be changed.` |
|     - |  543 | ` * Example #1 Cloning an object` |
|     - |  544 | ` * <?php` |
|     - |  545 | ` * class SubObject` |
|     - |  546 | ` * {` |
|     - |  547 | ` *   static $instances = 0;` |
|     - |  548 | ` *   public $instance;` |
|     - |  549 | ` *` |
|     - |  550 | ` *   public function __construct() {` |
|     - |  551 | ` *       $this->instance = ++self::$instances;` |
|     - |  552 | ` *   }` |
|     - |  553 | ` *` |
|     - |  554 | ` *   public function __clone() {` |
|     - |  555 | ` *       $this->instance = ++self::$instances;` |
|     - |  556 | ` *   }` |
|     - |  557 | ` * }` |
|     - |  558 | ` *` |
|     - |  559 | ` * class MyCloneable` |
|     - |  560 | ` * {` |
|     - |  561 | ` *   public $object1;` |
|     - |  562 | ` *   public $object2;` |
|     - |  563 | ` *` |
|     - |  564 | ` *   function __clone()` |
|     - |  565 | ` *   {` |
|     - |  566 | ` *       // Force a copy of this->object, otherwise` |
|     - |  567 | ` *       // it will point to same object.` |
|     - |  568 | ` *       $this->object1 = clone $this->object1;` |
|     - |  569 | ` *   }` |
|     - |  570 | ` * }` |
|     - |  571 | ` * $obj = new MyCloneable();` |
|     - |  572 | ` * $obj->object1 = new SubObject();` |
|     - |  573 | ` * $obj->object2 = new SubObject();` |
|     - |  574 | ` * $obj2 = clone $obj;` |
|     - |  575 | ` * print("Original Object:\n");` |
|     - |  576 | ` * print_r($obj);` |
|     - |  577 | ` * print("Cloned Object:\n");` |
|     - |  578 | ` * print_r($obj2);` |
|     - |  579 | ` * ?>` |
|     - |  580 | ` * The above example will output:` |
|     - |  581 | ` * Original Object:` |
|     - |  582 | ` * MyCloneable Object` |
|     - |  583 | ` * (` |
|     - |  584 | ` *   [object1] => SubObject Object` |
|     - |  585 | ` *       (` |
|     - |  586 | ` *           [instance] => 1` |
|     - |  587 | ` *       )` |
|     - |  588 | ` *` |
|     - |  589 | ` *   [object2] => SubObject Object` |
|     - |  590 | ` *       (` |
|     - |  591 | ` *           [instance] => 2` |
|     - |  592 | ` *       )` |
|     - |  593 | ` *` |
|     - |  594 | ` * )` |
|     - |  595 | ` * Cloned Object:` |
|     - |  596 | ` * MyCloneable Object` |
|     - |  597 | ` * (` |
|     - |  598 | ` *   [object1] => SubObject Object` |
|     - |  599 | ` *       (` |
|     - |  600 | ` *           [instance] => 3` |
|     - |  601 | ` *       )` |
|     - |  602 | ` *` |
|     - |  603 | ` *   [object2] => SubObject Object` |
|     - |  604 | ` *       (` |
|     - |  605 | ` *           [instance] => 2` |
|     - |  606 | ` *       )` |
|     - |  607 | ` * )` |
|     - |  608 | ` */` |
|    42 |  609 | `PH7_PRIVATE ph7_class_instance * PH7_CloneClassInstance(ph7_class_instance *pSrc)` |
|     2 |  610 |  |
|     - |  611 | `	ph7_class_instance *pClone;` |
|     - |  612 | `	ph7_class_method *pMethod;` |
|     - |  613 | `	SyHashEntry *pEntry2;` |
|     - |  614 | `	SyHashEntry *pEntry;` |
|     - |  615 | `	ph7_vm *pVm;` |
|     - |  616 | `	sxi32 rc;` |
|     - |  617 | `	/* Allocate a new instance */` |
|    44 |  618 | `	pVm = pSrc->pVm;` |
|    44 |  619 | `	pClone = NewClassInstance(pVm,pSrc->pClass);` |
|    44 |  620 | `	if( pClone == 0 ){` |
|   ! 0 |  621 | `		return 0;` |
|     - |  622 | `	}` |
|     - |  623 | `	/* Associate a private VM frame with this class instance */` |
|    44 |  624 | `	rc = PH7_VmCreateClassInstanceFrame(pVm,pClone);` |
|    44 |  625 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  626 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClone);` |
|   ! 0 |  627 | `		return 0;` |
|     - |  628 | `	}` |
|     - |  629 | `	/* Duplicate object values */` |
|    44 |  630 | `	SyHashResetLoopCursor(&pSrc->hAttr);` |
|    44 |  631 | `	SyHashResetLoopCursor(&pClone->hAttr);` |
|   111 |  632 | `	while((pEntry = SyHashGetNextEntry(&pSrc->hAttr)) != 0 && (pEntry2 = SyHashGetNextEntry(&pClone->hAttr)) != 0 ){` |
|    48 |  633 | `		VmClassAttr *pSrcAttr = (VmClassAttr *)pEntry->pUserData;` |
|    48 |  634 | `		VmClassAttr *pDestAttr = (VmClassAttr *)pEntry2->pUserData;` |
|     - |  635 | `		/* Duplicate non-static attribute */` |
|    48 |  636 | `		if( (pSrcAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|     - |  637 | `			ph7_value *pvSrc,*pvDest;` |
|    48 |  638 | `			pvSrc = ExtractClassAttrValue(pVm,pSrcAttr);` |
|    48 |  639 | `			pvDest = ExtractClassAttrValue(pVm,pDestAttr);` |
|    48 |  640 | `			if( pvSrc && pvDest ){` |
|    48 |  641 | `				PH7_MemObjStore(pvSrc,pvDest);` |
|    23 |  642 | `			}` |
|    23 |  643 | `		}` |
|     2 |  644 | `	}` |
|     - |  645 | `	/* call the __clone method on the cloned object if available */` |
|    44 |  646 | `	pMethod = PH7_ClassExtractMethod(pClone->pClass,"__clone",sizeof("__clone")-1);` |
|    44 |  647 | `	if( pMethod ){` |
|    38 |  648 | `		if( pMethod->iCloneDepth < 16 ){` |
|    36 |  649 | `			pMethod->iCloneDepth++;` |
|    36 |  650 | `			PH7_VmCallClassMethod(pVm,pClone,pMethod,0,0,0);` |
|    19 |  651 | `		}else{` |
|     - |  652 | `			/* Nesting limit reached */` |
|     3 |  653 | `			PH7_VmThrowError(pVm,0,PH7_CTX_ERR,"Object clone limit reached,no more call to __clone()");` |
|     - |  654 | `		}` |
|     - |  655 | `		/* Reset the cursor */` |
|    38 |  656 | `		pMethod->iCloneDepth = 0;` |
|    18 |  657 | `	}` |
|     - |  658 | `	/* Return the cloned object */` |
|    44 |  659 | `	return pClone;` |
|    23 |  660 |  |
|     - |  661 | `#define CLASS_INSTANCE_DESTROYED 0x001 /* Instance is released */` |
|     - |  662 | `/*` |
|     - |  663 | ` * Release a class instance [i.e: Object in the PHP jargon] and invoke any defined destructor.` |
|     - |  664 | ` * This routine is invoked as soon as there are no other references to a particular` |
|     - |  665 | ` * class instance.` |
|     - |  666 | ` */` |
|   410 |  667 | `static void PH7_ClassInstanceRelease(ph7_class_instance *pThis)` |
|     2 |  668 |  |
|     - |  669 | `	ph7_class_method *pDestr;` |
|     - |  670 | `	SyHashEntry *pEntry;` |
|     - |  671 | `	ph7_class *pClass;` |
|     - |  672 | `	ph7_vm *pVm;` |
|   412 |  673 | `	if( pThis->iFlags & CLASS_INSTANCE_DESTROYED ){` |
|     - |  674 | `		/*` |
|     - |  675 | `		 * Already destroyed,return immediately.` |
|     - |  676 | `		 * This could happend if someone perform unset($this) in the destructor body.` |
|     - |  677 | `		 */` |
|   ! 0 |  678 | `		return;` |
|     - |  679 | `	}` |
|     - |  680 | `	/* Mark as destroyed */` |
|   412 |  681 | `	pThis->iFlags \|= CLASS_INSTANCE_DESTROYED;` |
|     - |  682 | `	/* Invoke any defined destructor if available */` |
|   412 |  683 | `	pVm = pThis->pVm;` |
|   412 |  684 | `	pClass = pThis->pClass;` |
|   412 |  685 | `	pDestr = PH7_ClassExtractMethod(pClass,"__destruct",sizeof("__destruct")-1);` |
|   412 |  686 | `	if( pDestr ){` |
|     - |  687 | `		/* Invoke the destructor */` |
|     5 |  688 | `		pThis->iRef = 2; /* Prevent garbage collection */` |
|     5 |  689 | `		PH7_VmCallClassMethod(pVm,pThis,pDestr,0,0,0);` |
|     2 |  690 | `	}` |
|     - |  691 | `	/* Release non-static attributes */` |
|   412 |  692 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|  1522 |  693 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|  1112 |  694 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|  1112 |  695 | `		if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|  1108 |  696 | `			PH7_VmUnsetMemObj(pVm,pVmAttr->nIdx,TRUE);` |
|   553 |  697 | `		}` |
|  1112 |  698 | `		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|     2 |  699 | `	}` |
|     - |  700 | `	/* Release the whole structure */` |
|   412 |  701 | `	SyHashRelease(&pThis->hAttr);` |
|   412 |  702 | `	SyMemBackendPoolFree(&pVm->sAllocator,pThis);` |
|   207 |  703 |  |
|     - |  704 | `/*` |
|     - |  705 | ` * Decrement the reference count of a class instance [i.e Object in the PHP jargon].` |
|     - |  706 | ` * If the reference count reaches zero,release the whole instance.` |
|     - |  707 | ` */` |
|  6878 |  708 | `PH7_PRIVATE void PH7_ClassInstanceUnref(ph7_class_instance *pThis)` |
|     2 |  709 |  |
|  6880 |  710 | `	pThis->iRef--;` |
|  6880 |  711 | `	if( pThis->iRef < 1 ){` |
|     - |  712 | `		/* No more reference to this instance */` |
|   412 |  713 | `		PH7_ClassInstanceRelease(&(*pThis));` |
|   205 |  714 | `	}` |
|  6880 |  715 |  |
|     - |  716 | `/*` |
|     - |  717 | ` * Compare two class instances [i.e: Objects in the PHP jargon]` |
|     - |  718 | ` * Note on objects comparison:` |
|     - |  719 | ` *  According to the PHP langauge reference manual` |
|     - |  720 | ` *  When using the comparison operator (==), object variables are compared in a simple manner` |
|     - |  721 | ` *  namely: Two object instances are equal if they have the same attributes and values, and are` |
|     - |  722 | ` *  instances of the same class.` |
|     - |  723 | ` *  On the other hand, when using the identity operator (===), object variables are identical` |
|     - |  724 | ` *  if and only if they refer to the same instance of the same class.` |
|     - |  725 | ` *  An example will clarify these rules.` |
|     - |  726 | ` *  Example #1 Example of object comparison` |
|     - |  727 | ` *  <?php` |
|     - |  728 | ` *    function bool2str($bool)` |
|     - |  729 | ` * {` |
|     - |  730 | ` *   if ($bool === false) {` |
|     - |  731 | ` *       return 'FALSE';` |
|     - |  732 | ` *   } else {` |
|     - |  733 | ` *       return 'TRUE';` |
|     - |  734 | ` *   }` |
|     - |  735 | ` * }` |
|     - |  736 | ` * function compareObjects(&$o1, &$o2)` |
|     - |  737 | ` * {` |
|     - |  738 | ` *   echo 'o1 == o2 : ' . bool2str($o1 == $o2) . "\n";` |
|     - |  739 | ` *   echo 'o1 != o2 : ' . bool2str($o1 != $o2) . "\n";` |
|     - |  740 | ` *   echo 'o1 === o2 : ' . bool2str($o1 === $o2) . "\n";` |
|     - |  741 | ` *   echo 'o1 !== o2 : ' . bool2str($o1 !== $o2) . "\n";` |
|     - |  742 | ` * }` |
|     - |  743 | ` * class Flag` |
|     - |  744 | ` * {` |
|     - |  745 | ` *   public $flag;` |
|     - |  746 | ` *` |
|     - |  747 | ` *   function Flag($flag = true) {` |
|     - |  748 | ` *       $this->flag = $flag;` |
|     - |  749 | ` *   }` |
|     - |  750 | ` * }` |
|     - |  751 | ` *` |
|     - |  752 | ` * class OtherFlag` |
|     - |  753 | ` * {` |
|     - |  754 | ` *   public $flag;` |
|     - |  755 | ` *` |
|     - |  756 | ` *   function OtherFlag($flag = true) {` |
|     - |  757 | ` *       $this->flag = $flag;` |
|     - |  758 | ` *   }` |
|     - |  759 | ` * }` |
|     - |  760 | ` *` |
|     - |  761 | ` * $o = new Flag();` |
|     - |  762 | ` * $p = new Flag();` |
|     - |  763 | ` * $q = $o;` |
|     - |  764 | ` * $r = new OtherFlag();` |
|     - |  765 | ` *` |
|     - |  766 | ` * echo "Two instances of the same class\n";` |
|     - |  767 | ` * compareObjects($o, $p);` |
|     - |  768 | ` * echo "\nTwo references to the same instance\n";` |
|     - |  769 | ` * compareObjects($o, $q);` |
|     - |  770 | ` * echo "\nInstances of two different classes\n";` |
|     - |  771 | ` * compareObjects($o, $r);` |
|     - |  772 | ` * ?>` |
|     - |  773 | ` * The above example will output:` |
|     - |  774 | ` * Two instances of the same class` |
|     - |  775 | ` * o1 == o2 : TRUE` |
|     - |  776 | ` * o1 != o2 : FALSE` |
|     - |  777 | ` * o1 === o2 : FALSE` |
|     - |  778 | ` * o1 !== o2 : TRUE` |
|     - |  779 | ` * Two references to the same instance` |
|     - |  780 | ` * o1 == o2 : TRUE` |
|     - |  781 | ` * o1 != o2 : FALSE` |
|     - |  782 | ` * o1 === o2 : TRUE` |
|     - |  783 | ` * o1 !== o2 : FALSE` |
|     - |  784 | ` * Instances of two different classes` |
|     - |  785 | ` * o1 == o2 : FALSE` |
|     - |  786 | ` * o1 != o2 : TRUE` |
|     - |  787 | ` * o1 === o2 : FALSE` |
|     - |  788 | ` * o1 !== o2 : TRUE` |
|     - |  789 | ` *` |
|     - |  790 | ` * This function return 0 if the objects are equals according to the comprison rules defined above.` |
|     - |  791 | ` * Any other return values indicates difference.` |
|     - |  792 | ` */` |
|   160 |  793 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest)` |
|     2 |  794 |  |
|     - |  795 | `	SyHashEntry *pEntry,*pEntry2;` |
|     - |  796 | `	ph7_value sV1,sV2;` |
|     - |  797 | `	sxi32 rc;` |
|   162 |  798 | `	if( iNest > 31 ){` |
|     - |  799 | `		/* Nesting limit reached */` |
|     5 |  800 | `		PH7_VmThrowError(pLeft->pVm,0,PH7_CTX_ERR,"Nesting limit reached: Infinite recursion?");` |
|     5 |  801 | `		return 1;` |
|     - |  802 | `	}` |
|     - |  803 | `	/* Comparison is performed only if the objects are instance of the same class */` |
|   158 |  804 | `	if( pLeft->pClass != pRight->pClass ){` |
|     7 |  805 | `		return 1;` |
|     - |  806 | `	}` |
|   152 |  807 | `	if( bStrict ){` |
|     - |  808 | `		/*` |
|     - |  809 | `		 * According to the PHP language reference manual:` |
|     - |  810 | `		 *  when using the identity operator (===), object variables` |
|     - |  811 | `		 *  are identical if and only if they refer to the same instance` |
|     - |  812 | `		 *  of the same class.` |
|     - |  813 | `		 */` |
|    11 |  814 | `		return !(pLeft == pRight);` |
|     - |  815 | `	}` |
|     - |  816 | `	/*` |
|     - |  817 | `	 * Attribute comparison.` |
|     - |  818 | `	 * According to the PHP reference manual:` |
|     - |  819 | `	 *  When using the comparison operator (==), object variables are compared` |
|     - |  820 | `	 *  in a simple manner, namely: Two object instances are equal if they have` |
|     - |  821 | `	 *  the same attributes and values, and are instances of the same class.` |
|     - |  822 | `	 */` |
|   142 |  823 | `	if( pLeft == pRight ){` |
|     - |  824 | `		/* Same instance,don't bother processing,object are equals */` |
|     3 |  825 | `		return 0;` |
|     - |  826 | `	}` |
|   140 |  827 | `	SyHashResetLoopCursor(&pLeft->hAttr);` |
|   140 |  828 | `	SyHashResetLoopCursor(&pRight->hAttr);` |
|   140 |  829 | `	PH7_MemObjInit(pLeft->pVm,&sV1);` |
|   140 |  830 | `	PH7_MemObjInit(pLeft->pVm,&sV2);` |
|   140 |  831 | `	sV1.nIdx = sV2.nIdx = SXU32_HIGH;` |
|   223 |  832 | `	while((pEntry = SyHashGetNextEntry(&pLeft->hAttr)) != 0 && (pEntry2 = SyHashGetNextEntry(&pRight->hAttr)) != 0 ){` |
|   146 |  833 | `		VmClassAttr *p1 = (VmClassAttr *)pEntry->pUserData;` |
|   146 |  834 | `		VmClassAttr *p2 = (VmClassAttr *)pEntry2->pUserData;` |
|     - |  835 | `		/* Compare only non-static attribute */` |
|   146 |  836 | `		if( (p1->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|     - |  837 | `			ph7_value *pL,*pR;` |
|   146 |  838 | `			pL = ExtractClassAttrValue(pLeft->pVm,p1);` |
|   146 |  839 | `			pR = ExtractClassAttrValue(pRight->pVm,p2);` |
|   146 |  840 | `			if( pL && pR ){` |
|   146 |  841 | `				PH7_MemObjLoad(pL,&sV1);` |
|   146 |  842 | `				PH7_MemObjLoad(pR,&sV2);` |
|     - |  843 | `				/* Compare the two values now */` |
|   146 |  844 | `				rc = PH7_MemObjCmp(&sV1,&sV2,bStrict,iNest+1);` |
|   146 |  845 | `				PH7_MemObjRelease(&sV1);` |
|   146 |  846 | `				PH7_MemObjRelease(&sV2);` |
|   146 |  847 | `				if( rc != 0 ){` |
|     - |  848 | `					/* Not equals */` |
|   132 |  849 | `					return rc;` |
|     - |  850 | `				}` |
|     7 |  851 | `			}` |
|     7 |  852 | `		}` |
|     1 |  853 | `	}` |
|     - |  854 | `	/* Object are equals */` |
|     9 |  855 | `	return 0;` |
|    82 |  856 |  |
|     - |  857 | `/*` |
|     - |  858 | ` * Dump a class instance and the store the dump in the BLOB given` |
|     - |  859 | ` * as the first argument.` |
|     - |  860 | ` * Note that only non-static/non-constants attribute are dumped.` |
|     - |  861 | ` * This function is typically invoked when the user issue a call` |
|     - |  862 | ` * to [var_dump(),var_export(),print_r(),...].` |
|     - |  863 | ` * This function SXRET_OK on success. Any other return value including` |
|     - |  864 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|     - |  865 | ` */` |
|   132 |  866 | `PH7_PRIVATE sxi32 PH7_ClassInstanceDump(SyBlob *pOut,ph7_class_instance *pThis,int ShowType,int nTab,int nDepth)` |
|     1 |  867 |  |
|     - |  868 | `	SyHashEntry *pEntry;` |
|     - |  869 | `	ph7_value *pValue;` |
|     - |  870 | `	sxi32 rc;` |
|     - |  871 | `	int i;` |
|   133 |  872 | `	if( nDepth > 31 ){` |
|     - |  873 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|     - |  874 | `		/* Nesting limit reached..halt immediately*/` |
|     5 |  875 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|     5 |  876 | `		if( ShowType ){` |
|     5 |  877 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|     2 |  878 | `		}` |
|     5 |  879 | `		return SXERR_LIMIT;` |
|     - |  880 | `	}` |
|   129 |  881 | `	rc = SXRET_OK;` |
|   129 |  882 | `	if( !ShowType ){` |
|     3 |  883 | `		SyBlobAppend(&(*pOut),"Object(",sizeof("Object(")-1);` |
|     1 |  884 | `	}` |
|     - |  885 | `	/* Append class name */` |
|   129 |  886 | `	SyBlobFormat(&(*pOut),"%z) {",&pThis->pClass->sName);` |
|     - |  887 | `#ifdef __WINNT__` |
|     1 |  888 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|     - |  889 | `#else` |
|   128 |  890 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|     - |  891 | `#endif` |
|     - |  892 | `	/* Dump object attributes */` |
|   129 |  893 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|   201 |  894 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|   133 |  895 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|   133 |  896 | `		if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|     - |  897 | `			/* Dump non-static/constant attribute only */` |
|  3985 |  898 | `			for( i = 0 ; i < nTab ; i++ ){` |
|  3853 |  899 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|  1927 |  900 | `			}` |
|   133 |  901 | `			pValue = ExtractClassAttrValue(pThis->pVm,pVmAttr);` |
|   133 |  902 | `			if( pValue ){` |
|   133 |  903 | `				SyBlobFormat(&(*pOut),"['%z'] =>",&pVmAttr->pAttr->sName);` |
|     - |  904 | `#ifdef __WINNT__` |
|     1 |  905 | `				SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|     - |  906 | `#else` |
|   132 |  907 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|     - |  908 | `#endif` |
|   133 |  909 | `				rc = PH7_MemObjDump(&(*pOut),pValue,ShowType,nTab+1,nDepth,0);` |
|   133 |  910 | `				if( rc == SXERR_LIMIT ){` |
|   125 |  911 | `					break;` |
|     - |  912 | `				}` |
|     4 |  913 | `			}` |
|     4 |  914 | `		}` |
|     1 |  915 | `	}` |
|  3977 |  916 | `	for( i = 0 ; i < nTab ; i++ ){` |
|  3849 |  917 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|  1925 |  918 | `	}` |
|   129 |  919 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|   129 |  920 | `	return rc;` |
|    67 |  921 |  |
|     - |  922 | `/*` |
|     - |  923 | ` * Call a magic method [i.e: __toString(),__toBool(),__Invoke()...]` |
|     - |  924 | ` * Return SXRET_OK on successfull call. Any other return value indicates failure.` |
|     - |  925 | ` * Notes on magic methods.` |
|     - |  926 | ` * According to the PHP language reference manual.` |
|     - |  927 | ` *  The function names __construct(), __destruct(), __call(), __callStatic()` |
|     - |  928 | ` *  __get(),  __toString(), __invoke(), __clone() are magical in PHP classes.` |
|     - |  929 | ` * You cannot have functions with these names in any of your classes unless` |
|     - |  930 | ` * you want the magic functionality associated with them.` |
|     - |  931 | ` * Example of magical methods:` |
|     - |  932 | ` * __toString()` |
|     - |  933 | ` *  The __toString() method allows a class to decide how it will react when it is treated like` |
|     - |  934 | ` *  a string. For example, what echo $obj; will print. This method must return a string.` |
|     - |  935 | ` *  Example #2 Simple example` |
|     - |  936 | ` * <?php` |
|     - |  937 | ` * // Declare a simple class` |
|     - |  938 | ` * class TestClass` |
|     - |  939 | ` * {` |
|     - |  940 | ` *   public $foo;` |
|     - |  941 | ` *` |
|     - |  942 | ` *   public function __construct($foo)` |
|     - |  943 | ` *   {` |
|     - |  944 | ` *       $this->foo = $foo;` |
|     - |  945 | ` *   }` |
|     - |  946 | ` *` |
|     - |  947 | ` *   public function __toString()` |
|     - |  948 | ` *   {` |
|     - |  949 | ` *       return $this->foo;` |
|     - |  950 | ` *   }` |
|     - |  951 | ` * }` |
|     - |  952 | ` * $class = new TestClass('Hello');` |
|     - |  953 | ` * echo $class;` |
|     - |  954 | ` * ?>` |
|     - |  955 | ` * The above example will output:` |
|     - |  956 | ` *  Hello` |
|     - |  957 | ` *` |
|     - |  958 | ` * Note that PH7 does not support all the magical method and introudces __toFloat(),__toInt()` |
|     - |  959 | ` * which have the same behaviour as __toString() but for float and integer types` |
|     - |  960 | ` * respectively.` |
|     - |  961 | ` * Refer to the official documentation for more information.` |
|     - |  962 | ` */` |
|     4 |  963 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallMagicMethod(` |
|     - |  964 | `	ph7_vm *pVm,               /* VM that own all this stuff */` |
|     - |  965 | `	ph7_class *pClass,         /* Target class */` |
|     - |  966 | `	ph7_class_instance *pThis, /* Target object */` |
|     - |  967 | `	const char *zMethod,       /* Magic method name [i.e: __toString()]*/` |
|     - |  968 | `	sxu32 nByte,               /* zMethod length*/` |
|     - |  969 | `	const SyString *pAttrName  /* Attribute name */` |
|     - |  970 | `	)` |
|     2 |  971 |  |
|     6 |  972 | `	ph7_value *apArg[2] = { 0 , 0 };` |
|     - |  973 | `	ph7_class_method *pMeth;` |
|     - |  974 | `	ph7_value sAttr; /* cc warning */` |
|     - |  975 | `	sxi32 rc;` |
|     - |  976 | `	int nArg;` |
|     - |  977 | `	/* Make sure the magic method is available */` |
|     6 |  978 | `	pMeth = PH7_ClassExtractMethod(&(*pClass),zMethod,nByte);` |
|     6 |  979 | `	if( pMeth == 0 ){` |
|     - |  980 | `		/* No such method,return immediately */` |
|     3 |  981 | `		return SXERR_NOTFOUND;` |
|     - |  982 | `	}` |
|     3 |  983 | `	nArg = 0;` |
|     - |  984 | `	/* Copy arguments */` |
|     3 |  985 | `	if( pAttrName ){` |
|   ! 0 |  986 | `		PH7_MemObjInitFromString(pVm,&sAttr,pAttrName);` |
|   ! 0 |  987 | `		sAttr.nIdx = SXU32_HIGH; /* Mark as constant */` |
|   ! 0 |  988 | `		apArg[0] = &sAttr;` |
|   ! 0 |  989 | `		nArg = 1;` |
|   ! 0 |  990 | `	}` |
|     - |  991 | `	/* Call the magic method now */` |
|     3 |  992 | `	rc = PH7_VmCallClassMethod(pVm,&(*pThis),pMeth,0,nArg,apArg);` |
|     - |  993 | `	/* Clean up */` |
|     3 |  994 | `	if( pAttrName ){` |
|   ! 0 |  995 | `		PH7_MemObjRelease(&sAttr);` |
|   ! 0 |  996 | `	}` |
|     3 |  997 | `	return rc;` |
|     4 |  998 |  |
|     - |  999 | `/*` |
|     - | 1000 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon].` |
|     - | 1001 | ` * This function is simply a wrapper on ExtractClassAttrValue().` |
|     - | 1002 | ` */` |
|    18 | 1003 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceExtractAttrValue(ph7_class_instance *pThis,VmClassAttr *pAttr)` |
|     1 | 1004 |  |
|     - | 1005 | `   /* Extract the attribute value */` |
|     - | 1006 | `	ph7_value *pValue;` |
|    19 | 1007 | `	pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|    19 | 1008 | `	return pValue;` |
|     1 | 1009 |  |
|     - | 1010 | `/*` |
|     - | 1011 | ` * Convert a class instance [i.e: Object in the PHP jargon] into a hashmap [i.e: array in the PHP jargon].` |
|     - | 1012 | ` * Return SXRET_OK on success. Any other value indicates failure.` |
|     - | 1013 | ` * Note on object conversion to array:` |
|     - | 1014 | ` *  Acccording to the PHP language reference manual` |
|     - | 1015 | ` *  If an object is converted to an array, the result is an array whose elements are the object's properties.` |
|     - | 1016 | ` *  The keys are the member variable names.` |
|     - | 1017 | ` *` |
|     - | 1018 | ` *  The following example:` |
|     - | 1019 | ` *  class Test {` |
|     - | 1020 | ` *   public $A = 25<<1;  // 50` |
|     - | 1021 | ` *	 public $c = rand_str(3);   // Random string of length 3` |
|     - | 1022 | ` *	 public $d = rand() & 1023; // Random number between 0..1023` |
|     - | 1023 | ` *  }` |
|     - | 1024 | ` *  var_dump((array) new Test());` |
|     - | 1025 | ` *	Will output:` |
|     - | 1026 | ` *  array(3) {` |
|     - | 1027 | ` *   [A] =>` |
|     - | 1028 | ` *      int(50)` |
|     - | 1029 | ` *   [c] =>` |
|     - | 1030 | ` *     string(3 'aps')` |
|     - | 1031 | ` *   [d] =>` |
|     - | 1032 | ` *     int(991)` |
|     - | 1033 | ` *  }` |
|     - | 1034 | ` * You have noticed that PH7 allow class attributes [i.e: $a,$c,$d in the example above]` |
|     - | 1035 | ` * have any complex expression (even function calls/Annonymous functions) as their default` |
|     - | 1036 | ` * value unlike the standard PHP engine.` |
|     - | 1037 | ` * This is a very powerful feature that you have to look at.` |
|     - | 1038 | ` */` |
|     6 | 1039 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|     1 | 1040 |  |
|     - | 1041 | `	SyHashEntry *pEntry;` |
|     - | 1042 | `	SyString *pAttrName;` |
|     - | 1043 | `	VmClassAttr *pAttr;` |
|     - | 1044 | `	ph7_value *pValue;` |
|     - | 1045 | `	ph7_value sName;` |
|     - | 1046 | `	/* Reset the loop cursor */` |
|     7 | 1047 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|     7 | 1048 | `	PH7_MemObjInitFromString(pThis->pVm,&sName,0);` |
|    20 | 1049 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|     - | 1050 | `		/* Point to the current attribute */` |
|    11 | 1051 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|     - | 1052 | `		/* Extract attribute value */` |
|    11 | 1053 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|    11 | 1054 | `		if( pValue ){` |
|     - | 1055 | `			/* Build attribute name */` |
|    11 | 1056 | `			pAttrName = &pAttr->pAttr->sName;` |
|    11 | 1057 | `			PH7_MemObjStringAppend(&sName,pAttrName->zString,pAttrName->nByte);` |
|     - | 1058 | `			/* Perform the insertion */` |
|    11 | 1059 | `			PH7_HashmapInsert(pMap,&sName,pValue);` |
|     - | 1060 | `			/* Reset the string cursor */` |
|    11 | 1061 | `			SyBlobReset(&sName.sBlob);` |
|     5 | 1062 | `		}` |
|     1 | 1063 | `	}` |
|     7 | 1064 | `	PH7_MemObjRelease(&sName);` |
|     7 | 1065 | `	return SXRET_OK;` |
|     1 | 1066 |  |
|     - | 1067 | `/*` |
|     - | 1068 | ` * Iterate throw class attributes and invoke the given callback [i.e: xWalk()] for each` |
|     - | 1069 | ` * retrieved attribute.` |
|     - | 1070 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|     - | 1071 | ` * the attribute value in the callback body will not alter the real attribute value.` |
|     - | 1072 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|     - | 1073 | ` * a value different from PH7_OK.` |
|     - | 1074 | ` * Refer to [ph7_object_walk()] for more information.` |
|     - | 1075 | ` */` |
|   ! 0 | 1076 | `PH7_PRIVATE sxi32 PH7_ClassInstanceWalk(` |
|     - | 1077 | `	ph7_class_instance *pThis, /* Target object */` |
|     - | 1078 | `	int (*xWalk)(const char *,ph7_value *,void *), /* Walker callback */` |
|     - | 1079 | `	void *pUserData /* Last argument to xWalk() */` |
|     - | 1080 | `	)` |
|   ! 0 | 1081 |  |
|     - | 1082 | `	SyHashEntry *pEntry; /* Hash entry */` |
|     - | 1083 | `	VmClassAttr *pAttr;  /* Pointer to the attribute */` |
|     - | 1084 | `	ph7_value *pValue;   /* Attribute value */` |
|     - | 1085 | `	ph7_value sValue;    /* Copy of the attribute value */` |
|     - | 1086 | `	int rc;` |
|     - | 1087 | `	/* Reset the loop cursor */` |
|   ! 0 | 1088 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|   ! 0 | 1089 | `	PH7_MemObjInit(pThis->pVm,&sValue);` |
|     - | 1090 | `	/* Start the walk process */` |
|   ! 0 | 1091 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|     - | 1092 | `		/* Point to the current attribute */` |
|   ! 0 | 1093 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|     - | 1094 | `		/* Extract attribute value */` |
|   ! 0 | 1095 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|   ! 0 | 1096 | `		if( pValue ){` |
|   ! 0 | 1097 | `			PH7_MemObjLoad(pValue,&sValue);` |
|     - | 1098 | `			/* Invoke the supplied callback */` |
|   ! 0 | 1099 | `			rc =  xWalk(SyStringData(&pAttr->pAttr->sName),&sValue,pUserData);` |
|   ! 0 | 1100 | `			PH7_MemObjRelease(&sValue);` |
|   ! 0 | 1101 | `			if( rc != PH7_OK){` |
|     - | 1102 | `				/* User callback request an operation abort */` |
|   ! 0 | 1103 | `				return SXERR_ABORT;` |
|     - | 1104 | `			}` |
|   ! 0 | 1105 | `		}` |
|   ! 0 | 1106 | `	}` |
|     - | 1107 | `	/* All done */` |
|   ! 0 | 1108 | `	return SXRET_OK;` |
|   ! 0 | 1109 |  |
|     - | 1110 | `/*` |
|     - | 1111 | ` * Extract a class atrribute value.` |
|     - | 1112 | ` * Return a pointer to the attribute value on success. Otherwise NULL.` |
|     - | 1113 | ` * Note:` |
|     - | 1114 | ` *  Access to static and constant attribute is not allowed. That is,the function` |
|     - | 1115 | ` *  will return NULL in case someone (host-application code) try to extract` |
|     - | 1116 | ` *  a static/constant attribute.` |
|     - | 1117 | ` */` |
|   ! 0 | 1118 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceFetchAttr(ph7_class_instance *pThis,const SyString *pName)` |
|   ! 0 | 1119 |  |
|     - | 1120 | `	SyHashEntry *pEntry;` |
|     - | 1121 | `	VmClassAttr *pAttr;` |
|     - | 1122 | `	/* Query the attribute hashtable */` |
|   ! 0 | 1123 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);` |
|   ! 0 | 1124 | `	if( pEntry == 0 ){` |
|     - | 1125 | `		/* No such attribute */` |
|   ! 0 | 1126 | `		return 0;` |
|     - | 1127 | `	}` |
|     - | 1128 | `	/* Point to the class atrribute */` |
|   ! 0 | 1129 | `	pAttr = (VmClassAttr *)pEntry->pUserData;` |
|     - | 1130 | `	/* Check if we are dealing with a static/constant attribute */` |
|   ! 0 | 1131 | `	if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|     - | 1132 | `		/* Access is forbidden */` |
|   ! 0 | 1133 | `		return 0;` |
|     - | 1134 | `	}` |
|     - | 1135 | `	/* Return the attribute value */` |
|   ! 0 | 1136 | `	return ExtractClassAttrValue(pThis->pVm,pAttr);` |
|   ! 0 | 1137 |  |
|     - | 1138 |  |
