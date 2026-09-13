# src/ph7/oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 703/797 lines (88.21%)

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
|  464818 |   14 | `PH7_PRIVATE ph7_class * PH7_NewRawClass(ph7_vm *pVm,const SyString *pName,sxu32 nLine)` |
|       5 |   15 | `{` |
|       - |   16 | `	ph7_class *pClass;` |
|       - |   17 | `	char *zName;` |
|       - |   18 | `	/* Allocate a new instance */` |
|  464823 |   19 | `	pClass = (ph7_class *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class));` |
|  464823 |   20 | `	if( pClass == 0 ){` |
|     ! 0 |   21 | `		return 0;` |
|       - |   22 | `	}` |
|       - |   23 | `	/* Zero the structure */` |
|  464823 |   24 | `	SyZero(pClass,sizeof(ph7_class));` |
|       - |   25 | `	/* Duplicate class name */` |
|  464823 |   26 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  464823 |   27 | `	if( zName == 0 ){` |
|     ! 0 |   28 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClass);` |
|     ! 0 |   29 | `		return 0;` |
|       - |   30 | `	}` |
|       - |   31 | `	/* Initialize fields */` |
|  464823 |   32 | `	SyStringInitFromBuf(&pClass->sName,zName,pName->nByte);` |
|  464823 |   33 | `	SyHashInit(&pClass->hMethod,&pVm->sAllocator,0,0);` |
|  464823 |   34 | `	SyHashInit(&pClass->hAttr,&pVm->sAllocator,0,0);` |
|  464823 |   35 | `	SyHashInit(&pClass->hDerived,&pVm->sAllocator,0,0);` |
|  464823 |   36 | `	SySetInit(&pClass->aInterface,&pVm->sAllocator,sizeof(ph7_class *));` |
|  464823 |   37 | `	SySetInit(&pClass->aTrait,&pVm->sAllocator,sizeof(ph7_class *));` |
|  464823 |   38 | `	SySetInit(&pClass->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|  464823 |   39 | `	SySetInit(&pClass->aEnumCases,&pVm->sAllocator,sizeof(ph7_class_attr *));` |
|  464823 |   40 | `	pClass->nLine = nLine;` |
|  464823 |   41 | `	if( pVm->bCompilingBuiltin ){` |
|       - |   42 | `		/* Defined by an embedded builtin chunk: internal, no defining file.` |
|       - |   43 | `		 * Class compilers merge further flags with \|= so this survives. */` |
|  462677 |   44 | `		pClass->iFlags \|= PH7_CLASS_INTERNAL;` |
|  231341 |   45 | `	}else{` |
|       - |   46 | `		/* Alias the VM-lifetime path dup on top of the include stack */` |
|    2151 |   47 | `		SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|    2151 |   48 | `		if( pFile ){` |
|    2151 |   49 | `			SyStringDupPtr(&pClass->sFile,pFile);` |
|    1073 |   50 | `		}` |
|       - |   51 | `	}` |
|       - |   52 | `	/* All done */` |
|  464823 |   53 | `	return pClass;` |
|  232414 |   54 | `}` |
|       - |   55 | `/*` |
|       - |   56 | ` * Allocate and initialize a new class attribute.` |
|       - |   57 | ` * Return a pointer to the class attribute on success. NULL otherwise.` |
|       - |   58 | ` */` |
|  864534 |   59 | `PH7_PRIVATE ph7_class_attr * PH7_NewClassAttr(ph7_vm *pVm,const SyString *pName,sxu32 nLine,sxi32 iProtection,sxi32 iFlags)` |
|       5 |   60 | `{` |
|       - |   61 | `	ph7_class_attr *pAttr;` |
|       - |   62 | `	char *zName;` |
|  864539 |   63 | `	pAttr = (ph7_class_attr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_attr));` |
|  864539 |   64 | `	if( pAttr == 0 ){` |
|     ! 0 |   65 | `		return 0;` |
|       - |   66 | `	}` |
|       - |   67 | `	/* Zero the structure */` |
|  864539 |   68 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|  864539 |   69 | `	SySetInit(&pAttr->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|       - |   70 | `	/* Duplicate attribute name */` |
|  864539 |   71 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  864539 |   72 | `	if( zName == 0 ){` |
|     ! 0 |   73 | `		SyMemBackendPoolFree(&pVm->sAllocator,pAttr);` |
|     ! 0 |   74 | `		return 0;` |
|       - |   75 | `	}` |
|       - |   76 | `	/* Initialize fields */` |
|  864539 |   77 | `	SySetInit(&pAttr->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|  864539 |   78 | `	SySetInit(&pAttr->aUnionAlts,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|  864539 |   79 | `	SyStringInitFromBuf(&pAttr->sName,zName,pName->nByte);` |
|  864539 |   80 | `	pAttr->iProtection = iProtection;` |
|  864539 |   81 | `	pAttr->nIdx = SXU32_HIGH;` |
|  864539 |   82 | `	pAttr->iFlags = iFlags;` |
|  864539 |   83 | `	pAttr->nLine = nLine;` |
|  864539 |   84 | `	return pAttr;` |
|  432272 |   85 | `}` |
|       - |   86 | `/*` |
|       - |   87 | ` * Allocate and initialize a new class method.` |
|       - |   88 | ` * Return a pointer to the class method on success. NULL otherwise` |
|       - |   89 | ` * This function associate with the newly created method an automatically generated` |
|       - |   90 | ` * random unique name.` |
|       - |   91 | ` */` |
| 2649730 |   92 | `PH7_PRIVATE ph7_class_method * PH7_NewClassMethod(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,sxu32 nLine,` |
|       - |   93 | `	sxi32 iProtection,sxi32 iFlags,sxi32 iFuncFlags)` |
|       5 |   94 | `{` |
|       - |   95 | `	ph7_class_method *pMeth;` |
|       - |   96 | `	SyHashEntry *pEntry;` |
|       - |   97 | `	SyString *pNamePtr;` |
|       - |   98 | `	char zSalt[10];` |
|       - |   99 | `	char *zName;` |
|       - |  100 | `	sxu32 nByte;` |
|       - |  101 | `	/* Allocate a new class method instance */` |
| 2649735 |  102 | `	pMeth = (ph7_class_method *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_method));` |
| 2649735 |  103 | `	if( pMeth == 0 ){` |
|     ! 0 |  104 | `		return 0;` |
|       - |  105 | `	}` |
|       - |  106 | `	/* Zero the structure */` |
| 2649735 |  107 | `	SyZero(pMeth,sizeof(ph7_class_method));` |
|       - |  108 | `	/* Check for an already installed method with the same name */` |
| 2649735 |  109 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
| 2649735 |  110 | `	if( pEntry == 0 ){` |
|       - |  111 | `		/* Associate an unique VM name to this method */` |
| 2649733 |  112 | `		nByte = sizeof(zSalt) + pName->nByte + SyStringLength(&pClass->sName)+sizeof(char)*7/*[[__'\0'*/;` |
| 2649733 |  113 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,nByte);` |
| 2649733 |  114 | `		if( zName == 0 ){` |
|     ! 0 |  115 | `			SyMemBackendPoolFree(&pVm->sAllocator,pMeth);` |
|     ! 0 |  116 | `			return 0;` |
|       - |  117 | `		}` |
| 2649733 |  118 | `		pNamePtr = &pMeth->sVmName;` |
|       - |  119 | `		/* Generate a random string */` |
| 2649733 |  120 | `		PH7_VmRandomString(&(*pVm),zSalt,sizeof(zSalt));` |
| 2649733 |  121 | `		pNamePtr->nByte = SyBufferFormat(zName,nByte,"[__%z@%z_%.*s]",&pClass->sName,pName,sizeof(zSalt),zSalt);` |
| 2649733 |  122 | `		pNamePtr->zString = zName;` |
| 1324869 |  123 | `	}else{` |
|       - |  124 | `		/* Method is condidate for 'overloading' */` |
|       3 |  125 | `		ph7_class_method *pCurrent = (ph7_class_method *)pEntry->pUserData;` |
|       3 |  126 | `		pNamePtr = &pMeth->sVmName;` |
|       - |  127 | `		/* Use the same VM name */` |
|       3 |  128 | `		SyStringDupPtr(pNamePtr,&pCurrent->sVmName);` |
|       3 |  129 | `		zName = (char *)pNamePtr->zString;` |
|       - |  130 | `	}` |
| 2649735 |  131 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|  163374 |  132 | `		if( (pName->nByte == sizeof("__destruct") - 1 && SyMemcmp(pName->zString,"__destruct",sizeof("__destruct") - 1 ) == 0)` |
|  163379 |  133 | `			\|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|       - |  134 | `				/* Switch to public visibility for destructors and legacy class-name` |
|       - |  135 | `				 * constructors (the engine invokes destructors internally, bypassing` |
|       - |  136 | `				 * visibility either way). __construct KEEPS its declared visibility` |
|       - |  137 | ``				 * (band A #4): php enforces it at `new` — a private/protected ctor`` |
|       - |  138 | `				 * from the wrong scope is a catchable Error, checked at OP_NEW —` |
|       - |  139 | `				 * and ReflectionClass::isInstantiable()/newInstance() now see it. */` |
|     ! 0 |  140 | `				iProtection = PH7_CLASS_PROT_PUBLIC;` |
|     ! 0 |  141 | `		}` |
|   81687 |  142 | `	}` |
|       - |  143 | `	/* Initialize method fields */` |
| 2649735 |  144 | `	pMeth->iProtection = iProtection;` |
| 2649735 |  145 | `	pMeth->iFlags = iFlags;` |
| 2649735 |  146 | `	pMeth->nLine = nLine;` |
| 3974600 |  147 | `	PH7_VmInitFuncState(&(*pVm),&pMeth->sFunc,&zName[sizeof(char)*4/*[__@*/+SyStringLength(&pClass->sName)],` |
| 2649730 |  148 | `		pName->nByte,iFuncFlags\|VM_FUNC_CLASS_METHOD,pClass);` |
| 2649735 |  149 | `	return pMeth;` |
| 1324870 |  150 | `}` |
|       - |  151 | `/*` |
|       - |  152 | ` * Check if the given name have a class method associated with it.` |
|       - |  153 | ` * Return the desired method [i.e: ph7_class_method instance] on success. NULL otherwise.` |
|       - |  154 | ` */` |
|  862706 |  155 | `PH7_PRIVATE ph7_class_method * PH7_ClassExtractMethod(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|       5 |  156 | `{` |
|       - |  157 | `	SyHashEntry *pEntry;` |
|       - |  158 | `	/* Perform a hash lookup */` |
|  862711 |  159 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)zName,nByte);` |
|  862711 |  160 | `	if( pEntry == 0 ){` |
|       - |  161 | `		/* No such entry */` |
|   29511 |  162 | `		return 0;` |
|       - |  163 | `	}` |
|       - |  164 | `	/* Point to the desired method */` |
|  833205 |  165 | `	return (ph7_class_method *)pEntry->pUserData;` |
|  431358 |  166 | `}` |
|       - |  167 | `/*` |
|       - |  168 | ` * Check if the given name is a class attribute.` |
|       - |  169 | ` * Return the desired attribute [i.e: ph7_class_attr instance] on success.NULL otherwise.` |
|       - |  170 | ` */` |
|  508042 |  171 | `PH7_PRIVATE ph7_class_attr * PH7_ClassExtractAttribute(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|       5 |  172 | `{` |
|       - |  173 | `	SyHashEntry *pEntry;` |
|       - |  174 | `	/* Perform a hash lookup */` |
|  508047 |  175 | `	pEntry = SyHashGet(&pClass->hAttr,(const void *)zName,nByte);` |
|  508047 |  176 | `	if( pEntry == 0 ){` |
|       - |  177 | `		/* No such entry */` |
|  506655 |  178 | `		return 0;` |
|       - |  179 | `	}` |
|       - |  180 | `	/* Point to the desierd method */` |
|    1397 |  181 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|  254026 |  182 | `}` |
|       - |  183 | `/*` |
|       - |  184 | ` * Install a class attribute in the corresponding container.` |
|       - |  185 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|       - |  186 | ` */` |
|  864530 |  187 | `PH7_PRIVATE sxi32 PH7_ClassInstallAttr(ph7_class *pClass,ph7_class_attr *pAttr)` |
|       5 |  188 | `{` |
|  864535 |  189 | `	SyString *pName = &pAttr->sName;` |
|       - |  190 | `	sxi32 rc;` |
|       - |  191 | `	/* Remember where this attribute was originally declared so that later` |
|       - |  192 | `	 * inheritance/trait copies still know the declaring class (needed for` |
|       - |  193 | `	 * PHP-compatible error messages on typed properties). */` |
|  864535 |  194 | `	if( pAttr->pDeclClass == 0 ){` |
|  864535 |  195 | `		pAttr->pDeclClass = pClass;` |
|  432265 |  196 | `	}` |
|  864535 |  197 | `	rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|  864535 |  198 | `	return rc;` |
|       5 |  199 | `}` |
|       - |  200 | `/*` |
|       - |  201 | ` * Install a class method in the corresponding container.` |
|       - |  202 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|       - |  203 | ` */` |
| 2649718 |  204 | `PH7_PRIVATE sxi32 PH7_ClassInstallMethod(ph7_class *pClass,ph7_class_method *pMeth)` |
|       5 |  205 | `{` |
| 2649723 |  206 | `	SyString *pName = &pMeth->sFunc.sName;` |
|       - |  207 | `	sxi32 rc;` |
| 2649723 |  208 | `	rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 2649723 |  209 | `	return rc;` |
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
|  202400 |  234 | `static int OoClassifyOverrideType(ph7_vm *pVm, sxu32 nType, const SyString *pClass,` |
|       - |  235 | `	int bUnion, ph7_class **ppClass)` |
|       5 |  236 | `{` |
|  202405 |  237 | `	*ppClass = 0;` |
|  202405 |  238 | `	if( bUnion ){` |
|       3 |  239 | `		return OVT_SKIP; /* union/intersection — full lattice, skip */` |
|       - |  240 | `	}` |
|  202403 |  241 | `	if( nType == 0 ){` |
|  202299 |  242 | `		return OVT_NONE; /* no declared type */` |
|       - |  243 | `	}` |
|     109 |  244 | `	if( nType == SXU32_HIGH ){` |
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
|      31 |  268 | `	 \|\| nType == MEMOBJ_BOOL \|\| nType == MEMOBJ_HASHMAP ){` |
|      64 |  269 | `		return OVT_SCALAR;` |
|       - |  270 | `	}` |
|       - |  271 | `	/* MEMOBJ_OBJ (object — subtypes against classes), MEMOBJ_VOID/NULL/RES,` |
|       - |  272 | `	 * or anything unexpected: skip. */` |
|      26 |  273 | `	return OVT_SKIP;` |
|  101205 |  274 | `}` |
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
|  171232 |  288 | `static OvType OoTypeFromReturn(ph7_vm_func *pF)` |
|       5 |  289 | `{` |
|       - |  290 | `	OvType t;` |
|  171237 |  291 | `	t.nType = pF->nReturnType;` |
|  171237 |  292 | `	t.pClass = &pF->sReturnClass;` |
|  171237 |  293 | `	t.bUnion = SySetUsed(&pF->aReturnUnion) > 0;` |
|  171237 |  294 | `	t.bNullable = (pF->iFlags & VM_FUNC_RETURN_NULLABLE) != 0;` |
|  171237 |  295 | `	return t;` |
|       5 |  296 | `}` |
|   31168 |  297 | `static OvType OoTypeFromArg(ph7_vm_func_arg *pA)` |
|       5 |  298 | `{` |
|       - |  299 | `	OvType t;` |
|   31173 |  300 | `	t.nType = pA->nType;` |
|   31173 |  301 | `	t.pClass = &pA->sClass;` |
|   31173 |  302 | `	t.bUnion = (pA->iFlags & VM_FUNC_ARG_UNION) != 0;` |
|   31173 |  303 | `	t.bNullable = (pA->iFlags & VM_FUNC_ARG_NULLABLE) != 0;` |
|   31173 |  304 | `	return t;` |
|       5 |  305 | `}` |
|       - |  306 | `/*` |
|       - |  307 | ` * Return TRUE if the child type is an unambiguously-invalid override of the` |
|       - |  308 | ` * parent type. bCovariant=1 for a return type (child must be ⊆ parent),` |
|       - |  309 | ` * 0 for a parameter (child must be ⊇ parent). Returns FALSE (accept) on any` |
|       - |  310 | ` * skipped/ambiguous shape.` |
|       - |  311 | ` */` |
|  101200 |  312 | `static int OoOverrideTypeBad(ph7_vm *pVm, OvType parent, OvType child, int bCovariant)` |
|       5 |  313 | `{` |
|       - |  314 | `	ph7_class *pParentCls, *pChildCls;` |
|  101205 |  315 | `	int kP = OoClassifyOverrideType(pVm, parent.nType, parent.pClass, parent.bUnion, &pParentCls);` |
|  101205 |  316 | `	int kC = OoClassifyOverrideType(pVm, child.nType, child.pClass, child.bUnion, &pChildCls);` |
|  101205 |  317 | `	if( kP == OVT_SKIP \|\| kC == OVT_SKIP ){` |
|      21 |  318 | `		return 0; /* ambiguous shape — conservatively accept */` |
|       - |  319 | `	}` |
|       - |  320 | `	/* A missing type is the TOP type. covariant (return): a concrete child is a` |
|       - |  321 | `	 * subtype of top, fine; a top child over a concrete parent WIDENS → bad.` |
|       - |  322 | `	 * contravariant (param): a top child is a supertype of anything, fine; a` |
|       - |  323 | `	 * concrete child over a top parent NARROWS → bad. (A union/intersection child` |
|       - |  324 | `	 * already fell into OVT_SKIP above, so a flagged child here is scalar/class.) */` |
|  101187 |  325 | `	if( kP == OVT_NONE \|\| kC == OVT_NONE ){` |
|  101157 |  326 | `		if( bCovariant && kC == OVT_NONE && kP != OVT_NONE ) return 1;` |
|  101157 |  327 | `		if( !bCovariant && kP == OVT_NONE && kC != OVT_NONE ) return 1;` |
|  101157 |  328 | `		return 0;` |
|       - |  329 | `	}` |
|       - |  330 | `	/* Nullability: a covariant return may not ADD null; a contravariant param may` |
|       - |  331 | `	 * not REMOVE null. */` |
|      35 |  332 | `	if( bCovariant ){` |
|      18 |  333 | `		if( child.bNullable && !parent.bNullable ) return 1;` |
|      11 |  334 | `	}else{` |
|      19 |  335 | `		if( parent.bNullable && !child.bNullable ) return 1;` |
|       - |  336 | `	}` |
|      35 |  337 | `	if( kP == OVT_SCALAR && kC == OVT_SCALAR ){` |
|       - |  338 | `		/* Scalars are invariant — they must match exactly. */` |
|      28 |  339 | `		return (parent.nType != child.nType) ? 1 : 0;` |
|       - |  340 | `	}` |
|       8 |  341 | `	if( kP == OVT_CLASS && kC == OVT_CLASS ){` |
|       8 |  342 | `		if( bCovariant ){` |
|       3 |  343 | `			return PH7_VmInstanceOf(pChildCls, pParentCls) ? 0 : 1;  /* child ⊆ parent */` |
|       - |  344 | `		}` |
|       6 |  345 | `		return PH7_VmInstanceOf(pParentCls, pChildCls) ? 0 : 1;      /* child ⊇ parent */` |
|       - |  346 | `	}` |
|       - |  347 | `	/* One scalar and one class — disjoint. */` |
|     ! 0 |  348 | `	return 1;` |
|   50605 |  349 | `}` |
|       - |  350 |  |
|       - |  351 | `/*` |
|       - |  352 | ` * Check a child method's signature against the parent method it overrides.` |
|       - |  353 | ` * Emits a PHP-style "Declaration of … must be compatible …" fatal on a clear` |
|       - |  354 | `` * incompatibility. `__construct` is exempt (PHP does not apply variance to it).`` |
|       - |  355 | ` */` |
|  124504 |  356 | `static sxi32 OoCheckOverrideCompat(ph7_gen_state *pGen, ph7_class *pBase, ph7_class *pSub,` |
|       - |  357 | `	ph7_class_method *pParent, ph7_class_method *pChild)` |
|       5 |  358 | `{` |
|  124509 |  359 | `	ph7_vm *pVm = pGen->pVm;` |
|  124509 |  360 | `	ph7_vm_func *pPF = &pParent->sFunc;` |
|  124509 |  361 | `	ph7_vm_func *pCF = &pChild->sFunc;` |
|  124509 |  362 | `	SyString *pMName = &pCF->sName;` |
|       - |  363 | `	ph7_vm_func_arg *aP, *aC;` |
|       - |  364 | `	sxu32 nPArg, nCArg, k;` |
|  124509 |  365 | `	int bBad = 0;` |
|  124504 |  366 | `	if( pMName->nByte == sizeof("__construct")-1` |
|   83649 |  367 | `	 && SyStrnmicmp(pMName->zString,"__construct",pMName->nByte) == 0 ){` |
|   38893 |  368 | `		return SXRET_OK;` |
|       - |  369 | `	}` |
|       - |  370 | `	/* Return type — covariant. */` |
|   85621 |  371 | `	bBad = OoOverrideTypeBad(pVm, OoTypeFromReturn(pPF), OoTypeFromReturn(pCF), /* bCovariant */ 1);` |
|       - |  372 | `	/* Each overlapping parameter — contravariant. */` |
|   85621 |  373 | `	nPArg = SySetUsed(&pPF->aArgs);` |
|   85621 |  374 | `	nCArg = SySetUsed(&pCF->aArgs);` |
|   85621 |  375 | `	aP = (ph7_vm_func_arg *)SySetBasePtr(&pPF->aArgs);` |
|   85621 |  376 | `	aC = (ph7_vm_func_arg *)SySetBasePtr(&pCF->aArgs);` |
|  101205 |  377 | `	for( k = 0; !bBad && k < nPArg && k < nCArg; k++ ){` |
|   15589 |  378 | `		bBad = OoOverrideTypeBad(pVm, OoTypeFromArg(&aP[k]), OoTypeFromArg(&aC[k]), /* bCovariant */ 0);` |
|    7797 |  379 | `	}` |
|       - |  380 | `	/* Parameter arity: the child must declare at least the parent's parameters and` |
|       - |  381 | `	 * may add only OPTIONAL ones — PHP rejects dropping any param (even an optional` |
|       - |  382 | `	 * one) or adding a required one. Skip the rule if either signature is variadic` |
|       - |  383 | `	 * (arity semantics differ). */` |
|   85621 |  384 | `	if( !bBad ){` |
|   85617 |  385 | `		int bVariadic = 0;` |
|  101199 |  386 | `		for( k = 0; k < nPArg; k++ ){ if( aP[k].iFlags & VM_FUNC_ARG_VARIADIC ) bVariadic = 1; }` |
|  101201 |  387 | `		for( k = 0; k < nCArg; k++ ){ if( aC[k].iFlags & VM_FUNC_ARG_VARIADIC ) bVariadic = 1; }` |
|   85617 |  388 | `		if( !bVariadic ){` |
|   85617 |  389 | `			if( nCArg < nPArg ){` |
|     ! 0 |  390 | `				bBad = 1; /* dropped a parent parameter */` |
|     ! 0 |  391 | `			}else{` |
|   85619 |  392 | `				for( k = nPArg; k < nCArg; k++ ){` |
|       3 |  393 | `					if( SySetUsed(&aC[k].aByteCode) == 0 ){ bBad = 1; break; } /* new required */` |
|       2 |  394 | `				}` |
|       - |  395 | `			}` |
|   42806 |  396 | `		}` |
|   42806 |  397 | `	}` |
|   85621 |  398 | `	if( bBad ){` |
|       8 |  399 | `		sxi32 rc = PH7_GenCompileError(&(*pGen),E_ERROR,pChild->nLine,` |
|       - |  400 | `			"Declaration of %z::%z() must be compatible with %z::%z()",` |
|       2 |  401 | `			&pSub->sName,pMName,&pBase->sName,&pParent->sFunc.sName);` |
|       6 |  402 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  403 | `			return SXERR_ABORT;` |
|       - |  404 | `		}` |
|       2 |  405 | `	}` |
|   85621 |  406 | `	return SXRET_OK;` |
|   62257 |  407 | `}` |
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
|  206352 |  449 | `PH7_PRIVATE sxi32 PH7_ClassInherit(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase)` |
|       5 |  450 | `{` |
|       - |  451 | `	ph7_class_method *pMeth;` |
|       - |  452 | `	ph7_class_attr *pAttr;` |
|       - |  453 | `	SyHashEntry *pEntry;` |
|       - |  454 | `	SyString *pName;` |
|       - |  455 | `	sxi32 rc;` |
|       - |  456 | `	/* Install in the derived hashtable */` |
|  206357 |  457 | `	rc = SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|  206357 |  458 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  459 | `		return rc;` |
|       - |  460 | `	}` |
|       - |  461 | `	/* readonly class inheritance (PHP 8.2): a readonly class may only extend a` |
|       - |  462 | `	 * readonly class, and a non-readonly class may not extend a readonly one. */` |
|  206357 |  463 | `	if( (pBase->iFlags & PH7_CLASS_READONLY) != (pSub->iFlags & PH7_CLASS_READONLY) ){` |
|       5 |  464 | `		if( pBase->iFlags & PH7_CLASS_READONLY ){` |
|       4 |  465 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|       - |  466 | `				"Non-readonly class %z cannot extend readonly class %z",` |
|       1 |  467 | `				&pSub->sName,&pBase->sName);` |
|       2 |  468 | `		}else{` |
|       4 |  469 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|       - |  470 | `				"Readonly class %z cannot extend non-readonly class %z",` |
|       1 |  471 | `				&pSub->sName,&pBase->sName);` |
|       - |  472 | `		}` |
|       5 |  473 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  474 | `			return SXERR_ABORT;` |
|       - |  475 | `		}` |
|       2 |  476 | `	}` |
|       - |  477 | `	/* Copy public/protected attributes from the base class */` |
|  206357 |  478 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
| 1233311 |  479 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|       - |  480 | `		/* Make sure the private attributes are not redeclared in the subclass */` |
| 1026959 |  481 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
| 1026959 |  482 | `		pName = &pAttr->sName;` |
| 1026959 |  483 | `		if( (pEntry = SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|      18 |  484 | `			if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_FINAL))` |
|      12 |  485 | `				== (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_FINAL) ){` |
|       - |  486 | `				/* Cannot override a final class constant (PHP 8.1). Report the` |
|       - |  487 | `				 * class that originally declared it (pDeclClass) rather than the` |
|       - |  488 | `				 * immediate base, so a multi-level chain matches PHP. */` |
|       3 |  489 | `				ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pBase;` |
|       4 |  490 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_attr *)pEntry->pUserData)->nLine,` |
|       - |  491 | `					"%z::%z cannot override final constant %z::%z",` |
|       1 |  492 | `					&pSub->sName,pName,&pOwner->sName,pName);` |
|       3 |  493 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  494 | `					return SXERR_ABORT;` |
|       - |  495 | `				}` |
|       1 |  496 | `			}` |
|       - |  497 | `			/* A child MAY redeclare a base's private property: php treats the two` |
|       - |  498 | `			 * as independent members (each private to its declaring class), with no` |
|       - |  499 | `			 * diagnostic. PH7 warned here, which is wrong — the child's entry simply` |
|       - |  500 | `			 * shadows the base's in the by-name attribute table. */` |
|      21 |  501 | `			continue;` |
|       - |  502 | `		}` |
|       - |  503 | `		/* Install the attribute. php: a base class's private INSTANCE property` |
|       - |  504 | `		 * lives on every child instance too (its own methods read/write it` |
|       - |  505 | `		 * through $this on the child; the access check grants private access by` |
|       - |  506 | `		 * DECLARING class, so child methods and outsiders still can't touch it).` |
|       - |  507 | `		 * Private STATICS/CONSTANTS stay uncopied — base methods reach those` |
|       - |  508 | `		 * through self:: against the declaring class directly. */` |
| 1026936 |  509 | `		if( pAttr->iProtection != PH7_CLASS_PROT_PRIVATE` |
|  546533 |  510 | `		 \|\| (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
| 1026935 |  511 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
| 1026935 |  512 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  513 | `				return rc;` |
|       - |  514 | `			}` |
|  513465 |  515 | `		}` |
|       5 |  516 | `	}` |
|  206357 |  517 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
| 3092247 |  518 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|       - |  519 | `		/* Make sure the private/final methods are not redeclared in the subclass */` |
| 2885895 |  520 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
| 2885895 |  521 | `		pName = &pMeth->sFunc.sName;` |
| 2885895 |  522 | `		if( (pEntry = SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|  124513 |  523 | `			 if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|       - |  524 | `				/* php: "Cannot override final method A::test()" */` |
|       7 |  525 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_method *)pEntry->pUserData)->nLine,` |
|       - |  526 | `					"Cannot override final method %z::%z()",` |
|       2 |  527 | `					&pBase->sName,pName);` |
|       2 |  528 | `				(void)pSub;` |
|       5 |  529 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  530 | `					return SXERR_ABORT;` |
|       - |  531 | `				}` |
|       3 |  532 | `			}else{` |
|       - |  533 | `				/* Check the override's signature is compatible with the parent's. */` |
|  186761 |  534 | `				rc = OoCheckOverrideCompat(&(*pGen),pBase,pSub,pMeth,` |
|  124504 |  535 | `					(ph7_class_method *)pEntry->pUserData);` |
|  124509 |  536 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  537 | `					return SXERR_ABORT;` |
|       - |  538 | `				}` |
|       - |  539 | `			}` |
|  124513 |  540 | `			continue;` |
|       - |  541 | `		}` |
|       - |  542 | `		/* Install the method. php: a base class's private INSTANCE method is` |
|       - |  543 | `		 * dispatchable on child instances too — an inherited public method` |
|       - |  544 | `		 * calling $this->priv() must find it (the call-site visibility check` |
|       - |  545 | `		 * binds by DECLARING class, sFunc.pUserData, so child code and` |
|       - |  546 | `		 * outsiders still can't call it; a private ctor copied down also` |
|       - |  547 | ``		 * blocks `new Child` from outside like php). Private STATICS stay`` |
|       - |  548 | `		 * uncopied — base methods reach those through self:: against the` |
|       - |  549 | `		 * declaring class directly. */` |
| 2761382 |  550 | `		if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE` |
| 1396257 |  551 | `		 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
| 2761387 |  552 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 2761387 |  553 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  554 | `				return rc;` |
|       - |  555 | `			}` |
| 1380691 |  556 | `		}` |
|       5 |  557 | `	}` |
|       - |  558 | `	/* Mark as subclass */` |
|  206357 |  559 | `	pSub->pBase = pBase;` |
|       - |  560 | `	/* All done */` |
|  206357 |  561 | `	return SXRET_OK;` |
|  103181 |  562 | `}` |
|       - |  563 | `/*` |
|       - |  564 | ` * Apply a trait to a class: copy all methods and attributes from the trait` |
|       - |  565 | ` * into the target class. Unlike inheritance, traits copy ALL members including` |
|       - |  566 | ` * private ones. Members already defined in the class take precedence.` |
|       - |  567 | ` */` |
|   15634 |  568 | `PH7_PRIVATE sxi32 PH7_ClassUseTrait(ph7_gen_state *pGen,ph7_class *pClass,ph7_class *pTrait)` |
|       5 |  569 | `{` |
|       - |  570 | `	ph7_class_method *pMeth;` |
|       - |  571 | `	ph7_class_attr *pAttr;` |
|       - |  572 | `	SyHashEntry *pEntry;` |
|       - |  573 | `	SyString *pName;` |
|       - |  574 | `	sxi32 rc;` |
|       - |  575 | `	/* Detect cyclic trait composition (e.g. trait A { use B; } trait B { use A; }) */` |
|   15639 |  576 | `	if( pTrait->iFlags & PH7_CLASS_TRAIT_VISITING ){` |
|     ! 0 |  577 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|     ! 0 |  578 | `			"Trait circular reference detected: %z is already being applied",&pTrait->sName);` |
|     ! 0 |  579 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  580 | `			return SXERR_ABORT;` |
|       - |  581 | `		}` |
|     ! 0 |  582 | `		return SXRET_OK;` |
|       - |  583 | `	}` |
|   15639 |  584 | `	pTrait->iFlags \|= PH7_CLASS_TRAIT_VISITING;` |
|   15639 |  585 | `	rc = SXRET_OK;` |
|       - |  586 | `	/* Copy attributes from the trait */` |
|   15639 |  587 | `	SyHashResetLoopCursor(&pTrait->hAttr);` |
|   62317 |  588 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hAttr)) != 0 ){` |
|       - |  589 | `		SyHashEntry *pExisting;` |
|   46683 |  590 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   46683 |  591 | `		pName = &pAttr->sName;` |
|   46683 |  592 | `		pExisting = SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte);` |
|   46683 |  593 | `		if( pExisting != 0 ){` |
|       - |  594 | `			/* Attribute already exists. Check if it came from another trait` |
|       - |  595 | `			 * and whether the definitions are compatible (same defaults).` |
|       - |  596 | `			 */` |
|       - |  597 | `			ph7_class **apUsedTraits;` |
|       - |  598 | `			sxu32 nUsed,k;` |
|       6 |  599 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       6 |  600 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|       6 |  601 | `			for(k = 0; k < nUsed; k++){` |
|       - |  602 | `				ph7_class_attr *pOther;` |
|       3 |  603 | `				pOther = PH7_ClassExtractAttribute(apUsedTraits[k],pName->zString,pName->nByte);` |
|       3 |  604 | `				if( pOther ){` |
|       - |  605 | `					/* Two traits define the same property — check if defaults differ */` |
|       3 |  606 | `					ph7_class_attr *pClassAttr = (ph7_class_attr *)pExisting->pUserData;` |
|       4 |  607 | `					if( SySetUsed(&pAttr->aByteCode) != SySetUsed(&pClassAttr->aByteCode) \|\|` |
|       3 |  608 | `						(SySetUsed(&pAttr->aByteCode) > 0 &&` |
|       3 |  609 | `						 SyMemcmp(SySetBasePtr(&pAttr->aByteCode),SySetBasePtr(&pClassAttr->aByteCode),` |
|       3 |  610 | `							SySetUsed(&pAttr->aByteCode) * SySetElemSize(&pAttr->aByteCode)) != 0) ){` |
|       4 |  611 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine,` |
|       - |  612 | `							"%z and %z define the same property ($%z) in the composition of %z. "` |
|       - |  613 | `							"However, the definition differs and is considered incompatible",` |
|       2 |  614 | `							&apUsedTraits[k]->sName,&pTrait->sName,pName,&pClass->sName);` |
|       3 |  615 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  616 | `							goto cleanup;` |
|       - |  617 | `						}` |
|       1 |  618 | `					}` |
|       3 |  619 | `					break;` |
|       - |  620 | `				}` |
|     ! 0 |  621 | `			}` |
|       6 |  622 | `			continue;` |
|       - |  623 | `		}` |
|   46679 |  624 | `		rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|   46679 |  625 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  626 | `			goto cleanup;` |
|       - |  627 | `		}` |
|       5 |  628 | `	}` |
|       - |  629 | `	/* Copy methods from the trait */` |
|   15639 |  630 | `	SyHashResetLoopCursor(&pTrait->hMethod);` |
|  225679 |  631 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hMethod)) != 0 ){` |
|       - |  632 | `		SyHashEntry *pClassMethEntry;` |
|  210045 |  633 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  210045 |  634 | `		pName = &pMeth->sFunc.sName;` |
|  210045 |  635 | `		pClassMethEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
|  210045 |  636 | `		if( pClassMethEntry != 0 ){` |
|       - |  637 | `			/* Method already exists in the class. An ABSTRACT trait method is a` |
|       - |  638 | `			 * REQUIREMENT, not an implementation: php satisfies it with any concrete` |
|       - |  639 | `			 * method of the same name (from the class body or another trait) — no` |
|       - |  640 | `			 * collision. Only two CONCRETE trait methods actually conflict. */` |
|      18 |  641 | `			ph7_class_method *pExistingMeth = (ph7_class_method *)pClassMethEntry->pUserData;` |
|      18 |  642 | `			int bIncomingAbstract = (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0;` |
|      18 |  643 | `			int bExistingAbstract = (pExistingMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0;` |
|       - |  644 | `			ph7_class **apUsedTraits;` |
|       - |  645 | `			sxu32 nUsed,k;` |
|      18 |  646 | `			if( bIncomingAbstract ){` |
|       - |  647 | `				/* Incoming abstract requirement: the existing (concrete or abstract)` |
|       - |  648 | `				 * method already covers this name — keep it. */` |
|       8 |  649 | `				continue;` |
|       - |  650 | `			}` |
|      11 |  651 | `			if( bExistingAbstract ){` |
|       - |  652 | `				/* Existing entry is only an abstract requirement (from an earlier` |
|       - |  653 | `				 * trait): the incoming concrete method satisfies and replaces it. */` |
|       3 |  654 | `				pClassMethEntry->pUserData = (void *)pMeth;` |
|       3 |  655 | `				continue;` |
|       - |  656 | `			}` |
|       - |  657 | `			/* Both concrete: a genuine collision only when the OTHER definition came` |
|       - |  658 | `			 * from another trait (a concrete one). A class-body method wins silently. */` |
|       8 |  659 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       8 |  660 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|       8 |  661 | `			for(k = 0; k < nUsed; k++){` |
|       3 |  662 | `				ph7_class_method *pOtherMeth = PH7_ClassExtractMethod(apUsedTraits[k],pName->zString,pName->nByte);` |
|       3 |  663 | `				if( pOtherMeth != 0 && (pOtherMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       - |  664 | `					/* Two different traits define the same CONCRETE method with no resolution */` |
|       4 |  665 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|       - |  666 | `						"Trait method %z::%z has not been applied as %z::%z, "` |
|       - |  667 | `						"because of collision with %z::%z",` |
|       2 |  668 | `						&pTrait->sName,pName,` |
|       1 |  669 | `						&pClass->sName,pName,` |
|       2 |  670 | `						&apUsedTraits[k]->sName,pName);` |
|       3 |  671 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  672 | `						goto cleanup;` |
|       - |  673 | `					}` |
|       3 |  674 | `					break;` |
|       - |  675 | `				}` |
|     ! 0 |  676 | `			}` |
|       - |  677 | `			/* Class-defined method takes precedence */` |
|       8 |  678 | `			continue;` |
|       - |  679 | `		}` |
|  210031 |  680 | `		rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|  210031 |  681 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  682 | `			goto cleanup;` |
|       - |  683 | `		}` |
|       5 |  684 | `	}` |
|       - |  685 | `	/* Record trait in the class */` |
|   15639 |  686 | `	SySetPut(&pClass->aTrait,(const void *)&pTrait);` |
|    7817 |  687 | `cleanup:` |
|       - |  688 | `	/* Always clear visiting flag, even on error paths */` |
|   15639 |  689 | `	pTrait->iFlags &= ~PH7_CLASS_TRAIT_VISITING;` |
|    7817 |  690 | `	SXUNUSED(pGen);` |
|   15639 |  691 | `	return rc;` |
|    7822 |  692 | `}` |
|       - |  693 | `/*` |
|       - |  694 | ` * Inherit an object interface from another object interface.` |
|       - |  695 | ` * According to the PHP language reference manual.` |
|       - |  696 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|       - |  697 | ` *  must implement, without having to define how these methods are handled.` |
|       - |  698 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  699 | ` *  class, but without any of the methods having their contents defined.` |
|       - |  700 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  701 | ` *` |
|       - |  702 | ` * This function return SXRET_OK if the interface inheritance operation was successfully performed.` |
|       - |  703 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|       - |  704 | ` * error message.` |
|       - |  705 | ` */` |
|   27228 |  706 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceInherit(ph7_class *pSub,ph7_class *pBase)` |
|       5 |  707 | `{` |
|       - |  708 | `	ph7_class_method *pMeth;` |
|       - |  709 | `	ph7_class_attr *pAttr;` |
|       - |  710 | `	SyHashEntry *pEntry;` |
|       - |  711 | `	SyString *pName;` |
|       - |  712 | `	sxi32 rc;` |
|       - |  713 | `	/* Install in the derived hashtable */` |
|   27233 |  714 | `	SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|   27233 |  715 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
|       - |  716 | `	/* Copy constants */` |
|   40849 |  717 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|       - |  718 | `		/* Make sure the constants are not redeclared in the subclass */` |
|       3 |  719 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|       3 |  720 | `		pName = &pAttr->sName;` |
|       3 |  721 | `		if( SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte) == 0 ){` |
|       - |  722 | `			/* Install the constant in the subclass */` |
|       3 |  723 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|       3 |  724 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  725 | `				return rc;` |
|       - |  726 | `			}` |
|       1 |  727 | `		}` |
|       1 |  728 | `	}` |
|   27233 |  729 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
|       - |  730 | `	/* Copy methods signature */` |
|  106987 |  731 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|       - |  732 | `		/* Make sure the method are not redeclared in the subclass */` |
|   66145 |  733 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   66145 |  734 | `		pName = &pMeth->sFunc.sName;` |
|   66145 |  735 | `		if( SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte) == 0 ){` |
|       - |  736 | `			/* Install the method */` |
|   66145 |  737 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|   66145 |  738 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  739 | `				return rc;` |
|       - |  740 | `			}` |
|   33070 |  741 | `		}` |
|       5 |  742 | `	}` |
|       - |  743 | `	/* Mark as subclass */` |
|   27233 |  744 | `	pSub->pBase = pBase;` |
|       - |  745 | `	/* All done */` |
|   27233 |  746 | `	return SXRET_OK;` |
|   13619 |  747 | `}` |
|       - |  748 | `/*` |
|       - |  749 | ` * Implements an object interface in the given main class.` |
|       - |  750 | ` * According to the PHP language reference manual.` |
|       - |  751 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|       - |  752 | ` *  must implement, without having to define how these methods are handled.` |
|       - |  753 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  754 | ` *  class, but without any of the methods having their contents defined.` |
|       - |  755 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  756 | ` *` |
|       - |  757 | ` * This function return SXRET_OK if the interface was successfully implemented.` |
|       - |  758 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|       - |  759 | ` * error message.` |
|       - |  760 | ` */` |
|  396928 |  761 | `PH7_PRIVATE sxi32 PH7_ClassImplement(ph7_class *pMain,ph7_class *pInterface)` |
|       5 |  762 | `{` |
|       - |  763 | `	ph7_class_attr *pAttr;` |
|       - |  764 | `	SyHashEntry *pEntry;` |
|       - |  765 | `	SyString *pName;` |
|       - |  766 | `	sxi32 rc;` |
|       - |  767 | `	/* First off,copy all constants declared inside the interface */` |
|  396933 |  768 | `	SyHashResetLoopCursor(&pInterface->hAttr);` |
|  704275 |  769 | `	while((pEntry = SyHashGetNextEntry(&pInterface->hAttr)) != 0 ){` |
|       - |  770 | `		/* Point to the constant declaration */` |
|  108883 |  771 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  108883 |  772 | `		pName = &pAttr->sName;` |
|       - |  773 | `		/* Make sure the attribute is not redeclared in the main class */` |
|  108883 |  774 | `		if( SyHashGet(&pMain->hAttr,pName->zString,pName->nByte) == 0 ){` |
|       - |  775 | `			/* Install the attribute */` |
|  108879 |  776 | `			rc = SyHashInsert(&pMain->hAttr,pName->zString,pName->nByte,pAttr);` |
|  108879 |  777 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  778 | `				return rc;` |
|       - |  779 | `			}` |
|   54437 |  780 | `		}` |
|       5 |  781 | `	}` |
|       - |  782 | `	/* Install in the interface container */` |
|  396933 |  783 | `	SySetPut(&pMain->aInterface,(const void *)&pInterface);` |
|       - |  784 | `	/* Install interface method stubs into the implementing class.` |
|       - |  785 | `	 * Methods already defined in the class take precedence (they satisfy` |
|       - |  786 | `	 * the interface contract). Stubs retain PH7_CLASS_ATTR_ABSTRACT so` |
|       - |  787 | `	 * the unified check in GenStateCheckAbstractMethods catches missing ones.` |
|       - |  788 | `	 */` |
|       - |  789 | `	{` |
|       - |  790 | `		ph7_class_method *pMeth;` |
|       - |  791 | `		SyHashEntry *pMEntry;` |
|       - |  792 | `		SyString *pMName;` |
|  396933 |  793 | `		SyHashResetLoopCursor(&pInterface->hMethod);` |
| 1377489 |  794 | `		while((pMEntry = SyHashGetNextEntry(&pInterface->hMethod)) != 0 ){` |
|  782097 |  795 | `			pMeth = (ph7_class_method *)pMEntry->pUserData;` |
|  782097 |  796 | `			pMName = &pMeth->sFunc.sName;` |
|  782097 |  797 | `			if( SyHashGet(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte) == 0 ){` |
|      25 |  798 | `				rc = SyHashInsert(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte,pMeth);` |
|      25 |  799 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  800 | `					return rc;` |
|       - |  801 | `				}` |
|      10 |  802 | `			}` |
|       5 |  803 | `		}` |
|       - |  804 | `	}` |
|  396933 |  805 | `	return SXRET_OK;` |
|  198469 |  806 | `}` |
|       - |  807 | `/*` |
|       - |  808 | ` * Create a class instance [i.e: Object in the PHP jargon] at run-time.` |
|       - |  809 | ` * The following function is called when an object is created at run-time` |
|       - |  810 | ` * typically when the PH7_OP_NEW/PH7_OP_CLONE instructions are executed.` |
|       - |  811 | ` * Notes on object creation.` |
|       - |  812 | ` *` |
|       - |  813 | ` * According to PHP language reference manual.` |
|       - |  814 | ` * To create an instance of a class, the new keyword must be used. An object will always` |
|       - |  815 | ` * be created unless the object has a constructor defined that throws an exception on error.` |
|       - |  816 | ` * Classes should be defined before instantiation (and in some cases this is a requirement).` |
|       - |  817 | ` * If a string containing the name of a class is used with new, a new instance of that class` |
|       - |  818 | ` * will be created. If the class is in a namespace, its fully qualified name must be used when` |
|       - |  819 | ` * doing this.` |
|       - |  820 | ` * Example #3 Creating an instance` |
|       - |  821 | ` * <?php` |
|       - |  822 | ` *  $instance = new SimpleClass();` |
|       - |  823 | ` *   // This can also be done with a variable:` |
|       - |  824 | ` * $className = 'Foo';` |
|       - |  825 | ` * $instance = new $className(); // Foo()` |
|       - |  826 | ` * ?>` |
|       - |  827 | ` * In the class context, it is possible to create a new object by new self and new parent.` |
|       - |  828 | ` * When assigning an already created instance of a class to a new variable, the new variable` |
|       - |  829 | ` * will access the same instance as the object that was assigned. This behaviour is the same` |
|       - |  830 | ` * when passing instances to a function. A copy of an already created object can be made by` |
|       - |  831 | ` * cloning it.` |
|       - |  832 | ` * Example #4 Object Assignment` |
|       - |  833 | ` * <?php` |
|       - |  834 | ` *  class SimpleClass(){` |
|       - |  835 | ` *    public $var;` |
|       - |  836 | ` *  };` |
|       - |  837 | ` *  $instance = new SimpleClass();` |
|       - |  838 | ` *  $assigned   =  $instance;` |
|       - |  839 | ` *  $reference  =& $instance;` |
|       - |  840 | ` *  $instance->var = '$assigned will have this value';` |
|       - |  841 | ` *  $instance = null; // $instance and $reference become null` |
|       - |  842 | ` *  var_dump($instance);` |
|       - |  843 | ` *  var_dump($reference);` |
|       - |  844 | ` *  var_dump($assigned);` |
|       - |  845 | ` * ?>` |
|       - |  846 | ` * The above example will output:` |
|       - |  847 | ` * NULL` |
|       - |  848 | ` * NULL` |
|       - |  849 | ` * object(SimpleClass)#1 (1) {` |
|       - |  850 | ` *  ["var"]=>` |
|       - |  851 | ` *    string(30) "$assigned will have this value"` |
|       - |  852 | ` * }` |
|       - |  853 | ` * Example #5 Creating new objects` |
|       - |  854 | ` * <?php` |
|       - |  855 | ` * class Test` |
|       - |  856 | ` * {` |
|       - |  857 | ` *   static public function getNew()` |
|       - |  858 | ` *   {` |
|       - |  859 | ` *       return new static;` |
|       - |  860 | ` *   }` |
|       - |  861 | ` * }` |
|       - |  862 | ` * class Child extends Test` |
|       - |  863 | ` * {}` |
|       - |  864 | ` * $obj1 = new Test();` |
|       - |  865 | ` * $obj2 = new $obj1;` |
|       - |  866 | ` * var_dump($obj1 !== $obj2);` |
|       - |  867 | ` * $obj3 = Test::getNew();` |
|       - |  868 | ` * var_dump($obj3 instanceof Test);` |
|       - |  869 | ` * $obj4 = Child::getNew();` |
|       - |  870 | ` * var_dump($obj4 instanceof Child);` |
|       - |  871 | ` * ?>` |
|       - |  872 | ` * The above example will output:` |
|       - |  873 | ` * bool(true)` |
|       - |  874 | ` * bool(true)` |
|       - |  875 | ` * bool(true)` |
|       - |  876 | ` * Note that Symisc Systems have introduced powerfull extension to` |
|       - |  877 | ` * OO subsystem. For example a class attribute may have any complex` |
|       - |  878 | ` * expression associated with it when declaring the attribute unlike` |
|       - |  879 | ` * the standard PHP engine which would allow a single value.` |
|       - |  880 | ` * Example:` |
|       - |  881 | ` *  class myClass{` |
|       - |  882 | ` *    public $var = 25<<1+foo()/bar();` |
|       - |  883 | ` *  };` |
|       - |  884 | ` * Refer to the official documentation for more information.` |
|       - |  885 | ` */` |
|    8254 |  886 | `static ph7_class_instance * NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|       5 |  887 | `{` |
|       - |  888 | `	ph7_class_instance *pThis;` |
|       - |  889 | `	/* Allocate a new instance */` |
|    8259 |  890 | `	pThis = (ph7_class_instance *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_instance));` |
|    8259 |  891 | `	if( pThis == 0 ){` |
|     ! 0 |  892 | `		return 0;` |
|       - |  893 | `	}` |
|       - |  894 | `	/* Zero the structure */` |
|    8259 |  895 | `	SyZero(pThis,sizeof(ph7_class_instance));` |
|       - |  896 | `	/* Initialize fields */` |
|    8259 |  897 | `	pThis->iRef = 1;` |
|    8259 |  898 | `	pThis->pVm = pVm;` |
|    8259 |  899 | `	pThis->pClass = pClass;` |
|       - |  900 | `	/* Assign a fresh monotonic object handle id (clones get their own, like PHP). */` |
|    8259 |  901 | `	pThis->nObjId = pVm->nNextObjId++;` |
|    8259 |  902 | `	SyHashInit(&pThis->hAttr,&pVm->sAllocator,0,0);` |
|    8259 |  903 | `	return pThis;` |
|    4132 |  904 | `}` |
|       - |  905 | `/*` |
|       - |  906 | ` * Wrapper around the NewClassInstance() function defined above.` |
|       - |  907 | ` * See the block comment above for more information.` |
|       - |  908 | ` */` |
|    8012 |  909 | `PH7_PRIVATE ph7_class_instance * PH7_NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|       5 |  910 | `{` |
|       - |  911 | `	ph7_class_instance *pNew;` |
|       - |  912 | `	sxi32 rc;` |
|    8017 |  913 | `	pNew = NewClassInstance(&(*pVm),&(*pClass));` |
|    8017 |  914 | `	if( pNew == 0 ){` |
|     ! 0 |  915 | `		return 0;` |
|       - |  916 | `	}` |
|       - |  917 | `	/* Associate a private VM frame with this class instance */` |
|    8017 |  918 | `	rc = PH7_VmCreateClassInstanceFrame(&(*pVm),pNew);` |
|    8017 |  919 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  920 | `		SyMemBackendPoolFree(&pVm->sAllocator,pNew);` |
|     ! 0 |  921 | `		return 0;` |
|       - |  922 | `	}` |
|       - |  923 | `	/* php stamps a Throwable's file/line at CREATION, not in its constructor, so a` |
|       - |  924 | `	 * subclass that overrides __construct without calling parent::__construct still` |
|       - |  925 | `	 * reports the right site. Every instantiation path lands here. */` |
|    8017 |  926 | `	PH7_VmStampThrowableSite(&(*pVm),pNew);` |
|    8017 |  927 | `	return pNew;` |
|    4011 |  928 | `}` |
|       - |  929 | `/*` |
|       - |  930 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon] attribute.` |
|       - |  931 | ` * This function never fail.` |
|       - |  932 | ` */` |
|   23606 |  933 | `static ph7_value * ExtractClassAttrValue(ph7_vm *pVm,VmClassAttr *pAttr)` |
|       5 |  934 | `{` |
|       - |  935 | `	/* Extract the value */` |
|       - |  936 | `	ph7_value *pValue;` |
|   23611 |  937 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|   23611 |  938 | `	return pValue;` |
|       5 |  939 | `}` |
|       - |  940 | `/*` |
|       - |  941 | ` * Perform a clone operation on a class instance [i.e: Object in the PHP jargon].` |
|       - |  942 | ` * The following function is called when an object is cloned at run-time` |
|       - |  943 | ` * typically when the PH7_OP_CLONE instruction is executed.` |
|       - |  944 | ` * Notes on object cloning.` |
|       - |  945 | ` *` |
|       - |  946 | ` * According to PHP language reference manual.` |
|       - |  947 | ` * Creating a copy of an object with fully replicated properties is not always the wanted behavior.` |
|       - |  948 | ` * A good example of the need for copy constructors. Another example is if your object holds a reference` |
|       - |  949 | ` * to another object which it uses and when you replicate the parent object you want to create` |
|       - |  950 | ` * a new instance of this other object so that the replica has its own separate copy.` |
|       - |  951 | ` * An object copy is created by using the clone keyword (which calls the object's __clone() method if possible).` |
|       - |  952 | ` * An object's __clone() method cannot be called directly.` |
|       - |  953 | ` * $copy_of_object = clone $object;` |
|       - |  954 | ` * When an object is cloned, PHP 5 will perform a shallow copy of all of the object's properties.` |
|       - |  955 | ` * Any properties that are references to other variables, will remain references.` |
|       - |  956 | ` * Once the cloning is complete, if a __clone() method is defined, then the newly created object's __clone() method` |
|       - |  957 | ` * will be called, to allow any necessary properties that need to be changed.` |
|       - |  958 | ` * Example #1 Cloning an object` |
|       - |  959 | ` * <?php` |
|       - |  960 | ` * class SubObject` |
|       - |  961 | ` * {` |
|       - |  962 | ` *   static $instances = 0;` |
|       - |  963 | ` *   public $instance;` |
|       - |  964 | ` *` |
|       - |  965 | ` *   public function __construct() {` |
|       - |  966 | ` *       $this->instance = ++self::$instances;` |
|       - |  967 | ` *   }` |
|       - |  968 | ` *` |
|       - |  969 | ` *   public function __clone() {` |
|       - |  970 | ` *       $this->instance = ++self::$instances;` |
|       - |  971 | ` *   }` |
|       - |  972 | ` * }` |
|       - |  973 | ` *` |
|       - |  974 | ` * class MyCloneable` |
|       - |  975 | ` * {` |
|       - |  976 | ` *   public $object1;` |
|       - |  977 | ` *   public $object2;` |
|       - |  978 | ` *` |
|       - |  979 | ` *   function __clone()` |
|       - |  980 | ` *   {` |
|       - |  981 | ` *       // Force a copy of this->object, otherwise` |
|       - |  982 | ` *       // it will point to same object.` |
|       - |  983 | ` *       $this->object1 = clone $this->object1;` |
|       - |  984 | ` *   }` |
|       - |  985 | ` * }` |
|       - |  986 | ` * $obj = new MyCloneable();` |
|       - |  987 | ` * $obj->object1 = new SubObject();` |
|       - |  988 | ` * $obj->object2 = new SubObject();` |
|       - |  989 | ` * $obj2 = clone $obj;` |
|       - |  990 | ` * print("Original Object:\n");` |
|       - |  991 | ` * print_r($obj);` |
|       - |  992 | ` * print("Cloned Object:\n");` |
|       - |  993 | ` * print_r($obj2);` |
|       - |  994 | ` * ?>` |
|       - |  995 | ` * The above example will output:` |
|       - |  996 | ` * Original Object:` |
|       - |  997 | ` * MyCloneable Object` |
|       - |  998 | ` * (` |
|       - |  999 | ` *   [object1] => SubObject Object` |
|       - | 1000 | ` *       (` |
|       - | 1001 | ` *           [instance] => 1` |
|       - | 1002 | ` *       )` |
|       - | 1003 | ` *` |
|       - | 1004 | ` *   [object2] => SubObject Object` |
|       - | 1005 | ` *       (` |
|       - | 1006 | ` *           [instance] => 2` |
|       - | 1007 | ` *       )` |
|       - | 1008 | ` *` |
|       - | 1009 | ` * )` |
|       - | 1010 | ` * Cloned Object:` |
|       - | 1011 | ` * MyCloneable Object` |
|       - | 1012 | ` * (` |
|       - | 1013 | ` *   [object1] => SubObject Object` |
|       - | 1014 | ` *       (` |
|       - | 1015 | ` *           [instance] => 3` |
|       - | 1016 | ` *       )` |
|       - | 1017 | ` *` |
|       - | 1018 | ` *   [object2] => SubObject Object` |
|       - | 1019 | ` *       (` |
|       - | 1020 | ` *           [instance] => 2` |
|       - | 1021 | ` *       )` |
|       - | 1022 | ` * )` |
|       - | 1023 | ` */` |
|     242 | 1024 | `PH7_PRIVATE ph7_class_instance * PH7_CloneClassInstance(ph7_class_instance *pSrc)` |
|       4 | 1025 | `{` |
|       - | 1026 | `	ph7_class_instance *pClone;` |
|       - | 1027 | `	ph7_class_method *pMethod;` |
|       - | 1028 | `	SyHashEntry *pEntry2;` |
|       - | 1029 | `	SyHashEntry *pEntry;` |
|       - | 1030 | `	ph7_vm *pVm;` |
|       - | 1031 | `	sxi32 rc;` |
|       - | 1032 | `	/* Allocate a new instance */` |
|     246 | 1033 | `	pVm = pSrc->pVm;` |
|     246 | 1034 | `	pClone = NewClassInstance(pVm,pSrc->pClass);` |
|     246 | 1035 | `	if( pClone == 0 ){` |
|     ! 0 | 1036 | `		return 0;` |
|       - | 1037 | `	}` |
|       - | 1038 | `	/* Associate a private VM frame with this class instance */` |
|     246 | 1039 | `	rc = PH7_VmCreateClassInstanceFrame(pVm,pClone);` |
|     246 | 1040 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1041 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClone);` |
|     ! 0 | 1042 | `		return 0;` |
|       - | 1043 | `	}` |
|       - | 1044 | `	/* Duplicate object values. Iterate the SOURCE attributes and copy each into` |
|       - | 1045 | `	 * the clone's same-named slot (looked up by name, so order/count differences` |
|       - | 1046 | `	 * from dynamic properties don't matter). A dynamic (runtime-added) property` |
|       - | 1047 | `	 * has no declared counterpart in the clone, so synthesize it first — without` |
|       - | 1048 | `	 * this, a clone of a stdClass would silently lose all its dynamic properties. */` |
|     246 | 1049 | `	SyHashResetLoopCursor(&pSrc->hAttr);` |
|    2634 | 1050 | `	while((pEntry = SyHashGetNextEntry(&pSrc->hAttr)) != 0 ){` |
|    2392 | 1051 | `		VmClassAttr *pSrcAttr = (VmClassAttr *)pEntry->pUserData;` |
|    2392 | 1052 | `		VmClassAttr *pDestAttr = 0;` |
|    2392 | 1053 | `		ph7_value *pvSrc,*pvDest = 0;` |
|       - | 1054 | `		/* Duplicate non-static attribute */` |
|    2392 | 1055 | `		if( pSrcAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|    1693 | 1056 | `			continue;` |
|       - | 1057 | `		}` |
|     700 | 1058 | `		pEntry2 = SyHashGet(&pClone->hAttr,SyStringData(&pSrcAttr->pAttr->sName),SyStringLength(&pSrcAttr->pAttr->sName));` |
|     700 | 1059 | `		if( pEntry2 ){` |
|     686 | 1060 | `			pDestAttr = (VmClassAttr *)pEntry2->pUserData;` |
|     686 | 1061 | `			pvDest = ExtractClassAttrValue(pVm,pDestAttr);` |
|     356 | 1062 | `		}else if( pSrcAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){` |
|       - | 1063 | `			/* Dynamic property: synthesize the matching slot on the clone. */` |
|      22 | 1064 | `			pvDest = PH7_VmCreateDynamicAttr(pVm,pClone,` |
|      14 | 1065 | `				SyStringData(&pSrcAttr->pAttr->sName),SyStringLength(&pSrcAttr->pAttr->sName),&pDestAttr);` |
|       7 | 1066 | `		}` |
|       - | 1067 | `		/* Fetch the source value LAST: PH7_VmCreateDynamicAttr above may have` |
|       - | 1068 | `		 * reserved a slot and reallocated pVm->aMemObj, which would dangle any` |
|       - | 1069 | `		 * ph7_value* obtained before it. pvDest from the synth path already points` |
|       - | 1070 | `		 * into the post-realloc aMemObj; resolve pvSrc now so both are current. */` |
|     700 | 1071 | `		pvSrc = ExtractClassAttrValue(pVm,pSrcAttr);` |
|     700 | 1072 | `		if( pvSrc && pvDest ){` |
|     700 | 1073 | `			PH7_MemObjStore(pvSrc,pvDest);` |
|     348 | 1074 | `		}` |
|       - | 1075 | `		/* Carry over the per-instance state so the clone matches the source:` |
|       - | 1076 | `		 * VM_CLASS_ATTR_UNINIT marks a typed property as not-yet-initialized` |
|       - | 1077 | `		 * and doubles as the readonly write-once latch — without this a clone` |
|       - | 1078 | `		 * would reset to uninitialized (losing the value's readiness) and a` |
|       - | 1079 | `		 * readonly property would become writable again. */` |
|     700 | 1080 | `		if( pDestAttr ){` |
|     700 | 1081 | `			pDestAttr->iState = pSrcAttr->iState;` |
|     348 | 1082 | `		}` |
|       4 | 1083 | `	}` |
|       - | 1084 | `	/* A declared property unset() on the source is absent from the clone too (PHP). But the clone` |
|       - | 1085 | `	 * frame above materialized ALL declared attrs (with their defaults), so drop any clone attr whose` |
|       - | 1086 | `	 * name is not present on the source. Collect first, then delete — removing an entry mid-walk would` |
|       - | 1087 | `	 * free the node the SyHash loop cursor points at. */` |
|       - | 1088 | `	{` |
|       - | 1089 | `		SySet sDrop;` |
|     246 | 1090 | `		SySetInit(&sDrop,&pVm->sAllocator,sizeof(VmClassAttr *));` |
|     246 | 1091 | `		SyHashResetLoopCursor(&pClone->hAttr);` |
|    2636 | 1092 | `		while((pEntry = SyHashGetNextEntry(&pClone->hAttr)) != 0 ){` |
|    2394 | 1093 | `			VmClassAttr *pCloneAttr = (VmClassAttr *)pEntry->pUserData;` |
|    2394 | 1094 | `			if( pCloneAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|    1693 | 1095 | `				continue;` |
|       - | 1096 | `			}` |
|    1047 | 1097 | `			if( SyHashGet(&pSrc->hAttr,SyStringData(&pCloneAttr->pAttr->sName),` |
|    1051 | 1098 | `					SyStringLength(&pCloneAttr->pAttr->sName)) == 0 ){` |
|       3 | 1099 | `				SySetPut(&sDrop,(const void *)&pCloneAttr);` |
|       1 | 1100 | `			}` |
|       4 | 1101 | `		}` |
|     246 | 1102 | `		if( SySetUsed(&sDrop) > 0 ){` |
|       3 | 1103 | `			VmClassAttr **apDrop = (VmClassAttr **)SySetBasePtr(&sDrop);` |
|       - | 1104 | `			sxu32 i;` |
|       5 | 1105 | `			for( i = 0 ; i < SySetUsed(&sDrop) ; ++i ){` |
|       3 | 1106 | `				VmClassAttr *pVmAttr = apDrop[i];` |
|       4 | 1107 | `				SyHashDeleteEntry(&pClone->hAttr,SyStringData(&pVmAttr->pAttr->sName),` |
|       2 | 1108 | `					SyStringLength(&pVmAttr->pAttr->sName),0);` |
|       3 | 1109 | `				PH7_VmReleaseInstanceAttr(pVm,pVmAttr);` |
|       2 | 1110 | `			}` |
|       1 | 1111 | `		}` |
|     246 | 1112 | `		SySetRelease(&sDrop);` |
|       - | 1113 | `	}` |
|       - | 1114 | `	/* call the __clone method on the cloned object if available */` |
|     246 | 1115 | `	pMethod = PH7_ClassExtractMethod(pClone->pClass,"__clone",sizeof("__clone")-1);` |
|     246 | 1116 | `	if( pMethod ){` |
|      58 | 1117 | `		if( pMethod->iCloneDepth < 16 ){` |
|      56 | 1118 | `			pMethod->iCloneDepth++;` |
|       - | 1119 | `			/* PHP 8.3: __clone() may re-initialize the clone's readonly` |
|       - | 1120 | `			 * properties. Flag the instance so the readonly store guard allows` |
|       - | 1121 | `			 * it for the duration of the call. */` |
|      56 | 1122 | `			pClone->iFlags \|= VM_INSTANCE_CLONING;` |
|      56 | 1123 | `			PH7_VmCallClassMethod(pVm,pClone,pMethod,0,0,0);` |
|      56 | 1124 | `			pClone->iFlags &= ~VM_INSTANCE_CLONING;` |
|      29 | 1125 | `		}else{` |
|       - | 1126 | `			/* Nesting limit reached */` |
|       3 | 1127 | `			PH7_VmThrowError(pVm,0,PH7_CTX_ERR,"Object clone limit reached,no more call to __clone()");` |
|       - | 1128 | `		}` |
|       - | 1129 | `		/* Reset the cursor */` |
|      58 | 1130 | `		pMethod->iCloneDepth = 0;` |
|      28 | 1131 | `	}` |
|       - | 1132 | `	/* Return the cloned object */` |
|     246 | 1133 | `	return pClone;` |
|     125 | 1134 | `}` |
|       - | 1135 | `#define CLASS_INSTANCE_DESTROYED 0x001 /* Instance is released */` |
|       - | 1136 | `/*` |
|       - | 1137 | ` * Free the per-instance allocations owned by ONE object attribute: its value slot (+ the typed-slot` |
|       - | 1138 | ` * enforcement entry), the synthesized ph7_class_attr for a dynamic (runtime-added) property, and the` |
|       - | 1139 | ` * VmClassAttr wrapper itself. Does NOT touch the hAttr entry node — the caller removes it` |
|       - | 1140 | `` * (`unset($o->p)` via SyHashDeleteEntry2; instance teardown via the wholesale SyHashRelease, so it must`` |
|       - | 1141 | ` * not delete entries mid-walk). Shared by PH7_ClassInstanceRelease and the OP_MEMBER unset path.` |
|       - | 1142 | ` */` |
|   27748 | 1143 | `PH7_PRIVATE void PH7_VmReleaseInstanceAttr(ph7_vm *pVm, VmClassAttr *pVmAttr)` |
|       5 | 1144 | `{` |
|   27753 | 1145 | `	if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|       - | 1146 | `		/* Drop any typed-property enforcement slot registered for this memobj, before the memobj` |
|       - | 1147 | `		 * is returned to the free list, so a future recycled slot does not inherit the stale entry. */` |
|   20461 | 1148 | `		if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|     309 | 1149 | `			SyHashDeleteEntry(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),0);` |
|     152 | 1150 | `		}` |
|   20461 | 1151 | `		PH7_VmUnsetMemObj(pVm,pVmAttr->nIdx,TRUE);` |
|   10228 | 1152 | `	}` |
|       - | 1153 | `	/* A dynamic property owns its synthesized ph7_class_attr (struct + inline name in one block) —` |
|       - | 1154 | `	 * free it here (the only place a per-instance pAttr is freed; declared attrs are class-owned). */` |
|   27753 | 1155 | `	if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){` |
|     120 | 1156 | `		SyMemBackendFree(&pVm->sAllocator,pVmAttr->pAttr);` |
|      59 | 1157 | `	}` |
|   27753 | 1158 | `	SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|   27753 | 1159 | `}` |
|       - | 1160 | `/*` |
|       - | 1161 | ` * Release a class instance [i.e: Object in the PHP jargon] and invoke any defined destructor.` |
|       - | 1162 | ` * This routine is invoked as soon as there are no other references to a particular` |
|       - | 1163 | ` * class instance.` |
|       - | 1164 | ` */` |
|    5546 | 1165 | `static void PH7_ClassInstanceRelease(ph7_class_instance *pThis)` |
|       5 | 1166 | `{` |
|       - | 1167 | `	ph7_class_method *pDestr;` |
|       - | 1168 | `	SyHashEntry *pEntry;` |
|       - | 1169 | `	ph7_class *pClass;` |
|       - | 1170 | `	ph7_vm *pVm;` |
|    5551 | 1171 | `	if( pThis->iFlags & CLASS_INSTANCE_DESTROYED ){` |
|       - | 1172 | `		/*` |
|       - | 1173 | `		 * Already destroyed,return immediately.` |
|       - | 1174 | `		 * This could happend if someone perform unset($this) in the destructor body.` |
|       - | 1175 | `		 */` |
|     ! 0 | 1176 | `		return;` |
|       - | 1177 | `	}` |
|       - | 1178 | `	/* Mark as destroyed */` |
|    5551 | 1179 | `	pThis->iFlags \|= CLASS_INSTANCE_DESTROYED;` |
|       - | 1180 | `	/* Invoke any defined destructor if available */` |
|    5551 | 1181 | `	pVm = pThis->pVm;` |
|    5551 | 1182 | `	pClass = pThis->pClass;` |
|    5551 | 1183 | `	pDestr = PH7_ClassExtractMethod(pClass,"__destruct",sizeof("__destruct")-1);` |
|    5551 | 1184 | `	if( pDestr && !pVm->bInReset ){` |
|       - | 1185 | `		/* Invoke the destructor. Skipped during ph7_vm_reset() bulk teardown:` |
|       - | 1186 | `		 * running user PHP against a half-reset VM is unsafe (see bInReset). */` |
|     471 | 1187 | `		pThis->iRef = 2; /* Prevent garbage collection */` |
|     471 | 1188 | `		PH7_VmCallClassMethod(pVm,pThis,pDestr,0,0,0);` |
|     233 | 1189 | `	}` |
|       - | 1190 | `	/* Weak-reference registry: kill the cell for this instance so every` |
|       - | 1191 | `	 * WeakReference/WeakMap handle observes the death (the cell outlives the` |
|       - | 1192 | `	 * instance until its own handles drop; removing the hash entry here keeps` |
|       - | 1193 | `	 * a pool-reused address from resurrecting a dead cell). */` |
|    5551 | 1194 | `	if( SyHashTotalEntry(&pVm->hWeakCell) > 0 ){` |
|      27 | 1195 | `		void *pCellData = 0;` |
|      26 | 1196 | `		if( SyHashDeleteEntry(&pVm->hWeakCell,(const void *)&pThis,sizeof(void *),&pCellData) == SXRET_OK` |
|      18 | 1197 | `		 && pCellData ){` |
|       9 | 1198 | `			((VmWeakCell *)pCellData)->pObj = 0;` |
|       4 | 1199 | `		}` |
|      13 | 1200 | `	}` |
|       - | 1201 | `	/* Release non-static attributes (the wholesale SyHashRelease below frees the entry nodes,` |
|       - | 1202 | `	 * so the helper must not delete them mid-walk). */` |
|    5551 | 1203 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|   33271 | 1204 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|   27725 | 1205 | `		PH7_VmReleaseInstanceAttr(pVm,(VmClassAttr *)pEntry->pUserData);` |
|       5 | 1206 | `	}` |
|       - | 1207 | `	/* Release the whole structure */` |
|    5551 | 1208 | `	SyHashRelease(&pThis->hAttr);` |
|    5551 | 1209 | `	SyMemBackendPoolFree(&pVm->sAllocator,pThis);` |
|    2778 | 1210 | `}` |
|       - | 1211 | `/*` |
|       - | 1212 | ` * Decrement the reference count of a class instance [i.e Object in the PHP jargon].` |
|       - | 1213 | ` * If the reference count reaches zero,release the whole instance.` |
|       - | 1214 | ` */` |
|  130530 | 1215 | `PH7_PRIVATE void PH7_ClassInstanceUnref(ph7_class_instance *pThis)` |
|       5 | 1216 | `{` |
|  130535 | 1217 | `	pThis->iRef--;` |
|  130535 | 1218 | `	if( pThis->iRef < 1 ){` |
|       - | 1219 | `		/* No more reference to this instance */` |
|    5551 | 1220 | `		PH7_ClassInstanceRelease(&(*pThis));` |
|    2773 | 1221 | `	}` |
|  130535 | 1222 | `}` |
|       - | 1223 | `/*` |
|       - | 1224 | ` * Compare two class instances [i.e: Objects in the PHP jargon]` |
|       - | 1225 | ` * Note on objects comparison:` |
|       - | 1226 | ` *  According to the PHP langauge reference manual` |
|       - | 1227 | ` *  When using the comparison operator (==), object variables are compared in a simple manner` |
|       - | 1228 | ` *  namely: Two object instances are equal if they have the same attributes and values, and are` |
|       - | 1229 | ` *  instances of the same class.` |
|       - | 1230 | ` *  On the other hand, when using the identity operator (===), object variables are identical` |
|       - | 1231 | ` *  if and only if they refer to the same instance of the same class.` |
|       - | 1232 | ` *  An example will clarify these rules.` |
|       - | 1233 | ` *  Example #1 Example of object comparison` |
|       - | 1234 | ` *  <?php` |
|       - | 1235 | ` *    function bool2str($bool)` |
|       - | 1236 | ` * {` |
|       - | 1237 | ` *   if ($bool === false) {` |
|       - | 1238 | ` *       return 'FALSE';` |
|       - | 1239 | ` *   } else {` |
|       - | 1240 | ` *       return 'TRUE';` |
|       - | 1241 | ` *   }` |
|       - | 1242 | ` * }` |
|       - | 1243 | ` * function compareObjects(&$o1, &$o2)` |
|       - | 1244 | ` * {` |
|       - | 1245 | ` *   echo 'o1 == o2 : ' . bool2str($o1 == $o2) . "\n";` |
|       - | 1246 | ` *   echo 'o1 != o2 : ' . bool2str($o1 != $o2) . "\n";` |
|       - | 1247 | ` *   echo 'o1 === o2 : ' . bool2str($o1 === $o2) . "\n";` |
|       - | 1248 | ` *   echo 'o1 !== o2 : ' . bool2str($o1 !== $o2) . "\n";` |
|       - | 1249 | ` * }` |
|       - | 1250 | ` * class Flag` |
|       - | 1251 | ` * {` |
|       - | 1252 | ` *   public $flag;` |
|       - | 1253 | ` *` |
|       - | 1254 | ` *   function Flag($flag = true) {` |
|       - | 1255 | ` *       $this->flag = $flag;` |
|       - | 1256 | ` *   }` |
|       - | 1257 | ` * }` |
|       - | 1258 | ` *` |
|       - | 1259 | ` * class OtherFlag` |
|       - | 1260 | ` * {` |
|       - | 1261 | ` *   public $flag;` |
|       - | 1262 | ` *` |
|       - | 1263 | ` *   function OtherFlag($flag = true) {` |
|       - | 1264 | ` *       $this->flag = $flag;` |
|       - | 1265 | ` *   }` |
|       - | 1266 | ` * }` |
|       - | 1267 | ` *` |
|       - | 1268 | ` * $o = new Flag();` |
|       - | 1269 | ` * $p = new Flag();` |
|       - | 1270 | ` * $q = $o;` |
|       - | 1271 | ` * $r = new OtherFlag();` |
|       - | 1272 | ` *` |
|       - | 1273 | ` * echo "Two instances of the same class\n";` |
|       - | 1274 | ` * compareObjects($o, $p);` |
|       - | 1275 | ` * echo "\nTwo references to the same instance\n";` |
|       - | 1276 | ` * compareObjects($o, $q);` |
|       - | 1277 | ` * echo "\nInstances of two different classes\n";` |
|       - | 1278 | ` * compareObjects($o, $r);` |
|       - | 1279 | ` * ?>` |
|       - | 1280 | ` * The above example will output:` |
|       - | 1281 | ` * Two instances of the same class` |
|       - | 1282 | ` * o1 == o2 : TRUE` |
|       - | 1283 | ` * o1 != o2 : FALSE` |
|       - | 1284 | ` * o1 === o2 : FALSE` |
|       - | 1285 | ` * o1 !== o2 : TRUE` |
|       - | 1286 | ` * Two references to the same instance` |
|       - | 1287 | ` * o1 == o2 : TRUE` |
|       - | 1288 | ` * o1 != o2 : FALSE` |
|       - | 1289 | ` * o1 === o2 : TRUE` |
|       - | 1290 | ` * o1 !== o2 : FALSE` |
|       - | 1291 | ` * Instances of two different classes` |
|       - | 1292 | ` * o1 == o2 : FALSE` |
|       - | 1293 | ` * o1 != o2 : TRUE` |
|       - | 1294 | ` * o1 === o2 : FALSE` |
|       - | 1295 | ` * o1 !== o2 : TRUE` |
|       - | 1296 | ` *` |
|       - | 1297 | ` * This function return 0 if the objects are equals according to the comprison rules defined above.` |
|       - | 1298 | ` * Any other return values indicates difference.` |
|       - | 1299 | ` */` |
|     272 | 1300 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest)` |
|       5 | 1301 | `{` |
|       - | 1302 | `	SyHashEntry *pEntry,*pEntry2;` |
|       - | 1303 | `	ph7_value sV1,sV2;` |
|       - | 1304 | `	sxi32 rc;` |
|     277 | 1305 | `	if( iNest > 31 ){` |
|       - | 1306 | `		/* Nesting limit reached */` |
|       6 | 1307 | `		PH7_VmThrowError(pLeft->pVm,0,PH7_CTX_ERR,"Nesting limit reached: Infinite recursion?");` |
|       6 | 1308 | `		return 1;` |
|       - | 1309 | `	}` |
|       - | 1310 | `	/* Comparison is performed only if the objects are instance of the same class */` |
|     273 | 1311 | `	if( pLeft->pClass != pRight->pClass ){` |
|       7 | 1312 | `		return 1;` |
|       - | 1313 | `	}` |
|     267 | 1314 | `	if( bStrict ){` |
|       - | 1315 | `		/*` |
|       - | 1316 | `		 * According to the PHP language reference manual:` |
|       - | 1317 | `		 *  when using the identity operator (===), object variables` |
|       - | 1318 | `		 *  are identical if and only if they refer to the same instance` |
|       - | 1319 | `		 *  of the same class.` |
|       - | 1320 | `		 */` |
|     103 | 1321 | `		return !(pLeft == pRight);` |
|       - | 1322 | `	}` |
|       - | 1323 | `	/*` |
|       - | 1324 | `	 * Attribute comparison.` |
|       - | 1325 | `	 * According to the PHP reference manual:` |
|       - | 1326 | `	 *  When using the comparison operator (==), object variables are compared` |
|       - | 1327 | `	 *  in a simple manner, namely: Two object instances are equal if they have` |
|       - | 1328 | `	 *  the same attributes and values, and are instances of the same class.` |
|       - | 1329 | `	 */` |
|     168 | 1330 | `	if( pLeft == pRight ){` |
|       - | 1331 | `		/* Same instance,don't bother processing,object are equals */` |
|       5 | 1332 | `		return 0;` |
|       - | 1333 | `	}` |
|       - | 1334 | `	/* Closures compare by IDENTITY under == as well (not by attributes): two distinct` |
|       - | 1335 | `	 * Closure instances are never equal, even when they wrap the same underlying function` |
|       - | 1336 | `	 * (PHP semantics). pLeft != pRight here, so a Closure pair is unequal. Without this,` |
|       - | 1337 | `` 	 * two capture-less lambdas of the same `function(){}` share the template's `$__fn` `` |
|       - | 1338 | `	 * name and would compare equal. */` |
|     164 | 1339 | `	if( pLeft->pVm->pClosureClass && pLeft->pClass == pLeft->pVm->pClosureClass ){` |
|       5 | 1340 | `		return 1;` |
|       - | 1341 | `	}` |
|       - | 1342 | `	/* Same class but a different number of attributes ⇒ different property sets` |
|       - | 1343 | `	 * (dynamic properties can give two same-class instances different counts). */` |
|     160 | 1344 | `	if( pLeft->hAttr.nEntry != pRight->hAttr.nEntry ){` |
|       3 | 1345 | `		return 1;` |
|       - | 1346 | `	}` |
|     158 | 1347 | `	PH7_MemObjInit(pLeft->pVm,&sV1);` |
|     158 | 1348 | `	PH7_MemObjInit(pLeft->pVm,&sV2);` |
|     158 | 1349 | `	sV1.nIdx = sV2.nIdx = SXU32_HIGH;` |
|       - | 1350 | `	/* Compare each left attribute against the RIGHT attribute of the SAME NAME` |
|       - | 1351 | `	 * (not in lockstep): dynamic properties may be stored in a different order` |
|       - | 1352 | `	 * on the two instances. Counts already match, so if every left attribute has` |
|       - | 1353 | `	 * an equal-valued same-named right attribute the property sets are equal. */` |
|     158 | 1354 | `	SyHashResetLoopCursor(&pLeft->hAttr);` |
|     194 | 1355 | `	while((pEntry = SyHashGetNextEntry(&pLeft->hAttr)) != 0 ){` |
|     170 | 1356 | `		VmClassAttr *p1 = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1357 | `		VmClassAttr *p2;` |
|       - | 1358 | `		ph7_value *pL,*pR;` |
|       - | 1359 | `		/* Compare only non-static attribute */` |
|     170 | 1360 | `		if( p1->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|     ! 0 | 1361 | `			continue;` |
|       - | 1362 | `		}` |
|     170 | 1363 | `		pEntry2 = SyHashGet(&pRight->hAttr,SyStringData(&p1->pAttr->sName),SyStringLength(&p1->pAttr->sName));` |
|     170 | 1364 | `		if( pEntry2 == 0 ){` |
|       - | 1365 | `			/* Left has a property the right lacks ⇒ not equal. */` |
|     ! 0 | 1366 | `			return 1;` |
|       - | 1367 | `		}` |
|     170 | 1368 | `		p2 = (VmClassAttr *)pEntry2->pUserData;` |
|     170 | 1369 | `		pL = ExtractClassAttrValue(pLeft->pVm,p1);` |
|     170 | 1370 | `		pR = ExtractClassAttrValue(pRight->pVm,p2);` |
|     170 | 1371 | `		if( pL && pR ){` |
|     170 | 1372 | `			PH7_MemObjLoad(pL,&sV1);` |
|     170 | 1373 | `			PH7_MemObjLoad(pR,&sV2);` |
|       - | 1374 | `			/* Compare the two values now */` |
|     170 | 1375 | `			rc = PH7_MemObjCmp(&sV1,&sV2,bStrict,iNest+1);` |
|     170 | 1376 | `			PH7_MemObjRelease(&sV1);` |
|     170 | 1377 | `			PH7_MemObjRelease(&sV2);` |
|     170 | 1378 | `			if( rc != 0 ){` |
|       - | 1379 | `				/* Not equals */` |
|     133 | 1380 | `				return rc;` |
|       - | 1381 | `			}` |
|      18 | 1382 | `		}` |
|       2 | 1383 | `	}` |
|       - | 1384 | `	/* Object are equals */` |
|      26 | 1385 | `	return 0;` |
|     141 | 1386 | `}` |
|       - | 1387 | `/*` |
|       - | 1388 | ` * Dump a class instance and the store the dump in the BLOB given` |
|       - | 1389 | ` * as the first argument.` |
|       - | 1390 | ` * Note that only non-static/non-constants attribute are dumped.` |
|       - | 1391 | ` * This function is typically invoked when the user issue a call` |
|       - | 1392 | ` * to [var_dump(),var_export(),print_r(),...].` |
|       - | 1393 | ` * This function SXRET_OK on success. Any other return value including` |
|       - | 1394 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|       - | 1395 | ` */` |
|       - | 1396 | `/*` |
|       - | 1397 | `` * Return the `name` property value of an enum case instance (the case name),`` |
|       - | 1398 | ` * or 0 when unavailable. Shared by the var_dump/var_export/json/serialize` |
|       - | 1399 | ` * renderers, which all print enum cases as Class::CaseName forms.` |
|       - | 1400 | ` */` |
|       6 | 1401 | `PH7_PRIVATE ph7_value * PH7_EnumCaseNameValue(ph7_class_instance *pThis)` |
|       1 | 1402 | `{` |
|       - | 1403 | `	SyHashEntry *pEntry;` |
|       7 | 1404 | `	if( (pThis->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|     ! 0 | 1405 | `		return 0;` |
|       - | 1406 | `	}` |
|       7 | 1407 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)"name",sizeof("name")-1);` |
|       7 | 1408 | `	if( pEntry == 0 ){` |
|     ! 0 | 1409 | `		return 0;` |
|       - | 1410 | `	}` |
|       7 | 1411 | `	return PH7_ClassInstanceExtractAttrValue(pThis,(VmClassAttr *)pEntry->pUserData);` |
|       4 | 1412 | `}` |
|       - | 1413 | `/*` |
|       - | 1414 | `` * Return the `value` property value (the backing value) of an enum case`` |
|       - | 1415 | ` * instance, or 0 when unavailable (pure enums have none).` |
|       - | 1416 | ` */` |
|       8 | 1417 | `PH7_PRIVATE ph7_value * PH7_EnumCaseBackingValueOf(ph7_class_instance *pThis)` |
|       1 | 1418 | `{` |
|       - | 1419 | `	SyHashEntry *pEntry;` |
|       9 | 1420 | `	if( (pThis->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|     ! 0 | 1421 | `		return 0;` |
|       - | 1422 | `	}` |
|       9 | 1423 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)"value",sizeof("value")-1);` |
|       9 | 1424 | `	if( pEntry == 0 ){` |
|       3 | 1425 | `		return 0;` |
|       - | 1426 | `	}` |
|       7 | 1427 | `	return PH7_ClassInstanceExtractAttrValue(pThis,(VmClassAttr *)pEntry->pUserData);` |
|       5 | 1428 | `}` |
|       - | 1429 | `/*` |
|       - | 1430 | ` * Emit a class-instance dump header plus its trailing newline. For var_dump` |
|       - | 1431 | ` * (ShowType) it completes the "object(" prefix the caller already emitted as` |
|       - | 1432 | ` *   ClassName)#<id> (<count>) {` |
|       - | 1433 | ` * for print_r it emits the legacy PHL  Object(ClassName) {  (count/id unused).` |
|       - | 1434 | `` * Enum cases print php's `ClassName Enum {` print_r header (var_dump never`` |
|       - | 1435 | `` * reaches here for enums — PH7_MemObjDump prints `enum(S::A)` directly).`` |
|       - | 1436 | ` */` |
|     136 | 1437 | `static void DumpClassInstanceHeader(SyBlob *pOut,ph7_class *pClass,sxu32 nObjId,int ShowType,sxu32 nCount)` |
|       3 | 1438 | `{` |
|     139 | 1439 | `	if( ShowType ){` |
|       - | 1440 | ``		/* var_dump: `object(C)#id (n) {` */`` |
|     135 | 1441 | `		SyBlobFormat(&(*pOut),"object(%z)#%u (%u) {",&pClass->sName,nObjId,nCount);` |
|     135 | 1442 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|     135 | 1443 | `		return;` |
|       - | 1444 | `	}` |
|       - | 1445 | ``	/* print_r: `C Object` / `E Enum[:backing]` — the '(' line is emitted by`` |
|       - | 1446 | `	 * the body renderer at the container indent. */` |
|       6 | 1447 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|     ! 0 | 1448 | `		SyBlobFormat(&(*pOut),"%z Enum",&pClass->sName);` |
|     ! 0 | 1449 | `		if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|     ! 0 | 1450 | `			SyBlobAppend(&(*pOut),":int",sizeof(":int")-1);` |
|     ! 0 | 1451 | `		}else if( pClass->nEnumBacking == MEMOBJ_STRING ){` |
|     ! 0 | 1452 | `			SyBlobAppend(&(*pOut),":string",sizeof(":string")-1);` |
|     ! 0 | 1453 | `		}` |
|     ! 0 | 1454 | `	}else{` |
|       6 | 1455 | `		SyBlobFormat(&(*pOut),"%z Object",&pClass->sName);` |
|       - | 1456 | `	}` |
|       6 | 1457 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      71 | 1458 | `}` |
|       - | 1459 | `/*` |
|       - | 1460 | ` * The class that DECLARED pAttr: inheritance shares attr pointers down the` |
|       - | 1461 | ` * chain, so the declaring class is the most ANCESTRAL class whose hAttr still` |
|       - | 1462 | ` * maps the name to this exact pointer. php's var_dump/print_r use it for the` |
|       - | 1463 | `` * `["p":"Decl":private]` annotation.`` |
|       - | 1464 | ` */` |
|       6 | 1465 | `static ph7_class * OoAttrDeclaringClass(ph7_class *pClass,ph7_class_attr *pAttr)` |
|       1 | 1466 | `{` |
|       - | 1467 | `	/* Attrs record their declaring class at install time (inheritance/trait` |
|       - | 1468 | `	 * copies share the pointer, so the field survives the chain). */` |
|       7 | 1469 | `	return pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|       1 | 1470 | `}` |
|       - | 1471 | `/*` |
|       - | 1472 | `` * Emit a property's dump key: var_dump `["x"]=>` / `["p":"C":private]=>` /`` |
|       - | 1473 | `` * `["q":protected]=>`; print_r `[x] => ` / `[p:C:private] => ` /`` |
|       - | 1474 | `` * `[q:protected] => ` (php's exact annotations).`` |
|       - | 1475 | ` */` |
|     138 | 1476 | `static void OoDumpPropKey(SyBlob *pOut,ph7_class_instance *pThis,ph7_class_attr *pAttr,int ShowType)` |
|       2 | 1477 | `{` |
|     140 | 1478 | `	const char *zQ = ShowType ? "\"" : "";` |
|     140 | 1479 | `	SyBlobFormat(&(*pOut),"[%s%z%s",zQ,&pAttr->sName,zQ);` |
|     140 | 1480 | `	if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       7 | 1481 | `		ph7_class *pDecl = OoAttrDeclaringClass(pThis->pClass,pAttr);` |
|       7 | 1482 | `		SyBlobFormat(&(*pOut),":%s%z%s:private",zQ,&pDecl->sName,zQ);` |
|     137 | 1483 | `	}else if( pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
|     ! 0 | 1484 | `		SyBlobAppend(&(*pOut),":protected",sizeof(":protected")-1);` |
|     ! 0 | 1485 | `	}` |
|     140 | 1486 | `	SyBlobAppend(&(*pOut),ShowType ? "]=>" : "] => ",ShowType ? 3 : 5);` |
|     140 | 1487 | `}` |
|     140 | 1488 | `PH7_PRIVATE sxi32 PH7_ClassInstanceDump(SyBlob *pOut,ph7_class_instance *pThis,int ShowType,int nTab,int nDepth)` |
|       3 | 1489 | `{` |
|       - | 1490 | `	SyHashEntry *pEntry;` |
|       - | 1491 | `	ph7_value *pValue;` |
|       - | 1492 | `	sxi32 rc;` |
|       - | 1493 | `	int i;` |
|     143 | 1494 | `	if( nDepth > 31 ){` |
|       - | 1495 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|       - | 1496 | `		/* Nesting limit reached..halt immediately*/` |
|       5 | 1497 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|       5 | 1498 | `		return SXERR_LIMIT;` |
|       - | 1499 | `	}` |
|     139 | 1500 | `	rc = SXRET_OK;` |
|       - | 1501 | `	{` |
|       - | 1502 | `		/* Both var_dump and print_r consult __debugInfo() (PHP behavior);` |
|       - | 1503 | `		 * var_export uses a separate renderer and never reaches here. When the` |
|       - | 1504 | `		 * method is present and returns an array, render that array's entries as` |
|       - | 1505 | `		 * the object body, with the header showing the debug array's count. The` |
|       - | 1506 | `		 * nDepth guard above protects against a __debugInfo returning the object` |
|       - | 1507 | `		 * itself. */` |
|     139 | 1508 | `		ph7_class_method *pDbg = PH7_ClassExtractMethod(pThis->pClass,"__debugInfo",sizeof("__debugInfo")-1);` |
|     139 | 1509 | `		if( pDbg ){` |
|       - | 1510 | `			ph7_value sResult;` |
|       5 | 1511 | `			PH7_MemObjInit(pThis->pVm,&sResult);` |
|       5 | 1512 | `			PH7_VmCallClassMethod(pThis->pVm,pThis,pDbg,&sResult,0,0);` |
|       5 | 1513 | `			if( sResult.iFlags & MEMOBJ_HASHMAP ){` |
|       5 | 1514 | `				ph7_hashmap *pMap = (ph7_hashmap *)sResult.x.pOther;` |
|       - | 1515 | `				/* Header count is the debug array's entry count. */` |
|       5 | 1516 | `				DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,pMap->nEntry);` |
|       5 | 1517 | `				if( !ShowType ){` |
|       3 | 1518 | `					for( i = 0 ; i < nTab ; i++ ){` |
|     ! 0 | 1519 | `						SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     ! 0 | 1520 | `					}` |
|       3 | 1521 | `					SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       1 | 1522 | `				}` |
|       5 | 1523 | `				rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth);` |
|       5 | 1524 | `				for( i = 0 ; i < nTab ; i++ ){` |
|     ! 0 | 1525 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     ! 0 | 1526 | `				}` |
|       5 | 1527 | `				if( ShowType ){` |
|       3 | 1528 | `					SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|       2 | 1529 | `				}else{` |
|       3 | 1530 | `					SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       - | 1531 | `				}` |
|       5 | 1532 | `				PH7_MemObjRelease(&sResult);` |
|       5 | 1533 | `				return rc;` |
|       - | 1534 | `			}` |
|       - | 1535 | `			/* Non-array return: behave as if __debugInfo were absent. */` |
|     ! 0 | 1536 | `			PH7_MemObjRelease(&sResult);` |
|     ! 0 | 1537 | `		}` |
|       - | 1538 | `	}` |
|       - | 1539 | `	{` |
|       - | 1540 | `		/* var_dump's header needs the property count up front, so pre-count the` |
|       - | 1541 | `		 * non-static/non-constant attributes (matching the dump loop below). */` |
|     134 | 1542 | `		sxu32 nProp = 0;` |
|     134 | 1543 | `		if( ShowType ){` |
|     132 | 1544 | `			SyHashResetLoopCursor(&pThis->hAttr);` |
|     270 | 1545 | `			while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|     140 | 1546 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     140 | 1547 | `				if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_HOOK_VIRTUAL)) == 0 ){` |
|     136 | 1548 | `					nProp++;` |
|      67 | 1549 | `				}` |
|       2 | 1550 | `			}` |
|      65 | 1551 | `		}` |
|     134 | 1552 | `		DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,nProp);` |
|       - | 1553 | `	}` |
|     134 | 1554 | `	if( !ShowType ){` |
|       - | 1555 | `		/* print_r body opener: '(' at the container indent */` |
|       3 | 1556 | `		for( i = 0 ; i < nTab ; i++ ){` |
|     ! 0 | 1557 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     ! 0 | 1558 | `		}` |
|       3 | 1559 | `		SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       1 | 1560 | `	}` |
|       - | 1561 | `	/* Dump object attributes (php 8.4: VIRTUAL hooked properties have no` |
|       - | 1562 | `	 * backing store — excluded from var_dump/print_r) */` |
|     134 | 1563 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|     218 | 1564 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|     144 | 1565 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     144 | 1566 | `		if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_HOOK_VIRTUAL)) == 0 ){` |
|       - | 1567 | `			/* Dump non-static/constant attribute only */` |
|     140 | 1568 | `			pValue = ExtractClassAttrValue(pThis->pVm,pVmAttr);` |
|     140 | 1569 | `			if( pValue == 0 ){` |
|     ! 0 | 1570 | `				continue;` |
|       - | 1571 | `			}` |
|     140 | 1572 | `			if( ShowType ){` |
|       - | 1573 | ``				/* var_dump prop: `["x"(:…)]=>` at nTab+2, the value on the next`` |
|       - | 1574 | `				 * line at the same indent (php). */` |
|    4124 | 1575 | `				for( i = 0 ; i < nTab + 2 ; i++ ){` |
|    3990 | 1576 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|    1996 | 1577 | `				}` |
|     136 | 1578 | `				OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,TRUE);` |
|     136 | 1579 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|     136 | 1580 | `				rc = PH7_MemObjDump(&(*pOut),pValue,TRUE,nTab+2,nDepth,0);` |
|     136 | 1581 | `				if( rc == SXERR_LIMIT ){` |
|     125 | 1582 | `					break;` |
|       - | 1583 | `				}` |
|       7 | 1584 | `			}else{` |
|       - | 1585 | ``				/* print_r prop: `[x(:…)] => value` at nTab+4; container values`` |
|       - | 1586 | `				 * render their block at nTab+8 followed by php's blank line. */` |
|      21 | 1587 | `				for( i = 0 ; i < nTab + 4 ; i++ ){` |
|      17 | 1588 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       9 | 1589 | `				}` |
|       5 | 1590 | `				OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,FALSE);` |
|       4 | 1591 | `				if( (pValue->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ))` |
|       3 | 1592 | `				 && (pValue->iFlags & MEMOBJ_NULL) == 0 ){` |
|     ! 0 | 1593 | `					rc = PH7_MemObjDump(&(*pOut),pValue,FALSE,nTab+8,nDepth,0);` |
|     ! 0 | 1594 | `					SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|     ! 0 | 1595 | `					if( rc == SXERR_LIMIT ){` |
|     ! 0 | 1596 | `						break;` |
|       - | 1597 | `					}` |
|     ! 0 | 1598 | `				}else{` |
|       5 | 1599 | `					PH7_MemObjPrintRInline(&(*pOut),pValue);` |
|       5 | 1600 | `					SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 1601 | `				}` |
|       - | 1602 | `			}` |
|       7 | 1603 | `		}` |
|       2 | 1604 | `	}` |
|    3854 | 1605 | `	for( i = 0 ; i < nTab ; i++ ){` |
|    3721 | 1606 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|    1861 | 1607 | `	}` |
|     134 | 1608 | `	if( ShowType ){` |
|     132 | 1609 | `		SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      67 | 1610 | `	}else{` |
|       3 | 1611 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       - | 1612 | `	}` |
|     134 | 1613 | `	return rc;` |
|      73 | 1614 | `}` |
|       - | 1615 | `/*` |
|       - | 1616 | ` * Call a magic method [i.e: __toString(),__toBool(),__Invoke()...]` |
|       - | 1617 | ` * Return SXRET_OK on successfull call. Any other return value indicates failure.` |
|       - | 1618 | ` * Notes on magic methods.` |
|       - | 1619 | ` * According to the PHP language reference manual.` |
|       - | 1620 | ` *  The function names __construct(), __destruct(), __call(), __callStatic()` |
|       - | 1621 | ` *  __get(),  __toString(), __invoke(), __clone() are magical in PHP classes.` |
|       - | 1622 | ` * You cannot have functions with these names in any of your classes unless` |
|       - | 1623 | ` * you want the magic functionality associated with them.` |
|       - | 1624 | ` * Example of magical methods:` |
|       - | 1625 | ` * __toString()` |
|       - | 1626 | ` *  The __toString() method allows a class to decide how it will react when it is treated like` |
|       - | 1627 | ` *  a string. For example, what echo $obj; will print. This method must return a string.` |
|       - | 1628 | ` *  Example #2 Simple example` |
|       - | 1629 | ` * <?php` |
|       - | 1630 | ` * // Declare a simple class` |
|       - | 1631 | ` * class TestClass` |
|       - | 1632 | ` * {` |
|       - | 1633 | ` *   public $foo;` |
|       - | 1634 | ` *` |
|       - | 1635 | ` *   public function __construct($foo)` |
|       - | 1636 | ` *   {` |
|       - | 1637 | ` *       $this->foo = $foo;` |
|       - | 1638 | ` *   }` |
|       - | 1639 | ` *` |
|       - | 1640 | ` *   public function __toString()` |
|       - | 1641 | ` *   {` |
|       - | 1642 | ` *       return $this->foo;` |
|       - | 1643 | ` *   }` |
|       - | 1644 | ` * }` |
|       - | 1645 | ` * $class = new TestClass('Hello');` |
|       - | 1646 | ` * echo $class;` |
|       - | 1647 | ` * ?>` |
|       - | 1648 | ` * The above example will output:` |
|       - | 1649 | ` *  Hello` |
|       - | 1650 | ` *` |
|       - | 1651 | ` * Note that PH7 does not support all the magical method and introudces __toFloat(),__toInt()` |
|       - | 1652 | ` * which have the same behaviour as __toString() but for float and integer types` |
|       - | 1653 | ` * respectively.` |
|       - | 1654 | ` * Refer to the official documentation for more information.` |
|       - | 1655 | ` */` |
|      54 | 1656 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallMagicMethod(` |
|       - | 1657 | `	ph7_vm *pVm,               /* VM that own all this stuff */` |
|       - | 1658 | `	ph7_class *pClass,         /* Target class */` |
|       - | 1659 | `	ph7_class_instance *pThis, /* Target object */` |
|       - | 1660 | `	const char *zMethod,       /* Magic method name [i.e: __toString()]*/` |
|       - | 1661 | `	sxu32 nByte,               /* zMethod length*/` |
|       - | 1662 | `	const SyString *pAttrName, /* Attribute name */` |
|       - | 1663 | `	ph7_value *pResult         /* OUT: magic method return value. NULL to discard */` |
|       - | 1664 | `	)` |
|       1 | 1665 | `{` |
|      55 | 1666 | `	ph7_value *apArg[2] = { 0 , 0 };` |
|       - | 1667 | `	ph7_class_method *pMeth;` |
|       - | 1668 | `	ph7_value sAttr; /* cc warning */` |
|       - | 1669 | `	sxi32 rc;` |
|       - | 1670 | `	int nArg;` |
|       - | 1671 | `	/* Make sure the magic method is available */` |
|      55 | 1672 | `	pMeth = PH7_ClassExtractMethod(&(*pClass),zMethod,nByte);` |
|      55 | 1673 | `	if( pMeth == 0 ){` |
|       - | 1674 | `		/* No such method,return immediately */` |
|     ! 0 | 1675 | `		return SXERR_NOTFOUND;` |
|       - | 1676 | `	}` |
|      55 | 1677 | `	nArg = 0;` |
|       - | 1678 | `	/* Copy arguments */` |
|      55 | 1679 | `	if( pAttrName ){` |
|      55 | 1680 | `		PH7_MemObjInitFromString(pVm,&sAttr,pAttrName);` |
|      55 | 1681 | `		sAttr.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      55 | 1682 | `		apArg[0] = &sAttr;` |
|      55 | 1683 | `		nArg = 1;` |
|      27 | 1684 | `	}` |
|       - | 1685 | `	/* Call the magic method now */` |
|      55 | 1686 | `	rc = PH7_VmCallClassMethod(pVm,&(*pThis),pMeth,pResult,nArg,apArg);` |
|       - | 1687 | `	/* Clean up */` |
|      55 | 1688 | `	if( pAttrName ){` |
|      55 | 1689 | `		PH7_MemObjRelease(&sAttr);` |
|      27 | 1690 | `	}` |
|      55 | 1691 | `	return rc;` |
|      28 | 1692 | `}` |
|       - | 1693 | `/*` |
|       - | 1694 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon].` |
|       - | 1695 | ` * This function is simply a wrapper on ExtractClassAttrValue().` |
|       - | 1696 | ` */` |
|   10780 | 1697 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceExtractAttrValue(ph7_class_instance *pThis,VmClassAttr *pAttr)` |
|       5 | 1698 | `{` |
|       - | 1699 | `   /* Extract the attribute value */` |
|       - | 1700 | `	ph7_value *pValue;` |
|   10785 | 1701 | `	pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|   10785 | 1702 | `	return pValue;` |
|       5 | 1703 | `}` |
|       - | 1704 | `/*` |
|       - | 1705 | ` * Convert a class instance [i.e: Object in the PHP jargon] into a hashmap [i.e: array in the PHP jargon].` |
|       - | 1706 | ` * Return SXRET_OK on success. Any other value indicates failure.` |
|       - | 1707 | ` * Note on object conversion to array:` |
|       - | 1708 | ` *  Acccording to the PHP language reference manual` |
|       - | 1709 | ` *  If an object is converted to an array, the result is an array whose elements are the object's properties.` |
|       - | 1710 | ` *  The keys are the member variable names.` |
|       - | 1711 | ` *` |
|       - | 1712 | ` *  The following example:` |
|       - | 1713 | ` *  class Test {` |
|       - | 1714 | ` *   public $A = 25<<1;  // 50` |
|       - | 1715 | ` *	 public $c = rand_str(3);   // Random string of length 3` |
|       - | 1716 | ` *	 public $d = rand() & 1023; // Random number between 0..1023` |
|       - | 1717 | ` *  }` |
|       - | 1718 | ` *  var_dump((array) new Test());` |
|       - | 1719 | ` *	Will output:` |
|       - | 1720 | ` *  array(3) {` |
|       - | 1721 | ` *   [A] =>` |
|       - | 1722 | ` *      int(50)` |
|       - | 1723 | ` *   [c] =>` |
|       - | 1724 | ` *     string(3 'aps')` |
|       - | 1725 | ` *   [d] =>` |
|       - | 1726 | ` *     int(991)` |
|       - | 1727 | ` *  }` |
|       - | 1728 | ` * You have noticed that PH7 allow class attributes [i.e: $a,$c,$d in the example above]` |
|       - | 1729 | ` * have any complex expression (even function calls/Annonymous functions) as their default` |
|       - | 1730 | ` * value unlike the standard PHP engine.` |
|       - | 1731 | ` * This is a very powerful feature that you have to look at.` |
|       - | 1732 | ` */` |
|      14 | 1733 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|       1 | 1734 | `{` |
|       - | 1735 | `	SyHashEntry *pEntry;` |
|       - | 1736 | `	SyString *pAttrName;` |
|       - | 1737 | `	VmClassAttr *pAttr;` |
|       - | 1738 | `	ph7_value *pValue;` |
|       - | 1739 | `	ph7_value sName;` |
|       - | 1740 | `	/* Reset the loop cursor */` |
|      15 | 1741 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      15 | 1742 | `	PH7_MemObjInitFromString(pThis->pVm,&sName,0);` |
|      51 | 1743 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       - | 1744 | `		/* Point to the current attribute */` |
|      37 | 1745 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      37 | 1746 | `		if( pAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|       - | 1747 | `			/* php 8.4: a VIRTUAL hooked property has no backing store — the` |
|       - | 1748 | `			 * (array) cast excludes it (raw surface, get is NOT dispatched) */` |
|       7 | 1749 | `			continue;` |
|       - | 1750 | `		}` |
|       - | 1751 | `		/* Extract attribute value */` |
|      31 | 1752 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|      31 | 1753 | `		if( pValue ){` |
|       - | 1754 | `			/* Build attribute name */` |
|      31 | 1755 | `			pAttrName = &pAttr->pAttr->sName;` |
|      31 | 1756 | `			PH7_MemObjStringAppend(&sName,pAttrName->zString,pAttrName->nByte);` |
|       - | 1757 | `			/* Perform the insertion */` |
|      31 | 1758 | `			PH7_HashmapInsert(pMap,&sName,pValue);` |
|       - | 1759 | `			/* Reset the string cursor */` |
|      31 | 1760 | `			SyBlobReset(&sName.sBlob);` |
|      15 | 1761 | `		}` |
|       1 | 1762 | `	}` |
|      15 | 1763 | `	PH7_MemObjRelease(&sName);` |
|      15 | 1764 | `	return SXRET_OK;` |
|       1 | 1765 | `}` |
|       - | 1766 | `/*` |
|       - | 1767 | ` * Iterate throw class attributes and invoke the given callback [i.e: xWalk()] for each` |
|       - | 1768 | ` * retrieved attribute.` |
|       - | 1769 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|       - | 1770 | ` * the attribute value in the callback body will not alter the real attribute value.` |
|       - | 1771 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|       - | 1772 | ` * a value different from PH7_OK.` |
|       - | 1773 | ` * Refer to [ph7_object_walk()] for more information.` |
|       - | 1774 | ` */` |
|     ! 0 | 1775 | `PH7_PRIVATE sxi32 PH7_ClassInstanceWalk(` |
|       - | 1776 | `	ph7_class_instance *pThis, /* Target object */` |
|       - | 1777 | `	int (*xWalk)(const char *,ph7_value *,void *), /* Walker callback */` |
|       - | 1778 | `	void *pUserData /* Last argument to xWalk() */` |
|       - | 1779 | `	)` |
|     ! 0 | 1780 | `{` |
|       - | 1781 | `	SyHashEntry *pEntry; /* Hash entry */` |
|       - | 1782 | `	VmClassAttr *pAttr;  /* Pointer to the attribute */` |
|       - | 1783 | `	ph7_value *pValue;   /* Attribute value */` |
|       - | 1784 | `	ph7_value sValue;    /* Copy of the attribute value */` |
|       - | 1785 | `	int rc;` |
|       - | 1786 | `	/* Reset the loop cursor */` |
|     ! 0 | 1787 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|     ! 0 | 1788 | `	PH7_MemObjInit(pThis->pVm,&sValue);` |
|       - | 1789 | `	/* Start the walk process */` |
|     ! 0 | 1790 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       - | 1791 | `		/* Point to the current attribute */` |
|     ! 0 | 1792 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1793 | `		/* Extract attribute value */` |
|     ! 0 | 1794 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     ! 0 | 1795 | `		if( pValue ){` |
|     ! 0 | 1796 | `			PH7_MemObjLoad(pValue,&sValue);` |
|       - | 1797 | `			/* Invoke the supplied callback */` |
|     ! 0 | 1798 | `			rc =  xWalk(SyStringData(&pAttr->pAttr->sName),&sValue,pUserData);` |
|     ! 0 | 1799 | `			PH7_MemObjRelease(&sValue);` |
|     ! 0 | 1800 | `			if( rc != PH7_OK){` |
|       - | 1801 | `				/* User callback request an operation abort */` |
|     ! 0 | 1802 | `				return SXERR_ABORT;` |
|       - | 1803 | `			}` |
|     ! 0 | 1804 | `		}` |
|     ! 0 | 1805 | `	}` |
|       - | 1806 | `	/* All done */` |
|     ! 0 | 1807 | `	return SXRET_OK;` |
|     ! 0 | 1808 | `}` |
|       - | 1809 | `/*` |
|       - | 1810 | ` * Extract a class atrribute value.` |
|       - | 1811 | ` * Return a pointer to the attribute value on success. Otherwise NULL.` |
|       - | 1812 | ` * Note:` |
|       - | 1813 | ` *  Access to static and constant attribute is not allowed. That is,the function` |
|       - | 1814 | ` *  will return NULL in case someone (host-application code) try to extract` |
|       - | 1815 | ` *  a static/constant attribute.` |
|       - | 1816 | ` */` |
|   10948 | 1817 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceFetchAttr(ph7_class_instance *pThis,const SyString *pName)` |
|       5 | 1818 | `{` |
|       - | 1819 | `	SyHashEntry *pEntry;` |
|       - | 1820 | `	VmClassAttr *pAttr;` |
|       - | 1821 | `	/* Query the attribute hashtable */` |
|   10953 | 1822 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);` |
|   10953 | 1823 | `	if( pEntry == 0 ){` |
|       - | 1824 | `		/* No such attribute */` |
|     ! 0 | 1825 | `		return 0;` |
|       - | 1826 | `	}` |
|       - | 1827 | `	/* Point to the class atrribute */` |
|   10953 | 1828 | `	pAttr = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1829 | `	/* Check if we are dealing with a static/constant attribute */` |
|   10953 | 1830 | `	if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|       - | 1831 | `		/* Access is forbidden */` |
|     ! 0 | 1832 | `		return 0;` |
|       - | 1833 | `	}` |
|       - | 1834 | `	/* Return the attribute value */` |
|   10953 | 1835 | `	return ExtractClassAttrValue(pThis->pVm,pAttr);` |
|    5479 | 1836 | `}` |
|       - | 1837 |  |
