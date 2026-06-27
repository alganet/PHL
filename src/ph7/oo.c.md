# src/ph7/oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 597/657 lines (90.87%)

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
| 135380 |   14 | `PH7_PRIVATE ph7_class * PH7_NewRawClass(ph7_vm *pVm,const SyString *pName,sxu32 nLine)` |
|      5 |   15 |  |
|      - |   16 | `	ph7_class *pClass;` |
|      - |   17 | `	char *zName;` |
|      - |   18 | `	/* Allocate a new instance */` |
| 135385 |   19 | `	pClass = (ph7_class *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class));` |
| 135385 |   20 | `	if( pClass == 0 ){` |
|    ! 0 |   21 | `		return 0;` |
|      - |   22 | `	}` |
|      - |   23 | `	/* Zero the structure */` |
| 135385 |   24 | `	SyZero(pClass,sizeof(ph7_class));` |
|      - |   25 | `	/* Duplicate class name */` |
| 135385 |   26 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
| 135385 |   27 | `	if( zName == 0 ){` |
|    ! 0 |   28 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClass);` |
|    ! 0 |   29 | `		return 0;` |
|      - |   30 | `	}` |
|      - |   31 | `	/* Initialize fields */` |
| 135385 |   32 | `	SyStringInitFromBuf(&pClass->sName,zName,pName->nByte);` |
| 135385 |   33 | `	SyHashInit(&pClass->hMethod,&pVm->sAllocator,0,0);` |
| 135385 |   34 | `	SyHashInit(&pClass->hAttr,&pVm->sAllocator,0,0);` |
| 135385 |   35 | `	SyHashInit(&pClass->hDerived,&pVm->sAllocator,0,0);` |
| 135385 |   36 | `	SySetInit(&pClass->aInterface,&pVm->sAllocator,sizeof(ph7_class *));` |
| 135385 |   37 | `	SySetInit(&pClass->aTrait,&pVm->sAllocator,sizeof(ph7_class *));` |
| 135385 |   38 | `	pClass->nLine = nLine;` |
|      - |   39 | `	/* All done */` |
| 135385 |   40 | `	return pClass;` |
|  67695 |   41 |  |
|      - |   42 | `/*` |
|      - |   43 | ` * Allocate and initialize a new class attribute.` |
|      - |   44 | ` * Return a pointer to the class attribute on success. NULL otherwise.` |
|      - |   45 | ` */` |
|  67798 |   46 | `PH7_PRIVATE ph7_class_attr * PH7_NewClassAttr(ph7_vm *pVm,const SyString *pName,sxu32 nLine,sxi32 iProtection,sxi32 iFlags)` |
|      5 |   47 |  |
|      - |   48 | `	ph7_class_attr *pAttr;` |
|      - |   49 | `	char *zName;` |
|  67803 |   50 | `	pAttr = (ph7_class_attr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_attr));` |
|  67803 |   51 | `	if( pAttr == 0 ){` |
|    ! 0 |   52 | `		return 0;` |
|      - |   53 | `	}` |
|      - |   54 | `	/* Zero the structure */` |
|  67803 |   55 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|      - |   56 | `	/* Duplicate attribute name */` |
|  67803 |   57 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  67803 |   58 | `	if( zName == 0 ){` |
|    ! 0 |   59 | `		SyMemBackendPoolFree(&pVm->sAllocator,pAttr);` |
|    ! 0 |   60 | `		return 0;` |
|      - |   61 | `	}` |
|      - |   62 | `	/* Initialize fields */` |
|  67803 |   63 | `	SySetInit(&pAttr->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|  67803 |   64 | `	SySetInit(&pAttr->aUnionAlts,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|  67803 |   65 | `	SyStringInitFromBuf(&pAttr->sName,zName,pName->nByte);` |
|  67803 |   66 | `	pAttr->iProtection = iProtection;` |
|  67803 |   67 | `	pAttr->nIdx = SXU32_HIGH;` |
|  67803 |   68 | `	pAttr->iFlags = iFlags;` |
|  67803 |   69 | `	pAttr->nLine = nLine;` |
|  67803 |   70 | `	return pAttr;` |
|  33904 |   71 |  |
|      - |   72 | `/*` |
|      - |   73 | ` * Allocate and initialize a new class method.` |
|      - |   74 | ` * Return a pointer to the class method on success. NULL otherwise` |
|      - |   75 | ` * This function associate with the newly created method an automatically generated` |
|      - |   76 | ` * random unique name.` |
|      - |   77 | ` */` |
| 265684 |   78 | `PH7_PRIVATE ph7_class_method * PH7_NewClassMethod(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,sxu32 nLine,` |
|      - |   79 | `	sxi32 iProtection,sxi32 iFlags,sxi32 iFuncFlags)` |
|      5 |   80 |  |
|      - |   81 | `	ph7_class_method *pMeth;` |
|      - |   82 | `	SyHashEntry *pEntry;` |
|      - |   83 | `	SyString *pNamePtr;` |
|      - |   84 | `	char zSalt[10];` |
|      - |   85 | `	char *zName;` |
|      - |   86 | `	sxu32 nByte;` |
|      - |   87 | `	/* Allocate a new class method instance */` |
| 265689 |   88 | `	pMeth = (ph7_class_method *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_method));` |
| 265689 |   89 | `	if( pMeth == 0 ){` |
|    ! 0 |   90 | `		return 0;` |
|      - |   91 | `	}` |
|      - |   92 | `	/* Zero the structure */` |
| 265689 |   93 | `	SyZero(pMeth,sizeof(ph7_class_method));` |
|      - |   94 | `	/* Check for an already installed method with the same name */` |
| 265689 |   95 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
| 265689 |   96 | `	if( pEntry == 0 ){` |
|      - |   97 | `		/* Associate an unique VM name to this method */` |
| 265687 |   98 | `		nByte = sizeof(zSalt) + pName->nByte + SyStringLength(&pClass->sName)+sizeof(char)*7/*[[__'\0'*/;` |
| 265687 |   99 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,nByte);` |
| 265687 |  100 | `		if( zName == 0 ){` |
|    ! 0 |  101 | `			SyMemBackendPoolFree(&pVm->sAllocator,pMeth);` |
|    ! 0 |  102 | `			return 0;` |
|      - |  103 | `		}` |
| 265687 |  104 | `		pNamePtr = &pMeth->sVmName;` |
|      - |  105 | `		/* Generate a random string */` |
| 265687 |  106 | `		PH7_VmRandomString(&(*pVm),zSalt,sizeof(zSalt));` |
| 265687 |  107 | `		pNamePtr->nByte = SyBufferFormat(zName,nByte,"[__%z@%z_%.*s]",&pClass->sName,pName,sizeof(zSalt),zSalt);` |
| 265687 |  108 | `		pNamePtr->zString = zName;` |
| 132846 |  109 | `	}else{` |
|      - |  110 | `		/* Method is condidate for 'overloading' */` |
|      3 |  111 | `		ph7_class_method *pCurrent = (ph7_class_method *)pEntry->pUserData;` |
|      3 |  112 | `		pNamePtr = &pMeth->sVmName;` |
|      - |  113 | `		/* Use the same VM name */` |
|      3 |  114 | `		SyStringDupPtr(pNamePtr,&pCurrent->sVmName);` |
|      3 |  115 | `		zName = (char *)pNamePtr->zString;` |
|      - |  116 | `	}` |
| 265689 |  117 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|     32 |  118 | `		if( (pName->nByte == sizeof("__construct") - 1 && SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1 ) == 0)` |
|     21 |  119 | `			\|\| (pName->nByte == sizeof("__destruct") - 1 && SyMemcmp(pName->zString,"__destruct",sizeof("__destruct") - 1 ) == 0)` |
|     22 |  120 | `			\|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|      - |  121 | `				/* Switch to public visibility when dealing with constructor/destructor */` |
|      5 |  122 | `				iProtection = PH7_CLASS_PROT_PUBLIC;` |
|      2 |  123 | `		}` |
|     12 |  124 | `	}` |
|      - |  125 | `	/* Initialize method fields */` |
| 265691 |  126 | `	pMeth->iProtection = iProtection;` |
| 265691 |  127 | `	pMeth->iFlags = iFlags;` |
| 265691 |  128 | `	pMeth->nLine = nLine;` |
| 398535 |  129 | `	PH7_VmInitFuncState(&(*pVm),&pMeth->sFunc,&zName[sizeof(char)*4/*[__@*/+SyStringLength(&pClass->sName)],` |
| 265686 |  130 | `		pName->nByte,iFuncFlags\|VM_FUNC_CLASS_METHOD,pClass);` |
| 265691 |  131 | `	return pMeth;` |
| 132849 |  132 |  |
|      - |  133 | `/*` |
|      - |  134 | ` * Check if the given name have a class method associated with it.` |
|      - |  135 | ` * Return the desired method [i.e: ph7_class_method instance] on success. NULL otherwise.` |
|      - |  136 | ` */` |
| 172974 |  137 | `PH7_PRIVATE ph7_class_method * PH7_ClassExtractMethod(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|      5 |  138 |  |
|      - |  139 | `	SyHashEntry *pEntry;` |
|      - |  140 | `	/* Perform a hash lookup */` |
| 172979 |  141 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)zName,nByte);` |
| 172979 |  142 | `	if( pEntry == 0 ){` |
|      - |  143 | `		/* No such entry */` |
|   4227 |  144 | `		return 0;` |
|      - |  145 | `	}` |
|      - |  146 | `	/* Point to the desired method */` |
| 168757 |  147 | `	return (ph7_class_method *)pEntry->pUserData;` |
|  86492 |  148 |  |
|      - |  149 | `/*` |
|      - |  150 | ` * Check if the given name is a class attribute.` |
|      - |  151 | ` * Return the desired attribute [i.e: ph7_class_attr instance] on success.NULL otherwise.` |
|      - |  152 | ` */` |
|  67868 |  153 | `PH7_PRIVATE ph7_class_attr * PH7_ClassExtractAttribute(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|      5 |  154 |  |
|      - |  155 | `	SyHashEntry *pEntry;` |
|      - |  156 | `	/* Perform a hash lookup */` |
|  67873 |  157 | `	pEntry = SyHashGet(&pClass->hAttr,(const void *)zName,nByte);` |
|  67873 |  158 | `	if( pEntry == 0 ){` |
|      - |  159 | `		/* No such entry */` |
|  67727 |  160 | `		return 0;` |
|      - |  161 | `	}` |
|      - |  162 | `	/* Point to the desierd method */` |
|    151 |  163 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|  33939 |  164 |  |
|      - |  165 | `/*` |
|      - |  166 | ` * Install a class attribute in the corresponding container.` |
|      - |  167 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|      - |  168 | ` */` |
|  67798 |  169 | `PH7_PRIVATE sxi32 PH7_ClassInstallAttr(ph7_class *pClass,ph7_class_attr *pAttr)` |
|      5 |  170 |  |
|  67803 |  171 | `	SyString *pName = &pAttr->sName;` |
|      - |  172 | `	sxi32 rc;` |
|      - |  173 | `	/* Remember where this attribute was originally declared so that later` |
|      - |  174 | `	 * inheritance/trait copies still know the declaring class (needed for` |
|      - |  175 | `	 * PHP-compatible error messages on typed properties). */` |
|  67803 |  176 | `	if( pAttr->pDeclClass == 0 ){` |
|  67803 |  177 | `		pAttr->pDeclClass = pClass;` |
|  33899 |  178 | `	}` |
|  67803 |  179 | `	rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|  67803 |  180 | `	return rc;` |
|      5 |  181 |  |
|      - |  182 | `/*` |
|      - |  183 | ` * Install a class method in the corresponding container.` |
|      - |  184 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|      - |  185 | ` */` |
| 265672 |  186 | `PH7_PRIVATE sxi32 PH7_ClassInstallMethod(ph7_class *pClass,ph7_class_method *pMeth)` |
|      5 |  187 |  |
| 265677 |  188 | `	SyString *pName = &pMeth->sFunc.sName;` |
|      - |  189 | `	sxi32 rc;` |
| 265677 |  190 | `	rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 265677 |  191 | `	return rc;` |
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
|   3582 |  338 | `static sxi32 OoCheckOverrideCompat(ph7_gen_state *pGen, ph7_class *pBase, ph7_class *pSub,` |
|      - |  339 | `	ph7_class_method *pParent, ph7_class_method *pChild)` |
|      5 |  340 |  |
|   3587 |  341 | `	ph7_vm *pVm = pGen->pVm;` |
|   3587 |  342 | `	ph7_vm_func *pPF = &pParent->sFunc;` |
|   3587 |  343 | `	ph7_vm_func *pCF = &pChild->sFunc;` |
|   3587 |  344 | `	SyString *pMName = &pCF->sName;` |
|      - |  345 | `	ph7_vm_func_arg *aP, *aC;` |
|      - |  346 | `	sxu32 nPArg, nCArg, k;` |
|   3587 |  347 | `	int bBad = 0;` |
|   3582 |  348 | `	if( pMName->nByte == sizeof("__construct")-1` |
|   3567 |  349 | `	 && SyStrnmicmp(pMName->zString,"__construct",pMName->nByte) == 0 ){` |
|   3539 |  350 | `		return SXRET_OK;` |
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
|   1796 |  389 |  |
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
|  74256 |  431 | `PH7_PRIVATE sxi32 PH7_ClassInherit(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase)` |
|      5 |  432 |  |
|      - |  433 | `	ph7_class_method *pMeth;` |
|      - |  434 | `	ph7_class_attr *pAttr;` |
|      - |  435 | `	SyHashEntry *pEntry;` |
|      - |  436 | `	SyString *pName;` |
|      - |  437 | `	sxi32 rc;` |
|      - |  438 | `	/* Install in the derived hashtable */` |
|  74261 |  439 | `	rc = SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|  74261 |  440 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  441 | `		return rc;` |
|      - |  442 | `	}` |
|      - |  443 | `	/* readonly class inheritance (PHP 8.2): a readonly class may only extend a` |
|      - |  444 | `	 * readonly class, and a non-readonly class may not extend a readonly one. */` |
|  74261 |  445 | `	if( (pBase->iFlags & PH7_CLASS_READONLY) != (pSub->iFlags & PH7_CLASS_READONLY) ){` |
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
|  74261 |  460 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
| 519189 |  461 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|      - |  462 | `		/* Make sure the private attributes are not redeclared in the subclass */` |
| 444933 |  463 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
| 444933 |  464 | `		pName = &pAttr->sName;` |
| 444933 |  465 | `		if( (pEntry = SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte)) != 0 ){` |
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
| 444927 |  490 | `		if( pAttr->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
| 444923 |  491 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
| 444923 |  492 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  493 | `				return rc;` |
|      - |  494 | `			}` |
| 222459 |  495 | `		}` |
|      5 |  496 | `	}` |
|  74261 |  497 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
| 741743 |  498 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|      - |  499 | `		/* Make sure the private/final methods are not redeclared in the subclass */` |
| 667487 |  500 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
| 667487 |  501 | `		pName = &pMeth->sFunc.sName;` |
| 667487 |  502 | `		if( (pEntry = SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|   3591 |  503 | `			 if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|      - |  504 | `				/* Cannot Overwrite final method */` |
|      8 |  505 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_method *)pEntry->pUserData)->nLine,` |
|      - |  506 | `					"Cannot Overwrite final method '%z:%z' inside child class '%z'",` |
|      2 |  507 | `					&pBase->sName,pName,&pSub->sName);` |
|      6 |  508 | `				if( rc == SXERR_ABORT ){` |
|    ! 0 |  509 | `					return SXERR_ABORT;` |
|      - |  510 | `				}` |
|      4 |  511 | `			}else{` |
|      - |  512 | `				/* Check the override's signature is compatible with the parent's. */` |
|   5378 |  513 | `				rc = OoCheckOverrideCompat(&(*pGen),pBase,pSub,pMeth,` |
|   3582 |  514 | `					(ph7_class_method *)pEntry->pUserData);` |
|   3587 |  515 | `				if( rc == SXERR_ABORT ){` |
|    ! 0 |  516 | `					return SXERR_ABORT;` |
|      - |  517 | `				}` |
|      - |  518 | `			}` |
|   3591 |  519 | `			continue;` |
|      - |  520 | `		}` |
|      - |  521 | `		/* Install the method */` |
| 663901 |  522 | `		if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
| 663899 |  523 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 663899 |  524 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  525 | `				return rc;` |
|      - |  526 | `			}` |
| 331947 |  527 | `		}` |
|      5 |  528 | `	}` |
|      - |  529 | `	/* Mark as subclass */` |
|  74261 |  530 | `	pSub->pBase = pBase;` |
|      - |  531 | `	/* All done */` |
|  74261 |  532 | `	return SXRET_OK;` |
|  37133 |  533 |  |
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
|  10592 |  657 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceInherit(ph7_class *pSub,ph7_class *pBase)` |
|      5 |  658 |  |
|      - |  659 | `	ph7_class_method *pMeth;` |
|      - |  660 | `	ph7_class_attr *pAttr;` |
|      - |  661 | `	SyHashEntry *pEntry;` |
|      - |  662 | `	SyString *pName;` |
|      - |  663 | `	sxi32 rc;` |
|      - |  664 | `	/* Install in the derived hashtable */` |
|  10597 |  665 | `	SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|  10597 |  666 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
|      - |  667 | `	/* Copy constants */` |
|  15895 |  668 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
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
|  10597 |  680 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
|      - |  681 | `	/* Copy methods signature */` |
|  19463 |  682 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|      - |  683 | `		/* Make sure the method are not redeclared in the subclass */` |
|   3575 |  684 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   3575 |  685 | `		pName = &pMeth->sFunc.sName;` |
|   3575 |  686 | `		if( SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte) == 0 ){` |
|      - |  687 | `			/* Install the method */` |
|   3575 |  688 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|   3575 |  689 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  690 | `				return rc;` |
|      - |  691 | `			}` |
|   1785 |  692 | `		}` |
|      5 |  693 | `	}` |
|      - |  694 | `	/* Mark as subclass */` |
|  10597 |  695 | `	pSub->pBase = pBase;` |
|      - |  696 | `	/* All done */` |
|  10597 |  697 | `	return SXRET_OK;` |
|   5301 |  698 |  |
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
|  95498 |  712 | `PH7_PRIVATE sxi32 PH7_ClassImplement(ph7_class *pMain,ph7_class *pInterface)` |
|      5 |  713 |  |
|      - |  714 | `	ph7_class_attr *pAttr;` |
|      - |  715 | `	SyHashEntry *pEntry;` |
|      - |  716 | `	SyString *pName;` |
|      - |  717 | `	sxi32 rc;` |
|      - |  718 | `	/* First off,copy all constants declared inside the interface */` |
|  95503 |  719 | `	SyHashResetLoopCursor(&pInterface->hAttr);` |
| 143258 |  720 | `	while((pEntry = SyHashGetNextEntry(&pInterface->hAttr)) != 0 ){` |
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
|  95503 |  734 | `	SySetPut(&pMain->aInterface,(const void *)&pInterface);` |
|      - |  735 | `	/* Install interface method stubs into the implementing class.` |
|      - |  736 | `	 * Methods already defined in the class take precedence (they satisfy` |
|      - |  737 | `	 * the interface contract). Stubs retain PH7_CLASS_ATTR_ABSTRACT so` |
|      - |  738 | `	 * the unified check in GenStateCheckAbstractMethods catches missing ones.` |
|      - |  739 | `	 */` |
|      - |  740 | `	{` |
|      - |  741 | `		ph7_class_method *pMeth;` |
|      - |  742 | `		SyHashEntry *pMEntry;` |
|      - |  743 | `		SyString *pMName;` |
|  95503 |  744 | `		SyHashResetLoopCursor(&pInterface->hMethod);` |
| 302462 |  745 | `		while((pMEntry = SyHashGetNextEntry(&pInterface->hMethod)) != 0 ){` |
| 159215 |  746 | `			pMeth = (ph7_class_method *)pMEntry->pUserData;` |
| 159215 |  747 | `			pMName = &pMeth->sFunc.sName;` |
| 159215 |  748 | `			if( SyHashGet(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte) == 0 ){` |
|     18 |  749 | `				rc = SyHashInsert(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte,pMeth);` |
|     18 |  750 | `				if( rc != SXRET_OK ){` |
|    ! 0 |  751 | `					return rc;` |
|      - |  752 | `				}` |
|      7 |  753 | `			}` |
|      5 |  754 | `		}` |
|      - |  755 | `	}` |
|  95503 |  756 | `	return SXRET_OK;` |
|  47754 |  757 |  |
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
|   2666 |  837 | `static ph7_class_instance * NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|      5 |  838 |  |
|      - |  839 | `	ph7_class_instance *pThis;` |
|      - |  840 | `	/* Allocate a new instance */` |
|   2671 |  841 | `	pThis = (ph7_class_instance *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_instance));` |
|   2671 |  842 | `	if( pThis == 0 ){` |
|    ! 0 |  843 | `		return 0;` |
|      - |  844 | `	}` |
|      - |  845 | `	/* Zero the structure */` |
|   2671 |  846 | `	SyZero(pThis,sizeof(ph7_class_instance));` |
|      - |  847 | `	/* Initialize fields */` |
|   2671 |  848 | `	pThis->iRef = 1;` |
|   2671 |  849 | `	pThis->pVm = pVm;` |
|   2671 |  850 | `	pThis->pClass = pClass;` |
|      - |  851 | `	/* Assign a fresh monotonic object handle id (clones get their own, like PHP). */` |
|   2671 |  852 | `	pThis->nObjId = pVm->nNextObjId++;` |
|   2671 |  853 | `	SyHashInit(&pThis->hAttr,&pVm->sAllocator,0,0);` |
|   2671 |  854 | `	return pThis;` |
|   1338 |  855 |  |
|      - |  856 | `/*` |
|      - |  857 | ` * Wrapper around the NewClassInstance() function defined above.` |
|      - |  858 | ` * See the block comment above for more information.` |
|      - |  859 | ` */` |
|   2614 |  860 | `PH7_PRIVATE ph7_class_instance * PH7_NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|      5 |  861 |  |
|      - |  862 | `	ph7_class_instance *pNew;` |
|      - |  863 | `	sxi32 rc;` |
|   2619 |  864 | `	pNew = NewClassInstance(&(*pVm),&(*pClass));` |
|   2619 |  865 | `	if( pNew == 0 ){` |
|    ! 0 |  866 | `		return 0;` |
|      - |  867 | `	}` |
|      - |  868 | `	/* Associate a private VM frame with this class instance */` |
|   2619 |  869 | `	rc = PH7_VmCreateClassInstanceFrame(&(*pVm),pNew);` |
|   2619 |  870 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  871 | `		SyMemBackendPoolFree(&pVm->sAllocator,pNew);` |
|    ! 0 |  872 | `		return 0;` |
|      - |  873 | `	}` |
|   2619 |  874 | `	return pNew;` |
|   1312 |  875 |  |
|      - |  876 | `/*` |
|      - |  877 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon] attribute.` |
|      - |  878 | ` * This function never fail.` |
|      - |  879 | ` */` |
|   1902 |  880 | `static ph7_value * ExtractClassAttrValue(ph7_vm *pVm,VmClassAttr *pAttr)` |
|      5 |  881 |  |
|      - |  882 | `	/* Extract the value */` |
|      - |  883 | `	ph7_value *pValue;` |
|   1907 |  884 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|   1907 |  885 | `	return pValue;` |
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
|   1874 | 1035 | `static void PH7_ClassInstanceRelease(ph7_class_instance *pThis)` |
|      5 | 1036 |  |
|      - | 1037 | `	ph7_class_method *pDestr;` |
|      - | 1038 | `	SyHashEntry *pEntry;` |
|      - | 1039 | `	ph7_class *pClass;` |
|      - | 1040 | `	ph7_vm *pVm;` |
|   1879 | 1041 | `	if( pThis->iFlags & CLASS_INSTANCE_DESTROYED ){` |
|      - | 1042 | `		/*` |
|      - | 1043 | `		 * Already destroyed,return immediately.` |
|      - | 1044 | `		 * This could happend if someone perform unset($this) in the destructor body.` |
|      - | 1045 | `		 */` |
|      9 | 1046 | `		return;` |
|      - | 1047 | `	}` |
|      - | 1048 | `	/* Mark as destroyed */` |
|   1871 | 1049 | `	pThis->iFlags \|= CLASS_INSTANCE_DESTROYED;` |
|      - | 1050 | `	/* Invoke any defined destructor if available */` |
|   1871 | 1051 | `	pVm = pThis->pVm;` |
|   1871 | 1052 | `	pClass = pThis->pClass;` |
|   1871 | 1053 | `	pDestr = PH7_ClassExtractMethod(pClass,"__destruct",sizeof("__destruct")-1);` |
|   1871 | 1054 | `	if( pDestr && !pVm->bInReset ){` |
|      - | 1055 | `		/* Invoke the destructor. Skipped during ph7_vm_reset() bulk teardown:` |
|      - | 1056 | `		 * running user PHP against a half-reset VM is unsafe (see bInReset). */` |
|     13 | 1057 | `		pThis->iRef = 2; /* Prevent garbage collection */` |
|     13 | 1058 | `		PH7_VmCallClassMethod(pVm,pThis,pDestr,0,0,0);` |
|      6 | 1059 | `	}` |
|      - | 1060 | `	/* Release non-static attributes */` |
|   1871 | 1061 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|   8941 | 1062 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|   7075 | 1063 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|   7075 | 1064 | `		if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|      - | 1065 | `			/* Drop any typed-property enforcement slot registered for this` |
|      - | 1066 | `			 * memobj. Must happen before the memobj is returned to the free` |
|      - | 1067 | `			 * list so a future recycled slot does not inherit the stale entry. */` |
|   7053 | 1068 | `			if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|    274 | 1069 | `				SyHashDeleteEntry(&pVm->hTypedSlot,` |
|    180 | 1070 | `					(const void *)&pVmAttr->nIdx,sizeof(sxu32),0);` |
|     90 | 1071 | `			}` |
|   7053 | 1072 | `			PH7_VmUnsetMemObj(pVm,pVmAttr->nIdx,TRUE);` |
|   3524 | 1073 | `		}` |
|   7075 | 1074 | `		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      5 | 1075 | `	}` |
|      - | 1076 | `	/* Release the whole structure */` |
|   1871 | 1077 | `	SyHashRelease(&pThis->hAttr);` |
|   1871 | 1078 | `	SyMemBackendPoolFree(&pVm->sAllocator,pThis);` |
|    942 | 1079 |  |
|      - | 1080 | `/*` |
|      - | 1081 | ` * Decrement the reference count of a class instance [i.e Object in the PHP jargon].` |
|      - | 1082 | ` * If the reference count reaches zero,release the whole instance.` |
|      - | 1083 | ` */` |
|  35402 | 1084 | `PH7_PRIVATE void PH7_ClassInstanceUnref(ph7_class_instance *pThis)` |
|      5 | 1085 |  |
|  35407 | 1086 | `	pThis->iRef--;` |
|  35407 | 1087 | `	if( pThis->iRef < 1 ){` |
|      - | 1088 | `		/* No more reference to this instance */` |
|   1879 | 1089 | `		PH7_ClassInstanceRelease(&(*pThis));` |
|    937 | 1090 | `	}` |
|  35407 | 1091 |  |
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
|    174 | 1169 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest)` |
|      3 | 1170 |  |
|      - | 1171 | `	SyHashEntry *pEntry,*pEntry2;` |
|      - | 1172 | `	ph7_value sV1,sV2;` |
|      - | 1173 | `	sxi32 rc;` |
|    177 | 1174 | `	if( iNest > 31 ){` |
|      - | 1175 | `		/* Nesting limit reached */` |
|      6 | 1176 | `		PH7_VmThrowError(pLeft->pVm,0,PH7_CTX_ERR,"Nesting limit reached: Infinite recursion?");` |
|      6 | 1177 | `		return 1;` |
|      - | 1178 | `	}` |
|      - | 1179 | `	/* Comparison is performed only if the objects are instance of the same class */` |
|    173 | 1180 | `	if( pLeft->pClass != pRight->pClass ){` |
|      7 | 1181 | `		return 1;` |
|      - | 1182 | `	}` |
|    167 | 1183 | `	if( bStrict ){` |
|      - | 1184 | `		/*` |
|      - | 1185 | `		 * According to the PHP language reference manual:` |
|      - | 1186 | `		 *  when using the identity operator (===), object variables` |
|      - | 1187 | `		 *  are identical if and only if they refer to the same instance` |
|      - | 1188 | `		 *  of the same class.` |
|      - | 1189 | `		 */` |
|     25 | 1190 | `		return !(pLeft == pRight);` |
|      - | 1191 | `	}` |
|      - | 1192 | `	/*` |
|      - | 1193 | `	 * Attribute comparison.` |
|      - | 1194 | `	 * According to the PHP reference manual:` |
|      - | 1195 | `	 *  When using the comparison operator (==), object variables are compared` |
|      - | 1196 | `	 *  in a simple manner, namely: Two object instances are equal if they have` |
|      - | 1197 | `	 *  the same attributes and values, and are instances of the same class.` |
|      - | 1198 | `	 */` |
|    143 | 1199 | `	if( pLeft == pRight ){` |
|      - | 1200 | `		/* Same instance,don't bother processing,object are equals */` |
|      3 | 1201 | `		return 0;` |
|      - | 1202 | `	}` |
|    141 | 1203 | `	SyHashResetLoopCursor(&pLeft->hAttr);` |
|    141 | 1204 | `	SyHashResetLoopCursor(&pRight->hAttr);` |
|    141 | 1205 | `	PH7_MemObjInit(pLeft->pVm,&sV1);` |
|    141 | 1206 | `	PH7_MemObjInit(pLeft->pVm,&sV2);` |
|    141 | 1207 | `	sV1.nIdx = sV2.nIdx = SXU32_HIGH;` |
|    224 | 1208 | `	while((pEntry = SyHashGetNextEntry(&pLeft->hAttr)) != 0 && (pEntry2 = SyHashGetNextEntry(&pRight->hAttr)) != 0 ){` |
|    147 | 1209 | `		VmClassAttr *p1 = (VmClassAttr *)pEntry->pUserData;` |
|    147 | 1210 | `		VmClassAttr *p2 = (VmClassAttr *)pEntry2->pUserData;` |
|      - | 1211 | `		/* Compare only non-static attribute */` |
|    147 | 1212 | `		if( (p1->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|      - | 1213 | `			ph7_value *pL,*pR;` |
|    147 | 1214 | `			pL = ExtractClassAttrValue(pLeft->pVm,p1);` |
|    147 | 1215 | `			pR = ExtractClassAttrValue(pRight->pVm,p2);` |
|    147 | 1216 | `			if( pL && pR ){` |
|    147 | 1217 | `				PH7_MemObjLoad(pL,&sV1);` |
|    147 | 1218 | `				PH7_MemObjLoad(pR,&sV2);` |
|      - | 1219 | `				/* Compare the two values now */` |
|    147 | 1220 | `				rc = PH7_MemObjCmp(&sV1,&sV2,bStrict,iNest+1);` |
|    147 | 1221 | `				PH7_MemObjRelease(&sV1);` |
|    147 | 1222 | `				PH7_MemObjRelease(&sV2);` |
|    147 | 1223 | `				if( rc != 0 ){` |
|      - | 1224 | `					/* Not equals */` |
|    133 | 1225 | `					return rc;` |
|      - | 1226 | `				}` |
|      7 | 1227 | `			}` |
|      7 | 1228 | `		}` |
|      1 | 1229 | `	}` |
|      - | 1230 | `	/* Object are equals */` |
|      9 | 1231 | `	return 0;` |
|     90 | 1232 |  |
|      - | 1233 | `/*` |
|      - | 1234 | ` * Dump a class instance and the store the dump in the BLOB given` |
|      - | 1235 | ` * as the first argument.` |
|      - | 1236 | ` * Note that only non-static/non-constants attribute are dumped.` |
|      - | 1237 | ` * This function is typically invoked when the user issue a call` |
|      - | 1238 | ` * to [var_dump(),var_export(),print_r(),...].` |
|      - | 1239 | ` * This function SXRET_OK on success. Any other return value including` |
|      - | 1240 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|      - | 1241 | ` */` |
|      - | 1242 | `/*` |
|      - | 1243 | ` * Emit a class-instance dump header plus its trailing newline. For var_dump` |
|      - | 1244 | ` * (ShowType) it completes the "object(" prefix the caller already emitted as` |
|      - | 1245 | ` *   ClassName)#<id> (<count>) {` |
|      - | 1246 | ` * for print_r it emits the legacy PHL  Object(ClassName) {  (count/id unused).` |
|      - | 1247 | ` */` |
|    134 | 1248 | `static void DumpClassInstanceHeader(SyBlob *pOut,ph7_class *pClass,sxu32 nObjId,int ShowType,sxu32 nCount)` |
|      3 | 1249 |  |
|    137 | 1250 | `	if( !ShowType ){` |
|      6 | 1251 | `		SyBlobAppend(&(*pOut),"Object(",sizeof("Object(")-1);` |
|      6 | 1252 | `		SyBlobFormat(&(*pOut),"%z) {",&pClass->sName);` |
|      4 | 1253 | `	}else{` |
|    133 | 1254 | `		SyBlobFormat(&(*pOut),"%z)#%u (%u) {",&pClass->sName,nObjId,nCount);` |
|      - | 1255 | `	}` |
|      - | 1256 | `#ifdef __WINNT__` |
|      3 | 1257 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|      - | 1258 | `#else` |
|    134 | 1259 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      - | 1260 | `#endif` |
|    137 | 1261 |  |
|    138 | 1262 | `PH7_PRIVATE sxi32 PH7_ClassInstanceDump(SyBlob *pOut,ph7_class_instance *pThis,int ShowType,int nTab,int nDepth)` |
|      3 | 1263 |  |
|      - | 1264 | `	SyHashEntry *pEntry;` |
|      - | 1265 | `	ph7_value *pValue;` |
|      - | 1266 | `	sxi32 rc;` |
|      - | 1267 | `	int i;` |
|    141 | 1268 | `	if( nDepth > 31 ){` |
|      - | 1269 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|      - | 1270 | `		/* Nesting limit reached..halt immediately*/` |
|      5 | 1271 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|      5 | 1272 | `		if( ShowType ){` |
|      5 | 1273 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|      2 | 1274 | `		}` |
|      5 | 1275 | `		return SXERR_LIMIT;` |
|      - | 1276 | `	}` |
|    137 | 1277 | `	rc = SXRET_OK;` |
|      - | 1278 | `	{` |
|      - | 1279 | `		/* Both var_dump and print_r consult __debugInfo() (PHP behavior);` |
|      - | 1280 | `		 * var_export uses a separate renderer and never reaches here. When the` |
|      - | 1281 | `		 * method is present and returns an array, render that array's entries as` |
|      - | 1282 | `		 * the object body, with the header showing the debug array's count. The` |
|      - | 1283 | `		 * nDepth guard above protects against a __debugInfo returning the object` |
|      - | 1284 | `		 * itself. */` |
|    137 | 1285 | `		ph7_class_method *pDbg = PH7_ClassExtractMethod(pThis->pClass,"__debugInfo",sizeof("__debugInfo")-1);` |
|    137 | 1286 | `		if( pDbg ){` |
|      - | 1287 | `			ph7_value sResult;` |
|      5 | 1288 | `			PH7_MemObjInit(pThis->pVm,&sResult);` |
|      5 | 1289 | `			PH7_VmCallClassMethod(pThis->pVm,pThis,pDbg,&sResult,0,0);` |
|      5 | 1290 | `			if( sResult.iFlags & MEMOBJ_HASHMAP ){` |
|      5 | 1291 | `				ph7_hashmap *pMap = (ph7_hashmap *)sResult.x.pOther;` |
|      - | 1292 | `				/* Header count is the debug array's entry count. */` |
|      5 | 1293 | `				DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,pMap->nEntry);` |
|      5 | 1294 | `				rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth);` |
|      9 | 1295 | `				for( i = 0 ; i < nTab ; i++ ){` |
|      5 | 1296 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      3 | 1297 | `				}` |
|      5 | 1298 | `				SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      5 | 1299 | `				PH7_MemObjRelease(&sResult);` |
|      5 | 1300 | `				return rc;` |
|      - | 1301 | `			}` |
|      - | 1302 | `			/* Non-array return: behave as if __debugInfo were absent. */` |
|    ! 0 | 1303 | `			PH7_MemObjRelease(&sResult);` |
|    ! 0 | 1304 | `		}` |
|      - | 1305 | `	}` |
|      - | 1306 | `	{` |
|      - | 1307 | `		/* var_dump's header needs the property count up front, so pre-count the` |
|      - | 1308 | `		 * non-static/non-constant attributes (matching the dump loop below). */` |
|    132 | 1309 | `		sxu32 nProp = 0;` |
|    132 | 1310 | `		if( ShowType ){` |
|    130 | 1311 | `			SyHashResetLoopCursor(&pThis->hAttr);` |
|    266 | 1312 | `			while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|    138 | 1313 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    138 | 1314 | `				if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|    134 | 1315 | `					nProp++;` |
|     66 | 1316 | `				}` |
|      2 | 1317 | `			}` |
|     64 | 1318 | `		}` |
|    132 | 1319 | `		DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,nProp);` |
|      - | 1320 | `	}` |
|      - | 1321 | `	/* Dump object attributes */` |
|    132 | 1322 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|    213 | 1323 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|    142 | 1324 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    142 | 1325 | `		if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|      - | 1326 | `			/* Dump non-static/constant attribute only */` |
|   3994 | 1327 | `			for( i = 0 ; i < nTab ; i++ ){` |
|   3858 | 1328 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|   1930 | 1329 | `			}` |
|    138 | 1330 | `			pValue = ExtractClassAttrValue(pThis->pVm,pVmAttr);` |
|    138 | 1331 | `			if( pValue ){` |
|    138 | 1332 | `				SyBlobFormat(&(*pOut),"['%z'] =>",&pVmAttr->pAttr->sName);` |
|      - | 1333 | `#ifdef __WINNT__` |
|      2 | 1334 | `				SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|      - | 1335 | `#else` |
|    136 | 1336 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      - | 1337 | `#endif` |
|    138 | 1338 | `				rc = PH7_MemObjDump(&(*pOut),pValue,ShowType,nTab+1,nDepth,0);` |
|    138 | 1339 | `				if( rc == SXERR_LIMIT ){` |
|    125 | 1340 | `					break;` |
|      - | 1341 | `				}` |
|      6 | 1342 | `			}` |
|      6 | 1343 | `		}` |
|      2 | 1344 | `	}` |
|   3982 | 1345 | `	for( i = 0 ; i < nTab ; i++ ){` |
|   3852 | 1346 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|   1927 | 1347 | `	}` |
|    132 | 1348 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|    132 | 1349 | `	return rc;` |
|     72 | 1350 |  |
|      - | 1351 | `/*` |
|      - | 1352 | ` * Call a magic method [i.e: __toString(),__toBool(),__Invoke()...]` |
|      - | 1353 | ` * Return SXRET_OK on successfull call. Any other return value indicates failure.` |
|      - | 1354 | ` * Notes on magic methods.` |
|      - | 1355 | ` * According to the PHP language reference manual.` |
|      - | 1356 | ` *  The function names __construct(), __destruct(), __call(), __callStatic()` |
|      - | 1357 | ` *  __get(),  __toString(), __invoke(), __clone() are magical in PHP classes.` |
|      - | 1358 | ` * You cannot have functions with these names in any of your classes unless` |
|      - | 1359 | ` * you want the magic functionality associated with them.` |
|      - | 1360 | ` * Example of magical methods:` |
|      - | 1361 | ` * __toString()` |
|      - | 1362 | ` *  The __toString() method allows a class to decide how it will react when it is treated like` |
|      - | 1363 | ` *  a string. For example, what echo $obj; will print. This method must return a string.` |
|      - | 1364 | ` *  Example #2 Simple example` |
|      - | 1365 | ` * <?php` |
|      - | 1366 | ` * // Declare a simple class` |
|      - | 1367 | ` * class TestClass` |
|      - | 1368 | ` * {` |
|      - | 1369 | ` *   public $foo;` |
|      - | 1370 | ` *` |
|      - | 1371 | ` *   public function __construct($foo)` |
|      - | 1372 | ` *   {` |
|      - | 1373 | ` *       $this->foo = $foo;` |
|      - | 1374 | ` *   }` |
|      - | 1375 | ` *` |
|      - | 1376 | ` *   public function __toString()` |
|      - | 1377 | ` *   {` |
|      - | 1378 | ` *       return $this->foo;` |
|      - | 1379 | ` *   }` |
|      - | 1380 | ` * }` |
|      - | 1381 | ` * $class = new TestClass('Hello');` |
|      - | 1382 | ` * echo $class;` |
|      - | 1383 | ` * ?>` |
|      - | 1384 | ` * The above example will output:` |
|      - | 1385 | ` *  Hello` |
|      - | 1386 | ` *` |
|      - | 1387 | ` * Note that PH7 does not support all the magical method and introudces __toFloat(),__toInt()` |
|      - | 1388 | ` * which have the same behaviour as __toString() but for float and integer types` |
|      - | 1389 | ` * respectively.` |
|      - | 1390 | ` * Refer to the official documentation for more information.` |
|      - | 1391 | ` */` |
|      2 | 1392 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallMagicMethod(` |
|      - | 1393 | `	ph7_vm *pVm,               /* VM that own all this stuff */` |
|      - | 1394 | `	ph7_class *pClass,         /* Target class */` |
|      - | 1395 | `	ph7_class_instance *pThis, /* Target object */` |
|      - | 1396 | `	const char *zMethod,       /* Magic method name [i.e: __toString()]*/` |
|      - | 1397 | `	sxu32 nByte,               /* zMethod length*/` |
|      - | 1398 | `	const SyString *pAttrName  /* Attribute name */` |
|      - | 1399 | `	)` |
|      1 | 1400 |  |
|      3 | 1401 | `	ph7_value *apArg[2] = { 0 , 0 };` |
|      - | 1402 | `	ph7_class_method *pMeth;` |
|      - | 1403 | `	ph7_value sAttr; /* cc warning */` |
|      - | 1404 | `	sxi32 rc;` |
|      - | 1405 | `	int nArg;` |
|      - | 1406 | `	/* Make sure the magic method is available */` |
|      3 | 1407 | `	pMeth = PH7_ClassExtractMethod(&(*pClass),zMethod,nByte);` |
|      3 | 1408 | `	if( pMeth == 0 ){` |
|      - | 1409 | `		/* No such method,return immediately */` |
|      3 | 1410 | `		return SXERR_NOTFOUND;` |
|      - | 1411 | `	}` |
|    ! 0 | 1412 | `	nArg = 0;` |
|      - | 1413 | `	/* Copy arguments */` |
|    ! 0 | 1414 | `	if( pAttrName ){` |
|    ! 0 | 1415 | `		PH7_MemObjInitFromString(pVm,&sAttr,pAttrName);` |
|    ! 0 | 1416 | `		sAttr.nIdx = SXU32_HIGH; /* Mark as constant */` |
|    ! 0 | 1417 | `		apArg[0] = &sAttr;` |
|    ! 0 | 1418 | `		nArg = 1;` |
|    ! 0 | 1419 | `	}` |
|      - | 1420 | `	/* Call the magic method now */` |
|    ! 0 | 1421 | `	rc = PH7_VmCallClassMethod(pVm,&(*pThis),pMeth,0,nArg,apArg);` |
|      - | 1422 | `	/* Clean up */` |
|    ! 0 | 1423 | `	if( pAttrName ){` |
|    ! 0 | 1424 | `		PH7_MemObjRelease(&sAttr);` |
|    ! 0 | 1425 | `	}` |
|    ! 0 | 1426 | `	return rc;` |
|      2 | 1427 |  |
|      - | 1428 | `/*` |
|      - | 1429 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon].` |
|      - | 1430 | ` * This function is simply a wrapper on ExtractClassAttrValue().` |
|      - | 1431 | ` */` |
|     74 | 1432 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceExtractAttrValue(ph7_class_instance *pThis,VmClassAttr *pAttr)` |
|      3 | 1433 |  |
|      - | 1434 | `   /* Extract the attribute value */` |
|      - | 1435 | `	ph7_value *pValue;` |
|     77 | 1436 | `	pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     77 | 1437 | `	return pValue;` |
|      3 | 1438 |  |
|      - | 1439 | `/*` |
|      - | 1440 | ` * Convert a class instance [i.e: Object in the PHP jargon] into a hashmap [i.e: array in the PHP jargon].` |
|      - | 1441 | ` * Return SXRET_OK on success. Any other value indicates failure.` |
|      - | 1442 | ` * Note on object conversion to array:` |
|      - | 1443 | ` *  Acccording to the PHP language reference manual` |
|      - | 1444 | ` *  If an object is converted to an array, the result is an array whose elements are the object's properties.` |
|      - | 1445 | ` *  The keys are the member variable names.` |
|      - | 1446 | ` *` |
|      - | 1447 | ` *  The following example:` |
|      - | 1448 | ` *  class Test {` |
|      - | 1449 | ` *   public $A = 25<<1;  // 50` |
|      - | 1450 | ` *	 public $c = rand_str(3);   // Random string of length 3` |
|      - | 1451 | ` *	 public $d = rand() & 1023; // Random number between 0..1023` |
|      - | 1452 | ` *  }` |
|      - | 1453 | ` *  var_dump((array) new Test());` |
|      - | 1454 | ` *	Will output:` |
|      - | 1455 | ` *  array(3) {` |
|      - | 1456 | ` *   [A] =>` |
|      - | 1457 | ` *      int(50)` |
|      - | 1458 | ` *   [c] =>` |
|      - | 1459 | ` *     string(3 'aps')` |
|      - | 1460 | ` *   [d] =>` |
|      - | 1461 | ` *     int(991)` |
|      - | 1462 | ` *  }` |
|      - | 1463 | ` * You have noticed that PH7 allow class attributes [i.e: $a,$c,$d in the example above]` |
|      - | 1464 | ` * have any complex expression (even function calls/Annonymous functions) as their default` |
|      - | 1465 | ` * value unlike the standard PHP engine.` |
|      - | 1466 | ` * This is a very powerful feature that you have to look at.` |
|      - | 1467 | ` */` |
|      6 | 1468 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|      1 | 1469 |  |
|      - | 1470 | `	SyHashEntry *pEntry;` |
|      - | 1471 | `	SyString *pAttrName;` |
|      - | 1472 | `	VmClassAttr *pAttr;` |
|      - | 1473 | `	ph7_value *pValue;` |
|      - | 1474 | `	ph7_value sName;` |
|      - | 1475 | `	/* Reset the loop cursor */` |
|      7 | 1476 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      7 | 1477 | `	PH7_MemObjInitFromString(pThis->pVm,&sName,0);` |
|     20 | 1478 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|      - | 1479 | `		/* Point to the current attribute */` |
|     11 | 1480 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1481 | `		/* Extract attribute value */` |
|     11 | 1482 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     11 | 1483 | `		if( pValue ){` |
|      - | 1484 | `			/* Build attribute name */` |
|     11 | 1485 | `			pAttrName = &pAttr->pAttr->sName;` |
|     11 | 1486 | `			PH7_MemObjStringAppend(&sName,pAttrName->zString,pAttrName->nByte);` |
|      - | 1487 | `			/* Perform the insertion */` |
|     11 | 1488 | `			PH7_HashmapInsert(pMap,&sName,pValue);` |
|      - | 1489 | `			/* Reset the string cursor */` |
|     11 | 1490 | `			SyBlobReset(&sName.sBlob);` |
|      5 | 1491 | `		}` |
|      1 | 1492 | `	}` |
|      7 | 1493 | `	PH7_MemObjRelease(&sName);` |
|      7 | 1494 | `	return SXRET_OK;` |
|      1 | 1495 |  |
|      - | 1496 | `/*` |
|      - | 1497 | ` * Iterate throw class attributes and invoke the given callback [i.e: xWalk()] for each` |
|      - | 1498 | ` * retrieved attribute.` |
|      - | 1499 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|      - | 1500 | ` * the attribute value in the callback body will not alter the real attribute value.` |
|      - | 1501 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|      - | 1502 | ` * a value different from PH7_OK.` |
|      - | 1503 | ` * Refer to [ph7_object_walk()] for more information.` |
|      - | 1504 | ` */` |
|      2 | 1505 | `PH7_PRIVATE sxi32 PH7_ClassInstanceWalk(` |
|      - | 1506 | `	ph7_class_instance *pThis, /* Target object */` |
|      - | 1507 | `	int (*xWalk)(const char *,ph7_value *,void *), /* Walker callback */` |
|      - | 1508 | `	void *pUserData /* Last argument to xWalk() */` |
|      - | 1509 | `	)` |
|      1 | 1510 |  |
|      - | 1511 | `	SyHashEntry *pEntry; /* Hash entry */` |
|      - | 1512 | `	VmClassAttr *pAttr;  /* Pointer to the attribute */` |
|      - | 1513 | `	ph7_value *pValue;   /* Attribute value */` |
|      - | 1514 | `	ph7_value sValue;    /* Copy of the attribute value */` |
|      - | 1515 | `	int rc;` |
|      - | 1516 | `	/* Reset the loop cursor */` |
|      3 | 1517 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      3 | 1518 | `	PH7_MemObjInit(pThis->pVm,&sValue);` |
|      - | 1519 | `	/* Start the walk process */` |
|      8 | 1520 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|      - | 1521 | `		/* Point to the current attribute */` |
|      5 | 1522 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1523 | `		/* Extract attribute value */` |
|      5 | 1524 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|      5 | 1525 | `		if( pValue ){` |
|      5 | 1526 | `			PH7_MemObjLoad(pValue,&sValue);` |
|      - | 1527 | `			/* Invoke the supplied callback */` |
|      5 | 1528 | `			rc =  xWalk(SyStringData(&pAttr->pAttr->sName),&sValue,pUserData);` |
|      5 | 1529 | `			PH7_MemObjRelease(&sValue);` |
|      5 | 1530 | `			if( rc != PH7_OK){` |
|      - | 1531 | `				/* User callback request an operation abort */` |
|    ! 0 | 1532 | `				return SXERR_ABORT;` |
|      - | 1533 | `			}` |
|      2 | 1534 | `		}` |
|      1 | 1535 | `	}` |
|      - | 1536 | `	/* All done */` |
|      3 | 1537 | `	return SXRET_OK;` |
|      2 | 1538 |  |
|      - | 1539 | `/*` |
|      - | 1540 | ` * Extract a class atrribute value.` |
|      - | 1541 | ` * Return a pointer to the attribute value on success. Otherwise NULL.` |
|      - | 1542 | ` * Note:` |
|      - | 1543 | ` *  Access to static and constant attribute is not allowed. That is,the function` |
|      - | 1544 | ` *  will return NULL in case someone (host-application code) try to extract` |
|      - | 1545 | ` *  a static/constant attribute.` |
|      - | 1546 | ` */` |
|   1278 | 1547 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceFetchAttr(ph7_class_instance *pThis,const SyString *pName)` |
|      5 | 1548 |  |
|      - | 1549 | `	SyHashEntry *pEntry;` |
|      - | 1550 | `	VmClassAttr *pAttr;` |
|      - | 1551 | `	/* Query the attribute hashtable */` |
|   1283 | 1552 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);` |
|   1283 | 1553 | `	if( pEntry == 0 ){` |
|      - | 1554 | `		/* No such attribute */` |
|    ! 0 | 1555 | `		return 0;` |
|      - | 1556 | `	}` |
|      - | 1557 | `	/* Point to the class atrribute */` |
|   1283 | 1558 | `	pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1559 | `	/* Check if we are dealing with a static/constant attribute */` |
|   1283 | 1560 | `	if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|      - | 1561 | `		/* Access is forbidden */` |
|    ! 0 | 1562 | `		return 0;` |
|      - | 1563 | `	}` |
|      - | 1564 | `	/* Return the attribute value */` |
|   1283 | 1565 | `	return ExtractClassAttrValue(pThis->pVm,pAttr);` |
|    644 | 1566 |  |
|      - | 1567 |  |
