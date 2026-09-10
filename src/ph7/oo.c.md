# src/ph7/oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 696/790 lines (88.10%)

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
|  429402 |   14 | `PH7_PRIVATE ph7_class * PH7_NewRawClass(ph7_vm *pVm,const SyString *pName,sxu32 nLine)` |
|       5 |   15 | `{` |
|       - |   16 | `	ph7_class *pClass;` |
|       - |   17 | `	char *zName;` |
|       - |   18 | `	/* Allocate a new instance */` |
|  429407 |   19 | `	pClass = (ph7_class *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class));` |
|  429407 |   20 | `	if( pClass == 0 ){` |
|     ! 0 |   21 | `		return 0;` |
|       - |   22 | `	}` |
|       - |   23 | `	/* Zero the structure */` |
|  429407 |   24 | `	SyZero(pClass,sizeof(ph7_class));` |
|       - |   25 | `	/* Duplicate class name */` |
|  429407 |   26 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  429407 |   27 | `	if( zName == 0 ){` |
|     ! 0 |   28 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClass);` |
|     ! 0 |   29 | `		return 0;` |
|       - |   30 | `	}` |
|       - |   31 | `	/* Initialize fields */` |
|  429407 |   32 | `	SyStringInitFromBuf(&pClass->sName,zName,pName->nByte);` |
|  429407 |   33 | `	SyHashInit(&pClass->hMethod,&pVm->sAllocator,0,0);` |
|  429407 |   34 | `	SyHashInit(&pClass->hAttr,&pVm->sAllocator,0,0);` |
|  429407 |   35 | `	SyHashInit(&pClass->hDerived,&pVm->sAllocator,0,0);` |
|  429407 |   36 | `	SySetInit(&pClass->aInterface,&pVm->sAllocator,sizeof(ph7_class *));` |
|  429407 |   37 | `	SySetInit(&pClass->aTrait,&pVm->sAllocator,sizeof(ph7_class *));` |
|  429407 |   38 | `	SySetInit(&pClass->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|  429407 |   39 | `	SySetInit(&pClass->aEnumCases,&pVm->sAllocator,sizeof(ph7_class_attr *));` |
|  429407 |   40 | `	pClass->nLine = nLine;` |
|  429407 |   41 | `	if( pVm->bCompilingBuiltin ){` |
|       - |   42 | `		/* Defined by an embedded builtin chunk: internal, no defining file.` |
|       - |   43 | `		 * Class compilers merge further flags with \|= so this survives. */` |
|  427397 |   44 | `		pClass->iFlags \|= PH7_CLASS_INTERNAL;` |
|  213701 |   45 | `	}else{` |
|       - |   46 | `		/* Alias the VM-lifetime path dup on top of the include stack */` |
|    2015 |   47 | `		SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|    2015 |   48 | `		if( pFile ){` |
|    2015 |   49 | `			SyStringDupPtr(&pClass->sFile,pFile);` |
|    1005 |   50 | `		}` |
|       - |   51 | `	}` |
|       - |   52 | `	/* All done */` |
|  429407 |   53 | `	return pClass;` |
|  214706 |   54 | `}` |
|       - |   55 | `/*` |
|       - |   56 | ` * Allocate and initialize a new class attribute.` |
|       - |   57 | ` * Return a pointer to the class attribute on success. NULL otherwise.` |
|       - |   58 | ` */` |
|  764518 |   59 | `PH7_PRIVATE ph7_class_attr * PH7_NewClassAttr(ph7_vm *pVm,const SyString *pName,sxu32 nLine,sxi32 iProtection,sxi32 iFlags)` |
|       5 |   60 | `{` |
|       - |   61 | `	ph7_class_attr *pAttr;` |
|       - |   62 | `	char *zName;` |
|  764523 |   63 | `	pAttr = (ph7_class_attr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_attr));` |
|  764523 |   64 | `	if( pAttr == 0 ){` |
|     ! 0 |   65 | `		return 0;` |
|       - |   66 | `	}` |
|       - |   67 | `	/* Zero the structure */` |
|  764523 |   68 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|  764523 |   69 | `	SySetInit(&pAttr->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|       - |   70 | `	/* Duplicate attribute name */` |
|  764523 |   71 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  764523 |   72 | `	if( zName == 0 ){` |
|     ! 0 |   73 | `		SyMemBackendPoolFree(&pVm->sAllocator,pAttr);` |
|     ! 0 |   74 | `		return 0;` |
|       - |   75 | `	}` |
|       - |   76 | `	/* Initialize fields */` |
|  764523 |   77 | `	SySetInit(&pAttr->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|  764523 |   78 | `	SySetInit(&pAttr->aUnionAlts,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|  764523 |   79 | `	SyStringInitFromBuf(&pAttr->sName,zName,pName->nByte);` |
|  764523 |   80 | `	pAttr->iProtection = iProtection;` |
|  764523 |   81 | `	pAttr->nIdx = SXU32_HIGH;` |
|  764523 |   82 | `	pAttr->iFlags = iFlags;` |
|  764523 |   83 | `	pAttr->nLine = nLine;` |
|  764523 |   84 | `	return pAttr;` |
|  382264 |   85 | `}` |
|       - |   86 | `/*` |
|       - |   87 | ` * Allocate and initialize a new class method.` |
|       - |   88 | ` * Return a pointer to the class method on success. NULL otherwise` |
|       - |   89 | ` * This function associate with the newly created method an automatically generated` |
|       - |   90 | ` * random unique name.` |
|       - |   91 | ` */` |
| 2394440 |   92 | `PH7_PRIVATE ph7_class_method * PH7_NewClassMethod(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,sxu32 nLine,` |
|       - |   93 | `	sxi32 iProtection,sxi32 iFlags,sxi32 iFuncFlags)` |
|       5 |   94 | `{` |
|       - |   95 | `	ph7_class_method *pMeth;` |
|       - |   96 | `	SyHashEntry *pEntry;` |
|       - |   97 | `	SyString *pNamePtr;` |
|       - |   98 | `	char zSalt[10];` |
|       - |   99 | `	char *zName;` |
|       - |  100 | `	sxu32 nByte;` |
|       - |  101 | `	/* Allocate a new class method instance */` |
| 2394445 |  102 | `	pMeth = (ph7_class_method *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_method));` |
| 2394445 |  103 | `	if( pMeth == 0 ){` |
|     ! 0 |  104 | `		return 0;` |
|       - |  105 | `	}` |
|       - |  106 | `	/* Zero the structure */` |
| 2394445 |  107 | `	SyZero(pMeth,sizeof(ph7_class_method));` |
|       - |  108 | `	/* Check for an already installed method with the same name */` |
| 2394445 |  109 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
| 2394445 |  110 | `	if( pEntry == 0 ){` |
|       - |  111 | `		/* Associate an unique VM name to this method */` |
| 2394443 |  112 | `		nByte = sizeof(zSalt) + pName->nByte + SyStringLength(&pClass->sName)+sizeof(char)*7/*[[__'\0'*/;` |
| 2394443 |  113 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,nByte);` |
| 2394443 |  114 | `		if( zName == 0 ){` |
|     ! 0 |  115 | `			SyMemBackendPoolFree(&pVm->sAllocator,pMeth);` |
|     ! 0 |  116 | `			return 0;` |
|       - |  117 | `		}` |
| 2394443 |  118 | `		pNamePtr = &pMeth->sVmName;` |
|       - |  119 | `		/* Generate a random string */` |
| 2394443 |  120 | `		PH7_VmRandomString(&(*pVm),zSalt,sizeof(zSalt));` |
| 2394443 |  121 | `		pNamePtr->nByte = SyBufferFormat(zName,nByte,"[__%z@%z_%.*s]",&pClass->sName,pName,sizeof(zSalt),zSalt);` |
| 2394443 |  122 | `		pNamePtr->zString = zName;` |
| 1197224 |  123 | `	}else{` |
|       - |  124 | `		/* Method is condidate for 'overloading' */` |
|       3 |  125 | `		ph7_class_method *pCurrent = (ph7_class_method *)pEntry->pUserData;` |
|       3 |  126 | `		pNamePtr = &pMeth->sVmName;` |
|       - |  127 | `		/* Use the same VM name */` |
|       3 |  128 | `		SyStringDupPtr(pNamePtr,&pCurrent->sVmName);` |
|       3 |  129 | `		zName = (char *)pNamePtr->zString;` |
|       - |  130 | `	}` |
| 2394445 |  131 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|  145070 |  132 | `		if( (pName->nByte == sizeof("__destruct") - 1 && SyMemcmp(pName->zString,"__destruct",sizeof("__destruct") - 1 ) == 0)` |
|  145075 |  133 | `			\|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|       - |  134 | `				/* Switch to public visibility for destructors and legacy class-name` |
|       - |  135 | `				 * constructors (the engine invokes destructors internally, bypassing` |
|       - |  136 | `				 * visibility either way). __construct KEEPS its declared visibility` |
|       - |  137 | ``				 * (band A #4): php enforces it at `new` — a private/protected ctor`` |
|       - |  138 | `				 * from the wrong scope is a catchable Error, checked at OP_NEW —` |
|       - |  139 | `				 * and ReflectionClass::isInstantiable()/newInstance() now see it. */` |
|     ! 0 |  140 | `				iProtection = PH7_CLASS_PROT_PUBLIC;` |
|     ! 0 |  141 | `		}` |
|   72535 |  142 | `	}` |
|       - |  143 | `	/* Initialize method fields */` |
| 2394445 |  144 | `	pMeth->iProtection = iProtection;` |
| 2394445 |  145 | `	pMeth->iFlags = iFlags;` |
| 2394445 |  146 | `	pMeth->nLine = nLine;` |
| 3591665 |  147 | `	PH7_VmInitFuncState(&(*pVm),&pMeth->sFunc,&zName[sizeof(char)*4/*[__@*/+SyStringLength(&pClass->sName)],` |
| 2394440 |  148 | `		pName->nByte,iFuncFlags\|VM_FUNC_CLASS_METHOD,pClass);` |
| 2394445 |  149 | `	return pMeth;` |
| 1197225 |  150 | `}` |
|       - |  151 | `/*` |
|       - |  152 | ` * Check if the given name have a class method associated with it.` |
|       - |  153 | ` * Return the desired method [i.e: ph7_class_method instance] on success. NULL otherwise.` |
|       - |  154 | ` */` |
|  723138 |  155 | `PH7_PRIVATE ph7_class_method * PH7_ClassExtractMethod(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|       5 |  156 | `{` |
|       - |  157 | `	SyHashEntry *pEntry;` |
|       - |  158 | `	/* Perform a hash lookup */` |
|  723143 |  159 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)zName,nByte);` |
|  723143 |  160 | `	if( pEntry == 0 ){` |
|       - |  161 | `		/* No such entry */` |
|    9093 |  162 | `		return 0;` |
|       - |  163 | `	}` |
|       - |  164 | `	/* Point to the desired method */` |
|  714055 |  165 | `	return (ph7_class_method *)pEntry->pUserData;` |
|  361574 |  166 | `}` |
|       - |  167 | `/*` |
|       - |  168 | ` * Check if the given name is a class attribute.` |
|       - |  169 | ` * Return the desired attribute [i.e: ph7_class_attr instance] on success.NULL otherwise.` |
|       - |  170 | ` */` |
|  460378 |  171 | `PH7_PRIVATE ph7_class_attr * PH7_ClassExtractAttribute(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|       5 |  172 | `{` |
|       - |  173 | `	SyHashEntry *pEntry;` |
|       - |  174 | `	/* Perform a hash lookup */` |
|  460383 |  175 | `	pEntry = SyHashGet(&pClass->hAttr,(const void *)zName,nByte);` |
|  460383 |  176 | `	if( pEntry == 0 ){` |
|       - |  177 | `		/* No such entry */` |
|  459081 |  178 | `		return 0;` |
|       - |  179 | `	}` |
|       - |  180 | `	/* Point to the desierd method */` |
|    1307 |  181 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|  230194 |  182 | `}` |
|       - |  183 | `/*` |
|       - |  184 | ` * Install a class attribute in the corresponding container.` |
|       - |  185 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|       - |  186 | ` */` |
|  764514 |  187 | `PH7_PRIVATE sxi32 PH7_ClassInstallAttr(ph7_class *pClass,ph7_class_attr *pAttr)` |
|       5 |  188 | `{` |
|  764519 |  189 | `	SyString *pName = &pAttr->sName;` |
|       - |  190 | `	sxi32 rc;` |
|       - |  191 | `	/* Remember where this attribute was originally declared so that later` |
|       - |  192 | `	 * inheritance/trait copies still know the declaring class (needed for` |
|       - |  193 | `	 * PHP-compatible error messages on typed properties). */` |
|  764519 |  194 | `	if( pAttr->pDeclClass == 0 ){` |
|  764519 |  195 | `		pAttr->pDeclClass = pClass;` |
|  382257 |  196 | `	}` |
|  764519 |  197 | `	rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|  764519 |  198 | `	return rc;` |
|       5 |  199 | `}` |
|       - |  200 | `/*` |
|       - |  201 | ` * Install a class method in the corresponding container.` |
|       - |  202 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|       - |  203 | ` */` |
| 2394428 |  204 | `PH7_PRIVATE sxi32 PH7_ClassInstallMethod(ph7_class *pClass,ph7_class_method *pMeth)` |
|       5 |  205 | `{` |
| 2394433 |  206 | `	SyString *pName = &pMeth->sFunc.sName;` |
|       - |  207 | `	sxi32 rc;` |
| 2394433 |  208 | `	rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 2394433 |  209 | `	return rc;` |
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
|  152848 |  234 | `static int OoClassifyOverrideType(ph7_vm *pVm, sxu32 nType, const SyString *pClass,` |
|       - |  235 | `	int bUnion, ph7_class **ppClass)` |
|       5 |  236 | `{` |
|  152853 |  237 | `	*ppClass = 0;` |
|  152853 |  238 | `	if( bUnion ){` |
|       3 |  239 | `		return OVT_SKIP; /* union/intersection — full lattice, skip */` |
|       - |  240 | `	}` |
|  152851 |  241 | `	if( nType == 0 ){` |
|  152757 |  242 | `		return OVT_NONE; /* no declared type */` |
|       - |  243 | `	}` |
|      99 |  244 | `	if( nType == SXU32_HIGH ){` |
|       - |  245 | `		/* A class name OR a pseudo-type stored as a name atom. Skip every pseudo` |
|       - |  246 | `		 * (incl. self/parent/static, which are context-relative). */` |
|       - |  247 | `		static const struct { const char *z; sxu32 n; } aPseudo[] = {` |
|       - |  248 | `			{"mixed",5}, {"never",5}, {"iterable",8}, {"callable",8}, {"true",4},` |
|       - |  249 | `			{"false",5}, {"self",4}, {"parent",6}, {"static",6}` |
|       - |  250 | `		};` |
|      18 |  251 | `		const char *z = pClass->zString;` |
|      18 |  252 | `		sxu32 n = pClass->nByte;` |
|       - |  253 | `		SyHashEntry *pE;` |
|       - |  254 | `		sxu32 i;` |
|     154 |  255 | `		for( i = 0; i < SX_ARRAYSIZE(aPseudo); i++ ){` |
|     142 |  256 | `			if( n == aPseudo[i].n && SyStrnmicmp(z,aPseudo[i].z,n) == 0 ){` |
|       5 |  257 | `				return OVT_SKIP;` |
|       - |  258 | `			}` |
|      70 |  259 | `		}` |
|      14 |  260 | `		pE = SyHashGet(&pVm->hClass,(const void *)z,n);` |
|      14 |  261 | `		if( pE == 0 ){` |
|     ! 0 |  262 | `			return OVT_SKIP; /* not loaded / forward ref / namespaced — accept */` |
|       - |  263 | `		}` |
|      14 |  264 | `		*ppClass = (ph7_class *)pE->pUserData;` |
|      14 |  265 | `		return OVT_CLASS;` |
|       - |  266 | `	}` |
|      78 |  267 | `	if( nType == MEMOBJ_STRING \|\| nType == MEMOBJ_INT \|\| nType == MEMOBJ_REAL` |
|      29 |  268 | `	 \|\| nType == MEMOBJ_BOOL \|\| nType == MEMOBJ_HASHMAP ){` |
|      58 |  269 | `		return OVT_SCALAR;` |
|       - |  270 | `	}` |
|       - |  271 | `	/* MEMOBJ_OBJ (object — subtypes against classes), MEMOBJ_VOID/NULL/RES,` |
|       - |  272 | `	 * or anything unexpected: skip. */` |
|      26 |  273 | `	return OVT_SKIP;` |
|   76429 |  274 | `}` |
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
|  122260 |  288 | `static OvType OoTypeFromReturn(ph7_vm_func *pF)` |
|       5 |  289 | `{` |
|       - |  290 | `	OvType t;` |
|  122265 |  291 | `	t.nType = pF->nReturnType;` |
|  122265 |  292 | `	t.pClass = &pF->sReturnClass;` |
|  122265 |  293 | `	t.bUnion = SySetUsed(&pF->aReturnUnion) > 0;` |
|  122265 |  294 | `	t.bNullable = (pF->iFlags & VM_FUNC_RETURN_NULLABLE) != 0;` |
|  122265 |  295 | `	return t;` |
|       5 |  296 | `}` |
|   30588 |  297 | `static OvType OoTypeFromArg(ph7_vm_func_arg *pA)` |
|       5 |  298 | `{` |
|       - |  299 | `	OvType t;` |
|   30593 |  300 | `	t.nType = pA->nType;` |
|   30593 |  301 | `	t.pClass = &pA->sClass;` |
|   30593 |  302 | `	t.bUnion = (pA->iFlags & VM_FUNC_ARG_UNION) != 0;` |
|   30593 |  303 | `	t.bNullable = (pA->iFlags & VM_FUNC_ARG_NULLABLE) != 0;` |
|   30593 |  304 | `	return t;` |
|       5 |  305 | `}` |
|       - |  306 | `/*` |
|       - |  307 | ` * Return TRUE if the child type is an unambiguously-invalid override of the` |
|       - |  308 | ` * parent type. bCovariant=1 for a return type (child must be ⊆ parent),` |
|       - |  309 | ` * 0 for a parameter (child must be ⊇ parent). Returns FALSE (accept) on any` |
|       - |  310 | ` * skipped/ambiguous shape.` |
|       - |  311 | ` */` |
|   76424 |  312 | `static int OoOverrideTypeBad(ph7_vm *pVm, OvType parent, OvType child, int bCovariant)` |
|       5 |  313 | `{` |
|       - |  314 | `	ph7_class *pParentCls, *pChildCls;` |
|   76429 |  315 | `	int kP = OoClassifyOverrideType(pVm, parent.nType, parent.pClass, parent.bUnion, &pParentCls);` |
|   76429 |  316 | `	int kC = OoClassifyOverrideType(pVm, child.nType, child.pClass, child.bUnion, &pChildCls);` |
|   76429 |  317 | `	if( kP == OVT_SKIP \|\| kC == OVT_SKIP ){` |
|      18 |  318 | `		return 0; /* ambiguous shape — conservatively accept */` |
|       - |  319 | `	}` |
|       - |  320 | `	/* A missing type is the TOP type. covariant (return): a concrete child is a` |
|       - |  321 | `	 * subtype of top, fine; a top child over a concrete parent WIDENS → bad.` |
|       - |  322 | `	 * contravariant (param): a top child is a supertype of anything, fine; a` |
|       - |  323 | `	 * concrete child over a top parent NARROWS → bad. (A union/intersection child` |
|       - |  324 | `	 * already fell into OVT_SKIP above, so a flagged child here is scalar/class.) */` |
|   76413 |  325 | `	if( kP == OVT_NONE \|\| kC == OVT_NONE ){` |
|   76385 |  326 | `		if( bCovariant && kC == OVT_NONE && kP != OVT_NONE ) return 1;` |
|   76385 |  327 | `		if( !bCovariant && kP == OVT_NONE && kC != OVT_NONE ) return 1;` |
|   76385 |  328 | `		return 0;` |
|       - |  329 | `	}` |
|       - |  330 | `	/* Nullability: a covariant return may not ADD null; a contravariant param may` |
|       - |  331 | `	 * not REMOVE null. */` |
|      33 |  332 | `	if( bCovariant ){` |
|      16 |  333 | `		if( child.bNullable && !parent.bNullable ) return 1;` |
|      10 |  334 | `	}else{` |
|      19 |  335 | `		if( parent.bNullable && !child.bNullable ) return 1;` |
|       - |  336 | `	}` |
|      33 |  337 | `	if( kP == OVT_SCALAR && kC == OVT_SCALAR ){` |
|       - |  338 | `		/* Scalars are invariant — they must match exactly. */` |
|      26 |  339 | `		return (parent.nType != child.nType) ? 1 : 0;` |
|       - |  340 | `	}` |
|       8 |  341 | `	if( kP == OVT_CLASS && kC == OVT_CLASS ){` |
|       8 |  342 | `		if( bCovariant ){` |
|       3 |  343 | `			return PH7_VmInstanceOf(pChildCls, pParentCls) ? 0 : 1;  /* child ⊆ parent */` |
|       - |  344 | `		}` |
|       6 |  345 | `		return PH7_VmInstanceOf(pParentCls, pChildCls) ? 0 : 1;      /* child ⊇ parent */` |
|       - |  346 | `	}` |
|       - |  347 | `	/* One scalar and one class — disjoint. */` |
|     ! 0 |  348 | `	return 1;` |
|   38217 |  349 | `}` |
|       - |  350 |  |
|       - |  351 | `/*` |
|       - |  352 | ` * Check a child method's signature against the parent method it overrides.` |
|       - |  353 | ` * Emits a PHP-style "Declaration of … must be compatible …" fatal on a clear` |
|       - |  354 | `` * incompatibility. `__construct` is exempt (PHP does not apply variance to it).`` |
|       - |  355 | ` */` |
|   87850 |  356 | `static sxi32 OoCheckOverrideCompat(ph7_gen_state *pGen, ph7_class *pBase, ph7_class *pSub,` |
|       - |  357 | `	ph7_class_method *pParent, ph7_class_method *pChild)` |
|       5 |  358 | `{` |
|   87855 |  359 | `	ph7_vm *pVm = pGen->pVm;` |
|   87855 |  360 | `	ph7_vm_func *pPF = &pParent->sFunc;` |
|   87855 |  361 | `	ph7_vm_func *pCF = &pChild->sFunc;` |
|   87855 |  362 | `	SyString *pMName = &pCF->sName;` |
|       - |  363 | `	ph7_vm_func_arg *aP, *aC;` |
|       - |  364 | `	sxu32 nPArg, nCArg, k;` |
|   87855 |  365 | `	int bBad = 0;` |
|   87850 |  366 | `	if( pMName->nByte == sizeof("__construct")-1` |
|   57294 |  367 | `	 && SyStrnmicmp(pMName->zString,"__construct",pMName->nByte) == 0 ){` |
|   26725 |  368 | `		return SXRET_OK;` |
|       - |  369 | `	}` |
|       - |  370 | `	/* Return type — covariant. */` |
|   61135 |  371 | `	bBad = OoOverrideTypeBad(pVm, OoTypeFromReturn(pPF), OoTypeFromReturn(pCF), /* bCovariant */ 1);` |
|       - |  372 | `	/* Each overlapping parameter — contravariant. */` |
|   61135 |  373 | `	nPArg = SySetUsed(&pPF->aArgs);` |
|   61135 |  374 | `	nCArg = SySetUsed(&pCF->aArgs);` |
|   61135 |  375 | `	aP = (ph7_vm_func_arg *)SySetBasePtr(&pPF->aArgs);` |
|   61135 |  376 | `	aC = (ph7_vm_func_arg *)SySetBasePtr(&pCF->aArgs);` |
|   76429 |  377 | `	for( k = 0; !bBad && k < nPArg && k < nCArg; k++ ){` |
|   15299 |  378 | `		bBad = OoOverrideTypeBad(pVm, OoTypeFromArg(&aP[k]), OoTypeFromArg(&aC[k]), /* bCovariant */ 0);` |
|    7652 |  379 | `	}` |
|       - |  380 | `	/* Parameter arity: the child must declare at least the parent's parameters and` |
|       - |  381 | `	 * may add only OPTIONAL ones — PHP rejects dropping any param (even an optional` |
|       - |  382 | `	 * one) or adding a required one. Skip the rule if either signature is variadic` |
|       - |  383 | `	 * (arity semantics differ). */` |
|   61135 |  384 | `	if( !bBad ){` |
|   61131 |  385 | `		int bVariadic = 0;` |
|   76423 |  386 | `		for( k = 0; k < nPArg; k++ ){ if( aP[k].iFlags & VM_FUNC_ARG_VARIADIC ) bVariadic = 1; }` |
|   76425 |  387 | `		for( k = 0; k < nCArg; k++ ){ if( aC[k].iFlags & VM_FUNC_ARG_VARIADIC ) bVariadic = 1; }` |
|   61131 |  388 | `		if( !bVariadic ){` |
|   61131 |  389 | `			if( nCArg < nPArg ){` |
|     ! 0 |  390 | `				bBad = 1; /* dropped a parent parameter */` |
|     ! 0 |  391 | `			}else{` |
|   61133 |  392 | `				for( k = nPArg; k < nCArg; k++ ){` |
|       3 |  393 | `					if( SySetUsed(&aC[k].aByteCode) == 0 ){ bBad = 1; break; } /* new required */` |
|       2 |  394 | `				}` |
|       - |  395 | `			}` |
|   30563 |  396 | `		}` |
|   30563 |  397 | `	}` |
|   61135 |  398 | `	if( bBad ){` |
|       8 |  399 | `		sxi32 rc = PH7_GenCompileError(&(*pGen),E_ERROR,pChild->nLine,` |
|       - |  400 | `			"Declaration of %z::%z() must be compatible with %z::%z()",` |
|       2 |  401 | `			&pSub->sName,pMName,&pBase->sName,&pParent->sFunc.sName);` |
|       6 |  402 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  403 | `			return SXERR_ABORT;` |
|       - |  404 | `		}` |
|       2 |  405 | `	}` |
|   61135 |  406 | `	return SXRET_OK;` |
|   43930 |  407 | `}` |
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
|  183432 |  449 | `PH7_PRIVATE sxi32 PH7_ClassInherit(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase)` |
|       5 |  450 | `{` |
|       - |  451 | `	ph7_class_method *pMeth;` |
|       - |  452 | `	ph7_class_attr *pAttr;` |
|       - |  453 | `	SyHashEntry *pEntry;` |
|       - |  454 | `	SyString *pName;` |
|       - |  455 | `	sxi32 rc;` |
|       - |  456 | `	/* Install in the derived hashtable */` |
|  183437 |  457 | `	rc = SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|  183437 |  458 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  459 | `		return rc;` |
|       - |  460 | `	}` |
|       - |  461 | `	/* readonly class inheritance (PHP 8.2): a readonly class may only extend a` |
|       - |  462 | `	 * readonly class, and a non-readonly class may not extend a readonly one. */` |
|  183437 |  463 | `	if( (pBase->iFlags & PH7_CLASS_READONLY) != (pSub->iFlags & PH7_CLASS_READONLY) ){` |
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
|  183437 |  478 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
| 1069257 |  479 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|       - |  480 | `		/* Make sure the private attributes are not redeclared in the subclass */` |
|  885825 |  481 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  885825 |  482 | `		pName = &pAttr->sName;` |
|  885825 |  483 | `		if( (pEntry = SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|      16 |  484 | `			if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_FINAL))` |
|      11 |  485 | `				== (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_FINAL) ){` |
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
|      19 |  497 | `			if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE &&` |
|       2 |  498 | `				((ph7_class_attr *)pEntry->pUserData)->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       - |  499 | `					/* Cannot redeclare private attribute */` |
|       4 |  500 | `					PH7_GenCompileError(&(*pGen),E_WARNING,((ph7_class_attr *)pEntry->pUserData)->nLine,` |
|       - |  501 | `						"Private attribute '%z::%z' redeclared inside child class '%z'",` |
|       1 |  502 | `						&pBase->sName,pName,&pSub->sName);` |
|       - |  503 |  |
|       1 |  504 | `			}` |
|      19 |  505 | `			continue;` |
|       - |  506 | `		}` |
|       - |  507 | `		/* Install the attribute. php: a base class's private INSTANCE property` |
|       - |  508 | `		 * lives on every child instance too (its own methods read/write it` |
|       - |  509 | `		 * through $this on the child; the access check grants private access by` |
|       - |  510 | `		 * DECLARING class, so child methods and outsiders still can't touch it).` |
|       - |  511 | `		 * Private STATICS/CONSTANTS stay uncopied — base methods reach those` |
|       - |  512 | `		 * through self:: against the declaring class directly. */` |
|  885804 |  513 | `		if( pAttr->iProtection != PH7_CLASS_PROT_PRIVATE` |
|  473445 |  514 | `		 \|\| (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|  885803 |  515 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|  885803 |  516 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  517 | `				return rc;` |
|       - |  518 | `			}` |
|  442899 |  519 | `		}` |
|       5 |  520 | `	}` |
|  183437 |  521 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
| 2626611 |  522 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|       - |  523 | `		/* Make sure the private/final methods are not redeclared in the subclass */` |
| 2443179 |  524 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
| 2443179 |  525 | `		pName = &pMeth->sFunc.sName;` |
| 2443179 |  526 | `		if( (pEntry = SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|   87859 |  527 | `			 if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|       - |  528 | `				/* php: "Cannot override final method A::test()" */` |
|       7 |  529 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_method *)pEntry->pUserData)->nLine,` |
|       - |  530 | `					"Cannot override final method %z::%z()",` |
|       2 |  531 | `					&pBase->sName,pName);` |
|       2 |  532 | `				(void)pSub;` |
|       5 |  533 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  534 | `					return SXERR_ABORT;` |
|       - |  535 | `				}` |
|       3 |  536 | `			}else{` |
|       - |  537 | `				/* Check the override's signature is compatible with the parent's. */` |
|  131780 |  538 | `				rc = OoCheckOverrideCompat(&(*pGen),pBase,pSub,pMeth,` |
|   87850 |  539 | `					(ph7_class_method *)pEntry->pUserData);` |
|   87855 |  540 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  541 | `					return SXERR_ABORT;` |
|       - |  542 | `				}` |
|       - |  543 | `			}` |
|   87859 |  544 | `			continue;` |
|       - |  545 | `		}` |
|       - |  546 | `		/* Install the method. php: a base class's private INSTANCE method is` |
|       - |  547 | `		 * dispatchable on child instances too — an inherited public method` |
|       - |  548 | `		 * calling $this->priv() must find it (the call-site visibility check` |
|       - |  549 | `		 * binds by DECLARING class, sFunc.pUserData, so child code and` |
|       - |  550 | `		 * outsiders still can't call it; a private ctor copied down also` |
|       - |  551 | ``		 * blocks `new Child` from outside like php). Private STATICS stay`` |
|       - |  552 | `		 * uncopied — base methods reach those through self:: against the` |
|       - |  553 | `		 * declaring class directly. */` |
| 2355320 |  554 | `		if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE` |
| 1191029 |  555 | `		 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
| 2355325 |  556 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 2355325 |  557 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  558 | `				return rc;` |
|       - |  559 | `			}` |
| 1177660 |  560 | `		}` |
|       5 |  561 | `	}` |
|       - |  562 | `	/* Mark as subclass */` |
|  183437 |  563 | `	pSub->pBase = pBase;` |
|       - |  564 | `	/* All done */` |
|  183437 |  565 | `	return SXRET_OK;` |
|   91721 |  566 | `}` |
|       - |  567 | `/*` |
|       - |  568 | ` * Apply a trait to a class: copy all methods and attributes from the trait` |
|       - |  569 | ` * into the target class. Unlike inheritance, traits copy ALL members including` |
|       - |  570 | ` * private ones. Members already defined in the class take precedence.` |
|       - |  571 | ` */` |
|   15326 |  572 | `PH7_PRIVATE sxi32 PH7_ClassUseTrait(ph7_gen_state *pGen,ph7_class *pClass,ph7_class *pTrait)` |
|       5 |  573 | `{` |
|       - |  574 | `	ph7_class_method *pMeth;` |
|       - |  575 | `	ph7_class_attr *pAttr;` |
|       - |  576 | `	SyHashEntry *pEntry;` |
|       - |  577 | `	SyString *pName;` |
|       - |  578 | `	sxi32 rc;` |
|       - |  579 | `	/* Detect cyclic trait composition (e.g. trait A { use B; } trait B { use A; }) */` |
|   15331 |  580 | `	if( pTrait->iFlags & PH7_CLASS_TRAIT_VISITING ){` |
|     ! 0 |  581 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|     ! 0 |  582 | `			"Trait circular reference detected: %z is already being applied",&pTrait->sName);` |
|     ! 0 |  583 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  584 | `			return SXERR_ABORT;` |
|       - |  585 | `		}` |
|     ! 0 |  586 | `		return SXRET_OK;` |
|       - |  587 | `	}` |
|   15331 |  588 | `	pTrait->iFlags \|= PH7_CLASS_TRAIT_VISITING;` |
|   15331 |  589 | `	rc = SXRET_OK;` |
|       - |  590 | `	/* Copy attributes from the trait */` |
|   15331 |  591 | `	SyHashResetLoopCursor(&pTrait->hAttr);` |
|   61141 |  592 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hAttr)) != 0 ){` |
|       - |  593 | `		SyHashEntry *pExisting;` |
|   45815 |  594 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   45815 |  595 | `		pName = &pAttr->sName;` |
|   45815 |  596 | `		pExisting = SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte);` |
|   45815 |  597 | `		if( pExisting != 0 ){` |
|       - |  598 | `			/* Attribute already exists. Check if it came from another trait` |
|       - |  599 | `			 * and whether the definitions are compatible (same defaults).` |
|       - |  600 | `			 */` |
|       - |  601 | `			ph7_class **apUsedTraits;` |
|       - |  602 | `			sxu32 nUsed,k;` |
|       6 |  603 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       6 |  604 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|       6 |  605 | `			for(k = 0; k < nUsed; k++){` |
|       - |  606 | `				ph7_class_attr *pOther;` |
|       3 |  607 | `				pOther = PH7_ClassExtractAttribute(apUsedTraits[k],pName->zString,pName->nByte);` |
|       3 |  608 | `				if( pOther ){` |
|       - |  609 | `					/* Two traits define the same property — check if defaults differ */` |
|       3 |  610 | `					ph7_class_attr *pClassAttr = (ph7_class_attr *)pExisting->pUserData;` |
|       4 |  611 | `					if( SySetUsed(&pAttr->aByteCode) != SySetUsed(&pClassAttr->aByteCode) \|\|` |
|       3 |  612 | `						(SySetUsed(&pAttr->aByteCode) > 0 &&` |
|       3 |  613 | `						 SyMemcmp(SySetBasePtr(&pAttr->aByteCode),SySetBasePtr(&pClassAttr->aByteCode),` |
|       3 |  614 | `							SySetUsed(&pAttr->aByteCode) * SySetElemSize(&pAttr->aByteCode)) != 0) ){` |
|       4 |  615 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine,` |
|       - |  616 | `							"%z and %z define the same property ($%z) in the composition of %z. "` |
|       - |  617 | `							"However, the definition differs and is considered incompatible",` |
|       2 |  618 | `							&apUsedTraits[k]->sName,&pTrait->sName,pName,&pClass->sName);` |
|       3 |  619 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  620 | `							goto cleanup;` |
|       - |  621 | `						}` |
|       1 |  622 | `					}` |
|       3 |  623 | `					break;` |
|       - |  624 | `				}` |
|     ! 0 |  625 | `			}` |
|       6 |  626 | `			continue;` |
|       - |  627 | `		}` |
|   45811 |  628 | `		rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|   45811 |  629 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  630 | `			goto cleanup;` |
|       - |  631 | `		}` |
|       5 |  632 | `	}` |
|       - |  633 | `	/* Copy methods from the trait */` |
|   15331 |  634 | `	SyHashResetLoopCursor(&pTrait->hMethod);` |
|  221457 |  635 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hMethod)) != 0 ){` |
|  206131 |  636 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  206131 |  637 | `		pName = &pMeth->sFunc.sName;` |
|  206131 |  638 | `		if( SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte) != 0 ){` |
|       - |  639 | `			/* Method already exists in the class. Check if it came from another trait` |
|       - |  640 | `			 * (unresolved conflict) vs being defined by the class itself.` |
|       - |  641 | `			 */` |
|       - |  642 | `			ph7_class **apUsedTraits;` |
|       - |  643 | `			sxu32 nUsed,k;` |
|      11 |  644 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|      11 |  645 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|      11 |  646 | `			for(k = 0; k < nUsed; k++){` |
|       3 |  647 | `				if( PH7_ClassExtractMethod(apUsedTraits[k],pName->zString,pName->nByte) != 0 ){` |
|       - |  648 | `					/* Two different traits define the same method with no resolution */` |
|       4 |  649 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|       - |  650 | `						"Trait method %z::%z has not been applied as %z::%z, "` |
|       - |  651 | `						"because of collision with %z::%z",` |
|       2 |  652 | `						&pTrait->sName,pName,` |
|       1 |  653 | `						&pClass->sName,pName,` |
|       2 |  654 | `						&apUsedTraits[k]->sName,pName);` |
|       3 |  655 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  656 | `						goto cleanup;` |
|       - |  657 | `					}` |
|       3 |  658 | `					break;` |
|       - |  659 | `				}` |
|     ! 0 |  660 | `			}` |
|       - |  661 | `			/* Class-defined method takes precedence */` |
|      11 |  662 | `			continue;` |
|       - |  663 | `		}` |
|  206123 |  664 | `		rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|  206123 |  665 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  666 | `			goto cleanup;` |
|       - |  667 | `		}` |
|       5 |  668 | `	}` |
|       - |  669 | `	/* Record trait in the class */` |
|   15331 |  670 | `	SySetPut(&pClass->aTrait,(const void *)&pTrait);` |
|    7663 |  671 | `cleanup:` |
|       - |  672 | `	/* Always clear visiting flag, even on error paths */` |
|   15331 |  673 | `	pTrait->iFlags &= ~PH7_CLASS_TRAIT_VISITING;` |
|    7663 |  674 | `	SXUNUSED(pGen);` |
|   15331 |  675 | `	return rc;` |
|    7668 |  676 | `}` |
|       - |  677 | `/*` |
|       - |  678 | ` * Inherit an object interface from another object interface.` |
|       - |  679 | ` * According to the PHP language reference manual.` |
|       - |  680 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|       - |  681 | ` *  must implement, without having to define how these methods are handled.` |
|       - |  682 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  683 | ` *  class, but without any of the methods having their contents defined.` |
|       - |  684 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  685 | ` *` |
|       - |  686 | ` * This function return SXRET_OK if the interface inheritance operation was successfully performed.` |
|       - |  687 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|       - |  688 | ` * error message.` |
|       - |  689 | ` */` |
|   26722 |  690 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceInherit(ph7_class *pSub,ph7_class *pBase)` |
|       5 |  691 | `{` |
|       - |  692 | `	ph7_class_method *pMeth;` |
|       - |  693 | `	ph7_class_attr *pAttr;` |
|       - |  694 | `	SyHashEntry *pEntry;` |
|       - |  695 | `	SyString *pName;` |
|       - |  696 | `	sxi32 rc;` |
|       - |  697 | `	/* Install in the derived hashtable */` |
|   26727 |  698 | `	SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|   26727 |  699 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
|       - |  700 | `	/* Copy constants */` |
|   40090 |  701 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|       - |  702 | `		/* Make sure the constants are not redeclared in the subclass */` |
|       3 |  703 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|       3 |  704 | `		pName = &pAttr->sName;` |
|       3 |  705 | `		if( SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte) == 0 ){` |
|       - |  706 | `			/* Install the constant in the subclass */` |
|       3 |  707 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|       3 |  708 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  709 | `				return rc;` |
|       - |  710 | `			}` |
|       1 |  711 | `		}` |
|       1 |  712 | `	}` |
|   26727 |  713 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
|       - |  714 | `	/* Copy methods signature */` |
|  105002 |  715 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|       - |  716 | `		/* Make sure the method are not redeclared in the subclass */` |
|   64919 |  717 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   64919 |  718 | `		pName = &pMeth->sFunc.sName;` |
|   64919 |  719 | `		if( SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte) == 0 ){` |
|       - |  720 | `			/* Install the method */` |
|   64919 |  721 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|   64919 |  722 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  723 | `				return rc;` |
|       - |  724 | `			}` |
|   32457 |  725 | `		}` |
|       5 |  726 | `	}` |
|       - |  727 | `	/* Mark as subclass */` |
|   26727 |  728 | `	pSub->pBase = pBase;` |
|       - |  729 | `	/* All done */` |
|   26727 |  730 | `	return SXRET_OK;` |
|   13366 |  731 | `}` |
|       - |  732 | `/*` |
|       - |  733 | ` * Implements an object interface in the given main class.` |
|       - |  734 | ` * According to the PHP language reference manual.` |
|       - |  735 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|       - |  736 | ` *  must implement, without having to define how these methods are handled.` |
|       - |  737 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  738 | ` *  class, but without any of the methods having their contents defined.` |
|       - |  739 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  740 | ` *` |
|       - |  741 | ` * This function return SXRET_OK if the interface was successfully implemented.` |
|       - |  742 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|       - |  743 | ` * error message.` |
|       - |  744 | ` */` |
|  355228 |  745 | `PH7_PRIVATE sxi32 PH7_ClassImplement(ph7_class *pMain,ph7_class *pInterface)` |
|       5 |  746 | `{` |
|       - |  747 | `	ph7_class_attr *pAttr;` |
|       - |  748 | `	SyHashEntry *pEntry;` |
|       - |  749 | `	SyString *pName;` |
|       - |  750 | `	sxi32 rc;` |
|       - |  751 | `	/* First off,copy all constants declared inside the interface */` |
|  355233 |  752 | `	SyHashResetLoopCursor(&pInterface->hAttr);` |
|  639709 |  753 | `	while((pEntry = SyHashGetNextEntry(&pInterface->hAttr)) != 0 ){` |
|       - |  754 | `		/* Point to the constant declaration */` |
|  106867 |  755 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  106867 |  756 | `		pName = &pAttr->sName;` |
|       - |  757 | `		/* Make sure the attribute is not redeclared in the main class */` |
|  106867 |  758 | `		if( SyHashGet(&pMain->hAttr,pName->zString,pName->nByte) == 0 ){` |
|       - |  759 | `			/* Install the attribute */` |
|  106863 |  760 | `			rc = SyHashInsert(&pMain->hAttr,pName->zString,pName->nByte,pAttr);` |
|  106863 |  761 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  762 | `				return rc;` |
|       - |  763 | `			}` |
|   53429 |  764 | `		}` |
|       5 |  765 | `	}` |
|       - |  766 | `	/* Install in the interface container */` |
|  355233 |  767 | `	SySetPut(&pMain->aInterface,(const void *)&pInterface);` |
|       - |  768 | `	/* Install interface method stubs into the implementing class.` |
|       - |  769 | `	 * Methods already defined in the class take precedence (they satisfy` |
|       - |  770 | `	 * the interface contract). Stubs retain PH7_CLASS_ATTR_ABSTRACT so` |
|       - |  771 | `	 * the unified check in GenStateCheckAbstractMethods catches missing ones.` |
|       - |  772 | `	 */` |
|       - |  773 | `	{` |
|       - |  774 | `		ph7_class_method *pMeth;` |
|       - |  775 | `		SyHashEntry *pMEntry;` |
|       - |  776 | `		SyString *pMName;` |
|  355233 |  777 | `		SyHashResetLoopCursor(&pInterface->hMethod);` |
| 1201233 |  778 | `		while((pMEntry = SyHashGetNextEntry(&pInterface->hMethod)) != 0 ){` |
|  668391 |  779 | `			pMeth = (ph7_class_method *)pMEntry->pUserData;` |
|  668391 |  780 | `			pMName = &pMeth->sFunc.sName;` |
|  668391 |  781 | `			if( SyHashGet(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte) == 0 ){` |
|      23 |  782 | `				rc = SyHashInsert(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte,pMeth);` |
|      23 |  783 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  784 | `					return rc;` |
|       - |  785 | `				}` |
|       9 |  786 | `			}` |
|       5 |  787 | `		}` |
|       - |  788 | `	}` |
|  355233 |  789 | `	return SXRET_OK;` |
|  177619 |  790 | `}` |
|       - |  791 | `/*` |
|       - |  792 | ` * Create a class instance [i.e: Object in the PHP jargon] at run-time.` |
|       - |  793 | ` * The following function is called when an object is created at run-time` |
|       - |  794 | ` * typically when the PH7_OP_NEW/PH7_OP_CLONE instructions are executed.` |
|       - |  795 | ` * Notes on object creation.` |
|       - |  796 | ` *` |
|       - |  797 | ` * According to PHP language reference manual.` |
|       - |  798 | ` * To create an instance of a class, the new keyword must be used. An object will always` |
|       - |  799 | ` * be created unless the object has a constructor defined that throws an exception on error.` |
|       - |  800 | ` * Classes should be defined before instantiation (and in some cases this is a requirement).` |
|       - |  801 | ` * If a string containing the name of a class is used with new, a new instance of that class` |
|       - |  802 | ` * will be created. If the class is in a namespace, its fully qualified name must be used when` |
|       - |  803 | ` * doing this.` |
|       - |  804 | ` * Example #3 Creating an instance` |
|       - |  805 | ` * <?php` |
|       - |  806 | ` *  $instance = new SimpleClass();` |
|       - |  807 | ` *   // This can also be done with a variable:` |
|       - |  808 | ` * $className = 'Foo';` |
|       - |  809 | ` * $instance = new $className(); // Foo()` |
|       - |  810 | ` * ?>` |
|       - |  811 | ` * In the class context, it is possible to create a new object by new self and new parent.` |
|       - |  812 | ` * When assigning an already created instance of a class to a new variable, the new variable` |
|       - |  813 | ` * will access the same instance as the object that was assigned. This behaviour is the same` |
|       - |  814 | ` * when passing instances to a function. A copy of an already created object can be made by` |
|       - |  815 | ` * cloning it.` |
|       - |  816 | ` * Example #4 Object Assignment` |
|       - |  817 | ` * <?php` |
|       - |  818 | ` *  class SimpleClass(){` |
|       - |  819 | ` *    public $var;` |
|       - |  820 | ` *  };` |
|       - |  821 | ` *  $instance = new SimpleClass();` |
|       - |  822 | ` *  $assigned   =  $instance;` |
|       - |  823 | ` *  $reference  =& $instance;` |
|       - |  824 | ` *  $instance->var = '$assigned will have this value';` |
|       - |  825 | ` *  $instance = null; // $instance and $reference become null` |
|       - |  826 | ` *  var_dump($instance);` |
|       - |  827 | ` *  var_dump($reference);` |
|       - |  828 | ` *  var_dump($assigned);` |
|       - |  829 | ` * ?>` |
|       - |  830 | ` * The above example will output:` |
|       - |  831 | ` * NULL` |
|       - |  832 | ` * NULL` |
|       - |  833 | ` * object(SimpleClass)#1 (1) {` |
|       - |  834 | ` *  ["var"]=>` |
|       - |  835 | ` *    string(30) "$assigned will have this value"` |
|       - |  836 | ` * }` |
|       - |  837 | ` * Example #5 Creating new objects` |
|       - |  838 | ` * <?php` |
|       - |  839 | ` * class Test` |
|       - |  840 | ` * {` |
|       - |  841 | ` *   static public function getNew()` |
|       - |  842 | ` *   {` |
|       - |  843 | ` *       return new static;` |
|       - |  844 | ` *   }` |
|       - |  845 | ` * }` |
|       - |  846 | ` * class Child extends Test` |
|       - |  847 | ` * {}` |
|       - |  848 | ` * $obj1 = new Test();` |
|       - |  849 | ` * $obj2 = new $obj1;` |
|       - |  850 | ` * var_dump($obj1 !== $obj2);` |
|       - |  851 | ` * $obj3 = Test::getNew();` |
|       - |  852 | ` * var_dump($obj3 instanceof Test);` |
|       - |  853 | ` * $obj4 = Child::getNew();` |
|       - |  854 | ` * var_dump($obj4 instanceof Child);` |
|       - |  855 | ` * ?>` |
|       - |  856 | ` * The above example will output:` |
|       - |  857 | ` * bool(true)` |
|       - |  858 | ` * bool(true)` |
|       - |  859 | ` * bool(true)` |
|       - |  860 | ` * Note that Symisc Systems have introduced powerfull extension to` |
|       - |  861 | ` * OO subsystem. For example a class attribute may have any complex` |
|       - |  862 | ` * expression associated with it when declaring the attribute unlike` |
|       - |  863 | ` * the standard PHP engine which would allow a single value.` |
|       - |  864 | ` * Example:` |
|       - |  865 | ` *  class myClass{` |
|       - |  866 | ` *    public $var = 25<<1+foo()/bar();` |
|       - |  867 | ` *  };` |
|       - |  868 | ` * Refer to the official documentation for more information.` |
|       - |  869 | ` */` |
|    7966 |  870 | `static ph7_class_instance * NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|       5 |  871 | `{` |
|       - |  872 | `	ph7_class_instance *pThis;` |
|       - |  873 | `	/* Allocate a new instance */` |
|    7971 |  874 | `	pThis = (ph7_class_instance *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_instance));` |
|    7971 |  875 | `	if( pThis == 0 ){` |
|     ! 0 |  876 | `		return 0;` |
|       - |  877 | `	}` |
|       - |  878 | `	/* Zero the structure */` |
|    7971 |  879 | `	SyZero(pThis,sizeof(ph7_class_instance));` |
|       - |  880 | `	/* Initialize fields */` |
|    7971 |  881 | `	pThis->iRef = 1;` |
|    7971 |  882 | `	pThis->pVm = pVm;` |
|    7971 |  883 | `	pThis->pClass = pClass;` |
|       - |  884 | `	/* Assign a fresh monotonic object handle id (clones get their own, like PHP). */` |
|    7971 |  885 | `	pThis->nObjId = pVm->nNextObjId++;` |
|    7971 |  886 | `	SyHashInit(&pThis->hAttr,&pVm->sAllocator,0,0);` |
|    7971 |  887 | `	return pThis;` |
|    3988 |  888 | `}` |
|       - |  889 | `/*` |
|       - |  890 | ` * Wrapper around the NewClassInstance() function defined above.` |
|       - |  891 | ` * See the block comment above for more information.` |
|       - |  892 | ` */` |
|    7728 |  893 | `PH7_PRIVATE ph7_class_instance * PH7_NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|       5 |  894 | `{` |
|       - |  895 | `	ph7_class_instance *pNew;` |
|       - |  896 | `	sxi32 rc;` |
|    7733 |  897 | `	pNew = NewClassInstance(&(*pVm),&(*pClass));` |
|    7733 |  898 | `	if( pNew == 0 ){` |
|     ! 0 |  899 | `		return 0;` |
|       - |  900 | `	}` |
|       - |  901 | `	/* Associate a private VM frame with this class instance */` |
|    7733 |  902 | `	rc = PH7_VmCreateClassInstanceFrame(&(*pVm),pNew);` |
|    7733 |  903 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  904 | `		SyMemBackendPoolFree(&pVm->sAllocator,pNew);` |
|     ! 0 |  905 | `		return 0;` |
|       - |  906 | `	}` |
|       - |  907 | `	/* php stamps a Throwable's file/line at CREATION, not in its constructor, so a` |
|       - |  908 | `	 * subclass that overrides __construct without calling parent::__construct still` |
|       - |  909 | `	 * reports the right site. Every instantiation path lands here. */` |
|    7733 |  910 | `	PH7_VmStampThrowableSite(&(*pVm),pNew);` |
|    7733 |  911 | `	return pNew;` |
|    3869 |  912 | `}` |
|       - |  913 | `/*` |
|       - |  914 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon] attribute.` |
|       - |  915 | ` * This function never fail.` |
|       - |  916 | ` */` |
|   20598 |  917 | `static ph7_value * ExtractClassAttrValue(ph7_vm *pVm,VmClassAttr *pAttr)` |
|       5 |  918 | `{` |
|       - |  919 | `	/* Extract the value */` |
|       - |  920 | `	ph7_value *pValue;` |
|   20603 |  921 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|   20603 |  922 | `	return pValue;` |
|       5 |  923 | `}` |
|       - |  924 | `/*` |
|       - |  925 | ` * Perform a clone operation on a class instance [i.e: Object in the PHP jargon].` |
|       - |  926 | ` * The following function is called when an object is cloned at run-time` |
|       - |  927 | ` * typically when the PH7_OP_CLONE instruction is executed.` |
|       - |  928 | ` * Notes on object cloning.` |
|       - |  929 | ` *` |
|       - |  930 | ` * According to PHP language reference manual.` |
|       - |  931 | ` * Creating a copy of an object with fully replicated properties is not always the wanted behavior.` |
|       - |  932 | ` * A good example of the need for copy constructors. Another example is if your object holds a reference` |
|       - |  933 | ` * to another object which it uses and when you replicate the parent object you want to create` |
|       - |  934 | ` * a new instance of this other object so that the replica has its own separate copy.` |
|       - |  935 | ` * An object copy is created by using the clone keyword (which calls the object's __clone() method if possible).` |
|       - |  936 | ` * An object's __clone() method cannot be called directly.` |
|       - |  937 | ` * $copy_of_object = clone $object;` |
|       - |  938 | ` * When an object is cloned, PHP 5 will perform a shallow copy of all of the object's properties.` |
|       - |  939 | ` * Any properties that are references to other variables, will remain references.` |
|       - |  940 | ` * Once the cloning is complete, if a __clone() method is defined, then the newly created object's __clone() method` |
|       - |  941 | ` * will be called, to allow any necessary properties that need to be changed.` |
|       - |  942 | ` * Example #1 Cloning an object` |
|       - |  943 | ` * <?php` |
|       - |  944 | ` * class SubObject` |
|       - |  945 | ` * {` |
|       - |  946 | ` *   static $instances = 0;` |
|       - |  947 | ` *   public $instance;` |
|       - |  948 | ` *` |
|       - |  949 | ` *   public function __construct() {` |
|       - |  950 | ` *       $this->instance = ++self::$instances;` |
|       - |  951 | ` *   }` |
|       - |  952 | ` *` |
|       - |  953 | ` *   public function __clone() {` |
|       - |  954 | ` *       $this->instance = ++self::$instances;` |
|       - |  955 | ` *   }` |
|       - |  956 | ` * }` |
|       - |  957 | ` *` |
|       - |  958 | ` * class MyCloneable` |
|       - |  959 | ` * {` |
|       - |  960 | ` *   public $object1;` |
|       - |  961 | ` *   public $object2;` |
|       - |  962 | ` *` |
|       - |  963 | ` *   function __clone()` |
|       - |  964 | ` *   {` |
|       - |  965 | ` *       // Force a copy of this->object, otherwise` |
|       - |  966 | ` *       // it will point to same object.` |
|       - |  967 | ` *       $this->object1 = clone $this->object1;` |
|       - |  968 | ` *   }` |
|       - |  969 | ` * }` |
|       - |  970 | ` * $obj = new MyCloneable();` |
|       - |  971 | ` * $obj->object1 = new SubObject();` |
|       - |  972 | ` * $obj->object2 = new SubObject();` |
|       - |  973 | ` * $obj2 = clone $obj;` |
|       - |  974 | ` * print("Original Object:\n");` |
|       - |  975 | ` * print_r($obj);` |
|       - |  976 | ` * print("Cloned Object:\n");` |
|       - |  977 | ` * print_r($obj2);` |
|       - |  978 | ` * ?>` |
|       - |  979 | ` * The above example will output:` |
|       - |  980 | ` * Original Object:` |
|       - |  981 | ` * MyCloneable Object` |
|       - |  982 | ` * (` |
|       - |  983 | ` *   [object1] => SubObject Object` |
|       - |  984 | ` *       (` |
|       - |  985 | ` *           [instance] => 1` |
|       - |  986 | ` *       )` |
|       - |  987 | ` *` |
|       - |  988 | ` *   [object2] => SubObject Object` |
|       - |  989 | ` *       (` |
|       - |  990 | ` *           [instance] => 2` |
|       - |  991 | ` *       )` |
|       - |  992 | ` *` |
|       - |  993 | ` * )` |
|       - |  994 | ` * Cloned Object:` |
|       - |  995 | ` * MyCloneable Object` |
|       - |  996 | ` * (` |
|       - |  997 | ` *   [object1] => SubObject Object` |
|       - |  998 | ` *       (` |
|       - |  999 | ` *           [instance] => 3` |
|       - | 1000 | ` *       )` |
|       - | 1001 | ` *` |
|       - | 1002 | ` *   [object2] => SubObject Object` |
|       - | 1003 | ` *       (` |
|       - | 1004 | ` *           [instance] => 2` |
|       - | 1005 | ` *       )` |
|       - | 1006 | ` * )` |
|       - | 1007 | ` */` |
|     238 | 1008 | `PH7_PRIVATE ph7_class_instance * PH7_CloneClassInstance(ph7_class_instance *pSrc)` |
|       3 | 1009 | `{` |
|       - | 1010 | `	ph7_class_instance *pClone;` |
|       - | 1011 | `	ph7_class_method *pMethod;` |
|       - | 1012 | `	SyHashEntry *pEntry2;` |
|       - | 1013 | `	SyHashEntry *pEntry;` |
|       - | 1014 | `	ph7_vm *pVm;` |
|       - | 1015 | `	sxi32 rc;` |
|       - | 1016 | `	/* Allocate a new instance */` |
|     241 | 1017 | `	pVm = pSrc->pVm;` |
|     241 | 1018 | `	pClone = NewClassInstance(pVm,pSrc->pClass);` |
|     241 | 1019 | `	if( pClone == 0 ){` |
|     ! 0 | 1020 | `		return 0;` |
|       - | 1021 | `	}` |
|       - | 1022 | `	/* Associate a private VM frame with this class instance */` |
|     241 | 1023 | `	rc = PH7_VmCreateClassInstanceFrame(pVm,pClone);` |
|     241 | 1024 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1025 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClone);` |
|     ! 0 | 1026 | `		return 0;` |
|       - | 1027 | `	}` |
|       - | 1028 | `	/* Duplicate object values. Iterate the SOURCE attributes and copy each into` |
|       - | 1029 | `	 * the clone's same-named slot (looked up by name, so order/count differences` |
|       - | 1030 | `	 * from dynamic properties don't matter). A dynamic (runtime-added) property` |
|       - | 1031 | `	 * has no declared counterpart in the clone, so synthesize it first — without` |
|       - | 1032 | `	 * this, a clone of a stdClass would silently lose all its dynamic properties. */` |
|     241 | 1033 | `	SyHashResetLoopCursor(&pSrc->hAttr);` |
|    2621 | 1034 | `	while((pEntry = SyHashGetNextEntry(&pSrc->hAttr)) != 0 ){` |
|    2383 | 1035 | `		VmClassAttr *pSrcAttr = (VmClassAttr *)pEntry->pUserData;` |
|    2383 | 1036 | `		VmClassAttr *pDestAttr = 0;` |
|    2383 | 1037 | `		ph7_value *pvSrc,*pvDest = 0;` |
|       - | 1038 | `		/* Duplicate non-static attribute */` |
|    2383 | 1039 | `		if( pSrcAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|    1693 | 1040 | `			continue;` |
|       - | 1041 | `		}` |
|     691 | 1042 | `		pEntry2 = SyHashGet(&pClone->hAttr,SyStringData(&pSrcAttr->pAttr->sName),SyStringLength(&pSrcAttr->pAttr->sName));` |
|     691 | 1043 | `		if( pEntry2 ){` |
|     677 | 1044 | `			pDestAttr = (VmClassAttr *)pEntry2->pUserData;` |
|     677 | 1045 | `			pvDest = ExtractClassAttrValue(pVm,pDestAttr);` |
|     352 | 1046 | `		}else if( pSrcAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){` |
|       - | 1047 | `			/* Dynamic property: synthesize the matching slot on the clone. */` |
|      22 | 1048 | `			pvDest = PH7_VmCreateDynamicAttr(pVm,pClone,` |
|      14 | 1049 | `				SyStringData(&pSrcAttr->pAttr->sName),SyStringLength(&pSrcAttr->pAttr->sName),&pDestAttr);` |
|       7 | 1050 | `		}` |
|       - | 1051 | `		/* Fetch the source value LAST: PH7_VmCreateDynamicAttr above may have` |
|       - | 1052 | `		 * reserved a slot and reallocated pVm->aMemObj, which would dangle any` |
|       - | 1053 | `		 * ph7_value* obtained before it. pvDest from the synth path already points` |
|       - | 1054 | `		 * into the post-realloc aMemObj; resolve pvSrc now so both are current. */` |
|     691 | 1055 | `		pvSrc = ExtractClassAttrValue(pVm,pSrcAttr);` |
|     691 | 1056 | `		if( pvSrc && pvDest ){` |
|     691 | 1057 | `			PH7_MemObjStore(pvSrc,pvDest);` |
|     344 | 1058 | `		}` |
|       - | 1059 | `		/* Carry over the per-instance state so the clone matches the source:` |
|       - | 1060 | `		 * VM_CLASS_ATTR_UNINIT marks a typed property as not-yet-initialized` |
|       - | 1061 | `		 * and doubles as the readonly write-once latch — without this a clone` |
|       - | 1062 | `		 * would reset to uninitialized (losing the value's readiness) and a` |
|       - | 1063 | `		 * readonly property would become writable again. */` |
|     691 | 1064 | `		if( pDestAttr ){` |
|     691 | 1065 | `			pDestAttr->iState = pSrcAttr->iState;` |
|     344 | 1066 | `		}` |
|       3 | 1067 | `	}` |
|       - | 1068 | `	/* A declared property unset() on the source is absent from the clone too (PHP). But the clone` |
|       - | 1069 | `	 * frame above materialized ALL declared attrs (with their defaults), so drop any clone attr whose` |
|       - | 1070 | `	 * name is not present on the source. Collect first, then delete — removing an entry mid-walk would` |
|       - | 1071 | `	 * free the node the SyHash loop cursor points at. */` |
|       - | 1072 | `	{` |
|       - | 1073 | `		SySet sDrop;` |
|     241 | 1074 | `		SySetInit(&sDrop,&pVm->sAllocator,sizeof(VmClassAttr *));` |
|     241 | 1075 | `		SyHashResetLoopCursor(&pClone->hAttr);` |
|    2623 | 1076 | `		while((pEntry = SyHashGetNextEntry(&pClone->hAttr)) != 0 ){` |
|    2385 | 1077 | `			VmClassAttr *pCloneAttr = (VmClassAttr *)pEntry->pUserData;` |
|    2385 | 1078 | `			if( pCloneAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|    1693 | 1079 | `				continue;` |
|       - | 1080 | `			}` |
|    1035 | 1081 | `			if( SyHashGet(&pSrc->hAttr,SyStringData(&pCloneAttr->pAttr->sName),` |
|    1038 | 1082 | `					SyStringLength(&pCloneAttr->pAttr->sName)) == 0 ){` |
|       3 | 1083 | `				SySetPut(&sDrop,(const void *)&pCloneAttr);` |
|       1 | 1084 | `			}` |
|       3 | 1085 | `		}` |
|     241 | 1086 | `		if( SySetUsed(&sDrop) > 0 ){` |
|       3 | 1087 | `			VmClassAttr **apDrop = (VmClassAttr **)SySetBasePtr(&sDrop);` |
|       - | 1088 | `			sxu32 i;` |
|       5 | 1089 | `			for( i = 0 ; i < SySetUsed(&sDrop) ; ++i ){` |
|       3 | 1090 | `				VmClassAttr *pVmAttr = apDrop[i];` |
|       4 | 1091 | `				SyHashDeleteEntry(&pClone->hAttr,SyStringData(&pVmAttr->pAttr->sName),` |
|       2 | 1092 | `					SyStringLength(&pVmAttr->pAttr->sName),0);` |
|       3 | 1093 | `				PH7_VmReleaseInstanceAttr(pVm,pVmAttr);` |
|       2 | 1094 | `			}` |
|       1 | 1095 | `		}` |
|     241 | 1096 | `		SySetRelease(&sDrop);` |
|       - | 1097 | `	}` |
|       - | 1098 | `	/* call the __clone method on the cloned object if available */` |
|     241 | 1099 | `	pMethod = PH7_ClassExtractMethod(pClone->pClass,"__clone",sizeof("__clone")-1);` |
|     241 | 1100 | `	if( pMethod ){` |
|      56 | 1101 | `		if( pMethod->iCloneDepth < 16 ){` |
|      54 | 1102 | `			pMethod->iCloneDepth++;` |
|      54 | 1103 | `			PH7_VmCallClassMethod(pVm,pClone,pMethod,0,0,0);` |
|      28 | 1104 | `		}else{` |
|       - | 1105 | `			/* Nesting limit reached */` |
|       3 | 1106 | `			PH7_VmThrowError(pVm,0,PH7_CTX_ERR,"Object clone limit reached,no more call to __clone()");` |
|       - | 1107 | `		}` |
|       - | 1108 | `		/* Reset the cursor */` |
|      56 | 1109 | `		pMethod->iCloneDepth = 0;` |
|      27 | 1110 | `	}` |
|       - | 1111 | `	/* Return the cloned object */` |
|     241 | 1112 | `	return pClone;` |
|     122 | 1113 | `}` |
|       - | 1114 | `#define CLASS_INSTANCE_DESTROYED 0x001 /* Instance is released */` |
|       - | 1115 | `/*` |
|       - | 1116 | ` * Free the per-instance allocations owned by ONE object attribute: its value slot (+ the typed-slot` |
|       - | 1117 | ` * enforcement entry), the synthesized ph7_class_attr for a dynamic (runtime-added) property, and the` |
|       - | 1118 | ` * VmClassAttr wrapper itself. Does NOT touch the hAttr entry node — the caller removes it` |
|       - | 1119 | `` * (`unset($o->p)` via SyHashDeleteEntry2; instance teardown via the wholesale SyHashRelease, so it must`` |
|       - | 1120 | ` * not delete entries mid-walk). Shared by PH7_ClassInstanceRelease and the OP_MEMBER unset path.` |
|       - | 1121 | ` */` |
|   27224 | 1122 | `PH7_PRIVATE void PH7_VmReleaseInstanceAttr(ph7_vm *pVm, VmClassAttr *pVmAttr)` |
|       5 | 1123 | `{` |
|   27229 | 1124 | `	if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|       - | 1125 | `		/* Drop any typed-property enforcement slot registered for this memobj, before the memobj` |
|       - | 1126 | `		 * is returned to the free list, so a future recycled slot does not inherit the stale entry. */` |
|   20179 | 1127 | `		if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|     292 | 1128 | `			SyHashDeleteEntry(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),0);` |
|     144 | 1129 | `		}` |
|   20179 | 1130 | `		PH7_VmUnsetMemObj(pVm,pVmAttr->nIdx,TRUE);` |
|   10087 | 1131 | `	}` |
|       - | 1132 | `	/* A dynamic property owns its synthesized ph7_class_attr (struct + inline name in one block) —` |
|       - | 1133 | `	 * free it here (the only place a per-instance pAttr is freed; declared attrs are class-owned). */` |
|   27229 | 1134 | `	if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){` |
|     117 | 1135 | `		SyMemBackendFree(&pVm->sAllocator,pVmAttr->pAttr);` |
|      58 | 1136 | `	}` |
|   27229 | 1137 | `	SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|   27229 | 1138 | `}` |
|       - | 1139 | `/*` |
|       - | 1140 | ` * Release a class instance [i.e: Object in the PHP jargon] and invoke any defined destructor.` |
|       - | 1141 | ` * This routine is invoked as soon as there are no other references to a particular` |
|       - | 1142 | ` * class instance.` |
|       - | 1143 | ` */` |
|    5406 | 1144 | `static void PH7_ClassInstanceRelease(ph7_class_instance *pThis)` |
|       5 | 1145 | `{` |
|       - | 1146 | `	ph7_class_method *pDestr;` |
|       - | 1147 | `	SyHashEntry *pEntry;` |
|       - | 1148 | `	ph7_class *pClass;` |
|       - | 1149 | `	ph7_vm *pVm;` |
|    5411 | 1150 | `	if( pThis->iFlags & CLASS_INSTANCE_DESTROYED ){` |
|       - | 1151 | `		/*` |
|       - | 1152 | `		 * Already destroyed,return immediately.` |
|       - | 1153 | `		 * This could happend if someone perform unset($this) in the destructor body.` |
|       - | 1154 | `		 */` |
|     ! 0 | 1155 | `		return;` |
|       - | 1156 | `	}` |
|       - | 1157 | `	/* Mark as destroyed */` |
|    5411 | 1158 | `	pThis->iFlags \|= CLASS_INSTANCE_DESTROYED;` |
|       - | 1159 | `	/* Invoke any defined destructor if available */` |
|    5411 | 1160 | `	pVm = pThis->pVm;` |
|    5411 | 1161 | `	pClass = pThis->pClass;` |
|    5411 | 1162 | `	pDestr = PH7_ClassExtractMethod(pClass,"__destruct",sizeof("__destruct")-1);` |
|    5411 | 1163 | `	if( pDestr && !pVm->bInReset ){` |
|       - | 1164 | `		/* Invoke the destructor. Skipped during ph7_vm_reset() bulk teardown:` |
|       - | 1165 | `		 * running user PHP against a half-reset VM is unsafe (see bInReset). */` |
|     471 | 1166 | `		pThis->iRef = 2; /* Prevent garbage collection */` |
|     471 | 1167 | `		PH7_VmCallClassMethod(pVm,pThis,pDestr,0,0,0);` |
|     233 | 1168 | `	}` |
|       - | 1169 | `	/* Weak-reference registry: kill the cell for this instance so every` |
|       - | 1170 | `	 * WeakReference/WeakMap handle observes the death (the cell outlives the` |
|       - | 1171 | `	 * instance until its own handles drop; removing the hash entry here keeps` |
|       - | 1172 | `	 * a pool-reused address from resurrecting a dead cell). */` |
|    5411 | 1173 | `	if( SyHashTotalEntry(&pVm->hWeakCell) > 0 ){` |
|      27 | 1174 | `		void *pCellData = 0;` |
|      26 | 1175 | `		if( SyHashDeleteEntry(&pVm->hWeakCell,(const void *)&pThis,sizeof(void *),&pCellData) == SXRET_OK` |
|      18 | 1176 | `		 && pCellData ){` |
|       9 | 1177 | `			((VmWeakCell *)pCellData)->pObj = 0;` |
|       4 | 1178 | `		}` |
|      13 | 1179 | `	}` |
|       - | 1180 | `	/* Release non-static attributes (the wholesale SyHashRelease below frees the entry nodes,` |
|       - | 1181 | `	 * so the helper must not delete them mid-walk). */` |
|    5411 | 1182 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|   32607 | 1183 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|   27201 | 1184 | `		PH7_VmReleaseInstanceAttr(pVm,(VmClassAttr *)pEntry->pUserData);` |
|       5 | 1185 | `	}` |
|       - | 1186 | `	/* Release the whole structure */` |
|    5411 | 1187 | `	SyHashRelease(&pThis->hAttr);` |
|    5411 | 1188 | `	SyMemBackendPoolFree(&pVm->sAllocator,pThis);` |
|    2708 | 1189 | `}` |
|       - | 1190 | `/*` |
|       - | 1191 | ` * Decrement the reference count of a class instance [i.e Object in the PHP jargon].` |
|       - | 1192 | ` * If the reference count reaches zero,release the whole instance.` |
|       - | 1193 | ` */` |
|  125622 | 1194 | `PH7_PRIVATE void PH7_ClassInstanceUnref(ph7_class_instance *pThis)` |
|       5 | 1195 | `{` |
|  125627 | 1196 | `	pThis->iRef--;` |
|  125627 | 1197 | `	if( pThis->iRef < 1 ){` |
|       - | 1198 | `		/* No more reference to this instance */` |
|    5411 | 1199 | `		PH7_ClassInstanceRelease(&(*pThis));` |
|    2703 | 1200 | `	}` |
|  125627 | 1201 | `}` |
|       - | 1202 | `/*` |
|       - | 1203 | ` * Compare two class instances [i.e: Objects in the PHP jargon]` |
|       - | 1204 | ` * Note on objects comparison:` |
|       - | 1205 | ` *  According to the PHP langauge reference manual` |
|       - | 1206 | ` *  When using the comparison operator (==), object variables are compared in a simple manner` |
|       - | 1207 | ` *  namely: Two object instances are equal if they have the same attributes and values, and are` |
|       - | 1208 | ` *  instances of the same class.` |
|       - | 1209 | ` *  On the other hand, when using the identity operator (===), object variables are identical` |
|       - | 1210 | ` *  if and only if they refer to the same instance of the same class.` |
|       - | 1211 | ` *  An example will clarify these rules.` |
|       - | 1212 | ` *  Example #1 Example of object comparison` |
|       - | 1213 | ` *  <?php` |
|       - | 1214 | ` *    function bool2str($bool)` |
|       - | 1215 | ` * {` |
|       - | 1216 | ` *   if ($bool === false) {` |
|       - | 1217 | ` *       return 'FALSE';` |
|       - | 1218 | ` *   } else {` |
|       - | 1219 | ` *       return 'TRUE';` |
|       - | 1220 | ` *   }` |
|       - | 1221 | ` * }` |
|       - | 1222 | ` * function compareObjects(&$o1, &$o2)` |
|       - | 1223 | ` * {` |
|       - | 1224 | ` *   echo 'o1 == o2 : ' . bool2str($o1 == $o2) . "\n";` |
|       - | 1225 | ` *   echo 'o1 != o2 : ' . bool2str($o1 != $o2) . "\n";` |
|       - | 1226 | ` *   echo 'o1 === o2 : ' . bool2str($o1 === $o2) . "\n";` |
|       - | 1227 | ` *   echo 'o1 !== o2 : ' . bool2str($o1 !== $o2) . "\n";` |
|       - | 1228 | ` * }` |
|       - | 1229 | ` * class Flag` |
|       - | 1230 | ` * {` |
|       - | 1231 | ` *   public $flag;` |
|       - | 1232 | ` *` |
|       - | 1233 | ` *   function Flag($flag = true) {` |
|       - | 1234 | ` *       $this->flag = $flag;` |
|       - | 1235 | ` *   }` |
|       - | 1236 | ` * }` |
|       - | 1237 | ` *` |
|       - | 1238 | ` * class OtherFlag` |
|       - | 1239 | ` * {` |
|       - | 1240 | ` *   public $flag;` |
|       - | 1241 | ` *` |
|       - | 1242 | ` *   function OtherFlag($flag = true) {` |
|       - | 1243 | ` *       $this->flag = $flag;` |
|       - | 1244 | ` *   }` |
|       - | 1245 | ` * }` |
|       - | 1246 | ` *` |
|       - | 1247 | ` * $o = new Flag();` |
|       - | 1248 | ` * $p = new Flag();` |
|       - | 1249 | ` * $q = $o;` |
|       - | 1250 | ` * $r = new OtherFlag();` |
|       - | 1251 | ` *` |
|       - | 1252 | ` * echo "Two instances of the same class\n";` |
|       - | 1253 | ` * compareObjects($o, $p);` |
|       - | 1254 | ` * echo "\nTwo references to the same instance\n";` |
|       - | 1255 | ` * compareObjects($o, $q);` |
|       - | 1256 | ` * echo "\nInstances of two different classes\n";` |
|       - | 1257 | ` * compareObjects($o, $r);` |
|       - | 1258 | ` * ?>` |
|       - | 1259 | ` * The above example will output:` |
|       - | 1260 | ` * Two instances of the same class` |
|       - | 1261 | ` * o1 == o2 : TRUE` |
|       - | 1262 | ` * o1 != o2 : FALSE` |
|       - | 1263 | ` * o1 === o2 : FALSE` |
|       - | 1264 | ` * o1 !== o2 : TRUE` |
|       - | 1265 | ` * Two references to the same instance` |
|       - | 1266 | ` * o1 == o2 : TRUE` |
|       - | 1267 | ` * o1 != o2 : FALSE` |
|       - | 1268 | ` * o1 === o2 : TRUE` |
|       - | 1269 | ` * o1 !== o2 : FALSE` |
|       - | 1270 | ` * Instances of two different classes` |
|       - | 1271 | ` * o1 == o2 : FALSE` |
|       - | 1272 | ` * o1 != o2 : TRUE` |
|       - | 1273 | ` * o1 === o2 : FALSE` |
|       - | 1274 | ` * o1 !== o2 : TRUE` |
|       - | 1275 | ` *` |
|       - | 1276 | ` * This function return 0 if the objects are equals according to the comprison rules defined above.` |
|       - | 1277 | ` * Any other return values indicates difference.` |
|       - | 1278 | ` */` |
|     272 | 1279 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest)` |
|       5 | 1280 | `{` |
|       - | 1281 | `	SyHashEntry *pEntry,*pEntry2;` |
|       - | 1282 | `	ph7_value sV1,sV2;` |
|       - | 1283 | `	sxi32 rc;` |
|     277 | 1284 | `	if( iNest > 31 ){` |
|       - | 1285 | `		/* Nesting limit reached */` |
|       6 | 1286 | `		PH7_VmThrowError(pLeft->pVm,0,PH7_CTX_ERR,"Nesting limit reached: Infinite recursion?");` |
|       6 | 1287 | `		return 1;` |
|       - | 1288 | `	}` |
|       - | 1289 | `	/* Comparison is performed only if the objects are instance of the same class */` |
|     273 | 1290 | `	if( pLeft->pClass != pRight->pClass ){` |
|       7 | 1291 | `		return 1;` |
|       - | 1292 | `	}` |
|     267 | 1293 | `	if( bStrict ){` |
|       - | 1294 | `		/*` |
|       - | 1295 | `		 * According to the PHP language reference manual:` |
|       - | 1296 | `		 *  when using the identity operator (===), object variables` |
|       - | 1297 | `		 *  are identical if and only if they refer to the same instance` |
|       - | 1298 | `		 *  of the same class.` |
|       - | 1299 | `		 */` |
|     103 | 1300 | `		return !(pLeft == pRight);` |
|       - | 1301 | `	}` |
|       - | 1302 | `	/*` |
|       - | 1303 | `	 * Attribute comparison.` |
|       - | 1304 | `	 * According to the PHP reference manual:` |
|       - | 1305 | `	 *  When using the comparison operator (==), object variables are compared` |
|       - | 1306 | `	 *  in a simple manner, namely: Two object instances are equal if they have` |
|       - | 1307 | `	 *  the same attributes and values, and are instances of the same class.` |
|       - | 1308 | `	 */` |
|     168 | 1309 | `	if( pLeft == pRight ){` |
|       - | 1310 | `		/* Same instance,don't bother processing,object are equals */` |
|       5 | 1311 | `		return 0;` |
|       - | 1312 | `	}` |
|       - | 1313 | `	/* Closures compare by IDENTITY under == as well (not by attributes): two distinct` |
|       - | 1314 | `	 * Closure instances are never equal, even when they wrap the same underlying function` |
|       - | 1315 | `	 * (PHP semantics). pLeft != pRight here, so a Closure pair is unequal. Without this,` |
|       - | 1316 | `` 	 * two capture-less lambdas of the same `function(){}` share the template's `$__fn` `` |
|       - | 1317 | `	 * name and would compare equal. */` |
|     164 | 1318 | `	if( pLeft->pVm->pClosureClass && pLeft->pClass == pLeft->pVm->pClosureClass ){` |
|       5 | 1319 | `		return 1;` |
|       - | 1320 | `	}` |
|       - | 1321 | `	/* Same class but a different number of attributes ⇒ different property sets` |
|       - | 1322 | `	 * (dynamic properties can give two same-class instances different counts). */` |
|     160 | 1323 | `	if( pLeft->hAttr.nEntry != pRight->hAttr.nEntry ){` |
|       3 | 1324 | `		return 1;` |
|       - | 1325 | `	}` |
|     158 | 1326 | `	PH7_MemObjInit(pLeft->pVm,&sV1);` |
|     158 | 1327 | `	PH7_MemObjInit(pLeft->pVm,&sV2);` |
|     158 | 1328 | `	sV1.nIdx = sV2.nIdx = SXU32_HIGH;` |
|       - | 1329 | `	/* Compare each left attribute against the RIGHT attribute of the SAME NAME` |
|       - | 1330 | `	 * (not in lockstep): dynamic properties may be stored in a different order` |
|       - | 1331 | `	 * on the two instances. Counts already match, so if every left attribute has` |
|       - | 1332 | `	 * an equal-valued same-named right attribute the property sets are equal. */` |
|     158 | 1333 | `	SyHashResetLoopCursor(&pLeft->hAttr);` |
|     194 | 1334 | `	while((pEntry = SyHashGetNextEntry(&pLeft->hAttr)) != 0 ){` |
|     170 | 1335 | `		VmClassAttr *p1 = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1336 | `		VmClassAttr *p2;` |
|       - | 1337 | `		ph7_value *pL,*pR;` |
|       - | 1338 | `		/* Compare only non-static attribute */` |
|     170 | 1339 | `		if( p1->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|     ! 0 | 1340 | `			continue;` |
|       - | 1341 | `		}` |
|     170 | 1342 | `		pEntry2 = SyHashGet(&pRight->hAttr,SyStringData(&p1->pAttr->sName),SyStringLength(&p1->pAttr->sName));` |
|     170 | 1343 | `		if( pEntry2 == 0 ){` |
|       - | 1344 | `			/* Left has a property the right lacks ⇒ not equal. */` |
|     ! 0 | 1345 | `			return 1;` |
|       - | 1346 | `		}` |
|     170 | 1347 | `		p2 = (VmClassAttr *)pEntry2->pUserData;` |
|     170 | 1348 | `		pL = ExtractClassAttrValue(pLeft->pVm,p1);` |
|     170 | 1349 | `		pR = ExtractClassAttrValue(pRight->pVm,p2);` |
|     170 | 1350 | `		if( pL && pR ){` |
|     170 | 1351 | `			PH7_MemObjLoad(pL,&sV1);` |
|     170 | 1352 | `			PH7_MemObjLoad(pR,&sV2);` |
|       - | 1353 | `			/* Compare the two values now */` |
|     170 | 1354 | `			rc = PH7_MemObjCmp(&sV1,&sV2,bStrict,iNest+1);` |
|     170 | 1355 | `			PH7_MemObjRelease(&sV1);` |
|     170 | 1356 | `			PH7_MemObjRelease(&sV2);` |
|     170 | 1357 | `			if( rc != 0 ){` |
|       - | 1358 | `				/* Not equals */` |
|     133 | 1359 | `				return rc;` |
|       - | 1360 | `			}` |
|      18 | 1361 | `		}` |
|       2 | 1362 | `	}` |
|       - | 1363 | `	/* Object are equals */` |
|      26 | 1364 | `	return 0;` |
|     141 | 1365 | `}` |
|       - | 1366 | `/*` |
|       - | 1367 | ` * Dump a class instance and the store the dump in the BLOB given` |
|       - | 1368 | ` * as the first argument.` |
|       - | 1369 | ` * Note that only non-static/non-constants attribute are dumped.` |
|       - | 1370 | ` * This function is typically invoked when the user issue a call` |
|       - | 1371 | ` * to [var_dump(),var_export(),print_r(),...].` |
|       - | 1372 | ` * This function SXRET_OK on success. Any other return value including` |
|       - | 1373 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|       - | 1374 | ` */` |
|       - | 1375 | `/*` |
|       - | 1376 | `` * Return the `name` property value of an enum case instance (the case name),`` |
|       - | 1377 | ` * or 0 when unavailable. Shared by the var_dump/var_export/json/serialize` |
|       - | 1378 | ` * renderers, which all print enum cases as Class::CaseName forms.` |
|       - | 1379 | ` */` |
|       6 | 1380 | `PH7_PRIVATE ph7_value * PH7_EnumCaseNameValue(ph7_class_instance *pThis)` |
|       1 | 1381 | `{` |
|       - | 1382 | `	SyHashEntry *pEntry;` |
|       7 | 1383 | `	if( (pThis->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|     ! 0 | 1384 | `		return 0;` |
|       - | 1385 | `	}` |
|       7 | 1386 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)"name",sizeof("name")-1);` |
|       7 | 1387 | `	if( pEntry == 0 ){` |
|     ! 0 | 1388 | `		return 0;` |
|       - | 1389 | `	}` |
|       7 | 1390 | `	return PH7_ClassInstanceExtractAttrValue(pThis,(VmClassAttr *)pEntry->pUserData);` |
|       4 | 1391 | `}` |
|       - | 1392 | `/*` |
|       - | 1393 | `` * Return the `value` property value (the backing value) of an enum case`` |
|       - | 1394 | ` * instance, or 0 when unavailable (pure enums have none).` |
|       - | 1395 | ` */` |
|       8 | 1396 | `PH7_PRIVATE ph7_value * PH7_EnumCaseBackingValueOf(ph7_class_instance *pThis)` |
|       1 | 1397 | `{` |
|       - | 1398 | `	SyHashEntry *pEntry;` |
|       9 | 1399 | `	if( (pThis->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|     ! 0 | 1400 | `		return 0;` |
|       - | 1401 | `	}` |
|       9 | 1402 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)"value",sizeof("value")-1);` |
|       9 | 1403 | `	if( pEntry == 0 ){` |
|       3 | 1404 | `		return 0;` |
|       - | 1405 | `	}` |
|       7 | 1406 | `	return PH7_ClassInstanceExtractAttrValue(pThis,(VmClassAttr *)pEntry->pUserData);` |
|       5 | 1407 | `}` |
|       - | 1408 | `/*` |
|       - | 1409 | ` * Emit a class-instance dump header plus its trailing newline. For var_dump` |
|       - | 1410 | ` * (ShowType) it completes the "object(" prefix the caller already emitted as` |
|       - | 1411 | ` *   ClassName)#<id> (<count>) {` |
|       - | 1412 | ` * for print_r it emits the legacy PHL  Object(ClassName) {  (count/id unused).` |
|       - | 1413 | `` * Enum cases print php's `ClassName Enum {` print_r header (var_dump never`` |
|       - | 1414 | `` * reaches here for enums — PH7_MemObjDump prints `enum(S::A)` directly).`` |
|       - | 1415 | ` */` |
|     136 | 1416 | `static void DumpClassInstanceHeader(SyBlob *pOut,ph7_class *pClass,sxu32 nObjId,int ShowType,sxu32 nCount)` |
|       3 | 1417 | `{` |
|     139 | 1418 | `	if( ShowType ){` |
|       - | 1419 | ``		/* var_dump: `object(C)#id (n) {` */`` |
|     135 | 1420 | `		SyBlobFormat(&(*pOut),"object(%z)#%u (%u) {",&pClass->sName,nObjId,nCount);` |
|     135 | 1421 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|     135 | 1422 | `		return;` |
|       - | 1423 | `	}` |
|       - | 1424 | ``	/* print_r: `C Object` / `E Enum[:backing]` — the '(' line is emitted by`` |
|       - | 1425 | `	 * the body renderer at the container indent. */` |
|       6 | 1426 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|     ! 0 | 1427 | `		SyBlobFormat(&(*pOut),"%z Enum",&pClass->sName);` |
|     ! 0 | 1428 | `		if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|     ! 0 | 1429 | `			SyBlobAppend(&(*pOut),":int",sizeof(":int")-1);` |
|     ! 0 | 1430 | `		}else if( pClass->nEnumBacking == MEMOBJ_STRING ){` |
|     ! 0 | 1431 | `			SyBlobAppend(&(*pOut),":string",sizeof(":string")-1);` |
|     ! 0 | 1432 | `		}` |
|     ! 0 | 1433 | `	}else{` |
|       6 | 1434 | `		SyBlobFormat(&(*pOut),"%z Object",&pClass->sName);` |
|       - | 1435 | `	}` |
|       6 | 1436 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      71 | 1437 | `}` |
|       - | 1438 | `/*` |
|       - | 1439 | ` * The class that DECLARED pAttr: inheritance shares attr pointers down the` |
|       - | 1440 | ` * chain, so the declaring class is the most ANCESTRAL class whose hAttr still` |
|       - | 1441 | ` * maps the name to this exact pointer. php's var_dump/print_r use it for the` |
|       - | 1442 | `` * `["p":"Decl":private]` annotation.`` |
|       - | 1443 | ` */` |
|       6 | 1444 | `static ph7_class * OoAttrDeclaringClass(ph7_class *pClass,ph7_class_attr *pAttr)` |
|       1 | 1445 | `{` |
|       - | 1446 | `	/* Attrs record their declaring class at install time (inheritance/trait` |
|       - | 1447 | `	 * copies share the pointer, so the field survives the chain). */` |
|       7 | 1448 | `	return pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|       1 | 1449 | `}` |
|       - | 1450 | `/*` |
|       - | 1451 | `` * Emit a property's dump key: var_dump `["x"]=>` / `["p":"C":private]=>` /`` |
|       - | 1452 | `` * `["q":protected]=>`; print_r `[x] => ` / `[p:C:private] => ` /`` |
|       - | 1453 | `` * `[q:protected] => ` (php's exact annotations).`` |
|       - | 1454 | ` */` |
|     138 | 1455 | `static void OoDumpPropKey(SyBlob *pOut,ph7_class_instance *pThis,ph7_class_attr *pAttr,int ShowType)` |
|       2 | 1456 | `{` |
|     140 | 1457 | `	const char *zQ = ShowType ? "\"" : "";` |
|     140 | 1458 | `	SyBlobFormat(&(*pOut),"[%s%z%s",zQ,&pAttr->sName,zQ);` |
|     140 | 1459 | `	if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       7 | 1460 | `		ph7_class *pDecl = OoAttrDeclaringClass(pThis->pClass,pAttr);` |
|       7 | 1461 | `		SyBlobFormat(&(*pOut),":%s%z%s:private",zQ,&pDecl->sName,zQ);` |
|     137 | 1462 | `	}else if( pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
|     ! 0 | 1463 | `		SyBlobAppend(&(*pOut),":protected",sizeof(":protected")-1);` |
|     ! 0 | 1464 | `	}` |
|     140 | 1465 | `	SyBlobAppend(&(*pOut),ShowType ? "]=>" : "] => ",ShowType ? 3 : 5);` |
|     140 | 1466 | `}` |
|     140 | 1467 | `PH7_PRIVATE sxi32 PH7_ClassInstanceDump(SyBlob *pOut,ph7_class_instance *pThis,int ShowType,int nTab,int nDepth)` |
|       3 | 1468 | `{` |
|       - | 1469 | `	SyHashEntry *pEntry;` |
|       - | 1470 | `	ph7_value *pValue;` |
|       - | 1471 | `	sxi32 rc;` |
|       - | 1472 | `	int i;` |
|     143 | 1473 | `	if( nDepth > 31 ){` |
|       - | 1474 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|       - | 1475 | `		/* Nesting limit reached..halt immediately*/` |
|       5 | 1476 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|       5 | 1477 | `		return SXERR_LIMIT;` |
|       - | 1478 | `	}` |
|     139 | 1479 | `	rc = SXRET_OK;` |
|       - | 1480 | `	{` |
|       - | 1481 | `		/* Both var_dump and print_r consult __debugInfo() (PHP behavior);` |
|       - | 1482 | `		 * var_export uses a separate renderer and never reaches here. When the` |
|       - | 1483 | `		 * method is present and returns an array, render that array's entries as` |
|       - | 1484 | `		 * the object body, with the header showing the debug array's count. The` |
|       - | 1485 | `		 * nDepth guard above protects against a __debugInfo returning the object` |
|       - | 1486 | `		 * itself. */` |
|     139 | 1487 | `		ph7_class_method *pDbg = PH7_ClassExtractMethod(pThis->pClass,"__debugInfo",sizeof("__debugInfo")-1);` |
|     139 | 1488 | `		if( pDbg ){` |
|       - | 1489 | `			ph7_value sResult;` |
|       5 | 1490 | `			PH7_MemObjInit(pThis->pVm,&sResult);` |
|       5 | 1491 | `			PH7_VmCallClassMethod(pThis->pVm,pThis,pDbg,&sResult,0,0);` |
|       5 | 1492 | `			if( sResult.iFlags & MEMOBJ_HASHMAP ){` |
|       5 | 1493 | `				ph7_hashmap *pMap = (ph7_hashmap *)sResult.x.pOther;` |
|       - | 1494 | `				/* Header count is the debug array's entry count. */` |
|       5 | 1495 | `				DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,pMap->nEntry);` |
|       5 | 1496 | `				if( !ShowType ){` |
|       3 | 1497 | `					for( i = 0 ; i < nTab ; i++ ){` |
|     ! 0 | 1498 | `						SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     ! 0 | 1499 | `					}` |
|       3 | 1500 | `					SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       1 | 1501 | `				}` |
|       5 | 1502 | `				rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth);` |
|       5 | 1503 | `				for( i = 0 ; i < nTab ; i++ ){` |
|     ! 0 | 1504 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     ! 0 | 1505 | `				}` |
|       5 | 1506 | `				if( ShowType ){` |
|       3 | 1507 | `					SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|       2 | 1508 | `				}else{` |
|       3 | 1509 | `					SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       - | 1510 | `				}` |
|       5 | 1511 | `				PH7_MemObjRelease(&sResult);` |
|       5 | 1512 | `				return rc;` |
|       - | 1513 | `			}` |
|       - | 1514 | `			/* Non-array return: behave as if __debugInfo were absent. */` |
|     ! 0 | 1515 | `			PH7_MemObjRelease(&sResult);` |
|     ! 0 | 1516 | `		}` |
|       - | 1517 | `	}` |
|       - | 1518 | `	{` |
|       - | 1519 | `		/* var_dump's header needs the property count up front, so pre-count the` |
|       - | 1520 | `		 * non-static/non-constant attributes (matching the dump loop below). */` |
|     134 | 1521 | `		sxu32 nProp = 0;` |
|     134 | 1522 | `		if( ShowType ){` |
|     132 | 1523 | `			SyHashResetLoopCursor(&pThis->hAttr);` |
|     270 | 1524 | `			while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|     140 | 1525 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     140 | 1526 | `				if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_HOOK_VIRTUAL)) == 0 ){` |
|     136 | 1527 | `					nProp++;` |
|      67 | 1528 | `				}` |
|       2 | 1529 | `			}` |
|      65 | 1530 | `		}` |
|     134 | 1531 | `		DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,nProp);` |
|       - | 1532 | `	}` |
|     134 | 1533 | `	if( !ShowType ){` |
|       - | 1534 | `		/* print_r body opener: '(' at the container indent */` |
|       3 | 1535 | `		for( i = 0 ; i < nTab ; i++ ){` |
|     ! 0 | 1536 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     ! 0 | 1537 | `		}` |
|       3 | 1538 | `		SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       1 | 1539 | `	}` |
|       - | 1540 | `	/* Dump object attributes (php 8.4: VIRTUAL hooked properties have no` |
|       - | 1541 | `	 * backing store — excluded from var_dump/print_r) */` |
|     134 | 1542 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|     218 | 1543 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|     144 | 1544 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     144 | 1545 | `		if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_HOOK_VIRTUAL)) == 0 ){` |
|       - | 1546 | `			/* Dump non-static/constant attribute only */` |
|     140 | 1547 | `			pValue = ExtractClassAttrValue(pThis->pVm,pVmAttr);` |
|     140 | 1548 | `			if( pValue == 0 ){` |
|     ! 0 | 1549 | `				continue;` |
|       - | 1550 | `			}` |
|     140 | 1551 | `			if( ShowType ){` |
|       - | 1552 | ``				/* var_dump prop: `["x"(:…)]=>` at nTab+2, the value on the next`` |
|       - | 1553 | `				 * line at the same indent (php). */` |
|    4124 | 1554 | `				for( i = 0 ; i < nTab + 2 ; i++ ){` |
|    3990 | 1555 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|    1996 | 1556 | `				}` |
|     136 | 1557 | `				OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,TRUE);` |
|     136 | 1558 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|     136 | 1559 | `				rc = PH7_MemObjDump(&(*pOut),pValue,TRUE,nTab+2,nDepth,0);` |
|     136 | 1560 | `				if( rc == SXERR_LIMIT ){` |
|     125 | 1561 | `					break;` |
|       - | 1562 | `				}` |
|       7 | 1563 | `			}else{` |
|       - | 1564 | ``				/* print_r prop: `[x(:…)] => value` at nTab+4; container values`` |
|       - | 1565 | `				 * render their block at nTab+8 followed by php's blank line. */` |
|      21 | 1566 | `				for( i = 0 ; i < nTab + 4 ; i++ ){` |
|      17 | 1567 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       9 | 1568 | `				}` |
|       5 | 1569 | `				OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,FALSE);` |
|       4 | 1570 | `				if( (pValue->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ))` |
|       3 | 1571 | `				 && (pValue->iFlags & MEMOBJ_NULL) == 0 ){` |
|     ! 0 | 1572 | `					rc = PH7_MemObjDump(&(*pOut),pValue,FALSE,nTab+8,nDepth,0);` |
|     ! 0 | 1573 | `					SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|     ! 0 | 1574 | `					if( rc == SXERR_LIMIT ){` |
|     ! 0 | 1575 | `						break;` |
|       - | 1576 | `					}` |
|     ! 0 | 1577 | `				}else{` |
|       5 | 1578 | `					PH7_MemObjPrintRInline(&(*pOut),pValue);` |
|       5 | 1579 | `					SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 1580 | `				}` |
|       - | 1581 | `			}` |
|       7 | 1582 | `		}` |
|       2 | 1583 | `	}` |
|    3854 | 1584 | `	for( i = 0 ; i < nTab ; i++ ){` |
|    3721 | 1585 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|    1861 | 1586 | `	}` |
|     134 | 1587 | `	if( ShowType ){` |
|     132 | 1588 | `		SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      67 | 1589 | `	}else{` |
|       3 | 1590 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       - | 1591 | `	}` |
|     134 | 1592 | `	return rc;` |
|      73 | 1593 | `}` |
|       - | 1594 | `/*` |
|       - | 1595 | ` * Call a magic method [i.e: __toString(),__toBool(),__Invoke()...]` |
|       - | 1596 | ` * Return SXRET_OK on successfull call. Any other return value indicates failure.` |
|       - | 1597 | ` * Notes on magic methods.` |
|       - | 1598 | ` * According to the PHP language reference manual.` |
|       - | 1599 | ` *  The function names __construct(), __destruct(), __call(), __callStatic()` |
|       - | 1600 | ` *  __get(),  __toString(), __invoke(), __clone() are magical in PHP classes.` |
|       - | 1601 | ` * You cannot have functions with these names in any of your classes unless` |
|       - | 1602 | ` * you want the magic functionality associated with them.` |
|       - | 1603 | ` * Example of magical methods:` |
|       - | 1604 | ` * __toString()` |
|       - | 1605 | ` *  The __toString() method allows a class to decide how it will react when it is treated like` |
|       - | 1606 | ` *  a string. For example, what echo $obj; will print. This method must return a string.` |
|       - | 1607 | ` *  Example #2 Simple example` |
|       - | 1608 | ` * <?php` |
|       - | 1609 | ` * // Declare a simple class` |
|       - | 1610 | ` * class TestClass` |
|       - | 1611 | ` * {` |
|       - | 1612 | ` *   public $foo;` |
|       - | 1613 | ` *` |
|       - | 1614 | ` *   public function __construct($foo)` |
|       - | 1615 | ` *   {` |
|       - | 1616 | ` *       $this->foo = $foo;` |
|       - | 1617 | ` *   }` |
|       - | 1618 | ` *` |
|       - | 1619 | ` *   public function __toString()` |
|       - | 1620 | ` *   {` |
|       - | 1621 | ` *       return $this->foo;` |
|       - | 1622 | ` *   }` |
|       - | 1623 | ` * }` |
|       - | 1624 | ` * $class = new TestClass('Hello');` |
|       - | 1625 | ` * echo $class;` |
|       - | 1626 | ` * ?>` |
|       - | 1627 | ` * The above example will output:` |
|       - | 1628 | ` *  Hello` |
|       - | 1629 | ` *` |
|       - | 1630 | ` * Note that PH7 does not support all the magical method and introudces __toFloat(),__toInt()` |
|       - | 1631 | ` * which have the same behaviour as __toString() but for float and integer types` |
|       - | 1632 | ` * respectively.` |
|       - | 1633 | ` * Refer to the official documentation for more information.` |
|       - | 1634 | ` */` |
|      54 | 1635 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallMagicMethod(` |
|       - | 1636 | `	ph7_vm *pVm,               /* VM that own all this stuff */` |
|       - | 1637 | `	ph7_class *pClass,         /* Target class */` |
|       - | 1638 | `	ph7_class_instance *pThis, /* Target object */` |
|       - | 1639 | `	const char *zMethod,       /* Magic method name [i.e: __toString()]*/` |
|       - | 1640 | `	sxu32 nByte,               /* zMethod length*/` |
|       - | 1641 | `	const SyString *pAttrName, /* Attribute name */` |
|       - | 1642 | `	ph7_value *pResult         /* OUT: magic method return value. NULL to discard */` |
|       - | 1643 | `	)` |
|       1 | 1644 | `{` |
|      55 | 1645 | `	ph7_value *apArg[2] = { 0 , 0 };` |
|       - | 1646 | `	ph7_class_method *pMeth;` |
|       - | 1647 | `	ph7_value sAttr; /* cc warning */` |
|       - | 1648 | `	sxi32 rc;` |
|       - | 1649 | `	int nArg;` |
|       - | 1650 | `	/* Make sure the magic method is available */` |
|      55 | 1651 | `	pMeth = PH7_ClassExtractMethod(&(*pClass),zMethod,nByte);` |
|      55 | 1652 | `	if( pMeth == 0 ){` |
|       - | 1653 | `		/* No such method,return immediately */` |
|     ! 0 | 1654 | `		return SXERR_NOTFOUND;` |
|       - | 1655 | `	}` |
|      55 | 1656 | `	nArg = 0;` |
|       - | 1657 | `	/* Copy arguments */` |
|      55 | 1658 | `	if( pAttrName ){` |
|      55 | 1659 | `		PH7_MemObjInitFromString(pVm,&sAttr,pAttrName);` |
|      55 | 1660 | `		sAttr.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      55 | 1661 | `		apArg[0] = &sAttr;` |
|      55 | 1662 | `		nArg = 1;` |
|      27 | 1663 | `	}` |
|       - | 1664 | `	/* Call the magic method now */` |
|      55 | 1665 | `	rc = PH7_VmCallClassMethod(pVm,&(*pThis),pMeth,pResult,nArg,apArg);` |
|       - | 1666 | `	/* Clean up */` |
|      55 | 1667 | `	if( pAttrName ){` |
|      55 | 1668 | `		PH7_MemObjRelease(&sAttr);` |
|      27 | 1669 | `	}` |
|      55 | 1670 | `	return rc;` |
|      28 | 1671 | `}` |
|       - | 1672 | `/*` |
|       - | 1673 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon].` |
|       - | 1674 | ` * This function is simply a wrapper on ExtractClassAttrValue().` |
|       - | 1675 | ` */` |
|    8050 | 1676 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceExtractAttrValue(ph7_class_instance *pThis,VmClassAttr *pAttr)` |
|       5 | 1677 | `{` |
|       - | 1678 | `   /* Extract the attribute value */` |
|       - | 1679 | `	ph7_value *pValue;` |
|    8055 | 1680 | `	pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|    8055 | 1681 | `	return pValue;` |
|       5 | 1682 | `}` |
|       - | 1683 | `/*` |
|       - | 1684 | ` * Convert a class instance [i.e: Object in the PHP jargon] into a hashmap [i.e: array in the PHP jargon].` |
|       - | 1685 | ` * Return SXRET_OK on success. Any other value indicates failure.` |
|       - | 1686 | ` * Note on object conversion to array:` |
|       - | 1687 | ` *  Acccording to the PHP language reference manual` |
|       - | 1688 | ` *  If an object is converted to an array, the result is an array whose elements are the object's properties.` |
|       - | 1689 | ` *  The keys are the member variable names.` |
|       - | 1690 | ` *` |
|       - | 1691 | ` *  The following example:` |
|       - | 1692 | ` *  class Test {` |
|       - | 1693 | ` *   public $A = 25<<1;  // 50` |
|       - | 1694 | ` *	 public $c = rand_str(3);   // Random string of length 3` |
|       - | 1695 | ` *	 public $d = rand() & 1023; // Random number between 0..1023` |
|       - | 1696 | ` *  }` |
|       - | 1697 | ` *  var_dump((array) new Test());` |
|       - | 1698 | ` *	Will output:` |
|       - | 1699 | ` *  array(3) {` |
|       - | 1700 | ` *   [A] =>` |
|       - | 1701 | ` *      int(50)` |
|       - | 1702 | ` *   [c] =>` |
|       - | 1703 | ` *     string(3 'aps')` |
|       - | 1704 | ` *   [d] =>` |
|       - | 1705 | ` *     int(991)` |
|       - | 1706 | ` *  }` |
|       - | 1707 | ` * You have noticed that PH7 allow class attributes [i.e: $a,$c,$d in the example above]` |
|       - | 1708 | ` * have any complex expression (even function calls/Annonymous functions) as their default` |
|       - | 1709 | ` * value unlike the standard PHP engine.` |
|       - | 1710 | ` * This is a very powerful feature that you have to look at.` |
|       - | 1711 | ` */` |
|      14 | 1712 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|       1 | 1713 | `{` |
|       - | 1714 | `	SyHashEntry *pEntry;` |
|       - | 1715 | `	SyString *pAttrName;` |
|       - | 1716 | `	VmClassAttr *pAttr;` |
|       - | 1717 | `	ph7_value *pValue;` |
|       - | 1718 | `	ph7_value sName;` |
|       - | 1719 | `	/* Reset the loop cursor */` |
|      15 | 1720 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      15 | 1721 | `	PH7_MemObjInitFromString(pThis->pVm,&sName,0);` |
|      51 | 1722 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       - | 1723 | `		/* Point to the current attribute */` |
|      37 | 1724 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      37 | 1725 | `		if( pAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|       - | 1726 | `			/* php 8.4: a VIRTUAL hooked property has no backing store — the` |
|       - | 1727 | `			 * (array) cast excludes it (raw surface, get is NOT dispatched) */` |
|       7 | 1728 | `			continue;` |
|       - | 1729 | `		}` |
|       - | 1730 | `		/* Extract attribute value */` |
|      31 | 1731 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|      31 | 1732 | `		if( pValue ){` |
|       - | 1733 | `			/* Build attribute name */` |
|      31 | 1734 | `			pAttrName = &pAttr->pAttr->sName;` |
|      31 | 1735 | `			PH7_MemObjStringAppend(&sName,pAttrName->zString,pAttrName->nByte);` |
|       - | 1736 | `			/* Perform the insertion */` |
|      31 | 1737 | `			PH7_HashmapInsert(pMap,&sName,pValue);` |
|       - | 1738 | `			/* Reset the string cursor */` |
|      31 | 1739 | `			SyBlobReset(&sName.sBlob);` |
|      15 | 1740 | `		}` |
|       1 | 1741 | `	}` |
|      15 | 1742 | `	PH7_MemObjRelease(&sName);` |
|      15 | 1743 | `	return SXRET_OK;` |
|       1 | 1744 | `}` |
|       - | 1745 | `/*` |
|       - | 1746 | ` * Iterate throw class attributes and invoke the given callback [i.e: xWalk()] for each` |
|       - | 1747 | ` * retrieved attribute.` |
|       - | 1748 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|       - | 1749 | ` * the attribute value in the callback body will not alter the real attribute value.` |
|       - | 1750 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|       - | 1751 | ` * a value different from PH7_OK.` |
|       - | 1752 | ` * Refer to [ph7_object_walk()] for more information.` |
|       - | 1753 | ` */` |
|     ! 0 | 1754 | `PH7_PRIVATE sxi32 PH7_ClassInstanceWalk(` |
|       - | 1755 | `	ph7_class_instance *pThis, /* Target object */` |
|       - | 1756 | `	int (*xWalk)(const char *,ph7_value *,void *), /* Walker callback */` |
|       - | 1757 | `	void *pUserData /* Last argument to xWalk() */` |
|       - | 1758 | `	)` |
|     ! 0 | 1759 | `{` |
|       - | 1760 | `	SyHashEntry *pEntry; /* Hash entry */` |
|       - | 1761 | `	VmClassAttr *pAttr;  /* Pointer to the attribute */` |
|       - | 1762 | `	ph7_value *pValue;   /* Attribute value */` |
|       - | 1763 | `	ph7_value sValue;    /* Copy of the attribute value */` |
|       - | 1764 | `	int rc;` |
|       - | 1765 | `	/* Reset the loop cursor */` |
|     ! 0 | 1766 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|     ! 0 | 1767 | `	PH7_MemObjInit(pThis->pVm,&sValue);` |
|       - | 1768 | `	/* Start the walk process */` |
|     ! 0 | 1769 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       - | 1770 | `		/* Point to the current attribute */` |
|     ! 0 | 1771 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1772 | `		/* Extract attribute value */` |
|     ! 0 | 1773 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     ! 0 | 1774 | `		if( pValue ){` |
|     ! 0 | 1775 | `			PH7_MemObjLoad(pValue,&sValue);` |
|       - | 1776 | `			/* Invoke the supplied callback */` |
|     ! 0 | 1777 | `			rc =  xWalk(SyStringData(&pAttr->pAttr->sName),&sValue,pUserData);` |
|     ! 0 | 1778 | `			PH7_MemObjRelease(&sValue);` |
|     ! 0 | 1779 | `			if( rc != PH7_OK){` |
|       - | 1780 | `				/* User callback request an operation abort */` |
|     ! 0 | 1781 | `				return SXERR_ABORT;` |
|       - | 1782 | `			}` |
|     ! 0 | 1783 | `		}` |
|     ! 0 | 1784 | `	}` |
|       - | 1785 | `	/* All done */` |
|     ! 0 | 1786 | `	return SXRET_OK;` |
|     ! 0 | 1787 | `}` |
|       - | 1788 | `/*` |
|       - | 1789 | ` * Extract a class atrribute value.` |
|       - | 1790 | ` * Return a pointer to the attribute value on success. Otherwise NULL.` |
|       - | 1791 | ` * Note:` |
|       - | 1792 | ` *  Access to static and constant attribute is not allowed. That is,the function` |
|       - | 1793 | ` *  will return NULL in case someone (host-application code) try to extract` |
|       - | 1794 | ` *  a static/constant attribute.` |
|       - | 1795 | ` */` |
|   10686 | 1796 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceFetchAttr(ph7_class_instance *pThis,const SyString *pName)` |
|       5 | 1797 | `{` |
|       - | 1798 | `	SyHashEntry *pEntry;` |
|       - | 1799 | `	VmClassAttr *pAttr;` |
|       - | 1800 | `	/* Query the attribute hashtable */` |
|   10691 | 1801 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);` |
|   10691 | 1802 | `	if( pEntry == 0 ){` |
|       - | 1803 | `		/* No such attribute */` |
|     ! 0 | 1804 | `		return 0;` |
|       - | 1805 | `	}` |
|       - | 1806 | `	/* Point to the class atrribute */` |
|   10691 | 1807 | `	pAttr = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1808 | `	/* Check if we are dealing with a static/constant attribute */` |
|   10691 | 1809 | `	if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|       - | 1810 | `		/* Access is forbidden */` |
|     ! 0 | 1811 | `		return 0;` |
|       - | 1812 | `	}` |
|       - | 1813 | `	/* Return the attribute value */` |
|   10691 | 1814 | `	return ExtractClassAttrValue(pThis->pVm,pAttr);` |
|    5348 | 1815 | `}` |
|       - | 1816 |  |
