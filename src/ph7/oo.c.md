# src/ph7/oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 691/785 lines (88.03%)

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
|  430354 |   14 | `PH7_PRIVATE ph7_class * PH7_NewRawClass(ph7_vm *pVm,const SyString *pName,sxu32 nLine)` |
|       5 |   15 | `{` |
|       - |   16 | `	ph7_class *pClass;` |
|       - |   17 | `	char *zName;` |
|       - |   18 | `	/* Allocate a new instance */` |
|  430359 |   19 | `	pClass = (ph7_class *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class));` |
|  430359 |   20 | `	if( pClass == 0 ){` |
|     ! 0 |   21 | `		return 0;` |
|       - |   22 | `	}` |
|       - |   23 | `	/* Zero the structure */` |
|  430359 |   24 | `	SyZero(pClass,sizeof(ph7_class));` |
|       - |   25 | `	/* Duplicate class name */` |
|  430359 |   26 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  430359 |   27 | `	if( zName == 0 ){` |
|     ! 0 |   28 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClass);` |
|     ! 0 |   29 | `		return 0;` |
|       - |   30 | `	}` |
|       - |   31 | `	/* Initialize fields */` |
|  430359 |   32 | `	SyStringInitFromBuf(&pClass->sName,zName,pName->nByte);` |
|  430359 |   33 | `	SyHashInit(&pClass->hMethod,&pVm->sAllocator,0,0);` |
|  430359 |   34 | `	SyHashInit(&pClass->hAttr,&pVm->sAllocator,0,0);` |
|  430359 |   35 | `	SyHashInit(&pClass->hDerived,&pVm->sAllocator,0,0);` |
|  430359 |   36 | `	SySetInit(&pClass->aInterface,&pVm->sAllocator,sizeof(ph7_class *));` |
|  430359 |   37 | `	SySetInit(&pClass->aTrait,&pVm->sAllocator,sizeof(ph7_class *));` |
|  430359 |   38 | `	SySetInit(&pClass->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|  430359 |   39 | `	SySetInit(&pClass->aEnumCases,&pVm->sAllocator,sizeof(ph7_class_attr *));` |
|  430359 |   40 | `	pClass->nLine = nLine;` |
|  430359 |   41 | `	if( pVm->bCompilingBuiltin ){` |
|       - |   42 | `		/* Defined by an embedded builtin chunk: internal, no defining file.` |
|       - |   43 | `		 * Class compilers merge further flags with \|= so this survives. */` |
|  428293 |   44 | `		pClass->iFlags \|= PH7_CLASS_INTERNAL;` |
|  214149 |   45 | `	}else{` |
|       - |   46 | `		/* Alias the VM-lifetime path dup on top of the include stack */` |
|    2071 |   47 | `		SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|    2071 |   48 | `		if( pFile ){` |
|    2071 |   49 | `			SyStringDupPtr(&pClass->sFile,pFile);` |
|    1033 |   50 | `		}` |
|       - |   51 | `	}` |
|       - |   52 | `	/* All done */` |
|  430359 |   53 | `	return pClass;` |
|  215182 |   54 | `}` |
|       - |   55 | `/*` |
|       - |   56 | ` * Allocate and initialize a new class attribute.` |
|       - |   57 | ` * Return a pointer to the class attribute on success. NULL otherwise.` |
|       - |   58 | ` */` |
|  766138 |   59 | `PH7_PRIVATE ph7_class_attr * PH7_NewClassAttr(ph7_vm *pVm,const SyString *pName,sxu32 nLine,sxi32 iProtection,sxi32 iFlags)` |
|       5 |   60 | `{` |
|       - |   61 | `	ph7_class_attr *pAttr;` |
|       - |   62 | `	char *zName;` |
|  766143 |   63 | `	pAttr = (ph7_class_attr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_attr));` |
|  766143 |   64 | `	if( pAttr == 0 ){` |
|     ! 0 |   65 | `		return 0;` |
|       - |   66 | `	}` |
|       - |   67 | `	/* Zero the structure */` |
|  766143 |   68 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|  766143 |   69 | `	SySetInit(&pAttr->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|       - |   70 | `	/* Duplicate attribute name */` |
|  766143 |   71 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  766143 |   72 | `	if( zName == 0 ){` |
|     ! 0 |   73 | `		SyMemBackendPoolFree(&pVm->sAllocator,pAttr);` |
|     ! 0 |   74 | `		return 0;` |
|       - |   75 | `	}` |
|       - |   76 | `	/* Initialize fields */` |
|  766143 |   77 | `	SySetInit(&pAttr->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|  766143 |   78 | `	SySetInit(&pAttr->aUnionAlts,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|  766143 |   79 | `	SyStringInitFromBuf(&pAttr->sName,zName,pName->nByte);` |
|  766143 |   80 | `	pAttr->iProtection = iProtection;` |
|  766143 |   81 | `	pAttr->nIdx = SXU32_HIGH;` |
|  766143 |   82 | `	pAttr->iFlags = iFlags;` |
|  766143 |   83 | `	pAttr->nLine = nLine;` |
|  766143 |   84 | `	return pAttr;` |
|  383074 |   85 | `}` |
|       - |   86 | `/*` |
|       - |   87 | ` * Allocate and initialize a new class method.` |
|       - |   88 | ` * Return a pointer to the class method on success. NULL otherwise` |
|       - |   89 | ` * This function associate with the newly created method an automatically generated` |
|       - |   90 | ` * random unique name.` |
|       - |   91 | ` */` |
| 2399540 |   92 | `PH7_PRIVATE ph7_class_method * PH7_NewClassMethod(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,sxu32 nLine,` |
|       - |   93 | `	sxi32 iProtection,sxi32 iFlags,sxi32 iFuncFlags)` |
|       5 |   94 | `{` |
|       - |   95 | `	ph7_class_method *pMeth;` |
|       - |   96 | `	SyHashEntry *pEntry;` |
|       - |   97 | `	SyString *pNamePtr;` |
|       - |   98 | `	char zSalt[10];` |
|       - |   99 | `	char *zName;` |
|       - |  100 | `	sxu32 nByte;` |
|       - |  101 | `	/* Allocate a new class method instance */` |
| 2399545 |  102 | `	pMeth = (ph7_class_method *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_method));` |
| 2399545 |  103 | `	if( pMeth == 0 ){` |
|     ! 0 |  104 | `		return 0;` |
|       - |  105 | `	}` |
|       - |  106 | `	/* Zero the structure */` |
| 2399545 |  107 | `	SyZero(pMeth,sizeof(ph7_class_method));` |
|       - |  108 | `	/* Check for an already installed method with the same name */` |
| 2399545 |  109 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
| 2399545 |  110 | `	if( pEntry == 0 ){` |
|       - |  111 | `		/* Associate an unique VM name to this method */` |
| 2399543 |  112 | `		nByte = sizeof(zSalt) + pName->nByte + SyStringLength(&pClass->sName)+sizeof(char)*7/*[[__'\0'*/;` |
| 2399543 |  113 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,nByte);` |
| 2399543 |  114 | `		if( zName == 0 ){` |
|     ! 0 |  115 | `			SyMemBackendPoolFree(&pVm->sAllocator,pMeth);` |
|     ! 0 |  116 | `			return 0;` |
|       - |  117 | `		}` |
| 2399543 |  118 | `		pNamePtr = &pMeth->sVmName;` |
|       - |  119 | `		/* Generate a random string */` |
| 2399543 |  120 | `		PH7_VmRandomString(&(*pVm),zSalt,sizeof(zSalt));` |
| 2399543 |  121 | `		pNamePtr->nByte = SyBufferFormat(zName,nByte,"[__%z@%z_%.*s]",&pClass->sName,pName,sizeof(zSalt),zSalt);` |
| 2399543 |  122 | `		pNamePtr->zString = zName;` |
| 1199774 |  123 | `	}else{` |
|       - |  124 | `		/* Method is condidate for 'overloading' */` |
|       3 |  125 | `		ph7_class_method *pCurrent = (ph7_class_method *)pEntry->pUserData;` |
|       3 |  126 | `		pNamePtr = &pMeth->sVmName;` |
|       - |  127 | `		/* Use the same VM name */` |
|       3 |  128 | `		SyStringDupPtr(pNamePtr,&pCurrent->sVmName);` |
|       3 |  129 | `		zName = (char *)pNamePtr->zString;` |
|       - |  130 | `	}` |
| 2399545 |  131 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|  145382 |  132 | `		if( (pName->nByte == sizeof("__destruct") - 1 && SyMemcmp(pName->zString,"__destruct",sizeof("__destruct") - 1 ) == 0)` |
|  145387 |  133 | `			\|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|       - |  134 | `				/* Switch to public visibility for destructors and legacy class-name` |
|       - |  135 | `				 * constructors (the engine invokes destructors internally, bypassing` |
|       - |  136 | `				 * visibility either way). __construct KEEPS its declared visibility` |
|       - |  137 | ``				 * (band A #4): php enforces it at `new` — a private/protected ctor`` |
|       - |  138 | `				 * from the wrong scope is a catchable Error, checked at OP_NEW —` |
|       - |  139 | `				 * and ReflectionClass::isInstantiable()/newInstance() now see it. */` |
|     ! 0 |  140 | `				iProtection = PH7_CLASS_PROT_PUBLIC;` |
|     ! 0 |  141 | `		}` |
|   72691 |  142 | `	}` |
|       - |  143 | `	/* Initialize method fields */` |
| 2399545 |  144 | `	pMeth->iProtection = iProtection;` |
| 2399545 |  145 | `	pMeth->iFlags = iFlags;` |
| 2399545 |  146 | `	pMeth->nLine = nLine;` |
| 3599315 |  147 | `	PH7_VmInitFuncState(&(*pVm),&pMeth->sFunc,&zName[sizeof(char)*4/*[__@*/+SyStringLength(&pClass->sName)],` |
| 2399540 |  148 | `		pName->nByte,iFuncFlags\|VM_FUNC_CLASS_METHOD,pClass);` |
| 2399545 |  149 | `	return pMeth;` |
| 1199775 |  150 | `}` |
|       - |  151 | `/*` |
|       - |  152 | ` * Check if the given name have a class method associated with it.` |
|       - |  153 | ` * Return the desired method [i.e: ph7_class_method instance] on success. NULL otherwise.` |
|       - |  154 | ` */` |
|  725028 |  155 | `PH7_PRIVATE ph7_class_method * PH7_ClassExtractMethod(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|       5 |  156 | `{` |
|       - |  157 | `	SyHashEntry *pEntry;` |
|       - |  158 | `	/* Perform a hash lookup */` |
|  725033 |  159 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)zName,nByte);` |
|  725033 |  160 | `	if( pEntry == 0 ){` |
|       - |  161 | `		/* No such entry */` |
|    9229 |  162 | `		return 0;` |
|       - |  163 | `	}` |
|       - |  164 | `	/* Point to the desired method */` |
|  715809 |  165 | `	return (ph7_class_method *)pEntry->pUserData;` |
|  362519 |  166 | `}` |
|       - |  167 | `/*` |
|       - |  168 | ` * Check if the given name is a class attribute.` |
|       - |  169 | ` * Return the desired attribute [i.e: ph7_class_attr instance] on success.NULL otherwise.` |
|       - |  170 | ` */` |
|  461396 |  171 | `PH7_PRIVATE ph7_class_attr * PH7_ClassExtractAttribute(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|       5 |  172 | `{` |
|       - |  173 | `	SyHashEntry *pEntry;` |
|       - |  174 | `	/* Perform a hash lookup */` |
|  461401 |  175 | `	pEntry = SyHashGet(&pClass->hAttr,(const void *)zName,nByte);` |
|  461401 |  176 | `	if( pEntry == 0 ){` |
|       - |  177 | `		/* No such entry */` |
|  460059 |  178 | `		return 0;` |
|       - |  179 | `	}` |
|       - |  180 | `	/* Point to the desierd method */` |
|    1347 |  181 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|  230703 |  182 | `}` |
|       - |  183 | `/*` |
|       - |  184 | ` * Install a class attribute in the corresponding container.` |
|       - |  185 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|       - |  186 | ` */` |
|  766134 |  187 | `PH7_PRIVATE sxi32 PH7_ClassInstallAttr(ph7_class *pClass,ph7_class_attr *pAttr)` |
|       5 |  188 | `{` |
|  766139 |  189 | `	SyString *pName = &pAttr->sName;` |
|       - |  190 | `	sxi32 rc;` |
|       - |  191 | `	/* Remember where this attribute was originally declared so that later` |
|       - |  192 | `	 * inheritance/trait copies still know the declaring class (needed for` |
|       - |  193 | `	 * PHP-compatible error messages on typed properties). */` |
|  766139 |  194 | `	if( pAttr->pDeclClass == 0 ){` |
|  766139 |  195 | `		pAttr->pDeclClass = pClass;` |
|  383067 |  196 | `	}` |
|  766139 |  197 | `	rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|  766139 |  198 | `	return rc;` |
|       5 |  199 | `}` |
|       - |  200 | `/*` |
|       - |  201 | ` * Install a class method in the corresponding container.` |
|       - |  202 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|       - |  203 | ` */` |
| 2399528 |  204 | `PH7_PRIVATE sxi32 PH7_ClassInstallMethod(ph7_class *pClass,ph7_class_method *pMeth)` |
|       5 |  205 | `{` |
| 2399533 |  206 | `	SyString *pName = &pMeth->sFunc.sName;` |
|       - |  207 | `	sxi32 rc;` |
| 2399533 |  208 | `	rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 2399533 |  209 | `	return rc;` |
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
|  153168 |  234 | `static int OoClassifyOverrideType(ph7_vm *pVm, sxu32 nType, const SyString *pClass,` |
|       - |  235 | `	int bUnion, ph7_class **ppClass)` |
|       5 |  236 | `{` |
|  153173 |  237 | `	*ppClass = 0;` |
|  153173 |  238 | `	if( bUnion ){` |
|       3 |  239 | `		return OVT_SKIP; /* union/intersection — full lattice, skip */` |
|       - |  240 | `	}` |
|  153171 |  241 | `	if( nType == 0 ){` |
|  153077 |  242 | `		return OVT_NONE; /* no declared type */` |
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
|   76589 |  274 | `}` |
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
|  122516 |  288 | `static OvType OoTypeFromReturn(ph7_vm_func *pF)` |
|       5 |  289 | `{` |
|       - |  290 | `	OvType t;` |
|  122521 |  291 | `	t.nType = pF->nReturnType;` |
|  122521 |  292 | `	t.pClass = &pF->sReturnClass;` |
|  122521 |  293 | `	t.bUnion = SySetUsed(&pF->aReturnUnion) > 0;` |
|  122521 |  294 | `	t.bNullable = (pF->iFlags & VM_FUNC_RETURN_NULLABLE) != 0;` |
|  122521 |  295 | `	return t;` |
|       5 |  296 | `}` |
|   30652 |  297 | `static OvType OoTypeFromArg(ph7_vm_func_arg *pA)` |
|       5 |  298 | `{` |
|       - |  299 | `	OvType t;` |
|   30657 |  300 | `	t.nType = pA->nType;` |
|   30657 |  301 | `	t.pClass = &pA->sClass;` |
|   30657 |  302 | `	t.bUnion = (pA->iFlags & VM_FUNC_ARG_UNION) != 0;` |
|   30657 |  303 | `	t.bNullable = (pA->iFlags & VM_FUNC_ARG_NULLABLE) != 0;` |
|   30657 |  304 | `	return t;` |
|       5 |  305 | `}` |
|       - |  306 | `/*` |
|       - |  307 | ` * Return TRUE if the child type is an unambiguously-invalid override of the` |
|       - |  308 | ` * parent type. bCovariant=1 for a return type (child must be ⊆ parent),` |
|       - |  309 | ` * 0 for a parameter (child must be ⊇ parent). Returns FALSE (accept) on any` |
|       - |  310 | ` * skipped/ambiguous shape.` |
|       - |  311 | ` */` |
|   76584 |  312 | `static int OoOverrideTypeBad(ph7_vm *pVm, OvType parent, OvType child, int bCovariant)` |
|       5 |  313 | `{` |
|       - |  314 | `	ph7_class *pParentCls, *pChildCls;` |
|   76589 |  315 | `	int kP = OoClassifyOverrideType(pVm, parent.nType, parent.pClass, parent.bUnion, &pParentCls);` |
|   76589 |  316 | `	int kC = OoClassifyOverrideType(pVm, child.nType, child.pClass, child.bUnion, &pChildCls);` |
|   76589 |  317 | `	if( kP == OVT_SKIP \|\| kC == OVT_SKIP ){` |
|      18 |  318 | `		return 0; /* ambiguous shape — conservatively accept */` |
|       - |  319 | `	}` |
|       - |  320 | `	/* A missing type is the TOP type. covariant (return): a concrete child is a` |
|       - |  321 | `	 * subtype of top, fine; a top child over a concrete parent WIDENS → bad.` |
|       - |  322 | `	 * contravariant (param): a top child is a supertype of anything, fine; a` |
|       - |  323 | `	 * concrete child over a top parent NARROWS → bad. (A union/intersection child` |
|       - |  324 | `	 * already fell into OVT_SKIP above, so a flagged child here is scalar/class.) */` |
|   76573 |  325 | `	if( kP == OVT_NONE \|\| kC == OVT_NONE ){` |
|   76545 |  326 | `		if( bCovariant && kC == OVT_NONE && kP != OVT_NONE ) return 1;` |
|   76545 |  327 | `		if( !bCovariant && kP == OVT_NONE && kC != OVT_NONE ) return 1;` |
|   76545 |  328 | `		return 0;` |
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
|   38297 |  349 | `}` |
|       - |  350 |  |
|       - |  351 | `/*` |
|       - |  352 | ` * Check a child method's signature against the parent method it overrides.` |
|       - |  353 | ` * Emits a PHP-style "Declaration of … must be compatible …" fatal on a clear` |
|       - |  354 | `` * incompatibility. `__construct` is exempt (PHP does not apply variance to it).`` |
|       - |  355 | ` */` |
|   88034 |  356 | `static sxi32 OoCheckOverrideCompat(ph7_gen_state *pGen, ph7_class *pBase, ph7_class *pSub,` |
|       - |  357 | `	ph7_class_method *pParent, ph7_class_method *pChild)` |
|       5 |  358 | `{` |
|   88039 |  359 | `	ph7_vm *pVm = pGen->pVm;` |
|   88039 |  360 | `	ph7_vm_func *pPF = &pParent->sFunc;` |
|   88039 |  361 | `	ph7_vm_func *pCF = &pChild->sFunc;` |
|   88039 |  362 | `	SyString *pMName = &pCF->sName;` |
|       - |  363 | `	ph7_vm_func_arg *aP, *aC;` |
|       - |  364 | `	sxu32 nPArg, nCArg, k;` |
|   88039 |  365 | `	int bBad = 0;` |
|   88034 |  366 | `	if( pMName->nByte == sizeof("__construct")-1` |
|   57414 |  367 | `	 && SyStrnmicmp(pMName->zString,"__construct",pMName->nByte) == 0 ){` |
|   26781 |  368 | `		return SXRET_OK;` |
|       - |  369 | `	}` |
|       - |  370 | `	/* Return type — covariant. */` |
|   61263 |  371 | `	bBad = OoOverrideTypeBad(pVm, OoTypeFromReturn(pPF), OoTypeFromReturn(pCF), /* bCovariant */ 1);` |
|       - |  372 | `	/* Each overlapping parameter — contravariant. */` |
|   61263 |  373 | `	nPArg = SySetUsed(&pPF->aArgs);` |
|   61263 |  374 | `	nCArg = SySetUsed(&pCF->aArgs);` |
|   61263 |  375 | `	aP = (ph7_vm_func_arg *)SySetBasePtr(&pPF->aArgs);` |
|   61263 |  376 | `	aC = (ph7_vm_func_arg *)SySetBasePtr(&pCF->aArgs);` |
|   76589 |  377 | `	for( k = 0; !bBad && k < nPArg && k < nCArg; k++ ){` |
|   15331 |  378 | `		bBad = OoOverrideTypeBad(pVm, OoTypeFromArg(&aP[k]), OoTypeFromArg(&aC[k]), /* bCovariant */ 0);` |
|    7668 |  379 | `	}` |
|       - |  380 | `	/* Parameter arity: the child must declare at least the parent's parameters and` |
|       - |  381 | `	 * may add only OPTIONAL ones — PHP rejects dropping any param (even an optional` |
|       - |  382 | `	 * one) or adding a required one. Skip the rule if either signature is variadic` |
|       - |  383 | `	 * (arity semantics differ). */` |
|   61263 |  384 | `	if( !bBad ){` |
|   61259 |  385 | `		int bVariadic = 0;` |
|   76583 |  386 | `		for( k = 0; k < nPArg; k++ ){ if( aP[k].iFlags & VM_FUNC_ARG_VARIADIC ) bVariadic = 1; }` |
|   76585 |  387 | `		for( k = 0; k < nCArg; k++ ){ if( aC[k].iFlags & VM_FUNC_ARG_VARIADIC ) bVariadic = 1; }` |
|   61259 |  388 | `		if( !bVariadic ){` |
|   61259 |  389 | `			if( nCArg < nPArg ){` |
|     ! 0 |  390 | `				bBad = 1; /* dropped a parent parameter */` |
|     ! 0 |  391 | `			}else{` |
|   61261 |  392 | `				for( k = nPArg; k < nCArg; k++ ){` |
|       3 |  393 | `					if( SySetUsed(&aC[k].aByteCode) == 0 ){ bBad = 1; break; } /* new required */` |
|       2 |  394 | `				}` |
|       - |  395 | `			}` |
|   30627 |  396 | `		}` |
|   30627 |  397 | `	}` |
|   61263 |  398 | `	if( bBad ){` |
|       8 |  399 | `		sxi32 rc = PH7_GenCompileError(&(*pGen),E_ERROR,pChild->nLine,` |
|       - |  400 | `			"Declaration of %z::%z() must be compatible with %z::%z()",` |
|       2 |  401 | `			&pSub->sName,pMName,&pBase->sName,&pParent->sFunc.sName);` |
|       6 |  402 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  403 | `			return SXERR_ABORT;` |
|       - |  404 | `		}` |
|       2 |  405 | `	}` |
|   61263 |  406 | `	return SXRET_OK;` |
|   44022 |  407 | `}` |
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
|  183822 |  449 | `PH7_PRIVATE sxi32 PH7_ClassInherit(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase)` |
|       5 |  450 | `{` |
|       - |  451 | `	ph7_class_method *pMeth;` |
|       - |  452 | `	ph7_class_attr *pAttr;` |
|       - |  453 | `	SyHashEntry *pEntry;` |
|       - |  454 | `	SyString *pName;` |
|       - |  455 | `	sxi32 rc;` |
|       - |  456 | `	/* Install in the derived hashtable */` |
|  183827 |  457 | `	rc = SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|  183827 |  458 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  459 | `		return rc;` |
|       - |  460 | `	}` |
|       - |  461 | `	/* readonly class inheritance (PHP 8.2): a readonly class may only extend a` |
|       - |  462 | `	 * readonly class, and a non-readonly class may not extend a readonly one. */` |
|  183827 |  463 | `	if( (pBase->iFlags & PH7_CLASS_READONLY) != (pSub->iFlags & PH7_CLASS_READONLY) ){` |
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
|  183827 |  478 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
| 1071505 |  479 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|       - |  480 | `		/* Make sure the private attributes are not redeclared in the subclass */` |
|  887683 |  481 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  887683 |  482 | `		pName = &pAttr->sName;` |
|  887683 |  483 | `		if( (pEntry = SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte)) != 0 ){` |
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
|       - |  497 | `			/* A child MAY redeclare a base's private property: php treats the two` |
|       - |  498 | `			 * as independent members (each private to its declaring class), with no` |
|       - |  499 | `			 * diagnostic. PH7 warned here, which is wrong — the child's entry simply` |
|       - |  500 | `			 * shadows the base's in the by-name attribute table. */` |
|      19 |  501 | `			continue;` |
|       - |  502 | `		}` |
|       - |  503 | `		/* Install the attribute. php: a base class's private INSTANCE property` |
|       - |  504 | `		 * lives on every child instance too (its own methods read/write it` |
|       - |  505 | `		 * through $this on the child; the access check grants private access by` |
|       - |  506 | `		 * DECLARING class, so child methods and outsiders still can't touch it).` |
|       - |  507 | `		 * Private STATICS/CONSTANTS stay uncopied — base methods reach those` |
|       - |  508 | `		 * through self:: against the declaring class directly. */` |
|  887662 |  509 | `		if( pAttr->iProtection != PH7_CLASS_PROT_PRIVATE` |
|  474438 |  510 | `		 \|\| (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|  887661 |  511 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|  887661 |  512 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  513 | `				return rc;` |
|       - |  514 | `			}` |
|  443828 |  515 | `		}` |
|       5 |  516 | `	}` |
|  183827 |  517 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
| 2632131 |  518 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|       - |  519 | `		/* Make sure the private/final methods are not redeclared in the subclass */` |
| 2448309 |  520 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
| 2448309 |  521 | `		pName = &pMeth->sFunc.sName;` |
| 2448309 |  522 | `		if( (pEntry = SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|   88043 |  523 | `			 if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){` |
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
|  132056 |  534 | `				rc = OoCheckOverrideCompat(&(*pGen),pBase,pSub,pMeth,` |
|   88034 |  535 | `					(ph7_class_method *)pEntry->pUserData);` |
|   88039 |  536 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  537 | `					return SXERR_ABORT;` |
|       - |  538 | `				}` |
|       - |  539 | `			}` |
|   88043 |  540 | `			continue;` |
|       - |  541 | `		}` |
|       - |  542 | `		/* Install the method. php: a base class's private INSTANCE method is` |
|       - |  543 | `		 * dispatchable on child instances too — an inherited public method` |
|       - |  544 | `		 * calling $this->priv() must find it (the call-site visibility check` |
|       - |  545 | `		 * binds by DECLARING class, sFunc.pUserData, so child code and` |
|       - |  546 | `		 * outsiders still can't call it; a private ctor copied down also` |
|       - |  547 | ``		 * blocks `new Child` from outside like php). Private STATICS stay`` |
|       - |  548 | `		 * uncopied — base methods reach those through self:: against the` |
|       - |  549 | `		 * declaring class directly. */` |
| 2360266 |  550 | `		if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE` |
| 1193530 |  551 | `		 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
| 2360271 |  552 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 2360271 |  553 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  554 | `				return rc;` |
|       - |  555 | `			}` |
| 1180133 |  556 | `		}` |
|       5 |  557 | `	}` |
|       - |  558 | `	/* Mark as subclass */` |
|  183827 |  559 | `	pSub->pBase = pBase;` |
|       - |  560 | `	/* All done */` |
|  183827 |  561 | `	return SXRET_OK;` |
|   91916 |  562 | `}` |
|       - |  563 | `/*` |
|       - |  564 | ` * Apply a trait to a class: copy all methods and attributes from the trait` |
|       - |  565 | ` * into the target class. Unlike inheritance, traits copy ALL members including` |
|       - |  566 | ` * private ones. Members already defined in the class take precedence.` |
|       - |  567 | ` */` |
|   15364 |  568 | `PH7_PRIVATE sxi32 PH7_ClassUseTrait(ph7_gen_state *pGen,ph7_class *pClass,ph7_class *pTrait)` |
|       5 |  569 | `{` |
|       - |  570 | `	ph7_class_method *pMeth;` |
|       - |  571 | `	ph7_class_attr *pAttr;` |
|       - |  572 | `	SyHashEntry *pEntry;` |
|       - |  573 | `	SyString *pName;` |
|       - |  574 | `	sxi32 rc;` |
|       - |  575 | `	/* Detect cyclic trait composition (e.g. trait A { use B; } trait B { use A; }) */` |
|   15369 |  576 | `	if( pTrait->iFlags & PH7_CLASS_TRAIT_VISITING ){` |
|     ! 0 |  577 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|     ! 0 |  578 | `			"Trait circular reference detected: %z is already being applied",&pTrait->sName);` |
|     ! 0 |  579 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  580 | `			return SXERR_ABORT;` |
|       - |  581 | `		}` |
|     ! 0 |  582 | `		return SXRET_OK;` |
|       - |  583 | `	}` |
|   15369 |  584 | `	pTrait->iFlags \|= PH7_CLASS_TRAIT_VISITING;` |
|   15369 |  585 | `	rc = SXRET_OK;` |
|       - |  586 | `	/* Copy attributes from the trait */` |
|   15369 |  587 | `	SyHashResetLoopCursor(&pTrait->hAttr);` |
|   61277 |  588 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hAttr)) != 0 ){` |
|       - |  589 | `		SyHashEntry *pExisting;` |
|   45913 |  590 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   45913 |  591 | `		pName = &pAttr->sName;` |
|   45913 |  592 | `		pExisting = SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte);` |
|   45913 |  593 | `		if( pExisting != 0 ){` |
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
|   45909 |  624 | `		rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|   45909 |  625 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  626 | `			goto cleanup;` |
|       - |  627 | `		}` |
|       5 |  628 | `	}` |
|       - |  629 | `	/* Copy methods from the trait */` |
|   15369 |  630 | `	SyHashResetLoopCursor(&pTrait->hMethod);` |
|  221933 |  631 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hMethod)) != 0 ){` |
|  206569 |  632 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  206569 |  633 | `		pName = &pMeth->sFunc.sName;` |
|  206569 |  634 | `		if( SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte) != 0 ){` |
|       - |  635 | `			/* Method already exists in the class. Check if it came from another trait` |
|       - |  636 | `			 * (unresolved conflict) vs being defined by the class itself.` |
|       - |  637 | `			 */` |
|       - |  638 | `			ph7_class **apUsedTraits;` |
|       - |  639 | `			sxu32 nUsed,k;` |
|      11 |  640 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|      11 |  641 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|      11 |  642 | `			for(k = 0; k < nUsed; k++){` |
|       3 |  643 | `				if( PH7_ClassExtractMethod(apUsedTraits[k],pName->zString,pName->nByte) != 0 ){` |
|       - |  644 | `					/* Two different traits define the same method with no resolution */` |
|       4 |  645 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|       - |  646 | `						"Trait method %z::%z has not been applied as %z::%z, "` |
|       - |  647 | `						"because of collision with %z::%z",` |
|       2 |  648 | `						&pTrait->sName,pName,` |
|       1 |  649 | `						&pClass->sName,pName,` |
|       2 |  650 | `						&apUsedTraits[k]->sName,pName);` |
|       3 |  651 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  652 | `						goto cleanup;` |
|       - |  653 | `					}` |
|       3 |  654 | `					break;` |
|       - |  655 | `				}` |
|     ! 0 |  656 | `			}` |
|       - |  657 | `			/* Class-defined method takes precedence */` |
|      11 |  658 | `			continue;` |
|       - |  659 | `		}` |
|  206561 |  660 | `		rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|  206561 |  661 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  662 | `			goto cleanup;` |
|       - |  663 | `		}` |
|       5 |  664 | `	}` |
|       - |  665 | `	/* Record trait in the class */` |
|   15369 |  666 | `	SySetPut(&pClass->aTrait,(const void *)&pTrait);` |
|    7682 |  667 | `cleanup:` |
|       - |  668 | `	/* Always clear visiting flag, even on error paths */` |
|   15369 |  669 | `	pTrait->iFlags &= ~PH7_CLASS_TRAIT_VISITING;` |
|    7682 |  670 | `	SXUNUSED(pGen);` |
|   15369 |  671 | `	return rc;` |
|    7687 |  672 | `}` |
|       - |  673 | `/*` |
|       - |  674 | ` * Inherit an object interface from another object interface.` |
|       - |  675 | ` * According to the PHP language reference manual.` |
|       - |  676 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|       - |  677 | ` *  must implement, without having to define how these methods are handled.` |
|       - |  678 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  679 | ` *  class, but without any of the methods having their contents defined.` |
|       - |  680 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  681 | ` *` |
|       - |  682 | ` * This function return SXRET_OK if the interface inheritance operation was successfully performed.` |
|       - |  683 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|       - |  684 | ` * error message.` |
|       - |  685 | ` */` |
|   26780 |  686 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceInherit(ph7_class *pSub,ph7_class *pBase)` |
|       5 |  687 | `{` |
|       - |  688 | `	ph7_class_method *pMeth;` |
|       - |  689 | `	ph7_class_attr *pAttr;` |
|       - |  690 | `	SyHashEntry *pEntry;` |
|       - |  691 | `	SyString *pName;` |
|       - |  692 | `	sxi32 rc;` |
|       - |  693 | `	/* Install in the derived hashtable */` |
|   26785 |  694 | `	SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|   26785 |  695 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
|       - |  696 | `	/* Copy constants */` |
|   40177 |  697 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|       - |  698 | `		/* Make sure the constants are not redeclared in the subclass */` |
|       3 |  699 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|       3 |  700 | `		pName = &pAttr->sName;` |
|       3 |  701 | `		if( SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte) == 0 ){` |
|       - |  702 | `			/* Install the constant in the subclass */` |
|       3 |  703 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|       3 |  704 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  705 | `				return rc;` |
|       - |  706 | `			}` |
|       1 |  707 | `		}` |
|       1 |  708 | `	}` |
|   26785 |  709 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
|       - |  710 | `	/* Copy methods signature */` |
|  105227 |  711 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|       - |  712 | `		/* Make sure the method are not redeclared in the subclass */` |
|   65057 |  713 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   65057 |  714 | `		pName = &pMeth->sFunc.sName;` |
|   65057 |  715 | `		if( SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte) == 0 ){` |
|       - |  716 | `			/* Install the method */` |
|   65057 |  717 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|   65057 |  718 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  719 | `				return rc;` |
|       - |  720 | `			}` |
|   32526 |  721 | `		}` |
|       5 |  722 | `	}` |
|       - |  723 | `	/* Mark as subclass */` |
|   26785 |  724 | `	pSub->pBase = pBase;` |
|       - |  725 | `	/* All done */` |
|   26785 |  726 | `	return SXRET_OK;` |
|   13395 |  727 | `}` |
|       - |  728 | `/*` |
|       - |  729 | ` * Implements an object interface in the given main class.` |
|       - |  730 | ` * According to the PHP language reference manual.` |
|       - |  731 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|       - |  732 | ` *  must implement, without having to define how these methods are handled.` |
|       - |  733 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  734 | ` *  class, but without any of the methods having their contents defined.` |
|       - |  735 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  736 | ` *` |
|       - |  737 | ` * This function return SXRET_OK if the interface was successfully implemented.` |
|       - |  738 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|       - |  739 | ` * error message.` |
|       - |  740 | ` */` |
|  355976 |  741 | `PH7_PRIVATE sxi32 PH7_ClassImplement(ph7_class *pMain,ph7_class *pInterface)` |
|       5 |  742 | `{` |
|       - |  743 | `	ph7_class_attr *pAttr;` |
|       - |  744 | `	SyHashEntry *pEntry;` |
|       - |  745 | `	SyString *pName;` |
|       - |  746 | `	sxi32 rc;` |
|       - |  747 | `	/* First off,copy all constants declared inside the interface */` |
|  355981 |  748 | `	SyHashResetLoopCursor(&pInterface->hAttr);` |
|  641055 |  749 | `	while((pEntry = SyHashGetNextEntry(&pInterface->hAttr)) != 0 ){` |
|       - |  750 | `		/* Point to the constant declaration */` |
|  107091 |  751 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  107091 |  752 | `		pName = &pAttr->sName;` |
|       - |  753 | `		/* Make sure the attribute is not redeclared in the main class */` |
|  107091 |  754 | `		if( SyHashGet(&pMain->hAttr,pName->zString,pName->nByte) == 0 ){` |
|       - |  755 | `			/* Install the attribute */` |
|  107087 |  756 | `			rc = SyHashInsert(&pMain->hAttr,pName->zString,pName->nByte,pAttr);` |
|  107087 |  757 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  758 | `				return rc;` |
|       - |  759 | `			}` |
|   53541 |  760 | `		}` |
|       5 |  761 | `	}` |
|       - |  762 | `	/* Install in the interface container */` |
|  355981 |  763 | `	SySetPut(&pMain->aInterface,(const void *)&pInterface);` |
|       - |  764 | `	/* Install interface method stubs into the implementing class.` |
|       - |  765 | `	 * Methods already defined in the class take precedence (they satisfy` |
|       - |  766 | `	 * the interface contract). Stubs retain PH7_CLASS_ATTR_ABSTRACT so` |
|       - |  767 | `	 * the unified check in GenStateCheckAbstractMethods catches missing ones.` |
|       - |  768 | `	 */` |
|       - |  769 | `	{` |
|       - |  770 | `		ph7_class_method *pMeth;` |
|       - |  771 | `		SyHashEntry *pMEntry;` |
|       - |  772 | `		SyString *pMName;` |
|  355981 |  773 | `		SyHashResetLoopCursor(&pInterface->hMethod);` |
| 1203763 |  774 | `		while((pMEntry = SyHashGetNextEntry(&pInterface->hMethod)) != 0 ){` |
|  669799 |  775 | `			pMeth = (ph7_class_method *)pMEntry->pUserData;` |
|  669799 |  776 | `			pMName = &pMeth->sFunc.sName;` |
|  669799 |  777 | `			if( SyHashGet(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte) == 0 ){` |
|      25 |  778 | `				rc = SyHashInsert(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte,pMeth);` |
|      25 |  779 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  780 | `					return rc;` |
|       - |  781 | `				}` |
|      10 |  782 | `			}` |
|       5 |  783 | `		}` |
|       - |  784 | `	}` |
|  355981 |  785 | `	return SXRET_OK;` |
|  177993 |  786 | `}` |
|       - |  787 | `/*` |
|       - |  788 | ` * Create a class instance [i.e: Object in the PHP jargon] at run-time.` |
|       - |  789 | ` * The following function is called when an object is created at run-time` |
|       - |  790 | ` * typically when the PH7_OP_NEW/PH7_OP_CLONE instructions are executed.` |
|       - |  791 | ` * Notes on object creation.` |
|       - |  792 | ` *` |
|       - |  793 | ` * According to PHP language reference manual.` |
|       - |  794 | ` * To create an instance of a class, the new keyword must be used. An object will always` |
|       - |  795 | ` * be created unless the object has a constructor defined that throws an exception on error.` |
|       - |  796 | ` * Classes should be defined before instantiation (and in some cases this is a requirement).` |
|       - |  797 | ` * If a string containing the name of a class is used with new, a new instance of that class` |
|       - |  798 | ` * will be created. If the class is in a namespace, its fully qualified name must be used when` |
|       - |  799 | ` * doing this.` |
|       - |  800 | ` * Example #3 Creating an instance` |
|       - |  801 | ` * <?php` |
|       - |  802 | ` *  $instance = new SimpleClass();` |
|       - |  803 | ` *   // This can also be done with a variable:` |
|       - |  804 | ` * $className = 'Foo';` |
|       - |  805 | ` * $instance = new $className(); // Foo()` |
|       - |  806 | ` * ?>` |
|       - |  807 | ` * In the class context, it is possible to create a new object by new self and new parent.` |
|       - |  808 | ` * When assigning an already created instance of a class to a new variable, the new variable` |
|       - |  809 | ` * will access the same instance as the object that was assigned. This behaviour is the same` |
|       - |  810 | ` * when passing instances to a function. A copy of an already created object can be made by` |
|       - |  811 | ` * cloning it.` |
|       - |  812 | ` * Example #4 Object Assignment` |
|       - |  813 | ` * <?php` |
|       - |  814 | ` *  class SimpleClass(){` |
|       - |  815 | ` *    public $var;` |
|       - |  816 | ` *  };` |
|       - |  817 | ` *  $instance = new SimpleClass();` |
|       - |  818 | ` *  $assigned   =  $instance;` |
|       - |  819 | ` *  $reference  =& $instance;` |
|       - |  820 | ` *  $instance->var = '$assigned will have this value';` |
|       - |  821 | ` *  $instance = null; // $instance and $reference become null` |
|       - |  822 | ` *  var_dump($instance);` |
|       - |  823 | ` *  var_dump($reference);` |
|       - |  824 | ` *  var_dump($assigned);` |
|       - |  825 | ` * ?>` |
|       - |  826 | ` * The above example will output:` |
|       - |  827 | ` * NULL` |
|       - |  828 | ` * NULL` |
|       - |  829 | ` * object(SimpleClass)#1 (1) {` |
|       - |  830 | ` *  ["var"]=>` |
|       - |  831 | ` *    string(30) "$assigned will have this value"` |
|       - |  832 | ` * }` |
|       - |  833 | ` * Example #5 Creating new objects` |
|       - |  834 | ` * <?php` |
|       - |  835 | ` * class Test` |
|       - |  836 | ` * {` |
|       - |  837 | ` *   static public function getNew()` |
|       - |  838 | ` *   {` |
|       - |  839 | ` *       return new static;` |
|       - |  840 | ` *   }` |
|       - |  841 | ` * }` |
|       - |  842 | ` * class Child extends Test` |
|       - |  843 | ` * {}` |
|       - |  844 | ` * $obj1 = new Test();` |
|       - |  845 | ` * $obj2 = new $obj1;` |
|       - |  846 | ` * var_dump($obj1 !== $obj2);` |
|       - |  847 | ` * $obj3 = Test::getNew();` |
|       - |  848 | ` * var_dump($obj3 instanceof Test);` |
|       - |  849 | ` * $obj4 = Child::getNew();` |
|       - |  850 | ` * var_dump($obj4 instanceof Child);` |
|       - |  851 | ` * ?>` |
|       - |  852 | ` * The above example will output:` |
|       - |  853 | ` * bool(true)` |
|       - |  854 | ` * bool(true)` |
|       - |  855 | ` * bool(true)` |
|       - |  856 | ` * Note that Symisc Systems have introduced powerfull extension to` |
|       - |  857 | ` * OO subsystem. For example a class attribute may have any complex` |
|       - |  858 | ` * expression associated with it when declaring the attribute unlike` |
|       - |  859 | ` * the standard PHP engine which would allow a single value.` |
|       - |  860 | ` * Example:` |
|       - |  861 | ` *  class myClass{` |
|       - |  862 | ` *    public $var = 25<<1+foo()/bar();` |
|       - |  863 | ` *  };` |
|       - |  864 | ` * Refer to the official documentation for more information.` |
|       - |  865 | ` */` |
|    8064 |  866 | `static ph7_class_instance * NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|       5 |  867 | `{` |
|       - |  868 | `	ph7_class_instance *pThis;` |
|       - |  869 | `	/* Allocate a new instance */` |
|    8069 |  870 | `	pThis = (ph7_class_instance *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_instance));` |
|    8069 |  871 | `	if( pThis == 0 ){` |
|     ! 0 |  872 | `		return 0;` |
|       - |  873 | `	}` |
|       - |  874 | `	/* Zero the structure */` |
|    8069 |  875 | `	SyZero(pThis,sizeof(ph7_class_instance));` |
|       - |  876 | `	/* Initialize fields */` |
|    8069 |  877 | `	pThis->iRef = 1;` |
|    8069 |  878 | `	pThis->pVm = pVm;` |
|    8069 |  879 | `	pThis->pClass = pClass;` |
|       - |  880 | `	/* Assign a fresh monotonic object handle id (clones get their own, like PHP). */` |
|    8069 |  881 | `	pThis->nObjId = pVm->nNextObjId++;` |
|    8069 |  882 | `	SyHashInit(&pThis->hAttr,&pVm->sAllocator,0,0);` |
|    8069 |  883 | `	return pThis;` |
|    4037 |  884 | `}` |
|       - |  885 | `/*` |
|       - |  886 | ` * Wrapper around the NewClassInstance() function defined above.` |
|       - |  887 | ` * See the block comment above for more information.` |
|       - |  888 | ` */` |
|    7826 |  889 | `PH7_PRIVATE ph7_class_instance * PH7_NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|       5 |  890 | `{` |
|       - |  891 | `	ph7_class_instance *pNew;` |
|       - |  892 | `	sxi32 rc;` |
|    7831 |  893 | `	pNew = NewClassInstance(&(*pVm),&(*pClass));` |
|    7831 |  894 | `	if( pNew == 0 ){` |
|     ! 0 |  895 | `		return 0;` |
|       - |  896 | `	}` |
|       - |  897 | `	/* Associate a private VM frame with this class instance */` |
|    7831 |  898 | `	rc = PH7_VmCreateClassInstanceFrame(&(*pVm),pNew);` |
|    7831 |  899 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  900 | `		SyMemBackendPoolFree(&pVm->sAllocator,pNew);` |
|     ! 0 |  901 | `		return 0;` |
|       - |  902 | `	}` |
|       - |  903 | `	/* php stamps a Throwable's file/line at CREATION, not in its constructor, so a` |
|       - |  904 | `	 * subclass that overrides __construct without calling parent::__construct still` |
|       - |  905 | `	 * reports the right site. Every instantiation path lands here. */` |
|    7831 |  906 | `	PH7_VmStampThrowableSite(&(*pVm),pNew);` |
|    7831 |  907 | `	return pNew;` |
|    3918 |  908 | `}` |
|       - |  909 | `/*` |
|       - |  910 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon] attribute.` |
|       - |  911 | ` * This function never fail.` |
|       - |  912 | ` */` |
|   20674 |  913 | `static ph7_value * ExtractClassAttrValue(ph7_vm *pVm,VmClassAttr *pAttr)` |
|       5 |  914 | `{` |
|       - |  915 | `	/* Extract the value */` |
|       - |  916 | `	ph7_value *pValue;` |
|   20679 |  917 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|   20679 |  918 | `	return pValue;` |
|       5 |  919 | `}` |
|       - |  920 | `/*` |
|       - |  921 | ` * Perform a clone operation on a class instance [i.e: Object in the PHP jargon].` |
|       - |  922 | ` * The following function is called when an object is cloned at run-time` |
|       - |  923 | ` * typically when the PH7_OP_CLONE instruction is executed.` |
|       - |  924 | ` * Notes on object cloning.` |
|       - |  925 | ` *` |
|       - |  926 | ` * According to PHP language reference manual.` |
|       - |  927 | ` * Creating a copy of an object with fully replicated properties is not always the wanted behavior.` |
|       - |  928 | ` * A good example of the need for copy constructors. Another example is if your object holds a reference` |
|       - |  929 | ` * to another object which it uses and when you replicate the parent object you want to create` |
|       - |  930 | ` * a new instance of this other object so that the replica has its own separate copy.` |
|       - |  931 | ` * An object copy is created by using the clone keyword (which calls the object's __clone() method if possible).` |
|       - |  932 | ` * An object's __clone() method cannot be called directly.` |
|       - |  933 | ` * $copy_of_object = clone $object;` |
|       - |  934 | ` * When an object is cloned, PHP 5 will perform a shallow copy of all of the object's properties.` |
|       - |  935 | ` * Any properties that are references to other variables, will remain references.` |
|       - |  936 | ` * Once the cloning is complete, if a __clone() method is defined, then the newly created object's __clone() method` |
|       - |  937 | ` * will be called, to allow any necessary properties that need to be changed.` |
|       - |  938 | ` * Example #1 Cloning an object` |
|       - |  939 | ` * <?php` |
|       - |  940 | ` * class SubObject` |
|       - |  941 | ` * {` |
|       - |  942 | ` *   static $instances = 0;` |
|       - |  943 | ` *   public $instance;` |
|       - |  944 | ` *` |
|       - |  945 | ` *   public function __construct() {` |
|       - |  946 | ` *       $this->instance = ++self::$instances;` |
|       - |  947 | ` *   }` |
|       - |  948 | ` *` |
|       - |  949 | ` *   public function __clone() {` |
|       - |  950 | ` *       $this->instance = ++self::$instances;` |
|       - |  951 | ` *   }` |
|       - |  952 | ` * }` |
|       - |  953 | ` *` |
|       - |  954 | ` * class MyCloneable` |
|       - |  955 | ` * {` |
|       - |  956 | ` *   public $object1;` |
|       - |  957 | ` *   public $object2;` |
|       - |  958 | ` *` |
|       - |  959 | ` *   function __clone()` |
|       - |  960 | ` *   {` |
|       - |  961 | ` *       // Force a copy of this->object, otherwise` |
|       - |  962 | ` *       // it will point to same object.` |
|       - |  963 | ` *       $this->object1 = clone $this->object1;` |
|       - |  964 | ` *   }` |
|       - |  965 | ` * }` |
|       - |  966 | ` * $obj = new MyCloneable();` |
|       - |  967 | ` * $obj->object1 = new SubObject();` |
|       - |  968 | ` * $obj->object2 = new SubObject();` |
|       - |  969 | ` * $obj2 = clone $obj;` |
|       - |  970 | ` * print("Original Object:\n");` |
|       - |  971 | ` * print_r($obj);` |
|       - |  972 | ` * print("Cloned Object:\n");` |
|       - |  973 | ` * print_r($obj2);` |
|       - |  974 | ` * ?>` |
|       - |  975 | ` * The above example will output:` |
|       - |  976 | ` * Original Object:` |
|       - |  977 | ` * MyCloneable Object` |
|       - |  978 | ` * (` |
|       - |  979 | ` *   [object1] => SubObject Object` |
|       - |  980 | ` *       (` |
|       - |  981 | ` *           [instance] => 1` |
|       - |  982 | ` *       )` |
|       - |  983 | ` *` |
|       - |  984 | ` *   [object2] => SubObject Object` |
|       - |  985 | ` *       (` |
|       - |  986 | ` *           [instance] => 2` |
|       - |  987 | ` *       )` |
|       - |  988 | ` *` |
|       - |  989 | ` * )` |
|       - |  990 | ` * Cloned Object:` |
|       - |  991 | ` * MyCloneable Object` |
|       - |  992 | ` * (` |
|       - |  993 | ` *   [object1] => SubObject Object` |
|       - |  994 | ` *       (` |
|       - |  995 | ` *           [instance] => 3` |
|       - |  996 | ` *       )` |
|       - |  997 | ` *` |
|       - |  998 | ` *   [object2] => SubObject Object` |
|       - |  999 | ` *       (` |
|       - | 1000 | ` *           [instance] => 2` |
|       - | 1001 | ` *       )` |
|       - | 1002 | ` * )` |
|       - | 1003 | ` */` |
|     238 | 1004 | `PH7_PRIVATE ph7_class_instance * PH7_CloneClassInstance(ph7_class_instance *pSrc)` |
|       3 | 1005 | `{` |
|       - | 1006 | `	ph7_class_instance *pClone;` |
|       - | 1007 | `	ph7_class_method *pMethod;` |
|       - | 1008 | `	SyHashEntry *pEntry2;` |
|       - | 1009 | `	SyHashEntry *pEntry;` |
|       - | 1010 | `	ph7_vm *pVm;` |
|       - | 1011 | `	sxi32 rc;` |
|       - | 1012 | `	/* Allocate a new instance */` |
|     241 | 1013 | `	pVm = pSrc->pVm;` |
|     241 | 1014 | `	pClone = NewClassInstance(pVm,pSrc->pClass);` |
|     241 | 1015 | `	if( pClone == 0 ){` |
|     ! 0 | 1016 | `		return 0;` |
|       - | 1017 | `	}` |
|       - | 1018 | `	/* Associate a private VM frame with this class instance */` |
|     241 | 1019 | `	rc = PH7_VmCreateClassInstanceFrame(pVm,pClone);` |
|     241 | 1020 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1021 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClone);` |
|     ! 0 | 1022 | `		return 0;` |
|       - | 1023 | `	}` |
|       - | 1024 | `	/* Duplicate object values. Iterate the SOURCE attributes and copy each into` |
|       - | 1025 | `	 * the clone's same-named slot (looked up by name, so order/count differences` |
|       - | 1026 | `	 * from dynamic properties don't matter). A dynamic (runtime-added) property` |
|       - | 1027 | `	 * has no declared counterpart in the clone, so synthesize it first — without` |
|       - | 1028 | `	 * this, a clone of a stdClass would silently lose all its dynamic properties. */` |
|     241 | 1029 | `	SyHashResetLoopCursor(&pSrc->hAttr);` |
|    2621 | 1030 | `	while((pEntry = SyHashGetNextEntry(&pSrc->hAttr)) != 0 ){` |
|    2383 | 1031 | `		VmClassAttr *pSrcAttr = (VmClassAttr *)pEntry->pUserData;` |
|    2383 | 1032 | `		VmClassAttr *pDestAttr = 0;` |
|    2383 | 1033 | `		ph7_value *pvSrc,*pvDest = 0;` |
|       - | 1034 | `		/* Duplicate non-static attribute */` |
|    2383 | 1035 | `		if( pSrcAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|    1693 | 1036 | `			continue;` |
|       - | 1037 | `		}` |
|     691 | 1038 | `		pEntry2 = SyHashGet(&pClone->hAttr,SyStringData(&pSrcAttr->pAttr->sName),SyStringLength(&pSrcAttr->pAttr->sName));` |
|     691 | 1039 | `		if( pEntry2 ){` |
|     677 | 1040 | `			pDestAttr = (VmClassAttr *)pEntry2->pUserData;` |
|     677 | 1041 | `			pvDest = ExtractClassAttrValue(pVm,pDestAttr);` |
|     352 | 1042 | `		}else if( pSrcAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){` |
|       - | 1043 | `			/* Dynamic property: synthesize the matching slot on the clone. */` |
|      22 | 1044 | `			pvDest = PH7_VmCreateDynamicAttr(pVm,pClone,` |
|      14 | 1045 | `				SyStringData(&pSrcAttr->pAttr->sName),SyStringLength(&pSrcAttr->pAttr->sName),&pDestAttr);` |
|       7 | 1046 | `		}` |
|       - | 1047 | `		/* Fetch the source value LAST: PH7_VmCreateDynamicAttr above may have` |
|       - | 1048 | `		 * reserved a slot and reallocated pVm->aMemObj, which would dangle any` |
|       - | 1049 | `		 * ph7_value* obtained before it. pvDest from the synth path already points` |
|       - | 1050 | `		 * into the post-realloc aMemObj; resolve pvSrc now so both are current. */` |
|     691 | 1051 | `		pvSrc = ExtractClassAttrValue(pVm,pSrcAttr);` |
|     691 | 1052 | `		if( pvSrc && pvDest ){` |
|     691 | 1053 | `			PH7_MemObjStore(pvSrc,pvDest);` |
|     344 | 1054 | `		}` |
|       - | 1055 | `		/* Carry over the per-instance state so the clone matches the source:` |
|       - | 1056 | `		 * VM_CLASS_ATTR_UNINIT marks a typed property as not-yet-initialized` |
|       - | 1057 | `		 * and doubles as the readonly write-once latch — without this a clone` |
|       - | 1058 | `		 * would reset to uninitialized (losing the value's readiness) and a` |
|       - | 1059 | `		 * readonly property would become writable again. */` |
|     691 | 1060 | `		if( pDestAttr ){` |
|     691 | 1061 | `			pDestAttr->iState = pSrcAttr->iState;` |
|     344 | 1062 | `		}` |
|       3 | 1063 | `	}` |
|       - | 1064 | `	/* A declared property unset() on the source is absent from the clone too (PHP). But the clone` |
|       - | 1065 | `	 * frame above materialized ALL declared attrs (with their defaults), so drop any clone attr whose` |
|       - | 1066 | `	 * name is not present on the source. Collect first, then delete — removing an entry mid-walk would` |
|       - | 1067 | `	 * free the node the SyHash loop cursor points at. */` |
|       - | 1068 | `	{` |
|       - | 1069 | `		SySet sDrop;` |
|     241 | 1070 | `		SySetInit(&sDrop,&pVm->sAllocator,sizeof(VmClassAttr *));` |
|     241 | 1071 | `		SyHashResetLoopCursor(&pClone->hAttr);` |
|    2623 | 1072 | `		while((pEntry = SyHashGetNextEntry(&pClone->hAttr)) != 0 ){` |
|    2385 | 1073 | `			VmClassAttr *pCloneAttr = (VmClassAttr *)pEntry->pUserData;` |
|    2385 | 1074 | `			if( pCloneAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|    1693 | 1075 | `				continue;` |
|       - | 1076 | `			}` |
|    1035 | 1077 | `			if( SyHashGet(&pSrc->hAttr,SyStringData(&pCloneAttr->pAttr->sName),` |
|    1038 | 1078 | `					SyStringLength(&pCloneAttr->pAttr->sName)) == 0 ){` |
|       3 | 1079 | `				SySetPut(&sDrop,(const void *)&pCloneAttr);` |
|       1 | 1080 | `			}` |
|       3 | 1081 | `		}` |
|     241 | 1082 | `		if( SySetUsed(&sDrop) > 0 ){` |
|       3 | 1083 | `			VmClassAttr **apDrop = (VmClassAttr **)SySetBasePtr(&sDrop);` |
|       - | 1084 | `			sxu32 i;` |
|       5 | 1085 | `			for( i = 0 ; i < SySetUsed(&sDrop) ; ++i ){` |
|       3 | 1086 | `				VmClassAttr *pVmAttr = apDrop[i];` |
|       4 | 1087 | `				SyHashDeleteEntry(&pClone->hAttr,SyStringData(&pVmAttr->pAttr->sName),` |
|       2 | 1088 | `					SyStringLength(&pVmAttr->pAttr->sName),0);` |
|       3 | 1089 | `				PH7_VmReleaseInstanceAttr(pVm,pVmAttr);` |
|       2 | 1090 | `			}` |
|       1 | 1091 | `		}` |
|     241 | 1092 | `		SySetRelease(&sDrop);` |
|       - | 1093 | `	}` |
|       - | 1094 | `	/* call the __clone method on the cloned object if available */` |
|     241 | 1095 | `	pMethod = PH7_ClassExtractMethod(pClone->pClass,"__clone",sizeof("__clone")-1);` |
|     241 | 1096 | `	if( pMethod ){` |
|      56 | 1097 | `		if( pMethod->iCloneDepth < 16 ){` |
|      54 | 1098 | `			pMethod->iCloneDepth++;` |
|      54 | 1099 | `			PH7_VmCallClassMethod(pVm,pClone,pMethod,0,0,0);` |
|      28 | 1100 | `		}else{` |
|       - | 1101 | `			/* Nesting limit reached */` |
|       3 | 1102 | `			PH7_VmThrowError(pVm,0,PH7_CTX_ERR,"Object clone limit reached,no more call to __clone()");` |
|       - | 1103 | `		}` |
|       - | 1104 | `		/* Reset the cursor */` |
|      56 | 1105 | `		pMethod->iCloneDepth = 0;` |
|      27 | 1106 | `	}` |
|       - | 1107 | `	/* Return the cloned object */` |
|     241 | 1108 | `	return pClone;` |
|     122 | 1109 | `}` |
|       - | 1110 | `#define CLASS_INSTANCE_DESTROYED 0x001 /* Instance is released */` |
|       - | 1111 | `/*` |
|       - | 1112 | ` * Free the per-instance allocations owned by ONE object attribute: its value slot (+ the typed-slot` |
|       - | 1113 | ` * enforcement entry), the synthesized ph7_class_attr for a dynamic (runtime-added) property, and the` |
|       - | 1114 | ` * VmClassAttr wrapper itself. Does NOT touch the hAttr entry node — the caller removes it` |
|       - | 1115 | `` * (`unset($o->p)` via SyHashDeleteEntry2; instance teardown via the wholesale SyHashRelease, so it must`` |
|       - | 1116 | ` * not delete entries mid-walk). Shared by PH7_ClassInstanceRelease and the OP_MEMBER unset path.` |
|       - | 1117 | ` */` |
|   27452 | 1118 | `PH7_PRIVATE void PH7_VmReleaseInstanceAttr(ph7_vm *pVm, VmClassAttr *pVmAttr)` |
|       5 | 1119 | `{` |
|   27457 | 1120 | `	if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|       - | 1121 | `		/* Drop any typed-property enforcement slot registered for this memobj, before the memobj` |
|       - | 1122 | `		 * is returned to the free list, so a future recycled slot does not inherit the stale entry. */` |
|   20249 | 1123 | `		if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|     302 | 1124 | `			SyHashDeleteEntry(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),0);` |
|     149 | 1125 | `		}` |
|   20249 | 1126 | `		PH7_VmUnsetMemObj(pVm,pVmAttr->nIdx,TRUE);` |
|   10122 | 1127 | `	}` |
|       - | 1128 | `	/* A dynamic property owns its synthesized ph7_class_attr (struct + inline name in one block) —` |
|       - | 1129 | `	 * free it here (the only place a per-instance pAttr is freed; declared attrs are class-owned). */` |
|   27457 | 1130 | `	if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){` |
|     117 | 1131 | `		SyMemBackendFree(&pVm->sAllocator,pVmAttr->pAttr);` |
|      58 | 1132 | `	}` |
|   27457 | 1133 | `	SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|   27457 | 1134 | `}` |
|       - | 1135 | `/*` |
|       - | 1136 | ` * Release a class instance [i.e: Object in the PHP jargon] and invoke any defined destructor.` |
|       - | 1137 | ` * This routine is invoked as soon as there are no other references to a particular` |
|       - | 1138 | ` * class instance.` |
|       - | 1139 | ` */` |
|    5460 | 1140 | `static void PH7_ClassInstanceRelease(ph7_class_instance *pThis)` |
|       5 | 1141 | `{` |
|       - | 1142 | `	ph7_class_method *pDestr;` |
|       - | 1143 | `	SyHashEntry *pEntry;` |
|       - | 1144 | `	ph7_class *pClass;` |
|       - | 1145 | `	ph7_vm *pVm;` |
|    5465 | 1146 | `	if( pThis->iFlags & CLASS_INSTANCE_DESTROYED ){` |
|       - | 1147 | `		/*` |
|       - | 1148 | `		 * Already destroyed,return immediately.` |
|       - | 1149 | `		 * This could happend if someone perform unset($this) in the destructor body.` |
|       - | 1150 | `		 */` |
|     ! 0 | 1151 | `		return;` |
|       - | 1152 | `	}` |
|       - | 1153 | `	/* Mark as destroyed */` |
|    5465 | 1154 | `	pThis->iFlags \|= CLASS_INSTANCE_DESTROYED;` |
|       - | 1155 | `	/* Invoke any defined destructor if available */` |
|    5465 | 1156 | `	pVm = pThis->pVm;` |
|    5465 | 1157 | `	pClass = pThis->pClass;` |
|    5465 | 1158 | `	pDestr = PH7_ClassExtractMethod(pClass,"__destruct",sizeof("__destruct")-1);` |
|    5465 | 1159 | `	if( pDestr && !pVm->bInReset ){` |
|       - | 1160 | `		/* Invoke the destructor. Skipped during ph7_vm_reset() bulk teardown:` |
|       - | 1161 | `		 * running user PHP against a half-reset VM is unsafe (see bInReset). */` |
|     471 | 1162 | `		pThis->iRef = 2; /* Prevent garbage collection */` |
|     471 | 1163 | `		PH7_VmCallClassMethod(pVm,pThis,pDestr,0,0,0);` |
|     233 | 1164 | `	}` |
|       - | 1165 | `	/* Weak-reference registry: kill the cell for this instance so every` |
|       - | 1166 | `	 * WeakReference/WeakMap handle observes the death (the cell outlives the` |
|       - | 1167 | `	 * instance until its own handles drop; removing the hash entry here keeps` |
|       - | 1168 | `	 * a pool-reused address from resurrecting a dead cell). */` |
|    5465 | 1169 | `	if( SyHashTotalEntry(&pVm->hWeakCell) > 0 ){` |
|      27 | 1170 | `		void *pCellData = 0;` |
|      26 | 1171 | `		if( SyHashDeleteEntry(&pVm->hWeakCell,(const void *)&pThis,sizeof(void *),&pCellData) == SXRET_OK` |
|      18 | 1172 | `		 && pCellData ){` |
|       9 | 1173 | `			((VmWeakCell *)pCellData)->pObj = 0;` |
|       4 | 1174 | `		}` |
|      13 | 1175 | `	}` |
|       - | 1176 | `	/* Release non-static attributes (the wholesale SyHashRelease below frees the entry nodes,` |
|       - | 1177 | `	 * so the helper must not delete them mid-walk). */` |
|    5465 | 1178 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|   32889 | 1179 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|   27429 | 1180 | `		PH7_VmReleaseInstanceAttr(pVm,(VmClassAttr *)pEntry->pUserData);` |
|       5 | 1181 | `	}` |
|       - | 1182 | `	/* Release the whole structure */` |
|    5465 | 1183 | `	SyHashRelease(&pThis->hAttr);` |
|    5465 | 1184 | `	SyMemBackendPoolFree(&pVm->sAllocator,pThis);` |
|    2735 | 1185 | `}` |
|       - | 1186 | `/*` |
|       - | 1187 | ` * Decrement the reference count of a class instance [i.e Object in the PHP jargon].` |
|       - | 1188 | ` * If the reference count reaches zero,release the whole instance.` |
|       - | 1189 | ` */` |
|  126424 | 1190 | `PH7_PRIVATE void PH7_ClassInstanceUnref(ph7_class_instance *pThis)` |
|       5 | 1191 | `{` |
|  126429 | 1192 | `	pThis->iRef--;` |
|  126429 | 1193 | `	if( pThis->iRef < 1 ){` |
|       - | 1194 | `		/* No more reference to this instance */` |
|    5465 | 1195 | `		PH7_ClassInstanceRelease(&(*pThis));` |
|    2730 | 1196 | `	}` |
|  126429 | 1197 | `}` |
|       - | 1198 | `/*` |
|       - | 1199 | ` * Compare two class instances [i.e: Objects in the PHP jargon]` |
|       - | 1200 | ` * Note on objects comparison:` |
|       - | 1201 | ` *  According to the PHP langauge reference manual` |
|       - | 1202 | ` *  When using the comparison operator (==), object variables are compared in a simple manner` |
|       - | 1203 | ` *  namely: Two object instances are equal if they have the same attributes and values, and are` |
|       - | 1204 | ` *  instances of the same class.` |
|       - | 1205 | ` *  On the other hand, when using the identity operator (===), object variables are identical` |
|       - | 1206 | ` *  if and only if they refer to the same instance of the same class.` |
|       - | 1207 | ` *  An example will clarify these rules.` |
|       - | 1208 | ` *  Example #1 Example of object comparison` |
|       - | 1209 | ` *  <?php` |
|       - | 1210 | ` *    function bool2str($bool)` |
|       - | 1211 | ` * {` |
|       - | 1212 | ` *   if ($bool === false) {` |
|       - | 1213 | ` *       return 'FALSE';` |
|       - | 1214 | ` *   } else {` |
|       - | 1215 | ` *       return 'TRUE';` |
|       - | 1216 | ` *   }` |
|       - | 1217 | ` * }` |
|       - | 1218 | ` * function compareObjects(&$o1, &$o2)` |
|       - | 1219 | ` * {` |
|       - | 1220 | ` *   echo 'o1 == o2 : ' . bool2str($o1 == $o2) . "\n";` |
|       - | 1221 | ` *   echo 'o1 != o2 : ' . bool2str($o1 != $o2) . "\n";` |
|       - | 1222 | ` *   echo 'o1 === o2 : ' . bool2str($o1 === $o2) . "\n";` |
|       - | 1223 | ` *   echo 'o1 !== o2 : ' . bool2str($o1 !== $o2) . "\n";` |
|       - | 1224 | ` * }` |
|       - | 1225 | ` * class Flag` |
|       - | 1226 | ` * {` |
|       - | 1227 | ` *   public $flag;` |
|       - | 1228 | ` *` |
|       - | 1229 | ` *   function Flag($flag = true) {` |
|       - | 1230 | ` *       $this->flag = $flag;` |
|       - | 1231 | ` *   }` |
|       - | 1232 | ` * }` |
|       - | 1233 | ` *` |
|       - | 1234 | ` * class OtherFlag` |
|       - | 1235 | ` * {` |
|       - | 1236 | ` *   public $flag;` |
|       - | 1237 | ` *` |
|       - | 1238 | ` *   function OtherFlag($flag = true) {` |
|       - | 1239 | ` *       $this->flag = $flag;` |
|       - | 1240 | ` *   }` |
|       - | 1241 | ` * }` |
|       - | 1242 | ` *` |
|       - | 1243 | ` * $o = new Flag();` |
|       - | 1244 | ` * $p = new Flag();` |
|       - | 1245 | ` * $q = $o;` |
|       - | 1246 | ` * $r = new OtherFlag();` |
|       - | 1247 | ` *` |
|       - | 1248 | ` * echo "Two instances of the same class\n";` |
|       - | 1249 | ` * compareObjects($o, $p);` |
|       - | 1250 | ` * echo "\nTwo references to the same instance\n";` |
|       - | 1251 | ` * compareObjects($o, $q);` |
|       - | 1252 | ` * echo "\nInstances of two different classes\n";` |
|       - | 1253 | ` * compareObjects($o, $r);` |
|       - | 1254 | ` * ?>` |
|       - | 1255 | ` * The above example will output:` |
|       - | 1256 | ` * Two instances of the same class` |
|       - | 1257 | ` * o1 == o2 : TRUE` |
|       - | 1258 | ` * o1 != o2 : FALSE` |
|       - | 1259 | ` * o1 === o2 : FALSE` |
|       - | 1260 | ` * o1 !== o2 : TRUE` |
|       - | 1261 | ` * Two references to the same instance` |
|       - | 1262 | ` * o1 == o2 : TRUE` |
|       - | 1263 | ` * o1 != o2 : FALSE` |
|       - | 1264 | ` * o1 === o2 : TRUE` |
|       - | 1265 | ` * o1 !== o2 : FALSE` |
|       - | 1266 | ` * Instances of two different classes` |
|       - | 1267 | ` * o1 == o2 : FALSE` |
|       - | 1268 | ` * o1 != o2 : TRUE` |
|       - | 1269 | ` * o1 === o2 : FALSE` |
|       - | 1270 | ` * o1 !== o2 : TRUE` |
|       - | 1271 | ` *` |
|       - | 1272 | ` * This function return 0 if the objects are equals according to the comprison rules defined above.` |
|       - | 1273 | ` * Any other return values indicates difference.` |
|       - | 1274 | ` */` |
|     272 | 1275 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest)` |
|       5 | 1276 | `{` |
|       - | 1277 | `	SyHashEntry *pEntry,*pEntry2;` |
|       - | 1278 | `	ph7_value sV1,sV2;` |
|       - | 1279 | `	sxi32 rc;` |
|     277 | 1280 | `	if( iNest > 31 ){` |
|       - | 1281 | `		/* Nesting limit reached */` |
|       6 | 1282 | `		PH7_VmThrowError(pLeft->pVm,0,PH7_CTX_ERR,"Nesting limit reached: Infinite recursion?");` |
|       6 | 1283 | `		return 1;` |
|       - | 1284 | `	}` |
|       - | 1285 | `	/* Comparison is performed only if the objects are instance of the same class */` |
|     273 | 1286 | `	if( pLeft->pClass != pRight->pClass ){` |
|       7 | 1287 | `		return 1;` |
|       - | 1288 | `	}` |
|     267 | 1289 | `	if( bStrict ){` |
|       - | 1290 | `		/*` |
|       - | 1291 | `		 * According to the PHP language reference manual:` |
|       - | 1292 | `		 *  when using the identity operator (===), object variables` |
|       - | 1293 | `		 *  are identical if and only if they refer to the same instance` |
|       - | 1294 | `		 *  of the same class.` |
|       - | 1295 | `		 */` |
|     103 | 1296 | `		return !(pLeft == pRight);` |
|       - | 1297 | `	}` |
|       - | 1298 | `	/*` |
|       - | 1299 | `	 * Attribute comparison.` |
|       - | 1300 | `	 * According to the PHP reference manual:` |
|       - | 1301 | `	 *  When using the comparison operator (==), object variables are compared` |
|       - | 1302 | `	 *  in a simple manner, namely: Two object instances are equal if they have` |
|       - | 1303 | `	 *  the same attributes and values, and are instances of the same class.` |
|       - | 1304 | `	 */` |
|     168 | 1305 | `	if( pLeft == pRight ){` |
|       - | 1306 | `		/* Same instance,don't bother processing,object are equals */` |
|       5 | 1307 | `		return 0;` |
|       - | 1308 | `	}` |
|       - | 1309 | `	/* Closures compare by IDENTITY under == as well (not by attributes): two distinct` |
|       - | 1310 | `	 * Closure instances are never equal, even when they wrap the same underlying function` |
|       - | 1311 | `	 * (PHP semantics). pLeft != pRight here, so a Closure pair is unequal. Without this,` |
|       - | 1312 | `` 	 * two capture-less lambdas of the same `function(){}` share the template's `$__fn` `` |
|       - | 1313 | `	 * name and would compare equal. */` |
|     164 | 1314 | `	if( pLeft->pVm->pClosureClass && pLeft->pClass == pLeft->pVm->pClosureClass ){` |
|       5 | 1315 | `		return 1;` |
|       - | 1316 | `	}` |
|       - | 1317 | `	/* Same class but a different number of attributes ⇒ different property sets` |
|       - | 1318 | `	 * (dynamic properties can give two same-class instances different counts). */` |
|     160 | 1319 | `	if( pLeft->hAttr.nEntry != pRight->hAttr.nEntry ){` |
|       3 | 1320 | `		return 1;` |
|       - | 1321 | `	}` |
|     158 | 1322 | `	PH7_MemObjInit(pLeft->pVm,&sV1);` |
|     158 | 1323 | `	PH7_MemObjInit(pLeft->pVm,&sV2);` |
|     158 | 1324 | `	sV1.nIdx = sV2.nIdx = SXU32_HIGH;` |
|       - | 1325 | `	/* Compare each left attribute against the RIGHT attribute of the SAME NAME` |
|       - | 1326 | `	 * (not in lockstep): dynamic properties may be stored in a different order` |
|       - | 1327 | `	 * on the two instances. Counts already match, so if every left attribute has` |
|       - | 1328 | `	 * an equal-valued same-named right attribute the property sets are equal. */` |
|     158 | 1329 | `	SyHashResetLoopCursor(&pLeft->hAttr);` |
|     194 | 1330 | `	while((pEntry = SyHashGetNextEntry(&pLeft->hAttr)) != 0 ){` |
|     170 | 1331 | `		VmClassAttr *p1 = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1332 | `		VmClassAttr *p2;` |
|       - | 1333 | `		ph7_value *pL,*pR;` |
|       - | 1334 | `		/* Compare only non-static attribute */` |
|     170 | 1335 | `		if( p1->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|     ! 0 | 1336 | `			continue;` |
|       - | 1337 | `		}` |
|     170 | 1338 | `		pEntry2 = SyHashGet(&pRight->hAttr,SyStringData(&p1->pAttr->sName),SyStringLength(&p1->pAttr->sName));` |
|     170 | 1339 | `		if( pEntry2 == 0 ){` |
|       - | 1340 | `			/* Left has a property the right lacks ⇒ not equal. */` |
|     ! 0 | 1341 | `			return 1;` |
|       - | 1342 | `		}` |
|     170 | 1343 | `		p2 = (VmClassAttr *)pEntry2->pUserData;` |
|     170 | 1344 | `		pL = ExtractClassAttrValue(pLeft->pVm,p1);` |
|     170 | 1345 | `		pR = ExtractClassAttrValue(pRight->pVm,p2);` |
|     170 | 1346 | `		if( pL && pR ){` |
|     170 | 1347 | `			PH7_MemObjLoad(pL,&sV1);` |
|     170 | 1348 | `			PH7_MemObjLoad(pR,&sV2);` |
|       - | 1349 | `			/* Compare the two values now */` |
|     170 | 1350 | `			rc = PH7_MemObjCmp(&sV1,&sV2,bStrict,iNest+1);` |
|     170 | 1351 | `			PH7_MemObjRelease(&sV1);` |
|     170 | 1352 | `			PH7_MemObjRelease(&sV2);` |
|     170 | 1353 | `			if( rc != 0 ){` |
|       - | 1354 | `				/* Not equals */` |
|     133 | 1355 | `				return rc;` |
|       - | 1356 | `			}` |
|      18 | 1357 | `		}` |
|       2 | 1358 | `	}` |
|       - | 1359 | `	/* Object are equals */` |
|      26 | 1360 | `	return 0;` |
|     141 | 1361 | `}` |
|       - | 1362 | `/*` |
|       - | 1363 | ` * Dump a class instance and the store the dump in the BLOB given` |
|       - | 1364 | ` * as the first argument.` |
|       - | 1365 | ` * Note that only non-static/non-constants attribute are dumped.` |
|       - | 1366 | ` * This function is typically invoked when the user issue a call` |
|       - | 1367 | ` * to [var_dump(),var_export(),print_r(),...].` |
|       - | 1368 | ` * This function SXRET_OK on success. Any other return value including` |
|       - | 1369 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|       - | 1370 | ` */` |
|       - | 1371 | `/*` |
|       - | 1372 | `` * Return the `name` property value of an enum case instance (the case name),`` |
|       - | 1373 | ` * or 0 when unavailable. Shared by the var_dump/var_export/json/serialize` |
|       - | 1374 | ` * renderers, which all print enum cases as Class::CaseName forms.` |
|       - | 1375 | ` */` |
|       6 | 1376 | `PH7_PRIVATE ph7_value * PH7_EnumCaseNameValue(ph7_class_instance *pThis)` |
|       1 | 1377 | `{` |
|       - | 1378 | `	SyHashEntry *pEntry;` |
|       7 | 1379 | `	if( (pThis->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|     ! 0 | 1380 | `		return 0;` |
|       - | 1381 | `	}` |
|       7 | 1382 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)"name",sizeof("name")-1);` |
|       7 | 1383 | `	if( pEntry == 0 ){` |
|     ! 0 | 1384 | `		return 0;` |
|       - | 1385 | `	}` |
|       7 | 1386 | `	return PH7_ClassInstanceExtractAttrValue(pThis,(VmClassAttr *)pEntry->pUserData);` |
|       4 | 1387 | `}` |
|       - | 1388 | `/*` |
|       - | 1389 | `` * Return the `value` property value (the backing value) of an enum case`` |
|       - | 1390 | ` * instance, or 0 when unavailable (pure enums have none).` |
|       - | 1391 | ` */` |
|       8 | 1392 | `PH7_PRIVATE ph7_value * PH7_EnumCaseBackingValueOf(ph7_class_instance *pThis)` |
|       1 | 1393 | `{` |
|       - | 1394 | `	SyHashEntry *pEntry;` |
|       9 | 1395 | `	if( (pThis->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|     ! 0 | 1396 | `		return 0;` |
|       - | 1397 | `	}` |
|       9 | 1398 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)"value",sizeof("value")-1);` |
|       9 | 1399 | `	if( pEntry == 0 ){` |
|       3 | 1400 | `		return 0;` |
|       - | 1401 | `	}` |
|       7 | 1402 | `	return PH7_ClassInstanceExtractAttrValue(pThis,(VmClassAttr *)pEntry->pUserData);` |
|       5 | 1403 | `}` |
|       - | 1404 | `/*` |
|       - | 1405 | ` * Emit a class-instance dump header plus its trailing newline. For var_dump` |
|       - | 1406 | ` * (ShowType) it completes the "object(" prefix the caller already emitted as` |
|       - | 1407 | ` *   ClassName)#<id> (<count>) {` |
|       - | 1408 | ` * for print_r it emits the legacy PHL  Object(ClassName) {  (count/id unused).` |
|       - | 1409 | `` * Enum cases print php's `ClassName Enum {` print_r header (var_dump never`` |
|       - | 1410 | `` * reaches here for enums — PH7_MemObjDump prints `enum(S::A)` directly).`` |
|       - | 1411 | ` */` |
|     136 | 1412 | `static void DumpClassInstanceHeader(SyBlob *pOut,ph7_class *pClass,sxu32 nObjId,int ShowType,sxu32 nCount)` |
|       3 | 1413 | `{` |
|     139 | 1414 | `	if( ShowType ){` |
|       - | 1415 | ``		/* var_dump: `object(C)#id (n) {` */`` |
|     135 | 1416 | `		SyBlobFormat(&(*pOut),"object(%z)#%u (%u) {",&pClass->sName,nObjId,nCount);` |
|     135 | 1417 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|     135 | 1418 | `		return;` |
|       - | 1419 | `	}` |
|       - | 1420 | ``	/* print_r: `C Object` / `E Enum[:backing]` — the '(' line is emitted by`` |
|       - | 1421 | `	 * the body renderer at the container indent. */` |
|       6 | 1422 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|     ! 0 | 1423 | `		SyBlobFormat(&(*pOut),"%z Enum",&pClass->sName);` |
|     ! 0 | 1424 | `		if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|     ! 0 | 1425 | `			SyBlobAppend(&(*pOut),":int",sizeof(":int")-1);` |
|     ! 0 | 1426 | `		}else if( pClass->nEnumBacking == MEMOBJ_STRING ){` |
|     ! 0 | 1427 | `			SyBlobAppend(&(*pOut),":string",sizeof(":string")-1);` |
|     ! 0 | 1428 | `		}` |
|     ! 0 | 1429 | `	}else{` |
|       6 | 1430 | `		SyBlobFormat(&(*pOut),"%z Object",&pClass->sName);` |
|       - | 1431 | `	}` |
|       6 | 1432 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      71 | 1433 | `}` |
|       - | 1434 | `/*` |
|       - | 1435 | ` * The class that DECLARED pAttr: inheritance shares attr pointers down the` |
|       - | 1436 | ` * chain, so the declaring class is the most ANCESTRAL class whose hAttr still` |
|       - | 1437 | ` * maps the name to this exact pointer. php's var_dump/print_r use it for the` |
|       - | 1438 | `` * `["p":"Decl":private]` annotation.`` |
|       - | 1439 | ` */` |
|       6 | 1440 | `static ph7_class * OoAttrDeclaringClass(ph7_class *pClass,ph7_class_attr *pAttr)` |
|       1 | 1441 | `{` |
|       - | 1442 | `	/* Attrs record their declaring class at install time (inheritance/trait` |
|       - | 1443 | `	 * copies share the pointer, so the field survives the chain). */` |
|       7 | 1444 | `	return pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|       1 | 1445 | `}` |
|       - | 1446 | `/*` |
|       - | 1447 | `` * Emit a property's dump key: var_dump `["x"]=>` / `["p":"C":private]=>` /`` |
|       - | 1448 | `` * `["q":protected]=>`; print_r `[x] => ` / `[p:C:private] => ` /`` |
|       - | 1449 | `` * `[q:protected] => ` (php's exact annotations).`` |
|       - | 1450 | ` */` |
|     138 | 1451 | `static void OoDumpPropKey(SyBlob *pOut,ph7_class_instance *pThis,ph7_class_attr *pAttr,int ShowType)` |
|       2 | 1452 | `{` |
|     140 | 1453 | `	const char *zQ = ShowType ? "\"" : "";` |
|     140 | 1454 | `	SyBlobFormat(&(*pOut),"[%s%z%s",zQ,&pAttr->sName,zQ);` |
|     140 | 1455 | `	if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       7 | 1456 | `		ph7_class *pDecl = OoAttrDeclaringClass(pThis->pClass,pAttr);` |
|       7 | 1457 | `		SyBlobFormat(&(*pOut),":%s%z%s:private",zQ,&pDecl->sName,zQ);` |
|     137 | 1458 | `	}else if( pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
|     ! 0 | 1459 | `		SyBlobAppend(&(*pOut),":protected",sizeof(":protected")-1);` |
|     ! 0 | 1460 | `	}` |
|     140 | 1461 | `	SyBlobAppend(&(*pOut),ShowType ? "]=>" : "] => ",ShowType ? 3 : 5);` |
|     140 | 1462 | `}` |
|     140 | 1463 | `PH7_PRIVATE sxi32 PH7_ClassInstanceDump(SyBlob *pOut,ph7_class_instance *pThis,int ShowType,int nTab,int nDepth)` |
|       3 | 1464 | `{` |
|       - | 1465 | `	SyHashEntry *pEntry;` |
|       - | 1466 | `	ph7_value *pValue;` |
|       - | 1467 | `	sxi32 rc;` |
|       - | 1468 | `	int i;` |
|     143 | 1469 | `	if( nDepth > 31 ){` |
|       - | 1470 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|       - | 1471 | `		/* Nesting limit reached..halt immediately*/` |
|       5 | 1472 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|       5 | 1473 | `		return SXERR_LIMIT;` |
|       - | 1474 | `	}` |
|     139 | 1475 | `	rc = SXRET_OK;` |
|       - | 1476 | `	{` |
|       - | 1477 | `		/* Both var_dump and print_r consult __debugInfo() (PHP behavior);` |
|       - | 1478 | `		 * var_export uses a separate renderer and never reaches here. When the` |
|       - | 1479 | `		 * method is present and returns an array, render that array's entries as` |
|       - | 1480 | `		 * the object body, with the header showing the debug array's count. The` |
|       - | 1481 | `		 * nDepth guard above protects against a __debugInfo returning the object` |
|       - | 1482 | `		 * itself. */` |
|     139 | 1483 | `		ph7_class_method *pDbg = PH7_ClassExtractMethod(pThis->pClass,"__debugInfo",sizeof("__debugInfo")-1);` |
|     139 | 1484 | `		if( pDbg ){` |
|       - | 1485 | `			ph7_value sResult;` |
|       5 | 1486 | `			PH7_MemObjInit(pThis->pVm,&sResult);` |
|       5 | 1487 | `			PH7_VmCallClassMethod(pThis->pVm,pThis,pDbg,&sResult,0,0);` |
|       5 | 1488 | `			if( sResult.iFlags & MEMOBJ_HASHMAP ){` |
|       5 | 1489 | `				ph7_hashmap *pMap = (ph7_hashmap *)sResult.x.pOther;` |
|       - | 1490 | `				/* Header count is the debug array's entry count. */` |
|       5 | 1491 | `				DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,pMap->nEntry);` |
|       5 | 1492 | `				if( !ShowType ){` |
|       3 | 1493 | `					for( i = 0 ; i < nTab ; i++ ){` |
|     ! 0 | 1494 | `						SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     ! 0 | 1495 | `					}` |
|       3 | 1496 | `					SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       1 | 1497 | `				}` |
|       5 | 1498 | `				rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth);` |
|       5 | 1499 | `				for( i = 0 ; i < nTab ; i++ ){` |
|     ! 0 | 1500 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     ! 0 | 1501 | `				}` |
|       5 | 1502 | `				if( ShowType ){` |
|       3 | 1503 | `					SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|       2 | 1504 | `				}else{` |
|       3 | 1505 | `					SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       - | 1506 | `				}` |
|       5 | 1507 | `				PH7_MemObjRelease(&sResult);` |
|       5 | 1508 | `				return rc;` |
|       - | 1509 | `			}` |
|       - | 1510 | `			/* Non-array return: behave as if __debugInfo were absent. */` |
|     ! 0 | 1511 | `			PH7_MemObjRelease(&sResult);` |
|     ! 0 | 1512 | `		}` |
|       - | 1513 | `	}` |
|       - | 1514 | `	{` |
|       - | 1515 | `		/* var_dump's header needs the property count up front, so pre-count the` |
|       - | 1516 | `		 * non-static/non-constant attributes (matching the dump loop below). */` |
|     134 | 1517 | `		sxu32 nProp = 0;` |
|     134 | 1518 | `		if( ShowType ){` |
|     132 | 1519 | `			SyHashResetLoopCursor(&pThis->hAttr);` |
|     270 | 1520 | `			while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|     140 | 1521 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     140 | 1522 | `				if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_HOOK_VIRTUAL)) == 0 ){` |
|     136 | 1523 | `					nProp++;` |
|      67 | 1524 | `				}` |
|       2 | 1525 | `			}` |
|      65 | 1526 | `		}` |
|     134 | 1527 | `		DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,nProp);` |
|       - | 1528 | `	}` |
|     134 | 1529 | `	if( !ShowType ){` |
|       - | 1530 | `		/* print_r body opener: '(' at the container indent */` |
|       3 | 1531 | `		for( i = 0 ; i < nTab ; i++ ){` |
|     ! 0 | 1532 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     ! 0 | 1533 | `		}` |
|       3 | 1534 | `		SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       1 | 1535 | `	}` |
|       - | 1536 | `	/* Dump object attributes (php 8.4: VIRTUAL hooked properties have no` |
|       - | 1537 | `	 * backing store — excluded from var_dump/print_r) */` |
|     134 | 1538 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|     218 | 1539 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|     144 | 1540 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     144 | 1541 | `		if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_HOOK_VIRTUAL)) == 0 ){` |
|       - | 1542 | `			/* Dump non-static/constant attribute only */` |
|     140 | 1543 | `			pValue = ExtractClassAttrValue(pThis->pVm,pVmAttr);` |
|     140 | 1544 | `			if( pValue == 0 ){` |
|     ! 0 | 1545 | `				continue;` |
|       - | 1546 | `			}` |
|     140 | 1547 | `			if( ShowType ){` |
|       - | 1548 | ``				/* var_dump prop: `["x"(:…)]=>` at nTab+2, the value on the next`` |
|       - | 1549 | `				 * line at the same indent (php). */` |
|    4124 | 1550 | `				for( i = 0 ; i < nTab + 2 ; i++ ){` |
|    3990 | 1551 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|    1996 | 1552 | `				}` |
|     136 | 1553 | `				OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,TRUE);` |
|     136 | 1554 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|     136 | 1555 | `				rc = PH7_MemObjDump(&(*pOut),pValue,TRUE,nTab+2,nDepth,0);` |
|     136 | 1556 | `				if( rc == SXERR_LIMIT ){` |
|     125 | 1557 | `					break;` |
|       - | 1558 | `				}` |
|       7 | 1559 | `			}else{` |
|       - | 1560 | ``				/* print_r prop: `[x(:…)] => value` at nTab+4; container values`` |
|       - | 1561 | `				 * render their block at nTab+8 followed by php's blank line. */` |
|      21 | 1562 | `				for( i = 0 ; i < nTab + 4 ; i++ ){` |
|      17 | 1563 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       9 | 1564 | `				}` |
|       5 | 1565 | `				OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,FALSE);` |
|       4 | 1566 | `				if( (pValue->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ))` |
|       3 | 1567 | `				 && (pValue->iFlags & MEMOBJ_NULL) == 0 ){` |
|     ! 0 | 1568 | `					rc = PH7_MemObjDump(&(*pOut),pValue,FALSE,nTab+8,nDepth,0);` |
|     ! 0 | 1569 | `					SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|     ! 0 | 1570 | `					if( rc == SXERR_LIMIT ){` |
|     ! 0 | 1571 | `						break;` |
|       - | 1572 | `					}` |
|     ! 0 | 1573 | `				}else{` |
|       5 | 1574 | `					PH7_MemObjPrintRInline(&(*pOut),pValue);` |
|       5 | 1575 | `					SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 1576 | `				}` |
|       - | 1577 | `			}` |
|       7 | 1578 | `		}` |
|       2 | 1579 | `	}` |
|    3854 | 1580 | `	for( i = 0 ; i < nTab ; i++ ){` |
|    3721 | 1581 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|    1861 | 1582 | `	}` |
|     134 | 1583 | `	if( ShowType ){` |
|     132 | 1584 | `		SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      67 | 1585 | `	}else{` |
|       3 | 1586 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       - | 1587 | `	}` |
|     134 | 1588 | `	return rc;` |
|      73 | 1589 | `}` |
|       - | 1590 | `/*` |
|       - | 1591 | ` * Call a magic method [i.e: __toString(),__toBool(),__Invoke()...]` |
|       - | 1592 | ` * Return SXRET_OK on successfull call. Any other return value indicates failure.` |
|       - | 1593 | ` * Notes on magic methods.` |
|       - | 1594 | ` * According to the PHP language reference manual.` |
|       - | 1595 | ` *  The function names __construct(), __destruct(), __call(), __callStatic()` |
|       - | 1596 | ` *  __get(),  __toString(), __invoke(), __clone() are magical in PHP classes.` |
|       - | 1597 | ` * You cannot have functions with these names in any of your classes unless` |
|       - | 1598 | ` * you want the magic functionality associated with them.` |
|       - | 1599 | ` * Example of magical methods:` |
|       - | 1600 | ` * __toString()` |
|       - | 1601 | ` *  The __toString() method allows a class to decide how it will react when it is treated like` |
|       - | 1602 | ` *  a string. For example, what echo $obj; will print. This method must return a string.` |
|       - | 1603 | ` *  Example #2 Simple example` |
|       - | 1604 | ` * <?php` |
|       - | 1605 | ` * // Declare a simple class` |
|       - | 1606 | ` * class TestClass` |
|       - | 1607 | ` * {` |
|       - | 1608 | ` *   public $foo;` |
|       - | 1609 | ` *` |
|       - | 1610 | ` *   public function __construct($foo)` |
|       - | 1611 | ` *   {` |
|       - | 1612 | ` *       $this->foo = $foo;` |
|       - | 1613 | ` *   }` |
|       - | 1614 | ` *` |
|       - | 1615 | ` *   public function __toString()` |
|       - | 1616 | ` *   {` |
|       - | 1617 | ` *       return $this->foo;` |
|       - | 1618 | ` *   }` |
|       - | 1619 | ` * }` |
|       - | 1620 | ` * $class = new TestClass('Hello');` |
|       - | 1621 | ` * echo $class;` |
|       - | 1622 | ` * ?>` |
|       - | 1623 | ` * The above example will output:` |
|       - | 1624 | ` *  Hello` |
|       - | 1625 | ` *` |
|       - | 1626 | ` * Note that PH7 does not support all the magical method and introudces __toFloat(),__toInt()` |
|       - | 1627 | ` * which have the same behaviour as __toString() but for float and integer types` |
|       - | 1628 | ` * respectively.` |
|       - | 1629 | ` * Refer to the official documentation for more information.` |
|       - | 1630 | ` */` |
|      54 | 1631 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallMagicMethod(` |
|       - | 1632 | `	ph7_vm *pVm,               /* VM that own all this stuff */` |
|       - | 1633 | `	ph7_class *pClass,         /* Target class */` |
|       - | 1634 | `	ph7_class_instance *pThis, /* Target object */` |
|       - | 1635 | `	const char *zMethod,       /* Magic method name [i.e: __toString()]*/` |
|       - | 1636 | `	sxu32 nByte,               /* zMethod length*/` |
|       - | 1637 | `	const SyString *pAttrName, /* Attribute name */` |
|       - | 1638 | `	ph7_value *pResult         /* OUT: magic method return value. NULL to discard */` |
|       - | 1639 | `	)` |
|       1 | 1640 | `{` |
|      55 | 1641 | `	ph7_value *apArg[2] = { 0 , 0 };` |
|       - | 1642 | `	ph7_class_method *pMeth;` |
|       - | 1643 | `	ph7_value sAttr; /* cc warning */` |
|       - | 1644 | `	sxi32 rc;` |
|       - | 1645 | `	int nArg;` |
|       - | 1646 | `	/* Make sure the magic method is available */` |
|      55 | 1647 | `	pMeth = PH7_ClassExtractMethod(&(*pClass),zMethod,nByte);` |
|      55 | 1648 | `	if( pMeth == 0 ){` |
|       - | 1649 | `		/* No such method,return immediately */` |
|     ! 0 | 1650 | `		return SXERR_NOTFOUND;` |
|       - | 1651 | `	}` |
|      55 | 1652 | `	nArg = 0;` |
|       - | 1653 | `	/* Copy arguments */` |
|      55 | 1654 | `	if( pAttrName ){` |
|      55 | 1655 | `		PH7_MemObjInitFromString(pVm,&sAttr,pAttrName);` |
|      55 | 1656 | `		sAttr.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      55 | 1657 | `		apArg[0] = &sAttr;` |
|      55 | 1658 | `		nArg = 1;` |
|      27 | 1659 | `	}` |
|       - | 1660 | `	/* Call the magic method now */` |
|      55 | 1661 | `	rc = PH7_VmCallClassMethod(pVm,&(*pThis),pMeth,pResult,nArg,apArg);` |
|       - | 1662 | `	/* Clean up */` |
|      55 | 1663 | `	if( pAttrName ){` |
|      55 | 1664 | `		PH7_MemObjRelease(&sAttr);` |
|      27 | 1665 | `	}` |
|      55 | 1666 | `	return rc;` |
|      28 | 1667 | `}` |
|       - | 1668 | `/*` |
|       - | 1669 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon].` |
|       - | 1670 | ` * This function is simply a wrapper on ExtractClassAttrValue().` |
|       - | 1671 | ` */` |
|    8062 | 1672 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceExtractAttrValue(ph7_class_instance *pThis,VmClassAttr *pAttr)` |
|       5 | 1673 | `{` |
|       - | 1674 | `   /* Extract the attribute value */` |
|       - | 1675 | `	ph7_value *pValue;` |
|    8067 | 1676 | `	pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|    8067 | 1677 | `	return pValue;` |
|       5 | 1678 | `}` |
|       - | 1679 | `/*` |
|       - | 1680 | ` * Convert a class instance [i.e: Object in the PHP jargon] into a hashmap [i.e: array in the PHP jargon].` |
|       - | 1681 | ` * Return SXRET_OK on success. Any other value indicates failure.` |
|       - | 1682 | ` * Note on object conversion to array:` |
|       - | 1683 | ` *  Acccording to the PHP language reference manual` |
|       - | 1684 | ` *  If an object is converted to an array, the result is an array whose elements are the object's properties.` |
|       - | 1685 | ` *  The keys are the member variable names.` |
|       - | 1686 | ` *` |
|       - | 1687 | ` *  The following example:` |
|       - | 1688 | ` *  class Test {` |
|       - | 1689 | ` *   public $A = 25<<1;  // 50` |
|       - | 1690 | ` *	 public $c = rand_str(3);   // Random string of length 3` |
|       - | 1691 | ` *	 public $d = rand() & 1023; // Random number between 0..1023` |
|       - | 1692 | ` *  }` |
|       - | 1693 | ` *  var_dump((array) new Test());` |
|       - | 1694 | ` *	Will output:` |
|       - | 1695 | ` *  array(3) {` |
|       - | 1696 | ` *   [A] =>` |
|       - | 1697 | ` *      int(50)` |
|       - | 1698 | ` *   [c] =>` |
|       - | 1699 | ` *     string(3 'aps')` |
|       - | 1700 | ` *   [d] =>` |
|       - | 1701 | ` *     int(991)` |
|       - | 1702 | ` *  }` |
|       - | 1703 | ` * You have noticed that PH7 allow class attributes [i.e: $a,$c,$d in the example above]` |
|       - | 1704 | ` * have any complex expression (even function calls/Annonymous functions) as their default` |
|       - | 1705 | ` * value unlike the standard PHP engine.` |
|       - | 1706 | ` * This is a very powerful feature that you have to look at.` |
|       - | 1707 | ` */` |
|      14 | 1708 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|       1 | 1709 | `{` |
|       - | 1710 | `	SyHashEntry *pEntry;` |
|       - | 1711 | `	SyString *pAttrName;` |
|       - | 1712 | `	VmClassAttr *pAttr;` |
|       - | 1713 | `	ph7_value *pValue;` |
|       - | 1714 | `	ph7_value sName;` |
|       - | 1715 | `	/* Reset the loop cursor */` |
|      15 | 1716 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      15 | 1717 | `	PH7_MemObjInitFromString(pThis->pVm,&sName,0);` |
|      51 | 1718 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       - | 1719 | `		/* Point to the current attribute */` |
|      37 | 1720 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      37 | 1721 | `		if( pAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|       - | 1722 | `			/* php 8.4: a VIRTUAL hooked property has no backing store — the` |
|       - | 1723 | `			 * (array) cast excludes it (raw surface, get is NOT dispatched) */` |
|       7 | 1724 | `			continue;` |
|       - | 1725 | `		}` |
|       - | 1726 | `		/* Extract attribute value */` |
|      31 | 1727 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|      31 | 1728 | `		if( pValue ){` |
|       - | 1729 | `			/* Build attribute name */` |
|      31 | 1730 | `			pAttrName = &pAttr->pAttr->sName;` |
|      31 | 1731 | `			PH7_MemObjStringAppend(&sName,pAttrName->zString,pAttrName->nByte);` |
|       - | 1732 | `			/* Perform the insertion */` |
|      31 | 1733 | `			PH7_HashmapInsert(pMap,&sName,pValue);` |
|       - | 1734 | `			/* Reset the string cursor */` |
|      31 | 1735 | `			SyBlobReset(&sName.sBlob);` |
|      15 | 1736 | `		}` |
|       1 | 1737 | `	}` |
|      15 | 1738 | `	PH7_MemObjRelease(&sName);` |
|      15 | 1739 | `	return SXRET_OK;` |
|       1 | 1740 | `}` |
|       - | 1741 | `/*` |
|       - | 1742 | ` * Iterate throw class attributes and invoke the given callback [i.e: xWalk()] for each` |
|       - | 1743 | ` * retrieved attribute.` |
|       - | 1744 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|       - | 1745 | ` * the attribute value in the callback body will not alter the real attribute value.` |
|       - | 1746 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|       - | 1747 | ` * a value different from PH7_OK.` |
|       - | 1748 | ` * Refer to [ph7_object_walk()] for more information.` |
|       - | 1749 | ` */` |
|     ! 0 | 1750 | `PH7_PRIVATE sxi32 PH7_ClassInstanceWalk(` |
|       - | 1751 | `	ph7_class_instance *pThis, /* Target object */` |
|       - | 1752 | `	int (*xWalk)(const char *,ph7_value *,void *), /* Walker callback */` |
|       - | 1753 | `	void *pUserData /* Last argument to xWalk() */` |
|       - | 1754 | `	)` |
|     ! 0 | 1755 | `{` |
|       - | 1756 | `	SyHashEntry *pEntry; /* Hash entry */` |
|       - | 1757 | `	VmClassAttr *pAttr;  /* Pointer to the attribute */` |
|       - | 1758 | `	ph7_value *pValue;   /* Attribute value */` |
|       - | 1759 | `	ph7_value sValue;    /* Copy of the attribute value */` |
|       - | 1760 | `	int rc;` |
|       - | 1761 | `	/* Reset the loop cursor */` |
|     ! 0 | 1762 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|     ! 0 | 1763 | `	PH7_MemObjInit(pThis->pVm,&sValue);` |
|       - | 1764 | `	/* Start the walk process */` |
|     ! 0 | 1765 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       - | 1766 | `		/* Point to the current attribute */` |
|     ! 0 | 1767 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1768 | `		/* Extract attribute value */` |
|     ! 0 | 1769 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     ! 0 | 1770 | `		if( pValue ){` |
|     ! 0 | 1771 | `			PH7_MemObjLoad(pValue,&sValue);` |
|       - | 1772 | `			/* Invoke the supplied callback */` |
|     ! 0 | 1773 | `			rc =  xWalk(SyStringData(&pAttr->pAttr->sName),&sValue,pUserData);` |
|     ! 0 | 1774 | `			PH7_MemObjRelease(&sValue);` |
|     ! 0 | 1775 | `			if( rc != PH7_OK){` |
|       - | 1776 | `				/* User callback request an operation abort */` |
|     ! 0 | 1777 | `				return SXERR_ABORT;` |
|       - | 1778 | `			}` |
|     ! 0 | 1779 | `		}` |
|     ! 0 | 1780 | `	}` |
|       - | 1781 | `	/* All done */` |
|     ! 0 | 1782 | `	return SXRET_OK;` |
|     ! 0 | 1783 | `}` |
|       - | 1784 | `/*` |
|       - | 1785 | ` * Extract a class atrribute value.` |
|       - | 1786 | ` * Return a pointer to the attribute value on success. Otherwise NULL.` |
|       - | 1787 | ` * Note:` |
|       - | 1788 | ` *  Access to static and constant attribute is not allowed. That is,the function` |
|       - | 1789 | ` *  will return NULL in case someone (host-application code) try to extract` |
|       - | 1790 | ` *  a static/constant attribute.` |
|       - | 1791 | ` */` |
|   10750 | 1792 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceFetchAttr(ph7_class_instance *pThis,const SyString *pName)` |
|       5 | 1793 | `{` |
|       - | 1794 | `	SyHashEntry *pEntry;` |
|       - | 1795 | `	VmClassAttr *pAttr;` |
|       - | 1796 | `	/* Query the attribute hashtable */` |
|   10755 | 1797 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);` |
|   10755 | 1798 | `	if( pEntry == 0 ){` |
|       - | 1799 | `		/* No such attribute */` |
|     ! 0 | 1800 | `		return 0;` |
|       - | 1801 | `	}` |
|       - | 1802 | `	/* Point to the class atrribute */` |
|   10755 | 1803 | `	pAttr = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1804 | `	/* Check if we are dealing with a static/constant attribute */` |
|   10755 | 1805 | `	if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|       - | 1806 | `		/* Access is forbidden */` |
|     ! 0 | 1807 | `		return 0;` |
|       - | 1808 | `	}` |
|       - | 1809 | `	/* Return the attribute value */` |
|   10755 | 1810 | `	return ExtractClassAttrValue(pThis->pVm,pAttr);` |
|    5380 | 1811 | `}` |
|       - | 1812 |  |
