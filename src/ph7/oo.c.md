# src/ph7/oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 727/831 lines (87.48%)

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
|  523460 |   14 | `PH7_PRIVATE ph7_class * PH7_NewRawClass(ph7_vm *pVm,const SyString *pName,sxu32 nLine)` |
|       5 |   15 | `{` |
|       - |   16 | `	ph7_class *pClass;` |
|       - |   17 | `	char *zName;` |
|       - |   18 | `	/* Allocate a new instance */` |
|  523465 |   19 | `	pClass = (ph7_class *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class));` |
|  523465 |   20 | `	if( pClass == 0 ){` |
|     ! 0 |   21 | `		return 0;` |
|       - |   22 | `	}` |
|       - |   23 | `	/* Zero the structure */` |
|  523465 |   24 | `	SyZero(pClass,sizeof(ph7_class));` |
|       - |   25 | `	/* Duplicate class name */` |
|  523465 |   26 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  523465 |   27 | `	if( zName == 0 ){` |
|     ! 0 |   28 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClass);` |
|     ! 0 |   29 | `		return 0;` |
|       - |   30 | `	}` |
|       - |   31 | `	/* Initialize fields */` |
|  523465 |   32 | `	SyStringInitFromBuf(&pClass->sName,zName,pName->nByte);` |
|  523465 |   33 | `	SyHashInit(&pClass->hMethod,&pVm->sAllocator,0,0);` |
|  523465 |   34 | `	SyHashInit(&pClass->hAttr,&pVm->sAllocator,0,0);` |
|  523465 |   35 | `	SyHashInit(&pClass->hDerived,&pVm->sAllocator,0,0);` |
|  523465 |   36 | `	SySetInit(&pClass->aInterface,&pVm->sAllocator,sizeof(ph7_class *));` |
|  523465 |   37 | `	SySetInit(&pClass->aTrait,&pVm->sAllocator,sizeof(ph7_class *));` |
|  523465 |   38 | `	SySetInit(&pClass->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|  523465 |   39 | `	SySetInit(&pClass->aEnumCases,&pVm->sAllocator,sizeof(ph7_class_attr *));` |
|  523465 |   40 | `	pClass->nLine = nLine;` |
|  523465 |   41 | `	if( pVm->bCompilingBuiltin ){` |
|       - |   42 | `		/* Defined by an embedded builtin chunk: internal, no defining file.` |
|       - |   43 | `		 * Class compilers merge further flags with \|= so this survives. */` |
|  521265 |   44 | `		pClass->iFlags \|= PH7_CLASS_INTERNAL;` |
|  260635 |   45 | `	}else{` |
|       - |   46 | `		/* Alias the VM-lifetime path dup on top of the include stack */` |
|    2205 |   47 | `		SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|    2205 |   48 | `		if( pFile ){` |
|    2205 |   49 | `			SyStringDupPtr(&pClass->sFile,pFile);` |
|    1100 |   50 | `		}` |
|       - |   51 | `	}` |
|       - |   52 | `	/* All done */` |
|  523465 |   53 | `	return pClass;` |
|  261735 |   54 | `}` |
|       - |   55 | `/*` |
|       - |   56 | ` * Allocate and initialize a new class attribute.` |
|       - |   57 | ` * Return a pointer to the class attribute on success. NULL otherwise.` |
|       - |   58 | ` */` |
|  950608 |   59 | `PH7_PRIVATE ph7_class_attr * PH7_NewClassAttr(ph7_vm *pVm,const SyString *pName,sxu32 nLine,sxi32 iProtection,sxi32 iFlags)` |
|       5 |   60 | `{` |
|       - |   61 | `	ph7_class_attr *pAttr;` |
|       - |   62 | `	char *zName;` |
|  950613 |   63 | `	pAttr = (ph7_class_attr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_attr));` |
|  950613 |   64 | `	if( pAttr == 0 ){` |
|     ! 0 |   65 | `		return 0;` |
|       - |   66 | `	}` |
|       - |   67 | `	/* Zero the structure */` |
|  950613 |   68 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|  950613 |   69 | `	SySetInit(&pAttr->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|       - |   70 | `	/* Duplicate attribute name */` |
|  950613 |   71 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  950613 |   72 | `	if( zName == 0 ){` |
|     ! 0 |   73 | `		SyMemBackendPoolFree(&pVm->sAllocator,pAttr);` |
|     ! 0 |   74 | `		return 0;` |
|       - |   75 | `	}` |
|       - |   76 | `	/* Initialize fields */` |
|  950613 |   77 | `	SySetInit(&pAttr->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|  950613 |   78 | `	SySetInit(&pAttr->aUnionAlts,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|  950613 |   79 | `	SyStringInitFromBuf(&pAttr->sName,zName,pName->nByte);` |
|  950613 |   80 | `	pAttr->iProtection = iProtection;` |
|  950613 |   81 | `	pAttr->nIdx = SXU32_HIGH;` |
|  950613 |   82 | `	pAttr->iFlags = iFlags;` |
|  950613 |   83 | `	pAttr->nLine = nLine;` |
|  950613 |   84 | `	return pAttr;` |
|  475309 |   85 | `}` |
|       - |   86 | `/*` |
|       - |   87 | ` * Allocate and initialize a new class method.` |
|       - |   88 | ` * Return a pointer to the class method on success. NULL otherwise` |
|       - |   89 | ` * This function associate with the newly created method an automatically generated` |
|       - |   90 | ` * random unique name.` |
|       - |   91 | ` */` |
| 2903964 |   92 | `PH7_PRIVATE ph7_class_method * PH7_NewClassMethod(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,sxu32 nLine,` |
|       - |   93 | `	sxi32 iProtection,sxi32 iFlags,sxi32 iFuncFlags)` |
|       5 |   94 | `{` |
|       - |   95 | `	ph7_class_method *pMeth;` |
|       - |   96 | `	SyHashEntry *pEntry;` |
|       - |   97 | `	SyString *pNamePtr;` |
|       - |   98 | `	char zSalt[10];` |
|       - |   99 | `	char *zName;` |
|       - |  100 | `	sxu32 nByte;` |
|       - |  101 | `	/* Allocate a new class method instance */` |
| 2903969 |  102 | `	pMeth = (ph7_class_method *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_method));` |
| 2903969 |  103 | `	if( pMeth == 0 ){` |
|     ! 0 |  104 | `		return 0;` |
|       - |  105 | `	}` |
|       - |  106 | `	/* Zero the structure */` |
| 2903969 |  107 | `	SyZero(pMeth,sizeof(ph7_class_method));` |
|       - |  108 | `	/* Check for an already installed method with the same name */` |
| 2903969 |  109 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
| 2903969 |  110 | `	if( pEntry == 0 ){` |
|       - |  111 | `		/* Associate an unique VM name to this method */` |
| 2903967 |  112 | `		nByte = sizeof(zSalt) + pName->nByte + SyStringLength(&pClass->sName)+sizeof(char)*7/*[[__'\0'*/;` |
| 2903967 |  113 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,nByte);` |
| 2903967 |  114 | `		if( zName == 0 ){` |
|     ! 0 |  115 | `			SyMemBackendPoolFree(&pVm->sAllocator,pMeth);` |
|     ! 0 |  116 | `			return 0;` |
|       - |  117 | `		}` |
| 2903967 |  118 | `		pNamePtr = &pMeth->sVmName;` |
|       - |  119 | `		/* Generate a random string */` |
| 2903967 |  120 | `		PH7_VmRandomString(&(*pVm),zSalt,sizeof(zSalt));` |
| 2903967 |  121 | `		pNamePtr->nByte = SyBufferFormat(zName,nByte,"[__%z@%z_%.*s]",&pClass->sName,pName,sizeof(zSalt),zSalt);` |
| 2903967 |  122 | `		pNamePtr->zString = zName;` |
| 1451986 |  123 | `	}else{` |
|       - |  124 | `		/* Method is condidate for 'overloading' */` |
|       3 |  125 | `		ph7_class_method *pCurrent = (ph7_class_method *)pEntry->pUserData;` |
|       3 |  126 | `		pNamePtr = &pMeth->sVmName;` |
|       - |  127 | `		/* Use the same VM name */` |
|       3 |  128 | `		SyStringDupPtr(pNamePtr,&pCurrent->sVmName);` |
|       3 |  129 | `		zName = (char *)pNamePtr->zString;` |
|       - |  130 | `	}` |
| 2903969 |  131 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|  167348 |  132 | `		if( (pName->nByte == sizeof("__destruct") - 1 && SyMemcmp(pName->zString,"__destruct",sizeof("__destruct") - 1 ) == 0)` |
|  167353 |  133 | `			\|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|       - |  134 | `				/* Switch to public visibility for destructors and legacy class-name` |
|       - |  135 | `				 * constructors (the engine invokes destructors internally, bypassing` |
|       - |  136 | `				 * visibility either way). __construct KEEPS its declared visibility` |
|       - |  137 | ``				 * (band A #4): php enforces it at `new` — a private/protected ctor`` |
|       - |  138 | `				 * from the wrong scope is a catchable Error, checked at OP_NEW —` |
|       - |  139 | `				 * and ReflectionClass::isInstantiable()/newInstance() now see it. */` |
|     ! 0 |  140 | `				iProtection = PH7_CLASS_PROT_PUBLIC;` |
|     ! 0 |  141 | `		}` |
|   83674 |  142 | `	}` |
|       - |  143 | `	/* Initialize method fields */` |
| 2903969 |  144 | `	pMeth->iProtection = iProtection;` |
| 2903969 |  145 | `	pMeth->iFlags = iFlags;` |
| 2903969 |  146 | `	pMeth->nLine = nLine;` |
| 4355951 |  147 | `	PH7_VmInitFuncState(&(*pVm),&pMeth->sFunc,&zName[sizeof(char)*4/*[__@*/+SyStringLength(&pClass->sName)],` |
| 2903964 |  148 | `		pName->nByte,iFuncFlags\|VM_FUNC_CLASS_METHOD,pClass);` |
| 2903969 |  149 | `	return pMeth;` |
| 1451987 |  150 | `}` |
|       - |  151 | `/*` |
|       - |  152 | ` * Check if the given name have a class method associated with it.` |
|       - |  153 | ` * Return the desired method [i.e: ph7_class_method instance] on success. NULL otherwise.` |
|       - |  154 | ` */` |
|  903680 |  155 | `PH7_PRIVATE ph7_class_method * PH7_ClassExtractMethod(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|       5 |  156 | `{` |
|       - |  157 | `	SyHashEntry *pEntry;` |
|       - |  158 | `	/* Perform a hash lookup */` |
|  903685 |  159 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)zName,nByte);` |
|  903685 |  160 | `	if( pEntry == 0 ){` |
|       - |  161 | `		/* No such entry */` |
|   30373 |  162 | `		return 0;` |
|       - |  163 | `	}` |
|       - |  164 | `	/* Point to the desired method */` |
|  873317 |  165 | `	return (ph7_class_method *)pEntry->pUserData;` |
|  451845 |  166 | `}` |
|       - |  167 | `/*` |
|       - |  168 | ` * Check if the given name is a class attribute.` |
|       - |  169 | ` * Return the desired attribute [i.e: ph7_class_attr instance] on success.NULL otherwise.` |
|       - |  170 | ` */` |
|  593980 |  171 | `PH7_PRIVATE ph7_class_attr * PH7_ClassExtractAttribute(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|       5 |  172 | `{` |
|       - |  173 | `	SyHashEntry *pEntry;` |
|       - |  174 | `	/* Perform a hash lookup */` |
|  593985 |  175 | `	pEntry = SyHashGet(&pClass->hAttr,(const void *)zName,nByte);` |
|  593985 |  176 | `	if( pEntry == 0 ){` |
|       - |  177 | `		/* No such entry */` |
|  592533 |  178 | `		return 0;` |
|       - |  179 | `	}` |
|       - |  180 | `	/* Point to the desierd method */` |
|    1457 |  181 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|  296995 |  182 | `}` |
|       - |  183 | `/*` |
|       - |  184 | ` * Install a class attribute in the corresponding container.` |
|       - |  185 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|       - |  186 | ` */` |
|  950604 |  187 | `PH7_PRIVATE sxi32 PH7_ClassInstallAttr(ph7_class *pClass,ph7_class_attr *pAttr)` |
|       5 |  188 | `{` |
|  950609 |  189 | `	SyString *pName = &pAttr->sName;` |
|       - |  190 | `	sxi32 rc;` |
|       - |  191 | `	/* Remember where this attribute was originally declared so that later` |
|       - |  192 | `	 * inheritance/trait copies still know the declaring class (needed for` |
|       - |  193 | `	 * PHP-compatible error messages on typed properties). */` |
|  950609 |  194 | `	if( pAttr->pDeclClass == 0 ){` |
|  950609 |  195 | `		pAttr->pDeclClass = pClass;` |
|  475302 |  196 | `	}` |
|  950609 |  197 | `	rc = SyHashInsertTail(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|  950609 |  198 | `	return rc;` |
|       5 |  199 | `}` |
|       - |  200 | `/*` |
|       - |  201 | ` * Install a class method in the corresponding container.` |
|       - |  202 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|       - |  203 | ` */` |
| 2903952 |  204 | `PH7_PRIVATE sxi32 PH7_ClassInstallMethod(ph7_class *pClass,ph7_class_method *pMeth)` |
|       5 |  205 | `{` |
| 2903957 |  206 | `	SyString *pName = &pMeth->sFunc.sName;` |
|       - |  207 | `	sxi32 rc;` |
| 2903957 |  208 | `	rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 2903957 |  209 | `	return rc;` |
|       5 |  210 | `}` |
|       - |  211 | `/*` |
|       - |  212 | ` * Method-override compatibility (variance) checking.` |
|       - |  213 | ` *` |
|       - |  214 | ` * PHP rejects an override whose signature is incompatible with the parent's:` |
|       - |  215 | ` * return types are covariant (child may only narrow), parameter types are` |
|       - |  216 | ` * contravariant (child may only widen), and a child may not add a required` |
|       - |  217 | ` * parameter. We add the diagnostic — but conservatively: PHL must keep running` |
|       - |  218 | ` * valid PHP, so the comparator below is SKIP-BY-DEFAULT. It flags only cases that` |
|       - |  219 | ` * are unambiguously invalid and silently accepts anything subtle (unions,` |
|       - |  220 | ` * intersections, pseudo-types, self/parent/static, object, unresolved classes,` |
|       - |  221 | ` * or a missing type), so it can never reject valid code.` |
|       - |  222 | ` */` |
|       - |  223 | `#define OVT_NONE   0  /* no declared type */` |
|       - |  224 | `#define OVT_SCALAR 1  /* a concrete invariant scalar: int/float/string/bool/array */` |
|       - |  225 | `#define OVT_CLASS  2  /* a real, already-loaded class/interface */` |
|       - |  226 | `#define OVT_SKIP   3  /* union/intersection/pseudo/self/object/unresolved — never flag */` |
|       - |  227 |  |
|       - |  228 | `/*` |
|       - |  229 | ` * Classify one declared type (nType + class name + union flag) for override` |
|       - |  230 | ` * comparison. On OVT_CLASS, *ppClass receives the resolved class. Class names are` |
|       - |  231 | ` * resolved by a direct, autoload-free hClass lookup: a miss (forward reference,` |
|       - |  232 | ` * namespaced, or not-yet-loaded) yields OVT_SKIP, which the caller accepts.` |
|       - |  233 | ` */` |
|  280312 |  234 | `static int OoClassifyOverrideType(ph7_vm *pVm, sxu32 nType, const SyString *pClass,` |
|       - |  235 | `	int bUnion, ph7_class **ppClass)` |
|       5 |  236 | `{` |
|  280317 |  237 | `	*ppClass = 0;` |
|  280317 |  238 | `	if( bUnion ){` |
|       3 |  239 | `		return OVT_SKIP; /* union/intersection — full lattice, skip */` |
|       - |  240 | `	}` |
|  280315 |  241 | `	if( nType == 0 ){` |
|  280211 |  242 | `		return OVT_NONE; /* no declared type */` |
|       - |  243 | `	}` |
|     108 |  244 | `	if( nType == SXU32_HIGH ){` |
|       - |  245 | `		/* A class name OR a pseudo-type stored as a name atom. Skip every pseudo` |
|       - |  246 | `		 * (incl. self/parent/static, which are context-relative). */` |
|       - |  247 | `		static const struct { const char *z; sxu32 n; } aPseudo[] = {` |
|       - |  248 | `			{"mixed",5}, {"never",5}, {"iterable",8}, {"callable",8}, {"true",4},` |
|       - |  249 | `			{"false",5}, {"self",4}, {"parent",6}, {"static",6}` |
|       - |  250 | `		};` |
|      23 |  251 | `		const char *z = pClass->zString;` |
|      23 |  252 | `		sxu32 n = pClass->nByte;` |
|       - |  253 | `		SyHashEntry *pE;` |
|       - |  254 | `		sxu32 i;` |
|     159 |  255 | `		for( i = 0; i < SX_ARRAYSIZE(aPseudo); i++ ){` |
|     147 |  256 | `			if( n == aPseudo[i].n && SyStrnmicmp(z,aPseudo[i].z,n) == 0 ){` |
|      10 |  257 | `				return OVT_SKIP;` |
|       - |  258 | `			}` |
|      70 |  259 | `		}` |
|      14 |  260 | `		pE = SyHashGet(&pVm->hClass,(const void *)z,n);` |
|      14 |  261 | `		if( pE == 0 ){` |
|     ! 0 |  262 | `			return OVT_SKIP; /* not loaded / forward ref / namespaced — accept */` |
|       - |  263 | `		}` |
|      14 |  264 | `		*ppClass = (ph7_class *)pE->pUserData;` |
|      14 |  265 | `		return OVT_CLASS;` |
|       - |  266 | `	}` |
|      84 |  267 | `	if( nType == MEMOBJ_STRING \|\| nType == MEMOBJ_INT \|\| nType == MEMOBJ_REAL` |
|      30 |  268 | `	 \|\| nType == MEMOBJ_BOOL \|\| nType == MEMOBJ_HASHMAP ){` |
|      63 |  269 | `		return OVT_SCALAR;` |
|       - |  270 | `	}` |
|       - |  271 | `	/* MEMOBJ_OBJ (object — subtypes against classes), MEMOBJ_VOID/NULL/RES,` |
|       - |  272 | `	 * or anything unexpected: skip. */` |
|      26 |  273 | `	return OVT_SKIP;` |
|  140161 |  274 | `}` |
|       - |  275 |  |
|       - |  276 | `/*` |
|       - |  277 | ` * A declared type normalized for override comparison: the raw type code, the` |
|       - |  278 | ` * class-name string (when a class), and the union/nullable flags. Extracted once` |
|       - |  279 | ` * from each side so the comparator takes two of these instead of eight scalars.` |
|       - |  280 | ` */` |
|       - |  281 | `typedef struct OvType OvType;` |
|       - |  282 | `struct OvType {` |
|       - |  283 | `	sxu32 nType;` |
|       - |  284 | `	const SyString *pClass;` |
|       - |  285 | `	int bUnion;` |
|       - |  286 | `	int bNullable;` |
|       - |  287 | `};` |
|  210228 |  288 | `static OvType OoTypeFromReturn(ph7_vm_func *pF)` |
|       5 |  289 | `{` |
|       - |  290 | `	OvType t;` |
|  210233 |  291 | `	t.nType = pF->nReturnType;` |
|  210233 |  292 | `	t.pClass = &pF->sReturnClass;` |
|  210233 |  293 | `	t.bUnion = SySetUsed(&pF->aReturnUnion) > 0;` |
|  210233 |  294 | `	t.bNullable = (pF->iFlags & VM_FUNC_RETURN_NULLABLE) != 0;` |
|  210233 |  295 | `	return t;` |
|       5 |  296 | `}` |
|   70084 |  297 | `static OvType OoTypeFromArg(ph7_vm_func_arg *pA)` |
|       5 |  298 | `{` |
|       - |  299 | `	OvType t;` |
|   70089 |  300 | `	t.nType = pA->nType;` |
|   70089 |  301 | `	t.pClass = &pA->sClass;` |
|   70089 |  302 | `	t.bUnion = (pA->iFlags & VM_FUNC_ARG_UNION) != 0;` |
|   70089 |  303 | `	t.bNullable = (pA->iFlags & VM_FUNC_ARG_NULLABLE) != 0;` |
|   70089 |  304 | `	return t;` |
|       5 |  305 | `}` |
|       - |  306 | `/*` |
|       - |  307 | ` * Return TRUE if the child type is an unambiguously-invalid override of the` |
|       - |  308 | ` * parent type. bCovariant=1 for a return type (child must be ⊆ parent),` |
|       - |  309 | ` * 0 for a parameter (child must be ⊇ parent). Returns FALSE (accept) on any` |
|       - |  310 | ` * skipped/ambiguous shape.` |
|       - |  311 | ` */` |
|  140156 |  312 | `static int OoOverrideTypeBad(ph7_vm *pVm, OvType parent, OvType child, int bCovariant)` |
|       5 |  313 | `{` |
|       - |  314 | `	ph7_class *pParentCls, *pChildCls;` |
|  140161 |  315 | `	int kP = OoClassifyOverrideType(pVm, parent.nType, parent.pClass, parent.bUnion, &pParentCls);` |
|  140161 |  316 | `	int kC = OoClassifyOverrideType(pVm, child.nType, child.pClass, child.bUnion, &pChildCls);` |
|  140161 |  317 | `	if( kP == OVT_SKIP \|\| kC == OVT_SKIP ){` |
|      21 |  318 | `		return 0; /* ambiguous shape — conservatively accept */` |
|       - |  319 | `	}` |
|       - |  320 | `	/* A missing type is the TOP type. covariant (return): a concrete child is a` |
|       - |  321 | `	 * subtype of top, fine; a top child over a concrete parent WIDENS → bad.` |
|       - |  322 | `	 * contravariant (param): a top child is a supertype of anything, fine; a` |
|       - |  323 | `	 * concrete child over a top parent NARROWS → bad. (A union/intersection child` |
|       - |  324 | `	 * already fell into OVT_SKIP above, so a flagged child here is scalar/class.) */` |
|  140143 |  325 | `	if( kP == OVT_NONE \|\| kC == OVT_NONE ){` |
|  140113 |  326 | `		if( bCovariant && kC == OVT_NONE && kP != OVT_NONE ) return 1;` |
|  140113 |  327 | `		if( !bCovariant && kP == OVT_NONE && kC != OVT_NONE ) return 1;` |
|  140113 |  328 | `		return 0;` |
|       - |  329 | `	}` |
|       - |  330 | `	/* Nullability: a covariant return may not ADD null; a contravariant param may` |
|       - |  331 | `	 * not REMOVE null. */` |
|      34 |  332 | `	if( bCovariant ){` |
|      17 |  333 | `		if( child.bNullable && !parent.bNullable ) return 1;` |
|      10 |  334 | `	}else{` |
|      19 |  335 | `		if( parent.bNullable && !child.bNullable ) return 1;` |
|       - |  336 | `	}` |
|      34 |  337 | `	if( kP == OVT_SCALAR && kC == OVT_SCALAR ){` |
|       - |  338 | `		/* Scalars are invariant — they must match exactly. */` |
|      27 |  339 | `		return (parent.nType != child.nType) ? 1 : 0;` |
|       - |  340 | `	}` |
|       8 |  341 | `	if( kP == OVT_CLASS && kC == OVT_CLASS ){` |
|       8 |  342 | `		if( bCovariant ){` |
|       3 |  343 | `			return PH7_VmInstanceOf(pChildCls, pParentCls) ? 0 : 1;  /* child ⊆ parent */` |
|       - |  344 | `		}` |
|       6 |  345 | `		return PH7_VmInstanceOf(pParentCls, pChildCls) ? 0 : 1;      /* child ⊇ parent */` |
|       - |  346 | `	}` |
|       - |  347 | `	/* One scalar and one class — disjoint. */` |
|     ! 0 |  348 | `	return 1;` |
|   70083 |  349 | `}` |
|       - |  350 |  |
|       - |  351 | `/*` |
|       - |  352 | ` * Check a child method's signature against the parent method it overrides.` |
|       - |  353 | ` * Emits a PHP-style "Declaration of … must be compatible …" fatal on a clear` |
|       - |  354 | `` * incompatibility. `__construct` is exempt (PHP does not apply variance to it).`` |
|       - |  355 | ` */` |
|  147912 |  356 | `static sxi32 OoCheckOverrideCompat(ph7_gen_state *pGen, ph7_class *pBase, ph7_class *pSub,` |
|       - |  357 | `	ph7_class_method *pParent, ph7_class_method *pChild)` |
|       5 |  358 | `{` |
|  147917 |  359 | `	ph7_vm *pVm = pGen->pVm;` |
|  147917 |  360 | `	ph7_vm_func *pPF = &pParent->sFunc;` |
|  147917 |  361 | `	ph7_vm_func *pCF = &pChild->sFunc;` |
|  147917 |  362 | `	SyString *pMName = &pCF->sName;` |
|       - |  363 | `	ph7_vm_func_arg *aP, *aC;` |
|       - |  364 | `	sxu32 nPArg, nCArg, k;` |
|  147917 |  365 | `	int bBad = 0;` |
|  147912 |  366 | `	if( pMName->nByte == sizeof("__construct")-1` |
|   97309 |  367 | `	 && SyStrnmicmp(pMName->zString,"__construct",pMName->nByte) == 0 ){` |
|   42803 |  368 | `		return SXRET_OK;` |
|       - |  369 | `	}` |
|       - |  370 | `	/* Return type — covariant. */` |
|  105119 |  371 | `	bBad = OoOverrideTypeBad(pVm, OoTypeFromReturn(pPF), OoTypeFromReturn(pCF), /* bCovariant */ 1);` |
|       - |  372 | `	/* Each overlapping parameter — contravariant. */` |
|  105119 |  373 | `	nPArg = SySetUsed(&pPF->aArgs);` |
|  105119 |  374 | `	nCArg = SySetUsed(&pCF->aArgs);` |
|  105119 |  375 | `	aP = (ph7_vm_func_arg *)SySetBasePtr(&pPF->aArgs);` |
|  105119 |  376 | `	aC = (ph7_vm_func_arg *)SySetBasePtr(&pCF->aArgs);` |
|  140161 |  377 | `	for( k = 0; !bBad && k < nPArg && k < nCArg; k++ ){` |
|   35047 |  378 | `		bBad = OoOverrideTypeBad(pVm, OoTypeFromArg(&aP[k]), OoTypeFromArg(&aC[k]), /* bCovariant */ 0);` |
|   17526 |  379 | `	}` |
|       - |  380 | `	/* Parameter arity: the child must declare at least the parent's parameters and` |
|       - |  381 | `	 * may add only OPTIONAL ones — PHP rejects dropping any param (even an optional` |
|       - |  382 | `	 * one) or adding a required one. Skip the rule if either signature is variadic` |
|       - |  383 | `	 * (arity semantics differ). */` |
|  105119 |  384 | `	if( !bBad ){` |
|  105115 |  385 | `		int bVariadic = 0;` |
|  140155 |  386 | `		for( k = 0; k < nPArg; k++ ){ if( aP[k].iFlags & VM_FUNC_ARG_VARIADIC ) bVariadic = 1; }` |
|  140157 |  387 | `		for( k = 0; k < nCArg; k++ ){ if( aC[k].iFlags & VM_FUNC_ARG_VARIADIC ) bVariadic = 1; }` |
|  105115 |  388 | `		if( !bVariadic ){` |
|  105115 |  389 | `			if( nCArg < nPArg ){` |
|     ! 0 |  390 | `				bBad = 1; /* dropped a parent parameter */` |
|     ! 0 |  391 | `			}else{` |
|  105117 |  392 | `				for( k = nPArg; k < nCArg; k++ ){` |
|       3 |  393 | `					if( SySetUsed(&aC[k].aByteCode) == 0 ){ bBad = 1; break; } /* new required */` |
|       2 |  394 | `				}` |
|       - |  395 | `			}` |
|   52555 |  396 | `		}` |
|   52555 |  397 | `	}` |
|  105119 |  398 | `	if( bBad ){` |
|       8 |  399 | `		sxi32 rc = PH7_GenCompileError(&(*pGen),E_ERROR,pChild->nLine,` |
|       - |  400 | `			"Declaration of %z::%z() must be compatible with %z::%z()",` |
|       2 |  401 | `			&pSub->sName,pMName,&pBase->sName,&pParent->sFunc.sName);` |
|       6 |  402 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  403 | `			return SXERR_ABORT;` |
|       - |  404 | `		}` |
|       2 |  405 | `	}` |
|  105119 |  406 | `	return SXRET_OK;` |
|   73961 |  407 | `}` |
|       - |  408 | `/*` |
|       - |  409 | ` * Perform an inheritance operation.` |
|       - |  410 | ` * According to the PHP language reference manual` |
|       - |  411 | ` *  When you extend a class, the subclass inherits all of the public and protected methods` |
|       - |  412 | ` *  from the parent class. Unless a class Overwrites those methods, they will retain their original` |
|       - |  413 | ` *  functionality.` |
|       - |  414 | ` *  This is useful for defining and abstracting functionality, and permits the implementation` |
|       - |  415 | ` *  of additional functionality in similar objects without the need to reimplement all of the shared` |
|       - |  416 | ` *  functionality.` |
|       - |  417 | ` *  Example #1 Inheritance Example` |
|       - |  418 | ` * <?php` |
|       - |  419 | ` * class foo` |
|       - |  420 | ` * {` |
|       - |  421 | ` *   public function printItem($string)` |
|       - |  422 | ` *   {` |
|       - |  423 | ` *       echo 'Foo: ' . $string . PHP_EOL;` |
|       - |  424 | ` *   }` |
|       - |  425 | ` *` |
|       - |  426 | ` *   public function printPHP()` |
|       - |  427 | ` *   {` |
|       - |  428 | ` *       echo 'PHP is great.' . PHP_EOL;` |
|       - |  429 | ` *   }` |
|       - |  430 | ` * }` |
|       - |  431 | ` * class bar extends foo` |
|       - |  432 | ` * {` |
|       - |  433 | ` *   public function printItem($string)` |
|       - |  434 | ` *   {` |
|       - |  435 | ` *       echo 'Bar: ' . $string . PHP_EOL;` |
|       - |  436 | ` *   }` |
|       - |  437 | ` * }` |
|       - |  438 | ` * $foo = new foo();` |
|       - |  439 | ` * $bar = new bar();` |
|       - |  440 | ` * $foo->printItem('baz'); // Output: 'Foo: baz'` |
|       - |  441 | ` * $foo->printPHP();       // Output: 'PHP is great'` |
|       - |  442 | ` * $bar->printItem('baz'); // Output: 'Bar: baz'` |
|       - |  443 | ` * $bar->printPHP();       // Output: 'PHP is great'` |
|       - |  444 | ` *` |
|       - |  445 | ` * This function return SXRET_OK if the inheritance operation was successfully performed.` |
|       - |  446 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|       - |  447 | ` * error message.` |
|       - |  448 | ` */` |
|  241484 |  449 | `PH7_PRIVATE sxi32 PH7_ClassInherit(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase)` |
|       5 |  450 | `{` |
|       - |  451 | `	ph7_class_method *pMeth;` |
|       - |  452 | `	ph7_class_attr *pAttr;` |
|       - |  453 | `	SyHashEntry *pEntry;` |
|       - |  454 | `	SyString *pName;` |
|       - |  455 | `	SySet aInherited; /* base attributes to prepend (see the copy loop below) */` |
|       - |  456 | `	sxi32 rc;` |
|  241489 |  457 | `	SySetInit(&aInherited,&pGen->pVm->sAllocator,sizeof(ph7_class_attr *));` |
|       - |  458 | `	/* Install in the derived hashtable */` |
|  241489 |  459 | `	rc = SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|  241489 |  460 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  461 | `		SySetRelease(&aInherited);` |
|     ! 0 |  462 | `		return rc;` |
|       - |  463 | `	}` |
|       - |  464 | `	/* readonly class inheritance (PHP 8.2): a readonly class may only extend a` |
|       - |  465 | `	 * readonly class, and a non-readonly class may not extend a readonly one. */` |
|  241489 |  466 | `	if( (pBase->iFlags & PH7_CLASS_READONLY) != (pSub->iFlags & PH7_CLASS_READONLY) ){` |
|       5 |  467 | `		if( pBase->iFlags & PH7_CLASS_READONLY ){` |
|       4 |  468 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|       - |  469 | `				"Non-readonly class %z cannot extend readonly class %z",` |
|       1 |  470 | `				&pSub->sName,&pBase->sName);` |
|       2 |  471 | `		}else{` |
|       4 |  472 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|       - |  473 | `				"Readonly class %z cannot extend non-readonly class %z",` |
|       1 |  474 | `				&pSub->sName,&pBase->sName);` |
|       - |  475 | `		}` |
|       5 |  476 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  477 | `			SySetRelease(&aInherited);` |
|     ! 0 |  478 | `			return SXERR_ABORT;` |
|       - |  479 | `		}` |
|       2 |  480 | `	}` |
|       - |  481 | `	/* Copy public/protected attributes from the base class */` |
|  241489 |  482 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
| 1370147 |  483 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|       - |  484 | `		/* Make sure the private attributes are not redeclared in the subclass */` |
| 1128663 |  485 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
| 1128663 |  486 | `		pName = &pAttr->sName;` |
| 1128663 |  487 | `		if( (pEntry = SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|      20 |  488 | `			if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_FINAL))` |
|      13 |  489 | `				== (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_FINAL) ){` |
|       - |  490 | `				/* Cannot override a final class constant (PHP 8.1). Report the` |
|       - |  491 | `				 * class that originally declared it (pDeclClass) rather than the` |
|       - |  492 | `				 * immediate base, so a multi-level chain matches PHP. */` |
|       3 |  493 | `				ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pBase;` |
|       4 |  494 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_attr *)pEntry->pUserData)->nLine,` |
|       - |  495 | `					"%z::%z cannot override final constant %z::%z",` |
|       1 |  496 | `					&pSub->sName,pName,&pOwner->sName,pName);` |
|       3 |  497 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  498 | `					SySetRelease(&aInherited);` |
|     ! 0 |  499 | `					return SXERR_ABORT;` |
|       - |  500 | `				}` |
|       1 |  501 | `			}` |
|       - |  502 | `			/* A child MAY redeclare a base's private property: php treats the two` |
|       - |  503 | `			 * as independent members (each private to its declaring class), with no` |
|       - |  504 | `			 * diagnostic. PH7 warned here, which is wrong — the child's entry simply` |
|       - |  505 | `			 * shadows the base's in the by-name attribute table.` |
|       - |  506 | `			 *` |
|       - |  507 | `			 * Ordering: php keeps an overridden INSTANCE property at the position` |
|       - |  508 | `` 			 * the BASE declared it (`class G{$g1;$g2;} class H extends G{$h;$g1;}` `` |
|       - |  509 | `			 * iterates g1,g2,h — not g2,h,g1). Re-collect the CHILD's definition,` |
|       - |  510 | `			 * whose default value wins, and drop its current entry so the prepend` |
|       - |  511 | `			 * below re-inserts it in base order. Statics/constants are not part of` |
|       - |  512 | `			 * instance iteration, so they keep their existing slot. */` |
|      23 |  513 | `			if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|      20 |  514 | `				ph7_class_attr *pOwn = (ph7_class_attr *)pEntry->pUserData;` |
|      20 |  515 | `				SyHashDeleteEntry(&pSub->hAttr,(const void *)pName->zString,pName->nByte,0);` |
|      20 |  516 | `				rc = SySetPut(&aInherited,(const void *)&pOwn);` |
|      20 |  517 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  518 | `					SySetRelease(&aInherited);` |
|     ! 0 |  519 | `					return rc;` |
|       - |  520 | `				}` |
|       9 |  521 | `			}` |
|      23 |  522 | `			continue;` |
|       - |  523 | `		}` |
|       - |  524 | `		/* Collect the attribute. php: a base class's private INSTANCE property` |
|       - |  525 | `		 * lives on every child instance too (its own methods read/write it` |
|       - |  526 | `		 * through $this on the child; the access check grants private access by` |
|       - |  527 | `		 * DECLARING class, so child methods and outsiders still can't touch it).` |
|       - |  528 | `		 * Private STATICS/CONSTANTS stay uncopied — base methods reach those` |
|       - |  529 | `		 * through self:: against the declaring class directly.` |
|       - |  530 | `		 *` |
|       - |  531 | `		 * These are gathered rather than installed here because php orders an` |
|       - |  532 | `		 * instance's properties BASE-DECLARED FIRST, then the subclass's own,` |
|       - |  533 | `		 * then trait members — while inheritance runs AFTER the subclass body` |
|       - |  534 | `		 * has already filled hAttr. They are prepended below. */` |
| 1128638 |  535 | `		if( pAttr->iProtection != PH7_CLASS_PROT_PRIVATE` |
|  718057 |  536 | `		 \|\| (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
| 1128637 |  537 | `			rc = SySetPut(&aInherited,(const void *)&pAttr);` |
| 1128637 |  538 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  539 | `				SySetRelease(&aInherited);` |
|     ! 0 |  540 | `				return rc;` |
|       - |  541 | `			}` |
|  564316 |  542 | `		}` |
|       5 |  543 | `	}` |
|       - |  544 | `	/* Prepend the collected base attributes so hAttr reads base-first. The` |
|       - |  545 | `	 * table's iteration list is what every object-iteration consumer walks` |
|       - |  546 | `	 * (var_dump/print_r, get_object_vars, foreach, (array) casts, json_encode),` |
|       - |  547 | `	 * so this ordering is user-visible — json_encode emits its keys in exactly` |
|       - |  548 | `	 * this order. SyHashInsert is a HEAD insert, so walking the collected set` |
|       - |  549 | `	 * backwards leaves the base's own declaration order at the front. */` |
|  241489 |  550 | `	if( SySetUsed(&aInherited) > 0 ){` |
|  241355 |  551 | `		ph7_class_attr **apInherited = (ph7_class_attr **)SySetBasePtr(&aInherited);` |
|  241355 |  552 | `		sxu32 n = SySetUsed(&aInherited);` |
| 1370005 |  553 | `		while( n > 0 ){` |
| 1128655 |  554 | `			ph7_class_attr *pIn = apInherited[--n];` |
| 1128655 |  555 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pIn->sName.zString,pIn->sName.nByte,pIn);` |
| 1128655 |  556 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  557 | `				SySetRelease(&aInherited);` |
|     ! 0 |  558 | `				return rc;` |
|       - |  559 | `			}` |
|       5 |  560 | `		}` |
|  120675 |  561 | `	}` |
|  241489 |  562 | `	SySetRelease(&aInherited);` |
|  241489 |  563 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
| 3552895 |  564 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|       - |  565 | `		/* Make sure the private/final methods are not redeclared in the subclass */` |
| 3311411 |  566 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
| 3311411 |  567 | `		pName = &pMeth->sFunc.sName;` |
| 3311411 |  568 | `		if( (pEntry = SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|  147921 |  569 | `			 if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|       - |  570 | `				/* php: "Cannot override final method A::test()" */` |
|       7 |  571 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_method *)pEntry->pUserData)->nLine,` |
|       - |  572 | `					"Cannot override final method %z::%z()",` |
|       2 |  573 | `					&pBase->sName,pName);` |
|       2 |  574 | `				(void)pSub;` |
|       5 |  575 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  576 | `					return SXERR_ABORT;` |
|       - |  577 | `				}` |
|       3 |  578 | `			}else{` |
|       - |  579 | `				/* Check the override's signature is compatible with the parent's. */` |
|  221873 |  580 | `				rc = OoCheckOverrideCompat(&(*pGen),pBase,pSub,pMeth,` |
|  147912 |  581 | `					(ph7_class_method *)pEntry->pUserData);` |
|  147917 |  582 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  583 | `					return SXERR_ABORT;` |
|       - |  584 | `				}` |
|       - |  585 | `			}` |
|  147921 |  586 | `			continue;` |
|       - |  587 | `		}` |
|       - |  588 | `		/* Install the method. php: a base class's private INSTANCE method is` |
|       - |  589 | `		 * dispatchable on child instances too — an inherited public method` |
|       - |  590 | `		 * calling $this->priv() must find it (the call-site visibility check` |
|       - |  591 | `		 * binds by DECLARING class, sFunc.pUserData, so child code and` |
|       - |  592 | `		 * outsiders still can't call it; a private ctor copied down also` |
|       - |  593 | ``		 * blocks `new Child` from outside like php). Private STATICS stay`` |
|       - |  594 | `		 * uncopied — base methods reach those through self:: against the` |
|       - |  595 | `		 * declaring class directly. */` |
| 3163490 |  596 | `		if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE` |
| 1597319 |  597 | `		 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
| 3163495 |  598 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 3163495 |  599 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  600 | `				return rc;` |
|       - |  601 | `			}` |
| 1581745 |  602 | `		}` |
|       5 |  603 | `	}` |
|       - |  604 | `	/* Mark as subclass */` |
|  241489 |  605 | `	pSub->pBase = pBase;` |
|       - |  606 | `	/* All done */` |
|  241489 |  607 | `	return SXRET_OK;` |
|  120747 |  608 | `}` |
|       - |  609 | `/*` |
|       - |  610 | ` * Apply a trait to a class: copy all methods and attributes from the trait` |
|       - |  611 | ` * into the target class. Unlike inheritance, traits copy ALL members including` |
|       - |  612 | ` * private ones. Members already defined in the class take precedence.` |
|       - |  613 | ` */` |
|   15648 |  614 | `PH7_PRIVATE sxi32 PH7_ClassUseTrait(ph7_gen_state *pGen,ph7_class *pClass,ph7_class *pTrait)` |
|       5 |  615 | `{` |
|       - |  616 | `	ph7_class_method *pMeth;` |
|       - |  617 | `	ph7_class_attr *pAttr;` |
|       - |  618 | `	SyHashEntry *pEntry;` |
|       - |  619 | `	SyString *pName;` |
|       - |  620 | `	sxi32 rc;` |
|       - |  621 | `	/* Detect cyclic trait composition (e.g. trait A { use B; } trait B { use A; }) */` |
|   15653 |  622 | `	if( pTrait->iFlags & PH7_CLASS_TRAIT_VISITING ){` |
|     ! 0 |  623 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|     ! 0 |  624 | `			"Trait circular reference detected: %z is already being applied",&pTrait->sName);` |
|     ! 0 |  625 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  626 | `			return SXERR_ABORT;` |
|       - |  627 | `		}` |
|     ! 0 |  628 | `		return SXRET_OK;` |
|       - |  629 | `	}` |
|   15653 |  630 | `	pTrait->iFlags \|= PH7_CLASS_TRAIT_VISITING;` |
|   15653 |  631 | `	rc = SXRET_OK;` |
|       - |  632 | `	/* Copy attributes from the trait */` |
|   15653 |  633 | `	SyHashResetLoopCursor(&pTrait->hAttr);` |
|   62359 |  634 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hAttr)) != 0 ){` |
|       - |  635 | `		SyHashEntry *pExisting;` |
|   46711 |  636 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   46711 |  637 | `		pName = &pAttr->sName;` |
|   46711 |  638 | `		pExisting = SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte);` |
|   46711 |  639 | `		if( pExisting != 0 ){` |
|       - |  640 | `			/* Attribute already exists. Check if it came from another trait` |
|       - |  641 | `			 * and whether the definitions are compatible (same defaults).` |
|       - |  642 | `			 */` |
|       - |  643 | `			ph7_class **apUsedTraits;` |
|       - |  644 | `			sxu32 nUsed,k;` |
|       6 |  645 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       6 |  646 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|       6 |  647 | `			for(k = 0; k < nUsed; k++){` |
|       - |  648 | `				ph7_class_attr *pOther;` |
|       3 |  649 | `				pOther = PH7_ClassExtractAttribute(apUsedTraits[k],pName->zString,pName->nByte);` |
|       3 |  650 | `				if( pOther ){` |
|       - |  651 | `					/* Two traits define the same property — check if defaults differ */` |
|       3 |  652 | `					ph7_class_attr *pClassAttr = (ph7_class_attr *)pExisting->pUserData;` |
|       4 |  653 | `					if( SySetUsed(&pAttr->aByteCode) != SySetUsed(&pClassAttr->aByteCode) \|\|` |
|       3 |  654 | `						(SySetUsed(&pAttr->aByteCode) > 0 &&` |
|       3 |  655 | `						 SyMemcmp(SySetBasePtr(&pAttr->aByteCode),SySetBasePtr(&pClassAttr->aByteCode),` |
|       3 |  656 | `							SySetUsed(&pAttr->aByteCode) * SySetElemSize(&pAttr->aByteCode)) != 0) ){` |
|       4 |  657 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine,` |
|       - |  658 | `							"%z and %z define the same property ($%z) in the composition of %z. "` |
|       - |  659 | `							"However, the definition differs and is considered incompatible",` |
|       2 |  660 | `							&apUsedTraits[k]->sName,&pTrait->sName,pName,&pClass->sName);` |
|       3 |  661 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  662 | `							goto cleanup;` |
|       - |  663 | `						}` |
|       1 |  664 | `					}` |
|       3 |  665 | `					break;` |
|       - |  666 | `				}` |
|     ! 0 |  667 | `			}` |
|       6 |  668 | `			continue;` |
|       - |  669 | `		}` |
|   46707 |  670 | `		rc = SyHashInsertTail(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|   46707 |  671 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  672 | `			goto cleanup;` |
|       - |  673 | `		}` |
|       5 |  674 | `	}` |
|       - |  675 | `	/* Copy methods from the trait */` |
|   15653 |  676 | `	SyHashResetLoopCursor(&pTrait->hMethod);` |
|  225807 |  677 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hMethod)) != 0 ){` |
|       - |  678 | `		SyHashEntry *pClassMethEntry;` |
|  210159 |  679 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  210159 |  680 | `		pName = &pMeth->sFunc.sName;` |
|  210159 |  681 | `		pClassMethEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
|  210159 |  682 | `		if( pClassMethEntry != 0 ){` |
|       - |  683 | `			/* Method already exists in the class. An ABSTRACT trait method is a` |
|       - |  684 | `			 * REQUIREMENT, not an implementation: php satisfies it with any concrete` |
|       - |  685 | `			 * method of the same name (from the class body or another trait) — no` |
|       - |  686 | `			 * collision. Only two CONCRETE trait methods actually conflict. */` |
|      18 |  687 | `			ph7_class_method *pExistingMeth = (ph7_class_method *)pClassMethEntry->pUserData;` |
|      18 |  688 | `			int bIncomingAbstract = (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0;` |
|      18 |  689 | `			int bExistingAbstract = (pExistingMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0;` |
|       - |  690 | `			ph7_class **apUsedTraits;` |
|       - |  691 | `			sxu32 nUsed,k;` |
|      18 |  692 | `			if( bIncomingAbstract ){` |
|       - |  693 | `				/* Incoming abstract requirement: the existing (concrete or abstract)` |
|       - |  694 | `				 * method already covers this name — keep it. */` |
|       8 |  695 | `				continue;` |
|       - |  696 | `			}` |
|      11 |  697 | `			if( bExistingAbstract ){` |
|       - |  698 | `				/* Existing entry is only an abstract requirement (from an earlier` |
|       - |  699 | `				 * trait): the incoming concrete method satisfies and replaces it. */` |
|       3 |  700 | `				pClassMethEntry->pUserData = (void *)pMeth;` |
|       3 |  701 | `				continue;` |
|       - |  702 | `			}` |
|       - |  703 | `			/* Both concrete: a genuine collision only when the OTHER definition came` |
|       - |  704 | `			 * from another trait (a concrete one). A class-body method wins silently. */` |
|       8 |  705 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       8 |  706 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|       8 |  707 | `			for(k = 0; k < nUsed; k++){` |
|       3 |  708 | `				ph7_class_method *pOtherMeth = PH7_ClassExtractMethod(apUsedTraits[k],pName->zString,pName->nByte);` |
|       3 |  709 | `				if( pOtherMeth != 0 && (pOtherMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       - |  710 | `					/* Two different traits define the same CONCRETE method with no resolution */` |
|       4 |  711 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|       - |  712 | `						"Trait method %z::%z has not been applied as %z::%z, "` |
|       - |  713 | `						"because of collision with %z::%z",` |
|       2 |  714 | `						&pTrait->sName,pName,` |
|       1 |  715 | `						&pClass->sName,pName,` |
|       2 |  716 | `						&apUsedTraits[k]->sName,pName);` |
|       3 |  717 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  718 | `						goto cleanup;` |
|       - |  719 | `					}` |
|       3 |  720 | `					break;` |
|       - |  721 | `				}` |
|     ! 0 |  722 | `			}` |
|       - |  723 | `			/* Class-defined method takes precedence */` |
|       8 |  724 | `			continue;` |
|       - |  725 | `		}` |
|  210145 |  726 | `		rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|  210145 |  727 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  728 | `			goto cleanup;` |
|       - |  729 | `		}` |
|       5 |  730 | `	}` |
|       - |  731 | `	/* Record trait in the class */` |
|   15653 |  732 | `	SySetPut(&pClass->aTrait,(const void *)&pTrait);` |
|    7824 |  733 | `cleanup:` |
|       - |  734 | `	/* Always clear visiting flag, even on error paths */` |
|   15653 |  735 | `	pTrait->iFlags &= ~PH7_CLASS_TRAIT_VISITING;` |
|    7824 |  736 | `	SXUNUSED(pGen);` |
|   15653 |  737 | `	return rc;` |
|    7829 |  738 | `}` |
|       - |  739 | `/*` |
|       - |  740 | ` * Inherit an object interface from another object interface.` |
|       - |  741 | ` * According to the PHP language reference manual.` |
|       - |  742 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|       - |  743 | ` *  must implement, without having to define how these methods are handled.` |
|       - |  744 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  745 | ` *  class, but without any of the methods having their contents defined.` |
|       - |  746 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  747 | ` *` |
|       - |  748 | ` * This function return SXRET_OK if the interface inheritance operation was successfully performed.` |
|       - |  749 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|       - |  750 | ` * error message.` |
|       - |  751 | ` */` |
|   27242 |  752 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceInherit(ph7_class *pSub,ph7_class *pBase)` |
|       5 |  753 | `{` |
|       - |  754 | `	ph7_class_method *pMeth;` |
|       - |  755 | `	ph7_class_attr *pAttr;` |
|       - |  756 | `	SyHashEntry *pEntry;` |
|       - |  757 | `	SyString *pName;` |
|       - |  758 | `	sxi32 rc;` |
|       - |  759 | `	/* Install in the derived hashtable */` |
|   27247 |  760 | `	SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|   27247 |  761 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
|       - |  762 | `	/* Copy constants */` |
|   40870 |  763 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|       - |  764 | `		/* Make sure the constants are not redeclared in the subclass */` |
|       3 |  765 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|       3 |  766 | `		pName = &pAttr->sName;` |
|       3 |  767 | `		if( SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte) == 0 ){` |
|       - |  768 | `			/* Install the constant in the subclass */` |
|       3 |  769 | `			rc = SyHashInsertTail(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|       3 |  770 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  771 | `				return rc;` |
|       - |  772 | `			}` |
|       1 |  773 | `		}` |
|       1 |  774 | `	}` |
|   27247 |  775 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
|       - |  776 | `	/* Copy methods signature */` |
|  107042 |  777 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|       - |  778 | `		/* Make sure the method are not redeclared in the subclass */` |
|   66179 |  779 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   66179 |  780 | `		pName = &pMeth->sFunc.sName;` |
|   66179 |  781 | `		if( SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte) == 0 ){` |
|       - |  782 | `			/* Install the method */` |
|   66179 |  783 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|   66179 |  784 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  785 | `				return rc;` |
|       - |  786 | `			}` |
|   33087 |  787 | `		}` |
|       5 |  788 | `	}` |
|       - |  789 | `	/* Mark as subclass */` |
|   27247 |  790 | `	pSub->pBase = pBase;` |
|       - |  791 | `	/* All done */` |
|   27247 |  792 | `	return SXRET_OK;` |
|   13626 |  793 | `}` |
|       - |  794 | `/*` |
|       - |  795 | ` * Implements an object interface in the given main class.` |
|       - |  796 | ` * According to the PHP language reference manual.` |
|       - |  797 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|       - |  798 | ` *  must implement, without having to define how these methods are handled.` |
|       - |  799 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  800 | ` *  class, but without any of the methods having their contents defined.` |
|       - |  801 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  802 | ` *` |
|       - |  803 | ` * This function return SXRET_OK if the interface was successfully implemented.` |
|       - |  804 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|       - |  805 | ` * error message.` |
|       - |  806 | ` */` |
|  416586 |  807 | `PH7_PRIVATE sxi32 PH7_ClassImplement(ph7_class *pMain,ph7_class *pInterface)` |
|       5 |  808 | `{` |
|       - |  809 | `	ph7_class_attr *pAttr;` |
|       - |  810 | `	SyHashEntry *pEntry;` |
|       - |  811 | `	SyString *pName;` |
|       - |  812 | `	sxi32 rc;` |
|       - |  813 | `	/* First off,copy all constants declared inside the interface */` |
|  416591 |  814 | `	SyHashResetLoopCursor(&pInterface->hAttr);` |
|  733818 |  815 | `	while((pEntry = SyHashGetNextEntry(&pInterface->hAttr)) != 0 ){` |
|       - |  816 | `		/* Point to the constant declaration */` |
|  108939 |  817 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  108939 |  818 | `		pName = &pAttr->sName;` |
|       - |  819 | `		/* Make sure the attribute is not redeclared in the main class */` |
|  108939 |  820 | `		if( SyHashGet(&pMain->hAttr,pName->zString,pName->nByte) == 0 ){` |
|       - |  821 | `			/* Install the attribute */` |
|  108935 |  822 | `			rc = SyHashInsertTail(&pMain->hAttr,pName->zString,pName->nByte,pAttr);` |
|  108935 |  823 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  824 | `				return rc;` |
|       - |  825 | `			}` |
|   54465 |  826 | `		}` |
|       5 |  827 | `	}` |
|       - |  828 | `	/* Install in the interface container */` |
|  416591 |  829 | `	SySetPut(&pMain->aInterface,(const void *)&pInterface);` |
|       - |  830 | `	/* Install interface method stubs into the implementing class.` |
|       - |  831 | `	 * Methods already defined in the class take precedence (they satisfy` |
|       - |  832 | `	 * the interface contract). Stubs retain PH7_CLASS_ATTR_ABSTRACT so` |
|       - |  833 | `	 * the unified check in GenStateCheckAbstractMethods catches missing ones.` |
|       - |  834 | `	 */` |
|       - |  835 | `	{` |
|       - |  836 | `		ph7_class_method *pMeth;` |
|       - |  837 | `		SyHashEntry *pMEntry;` |
|       - |  838 | `		SyString *pMName;` |
|  416591 |  839 | `		SyHashResetLoopCursor(&pInterface->hMethod);` |
| 1442392 |  840 | `		while((pMEntry = SyHashGetNextEntry(&pInterface->hMethod)) != 0 ){` |
|  817513 |  841 | `			pMeth = (ph7_class_method *)pMEntry->pUserData;` |
|  817513 |  842 | `			pMName = &pMeth->sFunc.sName;` |
|  817513 |  843 | `			if( SyHashGet(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte) == 0 ){` |
|      25 |  844 | `				rc = SyHashInsert(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte,pMeth);` |
|      25 |  845 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  846 | `					return rc;` |
|       - |  847 | `				}` |
|      10 |  848 | `			}` |
|       5 |  849 | `		}` |
|       - |  850 | `	}` |
|  416591 |  851 | `	return SXRET_OK;` |
|  208298 |  852 | `}` |
|       - |  853 | `/*` |
|       - |  854 | ` * Create a class instance [i.e: Object in the PHP jargon] at run-time.` |
|       - |  855 | ` * The following function is called when an object is created at run-time` |
|       - |  856 | ` * typically when the PH7_OP_NEW/PH7_OP_CLONE instructions are executed.` |
|       - |  857 | ` * Notes on object creation.` |
|       - |  858 | ` *` |
|       - |  859 | ` * According to PHP language reference manual.` |
|       - |  860 | ` * To create an instance of a class, the new keyword must be used. An object will always` |
|       - |  861 | ` * be created unless the object has a constructor defined that throws an exception on error.` |
|       - |  862 | ` * Classes should be defined before instantiation (and in some cases this is a requirement).` |
|       - |  863 | ` * If a string containing the name of a class is used with new, a new instance of that class` |
|       - |  864 | ` * will be created. If the class is in a namespace, its fully qualified name must be used when` |
|       - |  865 | ` * doing this.` |
|       - |  866 | ` * Example #3 Creating an instance` |
|       - |  867 | ` * <?php` |
|       - |  868 | ` *  $instance = new SimpleClass();` |
|       - |  869 | ` *   // This can also be done with a variable:` |
|       - |  870 | ` * $className = 'Foo';` |
|       - |  871 | ` * $instance = new $className(); // Foo()` |
|       - |  872 | ` * ?>` |
|       - |  873 | ` * In the class context, it is possible to create a new object by new self and new parent.` |
|       - |  874 | ` * When assigning an already created instance of a class to a new variable, the new variable` |
|       - |  875 | ` * will access the same instance as the object that was assigned. This behaviour is the same` |
|       - |  876 | ` * when passing instances to a function. A copy of an already created object can be made by` |
|       - |  877 | ` * cloning it.` |
|       - |  878 | ` * Example #4 Object Assignment` |
|       - |  879 | ` * <?php` |
|       - |  880 | ` *  class SimpleClass(){` |
|       - |  881 | ` *    public $var;` |
|       - |  882 | ` *  };` |
|       - |  883 | ` *  $instance = new SimpleClass();` |
|       - |  884 | ` *  $assigned   =  $instance;` |
|       - |  885 | ` *  $reference  =& $instance;` |
|       - |  886 | ` *  $instance->var = '$assigned will have this value';` |
|       - |  887 | ` *  $instance = null; // $instance and $reference become null` |
|       - |  888 | ` *  var_dump($instance);` |
|       - |  889 | ` *  var_dump($reference);` |
|       - |  890 | ` *  var_dump($assigned);` |
|       - |  891 | ` * ?>` |
|       - |  892 | ` * The above example will output:` |
|       - |  893 | ` * NULL` |
|       - |  894 | ` * NULL` |
|       - |  895 | ` * object(SimpleClass)#1 (1) {` |
|       - |  896 | ` *  ["var"]=>` |
|       - |  897 | ` *    string(30) "$assigned will have this value"` |
|       - |  898 | ` * }` |
|       - |  899 | ` * Example #5 Creating new objects` |
|       - |  900 | ` * <?php` |
|       - |  901 | ` * class Test` |
|       - |  902 | ` * {` |
|       - |  903 | ` *   static public function getNew()` |
|       - |  904 | ` *   {` |
|       - |  905 | ` *       return new static;` |
|       - |  906 | ` *   }` |
|       - |  907 | ` * }` |
|       - |  908 | ` * class Child extends Test` |
|       - |  909 | ` * {}` |
|       - |  910 | ` * $obj1 = new Test();` |
|       - |  911 | ` * $obj2 = new $obj1;` |
|       - |  912 | ` * var_dump($obj1 !== $obj2);` |
|       - |  913 | ` * $obj3 = Test::getNew();` |
|       - |  914 | ` * var_dump($obj3 instanceof Test);` |
|       - |  915 | ` * $obj4 = Child::getNew();` |
|       - |  916 | ` * var_dump($obj4 instanceof Child);` |
|       - |  917 | ` * ?>` |
|       - |  918 | ` * The above example will output:` |
|       - |  919 | ` * bool(true)` |
|       - |  920 | ` * bool(true)` |
|       - |  921 | ` * bool(true)` |
|       - |  922 | ` * Note that Symisc Systems have introduced powerfull extension to` |
|       - |  923 | ` * OO subsystem. For example a class attribute may have any complex` |
|       - |  924 | ` * expression associated with it when declaring the attribute unlike` |
|       - |  925 | ` * the standard PHP engine which would allow a single value.` |
|       - |  926 | ` * Example:` |
|       - |  927 | ` *  class myClass{` |
|       - |  928 | ` *    public $var = 25<<1+foo()/bar();` |
|       - |  929 | ` *  };` |
|       - |  930 | ` * Refer to the official documentation for more information.` |
|       - |  931 | ` */` |
|    8682 |  932 | `static ph7_class_instance * NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|       5 |  933 | `{` |
|       - |  934 | `	ph7_class_instance *pThis;` |
|       - |  935 | `	/* Allocate a new instance */` |
|    8687 |  936 | `	pThis = (ph7_class_instance *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_instance));` |
|    8687 |  937 | `	if( pThis == 0 ){` |
|     ! 0 |  938 | `		return 0;` |
|       - |  939 | `	}` |
|       - |  940 | `	/* Zero the structure */` |
|    8687 |  941 | `	SyZero(pThis,sizeof(ph7_class_instance));` |
|       - |  942 | `	/* Initialize fields */` |
|    8687 |  943 | `	pThis->iRef = 1;` |
|    8687 |  944 | `	pThis->pVm = pVm;` |
|    8687 |  945 | `	pThis->pClass = pClass;` |
|       - |  946 | `	/* Assign a fresh monotonic object handle id (clones get their own, like PHP). */` |
|    8687 |  947 | `	pThis->nObjId = pVm->nNextObjId++;` |
|    8687 |  948 | `	SyHashInit(&pThis->hAttr,&pVm->sAllocator,0,0);` |
|    8687 |  949 | `	return pThis;` |
|    4346 |  950 | `}` |
|       - |  951 | `/*` |
|       - |  952 | ` * Wrapper around the NewClassInstance() function defined above.` |
|       - |  953 | ` * See the block comment above for more information.` |
|       - |  954 | ` */` |
|    8436 |  955 | `PH7_PRIVATE ph7_class_instance * PH7_NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|       5 |  956 | `{` |
|       - |  957 | `	ph7_class_instance *pNew;` |
|       - |  958 | `	sxi32 rc;` |
|    8441 |  959 | `	pNew = NewClassInstance(&(*pVm),&(*pClass));` |
|    8441 |  960 | `	if( pNew == 0 ){` |
|     ! 0 |  961 | `		return 0;` |
|       - |  962 | `	}` |
|       - |  963 | `	/* Associate a private VM frame with this class instance */` |
|    8441 |  964 | `	rc = PH7_VmCreateClassInstanceFrame(&(*pVm),pNew);` |
|    8441 |  965 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  966 | `		SyMemBackendPoolFree(&pVm->sAllocator,pNew);` |
|     ! 0 |  967 | `		return 0;` |
|       - |  968 | `	}` |
|       - |  969 | `	/* php stamps a Throwable's file/line at CREATION, not in its constructor, so a` |
|       - |  970 | `	 * subclass that overrides __construct without calling parent::__construct still` |
|       - |  971 | `	 * reports the right site. Every instantiation path lands here. */` |
|    8441 |  972 | `	PH7_VmStampThrowableSite(&(*pVm),pNew);` |
|    8441 |  973 | `	return pNew;` |
|    4223 |  974 | `}` |
|       - |  975 | `/*` |
|       - |  976 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon] attribute.` |
|       - |  977 | ` * This function never fail.` |
|       - |  978 | ` */` |
|   24232 |  979 | `static ph7_value * ExtractClassAttrValue(ph7_vm *pVm,VmClassAttr *pAttr)` |
|       5 |  980 | `{` |
|       - |  981 | `	/* Extract the value */` |
|       - |  982 | `	ph7_value *pValue;` |
|   24237 |  983 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|   24237 |  984 | `	return pValue;` |
|       5 |  985 | `}` |
|       - |  986 | `/*` |
|       - |  987 | ` * Perform a clone operation on a class instance [i.e: Object in the PHP jargon].` |
|       - |  988 | ` * The following function is called when an object is cloned at run-time` |
|       - |  989 | ` * typically when the PH7_OP_CLONE instruction is executed.` |
|       - |  990 | ` * Notes on object cloning.` |
|       - |  991 | ` *` |
|       - |  992 | ` * According to PHP language reference manual.` |
|       - |  993 | ` * Creating a copy of an object with fully replicated properties is not always the wanted behavior.` |
|       - |  994 | ` * A good example of the need for copy constructors. Another example is if your object holds a reference` |
|       - |  995 | ` * to another object which it uses and when you replicate the parent object you want to create` |
|       - |  996 | ` * a new instance of this other object so that the replica has its own separate copy.` |
|       - |  997 | ` * An object copy is created by using the clone keyword (which calls the object's __clone() method if possible).` |
|       - |  998 | ` * An object's __clone() method cannot be called directly.` |
|       - |  999 | ` * $copy_of_object = clone $object;` |
|       - | 1000 | ` * When an object is cloned, PHP 5 will perform a shallow copy of all of the object's properties.` |
|       - | 1001 | ` * Any properties that are references to other variables, will remain references.` |
|       - | 1002 | ` * Once the cloning is complete, if a __clone() method is defined, then the newly created object's __clone() method` |
|       - | 1003 | ` * will be called, to allow any necessary properties that need to be changed.` |
|       - | 1004 | ` * Example #1 Cloning an object` |
|       - | 1005 | ` * <?php` |
|       - | 1006 | ` * class SubObject` |
|       - | 1007 | ` * {` |
|       - | 1008 | ` *   static $instances = 0;` |
|       - | 1009 | ` *   public $instance;` |
|       - | 1010 | ` *` |
|       - | 1011 | ` *   public function __construct() {` |
|       - | 1012 | ` *       $this->instance = ++self::$instances;` |
|       - | 1013 | ` *   }` |
|       - | 1014 | ` *` |
|       - | 1015 | ` *   public function __clone() {` |
|       - | 1016 | ` *       $this->instance = ++self::$instances;` |
|       - | 1017 | ` *   }` |
|       - | 1018 | ` * }` |
|       - | 1019 | ` *` |
|       - | 1020 | ` * class MyCloneable` |
|       - | 1021 | ` * {` |
|       - | 1022 | ` *   public $object1;` |
|       - | 1023 | ` *   public $object2;` |
|       - | 1024 | ` *` |
|       - | 1025 | ` *   function __clone()` |
|       - | 1026 | ` *   {` |
|       - | 1027 | ` *       // Force a copy of this->object, otherwise` |
|       - | 1028 | ` *       // it will point to same object.` |
|       - | 1029 | ` *       $this->object1 = clone $this->object1;` |
|       - | 1030 | ` *   }` |
|       - | 1031 | ` * }` |
|       - | 1032 | ` * $obj = new MyCloneable();` |
|       - | 1033 | ` * $obj->object1 = new SubObject();` |
|       - | 1034 | ` * $obj->object2 = new SubObject();` |
|       - | 1035 | ` * $obj2 = clone $obj;` |
|       - | 1036 | ` * print("Original Object:\n");` |
|       - | 1037 | ` * print_r($obj);` |
|       - | 1038 | ` * print("Cloned Object:\n");` |
|       - | 1039 | ` * print_r($obj2);` |
|       - | 1040 | ` * ?>` |
|       - | 1041 | ` * The above example will output:` |
|       - | 1042 | ` * Original Object:` |
|       - | 1043 | ` * MyCloneable Object` |
|       - | 1044 | ` * (` |
|       - | 1045 | ` *   [object1] => SubObject Object` |
|       - | 1046 | ` *       (` |
|       - | 1047 | ` *           [instance] => 1` |
|       - | 1048 | ` *       )` |
|       - | 1049 | ` *` |
|       - | 1050 | ` *   [object2] => SubObject Object` |
|       - | 1051 | ` *       (` |
|       - | 1052 | ` *           [instance] => 2` |
|       - | 1053 | ` *       )` |
|       - | 1054 | ` *` |
|       - | 1055 | ` * )` |
|       - | 1056 | ` * Cloned Object:` |
|       - | 1057 | ` * MyCloneable Object` |
|       - | 1058 | ` * (` |
|       - | 1059 | ` *   [object1] => SubObject Object` |
|       - | 1060 | ` *       (` |
|       - | 1061 | ` *           [instance] => 3` |
|       - | 1062 | ` *       )` |
|       - | 1063 | ` *` |
|       - | 1064 | ` *   [object2] => SubObject Object` |
|       - | 1065 | ` *       (` |
|       - | 1066 | ` *           [instance] => 2` |
|       - | 1067 | ` *       )` |
|       - | 1068 | ` * )` |
|       - | 1069 | ` */` |
|     246 | 1070 | `PH7_PRIVATE ph7_class_instance * PH7_CloneClassInstance(ph7_class_instance *pSrc)` |
|       4 | 1071 | `{` |
|       - | 1072 | `	ph7_class_instance *pClone;` |
|       - | 1073 | `	ph7_class_method *pMethod;` |
|       - | 1074 | `	SyHashEntry *pEntry2;` |
|       - | 1075 | `	SyHashEntry *pEntry;` |
|       - | 1076 | `	ph7_vm *pVm;` |
|       - | 1077 | `	sxi32 rc;` |
|       - | 1078 | `	/* Allocate a new instance */` |
|     250 | 1079 | `	pVm = pSrc->pVm;` |
|     250 | 1080 | `	pClone = NewClassInstance(pVm,pSrc->pClass);` |
|     250 | 1081 | `	if( pClone == 0 ){` |
|     ! 0 | 1082 | `		return 0;` |
|       - | 1083 | `	}` |
|       - | 1084 | `	/* Associate a private VM frame with this class instance */` |
|     250 | 1085 | `	rc = PH7_VmCreateClassInstanceFrame(pVm,pClone);` |
|     250 | 1086 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1087 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClone);` |
|     ! 0 | 1088 | `		return 0;` |
|       - | 1089 | `	}` |
|       - | 1090 | `	/* Duplicate object values. Iterate the SOURCE attributes and copy each into` |
|       - | 1091 | `	 * the clone's same-named slot (looked up by name, so order/count differences` |
|       - | 1092 | `	 * from dynamic properties don't matter). A dynamic (runtime-added) property` |
|       - | 1093 | `	 * has no declared counterpart in the clone, so synthesize it first — without` |
|       - | 1094 | `	 * this, a clone of a stdClass would silently lose all its dynamic properties. */` |
|     250 | 1095 | `	SyHashResetLoopCursor(&pSrc->hAttr);` |
|    2642 | 1096 | `	while((pEntry = SyHashGetNextEntry(&pSrc->hAttr)) != 0 ){` |
|    2396 | 1097 | `		VmClassAttr *pSrcAttr = (VmClassAttr *)pEntry->pUserData;` |
|    2396 | 1098 | `		VmClassAttr *pDestAttr = 0;` |
|    2396 | 1099 | `		ph7_value *pvSrc,*pvDest = 0;` |
|       - | 1100 | `		/* Duplicate non-static attribute */` |
|    2396 | 1101 | `		if( pSrcAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|    1695 | 1102 | `			continue;` |
|       - | 1103 | `		}` |
|     702 | 1104 | `		pEntry2 = SyHashGet(&pClone->hAttr,SyStringData(&pSrcAttr->pAttr->sName),SyStringLength(&pSrcAttr->pAttr->sName));` |
|     702 | 1105 | `		if( pEntry2 ){` |
|     688 | 1106 | `			pDestAttr = (VmClassAttr *)pEntry2->pUserData;` |
|     688 | 1107 | `			pvDest = ExtractClassAttrValue(pVm,pDestAttr);` |
|     357 | 1108 | `		}else if( pSrcAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){` |
|       - | 1109 | `			/* Dynamic property: synthesize the matching slot on the clone. */` |
|      22 | 1110 | `			pvDest = PH7_VmCreateDynamicAttr(pVm,pClone,` |
|      14 | 1111 | `				SyStringData(&pSrcAttr->pAttr->sName),SyStringLength(&pSrcAttr->pAttr->sName),&pDestAttr);` |
|       7 | 1112 | `		}` |
|       - | 1113 | `		/* Fetch the source value LAST: PH7_VmCreateDynamicAttr above may have` |
|       - | 1114 | `		 * reserved a slot and reallocated pVm->aMemObj, which would dangle any` |
|       - | 1115 | `		 * ph7_value* obtained before it. pvDest from the synth path already points` |
|       - | 1116 | `		 * into the post-realloc aMemObj; resolve pvSrc now so both are current. */` |
|     702 | 1117 | `		pvSrc = ExtractClassAttrValue(pVm,pSrcAttr);` |
|     702 | 1118 | `		if( (pSrcAttr->iState & VM_CLASS_ATTR_REFBOUND) && pDestAttr ){` |
|       - | 1119 | `			/* php preserves references across clone: the clone shares the SAME slot` |
|       - | 1120 | `			 * as the source property (both alias the referenced variable), rather` |
|       - | 1121 | `			 * than getting an independent value copy. Drop the clone's fresh private` |
|       - | 1122 | `			 * slot and repoint at the (already-pinned) shared slot. The REFBOUND flag` |
|       - | 1123 | `			 * is carried over by the iState copy below, so the clone's release also` |
|       - | 1124 | `			 * leaves the shared slot alone. */` |
|       3 | 1125 | `			if( pDestAttr->nIdx != pSrcAttr->nIdx ){` |
|       3 | 1126 | `				if( pDestAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|     ! 0 | 1127 | `					SyHashDeleteEntry(&pVm->hTypedSlot,(const void *)&pDestAttr->nIdx,sizeof(sxu32),0);` |
|     ! 0 | 1128 | `				}` |
|       3 | 1129 | `				PH7_VmUnsetMemObj(pVm,pDestAttr->nIdx,TRUE);` |
|       3 | 1130 | `				pDestAttr->nIdx = pSrcAttr->nIdx;` |
|       2 | 1131 | `			}` |
|     701 | 1132 | `		}else if( pvSrc && pvDest ){` |
|     700 | 1133 | `			PH7_MemObjStore(pvSrc,pvDest);` |
|     348 | 1134 | `		}` |
|       - | 1135 | `		/* Carry over the per-instance state so the clone matches the source:` |
|       - | 1136 | `		 * VM_CLASS_ATTR_UNINIT marks a typed property as not-yet-initialized` |
|       - | 1137 | `		 * and doubles as the readonly write-once latch — without this a clone` |
|       - | 1138 | `		 * would reset to uninitialized (losing the value's readiness) and a` |
|       - | 1139 | `		 * readonly property would become writable again. */` |
|     702 | 1140 | `		if( pDestAttr ){` |
|     702 | 1141 | `			pDestAttr->iState = pSrcAttr->iState;` |
|     349 | 1142 | `		}` |
|       4 | 1143 | `	}` |
|       - | 1144 | `	/* A declared property unset() on the source is absent from the clone too (PHP). But the clone` |
|       - | 1145 | `	 * frame above materialized ALL declared attrs (with their defaults), so drop any clone attr whose` |
|       - | 1146 | `	 * name is not present on the source. Collect first, then delete — removing an entry mid-walk would` |
|       - | 1147 | `	 * free the node the SyHash loop cursor points at. */` |
|       - | 1148 | `	{` |
|       - | 1149 | `		SySet sDrop;` |
|     250 | 1150 | `		SySetInit(&sDrop,&pVm->sAllocator,sizeof(VmClassAttr *));` |
|     250 | 1151 | `		SyHashResetLoopCursor(&pClone->hAttr);` |
|    2644 | 1152 | `		while((pEntry = SyHashGetNextEntry(&pClone->hAttr)) != 0 ){` |
|    2398 | 1153 | `			VmClassAttr *pCloneAttr = (VmClassAttr *)pEntry->pUserData;` |
|    2398 | 1154 | `			if( pCloneAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|    1695 | 1155 | `				continue;` |
|       - | 1156 | `			}` |
|    1050 | 1157 | `			if( SyHashGet(&pSrc->hAttr,SyStringData(&pCloneAttr->pAttr->sName),` |
|    1054 | 1158 | `					SyStringLength(&pCloneAttr->pAttr->sName)) == 0 ){` |
|       3 | 1159 | `				SySetPut(&sDrop,(const void *)&pCloneAttr);` |
|       1 | 1160 | `			}` |
|       4 | 1161 | `		}` |
|     250 | 1162 | `		if( SySetUsed(&sDrop) > 0 ){` |
|       3 | 1163 | `			VmClassAttr **apDrop = (VmClassAttr **)SySetBasePtr(&sDrop);` |
|       - | 1164 | `			sxu32 i;` |
|       5 | 1165 | `			for( i = 0 ; i < SySetUsed(&sDrop) ; ++i ){` |
|       3 | 1166 | `				VmClassAttr *pVmAttr = apDrop[i];` |
|       4 | 1167 | `				SyHashDeleteEntry(&pClone->hAttr,SyStringData(&pVmAttr->pAttr->sName),` |
|       2 | 1168 | `					SyStringLength(&pVmAttr->pAttr->sName),0);` |
|       3 | 1169 | `				PH7_VmReleaseInstanceAttr(pVm,pVmAttr);` |
|       2 | 1170 | `			}` |
|       1 | 1171 | `		}` |
|     250 | 1172 | `		SySetRelease(&sDrop);` |
|       - | 1173 | `	}` |
|       - | 1174 | `	/* call the __clone method on the cloned object if available */` |
|     250 | 1175 | `	pMethod = PH7_ClassExtractMethod(pClone->pClass,"__clone",sizeof("__clone")-1);` |
|     250 | 1176 | `	if( pMethod ){` |
|      60 | 1177 | `		if( pMethod->iCloneDepth < 16 ){` |
|      58 | 1178 | `			pMethod->iCloneDepth++;` |
|       - | 1179 | `			/* PHP 8.3: __clone() may re-initialize the clone's readonly` |
|       - | 1180 | `			 * properties. Flag the instance so the readonly store guard allows` |
|       - | 1181 | `			 * it for the duration of the call. */` |
|      58 | 1182 | `			pClone->iFlags \|= VM_INSTANCE_CLONING;` |
|      58 | 1183 | `			PH7_VmCallClassMethod(pVm,pClone,pMethod,0,0,0);` |
|      58 | 1184 | `			pClone->iFlags &= ~VM_INSTANCE_CLONING;` |
|      30 | 1185 | `		}else{` |
|       - | 1186 | `			/* Nesting limit reached */` |
|       3 | 1187 | `			PH7_VmThrowError(pVm,0,PH7_CTX_ERR,"Object clone limit reached,no more call to __clone()");` |
|       - | 1188 | `		}` |
|       - | 1189 | `		/* Reset the cursor */` |
|      60 | 1190 | `		pMethod->iCloneDepth = 0;` |
|      29 | 1191 | `	}` |
|       - | 1192 | `	/* Return the cloned object */` |
|     250 | 1193 | `	return pClone;` |
|     127 | 1194 | `}` |
|       - | 1195 | `#define CLASS_INSTANCE_DESTROYED 0x001 /* Instance is released */` |
|       - | 1196 | `/*` |
|       - | 1197 | ` * Free the per-instance allocations owned by ONE object attribute: its value slot (+ the typed-slot` |
|       - | 1198 | ` * enforcement entry), the synthesized ph7_class_attr for a dynamic (runtime-added) property, and the` |
|       - | 1199 | ` * VmClassAttr wrapper itself. Does NOT touch the hAttr entry node — the caller removes it` |
|       - | 1200 | `` * (`unset($o->p)` via SyHashDeleteEntry2; instance teardown via the wholesale SyHashRelease, so it must`` |
|       - | 1201 | ` * not delete entries mid-walk). Shared by PH7_ClassInstanceRelease and the OP_MEMBER unset path.` |
|       - | 1202 | ` */` |
|   28912 | 1203 | `PH7_PRIVATE void PH7_VmReleaseInstanceAttr(ph7_vm *pVm, VmClassAttr *pVmAttr)` |
|       5 | 1204 | `{` |
|   28917 | 1205 | `	if( pVmAttr->iState & VM_CLASS_ATTR_REFBOUND ){` |
|       - | 1206 | ``		/* Reference-bound property (`$o->p =& $x`): its value slot is SHARED with`` |
|       - | 1207 | `		 * (and pinned by) the source variable — releasing/recycling it here would` |
|       - | 1208 | `		 * dangle the surviving alias. Leave the slot alone (script-lifetime pin,` |
|       - | 1209 | `		 * matching the use(&$x) capture tradeoff); just free the bookkeeping below. */` |
|   28912 | 1210 | `	}else if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|       - | 1211 | `		/* Drop any typed-property enforcement slot registered for this memobj, before the memobj` |
|       - | 1212 | `		 * is returned to the free list, so a future recycled slot does not inherit the stale entry. */` |
|   21611 | 1213 | `		if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|     309 | 1214 | `			SyHashDeleteEntry(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),0);` |
|     152 | 1215 | `		}` |
|   21611 | 1216 | `		PH7_VmUnsetMemObj(pVm,pVmAttr->nIdx,TRUE);` |
|   10803 | 1217 | `	}` |
|       - | 1218 | `	/* A dynamic property owns its synthesized ph7_class_attr (struct + inline name in one block) —` |
|       - | 1219 | `	 * free it here (the only place a per-instance pAttr is freed; declared attrs are class-owned). */` |
|   28917 | 1220 | `	if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){` |
|     116 | 1221 | `		SyMemBackendFree(&pVm->sAllocator,pVmAttr->pAttr);` |
|      57 | 1222 | `	}` |
|   28917 | 1223 | `	SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|   28917 | 1224 | `}` |
|       - | 1225 | `/*` |
|       - | 1226 | ` * Release a class instance [i.e: Object in the PHP jargon] and invoke any defined destructor.` |
|       - | 1227 | ` * This routine is invoked as soon as there are no other references to a particular` |
|       - | 1228 | ` * class instance.` |
|       - | 1229 | ` */` |
|    5774 | 1230 | `static void PH7_ClassInstanceRelease(ph7_class_instance *pThis)` |
|       5 | 1231 | `{` |
|       - | 1232 | `	ph7_class_method *pDestr;` |
|       - | 1233 | `	SyHashEntry *pEntry;` |
|       - | 1234 | `	ph7_class *pClass;` |
|       - | 1235 | `	ph7_vm *pVm;` |
|    5779 | 1236 | `	if( pThis->iFlags & CLASS_INSTANCE_DESTROYED ){` |
|       - | 1237 | `		/*` |
|       - | 1238 | `		 * Already destroyed,return immediately.` |
|       - | 1239 | `		 * This could happend if someone perform unset($this) in the destructor body.` |
|       - | 1240 | `		 */` |
|     ! 0 | 1241 | `		return;` |
|       - | 1242 | `	}` |
|       - | 1243 | `	/* Mark as destroyed */` |
|    5779 | 1244 | `	pThis->iFlags \|= CLASS_INSTANCE_DESTROYED;` |
|       - | 1245 | `	/* Invoke any defined destructor if available */` |
|    5779 | 1246 | `	pVm = pThis->pVm;` |
|    5779 | 1247 | `	pClass = pThis->pClass;` |
|    5779 | 1248 | `	pDestr = PH7_ClassExtractMethod(pClass,"__destruct",sizeof("__destruct")-1);` |
|    5779 | 1249 | `	if( pDestr && !pVm->bInReset ){` |
|       - | 1250 | `		/* Invoke the destructor. Skipped during ph7_vm_reset() bulk teardown:` |
|       - | 1251 | `		 * running user PHP against a half-reset VM is unsafe (see bInReset). */` |
|     473 | 1252 | `		pThis->iRef = 2; /* Prevent garbage collection */` |
|     473 | 1253 | `		PH7_VmCallClassMethod(pVm,pThis,pDestr,0,0,0);` |
|     234 | 1254 | `	}` |
|       - | 1255 | `	/* Weak-reference registry: kill the cell for this instance so every` |
|       - | 1256 | `	 * WeakReference/WeakMap handle observes the death (the cell outlives the` |
|       - | 1257 | `	 * instance until its own handles drop; removing the hash entry here keeps` |
|       - | 1258 | `	 * a pool-reused address from resurrecting a dead cell). */` |
|    5779 | 1259 | `	if( SyHashTotalEntry(&pVm->hWeakCell) > 0 ){` |
|      27 | 1260 | `		void *pCellData = 0;` |
|      26 | 1261 | `		if( SyHashDeleteEntry(&pVm->hWeakCell,(const void *)&pThis,sizeof(void *),&pCellData) == SXRET_OK` |
|      18 | 1262 | `		 && pCellData ){` |
|       9 | 1263 | `			((VmWeakCell *)pCellData)->pObj = 0;` |
|       4 | 1264 | `		}` |
|      13 | 1265 | `	}` |
|       - | 1266 | `	/* Release non-static attributes (the wholesale SyHashRelease below frees the entry nodes,` |
|       - | 1267 | `	 * so the helper must not delete them mid-walk). */` |
|    5779 | 1268 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|   34663 | 1269 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|   28889 | 1270 | `		PH7_VmReleaseInstanceAttr(pVm,(VmClassAttr *)pEntry->pUserData);` |
|       5 | 1271 | `	}` |
|       - | 1272 | `	/* Release the whole structure */` |
|    5779 | 1273 | `	SyHashRelease(&pThis->hAttr);` |
|    5779 | 1274 | `	SyMemBackendPoolFree(&pVm->sAllocator,pThis);` |
|    2892 | 1275 | `}` |
|       - | 1276 | `/*` |
|       - | 1277 | ` * Decrement the reference count of a class instance [i.e Object in the PHP jargon].` |
|       - | 1278 | ` * If the reference count reaches zero,release the whole instance.` |
|       - | 1279 | ` */` |
|  146932 | 1280 | `PH7_PRIVATE void PH7_ClassInstanceUnref(ph7_class_instance *pThis)` |
|       5 | 1281 | `{` |
|  146937 | 1282 | `	pThis->iRef--;` |
|  146937 | 1283 | `	if( pThis->iRef < 1 ){` |
|       - | 1284 | `		/* No more reference to this instance */` |
|    5779 | 1285 | `		PH7_ClassInstanceRelease(&(*pThis));` |
|    2887 | 1286 | `	}` |
|  146937 | 1287 | `}` |
|       - | 1288 | `/*` |
|       - | 1289 | ` * Compare two class instances [i.e: Objects in the PHP jargon]` |
|       - | 1290 | ` * Note on objects comparison:` |
|       - | 1291 | ` *  According to the PHP langauge reference manual` |
|       - | 1292 | ` *  When using the comparison operator (==), object variables are compared in a simple manner` |
|       - | 1293 | ` *  namely: Two object instances are equal if they have the same attributes and values, and are` |
|       - | 1294 | ` *  instances of the same class.` |
|       - | 1295 | ` *  On the other hand, when using the identity operator (===), object variables are identical` |
|       - | 1296 | ` *  if and only if they refer to the same instance of the same class.` |
|       - | 1297 | ` *  An example will clarify these rules.` |
|       - | 1298 | ` *  Example #1 Example of object comparison` |
|       - | 1299 | ` *  <?php` |
|       - | 1300 | ` *    function bool2str($bool)` |
|       - | 1301 | ` * {` |
|       - | 1302 | ` *   if ($bool === false) {` |
|       - | 1303 | ` *       return 'FALSE';` |
|       - | 1304 | ` *   } else {` |
|       - | 1305 | ` *       return 'TRUE';` |
|       - | 1306 | ` *   }` |
|       - | 1307 | ` * }` |
|       - | 1308 | ` * function compareObjects(&$o1, &$o2)` |
|       - | 1309 | ` * {` |
|       - | 1310 | ` *   echo 'o1 == o2 : ' . bool2str($o1 == $o2) . "\n";` |
|       - | 1311 | ` *   echo 'o1 != o2 : ' . bool2str($o1 != $o2) . "\n";` |
|       - | 1312 | ` *   echo 'o1 === o2 : ' . bool2str($o1 === $o2) . "\n";` |
|       - | 1313 | ` *   echo 'o1 !== o2 : ' . bool2str($o1 !== $o2) . "\n";` |
|       - | 1314 | ` * }` |
|       - | 1315 | ` * class Flag` |
|       - | 1316 | ` * {` |
|       - | 1317 | ` *   public $flag;` |
|       - | 1318 | ` *` |
|       - | 1319 | ` *   function Flag($flag = true) {` |
|       - | 1320 | ` *       $this->flag = $flag;` |
|       - | 1321 | ` *   }` |
|       - | 1322 | ` * }` |
|       - | 1323 | ` *` |
|       - | 1324 | ` * class OtherFlag` |
|       - | 1325 | ` * {` |
|       - | 1326 | ` *   public $flag;` |
|       - | 1327 | ` *` |
|       - | 1328 | ` *   function OtherFlag($flag = true) {` |
|       - | 1329 | ` *       $this->flag = $flag;` |
|       - | 1330 | ` *   }` |
|       - | 1331 | ` * }` |
|       - | 1332 | ` *` |
|       - | 1333 | ` * $o = new Flag();` |
|       - | 1334 | ` * $p = new Flag();` |
|       - | 1335 | ` * $q = $o;` |
|       - | 1336 | ` * $r = new OtherFlag();` |
|       - | 1337 | ` *` |
|       - | 1338 | ` * echo "Two instances of the same class\n";` |
|       - | 1339 | ` * compareObjects($o, $p);` |
|       - | 1340 | ` * echo "\nTwo references to the same instance\n";` |
|       - | 1341 | ` * compareObjects($o, $q);` |
|       - | 1342 | ` * echo "\nInstances of two different classes\n";` |
|       - | 1343 | ` * compareObjects($o, $r);` |
|       - | 1344 | ` * ?>` |
|       - | 1345 | ` * The above example will output:` |
|       - | 1346 | ` * Two instances of the same class` |
|       - | 1347 | ` * o1 == o2 : TRUE` |
|       - | 1348 | ` * o1 != o2 : FALSE` |
|       - | 1349 | ` * o1 === o2 : FALSE` |
|       - | 1350 | ` * o1 !== o2 : TRUE` |
|       - | 1351 | ` * Two references to the same instance` |
|       - | 1352 | ` * o1 == o2 : TRUE` |
|       - | 1353 | ` * o1 != o2 : FALSE` |
|       - | 1354 | ` * o1 === o2 : TRUE` |
|       - | 1355 | ` * o1 !== o2 : FALSE` |
|       - | 1356 | ` * Instances of two different classes` |
|       - | 1357 | ` * o1 == o2 : FALSE` |
|       - | 1358 | ` * o1 != o2 : TRUE` |
|       - | 1359 | ` * o1 === o2 : FALSE` |
|       - | 1360 | ` * o1 !== o2 : TRUE` |
|       - | 1361 | ` *` |
|       - | 1362 | ` * This function return 0 if the objects are equals according to the comprison rules defined above.` |
|       - | 1363 | ` * Any other return values indicates difference.` |
|       - | 1364 | ` */` |
|     290 | 1365 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest)` |
|       5 | 1366 | `{` |
|       - | 1367 | `	SyHashEntry *pEntry,*pEntry2;` |
|       - | 1368 | `	ph7_value sV1,sV2;` |
|       - | 1369 | `	sxi32 rc;` |
|     295 | 1370 | `	if( iNest > 31 ){` |
|       - | 1371 | `		/* Nesting limit reached */` |
|       6 | 1372 | `		PH7_VmThrowError(pLeft->pVm,0,PH7_CTX_ERR,"Nesting limit reached: Infinite recursion?");` |
|       6 | 1373 | `		return 1;` |
|       - | 1374 | `	}` |
|       - | 1375 | `	/* Comparison is performed only if the objects are instance of the same class */` |
|     291 | 1376 | `	if( pLeft->pClass != pRight->pClass ){` |
|       7 | 1377 | `		return 1;` |
|       - | 1378 | `	}` |
|     285 | 1379 | `	if( bStrict ){` |
|       - | 1380 | `		/*` |
|       - | 1381 | `		 * According to the PHP language reference manual:` |
|       - | 1382 | `		 *  when using the identity operator (===), object variables` |
|       - | 1383 | `		 *  are identical if and only if they refer to the same instance` |
|       - | 1384 | `		 *  of the same class.` |
|       - | 1385 | `		 */` |
|     121 | 1386 | `		return !(pLeft == pRight);` |
|       - | 1387 | `	}` |
|       - | 1388 | `	/*` |
|       - | 1389 | `	 * Attribute comparison.` |
|       - | 1390 | `	 * According to the PHP reference manual:` |
|       - | 1391 | `	 *  When using the comparison operator (==), object variables are compared` |
|       - | 1392 | `	 *  in a simple manner, namely: Two object instances are equal if they have` |
|       - | 1393 | `	 *  the same attributes and values, and are instances of the same class.` |
|       - | 1394 | `	 */` |
|     168 | 1395 | `	if( pLeft == pRight ){` |
|       - | 1396 | `		/* Same instance,don't bother processing,object are equals */` |
|       5 | 1397 | `		return 0;` |
|       - | 1398 | `	}` |
|       - | 1399 | `	/* Closures compare by IDENTITY under == as well (not by attributes): two distinct` |
|       - | 1400 | `	 * Closure instances are never equal, even when they wrap the same underlying function` |
|       - | 1401 | `	 * (PHP semantics). pLeft != pRight here, so a Closure pair is unequal. Without this,` |
|       - | 1402 | `` 	 * two capture-less lambdas of the same `function(){}` share the template's `$__fn` `` |
|       - | 1403 | `	 * name and would compare equal. */` |
|     164 | 1404 | `	if( pLeft->pVm->pClosureClass && pLeft->pClass == pLeft->pVm->pClosureClass ){` |
|       5 | 1405 | `		return 1;` |
|       - | 1406 | `	}` |
|       - | 1407 | `	/* Same class but a different number of attributes ⇒ different property sets` |
|       - | 1408 | `	 * (dynamic properties can give two same-class instances different counts). */` |
|     160 | 1409 | `	if( pLeft->hAttr.nEntry != pRight->hAttr.nEntry ){` |
|       3 | 1410 | `		return 1;` |
|       - | 1411 | `	}` |
|     158 | 1412 | `	PH7_MemObjInit(pLeft->pVm,&sV1);` |
|     158 | 1413 | `	PH7_MemObjInit(pLeft->pVm,&sV2);` |
|     158 | 1414 | `	sV1.nIdx = sV2.nIdx = SXU32_HIGH;` |
|       - | 1415 | `	/* Compare each left attribute against the RIGHT attribute of the SAME NAME` |
|       - | 1416 | `	 * (not in lockstep): dynamic properties may be stored in a different order` |
|       - | 1417 | `	 * on the two instances. Counts already match, so if every left attribute has` |
|       - | 1418 | `	 * an equal-valued same-named right attribute the property sets are equal. */` |
|     158 | 1419 | `	SyHashResetLoopCursor(&pLeft->hAttr);` |
|     194 | 1420 | `	while((pEntry = SyHashGetNextEntry(&pLeft->hAttr)) != 0 ){` |
|     170 | 1421 | `		VmClassAttr *p1 = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1422 | `		VmClassAttr *p2;` |
|       - | 1423 | `		ph7_value *pL,*pR;` |
|       - | 1424 | `		/* Compare only non-static attribute */` |
|     170 | 1425 | `		if( p1->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|     ! 0 | 1426 | `			continue;` |
|       - | 1427 | `		}` |
|     170 | 1428 | `		pEntry2 = SyHashGet(&pRight->hAttr,SyStringData(&p1->pAttr->sName),SyStringLength(&p1->pAttr->sName));` |
|     170 | 1429 | `		if( pEntry2 == 0 ){` |
|       - | 1430 | `			/* Left has a property the right lacks ⇒ not equal. */` |
|     ! 0 | 1431 | `			return 1;` |
|       - | 1432 | `		}` |
|     170 | 1433 | `		p2 = (VmClassAttr *)pEntry2->pUserData;` |
|     170 | 1434 | `		pL = ExtractClassAttrValue(pLeft->pVm,p1);` |
|     170 | 1435 | `		pR = ExtractClassAttrValue(pRight->pVm,p2);` |
|     170 | 1436 | `		if( pL && pR ){` |
|     170 | 1437 | `			PH7_MemObjLoad(pL,&sV1);` |
|     170 | 1438 | `			PH7_MemObjLoad(pR,&sV2);` |
|       - | 1439 | `			/* Compare the two values now */` |
|     170 | 1440 | `			rc = PH7_MemObjCmp(&sV1,&sV2,bStrict,iNest+1);` |
|     170 | 1441 | `			PH7_MemObjRelease(&sV1);` |
|     170 | 1442 | `			PH7_MemObjRelease(&sV2);` |
|     170 | 1443 | `			if( rc != 0 ){` |
|       - | 1444 | `				/* Not equals */` |
|     133 | 1445 | `				return rc;` |
|       - | 1446 | `			}` |
|      18 | 1447 | `		}` |
|       2 | 1448 | `	}` |
|       - | 1449 | `	/* Object are equals */` |
|      26 | 1450 | `	return 0;` |
|     150 | 1451 | `}` |
|       - | 1452 | `/*` |
|       - | 1453 | ` * Dump a class instance and the store the dump in the BLOB given` |
|       - | 1454 | ` * as the first argument.` |
|       - | 1455 | ` * Note that only non-static/non-constants attribute are dumped.` |
|       - | 1456 | ` * This function is typically invoked when the user issue a call` |
|       - | 1457 | ` * to [var_dump(),var_export(),print_r(),...].` |
|       - | 1458 | ` * This function SXRET_OK on success. Any other return value including` |
|       - | 1459 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|       - | 1460 | ` */` |
|       - | 1461 | `/*` |
|       - | 1462 | `` * Return the `name` property value of an enum case instance (the case name),`` |
|       - | 1463 | ` * or 0 when unavailable. Shared by the var_dump/var_export/json/serialize` |
|       - | 1464 | ` * renderers, which all print enum cases as Class::CaseName forms.` |
|       - | 1465 | ` */` |
|       6 | 1466 | `PH7_PRIVATE ph7_value * PH7_EnumCaseNameValue(ph7_class_instance *pThis)` |
|       1 | 1467 | `{` |
|       - | 1468 | `	SyHashEntry *pEntry;` |
|       7 | 1469 | `	if( (pThis->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|     ! 0 | 1470 | `		return 0;` |
|       - | 1471 | `	}` |
|       7 | 1472 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)"name",sizeof("name")-1);` |
|       7 | 1473 | `	if( pEntry == 0 ){` |
|     ! 0 | 1474 | `		return 0;` |
|       - | 1475 | `	}` |
|       7 | 1476 | `	return PH7_ClassInstanceExtractAttrValue(pThis,(VmClassAttr *)pEntry->pUserData);` |
|       4 | 1477 | `}` |
|       - | 1478 | `/*` |
|       - | 1479 | `` * Return the `value` property value (the backing value) of an enum case`` |
|       - | 1480 | ` * instance, or 0 when unavailable (pure enums have none).` |
|       - | 1481 | ` */` |
|       8 | 1482 | `PH7_PRIVATE ph7_value * PH7_EnumCaseBackingValueOf(ph7_class_instance *pThis)` |
|       1 | 1483 | `{` |
|       - | 1484 | `	SyHashEntry *pEntry;` |
|       9 | 1485 | `	if( (pThis->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|     ! 0 | 1486 | `		return 0;` |
|       - | 1487 | `	}` |
|       9 | 1488 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)"value",sizeof("value")-1);` |
|       9 | 1489 | `	if( pEntry == 0 ){` |
|       3 | 1490 | `		return 0;` |
|       - | 1491 | `	}` |
|       7 | 1492 | `	return PH7_ClassInstanceExtractAttrValue(pThis,(VmClassAttr *)pEntry->pUserData);` |
|       5 | 1493 | `}` |
|       - | 1494 | `/*` |
|       - | 1495 | ` * Emit a class-instance dump header plus its trailing newline. For var_dump` |
|       - | 1496 | ` * (ShowType) it completes the "object(" prefix the caller already emitted as` |
|       - | 1497 | ` *   ClassName)#<id> (<count>) {` |
|       - | 1498 | ` * for print_r it emits the legacy PHL  Object(ClassName) {  (count/id unused).` |
|       - | 1499 | `` * Enum cases print php's `ClassName Enum {` print_r header (var_dump never`` |
|       - | 1500 | `` * reaches here for enums — PH7_MemObjDump prints `enum(S::A)` directly).`` |
|       - | 1501 | ` */` |
|     136 | 1502 | `static void DumpClassInstanceHeader(SyBlob *pOut,ph7_class *pClass,sxu32 nObjId,int ShowType,sxu32 nCount)` |
|       3 | 1503 | `{` |
|     139 | 1504 | `	if( ShowType ){` |
|       - | 1505 | ``		/* var_dump: `object(C)#id (n) {` */`` |
|     135 | 1506 | `		SyBlobFormat(&(*pOut),"object(%z)#%u (%u) {",&pClass->sName,nObjId,nCount);` |
|     135 | 1507 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|     135 | 1508 | `		return;` |
|       - | 1509 | `	}` |
|       - | 1510 | ``	/* print_r: `C Object` / `E Enum[:backing]` — the '(' line is emitted by`` |
|       - | 1511 | `	 * the body renderer at the container indent. */` |
|       6 | 1512 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|     ! 0 | 1513 | `		SyBlobFormat(&(*pOut),"%z Enum",&pClass->sName);` |
|     ! 0 | 1514 | `		if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|     ! 0 | 1515 | `			SyBlobAppend(&(*pOut),":int",sizeof(":int")-1);` |
|     ! 0 | 1516 | `		}else if( pClass->nEnumBacking == MEMOBJ_STRING ){` |
|     ! 0 | 1517 | `			SyBlobAppend(&(*pOut),":string",sizeof(":string")-1);` |
|     ! 0 | 1518 | `		}` |
|     ! 0 | 1519 | `	}else{` |
|       6 | 1520 | `		SyBlobFormat(&(*pOut),"%z Object",&pClass->sName);` |
|       - | 1521 | `	}` |
|       6 | 1522 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      71 | 1523 | `}` |
|       - | 1524 | `/*` |
|       - | 1525 | ` * The class that DECLARED pAttr: inheritance shares attr pointers down the` |
|       - | 1526 | ` * chain, so the declaring class is the most ANCESTRAL class whose hAttr still` |
|       - | 1527 | ` * maps the name to this exact pointer. php's var_dump/print_r use it for the` |
|       - | 1528 | `` * `["p":"Decl":private]` annotation.`` |
|       - | 1529 | ` */` |
|       6 | 1530 | `static ph7_class * OoAttrDeclaringClass(ph7_class *pClass,ph7_class_attr *pAttr)` |
|       1 | 1531 | `{` |
|       - | 1532 | `	/* Attrs record their declaring class at install time (inheritance/trait` |
|       - | 1533 | `	 * copies share the pointer, so the field survives the chain). */` |
|       7 | 1534 | `	return pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|       1 | 1535 | `}` |
|       - | 1536 | `/*` |
|       - | 1537 | `` * Emit a property's dump key: var_dump `["x"]=>` / `["p":"C":private]=>` /`` |
|       - | 1538 | `` * `["q":protected]=>`; print_r `[x] => ` / `[p:C:private] => ` /`` |
|       - | 1539 | `` * `[q:protected] => ` (php's exact annotations).`` |
|       - | 1540 | ` */` |
|     138 | 1541 | `static void OoDumpPropKey(SyBlob *pOut,ph7_class_instance *pThis,ph7_class_attr *pAttr,int ShowType)` |
|       2 | 1542 | `{` |
|     140 | 1543 | `	const char *zQ = ShowType ? "\"" : "";` |
|     140 | 1544 | `	SyBlobFormat(&(*pOut),"[%s%z%s",zQ,&pAttr->sName,zQ);` |
|     140 | 1545 | `	if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       7 | 1546 | `		ph7_class *pDecl = OoAttrDeclaringClass(pThis->pClass,pAttr);` |
|       7 | 1547 | `		SyBlobFormat(&(*pOut),":%s%z%s:private",zQ,&pDecl->sName,zQ);` |
|     137 | 1548 | `	}else if( pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
|     ! 0 | 1549 | `		SyBlobAppend(&(*pOut),":protected",sizeof(":protected")-1);` |
|     ! 0 | 1550 | `	}` |
|     140 | 1551 | `	SyBlobAppend(&(*pOut),ShowType ? "]=>" : "] => ",ShowType ? 3 : 5);` |
|     140 | 1552 | `}` |
|     140 | 1553 | `PH7_PRIVATE sxi32 PH7_ClassInstanceDump(SyBlob *pOut,ph7_class_instance *pThis,int ShowType,int nTab,int nDepth)` |
|       3 | 1554 | `{` |
|       - | 1555 | `	SyHashEntry *pEntry;` |
|       - | 1556 | `	ph7_value *pValue;` |
|       - | 1557 | `	sxi32 rc;` |
|       - | 1558 | `	int i;` |
|     143 | 1559 | `	if( nDepth > 31 ){` |
|       - | 1560 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|       - | 1561 | `		/* Nesting limit reached..halt immediately*/` |
|       5 | 1562 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|       5 | 1563 | `		return SXERR_LIMIT;` |
|       - | 1564 | `	}` |
|     139 | 1565 | `	rc = SXRET_OK;` |
|       - | 1566 | `	{` |
|       - | 1567 | `		/* Both var_dump and print_r consult __debugInfo() (PHP behavior);` |
|       - | 1568 | `		 * var_export uses a separate renderer and never reaches here. When the` |
|       - | 1569 | `		 * method is present and returns an array, render that array's entries as` |
|       - | 1570 | `		 * the object body, with the header showing the debug array's count. The` |
|       - | 1571 | `		 * nDepth guard above protects against a __debugInfo returning the object` |
|       - | 1572 | `		 * itself. */` |
|     139 | 1573 | `		ph7_class_method *pDbg = PH7_ClassExtractMethod(pThis->pClass,"__debugInfo",sizeof("__debugInfo")-1);` |
|     139 | 1574 | `		if( pDbg ){` |
|       - | 1575 | `			ph7_value sResult;` |
|       5 | 1576 | `			PH7_MemObjInit(pThis->pVm,&sResult);` |
|       5 | 1577 | `			PH7_VmCallClassMethod(pThis->pVm,pThis,pDbg,&sResult,0,0);` |
|       5 | 1578 | `			if( sResult.iFlags & MEMOBJ_HASHMAP ){` |
|       5 | 1579 | `				ph7_hashmap *pMap = (ph7_hashmap *)sResult.x.pOther;` |
|       - | 1580 | `				/* Header count is the debug array's entry count. */` |
|       5 | 1581 | `				DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,pMap->nEntry);` |
|       5 | 1582 | `				if( !ShowType ){` |
|       3 | 1583 | `					for( i = 0 ; i < nTab ; i++ ){` |
|     ! 0 | 1584 | `						SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     ! 0 | 1585 | `					}` |
|       3 | 1586 | `					SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       1 | 1587 | `				}` |
|       5 | 1588 | `				rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth);` |
|       5 | 1589 | `				for( i = 0 ; i < nTab ; i++ ){` |
|     ! 0 | 1590 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     ! 0 | 1591 | `				}` |
|       5 | 1592 | `				if( ShowType ){` |
|       3 | 1593 | `					SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|       2 | 1594 | `				}else{` |
|       3 | 1595 | `					SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       - | 1596 | `				}` |
|       5 | 1597 | `				PH7_MemObjRelease(&sResult);` |
|       5 | 1598 | `				return rc;` |
|       - | 1599 | `			}` |
|       - | 1600 | `			/* Non-array return: behave as if __debugInfo were absent. */` |
|     ! 0 | 1601 | `			PH7_MemObjRelease(&sResult);` |
|     ! 0 | 1602 | `		}` |
|       - | 1603 | `	}` |
|       - | 1604 | `	{` |
|       - | 1605 | `		/* var_dump's header needs the property count up front, so pre-count the` |
|       - | 1606 | `		 * non-static/non-constant attributes (matching the dump loop below). */` |
|     134 | 1607 | `		sxu32 nProp = 0;` |
|     134 | 1608 | `		if( ShowType ){` |
|     132 | 1609 | `			SyHashResetLoopCursor(&pThis->hAttr);` |
|     270 | 1610 | `			while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|     140 | 1611 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     140 | 1612 | `				if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_HOOK_VIRTUAL)) == 0 ){` |
|     136 | 1613 | `					nProp++;` |
|      67 | 1614 | `				}` |
|       2 | 1615 | `			}` |
|      65 | 1616 | `		}` |
|     134 | 1617 | `		DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,nProp);` |
|       - | 1618 | `	}` |
|     134 | 1619 | `	if( !ShowType ){` |
|       - | 1620 | `		/* print_r body opener: '(' at the container indent */` |
|       3 | 1621 | `		for( i = 0 ; i < nTab ; i++ ){` |
|     ! 0 | 1622 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     ! 0 | 1623 | `		}` |
|       3 | 1624 | `		SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       1 | 1625 | `	}` |
|       - | 1626 | `	/* Dump object attributes (php 8.4: VIRTUAL hooked properties have no` |
|       - | 1627 | `	 * backing store — excluded from var_dump/print_r) */` |
|     134 | 1628 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|     218 | 1629 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|     144 | 1630 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     144 | 1631 | `		if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_HOOK_VIRTUAL)) == 0 ){` |
|       - | 1632 | `			/* Dump non-static/constant attribute only */` |
|     140 | 1633 | `			pValue = ExtractClassAttrValue(pThis->pVm,pVmAttr);` |
|     140 | 1634 | `			if( pValue == 0 ){` |
|     ! 0 | 1635 | `				continue;` |
|       - | 1636 | `			}` |
|     140 | 1637 | `			if( ShowType ){` |
|       - | 1638 | ``				/* var_dump prop: `["x"(:…)]=>` at nTab+2, the value on the next`` |
|       - | 1639 | `				 * line at the same indent (php). */` |
|    4124 | 1640 | `				for( i = 0 ; i < nTab + 2 ; i++ ){` |
|    3990 | 1641 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|    1996 | 1642 | `				}` |
|     136 | 1643 | `				OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,TRUE);` |
|     136 | 1644 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|     136 | 1645 | `				rc = PH7_MemObjDump(&(*pOut),pValue,TRUE,nTab+2,nDepth,0);` |
|     136 | 1646 | `				if( rc == SXERR_LIMIT ){` |
|     125 | 1647 | `					break;` |
|       - | 1648 | `				}` |
|       7 | 1649 | `			}else{` |
|       - | 1650 | ``				/* print_r prop: `[x(:…)] => value` at nTab+4; container values`` |
|       - | 1651 | `				 * render their block at nTab+8 followed by php's blank line. */` |
|      21 | 1652 | `				for( i = 0 ; i < nTab + 4 ; i++ ){` |
|      17 | 1653 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       9 | 1654 | `				}` |
|       5 | 1655 | `				OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,FALSE);` |
|       4 | 1656 | `				if( (pValue->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ))` |
|       3 | 1657 | `				 && (pValue->iFlags & MEMOBJ_NULL) == 0 ){` |
|     ! 0 | 1658 | `					rc = PH7_MemObjDump(&(*pOut),pValue,FALSE,nTab+8,nDepth,0);` |
|     ! 0 | 1659 | `					SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|     ! 0 | 1660 | `					if( rc == SXERR_LIMIT ){` |
|     ! 0 | 1661 | `						break;` |
|       - | 1662 | `					}` |
|     ! 0 | 1663 | `				}else{` |
|       5 | 1664 | `					PH7_MemObjPrintRInline(&(*pOut),pValue);` |
|       5 | 1665 | `					SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 1666 | `				}` |
|       - | 1667 | `			}` |
|       7 | 1668 | `		}` |
|       2 | 1669 | `	}` |
|    3854 | 1670 | `	for( i = 0 ; i < nTab ; i++ ){` |
|    3721 | 1671 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|    1861 | 1672 | `	}` |
|     134 | 1673 | `	if( ShowType ){` |
|     132 | 1674 | `		SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      67 | 1675 | `	}else{` |
|       3 | 1676 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       - | 1677 | `	}` |
|     134 | 1678 | `	return rc;` |
|      73 | 1679 | `}` |
|       - | 1680 | `/*` |
|       - | 1681 | ` * Call a magic method [i.e: __toString(),__toBool(),__Invoke()...]` |
|       - | 1682 | ` * Return SXRET_OK on successfull call. Any other return value indicates failure.` |
|       - | 1683 | ` * Notes on magic methods.` |
|       - | 1684 | ` * According to the PHP language reference manual.` |
|       - | 1685 | ` *  The function names __construct(), __destruct(), __call(), __callStatic()` |
|       - | 1686 | ` *  __get(),  __toString(), __invoke(), __clone() are magical in PHP classes.` |
|       - | 1687 | ` * You cannot have functions with these names in any of your classes unless` |
|       - | 1688 | ` * you want the magic functionality associated with them.` |
|       - | 1689 | ` * Example of magical methods:` |
|       - | 1690 | ` * __toString()` |
|       - | 1691 | ` *  The __toString() method allows a class to decide how it will react when it is treated like` |
|       - | 1692 | ` *  a string. For example, what echo $obj; will print. This method must return a string.` |
|       - | 1693 | ` *  Example #2 Simple example` |
|       - | 1694 | ` * <?php` |
|       - | 1695 | ` * // Declare a simple class` |
|       - | 1696 | ` * class TestClass` |
|       - | 1697 | ` * {` |
|       - | 1698 | ` *   public $foo;` |
|       - | 1699 | ` *` |
|       - | 1700 | ` *   public function __construct($foo)` |
|       - | 1701 | ` *   {` |
|       - | 1702 | ` *       $this->foo = $foo;` |
|       - | 1703 | ` *   }` |
|       - | 1704 | ` *` |
|       - | 1705 | ` *   public function __toString()` |
|       - | 1706 | ` *   {` |
|       - | 1707 | ` *       return $this->foo;` |
|       - | 1708 | ` *   }` |
|       - | 1709 | ` * }` |
|       - | 1710 | ` * $class = new TestClass('Hello');` |
|       - | 1711 | ` * echo $class;` |
|       - | 1712 | ` * ?>` |
|       - | 1713 | ` * The above example will output:` |
|       - | 1714 | ` *  Hello` |
|       - | 1715 | ` *` |
|       - | 1716 | ` * Note that PH7 does not support all the magical method and introudces __toFloat(),__toInt()` |
|       - | 1717 | ` * which have the same behaviour as __toString() but for float and integer types` |
|       - | 1718 | ` * respectively.` |
|       - | 1719 | ` * Refer to the official documentation for more information.` |
|       - | 1720 | ` */` |
|     286 | 1721 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallMagicMethod(` |
|       - | 1722 | `	ph7_vm *pVm,               /* VM that own all this stuff */` |
|       - | 1723 | `	ph7_class *pClass,         /* Target class */` |
|       - | 1724 | `	ph7_class_instance *pThis, /* Target object */` |
|       - | 1725 | `	const char *zMethod,       /* Magic method name [i.e: __toString()]*/` |
|       - | 1726 | `	sxu32 nByte,               /* zMethod length*/` |
|       - | 1727 | `	const SyString *pAttrName, /* Attribute name */` |
|       - | 1728 | `	ph7_value *pResult         /* OUT: magic method return value. NULL to discard */` |
|       - | 1729 | `	)` |
|       1 | 1730 | `{` |
|     287 | 1731 | `	ph7_value *apArg[2] = { 0 , 0 };` |
|       - | 1732 | `	ph7_class_method *pMeth;` |
|       - | 1733 | `	ph7_value sAttr; /* cc warning */` |
|       - | 1734 | `	sxi32 rc;` |
|       - | 1735 | `	int nArg;` |
|       - | 1736 | `	/* Make sure the magic method is available */` |
|     287 | 1737 | `	pMeth = PH7_ClassExtractMethod(&(*pClass),zMethod,nByte);` |
|     287 | 1738 | `	if( pMeth == 0 ){` |
|       - | 1739 | `		/* No such method,return immediately */` |
|     ! 0 | 1740 | `		return SXERR_NOTFOUND;` |
|       - | 1741 | `	}` |
|     287 | 1742 | `	nArg = 0;` |
|       - | 1743 | `	/* Copy arguments */` |
|     287 | 1744 | `	if( pAttrName ){` |
|     287 | 1745 | `		PH7_MemObjInitFromString(pVm,&sAttr,pAttrName);` |
|     287 | 1746 | `		sAttr.nIdx = SXU32_HIGH; /* Mark as constant */` |
|     287 | 1747 | `		apArg[0] = &sAttr;` |
|     287 | 1748 | `		nArg = 1;` |
|     143 | 1749 | `	}` |
|       - | 1750 | `	/* Call the magic method now */` |
|     287 | 1751 | `	rc = PH7_VmCallClassMethod(pVm,&(*pThis),pMeth,pResult,nArg,apArg);` |
|       - | 1752 | `	/* Clean up */` |
|     287 | 1753 | `	if( pAttrName ){` |
|     287 | 1754 | `		PH7_MemObjRelease(&sAttr);` |
|     143 | 1755 | `	}` |
|     287 | 1756 | `	return rc;` |
|     144 | 1757 | `}` |
|       - | 1758 | `/*` |
|       - | 1759 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon].` |
|       - | 1760 | ` * This function is simply a wrapper on ExtractClassAttrValue().` |
|       - | 1761 | ` */` |
|   11306 | 1762 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceExtractAttrValue(ph7_class_instance *pThis,VmClassAttr *pAttr)` |
|       5 | 1763 | `{` |
|       - | 1764 | `   /* Extract the attribute value */` |
|       - | 1765 | `	ph7_value *pValue;` |
|   11311 | 1766 | `	pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|   11311 | 1767 | `	return pValue;` |
|       5 | 1768 | `}` |
|       - | 1769 | `/*` |
|       - | 1770 | ` * Convert a class instance [i.e: Object in the PHP jargon] into a hashmap [i.e: array in the PHP jargon].` |
|       - | 1771 | ` * Return SXRET_OK on success. Any other value indicates failure.` |
|       - | 1772 | ` * Note on object conversion to array:` |
|       - | 1773 | ` *  Acccording to the PHP language reference manual` |
|       - | 1774 | ` *  If an object is converted to an array, the result is an array whose elements are the object's properties.` |
|       - | 1775 | ` *  The keys are the member variable names.` |
|       - | 1776 | ` *` |
|       - | 1777 | ` *  The following example:` |
|       - | 1778 | ` *  class Test {` |
|       - | 1779 | ` *   public $A = 25<<1;  // 50` |
|       - | 1780 | ` *	 public $c = rand_str(3);   // Random string of length 3` |
|       - | 1781 | ` *	 public $d = rand() & 1023; // Random number between 0..1023` |
|       - | 1782 | ` *  }` |
|       - | 1783 | ` *  var_dump((array) new Test());` |
|       - | 1784 | ` *	Will output:` |
|       - | 1785 | ` *  array(3) {` |
|       - | 1786 | ` *   [A] =>` |
|       - | 1787 | ` *      int(50)` |
|       - | 1788 | ` *   [c] =>` |
|       - | 1789 | ` *     string(3 'aps')` |
|       - | 1790 | ` *   [d] =>` |
|       - | 1791 | ` *     int(991)` |
|       - | 1792 | ` *  }` |
|       - | 1793 | ` * You have noticed that PH7 allow class attributes [i.e: $a,$c,$d in the example above]` |
|       - | 1794 | ` * have any complex expression (even function calls/Annonymous functions) as their default` |
|       - | 1795 | ` * value unlike the standard PHP engine.` |
|       - | 1796 | ` * This is a very powerful feature that you have to look at.` |
|       - | 1797 | ` */` |
|      16 | 1798 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|       2 | 1799 | `{` |
|       - | 1800 | `	SyHashEntry *pEntry;` |
|       - | 1801 | `	SyString *pAttrName;` |
|       - | 1802 | `	VmClassAttr *pAttr;` |
|       - | 1803 | `	ph7_value *pValue;` |
|       - | 1804 | `	ph7_value sName;` |
|       - | 1805 | `	/* Reset the loop cursor */` |
|      18 | 1806 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      18 | 1807 | `	PH7_MemObjInitFromString(pThis->pVm,&sName,0);` |
|      60 | 1808 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       - | 1809 | `		/* Point to the current attribute */` |
|      44 | 1810 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      44 | 1811 | `		if( pAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|       - | 1812 | `			/* php 8.4: a VIRTUAL hooked property has no backing store — the` |
|       - | 1813 | `			 * (array) cast excludes it (raw surface, get is NOT dispatched) */` |
|       7 | 1814 | `			continue;` |
|       - | 1815 | `		}` |
|       - | 1816 | `		/* Extract attribute value */` |
|      38 | 1817 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|      38 | 1818 | `		if( pValue ){` |
|       - | 1819 | `			/* Build attribute name */` |
|      38 | 1820 | `			pAttrName = &pAttr->pAttr->sName;` |
|      38 | 1821 | `			PH7_MemObjStringAppend(&sName,pAttrName->zString,pAttrName->nByte);` |
|       - | 1822 | `			/* Perform the insertion */` |
|      38 | 1823 | `			PH7_HashmapInsert(pMap,&sName,pValue);` |
|       - | 1824 | `			/* Reset the string cursor */` |
|      38 | 1825 | `			SyBlobReset(&sName.sBlob);` |
|      18 | 1826 | `		}` |
|       2 | 1827 | `	}` |
|      18 | 1828 | `	PH7_MemObjRelease(&sName);` |
|      18 | 1829 | `	return SXRET_OK;` |
|       2 | 1830 | `}` |
|       - | 1831 | `/*` |
|       - | 1832 | ` * Iterate throw class attributes and invoke the given callback [i.e: xWalk()] for each` |
|       - | 1833 | ` * retrieved attribute.` |
|       - | 1834 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|       - | 1835 | ` * the attribute value in the callback body will not alter the real attribute value.` |
|       - | 1836 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|       - | 1837 | ` * a value different from PH7_OK.` |
|       - | 1838 | ` * Refer to [ph7_object_walk()] for more information.` |
|       - | 1839 | ` */` |
|     ! 0 | 1840 | `PH7_PRIVATE sxi32 PH7_ClassInstanceWalk(` |
|       - | 1841 | `	ph7_class_instance *pThis, /* Target object */` |
|       - | 1842 | `	int (*xWalk)(const char *,ph7_value *,void *), /* Walker callback */` |
|       - | 1843 | `	void *pUserData /* Last argument to xWalk() */` |
|       - | 1844 | `	)` |
|     ! 0 | 1845 | `{` |
|       - | 1846 | `	SyHashEntry *pEntry; /* Hash entry */` |
|       - | 1847 | `	VmClassAttr *pAttr;  /* Pointer to the attribute */` |
|       - | 1848 | `	ph7_value *pValue;   /* Attribute value */` |
|       - | 1849 | `	ph7_value sValue;    /* Copy of the attribute value */` |
|       - | 1850 | `	int rc;` |
|       - | 1851 | `	/* Reset the loop cursor */` |
|     ! 0 | 1852 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|     ! 0 | 1853 | `	PH7_MemObjInit(pThis->pVm,&sValue);` |
|       - | 1854 | `	/* Start the walk process */` |
|     ! 0 | 1855 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       - | 1856 | `		/* Point to the current attribute */` |
|     ! 0 | 1857 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1858 | `		/* Extract attribute value */` |
|     ! 0 | 1859 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     ! 0 | 1860 | `		if( pValue ){` |
|     ! 0 | 1861 | `			PH7_MemObjLoad(pValue,&sValue);` |
|       - | 1862 | `			/* Invoke the supplied callback */` |
|     ! 0 | 1863 | `			rc =  xWalk(SyStringData(&pAttr->pAttr->sName),&sValue,pUserData);` |
|     ! 0 | 1864 | `			PH7_MemObjRelease(&sValue);` |
|     ! 0 | 1865 | `			if( rc != PH7_OK){` |
|       - | 1866 | `				/* User callback request an operation abort */` |
|     ! 0 | 1867 | `				return SXERR_ABORT;` |
|       - | 1868 | `			}` |
|     ! 0 | 1869 | `		}` |
|     ! 0 | 1870 | `	}` |
|       - | 1871 | `	/* All done */` |
|     ! 0 | 1872 | `	return SXRET_OK;` |
|     ! 0 | 1873 | `}` |
|       - | 1874 | `/*` |
|       - | 1875 | ` * Extract a class atrribute value.` |
|       - | 1876 | ` * Return a pointer to the attribute value on success. Otherwise NULL.` |
|       - | 1877 | ` * Note:` |
|       - | 1878 | ` *  Access to static and constant attribute is not allowed. That is,the function` |
|       - | 1879 | ` *  will return NULL in case someone (host-application code) try to extract` |
|       - | 1880 | ` *  a static/constant attribute.` |
|       - | 1881 | ` */` |
|   11038 | 1882 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceFetchAttr(ph7_class_instance *pThis,const SyString *pName)` |
|       5 | 1883 | `{` |
|       - | 1884 | `	SyHashEntry *pEntry;` |
|       - | 1885 | `	VmClassAttr *pAttr;` |
|       - | 1886 | `	/* Query the attribute hashtable */` |
|   11043 | 1887 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);` |
|   11043 | 1888 | `	if( pEntry == 0 ){` |
|       - | 1889 | `		/* No such attribute */` |
|     ! 0 | 1890 | `		return 0;` |
|       - | 1891 | `	}` |
|       - | 1892 | `	/* Point to the class atrribute */` |
|   11043 | 1893 | `	pAttr = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1894 | `	/* Check if we are dealing with a static/constant attribute */` |
|   11043 | 1895 | `	if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|       - | 1896 | `		/* Access is forbidden */` |
|     ! 0 | 1897 | `		return 0;` |
|       - | 1898 | `	}` |
|       - | 1899 | `	/* Return the attribute value */` |
|   11043 | 1900 | `	return ExtractClassAttrValue(pThis->pVm,pAttr);` |
|    5524 | 1901 | `}` |
|       - | 1902 |  |
