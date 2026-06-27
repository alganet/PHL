# src/ph7/oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 599/659 lines (90.90%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|      - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    5 | ` */` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `/*` |
|      - |    8 | ` * This file implement an Object Oriented (OO) subsystem for the PH7 engine.` |
|      - |    9 | ` */` |
|      - |   10 | `/*` |
|      - |   11 | ` * Create an empty class.` |
|      - |   12 | ` * Return a pointer to a raw class (ph7_class instance) on success. NULL otherwise.` |
|      - |   13 | ` */` |
| 139380 |   14 | `PH7_PRIVATE ph7_class * PH7_NewRawClass(ph7_vm *pVm,const SyString *pName,sxu32 nLine)` |
|      5 |   15 |  |
|      - |   16 | `	ph7_class *pClass;` |
|      - |   17 | `	char *zName;` |
|      - |   18 | `	/* Allocate a new instance */` |
| 139385 |   19 | `	pClass = (ph7_class *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class));` |
| 139385 |   20 | `	if( pClass == 0 ){` |
|    ! 0 |   21 | `		return 0;` |
|      - |   22 | `	}` |
|      - |   23 | `	/* Zero the structure */` |
| 139385 |   24 | `	SyZero(pClass,sizeof(ph7_class));` |
|      - |   25 | `	/* Duplicate class name */` |
| 139385 |   26 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
| 139385 |   27 | `	if( zName == 0 ){` |
|    ! 0 |   28 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClass);` |
|    ! 0 |   29 | `		return 0;` |
|      - |   30 | `	}` |
|      - |   31 | `	/* Initialize fields */` |
| 139385 |   32 | `	SyStringInitFromBuf(&pClass->sName,zName,pName->nByte);` |
| 139385 |   33 | `	SyHashInit(&pClass->hMethod,&pVm->sAllocator,0,0);` |
| 139385 |   34 | `	SyHashInit(&pClass->hAttr,&pVm->sAllocator,0,0);` |
| 139385 |   35 | `	SyHashInit(&pClass->hDerived,&pVm->sAllocator,0,0);` |
| 139385 |   36 | `	SySetInit(&pClass->aInterface,&pVm->sAllocator,sizeof(ph7_class *));` |
| 139385 |   37 | `	SySetInit(&pClass->aTrait,&pVm->sAllocator,sizeof(ph7_class *));` |
| 139385 |   38 | `	pClass->nLine = nLine;` |
|      - |   39 | `	/* All done */` |
| 139385 |   40 | `	return pClass;` |
|  69695 |   41 |  |
|      - |   42 | `/*` |
|      - |   43 | ` * Allocate and initialize a new class attribute.` |
|      - |   44 | ` * Return a pointer to the class attribute on success. NULL otherwise.` |
|      - |   45 | ` */` |
|  71568 |   46 | `PH7_PRIVATE ph7_class_attr * PH7_NewClassAttr(ph7_vm *pVm,const SyString *pName,sxu32 nLine,sxi32 iProtection,sxi32 iFlags)` |
|      5 |   47 |  |
|      - |   48 | `	ph7_class_attr *pAttr;` |
|      - |   49 | `	char *zName;` |
|  71573 |   50 | `	pAttr = (ph7_class_attr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_attr));` |
|  71573 |   51 | `	if( pAttr == 0 ){` |
|    ! 0 |   52 | `		return 0;` |
|      - |   53 | `	}` |
|      - |   54 | `	/* Zero the structure */` |
|  71573 |   55 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|      - |   56 | `	/* Duplicate attribute name */` |
|  71573 |   57 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  71573 |   58 | `	if( zName == 0 ){` |
|    ! 0 |   59 | `		SyMemBackendPoolFree(&pVm->sAllocator,pAttr);` |
|    ! 0 |   60 | `		return 0;` |
|      - |   61 | `	}` |
|      - |   62 | `	/* Initialize fields */` |
|  71573 |   63 | `	SySetInit(&pAttr->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|  71573 |   64 | `	SySetInit(&pAttr->aUnionAlts,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|  71573 |   65 | `	SyStringInitFromBuf(&pAttr->sName,zName,pName->nByte);` |
|  71573 |   66 | `	pAttr->iProtection = iProtection;` |
|  71573 |   67 | `	pAttr->nIdx = SXU32_HIGH;` |
|  71573 |   68 | `	pAttr->iFlags = iFlags;` |
|  71573 |   69 | `	pAttr->nLine = nLine;` |
|  71573 |   70 | `	return pAttr;` |
|  35789 |   71 |  |
|      - |   72 | `/*` |
|      - |   73 | ` * Allocate and initialize a new class method.` |
|      - |   74 | ` * Return a pointer to the class method on success. NULL otherwise` |
|      - |   75 | ` * This function associate with the newly created method an automatically generated` |
|      - |   76 | ` * random unique name.` |
|      - |   77 | ` */` |
| 270126 |   78 | `PH7_PRIVATE ph7_class_method * PH7_NewClassMethod(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,sxu32 nLine,` |
|      - |   79 | `	sxi32 iProtection,sxi32 iFlags,sxi32 iFuncFlags)` |
|      5 |   80 |  |
|      - |   81 | `	ph7_class_method *pMeth;` |
|      - |   82 | `	SyHashEntry *pEntry;` |
|      - |   83 | `	SyString *pNamePtr;` |
|      - |   84 | `	char zSalt[10];` |
|      - |   85 | `	char *zName;` |
|      - |   86 | `	sxu32 nByte;` |
|      - |   87 | `	/* Allocate a new class method instance */` |
| 270131 |   88 | `	pMeth = (ph7_class_method *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_method));` |
| 270131 |   89 | `	if( pMeth == 0 ){` |
|    ! 0 |   90 | `		return 0;` |
|      - |   91 | `	}` |
|      - |   92 | `	/* Zero the structure */` |
| 270131 |   93 | `	SyZero(pMeth,sizeof(ph7_class_method));` |
|      - |   94 | `	/* Check for an already installed method with the same name */` |
| 270131 |   95 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
| 270131 |   96 | `	if( pEntry == 0 ){` |
|      - |   97 | `		/* Associate an unique VM name to this method */` |
| 270129 |   98 | `		nByte = sizeof(zSalt) + pName->nByte + SyStringLength(&pClass->sName)+sizeof(char)*7/*[[__'\0'*/;` |
| 270129 |   99 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,nByte);` |
| 270129 |  100 | `		if( zName == 0 ){` |
|    ! 0 |  101 | `			SyMemBackendPoolFree(&pVm->sAllocator,pMeth);` |
|    ! 0 |  102 | `			return 0;` |
|      - |  103 | `		}` |
| 270129 |  104 | `		pNamePtr = &pMeth->sVmName;` |
|      - |  105 | `		/* Generate a random string */` |
| 270129 |  106 | `		PH7_VmRandomString(&(*pVm),zSalt,sizeof(zSalt));` |
| 270129 |  107 | `		pNamePtr->nByte = SyBufferFormat(zName,nByte,"[__%z@%z_%.*s]",&pClass->sName,pName,sizeof(zSalt),zSalt);` |
| 270129 |  108 | `		pNamePtr->zString = zName;` |
| 135067 |  109 | `	}else{` |
|      - |  110 | `		/* Method is condidate for 'overloading' */` |
|      3 |  111 | `		ph7_class_method *pCurrent = (ph7_class_method *)pEntry->pUserData;` |
|      3 |  112 | `		pNamePtr = &pMeth->sVmName;` |
|      - |  113 | `		/* Use the same VM name */` |
|      3 |  114 | `		SyStringDupPtr(pNamePtr,&pCurrent->sVmName);` |
|      3 |  115 | `		zName = (char *)pNamePtr->zString;` |
|      - |  116 | `	}` |
| 270131 |  117 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|     32 |  118 | `		if( (pName->nByte == sizeof("__construct") - 1 && SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1 ) == 0)` |
|     21 |  119 | `			\|\| (pName->nByte == sizeof("__destruct") - 1 && SyMemcmp(pName->zString,"__destruct",sizeof("__destruct") - 1 ) == 0)` |
|     22 |  120 | `			\|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|      - |  121 | `				/* Switch to public visibility when dealing with constructor/destructor */` |
|      5 |  122 | `				iProtection = PH7_CLASS_PROT_PUBLIC;` |
|      2 |  123 | `		}` |
|     12 |  124 | `	}` |
|      - |  125 | `	/* Initialize method fields */` |
| 270133 |  126 | `	pMeth->iProtection = iProtection;` |
| 270133 |  127 | `	pMeth->iFlags = iFlags;` |
| 270133 |  128 | `	pMeth->nLine = nLine;` |
| 405198 |  129 | `	PH7_VmInitFuncState(&(*pVm),&pMeth->sFunc,&zName[sizeof(char)*4/*[__@*/+SyStringLength(&pClass->sName)],` |
| 270128 |  130 | `		pName->nByte,iFuncFlags\|VM_FUNC_CLASS_METHOD,pClass);` |
| 270133 |  131 | `	return pMeth;` |
| 135070 |  132 |  |
|      - |  133 | `/*` |
|      - |  134 | ` * Check if the given name have a class method associated with it.` |
|      - |  135 | ` * Return the desired method [i.e: ph7_class_method instance] on success. NULL otherwise.` |
|      - |  136 | ` */` |
| 173624 |  137 | `PH7_PRIVATE ph7_class_method * PH7_ClassExtractMethod(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|      5 |  138 |  |
|      - |  139 | `	SyHashEntry *pEntry;` |
|      - |  140 | `	/* Perform a hash lookup */` |
| 173629 |  141 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)zName,nByte);` |
| 173629 |  142 | `	if( pEntry == 0 ){` |
|      - |  143 | `		/* No such entry */` |
|   4247 |  144 | `		return 0;` |
|      - |  145 | `	}` |
|      - |  146 | `	/* Point to the desired method */` |
| 169387 |  147 | `	return (ph7_class_method *)pEntry->pUserData;` |
|  86817 |  148 |  |
|      - |  149 | `/*` |
|      - |  150 | ` * Check if the given name is a class attribute.` |
|      - |  151 | ` * Return the desired attribute [i.e: ph7_class_attr instance] on success.NULL otherwise.` |
|      - |  152 | ` */` |
|  71638 |  153 | `PH7_PRIVATE ph7_class_attr * PH7_ClassExtractAttribute(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|      5 |  154 |  |
|      - |  155 | `	SyHashEntry *pEntry;` |
|      - |  156 | `	/* Perform a hash lookup */` |
|  71643 |  157 | `	pEntry = SyHashGet(&pClass->hAttr,(const void *)zName,nByte);` |
|  71643 |  158 | `	if( pEntry == 0 ){` |
|      - |  159 | `		/* No such entry */` |
|  71497 |  160 | `		return 0;` |
|      - |  161 | `	}` |
|      - |  162 | `	/* Point to the desierd method */` |
|    151 |  163 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|  35824 |  164 |  |
|      - |  165 | `/*` |
|      - |  166 | ` * Install a class attribute in the corresponding container.` |
|      - |  167 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|      - |  168 | ` */` |
|  71568 |  169 | `PH7_PRIVATE sxi32 PH7_ClassInstallAttr(ph7_class *pClass,ph7_class_attr *pAttr)` |
|      5 |  170 |  |
|  71573 |  171 | `	SyString *pName = &pAttr->sName;` |
|      - |  172 | `	sxi32 rc;` |
|      - |  173 | `	/* Remember where this attribute was originally declared so that later` |
|      - |  174 | `	 * inheritance/trait copies still know the declaring class (needed for` |
|      - |  175 | `	 * PHP-compatible error messages on typed properties). */` |
|  71573 |  176 | `	if( pAttr->pDeclClass == 0 ){` |
|  71573 |  177 | `		pAttr->pDeclClass = pClass;` |
|  35784 |  178 | `	}` |
|  71573 |  179 | `	rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|  71573 |  180 | `	return rc;` |
|      5 |  181 |  |
|      - |  182 | `/*` |
|      - |  183 | ` * Install a class method in the corresponding container.` |
|      - |  184 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|      - |  185 | ` */` |
| 270114 |  186 | `PH7_PRIVATE sxi32 PH7_ClassInstallMethod(ph7_class *pClass,ph7_class_method *pMeth)` |
|      5 |  187 |  |
| 270119 |  188 | `	SyString *pName = &pMeth->sFunc.sName;` |
|      - |  189 | `	sxi32 rc;` |
| 270119 |  190 | `	rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 270119 |  191 | `	return rc;` |
|      5 |  192 |  |
|      - |  193 | `/*` |
|      - |  194 | ` * Method-override compatibility (variance) checking.` |
|      - |  195 | ` *` |
|      - |  196 | ` * PHP rejects an override whose signature is incompatible with the parent's:` |
|      - |  197 | ` * return types are covariant (child may only narrow), parameter types are` |
|      - |  198 | ` * contravariant (child may only widen), and a child may not add a required` |
|      - |  199 | ` * parameter. We add the diagnostic — but conservatively: PHL must keep running` |
|      - |  200 | ` * valid PHP, so the comparator below is SKIP-BY-DEFAULT. It flags only cases that` |
|      - |  201 | ` * are unambiguously invalid and silently accepts anything subtle (unions,` |
|      - |  202 | ` * intersections, pseudo-types, self/parent/static, object, unresolved classes,` |
|      - |  203 | ` * or a missing type), so it can never reject valid code.` |
|      - |  204 | ` */` |
|      - |  205 | `#define OVT_NONE   0  /* no declared type */` |
|      - |  206 | `#define OVT_SCALAR 1  /* a concrete invariant scalar: int/float/string/bool/array */` |
|      - |  207 | `#define OVT_CLASS  2  /* a real, already-loaded class/interface */` |
|      - |  208 | `#define OVT_SKIP   3  /* union/intersection/pseudo/self/object/unresolved — never flag */` |
|      - |  209 |  |
|      - |  210 | `/*` |
|      - |  211 | ` * Classify one declared type (nType + class name + union flag) for override` |
|      - |  212 | ` * comparison. On OVT_CLASS, *ppClass receives the resolved class. Class names are` |
|      - |  213 | ` * resolved by a direct, autoload-free hClass lookup: a miss (forward reference,` |
|      - |  214 | ` * namespaced, or not-yet-loaded) yields OVT_SKIP, which the caller accepts.` |
|      - |  215 | ` */` |
|    128 |  216 | `static int OoClassifyOverrideType(ph7_vm *pVm, sxu32 nType, const SyString *pClass,` |
|      - |  217 | `	int bUnion, ph7_class **ppClass)` |
|      4 |  218 |  |
|    132 |  219 | `	*ppClass = 0;` |
|    132 |  220 | `	if( bUnion ){` |
|      3 |  221 | `		return OVT_SKIP; /* union/intersection — full lattice, skip */` |
|      - |  222 | `	}` |
|    130 |  223 | `	if( nType == 0 ){` |
|     55 |  224 | `		return OVT_NONE; /* no declared type */` |
|      - |  225 | `	}` |
|     77 |  226 | `	if( nType == SXU32_HIGH ){` |
|      - |  227 | `		/* A class name OR a pseudo-type stored as a name atom. Skip every pseudo` |
|      - |  228 | `		 * (incl. self/parent/static, which are context-relative). */` |
|      - |  229 | `		static const struct { const char *z; sxu32 n; } aPseudo[] = {` |
|      - |  230 | `			{"mixed",5}, {"never",5}, {"iterable",8}, {"callable",8}, {"true",4},` |
|      - |  231 | `			{"false",5}, {"self",4}, {"parent",6}, {"static",6}` |
|      - |  232 | `		};` |
|     18 |  233 | `		const char *z = pClass->zString;` |
|     18 |  234 | `		sxu32 n = pClass->nByte;` |
|      - |  235 | `		SyHashEntry *pE;` |
|      - |  236 | `		sxu32 i;` |
|    154 |  237 | `		for( i = 0; i < SX_ARRAYSIZE(aPseudo); i++ ){` |
|    142 |  238 | `			if( n == aPseudo[i].n && SyStrnmicmp(z,aPseudo[i].z,n) == 0 ){` |
|      5 |  239 | `				return OVT_SKIP;` |
|      - |  240 | `			}` |
|     70 |  241 | `		}` |
|     14 |  242 | `		pE = SyHashGet(&pVm->hClass,(const void *)z,n);` |
|     14 |  243 | `		if( pE == 0 ){` |
|    ! 0 |  244 | `			return OVT_SKIP; /* not loaded / forward ref / namespaced — accept */` |
|      - |  245 | `		}` |
|     14 |  246 | `		*ppClass = (ph7_class *)pE->pUserData;` |
|     14 |  247 | `		return OVT_CLASS;` |
|      - |  248 | `	}` |
|     70 |  249 | `	if( nType == MEMOBJ_STRING \|\| nType == MEMOBJ_INT \|\| nType == MEMOBJ_REAL` |
|     27 |  250 | `	 \|\| nType == MEMOBJ_BOOL \|\| nType == MEMOBJ_HASHMAP ){` |
|     36 |  251 | `		return OVT_SCALAR;` |
|      - |  252 | `	}` |
|      - |  253 | `	/* MEMOBJ_OBJ (object — subtypes against classes), MEMOBJ_VOID/NULL/RES,` |
|      - |  254 | `	 * or anything unexpected: skip. */` |
|     26 |  255 | `	return OVT_SKIP;` |
|     68 |  256 |  |
|      - |  257 |  |
|      - |  258 | `/*` |
|      - |  259 | ` * A declared type normalized for override comparison: the raw type code, the` |
|      - |  260 | ` * class-name string (when a class), and the union/nullable flags. Extracted once` |
|      - |  261 | ` * from each side so the comparator takes two of these instead of eight scalars.` |
|      - |  262 | ` */` |
|      - |  263 | `typedef struct OvType OvType;` |
|      - |  264 | `struct OvType {` |
|      - |  265 | `	sxu32 nType;` |
|      - |  266 | `	const SyString *pClass;` |
|      - |  267 | `	int bUnion;` |
|      - |  268 | `	int bNullable;` |
|      - |  269 | `};` |
|     96 |  270 | `static OvType OoTypeFromReturn(ph7_vm_func *pF)` |
|      4 |  271 |  |
|      - |  272 | `	OvType t;` |
|    100 |  273 | `	t.nType = pF->nReturnType;` |
|    100 |  274 | `	t.pClass = &pF->sReturnClass;` |
|    100 |  275 | `	t.bUnion = SySetUsed(&pF->aReturnUnion) > 0;` |
|    100 |  276 | `	t.bNullable = (pF->iFlags & VM_FUNC_RETURN_NULLABLE) != 0;` |
|    100 |  277 | `	return t;` |
|      4 |  278 |  |
|     32 |  279 | `static OvType OoTypeFromArg(ph7_vm_func_arg *pA)` |
|      3 |  280 |  |
|      - |  281 | `	OvType t;` |
|     35 |  282 | `	t.nType = pA->nType;` |
|     35 |  283 | `	t.pClass = &pA->sClass;` |
|     35 |  284 | `	t.bUnion = (pA->iFlags & VM_FUNC_ARG_UNION) != 0;` |
|     35 |  285 | `	t.bNullable = (pA->iFlags & VM_FUNC_ARG_NULLABLE) != 0;` |
|     35 |  286 | `	return t;` |
|      3 |  287 |  |
|      - |  288 | `/*` |
|      - |  289 | ` * Return TRUE if the child type is an unambiguously-invalid override of the` |
|      - |  290 | ` * parent type. bCovariant=1 for a return type (child must be ⊆ parent),` |
|      - |  291 | ` * 0 for a parameter (child must be ⊇ parent). Returns FALSE (accept) on any` |
|      - |  292 | ` * skipped/ambiguous shape.` |
|      - |  293 | ` */` |
|     64 |  294 | `static int OoOverrideTypeBad(ph7_vm *pVm, OvType parent, OvType child, int bCovariant)` |
|      4 |  295 |  |
|      - |  296 | `	ph7_class *pParentCls, *pChildCls;` |
|     68 |  297 | `	int kP = OoClassifyOverrideType(pVm, parent.nType, parent.pClass, parent.bUnion, &pParentCls);` |
|     68 |  298 | `	int kC = OoClassifyOverrideType(pVm, child.nType, child.pClass, child.bUnion, &pChildCls);` |
|     68 |  299 | `	if( kP == OVT_SKIP \|\| kC == OVT_SKIP ){` |
|     18 |  300 | `		return 0; /* ambiguous shape — conservatively accept */` |
|      - |  301 | `	}` |
|      - |  302 | `	/* A missing type is the TOP type. covariant (return): a concrete child is a` |
|      - |  303 | `	 * subtype of top, fine; a top child over a concrete parent WIDENS → bad.` |
|      - |  304 | `	 * contravariant (param): a top child is a supertype of anything, fine; a` |
|      - |  305 | `	 * concrete child over a top parent NARROWS → bad. (A union/intersection child` |
|      - |  306 | `	 * already fell into OVT_SKIP above, so a flagged child here is scalar/class.) */` |
|     52 |  307 | `	if( kP == OVT_NONE \|\| kC == OVT_NONE ){` |
|     31 |  308 | `		if( bCovariant && kC == OVT_NONE && kP != OVT_NONE ) return 1;` |
|     31 |  309 | `		if( !bCovariant && kP == OVT_NONE && kC != OVT_NONE ) return 1;` |
|     31 |  310 | `		return 0;` |
|      - |  311 | `	}` |
|      - |  312 | `	/* Nullability: a covariant return may not ADD null; a contravariant param may` |
|      - |  313 | `	 * not REMOVE null. */` |
|     23 |  314 | `	if( bCovariant ){` |
|     10 |  315 | `		if( child.bNullable && !parent.bNullable ) return 1;` |
|      6 |  316 | `	}else{` |
|     14 |  317 | `		if( parent.bNullable && !child.bNullable ) return 1;` |
|      - |  318 | `	}` |
|     23 |  319 | `	if( kP == OVT_SCALAR && kC == OVT_SCALAR ){` |
|      - |  320 | `		/* Scalars are invariant — they must match exactly. */` |
|     16 |  321 | `		return (parent.nType != child.nType) ? 1 : 0;` |
|      - |  322 | `	}` |
|      8 |  323 | `	if( kP == OVT_CLASS && kC == OVT_CLASS ){` |
|      8 |  324 | `		if( bCovariant ){` |
|      3 |  325 | `			return PH7_VmInstanceOf(pChildCls, pParentCls) ? 0 : 1;  /* child ⊆ parent */` |
|      - |  326 | `		}` |
|      6 |  327 | `		return PH7_VmInstanceOf(pParentCls, pChildCls) ? 0 : 1;      /* child ⊇ parent */` |
|      - |  328 | `	}` |
|      - |  329 | `	/* One scalar and one class — disjoint. */` |
|    ! 0 |  330 | `	return 1;` |
|     36 |  331 |  |
|      - |  332 |  |
|      - |  333 | `/*` |
|      - |  334 | ` * Check a child method's signature against the parent method it overrides.` |
|      - |  335 | ` * Emits a PHP-style "Declaration of … must be compatible …" fatal on a clear` |
|      - |  336 | `` * incompatibility. `__construct` is exempt (PHP does not apply variance to it).`` |
|      - |  337 | ` */` |
|   3594 |  338 | `static sxi32 OoCheckOverrideCompat(ph7_gen_state *pGen, ph7_class *pBase, ph7_class *pSub,` |
|      - |  339 | `	ph7_class_method *pParent, ph7_class_method *pChild)` |
|      5 |  340 |  |
|   3599 |  341 | `	ph7_vm *pVm = pGen->pVm;` |
|   3599 |  342 | `	ph7_vm_func *pPF = &pParent->sFunc;` |
|   3599 |  343 | `	ph7_vm_func *pCF = &pChild->sFunc;` |
|   3599 |  344 | `	SyString *pMName = &pCF->sName;` |
|      - |  345 | `	ph7_vm_func_arg *aP, *aC;` |
|      - |  346 | `	sxu32 nPArg, nCArg, k;` |
|   3599 |  347 | `	int bBad = 0;` |
|   3594 |  348 | `	if( pMName->nByte == sizeof("__construct")-1` |
|   3579 |  349 | `	 && SyStrnmicmp(pMName->zString,"__construct",pMName->nByte) == 0 ){` |
|   3551 |  350 | `		return SXRET_OK;` |
|      - |  351 | `	}` |
|      - |  352 | `	/* Return type — covariant. */` |
|     52 |  353 | `	bBad = OoOverrideTypeBad(pVm, OoTypeFromReturn(pPF), OoTypeFromReturn(pCF), /* bCovariant */ 1);` |
|      - |  354 | `	/* Each overlapping parameter — contravariant. */` |
|     52 |  355 | `	nPArg = SySetUsed(&pPF->aArgs);` |
|     52 |  356 | `	nCArg = SySetUsed(&pCF->aArgs);` |
|     52 |  357 | `	aP = (ph7_vm_func_arg *)SySetBasePtr(&pPF->aArgs);` |
|     52 |  358 | `	aC = (ph7_vm_func_arg *)SySetBasePtr(&pCF->aArgs);` |
|     68 |  359 | `	for( k = 0; !bBad && k < nPArg && k < nCArg; k++ ){` |
|     19 |  360 | `		bBad = OoOverrideTypeBad(pVm, OoTypeFromArg(&aP[k]), OoTypeFromArg(&aC[k]), /* bCovariant */ 0);` |
|     11 |  361 | `	}` |
|      - |  362 | `	/* Parameter arity: the child must declare at least the parent's parameters and` |
|      - |  363 | `	 * may add only OPTIONAL ones — PHP rejects dropping any param (even an optional` |
|      - |  364 | `	 * one) or adding a required one. Skip the rule if either signature is variadic` |
|      - |  365 | `	 * (arity semantics differ). */` |
|     52 |  366 | `	if( !bBad ){` |
|     48 |  367 | `		int bVariadic = 0;` |
|     62 |  368 | `		for( k = 0; k < nPArg; k++ ){ if( aP[k].iFlags & VM_FUNC_ARG_VARIADIC ) bVariadic = 1; }` |
|     64 |  369 | `		for( k = 0; k < nCArg; k++ ){ if( aC[k].iFlags & VM_FUNC_ARG_VARIADIC ) bVariadic = 1; }` |
|     48 |  370 | `		if( !bVariadic ){` |
|     48 |  371 | `			if( nCArg < nPArg ){` |
|    ! 0 |  372 | `				bBad = 1; /* dropped a parent parameter */` |
|    ! 0 |  373 | `			}else{` |
|     50 |  374 | `				for( k = nPArg; k < nCArg; k++ ){` |
|      3 |  375 | `					if( SySetUsed(&aC[k].aByteCode) == 0 ){ bBad = 1; break; } /* new required */` |
|      2 |  376 | `				}` |
|      - |  377 | `			}` |
|     22 |  378 | `		}` |
|     22 |  379 | `	}` |
|     52 |  380 | `	if( bBad ){` |
|      8 |  381 | `		sxi32 rc = PH7_GenCompileError(&(*pGen),E_ERROR,pChild->nLine,` |
|      - |  382 | `			"Declaration of %z::%z() must be compatible with %z::%z()",` |
|      2 |  383 | `			&pSub->sName,pMName,&pBase->sName,&pParent->sFunc.sName);` |
|      6 |  384 | `		if( rc == SXERR_ABORT ){` |
|    ! 0 |  385 | `			return SXERR_ABORT;` |
|      - |  386 | `		}` |
|      2 |  387 | `	}` |
|     52 |  388 | `	return SXRET_OK;` |
|   1802 |  389 |  |
|      - |  390 | `/*` |
|      - |  391 | ` * Perform an inheritance operation.` |
|      - |  392 | ` * According to the PHP language reference manual` |
|      - |  393 | ` *  When you extend a class, the subclass inherits all of the public and protected methods` |
|      - |  394 | ` *  from the parent class. Unless a class Overwrites those methods, they will retain their original` |
|      - |  395 | ` *  functionality.` |
|      - |  396 | ` *  This is useful for defining and abstracting functionality, and permits the implementation` |
|      - |  397 | ` *  of additional functionality in similar objects without the need to reimplement all of the shared` |
|      - |  398 | ` *  functionality.` |
|      - |  399 | ` *  Example #1 Inheritance Example` |
|      - |  400 | ` * <?php` |
|      - |  401 | ` * class foo` |
|      - |  402 | ` * {` |
|      - |  403 | ` *   public function printItem($string)` |
|      - |  404 | ` *   {` |
|      - |  405 | ` *       echo 'Foo: ' . $string . PHP_EOL;` |
|      - |  406 | ` *   }` |
|      - |  407 | ` *` |
|      - |  408 | ` *   public function printPHP()` |
|      - |  409 | ` *   {` |
|      - |  410 | ` *       echo 'PHP is great.' . PHP_EOL;` |
|      - |  411 | ` *   }` |
|      - |  412 | ` * }` |
|      - |  413 | ` * class bar extends foo` |
|      - |  414 | ` * {` |
|      - |  415 | ` *   public function printItem($string)` |
|      - |  416 | ` *   {` |
|      - |  417 | ` *       echo 'Bar: ' . $string . PHP_EOL;` |
|      - |  418 | ` *   }` |
|      - |  419 | ` * }` |
|      - |  420 | ` * $foo = new foo();` |
|      - |  421 | ` * $bar = new bar();` |
|      - |  422 | ` * $foo->printItem('baz'); // Output: 'Foo: baz'` |
|      - |  423 | ` * $foo->printPHP();       // Output: 'PHP is great'` |
|      - |  424 | ` * $bar->printItem('baz'); // Output: 'Bar: baz'` |
|      - |  425 | ` * $bar->printPHP();       // Output: 'PHP is great'` |
|      - |  426 | ` *` |
|      - |  427 | ` * This function return SXRET_OK if the inheritance operation was successfully performed.` |
|      - |  428 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|      - |  429 | ` * error message.` |
|      - |  430 | ` */` |
|  74508 |  431 | `PH7_PRIVATE sxi32 PH7_ClassInherit(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase)` |
|      5 |  432 |  |
|      - |  433 | `	ph7_class_method *pMeth;` |
|      - |  434 | `	ph7_class_attr *pAttr;` |
|      - |  435 | `	SyHashEntry *pEntry;` |
|      - |  436 | `	SyString *pName;` |
|      - |  437 | `	sxi32 rc;` |
|      - |  438 | `	/* Install in the derived hashtable */` |
|  74513 |  439 | `	rc = SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|  74513 |  440 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  441 | `		return rc;` |
|      - |  442 | `	}` |
|      - |  443 | `	/* readonly class inheritance (PHP 8.2): a readonly class may only extend a` |
|      - |  444 | `	 * readonly class, and a non-readonly class may not extend a readonly one. */` |
|  74513 |  445 | `	if( (pBase->iFlags & PH7_CLASS_READONLY) != (pSub->iFlags & PH7_CLASS_READONLY) ){` |
|      5 |  446 | `		if( pBase->iFlags & PH7_CLASS_READONLY ){` |
|      4 |  447 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|      - |  448 | `				"Non-readonly class %z cannot extend readonly class %z",` |
|      1 |  449 | `				&pSub->sName,&pBase->sName);` |
|      2 |  450 | `		}else{` |
|      4 |  451 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|      - |  452 | `				"Readonly class %z cannot extend non-readonly class %z",` |
|      1 |  453 | `				&pSub->sName,&pBase->sName);` |
|      - |  454 | `		}` |
|      5 |  455 | `		if( rc == SXERR_ABORT ){` |
|    ! 0 |  456 | `			return SXERR_ABORT;` |
|      - |  457 | `		}` |
|      2 |  458 | `	}` |
|      - |  459 | `	/* Copy public/protected attributes from the base class */` |
|  74513 |  460 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
| 520953 |  461 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|      - |  462 | `		/* Make sure the private attributes are not redeclared in the subclass */` |
| 446445 |  463 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
| 446445 |  464 | `		pName = &pAttr->sName;` |
| 446445 |  465 | `		if( (pEntry = SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|      9 |  466 | `			if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_FINAL))` |
|      6 |  467 | `				== (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_FINAL) ){` |
|      - |  468 | `				/* Cannot override a final class constant (PHP 8.1). Report the` |
|      - |  469 | `				 * class that originally declared it (pDeclClass) rather than the` |
|      - |  470 | `				 * immediate base, so a multi-level chain matches PHP. */` |
|      3 |  471 | `				ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pBase;` |
|      4 |  472 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_attr *)pEntry->pUserData)->nLine,` |
|      - |  473 | `					"%z::%z cannot override final constant %z::%z",` |
|      1 |  474 | `					&pSub->sName,pName,&pOwner->sName,pName);` |
|      3 |  475 | `				if( rc == SXERR_ABORT ){` |
|    ! 0 |  476 | `					return SXERR_ABORT;` |
|      - |  477 | `				}` |
|      1 |  478 | `			}` |
|      9 |  479 | `			if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE &&` |
|      2 |  480 | `				((ph7_class_attr *)pEntry->pUserData)->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|      - |  481 | `					/* Cannot redeclare private attribute */` |
|      4 |  482 | `					PH7_GenCompileError(&(*pGen),E_WARNING,((ph7_class_attr *)pEntry->pUserData)->nLine,` |
|      - |  483 | `						"Private attribute '%z::%z' redeclared inside child class '%z'",` |
|      1 |  484 | `						&pBase->sName,pName,&pSub->sName);` |
|      - |  485 |  |
|      1 |  486 | `			}` |
|      9 |  487 | `			continue;` |
|      - |  488 | `		}` |
|      - |  489 | `		/* Install the attribute */` |
| 446439 |  490 | `		if( pAttr->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
| 446435 |  491 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
| 446435 |  492 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  493 | `				return rc;` |
|      - |  494 | `			}` |
| 223215 |  495 | `		}` |
|      5 |  496 | `	}` |
|  74513 |  497 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
| 744263 |  498 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|      - |  499 | `		/* Make sure the private/final methods are not redeclared in the subclass */` |
| 669755 |  500 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
| 669755 |  501 | `		pName = &pMeth->sFunc.sName;` |
| 669755 |  502 | `		if( (pEntry = SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|   3603 |  503 | `			 if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|      - |  504 | `				/* Cannot Overwrite final method */` |
|      7 |  505 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_method *)pEntry->pUserData)->nLine,` |
|      - |  506 | `					"Cannot Overwrite final method '%z:%z' inside child class '%z'",` |
|      2 |  507 | `					&pBase->sName,pName,&pSub->sName);` |
|      5 |  508 | `				if( rc == SXERR_ABORT ){` |
|    ! 0 |  509 | `					return SXERR_ABORT;` |
|      - |  510 | `				}` |
|      3 |  511 | `			}else{` |
|      - |  512 | `				/* Check the override's signature is compatible with the parent's. */` |
|   5396 |  513 | `				rc = OoCheckOverrideCompat(&(*pGen),pBase,pSub,pMeth,` |
|   3594 |  514 | `					(ph7_class_method *)pEntry->pUserData);` |
|   3599 |  515 | `				if( rc == SXERR_ABORT ){` |
|    ! 0 |  516 | `					return SXERR_ABORT;` |
|      - |  517 | `				}` |
|      - |  518 | `			}` |
|   3603 |  519 | `			continue;` |
|      - |  520 | `		}` |
|      - |  521 | `		/* Install the method */` |
| 666157 |  522 | `		if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
| 666155 |  523 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 666155 |  524 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  525 | `				return rc;` |
|      - |  526 | `			}` |
| 333075 |  527 | `		}` |
|      5 |  528 | `	}` |
|      - |  529 | `	/* Mark as subclass */` |
|  74513 |  530 | `	pSub->pBase = pBase;` |
|      - |  531 | `	/* All done */` |
|  74513 |  532 | `	return SXRET_OK;` |
|  37259 |  533 |  |
|      - |  534 | `/*` |
|      - |  535 | ` * Apply a trait to a class: copy all methods and attributes from the trait` |
|      - |  536 | ` * into the target class. Unlike inheritance, traits copy ALL members including` |
|      - |  537 | ` * private ones. Members already defined in the class take precedence.` |
|      - |  538 | ` */` |
|     46 |  539 | `PH7_PRIVATE sxi32 PH7_ClassUseTrait(ph7_gen_state *pGen,ph7_class *pClass,ph7_class *pTrait)` |
|      5 |  540 |  |
|      - |  541 | `	ph7_class_method *pMeth;` |
|      - |  542 | `	ph7_class_attr *pAttr;` |
|      - |  543 | `	SyHashEntry *pEntry;` |
|      - |  544 | `	SyString *pName;` |
|      - |  545 | `	sxi32 rc;` |
|      - |  546 | `	/* Detect cyclic trait composition (e.g. trait A { use B; } trait B { use A; }) */` |
|     51 |  547 | `	if( pTrait->iFlags & PH7_CLASS_TRAIT_VISITING ){` |
|    ! 0 |  548 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|    ! 0 |  549 | `			"Trait circular reference detected: %z is already being applied",&pTrait->sName);` |
|    ! 0 |  550 | `		if( rc == SXERR_ABORT ){` |
|    ! 0 |  551 | `			return SXERR_ABORT;` |
|      - |  552 | `		}` |
|    ! 0 |  553 | `		return SXRET_OK;` |
|      - |  554 | `	}` |
|     51 |  555 | `	pTrait->iFlags \|= PH7_CLASS_TRAIT_VISITING;` |
|     51 |  556 | `	rc = SXRET_OK;` |
|      - |  557 | `	/* Copy attributes from the trait */` |
|     51 |  558 | `	SyHashResetLoopCursor(&pTrait->hAttr);` |
|     67 |  559 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hAttr)) != 0 ){` |
|      - |  560 | `		SyHashEntry *pExisting;` |
|     20 |  561 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     20 |  562 | `		pName = &pAttr->sName;` |
|     20 |  563 | `		pExisting = SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte);` |
|     20 |  564 | `		if( pExisting != 0 ){` |
|      - |  565 | `			/* Attribute already exists. Check if it came from another trait` |
|      - |  566 | `			 * and whether the definitions are compatible (same defaults).` |
|      - |  567 | `			 */` |
|      - |  568 | `			ph7_class **apUsedTraits;` |
|      - |  569 | `			sxu32 nUsed,k;` |
|      6 |  570 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|      6 |  571 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|      6 |  572 | `			for(k = 0; k < nUsed; k++){` |
|      - |  573 | `				ph7_class_attr *pOther;` |
|      3 |  574 | `				pOther = PH7_ClassExtractAttribute(apUsedTraits[k],pName->zString,pName->nByte);` |
|      3 |  575 | `				if( pOther ){` |
|      - |  576 | `					/* Two traits define the same property — check if defaults differ */` |
|      3 |  577 | `					ph7_class_attr *pClassAttr = (ph7_class_attr *)pExisting->pUserData;` |
|      4 |  578 | `					if( SySetUsed(&pAttr->aByteCode) != SySetUsed(&pClassAttr->aByteCode) \|\|` |
|      3 |  579 | `						(SySetUsed(&pAttr->aByteCode) > 0 &&` |
|      3 |  580 | `						 SyMemcmp(SySetBasePtr(&pAttr->aByteCode),SySetBasePtr(&pClassAttr->aByteCode),` |
|      3 |  581 | `							SySetUsed(&pAttr->aByteCode) * SySetElemSize(&pAttr->aByteCode)) != 0) ){` |
|      4 |  582 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine,` |
|      - |  583 | `							"%z and %z define the same property ($%z) in the composition of %z. "` |
|      - |  584 | `							"However, the definition differs and is considered incompatible",` |
|      2 |  585 | `							&apUsedTraits[k]->sName,&pTrait->sName,pName,&pClass->sName);` |
|      3 |  586 | `						if( rc == SXERR_ABORT ){` |
|    ! 0 |  587 | `							goto cleanup;` |
|      - |  588 | `						}` |
|      1 |  589 | `					}` |
|      3 |  590 | `					break;` |
|      - |  591 | `				}` |
|    ! 0 |  592 | `			}` |
|      6 |  593 | `			continue;` |
|      - |  594 | `		}` |
|     16 |  595 | `		rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|     16 |  596 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  597 | `			goto cleanup;` |
|      - |  598 | `		}` |
|      4 |  599 | `	}` |
|      - |  600 | `	/* Copy methods from the trait */` |
|     51 |  601 | `	SyHashResetLoopCursor(&pTrait->hMethod);` |
|     93 |  602 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hMethod)) != 0 ){` |
|     46 |  603 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|     46 |  604 | `		pName = &pMeth->sFunc.sName;` |
|     46 |  605 | `		if( SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte) != 0 ){` |
|      - |  606 | `			/* Method already exists in the class. Check if it came from another trait` |
|      - |  607 | `			 * (unresolved conflict) vs being defined by the class itself.` |
|      - |  608 | `			 */` |
|      - |  609 | `			ph7_class **apUsedTraits;` |
|      - |  610 | `			sxu32 nUsed,k;` |
|     11 |  611 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|     11 |  612 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|     11 |  613 | `			for(k = 0; k < nUsed; k++){` |
|      3 |  614 | `				if( PH7_ClassExtractMethod(apUsedTraits[k],pName->zString,pName->nByte) != 0 ){` |
|      - |  615 | `					/* Two different traits define the same method with no resolution */` |
|      4 |  616 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|      - |  617 | `						"Trait method %z::%z has not been applied as %z::%z, "` |
|      - |  618 | `						"because of collision with %z::%z",` |
|      2 |  619 | `						&pTrait->sName,pName,` |
|      1 |  620 | `						&pClass->sName,pName,` |
|      2 |  621 | `						&apUsedTraits[k]->sName,pName);` |
|      3 |  622 | `					if( rc == SXERR_ABORT ){` |
|    ! 0 |  623 | `						goto cleanup;` |
|      - |  624 | `					}` |
|      3 |  625 | `					break;` |
|      - |  626 | `				}` |
|    ! 0 |  627 | `			}` |
|      - |  628 | `			/* Class-defined method takes precedence */` |
|     11 |  629 | `			continue;` |
|      - |  630 | `		}` |
|     38 |  631 | `		rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|     38 |  632 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  633 | `			goto cleanup;` |
|      - |  634 | `		}` |
|      4 |  635 | `	}` |
|      - |  636 | `	/* Record trait in the class */` |
|     51 |  637 | `	SySetPut(&pClass->aTrait,(const void *)&pTrait);` |
|     23 |  638 | `cleanup:` |
|      - |  639 | `	/* Always clear visiting flag, even on error paths */` |
|     51 |  640 | `	pTrait->iFlags &= ~PH7_CLASS_TRAIT_VISITING;` |
|     23 |  641 | `	SXUNUSED(pGen);` |
|     51 |  642 | `	return rc;` |
|     28 |  643 |  |
|      - |  644 | `/*` |
|      - |  645 | ` * Inherit an object interface from another object interface.` |
|      - |  646 | ` * According to the PHP language reference manual.` |
|      - |  647 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|      - |  648 | ` *  must implement, without having to define how these methods are handled.` |
|      - |  649 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|      - |  650 | ` *  class, but without any of the methods having their contents defined.` |
|      - |  651 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|      - |  652 | ` *` |
|      - |  653 | ` * This function return SXRET_OK if the interface inheritance operation was successfully performed.` |
|      - |  654 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|      - |  655 | ` * error message.` |
|      - |  656 | ` */` |
|  10628 |  657 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceInherit(ph7_class *pSub,ph7_class *pBase)` |
|      5 |  658 |  |
|      - |  659 | `	ph7_class_method *pMeth;` |
|      - |  660 | `	ph7_class_attr *pAttr;` |
|      - |  661 | `	SyHashEntry *pEntry;` |
|      - |  662 | `	SyString *pName;` |
|      - |  663 | `	sxi32 rc;` |
|      - |  664 | `	/* Install in the derived hashtable */` |
|  10633 |  665 | `	SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|  10633 |  666 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
|      - |  667 | `	/* Copy constants */` |
|  15949 |  668 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|      - |  669 | `		/* Make sure the constants are not redeclared in the subclass */` |
|      3 |  670 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      3 |  671 | `		pName = &pAttr->sName;` |
|      3 |  672 | `		if( SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte) == 0 ){` |
|      - |  673 | `			/* Install the constant in the subclass */` |
|      3 |  674 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|      3 |  675 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  676 | `				return rc;` |
|      - |  677 | `			}` |
|      1 |  678 | `		}` |
|      1 |  679 | `	}` |
|  10633 |  680 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
|      - |  681 | `	/* Copy methods signature */` |
|  19529 |  682 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|      - |  683 | `		/* Make sure the method are not redeclared in the subclass */` |
|   3587 |  684 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   3587 |  685 | `		pName = &pMeth->sFunc.sName;` |
|   3587 |  686 | `		if( SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte) == 0 ){` |
|      - |  687 | `			/* Install the method */` |
|   3587 |  688 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|   3587 |  689 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  690 | `				return rc;` |
|      - |  691 | `			}` |
|   1791 |  692 | `		}` |
|      5 |  693 | `	}` |
|      - |  694 | `	/* Mark as subclass */` |
|  10633 |  695 | `	pSub->pBase = pBase;` |
|      - |  696 | `	/* All done */` |
|  10633 |  697 | `	return SXRET_OK;` |
|   5319 |  698 |  |
|      - |  699 | `/*` |
|      - |  700 | ` * Implements an object interface in the given main class.` |
|      - |  701 | ` * According to the PHP language reference manual.` |
|      - |  702 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|      - |  703 | ` *  must implement, without having to define how these methods are handled.` |
|      - |  704 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|      - |  705 | ` *  class, but without any of the methods having their contents defined.` |
|      - |  706 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|      - |  707 | ` *` |
|      - |  708 | ` * This function return SXRET_OK if the interface was successfully implemented.` |
|      - |  709 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|      - |  710 | ` * error message.` |
|      - |  711 | ` */` |
|  95822 |  712 | `PH7_PRIVATE sxi32 PH7_ClassImplement(ph7_class *pMain,ph7_class *pInterface)` |
|      5 |  713 |  |
|      - |  714 | `	ph7_class_attr *pAttr;` |
|      - |  715 | `	SyHashEntry *pEntry;` |
|      - |  716 | `	SyString *pName;` |
|      - |  717 | `	sxi32 rc;` |
|      - |  718 | `	/* First off,copy all constants declared inside the interface */` |
|  95827 |  719 | `	SyHashResetLoopCursor(&pInterface->hAttr);` |
| 143744 |  720 | `	while((pEntry = SyHashGetNextEntry(&pInterface->hAttr)) != 0 ){` |
|      - |  721 | `		/* Point to the constant declaration */` |
|      7 |  722 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      7 |  723 | `		pName = &pAttr->sName;` |
|      - |  724 | `		/* Make sure the attribute is not redeclared in the main class */` |
|      7 |  725 | `		if( SyHashGet(&pMain->hAttr,pName->zString,pName->nByte) == 0 ){` |
|      - |  726 | `			/* Install the attribute */` |
|      7 |  727 | `			rc = SyHashInsert(&pMain->hAttr,pName->zString,pName->nByte,pAttr);` |
|      7 |  728 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  729 | `				return rc;` |
|      - |  730 | `			}` |
|      3 |  731 | `		}` |
|      1 |  732 | `	}` |
|      - |  733 | `	/* Install in the interface container */` |
|  95827 |  734 | `	SySetPut(&pMain->aInterface,(const void *)&pInterface);` |
|      - |  735 | `	/* Install interface method stubs into the implementing class.` |
|      - |  736 | `	 * Methods already defined in the class take precedence (they satisfy` |
|      - |  737 | `	 * the interface contract). Stubs retain PH7_CLASS_ATTR_ABSTRACT so` |
|      - |  738 | `	 * the unified check in GenStateCheckAbstractMethods catches missing ones.` |
|      - |  739 | `	 */` |
|      - |  740 | `	{` |
|      - |  741 | `		ph7_class_method *pMeth;` |
|      - |  742 | `		SyHashEntry *pMEntry;` |
|      - |  743 | `		SyString *pMName;` |
|  95827 |  744 | `		SyHashResetLoopCursor(&pInterface->hMethod);` |
| 303488 |  745 | `		while((pMEntry = SyHashGetNextEntry(&pInterface->hMethod)) != 0 ){` |
| 159755 |  746 | `			pMeth = (ph7_class_method *)pMEntry->pUserData;` |
| 159755 |  747 | `			pMName = &pMeth->sFunc.sName;` |
| 159755 |  748 | `			if( SyHashGet(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte) == 0 ){` |
|     18 |  749 | `				rc = SyHashInsert(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte,pMeth);` |
|     18 |  750 | `				if( rc != SXRET_OK ){` |
|    ! 0 |  751 | `					return rc;` |
|      - |  752 | `				}` |
|      7 |  753 | `			}` |
|      5 |  754 | `		}` |
|      - |  755 | `	}` |
|  95827 |  756 | `	return SXRET_OK;` |
|  47916 |  757 |  |
|      - |  758 | `/*` |
|      - |  759 | ` * Create a class instance [i.e: Object in the PHP jargon] at run-time.` |
|      - |  760 | ` * The following function is called when an object is created at run-time` |
|      - |  761 | ` * typically when the PH7_OP_NEW/PH7_OP_CLONE instructions are executed.` |
|      - |  762 | ` * Notes on object creation.` |
|      - |  763 | ` *` |
|      - |  764 | ` * According to PHP language reference manual.` |
|      - |  765 | ` * To create an instance of a class, the new keyword must be used. An object will always` |
|      - |  766 | ` * be created unless the object has a constructor defined that throws an exception on error.` |
|      - |  767 | ` * Classes should be defined before instantiation (and in some cases this is a requirement).` |
|      - |  768 | ` * If a string containing the name of a class is used with new, a new instance of that class` |
|      - |  769 | ` * will be created. If the class is in a namespace, its fully qualified name must be used when` |
|      - |  770 | ` * doing this.` |
|      - |  771 | ` * Example #3 Creating an instance` |
|      - |  772 | ` * <?php` |
|      - |  773 | ` *  $instance = new SimpleClass();` |
|      - |  774 | ` *   // This can also be done with a variable:` |
|      - |  775 | ` * $className = 'Foo';` |
|      - |  776 | ` * $instance = new $className(); // Foo()` |
|      - |  777 | ` * ?>` |
|      - |  778 | ` * In the class context, it is possible to create a new object by new self and new parent.` |
|      - |  779 | ` * When assigning an already created instance of a class to a new variable, the new variable` |
|      - |  780 | ` * will access the same instance as the object that was assigned. This behaviour is the same` |
|      - |  781 | ` * when passing instances to a function. A copy of an already created object can be made by` |
|      - |  782 | ` * cloning it.` |
|      - |  783 | ` * Example #4 Object Assignment` |
|      - |  784 | ` * <?php` |
|      - |  785 | ` *  class SimpleClass(){` |
|      - |  786 | ` *    public $var;` |
|      - |  787 | ` *  };` |
|      - |  788 | ` *  $instance = new SimpleClass();` |
|      - |  789 | ` *  $assigned   =  $instance;` |
|      - |  790 | ` *  $reference  =& $instance;` |
|      - |  791 | ` *  $instance->var = '$assigned will have this value';` |
|      - |  792 | ` *  $instance = null; // $instance and $reference become null` |
|      - |  793 | ` *  var_dump($instance);` |
|      - |  794 | ` *  var_dump($reference);` |
|      - |  795 | ` *  var_dump($assigned);` |
|      - |  796 | ` * ?>` |
|      - |  797 | ` * The above example will output:` |
|      - |  798 | ` * NULL` |
|      - |  799 | ` * NULL` |
|      - |  800 | ` * object(SimpleClass)#1 (1) {` |
|      - |  801 | ` *  ["var"]=>` |
|      - |  802 | ` *    string(30) "$assigned will have this value"` |
|      - |  803 | ` * }` |
|      - |  804 | ` * Example #5 Creating new objects` |
|      - |  805 | ` * <?php` |
|      - |  806 | ` * class Test` |
|      - |  807 | ` * {` |
|      - |  808 | ` *   static public function getNew()` |
|      - |  809 | ` *   {` |
|      - |  810 | ` *       return new static;` |
|      - |  811 | ` *   }` |
|      - |  812 | ` * }` |
|      - |  813 | ` * class Child extends Test` |
|      - |  814 | ` * {}` |
|      - |  815 | ` * $obj1 = new Test();` |
|      - |  816 | ` * $obj2 = new $obj1;` |
|      - |  817 | ` * var_dump($obj1 !== $obj2);` |
|      - |  818 | ` * $obj3 = Test::getNew();` |
|      - |  819 | ` * var_dump($obj3 instanceof Test);` |
|      - |  820 | ` * $obj4 = Child::getNew();` |
|      - |  821 | ` * var_dump($obj4 instanceof Child);` |
|      - |  822 | ` * ?>` |
|      - |  823 | ` * The above example will output:` |
|      - |  824 | ` * bool(true)` |
|      - |  825 | ` * bool(true)` |
|      - |  826 | ` * bool(true)` |
|      - |  827 | ` * Note that Symisc Systems have introduced powerfull extension to` |
|      - |  828 | ` * OO subsystem. For example a class attribute may have any complex` |
|      - |  829 | ` * expression associated with it when declaring the attribute unlike` |
|      - |  830 | ` * the standard PHP engine which would allow a single value.` |
|      - |  831 | ` * Example:` |
|      - |  832 | ` *  class myClass{` |
|      - |  833 | ` *    public $var = 25<<1+foo()/bar();` |
|      - |  834 | ` *  };` |
|      - |  835 | ` * Refer to the official documentation for more information.` |
|      - |  836 | ` */` |
|   3098 |  837 | `static ph7_class_instance * NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|      5 |  838 |  |
|      - |  839 | `	ph7_class_instance *pThis;` |
|      - |  840 | `	/* Allocate a new instance */` |
|   3103 |  841 | `	pThis = (ph7_class_instance *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_instance));` |
|   3103 |  842 | `	if( pThis == 0 ){` |
|    ! 0 |  843 | `		return 0;` |
|      - |  844 | `	}` |
|      - |  845 | `	/* Zero the structure */` |
|   3103 |  846 | `	SyZero(pThis,sizeof(ph7_class_instance));` |
|      - |  847 | `	/* Initialize fields */` |
|   3103 |  848 | `	pThis->iRef = 1;` |
|   3103 |  849 | `	pThis->pVm = pVm;` |
|   3103 |  850 | `	pThis->pClass = pClass;` |
|      - |  851 | `	/* Assign a fresh monotonic object handle id (clones get their own, like PHP). */` |
|   3103 |  852 | `	pThis->nObjId = pVm->nNextObjId++;` |
|   3103 |  853 | `	SyHashInit(&pThis->hAttr,&pVm->sAllocator,0,0);` |
|   3103 |  854 | `	return pThis;` |
|   1554 |  855 |  |
|      - |  856 | `/*` |
|      - |  857 | ` * Wrapper around the NewClassInstance() function defined above.` |
|      - |  858 | ` * See the block comment above for more information.` |
|      - |  859 | ` */` |
|   3046 |  860 | `PH7_PRIVATE ph7_class_instance * PH7_NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|      5 |  861 |  |
|      - |  862 | `	ph7_class_instance *pNew;` |
|      - |  863 | `	sxi32 rc;` |
|   3051 |  864 | `	pNew = NewClassInstance(&(*pVm),&(*pClass));` |
|   3051 |  865 | `	if( pNew == 0 ){` |
|    ! 0 |  866 | `		return 0;` |
|      - |  867 | `	}` |
|      - |  868 | `	/* Associate a private VM frame with this class instance */` |
|   3051 |  869 | `	rc = PH7_VmCreateClassInstanceFrame(&(*pVm),pNew);` |
|   3051 |  870 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  871 | `		SyMemBackendPoolFree(&pVm->sAllocator,pNew);` |
|    ! 0 |  872 | `		return 0;` |
|      - |  873 | `	}` |
|   3051 |  874 | `	return pNew;` |
|   1528 |  875 |  |
|      - |  876 | `/*` |
|      - |  877 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon] attribute.` |
|      - |  878 | ` * This function never fail.` |
|      - |  879 | ` */` |
|   3004 |  880 | `static ph7_value * ExtractClassAttrValue(ph7_vm *pVm,VmClassAttr *pAttr)` |
|      5 |  881 |  |
|      - |  882 | `	/* Extract the value */` |
|      - |  883 | `	ph7_value *pValue;` |
|   3009 |  884 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|   3009 |  885 | `	return pValue;` |
|      5 |  886 |  |
|      - |  887 | `/*` |
|      - |  888 | ` * Perform a clone operation on a class instance [i.e: Object in the PHP jargon].` |
|      - |  889 | ` * The following function is called when an object is cloned at run-time` |
|      - |  890 | ` * typically when the PH7_OP_CLONE instruction is executed.` |
|      - |  891 | ` * Notes on object cloning.` |
|      - |  892 | ` *` |
|      - |  893 | ` * According to PHP language reference manual.` |
|      - |  894 | ` * Creating a copy of an object with fully replicated properties is not always the wanted behavior.` |
|      - |  895 | ` * A good example of the need for copy constructors. Another example is if your object holds a reference` |
|      - |  896 | ` * to another object which it uses and when you replicate the parent object you want to create` |
|      - |  897 | ` * a new instance of this other object so that the replica has its own separate copy.` |
|      - |  898 | ` * An object copy is created by using the clone keyword (which calls the object's __clone() method if possible).` |
|      - |  899 | ` * An object's __clone() method cannot be called directly.` |
|      - |  900 | ` * $copy_of_object = clone $object;` |
|      - |  901 | ` * When an object is cloned, PHP 5 will perform a shallow copy of all of the object's properties.` |
|      - |  902 | ` * Any properties that are references to other variables, will remain references.` |
|      - |  903 | ` * Once the cloning is complete, if a __clone() method is defined, then the newly created object's __clone() method` |
|      - |  904 | ` * will be called, to allow any necessary properties that need to be changed.` |
|      - |  905 | ` * Example #1 Cloning an object` |
|      - |  906 | ` * <?php` |
|      - |  907 | ` * class SubObject` |
|      - |  908 | ` * {` |
|      - |  909 | ` *   static $instances = 0;` |
|      - |  910 | ` *   public $instance;` |
|      - |  911 | ` *` |
|      - |  912 | ` *   public function __construct() {` |
|      - |  913 | ` *       $this->instance = ++self::$instances;` |
|      - |  914 | ` *   }` |
|      - |  915 | ` *` |
|      - |  916 | ` *   public function __clone() {` |
|      - |  917 | ` *       $this->instance = ++self::$instances;` |
|      - |  918 | ` *   }` |
|      - |  919 | ` * }` |
|      - |  920 | ` *` |
|      - |  921 | ` * class MyCloneable` |
|      - |  922 | ` * {` |
|      - |  923 | ` *   public $object1;` |
|      - |  924 | ` *   public $object2;` |
|      - |  925 | ` *` |
|      - |  926 | ` *   function __clone()` |
|      - |  927 | ` *   {` |
|      - |  928 | ` *       // Force a copy of this->object, otherwise` |
|      - |  929 | ` *       // it will point to same object.` |
|      - |  930 | ` *       $this->object1 = clone $this->object1;` |
|      - |  931 | ` *   }` |
|      - |  932 | ` * }` |
|      - |  933 | ` * $obj = new MyCloneable();` |
|      - |  934 | ` * $obj->object1 = new SubObject();` |
|      - |  935 | ` * $obj->object2 = new SubObject();` |
|      - |  936 | ` * $obj2 = clone $obj;` |
|      - |  937 | ` * print("Original Object:\n");` |
|      - |  938 | ` * print_r($obj);` |
|      - |  939 | ` * print("Cloned Object:\n");` |
|      - |  940 | ` * print_r($obj2);` |
|      - |  941 | ` * ?>` |
|      - |  942 | ` * The above example will output:` |
|      - |  943 | ` * Original Object:` |
|      - |  944 | ` * MyCloneable Object` |
|      - |  945 | ` * (` |
|      - |  946 | ` *   [object1] => SubObject Object` |
|      - |  947 | ` *       (` |
|      - |  948 | ` *           [instance] => 1` |
|      - |  949 | ` *       )` |
|      - |  950 | ` *` |
|      - |  951 | ` *   [object2] => SubObject Object` |
|      - |  952 | ` *       (` |
|      - |  953 | ` *           [instance] => 2` |
|      - |  954 | ` *       )` |
|      - |  955 | ` *` |
|      - |  956 | ` * )` |
|      - |  957 | ` * Cloned Object:` |
|      - |  958 | ` * MyCloneable Object` |
|      - |  959 | ` * (` |
|      - |  960 | ` *   [object1] => SubObject Object` |
|      - |  961 | ` *       (` |
|      - |  962 | ` *           [instance] => 3` |
|      - |  963 | ` *       )` |
|      - |  964 | ` *` |
|      - |  965 | ` *   [object2] => SubObject Object` |
|      - |  966 | ` *       (` |
|      - |  967 | ` *           [instance] => 2` |
|      - |  968 | ` *       )` |
|      - |  969 | ` * )` |
|      - |  970 | ` */` |
|     52 |  971 | `PH7_PRIVATE ph7_class_instance * PH7_CloneClassInstance(ph7_class_instance *pSrc)` |
|      4 |  972 |  |
|      - |  973 | `	ph7_class_instance *pClone;` |
|      - |  974 | `	ph7_class_method *pMethod;` |
|      - |  975 | `	SyHashEntry *pEntry2;` |
|      - |  976 | `	SyHashEntry *pEntry;` |
|      - |  977 | `	ph7_vm *pVm;` |
|      - |  978 | `	sxi32 rc;` |
|      - |  979 | `	/* Allocate a new instance */` |
|     56 |  980 | `	pVm = pSrc->pVm;` |
|     56 |  981 | `	pClone = NewClassInstance(pVm,pSrc->pClass);` |
|     56 |  982 | `	if( pClone == 0 ){` |
|    ! 0 |  983 | `		return 0;` |
|      - |  984 | `	}` |
|      - |  985 | `	/* Associate a private VM frame with this class instance */` |
|     56 |  986 | `	rc = PH7_VmCreateClassInstanceFrame(pVm,pClone);` |
|     56 |  987 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  988 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClone);` |
|    ! 0 |  989 | `		return 0;` |
|      - |  990 | `	}` |
|      - |  991 | `	/* Duplicate object values */` |
|     56 |  992 | `	SyHashResetLoopCursor(&pSrc->hAttr);` |
|     56 |  993 | `	SyHashResetLoopCursor(&pClone->hAttr);` |
|    138 |  994 | `	while((pEntry = SyHashGetNextEntry(&pSrc->hAttr)) != 0 && (pEntry2 = SyHashGetNextEntry(&pClone->hAttr)) != 0 ){` |
|     60 |  995 | `		VmClassAttr *pSrcAttr = (VmClassAttr *)pEntry->pUserData;` |
|     60 |  996 | `		VmClassAttr *pDestAttr = (VmClassAttr *)pEntry2->pUserData;` |
|      - |  997 | `		/* Duplicate non-static attribute */` |
|     60 |  998 | `		if( (pSrcAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|      - |  999 | `			ph7_value *pvSrc,*pvDest;` |
|     60 | 1000 | `			pvSrc = ExtractClassAttrValue(pVm,pSrcAttr);` |
|     60 | 1001 | `			pvDest = ExtractClassAttrValue(pVm,pDestAttr);` |
|     60 | 1002 | `			if( pvSrc && pvDest ){` |
|     60 | 1003 | `				PH7_MemObjStore(pvSrc,pvDest);` |
|     28 | 1004 | `			}` |
|      - | 1005 | `			/* Carry over the per-instance state so the clone matches the source:` |
|      - | 1006 | `			 * VM_CLASS_ATTR_UNINIT marks a typed property as not-yet-initialized` |
|      - | 1007 | `			 * and doubles as the readonly write-once latch — without this a clone` |
|      - | 1008 | `			 * would reset to uninitialized (losing the value's readiness) and a` |
|      - | 1009 | `			 * readonly property would become writable again. */` |
|     60 | 1010 | `			pDestAttr->iState = pSrcAttr->iState;` |
|     28 | 1011 | `		}` |
|      4 | 1012 | `	}` |
|      - | 1013 | `	/* call the __clone method on the cloned object if available */` |
|     56 | 1014 | `	pMethod = PH7_ClassExtractMethod(pClone->pClass,"__clone",sizeof("__clone")-1);` |
|     56 | 1015 | `	if( pMethod ){` |
|     38 | 1016 | `		if( pMethod->iCloneDepth < 16 ){` |
|     36 | 1017 | `			pMethod->iCloneDepth++;` |
|     36 | 1018 | `			PH7_VmCallClassMethod(pVm,pClone,pMethod,0,0,0);` |
|     19 | 1019 | `		}else{` |
|      - | 1020 | `			/* Nesting limit reached */` |
|      3 | 1021 | `			PH7_VmThrowError(pVm,0,PH7_CTX_ERR,"Object clone limit reached,no more call to __clone()");` |
|      - | 1022 | `		}` |
|      - | 1023 | `		/* Reset the cursor */` |
|     38 | 1024 | `		pMethod->iCloneDepth = 0;` |
|     18 | 1025 | `	}` |
|      - | 1026 | `	/* Return the cloned object */` |
|     56 | 1027 | `	return pClone;` |
|     30 | 1028 |  |
|      - | 1029 | `#define CLASS_INSTANCE_DESTROYED 0x001 /* Instance is released */` |
|      - | 1030 | `/*` |
|      - | 1031 | ` * Release a class instance [i.e: Object in the PHP jargon] and invoke any defined destructor.` |
|      - | 1032 | ` * This routine is invoked as soon as there are no other references to a particular` |
|      - | 1033 | ` * class instance.` |
|      - | 1034 | ` */` |
|   1886 | 1035 | `static void PH7_ClassInstanceRelease(ph7_class_instance *pThis)` |
|      5 | 1036 |  |
|      - | 1037 | `	ph7_class_method *pDestr;` |
|      - | 1038 | `	SyHashEntry *pEntry;` |
|      - | 1039 | `	ph7_class *pClass;` |
|      - | 1040 | `	ph7_vm *pVm;` |
|   1891 | 1041 | `	if( pThis->iFlags & CLASS_INSTANCE_DESTROYED ){` |
|      - | 1042 | `		/*` |
|      - | 1043 | `		 * Already destroyed,return immediately.` |
|      - | 1044 | `		 * This could happend if someone perform unset($this) in the destructor body.` |
|      - | 1045 | `		 */` |
|      9 | 1046 | `		return;` |
|      - | 1047 | `	}` |
|      - | 1048 | `	/* Mark as destroyed */` |
|   1883 | 1049 | `	pThis->iFlags \|= CLASS_INSTANCE_DESTROYED;` |
|      - | 1050 | `	/* Invoke any defined destructor if available */` |
|   1883 | 1051 | `	pVm = pThis->pVm;` |
|   1883 | 1052 | `	pClass = pThis->pClass;` |
|   1883 | 1053 | `	pDestr = PH7_ClassExtractMethod(pClass,"__destruct",sizeof("__destruct")-1);` |
|   1883 | 1054 | `	if( pDestr && !pVm->bInReset ){` |
|      - | 1055 | `		/* Invoke the destructor. Skipped during ph7_vm_reset() bulk teardown:` |
|      - | 1056 | `		 * running user PHP against a half-reset VM is unsafe (see bInReset). */` |
|     13 | 1057 | `		pThis->iRef = 2; /* Prevent garbage collection */` |
|     13 | 1058 | `		PH7_VmCallClassMethod(pVm,pThis,pDestr,0,0,0);` |
|      6 | 1059 | `	}` |
|      - | 1060 | `	/* Release non-static attributes */` |
|   1883 | 1061 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|   8993 | 1062 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|   7115 | 1063 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|   7115 | 1064 | `		if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|      - | 1065 | `			/* Drop any typed-property enforcement slot registered for this` |
|      - | 1066 | `			 * memobj. Must happen before the memobj is returned to the free` |
|      - | 1067 | `			 * list so a future recycled slot does not inherit the stale entry. */` |
|   7093 | 1068 | `			if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|    277 | 1069 | `				SyHashDeleteEntry(&pVm->hTypedSlot,` |
|    182 | 1070 | `					(const void *)&pVmAttr->nIdx,sizeof(sxu32),0);` |
|     91 | 1071 | `			}` |
|   7093 | 1072 | `			PH7_VmUnsetMemObj(pVm,pVmAttr->nIdx,TRUE);` |
|   3544 | 1073 | `		}` |
|   7115 | 1074 | `		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      5 | 1075 | `	}` |
|      - | 1076 | `	/* Release the whole structure */` |
|   1883 | 1077 | `	SyHashRelease(&pThis->hAttr);` |
|   1883 | 1078 | `	SyMemBackendPoolFree(&pVm->sAllocator,pThis);` |
|    948 | 1079 |  |
|      - | 1080 | `/*` |
|      - | 1081 | ` * Decrement the reference count of a class instance [i.e Object in the PHP jargon].` |
|      - | 1082 | ` * If the reference count reaches zero,release the whole instance.` |
|      - | 1083 | ` */` |
|  36752 | 1084 | `PH7_PRIVATE void PH7_ClassInstanceUnref(ph7_class_instance *pThis)` |
|      5 | 1085 |  |
|  36757 | 1086 | `	pThis->iRef--;` |
|  36757 | 1087 | `	if( pThis->iRef < 1 ){` |
|      - | 1088 | `		/* No more reference to this instance */` |
|   1891 | 1089 | `		PH7_ClassInstanceRelease(&(*pThis));` |
|    943 | 1090 | `	}` |
|  36757 | 1091 |  |
|      - | 1092 | `/*` |
|      - | 1093 | ` * Compare two class instances [i.e: Objects in the PHP jargon]` |
|      - | 1094 | ` * Note on objects comparison:` |
|      - | 1095 | ` *  According to the PHP langauge reference manual` |
|      - | 1096 | ` *  When using the comparison operator (==), object variables are compared in a simple manner` |
|      - | 1097 | ` *  namely: Two object instances are equal if they have the same attributes and values, and are` |
|      - | 1098 | ` *  instances of the same class.` |
|      - | 1099 | ` *  On the other hand, when using the identity operator (===), object variables are identical` |
|      - | 1100 | ` *  if and only if they refer to the same instance of the same class.` |
|      - | 1101 | ` *  An example will clarify these rules.` |
|      - | 1102 | ` *  Example #1 Example of object comparison` |
|      - | 1103 | ` *  <?php` |
|      - | 1104 | ` *    function bool2str($bool)` |
|      - | 1105 | ` * {` |
|      - | 1106 | ` *   if ($bool === false) {` |
|      - | 1107 | ` *       return 'FALSE';` |
|      - | 1108 | ` *   } else {` |
|      - | 1109 | ` *       return 'TRUE';` |
|      - | 1110 | ` *   }` |
|      - | 1111 | ` * }` |
|      - | 1112 | ` * function compareObjects(&$o1, &$o2)` |
|      - | 1113 | ` * {` |
|      - | 1114 | ` *   echo 'o1 == o2 : ' . bool2str($o1 == $o2) . "\n";` |
|      - | 1115 | ` *   echo 'o1 != o2 : ' . bool2str($o1 != $o2) . "\n";` |
|      - | 1116 | ` *   echo 'o1 === o2 : ' . bool2str($o1 === $o2) . "\n";` |
|      - | 1117 | ` *   echo 'o1 !== o2 : ' . bool2str($o1 !== $o2) . "\n";` |
|      - | 1118 | ` * }` |
|      - | 1119 | ` * class Flag` |
|      - | 1120 | ` * {` |
|      - | 1121 | ` *   public $flag;` |
|      - | 1122 | ` *` |
|      - | 1123 | ` *   function Flag($flag = true) {` |
|      - | 1124 | ` *       $this->flag = $flag;` |
|      - | 1125 | ` *   }` |
|      - | 1126 | ` * }` |
|      - | 1127 | ` *` |
|      - | 1128 | ` * class OtherFlag` |
|      - | 1129 | ` * {` |
|      - | 1130 | ` *   public $flag;` |
|      - | 1131 | ` *` |
|      - | 1132 | ` *   function OtherFlag($flag = true) {` |
|      - | 1133 | ` *       $this->flag = $flag;` |
|      - | 1134 | ` *   }` |
|      - | 1135 | ` * }` |
|      - | 1136 | ` *` |
|      - | 1137 | ` * $o = new Flag();` |
|      - | 1138 | ` * $p = new Flag();` |
|      - | 1139 | ` * $q = $o;` |
|      - | 1140 | ` * $r = new OtherFlag();` |
|      - | 1141 | ` *` |
|      - | 1142 | ` * echo "Two instances of the same class\n";` |
|      - | 1143 | ` * compareObjects($o, $p);` |
|      - | 1144 | ` * echo "\nTwo references to the same instance\n";` |
|      - | 1145 | ` * compareObjects($o, $q);` |
|      - | 1146 | ` * echo "\nInstances of two different classes\n";` |
|      - | 1147 | ` * compareObjects($o, $r);` |
|      - | 1148 | ` * ?>` |
|      - | 1149 | ` * The above example will output:` |
|      - | 1150 | ` * Two instances of the same class` |
|      - | 1151 | ` * o1 == o2 : TRUE` |
|      - | 1152 | ` * o1 != o2 : FALSE` |
|      - | 1153 | ` * o1 === o2 : FALSE` |
|      - | 1154 | ` * o1 !== o2 : TRUE` |
|      - | 1155 | ` * Two references to the same instance` |
|      - | 1156 | ` * o1 == o2 : TRUE` |
|      - | 1157 | ` * o1 != o2 : FALSE` |
|      - | 1158 | ` * o1 === o2 : TRUE` |
|      - | 1159 | ` * o1 !== o2 : FALSE` |
|      - | 1160 | ` * Instances of two different classes` |
|      - | 1161 | ` * o1 == o2 : FALSE` |
|      - | 1162 | ` * o1 != o2 : TRUE` |
|      - | 1163 | ` * o1 === o2 : FALSE` |
|      - | 1164 | ` * o1 !== o2 : TRUE` |
|      - | 1165 | ` *` |
|      - | 1166 | ` * This function return 0 if the objects are equals according to the comprison rules defined above.` |
|      - | 1167 | ` * Any other return values indicates difference.` |
|      - | 1168 | ` */` |
|    216 | 1169 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest)` |
|      5 | 1170 |  |
|      - | 1171 | `	SyHashEntry *pEntry,*pEntry2;` |
|      - | 1172 | `	ph7_value sV1,sV2;` |
|      - | 1173 | `	sxi32 rc;` |
|    221 | 1174 | `	if( iNest > 31 ){` |
|      - | 1175 | `		/* Nesting limit reached */` |
|      6 | 1176 | `		PH7_VmThrowError(pLeft->pVm,0,PH7_CTX_ERR,"Nesting limit reached: Infinite recursion?");` |
|      6 | 1177 | `		return 1;` |
|      - | 1178 | `	}` |
|      - | 1179 | `	/* Comparison is performed only if the objects are instance of the same class */` |
|    217 | 1180 | `	if( pLeft->pClass != pRight->pClass ){` |
|      7 | 1181 | `		return 1;` |
|      - | 1182 | `	}` |
|    211 | 1183 | `	if( bStrict ){` |
|      - | 1184 | `		/*` |
|      - | 1185 | `		 * According to the PHP language reference manual:` |
|      - | 1186 | `		 *  when using the identity operator (===), object variables` |
|      - | 1187 | `		 *  are identical if and only if they refer to the same instance` |
|      - | 1188 | `		 *  of the same class.` |
|      - | 1189 | `		 */` |
|     65 | 1190 | `		return !(pLeft == pRight);` |
|      - | 1191 | `	}` |
|      - | 1192 | `	/*` |
|      - | 1193 | `	 * Attribute comparison.` |
|      - | 1194 | `	 * According to the PHP reference manual:` |
|      - | 1195 | `	 *  When using the comparison operator (==), object variables are compared` |
|      - | 1196 | `	 *  in a simple manner, namely: Two object instances are equal if they have` |
|      - | 1197 | `	 *  the same attributes and values, and are instances of the same class.` |
|      - | 1198 | `	 */` |
|    149 | 1199 | `	if( pLeft == pRight ){` |
|      - | 1200 | `		/* Same instance,don't bother processing,object are equals */` |
|      5 | 1201 | `		return 0;` |
|      - | 1202 | `	}` |
|      - | 1203 | `	/* Closures compare by IDENTITY under == as well (not by attributes): two distinct` |
|      - | 1204 | `	 * Closure instances are never equal, even when they wrap the same underlying function` |
|      - | 1205 | `	 * (PHP semantics). pLeft != pRight here, so a Closure pair is unequal. Without this,` |
|      - | 1206 | `` 	 * two capture-less lambdas of the same `function(){}` share the template's `$__fn` `` |
|      - | 1207 | `	 * name and would compare equal. */` |
|    145 | 1208 | `	if( pLeft->pVm->pClosureClass && pLeft->pClass == pLeft->pVm->pClosureClass ){` |
|      5 | 1209 | `		return 1;` |
|      - | 1210 | `	}` |
|    141 | 1211 | `	SyHashResetLoopCursor(&pLeft->hAttr);` |
|    141 | 1212 | `	SyHashResetLoopCursor(&pRight->hAttr);` |
|    141 | 1213 | `	PH7_MemObjInit(pLeft->pVm,&sV1);` |
|    141 | 1214 | `	PH7_MemObjInit(pLeft->pVm,&sV2);` |
|    141 | 1215 | `	sV1.nIdx = sV2.nIdx = SXU32_HIGH;` |
|    224 | 1216 | `	while((pEntry = SyHashGetNextEntry(&pLeft->hAttr)) != 0 && (pEntry2 = SyHashGetNextEntry(&pRight->hAttr)) != 0 ){` |
|    147 | 1217 | `		VmClassAttr *p1 = (VmClassAttr *)pEntry->pUserData;` |
|    147 | 1218 | `		VmClassAttr *p2 = (VmClassAttr *)pEntry2->pUserData;` |
|      - | 1219 | `		/* Compare only non-static attribute */` |
|    147 | 1220 | `		if( (p1->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|      - | 1221 | `			ph7_value *pL,*pR;` |
|    147 | 1222 | `			pL = ExtractClassAttrValue(pLeft->pVm,p1);` |
|    147 | 1223 | `			pR = ExtractClassAttrValue(pRight->pVm,p2);` |
|    147 | 1224 | `			if( pL && pR ){` |
|    147 | 1225 | `				PH7_MemObjLoad(pL,&sV1);` |
|    147 | 1226 | `				PH7_MemObjLoad(pR,&sV2);` |
|      - | 1227 | `				/* Compare the two values now */` |
|    147 | 1228 | `				rc = PH7_MemObjCmp(&sV1,&sV2,bStrict,iNest+1);` |
|    147 | 1229 | `				PH7_MemObjRelease(&sV1);` |
|    147 | 1230 | `				PH7_MemObjRelease(&sV2);` |
|    147 | 1231 | `				if( rc != 0 ){` |
|      - | 1232 | `					/* Not equals */` |
|    133 | 1233 | `					return rc;` |
|      - | 1234 | `				}` |
|      7 | 1235 | `			}` |
|      7 | 1236 | `		}` |
|      1 | 1237 | `	}` |
|      - | 1238 | `	/* Object are equals */` |
|      9 | 1239 | `	return 0;` |
|    113 | 1240 |  |
|      - | 1241 | `/*` |
|      - | 1242 | ` * Dump a class instance and the store the dump in the BLOB given` |
|      - | 1243 | ` * as the first argument.` |
|      - | 1244 | ` * Note that only non-static/non-constants attribute are dumped.` |
|      - | 1245 | ` * This function is typically invoked when the user issue a call` |
|      - | 1246 | ` * to [var_dump(),var_export(),print_r(),...].` |
|      - | 1247 | ` * This function SXRET_OK on success. Any other return value including` |
|      - | 1248 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|      - | 1249 | ` */` |
|      - | 1250 | `/*` |
|      - | 1251 | ` * Emit a class-instance dump header plus its trailing newline. For var_dump` |
|      - | 1252 | ` * (ShowType) it completes the "object(" prefix the caller already emitted as` |
|      - | 1253 | ` *   ClassName)#<id> (<count>) {` |
|      - | 1254 | ` * for print_r it emits the legacy PHL  Object(ClassName) {  (count/id unused).` |
|      - | 1255 | ` */` |
|    134 | 1256 | `static void DumpClassInstanceHeader(SyBlob *pOut,ph7_class *pClass,sxu32 nObjId,int ShowType,sxu32 nCount)` |
|      3 | 1257 |  |
|    137 | 1258 | `	if( !ShowType ){` |
|      6 | 1259 | `		SyBlobAppend(&(*pOut),"Object(",sizeof("Object(")-1);` |
|      6 | 1260 | `		SyBlobFormat(&(*pOut),"%z) {",&pClass->sName);` |
|      4 | 1261 | `	}else{` |
|    133 | 1262 | `		SyBlobFormat(&(*pOut),"%z)#%u (%u) {",&pClass->sName,nObjId,nCount);` |
|      - | 1263 | `	}` |
|      - | 1264 | `#ifdef __WINNT__` |
|      3 | 1265 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|      - | 1266 | `#else` |
|    134 | 1267 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      - | 1268 | `#endif` |
|    137 | 1269 |  |
|    138 | 1270 | `PH7_PRIVATE sxi32 PH7_ClassInstanceDump(SyBlob *pOut,ph7_class_instance *pThis,int ShowType,int nTab,int nDepth)` |
|      3 | 1271 |  |
|      - | 1272 | `	SyHashEntry *pEntry;` |
|      - | 1273 | `	ph7_value *pValue;` |
|      - | 1274 | `	sxi32 rc;` |
|      - | 1275 | `	int i;` |
|    141 | 1276 | `	if( nDepth > 31 ){` |
|      - | 1277 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|      - | 1278 | `		/* Nesting limit reached..halt immediately*/` |
|      5 | 1279 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|      5 | 1280 | `		if( ShowType ){` |
|      5 | 1281 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|      2 | 1282 | `		}` |
|      5 | 1283 | `		return SXERR_LIMIT;` |
|      - | 1284 | `	}` |
|    137 | 1285 | `	rc = SXRET_OK;` |
|      - | 1286 | `	{` |
|      - | 1287 | `		/* Both var_dump and print_r consult __debugInfo() (PHP behavior);` |
|      - | 1288 | `		 * var_export uses a separate renderer and never reaches here. When the` |
|      - | 1289 | `		 * method is present and returns an array, render that array's entries as` |
|      - | 1290 | `		 * the object body, with the header showing the debug array's count. The` |
|      - | 1291 | `		 * nDepth guard above protects against a __debugInfo returning the object` |
|      - | 1292 | `		 * itself. */` |
|    137 | 1293 | `		ph7_class_method *pDbg = PH7_ClassExtractMethod(pThis->pClass,"__debugInfo",sizeof("__debugInfo")-1);` |
|    137 | 1294 | `		if( pDbg ){` |
|      - | 1295 | `			ph7_value sResult;` |
|      5 | 1296 | `			PH7_MemObjInit(pThis->pVm,&sResult);` |
|      5 | 1297 | `			PH7_VmCallClassMethod(pThis->pVm,pThis,pDbg,&sResult,0,0);` |
|      5 | 1298 | `			if( sResult.iFlags & MEMOBJ_HASHMAP ){` |
|      5 | 1299 | `				ph7_hashmap *pMap = (ph7_hashmap *)sResult.x.pOther;` |
|      - | 1300 | `				/* Header count is the debug array's entry count. */` |
|      5 | 1301 | `				DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,pMap->nEntry);` |
|      5 | 1302 | `				rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth);` |
|      9 | 1303 | `				for( i = 0 ; i < nTab ; i++ ){` |
|      5 | 1304 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      3 | 1305 | `				}` |
|      5 | 1306 | `				SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      5 | 1307 | `				PH7_MemObjRelease(&sResult);` |
|      5 | 1308 | `				return rc;` |
|      - | 1309 | `			}` |
|      - | 1310 | `			/* Non-array return: behave as if __debugInfo were absent. */` |
|    ! 0 | 1311 | `			PH7_MemObjRelease(&sResult);` |
|    ! 0 | 1312 | `		}` |
|      - | 1313 | `	}` |
|      - | 1314 | `	{` |
|      - | 1315 | `		/* var_dump's header needs the property count up front, so pre-count the` |
|      - | 1316 | `		 * non-static/non-constant attributes (matching the dump loop below). */` |
|    132 | 1317 | `		sxu32 nProp = 0;` |
|    132 | 1318 | `		if( ShowType ){` |
|    130 | 1319 | `			SyHashResetLoopCursor(&pThis->hAttr);` |
|    266 | 1320 | `			while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|    138 | 1321 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    138 | 1322 | `				if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|    134 | 1323 | `					nProp++;` |
|     66 | 1324 | `				}` |
|      2 | 1325 | `			}` |
|     64 | 1326 | `		}` |
|    132 | 1327 | `		DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,nProp);` |
|      - | 1328 | `	}` |
|      - | 1329 | `	/* Dump object attributes */` |
|    132 | 1330 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|    213 | 1331 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|    142 | 1332 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    142 | 1333 | `		if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|      - | 1334 | `			/* Dump non-static/constant attribute only */` |
|   3994 | 1335 | `			for( i = 0 ; i < nTab ; i++ ){` |
|   3858 | 1336 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|   1930 | 1337 | `			}` |
|    138 | 1338 | `			pValue = ExtractClassAttrValue(pThis->pVm,pVmAttr);` |
|    138 | 1339 | `			if( pValue ){` |
|    138 | 1340 | `				SyBlobFormat(&(*pOut),"['%z'] =>",&pVmAttr->pAttr->sName);` |
|      - | 1341 | `#ifdef __WINNT__` |
|      2 | 1342 | `				SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|      - | 1343 | `#else` |
|    136 | 1344 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      - | 1345 | `#endif` |
|    138 | 1346 | `				rc = PH7_MemObjDump(&(*pOut),pValue,ShowType,nTab+1,nDepth,0);` |
|    138 | 1347 | `				if( rc == SXERR_LIMIT ){` |
|    125 | 1348 | `					break;` |
|      - | 1349 | `				}` |
|      6 | 1350 | `			}` |
|      6 | 1351 | `		}` |
|      2 | 1352 | `	}` |
|   3982 | 1353 | `	for( i = 0 ; i < nTab ; i++ ){` |
|   3852 | 1354 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|   1927 | 1355 | `	}` |
|    132 | 1356 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|    132 | 1357 | `	return rc;` |
|     72 | 1358 |  |
|      - | 1359 | `/*` |
|      - | 1360 | ` * Call a magic method [i.e: __toString(),__toBool(),__Invoke()...]` |
|      - | 1361 | ` * Return SXRET_OK on successfull call. Any other return value indicates failure.` |
|      - | 1362 | ` * Notes on magic methods.` |
|      - | 1363 | ` * According to the PHP language reference manual.` |
|      - | 1364 | ` *  The function names __construct(), __destruct(), __call(), __callStatic()` |
|      - | 1365 | ` *  __get(),  __toString(), __invoke(), __clone() are magical in PHP classes.` |
|      - | 1366 | ` * You cannot have functions with these names in any of your classes unless` |
|      - | 1367 | ` * you want the magic functionality associated with them.` |
|      - | 1368 | ` * Example of magical methods:` |
|      - | 1369 | ` * __toString()` |
|      - | 1370 | ` *  The __toString() method allows a class to decide how it will react when it is treated like` |
|      - | 1371 | ` *  a string. For example, what echo $obj; will print. This method must return a string.` |
|      - | 1372 | ` *  Example #2 Simple example` |
|      - | 1373 | ` * <?php` |
|      - | 1374 | ` * // Declare a simple class` |
|      - | 1375 | ` * class TestClass` |
|      - | 1376 | ` * {` |
|      - | 1377 | ` *   public $foo;` |
|      - | 1378 | ` *` |
|      - | 1379 | ` *   public function __construct($foo)` |
|      - | 1380 | ` *   {` |
|      - | 1381 | ` *       $this->foo = $foo;` |
|      - | 1382 | ` *   }` |
|      - | 1383 | ` *` |
|      - | 1384 | ` *   public function __toString()` |
|      - | 1385 | ` *   {` |
|      - | 1386 | ` *       return $this->foo;` |
|      - | 1387 | ` *   }` |
|      - | 1388 | ` * }` |
|      - | 1389 | ` * $class = new TestClass('Hello');` |
|      - | 1390 | ` * echo $class;` |
|      - | 1391 | ` * ?>` |
|      - | 1392 | ` * The above example will output:` |
|      - | 1393 | ` *  Hello` |
|      - | 1394 | ` *` |
|      - | 1395 | ` * Note that PH7 does not support all the magical method and introudces __toFloat(),__toInt()` |
|      - | 1396 | ` * which have the same behaviour as __toString() but for float and integer types` |
|      - | 1397 | ` * respectively.` |
|      - | 1398 | ` * Refer to the official documentation for more information.` |
|      - | 1399 | ` */` |
|      2 | 1400 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallMagicMethod(` |
|      - | 1401 | `	ph7_vm *pVm,               /* VM that own all this stuff */` |
|      - | 1402 | `	ph7_class *pClass,         /* Target class */` |
|      - | 1403 | `	ph7_class_instance *pThis, /* Target object */` |
|      - | 1404 | `	const char *zMethod,       /* Magic method name [i.e: __toString()]*/` |
|      - | 1405 | `	sxu32 nByte,               /* zMethod length*/` |
|      - | 1406 | `	const SyString *pAttrName  /* Attribute name */` |
|      - | 1407 | `	)` |
|      1 | 1408 |  |
|      3 | 1409 | `	ph7_value *apArg[2] = { 0 , 0 };` |
|      - | 1410 | `	ph7_class_method *pMeth;` |
|      - | 1411 | `	ph7_value sAttr; /* cc warning */` |
|      - | 1412 | `	sxi32 rc;` |
|      - | 1413 | `	int nArg;` |
|      - | 1414 | `	/* Make sure the magic method is available */` |
|      3 | 1415 | `	pMeth = PH7_ClassExtractMethod(&(*pClass),zMethod,nByte);` |
|      3 | 1416 | `	if( pMeth == 0 ){` |
|      - | 1417 | `		/* No such method,return immediately */` |
|      3 | 1418 | `		return SXERR_NOTFOUND;` |
|      - | 1419 | `	}` |
|    ! 0 | 1420 | `	nArg = 0;` |
|      - | 1421 | `	/* Copy arguments */` |
|    ! 0 | 1422 | `	if( pAttrName ){` |
|    ! 0 | 1423 | `		PH7_MemObjInitFromString(pVm,&sAttr,pAttrName);` |
|    ! 0 | 1424 | `		sAttr.nIdx = SXU32_HIGH; /* Mark as constant */` |
|    ! 0 | 1425 | `		apArg[0] = &sAttr;` |
|    ! 0 | 1426 | `		nArg = 1;` |
|    ! 0 | 1427 | `	}` |
|      - | 1428 | `	/* Call the magic method now */` |
|    ! 0 | 1429 | `	rc = PH7_VmCallClassMethod(pVm,&(*pThis),pMeth,0,nArg,apArg);` |
|      - | 1430 | `	/* Clean up */` |
|    ! 0 | 1431 | `	if( pAttrName ){` |
|    ! 0 | 1432 | `		PH7_MemObjRelease(&sAttr);` |
|    ! 0 | 1433 | `	}` |
|    ! 0 | 1434 | `	return rc;` |
|      2 | 1435 |  |
|      - | 1436 | `/*` |
|      - | 1437 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon].` |
|      - | 1438 | ` * This function is simply a wrapper on ExtractClassAttrValue().` |
|      - | 1439 | ` */` |
|     74 | 1440 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceExtractAttrValue(ph7_class_instance *pThis,VmClassAttr *pAttr)` |
|      3 | 1441 |  |
|      - | 1442 | `   /* Extract the attribute value */` |
|      - | 1443 | `	ph7_value *pValue;` |
|     77 | 1444 | `	pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     77 | 1445 | `	return pValue;` |
|      3 | 1446 |  |
|      - | 1447 | `/*` |
|      - | 1448 | ` * Convert a class instance [i.e: Object in the PHP jargon] into a hashmap [i.e: array in the PHP jargon].` |
|      - | 1449 | ` * Return SXRET_OK on success. Any other value indicates failure.` |
|      - | 1450 | ` * Note on object conversion to array:` |
|      - | 1451 | ` *  Acccording to the PHP language reference manual` |
|      - | 1452 | ` *  If an object is converted to an array, the result is an array whose elements are the object's properties.` |
|      - | 1453 | ` *  The keys are the member variable names.` |
|      - | 1454 | ` *` |
|      - | 1455 | ` *  The following example:` |
|      - | 1456 | ` *  class Test {` |
|      - | 1457 | ` *   public $A = 25<<1;  // 50` |
|      - | 1458 | ` *	 public $c = rand_str(3);   // Random string of length 3` |
|      - | 1459 | ` *	 public $d = rand() & 1023; // Random number between 0..1023` |
|      - | 1460 | ` *  }` |
|      - | 1461 | ` *  var_dump((array) new Test());` |
|      - | 1462 | ` *	Will output:` |
|      - | 1463 | ` *  array(3) {` |
|      - | 1464 | ` *   [A] =>` |
|      - | 1465 | ` *      int(50)` |
|      - | 1466 | ` *   [c] =>` |
|      - | 1467 | ` *     string(3 'aps')` |
|      - | 1468 | ` *   [d] =>` |
|      - | 1469 | ` *     int(991)` |
|      - | 1470 | ` *  }` |
|      - | 1471 | ` * You have noticed that PH7 allow class attributes [i.e: $a,$c,$d in the example above]` |
|      - | 1472 | ` * have any complex expression (even function calls/Annonymous functions) as their default` |
|      - | 1473 | ` * value unlike the standard PHP engine.` |
|      - | 1474 | ` * This is a very powerful feature that you have to look at.` |
|      - | 1475 | ` */` |
|      6 | 1476 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|      1 | 1477 |  |
|      - | 1478 | `	SyHashEntry *pEntry;` |
|      - | 1479 | `	SyString *pAttrName;` |
|      - | 1480 | `	VmClassAttr *pAttr;` |
|      - | 1481 | `	ph7_value *pValue;` |
|      - | 1482 | `	ph7_value sName;` |
|      - | 1483 | `	/* Reset the loop cursor */` |
|      7 | 1484 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      7 | 1485 | `	PH7_MemObjInitFromString(pThis->pVm,&sName,0);` |
|     20 | 1486 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|      - | 1487 | `		/* Point to the current attribute */` |
|     11 | 1488 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1489 | `		/* Extract attribute value */` |
|     11 | 1490 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     11 | 1491 | `		if( pValue ){` |
|      - | 1492 | `			/* Build attribute name */` |
|     11 | 1493 | `			pAttrName = &pAttr->pAttr->sName;` |
|     11 | 1494 | `			PH7_MemObjStringAppend(&sName,pAttrName->zString,pAttrName->nByte);` |
|      - | 1495 | `			/* Perform the insertion */` |
|     11 | 1496 | `			PH7_HashmapInsert(pMap,&sName,pValue);` |
|      - | 1497 | `			/* Reset the string cursor */` |
|     11 | 1498 | `			SyBlobReset(&sName.sBlob);` |
|      5 | 1499 | `		}` |
|      1 | 1500 | `	}` |
|      7 | 1501 | `	PH7_MemObjRelease(&sName);` |
|      7 | 1502 | `	return SXRET_OK;` |
|      1 | 1503 |  |
|      - | 1504 | `/*` |
|      - | 1505 | ` * Iterate throw class attributes and invoke the given callback [i.e: xWalk()] for each` |
|      - | 1506 | ` * retrieved attribute.` |
|      - | 1507 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|      - | 1508 | ` * the attribute value in the callback body will not alter the real attribute value.` |
|      - | 1509 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|      - | 1510 | ` * a value different from PH7_OK.` |
|      - | 1511 | ` * Refer to [ph7_object_walk()] for more information.` |
|      - | 1512 | ` */` |
|      2 | 1513 | `PH7_PRIVATE sxi32 PH7_ClassInstanceWalk(` |
|      - | 1514 | `	ph7_class_instance *pThis, /* Target object */` |
|      - | 1515 | `	int (*xWalk)(const char *,ph7_value *,void *), /* Walker callback */` |
|      - | 1516 | `	void *pUserData /* Last argument to xWalk() */` |
|      - | 1517 | `	)` |
|      1 | 1518 |  |
|      - | 1519 | `	SyHashEntry *pEntry; /* Hash entry */` |
|      - | 1520 | `	VmClassAttr *pAttr;  /* Pointer to the attribute */` |
|      - | 1521 | `	ph7_value *pValue;   /* Attribute value */` |
|      - | 1522 | `	ph7_value sValue;    /* Copy of the attribute value */` |
|      - | 1523 | `	int rc;` |
|      - | 1524 | `	/* Reset the loop cursor */` |
|      3 | 1525 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      3 | 1526 | `	PH7_MemObjInit(pThis->pVm,&sValue);` |
|      - | 1527 | `	/* Start the walk process */` |
|      8 | 1528 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|      - | 1529 | `		/* Point to the current attribute */` |
|      5 | 1530 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1531 | `		/* Extract attribute value */` |
|      5 | 1532 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|      5 | 1533 | `		if( pValue ){` |
|      5 | 1534 | `			PH7_MemObjLoad(pValue,&sValue);` |
|      - | 1535 | `			/* Invoke the supplied callback */` |
|      5 | 1536 | `			rc =  xWalk(SyStringData(&pAttr->pAttr->sName),&sValue,pUserData);` |
|      5 | 1537 | `			PH7_MemObjRelease(&sValue);` |
|      5 | 1538 | `			if( rc != PH7_OK){` |
|      - | 1539 | `				/* User callback request an operation abort */` |
|    ! 0 | 1540 | `				return SXERR_ABORT;` |
|      - | 1541 | `			}` |
|      2 | 1542 | `		}` |
|      1 | 1543 | `	}` |
|      - | 1544 | `	/* All done */` |
|      3 | 1545 | `	return SXRET_OK;` |
|      2 | 1546 |  |
|      - | 1547 | `/*` |
|      - | 1548 | ` * Extract a class atrribute value.` |
|      - | 1549 | ` * Return a pointer to the attribute value on success. Otherwise NULL.` |
|      - | 1550 | ` * Note:` |
|      - | 1551 | ` *  Access to static and constant attribute is not allowed. That is,the function` |
|      - | 1552 | ` *  will return NULL in case someone (host-application code) try to extract` |
|      - | 1553 | ` *  a static/constant attribute.` |
|      - | 1554 | ` */` |
|   2380 | 1555 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceFetchAttr(ph7_class_instance *pThis,const SyString *pName)` |
|      5 | 1556 |  |
|      - | 1557 | `	SyHashEntry *pEntry;` |
|      - | 1558 | `	VmClassAttr *pAttr;` |
|      - | 1559 | `	/* Query the attribute hashtable */` |
|   2385 | 1560 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);` |
|   2385 | 1561 | `	if( pEntry == 0 ){` |
|      - | 1562 | `		/* No such attribute */` |
|    ! 0 | 1563 | `		return 0;` |
|      - | 1564 | `	}` |
|      - | 1565 | `	/* Point to the class atrribute */` |
|   2385 | 1566 | `	pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1567 | `	/* Check if we are dealing with a static/constant attribute */` |
|   2385 | 1568 | `	if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|      - | 1569 | `		/* Access is forbidden */` |
|    ! 0 | 1570 | `		return 0;` |
|      - | 1571 | `	}` |
|      - | 1572 | `	/* Return the attribute value */` |
|   2385 | 1573 | `	return ExtractClassAttrValue(pThis->pVm,pAttr);` |
|   1195 | 1574 |  |
|      - | 1575 |  |
