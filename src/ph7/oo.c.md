# src/ph7/oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 736/840 lines (87.62%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h"` |
|       - |    7 | `/*` |
|       - |    8 | ` * This file implement an Object Oriented (OO) subsystem for the PH7 engine.` |
|       - |    9 | ` */` |
|       - |   10 | `/*` |
|       - |   11 | ` * Create an empty class.` |
|       - |   12 | ` * Return a pointer to a raw class (ph7_class instance) on success. NULL otherwise.` |
|       - |   13 | ` */` |
|  528296 |   14 | `PH7_PRIVATE ph7_class * PH7_NewRawClass(ph7_vm *pVm,const SyString *pName,sxu32 nLine)` |
|       5 |   15 | `{` |
|       - |   16 | `	ph7_class *pClass;` |
|       - |   17 | `	char *zName;` |
|       - |   18 | `	/* Allocate a new instance */` |
|  528301 |   19 | `	pClass = (ph7_class *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class));` |
|  528301 |   20 | `	if( pClass == 0 ){` |
|     ! 0 |   21 | `		return 0;` |
|       - |   22 | `	}` |
|       - |   23 | `	/* Zero the structure */` |
|  528301 |   24 | `	SyZero(pClass,sizeof(ph7_class));` |
|       - |   25 | `	/* Duplicate class name */` |
|  528301 |   26 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  528301 |   27 | `	if( zName == 0 ){` |
|     ! 0 |   28 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClass);` |
|     ! 0 |   29 | `		return 0;` |
|       - |   30 | `	}` |
|       - |   31 | `	/* Initialize fields */` |
|  528301 |   32 | `	SyStringInitFromBuf(&pClass->sName,zName,pName->nByte);` |
|       - |   33 | `	/* php method names are CASE-INSENSITIVE ($o->FOO() finds foo(), and declaring` |
|       - |   34 | `	 * both is a redeclaration), so the method table matches on them the same way` |
|       - |   35 | `	 * hClass does for class names. Properties and class constants ARE case` |
|       - |   36 | `	 * sensitive in php, so hAttr keeps the default exact comparator. */` |
|  528301 |   37 | `	SyHashInit(&pClass->hMethod,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|  528301 |   38 | `	SyHashInit(&pClass->hAttr,&pVm->sAllocator,0,0);` |
|  528301 |   39 | `	SyHashInit(&pClass->hDerived,&pVm->sAllocator,0,0);` |
|  528301 |   40 | `	SySetInit(&pClass->aInterface,&pVm->sAllocator,sizeof(ph7_class *));` |
|  528301 |   41 | `	SySetInit(&pClass->aTrait,&pVm->sAllocator,sizeof(ph7_class *));` |
|  528301 |   42 | `	SySetInit(&pClass->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|  528301 |   43 | `	SySetInit(&pClass->aEnumCases,&pVm->sAllocator,sizeof(ph7_class_attr *));` |
|  528301 |   44 | `	pClass->nLine = nLine;` |
|  528301 |   45 | `	if( pVm->bCompilingBuiltin ){` |
|       - |   46 | `		/* Defined by an embedded builtin chunk: internal, no defining file.` |
|       - |   47 | `		 * Class compilers merge further flags with \|= so this survives. */` |
|  526053 |   48 | `		pClass->iFlags \|= PH7_CLASS_INTERNAL;` |
|  263029 |   49 | `	}else{` |
|       - |   50 | `		/* Alias the VM-lifetime path dup on top of the include stack */` |
|    2253 |   51 | `		SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|    2253 |   52 | `		if( pFile ){` |
|    2253 |   53 | `			SyStringDupPtr(&pClass->sFile,pFile);` |
|    1124 |   54 | `		}` |
|       - |   55 | `	}` |
|       - |   56 | `	/* All done */` |
|  528301 |   57 | `	return pClass;` |
|  264153 |   58 | `}` |
|       - |   59 | `/*` |
|       - |   60 | ` * Allocate and initialize a new class attribute.` |
|       - |   61 | ` * Return a pointer to the class attribute on success. NULL otherwise.` |
|       - |   62 | ` */` |
|  945270 |   63 | `PH7_PRIVATE ph7_class_attr * PH7_NewClassAttr(ph7_vm *pVm,const SyString *pName,sxu32 nLine,sxi32 iProtection,sxi32 iFlags)` |
|       5 |   64 | `{` |
|       - |   65 | `	ph7_class_attr *pAttr;` |
|       - |   66 | `	char *zName;` |
|  945275 |   67 | `	pAttr = (ph7_class_attr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_attr));` |
|  945275 |   68 | `	if( pAttr == 0 ){` |
|     ! 0 |   69 | `		return 0;` |
|       - |   70 | `	}` |
|       - |   71 | `	/* Zero the structure */` |
|  945275 |   72 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|  945275 |   73 | `	SySetInit(&pAttr->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|       - |   74 | `	/* Duplicate attribute name */` |
|  945275 |   75 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  945275 |   76 | `	if( zName == 0 ){` |
|     ! 0 |   77 | `		SyMemBackendPoolFree(&pVm->sAllocator,pAttr);` |
|     ! 0 |   78 | `		return 0;` |
|       - |   79 | `	}` |
|       - |   80 | `	/* Initialize fields */` |
|  945275 |   81 | `	SySetInit(&pAttr->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|  945275 |   82 | `	SySetInit(&pAttr->aUnionAlts,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|  945275 |   83 | `	SyStringInitFromBuf(&pAttr->sName,zName,pName->nByte);` |
|  945275 |   84 | `	pAttr->iProtection = iProtection;` |
|  945275 |   85 | `	pAttr->nIdx = SXU32_HIGH;` |
|  945275 |   86 | `	pAttr->iFlags = iFlags;` |
|  945275 |   87 | `	pAttr->nLine = nLine;` |
|  945275 |   88 | `	return pAttr;` |
|  472640 |   89 | `}` |
|       - |   90 | `/*` |
|       - |   91 | ` * Allocate and initialize a new class method.` |
|       - |   92 | ` * Return a pointer to the class method on success. NULL otherwise` |
|       - |   93 | ` * This function associate with the newly created method an automatically generated` |
|       - |   94 | ` * random unique name.` |
|       - |   95 | ` */` |
| 2887564 |   96 | `PH7_PRIVATE ph7_class_method * PH7_NewClassMethod(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,sxu32 nLine,` |
|       - |   97 | `	sxi32 iProtection,sxi32 iFlags,sxi32 iFuncFlags)` |
|       5 |   98 | `{` |
|       - |   99 | `	ph7_class_method *pMeth;` |
|       - |  100 | `	SyHashEntry *pEntry;` |
|       - |  101 | `	SyString *pNamePtr;` |
|       - |  102 | `	char zSalt[10];` |
|       - |  103 | `	char *zName;` |
|       - |  104 | `	sxu32 nByte;` |
|       - |  105 | `	/* Allocate a new class method instance */` |
| 2887569 |  106 | `	pMeth = (ph7_class_method *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_method));` |
| 2887569 |  107 | `	if( pMeth == 0 ){` |
|     ! 0 |  108 | `		return 0;` |
|       - |  109 | `	}` |
|       - |  110 | `	/* Zero the structure */` |
| 2887569 |  111 | `	SyZero(pMeth,sizeof(ph7_class_method));` |
|       - |  112 | `	/* Check for an already installed method with the same name */` |
| 2887569 |  113 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
| 2887569 |  114 | `	if( pEntry == 0 ){` |
|       - |  115 | `		/* Associate an unique VM name to this method */` |
| 2887565 |  116 | `		nByte = sizeof(zSalt) + pName->nByte + SyStringLength(&pClass->sName)+sizeof(char)*7/*[[__'\0'*/;` |
| 2887565 |  117 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,nByte);` |
| 2887565 |  118 | `		if( zName == 0 ){` |
|     ! 0 |  119 | `			SyMemBackendPoolFree(&pVm->sAllocator,pMeth);` |
|     ! 0 |  120 | `			return 0;` |
|       - |  121 | `		}` |
| 2887565 |  122 | `		pNamePtr = &pMeth->sVmName;` |
|       - |  123 | `		/* Generate a random string */` |
| 2887565 |  124 | `		PH7_VmRandomString(&(*pVm),zSalt,sizeof(zSalt));` |
| 2887565 |  125 | `		pNamePtr->nByte = SyBufferFormat(zName,nByte,"[__%z@%z_%.*s]",&pClass->sName,pName,sizeof(zSalt),zSalt);` |
| 2887565 |  126 | `		pNamePtr->zString = zName;` |
| 1443785 |  127 | `	}else{` |
|       - |  128 | `		/* Method is condidate for 'overloading' */` |
|       6 |  129 | `		ph7_class_method *pCurrent = (ph7_class_method *)pEntry->pUserData;` |
|       6 |  130 | `		pNamePtr = &pMeth->sVmName;` |
|       - |  131 | `		/* Use the same VM name */` |
|       6 |  132 | `		SyStringDupPtr(pNamePtr,&pCurrent->sVmName);` |
|       6 |  133 | `		zName = (char *)pNamePtr->zString;` |
|       - |  134 | `	}` |
| 2887569 |  135 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|  166402 |  136 | `		if( (pName->nByte == sizeof("__destruct") - 1 && SyMemcmp(pName->zString,"__destruct",sizeof("__destruct") - 1 ) == 0)` |
|  166407 |  137 | `			\|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|       - |  138 | `				/* Switch to public visibility for destructors and legacy class-name` |
|       - |  139 | `				 * constructors (the engine invokes destructors internally, bypassing` |
|       - |  140 | `				 * visibility either way). __construct KEEPS its declared visibility` |
|       - |  141 | ``				 * (band A #4): php enforces it at `new` — a private/protected ctor`` |
|       - |  142 | `				 * from the wrong scope is a catchable Error, checked at OP_NEW —` |
|       - |  143 | `				 * and ReflectionClass::isInstantiable()/newInstance() now see it. */` |
|     ! 0 |  144 | `				iProtection = PH7_CLASS_PROT_PUBLIC;` |
|     ! 0 |  145 | `		}` |
|   83201 |  146 | `	}` |
|       - |  147 | `	/* Initialize method fields */` |
| 2887569 |  148 | `	pMeth->iProtection = iProtection;` |
| 2887569 |  149 | `	pMeth->iFlags = iFlags;` |
| 2887569 |  150 | `	pMeth->nLine = nLine;` |
| 4331351 |  151 | `	PH7_VmInitFuncState(&(*pVm),&pMeth->sFunc,&zName[sizeof(char)*4/*[__@*/+SyStringLength(&pClass->sName)],` |
| 2887564 |  152 | `		pName->nByte,iFuncFlags\|VM_FUNC_CLASS_METHOD,pClass);` |
| 2887569 |  153 | `	return pMeth;` |
| 1443787 |  154 | `}` |
|       - |  155 | `/*` |
|       - |  156 | ` * Check if the given name have a class method associated with it.` |
|       - |  157 | ` * Return the desired method [i.e: ph7_class_method instance] on success. NULL otherwise.` |
|       - |  158 | ` */` |
| 3795242 |  159 | `PH7_PRIVATE ph7_class_method * PH7_ClassExtractMethod(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|       5 |  160 | `{` |
|       - |  161 | `	SyHashEntry *pEntry;` |
|       - |  162 | `	/* Perform a hash lookup */` |
| 3795247 |  163 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)zName,nByte);` |
| 3795247 |  164 | `	if( pEntry == 0 ){` |
|       - |  165 | `		/* No such entry */` |
| 2918399 |  166 | `		return 0;` |
|       - |  167 | `	}` |
|       - |  168 | `	/* Point to the desired method */` |
|  876853 |  169 | `	return (ph7_class_method *)pEntry->pUserData;` |
| 1897626 |  170 | `}` |
|       - |  171 | `/*` |
|       - |  172 | ` * Check if the given name is a class attribute.` |
|       - |  173 | ` * Return the desired attribute [i.e: ph7_class_attr instance] on success.NULL otherwise.` |
|       - |  174 | ` */` |
| 2790504 |  175 | `PH7_PRIVATE ph7_class_attr * PH7_ClassExtractAttribute(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|       5 |  176 | `{` |
|       - |  177 | `	SyHashEntry *pEntry;` |
|       - |  178 | `	/* Perform a hash lookup */` |
| 2790509 |  179 | `	pEntry = SyHashGet(&pClass->hAttr,(const void *)zName,nByte);` |
| 2790509 |  180 | `	if( pEntry == 0 ){` |
|       - |  181 | `		/* No such entry */` |
| 2789035 |  182 | `		return 0;` |
|       - |  183 | `	}` |
|       - |  184 | `	/* Point to the desierd method */` |
|    1479 |  185 | `	return (ph7_class_attr *)pEntry->pUserData;` |
| 1395257 |  186 | `}` |
|       - |  187 | `/*` |
|       - |  188 | ` * Install a class attribute in the corresponding container.` |
|       - |  189 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|       - |  190 | ` */` |
|  945266 |  191 | `PH7_PRIVATE sxi32 PH7_ClassInstallAttr(ph7_class *pClass,ph7_class_attr *pAttr)` |
|       5 |  192 | `{` |
|  945271 |  193 | `	SyString *pName = &pAttr->sName;` |
|       - |  194 | `	sxi32 rc;` |
|       - |  195 | `	/* Remember where this attribute was originally declared so that later` |
|       - |  196 | `	 * inheritance/trait copies still know the declaring class (needed for` |
|       - |  197 | `	 * PHP-compatible error messages on typed properties). */` |
|  945271 |  198 | `	if( pAttr->pDeclClass == 0 ){` |
|  945271 |  199 | `		pAttr->pDeclClass = pClass;` |
|  472633 |  200 | `	}` |
|  945271 |  201 | `	rc = SyHashInsertTail(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|  945271 |  202 | `	return rc;` |
|       5 |  203 | `}` |
|       - |  204 | `/*` |
|       - |  205 | ` * Install a class method in the corresponding container.` |
|       - |  206 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|       - |  207 | ` */` |
| 2887546 |  208 | `PH7_PRIVATE sxi32 PH7_ClassInstallMethod(ph7_class *pClass,ph7_class_method *pMeth)` |
|       5 |  209 | `{` |
| 2887551 |  210 | `	SyString *pName = &pMeth->sFunc.sName;` |
|       - |  211 | `	sxi32 rc;` |
| 2887551 |  212 | `	rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 2887551 |  213 | `	return rc;` |
|       5 |  214 | `}` |
|       - |  215 | `/*` |
|       - |  216 | ` * Method-override compatibility (variance) checking.` |
|       - |  217 | ` *` |
|       - |  218 | ` * PHP rejects an override whose signature is incompatible with the parent's:` |
|       - |  219 | ` * return types are covariant (child may only narrow), parameter types are` |
|       - |  220 | ` * contravariant (child may only widen), and a child may not add a required` |
|       - |  221 | ` * parameter. We add the diagnostic — but conservatively: PHL must keep running` |
|       - |  222 | ` * valid PHP, so the comparator below is SKIP-BY-DEFAULT. It flags only cases that` |
|       - |  223 | ` * are unambiguously invalid and silently accepts anything subtle (unions,` |
|       - |  224 | ` * intersections, pseudo-types, self/parent/static, object, unresolved classes,` |
|       - |  225 | ` * or a missing type), so it can never reject valid code.` |
|       - |  226 | ` */` |
|       - |  227 | `#define OVT_NONE   0  /* no declared type */` |
|       - |  228 | `#define OVT_SCALAR 1  /* a concrete invariant scalar: int/float/string/bool/array */` |
|       - |  229 | `#define OVT_CLASS  2  /* a real, already-loaded class/interface */` |
|       - |  230 | `#define OVT_SKIP   3  /* union/intersection/pseudo/self/object/unresolved — never flag */` |
|       - |  231 |  |
|       - |  232 | `/*` |
|       - |  233 | ` * Classify one declared type (nType + class name + union flag) for override` |
|       - |  234 | ` * comparison. On OVT_CLASS, *ppClass receives the resolved class. Class names are` |
|       - |  235 | ` * resolved by a direct, autoload-free hClass lookup: a miss (forward reference,` |
|       - |  236 | ` * namespaced, or not-yet-loaded) yields OVT_SKIP, which the caller accepts.` |
|       - |  237 | ` */` |
|  278732 |  238 | `static int OoClassifyOverrideType(ph7_vm *pVm, sxu32 nType, const SyString *pClass,` |
|       - |  239 | `	int bUnion, ph7_class **ppClass)` |
|       5 |  240 | `{` |
|  278737 |  241 | `	*ppClass = 0;` |
|  278737 |  242 | `	if( bUnion ){` |
|       3 |  243 | `		return OVT_SKIP; /* union/intersection — full lattice, skip */` |
|       - |  244 | `	}` |
|  278735 |  245 | `	if( nType == 0 ){` |
|  278631 |  246 | `		return OVT_NONE; /* no declared type */` |
|       - |  247 | `	}` |
|     108 |  248 | `	if( nType == SXU32_HIGH ){` |
|       - |  249 | `		/* A class name OR a pseudo-type stored as a name atom. Skip every pseudo` |
|       - |  250 | `		 * (incl. self/parent/static, which are context-relative). */` |
|       - |  251 | `		static const struct { const char *z; sxu32 n; } aPseudo[] = {` |
|       - |  252 | `			{"mixed",5}, {"never",5}, {"iterable",8}, {"callable",8}, {"true",4},` |
|       - |  253 | `			{"false",5}, {"self",4}, {"parent",6}, {"static",6}` |
|       - |  254 | `		};` |
|      23 |  255 | `		const char *z = pClass->zString;` |
|      23 |  256 | `		sxu32 n = pClass->nByte;` |
|       - |  257 | `		SyHashEntry *pE;` |
|       - |  258 | `		sxu32 i;` |
|     159 |  259 | `		for( i = 0; i < SX_ARRAYSIZE(aPseudo); i++ ){` |
|     147 |  260 | `			if( n == aPseudo[i].n && SyStrnmicmp(z,aPseudo[i].z,n) == 0 ){` |
|      10 |  261 | `				return OVT_SKIP;` |
|       - |  262 | `			}` |
|      70 |  263 | `		}` |
|      14 |  264 | `		pE = SyHashGet(&pVm->hClass,(const void *)z,n);` |
|      14 |  265 | `		if( pE == 0 ){` |
|     ! 0 |  266 | `			return OVT_SKIP; /* not loaded / forward ref / namespaced — accept */` |
|       - |  267 | `		}` |
|      14 |  268 | `		*ppClass = (ph7_class *)pE->pUserData;` |
|      14 |  269 | `		return OVT_CLASS;` |
|       - |  270 | `	}` |
|      84 |  271 | `	if( nType == MEMOBJ_STRING \|\| nType == MEMOBJ_INT \|\| nType == MEMOBJ_REAL` |
|      30 |  272 | `	 \|\| nType == MEMOBJ_BOOL \|\| nType == MEMOBJ_HASHMAP ){` |
|      63 |  273 | `		return OVT_SCALAR;` |
|       - |  274 | `	}` |
|       - |  275 | `	/* MEMOBJ_OBJ (object — subtypes against classes), MEMOBJ_VOID/NULL/RES,` |
|       - |  276 | `	 * or anything unexpected: skip. */` |
|      26 |  277 | `	return OVT_SKIP;` |
|  139371 |  278 | `}` |
|       - |  279 |  |
|       - |  280 | `/*` |
|       - |  281 | ` * A declared type normalized for override comparison: the raw type code, the` |
|       - |  282 | ` * class-name string (when a class), and the union/nullable flags. Extracted once` |
|       - |  283 | ` * from each side so the comparator takes two of these instead of eight scalars.` |
|       - |  284 | ` */` |
|       - |  285 | `typedef struct OvType OvType;` |
|       - |  286 | `struct OvType {` |
|       - |  287 | `	sxu32 nType;` |
|       - |  288 | `	const SyString *pClass;` |
|       - |  289 | `	int bUnion;` |
|       - |  290 | `	int bNullable;` |
|       - |  291 | `};` |
|  209044 |  292 | `static OvType OoTypeFromReturn(ph7_vm_func *pF)` |
|       5 |  293 | `{` |
|       - |  294 | `	OvType t;` |
|  209049 |  295 | `	t.nType = pF->nReturnType;` |
|  209049 |  296 | `	t.pClass = &pF->sReturnClass;` |
|  209049 |  297 | `	t.bUnion = SySetUsed(&pF->aReturnUnion) > 0;` |
|  209049 |  298 | `	t.bNullable = (pF->iFlags & VM_FUNC_RETURN_NULLABLE) != 0;` |
|  209049 |  299 | `	return t;` |
|       5 |  300 | `}` |
|   69688 |  301 | `static OvType OoTypeFromArg(ph7_vm_func_arg *pA)` |
|       5 |  302 | `{` |
|       - |  303 | `	OvType t;` |
|   69693 |  304 | `	t.nType = pA->nType;` |
|   69693 |  305 | `	t.pClass = &pA->sClass;` |
|   69693 |  306 | `	t.bUnion = (pA->iFlags & VM_FUNC_ARG_UNION) != 0;` |
|   69693 |  307 | `	t.bNullable = (pA->iFlags & VM_FUNC_ARG_NULLABLE) != 0;` |
|   69693 |  308 | `	return t;` |
|       5 |  309 | `}` |
|       - |  310 | `/*` |
|       - |  311 | ` * Return TRUE if the child type is an unambiguously-invalid override of the` |
|       - |  312 | ` * parent type. bCovariant=1 for a return type (child must be ⊆ parent),` |
|       - |  313 | ` * 0 for a parameter (child must be ⊇ parent). Returns FALSE (accept) on any` |
|       - |  314 | ` * skipped/ambiguous shape.` |
|       - |  315 | ` */` |
|  139366 |  316 | `static int OoOverrideTypeBad(ph7_vm *pVm, OvType parent, OvType child, int bCovariant)` |
|       5 |  317 | `{` |
|       - |  318 | `	ph7_class *pParentCls, *pChildCls;` |
|  139371 |  319 | `	int kP = OoClassifyOverrideType(pVm, parent.nType, parent.pClass, parent.bUnion, &pParentCls);` |
|  139371 |  320 | `	int kC = OoClassifyOverrideType(pVm, child.nType, child.pClass, child.bUnion, &pChildCls);` |
|  139371 |  321 | `	if( kP == OVT_SKIP \|\| kC == OVT_SKIP ){` |
|      21 |  322 | `		return 0; /* ambiguous shape — conservatively accept */` |
|       - |  323 | `	}` |
|       - |  324 | `	/* A missing type is the TOP type. covariant (return): a concrete child is a` |
|       - |  325 | `	 * subtype of top, fine; a top child over a concrete parent WIDENS → bad.` |
|       - |  326 | `	 * contravariant (param): a top child is a supertype of anything, fine; a` |
|       - |  327 | `	 * concrete child over a top parent NARROWS → bad. (A union/intersection child` |
|       - |  328 | `	 * already fell into OVT_SKIP above, so a flagged child here is scalar/class.) */` |
|  139353 |  329 | `	if( kP == OVT_NONE \|\| kC == OVT_NONE ){` |
|  139323 |  330 | `		if( bCovariant && kC == OVT_NONE && kP != OVT_NONE ) return 1;` |
|  139323 |  331 | `		if( !bCovariant && kP == OVT_NONE && kC != OVT_NONE ) return 1;` |
|  139323 |  332 | `		return 0;` |
|       - |  333 | `	}` |
|       - |  334 | `	/* Nullability: a covariant return may not ADD null; a contravariant param may` |
|       - |  335 | `	 * not REMOVE null. */` |
|      34 |  336 | `	if( bCovariant ){` |
|      17 |  337 | `		if( child.bNullable && !parent.bNullable ) return 1;` |
|      10 |  338 | `	}else{` |
|      19 |  339 | `		if( parent.bNullable && !child.bNullable ) return 1;` |
|       - |  340 | `	}` |
|      34 |  341 | `	if( kP == OVT_SCALAR && kC == OVT_SCALAR ){` |
|       - |  342 | `		/* Scalars are invariant — they must match exactly. */` |
|      27 |  343 | `		return (parent.nType != child.nType) ? 1 : 0;` |
|       - |  344 | `	}` |
|       8 |  345 | `	if( kP == OVT_CLASS && kC == OVT_CLASS ){` |
|       8 |  346 | `		if( bCovariant ){` |
|       3 |  347 | `			return PH7_VmInstanceOf(pChildCls, pParentCls) ? 0 : 1;  /* child ⊆ parent */` |
|       - |  348 | `		}` |
|       6 |  349 | `		return PH7_VmInstanceOf(pParentCls, pChildCls) ? 0 : 1;      /* child ⊇ parent */` |
|       - |  350 | `	}` |
|       - |  351 | `	/* One scalar and one class — disjoint. */` |
|     ! 0 |  352 | `	return 1;` |
|   69688 |  353 | `}` |
|       - |  354 |  |
|       - |  355 | `/*` |
|       - |  356 | ` * Check a child method's signature against the parent method it overrides.` |
|       - |  357 | ` * Emits a PHP-style "Declaration of … must be compatible …" fatal on a clear` |
|       - |  358 | `` * incompatibility. `__construct` is exempt (PHP does not apply variance to it).`` |
|       - |  359 | ` */` |
|  147078 |  360 | `static sxi32 OoCheckOverrideCompat(ph7_gen_state *pGen, ph7_class *pBase, ph7_class *pSub,` |
|       - |  361 | `	ph7_class_method *pParent, ph7_class_method *pChild)` |
|       5 |  362 | `{` |
|  147083 |  363 | `	ph7_vm *pVm = pGen->pVm;` |
|  147083 |  364 | `	ph7_vm_func *pPF = &pParent->sFunc;` |
|  147083 |  365 | `	ph7_vm_func *pCF = &pChild->sFunc;` |
|  147083 |  366 | `	SyString *pMName = &pCF->sName;` |
|       - |  367 | `	ph7_vm_func_arg *aP, *aC;` |
|       - |  368 | `	sxu32 nPArg, nCArg, k;` |
|  147083 |  369 | `	int bBad = 0;` |
|  147078 |  370 | `	if( pMName->nByte == sizeof("__construct")-1` |
|   96760 |  371 | `	 && SyStrnmicmp(pMName->zString,"__construct",pMName->nByte) == 0 ){` |
|   42561 |  372 | `		return SXRET_OK;` |
|       - |  373 | `	}` |
|       - |  374 | `	/* Return type — covariant. */` |
|  104527 |  375 | `	bBad = OoOverrideTypeBad(pVm, OoTypeFromReturn(pPF), OoTypeFromReturn(pCF), /* bCovariant */ 1);` |
|       - |  376 | `	/* Each overlapping parameter — contravariant. */` |
|  104527 |  377 | `	nPArg = SySetUsed(&pPF->aArgs);` |
|  104527 |  378 | `	nCArg = SySetUsed(&pCF->aArgs);` |
|  104527 |  379 | `	aP = (ph7_vm_func_arg *)SySetBasePtr(&pPF->aArgs);` |
|  104527 |  380 | `	aC = (ph7_vm_func_arg *)SySetBasePtr(&pCF->aArgs);` |
|  139371 |  381 | `	for( k = 0; !bBad && k < nPArg && k < nCArg; k++ ){` |
|   34849 |  382 | `		bBad = OoOverrideTypeBad(pVm, OoTypeFromArg(&aP[k]), OoTypeFromArg(&aC[k]), /* bCovariant */ 0);` |
|   17427 |  383 | `	}` |
|       - |  384 | `	/* Parameter arity: the child must declare at least the parent's parameters and` |
|       - |  385 | `	 * may add only OPTIONAL ones — PHP rejects dropping any param (even an optional` |
|       - |  386 | `	 * one) or adding a required one. Skip the rule if either signature is variadic` |
|       - |  387 | `	 * (arity semantics differ). */` |
|  104527 |  388 | `	if( !bBad ){` |
|  104523 |  389 | `		int bVariadic = 0;` |
|  139365 |  390 | `		for( k = 0; k < nPArg; k++ ){ if( aP[k].iFlags & VM_FUNC_ARG_VARIADIC ) bVariadic = 1; }` |
|  139367 |  391 | `		for( k = 0; k < nCArg; k++ ){ if( aC[k].iFlags & VM_FUNC_ARG_VARIADIC ) bVariadic = 1; }` |
|  104523 |  392 | `		if( !bVariadic ){` |
|  104523 |  393 | `			if( nCArg < nPArg ){` |
|     ! 0 |  394 | `				bBad = 1; /* dropped a parent parameter */` |
|     ! 0 |  395 | `			}else{` |
|  104525 |  396 | `				for( k = nPArg; k < nCArg; k++ ){` |
|       3 |  397 | `					if( SySetUsed(&aC[k].aByteCode) == 0 ){ bBad = 1; break; } /* new required */` |
|       2 |  398 | `				}` |
|       - |  399 | `			}` |
|   52259 |  400 | `		}` |
|   52259 |  401 | `	}` |
|  104527 |  402 | `	if( bBad ){` |
|       8 |  403 | `		sxi32 rc = PH7_GenCompileError(&(*pGen),E_ERROR,pChild->nLine,` |
|       - |  404 | `			"Declaration of %z::%z() must be compatible with %z::%z()",` |
|       2 |  405 | `			&pSub->sName,pMName,&pBase->sName,&pParent->sFunc.sName);` |
|       6 |  406 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  407 | `			return SXERR_ABORT;` |
|       - |  408 | `		}` |
|       2 |  409 | `	}` |
|  104527 |  410 | `	return SXRET_OK;` |
|   73544 |  411 | `}` |
|       - |  412 | `/*` |
|       - |  413 | ` * Perform an inheritance operation.` |
|       - |  414 | ` * According to the PHP language reference manual` |
|       - |  415 | ` *  When you extend a class, the subclass inherits all of the public and protected methods` |
|       - |  416 | ` *  from the parent class. Unless a class Overwrites those methods, they will retain their original` |
|       - |  417 | ` *  functionality.` |
|       - |  418 | ` *  This is useful for defining and abstracting functionality, and permits the implementation` |
|       - |  419 | ` *  of additional functionality in similar objects without the need to reimplement all of the shared` |
|       - |  420 | ` *  functionality.` |
|       - |  421 | ` *  Example #1 Inheritance Example` |
|       - |  422 | ` * <?php` |
|       - |  423 | ` * class foo` |
|       - |  424 | ` * {` |
|       - |  425 | ` *   public function printItem($string)` |
|       - |  426 | ` *   {` |
|       - |  427 | ` *       echo 'Foo: ' . $string . PHP_EOL;` |
|       - |  428 | ` *   }` |
|       - |  429 | ` *` |
|       - |  430 | ` *   public function printPHP()` |
|       - |  431 | ` *   {` |
|       - |  432 | ` *       echo 'PHP is great.' . PHP_EOL;` |
|       - |  433 | ` *   }` |
|       - |  434 | ` * }` |
|       - |  435 | ` * class bar extends foo` |
|       - |  436 | ` * {` |
|       - |  437 | ` *   public function printItem($string)` |
|       - |  438 | ` *   {` |
|       - |  439 | ` *       echo 'Bar: ' . $string . PHP_EOL;` |
|       - |  440 | ` *   }` |
|       - |  441 | ` * }` |
|       - |  442 | ` * $foo = new foo();` |
|       - |  443 | ` * $bar = new bar();` |
|       - |  444 | ` * $foo->printItem('baz'); // Output: 'Foo: baz'` |
|       - |  445 | ` * $foo->printPHP();       // Output: 'PHP is great'` |
|       - |  446 | ` * $bar->printItem('baz'); // Output: 'Bar: baz'` |
|       - |  447 | ` * $bar->printPHP();       // Output: 'PHP is great'` |
|       - |  448 | ` *` |
|       - |  449 | ` * This function return SXRET_OK if the inheritance operation was successfully performed.` |
|       - |  450 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|       - |  451 | ` * error message.` |
|       - |  452 | ` */` |
|  247868 |  453 | `PH7_PRIVATE sxi32 PH7_ClassInherit(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase)` |
|       5 |  454 | `{` |
|       - |  455 | `	ph7_class_method *pMeth;` |
|       - |  456 | `	ph7_class_attr *pAttr;` |
|       - |  457 | `	SyHashEntry *pEntry;` |
|       - |  458 | `	SyString *pName;` |
|       - |  459 | `	SySet aInherited; /* base attributes to prepend (see the copy loop below) */` |
|       - |  460 | `	sxi32 rc;` |
|  247873 |  461 | `	SySetInit(&aInherited,&pGen->pVm->sAllocator,sizeof(ph7_class_attr *));` |
|       - |  462 | `	/* Install in the derived hashtable */` |
|  247873 |  463 | `	rc = SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|  247873 |  464 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  465 | `		SySetRelease(&aInherited);` |
|     ! 0 |  466 | `		return rc;` |
|       - |  467 | `	}` |
|       - |  468 | `	/* readonly class inheritance (PHP 8.2): a readonly class may only extend a` |
|       - |  469 | `	 * readonly class, and a non-readonly class may not extend a readonly one. */` |
|  247873 |  470 | `	if( (pBase->iFlags & PH7_CLASS_READONLY) != (pSub->iFlags & PH7_CLASS_READONLY) ){` |
|       5 |  471 | `		if( pBase->iFlags & PH7_CLASS_READONLY ){` |
|       4 |  472 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|       - |  473 | `				"Non-readonly class %z cannot extend readonly class %z",` |
|       1 |  474 | `				&pSub->sName,&pBase->sName);` |
|       2 |  475 | `		}else{` |
|       4 |  476 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|       - |  477 | `				"Readonly class %z cannot extend non-readonly class %z",` |
|       1 |  478 | `				&pSub->sName,&pBase->sName);` |
|       - |  479 | `		}` |
|       5 |  480 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  481 | `			SySetRelease(&aInherited);` |
|     ! 0 |  482 | `			return SXERR_ABORT;` |
|       - |  483 | `		}` |
|       2 |  484 | `	}` |
|       - |  485 | `	/* Copy public/protected attributes from the base class */` |
|  247873 |  486 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
| 1416573 |  487 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|       - |  488 | `		/* Make sure the private attributes are not redeclared in the subclass */` |
| 1168705 |  489 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
| 1168705 |  490 | `		pName = &pAttr->sName;` |
| 1168705 |  491 | `		if( (pEntry = SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|      20 |  492 | `			if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_FINAL))` |
|      12 |  493 | `				== (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_FINAL) ){` |
|       - |  494 | `				/* Cannot override a final class constant (PHP 8.1). Report the` |
|       - |  495 | `				 * class that originally declared it (pDeclClass) rather than the` |
|       - |  496 | `				 * immediate base, so a multi-level chain matches PHP. */` |
|       3 |  497 | `				ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pBase;` |
|       4 |  498 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_attr *)pEntry->pUserData)->nLine,` |
|       - |  499 | `					"%z::%z cannot override final constant %z::%z",` |
|       1 |  500 | `					&pSub->sName,pName,&pOwner->sName,pName);` |
|       3 |  501 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  502 | `					SySetRelease(&aInherited);` |
|     ! 0 |  503 | `					return SXERR_ABORT;` |
|       - |  504 | `				}` |
|       1 |  505 | `			}` |
|       - |  506 | `			/* A child MAY redeclare a base's private property: php treats the two` |
|       - |  507 | `			 * as independent members (each private to its declaring class), with no` |
|       - |  508 | `			 * diagnostic. PH7 warned here, which is wrong — the child's entry simply` |
|       - |  509 | `			 * shadows the base's in the by-name attribute table.` |
|       - |  510 | `			 *` |
|       - |  511 | `			 * Ordering: php keeps an overridden INSTANCE property at the position` |
|       - |  512 | `` 			 * the BASE declared it (`class G{$g1;$g2;} class H extends G{$h;$g1;}` `` |
|       - |  513 | `			 * iterates g1,g2,h — not g2,h,g1). Re-collect the CHILD's definition,` |
|       - |  514 | `			 * whose default value wins, and drop its current entry so the prepend` |
|       - |  515 | `			 * below re-inserts it in base order. Statics/constants are not part of` |
|       - |  516 | `			 * instance iteration, so they keep their existing slot. */` |
|      22 |  517 | `			if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|      20 |  518 | `				ph7_class_attr *pOwn = (ph7_class_attr *)pEntry->pUserData;` |
|      20 |  519 | `				SyHashDeleteEntry(&pSub->hAttr,(const void *)pName->zString,pName->nByte,0);` |
|      20 |  520 | `				rc = SySetPut(&aInherited,(const void *)&pOwn);` |
|      20 |  521 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  522 | `					SySetRelease(&aInherited);` |
|     ! 0 |  523 | `					return rc;` |
|       - |  524 | `				}` |
|       9 |  525 | `			}` |
|      22 |  526 | `			continue;` |
|       - |  527 | `		}` |
|       - |  528 | `		/* Collect the attribute. php: a base class's private INSTANCE property` |
|       - |  529 | `		 * lives on every child instance too (its own methods read/write it` |
|       - |  530 | `		 * through $this on the child; the access check grants private access by` |
|       - |  531 | `		 * DECLARING class, so child methods and outsiders still can't touch it).` |
|       - |  532 | `		 * Private STATICS/CONSTANTS stay uncopied — base methods reach those` |
|       - |  533 | `		 * through self:: against the declaring class directly.` |
|       - |  534 | `		 *` |
|       - |  535 | `		 * These are gathered rather than installed here because php orders an` |
|       - |  536 | `		 * instance's properties BASE-DECLARED FIRST, then the subclass's own,` |
|       - |  537 | `		 * then trait members — while inheritance runs AFTER the subclass body` |
|       - |  538 | `		 * has already filled hAttr. They are prepended below. */` |
| 1168680 |  539 | `		if( pAttr->iProtection != PH7_CLASS_PROT_PRIVATE` |
|  744946 |  540 | `		 \|\| (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
| 1168679 |  541 | `			rc = SySetPut(&aInherited,(const void *)&pAttr);` |
| 1168679 |  542 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  543 | `				SySetRelease(&aInherited);` |
|     ! 0 |  544 | `				return rc;` |
|       - |  545 | `			}` |
|  584337 |  546 | `		}` |
|       5 |  547 | `	}` |
|       - |  548 | `	/* Prepend the collected base attributes so hAttr reads base-first. The` |
|       - |  549 | `	 * table's iteration list is what every object-iteration consumer walks` |
|       - |  550 | `	 * (var_dump/print_r, get_object_vars, foreach, (array) casts, json_encode),` |
|       - |  551 | `	 * so this ordering is user-visible — json_encode emits its keys in exactly` |
|       - |  552 | `	 * this order. SyHashInsert is a HEAD insert, so walking the collected set` |
|       - |  553 | `	 * backwards leaves the base's own declaration order at the front. */` |
|  247873 |  554 | `	if( SySetUsed(&aInherited) > 0 ){` |
|  247729 |  555 | `		ph7_class_attr **apInherited = (ph7_class_attr **)SySetBasePtr(&aInherited);` |
|  247729 |  556 | `		sxu32 n = SySetUsed(&aInherited);` |
| 1416421 |  557 | `		while( n > 0 ){` |
| 1168697 |  558 | `			ph7_class_attr *pIn = apInherited[--n];` |
| 1168697 |  559 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pIn->sName.zString,pIn->sName.nByte,pIn);` |
| 1168697 |  560 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  561 | `				SySetRelease(&aInherited);` |
|     ! 0 |  562 | `				return rc;` |
|       - |  563 | `			}` |
|       5 |  564 | `		}` |
|  123862 |  565 | `	}` |
|  247873 |  566 | `	SySetRelease(&aInherited);` |
|  247873 |  567 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
| 3610185 |  568 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|       - |  569 | `		/* Make sure the private/final methods are not redeclared in the subclass */` |
| 3362317 |  570 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
| 3362317 |  571 | `		pName = &pMeth->sFunc.sName;` |
| 3362317 |  572 | `		if( (pEntry = SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|  147087 |  573 | `			 if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|       - |  574 | `				/* php: "Cannot override final method A::test()" */` |
|       7 |  575 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_method *)pEntry->pUserData)->nLine,` |
|       - |  576 | `					"Cannot override final method %z::%z()",` |
|       2 |  577 | `					&pBase->sName,pName);` |
|       2 |  578 | `				(void)pSub;` |
|       5 |  579 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  580 | `					return SXERR_ABORT;` |
|       - |  581 | `				}` |
|       3 |  582 | `			}else{` |
|       - |  583 | `				/* Check the override's signature is compatible with the parent's. */` |
|  220622 |  584 | `				rc = OoCheckOverrideCompat(&(*pGen),pBase,pSub,pMeth,` |
|  147078 |  585 | `					(ph7_class_method *)pEntry->pUserData);` |
|  147083 |  586 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  587 | `					return SXERR_ABORT;` |
|       - |  588 | `				}` |
|       - |  589 | `			}` |
|  147087 |  590 | `			continue;` |
|       - |  591 | `		}` |
|       - |  592 | `		/* Install the method. php: a base class's private INSTANCE method is` |
|       - |  593 | `		 * dispatchable on child instances too — an inherited public method` |
|       - |  594 | `		 * calling $this->priv() must find it (the call-site visibility check` |
|       - |  595 | `		 * binds by DECLARING class, sFunc.pUserData, so child code and` |
|       - |  596 | `		 * outsiders still can't call it; a private ctor copied down also` |
|       - |  597 | ``		 * blocks `new Child` from outside like php). Private STATICS stay`` |
|       - |  598 | `		 * uncopied — base methods reach those through self:: against the` |
|       - |  599 | `		 * declaring class directly. */` |
| 3215230 |  600 | `		if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE` |
| 1623101 |  601 | `		 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
| 3215235 |  602 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 3215235 |  603 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  604 | `				return rc;` |
|       - |  605 | `			}` |
| 1607615 |  606 | `		}` |
|       5 |  607 | `	}` |
|       - |  608 | `	/* Mark as subclass */` |
|  247873 |  609 | `	pSub->pBase = pBase;` |
|       - |  610 | `	/* All done */` |
|  247873 |  611 | `	return SXRET_OK;` |
|  123939 |  612 | `}` |
|       - |  613 | `/*` |
|       - |  614 | ` * Apply a trait to a class: copy all methods and attributes from the trait` |
|       - |  615 | ` * into the target class. Unlike inheritance, traits copy ALL members including` |
|       - |  616 | ` * private ones. Members already defined in the class take precedence.` |
|       - |  617 | ` */` |
|   15560 |  618 | `PH7_PRIVATE sxi32 PH7_ClassUseTrait(ph7_gen_state *pGen,ph7_class *pClass,ph7_class *pTrait)` |
|       5 |  619 | `{` |
|       - |  620 | `	ph7_class_method *pMeth;` |
|       - |  621 | `	ph7_class_attr *pAttr;` |
|       - |  622 | `	SyHashEntry *pEntry;` |
|       - |  623 | `	SyString *pName;` |
|       - |  624 | `	sxi32 rc;` |
|       - |  625 | `	/* Detect cyclic trait composition (e.g. trait A { use B; } trait B { use A; }) */` |
|   15565 |  626 | `	if( pTrait->iFlags & PH7_CLASS_TRAIT_VISITING ){` |
|     ! 0 |  627 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|     ! 0 |  628 | `			"Trait circular reference detected: %z is already being applied",&pTrait->sName);` |
|     ! 0 |  629 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  630 | `			return SXERR_ABORT;` |
|       - |  631 | `		}` |
|     ! 0 |  632 | `		return SXRET_OK;` |
|       - |  633 | `	}` |
|   15565 |  634 | `	pTrait->iFlags \|= PH7_CLASS_TRAIT_VISITING;` |
|   15565 |  635 | `	rc = SXRET_OK;` |
|       - |  636 | `	/* Copy attributes from the trait */` |
|   15565 |  637 | `	SyHashResetLoopCursor(&pTrait->hAttr);` |
|   62007 |  638 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hAttr)) != 0 ){` |
|       - |  639 | `		SyHashEntry *pExisting;` |
|   46447 |  640 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   46447 |  641 | `		pName = &pAttr->sName;` |
|   46447 |  642 | `		pExisting = SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte);` |
|   46447 |  643 | `		if( pExisting != 0 ){` |
|       - |  644 | `			/* Attribute already exists. Check if it came from another trait` |
|       - |  645 | `			 * and whether the definitions are compatible (same defaults).` |
|       - |  646 | `			 */` |
|       - |  647 | `			ph7_class **apUsedTraits;` |
|       - |  648 | `			sxu32 nUsed,k;` |
|       6 |  649 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       6 |  650 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|       6 |  651 | `			for(k = 0; k < nUsed; k++){` |
|       - |  652 | `				ph7_class_attr *pOther;` |
|       3 |  653 | `				pOther = PH7_ClassExtractAttribute(apUsedTraits[k],pName->zString,pName->nByte);` |
|       3 |  654 | `				if( pOther ){` |
|       - |  655 | `					/* Two traits define the same property — check if defaults differ */` |
|       3 |  656 | `					ph7_class_attr *pClassAttr = (ph7_class_attr *)pExisting->pUserData;` |
|       4 |  657 | `					if( SySetUsed(&pAttr->aByteCode) != SySetUsed(&pClassAttr->aByteCode) \|\|` |
|       3 |  658 | `						(SySetUsed(&pAttr->aByteCode) > 0 &&` |
|       3 |  659 | `						 SyMemcmp(SySetBasePtr(&pAttr->aByteCode),SySetBasePtr(&pClassAttr->aByteCode),` |
|       3 |  660 | `							SySetUsed(&pAttr->aByteCode) * SySetElemSize(&pAttr->aByteCode)) != 0) ){` |
|       4 |  661 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine,` |
|       - |  662 | `							"%z and %z define the same property ($%z) in the composition of %z. "` |
|       - |  663 | `							"However, the definition differs and is considered incompatible",` |
|       2 |  664 | `							&apUsedTraits[k]->sName,&pTrait->sName,pName,&pClass->sName);` |
|       3 |  665 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  666 | `							goto cleanup;` |
|       - |  667 | `						}` |
|       1 |  668 | `					}` |
|       3 |  669 | `					break;` |
|       - |  670 | `				}` |
|     ! 0 |  671 | `			}` |
|       6 |  672 | `			continue;` |
|       - |  673 | `		}` |
|   46443 |  674 | `		rc = SyHashInsertTail(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|   46443 |  675 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  676 | `			goto cleanup;` |
|       - |  677 | `		}` |
|       5 |  678 | `	}` |
|       - |  679 | `	/* Copy methods from the trait */` |
|   15565 |  680 | `	SyHashResetLoopCursor(&pTrait->hMethod);` |
|  224531 |  681 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hMethod)) != 0 ){` |
|       - |  682 | `		SyHashEntry *pClassMethEntry;` |
|  208971 |  683 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  208971 |  684 | `		pName = &pMeth->sFunc.sName;` |
|  208971 |  685 | `		pClassMethEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
|  208971 |  686 | `		if( pClassMethEntry != 0 ){` |
|       - |  687 | `			/* Method already exists in the class. An ABSTRACT trait method is a` |
|       - |  688 | `			 * REQUIREMENT, not an implementation: php satisfies it with any concrete` |
|       - |  689 | `			 * method of the same name (from the class body or another trait) — no` |
|       - |  690 | `			 * collision. Only two CONCRETE trait methods actually conflict. */` |
|      18 |  691 | `			ph7_class_method *pExistingMeth = (ph7_class_method *)pClassMethEntry->pUserData;` |
|      18 |  692 | `			int bIncomingAbstract = (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0;` |
|      18 |  693 | `			int bExistingAbstract = (pExistingMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0;` |
|       - |  694 | `			ph7_class **apUsedTraits;` |
|       - |  695 | `			sxu32 nUsed,k;` |
|      18 |  696 | `			if( bIncomingAbstract ){` |
|       - |  697 | `				/* Incoming abstract requirement: the existing (concrete or abstract)` |
|       - |  698 | `				 * method already covers this name — keep it. */` |
|       8 |  699 | `				continue;` |
|       - |  700 | `			}` |
|      11 |  701 | `			if( bExistingAbstract ){` |
|       - |  702 | `				/* Existing entry is only an abstract requirement (from an earlier` |
|       - |  703 | `				 * trait): the incoming concrete method satisfies and replaces it. */` |
|       3 |  704 | `				pClassMethEntry->pUserData = (void *)pMeth;` |
|       3 |  705 | `				continue;` |
|       - |  706 | `			}` |
|       - |  707 | `			/* Both concrete: a genuine collision only when the OTHER definition came` |
|       - |  708 | `			 * from another trait (a concrete one). A class-body method wins silently. */` |
|       8 |  709 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       8 |  710 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|       8 |  711 | `			for(k = 0; k < nUsed; k++){` |
|       3 |  712 | `				ph7_class_method *pOtherMeth = PH7_ClassExtractMethod(apUsedTraits[k],pName->zString,pName->nByte);` |
|       3 |  713 | `				if( pOtherMeth != 0 && (pOtherMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       - |  714 | `					/* Two different traits define the same CONCRETE method with no resolution */` |
|       4 |  715 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|       - |  716 | `						"Trait method %z::%z has not been applied as %z::%z, "` |
|       - |  717 | `						"because of collision with %z::%z",` |
|       2 |  718 | `						&pTrait->sName,pName,` |
|       1 |  719 | `						&pClass->sName,pName,` |
|       2 |  720 | `						&apUsedTraits[k]->sName,pName);` |
|       3 |  721 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  722 | `						goto cleanup;` |
|       - |  723 | `					}` |
|       3 |  724 | `					break;` |
|       - |  725 | `				}` |
|     ! 0 |  726 | `			}` |
|       - |  727 | `			/* Class-defined method takes precedence */` |
|       8 |  728 | `			continue;` |
|       - |  729 | `		}` |
|  208957 |  730 | `		rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|  208957 |  731 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  732 | `			goto cleanup;` |
|       - |  733 | `		}` |
|       5 |  734 | `	}` |
|       - |  735 | `	/* Record trait in the class */` |
|   15565 |  736 | `	SySetPut(&pClass->aTrait,(const void *)&pTrait);` |
|    7780 |  737 | `cleanup:` |
|       - |  738 | `	/* Always clear visiting flag, even on error paths */` |
|   15565 |  739 | `	pTrait->iFlags &= ~PH7_CLASS_TRAIT_VISITING;` |
|    7780 |  740 | `	SXUNUSED(pGen);` |
|   15565 |  741 | `	return rc;` |
|    7785 |  742 | `}` |
|       - |  743 | `/*` |
|       - |  744 | ` * Inherit an object interface from another object interface.` |
|       - |  745 | ` * According to the PHP language reference manual.` |
|       - |  746 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|       - |  747 | ` *  must implement, without having to define how these methods are handled.` |
|       - |  748 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  749 | ` *  class, but without any of the methods having their contents defined.` |
|       - |  750 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  751 | ` *` |
|       - |  752 | ` * This function return SXRET_OK if the interface inheritance operation was successfully performed.` |
|       - |  753 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|       - |  754 | ` * error message.` |
|       - |  755 | ` */` |
|   27090 |  756 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceInherit(ph7_class *pSub,ph7_class *pBase)` |
|       5 |  757 | `{` |
|       - |  758 | `	ph7_class_method *pMeth;` |
|       - |  759 | `	ph7_class_attr *pAttr;` |
|       - |  760 | `	SyHashEntry *pEntry;` |
|       - |  761 | `	SyString *pName;` |
|       - |  762 | `	sxi32 rc;` |
|       - |  763 | `	/* Install in the derived hashtable */` |
|   27095 |  764 | `	SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|   27095 |  765 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
|       - |  766 | `	/* Copy constants */` |
|   40642 |  767 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|       - |  768 | `		/* Make sure the constants are not redeclared in the subclass */` |
|       3 |  769 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|       3 |  770 | `		pName = &pAttr->sName;` |
|       3 |  771 | `		if( SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte) == 0 ){` |
|       - |  772 | `			/* Install the constant in the subclass */` |
|       3 |  773 | `			rc = SyHashInsertTail(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|       3 |  774 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  775 | `				return rc;` |
|       - |  776 | `			}` |
|       1 |  777 | `		}` |
|       1 |  778 | `	}` |
|   27095 |  779 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
|       - |  780 | `	/* Copy methods signature */` |
|  106440 |  781 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|       - |  782 | `		/* Make sure the method are not redeclared in the subclass */` |
|   65805 |  783 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   65805 |  784 | `		pName = &pMeth->sFunc.sName;` |
|   65805 |  785 | `		if( SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte) == 0 ){` |
|       - |  786 | `			/* Install the method */` |
|   65805 |  787 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|   65805 |  788 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  789 | `				return rc;` |
|       - |  790 | `			}` |
|   32900 |  791 | `		}` |
|       5 |  792 | `	}` |
|       - |  793 | `	/* Mark as subclass */` |
|   27095 |  794 | `	pSub->pBase = pBase;` |
|       - |  795 | `	/* All done */` |
|   27095 |  796 | `	return SXRET_OK;` |
|   13550 |  797 | `}` |
|       - |  798 | `/*` |
|       - |  799 | ` * Implements an object interface in the given main class.` |
|       - |  800 | ` * According to the PHP language reference manual.` |
|       - |  801 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|       - |  802 | ` *  must implement, without having to define how these methods are handled.` |
|       - |  803 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  804 | ` *  class, but without any of the methods having their contents defined.` |
|       - |  805 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  806 | ` *` |
|       - |  807 | ` * This function return SXRET_OK if the interface was successfully implemented.` |
|       - |  808 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|       - |  809 | ` * error message.` |
|       - |  810 | ` */` |
|  421974 |  811 | `PH7_PRIVATE sxi32 PH7_ClassImplement(ph7_class *pMain,ph7_class *pInterface)` |
|       5 |  812 | `{` |
|       - |  813 | `	ph7_class_attr *pAttr;` |
|       - |  814 | `	SyHashEntry *pEntry;` |
|       - |  815 | `	SyString *pName;` |
|       - |  816 | `	sxi32 rc;` |
|       - |  817 | `	/* First off,copy all constants declared inside the interface */` |
|  421979 |  818 | `	SyHashResetLoopCursor(&pInterface->hAttr);` |
|  741284 |  819 | `	while((pEntry = SyHashGetNextEntry(&pInterface->hAttr)) != 0 ){` |
|       - |  820 | `		/* Point to the constant declaration */` |
|  108323 |  821 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  108323 |  822 | `		pName = &pAttr->sName;` |
|       - |  823 | `		/* Make sure the attribute is not redeclared in the main class */` |
|  108323 |  824 | `		if( SyHashGet(&pMain->hAttr,pName->zString,pName->nByte) == 0 ){` |
|       - |  825 | `			/* Install the attribute */` |
|  108319 |  826 | `			rc = SyHashInsertTail(&pMain->hAttr,pName->zString,pName->nByte,pAttr);` |
|  108319 |  827 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  828 | `				return rc;` |
|       - |  829 | `			}` |
|   54157 |  830 | `		}` |
|       5 |  831 | `	}` |
|       - |  832 | `	/* Install in the interface container */` |
|  421979 |  833 | `	SySetPut(&pMain->aInterface,(const void *)&pInterface);` |
|       - |  834 | `	/* Install interface method stubs into the implementing class.` |
|       - |  835 | `	 * Methods already defined in the class take precedence (they satisfy` |
|       - |  836 | `	 * the interface contract). Stubs retain PH7_CLASS_ATTR_ABSTRACT so` |
|       - |  837 | `	 * the unified check in GenStateCheckAbstractMethods catches missing ones.` |
|       - |  838 | `	 */` |
|       - |  839 | `	{` |
|       - |  840 | `		ph7_class_method *pMeth;` |
|       - |  841 | `		SyHashEntry *pMEntry;` |
|       - |  842 | `		SyString *pMName;` |
|  421979 |  843 | `		SyHashResetLoopCursor(&pInterface->hMethod);` |
| 1453592 |  844 | `		while((pMEntry = SyHashGetNextEntry(&pInterface->hMethod)) != 0 ){` |
|  820631 |  845 | `			pMeth = (ph7_class_method *)pMEntry->pUserData;` |
|  820631 |  846 | `			pMName = &pMeth->sFunc.sName;` |
|  820631 |  847 | `			if( SyHashGet(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte) == 0 ){` |
|      25 |  848 | `				rc = SyHashInsert(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte,pMeth);` |
|      25 |  849 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  850 | `					return rc;` |
|       - |  851 | `				}` |
|      10 |  852 | `			}` |
|       5 |  853 | `		}` |
|       - |  854 | `	}` |
|  421979 |  855 | `	return SXRET_OK;` |
|  210992 |  856 | `}` |
|       - |  857 | `/*` |
|       - |  858 | ` * Create a class instance [i.e: Object in the PHP jargon] at run-time.` |
|       - |  859 | ` * The following function is called when an object is created at run-time` |
|       - |  860 | ` * typically when the PH7_OP_NEW/PH7_OP_CLONE instructions are executed.` |
|       - |  861 | ` * Notes on object creation.` |
|       - |  862 | ` *` |
|       - |  863 | ` * According to PHP language reference manual.` |
|       - |  864 | ` * To create an instance of a class, the new keyword must be used. An object will always` |
|       - |  865 | ` * be created unless the object has a constructor defined that throws an exception on error.` |
|       - |  866 | ` * Classes should be defined before instantiation (and in some cases this is a requirement).` |
|       - |  867 | ` * If a string containing the name of a class is used with new, a new instance of that class` |
|       - |  868 | ` * will be created. If the class is in a namespace, its fully qualified name must be used when` |
|       - |  869 | ` * doing this.` |
|       - |  870 | ` * Example #3 Creating an instance` |
|       - |  871 | ` * <?php` |
|       - |  872 | ` *  $instance = new SimpleClass();` |
|       - |  873 | ` *   // This can also be done with a variable:` |
|       - |  874 | ` * $className = 'Foo';` |
|       - |  875 | ` * $instance = new $className(); // Foo()` |
|       - |  876 | ` * ?>` |
|       - |  877 | ` * In the class context, it is possible to create a new object by new self and new parent.` |
|       - |  878 | ` * When assigning an already created instance of a class to a new variable, the new variable` |
|       - |  879 | ` * will access the same instance as the object that was assigned. This behaviour is the same` |
|       - |  880 | ` * when passing instances to a function. A copy of an already created object can be made by` |
|       - |  881 | ` * cloning it.` |
|       - |  882 | ` * Example #4 Object Assignment` |
|       - |  883 | ` * <?php` |
|       - |  884 | ` *  class SimpleClass(){` |
|       - |  885 | ` *    public $var;` |
|       - |  886 | ` *  };` |
|       - |  887 | ` *  $instance = new SimpleClass();` |
|       - |  888 | ` *  $assigned   =  $instance;` |
|       - |  889 | ` *  $reference  =& $instance;` |
|       - |  890 | ` *  $instance->var = '$assigned will have this value';` |
|       - |  891 | ` *  $instance = null; // $instance and $reference become null` |
|       - |  892 | ` *  var_dump($instance);` |
|       - |  893 | ` *  var_dump($reference);` |
|       - |  894 | ` *  var_dump($assigned);` |
|       - |  895 | ` * ?>` |
|       - |  896 | ` * The above example will output:` |
|       - |  897 | ` * NULL` |
|       - |  898 | ` * NULL` |
|       - |  899 | ` * object(SimpleClass)#1 (1) {` |
|       - |  900 | ` *  ["var"]=>` |
|       - |  901 | ` *    string(30) "$assigned will have this value"` |
|       - |  902 | ` * }` |
|       - |  903 | ` * Example #5 Creating new objects` |
|       - |  904 | ` * <?php` |
|       - |  905 | ` * class Test` |
|       - |  906 | ` * {` |
|       - |  907 | ` *   static public function getNew()` |
|       - |  908 | ` *   {` |
|       - |  909 | ` *       return new static;` |
|       - |  910 | ` *   }` |
|       - |  911 | ` * }` |
|       - |  912 | ` * class Child extends Test` |
|       - |  913 | ` * {}` |
|       - |  914 | ` * $obj1 = new Test();` |
|       - |  915 | ` * $obj2 = new $obj1;` |
|       - |  916 | ` * var_dump($obj1 !== $obj2);` |
|       - |  917 | ` * $obj3 = Test::getNew();` |
|       - |  918 | ` * var_dump($obj3 instanceof Test);` |
|       - |  919 | ` * $obj4 = Child::getNew();` |
|       - |  920 | ` * var_dump($obj4 instanceof Child);` |
|       - |  921 | ` * ?>` |
|       - |  922 | ` * The above example will output:` |
|       - |  923 | ` * bool(true)` |
|       - |  924 | ` * bool(true)` |
|       - |  925 | ` * bool(true)` |
|       - |  926 | ` * Note that Symisc Systems have introduced powerfull extension to` |
|       - |  927 | ` * OO subsystem. For example a class attribute may have any complex` |
|       - |  928 | ` * expression associated with it when declaring the attribute unlike` |
|       - |  929 | ` * the standard PHP engine which would allow a single value.` |
|       - |  930 | ` * Example:` |
|       - |  931 | ` *  class myClass{` |
|       - |  932 | ` *    public $var = 25<<1+foo()/bar();` |
|       - |  933 | ` *  };` |
|       - |  934 | ` * Refer to the official documentation for more information.` |
|       - |  935 | ` */` |
|    8866 |  936 | `static ph7_class_instance * NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|       5 |  937 | `{` |
|       - |  938 | `	ph7_class_instance *pThis;` |
|       - |  939 | `	/* Allocate a new instance */` |
|    8871 |  940 | `	pThis = (ph7_class_instance *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_instance));` |
|    8871 |  941 | `	if( pThis == 0 ){` |
|     ! 0 |  942 | `		return 0;` |
|       - |  943 | `	}` |
|       - |  944 | `	/* Zero the structure */` |
|    8871 |  945 | `	SyZero(pThis,sizeof(ph7_class_instance));` |
|       - |  946 | `	/* Initialize fields */` |
|    8871 |  947 | `	pThis->iRef = 1;` |
|    8871 |  948 | `	pThis->pVm = pVm;` |
|    8871 |  949 | `	pThis->pClass = pClass;` |
|       - |  950 | `	/* Assign a fresh monotonic object handle id (clones get their own, like PHP). */` |
|    8871 |  951 | `	pThis->nObjId = pVm->nNextObjId++;` |
|    8871 |  952 | `	SyHashInit(&pThis->hAttr,&pVm->sAllocator,0,0);` |
|    8871 |  953 | `	return pThis;` |
|    4438 |  954 | `}` |
|       - |  955 | `/*` |
|       - |  956 | ` * Wrapper around the NewClassInstance() function defined above.` |
|       - |  957 | ` * See the block comment above for more information.` |
|       - |  958 | ` */` |
|    8620 |  959 | `PH7_PRIVATE ph7_class_instance * PH7_NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|       5 |  960 | `{` |
|       - |  961 | `	ph7_class_instance *pNew;` |
|       - |  962 | `	sxi32 rc;` |
|    8625 |  963 | `	pNew = NewClassInstance(&(*pVm),&(*pClass));` |
|    8625 |  964 | `	if( pNew == 0 ){` |
|     ! 0 |  965 | `		return 0;` |
|       - |  966 | `	}` |
|       - |  967 | `	/* Associate a private VM frame with this class instance */` |
|    8625 |  968 | `	rc = PH7_VmCreateClassInstanceFrame(&(*pVm),pNew);` |
|    8625 |  969 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  970 | `		SyMemBackendPoolFree(&pVm->sAllocator,pNew);` |
|     ! 0 |  971 | `		return 0;` |
|       - |  972 | `	}` |
|       - |  973 | `	/* php stamps a Throwable's file/line at CREATION, not in its constructor, so a` |
|       - |  974 | `	 * subclass that overrides __construct without calling parent::__construct still` |
|       - |  975 | `	 * reports the right site. Every instantiation path lands here. */` |
|    8625 |  976 | `	PH7_VmStampThrowableSite(&(*pVm),pNew);` |
|    8625 |  977 | `	return pNew;` |
|    4315 |  978 | `}` |
|       - |  979 | `/*` |
|       - |  980 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon] attribute.` |
|       - |  981 | ` * This function never fail.` |
|       - |  982 | ` */` |
|   24746 |  983 | `static ph7_value * ExtractClassAttrValue(ph7_vm *pVm,VmClassAttr *pAttr)` |
|       5 |  984 | `{` |
|       - |  985 | `	/* Extract the value */` |
|       - |  986 | `	ph7_value *pValue;` |
|   24751 |  987 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|   24751 |  988 | `	return pValue;` |
|       5 |  989 | `}` |
|       - |  990 | `/*` |
|       - |  991 | ` * Perform a clone operation on a class instance [i.e: Object in the PHP jargon].` |
|       - |  992 | ` * The following function is called when an object is cloned at run-time` |
|       - |  993 | ` * typically when the PH7_OP_CLONE instruction is executed.` |
|       - |  994 | ` * Notes on object cloning.` |
|       - |  995 | ` *` |
|       - |  996 | ` * According to PHP language reference manual.` |
|       - |  997 | ` * Creating a copy of an object with fully replicated properties is not always the wanted behavior.` |
|       - |  998 | ` * A good example of the need for copy constructors. Another example is if your object holds a reference` |
|       - |  999 | ` * to another object which it uses and when you replicate the parent object you want to create` |
|       - | 1000 | ` * a new instance of this other object so that the replica has its own separate copy.` |
|       - | 1001 | ` * An object copy is created by using the clone keyword (which calls the object's __clone() method if possible).` |
|       - | 1002 | ` * An object's __clone() method cannot be called directly.` |
|       - | 1003 | ` * $copy_of_object = clone $object;` |
|       - | 1004 | ` * When an object is cloned, PHP 5 will perform a shallow copy of all of the object's properties.` |
|       - | 1005 | ` * Any properties that are references to other variables, will remain references.` |
|       - | 1006 | ` * Once the cloning is complete, if a __clone() method is defined, then the newly created object's __clone() method` |
|       - | 1007 | ` * will be called, to allow any necessary properties that need to be changed.` |
|       - | 1008 | ` * Example #1 Cloning an object` |
|       - | 1009 | ` * <?php` |
|       - | 1010 | ` * class SubObject` |
|       - | 1011 | ` * {` |
|       - | 1012 | ` *   static $instances = 0;` |
|       - | 1013 | ` *   public $instance;` |
|       - | 1014 | ` *` |
|       - | 1015 | ` *   public function __construct() {` |
|       - | 1016 | ` *       $this->instance = ++self::$instances;` |
|       - | 1017 | ` *   }` |
|       - | 1018 | ` *` |
|       - | 1019 | ` *   public function __clone() {` |
|       - | 1020 | ` *       $this->instance = ++self::$instances;` |
|       - | 1021 | ` *   }` |
|       - | 1022 | ` * }` |
|       - | 1023 | ` *` |
|       - | 1024 | ` * class MyCloneable` |
|       - | 1025 | ` * {` |
|       - | 1026 | ` *   public $object1;` |
|       - | 1027 | ` *   public $object2;` |
|       - | 1028 | ` *` |
|       - | 1029 | ` *   function __clone()` |
|       - | 1030 | ` *   {` |
|       - | 1031 | ` *       // Force a copy of this->object, otherwise` |
|       - | 1032 | ` *       // it will point to same object.` |
|       - | 1033 | ` *       $this->object1 = clone $this->object1;` |
|       - | 1034 | ` *   }` |
|       - | 1035 | ` * }` |
|       - | 1036 | ` * $obj = new MyCloneable();` |
|       - | 1037 | ` * $obj->object1 = new SubObject();` |
|       - | 1038 | ` * $obj->object2 = new SubObject();` |
|       - | 1039 | ` * $obj2 = clone $obj;` |
|       - | 1040 | ` * print("Original Object:\n");` |
|       - | 1041 | ` * print_r($obj);` |
|       - | 1042 | ` * print("Cloned Object:\n");` |
|       - | 1043 | ` * print_r($obj2);` |
|       - | 1044 | ` * ?>` |
|       - | 1045 | ` * The above example will output:` |
|       - | 1046 | ` * Original Object:` |
|       - | 1047 | ` * MyCloneable Object` |
|       - | 1048 | ` * (` |
|       - | 1049 | ` *   [object1] => SubObject Object` |
|       - | 1050 | ` *       (` |
|       - | 1051 | ` *           [instance] => 1` |
|       - | 1052 | ` *       )` |
|       - | 1053 | ` *` |
|       - | 1054 | ` *   [object2] => SubObject Object` |
|       - | 1055 | ` *       (` |
|       - | 1056 | ` *           [instance] => 2` |
|       - | 1057 | ` *       )` |
|       - | 1058 | ` *` |
|       - | 1059 | ` * )` |
|       - | 1060 | ` * Cloned Object:` |
|       - | 1061 | ` * MyCloneable Object` |
|       - | 1062 | ` * (` |
|       - | 1063 | ` *   [object1] => SubObject Object` |
|       - | 1064 | ` *       (` |
|       - | 1065 | ` *           [instance] => 3` |
|       - | 1066 | ` *       )` |
|       - | 1067 | ` *` |
|       - | 1068 | ` *   [object2] => SubObject Object` |
|       - | 1069 | ` *       (` |
|       - | 1070 | ` *           [instance] => 2` |
|       - | 1071 | ` *       )` |
|       - | 1072 | ` * )` |
|       - | 1073 | ` */` |
|     246 | 1074 | `PH7_PRIVATE ph7_class_instance * PH7_CloneClassInstance(ph7_class_instance *pSrc)` |
|       4 | 1075 | `{` |
|       - | 1076 | `	ph7_class_instance *pClone;` |
|       - | 1077 | `	ph7_class_method *pMethod;` |
|       - | 1078 | `	SyHashEntry *pEntry2;` |
|       - | 1079 | `	SyHashEntry *pEntry;` |
|       - | 1080 | `	ph7_vm *pVm;` |
|       - | 1081 | `	sxi32 rc;` |
|       - | 1082 | `	/* Allocate a new instance */` |
|     250 | 1083 | `	pVm = pSrc->pVm;` |
|     250 | 1084 | `	pClone = NewClassInstance(pVm,pSrc->pClass);` |
|     250 | 1085 | `	if( pClone == 0 ){` |
|     ! 0 | 1086 | `		return 0;` |
|       - | 1087 | `	}` |
|       - | 1088 | `	/* Associate a private VM frame with this class instance */` |
|     250 | 1089 | `	rc = PH7_VmCreateClassInstanceFrame(pVm,pClone);` |
|     250 | 1090 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1091 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClone);` |
|     ! 0 | 1092 | `		return 0;` |
|       - | 1093 | `	}` |
|       - | 1094 | `	/* Duplicate object values. Iterate the SOURCE attributes and copy each into` |
|       - | 1095 | `	 * the clone's same-named slot (looked up by name, so order/count differences` |
|       - | 1096 | `	 * from dynamic properties don't matter). A dynamic (runtime-added) property` |
|       - | 1097 | `	 * has no declared counterpart in the clone, so synthesize it first — without` |
|       - | 1098 | `	 * this, a clone of a stdClass would silently lose all its dynamic properties. */` |
|     250 | 1099 | `	SyHashResetLoopCursor(&pSrc->hAttr);` |
|    2642 | 1100 | `	while((pEntry = SyHashGetNextEntry(&pSrc->hAttr)) != 0 ){` |
|    2396 | 1101 | `		VmClassAttr *pSrcAttr = (VmClassAttr *)pEntry->pUserData;` |
|    2396 | 1102 | `		VmClassAttr *pDestAttr = 0;` |
|    2396 | 1103 | `		ph7_value *pvSrc,*pvDest = 0;` |
|       - | 1104 | `		/* Duplicate non-static attribute */` |
|    2396 | 1105 | `		if( pSrcAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|    1695 | 1106 | `			continue;` |
|       - | 1107 | `		}` |
|     702 | 1108 | `		pEntry2 = SyHashGet(&pClone->hAttr,SyStringData(&pSrcAttr->pAttr->sName),SyStringLength(&pSrcAttr->pAttr->sName));` |
|     702 | 1109 | `		if( pEntry2 ){` |
|     688 | 1110 | `			pDestAttr = (VmClassAttr *)pEntry2->pUserData;` |
|     688 | 1111 | `			pvDest = ExtractClassAttrValue(pVm,pDestAttr);` |
|     357 | 1112 | `		}else if( pSrcAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){` |
|       - | 1113 | `			/* Dynamic property: synthesize the matching slot on the clone. */` |
|      22 | 1114 | `			pvDest = PH7_VmCreateDynamicAttr(pVm,pClone,` |
|      14 | 1115 | `				SyStringData(&pSrcAttr->pAttr->sName),SyStringLength(&pSrcAttr->pAttr->sName),&pDestAttr);` |
|       7 | 1116 | `		}` |
|       - | 1117 | `		/* Fetch the source value LAST: PH7_VmCreateDynamicAttr above may have` |
|       - | 1118 | `		 * reserved a slot and reallocated pVm->aMemObj, which would dangle any` |
|       - | 1119 | `		 * ph7_value* obtained before it. pvDest from the synth path already points` |
|       - | 1120 | `		 * into the post-realloc aMemObj; resolve pvSrc now so both are current. */` |
|     702 | 1121 | `		pvSrc = ExtractClassAttrValue(pVm,pSrcAttr);` |
|     702 | 1122 | `		if( (pSrcAttr->iState & VM_CLASS_ATTR_REFBOUND) && pDestAttr ){` |
|       - | 1123 | `			/* php preserves references across clone: the clone shares the SAME slot` |
|       - | 1124 | `			 * as the source property (both alias the referenced variable), rather` |
|       - | 1125 | `			 * than getting an independent value copy. Drop the clone's fresh private` |
|       - | 1126 | `			 * slot and repoint at the (already-pinned) shared slot. The REFBOUND flag` |
|       - | 1127 | `			 * is carried over by the iState copy below, so the clone's release also` |
|       - | 1128 | `			 * leaves the shared slot alone. */` |
|       3 | 1129 | `			if( pDestAttr->nIdx != pSrcAttr->nIdx ){` |
|       3 | 1130 | `				if( pDestAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|     ! 0 | 1131 | `					SyHashDeleteEntry(&pVm->hTypedSlot,(const void *)&pDestAttr->nIdx,sizeof(sxu32),0);` |
|     ! 0 | 1132 | `				}` |
|       3 | 1133 | `				PH7_VmUnsetMemObj(pVm,pDestAttr->nIdx,TRUE);` |
|       3 | 1134 | `				pDestAttr->nIdx = pSrcAttr->nIdx;` |
|       2 | 1135 | `			}` |
|     701 | 1136 | `		}else if( pvSrc && pvDest ){` |
|     700 | 1137 | `			PH7_MemObjStore(pvSrc,pvDest);` |
|     348 | 1138 | `		}` |
|       - | 1139 | `		/* Carry over the per-instance state so the clone matches the source:` |
|       - | 1140 | `		 * VM_CLASS_ATTR_UNINIT marks a typed property as not-yet-initialized` |
|       - | 1141 | `		 * and doubles as the readonly write-once latch — without this a clone` |
|       - | 1142 | `		 * would reset to uninitialized (losing the value's readiness) and a` |
|       - | 1143 | `		 * readonly property would become writable again. */` |
|     702 | 1144 | `		if( pDestAttr ){` |
|     702 | 1145 | `			pDestAttr->iState = pSrcAttr->iState;` |
|     349 | 1146 | `		}` |
|       4 | 1147 | `	}` |
|       - | 1148 | `	/* A declared property unset() on the source is absent from the clone too (PHP). But the clone` |
|       - | 1149 | `	 * frame above materialized ALL declared attrs (with their defaults), so drop any clone attr whose` |
|       - | 1150 | `	 * name is not present on the source. Collect first, then delete — removing an entry mid-walk would` |
|       - | 1151 | `	 * free the node the SyHash loop cursor points at. */` |
|       - | 1152 | `	{` |
|       - | 1153 | `		SySet sDrop;` |
|     250 | 1154 | `		SySetInit(&sDrop,&pVm->sAllocator,sizeof(VmClassAttr *));` |
|     250 | 1155 | `		SyHashResetLoopCursor(&pClone->hAttr);` |
|    2644 | 1156 | `		while((pEntry = SyHashGetNextEntry(&pClone->hAttr)) != 0 ){` |
|    2398 | 1157 | `			VmClassAttr *pCloneAttr = (VmClassAttr *)pEntry->pUserData;` |
|    2398 | 1158 | `			if( pCloneAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|    1695 | 1159 | `				continue;` |
|       - | 1160 | `			}` |
|    1050 | 1161 | `			if( SyHashGet(&pSrc->hAttr,SyStringData(&pCloneAttr->pAttr->sName),` |
|    1054 | 1162 | `					SyStringLength(&pCloneAttr->pAttr->sName)) == 0 ){` |
|       3 | 1163 | `				SySetPut(&sDrop,(const void *)&pCloneAttr);` |
|       1 | 1164 | `			}` |
|       4 | 1165 | `		}` |
|     250 | 1166 | `		if( SySetUsed(&sDrop) > 0 ){` |
|       3 | 1167 | `			VmClassAttr **apDrop = (VmClassAttr **)SySetBasePtr(&sDrop);` |
|       - | 1168 | `			sxu32 i;` |
|       5 | 1169 | `			for( i = 0 ; i < SySetUsed(&sDrop) ; ++i ){` |
|       3 | 1170 | `				VmClassAttr *pVmAttr = apDrop[i];` |
|       4 | 1171 | `				SyHashDeleteEntry(&pClone->hAttr,SyStringData(&pVmAttr->pAttr->sName),` |
|       2 | 1172 | `					SyStringLength(&pVmAttr->pAttr->sName),0);` |
|       3 | 1173 | `				PH7_VmReleaseInstanceAttr(pVm,pVmAttr);` |
|       2 | 1174 | `			}` |
|       1 | 1175 | `		}` |
|     250 | 1176 | `		SySetRelease(&sDrop);` |
|       - | 1177 | `	}` |
|       - | 1178 | `	/* call the __clone method on the cloned object if available */` |
|     250 | 1179 | `	pMethod = PH7_ClassExtractMethod(pClone->pClass,"__clone",sizeof("__clone")-1);` |
|     250 | 1180 | `	if( pMethod ){` |
|      60 | 1181 | `		if( pMethod->iCloneDepth < 16 ){` |
|      58 | 1182 | `			pMethod->iCloneDepth++;` |
|       - | 1183 | `			/* PHP 8.3: __clone() may re-initialize the clone's readonly` |
|       - | 1184 | `			 * properties. Flag the instance so the readonly store guard allows` |
|       - | 1185 | `			 * it for the duration of the call. */` |
|      58 | 1186 | `			pClone->iFlags \|= VM_INSTANCE_CLONING;` |
|      58 | 1187 | `			PH7_VmCallClassMethod(pVm,pClone,pMethod,0,0,0);` |
|      58 | 1188 | `			pClone->iFlags &= ~VM_INSTANCE_CLONING;` |
|      30 | 1189 | `		}else{` |
|       - | 1190 | `			/* Nesting limit reached */` |
|       3 | 1191 | `			PH7_VmThrowError(pVm,0,PH7_CTX_ERR,"Object clone limit reached,no more call to __clone()");` |
|       - | 1192 | `		}` |
|       - | 1193 | `		/* Reset the cursor */` |
|      60 | 1194 | `		pMethod->iCloneDepth = 0;` |
|      29 | 1195 | `	}` |
|       - | 1196 | `	/* Return the cloned object */` |
|     250 | 1197 | `	return pClone;` |
|     127 | 1198 | `}` |
|       - | 1199 | `#define CLASS_INSTANCE_DESTROYED 0x001 /* Instance is released */` |
|       - | 1200 | `/*` |
|       - | 1201 | ` * Free the per-instance allocations owned by ONE object attribute: its value slot (+ the typed-slot` |
|       - | 1202 | ` * enforcement entry), the synthesized ph7_class_attr for a dynamic (runtime-added) property, and the` |
|       - | 1203 | ` * VmClassAttr wrapper itself. Does NOT touch the hAttr entry node — the caller removes it` |
|       - | 1204 | `` * (`unset($o->p)` via SyHashDeleteEntry2; instance teardown via the wholesale SyHashRelease, so it must`` |
|       - | 1205 | ` * not delete entries mid-walk). Shared by PH7_ClassInstanceRelease and the OP_MEMBER unset path.` |
|       - | 1206 | ` */` |
|   29476 | 1207 | `PH7_PRIVATE void PH7_VmReleaseInstanceAttr(ph7_vm *pVm, VmClassAttr *pVmAttr)` |
|       5 | 1208 | `{` |
|   29481 | 1209 | `	if( pVmAttr->iState & VM_CLASS_ATTR_REFBOUND ){` |
|       - | 1210 | ``		/* Reference-bound property (`$o->p =& $x`): its value slot is SHARED with`` |
|       - | 1211 | `		 * (and pinned by) the source variable — releasing/recycling it here would` |
|       - | 1212 | `		 * dangle the surviving alias. Leave the slot alone (script-lifetime pin,` |
|       - | 1213 | `		 * matching the use(&$x) capture tradeoff); just free the bookkeeping below. */` |
|   29476 | 1214 | `	}else if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|       - | 1215 | `		/* Drop any typed-property enforcement slot registered for this memobj, before the memobj` |
|       - | 1216 | `		 * is returned to the free list, so a future recycled slot does not inherit the stale entry. */` |
|   22173 | 1217 | `		if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|     308 | 1218 | `			SyHashDeleteEntry(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),0);` |
|     152 | 1219 | `		}` |
|   22173 | 1220 | `		PH7_VmUnsetMemObj(pVm,pVmAttr->nIdx,TRUE);` |
|   11084 | 1221 | `	}` |
|       - | 1222 | `	/* A dynamic property owns its synthesized ph7_class_attr (struct + inline name in one block) —` |
|       - | 1223 | `	 * free it here (the only place a per-instance pAttr is freed; declared attrs are class-owned). */` |
|   29481 | 1224 | `	if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){` |
|     116 | 1225 | `		SyMemBackendFree(&pVm->sAllocator,pVmAttr->pAttr);` |
|      57 | 1226 | `	}` |
|   29481 | 1227 | `	SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|   29481 | 1228 | `}` |
|       - | 1229 | `/*` |
|       - | 1230 | ` * Release a class instance [i.e: Object in the PHP jargon] and invoke any defined destructor.` |
|       - | 1231 | ` * This routine is invoked as soon as there are no other references to a particular` |
|       - | 1232 | ` * class instance.` |
|       - | 1233 | ` */` |
|    5900 | 1234 | `static void PH7_ClassInstanceRelease(ph7_class_instance *pThis)` |
|       5 | 1235 | `{` |
|       - | 1236 | `	ph7_class_method *pDestr;` |
|       - | 1237 | `	SyHashEntry *pEntry;` |
|       - | 1238 | `	ph7_class *pClass;` |
|       - | 1239 | `	ph7_vm *pVm;` |
|    5905 | 1240 | `	if( pThis->iFlags & CLASS_INSTANCE_DESTROYED ){` |
|       - | 1241 | `		/*` |
|       - | 1242 | `		 * Already destroyed,return immediately.` |
|       - | 1243 | `		 * This could happend if someone perform unset($this) in the destructor body.` |
|       - | 1244 | `		 */` |
|     ! 0 | 1245 | `		return;` |
|       - | 1246 | `	}` |
|       - | 1247 | `	/* Mark as destroyed */` |
|    5905 | 1248 | `	pThis->iFlags \|= CLASS_INSTANCE_DESTROYED;` |
|       - | 1249 | `	/* Invoke any defined destructor if available */` |
|    5905 | 1250 | `	pVm = pThis->pVm;` |
|    5905 | 1251 | `	pClass = pThis->pClass;` |
|    5905 | 1252 | `	pDestr = PH7_ClassExtractMethod(pClass,"__destruct",sizeof("__destruct")-1);` |
|    5905 | 1253 | `	if( pDestr && !pVm->bInReset ){` |
|       - | 1254 | `		/* Invoke the destructor. Skipped during ph7_vm_reset() bulk teardown:` |
|       - | 1255 | `		 * running user PHP against a half-reset VM is unsafe (see bInReset). */` |
|     473 | 1256 | `		pThis->iRef = 2; /* Prevent garbage collection */` |
|     473 | 1257 | `		PH7_VmCallClassMethod(pVm,pThis,pDestr,0,0,0);` |
|     234 | 1258 | `	}` |
|       - | 1259 | `	/* Weak-reference registry: kill the cell for this instance so every` |
|       - | 1260 | `	 * WeakReference/WeakMap handle observes the death (the cell outlives the` |
|       - | 1261 | `	 * instance until its own handles drop; removing the hash entry here keeps` |
|       - | 1262 | `	 * a pool-reused address from resurrecting a dead cell). */` |
|    5905 | 1263 | `	if( SyHashTotalEntry(&pVm->hWeakCell) > 0 ){` |
|      27 | 1264 | `		void *pCellData = 0;` |
|      26 | 1265 | `		if( SyHashDeleteEntry(&pVm->hWeakCell,(const void *)&pThis,sizeof(void *),&pCellData) == SXRET_OK` |
|      18 | 1266 | `		 && pCellData ){` |
|       9 | 1267 | `			((VmWeakCell *)pCellData)->pObj = 0;` |
|       4 | 1268 | `		}` |
|      13 | 1269 | `	}` |
|       - | 1270 | `	/* Release non-static attributes (the wholesale SyHashRelease below frees the entry nodes,` |
|       - | 1271 | `	 * so the helper must not delete them mid-walk). */` |
|    5905 | 1272 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|   35353 | 1273 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|   29453 | 1274 | `		PH7_VmReleaseInstanceAttr(pVm,(VmClassAttr *)pEntry->pUserData);` |
|       5 | 1275 | `	}` |
|       - | 1276 | `	/* Release the whole structure */` |
|    5905 | 1277 | `	SyHashRelease(&pThis->hAttr);` |
|    5905 | 1278 | `	SyMemBackendPoolFree(&pVm->sAllocator,pThis);` |
|    2955 | 1279 | `}` |
|       - | 1280 | `/*` |
|       - | 1281 | ` * Decrement the reference count of a class instance [i.e Object in the PHP jargon].` |
|       - | 1282 | ` * If the reference count reaches zero,release the whole instance.` |
|       - | 1283 | ` */` |
|  148390 | 1284 | `PH7_PRIVATE void PH7_ClassInstanceUnref(ph7_class_instance *pThis)` |
|       5 | 1285 | `{` |
|  148395 | 1286 | `	pThis->iRef--;` |
|  148395 | 1287 | `	if( pThis->iRef < 1 ){` |
|       - | 1288 | `		/* No more reference to this instance */` |
|    5905 | 1289 | `		PH7_ClassInstanceRelease(&(*pThis));` |
|    2950 | 1290 | `	}` |
|  148395 | 1291 | `}` |
|       - | 1292 | `/*` |
|       - | 1293 | ` * Compare two class instances [i.e: Objects in the PHP jargon]` |
|       - | 1294 | ` * Note on objects comparison:` |
|       - | 1295 | ` *  According to the PHP langauge reference manual` |
|       - | 1296 | ` *  When using the comparison operator (==), object variables are compared in a simple manner` |
|       - | 1297 | ` *  namely: Two object instances are equal if they have the same attributes and values, and are` |
|       - | 1298 | ` *  instances of the same class.` |
|       - | 1299 | ` *  On the other hand, when using the identity operator (===), object variables are identical` |
|       - | 1300 | ` *  if and only if they refer to the same instance of the same class.` |
|       - | 1301 | ` *  An example will clarify these rules.` |
|       - | 1302 | ` *  Example #1 Example of object comparison` |
|       - | 1303 | ` *  <?php` |
|       - | 1304 | ` *    function bool2str($bool)` |
|       - | 1305 | ` * {` |
|       - | 1306 | ` *   if ($bool === false) {` |
|       - | 1307 | ` *       return 'FALSE';` |
|       - | 1308 | ` *   } else {` |
|       - | 1309 | ` *       return 'TRUE';` |
|       - | 1310 | ` *   }` |
|       - | 1311 | ` * }` |
|       - | 1312 | ` * function compareObjects(&$o1, &$o2)` |
|       - | 1313 | ` * {` |
|       - | 1314 | ` *   echo 'o1 == o2 : ' . bool2str($o1 == $o2) . "\n";` |
|       - | 1315 | ` *   echo 'o1 != o2 : ' . bool2str($o1 != $o2) . "\n";` |
|       - | 1316 | ` *   echo 'o1 === o2 : ' . bool2str($o1 === $o2) . "\n";` |
|       - | 1317 | ` *   echo 'o1 !== o2 : ' . bool2str($o1 !== $o2) . "\n";` |
|       - | 1318 | ` * }` |
|       - | 1319 | ` * class Flag` |
|       - | 1320 | ` * {` |
|       - | 1321 | ` *   public $flag;` |
|       - | 1322 | ` *` |
|       - | 1323 | ` *   function Flag($flag = true) {` |
|       - | 1324 | ` *       $this->flag = $flag;` |
|       - | 1325 | ` *   }` |
|       - | 1326 | ` * }` |
|       - | 1327 | ` *` |
|       - | 1328 | ` * class OtherFlag` |
|       - | 1329 | ` * {` |
|       - | 1330 | ` *   public $flag;` |
|       - | 1331 | ` *` |
|       - | 1332 | ` *   function OtherFlag($flag = true) {` |
|       - | 1333 | ` *       $this->flag = $flag;` |
|       - | 1334 | ` *   }` |
|       - | 1335 | ` * }` |
|       - | 1336 | ` *` |
|       - | 1337 | ` * $o = new Flag();` |
|       - | 1338 | ` * $p = new Flag();` |
|       - | 1339 | ` * $q = $o;` |
|       - | 1340 | ` * $r = new OtherFlag();` |
|       - | 1341 | ` *` |
|       - | 1342 | ` * echo "Two instances of the same class\n";` |
|       - | 1343 | ` * compareObjects($o, $p);` |
|       - | 1344 | ` * echo "\nTwo references to the same instance\n";` |
|       - | 1345 | ` * compareObjects($o, $q);` |
|       - | 1346 | ` * echo "\nInstances of two different classes\n";` |
|       - | 1347 | ` * compareObjects($o, $r);` |
|       - | 1348 | ` * ?>` |
|       - | 1349 | ` * The above example will output:` |
|       - | 1350 | ` * Two instances of the same class` |
|       - | 1351 | ` * o1 == o2 : TRUE` |
|       - | 1352 | ` * o1 != o2 : FALSE` |
|       - | 1353 | ` * o1 === o2 : FALSE` |
|       - | 1354 | ` * o1 !== o2 : TRUE` |
|       - | 1355 | ` * Two references to the same instance` |
|       - | 1356 | ` * o1 == o2 : TRUE` |
|       - | 1357 | ` * o1 != o2 : FALSE` |
|       - | 1358 | ` * o1 === o2 : TRUE` |
|       - | 1359 | ` * o1 !== o2 : FALSE` |
|       - | 1360 | ` * Instances of two different classes` |
|       - | 1361 | ` * o1 == o2 : FALSE` |
|       - | 1362 | ` * o1 != o2 : TRUE` |
|       - | 1363 | ` * o1 === o2 : FALSE` |
|       - | 1364 | ` * o1 !== o2 : TRUE` |
|       - | 1365 | ` *` |
|       - | 1366 | ` * This function return 0 if the objects are equals according to the comprison rules defined above.` |
|       - | 1367 | ` * Any other return values indicates difference.` |
|       - | 1368 | ` */` |
|     290 | 1369 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest)` |
|       5 | 1370 | `{` |
|       - | 1371 | `	SyHashEntry *pEntry,*pEntry2;` |
|       - | 1372 | `	ph7_value sV1,sV2;` |
|       - | 1373 | `	sxi32 rc;` |
|     295 | 1374 | `	if( iNest > 31 ){` |
|       - | 1375 | `		/* Nesting limit reached */` |
|       6 | 1376 | `		PH7_VmThrowError(pLeft->pVm,0,PH7_CTX_ERR,"Nesting limit reached: Infinite recursion?");` |
|       6 | 1377 | `		return 1;` |
|       - | 1378 | `	}` |
|       - | 1379 | `	/* Comparison is performed only if the objects are instance of the same class */` |
|     291 | 1380 | `	if( pLeft->pClass != pRight->pClass ){` |
|       7 | 1381 | `		return 1;` |
|       - | 1382 | `	}` |
|     285 | 1383 | `	if( bStrict ){` |
|       - | 1384 | `		/*` |
|       - | 1385 | `		 * According to the PHP language reference manual:` |
|       - | 1386 | `		 *  when using the identity operator (===), object variables` |
|       - | 1387 | `		 *  are identical if and only if they refer to the same instance` |
|       - | 1388 | `		 *  of the same class.` |
|       - | 1389 | `		 */` |
|     121 | 1390 | `		return !(pLeft == pRight);` |
|       - | 1391 | `	}` |
|       - | 1392 | `	/*` |
|       - | 1393 | `	 * Attribute comparison.` |
|       - | 1394 | `	 * According to the PHP reference manual:` |
|       - | 1395 | `	 *  When using the comparison operator (==), object variables are compared` |
|       - | 1396 | `	 *  in a simple manner, namely: Two object instances are equal if they have` |
|       - | 1397 | `	 *  the same attributes and values, and are instances of the same class.` |
|       - | 1398 | `	 */` |
|     168 | 1399 | `	if( pLeft == pRight ){` |
|       - | 1400 | `		/* Same instance,don't bother processing,object are equals */` |
|       5 | 1401 | `		return 0;` |
|       - | 1402 | `	}` |
|       - | 1403 | `	/* Closures compare by IDENTITY under == as well (not by attributes): two distinct` |
|       - | 1404 | `	 * Closure instances are never equal, even when they wrap the same underlying function` |
|       - | 1405 | `	 * (PHP semantics). pLeft != pRight here, so a Closure pair is unequal. Without this,` |
|       - | 1406 | `` 	 * two capture-less lambdas of the same `function(){}` share the template's `$__fn` `` |
|       - | 1407 | `	 * name and would compare equal. */` |
|     164 | 1408 | `	if( pLeft->pVm->pClosureClass && pLeft->pClass == pLeft->pVm->pClosureClass ){` |
|       5 | 1409 | `		return 1;` |
|       - | 1410 | `	}` |
|       - | 1411 | `	/* Same class but a different number of attributes ⇒ different property sets` |
|       - | 1412 | `	 * (dynamic properties can give two same-class instances different counts). */` |
|     160 | 1413 | `	if( pLeft->hAttr.nEntry != pRight->hAttr.nEntry ){` |
|       3 | 1414 | `		return 1;` |
|       - | 1415 | `	}` |
|     158 | 1416 | `	PH7_MemObjInit(pLeft->pVm,&sV1);` |
|     158 | 1417 | `	PH7_MemObjInit(pLeft->pVm,&sV2);` |
|     158 | 1418 | `	sV1.nIdx = sV2.nIdx = SXU32_HIGH;` |
|       - | 1419 | `	/* Compare each left attribute against the RIGHT attribute of the SAME NAME` |
|       - | 1420 | `	 * (not in lockstep): dynamic properties may be stored in a different order` |
|       - | 1421 | `	 * on the two instances. Counts already match, so if every left attribute has` |
|       - | 1422 | `	 * an equal-valued same-named right attribute the property sets are equal. */` |
|     158 | 1423 | `	SyHashResetLoopCursor(&pLeft->hAttr);` |
|     194 | 1424 | `	while((pEntry = SyHashGetNextEntry(&pLeft->hAttr)) != 0 ){` |
|     170 | 1425 | `		VmClassAttr *p1 = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1426 | `		VmClassAttr *p2;` |
|       - | 1427 | `		ph7_value *pL,*pR;` |
|       - | 1428 | `		/* Compare only non-static attribute */` |
|     170 | 1429 | `		if( p1->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|     ! 0 | 1430 | `			continue;` |
|       - | 1431 | `		}` |
|     170 | 1432 | `		pEntry2 = SyHashGet(&pRight->hAttr,SyStringData(&p1->pAttr->sName),SyStringLength(&p1->pAttr->sName));` |
|     170 | 1433 | `		if( pEntry2 == 0 ){` |
|       - | 1434 | `			/* Left has a property the right lacks ⇒ not equal. */` |
|     ! 0 | 1435 | `			return 1;` |
|       - | 1436 | `		}` |
|     170 | 1437 | `		p2 = (VmClassAttr *)pEntry2->pUserData;` |
|     170 | 1438 | `		pL = ExtractClassAttrValue(pLeft->pVm,p1);` |
|     170 | 1439 | `		pR = ExtractClassAttrValue(pRight->pVm,p2);` |
|     170 | 1440 | `		if( pL && pR ){` |
|     170 | 1441 | `			PH7_MemObjLoad(pL,&sV1);` |
|     170 | 1442 | `			PH7_MemObjLoad(pR,&sV2);` |
|       - | 1443 | `			/* Compare the two values now */` |
|     170 | 1444 | `			rc = PH7_MemObjCmp(&sV1,&sV2,bStrict,iNest+1);` |
|     170 | 1445 | `			PH7_MemObjRelease(&sV1);` |
|     170 | 1446 | `			PH7_MemObjRelease(&sV2);` |
|     170 | 1447 | `			if( rc != 0 ){` |
|       - | 1448 | `				/* Not equals */` |
|     133 | 1449 | `				return rc;` |
|       - | 1450 | `			}` |
|      18 | 1451 | `		}` |
|       2 | 1452 | `	}` |
|       - | 1453 | `	/* Object are equals */` |
|      26 | 1454 | `	return 0;` |
|     150 | 1455 | `}` |
|       - | 1456 | `/*` |
|       - | 1457 | ` * Dump a class instance and the store the dump in the BLOB given` |
|       - | 1458 | ` * as the first argument.` |
|       - | 1459 | ` * Note that only non-static/non-constants attribute are dumped.` |
|       - | 1460 | ` * This function is typically invoked when the user issue a call` |
|       - | 1461 | ` * to [var_dump(),var_export(),print_r(),...].` |
|       - | 1462 | ` * This function SXRET_OK on success. Any other return value including` |
|       - | 1463 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|       - | 1464 | ` */` |
|       - | 1465 | `/*` |
|       - | 1466 | `` * Return the `name` property value of an enum case instance (the case name),`` |
|       - | 1467 | ` * or 0 when unavailable. Shared by the var_dump/var_export/json/serialize` |
|       - | 1468 | ` * renderers, which all print enum cases as Class::CaseName forms.` |
|       - | 1469 | ` */` |
|       6 | 1470 | `PH7_PRIVATE ph7_value * PH7_EnumCaseNameValue(ph7_class_instance *pThis)` |
|       1 | 1471 | `{` |
|       - | 1472 | `	SyHashEntry *pEntry;` |
|       7 | 1473 | `	if( (pThis->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|     ! 0 | 1474 | `		return 0;` |
|       - | 1475 | `	}` |
|       7 | 1476 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)"name",sizeof("name")-1);` |
|       7 | 1477 | `	if( pEntry == 0 ){` |
|     ! 0 | 1478 | `		return 0;` |
|       - | 1479 | `	}` |
|       7 | 1480 | `	return PH7_ClassInstanceExtractAttrValue(pThis,(VmClassAttr *)pEntry->pUserData);` |
|       4 | 1481 | `}` |
|       - | 1482 | `/*` |
|       - | 1483 | `` * Return the `value` property value (the backing value) of an enum case`` |
|       - | 1484 | ` * instance, or 0 when unavailable (pure enums have none).` |
|       - | 1485 | ` */` |
|       8 | 1486 | `PH7_PRIVATE ph7_value * PH7_EnumCaseBackingValueOf(ph7_class_instance *pThis)` |
|       1 | 1487 | `{` |
|       - | 1488 | `	SyHashEntry *pEntry;` |
|       9 | 1489 | `	if( (pThis->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|     ! 0 | 1490 | `		return 0;` |
|       - | 1491 | `	}` |
|       9 | 1492 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)"value",sizeof("value")-1);` |
|       9 | 1493 | `	if( pEntry == 0 ){` |
|       3 | 1494 | `		return 0;` |
|       - | 1495 | `	}` |
|       7 | 1496 | `	return PH7_ClassInstanceExtractAttrValue(pThis,(VmClassAttr *)pEntry->pUserData);` |
|       5 | 1497 | `}` |
|       - | 1498 | `/*` |
|       - | 1499 | ` * Emit a class-instance dump header plus its trailing newline. For var_dump` |
|       - | 1500 | ` * (ShowType) it completes the "object(" prefix the caller already emitted as` |
|       - | 1501 | ` *   ClassName)#<id> (<count>) {` |
|       - | 1502 | ` * for print_r it emits the legacy PHL  Object(ClassName) {  (count/id unused).` |
|       - | 1503 | `` * Enum cases print php's `ClassName Enum {` print_r header (var_dump never`` |
|       - | 1504 | `` * reaches here for enums — PH7_MemObjDump prints `enum(S::A)` directly).`` |
|       - | 1505 | ` */` |
|     136 | 1506 | `static void DumpClassInstanceHeader(SyBlob *pOut,ph7_class *pClass,sxu32 nObjId,int ShowType,sxu32 nCount)` |
|       3 | 1507 | `{` |
|     139 | 1508 | `	if( ShowType ){` |
|       - | 1509 | ``		/* var_dump: `object(C)#id (n) {` */`` |
|     135 | 1510 | `		SyBlobFormat(&(*pOut),"object(%z)#%u (%u) {",&pClass->sName,nObjId,nCount);` |
|     135 | 1511 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|     135 | 1512 | `		return;` |
|       - | 1513 | `	}` |
|       - | 1514 | ``	/* print_r: `C Object` / `E Enum[:backing]` — the '(' line is emitted by`` |
|       - | 1515 | `	 * the body renderer at the container indent. */` |
|       6 | 1516 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|     ! 0 | 1517 | `		SyBlobFormat(&(*pOut),"%z Enum",&pClass->sName);` |
|     ! 0 | 1518 | `		if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|     ! 0 | 1519 | `			SyBlobAppend(&(*pOut),":int",sizeof(":int")-1);` |
|     ! 0 | 1520 | `		}else if( pClass->nEnumBacking == MEMOBJ_STRING ){` |
|     ! 0 | 1521 | `			SyBlobAppend(&(*pOut),":string",sizeof(":string")-1);` |
|     ! 0 | 1522 | `		}` |
|     ! 0 | 1523 | `	}else{` |
|       6 | 1524 | `		SyBlobFormat(&(*pOut),"%z Object",&pClass->sName);` |
|       - | 1525 | `	}` |
|       6 | 1526 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      71 | 1527 | `}` |
|       - | 1528 | `/*` |
|       - | 1529 | ` * The class that DECLARED pAttr: inheritance shares attr pointers down the` |
|       - | 1530 | ` * chain, so the declaring class is the most ANCESTRAL class whose hAttr still` |
|       - | 1531 | ` * maps the name to this exact pointer. php's var_dump/print_r use it for the` |
|       - | 1532 | `` * `["p":"Decl":private]` annotation.`` |
|       - | 1533 | ` */` |
|       6 | 1534 | `static ph7_class * OoAttrDeclaringClass(ph7_class *pClass,ph7_class_attr *pAttr)` |
|       1 | 1535 | `{` |
|       - | 1536 | `	/* Attrs record their declaring class at install time (inheritance/trait` |
|       - | 1537 | `	 * copies share the pointer, so the field survives the chain). */` |
|       7 | 1538 | `	return pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|       1 | 1539 | `}` |
|       - | 1540 | `/*` |
|       - | 1541 | `` * Emit a property's dump key: var_dump `["x"]=>` / `["p":"C":private]=>` /`` |
|       - | 1542 | `` * `["q":protected]=>`; print_r `[x] => ` / `[p:C:private] => ` /`` |
|       - | 1543 | `` * `[q:protected] => ` (php's exact annotations).`` |
|       - | 1544 | ` */` |
|     138 | 1545 | `static void OoDumpPropKey(SyBlob *pOut,ph7_class_instance *pThis,ph7_class_attr *pAttr,int ShowType)` |
|       2 | 1546 | `{` |
|     140 | 1547 | `	const char *zQ = ShowType ? "\"" : "";` |
|     140 | 1548 | `	SyBlobFormat(&(*pOut),"[%s%z%s",zQ,&pAttr->sName,zQ);` |
|     140 | 1549 | `	if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       7 | 1550 | `		ph7_class *pDecl = OoAttrDeclaringClass(pThis->pClass,pAttr);` |
|       7 | 1551 | `		SyBlobFormat(&(*pOut),":%s%z%s:private",zQ,&pDecl->sName,zQ);` |
|     137 | 1552 | `	}else if( pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
|     ! 0 | 1553 | `		SyBlobAppend(&(*pOut),":protected",sizeof(":protected")-1);` |
|     ! 0 | 1554 | `	}` |
|     140 | 1555 | `	SyBlobAppend(&(*pOut),ShowType ? "]=>" : "] => ",ShowType ? 3 : 5);` |
|     140 | 1556 | `}` |
|     140 | 1557 | `PH7_PRIVATE sxi32 PH7_ClassInstanceDump(SyBlob *pOut,ph7_class_instance *pThis,int ShowType,int nTab,int nDepth)` |
|       3 | 1558 | `{` |
|       - | 1559 | `	SyHashEntry *pEntry;` |
|       - | 1560 | `	ph7_value *pValue;` |
|       - | 1561 | `	sxi32 rc;` |
|       - | 1562 | `	int i;` |
|     143 | 1563 | `	if( nDepth > 31 ){` |
|       - | 1564 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|       - | 1565 | `		/* Nesting limit reached..halt immediately*/` |
|       5 | 1566 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|       5 | 1567 | `		return SXERR_LIMIT;` |
|       - | 1568 | `	}` |
|     139 | 1569 | `	rc = SXRET_OK;` |
|       - | 1570 | `	{` |
|       - | 1571 | `		/* Both var_dump and print_r consult __debugInfo() (PHP behavior);` |
|       - | 1572 | `		 * var_export uses a separate renderer and never reaches here. When the` |
|       - | 1573 | `		 * method is present and returns an array, render that array's entries as` |
|       - | 1574 | `		 * the object body, with the header showing the debug array's count. The` |
|       - | 1575 | `		 * nDepth guard above protects against a __debugInfo returning the object` |
|       - | 1576 | `		 * itself. */` |
|     139 | 1577 | `		ph7_class_method *pDbg = PH7_ClassExtractMethod(pThis->pClass,"__debugInfo",sizeof("__debugInfo")-1);` |
|     139 | 1578 | `		if( pDbg ){` |
|       - | 1579 | `			ph7_value sResult;` |
|       5 | 1580 | `			PH7_MemObjInit(pThis->pVm,&sResult);` |
|       5 | 1581 | `			PH7_VmCallClassMethod(pThis->pVm,pThis,pDbg,&sResult,0,0);` |
|       5 | 1582 | `			if( sResult.iFlags & MEMOBJ_HASHMAP ){` |
|       5 | 1583 | `				ph7_hashmap *pMap = (ph7_hashmap *)sResult.x.pOther;` |
|       - | 1584 | `				/* Header count is the debug array's entry count. */` |
|       5 | 1585 | `				DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,pMap->nEntry);` |
|       5 | 1586 | `				if( !ShowType ){` |
|       3 | 1587 | `					for( i = 0 ; i < nTab ; i++ ){` |
|     ! 0 | 1588 | `						SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     ! 0 | 1589 | `					}` |
|       3 | 1590 | `					SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       1 | 1591 | `				}` |
|       5 | 1592 | `				rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth);` |
|       5 | 1593 | `				for( i = 0 ; i < nTab ; i++ ){` |
|     ! 0 | 1594 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     ! 0 | 1595 | `				}` |
|       5 | 1596 | `				if( ShowType ){` |
|       3 | 1597 | `					SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|       2 | 1598 | `				}else{` |
|       3 | 1599 | `					SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       - | 1600 | `				}` |
|       5 | 1601 | `				PH7_MemObjRelease(&sResult);` |
|       5 | 1602 | `				return rc;` |
|       - | 1603 | `			}` |
|       - | 1604 | `			/* Non-array return: behave as if __debugInfo were absent. */` |
|     ! 0 | 1605 | `			PH7_MemObjRelease(&sResult);` |
|     ! 0 | 1606 | `		}` |
|       - | 1607 | `	}` |
|       - | 1608 | `	{` |
|       - | 1609 | `		/* var_dump's header needs the property count up front, so pre-count the` |
|       - | 1610 | `		 * non-static/non-constant attributes (matching the dump loop below). */` |
|     134 | 1611 | `		sxu32 nProp = 0;` |
|     134 | 1612 | `		if( ShowType ){` |
|     132 | 1613 | `			SyHashResetLoopCursor(&pThis->hAttr);` |
|     270 | 1614 | `			while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|     140 | 1615 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     140 | 1616 | `				if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_HOOK_VIRTUAL)) == 0 ){` |
|     136 | 1617 | `					nProp++;` |
|      67 | 1618 | `				}` |
|       2 | 1619 | `			}` |
|      65 | 1620 | `		}` |
|     134 | 1621 | `		DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,nProp);` |
|       - | 1622 | `	}` |
|     134 | 1623 | `	if( !ShowType ){` |
|       - | 1624 | `		/* print_r body opener: '(' at the container indent */` |
|       3 | 1625 | `		for( i = 0 ; i < nTab ; i++ ){` |
|     ! 0 | 1626 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     ! 0 | 1627 | `		}` |
|       3 | 1628 | `		SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       1 | 1629 | `	}` |
|       - | 1630 | `	/* Dump object attributes (php 8.4: VIRTUAL hooked properties have no` |
|       - | 1631 | `	 * backing store — excluded from var_dump/print_r) */` |
|     134 | 1632 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|     218 | 1633 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|     144 | 1634 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     144 | 1635 | `		if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_HOOK_VIRTUAL)) == 0 ){` |
|       - | 1636 | `			/* Dump non-static/constant attribute only */` |
|     140 | 1637 | `			pValue = ExtractClassAttrValue(pThis->pVm,pVmAttr);` |
|     140 | 1638 | `			if( pValue == 0 ){` |
|     ! 0 | 1639 | `				continue;` |
|       - | 1640 | `			}` |
|     140 | 1641 | `			if( ShowType ){` |
|       - | 1642 | ``				/* var_dump prop: `["x"(:…)]=>` at nTab+2, the value on the next`` |
|       - | 1643 | `				 * line at the same indent (php). */` |
|    4124 | 1644 | `				for( i = 0 ; i < nTab + 2 ; i++ ){` |
|    3990 | 1645 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|    1996 | 1646 | `				}` |
|     136 | 1647 | `				OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,TRUE);` |
|     136 | 1648 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|     136 | 1649 | `				rc = PH7_MemObjDump(&(*pOut),pValue,TRUE,nTab+2,nDepth,0);` |
|     136 | 1650 | `				if( rc == SXERR_LIMIT ){` |
|     125 | 1651 | `					break;` |
|       - | 1652 | `				}` |
|       7 | 1653 | `			}else{` |
|       - | 1654 | ``				/* print_r prop: `[x(:…)] => value` at nTab+4; container values`` |
|       - | 1655 | `				 * render their block at nTab+8 followed by php's blank line. */` |
|      21 | 1656 | `				for( i = 0 ; i < nTab + 4 ; i++ ){` |
|      17 | 1657 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       9 | 1658 | `				}` |
|       5 | 1659 | `				OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,FALSE);` |
|       4 | 1660 | `				if( (pValue->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ))` |
|       3 | 1661 | `				 && (pValue->iFlags & MEMOBJ_NULL) == 0 ){` |
|     ! 0 | 1662 | `					rc = PH7_MemObjDump(&(*pOut),pValue,FALSE,nTab+8,nDepth,0);` |
|     ! 0 | 1663 | `					SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|     ! 0 | 1664 | `					if( rc == SXERR_LIMIT ){` |
|     ! 0 | 1665 | `						break;` |
|       - | 1666 | `					}` |
|     ! 0 | 1667 | `				}else{` |
|       5 | 1668 | `					PH7_MemObjPrintRInline(&(*pOut),pValue);` |
|       5 | 1669 | `					SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 1670 | `				}` |
|       - | 1671 | `			}` |
|       7 | 1672 | `		}` |
|       2 | 1673 | `	}` |
|    3854 | 1674 | `	for( i = 0 ; i < nTab ; i++ ){` |
|    3721 | 1675 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|    1861 | 1676 | `	}` |
|     134 | 1677 | `	if( ShowType ){` |
|     132 | 1678 | `		SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      67 | 1679 | `	}else{` |
|       3 | 1680 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       - | 1681 | `	}` |
|     134 | 1682 | `	return rc;` |
|      73 | 1683 | `}` |
|       - | 1684 | `/*` |
|       - | 1685 | ` * Call a magic method [i.e: __toString(),__toBool(),__Invoke()...]` |
|       - | 1686 | ` * Return SXRET_OK on successfull call. Any other return value indicates failure.` |
|       - | 1687 | ` * Notes on magic methods.` |
|       - | 1688 | ` * According to the PHP language reference manual.` |
|       - | 1689 | ` *  The function names __construct(), __destruct(), __call(), __callStatic()` |
|       - | 1690 | ` *  __get(),  __toString(), __invoke(), __clone() are magical in PHP classes.` |
|       - | 1691 | ` * You cannot have functions with these names in any of your classes unless` |
|       - | 1692 | ` * you want the magic functionality associated with them.` |
|       - | 1693 | ` * Example of magical methods:` |
|       - | 1694 | ` * __toString()` |
|       - | 1695 | ` *  The __toString() method allows a class to decide how it will react when it is treated like` |
|       - | 1696 | ` *  a string. For example, what echo $obj; will print. This method must return a string.` |
|       - | 1697 | ` *  Example #2 Simple example` |
|       - | 1698 | ` * <?php` |
|       - | 1699 | ` * // Declare a simple class` |
|       - | 1700 | ` * class TestClass` |
|       - | 1701 | ` * {` |
|       - | 1702 | ` *   public $foo;` |
|       - | 1703 | ` *` |
|       - | 1704 | ` *   public function __construct($foo)` |
|       - | 1705 | ` *   {` |
|       - | 1706 | ` *       $this->foo = $foo;` |
|       - | 1707 | ` *   }` |
|       - | 1708 | ` *` |
|       - | 1709 | ` *   public function __toString()` |
|       - | 1710 | ` *   {` |
|       - | 1711 | ` *       return $this->foo;` |
|       - | 1712 | ` *   }` |
|       - | 1713 | ` * }` |
|       - | 1714 | ` * $class = new TestClass('Hello');` |
|       - | 1715 | ` * echo $class;` |
|       - | 1716 | ` * ?>` |
|       - | 1717 | ` * The above example will output:` |
|       - | 1718 | ` *  Hello` |
|       - | 1719 | ` *` |
|       - | 1720 | ` * Note that PH7 does not support all the magical method and introudces __toFloat(),__toInt()` |
|       - | 1721 | ` * which have the same behaviour as __toString() but for float and integer types` |
|       - | 1722 | ` * respectively.` |
|       - | 1723 | ` * Refer to the official documentation for more information.` |
|       - | 1724 | ` */` |
|     286 | 1725 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallMagicMethod(` |
|       - | 1726 | `	ph7_vm *pVm,               /* VM that own all this stuff */` |
|       - | 1727 | `	ph7_class *pClass,         /* Target class */` |
|       - | 1728 | `	ph7_class_instance *pThis, /* Target object */` |
|       - | 1729 | `	const char *zMethod,       /* Magic method name [i.e: __toString()]*/` |
|       - | 1730 | `	sxu32 nByte,               /* zMethod length*/` |
|       - | 1731 | `	const SyString *pAttrName, /* Attribute name */` |
|       - | 1732 | `	ph7_value *pResult         /* OUT: magic method return value. NULL to discard */` |
|       - | 1733 | `	)` |
|       1 | 1734 | `{` |
|     287 | 1735 | `	ph7_value *apArg[2] = { 0 , 0 };` |
|       - | 1736 | `	ph7_class_method *pMeth;` |
|       - | 1737 | `	ph7_value sAttr; /* cc warning */` |
|       - | 1738 | `	sxi32 rc;` |
|       - | 1739 | `	int nArg;` |
|       - | 1740 | `	/* Make sure the magic method is available */` |
|     287 | 1741 | `	pMeth = PH7_ClassExtractMethod(&(*pClass),zMethod,nByte);` |
|     287 | 1742 | `	if( pMeth == 0 ){` |
|       - | 1743 | `		/* No such method,return immediately */` |
|     ! 0 | 1744 | `		return SXERR_NOTFOUND;` |
|       - | 1745 | `	}` |
|     287 | 1746 | `	nArg = 0;` |
|       - | 1747 | `	/* Copy arguments */` |
|     287 | 1748 | `	if( pAttrName ){` |
|     287 | 1749 | `		PH7_MemObjInitFromString(pVm,&sAttr,pAttrName);` |
|     287 | 1750 | `		sAttr.nIdx = SXU32_HIGH; /* Mark as constant */` |
|     287 | 1751 | `		apArg[0] = &sAttr;` |
|     287 | 1752 | `		nArg = 1;` |
|     143 | 1753 | `	}` |
|       - | 1754 | `	/* Call the magic method now */` |
|     287 | 1755 | `	rc = PH7_VmCallClassMethod(pVm,&(*pThis),pMeth,pResult,nArg,apArg);` |
|       - | 1756 | `	/* Clean up */` |
|     287 | 1757 | `	if( pAttrName ){` |
|     287 | 1758 | `		PH7_MemObjRelease(&sAttr);` |
|     143 | 1759 | `	}` |
|     287 | 1760 | `	return rc;` |
|     144 | 1761 | `}` |
|       - | 1762 | `/*` |
|       - | 1763 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon].` |
|       - | 1764 | ` * This function is simply a wrapper on ExtractClassAttrValue().` |
|       - | 1765 | ` */` |
|   11710 | 1766 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceExtractAttrValue(ph7_class_instance *pThis,VmClassAttr *pAttr)` |
|       5 | 1767 | `{` |
|       - | 1768 | `   /* Extract the attribute value */` |
|       - | 1769 | `	ph7_value *pValue;` |
|   11715 | 1770 | `	pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|   11715 | 1771 | `	return pValue;` |
|       5 | 1772 | `}` |
|       - | 1773 | `/*` |
|       - | 1774 | ` * Convert a class instance [i.e: Object in the PHP jargon] into a hashmap [i.e: array in the PHP jargon].` |
|       - | 1775 | ` * Return SXRET_OK on success. Any other value indicates failure.` |
|       - | 1776 | ` * Note on object conversion to array:` |
|       - | 1777 | ` *  Acccording to the PHP language reference manual` |
|       - | 1778 | ` *  If an object is converted to an array, the result is an array whose elements are the object's properties.` |
|       - | 1779 | ` *  The keys are the member variable names.` |
|       - | 1780 | ` *` |
|       - | 1781 | ` *  The following example:` |
|       - | 1782 | ` *  class Test {` |
|       - | 1783 | ` *   public $A = 25<<1;  // 50` |
|       - | 1784 | ` *	 public $c = rand_str(3);   // Random string of length 3` |
|       - | 1785 | ` *	 public $d = rand() & 1023; // Random number between 0..1023` |
|       - | 1786 | ` *  }` |
|       - | 1787 | ` *  var_dump((array) new Test());` |
|       - | 1788 | ` *	Will output:` |
|       - | 1789 | ` *  array(3) {` |
|       - | 1790 | ` *   [A] =>` |
|       - | 1791 | ` *      int(50)` |
|       - | 1792 | ` *   [c] =>` |
|       - | 1793 | ` *     string(3 'aps')` |
|       - | 1794 | ` *   [d] =>` |
|       - | 1795 | ` *     int(991)` |
|       - | 1796 | ` *  }` |
|       - | 1797 | ` * You have noticed that PH7 allow class attributes [i.e: $a,$c,$d in the example above]` |
|       - | 1798 | ` * have any complex expression (even function calls/Annonymous functions) as their default` |
|       - | 1799 | ` * value unlike the standard PHP engine.` |
|       - | 1800 | ` * This is a very powerful feature that you have to look at.` |
|       - | 1801 | ` */` |
|      20 | 1802 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|       2 | 1803 | `{` |
|       - | 1804 | `	SyHashEntry *pEntry;` |
|       - | 1805 | `	SyString *pAttrName;` |
|       - | 1806 | `	VmClassAttr *pAttr;` |
|       - | 1807 | `	ph7_value *pValue;` |
|       - | 1808 | `	ph7_value sName;` |
|       - | 1809 | `	/* Reset the loop cursor */` |
|      22 | 1810 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      22 | 1811 | `	PH7_MemObjInitFromString(pThis->pVm,&sName,0);` |
|      78 | 1812 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       - | 1813 | `		/* Point to the current attribute */` |
|      58 | 1814 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      58 | 1815 | `		if( pAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|       - | 1816 | `			/* php 8.4: a VIRTUAL hooked property has no backing store — the` |
|       - | 1817 | `			 * (array) cast excludes it (raw surface, get is NOT dispatched) */` |
|       7 | 1818 | `			continue;` |
|       - | 1819 | `		}` |
|       - | 1820 | `		/* Extract attribute value */` |
|      52 | 1821 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|      52 | 1822 | `		if( pValue ){` |
|       - | 1823 | `			/* Build attribute name. php MANGLES the key of a non-public property` |
|       - | 1824 | `			 * when it casts an object to an array: a private one becomes` |
|       - | 1825 | `			 * "\0DeclaringClass\0name" and a protected one "\0*\0name", so two` |
|       - | 1826 | `			 * same-named members from different visibility levels stay distinct` |
|       - | 1827 | ``			 * and `isset($arr['priv'])` is FALSE — PHL emitted the bare name,`` |
|       - | 1828 | `			 * which collided them and answered TRUE. The NULs are real bytes in` |
|       - | 1829 | `			 * the key (this append is length-based, not NUL-terminated). */` |
|      52 | 1830 | `			pAttrName = &pAttr->pAttr->sName;` |
|      52 | 1831 | `			if( pAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|      14 | 1832 | `				ph7_class *pDecl = pAttr->pAttr->pDeclClass` |
|       8 | 1833 | `					? pAttr->pAttr->pDeclClass : pThis->pClass;` |
|      10 | 1834 | `				PH7_MemObjStringAppend(&sName,"\0",1);` |
|      10 | 1835 | `				PH7_MemObjStringAppend(&sName,SyStringData(&pDecl->sName),SyStringLength(&pDecl->sName));` |
|      10 | 1836 | `				PH7_MemObjStringAppend(&sName,"\0",1);` |
|      48 | 1837 | `			}else if( pAttr->pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
|       8 | 1838 | `				PH7_MemObjStringAppend(&sName,"\0*\0",3);` |
|       3 | 1839 | `			}` |
|      52 | 1840 | `			PH7_MemObjStringAppend(&sName,pAttrName->zString,pAttrName->nByte);` |
|       - | 1841 | `			/* Perform the insertion */` |
|      52 | 1842 | `			PH7_HashmapInsert(pMap,&sName,pValue);` |
|       - | 1843 | `			/* Reset the string cursor */` |
|      52 | 1844 | `			SyBlobReset(&sName.sBlob);` |
|      25 | 1845 | `		}` |
|       2 | 1846 | `	}` |
|      22 | 1847 | `	PH7_MemObjRelease(&sName);` |
|      22 | 1848 | `	return SXRET_OK;` |
|       2 | 1849 | `}` |
|       - | 1850 | `/*` |
|       - | 1851 | ` * Iterate throw class attributes and invoke the given callback [i.e: xWalk()] for each` |
|       - | 1852 | ` * retrieved attribute.` |
|       - | 1853 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|       - | 1854 | ` * the attribute value in the callback body will not alter the real attribute value.` |
|       - | 1855 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|       - | 1856 | ` * a value different from PH7_OK.` |
|       - | 1857 | ` * Refer to [ph7_object_walk()] for more information.` |
|       - | 1858 | ` */` |
|     ! 0 | 1859 | `PH7_PRIVATE sxi32 PH7_ClassInstanceWalk(` |
|       - | 1860 | `	ph7_class_instance *pThis, /* Target object */` |
|       - | 1861 | `	int (*xWalk)(const char *,ph7_value *,void *), /* Walker callback */` |
|       - | 1862 | `	void *pUserData /* Last argument to xWalk() */` |
|       - | 1863 | `	)` |
|     ! 0 | 1864 | `{` |
|       - | 1865 | `	SyHashEntry *pEntry; /* Hash entry */` |
|       - | 1866 | `	VmClassAttr *pAttr;  /* Pointer to the attribute */` |
|       - | 1867 | `	ph7_value *pValue;   /* Attribute value */` |
|       - | 1868 | `	ph7_value sValue;    /* Copy of the attribute value */` |
|       - | 1869 | `	int rc;` |
|       - | 1870 | `	/* Reset the loop cursor */` |
|     ! 0 | 1871 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|     ! 0 | 1872 | `	PH7_MemObjInit(pThis->pVm,&sValue);` |
|       - | 1873 | `	/* Start the walk process */` |
|     ! 0 | 1874 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       - | 1875 | `		/* Point to the current attribute */` |
|     ! 0 | 1876 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1877 | `		/* Extract attribute value */` |
|     ! 0 | 1878 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     ! 0 | 1879 | `		if( pValue ){` |
|     ! 0 | 1880 | `			PH7_MemObjLoad(pValue,&sValue);` |
|       - | 1881 | `			/* Invoke the supplied callback */` |
|     ! 0 | 1882 | `			rc =  xWalk(SyStringData(&pAttr->pAttr->sName),&sValue,pUserData);` |
|     ! 0 | 1883 | `			PH7_MemObjRelease(&sValue);` |
|     ! 0 | 1884 | `			if( rc != PH7_OK){` |
|       - | 1885 | `				/* User callback request an operation abort */` |
|     ! 0 | 1886 | `				return SXERR_ABORT;` |
|       - | 1887 | `			}` |
|     ! 0 | 1888 | `		}` |
|     ! 0 | 1889 | `	}` |
|       - | 1890 | `	/* All done */` |
|     ! 0 | 1891 | `	return SXRET_OK;` |
|     ! 0 | 1892 | `}` |
|       - | 1893 | `/*` |
|       - | 1894 | ` * Extract a class atrribute value.` |
|       - | 1895 | ` * Return a pointer to the attribute value on success. Otherwise NULL.` |
|       - | 1896 | ` * Note:` |
|       - | 1897 | ` *  Access to static and constant attribute is not allowed. That is,the function` |
|       - | 1898 | ` *  will return NULL in case someone (host-application code) try to extract` |
|       - | 1899 | ` *  a static/constant attribute.` |
|       - | 1900 | ` */` |
|   11134 | 1901 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceFetchAttr(ph7_class_instance *pThis,const SyString *pName)` |
|       5 | 1902 | `{` |
|       - | 1903 | `	SyHashEntry *pEntry;` |
|       - | 1904 | `	VmClassAttr *pAttr;` |
|       - | 1905 | `	/* Query the attribute hashtable */` |
|   11139 | 1906 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);` |
|   11139 | 1907 | `	if( pEntry == 0 ){` |
|       - | 1908 | `		/* No such attribute */` |
|     ! 0 | 1909 | `		return 0;` |
|       - | 1910 | `	}` |
|       - | 1911 | `	/* Point to the class atrribute */` |
|   11139 | 1912 | `	pAttr = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1913 | `	/* Check if we are dealing with a static/constant attribute */` |
|   11139 | 1914 | `	if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|       - | 1915 | `		/* Access is forbidden */` |
|     ! 0 | 1916 | `		return 0;` |
|       - | 1917 | `	}` |
|       - | 1918 | `	/* Return the attribute value */` |
|   11139 | 1919 | `	return ExtractClassAttrValue(pThis->pVm,pAttr);` |
|    5572 | 1920 | `}` |
|       - | 1921 |  |
