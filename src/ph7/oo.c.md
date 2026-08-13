# src/ph7/oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 667/727 lines (91.75%)

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
|  262022 |   14 | `PH7_PRIVATE ph7_class * PH7_NewRawClass(ph7_vm *pVm,const SyString *pName,sxu32 nLine)` |
|       5 |   15 | `{` |
|       - |   16 | `	ph7_class *pClass;` |
|       - |   17 | `	char *zName;` |
|       - |   18 | `	/* Allocate a new instance */` |
|  262027 |   19 | `	pClass = (ph7_class *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class));` |
|  262027 |   20 | `	if( pClass == 0 ){` |
|     ! 0 |   21 | `		return 0;` |
|       - |   22 | `	}` |
|       - |   23 | `	/* Zero the structure */` |
|  262027 |   24 | `	SyZero(pClass,sizeof(ph7_class));` |
|       - |   25 | `	/* Duplicate class name */` |
|  262027 |   26 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  262027 |   27 | `	if( zName == 0 ){` |
|     ! 0 |   28 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClass);` |
|     ! 0 |   29 | `		return 0;` |
|       - |   30 | `	}` |
|       - |   31 | `	/* Initialize fields */` |
|  262027 |   32 | `	SyStringInitFromBuf(&pClass->sName,zName,pName->nByte);` |
|  262027 |   33 | `	SyHashInit(&pClass->hMethod,&pVm->sAllocator,0,0);` |
|  262027 |   34 | `	SyHashInit(&pClass->hAttr,&pVm->sAllocator,0,0);` |
|  262027 |   35 | `	SyHashInit(&pClass->hDerived,&pVm->sAllocator,0,0);` |
|  262027 |   36 | `	SySetInit(&pClass->aInterface,&pVm->sAllocator,sizeof(ph7_class *));` |
|  262027 |   37 | `	SySetInit(&pClass->aTrait,&pVm->sAllocator,sizeof(ph7_class *));` |
|  262027 |   38 | `	SySetInit(&pClass->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|  262027 |   39 | `	SySetInit(&pClass->aEnumCases,&pVm->sAllocator,sizeof(ph7_class_attr *));` |
|  262027 |   40 | `	pClass->nLine = nLine;` |
|  262027 |   41 | `	if( pVm->bCompilingBuiltin ){` |
|       - |   42 | `		/* Defined by an embedded builtin chunk: internal, no defining file.` |
|       - |   43 | `		 * Class compilers merge further flags with \|= so this survives. */` |
|  260233 |   44 | `		pClass->iFlags \|= PH7_CLASS_INTERNAL;` |
|  130119 |   45 | `	}else{` |
|       - |   46 | `		/* Alias the VM-lifetime path dup on top of the include stack */` |
|    1799 |   47 | `		SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|    1799 |   48 | `		if( pFile ){` |
|    1799 |   49 | `			SyStringDupPtr(&pClass->sFile,pFile);` |
|     897 |   50 | `		}` |
|       - |   51 | `	}` |
|       - |   52 | `	/* All done */` |
|  262027 |   53 | `	return pClass;` |
|  131016 |   54 | `}` |
|       - |   55 | `/*` |
|       - |   56 | ` * Allocate and initialize a new class attribute.` |
|       - |   57 | ` * Return a pointer to the class attribute on success. NULL otherwise.` |
|       - |   58 | ` */` |
|  354576 |   59 | `PH7_PRIVATE ph7_class_attr * PH7_NewClassAttr(ph7_vm *pVm,const SyString *pName,sxu32 nLine,sxi32 iProtection,sxi32 iFlags)` |
|       5 |   60 | `{` |
|       - |   61 | `	ph7_class_attr *pAttr;` |
|       - |   62 | `	char *zName;` |
|  354581 |   63 | `	pAttr = (ph7_class_attr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_attr));` |
|  354581 |   64 | `	if( pAttr == 0 ){` |
|     ! 0 |   65 | `		return 0;` |
|       - |   66 | `	}` |
|       - |   67 | `	/* Zero the structure */` |
|  354581 |   68 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|  354581 |   69 | `	SySetInit(&pAttr->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|       - |   70 | `	/* Duplicate attribute name */` |
|  354581 |   71 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  354581 |   72 | `	if( zName == 0 ){` |
|     ! 0 |   73 | `		SyMemBackendPoolFree(&pVm->sAllocator,pAttr);` |
|     ! 0 |   74 | `		return 0;` |
|       - |   75 | `	}` |
|       - |   76 | `	/* Initialize fields */` |
|  354581 |   77 | `	SySetInit(&pAttr->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|  354581 |   78 | `	SySetInit(&pAttr->aUnionAlts,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|  354581 |   79 | `	SyStringInitFromBuf(&pAttr->sName,zName,pName->nByte);` |
|  354581 |   80 | `	pAttr->iProtection = iProtection;` |
|  354581 |   81 | `	pAttr->nIdx = SXU32_HIGH;` |
|  354581 |   82 | `	pAttr->iFlags = iFlags;` |
|  354581 |   83 | `	pAttr->nLine = nLine;` |
|  354581 |   84 | `	return pAttr;` |
|  177293 |   85 | `}` |
|       - |   86 | `/*` |
|       - |   87 | ` * Allocate and initialize a new class method.` |
|       - |   88 | ` * Return a pointer to the class method on success. NULL otherwise` |
|       - |   89 | ` * This function associate with the newly created method an automatically generated` |
|       - |   90 | ` * random unique name.` |
|       - |   91 | ` */` |
| 1388104 |   92 | `PH7_PRIVATE ph7_class_method * PH7_NewClassMethod(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,sxu32 nLine,` |
|       - |   93 | `	sxi32 iProtection,sxi32 iFlags,sxi32 iFuncFlags)` |
|       5 |   94 | `{` |
|       - |   95 | `	ph7_class_method *pMeth;` |
|       - |   96 | `	SyHashEntry *pEntry;` |
|       - |   97 | `	SyString *pNamePtr;` |
|       - |   98 | `	char zSalt[10];` |
|       - |   99 | `	char *zName;` |
|       - |  100 | `	sxu32 nByte;` |
|       - |  101 | `	/* Allocate a new class method instance */` |
| 1388109 |  102 | `	pMeth = (ph7_class_method *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_method));` |
| 1388109 |  103 | `	if( pMeth == 0 ){` |
|     ! 0 |  104 | `		return 0;` |
|       - |  105 | `	}` |
|       - |  106 | `	/* Zero the structure */` |
| 1388109 |  107 | `	SyZero(pMeth,sizeof(ph7_class_method));` |
|       - |  108 | `	/* Check for an already installed method with the same name */` |
| 1388109 |  109 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
| 1388109 |  110 | `	if( pEntry == 0 ){` |
|       - |  111 | `		/* Associate an unique VM name to this method */` |
| 1388107 |  112 | `		nByte = sizeof(zSalt) + pName->nByte + SyStringLength(&pClass->sName)+sizeof(char)*7/*[[__'\0'*/;` |
| 1388107 |  113 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,nByte);` |
| 1388107 |  114 | `		if( zName == 0 ){` |
|     ! 0 |  115 | `			SyMemBackendPoolFree(&pVm->sAllocator,pMeth);` |
|     ! 0 |  116 | `			return 0;` |
|       - |  117 | `		}` |
| 1388107 |  118 | `		pNamePtr = &pMeth->sVmName;` |
|       - |  119 | `		/* Generate a random string */` |
| 1388107 |  120 | `		PH7_VmRandomString(&(*pVm),zSalt,sizeof(zSalt));` |
| 1388107 |  121 | `		pNamePtr->nByte = SyBufferFormat(zName,nByte,"[__%z@%z_%.*s]",&pClass->sName,pName,sizeof(zSalt),zSalt);` |
| 1388107 |  122 | `		pNamePtr->zString = zName;` |
|  694056 |  123 | `	}else{` |
|       - |  124 | `		/* Method is condidate for 'overloading' */` |
|       3 |  125 | `		ph7_class_method *pCurrent = (ph7_class_method *)pEntry->pUserData;` |
|       3 |  126 | `		pNamePtr = &pMeth->sVmName;` |
|       - |  127 | `		/* Use the same VM name */` |
|       3 |  128 | `		SyStringDupPtr(pNamePtr,&pCurrent->sVmName);` |
|       3 |  129 | `		zName = (char *)pNamePtr->zString;` |
|       - |  130 | `	}` |
| 1388109 |  131 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|   58300 |  132 | `		if( (pName->nByte == sizeof("__destruct") - 1 && SyMemcmp(pName->zString,"__destruct",sizeof("__destruct") - 1 ) == 0)` |
|   58305 |  133 | `			\|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|       - |  134 | `				/* Switch to public visibility for destructors and legacy class-name` |
|       - |  135 | `				 * constructors (the engine invokes destructors internally, bypassing` |
|       - |  136 | `				 * visibility either way). __construct KEEPS its declared visibility` |
|       - |  137 | ``				 * (band A #4): php enforces it at `new` — a private/protected ctor`` |
|       - |  138 | `				 * from the wrong scope is a catchable Error, checked at OP_NEW —` |
|       - |  139 | `				 * and ReflectionClass::isInstantiable()/newInstance() now see it. */` |
|     ! 0 |  140 | `				iProtection = PH7_CLASS_PROT_PUBLIC;` |
|     ! 0 |  141 | `		}` |
|   29150 |  142 | `	}` |
|       - |  143 | `	/* Initialize method fields */` |
| 1388109 |  144 | `	pMeth->iProtection = iProtection;` |
| 1388109 |  145 | `	pMeth->iFlags = iFlags;` |
| 1388109 |  146 | `	pMeth->nLine = nLine;` |
| 2082161 |  147 | `	PH7_VmInitFuncState(&(*pVm),&pMeth->sFunc,&zName[sizeof(char)*4/*[__@*/+SyStringLength(&pClass->sName)],` |
| 1388104 |  148 | `		pName->nByte,iFuncFlags\|VM_FUNC_CLASS_METHOD,pClass);` |
| 1388109 |  149 | `	return pMeth;` |
|  694057 |  150 | `}` |
|       - |  151 | `/*` |
|       - |  152 | ` * Check if the given name have a class method associated with it.` |
|       - |  153 | ` * Return the desired method [i.e: ph7_class_method instance] on success. NULL otherwise.` |
|       - |  154 | ` */` |
|  322102 |  155 | `PH7_PRIVATE ph7_class_method * PH7_ClassExtractMethod(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|       5 |  156 | `{` |
|       - |  157 | `	SyHashEntry *pEntry;` |
|       - |  158 | `	/* Perform a hash lookup */` |
|  322107 |  159 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)zName,nByte);` |
|  322107 |  160 | `	if( pEntry == 0 ){` |
|       - |  161 | `		/* No such entry */` |
|    7059 |  162 | `		return 0;` |
|       - |  163 | `	}` |
|       - |  164 | `	/* Point to the desired method */` |
|  315053 |  165 | `	return (ph7_class_method *)pEntry->pUserData;` |
|  161056 |  166 | `}` |
|       - |  167 | `/*` |
|       - |  168 | ` * Check if the given name is a class attribute.` |
|       - |  169 | ` * Return the desired attribute [i.e: ph7_class_attr instance] on success.NULL otherwise.` |
|       - |  170 | ` */` |
|  211068 |  171 | `PH7_PRIVATE ph7_class_attr * PH7_ClassExtractAttribute(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|       5 |  172 | `{` |
|       - |  173 | `	SyHashEntry *pEntry;` |
|       - |  174 | `	/* Perform a hash lookup */` |
|  211073 |  175 | `	pEntry = SyHashGet(&pClass->hAttr,(const void *)zName,nByte);` |
|  211073 |  176 | `	if( pEntry == 0 ){` |
|       - |  177 | `		/* No such entry */` |
|  210705 |  178 | `		return 0;` |
|       - |  179 | `	}` |
|       - |  180 | `	/* Point to the desierd method */` |
|     373 |  181 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|  105539 |  182 | `}` |
|       - |  183 | `/*` |
|       - |  184 | ` * Install a class attribute in the corresponding container.` |
|       - |  185 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|       - |  186 | ` */` |
|  354572 |  187 | `PH7_PRIVATE sxi32 PH7_ClassInstallAttr(ph7_class *pClass,ph7_class_attr *pAttr)` |
|       5 |  188 | `{` |
|  354577 |  189 | `	SyString *pName = &pAttr->sName;` |
|       - |  190 | `	sxi32 rc;` |
|       - |  191 | `	/* Remember where this attribute was originally declared so that later` |
|       - |  192 | `	 * inheritance/trait copies still know the declaring class (needed for` |
|       - |  193 | `	 * PHP-compatible error messages on typed properties). */` |
|  354577 |  194 | `	if( pAttr->pDeclClass == 0 ){` |
|  354577 |  195 | `		pAttr->pDeclClass = pClass;` |
|  177286 |  196 | `	}` |
|  354577 |  197 | `	rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|  354577 |  198 | `	return rc;` |
|       5 |  199 | `}` |
|       - |  200 | `/*` |
|       - |  201 | ` * Install a class method in the corresponding container.` |
|       - |  202 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|       - |  203 | ` */` |
| 1388092 |  204 | `PH7_PRIVATE sxi32 PH7_ClassInstallMethod(ph7_class *pClass,ph7_class_method *pMeth)` |
|       5 |  205 | `{` |
| 1388097 |  206 | `	SyString *pName = &pMeth->sFunc.sName;` |
|       - |  207 | `	sxi32 rc;` |
| 1388097 |  208 | `	rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 1388097 |  209 | `	return rc;` |
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
|   31212 |  234 | `static int OoClassifyOverrideType(ph7_vm *pVm, sxu32 nType, const SyString *pClass,` |
|       - |  235 | `	int bUnion, ph7_class **ppClass)` |
|       5 |  236 | `{` |
|   31217 |  237 | `	*ppClass = 0;` |
|   31217 |  238 | `	if( bUnion ){` |
|       3 |  239 | `		return OVT_SKIP; /* union/intersection — full lattice, skip */` |
|       - |  240 | `	}` |
|   31215 |  241 | `	if( nType == 0 ){` |
|   31141 |  242 | `		return OVT_NONE; /* no declared type */` |
|       - |  243 | `	}` |
|      78 |  244 | `	if( nType == SXU32_HIGH ){` |
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
|      58 |  267 | `	if( nType == MEMOBJ_STRING \|\| nType == MEMOBJ_INT \|\| nType == MEMOBJ_REAL` |
|      28 |  268 | `	 \|\| nType == MEMOBJ_BOOL \|\| nType == MEMOBJ_HASHMAP ){` |
|      37 |  269 | `		return OVT_SCALAR;` |
|       - |  270 | `	}` |
|       - |  271 | `	/* MEMOBJ_OBJ (object — subtypes against classes), MEMOBJ_VOID/NULL/RES,` |
|       - |  272 | `	 * or anything unexpected: skip. */` |
|      26 |  273 | `	return OVT_SKIP;` |
|   15611 |  274 | `}` |
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
|   31176 |  288 | `static OvType OoTypeFromReturn(ph7_vm_func *pF)` |
|       5 |  289 | `{` |
|       - |  290 | `	OvType t;` |
|   31181 |  291 | `	t.nType = pF->nReturnType;` |
|   31181 |  292 | `	t.pClass = &pF->sReturnClass;` |
|   31181 |  293 | `	t.bUnion = SySetUsed(&pF->aReturnUnion) > 0;` |
|   31181 |  294 | `	t.bNullable = (pF->iFlags & VM_FUNC_RETURN_NULLABLE) != 0;` |
|   31181 |  295 | `	return t;` |
|       5 |  296 | `}` |
|      36 |  297 | `static OvType OoTypeFromArg(ph7_vm_func_arg *pA)` |
|       3 |  298 | `{` |
|       - |  299 | `	OvType t;` |
|      39 |  300 | `	t.nType = pA->nType;` |
|      39 |  301 | `	t.pClass = &pA->sClass;` |
|      39 |  302 | `	t.bUnion = (pA->iFlags & VM_FUNC_ARG_UNION) != 0;` |
|      39 |  303 | `	t.bNullable = (pA->iFlags & VM_FUNC_ARG_NULLABLE) != 0;` |
|      39 |  304 | `	return t;` |
|       3 |  305 | `}` |
|       - |  306 | `/*` |
|       - |  307 | ` * Return TRUE if the child type is an unambiguously-invalid override of the` |
|       - |  308 | ` * parent type. bCovariant=1 for a return type (child must be ⊆ parent),` |
|       - |  309 | ` * 0 for a parameter (child must be ⊇ parent). Returns FALSE (accept) on any` |
|       - |  310 | ` * skipped/ambiguous shape.` |
|       - |  311 | ` */` |
|   15606 |  312 | `static int OoOverrideTypeBad(ph7_vm *pVm, OvType parent, OvType child, int bCovariant)` |
|       5 |  313 | `{` |
|       - |  314 | `	ph7_class *pParentCls, *pChildCls;` |
|   15611 |  315 | `	int kP = OoClassifyOverrideType(pVm, parent.nType, parent.pClass, parent.bUnion, &pParentCls);` |
|   15611 |  316 | `	int kC = OoClassifyOverrideType(pVm, child.nType, child.pClass, child.bUnion, &pChildCls);` |
|   15611 |  317 | `	if( kP == OVT_SKIP \|\| kC == OVT_SKIP ){` |
|      18 |  318 | `		return 0; /* ambiguous shape — conservatively accept */` |
|       - |  319 | `	}` |
|       - |  320 | `	/* A missing type is the TOP type. covariant (return): a concrete child is a` |
|       - |  321 | `	 * subtype of top, fine; a top child over a concrete parent WIDENS → bad.` |
|       - |  322 | `	 * contravariant (param): a top child is a supertype of anything, fine; a` |
|       - |  323 | `	 * concrete child over a top parent NARROWS → bad. (A union/intersection child` |
|       - |  324 | `	 * already fell into OVT_SKIP above, so a flagged child here is scalar/class.) */` |
|   15595 |  325 | `	if( kP == OVT_NONE \|\| kC == OVT_NONE ){` |
|   15575 |  326 | `		if( bCovariant && kC == OVT_NONE && kP != OVT_NONE ) return 1;` |
|   15575 |  327 | `		if( !bCovariant && kP == OVT_NONE && kC != OVT_NONE ) return 1;` |
|   15575 |  328 | `		return 0;` |
|       - |  329 | `	}` |
|       - |  330 | `	/* Nullability: a covariant return may not ADD null; a contravariant param may` |
|       - |  331 | `	 * not REMOVE null. */` |
|      24 |  332 | `	if( bCovariant ){` |
|      11 |  333 | `		if( child.bNullable && !parent.bNullable ) return 1;` |
|       7 |  334 | `	}else{` |
|      14 |  335 | `		if( parent.bNullable && !child.bNullable ) return 1;` |
|       - |  336 | `	}` |
|      24 |  337 | `	if( kP == OVT_SCALAR && kC == OVT_SCALAR ){` |
|       - |  338 | `		/* Scalars are invariant — they must match exactly. */` |
|      17 |  339 | `		return (parent.nType != child.nType) ? 1 : 0;` |
|       - |  340 | `	}` |
|       8 |  341 | `	if( kP == OVT_CLASS && kC == OVT_CLASS ){` |
|       8 |  342 | `		if( bCovariant ){` |
|       3 |  343 | `			return PH7_VmInstanceOf(pChildCls, pParentCls) ? 0 : 1;  /* child ⊆ parent */` |
|       - |  344 | `		}` |
|       6 |  345 | `		return PH7_VmInstanceOf(pParentCls, pChildCls) ? 0 : 1;      /* child ⊇ parent */` |
|       - |  346 | `	}` |
|       - |  347 | `	/* One scalar and one class — disjoint. */` |
|     ! 0 |  348 | `	return 1;` |
|    7808 |  349 | `}` |
|       - |  350 |  |
|       - |  351 | `/*` |
|       - |  352 | ` * Check a child method's signature against the parent method it overrides.` |
|       - |  353 | ` * Emits a PHP-style "Declaration of … must be compatible …" fatal on a clear` |
|       - |  354 | `` * incompatibility. `__construct` is exempt (PHP does not apply variance to it).`` |
|       - |  355 | ` */` |
|   31130 |  356 | `static sxi32 OoCheckOverrideCompat(ph7_gen_state *pGen, ph7_class *pBase, ph7_class *pSub,` |
|       - |  357 | `	ph7_class_method *pParent, ph7_class_method *pChild)` |
|       5 |  358 | `{` |
|   31135 |  359 | `	ph7_vm *pVm = pGen->pVm;` |
|   31135 |  360 | `	ph7_vm_func *pPF = &pParent->sFunc;` |
|   31135 |  361 | `	ph7_vm_func *pCF = &pChild->sFunc;` |
|   31135 |  362 | `	SyString *pMName = &pCF->sName;` |
|       - |  363 | `	ph7_vm_func_arg *aP, *aC;` |
|       - |  364 | `	sxu32 nPArg, nCArg, k;` |
|   31135 |  365 | `	int bBad = 0;` |
|   31130 |  366 | `	if( pMName->nByte == sizeof("__construct")-1` |
|   23345 |  367 | `	 && SyStrnmicmp(pMName->zString,"__construct",pMName->nByte) == 0 ){` |
|   15547 |  368 | `		return SXRET_OK;` |
|       - |  369 | `	}` |
|       - |  370 | `	/* Return type — covariant. */` |
|   15593 |  371 | `	bBad = OoOverrideTypeBad(pVm, OoTypeFromReturn(pPF), OoTypeFromReturn(pCF), /* bCovariant */ 1);` |
|       - |  372 | `	/* Each overlapping parameter — contravariant. */` |
|   15593 |  373 | `	nPArg = SySetUsed(&pPF->aArgs);` |
|   15593 |  374 | `	nCArg = SySetUsed(&pCF->aArgs);` |
|   15593 |  375 | `	aP = (ph7_vm_func_arg *)SySetBasePtr(&pPF->aArgs);` |
|   15593 |  376 | `	aC = (ph7_vm_func_arg *)SySetBasePtr(&pCF->aArgs);` |
|   15611 |  377 | `	for( k = 0; !bBad && k < nPArg && k < nCArg; k++ ){` |
|      21 |  378 | `		bBad = OoOverrideTypeBad(pVm, OoTypeFromArg(&aP[k]), OoTypeFromArg(&aC[k]), /* bCovariant */ 0);` |
|      12 |  379 | `	}` |
|       - |  380 | `	/* Parameter arity: the child must declare at least the parent's parameters and` |
|       - |  381 | `	 * may add only OPTIONAL ones — PHP rejects dropping any param (even an optional` |
|       - |  382 | `	 * one) or adding a required one. Skip the rule if either signature is variadic` |
|       - |  383 | `	 * (arity semantics differ). */` |
|   15593 |  384 | `	if( !bBad ){` |
|   15589 |  385 | `		int bVariadic = 0;` |
|   15605 |  386 | `		for( k = 0; k < nPArg; k++ ){ if( aP[k].iFlags & VM_FUNC_ARG_VARIADIC ) bVariadic = 1; }` |
|   15607 |  387 | `		for( k = 0; k < nCArg; k++ ){ if( aC[k].iFlags & VM_FUNC_ARG_VARIADIC ) bVariadic = 1; }` |
|   15589 |  388 | `		if( !bVariadic ){` |
|   15589 |  389 | `			if( nCArg < nPArg ){` |
|     ! 0 |  390 | `				bBad = 1; /* dropped a parent parameter */` |
|     ! 0 |  391 | `			}else{` |
|   15591 |  392 | `				for( k = nPArg; k < nCArg; k++ ){` |
|       3 |  393 | `					if( SySetUsed(&aC[k].aByteCode) == 0 ){ bBad = 1; break; } /* new required */` |
|       2 |  394 | `				}` |
|       - |  395 | `			}` |
|    7792 |  396 | `		}` |
|    7792 |  397 | `	}` |
|   15593 |  398 | `	if( bBad ){` |
|       8 |  399 | `		sxi32 rc = PH7_GenCompileError(&(*pGen),E_ERROR,pChild->nLine,` |
|       - |  400 | `			"Declaration of %z::%z() must be compatible with %z::%z()",` |
|       2 |  401 | `			&pSub->sName,pMName,&pBase->sName,&pParent->sFunc.sName);` |
|       6 |  402 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  403 | `			return SXERR_ABORT;` |
|       - |  404 | `		}` |
|       2 |  405 | `	}` |
|   15593 |  406 | `	return SXRET_OK;` |
|   15570 |  407 | `}` |
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
|  124504 |  449 | `PH7_PRIVATE sxi32 PH7_ClassInherit(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase)` |
|       5 |  450 | `{` |
|       - |  451 | `	ph7_class_method *pMeth;` |
|       - |  452 | `	ph7_class_attr *pAttr;` |
|       - |  453 | `	SyHashEntry *pEntry;` |
|       - |  454 | `	SyString *pName;` |
|       - |  455 | `	sxi32 rc;` |
|       - |  456 | `	/* Install in the derived hashtable */` |
|  124509 |  457 | `	rc = SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|  124509 |  458 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  459 | `		return rc;` |
|       - |  460 | `	}` |
|       - |  461 | `	/* readonly class inheritance (PHP 8.2): a readonly class may only extend a` |
|       - |  462 | `	 * readonly class, and a non-readonly class may not extend a readonly one. */` |
|  124509 |  463 | `	if( (pBase->iFlags & PH7_CLASS_READONLY) != (pSub->iFlags & PH7_CLASS_READONLY) ){` |
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
|  124509 |  478 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
|  808543 |  479 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|       - |  480 | `		/* Make sure the private attributes are not redeclared in the subclass */` |
|  684039 |  481 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  684039 |  482 | `		pName = &pAttr->sName;` |
|  684039 |  483 | `		if( (pEntry = SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|       6 |  484 | `			if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_FINAL))` |
|       6 |  485 | `				== (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_FINAL) ){` |
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
|       9 |  497 | `			if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE &&` |
|       2 |  498 | `				((ph7_class_attr *)pEntry->pUserData)->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       - |  499 | `					/* Cannot redeclare private attribute */` |
|       4 |  500 | `					PH7_GenCompileError(&(*pGen),E_WARNING,((ph7_class_attr *)pEntry->pUserData)->nLine,` |
|       - |  501 | `						"Private attribute '%z::%z' redeclared inside child class '%z'",` |
|       1 |  502 | `						&pBase->sName,pName,&pSub->sName);` |
|       - |  503 |  |
|       1 |  504 | `			}` |
|       9 |  505 | `			continue;` |
|       - |  506 | `		}` |
|       - |  507 | `		/* Install the attribute */` |
|  684033 |  508 | `		if( pAttr->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
|  684027 |  509 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|  684027 |  510 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  511 | `				return rc;` |
|       - |  512 | `			}` |
|  342011 |  513 | `		}` |
|       5 |  514 | `	}` |
|  124509 |  515 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
| 1873079 |  516 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|       - |  517 | `		/* Make sure the private/final methods are not redeclared in the subclass */` |
| 1748575 |  518 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
| 1748575 |  519 | `		pName = &pMeth->sFunc.sName;` |
| 1748575 |  520 | `		if( (pEntry = SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|   31139 |  521 | `			 if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|       - |  522 | `				/* Cannot Overwrite final method */` |
|       8 |  523 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_method *)pEntry->pUserData)->nLine,` |
|       - |  524 | `					"Cannot Overwrite final method '%z:%z' inside child class '%z'",` |
|       2 |  525 | `					&pBase->sName,pName,&pSub->sName);` |
|       6 |  526 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  527 | `					return SXERR_ABORT;` |
|       - |  528 | `				}` |
|       4 |  529 | `			}else{` |
|       - |  530 | `				/* Check the override's signature is compatible with the parent's. */` |
|   46700 |  531 | `				rc = OoCheckOverrideCompat(&(*pGen),pBase,pSub,pMeth,` |
|   31130 |  532 | `					(ph7_class_method *)pEntry->pUserData);` |
|   31135 |  533 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  534 | `					return SXERR_ABORT;` |
|       - |  535 | `				}` |
|       - |  536 | `			}` |
|   31139 |  537 | `			continue;` |
|       - |  538 | `		}` |
|       - |  539 | `		/* Install the method */` |
| 1717441 |  540 | `		if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
| 1717437 |  541 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 1717437 |  542 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  543 | `				return rc;` |
|       - |  544 | `			}` |
|  858716 |  545 | `		}` |
|       5 |  546 | `	}` |
|       - |  547 | `	/* Mark as subclass */` |
|  124509 |  548 | `	pSub->pBase = pBase;` |
|       - |  549 | `	/* All done */` |
|  124509 |  550 | `	return SXRET_OK;` |
|   62257 |  551 | `}` |
|       - |  552 | `/*` |
|       - |  553 | ` * Apply a trait to a class: copy all methods and attributes from the trait` |
|       - |  554 | ` * into the target class. Unlike inheritance, traits copy ALL members including` |
|       - |  555 | ` * private ones. Members already defined in the class take precedence.` |
|       - |  556 | ` */` |
|      56 |  557 | `PH7_PRIVATE sxi32 PH7_ClassUseTrait(ph7_gen_state *pGen,ph7_class *pClass,ph7_class *pTrait)` |
|       5 |  558 | `{` |
|       - |  559 | `	ph7_class_method *pMeth;` |
|       - |  560 | `	ph7_class_attr *pAttr;` |
|       - |  561 | `	SyHashEntry *pEntry;` |
|       - |  562 | `	SyString *pName;` |
|       - |  563 | `	sxi32 rc;` |
|       - |  564 | `	/* Detect cyclic trait composition (e.g. trait A { use B; } trait B { use A; }) */` |
|      61 |  565 | `	if( pTrait->iFlags & PH7_CLASS_TRAIT_VISITING ){` |
|     ! 0 |  566 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|     ! 0 |  567 | `			"Trait circular reference detected: %z is already being applied",&pTrait->sName);` |
|     ! 0 |  568 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  569 | `			return SXERR_ABORT;` |
|       - |  570 | `		}` |
|     ! 0 |  571 | `		return SXRET_OK;` |
|       - |  572 | `	}` |
|      61 |  573 | `	pTrait->iFlags \|= PH7_CLASS_TRAIT_VISITING;` |
|      61 |  574 | `	rc = SXRET_OK;` |
|       - |  575 | `	/* Copy attributes from the trait */` |
|      61 |  576 | `	SyHashResetLoopCursor(&pTrait->hAttr);` |
|      77 |  577 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hAttr)) != 0 ){` |
|       - |  578 | `		SyHashEntry *pExisting;` |
|      20 |  579 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      20 |  580 | `		pName = &pAttr->sName;` |
|      20 |  581 | `		pExisting = SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte);` |
|      20 |  582 | `		if( pExisting != 0 ){` |
|       - |  583 | `			/* Attribute already exists. Check if it came from another trait` |
|       - |  584 | `			 * and whether the definitions are compatible (same defaults).` |
|       - |  585 | `			 */` |
|       - |  586 | `			ph7_class **apUsedTraits;` |
|       - |  587 | `			sxu32 nUsed,k;` |
|       6 |  588 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       6 |  589 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|       6 |  590 | `			for(k = 0; k < nUsed; k++){` |
|       - |  591 | `				ph7_class_attr *pOther;` |
|       3 |  592 | `				pOther = PH7_ClassExtractAttribute(apUsedTraits[k],pName->zString,pName->nByte);` |
|       3 |  593 | `				if( pOther ){` |
|       - |  594 | `					/* Two traits define the same property — check if defaults differ */` |
|       3 |  595 | `					ph7_class_attr *pClassAttr = (ph7_class_attr *)pExisting->pUserData;` |
|       4 |  596 | `					if( SySetUsed(&pAttr->aByteCode) != SySetUsed(&pClassAttr->aByteCode) \|\|` |
|       3 |  597 | `						(SySetUsed(&pAttr->aByteCode) > 0 &&` |
|       3 |  598 | `						 SyMemcmp(SySetBasePtr(&pAttr->aByteCode),SySetBasePtr(&pClassAttr->aByteCode),` |
|       3 |  599 | `							SySetUsed(&pAttr->aByteCode) * SySetElemSize(&pAttr->aByteCode)) != 0) ){` |
|       4 |  600 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine,` |
|       - |  601 | `							"%z and %z define the same property ($%z) in the composition of %z. "` |
|       - |  602 | `							"However, the definition differs and is considered incompatible",` |
|       2 |  603 | `							&apUsedTraits[k]->sName,&pTrait->sName,pName,&pClass->sName);` |
|       3 |  604 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  605 | `							goto cleanup;` |
|       - |  606 | `						}` |
|       1 |  607 | `					}` |
|       3 |  608 | `					break;` |
|       - |  609 | `				}` |
|     ! 0 |  610 | `			}` |
|       6 |  611 | `			continue;` |
|       - |  612 | `		}` |
|      16 |  613 | `		rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|      16 |  614 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  615 | `			goto cleanup;` |
|       - |  616 | `		}` |
|       4 |  617 | `	}` |
|       - |  618 | `	/* Copy methods from the trait */` |
|      61 |  619 | `	SyHashResetLoopCursor(&pTrait->hMethod);` |
|     113 |  620 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hMethod)) != 0 ){` |
|      57 |  621 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      57 |  622 | `		pName = &pMeth->sFunc.sName;` |
|      57 |  623 | `		if( SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte) != 0 ){` |
|       - |  624 | `			/* Method already exists in the class. Check if it came from another trait` |
|       - |  625 | `			 * (unresolved conflict) vs being defined by the class itself.` |
|       - |  626 | `			 */` |
|       - |  627 | `			ph7_class **apUsedTraits;` |
|       - |  628 | `			sxu32 nUsed,k;` |
|      11 |  629 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|      11 |  630 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|      11 |  631 | `			for(k = 0; k < nUsed; k++){` |
|       3 |  632 | `				if( PH7_ClassExtractMethod(apUsedTraits[k],pName->zString,pName->nByte) != 0 ){` |
|       - |  633 | `					/* Two different traits define the same method with no resolution */` |
|       4 |  634 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|       - |  635 | `						"Trait method %z::%z has not been applied as %z::%z, "` |
|       - |  636 | `						"because of collision with %z::%z",` |
|       2 |  637 | `						&pTrait->sName,pName,` |
|       1 |  638 | `						&pClass->sName,pName,` |
|       2 |  639 | `						&apUsedTraits[k]->sName,pName);` |
|       3 |  640 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  641 | `						goto cleanup;` |
|       - |  642 | `					}` |
|       3 |  643 | `					break;` |
|       - |  644 | `				}` |
|     ! 0 |  645 | `			}` |
|       - |  646 | `			/* Class-defined method takes precedence */` |
|      11 |  647 | `			continue;` |
|       - |  648 | `		}` |
|      49 |  649 | `		rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|      49 |  650 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  651 | `			goto cleanup;` |
|       - |  652 | `		}` |
|       5 |  653 | `	}` |
|       - |  654 | `	/* Record trait in the class */` |
|      61 |  655 | `	SySetPut(&pClass->aTrait,(const void *)&pTrait);` |
|      28 |  656 | `cleanup:` |
|       - |  657 | `	/* Always clear visiting flag, even on error paths */` |
|      61 |  658 | `	pTrait->iFlags &= ~PH7_CLASS_TRAIT_VISITING;` |
|      28 |  659 | `	SXUNUSED(pGen);` |
|      61 |  660 | `	return rc;` |
|      33 |  661 | `}` |
|       - |  662 | `/*` |
|       - |  663 | ` * Inherit an object interface from another object interface.` |
|       - |  664 | ` * According to the PHP language reference manual.` |
|       - |  665 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|       - |  666 | ` *  must implement, without having to define how these methods are handled.` |
|       - |  667 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  668 | ` *  class, but without any of the methods having their contents defined.` |
|       - |  669 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  670 | ` *` |
|       - |  671 | ` * This function return SXRET_OK if the interface inheritance operation was successfully performed.` |
|       - |  672 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|       - |  673 | ` * error message.` |
|       - |  674 | ` */` |
|   15546 |  675 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceInherit(ph7_class *pSub,ph7_class *pBase)` |
|       5 |  676 | `{` |
|       - |  677 | `	ph7_class_method *pMeth;` |
|       - |  678 | `	ph7_class_attr *pAttr;` |
|       - |  679 | `	SyHashEntry *pEntry;` |
|       - |  680 | `	SyString *pName;` |
|       - |  681 | `	sxi32 rc;` |
|       - |  682 | `	/* Install in the derived hashtable */` |
|   15551 |  683 | `	SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|   15551 |  684 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
|       - |  685 | `	/* Copy constants */` |
|   23326 |  686 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|       - |  687 | `		/* Make sure the constants are not redeclared in the subclass */` |
|       3 |  688 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|       3 |  689 | `		pName = &pAttr->sName;` |
|       3 |  690 | `		if( SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte) == 0 ){` |
|       - |  691 | `			/* Install the constant in the subclass */` |
|       3 |  692 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|       3 |  693 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  694 | `				return rc;` |
|       - |  695 | `			}` |
|       1 |  696 | `		}` |
|       1 |  697 | `	}` |
|   15551 |  698 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
|       - |  699 | `	/* Copy methods signature */` |
|   31134 |  700 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|       - |  701 | `		/* Make sure the method are not redeclared in the subclass */` |
|    7815 |  702 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|    7815 |  703 | `		pName = &pMeth->sFunc.sName;` |
|    7815 |  704 | `		if( SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte) == 0 ){` |
|       - |  705 | `			/* Install the method */` |
|    7815 |  706 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|    7815 |  707 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  708 | `				return rc;` |
|       - |  709 | `			}` |
|    3905 |  710 | `		}` |
|       5 |  711 | `	}` |
|       - |  712 | `	/* Mark as subclass */` |
|   15551 |  713 | `	pSub->pBase = pBase;` |
|       - |  714 | `	/* All done */` |
|   15551 |  715 | `	return SXRET_OK;` |
|    7778 |  716 | `}` |
|       - |  717 | `/*` |
|       - |  718 | ` * Implements an object interface in the given main class.` |
|       - |  719 | ` * According to the PHP language reference manual.` |
|       - |  720 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|       - |  721 | ` *  must implement, without having to define how these methods are handled.` |
|       - |  722 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  723 | ` *  class, but without any of the methods having their contents defined.` |
|       - |  724 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  725 | ` *` |
|       - |  726 | ` * This function return SXRET_OK if the interface was successfully implemented.` |
|       - |  727 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|       - |  728 | ` * error message.` |
|       - |  729 | ` */` |
|  213946 |  730 | `PH7_PRIVATE sxi32 PH7_ClassImplement(ph7_class *pMain,ph7_class *pInterface)` |
|       5 |  731 | `{` |
|       - |  732 | `	ph7_class_attr *pAttr;` |
|       - |  733 | `	SyHashEntry *pEntry;` |
|       - |  734 | `	SyString *pName;` |
|       - |  735 | `	sxi32 rc;` |
|       - |  736 | `	/* First off,copy all constants declared inside the interface */` |
|  213951 |  737 | `	SyHashResetLoopCursor(&pInterface->hAttr);` |
|  320934 |  738 | `	while((pEntry = SyHashGetNextEntry(&pInterface->hAttr)) != 0 ){` |
|       - |  739 | `		/* Point to the constant declaration */` |
|      11 |  740 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      11 |  741 | `		pName = &pAttr->sName;` |
|       - |  742 | `		/* Make sure the attribute is not redeclared in the main class */` |
|      11 |  743 | `		if( SyHashGet(&pMain->hAttr,pName->zString,pName->nByte) == 0 ){` |
|       - |  744 | `			/* Install the attribute */` |
|      11 |  745 | `			rc = SyHashInsert(&pMain->hAttr,pName->zString,pName->nByte,pAttr);` |
|      11 |  746 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  747 | `				return rc;` |
|       - |  748 | `			}` |
|       5 |  749 | `		}` |
|       1 |  750 | `	}` |
|       - |  751 | `	/* Install in the interface container */` |
|  213951 |  752 | `	SySetPut(&pMain->aInterface,(const void *)&pInterface);` |
|       - |  753 | `	/* Install interface method stubs into the implementing class.` |
|       - |  754 | `	 * Methods already defined in the class take precedence (they satisfy` |
|       - |  755 | `	 * the interface contract). Stubs retain PH7_CLASS_ATTR_ABSTRACT so` |
|       - |  756 | `	 * the unified check in GenStateCheckAbstractMethods catches missing ones.` |
|       - |  757 | `	 */` |
|       - |  758 | `	{` |
|       - |  759 | `		ph7_class_method *pMeth;` |
|       - |  760 | `		SyHashEntry *pMEntry;` |
|       - |  761 | `		SyString *pMName;` |
|  213951 |  762 | `		SyHashResetLoopCursor(&pInterface->hMethod);` |
|  605028 |  763 | `		while((pMEntry = SyHashGetNextEntry(&pInterface->hMethod)) != 0 ){` |
|  284109 |  764 | `			pMeth = (ph7_class_method *)pMEntry->pUserData;` |
|  284109 |  765 | `			pMName = &pMeth->sFunc.sName;` |
|  284109 |  766 | `			if( SyHashGet(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte) == 0 ){` |
|      18 |  767 | `				rc = SyHashInsert(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte,pMeth);` |
|      18 |  768 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  769 | `					return rc;` |
|       - |  770 | `				}` |
|       7 |  771 | `			}` |
|       5 |  772 | `		}` |
|       - |  773 | `	}` |
|  213951 |  774 | `	return SXRET_OK;` |
|  106978 |  775 | `}` |
|       - |  776 | `/*` |
|       - |  777 | ` * Create a class instance [i.e: Object in the PHP jargon] at run-time.` |
|       - |  778 | ` * The following function is called when an object is created at run-time` |
|       - |  779 | ` * typically when the PH7_OP_NEW/PH7_OP_CLONE instructions are executed.` |
|       - |  780 | ` * Notes on object creation.` |
|       - |  781 | ` *` |
|       - |  782 | ` * According to PHP language reference manual.` |
|       - |  783 | ` * To create an instance of a class, the new keyword must be used. An object will always` |
|       - |  784 | ` * be created unless the object has a constructor defined that throws an exception on error.` |
|       - |  785 | ` * Classes should be defined before instantiation (and in some cases this is a requirement).` |
|       - |  786 | ` * If a string containing the name of a class is used with new, a new instance of that class` |
|       - |  787 | ` * will be created. If the class is in a namespace, its fully qualified name must be used when` |
|       - |  788 | ` * doing this.` |
|       - |  789 | ` * Example #3 Creating an instance` |
|       - |  790 | ` * <?php` |
|       - |  791 | ` *  $instance = new SimpleClass();` |
|       - |  792 | ` *   // This can also be done with a variable:` |
|       - |  793 | ` * $className = 'Foo';` |
|       - |  794 | ` * $instance = new $className(); // Foo()` |
|       - |  795 | ` * ?>` |
|       - |  796 | ` * In the class context, it is possible to create a new object by new self and new parent.` |
|       - |  797 | ` * When assigning an already created instance of a class to a new variable, the new variable` |
|       - |  798 | ` * will access the same instance as the object that was assigned. This behaviour is the same` |
|       - |  799 | ` * when passing instances to a function. A copy of an already created object can be made by` |
|       - |  800 | ` * cloning it.` |
|       - |  801 | ` * Example #4 Object Assignment` |
|       - |  802 | ` * <?php` |
|       - |  803 | ` *  class SimpleClass(){` |
|       - |  804 | ` *    public $var;` |
|       - |  805 | ` *  };` |
|       - |  806 | ` *  $instance = new SimpleClass();` |
|       - |  807 | ` *  $assigned   =  $instance;` |
|       - |  808 | ` *  $reference  =& $instance;` |
|       - |  809 | ` *  $instance->var = '$assigned will have this value';` |
|       - |  810 | ` *  $instance = null; // $instance and $reference become null` |
|       - |  811 | ` *  var_dump($instance);` |
|       - |  812 | ` *  var_dump($reference);` |
|       - |  813 | ` *  var_dump($assigned);` |
|       - |  814 | ` * ?>` |
|       - |  815 | ` * The above example will output:` |
|       - |  816 | ` * NULL` |
|       - |  817 | ` * NULL` |
|       - |  818 | ` * object(SimpleClass)#1 (1) {` |
|       - |  819 | ` *  ["var"]=>` |
|       - |  820 | ` *    string(30) "$assigned will have this value"` |
|       - |  821 | ` * }` |
|       - |  822 | ` * Example #5 Creating new objects` |
|       - |  823 | ` * <?php` |
|       - |  824 | ` * class Test` |
|       - |  825 | ` * {` |
|       - |  826 | ` *   static public function getNew()` |
|       - |  827 | ` *   {` |
|       - |  828 | ` *       return new static;` |
|       - |  829 | ` *   }` |
|       - |  830 | ` * }` |
|       - |  831 | ` * class Child extends Test` |
|       - |  832 | ` * {}` |
|       - |  833 | ` * $obj1 = new Test();` |
|       - |  834 | ` * $obj2 = new $obj1;` |
|       - |  835 | ` * var_dump($obj1 !== $obj2);` |
|       - |  836 | ` * $obj3 = Test::getNew();` |
|       - |  837 | ` * var_dump($obj3 instanceof Test);` |
|       - |  838 | ` * $obj4 = Child::getNew();` |
|       - |  839 | ` * var_dump($obj4 instanceof Child);` |
|       - |  840 | ` * ?>` |
|       - |  841 | ` * The above example will output:` |
|       - |  842 | ` * bool(true)` |
|       - |  843 | ` * bool(true)` |
|       - |  844 | ` * bool(true)` |
|       - |  845 | ` * Note that Symisc Systems have introduced powerfull extension to` |
|       - |  846 | ` * OO subsystem. For example a class attribute may have any complex` |
|       - |  847 | ` * expression associated with it when declaring the attribute unlike` |
|       - |  848 | ` * the standard PHP engine which would allow a single value.` |
|       - |  849 | ` * Example:` |
|       - |  850 | ` *  class myClass{` |
|       - |  851 | ` *    public $var = 25<<1+foo()/bar();` |
|       - |  852 | ` *  };` |
|       - |  853 | ` * Refer to the official documentation for more information.` |
|       - |  854 | ` */` |
|    6266 |  855 | `static ph7_class_instance * NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|       5 |  856 | `{` |
|       - |  857 | `	ph7_class_instance *pThis;` |
|       - |  858 | `	/* Allocate a new instance */` |
|    6271 |  859 | `	pThis = (ph7_class_instance *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_instance));` |
|    6271 |  860 | `	if( pThis == 0 ){` |
|     ! 0 |  861 | `		return 0;` |
|       - |  862 | `	}` |
|       - |  863 | `	/* Zero the structure */` |
|    6271 |  864 | `	SyZero(pThis,sizeof(ph7_class_instance));` |
|       - |  865 | `	/* Initialize fields */` |
|    6271 |  866 | `	pThis->iRef = 1;` |
|    6271 |  867 | `	pThis->pVm = pVm;` |
|    6271 |  868 | `	pThis->pClass = pClass;` |
|       - |  869 | `	/* Assign a fresh monotonic object handle id (clones get their own, like PHP). */` |
|    6271 |  870 | `	pThis->nObjId = pVm->nNextObjId++;` |
|    6271 |  871 | `	SyHashInit(&pThis->hAttr,&pVm->sAllocator,0,0);` |
|    6271 |  872 | `	return pThis;` |
|    3138 |  873 | `}` |
|       - |  874 | `/*` |
|       - |  875 | ` * Wrapper around the NewClassInstance() function defined above.` |
|       - |  876 | ` * See the block comment above for more information.` |
|       - |  877 | ` */` |
|    6146 |  878 | `PH7_PRIVATE ph7_class_instance * PH7_NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|       5 |  879 | `{` |
|       - |  880 | `	ph7_class_instance *pNew;` |
|       - |  881 | `	sxi32 rc;` |
|    6151 |  882 | `	pNew = NewClassInstance(&(*pVm),&(*pClass));` |
|    6151 |  883 | `	if( pNew == 0 ){` |
|     ! 0 |  884 | `		return 0;` |
|       - |  885 | `	}` |
|       - |  886 | `	/* Associate a private VM frame with this class instance */` |
|    6151 |  887 | `	rc = PH7_VmCreateClassInstanceFrame(&(*pVm),pNew);` |
|    6151 |  888 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  889 | `		SyMemBackendPoolFree(&pVm->sAllocator,pNew);` |
|     ! 0 |  890 | `		return 0;` |
|       - |  891 | `	}` |
|    6151 |  892 | `	return pNew;` |
|    3078 |  893 | `}` |
|       - |  894 | `/*` |
|       - |  895 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon] attribute.` |
|       - |  896 | ` * This function never fail.` |
|       - |  897 | ` */` |
|   10668 |  898 | `static ph7_value * ExtractClassAttrValue(ph7_vm *pVm,VmClassAttr *pAttr)` |
|       5 |  899 | `{` |
|       - |  900 | `	/* Extract the value */` |
|       - |  901 | `	ph7_value *pValue;` |
|   10673 |  902 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|   10673 |  903 | `	return pValue;` |
|       5 |  904 | `}` |
|       - |  905 | `/*` |
|       - |  906 | ` * Perform a clone operation on a class instance [i.e: Object in the PHP jargon].` |
|       - |  907 | ` * The following function is called when an object is cloned at run-time` |
|       - |  908 | ` * typically when the PH7_OP_CLONE instruction is executed.` |
|       - |  909 | ` * Notes on object cloning.` |
|       - |  910 | ` *` |
|       - |  911 | ` * According to PHP language reference manual.` |
|       - |  912 | ` * Creating a copy of an object with fully replicated properties is not always the wanted behavior.` |
|       - |  913 | ` * A good example of the need for copy constructors. Another example is if your object holds a reference` |
|       - |  914 | ` * to another object which it uses and when you replicate the parent object you want to create` |
|       - |  915 | ` * a new instance of this other object so that the replica has its own separate copy.` |
|       - |  916 | ` * An object copy is created by using the clone keyword (which calls the object's __clone() method if possible).` |
|       - |  917 | ` * An object's __clone() method cannot be called directly.` |
|       - |  918 | ` * $copy_of_object = clone $object;` |
|       - |  919 | ` * When an object is cloned, PHP 5 will perform a shallow copy of all of the object's properties.` |
|       - |  920 | ` * Any properties that are references to other variables, will remain references.` |
|       - |  921 | ` * Once the cloning is complete, if a __clone() method is defined, then the newly created object's __clone() method` |
|       - |  922 | ` * will be called, to allow any necessary properties that need to be changed.` |
|       - |  923 | ` * Example #1 Cloning an object` |
|       - |  924 | ` * <?php` |
|       - |  925 | ` * class SubObject` |
|       - |  926 | ` * {` |
|       - |  927 | ` *   static $instances = 0;` |
|       - |  928 | ` *   public $instance;` |
|       - |  929 | ` *` |
|       - |  930 | ` *   public function __construct() {` |
|       - |  931 | ` *       $this->instance = ++self::$instances;` |
|       - |  932 | ` *   }` |
|       - |  933 | ` *` |
|       - |  934 | ` *   public function __clone() {` |
|       - |  935 | ` *       $this->instance = ++self::$instances;` |
|       - |  936 | ` *   }` |
|       - |  937 | ` * }` |
|       - |  938 | ` *` |
|       - |  939 | ` * class MyCloneable` |
|       - |  940 | ` * {` |
|       - |  941 | ` *   public $object1;` |
|       - |  942 | ` *   public $object2;` |
|       - |  943 | ` *` |
|       - |  944 | ` *   function __clone()` |
|       - |  945 | ` *   {` |
|       - |  946 | ` *       // Force a copy of this->object, otherwise` |
|       - |  947 | ` *       // it will point to same object.` |
|       - |  948 | ` *       $this->object1 = clone $this->object1;` |
|       - |  949 | ` *   }` |
|       - |  950 | ` * }` |
|       - |  951 | ` * $obj = new MyCloneable();` |
|       - |  952 | ` * $obj->object1 = new SubObject();` |
|       - |  953 | ` * $obj->object2 = new SubObject();` |
|       - |  954 | ` * $obj2 = clone $obj;` |
|       - |  955 | ` * print("Original Object:\n");` |
|       - |  956 | ` * print_r($obj);` |
|       - |  957 | ` * print("Cloned Object:\n");` |
|       - |  958 | ` * print_r($obj2);` |
|       - |  959 | ` * ?>` |
|       - |  960 | ` * The above example will output:` |
|       - |  961 | ` * Original Object:` |
|       - |  962 | ` * MyCloneable Object` |
|       - |  963 | ` * (` |
|       - |  964 | ` *   [object1] => SubObject Object` |
|       - |  965 | ` *       (` |
|       - |  966 | ` *           [instance] => 1` |
|       - |  967 | ` *       )` |
|       - |  968 | ` *` |
|       - |  969 | ` *   [object2] => SubObject Object` |
|       - |  970 | ` *       (` |
|       - |  971 | ` *           [instance] => 2` |
|       - |  972 | ` *       )` |
|       - |  973 | ` *` |
|       - |  974 | ` * )` |
|       - |  975 | ` * Cloned Object:` |
|       - |  976 | ` * MyCloneable Object` |
|       - |  977 | ` * (` |
|       - |  978 | ` *   [object1] => SubObject Object` |
|       - |  979 | ` *       (` |
|       - |  980 | ` *           [instance] => 3` |
|       - |  981 | ` *       )` |
|       - |  982 | ` *` |
|       - |  983 | ` *   [object2] => SubObject Object` |
|       - |  984 | ` *       (` |
|       - |  985 | ` *           [instance] => 2` |
|       - |  986 | ` *       )` |
|       - |  987 | ` * )` |
|       - |  988 | ` */` |
|     120 |  989 | `PH7_PRIVATE ph7_class_instance * PH7_CloneClassInstance(ph7_class_instance *pSrc)` |
|       3 |  990 | `{` |
|       - |  991 | `	ph7_class_instance *pClone;` |
|       - |  992 | `	ph7_class_method *pMethod;` |
|       - |  993 | `	SyHashEntry *pEntry2;` |
|       - |  994 | `	SyHashEntry *pEntry;` |
|       - |  995 | `	ph7_vm *pVm;` |
|       - |  996 | `	sxi32 rc;` |
|       - |  997 | `	/* Allocate a new instance */` |
|     123 |  998 | `	pVm = pSrc->pVm;` |
|     123 |  999 | `	pClone = NewClassInstance(pVm,pSrc->pClass);` |
|     123 | 1000 | `	if( pClone == 0 ){` |
|     ! 0 | 1001 | `		return 0;` |
|       - | 1002 | `	}` |
|       - | 1003 | `	/* Associate a private VM frame with this class instance */` |
|     123 | 1004 | `	rc = PH7_VmCreateClassInstanceFrame(pVm,pClone);` |
|     123 | 1005 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1006 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClone);` |
|     ! 0 | 1007 | `		return 0;` |
|       - | 1008 | `	}` |
|       - | 1009 | `	/* Duplicate object values. Iterate the SOURCE attributes and copy each into` |
|       - | 1010 | `	 * the clone's same-named slot (looked up by name, so order/count differences` |
|       - | 1011 | `	 * from dynamic properties don't matter). A dynamic (runtime-added) property` |
|       - | 1012 | `	 * has no declared counterpart in the clone, so synthesize it first — without` |
|       - | 1013 | `	 * this, a clone of a stdClass would silently lose all its dynamic properties. */` |
|     123 | 1014 | `	SyHashResetLoopCursor(&pSrc->hAttr);` |
|     341 | 1015 | `	while((pEntry = SyHashGetNextEntry(&pSrc->hAttr)) != 0 ){` |
|     221 | 1016 | `		VmClassAttr *pSrcAttr = (VmClassAttr *)pEntry->pUserData;` |
|     221 | 1017 | `		VmClassAttr *pDestAttr = 0;` |
|     221 | 1018 | `		ph7_value *pvSrc,*pvDest = 0;` |
|       - | 1019 | `		/* Duplicate non-static attribute */` |
|     221 | 1020 | `		if( pSrcAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|     ! 0 | 1021 | `			continue;` |
|       - | 1022 | `		}` |
|     221 | 1023 | `		pEntry2 = SyHashGet(&pClone->hAttr,SyStringData(&pSrcAttr->pAttr->sName),SyStringLength(&pSrcAttr->pAttr->sName));` |
|     221 | 1024 | `		if( pEntry2 ){` |
|     207 | 1025 | `			pDestAttr = (VmClassAttr *)pEntry2->pUserData;` |
|     207 | 1026 | `			pvDest = ExtractClassAttrValue(pVm,pDestAttr);` |
|     117 | 1027 | `		}else if( pSrcAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){` |
|       - | 1028 | `			/* Dynamic property: synthesize the matching slot on the clone. */` |
|      22 | 1029 | `			pvDest = PH7_VmCreateDynamicAttr(pVm,pClone,` |
|      14 | 1030 | `				SyStringData(&pSrcAttr->pAttr->sName),SyStringLength(&pSrcAttr->pAttr->sName),&pDestAttr);` |
|       7 | 1031 | `		}` |
|       - | 1032 | `		/* Fetch the source value LAST: PH7_VmCreateDynamicAttr above may have` |
|       - | 1033 | `		 * reserved a slot and reallocated pVm->aMemObj, which would dangle any` |
|       - | 1034 | `		 * ph7_value* obtained before it. pvDest from the synth path already points` |
|       - | 1035 | `		 * into the post-realloc aMemObj; resolve pvSrc now so both are current. */` |
|     221 | 1036 | `		pvSrc = ExtractClassAttrValue(pVm,pSrcAttr);` |
|     221 | 1037 | `		if( pvSrc && pvDest ){` |
|     221 | 1038 | `			PH7_MemObjStore(pvSrc,pvDest);` |
|     109 | 1039 | `		}` |
|       - | 1040 | `		/* Carry over the per-instance state so the clone matches the source:` |
|       - | 1041 | `		 * VM_CLASS_ATTR_UNINIT marks a typed property as not-yet-initialized` |
|       - | 1042 | `		 * and doubles as the readonly write-once latch — without this a clone` |
|       - | 1043 | `		 * would reset to uninitialized (losing the value's readiness) and a` |
|       - | 1044 | `		 * readonly property would become writable again. */` |
|     221 | 1045 | `		if( pDestAttr ){` |
|     221 | 1046 | `			pDestAttr->iState = pSrcAttr->iState;` |
|     109 | 1047 | `		}` |
|       3 | 1048 | `	}` |
|       - | 1049 | `	/* A declared property unset() on the source is absent from the clone too (PHP). But the clone` |
|       - | 1050 | `	 * frame above materialized ALL declared attrs (with their defaults), so drop any clone attr whose` |
|       - | 1051 | `	 * name is not present on the source. Collect first, then delete — removing an entry mid-walk would` |
|       - | 1052 | `	 * free the node the SyHash loop cursor points at. */` |
|       - | 1053 | `	{` |
|       - | 1054 | `		SySet sDrop;` |
|     123 | 1055 | `		SySetInit(&sDrop,&pVm->sAllocator,sizeof(VmClassAttr *));` |
|     123 | 1056 | `		SyHashResetLoopCursor(&pClone->hAttr);` |
|     343 | 1057 | `		while((pEntry = SyHashGetNextEntry(&pClone->hAttr)) != 0 ){` |
|     223 | 1058 | `			VmClassAttr *pCloneAttr = (VmClassAttr *)pEntry->pUserData;` |
|     223 | 1059 | `			if( pCloneAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|     ! 0 | 1060 | `				continue;` |
|       - | 1061 | `			}` |
|     330 | 1062 | `			if( SyHashGet(&pSrc->hAttr,SyStringData(&pCloneAttr->pAttr->sName),` |
|     333 | 1063 | `					SyStringLength(&pCloneAttr->pAttr->sName)) == 0 ){` |
|       3 | 1064 | `				SySetPut(&sDrop,(const void *)&pCloneAttr);` |
|       1 | 1065 | `			}` |
|       3 | 1066 | `		}` |
|     123 | 1067 | `		if( SySetUsed(&sDrop) > 0 ){` |
|       3 | 1068 | `			VmClassAttr **apDrop = (VmClassAttr **)SySetBasePtr(&sDrop);` |
|       - | 1069 | `			sxu32 i;` |
|       5 | 1070 | `			for( i = 0 ; i < SySetUsed(&sDrop) ; ++i ){` |
|       3 | 1071 | `				VmClassAttr *pVmAttr = apDrop[i];` |
|       4 | 1072 | `				SyHashDeleteEntry(&pClone->hAttr,SyStringData(&pVmAttr->pAttr->sName),` |
|       2 | 1073 | `					SyStringLength(&pVmAttr->pAttr->sName),0);` |
|       3 | 1074 | `				PH7_VmReleaseInstanceAttr(pVm,pVmAttr);` |
|       2 | 1075 | `			}` |
|       1 | 1076 | `		}` |
|     123 | 1077 | `		SySetRelease(&sDrop);` |
|       - | 1078 | `	}` |
|       - | 1079 | `	/* call the __clone method on the cloned object if available */` |
|     123 | 1080 | `	pMethod = PH7_ClassExtractMethod(pClone->pClass,"__clone",sizeof("__clone")-1);` |
|     123 | 1081 | `	if( pMethod ){` |
|      56 | 1082 | `		if( pMethod->iCloneDepth < 16 ){` |
|      54 | 1083 | `			pMethod->iCloneDepth++;` |
|      54 | 1084 | `			PH7_VmCallClassMethod(pVm,pClone,pMethod,0,0,0);` |
|      28 | 1085 | `		}else{` |
|       - | 1086 | `			/* Nesting limit reached */` |
|       3 | 1087 | `			PH7_VmThrowError(pVm,0,PH7_CTX_ERR,"Object clone limit reached,no more call to __clone()");` |
|       - | 1088 | `		}` |
|       - | 1089 | `		/* Reset the cursor */` |
|      56 | 1090 | `		pMethod->iCloneDepth = 0;` |
|      27 | 1091 | `	}` |
|       - | 1092 | `	/* Return the cloned object */` |
|     123 | 1093 | `	return pClone;` |
|      63 | 1094 | `}` |
|       - | 1095 | `#define CLASS_INSTANCE_DESTROYED 0x001 /* Instance is released */` |
|       - | 1096 | `/*` |
|       - | 1097 | ` * Free the per-instance allocations owned by ONE object attribute: its value slot (+ the typed-slot` |
|       - | 1098 | ` * enforcement entry), the synthesized ph7_class_attr for a dynamic (runtime-added) property, and the` |
|       - | 1099 | ` * VmClassAttr wrapper itself. Does NOT touch the hAttr entry node — the caller removes it` |
|       - | 1100 | `` * (`unset($o->p)` via SyHashDeleteEntry2; instance teardown via the wholesale SyHashRelease, so it must`` |
|       - | 1101 | ` * not delete entries mid-walk). Shared by PH7_ClassInstanceRelease and the OP_MEMBER unset path.` |
|       - | 1102 | ` */` |
|   17216 | 1103 | `PH7_PRIVATE void PH7_VmReleaseInstanceAttr(ph7_vm *pVm, VmClassAttr *pVmAttr)` |
|       5 | 1104 | `{` |
|   17221 | 1105 | `	if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|       - | 1106 | `		/* Drop any typed-property enforcement slot registered for this memobj, before the memobj` |
|       - | 1107 | `		 * is returned to the free list, so a future recycled slot does not inherit the stale entry. */` |
|   15291 | 1108 | `		if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|     260 | 1109 | `			SyHashDeleteEntry(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),0);` |
|     128 | 1110 | `		}` |
|   15291 | 1111 | `		PH7_VmUnsetMemObj(pVm,pVmAttr->nIdx,TRUE);` |
|    7643 | 1112 | `	}` |
|       - | 1113 | `	/* A dynamic property owns its synthesized ph7_class_attr (struct + inline name in one block) —` |
|       - | 1114 | `	 * free it here (the only place a per-instance pAttr is freed; declared attrs are class-owned). */` |
|   17221 | 1115 | `	if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){` |
|     111 | 1116 | `		SyMemBackendFree(&pVm->sAllocator,pVmAttr->pAttr);` |
|      55 | 1117 | `	}` |
|   17221 | 1118 | `	SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|   17221 | 1119 | `}` |
|       - | 1120 | `/*` |
|       - | 1121 | ` * Release a class instance [i.e: Object in the PHP jargon] and invoke any defined destructor.` |
|       - | 1122 | ` * This routine is invoked as soon as there are no other references to a particular` |
|       - | 1123 | ` * class instance.` |
|       - | 1124 | ` */` |
|    4166 | 1125 | `static void PH7_ClassInstanceRelease(ph7_class_instance *pThis)` |
|       5 | 1126 | `{` |
|       - | 1127 | `	ph7_class_method *pDestr;` |
|       - | 1128 | `	SyHashEntry *pEntry;` |
|       - | 1129 | `	ph7_class *pClass;` |
|       - | 1130 | `	ph7_vm *pVm;` |
|    4171 | 1131 | `	if( pThis->iFlags & CLASS_INSTANCE_DESTROYED ){` |
|       - | 1132 | `		/*` |
|       - | 1133 | `		 * Already destroyed,return immediately.` |
|       - | 1134 | `		 * This could happend if someone perform unset($this) in the destructor body.` |
|       - | 1135 | `		 */` |
|     ! 0 | 1136 | `		return;` |
|       - | 1137 | `	}` |
|       - | 1138 | `	/* Mark as destroyed */` |
|    4171 | 1139 | `	pThis->iFlags \|= CLASS_INSTANCE_DESTROYED;` |
|       - | 1140 | `	/* Invoke any defined destructor if available */` |
|    4171 | 1141 | `	pVm = pThis->pVm;` |
|    4171 | 1142 | `	pClass = pThis->pClass;` |
|    4171 | 1143 | `	pDestr = PH7_ClassExtractMethod(pClass,"__destruct",sizeof("__destruct")-1);` |
|    4171 | 1144 | `	if( pDestr && !pVm->bInReset ){` |
|       - | 1145 | `		/* Invoke the destructor. Skipped during ph7_vm_reset() bulk teardown:` |
|       - | 1146 | `		 * running user PHP against a half-reset VM is unsafe (see bInReset). */` |
|     439 | 1147 | `		pThis->iRef = 2; /* Prevent garbage collection */` |
|     439 | 1148 | `		PH7_VmCallClassMethod(pVm,pThis,pDestr,0,0,0);` |
|     217 | 1149 | `	}` |
|       - | 1150 | `	/* Release non-static attributes (the wholesale SyHashRelease below frees the entry nodes,` |
|       - | 1151 | `	 * so the helper must not delete them mid-walk). */` |
|    4171 | 1152 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|   21357 | 1153 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|   17191 | 1154 | `		PH7_VmReleaseInstanceAttr(pVm,(VmClassAttr *)pEntry->pUserData);` |
|       5 | 1155 | `	}` |
|       - | 1156 | `	/* Release the whole structure */` |
|    4171 | 1157 | `	SyHashRelease(&pThis->hAttr);` |
|    4171 | 1158 | `	SyMemBackendPoolFree(&pVm->sAllocator,pThis);` |
|    2088 | 1159 | `}` |
|       - | 1160 | `/*` |
|       - | 1161 | ` * Decrement the reference count of a class instance [i.e Object in the PHP jargon].` |
|       - | 1162 | ` * If the reference count reaches zero,release the whole instance.` |
|       - | 1163 | ` */` |
|   93916 | 1164 | `PH7_PRIVATE void PH7_ClassInstanceUnref(ph7_class_instance *pThis)` |
|       5 | 1165 | `{` |
|   93921 | 1166 | `	pThis->iRef--;` |
|   93921 | 1167 | `	if( pThis->iRef < 1 ){` |
|       - | 1168 | `		/* No more reference to this instance */` |
|    4171 | 1169 | `		PH7_ClassInstanceRelease(&(*pThis));` |
|    2083 | 1170 | `	}` |
|   93921 | 1171 | `}` |
|       - | 1172 | `/*` |
|       - | 1173 | ` * Compare two class instances [i.e: Objects in the PHP jargon]` |
|       - | 1174 | ` * Note on objects comparison:` |
|       - | 1175 | ` *  According to the PHP langauge reference manual` |
|       - | 1176 | ` *  When using the comparison operator (==), object variables are compared in a simple manner` |
|       - | 1177 | ` *  namely: Two object instances are equal if they have the same attributes and values, and are` |
|       - | 1178 | ` *  instances of the same class.` |
|       - | 1179 | ` *  On the other hand, when using the identity operator (===), object variables are identical` |
|       - | 1180 | ` *  if and only if they refer to the same instance of the same class.` |
|       - | 1181 | ` *  An example will clarify these rules.` |
|       - | 1182 | ` *  Example #1 Example of object comparison` |
|       - | 1183 | ` *  <?php` |
|       - | 1184 | ` *    function bool2str($bool)` |
|       - | 1185 | ` * {` |
|       - | 1186 | ` *   if ($bool === false) {` |
|       - | 1187 | ` *       return 'FALSE';` |
|       - | 1188 | ` *   } else {` |
|       - | 1189 | ` *       return 'TRUE';` |
|       - | 1190 | ` *   }` |
|       - | 1191 | ` * }` |
|       - | 1192 | ` * function compareObjects(&$o1, &$o2)` |
|       - | 1193 | ` * {` |
|       - | 1194 | ` *   echo 'o1 == o2 : ' . bool2str($o1 == $o2) . "\n";` |
|       - | 1195 | ` *   echo 'o1 != o2 : ' . bool2str($o1 != $o2) . "\n";` |
|       - | 1196 | ` *   echo 'o1 === o2 : ' . bool2str($o1 === $o2) . "\n";` |
|       - | 1197 | ` *   echo 'o1 !== o2 : ' . bool2str($o1 !== $o2) . "\n";` |
|       - | 1198 | ` * }` |
|       - | 1199 | ` * class Flag` |
|       - | 1200 | ` * {` |
|       - | 1201 | ` *   public $flag;` |
|       - | 1202 | ` *` |
|       - | 1203 | ` *   function Flag($flag = true) {` |
|       - | 1204 | ` *       $this->flag = $flag;` |
|       - | 1205 | ` *   }` |
|       - | 1206 | ` * }` |
|       - | 1207 | ` *` |
|       - | 1208 | ` * class OtherFlag` |
|       - | 1209 | ` * {` |
|       - | 1210 | ` *   public $flag;` |
|       - | 1211 | ` *` |
|       - | 1212 | ` *   function OtherFlag($flag = true) {` |
|       - | 1213 | ` *       $this->flag = $flag;` |
|       - | 1214 | ` *   }` |
|       - | 1215 | ` * }` |
|       - | 1216 | ` *` |
|       - | 1217 | ` * $o = new Flag();` |
|       - | 1218 | ` * $p = new Flag();` |
|       - | 1219 | ` * $q = $o;` |
|       - | 1220 | ` * $r = new OtherFlag();` |
|       - | 1221 | ` *` |
|       - | 1222 | ` * echo "Two instances of the same class\n";` |
|       - | 1223 | ` * compareObjects($o, $p);` |
|       - | 1224 | ` * echo "\nTwo references to the same instance\n";` |
|       - | 1225 | ` * compareObjects($o, $q);` |
|       - | 1226 | ` * echo "\nInstances of two different classes\n";` |
|       - | 1227 | ` * compareObjects($o, $r);` |
|       - | 1228 | ` * ?>` |
|       - | 1229 | ` * The above example will output:` |
|       - | 1230 | ` * Two instances of the same class` |
|       - | 1231 | ` * o1 == o2 : TRUE` |
|       - | 1232 | ` * o1 != o2 : FALSE` |
|       - | 1233 | ` * o1 === o2 : FALSE` |
|       - | 1234 | ` * o1 !== o2 : TRUE` |
|       - | 1235 | ` * Two references to the same instance` |
|       - | 1236 | ` * o1 == o2 : TRUE` |
|       - | 1237 | ` * o1 != o2 : FALSE` |
|       - | 1238 | ` * o1 === o2 : TRUE` |
|       - | 1239 | ` * o1 !== o2 : FALSE` |
|       - | 1240 | ` * Instances of two different classes` |
|       - | 1241 | ` * o1 == o2 : FALSE` |
|       - | 1242 | ` * o1 != o2 : TRUE` |
|       - | 1243 | ` * o1 === o2 : FALSE` |
|       - | 1244 | ` * o1 !== o2 : TRUE` |
|       - | 1245 | ` *` |
|       - | 1246 | ` * This function return 0 if the objects are equals according to the comprison rules defined above.` |
|       - | 1247 | ` * Any other return values indicates difference.` |
|       - | 1248 | ` */` |
|     252 | 1249 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest)` |
|       5 | 1250 | `{` |
|       - | 1251 | `	SyHashEntry *pEntry,*pEntry2;` |
|       - | 1252 | `	ph7_value sV1,sV2;` |
|       - | 1253 | `	sxi32 rc;` |
|     257 | 1254 | `	if( iNest > 31 ){` |
|       - | 1255 | `		/* Nesting limit reached */` |
|       6 | 1256 | `		PH7_VmThrowError(pLeft->pVm,0,PH7_CTX_ERR,"Nesting limit reached: Infinite recursion?");` |
|       6 | 1257 | `		return 1;` |
|       - | 1258 | `	}` |
|       - | 1259 | `	/* Comparison is performed only if the objects are instance of the same class */` |
|     253 | 1260 | `	if( pLeft->pClass != pRight->pClass ){` |
|       7 | 1261 | `		return 1;` |
|       - | 1262 | `	}` |
|     247 | 1263 | `	if( bStrict ){` |
|       - | 1264 | `		/*` |
|       - | 1265 | `		 * According to the PHP language reference manual:` |
|       - | 1266 | `		 *  when using the identity operator (===), object variables` |
|       - | 1267 | `		 *  are identical if and only if they refer to the same instance` |
|       - | 1268 | `		 *  of the same class.` |
|       - | 1269 | `		 */` |
|      95 | 1270 | `		return !(pLeft == pRight);` |
|       - | 1271 | `	}` |
|       - | 1272 | `	/*` |
|       - | 1273 | `	 * Attribute comparison.` |
|       - | 1274 | `	 * According to the PHP reference manual:` |
|       - | 1275 | `	 *  When using the comparison operator (==), object variables are compared` |
|       - | 1276 | `	 *  in a simple manner, namely: Two object instances are equal if they have` |
|       - | 1277 | `	 *  the same attributes and values, and are instances of the same class.` |
|       - | 1278 | `	 */` |
|     155 | 1279 | `	if( pLeft == pRight ){` |
|       - | 1280 | `		/* Same instance,don't bother processing,object are equals */` |
|       5 | 1281 | `		return 0;` |
|       - | 1282 | `	}` |
|       - | 1283 | `	/* Closures compare by IDENTITY under == as well (not by attributes): two distinct` |
|       - | 1284 | `	 * Closure instances are never equal, even when they wrap the same underlying function` |
|       - | 1285 | `	 * (PHP semantics). pLeft != pRight here, so a Closure pair is unequal. Without this,` |
|       - | 1286 | `` 	 * two capture-less lambdas of the same `function(){}` share the template's `$__fn` `` |
|       - | 1287 | `	 * name and would compare equal. */` |
|     151 | 1288 | `	if( pLeft->pVm->pClosureClass && pLeft->pClass == pLeft->pVm->pClosureClass ){` |
|       5 | 1289 | `		return 1;` |
|       - | 1290 | `	}` |
|       - | 1291 | `	/* Same class but a different number of attributes ⇒ different property sets` |
|       - | 1292 | `	 * (dynamic properties can give two same-class instances different counts). */` |
|     147 | 1293 | `	if( pLeft->hAttr.nEntry != pRight->hAttr.nEntry ){` |
|       3 | 1294 | `		return 1;` |
|       - | 1295 | `	}` |
|     145 | 1296 | `	PH7_MemObjInit(pLeft->pVm,&sV1);` |
|     145 | 1297 | `	PH7_MemObjInit(pLeft->pVm,&sV2);` |
|     145 | 1298 | `	sV1.nIdx = sV2.nIdx = SXU32_HIGH;` |
|       - | 1299 | `	/* Compare each left attribute against the RIGHT attribute of the SAME NAME` |
|       - | 1300 | `	 * (not in lockstep): dynamic properties may be stored in a different order` |
|       - | 1301 | `	 * on the two instances. Counts already match, so if every left attribute has` |
|       - | 1302 | `	 * an equal-valued same-named right attribute the property sets are equal. */` |
|     145 | 1303 | `	SyHashResetLoopCursor(&pLeft->hAttr);` |
|     169 | 1304 | `	while((pEntry = SyHashGetNextEntry(&pLeft->hAttr)) != 0 ){` |
|     157 | 1305 | `		VmClassAttr *p1 = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1306 | `		VmClassAttr *p2;` |
|       - | 1307 | `		ph7_value *pL,*pR;` |
|       - | 1308 | `		/* Compare only non-static attribute */` |
|     157 | 1309 | `		if( p1->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|     ! 0 | 1310 | `			continue;` |
|       - | 1311 | `		}` |
|     157 | 1312 | `		pEntry2 = SyHashGet(&pRight->hAttr,SyStringData(&p1->pAttr->sName),SyStringLength(&p1->pAttr->sName));` |
|     157 | 1313 | `		if( pEntry2 == 0 ){` |
|       - | 1314 | `			/* Left has a property the right lacks ⇒ not equal. */` |
|     ! 0 | 1315 | `			return 1;` |
|       - | 1316 | `		}` |
|     157 | 1317 | `		p2 = (VmClassAttr *)pEntry2->pUserData;` |
|     157 | 1318 | `		pL = ExtractClassAttrValue(pLeft->pVm,p1);` |
|     157 | 1319 | `		pR = ExtractClassAttrValue(pRight->pVm,p2);` |
|     157 | 1320 | `		if( pL && pR ){` |
|     157 | 1321 | `			PH7_MemObjLoad(pL,&sV1);` |
|     157 | 1322 | `			PH7_MemObjLoad(pR,&sV2);` |
|       - | 1323 | `			/* Compare the two values now */` |
|     157 | 1324 | `			rc = PH7_MemObjCmp(&sV1,&sV2,bStrict,iNest+1);` |
|     157 | 1325 | `			PH7_MemObjRelease(&sV1);` |
|     157 | 1326 | `			PH7_MemObjRelease(&sV2);` |
|     157 | 1327 | `			if( rc != 0 ){` |
|       - | 1328 | `				/* Not equals */` |
|     133 | 1329 | `				return rc;` |
|       - | 1330 | `			}` |
|      12 | 1331 | `		}` |
|       1 | 1332 | `	}` |
|       - | 1333 | `	/* Object are equals */` |
|      13 | 1334 | `	return 0;` |
|     131 | 1335 | `}` |
|       - | 1336 | `/*` |
|       - | 1337 | ` * Dump a class instance and the store the dump in the BLOB given` |
|       - | 1338 | ` * as the first argument.` |
|       - | 1339 | ` * Note that only non-static/non-constants attribute are dumped.` |
|       - | 1340 | ` * This function is typically invoked when the user issue a call` |
|       - | 1341 | ` * to [var_dump(),var_export(),print_r(),...].` |
|       - | 1342 | ` * This function SXRET_OK on success. Any other return value including` |
|       - | 1343 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|       - | 1344 | ` */` |
|       - | 1345 | `/*` |
|       - | 1346 | `` * Return the `name` property value of an enum case instance (the case name),`` |
|       - | 1347 | ` * or 0 when unavailable. Shared by the var_dump/var_export/json/serialize` |
|       - | 1348 | ` * renderers, which all print enum cases as Class::CaseName forms.` |
|       - | 1349 | ` */` |
|       6 | 1350 | `PH7_PRIVATE ph7_value * PH7_EnumCaseNameValue(ph7_class_instance *pThis)` |
|       1 | 1351 | `{` |
|       - | 1352 | `	SyHashEntry *pEntry;` |
|       7 | 1353 | `	if( (pThis->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|     ! 0 | 1354 | `		return 0;` |
|       - | 1355 | `	}` |
|       7 | 1356 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)"name",sizeof("name")-1);` |
|       7 | 1357 | `	if( pEntry == 0 ){` |
|     ! 0 | 1358 | `		return 0;` |
|       - | 1359 | `	}` |
|       7 | 1360 | `	return PH7_ClassInstanceExtractAttrValue(pThis,(VmClassAttr *)pEntry->pUserData);` |
|       4 | 1361 | `}` |
|       - | 1362 | `/*` |
|       - | 1363 | `` * Return the `value` property value (the backing value) of an enum case`` |
|       - | 1364 | ` * instance, or 0 when unavailable (pure enums have none).` |
|       - | 1365 | ` */` |
|       8 | 1366 | `PH7_PRIVATE ph7_value * PH7_EnumCaseBackingValueOf(ph7_class_instance *pThis)` |
|       1 | 1367 | `{` |
|       - | 1368 | `	SyHashEntry *pEntry;` |
|       9 | 1369 | `	if( (pThis->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|     ! 0 | 1370 | `		return 0;` |
|       - | 1371 | `	}` |
|       9 | 1372 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)"value",sizeof("value")-1);` |
|       9 | 1373 | `	if( pEntry == 0 ){` |
|       3 | 1374 | `		return 0;` |
|       - | 1375 | `	}` |
|       7 | 1376 | `	return PH7_ClassInstanceExtractAttrValue(pThis,(VmClassAttr *)pEntry->pUserData);` |
|       5 | 1377 | `}` |
|       - | 1378 | `/*` |
|       - | 1379 | ` * Emit a class-instance dump header plus its trailing newline. For var_dump` |
|       - | 1380 | ` * (ShowType) it completes the "object(" prefix the caller already emitted as` |
|       - | 1381 | ` *   ClassName)#<id> (<count>) {` |
|       - | 1382 | ` * for print_r it emits the legacy PHL  Object(ClassName) {  (count/id unused).` |
|       - | 1383 | `` * Enum cases print php's `ClassName Enum {` print_r header (var_dump never`` |
|       - | 1384 | `` * reaches here for enums — PH7_MemObjDump prints `enum(S::A)` directly).`` |
|       - | 1385 | ` */` |
|     134 | 1386 | `static void DumpClassInstanceHeader(SyBlob *pOut,ph7_class *pClass,sxu32 nObjId,int ShowType,sxu32 nCount)` |
|       3 | 1387 | `{` |
|     137 | 1388 | `	if( !ShowType && (pClass->iFlags & PH7_CLASS_ENUM) != 0 ){` |
|     ! 0 | 1389 | `		SyBlobFormat(&(*pOut),"%z Enum {",&pClass->sName);` |
|     137 | 1390 | `	}else if( !ShowType ){` |
|       6 | 1391 | `		SyBlobAppend(&(*pOut),"Object(",sizeof("Object(")-1);` |
|       6 | 1392 | `		SyBlobFormat(&(*pOut),"%z) {",&pClass->sName);` |
|       4 | 1393 | `	}else{` |
|     133 | 1394 | `		SyBlobFormat(&(*pOut),"%z)#%u (%u) {",&pClass->sName,nObjId,nCount);` |
|       - | 1395 | `	}` |
|       - | 1396 | `#ifdef __WINNT__` |
|       3 | 1397 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 1398 | `#else` |
|     134 | 1399 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 1400 | `#endif` |
|     137 | 1401 | `}` |
|     138 | 1402 | `PH7_PRIVATE sxi32 PH7_ClassInstanceDump(SyBlob *pOut,ph7_class_instance *pThis,int ShowType,int nTab,int nDepth)` |
|       3 | 1403 | `{` |
|       - | 1404 | `	SyHashEntry *pEntry;` |
|       - | 1405 | `	ph7_value *pValue;` |
|       - | 1406 | `	sxi32 rc;` |
|       - | 1407 | `	int i;` |
|     141 | 1408 | `	if( nDepth > 31 ){` |
|       - | 1409 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|       - | 1410 | `		/* Nesting limit reached..halt immediately*/` |
|       5 | 1411 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|       5 | 1412 | `		if( ShowType ){` |
|       5 | 1413 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|       2 | 1414 | `		}` |
|       5 | 1415 | `		return SXERR_LIMIT;` |
|       - | 1416 | `	}` |
|     137 | 1417 | `	rc = SXRET_OK;` |
|       - | 1418 | `	{` |
|       - | 1419 | `		/* Both var_dump and print_r consult __debugInfo() (PHP behavior);` |
|       - | 1420 | `		 * var_export uses a separate renderer and never reaches here. When the` |
|       - | 1421 | `		 * method is present and returns an array, render that array's entries as` |
|       - | 1422 | `		 * the object body, with the header showing the debug array's count. The` |
|       - | 1423 | `		 * nDepth guard above protects against a __debugInfo returning the object` |
|       - | 1424 | `		 * itself. */` |
|     137 | 1425 | `		ph7_class_method *pDbg = PH7_ClassExtractMethod(pThis->pClass,"__debugInfo",sizeof("__debugInfo")-1);` |
|     137 | 1426 | `		if( pDbg ){` |
|       - | 1427 | `			ph7_value sResult;` |
|       5 | 1428 | `			PH7_MemObjInit(pThis->pVm,&sResult);` |
|       5 | 1429 | `			PH7_VmCallClassMethod(pThis->pVm,pThis,pDbg,&sResult,0,0);` |
|       5 | 1430 | `			if( sResult.iFlags & MEMOBJ_HASHMAP ){` |
|       5 | 1431 | `				ph7_hashmap *pMap = (ph7_hashmap *)sResult.x.pOther;` |
|       - | 1432 | `				/* Header count is the debug array's entry count. */` |
|       5 | 1433 | `				DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,pMap->nEntry);` |
|       5 | 1434 | `				rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth);` |
|       9 | 1435 | `				for( i = 0 ; i < nTab ; i++ ){` |
|       5 | 1436 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       3 | 1437 | `				}` |
|       5 | 1438 | `				SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|       5 | 1439 | `				PH7_MemObjRelease(&sResult);` |
|       5 | 1440 | `				return rc;` |
|       - | 1441 | `			}` |
|       - | 1442 | `			/* Non-array return: behave as if __debugInfo were absent. */` |
|     ! 0 | 1443 | `			PH7_MemObjRelease(&sResult);` |
|     ! 0 | 1444 | `		}` |
|       - | 1445 | `	}` |
|       - | 1446 | `	{` |
|       - | 1447 | `		/* var_dump's header needs the property count up front, so pre-count the` |
|       - | 1448 | `		 * non-static/non-constant attributes (matching the dump loop below). */` |
|     132 | 1449 | `		sxu32 nProp = 0;` |
|     132 | 1450 | `		if( ShowType ){` |
|     130 | 1451 | `			SyHashResetLoopCursor(&pThis->hAttr);` |
|     266 | 1452 | `			while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|     138 | 1453 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     138 | 1454 | `				if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|     134 | 1455 | `					nProp++;` |
|      66 | 1456 | `				}` |
|       2 | 1457 | `			}` |
|      64 | 1458 | `		}` |
|     132 | 1459 | `		DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,nProp);` |
|       - | 1460 | `	}` |
|       - | 1461 | `	/* Dump object attributes */` |
|     132 | 1462 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|     213 | 1463 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|     142 | 1464 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     142 | 1465 | `		if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|       - | 1466 | `			/* Dump non-static/constant attribute only */` |
|    3994 | 1467 | `			for( i = 0 ; i < nTab ; i++ ){` |
|    3858 | 1468 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|    1930 | 1469 | `			}` |
|     138 | 1470 | `			pValue = ExtractClassAttrValue(pThis->pVm,pVmAttr);` |
|     138 | 1471 | `			if( pValue ){` |
|     138 | 1472 | `				SyBlobFormat(&(*pOut),"['%z'] =>",&pVmAttr->pAttr->sName);` |
|       - | 1473 | `#ifdef __WINNT__` |
|       2 | 1474 | `				SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 1475 | `#else` |
|     136 | 1476 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 1477 | `#endif` |
|     138 | 1478 | `				rc = PH7_MemObjDump(&(*pOut),pValue,ShowType,nTab+1,nDepth,0);` |
|     138 | 1479 | `				if( rc == SXERR_LIMIT ){` |
|     125 | 1480 | `					break;` |
|       - | 1481 | `				}` |
|       6 | 1482 | `			}` |
|       6 | 1483 | `		}` |
|       2 | 1484 | `	}` |
|    3982 | 1485 | `	for( i = 0 ; i < nTab ; i++ ){` |
|    3852 | 1486 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|    1927 | 1487 | `	}` |
|     132 | 1488 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|     132 | 1489 | `	return rc;` |
|      72 | 1490 | `}` |
|       - | 1491 | `/*` |
|       - | 1492 | ` * Call a magic method [i.e: __toString(),__toBool(),__Invoke()...]` |
|       - | 1493 | ` * Return SXRET_OK on successfull call. Any other return value indicates failure.` |
|       - | 1494 | ` * Notes on magic methods.` |
|       - | 1495 | ` * According to the PHP language reference manual.` |
|       - | 1496 | ` *  The function names __construct(), __destruct(), __call(), __callStatic()` |
|       - | 1497 | ` *  __get(),  __toString(), __invoke(), __clone() are magical in PHP classes.` |
|       - | 1498 | ` * You cannot have functions with these names in any of your classes unless` |
|       - | 1499 | ` * you want the magic functionality associated with them.` |
|       - | 1500 | ` * Example of magical methods:` |
|       - | 1501 | ` * __toString()` |
|       - | 1502 | ` *  The __toString() method allows a class to decide how it will react when it is treated like` |
|       - | 1503 | ` *  a string. For example, what echo $obj; will print. This method must return a string.` |
|       - | 1504 | ` *  Example #2 Simple example` |
|       - | 1505 | ` * <?php` |
|       - | 1506 | ` * // Declare a simple class` |
|       - | 1507 | ` * class TestClass` |
|       - | 1508 | ` * {` |
|       - | 1509 | ` *   public $foo;` |
|       - | 1510 | ` *` |
|       - | 1511 | ` *   public function __construct($foo)` |
|       - | 1512 | ` *   {` |
|       - | 1513 | ` *       $this->foo = $foo;` |
|       - | 1514 | ` *   }` |
|       - | 1515 | ` *` |
|       - | 1516 | ` *   public function __toString()` |
|       - | 1517 | ` *   {` |
|       - | 1518 | ` *       return $this->foo;` |
|       - | 1519 | ` *   }` |
|       - | 1520 | ` * }` |
|       - | 1521 | ` * $class = new TestClass('Hello');` |
|       - | 1522 | ` * echo $class;` |
|       - | 1523 | ` * ?>` |
|       - | 1524 | ` * The above example will output:` |
|       - | 1525 | ` *  Hello` |
|       - | 1526 | ` *` |
|       - | 1527 | ` * Note that PH7 does not support all the magical method and introudces __toFloat(),__toInt()` |
|       - | 1528 | ` * which have the same behaviour as __toString() but for float and integer types` |
|       - | 1529 | ` * respectively.` |
|       - | 1530 | ` * Refer to the official documentation for more information.` |
|       - | 1531 | ` */` |
|      32 | 1532 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallMagicMethod(` |
|       - | 1533 | `	ph7_vm *pVm,               /* VM that own all this stuff */` |
|       - | 1534 | `	ph7_class *pClass,         /* Target class */` |
|       - | 1535 | `	ph7_class_instance *pThis, /* Target object */` |
|       - | 1536 | `	const char *zMethod,       /* Magic method name [i.e: __toString()]*/` |
|       - | 1537 | `	sxu32 nByte,               /* zMethod length*/` |
|       - | 1538 | `	const SyString *pAttrName, /* Attribute name */` |
|       - | 1539 | `	ph7_value *pResult         /* OUT: magic method return value. NULL to discard */` |
|       - | 1540 | `	)` |
|       1 | 1541 | `{` |
|      33 | 1542 | `	ph7_value *apArg[2] = { 0 , 0 };` |
|       - | 1543 | `	ph7_class_method *pMeth;` |
|       - | 1544 | `	ph7_value sAttr; /* cc warning */` |
|       - | 1545 | `	sxi32 rc;` |
|       - | 1546 | `	int nArg;` |
|       - | 1547 | `	/* Make sure the magic method is available */` |
|      33 | 1548 | `	pMeth = PH7_ClassExtractMethod(&(*pClass),zMethod,nByte);` |
|      33 | 1549 | `	if( pMeth == 0 ){` |
|       - | 1550 | `		/* No such method,return immediately */` |
|     ! 0 | 1551 | `		return SXERR_NOTFOUND;` |
|       - | 1552 | `	}` |
|      33 | 1553 | `	nArg = 0;` |
|       - | 1554 | `	/* Copy arguments */` |
|      33 | 1555 | `	if( pAttrName ){` |
|      33 | 1556 | `		PH7_MemObjInitFromString(pVm,&sAttr,pAttrName);` |
|      33 | 1557 | `		sAttr.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      33 | 1558 | `		apArg[0] = &sAttr;` |
|      33 | 1559 | `		nArg = 1;` |
|      16 | 1560 | `	}` |
|       - | 1561 | `	/* Call the magic method now */` |
|      33 | 1562 | `	rc = PH7_VmCallClassMethod(pVm,&(*pThis),pMeth,pResult,nArg,apArg);` |
|       - | 1563 | `	/* Clean up */` |
|      33 | 1564 | `	if( pAttrName ){` |
|      33 | 1565 | `		PH7_MemObjRelease(&sAttr);` |
|      16 | 1566 | `	}` |
|      33 | 1567 | `	return rc;` |
|      17 | 1568 | `}` |
|       - | 1569 | `/*` |
|       - | 1570 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon].` |
|       - | 1571 | ` * This function is simply a wrapper on ExtractClassAttrValue().` |
|       - | 1572 | ` */` |
|     142 | 1573 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceExtractAttrValue(ph7_class_instance *pThis,VmClassAttr *pAttr)` |
|       3 | 1574 | `{` |
|       - | 1575 | `   /* Extract the attribute value */` |
|       - | 1576 | `	ph7_value *pValue;` |
|     145 | 1577 | `	pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     145 | 1578 | `	return pValue;` |
|       3 | 1579 | `}` |
|       - | 1580 | `/*` |
|       - | 1581 | ` * Convert a class instance [i.e: Object in the PHP jargon] into a hashmap [i.e: array in the PHP jargon].` |
|       - | 1582 | ` * Return SXRET_OK on success. Any other value indicates failure.` |
|       - | 1583 | ` * Note on object conversion to array:` |
|       - | 1584 | ` *  Acccording to the PHP language reference manual` |
|       - | 1585 | ` *  If an object is converted to an array, the result is an array whose elements are the object's properties.` |
|       - | 1586 | ` *  The keys are the member variable names.` |
|       - | 1587 | ` *` |
|       - | 1588 | ` *  The following example:` |
|       - | 1589 | ` *  class Test {` |
|       - | 1590 | ` *   public $A = 25<<1;  // 50` |
|       - | 1591 | ` *	 public $c = rand_str(3);   // Random string of length 3` |
|       - | 1592 | ` *	 public $d = rand() & 1023; // Random number between 0..1023` |
|       - | 1593 | ` *  }` |
|       - | 1594 | ` *  var_dump((array) new Test());` |
|       - | 1595 | ` *	Will output:` |
|       - | 1596 | ` *  array(3) {` |
|       - | 1597 | ` *   [A] =>` |
|       - | 1598 | ` *      int(50)` |
|       - | 1599 | ` *   [c] =>` |
|       - | 1600 | ` *     string(3 'aps')` |
|       - | 1601 | ` *   [d] =>` |
|       - | 1602 | ` *     int(991)` |
|       - | 1603 | ` *  }` |
|       - | 1604 | ` * You have noticed that PH7 allow class attributes [i.e: $a,$c,$d in the example above]` |
|       - | 1605 | ` * have any complex expression (even function calls/Annonymous functions) as their default` |
|       - | 1606 | ` * value unlike the standard PHP engine.` |
|       - | 1607 | ` * This is a very powerful feature that you have to look at.` |
|       - | 1608 | ` */` |
|      12 | 1609 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|       1 | 1610 | `{` |
|       - | 1611 | `	SyHashEntry *pEntry;` |
|       - | 1612 | `	SyString *pAttrName;` |
|       - | 1613 | `	VmClassAttr *pAttr;` |
|       - | 1614 | `	ph7_value *pValue;` |
|       - | 1615 | `	ph7_value sName;` |
|       - | 1616 | `	/* Reset the loop cursor */` |
|      13 | 1617 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      13 | 1618 | `	PH7_MemObjInitFromString(pThis->pVm,&sName,0);` |
|      47 | 1619 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       - | 1620 | `		/* Point to the current attribute */` |
|      29 | 1621 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1622 | `		/* Extract attribute value */` |
|      29 | 1623 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|      29 | 1624 | `		if( pValue ){` |
|       - | 1625 | `			/* Build attribute name */` |
|      29 | 1626 | `			pAttrName = &pAttr->pAttr->sName;` |
|      29 | 1627 | `			PH7_MemObjStringAppend(&sName,pAttrName->zString,pAttrName->nByte);` |
|       - | 1628 | `			/* Perform the insertion */` |
|      29 | 1629 | `			PH7_HashmapInsert(pMap,&sName,pValue);` |
|       - | 1630 | `			/* Reset the string cursor */` |
|      29 | 1631 | `			SyBlobReset(&sName.sBlob);` |
|      14 | 1632 | `		}` |
|       1 | 1633 | `	}` |
|      13 | 1634 | `	PH7_MemObjRelease(&sName);` |
|      13 | 1635 | `	return SXRET_OK;` |
|       1 | 1636 | `}` |
|       - | 1637 | `/*` |
|       - | 1638 | ` * Iterate throw class attributes and invoke the given callback [i.e: xWalk()] for each` |
|       - | 1639 | ` * retrieved attribute.` |
|       - | 1640 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|       - | 1641 | ` * the attribute value in the callback body will not alter the real attribute value.` |
|       - | 1642 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|       - | 1643 | ` * a value different from PH7_OK.` |
|       - | 1644 | ` * Refer to [ph7_object_walk()] for more information.` |
|       - | 1645 | ` */` |
|      40 | 1646 | `PH7_PRIVATE sxi32 PH7_ClassInstanceWalk(` |
|       - | 1647 | `	ph7_class_instance *pThis, /* Target object */` |
|       - | 1648 | `	int (*xWalk)(const char *,ph7_value *,void *), /* Walker callback */` |
|       - | 1649 | `	void *pUserData /* Last argument to xWalk() */` |
|       - | 1650 | `	)` |
|       2 | 1651 | `{` |
|       - | 1652 | `	SyHashEntry *pEntry; /* Hash entry */` |
|       - | 1653 | `	VmClassAttr *pAttr;  /* Pointer to the attribute */` |
|       - | 1654 | `	ph7_value *pValue;   /* Attribute value */` |
|       - | 1655 | `	ph7_value sValue;    /* Copy of the attribute value */` |
|       - | 1656 | `	int rc;` |
|       - | 1657 | `	/* Reset the loop cursor */` |
|      42 | 1658 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      42 | 1659 | `	PH7_MemObjInit(pThis->pVm,&sValue);` |
|       - | 1660 | `	/* Start the walk process */` |
|     124 | 1661 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       - | 1662 | `		/* Point to the current attribute */` |
|      64 | 1663 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1664 | `		/* Extract attribute value */` |
|      64 | 1665 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|      64 | 1666 | `		if( pValue ){` |
|      64 | 1667 | `			PH7_MemObjLoad(pValue,&sValue);` |
|       - | 1668 | `			/* Invoke the supplied callback */` |
|      64 | 1669 | `			rc =  xWalk(SyStringData(&pAttr->pAttr->sName),&sValue,pUserData);` |
|      64 | 1670 | `			PH7_MemObjRelease(&sValue);` |
|      64 | 1671 | `			if( rc != PH7_OK){` |
|       - | 1672 | `				/* User callback request an operation abort */` |
|     ! 0 | 1673 | `				return SXERR_ABORT;` |
|       - | 1674 | `			}` |
|      31 | 1675 | `		}` |
|       2 | 1676 | `	}` |
|       - | 1677 | `	/* All done */` |
|      42 | 1678 | `	return SXRET_OK;` |
|      22 | 1679 | `}` |
|       - | 1680 | `/*` |
|       - | 1681 | ` * Extract a class atrribute value.` |
|       - | 1682 | ` * Return a pointer to the attribute value on success. Otherwise NULL.` |
|       - | 1683 | ` * Note:` |
|       - | 1684 | ` *  Access to static and constant attribute is not allowed. That is,the function` |
|       - | 1685 | ` *  will return NULL in case someone (host-application code) try to extract` |
|       - | 1686 | ` *  a static/constant attribute.` |
|       - | 1687 | ` */` |
|    9570 | 1688 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceFetchAttr(ph7_class_instance *pThis,const SyString *pName)` |
|       5 | 1689 | `{` |
|       - | 1690 | `	SyHashEntry *pEntry;` |
|       - | 1691 | `	VmClassAttr *pAttr;` |
|       - | 1692 | `	/* Query the attribute hashtable */` |
|    9575 | 1693 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);` |
|    9575 | 1694 | `	if( pEntry == 0 ){` |
|       - | 1695 | `		/* No such attribute */` |
|     ! 0 | 1696 | `		return 0;` |
|       - | 1697 | `	}` |
|       - | 1698 | `	/* Point to the class atrribute */` |
|    9575 | 1699 | `	pAttr = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1700 | `	/* Check if we are dealing with a static/constant attribute */` |
|    9575 | 1701 | `	if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|       - | 1702 | `		/* Access is forbidden */` |
|     ! 0 | 1703 | `		return 0;` |
|       - | 1704 | `	}` |
|       - | 1705 | `	/* Return the attribute value */` |
|    9575 | 1706 | `	return ExtractClassAttrValue(pThis->pVm,pAttr);` |
|    4790 | 1707 | `}` |
|       - | 1708 |  |
