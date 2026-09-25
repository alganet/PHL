# src/ph7/oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 914/1047 lines (87.30%)

[Root index](../../index.md) | [Directory index](index.md)

|     Hits | Line | Source |
| -------: | ---: | :--- |
|        - |    1 | `/**` |
|        - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|        - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|        - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|        - |    5 | ` */` |
|        - |    6 | `#include "ph7int.h"` |
|        - |    7 | `/*` |
|        - |    8 | ` * This file implement an Object Oriented (OO) subsystem for the PH7 engine.` |
|        - |    9 | ` */` |
|        - |   10 | `/*` |
|        - |   11 | ` * Create an empty class.` |
|        - |   12 | ` * Return a pointer to a raw class (ph7_class instance) on success. NULL otherwise.` |
|        - |   13 | ` */` |
|   739754 |   14 | `PH7_PRIVATE ph7_class * PH7_NewRawClass(ph7_vm *pVm,const SyString *pName,sxu32 nLine)` |
|        5 |   15 | `{` |
|        - |   16 | `	ph7_class *pClass;` |
|        - |   17 | `	char *zName;` |
|        - |   18 | `	/* Allocate a new instance */` |
|   739759 |   19 | `	pClass = (ph7_class *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class));` |
|   739759 |   20 | `	if( pClass == 0 ){` |
|      ! 0 |   21 | `		return 0;` |
|        - |   22 | `	}` |
|        - |   23 | `	/* Zero the structure */` |
|   739759 |   24 | `	SyZero(pClass,sizeof(ph7_class));` |
|        - |   25 | `	/* Duplicate class name */` |
|   739759 |   26 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   739759 |   27 | `	if( zName == 0 ){` |
|      ! 0 |   28 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClass);` |
|      ! 0 |   29 | `		return 0;` |
|        - |   30 | `	}` |
|        - |   31 | `	/* Initialize fields */` |
|   739759 |   32 | `	SyStringInitFromBuf(&pClass->sName,zName,pName->nByte);` |
|        - |   33 | `	/* php method names are CASE-INSENSITIVE ($o->FOO() finds foo(), and declaring` |
|        - |   34 | `	 * both is a redeclaration), so the method table matches on them the same way` |
|        - |   35 | `	 * hClass does for class names. Properties and class constants ARE case` |
|        - |   36 | `	 * sensitive in php, so hAttr keeps the default exact comparator. */` |
|   739759 |   37 | `	SyHashInit(&pClass->hMethod,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|   739759 |   38 | `	SyHashInit(&pClass->hAttr,&pVm->sAllocator,0,0);` |
|   739759 |   39 | `	SyHashInit(&pClass->hConst,&pVm->sAllocator,0,0);` |
|   739759 |   40 | `	SyHashInit(&pClass->hDerived,&pVm->sAllocator,0,0);` |
|   739759 |   41 | `	SySetInit(&pClass->aInterface,&pVm->sAllocator,sizeof(ph7_class *));` |
|   739759 |   42 | `	SySetInit(&pClass->aTrait,&pVm->sAllocator,sizeof(ph7_class *));` |
|   739759 |   43 | `	SySetInit(&pClass->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|   739759 |   44 | `	SySetInit(&pClass->aEnumCases,&pVm->sAllocator,sizeof(ph7_class_attr *));` |
|   739759 |   45 | `	pClass->nLine = nLine;` |
|   739759 |   46 | `	if( pVm->bCompilingBuiltin ){` |
|        - |   47 | `		/* Defined by an embedded builtin chunk: internal, no defining file.` |
|        - |   48 | `		 * Class compilers merge further flags with \|= so this survives. */` |
|   735883 |   49 | `		pClass->iFlags \|= PH7_CLASS_INTERNAL;` |
|   367944 |   50 | `	}else{` |
|        - |   51 | `		/* Alias the VM-lifetime path dup on top of the include stack */` |
|     3881 |   52 | `		SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     3881 |   53 | `		if( pFile ){` |
|     3881 |   54 | `			SyStringDupPtr(&pClass->sFile,pFile);` |
|     1938 |   55 | `		}` |
|        - |   56 | `	}` |
|        - |   57 | `	/* All done */` |
|   739759 |   58 | `	return pClass;` |
|   369882 |   59 | `}` |
|        - |   60 | `/*` |
|        - |   61 | ` * Allocate and initialize a new class attribute.` |
|        - |   62 | ` * Return a pointer to the class attribute on success. NULL otherwise.` |
|        - |   63 | ` */` |
|  1387008 |   64 | `PH7_PRIVATE ph7_class_attr * PH7_NewClassAttr(ph7_vm *pVm,const SyString *pName,sxu32 nLine,sxi32 iProtection,sxi32 iFlags)` |
|        5 |   65 | `{` |
|        - |   66 | `	ph7_class_attr *pAttr;` |
|        - |   67 | `	char *zName;` |
|  1387013 |   68 | `	pAttr = (ph7_class_attr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_attr));` |
|  1387013 |   69 | `	if( pAttr == 0 ){` |
|      ! 0 |   70 | `		return 0;` |
|        - |   71 | `	}` |
|        - |   72 | `	/* Zero the structure */` |
|  1387013 |   73 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|  1387013 |   74 | `	SySetInit(&pAttr->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|        - |   75 | `	/* Duplicate attribute name */` |
|  1387013 |   76 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1387013 |   77 | `	if( zName == 0 ){` |
|      ! 0 |   78 | `		SyMemBackendPoolFree(&pVm->sAllocator,pAttr);` |
|      ! 0 |   79 | `		return 0;` |
|        - |   80 | `	}` |
|        - |   81 | `	/* Initialize fields */` |
|  1387013 |   82 | `	SySetInit(&pAttr->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|  1387013 |   83 | `	SySetInit(&pAttr->aUnionAlts,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|  1387013 |   84 | `	SyStringInitFromBuf(&pAttr->sName,zName,pName->nByte);` |
|  1387013 |   85 | `	pAttr->iProtection = iProtection;` |
|  1387013 |   86 | `	pAttr->nIdx = SXU32_HIGH;` |
|  1387013 |   87 | `	pAttr->iFlags = iFlags;` |
|  1387013 |   88 | `	pAttr->nLine = nLine;` |
|  1387013 |   89 | `	return pAttr;` |
|   693509 |   90 | `}` |
|        - |   91 | `/*` |
|        - |   92 | ` * Allocate and initialize a new class method.` |
|        - |   93 | ` * Return a pointer to the class method on success. NULL otherwise` |
|        - |   94 | ` * This function associate with the newly created method an automatically generated` |
|        - |   95 | ` * random unique name.` |
|        - |   96 | ` */` |
|  4156784 |   97 | `PH7_PRIVATE ph7_class_method * PH7_NewClassMethod(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,sxu32 nLine,` |
|        - |   98 | `	sxi32 iProtection,sxi32 iFlags,sxi32 iFuncFlags)` |
|        5 |   99 | `{` |
|        - |  100 | `	ph7_class_method *pMeth;` |
|        - |  101 | `	SyHashEntry *pEntry;` |
|        - |  102 | `	SyString *pNamePtr;` |
|        - |  103 | `	char zSalt[10];` |
|        - |  104 | `	char *zName;` |
|        - |  105 | `	sxu32 nByte;` |
|        - |  106 | `	/* Allocate a new class method instance */` |
|  4156789 |  107 | `	pMeth = (ph7_class_method *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_method));` |
|  4156789 |  108 | `	if( pMeth == 0 ){` |
|      ! 0 |  109 | `		return 0;` |
|        - |  110 | `	}` |
|        - |  111 | `	/* Zero the structure */` |
|  4156789 |  112 | `	SyZero(pMeth,sizeof(ph7_class_method));` |
|        - |  113 | `	/* Check for an already installed method with the same name */` |
|  4156789 |  114 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
|  4156789 |  115 | `	if( pEntry == 0 ){` |
|        - |  116 | `		/* Associate an unique VM name to this method */` |
|  4156785 |  117 | `		nByte = sizeof(zSalt) + pName->nByte + SyStringLength(&pClass->sName)+sizeof(char)*7/*[[__'\0'*/;` |
|  4156785 |  118 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,nByte);` |
|  4156785 |  119 | `		if( zName == 0 ){` |
|      ! 0 |  120 | `			SyMemBackendPoolFree(&pVm->sAllocator,pMeth);` |
|      ! 0 |  121 | `			return 0;` |
|        - |  122 | `		}` |
|  4156785 |  123 | `		pNamePtr = &pMeth->sVmName;` |
|        - |  124 | `		/* Generate a random string */` |
|  4156785 |  125 | `		PH7_VmRandomString(&(*pVm),zSalt,sizeof(zSalt));` |
|  4156785 |  126 | `		pNamePtr->nByte = SyBufferFormat(zName,nByte,"[__%z@%z_%.*s]",&pClass->sName,pName,sizeof(zSalt),zSalt);` |
|  4156785 |  127 | `		pNamePtr->zString = zName;` |
|  2078395 |  128 | `	}else{` |
|        - |  129 | `		/* Method is condidate for 'overloading' */` |
|        6 |  130 | `		ph7_class_method *pCurrent = (ph7_class_method *)pEntry->pUserData;` |
|        6 |  131 | `		pNamePtr = &pMeth->sVmName;` |
|        - |  132 | `		/* Use the same VM name */` |
|        6 |  133 | `		SyStringDupPtr(pNamePtr,&pCurrent->sVmName);` |
|        6 |  134 | `		zName = (char *)pNamePtr->zString;` |
|        - |  135 | `	}` |
|  4156789 |  136 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|    82597 |  137 | `		if( pName->nByte == sizeof("__destruct") - 1 && SyMemcmp(pName->zString,"__destruct",sizeof("__destruct") - 1 ) == 0 ){` |
|        - |  138 | `				/* Switch to public visibility for destructors (the engine invokes them` |
|        - |  139 | `				 * internally, bypassing visibility either way). __construct KEEPS its` |
|        - |  140 | ``				 * declared visibility (band A #4): php enforces it at `new` — a`` |
|        - |  141 | `				 * private/protected ctor from the wrong scope is a catchable Error,` |
|        - |  142 | `				 * checked at OP_NEW — and ReflectionClass::isInstantiable()/` |
|        - |  143 | `				 * newInstance() now see it. A method named like the class is a PLAIN` |
|        - |  144 | `				 * method (PHP-4 constructors removed in 8.0), so it keeps its declared` |
|        - |  145 | `				 * visibility too — no longer forced public. */` |
|      ! 0 |  146 | `				iProtection = PH7_CLASS_PROT_PUBLIC;` |
|      ! 0 |  147 | `		}` |
|    41296 |  148 | `	}` |
|        - |  149 | `	/* Initialize method fields */` |
|  4156789 |  150 | `	pMeth->iProtection = iProtection;` |
|  4156789 |  151 | `	pMeth->iFlags = iFlags;` |
|  4156789 |  152 | `	pMeth->nLine = nLine;` |
|  6235181 |  153 | `	PH7_VmInitFuncState(&(*pVm),&pMeth->sFunc,&zName[sizeof(char)*4/*[__@*/+SyStringLength(&pClass->sName)],` |
|  4156784 |  154 | `		pName->nByte,iFuncFlags\|VM_FUNC_CLASS_METHOD,pClass);` |
|  4156789 |  155 | `	return pMeth;` |
|  2078397 |  156 | `}` |
|        - |  157 | `/*` |
|        - |  158 | ` * Check if the given name have a class method associated with it.` |
|        - |  159 | ` * Return the desired method [i.e: ph7_class_method instance] on success. NULL otherwise.` |
|        - |  160 | ` */` |
|  6978649 |  161 | `PH7_PRIVATE ph7_class_method * PH7_ClassExtractMethod(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|        5 |  162 | `{` |
|        - |  163 | `	SyHashEntry *pEntry;` |
|        - |  164 | `	/* Perform a hash lookup */` |
|  6978654 |  165 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)zName,nByte);` |
|  6978654 |  166 | `	if( pEntry == 0 ){` |
|        - |  167 | `		/* No such entry */` |
|  1776301 |  168 | `		return 0;` |
|        - |  169 | `	}` |
|        - |  170 | `	/* Point to the desired method */` |
|  5202358 |  171 | `	return (ph7_class_method *)pEntry->pUserData;` |
|  3489331 |  172 | `}` |
|        - |  173 | `/*` |
|        - |  174 | ` * Check if the given name is a class attribute.` |
|        - |  175 | ` * Return the desired attribute [i.e: ph7_class_attr instance] on success.NULL otherwise.` |
|        - |  176 | ` */` |
|   102678 |  177 | `PH7_PRIVATE ph7_class_attr * PH7_ClassExtractAttribute(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|        5 |  178 | `{` |
|        - |  179 | `	SyHashEntry *pEntry;` |
|        - |  180 | `	/* Perform a hash lookup */` |
|   102683 |  181 | `	pEntry = SyHashGet(&pClass->hAttr,(const void *)zName,nByte);` |
|   102683 |  182 | `	if( pEntry == 0 ){` |
|        - |  183 | `		/* No such entry */` |
|     2211 |  184 | `		return 0;` |
|        - |  185 | `	}` |
|        - |  186 | `	/* Point to the desierd method */` |
|   100477 |  187 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|    51344 |  188 | `}` |
|        - |  189 | `/*` |
|        - |  190 | ` * Check if the given name is a class CONSTANT (or enum case).` |
|        - |  191 | ` * php keeps constants and properties in separate namespaces, so constants live` |
|        - |  192 | ` * in a dedicated table (hConst) and never collide with a same-named property.` |
|        - |  193 | ` * Return the desired constant [ph7_class_attr with PH7_CLASS_ATTR_CONSTANT] on` |
|        - |  194 | ` * success, NULL otherwise.` |
|        - |  195 | ` */` |
|     1500 |  196 | `PH7_PRIVATE ph7_class_attr * PH7_ClassExtractConstant(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|        5 |  197 | `{` |
|        - |  198 | `	SyHashEntry *pEntry;` |
|     1505 |  199 | `	pEntry = SyHashGet(&pClass->hConst,(const void *)zName,nByte);` |
|     1505 |  200 | `	if( pEntry == 0 ){` |
|      481 |  201 | `		return 0;` |
|        - |  202 | `	}` |
|     1029 |  203 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|      755 |  204 | `}` |
|        - |  205 | `/*` |
|        - |  206 | ` * Install a class attribute in the corresponding container.` |
|        - |  207 | ` * A constant (or enum case) goes to hConst, a property to hAttr — php's two` |
|        - |  208 | `` * separate member namespaces, so `const C` and `public $C` coexist.`` |
|        - |  209 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|        - |  210 | ` */` |
|  1387004 |  211 | `PH7_PRIVATE sxi32 PH7_ClassInstallAttr(ph7_class *pClass,ph7_class_attr *pAttr)` |
|        5 |  212 | `{` |
|  1387009 |  213 | `	SyString *pName = &pAttr->sName;` |
|        - |  214 | `	sxi32 rc;` |
|        - |  215 | `	/* Remember where this attribute was originally declared so that later` |
|        - |  216 | `	 * inheritance/trait copies still know the declaring class (needed for` |
|        - |  217 | `	 * PHP-compatible error messages on typed properties). */` |
|  1387009 |  218 | `	if( pAttr->pDeclClass == 0 ){` |
|    13027 |  219 | `		pAttr->pDeclClass = pClass;` |
|     6511 |  220 | `	}` |
|  1387009 |  221 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|   463729 |  222 | `		rc = SyHashInsertTail(&pClass->hConst,(const void *)pName->zString,pName->nByte,pAttr);` |
|   231867 |  223 | `	}else{` |
|   923285 |  224 | `		rc = SyHashInsertTail(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|        - |  225 | `	}` |
|  1387009 |  226 | `	return rc;` |
|        5 |  227 | `}` |
|        - |  228 | `/*` |
|        - |  229 | ` * Install a class method in the corresponding container.` |
|        - |  230 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|        - |  231 | ` */` |
|  4156746 |  232 | `PH7_PRIVATE sxi32 PH7_ClassInstallMethod(ph7_class *pClass,ph7_class_method *pMeth)` |
|        5 |  233 | `{` |
|  4156751 |  234 | `	SyString *pName = &pMeth->sFunc.sName;` |
|        - |  235 | `	sxi32 rc;` |
|  4156751 |  236 | `	rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|  4156751 |  237 | `	return rc;` |
|        5 |  238 | `}` |
|        - |  239 | `/*` |
|        - |  240 | ` * Method-override compatibility (variance) checking.` |
|        - |  241 | ` *` |
|        - |  242 | ` * PHP rejects an override whose signature is incompatible with the parent's:` |
|        - |  243 | ` * return types are covariant (child may only narrow), parameter types are` |
|        - |  244 | ` * contravariant (child may only widen), and a child may not add a required` |
|        - |  245 | ` * parameter. We add the diagnostic — but conservatively: PHL must keep running` |
|        - |  246 | ` * valid PHP, so the comparator below is SKIP-BY-DEFAULT. It flags only cases that` |
|        - |  247 | ` * are unambiguously invalid and silently accepts anything subtle (unions,` |
|        - |  248 | ` * intersections, pseudo-types, self/parent/static, object, unresolved classes,` |
|        - |  249 | ` * or a missing type), so it can never reject valid code.` |
|        - |  250 | ` */` |
|        - |  251 | `#define OVT_NONE   0  /* no declared type */` |
|        - |  252 | `#define OVT_SCALAR 1  /* a concrete invariant scalar: int/float/string/bool/array */` |
|        - |  253 | `#define OVT_CLASS  2  /* a real, already-loaded class/interface */` |
|        - |  254 | `#define OVT_SKIP   3  /* union/intersection/pseudo/self/object/unresolved — never flag */` |
|        - |  255 |  |
|        - |  256 | `/*` |
|        - |  257 | ` * Classify one declared type (nType + class name + union flag) for override` |
|        - |  258 | ` * comparison. On OVT_CLASS, *ppClass receives the resolved class. Class names are` |
|        - |  259 | ` * resolved by a direct, autoload-free hClass lookup: a miss (forward reference,` |
|        - |  260 | ` * namespaced, or not-yet-loaded) yields OVT_SKIP, which the caller accepts.` |
|        - |  261 | ` */` |
|      340 |  262 | `static int OoClassifyOverrideType(ph7_vm *pVm, sxu32 nType, const SyString *pClass,` |
|        - |  263 | `	int bUnion, ph7_class **ppClass)` |
|        5 |  264 | `{` |
|      345 |  265 | `	*ppClass = 0;` |
|      345 |  266 | `	if( bUnion ){` |
|        3 |  267 | `		return OVT_SKIP; /* union/intersection — full lattice, skip */` |
|        - |  268 | `	}` |
|      343 |  269 | `	if( nType == 0 ){` |
|      184 |  270 | `		return OVT_NONE; /* no declared type */` |
|        - |  271 | `	}` |
|      163 |  272 | `	if( nType == SXU32_HIGH ){` |
|        - |  273 | `		/* A class name OR a pseudo-type stored as a name atom. Skip every pseudo` |
|        - |  274 | `		 * (incl. self/parent/static, which are context-relative). */` |
|        - |  275 | `		static const struct { const char *z; sxu32 n; } aPseudo[] = {` |
|        - |  276 | `			{"mixed",5}, {"never",5}, {"iterable",8}, {"callable",8}, {"true",4},` |
|        - |  277 | `			{"false",5}, {"self",4}, {"parent",6}, {"static",6}` |
|        - |  278 | `		};` |
|       27 |  279 | `		const char *z = pClass->zString;` |
|       27 |  280 | `		sxu32 n = pClass->nByte;` |
|        - |  281 | `		SyHashEntry *pE;` |
|        - |  282 | `		sxu32 i;` |
|      199 |  283 | `		for( i = 0; i < SX_ARRAYSIZE(aPseudo); i++ ){` |
|      183 |  284 | `			if( n == aPseudo[i].n && SyStrnmicmp(z,aPseudo[i].z,n) == 0 ){` |
|       10 |  285 | `				return OVT_SKIP;` |
|        - |  286 | `			}` |
|       89 |  287 | `		}` |
|       19 |  288 | `		pE = SyHashGet(&pVm->hClass,(const void *)z,n);` |
|       19 |  289 | `		if( pE == 0 ){` |
|      ! 0 |  290 | `			return OVT_SKIP; /* not loaded / forward ref / namespaced — accept */` |
|        - |  291 | `		}` |
|       19 |  292 | `		*ppClass = (ph7_class *)pE->pUserData;` |
|       19 |  293 | `		return OVT_CLASS;` |
|        - |  294 | `	}` |
|      134 |  295 | `	if( nType == MEMOBJ_STRING \|\| nType == MEMOBJ_INT \|\| nType == MEMOBJ_REAL` |
|       49 |  296 | `	 \|\| nType == MEMOBJ_BOOL \|\| nType == MEMOBJ_HASHMAP ){` |
|       94 |  297 | `		return OVT_SCALAR;` |
|        - |  298 | `	}` |
|        - |  299 | `	/* MEMOBJ_OBJ (object — subtypes against classes), MEMOBJ_VOID/NULL/RES,` |
|        - |  300 | `	 * or anything unexpected: skip. */` |
|       47 |  301 | `	return OVT_SKIP;` |
|      175 |  302 | `}` |
|        - |  303 |  |
|        - |  304 | `/*` |
|        - |  305 | ` * A declared type normalized for override comparison: the raw type code, the` |
|        - |  306 | ` * class-name string (when a class), and the union/nullable flags. Extracted once` |
|        - |  307 | ` * from each side so the comparator takes two of these instead of eight scalars.` |
|        - |  308 | ` */` |
|        - |  309 | `typedef struct OvType OvType;` |
|        - |  310 | `struct OvType {` |
|        - |  311 | `	sxu32 nType;` |
|        - |  312 | `	const SyString *pClass;` |
|        - |  313 | `	int bUnion;` |
|        - |  314 | `	int bNullable;` |
|        - |  315 | `};` |
|      244 |  316 | `static OvType OoTypeFromReturn(ph7_vm_func *pF)` |
|        5 |  317 | `{` |
|        - |  318 | `	OvType t;` |
|      249 |  319 | `	t.nType = pF->nReturnType;` |
|      249 |  320 | `	t.pClass = &pF->sReturnClass;` |
|      249 |  321 | `	t.bUnion = SySetUsed(&pF->aReturnUnion) > 0;` |
|      249 |  322 | `	t.bNullable = (pF->iFlags & VM_FUNC_RETURN_NULLABLE) != 0;` |
|      249 |  323 | `	return t;` |
|        5 |  324 | `}` |
|       96 |  325 | `static OvType OoTypeFromArg(ph7_vm_func_arg *pA)` |
|        3 |  326 | `{` |
|        - |  327 | `	OvType t;` |
|       99 |  328 | `	t.nType = pA->nType;` |
|       99 |  329 | `	t.pClass = &pA->sClass;` |
|       99 |  330 | `	t.bUnion = (pA->iFlags & VM_FUNC_ARG_UNION) != 0;` |
|       99 |  331 | `	t.bNullable = (pA->iFlags & VM_FUNC_ARG_NULLABLE) != 0;` |
|       99 |  332 | `	return t;` |
|        3 |  333 | `}` |
|        - |  334 | `/*` |
|        - |  335 | ` * Return TRUE if the child type is an unambiguously-invalid override of the` |
|        - |  336 | ` * parent type. bCovariant=1 for a return type (child must be ⊆ parent),` |
|        - |  337 | ` * 0 for a parameter (child must be ⊇ parent). Returns FALSE (accept) on any` |
|        - |  338 | ` * skipped/ambiguous shape.` |
|        - |  339 | ` */` |
|      170 |  340 | `static int OoOverrideTypeBad(ph7_vm *pVm, OvType parent, OvType child, int bCovariant)` |
|        5 |  341 | `{` |
|        - |  342 | `	ph7_class *pParentCls, *pChildCls;` |
|      175 |  343 | `	int kP = OoClassifyOverrideType(pVm, parent.nType, parent.pClass, parent.bUnion, &pParentCls);` |
|      175 |  344 | `	int kC = OoClassifyOverrideType(pVm, child.nType, child.pClass, child.bUnion, &pChildCls);` |
|      175 |  345 | `	if( kP == OVT_SKIP \|\| kC == OVT_SKIP ){` |
|       31 |  346 | `		return 0; /* ambiguous shape — conservatively accept */` |
|        - |  347 | `	}` |
|        - |  348 | `	/* A missing type is the TOP type. covariant (return): a concrete child is a` |
|        - |  349 | `	 * subtype of top, fine; a top child over a concrete parent WIDENS → bad.` |
|        - |  350 | `	 * contravariant (param): a top child is a supertype of anything, fine; a` |
|        - |  351 | `	 * concrete child over a top parent NARROWS → bad. (A union/intersection child` |
|        - |  352 | `	 * already fell into OVT_SKIP above, so a flagged child here is scalar/class.) */` |
|      147 |  353 | `	if( kP == OVT_NONE \|\| kC == OVT_NONE ){` |
|       96 |  354 | `		if( bCovariant && kC == OVT_NONE && kP != OVT_NONE ) return 1;` |
|       96 |  355 | `		if( !bCovariant && kP == OVT_NONE && kC != OVT_NONE ) return 1;` |
|       96 |  356 | `		return 0;` |
|        - |  357 | `	}` |
|        - |  358 | `	/* Nullability: a covariant return may not ADD null; a contravariant param may` |
|        - |  359 | `	 * not REMOVE null. */` |
|       55 |  360 | `	if( bCovariant ){` |
|       34 |  361 | `		if( child.bNullable && !parent.bNullable ) return 1;` |
|       19 |  362 | `	}else{` |
|       23 |  363 | `		if( parent.bNullable && !child.bNullable ) return 1;` |
|        - |  364 | `	}` |
|       55 |  365 | `	if( kP == OVT_SCALAR && kC == OVT_SCALAR ){` |
|        - |  366 | `		/* Scalars are invariant — they must match exactly. */` |
|       46 |  367 | `		return (parent.nType != child.nType) ? 1 : 0;` |
|        - |  368 | `	}` |
|       11 |  369 | `	if( kP == OVT_CLASS && kC == OVT_CLASS ){` |
|       11 |  370 | `		if( bCovariant ){` |
|        6 |  371 | `			return PH7_VmInstanceOf(pChildCls, pParentCls) ? 0 : 1;  /* child ⊆ parent */` |
|        - |  372 | `		}` |
|        6 |  373 | `		return PH7_VmInstanceOf(pParentCls, pChildCls) ? 0 : 1;      /* child ⊇ parent */` |
|        - |  374 | `	}` |
|        - |  375 | `	/* One scalar and one class — disjoint. */` |
|      ! 0 |  376 | `	return 1;` |
|       90 |  377 | `}` |
|        - |  378 |  |
|        - |  379 | `/*` |
|        - |  380 | ` * Check a child method's signature against the parent method it overrides.` |
|        - |  381 | ` * Emits a PHP-style "Declaration of … must be compatible …" fatal on a clear` |
|        - |  382 | `` * incompatibility. `__construct` is exempt (PHP does not apply variance to it).`` |
|        - |  383 | ` */` |
|   257520 |  384 | `static sxi32 OoCheckOverrideCompat(ph7_gen_state *pGen, ph7_class *pBase, ph7_class *pSub,` |
|        - |  385 | `	ph7_class_method *pParent, ph7_class_method *pChild)` |
|        5 |  386 | `{` |
|   257525 |  387 | `	ph7_vm *pVm = pGen->pVm;` |
|   257525 |  388 | `	ph7_vm_func *pPF = &pParent->sFunc;` |
|   257525 |  389 | `	ph7_vm_func *pCF = &pChild->sFunc;` |
|   257525 |  390 | `	SyString *pMName = &pCF->sName;` |
|        - |  391 | `	ph7_vm_func_arg *aP, *aC;` |
|        - |  392 | `	sxu32 nPArg, nCArg, k;` |
|   257525 |  393 | `	int bBad = 0;` |
|   257520 |  394 | `	if( pMName->nByte == sizeof("__construct")-1` |
|   175094 |  395 | `	 && SyStrnmicmp(pMName->zString,"__construct",pMName->nByte) == 0 ){` |
|    82357 |  396 | `		return SXRET_OK;` |
|        - |  397 | `	}` |
|        - |  398 | `	/*` |
|        - |  399 | `	 * A NATIVE method declares its parameters in a zSig STRING, so its aArgs set` |
|        - |  400 | `	 * is empty and there is nothing here to compare against. Reading that as` |
|        - |  401 | `	 * "declares no parameters" made every override of one incompatible: a user` |
|        - |  402 | `	 * class extending DOMDocument, a Reflection class or a native enum's own` |
|        - |  403 | `	 * cases()/from() all fataled on a declaration php accepts. An` |
|        - |  404 | `	 * engine-declared signature is compatible by construction, on either side.` |
|        - |  405 | `	 */` |
|   175173 |  406 | `	if( ((pPF->iFlags \| pCF->iFlags) & VM_FUNC_NATIVE) != 0 ){` |
|   175051 |  407 | `		return SXRET_OK;` |
|        - |  408 | `	}` |
|        - |  409 | `	/* Return type — covariant. */` |
|      127 |  410 | `	bBad = OoOverrideTypeBad(pVm, OoTypeFromReturn(pPF), OoTypeFromReturn(pCF), /* bCovariant */ 1);` |
|        - |  411 | `	/* Each overlapping parameter — contravariant. */` |
|      127 |  412 | `	nPArg = SySetUsed(&pPF->aArgs);` |
|      127 |  413 | `	nCArg = SySetUsed(&pCF->aArgs);` |
|      127 |  414 | `	aP = (ph7_vm_func_arg *)SySetBasePtr(&pPF->aArgs);` |
|      127 |  415 | `	aC = (ph7_vm_func_arg *)SySetBasePtr(&pCF->aArgs);` |
|      175 |  416 | `	for( k = 0; !bBad && k < nPArg && k < nCArg; k++ ){` |
|       51 |  417 | `		bBad = OoOverrideTypeBad(pVm, OoTypeFromArg(&aP[k]), OoTypeFromArg(&aC[k]), /* bCovariant */ 0);` |
|       27 |  418 | `	}` |
|        - |  419 | `	/* Parameter arity: the child must declare at least the parent's parameters and` |
|        - |  420 | `	 * may add only OPTIONAL ones — PHP rejects dropping any param (even an optional` |
|        - |  421 | `	 * one) or adding a required one. Skip the rule if either signature is variadic` |
|        - |  422 | `	 * (arity semantics differ). */` |
|      127 |  423 | `	if( !bBad ){` |
|      122 |  424 | `		int bVariadic = 0;` |
|      168 |  425 | `		for( k = 0; k < nPArg; k++ ){ if( aP[k].iFlags & VM_FUNC_ARG_VARIADIC ) bVariadic = 1; }` |
|      170 |  426 | `		for( k = 0; k < nCArg; k++ ){ if( aC[k].iFlags & VM_FUNC_ARG_VARIADIC ) bVariadic = 1; }` |
|      122 |  427 | `		if( !bVariadic ){` |
|      122 |  428 | `			if( nCArg < nPArg ){` |
|      ! 0 |  429 | `				bBad = 1; /* dropped a parent parameter */` |
|      ! 0 |  430 | `			}else{` |
|      124 |  431 | `				for( k = nPArg; k < nCArg; k++ ){` |
|        3 |  432 | `					if( SySetUsed(&aC[k].aByteCode) == 0 ){ bBad = 1; break; } /* new required */` |
|        2 |  433 | `				}` |
|        - |  434 | `			}` |
|       59 |  435 | `		}` |
|       59 |  436 | `	}` |
|      127 |  437 | `	if( bBad ){` |
|        8 |  438 | `		sxi32 rc = PH7_GenCompileError(&(*pGen),E_ERROR,pChild->nLine,` |
|        - |  439 | `			"Declaration of %z::%z() must be compatible with %z::%z()",` |
|        2 |  440 | `			&pSub->sName,pMName,&pBase->sName,&pParent->sFunc.sName);` |
|        6 |  441 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  442 | `			return SXERR_ABORT;` |
|        - |  443 | `		}` |
|        2 |  444 | `	}` |
|      127 |  445 | `	return SXRET_OK;` |
|   128765 |  446 | `}` |
|        - |  447 | `/*` |
|        - |  448 | ` * Perform an inheritance operation.` |
|        - |  449 | ` * According to the PHP language reference manual` |
|        - |  450 | ` *  When you extend a class, the subclass inherits all of the public and protected methods` |
|        - |  451 | ` *  from the parent class. Unless a class Overwrites those methods, they will retain their original` |
|        - |  452 | ` *  functionality.` |
|        - |  453 | ` *  This is useful for defining and abstracting functionality, and permits the implementation` |
|        - |  454 | ` *  of additional functionality in similar objects without the need to reimplement all of the shared` |
|        - |  455 | ` *  functionality.` |
|        - |  456 | ` *  Example #1 Inheritance Example` |
|        - |  457 | ` * <?php` |
|        - |  458 | ` * class foo` |
|        - |  459 | ` * {` |
|        - |  460 | ` *   public function printItem($string)` |
|        - |  461 | ` *   {` |
|        - |  462 | ` *       echo 'Foo: ' . $string . PHP_EOL;` |
|        - |  463 | ` *   }` |
|        - |  464 | ` *` |
|        - |  465 | ` *   public function printPHP()` |
|        - |  466 | ` *   {` |
|        - |  467 | ` *       echo 'PHP is great.' . PHP_EOL;` |
|        - |  468 | ` *   }` |
|        - |  469 | ` * }` |
|        - |  470 | ` * class bar extends foo` |
|        - |  471 | ` * {` |
|        - |  472 | ` *   public function printItem($string)` |
|        - |  473 | ` *   {` |
|        - |  474 | ` *       echo 'Bar: ' . $string . PHP_EOL;` |
|        - |  475 | ` *   }` |
|        - |  476 | ` * }` |
|        - |  477 | ` * $foo = new foo();` |
|        - |  478 | ` * $bar = new bar();` |
|        - |  479 | ` * $foo->printItem('baz'); // Output: 'Foo: baz'` |
|        - |  480 | ` * $foo->printPHP();       // Output: 'PHP is great'` |
|        - |  481 | ` * $bar->printItem('baz'); // Output: 'Bar: baz'` |
|        - |  482 | ` * $bar->printPHP();       // Output: 'PHP is great'` |
|        - |  483 | ` *` |
|        - |  484 | ` * This function return SXRET_OK if the inheritance operation was successfully performed.` |
|        - |  485 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|        - |  486 | ` * error message.` |
|        - |  487 | ` */` |
|   345368 |  488 | `PH7_PRIVATE sxi32 PH7_ClassInherit(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase)` |
|        5 |  489 | `{` |
|        - |  490 | `	ph7_class_method *pMeth;` |
|        - |  491 | `	ph7_class_attr *pAttr;` |
|        - |  492 | `	SyHashEntry *pEntry;` |
|        - |  493 | `	SyString *pName;` |
|        - |  494 | `	SySet aInherited; /* base attributes to prepend (see the copy loop below) */` |
|        - |  495 | `	sxi32 rc;` |
|   345373 |  496 | `	SySetInit(&aInherited,&pGen->pVm->sAllocator,sizeof(ph7_class_attr *));` |
|        - |  497 | `	/* Install in the derived hashtable */` |
|   345373 |  498 | `	rc = SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|   345373 |  499 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  500 | `		SySetRelease(&aInherited);` |
|      ! 0 |  501 | `		return rc;` |
|        - |  502 | `	}` |
|        - |  503 | `	/* readonly class inheritance (PHP 8.2): a readonly class may only extend a` |
|        - |  504 | `	 * readonly class, and a non-readonly class may not extend a readonly one. */` |
|   345373 |  505 | `	if( (pBase->iFlags & PH7_CLASS_READONLY) != (pSub->iFlags & PH7_CLASS_READONLY) ){` |
|        5 |  506 | `		if( pBase->iFlags & PH7_CLASS_READONLY ){` |
|        4 |  507 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|        - |  508 | `				"Non-readonly class %z cannot extend readonly class %z",` |
|        1 |  509 | `				&pSub->sName,&pBase->sName);` |
|        2 |  510 | `		}else{` |
|        4 |  511 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|        - |  512 | `				"Readonly class %z cannot extend non-readonly class %z",` |
|        1 |  513 | `				&pSub->sName,&pBase->sName);` |
|        - |  514 | `		}` |
|        5 |  515 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  516 | `			SySetRelease(&aInherited);` |
|      ! 0 |  517 | `			return SXERR_ABORT;` |
|        - |  518 | `		}` |
|        2 |  519 | `	}` |
|        - |  520 | `	/* Copy public/protected attributes from the base class */` |
|   345373 |  521 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
|  2178543 |  522 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|        - |  523 | `		/* Make sure the private attributes are not redeclared in the subclass */` |
|  1833175 |  524 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  1833175 |  525 | `		pName = &pAttr->sName;` |
|  1833175 |  526 | `		if( (pEntry = SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|    10318 |  527 | `			if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_FINAL))` |
|     5164 |  528 | `				== (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_FINAL) ){` |
|        - |  529 | `				/* Cannot override a final class constant (PHP 8.1). Report the` |
|        - |  530 | `				 * class that originally declared it (pDeclClass) rather than the` |
|        - |  531 | `				 * immediate base, so a multi-level chain matches PHP. */` |
|      ! 0 |  532 | `				ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pBase;` |
|      ! 0 |  533 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_attr *)pEntry->pUserData)->nLine,` |
|        - |  534 | `					"%z::%z cannot override final constant %z::%z",` |
|      ! 0 |  535 | `					&pSub->sName,pName,&pOwner->sName,pName);` |
|      ! 0 |  536 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  537 | `					SySetRelease(&aInherited);` |
|      ! 0 |  538 | `					return SXERR_ABORT;` |
|        - |  539 | `				}` |
|      ! 0 |  540 | `			}` |
|        - |  541 | `			/* A child MAY redeclare a base's private property: php treats the two` |
|        - |  542 | `			 * as independent members (each private to its declaring class), with no` |
|        - |  543 | `			 * diagnostic. PH7 warned here, which is wrong — the child's entry simply` |
|        - |  544 | `			 * shadows the base's in the by-name attribute table.` |
|        - |  545 | `			 *` |
|        - |  546 | `			 * Ordering: php keeps an overridden INSTANCE property at the position` |
|        - |  547 | `` 			 * the BASE declared it (`class G{$g1;$g2;} class H extends G{$h;$g1;}` `` |
|        - |  548 | `			 * iterates g1,g2,h — not g2,h,g1). Re-collect the CHILD's definition,` |
|        - |  549 | `			 * whose default value wins, and drop its current entry so the prepend` |
|        - |  550 | `			 * below re-inserts it in base order. Statics/constants are not part of` |
|        - |  551 | `			 * instance iteration, so they keep their existing slot. */` |
|    10323 |  552 | `			if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|    10321 |  553 | `				ph7_class_attr *pOwn = (ph7_class_attr *)pEntry->pUserData;` |
|    10321 |  554 | `				SyHashDeleteEntry(&pSub->hAttr,(const void *)pName->zString,pName->nByte,0);` |
|    10321 |  555 | `				rc = SySetPut(&aInherited,(const void *)&pOwn);` |
|    10321 |  556 | `				if( rc != SXRET_OK ){` |
|      ! 0 |  557 | `					SySetRelease(&aInherited);` |
|      ! 0 |  558 | `					return rc;` |
|        - |  559 | `				}` |
|     5158 |  560 | `			}` |
|    10323 |  561 | `			continue;` |
|        - |  562 | `		}` |
|        - |  563 | `		/* Collect the attribute. php: a base class's private INSTANCE property` |
|        - |  564 | `		 * lives on every child instance too (its own methods read/write it` |
|        - |  565 | `		 * through $this on the child; the access check grants private access by` |
|        - |  566 | `		 * DECLARING class, so child methods and outsiders still can't touch it).` |
|        - |  567 | `		 * Private STATICS/CONSTANTS stay uncopied — base methods reach those` |
|        - |  568 | `		 * through self:: against the declaring class directly.` |
|        - |  569 | `		 *` |
|        - |  570 | `		 * These are gathered rather than installed here because php orders an` |
|        - |  571 | `		 * instance's properties BASE-DECLARED FIRST, then the subclass's own,` |
|        - |  572 | `		 * then trait members — while inheritance runs AFTER the subclass body` |
|        - |  573 | `		 * has already filled hAttr. They are prepended below. */` |
|  1822852 |  574 | `		if( pAttr->iProtection != PH7_CLASS_PROT_PRIVATE` |
|  1380012 |  575 | `		 \|\| (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|  1822853 |  576 | `			rc = SySetPut(&aInherited,(const void *)&pAttr);` |
|  1822853 |  577 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  578 | `				SySetRelease(&aInherited);` |
|      ! 0 |  579 | `				return rc;` |
|        - |  580 | `			}` |
|   911424 |  581 | `		}` |
|        5 |  582 | `	}` |
|        - |  583 | `	/* Prepend the collected base attributes so hAttr reads base-first. The` |
|        - |  584 | `	 * table's iteration list is what every object-iteration consumer walks` |
|        - |  585 | `	 * (var_dump/print_r, get_object_vars, foreach, (array) casts, json_encode),` |
|        - |  586 | `	 * so this ordering is user-visible — json_encode emits its keys in exactly` |
|        - |  587 | `	 * this order. SyHashInsert is a HEAD insert, so walking the collected set` |
|        - |  588 | `	 * backwards leaves the base's own declaration order at the front. */` |
|   345373 |  589 | `	if( SySetUsed(&aInherited) > 0 ){` |
|   345107 |  590 | `		ph7_class_attr **apInherited = (ph7_class_attr **)SySetBasePtr(&aInherited);` |
|   345107 |  591 | `		sxu32 n = SySetUsed(&aInherited);` |
|  2178271 |  592 | `		while( n > 0 ){` |
|  1833169 |  593 | `			ph7_class_attr *pIn = apInherited[--n];` |
|  1833169 |  594 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pIn->sName.zString,pIn->sName.nByte,pIn);` |
|  1833169 |  595 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  596 | `				SySetRelease(&aInherited);` |
|      ! 0 |  597 | `				return rc;` |
|        - |  598 | `			}` |
|        5 |  599 | `		}` |
|   172551 |  600 | `	}` |
|        - |  601 | `	/* Inherit the base's CONSTANTS (separate namespace: hConst). Constants are not` |
|        - |  602 | `	 * part of instance iteration, so no base-first ordering dance is needed — a plain` |
|        - |  603 | `	 * copy of every constant the subclass did not itself redeclare. A subclass that` |
|        - |  604 | `	 * redeclares a base FINAL constant is a fatal (PHP 8.1). */` |
|   345373 |  605 | `	SyHashResetLoopCursor(&pBase->hConst);` |
|   561803 |  606 | `	while((pEntry = SyHashGetNextEntry(&pBase->hConst)) != 0 ){` |
|        - |  607 | `		SyHashEntry *pOwn;` |
|   216435 |  608 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   216435 |  609 | `		pName = &pAttr->sName;` |
|   216435 |  610 | `		if( (pOwn = SyHashGet(&pSub->hConst,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|        6 |  611 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_FINAL) ){` |
|        - |  612 | `				/* Cannot override a final class constant. Report the class that` |
|        - |  613 | `				 * originally declared it (pDeclClass) for a multi-level chain. */` |
|        3 |  614 | `				ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pBase;` |
|        4 |  615 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_attr *)pOwn->pUserData)->nLine,` |
|        - |  616 | `					"%z::%z cannot override final constant %z::%z",` |
|        1 |  617 | `					&pSub->sName,pName,&pOwner->sName,pName);` |
|        3 |  618 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  619 | `					SySetRelease(&aInherited);` |
|      ! 0 |  620 | `					return SXERR_ABORT;` |
|        - |  621 | `				}` |
|        1 |  622 | `			}` |
|        6 |  623 | `			continue;` |
|        - |  624 | `		}` |
|   216431 |  625 | `		rc = SyHashInsertTail(&pSub->hConst,(const void *)pName->zString,pName->nByte,pAttr);` |
|   216431 |  626 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  627 | `			SySetRelease(&aInherited);` |
|      ! 0 |  628 | `			return rc;` |
|        - |  629 | `		}` |
|        5 |  630 | `	}` |
|   345373 |  631 | `	SySetRelease(&aInherited);` |
|   345373 |  632 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
|  5289183 |  633 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|        - |  634 | `		SyHashEntry *pOwn;` |
|        - |  635 | `		SyString sKey;` |
|        - |  636 | `		/* Make sure the private/final methods are not redeclared in the subclass.` |
|        - |  637 | `		 * The identity inherited is the base's HASH KEY, not sFunc.sName — the same` |
|        - |  638 | `		 * rule PH7_ClassUseTrait copies a trait by. A trait adaptation leaves the` |
|        - |  639 | `		 * composed class holding entries whose key is the name the class ANSWERS to` |
|        - |  640 | ``		 * while the method struct keeps its original name: `B::m as mB` is the key`` |
|        - |  641 | `` 		 * `mB` over a struct still called `m`, and `A::m insteadof B` is the key `m` `` |
|        - |  642 | ``		 * over A's struct. Keying the copy off sFunc.sName re-filed both under `m`,`` |
|        - |  643 | `		 * so a subclass of the composing class lost the alias entirely and took` |
|        - |  644 | ``		 * whichever of the two the hash walk reached last as its `m` — the insteadof`` |
|        - |  645 | `		 * choice, silently reversed. */` |
|  4943815 |  646 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  4943815 |  647 | `		SyStringInitFromBuf(&sKey,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|  4943815 |  648 | `		pName = &sKey;` |
|  4943815 |  649 | `		if( (pOwn = SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|   257537 |  650 | `			if( pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|        - |  651 | `				/* php: a base's PRIVATE method is never overridden — the child's` |
|        - |  652 | `				 * declaration is an independent member of the same name, so neither` |
|        - |  653 | `				 * the final rule nor the signature-compatibility rule applies to it` |
|        - |  654 | `				 * (zend's do_inherit_method skips both for a private parent). PHL` |
|        - |  655 | ``				 * ran both: `class A { private function m($a){} } class B extends A`` |
|        - |  656 | ``				 * { private function m($a,$b,$c){} }` was a fatal php compiles, and`` |
|        - |  657 | ``				 * `final private` in the base fataled every child that reused the`` |
|        - |  658 | `				 * name — php only WARNS at the final-private DECLARATION and lets` |
|        - |  659 | `				 * the child have the name. */` |
|   257533 |  660 | `			}else if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|        - |  661 | `				/* php: "Cannot override final method A::test()" */` |
|        7 |  662 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_method *)pOwn->pUserData)->nLine,` |
|        - |  663 | `					"Cannot override final method %z::%z()",` |
|        2 |  664 | `					&pBase->sName,pName);` |
|        2 |  665 | `				(void)pSub;` |
|        5 |  666 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  667 | `					return SXERR_ABORT;` |
|        - |  668 | `				}` |
|        3 |  669 | `			}else{` |
|        - |  670 | `				/* Check the override's signature is compatible with the parent's. */` |
|   386285 |  671 | `				rc = OoCheckOverrideCompat(&(*pGen),pBase,pSub,pMeth,` |
|   257520 |  672 | `					(ph7_class_method *)pOwn->pUserData);` |
|   257525 |  673 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  674 | `					return SXERR_ABORT;` |
|        - |  675 | `				}` |
|        - |  676 | `			}` |
|   257537 |  677 | `			continue;` |
|        - |  678 | `		}` |
|        - |  679 | `		/* Install the method. php: a base class's private method is in the child's` |
|        - |  680 | `		 * table too — an inherited public method calling $this->priv() must find it,` |
|        - |  681 | ``		 * and the LOOKUP has to find it for php's answer to `B::p()` to be`` |
|        - |  682 | `		 * "Call to private method A::p() from global scope" rather than` |
|        - |  683 | `		 * "Call to undefined method B::p()". The call-site visibility check binds by` |
|        - |  684 | `		 * DECLARING class (sFunc.pUserData), so child code and outsiders still cannot` |
|        - |  685 | ``		 * reach it; a private ctor copied down blocks `new Child` from outside like`` |
|        - |  686 | `		 * php's; and the surfaces that must NOT show an inherited private say so` |
|        - |  687 | `		 * themselves (method_exists, get_class_methods, ReflectionClass::getMethods).` |
|        - |  688 | `		 *` |
|        - |  689 | `		 * STATIC privates used to be skipped here, on the reasoning that base methods` |
|        - |  690 | `		 * reach them through self:: against the declaring class anyway. They do — but` |
|        - |  691 | ``		 * nothing else could: every spelling of `B::p()` (the call, `['B','p']()`,`` |
|        - |  692 | `		 * call_user_func, a first-class callable) reported the name as UNDEFINED, and` |
|        - |  693 | ``		 * `static::p()` from the base with a subclass as the late-static-binding`` |
|        - |  694 | `		 * target could not find its own method. */` |
|  4686283 |  695 | `		rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|  4686283 |  696 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  697 | `			return rc;` |
|        - |  698 | `		}` |
|        5 |  699 | `	}` |
|        - |  700 | `	/* Mark as subclass */` |
|   345373 |  701 | `	pSub->pBase = pBase;` |
|        - |  702 | `	/* All done */` |
|   345373 |  703 | `	return SXRET_OK;` |
|   172689 |  704 | `}` |
|        - |  705 | `/*` |
|        - |  706 | ` * Apply a trait to a class: copy all methods and attributes from the trait` |
|        - |  707 | ` * into the target class. Unlike inheritance, traits copy ALL members including` |
|        - |  708 | ` * private ones. Members already defined in the class take precedence.` |
|        - |  709 | ` */` |
|      154 |  710 | `PH7_PRIVATE sxi32 PH7_ClassUseTrait(ph7_gen_state *pGen,ph7_class *pClass,ph7_class *pTrait)` |
|        5 |  711 | `{` |
|        - |  712 | `	ph7_class_method *pMeth;` |
|        - |  713 | `	ph7_class_attr *pAttr;` |
|        - |  714 | `	SyHashEntry *pEntry;` |
|        - |  715 | `	SyString *pName;` |
|        - |  716 | `	sxi32 rc;` |
|        - |  717 | `	/* Detect cyclic trait composition (e.g. trait A { use B; } trait B { use A; }) */` |
|      159 |  718 | `	if( pTrait->iFlags & PH7_CLASS_TRAIT_VISITING ){` |
|      ! 0 |  719 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|      ! 0 |  720 | `			"Trait circular reference detected: %z is already being applied",&pTrait->sName);` |
|      ! 0 |  721 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  722 | `			return SXERR_ABORT;` |
|        - |  723 | `		}` |
|      ! 0 |  724 | `		return SXRET_OK;` |
|        - |  725 | `	}` |
|      159 |  726 | `	pTrait->iFlags \|= PH7_CLASS_TRAIT_VISITING;` |
|      159 |  727 | `	rc = SXRET_OK;` |
|        - |  728 | `	/* Copy attributes from the trait */` |
|      159 |  729 | `	SyHashResetLoopCursor(&pTrait->hAttr);` |
|      189 |  730 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hAttr)) != 0 ){` |
|        - |  731 | `		SyHashEntry *pExisting;` |
|       35 |  732 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|       35 |  733 | `		pName = &pAttr->sName;` |
|       35 |  734 | `		pExisting = SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte);` |
|       35 |  735 | `		if( pExisting != 0 ){` |
|        - |  736 | `			/* Attribute already exists. Check if it came from another trait` |
|        - |  737 | `			 * and whether the definitions are compatible (same defaults).` |
|        - |  738 | `			 */` |
|        - |  739 | `			ph7_class **apUsedTraits;` |
|        - |  740 | `			sxu32 nUsed,k;` |
|        6 |  741 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|        6 |  742 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|        6 |  743 | `			for(k = 0; k < nUsed; k++){` |
|        - |  744 | `				ph7_class_attr *pOther;` |
|        3 |  745 | `				pOther = PH7_ClassExtractAttribute(apUsedTraits[k],pName->zString,pName->nByte);` |
|        3 |  746 | `				if( pOther ){` |
|        - |  747 | `					/* Two traits define the same property — check if defaults differ */` |
|        3 |  748 | `					ph7_class_attr *pClassAttr = (ph7_class_attr *)pExisting->pUserData;` |
|        4 |  749 | `					if( SySetUsed(&pAttr->aByteCode) != SySetUsed(&pClassAttr->aByteCode) \|\|` |
|        3 |  750 | `						(SySetUsed(&pAttr->aByteCode) > 0 &&` |
|        3 |  751 | `						 SyMemcmp(SySetBasePtr(&pAttr->aByteCode),SySetBasePtr(&pClassAttr->aByteCode),` |
|        3 |  752 | `							SySetUsed(&pAttr->aByteCode) * SySetElemSize(&pAttr->aByteCode)) != 0) ){` |
|        4 |  753 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine,` |
|        - |  754 | `							"%z and %z define the same property ($%z) in the composition of %z. "` |
|        - |  755 | `							"However, the definition differs and is considered incompatible",` |
|        2 |  756 | `							&apUsedTraits[k]->sName,&pTrait->sName,pName,&pClass->sName);` |
|        3 |  757 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 |  758 | `							goto cleanup;` |
|        - |  759 | `						}` |
|        1 |  760 | `					}` |
|        3 |  761 | `					break;` |
|        - |  762 | `				}` |
|      ! 0 |  763 | `			}` |
|        6 |  764 | `			continue;` |
|        - |  765 | `		}` |
|       31 |  766 | `		rc = SyHashInsertTail(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|       31 |  767 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  768 | `			goto cleanup;` |
|        - |  769 | `		}` |
|        5 |  770 | `	}` |
|        - |  771 | `	/* Copy constants from the trait (PHP 8.2 trait constants; separate hConst` |
|        - |  772 | `	 * namespace). A constant already present in the class wins silently. */` |
|      159 |  773 | `	SyHashResetLoopCursor(&pTrait->hConst);` |
|      159 |  774 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hConst)) != 0 ){` |
|      ! 0 |  775 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      ! 0 |  776 | `		pName = &pAttr->sName;` |
|      ! 0 |  777 | `		if( SyHashGet(&pClass->hConst,(const void *)pName->zString,pName->nByte) != 0 ){` |
|      ! 0 |  778 | `			continue;` |
|        - |  779 | `		}` |
|      ! 0 |  780 | `		rc = SyHashInsertTail(&pClass->hConst,(const void *)pName->zString,pName->nByte,pAttr);` |
|      ! 0 |  781 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  782 | `			goto cleanup;` |
|        - |  783 | `		}` |
|      ! 0 |  784 | `	}` |
|        - |  785 | `	/* Copy methods from the trait. The identity copied is the trait's HASH KEY,` |
|        - |  786 | ``	 * not sFunc.sName: an adaptation alias (`hi as bHi`) is a hash entry whose`` |
|        - |  787 | `	 * key is the alias while the method struct keeps its original name — keying` |
|        - |  788 | ``	 * the copy off sFunc.sName silently re-filed `bHi` under `hi`, so an alias`` |
|        - |  789 | `	 * made inside a TRAIT vanished when that trait was composed into a class. */` |
|      159 |  790 | `	SyHashResetLoopCursor(&pTrait->hMethod);` |
|      415 |  791 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hMethod)) != 0 ){` |
|        - |  792 | `		SyHashEntry *pClassMethEntry;` |
|        - |  793 | `		SyString sKey;` |
|      261 |  794 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      261 |  795 | `		SyStringInitFromBuf(&sKey,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|      261 |  796 | `		pName = &sKey;` |
|      261 |  797 | `		pClassMethEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
|      261 |  798 | `		if( pClassMethEntry != 0 ){` |
|        - |  799 | `			/* Method already exists in the class. An ABSTRACT trait method is a` |
|        - |  800 | `			 * REQUIREMENT, not an implementation: php satisfies it with any concrete` |
|        - |  801 | `			 * method of the same name (from the class body or another trait) — no` |
|        - |  802 | `			 * collision. Only two CONCRETE trait methods actually conflict. */` |
|       18 |  803 | `			ph7_class_method *pExistingMeth = (ph7_class_method *)pClassMethEntry->pUserData;` |
|       18 |  804 | `			int bIncomingAbstract = (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0;` |
|       18 |  805 | `			int bExistingAbstract = (pExistingMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0;` |
|        - |  806 | `			ph7_class **apUsedTraits;` |
|        - |  807 | `			sxu32 nUsed,k;` |
|       18 |  808 | `			if( bIncomingAbstract ){` |
|        - |  809 | `				/* Incoming abstract requirement: the existing (concrete or abstract)` |
|        - |  810 | `				 * method already covers this name — keep it. */` |
|       12 |  811 | `				continue;` |
|        - |  812 | `			}` |
|       11 |  813 | `			if( bExistingAbstract ){` |
|        - |  814 | `				/* Existing entry is only an abstract requirement (from an earlier` |
|        - |  815 | `				 * trait): the incoming concrete method satisfies and replaces it. */` |
|        3 |  816 | `				pClassMethEntry->pUserData = (void *)pMeth;` |
|        3 |  817 | `				continue;` |
|        - |  818 | `			}` |
|        - |  819 | `			/* Both concrete: a genuine collision only when the OTHER definition came` |
|        - |  820 | `			 * from another trait (a concrete one). A class-body method wins silently. */` |
|        8 |  821 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|        8 |  822 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|        8 |  823 | `			for(k = 0; k < nUsed; k++){` |
|        3 |  824 | `				ph7_class_method *pOtherMeth = PH7_ClassExtractMethod(apUsedTraits[k],pName->zString,pName->nByte);` |
|        3 |  825 | `				if( pOtherMeth != 0 && (pOtherMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|        - |  826 | `					/* Two different traits define the same CONCRETE method with no resolution */` |
|        4 |  827 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|        - |  828 | `						"Trait method %z::%z has not been applied as %z::%z, "` |
|        - |  829 | `						"because of collision with %z::%z",` |
|        2 |  830 | `						&pTrait->sName,pName,` |
|        1 |  831 | `						&pClass->sName,pName,` |
|        2 |  832 | `						&apUsedTraits[k]->sName,pName);` |
|        3 |  833 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  834 | `						goto cleanup;` |
|        - |  835 | `					}` |
|        3 |  836 | `					break;` |
|        - |  837 | `				}` |
|      ! 0 |  838 | `			}` |
|        - |  839 | `			/* Class-defined method takes precedence */` |
|        8 |  840 | `			continue;` |
|        - |  841 | `		}` |
|      247 |  842 | `		rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|      247 |  843 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  844 | `			goto cleanup;` |
|        - |  845 | `		}` |
|        5 |  846 | `	}` |
|        - |  847 | `	/* Record trait in the class */` |
|      159 |  848 | `	SySetPut(&pClass->aTrait,(const void *)&pTrait);` |
|       77 |  849 | `cleanup:` |
|        - |  850 | `	/* Always clear visiting flag, even on error paths */` |
|      159 |  851 | `	pTrait->iFlags &= ~PH7_CLASS_TRAIT_VISITING;` |
|       77 |  852 | `	SXUNUSED(pGen);` |
|      159 |  853 | `	return rc;` |
|       82 |  854 | `}` |
|        - |  855 | `/*` |
|        - |  856 | ` * Inherit an object interface from another object interface.` |
|        - |  857 | ` * According to the PHP language reference manual.` |
|        - |  858 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|        - |  859 | ` *  must implement, without having to define how these methods are handled.` |
|        - |  860 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|        - |  861 | ` *  class, but without any of the methods having their contents defined.` |
|        - |  862 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|        - |  863 | ` *` |
|        - |  864 | ` * This function return SXRET_OK if the interface inheritance operation was successfully performed.` |
|        - |  865 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|        - |  866 | ` * error message.` |
|        - |  867 | ` */` |
|    41188 |  868 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceInherit(ph7_class *pSub,ph7_class *pBase)` |
|        5 |  869 | `{` |
|        - |  870 | `	ph7_class_method *pMeth;` |
|        - |  871 | `	ph7_class_attr *pAttr;` |
|        - |  872 | `	SyHashEntry *pEntry;` |
|        - |  873 | `	SyString *pName;` |
|        - |  874 | `	sxi32 rc;` |
|        - |  875 | `	/* Install in the derived hashtable */` |
|    41193 |  876 | `	SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|    41193 |  877 | `	SyHashResetLoopCursor(&pBase->hConst);` |
|        - |  878 | `	/* Copy constants (interfaces carry only constants + method signatures) */` |
|    61789 |  879 | `	while((pEntry = SyHashGetNextEntry(&pBase->hConst)) != 0 ){` |
|        - |  880 | `		/* Make sure the constants are not redeclared in the subclass */` |
|        3 |  881 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|        3 |  882 | `		pName = &pAttr->sName;` |
|        3 |  883 | `		if( SyHashGet(&pSub->hConst,(const void *)pName->zString,pName->nByte) == 0 ){` |
|        - |  884 | `			/* Install the constant in the subclass */` |
|        3 |  885 | `			rc = SyHashInsertTail(&pSub->hConst,(const void *)pName->zString,pName->nByte,pAttr);` |
|        3 |  886 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  887 | `				return rc;` |
|        - |  888 | `			}` |
|        1 |  889 | `		}` |
|        1 |  890 | `	}` |
|    41193 |  891 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
|        - |  892 | `	/* Copy methods signature */` |
|   154471 |  893 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|        - |  894 | `		/* Make sure the method are not redeclared in the subclass */` |
|    92689 |  895 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|    92689 |  896 | `		pName = &pMeth->sFunc.sName;` |
|    92689 |  897 | `		if( SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte) == 0 ){` |
|        - |  898 | `			/* Install the method */` |
|    92689 |  899 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|    92689 |  900 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  901 | `				return rc;` |
|        - |  902 | `			}` |
|    46342 |  903 | `		}` |
|        5 |  904 | `	}` |
|        - |  905 | `	/* Mark as subclass */` |
|    41193 |  906 | `	pSub->pBase = pBase;` |
|        - |  907 | `	/* All done */` |
|    41193 |  908 | `	return SXRET_OK;` |
|    20599 |  909 | `}` |
|        - |  910 | `/*` |
|        - |  911 | ` * Implements an object interface in the given main class.` |
|        - |  912 | ` * According to the PHP language reference manual.` |
|        - |  913 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|        - |  914 | ` *  must implement, without having to define how these methods are handled.` |
|        - |  915 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|        - |  916 | ` *  class, but without any of the methods having their contents defined.` |
|        - |  917 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|        - |  918 | ` *` |
|        - |  919 | ` * This function return SXRET_OK if the interface was successfully implemented.` |
|        - |  920 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|        - |  921 | ` * error message.` |
|        - |  922 | ` */` |
|   304300 |  923 | `PH7_PRIVATE sxi32 PH7_ClassImplement(ph7_class *pMain,ph7_class *pInterface)` |
|        5 |  924 | `{` |
|        - |  925 | `	ph7_class_attr *pAttr;` |
|        - |  926 | `	SyHashEntry *pEntry;` |
|        - |  927 | `	SyString *pName;` |
|        - |  928 | `	sxi32 rc;` |
|        - |  929 | `	/* First off,copy all constants declared inside the interface (hConst namespace) */` |
|   304305 |  930 | `	SyHashResetLoopCursor(&pInterface->hConst);` |
|   600561 |  931 | `	while((pEntry = SyHashGetNextEntry(&pInterface->hConst)) != 0 ){` |
|        - |  932 | `		/* Point to the constant declaration */` |
|   144111 |  933 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   144111 |  934 | `		pName = &pAttr->sName;` |
|        - |  935 | `		/* Make sure the constant is not redeclared in the main class */` |
|   144111 |  936 | `		if( SyHashGet(&pMain->hConst,pName->zString,pName->nByte) == 0 ){` |
|        - |  937 | `			/* Install the constant */` |
|   144111 |  938 | `			rc = SyHashInsertTail(&pMain->hConst,pName->zString,pName->nByte,pAttr);` |
|   144111 |  939 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  940 | `				return rc;` |
|        - |  941 | `			}` |
|    72053 |  942 | `		}` |
|        5 |  943 | `	}` |
|        - |  944 | `	/* Install in the interface container */` |
|   304305 |  945 | `	SySetPut(&pMain->aInterface,(const void *)&pInterface);` |
|        - |  946 | `	/* Install interface method stubs into the implementing class.` |
|        - |  947 | `	 * Methods already defined in the class take precedence (they satisfy` |
|        - |  948 | `	 * the interface contract). Stubs retain PH7_CLASS_ATTR_ABSTRACT so` |
|        - |  949 | `	 * the unified check in GenStateCheckAbstractMethods catches missing ones.` |
|        - |  950 | `	 */` |
|        - |  951 | `	{` |
|        - |  952 | `		ph7_class_method *pMeth;` |
|        - |  953 | `		SyHashEntry *pMEntry;` |
|        - |  954 | `		SyString *pMName;` |
|   304305 |  955 | `		SyHashResetLoopCursor(&pInterface->hMethod);` |
|  1306653 |  956 | `		while((pMEntry = SyHashGetNextEntry(&pInterface->hMethod)) != 0 ){` |
|   850203 |  957 | `			pMeth = (ph7_class_method *)pMEntry->pUserData;` |
|   850203 |  958 | `			pMName = &pMeth->sFunc.sName;` |
|   850203 |  959 | `			if( SyHashGet(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte) == 0 ){` |
|     5171 |  960 | `				rc = SyHashInsert(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte,pMeth);` |
|     5171 |  961 | `				if( rc != SXRET_OK ){` |
|      ! 0 |  962 | `					return rc;` |
|        - |  963 | `				}` |
|     2583 |  964 | `			}` |
|        5 |  965 | `		}` |
|        - |  966 | `	}` |
|   304305 |  967 | `	return SXRET_OK;` |
|   152155 |  968 | `}` |
|        - |  969 | `/*` |
|        - |  970 | ` * Create a class instance [i.e: Object in the PHP jargon] at run-time.` |
|        - |  971 | ` * The following function is called when an object is created at run-time` |
|        - |  972 | ` * typically when the PH7_OP_NEW/PH7_OP_CLONE instructions are executed.` |
|        - |  973 | ` * Notes on object creation.` |
|        - |  974 | ` *` |
|        - |  975 | ` * According to PHP language reference manual.` |
|        - |  976 | ` * To create an instance of a class, the new keyword must be used. An object will always` |
|        - |  977 | ` * be created unless the object has a constructor defined that throws an exception on error.` |
|        - |  978 | ` * Classes should be defined before instantiation (and in some cases this is a requirement).` |
|        - |  979 | ` * If a string containing the name of a class is used with new, a new instance of that class` |
|        - |  980 | ` * will be created. If the class is in a namespace, its fully qualified name must be used when` |
|        - |  981 | ` * doing this.` |
|        - |  982 | ` * Example #3 Creating an instance` |
|        - |  983 | ` * <?php` |
|        - |  984 | ` *  $instance = new SimpleClass();` |
|        - |  985 | ` *   // This can also be done with a variable:` |
|        - |  986 | ` * $className = 'Foo';` |
|        - |  987 | ` * $instance = new $className(); // Foo()` |
|        - |  988 | ` * ?>` |
|        - |  989 | ` * In the class context, it is possible to create a new object by new self and new parent.` |
|        - |  990 | ` * When assigning an already created instance of a class to a new variable, the new variable` |
|        - |  991 | ` * will access the same instance as the object that was assigned. This behaviour is the same` |
|        - |  992 | ` * when passing instances to a function. A copy of an already created object can be made by` |
|        - |  993 | ` * cloning it.` |
|        - |  994 | ` * Example #4 Object Assignment` |
|        - |  995 | ` * <?php` |
|        - |  996 | ` *  class SimpleClass(){` |
|        - |  997 | ` *    public $var;` |
|        - |  998 | ` *  };` |
|        - |  999 | ` *  $instance = new SimpleClass();` |
|        - | 1000 | ` *  $assigned   =  $instance;` |
|        - | 1001 | ` *  $reference  =& $instance;` |
|        - | 1002 | ` *  $instance->var = '$assigned will have this value';` |
|        - | 1003 | ` *  $instance = null; // $instance and $reference become null` |
|        - | 1004 | ` *  var_dump($instance);` |
|        - | 1005 | ` *  var_dump($reference);` |
|        - | 1006 | ` *  var_dump($assigned);` |
|        - | 1007 | ` * ?>` |
|        - | 1008 | ` * The above example will output:` |
|        - | 1009 | ` * NULL` |
|        - | 1010 | ` * NULL` |
|        - | 1011 | ` * object(SimpleClass)#1 (1) {` |
|        - | 1012 | ` *  ["var"]=>` |
|        - | 1013 | ` *    string(30) "$assigned will have this value"` |
|        - | 1014 | ` * }` |
|        - | 1015 | ` * Example #5 Creating new objects` |
|        - | 1016 | ` * <?php` |
|        - | 1017 | ` * class Test` |
|        - | 1018 | ` * {` |
|        - | 1019 | ` *   static public function getNew()` |
|        - | 1020 | ` *   {` |
|        - | 1021 | ` *       return new static;` |
|        - | 1022 | ` *   }` |
|        - | 1023 | ` * }` |
|        - | 1024 | ` * class Child extends Test` |
|        - | 1025 | ` * {}` |
|        - | 1026 | ` * $obj1 = new Test();` |
|        - | 1027 | ` * $obj2 = new $obj1;` |
|        - | 1028 | ` * var_dump($obj1 !== $obj2);` |
|        - | 1029 | ` * $obj3 = Test::getNew();` |
|        - | 1030 | ` * var_dump($obj3 instanceof Test);` |
|        - | 1031 | ` * $obj4 = Child::getNew();` |
|        - | 1032 | ` * var_dump($obj4 instanceof Child);` |
|        - | 1033 | ` * ?>` |
|        - | 1034 | ` * The above example will output:` |
|        - | 1035 | ` * bool(true)` |
|        - | 1036 | ` * bool(true)` |
|        - | 1037 | ` * bool(true)` |
|        - | 1038 | ` * Note that Symisc Systems have introduced powerfull extension to` |
|        - | 1039 | ` * OO subsystem. For example a class attribute may have any complex` |
|        - | 1040 | ` * expression associated with it when declaring the attribute unlike` |
|        - | 1041 | ` * the standard PHP engine which would allow a single value.` |
|        - | 1042 | ` * Example:` |
|        - | 1043 | ` *  class myClass{` |
|        - | 1044 | ` *    public $var = 25<<1+foo()/bar();` |
|        - | 1045 | ` *  };` |
|        - | 1046 | ` * Refer to the official documentation for more information.` |
|        - | 1047 | ` */` |
|  1577106 | 1048 | `static ph7_class_instance * NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|        5 | 1049 | `{` |
|        - | 1050 | `	ph7_class_instance *pThis;` |
|        - | 1051 | `	/* Allocate a new instance */` |
|  1577111 | 1052 | `	pThis = (ph7_class_instance *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_instance));` |
|  1577111 | 1053 | `	if( pThis == 0 ){` |
|      ! 0 | 1054 | `		return 0;` |
|        - | 1055 | `	}` |
|        - | 1056 | `	/* Zero the structure */` |
|  1577111 | 1057 | `	SyZero(pThis,sizeof(ph7_class_instance));` |
|        - | 1058 | `	/* Initialize fields */` |
|  1577111 | 1059 | `	pThis->iRef = 1;` |
|  1577111 | 1060 | `	pThis->pVm = pVm;` |
|  1577111 | 1061 | `	pThis->pClass = pClass;` |
|        - | 1062 | `	/* Assign a fresh monotonic object handle id (clones get their own, like PHP). */` |
|  1577111 | 1063 | `	pThis->nObjId = pVm->nNextObjId++;` |
|  1577111 | 1064 | `	SyHashInit(&pThis->hAttr,&pVm->sAllocator,0,0);` |
|  1577111 | 1065 | `	return pThis;` |
|   788558 | 1066 | `}` |
|        - | 1067 | `/*` |
|        - | 1068 | ` * Wrapper around the NewClassInstance() function defined above.` |
|        - | 1069 | ` * See the block comment above for more information.` |
|        - | 1070 | ` */` |
|  1576610 | 1071 | `PH7_PRIVATE ph7_class_instance * PH7_NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|        5 | 1072 | `{` |
|        - | 1073 | `	ph7_class_instance *pNew;` |
|        - | 1074 | `	sxi32 rc;` |
|  1576615 | 1075 | `	pNew = NewClassInstance(&(*pVm),&(*pClass));` |
|  1576615 | 1076 | `	if( pNew == 0 ){` |
|      ! 0 | 1077 | `		return 0;` |
|        - | 1078 | `	}` |
|        - | 1079 | `	/* Associate a private VM frame with this class instance */` |
|  1576615 | 1080 | `	rc = PH7_VmCreateClassInstanceFrame(&(*pVm),pNew);` |
|  1576615 | 1081 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1082 | `		SyMemBackendPoolFree(&pVm->sAllocator,pNew);` |
|      ! 0 | 1083 | `		return 0;` |
|        - | 1084 | `	}` |
|        - | 1085 | `	/* php stamps a Throwable's file/line at CREATION, not in its constructor, so a` |
|        - | 1086 | `	 * subclass that overrides __construct without calling parent::__construct still` |
|        - | 1087 | `	 * reports the right site. Every instantiation path lands here. */` |
|  1576615 | 1088 | `	PH7_VmStampThrowableSite(&(*pVm),pNew);` |
|  1576615 | 1089 | `	return pNew;` |
|   788310 | 1090 | `}` |
|        - | 1091 | `/*` |
|        - | 1092 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon] attribute.` |
|        - | 1093 | ` * This function never fail.` |
|        - | 1094 | ` */` |
|  7412054 | 1095 | `static ph7_value * ExtractClassAttrValue(ph7_vm *pVm,VmClassAttr *pAttr)` |
|        5 | 1096 | `{` |
|        - | 1097 | `	/* Extract the value */` |
|        - | 1098 | `	ph7_value *pValue;` |
|  7412059 | 1099 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|  7412059 | 1100 | `	return pValue;` |
|        5 | 1101 | `}` |
|        - | 1102 | `/*` |
|        - | 1103 | ` * Perform a clone operation on a class instance [i.e: Object in the PHP jargon].` |
|        - | 1104 | ` * The following function is called when an object is cloned at run-time` |
|        - | 1105 | ` * typically when the PH7_OP_CLONE instruction is executed.` |
|        - | 1106 | ` * Notes on object cloning.` |
|        - | 1107 | ` *` |
|        - | 1108 | ` * According to PHP language reference manual.` |
|        - | 1109 | ` * Creating a copy of an object with fully replicated properties is not always the wanted behavior.` |
|        - | 1110 | ` * A good example of the need for copy constructors. Another example is if your object holds a reference` |
|        - | 1111 | ` * to another object which it uses and when you replicate the parent object you want to create` |
|        - | 1112 | ` * a new instance of this other object so that the replica has its own separate copy.` |
|        - | 1113 | ` * An object copy is created by using the clone keyword (which calls the object's __clone() method if possible).` |
|        - | 1114 | ` * An object's __clone() method cannot be called directly.` |
|        - | 1115 | ` * $copy_of_object = clone $object;` |
|        - | 1116 | ` * When an object is cloned, PHP 5 will perform a shallow copy of all of the object's properties.` |
|        - | 1117 | ` * Any properties that are references to other variables, will remain references.` |
|        - | 1118 | ` * Once the cloning is complete, if a __clone() method is defined, then the newly created object's __clone() method` |
|        - | 1119 | ` * will be called, to allow any necessary properties that need to be changed.` |
|        - | 1120 | ` * Example #1 Cloning an object` |
|        - | 1121 | ` * <?php` |
|        - | 1122 | ` * class SubObject` |
|        - | 1123 | ` * {` |
|        - | 1124 | ` *   static $instances = 0;` |
|        - | 1125 | ` *   public $instance;` |
|        - | 1126 | ` *` |
|        - | 1127 | ` *   public function __construct() {` |
|        - | 1128 | ` *       $this->instance = ++self::$instances;` |
|        - | 1129 | ` *   }` |
|        - | 1130 | ` *` |
|        - | 1131 | ` *   public function __clone() {` |
|        - | 1132 | ` *       $this->instance = ++self::$instances;` |
|        - | 1133 | ` *   }` |
|        - | 1134 | ` * }` |
|        - | 1135 | ` *` |
|        - | 1136 | ` * class MyCloneable` |
|        - | 1137 | ` * {` |
|        - | 1138 | ` *   public $object1;` |
|        - | 1139 | ` *   public $object2;` |
|        - | 1140 | ` *` |
|        - | 1141 | ` *   function __clone()` |
|        - | 1142 | ` *   {` |
|        - | 1143 | ` *       // Force a copy of this->object, otherwise` |
|        - | 1144 | ` *       // it will point to same object.` |
|        - | 1145 | ` *       $this->object1 = clone $this->object1;` |
|        - | 1146 | ` *   }` |
|        - | 1147 | ` * }` |
|        - | 1148 | ` * $obj = new MyCloneable();` |
|        - | 1149 | ` * $obj->object1 = new SubObject();` |
|        - | 1150 | ` * $obj->object2 = new SubObject();` |
|        - | 1151 | ` * $obj2 = clone $obj;` |
|        - | 1152 | ` * print("Original Object:\n");` |
|        - | 1153 | ` * print_r($obj);` |
|        - | 1154 | ` * print("Cloned Object:\n");` |
|        - | 1155 | ` * print_r($obj2);` |
|        - | 1156 | ` * ?>` |
|        - | 1157 | ` * The above example will output:` |
|        - | 1158 | ` * Original Object:` |
|        - | 1159 | ` * MyCloneable Object` |
|        - | 1160 | ` * (` |
|        - | 1161 | ` *   [object1] => SubObject Object` |
|        - | 1162 | ` *       (` |
|        - | 1163 | ` *           [instance] => 1` |
|        - | 1164 | ` *       )` |
|        - | 1165 | ` *` |
|        - | 1166 | ` *   [object2] => SubObject Object` |
|        - | 1167 | ` *       (` |
|        - | 1168 | ` *           [instance] => 2` |
|        - | 1169 | ` *       )` |
|        - | 1170 | ` *` |
|        - | 1171 | ` * )` |
|        - | 1172 | ` * Cloned Object:` |
|        - | 1173 | ` * MyCloneable Object` |
|        - | 1174 | ` * (` |
|        - | 1175 | ` *   [object1] => SubObject Object` |
|        - | 1176 | ` *       (` |
|        - | 1177 | ` *           [instance] => 3` |
|        - | 1178 | ` *       )` |
|        - | 1179 | ` *` |
|        - | 1180 | ` *   [object2] => SubObject Object` |
|        - | 1181 | ` *       (` |
|        - | 1182 | ` *           [instance] => 2` |
|        - | 1183 | ` *       )` |
|        - | 1184 | ` * )` |
|        - | 1185 | ` */` |
|      496 | 1186 | `PH7_PRIVATE ph7_class_instance * PH7_CloneClassInstance(ph7_class_instance *pSrc)` |
|        5 | 1187 | `{` |
|        - | 1188 | `	ph7_class_instance *pClone;` |
|        - | 1189 | `	ph7_class_method *pMethod;` |
|        - | 1190 | `	SyHashEntry *pEntry2;` |
|        - | 1191 | `	SyHashEntry *pEntry;` |
|        - | 1192 | `	ph7_vm *pVm;` |
|        - | 1193 | `	sxi32 rc;` |
|        - | 1194 | `	/* Allocate a new instance */` |
|      501 | 1195 | `	pVm = pSrc->pVm;` |
|      501 | 1196 | `	pClone = NewClassInstance(pVm,pSrc->pClass);` |
|      501 | 1197 | `	if( pClone == 0 ){` |
|      ! 0 | 1198 | `		return 0;` |
|        - | 1199 | `	}` |
|        - | 1200 | `	/* Associate a private VM frame with this class instance */` |
|      501 | 1201 | `	rc = PH7_VmCreateClassInstanceFrame(pVm,pClone);` |
|      501 | 1202 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1203 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClone);` |
|      ! 0 | 1204 | `		return 0;` |
|        - | 1205 | `	}` |
|        - | 1206 | `	/* Duplicate object values. Iterate the SOURCE attributes and copy each into` |
|        - | 1207 | `	 * the clone's same-named slot (looked up by name, so order/count differences` |
|        - | 1208 | `	 * from dynamic properties don't matter). A dynamic (runtime-added) property` |
|        - | 1209 | `	 * has no declared counterpart in the clone, so synthesize it first — without` |
|        - | 1210 | `	 * this, a clone of a stdClass would silently lose all its dynamic properties. */` |
|      501 | 1211 | `	SyHashResetLoopCursor(&pSrc->hAttr);` |
|     1965 | 1212 | `	while((pEntry = SyHashGetNextEntry(&pSrc->hAttr)) != 0 ){` |
|     1469 | 1213 | `		VmClassAttr *pSrcAttr = (VmClassAttr *)pEntry->pUserData;` |
|     1469 | 1214 | `		VmClassAttr *pDestAttr = 0;` |
|     1469 | 1215 | `		ph7_value *pvSrc,*pvDest = 0;` |
|        - | 1216 | `		/* Duplicate non-static attribute */` |
|     1469 | 1217 | `		if( pSrcAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|        6 | 1218 | `			continue;` |
|        - | 1219 | `		}` |
|     1465 | 1220 | `		pEntry2 = SyHashGet(&pClone->hAttr,SyStringData(&pSrcAttr->pAttr->sName),SyStringLength(&pSrcAttr->pAttr->sName));` |
|     1465 | 1221 | `		if( pEntry2 ){` |
|     1443 | 1222 | `			pDestAttr = (VmClassAttr *)pEntry2->pUserData;` |
|     1443 | 1223 | `			pvDest = ExtractClassAttrValue(pVm,pDestAttr);` |
|      742 | 1224 | `		}else if( pSrcAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){` |
|        - | 1225 | `			/* Dynamic property: synthesize the matching slot on the clone. */` |
|       34 | 1226 | `			pvDest = PH7_VmCreateDynamicAttr(pVm,pClone,` |
|       22 | 1227 | `				SyStringData(&pSrcAttr->pAttr->sName),SyStringLength(&pSrcAttr->pAttr->sName),&pDestAttr);` |
|       11 | 1228 | `		}` |
|        - | 1229 | `		/* Fetch the source value LAST: PH7_VmCreateDynamicAttr above may have` |
|        - | 1230 | `		 * reserved a slot and reallocated pVm->aMemObj, which would dangle any` |
|        - | 1231 | `		 * ph7_value* obtained before it. pvDest from the synth path already points` |
|        - | 1232 | `		 * into the post-realloc aMemObj; resolve pvSrc now so both are current. */` |
|     1465 | 1233 | `		pvSrc = ExtractClassAttrValue(pVm,pSrcAttr);` |
|     1465 | 1234 | `		if( (pSrcAttr->iState & VM_CLASS_ATTR_REFBOUND) && pDestAttr ){` |
|        - | 1235 | `			/* php preserves references across clone: the clone shares the SAME slot` |
|        - | 1236 | `			 * as the source property (both alias the referenced variable), rather` |
|        - | 1237 | `			 * than getting an independent value copy. Drop the clone's fresh private` |
|        - | 1238 | `			 * slot and repoint at the (already-pinned) shared slot. The REFBOUND flag` |
|        - | 1239 | `			 * is carried over by the iState copy below, so the clone's release also` |
|        - | 1240 | `			 * leaves the shared slot alone. */` |
|        5 | 1241 | `			if( pDestAttr->nIdx != pSrcAttr->nIdx ){` |
|        5 | 1242 | `				if( pDestAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|      ! 0 | 1243 | `					SyHashDeleteEntry(&pVm->hTypedSlot,(const void *)&pDestAttr->nIdx,sizeof(sxu32),0);` |
|      ! 0 | 1244 | `				}` |
|        5 | 1245 | `				PH7_VmUnsetMemObj(pVm,pDestAttr->nIdx,TRUE);` |
|        5 | 1246 | `				pDestAttr->nIdx = pSrcAttr->nIdx;` |
|        - | 1247 | `				/* The clone is a holder of the shared slot in its own right — take a pin` |
|        - | 1248 | `				 * for it, since its own release will give one back. */` |
|        5 | 1249 | `				VmPinMemObjSlotCounted(pVm,pDestAttr->nIdx);` |
|        3 | 1250 | `			}` |
|     1463 | 1251 | `		}else if( pvSrc && pvDest ){` |
|     1461 | 1252 | `			PH7_MemObjStore(pvSrc,pvDest);` |
|      728 | 1253 | `		}` |
|        - | 1254 | `		/* Carry over the per-instance state so the clone matches the source:` |
|        - | 1255 | `		 * VM_CLASS_ATTR_UNINIT marks a typed property as not-yet-initialized` |
|        - | 1256 | `		 * and doubles as the readonly write-once latch — without this a clone` |
|        - | 1257 | `		 * would reset to uninitialized (losing the value's readiness) and a` |
|        - | 1258 | `		 * readonly property would become writable again. */` |
|     1465 | 1259 | `		if( pDestAttr ){` |
|     1465 | 1260 | `			pDestAttr->iState = pSrcAttr->iState;` |
|      730 | 1261 | `		}` |
|        5 | 1262 | `	}` |
|        - | 1263 | `	/* A declared property unset() on the source is absent from the clone too (PHP). But the clone` |
|        - | 1264 | `	 * frame above materialized ALL declared attrs (with their defaults), so drop any clone attr whose` |
|        - | 1265 | `	 * name is not present on the source. Collect first, then delete — removing an entry mid-walk would` |
|        - | 1266 | `	 * free the node the SyHash loop cursor points at. */` |
|        - | 1267 | `	{` |
|        - | 1268 | `		SySet sDrop;` |
|      501 | 1269 | `		SySetInit(&sDrop,&pVm->sAllocator,sizeof(VmClassAttr *));` |
|      501 | 1270 | `		SyHashResetLoopCursor(&pClone->hAttr);` |
|     1967 | 1271 | `		while((pEntry = SyHashGetNextEntry(&pClone->hAttr)) != 0 ){` |
|     1471 | 1272 | `			VmClassAttr *pCloneAttr = (VmClassAttr *)pEntry->pUserData;` |
|     1471 | 1273 | `			if( pCloneAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|        6 | 1274 | `				continue;` |
|        - | 1275 | `			}` |
|     2193 | 1276 | `			if( SyHashGet(&pSrc->hAttr,SyStringData(&pCloneAttr->pAttr->sName),` |
|     2198 | 1277 | `					SyStringLength(&pCloneAttr->pAttr->sName)) == 0 ){` |
|        3 | 1278 | `				SySetPut(&sDrop,(const void *)&pCloneAttr);` |
|        1 | 1279 | `			}` |
|        5 | 1280 | `		}` |
|      501 | 1281 | `		if( SySetUsed(&sDrop) > 0 ){` |
|        3 | 1282 | `			VmClassAttr **apDrop = (VmClassAttr **)SySetBasePtr(&sDrop);` |
|        - | 1283 | `			sxu32 i;` |
|        5 | 1284 | `			for( i = 0 ; i < SySetUsed(&sDrop) ; ++i ){` |
|        3 | 1285 | `				VmClassAttr *pVmAttr = apDrop[i];` |
|        4 | 1286 | `				SyHashDeleteEntry(&pClone->hAttr,SyStringData(&pVmAttr->pAttr->sName),` |
|        2 | 1287 | `					SyStringLength(&pVmAttr->pAttr->sName),0);` |
|        3 | 1288 | `				PH7_VmReleaseInstanceAttr(pVm,pVmAttr);` |
|        2 | 1289 | `			}` |
|        1 | 1290 | `		}` |
|      501 | 1291 | `		SySetRelease(&sDrop);` |
|        - | 1292 | `	}` |
|        - | 1293 | `	/* call the __clone method on the cloned object if available */` |
|      501 | 1294 | `	pMethod = PH7_ClassExtractMethod(pClone->pClass,"__clone",sizeof("__clone")-1);` |
|      501 | 1295 | `	if( pMethod ){` |
|      101 | 1296 | `		if( pMethod->iCloneDepth < 16 ){` |
|       99 | 1297 | `			pMethod->iCloneDepth++;` |
|        - | 1298 | `			/* PHP 8.3: __clone() may re-initialize the clone's readonly` |
|        - | 1299 | `			 * properties. Flag the instance so the readonly store guard allows` |
|        - | 1300 | `			 * it for the duration of the call. */` |
|       99 | 1301 | `			pClone->iFlags \|= VM_INSTANCE_CLONING;` |
|       99 | 1302 | `			PH7_VmCallClassMethod(pVm,pClone,pMethod,0,0,0);` |
|       99 | 1303 | `			pClone->iFlags &= ~VM_INSTANCE_CLONING;` |
|       51 | 1304 | `		}else{` |
|        - | 1305 | `			/* Nesting limit reached */` |
|        3 | 1306 | `			PH7_VmThrowError(pVm,0,PH7_CTX_ERR,"Object clone limit reached,no more call to __clone()");` |
|        - | 1307 | `		}` |
|        - | 1308 | `		/* Reset the cursor */` |
|      101 | 1309 | `		pMethod->iCloneDepth = 0;` |
|       49 | 1310 | `	}` |
|        - | 1311 | `	/* Return the cloned object */` |
|      501 | 1312 | `	return pClone;` |
|      253 | 1313 | `}` |
|        - | 1314 | `#define CLASS_INSTANCE_DESTROYED 0x001 /* Instance is released */` |
|        - | 1315 | `/*` |
|        - | 1316 | ` * Free the per-instance allocations owned by ONE object attribute: its value slot (+ the typed-slot` |
|        - | 1317 | ` * enforcement entry), the synthesized ph7_class_attr for a dynamic (runtime-added) property, and the` |
|        - | 1318 | ` * VmClassAttr wrapper itself. Does NOT touch the hAttr entry node — the caller removes it` |
|        - | 1319 | `` * (`unset($o->p)` via SyHashDeleteEntry2; instance teardown via the wholesale SyHashRelease, so it must`` |
|        - | 1320 | ` * not delete entries mid-walk). Shared by PH7_ClassInstanceRelease and the OP_MEMBER unset path.` |
|        - | 1321 | ` */` |
|  9511486 | 1322 | `PH7_PRIVATE void PH7_VmReleaseInstanceAttr(ph7_vm *pVm, VmClassAttr *pVmAttr)` |
|        5 | 1323 | `{` |
|  9511491 | 1324 | `	if( pVmAttr->iState & VM_CLASS_ATTR_REFBOUND ){` |
|        - | 1325 | ``		/* Reference-bound property (`$o->p =& $x`): its value slot is SHARED, so it must`` |
|        - | 1326 | `		 * not be released here — but the property WAS one of its holders, so give the pin` |
|        - | 1327 | `		 * back. The slot (and its value, which used to stay alive for the rest of the` |
|        - | 1328 | `		 * script) goes if the property was the last thing holding it. */` |
|       27 | 1329 | `		VmUnpinMemObjSlot(pVm,pVmAttr->nIdx);` |
|  9511478 | 1330 | `	}else if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - | 1331 | `		/* Drop any typed-property enforcement slot registered for this memobj, before the memobj` |
|        - | 1332 | `		 * is returned to the free list, so a future recycled slot does not inherit the stale entry. */` |
|  9511369 | 1333 | `		if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|  6781985 | 1334 | `			SyHashDeleteEntry(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),0);` |
|  3390990 | 1335 | `		}` |
|  9511369 | 1336 | `		PH7_VmUnsetMemObj(pVm,pVmAttr->nIdx,TRUE);` |
|  4755682 | 1337 | `	}` |
|        - | 1338 | `	/* A dynamic property owns its synthesized ph7_class_attr (struct + inline name in one block) —` |
|        - | 1339 | `	 * free it here (the only place a per-instance pAttr is freed; declared attrs are class-owned). */` |
|  9511491 | 1340 | `	if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){` |
|      260 | 1341 | `		SyMemBackendFree(&pVm->sAllocator,pVmAttr->pAttr);` |
|      128 | 1342 | `	}` |
|  9511491 | 1343 | `	SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|  9511491 | 1344 | `}` |
|        - | 1345 | `/*` |
|        - | 1346 | ` * Release a class instance [i.e: Object in the PHP jargon] and invoke any defined destructor.` |
|        - | 1347 | ` * This routine is invoked as soon as there are no other references to a particular` |
|        - | 1348 | ` * class instance.` |
|        - | 1349 | ` */` |
|  1465506 | 1350 | `static void PH7_ClassInstanceRelease(ph7_class_instance *pThis)` |
|        5 | 1351 | `{` |
|        - | 1352 | `	ph7_class_method *pDestr;` |
|        - | 1353 | `	SyHashEntry *pEntry;` |
|        - | 1354 | `	ph7_class *pClass;` |
|        - | 1355 | `	ph7_vm *pVm;` |
|  1465511 | 1356 | `	if( pThis->iFlags & CLASS_INSTANCE_DESTROYED ){` |
|        - | 1357 | `		/*` |
|        - | 1358 | `		 * Already destroyed,return immediately.` |
|        - | 1359 | `		 * This could happend if someone perform unset($this) in the destructor body.` |
|        - | 1360 | `		 */` |
|      ! 0 | 1361 | `		return;` |
|        - | 1362 | `	}` |
|        - | 1363 | `	/* Mark as destroyed */` |
|  1465511 | 1364 | `	pThis->iFlags \|= CLASS_INSTANCE_DESTROYED;` |
|        - | 1365 | `	/* Invoke any defined destructor if available */` |
|  1465511 | 1366 | `	pVm = pThis->pVm;` |
|  1465511 | 1367 | `	pClass = pThis->pClass;` |
|  1465511 | 1368 | `	pDestr = PH7_ClassExtractMethod(pClass,"__destruct",sizeof("__destruct")-1);` |
|  1465511 | 1369 | `	if( pDestr && !pVm->bInReset ){` |
|        - | 1370 | `		/* Invoke the destructor. Skipped during ph7_vm_reset() bulk teardown:` |
|        - | 1371 | `		 * running user PHP against a half-reset VM is unsafe (see bInReset). */` |
|      557 | 1372 | `		pThis->iRef = 2; /* Prevent garbage collection */` |
|      557 | 1373 | `		PH7_VmCallClassMethod(pVm,pThis,pDestr,0,0,0);` |
|      276 | 1374 | `	}` |
|        - | 1375 | `	/* A native class's own teardown, while its slots are still readable. Not a` |
|        - | 1376 | `	 * __destruct: the classes that need this (WeakReference) declare none in php,` |
|        - | 1377 | `	 * and Reflection must not grow one. */` |
|  1465511 | 1378 | `	if( pClass->xRelease ){` |
|      104 | 1379 | `		pClass->xRelease(pVm,pThis);` |
|       51 | 1380 | `	}` |
|        - | 1381 | `	/* Weak-reference registry: kill the cell for this instance so every` |
|        - | 1382 | `	 * WeakReference/WeakMap handle observes the death (the cell outlives the` |
|        - | 1383 | `	 * instance until its own handles drop; removing the hash entry here keeps` |
|        - | 1384 | `	 * a pool-reused address from resurrecting a dead cell). */` |
|  1465511 | 1385 | `	if( SyHashTotalEntry(&pVm->hWeakCell) > 0 ){` |
|     5124 | 1386 | `		void *pCellData = 0;` |
|     5122 | 1387 | `		if( SyHashDeleteEntry(&pVm->hWeakCell,(const void *)&pThis,sizeof(void *),&pCellData) == SXRET_OK` |
|     2577 | 1388 | `		 && pCellData ){` |
|       30 | 1389 | `			((VmWeakCell *)pCellData)->pObj = 0;` |
|       14 | 1390 | `		}` |
|     2561 | 1391 | `	}` |
|        - | 1392 | `	/* Release non-static attributes (the wholesale SyHashRelease below frees the entry nodes,` |
|        - | 1393 | `	 * so the helper must not delete them mid-walk). */` |
|  1465511 | 1394 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
| 10976957 | 1395 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|  9511451 | 1396 | `		PH7_VmReleaseInstanceAttr(pVm,(VmClassAttr *)pEntry->pUserData);` |
|        5 | 1397 | `	}` |
|        - | 1398 | `	/* Release the whole structure */` |
|  1465511 | 1399 | `	SyHashRelease(&pThis->hAttr);` |
|  1465511 | 1400 | `	SyMemBackendPoolFree(&pVm->sAllocator,pThis);` |
|   732758 | 1401 | `}` |
|        - | 1402 | `/*` |
|        - | 1403 | ` * Decrement the reference count of a class instance [i.e Object in the PHP jargon].` |
|        - | 1404 | ` * If the reference count reaches zero,release the whole instance.` |
|        - | 1405 | ` */` |
|  7366300 | 1406 | `PH7_PRIVATE void PH7_ClassInstanceUnref(ph7_class_instance *pThis)` |
|        5 | 1407 | `{` |
|  7366305 | 1408 | `	pThis->iRef--;` |
|  7366305 | 1409 | `	if( pThis->iRef < 1 ){` |
|        - | 1410 | `		/* No more reference to this instance */` |
|  1465511 | 1411 | `		PH7_ClassInstanceRelease(&(*pThis));` |
|   732753 | 1412 | `	}` |
|  7366305 | 1413 | `}` |
|        - | 1414 | `/*` |
|        - | 1415 | ` * Compare two class instances [i.e: Objects in the PHP jargon]` |
|        - | 1416 | ` * Note on objects comparison:` |
|        - | 1417 | ` *  According to the PHP langauge reference manual` |
|        - | 1418 | ` *  When using the comparison operator (==), object variables are compared in a simple manner` |
|        - | 1419 | ` *  namely: Two object instances are equal if they have the same attributes and values, and are` |
|        - | 1420 | ` *  instances of the same class.` |
|        - | 1421 | ` *  On the other hand, when using the identity operator (===), object variables are identical` |
|        - | 1422 | ` *  if and only if they refer to the same instance of the same class.` |
|        - | 1423 | ` *  An example will clarify these rules.` |
|        - | 1424 | ` *  Example #1 Example of object comparison` |
|        - | 1425 | ` *  <?php` |
|        - | 1426 | ` *    function bool2str($bool)` |
|        - | 1427 | ` * {` |
|        - | 1428 | ` *   if ($bool === false) {` |
|        - | 1429 | ` *       return 'FALSE';` |
|        - | 1430 | ` *   } else {` |
|        - | 1431 | ` *       return 'TRUE';` |
|        - | 1432 | ` *   }` |
|        - | 1433 | ` * }` |
|        - | 1434 | ` * function compareObjects(&$o1, &$o2)` |
|        - | 1435 | ` * {` |
|        - | 1436 | ` *   echo 'o1 == o2 : ' . bool2str($o1 == $o2) . "\n";` |
|        - | 1437 | ` *   echo 'o1 != o2 : ' . bool2str($o1 != $o2) . "\n";` |
|        - | 1438 | ` *   echo 'o1 === o2 : ' . bool2str($o1 === $o2) . "\n";` |
|        - | 1439 | ` *   echo 'o1 !== o2 : ' . bool2str($o1 !== $o2) . "\n";` |
|        - | 1440 | ` * }` |
|        - | 1441 | ` * class Flag` |
|        - | 1442 | ` * {` |
|        - | 1443 | ` *   public $flag;` |
|        - | 1444 | ` *` |
|        - | 1445 | ` *   function Flag($flag = true) {` |
|        - | 1446 | ` *       $this->flag = $flag;` |
|        - | 1447 | ` *   }` |
|        - | 1448 | ` * }` |
|        - | 1449 | ` *` |
|        - | 1450 | ` * class OtherFlag` |
|        - | 1451 | ` * {` |
|        - | 1452 | ` *   public $flag;` |
|        - | 1453 | ` *` |
|        - | 1454 | ` *   function OtherFlag($flag = true) {` |
|        - | 1455 | ` *       $this->flag = $flag;` |
|        - | 1456 | ` *   }` |
|        - | 1457 | ` * }` |
|        - | 1458 | ` *` |
|        - | 1459 | ` * $o = new Flag();` |
|        - | 1460 | ` * $p = new Flag();` |
|        - | 1461 | ` * $q = $o;` |
|        - | 1462 | ` * $r = new OtherFlag();` |
|        - | 1463 | ` *` |
|        - | 1464 | ` * echo "Two instances of the same class\n";` |
|        - | 1465 | ` * compareObjects($o, $p);` |
|        - | 1466 | ` * echo "\nTwo references to the same instance\n";` |
|        - | 1467 | ` * compareObjects($o, $q);` |
|        - | 1468 | ` * echo "\nInstances of two different classes\n";` |
|        - | 1469 | ` * compareObjects($o, $r);` |
|        - | 1470 | ` * ?>` |
|        - | 1471 | ` * The above example will output:` |
|        - | 1472 | ` * Two instances of the same class` |
|        - | 1473 | ` * o1 == o2 : TRUE` |
|        - | 1474 | ` * o1 != o2 : FALSE` |
|        - | 1475 | ` * o1 === o2 : FALSE` |
|        - | 1476 | ` * o1 !== o2 : TRUE` |
|        - | 1477 | ` * Two references to the same instance` |
|        - | 1478 | ` * o1 == o2 : TRUE` |
|        - | 1479 | ` * o1 != o2 : FALSE` |
|        - | 1480 | ` * o1 === o2 : TRUE` |
|        - | 1481 | ` * o1 !== o2 : FALSE` |
|        - | 1482 | ` * Instances of two different classes` |
|        - | 1483 | ` * o1 == o2 : FALSE` |
|        - | 1484 | ` * o1 != o2 : TRUE` |
|        - | 1485 | ` * o1 === o2 : FALSE` |
|        - | 1486 | ` * o1 !== o2 : TRUE` |
|        - | 1487 | ` *` |
|        - | 1488 | ` * This function return 0 if the objects are equals according to the comprison rules defined above.` |
|        - | 1489 | ` * Any other return values indicates difference.` |
|        - | 1490 | ` */` |
|      392 | 1491 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest)` |
|        5 | 1492 | `{` |
|        - | 1493 | `	SyHashEntry *pEntry,*pEntry2;` |
|        - | 1494 | `	ph7_value sV1,sV2;` |
|        - | 1495 | `	sxi32 rc;` |
|      397 | 1496 | `	if( iNest > 31 ){` |
|        - | 1497 | `		/* Nesting limit reached */` |
|        6 | 1498 | `		PH7_VmThrowError(pLeft->pVm,0,PH7_CTX_ERR,"Nesting limit reached: Infinite recursion?");` |
|        6 | 1499 | `		return 1;` |
|        - | 1500 | `	}` |
|        - | 1501 | `	/* Comparison is performed only if the objects are instance of the same class */` |
|      393 | 1502 | `	if( pLeft->pClass != pRight->pClass ){` |
|       10 | 1503 | `		return 1;` |
|        - | 1504 | `	}` |
|      385 | 1505 | `	if( bStrict ){` |
|        - | 1506 | `		/*` |
|        - | 1507 | `		 * According to the PHP language reference manual:` |
|        - | 1508 | `		 *  when using the identity operator (===), object variables` |
|        - | 1509 | `		 *  are identical if and only if they refer to the same instance` |
|        - | 1510 | `		 *  of the same class.` |
|        - | 1511 | `		 */` |
|      205 | 1512 | `		return !(pLeft == pRight);` |
|        - | 1513 | `	}` |
|        - | 1514 | `	/*` |
|        - | 1515 | `	 * Attribute comparison.` |
|        - | 1516 | `	 * According to the PHP reference manual:` |
|        - | 1517 | `	 *  When using the comparison operator (==), object variables are compared` |
|        - | 1518 | `	 *  in a simple manner, namely: Two object instances are equal if they have` |
|        - | 1519 | `	 *  the same attributes and values, and are instances of the same class.` |
|        - | 1520 | `	 */` |
|      185 | 1521 | `	if( pLeft == pRight ){` |
|        - | 1522 | `		/* Same instance,don't bother processing,object are equals */` |
|        5 | 1523 | `		return 0;` |
|        - | 1524 | `	}` |
|        - | 1525 | `	/* Closures compare by IDENTITY under == as well (not by attributes): two distinct` |
|        - | 1526 | `	 * Closure instances are never equal, even when they wrap the same underlying function` |
|        - | 1527 | `	 * (PHP semantics). pLeft != pRight here, so a Closure pair is unequal. Without this,` |
|        - | 1528 | `` 	 * two capture-less lambdas of the same `function(){}` share the template's `$__fn` `` |
|        - | 1529 | `	 * name and would compare equal. */` |
|      181 | 1530 | `	if( pLeft->pVm->pClosureClass && pLeft->pClass == pLeft->pVm->pClosureClass ){` |
|        5 | 1531 | `		return 1;` |
|        - | 1532 | `	}` |
|        - | 1533 | `	/* Same class but a different number of attributes ⇒ different property sets` |
|        - | 1534 | `	 * (dynamic properties can give two same-class instances different counts). */` |
|      177 | 1535 | `	if( pLeft->hAttr.nEntry != pRight->hAttr.nEntry ){` |
|        3 | 1536 | `		return 1;` |
|        - | 1537 | `	}` |
|      175 | 1538 | `	PH7_MemObjInit(pLeft->pVm,&sV1);` |
|      175 | 1539 | `	PH7_MemObjInit(pLeft->pVm,&sV2);` |
|      175 | 1540 | `	sV1.nIdx = sV2.nIdx = SXU32_HIGH;` |
|        - | 1541 | `	/* Compare each left attribute against the RIGHT attribute of the SAME NAME` |
|        - | 1542 | `	 * (not in lockstep): dynamic properties may be stored in a different order` |
|        - | 1543 | `	 * on the two instances. Counts already match, so if every left attribute has` |
|        - | 1544 | `	 * an equal-valued same-named right attribute the property sets are equal. */` |
|      175 | 1545 | `	SyHashResetLoopCursor(&pLeft->hAttr);` |
|      239 | 1546 | `	while((pEntry = SyHashGetNextEntry(&pLeft->hAttr)) != 0 ){` |
|      199 | 1547 | `		VmClassAttr *p1 = (VmClassAttr *)pEntry->pUserData;` |
|        - | 1548 | `		VmClassAttr *p2;` |
|        - | 1549 | `		ph7_value *pL,*pR;` |
|        - | 1550 | `		/* Compare only non-static attribute */` |
|      199 | 1551 | `		if( p1->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|      ! 0 | 1552 | `			continue;` |
|        - | 1553 | `		}` |
|      199 | 1554 | `		pEntry2 = SyHashGet(&pRight->hAttr,SyStringData(&p1->pAttr->sName),SyStringLength(&p1->pAttr->sName));` |
|      199 | 1555 | `		if( pEntry2 == 0 ){` |
|        - | 1556 | `			/* Left has a property the right lacks ⇒ not equal. */` |
|      ! 0 | 1557 | `			return 1;` |
|        - | 1558 | `		}` |
|      199 | 1559 | `		p2 = (VmClassAttr *)pEntry2->pUserData;` |
|      199 | 1560 | `		pL = ExtractClassAttrValue(pLeft->pVm,p1);` |
|      199 | 1561 | `		pR = ExtractClassAttrValue(pRight->pVm,p2);` |
|      199 | 1562 | `		if( pL && pR ){` |
|      199 | 1563 | `			PH7_MemObjLoad(pL,&sV1);` |
|      199 | 1564 | `			PH7_MemObjLoad(pR,&sV2);` |
|        - | 1565 | `			/* Compare the two values now */` |
|      199 | 1566 | `			rc = PH7_MemObjCmp(&sV1,&sV2,bStrict,iNest+1);` |
|      199 | 1567 | `			PH7_MemObjRelease(&sV1);` |
|      199 | 1568 | `			PH7_MemObjRelease(&sV2);` |
|      199 | 1569 | `			if( rc != 0 ){` |
|        - | 1570 | `				/* Not equals */` |
|      133 | 1571 | `				return rc;` |
|        - | 1572 | `			}` |
|       32 | 1573 | `		}` |
|        3 | 1574 | `	}` |
|        - | 1575 | `	/* Object are equals */` |
|       44 | 1576 | `	return 0;` |
|      201 | 1577 | `}` |
|        - | 1578 | `/*` |
|        - | 1579 | ` * Dump a class instance and the store the dump in the BLOB given` |
|        - | 1580 | ` * as the first argument.` |
|        - | 1581 | ` * Note that only non-static/non-constants attribute are dumped.` |
|        - | 1582 | ` * This function is typically invoked when the user issue a call` |
|        - | 1583 | ` * to [var_dump(),var_export(),print_r(),...].` |
|        - | 1584 | ` * This function SXRET_OK on success. Any other return value including` |
|        - | 1585 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|        - | 1586 | ` */` |
|        - | 1587 | `/*` |
|        - | 1588 | `` * Return the `name` property value of an enum case instance (the case name),`` |
|        - | 1589 | ` * or 0 when unavailable. Shared by the var_dump/var_export/json/serialize` |
|        - | 1590 | ` * renderers, which all print enum cases as Class::CaseName forms.` |
|        - | 1591 | ` */` |
|       20 | 1592 | `PH7_PRIVATE ph7_value * PH7_EnumCaseNameValue(ph7_class_instance *pThis)` |
|        1 | 1593 | `{` |
|        - | 1594 | `	SyHashEntry *pEntry;` |
|       21 | 1595 | `	if( (pThis->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|      ! 0 | 1596 | `		return 0;` |
|        - | 1597 | `	}` |
|       21 | 1598 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)"name",sizeof("name")-1);` |
|       21 | 1599 | `	if( pEntry == 0 ){` |
|      ! 0 | 1600 | `		return 0;` |
|        - | 1601 | `	}` |
|       21 | 1602 | `	return PH7_ClassInstanceExtractAttrValue(pThis,(VmClassAttr *)pEntry->pUserData);` |
|       11 | 1603 | `}` |
|        - | 1604 | `/*` |
|        - | 1605 | `` * Return the `value` property value (the backing value) of an enum case`` |
|        - | 1606 | ` * instance, or 0 when unavailable (pure enums have none).` |
|        - | 1607 | ` */` |
|       20 | 1608 | `PH7_PRIVATE ph7_value * PH7_EnumCaseBackingValueOf(ph7_class_instance *pThis)` |
|        1 | 1609 | `{` |
|        - | 1610 | `	SyHashEntry *pEntry;` |
|       21 | 1611 | `	if( (pThis->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|      ! 0 | 1612 | `		return 0;` |
|        - | 1613 | `	}` |
|       21 | 1614 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)"value",sizeof("value")-1);` |
|       21 | 1615 | `	if( pEntry == 0 ){` |
|        7 | 1616 | `		return 0;` |
|        - | 1617 | `	}` |
|       15 | 1618 | `	return PH7_ClassInstanceExtractAttrValue(pThis,(VmClassAttr *)pEntry->pUserData);` |
|       11 | 1619 | `}` |
|        - | 1620 | `/*` |
|        - | 1621 | ` * Emit a class-instance dump header plus its trailing newline. For var_dump` |
|        - | 1622 | ` * (ShowType) it completes the "object(" prefix the caller already emitted as` |
|        - | 1623 | ` *   ClassName)#<id> (<count>) {` |
|        - | 1624 | ` * for print_r it emits the legacy PHL  Object(ClassName) {  (count/id unused).` |
|        - | 1625 | `` * Enum cases print php's `ClassName Enum {` print_r header (var_dump never`` |
|        - | 1626 | `` * reaches here for enums — PH7_MemObjDump prints `enum(S::A)` directly).`` |
|        - | 1627 | ` */` |
|      242 | 1628 | `static void DumpClassInstanceHeader(SyBlob *pOut,ph7_class *pClass,sxu32 nObjId,int ShowType,sxu32 nCount)` |
|        4 | 1629 | `{` |
|      246 | 1630 | `	if( ShowType ){` |
|        - | 1631 | ``		/* var_dump: `object(C)#id (n) {` */`` |
|      187 | 1632 | `		SyBlobFormat(&(*pOut),"object(%z)#%u (%u) {",&pClass->sName,nObjId,nCount);` |
|      187 | 1633 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      187 | 1634 | `		return;` |
|        - | 1635 | `	}` |
|        - | 1636 | ``	/* print_r: `C Object` / `E Enum[:backing]` — the '(' line is emitted by`` |
|        - | 1637 | `	 * the body renderer at the container indent. */` |
|       62 | 1638 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|      ! 0 | 1639 | `		SyBlobFormat(&(*pOut),"%z Enum",&pClass->sName);` |
|      ! 0 | 1640 | `		if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|      ! 0 | 1641 | `			SyBlobAppend(&(*pOut),":int",sizeof(":int")-1);` |
|      ! 0 | 1642 | `		}else if( pClass->nEnumBacking == MEMOBJ_STRING ){` |
|      ! 0 | 1643 | `			SyBlobAppend(&(*pOut),":string",sizeof(":string")-1);` |
|      ! 0 | 1644 | `		}` |
|      ! 0 | 1645 | `	}else{` |
|       62 | 1646 | `		SyBlobFormat(&(*pOut),"%z Object",&pClass->sName);` |
|        - | 1647 | `	}` |
|       62 | 1648 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      125 | 1649 | `}` |
|        - | 1650 | `/*` |
|        - | 1651 | ` * The class that DECLARED pAttr: inheritance shares attr pointers down the` |
|        - | 1652 | ` * chain, so the declaring class is the most ANCESTRAL class whose hAttr still` |
|        - | 1653 | ` * maps the name to this exact pointer. php's var_dump/print_r use it for the` |
|        - | 1654 | `` * `["p":"Decl":private]` annotation.`` |
|        - | 1655 | ` */` |
|        8 | 1656 | `static ph7_class * OoAttrDeclaringClass(ph7_class *pClass,ph7_class_attr *pAttr)` |
|        2 | 1657 | `{` |
|        - | 1658 | `	/* Attrs record their declaring class at install time (inheritance/trait` |
|        - | 1659 | `	 * copies share the pointer, so the field survives the chain). */` |
|       10 | 1660 | `	return pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        2 | 1661 | `}` |
|        - | 1662 | `/*` |
|        - | 1663 | ` * php's zend_unmangle_property_name_ex: a property key carries its own` |
|        - | 1664 | ` * visibility when it is MANGLED — "\0*\0name" is protected and "\0Class\0name"` |
|        - | 1665 | ` * is private to Class, which is how the (array) cast, __debugInfo() and every` |
|        - | 1666 | ` * get_debug_info handler say what a plain array key cannot. A key that does not` |
|        - | 1667 | ` * begin with a NUL is a public name and comes back unchanged.` |
|        - | 1668 | ` *` |
|        - | 1669 | ` * Answers 0 for a key that begins with a NUL and is NOT a well-formed mangled` |
|        - | 1670 | ` * name — php's "Illegal member variable name" (nothing after the NUL, or an` |
|        - | 1671 | ` * empty class part) and "Corrupt member variable name" (no second NUL, or` |
|        - | 1672 | ` * nothing after it). php renders those raw, notice aside, and so does the caller.` |
|        - | 1673 | ` */` |
|      182 | 1674 | `PH7_PRIVATE int PH7_UnmangleAttrName(const char *zKey,sxu32 nKey,SyString *pClass,SyString *pName)` |
|        3 | 1675 | `{` |
|        - | 1676 | `	sxu32 nCls,nSrc;` |
|      185 | 1677 | `	SyStringInitFromBuf(pClass,0,0);` |
|      185 | 1678 | `	SyStringInitFromBuf(pName,zKey,nKey);` |
|      185 | 1679 | `	if( nKey < 1 \|\| zKey[0] != 0 ){` |
|      137 | 1680 | `		return 1;   /* a plain public name */` |
|        - | 1681 | `	}` |
|       49 | 1682 | `	if( nKey < 3 \|\| zKey[1] == 0 ){` |
|      ! 0 | 1683 | `		return 0;   /* php: "Illegal member variable name" */` |
|        - | 1684 | `	}` |
|       49 | 1685 | `	nCls = 0;` |
|      429 | 1686 | `	while( nCls < nKey - 2 && zKey[1+nCls] != 0 ){` |
|      381 | 1687 | `		nCls++;` |
|        1 | 1688 | `	}` |
|       49 | 1689 | `	if( nCls >= nKey - 2 ){` |
|      ! 0 | 1690 | `		return 0;   /* php: "Corrupt member variable name" */` |
|        - | 1691 | `	}` |
|        - | 1692 | `	/* An ANONYMOUS class mangles its source location in as a SECOND NUL-separated` |
|        - | 1693 | `	 * part, so the property name is what follows the LAST NUL rather than the` |
|        - | 1694 | `	 * second one (php's anonclass_src_len step). The class STRING php shows is` |
|        - | 1695 | `	 * still only the first part — it prints that one as a C string. */` |
|       49 | 1696 | `	nSrc = 0;` |
|      313 | 1697 | `	while( nCls + 2 + nSrc < nKey && zKey[nCls+2+nSrc] != 0 ){` |
|      265 | 1698 | `		nSrc++;` |
|        1 | 1699 | `	}` |
|       49 | 1700 | `	SyStringInitFromBuf(pClass,&zKey[1],nCls);` |
|       49 | 1701 | `	if( nCls + nSrc + 2 != nKey ){` |
|      ! 0 | 1702 | `		nCls += nSrc + 1;` |
|      ! 0 | 1703 | `	}` |
|       49 | 1704 | `	SyStringInitFromBuf(pName,&zKey[nCls+2],nKey - nCls - 2);` |
|       49 | 1705 | `	return 1;` |
|       94 | 1706 | `}` |
|        - | 1707 | `/*` |
|        - | 1708 | `` * Emit a property's dump key: var_dump `["x"]=>` / `["p":"C":private]=>` /`` |
|        - | 1709 | `` * `["q":protected]=>`; print_r `[x] => ` / `[p:C:private] => ` /`` |
|        - | 1710 | `` * `[q:protected] => ` (php's exact annotations).`` |
|        - | 1711 | ` */` |
|      198 | 1712 | `static void OoDumpPropKey(SyBlob *pOut,ph7_class_instance *pThis,ph7_class_attr *pAttr,int ShowType)` |
|        4 | 1713 | `{` |
|      202 | 1714 | `	const char *zQ = ShowType ? "\"" : "";` |
|      202 | 1715 | `	if( SyStringLength(&pAttr->sName) > 0 && SyStringData(&pAttr->sName)[0] == 0 ){` |
|        - | 1716 | `		/* A MANGLED name stored raw — the __PHP_Incomplete_Class carrier's` |
|        - | 1717 | `		 * private/protected payload keys. php's dump unmangles them exactly as` |
|        - | 1718 | ``		 * it does an (array) cast's: `["bp":"PB":private]` / `["pp":protected]`. */`` |
|        - | 1719 | `		SyString sUnmCls, sUnmName;` |
|        9 | 1720 | `		if( PH7_UnmangleAttrName(SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),` |
|        - | 1721 | `			&sUnmCls,&sUnmName) ){` |
|        9 | 1722 | `			SyBlobFormat(&(*pOut),"[%s%z%s",zQ,&sUnmName,zQ);` |
|        9 | 1723 | `			if( sUnmCls.nByte == 1 && sUnmCls.zString[0] == '*' ){` |
|        5 | 1724 | `				SyBlobAppend(&(*pOut),":protected",sizeof(":protected")-1);` |
|        3 | 1725 | `			}else{` |
|        5 | 1726 | `				SyBlobFormat(&(*pOut),":%s%z%s:private",zQ,&sUnmCls,zQ);` |
|        - | 1727 | `			}` |
|        9 | 1728 | `			SyBlobAppend(&(*pOut),ShowType ? "]=>" : "] => ",ShowType ? 3 : 5);` |
|        9 | 1729 | `			return;` |
|        - | 1730 | `		}` |
|      ! 0 | 1731 | `	}` |
|      194 | 1732 | `	SyBlobFormat(&(*pOut),"[%s%z%s",zQ,&pAttr->sName,zQ);` |
|      194 | 1733 | `	if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       10 | 1734 | `		ph7_class *pDecl = OoAttrDeclaringClass(pThis->pClass,pAttr);` |
|       10 | 1735 | `		SyBlobFormat(&(*pOut),":%s%z%s:private",zQ,&pDecl->sName,zQ);` |
|      190 | 1736 | `	}else if( pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
|        7 | 1737 | `		SyBlobAppend(&(*pOut),":protected",sizeof(":protected")-1);` |
|        3 | 1738 | `	}` |
|      194 | 1739 | `	SyBlobAppend(&(*pOut),ShowType ? "]=>" : "] => ",ShowType ? 3 : 5);` |
|      103 | 1740 | `}` |
|      246 | 1741 | `PH7_PRIVATE sxi32 PH7_ClassInstanceDump(SyBlob *pOut,ph7_class_instance *pThis,int ShowType,int nTab,int nDepth)` |
|        4 | 1742 | `{` |
|        - | 1743 | `	SyHashEntry *pEntry;` |
|        - | 1744 | `	ph7_value *pValue;` |
|        - | 1745 | `	sxi32 rc;` |
|        - | 1746 | `	int i;` |
|      250 | 1747 | `	if( nDepth > 31 ){` |
|        - | 1748 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|        - | 1749 | `		/* Nesting limit reached..halt immediately*/` |
|        5 | 1750 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|        5 | 1751 | `		return SXERR_LIMIT;` |
|        - | 1752 | `	}` |
|      246 | 1753 | `	rc = SXRET_OK;` |
|        - | 1754 | `	{` |
|        - | 1755 | `		/* A native class's PRESENTATION (php's get_debug_info): the shape it shows` |
|        - | 1756 | `		 * is not its storage — a DateTime shows date/timezone_type/timezone, a` |
|        - | 1757 | `		 * WeakReference shows ["object"] — and the slots underneath are hidden.` |
|        - | 1758 | `		 * Rendered exactly like a __debugInfo() array, which is what php does with` |
|        - | 1759 | `		 * it, and consulted first because php's handler wins over a userland` |
|        - | 1760 | `		 * method a native class cannot declare anyway. */` |
|        - | 1761 | `		ph7_value sPresent;` |
|      246 | 1762 | `		PH7_MemObjInit(pThis->pVm,&sPresent);` |
|      246 | 1763 | `		if( ph7_value_is_array(&sPresent) == 0 ){` |
|      246 | 1764 | `			ph7_hashmap *pPresent = PH7_NewHashmap(pThis->pVm,0,0);` |
|      246 | 1765 | `			if( pPresent ){` |
|      246 | 1766 | `				sPresent.x.pOther = pPresent;` |
|      246 | 1767 | `				MemObjSetType(&sPresent,MEMOBJ_HASHMAP);` |
|      121 | 1768 | `			}` |
|      121 | 1769 | `		}` |
|      242 | 1770 | `		if( (sPresent.iFlags & MEMOBJ_HASHMAP)` |
|      246 | 1771 | `		 && PH7_ClassInstancePresent(pThis,&sPresent,1) ){` |
|       62 | 1772 | `			ph7_hashmap *pMap = (ph7_hashmap *)sPresent.x.pOther;` |
|       62 | 1773 | `			DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,pMap->nEntry);` |
|       62 | 1774 | `			if( !ShowType ){` |
|       34 | 1775 | `				for( i = 0 ; i < nTab ; i++ ){` |
|      ! 0 | 1776 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      ! 0 | 1777 | `				}` |
|       34 | 1778 | `				SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       16 | 1779 | `			}` |
|       62 | 1780 | `			rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth,1);` |
|       62 | 1781 | `			for( i = 0 ; i < nTab ; i++ ){` |
|      ! 0 | 1782 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      ! 0 | 1783 | `			}` |
|       62 | 1784 | `			if( ShowType ){` |
|       29 | 1785 | `				SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|       15 | 1786 | `			}else{` |
|       34 | 1787 | `				SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        - | 1788 | `			}` |
|       62 | 1789 | `			PH7_MemObjRelease(&sPresent);` |
|       62 | 1790 | `			return rc;` |
|        - | 1791 | `		}` |
|      186 | 1792 | `		PH7_MemObjRelease(&sPresent);` |
|        - | 1793 | `	}` |
|        - | 1794 | `	{` |
|        - | 1795 | `		/* Both var_dump and print_r consult __debugInfo() (PHP behavior);` |
|        - | 1796 | `		 * var_export uses a separate renderer and never reaches here. When the` |
|        - | 1797 | `		 * method is present and returns an array, render that array's entries as` |
|        - | 1798 | `		 * the object body, with the header showing the debug array's count. The` |
|        - | 1799 | `		 * nDepth guard above protects against a __debugInfo returning the object` |
|        - | 1800 | `		 * itself. */` |
|      186 | 1801 | `		ph7_class_method *pDbg = PH7_ClassExtractMethod(pThis->pClass,"__debugInfo",sizeof("__debugInfo")-1);` |
|      186 | 1802 | `		if( pDbg ){` |
|        - | 1803 | `			ph7_value sResult;` |
|       19 | 1804 | `			PH7_MemObjInit(pThis->pVm,&sResult);` |
|       19 | 1805 | `			PH7_VmCallMagicMethod(pThis->pVm,pThis,pDbg,&sResult,0,0);` |
|       19 | 1806 | `			if( sResult.iFlags & MEMOBJ_HASHMAP ){` |
|       19 | 1807 | `				ph7_hashmap *pMap = (ph7_hashmap *)sResult.x.pOther;` |
|        - | 1808 | `				/* Header count is the debug array's entry count. */` |
|       19 | 1809 | `				DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,pMap->nEntry);` |
|       19 | 1810 | `				if( !ShowType ){` |
|        8 | 1811 | `					for( i = 0 ; i < nTab ; i++ ){` |
|      ! 0 | 1812 | `						SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      ! 0 | 1813 | `					}` |
|        8 | 1814 | `					SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|        3 | 1815 | `				}` |
|       19 | 1816 | `				rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth,1);` |
|       19 | 1817 | `				for( i = 0 ; i < nTab ; i++ ){` |
|      ! 0 | 1818 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      ! 0 | 1819 | `				}` |
|       19 | 1820 | `				if( ShowType ){` |
|       13 | 1821 | `					SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|        8 | 1822 | `				}else{` |
|        8 | 1823 | `					SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        - | 1824 | `				}` |
|       19 | 1825 | `				PH7_MemObjRelease(&sResult);` |
|       19 | 1826 | `				return rc;` |
|        - | 1827 | `			}` |
|        - | 1828 | `			/* Non-array return: behave as if __debugInfo were absent. */` |
|      ! 0 | 1829 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 | 1830 | `		}` |
|        - | 1831 | `	}` |
|        - | 1832 | `	{` |
|        - | 1833 | `		/* var_dump's header needs the property count up front, so pre-count the` |
|        - | 1834 | `		 * non-static/non-constant attributes (matching the dump loop below).` |
|        - | 1835 | `		 * An UNINITIALIZED typed property is still LISTED by var_dump — with php's` |
|        - | 1836 | ``		 * `uninitialized(T)` marker in place of a value — but it does not COUNT,`` |
|        - | 1837 | `` 		 * which is how php's `object(C)#1 (0) { ["a"]=> uninitialized(int) }` `` |
|        - | 1838 | `		 * reads. */` |
|      170 | 1839 | `		sxu32 nProp = 0;` |
|      170 | 1840 | `		if( ShowType ){` |
|      149 | 1841 | `			SyHashResetLoopCursor(&pThis->hAttr);` |
|      402 | 1842 | `			while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|      183 | 1843 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      180 | 1844 | `				if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_HOOK_VIRTUAL\|PH7_CLASS_ATTR_HIDDEN)) == 0` |
|      180 | 1845 | `				 && !PH7_ClassAttrUninitialized(pVmAttr) ){` |
|      159 | 1846 | `					nProp++;` |
|       78 | 1847 | `				}` |
|        3 | 1848 | `			}` |
|       73 | 1849 | `		}` |
|      170 | 1850 | `		DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,nProp);` |
|        - | 1851 | `	}` |
|      170 | 1852 | `	if( !ShowType ){` |
|        - | 1853 | `		/* print_r body opener: '(' at the container indent */` |
|      136 | 1854 | `		for( i = 0 ; i < nTab ; i++ ){` |
|      114 | 1855 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       58 | 1856 | `		}` |
|       24 | 1857 | `		SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       10 | 1858 | `	}` |
|        - | 1859 | `	/* Dump object attributes (php 8.4: VIRTUAL hooked properties have no` |
|        - | 1860 | `	 * backing store — excluded from var_dump/print_r) */` |
|      170 | 1861 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      355 | 1862 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|      230 | 1863 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      230 | 1864 | `		if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_HOOK_VIRTUAL\|PH7_CLASS_ATTR_HIDDEN)) == 0 ){` |
|      220 | 1865 | `			if( PH7_ClassAttrUninitialized(pVmAttr) ){` |
|        - | 1866 | `				/* var_dump names the property and prints php's marker in place of` |
|        - | 1867 | `				 * the value it has not got; print_r has no such marker and leaves` |
|        - | 1868 | `				 * the property out entirely. */` |
|       37 | 1869 | `				if( ShowType ){` |
|        - | 1870 | `					char zType[192];` |
|       28 | 1871 | `					const char *zText = VmHintTextResolved(pThis->pVm,&pVmAttr->pAttr->sTypeName,` |
|       18 | 1872 | `						VmHintScopeClass(pThis->pVm,pVmAttr->pAttr->pDeclClass,pVmAttr->pOwner),` |
|        9 | 1873 | `						zType,sizeof(zType));` |
|       55 | 1874 | `					for( i = 0 ; i < nTab + 2 ; i++ ){` |
|       37 | 1875 | `						SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       19 | 1876 | `					}` |
|       19 | 1877 | `					OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,TRUE);` |
|       19 | 1878 | `					SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       55 | 1879 | `					for( i = 0 ; i < nTab + 2 ; i++ ){` |
|       37 | 1880 | `						SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       19 | 1881 | `					}` |
|       19 | 1882 | `					SyBlobFormat(&(*pOut),"uninitialized(%s)\n",zText);` |
|        9 | 1883 | `				}` |
|       37 | 1884 | `				continue;` |
|        - | 1885 | `			}` |
|        - | 1886 | `			/* Dump non-static/constant attribute only */` |
|      184 | 1887 | `			pValue = ExtractClassAttrValue(pThis->pVm,pVmAttr);` |
|      184 | 1888 | `			if( pValue == 0 ){` |
|      ! 0 | 1889 | `				continue;` |
|        - | 1890 | `			}` |
|      184 | 1891 | `			if( ShowType ){` |
|        - | 1892 | ``				/* var_dump prop: `["x"(:…)]=>` at nTab+2, the value on the next`` |
|        - | 1893 | `				 * line at the same indent (php). */` |
|     4207 | 1894 | `				for( i = 0 ; i < nTab + 2 ; i++ ){` |
|     4051 | 1895 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     2027 | 1896 | `				}` |
|      159 | 1897 | `				OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,TRUE);` |
|      159 | 1898 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      159 | 1899 | `				rc = PH7_MemObjDump(&(*pOut),pValue,TRUE,nTab+2,nDepth,0);` |
|      159 | 1900 | `				if( rc == SXERR_LIMIT ){` |
|      125 | 1901 | `					break;` |
|        - | 1902 | `				}` |
|       19 | 1903 | `			}else{` |
|        - | 1904 | ``				/* print_r prop: `[x(:…)] => value` at nTab+4; container values`` |
|        - | 1905 | `				 * render their block at nTab+8 followed by php's blank line. */` |
|      187 | 1906 | `				for( i = 0 ; i < nTab + 4 ; i++ ){` |
|      163 | 1907 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       83 | 1908 | `				}` |
|       27 | 1909 | `				OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,FALSE);` |
|       24 | 1910 | `				if( (pValue->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ))` |
|       15 | 1911 | `				 && (pValue->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 | 1912 | `					rc = PH7_MemObjDump(&(*pOut),pValue,FALSE,nTab+8,nDepth,0);` |
|      ! 0 | 1913 | `					SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      ! 0 | 1914 | `					if( rc == SXERR_LIMIT ){` |
|      ! 0 | 1915 | `						break;` |
|        - | 1916 | `					}` |
|      ! 0 | 1917 | `				}else{` |
|       27 | 1918 | `					PH7_MemObjPrintRInline(&(*pOut),pValue);` |
|       27 | 1919 | `					SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        - | 1920 | `				}` |
|        - | 1921 | `			}` |
|       28 | 1922 | `		}` |
|        4 | 1923 | `	}` |
|     4022 | 1924 | `	for( i = 0 ; i < nTab ; i++ ){` |
|     3854 | 1925 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     1928 | 1926 | `	}` |
|      170 | 1927 | `	if( ShowType ){` |
|      149 | 1928 | `		SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|       76 | 1929 | `	}else{` |
|       24 | 1930 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        - | 1931 | `	}` |
|      170 | 1932 | `	return rc;` |
|      127 | 1933 | `}` |
|        - | 1934 | `/*` |
|        - | 1935 | ` * Call a magic method [i.e: __toString(),__toBool(),__Invoke()...]` |
|        - | 1936 | ` * Return SXRET_OK on successfull call. Any other return value indicates failure.` |
|        - | 1937 | ` * Notes on magic methods.` |
|        - | 1938 | ` * According to the PHP language reference manual.` |
|        - | 1939 | ` *  The function names __construct(), __destruct(), __call(), __callStatic()` |
|        - | 1940 | ` *  __get(),  __toString(), __invoke(), __clone() are magical in PHP classes.` |
|        - | 1941 | ` * You cannot have functions with these names in any of your classes unless` |
|        - | 1942 | ` * you want the magic functionality associated with them.` |
|        - | 1943 | ` * Example of magical methods:` |
|        - | 1944 | ` * __toString()` |
|        - | 1945 | ` *  The __toString() method allows a class to decide how it will react when it is treated like` |
|        - | 1946 | ` *  a string. For example, what echo $obj; will print. This method must return a string.` |
|        - | 1947 | ` *  Example #2 Simple example` |
|        - | 1948 | ` * <?php` |
|        - | 1949 | ` * // Declare a simple class` |
|        - | 1950 | ` * class TestClass` |
|        - | 1951 | ` * {` |
|        - | 1952 | ` *   public $foo;` |
|        - | 1953 | ` *` |
|        - | 1954 | ` *   public function __construct($foo)` |
|        - | 1955 | ` *   {` |
|        - | 1956 | ` *       $this->foo = $foo;` |
|        - | 1957 | ` *   }` |
|        - | 1958 | ` *` |
|        - | 1959 | ` *   public function __toString()` |
|        - | 1960 | ` *   {` |
|        - | 1961 | ` *       return $this->foo;` |
|        - | 1962 | ` *   }` |
|        - | 1963 | ` * }` |
|        - | 1964 | ` * $class = new TestClass('Hello');` |
|        - | 1965 | ` * echo $class;` |
|        - | 1966 | ` * ?>` |
|        - | 1967 | ` * The above example will output:` |
|        - | 1968 | ` *  Hello` |
|        - | 1969 | ` *` |
|        - | 1970 | ` * Note that PH7 does not support all the magical method and introudces __toFloat(),__toInt()` |
|        - | 1971 | ` * which have the same behaviour as __toString() but for float and integer types` |
|        - | 1972 | ` * respectively.` |
|        - | 1973 | ` * Refer to the official documentation for more information.` |
|        - | 1974 | ` */` |
|      484 | 1975 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallMagicMethod(` |
|        - | 1976 | `	ph7_vm *pVm,               /* VM that own all this stuff */` |
|        - | 1977 | `	ph7_class *pClass,         /* Target class */` |
|        - | 1978 | `	ph7_class_instance *pThis, /* Target object */` |
|        - | 1979 | `	const char *zMethod,       /* Magic method name [i.e: __toString()]*/` |
|        - | 1980 | `	sxu32 nByte,               /* zMethod length*/` |
|        - | 1981 | `	const SyString *pAttrName, /* Attribute name */` |
|        - | 1982 | `	ph7_value *pResult         /* OUT: magic method return value. NULL to discard */` |
|        - | 1983 | `	)` |
|        5 | 1984 | `{` |
|      489 | 1985 | `	ph7_value *apArg[2] = { 0 , 0 };` |
|        - | 1986 | `	ph7_class_method *pMeth;` |
|        - | 1987 | `	ph7_value sAttr; /* cc warning */` |
|        - | 1988 | `	sxi32 rc;` |
|        - | 1989 | `	int nArg;` |
|        - | 1990 | `	/* Make sure the magic method is available */` |
|      489 | 1991 | `	pMeth = PH7_ClassExtractMethod(&(*pClass),zMethod,nByte);` |
|      489 | 1992 | `	if( pMeth == 0 ){` |
|        - | 1993 | `		/* No such method,return immediately */` |
|      ! 0 | 1994 | `		return SXERR_NOTFOUND;` |
|        - | 1995 | `	}` |
|      489 | 1996 | `	nArg = 0;` |
|        - | 1997 | `	/* Copy arguments */` |
|      489 | 1998 | `	if( pAttrName ){` |
|      489 | 1999 | `		PH7_MemObjInitFromString(pVm,&sAttr,pAttrName);` |
|      489 | 2000 | `		sAttr.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      489 | 2001 | `		apArg[0] = &sAttr;` |
|      489 | 2002 | `		nArg = 1;` |
|      242 | 2003 | `	}` |
|        - | 2004 | `	/* Call the magic method now */` |
|      489 | 2005 | `	rc = PH7_VmCallMagicMethod(pVm,&(*pThis),pMeth,pResult,nArg,apArg);` |
|        - | 2006 | `	/* Clean up */` |
|      489 | 2007 | `	if( pAttrName ){` |
|      489 | 2008 | `		PH7_MemObjRelease(&sAttr);` |
|      242 | 2009 | `	}` |
|      489 | 2010 | `	return rc;` |
|      247 | 2011 | `}` |
|        - | 2012 | `/*` |
|        - | 2013 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon].` |
|        - | 2014 | ` * This function is simply a wrapper on ExtractClassAttrValue().` |
|        - | 2015 | ` */` |
|  5824752 | 2016 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceExtractAttrValue(ph7_class_instance *pThis,VmClassAttr *pAttr)` |
|        5 | 2017 | `{` |
|        - | 2018 | `   /* Extract the attribute value */` |
|        - | 2019 | `	ph7_value *pValue;` |
|  5824757 | 2020 | `	pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|  5824757 | 2021 | `	return pValue;` |
|        5 | 2022 | `}` |
|        - | 2023 | `/*` |
|        - | 2024 | ` * Convert a class instance [i.e: Object in the PHP jargon] into a hashmap [i.e: array in the PHP jargon].` |
|        - | 2025 | ` * Return SXRET_OK on success. Any other value indicates failure.` |
|        - | 2026 | ` * Note on object conversion to array:` |
|        - | 2027 | ` *  Acccording to the PHP language reference manual` |
|        - | 2028 | ` *  If an object is converted to an array, the result is an array whose elements are the object's properties.` |
|        - | 2029 | ` *  The keys are the member variable names.` |
|        - | 2030 | ` *` |
|        - | 2031 | ` *  The following example:` |
|        - | 2032 | ` *  class Test {` |
|        - | 2033 | ` *   public $A = 25<<1;  // 50` |
|        - | 2034 | ` *	 public $c = rand_str(3);   // Random string of length 3` |
|        - | 2035 | ` *	 public $d = rand() & 1023; // Random number between 0..1023` |
|        - | 2036 | ` *  }` |
|        - | 2037 | ` *  var_dump((array) new Test());` |
|        - | 2038 | ` *	Will output:` |
|        - | 2039 | ` *  array(3) {` |
|        - | 2040 | ` *   [A] =>` |
|        - | 2041 | ` *      int(50)` |
|        - | 2042 | ` *   [c] =>` |
|        - | 2043 | ` *     string(3 'aps')` |
|        - | 2044 | ` *   [d] =>` |
|        - | 2045 | ` *     int(991)` |
|        - | 2046 | ` *  }` |
|        - | 2047 | ` * You have noticed that PH7 allow class attributes [i.e: $a,$c,$d in the example above]` |
|        - | 2048 | ` * have any complex expression (even function calls/Annonymous functions) as their default` |
|        - | 2049 | ` * value unlike the standard PHP engine.` |
|        - | 2050 | ` * This is a very powerful feature that you have to look at.` |
|        - | 2051 | ` */` |
|      118 | 2052 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|        4 | 2053 | `{` |
|        - | 2054 | `	{` |
|        - | 2055 | `		/* php's get_properties handler, which is what the (array) cast reads: a` |
|        - | 2056 | `		 * DateTime casts to date/timezone_type/timezone, not to the hidden slots` |
|        - | 2057 | `		 * holding its timestamp. A class whose hook answers only the DEBUG surface` |
|        - | 2058 | `		 * (WeakReference) fills nothing here, and that EMPTY answer is php's — the` |
|        - | 2059 | `		 * hook is authoritative, so there is no falling back to the slot walk. It` |
|        - | 2060 | `		 * used to fall back when the hook filled nothing, which was invisible while` |
|        - | 2061 | `		 * every hooked class also had no visible property: an ArrayObject SUBCLASS` |
|        - | 2062 | ``		 * with an empty storage would have cast to its own `p` where php casts to`` |
|        - | 2063 | `		 * the (empty) storage. */` |
|        - | 2064 | `		ph7_value sPresent;` |
|      122 | 2065 | `		PH7_MemObjInit(pThis->pVm,&sPresent);` |
|      122 | 2066 | `		sPresent.x.pOther = pMap;` |
|      122 | 2067 | `		MemObjSetType(&sPresent,MEMOBJ_HASHMAP);` |
|      122 | 2068 | `		if( PH7_ClassInstancePresent(pThis,&sPresent,0) ){` |
|        - | 2069 | `			/* The map IS the destination; do not release the carrier's hashmap. */` |
|       44 | 2070 | `			sPresent.x.pOther = 0;` |
|       44 | 2071 | `			sPresent.iFlags = MEMOBJ_NULL;` |
|       44 | 2072 | `			return SXRET_OK;` |
|        - | 2073 | `		}` |
|       80 | 2074 | `		sPresent.x.pOther = 0;` |
|       80 | 2075 | `		sPresent.iFlags = MEMOBJ_NULL;` |
|       80 | 2076 | `		PH7_MemObjRelease(&sPresent);` |
|        - | 2077 | `	}` |
|       80 | 2078 | `	return PH7_ClassInstanceToHashmapRaw(pThis,pMap);` |
|       63 | 2079 | `}` |
|        - | 2080 | `/*` |
|        - | 2081 | ` * Is this property NOT THERE YET?` |
|        - | 2082 | ` *` |
|        - | 2083 | ` * php 7.4's typed properties have a third state beside "holds a value" and "does not` |
|        - | 2084 | `` * exist": a typed property with no default is UNINITIALIZED at `new`, reading it is an`` |
|        - | 2085 | ` * Error rather than a null, and every surface that presents an object's properties` |
|        - | 2086 | ` * leaves it out — get_object_vars(), get_mangled_object_vars(), the (array) cast,` |
|        - | 2087 | ` * foreach, json_encode(), serialize(), print_r() and var_export(). var_dump() is the one` |
|        - | 2088 | ``  * exception and only half of one: it NAMES the property and prints `uninitialized(T)` `` |
|        - | 2089 | ` * where the value would be, and does not count it in the header.` |
|        - | 2090 | ` *` |
|        - | 2091 | ` * The state is already tracked (it is what makes the read throw); this asks it by name` |
|        - | 2092 | ` * so the eight surfaces agree. VM_CLASS_ATTR_UNINIT doubles as the readonly write-once` |
|        - | 2093 | ` * latch, which wants the same answer: a readonly property must be typed and cannot have` |
|        - | 2094 | ` * a default, so before its first write php calls it uninitialized too.` |
|        - | 2095 | ` */` |
|     1468 | 2096 | `PH7_PRIVATE int PH7_ClassAttrUninitialized(VmClassAttr *pVmAttr)` |
|        5 | 2097 | `{` |
|     1473 | 2098 | `	return pVmAttr != 0 && (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) != 0;` |
|        5 | 2099 | `}` |
|        - | 2100 | `/*` |
|        - | 2101 | `` * The same question asked by the four surfaces that read through a php 8.4 `get` HOOK —`` |
|        - | 2102 | ` * get_object_vars(), json_encode(), foreach and var_export().` |
|        - | 2103 | ` *` |
|        - | 2104 | ` * A hooked property's value is whatever its hook answers, so an empty backing slot is` |
|        - | 2105 | ` * not an absence there: a VIRTUAL property has no slot at all and still has a value` |
|        - | 2106 | `` * (php lists `virt` in all four), and a BACKED one whose hook reads its own slot raises`` |
|        - | 2107 | ` * the uninitialized Error from inside the hook — which is php's answer for these four` |
|        - | 2108 | ``  * and not something to pre-empt by skipping the property. Only a property with no `get` `` |
|        - | 2109 | ` * at all is absent.` |
|        - | 2110 | ` */` |
|      558 | 2111 | `PH7_PRIVATE int PH7_ClassAttrUninitializedForRead(VmClassAttr *pVmAttr)` |
|        5 | 2112 | `{` |
|      619 | 2113 | `	return PH7_ClassAttrUninitialized(pVmAttr)` |
|      558 | 2114 | `		&& (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET) == 0;` |
|        5 | 2115 | `}` |
|        - | 2116 | `/*` |
|        - | 2117 | ` * The SLOT walk under the cast above, with php's get_properties handler left out:` |
|        - | 2118 | ` * the instance's own property table, mangled, and nothing else.` |
|        - | 2119 | ` *` |
|        - | 2120 | `` * This is what `get_mangled_object_vars()` answers — php asks the same handler with a`` |
|        - | 2121 | ` * different PURPOSE there, and every class here that has a handler answers the raw` |
|        - | 2122 | `` * table for it: `get_mangled_object_vars(new ArrayObject([1,2]))` is the EMPTY array`` |
|        - | 2123 | `` * where the cast is `[1,2]`, and a DateTime's is empty where the cast has three keys.`` |
|        - | 2124 | ` * Since PHL's engine slots are hidden (and php's equivalents live outside the property` |
|        - | 2125 | ` * table entirely), dropping the handler is the whole difference.` |
|        - | 2126 | ` */` |
|       90 | 2127 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmapRaw(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|        4 | 2128 | `{` |
|        - | 2129 | `	SyHashEntry *pEntry;` |
|        - | 2130 | `	SyString *pAttrName;` |
|        - | 2131 | `	VmClassAttr *pAttr;` |
|        - | 2132 | `	ph7_value *pValue;` |
|        - | 2133 | `	ph7_value sName;` |
|        - | 2134 | `	/* Reset the loop cursor */` |
|       94 | 2135 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|       94 | 2136 | `	PH7_MemObjInitFromString(pThis->pVm,&sName,0);` |
|      398 | 2137 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|        - | 2138 | `		/* Point to the current attribute */` |
|      308 | 2139 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      308 | 2140 | `		if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_HIDDEN) ){` |
|        - | 2141 | `			/* A static property is the CLASS's, not the object's: php's cast` |
|        - | 2142 | `			 * yields only the instance's own properties. */` |
|       71 | 2143 | `			continue;` |
|        - | 2144 | `		}` |
|      240 | 2145 | `		if( PH7_ClassAttrUninitialized(pAttr) ){` |
|       81 | 2146 | `			continue; /* typed, never written: not there yet (php) */` |
|        - | 2147 | `		}` |
|      162 | 2148 | `		if( pAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|        - | 2149 | `			/* php 8.4: a VIRTUAL hooked property has no backing store — the` |
|        - | 2150 | `			 * (array) cast excludes it (raw surface, get is NOT dispatched) */` |
|      ! 0 | 2151 | `			continue;` |
|        - | 2152 | `		}` |
|        - | 2153 | `		/* Extract attribute value */` |
|      162 | 2154 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|      162 | 2155 | `		if( pValue ){` |
|        - | 2156 | `			/* Build attribute name. php MANGLES the key of a non-public property` |
|        - | 2157 | `			 * when it casts an object to an array: a private one becomes` |
|        - | 2158 | `			 * "\0DeclaringClass\0name" and a protected one "\0*\0name", so two` |
|        - | 2159 | `			 * same-named members from different visibility levels stay distinct` |
|        - | 2160 | ``			 * and `isset($arr['priv'])` is FALSE — PHL emitted the bare name,`` |
|        - | 2161 | `			 * which collided them and answered TRUE. The NULs are real bytes in` |
|        - | 2162 | `			 * the key (this append is length-based, not NUL-terminated). */` |
|      162 | 2163 | `			pAttrName = &pAttr->pAttr->sName;` |
|      162 | 2164 | `			if( pAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       35 | 2165 | `				ph7_class *pDecl = pAttr->pAttr->pDeclClass` |
|       22 | 2166 | `					? pAttr->pAttr->pDeclClass : pThis->pClass;` |
|       24 | 2167 | `				PH7_MemObjStringAppend(&sName,"\0",1);` |
|       24 | 2168 | `				PH7_MemObjStringAppend(&sName,SyStringData(&pDecl->sName),SyStringLength(&pDecl->sName));` |
|       24 | 2169 | `				PH7_MemObjStringAppend(&sName,"\0",1);` |
|      151 | 2170 | `			}else if( pAttr->pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
|       23 | 2171 | `				PH7_MemObjStringAppend(&sName,"\0*\0",3);` |
|       10 | 2172 | `			}` |
|      162 | 2173 | `			PH7_MemObjStringAppend(&sName,pAttrName->zString,pAttrName->nByte);` |
|        - | 2174 | `			/* Perform the insertion */` |
|      162 | 2175 | `			PH7_HashmapInsert(pMap,&sName,pValue);` |
|        - | 2176 | `			/* Reset the string cursor */` |
|      162 | 2177 | `			SyBlobReset(&sName.sBlob);` |
|       79 | 2178 | `		}` |
|        4 | 2179 | `	}` |
|       94 | 2180 | `	PH7_MemObjRelease(&sName);` |
|       94 | 2181 | `	return SXRET_OK;` |
|        4 | 2182 | `}` |
|        - | 2183 | `/*` |
|        - | 2184 | ` * Iterate throw class attributes and invoke the given callback [i.e: xWalk()] for each` |
|        - | 2185 | ` * retrieved attribute.` |
|        - | 2186 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|        - | 2187 | ` * the attribute value in the callback body will not alter the real attribute value.` |
|        - | 2188 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|        - | 2189 | ` * a value different from PH7_OK.` |
|        - | 2190 | ` * Refer to [ph7_object_walk()] for more information.` |
|        - | 2191 | ` */` |
|      ! 0 | 2192 | `PH7_PRIVATE sxi32 PH7_ClassInstanceWalk(` |
|        - | 2193 | `	ph7_class_instance *pThis, /* Target object */` |
|        - | 2194 | `	int (*xWalk)(const char *,ph7_value *,void *), /* Walker callback */` |
|        - | 2195 | `	void *pUserData /* Last argument to xWalk() */` |
|        - | 2196 | `	)` |
|      ! 0 | 2197 | `{` |
|        - | 2198 | `	SyHashEntry *pEntry; /* Hash entry */` |
|        - | 2199 | `	VmClassAttr *pAttr;  /* Pointer to the attribute */` |
|        - | 2200 | `	ph7_value *pValue;   /* Attribute value */` |
|        - | 2201 | `	ph7_value sValue;    /* Copy of the attribute value */` |
|        - | 2202 | `	int rc;` |
|        - | 2203 | `	/* Reset the loop cursor */` |
|      ! 0 | 2204 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      ! 0 | 2205 | `	PH7_MemObjInit(pThis->pVm,&sValue);` |
|        - | 2206 | `	/* Start the walk process */` |
|      ! 0 | 2207 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|        - | 2208 | `		/* Point to the current attribute */` |
|      ! 0 | 2209 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      ! 0 | 2210 | `		if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_HIDDEN) ){` |
|        - | 2211 | `			/* Class-level members are not part of the object (php) */` |
|      ! 0 | 2212 | `			continue;` |
|        - | 2213 | `		}` |
|      ! 0 | 2214 | `		if( PH7_ClassAttrUninitialized(pAttr) ){` |
|      ! 0 | 2215 | `			continue; /* typed, never written: not there yet (php) */` |
|        - | 2216 | `		}` |
|        - | 2217 | `		/* Extract attribute value */` |
|      ! 0 | 2218 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|      ! 0 | 2219 | `		if( pValue ){` |
|      ! 0 | 2220 | `			PH7_MemObjLoad(pValue,&sValue);` |
|        - | 2221 | `			/* Invoke the supplied callback */` |
|      ! 0 | 2222 | `			rc =  xWalk(SyStringData(&pAttr->pAttr->sName),&sValue,pUserData);` |
|      ! 0 | 2223 | `			PH7_MemObjRelease(&sValue);` |
|      ! 0 | 2224 | `			if( rc != PH7_OK){` |
|        - | 2225 | `				/* User callback request an operation abort */` |
|      ! 0 | 2226 | `				return SXERR_ABORT;` |
|        - | 2227 | `			}` |
|      ! 0 | 2228 | `		}` |
|      ! 0 | 2229 | `	}` |
|        - | 2230 | `	/* All done */` |
|      ! 0 | 2231 | `	return SXRET_OK;` |
|      ! 0 | 2232 | `}` |
|        - | 2233 | `/*` |
|        - | 2234 | ` * The instance's attribute entry for a property name, INCLUDING the empty one.` |
|        - | 2235 | ` *` |
|        - | 2236 | ` * SyHashGet answers 0 for a zero-length key engine-wide, so a property named ""` |
|        - | 2237 | `` * — php's own `s:0:""` payload builds one, and every surface that walks hAttr`` |
|        - | 2238 | ` * shows it — can only be found by walking the list. Used by the presentation` |
|        - | 2239 | ` * surfaces that snapshot names and re-look-up each one before reading it (a PHP` |
|        - | 2240 | ` * 8.4 get hook may unset a property mid-walk), where the plain hash probe would` |
|        - | 2241 | ` * silently drop it from the output while var_dump, which walks directly, showed` |
|        - | 2242 | ` * it. The walk uses the hash's embedded loop cursor, so it must not run inside` |
|        - | 2243 | ` * another walk of the SAME table — the callers below re-enter only through the` |
|        - | 2244 | ` * hook dispatch, which happens after this returns.` |
|        - | 2245 | ` */` |
|      342 | 2246 | `PH7_PRIVATE SyHashEntry * PH7_ClassInstanceAttrEntry(ph7_class_instance *pThis,const char *zName,sxu32 nName)` |
|        5 | 2247 | `{` |
|        - | 2248 | `	SyHashEntry *pEntry;` |
|      347 | 2249 | `	if( nName > 0 ){` |
|      329 | 2250 | `		return SyHashGet(&pThis->hAttr,(const void *)zName,nName);` |
|        - | 2251 | `	}` |
|       19 | 2252 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|       29 | 2253 | `	while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 | 2254 | `		if( pEntry->nKeyLen == 0 ){` |
|       11 | 2255 | `			return pEntry;` |
|        - | 2256 | `		}` |
|        1 | 2257 | `	}` |
|        9 | 2258 | `	return 0;` |
|      176 | 2259 | `}` |
|        - | 2260 | `/*` |
|        - | 2261 | ` * Extract a class atrribute value.` |
|        - | 2262 | ` * Return a pointer to the attribute value on success. Otherwise NULL.` |
|        - | 2263 | ` * Note:` |
|        - | 2264 | ` *  Access to static and constant attribute is not allowed. That is,the function` |
|        - | 2265 | ` *  will return NULL in case someone (host-application code) try to extract` |
|        - | 2266 | ` *  a static/constant attribute.` |
|        - | 2267 | ` */` |
|  1583756 | 2268 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceFetchAttr(ph7_class_instance *pThis,const SyString *pName)` |
|        5 | 2269 | `{` |
|        - | 2270 | `	SyHashEntry *pEntry;` |
|        - | 2271 | `	VmClassAttr *pAttr;` |
|        - | 2272 | `	/* Query the attribute hashtable */` |
|  1583761 | 2273 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);` |
|  1583761 | 2274 | `	if( pEntry == 0 ){` |
|        - | 2275 | `		/* No such attribute */` |
|       80 | 2276 | `		return 0;` |
|        - | 2277 | `	}` |
|        - | 2278 | `	/* Point to the class atrribute */` |
|  1583683 | 2279 | `	pAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - | 2280 | `	/* Check if we are dealing with a static/constant attribute */` |
|  1583683 | 2281 | `	if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - | 2282 | `		/* Access is forbidden */` |
|      ! 0 | 2283 | `		return 0;` |
|        - | 2284 | `	}` |
|        - | 2285 | `	/* Return the attribute value */` |
|  1583683 | 2286 | `	return ExtractClassAttrValue(pThis->pVm,pAttr);` |
|   791888 | 2287 | `}` |
|        - | 2288 | `/*` |
|        - | 2289 | `` * Does a WRITE through `$obj[k]` land on this class's storage? php answers with`` |
|        - | 2290 | ` * the class's read_dimension handler: an internal class whose own handler hands` |
|        - | 2291 | ` * back the real element supports indirect modification, while everything routed` |
|        - | 2292 | ` * through zend_std_read_dimension — every userland ArrayAccess, and the SPL` |
|        - | 2293 | ` * classes that keep the standard handler (SplFixedArray, the SplDoublyLinkedList` |
|        - | 2294 | ` * family, SplObjectStorage) — gets a TEMPORARY back, so the write is lost and php` |
|        - | 2295 | ` * says so. The engine cannot tell those apart from the interface alone: both` |
|        - | 2296 | ` * implement ArrayAccess.` |
|        - | 2297 | ` *` |
|        - | 2298 | ` * PHL's equivalent evidence is the offsetGet that ANSWERS: the native one on a` |
|        - | 2299 | ` * class carrying PH7_CLASS_DIM_WRITABLE means the storage is reachable, and a` |
|        - | 2300 | ` * userland OVERRIDE of it means it is not — which is php's own rule for an` |
|        - | 2301 | ` * ArrayObject subclass (spl_array_read_dimension steps aside for a declared` |
|        - | 2302 | `` * offsetGet). A `&offsetGet` return is php's other yes: it hands back a reference,`` |
|        - | 2303 | ` * so the write reaches whatever it aliases.` |
|        - | 2304 | ` */` |
|      446 | 2305 | `PH7_PRIVATE int PH7_VmDimFetchWritable(ph7_class *pClass)` |
|        5 | 2306 | `{` |
|        - | 2307 | `	ph7_class_method *pGet;` |
|        - | 2308 | `	ph7_class *pCur;` |
|      451 | 2309 | `	if( pClass == 0 ){` |
|      ! 0 | 2310 | `		return FALSE;` |
|        - | 2311 | `	}` |
|      451 | 2312 | `	pGet = PH7_ClassExtractMethod(pClass,"offsetGet",sizeof("offsetGet")-1);` |
|      451 | 2313 | `	if( pGet == 0 ){` |
|      ! 0 | 2314 | `		return FALSE;` |
|        - | 2315 | `	}` |
|      451 | 2316 | `	if( pGet->sFunc.iFlags & VM_FUNC_REF_RETURN ){` |
|        5 | 2317 | `		return TRUE;` |
|        - | 2318 | `	}` |
|      447 | 2319 | `	if( (pGet->sFunc.iFlags & VM_FUNC_NATIVE) == 0 ){` |
|      159 | 2320 | `		return FALSE;` |
|        - | 2321 | `	}` |
|      434 | 2322 | `	for( pCur = pClass ; pCur ; pCur = pCur->pBase ){` |
|      342 | 2323 | `		if( pCur->iFlags & PH7_CLASS_DIM_WRITABLE ){` |
|      198 | 2324 | `			return TRUE;` |
|        - | 2325 | `		}` |
|       73 | 2326 | `	}` |
|       93 | 2327 | `	return FALSE;` |
|      228 | 2328 | `}` |
|        - | 2329 | `/*` |
|        - | 2330 | ` * The three accessors a VM_FUNC_NATIVE method body uses to reach its receiver.` |
|        - | 2331 | ` *` |
|        - | 2332 | ` * A native method is dispatched down the host-function path, so it is handed the` |
|        - | 2333 | ` * plain (ph7_context*, argc, argv) of any builtin — the receiver rides on the` |
|        - | 2334 | ` * context instead of occupying an argument slot. That is the whole point: the C` |
|        - | 2335 | `` * routine that used to be a global `__prefix_verb($target, ...)` thunk taking its`` |
|        - | 2336 | ` * target explicitly becomes a method whose target is $this.` |
|        - | 2337 | ` */` |
|        - | 2338 | `/*` |
|        - | 2339 | ` * The receiver, or NULL when the method was called statically (and always NULL in` |
|        - | 2340 | ` * a plain host function). Borrowed: the operand stack holds the reference for the` |
|        - | 2341 | ` * duration of the call, so the body must not unref it.` |
|        - | 2342 | ` */` |
|  1489602 | 2343 | `PH7_PRIVATE ph7_class_instance * PH7_ContextThis(ph7_context *pCtx)` |
|        5 | 2344 | `{` |
|  1489607 | 2345 | `	return pCtx->pThis;` |
|        5 | 2346 | `}` |
|        - | 2347 | `/*` |
|        - | 2348 | ` * The class the call was made THROUGH — php's late-static-binding target, so an` |
|        - | 2349 | ` * inherited native method sees the SUBCLASS here, not the class that declared it.` |
|        - | 2350 | ` * NULL in a plain host function.` |
|        - | 2351 | ` */` |
|      314 | 2352 | `PH7_PRIVATE ph7_class * PH7_ContextCalledClass(ph7_context *pCtx)` |
|        2 | 2353 | `{` |
|      316 | 2354 | `	return pCtx->pCalledClass;` |
|        2 | 2355 | `}` |
|        - | 2356 | `/*` |
|        - | 2357 | ` * The receiver as a ph7_value, so the body can use the ordinary object helpers` |
|        - | 2358 | ` * (ph7_object_fetch_attr, ph7_value_is_object, ph7_result_value, ...) instead of` |
|        - | 2359 | ` * reaching into ph7_class_instance. NULL when there is no receiver.` |
|        - | 2360 | ` *` |
|        - | 2361 | ` * The view ALIASES the instance without bumping its refcount: it lives exactly as` |
|        - | 2362 | ` * long as the call, during which the caller's reference is already keeping the` |
|        - | 2363 | ` * object alive, and VmReleaseCallContext drops the alias without unreffing. Handing` |
|        - | 2364 | ` * it to ph7_result_value() is safe — that copies through PH7_MemObjStore, which` |
|        - | 2365 | ` * takes its own reference.` |
|        - | 2366 | ` */` |
|     5584 | 2367 | `PH7_PRIVATE ph7_value * PH7_ContextThisValue(ph7_context *pCtx)` |
|        5 | 2368 | `{` |
|     5589 | 2369 | `	if( pCtx->pThis == 0 ){` |
|      ! 0 | 2370 | `		return 0;` |
|        - | 2371 | `	}` |
|     5589 | 2372 | `	if( !pCtx->bThisInit ){` |
|     5589 | 2373 | `		PH7_MemObjInit(pCtx->pVm,&pCtx->sThis);` |
|     5589 | 2374 | `		pCtx->sThis.x.pOther = pCtx->pThis;` |
|     5589 | 2375 | `		pCtx->sThis.iFlags = MEMOBJ_OBJ;` |
|     5589 | 2376 | `		pCtx->sThis.nIdx = SXU32_HIGH;` |
|     5589 | 2377 | `		pCtx->bThisInit = 1;` |
|     2792 | 2378 | `	}` |
|     5589 | 2379 | `	return &pCtx->sThis;` |
|     2797 | 2380 | `}` |
|        - | 2381 |  |
