# src/ph7/oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 966/1092 lines (88.46%)

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
|  1135258 |   14 | `PH7_PRIVATE ph7_class * PH7_NewRawClass(ph7_vm *pVm,const SyString *pName,sxu32 nLine)` |
|        5 |   15 | `{` |
|        - |   16 | `	ph7_class *pClass;` |
|        - |   17 | `	char *zName;` |
|        - |   18 | `	/* Allocate a new instance */` |
|  1135263 |   19 | `	pClass = (ph7_class *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class));` |
|  1135263 |   20 | `	if( pClass == 0 ){` |
|      ! 0 |   21 | `		return 0;` |
|        - |   22 | `	}` |
|        - |   23 | `	/* Zero the structure */` |
|  1135263 |   24 | `	SyZero(pClass,sizeof(ph7_class));` |
|        - |   25 | `	/* Duplicate class name */` |
|  1135263 |   26 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1135263 |   27 | `	if( zName == 0 ){` |
|      ! 0 |   28 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClass);` |
|      ! 0 |   29 | `		return 0;` |
|        - |   30 | `	}` |
|        - |   31 | `	/* Initialize fields */` |
|  1135263 |   32 | `	SyStringInitFromBuf(&pClass->sName,zName,pName->nByte);` |
|        - |   33 | `	/* php method names are CASE-INSENSITIVE ($o->FOO() finds foo(), and declaring` |
|        - |   34 | `	 * both is a redeclaration), so the method table matches on them the same way` |
|        - |   35 | `	 * hClass does for class names. Properties and class constants ARE case` |
|        - |   36 | `	 * sensitive in php, so hAttr keeps the default exact comparator. */` |
|  1135263 |   37 | `	SyHashInit(&pClass->hMethod,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|  1135263 |   38 | `	SyHashInit(&pClass->hAttr,&pVm->sAllocator,0,0);` |
|  1135263 |   39 | `	SyHashInit(&pClass->hConst,&pVm->sAllocator,0,0);` |
|  1135263 |   40 | `	SyHashInit(&pClass->hDerived,&pVm->sAllocator,0,0);` |
|  1135263 |   41 | `	SySetInit(&pClass->aInterface,&pVm->sAllocator,sizeof(ph7_class *));` |
|  1135263 |   42 | `	SySetInit(&pClass->aTrait,&pVm->sAllocator,sizeof(ph7_class *));` |
|  1135263 |   43 | `	SySetInit(&pClass->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|  1135263 |   44 | `	SySetInit(&pClass->aEnumCases,&pVm->sAllocator,sizeof(ph7_class_attr *));` |
|  1135263 |   45 | `	pClass->nLine = nLine;` |
|  1135263 |   46 | `	if( pVm->bCompilingBuiltin ){` |
|        - |   47 | `		/* Defined by an embedded builtin chunk: internal, no defining file.` |
|        - |   48 | `		 * Class compilers merge further flags with \|= so this survives. */` |
|  1130785 |   49 | `		pClass->iFlags \|= PH7_CLASS_INTERNAL;` |
|   565395 |   50 | `	}else{` |
|        - |   51 | `		/* Alias the VM-lifetime path dup on top of the include stack */` |
|     4483 |   52 | `		SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     4483 |   53 | `		if( pFile ){` |
|     4483 |   54 | `			SyStringDupPtr(&pClass->sFile,pFile);` |
|     2239 |   55 | `		}` |
|        - |   56 | `	}` |
|        - |   57 | `	/* All done */` |
|  1135263 |   58 | `	return pClass;` |
|   567634 |   59 | `}` |
|        - |   60 | `/*` |
|        - |   61 | ` * Allocate and initialize a new class attribute.` |
|        - |   62 | ` * Return a pointer to the class attribute on success. NULL otherwise.` |
|        - |   63 | ` */` |
|  2758262 |   64 | `PH7_PRIVATE ph7_class_attr * PH7_NewClassAttr(ph7_vm *pVm,const SyString *pName,sxu32 nLine,sxi32 iProtection,sxi32 iFlags)` |
|        5 |   65 | `{` |
|        - |   66 | `	ph7_class_attr *pAttr;` |
|        - |   67 | `	char *zName;` |
|  2758267 |   68 | `	pAttr = (ph7_class_attr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_attr));` |
|  2758267 |   69 | `	if( pAttr == 0 ){` |
|      ! 0 |   70 | `		return 0;` |
|        - |   71 | `	}` |
|        - |   72 | `	/* Zero the structure */` |
|  2758267 |   73 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|  2758267 |   74 | `	SySetInit(&pAttr->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|        - |   75 | `	/* Duplicate attribute name */` |
|  2758267 |   76 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  2758267 |   77 | `	if( zName == 0 ){` |
|      ! 0 |   78 | `		SyMemBackendPoolFree(&pVm->sAllocator,pAttr);` |
|      ! 0 |   79 | `		return 0;` |
|        - |   80 | `	}` |
|        - |   81 | `	/* Initialize fields */` |
|  2758267 |   82 | `	SySetInit(&pAttr->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|  2758267 |   83 | `	SySetInit(&pAttr->aUnionAlts,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|  2758267 |   84 | `	SyStringInitFromBuf(&pAttr->sName,zName,pName->nByte);` |
|  2758267 |   85 | `	pAttr->iProtection = iProtection;` |
|  2758267 |   86 | `	pAttr->nIdx = SXU32_HIGH;` |
|  2758267 |   87 | `	pAttr->iFlags = iFlags;` |
|  2758267 |   88 | `	pAttr->nLine = nLine;` |
|  2758267 |   89 | `	return pAttr;` |
|  1379136 |   90 | `}` |
|        - |   91 | `/*` |
|        - |   92 | ` * Allocate and initialize a new class method.` |
|        - |   93 | ` * Return a pointer to the class method on success. NULL otherwise` |
|        - |   94 | ` * This function associate with the newly created method an automatically generated` |
|        - |   95 | ` * random unique name.` |
|        - |   96 | ` */` |
|  6754596 |   97 | `PH7_PRIVATE ph7_class_method * PH7_NewClassMethod(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,sxu32 nLine,` |
|        - |   98 | `	sxi32 iProtection,sxi32 iFlags,sxi32 iFuncFlags)` |
|        5 |   99 | `{` |
|        - |  100 | `	ph7_class_method *pMeth;` |
|        - |  101 | `	SyHashEntry *pEntry;` |
|        - |  102 | `	SyString *pNamePtr;` |
|        - |  103 | `	char zSalt[10];` |
|        - |  104 | `	char *zName;` |
|        - |  105 | `	sxu32 nByte;` |
|        - |  106 | `	/* Allocate a new class method instance */` |
|  6754601 |  107 | `	pMeth = (ph7_class_method *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_method));` |
|  6754601 |  108 | `	if( pMeth == 0 ){` |
|      ! 0 |  109 | `		return 0;` |
|        - |  110 | `	}` |
|        - |  111 | `	/* Zero the structure */` |
|  6754601 |  112 | `	SyZero(pMeth,sizeof(ph7_class_method));` |
|        - |  113 | `	/* Check for an already installed method with the same name */` |
|  6754601 |  114 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
|  6754601 |  115 | `	if( pEntry == 0 ){` |
|        - |  116 | `		/* Associate an unique VM name to this method */` |
|  6754597 |  117 | `		nByte = sizeof(zSalt) + pName->nByte + SyStringLength(&pClass->sName)+sizeof(char)*7/*[[__'\0'*/;` |
|  6754597 |  118 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,nByte);` |
|  6754597 |  119 | `		if( zName == 0 ){` |
|      ! 0 |  120 | `			SyMemBackendPoolFree(&pVm->sAllocator,pMeth);` |
|      ! 0 |  121 | `			return 0;` |
|        - |  122 | `		}` |
|  6754597 |  123 | `		pNamePtr = &pMeth->sVmName;` |
|        - |  124 | `		/* Generate a random string */` |
|  6754597 |  125 | `		PH7_VmRandomString(&(*pVm),zSalt,sizeof(zSalt));` |
|  6754597 |  126 | `		pNamePtr->nByte = SyBufferFormat(zName,nByte,"[__%z@%z_%.*s]",&pClass->sName,pName,sizeof(zSalt),zSalt);` |
|  6754597 |  127 | `		pNamePtr->zString = zName;` |
|  3377301 |  128 | `	}else{` |
|        - |  129 | `		/* Method is condidate for 'overloading' */` |
|        6 |  130 | `		ph7_class_method *pCurrent = (ph7_class_method *)pEntry->pUserData;` |
|        6 |  131 | `		pNamePtr = &pMeth->sVmName;` |
|        - |  132 | `		/* Use the same VM name */` |
|        6 |  133 | `		SyStringDupPtr(pNamePtr,&pCurrent->sVmName);` |
|        6 |  134 | `		zName = (char *)pNamePtr->zString;` |
|        - |  135 | `	}` |
|  6754601 |  136 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|    92109 |  137 | `		if( pName->nByte == sizeof("__destruct") - 1 && SyMemcmp(pName->zString,"__destruct",sizeof("__destruct") - 1 ) == 0 ){` |
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
|    46052 |  148 | `	}` |
|        - |  149 | `	/* Initialize method fields */` |
|  6754601 |  150 | `	pMeth->iProtection = iProtection;` |
|  6754601 |  151 | `	pMeth->iFlags = iFlags;` |
|  6754601 |  152 | `	pMeth->nLine = nLine;` |
| 10131899 |  153 | `	PH7_VmInitFuncState(&(*pVm),&pMeth->sFunc,&zName[sizeof(char)*4/*[__@*/+SyStringLength(&pClass->sName)],` |
|  6754596 |  154 | `		pName->nByte,iFuncFlags\|VM_FUNC_CLASS_METHOD,pClass);` |
|  6754601 |  155 | `	return pMeth;` |
|  3377303 |  156 | `}` |
|        - |  157 | `/*` |
|        - |  158 | ` * Check if the given name have a class method associated with it.` |
|        - |  159 | ` * Return the desired method [i.e: ph7_class_method instance] on success. NULL otherwise.` |
|        - |  160 | ` */` |
|  7168162 |  161 | `PH7_PRIVATE ph7_class_method * PH7_ClassExtractMethod(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|        5 |  162 | `{` |
|        - |  163 | `	SyHashEntry *pEntry;` |
|        - |  164 | `	/* Perform a hash lookup */` |
|  7168167 |  165 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)zName,nByte);` |
|  7168167 |  166 | `	if( pEntry == 0 ){` |
|        - |  167 | `		/* No such entry */` |
|  1798436 |  168 | `		return 0;` |
|        - |  169 | `	}` |
|        - |  170 | `	/* Point to the desired method */` |
|  5369736 |  171 | `	return (ph7_class_method *)pEntry->pUserData;` |
|  3584090 |  172 | `}` |
|        - |  173 | `/*` |
|        - |  174 | ` * Check if the given name is a class attribute.` |
|        - |  175 | ` * Return the desired attribute [i.e: ph7_class_attr instance] on success.NULL otherwise.` |
|        - |  176 | ` */` |
|   107159 |  177 | `PH7_PRIVATE ph7_class_attr * PH7_ClassExtractAttribute(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|        5 |  178 | `{` |
|        - |  179 | `	SyHashEntry *pEntry;` |
|        - |  180 | `	/* Perform a hash lookup */` |
|   107164 |  181 | `	pEntry = SyHashGet(&pClass->hAttr,(const void *)zName,nByte);` |
|   107164 |  182 | `	if( pEntry == 0 ){` |
|        - |  183 | `		/* No such entry */` |
|     6598 |  184 | `		return 0;` |
|        - |  185 | `	}` |
|        - |  186 | `	/* Point to the desierd method */` |
|   100571 |  187 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|    53585 |  188 | `}` |
|        - |  189 | `/*` |
|        - |  190 | ` * Check if the given name is a class CONSTANT (or enum case).` |
|        - |  191 | ` * php keeps constants and properties in separate namespaces, so constants live` |
|        - |  192 | ` * in a dedicated table (hConst) and never collide with a same-named property.` |
|        - |  193 | ` * Return the desired constant [ph7_class_attr with PH7_CLASS_ATTR_CONSTANT] on` |
|        - |  194 | ` * success, NULL otherwise.` |
|        - |  195 | ` */` |
|     2720 |  196 | `PH7_PRIVATE ph7_class_attr * PH7_ClassExtractConstant(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|        5 |  197 | `{` |
|        - |  198 | `	SyHashEntry *pEntry;` |
|     2725 |  199 | `	pEntry = SyHashGet(&pClass->hConst,(const void *)zName,nByte);` |
|     2725 |  200 | `	if( pEntry == 0 ){` |
|      513 |  201 | `		return 0;` |
|        - |  202 | `	}` |
|     2217 |  203 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|     1365 |  204 | `}` |
|        - |  205 | `/*` |
|        - |  206 | ` * Install a class attribute in the corresponding container.` |
|        - |  207 | ` * A constant (or enum case) goes to hConst, a property to hAttr — php's two` |
|        - |  208 | `` * separate member namespaces, so `const C` and `public $C` coexist.`` |
|        - |  209 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|        - |  210 | ` */` |
|  2758258 |  211 | `PH7_PRIVATE sxi32 PH7_ClassInstallAttr(ph7_class *pClass,ph7_class_attr *pAttr)` |
|        5 |  212 | `{` |
|  2758263 |  213 | `	SyString *pName = &pAttr->sName;` |
|        - |  214 | `	sxi32 rc;` |
|        - |  215 | `	/* Remember where this attribute was originally declared so that later` |
|        - |  216 | `	 * inheritance/trait copies still know the declaring class (needed for` |
|        - |  217 | `	 * PHP-compatible error messages on typed properties). */` |
|  2758263 |  218 | `	if( pAttr->pDeclClass == 0 ){` |
|    26023 |  219 | `		pAttr->pDeclClass = pClass;` |
|    13009 |  220 | `	}` |
|  2758263 |  221 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|  1280649 |  222 | `		rc = SyHashInsertTail(&pClass->hConst,(const void *)pName->zString,pName->nByte,pAttr);` |
|   640327 |  223 | `	}else{` |
|  1477619 |  224 | `		rc = SyHashInsertTail(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|        - |  225 | `	}` |
|  2758263 |  226 | `	return rc;` |
|        5 |  227 | `}` |
|        - |  228 | `/*` |
|        - |  229 | ` * Install a class method in the corresponding container.` |
|        - |  230 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|        - |  231 | ` */` |
|  6754558 |  232 | `PH7_PRIVATE sxi32 PH7_ClassInstallMethod(ph7_class *pClass,ph7_class_method *pMeth)` |
|        5 |  233 | `{` |
|  6754563 |  234 | `	SyString *pName = &pMeth->sFunc.sName;` |
|        - |  235 | `	sxi32 rc;` |
|  6754563 |  236 | `	rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|  6754563 |  237 | `	return rc;` |
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
|      376 |  262 | `static int OoClassifyOverrideType(ph7_vm *pVm, sxu32 nType, const SyString *pClass,` |
|        - |  263 | `	int bUnion, ph7_class **ppClass)` |
|        5 |  264 | `{` |
|      381 |  265 | `	*ppClass = 0;` |
|      381 |  266 | `	if( bUnion ){` |
|        3 |  267 | `		return OVT_SKIP; /* union/intersection — full lattice, skip */` |
|        - |  268 | `	}` |
|      379 |  269 | `	if( nType == 0 ){` |
|      220 |  270 | `		return OVT_NONE; /* no declared type */` |
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
|      193 |  302 | `}` |
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
|      280 |  316 | `static OvType OoTypeFromReturn(ph7_vm_func *pF)` |
|        5 |  317 | `{` |
|        - |  318 | `	OvType t;` |
|      285 |  319 | `	t.nType = pF->nReturnType;` |
|      285 |  320 | `	t.pClass = &pF->sReturnClass;` |
|      285 |  321 | `	t.bUnion = SySetUsed(&pF->aReturnUnion) > 0;` |
|      285 |  322 | `	t.bNullable = (pF->iFlags & VM_FUNC_RETURN_NULLABLE) != 0;` |
|      285 |  323 | `	return t;` |
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
|      188 |  340 | `static int OoOverrideTypeBad(ph7_vm *pVm, OvType parent, OvType child, int bCovariant)` |
|        5 |  341 | `{` |
|        - |  342 | `	ph7_class *pParentCls, *pChildCls;` |
|      193 |  343 | `	int kP = OoClassifyOverrideType(pVm, parent.nType, parent.pClass, parent.bUnion, &pParentCls);` |
|      193 |  344 | `	int kC = OoClassifyOverrideType(pVm, child.nType, child.pClass, child.bUnion, &pChildCls);` |
|      193 |  345 | `	if( kP == OVT_SKIP \|\| kC == OVT_SKIP ){` |
|       31 |  346 | `		return 0; /* ambiguous shape — conservatively accept */` |
|        - |  347 | `	}` |
|        - |  348 | `	/* A missing type is the TOP type. covariant (return): a concrete child is a` |
|        - |  349 | `	 * subtype of top, fine; a top child over a concrete parent WIDENS → bad.` |
|        - |  350 | `	 * contravariant (param): a top child is a supertype of anything, fine; a` |
|        - |  351 | `	 * concrete child over a top parent NARROWS → bad. (A union/intersection child` |
|        - |  352 | `	 * already fell into OVT_SKIP above, so a flagged child here is scalar/class.) */` |
|      165 |  353 | `	if( kP == OVT_NONE \|\| kC == OVT_NONE ){` |
|      114 |  354 | `		if( bCovariant && kC == OVT_NONE && kP != OVT_NONE ) return 1;` |
|      114 |  355 | `		if( !bCovariant && kP == OVT_NONE && kC != OVT_NONE ) return 1;` |
|      114 |  356 | `		return 0;` |
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
|       99 |  377 | `}` |
|        - |  378 |  |
|        - |  379 | `/*` |
|        - |  380 | ` * Check a child method's signature against the parent method it overrides.` |
|        - |  381 | ` * Emits a PHP-style "Declaration of … must be compatible …" fatal on a clear` |
|        - |  382 | `` * incompatibility. `__construct` is exempt (PHP does not apply variance to it).`` |
|        - |  383 | ` */` |
|   534122 |  384 | `static sxi32 OoCheckOverrideCompat(ph7_gen_state *pGen, ph7_class *pBase, ph7_class *pSub,` |
|        - |  385 | `	ph7_class_method *pParent, ph7_class_method *pChild)` |
|        5 |  386 | `{` |
|   534127 |  387 | `	ph7_vm *pVm = pGen->pVm;` |
|   534127 |  388 | `	ph7_vm_func *pPF = &pParent->sFunc;` |
|   534127 |  389 | `	ph7_vm_func *pCF = &pChild->sFunc;` |
|   534127 |  390 | `	SyString *pMName = &pCF->sName;` |
|        - |  391 | `	ph7_vm_func_arg *aP, *aC;` |
|        - |  392 | `	sxu32 nPArg, nCArg, k;` |
|   534127 |  393 | `	int bBad = 0;` |
|   534122 |  394 | `	if( pMName->nByte == sizeof("__construct")-1` |
|   347464 |  395 | `	 && SyStrnmicmp(pMName->zString,"__construct",pMName->nByte) == 0 ){` |
|   149297 |  396 | `		return SXRET_OK;` |
|        - |  397 | `	}` |
|        - |  398 | `	/*` |
|        - |  399 | `	 * A NATIVE method declares its parameters in a zSig STRING, so its aArgs set` |
|        - |  400 | `	 * is empty and there is nothing here to compare against. Reading that as` |
|        - |  401 | `	 * "declares no parameters" made every override of one incompatible: a user` |
|        - |  402 | `	 * class extending DOMDocument, a Reflection class or a native enum's own` |
|        - |  403 | `	 * cases()/from() all fataled on a declaration php accepts. An` |
|        - |  404 | `	 * engine-declared signature is compatible by construction, on either side.` |
|        - |  405 | `	 */` |
|   384835 |  406 | `	if( ((pPF->iFlags \| pCF->iFlags) & VM_FUNC_NATIVE) != 0 ){` |
|   384695 |  407 | `		return SXRET_OK;` |
|        - |  408 | `	}` |
|        - |  409 | `	/* Return type — covariant. */` |
|      145 |  410 | `	bBad = OoOverrideTypeBad(pVm, OoTypeFromReturn(pPF), OoTypeFromReturn(pCF), /* bCovariant */ 1);` |
|        - |  411 | `	/* Each overlapping parameter — contravariant. */` |
|      145 |  412 | `	nPArg = SySetUsed(&pPF->aArgs);` |
|      145 |  413 | `	nCArg = SySetUsed(&pCF->aArgs);` |
|      145 |  414 | `	aP = (ph7_vm_func_arg *)SySetBasePtr(&pPF->aArgs);` |
|      145 |  415 | `	aC = (ph7_vm_func_arg *)SySetBasePtr(&pCF->aArgs);` |
|      193 |  416 | `	for( k = 0; !bBad && k < nPArg && k < nCArg; k++ ){` |
|       51 |  417 | `		bBad = OoOverrideTypeBad(pVm, OoTypeFromArg(&aP[k]), OoTypeFromArg(&aC[k]), /* bCovariant */ 0);` |
|       27 |  418 | `	}` |
|        - |  419 | `	/* Parameter arity: the child must declare at least the parent's parameters and` |
|        - |  420 | `	 * may add only OPTIONAL ones — PHP rejects dropping any param (even an optional` |
|        - |  421 | `	 * one) or adding a required one. Skip the rule if either signature is variadic` |
|        - |  422 | `	 * (arity semantics differ). */` |
|      145 |  423 | `	if( !bBad ){` |
|      140 |  424 | `		int bVariadic = 0;` |
|      186 |  425 | `		for( k = 0; k < nPArg; k++ ){ if( aP[k].iFlags & VM_FUNC_ARG_VARIADIC ) bVariadic = 1; }` |
|      188 |  426 | `		for( k = 0; k < nCArg; k++ ){ if( aC[k].iFlags & VM_FUNC_ARG_VARIADIC ) bVariadic = 1; }` |
|      140 |  427 | `		if( !bVariadic ){` |
|      140 |  428 | `			if( nCArg < nPArg ){` |
|      ! 0 |  429 | `				bBad = 1; /* dropped a parent parameter */` |
|      ! 0 |  430 | `			}else{` |
|      142 |  431 | `				for( k = nPArg; k < nCArg; k++ ){` |
|        3 |  432 | `					if( SySetUsed(&aC[k].aByteCode) == 0 ){ bBad = 1; break; } /* new required */` |
|        2 |  433 | `				}` |
|        - |  434 | `			}` |
|       68 |  435 | `		}` |
|       68 |  436 | `	}` |
|      145 |  437 | `	if( bBad ){` |
|        8 |  438 | `		sxi32 rc = PH7_GenCompileError(&(*pGen),E_ERROR,pChild->nLine,` |
|        - |  439 | `			"Declaration of %z::%z() must be compatible with %z::%z()",` |
|        2 |  440 | `			&pSub->sName,pMName,&pBase->sName,&pParent->sFunc.sName);` |
|        6 |  441 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  442 | `			return SXERR_ABORT;` |
|        - |  443 | `		}` |
|        2 |  444 | `	}` |
|      145 |  445 | `	return SXRET_OK;` |
|   267066 |  446 | `}` |
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
|   517408 |  488 | `PH7_PRIVATE sxi32 PH7_ClassInherit(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase)` |
|        5 |  489 | `{` |
|        - |  490 | `	ph7_class_method *pMeth;` |
|        - |  491 | `	ph7_class_attr *pAttr;` |
|        - |  492 | `	SyHashEntry *pEntry;` |
|        - |  493 | `	SyString *pName;` |
|        - |  494 | `	SySet aInherited; /* base attributes to prepend (see the copy loop below) */` |
|        - |  495 | `	sxi32 rc;` |
|   517413 |  496 | `	SySetInit(&aInherited,&pGen->pVm->sAllocator,sizeof(ph7_class_attr *));` |
|        - |  497 | `	/* Install in the derived hashtable */` |
|   517413 |  498 | `	rc = SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|   517413 |  499 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  500 | `		SySetRelease(&aInherited);` |
|      ! 0 |  501 | `		return rc;` |
|        - |  502 | `	}` |
|        - |  503 | `	/* readonly class inheritance (PHP 8.2): a readonly class may only extend a` |
|        - |  504 | `	 * readonly class, and a non-readonly class may not extend a readonly one.` |
|        - |  505 | `	 * A FINAL base is not one of these cases at all -- it cannot be extended by` |
|        - |  506 | `	 * anything, and php reports only that. Both diagnostics used to fire for a` |
|        - |  507 | ``	 * `final readonly` base and the readonly one was reported, which is the wrong`` |
|        - |  508 | `	 * reason; BcMath\Number is the engine's first such class. */` |
|   517408 |  509 | `	if( (pBase->iFlags & PH7_CLASS_FINAL) == 0` |
|   517411 |  510 | `	 && (pBase->iFlags & PH7_CLASS_READONLY) != (pSub->iFlags & PH7_CLASS_READONLY) ){` |
|       10 |  511 | `		if( pBase->iFlags & PH7_CLASS_READONLY ){` |
|        8 |  512 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|        - |  513 | `				"Non-readonly class %z cannot extend readonly class %z",` |
|        2 |  514 | `				&pSub->sName,&pBase->sName);` |
|        4 |  515 | `		}else{` |
|        8 |  516 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|        - |  517 | `				"Readonly class %z cannot extend non-readonly class %z",` |
|        2 |  518 | `				&pSub->sName,&pBase->sName);` |
|        - |  519 | `		}` |
|       10 |  520 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  521 | `			SySetRelease(&aInherited);` |
|      ! 0 |  522 | `			return SXERR_ABORT;` |
|        - |  523 | `		}` |
|        4 |  524 | `	}` |
|        - |  525 | `	/* A native class whose php-visible properties are LAZY passes that on: the` |
|        - |  526 | `	 * attributes copied below keep their flags, so a subclass of DateInterval has` |
|        - |  527 | `	 * the same ten to install, and the O(1) gate in front of the materialization` |
|        - |  528 | `	 * walk has to see it on the SUBCLASS or the constructor's writes land nowhere. */` |
|   517413 |  529 | `	if( pBase->iFlags & PH7_CLASS_LAZY_ATTR ){` |
|       27 |  530 | `		pSub->iFlags \|= PH7_CLASS_LAZY_ATTR;` |
|       13 |  531 | `	}` |
|        - |  532 | `	/* Copy public/protected attributes from the base class */` |
|   517413 |  533 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
|  3447035 |  534 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|        - |  535 | `		/* Make sure the private attributes are not redeclared in the subclass */` |
|  2929627 |  536 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  2929627 |  537 | `		pName = &pAttr->sName;` |
|  2929627 |  538 | `		if( (pEntry = SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|    23000 |  539 | `			if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_FINAL))` |
|    11505 |  540 | `				== (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_FINAL) ){` |
|        - |  541 | `				/* Cannot override a final class constant (PHP 8.1). Report the` |
|        - |  542 | `				 * class that originally declared it (pDeclClass) rather than the` |
|        - |  543 | `				 * immediate base, so a multi-level chain matches PHP. */` |
|      ! 0 |  544 | `				ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pBase;` |
|      ! 0 |  545 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_attr *)pEntry->pUserData)->nLine,` |
|        - |  546 | `					"%z::%z cannot override final constant %z::%z",` |
|      ! 0 |  547 | `					&pSub->sName,pName,&pOwner->sName,pName);` |
|      ! 0 |  548 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  549 | `					SySetRelease(&aInherited);` |
|      ! 0 |  550 | `					return SXERR_ABORT;` |
|        - |  551 | `				}` |
|      ! 0 |  552 | `			}` |
|        - |  553 | `			/* A child MAY redeclare a base's private property: php treats the two` |
|        - |  554 | `			 * as independent members (each private to its declaring class), with no` |
|        - |  555 | `			 * diagnostic. PH7 warned here, which is wrong — the child's entry simply` |
|        - |  556 | `			 * shadows the base's in the by-name attribute table.` |
|        - |  557 | `			 *` |
|        - |  558 | `			 * Ordering: php keeps an overridden INSTANCE property at the position` |
|        - |  559 | `` 			 * the BASE declared it (`class G{$g1;$g2;} class H extends G{$h;$g1;}` `` |
|        - |  560 | `			 * iterates g1,g2,h — not g2,h,g1). Re-collect the CHILD's definition,` |
|        - |  561 | `			 * whose default value wins, and drop its current entry so the prepend` |
|        - |  562 | `			 * below re-inserts it in base order. Statics/constants are not part of` |
|        - |  563 | `			 * instance iteration, so they keep their existing slot. */` |
|    23005 |  564 | `			if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|    23001 |  565 | `				ph7_class_attr *pOwn = (ph7_class_attr *)pEntry->pUserData;` |
|    23001 |  566 | `				SyHashDeleteEntry(&pSub->hAttr,(const void *)pName->zString,pName->nByte,0);` |
|    23001 |  567 | `				rc = SySetPut(&aInherited,(const void *)&pOwn);` |
|    23001 |  568 | `				if( rc != SXRET_OK ){` |
|      ! 0 |  569 | `					SySetRelease(&aInherited);` |
|      ! 0 |  570 | `					return rc;` |
|        - |  571 | `				}` |
|    11498 |  572 | `			}` |
|    23005 |  573 | `			continue;` |
|        - |  574 | `		}` |
|        - |  575 | `		/* Collect the attribute. php: a base class's private INSTANCE property` |
|        - |  576 | `		 * lives on every child instance too (its own methods read/write it` |
|        - |  577 | `		 * through $this on the child; the access check grants private access by` |
|        - |  578 | `		 * DECLARING class, so child methods and outsiders still can't touch it).` |
|        - |  579 | `		 * Private STATICS/CONSTANTS stay uncopied — base methods reach those` |
|        - |  580 | `		 * through self:: against the declaring class directly.` |
|        - |  581 | `		 *` |
|        - |  582 | `		 * These are gathered rather than installed here because php orders an` |
|        - |  583 | `		 * instance's properties BASE-DECLARED FIRST, then the subclass's own,` |
|        - |  584 | `		 * then trait members — while inheritance runs AFTER the subclass body` |
|        - |  585 | `		 * has already filled hAttr. They are prepended below. */` |
|  2906622 |  586 | `		if( pAttr->iProtection != PH7_CLASS_PROT_PRIVATE` |
|  2263243 |  587 | `		 \|\| (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|  2906623 |  588 | `			rc = SySetPut(&aInherited,(const void *)&pAttr);` |
|  2906623 |  589 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  590 | `				SySetRelease(&aInherited);` |
|      ! 0 |  591 | `				return rc;` |
|        - |  592 | `			}` |
|  1453309 |  593 | `		}` |
|        5 |  594 | `	}` |
|        - |  595 | `	/* Prepend the collected base attributes so hAttr reads base-first. The` |
|        - |  596 | `	 * table's iteration list is what every object-iteration consumer walks` |
|        - |  597 | `	 * (var_dump/print_r, get_object_vars, foreach, (array) casts, json_encode),` |
|        - |  598 | `	 * so this ordering is user-visible — json_encode emits its keys in exactly` |
|        - |  599 | `	 * this order. SyHashInsert is a HEAD insert, so walking the collected set` |
|        - |  600 | `	 * backwards leaves the base's own declaration order at the front. */` |
|   517413 |  601 | `	if( SySetUsed(&aInherited) > 0 ){` |
|   517089 |  602 | `		ph7_class_attr **apInherited = (ph7_class_attr **)SySetBasePtr(&aInherited);` |
|   517089 |  603 | `		sxu32 n = SySetUsed(&aInherited);` |
|  3446703 |  604 | `		while( n > 0 ){` |
|  2929619 |  605 | `			ph7_class_attr *pIn = apInherited[--n];` |
|  2929619 |  606 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pIn->sName.zString,pIn->sName.nByte,pIn);` |
|  2929619 |  607 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  608 | `				SySetRelease(&aInherited);` |
|      ! 0 |  609 | `				return rc;` |
|        - |  610 | `			}` |
|        5 |  611 | `		}` |
|   258542 |  612 | `	}` |
|        - |  613 | `	/* Inherit the base's CONSTANTS (separate namespace: hConst). Constants are not` |
|        - |  614 | `	 * part of instance iteration, so no base-first ordering dance is needed — a plain` |
|        - |  615 | `	 * copy of every constant the subclass did not itself redeclare. A subclass that` |
|        - |  616 | `	 * redeclares a base FINAL constant is a fatal (PHP 8.1). */` |
|   517413 |  617 | `	SyHashResetLoopCursor(&pBase->hConst);` |
|  1821597 |  618 | `	while((pEntry = SyHashGetNextEntry(&pBase->hConst)) != 0 ){` |
|        - |  619 | `		SyHashEntry *pOwn;` |
|  1304189 |  620 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  1304189 |  621 | `		pName = &pAttr->sName;` |
|  1304189 |  622 | `		if( (pOwn = SyHashGet(&pSub->hConst,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|        9 |  623 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_FINAL) ){` |
|        - |  624 | `				/* Cannot override a final class constant. Report the class that` |
|        - |  625 | `				 * originally declared it (pDeclClass) for a multi-level chain. */` |
|        3 |  626 | `				ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pBase;` |
|        4 |  627 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_attr *)pOwn->pUserData)->nLine,` |
|        - |  628 | `					"%z::%z cannot override final constant %z::%z",` |
|        1 |  629 | `					&pSub->sName,pName,&pOwner->sName,pName);` |
|        3 |  630 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  631 | `					SySetRelease(&aInherited);` |
|      ! 0 |  632 | `					return SXERR_ABORT;` |
|        - |  633 | `				}` |
|        1 |  634 | `			}` |
|        9 |  635 | `			continue;` |
|        - |  636 | `		}` |
|  1304183 |  637 | `		rc = SyHashInsertTail(&pSub->hConst,(const void *)pName->zString,pName->nByte,pAttr);` |
|  1304183 |  638 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  639 | `			SySetRelease(&aInherited);` |
|      ! 0 |  640 | `			return rc;` |
|        - |  641 | `		}` |
|        5 |  642 | `	}` |
|   517413 |  643 | `	SySetRelease(&aInherited);` |
|   517413 |  644 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
|  9508557 |  645 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|        - |  646 | `		SyHashEntry *pOwn;` |
|        - |  647 | `		SyString sKey;` |
|        - |  648 | `		/* Make sure the private/final methods are not redeclared in the subclass.` |
|        - |  649 | `		 * The identity inherited is the base's HASH KEY, not sFunc.sName — the same` |
|        - |  650 | `		 * rule PH7_ClassUseTrait copies a trait by. A trait adaptation leaves the` |
|        - |  651 | `		 * composed class holding entries whose key is the name the class ANSWERS to` |
|        - |  652 | ``		 * while the method struct keeps its original name: `B::m as mB` is the key`` |
|        - |  653 | `` 		 * `mB` over a struct still called `m`, and `A::m insteadof B` is the key `m` `` |
|        - |  654 | ``		 * over A's struct. Keying the copy off sFunc.sName re-filed both under `m`,`` |
|        - |  655 | `		 * so a subclass of the composing class lost the alias entirely and took` |
|        - |  656 | ``		 * whichever of the two the hash walk reached last as its `m` — the insteadof`` |
|        - |  657 | `		 * choice, silently reversed. */` |
|  8991149 |  658 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  8991149 |  659 | `		SyStringInitFromBuf(&sKey,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|  8991149 |  660 | `		pName = &sKey;` |
|  8991149 |  661 | `		if( (pOwn = SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|   534143 |  662 | `			if( pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|        - |  663 | `				/* php: a base's PRIVATE method is never overridden — the child's` |
|        - |  664 | `				 * declaration is an independent member of the same name, so neither` |
|        - |  665 | `				 * the final rule nor the signature-compatibility rule applies to it` |
|        - |  666 | `				 * (zend's do_inherit_method skips both for a private parent). PHL` |
|        - |  667 | ``				 * ran both: `class A { private function m($a){} } class B extends A`` |
|        - |  668 | ``				 * { private function m($a,$b,$c){} }` was a fatal php compiles, and`` |
|        - |  669 | ``				 * `final private` in the base fataled every child that reused the`` |
|        - |  670 | `				 * name — php only WARNS at the final-private DECLARATION and lets` |
|        - |  671 | `				 * the child have the name. */` |
|   534137 |  672 | `			}else if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|        - |  673 | `				/* php: "Cannot override final method A::test()" */` |
|        8 |  674 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_method *)pOwn->pUserData)->nLine,` |
|        - |  675 | `					"Cannot override final method %z::%z()",` |
|        2 |  676 | `					&pBase->sName,pName);` |
|        2 |  677 | `				(void)pSub;` |
|        6 |  678 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  679 | `					return SXERR_ABORT;` |
|        - |  680 | `				}` |
|        4 |  681 | `			}else{` |
|        - |  682 | `				/* Check the override's signature is compatible with the parent's. */` |
|   801188 |  683 | `				rc = OoCheckOverrideCompat(&(*pGen),pBase,pSub,pMeth,` |
|   534122 |  684 | `					(ph7_class_method *)pOwn->pUserData);` |
|   534127 |  685 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  686 | `					return SXERR_ABORT;` |
|        - |  687 | `				}` |
|        - |  688 | `			}` |
|   534143 |  689 | `			continue;` |
|        - |  690 | `		}` |
|        - |  691 | `		/* Install the method. php: a base class's private method is in the child's` |
|        - |  692 | `		 * table too — an inherited public method calling $this->priv() must find it,` |
|        - |  693 | ``		 * and the LOOKUP has to find it for php's answer to `B::p()` to be`` |
|        - |  694 | `		 * "Call to private method A::p() from global scope" rather than` |
|        - |  695 | `		 * "Call to undefined method B::p()". The call-site visibility check binds by` |
|        - |  696 | `		 * DECLARING class (sFunc.pUserData), so child code and outsiders still cannot` |
|        - |  697 | ``		 * reach it; a private ctor copied down blocks `new Child` from outside like`` |
|        - |  698 | `		 * php's; and the surfaces that must NOT show an inherited private say so` |
|        - |  699 | `		 * themselves (method_exists, get_class_methods, ReflectionClass::getMethods).` |
|        - |  700 | `		 *` |
|        - |  701 | `		 * STATIC privates used to be skipped here, on the reasoning that base methods` |
|        - |  702 | `		 * reach them through self:: against the declaring class anyway. They do — but` |
|        - |  703 | ``		 * nothing else could: every spelling of `B::p()` (the call, `['B','p']()`,`` |
|        - |  704 | `		 * call_user_func, a first-class callable) reported the name as UNDEFINED, and` |
|        - |  705 | ``		 * `static::p()` from the base with a subclass as the late-static-binding`` |
|        - |  706 | `		 * target could not find its own method. */` |
|  8457011 |  707 | `		rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|  8457011 |  708 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  709 | `			return rc;` |
|        - |  710 | `		}` |
|        5 |  711 | `	}` |
|        - |  712 | `	/* Mark as subclass */` |
|   517413 |  713 | `	pSub->pBase = pBase;` |
|        - |  714 | `	/* All done */` |
|   517413 |  715 | `	return SXRET_OK;` |
|   258709 |  716 | `}` |
|        - |  717 | `/*` |
|        - |  718 | ` * Apply a trait to a class: copy all methods and attributes from the trait` |
|        - |  719 | ` * into the target class. Unlike inheritance, traits copy ALL members including` |
|        - |  720 | ` * private ones. Members already defined in the class take precedence.` |
|        - |  721 | ` */` |
|      162 |  722 | `PH7_PRIVATE sxi32 PH7_ClassUseTrait(ph7_gen_state *pGen,ph7_class *pClass,ph7_class *pTrait)` |
|        5 |  723 | `{` |
|        - |  724 | `	ph7_class_method *pMeth;` |
|        - |  725 | `	ph7_class_attr *pAttr;` |
|        - |  726 | `	SyHashEntry *pEntry;` |
|        - |  727 | `	SyString *pName;` |
|        - |  728 | `	sxi32 rc;` |
|        - |  729 | `	/* Detect cyclic trait composition (e.g. trait A { use B; } trait B { use A; }) */` |
|      167 |  730 | `	if( pTrait->iFlags & PH7_CLASS_TRAIT_VISITING ){` |
|      ! 0 |  731 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|      ! 0 |  732 | `			"Trait circular reference detected: %z is already being applied",&pTrait->sName);` |
|      ! 0 |  733 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  734 | `			return SXERR_ABORT;` |
|        - |  735 | `		}` |
|      ! 0 |  736 | `		return SXRET_OK;` |
|        - |  737 | `	}` |
|      167 |  738 | `	pTrait->iFlags \|= PH7_CLASS_TRAIT_VISITING;` |
|      167 |  739 | `	rc = SXRET_OK;` |
|        - |  740 | `	/* Copy attributes from the trait */` |
|      167 |  741 | `	SyHashResetLoopCursor(&pTrait->hAttr);` |
|      197 |  742 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hAttr)) != 0 ){` |
|        - |  743 | `		SyHashEntry *pExisting;` |
|       35 |  744 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|       35 |  745 | `		pName = &pAttr->sName;` |
|       35 |  746 | `		pExisting = SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte);` |
|       35 |  747 | `		if( pExisting != 0 ){` |
|        - |  748 | `			/* Attribute already exists. Check if it came from another trait` |
|        - |  749 | `			 * and whether the definitions are compatible (same defaults).` |
|        - |  750 | `			 */` |
|        - |  751 | `			ph7_class **apUsedTraits;` |
|        - |  752 | `			sxu32 nUsed,k;` |
|        6 |  753 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|        6 |  754 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|        6 |  755 | `			for(k = 0; k < nUsed; k++){` |
|        - |  756 | `				ph7_class_attr *pOther;` |
|        3 |  757 | `				pOther = PH7_ClassExtractAttribute(apUsedTraits[k],pName->zString,pName->nByte);` |
|        3 |  758 | `				if( pOther ){` |
|        - |  759 | `					/* Two traits define the same property — check if defaults differ */` |
|        3 |  760 | `					ph7_class_attr *pClassAttr = (ph7_class_attr *)pExisting->pUserData;` |
|        4 |  761 | `					if( SySetUsed(&pAttr->aByteCode) != SySetUsed(&pClassAttr->aByteCode) \|\|` |
|        3 |  762 | `						(SySetUsed(&pAttr->aByteCode) > 0 &&` |
|        3 |  763 | `						 SyMemcmp(SySetBasePtr(&pAttr->aByteCode),SySetBasePtr(&pClassAttr->aByteCode),` |
|        3 |  764 | `							SySetUsed(&pAttr->aByteCode) * SySetElemSize(&pAttr->aByteCode)) != 0) ){` |
|        4 |  765 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine,` |
|        - |  766 | `							"%z and %z define the same property ($%z) in the composition of %z. "` |
|        - |  767 | `							"However, the definition differs and is considered incompatible",` |
|        2 |  768 | `							&apUsedTraits[k]->sName,&pTrait->sName,pName,&pClass->sName);` |
|        3 |  769 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 |  770 | `							goto cleanup;` |
|        - |  771 | `						}` |
|        1 |  772 | `					}` |
|        3 |  773 | `					break;` |
|        - |  774 | `				}` |
|      ! 0 |  775 | `			}` |
|        6 |  776 | `			continue;` |
|        - |  777 | `		}` |
|       31 |  778 | `		rc = SyHashInsertTail(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|       31 |  779 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  780 | `			goto cleanup;` |
|        - |  781 | `		}` |
|        5 |  782 | `	}` |
|        - |  783 | `	/* Copy constants from the trait (PHP 8.2 trait constants; separate hConst` |
|        - |  784 | `	 * namespace). A constant already present in the class wins silently. */` |
|      167 |  785 | `	SyHashResetLoopCursor(&pTrait->hConst);` |
|      167 |  786 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hConst)) != 0 ){` |
|      ! 0 |  787 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      ! 0 |  788 | `		pName = &pAttr->sName;` |
|      ! 0 |  789 | `		if( SyHashGet(&pClass->hConst,(const void *)pName->zString,pName->nByte) != 0 ){` |
|      ! 0 |  790 | `			continue;` |
|        - |  791 | `		}` |
|      ! 0 |  792 | `		rc = SyHashInsertTail(&pClass->hConst,(const void *)pName->zString,pName->nByte,pAttr);` |
|      ! 0 |  793 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  794 | `			goto cleanup;` |
|        - |  795 | `		}` |
|      ! 0 |  796 | `	}` |
|        - |  797 | `	/* Copy methods from the trait. The identity copied is the trait's HASH KEY,` |
|        - |  798 | ``	 * not sFunc.sName: an adaptation alias (`hi as bHi`) is a hash entry whose`` |
|        - |  799 | `	 * key is the alias while the method struct keeps its original name — keying` |
|        - |  800 | ``	 * the copy off sFunc.sName silently re-filed `bHi` under `hi`, so an alias`` |
|        - |  801 | `	 * made inside a TRAIT vanished when that trait was composed into a class. */` |
|      167 |  802 | `	SyHashResetLoopCursor(&pTrait->hMethod);` |
|      431 |  803 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hMethod)) != 0 ){` |
|        - |  804 | `		SyHashEntry *pClassMethEntry;` |
|        - |  805 | `		SyString sKey;` |
|      269 |  806 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      269 |  807 | `		SyStringInitFromBuf(&sKey,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|      269 |  808 | `		pName = &sKey;` |
|      269 |  809 | `		pClassMethEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
|      269 |  810 | `		if( pClassMethEntry != 0 ){` |
|        - |  811 | `			/* Method already exists in the class. An ABSTRACT trait method is a` |
|        - |  812 | `			 * REQUIREMENT, not an implementation: php satisfies it with any concrete` |
|        - |  813 | `			 * method of the same name (from the class body or another trait) — no` |
|        - |  814 | `			 * collision. Only two CONCRETE trait methods actually conflict. */` |
|       20 |  815 | `			ph7_class_method *pExistingMeth = (ph7_class_method *)pClassMethEntry->pUserData;` |
|       20 |  816 | `			int bIncomingAbstract = (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0;` |
|       20 |  817 | `			int bExistingAbstract = (pExistingMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0;` |
|        - |  818 | `			ph7_class **apUsedTraits;` |
|        - |  819 | `			sxu32 nUsed,k;` |
|       20 |  820 | `			if( bIncomingAbstract ){` |
|        - |  821 | `				/* Incoming abstract requirement: the existing (concrete or abstract)` |
|        - |  822 | `				 * method already covers this name — keep it. */` |
|       13 |  823 | `				continue;` |
|        - |  824 | `			}` |
|       13 |  825 | `			if( bExistingAbstract ){` |
|        - |  826 | `				/* Existing entry is only an abstract requirement (from an earlier` |
|        - |  827 | `				 * trait): the incoming concrete method satisfies and replaces it. */` |
|        3 |  828 | `				pClassMethEntry->pUserData = (void *)pMeth;` |
|        3 |  829 | `				continue;` |
|        - |  830 | `			}` |
|        - |  831 | `			/* Both concrete: a genuine collision only when the OTHER definition came` |
|        - |  832 | `			 * from another trait (a concrete one). A class-body method wins silently. */` |
|       10 |  833 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       10 |  834 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|       10 |  835 | `			for(k = 0; k < nUsed; k++){` |
|        3 |  836 | `				ph7_class_method *pOtherMeth = PH7_ClassExtractMethod(apUsedTraits[k],pName->zString,pName->nByte);` |
|        3 |  837 | `				if( pOtherMeth != 0 && (pOtherMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|        - |  838 | `					/* Two different traits define the same CONCRETE method with no resolution */` |
|        4 |  839 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|        - |  840 | `						"Trait method %z::%z has not been applied as %z::%z, "` |
|        - |  841 | `						"because of collision with %z::%z",` |
|        2 |  842 | `						&pTrait->sName,pName,` |
|        1 |  843 | `						&pClass->sName,pName,` |
|        2 |  844 | `						&apUsedTraits[k]->sName,pName);` |
|        3 |  845 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  846 | `						goto cleanup;` |
|        - |  847 | `					}` |
|        3 |  848 | `					break;` |
|        - |  849 | `				}` |
|      ! 0 |  850 | `			}` |
|        - |  851 | `			/* Class-defined method takes precedence */` |
|       10 |  852 | `			continue;` |
|        - |  853 | `		}` |
|      253 |  854 | `		rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|      253 |  855 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  856 | `			goto cleanup;` |
|        - |  857 | `		}` |
|        5 |  858 | `	}` |
|        - |  859 | `	/* Record trait in the class */` |
|      167 |  860 | `	SySetPut(&pClass->aTrait,(const void *)&pTrait);` |
|       81 |  861 | `cleanup:` |
|        - |  862 | `	/* Always clear visiting flag, even on error paths */` |
|      167 |  863 | `	pTrait->iFlags &= ~PH7_CLASS_TRAIT_VISITING;` |
|       81 |  864 | `	SXUNUSED(pGen);` |
|      167 |  865 | `	return rc;` |
|       86 |  866 | `}` |
|        - |  867 | `/*` |
|        - |  868 | ` * Inherit an object interface from another object interface.` |
|        - |  869 | ` * According to the PHP language reference manual.` |
|        - |  870 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|        - |  871 | ` *  must implement, without having to define how these methods are handled.` |
|        - |  872 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|        - |  873 | ` *  class, but without any of the methods having their contents defined.` |
|        - |  874 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|        - |  875 | ` *` |
|        - |  876 | ` * This function return SXRET_OK if the interface inheritance operation was successfully performed.` |
|        - |  877 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|        - |  878 | ` * error message.` |
|        - |  879 | ` */` |
|    51684 |  880 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceInherit(ph7_class *pSub,ph7_class *pBase)` |
|        5 |  881 | `{` |
|        - |  882 | `	ph7_class_method *pMeth;` |
|        - |  883 | `	ph7_class_attr *pAttr;` |
|        - |  884 | `	SyHashEntry *pEntry;` |
|        - |  885 | `	SyString *pName;` |
|        - |  886 | `	sxi32 rc;` |
|        - |  887 | `	/* Install in the derived hashtable */` |
|    51689 |  888 | `	SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|    51689 |  889 | `	SyHashResetLoopCursor(&pBase->hConst);` |
|        - |  890 | `	/* Copy constants (interfaces carry only constants + method signatures) */` |
|    77533 |  891 | `	while((pEntry = SyHashGetNextEntry(&pBase->hConst)) != 0 ){` |
|        - |  892 | `		/* Make sure the constants are not redeclared in the subclass */` |
|        3 |  893 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|        3 |  894 | `		pName = &pAttr->sName;` |
|        3 |  895 | `		if( SyHashGet(&pSub->hConst,(const void *)pName->zString,pName->nByte) == 0 ){` |
|        - |  896 | `			/* Install the constant in the subclass */` |
|        3 |  897 | `			rc = SyHashInsertTail(&pSub->hConst,(const void *)pName->zString,pName->nByte,pAttr);` |
|        3 |  898 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  899 | `				return rc;` |
|        - |  900 | `			}` |
|        1 |  901 | `		}` |
|        1 |  902 | `	}` |
|    51689 |  903 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
|        - |  904 | `	/* Copy methods signature */` |
|   186651 |  905 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|        - |  906 | `		/* Make sure the method are not redeclared in the subclass */` |
|   109125 |  907 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   109125 |  908 | `		pName = &pMeth->sFunc.sName;` |
|   109125 |  909 | `		if( SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte) == 0 ){` |
|        - |  910 | `			/* Install the method */` |
|   109123 |  911 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|   109123 |  912 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  913 | `				return rc;` |
|        - |  914 | `			}` |
|    54559 |  915 | `		}` |
|        5 |  916 | `	}` |
|        - |  917 | `	/* Mark as subclass */` |
|    51689 |  918 | `	pSub->pBase = pBase;` |
|        - |  919 | `	/* All done */` |
|    51689 |  920 | `	return SXRET_OK;` |
|    25847 |  921 | `}` |
|        - |  922 | `/*` |
|        - |  923 | ` * Implements an object interface in the given main class.` |
|        - |  924 | ` * According to the PHP language reference manual.` |
|        - |  925 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|        - |  926 | ` *  must implement, without having to define how these methods are handled.` |
|        - |  927 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|        - |  928 | ` *  class, but without any of the methods having their contents defined.` |
|        - |  929 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|        - |  930 | ` *` |
|        - |  931 | ` * This function return SXRET_OK if the interface was successfully implemented.` |
|        - |  932 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|        - |  933 | ` * error message.` |
|        - |  934 | ` */` |
|   471468 |  935 | `PH7_PRIVATE sxi32 PH7_ClassImplement(ph7_class *pMain,ph7_class *pInterface)` |
|        5 |  936 | `{` |
|        - |  937 | `	ph7_class_attr *pAttr;` |
|        - |  938 | `	SyHashEntry *pEntry;` |
|        - |  939 | `	SyString *pName;` |
|        - |  940 | `	sxi32 rc;` |
|        - |  941 | `	/* First off,copy all constants declared inside the interface (hConst namespace) */` |
|   471473 |  942 | `	SyHashResetLoopCursor(&pInterface->hConst);` |
|   867945 |  943 | `	while((pEntry = SyHashGetNextEntry(&pInterface->hConst)) != 0 ){` |
|        - |  944 | `		/* Point to the constant declaration */` |
|   160743 |  945 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   160743 |  946 | `		pName = &pAttr->sName;` |
|        - |  947 | `		/* Make sure the constant is not redeclared in the main class */` |
|   160743 |  948 | `		if( SyHashGet(&pMain->hConst,pName->zString,pName->nByte) == 0 ){` |
|        - |  949 | `			/* Install the constant */` |
|   160743 |  950 | `			rc = SyHashInsertTail(&pMain->hConst,pName->zString,pName->nByte,pAttr);` |
|   160743 |  951 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  952 | `				return rc;` |
|        - |  953 | `			}` |
|    80369 |  954 | `		}` |
|        5 |  955 | `	}` |
|        - |  956 | `	/* Install in the interface container */` |
|   471473 |  957 | `	SySetPut(&pMain->aInterface,(const void *)&pInterface);` |
|        - |  958 | `	/* Install interface method stubs into the implementing class.` |
|        - |  959 | `	 * Methods already defined in the class take precedence (they satisfy` |
|        - |  960 | `	 * the interface contract). Stubs retain PH7_CLASS_ATTR_ABSTRACT so` |
|        - |  961 | `	 * the unified check in GenStateCheckAbstractMethods catches missing ones.` |
|        - |  962 | `	 */` |
|        - |  963 | `	{` |
|        - |  964 | `		ph7_class_method *pMeth;` |
|        - |  965 | `		SyHashEntry *pMEntry;` |
|        - |  966 | `		SyString *pMName;` |
|   471473 |  967 | `		SyHashResetLoopCursor(&pInterface->hMethod);` |
|  2063089 |  968 | `		while((pMEntry = SyHashGetNextEntry(&pInterface->hMethod)) != 0 ){` |
|  1355887 |  969 | `			pMeth = (ph7_class_method *)pMEntry->pUserData;` |
|  1355887 |  970 | `			pMName = &pMeth->sFunc.sName;` |
|  1355887 |  971 | `			if( SyHashGet(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte) == 0 ){` |
|     5767 |  972 | `				rc = SyHashInsert(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte,pMeth);` |
|     5767 |  973 | `				if( rc != SXRET_OK ){` |
|      ! 0 |  974 | `					return rc;` |
|        - |  975 | `				}` |
|     2881 |  976 | `			}` |
|        5 |  977 | `		}` |
|        - |  978 | `	}` |
|   471473 |  979 | `	return SXRET_OK;` |
|   235739 |  980 | `}` |
|        - |  981 | `/*` |
|        - |  982 | ` * Create a class instance [i.e: Object in the PHP jargon] at run-time.` |
|        - |  983 | ` * The following function is called when an object is created at run-time` |
|        - |  984 | ` * typically when the PH7_OP_NEW/PH7_OP_CLONE instructions are executed.` |
|        - |  985 | ` * Notes on object creation.` |
|        - |  986 | ` *` |
|        - |  987 | ` * According to PHP language reference manual.` |
|        - |  988 | ` * To create an instance of a class, the new keyword must be used. An object will always` |
|        - |  989 | ` * be created unless the object has a constructor defined that throws an exception on error.` |
|        - |  990 | ` * Classes should be defined before instantiation (and in some cases this is a requirement).` |
|        - |  991 | ` * If a string containing the name of a class is used with new, a new instance of that class` |
|        - |  992 | ` * will be created. If the class is in a namespace, its fully qualified name must be used when` |
|        - |  993 | ` * doing this.` |
|        - |  994 | ` * Example #3 Creating an instance` |
|        - |  995 | ` * <?php` |
|        - |  996 | ` *  $instance = new SimpleClass();` |
|        - |  997 | ` *   // This can also be done with a variable:` |
|        - |  998 | ` * $className = 'Foo';` |
|        - |  999 | ` * $instance = new $className(); // Foo()` |
|        - | 1000 | ` * ?>` |
|        - | 1001 | ` * In the class context, it is possible to create a new object by new self and new parent.` |
|        - | 1002 | ` * When assigning an already created instance of a class to a new variable, the new variable` |
|        - | 1003 | ` * will access the same instance as the object that was assigned. This behaviour is the same` |
|        - | 1004 | ` * when passing instances to a function. A copy of an already created object can be made by` |
|        - | 1005 | ` * cloning it.` |
|        - | 1006 | ` * Example #4 Object Assignment` |
|        - | 1007 | ` * <?php` |
|        - | 1008 | ` *  class SimpleClass(){` |
|        - | 1009 | ` *    public $var;` |
|        - | 1010 | ` *  };` |
|        - | 1011 | ` *  $instance = new SimpleClass();` |
|        - | 1012 | ` *  $assigned   =  $instance;` |
|        - | 1013 | ` *  $reference  =& $instance;` |
|        - | 1014 | ` *  $instance->var = '$assigned will have this value';` |
|        - | 1015 | ` *  $instance = null; // $instance and $reference become null` |
|        - | 1016 | ` *  var_dump($instance);` |
|        - | 1017 | ` *  var_dump($reference);` |
|        - | 1018 | ` *  var_dump($assigned);` |
|        - | 1019 | ` * ?>` |
|        - | 1020 | ` * The above example will output:` |
|        - | 1021 | ` * NULL` |
|        - | 1022 | ` * NULL` |
|        - | 1023 | ` * object(SimpleClass)#1 (1) {` |
|        - | 1024 | ` *  ["var"]=>` |
|        - | 1025 | ` *    string(30) "$assigned will have this value"` |
|        - | 1026 | ` * }` |
|        - | 1027 | ` * Example #5 Creating new objects` |
|        - | 1028 | ` * <?php` |
|        - | 1029 | ` * class Test` |
|        - | 1030 | ` * {` |
|        - | 1031 | ` *   static public function getNew()` |
|        - | 1032 | ` *   {` |
|        - | 1033 | ` *       return new static;` |
|        - | 1034 | ` *   }` |
|        - | 1035 | ` * }` |
|        - | 1036 | ` * class Child extends Test` |
|        - | 1037 | ` * {}` |
|        - | 1038 | ` * $obj1 = new Test();` |
|        - | 1039 | ` * $obj2 = new $obj1;` |
|        - | 1040 | ` * var_dump($obj1 !== $obj2);` |
|        - | 1041 | ` * $obj3 = Test::getNew();` |
|        - | 1042 | ` * var_dump($obj3 instanceof Test);` |
|        - | 1043 | ` * $obj4 = Child::getNew();` |
|        - | 1044 | ` * var_dump($obj4 instanceof Child);` |
|        - | 1045 | ` * ?>` |
|        - | 1046 | ` * The above example will output:` |
|        - | 1047 | ` * bool(true)` |
|        - | 1048 | ` * bool(true)` |
|        - | 1049 | ` * bool(true)` |
|        - | 1050 | ` * Note that Symisc Systems have introduced powerfull extension to` |
|        - | 1051 | ` * OO subsystem. For example a class attribute may have any complex` |
|        - | 1052 | ` * expression associated with it when declaring the attribute unlike` |
|        - | 1053 | ` * the standard PHP engine which would allow a single value.` |
|        - | 1054 | ` * Example:` |
|        - | 1055 | ` *  class myClass{` |
|        - | 1056 | ` *    public $var = 25<<1+foo()/bar();` |
|        - | 1057 | ` *  };` |
|        - | 1058 | ` * Refer to the official documentation for more information.` |
|        - | 1059 | ` */` |
|  1606267 | 1060 | `static ph7_class_instance * NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|        5 | 1061 | `{` |
|        - | 1062 | `	ph7_class_instance *pThis;` |
|        - | 1063 | `	/* Allocate a new instance */` |
|  1606272 | 1064 | `	pThis = (ph7_class_instance *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_instance));` |
|  1606272 | 1065 | `	if( pThis == 0 ){` |
|      ! 0 | 1066 | `		return 0;` |
|        - | 1067 | `	}` |
|        - | 1068 | `	/* Zero the structure */` |
|  1606272 | 1069 | `	SyZero(pThis,sizeof(ph7_class_instance));` |
|        - | 1070 | `	/* Initialize fields */` |
|  1606272 | 1071 | `	pThis->iRef = 1;` |
|  1606272 | 1072 | `	pThis->pVm = pVm;` |
|  1606272 | 1073 | `	pThis->pClass = pClass;` |
|        - | 1074 | `	/* Assign a fresh monotonic object handle id (clones get their own, like PHP). */` |
|  1606272 | 1075 | `	pThis->nObjId = pVm->nNextObjId++;` |
|  1606272 | 1076 | `	SyHashInit(&pThis->hAttr,&pVm->sAllocator,0,0);` |
|  1606272 | 1077 | `	return pThis;` |
|   803138 | 1078 | `}` |
|        - | 1079 | `/*` |
|        - | 1080 | ` * Wrapper around the NewClassInstance() function defined above.` |
|        - | 1081 | ` * See the block comment above for more information.` |
|        - | 1082 | ` */` |
|  1605215 | 1083 | `PH7_PRIVATE ph7_class_instance * PH7_NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|        5 | 1084 | `{` |
|        - | 1085 | `	ph7_class_instance *pNew;` |
|        - | 1086 | `	sxi32 rc;` |
|  1605220 | 1087 | `	pNew = NewClassInstance(&(*pVm),&(*pClass));` |
|  1605220 | 1088 | `	if( pNew == 0 ){` |
|      ! 0 | 1089 | `		return 0;` |
|        - | 1090 | `	}` |
|        - | 1091 | `	/* Associate a private VM frame with this class instance */` |
|  1605220 | 1092 | `	rc = PH7_VmCreateClassInstanceFrame(&(*pVm),pNew);` |
|  1605220 | 1093 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1094 | `		SyMemBackendPoolFree(&pVm->sAllocator,pNew);` |
|      ! 0 | 1095 | `		return 0;` |
|        - | 1096 | `	}` |
|        - | 1097 | `	/* php stamps a Throwable's file/line at CREATION, not in its constructor, so a` |
|        - | 1098 | `	 * subclass that overrides __construct without calling parent::__construct still` |
|        - | 1099 | `	 * reports the right site. Every instantiation path lands here. */` |
|  1605220 | 1100 | `	PH7_VmStampThrowableSite(&(*pVm),pNew);` |
|  1605220 | 1101 | `	return pNew;` |
|   802612 | 1102 | `}` |
|        - | 1103 | `/*` |
|        - | 1104 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon] attribute.` |
|        - | 1105 | ` * This function never fail.` |
|        - | 1106 | ` */` |
|  7693286 | 1107 | `static ph7_value * ExtractClassAttrValue(ph7_vm *pVm,VmClassAttr *pAttr)` |
|        5 | 1108 | `{` |
|        - | 1109 | `	/* Extract the value */` |
|        - | 1110 | `	ph7_value *pValue;` |
|  7693291 | 1111 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|  7693291 | 1112 | `	return pValue;` |
|        5 | 1113 | `}` |
|        - | 1114 | `/*` |
|        - | 1115 | ` * Perform a clone operation on a class instance [i.e: Object in the PHP jargon].` |
|        - | 1116 | ` * The following function is called when an object is cloned at run-time` |
|        - | 1117 | ` * typically when the PH7_OP_CLONE instruction is executed.` |
|        - | 1118 | ` * Notes on object cloning.` |
|        - | 1119 | ` *` |
|        - | 1120 | ` * According to PHP language reference manual.` |
|        - | 1121 | ` * Creating a copy of an object with fully replicated properties is not always the wanted behavior.` |
|        - | 1122 | ` * A good example of the need for copy constructors. Another example is if your object holds a reference` |
|        - | 1123 | ` * to another object which it uses and when you replicate the parent object you want to create` |
|        - | 1124 | ` * a new instance of this other object so that the replica has its own separate copy.` |
|        - | 1125 | ` * An object copy is created by using the clone keyword (which calls the object's __clone() method if possible).` |
|        - | 1126 | ` * An object's __clone() method cannot be called directly.` |
|        - | 1127 | ` * $copy_of_object = clone $object;` |
|        - | 1128 | ` * When an object is cloned, PHP 5 will perform a shallow copy of all of the object's properties.` |
|        - | 1129 | ` * Any properties that are references to other variables, will remain references.` |
|        - | 1130 | ` * Once the cloning is complete, if a __clone() method is defined, then the newly created object's __clone() method` |
|        - | 1131 | ` * will be called, to allow any necessary properties that need to be changed.` |
|        - | 1132 | ` * Example #1 Cloning an object` |
|        - | 1133 | ` * <?php` |
|        - | 1134 | ` * class SubObject` |
|        - | 1135 | ` * {` |
|        - | 1136 | ` *   static $instances = 0;` |
|        - | 1137 | ` *   public $instance;` |
|        - | 1138 | ` *` |
|        - | 1139 | ` *   public function __construct() {` |
|        - | 1140 | ` *       $this->instance = ++self::$instances;` |
|        - | 1141 | ` *   }` |
|        - | 1142 | ` *` |
|        - | 1143 | ` *   public function __clone() {` |
|        - | 1144 | ` *       $this->instance = ++self::$instances;` |
|        - | 1145 | ` *   }` |
|        - | 1146 | ` * }` |
|        - | 1147 | ` *` |
|        - | 1148 | ` * class MyCloneable` |
|        - | 1149 | ` * {` |
|        - | 1150 | ` *   public $object1;` |
|        - | 1151 | ` *   public $object2;` |
|        - | 1152 | ` *` |
|        - | 1153 | ` *   function __clone()` |
|        - | 1154 | ` *   {` |
|        - | 1155 | ` *       // Force a copy of this->object, otherwise` |
|        - | 1156 | ` *       // it will point to same object.` |
|        - | 1157 | ` *       $this->object1 = clone $this->object1;` |
|        - | 1158 | ` *   }` |
|        - | 1159 | ` * }` |
|        - | 1160 | ` * $obj = new MyCloneable();` |
|        - | 1161 | ` * $obj->object1 = new SubObject();` |
|        - | 1162 | ` * $obj->object2 = new SubObject();` |
|        - | 1163 | ` * $obj2 = clone $obj;` |
|        - | 1164 | ` * print("Original Object:\n");` |
|        - | 1165 | ` * print_r($obj);` |
|        - | 1166 | ` * print("Cloned Object:\n");` |
|        - | 1167 | ` * print_r($obj2);` |
|        - | 1168 | ` * ?>` |
|        - | 1169 | ` * The above example will output:` |
|        - | 1170 | ` * Original Object:` |
|        - | 1171 | ` * MyCloneable Object` |
|        - | 1172 | ` * (` |
|        - | 1173 | ` *   [object1] => SubObject Object` |
|        - | 1174 | ` *       (` |
|        - | 1175 | ` *           [instance] => 1` |
|        - | 1176 | ` *       )` |
|        - | 1177 | ` *` |
|        - | 1178 | ` *   [object2] => SubObject Object` |
|        - | 1179 | ` *       (` |
|        - | 1180 | ` *           [instance] => 2` |
|        - | 1181 | ` *       )` |
|        - | 1182 | ` *` |
|        - | 1183 | ` * )` |
|        - | 1184 | ` * Cloned Object:` |
|        - | 1185 | ` * MyCloneable Object` |
|        - | 1186 | ` * (` |
|        - | 1187 | ` *   [object1] => SubObject Object` |
|        - | 1188 | ` *       (` |
|        - | 1189 | ` *           [instance] => 3` |
|        - | 1190 | ` *       )` |
|        - | 1191 | ` *` |
|        - | 1192 | ` *   [object2] => SubObject Object` |
|        - | 1193 | ` *       (` |
|        - | 1194 | ` *           [instance] => 2` |
|        - | 1195 | ` *       )` |
|        - | 1196 | ` * )` |
|        - | 1197 | ` */` |
|        - | 1198 | `/*` |
|        - | 1199 | `` * Is `clone` refused for this class? php's uncloneable internal classes refuse`` |
|        - | 1200 | `` * for their USER SUBCLASSES too -- `class M extends IteratorIterator {}` makes`` |
|        - | 1201 | `` * `clone $m` the same catchable Error, named after M -- because the refusal is`` |
|        - | 1202 | ` * the inherited clone_obj handler, not the class's own row. So the flag is` |
|        - | 1203 | ` * consulted up the base chain, not on the instance's class alone. (A subclass` |
|        - | 1204 | ` * declaring its own __clone() changes nothing there either: php never reaches` |
|        - | 1205 | ` * it, and neither does this engine -- the refusal answers first.)` |
|        - | 1206 | ` */` |
|      458 | 1207 | `PH7_PRIVATE int PH7_ClassIsUncloneable(ph7_class *pClass)` |
|        5 | 1208 | `{` |
|        - | 1209 | `	ph7_class *pC;` |
|      855 | 1210 | `	for( pC = pClass ; pC ; pC = pC->pBase ){` |
|      529 | 1211 | `		if( pC->iFlags & PH7_CLASS_NOCLONE ){` |
|      134 | 1212 | `			return 1;` |
|        - | 1213 | `		}` |
|      201 | 1214 | `	}` |
|      331 | 1215 | `	return 0;` |
|      234 | 1216 | `}` |
|     1052 | 1217 | `PH7_PRIVATE ph7_class_instance * PH7_CloneClassInstance(ph7_class_instance *pSrc)` |
|        5 | 1218 | `{` |
|        - | 1219 | `	ph7_class_instance *pClone;` |
|        - | 1220 | `	ph7_class_method *pMethod;` |
|        - | 1221 | `	SyHashEntry *pEntry2;` |
|        - | 1222 | `	SyHashEntry *pEntry;` |
|        - | 1223 | `	ph7_vm *pVm;` |
|        - | 1224 | `	sxi32 rc;` |
|        - | 1225 | `	/* Allocate a new instance */` |
|     1057 | 1226 | `	pVm = pSrc->pVm;` |
|     1057 | 1227 | `	pClone = NewClassInstance(pVm,pSrc->pClass);` |
|     1057 | 1228 | `	if( pClone == 0 ){` |
|      ! 0 | 1229 | `		return 0;` |
|        - | 1230 | `	}` |
|        - | 1231 | `	/* Associate a private VM frame with this class instance */` |
|     1057 | 1232 | `	rc = PH7_VmCreateClassInstanceFrame(pVm,pClone);` |
|     1057 | 1233 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1234 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClone);` |
|      ! 0 | 1235 | `		return 0;` |
|        - | 1236 | `	}` |
|        - | 1237 | `	/* A clone of an object whose LAZY native properties are installed has them` |
|        - | 1238 | `	 * too: php clones the C struct the table is written from, so the copy shows` |
|        - | 1239 | ``	 * what the original shows. The frame above skipped them (as it does at `new`),`` |
|        - | 1240 | `	 * so install them before the value copy below looks for the same-named slots. */` |
|     1057 | 1241 | `	if( pSrc->iFlags & VM_INSTANCE_LAZY_DONE ){` |
|        7 | 1242 | `		PH7_NativeMaterializeLazy(pVm,pClone);` |
|        3 | 1243 | `	}` |
|        - | 1244 | `	/* Duplicate object values. Iterate the SOURCE attributes and copy each into` |
|        - | 1245 | `	 * the clone's same-named slot (looked up by name, so order/count differences` |
|        - | 1246 | `	 * from dynamic properties don't matter). A dynamic (runtime-added) property` |
|        - | 1247 | `	 * has no declared counterpart in the clone, so synthesize it first — without` |
|        - | 1248 | `	 * this, a clone of a stdClass would silently lose all its dynamic properties. */` |
|     1057 | 1249 | `	SyHashResetLoopCursor(&pSrc->hAttr);` |
|     6161 | 1250 | `	while((pEntry = SyHashGetNextEntry(&pSrc->hAttr)) != 0 ){` |
|     5109 | 1251 | `		VmClassAttr *pSrcAttr = (VmClassAttr *)pEntry->pUserData;` |
|     5109 | 1252 | `		VmClassAttr *pDestAttr = 0;` |
|     5109 | 1253 | `		ph7_value *pvSrc,*pvDest = 0;` |
|        - | 1254 | `		/* Duplicate non-static attribute */` |
|     5109 | 1255 | `		if( pSrcAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|        6 | 1256 | `			continue;` |
|        - | 1257 | `		}` |
|     5105 | 1258 | `		pEntry2 = SyHashGet(&pClone->hAttr,SyStringData(&pSrcAttr->pAttr->sName),SyStringLength(&pSrcAttr->pAttr->sName));` |
|     5105 | 1259 | `		if( pEntry2 ){` |
|     5081 | 1260 | `			pDestAttr = (VmClassAttr *)pEntry2->pUserData;` |
|     5081 | 1261 | `			pvDest = ExtractClassAttrValue(pVm,pDestAttr);` |
|     2563 | 1262 | `		}else if( pSrcAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){` |
|        - | 1263 | `			/* Dynamic property: synthesize the matching slot on the clone. */` |
|       34 | 1264 | `			pvDest = PH7_VmCreateDynamicAttr(pVm,pClone,` |
|       22 | 1265 | `				SyStringData(&pSrcAttr->pAttr->sName),SyStringLength(&pSrcAttr->pAttr->sName),&pDestAttr);` |
|       14 | 1266 | `		}else if( pSrcAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_ONDEMAND ){` |
|        - | 1267 | `			/* An ON-DEMAND property is installed by the write that names it, so` |
|        - | 1268 | `			 * the clone's frame has no slot for one -- and php's copy carries it` |
|        - | 1269 | ``			 * (a cloned from-string DateInterval keeps its `date_string`). */`` |
|        3 | 1270 | `			VmRecreateDeclaredAttr(pVm,pClone,pSrcAttr->pAttr,&pDestAttr);` |
|        3 | 1271 | `			if( pDestAttr ){` |
|        3 | 1272 | `				pvDest = ExtractClassAttrValue(pVm,pDestAttr);` |
|        1 | 1273 | `			}` |
|        1 | 1274 | `		}` |
|        - | 1275 | `		/* Fetch the source value LAST: PH7_VmCreateDynamicAttr above may have` |
|        - | 1276 | `		 * reserved a slot and reallocated pVm->aMemObj, which would dangle any` |
|        - | 1277 | `		 * ph7_value* obtained before it. pvDest from the synth path already points` |
|        - | 1278 | `		 * into the post-realloc aMemObj; resolve pvSrc now so both are current. */` |
|     5105 | 1279 | `		pvSrc = ExtractClassAttrValue(pVm,pSrcAttr);` |
|     5105 | 1280 | `		if( (pSrcAttr->iState & VM_CLASS_ATTR_REFBOUND) && pDestAttr ){` |
|        - | 1281 | `			/* php preserves references across clone: the clone shares the SAME slot` |
|        - | 1282 | `			 * as the source property (both alias the referenced variable), rather` |
|        - | 1283 | `			 * than getting an independent value copy. Drop the clone's fresh private` |
|        - | 1284 | `			 * slot and repoint at the (already-pinned) shared slot. The REFBOUND flag` |
|        - | 1285 | `			 * is carried over by the iState copy below, so the clone's release also` |
|        - | 1286 | `			 * leaves the shared slot alone. */` |
|        5 | 1287 | `			if( pDestAttr->nIdx != pSrcAttr->nIdx ){` |
|        5 | 1288 | `				PH7_VmStoreFilterDrop(pVm,pDestAttr->pAttr,pDestAttr->nIdx);` |
|        5 | 1289 | `				PH7_VmUnsetMemObj(pVm,pDestAttr->nIdx,TRUE);` |
|        5 | 1290 | `				pDestAttr->nIdx = pSrcAttr->nIdx;` |
|        - | 1291 | `				/* The clone is a holder of the shared slot in its own right — take a pin` |
|        - | 1292 | `				 * for it, since its own release will give one back. */` |
|        5 | 1293 | `				VmPinMemObjSlotCounted(pVm,pDestAttr->nIdx);` |
|        3 | 1294 | `			}` |
|     5103 | 1295 | `		}else if( pvSrc && pvDest ){` |
|     5101 | 1296 | `			PH7_MemObjStore(pvSrc,pvDest);` |
|     2548 | 1297 | `		}` |
|        - | 1298 | `		/* Carry over the per-instance state so the clone matches the source:` |
|        - | 1299 | `		 * VM_CLASS_ATTR_UNINIT marks a typed property as not-yet-initialized` |
|        - | 1300 | `		 * and doubles as the readonly write-once latch — without this a clone` |
|        - | 1301 | `		 * would reset to uninitialized (losing the value's readiness) and a` |
|        - | 1302 | `		 * readonly property would become writable again. */` |
|     5105 | 1303 | `		if( pDestAttr ){` |
|     5105 | 1304 | `			pDestAttr->iState = pSrcAttr->iState;` |
|     2550 | 1305 | `		}` |
|        5 | 1306 | `	}` |
|        - | 1307 | `	/* A declared property unset() on the source is absent from the clone too (PHP). But the clone` |
|        - | 1308 | `	 * frame above materialized ALL declared attrs (with their defaults), so drop any clone attr whose` |
|        - | 1309 | `	 * name is not present on the source. Collect first, then delete — removing an entry mid-walk would` |
|        - | 1310 | `	 * free the node the SyHash loop cursor points at. */` |
|        - | 1311 | `	{` |
|        - | 1312 | `		SySet sDrop;` |
|     1057 | 1313 | `		SySetInit(&sDrop,&pVm->sAllocator,sizeof(VmClassAttr *));` |
|     1057 | 1314 | `		SyHashResetLoopCursor(&pClone->hAttr);` |
|     6165 | 1315 | `		while((pEntry = SyHashGetNextEntry(&pClone->hAttr)) != 0 ){` |
|     5113 | 1316 | `			VmClassAttr *pCloneAttr = (VmClassAttr *)pEntry->pUserData;` |
|     5113 | 1317 | `			if( pCloneAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|        6 | 1318 | `				continue;` |
|        - | 1319 | `			}` |
|     7656 | 1320 | `			if( SyHashGet(&pSrc->hAttr,SyStringData(&pCloneAttr->pAttr->sName),` |
|     7661 | 1321 | `					SyStringLength(&pCloneAttr->pAttr->sName)) == 0 ){` |
|        5 | 1322 | `				SySetPut(&sDrop,(const void *)&pCloneAttr);` |
|        2 | 1323 | `			}` |
|        5 | 1324 | `		}` |
|     1057 | 1325 | `		if( SySetUsed(&sDrop) > 0 ){` |
|        5 | 1326 | `			VmClassAttr **apDrop = (VmClassAttr **)SySetBasePtr(&sDrop);` |
|        - | 1327 | `			sxu32 i;` |
|        9 | 1328 | `			for( i = 0 ; i < SySetUsed(&sDrop) ; ++i ){` |
|        5 | 1329 | `				VmClassAttr *pVmAttr = apDrop[i];` |
|        7 | 1330 | `				SyHashDeleteEntry(&pClone->hAttr,SyStringData(&pVmAttr->pAttr->sName),` |
|        4 | 1331 | `					SyStringLength(&pVmAttr->pAttr->sName),0);` |
|        5 | 1332 | `				PH7_VmReleaseInstanceAttr(pVm,pVmAttr);` |
|        3 | 1333 | `			}` |
|        2 | 1334 | `		}` |
|     1057 | 1335 | `		SySetRelease(&sDrop);` |
|        - | 1336 | `	}` |
|        - | 1337 | `	/* The native clone hook (php's clone_obj handler): what the copy MEANS for a` |
|        - | 1338 | `	 * class whose instances stand for engine-side state -- a DOM wrapper's copy` |
|        - | 1339 | `	 * is a copy of the node. The nearest ancestor's hook serves a user subclass,` |
|        - | 1340 | `	 * which is php's handler inheritance. Runs before any __clone(), as php's` |
|        - | 1341 | `	 * handler does. */` |
|        - | 1342 | `	{` |
|        - | 1343 | `		ph7_class *pHook;` |
|     2089 | 1344 | `		for( pHook = pClone->pClass ; pHook ; pHook = pHook->pBase ){` |
|     1075 | 1345 | `			if( pHook->xClone ){` |
|       39 | 1346 | `				pHook->xClone(pVm,pClone,pSrc);` |
|       39 | 1347 | `				break;` |
|        - | 1348 | `			}` |
|      521 | 1349 | `		}` |
|        - | 1350 | `	}` |
|        - | 1351 | `	/* call the __clone method on the cloned object if available */` |
|     1057 | 1352 | `	pMethod = PH7_ClassExtractMethod(pClone->pClass,"__clone",sizeof("__clone")-1);` |
|     1057 | 1353 | `	if( pMethod ){` |
|      101 | 1354 | `		if( pMethod->iCloneDepth < 16 ){` |
|       99 | 1355 | `			pMethod->iCloneDepth++;` |
|        - | 1356 | `			/* PHP 8.3: __clone() may re-initialize the clone's readonly` |
|        - | 1357 | `			 * properties. Flag the instance so the readonly store guard allows` |
|        - | 1358 | `			 * it for the duration of the call. */` |
|       99 | 1359 | `			pClone->iFlags \|= VM_INSTANCE_CLONING;` |
|       99 | 1360 | `			PH7_VmCallClassMethod(pVm,pClone,pMethod,0,0,0);` |
|       99 | 1361 | `			pClone->iFlags &= ~VM_INSTANCE_CLONING;` |
|       51 | 1362 | `		}else{` |
|        - | 1363 | `			/* Nesting limit reached */` |
|        3 | 1364 | `			PH7_VmThrowError(pVm,0,PH7_CTX_ERR,"Object clone limit reached,no more call to __clone()");` |
|        - | 1365 | `		}` |
|        - | 1366 | `		/* Reset the cursor */` |
|      101 | 1367 | `		pMethod->iCloneDepth = 0;` |
|       49 | 1368 | `	}` |
|        - | 1369 | `	/* Return the cloned object */` |
|     1057 | 1370 | `	return pClone;` |
|      531 | 1371 | `}` |
|        - | 1372 | `#define CLASS_INSTANCE_DESTROYED 0x001 /* Instance is released */` |
|        - | 1373 | `/*` |
|        - | 1374 | ` * Free the per-instance allocations owned by ONE object attribute: its value slot (+ the typed-slot` |
|        - | 1375 | ` * enforcement entry), the synthesized ph7_class_attr for a dynamic (runtime-added) property, and the` |
|        - | 1376 | ` * VmClassAttr wrapper itself. Does NOT touch the hAttr entry node — the caller removes it` |
|        - | 1377 | `` * (`unset($o->p)` via SyHashDeleteEntry2; instance teardown via the wholesale SyHashRelease, so it must`` |
|        - | 1378 | ` * not delete entries mid-walk). Shared by PH7_ClassInstanceRelease and the OP_MEMBER unset path.` |
|        - | 1379 | ` */` |
|  9617763 | 1380 | `PH7_PRIVATE void PH7_VmReleaseInstanceAttr(ph7_vm *pVm, VmClassAttr *pVmAttr)` |
|        5 | 1381 | `{` |
|  9617768 | 1382 | `	if( pVmAttr->iState & VM_CLASS_ATTR_REFBOUND ){` |
|        - | 1383 | ``		/* Reference-bound property (`$o->p =& $x`): its value slot is SHARED, so it must`` |
|        - | 1384 | `		 * not be released here — but the property WAS one of its holders, so give the pin` |
|        - | 1385 | `		 * back. The slot (and its value, which used to stay alive for the rest of the` |
|        - | 1386 | `		 * script) goes if the property was the last thing holding it. */` |
|       27 | 1387 | `		VmUnpinMemObjSlot(pVm,pVmAttr->nIdx);` |
|  9617755 | 1388 | `	}else if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - | 1389 | `		/* Drop any typed-property enforcement slot registered for this memobj, before the memobj` |
|        - | 1390 | `		 * is returned to the free list, so a future recycled slot does not inherit the stale entry. */` |
|  9617634 | 1391 | `		PH7_VmStoreFilterDrop(pVm,pVmAttr->pAttr,pVmAttr->nIdx);` |
|  9617634 | 1392 | `		PH7_VmUnsetMemObj(pVm,pVmAttr->nIdx,TRUE);` |
|  4808811 | 1393 | `	}` |
|        - | 1394 | `	/* A dynamic property owns its synthesized ph7_class_attr (struct + inline name in one block) —` |
|        - | 1395 | `	 * free it here (the only place a per-instance pAttr is freed; declared attrs are class-owned). */` |
|  9617768 | 1396 | `	if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){` |
|      411 | 1397 | `		SyMemBackendFree(&pVm->sAllocator,pVmAttr->pAttr);` |
|      203 | 1398 | `	}` |
|  9617768 | 1399 | `	SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|  9617768 | 1400 | `}` |
|        - | 1401 | `/*` |
|        - | 1402 | ` * Release a class instance [i.e: Object in the PHP jargon] and invoke any defined destructor.` |
|        - | 1403 | ` * This routine is invoked as soon as there are no other references to a particular` |
|        - | 1404 | ` * class instance.` |
|        - | 1405 | ` */` |
|  1484963 | 1406 | `static void PH7_ClassInstanceRelease(ph7_class_instance *pThis)` |
|        5 | 1407 | `{` |
|        - | 1408 | `	ph7_class_method *pDestr;` |
|        - | 1409 | `	SyHashEntry *pEntry;` |
|        - | 1410 | `	ph7_class *pClass;` |
|        - | 1411 | `	ph7_vm *pVm;` |
|  1484968 | 1412 | `	if( pThis->iFlags & CLASS_INSTANCE_DESTROYED ){` |
|        - | 1413 | `		/*` |
|        - | 1414 | `		 * Already destroyed,return immediately.` |
|        - | 1415 | `		 * This could happend if someone perform unset($this) in the destructor body.` |
|        - | 1416 | `		 */` |
|      ! 0 | 1417 | `		return;` |
|        - | 1418 | `	}` |
|        - | 1419 | `	/* Mark as destroyed */` |
|  1484968 | 1420 | `	pThis->iFlags \|= CLASS_INSTANCE_DESTROYED;` |
|        - | 1421 | `	/* Invoke any defined destructor if available */` |
|  1484968 | 1422 | `	pVm = pThis->pVm;` |
|  1484968 | 1423 | `	pClass = pThis->pClass;` |
|  1484968 | 1424 | `	pDestr = PH7_ClassExtractMethod(pClass,"__destruct",sizeof("__destruct")-1);` |
|  1484968 | 1425 | `	if( pDestr && !pVm->bInReset ){` |
|        - | 1426 | `		/* Invoke the destructor. Skipped during ph7_vm_reset() bulk teardown:` |
|        - | 1427 | `		 * running user PHP against a half-reset VM is unsafe (see bInReset). */` |
|      611 | 1428 | `		pThis->iRef = 2; /* Prevent garbage collection */` |
|      611 | 1429 | `		PH7_VmCallClassMethod(pVm,pThis,pDestr,0,0,0);` |
|      303 | 1430 | `	}` |
|        - | 1431 | `	/* A native class's own teardown, while its slots are still readable. Not a` |
|        - | 1432 | `	 * __destruct: the classes that need this (WeakReference) declare none in php,` |
|        - | 1433 | `	 * and Reflection must not grow one.` |
|        - | 1434 | `	 *` |
|        - | 1435 | `	 * Resolved through the ANCESTORS, like php's own free_obj handler: a` |
|        - | 1436 | `	 * subclass inherits it unless it declares one of its own. Reading it off` |
|        - | 1437 | `	 * this class alone left every subclass of a handle-owning native class` |
|        - | 1438 | ``	 * without teardown -- `Pdo\Sqlite` (which is how PDO::connect() answers) and`` |
|        - | 1439 | ``	 * any userland `extends PDO` alike, both of which then died holding engine`` |
|        - | 1440 | `	 * state that believed it was still reachable. */` |
|        - | 1441 | `	{` |
|  1484968 | 1442 | `		ph7_class *pOwner = pClass;` |
|  3091584 | 1443 | `		while( pOwner && pOwner->xRelease == 0 ){` |
|  1606621 | 1444 | `			pOwner = pOwner->pBase;` |
|        5 | 1445 | `		}` |
|  1484968 | 1446 | `		if( pOwner && pOwner->xRelease ){` |
|     1685 | 1447 | `			pOwner->xRelease(pVm,pThis);` |
|      840 | 1448 | `		}` |
|        - | 1449 | `	}` |
|        - | 1450 | `	/* Weak-reference registry: kill the cell for this instance so every` |
|        - | 1451 | `	 * WeakReference/WeakMap handle observes the death (the cell outlives the` |
|        - | 1452 | `	 * instance until its own handles drop; removing the hash entry here keeps` |
|        - | 1453 | `	 * a pool-reused address from resurrecting a dead cell). */` |
|  1484968 | 1454 | `	if( SyHashTotalEntry(&pVm->hWeakCell) > 0 ){` |
|     9442 | 1455 | `		void *pCellData = 0;` |
|     9440 | 1456 | `		if( SyHashDeleteEntry(&pVm->hWeakCell,(const void *)&pThis,sizeof(void *),&pCellData) == SXRET_OK` |
|     4736 | 1457 | `		 && pCellData ){` |
|       30 | 1458 | `			((VmWeakCell *)pCellData)->pObj = 0;` |
|       14 | 1459 | `		}` |
|     4720 | 1460 | `	}` |
|        - | 1461 | `	/* Release non-static attributes (the wholesale SyHashRelease below frees the entry nodes,` |
|        - | 1462 | `	 * so the helper must not delete them mid-walk). */` |
|  1484968 | 1463 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
| 11102687 | 1464 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|  9617724 | 1465 | `		PH7_VmReleaseInstanceAttr(pVm,(VmClassAttr *)pEntry->pUserData);` |
|        5 | 1466 | `	}` |
|        - | 1467 | `	/* Release the whole structure */` |
|  1484968 | 1468 | `	SyHashRelease(&pThis->hAttr);` |
|  1484968 | 1469 | `	SyMemBackendPoolFree(&pVm->sAllocator,pThis);` |
|   742486 | 1470 | `}` |
|        - | 1471 | `/*` |
|        - | 1472 | ` * Decrement the reference count of a class instance [i.e Object in the PHP jargon].` |
|        - | 1473 | ` * If the reference count reaches zero,release the whole instance.` |
|        - | 1474 | ` */` |
|  7627853 | 1475 | `PH7_PRIVATE void PH7_ClassInstanceUnref(ph7_class_instance *pThis)` |
|        5 | 1476 | `{` |
|  7627858 | 1477 | `	pThis->iRef--;` |
|  7627858 | 1478 | `	if( pThis->iRef < 1 ){` |
|        - | 1479 | `		/* No more reference to this instance */` |
|  1484968 | 1480 | `		PH7_ClassInstanceRelease(&(*pThis));` |
|   742481 | 1481 | `	}` |
|  7627858 | 1482 | `}` |
|        - | 1483 | `/*` |
|        - | 1484 | ` * Compare two class instances [i.e: Objects in the PHP jargon]` |
|        - | 1485 | ` * Note on objects comparison:` |
|        - | 1486 | ` *  According to the PHP langauge reference manual` |
|        - | 1487 | ` *  When using the comparison operator (==), object variables are compared in a simple manner` |
|        - | 1488 | ` *  namely: Two object instances are equal if they have the same attributes and values, and are` |
|        - | 1489 | ` *  instances of the same class.` |
|        - | 1490 | ` *  On the other hand, when using the identity operator (===), object variables are identical` |
|        - | 1491 | ` *  if and only if they refer to the same instance of the same class.` |
|        - | 1492 | ` *  An example will clarify these rules.` |
|        - | 1493 | ` *  Example #1 Example of object comparison` |
|        - | 1494 | ` *  <?php` |
|        - | 1495 | ` *    function bool2str($bool)` |
|        - | 1496 | ` * {` |
|        - | 1497 | ` *   if ($bool === false) {` |
|        - | 1498 | ` *       return 'FALSE';` |
|        - | 1499 | ` *   } else {` |
|        - | 1500 | ` *       return 'TRUE';` |
|        - | 1501 | ` *   }` |
|        - | 1502 | ` * }` |
|        - | 1503 | ` * function compareObjects(&$o1, &$o2)` |
|        - | 1504 | ` * {` |
|        - | 1505 | ` *   echo 'o1 == o2 : ' . bool2str($o1 == $o2) . "\n";` |
|        - | 1506 | ` *   echo 'o1 != o2 : ' . bool2str($o1 != $o2) . "\n";` |
|        - | 1507 | ` *   echo 'o1 === o2 : ' . bool2str($o1 === $o2) . "\n";` |
|        - | 1508 | ` *   echo 'o1 !== o2 : ' . bool2str($o1 !== $o2) . "\n";` |
|        - | 1509 | ` * }` |
|        - | 1510 | ` * class Flag` |
|        - | 1511 | ` * {` |
|        - | 1512 | ` *   public $flag;` |
|        - | 1513 | ` *` |
|        - | 1514 | ` *   function Flag($flag = true) {` |
|        - | 1515 | ` *       $this->flag = $flag;` |
|        - | 1516 | ` *   }` |
|        - | 1517 | ` * }` |
|        - | 1518 | ` *` |
|        - | 1519 | ` * class OtherFlag` |
|        - | 1520 | ` * {` |
|        - | 1521 | ` *   public $flag;` |
|        - | 1522 | ` *` |
|        - | 1523 | ` *   function OtherFlag($flag = true) {` |
|        - | 1524 | ` *       $this->flag = $flag;` |
|        - | 1525 | ` *   }` |
|        - | 1526 | ` * }` |
|        - | 1527 | ` *` |
|        - | 1528 | ` * $o = new Flag();` |
|        - | 1529 | ` * $p = new Flag();` |
|        - | 1530 | ` * $q = $o;` |
|        - | 1531 | ` * $r = new OtherFlag();` |
|        - | 1532 | ` *` |
|        - | 1533 | ` * echo "Two instances of the same class\n";` |
|        - | 1534 | ` * compareObjects($o, $p);` |
|        - | 1535 | ` * echo "\nTwo references to the same instance\n";` |
|        - | 1536 | ` * compareObjects($o, $q);` |
|        - | 1537 | ` * echo "\nInstances of two different classes\n";` |
|        - | 1538 | ` * compareObjects($o, $r);` |
|        - | 1539 | ` * ?>` |
|        - | 1540 | ` * The above example will output:` |
|        - | 1541 | ` * Two instances of the same class` |
|        - | 1542 | ` * o1 == o2 : TRUE` |
|        - | 1543 | ` * o1 != o2 : FALSE` |
|        - | 1544 | ` * o1 === o2 : FALSE` |
|        - | 1545 | ` * o1 !== o2 : TRUE` |
|        - | 1546 | ` * Two references to the same instance` |
|        - | 1547 | ` * o1 == o2 : TRUE` |
|        - | 1548 | ` * o1 != o2 : FALSE` |
|        - | 1549 | ` * o1 === o2 : TRUE` |
|        - | 1550 | ` * o1 !== o2 : FALSE` |
|        - | 1551 | ` * Instances of two different classes` |
|        - | 1552 | ` * o1 == o2 : FALSE` |
|        - | 1553 | ` * o1 != o2 : TRUE` |
|        - | 1554 | ` * o1 === o2 : FALSE` |
|        - | 1555 | ` * o1 !== o2 : TRUE` |
|        - | 1556 | ` *` |
|        - | 1557 | ` * This function return 0 if the objects are equals according to the comprison rules defined above.` |
|        - | 1558 | ` * Any other return values indicates difference.` |
|        - | 1559 | ` */` |
|      894 | 1560 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest)` |
|        5 | 1561 | `{` |
|        - | 1562 | `	SyHashEntry *pEntry,*pEntry2;` |
|        - | 1563 | `	ph7_value sV1,sV2;` |
|        - | 1564 | `	sxi32 rc;` |
|      899 | 1565 | `	if( iNest > 31 ){` |
|        - | 1566 | `		/* Nesting limit reached */` |
|        6 | 1567 | `		PH7_VmThrowError(pLeft->pVm,0,PH7_CTX_ERR,"Nesting limit reached: Infinite recursion?");` |
|        6 | 1568 | `		return 1;` |
|        - | 1569 | `	}` |
|        - | 1570 | `	/*` |
|        - | 1571 | `	 * php's identity shortcut, and it comes FIRST -- before the same-class screen` |
|        - | 1572 | ``	 * and before any handler: `$i == $i` is 0 for a DateInterval, the one pair of`` |
|        - | 1573 | `	 * intervals php will compare at all.` |
|        - | 1574 | `	 */` |
|      895 | 1575 | `	if( pLeft == pRight ){` |
|        - | 1576 | `		/* Same instance,don't bother processing,object are equals */` |
|      411 | 1577 | `		return 0;` |
|        - | 1578 | `	}` |
|      489 | 1579 | `	if( bStrict ){` |
|        - | 1580 | `		/*` |
|        - | 1581 | `		 * According to the PHP language reference manual:` |
|        - | 1582 | `		 *  when using the identity operator (===), object variables` |
|        - | 1583 | `		 *  are identical if and only if they refer to the same instance` |
|        - | 1584 | `		 *  of the same class.` |
|        - | 1585 | `		 * Two DISTINCT instances, so this is never identical -- and no compare` |
|        - | 1586 | ``		 * handler is asked, because php's `===` is pointer identity and never`` |
|        - | 1587 | `		 * reaches one.` |
|        - | 1588 | `		 */` |
|      121 | 1589 | `		return 1;` |
|        - | 1590 | `	}` |
|        - | 1591 | `	/*` |
|        - | 1592 | `	 * php's compare handler (ph7_class::xCmp), asked of the LEFT operand and` |
|        - | 1593 | `	 * ABOVE the same-class screen: a DateTime and a DateTimeImmutable of the same` |
|        - | 1594 | `	 * instant are equal there, which no property walk between two different` |
|        - | 1595 | `	 * classes could ever answer. A class with no handler falls through to the` |
|        - | 1596 | `	 * walk, which is php's zend_std_compare_objects.` |
|        - | 1597 | `	 */` |
|      371 | 1598 | `	if( PH7_ClassNativeCmp(pLeft,pRight,&rc) ){` |
|      156 | 1599 | `		return rc;` |
|        - | 1600 | `	}` |
|        - | 1601 | `	/* Comparison is performed only if the objects are instance of the same class */` |
|      217 | 1602 | `	if( pLeft->pClass != pRight->pClass ){` |
|       14 | 1603 | `		return 1;` |
|        - | 1604 | `	}` |
|        - | 1605 | `	/*` |
|        - | 1606 | `	 * Attribute comparison.` |
|        - | 1607 | `	 * According to the PHP reference manual:` |
|        - | 1608 | `	 *  When using the comparison operator (==), object variables are compared` |
|        - | 1609 | `	 *  in a simple manner, namely: Two object instances are equal if they have` |
|        - | 1610 | `	 *  the same attributes and values, and are instances of the same class.` |
|        - | 1611 | `	 */` |
|        - | 1612 | `	/* Closures compare by IDENTITY under == as well (not by attributes): two distinct` |
|        - | 1613 | `	 * Closure instances are never equal, even when they wrap the same underlying function` |
|        - | 1614 | `	 * (PHP semantics). pLeft != pRight here, so a Closure pair is unequal. Without this,` |
|        - | 1615 | `` 	 * two capture-less lambdas of the same `function(){}` share the template's `$__fn` `` |
|        - | 1616 | `	 * name and would compare equal. */` |
|      205 | 1617 | `	if( pLeft->pVm->pClosureClass && pLeft->pClass == pLeft->pVm->pClosureClass ){` |
|        5 | 1618 | `		return 1;` |
|        - | 1619 | `	}` |
|        - | 1620 | `	/* Same class but a different number of attributes ⇒ different property sets` |
|        - | 1621 | `	 * (dynamic properties can give two same-class instances different counts). */` |
|      201 | 1622 | `	if( pLeft->hAttr.nEntry != pRight->hAttr.nEntry ){` |
|        3 | 1623 | `		return 1;` |
|        - | 1624 | `	}` |
|      199 | 1625 | `	PH7_MemObjInit(pLeft->pVm,&sV1);` |
|      199 | 1626 | `	PH7_MemObjInit(pLeft->pVm,&sV2);` |
|      199 | 1627 | `	sV1.nIdx = sV2.nIdx = SXU32_HIGH;` |
|        - | 1628 | `	/* Compare each left attribute against the RIGHT attribute of the SAME NAME` |
|        - | 1629 | `	 * (not in lockstep): dynamic properties may be stored in a different order` |
|        - | 1630 | `	 * on the two instances. Counts already match, so if every left attribute has` |
|        - | 1631 | `	 * an equal-valued same-named right attribute the property sets are equal. */` |
|      199 | 1632 | `	SyHashResetLoopCursor(&pLeft->hAttr);` |
|      339 | 1633 | `	while((pEntry = SyHashGetNextEntry(&pLeft->hAttr)) != 0 ){` |
|      287 | 1634 | `		VmClassAttr *p1 = (VmClassAttr *)pEntry->pUserData;` |
|        - | 1635 | `		VmClassAttr *p2;` |
|        - | 1636 | `		ph7_value *pL,*pR;` |
|        - | 1637 | `		/* Compare only non-static attribute. A native class's VIRTUAL property is` |
|        - | 1638 | `		 * skipped too: php fabricates DatePeriod's seven on demand and its real` |
|        - | 1639 | `		 * property table is empty, so any two DatePeriods are equal there whatever` |
|        - | 1640 | `		 * they contain -- while a subclass's own property, which IS in the table,` |
|        - | 1641 | `		 * still decides. */` |
|      287 | 1642 | `		if( p1->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC` |
|        - | 1643 | `		                        \|PH7_CLASS_ATTR_NATIVE_VIRTUAL) ){` |
|       71 | 1644 | `			continue;` |
|        - | 1645 | `		}` |
|      217 | 1646 | `		pEntry2 = SyHashGet(&pRight->hAttr,SyStringData(&p1->pAttr->sName),SyStringLength(&p1->pAttr->sName));` |
|      217 | 1647 | `		if( pEntry2 == 0 ){` |
|        - | 1648 | `			/* Left has a property the right lacks ⇒ not equal. */` |
|      ! 0 | 1649 | `			return 1;` |
|        - | 1650 | `		}` |
|      217 | 1651 | `		p2 = (VmClassAttr *)pEntry2->pUserData;` |
|      217 | 1652 | `		pL = ExtractClassAttrValue(pLeft->pVm,p1);` |
|      217 | 1653 | `		pR = ExtractClassAttrValue(pRight->pVm,p2);` |
|      217 | 1654 | `		if( pL && pR ){` |
|      217 | 1655 | `			PH7_MemObjLoad(pL,&sV1);` |
|      217 | 1656 | `			PH7_MemObjLoad(pR,&sV2);` |
|        - | 1657 | `			/* Compare the two values now */` |
|      217 | 1658 | `			rc = PH7_MemObjCmp(&sV1,&sV2,bStrict,iNest+1);` |
|      217 | 1659 | `			PH7_MemObjRelease(&sV1);` |
|      217 | 1660 | `			PH7_MemObjRelease(&sV2);` |
|      217 | 1661 | `			if( rc != 0 ){` |
|        - | 1662 | `				/* Not equals */` |
|      146 | 1663 | `				return rc;` |
|        - | 1664 | `			}` |
|       35 | 1665 | `		}` |
|        2 | 1666 | `	}` |
|        - | 1667 | `	/* Object are equals */` |
|       55 | 1668 | `	return 0;` |
|      452 | 1669 | `}` |
|        - | 1670 | `/*` |
|        - | 1671 | ` * Dump a class instance and the store the dump in the BLOB given` |
|        - | 1672 | ` * as the first argument.` |
|        - | 1673 | ` * Note that only non-static/non-constants attribute are dumped.` |
|        - | 1674 | ` * This function is typically invoked when the user issue a call` |
|        - | 1675 | ` * to [var_dump(),var_export(),print_r(),...].` |
|        - | 1676 | ` * This function SXRET_OK on success. Any other return value including` |
|        - | 1677 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|        - | 1678 | ` */` |
|        - | 1679 | `/*` |
|        - | 1680 | `` * Return the `name` property value of an enum case instance (the case name),`` |
|        - | 1681 | ` * or 0 when unavailable. Shared by the var_dump/var_export/json/serialize` |
|        - | 1682 | ` * renderers, which all print enum cases as Class::CaseName forms.` |
|        - | 1683 | ` */` |
|       24 | 1684 | `PH7_PRIVATE ph7_value * PH7_EnumCaseNameValue(ph7_class_instance *pThis)` |
|        1 | 1685 | `{` |
|        - | 1686 | `	SyHashEntry *pEntry;` |
|       25 | 1687 | `	if( (pThis->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|      ! 0 | 1688 | `		return 0;` |
|        - | 1689 | `	}` |
|       25 | 1690 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)"name",sizeof("name")-1);` |
|       25 | 1691 | `	if( pEntry == 0 ){` |
|      ! 0 | 1692 | `		return 0;` |
|        - | 1693 | `	}` |
|       25 | 1694 | `	return PH7_ClassInstanceExtractAttrValue(pThis,(VmClassAttr *)pEntry->pUserData);` |
|       13 | 1695 | `}` |
|        - | 1696 | `/*` |
|        - | 1697 | `` * Return the `value` property value (the backing value) of an enum case`` |
|        - | 1698 | ` * instance, or 0 when unavailable (pure enums have none).` |
|        - | 1699 | ` */` |
|       20 | 1700 | `PH7_PRIVATE ph7_value * PH7_EnumCaseBackingValueOf(ph7_class_instance *pThis)` |
|        1 | 1701 | `{` |
|        - | 1702 | `	SyHashEntry *pEntry;` |
|       21 | 1703 | `	if( (pThis->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|      ! 0 | 1704 | `		return 0;` |
|        - | 1705 | `	}` |
|       21 | 1706 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)"value",sizeof("value")-1);` |
|       21 | 1707 | `	if( pEntry == 0 ){` |
|        7 | 1708 | `		return 0;` |
|        - | 1709 | `	}` |
|       15 | 1710 | `	return PH7_ClassInstanceExtractAttrValue(pThis,(VmClassAttr *)pEntry->pUserData);` |
|       11 | 1711 | `}` |
|        - | 1712 | `/*` |
|        - | 1713 | ` * Emit a class-instance dump header plus its trailing newline. For var_dump` |
|        - | 1714 | ` * (ShowType) it completes the "object(" prefix the caller already emitted as` |
|        - | 1715 | ` *   ClassName)#<id> (<count>) {` |
|        - | 1716 | ` * for print_r it emits the legacy PHL  Object(ClassName) {  (count/id unused).` |
|        - | 1717 | `` * Enum cases print php's `ClassName Enum {` print_r header (var_dump never`` |
|        - | 1718 | `` * reaches here for enums — PH7_MemObjDump prints `enum(S::A)` directly).`` |
|        - | 1719 | ` */` |
|      332 | 1720 | `static void DumpClassInstanceHeader(SyBlob *pOut,ph7_class *pClass,sxu32 nObjId,int ShowType,sxu32 nCount)` |
|        5 | 1721 | `{` |
|      337 | 1722 | `	if( ShowType ){` |
|        - | 1723 | ``		/* var_dump: `object(C)#id (n) {` */`` |
|      237 | 1724 | `		SyBlobFormat(&(*pOut),"object(%z)#%u (%u) {",&pClass->sName,nObjId,nCount);` |
|      237 | 1725 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      237 | 1726 | `		return;` |
|        - | 1727 | `	}` |
|        - | 1728 | ``	/* print_r: `C Object` / `E Enum[:backing]` — the '(' line is emitted by`` |
|        - | 1729 | `	 * the body renderer at the container indent. */` |
|      105 | 1730 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|      ! 0 | 1731 | `		SyBlobFormat(&(*pOut),"%z Enum",&pClass->sName);` |
|      ! 0 | 1732 | `		if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|      ! 0 | 1733 | `			SyBlobAppend(&(*pOut),":int",sizeof(":int")-1);` |
|      ! 0 | 1734 | `		}else if( pClass->nEnumBacking == MEMOBJ_STRING ){` |
|      ! 0 | 1735 | `			SyBlobAppend(&(*pOut),":string",sizeof(":string")-1);` |
|      ! 0 | 1736 | `		}` |
|      ! 0 | 1737 | `	}else{` |
|      105 | 1738 | `		SyBlobFormat(&(*pOut),"%z Object",&pClass->sName);` |
|        - | 1739 | `	}` |
|      105 | 1740 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      171 | 1741 | `}` |
|        - | 1742 | `/*` |
|        - | 1743 | ` * The class that DECLARED pAttr: inheritance shares attr pointers down the` |
|        - | 1744 | ` * chain, so the declaring class is the most ANCESTRAL class whose hAttr still` |
|        - | 1745 | ` * maps the name to this exact pointer. php's var_dump/print_r use it for the` |
|        - | 1746 | `` * `["p":"Decl":private]` annotation.`` |
|        - | 1747 | ` */` |
|        8 | 1748 | `static ph7_class * OoAttrDeclaringClass(ph7_class *pClass,ph7_class_attr *pAttr)` |
|        2 | 1749 | `{` |
|        - | 1750 | `	/* Attrs record their declaring class at install time (inheritance/trait` |
|        - | 1751 | `	 * copies share the pointer, so the field survives the chain). */` |
|       10 | 1752 | `	return pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        2 | 1753 | `}` |
|        - | 1754 | `/*` |
|        - | 1755 | ` * php's zend_unmangle_property_name_ex: a property key carries its own` |
|        - | 1756 | ` * visibility when it is MANGLED — "\0*\0name" is protected and "\0Class\0name"` |
|        - | 1757 | ` * is private to Class, which is how the (array) cast, __debugInfo() and every` |
|        - | 1758 | ` * get_debug_info handler say what a plain array key cannot. A key that does not` |
|        - | 1759 | ` * begin with a NUL is a public name and comes back unchanged.` |
|        - | 1760 | ` *` |
|        - | 1761 | ` * Answers 0 for a key that begins with a NUL and is NOT a well-formed mangled` |
|        - | 1762 | ` * name — php's "Illegal member variable name" (nothing after the NUL, or an` |
|        - | 1763 | ` * empty class part) and "Corrupt member variable name" (no second NUL, or` |
|        - | 1764 | ` * nothing after it). php renders those raw, notice aside, and so does the caller.` |
|        - | 1765 | ` */` |
|      416 | 1766 | `PH7_PRIVATE int PH7_UnmangleAttrName(const char *zKey,sxu32 nKey,SyString *pClass,SyString *pName)` |
|        3 | 1767 | `{` |
|        - | 1768 | `	sxu32 nCls,nSrc;` |
|      419 | 1769 | `	SyStringInitFromBuf(pClass,0,0);` |
|      419 | 1770 | `	SyStringInitFromBuf(pName,zKey,nKey);` |
|      419 | 1771 | `	if( nKey < 1 \|\| zKey[0] != 0 ){` |
|      327 | 1772 | `		return 1;   /* a plain public name */` |
|        - | 1773 | `	}` |
|       93 | 1774 | `	if( nKey < 3 \|\| zKey[1] == 0 ){` |
|      ! 0 | 1775 | `		return 0;   /* php: "Illegal member variable name" */` |
|        - | 1776 | `	}` |
|       93 | 1777 | `	nCls = 0;` |
|      809 | 1778 | `	while( nCls < nKey - 2 && zKey[1+nCls] != 0 ){` |
|      717 | 1779 | `		nCls++;` |
|        1 | 1780 | `	}` |
|       93 | 1781 | `	if( nCls >= nKey - 2 ){` |
|      ! 0 | 1782 | `		return 0;   /* php: "Corrupt member variable name" */` |
|        - | 1783 | `	}` |
|        - | 1784 | `	/* An ANONYMOUS class mangles its source location in as a SECOND NUL-separated` |
|        - | 1785 | `	 * part, so the property name is what follows the LAST NUL rather than the` |
|        - | 1786 | `	 * second one (php's anonclass_src_len step). The class STRING php shows is` |
|        - | 1787 | `	 * still only the first part — it prints that one as a C string. */` |
|       93 | 1788 | `	nSrc = 0;` |
|      621 | 1789 | `	while( nCls + 2 + nSrc < nKey && zKey[nCls+2+nSrc] != 0 ){` |
|      529 | 1790 | `		nSrc++;` |
|        1 | 1791 | `	}` |
|       93 | 1792 | `	SyStringInitFromBuf(pClass,&zKey[1],nCls);` |
|       93 | 1793 | `	if( nCls + nSrc + 2 != nKey ){` |
|      ! 0 | 1794 | `		nCls += nSrc + 1;` |
|      ! 0 | 1795 | `	}` |
|       93 | 1796 | `	SyStringInitFromBuf(pName,&zKey[nCls+2],nKey - nCls - 2);` |
|       93 | 1797 | `	return 1;` |
|      211 | 1798 | `}` |
|        - | 1799 | `/*` |
|        - | 1800 | `` * Emit a property's dump key: var_dump `["x"]=>` / `["p":"C":private]=>` /`` |
|        - | 1801 | `` * `["q":protected]=>`; print_r `[x] => ` / `[p:C:private] => ` /`` |
|        - | 1802 | `` * `[q:protected] => ` (php's exact annotations).`` |
|        - | 1803 | ` */` |
|      216 | 1804 | `static void OoDumpPropKey(SyBlob *pOut,ph7_class_instance *pThis,ph7_class_attr *pAttr,int ShowType)` |
|        4 | 1805 | `{` |
|      220 | 1806 | `	const char *zQ = ShowType ? "\"" : "";` |
|      220 | 1807 | `	if( SyStringLength(&pAttr->sName) > 0 && SyStringData(&pAttr->sName)[0] == 0 ){` |
|        - | 1808 | `		/* A MANGLED name stored raw — the __PHP_Incomplete_Class carrier's` |
|        - | 1809 | `		 * private/protected payload keys. php's dump unmangles them exactly as` |
|        - | 1810 | ``		 * it does an (array) cast's: `["bp":"PB":private]` / `["pp":protected]`. */`` |
|        - | 1811 | `		SyString sUnmCls, sUnmName;` |
|        9 | 1812 | `		if( PH7_UnmangleAttrName(SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),` |
|        - | 1813 | `			&sUnmCls,&sUnmName) ){` |
|        9 | 1814 | `			SyBlobFormat(&(*pOut),"[%s%z%s",zQ,&sUnmName,zQ);` |
|        9 | 1815 | `			if( sUnmCls.nByte == 1 && sUnmCls.zString[0] == '*' ){` |
|        5 | 1816 | `				SyBlobAppend(&(*pOut),":protected",sizeof(":protected")-1);` |
|        3 | 1817 | `			}else{` |
|        5 | 1818 | `				SyBlobFormat(&(*pOut),":%s%z%s:private",zQ,&sUnmCls,zQ);` |
|        - | 1819 | `			}` |
|        9 | 1820 | `			SyBlobAppend(&(*pOut),ShowType ? "]=>" : "] => ",ShowType ? 3 : 5);` |
|        9 | 1821 | `			return;` |
|        - | 1822 | `		}` |
|      ! 0 | 1823 | `	}` |
|      212 | 1824 | `	SyBlobFormat(&(*pOut),"[%s%z%s",zQ,&pAttr->sName,zQ);` |
|      212 | 1825 | `	if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       10 | 1826 | `		ph7_class *pDecl = OoAttrDeclaringClass(pThis->pClass,pAttr);` |
|       10 | 1827 | `		SyBlobFormat(&(*pOut),":%s%z%s:private",zQ,&pDecl->sName,zQ);` |
|      208 | 1828 | `	}else if( pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
|        7 | 1829 | `		SyBlobAppend(&(*pOut),":protected",sizeof(":protected")-1);` |
|        3 | 1830 | `	}` |
|      212 | 1831 | `	SyBlobAppend(&(*pOut),ShowType ? "]=>" : "] => ",ShowType ? 3 : 5);` |
|      112 | 1832 | `}` |
|      336 | 1833 | `PH7_PRIVATE sxi32 PH7_ClassInstanceDump(SyBlob *pOut,ph7_class_instance *pThis,int ShowType,int nTab,int nDepth)` |
|        5 | 1834 | `{` |
|        - | 1835 | `	SyHashEntry *pEntry;` |
|        - | 1836 | `	ph7_value *pValue;` |
|        - | 1837 | `	sxi32 rc;` |
|        - | 1838 | `	int i;` |
|      341 | 1839 | `	if( nDepth > 31 ){` |
|        - | 1840 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|        - | 1841 | `		/* Nesting limit reached..halt immediately*/` |
|        5 | 1842 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|        5 | 1843 | `		return SXERR_LIMIT;` |
|        - | 1844 | `	}` |
|      337 | 1845 | `	rc = SXRET_OK;` |
|        - | 1846 | `	{` |
|        - | 1847 | `		/* A native class's PRESENTATION (php's get_debug_info): the shape it shows` |
|        - | 1848 | `		 * is not its storage — a DateTime shows date/timezone_type/timezone, a` |
|        - | 1849 | `		 * WeakReference shows ["object"] — and the slots underneath are hidden.` |
|        - | 1850 | `		 * Rendered exactly like a __debugInfo() array, which is what php does with` |
|        - | 1851 | `		 * it, and consulted first because php's handler wins over a userland` |
|        - | 1852 | `		 * method a native class cannot declare anyway. */` |
|        - | 1853 | `		ph7_value sPresent;` |
|      337 | 1854 | `		PH7_MemObjInit(pThis->pVm,&sPresent);` |
|      337 | 1855 | `		if( ph7_value_is_array(&sPresent) == 0 ){` |
|      337 | 1856 | `			ph7_hashmap *pPresent = PH7_NewHashmap(pThis->pVm,0,0);` |
|      337 | 1857 | `			if( pPresent ){` |
|      337 | 1858 | `				sPresent.x.pOther = pPresent;` |
|      337 | 1859 | `				MemObjSetType(&sPresent,MEMOBJ_HASHMAP);` |
|      166 | 1860 | `			}` |
|      166 | 1861 | `		}` |
|      332 | 1862 | `		if( (sPresent.iFlags & MEMOBJ_HASHMAP)` |
|      337 | 1863 | `		 && PH7_ClassInstancePresent(pThis,&sPresent,1) ){` |
|      120 | 1864 | `			ph7_hashmap *pMap = (ph7_hashmap *)sPresent.x.pOther;` |
|      120 | 1865 | `			DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,pMap->nEntry);` |
|      120 | 1866 | `			if( !ShowType ){` |
|       62 | 1867 | `				for( i = 0 ; i < nTab ; i++ ){` |
|      ! 0 | 1868 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      ! 0 | 1869 | `				}` |
|       62 | 1870 | `				SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       30 | 1871 | `			}` |
|      120 | 1872 | `			rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth,1);` |
|      120 | 1873 | `			for( i = 0 ; i < nTab ; i++ ){` |
|      ! 0 | 1874 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      ! 0 | 1875 | `			}` |
|      120 | 1876 | `			if( ShowType ){` |
|       59 | 1877 | `				SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|       30 | 1878 | `			}else{` |
|       62 | 1879 | `				SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        - | 1880 | `			}` |
|      120 | 1881 | `			PH7_MemObjRelease(&sPresent);` |
|      120 | 1882 | `			return rc;` |
|        - | 1883 | `		}` |
|      219 | 1884 | `		PH7_MemObjRelease(&sPresent);` |
|        - | 1885 | `	}` |
|        - | 1886 | `	{` |
|        - | 1887 | `		/* Both var_dump and print_r consult __debugInfo() (PHP behavior);` |
|        - | 1888 | `		 * var_export uses a separate renderer and never reaches here. When the` |
|        - | 1889 | `		 * method is present and returns an array, render that array's entries as` |
|        - | 1890 | `		 * the object body, with the header showing the debug array's count. The` |
|        - | 1891 | `		 * nDepth guard above protects against a __debugInfo returning the object` |
|        - | 1892 | `		 * itself. */` |
|      219 | 1893 | `		ph7_class_method *pDbg = PH7_ClassExtractMethod(pThis->pClass,"__debugInfo",sizeof("__debugInfo")-1);` |
|      219 | 1894 | `		if( pDbg ){` |
|        - | 1895 | `			ph7_value sResult;` |
|       18 | 1896 | `			PH7_MemObjInit(pThis->pVm,&sResult);` |
|       18 | 1897 | `			PH7_VmCallMagicMethod(pThis->pVm,pThis,pDbg,&sResult,0,0);` |
|       18 | 1898 | `			if( sResult.iFlags & MEMOBJ_HASHMAP ){` |
|       18 | 1899 | `				ph7_hashmap *pMap = (ph7_hashmap *)sResult.x.pOther;` |
|        - | 1900 | `				/* Header count is the debug array's entry count. */` |
|       18 | 1901 | `				DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,pMap->nEntry);` |
|       18 | 1902 | `				if( !ShowType ){` |
|        8 | 1903 | `					for( i = 0 ; i < nTab ; i++ ){` |
|      ! 0 | 1904 | `						SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      ! 0 | 1905 | `					}` |
|        8 | 1906 | `					SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|        3 | 1907 | `				}` |
|       18 | 1908 | `				rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth,1);` |
|       18 | 1909 | `				for( i = 0 ; i < nTab ; i++ ){` |
|      ! 0 | 1910 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      ! 0 | 1911 | `				}` |
|       18 | 1912 | `				if( ShowType ){` |
|       12 | 1913 | `					SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|        7 | 1914 | `				}else{` |
|        8 | 1915 | `					SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        - | 1916 | `				}` |
|       18 | 1917 | `				PH7_MemObjRelease(&sResult);` |
|       18 | 1918 | `				return rc;` |
|        - | 1919 | `			}` |
|        - | 1920 | `			/* Non-array return: behave as if __debugInfo were absent. */` |
|      ! 0 | 1921 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 | 1922 | `		}` |
|        - | 1923 | `	}` |
|        - | 1924 | `	{` |
|        - | 1925 | `		/* var_dump's header needs the property count up front, so pre-count the` |
|        - | 1926 | `		 * non-static/non-constant attributes (matching the dump loop below).` |
|        - | 1927 | `		 * An UNINITIALIZED typed property is still LISTED by var_dump — with php's` |
|        - | 1928 | ``		 * `uninitialized(T)` marker in place of a value — but it does not COUNT,`` |
|        - | 1929 | `` 		 * which is how php's `object(C)#1 (0) { ["a"]=> uninitialized(int) }` `` |
|        - | 1930 | `		 * reads. */` |
|      202 | 1931 | `		sxu32 nProp = 0;` |
|      202 | 1932 | `		if( ShowType ){` |
|      168 | 1933 | `			SyHashResetLoopCursor(&pThis->hAttr);` |
|      374 | 1934 | `			while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|      209 | 1935 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      206 | 1936 | `				if( !PH7_ATTR_UNPRESENTED(pVmAttr)` |
|      188 | 1937 | `				 && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL) == 0` |
|      191 | 1938 | `				 && !PH7_ClassAttrUninitialized(pVmAttr) ){` |
|      165 | 1939 | `					nProp++;` |
|       81 | 1940 | `				}` |
|        3 | 1941 | `			}` |
|       82 | 1942 | `		}` |
|      202 | 1943 | `		DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,nProp);` |
|        - | 1944 | `	}` |
|      202 | 1945 | `	if( !ShowType ){` |
|        - | 1946 | `		/* print_r body opener: '(' at the container indent */` |
|      150 | 1947 | `		for( i = 0 ; i < nTab ; i++ ){` |
|      114 | 1948 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       58 | 1949 | `		}` |
|       38 | 1950 | `		SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       17 | 1951 | `	}` |
|        - | 1952 | `	/* Dump object attributes (php 8.4: VIRTUAL hooked properties have no` |
|        - | 1953 | `	 * backing store — excluded from var_dump/print_r) */` |
|      202 | 1954 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      350 | 1955 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|      276 | 1956 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      272 | 1957 | `		if( !PH7_ATTR_UNPRESENTED(pVmAttr)` |
|      242 | 1958 | `		 && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL) == 0 ){` |
|      242 | 1959 | `			if( PH7_ClassAttrUninitialized(pVmAttr) ){` |
|        - | 1960 | `				/* var_dump names the property and prints php's marker in place of` |
|        - | 1961 | `				 * the value it has not got; print_r has no such marker and leaves` |
|        - | 1962 | `				 * the property out entirely. */` |
|       50 | 1963 | `				if( ShowType ){` |
|        - | 1964 | `					char zType[192];` |
|       41 | 1965 | `					const char *zText = VmHintTextResolved(pThis->pVm,&pVmAttr->pAttr->sTypeName,` |
|       26 | 1966 | `						VmHintScopeClass(pThis->pVm,pVmAttr->pAttr->pDeclClass,pVmAttr->pOwner),` |
|       13 | 1967 | `						zType,sizeof(zType));` |
|       80 | 1968 | `					for( i = 0 ; i < nTab + 2 ; i++ ){` |
|       54 | 1969 | `						SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       28 | 1970 | `					}` |
|       28 | 1971 | `					OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,TRUE);` |
|       28 | 1972 | `					SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       80 | 1973 | `					for( i = 0 ; i < nTab + 2 ; i++ ){` |
|       54 | 1974 | `						SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       28 | 1975 | `					}` |
|       28 | 1976 | `					SyBlobFormat(&(*pOut),"uninitialized(%s)\n",zText);` |
|       13 | 1977 | `				}` |
|       50 | 1978 | `				continue;` |
|        - | 1979 | `			}` |
|        - | 1980 | `			/* Dump non-static/constant attribute only */` |
|      194 | 1981 | `			pValue = ExtractClassAttrValue(pThis->pVm,pVmAttr);` |
|      194 | 1982 | `			if( pValue == 0 ){` |
|      ! 0 | 1983 | `				continue;` |
|        - | 1984 | `			}` |
|      194 | 1985 | `			if( ShowType ){` |
|        - | 1986 | ``				/* var_dump prop: `["x"(:…)]=>` at nTab+2, the value on the next`` |
|        - | 1987 | `				 * line at the same indent (php). */` |
|     4225 | 1988 | `				for( i = 0 ; i < nTab + 2 ; i++ ){` |
|     4063 | 1989 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     2033 | 1990 | `				}` |
|      165 | 1991 | `				OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,TRUE);` |
|      165 | 1992 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      165 | 1993 | `				rc = PH7_MemObjDump(&(*pOut),pValue,TRUE,nTab+2,nDepth,0);` |
|      165 | 1994 | `				if( rc == SXERR_LIMIT ){` |
|      125 | 1995 | `					break;` |
|        - | 1996 | `				}` |
|       22 | 1997 | `			}else{` |
|        - | 1998 | ``				/* print_r prop: `[x(:…)] => value` at nTab+4; container values`` |
|        - | 1999 | `				 * render their block at nTab+8 followed by php's blank line. */` |
|      208 | 2000 | `				for( i = 0 ; i < nTab + 4 ; i++ ){` |
|      180 | 2001 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       92 | 2002 | `				}` |
|       32 | 2003 | `				OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,FALSE);` |
|       28 | 2004 | `				if( (pValue->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ))` |
|       19 | 2005 | `				 && (pValue->iFlags & MEMOBJ_NULL) == 0 ){` |
|        3 | 2006 | `					rc = PH7_MemObjDump(&(*pOut),pValue,FALSE,nTab+8,nDepth,0);` |
|        3 | 2007 | `					SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        3 | 2008 | `					if( rc == SXERR_LIMIT ){` |
|      ! 0 | 2009 | `						break;` |
|        - | 2010 | `					}` |
|        2 | 2011 | `				}else{` |
|       30 | 2012 | `					PH7_MemObjPrintRInline(&(*pOut),pValue);` |
|       30 | 2013 | `					SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        - | 2014 | `				}` |
|        - | 2015 | `			}` |
|       33 | 2016 | `		}` |
|        4 | 2017 | `	}` |
|     4062 | 2018 | `	for( i = 0 ; i < nTab ; i++ ){` |
|     3863 | 2019 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     1933 | 2020 | `	}` |
|      202 | 2021 | `	if( ShowType ){` |
|      168 | 2022 | `		SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|       86 | 2023 | `	}else{` |
|       38 | 2024 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        - | 2025 | `	}` |
|      202 | 2026 | `	return rc;` |
|      173 | 2027 | `}` |
|        - | 2028 | `/*` |
|        - | 2029 | ` * Call a magic method [i.e: __toString(),__toBool(),__Invoke()...]` |
|        - | 2030 | ` * Return SXRET_OK on successfull call. Any other return value indicates failure.` |
|        - | 2031 | ` * Notes on magic methods.` |
|        - | 2032 | ` * According to the PHP language reference manual.` |
|        - | 2033 | ` *  The function names __construct(), __destruct(), __call(), __callStatic()` |
|        - | 2034 | ` *  __get(),  __toString(), __invoke(), __clone() are magical in PHP classes.` |
|        - | 2035 | ` * You cannot have functions with these names in any of your classes unless` |
|        - | 2036 | ` * you want the magic functionality associated with them.` |
|        - | 2037 | ` * Example of magical methods:` |
|        - | 2038 | ` * __toString()` |
|        - | 2039 | ` *  The __toString() method allows a class to decide how it will react when it is treated like` |
|        - | 2040 | ` *  a string. For example, what echo $obj; will print. This method must return a string.` |
|        - | 2041 | ` *  Example #2 Simple example` |
|        - | 2042 | ` * <?php` |
|        - | 2043 | ` * // Declare a simple class` |
|        - | 2044 | ` * class TestClass` |
|        - | 2045 | ` * {` |
|        - | 2046 | ` *   public $foo;` |
|        - | 2047 | ` *` |
|        - | 2048 | ` *   public function __construct($foo)` |
|        - | 2049 | ` *   {` |
|        - | 2050 | ` *       $this->foo = $foo;` |
|        - | 2051 | ` *   }` |
|        - | 2052 | ` *` |
|        - | 2053 | ` *   public function __toString()` |
|        - | 2054 | ` *   {` |
|        - | 2055 | ` *       return $this->foo;` |
|        - | 2056 | ` *   }` |
|        - | 2057 | ` * }` |
|        - | 2058 | ` * $class = new TestClass('Hello');` |
|        - | 2059 | ` * echo $class;` |
|        - | 2060 | ` * ?>` |
|        - | 2061 | ` * The above example will output:` |
|        - | 2062 | ` *  Hello` |
|        - | 2063 | ` *` |
|        - | 2064 | ` * Note that PH7 does not support all the magical method and introudces __toFloat(),__toInt()` |
|        - | 2065 | ` * which have the same behaviour as __toString() but for float and integer types` |
|        - | 2066 | ` * respectively.` |
|        - | 2067 | ` * Refer to the official documentation for more information.` |
|        - | 2068 | ` */` |
|     6147 | 2069 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallMagicMethod(` |
|        - | 2070 | `	ph7_vm *pVm,               /* VM that own all this stuff */` |
|        - | 2071 | `	ph7_class *pClass,         /* Target class */` |
|        - | 2072 | `	ph7_class_instance *pThis, /* Target object */` |
|        - | 2073 | `	const char *zMethod,       /* Magic method name [i.e: __toString()]*/` |
|        - | 2074 | `	sxu32 nByte,               /* zMethod length*/` |
|        - | 2075 | `	const SyString *pAttrName, /* Attribute name */` |
|        - | 2076 | `	ph7_value *pResult         /* OUT: magic method return value. NULL to discard */` |
|        - | 2077 | `	)` |
|        5 | 2078 | `{` |
|     6152 | 2079 | `	ph7_value *apArg[2] = { 0 , 0 };` |
|        - | 2080 | `	ph7_class_method *pMeth;` |
|        - | 2081 | `	ph7_value sAttr; /* cc warning */` |
|        - | 2082 | `	sxi32 rc;` |
|        - | 2083 | `	int nArg;` |
|        - | 2084 | `	/* Make sure the magic method is available */` |
|     6152 | 2085 | `	pMeth = PH7_ClassExtractMethod(&(*pClass),zMethod,nByte);` |
|     6152 | 2086 | `	if( pMeth == 0 ){` |
|        - | 2087 | `		/* No such method,return immediately */` |
|      ! 0 | 2088 | `		return SXERR_NOTFOUND;` |
|        - | 2089 | `	}` |
|     6152 | 2090 | `	nArg = 0;` |
|        - | 2091 | `	/* Copy arguments */` |
|     6152 | 2092 | `	if( pAttrName ){` |
|     6152 | 2093 | `		PH7_MemObjInitFromString(pVm,&sAttr,pAttrName);` |
|     6152 | 2094 | `		sAttr.nIdx = SXU32_HIGH; /* Mark as constant */` |
|     6152 | 2095 | `		apArg[0] = &sAttr;` |
|     6152 | 2096 | `		nArg = 1;` |
|     3074 | 2097 | `	}` |
|        - | 2098 | `	/* Call the magic method now */` |
|     6152 | 2099 | `	rc = PH7_VmCallMagicMethod(pVm,&(*pThis),pMeth,pResult,nArg,apArg);` |
|        - | 2100 | `	/* Clean up */` |
|     6152 | 2101 | `	if( pAttrName ){` |
|     6152 | 2102 | `		PH7_MemObjRelease(&sAttr);` |
|     3074 | 2103 | `	}` |
|     6152 | 2104 | `	return rc;` |
|     3079 | 2105 | `}` |
|        - | 2106 | `/*` |
|        - | 2107 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon].` |
|        - | 2108 | ` * This function is simply a wrapper on ExtractClassAttrValue().` |
|        - | 2109 | ` */` |
|  5855928 | 2110 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceExtractAttrValue(ph7_class_instance *pThis,VmClassAttr *pAttr)` |
|        5 | 2111 | `{` |
|        - | 2112 | `   /* Extract the attribute value */` |
|        - | 2113 | `	ph7_value *pValue;` |
|  5855933 | 2114 | `	pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|  5855933 | 2115 | `	return pValue;` |
|        5 | 2116 | `}` |
|        - | 2117 | `/*` |
|        - | 2118 | ` * Convert a class instance [i.e: Object in the PHP jargon] into a hashmap [i.e: array in the PHP jargon].` |
|        - | 2119 | ` * Return SXRET_OK on success. Any other value indicates failure.` |
|        - | 2120 | ` * Note on object conversion to array:` |
|        - | 2121 | ` *  Acccording to the PHP language reference manual` |
|        - | 2122 | ` *  If an object is converted to an array, the result is an array whose elements are the object's properties.` |
|        - | 2123 | ` *  The keys are the member variable names.` |
|        - | 2124 | ` *` |
|        - | 2125 | ` *  The following example:` |
|        - | 2126 | ` *  class Test {` |
|        - | 2127 | ` *   public $A = 25<<1;  // 50` |
|        - | 2128 | ` *	 public $c = rand_str(3);   // Random string of length 3` |
|        - | 2129 | ` *	 public $d = rand() & 1023; // Random number between 0..1023` |
|        - | 2130 | ` *  }` |
|        - | 2131 | ` *  var_dump((array) new Test());` |
|        - | 2132 | ` *	Will output:` |
|        - | 2133 | ` *  array(3) {` |
|        - | 2134 | ` *   [A] =>` |
|        - | 2135 | ` *      int(50)` |
|        - | 2136 | ` *   [c] =>` |
|        - | 2137 | ` *     string(3 'aps')` |
|        - | 2138 | ` *   [d] =>` |
|        - | 2139 | ` *     int(991)` |
|        - | 2140 | ` *  }` |
|        - | 2141 | ` * You have noticed that PH7 allow class attributes [i.e: $a,$c,$d in the example above]` |
|        - | 2142 | ` * have any complex expression (even function calls/Annonymous functions) as their default` |
|        - | 2143 | ` * value unlike the standard PHP engine.` |
|        - | 2144 | ` * This is a very powerful feature that you have to look at.` |
|        - | 2145 | ` */` |
|      560 | 2146 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|        5 | 2147 | `{` |
|        - | 2148 | `	{` |
|        - | 2149 | `		/* php's get_properties handler, which is what the (array) cast reads: a` |
|        - | 2150 | `		 * DateTime casts to date/timezone_type/timezone, not to the hidden slots` |
|        - | 2151 | `		 * holding its timestamp. A class whose hook answers only the DEBUG surface` |
|        - | 2152 | `		 * (WeakReference) fills nothing here, and that EMPTY answer is php's — the` |
|        - | 2153 | `		 * hook is authoritative, so there is no falling back to the slot walk. It` |
|        - | 2154 | `		 * used to fall back when the hook filled nothing, which was invisible while` |
|        - | 2155 | `		 * every hooked class also had no visible property: an ArrayObject SUBCLASS` |
|        - | 2156 | ``		 * with an empty storage would have cast to its own `p` where php casts to`` |
|        - | 2157 | `		 * the (empty) storage. */` |
|        - | 2158 | `		ph7_value sPresent;` |
|      565 | 2159 | `		PH7_MemObjInit(pThis->pVm,&sPresent);` |
|      565 | 2160 | `		sPresent.x.pOther = pMap;` |
|      565 | 2161 | `		MemObjSetType(&sPresent,MEMOBJ_HASHMAP);` |
|      565 | 2162 | `		if( PH7_ClassInstancePresent(pThis,&sPresent,0) ){` |
|        - | 2163 | `			/* The map IS the destination; do not release the carrier's hashmap. */` |
|      449 | 2164 | `			sPresent.x.pOther = 0;` |
|      449 | 2165 | `			sPresent.iFlags = MEMOBJ_NULL;` |
|      449 | 2166 | `			return SXRET_OK;` |
|        - | 2167 | `		}` |
|      119 | 2168 | `		sPresent.x.pOther = 0;` |
|      119 | 2169 | `		sPresent.iFlags = MEMOBJ_NULL;` |
|      119 | 2170 | `		PH7_MemObjRelease(&sPresent);` |
|        - | 2171 | `	}` |
|      119 | 2172 | `	return PH7_ClassInstanceToHashmapRaw(pThis,pMap);` |
|      285 | 2173 | `}` |
|        - | 2174 | `/*` |
|        - | 2175 | ` * Is this property NOT THERE YET?` |
|        - | 2176 | ` *` |
|        - | 2177 | ` * php 7.4's typed properties have a third state beside "holds a value" and "does not` |
|        - | 2178 | `` * exist": a typed property with no default is UNINITIALIZED at `new`, reading it is an`` |
|        - | 2179 | ` * Error rather than a null, and every surface that presents an object's properties` |
|        - | 2180 | ` * leaves it out — get_object_vars(), get_mangled_object_vars(), the (array) cast,` |
|        - | 2181 | ` * foreach, json_encode(), serialize(), print_r() and var_export(). var_dump() is the one` |
|        - | 2182 | ``  * exception and only half of one: it NAMES the property and prints `uninitialized(T)` `` |
|        - | 2183 | ` * where the value would be, and does not count it in the header.` |
|        - | 2184 | ` *` |
|        - | 2185 | ` * The state is already tracked (it is what makes the read throw); this asks it by name` |
|        - | 2186 | ` * so the eight surfaces agree. VM_CLASS_ATTR_UNINIT doubles as the readonly write-once` |
|        - | 2187 | ` * latch, which wants the same answer: a readonly property must be typed and cannot have` |
|        - | 2188 | ` * a default, so before its first write php calls it uninitialized too.` |
|        - | 2189 | ` */` |
|     3174 | 2190 | `PH7_PRIVATE int PH7_ClassAttrUninitialized(VmClassAttr *pVmAttr)` |
|        5 | 2191 | `{` |
|     3179 | 2192 | `	return pVmAttr != 0 && (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) != 0;` |
|        5 | 2193 | `}` |
|        - | 2194 | `/*` |
|        - | 2195 | `` * The same question asked by the four surfaces that read through a php 8.4 `get` HOOK —`` |
|        - | 2196 | ` * get_object_vars(), json_encode(), foreach and var_export().` |
|        - | 2197 | ` *` |
|        - | 2198 | ` * A hooked property's value is whatever its hook answers, so an empty backing slot is` |
|        - | 2199 | ` * not an absence there: a VIRTUAL property has no slot at all and still has a value` |
|        - | 2200 | `` * (php lists `virt` in all four), and a BACKED one whose hook reads its own slot raises`` |
|        - | 2201 | ` * the uninitialized Error from inside the hook — which is php's answer for these four` |
|        - | 2202 | ``  * and not something to pre-empt by skipping the property. Only a property with no `get` `` |
|        - | 2203 | ` * at all is absent.` |
|        - | 2204 | ` */` |
|     1114 | 2205 | `PH7_PRIVATE int PH7_ClassAttrUninitializedForRead(VmClassAttr *pVmAttr)` |
|        5 | 2206 | `{` |
|     1181 | 2207 | `	return PH7_ClassAttrUninitialized(pVmAttr)` |
|     1114 | 2208 | `		&& (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET) == 0;` |
|        5 | 2209 | `}` |
|        - | 2210 | `/*` |
|        - | 2211 | ` * The SLOT walk under the cast above, with php's get_properties handler left out:` |
|        - | 2212 | ` * the instance's own property table, mangled, and nothing else.` |
|        - | 2213 | ` *` |
|        - | 2214 | `` * This is what `get_mangled_object_vars()` answers — php asks the same handler with a`` |
|        - | 2215 | ` * different PURPOSE there, and every class here that has a handler answers the raw` |
|        - | 2216 | `` * table for it: `get_mangled_object_vars(new ArrayObject([1,2]))` is the EMPTY array`` |
|        - | 2217 | `` * where the cast is `[1,2]`, and a DateTime's is empty where the cast has three keys.`` |
|        - | 2218 | ` * Since PHL's engine slots are hidden (and php's equivalents live outside the property` |
|        - | 2219 | ` * table entirely), dropping the handler is the whole difference.` |
|        - | 2220 | ` */` |
|      866 | 2221 | `static sxi32 ClassInstanceToHashmapRaw(ph7_class_instance *pThis,ph7_hashmap *pMap,int bOwnOnly)` |
|        5 | 2222 | `{` |
|        - | 2223 | `	SyHashEntry *pEntry;` |
|        - | 2224 | `	SyString *pAttrName;` |
|        - | 2225 | `	VmClassAttr *pAttr;` |
|        - | 2226 | `	ph7_value *pValue;` |
|        - | 2227 | `	ph7_value sName;` |
|        - | 2228 | `	/* Reset the loop cursor */` |
|      871 | 2229 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      871 | 2230 | `	PH7_MemObjInitFromString(pThis->pVm,&sName,0);` |
|     5917 | 2231 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|        - | 2232 | `		/* Point to the current attribute */` |
|     5051 | 2233 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|     5051 | 2234 | `		if( PH7_ATTR_UNPRESENTED(pAttr) ){` |
|        - | 2235 | `			/* A static property is the CLASS's, not the object's: php's cast` |
|        - | 2236 | `			 * yields only the instance's own properties. */` |
|     3792 | 2237 | `			continue;` |
|        - | 2238 | `		}` |
|     1263 | 2239 | `		if( PH7_ClassAttrUninitialized(pAttr) ){` |
|       84 | 2240 | `			continue; /* typed, never written: not there yet (php) */` |
|        - | 2241 | `		}` |
|     1181 | 2242 | `		if( pAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|        - | 2243 | `			/* php 8.4: a VIRTUAL hooked property has no backing store — the` |
|        - | 2244 | `			 * (array) cast excludes it (raw surface, get is NOT dispatched) */` |
|      ! 0 | 2245 | `			continue;` |
|        - | 2246 | `		}` |
|     1181 | 2247 | `		if( bOwnOnly && (pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_NATIVE_SET` |
|        - | 2248 | `			\|PH7_CLASS_ATTR_NATIVE_VIRTUAL\|PH7_CLASS_ATTR_NATIVE_LAZY)) ){` |
|        - | 2249 | `			/* The STATE of a native class whose state happens to be public` |
|        - | 2250 | `			 * (DateInterval's ten, DatePeriod's seven): the caller is building the` |
|        - | 2251 | `			 * shape those belong to, and wants only what the OBJECT added to it. */` |
|      388 | 2252 | `			continue;` |
|        - | 2253 | `		}` |
|        - | 2254 | `		/* Extract attribute value */` |
|      795 | 2255 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|      795 | 2256 | `		if( pValue ){` |
|        - | 2257 | `			/* Build attribute name. php MANGLES the key of a non-public property` |
|        - | 2258 | `			 * when it casts an object to an array: a private one becomes` |
|        - | 2259 | `			 * "\0DeclaringClass\0name" and a protected one "\0*\0name", so two` |
|        - | 2260 | `			 * same-named members from different visibility levels stay distinct` |
|        - | 2261 | ``			 * and `isset($arr['priv'])` is FALSE — PHL emitted the bare name,`` |
|        - | 2262 | `			 * which collided them and answered TRUE. The NULs are real bytes in` |
|        - | 2263 | `			 * the key (this append is length-based, not NUL-terminated). */` |
|      795 | 2264 | `			pAttrName = &pAttr->pAttr->sName;` |
|      795 | 2265 | `			if( pAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|      108 | 2266 | `				ph7_class *pDecl = pAttr->pAttr->pDeclClass` |
|       70 | 2267 | `					? pAttr->pAttr->pDeclClass : pThis->pClass;` |
|       73 | 2268 | `				PH7_MemObjStringAppend(&sName,"\0",1);` |
|       73 | 2269 | `				PH7_MemObjStringAppend(&sName,SyStringData(&pDecl->sName),SyStringLength(&pDecl->sName));` |
|       73 | 2270 | `				PH7_MemObjStringAppend(&sName,"\0",1);` |
|      760 | 2271 | `			}else if( pAttr->pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
|       71 | 2272 | `				PH7_MemObjStringAppend(&sName,"\0*\0",3);` |
|       34 | 2273 | `			}` |
|      795 | 2274 | `			PH7_MemObjStringAppend(&sName,pAttrName->zString,pAttrName->nByte);` |
|        - | 2275 | `			/* Perform the insertion. An OWN-props walk laid beside a shape the` |
|        - | 2276 | `			 * caller already built ADDS rather than updates: php's` |
|        - | 2277 | `			 * add_common_properties is a zend_hash_add, so a subclass property` |
|        - | 2278 | `			 * named like one of the internal keys loses to the internal value` |
|        - | 2279 | ``			 * there (`class S extends DateTime { public $date; }` serializes the`` |
|        - | 2280 | `			 * DATE). */` |
|      795 | 2281 | `			if( bOwnOnly ){` |
|      129 | 2282 | `				ph7_hashmap_node *pDup = 0;` |
|      129 | 2283 | `				if( PH7_HashmapLookup(pMap,&sName,&pDup) == SXRET_OK ){` |
|        3 | 2284 | `					SyBlobReset(&sName.sBlob);` |
|        3 | 2285 | `					continue;` |
|        - | 2286 | `				}` |
|       63 | 2287 | `			}` |
|      793 | 2288 | `			PH7_HashmapInsert(pMap,&sName,pValue);` |
|        - | 2289 | `			/* Reset the string cursor */` |
|      793 | 2290 | `			SyBlobReset(&sName.sBlob);` |
|      394 | 2291 | `		}` |
|        5 | 2292 | `	}` |
|      871 | 2293 | `	PH7_MemObjRelease(&sName);` |
|      871 | 2294 | `	return SXRET_OK;` |
|        5 | 2295 | `}` |
|      184 | 2296 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmapRaw(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|        5 | 2297 | `{` |
|      189 | 2298 | `	return ClassInstanceToHashmapRaw(pThis,pMap,0);` |
|        5 | 2299 | `}` |
|        - | 2300 | `/*` |
|        - | 2301 | ` * The same walk, restricted to what the OBJECT added: a native class's own public` |
|        - | 2302 | ` * STATE is left out, so a subclass's properties can be laid beside the shape that` |
|        - | 2303 | ` * state builds rather than inside it. php's add_common_properties.` |
|        - | 2304 | ` */` |
|      682 | 2305 | `PH7_PRIVATE sxi32 PH7_ClassInstanceOwnPropsToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|        2 | 2306 | `{` |
|      684 | 2307 | `	return ClassInstanceToHashmapRaw(pThis,pMap,1);` |
|        2 | 2308 | `}` |
|        - | 2309 | `/*` |
|        - | 2310 | ` * Iterate throw class attributes and invoke the given callback [i.e: xWalk()] for each` |
|        - | 2311 | ` * retrieved attribute.` |
|        - | 2312 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|        - | 2313 | ` * the attribute value in the callback body will not alter the real attribute value.` |
|        - | 2314 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|        - | 2315 | ` * a value different from PH7_OK.` |
|        - | 2316 | ` * Refer to [ph7_object_walk()] for more information.` |
|        - | 2317 | ` */` |
|      ! 0 | 2318 | `PH7_PRIVATE sxi32 PH7_ClassInstanceWalk(` |
|        - | 2319 | `	ph7_class_instance *pThis, /* Target object */` |
|        - | 2320 | `	int (*xWalk)(const char *,ph7_value *,void *), /* Walker callback */` |
|        - | 2321 | `	void *pUserData /* Last argument to xWalk() */` |
|        - | 2322 | `	)` |
|      ! 0 | 2323 | `{` |
|        - | 2324 | `	SyHashEntry *pEntry; /* Hash entry */` |
|        - | 2325 | `	VmClassAttr *pAttr;  /* Pointer to the attribute */` |
|        - | 2326 | `	ph7_value *pValue;   /* Attribute value */` |
|        - | 2327 | `	ph7_value sValue;    /* Copy of the attribute value */` |
|        - | 2328 | `	int rc;` |
|        - | 2329 | `	/* Reset the loop cursor */` |
|      ! 0 | 2330 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      ! 0 | 2331 | `	PH7_MemObjInit(pThis->pVm,&sValue);` |
|        - | 2332 | `	/* Start the walk process */` |
|      ! 0 | 2333 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|        - | 2334 | `		/* Point to the current attribute */` |
|      ! 0 | 2335 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      ! 0 | 2336 | `		if( PH7_ATTR_UNPRESENTED(pAttr) ){` |
|        - | 2337 | `			/* Class-level members are not part of the object (php) */` |
|      ! 0 | 2338 | `			continue;` |
|        - | 2339 | `		}` |
|      ! 0 | 2340 | `		if( PH7_ClassAttrUninitialized(pAttr) ){` |
|      ! 0 | 2341 | `			continue; /* typed, never written: not there yet (php) */` |
|        - | 2342 | `		}` |
|        - | 2343 | `		/* Extract attribute value */` |
|      ! 0 | 2344 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|      ! 0 | 2345 | `		if( pValue ){` |
|      ! 0 | 2346 | `			PH7_MemObjLoad(pValue,&sValue);` |
|        - | 2347 | `			/* Invoke the supplied callback */` |
|      ! 0 | 2348 | `			rc =  xWalk(SyStringData(&pAttr->pAttr->sName),&sValue,pUserData);` |
|      ! 0 | 2349 | `			PH7_MemObjRelease(&sValue);` |
|      ! 0 | 2350 | `			if( rc != PH7_OK){` |
|        - | 2351 | `				/* User callback request an operation abort */` |
|      ! 0 | 2352 | `				return SXERR_ABORT;` |
|        - | 2353 | `			}` |
|      ! 0 | 2354 | `		}` |
|      ! 0 | 2355 | `	}` |
|        - | 2356 | `	/* All done */` |
|      ! 0 | 2357 | `	return SXRET_OK;` |
|      ! 0 | 2358 | `}` |
|        - | 2359 | `/*` |
|        - | 2360 | ` * The instance's attribute entry for a property name, INCLUDING the empty one.` |
|        - | 2361 | ` *` |
|        - | 2362 | ` * SyHashGet answers 0 for a zero-length key engine-wide, so a property named ""` |
|        - | 2363 | `` * — php's own `s:0:""` payload builds one, and every surface that walks hAttr`` |
|        - | 2364 | ` * shows it — can only be found by walking the list. Used by the presentation` |
|        - | 2365 | ` * surfaces that snapshot names and re-look-up each one before reading it (a PHP` |
|        - | 2366 | ` * 8.4 get hook may unset a property mid-walk), where the plain hash probe would` |
|        - | 2367 | ` * silently drop it from the output while var_dump, which walks directly, showed` |
|        - | 2368 | ` * it. The walk uses the hash's embedded loop cursor, so it must not run inside` |
|        - | 2369 | ` * another walk of the SAME table — the callers below re-enter only through the` |
|        - | 2370 | ` * hook dispatch, which happens after this returns.` |
|        - | 2371 | ` */` |
|      486 | 2372 | `PH7_PRIVATE SyHashEntry * PH7_ClassInstanceAttrEntry(ph7_class_instance *pThis,const char *zName,sxu32 nName)` |
|        5 | 2373 | `{` |
|        - | 2374 | `	SyHashEntry *pEntry;` |
|      491 | 2375 | `	if( nName > 0 ){` |
|      473 | 2376 | `		return SyHashGet(&pThis->hAttr,(const void *)zName,nName);` |
|        - | 2377 | `	}` |
|       19 | 2378 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|       29 | 2379 | `	while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 | 2380 | `		if( pEntry->nKeyLen == 0 ){` |
|       11 | 2381 | `			return pEntry;` |
|        - | 2382 | `		}` |
|        1 | 2383 | `	}` |
|        9 | 2384 | `	return 0;` |
|      248 | 2385 | `}` |
|        - | 2386 | `/*` |
|        - | 2387 | ` * Extract a class atrribute value.` |
|        - | 2388 | ` * Return a pointer to the attribute value on success. Otherwise NULL.` |
|        - | 2389 | ` * Note:` |
|        - | 2390 | ` *  Access to static and constant attribute is not allowed. That is,the function` |
|        - | 2391 | ` *  will return NULL in case someone (host-application code) try to extract` |
|        - | 2392 | ` *  a static/constant attribute.` |
|        - | 2393 | ` */` |
|  1826326 | 2394 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceFetchAttr(ph7_class_instance *pThis,const SyString *pName)` |
|        5 | 2395 | `{` |
|        - | 2396 | `	SyHashEntry *pEntry;` |
|        - | 2397 | `	VmClassAttr *pAttr;` |
|        - | 2398 | `	/* Query the attribute hashtable */` |
|  1826331 | 2399 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);` |
|  1826331 | 2400 | `	if( pEntry == 0 ){` |
|        - | 2401 | `		/* No such attribute */` |
|      553 | 2402 | `		return 0;` |
|        - | 2403 | `	}` |
|        - | 2404 | `	/* Point to the class atrribute */` |
|  1825781 | 2405 | `	pAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - | 2406 | `	/* Check if we are dealing with a static/constant attribute */` |
|  1825781 | 2407 | `	if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - | 2408 | `		/* Access is forbidden */` |
|      ! 0 | 2409 | `		return 0;` |
|        - | 2410 | `	}` |
|        - | 2411 | `	/* Return the attribute value */` |
|  1825781 | 2412 | `	return ExtractClassAttrValue(pThis->pVm,pAttr);` |
|   913191 | 2413 | `}` |
|        - | 2414 | `/*` |
|        - | 2415 | `` * Does a WRITE through `$obj[k]` land on this class's storage? php answers with`` |
|        - | 2416 | ` * the class's read_dimension handler: an internal class whose own handler hands` |
|        - | 2417 | ` * back the real element supports indirect modification, while everything routed` |
|        - | 2418 | ` * through zend_std_read_dimension — every userland ArrayAccess, and the SPL` |
|        - | 2419 | ` * classes that keep the standard handler (SplFixedArray, the SplDoublyLinkedList` |
|        - | 2420 | ` * family, SplObjectStorage) — gets a TEMPORARY back, so the write is lost and php` |
|        - | 2421 | ` * says so. The engine cannot tell those apart from the interface alone: both` |
|        - | 2422 | ` * implement ArrayAccess.` |
|        - | 2423 | ` *` |
|        - | 2424 | ` * PHL's equivalent evidence is the offsetGet that ANSWERS: the native one on a` |
|        - | 2425 | ` * class carrying PH7_CLASS_DIM_WRITABLE means the storage is reachable, and a` |
|        - | 2426 | ` * userland OVERRIDE of it means it is not — which is php's own rule for an` |
|        - | 2427 | ` * ArrayObject subclass (spl_array_read_dimension steps aside for a declared` |
|        - | 2428 | `` * offsetGet). A `&offsetGet` return is php's other yes: it hands back a reference,`` |
|        - | 2429 | ` * so the write reaches whatever it aliases.` |
|        - | 2430 | ` */` |
|      492 | 2431 | `PH7_PRIVATE int PH7_VmDimFetchWritable(ph7_class *pClass)` |
|        5 | 2432 | `{` |
|        - | 2433 | `	ph7_class_method *pGet;` |
|        - | 2434 | `	ph7_class *pCur;` |
|      497 | 2435 | `	if( pClass == 0 ){` |
|      ! 0 | 2436 | `		return FALSE;` |
|        - | 2437 | `	}` |
|      497 | 2438 | `	pGet = PH7_ClassExtractMethod(pClass,"offsetGet",sizeof("offsetGet")-1);` |
|      497 | 2439 | `	if( pGet == 0 ){` |
|      ! 0 | 2440 | `		return FALSE;` |
|        - | 2441 | `	}` |
|      497 | 2442 | `	if( pGet->sFunc.iFlags & VM_FUNC_REF_RETURN ){` |
|        5 | 2443 | `		return TRUE;` |
|        - | 2444 | `	}` |
|      493 | 2445 | `	if( (pGet->sFunc.iFlags & VM_FUNC_NATIVE) == 0 ){` |
|      161 | 2446 | `		return FALSE;` |
|        - | 2447 | `	}` |
|      512 | 2448 | `	for( pCur = pClass ; pCur ; pCur = pCur->pBase ){` |
|      404 | 2449 | `		if( pCur->iFlags & PH7_CLASS_DIM_WRITABLE ){` |
|      226 | 2450 | `			return TRUE;` |
|        - | 2451 | `		}` |
|       91 | 2452 | `	}` |
|      110 | 2453 | `	return FALSE;` |
|      251 | 2454 | `}` |
|        - | 2455 | `/*` |
|        - | 2456 | ` * The three accessors a VM_FUNC_NATIVE method body uses to reach its receiver.` |
|        - | 2457 | ` *` |
|        - | 2458 | ` * A native method is dispatched down the host-function path, so it is handed the` |
|        - | 2459 | ` * plain (ph7_context*, argc, argv) of any builtin — the receiver rides on the` |
|        - | 2460 | ` * context instead of occupying an argument slot. That is the whole point: the C` |
|        - | 2461 | `` * routine that used to be a global `__prefix_verb($target, ...)` thunk taking its`` |
|        - | 2462 | ` * target explicitly becomes a method whose target is $this.` |
|        - | 2463 | ` */` |
|        - | 2464 | `/*` |
|        - | 2465 | ` * The receiver, or NULL when the method was called statically (and always NULL in` |
|        - | 2466 | ` * a plain host function). Borrowed: the operand stack holds the reference for the` |
|        - | 2467 | ` * duration of the call, so the body must not unref it.` |
|        - | 2468 | ` */` |
|  1566480 | 2469 | `PH7_PRIVATE ph7_class_instance * PH7_ContextThis(ph7_context *pCtx)` |
|        5 | 2470 | `{` |
|  1566485 | 2471 | `	return pCtx->pThis;` |
|        5 | 2472 | `}` |
|        - | 2473 | `/*` |
|        - | 2474 | ` * The class the call was made THROUGH — php's late-static-binding target, so an` |
|        - | 2475 | ` * inherited native method sees the SUBCLASS here, not the class that declared it.` |
|        - | 2476 | ` * NULL in a plain host function.` |
|        - | 2477 | ` */` |
|     1056 | 2478 | `PH7_PRIVATE ph7_class * PH7_ContextCalledClass(ph7_context *pCtx)` |
|        5 | 2479 | `{` |
|     1061 | 2480 | `	return pCtx->pCalledClass;` |
|        5 | 2481 | `}` |
|        - | 2482 | `/*` |
|        - | 2483 | ` * The receiver as a ph7_value, so the body can use the ordinary object helpers` |
|        - | 2484 | ` * (ph7_object_fetch_attr, ph7_value_is_object, ph7_result_value, ...) instead of` |
|        - | 2485 | ` * reaching into ph7_class_instance. NULL when there is no receiver.` |
|        - | 2486 | ` *` |
|        - | 2487 | ` * The view ALIASES the instance without bumping its refcount: it lives exactly as` |
|        - | 2488 | ` * long as the call, during which the caller's reference is already keeping the` |
|        - | 2489 | ` * object alive, and VmReleaseCallContext drops the alias without unreffing. Handing` |
|        - | 2490 | ` * it to ph7_result_value() is safe — that copies through PH7_MemObjStore, which` |
|        - | 2491 | ` * takes its own reference.` |
|        - | 2492 | ` */` |
|     7022 | 2493 | `PH7_PRIVATE ph7_value * PH7_ContextThisValue(ph7_context *pCtx)` |
|        5 | 2494 | `{` |
|     7027 | 2495 | `	if( pCtx->pThis == 0 ){` |
|      ! 0 | 2496 | `		return 0;` |
|        - | 2497 | `	}` |
|     7027 | 2498 | `	if( !pCtx->bThisInit ){` |
|     7027 | 2499 | `		PH7_MemObjInit(pCtx->pVm,&pCtx->sThis);` |
|     7027 | 2500 | `		pCtx->sThis.x.pOther = pCtx->pThis;` |
|     7027 | 2501 | `		pCtx->sThis.iFlags = MEMOBJ_OBJ;` |
|     7027 | 2502 | `		pCtx->sThis.nIdx = SXU32_HIGH;` |
|     7027 | 2503 | `		pCtx->bThisInit = 1;` |
|     3511 | 2504 | `	}` |
|     7027 | 2505 | `	return &pCtx->sThis;` |
|     3516 | 2506 | `}` |
|        - | 2507 |  |
