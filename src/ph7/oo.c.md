# src/ph7/oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 710/806 lines (88.09%)

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
|  468248 |   14 | `PH7_PRIVATE ph7_class * PH7_NewRawClass(ph7_vm *pVm,const SyString *pName,sxu32 nLine)` |
|       5 |   15 | `{` |
|       - |   16 | `	ph7_class *pClass;` |
|       - |   17 | `	char *zName;` |
|       - |   18 | `	/* Allocate a new instance */` |
|  468253 |   19 | `	pClass = (ph7_class *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class));` |
|  468253 |   20 | `	if( pClass == 0 ){` |
|     ! 0 |   21 | `		return 0;` |
|       - |   22 | `	}` |
|       - |   23 | `	/* Zero the structure */` |
|  468253 |   24 | `	SyZero(pClass,sizeof(ph7_class));` |
|       - |   25 | `	/* Duplicate class name */` |
|  468253 |   26 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  468253 |   27 | `	if( zName == 0 ){` |
|     ! 0 |   28 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClass);` |
|     ! 0 |   29 | `		return 0;` |
|       - |   30 | `	}` |
|       - |   31 | `	/* Initialize fields */` |
|  468253 |   32 | `	SyStringInitFromBuf(&pClass->sName,zName,pName->nByte);` |
|  468253 |   33 | `	SyHashInit(&pClass->hMethod,&pVm->sAllocator,0,0);` |
|  468253 |   34 | `	SyHashInit(&pClass->hAttr,&pVm->sAllocator,0,0);` |
|  468253 |   35 | `	SyHashInit(&pClass->hDerived,&pVm->sAllocator,0,0);` |
|  468253 |   36 | `	SySetInit(&pClass->aInterface,&pVm->sAllocator,sizeof(ph7_class *));` |
|  468253 |   37 | `	SySetInit(&pClass->aTrait,&pVm->sAllocator,sizeof(ph7_class *));` |
|  468253 |   38 | `	SySetInit(&pClass->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|  468253 |   39 | `	SySetInit(&pClass->aEnumCases,&pVm->sAllocator,sizeof(ph7_class_attr *));` |
|  468253 |   40 | `	pClass->nLine = nLine;` |
|  468253 |   41 | `	if( pVm->bCompilingBuiltin ){` |
|       - |   42 | `		/* Defined by an embedded builtin chunk: internal, no defining file.` |
|       - |   43 | `		 * Class compilers merge further flags with \|= so this survives. */` |
|  466085 |   44 | `		pClass->iFlags \|= PH7_CLASS_INTERNAL;` |
|  233045 |   45 | `	}else{` |
|       - |   46 | `		/* Alias the VM-lifetime path dup on top of the include stack */` |
|    2173 |   47 | `		SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|    2173 |   48 | `		if( pFile ){` |
|    2173 |   49 | `			SyStringDupPtr(&pClass->sFile,pFile);` |
|    1084 |   50 | `		}` |
|       - |   51 | `	}` |
|       - |   52 | `	/* All done */` |
|  468253 |   53 | `	return pClass;` |
|  234129 |   54 | `}` |
|       - |   55 | `/*` |
|       - |   56 | ` * Allocate and initialize a new class attribute.` |
|       - |   57 | ` * Return a pointer to the class attribute on success. NULL otherwise.` |
|       - |   58 | ` */` |
|  886964 |   59 | `PH7_PRIVATE ph7_class_attr * PH7_NewClassAttr(ph7_vm *pVm,const SyString *pName,sxu32 nLine,sxi32 iProtection,sxi32 iFlags)` |
|       5 |   60 | `{` |
|       - |   61 | `	ph7_class_attr *pAttr;` |
|       - |   62 | `	char *zName;` |
|  886969 |   63 | `	pAttr = (ph7_class_attr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_attr));` |
|  886969 |   64 | `	if( pAttr == 0 ){` |
|     ! 0 |   65 | `		return 0;` |
|       - |   66 | `	}` |
|       - |   67 | `	/* Zero the structure */` |
|  886969 |   68 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|  886969 |   69 | `	SySetInit(&pAttr->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|       - |   70 | `	/* Duplicate attribute name */` |
|  886969 |   71 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  886969 |   72 | `	if( zName == 0 ){` |
|     ! 0 |   73 | `		SyMemBackendPoolFree(&pVm->sAllocator,pAttr);` |
|     ! 0 |   74 | `		return 0;` |
|       - |   75 | `	}` |
|       - |   76 | `	/* Initialize fields */` |
|  886969 |   77 | `	SySetInit(&pAttr->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|  886969 |   78 | `	SySetInit(&pAttr->aUnionAlts,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|  886969 |   79 | `	SyStringInitFromBuf(&pAttr->sName,zName,pName->nByte);` |
|  886969 |   80 | `	pAttr->iProtection = iProtection;` |
|  886969 |   81 | `	pAttr->nIdx = SXU32_HIGH;` |
|  886969 |   82 | `	pAttr->iFlags = iFlags;` |
|  886969 |   83 | `	pAttr->nLine = nLine;` |
|  886969 |   84 | `	return pAttr;` |
|  443487 |   85 | `}` |
|       - |   86 | `/*` |
|       - |   87 | ` * Allocate and initialize a new class method.` |
|       - |   88 | ` * Return a pointer to the class method on success. NULL otherwise` |
|       - |   89 | ` * This function associate with the newly created method an automatically generated` |
|       - |   90 | ` * random unique name.` |
|       - |   91 | ` */` |
| 2647030 |   92 | `PH7_PRIVATE ph7_class_method * PH7_NewClassMethod(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,sxu32 nLine,` |
|       - |   93 | `	sxi32 iProtection,sxi32 iFlags,sxi32 iFuncFlags)` |
|       5 |   94 | `{` |
|       - |   95 | `	ph7_class_method *pMeth;` |
|       - |   96 | `	SyHashEntry *pEntry;` |
|       - |   97 | `	SyString *pNamePtr;` |
|       - |   98 | `	char zSalt[10];` |
|       - |   99 | `	char *zName;` |
|       - |  100 | `	sxu32 nByte;` |
|       - |  101 | `	/* Allocate a new class method instance */` |
| 2647035 |  102 | `	pMeth = (ph7_class_method *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_method));` |
| 2647035 |  103 | `	if( pMeth == 0 ){` |
|     ! 0 |  104 | `		return 0;` |
|       - |  105 | `	}` |
|       - |  106 | `	/* Zero the structure */` |
| 2647035 |  107 | `	SyZero(pMeth,sizeof(ph7_class_method));` |
|       - |  108 | `	/* Check for an already installed method with the same name */` |
| 2647035 |  109 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
| 2647035 |  110 | `	if( pEntry == 0 ){` |
|       - |  111 | `		/* Associate an unique VM name to this method */` |
| 2647033 |  112 | `		nByte = sizeof(zSalt) + pName->nByte + SyStringLength(&pClass->sName)+sizeof(char)*7/*[[__'\0'*/;` |
| 2647033 |  113 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,nByte);` |
| 2647033 |  114 | `		if( zName == 0 ){` |
|     ! 0 |  115 | `			SyMemBackendPoolFree(&pVm->sAllocator,pMeth);` |
|     ! 0 |  116 | `			return 0;` |
|       - |  117 | `		}` |
| 2647033 |  118 | `		pNamePtr = &pMeth->sVmName;` |
|       - |  119 | `		/* Generate a random string */` |
| 2647033 |  120 | `		PH7_VmRandomString(&(*pVm),zSalt,sizeof(zSalt));` |
| 2647033 |  121 | `		pNamePtr->nByte = SyBufferFormat(zName,nByte,"[__%z@%z_%.*s]",&pClass->sName,pName,sizeof(zSalt),zSalt);` |
| 2647033 |  122 | `		pNamePtr->zString = zName;` |
| 1323519 |  123 | `	}else{` |
|       - |  124 | `		/* Method is condidate for 'overloading' */` |
|       3 |  125 | `		ph7_class_method *pCurrent = (ph7_class_method *)pEntry->pUserData;` |
|       3 |  126 | `		pNamePtr = &pMeth->sVmName;` |
|       - |  127 | `		/* Use the same VM name */` |
|       3 |  128 | `		SyStringDupPtr(pNamePtr,&pCurrent->sVmName);` |
|       3 |  129 | `		zName = (char *)pNamePtr->zString;` |
|       - |  130 | `	}` |
| 2647035 |  131 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|  163206 |  132 | `		if( (pName->nByte == sizeof("__destruct") - 1 && SyMemcmp(pName->zString,"__destruct",sizeof("__destruct") - 1 ) == 0)` |
|  163211 |  133 | `			\|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|       - |  134 | `				/* Switch to public visibility for destructors and legacy class-name` |
|       - |  135 | `				 * constructors (the engine invokes destructors internally, bypassing` |
|       - |  136 | `				 * visibility either way). __construct KEEPS its declared visibility` |
|       - |  137 | ``				 * (band A #4): php enforces it at `new` — a private/protected ctor`` |
|       - |  138 | `				 * from the wrong scope is a catchable Error, checked at OP_NEW —` |
|       - |  139 | `				 * and ReflectionClass::isInstantiable()/newInstance() now see it. */` |
|     ! 0 |  140 | `				iProtection = PH7_CLASS_PROT_PUBLIC;` |
|     ! 0 |  141 | `		}` |
|   81603 |  142 | `	}` |
|       - |  143 | `	/* Initialize method fields */` |
| 2647035 |  144 | `	pMeth->iProtection = iProtection;` |
| 2647035 |  145 | `	pMeth->iFlags = iFlags;` |
| 2647035 |  146 | `	pMeth->nLine = nLine;` |
| 3970550 |  147 | `	PH7_VmInitFuncState(&(*pVm),&pMeth->sFunc,&zName[sizeof(char)*4/*[__@*/+SyStringLength(&pClass->sName)],` |
| 2647030 |  148 | `		pName->nByte,iFuncFlags\|VM_FUNC_CLASS_METHOD,pClass);` |
| 2647035 |  149 | `	return pMeth;` |
| 1323520 |  150 | `}` |
|       - |  151 | `/*` |
|       - |  152 | ` * Check if the given name have a class method associated with it.` |
|       - |  153 | ` * Return the desired method [i.e: ph7_class_method instance] on success. NULL otherwise.` |
|       - |  154 | ` */` |
|  862118 |  155 | `PH7_PRIVATE ph7_class_method * PH7_ClassExtractMethod(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|       5 |  156 | `{` |
|       - |  157 | `	SyHashEntry *pEntry;` |
|       - |  158 | `	/* Perform a hash lookup */` |
|  862123 |  159 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)zName,nByte);` |
|  862123 |  160 | `	if( pEntry == 0 ){` |
|       - |  161 | `		/* No such entry */` |
|   29621 |  162 | `		return 0;` |
|       - |  163 | `	}` |
|       - |  164 | `	/* Point to the desired method */` |
|  832507 |  165 | `	return (ph7_class_method *)pEntry->pUserData;` |
|  431064 |  166 | `}` |
|       - |  167 | `/*` |
|       - |  168 | ` * Check if the given name is a class attribute.` |
|       - |  169 | ` * Return the desired attribute [i.e: ph7_class_attr instance] on success.NULL otherwise.` |
|       - |  170 | ` */` |
|  530846 |  171 | `PH7_PRIVATE ph7_class_attr * PH7_ClassExtractAttribute(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|       5 |  172 | `{` |
|       - |  173 | `	SyHashEntry *pEntry;` |
|       - |  174 | `	/* Perform a hash lookup */` |
|  530851 |  175 | `	pEntry = SyHashGet(&pClass->hAttr,(const void *)zName,nByte);` |
|  530851 |  176 | `	if( pEntry == 0 ){` |
|       - |  177 | `		/* No such entry */` |
|  529451 |  178 | `		return 0;` |
|       - |  179 | `	}` |
|       - |  180 | `	/* Point to the desierd method */` |
|    1405 |  181 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|  265428 |  182 | `}` |
|       - |  183 | `/*` |
|       - |  184 | ` * Install a class attribute in the corresponding container.` |
|       - |  185 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|       - |  186 | ` */` |
|  886960 |  187 | `PH7_PRIVATE sxi32 PH7_ClassInstallAttr(ph7_class *pClass,ph7_class_attr *pAttr)` |
|       5 |  188 | `{` |
|  886965 |  189 | `	SyString *pName = &pAttr->sName;` |
|       - |  190 | `	sxi32 rc;` |
|       - |  191 | `	/* Remember where this attribute was originally declared so that later` |
|       - |  192 | `	 * inheritance/trait copies still know the declaring class (needed for` |
|       - |  193 | `	 * PHP-compatible error messages on typed properties). */` |
|  886965 |  194 | `	if( pAttr->pDeclClass == 0 ){` |
|  886965 |  195 | `		pAttr->pDeclClass = pClass;` |
|  443480 |  196 | `	}` |
|  886965 |  197 | `	rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|  886965 |  198 | `	return rc;` |
|       5 |  199 | `}` |
|       - |  200 | `/*` |
|       - |  201 | ` * Install a class method in the corresponding container.` |
|       - |  202 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|       - |  203 | ` */` |
| 2647018 |  204 | `PH7_PRIVATE sxi32 PH7_ClassInstallMethod(ph7_class *pClass,ph7_class_method *pMeth)` |
|       5 |  205 | `{` |
| 2647023 |  206 | `	SyString *pName = &pMeth->sFunc.sName;` |
|       - |  207 | `	sxi32 rc;` |
| 2647023 |  208 | `	rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 2647023 |  209 | `	return rc;` |
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
|  202200 |  234 | `static int OoClassifyOverrideType(ph7_vm *pVm, sxu32 nType, const SyString *pClass,` |
|       - |  235 | `	int bUnion, ph7_class **ppClass)` |
|       5 |  236 | `{` |
|  202205 |  237 | `	*ppClass = 0;` |
|  202205 |  238 | `	if( bUnion ){` |
|       3 |  239 | `		return OVT_SKIP; /* union/intersection — full lattice, skip */` |
|       - |  240 | `	}` |
|  202203 |  241 | `	if( nType == 0 ){` |
|  202099 |  242 | `		return OVT_NONE; /* no declared type */` |
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
|  101105 |  274 | `}` |
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
|  171064 |  288 | `static OvType OoTypeFromReturn(ph7_vm_func *pF)` |
|       5 |  289 | `{` |
|       - |  290 | `	OvType t;` |
|  171069 |  291 | `	t.nType = pF->nReturnType;` |
|  171069 |  292 | `	t.pClass = &pF->sReturnClass;` |
|  171069 |  293 | `	t.bUnion = SySetUsed(&pF->aReturnUnion) > 0;` |
|  171069 |  294 | `	t.bNullable = (pF->iFlags & VM_FUNC_RETURN_NULLABLE) != 0;` |
|  171069 |  295 | `	return t;` |
|       5 |  296 | `}` |
|   31136 |  297 | `static OvType OoTypeFromArg(ph7_vm_func_arg *pA)` |
|       5 |  298 | `{` |
|       - |  299 | `	OvType t;` |
|   31141 |  300 | `	t.nType = pA->nType;` |
|   31141 |  301 | `	t.pClass = &pA->sClass;` |
|   31141 |  302 | `	t.bUnion = (pA->iFlags & VM_FUNC_ARG_UNION) != 0;` |
|   31141 |  303 | `	t.bNullable = (pA->iFlags & VM_FUNC_ARG_NULLABLE) != 0;` |
|   31141 |  304 | `	return t;` |
|       5 |  305 | `}` |
|       - |  306 | `/*` |
|       - |  307 | ` * Return TRUE if the child type is an unambiguously-invalid override of the` |
|       - |  308 | ` * parent type. bCovariant=1 for a return type (child must be ⊆ parent),` |
|       - |  309 | ` * 0 for a parameter (child must be ⊇ parent). Returns FALSE (accept) on any` |
|       - |  310 | ` * skipped/ambiguous shape.` |
|       - |  311 | ` */` |
|  101100 |  312 | `static int OoOverrideTypeBad(ph7_vm *pVm, OvType parent, OvType child, int bCovariant)` |
|       5 |  313 | `{` |
|       - |  314 | `	ph7_class *pParentCls, *pChildCls;` |
|  101105 |  315 | `	int kP = OoClassifyOverrideType(pVm, parent.nType, parent.pClass, parent.bUnion, &pParentCls);` |
|  101105 |  316 | `	int kC = OoClassifyOverrideType(pVm, child.nType, child.pClass, child.bUnion, &pChildCls);` |
|  101105 |  317 | `	if( kP == OVT_SKIP \|\| kC == OVT_SKIP ){` |
|      21 |  318 | `		return 0; /* ambiguous shape — conservatively accept */` |
|       - |  319 | `	}` |
|       - |  320 | `	/* A missing type is the TOP type. covariant (return): a concrete child is a` |
|       - |  321 | `	 * subtype of top, fine; a top child over a concrete parent WIDENS → bad.` |
|       - |  322 | `	 * contravariant (param): a top child is a supertype of anything, fine; a` |
|       - |  323 | `	 * concrete child over a top parent NARROWS → bad. (A union/intersection child` |
|       - |  324 | `	 * already fell into OVT_SKIP above, so a flagged child here is scalar/class.) */` |
|  101087 |  325 | `	if( kP == OVT_NONE \|\| kC == OVT_NONE ){` |
|  101057 |  326 | `		if( bCovariant && kC == OVT_NONE && kP != OVT_NONE ) return 1;` |
|  101057 |  327 | `		if( !bCovariant && kP == OVT_NONE && kC != OVT_NONE ) return 1;` |
|  101057 |  328 | `		return 0;` |
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
|   50555 |  349 | `}` |
|       - |  350 |  |
|       - |  351 | `/*` |
|       - |  352 | ` * Check a child method's signature against the parent method it overrides.` |
|       - |  353 | ` * Emits a PHP-style "Declaration of … must be compatible …" fatal on a clear` |
|       - |  354 | `` * incompatibility. `__construct` is exempt (PHP does not apply variance to it).`` |
|       - |  355 | ` */` |
|  124380 |  356 | `static sxi32 OoCheckOverrideCompat(ph7_gen_state *pGen, ph7_class *pBase, ph7_class *pSub,` |
|       - |  357 | `	ph7_class_method *pParent, ph7_class_method *pChild)` |
|       5 |  358 | `{` |
|  124385 |  359 | `	ph7_vm *pVm = pGen->pVm;` |
|  124385 |  360 | `	ph7_vm_func *pPF = &pParent->sFunc;` |
|  124385 |  361 | `	ph7_vm_func *pCF = &pChild->sFunc;` |
|  124385 |  362 | `	SyString *pMName = &pCF->sName;` |
|       - |  363 | `	ph7_vm_func_arg *aP, *aC;` |
|       - |  364 | `	sxu32 nPArg, nCArg, k;` |
|  124385 |  365 | `	int bBad = 0;` |
|  124380 |  366 | `	if( pMName->nByte == sizeof("__construct")-1` |
|   83565 |  367 | `	 && SyStrnmicmp(pMName->zString,"__construct",pMName->nByte) == 0 ){` |
|   38853 |  368 | `		return SXRET_OK;` |
|       - |  369 | `	}` |
|       - |  370 | `	/* Return type — covariant. */` |
|   85537 |  371 | `	bBad = OoOverrideTypeBad(pVm, OoTypeFromReturn(pPF), OoTypeFromReturn(pCF), /* bCovariant */ 1);` |
|       - |  372 | `	/* Each overlapping parameter — contravariant. */` |
|   85537 |  373 | `	nPArg = SySetUsed(&pPF->aArgs);` |
|   85537 |  374 | `	nCArg = SySetUsed(&pCF->aArgs);` |
|   85537 |  375 | `	aP = (ph7_vm_func_arg *)SySetBasePtr(&pPF->aArgs);` |
|   85537 |  376 | `	aC = (ph7_vm_func_arg *)SySetBasePtr(&pCF->aArgs);` |
|  101105 |  377 | `	for( k = 0; !bBad && k < nPArg && k < nCArg; k++ ){` |
|   15573 |  378 | `		bBad = OoOverrideTypeBad(pVm, OoTypeFromArg(&aP[k]), OoTypeFromArg(&aC[k]), /* bCovariant */ 0);` |
|    7789 |  379 | `	}` |
|       - |  380 | `	/* Parameter arity: the child must declare at least the parent's parameters and` |
|       - |  381 | `	 * may add only OPTIONAL ones — PHP rejects dropping any param (even an optional` |
|       - |  382 | `	 * one) or adding a required one. Skip the rule if either signature is variadic` |
|       - |  383 | `	 * (arity semantics differ). */` |
|   85537 |  384 | `	if( !bBad ){` |
|   85533 |  385 | `		int bVariadic = 0;` |
|  101099 |  386 | `		for( k = 0; k < nPArg; k++ ){ if( aP[k].iFlags & VM_FUNC_ARG_VARIADIC ) bVariadic = 1; }` |
|  101101 |  387 | `		for( k = 0; k < nCArg; k++ ){ if( aC[k].iFlags & VM_FUNC_ARG_VARIADIC ) bVariadic = 1; }` |
|   85533 |  388 | `		if( !bVariadic ){` |
|   85533 |  389 | `			if( nCArg < nPArg ){` |
|     ! 0 |  390 | `				bBad = 1; /* dropped a parent parameter */` |
|     ! 0 |  391 | `			}else{` |
|   85535 |  392 | `				for( k = nPArg; k < nCArg; k++ ){` |
|       3 |  393 | `					if( SySetUsed(&aC[k].aByteCode) == 0 ){ bBad = 1; break; } /* new required */` |
|       2 |  394 | `				}` |
|       - |  395 | `			}` |
|   42764 |  396 | `		}` |
|   42764 |  397 | `	}` |
|   85537 |  398 | `	if( bBad ){` |
|       8 |  399 | `		sxi32 rc = PH7_GenCompileError(&(*pGen),E_ERROR,pChild->nLine,` |
|       - |  400 | `			"Declaration of %z::%z() must be compatible with %z::%z()",` |
|       2 |  401 | `			&pSub->sName,pMName,&pBase->sName,&pParent->sFunc.sName);` |
|       6 |  402 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  403 | `			return SXERR_ABORT;` |
|       - |  404 | `		}` |
|       2 |  405 | `	}` |
|   85537 |  406 | `	return SXRET_OK;` |
|   62195 |  407 | `}` |
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
|  206144 |  449 | `PH7_PRIVATE sxi32 PH7_ClassInherit(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase)` |
|       5 |  450 | `{` |
|       - |  451 | `	ph7_class_method *pMeth;` |
|       - |  452 | `	ph7_class_attr *pAttr;` |
|       - |  453 | `	SyHashEntry *pEntry;` |
|       - |  454 | `	SyString *pName;` |
|       - |  455 | `	sxi32 rc;` |
|       - |  456 | `	/* Install in the derived hashtable */` |
|  206149 |  457 | `	rc = SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|  206149 |  458 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  459 | `		return rc;` |
|       - |  460 | `	}` |
|       - |  461 | `	/* readonly class inheritance (PHP 8.2): a readonly class may only extend a` |
|       - |  462 | `	 * readonly class, and a non-readonly class may not extend a readonly one. */` |
|  206149 |  463 | `	if( (pBase->iFlags & PH7_CLASS_READONLY) != (pSub->iFlags & PH7_CLASS_READONLY) ){` |
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
|  206149 |  478 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
| 1232061 |  479 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|       - |  480 | `		/* Make sure the private attributes are not redeclared in the subclass */` |
| 1025917 |  481 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
| 1025917 |  482 | `		pName = &pAttr->sName;` |
| 1025917 |  483 | `		if( (pEntry = SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte)) != 0 ){` |
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
| 1025894 |  509 | `		if( pAttr->iProtection != PH7_CLASS_PROT_PRIVATE` |
|  658680 |  510 | `		 \|\| (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
| 1025893 |  511 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
| 1025893 |  512 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  513 | `				return rc;` |
|       - |  514 | `			}` |
|  512944 |  515 | `		}` |
|       5 |  516 | `	}` |
|  206149 |  517 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
| 3089093 |  518 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|       - |  519 | `		/* Make sure the private/final methods are not redeclared in the subclass */` |
| 2882949 |  520 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
| 2882949 |  521 | `		pName = &pMeth->sFunc.sName;` |
| 2882949 |  522 | `		if( (pEntry = SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|  124389 |  523 | `			 if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|       - |  524 | `				/* php: "Cannot override final method A::test()" */` |
|       8 |  525 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_method *)pEntry->pUserData)->nLine,` |
|       - |  526 | `					"Cannot override final method %z::%z()",` |
|       2 |  527 | `					&pBase->sName,pName);` |
|       2 |  528 | `				(void)pSub;` |
|       6 |  529 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  530 | `					return SXERR_ABORT;` |
|       - |  531 | `				}` |
|       4 |  532 | `			}else{` |
|       - |  533 | `				/* Check the override's signature is compatible with the parent's. */` |
|  186575 |  534 | `				rc = OoCheckOverrideCompat(&(*pGen),pBase,pSub,pMeth,` |
|  124380 |  535 | `					(ph7_class_method *)pEntry->pUserData);` |
|  124385 |  536 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  537 | `					return SXERR_ABORT;` |
|       - |  538 | `				}` |
|       - |  539 | `			}` |
|  124389 |  540 | `			continue;` |
|       - |  541 | `		}` |
|       - |  542 | `		/* Install the method. php: a base class's private INSTANCE method is` |
|       - |  543 | `		 * dispatchable on child instances too — an inherited public method` |
|       - |  544 | `		 * calling $this->priv() must find it (the call-site visibility check` |
|       - |  545 | `		 * binds by DECLARING class, sFunc.pUserData, so child code and` |
|       - |  546 | `		 * outsiders still can't call it; a private ctor copied down also` |
|       - |  547 | ``		 * blocks `new Child` from outside like php). Private STATICS stay`` |
|       - |  548 | `		 * uncopied — base methods reach those through self:: against the` |
|       - |  549 | `		 * declaring class directly. */` |
| 2758560 |  550 | `		if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE` |
| 1394830 |  551 | `		 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
| 2758565 |  552 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 2758565 |  553 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  554 | `				return rc;` |
|       - |  555 | `			}` |
| 1379280 |  556 | `		}` |
|       5 |  557 | `	}` |
|       - |  558 | `	/* Mark as subclass */` |
|  206149 |  559 | `	pSub->pBase = pBase;` |
|       - |  560 | `	/* All done */` |
|  206149 |  561 | `	return SXRET_OK;` |
|  103077 |  562 | `}` |
|       - |  563 | `/*` |
|       - |  564 | ` * Apply a trait to a class: copy all methods and attributes from the trait` |
|       - |  565 | ` * into the target class. Unlike inheritance, traits copy ALL members including` |
|       - |  566 | ` * private ones. Members already defined in the class take precedence.` |
|       - |  567 | ` */` |
|   15620 |  568 | `PH7_PRIVATE sxi32 PH7_ClassUseTrait(ph7_gen_state *pGen,ph7_class *pClass,ph7_class *pTrait)` |
|       5 |  569 | `{` |
|       - |  570 | `	ph7_class_method *pMeth;` |
|       - |  571 | `	ph7_class_attr *pAttr;` |
|       - |  572 | `	SyHashEntry *pEntry;` |
|       - |  573 | `	SyString *pName;` |
|       - |  574 | `	sxi32 rc;` |
|       - |  575 | `	/* Detect cyclic trait composition (e.g. trait A { use B; } trait B { use A; }) */` |
|   15625 |  576 | `	if( pTrait->iFlags & PH7_CLASS_TRAIT_VISITING ){` |
|     ! 0 |  577 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|     ! 0 |  578 | `			"Trait circular reference detected: %z is already being applied",&pTrait->sName);` |
|     ! 0 |  579 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  580 | `			return SXERR_ABORT;` |
|       - |  581 | `		}` |
|     ! 0 |  582 | `		return SXRET_OK;` |
|       - |  583 | `	}` |
|   15625 |  584 | `	pTrait->iFlags \|= PH7_CLASS_TRAIT_VISITING;` |
|   15625 |  585 | `	rc = SXRET_OK;` |
|       - |  586 | `	/* Copy attributes from the trait */` |
|   15625 |  587 | `	SyHashResetLoopCursor(&pTrait->hAttr);` |
|   62255 |  588 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hAttr)) != 0 ){` |
|       - |  589 | `		SyHashEntry *pExisting;` |
|   46635 |  590 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   46635 |  591 | `		pName = &pAttr->sName;` |
|   46635 |  592 | `		pExisting = SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte);` |
|   46635 |  593 | `		if( pExisting != 0 ){` |
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
|   46631 |  624 | `		rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|   46631 |  625 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  626 | `			goto cleanup;` |
|       - |  627 | `		}` |
|       5 |  628 | `	}` |
|       - |  629 | `	/* Copy methods from the trait */` |
|   15625 |  630 | `	SyHashResetLoopCursor(&pTrait->hMethod);` |
|  225455 |  631 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hMethod)) != 0 ){` |
|       - |  632 | `		SyHashEntry *pClassMethEntry;` |
|  209835 |  633 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  209835 |  634 | `		pName = &pMeth->sFunc.sName;` |
|  209835 |  635 | `		pClassMethEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
|  209835 |  636 | `		if( pClassMethEntry != 0 ){` |
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
|  209821 |  680 | `		rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|  209821 |  681 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  682 | `			goto cleanup;` |
|       - |  683 | `		}` |
|       5 |  684 | `	}` |
|       - |  685 | `	/* Record trait in the class */` |
|   15625 |  686 | `	SySetPut(&pClass->aTrait,(const void *)&pTrait);` |
|    7810 |  687 | `cleanup:` |
|       - |  688 | `	/* Always clear visiting flag, even on error paths */` |
|   15625 |  689 | `	pTrait->iFlags &= ~PH7_CLASS_TRAIT_VISITING;` |
|    7810 |  690 | `	SXUNUSED(pGen);` |
|   15625 |  691 | `	return rc;` |
|    7815 |  692 | `}` |
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
|   27200 |  706 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceInherit(ph7_class *pSub,ph7_class *pBase)` |
|       5 |  707 | `{` |
|       - |  708 | `	ph7_class_method *pMeth;` |
|       - |  709 | `	ph7_class_attr *pAttr;` |
|       - |  710 | `	SyHashEntry *pEntry;` |
|       - |  711 | `	SyString *pName;` |
|       - |  712 | `	sxi32 rc;` |
|       - |  713 | `	/* Install in the derived hashtable */` |
|   27205 |  714 | `	SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|   27205 |  715 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
|       - |  716 | `	/* Copy constants */` |
|   40807 |  717 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
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
|   27205 |  729 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
|       - |  730 | `	/* Copy methods signature */` |
|  106877 |  731 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|       - |  732 | `		/* Make sure the method are not redeclared in the subclass */` |
|   66077 |  733 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   66077 |  734 | `		pName = &pMeth->sFunc.sName;` |
|   66077 |  735 | `		if( SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte) == 0 ){` |
|       - |  736 | `			/* Install the method */` |
|   66077 |  737 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|   66077 |  738 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  739 | `				return rc;` |
|       - |  740 | `			}` |
|   33036 |  741 | `		}` |
|       5 |  742 | `	}` |
|       - |  743 | `	/* Mark as subclass */` |
|   27205 |  744 | `	pSub->pBase = pBase;` |
|       - |  745 | `	/* All done */` |
|   27205 |  746 | `	return SXRET_OK;` |
|   13605 |  747 | `}` |
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
|  396522 |  761 | `PH7_PRIVATE sxi32 PH7_ClassImplement(ph7_class *pMain,ph7_class *pInterface)` |
|       5 |  762 | `{` |
|       - |  763 | `	ph7_class_attr *pAttr;` |
|       - |  764 | `	SyHashEntry *pEntry;` |
|       - |  765 | `	SyString *pName;` |
|       - |  766 | `	sxi32 rc;` |
|       - |  767 | `	/* First off,copy all constants declared inside the interface */` |
|  396527 |  768 | `	SyHashResetLoopCursor(&pInterface->hAttr);` |
|  703554 |  769 | `	while((pEntry = SyHashGetNextEntry(&pInterface->hAttr)) != 0 ){` |
|       - |  770 | `		/* Point to the constant declaration */` |
|  108771 |  771 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  108771 |  772 | `		pName = &pAttr->sName;` |
|       - |  773 | `		/* Make sure the attribute is not redeclared in the main class */` |
|  108771 |  774 | `		if( SyHashGet(&pMain->hAttr,pName->zString,pName->nByte) == 0 ){` |
|       - |  775 | `			/* Install the attribute */` |
|  108767 |  776 | `			rc = SyHashInsert(&pMain->hAttr,pName->zString,pName->nByte,pAttr);` |
|  108767 |  777 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  778 | `				return rc;` |
|       - |  779 | `			}` |
|   54381 |  780 | `		}` |
|       5 |  781 | `	}` |
|       - |  782 | `	/* Install in the interface container */` |
|  396527 |  783 | `	SySetPut(&pMain->aInterface,(const void *)&pInterface);` |
|       - |  784 | `	/* Install interface method stubs into the implementing class.` |
|       - |  785 | `	 * Methods already defined in the class take precedence (they satisfy` |
|       - |  786 | `	 * the interface contract). Stubs retain PH7_CLASS_ATTR_ABSTRACT so` |
|       - |  787 | `	 * the unified check in GenStateCheckAbstractMethods catches missing ones.` |
|       - |  788 | `	 */` |
|       - |  789 | `	{` |
|       - |  790 | `		ph7_class_method *pMeth;` |
|       - |  791 | `		SyHashEntry *pMEntry;` |
|       - |  792 | `		SyString *pMName;` |
|  396527 |  793 | `		SyHashResetLoopCursor(&pInterface->hMethod);` |
| 1376078 |  794 | `		while((pMEntry = SyHashGetNextEntry(&pInterface->hMethod)) != 0 ){` |
|  781295 |  795 | `			pMeth = (ph7_class_method *)pMEntry->pUserData;` |
|  781295 |  796 | `			pMName = &pMeth->sFunc.sName;` |
|  781295 |  797 | `			if( SyHashGet(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte) == 0 ){` |
|      25 |  798 | `				rc = SyHashInsert(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte,pMeth);` |
|      25 |  799 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  800 | `					return rc;` |
|       - |  801 | `				}` |
|      10 |  802 | `			}` |
|       5 |  803 | `		}` |
|       - |  804 | `	}` |
|  396527 |  805 | `	return SXRET_OK;` |
|  198266 |  806 | `}` |
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
|    8298 |  886 | `static ph7_class_instance * NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|       5 |  887 | `{` |
|       - |  888 | `	ph7_class_instance *pThis;` |
|       - |  889 | `	/* Allocate a new instance */` |
|    8303 |  890 | `	pThis = (ph7_class_instance *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_instance));` |
|    8303 |  891 | `	if( pThis == 0 ){` |
|     ! 0 |  892 | `		return 0;` |
|       - |  893 | `	}` |
|       - |  894 | `	/* Zero the structure */` |
|    8303 |  895 | `	SyZero(pThis,sizeof(ph7_class_instance));` |
|       - |  896 | `	/* Initialize fields */` |
|    8303 |  897 | `	pThis->iRef = 1;` |
|    8303 |  898 | `	pThis->pVm = pVm;` |
|    8303 |  899 | `	pThis->pClass = pClass;` |
|       - |  900 | `	/* Assign a fresh monotonic object handle id (clones get their own, like PHP). */` |
|    8303 |  901 | `	pThis->nObjId = pVm->nNextObjId++;` |
|    8303 |  902 | `	SyHashInit(&pThis->hAttr,&pVm->sAllocator,0,0);` |
|    8303 |  903 | `	return pThis;` |
|    4154 |  904 | `}` |
|       - |  905 | `/*` |
|       - |  906 | ` * Wrapper around the NewClassInstance() function defined above.` |
|       - |  907 | ` * See the block comment above for more information.` |
|       - |  908 | ` */` |
|    8052 |  909 | `PH7_PRIVATE ph7_class_instance * PH7_NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|       5 |  910 | `{` |
|       - |  911 | `	ph7_class_instance *pNew;` |
|       - |  912 | `	sxi32 rc;` |
|    8057 |  913 | `	pNew = NewClassInstance(&(*pVm),&(*pClass));` |
|    8057 |  914 | `	if( pNew == 0 ){` |
|     ! 0 |  915 | `		return 0;` |
|       - |  916 | `	}` |
|       - |  917 | `	/* Associate a private VM frame with this class instance */` |
|    8057 |  918 | `	rc = PH7_VmCreateClassInstanceFrame(&(*pVm),pNew);` |
|    8057 |  919 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  920 | `		SyMemBackendPoolFree(&pVm->sAllocator,pNew);` |
|     ! 0 |  921 | `		return 0;` |
|       - |  922 | `	}` |
|       - |  923 | `	/* php stamps a Throwable's file/line at CREATION, not in its constructor, so a` |
|       - |  924 | `	 * subclass that overrides __construct without calling parent::__construct still` |
|       - |  925 | `	 * reports the right site. Every instantiation path lands here. */` |
|    8057 |  926 | `	PH7_VmStampThrowableSite(&(*pVm),pNew);` |
|    8057 |  927 | `	return pNew;` |
|    4031 |  928 | `}` |
|       - |  929 | `/*` |
|       - |  930 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon] attribute.` |
|       - |  931 | ` * This function never fail.` |
|       - |  932 | ` */` |
|   23740 |  933 | `static ph7_value * ExtractClassAttrValue(ph7_vm *pVm,VmClassAttr *pAttr)` |
|       5 |  934 | `{` |
|       - |  935 | `	/* Extract the value */` |
|       - |  936 | `	ph7_value *pValue;` |
|   23745 |  937 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|   23745 |  938 | `	return pValue;` |
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
|     246 | 1024 | `PH7_PRIVATE ph7_class_instance * PH7_CloneClassInstance(ph7_class_instance *pSrc)` |
|       4 | 1025 | `{` |
|       - | 1026 | `	ph7_class_instance *pClone;` |
|       - | 1027 | `	ph7_class_method *pMethod;` |
|       - | 1028 | `	SyHashEntry *pEntry2;` |
|       - | 1029 | `	SyHashEntry *pEntry;` |
|       - | 1030 | `	ph7_vm *pVm;` |
|       - | 1031 | `	sxi32 rc;` |
|       - | 1032 | `	/* Allocate a new instance */` |
|     250 | 1033 | `	pVm = pSrc->pVm;` |
|     250 | 1034 | `	pClone = NewClassInstance(pVm,pSrc->pClass);` |
|     250 | 1035 | `	if( pClone == 0 ){` |
|     ! 0 | 1036 | `		return 0;` |
|       - | 1037 | `	}` |
|       - | 1038 | `	/* Associate a private VM frame with this class instance */` |
|     250 | 1039 | `	rc = PH7_VmCreateClassInstanceFrame(pVm,pClone);` |
|     250 | 1040 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1041 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClone);` |
|     ! 0 | 1042 | `		return 0;` |
|       - | 1043 | `	}` |
|       - | 1044 | `	/* Duplicate object values. Iterate the SOURCE attributes and copy each into` |
|       - | 1045 | `	 * the clone's same-named slot (looked up by name, so order/count differences` |
|       - | 1046 | `	 * from dynamic properties don't matter). A dynamic (runtime-added) property` |
|       - | 1047 | `	 * has no declared counterpart in the clone, so synthesize it first — without` |
|       - | 1048 | `	 * this, a clone of a stdClass would silently lose all its dynamic properties. */` |
|     250 | 1049 | `	SyHashResetLoopCursor(&pSrc->hAttr);` |
|    2642 | 1050 | `	while((pEntry = SyHashGetNextEntry(&pSrc->hAttr)) != 0 ){` |
|    2396 | 1051 | `		VmClassAttr *pSrcAttr = (VmClassAttr *)pEntry->pUserData;` |
|    2396 | 1052 | `		VmClassAttr *pDestAttr = 0;` |
|    2396 | 1053 | `		ph7_value *pvSrc,*pvDest = 0;` |
|       - | 1054 | `		/* Duplicate non-static attribute */` |
|    2396 | 1055 | `		if( pSrcAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|    1695 | 1056 | `			continue;` |
|       - | 1057 | `		}` |
|     702 | 1058 | `		pEntry2 = SyHashGet(&pClone->hAttr,SyStringData(&pSrcAttr->pAttr->sName),SyStringLength(&pSrcAttr->pAttr->sName));` |
|     702 | 1059 | `		if( pEntry2 ){` |
|     688 | 1060 | `			pDestAttr = (VmClassAttr *)pEntry2->pUserData;` |
|     688 | 1061 | `			pvDest = ExtractClassAttrValue(pVm,pDestAttr);` |
|     357 | 1062 | `		}else if( pSrcAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){` |
|       - | 1063 | `			/* Dynamic property: synthesize the matching slot on the clone. */` |
|      22 | 1064 | `			pvDest = PH7_VmCreateDynamicAttr(pVm,pClone,` |
|      14 | 1065 | `				SyStringData(&pSrcAttr->pAttr->sName),SyStringLength(&pSrcAttr->pAttr->sName),&pDestAttr);` |
|       7 | 1066 | `		}` |
|       - | 1067 | `		/* Fetch the source value LAST: PH7_VmCreateDynamicAttr above may have` |
|       - | 1068 | `		 * reserved a slot and reallocated pVm->aMemObj, which would dangle any` |
|       - | 1069 | `		 * ph7_value* obtained before it. pvDest from the synth path already points` |
|       - | 1070 | `		 * into the post-realloc aMemObj; resolve pvSrc now so both are current. */` |
|     702 | 1071 | `		pvSrc = ExtractClassAttrValue(pVm,pSrcAttr);` |
|     702 | 1072 | `		if( (pSrcAttr->iState & VM_CLASS_ATTR_REFBOUND) && pDestAttr ){` |
|       - | 1073 | `			/* php preserves references across clone: the clone shares the SAME slot` |
|       - | 1074 | `			 * as the source property (both alias the referenced variable), rather` |
|       - | 1075 | `			 * than getting an independent value copy. Drop the clone's fresh private` |
|       - | 1076 | `			 * slot and repoint at the (already-pinned) shared slot. The REFBOUND flag` |
|       - | 1077 | `			 * is carried over by the iState copy below, so the clone's release also` |
|       - | 1078 | `			 * leaves the shared slot alone. */` |
|       3 | 1079 | `			if( pDestAttr->nIdx != pSrcAttr->nIdx ){` |
|       3 | 1080 | `				if( pDestAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|     ! 0 | 1081 | `					SyHashDeleteEntry(&pVm->hTypedSlot,(const void *)&pDestAttr->nIdx,sizeof(sxu32),0);` |
|     ! 0 | 1082 | `				}` |
|       3 | 1083 | `				PH7_VmUnsetMemObj(pVm,pDestAttr->nIdx,TRUE);` |
|       3 | 1084 | `				pDestAttr->nIdx = pSrcAttr->nIdx;` |
|       2 | 1085 | `			}` |
|     701 | 1086 | `		}else if( pvSrc && pvDest ){` |
|     700 | 1087 | `			PH7_MemObjStore(pvSrc,pvDest);` |
|     348 | 1088 | `		}` |
|       - | 1089 | `		/* Carry over the per-instance state so the clone matches the source:` |
|       - | 1090 | `		 * VM_CLASS_ATTR_UNINIT marks a typed property as not-yet-initialized` |
|       - | 1091 | `		 * and doubles as the readonly write-once latch — without this a clone` |
|       - | 1092 | `		 * would reset to uninitialized (losing the value's readiness) and a` |
|       - | 1093 | `		 * readonly property would become writable again. */` |
|     702 | 1094 | `		if( pDestAttr ){` |
|     702 | 1095 | `			pDestAttr->iState = pSrcAttr->iState;` |
|     349 | 1096 | `		}` |
|       4 | 1097 | `	}` |
|       - | 1098 | `	/* A declared property unset() on the source is absent from the clone too (PHP). But the clone` |
|       - | 1099 | `	 * frame above materialized ALL declared attrs (with their defaults), so drop any clone attr whose` |
|       - | 1100 | `	 * name is not present on the source. Collect first, then delete — removing an entry mid-walk would` |
|       - | 1101 | `	 * free the node the SyHash loop cursor points at. */` |
|       - | 1102 | `	{` |
|       - | 1103 | `		SySet sDrop;` |
|     250 | 1104 | `		SySetInit(&sDrop,&pVm->sAllocator,sizeof(VmClassAttr *));` |
|     250 | 1105 | `		SyHashResetLoopCursor(&pClone->hAttr);` |
|    2644 | 1106 | `		while((pEntry = SyHashGetNextEntry(&pClone->hAttr)) != 0 ){` |
|    2398 | 1107 | `			VmClassAttr *pCloneAttr = (VmClassAttr *)pEntry->pUserData;` |
|    2398 | 1108 | `			if( pCloneAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|    1695 | 1109 | `				continue;` |
|       - | 1110 | `			}` |
|    1050 | 1111 | `			if( SyHashGet(&pSrc->hAttr,SyStringData(&pCloneAttr->pAttr->sName),` |
|    1054 | 1112 | `					SyStringLength(&pCloneAttr->pAttr->sName)) == 0 ){` |
|       3 | 1113 | `				SySetPut(&sDrop,(const void *)&pCloneAttr);` |
|       1 | 1114 | `			}` |
|       4 | 1115 | `		}` |
|     250 | 1116 | `		if( SySetUsed(&sDrop) > 0 ){` |
|       3 | 1117 | `			VmClassAttr **apDrop = (VmClassAttr **)SySetBasePtr(&sDrop);` |
|       - | 1118 | `			sxu32 i;` |
|       5 | 1119 | `			for( i = 0 ; i < SySetUsed(&sDrop) ; ++i ){` |
|       3 | 1120 | `				VmClassAttr *pVmAttr = apDrop[i];` |
|       4 | 1121 | `				SyHashDeleteEntry(&pClone->hAttr,SyStringData(&pVmAttr->pAttr->sName),` |
|       2 | 1122 | `					SyStringLength(&pVmAttr->pAttr->sName),0);` |
|       3 | 1123 | `				PH7_VmReleaseInstanceAttr(pVm,pVmAttr);` |
|       2 | 1124 | `			}` |
|       1 | 1125 | `		}` |
|     250 | 1126 | `		SySetRelease(&sDrop);` |
|       - | 1127 | `	}` |
|       - | 1128 | `	/* call the __clone method on the cloned object if available */` |
|     250 | 1129 | `	pMethod = PH7_ClassExtractMethod(pClone->pClass,"__clone",sizeof("__clone")-1);` |
|     250 | 1130 | `	if( pMethod ){` |
|      60 | 1131 | `		if( pMethod->iCloneDepth < 16 ){` |
|      58 | 1132 | `			pMethod->iCloneDepth++;` |
|       - | 1133 | `			/* PHP 8.3: __clone() may re-initialize the clone's readonly` |
|       - | 1134 | `			 * properties. Flag the instance so the readonly store guard allows` |
|       - | 1135 | `			 * it for the duration of the call. */` |
|      58 | 1136 | `			pClone->iFlags \|= VM_INSTANCE_CLONING;` |
|      58 | 1137 | `			PH7_VmCallClassMethod(pVm,pClone,pMethod,0,0,0);` |
|      58 | 1138 | `			pClone->iFlags &= ~VM_INSTANCE_CLONING;` |
|      30 | 1139 | `		}else{` |
|       - | 1140 | `			/* Nesting limit reached */` |
|       3 | 1141 | `			PH7_VmThrowError(pVm,0,PH7_CTX_ERR,"Object clone limit reached,no more call to __clone()");` |
|       - | 1142 | `		}` |
|       - | 1143 | `		/* Reset the cursor */` |
|      60 | 1144 | `		pMethod->iCloneDepth = 0;` |
|      29 | 1145 | `	}` |
|       - | 1146 | `	/* Return the cloned object */` |
|     250 | 1147 | `	return pClone;` |
|     127 | 1148 | `}` |
|       - | 1149 | `#define CLASS_INSTANCE_DESTROYED 0x001 /* Instance is released */` |
|       - | 1150 | `/*` |
|       - | 1151 | ` * Free the per-instance allocations owned by ONE object attribute: its value slot (+ the typed-slot` |
|       - | 1152 | ` * enforcement entry), the synthesized ph7_class_attr for a dynamic (runtime-added) property, and the` |
|       - | 1153 | ` * VmClassAttr wrapper itself. Does NOT touch the hAttr entry node — the caller removes it` |
|       - | 1154 | `` * (`unset($o->p)` via SyHashDeleteEntry2; instance teardown via the wholesale SyHashRelease, so it must`` |
|       - | 1155 | ` * not delete entries mid-walk). Shared by PH7_ClassInstanceRelease and the OP_MEMBER unset path.` |
|       - | 1156 | ` */` |
|   27828 | 1157 | `PH7_PRIVATE void PH7_VmReleaseInstanceAttr(ph7_vm *pVm, VmClassAttr *pVmAttr)` |
|       5 | 1158 | `{` |
|   27833 | 1159 | `	if( pVmAttr->iState & VM_CLASS_ATTR_REFBOUND ){` |
|       - | 1160 | ``		/* Reference-bound property (`$o->p =& $x`): its value slot is SHARED with`` |
|       - | 1161 | `		 * (and pinned by) the source variable — releasing/recycling it here would` |
|       - | 1162 | `		 * dangle the surviving alias. Leave the slot alone (script-lifetime pin,` |
|       - | 1163 | `		 * matching the use(&$x) capture tradeoff); just free the bookkeeping below. */` |
|   27828 | 1164 | `	}else if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|       - | 1165 | `		/* Drop any typed-property enforcement slot registered for this memobj, before the memobj` |
|       - | 1166 | `		 * is returned to the free list, so a future recycled slot does not inherit the stale entry. */` |
|   20527 | 1167 | `		if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|     309 | 1168 | `			SyHashDeleteEntry(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),0);` |
|     152 | 1169 | `		}` |
|   20527 | 1170 | `		PH7_VmUnsetMemObj(pVm,pVmAttr->nIdx,TRUE);` |
|   10261 | 1171 | `	}` |
|       - | 1172 | `	/* A dynamic property owns its synthesized ph7_class_attr (struct + inline name in one block) —` |
|       - | 1173 | `	 * free it here (the only place a per-instance pAttr is freed; declared attrs are class-owned). */` |
|   27833 | 1174 | `	if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){` |
|     120 | 1175 | `		SyMemBackendFree(&pVm->sAllocator,pVmAttr->pAttr);` |
|      59 | 1176 | `	}` |
|   27833 | 1177 | `	SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|   27833 | 1178 | `}` |
|       - | 1179 | `/*` |
|       - | 1180 | ` * Release a class instance [i.e: Object in the PHP jargon] and invoke any defined destructor.` |
|       - | 1181 | ` * This routine is invoked as soon as there are no other references to a particular` |
|       - | 1182 | ` * class instance.` |
|       - | 1183 | ` */` |
|    5584 | 1184 | `static void PH7_ClassInstanceRelease(ph7_class_instance *pThis)` |
|       5 | 1185 | `{` |
|       - | 1186 | `	ph7_class_method *pDestr;` |
|       - | 1187 | `	SyHashEntry *pEntry;` |
|       - | 1188 | `	ph7_class *pClass;` |
|       - | 1189 | `	ph7_vm *pVm;` |
|    5589 | 1190 | `	if( pThis->iFlags & CLASS_INSTANCE_DESTROYED ){` |
|       - | 1191 | `		/*` |
|       - | 1192 | `		 * Already destroyed,return immediately.` |
|       - | 1193 | `		 * This could happend if someone perform unset($this) in the destructor body.` |
|       - | 1194 | `		 */` |
|     ! 0 | 1195 | `		return;` |
|       - | 1196 | `	}` |
|       - | 1197 | `	/* Mark as destroyed */` |
|    5589 | 1198 | `	pThis->iFlags \|= CLASS_INSTANCE_DESTROYED;` |
|       - | 1199 | `	/* Invoke any defined destructor if available */` |
|    5589 | 1200 | `	pVm = pThis->pVm;` |
|    5589 | 1201 | `	pClass = pThis->pClass;` |
|    5589 | 1202 | `	pDestr = PH7_ClassExtractMethod(pClass,"__destruct",sizeof("__destruct")-1);` |
|    5589 | 1203 | `	if( pDestr && !pVm->bInReset ){` |
|       - | 1204 | `		/* Invoke the destructor. Skipped during ph7_vm_reset() bulk teardown:` |
|       - | 1205 | `		 * running user PHP against a half-reset VM is unsafe (see bInReset). */` |
|     471 | 1206 | `		pThis->iRef = 2; /* Prevent garbage collection */` |
|     471 | 1207 | `		PH7_VmCallClassMethod(pVm,pThis,pDestr,0,0,0);` |
|     233 | 1208 | `	}` |
|       - | 1209 | `	/* Weak-reference registry: kill the cell for this instance so every` |
|       - | 1210 | `	 * WeakReference/WeakMap handle observes the death (the cell outlives the` |
|       - | 1211 | `	 * instance until its own handles drop; removing the hash entry here keeps` |
|       - | 1212 | `	 * a pool-reused address from resurrecting a dead cell). */` |
|    5589 | 1213 | `	if( SyHashTotalEntry(&pVm->hWeakCell) > 0 ){` |
|      27 | 1214 | `		void *pCellData = 0;` |
|      26 | 1215 | `		if( SyHashDeleteEntry(&pVm->hWeakCell,(const void *)&pThis,sizeof(void *),&pCellData) == SXRET_OK` |
|      18 | 1216 | `		 && pCellData ){` |
|       9 | 1217 | `			((VmWeakCell *)pCellData)->pObj = 0;` |
|       4 | 1218 | `		}` |
|      13 | 1219 | `	}` |
|       - | 1220 | `	/* Release non-static attributes (the wholesale SyHashRelease below frees the entry nodes,` |
|       - | 1221 | `	 * so the helper must not delete them mid-walk). */` |
|    5589 | 1222 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|   33389 | 1223 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|   27805 | 1224 | `		PH7_VmReleaseInstanceAttr(pVm,(VmClassAttr *)pEntry->pUserData);` |
|       5 | 1225 | `	}` |
|       - | 1226 | `	/* Release the whole structure */` |
|    5589 | 1227 | `	SyHashRelease(&pThis->hAttr);` |
|    5589 | 1228 | `	SyMemBackendPoolFree(&pVm->sAllocator,pThis);` |
|    2797 | 1229 | `}` |
|       - | 1230 | `/*` |
|       - | 1231 | ` * Decrement the reference count of a class instance [i.e Object in the PHP jargon].` |
|       - | 1232 | ` * If the reference count reaches zero,release the whole instance.` |
|       - | 1233 | ` */` |
|  130816 | 1234 | `PH7_PRIVATE void PH7_ClassInstanceUnref(ph7_class_instance *pThis)` |
|       5 | 1235 | `{` |
|  130821 | 1236 | `	pThis->iRef--;` |
|  130821 | 1237 | `	if( pThis->iRef < 1 ){` |
|       - | 1238 | `		/* No more reference to this instance */` |
|    5589 | 1239 | `		PH7_ClassInstanceRelease(&(*pThis));` |
|    2792 | 1240 | `	}` |
|  130821 | 1241 | `}` |
|       - | 1242 | `/*` |
|       - | 1243 | ` * Compare two class instances [i.e: Objects in the PHP jargon]` |
|       - | 1244 | ` * Note on objects comparison:` |
|       - | 1245 | ` *  According to the PHP langauge reference manual` |
|       - | 1246 | ` *  When using the comparison operator (==), object variables are compared in a simple manner` |
|       - | 1247 | ` *  namely: Two object instances are equal if they have the same attributes and values, and are` |
|       - | 1248 | ` *  instances of the same class.` |
|       - | 1249 | ` *  On the other hand, when using the identity operator (===), object variables are identical` |
|       - | 1250 | ` *  if and only if they refer to the same instance of the same class.` |
|       - | 1251 | ` *  An example will clarify these rules.` |
|       - | 1252 | ` *  Example #1 Example of object comparison` |
|       - | 1253 | ` *  <?php` |
|       - | 1254 | ` *    function bool2str($bool)` |
|       - | 1255 | ` * {` |
|       - | 1256 | ` *   if ($bool === false) {` |
|       - | 1257 | ` *       return 'FALSE';` |
|       - | 1258 | ` *   } else {` |
|       - | 1259 | ` *       return 'TRUE';` |
|       - | 1260 | ` *   }` |
|       - | 1261 | ` * }` |
|       - | 1262 | ` * function compareObjects(&$o1, &$o2)` |
|       - | 1263 | ` * {` |
|       - | 1264 | ` *   echo 'o1 == o2 : ' . bool2str($o1 == $o2) . "\n";` |
|       - | 1265 | ` *   echo 'o1 != o2 : ' . bool2str($o1 != $o2) . "\n";` |
|       - | 1266 | ` *   echo 'o1 === o2 : ' . bool2str($o1 === $o2) . "\n";` |
|       - | 1267 | ` *   echo 'o1 !== o2 : ' . bool2str($o1 !== $o2) . "\n";` |
|       - | 1268 | ` * }` |
|       - | 1269 | ` * class Flag` |
|       - | 1270 | ` * {` |
|       - | 1271 | ` *   public $flag;` |
|       - | 1272 | ` *` |
|       - | 1273 | ` *   function Flag($flag = true) {` |
|       - | 1274 | ` *       $this->flag = $flag;` |
|       - | 1275 | ` *   }` |
|       - | 1276 | ` * }` |
|       - | 1277 | ` *` |
|       - | 1278 | ` * class OtherFlag` |
|       - | 1279 | ` * {` |
|       - | 1280 | ` *   public $flag;` |
|       - | 1281 | ` *` |
|       - | 1282 | ` *   function OtherFlag($flag = true) {` |
|       - | 1283 | ` *       $this->flag = $flag;` |
|       - | 1284 | ` *   }` |
|       - | 1285 | ` * }` |
|       - | 1286 | ` *` |
|       - | 1287 | ` * $o = new Flag();` |
|       - | 1288 | ` * $p = new Flag();` |
|       - | 1289 | ` * $q = $o;` |
|       - | 1290 | ` * $r = new OtherFlag();` |
|       - | 1291 | ` *` |
|       - | 1292 | ` * echo "Two instances of the same class\n";` |
|       - | 1293 | ` * compareObjects($o, $p);` |
|       - | 1294 | ` * echo "\nTwo references to the same instance\n";` |
|       - | 1295 | ` * compareObjects($o, $q);` |
|       - | 1296 | ` * echo "\nInstances of two different classes\n";` |
|       - | 1297 | ` * compareObjects($o, $r);` |
|       - | 1298 | ` * ?>` |
|       - | 1299 | ` * The above example will output:` |
|       - | 1300 | ` * Two instances of the same class` |
|       - | 1301 | ` * o1 == o2 : TRUE` |
|       - | 1302 | ` * o1 != o2 : FALSE` |
|       - | 1303 | ` * o1 === o2 : FALSE` |
|       - | 1304 | ` * o1 !== o2 : TRUE` |
|       - | 1305 | ` * Two references to the same instance` |
|       - | 1306 | ` * o1 == o2 : TRUE` |
|       - | 1307 | ` * o1 != o2 : FALSE` |
|       - | 1308 | ` * o1 === o2 : TRUE` |
|       - | 1309 | ` * o1 !== o2 : FALSE` |
|       - | 1310 | ` * Instances of two different classes` |
|       - | 1311 | ` * o1 == o2 : FALSE` |
|       - | 1312 | ` * o1 != o2 : TRUE` |
|       - | 1313 | ` * o1 === o2 : FALSE` |
|       - | 1314 | ` * o1 !== o2 : TRUE` |
|       - | 1315 | ` *` |
|       - | 1316 | ` * This function return 0 if the objects are equals according to the comprison rules defined above.` |
|       - | 1317 | ` * Any other return values indicates difference.` |
|       - | 1318 | ` */` |
|     272 | 1319 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest)` |
|       5 | 1320 | `{` |
|       - | 1321 | `	SyHashEntry *pEntry,*pEntry2;` |
|       - | 1322 | `	ph7_value sV1,sV2;` |
|       - | 1323 | `	sxi32 rc;` |
|     277 | 1324 | `	if( iNest > 31 ){` |
|       - | 1325 | `		/* Nesting limit reached */` |
|       6 | 1326 | `		PH7_VmThrowError(pLeft->pVm,0,PH7_CTX_ERR,"Nesting limit reached: Infinite recursion?");` |
|       6 | 1327 | `		return 1;` |
|       - | 1328 | `	}` |
|       - | 1329 | `	/* Comparison is performed only if the objects are instance of the same class */` |
|     273 | 1330 | `	if( pLeft->pClass != pRight->pClass ){` |
|       7 | 1331 | `		return 1;` |
|       - | 1332 | `	}` |
|     267 | 1333 | `	if( bStrict ){` |
|       - | 1334 | `		/*` |
|       - | 1335 | `		 * According to the PHP language reference manual:` |
|       - | 1336 | `		 *  when using the identity operator (===), object variables` |
|       - | 1337 | `		 *  are identical if and only if they refer to the same instance` |
|       - | 1338 | `		 *  of the same class.` |
|       - | 1339 | `		 */` |
|     103 | 1340 | `		return !(pLeft == pRight);` |
|       - | 1341 | `	}` |
|       - | 1342 | `	/*` |
|       - | 1343 | `	 * Attribute comparison.` |
|       - | 1344 | `	 * According to the PHP reference manual:` |
|       - | 1345 | `	 *  When using the comparison operator (==), object variables are compared` |
|       - | 1346 | `	 *  in a simple manner, namely: Two object instances are equal if they have` |
|       - | 1347 | `	 *  the same attributes and values, and are instances of the same class.` |
|       - | 1348 | `	 */` |
|     168 | 1349 | `	if( pLeft == pRight ){` |
|       - | 1350 | `		/* Same instance,don't bother processing,object are equals */` |
|       5 | 1351 | `		return 0;` |
|       - | 1352 | `	}` |
|       - | 1353 | `	/* Closures compare by IDENTITY under == as well (not by attributes): two distinct` |
|       - | 1354 | `	 * Closure instances are never equal, even when they wrap the same underlying function` |
|       - | 1355 | `	 * (PHP semantics). pLeft != pRight here, so a Closure pair is unequal. Without this,` |
|       - | 1356 | `` 	 * two capture-less lambdas of the same `function(){}` share the template's `$__fn` `` |
|       - | 1357 | `	 * name and would compare equal. */` |
|     164 | 1358 | `	if( pLeft->pVm->pClosureClass && pLeft->pClass == pLeft->pVm->pClosureClass ){` |
|       5 | 1359 | `		return 1;` |
|       - | 1360 | `	}` |
|       - | 1361 | `	/* Same class but a different number of attributes ⇒ different property sets` |
|       - | 1362 | `	 * (dynamic properties can give two same-class instances different counts). */` |
|     160 | 1363 | `	if( pLeft->hAttr.nEntry != pRight->hAttr.nEntry ){` |
|       3 | 1364 | `		return 1;` |
|       - | 1365 | `	}` |
|     158 | 1366 | `	PH7_MemObjInit(pLeft->pVm,&sV1);` |
|     158 | 1367 | `	PH7_MemObjInit(pLeft->pVm,&sV2);` |
|     158 | 1368 | `	sV1.nIdx = sV2.nIdx = SXU32_HIGH;` |
|       - | 1369 | `	/* Compare each left attribute against the RIGHT attribute of the SAME NAME` |
|       - | 1370 | `	 * (not in lockstep): dynamic properties may be stored in a different order` |
|       - | 1371 | `	 * on the two instances. Counts already match, so if every left attribute has` |
|       - | 1372 | `	 * an equal-valued same-named right attribute the property sets are equal. */` |
|     158 | 1373 | `	SyHashResetLoopCursor(&pLeft->hAttr);` |
|     194 | 1374 | `	while((pEntry = SyHashGetNextEntry(&pLeft->hAttr)) != 0 ){` |
|     170 | 1375 | `		VmClassAttr *p1 = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1376 | `		VmClassAttr *p2;` |
|       - | 1377 | `		ph7_value *pL,*pR;` |
|       - | 1378 | `		/* Compare only non-static attribute */` |
|     170 | 1379 | `		if( p1->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|     ! 0 | 1380 | `			continue;` |
|       - | 1381 | `		}` |
|     170 | 1382 | `		pEntry2 = SyHashGet(&pRight->hAttr,SyStringData(&p1->pAttr->sName),SyStringLength(&p1->pAttr->sName));` |
|     170 | 1383 | `		if( pEntry2 == 0 ){` |
|       - | 1384 | `			/* Left has a property the right lacks ⇒ not equal. */` |
|     ! 0 | 1385 | `			return 1;` |
|       - | 1386 | `		}` |
|     170 | 1387 | `		p2 = (VmClassAttr *)pEntry2->pUserData;` |
|     170 | 1388 | `		pL = ExtractClassAttrValue(pLeft->pVm,p1);` |
|     170 | 1389 | `		pR = ExtractClassAttrValue(pRight->pVm,p2);` |
|     170 | 1390 | `		if( pL && pR ){` |
|     170 | 1391 | `			PH7_MemObjLoad(pL,&sV1);` |
|     170 | 1392 | `			PH7_MemObjLoad(pR,&sV2);` |
|       - | 1393 | `			/* Compare the two values now */` |
|     170 | 1394 | `			rc = PH7_MemObjCmp(&sV1,&sV2,bStrict,iNest+1);` |
|     170 | 1395 | `			PH7_MemObjRelease(&sV1);` |
|     170 | 1396 | `			PH7_MemObjRelease(&sV2);` |
|     170 | 1397 | `			if( rc != 0 ){` |
|       - | 1398 | `				/* Not equals */` |
|     133 | 1399 | `				return rc;` |
|       - | 1400 | `			}` |
|      18 | 1401 | `		}` |
|       2 | 1402 | `	}` |
|       - | 1403 | `	/* Object are equals */` |
|      26 | 1404 | `	return 0;` |
|     141 | 1405 | `}` |
|       - | 1406 | `/*` |
|       - | 1407 | ` * Dump a class instance and the store the dump in the BLOB given` |
|       - | 1408 | ` * as the first argument.` |
|       - | 1409 | ` * Note that only non-static/non-constants attribute are dumped.` |
|       - | 1410 | ` * This function is typically invoked when the user issue a call` |
|       - | 1411 | ` * to [var_dump(),var_export(),print_r(),...].` |
|       - | 1412 | ` * This function SXRET_OK on success. Any other return value including` |
|       - | 1413 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|       - | 1414 | ` */` |
|       - | 1415 | `/*` |
|       - | 1416 | `` * Return the `name` property value of an enum case instance (the case name),`` |
|       - | 1417 | ` * or 0 when unavailable. Shared by the var_dump/var_export/json/serialize` |
|       - | 1418 | ` * renderers, which all print enum cases as Class::CaseName forms.` |
|       - | 1419 | ` */` |
|       6 | 1420 | `PH7_PRIVATE ph7_value * PH7_EnumCaseNameValue(ph7_class_instance *pThis)` |
|       1 | 1421 | `{` |
|       - | 1422 | `	SyHashEntry *pEntry;` |
|       7 | 1423 | `	if( (pThis->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|     ! 0 | 1424 | `		return 0;` |
|       - | 1425 | `	}` |
|       7 | 1426 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)"name",sizeof("name")-1);` |
|       7 | 1427 | `	if( pEntry == 0 ){` |
|     ! 0 | 1428 | `		return 0;` |
|       - | 1429 | `	}` |
|       7 | 1430 | `	return PH7_ClassInstanceExtractAttrValue(pThis,(VmClassAttr *)pEntry->pUserData);` |
|       4 | 1431 | `}` |
|       - | 1432 | `/*` |
|       - | 1433 | `` * Return the `value` property value (the backing value) of an enum case`` |
|       - | 1434 | ` * instance, or 0 when unavailable (pure enums have none).` |
|       - | 1435 | ` */` |
|       8 | 1436 | `PH7_PRIVATE ph7_value * PH7_EnumCaseBackingValueOf(ph7_class_instance *pThis)` |
|       1 | 1437 | `{` |
|       - | 1438 | `	SyHashEntry *pEntry;` |
|       9 | 1439 | `	if( (pThis->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|     ! 0 | 1440 | `		return 0;` |
|       - | 1441 | `	}` |
|       9 | 1442 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)"value",sizeof("value")-1);` |
|       9 | 1443 | `	if( pEntry == 0 ){` |
|       3 | 1444 | `		return 0;` |
|       - | 1445 | `	}` |
|       7 | 1446 | `	return PH7_ClassInstanceExtractAttrValue(pThis,(VmClassAttr *)pEntry->pUserData);` |
|       5 | 1447 | `}` |
|       - | 1448 | `/*` |
|       - | 1449 | ` * Emit a class-instance dump header plus its trailing newline. For var_dump` |
|       - | 1450 | ` * (ShowType) it completes the "object(" prefix the caller already emitted as` |
|       - | 1451 | ` *   ClassName)#<id> (<count>) {` |
|       - | 1452 | ` * for print_r it emits the legacy PHL  Object(ClassName) {  (count/id unused).` |
|       - | 1453 | `` * Enum cases print php's `ClassName Enum {` print_r header (var_dump never`` |
|       - | 1454 | `` * reaches here for enums — PH7_MemObjDump prints `enum(S::A)` directly).`` |
|       - | 1455 | ` */` |
|     136 | 1456 | `static void DumpClassInstanceHeader(SyBlob *pOut,ph7_class *pClass,sxu32 nObjId,int ShowType,sxu32 nCount)` |
|       3 | 1457 | `{` |
|     139 | 1458 | `	if( ShowType ){` |
|       - | 1459 | ``		/* var_dump: `object(C)#id (n) {` */`` |
|     135 | 1460 | `		SyBlobFormat(&(*pOut),"object(%z)#%u (%u) {",&pClass->sName,nObjId,nCount);` |
|     135 | 1461 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|     135 | 1462 | `		return;` |
|       - | 1463 | `	}` |
|       - | 1464 | ``	/* print_r: `C Object` / `E Enum[:backing]` — the '(' line is emitted by`` |
|       - | 1465 | `	 * the body renderer at the container indent. */` |
|       6 | 1466 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|     ! 0 | 1467 | `		SyBlobFormat(&(*pOut),"%z Enum",&pClass->sName);` |
|     ! 0 | 1468 | `		if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|     ! 0 | 1469 | `			SyBlobAppend(&(*pOut),":int",sizeof(":int")-1);` |
|     ! 0 | 1470 | `		}else if( pClass->nEnumBacking == MEMOBJ_STRING ){` |
|     ! 0 | 1471 | `			SyBlobAppend(&(*pOut),":string",sizeof(":string")-1);` |
|     ! 0 | 1472 | `		}` |
|     ! 0 | 1473 | `	}else{` |
|       6 | 1474 | `		SyBlobFormat(&(*pOut),"%z Object",&pClass->sName);` |
|       - | 1475 | `	}` |
|       6 | 1476 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      71 | 1477 | `}` |
|       - | 1478 | `/*` |
|       - | 1479 | ` * The class that DECLARED pAttr: inheritance shares attr pointers down the` |
|       - | 1480 | ` * chain, so the declaring class is the most ANCESTRAL class whose hAttr still` |
|       - | 1481 | ` * maps the name to this exact pointer. php's var_dump/print_r use it for the` |
|       - | 1482 | `` * `["p":"Decl":private]` annotation.`` |
|       - | 1483 | ` */` |
|       6 | 1484 | `static ph7_class * OoAttrDeclaringClass(ph7_class *pClass,ph7_class_attr *pAttr)` |
|       1 | 1485 | `{` |
|       - | 1486 | `	/* Attrs record their declaring class at install time (inheritance/trait` |
|       - | 1487 | `	 * copies share the pointer, so the field survives the chain). */` |
|       7 | 1488 | `	return pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|       1 | 1489 | `}` |
|       - | 1490 | `/*` |
|       - | 1491 | `` * Emit a property's dump key: var_dump `["x"]=>` / `["p":"C":private]=>` /`` |
|       - | 1492 | `` * `["q":protected]=>`; print_r `[x] => ` / `[p:C:private] => ` /`` |
|       - | 1493 | `` * `[q:protected] => ` (php's exact annotations).`` |
|       - | 1494 | ` */` |
|     138 | 1495 | `static void OoDumpPropKey(SyBlob *pOut,ph7_class_instance *pThis,ph7_class_attr *pAttr,int ShowType)` |
|       2 | 1496 | `{` |
|     140 | 1497 | `	const char *zQ = ShowType ? "\"" : "";` |
|     140 | 1498 | `	SyBlobFormat(&(*pOut),"[%s%z%s",zQ,&pAttr->sName,zQ);` |
|     140 | 1499 | `	if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       7 | 1500 | `		ph7_class *pDecl = OoAttrDeclaringClass(pThis->pClass,pAttr);` |
|       7 | 1501 | `		SyBlobFormat(&(*pOut),":%s%z%s:private",zQ,&pDecl->sName,zQ);` |
|     137 | 1502 | `	}else if( pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
|     ! 0 | 1503 | `		SyBlobAppend(&(*pOut),":protected",sizeof(":protected")-1);` |
|     ! 0 | 1504 | `	}` |
|     140 | 1505 | `	SyBlobAppend(&(*pOut),ShowType ? "]=>" : "] => ",ShowType ? 3 : 5);` |
|     140 | 1506 | `}` |
|     140 | 1507 | `PH7_PRIVATE sxi32 PH7_ClassInstanceDump(SyBlob *pOut,ph7_class_instance *pThis,int ShowType,int nTab,int nDepth)` |
|       3 | 1508 | `{` |
|       - | 1509 | `	SyHashEntry *pEntry;` |
|       - | 1510 | `	ph7_value *pValue;` |
|       - | 1511 | `	sxi32 rc;` |
|       - | 1512 | `	int i;` |
|     143 | 1513 | `	if( nDepth > 31 ){` |
|       - | 1514 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|       - | 1515 | `		/* Nesting limit reached..halt immediately*/` |
|       5 | 1516 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|       5 | 1517 | `		return SXERR_LIMIT;` |
|       - | 1518 | `	}` |
|     139 | 1519 | `	rc = SXRET_OK;` |
|       - | 1520 | `	{` |
|       - | 1521 | `		/* Both var_dump and print_r consult __debugInfo() (PHP behavior);` |
|       - | 1522 | `		 * var_export uses a separate renderer and never reaches here. When the` |
|       - | 1523 | `		 * method is present and returns an array, render that array's entries as` |
|       - | 1524 | `		 * the object body, with the header showing the debug array's count. The` |
|       - | 1525 | `		 * nDepth guard above protects against a __debugInfo returning the object` |
|       - | 1526 | `		 * itself. */` |
|     139 | 1527 | `		ph7_class_method *pDbg = PH7_ClassExtractMethod(pThis->pClass,"__debugInfo",sizeof("__debugInfo")-1);` |
|     139 | 1528 | `		if( pDbg ){` |
|       - | 1529 | `			ph7_value sResult;` |
|       5 | 1530 | `			PH7_MemObjInit(pThis->pVm,&sResult);` |
|       5 | 1531 | `			PH7_VmCallClassMethod(pThis->pVm,pThis,pDbg,&sResult,0,0);` |
|       5 | 1532 | `			if( sResult.iFlags & MEMOBJ_HASHMAP ){` |
|       5 | 1533 | `				ph7_hashmap *pMap = (ph7_hashmap *)sResult.x.pOther;` |
|       - | 1534 | `				/* Header count is the debug array's entry count. */` |
|       5 | 1535 | `				DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,pMap->nEntry);` |
|       5 | 1536 | `				if( !ShowType ){` |
|       3 | 1537 | `					for( i = 0 ; i < nTab ; i++ ){` |
|     ! 0 | 1538 | `						SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     ! 0 | 1539 | `					}` |
|       3 | 1540 | `					SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       1 | 1541 | `				}` |
|       5 | 1542 | `				rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth);` |
|       5 | 1543 | `				for( i = 0 ; i < nTab ; i++ ){` |
|     ! 0 | 1544 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     ! 0 | 1545 | `				}` |
|       5 | 1546 | `				if( ShowType ){` |
|       3 | 1547 | `					SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|       2 | 1548 | `				}else{` |
|       3 | 1549 | `					SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       - | 1550 | `				}` |
|       5 | 1551 | `				PH7_MemObjRelease(&sResult);` |
|       5 | 1552 | `				return rc;` |
|       - | 1553 | `			}` |
|       - | 1554 | `			/* Non-array return: behave as if __debugInfo were absent. */` |
|     ! 0 | 1555 | `			PH7_MemObjRelease(&sResult);` |
|     ! 0 | 1556 | `		}` |
|       - | 1557 | `	}` |
|       - | 1558 | `	{` |
|       - | 1559 | `		/* var_dump's header needs the property count up front, so pre-count the` |
|       - | 1560 | `		 * non-static/non-constant attributes (matching the dump loop below). */` |
|     134 | 1561 | `		sxu32 nProp = 0;` |
|     134 | 1562 | `		if( ShowType ){` |
|     132 | 1563 | `			SyHashResetLoopCursor(&pThis->hAttr);` |
|     270 | 1564 | `			while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|     140 | 1565 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     140 | 1566 | `				if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_HOOK_VIRTUAL)) == 0 ){` |
|     136 | 1567 | `					nProp++;` |
|      67 | 1568 | `				}` |
|       2 | 1569 | `			}` |
|      65 | 1570 | `		}` |
|     134 | 1571 | `		DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,nProp);` |
|       - | 1572 | `	}` |
|     134 | 1573 | `	if( !ShowType ){` |
|       - | 1574 | `		/* print_r body opener: '(' at the container indent */` |
|       3 | 1575 | `		for( i = 0 ; i < nTab ; i++ ){` |
|     ! 0 | 1576 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     ! 0 | 1577 | `		}` |
|       3 | 1578 | `		SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       1 | 1579 | `	}` |
|       - | 1580 | `	/* Dump object attributes (php 8.4: VIRTUAL hooked properties have no` |
|       - | 1581 | `	 * backing store — excluded from var_dump/print_r) */` |
|     134 | 1582 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|     218 | 1583 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|     144 | 1584 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     144 | 1585 | `		if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_HOOK_VIRTUAL)) == 0 ){` |
|       - | 1586 | `			/* Dump non-static/constant attribute only */` |
|     140 | 1587 | `			pValue = ExtractClassAttrValue(pThis->pVm,pVmAttr);` |
|     140 | 1588 | `			if( pValue == 0 ){` |
|     ! 0 | 1589 | `				continue;` |
|       - | 1590 | `			}` |
|     140 | 1591 | `			if( ShowType ){` |
|       - | 1592 | ``				/* var_dump prop: `["x"(:…)]=>` at nTab+2, the value on the next`` |
|       - | 1593 | `				 * line at the same indent (php). */` |
|    4124 | 1594 | `				for( i = 0 ; i < nTab + 2 ; i++ ){` |
|    3990 | 1595 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|    1996 | 1596 | `				}` |
|     136 | 1597 | `				OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,TRUE);` |
|     136 | 1598 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|     136 | 1599 | `				rc = PH7_MemObjDump(&(*pOut),pValue,TRUE,nTab+2,nDepth,0);` |
|     136 | 1600 | `				if( rc == SXERR_LIMIT ){` |
|     125 | 1601 | `					break;` |
|       - | 1602 | `				}` |
|       7 | 1603 | `			}else{` |
|       - | 1604 | ``				/* print_r prop: `[x(:…)] => value` at nTab+4; container values`` |
|       - | 1605 | `				 * render their block at nTab+8 followed by php's blank line. */` |
|      21 | 1606 | `				for( i = 0 ; i < nTab + 4 ; i++ ){` |
|      17 | 1607 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       9 | 1608 | `				}` |
|       5 | 1609 | `				OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,FALSE);` |
|       4 | 1610 | `				if( (pValue->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ))` |
|       3 | 1611 | `				 && (pValue->iFlags & MEMOBJ_NULL) == 0 ){` |
|     ! 0 | 1612 | `					rc = PH7_MemObjDump(&(*pOut),pValue,FALSE,nTab+8,nDepth,0);` |
|     ! 0 | 1613 | `					SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|     ! 0 | 1614 | `					if( rc == SXERR_LIMIT ){` |
|     ! 0 | 1615 | `						break;` |
|       - | 1616 | `					}` |
|     ! 0 | 1617 | `				}else{` |
|       5 | 1618 | `					PH7_MemObjPrintRInline(&(*pOut),pValue);` |
|       5 | 1619 | `					SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 1620 | `				}` |
|       - | 1621 | `			}` |
|       7 | 1622 | `		}` |
|       2 | 1623 | `	}` |
|    3854 | 1624 | `	for( i = 0 ; i < nTab ; i++ ){` |
|    3721 | 1625 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|    1861 | 1626 | `	}` |
|     134 | 1627 | `	if( ShowType ){` |
|     132 | 1628 | `		SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      67 | 1629 | `	}else{` |
|       3 | 1630 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       - | 1631 | `	}` |
|     134 | 1632 | `	return rc;` |
|      73 | 1633 | `}` |
|       - | 1634 | `/*` |
|       - | 1635 | ` * Call a magic method [i.e: __toString(),__toBool(),__Invoke()...]` |
|       - | 1636 | ` * Return SXRET_OK on successfull call. Any other return value indicates failure.` |
|       - | 1637 | ` * Notes on magic methods.` |
|       - | 1638 | ` * According to the PHP language reference manual.` |
|       - | 1639 | ` *  The function names __construct(), __destruct(), __call(), __callStatic()` |
|       - | 1640 | ` *  __get(),  __toString(), __invoke(), __clone() are magical in PHP classes.` |
|       - | 1641 | ` * You cannot have functions with these names in any of your classes unless` |
|       - | 1642 | ` * you want the magic functionality associated with them.` |
|       - | 1643 | ` * Example of magical methods:` |
|       - | 1644 | ` * __toString()` |
|       - | 1645 | ` *  The __toString() method allows a class to decide how it will react when it is treated like` |
|       - | 1646 | ` *  a string. For example, what echo $obj; will print. This method must return a string.` |
|       - | 1647 | ` *  Example #2 Simple example` |
|       - | 1648 | ` * <?php` |
|       - | 1649 | ` * // Declare a simple class` |
|       - | 1650 | ` * class TestClass` |
|       - | 1651 | ` * {` |
|       - | 1652 | ` *   public $foo;` |
|       - | 1653 | ` *` |
|       - | 1654 | ` *   public function __construct($foo)` |
|       - | 1655 | ` *   {` |
|       - | 1656 | ` *       $this->foo = $foo;` |
|       - | 1657 | ` *   }` |
|       - | 1658 | ` *` |
|       - | 1659 | ` *   public function __toString()` |
|       - | 1660 | ` *   {` |
|       - | 1661 | ` *       return $this->foo;` |
|       - | 1662 | ` *   }` |
|       - | 1663 | ` * }` |
|       - | 1664 | ` * $class = new TestClass('Hello');` |
|       - | 1665 | ` * echo $class;` |
|       - | 1666 | ` * ?>` |
|       - | 1667 | ` * The above example will output:` |
|       - | 1668 | ` *  Hello` |
|       - | 1669 | ` *` |
|       - | 1670 | ` * Note that PH7 does not support all the magical method and introudces __toFloat(),__toInt()` |
|       - | 1671 | ` * which have the same behaviour as __toString() but for float and integer types` |
|       - | 1672 | ` * respectively.` |
|       - | 1673 | ` * Refer to the official documentation for more information.` |
|       - | 1674 | ` */` |
|      54 | 1675 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallMagicMethod(` |
|       - | 1676 | `	ph7_vm *pVm,               /* VM that own all this stuff */` |
|       - | 1677 | `	ph7_class *pClass,         /* Target class */` |
|       - | 1678 | `	ph7_class_instance *pThis, /* Target object */` |
|       - | 1679 | `	const char *zMethod,       /* Magic method name [i.e: __toString()]*/` |
|       - | 1680 | `	sxu32 nByte,               /* zMethod length*/` |
|       - | 1681 | `	const SyString *pAttrName, /* Attribute name */` |
|       - | 1682 | `	ph7_value *pResult         /* OUT: magic method return value. NULL to discard */` |
|       - | 1683 | `	)` |
|       1 | 1684 | `{` |
|      55 | 1685 | `	ph7_value *apArg[2] = { 0 , 0 };` |
|       - | 1686 | `	ph7_class_method *pMeth;` |
|       - | 1687 | `	ph7_value sAttr; /* cc warning */` |
|       - | 1688 | `	sxi32 rc;` |
|       - | 1689 | `	int nArg;` |
|       - | 1690 | `	/* Make sure the magic method is available */` |
|      55 | 1691 | `	pMeth = PH7_ClassExtractMethod(&(*pClass),zMethod,nByte);` |
|      55 | 1692 | `	if( pMeth == 0 ){` |
|       - | 1693 | `		/* No such method,return immediately */` |
|     ! 0 | 1694 | `		return SXERR_NOTFOUND;` |
|       - | 1695 | `	}` |
|      55 | 1696 | `	nArg = 0;` |
|       - | 1697 | `	/* Copy arguments */` |
|      55 | 1698 | `	if( pAttrName ){` |
|      55 | 1699 | `		PH7_MemObjInitFromString(pVm,&sAttr,pAttrName);` |
|      55 | 1700 | `		sAttr.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      55 | 1701 | `		apArg[0] = &sAttr;` |
|      55 | 1702 | `		nArg = 1;` |
|      27 | 1703 | `	}` |
|       - | 1704 | `	/* Call the magic method now */` |
|      55 | 1705 | `	rc = PH7_VmCallClassMethod(pVm,&(*pThis),pMeth,pResult,nArg,apArg);` |
|       - | 1706 | `	/* Clean up */` |
|      55 | 1707 | `	if( pAttrName ){` |
|      55 | 1708 | `		PH7_MemObjRelease(&sAttr);` |
|      27 | 1709 | `	}` |
|      55 | 1710 | `	return rc;` |
|      28 | 1711 | `}` |
|       - | 1712 | `/*` |
|       - | 1713 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon].` |
|       - | 1714 | ` * This function is simply a wrapper on ExtractClassAttrValue().` |
|       - | 1715 | ` */` |
|   10828 | 1716 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceExtractAttrValue(ph7_class_instance *pThis,VmClassAttr *pAttr)` |
|       5 | 1717 | `{` |
|       - | 1718 | `   /* Extract the attribute value */` |
|       - | 1719 | `	ph7_value *pValue;` |
|   10833 | 1720 | `	pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|   10833 | 1721 | `	return pValue;` |
|       5 | 1722 | `}` |
|       - | 1723 | `/*` |
|       - | 1724 | ` * Convert a class instance [i.e: Object in the PHP jargon] into a hashmap [i.e: array in the PHP jargon].` |
|       - | 1725 | ` * Return SXRET_OK on success. Any other value indicates failure.` |
|       - | 1726 | ` * Note on object conversion to array:` |
|       - | 1727 | ` *  Acccording to the PHP language reference manual` |
|       - | 1728 | ` *  If an object is converted to an array, the result is an array whose elements are the object's properties.` |
|       - | 1729 | ` *  The keys are the member variable names.` |
|       - | 1730 | ` *` |
|       - | 1731 | ` *  The following example:` |
|       - | 1732 | ` *  class Test {` |
|       - | 1733 | ` *   public $A = 25<<1;  // 50` |
|       - | 1734 | ` *	 public $c = rand_str(3);   // Random string of length 3` |
|       - | 1735 | ` *	 public $d = rand() & 1023; // Random number between 0..1023` |
|       - | 1736 | ` *  }` |
|       - | 1737 | ` *  var_dump((array) new Test());` |
|       - | 1738 | ` *	Will output:` |
|       - | 1739 | ` *  array(3) {` |
|       - | 1740 | ` *   [A] =>` |
|       - | 1741 | ` *      int(50)` |
|       - | 1742 | ` *   [c] =>` |
|       - | 1743 | ` *     string(3 'aps')` |
|       - | 1744 | ` *   [d] =>` |
|       - | 1745 | ` *     int(991)` |
|       - | 1746 | ` *  }` |
|       - | 1747 | ` * You have noticed that PH7 allow class attributes [i.e: $a,$c,$d in the example above]` |
|       - | 1748 | ` * have any complex expression (even function calls/Annonymous functions) as their default` |
|       - | 1749 | ` * value unlike the standard PHP engine.` |
|       - | 1750 | ` * This is a very powerful feature that you have to look at.` |
|       - | 1751 | ` */` |
|      14 | 1752 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|       1 | 1753 | `{` |
|       - | 1754 | `	SyHashEntry *pEntry;` |
|       - | 1755 | `	SyString *pAttrName;` |
|       - | 1756 | `	VmClassAttr *pAttr;` |
|       - | 1757 | `	ph7_value *pValue;` |
|       - | 1758 | `	ph7_value sName;` |
|       - | 1759 | `	/* Reset the loop cursor */` |
|      15 | 1760 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      15 | 1761 | `	PH7_MemObjInitFromString(pThis->pVm,&sName,0);` |
|      51 | 1762 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       - | 1763 | `		/* Point to the current attribute */` |
|      37 | 1764 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      37 | 1765 | `		if( pAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|       - | 1766 | `			/* php 8.4: a VIRTUAL hooked property has no backing store — the` |
|       - | 1767 | `			 * (array) cast excludes it (raw surface, get is NOT dispatched) */` |
|       7 | 1768 | `			continue;` |
|       - | 1769 | `		}` |
|       - | 1770 | `		/* Extract attribute value */` |
|      31 | 1771 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|      31 | 1772 | `		if( pValue ){` |
|       - | 1773 | `			/* Build attribute name */` |
|      31 | 1774 | `			pAttrName = &pAttr->pAttr->sName;` |
|      31 | 1775 | `			PH7_MemObjStringAppend(&sName,pAttrName->zString,pAttrName->nByte);` |
|       - | 1776 | `			/* Perform the insertion */` |
|      31 | 1777 | `			PH7_HashmapInsert(pMap,&sName,pValue);` |
|       - | 1778 | `			/* Reset the string cursor */` |
|      31 | 1779 | `			SyBlobReset(&sName.sBlob);` |
|      15 | 1780 | `		}` |
|       1 | 1781 | `	}` |
|      15 | 1782 | `	PH7_MemObjRelease(&sName);` |
|      15 | 1783 | `	return SXRET_OK;` |
|       1 | 1784 | `}` |
|       - | 1785 | `/*` |
|       - | 1786 | ` * Iterate throw class attributes and invoke the given callback [i.e: xWalk()] for each` |
|       - | 1787 | ` * retrieved attribute.` |
|       - | 1788 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|       - | 1789 | ` * the attribute value in the callback body will not alter the real attribute value.` |
|       - | 1790 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|       - | 1791 | ` * a value different from PH7_OK.` |
|       - | 1792 | ` * Refer to [ph7_object_walk()] for more information.` |
|       - | 1793 | ` */` |
|     ! 0 | 1794 | `PH7_PRIVATE sxi32 PH7_ClassInstanceWalk(` |
|       - | 1795 | `	ph7_class_instance *pThis, /* Target object */` |
|       - | 1796 | `	int (*xWalk)(const char *,ph7_value *,void *), /* Walker callback */` |
|       - | 1797 | `	void *pUserData /* Last argument to xWalk() */` |
|       - | 1798 | `	)` |
|     ! 0 | 1799 | `{` |
|       - | 1800 | `	SyHashEntry *pEntry; /* Hash entry */` |
|       - | 1801 | `	VmClassAttr *pAttr;  /* Pointer to the attribute */` |
|       - | 1802 | `	ph7_value *pValue;   /* Attribute value */` |
|       - | 1803 | `	ph7_value sValue;    /* Copy of the attribute value */` |
|       - | 1804 | `	int rc;` |
|       - | 1805 | `	/* Reset the loop cursor */` |
|     ! 0 | 1806 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|     ! 0 | 1807 | `	PH7_MemObjInit(pThis->pVm,&sValue);` |
|       - | 1808 | `	/* Start the walk process */` |
|     ! 0 | 1809 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       - | 1810 | `		/* Point to the current attribute */` |
|     ! 0 | 1811 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1812 | `		/* Extract attribute value */` |
|     ! 0 | 1813 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     ! 0 | 1814 | `		if( pValue ){` |
|     ! 0 | 1815 | `			PH7_MemObjLoad(pValue,&sValue);` |
|       - | 1816 | `			/* Invoke the supplied callback */` |
|     ! 0 | 1817 | `			rc =  xWalk(SyStringData(&pAttr->pAttr->sName),&sValue,pUserData);` |
|     ! 0 | 1818 | `			PH7_MemObjRelease(&sValue);` |
|     ! 0 | 1819 | `			if( rc != PH7_OK){` |
|       - | 1820 | `				/* User callback request an operation abort */` |
|     ! 0 | 1821 | `				return SXERR_ABORT;` |
|       - | 1822 | `			}` |
|     ! 0 | 1823 | `		}` |
|     ! 0 | 1824 | `	}` |
|       - | 1825 | `	/* All done */` |
|     ! 0 | 1826 | `	return SXRET_OK;` |
|     ! 0 | 1827 | `}` |
|       - | 1828 | `/*` |
|       - | 1829 | ` * Extract a class atrribute value.` |
|       - | 1830 | ` * Return a pointer to the attribute value on success. Otherwise NULL.` |
|       - | 1831 | ` * Note:` |
|       - | 1832 | ` *  Access to static and constant attribute is not allowed. That is,the function` |
|       - | 1833 | ` *  will return NULL in case someone (host-application code) try to extract` |
|       - | 1834 | ` *  a static/constant attribute.` |
|       - | 1835 | ` */` |
|   11030 | 1836 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceFetchAttr(ph7_class_instance *pThis,const SyString *pName)` |
|       5 | 1837 | `{` |
|       - | 1838 | `	SyHashEntry *pEntry;` |
|       - | 1839 | `	VmClassAttr *pAttr;` |
|       - | 1840 | `	/* Query the attribute hashtable */` |
|   11035 | 1841 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);` |
|   11035 | 1842 | `	if( pEntry == 0 ){` |
|       - | 1843 | `		/* No such attribute */` |
|     ! 0 | 1844 | `		return 0;` |
|       - | 1845 | `	}` |
|       - | 1846 | `	/* Point to the class atrribute */` |
|   11035 | 1847 | `	pAttr = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1848 | `	/* Check if we are dealing with a static/constant attribute */` |
|   11035 | 1849 | `	if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|       - | 1850 | `		/* Access is forbidden */` |
|     ! 0 | 1851 | `		return 0;` |
|       - | 1852 | `	}` |
|       - | 1853 | `	/* Return the attribute value */` |
|   11035 | 1854 | `	return ExtractClassAttrValue(pThis->pVm,pAttr);` |
|    5520 | 1855 | `}` |
|       - | 1856 |  |
