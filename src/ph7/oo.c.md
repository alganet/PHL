# src/ph7/oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 687/781 lines (87.96%)

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
|  408930 |   14 | `PH7_PRIVATE ph7_class * PH7_NewRawClass(ph7_vm *pVm,const SyString *pName,sxu32 nLine)` |
|       5 |   15 | `{` |
|       - |   16 | `	ph7_class *pClass;` |
|       - |   17 | `	char *zName;` |
|       - |   18 | `	/* Allocate a new instance */` |
|  408935 |   19 | `	pClass = (ph7_class *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class));` |
|  408935 |   20 | `	if( pClass == 0 ){` |
|     ! 0 |   21 | `		return 0;` |
|       - |   22 | `	}` |
|       - |   23 | `	/* Zero the structure */` |
|  408935 |   24 | `	SyZero(pClass,sizeof(ph7_class));` |
|       - |   25 | `	/* Duplicate class name */` |
|  408935 |   26 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  408935 |   27 | `	if( zName == 0 ){` |
|     ! 0 |   28 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClass);` |
|     ! 0 |   29 | `		return 0;` |
|       - |   30 | `	}` |
|       - |   31 | `	/* Initialize fields */` |
|  408935 |   32 | `	SyStringInitFromBuf(&pClass->sName,zName,pName->nByte);` |
|  408935 |   33 | `	SyHashInit(&pClass->hMethod,&pVm->sAllocator,0,0);` |
|  408935 |   34 | `	SyHashInit(&pClass->hAttr,&pVm->sAllocator,0,0);` |
|  408935 |   35 | `	SyHashInit(&pClass->hDerived,&pVm->sAllocator,0,0);` |
|  408935 |   36 | `	SySetInit(&pClass->aInterface,&pVm->sAllocator,sizeof(ph7_class *));` |
|  408935 |   37 | `	SySetInit(&pClass->aTrait,&pVm->sAllocator,sizeof(ph7_class *));` |
|  408935 |   38 | `	SySetInit(&pClass->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|  408935 |   39 | `	SySetInit(&pClass->aEnumCases,&pVm->sAllocator,sizeof(ph7_class_attr *));` |
|  408935 |   40 | `	pClass->nLine = nLine;` |
|  408935 |   41 | `	if( pVm->bCompilingBuiltin ){` |
|       - |   42 | `		/* Defined by an embedded builtin chunk: internal, no defining file.` |
|       - |   43 | `		 * Class compilers merge further flags with \|= so this survives. */` |
|  406985 |   44 | `		pClass->iFlags \|= PH7_CLASS_INTERNAL;` |
|  203495 |   45 | `	}else{` |
|       - |   46 | `		/* Alias the VM-lifetime path dup on top of the include stack */` |
|    1955 |   47 | `		SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|    1955 |   48 | `		if( pFile ){` |
|    1955 |   49 | `			SyStringDupPtr(&pClass->sFile,pFile);` |
|     975 |   50 | `		}` |
|       - |   51 | `	}` |
|       - |   52 | `	/* All done */` |
|  408935 |   53 | `	return pClass;` |
|  204470 |   54 | `}` |
|       - |   55 | `/*` |
|       - |   56 | ` * Allocate and initialize a new class attribute.` |
|       - |   57 | ` * Return a pointer to the class attribute on success. NULL otherwise.` |
|       - |   58 | ` */` |
|  698954 |   59 | `PH7_PRIVATE ph7_class_attr * PH7_NewClassAttr(ph7_vm *pVm,const SyString *pName,sxu32 nLine,sxi32 iProtection,sxi32 iFlags)` |
|       5 |   60 | `{` |
|       - |   61 | `	ph7_class_attr *pAttr;` |
|       - |   62 | `	char *zName;` |
|  698959 |   63 | `	pAttr = (ph7_class_attr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_attr));` |
|  698959 |   64 | `	if( pAttr == 0 ){` |
|     ! 0 |   65 | `		return 0;` |
|       - |   66 | `	}` |
|       - |   67 | `	/* Zero the structure */` |
|  698959 |   68 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|  698959 |   69 | `	SySetInit(&pAttr->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|       - |   70 | `	/* Duplicate attribute name */` |
|  698959 |   71 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  698959 |   72 | `	if( zName == 0 ){` |
|     ! 0 |   73 | `		SyMemBackendPoolFree(&pVm->sAllocator,pAttr);` |
|     ! 0 |   74 | `		return 0;` |
|       - |   75 | `	}` |
|       - |   76 | `	/* Initialize fields */` |
|  698959 |   77 | `	SySetInit(&pAttr->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|  698959 |   78 | `	SySetInit(&pAttr->aUnionAlts,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|  698959 |   79 | `	SyStringInitFromBuf(&pAttr->sName,zName,pName->nByte);` |
|  698959 |   80 | `	pAttr->iProtection = iProtection;` |
|  698959 |   81 | `	pAttr->nIdx = SXU32_HIGH;` |
|  698959 |   82 | `	pAttr->iFlags = iFlags;` |
|  698959 |   83 | `	pAttr->nLine = nLine;` |
|  698959 |   84 | `	return pAttr;` |
|  349482 |   85 | `}` |
|       - |   86 | `/*` |
|       - |   87 | ` * Allocate and initialize a new class method.` |
|       - |   88 | ` * Return a pointer to the class method on success. NULL otherwise` |
|       - |   89 | ` * This function associate with the newly created method an automatically generated` |
|       - |   90 | ` * random unique name.` |
|       - |   91 | ` */` |
| 2276950 |   92 | `PH7_PRIVATE ph7_class_method * PH7_NewClassMethod(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,sxu32 nLine,` |
|       - |   93 | `	sxi32 iProtection,sxi32 iFlags,sxi32 iFuncFlags)` |
|       5 |   94 | `{` |
|       - |   95 | `	ph7_class_method *pMeth;` |
|       - |   96 | `	SyHashEntry *pEntry;` |
|       - |   97 | `	SyString *pNamePtr;` |
|       - |   98 | `	char zSalt[10];` |
|       - |   99 | `	char *zName;` |
|       - |  100 | `	sxu32 nByte;` |
|       - |  101 | `	/* Allocate a new class method instance */` |
| 2276955 |  102 | `	pMeth = (ph7_class_method *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_method));` |
| 2276955 |  103 | `	if( pMeth == 0 ){` |
|     ! 0 |  104 | `		return 0;` |
|       - |  105 | `	}` |
|       - |  106 | `	/* Zero the structure */` |
| 2276955 |  107 | `	SyZero(pMeth,sizeof(ph7_class_method));` |
|       - |  108 | `	/* Check for an already installed method with the same name */` |
| 2276955 |  109 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
| 2276955 |  110 | `	if( pEntry == 0 ){` |
|       - |  111 | `		/* Associate an unique VM name to this method */` |
| 2276953 |  112 | `		nByte = sizeof(zSalt) + pName->nByte + SyStringLength(&pClass->sName)+sizeof(char)*7/*[[__'\0'*/;` |
| 2276953 |  113 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,nByte);` |
| 2276953 |  114 | `		if( zName == 0 ){` |
|     ! 0 |  115 | `			SyMemBackendPoolFree(&pVm->sAllocator,pMeth);` |
|     ! 0 |  116 | `			return 0;` |
|       - |  117 | `		}` |
| 2276953 |  118 | `		pNamePtr = &pMeth->sVmName;` |
|       - |  119 | `		/* Generate a random string */` |
| 2276953 |  120 | `		PH7_VmRandomString(&(*pVm),zSalt,sizeof(zSalt));` |
| 2276953 |  121 | `		pNamePtr->nByte = SyBufferFormat(zName,nByte,"[__%z@%z_%.*s]",&pClass->sName,pName,sizeof(zSalt),zSalt);` |
| 2276953 |  122 | `		pNamePtr->zString = zName;` |
| 1138479 |  123 | `	}else{` |
|       - |  124 | `		/* Method is condidate for 'overloading' */` |
|       3 |  125 | `		ph7_class_method *pCurrent = (ph7_class_method *)pEntry->pUserData;` |
|       3 |  126 | `		pNamePtr = &pMeth->sVmName;` |
|       - |  127 | `		/* Use the same VM name */` |
|       3 |  128 | `		SyStringDupPtr(pNamePtr,&pCurrent->sVmName);` |
|       3 |  129 | `		zName = (char *)pNamePtr->zString;` |
|       - |  130 | `	}` |
| 2276955 |  131 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|  127966 |  132 | `		if( (pName->nByte == sizeof("__destruct") - 1 && SyMemcmp(pName->zString,"__destruct",sizeof("__destruct") - 1 ) == 0)` |
|  127971 |  133 | `			\|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|       - |  134 | `				/* Switch to public visibility for destructors and legacy class-name` |
|       - |  135 | `				 * constructors (the engine invokes destructors internally, bypassing` |
|       - |  136 | `				 * visibility either way). __construct KEEPS its declared visibility` |
|       - |  137 | ``				 * (band A #4): php enforces it at `new` — a private/protected ctor`` |
|       - |  138 | `				 * from the wrong scope is a catchable Error, checked at OP_NEW —` |
|       - |  139 | `				 * and ReflectionClass::isInstantiable()/newInstance() now see it. */` |
|     ! 0 |  140 | `				iProtection = PH7_CLASS_PROT_PUBLIC;` |
|     ! 0 |  141 | `		}` |
|   63983 |  142 | `	}` |
|       - |  143 | `	/* Initialize method fields */` |
| 2276955 |  144 | `	pMeth->iProtection = iProtection;` |
| 2276955 |  145 | `	pMeth->iFlags = iFlags;` |
| 2276955 |  146 | `	pMeth->nLine = nLine;` |
| 3415430 |  147 | `	PH7_VmInitFuncState(&(*pVm),&pMeth->sFunc,&zName[sizeof(char)*4/*[__@*/+SyStringLength(&pClass->sName)],` |
| 2276950 |  148 | `		pName->nByte,iFuncFlags\|VM_FUNC_CLASS_METHOD,pClass);` |
| 2276955 |  149 | `	return pMeth;` |
| 1138480 |  150 | `}` |
|       - |  151 | `/*` |
|       - |  152 | ` * Check if the given name have a class method associated with it.` |
|       - |  153 | ` * Return the desired method [i.e: ph7_class_method instance] on success. NULL otherwise.` |
|       - |  154 | ` */` |
|  653630 |  155 | `PH7_PRIVATE ph7_class_method * PH7_ClassExtractMethod(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|       5 |  156 | `{` |
|       - |  157 | `	SyHashEntry *pEntry;` |
|       - |  158 | `	/* Perform a hash lookup */` |
|  653635 |  159 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)zName,nByte);` |
|  653635 |  160 | `	if( pEntry == 0 ){` |
|       - |  161 | `		/* No such entry */` |
|    8365 |  162 | `		return 0;` |
|       - |  163 | `	}` |
|       - |  164 | `	/* Point to the desired method */` |
|  645275 |  165 | `	return (ph7_class_method *)pEntry->pUserData;` |
|  326820 |  166 | `}` |
|       - |  167 | `/*` |
|       - |  168 | ` * Check if the given name is a class attribute.` |
|       - |  169 | ` * Return the desired attribute [i.e: ph7_class_attr instance] on success.NULL otherwise.` |
|       - |  170 | ` */` |
|  408786 |  171 | `PH7_PRIVATE ph7_class_attr * PH7_ClassExtractAttribute(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|       5 |  172 | `{` |
|       - |  173 | `	SyHashEntry *pEntry;` |
|       - |  174 | `	/* Perform a hash lookup */` |
|  408791 |  175 | `	pEntry = SyHashGet(&pClass->hAttr,(const void *)zName,nByte);` |
|  408791 |  176 | `	if( pEntry == 0 ){` |
|       - |  177 | `		/* No such entry */` |
|  408099 |  178 | `		return 0;` |
|       - |  179 | `	}` |
|       - |  180 | `	/* Point to the desierd method */` |
|     697 |  181 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|  204398 |  182 | `}` |
|       - |  183 | `/*` |
|       - |  184 | ` * Install a class attribute in the corresponding container.` |
|       - |  185 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|       - |  186 | ` */` |
|  698950 |  187 | `PH7_PRIVATE sxi32 PH7_ClassInstallAttr(ph7_class *pClass,ph7_class_attr *pAttr)` |
|       5 |  188 | `{` |
|  698955 |  189 | `	SyString *pName = &pAttr->sName;` |
|       - |  190 | `	sxi32 rc;` |
|       - |  191 | `	/* Remember where this attribute was originally declared so that later` |
|       - |  192 | `	 * inheritance/trait copies still know the declaring class (needed for` |
|       - |  193 | `	 * PHP-compatible error messages on typed properties). */` |
|  698955 |  194 | `	if( pAttr->pDeclClass == 0 ){` |
|  698955 |  195 | `		pAttr->pDeclClass = pClass;` |
|  349475 |  196 | `	}` |
|  698955 |  197 | `	rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|  698955 |  198 | `	return rc;` |
|       5 |  199 | `}` |
|       - |  200 | `/*` |
|       - |  201 | ` * Install a class method in the corresponding container.` |
|       - |  202 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|       - |  203 | ` */` |
| 2276938 |  204 | `PH7_PRIVATE sxi32 PH7_ClassInstallMethod(ph7_class *pClass,ph7_class_method *pMeth)` |
|       5 |  205 | `{` |
| 2276943 |  206 | `	SyString *pName = &pMeth->sFunc.sName;` |
|       - |  207 | `	sxi32 rc;` |
| 2276943 |  208 | `	rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 2276943 |  209 | `	return rc;` |
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
|  155240 |  234 | `static int OoClassifyOverrideType(ph7_vm *pVm, sxu32 nType, const SyString *pClass,` |
|       - |  235 | `	int bUnion, ph7_class **ppClass)` |
|       5 |  236 | `{` |
|  155245 |  237 | `	*ppClass = 0;` |
|  155245 |  238 | `	if( bUnion ){` |
|       3 |  239 | `		return OVT_SKIP; /* union/intersection — full lattice, skip */` |
|       - |  240 | `	}` |
|  155243 |  241 | `	if( nType == 0 ){` |
|  155157 |  242 | `		return OVT_NONE; /* no declared type */` |
|       - |  243 | `	}` |
|      91 |  244 | `	if( nType == SXU32_HIGH ){` |
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
|      70 |  267 | `	if( nType == MEMOBJ_STRING \|\| nType == MEMOBJ_INT \|\| nType == MEMOBJ_REAL` |
|      29 |  268 | `	 \|\| nType == MEMOBJ_BOOL \|\| nType == MEMOBJ_HASHMAP ){` |
|      50 |  269 | `		return OVT_SCALAR;` |
|       - |  270 | `	}` |
|       - |  271 | `	/* MEMOBJ_OBJ (object — subtypes against classes), MEMOBJ_VOID/NULL/RES,` |
|       - |  272 | `	 * or anything unexpected: skip. */` |
|      26 |  273 | `	return OVT_SKIP;` |
|   77625 |  274 | `}` |
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
|  124172 |  288 | `static OvType OoTypeFromReturn(ph7_vm_func *pF)` |
|       5 |  289 | `{` |
|       - |  290 | `	OvType t;` |
|  124177 |  291 | `	t.nType = pF->nReturnType;` |
|  124177 |  292 | `	t.pClass = &pF->sReturnClass;` |
|  124177 |  293 | `	t.bUnion = SySetUsed(&pF->aReturnUnion) > 0;` |
|  124177 |  294 | `	t.bNullable = (pF->iFlags & VM_FUNC_RETURN_NULLABLE) != 0;` |
|  124177 |  295 | `	return t;` |
|       5 |  296 | `}` |
|   31068 |  297 | `static OvType OoTypeFromArg(ph7_vm_func_arg *pA)` |
|       5 |  298 | `{` |
|       - |  299 | `	OvType t;` |
|   31073 |  300 | `	t.nType = pA->nType;` |
|   31073 |  301 | `	t.pClass = &pA->sClass;` |
|   31073 |  302 | `	t.bUnion = (pA->iFlags & VM_FUNC_ARG_UNION) != 0;` |
|   31073 |  303 | `	t.bNullable = (pA->iFlags & VM_FUNC_ARG_NULLABLE) != 0;` |
|   31073 |  304 | `	return t;` |
|       5 |  305 | `}` |
|       - |  306 | `/*` |
|       - |  307 | ` * Return TRUE if the child type is an unambiguously-invalid override of the` |
|       - |  308 | ` * parent type. bCovariant=1 for a return type (child must be ⊆ parent),` |
|       - |  309 | ` * 0 for a parameter (child must be ⊇ parent). Returns FALSE (accept) on any` |
|       - |  310 | ` * skipped/ambiguous shape.` |
|       - |  311 | ` */` |
|   77620 |  312 | `static int OoOverrideTypeBad(ph7_vm *pVm, OvType parent, OvType child, int bCovariant)` |
|       5 |  313 | `{` |
|       - |  314 | `	ph7_class *pParentCls, *pChildCls;` |
|   77625 |  315 | `	int kP = OoClassifyOverrideType(pVm, parent.nType, parent.pClass, parent.bUnion, &pParentCls);` |
|   77625 |  316 | `	int kC = OoClassifyOverrideType(pVm, child.nType, child.pClass, child.bUnion, &pChildCls);` |
|   77625 |  317 | `	if( kP == OVT_SKIP \|\| kC == OVT_SKIP ){` |
|      18 |  318 | `		return 0; /* ambiguous shape — conservatively accept */` |
|       - |  319 | `	}` |
|       - |  320 | `	/* A missing type is the TOP type. covariant (return): a concrete child is a` |
|       - |  321 | `	 * subtype of top, fine; a top child over a concrete parent WIDENS → bad.` |
|       - |  322 | `	 * contravariant (param): a top child is a supertype of anything, fine; a` |
|       - |  323 | `	 * concrete child over a top parent NARROWS → bad. (A union/intersection child` |
|       - |  324 | `	 * already fell into OVT_SKIP above, so a flagged child here is scalar/class.) */` |
|   77609 |  325 | `	if( kP == OVT_NONE \|\| kC == OVT_NONE ){` |
|   77585 |  326 | `		if( bCovariant && kC == OVT_NONE && kP != OVT_NONE ) return 1;` |
|   77585 |  327 | `		if( !bCovariant && kP == OVT_NONE && kC != OVT_NONE ) return 1;` |
|   77585 |  328 | `		return 0;` |
|       - |  329 | `	}` |
|       - |  330 | `	/* Nullability: a covariant return may not ADD null; a contravariant param may` |
|       - |  331 | `	 * not REMOVE null. */` |
|      29 |  332 | `	if( bCovariant ){` |
|      11 |  333 | `		if( child.bNullable && !parent.bNullable ) return 1;` |
|       7 |  334 | `	}else{` |
|      19 |  335 | `		if( parent.bNullable && !child.bNullable ) return 1;` |
|       - |  336 | `	}` |
|      29 |  337 | `	if( kP == OVT_SCALAR && kC == OVT_SCALAR ){` |
|       - |  338 | `		/* Scalars are invariant — they must match exactly. */` |
|      22 |  339 | `		return (parent.nType != child.nType) ? 1 : 0;` |
|       - |  340 | `	}` |
|       8 |  341 | `	if( kP == OVT_CLASS && kC == OVT_CLASS ){` |
|       8 |  342 | `		if( bCovariant ){` |
|       3 |  343 | `			return PH7_VmInstanceOf(pChildCls, pParentCls) ? 0 : 1;  /* child ⊆ parent */` |
|       - |  344 | `		}` |
|       6 |  345 | `		return PH7_VmInstanceOf(pParentCls, pChildCls) ? 0 : 1;      /* child ⊇ parent */` |
|       - |  346 | `	}` |
|       - |  347 | `	/* One scalar and one class — disjoint. */` |
|     ! 0 |  348 | `	return 1;` |
|   38815 |  349 | `}` |
|       - |  350 |  |
|       - |  351 | `/*` |
|       - |  352 | ` * Check a child method's signature against the parent method it overrides.` |
|       - |  353 | ` * Emits a PHP-style "Declaration of … must be compatible …" fatal on a clear` |
|       - |  354 | `` * incompatibility. `__construct` is exempt (PHP does not apply variance to it).`` |
|       - |  355 | ` */` |
|   89224 |  356 | `static sxi32 OoCheckOverrideCompat(ph7_gen_state *pGen, ph7_class *pBase, ph7_class *pSub,` |
|       - |  357 | `	ph7_class_method *pParent, ph7_class_method *pChild)` |
|       5 |  358 | `{` |
|   89229 |  359 | `	ph7_vm *pVm = pGen->pVm;` |
|   89229 |  360 | `	ph7_vm_func *pPF = &pParent->sFunc;` |
|   89229 |  361 | `	ph7_vm_func *pCF = &pChild->sFunc;` |
|   89229 |  362 | `	SyString *pMName = &pCF->sName;` |
|       - |  363 | `	ph7_vm_func_arg *aP, *aC;` |
|       - |  364 | `	sxu32 nPArg, nCArg, k;` |
|   89229 |  365 | `	int bBad = 0;` |
|   89224 |  366 | `	if( pMName->nByte == sizeof("__construct")-1` |
|   58190 |  367 | `	 && SyStrnmicmp(pMName->zString,"__construct",pMName->nByte) == 0 ){` |
|   27143 |  368 | `		return SXRET_OK;` |
|       - |  369 | `	}` |
|       - |  370 | `	/* Return type — covariant. */` |
|   62091 |  371 | `	bBad = OoOverrideTypeBad(pVm, OoTypeFromReturn(pPF), OoTypeFromReturn(pCF), /* bCovariant */ 1);` |
|       - |  372 | `	/* Each overlapping parameter — contravariant. */` |
|   62091 |  373 | `	nPArg = SySetUsed(&pPF->aArgs);` |
|   62091 |  374 | `	nCArg = SySetUsed(&pCF->aArgs);` |
|   62091 |  375 | `	aP = (ph7_vm_func_arg *)SySetBasePtr(&pPF->aArgs);` |
|   62091 |  376 | `	aC = (ph7_vm_func_arg *)SySetBasePtr(&pCF->aArgs);` |
|   77625 |  377 | `	for( k = 0; !bBad && k < nPArg && k < nCArg; k++ ){` |
|   15539 |  378 | `		bBad = OoOverrideTypeBad(pVm, OoTypeFromArg(&aP[k]), OoTypeFromArg(&aC[k]), /* bCovariant */ 0);` |
|    7772 |  379 | `	}` |
|       - |  380 | `	/* Parameter arity: the child must declare at least the parent's parameters and` |
|       - |  381 | `	 * may add only OPTIONAL ones — PHP rejects dropping any param (even an optional` |
|       - |  382 | `	 * one) or adding a required one. Skip the rule if either signature is variadic` |
|       - |  383 | `	 * (arity semantics differ). */` |
|   62091 |  384 | `	if( !bBad ){` |
|   62087 |  385 | `		int bVariadic = 0;` |
|   77619 |  386 | `		for( k = 0; k < nPArg; k++ ){ if( aP[k].iFlags & VM_FUNC_ARG_VARIADIC ) bVariadic = 1; }` |
|   77621 |  387 | `		for( k = 0; k < nCArg; k++ ){ if( aC[k].iFlags & VM_FUNC_ARG_VARIADIC ) bVariadic = 1; }` |
|   62087 |  388 | `		if( !bVariadic ){` |
|   62087 |  389 | `			if( nCArg < nPArg ){` |
|     ! 0 |  390 | `				bBad = 1; /* dropped a parent parameter */` |
|     ! 0 |  391 | `			}else{` |
|   62089 |  392 | `				for( k = nPArg; k < nCArg; k++ ){` |
|       3 |  393 | `					if( SySetUsed(&aC[k].aByteCode) == 0 ){ bBad = 1; break; } /* new required */` |
|       2 |  394 | `				}` |
|       - |  395 | `			}` |
|   31041 |  396 | `		}` |
|   31041 |  397 | `	}` |
|   62091 |  398 | `	if( bBad ){` |
|       8 |  399 | `		sxi32 rc = PH7_GenCompileError(&(*pGen),E_ERROR,pChild->nLine,` |
|       - |  400 | `			"Declaration of %z::%z() must be compatible with %z::%z()",` |
|       2 |  401 | `			&pSub->sName,pMName,&pBase->sName,&pParent->sFunc.sName);` |
|       6 |  402 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  403 | `			return SXERR_ABORT;` |
|       - |  404 | `		}` |
|       2 |  405 | `	}` |
|   62091 |  406 | `	return SXRET_OK;` |
|   44617 |  407 | `}` |
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
|  182422 |  449 | `PH7_PRIVATE sxi32 PH7_ClassInherit(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase)` |
|       5 |  450 | `{` |
|       - |  451 | `	ph7_class_method *pMeth;` |
|       - |  452 | `	ph7_class_attr *pAttr;` |
|       - |  453 | `	SyHashEntry *pEntry;` |
|       - |  454 | `	SyString *pName;` |
|       - |  455 | `	sxi32 rc;` |
|       - |  456 | `	/* Install in the derived hashtable */` |
|  182427 |  457 | `	rc = SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|  182427 |  458 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  459 | `		return rc;` |
|       - |  460 | `	}` |
|       - |  461 | `	/* readonly class inheritance (PHP 8.2): a readonly class may only extend a` |
|       - |  462 | `	 * readonly class, and a non-readonly class may not extend a readonly one. */` |
|  182427 |  463 | `	if( (pBase->iFlags & PH7_CLASS_READONLY) != (pSub->iFlags & PH7_CLASS_READONLY) ){` |
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
|  182427 |  478 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
| 1066641 |  479 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|       - |  480 | `		/* Make sure the private attributes are not redeclared in the subclass */` |
|  884219 |  481 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  884219 |  482 | `		pName = &pAttr->sName;` |
|  884219 |  483 | `		if( (pEntry = SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte)) != 0 ){` |
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
|  884198 |  513 | `		if( pAttr->iProtection != PH7_CLASS_PROT_PRIVATE` |
|  469245 |  514 | `		 \|\| (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|  884199 |  515 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|  884199 |  516 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  517 | `				return rc;` |
|       - |  518 | `			}` |
|  442097 |  519 | `		}` |
|       5 |  520 | `	}` |
|  182427 |  521 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
| 2574819 |  522 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|       - |  523 | `		/* Make sure the private/final methods are not redeclared in the subclass */` |
| 2392397 |  524 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
| 2392397 |  525 | `		pName = &pMeth->sFunc.sName;` |
| 2392397 |  526 | `		if( (pEntry = SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|   89233 |  527 | `			 if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|       - |  528 | `				/* Cannot Overwrite final method */` |
|       7 |  529 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_method *)pEntry->pUserData)->nLine,` |
|       - |  530 | `					"Cannot Overwrite final method '%z:%z' inside child class '%z'",` |
|       2 |  531 | `					&pBase->sName,pName,&pSub->sName);` |
|       5 |  532 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  533 | `					return SXERR_ABORT;` |
|       - |  534 | `				}` |
|       3 |  535 | `			}else{` |
|       - |  536 | `				/* Check the override's signature is compatible with the parent's. */` |
|  133841 |  537 | `				rc = OoCheckOverrideCompat(&(*pGen),pBase,pSub,pMeth,` |
|   89224 |  538 | `					(ph7_class_method *)pEntry->pUserData);` |
|   89229 |  539 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  540 | `					return SXERR_ABORT;` |
|       - |  541 | `				}` |
|       - |  542 | `			}` |
|   89233 |  543 | `			continue;` |
|       - |  544 | `		}` |
|       - |  545 | `		/* Install the method. php: a base class's private INSTANCE method is` |
|       - |  546 | `		 * dispatchable on child instances too — an inherited public method` |
|       - |  547 | `		 * calling $this->priv() must find it (the call-site visibility check` |
|       - |  548 | `		 * binds by DECLARING class, sFunc.pUserData, so child code and` |
|       - |  549 | `		 * outsiders still can't call it; a private ctor copied down also` |
|       - |  550 | ``		 * blocks `new Child` from outside like php). Private STATICS stay`` |
|       - |  551 | `		 * uncopied — base methods reach those through self:: against the` |
|       - |  552 | `		 * declaring class directly. */` |
| 2303164 |  553 | `		if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE` |
| 1163222 |  554 | `		 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
| 2303169 |  555 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 2303169 |  556 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  557 | `				return rc;` |
|       - |  558 | `			}` |
| 1151582 |  559 | `		}` |
|       5 |  560 | `	}` |
|       - |  561 | `	/* Mark as subclass */` |
|  182427 |  562 | `	pSub->pBase = pBase;` |
|       - |  563 | `	/* All done */` |
|  182427 |  564 | `	return SXRET_OK;` |
|   91216 |  565 | `}` |
|       - |  566 | `/*` |
|       - |  567 | ` * Apply a trait to a class: copy all methods and attributes from the trait` |
|       - |  568 | ` * into the target class. Unlike inheritance, traits copy ALL members including` |
|       - |  569 | ` * private ones. Members already defined in the class take precedence.` |
|       - |  570 | ` */` |
|   15566 |  571 | `PH7_PRIVATE sxi32 PH7_ClassUseTrait(ph7_gen_state *pGen,ph7_class *pClass,ph7_class *pTrait)` |
|       5 |  572 | `{` |
|       - |  573 | `	ph7_class_method *pMeth;` |
|       - |  574 | `	ph7_class_attr *pAttr;` |
|       - |  575 | `	SyHashEntry *pEntry;` |
|       - |  576 | `	SyString *pName;` |
|       - |  577 | `	sxi32 rc;` |
|       - |  578 | `	/* Detect cyclic trait composition (e.g. trait A { use B; } trait B { use A; }) */` |
|   15571 |  579 | `	if( pTrait->iFlags & PH7_CLASS_TRAIT_VISITING ){` |
|     ! 0 |  580 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|     ! 0 |  581 | `			"Trait circular reference detected: %z is already being applied",&pTrait->sName);` |
|     ! 0 |  582 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  583 | `			return SXERR_ABORT;` |
|       - |  584 | `		}` |
|     ! 0 |  585 | `		return SXRET_OK;` |
|       - |  586 | `	}` |
|   15571 |  587 | `	pTrait->iFlags \|= PH7_CLASS_TRAIT_VISITING;` |
|   15571 |  588 | `	rc = SXRET_OK;` |
|       - |  589 | `	/* Copy attributes from the trait */` |
|   15571 |  590 | `	SyHashResetLoopCursor(&pTrait->hAttr);` |
|   54349 |  591 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hAttr)) != 0 ){` |
|       - |  592 | `		SyHashEntry *pExisting;` |
|   38783 |  593 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   38783 |  594 | `		pName = &pAttr->sName;` |
|   38783 |  595 | `		pExisting = SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte);` |
|   38783 |  596 | `		if( pExisting != 0 ){` |
|       - |  597 | `			/* Attribute already exists. Check if it came from another trait` |
|       - |  598 | `			 * and whether the definitions are compatible (same defaults).` |
|       - |  599 | `			 */` |
|       - |  600 | `			ph7_class **apUsedTraits;` |
|       - |  601 | `			sxu32 nUsed,k;` |
|       6 |  602 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       6 |  603 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|       6 |  604 | `			for(k = 0; k < nUsed; k++){` |
|       - |  605 | `				ph7_class_attr *pOther;` |
|       3 |  606 | `				pOther = PH7_ClassExtractAttribute(apUsedTraits[k],pName->zString,pName->nByte);` |
|       3 |  607 | `				if( pOther ){` |
|       - |  608 | `					/* Two traits define the same property — check if defaults differ */` |
|       3 |  609 | `					ph7_class_attr *pClassAttr = (ph7_class_attr *)pExisting->pUserData;` |
|       4 |  610 | `					if( SySetUsed(&pAttr->aByteCode) != SySetUsed(&pClassAttr->aByteCode) \|\|` |
|       3 |  611 | `						(SySetUsed(&pAttr->aByteCode) > 0 &&` |
|       3 |  612 | `						 SyMemcmp(SySetBasePtr(&pAttr->aByteCode),SySetBasePtr(&pClassAttr->aByteCode),` |
|       3 |  613 | `							SySetUsed(&pAttr->aByteCode) * SySetElemSize(&pAttr->aByteCode)) != 0) ){` |
|       4 |  614 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine,` |
|       - |  615 | `							"%z and %z define the same property ($%z) in the composition of %z. "` |
|       - |  616 | `							"However, the definition differs and is considered incompatible",` |
|       2 |  617 | `							&apUsedTraits[k]->sName,&pTrait->sName,pName,&pClass->sName);` |
|       3 |  618 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  619 | `							goto cleanup;` |
|       - |  620 | `						}` |
|       1 |  621 | `					}` |
|       3 |  622 | `					break;` |
|       - |  623 | `				}` |
|     ! 0 |  624 | `			}` |
|       6 |  625 | `			continue;` |
|       - |  626 | `		}` |
|   38779 |  627 | `		rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|   38779 |  628 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  629 | `			goto cleanup;` |
|       - |  630 | `		}` |
|       5 |  631 | `	}` |
|       - |  632 | `	/* Copy methods from the trait */` |
|   15571 |  633 | `	SyHashResetLoopCursor(&pTrait->hMethod);` |
|  217185 |  634 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hMethod)) != 0 ){` |
|  201619 |  635 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  201619 |  636 | `		pName = &pMeth->sFunc.sName;` |
|  201619 |  637 | `		if( SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte) != 0 ){` |
|       - |  638 | `			/* Method already exists in the class. Check if it came from another trait` |
|       - |  639 | `			 * (unresolved conflict) vs being defined by the class itself.` |
|       - |  640 | `			 */` |
|       - |  641 | `			ph7_class **apUsedTraits;` |
|       - |  642 | `			sxu32 nUsed,k;` |
|      11 |  643 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|      11 |  644 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|      11 |  645 | `			for(k = 0; k < nUsed; k++){` |
|       3 |  646 | `				if( PH7_ClassExtractMethod(apUsedTraits[k],pName->zString,pName->nByte) != 0 ){` |
|       - |  647 | `					/* Two different traits define the same method with no resolution */` |
|       4 |  648 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|       - |  649 | `						"Trait method %z::%z has not been applied as %z::%z, "` |
|       - |  650 | `						"because of collision with %z::%z",` |
|       2 |  651 | `						&pTrait->sName,pName,` |
|       1 |  652 | `						&pClass->sName,pName,` |
|       2 |  653 | `						&apUsedTraits[k]->sName,pName);` |
|       3 |  654 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  655 | `						goto cleanup;` |
|       - |  656 | `					}` |
|       3 |  657 | `					break;` |
|       - |  658 | `				}` |
|     ! 0 |  659 | `			}` |
|       - |  660 | `			/* Class-defined method takes precedence */` |
|      11 |  661 | `			continue;` |
|       - |  662 | `		}` |
|  201611 |  663 | `		rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|  201611 |  664 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  665 | `			goto cleanup;` |
|       - |  666 | `		}` |
|       5 |  667 | `	}` |
|       - |  668 | `	/* Record trait in the class */` |
|   15571 |  669 | `	SySetPut(&pClass->aTrait,(const void *)&pTrait);` |
|    7783 |  670 | `cleanup:` |
|       - |  671 | `	/* Always clear visiting flag, even on error paths */` |
|   15571 |  672 | `	pTrait->iFlags &= ~PH7_CLASS_TRAIT_VISITING;` |
|    7783 |  673 | `	SXUNUSED(pGen);` |
|   15571 |  674 | `	return rc;` |
|    7788 |  675 | `}` |
|       - |  676 | `/*` |
|       - |  677 | ` * Inherit an object interface from another object interface.` |
|       - |  678 | ` * According to the PHP language reference manual.` |
|       - |  679 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|       - |  680 | ` *  must implement, without having to define how these methods are handled.` |
|       - |  681 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  682 | ` *  class, but without any of the methods having their contents defined.` |
|       - |  683 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  684 | ` *` |
|       - |  685 | ` * This function return SXRET_OK if the interface inheritance operation was successfully performed.` |
|       - |  686 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|       - |  687 | ` * error message.` |
|       - |  688 | ` */` |
|   23266 |  689 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceInherit(ph7_class *pSub,ph7_class *pBase)` |
|       5 |  690 | `{` |
|       - |  691 | `	ph7_class_method *pMeth;` |
|       - |  692 | `	ph7_class_attr *pAttr;` |
|       - |  693 | `	SyHashEntry *pEntry;` |
|       - |  694 | `	SyString *pName;` |
|       - |  695 | `	sxi32 rc;` |
|       - |  696 | `	/* Install in the derived hashtable */` |
|   23271 |  697 | `	SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|   23271 |  698 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
|       - |  699 | `	/* Copy constants */` |
|   34906 |  700 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|       - |  701 | `		/* Make sure the constants are not redeclared in the subclass */` |
|       3 |  702 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|       3 |  703 | `		pName = &pAttr->sName;` |
|       3 |  704 | `		if( SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte) == 0 ){` |
|       - |  705 | `			/* Install the constant in the subclass */` |
|       3 |  706 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|       3 |  707 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  708 | `				return rc;` |
|       - |  709 | `			}` |
|       1 |  710 | `		}` |
|       1 |  711 | `	}` |
|   23271 |  712 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
|       - |  713 | `	/* Copy methods signature */` |
|   81458 |  714 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|       - |  715 | `		/* Make sure the method are not redeclared in the subclass */` |
|   46559 |  716 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   46559 |  717 | `		pName = &pMeth->sFunc.sName;` |
|   46559 |  718 | `		if( SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte) == 0 ){` |
|       - |  719 | `			/* Install the method */` |
|   46559 |  720 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|   46559 |  721 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  722 | `				return rc;` |
|       - |  723 | `			}` |
|   23277 |  724 | `		}` |
|       5 |  725 | `	}` |
|       - |  726 | `	/* Mark as subclass */` |
|   23271 |  727 | `	pSub->pBase = pBase;` |
|       - |  728 | `	/* All done */` |
|   23271 |  729 | `	return SXRET_OK;` |
|   11638 |  730 | `}` |
|       - |  731 | `/*` |
|       - |  732 | ` * Implements an object interface in the given main class.` |
|       - |  733 | ` * According to the PHP language reference manual.` |
|       - |  734 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|       - |  735 | ` *  must implement, without having to define how these methods are handled.` |
|       - |  736 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  737 | ` *  class, but without any of the methods having their contents defined.` |
|       - |  738 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  739 | ` *` |
|       - |  740 | ` * This function return SXRET_OK if the interface was successfully implemented.` |
|       - |  741 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|       - |  742 | ` * error message.` |
|       - |  743 | ` */` |
|  341420 |  744 | `PH7_PRIVATE sxi32 PH7_ClassImplement(ph7_class *pMain,ph7_class *pInterface)` |
|       5 |  745 | `{` |
|       - |  746 | `	ph7_class_attr *pAttr;` |
|       - |  747 | `	SyHashEntry *pEntry;` |
|       - |  748 | `	SyString *pName;` |
|       - |  749 | `	sxi32 rc;` |
|       - |  750 | `	/* First off,copy all constants declared inside the interface */` |
|  341425 |  751 | `	SyHashResetLoopCursor(&pInterface->hAttr);` |
|  620677 |  752 | `	while((pEntry = SyHashGetNextEntry(&pInterface->hAttr)) != 0 ){` |
|       - |  753 | `		/* Point to the constant declaration */` |
|  108547 |  754 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  108547 |  755 | `		pName = &pAttr->sName;` |
|       - |  756 | `		/* Make sure the attribute is not redeclared in the main class */` |
|  108547 |  757 | `		if( SyHashGet(&pMain->hAttr,pName->zString,pName->nByte) == 0 ){` |
|       - |  758 | `			/* Install the attribute */` |
|  108543 |  759 | `			rc = SyHashInsert(&pMain->hAttr,pName->zString,pName->nByte,pAttr);` |
|  108543 |  760 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  761 | `				return rc;` |
|       - |  762 | `			}` |
|   54269 |  763 | `		}` |
|       5 |  764 | `	}` |
|       - |  765 | `	/* Install in the interface container */` |
|  341425 |  766 | `	SySetPut(&pMain->aInterface,(const void *)&pInterface);` |
|       - |  767 | `	/* Install interface method stubs into the implementing class.` |
|       - |  768 | `	 * Methods already defined in the class take precedence (they satisfy` |
|       - |  769 | `	 * the interface contract). Stubs retain PH7_CLASS_ATTR_ABSTRACT so` |
|       - |  770 | `	 * the unified check in GenStateCheckAbstractMethods catches missing ones.` |
|       - |  771 | `	 */` |
|       - |  772 | `	{` |
|       - |  773 | `		ph7_class_method *pMeth;` |
|       - |  774 | `		SyHashEntry *pMEntry;` |
|       - |  775 | `		SyString *pMName;` |
|  341425 |  776 | `		SyHashResetLoopCursor(&pInterface->hMethod);` |
| 1117371 |  777 | `		while((pMEntry = SyHashGetNextEntry(&pInterface->hMethod)) != 0 ){` |
|  605241 |  778 | `			pMeth = (ph7_class_method *)pMEntry->pUserData;` |
|  605241 |  779 | `			pMName = &pMeth->sFunc.sName;` |
|  605241 |  780 | `			if( SyHashGet(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte) == 0 ){` |
|      23 |  781 | `				rc = SyHashInsert(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte,pMeth);` |
|      23 |  782 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  783 | `					return rc;` |
|       - |  784 | `				}` |
|       9 |  785 | `			}` |
|       5 |  786 | `		}` |
|       - |  787 | `	}` |
|  341425 |  788 | `	return SXRET_OK;` |
|  170715 |  789 | `}` |
|       - |  790 | `/*` |
|       - |  791 | ` * Create a class instance [i.e: Object in the PHP jargon] at run-time.` |
|       - |  792 | ` * The following function is called when an object is created at run-time` |
|       - |  793 | ` * typically when the PH7_OP_NEW/PH7_OP_CLONE instructions are executed.` |
|       - |  794 | ` * Notes on object creation.` |
|       - |  795 | ` *` |
|       - |  796 | ` * According to PHP language reference manual.` |
|       - |  797 | ` * To create an instance of a class, the new keyword must be used. An object will always` |
|       - |  798 | ` * be created unless the object has a constructor defined that throws an exception on error.` |
|       - |  799 | ` * Classes should be defined before instantiation (and in some cases this is a requirement).` |
|       - |  800 | ` * If a string containing the name of a class is used with new, a new instance of that class` |
|       - |  801 | ` * will be created. If the class is in a namespace, its fully qualified name must be used when` |
|       - |  802 | ` * doing this.` |
|       - |  803 | ` * Example #3 Creating an instance` |
|       - |  804 | ` * <?php` |
|       - |  805 | ` *  $instance = new SimpleClass();` |
|       - |  806 | ` *   // This can also be done with a variable:` |
|       - |  807 | ` * $className = 'Foo';` |
|       - |  808 | ` * $instance = new $className(); // Foo()` |
|       - |  809 | ` * ?>` |
|       - |  810 | ` * In the class context, it is possible to create a new object by new self and new parent.` |
|       - |  811 | ` * When assigning an already created instance of a class to a new variable, the new variable` |
|       - |  812 | ` * will access the same instance as the object that was assigned. This behaviour is the same` |
|       - |  813 | ` * when passing instances to a function. A copy of an already created object can be made by` |
|       - |  814 | ` * cloning it.` |
|       - |  815 | ` * Example #4 Object Assignment` |
|       - |  816 | ` * <?php` |
|       - |  817 | ` *  class SimpleClass(){` |
|       - |  818 | ` *    public $var;` |
|       - |  819 | ` *  };` |
|       - |  820 | ` *  $instance = new SimpleClass();` |
|       - |  821 | ` *  $assigned   =  $instance;` |
|       - |  822 | ` *  $reference  =& $instance;` |
|       - |  823 | ` *  $instance->var = '$assigned will have this value';` |
|       - |  824 | ` *  $instance = null; // $instance and $reference become null` |
|       - |  825 | ` *  var_dump($instance);` |
|       - |  826 | ` *  var_dump($reference);` |
|       - |  827 | ` *  var_dump($assigned);` |
|       - |  828 | ` * ?>` |
|       - |  829 | ` * The above example will output:` |
|       - |  830 | ` * NULL` |
|       - |  831 | ` * NULL` |
|       - |  832 | ` * object(SimpleClass)#1 (1) {` |
|       - |  833 | ` *  ["var"]=>` |
|       - |  834 | ` *    string(30) "$assigned will have this value"` |
|       - |  835 | ` * }` |
|       - |  836 | ` * Example #5 Creating new objects` |
|       - |  837 | ` * <?php` |
|       - |  838 | ` * class Test` |
|       - |  839 | ` * {` |
|       - |  840 | ` *   static public function getNew()` |
|       - |  841 | ` *   {` |
|       - |  842 | ` *       return new static;` |
|       - |  843 | ` *   }` |
|       - |  844 | ` * }` |
|       - |  845 | ` * class Child extends Test` |
|       - |  846 | ` * {}` |
|       - |  847 | ` * $obj1 = new Test();` |
|       - |  848 | ` * $obj2 = new $obj1;` |
|       - |  849 | ` * var_dump($obj1 !== $obj2);` |
|       - |  850 | ` * $obj3 = Test::getNew();` |
|       - |  851 | ` * var_dump($obj3 instanceof Test);` |
|       - |  852 | ` * $obj4 = Child::getNew();` |
|       - |  853 | ` * var_dump($obj4 instanceof Child);` |
|       - |  854 | ` * ?>` |
|       - |  855 | ` * The above example will output:` |
|       - |  856 | ` * bool(true)` |
|       - |  857 | ` * bool(true)` |
|       - |  858 | ` * bool(true)` |
|       - |  859 | ` * Note that Symisc Systems have introduced powerfull extension to` |
|       - |  860 | ` * OO subsystem. For example a class attribute may have any complex` |
|       - |  861 | ` * expression associated with it when declaring the attribute unlike` |
|       - |  862 | ` * the standard PHP engine which would allow a single value.` |
|       - |  863 | ` * Example:` |
|       - |  864 | ` *  class myClass{` |
|       - |  865 | ` *    public $var = 25<<1+foo()/bar();` |
|       - |  866 | ` *  };` |
|       - |  867 | ` * Refer to the official documentation for more information.` |
|       - |  868 | ` */` |
|    7254 |  869 | `static ph7_class_instance * NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|       5 |  870 | `{` |
|       - |  871 | `	ph7_class_instance *pThis;` |
|       - |  872 | `	/* Allocate a new instance */` |
|    7259 |  873 | `	pThis = (ph7_class_instance *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_instance));` |
|    7259 |  874 | `	if( pThis == 0 ){` |
|     ! 0 |  875 | `		return 0;` |
|       - |  876 | `	}` |
|       - |  877 | `	/* Zero the structure */` |
|    7259 |  878 | `	SyZero(pThis,sizeof(ph7_class_instance));` |
|       - |  879 | `	/* Initialize fields */` |
|    7259 |  880 | `	pThis->iRef = 1;` |
|    7259 |  881 | `	pThis->pVm = pVm;` |
|    7259 |  882 | `	pThis->pClass = pClass;` |
|       - |  883 | `	/* Assign a fresh monotonic object handle id (clones get their own, like PHP). */` |
|    7259 |  884 | `	pThis->nObjId = pVm->nNextObjId++;` |
|    7259 |  885 | `	SyHashInit(&pThis->hAttr,&pVm->sAllocator,0,0);` |
|    7259 |  886 | `	return pThis;` |
|    3632 |  887 | `}` |
|       - |  888 | `/*` |
|       - |  889 | ` * Wrapper around the NewClassInstance() function defined above.` |
|       - |  890 | ` * See the block comment above for more information.` |
|       - |  891 | ` */` |
|    7022 |  892 | `PH7_PRIVATE ph7_class_instance * PH7_NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|       5 |  893 | `{` |
|       - |  894 | `	ph7_class_instance *pNew;` |
|       - |  895 | `	sxi32 rc;` |
|    7027 |  896 | `	pNew = NewClassInstance(&(*pVm),&(*pClass));` |
|    7027 |  897 | `	if( pNew == 0 ){` |
|     ! 0 |  898 | `		return 0;` |
|       - |  899 | `	}` |
|       - |  900 | `	/* Associate a private VM frame with this class instance */` |
|    7027 |  901 | `	rc = PH7_VmCreateClassInstanceFrame(&(*pVm),pNew);` |
|    7027 |  902 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  903 | `		SyMemBackendPoolFree(&pVm->sAllocator,pNew);` |
|     ! 0 |  904 | `		return 0;` |
|       - |  905 | `	}` |
|    7027 |  906 | `	return pNew;` |
|    3516 |  907 | `}` |
|       - |  908 | `/*` |
|       - |  909 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon] attribute.` |
|       - |  910 | ` * This function never fail.` |
|       - |  911 | ` */` |
|   11674 |  912 | `static ph7_value * ExtractClassAttrValue(ph7_vm *pVm,VmClassAttr *pAttr)` |
|       5 |  913 | `{` |
|       - |  914 | `	/* Extract the value */` |
|       - |  915 | `	ph7_value *pValue;` |
|   11679 |  916 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|   11679 |  917 | `	return pValue;` |
|       5 |  918 | `}` |
|       - |  919 | `/*` |
|       - |  920 | ` * Perform a clone operation on a class instance [i.e: Object in the PHP jargon].` |
|       - |  921 | ` * The following function is called when an object is cloned at run-time` |
|       - |  922 | ` * typically when the PH7_OP_CLONE instruction is executed.` |
|       - |  923 | ` * Notes on object cloning.` |
|       - |  924 | ` *` |
|       - |  925 | ` * According to PHP language reference manual.` |
|       - |  926 | ` * Creating a copy of an object with fully replicated properties is not always the wanted behavior.` |
|       - |  927 | ` * A good example of the need for copy constructors. Another example is if your object holds a reference` |
|       - |  928 | ` * to another object which it uses and when you replicate the parent object you want to create` |
|       - |  929 | ` * a new instance of this other object so that the replica has its own separate copy.` |
|       - |  930 | ` * An object copy is created by using the clone keyword (which calls the object's __clone() method if possible).` |
|       - |  931 | ` * An object's __clone() method cannot be called directly.` |
|       - |  932 | ` * $copy_of_object = clone $object;` |
|       - |  933 | ` * When an object is cloned, PHP 5 will perform a shallow copy of all of the object's properties.` |
|       - |  934 | ` * Any properties that are references to other variables, will remain references.` |
|       - |  935 | ` * Once the cloning is complete, if a __clone() method is defined, then the newly created object's __clone() method` |
|       - |  936 | ` * will be called, to allow any necessary properties that need to be changed.` |
|       - |  937 | ` * Example #1 Cloning an object` |
|       - |  938 | ` * <?php` |
|       - |  939 | ` * class SubObject` |
|       - |  940 | ` * {` |
|       - |  941 | ` *   static $instances = 0;` |
|       - |  942 | ` *   public $instance;` |
|       - |  943 | ` *` |
|       - |  944 | ` *   public function __construct() {` |
|       - |  945 | ` *       $this->instance = ++self::$instances;` |
|       - |  946 | ` *   }` |
|       - |  947 | ` *` |
|       - |  948 | ` *   public function __clone() {` |
|       - |  949 | ` *       $this->instance = ++self::$instances;` |
|       - |  950 | ` *   }` |
|       - |  951 | ` * }` |
|       - |  952 | ` *` |
|       - |  953 | ` * class MyCloneable` |
|       - |  954 | ` * {` |
|       - |  955 | ` *   public $object1;` |
|       - |  956 | ` *   public $object2;` |
|       - |  957 | ` *` |
|       - |  958 | ` *   function __clone()` |
|       - |  959 | ` *   {` |
|       - |  960 | ` *       // Force a copy of this->object, otherwise` |
|       - |  961 | ` *       // it will point to same object.` |
|       - |  962 | ` *       $this->object1 = clone $this->object1;` |
|       - |  963 | ` *   }` |
|       - |  964 | ` * }` |
|       - |  965 | ` * $obj = new MyCloneable();` |
|       - |  966 | ` * $obj->object1 = new SubObject();` |
|       - |  967 | ` * $obj->object2 = new SubObject();` |
|       - |  968 | ` * $obj2 = clone $obj;` |
|       - |  969 | ` * print("Original Object:\n");` |
|       - |  970 | ` * print_r($obj);` |
|       - |  971 | ` * print("Cloned Object:\n");` |
|       - |  972 | ` * print_r($obj2);` |
|       - |  973 | ` * ?>` |
|       - |  974 | ` * The above example will output:` |
|       - |  975 | ` * Original Object:` |
|       - |  976 | ` * MyCloneable Object` |
|       - |  977 | ` * (` |
|       - |  978 | ` *   [object1] => SubObject Object` |
|       - |  979 | ` *       (` |
|       - |  980 | ` *           [instance] => 1` |
|       - |  981 | ` *       )` |
|       - |  982 | ` *` |
|       - |  983 | ` *   [object2] => SubObject Object` |
|       - |  984 | ` *       (` |
|       - |  985 | ` *           [instance] => 2` |
|       - |  986 | ` *       )` |
|       - |  987 | ` *` |
|       - |  988 | ` * )` |
|       - |  989 | ` * Cloned Object:` |
|       - |  990 | ` * MyCloneable Object` |
|       - |  991 | ` * (` |
|       - |  992 | ` *   [object1] => SubObject Object` |
|       - |  993 | ` *       (` |
|       - |  994 | ` *           [instance] => 3` |
|       - |  995 | ` *       )` |
|       - |  996 | ` *` |
|       - |  997 | ` *   [object2] => SubObject Object` |
|       - |  998 | ` *       (` |
|       - |  999 | ` *           [instance] => 2` |
|       - | 1000 | ` *       )` |
|       - | 1001 | ` * )` |
|       - | 1002 | ` */` |
|     232 | 1003 | `PH7_PRIVATE ph7_class_instance * PH7_CloneClassInstance(ph7_class_instance *pSrc)` |
|       3 | 1004 | `{` |
|       - | 1005 | `	ph7_class_instance *pClone;` |
|       - | 1006 | `	ph7_class_method *pMethod;` |
|       - | 1007 | `	SyHashEntry *pEntry2;` |
|       - | 1008 | `	SyHashEntry *pEntry;` |
|       - | 1009 | `	ph7_vm *pVm;` |
|       - | 1010 | `	sxi32 rc;` |
|       - | 1011 | `	/* Allocate a new instance */` |
|     235 | 1012 | `	pVm = pSrc->pVm;` |
|     235 | 1013 | `	pClone = NewClassInstance(pVm,pSrc->pClass);` |
|     235 | 1014 | `	if( pClone == 0 ){` |
|     ! 0 | 1015 | `		return 0;` |
|       - | 1016 | `	}` |
|       - | 1017 | `	/* Associate a private VM frame with this class instance */` |
|     235 | 1018 | `	rc = PH7_VmCreateClassInstanceFrame(pVm,pClone);` |
|     235 | 1019 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1020 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClone);` |
|     ! 0 | 1021 | `		return 0;` |
|       - | 1022 | `	}` |
|       - | 1023 | `	/* Duplicate object values. Iterate the SOURCE attributes and copy each into` |
|       - | 1024 | `	 * the clone's same-named slot (looked up by name, so order/count differences` |
|       - | 1025 | `	 * from dynamic properties don't matter). A dynamic (runtime-added) property` |
|       - | 1026 | `	 * has no declared counterpart in the clone, so synthesize it first — without` |
|       - | 1027 | `	 * this, a clone of a stdClass would silently lose all its dynamic properties. */` |
|     235 | 1028 | `	SyHashResetLoopCursor(&pSrc->hAttr);` |
|    2425 | 1029 | `	while((pEntry = SyHashGetNextEntry(&pSrc->hAttr)) != 0 ){` |
|    2193 | 1030 | `		VmClassAttr *pSrcAttr = (VmClassAttr *)pEntry->pUserData;` |
|    2193 | 1031 | `		VmClassAttr *pDestAttr = 0;` |
|    2193 | 1032 | `		ph7_value *pvSrc,*pvDest = 0;` |
|       - | 1033 | `		/* Duplicate non-static attribute */` |
|    2193 | 1034 | `		if( pSrcAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|    1637 | 1035 | `			continue;` |
|       - | 1036 | `		}` |
|     557 | 1037 | `		pEntry2 = SyHashGet(&pClone->hAttr,SyStringData(&pSrcAttr->pAttr->sName),SyStringLength(&pSrcAttr->pAttr->sName));` |
|     557 | 1038 | `		if( pEntry2 ){` |
|     543 | 1039 | `			pDestAttr = (VmClassAttr *)pEntry2->pUserData;` |
|     543 | 1040 | `			pvDest = ExtractClassAttrValue(pVm,pDestAttr);` |
|     285 | 1041 | `		}else if( pSrcAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){` |
|       - | 1042 | `			/* Dynamic property: synthesize the matching slot on the clone. */` |
|      22 | 1043 | `			pvDest = PH7_VmCreateDynamicAttr(pVm,pClone,` |
|      14 | 1044 | `				SyStringData(&pSrcAttr->pAttr->sName),SyStringLength(&pSrcAttr->pAttr->sName),&pDestAttr);` |
|       7 | 1045 | `		}` |
|       - | 1046 | `		/* Fetch the source value LAST: PH7_VmCreateDynamicAttr above may have` |
|       - | 1047 | `		 * reserved a slot and reallocated pVm->aMemObj, which would dangle any` |
|       - | 1048 | `		 * ph7_value* obtained before it. pvDest from the synth path already points` |
|       - | 1049 | `		 * into the post-realloc aMemObj; resolve pvSrc now so both are current. */` |
|     557 | 1050 | `		pvSrc = ExtractClassAttrValue(pVm,pSrcAttr);` |
|     557 | 1051 | `		if( pvSrc && pvDest ){` |
|     557 | 1052 | `			PH7_MemObjStore(pvSrc,pvDest);` |
|     277 | 1053 | `		}` |
|       - | 1054 | `		/* Carry over the per-instance state so the clone matches the source:` |
|       - | 1055 | `		 * VM_CLASS_ATTR_UNINIT marks a typed property as not-yet-initialized` |
|       - | 1056 | `		 * and doubles as the readonly write-once latch — without this a clone` |
|       - | 1057 | `		 * would reset to uninitialized (losing the value's readiness) and a` |
|       - | 1058 | `		 * readonly property would become writable again. */` |
|     557 | 1059 | `		if( pDestAttr ){` |
|     557 | 1060 | `			pDestAttr->iState = pSrcAttr->iState;` |
|     277 | 1061 | `		}` |
|       3 | 1062 | `	}` |
|       - | 1063 | `	/* A declared property unset() on the source is absent from the clone too (PHP). But the clone` |
|       - | 1064 | `	 * frame above materialized ALL declared attrs (with their defaults), so drop any clone attr whose` |
|       - | 1065 | `	 * name is not present on the source. Collect first, then delete — removing an entry mid-walk would` |
|       - | 1066 | `	 * free the node the SyHash loop cursor points at. */` |
|       - | 1067 | `	{` |
|       - | 1068 | `		SySet sDrop;` |
|     235 | 1069 | `		SySetInit(&sDrop,&pVm->sAllocator,sizeof(VmClassAttr *));` |
|     235 | 1070 | `		SyHashResetLoopCursor(&pClone->hAttr);` |
|    2427 | 1071 | `		while((pEntry = SyHashGetNextEntry(&pClone->hAttr)) != 0 ){` |
|    2195 | 1072 | `			VmClassAttr *pCloneAttr = (VmClassAttr *)pEntry->pUserData;` |
|    2195 | 1073 | `			if( pCloneAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|    1637 | 1074 | `				continue;` |
|       - | 1075 | `			}` |
|     834 | 1076 | `			if( SyHashGet(&pSrc->hAttr,SyStringData(&pCloneAttr->pAttr->sName),` |
|     837 | 1077 | `					SyStringLength(&pCloneAttr->pAttr->sName)) == 0 ){` |
|       3 | 1078 | `				SySetPut(&sDrop,(const void *)&pCloneAttr);` |
|       1 | 1079 | `			}` |
|       3 | 1080 | `		}` |
|     235 | 1081 | `		if( SySetUsed(&sDrop) > 0 ){` |
|       3 | 1082 | `			VmClassAttr **apDrop = (VmClassAttr **)SySetBasePtr(&sDrop);` |
|       - | 1083 | `			sxu32 i;` |
|       5 | 1084 | `			for( i = 0 ; i < SySetUsed(&sDrop) ; ++i ){` |
|       3 | 1085 | `				VmClassAttr *pVmAttr = apDrop[i];` |
|       4 | 1086 | `				SyHashDeleteEntry(&pClone->hAttr,SyStringData(&pVmAttr->pAttr->sName),` |
|       2 | 1087 | `					SyStringLength(&pVmAttr->pAttr->sName),0);` |
|       3 | 1088 | `				PH7_VmReleaseInstanceAttr(pVm,pVmAttr);` |
|       2 | 1089 | `			}` |
|       1 | 1090 | `		}` |
|     235 | 1091 | `		SySetRelease(&sDrop);` |
|       - | 1092 | `	}` |
|       - | 1093 | `	/* call the __clone method on the cloned object if available */` |
|     235 | 1094 | `	pMethod = PH7_ClassExtractMethod(pClone->pClass,"__clone",sizeof("__clone")-1);` |
|     235 | 1095 | `	if( pMethod ){` |
|      56 | 1096 | `		if( pMethod->iCloneDepth < 16 ){` |
|      54 | 1097 | `			pMethod->iCloneDepth++;` |
|      54 | 1098 | `			PH7_VmCallClassMethod(pVm,pClone,pMethod,0,0,0);` |
|      28 | 1099 | `		}else{` |
|       - | 1100 | `			/* Nesting limit reached */` |
|       3 | 1101 | `			PH7_VmThrowError(pVm,0,PH7_CTX_ERR,"Object clone limit reached,no more call to __clone()");` |
|       - | 1102 | `		}` |
|       - | 1103 | `		/* Reset the cursor */` |
|      56 | 1104 | `		pMethod->iCloneDepth = 0;` |
|      27 | 1105 | `	}` |
|       - | 1106 | `	/* Return the cloned object */` |
|     235 | 1107 | `	return pClone;` |
|     119 | 1108 | `}` |
|       - | 1109 | `#define CLASS_INSTANCE_DESTROYED 0x001 /* Instance is released */` |
|       - | 1110 | `/*` |
|       - | 1111 | ` * Free the per-instance allocations owned by ONE object attribute: its value slot (+ the typed-slot` |
|       - | 1112 | ` * enforcement entry), the synthesized ph7_class_attr for a dynamic (runtime-added) property, and the` |
|       - | 1113 | ` * VmClassAttr wrapper itself. Does NOT touch the hAttr entry node — the caller removes it` |
|       - | 1114 | `` * (`unset($o->p)` via SyHashDeleteEntry2; instance teardown via the wholesale SyHashRelease, so it must`` |
|       - | 1115 | ` * not delete entries mid-walk). Shared by PH7_ClassInstanceRelease and the OP_MEMBER unset path.` |
|       - | 1116 | ` */` |
|   24474 | 1117 | `PH7_PRIVATE void PH7_VmReleaseInstanceAttr(ph7_vm *pVm, VmClassAttr *pVmAttr)` |
|       5 | 1118 | `{` |
|   24479 | 1119 | `	if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|       - | 1120 | `		/* Drop any typed-property enforcement slot registered for this memobj, before the memobj` |
|       - | 1121 | `		 * is returned to the free list, so a future recycled slot does not inherit the stale entry. */` |
|   18447 | 1122 | `		if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|     282 | 1123 | `			SyHashDeleteEntry(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),0);` |
|     139 | 1124 | `		}` |
|   18447 | 1125 | `		PH7_VmUnsetMemObj(pVm,pVmAttr->nIdx,TRUE);` |
|    9221 | 1126 | `	}` |
|       - | 1127 | `	/* A dynamic property owns its synthesized ph7_class_attr (struct + inline name in one block) —` |
|       - | 1128 | `	 * free it here (the only place a per-instance pAttr is freed; declared attrs are class-owned). */` |
|   24479 | 1129 | `	if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){` |
|     111 | 1130 | `		SyMemBackendFree(&pVm->sAllocator,pVmAttr->pAttr);` |
|      55 | 1131 | `	}` |
|   24479 | 1132 | `	SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|   24479 | 1133 | `}` |
|       - | 1134 | `/*` |
|       - | 1135 | ` * Release a class instance [i.e: Object in the PHP jargon] and invoke any defined destructor.` |
|       - | 1136 | ` * This routine is invoked as soon as there are no other references to a particular` |
|       - | 1137 | ` * class instance.` |
|       - | 1138 | ` */` |
|    5010 | 1139 | `static void PH7_ClassInstanceRelease(ph7_class_instance *pThis)` |
|       5 | 1140 | `{` |
|       - | 1141 | `	ph7_class_method *pDestr;` |
|       - | 1142 | `	SyHashEntry *pEntry;` |
|       - | 1143 | `	ph7_class *pClass;` |
|       - | 1144 | `	ph7_vm *pVm;` |
|    5015 | 1145 | `	if( pThis->iFlags & CLASS_INSTANCE_DESTROYED ){` |
|       - | 1146 | `		/*` |
|       - | 1147 | `		 * Already destroyed,return immediately.` |
|       - | 1148 | `		 * This could happend if someone perform unset($this) in the destructor body.` |
|       - | 1149 | `		 */` |
|     ! 0 | 1150 | `		return;` |
|       - | 1151 | `	}` |
|       - | 1152 | `	/* Mark as destroyed */` |
|    5015 | 1153 | `	pThis->iFlags \|= CLASS_INSTANCE_DESTROYED;` |
|       - | 1154 | `	/* Invoke any defined destructor if available */` |
|    5015 | 1155 | `	pVm = pThis->pVm;` |
|    5015 | 1156 | `	pClass = pThis->pClass;` |
|    5015 | 1157 | `	pDestr = PH7_ClassExtractMethod(pClass,"__destruct",sizeof("__destruct")-1);` |
|    5015 | 1158 | `	if( pDestr && !pVm->bInReset ){` |
|       - | 1159 | `		/* Invoke the destructor. Skipped during ph7_vm_reset() bulk teardown:` |
|       - | 1160 | `		 * running user PHP against a half-reset VM is unsafe (see bInReset). */` |
|     453 | 1161 | `		pThis->iRef = 2; /* Prevent garbage collection */` |
|     453 | 1162 | `		PH7_VmCallClassMethod(pVm,pThis,pDestr,0,0,0);` |
|     224 | 1163 | `	}` |
|       - | 1164 | `	/* Release non-static attributes (the wholesale SyHashRelease below frees the entry nodes,` |
|       - | 1165 | `	 * so the helper must not delete them mid-walk). */` |
|    5015 | 1166 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|   29459 | 1167 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|   24449 | 1168 | `		PH7_VmReleaseInstanceAttr(pVm,(VmClassAttr *)pEntry->pUserData);` |
|       5 | 1169 | `	}` |
|       - | 1170 | `	/* Release the whole structure */` |
|    5015 | 1171 | `	SyHashRelease(&pThis->hAttr);` |
|    5015 | 1172 | `	SyMemBackendPoolFree(&pVm->sAllocator,pThis);` |
|    2510 | 1173 | `}` |
|       - | 1174 | `/*` |
|       - | 1175 | ` * Decrement the reference count of a class instance [i.e Object in the PHP jargon].` |
|       - | 1176 | ` * If the reference count reaches zero,release the whole instance.` |
|       - | 1177 | ` */` |
|  123280 | 1178 | `PH7_PRIVATE void PH7_ClassInstanceUnref(ph7_class_instance *pThis)` |
|       5 | 1179 | `{` |
|  123285 | 1180 | `	pThis->iRef--;` |
|  123285 | 1181 | `	if( pThis->iRef < 1 ){` |
|       - | 1182 | `		/* No more reference to this instance */` |
|    5015 | 1183 | `		PH7_ClassInstanceRelease(&(*pThis));` |
|    2505 | 1184 | `	}` |
|  123285 | 1185 | `}` |
|       - | 1186 | `/*` |
|       - | 1187 | ` * Compare two class instances [i.e: Objects in the PHP jargon]` |
|       - | 1188 | ` * Note on objects comparison:` |
|       - | 1189 | ` *  According to the PHP langauge reference manual` |
|       - | 1190 | ` *  When using the comparison operator (==), object variables are compared in a simple manner` |
|       - | 1191 | ` *  namely: Two object instances are equal if they have the same attributes and values, and are` |
|       - | 1192 | ` *  instances of the same class.` |
|       - | 1193 | ` *  On the other hand, when using the identity operator (===), object variables are identical` |
|       - | 1194 | ` *  if and only if they refer to the same instance of the same class.` |
|       - | 1195 | ` *  An example will clarify these rules.` |
|       - | 1196 | ` *  Example #1 Example of object comparison` |
|       - | 1197 | ` *  <?php` |
|       - | 1198 | ` *    function bool2str($bool)` |
|       - | 1199 | ` * {` |
|       - | 1200 | ` *   if ($bool === false) {` |
|       - | 1201 | ` *       return 'FALSE';` |
|       - | 1202 | ` *   } else {` |
|       - | 1203 | ` *       return 'TRUE';` |
|       - | 1204 | ` *   }` |
|       - | 1205 | ` * }` |
|       - | 1206 | ` * function compareObjects(&$o1, &$o2)` |
|       - | 1207 | ` * {` |
|       - | 1208 | ` *   echo 'o1 == o2 : ' . bool2str($o1 == $o2) . "\n";` |
|       - | 1209 | ` *   echo 'o1 != o2 : ' . bool2str($o1 != $o2) . "\n";` |
|       - | 1210 | ` *   echo 'o1 === o2 : ' . bool2str($o1 === $o2) . "\n";` |
|       - | 1211 | ` *   echo 'o1 !== o2 : ' . bool2str($o1 !== $o2) . "\n";` |
|       - | 1212 | ` * }` |
|       - | 1213 | ` * class Flag` |
|       - | 1214 | ` * {` |
|       - | 1215 | ` *   public $flag;` |
|       - | 1216 | ` *` |
|       - | 1217 | ` *   function Flag($flag = true) {` |
|       - | 1218 | ` *       $this->flag = $flag;` |
|       - | 1219 | ` *   }` |
|       - | 1220 | ` * }` |
|       - | 1221 | ` *` |
|       - | 1222 | ` * class OtherFlag` |
|       - | 1223 | ` * {` |
|       - | 1224 | ` *   public $flag;` |
|       - | 1225 | ` *` |
|       - | 1226 | ` *   function OtherFlag($flag = true) {` |
|       - | 1227 | ` *       $this->flag = $flag;` |
|       - | 1228 | ` *   }` |
|       - | 1229 | ` * }` |
|       - | 1230 | ` *` |
|       - | 1231 | ` * $o = new Flag();` |
|       - | 1232 | ` * $p = new Flag();` |
|       - | 1233 | ` * $q = $o;` |
|       - | 1234 | ` * $r = new OtherFlag();` |
|       - | 1235 | ` *` |
|       - | 1236 | ` * echo "Two instances of the same class\n";` |
|       - | 1237 | ` * compareObjects($o, $p);` |
|       - | 1238 | ` * echo "\nTwo references to the same instance\n";` |
|       - | 1239 | ` * compareObjects($o, $q);` |
|       - | 1240 | ` * echo "\nInstances of two different classes\n";` |
|       - | 1241 | ` * compareObjects($o, $r);` |
|       - | 1242 | ` * ?>` |
|       - | 1243 | ` * The above example will output:` |
|       - | 1244 | ` * Two instances of the same class` |
|       - | 1245 | ` * o1 == o2 : TRUE` |
|       - | 1246 | ` * o1 != o2 : FALSE` |
|       - | 1247 | ` * o1 === o2 : FALSE` |
|       - | 1248 | ` * o1 !== o2 : TRUE` |
|       - | 1249 | ` * Two references to the same instance` |
|       - | 1250 | ` * o1 == o2 : TRUE` |
|       - | 1251 | ` * o1 != o2 : FALSE` |
|       - | 1252 | ` * o1 === o2 : TRUE` |
|       - | 1253 | ` * o1 !== o2 : FALSE` |
|       - | 1254 | ` * Instances of two different classes` |
|       - | 1255 | ` * o1 == o2 : FALSE` |
|       - | 1256 | ` * o1 != o2 : TRUE` |
|       - | 1257 | ` * o1 === o2 : FALSE` |
|       - | 1258 | ` * o1 !== o2 : TRUE` |
|       - | 1259 | ` *` |
|       - | 1260 | ` * This function return 0 if the objects are equals according to the comprison rules defined above.` |
|       - | 1261 | ` * Any other return values indicates difference.` |
|       - | 1262 | ` */` |
|     252 | 1263 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest)` |
|       5 | 1264 | `{` |
|       - | 1265 | `	SyHashEntry *pEntry,*pEntry2;` |
|       - | 1266 | `	ph7_value sV1,sV2;` |
|       - | 1267 | `	sxi32 rc;` |
|     257 | 1268 | `	if( iNest > 31 ){` |
|       - | 1269 | `		/* Nesting limit reached */` |
|       6 | 1270 | `		PH7_VmThrowError(pLeft->pVm,0,PH7_CTX_ERR,"Nesting limit reached: Infinite recursion?");` |
|       6 | 1271 | `		return 1;` |
|       - | 1272 | `	}` |
|       - | 1273 | `	/* Comparison is performed only if the objects are instance of the same class */` |
|     253 | 1274 | `	if( pLeft->pClass != pRight->pClass ){` |
|       7 | 1275 | `		return 1;` |
|       - | 1276 | `	}` |
|     247 | 1277 | `	if( bStrict ){` |
|       - | 1278 | `		/*` |
|       - | 1279 | `		 * According to the PHP language reference manual:` |
|       - | 1280 | `		 *  when using the identity operator (===), object variables` |
|       - | 1281 | `		 *  are identical if and only if they refer to the same instance` |
|       - | 1282 | `		 *  of the same class.` |
|       - | 1283 | `		 */` |
|      95 | 1284 | `		return !(pLeft == pRight);` |
|       - | 1285 | `	}` |
|       - | 1286 | `	/*` |
|       - | 1287 | `	 * Attribute comparison.` |
|       - | 1288 | `	 * According to the PHP reference manual:` |
|       - | 1289 | `	 *  When using the comparison operator (==), object variables are compared` |
|       - | 1290 | `	 *  in a simple manner, namely: Two object instances are equal if they have` |
|       - | 1291 | `	 *  the same attributes and values, and are instances of the same class.` |
|       - | 1292 | `	 */` |
|     155 | 1293 | `	if( pLeft == pRight ){` |
|       - | 1294 | `		/* Same instance,don't bother processing,object are equals */` |
|       5 | 1295 | `		return 0;` |
|       - | 1296 | `	}` |
|       - | 1297 | `	/* Closures compare by IDENTITY under == as well (not by attributes): two distinct` |
|       - | 1298 | `	 * Closure instances are never equal, even when they wrap the same underlying function` |
|       - | 1299 | `	 * (PHP semantics). pLeft != pRight here, so a Closure pair is unequal. Without this,` |
|       - | 1300 | `` 	 * two capture-less lambdas of the same `function(){}` share the template's `$__fn` `` |
|       - | 1301 | `	 * name and would compare equal. */` |
|     151 | 1302 | `	if( pLeft->pVm->pClosureClass && pLeft->pClass == pLeft->pVm->pClosureClass ){` |
|       5 | 1303 | `		return 1;` |
|       - | 1304 | `	}` |
|       - | 1305 | `	/* Same class but a different number of attributes ⇒ different property sets` |
|       - | 1306 | `	 * (dynamic properties can give two same-class instances different counts). */` |
|     147 | 1307 | `	if( pLeft->hAttr.nEntry != pRight->hAttr.nEntry ){` |
|       3 | 1308 | `		return 1;` |
|       - | 1309 | `	}` |
|     145 | 1310 | `	PH7_MemObjInit(pLeft->pVm,&sV1);` |
|     145 | 1311 | `	PH7_MemObjInit(pLeft->pVm,&sV2);` |
|     145 | 1312 | `	sV1.nIdx = sV2.nIdx = SXU32_HIGH;` |
|       - | 1313 | `	/* Compare each left attribute against the RIGHT attribute of the SAME NAME` |
|       - | 1314 | `	 * (not in lockstep): dynamic properties may be stored in a different order` |
|       - | 1315 | `	 * on the two instances. Counts already match, so if every left attribute has` |
|       - | 1316 | `	 * an equal-valued same-named right attribute the property sets are equal. */` |
|     145 | 1317 | `	SyHashResetLoopCursor(&pLeft->hAttr);` |
|     169 | 1318 | `	while((pEntry = SyHashGetNextEntry(&pLeft->hAttr)) != 0 ){` |
|     157 | 1319 | `		VmClassAttr *p1 = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1320 | `		VmClassAttr *p2;` |
|       - | 1321 | `		ph7_value *pL,*pR;` |
|       - | 1322 | `		/* Compare only non-static attribute */` |
|     157 | 1323 | `		if( p1->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|     ! 0 | 1324 | `			continue;` |
|       - | 1325 | `		}` |
|     157 | 1326 | `		pEntry2 = SyHashGet(&pRight->hAttr,SyStringData(&p1->pAttr->sName),SyStringLength(&p1->pAttr->sName));` |
|     157 | 1327 | `		if( pEntry2 == 0 ){` |
|       - | 1328 | `			/* Left has a property the right lacks ⇒ not equal. */` |
|     ! 0 | 1329 | `			return 1;` |
|       - | 1330 | `		}` |
|     157 | 1331 | `		p2 = (VmClassAttr *)pEntry2->pUserData;` |
|     157 | 1332 | `		pL = ExtractClassAttrValue(pLeft->pVm,p1);` |
|     157 | 1333 | `		pR = ExtractClassAttrValue(pRight->pVm,p2);` |
|     157 | 1334 | `		if( pL && pR ){` |
|     157 | 1335 | `			PH7_MemObjLoad(pL,&sV1);` |
|     157 | 1336 | `			PH7_MemObjLoad(pR,&sV2);` |
|       - | 1337 | `			/* Compare the two values now */` |
|     157 | 1338 | `			rc = PH7_MemObjCmp(&sV1,&sV2,bStrict,iNest+1);` |
|     157 | 1339 | `			PH7_MemObjRelease(&sV1);` |
|     157 | 1340 | `			PH7_MemObjRelease(&sV2);` |
|     157 | 1341 | `			if( rc != 0 ){` |
|       - | 1342 | `				/* Not equals */` |
|     133 | 1343 | `				return rc;` |
|       - | 1344 | `			}` |
|      12 | 1345 | `		}` |
|       1 | 1346 | `	}` |
|       - | 1347 | `	/* Object are equals */` |
|      13 | 1348 | `	return 0;` |
|     131 | 1349 | `}` |
|       - | 1350 | `/*` |
|       - | 1351 | ` * Dump a class instance and the store the dump in the BLOB given` |
|       - | 1352 | ` * as the first argument.` |
|       - | 1353 | ` * Note that only non-static/non-constants attribute are dumped.` |
|       - | 1354 | ` * This function is typically invoked when the user issue a call` |
|       - | 1355 | ` * to [var_dump(),var_export(),print_r(),...].` |
|       - | 1356 | ` * This function SXRET_OK on success. Any other return value including` |
|       - | 1357 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|       - | 1358 | ` */` |
|       - | 1359 | `/*` |
|       - | 1360 | `` * Return the `name` property value of an enum case instance (the case name),`` |
|       - | 1361 | ` * or 0 when unavailable. Shared by the var_dump/var_export/json/serialize` |
|       - | 1362 | ` * renderers, which all print enum cases as Class::CaseName forms.` |
|       - | 1363 | ` */` |
|       6 | 1364 | `PH7_PRIVATE ph7_value * PH7_EnumCaseNameValue(ph7_class_instance *pThis)` |
|       1 | 1365 | `{` |
|       - | 1366 | `	SyHashEntry *pEntry;` |
|       7 | 1367 | `	if( (pThis->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|     ! 0 | 1368 | `		return 0;` |
|       - | 1369 | `	}` |
|       7 | 1370 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)"name",sizeof("name")-1);` |
|       7 | 1371 | `	if( pEntry == 0 ){` |
|     ! 0 | 1372 | `		return 0;` |
|       - | 1373 | `	}` |
|       7 | 1374 | `	return PH7_ClassInstanceExtractAttrValue(pThis,(VmClassAttr *)pEntry->pUserData);` |
|       4 | 1375 | `}` |
|       - | 1376 | `/*` |
|       - | 1377 | `` * Return the `value` property value (the backing value) of an enum case`` |
|       - | 1378 | ` * instance, or 0 when unavailable (pure enums have none).` |
|       - | 1379 | ` */` |
|       8 | 1380 | `PH7_PRIVATE ph7_value * PH7_EnumCaseBackingValueOf(ph7_class_instance *pThis)` |
|       1 | 1381 | `{` |
|       - | 1382 | `	SyHashEntry *pEntry;` |
|       9 | 1383 | `	if( (pThis->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|     ! 0 | 1384 | `		return 0;` |
|       - | 1385 | `	}` |
|       9 | 1386 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)"value",sizeof("value")-1);` |
|       9 | 1387 | `	if( pEntry == 0 ){` |
|       3 | 1388 | `		return 0;` |
|       - | 1389 | `	}` |
|       7 | 1390 | `	return PH7_ClassInstanceExtractAttrValue(pThis,(VmClassAttr *)pEntry->pUserData);` |
|       5 | 1391 | `}` |
|       - | 1392 | `/*` |
|       - | 1393 | ` * Emit a class-instance dump header plus its trailing newline. For var_dump` |
|       - | 1394 | ` * (ShowType) it completes the "object(" prefix the caller already emitted as` |
|       - | 1395 | ` *   ClassName)#<id> (<count>) {` |
|       - | 1396 | ` * for print_r it emits the legacy PHL  Object(ClassName) {  (count/id unused).` |
|       - | 1397 | `` * Enum cases print php's `ClassName Enum {` print_r header (var_dump never`` |
|       - | 1398 | `` * reaches here for enums — PH7_MemObjDump prints `enum(S::A)` directly).`` |
|       - | 1399 | ` */` |
|     136 | 1400 | `static void DumpClassInstanceHeader(SyBlob *pOut,ph7_class *pClass,sxu32 nObjId,int ShowType,sxu32 nCount)` |
|       3 | 1401 | `{` |
|     139 | 1402 | `	if( ShowType ){` |
|       - | 1403 | ``		/* var_dump: `object(C)#id (n) {` */`` |
|     135 | 1404 | `		SyBlobFormat(&(*pOut),"object(%z)#%u (%u) {",&pClass->sName,nObjId,nCount);` |
|     135 | 1405 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|     135 | 1406 | `		return;` |
|       - | 1407 | `	}` |
|       - | 1408 | ``	/* print_r: `C Object` / `E Enum[:backing]` — the '(' line is emitted by`` |
|       - | 1409 | `	 * the body renderer at the container indent. */` |
|       6 | 1410 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|     ! 0 | 1411 | `		SyBlobFormat(&(*pOut),"%z Enum",&pClass->sName);` |
|     ! 0 | 1412 | `		if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|     ! 0 | 1413 | `			SyBlobAppend(&(*pOut),":int",sizeof(":int")-1);` |
|     ! 0 | 1414 | `		}else if( pClass->nEnumBacking == MEMOBJ_STRING ){` |
|     ! 0 | 1415 | `			SyBlobAppend(&(*pOut),":string",sizeof(":string")-1);` |
|     ! 0 | 1416 | `		}` |
|     ! 0 | 1417 | `	}else{` |
|       6 | 1418 | `		SyBlobFormat(&(*pOut),"%z Object",&pClass->sName);` |
|       - | 1419 | `	}` |
|       6 | 1420 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      71 | 1421 | `}` |
|       - | 1422 | `/*` |
|       - | 1423 | ` * The class that DECLARED pAttr: inheritance shares attr pointers down the` |
|       - | 1424 | ` * chain, so the declaring class is the most ANCESTRAL class whose hAttr still` |
|       - | 1425 | ` * maps the name to this exact pointer. php's var_dump/print_r use it for the` |
|       - | 1426 | `` * `["p":"Decl":private]` annotation.`` |
|       - | 1427 | ` */` |
|       6 | 1428 | `static ph7_class * OoAttrDeclaringClass(ph7_class *pClass,ph7_class_attr *pAttr)` |
|       1 | 1429 | `{` |
|       - | 1430 | `	/* Attrs record their declaring class at install time (inheritance/trait` |
|       - | 1431 | `	 * copies share the pointer, so the field survives the chain). */` |
|       7 | 1432 | `	return pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|       1 | 1433 | `}` |
|       - | 1434 | `/*` |
|       - | 1435 | `` * Emit a property's dump key: var_dump `["x"]=>` / `["p":"C":private]=>` /`` |
|       - | 1436 | `` * `["q":protected]=>`; print_r `[x] => ` / `[p:C:private] => ` /`` |
|       - | 1437 | `` * `[q:protected] => ` (php's exact annotations).`` |
|       - | 1438 | ` */` |
|     138 | 1439 | `static void OoDumpPropKey(SyBlob *pOut,ph7_class_instance *pThis,ph7_class_attr *pAttr,int ShowType)` |
|       2 | 1440 | `{` |
|     140 | 1441 | `	const char *zQ = ShowType ? "\"" : "";` |
|     140 | 1442 | `	SyBlobFormat(&(*pOut),"[%s%z%s",zQ,&pAttr->sName,zQ);` |
|     140 | 1443 | `	if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       7 | 1444 | `		ph7_class *pDecl = OoAttrDeclaringClass(pThis->pClass,pAttr);` |
|       7 | 1445 | `		SyBlobFormat(&(*pOut),":%s%z%s:private",zQ,&pDecl->sName,zQ);` |
|     137 | 1446 | `	}else if( pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
|     ! 0 | 1447 | `		SyBlobAppend(&(*pOut),":protected",sizeof(":protected")-1);` |
|     ! 0 | 1448 | `	}` |
|     140 | 1449 | `	SyBlobAppend(&(*pOut),ShowType ? "]=>" : "] => ",ShowType ? 3 : 5);` |
|     140 | 1450 | `}` |
|     140 | 1451 | `PH7_PRIVATE sxi32 PH7_ClassInstanceDump(SyBlob *pOut,ph7_class_instance *pThis,int ShowType,int nTab,int nDepth)` |
|       3 | 1452 | `{` |
|       - | 1453 | `	SyHashEntry *pEntry;` |
|       - | 1454 | `	ph7_value *pValue;` |
|       - | 1455 | `	sxi32 rc;` |
|       - | 1456 | `	int i;` |
|     143 | 1457 | `	if( nDepth > 31 ){` |
|       - | 1458 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|       - | 1459 | `		/* Nesting limit reached..halt immediately*/` |
|       5 | 1460 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|       5 | 1461 | `		return SXERR_LIMIT;` |
|       - | 1462 | `	}` |
|     139 | 1463 | `	rc = SXRET_OK;` |
|       - | 1464 | `	{` |
|       - | 1465 | `		/* Both var_dump and print_r consult __debugInfo() (PHP behavior);` |
|       - | 1466 | `		 * var_export uses a separate renderer and never reaches here. When the` |
|       - | 1467 | `		 * method is present and returns an array, render that array's entries as` |
|       - | 1468 | `		 * the object body, with the header showing the debug array's count. The` |
|       - | 1469 | `		 * nDepth guard above protects against a __debugInfo returning the object` |
|       - | 1470 | `		 * itself. */` |
|     139 | 1471 | `		ph7_class_method *pDbg = PH7_ClassExtractMethod(pThis->pClass,"__debugInfo",sizeof("__debugInfo")-1);` |
|     139 | 1472 | `		if( pDbg ){` |
|       - | 1473 | `			ph7_value sResult;` |
|       5 | 1474 | `			PH7_MemObjInit(pThis->pVm,&sResult);` |
|       5 | 1475 | `			PH7_VmCallClassMethod(pThis->pVm,pThis,pDbg,&sResult,0,0);` |
|       5 | 1476 | `			if( sResult.iFlags & MEMOBJ_HASHMAP ){` |
|       5 | 1477 | `				ph7_hashmap *pMap = (ph7_hashmap *)sResult.x.pOther;` |
|       - | 1478 | `				/* Header count is the debug array's entry count. */` |
|       5 | 1479 | `				DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,pMap->nEntry);` |
|       5 | 1480 | `				if( !ShowType ){` |
|       3 | 1481 | `					for( i = 0 ; i < nTab ; i++ ){` |
|     ! 0 | 1482 | `						SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     ! 0 | 1483 | `					}` |
|       3 | 1484 | `					SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       1 | 1485 | `				}` |
|       5 | 1486 | `				rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth);` |
|       5 | 1487 | `				for( i = 0 ; i < nTab ; i++ ){` |
|     ! 0 | 1488 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     ! 0 | 1489 | `				}` |
|       5 | 1490 | `				if( ShowType ){` |
|       3 | 1491 | `					SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|       2 | 1492 | `				}else{` |
|       3 | 1493 | `					SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       - | 1494 | `				}` |
|       5 | 1495 | `				PH7_MemObjRelease(&sResult);` |
|       5 | 1496 | `				return rc;` |
|       - | 1497 | `			}` |
|       - | 1498 | `			/* Non-array return: behave as if __debugInfo were absent. */` |
|     ! 0 | 1499 | `			PH7_MemObjRelease(&sResult);` |
|     ! 0 | 1500 | `		}` |
|       - | 1501 | `	}` |
|       - | 1502 | `	{` |
|       - | 1503 | `		/* var_dump's header needs the property count up front, so pre-count the` |
|       - | 1504 | `		 * non-static/non-constant attributes (matching the dump loop below). */` |
|     134 | 1505 | `		sxu32 nProp = 0;` |
|     134 | 1506 | `		if( ShowType ){` |
|     132 | 1507 | `			SyHashResetLoopCursor(&pThis->hAttr);` |
|     270 | 1508 | `			while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|     140 | 1509 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     140 | 1510 | `				if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_HOOK_VIRTUAL)) == 0 ){` |
|     136 | 1511 | `					nProp++;` |
|      67 | 1512 | `				}` |
|       2 | 1513 | `			}` |
|      65 | 1514 | `		}` |
|     134 | 1515 | `		DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,nProp);` |
|       - | 1516 | `	}` |
|     134 | 1517 | `	if( !ShowType ){` |
|       - | 1518 | `		/* print_r body opener: '(' at the container indent */` |
|       3 | 1519 | `		for( i = 0 ; i < nTab ; i++ ){` |
|     ! 0 | 1520 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     ! 0 | 1521 | `		}` |
|       3 | 1522 | `		SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       1 | 1523 | `	}` |
|       - | 1524 | `	/* Dump object attributes (php 8.4: VIRTUAL hooked properties have no` |
|       - | 1525 | `	 * backing store — excluded from var_dump/print_r) */` |
|     134 | 1526 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|     218 | 1527 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|     144 | 1528 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     144 | 1529 | `		if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_HOOK_VIRTUAL)) == 0 ){` |
|       - | 1530 | `			/* Dump non-static/constant attribute only */` |
|     140 | 1531 | `			pValue = ExtractClassAttrValue(pThis->pVm,pVmAttr);` |
|     140 | 1532 | `			if( pValue == 0 ){` |
|     ! 0 | 1533 | `				continue;` |
|       - | 1534 | `			}` |
|     140 | 1535 | `			if( ShowType ){` |
|       - | 1536 | ``				/* var_dump prop: `["x"(:…)]=>` at nTab+2, the value on the next`` |
|       - | 1537 | `				 * line at the same indent (php). */` |
|    4124 | 1538 | `				for( i = 0 ; i < nTab + 2 ; i++ ){` |
|    3990 | 1539 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|    1996 | 1540 | `				}` |
|     136 | 1541 | `				OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,TRUE);` |
|     136 | 1542 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|     136 | 1543 | `				rc = PH7_MemObjDump(&(*pOut),pValue,TRUE,nTab+2,nDepth,0);` |
|     136 | 1544 | `				if( rc == SXERR_LIMIT ){` |
|     125 | 1545 | `					break;` |
|       - | 1546 | `				}` |
|       7 | 1547 | `			}else{` |
|       - | 1548 | ``				/* print_r prop: `[x(:…)] => value` at nTab+4; container values`` |
|       - | 1549 | `				 * render their block at nTab+8 followed by php's blank line. */` |
|      21 | 1550 | `				for( i = 0 ; i < nTab + 4 ; i++ ){` |
|      17 | 1551 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       9 | 1552 | `				}` |
|       5 | 1553 | `				OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,FALSE);` |
|       4 | 1554 | `				if( (pValue->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ))` |
|       3 | 1555 | `				 && (pValue->iFlags & MEMOBJ_NULL) == 0 ){` |
|     ! 0 | 1556 | `					rc = PH7_MemObjDump(&(*pOut),pValue,FALSE,nTab+8,nDepth,0);` |
|     ! 0 | 1557 | `					SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|     ! 0 | 1558 | `					if( rc == SXERR_LIMIT ){` |
|     ! 0 | 1559 | `						break;` |
|       - | 1560 | `					}` |
|     ! 0 | 1561 | `				}else{` |
|       5 | 1562 | `					PH7_MemObjPrintRInline(&(*pOut),pValue);` |
|       5 | 1563 | `					SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 1564 | `				}` |
|       - | 1565 | `			}` |
|       7 | 1566 | `		}` |
|       2 | 1567 | `	}` |
|    3854 | 1568 | `	for( i = 0 ; i < nTab ; i++ ){` |
|    3721 | 1569 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|    1861 | 1570 | `	}` |
|     134 | 1571 | `	if( ShowType ){` |
|     132 | 1572 | `		SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      67 | 1573 | `	}else{` |
|       3 | 1574 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       - | 1575 | `	}` |
|     134 | 1576 | `	return rc;` |
|      73 | 1577 | `}` |
|       - | 1578 | `/*` |
|       - | 1579 | ` * Call a magic method [i.e: __toString(),__toBool(),__Invoke()...]` |
|       - | 1580 | ` * Return SXRET_OK on successfull call. Any other return value indicates failure.` |
|       - | 1581 | ` * Notes on magic methods.` |
|       - | 1582 | ` * According to the PHP language reference manual.` |
|       - | 1583 | ` *  The function names __construct(), __destruct(), __call(), __callStatic()` |
|       - | 1584 | ` *  __get(),  __toString(), __invoke(), __clone() are magical in PHP classes.` |
|       - | 1585 | ` * You cannot have functions with these names in any of your classes unless` |
|       - | 1586 | ` * you want the magic functionality associated with them.` |
|       - | 1587 | ` * Example of magical methods:` |
|       - | 1588 | ` * __toString()` |
|       - | 1589 | ` *  The __toString() method allows a class to decide how it will react when it is treated like` |
|       - | 1590 | ` *  a string. For example, what echo $obj; will print. This method must return a string.` |
|       - | 1591 | ` *  Example #2 Simple example` |
|       - | 1592 | ` * <?php` |
|       - | 1593 | ` * // Declare a simple class` |
|       - | 1594 | ` * class TestClass` |
|       - | 1595 | ` * {` |
|       - | 1596 | ` *   public $foo;` |
|       - | 1597 | ` *` |
|       - | 1598 | ` *   public function __construct($foo)` |
|       - | 1599 | ` *   {` |
|       - | 1600 | ` *       $this->foo = $foo;` |
|       - | 1601 | ` *   }` |
|       - | 1602 | ` *` |
|       - | 1603 | ` *   public function __toString()` |
|       - | 1604 | ` *   {` |
|       - | 1605 | ` *       return $this->foo;` |
|       - | 1606 | ` *   }` |
|       - | 1607 | ` * }` |
|       - | 1608 | ` * $class = new TestClass('Hello');` |
|       - | 1609 | ` * echo $class;` |
|       - | 1610 | ` * ?>` |
|       - | 1611 | ` * The above example will output:` |
|       - | 1612 | ` *  Hello` |
|       - | 1613 | ` *` |
|       - | 1614 | ` * Note that PH7 does not support all the magical method and introudces __toFloat(),__toInt()` |
|       - | 1615 | ` * which have the same behaviour as __toString() but for float and integer types` |
|       - | 1616 | ` * respectively.` |
|       - | 1617 | ` * Refer to the official documentation for more information.` |
|       - | 1618 | ` */` |
|      54 | 1619 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallMagicMethod(` |
|       - | 1620 | `	ph7_vm *pVm,               /* VM that own all this stuff */` |
|       - | 1621 | `	ph7_class *pClass,         /* Target class */` |
|       - | 1622 | `	ph7_class_instance *pThis, /* Target object */` |
|       - | 1623 | `	const char *zMethod,       /* Magic method name [i.e: __toString()]*/` |
|       - | 1624 | `	sxu32 nByte,               /* zMethod length*/` |
|       - | 1625 | `	const SyString *pAttrName, /* Attribute name */` |
|       - | 1626 | `	ph7_value *pResult         /* OUT: magic method return value. NULL to discard */` |
|       - | 1627 | `	)` |
|       1 | 1628 | `{` |
|      55 | 1629 | `	ph7_value *apArg[2] = { 0 , 0 };` |
|       - | 1630 | `	ph7_class_method *pMeth;` |
|       - | 1631 | `	ph7_value sAttr; /* cc warning */` |
|       - | 1632 | `	sxi32 rc;` |
|       - | 1633 | `	int nArg;` |
|       - | 1634 | `	/* Make sure the magic method is available */` |
|      55 | 1635 | `	pMeth = PH7_ClassExtractMethod(&(*pClass),zMethod,nByte);` |
|      55 | 1636 | `	if( pMeth == 0 ){` |
|       - | 1637 | `		/* No such method,return immediately */` |
|     ! 0 | 1638 | `		return SXERR_NOTFOUND;` |
|       - | 1639 | `	}` |
|      55 | 1640 | `	nArg = 0;` |
|       - | 1641 | `	/* Copy arguments */` |
|      55 | 1642 | `	if( pAttrName ){` |
|      55 | 1643 | `		PH7_MemObjInitFromString(pVm,&sAttr,pAttrName);` |
|      55 | 1644 | `		sAttr.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      55 | 1645 | `		apArg[0] = &sAttr;` |
|      55 | 1646 | `		nArg = 1;` |
|      27 | 1647 | `	}` |
|       - | 1648 | `	/* Call the magic method now */` |
|      55 | 1649 | `	rc = PH7_VmCallClassMethod(pVm,&(*pThis),pMeth,pResult,nArg,apArg);` |
|       - | 1650 | `	/* Clean up */` |
|      55 | 1651 | `	if( pAttrName ){` |
|      55 | 1652 | `		PH7_MemObjRelease(&sAttr);` |
|      27 | 1653 | `	}` |
|      55 | 1654 | `	return rc;` |
|      28 | 1655 | `}` |
|       - | 1656 | `/*` |
|       - | 1657 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon].` |
|       - | 1658 | ` * This function is simply a wrapper on ExtractClassAttrValue().` |
|       - | 1659 | ` */` |
|     216 | 1660 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceExtractAttrValue(ph7_class_instance *pThis,VmClassAttr *pAttr)` |
|       3 | 1661 | `{` |
|       - | 1662 | `   /* Extract the attribute value */` |
|       - | 1663 | `	ph7_value *pValue;` |
|     219 | 1664 | `	pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     219 | 1665 | `	return pValue;` |
|       3 | 1666 | `}` |
|       - | 1667 | `/*` |
|       - | 1668 | ` * Convert a class instance [i.e: Object in the PHP jargon] into a hashmap [i.e: array in the PHP jargon].` |
|       - | 1669 | ` * Return SXRET_OK on success. Any other value indicates failure.` |
|       - | 1670 | ` * Note on object conversion to array:` |
|       - | 1671 | ` *  Acccording to the PHP language reference manual` |
|       - | 1672 | ` *  If an object is converted to an array, the result is an array whose elements are the object's properties.` |
|       - | 1673 | ` *  The keys are the member variable names.` |
|       - | 1674 | ` *` |
|       - | 1675 | ` *  The following example:` |
|       - | 1676 | ` *  class Test {` |
|       - | 1677 | ` *   public $A = 25<<1;  // 50` |
|       - | 1678 | ` *	 public $c = rand_str(3);   // Random string of length 3` |
|       - | 1679 | ` *	 public $d = rand() & 1023; // Random number between 0..1023` |
|       - | 1680 | ` *  }` |
|       - | 1681 | ` *  var_dump((array) new Test());` |
|       - | 1682 | ` *	Will output:` |
|       - | 1683 | ` *  array(3) {` |
|       - | 1684 | ` *   [A] =>` |
|       - | 1685 | ` *      int(50)` |
|       - | 1686 | ` *   [c] =>` |
|       - | 1687 | ` *     string(3 'aps')` |
|       - | 1688 | ` *   [d] =>` |
|       - | 1689 | ` *     int(991)` |
|       - | 1690 | ` *  }` |
|       - | 1691 | ` * You have noticed that PH7 allow class attributes [i.e: $a,$c,$d in the example above]` |
|       - | 1692 | ` * have any complex expression (even function calls/Annonymous functions) as their default` |
|       - | 1693 | ` * value unlike the standard PHP engine.` |
|       - | 1694 | ` * This is a very powerful feature that you have to look at.` |
|       - | 1695 | ` */` |
|      14 | 1696 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|       1 | 1697 | `{` |
|       - | 1698 | `	SyHashEntry *pEntry;` |
|       - | 1699 | `	SyString *pAttrName;` |
|       - | 1700 | `	VmClassAttr *pAttr;` |
|       - | 1701 | `	ph7_value *pValue;` |
|       - | 1702 | `	ph7_value sName;` |
|       - | 1703 | `	/* Reset the loop cursor */` |
|      15 | 1704 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      15 | 1705 | `	PH7_MemObjInitFromString(pThis->pVm,&sName,0);` |
|      51 | 1706 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       - | 1707 | `		/* Point to the current attribute */` |
|      37 | 1708 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      37 | 1709 | `		if( pAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|       - | 1710 | `			/* php 8.4: a VIRTUAL hooked property has no backing store — the` |
|       - | 1711 | `			 * (array) cast excludes it (raw surface, get is NOT dispatched) */` |
|       7 | 1712 | `			continue;` |
|       - | 1713 | `		}` |
|       - | 1714 | `		/* Extract attribute value */` |
|      31 | 1715 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|      31 | 1716 | `		if( pValue ){` |
|       - | 1717 | `			/* Build attribute name */` |
|      31 | 1718 | `			pAttrName = &pAttr->pAttr->sName;` |
|      31 | 1719 | `			PH7_MemObjStringAppend(&sName,pAttrName->zString,pAttrName->nByte);` |
|       - | 1720 | `			/* Perform the insertion */` |
|      31 | 1721 | `			PH7_HashmapInsert(pMap,&sName,pValue);` |
|       - | 1722 | `			/* Reset the string cursor */` |
|      31 | 1723 | `			SyBlobReset(&sName.sBlob);` |
|      15 | 1724 | `		}` |
|       1 | 1725 | `	}` |
|      15 | 1726 | `	PH7_MemObjRelease(&sName);` |
|      15 | 1727 | `	return SXRET_OK;` |
|       1 | 1728 | `}` |
|       - | 1729 | `/*` |
|       - | 1730 | ` * Iterate throw class attributes and invoke the given callback [i.e: xWalk()] for each` |
|       - | 1731 | ` * retrieved attribute.` |
|       - | 1732 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|       - | 1733 | ` * the attribute value in the callback body will not alter the real attribute value.` |
|       - | 1734 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|       - | 1735 | ` * a value different from PH7_OK.` |
|       - | 1736 | ` * Refer to [ph7_object_walk()] for more information.` |
|       - | 1737 | ` */` |
|     ! 0 | 1738 | `PH7_PRIVATE sxi32 PH7_ClassInstanceWalk(` |
|       - | 1739 | `	ph7_class_instance *pThis, /* Target object */` |
|       - | 1740 | `	int (*xWalk)(const char *,ph7_value *,void *), /* Walker callback */` |
|       - | 1741 | `	void *pUserData /* Last argument to xWalk() */` |
|       - | 1742 | `	)` |
|     ! 0 | 1743 | `{` |
|       - | 1744 | `	SyHashEntry *pEntry; /* Hash entry */` |
|       - | 1745 | `	VmClassAttr *pAttr;  /* Pointer to the attribute */` |
|       - | 1746 | `	ph7_value *pValue;   /* Attribute value */` |
|       - | 1747 | `	ph7_value sValue;    /* Copy of the attribute value */` |
|       - | 1748 | `	int rc;` |
|       - | 1749 | `	/* Reset the loop cursor */` |
|     ! 0 | 1750 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|     ! 0 | 1751 | `	PH7_MemObjInit(pThis->pVm,&sValue);` |
|       - | 1752 | `	/* Start the walk process */` |
|     ! 0 | 1753 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       - | 1754 | `		/* Point to the current attribute */` |
|     ! 0 | 1755 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1756 | `		/* Extract attribute value */` |
|     ! 0 | 1757 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     ! 0 | 1758 | `		if( pValue ){` |
|     ! 0 | 1759 | `			PH7_MemObjLoad(pValue,&sValue);` |
|       - | 1760 | `			/* Invoke the supplied callback */` |
|     ! 0 | 1761 | `			rc =  xWalk(SyStringData(&pAttr->pAttr->sName),&sValue,pUserData);` |
|     ! 0 | 1762 | `			PH7_MemObjRelease(&sValue);` |
|     ! 0 | 1763 | `			if( rc != PH7_OK){` |
|       - | 1764 | `				/* User callback request an operation abort */` |
|     ! 0 | 1765 | `				return SXERR_ABORT;` |
|       - | 1766 | `			}` |
|     ! 0 | 1767 | `		}` |
|     ! 0 | 1768 | `	}` |
|       - | 1769 | `	/* All done */` |
|     ! 0 | 1770 | `	return SXRET_OK;` |
|     ! 0 | 1771 | `}` |
|       - | 1772 | `/*` |
|       - | 1773 | ` * Extract a class atrribute value.` |
|       - | 1774 | ` * Return a pointer to the attribute value on success. Otherwise NULL.` |
|       - | 1775 | ` * Note:` |
|       - | 1776 | ` *  Access to static and constant attribute is not allowed. That is,the function` |
|       - | 1777 | ` *  will return NULL in case someone (host-application code) try to extract` |
|       - | 1778 | ` *  a static/constant attribute.` |
|       - | 1779 | ` */` |
|    9888 | 1780 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceFetchAttr(ph7_class_instance *pThis,const SyString *pName)` |
|       5 | 1781 | `{` |
|       - | 1782 | `	SyHashEntry *pEntry;` |
|       - | 1783 | `	VmClassAttr *pAttr;` |
|       - | 1784 | `	/* Query the attribute hashtable */` |
|    9893 | 1785 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);` |
|    9893 | 1786 | `	if( pEntry == 0 ){` |
|       - | 1787 | `		/* No such attribute */` |
|     ! 0 | 1788 | `		return 0;` |
|       - | 1789 | `	}` |
|       - | 1790 | `	/* Point to the class atrribute */` |
|    9893 | 1791 | `	pAttr = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1792 | `	/* Check if we are dealing with a static/constant attribute */` |
|    9893 | 1793 | `	if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|       - | 1794 | `		/* Access is forbidden */` |
|     ! 0 | 1795 | `		return 0;` |
|       - | 1796 | `	}` |
|       - | 1797 | `	/* Return the attribute value */` |
|    9893 | 1798 | `	return ExtractClassAttrValue(pThis->pVm,pAttr);` |
|    4949 | 1799 | `}` |
|       - | 1800 |  |
