# src/ph7/oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 653/707 lines (92.36%)

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
|  259324 |   14 | `PH7_PRIVATE ph7_class * PH7_NewRawClass(ph7_vm *pVm,const SyString *pName,sxu32 nLine)` |
|       5 |   15 | `{` |
|       - |   16 | `	ph7_class *pClass;` |
|       - |   17 | `	char *zName;` |
|       - |   18 | `	/* Allocate a new instance */` |
|  259329 |   19 | `	pClass = (ph7_class *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class));` |
|  259329 |   20 | `	if( pClass == 0 ){` |
|     ! 0 |   21 | `		return 0;` |
|       - |   22 | `	}` |
|       - |   23 | `	/* Zero the structure */` |
|  259329 |   24 | `	SyZero(pClass,sizeof(ph7_class));` |
|       - |   25 | `	/* Duplicate class name */` |
|  259329 |   26 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  259329 |   27 | `	if( zName == 0 ){` |
|     ! 0 |   28 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClass);` |
|     ! 0 |   29 | `		return 0;` |
|       - |   30 | `	}` |
|       - |   31 | `	/* Initialize fields */` |
|  259329 |   32 | `	SyStringInitFromBuf(&pClass->sName,zName,pName->nByte);` |
|  259329 |   33 | `	SyHashInit(&pClass->hMethod,&pVm->sAllocator,0,0);` |
|  259329 |   34 | `	SyHashInit(&pClass->hAttr,&pVm->sAllocator,0,0);` |
|  259329 |   35 | `	SyHashInit(&pClass->hDerived,&pVm->sAllocator,0,0);` |
|  259329 |   36 | `	SySetInit(&pClass->aInterface,&pVm->sAllocator,sizeof(ph7_class *));` |
|  259329 |   37 | `	SySetInit(&pClass->aTrait,&pVm->sAllocator,sizeof(ph7_class *));` |
|  259329 |   38 | `	SySetInit(&pClass->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|  259329 |   39 | `	pClass->nLine = nLine;` |
|  259329 |   40 | `	if( pVm->bCompilingBuiltin ){` |
|       - |   41 | `		/* Defined by an embedded builtin chunk: internal, no defining file.` |
|       - |   42 | `		 * Class compilers merge further flags with \|= so this survives. */` |
|  257687 |   43 | `		pClass->iFlags \|= PH7_CLASS_INTERNAL;` |
|  128846 |   44 | `	}else{` |
|       - |   45 | `		/* Alias the VM-lifetime path dup on top of the include stack */` |
|    1647 |   46 | `		SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|    1647 |   47 | `		if( pFile ){` |
|    1647 |   48 | `			SyStringDupPtr(&pClass->sFile,pFile);` |
|     821 |   49 | `		}` |
|       - |   50 | `	}` |
|       - |   51 | `	/* All done */` |
|  259329 |   52 | `	return pClass;` |
|  129667 |   53 | `}` |
|       - |   54 | `/*` |
|       - |   55 | ` * Allocate and initialize a new class attribute.` |
|       - |   56 | ` * Return a pointer to the class attribute on success. NULL otherwise.` |
|       - |   57 | ` */` |
|  350952 |   58 | `PH7_PRIVATE ph7_class_attr * PH7_NewClassAttr(ph7_vm *pVm,const SyString *pName,sxu32 nLine,sxi32 iProtection,sxi32 iFlags)` |
|       5 |   59 | `{` |
|       - |   60 | `	ph7_class_attr *pAttr;` |
|       - |   61 | `	char *zName;` |
|  350957 |   62 | `	pAttr = (ph7_class_attr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_attr));` |
|  350957 |   63 | `	if( pAttr == 0 ){` |
|     ! 0 |   64 | `		return 0;` |
|       - |   65 | `	}` |
|       - |   66 | `	/* Zero the structure */` |
|  350957 |   67 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|  350957 |   68 | `	SySetInit(&pAttr->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|       - |   69 | `	/* Duplicate attribute name */` |
|  350957 |   70 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  350957 |   71 | `	if( zName == 0 ){` |
|     ! 0 |   72 | `		SyMemBackendPoolFree(&pVm->sAllocator,pAttr);` |
|     ! 0 |   73 | `		return 0;` |
|       - |   74 | `	}` |
|       - |   75 | `	/* Initialize fields */` |
|  350957 |   76 | `	SySetInit(&pAttr->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|  350957 |   77 | `	SySetInit(&pAttr->aUnionAlts,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|  350957 |   78 | `	SyStringInitFromBuf(&pAttr->sName,zName,pName->nByte);` |
|  350957 |   79 | `	pAttr->iProtection = iProtection;` |
|  350957 |   80 | `	pAttr->nIdx = SXU32_HIGH;` |
|  350957 |   81 | `	pAttr->iFlags = iFlags;` |
|  350957 |   82 | `	pAttr->nLine = nLine;` |
|  350957 |   83 | `	return pAttr;` |
|  175481 |   84 | `}` |
|       - |   85 | `/*` |
|       - |   86 | ` * Allocate and initialize a new class method.` |
|       - |   87 | ` * Return a pointer to the class method on success. NULL otherwise` |
|       - |   88 | ` * This function associate with the newly created method an automatically generated` |
|       - |   89 | ` * random unique name.` |
|       - |   90 | ` */` |
| 1374382 |   91 | `PH7_PRIVATE ph7_class_method * PH7_NewClassMethod(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,sxu32 nLine,` |
|       - |   92 | `	sxi32 iProtection,sxi32 iFlags,sxi32 iFuncFlags)` |
|       5 |   93 | `{` |
|       - |   94 | `	ph7_class_method *pMeth;` |
|       - |   95 | `	SyHashEntry *pEntry;` |
|       - |   96 | `	SyString *pNamePtr;` |
|       - |   97 | `	char zSalt[10];` |
|       - |   98 | `	char *zName;` |
|       - |   99 | `	sxu32 nByte;` |
|       - |  100 | `	/* Allocate a new class method instance */` |
| 1374387 |  101 | `	pMeth = (ph7_class_method *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_method));` |
| 1374387 |  102 | `	if( pMeth == 0 ){` |
|     ! 0 |  103 | `		return 0;` |
|       - |  104 | `	}` |
|       - |  105 | `	/* Zero the structure */` |
| 1374387 |  106 | `	SyZero(pMeth,sizeof(ph7_class_method));` |
|       - |  107 | `	/* Check for an already installed method with the same name */` |
| 1374387 |  108 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
| 1374387 |  109 | `	if( pEntry == 0 ){` |
|       - |  110 | `		/* Associate an unique VM name to this method */` |
| 1374385 |  111 | `		nByte = sizeof(zSalt) + pName->nByte + SyStringLength(&pClass->sName)+sizeof(char)*7/*[[__'\0'*/;` |
| 1374385 |  112 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,nByte);` |
| 1374385 |  113 | `		if( zName == 0 ){` |
|     ! 0 |  114 | `			SyMemBackendPoolFree(&pVm->sAllocator,pMeth);` |
|     ! 0 |  115 | `			return 0;` |
|       - |  116 | `		}` |
| 1374385 |  117 | `		pNamePtr = &pMeth->sVmName;` |
|       - |  118 | `		/* Generate a random string */` |
| 1374385 |  119 | `		PH7_VmRandomString(&(*pVm),zSalt,sizeof(zSalt));` |
| 1374385 |  120 | `		pNamePtr->nByte = SyBufferFormat(zName,nByte,"[__%z@%z_%.*s]",&pClass->sName,pName,sizeof(zSalt),zSalt);` |
| 1374385 |  121 | `		pNamePtr->zString = zName;` |
|  687195 |  122 | `	}else{` |
|       - |  123 | `		/* Method is condidate for 'overloading' */` |
|       3 |  124 | `		ph7_class_method *pCurrent = (ph7_class_method *)pEntry->pUserData;` |
|       3 |  125 | `		pNamePtr = &pMeth->sVmName;` |
|       - |  126 | `		/* Use the same VM name */` |
|       3 |  127 | `		SyStringDupPtr(pNamePtr,&pCurrent->sVmName);` |
|       3 |  128 | `		zName = (char *)pNamePtr->zString;` |
|       - |  129 | `	}` |
| 1374387 |  130 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|   57726 |  131 | `		if( (pName->nByte == sizeof("__construct") - 1 && SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1 ) == 0)` |
|   57725 |  132 | `			\|\| (pName->nByte == sizeof("__destruct") - 1 && SyMemcmp(pName->zString,"__destruct",sizeof("__destruct") - 1 ) == 0)` |
|   57729 |  133 | `			\|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|       - |  134 | `				/* Switch to public visibility when dealing with constructor/destructor */` |
|       5 |  135 | `				iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       2 |  136 | `		}` |
|   28864 |  137 | `	}` |
|       - |  138 | `	/* Initialize method fields */` |
| 1374389 |  139 | `	pMeth->iProtection = iProtection;` |
| 1374389 |  140 | `	pMeth->iFlags = iFlags;` |
| 1374389 |  141 | `	pMeth->nLine = nLine;` |
| 2061582 |  142 | `	PH7_VmInitFuncState(&(*pVm),&pMeth->sFunc,&zName[sizeof(char)*4/*[__@*/+SyStringLength(&pClass->sName)],` |
| 1374384 |  143 | `		pName->nByte,iFuncFlags\|VM_FUNC_CLASS_METHOD,pClass);` |
| 1374389 |  144 | `	return pMeth;` |
|  687198 |  145 | `}` |
|       - |  146 | `/*` |
|       - |  147 | ` * Check if the given name have a class method associated with it.` |
|       - |  148 | ` * Return the desired method [i.e: ph7_class_method instance] on success. NULL otherwise.` |
|       - |  149 | ` */` |
|  313092 |  150 | `PH7_PRIVATE ph7_class_method * PH7_ClassExtractMethod(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|       5 |  151 | `{` |
|       - |  152 | `	SyHashEntry *pEntry;` |
|       - |  153 | `	/* Perform a hash lookup */` |
|  313097 |  154 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)zName,nByte);` |
|  313097 |  155 | `	if( pEntry == 0 ){` |
|       - |  156 | `		/* No such entry */` |
|    6571 |  157 | `		return 0;` |
|       - |  158 | `	}` |
|       - |  159 | `	/* Point to the desired method */` |
|  306531 |  160 | `	return (ph7_class_method *)pEntry->pUserData;` |
|  156551 |  161 | `}` |
|       - |  162 | `/*` |
|       - |  163 | ` * Check if the given name is a class attribute.` |
|       - |  164 | ` * Return the desired attribute [i.e: ph7_class_attr instance] on success.NULL otherwise.` |
|       - |  165 | ` */` |
|  208828 |  166 | `PH7_PRIVATE ph7_class_attr * PH7_ClassExtractAttribute(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|       5 |  167 | `{` |
|       - |  168 | `	SyHashEntry *pEntry;` |
|       - |  169 | `	/* Perform a hash lookup */` |
|  208833 |  170 | `	pEntry = SyHashGet(&pClass->hAttr,(const void *)zName,nByte);` |
|  208833 |  171 | `	if( pEntry == 0 ){` |
|       - |  172 | `		/* No such entry */` |
|  208611 |  173 | `		return 0;` |
|       - |  174 | `	}` |
|       - |  175 | `	/* Point to the desierd method */` |
|     227 |  176 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|  104419 |  177 | `}` |
|       - |  178 | `/*` |
|       - |  179 | ` * Install a class attribute in the corresponding container.` |
|       - |  180 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|       - |  181 | ` */` |
|  350952 |  182 | `PH7_PRIVATE sxi32 PH7_ClassInstallAttr(ph7_class *pClass,ph7_class_attr *pAttr)` |
|       5 |  183 | `{` |
|  350957 |  184 | `	SyString *pName = &pAttr->sName;` |
|       - |  185 | `	sxi32 rc;` |
|       - |  186 | `	/* Remember where this attribute was originally declared so that later` |
|       - |  187 | `	 * inheritance/trait copies still know the declaring class (needed for` |
|       - |  188 | `	 * PHP-compatible error messages on typed properties). */` |
|  350957 |  189 | `	if( pAttr->pDeclClass == 0 ){` |
|  350957 |  190 | `		pAttr->pDeclClass = pClass;` |
|  175476 |  191 | `	}` |
|  350957 |  192 | `	rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|  350957 |  193 | `	return rc;` |
|       5 |  194 | `}` |
|       - |  195 | `/*` |
|       - |  196 | ` * Install a class method in the corresponding container.` |
|       - |  197 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|       - |  198 | ` */` |
| 1374370 |  199 | `PH7_PRIVATE sxi32 PH7_ClassInstallMethod(ph7_class *pClass,ph7_class_method *pMeth)` |
|       5 |  200 | `{` |
| 1374375 |  201 | `	SyString *pName = &pMeth->sFunc.sName;` |
|       - |  202 | `	sxi32 rc;` |
| 1374375 |  203 | `	rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 1374375 |  204 | `	return rc;` |
|       5 |  205 | `}` |
|       - |  206 | `/*` |
|       - |  207 | ` * Method-override compatibility (variance) checking.` |
|       - |  208 | ` *` |
|       - |  209 | ` * PHP rejects an override whose signature is incompatible with the parent's:` |
|       - |  210 | ` * return types are covariant (child may only narrow), parameter types are` |
|       - |  211 | ` * contravariant (child may only widen), and a child may not add a required` |
|       - |  212 | ` * parameter. We add the diagnostic — but conservatively: PHL must keep running` |
|       - |  213 | ` * valid PHP, so the comparator below is SKIP-BY-DEFAULT. It flags only cases that` |
|       - |  214 | ` * are unambiguously invalid and silently accepts anything subtle (unions,` |
|       - |  215 | ` * intersections, pseudo-types, self/parent/static, object, unresolved classes,` |
|       - |  216 | ` * or a missing type), so it can never reject valid code.` |
|       - |  217 | ` */` |
|       - |  218 | `#define OVT_NONE   0  /* no declared type */` |
|       - |  219 | `#define OVT_SCALAR 1  /* a concrete invariant scalar: int/float/string/bool/array */` |
|       - |  220 | `#define OVT_CLASS  2  /* a real, already-loaded class/interface */` |
|       - |  221 | `#define OVT_SKIP   3  /* union/intersection/pseudo/self/object/unresolved — never flag */` |
|       - |  222 |  |
|       - |  223 | `/*` |
|       - |  224 | ` * Classify one declared type (nType + class name + union flag) for override` |
|       - |  225 | ` * comparison. On OVT_CLASS, *ppClass receives the resolved class. Class names are` |
|       - |  226 | ` * resolved by a direct, autoload-free hClass lookup: a miss (forward reference,` |
|       - |  227 | ` * namespaced, or not-yet-loaded) yields OVT_SKIP, which the caller accepts.` |
|       - |  228 | ` */` |
|   30908 |  229 | `static int OoClassifyOverrideType(ph7_vm *pVm, sxu32 nType, const SyString *pClass,` |
|       - |  230 | `	int bUnion, ph7_class **ppClass)` |
|       5 |  231 | `{` |
|   30913 |  232 | `	*ppClass = 0;` |
|   30913 |  233 | `	if( bUnion ){` |
|       3 |  234 | `		return OVT_SKIP; /* union/intersection — full lattice, skip */` |
|       - |  235 | `	}` |
|   30911 |  236 | `	if( nType == 0 ){` |
|   30837 |  237 | `		return OVT_NONE; /* no declared type */` |
|       - |  238 | `	}` |
|      78 |  239 | `	if( nType == SXU32_HIGH ){` |
|       - |  240 | `		/* A class name OR a pseudo-type stored as a name atom. Skip every pseudo` |
|       - |  241 | `		 * (incl. self/parent/static, which are context-relative). */` |
|       - |  242 | `		static const struct { const char *z; sxu32 n; } aPseudo[] = {` |
|       - |  243 | `			{"mixed",5}, {"never",5}, {"iterable",8}, {"callable",8}, {"true",4},` |
|       - |  244 | `			{"false",5}, {"self",4}, {"parent",6}, {"static",6}` |
|       - |  245 | `		};` |
|      18 |  246 | `		const char *z = pClass->zString;` |
|      18 |  247 | `		sxu32 n = pClass->nByte;` |
|       - |  248 | `		SyHashEntry *pE;` |
|       - |  249 | `		sxu32 i;` |
|     154 |  250 | `		for( i = 0; i < SX_ARRAYSIZE(aPseudo); i++ ){` |
|     142 |  251 | `			if( n == aPseudo[i].n && SyStrnmicmp(z,aPseudo[i].z,n) == 0 ){` |
|       5 |  252 | `				return OVT_SKIP;` |
|       - |  253 | `			}` |
|      70 |  254 | `		}` |
|      14 |  255 | `		pE = SyHashGet(&pVm->hClass,(const void *)z,n);` |
|      14 |  256 | `		if( pE == 0 ){` |
|     ! 0 |  257 | `			return OVT_SKIP; /* not loaded / forward ref / namespaced — accept */` |
|       - |  258 | `		}` |
|      14 |  259 | `		*ppClass = (ph7_class *)pE->pUserData;` |
|      14 |  260 | `		return OVT_CLASS;` |
|       - |  261 | `	}` |
|      58 |  262 | `	if( nType == MEMOBJ_STRING \|\| nType == MEMOBJ_INT \|\| nType == MEMOBJ_REAL` |
|      28 |  263 | `	 \|\| nType == MEMOBJ_BOOL \|\| nType == MEMOBJ_HASHMAP ){` |
|      37 |  264 | `		return OVT_SCALAR;` |
|       - |  265 | `	}` |
|       - |  266 | `	/* MEMOBJ_OBJ (object — subtypes against classes), MEMOBJ_VOID/NULL/RES,` |
|       - |  267 | `	 * or anything unexpected: skip. */` |
|      26 |  268 | `	return OVT_SKIP;` |
|   15459 |  269 | `}` |
|       - |  270 |  |
|       - |  271 | `/*` |
|       - |  272 | ` * A declared type normalized for override comparison: the raw type code, the` |
|       - |  273 | ` * class-name string (when a class), and the union/nullable flags. Extracted once` |
|       - |  274 | ` * from each side so the comparator takes two of these instead of eight scalars.` |
|       - |  275 | ` */` |
|       - |  276 | `typedef struct OvType OvType;` |
|       - |  277 | `struct OvType {` |
|       - |  278 | `	sxu32 nType;` |
|       - |  279 | `	const SyString *pClass;` |
|       - |  280 | `	int bUnion;` |
|       - |  281 | `	int bNullable;` |
|       - |  282 | `};` |
|   30872 |  283 | `static OvType OoTypeFromReturn(ph7_vm_func *pF)` |
|       5 |  284 | `{` |
|       - |  285 | `	OvType t;` |
|   30877 |  286 | `	t.nType = pF->nReturnType;` |
|   30877 |  287 | `	t.pClass = &pF->sReturnClass;` |
|   30877 |  288 | `	t.bUnion = SySetUsed(&pF->aReturnUnion) > 0;` |
|   30877 |  289 | `	t.bNullable = (pF->iFlags & VM_FUNC_RETURN_NULLABLE) != 0;` |
|   30877 |  290 | `	return t;` |
|       5 |  291 | `}` |
|      36 |  292 | `static OvType OoTypeFromArg(ph7_vm_func_arg *pA)` |
|       3 |  293 | `{` |
|       - |  294 | `	OvType t;` |
|      39 |  295 | `	t.nType = pA->nType;` |
|      39 |  296 | `	t.pClass = &pA->sClass;` |
|      39 |  297 | `	t.bUnion = (pA->iFlags & VM_FUNC_ARG_UNION) != 0;` |
|      39 |  298 | `	t.bNullable = (pA->iFlags & VM_FUNC_ARG_NULLABLE) != 0;` |
|      39 |  299 | `	return t;` |
|       3 |  300 | `}` |
|       - |  301 | `/*` |
|       - |  302 | ` * Return TRUE if the child type is an unambiguously-invalid override of the` |
|       - |  303 | ` * parent type. bCovariant=1 for a return type (child must be ⊆ parent),` |
|       - |  304 | ` * 0 for a parameter (child must be ⊇ parent). Returns FALSE (accept) on any` |
|       - |  305 | ` * skipped/ambiguous shape.` |
|       - |  306 | ` */` |
|   15454 |  307 | `static int OoOverrideTypeBad(ph7_vm *pVm, OvType parent, OvType child, int bCovariant)` |
|       5 |  308 | `{` |
|       - |  309 | `	ph7_class *pParentCls, *pChildCls;` |
|   15459 |  310 | `	int kP = OoClassifyOverrideType(pVm, parent.nType, parent.pClass, parent.bUnion, &pParentCls);` |
|   15459 |  311 | `	int kC = OoClassifyOverrideType(pVm, child.nType, child.pClass, child.bUnion, &pChildCls);` |
|   15459 |  312 | `	if( kP == OVT_SKIP \|\| kC == OVT_SKIP ){` |
|      18 |  313 | `		return 0; /* ambiguous shape — conservatively accept */` |
|       - |  314 | `	}` |
|       - |  315 | `	/* A missing type is the TOP type. covariant (return): a concrete child is a` |
|       - |  316 | `	 * subtype of top, fine; a top child over a concrete parent WIDENS → bad.` |
|       - |  317 | `	 * contravariant (param): a top child is a supertype of anything, fine; a` |
|       - |  318 | `	 * concrete child over a top parent NARROWS → bad. (A union/intersection child` |
|       - |  319 | `	 * already fell into OVT_SKIP above, so a flagged child here is scalar/class.) */` |
|   15443 |  320 | `	if( kP == OVT_NONE \|\| kC == OVT_NONE ){` |
|   15423 |  321 | `		if( bCovariant && kC == OVT_NONE && kP != OVT_NONE ) return 1;` |
|   15423 |  322 | `		if( !bCovariant && kP == OVT_NONE && kC != OVT_NONE ) return 1;` |
|   15423 |  323 | `		return 0;` |
|       - |  324 | `	}` |
|       - |  325 | `	/* Nullability: a covariant return may not ADD null; a contravariant param may` |
|       - |  326 | `	 * not REMOVE null. */` |
|      24 |  327 | `	if( bCovariant ){` |
|      11 |  328 | `		if( child.bNullable && !parent.bNullable ) return 1;` |
|       7 |  329 | `	}else{` |
|      14 |  330 | `		if( parent.bNullable && !child.bNullable ) return 1;` |
|       - |  331 | `	}` |
|      24 |  332 | `	if( kP == OVT_SCALAR && kC == OVT_SCALAR ){` |
|       - |  333 | `		/* Scalars are invariant — they must match exactly. */` |
|      17 |  334 | `		return (parent.nType != child.nType) ? 1 : 0;` |
|       - |  335 | `	}` |
|       8 |  336 | `	if( kP == OVT_CLASS && kC == OVT_CLASS ){` |
|       8 |  337 | `		if( bCovariant ){` |
|       3 |  338 | `			return PH7_VmInstanceOf(pChildCls, pParentCls) ? 0 : 1;  /* child ⊆ parent */` |
|       - |  339 | `		}` |
|       6 |  340 | `		return PH7_VmInstanceOf(pParentCls, pChildCls) ? 0 : 1;      /* child ⊇ parent */` |
|       - |  341 | `	}` |
|       - |  342 | `	/* One scalar and one class — disjoint. */` |
|     ! 0 |  343 | `	return 1;` |
|    7732 |  344 | `}` |
|       - |  345 |  |
|       - |  346 | `/*` |
|       - |  347 | ` * Check a child method's signature against the parent method it overrides.` |
|       - |  348 | ` * Emits a PHP-style "Declaration of … must be compatible …" fatal on a clear` |
|       - |  349 | `` * incompatibility. `__construct` is exempt (PHP does not apply variance to it).`` |
|       - |  350 | ` */` |
|   30826 |  351 | `static sxi32 OoCheckOverrideCompat(ph7_gen_state *pGen, ph7_class *pBase, ph7_class *pSub,` |
|       - |  352 | `	ph7_class_method *pParent, ph7_class_method *pChild)` |
|       5 |  353 | `{` |
|   30831 |  354 | `	ph7_vm *pVm = pGen->pVm;` |
|   30831 |  355 | `	ph7_vm_func *pPF = &pParent->sFunc;` |
|   30831 |  356 | `	ph7_vm_func *pCF = &pChild->sFunc;` |
|   30831 |  357 | `	SyString *pMName = &pCF->sName;` |
|       - |  358 | `	ph7_vm_func_arg *aP, *aC;` |
|       - |  359 | `	sxu32 nPArg, nCArg, k;` |
|   30831 |  360 | `	int bBad = 0;` |
|   30826 |  361 | `	if( pMName->nByte == sizeof("__construct")-1` |
|   23117 |  362 | `	 && SyStrnmicmp(pMName->zString,"__construct",pMName->nByte) == 0 ){` |
|   15395 |  363 | `		return SXRET_OK;` |
|       - |  364 | `	}` |
|       - |  365 | `	/* Return type — covariant. */` |
|   15441 |  366 | `	bBad = OoOverrideTypeBad(pVm, OoTypeFromReturn(pPF), OoTypeFromReturn(pCF), /* bCovariant */ 1);` |
|       - |  367 | `	/* Each overlapping parameter — contravariant. */` |
|   15441 |  368 | `	nPArg = SySetUsed(&pPF->aArgs);` |
|   15441 |  369 | `	nCArg = SySetUsed(&pCF->aArgs);` |
|   15441 |  370 | `	aP = (ph7_vm_func_arg *)SySetBasePtr(&pPF->aArgs);` |
|   15441 |  371 | `	aC = (ph7_vm_func_arg *)SySetBasePtr(&pCF->aArgs);` |
|   15459 |  372 | `	for( k = 0; !bBad && k < nPArg && k < nCArg; k++ ){` |
|      21 |  373 | `		bBad = OoOverrideTypeBad(pVm, OoTypeFromArg(&aP[k]), OoTypeFromArg(&aC[k]), /* bCovariant */ 0);` |
|      12 |  374 | `	}` |
|       - |  375 | `	/* Parameter arity: the child must declare at least the parent's parameters and` |
|       - |  376 | `	 * may add only OPTIONAL ones — PHP rejects dropping any param (even an optional` |
|       - |  377 | `	 * one) or adding a required one. Skip the rule if either signature is variadic` |
|       - |  378 | `	 * (arity semantics differ). */` |
|   15441 |  379 | `	if( !bBad ){` |
|   15437 |  380 | `		int bVariadic = 0;` |
|   15453 |  381 | `		for( k = 0; k < nPArg; k++ ){ if( aP[k].iFlags & VM_FUNC_ARG_VARIADIC ) bVariadic = 1; }` |
|   15455 |  382 | `		for( k = 0; k < nCArg; k++ ){ if( aC[k].iFlags & VM_FUNC_ARG_VARIADIC ) bVariadic = 1; }` |
|   15437 |  383 | `		if( !bVariadic ){` |
|   15437 |  384 | `			if( nCArg < nPArg ){` |
|     ! 0 |  385 | `				bBad = 1; /* dropped a parent parameter */` |
|     ! 0 |  386 | `			}else{` |
|   15439 |  387 | `				for( k = nPArg; k < nCArg; k++ ){` |
|       3 |  388 | `					if( SySetUsed(&aC[k].aByteCode) == 0 ){ bBad = 1; break; } /* new required */` |
|       2 |  389 | `				}` |
|       - |  390 | `			}` |
|    7716 |  391 | `		}` |
|    7716 |  392 | `	}` |
|   15441 |  393 | `	if( bBad ){` |
|       8 |  394 | `		sxi32 rc = PH7_GenCompileError(&(*pGen),E_ERROR,pChild->nLine,` |
|       - |  395 | `			"Declaration of %z::%z() must be compatible with %z::%z()",` |
|       2 |  396 | `			&pSub->sName,pMName,&pBase->sName,&pParent->sFunc.sName);` |
|       6 |  397 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  398 | `			return SXERR_ABORT;` |
|       - |  399 | `		}` |
|       2 |  400 | `	}` |
|   15441 |  401 | `	return SXRET_OK;` |
|   15418 |  402 | `}` |
|       - |  403 | `/*` |
|       - |  404 | ` * Perform an inheritance operation.` |
|       - |  405 | ` * According to the PHP language reference manual` |
|       - |  406 | ` *  When you extend a class, the subclass inherits all of the public and protected methods` |
|       - |  407 | ` *  from the parent class. Unless a class Overwrites those methods, they will retain their original` |
|       - |  408 | ` *  functionality.` |
|       - |  409 | ` *  This is useful for defining and abstracting functionality, and permits the implementation` |
|       - |  410 | ` *  of additional functionality in similar objects without the need to reimplement all of the shared` |
|       - |  411 | ` *  functionality.` |
|       - |  412 | ` *  Example #1 Inheritance Example` |
|       - |  413 | ` * <?php` |
|       - |  414 | ` * class foo` |
|       - |  415 | ` * {` |
|       - |  416 | ` *   public function printItem($string)` |
|       - |  417 | ` *   {` |
|       - |  418 | ` *       echo 'Foo: ' . $string . PHP_EOL;` |
|       - |  419 | ` *   }` |
|       - |  420 | ` *` |
|       - |  421 | ` *   public function printPHP()` |
|       - |  422 | ` *   {` |
|       - |  423 | ` *       echo 'PHP is great.' . PHP_EOL;` |
|       - |  424 | ` *   }` |
|       - |  425 | ` * }` |
|       - |  426 | ` * class bar extends foo` |
|       - |  427 | ` * {` |
|       - |  428 | ` *   public function printItem($string)` |
|       - |  429 | ` *   {` |
|       - |  430 | ` *       echo 'Bar: ' . $string . PHP_EOL;` |
|       - |  431 | ` *   }` |
|       - |  432 | ` * }` |
|       - |  433 | ` * $foo = new foo();` |
|       - |  434 | ` * $bar = new bar();` |
|       - |  435 | ` * $foo->printItem('baz'); // Output: 'Foo: baz'` |
|       - |  436 | ` * $foo->printPHP();       // Output: 'PHP is great'` |
|       - |  437 | ` * $bar->printItem('baz'); // Output: 'Bar: baz'` |
|       - |  438 | ` * $bar->printPHP();       // Output: 'PHP is great'` |
|       - |  439 | ` *` |
|       - |  440 | ` * This function return SXRET_OK if the inheritance operation was successfully performed.` |
|       - |  441 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|       - |  442 | ` * error message.` |
|       - |  443 | ` */` |
|  123272 |  444 | `PH7_PRIVATE sxi32 PH7_ClassInherit(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase)` |
|       5 |  445 | `{` |
|       - |  446 | `	ph7_class_method *pMeth;` |
|       - |  447 | `	ph7_class_attr *pAttr;` |
|       - |  448 | `	SyHashEntry *pEntry;` |
|       - |  449 | `	SyString *pName;` |
|       - |  450 | `	sxi32 rc;` |
|       - |  451 | `	/* Install in the derived hashtable */` |
|  123277 |  452 | `	rc = SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|  123277 |  453 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  454 | `		return rc;` |
|       - |  455 | `	}` |
|       - |  456 | `	/* readonly class inheritance (PHP 8.2): a readonly class may only extend a` |
|       - |  457 | `	 * readonly class, and a non-readonly class may not extend a readonly one. */` |
|  123277 |  458 | `	if( (pBase->iFlags & PH7_CLASS_READONLY) != (pSub->iFlags & PH7_CLASS_READONLY) ){` |
|       5 |  459 | `		if( pBase->iFlags & PH7_CLASS_READONLY ){` |
|       4 |  460 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|       - |  461 | `				"Non-readonly class %z cannot extend readonly class %z",` |
|       1 |  462 | `				&pSub->sName,&pBase->sName);` |
|       2 |  463 | `		}else{` |
|       4 |  464 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|       - |  465 | `				"Readonly class %z cannot extend non-readonly class %z",` |
|       1 |  466 | `				&pSub->sName,&pBase->sName);` |
|       - |  467 | `		}` |
|       5 |  468 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  469 | `			return SXERR_ABORT;` |
|       - |  470 | `		}` |
|       2 |  471 | `	}` |
|       - |  472 | `	/* Copy public/protected attributes from the base class */` |
|  123277 |  473 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
|  800607 |  474 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|       - |  475 | `		/* Make sure the private attributes are not redeclared in the subclass */` |
|  677335 |  476 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  677335 |  477 | `		pName = &pAttr->sName;` |
|  677335 |  478 | `		if( (pEntry = SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|       6 |  479 | `			if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_FINAL))` |
|       6 |  480 | `				== (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_FINAL) ){` |
|       - |  481 | `				/* Cannot override a final class constant (PHP 8.1). Report the` |
|       - |  482 | `				 * class that originally declared it (pDeclClass) rather than the` |
|       - |  483 | `				 * immediate base, so a multi-level chain matches PHP. */` |
|       3 |  484 | `				ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pBase;` |
|       4 |  485 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_attr *)pEntry->pUserData)->nLine,` |
|       - |  486 | `					"%z::%z cannot override final constant %z::%z",` |
|       1 |  487 | `					&pSub->sName,pName,&pOwner->sName,pName);` |
|       3 |  488 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  489 | `					return SXERR_ABORT;` |
|       - |  490 | `				}` |
|       1 |  491 | `			}` |
|       9 |  492 | `			if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE &&` |
|       2 |  493 | `				((ph7_class_attr *)pEntry->pUserData)->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       - |  494 | `					/* Cannot redeclare private attribute */` |
|       4 |  495 | `					PH7_GenCompileError(&(*pGen),E_WARNING,((ph7_class_attr *)pEntry->pUserData)->nLine,` |
|       - |  496 | `						"Private attribute '%z::%z' redeclared inside child class '%z'",` |
|       1 |  497 | `						&pBase->sName,pName,&pSub->sName);` |
|       - |  498 |  |
|       1 |  499 | `			}` |
|       9 |  500 | `			continue;` |
|       - |  501 | `		}` |
|       - |  502 | `		/* Install the attribute */` |
|  677329 |  503 | `		if( pAttr->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
|  677325 |  504 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|  677325 |  505 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  506 | `				return rc;` |
|       - |  507 | `			}` |
|  338660 |  508 | `		}` |
|       5 |  509 | `	}` |
|  123277 |  510 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
| 1854721 |  511 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|       - |  512 | `		/* Make sure the private/final methods are not redeclared in the subclass */` |
| 1731449 |  513 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
| 1731449 |  514 | `		pName = &pMeth->sFunc.sName;` |
| 1731449 |  515 | `		if( (pEntry = SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|   30835 |  516 | `			 if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|       - |  517 | `				/* Cannot Overwrite final method */` |
|       7 |  518 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_method *)pEntry->pUserData)->nLine,` |
|       - |  519 | `					"Cannot Overwrite final method '%z:%z' inside child class '%z'",` |
|       2 |  520 | `					&pBase->sName,pName,&pSub->sName);` |
|       5 |  521 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  522 | `					return SXERR_ABORT;` |
|       - |  523 | `				}` |
|       3 |  524 | `			}else{` |
|       - |  525 | `				/* Check the override's signature is compatible with the parent's. */` |
|   46244 |  526 | `				rc = OoCheckOverrideCompat(&(*pGen),pBase,pSub,pMeth,` |
|   30826 |  527 | `					(ph7_class_method *)pEntry->pUserData);` |
|   30831 |  528 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  529 | `					return SXERR_ABORT;` |
|       - |  530 | `				}` |
|       - |  531 | `			}` |
|   30835 |  532 | `			continue;` |
|       - |  533 | `		}` |
|       - |  534 | `		/* Install the method */` |
| 1700619 |  535 | `		if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
| 1700615 |  536 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 1700615 |  537 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  538 | `				return rc;` |
|       - |  539 | `			}` |
|  850305 |  540 | `		}` |
|       5 |  541 | `	}` |
|       - |  542 | `	/* Mark as subclass */` |
|  123277 |  543 | `	pSub->pBase = pBase;` |
|       - |  544 | `	/* All done */` |
|  123277 |  545 | `	return SXRET_OK;` |
|   61641 |  546 | `}` |
|       - |  547 | `/*` |
|       - |  548 | ` * Apply a trait to a class: copy all methods and attributes from the trait` |
|       - |  549 | ` * into the target class. Unlike inheritance, traits copy ALL members including` |
|       - |  550 | ` * private ones. Members already defined in the class take precedence.` |
|       - |  551 | ` */` |
|      52 |  552 | `PH7_PRIVATE sxi32 PH7_ClassUseTrait(ph7_gen_state *pGen,ph7_class *pClass,ph7_class *pTrait)` |
|       5 |  553 | `{` |
|       - |  554 | `	ph7_class_method *pMeth;` |
|       - |  555 | `	ph7_class_attr *pAttr;` |
|       - |  556 | `	SyHashEntry *pEntry;` |
|       - |  557 | `	SyString *pName;` |
|       - |  558 | `	sxi32 rc;` |
|       - |  559 | `	/* Detect cyclic trait composition (e.g. trait A { use B; } trait B { use A; }) */` |
|      57 |  560 | `	if( pTrait->iFlags & PH7_CLASS_TRAIT_VISITING ){` |
|     ! 0 |  561 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|     ! 0 |  562 | `			"Trait circular reference detected: %z is already being applied",&pTrait->sName);` |
|     ! 0 |  563 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  564 | `			return SXERR_ABORT;` |
|       - |  565 | `		}` |
|     ! 0 |  566 | `		return SXRET_OK;` |
|       - |  567 | `	}` |
|      57 |  568 | `	pTrait->iFlags \|= PH7_CLASS_TRAIT_VISITING;` |
|      57 |  569 | `	rc = SXRET_OK;` |
|       - |  570 | `	/* Copy attributes from the trait */` |
|      57 |  571 | `	SyHashResetLoopCursor(&pTrait->hAttr);` |
|      73 |  572 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hAttr)) != 0 ){` |
|       - |  573 | `		SyHashEntry *pExisting;` |
|      20 |  574 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      20 |  575 | `		pName = &pAttr->sName;` |
|      20 |  576 | `		pExisting = SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte);` |
|      20 |  577 | `		if( pExisting != 0 ){` |
|       - |  578 | `			/* Attribute already exists. Check if it came from another trait` |
|       - |  579 | `			 * and whether the definitions are compatible (same defaults).` |
|       - |  580 | `			 */` |
|       - |  581 | `			ph7_class **apUsedTraits;` |
|       - |  582 | `			sxu32 nUsed,k;` |
|       6 |  583 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       6 |  584 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|       6 |  585 | `			for(k = 0; k < nUsed; k++){` |
|       - |  586 | `				ph7_class_attr *pOther;` |
|       3 |  587 | `				pOther = PH7_ClassExtractAttribute(apUsedTraits[k],pName->zString,pName->nByte);` |
|       3 |  588 | `				if( pOther ){` |
|       - |  589 | `					/* Two traits define the same property — check if defaults differ */` |
|       3 |  590 | `					ph7_class_attr *pClassAttr = (ph7_class_attr *)pExisting->pUserData;` |
|       4 |  591 | `					if( SySetUsed(&pAttr->aByteCode) != SySetUsed(&pClassAttr->aByteCode) \|\|` |
|       3 |  592 | `						(SySetUsed(&pAttr->aByteCode) > 0 &&` |
|       3 |  593 | `						 SyMemcmp(SySetBasePtr(&pAttr->aByteCode),SySetBasePtr(&pClassAttr->aByteCode),` |
|       3 |  594 | `							SySetUsed(&pAttr->aByteCode) * SySetElemSize(&pAttr->aByteCode)) != 0) ){` |
|       4 |  595 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine,` |
|       - |  596 | `							"%z and %z define the same property ($%z) in the composition of %z. "` |
|       - |  597 | `							"However, the definition differs and is considered incompatible",` |
|       2 |  598 | `							&apUsedTraits[k]->sName,&pTrait->sName,pName,&pClass->sName);` |
|       3 |  599 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  600 | `							goto cleanup;` |
|       - |  601 | `						}` |
|       1 |  602 | `					}` |
|       3 |  603 | `					break;` |
|       - |  604 | `				}` |
|     ! 0 |  605 | `			}` |
|       6 |  606 | `			continue;` |
|       - |  607 | `		}` |
|      16 |  608 | `		rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|      16 |  609 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  610 | `			goto cleanup;` |
|       - |  611 | `		}` |
|       4 |  612 | `	}` |
|       - |  613 | `	/* Copy methods from the trait */` |
|      57 |  614 | `	SyHashResetLoopCursor(&pTrait->hMethod);` |
|     105 |  615 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hMethod)) != 0 ){` |
|      53 |  616 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      53 |  617 | `		pName = &pMeth->sFunc.sName;` |
|      53 |  618 | `		if( SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte) != 0 ){` |
|       - |  619 | `			/* Method already exists in the class. Check if it came from another trait` |
|       - |  620 | `			 * (unresolved conflict) vs being defined by the class itself.` |
|       - |  621 | `			 */` |
|       - |  622 | `			ph7_class **apUsedTraits;` |
|       - |  623 | `			sxu32 nUsed,k;` |
|      11 |  624 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|      11 |  625 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|      11 |  626 | `			for(k = 0; k < nUsed; k++){` |
|       3 |  627 | `				if( PH7_ClassExtractMethod(apUsedTraits[k],pName->zString,pName->nByte) != 0 ){` |
|       - |  628 | `					/* Two different traits define the same method with no resolution */` |
|       4 |  629 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|       - |  630 | `						"Trait method %z::%z has not been applied as %z::%z, "` |
|       - |  631 | `						"because of collision with %z::%z",` |
|       2 |  632 | `						&pTrait->sName,pName,` |
|       1 |  633 | `						&pClass->sName,pName,` |
|       2 |  634 | `						&apUsedTraits[k]->sName,pName);` |
|       3 |  635 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  636 | `						goto cleanup;` |
|       - |  637 | `					}` |
|       3 |  638 | `					break;` |
|       - |  639 | `				}` |
|     ! 0 |  640 | `			}` |
|       - |  641 | `			/* Class-defined method takes precedence */` |
|      11 |  642 | `			continue;` |
|       - |  643 | `		}` |
|      45 |  644 | `		rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|      45 |  645 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  646 | `			goto cleanup;` |
|       - |  647 | `		}` |
|       5 |  648 | `	}` |
|       - |  649 | `	/* Record trait in the class */` |
|      57 |  650 | `	SySetPut(&pClass->aTrait,(const void *)&pTrait);` |
|      26 |  651 | `cleanup:` |
|       - |  652 | `	/* Always clear visiting flag, even on error paths */` |
|      57 |  653 | `	pTrait->iFlags &= ~PH7_CLASS_TRAIT_VISITING;` |
|      26 |  654 | `	SXUNUSED(pGen);` |
|      57 |  655 | `	return rc;` |
|      31 |  656 | `}` |
|       - |  657 | `/*` |
|       - |  658 | ` * Inherit an object interface from another object interface.` |
|       - |  659 | ` * According to the PHP language reference manual.` |
|       - |  660 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|       - |  661 | ` *  must implement, without having to define how these methods are handled.` |
|       - |  662 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  663 | ` *  class, but without any of the methods having their contents defined.` |
|       - |  664 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  665 | ` *` |
|       - |  666 | ` * This function return SXRET_OK if the interface inheritance operation was successfully performed.` |
|       - |  667 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|       - |  668 | ` * error message.` |
|       - |  669 | ` */` |
|   15394 |  670 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceInherit(ph7_class *pSub,ph7_class *pBase)` |
|       5 |  671 | `{` |
|       - |  672 | `	ph7_class_method *pMeth;` |
|       - |  673 | `	ph7_class_attr *pAttr;` |
|       - |  674 | `	SyHashEntry *pEntry;` |
|       - |  675 | `	SyString *pName;` |
|       - |  676 | `	sxi32 rc;` |
|       - |  677 | `	/* Install in the derived hashtable */` |
|   15399 |  678 | `	SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|   15399 |  679 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
|       - |  680 | `	/* Copy constants */` |
|   23098 |  681 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|       - |  682 | `		/* Make sure the constants are not redeclared in the subclass */` |
|       3 |  683 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|       3 |  684 | `		pName = &pAttr->sName;` |
|       3 |  685 | `		if( SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte) == 0 ){` |
|       - |  686 | `			/* Install the constant in the subclass */` |
|       3 |  687 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|       3 |  688 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  689 | `				return rc;` |
|       - |  690 | `			}` |
|       1 |  691 | `		}` |
|       1 |  692 | `	}` |
|   15399 |  693 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
|       - |  694 | `	/* Copy methods signature */` |
|   30830 |  695 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|       - |  696 | `		/* Make sure the method are not redeclared in the subclass */` |
|    7739 |  697 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|    7739 |  698 | `		pName = &pMeth->sFunc.sName;` |
|    7739 |  699 | `		if( SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte) == 0 ){` |
|       - |  700 | `			/* Install the method */` |
|    7739 |  701 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|    7739 |  702 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  703 | `				return rc;` |
|       - |  704 | `			}` |
|    3867 |  705 | `		}` |
|       5 |  706 | `	}` |
|       - |  707 | `	/* Mark as subclass */` |
|   15399 |  708 | `	pSub->pBase = pBase;` |
|       - |  709 | `	/* All done */` |
|   15399 |  710 | `	return SXRET_OK;` |
|    7702 |  711 | `}` |
|       - |  712 | `/*` |
|       - |  713 | ` * Implements an object interface in the given main class.` |
|       - |  714 | ` * According to the PHP language reference manual.` |
|       - |  715 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|       - |  716 | ` *  must implement, without having to define how these methods are handled.` |
|       - |  717 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  718 | ` *  class, but without any of the methods having their contents defined.` |
|       - |  719 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  720 | ` *` |
|       - |  721 | ` * This function return SXRET_OK if the interface was successfully implemented.` |
|       - |  722 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|       - |  723 | ` * error message.` |
|       - |  724 | ` */` |
|  211808 |  725 | `PH7_PRIVATE sxi32 PH7_ClassImplement(ph7_class *pMain,ph7_class *pInterface)` |
|       5 |  726 | `{` |
|       - |  727 | `	ph7_class_attr *pAttr;` |
|       - |  728 | `	SyHashEntry *pEntry;` |
|       - |  729 | `	SyString *pName;` |
|       - |  730 | `	sxi32 rc;` |
|       - |  731 | `	/* First off,copy all constants declared inside the interface */` |
|  211813 |  732 | `	SyHashResetLoopCursor(&pInterface->hAttr);` |
|  317723 |  733 | `	while((pEntry = SyHashGetNextEntry(&pInterface->hAttr)) != 0 ){` |
|       - |  734 | `		/* Point to the constant declaration */` |
|       7 |  735 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|       7 |  736 | `		pName = &pAttr->sName;` |
|       - |  737 | `		/* Make sure the attribute is not redeclared in the main class */` |
|       7 |  738 | `		if( SyHashGet(&pMain->hAttr,pName->zString,pName->nByte) == 0 ){` |
|       - |  739 | `			/* Install the attribute */` |
|       7 |  740 | `			rc = SyHashInsert(&pMain->hAttr,pName->zString,pName->nByte,pAttr);` |
|       7 |  741 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  742 | `				return rc;` |
|       - |  743 | `			}` |
|       3 |  744 | `		}` |
|       1 |  745 | `	}` |
|       - |  746 | `	/* Install in the interface container */` |
|  211813 |  747 | `	SySetPut(&pMain->aInterface,(const void *)&pInterface);` |
|       - |  748 | `	/* Install interface method stubs into the implementing class.` |
|       - |  749 | `	 * Methods already defined in the class take precedence (they satisfy` |
|       - |  750 | `	 * the interface contract). Stubs retain PH7_CLASS_ATTR_ABSTRACT so` |
|       - |  751 | `	 * the unified check in GenStateCheckAbstractMethods catches missing ones.` |
|       - |  752 | `	 */` |
|       - |  753 | `	{` |
|       - |  754 | `		ph7_class_method *pMeth;` |
|       - |  755 | `		SyHashEntry *pMEntry;` |
|       - |  756 | `		SyString *pMName;` |
|  211813 |  757 | `		SyHashResetLoopCursor(&pInterface->hMethod);` |
|  598977 |  758 | `		while((pMEntry = SyHashGetNextEntry(&pInterface->hMethod)) != 0 ){` |
|  281265 |  759 | `			pMeth = (ph7_class_method *)pMEntry->pUserData;` |
|  281265 |  760 | `			pMName = &pMeth->sFunc.sName;` |
|  281265 |  761 | `			if( SyHashGet(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte) == 0 ){` |
|      18 |  762 | `				rc = SyHashInsert(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte,pMeth);` |
|      18 |  763 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  764 | `					return rc;` |
|       - |  765 | `				}` |
|       7 |  766 | `			}` |
|       5 |  767 | `		}` |
|       - |  768 | `	}` |
|  211813 |  769 | `	return SXRET_OK;` |
|  105909 |  770 | `}` |
|       - |  771 | `/*` |
|       - |  772 | ` * Create a class instance [i.e: Object in the PHP jargon] at run-time.` |
|       - |  773 | ` * The following function is called when an object is created at run-time` |
|       - |  774 | ` * typically when the PH7_OP_NEW/PH7_OP_CLONE instructions are executed.` |
|       - |  775 | ` * Notes on object creation.` |
|       - |  776 | ` *` |
|       - |  777 | ` * According to PHP language reference manual.` |
|       - |  778 | ` * To create an instance of a class, the new keyword must be used. An object will always` |
|       - |  779 | ` * be created unless the object has a constructor defined that throws an exception on error.` |
|       - |  780 | ` * Classes should be defined before instantiation (and in some cases this is a requirement).` |
|       - |  781 | ` * If a string containing the name of a class is used with new, a new instance of that class` |
|       - |  782 | ` * will be created. If the class is in a namespace, its fully qualified name must be used when` |
|       - |  783 | ` * doing this.` |
|       - |  784 | ` * Example #3 Creating an instance` |
|       - |  785 | ` * <?php` |
|       - |  786 | ` *  $instance = new SimpleClass();` |
|       - |  787 | ` *   // This can also be done with a variable:` |
|       - |  788 | ` * $className = 'Foo';` |
|       - |  789 | ` * $instance = new $className(); // Foo()` |
|       - |  790 | ` * ?>` |
|       - |  791 | ` * In the class context, it is possible to create a new object by new self and new parent.` |
|       - |  792 | ` * When assigning an already created instance of a class to a new variable, the new variable` |
|       - |  793 | ` * will access the same instance as the object that was assigned. This behaviour is the same` |
|       - |  794 | ` * when passing instances to a function. A copy of an already created object can be made by` |
|       - |  795 | ` * cloning it.` |
|       - |  796 | ` * Example #4 Object Assignment` |
|       - |  797 | ` * <?php` |
|       - |  798 | ` *  class SimpleClass(){` |
|       - |  799 | ` *    public $var;` |
|       - |  800 | ` *  };` |
|       - |  801 | ` *  $instance = new SimpleClass();` |
|       - |  802 | ` *  $assigned   =  $instance;` |
|       - |  803 | ` *  $reference  =& $instance;` |
|       - |  804 | ` *  $instance->var = '$assigned will have this value';` |
|       - |  805 | ` *  $instance = null; // $instance and $reference become null` |
|       - |  806 | ` *  var_dump($instance);` |
|       - |  807 | ` *  var_dump($reference);` |
|       - |  808 | ` *  var_dump($assigned);` |
|       - |  809 | ` * ?>` |
|       - |  810 | ` * The above example will output:` |
|       - |  811 | ` * NULL` |
|       - |  812 | ` * NULL` |
|       - |  813 | ` * object(SimpleClass)#1 (1) {` |
|       - |  814 | ` *  ["var"]=>` |
|       - |  815 | ` *    string(30) "$assigned will have this value"` |
|       - |  816 | ` * }` |
|       - |  817 | ` * Example #5 Creating new objects` |
|       - |  818 | ` * <?php` |
|       - |  819 | ` * class Test` |
|       - |  820 | ` * {` |
|       - |  821 | ` *   static public function getNew()` |
|       - |  822 | ` *   {` |
|       - |  823 | ` *       return new static;` |
|       - |  824 | ` *   }` |
|       - |  825 | ` * }` |
|       - |  826 | ` * class Child extends Test` |
|       - |  827 | ` * {}` |
|       - |  828 | ` * $obj1 = new Test();` |
|       - |  829 | ` * $obj2 = new $obj1;` |
|       - |  830 | ` * var_dump($obj1 !== $obj2);` |
|       - |  831 | ` * $obj3 = Test::getNew();` |
|       - |  832 | ` * var_dump($obj3 instanceof Test);` |
|       - |  833 | ` * $obj4 = Child::getNew();` |
|       - |  834 | ` * var_dump($obj4 instanceof Child);` |
|       - |  835 | ` * ?>` |
|       - |  836 | ` * The above example will output:` |
|       - |  837 | ` * bool(true)` |
|       - |  838 | ` * bool(true)` |
|       - |  839 | ` * bool(true)` |
|       - |  840 | ` * Note that Symisc Systems have introduced powerfull extension to` |
|       - |  841 | ` * OO subsystem. For example a class attribute may have any complex` |
|       - |  842 | ` * expression associated with it when declaring the attribute unlike` |
|       - |  843 | ` * the standard PHP engine which would allow a single value.` |
|       - |  844 | ` * Example:` |
|       - |  845 | ` *  class myClass{` |
|       - |  846 | ` *    public $var = 25<<1+foo()/bar();` |
|       - |  847 | ` *  };` |
|       - |  848 | ` * Refer to the official documentation for more information.` |
|       - |  849 | ` */` |
|    5692 |  850 | `static ph7_class_instance * NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|       5 |  851 | `{` |
|       - |  852 | `	ph7_class_instance *pThis;` |
|       - |  853 | `	/* Allocate a new instance */` |
|    5697 |  854 | `	pThis = (ph7_class_instance *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_instance));` |
|    5697 |  855 | `	if( pThis == 0 ){` |
|     ! 0 |  856 | `		return 0;` |
|       - |  857 | `	}` |
|       - |  858 | `	/* Zero the structure */` |
|    5697 |  859 | `	SyZero(pThis,sizeof(ph7_class_instance));` |
|       - |  860 | `	/* Initialize fields */` |
|    5697 |  861 | `	pThis->iRef = 1;` |
|    5697 |  862 | `	pThis->pVm = pVm;` |
|    5697 |  863 | `	pThis->pClass = pClass;` |
|       - |  864 | `	/* Assign a fresh monotonic object handle id (clones get their own, like PHP). */` |
|    5697 |  865 | `	pThis->nObjId = pVm->nNextObjId++;` |
|    5697 |  866 | `	SyHashInit(&pThis->hAttr,&pVm->sAllocator,0,0);` |
|    5697 |  867 | `	return pThis;` |
|    2851 |  868 | `}` |
|       - |  869 | `/*` |
|       - |  870 | ` * Wrapper around the NewClassInstance() function defined above.` |
|       - |  871 | ` * See the block comment above for more information.` |
|       - |  872 | ` */` |
|    5584 |  873 | `PH7_PRIVATE ph7_class_instance * PH7_NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|       5 |  874 | `{` |
|       - |  875 | `	ph7_class_instance *pNew;` |
|       - |  876 | `	sxi32 rc;` |
|    5589 |  877 | `	pNew = NewClassInstance(&(*pVm),&(*pClass));` |
|    5589 |  878 | `	if( pNew == 0 ){` |
|     ! 0 |  879 | `		return 0;` |
|       - |  880 | `	}` |
|       - |  881 | `	/* Associate a private VM frame with this class instance */` |
|    5589 |  882 | `	rc = PH7_VmCreateClassInstanceFrame(&(*pVm),pNew);` |
|    5589 |  883 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  884 | `		SyMemBackendPoolFree(&pVm->sAllocator,pNew);` |
|     ! 0 |  885 | `		return 0;` |
|       - |  886 | `	}` |
|    5589 |  887 | `	return pNew;` |
|    2797 |  888 | `}` |
|       - |  889 | `/*` |
|       - |  890 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon] attribute.` |
|       - |  891 | ` * This function never fail.` |
|       - |  892 | ` */` |
|    7832 |  893 | `static ph7_value * ExtractClassAttrValue(ph7_vm *pVm,VmClassAttr *pAttr)` |
|       5 |  894 | `{` |
|       - |  895 | `	/* Extract the value */` |
|       - |  896 | `	ph7_value *pValue;` |
|    7837 |  897 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|    7837 |  898 | `	return pValue;` |
|       5 |  899 | `}` |
|       - |  900 | `/*` |
|       - |  901 | ` * Perform a clone operation on a class instance [i.e: Object in the PHP jargon].` |
|       - |  902 | ` * The following function is called when an object is cloned at run-time` |
|       - |  903 | ` * typically when the PH7_OP_CLONE instruction is executed.` |
|       - |  904 | ` * Notes on object cloning.` |
|       - |  905 | ` *` |
|       - |  906 | ` * According to PHP language reference manual.` |
|       - |  907 | ` * Creating a copy of an object with fully replicated properties is not always the wanted behavior.` |
|       - |  908 | ` * A good example of the need for copy constructors. Another example is if your object holds a reference` |
|       - |  909 | ` * to another object which it uses and when you replicate the parent object you want to create` |
|       - |  910 | ` * a new instance of this other object so that the replica has its own separate copy.` |
|       - |  911 | ` * An object copy is created by using the clone keyword (which calls the object's __clone() method if possible).` |
|       - |  912 | ` * An object's __clone() method cannot be called directly.` |
|       - |  913 | ` * $copy_of_object = clone $object;` |
|       - |  914 | ` * When an object is cloned, PHP 5 will perform a shallow copy of all of the object's properties.` |
|       - |  915 | ` * Any properties that are references to other variables, will remain references.` |
|       - |  916 | ` * Once the cloning is complete, if a __clone() method is defined, then the newly created object's __clone() method` |
|       - |  917 | ` * will be called, to allow any necessary properties that need to be changed.` |
|       - |  918 | ` * Example #1 Cloning an object` |
|       - |  919 | ` * <?php` |
|       - |  920 | ` * class SubObject` |
|       - |  921 | ` * {` |
|       - |  922 | ` *   static $instances = 0;` |
|       - |  923 | ` *   public $instance;` |
|       - |  924 | ` *` |
|       - |  925 | ` *   public function __construct() {` |
|       - |  926 | ` *       $this->instance = ++self::$instances;` |
|       - |  927 | ` *   }` |
|       - |  928 | ` *` |
|       - |  929 | ` *   public function __clone() {` |
|       - |  930 | ` *       $this->instance = ++self::$instances;` |
|       - |  931 | ` *   }` |
|       - |  932 | ` * }` |
|       - |  933 | ` *` |
|       - |  934 | ` * class MyCloneable` |
|       - |  935 | ` * {` |
|       - |  936 | ` *   public $object1;` |
|       - |  937 | ` *   public $object2;` |
|       - |  938 | ` *` |
|       - |  939 | ` *   function __clone()` |
|       - |  940 | ` *   {` |
|       - |  941 | ` *       // Force a copy of this->object, otherwise` |
|       - |  942 | ` *       // it will point to same object.` |
|       - |  943 | ` *       $this->object1 = clone $this->object1;` |
|       - |  944 | ` *   }` |
|       - |  945 | ` * }` |
|       - |  946 | ` * $obj = new MyCloneable();` |
|       - |  947 | ` * $obj->object1 = new SubObject();` |
|       - |  948 | ` * $obj->object2 = new SubObject();` |
|       - |  949 | ` * $obj2 = clone $obj;` |
|       - |  950 | ` * print("Original Object:\n");` |
|       - |  951 | ` * print_r($obj);` |
|       - |  952 | ` * print("Cloned Object:\n");` |
|       - |  953 | ` * print_r($obj2);` |
|       - |  954 | ` * ?>` |
|       - |  955 | ` * The above example will output:` |
|       - |  956 | ` * Original Object:` |
|       - |  957 | ` * MyCloneable Object` |
|       - |  958 | ` * (` |
|       - |  959 | ` *   [object1] => SubObject Object` |
|       - |  960 | ` *       (` |
|       - |  961 | ` *           [instance] => 1` |
|       - |  962 | ` *       )` |
|       - |  963 | ` *` |
|       - |  964 | ` *   [object2] => SubObject Object` |
|       - |  965 | ` *       (` |
|       - |  966 | ` *           [instance] => 2` |
|       - |  967 | ` *       )` |
|       - |  968 | ` *` |
|       - |  969 | ` * )` |
|       - |  970 | ` * Cloned Object:` |
|       - |  971 | ` * MyCloneable Object` |
|       - |  972 | ` * (` |
|       - |  973 | ` *   [object1] => SubObject Object` |
|       - |  974 | ` *       (` |
|       - |  975 | ` *           [instance] => 3` |
|       - |  976 | ` *       )` |
|       - |  977 | ` *` |
|       - |  978 | ` *   [object2] => SubObject Object` |
|       - |  979 | ` *       (` |
|       - |  980 | ` *           [instance] => 2` |
|       - |  981 | ` *       )` |
|       - |  982 | ` * )` |
|       - |  983 | ` */` |
|     108 |  984 | `PH7_PRIVATE ph7_class_instance * PH7_CloneClassInstance(ph7_class_instance *pSrc)` |
|       3 |  985 | `{` |
|       - |  986 | `	ph7_class_instance *pClone;` |
|       - |  987 | `	ph7_class_method *pMethod;` |
|       - |  988 | `	SyHashEntry *pEntry2;` |
|       - |  989 | `	SyHashEntry *pEntry;` |
|       - |  990 | `	ph7_vm *pVm;` |
|       - |  991 | `	sxi32 rc;` |
|       - |  992 | `	/* Allocate a new instance */` |
|     111 |  993 | `	pVm = pSrc->pVm;` |
|     111 |  994 | `	pClone = NewClassInstance(pVm,pSrc->pClass);` |
|     111 |  995 | `	if( pClone == 0 ){` |
|     ! 0 |  996 | `		return 0;` |
|       - |  997 | `	}` |
|       - |  998 | `	/* Associate a private VM frame with this class instance */` |
|     111 |  999 | `	rc = PH7_VmCreateClassInstanceFrame(pVm,pClone);` |
|     111 | 1000 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1001 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClone);` |
|     ! 0 | 1002 | `		return 0;` |
|       - | 1003 | `	}` |
|       - | 1004 | `	/* Duplicate object values. Iterate the SOURCE attributes and copy each into` |
|       - | 1005 | `	 * the clone's same-named slot (looked up by name, so order/count differences` |
|       - | 1006 | `	 * from dynamic properties don't matter). A dynamic (runtime-added) property` |
|       - | 1007 | `	 * has no declared counterpart in the clone, so synthesize it first — without` |
|       - | 1008 | `	 * this, a clone of a stdClass would silently lose all its dynamic properties. */` |
|     111 | 1009 | `	SyHashResetLoopCursor(&pSrc->hAttr);` |
|     293 | 1010 | `	while((pEntry = SyHashGetNextEntry(&pSrc->hAttr)) != 0 ){` |
|     185 | 1011 | `		VmClassAttr *pSrcAttr = (VmClassAttr *)pEntry->pUserData;` |
|     185 | 1012 | `		VmClassAttr *pDestAttr = 0;` |
|     185 | 1013 | `		ph7_value *pvSrc,*pvDest = 0;` |
|       - | 1014 | `		/* Duplicate non-static attribute */` |
|     185 | 1015 | `		if( pSrcAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|     ! 0 | 1016 | `			continue;` |
|       - | 1017 | `		}` |
|     185 | 1018 | `		pEntry2 = SyHashGet(&pClone->hAttr,SyStringData(&pSrcAttr->pAttr->sName),SyStringLength(&pSrcAttr->pAttr->sName));` |
|     185 | 1019 | `		if( pEntry2 ){` |
|     171 | 1020 | `			pDestAttr = (VmClassAttr *)pEntry2->pUserData;` |
|     171 | 1021 | `			pvDest = ExtractClassAttrValue(pVm,pDestAttr);` |
|      99 | 1022 | `		}else if( pSrcAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){` |
|       - | 1023 | `			/* Dynamic property: synthesize the matching slot on the clone. */` |
|      22 | 1024 | `			pvDest = PH7_VmCreateDynamicAttr(pVm,pClone,` |
|      14 | 1025 | `				SyStringData(&pSrcAttr->pAttr->sName),SyStringLength(&pSrcAttr->pAttr->sName),&pDestAttr);` |
|       7 | 1026 | `		}` |
|       - | 1027 | `		/* Fetch the source value LAST: PH7_VmCreateDynamicAttr above may have` |
|       - | 1028 | `		 * reserved a slot and reallocated pVm->aMemObj, which would dangle any` |
|       - | 1029 | `		 * ph7_value* obtained before it. pvDest from the synth path already points` |
|       - | 1030 | `		 * into the post-realloc aMemObj; resolve pvSrc now so both are current. */` |
|     185 | 1031 | `		pvSrc = ExtractClassAttrValue(pVm,pSrcAttr);` |
|     185 | 1032 | `		if( pvSrc && pvDest ){` |
|     185 | 1033 | `			PH7_MemObjStore(pvSrc,pvDest);` |
|      91 | 1034 | `		}` |
|       - | 1035 | `		/* Carry over the per-instance state so the clone matches the source:` |
|       - | 1036 | `		 * VM_CLASS_ATTR_UNINIT marks a typed property as not-yet-initialized` |
|       - | 1037 | `		 * and doubles as the readonly write-once latch — without this a clone` |
|       - | 1038 | `		 * would reset to uninitialized (losing the value's readiness) and a` |
|       - | 1039 | `		 * readonly property would become writable again. */` |
|     185 | 1040 | `		if( pDestAttr ){` |
|     185 | 1041 | `			pDestAttr->iState = pSrcAttr->iState;` |
|      91 | 1042 | `		}` |
|       3 | 1043 | `	}` |
|       - | 1044 | `	/* A declared property unset() on the source is absent from the clone too (PHP). But the clone` |
|       - | 1045 | `	 * frame above materialized ALL declared attrs (with their defaults), so drop any clone attr whose` |
|       - | 1046 | `	 * name is not present on the source. Collect first, then delete — removing an entry mid-walk would` |
|       - | 1047 | `	 * free the node the SyHash loop cursor points at. */` |
|       - | 1048 | `	{` |
|       - | 1049 | `		SySet sDrop;` |
|     111 | 1050 | `		SySetInit(&sDrop,&pVm->sAllocator,sizeof(VmClassAttr *));` |
|     111 | 1051 | `		SyHashResetLoopCursor(&pClone->hAttr);` |
|     295 | 1052 | `		while((pEntry = SyHashGetNextEntry(&pClone->hAttr)) != 0 ){` |
|     187 | 1053 | `			VmClassAttr *pCloneAttr = (VmClassAttr *)pEntry->pUserData;` |
|     187 | 1054 | `			if( pCloneAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|     ! 0 | 1055 | `				continue;` |
|       - | 1056 | `			}` |
|     276 | 1057 | `			if( SyHashGet(&pSrc->hAttr,SyStringData(&pCloneAttr->pAttr->sName),` |
|     279 | 1058 | `					SyStringLength(&pCloneAttr->pAttr->sName)) == 0 ){` |
|       3 | 1059 | `				SySetPut(&sDrop,(const void *)&pCloneAttr);` |
|       1 | 1060 | `			}` |
|       3 | 1061 | `		}` |
|     111 | 1062 | `		if( SySetUsed(&sDrop) > 0 ){` |
|       3 | 1063 | `			VmClassAttr **apDrop = (VmClassAttr **)SySetBasePtr(&sDrop);` |
|       - | 1064 | `			sxu32 i;` |
|       5 | 1065 | `			for( i = 0 ; i < SySetUsed(&sDrop) ; ++i ){` |
|       3 | 1066 | `				VmClassAttr *pVmAttr = apDrop[i];` |
|       4 | 1067 | `				SyHashDeleteEntry(&pClone->hAttr,SyStringData(&pVmAttr->pAttr->sName),` |
|       2 | 1068 | `					SyStringLength(&pVmAttr->pAttr->sName),0);` |
|       3 | 1069 | `				PH7_VmReleaseInstanceAttr(pVm,pVmAttr);` |
|       2 | 1070 | `			}` |
|       1 | 1071 | `		}` |
|     111 | 1072 | `		SySetRelease(&sDrop);` |
|       - | 1073 | `	}` |
|       - | 1074 | `	/* call the __clone method on the cloned object if available */` |
|     111 | 1075 | `	pMethod = PH7_ClassExtractMethod(pClone->pClass,"__clone",sizeof("__clone")-1);` |
|     111 | 1076 | `	if( pMethod ){` |
|      56 | 1077 | `		if( pMethod->iCloneDepth < 16 ){` |
|      54 | 1078 | `			pMethod->iCloneDepth++;` |
|      54 | 1079 | `			PH7_VmCallClassMethod(pVm,pClone,pMethod,0,0,0);` |
|      28 | 1080 | `		}else{` |
|       - | 1081 | `			/* Nesting limit reached */` |
|       3 | 1082 | `			PH7_VmThrowError(pVm,0,PH7_CTX_ERR,"Object clone limit reached,no more call to __clone()");` |
|       - | 1083 | `		}` |
|       - | 1084 | `		/* Reset the cursor */` |
|      56 | 1085 | `		pMethod->iCloneDepth = 0;` |
|      27 | 1086 | `	}` |
|       - | 1087 | `	/* Return the cloned object */` |
|     111 | 1088 | `	return pClone;` |
|      57 | 1089 | `}` |
|       - | 1090 | `#define CLASS_INSTANCE_DESTROYED 0x001 /* Instance is released */` |
|       - | 1091 | `/*` |
|       - | 1092 | ` * Free the per-instance allocations owned by ONE object attribute: its value slot (+ the typed-slot` |
|       - | 1093 | ` * enforcement entry), the synthesized ph7_class_attr for a dynamic (runtime-added) property, and the` |
|       - | 1094 | ` * VmClassAttr wrapper itself. Does NOT touch the hAttr entry node — the caller removes it` |
|       - | 1095 | `` * (`unset($o->p)` via SyHashDeleteEntry2; instance teardown via the wholesale SyHashRelease, so it must`` |
|       - | 1096 | ` * not delete entries mid-walk). Shared by PH7_ClassInstanceRelease and the OP_MEMBER unset path.` |
|       - | 1097 | ` */` |
|   16014 | 1098 | `PH7_PRIVATE void PH7_VmReleaseInstanceAttr(ph7_vm *pVm, VmClassAttr *pVmAttr)` |
|       5 | 1099 | `{` |
|   16019 | 1100 | `	if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|       - | 1101 | `		/* Drop any typed-property enforcement slot registered for this memobj, before the memobj` |
|       - | 1102 | `		 * is returned to the free list, so a future recycled slot does not inherit the stale entry. */` |
|   14313 | 1103 | `		if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|     244 | 1104 | `			SyHashDeleteEntry(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),0);` |
|     120 | 1105 | `		}` |
|   14313 | 1106 | `		PH7_VmUnsetMemObj(pVm,pVmAttr->nIdx,TRUE);` |
|    7154 | 1107 | `	}` |
|       - | 1108 | `	/* A dynamic property owns its synthesized ph7_class_attr (struct + inline name in one block) —` |
|       - | 1109 | `	 * free it here (the only place a per-instance pAttr is freed; declared attrs are class-owned). */` |
|   16019 | 1110 | `	if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){` |
|     111 | 1111 | `		SyMemBackendFree(&pVm->sAllocator,pVmAttr->pAttr);` |
|      55 | 1112 | `	}` |
|   16019 | 1113 | `	SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|   16019 | 1114 | `}` |
|       - | 1115 | `/*` |
|       - | 1116 | ` * Release a class instance [i.e: Object in the PHP jargon] and invoke any defined destructor.` |
|       - | 1117 | ` * This routine is invoked as soon as there are no other references to a particular` |
|       - | 1118 | ` * class instance.` |
|       - | 1119 | ` */` |
|    3864 | 1120 | `static void PH7_ClassInstanceRelease(ph7_class_instance *pThis)` |
|       5 | 1121 | `{` |
|       - | 1122 | `	ph7_class_method *pDestr;` |
|       - | 1123 | `	SyHashEntry *pEntry;` |
|       - | 1124 | `	ph7_class *pClass;` |
|       - | 1125 | `	ph7_vm *pVm;` |
|    3869 | 1126 | `	if( pThis->iFlags & CLASS_INSTANCE_DESTROYED ){` |
|       - | 1127 | `		/*` |
|       - | 1128 | `		 * Already destroyed,return immediately.` |
|       - | 1129 | `		 * This could happend if someone perform unset($this) in the destructor body.` |
|       - | 1130 | `		 */` |
|     ! 0 | 1131 | `		return;` |
|       - | 1132 | `	}` |
|       - | 1133 | `	/* Mark as destroyed */` |
|    3869 | 1134 | `	pThis->iFlags \|= CLASS_INSTANCE_DESTROYED;` |
|       - | 1135 | `	/* Invoke any defined destructor if available */` |
|    3869 | 1136 | `	pVm = pThis->pVm;` |
|    3869 | 1137 | `	pClass = pThis->pClass;` |
|    3869 | 1138 | `	pDestr = PH7_ClassExtractMethod(pClass,"__destruct",sizeof("__destruct")-1);` |
|    3869 | 1139 | `	if( pDestr && !pVm->bInReset ){` |
|       - | 1140 | `		/* Invoke the destructor. Skipped during ph7_vm_reset() bulk teardown:` |
|       - | 1141 | `		 * running user PHP against a half-reset VM is unsafe (see bInReset). */` |
|     411 | 1142 | `		pThis->iRef = 2; /* Prevent garbage collection */` |
|     411 | 1143 | `		PH7_VmCallClassMethod(pVm,pThis,pDestr,0,0,0);` |
|     203 | 1144 | `	}` |
|       - | 1145 | `	/* Release non-static attributes (the wholesale SyHashRelease below frees the entry nodes,` |
|       - | 1146 | `	 * so the helper must not delete them mid-walk). */` |
|    3869 | 1147 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|   19853 | 1148 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|   15989 | 1149 | `		PH7_VmReleaseInstanceAttr(pVm,(VmClassAttr *)pEntry->pUserData);` |
|       5 | 1150 | `	}` |
|       - | 1151 | `	/* Release the whole structure */` |
|    3869 | 1152 | `	SyHashRelease(&pThis->hAttr);` |
|    3869 | 1153 | `	SyMemBackendPoolFree(&pVm->sAllocator,pThis);` |
|    1937 | 1154 | `}` |
|       - | 1155 | `/*` |
|       - | 1156 | ` * Decrement the reference count of a class instance [i.e Object in the PHP jargon].` |
|       - | 1157 | ` * If the reference count reaches zero,release the whole instance.` |
|       - | 1158 | ` */` |
|   82028 | 1159 | `PH7_PRIVATE void PH7_ClassInstanceUnref(ph7_class_instance *pThis)` |
|       5 | 1160 | `{` |
|   82033 | 1161 | `	pThis->iRef--;` |
|   82033 | 1162 | `	if( pThis->iRef < 1 ){` |
|       - | 1163 | `		/* No more reference to this instance */` |
|    3869 | 1164 | `		PH7_ClassInstanceRelease(&(*pThis));` |
|    1932 | 1165 | `	}` |
|   82033 | 1166 | `}` |
|       - | 1167 | `/*` |
|       - | 1168 | ` * Compare two class instances [i.e: Objects in the PHP jargon]` |
|       - | 1169 | ` * Note on objects comparison:` |
|       - | 1170 | ` *  According to the PHP langauge reference manual` |
|       - | 1171 | ` *  When using the comparison operator (==), object variables are compared in a simple manner` |
|       - | 1172 | ` *  namely: Two object instances are equal if they have the same attributes and values, and are` |
|       - | 1173 | ` *  instances of the same class.` |
|       - | 1174 | ` *  On the other hand, when using the identity operator (===), object variables are identical` |
|       - | 1175 | ` *  if and only if they refer to the same instance of the same class.` |
|       - | 1176 | ` *  An example will clarify these rules.` |
|       - | 1177 | ` *  Example #1 Example of object comparison` |
|       - | 1178 | ` *  <?php` |
|       - | 1179 | ` *    function bool2str($bool)` |
|       - | 1180 | ` * {` |
|       - | 1181 | ` *   if ($bool === false) {` |
|       - | 1182 | ` *       return 'FALSE';` |
|       - | 1183 | ` *   } else {` |
|       - | 1184 | ` *       return 'TRUE';` |
|       - | 1185 | ` *   }` |
|       - | 1186 | ` * }` |
|       - | 1187 | ` * function compareObjects(&$o1, &$o2)` |
|       - | 1188 | ` * {` |
|       - | 1189 | ` *   echo 'o1 == o2 : ' . bool2str($o1 == $o2) . "\n";` |
|       - | 1190 | ` *   echo 'o1 != o2 : ' . bool2str($o1 != $o2) . "\n";` |
|       - | 1191 | ` *   echo 'o1 === o2 : ' . bool2str($o1 === $o2) . "\n";` |
|       - | 1192 | ` *   echo 'o1 !== o2 : ' . bool2str($o1 !== $o2) . "\n";` |
|       - | 1193 | ` * }` |
|       - | 1194 | ` * class Flag` |
|       - | 1195 | ` * {` |
|       - | 1196 | ` *   public $flag;` |
|       - | 1197 | ` *` |
|       - | 1198 | ` *   function Flag($flag = true) {` |
|       - | 1199 | ` *       $this->flag = $flag;` |
|       - | 1200 | ` *   }` |
|       - | 1201 | ` * }` |
|       - | 1202 | ` *` |
|       - | 1203 | ` * class OtherFlag` |
|       - | 1204 | ` * {` |
|       - | 1205 | ` *   public $flag;` |
|       - | 1206 | ` *` |
|       - | 1207 | ` *   function OtherFlag($flag = true) {` |
|       - | 1208 | ` *       $this->flag = $flag;` |
|       - | 1209 | ` *   }` |
|       - | 1210 | ` * }` |
|       - | 1211 | ` *` |
|       - | 1212 | ` * $o = new Flag();` |
|       - | 1213 | ` * $p = new Flag();` |
|       - | 1214 | ` * $q = $o;` |
|       - | 1215 | ` * $r = new OtherFlag();` |
|       - | 1216 | ` *` |
|       - | 1217 | ` * echo "Two instances of the same class\n";` |
|       - | 1218 | ` * compareObjects($o, $p);` |
|       - | 1219 | ` * echo "\nTwo references to the same instance\n";` |
|       - | 1220 | ` * compareObjects($o, $q);` |
|       - | 1221 | ` * echo "\nInstances of two different classes\n";` |
|       - | 1222 | ` * compareObjects($o, $r);` |
|       - | 1223 | ` * ?>` |
|       - | 1224 | ` * The above example will output:` |
|       - | 1225 | ` * Two instances of the same class` |
|       - | 1226 | ` * o1 == o2 : TRUE` |
|       - | 1227 | ` * o1 != o2 : FALSE` |
|       - | 1228 | ` * o1 === o2 : FALSE` |
|       - | 1229 | ` * o1 !== o2 : TRUE` |
|       - | 1230 | ` * Two references to the same instance` |
|       - | 1231 | ` * o1 == o2 : TRUE` |
|       - | 1232 | ` * o1 != o2 : FALSE` |
|       - | 1233 | ` * o1 === o2 : TRUE` |
|       - | 1234 | ` * o1 !== o2 : FALSE` |
|       - | 1235 | ` * Instances of two different classes` |
|       - | 1236 | ` * o1 == o2 : FALSE` |
|       - | 1237 | ` * o1 != o2 : TRUE` |
|       - | 1238 | ` * o1 === o2 : FALSE` |
|       - | 1239 | ` * o1 !== o2 : TRUE` |
|       - | 1240 | ` *` |
|       - | 1241 | ` * This function return 0 if the objects are equals according to the comprison rules defined above.` |
|       - | 1242 | ` * Any other return values indicates difference.` |
|       - | 1243 | ` */` |
|     230 | 1244 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest)` |
|       5 | 1245 | `{` |
|       - | 1246 | `	SyHashEntry *pEntry,*pEntry2;` |
|       - | 1247 | `	ph7_value sV1,sV2;` |
|       - | 1248 | `	sxi32 rc;` |
|     235 | 1249 | `	if( iNest > 31 ){` |
|       - | 1250 | `		/* Nesting limit reached */` |
|       6 | 1251 | `		PH7_VmThrowError(pLeft->pVm,0,PH7_CTX_ERR,"Nesting limit reached: Infinite recursion?");` |
|       6 | 1252 | `		return 1;` |
|       - | 1253 | `	}` |
|       - | 1254 | `	/* Comparison is performed only if the objects are instance of the same class */` |
|     231 | 1255 | `	if( pLeft->pClass != pRight->pClass ){` |
|       7 | 1256 | `		return 1;` |
|       - | 1257 | `	}` |
|     225 | 1258 | `	if( bStrict ){` |
|       - | 1259 | `		/*` |
|       - | 1260 | `		 * According to the PHP language reference manual:` |
|       - | 1261 | `		 *  when using the identity operator (===), object variables` |
|       - | 1262 | `		 *  are identical if and only if they refer to the same instance` |
|       - | 1263 | `		 *  of the same class.` |
|       - | 1264 | `		 */` |
|      73 | 1265 | `		return !(pLeft == pRight);` |
|       - | 1266 | `	}` |
|       - | 1267 | `	/*` |
|       - | 1268 | `	 * Attribute comparison.` |
|       - | 1269 | `	 * According to the PHP reference manual:` |
|       - | 1270 | `	 *  When using the comparison operator (==), object variables are compared` |
|       - | 1271 | `	 *  in a simple manner, namely: Two object instances are equal if they have` |
|       - | 1272 | `	 *  the same attributes and values, and are instances of the same class.` |
|       - | 1273 | `	 */` |
|     155 | 1274 | `	if( pLeft == pRight ){` |
|       - | 1275 | `		/* Same instance,don't bother processing,object are equals */` |
|       5 | 1276 | `		return 0;` |
|       - | 1277 | `	}` |
|       - | 1278 | `	/* Closures compare by IDENTITY under == as well (not by attributes): two distinct` |
|       - | 1279 | `	 * Closure instances are never equal, even when they wrap the same underlying function` |
|       - | 1280 | `	 * (PHP semantics). pLeft != pRight here, so a Closure pair is unequal. Without this,` |
|       - | 1281 | `` 	 * two capture-less lambdas of the same `function(){}` share the template's `$__fn` `` |
|       - | 1282 | `	 * name and would compare equal. */` |
|     151 | 1283 | `	if( pLeft->pVm->pClosureClass && pLeft->pClass == pLeft->pVm->pClosureClass ){` |
|       5 | 1284 | `		return 1;` |
|       - | 1285 | `	}` |
|       - | 1286 | `	/* Same class but a different number of attributes ⇒ different property sets` |
|       - | 1287 | `	 * (dynamic properties can give two same-class instances different counts). */` |
|     147 | 1288 | `	if( pLeft->hAttr.nEntry != pRight->hAttr.nEntry ){` |
|       3 | 1289 | `		return 1;` |
|       - | 1290 | `	}` |
|     145 | 1291 | `	PH7_MemObjInit(pLeft->pVm,&sV1);` |
|     145 | 1292 | `	PH7_MemObjInit(pLeft->pVm,&sV2);` |
|     145 | 1293 | `	sV1.nIdx = sV2.nIdx = SXU32_HIGH;` |
|       - | 1294 | `	/* Compare each left attribute against the RIGHT attribute of the SAME NAME` |
|       - | 1295 | `	 * (not in lockstep): dynamic properties may be stored in a different order` |
|       - | 1296 | `	 * on the two instances. Counts already match, so if every left attribute has` |
|       - | 1297 | `	 * an equal-valued same-named right attribute the property sets are equal. */` |
|     145 | 1298 | `	SyHashResetLoopCursor(&pLeft->hAttr);` |
|     169 | 1299 | `	while((pEntry = SyHashGetNextEntry(&pLeft->hAttr)) != 0 ){` |
|     157 | 1300 | `		VmClassAttr *p1 = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1301 | `		VmClassAttr *p2;` |
|       - | 1302 | `		ph7_value *pL,*pR;` |
|       - | 1303 | `		/* Compare only non-static attribute */` |
|     157 | 1304 | `		if( p1->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|     ! 0 | 1305 | `			continue;` |
|       - | 1306 | `		}` |
|     157 | 1307 | `		pEntry2 = SyHashGet(&pRight->hAttr,SyStringData(&p1->pAttr->sName),SyStringLength(&p1->pAttr->sName));` |
|     157 | 1308 | `		if( pEntry2 == 0 ){` |
|       - | 1309 | `			/* Left has a property the right lacks ⇒ not equal. */` |
|     ! 0 | 1310 | `			return 1;` |
|       - | 1311 | `		}` |
|     157 | 1312 | `		p2 = (VmClassAttr *)pEntry2->pUserData;` |
|     157 | 1313 | `		pL = ExtractClassAttrValue(pLeft->pVm,p1);` |
|     157 | 1314 | `		pR = ExtractClassAttrValue(pRight->pVm,p2);` |
|     157 | 1315 | `		if( pL && pR ){` |
|     157 | 1316 | `			PH7_MemObjLoad(pL,&sV1);` |
|     157 | 1317 | `			PH7_MemObjLoad(pR,&sV2);` |
|       - | 1318 | `			/* Compare the two values now */` |
|     157 | 1319 | `			rc = PH7_MemObjCmp(&sV1,&sV2,bStrict,iNest+1);` |
|     157 | 1320 | `			PH7_MemObjRelease(&sV1);` |
|     157 | 1321 | `			PH7_MemObjRelease(&sV2);` |
|     157 | 1322 | `			if( rc != 0 ){` |
|       - | 1323 | `				/* Not equals */` |
|     133 | 1324 | `				return rc;` |
|       - | 1325 | `			}` |
|      12 | 1326 | `		}` |
|       1 | 1327 | `	}` |
|       - | 1328 | `	/* Object are equals */` |
|      13 | 1329 | `	return 0;` |
|     120 | 1330 | `}` |
|       - | 1331 | `/*` |
|       - | 1332 | ` * Dump a class instance and the store the dump in the BLOB given` |
|       - | 1333 | ` * as the first argument.` |
|       - | 1334 | ` * Note that only non-static/non-constants attribute are dumped.` |
|       - | 1335 | ` * This function is typically invoked when the user issue a call` |
|       - | 1336 | ` * to [var_dump(),var_export(),print_r(),...].` |
|       - | 1337 | ` * This function SXRET_OK on success. Any other return value including` |
|       - | 1338 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|       - | 1339 | ` */` |
|       - | 1340 | `/*` |
|       - | 1341 | ` * Emit a class-instance dump header plus its trailing newline. For var_dump` |
|       - | 1342 | ` * (ShowType) it completes the "object(" prefix the caller already emitted as` |
|       - | 1343 | ` *   ClassName)#<id> (<count>) {` |
|       - | 1344 | ` * for print_r it emits the legacy PHL  Object(ClassName) {  (count/id unused).` |
|       - | 1345 | ` */` |
|     134 | 1346 | `static void DumpClassInstanceHeader(SyBlob *pOut,ph7_class *pClass,sxu32 nObjId,int ShowType,sxu32 nCount)` |
|       3 | 1347 | `{` |
|     137 | 1348 | `	if( !ShowType ){` |
|       6 | 1349 | `		SyBlobAppend(&(*pOut),"Object(",sizeof("Object(")-1);` |
|       6 | 1350 | `		SyBlobFormat(&(*pOut),"%z) {",&pClass->sName);` |
|       4 | 1351 | `	}else{` |
|     133 | 1352 | `		SyBlobFormat(&(*pOut),"%z)#%u (%u) {",&pClass->sName,nObjId,nCount);` |
|       - | 1353 | `	}` |
|       - | 1354 | `#ifdef __WINNT__` |
|       3 | 1355 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 1356 | `#else` |
|     134 | 1357 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 1358 | `#endif` |
|     137 | 1359 | `}` |
|     138 | 1360 | `PH7_PRIVATE sxi32 PH7_ClassInstanceDump(SyBlob *pOut,ph7_class_instance *pThis,int ShowType,int nTab,int nDepth)` |
|       3 | 1361 | `{` |
|       - | 1362 | `	SyHashEntry *pEntry;` |
|       - | 1363 | `	ph7_value *pValue;` |
|       - | 1364 | `	sxi32 rc;` |
|       - | 1365 | `	int i;` |
|     141 | 1366 | `	if( nDepth > 31 ){` |
|       - | 1367 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|       - | 1368 | `		/* Nesting limit reached..halt immediately*/` |
|       5 | 1369 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|       5 | 1370 | `		if( ShowType ){` |
|       5 | 1371 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|       2 | 1372 | `		}` |
|       5 | 1373 | `		return SXERR_LIMIT;` |
|       - | 1374 | `	}` |
|     137 | 1375 | `	rc = SXRET_OK;` |
|       - | 1376 | `	{` |
|       - | 1377 | `		/* Both var_dump and print_r consult __debugInfo() (PHP behavior);` |
|       - | 1378 | `		 * var_export uses a separate renderer and never reaches here. When the` |
|       - | 1379 | `		 * method is present and returns an array, render that array's entries as` |
|       - | 1380 | `		 * the object body, with the header showing the debug array's count. The` |
|       - | 1381 | `		 * nDepth guard above protects against a __debugInfo returning the object` |
|       - | 1382 | `		 * itself. */` |
|     137 | 1383 | `		ph7_class_method *pDbg = PH7_ClassExtractMethod(pThis->pClass,"__debugInfo",sizeof("__debugInfo")-1);` |
|     137 | 1384 | `		if( pDbg ){` |
|       - | 1385 | `			ph7_value sResult;` |
|       5 | 1386 | `			PH7_MemObjInit(pThis->pVm,&sResult);` |
|       5 | 1387 | `			PH7_VmCallClassMethod(pThis->pVm,pThis,pDbg,&sResult,0,0);` |
|       5 | 1388 | `			if( sResult.iFlags & MEMOBJ_HASHMAP ){` |
|       5 | 1389 | `				ph7_hashmap *pMap = (ph7_hashmap *)sResult.x.pOther;` |
|       - | 1390 | `				/* Header count is the debug array's entry count. */` |
|       5 | 1391 | `				DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,pMap->nEntry);` |
|       5 | 1392 | `				rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth);` |
|       9 | 1393 | `				for( i = 0 ; i < nTab ; i++ ){` |
|       5 | 1394 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       3 | 1395 | `				}` |
|       5 | 1396 | `				SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|       5 | 1397 | `				PH7_MemObjRelease(&sResult);` |
|       5 | 1398 | `				return rc;` |
|       - | 1399 | `			}` |
|       - | 1400 | `			/* Non-array return: behave as if __debugInfo were absent. */` |
|     ! 0 | 1401 | `			PH7_MemObjRelease(&sResult);` |
|     ! 0 | 1402 | `		}` |
|       - | 1403 | `	}` |
|       - | 1404 | `	{` |
|       - | 1405 | `		/* var_dump's header needs the property count up front, so pre-count the` |
|       - | 1406 | `		 * non-static/non-constant attributes (matching the dump loop below). */` |
|     132 | 1407 | `		sxu32 nProp = 0;` |
|     132 | 1408 | `		if( ShowType ){` |
|     130 | 1409 | `			SyHashResetLoopCursor(&pThis->hAttr);` |
|     266 | 1410 | `			while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|     138 | 1411 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     138 | 1412 | `				if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|     134 | 1413 | `					nProp++;` |
|      66 | 1414 | `				}` |
|       2 | 1415 | `			}` |
|      64 | 1416 | `		}` |
|     132 | 1417 | `		DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,nProp);` |
|       - | 1418 | `	}` |
|       - | 1419 | `	/* Dump object attributes */` |
|     132 | 1420 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|     213 | 1421 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|     142 | 1422 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     142 | 1423 | `		if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|       - | 1424 | `			/* Dump non-static/constant attribute only */` |
|    3994 | 1425 | `			for( i = 0 ; i < nTab ; i++ ){` |
|    3858 | 1426 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|    1930 | 1427 | `			}` |
|     138 | 1428 | `			pValue = ExtractClassAttrValue(pThis->pVm,pVmAttr);` |
|     138 | 1429 | `			if( pValue ){` |
|     138 | 1430 | `				SyBlobFormat(&(*pOut),"['%z'] =>",&pVmAttr->pAttr->sName);` |
|       - | 1431 | `#ifdef __WINNT__` |
|       2 | 1432 | `				SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 1433 | `#else` |
|     136 | 1434 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 1435 | `#endif` |
|     138 | 1436 | `				rc = PH7_MemObjDump(&(*pOut),pValue,ShowType,nTab+1,nDepth,0);` |
|     138 | 1437 | `				if( rc == SXERR_LIMIT ){` |
|     125 | 1438 | `					break;` |
|       - | 1439 | `				}` |
|       6 | 1440 | `			}` |
|       6 | 1441 | `		}` |
|       2 | 1442 | `	}` |
|    3982 | 1443 | `	for( i = 0 ; i < nTab ; i++ ){` |
|    3852 | 1444 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|    1927 | 1445 | `	}` |
|     132 | 1446 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|     132 | 1447 | `	return rc;` |
|      72 | 1448 | `}` |
|       - | 1449 | `/*` |
|       - | 1450 | ` * Call a magic method [i.e: __toString(),__toBool(),__Invoke()...]` |
|       - | 1451 | ` * Return SXRET_OK on successfull call. Any other return value indicates failure.` |
|       - | 1452 | ` * Notes on magic methods.` |
|       - | 1453 | ` * According to the PHP language reference manual.` |
|       - | 1454 | ` *  The function names __construct(), __destruct(), __call(), __callStatic()` |
|       - | 1455 | ` *  __get(),  __toString(), __invoke(), __clone() are magical in PHP classes.` |
|       - | 1456 | ` * You cannot have functions with these names in any of your classes unless` |
|       - | 1457 | ` * you want the magic functionality associated with them.` |
|       - | 1458 | ` * Example of magical methods:` |
|       - | 1459 | ` * __toString()` |
|       - | 1460 | ` *  The __toString() method allows a class to decide how it will react when it is treated like` |
|       - | 1461 | ` *  a string. For example, what echo $obj; will print. This method must return a string.` |
|       - | 1462 | ` *  Example #2 Simple example` |
|       - | 1463 | ` * <?php` |
|       - | 1464 | ` * // Declare a simple class` |
|       - | 1465 | ` * class TestClass` |
|       - | 1466 | ` * {` |
|       - | 1467 | ` *   public $foo;` |
|       - | 1468 | ` *` |
|       - | 1469 | ` *   public function __construct($foo)` |
|       - | 1470 | ` *   {` |
|       - | 1471 | ` *       $this->foo = $foo;` |
|       - | 1472 | ` *   }` |
|       - | 1473 | ` *` |
|       - | 1474 | ` *   public function __toString()` |
|       - | 1475 | ` *   {` |
|       - | 1476 | ` *       return $this->foo;` |
|       - | 1477 | ` *   }` |
|       - | 1478 | ` * }` |
|       - | 1479 | ` * $class = new TestClass('Hello');` |
|       - | 1480 | ` * echo $class;` |
|       - | 1481 | ` * ?>` |
|       - | 1482 | ` * The above example will output:` |
|       - | 1483 | ` *  Hello` |
|       - | 1484 | ` *` |
|       - | 1485 | ` * Note that PH7 does not support all the magical method and introudces __toFloat(),__toInt()` |
|       - | 1486 | ` * which have the same behaviour as __toString() but for float and integer types` |
|       - | 1487 | ` * respectively.` |
|       - | 1488 | ` * Refer to the official documentation for more information.` |
|       - | 1489 | ` */` |
|      32 | 1490 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallMagicMethod(` |
|       - | 1491 | `	ph7_vm *pVm,               /* VM that own all this stuff */` |
|       - | 1492 | `	ph7_class *pClass,         /* Target class */` |
|       - | 1493 | `	ph7_class_instance *pThis, /* Target object */` |
|       - | 1494 | `	const char *zMethod,       /* Magic method name [i.e: __toString()]*/` |
|       - | 1495 | `	sxu32 nByte,               /* zMethod length*/` |
|       - | 1496 | `	const SyString *pAttrName, /* Attribute name */` |
|       - | 1497 | `	ph7_value *pResult         /* OUT: magic method return value. NULL to discard */` |
|       - | 1498 | `	)` |
|       1 | 1499 | `{` |
|      33 | 1500 | `	ph7_value *apArg[2] = { 0 , 0 };` |
|       - | 1501 | `	ph7_class_method *pMeth;` |
|       - | 1502 | `	ph7_value sAttr; /* cc warning */` |
|       - | 1503 | `	sxi32 rc;` |
|       - | 1504 | `	int nArg;` |
|       - | 1505 | `	/* Make sure the magic method is available */` |
|      33 | 1506 | `	pMeth = PH7_ClassExtractMethod(&(*pClass),zMethod,nByte);` |
|      33 | 1507 | `	if( pMeth == 0 ){` |
|       - | 1508 | `		/* No such method,return immediately */` |
|     ! 0 | 1509 | `		return SXERR_NOTFOUND;` |
|       - | 1510 | `	}` |
|      33 | 1511 | `	nArg = 0;` |
|       - | 1512 | `	/* Copy arguments */` |
|      33 | 1513 | `	if( pAttrName ){` |
|      33 | 1514 | `		PH7_MemObjInitFromString(pVm,&sAttr,pAttrName);` |
|      33 | 1515 | `		sAttr.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      33 | 1516 | `		apArg[0] = &sAttr;` |
|      33 | 1517 | `		nArg = 1;` |
|      16 | 1518 | `	}` |
|       - | 1519 | `	/* Call the magic method now */` |
|      33 | 1520 | `	rc = PH7_VmCallClassMethod(pVm,&(*pThis),pMeth,pResult,nArg,apArg);` |
|       - | 1521 | `	/* Clean up */` |
|      33 | 1522 | `	if( pAttrName ){` |
|      33 | 1523 | `		PH7_MemObjRelease(&sAttr);` |
|      16 | 1524 | `	}` |
|      33 | 1525 | `	return rc;` |
|      17 | 1526 | `}` |
|       - | 1527 | `/*` |
|       - | 1528 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon].` |
|       - | 1529 | ` * This function is simply a wrapper on ExtractClassAttrValue().` |
|       - | 1530 | ` */` |
|     126 | 1531 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceExtractAttrValue(ph7_class_instance *pThis,VmClassAttr *pAttr)` |
|       3 | 1532 | `{` |
|       - | 1533 | `   /* Extract the attribute value */` |
|       - | 1534 | `	ph7_value *pValue;` |
|     129 | 1535 | `	pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     129 | 1536 | `	return pValue;` |
|       3 | 1537 | `}` |
|       - | 1538 | `/*` |
|       - | 1539 | ` * Convert a class instance [i.e: Object in the PHP jargon] into a hashmap [i.e: array in the PHP jargon].` |
|       - | 1540 | ` * Return SXRET_OK on success. Any other value indicates failure.` |
|       - | 1541 | ` * Note on object conversion to array:` |
|       - | 1542 | ` *  Acccording to the PHP language reference manual` |
|       - | 1543 | ` *  If an object is converted to an array, the result is an array whose elements are the object's properties.` |
|       - | 1544 | ` *  The keys are the member variable names.` |
|       - | 1545 | ` *` |
|       - | 1546 | ` *  The following example:` |
|       - | 1547 | ` *  class Test {` |
|       - | 1548 | ` *   public $A = 25<<1;  // 50` |
|       - | 1549 | ` *	 public $c = rand_str(3);   // Random string of length 3` |
|       - | 1550 | ` *	 public $d = rand() & 1023; // Random number between 0..1023` |
|       - | 1551 | ` *  }` |
|       - | 1552 | ` *  var_dump((array) new Test());` |
|       - | 1553 | ` *	Will output:` |
|       - | 1554 | ` *  array(3) {` |
|       - | 1555 | ` *   [A] =>` |
|       - | 1556 | ` *      int(50)` |
|       - | 1557 | ` *   [c] =>` |
|       - | 1558 | ` *     string(3 'aps')` |
|       - | 1559 | ` *   [d] =>` |
|       - | 1560 | ` *     int(991)` |
|       - | 1561 | ` *  }` |
|       - | 1562 | ` * You have noticed that PH7 allow class attributes [i.e: $a,$c,$d in the example above]` |
|       - | 1563 | ` * have any complex expression (even function calls/Annonymous functions) as their default` |
|       - | 1564 | ` * value unlike the standard PHP engine.` |
|       - | 1565 | ` * This is a very powerful feature that you have to look at.` |
|       - | 1566 | ` */` |
|      12 | 1567 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|       1 | 1568 | `{` |
|       - | 1569 | `	SyHashEntry *pEntry;` |
|       - | 1570 | `	SyString *pAttrName;` |
|       - | 1571 | `	VmClassAttr *pAttr;` |
|       - | 1572 | `	ph7_value *pValue;` |
|       - | 1573 | `	ph7_value sName;` |
|       - | 1574 | `	/* Reset the loop cursor */` |
|      13 | 1575 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      13 | 1576 | `	PH7_MemObjInitFromString(pThis->pVm,&sName,0);` |
|      47 | 1577 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       - | 1578 | `		/* Point to the current attribute */` |
|      29 | 1579 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1580 | `		/* Extract attribute value */` |
|      29 | 1581 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|      29 | 1582 | `		if( pValue ){` |
|       - | 1583 | `			/* Build attribute name */` |
|      29 | 1584 | `			pAttrName = &pAttr->pAttr->sName;` |
|      29 | 1585 | `			PH7_MemObjStringAppend(&sName,pAttrName->zString,pAttrName->nByte);` |
|       - | 1586 | `			/* Perform the insertion */` |
|      29 | 1587 | `			PH7_HashmapInsert(pMap,&sName,pValue);` |
|       - | 1588 | `			/* Reset the string cursor */` |
|      29 | 1589 | `			SyBlobReset(&sName.sBlob);` |
|      14 | 1590 | `		}` |
|       1 | 1591 | `	}` |
|      13 | 1592 | `	PH7_MemObjRelease(&sName);` |
|      13 | 1593 | `	return SXRET_OK;` |
|       1 | 1594 | `}` |
|       - | 1595 | `/*` |
|       - | 1596 | ` * Iterate throw class attributes and invoke the given callback [i.e: xWalk()] for each` |
|       - | 1597 | ` * retrieved attribute.` |
|       - | 1598 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|       - | 1599 | ` * the attribute value in the callback body will not alter the real attribute value.` |
|       - | 1600 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|       - | 1601 | ` * a value different from PH7_OK.` |
|       - | 1602 | ` * Refer to [ph7_object_walk()] for more information.` |
|       - | 1603 | ` */` |
|      40 | 1604 | `PH7_PRIVATE sxi32 PH7_ClassInstanceWalk(` |
|       - | 1605 | `	ph7_class_instance *pThis, /* Target object */` |
|       - | 1606 | `	int (*xWalk)(const char *,ph7_value *,void *), /* Walker callback */` |
|       - | 1607 | `	void *pUserData /* Last argument to xWalk() */` |
|       - | 1608 | `	)` |
|       2 | 1609 | `{` |
|       - | 1610 | `	SyHashEntry *pEntry; /* Hash entry */` |
|       - | 1611 | `	VmClassAttr *pAttr;  /* Pointer to the attribute */` |
|       - | 1612 | `	ph7_value *pValue;   /* Attribute value */` |
|       - | 1613 | `	ph7_value sValue;    /* Copy of the attribute value */` |
|       - | 1614 | `	int rc;` |
|       - | 1615 | `	/* Reset the loop cursor */` |
|      42 | 1616 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      42 | 1617 | `	PH7_MemObjInit(pThis->pVm,&sValue);` |
|       - | 1618 | `	/* Start the walk process */` |
|     124 | 1619 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       - | 1620 | `		/* Point to the current attribute */` |
|      64 | 1621 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1622 | `		/* Extract attribute value */` |
|      64 | 1623 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|      64 | 1624 | `		if( pValue ){` |
|      64 | 1625 | `			PH7_MemObjLoad(pValue,&sValue);` |
|       - | 1626 | `			/* Invoke the supplied callback */` |
|      64 | 1627 | `			rc =  xWalk(SyStringData(&pAttr->pAttr->sName),&sValue,pUserData);` |
|      64 | 1628 | `			PH7_MemObjRelease(&sValue);` |
|      64 | 1629 | `			if( rc != PH7_OK){` |
|       - | 1630 | `				/* User callback request an operation abort */` |
|     ! 0 | 1631 | `				return SXERR_ABORT;` |
|       - | 1632 | `			}` |
|      31 | 1633 | `		}` |
|       2 | 1634 | `	}` |
|       - | 1635 | `	/* All done */` |
|      42 | 1636 | `	return SXRET_OK;` |
|      22 | 1637 | `}` |
|       - | 1638 | `/*` |
|       - | 1639 | ` * Extract a class atrribute value.` |
|       - | 1640 | ` * Return a pointer to the attribute value on success. Otherwise NULL.` |
|       - | 1641 | ` * Note:` |
|       - | 1642 | ` *  Access to static and constant attribute is not allowed. That is,the function` |
|       - | 1643 | ` *  will return NULL in case someone (host-application code) try to extract` |
|       - | 1644 | ` *  a static/constant attribute.` |
|       - | 1645 | ` */` |
|    6822 | 1646 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceFetchAttr(ph7_class_instance *pThis,const SyString *pName)` |
|       5 | 1647 | `{` |
|       - | 1648 | `	SyHashEntry *pEntry;` |
|       - | 1649 | `	VmClassAttr *pAttr;` |
|       - | 1650 | `	/* Query the attribute hashtable */` |
|    6827 | 1651 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);` |
|    6827 | 1652 | `	if( pEntry == 0 ){` |
|       - | 1653 | `		/* No such attribute */` |
|     ! 0 | 1654 | `		return 0;` |
|       - | 1655 | `	}` |
|       - | 1656 | `	/* Point to the class atrribute */` |
|    6827 | 1657 | `	pAttr = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1658 | `	/* Check if we are dealing with a static/constant attribute */` |
|    6827 | 1659 | `	if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|       - | 1660 | `		/* Access is forbidden */` |
|     ! 0 | 1661 | `		return 0;` |
|       - | 1662 | `	}` |
|       - | 1663 | `	/* Return the attribute value */` |
|    6827 | 1664 | `	return ExtractClassAttrValue(pThis->pVm,pAttr);` |
|    3416 | 1665 | `}` |
|       - | 1666 |  |
