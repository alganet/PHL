# src/ph7/oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 396/460 lines (86.09%)

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
|  29166 |   14 | `PH7_PRIVATE ph7_class * PH7_NewRawClass(ph7_vm *pVm,const SyString *pName,sxu32 nLine)` |
|      2 |   15 |  |
|      - |   16 | `	ph7_class *pClass;` |
|      - |   17 | `	char *zName;` |
|      - |   18 | `	/* Allocate a new instance */` |
|  29168 |   19 | `	pClass = (ph7_class *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class));` |
|  29168 |   20 | `	if( pClass == 0 ){` |
|    ! 0 |   21 | `		return 0;` |
|      - |   22 | `	}` |
|      - |   23 | `	/* Zero the structure */` |
|  29168 |   24 | `	SyZero(pClass,sizeof(ph7_class));` |
|      - |   25 | `	/* Duplicate class name */` |
|  29168 |   26 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  29168 |   27 | `	if( zName == 0 ){` |
|    ! 0 |   28 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClass);` |
|    ! 0 |   29 | `		return 0;` |
|      - |   30 | `	}` |
|      - |   31 | `	/* Initialize fields */` |
|  29168 |   32 | `	SyStringInitFromBuf(&pClass->sName,zName,pName->nByte);` |
|  29168 |   33 | `	SyHashInit(&pClass->hMethod,&pVm->sAllocator,0,0);` |
|  29168 |   34 | `	SyHashInit(&pClass->hAttr,&pVm->sAllocator,0,0);` |
|  29168 |   35 | `	SyHashInit(&pClass->hDerived,&pVm->sAllocator,0,0);` |
|  29168 |   36 | `	SySetInit(&pClass->aInterface,&pVm->sAllocator,sizeof(ph7_class *));` |
|  29168 |   37 | `	SySetInit(&pClass->aTrait,&pVm->sAllocator,sizeof(ph7_class *));` |
|  29168 |   38 | `	pClass->nLine = nLine;` |
|      - |   39 | `	/* All done */` |
|  29168 |   40 | `	return pClass;` |
|  14585 |   41 |  |
|      - |   42 | `/*` |
|      - |   43 | ` * Allocate and initialize a new class attribute.` |
|      - |   44 | ` * Return a pointer to the class attribute on success. NULL otherwise.` |
|      - |   45 | ` */` |
|  24196 |   46 | `PH7_PRIVATE ph7_class_attr * PH7_NewClassAttr(ph7_vm *pVm,const SyString *pName,sxu32 nLine,sxi32 iProtection,sxi32 iFlags)` |
|      2 |   47 |  |
|      - |   48 | `	ph7_class_attr *pAttr;` |
|      - |   49 | `	char *zName;` |
|  24198 |   50 | `	pAttr = (ph7_class_attr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_attr));` |
|  24198 |   51 | `	if( pAttr == 0 ){` |
|    ! 0 |   52 | `		return 0;` |
|      - |   53 | `	}` |
|      - |   54 | `	/* Zero the structure */` |
|  24198 |   55 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|      - |   56 | `	/* Duplicate attribute name */` |
|  24198 |   57 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  24198 |   58 | `	if( zName == 0 ){` |
|    ! 0 |   59 | `		SyMemBackendPoolFree(&pVm->sAllocator,pAttr);` |
|    ! 0 |   60 | `		return 0;` |
|      - |   61 | `	}` |
|      - |   62 | `	/* Initialize fields */` |
|  24198 |   63 | `	SySetInit(&pAttr->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|  24198 |   64 | `	SyStringInitFromBuf(&pAttr->sName,zName,pName->nByte);` |
|  24198 |   65 | `	pAttr->iProtection = iProtection;` |
|  24198 |   66 | `	pAttr->nIdx = SXU32_HIGH;` |
|  24198 |   67 | `	pAttr->iFlags = iFlags;` |
|  24198 |   68 | `	pAttr->nLine = nLine;` |
|  24198 |   69 | `	return pAttr;` |
|  12100 |   70 |  |
|      - |   71 | `/*` |
|      - |   72 | ` * Allocate and initialize a new class method.` |
|      - |   73 | ` * Return a pointer to the class method on success. NULL otherwise` |
|      - |   74 | ` * This function associate with the newly created method an automatically generated` |
|      - |   75 | ` * random unique name.` |
|      - |   76 | ` */` |
|  69920 |   77 | `PH7_PRIVATE ph7_class_method * PH7_NewClassMethod(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,sxu32 nLine,` |
|      - |   78 | `	sxi32 iProtection,sxi32 iFlags,sxi32 iFuncFlags)` |
|      2 |   79 |  |
|      - |   80 | `	ph7_class_method *pMeth;` |
|      - |   81 | `	SyHashEntry *pEntry;` |
|      - |   82 | `	SyString *pNamePtr;` |
|      - |   83 | `	char zSalt[10];` |
|      - |   84 | `	char *zName;` |
|      - |   85 | `	sxu32 nByte;` |
|      - |   86 | `	/* Allocate a new class method instance */` |
|  69922 |   87 | `	pMeth = (ph7_class_method *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_method));` |
|  69922 |   88 | `	if( pMeth == 0 ){` |
|    ! 0 |   89 | `		return 0;` |
|      - |   90 | `	}` |
|      - |   91 | `	/* Zero the structure */` |
|  69922 |   92 | `	SyZero(pMeth,sizeof(ph7_class_method));` |
|      - |   93 | `	/* Check for an already installed method with the same name */` |
|  69922 |   94 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
|  69922 |   95 | `	if( pEntry == 0 ){` |
|      - |   96 | `		/* Associate an unique VM name to this method */` |
|  69920 |   97 | `		nByte = sizeof(zSalt) + pName->nByte + SyStringLength(&pClass->sName)+sizeof(char)*7/*[[__'\0'*/;` |
|  69920 |   98 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,nByte);` |
|  69920 |   99 | `		if( zName == 0 ){` |
|    ! 0 |  100 | `			SyMemBackendPoolFree(&pVm->sAllocator,pMeth);` |
|    ! 0 |  101 | `			return 0;` |
|      - |  102 | `		}` |
|  69920 |  103 | `		pNamePtr = &pMeth->sVmName;` |
|      - |  104 | `		/* Generate a random string */` |
|  69920 |  105 | `		PH7_VmRandomString(&(*pVm),zSalt,sizeof(zSalt));` |
|  69920 |  106 | `		pNamePtr->nByte = SyBufferFormat(zName,nByte,"[__%z@%z_%.*s]",&pClass->sName,pName,sizeof(zSalt),zSalt);` |
|  69920 |  107 | `		pNamePtr->zString = zName;` |
|  34961 |  108 | `	}else{` |
|      - |  109 | `		/* Method is condidate for 'overloading' */` |
|      3 |  110 | `		ph7_class_method *pCurrent = (ph7_class_method *)pEntry->pUserData;` |
|      3 |  111 | `		pNamePtr = &pMeth->sVmName;` |
|      - |  112 | `		/* Use the same VM name */` |
|      3 |  113 | `		SyStringDupPtr(pNamePtr,&pCurrent->sVmName);` |
|      3 |  114 | `		zName = (char *)pNamePtr->zString;` |
|      - |  115 | `	}` |
|  69922 |  116 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|     23 |  117 | `		if( (pName->nByte == sizeof("__construct") - 1 && SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1 ) == 0)` |
|     15 |  118 | `			\|\| (pName->nByte == sizeof("__destruct") - 1 && SyMemcmp(pName->zString,"__destruct",sizeof("__destruct") - 1 ) == 0)` |
|     15 |  119 | `			\|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|      - |  120 | `				/* Switch to public visibility when dealing with constructor/destructor */` |
|      5 |  121 | `				iProtection = PH7_CLASS_PROT_PUBLIC;` |
|      2 |  122 | `		}` |
|      9 |  123 | `	}` |
|      - |  124 | `	/* Initialize method fields */` |
|  69924 |  125 | `	pMeth->iProtection = iProtection;` |
|  69924 |  126 | `	pMeth->iFlags = iFlags;` |
|  69924 |  127 | `	pMeth->nLine = nLine;` |
| 104886 |  128 | `	PH7_VmInitFuncState(&(*pVm),&pMeth->sFunc,&zName[sizeof(char)*4/*[__@*/+SyStringLength(&pClass->sName)],` |
|  69922 |  129 | `		pName->nByte,iFuncFlags\|VM_FUNC_CLASS_METHOD,pClass);` |
|  69924 |  130 | `	return pMeth;` |
|  34964 |  131 |  |
|      - |  132 | `/*` |
|      - |  133 | ` * Check if the given name have a class method associated with it.` |
|      - |  134 | ` * Return the desired method [i.e: ph7_class_method instance] on success. NULL otherwise.` |
|      - |  135 | ` */` |
|   4662 |  136 | `PH7_PRIVATE ph7_class_method * PH7_ClassExtractMethod(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|      2 |  137 |  |
|      - |  138 | `	SyHashEntry *pEntry;` |
|      - |  139 | `	/* Perform a hash lookup */` |
|   4664 |  140 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)zName,nByte);` |
|   4664 |  141 | `	if( pEntry == 0 ){` |
|      - |  142 | `		/* No such entry */` |
|   1808 |  143 | `		return 0;` |
|      - |  144 | `	}` |
|      - |  145 | `	/* Point to the desired method */` |
|   2858 |  146 | `	return (ph7_class_method *)pEntry->pUserData;` |
|   2333 |  147 |  |
|      - |  148 | `/*` |
|      - |  149 | ` * Check if the given name is a class attribute.` |
|      - |  150 | ` * Return the desired attribute [i.e: ph7_class_attr instance] on success.NULL otherwise.` |
|      - |  151 | ` */` |
|     18 |  152 | `PH7_PRIVATE ph7_class_attr * PH7_ClassExtractAttribute(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|      2 |  153 |  |
|      - |  154 | `	SyHashEntry *pEntry;` |
|      - |  155 | `	/* Perform a hash lookup */` |
|     20 |  156 | `	pEntry = SyHashGet(&pClass->hAttr,(const void *)zName,nByte);` |
|     20 |  157 | `	if( pEntry == 0 ){` |
|      - |  158 | `		/* No such entry */` |
|    ! 0 |  159 | `		return 0;` |
|      - |  160 | `	}` |
|      - |  161 | `	/* Point to the desierd method */` |
|     20 |  162 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|     11 |  163 |  |
|      - |  164 | `/*` |
|      - |  165 | ` * Install a class attribute in the corresponding container.` |
|      - |  166 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|      - |  167 | ` */` |
|  24196 |  168 | `PH7_PRIVATE sxi32 PH7_ClassInstallAttr(ph7_class *pClass,ph7_class_attr *pAttr)` |
|      2 |  169 |  |
|  24198 |  170 | `	SyString *pName = &pAttr->sName;` |
|      - |  171 | `	sxi32 rc;` |
|  24198 |  172 | `	rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|  24198 |  173 | `	return rc;` |
|      2 |  174 |  |
|      - |  175 | `/*` |
|      - |  176 | ` * Install a class method in the corresponding container.` |
|      - |  177 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|      - |  178 | ` */` |
|  69918 |  179 | `PH7_PRIVATE sxi32 PH7_ClassInstallMethod(ph7_class *pClass,ph7_class_method *pMeth)` |
|      2 |  180 |  |
|  69920 |  181 | `	SyString *pName = &pMeth->sFunc.sName;` |
|      - |  182 | `	sxi32 rc;` |
|  69920 |  183 | `	rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|  69920 |  184 | `	return rc;` |
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
|  14450 |  227 | `PH7_PRIVATE sxi32 PH7_ClassInherit(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase)` |
|      2 |  228 |  |
|      - |  229 | `	ph7_class_method *pMeth;` |
|      - |  230 | `	ph7_class_attr *pAttr;` |
|      - |  231 | `	SyHashEntry *pEntry;` |
|      - |  232 | `	SyString *pName;` |
|      - |  233 | `	sxi32 rc;` |
|      - |  234 | `	/* Install in the derived hashtable */` |
|  14452 |  235 | `	rc = SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|  14452 |  236 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  237 | `		return rc;` |
|      - |  238 | `	}` |
|      - |  239 | `	/* Copy public/protected attributes from the base class */` |
|  14452 |  240 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
| 100876 |  241 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|      - |  242 | `		/* Make sure the private attributes are not redeclared in the subclass */` |
|  86426 |  243 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  86426 |  244 | `		pName = &pAttr->sName;` |
|  86426 |  245 | `		if( (pEntry = SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte)) != 0 ){` |
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
|  86424 |  257 | `		if( pAttr->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
|  86422 |  258 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|  86422 |  259 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  260 | `				return rc;` |
|      - |  261 | `			}` |
|  43210 |  262 | `		}` |
|      2 |  263 | `	}` |
|  14452 |  264 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
| 144144 |  265 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|      - |  266 | `		/* Make sure the private/final methods are not redeclared in the subclass */` |
| 129694 |  267 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
| 129694 |  268 | `		pName = &pMeth->sFunc.sName;` |
| 129694 |  269 | `		if( (pEntry = SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|   2428 |  270 | `			 if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|      - |  271 | `				/* Cannot Overwrite final method */` |
|      7 |  272 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_method *)pEntry->pUserData)->nLine,` |
|      - |  273 | `					"Cannot Overwrite final method '%z:%z' inside child class '%z'",` |
|      2 |  274 | `					&pBase->sName,pName,&pSub->sName);` |
|      5 |  275 | `				if( rc == SXERR_ABORT ){` |
|    ! 0 |  276 | `					return SXERR_ABORT;` |
|      - |  277 | `				}` |
|      2 |  278 | `			}` |
|   2428 |  279 | `			continue;` |
|    ! 0 |  280 | `		}else{` |
| 127268 |  281 | `			if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      - |  282 | `				/* Abstract method must be defined in the child class */` |
|      4 |  283 | `				PH7_GenCompileError(&(*pGen),E_WARNING,pMeth->nLine,` |
|      - |  284 | `					"Abstract method '%z:%z' must be defined inside child class '%z'",` |
|      1 |  285 | `					&pBase->sName,pName,&pSub->sName);` |
|      3 |  286 | `				continue;` |
|      - |  287 | `			}` |
|      - |  288 | `		}` |
|      - |  289 | `		/* Install the method */` |
| 127266 |  290 | `		if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
| 127264 |  291 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 127264 |  292 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  293 | `				return rc;` |
|      - |  294 | `			}` |
|  63631 |  295 | `		}` |
|      2 |  296 | `	}` |
|      - |  297 | `	/* Mark as subclass */` |
|  14452 |  298 | `	pSub->pBase = pBase;` |
|      - |  299 | `	/* All done */` |
|  14452 |  300 | `	return SXRET_OK;` |
|   7227 |  301 |  |
|      - |  302 | `/*` |
|      - |  303 | ` * Apply a trait to a class: copy all methods and attributes from the trait` |
|      - |  304 | ` * into the target class. Unlike inheritance, traits copy ALL members including` |
|      - |  305 | ` * private ones. Members already defined in the class take precedence.` |
|      - |  306 | ` */` |
|     20 |  307 | `PH7_PRIVATE sxi32 PH7_ClassUseTrait(ph7_gen_state *pGen,ph7_class *pClass,ph7_class *pTrait)` |
|      1 |  308 |  |
|      - |  309 | `	ph7_class_method *pMeth;` |
|      - |  310 | `	ph7_class_attr *pAttr;` |
|      - |  311 | `	SyHashEntry *pEntry;` |
|      - |  312 | `	SyString *pName;` |
|      - |  313 | `	sxi32 rc;` |
|      - |  314 | `	/* Copy attributes from the trait */` |
|     21 |  315 | `	SyHashResetLoopCursor(&pTrait->hAttr);` |
|     29 |  316 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hAttr)) != 0 ){` |
|      9 |  317 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      9 |  318 | `		pName = &pAttr->sName;` |
|      9 |  319 | `		if( SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte) != 0 ){` |
|      - |  320 | `			/* Class attribute takes precedence over trait attribute */` |
|      3 |  321 | `			continue;` |
|      - |  322 | `		}` |
|      7 |  323 | `		rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|      7 |  324 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  325 | `			return rc;` |
|      - |  326 | `		}` |
|      1 |  327 | `	}` |
|      - |  328 | `	/* Copy methods from the trait */` |
|     21 |  329 | `	SyHashResetLoopCursor(&pTrait->hMethod);` |
|     39 |  330 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hMethod)) != 0 ){` |
|     19 |  331 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|     19 |  332 | `		pName = &pMeth->sFunc.sName;` |
|     19 |  333 | `		if( SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte) != 0 ){` |
|      - |  334 | `			/* Method already exists in the class. Check if it came from another trait` |
|      - |  335 | `			 * (unresolved conflict) vs being defined by the class itself.` |
|      - |  336 | `			 */` |
|      - |  337 | `			ph7_class **apUsedTraits;` |
|      - |  338 | `			sxu32 nUsed,k;` |
|      5 |  339 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|      5 |  340 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|      5 |  341 | `			for(k = 0; k < nUsed; k++){` |
|      3 |  342 | `				if( PH7_ClassExtractMethod(apUsedTraits[k],pName->zString,pName->nByte) != 0 ){` |
|      - |  343 | `					/* Two different traits define the same method with no resolution */` |
|      4 |  344 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|      - |  345 | `						"Trait method %z::%z has not been applied as %z::%z, "` |
|      - |  346 | `						"because of collision with %z::%z",` |
|      2 |  347 | `						&pTrait->sName,pName,` |
|      1 |  348 | `						&pClass->sName,pName,` |
|      2 |  349 | `						&apUsedTraits[k]->sName,pName);` |
|      3 |  350 | `					if( rc == SXERR_ABORT ){` |
|    ! 0 |  351 | `						return SXERR_ABORT;` |
|      - |  352 | `					}` |
|      3 |  353 | `					break;` |
|      - |  354 | `				}` |
|    ! 0 |  355 | `			}` |
|      - |  356 | `			/* Class-defined method takes precedence */` |
|      5 |  357 | `			continue;` |
|      - |  358 | `		}` |
|     15 |  359 | `		rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|     15 |  360 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  361 | `			return rc;` |
|      - |  362 | `		}` |
|      1 |  363 | `	}` |
|      - |  364 | `	/* Record trait in the class */` |
|     21 |  365 | `	SySetPut(&pClass->aTrait,(const void *)&pTrait);` |
|     10 |  366 | `	SXUNUSED(pGen);` |
|     21 |  367 | `	return SXRET_OK;` |
|     11 |  368 |  |
|      - |  369 | `/*` |
|      - |  370 | ` * Inherit an object interface from another object interface.` |
|      - |  371 | ` * According to the PHP language reference manual.` |
|      - |  372 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|      - |  373 | ` *  must implement, without having to define how these methods are handled.` |
|      - |  374 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|      - |  375 | ` *  class, but without any of the methods having their contents defined.` |
|      - |  376 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|      - |  377 | ` *` |
|      - |  378 | ` * This function return SXRET_OK if the interface inheritance operation was successfully performed.` |
|      - |  379 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|      - |  380 | ` * error message.` |
|      - |  381 | ` */` |
|      2 |  382 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceInherit(ph7_class *pSub,ph7_class *pBase)` |
|      1 |  383 |  |
|      - |  384 | `	ph7_class_method *pMeth;` |
|      - |  385 | `	ph7_class_attr *pAttr;` |
|      - |  386 | `	SyHashEntry *pEntry;` |
|      - |  387 | `	SyString *pName;` |
|      - |  388 | `	sxi32 rc;` |
|      - |  389 | `	/* Install in the derived hashtable */` |
|      3 |  390 | `	SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|      3 |  391 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
|      - |  392 | `	/* Copy constants */` |
|      6 |  393 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|      - |  394 | `		/* Make sure the constants are not redeclared in the subclass */` |
|      3 |  395 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      3 |  396 | `		pName = &pAttr->sName;` |
|      3 |  397 | `		if( SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte) == 0 ){` |
|      - |  398 | `			/* Install the constant in the subclass */` |
|      3 |  399 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|      3 |  400 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  401 | `				return rc;` |
|      - |  402 | `			}` |
|      1 |  403 | `		}` |
|      1 |  404 | `	}` |
|      3 |  405 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
|      - |  406 | `	/* Copy methods signature */` |
|      6 |  407 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|      - |  408 | `		/* Make sure the method are not redeclared in the subclass */` |
|      3 |  409 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      3 |  410 | `		pName = &pMeth->sFunc.sName;` |
|      3 |  411 | `		if( SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte) == 0 ){` |
|      - |  412 | `			/* Install the method */` |
|      3 |  413 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|      3 |  414 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  415 | `				return rc;` |
|      - |  416 | `			}` |
|      1 |  417 | `		}` |
|      1 |  418 | `	}` |
|      - |  419 | `	/* Mark as subclass */` |
|      3 |  420 | `	pSub->pBase = pBase;` |
|      - |  421 | `	/* All done */` |
|      3 |  422 | `	return SXRET_OK;` |
|      2 |  423 |  |
|      - |  424 | `/*` |
|      - |  425 | ` * Implements an object interface in the given main class.` |
|      - |  426 | ` * According to the PHP language reference manual.` |
|      - |  427 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|      - |  428 | ` *  must implement, without having to define how these methods are handled.` |
|      - |  429 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|      - |  430 | ` *  class, but without any of the methods having their contents defined.` |
|      - |  431 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|      - |  432 | ` *` |
|      - |  433 | ` * This function return SXRET_OK if the interface was successfully implemented.` |
|      - |  434 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|      - |  435 | ` * error message.` |
|      - |  436 | ` */` |
|      6 |  437 | `PH7_PRIVATE sxi32 PH7_ClassImplement(ph7_class *pMain,ph7_class *pInterface)` |
|      2 |  438 |  |
|      - |  439 | `	ph7_class_attr *pAttr;` |
|      - |  440 | `	SyHashEntry *pEntry;` |
|      - |  441 | `	SyString *pName;` |
|      - |  442 | `	sxi32 rc;` |
|      - |  443 | `	/* First off,copy all constants declared inside the interface */` |
|      8 |  444 | `	SyHashResetLoopCursor(&pInterface->hAttr);` |
|     13 |  445 | `	while((pEntry = SyHashGetNextEntry(&pInterface->hAttr)) != 0 ){` |
|      - |  446 | `		/* Point to the constant declaration */` |
|      3 |  447 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      3 |  448 | `		pName = &pAttr->sName;` |
|      - |  449 | `		/* Make sure the attribute is not redeclared in the main class */` |
|      3 |  450 | `		if( SyHashGet(&pMain->hAttr,pName->zString,pName->nByte) == 0 ){` |
|      - |  451 | `			/* Install the attribute */` |
|      3 |  452 | `			rc = SyHashInsert(&pMain->hAttr,pName->zString,pName->nByte,pAttr);` |
|      3 |  453 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  454 | `				return rc;` |
|      - |  455 | `			}` |
|      1 |  456 | `		}` |
|      1 |  457 | `	}` |
|      - |  458 | `	/* Install in the interface container */` |
|      8 |  459 | `	SySetPut(&pMain->aInterface,(const void *)&pInterface);` |
|      - |  460 | `	/* TICKET 1433-49/1: Symisc eXtension` |
|      - |  461 | `	 *  A class may not implemnt all declared interface methods,so there` |
|      - |  462 | `	 *  is no need for a method installer loop here.` |
|      - |  463 | `	 */` |
|      8 |  464 | `	return SXRET_OK;` |
|      5 |  465 |  |
|      - |  466 | `/*` |
|      - |  467 | ` * Create a class instance [i.e: Object in the PHP jargon] at run-time.` |
|      - |  468 | ` * The following function is called when an object is created at run-time` |
|      - |  469 | ` * typically when the PH7_OP_NEW/PH7_OP_CLONE instructions are executed.` |
|      - |  470 | ` * Notes on object creation.` |
|      - |  471 | ` *` |
|      - |  472 | ` * According to PHP language reference manual.` |
|      - |  473 | ` * To create an instance of a class, the new keyword must be used. An object will always` |
|      - |  474 | ` * be created unless the object has a constructor defined that throws an exception on error.` |
|      - |  475 | ` * Classes should be defined before instantiation (and in some cases this is a requirement).` |
|      - |  476 | ` * If a string containing the name of a class is used with new, a new instance of that class` |
|      - |  477 | ` * will be created. If the class is in a namespace, its fully qualified name must be used when` |
|      - |  478 | ` * doing this.` |
|      - |  479 | ` * Example #3 Creating an instance` |
|      - |  480 | ` * <?php` |
|      - |  481 | ` *  $instance = new SimpleClass();` |
|      - |  482 | ` *   // This can also be done with a variable:` |
|      - |  483 | ` * $className = 'Foo';` |
|      - |  484 | ` * $instance = new $className(); // Foo()` |
|      - |  485 | ` * ?>` |
|      - |  486 | ` * In the class context, it is possible to create a new object by new self and new parent.` |
|      - |  487 | ` * When assigning an already created instance of a class to a new variable, the new variable` |
|      - |  488 | ` * will access the same instance as the object that was assigned. This behaviour is the same` |
|      - |  489 | ` * when passing instances to a function. A copy of an already created object can be made by` |
|      - |  490 | ` * cloning it.` |
|      - |  491 | ` * Example #4 Object Assignment` |
|      - |  492 | ` * <?php` |
|      - |  493 | ` *  class SimpleClass(){` |
|      - |  494 | ` *    public $var;` |
|      - |  495 | ` *  };` |
|      - |  496 | ` *  $instance = new SimpleClass();` |
|      - |  497 | ` *  $assigned   =  $instance;` |
|      - |  498 | ` *  $reference  =& $instance;` |
|      - |  499 | ` *  $instance->var = '$assigned will have this value';` |
|      - |  500 | ` *  $instance = null; // $instance and $reference become null` |
|      - |  501 | ` *  var_dump($instance);` |
|      - |  502 | ` *  var_dump($reference);` |
|      - |  503 | ` *  var_dump($assigned);` |
|      - |  504 | ` * ?>` |
|      - |  505 | ` * The above example will output:` |
|      - |  506 | ` * NULL` |
|      - |  507 | ` * NULL` |
|      - |  508 | ` * object(SimpleClass)#1 (1) {` |
|      - |  509 | ` *  ["var"]=>` |
|      - |  510 | ` *    string(30) "$assigned will have this value"` |
|      - |  511 | ` * }` |
|      - |  512 | ` * Example #5 Creating new objects` |
|      - |  513 | ` * <?php` |
|      - |  514 | ` * class Test` |
|      - |  515 | ` * {` |
|      - |  516 | ` *   static public function getNew()` |
|      - |  517 | ` *   {` |
|      - |  518 | ` *       return new static;` |
|      - |  519 | ` *   }` |
|      - |  520 | ` * }` |
|      - |  521 | ` * class Child extends Test` |
|      - |  522 | ` * {}` |
|      - |  523 | ` * $obj1 = new Test();` |
|      - |  524 | ` * $obj2 = new $obj1;` |
|      - |  525 | ` * var_dump($obj1 !== $obj2);` |
|      - |  526 | ` * $obj3 = Test::getNew();` |
|      - |  527 | ` * var_dump($obj3 instanceof Test);` |
|      - |  528 | ` * $obj4 = Child::getNew();` |
|      - |  529 | ` * var_dump($obj4 instanceof Child);` |
|      - |  530 | ` * ?>` |
|      - |  531 | ` * The above example will output:` |
|      - |  532 | ` * bool(true)` |
|      - |  533 | ` * bool(true)` |
|      - |  534 | ` * bool(true)` |
|      - |  535 | ` * Note that Symisc Systems have introduced powerfull extension to` |
|      - |  536 | ` * OO subsystem. For example a class attribute may have any complex` |
|      - |  537 | ` * expression associated with it when declaring the attribute unlike` |
|      - |  538 | ` * the standard PHP engine which would allow a single value.` |
|      - |  539 | ` * Example:` |
|      - |  540 | ` *  class myClass{` |
|      - |  541 | ` *    public $var = 25<<1+foo()/bar();` |
|      - |  542 | ` *  };` |
|      - |  543 | ` * Refer to the official documentation for more information.` |
|      - |  544 | ` */` |
|   1100 |  545 | `static ph7_class_instance * NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|      2 |  546 |  |
|      - |  547 | `	ph7_class_instance *pThis;` |
|      - |  548 | `	/* Allocate a new instance */` |
|   1102 |  549 | `	pThis = (ph7_class_instance *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_instance));` |
|   1102 |  550 | `	if( pThis == 0 ){` |
|    ! 0 |  551 | `		return 0;` |
|      - |  552 | `	}` |
|      - |  553 | `	/* Zero the structure */` |
|   1102 |  554 | `	SyZero(pThis,sizeof(ph7_class_instance));` |
|      - |  555 | `	/* Initialize fields */` |
|   1102 |  556 | `	pThis->iRef = 1;` |
|   1102 |  557 | `	pThis->pVm = pVm;` |
|   1102 |  558 | `	pThis->pClass = pClass;` |
|   1102 |  559 | `	SyHashInit(&pThis->hAttr,&pVm->sAllocator,0,0);` |
|   1102 |  560 | `	return pThis;` |
|    552 |  561 |  |
|      - |  562 | `/*` |
|      - |  563 | ` * Wrapper around the NewClassInstance() function defined above.` |
|      - |  564 | ` * See the block comment above for more information.` |
|      - |  565 | ` */` |
|   1058 |  566 | `PH7_PRIVATE ph7_class_instance * PH7_NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|      2 |  567 |  |
|      - |  568 | `	ph7_class_instance *pNew;` |
|      - |  569 | `	sxi32 rc;` |
|   1060 |  570 | `	pNew = NewClassInstance(&(*pVm),&(*pClass));` |
|   1060 |  571 | `	if( pNew == 0 ){` |
|    ! 0 |  572 | `		return 0;` |
|      - |  573 | `	}` |
|      - |  574 | `	/* Associate a private VM frame with this class instance */` |
|   1060 |  575 | `	rc = PH7_VmCreateClassInstanceFrame(&(*pVm),pNew);` |
|   1060 |  576 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  577 | `		SyMemBackendPoolFree(&pVm->sAllocator,pNew);` |
|    ! 0 |  578 | `		return 0;` |
|      - |  579 | `	}` |
|   1060 |  580 | `	return pNew;` |
|    531 |  581 |  |
|      - |  582 | `/*` |
|      - |  583 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon] attribute.` |
|      - |  584 | ` * This function never fail.` |
|      - |  585 | ` */` |
|    540 |  586 | `static ph7_value * ExtractClassAttrValue(ph7_vm *pVm,VmClassAttr *pAttr)` |
|      2 |  587 |  |
|      - |  588 | `	/* Extract the value */` |
|      - |  589 | `	ph7_value *pValue;` |
|    542 |  590 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|    542 |  591 | `	return pValue;` |
|      2 |  592 |  |
|      - |  593 | `/*` |
|      - |  594 | ` * Perform a clone operation on a class instance [i.e: Object in the PHP jargon].` |
|      - |  595 | ` * The following function is called when an object is cloned at run-time` |
|      - |  596 | ` * typically when the PH7_OP_CLONE instruction is executed.` |
|      - |  597 | ` * Notes on object cloning.` |
|      - |  598 | ` *` |
|      - |  599 | ` * According to PHP language reference manual.` |
|      - |  600 | ` * Creating a copy of an object with fully replicated properties is not always the wanted behavior.` |
|      - |  601 | ` * A good example of the need for copy constructors. Another example is if your object holds a reference` |
|      - |  602 | ` * to another object which it uses and when you replicate the parent object you want to create` |
|      - |  603 | ` * a new instance of this other object so that the replica has its own separate copy.` |
|      - |  604 | ` * An object copy is created by using the clone keyword (which calls the object's __clone() method if possible).` |
|      - |  605 | ` * An object's __clone() method cannot be called directly.` |
|      - |  606 | ` * $copy_of_object = clone $object;` |
|      - |  607 | ` * When an object is cloned, PHP 5 will perform a shallow copy of all of the object's properties.` |
|      - |  608 | ` * Any properties that are references to other variables, will remain references.` |
|      - |  609 | ` * Once the cloning is complete, if a __clone() method is defined, then the newly created object's __clone() method` |
|      - |  610 | ` * will be called, to allow any necessary properties that need to be changed.` |
|      - |  611 | ` * Example #1 Cloning an object` |
|      - |  612 | ` * <?php` |
|      - |  613 | ` * class SubObject` |
|      - |  614 | ` * {` |
|      - |  615 | ` *   static $instances = 0;` |
|      - |  616 | ` *   public $instance;` |
|      - |  617 | ` *` |
|      - |  618 | ` *   public function __construct() {` |
|      - |  619 | ` *       $this->instance = ++self::$instances;` |
|      - |  620 | ` *   }` |
|      - |  621 | ` *` |
|      - |  622 | ` *   public function __clone() {` |
|      - |  623 | ` *       $this->instance = ++self::$instances;` |
|      - |  624 | ` *   }` |
|      - |  625 | ` * }` |
|      - |  626 | ` *` |
|      - |  627 | ` * class MyCloneable` |
|      - |  628 | ` * {` |
|      - |  629 | ` *   public $object1;` |
|      - |  630 | ` *   public $object2;` |
|      - |  631 | ` *` |
|      - |  632 | ` *   function __clone()` |
|      - |  633 | ` *   {` |
|      - |  634 | ` *       // Force a copy of this->object, otherwise` |
|      - |  635 | ` *       // it will point to same object.` |
|      - |  636 | ` *       $this->object1 = clone $this->object1;` |
|      - |  637 | ` *   }` |
|      - |  638 | ` * }` |
|      - |  639 | ` * $obj = new MyCloneable();` |
|      - |  640 | ` * $obj->object1 = new SubObject();` |
|      - |  641 | ` * $obj->object2 = new SubObject();` |
|      - |  642 | ` * $obj2 = clone $obj;` |
|      - |  643 | ` * print("Original Object:\n");` |
|      - |  644 | ` * print_r($obj);` |
|      - |  645 | ` * print("Cloned Object:\n");` |
|      - |  646 | ` * print_r($obj2);` |
|      - |  647 | ` * ?>` |
|      - |  648 | ` * The above example will output:` |
|      - |  649 | ` * Original Object:` |
|      - |  650 | ` * MyCloneable Object` |
|      - |  651 | ` * (` |
|      - |  652 | ` *   [object1] => SubObject Object` |
|      - |  653 | ` *       (` |
|      - |  654 | ` *           [instance] => 1` |
|      - |  655 | ` *       )` |
|      - |  656 | ` *` |
|      - |  657 | ` *   [object2] => SubObject Object` |
|      - |  658 | ` *       (` |
|      - |  659 | ` *           [instance] => 2` |
|      - |  660 | ` *       )` |
|      - |  661 | ` *` |
|      - |  662 | ` * )` |
|      - |  663 | ` * Cloned Object:` |
|      - |  664 | ` * MyCloneable Object` |
|      - |  665 | ` * (` |
|      - |  666 | ` *   [object1] => SubObject Object` |
|      - |  667 | ` *       (` |
|      - |  668 | ` *           [instance] => 3` |
|      - |  669 | ` *       )` |
|      - |  670 | ` *` |
|      - |  671 | ` *   [object2] => SubObject Object` |
|      - |  672 | ` *       (` |
|      - |  673 | ` *           [instance] => 2` |
|      - |  674 | ` *       )` |
|      - |  675 | ` * )` |
|      - |  676 | ` */` |
|     42 |  677 | `PH7_PRIVATE ph7_class_instance * PH7_CloneClassInstance(ph7_class_instance *pSrc)` |
|      2 |  678 |  |
|      - |  679 | `	ph7_class_instance *pClone;` |
|      - |  680 | `	ph7_class_method *pMethod;` |
|      - |  681 | `	SyHashEntry *pEntry2;` |
|      - |  682 | `	SyHashEntry *pEntry;` |
|      - |  683 | `	ph7_vm *pVm;` |
|      - |  684 | `	sxi32 rc;` |
|      - |  685 | `	/* Allocate a new instance */` |
|     44 |  686 | `	pVm = pSrc->pVm;` |
|     44 |  687 | `	pClone = NewClassInstance(pVm,pSrc->pClass);` |
|     44 |  688 | `	if( pClone == 0 ){` |
|    ! 0 |  689 | `		return 0;` |
|      - |  690 | `	}` |
|      - |  691 | `	/* Associate a private VM frame with this class instance */` |
|     44 |  692 | `	rc = PH7_VmCreateClassInstanceFrame(pVm,pClone);` |
|     44 |  693 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  694 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClone);` |
|    ! 0 |  695 | `		return 0;` |
|      - |  696 | `	}` |
|      - |  697 | `	/* Duplicate object values */` |
|     44 |  698 | `	SyHashResetLoopCursor(&pSrc->hAttr);` |
|     44 |  699 | `	SyHashResetLoopCursor(&pClone->hAttr);` |
|    111 |  700 | `	while((pEntry = SyHashGetNextEntry(&pSrc->hAttr)) != 0 && (pEntry2 = SyHashGetNextEntry(&pClone->hAttr)) != 0 ){` |
|     48 |  701 | `		VmClassAttr *pSrcAttr = (VmClassAttr *)pEntry->pUserData;` |
|     48 |  702 | `		VmClassAttr *pDestAttr = (VmClassAttr *)pEntry2->pUserData;` |
|      - |  703 | `		/* Duplicate non-static attribute */` |
|     48 |  704 | `		if( (pSrcAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|      - |  705 | `			ph7_value *pvSrc,*pvDest;` |
|     48 |  706 | `			pvSrc = ExtractClassAttrValue(pVm,pSrcAttr);` |
|     48 |  707 | `			pvDest = ExtractClassAttrValue(pVm,pDestAttr);` |
|     48 |  708 | `			if( pvSrc && pvDest ){` |
|     48 |  709 | `				PH7_MemObjStore(pvSrc,pvDest);` |
|     23 |  710 | `			}` |
|     23 |  711 | `		}` |
|      2 |  712 | `	}` |
|      - |  713 | `	/* call the __clone method on the cloned object if available */` |
|     44 |  714 | `	pMethod = PH7_ClassExtractMethod(pClone->pClass,"__clone",sizeof("__clone")-1);` |
|     44 |  715 | `	if( pMethod ){` |
|     38 |  716 | `		if( pMethod->iCloneDepth < 16 ){` |
|     36 |  717 | `			pMethod->iCloneDepth++;` |
|     36 |  718 | `			PH7_VmCallClassMethod(pVm,pClone,pMethod,0,0,0);` |
|     19 |  719 | `		}else{` |
|      - |  720 | `			/* Nesting limit reached */` |
|      3 |  721 | `			PH7_VmThrowError(pVm,0,PH7_CTX_ERR,"Object clone limit reached,no more call to __clone()");` |
|      - |  722 | `		}` |
|      - |  723 | `		/* Reset the cursor */` |
|     38 |  724 | `		pMethod->iCloneDepth = 0;` |
|     18 |  725 | `	}` |
|      - |  726 | `	/* Return the cloned object */` |
|     44 |  727 | `	return pClone;` |
|     23 |  728 |  |
|      - |  729 | `#define CLASS_INSTANCE_DESTROYED 0x001 /* Instance is released */` |
|      - |  730 | `/*` |
|      - |  731 | ` * Release a class instance [i.e: Object in the PHP jargon] and invoke any defined destructor.` |
|      - |  732 | ` * This routine is invoked as soon as there are no other references to a particular` |
|      - |  733 | ` * class instance.` |
|      - |  734 | ` */` |
|    772 |  735 | `static void PH7_ClassInstanceRelease(ph7_class_instance *pThis)` |
|      2 |  736 |  |
|      - |  737 | `	ph7_class_method *pDestr;` |
|      - |  738 | `	SyHashEntry *pEntry;` |
|      - |  739 | `	ph7_class *pClass;` |
|      - |  740 | `	ph7_vm *pVm;` |
|    774 |  741 | `	if( pThis->iFlags & CLASS_INSTANCE_DESTROYED ){` |
|      - |  742 | `		/*` |
|      - |  743 | `		 * Already destroyed,return immediately.` |
|      - |  744 | `		 * This could happend if someone perform unset($this) in the destructor body.` |
|      - |  745 | `		 */` |
|    ! 0 |  746 | `		return;` |
|      - |  747 | `	}` |
|      - |  748 | `	/* Mark as destroyed */` |
|    774 |  749 | `	pThis->iFlags \|= CLASS_INSTANCE_DESTROYED;` |
|      - |  750 | `	/* Invoke any defined destructor if available */` |
|    774 |  751 | `	pVm = pThis->pVm;` |
|    774 |  752 | `	pClass = pThis->pClass;` |
|    774 |  753 | `	pDestr = PH7_ClassExtractMethod(pClass,"__destruct",sizeof("__destruct")-1);` |
|    774 |  754 | `	if( pDestr ){` |
|      - |  755 | `		/* Invoke the destructor */` |
|      5 |  756 | `		pThis->iRef = 2; /* Prevent garbage collection */` |
|      5 |  757 | `		PH7_VmCallClassMethod(pVm,pThis,pDestr,0,0,0);` |
|      2 |  758 | `	}` |
|      - |  759 | `	/* Release non-static attributes */` |
|    774 |  760 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|   3998 |  761 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|   3226 |  762 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|   3226 |  763 | `		if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|   3222 |  764 | `			PH7_VmUnsetMemObj(pVm,pVmAttr->nIdx,TRUE);` |
|   1610 |  765 | `		}` |
|   3226 |  766 | `		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      2 |  767 | `	}` |
|      - |  768 | `	/* Release the whole structure */` |
|    774 |  769 | `	SyHashRelease(&pThis->hAttr);` |
|    774 |  770 | `	SyMemBackendPoolFree(&pVm->sAllocator,pThis);` |
|    388 |  771 |  |
|      - |  772 | `/*` |
|      - |  773 | ` * Decrement the reference count of a class instance [i.e Object in the PHP jargon].` |
|      - |  774 | ` * If the reference count reaches zero,release the whole instance.` |
|      - |  775 | ` */` |
|  13612 |  776 | `PH7_PRIVATE void PH7_ClassInstanceUnref(ph7_class_instance *pThis)` |
|      2 |  777 |  |
|  13614 |  778 | `	pThis->iRef--;` |
|  13614 |  779 | `	if( pThis->iRef < 1 ){` |
|      - |  780 | `		/* No more reference to this instance */` |
|    774 |  781 | `		PH7_ClassInstanceRelease(&(*pThis));` |
|    386 |  782 | `	}` |
|  13614 |  783 |  |
|      - |  784 | `/*` |
|      - |  785 | ` * Compare two class instances [i.e: Objects in the PHP jargon]` |
|      - |  786 | ` * Note on objects comparison:` |
|      - |  787 | ` *  According to the PHP langauge reference manual` |
|      - |  788 | ` *  When using the comparison operator (==), object variables are compared in a simple manner` |
|      - |  789 | ` *  namely: Two object instances are equal if they have the same attributes and values, and are` |
|      - |  790 | ` *  instances of the same class.` |
|      - |  791 | ` *  On the other hand, when using the identity operator (===), object variables are identical` |
|      - |  792 | ` *  if and only if they refer to the same instance of the same class.` |
|      - |  793 | ` *  An example will clarify these rules.` |
|      - |  794 | ` *  Example #1 Example of object comparison` |
|      - |  795 | ` *  <?php` |
|      - |  796 | ` *    function bool2str($bool)` |
|      - |  797 | ` * {` |
|      - |  798 | ` *   if ($bool === false) {` |
|      - |  799 | ` *       return 'FALSE';` |
|      - |  800 | ` *   } else {` |
|      - |  801 | ` *       return 'TRUE';` |
|      - |  802 | ` *   }` |
|      - |  803 | ` * }` |
|      - |  804 | ` * function compareObjects(&$o1, &$o2)` |
|      - |  805 | ` * {` |
|      - |  806 | ` *   echo 'o1 == o2 : ' . bool2str($o1 == $o2) . "\n";` |
|      - |  807 | ` *   echo 'o1 != o2 : ' . bool2str($o1 != $o2) . "\n";` |
|      - |  808 | ` *   echo 'o1 === o2 : ' . bool2str($o1 === $o2) . "\n";` |
|      - |  809 | ` *   echo 'o1 !== o2 : ' . bool2str($o1 !== $o2) . "\n";` |
|      - |  810 | ` * }` |
|      - |  811 | ` * class Flag` |
|      - |  812 | ` * {` |
|      - |  813 | ` *   public $flag;` |
|      - |  814 | ` *` |
|      - |  815 | ` *   function Flag($flag = true) {` |
|      - |  816 | ` *       $this->flag = $flag;` |
|      - |  817 | ` *   }` |
|      - |  818 | ` * }` |
|      - |  819 | ` *` |
|      - |  820 | ` * class OtherFlag` |
|      - |  821 | ` * {` |
|      - |  822 | ` *   public $flag;` |
|      - |  823 | ` *` |
|      - |  824 | ` *   function OtherFlag($flag = true) {` |
|      - |  825 | ` *       $this->flag = $flag;` |
|      - |  826 | ` *   }` |
|      - |  827 | ` * }` |
|      - |  828 | ` *` |
|      - |  829 | ` * $o = new Flag();` |
|      - |  830 | ` * $p = new Flag();` |
|      - |  831 | ` * $q = $o;` |
|      - |  832 | ` * $r = new OtherFlag();` |
|      - |  833 | ` *` |
|      - |  834 | ` * echo "Two instances of the same class\n";` |
|      - |  835 | ` * compareObjects($o, $p);` |
|      - |  836 | ` * echo "\nTwo references to the same instance\n";` |
|      - |  837 | ` * compareObjects($o, $q);` |
|      - |  838 | ` * echo "\nInstances of two different classes\n";` |
|      - |  839 | ` * compareObjects($o, $r);` |
|      - |  840 | ` * ?>` |
|      - |  841 | ` * The above example will output:` |
|      - |  842 | ` * Two instances of the same class` |
|      - |  843 | ` * o1 == o2 : TRUE` |
|      - |  844 | ` * o1 != o2 : FALSE` |
|      - |  845 | ` * o1 === o2 : FALSE` |
|      - |  846 | ` * o1 !== o2 : TRUE` |
|      - |  847 | ` * Two references to the same instance` |
|      - |  848 | ` * o1 == o2 : TRUE` |
|      - |  849 | ` * o1 != o2 : FALSE` |
|      - |  850 | ` * o1 === o2 : TRUE` |
|      - |  851 | ` * o1 !== o2 : FALSE` |
|      - |  852 | ` * Instances of two different classes` |
|      - |  853 | ` * o1 == o2 : FALSE` |
|      - |  854 | ` * o1 != o2 : TRUE` |
|      - |  855 | ` * o1 === o2 : FALSE` |
|      - |  856 | ` * o1 !== o2 : TRUE` |
|      - |  857 | ` *` |
|      - |  858 | ` * This function return 0 if the objects are equals according to the comprison rules defined above.` |
|      - |  859 | ` * Any other return values indicates difference.` |
|      - |  860 | ` */` |
|    160 |  861 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest)` |
|      2 |  862 |  |
|      - |  863 | `	SyHashEntry *pEntry,*pEntry2;` |
|      - |  864 | `	ph7_value sV1,sV2;` |
|      - |  865 | `	sxi32 rc;` |
|    162 |  866 | `	if( iNest > 31 ){` |
|      - |  867 | `		/* Nesting limit reached */` |
|      5 |  868 | `		PH7_VmThrowError(pLeft->pVm,0,PH7_CTX_ERR,"Nesting limit reached: Infinite recursion?");` |
|      5 |  869 | `		return 1;` |
|      - |  870 | `	}` |
|      - |  871 | `	/* Comparison is performed only if the objects are instance of the same class */` |
|    158 |  872 | `	if( pLeft->pClass != pRight->pClass ){` |
|      7 |  873 | `		return 1;` |
|      - |  874 | `	}` |
|    152 |  875 | `	if( bStrict ){` |
|      - |  876 | `		/*` |
|      - |  877 | `		 * According to the PHP language reference manual:` |
|      - |  878 | `		 *  when using the identity operator (===), object variables` |
|      - |  879 | `		 *  are identical if and only if they refer to the same instance` |
|      - |  880 | `		 *  of the same class.` |
|      - |  881 | `		 */` |
|     11 |  882 | `		return !(pLeft == pRight);` |
|      - |  883 | `	}` |
|      - |  884 | `	/*` |
|      - |  885 | `	 * Attribute comparison.` |
|      - |  886 | `	 * According to the PHP reference manual:` |
|      - |  887 | `	 *  When using the comparison operator (==), object variables are compared` |
|      - |  888 | `	 *  in a simple manner, namely: Two object instances are equal if they have` |
|      - |  889 | `	 *  the same attributes and values, and are instances of the same class.` |
|      - |  890 | `	 */` |
|    142 |  891 | `	if( pLeft == pRight ){` |
|      - |  892 | `		/* Same instance,don't bother processing,object are equals */` |
|      3 |  893 | `		return 0;` |
|      - |  894 | `	}` |
|    140 |  895 | `	SyHashResetLoopCursor(&pLeft->hAttr);` |
|    140 |  896 | `	SyHashResetLoopCursor(&pRight->hAttr);` |
|    140 |  897 | `	PH7_MemObjInit(pLeft->pVm,&sV1);` |
|    140 |  898 | `	PH7_MemObjInit(pLeft->pVm,&sV2);` |
|    140 |  899 | `	sV1.nIdx = sV2.nIdx = SXU32_HIGH;` |
|    223 |  900 | `	while((pEntry = SyHashGetNextEntry(&pLeft->hAttr)) != 0 && (pEntry2 = SyHashGetNextEntry(&pRight->hAttr)) != 0 ){` |
|    146 |  901 | `		VmClassAttr *p1 = (VmClassAttr *)pEntry->pUserData;` |
|    146 |  902 | `		VmClassAttr *p2 = (VmClassAttr *)pEntry2->pUserData;` |
|      - |  903 | `		/* Compare only non-static attribute */` |
|    146 |  904 | `		if( (p1->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|      - |  905 | `			ph7_value *pL,*pR;` |
|    146 |  906 | `			pL = ExtractClassAttrValue(pLeft->pVm,p1);` |
|    146 |  907 | `			pR = ExtractClassAttrValue(pRight->pVm,p2);` |
|    146 |  908 | `			if( pL && pR ){` |
|    146 |  909 | `				PH7_MemObjLoad(pL,&sV1);` |
|    146 |  910 | `				PH7_MemObjLoad(pR,&sV2);` |
|      - |  911 | `				/* Compare the two values now */` |
|    146 |  912 | `				rc = PH7_MemObjCmp(&sV1,&sV2,bStrict,iNest+1);` |
|    146 |  913 | `				PH7_MemObjRelease(&sV1);` |
|    146 |  914 | `				PH7_MemObjRelease(&sV2);` |
|    146 |  915 | `				if( rc != 0 ){` |
|      - |  916 | `					/* Not equals */` |
|    132 |  917 | `					return rc;` |
|      - |  918 | `				}` |
|      7 |  919 | `			}` |
|      7 |  920 | `		}` |
|      1 |  921 | `	}` |
|      - |  922 | `	/* Object are equals */` |
|      9 |  923 | `	return 0;` |
|     82 |  924 |  |
|      - |  925 | `/*` |
|      - |  926 | ` * Dump a class instance and the store the dump in the BLOB given` |
|      - |  927 | ` * as the first argument.` |
|      - |  928 | ` * Note that only non-static/non-constants attribute are dumped.` |
|      - |  929 | ` * This function is typically invoked when the user issue a call` |
|      - |  930 | ` * to [var_dump(),var_export(),print_r(),...].` |
|      - |  931 | ` * This function SXRET_OK on success. Any other return value including` |
|      - |  932 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|      - |  933 | ` */` |
|    132 |  934 | `PH7_PRIVATE sxi32 PH7_ClassInstanceDump(SyBlob *pOut,ph7_class_instance *pThis,int ShowType,int nTab,int nDepth)` |
|      1 |  935 |  |
|      - |  936 | `	SyHashEntry *pEntry;` |
|      - |  937 | `	ph7_value *pValue;` |
|      - |  938 | `	sxi32 rc;` |
|      - |  939 | `	int i;` |
|    133 |  940 | `	if( nDepth > 31 ){` |
|      - |  941 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|      - |  942 | `		/* Nesting limit reached..halt immediately*/` |
|      5 |  943 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|      5 |  944 | `		if( ShowType ){` |
|      5 |  945 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|      2 |  946 | `		}` |
|      5 |  947 | `		return SXERR_LIMIT;` |
|      - |  948 | `	}` |
|    129 |  949 | `	rc = SXRET_OK;` |
|    129 |  950 | `	if( !ShowType ){` |
|      3 |  951 | `		SyBlobAppend(&(*pOut),"Object(",sizeof("Object(")-1);` |
|      1 |  952 | `	}` |
|      - |  953 | `	/* Append class name */` |
|    129 |  954 | `	SyBlobFormat(&(*pOut),"%z) {",&pThis->pClass->sName);` |
|      - |  955 | `#ifdef __WINNT__` |
|      1 |  956 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|      - |  957 | `#else` |
|    128 |  958 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      - |  959 | `#endif` |
|      - |  960 | `	/* Dump object attributes */` |
|    129 |  961 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|    201 |  962 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|    133 |  963 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    133 |  964 | `		if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|      - |  965 | `			/* Dump non-static/constant attribute only */` |
|   3985 |  966 | `			for( i = 0 ; i < nTab ; i++ ){` |
|   3853 |  967 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|   1927 |  968 | `			}` |
|    133 |  969 | `			pValue = ExtractClassAttrValue(pThis->pVm,pVmAttr);` |
|    133 |  970 | `			if( pValue ){` |
|    133 |  971 | `				SyBlobFormat(&(*pOut),"['%z'] =>",&pVmAttr->pAttr->sName);` |
|      - |  972 | `#ifdef __WINNT__` |
|      1 |  973 | `				SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|      - |  974 | `#else` |
|    132 |  975 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      - |  976 | `#endif` |
|    133 |  977 | `				rc = PH7_MemObjDump(&(*pOut),pValue,ShowType,nTab+1,nDepth,0);` |
|    133 |  978 | `				if( rc == SXERR_LIMIT ){` |
|    125 |  979 | `					break;` |
|      - |  980 | `				}` |
|      4 |  981 | `			}` |
|      4 |  982 | `		}` |
|      1 |  983 | `	}` |
|   3977 |  984 | `	for( i = 0 ; i < nTab ; i++ ){` |
|   3849 |  985 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|   1925 |  986 | `	}` |
|    129 |  987 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|    129 |  988 | `	return rc;` |
|     67 |  989 |  |
|      - |  990 | `/*` |
|      - |  991 | ` * Call a magic method [i.e: __toString(),__toBool(),__Invoke()...]` |
|      - |  992 | ` * Return SXRET_OK on successfull call. Any other return value indicates failure.` |
|      - |  993 | ` * Notes on magic methods.` |
|      - |  994 | ` * According to the PHP language reference manual.` |
|      - |  995 | ` *  The function names __construct(), __destruct(), __call(), __callStatic()` |
|      - |  996 | ` *  __get(),  __toString(), __invoke(), __clone() are magical in PHP classes.` |
|      - |  997 | ` * You cannot have functions with these names in any of your classes unless` |
|      - |  998 | ` * you want the magic functionality associated with them.` |
|      - |  999 | ` * Example of magical methods:` |
|      - | 1000 | ` * __toString()` |
|      - | 1001 | ` *  The __toString() method allows a class to decide how it will react when it is treated like` |
|      - | 1002 | ` *  a string. For example, what echo $obj; will print. This method must return a string.` |
|      - | 1003 | ` *  Example #2 Simple example` |
|      - | 1004 | ` * <?php` |
|      - | 1005 | ` * // Declare a simple class` |
|      - | 1006 | ` * class TestClass` |
|      - | 1007 | ` * {` |
|      - | 1008 | ` *   public $foo;` |
|      - | 1009 | ` *` |
|      - | 1010 | ` *   public function __construct($foo)` |
|      - | 1011 | ` *   {` |
|      - | 1012 | ` *       $this->foo = $foo;` |
|      - | 1013 | ` *   }` |
|      - | 1014 | ` *` |
|      - | 1015 | ` *   public function __toString()` |
|      - | 1016 | ` *   {` |
|      - | 1017 | ` *       return $this->foo;` |
|      - | 1018 | ` *   }` |
|      - | 1019 | ` * }` |
|      - | 1020 | ` * $class = new TestClass('Hello');` |
|      - | 1021 | ` * echo $class;` |
|      - | 1022 | ` * ?>` |
|      - | 1023 | ` * The above example will output:` |
|      - | 1024 | ` *  Hello` |
|      - | 1025 | ` *` |
|      - | 1026 | ` * Note that PH7 does not support all the magical method and introudces __toFloat(),__toInt()` |
|      - | 1027 | ` * which have the same behaviour as __toString() but for float and integer types` |
|      - | 1028 | ` * respectively.` |
|      - | 1029 | ` * Refer to the official documentation for more information.` |
|      - | 1030 | ` */` |
|      4 | 1031 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallMagicMethod(` |
|      - | 1032 | `	ph7_vm *pVm,               /* VM that own all this stuff */` |
|      - | 1033 | `	ph7_class *pClass,         /* Target class */` |
|      - | 1034 | `	ph7_class_instance *pThis, /* Target object */` |
|      - | 1035 | `	const char *zMethod,       /* Magic method name [i.e: __toString()]*/` |
|      - | 1036 | `	sxu32 nByte,               /* zMethod length*/` |
|      - | 1037 | `	const SyString *pAttrName  /* Attribute name */` |
|      - | 1038 | `	)` |
|      2 | 1039 |  |
|      6 | 1040 | `	ph7_value *apArg[2] = { 0 , 0 };` |
|      - | 1041 | `	ph7_class_method *pMeth;` |
|      - | 1042 | `	ph7_value sAttr; /* cc warning */` |
|      - | 1043 | `	sxi32 rc;` |
|      - | 1044 | `	int nArg;` |
|      - | 1045 | `	/* Make sure the magic method is available */` |
|      6 | 1046 | `	pMeth = PH7_ClassExtractMethod(&(*pClass),zMethod,nByte);` |
|      6 | 1047 | `	if( pMeth == 0 ){` |
|      - | 1048 | `		/* No such method,return immediately */` |
|      3 | 1049 | `		return SXERR_NOTFOUND;` |
|      - | 1050 | `	}` |
|      3 | 1051 | `	nArg = 0;` |
|      - | 1052 | `	/* Copy arguments */` |
|      3 | 1053 | `	if( pAttrName ){` |
|    ! 0 | 1054 | `		PH7_MemObjInitFromString(pVm,&sAttr,pAttrName);` |
|    ! 0 | 1055 | `		sAttr.nIdx = SXU32_HIGH; /* Mark as constant */` |
|    ! 0 | 1056 | `		apArg[0] = &sAttr;` |
|    ! 0 | 1057 | `		nArg = 1;` |
|    ! 0 | 1058 | `	}` |
|      - | 1059 | `	/* Call the magic method now */` |
|      3 | 1060 | `	rc = PH7_VmCallClassMethod(pVm,&(*pThis),pMeth,0,nArg,apArg);` |
|      - | 1061 | `	/* Clean up */` |
|      3 | 1062 | `	if( pAttrName ){` |
|    ! 0 | 1063 | `		PH7_MemObjRelease(&sAttr);` |
|    ! 0 | 1064 | `	}` |
|      3 | 1065 | `	return rc;` |
|      4 | 1066 |  |
|      - | 1067 | `/*` |
|      - | 1068 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon].` |
|      - | 1069 | ` * This function is simply a wrapper on ExtractClassAttrValue().` |
|      - | 1070 | ` */` |
|     18 | 1071 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceExtractAttrValue(ph7_class_instance *pThis,VmClassAttr *pAttr)` |
|      1 | 1072 |  |
|      - | 1073 | `   /* Extract the attribute value */` |
|      - | 1074 | `	ph7_value *pValue;` |
|     19 | 1075 | `	pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     19 | 1076 | `	return pValue;` |
|      1 | 1077 |  |
|      - | 1078 | `/*` |
|      - | 1079 | ` * Convert a class instance [i.e: Object in the PHP jargon] into a hashmap [i.e: array in the PHP jargon].` |
|      - | 1080 | ` * Return SXRET_OK on success. Any other value indicates failure.` |
|      - | 1081 | ` * Note on object conversion to array:` |
|      - | 1082 | ` *  Acccording to the PHP language reference manual` |
|      - | 1083 | ` *  If an object is converted to an array, the result is an array whose elements are the object's properties.` |
|      - | 1084 | ` *  The keys are the member variable names.` |
|      - | 1085 | ` *` |
|      - | 1086 | ` *  The following example:` |
|      - | 1087 | ` *  class Test {` |
|      - | 1088 | ` *   public $A = 25<<1;  // 50` |
|      - | 1089 | ` *	 public $c = rand_str(3);   // Random string of length 3` |
|      - | 1090 | ` *	 public $d = rand() & 1023; // Random number between 0..1023` |
|      - | 1091 | ` *  }` |
|      - | 1092 | ` *  var_dump((array) new Test());` |
|      - | 1093 | ` *	Will output:` |
|      - | 1094 | ` *  array(3) {` |
|      - | 1095 | ` *   [A] =>` |
|      - | 1096 | ` *      int(50)` |
|      - | 1097 | ` *   [c] =>` |
|      - | 1098 | ` *     string(3 'aps')` |
|      - | 1099 | ` *   [d] =>` |
|      - | 1100 | ` *     int(991)` |
|      - | 1101 | ` *  }` |
|      - | 1102 | ` * You have noticed that PH7 allow class attributes [i.e: $a,$c,$d in the example above]` |
|      - | 1103 | ` * have any complex expression (even function calls/Annonymous functions) as their default` |
|      - | 1104 | ` * value unlike the standard PHP engine.` |
|      - | 1105 | ` * This is a very powerful feature that you have to look at.` |
|      - | 1106 | ` */` |
|      6 | 1107 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|      1 | 1108 |  |
|      - | 1109 | `	SyHashEntry *pEntry;` |
|      - | 1110 | `	SyString *pAttrName;` |
|      - | 1111 | `	VmClassAttr *pAttr;` |
|      - | 1112 | `	ph7_value *pValue;` |
|      - | 1113 | `	ph7_value sName;` |
|      - | 1114 | `	/* Reset the loop cursor */` |
|      7 | 1115 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      7 | 1116 | `	PH7_MemObjInitFromString(pThis->pVm,&sName,0);` |
|     20 | 1117 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|      - | 1118 | `		/* Point to the current attribute */` |
|     11 | 1119 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1120 | `		/* Extract attribute value */` |
|     11 | 1121 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     11 | 1122 | `		if( pValue ){` |
|      - | 1123 | `			/* Build attribute name */` |
|     11 | 1124 | `			pAttrName = &pAttr->pAttr->sName;` |
|     11 | 1125 | `			PH7_MemObjStringAppend(&sName,pAttrName->zString,pAttrName->nByte);` |
|      - | 1126 | `			/* Perform the insertion */` |
|     11 | 1127 | `			PH7_HashmapInsert(pMap,&sName,pValue);` |
|      - | 1128 | `			/* Reset the string cursor */` |
|     11 | 1129 | `			SyBlobReset(&sName.sBlob);` |
|      5 | 1130 | `		}` |
|      1 | 1131 | `	}` |
|      7 | 1132 | `	PH7_MemObjRelease(&sName);` |
|      7 | 1133 | `	return SXRET_OK;` |
|      1 | 1134 |  |
|      - | 1135 | `/*` |
|      - | 1136 | ` * Iterate throw class attributes and invoke the given callback [i.e: xWalk()] for each` |
|      - | 1137 | ` * retrieved attribute.` |
|      - | 1138 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|      - | 1139 | ` * the attribute value in the callback body will not alter the real attribute value.` |
|      - | 1140 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|      - | 1141 | ` * a value different from PH7_OK.` |
|      - | 1142 | ` * Refer to [ph7_object_walk()] for more information.` |
|      - | 1143 | ` */` |
|    ! 0 | 1144 | `PH7_PRIVATE sxi32 PH7_ClassInstanceWalk(` |
|      - | 1145 | `	ph7_class_instance *pThis, /* Target object */` |
|      - | 1146 | `	int (*xWalk)(const char *,ph7_value *,void *), /* Walker callback */` |
|      - | 1147 | `	void *pUserData /* Last argument to xWalk() */` |
|      - | 1148 | `	)` |
|    ! 0 | 1149 |  |
|      - | 1150 | `	SyHashEntry *pEntry; /* Hash entry */` |
|      - | 1151 | `	VmClassAttr *pAttr;  /* Pointer to the attribute */` |
|      - | 1152 | `	ph7_value *pValue;   /* Attribute value */` |
|      - | 1153 | `	ph7_value sValue;    /* Copy of the attribute value */` |
|      - | 1154 | `	int rc;` |
|      - | 1155 | `	/* Reset the loop cursor */` |
|    ! 0 | 1156 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|    ! 0 | 1157 | `	PH7_MemObjInit(pThis->pVm,&sValue);` |
|      - | 1158 | `	/* Start the walk process */` |
|    ! 0 | 1159 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|      - | 1160 | `		/* Point to the current attribute */` |
|    ! 0 | 1161 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1162 | `		/* Extract attribute value */` |
|    ! 0 | 1163 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|    ! 0 | 1164 | `		if( pValue ){` |
|    ! 0 | 1165 | `			PH7_MemObjLoad(pValue,&sValue);` |
|      - | 1166 | `			/* Invoke the supplied callback */` |
|    ! 0 | 1167 | `			rc =  xWalk(SyStringData(&pAttr->pAttr->sName),&sValue,pUserData);` |
|    ! 0 | 1168 | `			PH7_MemObjRelease(&sValue);` |
|    ! 0 | 1169 | `			if( rc != PH7_OK){` |
|      - | 1170 | `				/* User callback request an operation abort */` |
|    ! 0 | 1171 | `				return SXERR_ABORT;` |
|      - | 1172 | `			}` |
|    ! 0 | 1173 | `		}` |
|    ! 0 | 1174 | `	}` |
|      - | 1175 | `	/* All done */` |
|    ! 0 | 1176 | `	return SXRET_OK;` |
|    ! 0 | 1177 |  |
|      - | 1178 | `/*` |
|      - | 1179 | ` * Extract a class atrribute value.` |
|      - | 1180 | ` * Return a pointer to the attribute value on success. Otherwise NULL.` |
|      - | 1181 | ` * Note:` |
|      - | 1182 | ` *  Access to static and constant attribute is not allowed. That is,the function` |
|      - | 1183 | ` *  will return NULL in case someone (host-application code) try to extract` |
|      - | 1184 | ` *  a static/constant attribute.` |
|      - | 1185 | ` */` |
|    ! 0 | 1186 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceFetchAttr(ph7_class_instance *pThis,const SyString *pName)` |
|    ! 0 | 1187 |  |
|      - | 1188 | `	SyHashEntry *pEntry;` |
|      - | 1189 | `	VmClassAttr *pAttr;` |
|      - | 1190 | `	/* Query the attribute hashtable */` |
|    ! 0 | 1191 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);` |
|    ! 0 | 1192 | `	if( pEntry == 0 ){` |
|      - | 1193 | `		/* No such attribute */` |
|    ! 0 | 1194 | `		return 0;` |
|      - | 1195 | `	}` |
|      - | 1196 | `	/* Point to the class atrribute */` |
|    ! 0 | 1197 | `	pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1198 | `	/* Check if we are dealing with a static/constant attribute */` |
|    ! 0 | 1199 | `	if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|      - | 1200 | `		/* Access is forbidden */` |
|    ! 0 | 1201 | `		return 0;` |
|      - | 1202 | `	}` |
|      - | 1203 | `	/* Return the attribute value */` |
|    ! 0 | 1204 | `	return ExtractClassAttrValue(pThis->pVm,pAttr);` |
|    ! 0 | 1205 |  |
|      - | 1206 |  |
