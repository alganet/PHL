# src/ph7/oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 650/706 lines (92.07%)

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
|  260836 |   14 | `PH7_PRIVATE ph7_class * PH7_NewRawClass(ph7_vm *pVm,const SyString *pName,sxu32 nLine)` |
|       5 |   15 | `{` |
|       - |   16 | `	ph7_class *pClass;` |
|       - |   17 | `	char *zName;` |
|       - |   18 | `	/* Allocate a new instance */` |
|  260841 |   19 | `	pClass = (ph7_class *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class));` |
|  260841 |   20 | `	if( pClass == 0 ){` |
|     ! 0 |   21 | `		return 0;` |
|       - |   22 | `	}` |
|       - |   23 | `	/* Zero the structure */` |
|  260841 |   24 | `	SyZero(pClass,sizeof(ph7_class));` |
|       - |   25 | `	/* Duplicate class name */` |
|  260841 |   26 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  260841 |   27 | `	if( zName == 0 ){` |
|     ! 0 |   28 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClass);` |
|     ! 0 |   29 | `		return 0;` |
|       - |   30 | `	}` |
|       - |   31 | `	/* Initialize fields */` |
|  260841 |   32 | `	SyStringInitFromBuf(&pClass->sName,zName,pName->nByte);` |
|  260841 |   33 | `	SyHashInit(&pClass->hMethod,&pVm->sAllocator,0,0);` |
|  260841 |   34 | `	SyHashInit(&pClass->hAttr,&pVm->sAllocator,0,0);` |
|  260841 |   35 | `	SyHashInit(&pClass->hDerived,&pVm->sAllocator,0,0);` |
|  260841 |   36 | `	SySetInit(&pClass->aInterface,&pVm->sAllocator,sizeof(ph7_class *));` |
|  260841 |   37 | `	SySetInit(&pClass->aTrait,&pVm->sAllocator,sizeof(ph7_class *));` |
|  260841 |   38 | `	SySetInit(&pClass->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|  260841 |   39 | `	pClass->nLine = nLine;` |
|  260841 |   40 | `	if( pVm->bCompilingBuiltin ){` |
|       - |   41 | `		/* Defined by an embedded builtin chunk: internal, no defining file.` |
|       - |   42 | `		 * Class compilers merge further flags with \|= so this survives. */` |
|  259161 |   43 | `		pClass->iFlags \|= PH7_CLASS_INTERNAL;` |
|  129583 |   44 | `	}else{` |
|       - |   45 | `		/* Alias the VM-lifetime path dup on top of the include stack */` |
|    1685 |   46 | `		SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|    1685 |   47 | `		if( pFile ){` |
|    1685 |   48 | `			SyStringDupPtr(&pClass->sFile,pFile);` |
|     840 |   49 | `		}` |
|       - |   50 | `	}` |
|       - |   51 | `	/* All done */` |
|  260841 |   52 | `	return pClass;` |
|  130423 |   53 | `}` |
|       - |   54 | `/*` |
|       - |   55 | ` * Allocate and initialize a new class attribute.` |
|       - |   56 | ` * Return a pointer to the class attribute on success. NULL otherwise.` |
|       - |   57 | ` */` |
|  352960 |   58 | `PH7_PRIVATE ph7_class_attr * PH7_NewClassAttr(ph7_vm *pVm,const SyString *pName,sxu32 nLine,sxi32 iProtection,sxi32 iFlags)` |
|       5 |   59 | `{` |
|       - |   60 | `	ph7_class_attr *pAttr;` |
|       - |   61 | `	char *zName;` |
|  352965 |   62 | `	pAttr = (ph7_class_attr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_attr));` |
|  352965 |   63 | `	if( pAttr == 0 ){` |
|     ! 0 |   64 | `		return 0;` |
|       - |   65 | `	}` |
|       - |   66 | `	/* Zero the structure */` |
|  352965 |   67 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|  352965 |   68 | `	SySetInit(&pAttr->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|       - |   69 | `	/* Duplicate attribute name */` |
|  352965 |   70 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  352965 |   71 | `	if( zName == 0 ){` |
|     ! 0 |   72 | `		SyMemBackendPoolFree(&pVm->sAllocator,pAttr);` |
|     ! 0 |   73 | `		return 0;` |
|       - |   74 | `	}` |
|       - |   75 | `	/* Initialize fields */` |
|  352965 |   76 | `	SySetInit(&pAttr->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|  352965 |   77 | `	SySetInit(&pAttr->aUnionAlts,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|  352965 |   78 | `	SyStringInitFromBuf(&pAttr->sName,zName,pName->nByte);` |
|  352965 |   79 | `	pAttr->iProtection = iProtection;` |
|  352965 |   80 | `	pAttr->nIdx = SXU32_HIGH;` |
|  352965 |   81 | `	pAttr->iFlags = iFlags;` |
|  352965 |   82 | `	pAttr->nLine = nLine;` |
|  352965 |   83 | `	return pAttr;` |
|  176485 |   84 | `}` |
|       - |   85 | `/*` |
|       - |   86 | ` * Allocate and initialize a new class method.` |
|       - |   87 | ` * Return a pointer to the class method on success. NULL otherwise` |
|       - |   88 | ` * This function associate with the newly created method an automatically generated` |
|       - |   89 | ` * random unique name.` |
|       - |   90 | ` */` |
| 1382272 |   91 | `PH7_PRIVATE ph7_class_method * PH7_NewClassMethod(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,sxu32 nLine,` |
|       - |   92 | `	sxi32 iProtection,sxi32 iFlags,sxi32 iFuncFlags)` |
|       5 |   93 | `{` |
|       - |   94 | `	ph7_class_method *pMeth;` |
|       - |   95 | `	SyHashEntry *pEntry;` |
|       - |   96 | `	SyString *pNamePtr;` |
|       - |   97 | `	char zSalt[10];` |
|       - |   98 | `	char *zName;` |
|       - |   99 | `	sxu32 nByte;` |
|       - |  100 | `	/* Allocate a new class method instance */` |
| 1382277 |  101 | `	pMeth = (ph7_class_method *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_method));` |
| 1382277 |  102 | `	if( pMeth == 0 ){` |
|     ! 0 |  103 | `		return 0;` |
|       - |  104 | `	}` |
|       - |  105 | `	/* Zero the structure */` |
| 1382277 |  106 | `	SyZero(pMeth,sizeof(ph7_class_method));` |
|       - |  107 | `	/* Check for an already installed method with the same name */` |
| 1382277 |  108 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
| 1382277 |  109 | `	if( pEntry == 0 ){` |
|       - |  110 | `		/* Associate an unique VM name to this method */` |
| 1382275 |  111 | `		nByte = sizeof(zSalt) + pName->nByte + SyStringLength(&pClass->sName)+sizeof(char)*7/*[[__'\0'*/;` |
| 1382275 |  112 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,nByte);` |
| 1382275 |  113 | `		if( zName == 0 ){` |
|     ! 0 |  114 | `			SyMemBackendPoolFree(&pVm->sAllocator,pMeth);` |
|     ! 0 |  115 | `			return 0;` |
|       - |  116 | `		}` |
| 1382275 |  117 | `		pNamePtr = &pMeth->sVmName;` |
|       - |  118 | `		/* Generate a random string */` |
| 1382275 |  119 | `		PH7_VmRandomString(&(*pVm),zSalt,sizeof(zSalt));` |
| 1382275 |  120 | `		pNamePtr->nByte = SyBufferFormat(zName,nByte,"[__%z@%z_%.*s]",&pClass->sName,pName,sizeof(zSalt),zSalt);` |
| 1382275 |  121 | `		pNamePtr->zString = zName;` |
|  691140 |  122 | `	}else{` |
|       - |  123 | `		/* Method is condidate for 'overloading' */` |
|       3 |  124 | `		ph7_class_method *pCurrent = (ph7_class_method *)pEntry->pUserData;` |
|       3 |  125 | `		pNamePtr = &pMeth->sVmName;` |
|       - |  126 | `		/* Use the same VM name */` |
|       3 |  127 | `		SyStringDupPtr(pNamePtr,&pCurrent->sVmName);` |
|       3 |  128 | `		zName = (char *)pNamePtr->zString;` |
|       - |  129 | `	}` |
| 1382277 |  130 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|   58060 |  131 | `		if( (pName->nByte == sizeof("__destruct") - 1 && SyMemcmp(pName->zString,"__destruct",sizeof("__destruct") - 1 ) == 0)` |
|   58065 |  132 | `			\|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|       - |  133 | `				/* Switch to public visibility for destructors and legacy class-name` |
|       - |  134 | `				 * constructors (the engine invokes destructors internally, bypassing` |
|       - |  135 | `				 * visibility either way). __construct KEEPS its declared visibility` |
|       - |  136 | ``				 * (band A #4): php enforces it at `new` — a private/protected ctor`` |
|       - |  137 | `				 * from the wrong scope is a catchable Error, checked at OP_NEW —` |
|       - |  138 | `				 * and ReflectionClass::isInstantiable()/newInstance() now see it. */` |
|     ! 0 |  139 | `				iProtection = PH7_CLASS_PROT_PUBLIC;` |
|     ! 0 |  140 | `		}` |
|   29030 |  141 | `	}` |
|       - |  142 | `	/* Initialize method fields */` |
| 1382277 |  143 | `	pMeth->iProtection = iProtection;` |
| 1382277 |  144 | `	pMeth->iFlags = iFlags;` |
| 1382277 |  145 | `	pMeth->nLine = nLine;` |
| 2073413 |  146 | `	PH7_VmInitFuncState(&(*pVm),&pMeth->sFunc,&zName[sizeof(char)*4/*[__@*/+SyStringLength(&pClass->sName)],` |
| 1382272 |  147 | `		pName->nByte,iFuncFlags\|VM_FUNC_CLASS_METHOD,pClass);` |
| 1382277 |  148 | `	return pMeth;` |
|  691141 |  149 | `}` |
|       - |  150 | `/*` |
|       - |  151 | ` * Check if the given name have a class method associated with it.` |
|       - |  152 | ` * Return the desired method [i.e: ph7_class_method instance] on success. NULL otherwise.` |
|       - |  153 | ` */` |
|  315546 |  154 | `PH7_PRIVATE ph7_class_method * PH7_ClassExtractMethod(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|       5 |  155 | `{` |
|       - |  156 | `	SyHashEntry *pEntry;` |
|       - |  157 | `	/* Perform a hash lookup */` |
|  315551 |  158 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)zName,nByte);` |
|  315551 |  159 | `	if( pEntry == 0 ){` |
|       - |  160 | `		/* No such entry */` |
|    6803 |  161 | `		return 0;` |
|       - |  162 | `	}` |
|       - |  163 | `	/* Point to the desired method */` |
|  308753 |  164 | `	return (ph7_class_method *)pEntry->pUserData;` |
|  157778 |  165 | `}` |
|       - |  166 | `/*` |
|       - |  167 | ` * Check if the given name is a class attribute.` |
|       - |  168 | ` * Return the desired attribute [i.e: ph7_class_attr instance] on success.NULL otherwise.` |
|       - |  169 | ` */` |
|  210024 |  170 | `PH7_PRIVATE ph7_class_attr * PH7_ClassExtractAttribute(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|       5 |  171 | `{` |
|       - |  172 | `	SyHashEntry *pEntry;` |
|       - |  173 | `	/* Perform a hash lookup */` |
|  210029 |  174 | `	pEntry = SyHashGet(&pClass->hAttr,(const void *)zName,nByte);` |
|  210029 |  175 | `	if( pEntry == 0 ){` |
|       - |  176 | `		/* No such entry */` |
|  209803 |  177 | `		return 0;` |
|       - |  178 | `	}` |
|       - |  179 | `	/* Point to the desierd method */` |
|     231 |  180 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|  105017 |  181 | `}` |
|       - |  182 | `/*` |
|       - |  183 | ` * Install a class attribute in the corresponding container.` |
|       - |  184 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|       - |  185 | ` */` |
|  352960 |  186 | `PH7_PRIVATE sxi32 PH7_ClassInstallAttr(ph7_class *pClass,ph7_class_attr *pAttr)` |
|       5 |  187 | `{` |
|  352965 |  188 | `	SyString *pName = &pAttr->sName;` |
|       - |  189 | `	sxi32 rc;` |
|       - |  190 | `	/* Remember where this attribute was originally declared so that later` |
|       - |  191 | `	 * inheritance/trait copies still know the declaring class (needed for` |
|       - |  192 | `	 * PHP-compatible error messages on typed properties). */` |
|  352965 |  193 | `	if( pAttr->pDeclClass == 0 ){` |
|  352965 |  194 | `		pAttr->pDeclClass = pClass;` |
|  176480 |  195 | `	}` |
|  352965 |  196 | `	rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|  352965 |  197 | `	return rc;` |
|       5 |  198 | `}` |
|       - |  199 | `/*` |
|       - |  200 | ` * Install a class method in the corresponding container.` |
|       - |  201 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|       - |  202 | ` */` |
| 1382260 |  203 | `PH7_PRIVATE sxi32 PH7_ClassInstallMethod(ph7_class *pClass,ph7_class_method *pMeth)` |
|       5 |  204 | `{` |
| 1382265 |  205 | `	SyString *pName = &pMeth->sFunc.sName;` |
|       - |  206 | `	sxi32 rc;` |
| 1382265 |  207 | `	rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 1382265 |  208 | `	return rc;` |
|       5 |  209 | `}` |
|       - |  210 | `/*` |
|       - |  211 | ` * Method-override compatibility (variance) checking.` |
|       - |  212 | ` *` |
|       - |  213 | ` * PHP rejects an override whose signature is incompatible with the parent's:` |
|       - |  214 | ` * return types are covariant (child may only narrow), parameter types are` |
|       - |  215 | ` * contravariant (child may only widen), and a child may not add a required` |
|       - |  216 | ` * parameter. We add the diagnostic — but conservatively: PHL must keep running` |
|       - |  217 | ` * valid PHP, so the comparator below is SKIP-BY-DEFAULT. It flags only cases that` |
|       - |  218 | ` * are unambiguously invalid and silently accepts anything subtle (unions,` |
|       - |  219 | ` * intersections, pseudo-types, self/parent/static, object, unresolved classes,` |
|       - |  220 | ` * or a missing type), so it can never reject valid code.` |
|       - |  221 | ` */` |
|       - |  222 | `#define OVT_NONE   0  /* no declared type */` |
|       - |  223 | `#define OVT_SCALAR 1  /* a concrete invariant scalar: int/float/string/bool/array */` |
|       - |  224 | `#define OVT_CLASS  2  /* a real, already-loaded class/interface */` |
|       - |  225 | `#define OVT_SKIP   3  /* union/intersection/pseudo/self/object/unresolved — never flag */` |
|       - |  226 |  |
|       - |  227 | `/*` |
|       - |  228 | ` * Classify one declared type (nType + class name + union flag) for override` |
|       - |  229 | ` * comparison. On OVT_CLASS, *ppClass receives the resolved class. Class names are` |
|       - |  230 | ` * resolved by a direct, autoload-free hClass lookup: a miss (forward reference,` |
|       - |  231 | ` * namespaced, or not-yet-loaded) yields OVT_SKIP, which the caller accepts.` |
|       - |  232 | ` */` |
|   31084 |  233 | `static int OoClassifyOverrideType(ph7_vm *pVm, sxu32 nType, const SyString *pClass,` |
|       - |  234 | `	int bUnion, ph7_class **ppClass)` |
|       5 |  235 | `{` |
|   31089 |  236 | `	*ppClass = 0;` |
|   31089 |  237 | `	if( bUnion ){` |
|       3 |  238 | `		return OVT_SKIP; /* union/intersection — full lattice, skip */` |
|       - |  239 | `	}` |
|   31087 |  240 | `	if( nType == 0 ){` |
|   31013 |  241 | `		return OVT_NONE; /* no declared type */` |
|       - |  242 | `	}` |
|      78 |  243 | `	if( nType == SXU32_HIGH ){` |
|       - |  244 | `		/* A class name OR a pseudo-type stored as a name atom. Skip every pseudo` |
|       - |  245 | `		 * (incl. self/parent/static, which are context-relative). */` |
|       - |  246 | `		static const struct { const char *z; sxu32 n; } aPseudo[] = {` |
|       - |  247 | `			{"mixed",5}, {"never",5}, {"iterable",8}, {"callable",8}, {"true",4},` |
|       - |  248 | `			{"false",5}, {"self",4}, {"parent",6}, {"static",6}` |
|       - |  249 | `		};` |
|      18 |  250 | `		const char *z = pClass->zString;` |
|      18 |  251 | `		sxu32 n = pClass->nByte;` |
|       - |  252 | `		SyHashEntry *pE;` |
|       - |  253 | `		sxu32 i;` |
|     154 |  254 | `		for( i = 0; i < SX_ARRAYSIZE(aPseudo); i++ ){` |
|     142 |  255 | `			if( n == aPseudo[i].n && SyStrnmicmp(z,aPseudo[i].z,n) == 0 ){` |
|       5 |  256 | `				return OVT_SKIP;` |
|       - |  257 | `			}` |
|      70 |  258 | `		}` |
|      14 |  259 | `		pE = SyHashGet(&pVm->hClass,(const void *)z,n);` |
|      14 |  260 | `		if( pE == 0 ){` |
|     ! 0 |  261 | `			return OVT_SKIP; /* not loaded / forward ref / namespaced — accept */` |
|       - |  262 | `		}` |
|      14 |  263 | `		*ppClass = (ph7_class *)pE->pUserData;` |
|      14 |  264 | `		return OVT_CLASS;` |
|       - |  265 | `	}` |
|      58 |  266 | `	if( nType == MEMOBJ_STRING \|\| nType == MEMOBJ_INT \|\| nType == MEMOBJ_REAL` |
|      28 |  267 | `	 \|\| nType == MEMOBJ_BOOL \|\| nType == MEMOBJ_HASHMAP ){` |
|      37 |  268 | `		return OVT_SCALAR;` |
|       - |  269 | `	}` |
|       - |  270 | `	/* MEMOBJ_OBJ (object — subtypes against classes), MEMOBJ_VOID/NULL/RES,` |
|       - |  271 | `	 * or anything unexpected: skip. */` |
|      26 |  272 | `	return OVT_SKIP;` |
|   15547 |  273 | `}` |
|       - |  274 |  |
|       - |  275 | `/*` |
|       - |  276 | ` * A declared type normalized for override comparison: the raw type code, the` |
|       - |  277 | ` * class-name string (when a class), and the union/nullable flags. Extracted once` |
|       - |  278 | ` * from each side so the comparator takes two of these instead of eight scalars.` |
|       - |  279 | ` */` |
|       - |  280 | `typedef struct OvType OvType;` |
|       - |  281 | `struct OvType {` |
|       - |  282 | `	sxu32 nType;` |
|       - |  283 | `	const SyString *pClass;` |
|       - |  284 | `	int bUnion;` |
|       - |  285 | `	int bNullable;` |
|       - |  286 | `};` |
|   31048 |  287 | `static OvType OoTypeFromReturn(ph7_vm_func *pF)` |
|       5 |  288 | `{` |
|       - |  289 | `	OvType t;` |
|   31053 |  290 | `	t.nType = pF->nReturnType;` |
|   31053 |  291 | `	t.pClass = &pF->sReturnClass;` |
|   31053 |  292 | `	t.bUnion = SySetUsed(&pF->aReturnUnion) > 0;` |
|   31053 |  293 | `	t.bNullable = (pF->iFlags & VM_FUNC_RETURN_NULLABLE) != 0;` |
|   31053 |  294 | `	return t;` |
|       5 |  295 | `}` |
|      36 |  296 | `static OvType OoTypeFromArg(ph7_vm_func_arg *pA)` |
|       3 |  297 | `{` |
|       - |  298 | `	OvType t;` |
|      39 |  299 | `	t.nType = pA->nType;` |
|      39 |  300 | `	t.pClass = &pA->sClass;` |
|      39 |  301 | `	t.bUnion = (pA->iFlags & VM_FUNC_ARG_UNION) != 0;` |
|      39 |  302 | `	t.bNullable = (pA->iFlags & VM_FUNC_ARG_NULLABLE) != 0;` |
|      39 |  303 | `	return t;` |
|       3 |  304 | `}` |
|       - |  305 | `/*` |
|       - |  306 | ` * Return TRUE if the child type is an unambiguously-invalid override of the` |
|       - |  307 | ` * parent type. bCovariant=1 for a return type (child must be ⊆ parent),` |
|       - |  308 | ` * 0 for a parameter (child must be ⊇ parent). Returns FALSE (accept) on any` |
|       - |  309 | ` * skipped/ambiguous shape.` |
|       - |  310 | ` */` |
|   15542 |  311 | `static int OoOverrideTypeBad(ph7_vm *pVm, OvType parent, OvType child, int bCovariant)` |
|       5 |  312 | `{` |
|       - |  313 | `	ph7_class *pParentCls, *pChildCls;` |
|   15547 |  314 | `	int kP = OoClassifyOverrideType(pVm, parent.nType, parent.pClass, parent.bUnion, &pParentCls);` |
|   15547 |  315 | `	int kC = OoClassifyOverrideType(pVm, child.nType, child.pClass, child.bUnion, &pChildCls);` |
|   15547 |  316 | `	if( kP == OVT_SKIP \|\| kC == OVT_SKIP ){` |
|      18 |  317 | `		return 0; /* ambiguous shape — conservatively accept */` |
|       - |  318 | `	}` |
|       - |  319 | `	/* A missing type is the TOP type. covariant (return): a concrete child is a` |
|       - |  320 | `	 * subtype of top, fine; a top child over a concrete parent WIDENS → bad.` |
|       - |  321 | `	 * contravariant (param): a top child is a supertype of anything, fine; a` |
|       - |  322 | `	 * concrete child over a top parent NARROWS → bad. (A union/intersection child` |
|       - |  323 | `	 * already fell into OVT_SKIP above, so a flagged child here is scalar/class.) */` |
|   15531 |  324 | `	if( kP == OVT_NONE \|\| kC == OVT_NONE ){` |
|   15511 |  325 | `		if( bCovariant && kC == OVT_NONE && kP != OVT_NONE ) return 1;` |
|   15511 |  326 | `		if( !bCovariant && kP == OVT_NONE && kC != OVT_NONE ) return 1;` |
|   15511 |  327 | `		return 0;` |
|       - |  328 | `	}` |
|       - |  329 | `	/* Nullability: a covariant return may not ADD null; a contravariant param may` |
|       - |  330 | `	 * not REMOVE null. */` |
|      24 |  331 | `	if( bCovariant ){` |
|      11 |  332 | `		if( child.bNullable && !parent.bNullable ) return 1;` |
|       7 |  333 | `	}else{` |
|      14 |  334 | `		if( parent.bNullable && !child.bNullable ) return 1;` |
|       - |  335 | `	}` |
|      24 |  336 | `	if( kP == OVT_SCALAR && kC == OVT_SCALAR ){` |
|       - |  337 | `		/* Scalars are invariant — they must match exactly. */` |
|      17 |  338 | `		return (parent.nType != child.nType) ? 1 : 0;` |
|       - |  339 | `	}` |
|       8 |  340 | `	if( kP == OVT_CLASS && kC == OVT_CLASS ){` |
|       8 |  341 | `		if( bCovariant ){` |
|       3 |  342 | `			return PH7_VmInstanceOf(pChildCls, pParentCls) ? 0 : 1;  /* child ⊆ parent */` |
|       - |  343 | `		}` |
|       6 |  344 | `		return PH7_VmInstanceOf(pParentCls, pChildCls) ? 0 : 1;      /* child ⊇ parent */` |
|       - |  345 | `	}` |
|       - |  346 | `	/* One scalar and one class — disjoint. */` |
|     ! 0 |  347 | `	return 1;` |
|    7776 |  348 | `}` |
|       - |  349 |  |
|       - |  350 | `/*` |
|       - |  351 | ` * Check a child method's signature against the parent method it overrides.` |
|       - |  352 | ` * Emits a PHP-style "Declaration of … must be compatible …" fatal on a clear` |
|       - |  353 | `` * incompatibility. `__construct` is exempt (PHP does not apply variance to it).`` |
|       - |  354 | ` */` |
|   31002 |  355 | `static sxi32 OoCheckOverrideCompat(ph7_gen_state *pGen, ph7_class *pBase, ph7_class *pSub,` |
|       - |  356 | `	ph7_class_method *pParent, ph7_class_method *pChild)` |
|       5 |  357 | `{` |
|   31007 |  358 | `	ph7_vm *pVm = pGen->pVm;` |
|   31007 |  359 | `	ph7_vm_func *pPF = &pParent->sFunc;` |
|   31007 |  360 | `	ph7_vm_func *pCF = &pChild->sFunc;` |
|   31007 |  361 | `	SyString *pMName = &pCF->sName;` |
|       - |  362 | `	ph7_vm_func_arg *aP, *aC;` |
|       - |  363 | `	sxu32 nPArg, nCArg, k;` |
|   31007 |  364 | `	int bBad = 0;` |
|   31002 |  365 | `	if( pMName->nByte == sizeof("__construct")-1` |
|   23249 |  366 | `	 && SyStrnmicmp(pMName->zString,"__construct",pMName->nByte) == 0 ){` |
|   15483 |  367 | `		return SXRET_OK;` |
|       - |  368 | `	}` |
|       - |  369 | `	/* Return type — covariant. */` |
|   15529 |  370 | `	bBad = OoOverrideTypeBad(pVm, OoTypeFromReturn(pPF), OoTypeFromReturn(pCF), /* bCovariant */ 1);` |
|       - |  371 | `	/* Each overlapping parameter — contravariant. */` |
|   15529 |  372 | `	nPArg = SySetUsed(&pPF->aArgs);` |
|   15529 |  373 | `	nCArg = SySetUsed(&pCF->aArgs);` |
|   15529 |  374 | `	aP = (ph7_vm_func_arg *)SySetBasePtr(&pPF->aArgs);` |
|   15529 |  375 | `	aC = (ph7_vm_func_arg *)SySetBasePtr(&pCF->aArgs);` |
|   15547 |  376 | `	for( k = 0; !bBad && k < nPArg && k < nCArg; k++ ){` |
|      21 |  377 | `		bBad = OoOverrideTypeBad(pVm, OoTypeFromArg(&aP[k]), OoTypeFromArg(&aC[k]), /* bCovariant */ 0);` |
|      12 |  378 | `	}` |
|       - |  379 | `	/* Parameter arity: the child must declare at least the parent's parameters and` |
|       - |  380 | `	 * may add only OPTIONAL ones — PHP rejects dropping any param (even an optional` |
|       - |  381 | `	 * one) or adding a required one. Skip the rule if either signature is variadic` |
|       - |  382 | `	 * (arity semantics differ). */` |
|   15529 |  383 | `	if( !bBad ){` |
|   15525 |  384 | `		int bVariadic = 0;` |
|   15541 |  385 | `		for( k = 0; k < nPArg; k++ ){ if( aP[k].iFlags & VM_FUNC_ARG_VARIADIC ) bVariadic = 1; }` |
|   15543 |  386 | `		for( k = 0; k < nCArg; k++ ){ if( aC[k].iFlags & VM_FUNC_ARG_VARIADIC ) bVariadic = 1; }` |
|   15525 |  387 | `		if( !bVariadic ){` |
|   15525 |  388 | `			if( nCArg < nPArg ){` |
|     ! 0 |  389 | `				bBad = 1; /* dropped a parent parameter */` |
|     ! 0 |  390 | `			}else{` |
|   15527 |  391 | `				for( k = nPArg; k < nCArg; k++ ){` |
|       3 |  392 | `					if( SySetUsed(&aC[k].aByteCode) == 0 ){ bBad = 1; break; } /* new required */` |
|       2 |  393 | `				}` |
|       - |  394 | `			}` |
|    7760 |  395 | `		}` |
|    7760 |  396 | `	}` |
|   15529 |  397 | `	if( bBad ){` |
|       8 |  398 | `		sxi32 rc = PH7_GenCompileError(&(*pGen),E_ERROR,pChild->nLine,` |
|       - |  399 | `			"Declaration of %z::%z() must be compatible with %z::%z()",` |
|       2 |  400 | `			&pSub->sName,pMName,&pBase->sName,&pParent->sFunc.sName);` |
|       6 |  401 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  402 | `			return SXERR_ABORT;` |
|       - |  403 | `		}` |
|       2 |  404 | `	}` |
|   15529 |  405 | `	return SXRET_OK;` |
|   15506 |  406 | `}` |
|       - |  407 | `/*` |
|       - |  408 | ` * Perform an inheritance operation.` |
|       - |  409 | ` * According to the PHP language reference manual` |
|       - |  410 | ` *  When you extend a class, the subclass inherits all of the public and protected methods` |
|       - |  411 | ` *  from the parent class. Unless a class Overwrites those methods, they will retain their original` |
|       - |  412 | ` *  functionality.` |
|       - |  413 | ` *  This is useful for defining and abstracting functionality, and permits the implementation` |
|       - |  414 | ` *  of additional functionality in similar objects without the need to reimplement all of the shared` |
|       - |  415 | ` *  functionality.` |
|       - |  416 | ` *  Example #1 Inheritance Example` |
|       - |  417 | ` * <?php` |
|       - |  418 | ` * class foo` |
|       - |  419 | ` * {` |
|       - |  420 | ` *   public function printItem($string)` |
|       - |  421 | ` *   {` |
|       - |  422 | ` *       echo 'Foo: ' . $string . PHP_EOL;` |
|       - |  423 | ` *   }` |
|       - |  424 | ` *` |
|       - |  425 | ` *   public function printPHP()` |
|       - |  426 | ` *   {` |
|       - |  427 | ` *       echo 'PHP is great.' . PHP_EOL;` |
|       - |  428 | ` *   }` |
|       - |  429 | ` * }` |
|       - |  430 | ` * class bar extends foo` |
|       - |  431 | ` * {` |
|       - |  432 | ` *   public function printItem($string)` |
|       - |  433 | ` *   {` |
|       - |  434 | ` *       echo 'Bar: ' . $string . PHP_EOL;` |
|       - |  435 | ` *   }` |
|       - |  436 | ` * }` |
|       - |  437 | ` * $foo = new foo();` |
|       - |  438 | ` * $bar = new bar();` |
|       - |  439 | ` * $foo->printItem('baz'); // Output: 'Foo: baz'` |
|       - |  440 | ` * $foo->printPHP();       // Output: 'PHP is great'` |
|       - |  441 | ` * $bar->printItem('baz'); // Output: 'Bar: baz'` |
|       - |  442 | ` * $bar->printPHP();       // Output: 'PHP is great'` |
|       - |  443 | ` *` |
|       - |  444 | ` * This function return SXRET_OK if the inheritance operation was successfully performed.` |
|       - |  445 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|       - |  446 | ` * error message.` |
|       - |  447 | ` */` |
|  123980 |  448 | `PH7_PRIVATE sxi32 PH7_ClassInherit(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase)` |
|       5 |  449 | `{` |
|       - |  450 | `	ph7_class_method *pMeth;` |
|       - |  451 | `	ph7_class_attr *pAttr;` |
|       - |  452 | `	SyHashEntry *pEntry;` |
|       - |  453 | `	SyString *pName;` |
|       - |  454 | `	sxi32 rc;` |
|       - |  455 | `	/* Install in the derived hashtable */` |
|  123985 |  456 | `	rc = SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|  123985 |  457 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  458 | `		return rc;` |
|       - |  459 | `	}` |
|       - |  460 | `	/* readonly class inheritance (PHP 8.2): a readonly class may only extend a` |
|       - |  461 | `	 * readonly class, and a non-readonly class may not extend a readonly one. */` |
|  123985 |  462 | `	if( (pBase->iFlags & PH7_CLASS_READONLY) != (pSub->iFlags & PH7_CLASS_READONLY) ){` |
|       5 |  463 | `		if( pBase->iFlags & PH7_CLASS_READONLY ){` |
|       4 |  464 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|       - |  465 | `				"Non-readonly class %z cannot extend readonly class %z",` |
|       1 |  466 | `				&pSub->sName,&pBase->sName);` |
|       2 |  467 | `		}else{` |
|       4 |  468 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|       - |  469 | `				"Readonly class %z cannot extend non-readonly class %z",` |
|       1 |  470 | `				&pSub->sName,&pBase->sName);` |
|       - |  471 | `		}` |
|       5 |  472 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  473 | `			return SXERR_ABORT;` |
|       - |  474 | `		}` |
|       2 |  475 | `	}` |
|       - |  476 | `	/* Copy public/protected attributes from the base class */` |
|  123985 |  477 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
|  805187 |  478 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|       - |  479 | `		/* Make sure the private attributes are not redeclared in the subclass */` |
|  681207 |  480 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  681207 |  481 | `		pName = &pAttr->sName;` |
|  681207 |  482 | `		if( (pEntry = SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|       6 |  483 | `			if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_FINAL))` |
|       6 |  484 | `				== (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_FINAL) ){` |
|       - |  485 | `				/* Cannot override a final class constant (PHP 8.1). Report the` |
|       - |  486 | `				 * class that originally declared it (pDeclClass) rather than the` |
|       - |  487 | `				 * immediate base, so a multi-level chain matches PHP. */` |
|       3 |  488 | `				ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pBase;` |
|       4 |  489 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_attr *)pEntry->pUserData)->nLine,` |
|       - |  490 | `					"%z::%z cannot override final constant %z::%z",` |
|       1 |  491 | `					&pSub->sName,pName,&pOwner->sName,pName);` |
|       3 |  492 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  493 | `					return SXERR_ABORT;` |
|       - |  494 | `				}` |
|       1 |  495 | `			}` |
|       9 |  496 | `			if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE &&` |
|       2 |  497 | `				((ph7_class_attr *)pEntry->pUserData)->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       - |  498 | `					/* Cannot redeclare private attribute */` |
|       4 |  499 | `					PH7_GenCompileError(&(*pGen),E_WARNING,((ph7_class_attr *)pEntry->pUserData)->nLine,` |
|       - |  500 | `						"Private attribute '%z::%z' redeclared inside child class '%z'",` |
|       1 |  501 | `						&pBase->sName,pName,&pSub->sName);` |
|       - |  502 |  |
|       1 |  503 | `			}` |
|       9 |  504 | `			continue;` |
|       - |  505 | `		}` |
|       - |  506 | `		/* Install the attribute */` |
|  681201 |  507 | `		if( pAttr->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
|  681197 |  508 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|  681197 |  509 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  510 | `				return rc;` |
|       - |  511 | `			}` |
|  340596 |  512 | `		}` |
|       5 |  513 | `	}` |
|  123985 |  514 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
| 1865335 |  515 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|       - |  516 | `		/* Make sure the private/final methods are not redeclared in the subclass */` |
| 1741355 |  517 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
| 1741355 |  518 | `		pName = &pMeth->sFunc.sName;` |
| 1741355 |  519 | `		if( (pEntry = SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|   31011 |  520 | `			 if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|       - |  521 | `				/* Cannot Overwrite final method */` |
|       8 |  522 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_method *)pEntry->pUserData)->nLine,` |
|       - |  523 | `					"Cannot Overwrite final method '%z:%z' inside child class '%z'",` |
|       2 |  524 | `					&pBase->sName,pName,&pSub->sName);` |
|       6 |  525 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  526 | `					return SXERR_ABORT;` |
|       - |  527 | `				}` |
|       4 |  528 | `			}else{` |
|       - |  529 | `				/* Check the override's signature is compatible with the parent's. */` |
|   46508 |  530 | `				rc = OoCheckOverrideCompat(&(*pGen),pBase,pSub,pMeth,` |
|   31002 |  531 | `					(ph7_class_method *)pEntry->pUserData);` |
|   31007 |  532 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  533 | `					return SXERR_ABORT;` |
|       - |  534 | `				}` |
|       - |  535 | `			}` |
|   31011 |  536 | `			continue;` |
|       - |  537 | `		}` |
|       - |  538 | `		/* Install the method */` |
| 1710349 |  539 | `		if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
| 1710345 |  540 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 1710345 |  541 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  542 | `				return rc;` |
|       - |  543 | `			}` |
|  855170 |  544 | `		}` |
|       5 |  545 | `	}` |
|       - |  546 | `	/* Mark as subclass */` |
|  123985 |  547 | `	pSub->pBase = pBase;` |
|       - |  548 | `	/* All done */` |
|  123985 |  549 | `	return SXRET_OK;` |
|   61995 |  550 | `}` |
|       - |  551 | `/*` |
|       - |  552 | ` * Apply a trait to a class: copy all methods and attributes from the trait` |
|       - |  553 | ` * into the target class. Unlike inheritance, traits copy ALL members including` |
|       - |  554 | ` * private ones. Members already defined in the class take precedence.` |
|       - |  555 | ` */` |
|      52 |  556 | `PH7_PRIVATE sxi32 PH7_ClassUseTrait(ph7_gen_state *pGen,ph7_class *pClass,ph7_class *pTrait)` |
|       5 |  557 | `{` |
|       - |  558 | `	ph7_class_method *pMeth;` |
|       - |  559 | `	ph7_class_attr *pAttr;` |
|       - |  560 | `	SyHashEntry *pEntry;` |
|       - |  561 | `	SyString *pName;` |
|       - |  562 | `	sxi32 rc;` |
|       - |  563 | `	/* Detect cyclic trait composition (e.g. trait A { use B; } trait B { use A; }) */` |
|      57 |  564 | `	if( pTrait->iFlags & PH7_CLASS_TRAIT_VISITING ){` |
|     ! 0 |  565 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|     ! 0 |  566 | `			"Trait circular reference detected: %z is already being applied",&pTrait->sName);` |
|     ! 0 |  567 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  568 | `			return SXERR_ABORT;` |
|       - |  569 | `		}` |
|     ! 0 |  570 | `		return SXRET_OK;` |
|       - |  571 | `	}` |
|      57 |  572 | `	pTrait->iFlags \|= PH7_CLASS_TRAIT_VISITING;` |
|      57 |  573 | `	rc = SXRET_OK;` |
|       - |  574 | `	/* Copy attributes from the trait */` |
|      57 |  575 | `	SyHashResetLoopCursor(&pTrait->hAttr);` |
|      73 |  576 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hAttr)) != 0 ){` |
|       - |  577 | `		SyHashEntry *pExisting;` |
|      20 |  578 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      20 |  579 | `		pName = &pAttr->sName;` |
|      20 |  580 | `		pExisting = SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte);` |
|      20 |  581 | `		if( pExisting != 0 ){` |
|       - |  582 | `			/* Attribute already exists. Check if it came from another trait` |
|       - |  583 | `			 * and whether the definitions are compatible (same defaults).` |
|       - |  584 | `			 */` |
|       - |  585 | `			ph7_class **apUsedTraits;` |
|       - |  586 | `			sxu32 nUsed,k;` |
|       6 |  587 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       6 |  588 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|       6 |  589 | `			for(k = 0; k < nUsed; k++){` |
|       - |  590 | `				ph7_class_attr *pOther;` |
|       3 |  591 | `				pOther = PH7_ClassExtractAttribute(apUsedTraits[k],pName->zString,pName->nByte);` |
|       3 |  592 | `				if( pOther ){` |
|       - |  593 | `					/* Two traits define the same property — check if defaults differ */` |
|       3 |  594 | `					ph7_class_attr *pClassAttr = (ph7_class_attr *)pExisting->pUserData;` |
|       4 |  595 | `					if( SySetUsed(&pAttr->aByteCode) != SySetUsed(&pClassAttr->aByteCode) \|\|` |
|       3 |  596 | `						(SySetUsed(&pAttr->aByteCode) > 0 &&` |
|       3 |  597 | `						 SyMemcmp(SySetBasePtr(&pAttr->aByteCode),SySetBasePtr(&pClassAttr->aByteCode),` |
|       3 |  598 | `							SySetUsed(&pAttr->aByteCode) * SySetElemSize(&pAttr->aByteCode)) != 0) ){` |
|       4 |  599 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine,` |
|       - |  600 | `							"%z and %z define the same property ($%z) in the composition of %z. "` |
|       - |  601 | `							"However, the definition differs and is considered incompatible",` |
|       2 |  602 | `							&apUsedTraits[k]->sName,&pTrait->sName,pName,&pClass->sName);` |
|       3 |  603 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  604 | `							goto cleanup;` |
|       - |  605 | `						}` |
|       1 |  606 | `					}` |
|       3 |  607 | `					break;` |
|       - |  608 | `				}` |
|     ! 0 |  609 | `			}` |
|       6 |  610 | `			continue;` |
|       - |  611 | `		}` |
|      16 |  612 | `		rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|      16 |  613 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  614 | `			goto cleanup;` |
|       - |  615 | `		}` |
|       4 |  616 | `	}` |
|       - |  617 | `	/* Copy methods from the trait */` |
|      57 |  618 | `	SyHashResetLoopCursor(&pTrait->hMethod);` |
|     105 |  619 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hMethod)) != 0 ){` |
|      53 |  620 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      53 |  621 | `		pName = &pMeth->sFunc.sName;` |
|      53 |  622 | `		if( SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte) != 0 ){` |
|       - |  623 | `			/* Method already exists in the class. Check if it came from another trait` |
|       - |  624 | `			 * (unresolved conflict) vs being defined by the class itself.` |
|       - |  625 | `			 */` |
|       - |  626 | `			ph7_class **apUsedTraits;` |
|       - |  627 | `			sxu32 nUsed,k;` |
|      11 |  628 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|      11 |  629 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|      11 |  630 | `			for(k = 0; k < nUsed; k++){` |
|       3 |  631 | `				if( PH7_ClassExtractMethod(apUsedTraits[k],pName->zString,pName->nByte) != 0 ){` |
|       - |  632 | `					/* Two different traits define the same method with no resolution */` |
|       4 |  633 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|       - |  634 | `						"Trait method %z::%z has not been applied as %z::%z, "` |
|       - |  635 | `						"because of collision with %z::%z",` |
|       2 |  636 | `						&pTrait->sName,pName,` |
|       1 |  637 | `						&pClass->sName,pName,` |
|       2 |  638 | `						&apUsedTraits[k]->sName,pName);` |
|       3 |  639 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  640 | `						goto cleanup;` |
|       - |  641 | `					}` |
|       3 |  642 | `					break;` |
|       - |  643 | `				}` |
|     ! 0 |  644 | `			}` |
|       - |  645 | `			/* Class-defined method takes precedence */` |
|      11 |  646 | `			continue;` |
|       - |  647 | `		}` |
|      45 |  648 | `		rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|      45 |  649 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  650 | `			goto cleanup;` |
|       - |  651 | `		}` |
|       5 |  652 | `	}` |
|       - |  653 | `	/* Record trait in the class */` |
|      57 |  654 | `	SySetPut(&pClass->aTrait,(const void *)&pTrait);` |
|      26 |  655 | `cleanup:` |
|       - |  656 | `	/* Always clear visiting flag, even on error paths */` |
|      57 |  657 | `	pTrait->iFlags &= ~PH7_CLASS_TRAIT_VISITING;` |
|      26 |  658 | `	SXUNUSED(pGen);` |
|      57 |  659 | `	return rc;` |
|      31 |  660 | `}` |
|       - |  661 | `/*` |
|       - |  662 | ` * Inherit an object interface from another object interface.` |
|       - |  663 | ` * According to the PHP language reference manual.` |
|       - |  664 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|       - |  665 | ` *  must implement, without having to define how these methods are handled.` |
|       - |  666 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  667 | ` *  class, but without any of the methods having their contents defined.` |
|       - |  668 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  669 | ` *` |
|       - |  670 | ` * This function return SXRET_OK if the interface inheritance operation was successfully performed.` |
|       - |  671 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|       - |  672 | ` * error message.` |
|       - |  673 | ` */` |
|   15482 |  674 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceInherit(ph7_class *pSub,ph7_class *pBase)` |
|       5 |  675 | `{` |
|       - |  676 | `	ph7_class_method *pMeth;` |
|       - |  677 | `	ph7_class_attr *pAttr;` |
|       - |  678 | `	SyHashEntry *pEntry;` |
|       - |  679 | `	SyString *pName;` |
|       - |  680 | `	sxi32 rc;` |
|       - |  681 | `	/* Install in the derived hashtable */` |
|   15487 |  682 | `	SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|   15487 |  683 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
|       - |  684 | `	/* Copy constants */` |
|   23230 |  685 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|       - |  686 | `		/* Make sure the constants are not redeclared in the subclass */` |
|       3 |  687 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|       3 |  688 | `		pName = &pAttr->sName;` |
|       3 |  689 | `		if( SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte) == 0 ){` |
|       - |  690 | `			/* Install the constant in the subclass */` |
|       3 |  691 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|       3 |  692 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  693 | `				return rc;` |
|       - |  694 | `			}` |
|       1 |  695 | `		}` |
|       1 |  696 | `	}` |
|   15487 |  697 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
|       - |  698 | `	/* Copy methods signature */` |
|   31006 |  699 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|       - |  700 | `		/* Make sure the method are not redeclared in the subclass */` |
|    7783 |  701 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|    7783 |  702 | `		pName = &pMeth->sFunc.sName;` |
|    7783 |  703 | `		if( SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte) == 0 ){` |
|       - |  704 | `			/* Install the method */` |
|    7783 |  705 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|    7783 |  706 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  707 | `				return rc;` |
|       - |  708 | `			}` |
|    3889 |  709 | `		}` |
|       5 |  710 | `	}` |
|       - |  711 | `	/* Mark as subclass */` |
|   15487 |  712 | `	pSub->pBase = pBase;` |
|       - |  713 | `	/* All done */` |
|   15487 |  714 | `	return SXRET_OK;` |
|    7746 |  715 | `}` |
|       - |  716 | `/*` |
|       - |  717 | ` * Implements an object interface in the given main class.` |
|       - |  718 | ` * According to the PHP language reference manual.` |
|       - |  719 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|       - |  720 | ` *  must implement, without having to define how these methods are handled.` |
|       - |  721 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  722 | ` *  class, but without any of the methods having their contents defined.` |
|       - |  723 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  724 | ` *` |
|       - |  725 | ` * This function return SXRET_OK if the interface was successfully implemented.` |
|       - |  726 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|       - |  727 | ` * error message.` |
|       - |  728 | ` */` |
|  213026 |  729 | `PH7_PRIVATE sxi32 PH7_ClassImplement(ph7_class *pMain,ph7_class *pInterface)` |
|       5 |  730 | `{` |
|       - |  731 | `	ph7_class_attr *pAttr;` |
|       - |  732 | `	SyHashEntry *pEntry;` |
|       - |  733 | `	SyString *pName;` |
|       - |  734 | `	sxi32 rc;` |
|       - |  735 | `	/* First off,copy all constants declared inside the interface */` |
|  213031 |  736 | `	SyHashResetLoopCursor(&pInterface->hAttr);` |
|  319550 |  737 | `	while((pEntry = SyHashGetNextEntry(&pInterface->hAttr)) != 0 ){` |
|       - |  738 | `		/* Point to the constant declaration */` |
|       7 |  739 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|       7 |  740 | `		pName = &pAttr->sName;` |
|       - |  741 | `		/* Make sure the attribute is not redeclared in the main class */` |
|       7 |  742 | `		if( SyHashGet(&pMain->hAttr,pName->zString,pName->nByte) == 0 ){` |
|       - |  743 | `			/* Install the attribute */` |
|       7 |  744 | `			rc = SyHashInsert(&pMain->hAttr,pName->zString,pName->nByte,pAttr);` |
|       7 |  745 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  746 | `				return rc;` |
|       - |  747 | `			}` |
|       3 |  748 | `		}` |
|       1 |  749 | `	}` |
|       - |  750 | `	/* Install in the interface container */` |
|  213031 |  751 | `	SySetPut(&pMain->aInterface,(const void *)&pInterface);` |
|       - |  752 | `	/* Install interface method stubs into the implementing class.` |
|       - |  753 | `	 * Methods already defined in the class take precedence (they satisfy` |
|       - |  754 | `	 * the interface contract). Stubs retain PH7_CLASS_ATTR_ABSTRACT so` |
|       - |  755 | `	 * the unified check in GenStateCheckAbstractMethods catches missing ones.` |
|       - |  756 | `	 */` |
|       - |  757 | `	{` |
|       - |  758 | `		ph7_class_method *pMeth;` |
|       - |  759 | `		SyHashEntry *pMEntry;` |
|       - |  760 | `		SyString *pMName;` |
|  213031 |  761 | `		SyHashResetLoopCursor(&pInterface->hMethod);` |
|  602418 |  762 | `		while((pMEntry = SyHashGetNextEntry(&pInterface->hMethod)) != 0 ){` |
|  282879 |  763 | `			pMeth = (ph7_class_method *)pMEntry->pUserData;` |
|  282879 |  764 | `			pMName = &pMeth->sFunc.sName;` |
|  282879 |  765 | `			if( SyHashGet(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte) == 0 ){` |
|      18 |  766 | `				rc = SyHashInsert(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte,pMeth);` |
|      18 |  767 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  768 | `					return rc;` |
|       - |  769 | `				}` |
|       7 |  770 | `			}` |
|       5 |  771 | `		}` |
|       - |  772 | `	}` |
|  213031 |  773 | `	return SXRET_OK;` |
|  106518 |  774 | `}` |
|       - |  775 | `/*` |
|       - |  776 | ` * Create a class instance [i.e: Object in the PHP jargon] at run-time.` |
|       - |  777 | ` * The following function is called when an object is created at run-time` |
|       - |  778 | ` * typically when the PH7_OP_NEW/PH7_OP_CLONE instructions are executed.` |
|       - |  779 | ` * Notes on object creation.` |
|       - |  780 | ` *` |
|       - |  781 | ` * According to PHP language reference manual.` |
|       - |  782 | ` * To create an instance of a class, the new keyword must be used. An object will always` |
|       - |  783 | ` * be created unless the object has a constructor defined that throws an exception on error.` |
|       - |  784 | ` * Classes should be defined before instantiation (and in some cases this is a requirement).` |
|       - |  785 | ` * If a string containing the name of a class is used with new, a new instance of that class` |
|       - |  786 | ` * will be created. If the class is in a namespace, its fully qualified name must be used when` |
|       - |  787 | ` * doing this.` |
|       - |  788 | ` * Example #3 Creating an instance` |
|       - |  789 | ` * <?php` |
|       - |  790 | ` *  $instance = new SimpleClass();` |
|       - |  791 | ` *   // This can also be done with a variable:` |
|       - |  792 | ` * $className = 'Foo';` |
|       - |  793 | ` * $instance = new $className(); // Foo()` |
|       - |  794 | ` * ?>` |
|       - |  795 | ` * In the class context, it is possible to create a new object by new self and new parent.` |
|       - |  796 | ` * When assigning an already created instance of a class to a new variable, the new variable` |
|       - |  797 | ` * will access the same instance as the object that was assigned. This behaviour is the same` |
|       - |  798 | ` * when passing instances to a function. A copy of an already created object can be made by` |
|       - |  799 | ` * cloning it.` |
|       - |  800 | ` * Example #4 Object Assignment` |
|       - |  801 | ` * <?php` |
|       - |  802 | ` *  class SimpleClass(){` |
|       - |  803 | ` *    public $var;` |
|       - |  804 | ` *  };` |
|       - |  805 | ` *  $instance = new SimpleClass();` |
|       - |  806 | ` *  $assigned   =  $instance;` |
|       - |  807 | ` *  $reference  =& $instance;` |
|       - |  808 | ` *  $instance->var = '$assigned will have this value';` |
|       - |  809 | ` *  $instance = null; // $instance and $reference become null` |
|       - |  810 | ` *  var_dump($instance);` |
|       - |  811 | ` *  var_dump($reference);` |
|       - |  812 | ` *  var_dump($assigned);` |
|       - |  813 | ` * ?>` |
|       - |  814 | ` * The above example will output:` |
|       - |  815 | ` * NULL` |
|       - |  816 | ` * NULL` |
|       - |  817 | ` * object(SimpleClass)#1 (1) {` |
|       - |  818 | ` *  ["var"]=>` |
|       - |  819 | ` *    string(30) "$assigned will have this value"` |
|       - |  820 | ` * }` |
|       - |  821 | ` * Example #5 Creating new objects` |
|       - |  822 | ` * <?php` |
|       - |  823 | ` * class Test` |
|       - |  824 | ` * {` |
|       - |  825 | ` *   static public function getNew()` |
|       - |  826 | ` *   {` |
|       - |  827 | ` *       return new static;` |
|       - |  828 | ` *   }` |
|       - |  829 | ` * }` |
|       - |  830 | ` * class Child extends Test` |
|       - |  831 | ` * {}` |
|       - |  832 | ` * $obj1 = new Test();` |
|       - |  833 | ` * $obj2 = new $obj1;` |
|       - |  834 | ` * var_dump($obj1 !== $obj2);` |
|       - |  835 | ` * $obj3 = Test::getNew();` |
|       - |  836 | ` * var_dump($obj3 instanceof Test);` |
|       - |  837 | ` * $obj4 = Child::getNew();` |
|       - |  838 | ` * var_dump($obj4 instanceof Child);` |
|       - |  839 | ` * ?>` |
|       - |  840 | ` * The above example will output:` |
|       - |  841 | ` * bool(true)` |
|       - |  842 | ` * bool(true)` |
|       - |  843 | ` * bool(true)` |
|       - |  844 | ` * Note that Symisc Systems have introduced powerfull extension to` |
|       - |  845 | ` * OO subsystem. For example a class attribute may have any complex` |
|       - |  846 | ` * expression associated with it when declaring the attribute unlike` |
|       - |  847 | ` * the standard PHP engine which would allow a single value.` |
|       - |  848 | ` * Example:` |
|       - |  849 | ` *  class myClass{` |
|       - |  850 | ` *    public $var = 25<<1+foo()/bar();` |
|       - |  851 | ` *  };` |
|       - |  852 | ` * Refer to the official documentation for more information.` |
|       - |  853 | ` */` |
|    5886 |  854 | `static ph7_class_instance * NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|       5 |  855 | `{` |
|       - |  856 | `	ph7_class_instance *pThis;` |
|       - |  857 | `	/* Allocate a new instance */` |
|    5891 |  858 | `	pThis = (ph7_class_instance *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_instance));` |
|    5891 |  859 | `	if( pThis == 0 ){` |
|     ! 0 |  860 | `		return 0;` |
|       - |  861 | `	}` |
|       - |  862 | `	/* Zero the structure */` |
|    5891 |  863 | `	SyZero(pThis,sizeof(ph7_class_instance));` |
|       - |  864 | `	/* Initialize fields */` |
|    5891 |  865 | `	pThis->iRef = 1;` |
|    5891 |  866 | `	pThis->pVm = pVm;` |
|    5891 |  867 | `	pThis->pClass = pClass;` |
|       - |  868 | `	/* Assign a fresh monotonic object handle id (clones get their own, like PHP). */` |
|    5891 |  869 | `	pThis->nObjId = pVm->nNextObjId++;` |
|    5891 |  870 | `	SyHashInit(&pThis->hAttr,&pVm->sAllocator,0,0);` |
|    5891 |  871 | `	return pThis;` |
|    2948 |  872 | `}` |
|       - |  873 | `/*` |
|       - |  874 | ` * Wrapper around the NewClassInstance() function defined above.` |
|       - |  875 | ` * See the block comment above for more information.` |
|       - |  876 | ` */` |
|    5778 |  877 | `PH7_PRIVATE ph7_class_instance * PH7_NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|       5 |  878 | `{` |
|       - |  879 | `	ph7_class_instance *pNew;` |
|       - |  880 | `	sxi32 rc;` |
|    5783 |  881 | `	pNew = NewClassInstance(&(*pVm),&(*pClass));` |
|    5783 |  882 | `	if( pNew == 0 ){` |
|     ! 0 |  883 | `		return 0;` |
|       - |  884 | `	}` |
|       - |  885 | `	/* Associate a private VM frame with this class instance */` |
|    5783 |  886 | `	rc = PH7_VmCreateClassInstanceFrame(&(*pVm),pNew);` |
|    5783 |  887 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  888 | `		SyMemBackendPoolFree(&pVm->sAllocator,pNew);` |
|     ! 0 |  889 | `		return 0;` |
|       - |  890 | `	}` |
|    5783 |  891 | `	return pNew;` |
|    2894 |  892 | `}` |
|       - |  893 | `/*` |
|       - |  894 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon] attribute.` |
|       - |  895 | ` * This function never fail.` |
|       - |  896 | ` */` |
|    7896 |  897 | `static ph7_value * ExtractClassAttrValue(ph7_vm *pVm,VmClassAttr *pAttr)` |
|       5 |  898 | `{` |
|       - |  899 | `	/* Extract the value */` |
|       - |  900 | `	ph7_value *pValue;` |
|    7901 |  901 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|    7901 |  902 | `	return pValue;` |
|       5 |  903 | `}` |
|       - |  904 | `/*` |
|       - |  905 | ` * Perform a clone operation on a class instance [i.e: Object in the PHP jargon].` |
|       - |  906 | ` * The following function is called when an object is cloned at run-time` |
|       - |  907 | ` * typically when the PH7_OP_CLONE instruction is executed.` |
|       - |  908 | ` * Notes on object cloning.` |
|       - |  909 | ` *` |
|       - |  910 | ` * According to PHP language reference manual.` |
|       - |  911 | ` * Creating a copy of an object with fully replicated properties is not always the wanted behavior.` |
|       - |  912 | ` * A good example of the need for copy constructors. Another example is if your object holds a reference` |
|       - |  913 | ` * to another object which it uses and when you replicate the parent object you want to create` |
|       - |  914 | ` * a new instance of this other object so that the replica has its own separate copy.` |
|       - |  915 | ` * An object copy is created by using the clone keyword (which calls the object's __clone() method if possible).` |
|       - |  916 | ` * An object's __clone() method cannot be called directly.` |
|       - |  917 | ` * $copy_of_object = clone $object;` |
|       - |  918 | ` * When an object is cloned, PHP 5 will perform a shallow copy of all of the object's properties.` |
|       - |  919 | ` * Any properties that are references to other variables, will remain references.` |
|       - |  920 | ` * Once the cloning is complete, if a __clone() method is defined, then the newly created object's __clone() method` |
|       - |  921 | ` * will be called, to allow any necessary properties that need to be changed.` |
|       - |  922 | ` * Example #1 Cloning an object` |
|       - |  923 | ` * <?php` |
|       - |  924 | ` * class SubObject` |
|       - |  925 | ` * {` |
|       - |  926 | ` *   static $instances = 0;` |
|       - |  927 | ` *   public $instance;` |
|       - |  928 | ` *` |
|       - |  929 | ` *   public function __construct() {` |
|       - |  930 | ` *       $this->instance = ++self::$instances;` |
|       - |  931 | ` *   }` |
|       - |  932 | ` *` |
|       - |  933 | ` *   public function __clone() {` |
|       - |  934 | ` *       $this->instance = ++self::$instances;` |
|       - |  935 | ` *   }` |
|       - |  936 | ` * }` |
|       - |  937 | ` *` |
|       - |  938 | ` * class MyCloneable` |
|       - |  939 | ` * {` |
|       - |  940 | ` *   public $object1;` |
|       - |  941 | ` *   public $object2;` |
|       - |  942 | ` *` |
|       - |  943 | ` *   function __clone()` |
|       - |  944 | ` *   {` |
|       - |  945 | ` *       // Force a copy of this->object, otherwise` |
|       - |  946 | ` *       // it will point to same object.` |
|       - |  947 | ` *       $this->object1 = clone $this->object1;` |
|       - |  948 | ` *   }` |
|       - |  949 | ` * }` |
|       - |  950 | ` * $obj = new MyCloneable();` |
|       - |  951 | ` * $obj->object1 = new SubObject();` |
|       - |  952 | ` * $obj->object2 = new SubObject();` |
|       - |  953 | ` * $obj2 = clone $obj;` |
|       - |  954 | ` * print("Original Object:\n");` |
|       - |  955 | ` * print_r($obj);` |
|       - |  956 | ` * print("Cloned Object:\n");` |
|       - |  957 | ` * print_r($obj2);` |
|       - |  958 | ` * ?>` |
|       - |  959 | ` * The above example will output:` |
|       - |  960 | ` * Original Object:` |
|       - |  961 | ` * MyCloneable Object` |
|       - |  962 | ` * (` |
|       - |  963 | ` *   [object1] => SubObject Object` |
|       - |  964 | ` *       (` |
|       - |  965 | ` *           [instance] => 1` |
|       - |  966 | ` *       )` |
|       - |  967 | ` *` |
|       - |  968 | ` *   [object2] => SubObject Object` |
|       - |  969 | ` *       (` |
|       - |  970 | ` *           [instance] => 2` |
|       - |  971 | ` *       )` |
|       - |  972 | ` *` |
|       - |  973 | ` * )` |
|       - |  974 | ` * Cloned Object:` |
|       - |  975 | ` * MyCloneable Object` |
|       - |  976 | ` * (` |
|       - |  977 | ` *   [object1] => SubObject Object` |
|       - |  978 | ` *       (` |
|       - |  979 | ` *           [instance] => 3` |
|       - |  980 | ` *       )` |
|       - |  981 | ` *` |
|       - |  982 | ` *   [object2] => SubObject Object` |
|       - |  983 | ` *       (` |
|       - |  984 | ` *           [instance] => 2` |
|       - |  985 | ` *       )` |
|       - |  986 | ` * )` |
|       - |  987 | ` */` |
|     108 |  988 | `PH7_PRIVATE ph7_class_instance * PH7_CloneClassInstance(ph7_class_instance *pSrc)` |
|       3 |  989 | `{` |
|       - |  990 | `	ph7_class_instance *pClone;` |
|       - |  991 | `	ph7_class_method *pMethod;` |
|       - |  992 | `	SyHashEntry *pEntry2;` |
|       - |  993 | `	SyHashEntry *pEntry;` |
|       - |  994 | `	ph7_vm *pVm;` |
|       - |  995 | `	sxi32 rc;` |
|       - |  996 | `	/* Allocate a new instance */` |
|     111 |  997 | `	pVm = pSrc->pVm;` |
|     111 |  998 | `	pClone = NewClassInstance(pVm,pSrc->pClass);` |
|     111 |  999 | `	if( pClone == 0 ){` |
|     ! 0 | 1000 | `		return 0;` |
|       - | 1001 | `	}` |
|       - | 1002 | `	/* Associate a private VM frame with this class instance */` |
|     111 | 1003 | `	rc = PH7_VmCreateClassInstanceFrame(pVm,pClone);` |
|     111 | 1004 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1005 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClone);` |
|     ! 0 | 1006 | `		return 0;` |
|       - | 1007 | `	}` |
|       - | 1008 | `	/* Duplicate object values. Iterate the SOURCE attributes and copy each into` |
|       - | 1009 | `	 * the clone's same-named slot (looked up by name, so order/count differences` |
|       - | 1010 | `	 * from dynamic properties don't matter). A dynamic (runtime-added) property` |
|       - | 1011 | `	 * has no declared counterpart in the clone, so synthesize it first — without` |
|       - | 1012 | `	 * this, a clone of a stdClass would silently lose all its dynamic properties. */` |
|     111 | 1013 | `	SyHashResetLoopCursor(&pSrc->hAttr);` |
|     293 | 1014 | `	while((pEntry = SyHashGetNextEntry(&pSrc->hAttr)) != 0 ){` |
|     185 | 1015 | `		VmClassAttr *pSrcAttr = (VmClassAttr *)pEntry->pUserData;` |
|     185 | 1016 | `		VmClassAttr *pDestAttr = 0;` |
|     185 | 1017 | `		ph7_value *pvSrc,*pvDest = 0;` |
|       - | 1018 | `		/* Duplicate non-static attribute */` |
|     185 | 1019 | `		if( pSrcAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|     ! 0 | 1020 | `			continue;` |
|       - | 1021 | `		}` |
|     185 | 1022 | `		pEntry2 = SyHashGet(&pClone->hAttr,SyStringData(&pSrcAttr->pAttr->sName),SyStringLength(&pSrcAttr->pAttr->sName));` |
|     185 | 1023 | `		if( pEntry2 ){` |
|     171 | 1024 | `			pDestAttr = (VmClassAttr *)pEntry2->pUserData;` |
|     171 | 1025 | `			pvDest = ExtractClassAttrValue(pVm,pDestAttr);` |
|      99 | 1026 | `		}else if( pSrcAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){` |
|       - | 1027 | `			/* Dynamic property: synthesize the matching slot on the clone. */` |
|      22 | 1028 | `			pvDest = PH7_VmCreateDynamicAttr(pVm,pClone,` |
|      14 | 1029 | `				SyStringData(&pSrcAttr->pAttr->sName),SyStringLength(&pSrcAttr->pAttr->sName),&pDestAttr);` |
|       7 | 1030 | `		}` |
|       - | 1031 | `		/* Fetch the source value LAST: PH7_VmCreateDynamicAttr above may have` |
|       - | 1032 | `		 * reserved a slot and reallocated pVm->aMemObj, which would dangle any` |
|       - | 1033 | `		 * ph7_value* obtained before it. pvDest from the synth path already points` |
|       - | 1034 | `		 * into the post-realloc aMemObj; resolve pvSrc now so both are current. */` |
|     185 | 1035 | `		pvSrc = ExtractClassAttrValue(pVm,pSrcAttr);` |
|     185 | 1036 | `		if( pvSrc && pvDest ){` |
|     185 | 1037 | `			PH7_MemObjStore(pvSrc,pvDest);` |
|      91 | 1038 | `		}` |
|       - | 1039 | `		/* Carry over the per-instance state so the clone matches the source:` |
|       - | 1040 | `		 * VM_CLASS_ATTR_UNINIT marks a typed property as not-yet-initialized` |
|       - | 1041 | `		 * and doubles as the readonly write-once latch — without this a clone` |
|       - | 1042 | `		 * would reset to uninitialized (losing the value's readiness) and a` |
|       - | 1043 | `		 * readonly property would become writable again. */` |
|     185 | 1044 | `		if( pDestAttr ){` |
|     185 | 1045 | `			pDestAttr->iState = pSrcAttr->iState;` |
|      91 | 1046 | `		}` |
|       3 | 1047 | `	}` |
|       - | 1048 | `	/* A declared property unset() on the source is absent from the clone too (PHP). But the clone` |
|       - | 1049 | `	 * frame above materialized ALL declared attrs (with their defaults), so drop any clone attr whose` |
|       - | 1050 | `	 * name is not present on the source. Collect first, then delete — removing an entry mid-walk would` |
|       - | 1051 | `	 * free the node the SyHash loop cursor points at. */` |
|       - | 1052 | `	{` |
|       - | 1053 | `		SySet sDrop;` |
|     111 | 1054 | `		SySetInit(&sDrop,&pVm->sAllocator,sizeof(VmClassAttr *));` |
|     111 | 1055 | `		SyHashResetLoopCursor(&pClone->hAttr);` |
|     295 | 1056 | `		while((pEntry = SyHashGetNextEntry(&pClone->hAttr)) != 0 ){` |
|     187 | 1057 | `			VmClassAttr *pCloneAttr = (VmClassAttr *)pEntry->pUserData;` |
|     187 | 1058 | `			if( pCloneAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|     ! 0 | 1059 | `				continue;` |
|       - | 1060 | `			}` |
|     276 | 1061 | `			if( SyHashGet(&pSrc->hAttr,SyStringData(&pCloneAttr->pAttr->sName),` |
|     279 | 1062 | `					SyStringLength(&pCloneAttr->pAttr->sName)) == 0 ){` |
|       3 | 1063 | `				SySetPut(&sDrop,(const void *)&pCloneAttr);` |
|       1 | 1064 | `			}` |
|       3 | 1065 | `		}` |
|     111 | 1066 | `		if( SySetUsed(&sDrop) > 0 ){` |
|       3 | 1067 | `			VmClassAttr **apDrop = (VmClassAttr **)SySetBasePtr(&sDrop);` |
|       - | 1068 | `			sxu32 i;` |
|       5 | 1069 | `			for( i = 0 ; i < SySetUsed(&sDrop) ; ++i ){` |
|       3 | 1070 | `				VmClassAttr *pVmAttr = apDrop[i];` |
|       4 | 1071 | `				SyHashDeleteEntry(&pClone->hAttr,SyStringData(&pVmAttr->pAttr->sName),` |
|       2 | 1072 | `					SyStringLength(&pVmAttr->pAttr->sName),0);` |
|       3 | 1073 | `				PH7_VmReleaseInstanceAttr(pVm,pVmAttr);` |
|       2 | 1074 | `			}` |
|       1 | 1075 | `		}` |
|     111 | 1076 | `		SySetRelease(&sDrop);` |
|       - | 1077 | `	}` |
|       - | 1078 | `	/* call the __clone method on the cloned object if available */` |
|     111 | 1079 | `	pMethod = PH7_ClassExtractMethod(pClone->pClass,"__clone",sizeof("__clone")-1);` |
|     111 | 1080 | `	if( pMethod ){` |
|      56 | 1081 | `		if( pMethod->iCloneDepth < 16 ){` |
|      54 | 1082 | `			pMethod->iCloneDepth++;` |
|      54 | 1083 | `			PH7_VmCallClassMethod(pVm,pClone,pMethod,0,0,0);` |
|      28 | 1084 | `		}else{` |
|       - | 1085 | `			/* Nesting limit reached */` |
|       3 | 1086 | `			PH7_VmThrowError(pVm,0,PH7_CTX_ERR,"Object clone limit reached,no more call to __clone()");` |
|       - | 1087 | `		}` |
|       - | 1088 | `		/* Reset the cursor */` |
|      56 | 1089 | `		pMethod->iCloneDepth = 0;` |
|      27 | 1090 | `	}` |
|       - | 1091 | `	/* Return the cloned object */` |
|     111 | 1092 | `	return pClone;` |
|      57 | 1093 | `}` |
|       - | 1094 | `#define CLASS_INSTANCE_DESTROYED 0x001 /* Instance is released */` |
|       - | 1095 | `/*` |
|       - | 1096 | ` * Free the per-instance allocations owned by ONE object attribute: its value slot (+ the typed-slot` |
|       - | 1097 | ` * enforcement entry), the synthesized ph7_class_attr for a dynamic (runtime-added) property, and the` |
|       - | 1098 | ` * VmClassAttr wrapper itself. Does NOT touch the hAttr entry node — the caller removes it` |
|       - | 1099 | `` * (`unset($o->p)` via SyHashDeleteEntry2; instance teardown via the wholesale SyHashRelease, so it must`` |
|       - | 1100 | ` * not delete entries mid-walk). Shared by PH7_ClassInstanceRelease and the OP_MEMBER unset path.` |
|       - | 1101 | ` */` |
|   16768 | 1102 | `PH7_PRIVATE void PH7_VmReleaseInstanceAttr(ph7_vm *pVm, VmClassAttr *pVmAttr)` |
|       5 | 1103 | `{` |
|   16773 | 1104 | `	if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|       - | 1105 | `		/* Drop any typed-property enforcement slot registered for this memobj, before the memobj` |
|       - | 1106 | `		 * is returned to the free list, so a future recycled slot does not inherit the stale entry. */` |
|   15011 | 1107 | `		if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|     244 | 1108 | `			SyHashDeleteEntry(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),0);` |
|     120 | 1109 | `		}` |
|   15011 | 1110 | `		PH7_VmUnsetMemObj(pVm,pVmAttr->nIdx,TRUE);` |
|    7503 | 1111 | `	}` |
|       - | 1112 | `	/* A dynamic property owns its synthesized ph7_class_attr (struct + inline name in one block) —` |
|       - | 1113 | `	 * free it here (the only place a per-instance pAttr is freed; declared attrs are class-owned). */` |
|   16773 | 1114 | `	if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){` |
|     111 | 1115 | `		SyMemBackendFree(&pVm->sAllocator,pVmAttr->pAttr);` |
|      55 | 1116 | `	}` |
|   16773 | 1117 | `	SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|   16773 | 1118 | `}` |
|       - | 1119 | `/*` |
|       - | 1120 | ` * Release a class instance [i.e: Object in the PHP jargon] and invoke any defined destructor.` |
|       - | 1121 | ` * This routine is invoked as soon as there are no other references to a particular` |
|       - | 1122 | ` * class instance.` |
|       - | 1123 | ` */` |
|    4028 | 1124 | `static void PH7_ClassInstanceRelease(ph7_class_instance *pThis)` |
|       5 | 1125 | `{` |
|       - | 1126 | `	ph7_class_method *pDestr;` |
|       - | 1127 | `	SyHashEntry *pEntry;` |
|       - | 1128 | `	ph7_class *pClass;` |
|       - | 1129 | `	ph7_vm *pVm;` |
|    4033 | 1130 | `	if( pThis->iFlags & CLASS_INSTANCE_DESTROYED ){` |
|       - | 1131 | `		/*` |
|       - | 1132 | `		 * Already destroyed,return immediately.` |
|       - | 1133 | `		 * This could happend if someone perform unset($this) in the destructor body.` |
|       - | 1134 | `		 */` |
|     ! 0 | 1135 | `		return;` |
|       - | 1136 | `	}` |
|       - | 1137 | `	/* Mark as destroyed */` |
|    4033 | 1138 | `	pThis->iFlags \|= CLASS_INSTANCE_DESTROYED;` |
|       - | 1139 | `	/* Invoke any defined destructor if available */` |
|    4033 | 1140 | `	pVm = pThis->pVm;` |
|    4033 | 1141 | `	pClass = pThis->pClass;` |
|    4033 | 1142 | `	pDestr = PH7_ClassExtractMethod(pClass,"__destruct",sizeof("__destruct")-1);` |
|    4033 | 1143 | `	if( pDestr && !pVm->bInReset ){` |
|       - | 1144 | `		/* Invoke the destructor. Skipped during ph7_vm_reset() bulk teardown:` |
|       - | 1145 | `		 * running user PHP against a half-reset VM is unsafe (see bInReset). */` |
|     413 | 1146 | `		pThis->iRef = 2; /* Prevent garbage collection */` |
|     413 | 1147 | `		PH7_VmCallClassMethod(pVm,pThis,pDestr,0,0,0);` |
|     204 | 1148 | `	}` |
|       - | 1149 | `	/* Release non-static attributes (the wholesale SyHashRelease below frees the entry nodes,` |
|       - | 1150 | `	 * so the helper must not delete them mid-walk). */` |
|    4033 | 1151 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|   20771 | 1152 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|   16743 | 1153 | `		PH7_VmReleaseInstanceAttr(pVm,(VmClassAttr *)pEntry->pUserData);` |
|       5 | 1154 | `	}` |
|       - | 1155 | `	/* Release the whole structure */` |
|    4033 | 1156 | `	SyHashRelease(&pThis->hAttr);` |
|    4033 | 1157 | `	SyMemBackendPoolFree(&pVm->sAllocator,pThis);` |
|    2019 | 1158 | `}` |
|       - | 1159 | `/*` |
|       - | 1160 | ` * Decrement the reference count of a class instance [i.e Object in the PHP jargon].` |
|       - | 1161 | ` * If the reference count reaches zero,release the whole instance.` |
|       - | 1162 | ` */` |
|   84480 | 1163 | `PH7_PRIVATE void PH7_ClassInstanceUnref(ph7_class_instance *pThis)` |
|       5 | 1164 | `{` |
|   84485 | 1165 | `	pThis->iRef--;` |
|   84485 | 1166 | `	if( pThis->iRef < 1 ){` |
|       - | 1167 | `		/* No more reference to this instance */` |
|    4033 | 1168 | `		PH7_ClassInstanceRelease(&(*pThis));` |
|    2014 | 1169 | `	}` |
|   84485 | 1170 | `}` |
|       - | 1171 | `/*` |
|       - | 1172 | ` * Compare two class instances [i.e: Objects in the PHP jargon]` |
|       - | 1173 | ` * Note on objects comparison:` |
|       - | 1174 | ` *  According to the PHP langauge reference manual` |
|       - | 1175 | ` *  When using the comparison operator (==), object variables are compared in a simple manner` |
|       - | 1176 | ` *  namely: Two object instances are equal if they have the same attributes and values, and are` |
|       - | 1177 | ` *  instances of the same class.` |
|       - | 1178 | ` *  On the other hand, when using the identity operator (===), object variables are identical` |
|       - | 1179 | ` *  if and only if they refer to the same instance of the same class.` |
|       - | 1180 | ` *  An example will clarify these rules.` |
|       - | 1181 | ` *  Example #1 Example of object comparison` |
|       - | 1182 | ` *  <?php` |
|       - | 1183 | ` *    function bool2str($bool)` |
|       - | 1184 | ` * {` |
|       - | 1185 | ` *   if ($bool === false) {` |
|       - | 1186 | ` *       return 'FALSE';` |
|       - | 1187 | ` *   } else {` |
|       - | 1188 | ` *       return 'TRUE';` |
|       - | 1189 | ` *   }` |
|       - | 1190 | ` * }` |
|       - | 1191 | ` * function compareObjects(&$o1, &$o2)` |
|       - | 1192 | ` * {` |
|       - | 1193 | ` *   echo 'o1 == o2 : ' . bool2str($o1 == $o2) . "\n";` |
|       - | 1194 | ` *   echo 'o1 != o2 : ' . bool2str($o1 != $o2) . "\n";` |
|       - | 1195 | ` *   echo 'o1 === o2 : ' . bool2str($o1 === $o2) . "\n";` |
|       - | 1196 | ` *   echo 'o1 !== o2 : ' . bool2str($o1 !== $o2) . "\n";` |
|       - | 1197 | ` * }` |
|       - | 1198 | ` * class Flag` |
|       - | 1199 | ` * {` |
|       - | 1200 | ` *   public $flag;` |
|       - | 1201 | ` *` |
|       - | 1202 | ` *   function Flag($flag = true) {` |
|       - | 1203 | ` *       $this->flag = $flag;` |
|       - | 1204 | ` *   }` |
|       - | 1205 | ` * }` |
|       - | 1206 | ` *` |
|       - | 1207 | ` * class OtherFlag` |
|       - | 1208 | ` * {` |
|       - | 1209 | ` *   public $flag;` |
|       - | 1210 | ` *` |
|       - | 1211 | ` *   function OtherFlag($flag = true) {` |
|       - | 1212 | ` *       $this->flag = $flag;` |
|       - | 1213 | ` *   }` |
|       - | 1214 | ` * }` |
|       - | 1215 | ` *` |
|       - | 1216 | ` * $o = new Flag();` |
|       - | 1217 | ` * $p = new Flag();` |
|       - | 1218 | ` * $q = $o;` |
|       - | 1219 | ` * $r = new OtherFlag();` |
|       - | 1220 | ` *` |
|       - | 1221 | ` * echo "Two instances of the same class\n";` |
|       - | 1222 | ` * compareObjects($o, $p);` |
|       - | 1223 | ` * echo "\nTwo references to the same instance\n";` |
|       - | 1224 | ` * compareObjects($o, $q);` |
|       - | 1225 | ` * echo "\nInstances of two different classes\n";` |
|       - | 1226 | ` * compareObjects($o, $r);` |
|       - | 1227 | ` * ?>` |
|       - | 1228 | ` * The above example will output:` |
|       - | 1229 | ` * Two instances of the same class` |
|       - | 1230 | ` * o1 == o2 : TRUE` |
|       - | 1231 | ` * o1 != o2 : FALSE` |
|       - | 1232 | ` * o1 === o2 : FALSE` |
|       - | 1233 | ` * o1 !== o2 : TRUE` |
|       - | 1234 | ` * Two references to the same instance` |
|       - | 1235 | ` * o1 == o2 : TRUE` |
|       - | 1236 | ` * o1 != o2 : FALSE` |
|       - | 1237 | ` * o1 === o2 : TRUE` |
|       - | 1238 | ` * o1 !== o2 : FALSE` |
|       - | 1239 | ` * Instances of two different classes` |
|       - | 1240 | ` * o1 == o2 : FALSE` |
|       - | 1241 | ` * o1 != o2 : TRUE` |
|       - | 1242 | ` * o1 === o2 : FALSE` |
|       - | 1243 | ` * o1 !== o2 : TRUE` |
|       - | 1244 | ` *` |
|       - | 1245 | ` * This function return 0 if the objects are equals according to the comprison rules defined above.` |
|       - | 1246 | ` * Any other return values indicates difference.` |
|       - | 1247 | ` */` |
|     230 | 1248 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest)` |
|       5 | 1249 | `{` |
|       - | 1250 | `	SyHashEntry *pEntry,*pEntry2;` |
|       - | 1251 | `	ph7_value sV1,sV2;` |
|       - | 1252 | `	sxi32 rc;` |
|     235 | 1253 | `	if( iNest > 31 ){` |
|       - | 1254 | `		/* Nesting limit reached */` |
|       6 | 1255 | `		PH7_VmThrowError(pLeft->pVm,0,PH7_CTX_ERR,"Nesting limit reached: Infinite recursion?");` |
|       6 | 1256 | `		return 1;` |
|       - | 1257 | `	}` |
|       - | 1258 | `	/* Comparison is performed only if the objects are instance of the same class */` |
|     231 | 1259 | `	if( pLeft->pClass != pRight->pClass ){` |
|       7 | 1260 | `		return 1;` |
|       - | 1261 | `	}` |
|     225 | 1262 | `	if( bStrict ){` |
|       - | 1263 | `		/*` |
|       - | 1264 | `		 * According to the PHP language reference manual:` |
|       - | 1265 | `		 *  when using the identity operator (===), object variables` |
|       - | 1266 | `		 *  are identical if and only if they refer to the same instance` |
|       - | 1267 | `		 *  of the same class.` |
|       - | 1268 | `		 */` |
|      73 | 1269 | `		return !(pLeft == pRight);` |
|       - | 1270 | `	}` |
|       - | 1271 | `	/*` |
|       - | 1272 | `	 * Attribute comparison.` |
|       - | 1273 | `	 * According to the PHP reference manual:` |
|       - | 1274 | `	 *  When using the comparison operator (==), object variables are compared` |
|       - | 1275 | `	 *  in a simple manner, namely: Two object instances are equal if they have` |
|       - | 1276 | `	 *  the same attributes and values, and are instances of the same class.` |
|       - | 1277 | `	 */` |
|     155 | 1278 | `	if( pLeft == pRight ){` |
|       - | 1279 | `		/* Same instance,don't bother processing,object are equals */` |
|       5 | 1280 | `		return 0;` |
|       - | 1281 | `	}` |
|       - | 1282 | `	/* Closures compare by IDENTITY under == as well (not by attributes): two distinct` |
|       - | 1283 | `	 * Closure instances are never equal, even when they wrap the same underlying function` |
|       - | 1284 | `	 * (PHP semantics). pLeft != pRight here, so a Closure pair is unequal. Without this,` |
|       - | 1285 | `` 	 * two capture-less lambdas of the same `function(){}` share the template's `$__fn` `` |
|       - | 1286 | `	 * name and would compare equal. */` |
|     151 | 1287 | `	if( pLeft->pVm->pClosureClass && pLeft->pClass == pLeft->pVm->pClosureClass ){` |
|       5 | 1288 | `		return 1;` |
|       - | 1289 | `	}` |
|       - | 1290 | `	/* Same class but a different number of attributes ⇒ different property sets` |
|       - | 1291 | `	 * (dynamic properties can give two same-class instances different counts). */` |
|     147 | 1292 | `	if( pLeft->hAttr.nEntry != pRight->hAttr.nEntry ){` |
|       3 | 1293 | `		return 1;` |
|       - | 1294 | `	}` |
|     145 | 1295 | `	PH7_MemObjInit(pLeft->pVm,&sV1);` |
|     145 | 1296 | `	PH7_MemObjInit(pLeft->pVm,&sV2);` |
|     145 | 1297 | `	sV1.nIdx = sV2.nIdx = SXU32_HIGH;` |
|       - | 1298 | `	/* Compare each left attribute against the RIGHT attribute of the SAME NAME` |
|       - | 1299 | `	 * (not in lockstep): dynamic properties may be stored in a different order` |
|       - | 1300 | `	 * on the two instances. Counts already match, so if every left attribute has` |
|       - | 1301 | `	 * an equal-valued same-named right attribute the property sets are equal. */` |
|     145 | 1302 | `	SyHashResetLoopCursor(&pLeft->hAttr);` |
|     169 | 1303 | `	while((pEntry = SyHashGetNextEntry(&pLeft->hAttr)) != 0 ){` |
|     157 | 1304 | `		VmClassAttr *p1 = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1305 | `		VmClassAttr *p2;` |
|       - | 1306 | `		ph7_value *pL,*pR;` |
|       - | 1307 | `		/* Compare only non-static attribute */` |
|     157 | 1308 | `		if( p1->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|     ! 0 | 1309 | `			continue;` |
|       - | 1310 | `		}` |
|     157 | 1311 | `		pEntry2 = SyHashGet(&pRight->hAttr,SyStringData(&p1->pAttr->sName),SyStringLength(&p1->pAttr->sName));` |
|     157 | 1312 | `		if( pEntry2 == 0 ){` |
|       - | 1313 | `			/* Left has a property the right lacks ⇒ not equal. */` |
|     ! 0 | 1314 | `			return 1;` |
|       - | 1315 | `		}` |
|     157 | 1316 | `		p2 = (VmClassAttr *)pEntry2->pUserData;` |
|     157 | 1317 | `		pL = ExtractClassAttrValue(pLeft->pVm,p1);` |
|     157 | 1318 | `		pR = ExtractClassAttrValue(pRight->pVm,p2);` |
|     157 | 1319 | `		if( pL && pR ){` |
|     157 | 1320 | `			PH7_MemObjLoad(pL,&sV1);` |
|     157 | 1321 | `			PH7_MemObjLoad(pR,&sV2);` |
|       - | 1322 | `			/* Compare the two values now */` |
|     157 | 1323 | `			rc = PH7_MemObjCmp(&sV1,&sV2,bStrict,iNest+1);` |
|     157 | 1324 | `			PH7_MemObjRelease(&sV1);` |
|     157 | 1325 | `			PH7_MemObjRelease(&sV2);` |
|     157 | 1326 | `			if( rc != 0 ){` |
|       - | 1327 | `				/* Not equals */` |
|     133 | 1328 | `				return rc;` |
|       - | 1329 | `			}` |
|      12 | 1330 | `		}` |
|       1 | 1331 | `	}` |
|       - | 1332 | `	/* Object are equals */` |
|      13 | 1333 | `	return 0;` |
|     120 | 1334 | `}` |
|       - | 1335 | `/*` |
|       - | 1336 | ` * Dump a class instance and the store the dump in the BLOB given` |
|       - | 1337 | ` * as the first argument.` |
|       - | 1338 | ` * Note that only non-static/non-constants attribute are dumped.` |
|       - | 1339 | ` * This function is typically invoked when the user issue a call` |
|       - | 1340 | ` * to [var_dump(),var_export(),print_r(),...].` |
|       - | 1341 | ` * This function SXRET_OK on success. Any other return value including` |
|       - | 1342 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|       - | 1343 | ` */` |
|       - | 1344 | `/*` |
|       - | 1345 | ` * Emit a class-instance dump header plus its trailing newline. For var_dump` |
|       - | 1346 | ` * (ShowType) it completes the "object(" prefix the caller already emitted as` |
|       - | 1347 | ` *   ClassName)#<id> (<count>) {` |
|       - | 1348 | ` * for print_r it emits the legacy PHL  Object(ClassName) {  (count/id unused).` |
|       - | 1349 | ` */` |
|     134 | 1350 | `static void DumpClassInstanceHeader(SyBlob *pOut,ph7_class *pClass,sxu32 nObjId,int ShowType,sxu32 nCount)` |
|       3 | 1351 | `{` |
|     137 | 1352 | `	if( !ShowType ){` |
|       6 | 1353 | `		SyBlobAppend(&(*pOut),"Object(",sizeof("Object(")-1);` |
|       6 | 1354 | `		SyBlobFormat(&(*pOut),"%z) {",&pClass->sName);` |
|       4 | 1355 | `	}else{` |
|     133 | 1356 | `		SyBlobFormat(&(*pOut),"%z)#%u (%u) {",&pClass->sName,nObjId,nCount);` |
|       - | 1357 | `	}` |
|       - | 1358 | `#ifdef __WINNT__` |
|       3 | 1359 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 1360 | `#else` |
|     134 | 1361 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 1362 | `#endif` |
|     137 | 1363 | `}` |
|     138 | 1364 | `PH7_PRIVATE sxi32 PH7_ClassInstanceDump(SyBlob *pOut,ph7_class_instance *pThis,int ShowType,int nTab,int nDepth)` |
|       3 | 1365 | `{` |
|       - | 1366 | `	SyHashEntry *pEntry;` |
|       - | 1367 | `	ph7_value *pValue;` |
|       - | 1368 | `	sxi32 rc;` |
|       - | 1369 | `	int i;` |
|     141 | 1370 | `	if( nDepth > 31 ){` |
|       - | 1371 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|       - | 1372 | `		/* Nesting limit reached..halt immediately*/` |
|       5 | 1373 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|       5 | 1374 | `		if( ShowType ){` |
|       5 | 1375 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|       2 | 1376 | `		}` |
|       5 | 1377 | `		return SXERR_LIMIT;` |
|       - | 1378 | `	}` |
|     137 | 1379 | `	rc = SXRET_OK;` |
|       - | 1380 | `	{` |
|       - | 1381 | `		/* Both var_dump and print_r consult __debugInfo() (PHP behavior);` |
|       - | 1382 | `		 * var_export uses a separate renderer and never reaches here. When the` |
|       - | 1383 | `		 * method is present and returns an array, render that array's entries as` |
|       - | 1384 | `		 * the object body, with the header showing the debug array's count. The` |
|       - | 1385 | `		 * nDepth guard above protects against a __debugInfo returning the object` |
|       - | 1386 | `		 * itself. */` |
|     137 | 1387 | `		ph7_class_method *pDbg = PH7_ClassExtractMethod(pThis->pClass,"__debugInfo",sizeof("__debugInfo")-1);` |
|     137 | 1388 | `		if( pDbg ){` |
|       - | 1389 | `			ph7_value sResult;` |
|       5 | 1390 | `			PH7_MemObjInit(pThis->pVm,&sResult);` |
|       5 | 1391 | `			PH7_VmCallClassMethod(pThis->pVm,pThis,pDbg,&sResult,0,0);` |
|       5 | 1392 | `			if( sResult.iFlags & MEMOBJ_HASHMAP ){` |
|       5 | 1393 | `				ph7_hashmap *pMap = (ph7_hashmap *)sResult.x.pOther;` |
|       - | 1394 | `				/* Header count is the debug array's entry count. */` |
|       5 | 1395 | `				DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,pMap->nEntry);` |
|       5 | 1396 | `				rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth);` |
|       9 | 1397 | `				for( i = 0 ; i < nTab ; i++ ){` |
|       5 | 1398 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       3 | 1399 | `				}` |
|       5 | 1400 | `				SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|       5 | 1401 | `				PH7_MemObjRelease(&sResult);` |
|       5 | 1402 | `				return rc;` |
|       - | 1403 | `			}` |
|       - | 1404 | `			/* Non-array return: behave as if __debugInfo were absent. */` |
|     ! 0 | 1405 | `			PH7_MemObjRelease(&sResult);` |
|     ! 0 | 1406 | `		}` |
|       - | 1407 | `	}` |
|       - | 1408 | `	{` |
|       - | 1409 | `		/* var_dump's header needs the property count up front, so pre-count the` |
|       - | 1410 | `		 * non-static/non-constant attributes (matching the dump loop below). */` |
|     132 | 1411 | `		sxu32 nProp = 0;` |
|     132 | 1412 | `		if( ShowType ){` |
|     130 | 1413 | `			SyHashResetLoopCursor(&pThis->hAttr);` |
|     266 | 1414 | `			while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|     138 | 1415 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     138 | 1416 | `				if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|     134 | 1417 | `					nProp++;` |
|      66 | 1418 | `				}` |
|       2 | 1419 | `			}` |
|      64 | 1420 | `		}` |
|     132 | 1421 | `		DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,nProp);` |
|       - | 1422 | `	}` |
|       - | 1423 | `	/* Dump object attributes */` |
|     132 | 1424 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|     213 | 1425 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|     142 | 1426 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     142 | 1427 | `		if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|       - | 1428 | `			/* Dump non-static/constant attribute only */` |
|    3994 | 1429 | `			for( i = 0 ; i < nTab ; i++ ){` |
|    3858 | 1430 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|    1930 | 1431 | `			}` |
|     138 | 1432 | `			pValue = ExtractClassAttrValue(pThis->pVm,pVmAttr);` |
|     138 | 1433 | `			if( pValue ){` |
|     138 | 1434 | `				SyBlobFormat(&(*pOut),"['%z'] =>",&pVmAttr->pAttr->sName);` |
|       - | 1435 | `#ifdef __WINNT__` |
|       2 | 1436 | `				SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 1437 | `#else` |
|     136 | 1438 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 1439 | `#endif` |
|     138 | 1440 | `				rc = PH7_MemObjDump(&(*pOut),pValue,ShowType,nTab+1,nDepth,0);` |
|     138 | 1441 | `				if( rc == SXERR_LIMIT ){` |
|     125 | 1442 | `					break;` |
|       - | 1443 | `				}` |
|       6 | 1444 | `			}` |
|       6 | 1445 | `		}` |
|       2 | 1446 | `	}` |
|    3982 | 1447 | `	for( i = 0 ; i < nTab ; i++ ){` |
|    3852 | 1448 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|    1927 | 1449 | `	}` |
|     132 | 1450 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|     132 | 1451 | `	return rc;` |
|      72 | 1452 | `}` |
|       - | 1453 | `/*` |
|       - | 1454 | ` * Call a magic method [i.e: __toString(),__toBool(),__Invoke()...]` |
|       - | 1455 | ` * Return SXRET_OK on successfull call. Any other return value indicates failure.` |
|       - | 1456 | ` * Notes on magic methods.` |
|       - | 1457 | ` * According to the PHP language reference manual.` |
|       - | 1458 | ` *  The function names __construct(), __destruct(), __call(), __callStatic()` |
|       - | 1459 | ` *  __get(),  __toString(), __invoke(), __clone() are magical in PHP classes.` |
|       - | 1460 | ` * You cannot have functions with these names in any of your classes unless` |
|       - | 1461 | ` * you want the magic functionality associated with them.` |
|       - | 1462 | ` * Example of magical methods:` |
|       - | 1463 | ` * __toString()` |
|       - | 1464 | ` *  The __toString() method allows a class to decide how it will react when it is treated like` |
|       - | 1465 | ` *  a string. For example, what echo $obj; will print. This method must return a string.` |
|       - | 1466 | ` *  Example #2 Simple example` |
|       - | 1467 | ` * <?php` |
|       - | 1468 | ` * // Declare a simple class` |
|       - | 1469 | ` * class TestClass` |
|       - | 1470 | ` * {` |
|       - | 1471 | ` *   public $foo;` |
|       - | 1472 | ` *` |
|       - | 1473 | ` *   public function __construct($foo)` |
|       - | 1474 | ` *   {` |
|       - | 1475 | ` *       $this->foo = $foo;` |
|       - | 1476 | ` *   }` |
|       - | 1477 | ` *` |
|       - | 1478 | ` *   public function __toString()` |
|       - | 1479 | ` *   {` |
|       - | 1480 | ` *       return $this->foo;` |
|       - | 1481 | ` *   }` |
|       - | 1482 | ` * }` |
|       - | 1483 | ` * $class = new TestClass('Hello');` |
|       - | 1484 | ` * echo $class;` |
|       - | 1485 | ` * ?>` |
|       - | 1486 | ` * The above example will output:` |
|       - | 1487 | ` *  Hello` |
|       - | 1488 | ` *` |
|       - | 1489 | ` * Note that PH7 does not support all the magical method and introudces __toFloat(),__toInt()` |
|       - | 1490 | ` * which have the same behaviour as __toString() but for float and integer types` |
|       - | 1491 | ` * respectively.` |
|       - | 1492 | ` * Refer to the official documentation for more information.` |
|       - | 1493 | ` */` |
|      32 | 1494 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallMagicMethod(` |
|       - | 1495 | `	ph7_vm *pVm,               /* VM that own all this stuff */` |
|       - | 1496 | `	ph7_class *pClass,         /* Target class */` |
|       - | 1497 | `	ph7_class_instance *pThis, /* Target object */` |
|       - | 1498 | `	const char *zMethod,       /* Magic method name [i.e: __toString()]*/` |
|       - | 1499 | `	sxu32 nByte,               /* zMethod length*/` |
|       - | 1500 | `	const SyString *pAttrName, /* Attribute name */` |
|       - | 1501 | `	ph7_value *pResult         /* OUT: magic method return value. NULL to discard */` |
|       - | 1502 | `	)` |
|       1 | 1503 | `{` |
|      33 | 1504 | `	ph7_value *apArg[2] = { 0 , 0 };` |
|       - | 1505 | `	ph7_class_method *pMeth;` |
|       - | 1506 | `	ph7_value sAttr; /* cc warning */` |
|       - | 1507 | `	sxi32 rc;` |
|       - | 1508 | `	int nArg;` |
|       - | 1509 | `	/* Make sure the magic method is available */` |
|      33 | 1510 | `	pMeth = PH7_ClassExtractMethod(&(*pClass),zMethod,nByte);` |
|      33 | 1511 | `	if( pMeth == 0 ){` |
|       - | 1512 | `		/* No such method,return immediately */` |
|     ! 0 | 1513 | `		return SXERR_NOTFOUND;` |
|       - | 1514 | `	}` |
|      33 | 1515 | `	nArg = 0;` |
|       - | 1516 | `	/* Copy arguments */` |
|      33 | 1517 | `	if( pAttrName ){` |
|      33 | 1518 | `		PH7_MemObjInitFromString(pVm,&sAttr,pAttrName);` |
|      33 | 1519 | `		sAttr.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      33 | 1520 | `		apArg[0] = &sAttr;` |
|      33 | 1521 | `		nArg = 1;` |
|      16 | 1522 | `	}` |
|       - | 1523 | `	/* Call the magic method now */` |
|      33 | 1524 | `	rc = PH7_VmCallClassMethod(pVm,&(*pThis),pMeth,pResult,nArg,apArg);` |
|       - | 1525 | `	/* Clean up */` |
|      33 | 1526 | `	if( pAttrName ){` |
|      33 | 1527 | `		PH7_MemObjRelease(&sAttr);` |
|      16 | 1528 | `	}` |
|      33 | 1529 | `	return rc;` |
|      17 | 1530 | `}` |
|       - | 1531 | `/*` |
|       - | 1532 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon].` |
|       - | 1533 | ` * This function is simply a wrapper on ExtractClassAttrValue().` |
|       - | 1534 | ` */` |
|     126 | 1535 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceExtractAttrValue(ph7_class_instance *pThis,VmClassAttr *pAttr)` |
|       3 | 1536 | `{` |
|       - | 1537 | `   /* Extract the attribute value */` |
|       - | 1538 | `	ph7_value *pValue;` |
|     129 | 1539 | `	pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     129 | 1540 | `	return pValue;` |
|       3 | 1541 | `}` |
|       - | 1542 | `/*` |
|       - | 1543 | ` * Convert a class instance [i.e: Object in the PHP jargon] into a hashmap [i.e: array in the PHP jargon].` |
|       - | 1544 | ` * Return SXRET_OK on success. Any other value indicates failure.` |
|       - | 1545 | ` * Note on object conversion to array:` |
|       - | 1546 | ` *  Acccording to the PHP language reference manual` |
|       - | 1547 | ` *  If an object is converted to an array, the result is an array whose elements are the object's properties.` |
|       - | 1548 | ` *  The keys are the member variable names.` |
|       - | 1549 | ` *` |
|       - | 1550 | ` *  The following example:` |
|       - | 1551 | ` *  class Test {` |
|       - | 1552 | ` *   public $A = 25<<1;  // 50` |
|       - | 1553 | ` *	 public $c = rand_str(3);   // Random string of length 3` |
|       - | 1554 | ` *	 public $d = rand() & 1023; // Random number between 0..1023` |
|       - | 1555 | ` *  }` |
|       - | 1556 | ` *  var_dump((array) new Test());` |
|       - | 1557 | ` *	Will output:` |
|       - | 1558 | ` *  array(3) {` |
|       - | 1559 | ` *   [A] =>` |
|       - | 1560 | ` *      int(50)` |
|       - | 1561 | ` *   [c] =>` |
|       - | 1562 | ` *     string(3 'aps')` |
|       - | 1563 | ` *   [d] =>` |
|       - | 1564 | ` *     int(991)` |
|       - | 1565 | ` *  }` |
|       - | 1566 | ` * You have noticed that PH7 allow class attributes [i.e: $a,$c,$d in the example above]` |
|       - | 1567 | ` * have any complex expression (even function calls/Annonymous functions) as their default` |
|       - | 1568 | ` * value unlike the standard PHP engine.` |
|       - | 1569 | ` * This is a very powerful feature that you have to look at.` |
|       - | 1570 | ` */` |
|      12 | 1571 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|       1 | 1572 | `{` |
|       - | 1573 | `	SyHashEntry *pEntry;` |
|       - | 1574 | `	SyString *pAttrName;` |
|       - | 1575 | `	VmClassAttr *pAttr;` |
|       - | 1576 | `	ph7_value *pValue;` |
|       - | 1577 | `	ph7_value sName;` |
|       - | 1578 | `	/* Reset the loop cursor */` |
|      13 | 1579 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      13 | 1580 | `	PH7_MemObjInitFromString(pThis->pVm,&sName,0);` |
|      47 | 1581 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       - | 1582 | `		/* Point to the current attribute */` |
|      29 | 1583 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1584 | `		/* Extract attribute value */` |
|      29 | 1585 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|      29 | 1586 | `		if( pValue ){` |
|       - | 1587 | `			/* Build attribute name */` |
|      29 | 1588 | `			pAttrName = &pAttr->pAttr->sName;` |
|      29 | 1589 | `			PH7_MemObjStringAppend(&sName,pAttrName->zString,pAttrName->nByte);` |
|       - | 1590 | `			/* Perform the insertion */` |
|      29 | 1591 | `			PH7_HashmapInsert(pMap,&sName,pValue);` |
|       - | 1592 | `			/* Reset the string cursor */` |
|      29 | 1593 | `			SyBlobReset(&sName.sBlob);` |
|      14 | 1594 | `		}` |
|       1 | 1595 | `	}` |
|      13 | 1596 | `	PH7_MemObjRelease(&sName);` |
|      13 | 1597 | `	return SXRET_OK;` |
|       1 | 1598 | `}` |
|       - | 1599 | `/*` |
|       - | 1600 | ` * Iterate throw class attributes and invoke the given callback [i.e: xWalk()] for each` |
|       - | 1601 | ` * retrieved attribute.` |
|       - | 1602 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|       - | 1603 | ` * the attribute value in the callback body will not alter the real attribute value.` |
|       - | 1604 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|       - | 1605 | ` * a value different from PH7_OK.` |
|       - | 1606 | ` * Refer to [ph7_object_walk()] for more information.` |
|       - | 1607 | ` */` |
|      40 | 1608 | `PH7_PRIVATE sxi32 PH7_ClassInstanceWalk(` |
|       - | 1609 | `	ph7_class_instance *pThis, /* Target object */` |
|       - | 1610 | `	int (*xWalk)(const char *,ph7_value *,void *), /* Walker callback */` |
|       - | 1611 | `	void *pUserData /* Last argument to xWalk() */` |
|       - | 1612 | `	)` |
|       2 | 1613 | `{` |
|       - | 1614 | `	SyHashEntry *pEntry; /* Hash entry */` |
|       - | 1615 | `	VmClassAttr *pAttr;  /* Pointer to the attribute */` |
|       - | 1616 | `	ph7_value *pValue;   /* Attribute value */` |
|       - | 1617 | `	ph7_value sValue;    /* Copy of the attribute value */` |
|       - | 1618 | `	int rc;` |
|       - | 1619 | `	/* Reset the loop cursor */` |
|      42 | 1620 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      42 | 1621 | `	PH7_MemObjInit(pThis->pVm,&sValue);` |
|       - | 1622 | `	/* Start the walk process */` |
|     124 | 1623 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       - | 1624 | `		/* Point to the current attribute */` |
|      64 | 1625 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1626 | `		/* Extract attribute value */` |
|      64 | 1627 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|      64 | 1628 | `		if( pValue ){` |
|      64 | 1629 | `			PH7_MemObjLoad(pValue,&sValue);` |
|       - | 1630 | `			/* Invoke the supplied callback */` |
|      64 | 1631 | `			rc =  xWalk(SyStringData(&pAttr->pAttr->sName),&sValue,pUserData);` |
|      64 | 1632 | `			PH7_MemObjRelease(&sValue);` |
|      64 | 1633 | `			if( rc != PH7_OK){` |
|       - | 1634 | `				/* User callback request an operation abort */` |
|     ! 0 | 1635 | `				return SXERR_ABORT;` |
|       - | 1636 | `			}` |
|      31 | 1637 | `		}` |
|       2 | 1638 | `	}` |
|       - | 1639 | `	/* All done */` |
|      42 | 1640 | `	return SXRET_OK;` |
|      22 | 1641 | `}` |
|       - | 1642 | `/*` |
|       - | 1643 | ` * Extract a class atrribute value.` |
|       - | 1644 | ` * Return a pointer to the attribute value on success. Otherwise NULL.` |
|       - | 1645 | ` * Note:` |
|       - | 1646 | ` *  Access to static and constant attribute is not allowed. That is,the function` |
|       - | 1647 | ` *  will return NULL in case someone (host-application code) try to extract` |
|       - | 1648 | ` *  a static/constant attribute.` |
|       - | 1649 | ` */` |
|    6886 | 1650 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceFetchAttr(ph7_class_instance *pThis,const SyString *pName)` |
|       5 | 1651 | `{` |
|       - | 1652 | `	SyHashEntry *pEntry;` |
|       - | 1653 | `	VmClassAttr *pAttr;` |
|       - | 1654 | `	/* Query the attribute hashtable */` |
|    6891 | 1655 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);` |
|    6891 | 1656 | `	if( pEntry == 0 ){` |
|       - | 1657 | `		/* No such attribute */` |
|     ! 0 | 1658 | `		return 0;` |
|       - | 1659 | `	}` |
|       - | 1660 | `	/* Point to the class atrribute */` |
|    6891 | 1661 | `	pAttr = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1662 | `	/* Check if we are dealing with a static/constant attribute */` |
|    6891 | 1663 | `	if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|       - | 1664 | `		/* Access is forbidden */` |
|     ! 0 | 1665 | `		return 0;` |
|       - | 1666 | `	}` |
|       - | 1667 | `	/* Return the attribute value */` |
|    6891 | 1668 | `	return ExtractClassAttrValue(pThis->pVm,pAttr);` |
|    3448 | 1669 | `}` |
|       - | 1670 |  |
