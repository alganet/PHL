# src/ph7/oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 633/698 lines (90.69%)

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
| 154512 |   14 | `PH7_PRIVATE ph7_class * PH7_NewRawClass(ph7_vm *pVm,const SyString *pName,sxu32 nLine)` |
|      5 |   15 | `{` |
|      - |   16 | `	ph7_class *pClass;` |
|      - |   17 | `	char *zName;` |
|      - |   18 | `	/* Allocate a new instance */` |
| 154517 |   19 | `	pClass = (ph7_class *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class));` |
| 154517 |   20 | `	if( pClass == 0 ){` |
|    ! 0 |   21 | `		return 0;` |
|      - |   22 | `	}` |
|      - |   23 | `	/* Zero the structure */` |
| 154517 |   24 | `	SyZero(pClass,sizeof(ph7_class));` |
|      - |   25 | `	/* Duplicate class name */` |
| 154517 |   26 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
| 154517 |   27 | `	if( zName == 0 ){` |
|    ! 0 |   28 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClass);` |
|    ! 0 |   29 | `		return 0;` |
|      - |   30 | `	}` |
|      - |   31 | `	/* Initialize fields */` |
| 154517 |   32 | `	SyStringInitFromBuf(&pClass->sName,zName,pName->nByte);` |
| 154517 |   33 | `	SyHashInit(&pClass->hMethod,&pVm->sAllocator,0,0);` |
| 154517 |   34 | `	SyHashInit(&pClass->hAttr,&pVm->sAllocator,0,0);` |
| 154517 |   35 | `	SyHashInit(&pClass->hDerived,&pVm->sAllocator,0,0);` |
| 154517 |   36 | `	SySetInit(&pClass->aInterface,&pVm->sAllocator,sizeof(ph7_class *));` |
| 154517 |   37 | `	SySetInit(&pClass->aTrait,&pVm->sAllocator,sizeof(ph7_class *));` |
| 154517 |   38 | `	pClass->nLine = nLine;` |
|      - |   39 | `	/* All done */` |
| 154517 |   40 | `	return pClass;` |
|  77261 |   41 | `}` |
|      - |   42 | `/*` |
|      - |   43 | ` * Allocate and initialize a new class attribute.` |
|      - |   44 | ` * Return a pointer to the class attribute on success. NULL otherwise.` |
|      - |   45 | ` */` |
|  81180 |   46 | `PH7_PRIVATE ph7_class_attr * PH7_NewClassAttr(ph7_vm *pVm,const SyString *pName,sxu32 nLine,sxi32 iProtection,sxi32 iFlags)` |
|      5 |   47 | `{` |
|      - |   48 | `	ph7_class_attr *pAttr;` |
|      - |   49 | `	char *zName;` |
|  81185 |   50 | `	pAttr = (ph7_class_attr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_attr));` |
|  81185 |   51 | `	if( pAttr == 0 ){` |
|    ! 0 |   52 | `		return 0;` |
|      - |   53 | `	}` |
|      - |   54 | `	/* Zero the structure */` |
|  81185 |   55 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|      - |   56 | `	/* Duplicate attribute name */` |
|  81185 |   57 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  81185 |   58 | `	if( zName == 0 ){` |
|    ! 0 |   59 | `		SyMemBackendPoolFree(&pVm->sAllocator,pAttr);` |
|    ! 0 |   60 | `		return 0;` |
|      - |   61 | `	}` |
|      - |   62 | `	/* Initialize fields */` |
|  81185 |   63 | `	SySetInit(&pAttr->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|  81185 |   64 | `	SySetInit(&pAttr->aUnionAlts,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|  81185 |   65 | `	SyStringInitFromBuf(&pAttr->sName,zName,pName->nByte);` |
|  81185 |   66 | `	pAttr->iProtection = iProtection;` |
|  81185 |   67 | `	pAttr->nIdx = SXU32_HIGH;` |
|  81185 |   68 | `	pAttr->iFlags = iFlags;` |
|  81185 |   69 | `	pAttr->nLine = nLine;` |
|  81185 |   70 | `	return pAttr;` |
|  40595 |   71 | `}` |
|      - |   72 | `/*` |
|      - |   73 | ` * Allocate and initialize a new class method.` |
|      - |   74 | ` * Return a pointer to the class method on success. NULL otherwise` |
|      - |   75 | ` * This function associate with the newly created method an automatically generated` |
|      - |   76 | ` * random unique name.` |
|      - |   77 | ` */` |
| 288124 |   78 | `PH7_PRIVATE ph7_class_method * PH7_NewClassMethod(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,sxu32 nLine,` |
|      - |   79 | `	sxi32 iProtection,sxi32 iFlags,sxi32 iFuncFlags)` |
|      5 |   80 | `{` |
|      - |   81 | `	ph7_class_method *pMeth;` |
|      - |   82 | `	SyHashEntry *pEntry;` |
|      - |   83 | `	SyString *pNamePtr;` |
|      - |   84 | `	char zSalt[10];` |
|      - |   85 | `	char *zName;` |
|      - |   86 | `	sxu32 nByte;` |
|      - |   87 | `	/* Allocate a new class method instance */` |
| 288129 |   88 | `	pMeth = (ph7_class_method *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_method));` |
| 288129 |   89 | `	if( pMeth == 0 ){` |
|    ! 0 |   90 | `		return 0;` |
|      - |   91 | `	}` |
|      - |   92 | `	/* Zero the structure */` |
| 288129 |   93 | `	SyZero(pMeth,sizeof(ph7_class_method));` |
|      - |   94 | `	/* Check for an already installed method with the same name */` |
| 288129 |   95 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
| 288129 |   96 | `	if( pEntry == 0 ){` |
|      - |   97 | `		/* Associate an unique VM name to this method */` |
| 288127 |   98 | `		nByte = sizeof(zSalt) + pName->nByte + SyStringLength(&pClass->sName)+sizeof(char)*7/*[[__'\0'*/;` |
| 288127 |   99 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,nByte);` |
| 288127 |  100 | `		if( zName == 0 ){` |
|    ! 0 |  101 | `			SyMemBackendPoolFree(&pVm->sAllocator,pMeth);` |
|    ! 0 |  102 | `			return 0;` |
|      - |  103 | `		}` |
| 288127 |  104 | `		pNamePtr = &pMeth->sVmName;` |
|      - |  105 | `		/* Generate a random string */` |
| 288127 |  106 | `		PH7_VmRandomString(&(*pVm),zSalt,sizeof(zSalt));` |
| 288127 |  107 | `		pNamePtr->nByte = SyBufferFormat(zName,nByte,"[__%z@%z_%.*s]",&pClass->sName,pName,sizeof(zSalt),zSalt);` |
| 288127 |  108 | `		pNamePtr->zString = zName;` |
| 144066 |  109 | `	}else{` |
|      - |  110 | `		/* Method is condidate for 'overloading' */` |
|      3 |  111 | `		ph7_class_method *pCurrent = (ph7_class_method *)pEntry->pUserData;` |
|      3 |  112 | `		pNamePtr = &pMeth->sVmName;` |
|      - |  113 | `		/* Use the same VM name */` |
|      3 |  114 | `		SyStringDupPtr(pNamePtr,&pCurrent->sVmName);` |
|      3 |  115 | `		zName = (char *)pNamePtr->zString;` |
|      - |  116 | `	}` |
| 288129 |  117 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|     22 |  118 | `		if( (pName->nByte == sizeof("__construct") - 1 && SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1 ) == 0)` |
|     21 |  119 | `			\|\| (pName->nByte == sizeof("__destruct") - 1 && SyMemcmp(pName->zString,"__destruct",sizeof("__destruct") - 1 ) == 0)` |
|     22 |  120 | `			\|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|      - |  121 | `				/* Switch to public visibility when dealing with constructor/destructor */` |
|      5 |  122 | `				iProtection = PH7_CLASS_PROT_PUBLIC;` |
|      2 |  123 | `		}` |
|     12 |  124 | `	}` |
|      - |  125 | `	/* Initialize method fields */` |
| 288131 |  126 | `	pMeth->iProtection = iProtection;` |
| 288131 |  127 | `	pMeth->iFlags = iFlags;` |
| 288131 |  128 | `	pMeth->nLine = nLine;` |
| 432195 |  129 | `	PH7_VmInitFuncState(&(*pVm),&pMeth->sFunc,&zName[sizeof(char)*4/*[__@*/+SyStringLength(&pClass->sName)],` |
| 288126 |  130 | `		pName->nByte,iFuncFlags\|VM_FUNC_CLASS_METHOD,pClass);` |
| 288131 |  131 | `	return pMeth;` |
| 144069 |  132 | `}` |
|      - |  133 | `/*` |
|      - |  134 | ` * Check if the given name have a class method associated with it.` |
|      - |  135 | ` * Return the desired method [i.e: ph7_class_method instance] on success. NULL otherwise.` |
|      - |  136 | ` */` |
| 192206 |  137 | `PH7_PRIVATE ph7_class_method * PH7_ClassExtractMethod(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|      5 |  138 | `{` |
|      - |  139 | `	SyHashEntry *pEntry;` |
|      - |  140 | `	/* Perform a hash lookup */` |
| 192211 |  141 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)zName,nByte);` |
| 192211 |  142 | `	if( pEntry == 0 ){` |
|      - |  143 | `		/* No such entry */` |
|   4985 |  144 | `		return 0;` |
|      - |  145 | `	}` |
|      - |  146 | `	/* Point to the desired method */` |
| 187231 |  147 | `	return (ph7_class_method *)pEntry->pUserData;` |
|  96108 |  148 | `}` |
|      - |  149 | `/*` |
|      - |  150 | ` * Check if the given name is a class attribute.` |
|      - |  151 | ` * Return the desired attribute [i.e: ph7_class_attr instance] on success.NULL otherwise.` |
|      - |  152 | ` */` |
|  81314 |  153 | `PH7_PRIVATE ph7_class_attr * PH7_ClassExtractAttribute(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|      5 |  154 | `{` |
|      - |  155 | `	SyHashEntry *pEntry;` |
|      - |  156 | `	/* Perform a hash lookup */` |
|  81319 |  157 | `	pEntry = SyHashGet(&pClass->hAttr,(const void *)zName,nByte);` |
|  81319 |  158 | `	if( pEntry == 0 ){` |
|      - |  159 | `		/* No such entry */` |
|  81149 |  160 | `		return 0;` |
|      - |  161 | `	}` |
|      - |  162 | `	/* Point to the desierd method */` |
|    175 |  163 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|  40662 |  164 | `}` |
|      - |  165 | `/*` |
|      - |  166 | ` * Install a class attribute in the corresponding container.` |
|      - |  167 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|      - |  168 | ` */` |
|  81180 |  169 | `PH7_PRIVATE sxi32 PH7_ClassInstallAttr(ph7_class *pClass,ph7_class_attr *pAttr)` |
|      5 |  170 | `{` |
|  81185 |  171 | `	SyString *pName = &pAttr->sName;` |
|      - |  172 | `	sxi32 rc;` |
|      - |  173 | `	/* Remember where this attribute was originally declared so that later` |
|      - |  174 | `	 * inheritance/trait copies still know the declaring class (needed for` |
|      - |  175 | `	 * PHP-compatible error messages on typed properties). */` |
|  81185 |  176 | `	if( pAttr->pDeclClass == 0 ){` |
|  81185 |  177 | `		pAttr->pDeclClass = pClass;` |
|  40590 |  178 | `	}` |
|  81185 |  179 | `	rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|  81185 |  180 | `	return rc;` |
|      5 |  181 | `}` |
|      - |  182 | `/*` |
|      - |  183 | ` * Install a class method in the corresponding container.` |
|      - |  184 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|      - |  185 | ` */` |
| 288112 |  186 | `PH7_PRIVATE sxi32 PH7_ClassInstallMethod(ph7_class *pClass,ph7_class_method *pMeth)` |
|      5 |  187 | `{` |
| 288117 |  188 | `	SyString *pName = &pMeth->sFunc.sName;` |
|      - |  189 | `	sxi32 rc;` |
| 288117 |  190 | `	rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 288117 |  191 | `	return rc;` |
|      5 |  192 | `}` |
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
|    132 |  216 | `static int OoClassifyOverrideType(ph7_vm *pVm, sxu32 nType, const SyString *pClass,` |
|      - |  217 | `	int bUnion, ph7_class **ppClass)` |
|      5 |  218 | `{` |
|    137 |  219 | `	*ppClass = 0;` |
|    137 |  220 | `	if( bUnion ){` |
|      3 |  221 | `		return OVT_SKIP; /* union/intersection — full lattice, skip */` |
|      - |  222 | `	}` |
|    135 |  223 | `	if( nType == 0 ){` |
|     59 |  224 | `		return OVT_NONE; /* no declared type */` |
|      - |  225 | `	}` |
|     78 |  226 | `	if( nType == SXU32_HIGH ){` |
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
|     58 |  249 | `	if( nType == MEMOBJ_STRING \|\| nType == MEMOBJ_INT \|\| nType == MEMOBJ_REAL` |
|     28 |  250 | `	 \|\| nType == MEMOBJ_BOOL \|\| nType == MEMOBJ_HASHMAP ){` |
|     37 |  251 | `		return OVT_SCALAR;` |
|      - |  252 | `	}` |
|      - |  253 | `	/* MEMOBJ_OBJ (object — subtypes against classes), MEMOBJ_VOID/NULL/RES,` |
|      - |  254 | `	 * or anything unexpected: skip. */` |
|     26 |  255 | `	return OVT_SKIP;` |
|     71 |  256 | `}` |
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
|    100 |  270 | `static OvType OoTypeFromReturn(ph7_vm_func *pF)` |
|      5 |  271 | `{` |
|      - |  272 | `	OvType t;` |
|    105 |  273 | `	t.nType = pF->nReturnType;` |
|    105 |  274 | `	t.pClass = &pF->sReturnClass;` |
|    105 |  275 | `	t.bUnion = SySetUsed(&pF->aReturnUnion) > 0;` |
|    105 |  276 | `	t.bNullable = (pF->iFlags & VM_FUNC_RETURN_NULLABLE) != 0;` |
|    105 |  277 | `	return t;` |
|      5 |  278 | `}` |
|     32 |  279 | `static OvType OoTypeFromArg(ph7_vm_func_arg *pA)` |
|      3 |  280 | `{` |
|      - |  281 | `	OvType t;` |
|     35 |  282 | `	t.nType = pA->nType;` |
|     35 |  283 | `	t.pClass = &pA->sClass;` |
|     35 |  284 | `	t.bUnion = (pA->iFlags & VM_FUNC_ARG_UNION) != 0;` |
|     35 |  285 | `	t.bNullable = (pA->iFlags & VM_FUNC_ARG_NULLABLE) != 0;` |
|     35 |  286 | `	return t;` |
|      3 |  287 | `}` |
|      - |  288 | `/*` |
|      - |  289 | ` * Return TRUE if the child type is an unambiguously-invalid override of the` |
|      - |  290 | ` * parent type. bCovariant=1 for a return type (child must be ⊆ parent),` |
|      - |  291 | ` * 0 for a parameter (child must be ⊇ parent). Returns FALSE (accept) on any` |
|      - |  292 | ` * skipped/ambiguous shape.` |
|      - |  293 | ` */` |
|     66 |  294 | `static int OoOverrideTypeBad(ph7_vm *pVm, OvType parent, OvType child, int bCovariant)` |
|      5 |  295 | `{` |
|      - |  296 | `	ph7_class *pParentCls, *pChildCls;` |
|     71 |  297 | `	int kP = OoClassifyOverrideType(pVm, parent.nType, parent.pClass, parent.bUnion, &pParentCls);` |
|     71 |  298 | `	int kC = OoClassifyOverrideType(pVm, child.nType, child.pClass, child.bUnion, &pChildCls);` |
|     71 |  299 | `	if( kP == OVT_SKIP \|\| kC == OVT_SKIP ){` |
|     18 |  300 | `		return 0; /* ambiguous shape — conservatively accept */` |
|      - |  301 | `	}` |
|      - |  302 | `	/* A missing type is the TOP type. covariant (return): a concrete child is a` |
|      - |  303 | `	 * subtype of top, fine; a top child over a concrete parent WIDENS → bad.` |
|      - |  304 | `	 * contravariant (param): a top child is a supertype of anything, fine; a` |
|      - |  305 | `	 * concrete child over a top parent NARROWS → bad. (A union/intersection child` |
|      - |  306 | `	 * already fell into OVT_SKIP above, so a flagged child here is scalar/class.) */` |
|     55 |  307 | `	if( kP == OVT_NONE \|\| kC == OVT_NONE ){` |
|     33 |  308 | `		if( bCovariant && kC == OVT_NONE && kP != OVT_NONE ) return 1;` |
|     33 |  309 | `		if( !bCovariant && kP == OVT_NONE && kC != OVT_NONE ) return 1;` |
|     33 |  310 | `		return 0;` |
|      - |  311 | `	}` |
|      - |  312 | `	/* Nullability: a covariant return may not ADD null; a contravariant param may` |
|      - |  313 | `	 * not REMOVE null. */` |
|     24 |  314 | `	if( bCovariant ){` |
|     11 |  315 | `		if( child.bNullable && !parent.bNullable ) return 1;` |
|      7 |  316 | `	}else{` |
|     14 |  317 | `		if( parent.bNullable && !child.bNullable ) return 1;` |
|      - |  318 | `	}` |
|     24 |  319 | `	if( kP == OVT_SCALAR && kC == OVT_SCALAR ){` |
|      - |  320 | `		/* Scalars are invariant — they must match exactly. */` |
|     17 |  321 | `		return (parent.nType != child.nType) ? 1 : 0;` |
|      - |  322 | `	}` |
|      8 |  323 | `	if( kP == OVT_CLASS && kC == OVT_CLASS ){` |
|      8 |  324 | `		if( bCovariant ){` |
|      3 |  325 | `			return PH7_VmInstanceOf(pChildCls, pParentCls) ? 0 : 1;  /* child ⊆ parent */` |
|      - |  326 | `		}` |
|      6 |  327 | `		return PH7_VmInstanceOf(pParentCls, pChildCls) ? 0 : 1;      /* child ⊇ parent */` |
|      - |  328 | `	}` |
|      - |  329 | `	/* One scalar and one class — disjoint. */` |
|    ! 0 |  330 | `	return 1;` |
|     38 |  331 | `}` |
|      - |  332 |  |
|      - |  333 | `/*` |
|      - |  334 | ` * Check a child method's signature against the parent method it overrides.` |
|      - |  335 | ` * Emits a PHP-style "Declaration of … must be compatible …" fatal on a clear` |
|      - |  336 | `` * incompatibility. `__construct` is exempt (PHP does not apply variance to it).`` |
|      - |  337 | ` */` |
|   3882 |  338 | `static sxi32 OoCheckOverrideCompat(ph7_gen_state *pGen, ph7_class *pBase, ph7_class *pSub,` |
|      - |  339 | `	ph7_class_method *pParent, ph7_class_method *pChild)` |
|      5 |  340 | `{` |
|   3887 |  341 | `	ph7_vm *pVm = pGen->pVm;` |
|   3887 |  342 | `	ph7_vm_func *pPF = &pParent->sFunc;` |
|   3887 |  343 | `	ph7_vm_func *pCF = &pChild->sFunc;` |
|   3887 |  344 | `	SyString *pMName = &pCF->sName;` |
|      - |  345 | `	ph7_vm_func_arg *aP, *aC;` |
|      - |  346 | `	sxu32 nPArg, nCArg, k;` |
|   3887 |  347 | `	int bBad = 0;` |
|   3882 |  348 | `	if( pMName->nByte == sizeof("__construct")-1` |
|   3866 |  349 | `	 && SyStrnmicmp(pMName->zString,"__construct",pMName->nByte) == 0 ){` |
|   3837 |  350 | `		return SXRET_OK;` |
|      - |  351 | `	}` |
|      - |  352 | `	/* Return type — covariant. */` |
|     55 |  353 | `	bBad = OoOverrideTypeBad(pVm, OoTypeFromReturn(pPF), OoTypeFromReturn(pCF), /* bCovariant */ 1);` |
|      - |  354 | `	/* Each overlapping parameter — contravariant. */` |
|     55 |  355 | `	nPArg = SySetUsed(&pPF->aArgs);` |
|     55 |  356 | `	nCArg = SySetUsed(&pCF->aArgs);` |
|     55 |  357 | `	aP = (ph7_vm_func_arg *)SySetBasePtr(&pPF->aArgs);` |
|     55 |  358 | `	aC = (ph7_vm_func_arg *)SySetBasePtr(&pCF->aArgs);` |
|     71 |  359 | `	for( k = 0; !bBad && k < nPArg && k < nCArg; k++ ){` |
|     19 |  360 | `		bBad = OoOverrideTypeBad(pVm, OoTypeFromArg(&aP[k]), OoTypeFromArg(&aC[k]), /* bCovariant */ 0);` |
|     11 |  361 | `	}` |
|      - |  362 | `	/* Parameter arity: the child must declare at least the parent's parameters and` |
|      - |  363 | `	 * may add only OPTIONAL ones — PHP rejects dropping any param (even an optional` |
|      - |  364 | `	 * one) or adding a required one. Skip the rule if either signature is variadic` |
|      - |  365 | `	 * (arity semantics differ). */` |
|     55 |  366 | `	if( !bBad ){` |
|     50 |  367 | `		int bVariadic = 0;` |
|     64 |  368 | `		for( k = 0; k < nPArg; k++ ){ if( aP[k].iFlags & VM_FUNC_ARG_VARIADIC ) bVariadic = 1; }` |
|     66 |  369 | `		for( k = 0; k < nCArg; k++ ){ if( aC[k].iFlags & VM_FUNC_ARG_VARIADIC ) bVariadic = 1; }` |
|     50 |  370 | `		if( !bVariadic ){` |
|     50 |  371 | `			if( nCArg < nPArg ){` |
|    ! 0 |  372 | `				bBad = 1; /* dropped a parent parameter */` |
|    ! 0 |  373 | `			}else{` |
|     52 |  374 | `				for( k = nPArg; k < nCArg; k++ ){` |
|      3 |  375 | `					if( SySetUsed(&aC[k].aByteCode) == 0 ){ bBad = 1; break; } /* new required */` |
|      2 |  376 | `				}` |
|      - |  377 | `			}` |
|     23 |  378 | `		}` |
|     23 |  379 | `	}` |
|     55 |  380 | `	if( bBad ){` |
|      8 |  381 | `		sxi32 rc = PH7_GenCompileError(&(*pGen),E_ERROR,pChild->nLine,` |
|      - |  382 | `			"Declaration of %z::%z() must be compatible with %z::%z()",` |
|      2 |  383 | `			&pSub->sName,pMName,&pBase->sName,&pParent->sFunc.sName);` |
|      6 |  384 | `		if( rc == SXERR_ABORT ){` |
|    ! 0 |  385 | `			return SXERR_ABORT;` |
|      - |  386 | `		}` |
|      2 |  387 | `	}` |
|     55 |  388 | `	return SXRET_OK;` |
|   1946 |  389 | `}` |
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
|  84360 |  431 | `PH7_PRIVATE sxi32 PH7_ClassInherit(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase)` |
|      5 |  432 | `{` |
|      - |  433 | `	ph7_class_method *pMeth;` |
|      - |  434 | `	ph7_class_attr *pAttr;` |
|      - |  435 | `	SyHashEntry *pEntry;` |
|      - |  436 | `	SyString *pName;` |
|      - |  437 | `	sxi32 rc;` |
|      - |  438 | `	/* Install in the derived hashtable */` |
|  84365 |  439 | `	rc = SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|  84365 |  440 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  441 | `		return rc;` |
|      - |  442 | `	}` |
|      - |  443 | `	/* readonly class inheritance (PHP 8.2): a readonly class may only extend a` |
|      - |  444 | `	 * readonly class, and a non-readonly class may not extend a readonly one. */` |
|  84365 |  445 | `	if( (pBase->iFlags & PH7_CLASS_READONLY) != (pSub->iFlags & PH7_CLASS_READONLY) ){` |
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
|  84365 |  460 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
| 589821 |  461 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|      - |  462 | `		/* Make sure the private attributes are not redeclared in the subclass */` |
| 505461 |  463 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
| 505461 |  464 | `		pName = &pAttr->sName;` |
| 505461 |  465 | `		if( (pEntry = SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|      6 |  466 | `			if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_FINAL))` |
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
| 505455 |  490 | `		if( pAttr->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
| 505451 |  491 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
| 505451 |  492 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  493 | `				return rc;` |
|      - |  494 | `			}` |
| 252723 |  495 | `		}` |
|      5 |  496 | `	}` |
|  84365 |  497 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
| 842647 |  498 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|      - |  499 | `		/* Make sure the private/final methods are not redeclared in the subclass */` |
| 758287 |  500 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
| 758287 |  501 | `		pName = &pMeth->sFunc.sName;` |
| 758287 |  502 | `		if( (pEntry = SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|   3891 |  503 | `			 if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|      - |  504 | `				/* Cannot Overwrite final method */` |
|      7 |  505 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_method *)pEntry->pUserData)->nLine,` |
|      - |  506 | `					"Cannot Overwrite final method '%z:%z' inside child class '%z'",` |
|      2 |  507 | `					&pBase->sName,pName,&pSub->sName);` |
|      5 |  508 | `				if( rc == SXERR_ABORT ){` |
|    ! 0 |  509 | `					return SXERR_ABORT;` |
|      - |  510 | `				}` |
|      3 |  511 | `			}else{` |
|      - |  512 | `				/* Check the override's signature is compatible with the parent's. */` |
|   5828 |  513 | `				rc = OoCheckOverrideCompat(&(*pGen),pBase,pSub,pMeth,` |
|   3882 |  514 | `					(ph7_class_method *)pEntry->pUserData);` |
|   3887 |  515 | `				if( rc == SXERR_ABORT ){` |
|    ! 0 |  516 | `					return SXERR_ABORT;` |
|      - |  517 | `				}` |
|      - |  518 | `			}` |
|   3891 |  519 | `			continue;` |
|      - |  520 | `		}` |
|      - |  521 | `		/* Install the method */` |
| 754401 |  522 | `		if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
| 754399 |  523 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 754399 |  524 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  525 | `				return rc;` |
|      - |  526 | `			}` |
| 377197 |  527 | `		}` |
|      5 |  528 | `	}` |
|      - |  529 | `	/* Mark as subclass */` |
|  84365 |  530 | `	pSub->pBase = pBase;` |
|      - |  531 | `	/* All done */` |
|  84365 |  532 | `	return SXRET_OK;` |
|  42185 |  533 | `}` |
|      - |  534 | `/*` |
|      - |  535 | ` * Apply a trait to a class: copy all methods and attributes from the trait` |
|      - |  536 | ` * into the target class. Unlike inheritance, traits copy ALL members including` |
|      - |  537 | ` * private ones. Members already defined in the class take precedence.` |
|      - |  538 | ` */` |
|     50 |  539 | `PH7_PRIVATE sxi32 PH7_ClassUseTrait(ph7_gen_state *pGen,ph7_class *pClass,ph7_class *pTrait)` |
|      5 |  540 | `{` |
|      - |  541 | `	ph7_class_method *pMeth;` |
|      - |  542 | `	ph7_class_attr *pAttr;` |
|      - |  543 | `	SyHashEntry *pEntry;` |
|      - |  544 | `	SyString *pName;` |
|      - |  545 | `	sxi32 rc;` |
|      - |  546 | `	/* Detect cyclic trait composition (e.g. trait A { use B; } trait B { use A; }) */` |
|     55 |  547 | `	if( pTrait->iFlags & PH7_CLASS_TRAIT_VISITING ){` |
|    ! 0 |  548 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|    ! 0 |  549 | `			"Trait circular reference detected: %z is already being applied",&pTrait->sName);` |
|    ! 0 |  550 | `		if( rc == SXERR_ABORT ){` |
|    ! 0 |  551 | `			return SXERR_ABORT;` |
|      - |  552 | `		}` |
|    ! 0 |  553 | `		return SXRET_OK;` |
|      - |  554 | `	}` |
|     55 |  555 | `	pTrait->iFlags \|= PH7_CLASS_TRAIT_VISITING;` |
|     55 |  556 | `	rc = SXRET_OK;` |
|      - |  557 | `	/* Copy attributes from the trait */` |
|     55 |  558 | `	SyHashResetLoopCursor(&pTrait->hAttr);` |
|     71 |  559 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hAttr)) != 0 ){` |
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
|     55 |  601 | `	SyHashResetLoopCursor(&pTrait->hMethod);` |
|    101 |  602 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hMethod)) != 0 ){` |
|     51 |  603 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|     51 |  604 | `		pName = &pMeth->sFunc.sName;` |
|     51 |  605 | `		if( SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte) != 0 ){` |
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
|     43 |  631 | `		rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|     43 |  632 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  633 | `			goto cleanup;` |
|      - |  634 | `		}` |
|      5 |  635 | `	}` |
|      - |  636 | `	/* Record trait in the class */` |
|     55 |  637 | `	SySetPut(&pClass->aTrait,(const void *)&pTrait);` |
|     25 |  638 | `cleanup:` |
|      - |  639 | `	/* Always clear visiting flag, even on error paths */` |
|     55 |  640 | `	pTrait->iFlags &= ~PH7_CLASS_TRAIT_VISITING;` |
|     25 |  641 | `	SXUNUSED(pGen);` |
|     55 |  642 | `	return rc;` |
|     30 |  643 | `}` |
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
|  11486 |  657 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceInherit(ph7_class *pSub,ph7_class *pBase)` |
|      5 |  658 | `{` |
|      - |  659 | `	ph7_class_method *pMeth;` |
|      - |  660 | `	ph7_class_attr *pAttr;` |
|      - |  661 | `	SyHashEntry *pEntry;` |
|      - |  662 | `	SyString *pName;` |
|      - |  663 | `	sxi32 rc;` |
|      - |  664 | `	/* Install in the derived hashtable */` |
|  11491 |  665 | `	SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|  11491 |  666 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
|      - |  667 | `	/* Copy constants */` |
|  17236 |  668 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
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
|  11491 |  680 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
|      - |  681 | `	/* Copy methods signature */` |
|  21102 |  682 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|      - |  683 | `		/* Make sure the method are not redeclared in the subclass */` |
|   3873 |  684 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   3873 |  685 | `		pName = &pMeth->sFunc.sName;` |
|   3873 |  686 | `		if( SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte) == 0 ){` |
|      - |  687 | `			/* Install the method */` |
|   3873 |  688 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|   3873 |  689 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  690 | `				return rc;` |
|      - |  691 | `			}` |
|   1934 |  692 | `		}` |
|      5 |  693 | `	}` |
|      - |  694 | `	/* Mark as subclass */` |
|  11491 |  695 | `	pSub->pBase = pBase;` |
|      - |  696 | `	/* All done */` |
|  11491 |  697 | `	return SXRET_OK;` |
|   5748 |  698 | `}` |
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
| 103560 |  712 | `PH7_PRIVATE sxi32 PH7_ClassImplement(ph7_class *pMain,ph7_class *pInterface)` |
|      5 |  713 | `{` |
|      - |  714 | `	ph7_class_attr *pAttr;` |
|      - |  715 | `	SyHashEntry *pEntry;` |
|      - |  716 | `	SyString *pName;` |
|      - |  717 | `	sxi32 rc;` |
|      - |  718 | `	/* First off,copy all constants declared inside the interface */` |
| 103565 |  719 | `	SyHashResetLoopCursor(&pInterface->hAttr);` |
| 155351 |  720 | `	while((pEntry = SyHashGetNextEntry(&pInterface->hAttr)) != 0 ){` |
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
| 103565 |  734 | `	SySetPut(&pMain->aInterface,(const void *)&pInterface);` |
|      - |  735 | `	/* Install interface method stubs into the implementing class.` |
|      - |  736 | `	 * Methods already defined in the class take precedence (they satisfy` |
|      - |  737 | `	 * the interface contract). Stubs retain PH7_CLASS_ATTR_ABSTRACT so` |
|      - |  738 | `	 * the unified check in GenStateCheckAbstractMethods catches missing ones.` |
|      - |  739 | `	 */` |
|      - |  740 | `	{` |
|      - |  741 | `		ph7_class_method *pMeth;` |
|      - |  742 | `		SyHashEntry *pMEntry;` |
|      - |  743 | `		SyString *pMName;` |
| 103565 |  744 | `		SyHashResetLoopCursor(&pInterface->hMethod);` |
| 327979 |  745 | `		while((pMEntry = SyHashGetNextEntry(&pInterface->hMethod)) != 0 ){` |
| 172639 |  746 | `			pMeth = (ph7_class_method *)pMEntry->pUserData;` |
| 172639 |  747 | `			pMName = &pMeth->sFunc.sName;` |
| 172639 |  748 | `			if( SyHashGet(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte) == 0 ){` |
|     18 |  749 | `				rc = SyHashInsert(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte,pMeth);` |
|     18 |  750 | `				if( rc != SXRET_OK ){` |
|    ! 0 |  751 | `					return rc;` |
|      - |  752 | `				}` |
|      7 |  753 | `			}` |
|      5 |  754 | `		}` |
|      - |  755 | `	}` |
| 103565 |  756 | `	return SXRET_OK;` |
|  51785 |  757 | `}` |
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
|   4054 |  837 | `static ph7_class_instance * NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|      5 |  838 | `{` |
|      - |  839 | `	ph7_class_instance *pThis;` |
|      - |  840 | `	/* Allocate a new instance */` |
|   4059 |  841 | `	pThis = (ph7_class_instance *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_instance));` |
|   4059 |  842 | `	if( pThis == 0 ){` |
|    ! 0 |  843 | `		return 0;` |
|      - |  844 | `	}` |
|      - |  845 | `	/* Zero the structure */` |
|   4059 |  846 | `	SyZero(pThis,sizeof(ph7_class_instance));` |
|      - |  847 | `	/* Initialize fields */` |
|   4059 |  848 | `	pThis->iRef = 1;` |
|   4059 |  849 | `	pThis->pVm = pVm;` |
|   4059 |  850 | `	pThis->pClass = pClass;` |
|      - |  851 | `	/* Assign a fresh monotonic object handle id (clones get their own, like PHP). */` |
|   4059 |  852 | `	pThis->nObjId = pVm->nNextObjId++;` |
|   4059 |  853 | `	SyHashInit(&pThis->hAttr,&pVm->sAllocator,0,0);` |
|   4059 |  854 | `	return pThis;` |
|   2032 |  855 | `}` |
|      - |  856 | `/*` |
|      - |  857 | ` * Wrapper around the NewClassInstance() function defined above.` |
|      - |  858 | ` * See the block comment above for more information.` |
|      - |  859 | ` */` |
|   3970 |  860 | `PH7_PRIVATE ph7_class_instance * PH7_NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|      5 |  861 | `{` |
|      - |  862 | `	ph7_class_instance *pNew;` |
|      - |  863 | `	sxi32 rc;` |
|   3975 |  864 | `	pNew = NewClassInstance(&(*pVm),&(*pClass));` |
|   3975 |  865 | `	if( pNew == 0 ){` |
|    ! 0 |  866 | `		return 0;` |
|      - |  867 | `	}` |
|      - |  868 | `	/* Associate a private VM frame with this class instance */` |
|   3975 |  869 | `	rc = PH7_VmCreateClassInstanceFrame(&(*pVm),pNew);` |
|   3975 |  870 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  871 | `		SyMemBackendPoolFree(&pVm->sAllocator,pNew);` |
|    ! 0 |  872 | `		return 0;` |
|      - |  873 | `	}` |
|   3975 |  874 | `	return pNew;` |
|   1990 |  875 | `}` |
|      - |  876 | `/*` |
|      - |  877 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon] attribute.` |
|      - |  878 | ` * This function never fail.` |
|      - |  879 | ` */` |
|   6672 |  880 | `static ph7_value * ExtractClassAttrValue(ph7_vm *pVm,VmClassAttr *pAttr)` |
|      5 |  881 | `{` |
|      - |  882 | `	/* Extract the value */` |
|      - |  883 | `	ph7_value *pValue;` |
|   6677 |  884 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|   6677 |  885 | `	return pValue;` |
|      5 |  886 | `}` |
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
|     84 |  971 | `PH7_PRIVATE ph7_class_instance * PH7_CloneClassInstance(ph7_class_instance *pSrc)` |
|      3 |  972 | `{` |
|      - |  973 | `	ph7_class_instance *pClone;` |
|      - |  974 | `	ph7_class_method *pMethod;` |
|      - |  975 | `	SyHashEntry *pEntry2;` |
|      - |  976 | `	SyHashEntry *pEntry;` |
|      - |  977 | `	ph7_vm *pVm;` |
|      - |  978 | `	sxi32 rc;` |
|      - |  979 | `	/* Allocate a new instance */` |
|     87 |  980 | `	pVm = pSrc->pVm;` |
|     87 |  981 | `	pClone = NewClassInstance(pVm,pSrc->pClass);` |
|     87 |  982 | `	if( pClone == 0 ){` |
|    ! 0 |  983 | `		return 0;` |
|      - |  984 | `	}` |
|      - |  985 | `	/* Associate a private VM frame with this class instance */` |
|     87 |  986 | `	rc = PH7_VmCreateClassInstanceFrame(pVm,pClone);` |
|     87 |  987 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  988 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClone);` |
|    ! 0 |  989 | `		return 0;` |
|      - |  990 | `	}` |
|      - |  991 | `	/* Duplicate object values. Iterate the SOURCE attributes and copy each into` |
|      - |  992 | `	 * the clone's same-named slot (looked up by name, so order/count differences` |
|      - |  993 | `	 * from dynamic properties don't matter). A dynamic (runtime-added) property` |
|      - |  994 | `	 * has no declared counterpart in the clone, so synthesize it first — without` |
|      - |  995 | `	 * this, a clone of a stdClass would silently lose all its dynamic properties. */` |
|     87 |  996 | `	SyHashResetLoopCursor(&pSrc->hAttr);` |
|    231 |  997 | `	while((pEntry = SyHashGetNextEntry(&pSrc->hAttr)) != 0 ){` |
|    147 |  998 | `		VmClassAttr *pSrcAttr = (VmClassAttr *)pEntry->pUserData;` |
|    147 |  999 | `		VmClassAttr *pDestAttr = 0;` |
|    147 | 1000 | `		ph7_value *pvSrc,*pvDest = 0;` |
|      - | 1001 | `		/* Duplicate non-static attribute */` |
|    147 | 1002 | `		if( pSrcAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|    ! 0 | 1003 | `			continue;` |
|      - | 1004 | `		}` |
|    147 | 1005 | `		pEntry2 = SyHashGet(&pClone->hAttr,SyStringData(&pSrcAttr->pAttr->sName),SyStringLength(&pSrcAttr->pAttr->sName));` |
|    147 | 1006 | `		if( pEntry2 ){` |
|    133 | 1007 | `			pDestAttr = (VmClassAttr *)pEntry2->pUserData;` |
|    133 | 1008 | `			pvDest = ExtractClassAttrValue(pVm,pDestAttr);` |
|     80 | 1009 | `		}else if( pSrcAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){` |
|      - | 1010 | `			/* Dynamic property: synthesize the matching slot on the clone. */` |
|     22 | 1011 | `			pvDest = PH7_VmCreateDynamicAttr(pVm,pClone,` |
|     14 | 1012 | `				SyStringData(&pSrcAttr->pAttr->sName),SyStringLength(&pSrcAttr->pAttr->sName),&pDestAttr);` |
|      7 | 1013 | `		}` |
|      - | 1014 | `		/* Fetch the source value LAST: PH7_VmCreateDynamicAttr above may have` |
|      - | 1015 | `		 * reserved a slot and reallocated pVm->aMemObj, which would dangle any` |
|      - | 1016 | `		 * ph7_value* obtained before it. pvDest from the synth path already points` |
|      - | 1017 | `		 * into the post-realloc aMemObj; resolve pvSrc now so both are current. */` |
|    147 | 1018 | `		pvSrc = ExtractClassAttrValue(pVm,pSrcAttr);` |
|    147 | 1019 | `		if( pvSrc && pvDest ){` |
|    147 | 1020 | `			PH7_MemObjStore(pvSrc,pvDest);` |
|     72 | 1021 | `		}` |
|      - | 1022 | `		/* Carry over the per-instance state so the clone matches the source:` |
|      - | 1023 | `		 * VM_CLASS_ATTR_UNINIT marks a typed property as not-yet-initialized` |
|      - | 1024 | `		 * and doubles as the readonly write-once latch — without this a clone` |
|      - | 1025 | `		 * would reset to uninitialized (losing the value's readiness) and a` |
|      - | 1026 | `		 * readonly property would become writable again. */` |
|    147 | 1027 | `		if( pDestAttr ){` |
|    147 | 1028 | `			pDestAttr->iState = pSrcAttr->iState;` |
|     72 | 1029 | `		}` |
|      3 | 1030 | `	}` |
|      - | 1031 | `	/* A declared property unset() on the source is absent from the clone too (PHP). But the clone` |
|      - | 1032 | `	 * frame above materialized ALL declared attrs (with their defaults), so drop any clone attr whose` |
|      - | 1033 | `	 * name is not present on the source. Collect first, then delete — removing an entry mid-walk would` |
|      - | 1034 | `	 * free the node the SyHash loop cursor points at. */` |
|      - | 1035 | `	{` |
|      - | 1036 | `		SySet sDrop;` |
|     87 | 1037 | `		SySetInit(&sDrop,&pVm->sAllocator,sizeof(VmClassAttr *));` |
|     87 | 1038 | `		SyHashResetLoopCursor(&pClone->hAttr);` |
|    233 | 1039 | `		while((pEntry = SyHashGetNextEntry(&pClone->hAttr)) != 0 ){` |
|    149 | 1040 | `			VmClassAttr *pCloneAttr = (VmClassAttr *)pEntry->pUserData;` |
|    149 | 1041 | `			if( pCloneAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|    ! 0 | 1042 | `				continue;` |
|      - | 1043 | `			}` |
|    219 | 1044 | `			if( SyHashGet(&pSrc->hAttr,SyStringData(&pCloneAttr->pAttr->sName),` |
|    222 | 1045 | `					SyStringLength(&pCloneAttr->pAttr->sName)) == 0 ){` |
|      3 | 1046 | `				SySetPut(&sDrop,(const void *)&pCloneAttr);` |
|      1 | 1047 | `			}` |
|      3 | 1048 | `		}` |
|     87 | 1049 | `		if( SySetUsed(&sDrop) > 0 ){` |
|      3 | 1050 | `			VmClassAttr **apDrop = (VmClassAttr **)SySetBasePtr(&sDrop);` |
|      - | 1051 | `			sxu32 i;` |
|      5 | 1052 | `			for( i = 0 ; i < SySetUsed(&sDrop) ; ++i ){` |
|      3 | 1053 | `				VmClassAttr *pVmAttr = apDrop[i];` |
|      4 | 1054 | `				SyHashDeleteEntry(&pClone->hAttr,SyStringData(&pVmAttr->pAttr->sName),` |
|      2 | 1055 | `					SyStringLength(&pVmAttr->pAttr->sName),0);` |
|      3 | 1056 | `				PH7_VmReleaseInstanceAttr(pVm,pVmAttr);` |
|      2 | 1057 | `			}` |
|      1 | 1058 | `		}` |
|     87 | 1059 | `		SySetRelease(&sDrop);` |
|      - | 1060 | `	}` |
|      - | 1061 | `	/* call the __clone method on the cloned object if available */` |
|     87 | 1062 | `	pMethod = PH7_ClassExtractMethod(pClone->pClass,"__clone",sizeof("__clone")-1);` |
|     87 | 1063 | `	if( pMethod ){` |
|     38 | 1064 | `		if( pMethod->iCloneDepth < 16 ){` |
|     36 | 1065 | `			pMethod->iCloneDepth++;` |
|     36 | 1066 | `			PH7_VmCallClassMethod(pVm,pClone,pMethod,0,0,0);` |
|     19 | 1067 | `		}else{` |
|      - | 1068 | `			/* Nesting limit reached */` |
|      3 | 1069 | `			PH7_VmThrowError(pVm,0,PH7_CTX_ERR,"Object clone limit reached,no more call to __clone()");` |
|      - | 1070 | `		}` |
|      - | 1071 | `		/* Reset the cursor */` |
|     38 | 1072 | `		pMethod->iCloneDepth = 0;` |
|     18 | 1073 | `	}` |
|      - | 1074 | `	/* Return the cloned object */` |
|     87 | 1075 | `	return pClone;` |
|     45 | 1076 | `}` |
|      - | 1077 | `#define CLASS_INSTANCE_DESTROYED 0x001 /* Instance is released */` |
|      - | 1078 | `/*` |
|      - | 1079 | ` * Free the per-instance allocations owned by ONE object attribute: its value slot (+ the typed-slot` |
|      - | 1080 | ` * enforcement entry), the synthesized ph7_class_attr for a dynamic (runtime-added) property, and the` |
|      - | 1081 | ` * VmClassAttr wrapper itself. Does NOT touch the hAttr entry node — the caller removes it` |
|      - | 1082 | `` * (`unset($o->p)` via SyHashDeleteEntry2; instance teardown via the wholesale SyHashRelease, so it must`` |
|      - | 1083 | ` * not delete entries mid-walk). Shared by PH7_ClassInstanceRelease and the OP_MEMBER unset path.` |
|      - | 1084 | ` */` |
|   8850 | 1085 | `PH7_PRIVATE void PH7_VmReleaseInstanceAttr(ph7_vm *pVm, VmClassAttr *pVmAttr)` |
|      5 | 1086 | `{` |
|   8855 | 1087 | `	if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|      - | 1088 | `		/* Drop any typed-property enforcement slot registered for this memobj, before the memobj` |
|      - | 1089 | `		 * is returned to the free list, so a future recycled slot does not inherit the stale entry. */` |
|   8827 | 1090 | `		if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|    205 | 1091 | `			SyHashDeleteEntry(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),0);` |
|    100 | 1092 | `		}` |
|   8827 | 1093 | `		PH7_VmUnsetMemObj(pVm,pVmAttr->nIdx,TRUE);` |
|   4411 | 1094 | `	}` |
|      - | 1095 | `	/* A dynamic property owns its synthesized ph7_class_attr (struct + inline name in one block) —` |
|      - | 1096 | `	 * free it here (the only place a per-instance pAttr is freed; declared attrs are class-owned). */` |
|   8855 | 1097 | `	if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){` |
|    107 | 1098 | `		SyMemBackendFree(&pVm->sAllocator,pVmAttr->pAttr);` |
|     53 | 1099 | `	}` |
|   8855 | 1100 | `	SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|   8855 | 1101 | `}` |
|      - | 1102 | `/*` |
|      - | 1103 | ` * Release a class instance [i.e: Object in the PHP jargon] and invoke any defined destructor.` |
|      - | 1104 | ` * This routine is invoked as soon as there are no other references to a particular` |
|      - | 1105 | ` * class instance.` |
|      - | 1106 | ` */` |
|   2430 | 1107 | `static void PH7_ClassInstanceRelease(ph7_class_instance *pThis)` |
|      5 | 1108 | `{` |
|      - | 1109 | `	ph7_class_method *pDestr;` |
|      - | 1110 | `	SyHashEntry *pEntry;` |
|      - | 1111 | `	ph7_class *pClass;` |
|      - | 1112 | `	ph7_vm *pVm;` |
|   2435 | 1113 | `	if( pThis->iFlags & CLASS_INSTANCE_DESTROYED ){` |
|      - | 1114 | `		/*` |
|      - | 1115 | `		 * Already destroyed,return immediately.` |
|      - | 1116 | `		 * This could happend if someone perform unset($this) in the destructor body.` |
|      - | 1117 | `		 */` |
|    ! 0 | 1118 | `		return;` |
|      - | 1119 | `	}` |
|      - | 1120 | `	/* Mark as destroyed */` |
|   2435 | 1121 | `	pThis->iFlags \|= CLASS_INSTANCE_DESTROYED;` |
|      - | 1122 | `	/* Invoke any defined destructor if available */` |
|   2435 | 1123 | `	pVm = pThis->pVm;` |
|   2435 | 1124 | `	pClass = pThis->pClass;` |
|   2435 | 1125 | `	pDestr = PH7_ClassExtractMethod(pClass,"__destruct",sizeof("__destruct")-1);` |
|   2435 | 1126 | `	if( pDestr && !pVm->bInReset ){` |
|      - | 1127 | `		/* Invoke the destructor. Skipped during ph7_vm_reset() bulk teardown:` |
|      - | 1128 | `		 * running user PHP against a half-reset VM is unsafe (see bInReset). */` |
|    231 | 1129 | `		pThis->iRef = 2; /* Prevent garbage collection */` |
|    231 | 1130 | `		PH7_VmCallClassMethod(pVm,pThis,pDestr,0,0,0);` |
|    114 | 1131 | `	}` |
|      - | 1132 | `	/* Release non-static attributes (the wholesale SyHashRelease below frees the entry nodes,` |
|      - | 1133 | `	 * so the helper must not delete them mid-walk). */` |
|   2435 | 1134 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|  11255 | 1135 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|   8825 | 1136 | `		PH7_VmReleaseInstanceAttr(pVm,(VmClassAttr *)pEntry->pUserData);` |
|      5 | 1137 | `	}` |
|      - | 1138 | `	/* Release the whole structure */` |
|   2435 | 1139 | `	SyHashRelease(&pThis->hAttr);` |
|   2435 | 1140 | `	SyMemBackendPoolFree(&pVm->sAllocator,pThis);` |
|   1220 | 1141 | `}` |
|      - | 1142 | `/*` |
|      - | 1143 | ` * Decrement the reference count of a class instance [i.e Object in the PHP jargon].` |
|      - | 1144 | ` * If the reference count reaches zero,release the whole instance.` |
|      - | 1145 | ` */` |
|  49086 | 1146 | `PH7_PRIVATE void PH7_ClassInstanceUnref(ph7_class_instance *pThis)` |
|      5 | 1147 | `{` |
|  49091 | 1148 | `	pThis->iRef--;` |
|  49091 | 1149 | `	if( pThis->iRef < 1 ){` |
|      - | 1150 | `		/* No more reference to this instance */` |
|   2435 | 1151 | `		PH7_ClassInstanceRelease(&(*pThis));` |
|   1215 | 1152 | `	}` |
|  49091 | 1153 | `}` |
|      - | 1154 | `/*` |
|      - | 1155 | ` * Compare two class instances [i.e: Objects in the PHP jargon]` |
|      - | 1156 | ` * Note on objects comparison:` |
|      - | 1157 | ` *  According to the PHP langauge reference manual` |
|      - | 1158 | ` *  When using the comparison operator (==), object variables are compared in a simple manner` |
|      - | 1159 | ` *  namely: Two object instances are equal if they have the same attributes and values, and are` |
|      - | 1160 | ` *  instances of the same class.` |
|      - | 1161 | ` *  On the other hand, when using the identity operator (===), object variables are identical` |
|      - | 1162 | ` *  if and only if they refer to the same instance of the same class.` |
|      - | 1163 | ` *  An example will clarify these rules.` |
|      - | 1164 | ` *  Example #1 Example of object comparison` |
|      - | 1165 | ` *  <?php` |
|      - | 1166 | ` *    function bool2str($bool)` |
|      - | 1167 | ` * {` |
|      - | 1168 | ` *   if ($bool === false) {` |
|      - | 1169 | ` *       return 'FALSE';` |
|      - | 1170 | ` *   } else {` |
|      - | 1171 | ` *       return 'TRUE';` |
|      - | 1172 | ` *   }` |
|      - | 1173 | ` * }` |
|      - | 1174 | ` * function compareObjects(&$o1, &$o2)` |
|      - | 1175 | ` * {` |
|      - | 1176 | ` *   echo 'o1 == o2 : ' . bool2str($o1 == $o2) . "\n";` |
|      - | 1177 | ` *   echo 'o1 != o2 : ' . bool2str($o1 != $o2) . "\n";` |
|      - | 1178 | ` *   echo 'o1 === o2 : ' . bool2str($o1 === $o2) . "\n";` |
|      - | 1179 | ` *   echo 'o1 !== o2 : ' . bool2str($o1 !== $o2) . "\n";` |
|      - | 1180 | ` * }` |
|      - | 1181 | ` * class Flag` |
|      - | 1182 | ` * {` |
|      - | 1183 | ` *   public $flag;` |
|      - | 1184 | ` *` |
|      - | 1185 | ` *   function Flag($flag = true) {` |
|      - | 1186 | ` *       $this->flag = $flag;` |
|      - | 1187 | ` *   }` |
|      - | 1188 | ` * }` |
|      - | 1189 | ` *` |
|      - | 1190 | ` * class OtherFlag` |
|      - | 1191 | ` * {` |
|      - | 1192 | ` *   public $flag;` |
|      - | 1193 | ` *` |
|      - | 1194 | ` *   function OtherFlag($flag = true) {` |
|      - | 1195 | ` *       $this->flag = $flag;` |
|      - | 1196 | ` *   }` |
|      - | 1197 | ` * }` |
|      - | 1198 | ` *` |
|      - | 1199 | ` * $o = new Flag();` |
|      - | 1200 | ` * $p = new Flag();` |
|      - | 1201 | ` * $q = $o;` |
|      - | 1202 | ` * $r = new OtherFlag();` |
|      - | 1203 | ` *` |
|      - | 1204 | ` * echo "Two instances of the same class\n";` |
|      - | 1205 | ` * compareObjects($o, $p);` |
|      - | 1206 | ` * echo "\nTwo references to the same instance\n";` |
|      - | 1207 | ` * compareObjects($o, $q);` |
|      - | 1208 | ` * echo "\nInstances of two different classes\n";` |
|      - | 1209 | ` * compareObjects($o, $r);` |
|      - | 1210 | ` * ?>` |
|      - | 1211 | ` * The above example will output:` |
|      - | 1212 | ` * Two instances of the same class` |
|      - | 1213 | ` * o1 == o2 : TRUE` |
|      - | 1214 | ` * o1 != o2 : FALSE` |
|      - | 1215 | ` * o1 === o2 : FALSE` |
|      - | 1216 | ` * o1 !== o2 : TRUE` |
|      - | 1217 | ` * Two references to the same instance` |
|      - | 1218 | ` * o1 == o2 : TRUE` |
|      - | 1219 | ` * o1 != o2 : FALSE` |
|      - | 1220 | ` * o1 === o2 : TRUE` |
|      - | 1221 | ` * o1 !== o2 : FALSE` |
|      - | 1222 | ` * Instances of two different classes` |
|      - | 1223 | ` * o1 == o2 : FALSE` |
|      - | 1224 | ` * o1 != o2 : TRUE` |
|      - | 1225 | ` * o1 === o2 : FALSE` |
|      - | 1226 | ` * o1 !== o2 : TRUE` |
|      - | 1227 | ` *` |
|      - | 1228 | ` * This function return 0 if the objects are equals according to the comprison rules defined above.` |
|      - | 1229 | ` * Any other return values indicates difference.` |
|      - | 1230 | ` */` |
|    224 | 1231 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest)` |
|      5 | 1232 | `{` |
|      - | 1233 | `	SyHashEntry *pEntry,*pEntry2;` |
|      - | 1234 | `	ph7_value sV1,sV2;` |
|      - | 1235 | `	sxi32 rc;` |
|    229 | 1236 | `	if( iNest > 31 ){` |
|      - | 1237 | `		/* Nesting limit reached */` |
|      6 | 1238 | `		PH7_VmThrowError(pLeft->pVm,0,PH7_CTX_ERR,"Nesting limit reached: Infinite recursion?");` |
|      6 | 1239 | `		return 1;` |
|      - | 1240 | `	}` |
|      - | 1241 | `	/* Comparison is performed only if the objects are instance of the same class */` |
|    225 | 1242 | `	if( pLeft->pClass != pRight->pClass ){` |
|      7 | 1243 | `		return 1;` |
|      - | 1244 | `	}` |
|    219 | 1245 | `	if( bStrict ){` |
|      - | 1246 | `		/*` |
|      - | 1247 | `		 * According to the PHP language reference manual:` |
|      - | 1248 | `		 *  when using the identity operator (===), object variables` |
|      - | 1249 | `		 *  are identical if and only if they refer to the same instance` |
|      - | 1250 | `		 *  of the same class.` |
|      - | 1251 | `		 */` |
|     67 | 1252 | `		return !(pLeft == pRight);` |
|      - | 1253 | `	}` |
|      - | 1254 | `	/*` |
|      - | 1255 | `	 * Attribute comparison.` |
|      - | 1256 | `	 * According to the PHP reference manual:` |
|      - | 1257 | `	 *  When using the comparison operator (==), object variables are compared` |
|      - | 1258 | `	 *  in a simple manner, namely: Two object instances are equal if they have` |
|      - | 1259 | `	 *  the same attributes and values, and are instances of the same class.` |
|      - | 1260 | `	 */` |
|    155 | 1261 | `	if( pLeft == pRight ){` |
|      - | 1262 | `		/* Same instance,don't bother processing,object are equals */` |
|      5 | 1263 | `		return 0;` |
|      - | 1264 | `	}` |
|      - | 1265 | `	/* Closures compare by IDENTITY under == as well (not by attributes): two distinct` |
|      - | 1266 | `	 * Closure instances are never equal, even when they wrap the same underlying function` |
|      - | 1267 | `	 * (PHP semantics). pLeft != pRight here, so a Closure pair is unequal. Without this,` |
|      - | 1268 | `` 	 * two capture-less lambdas of the same `function(){}` share the template's `$__fn` `` |
|      - | 1269 | `	 * name and would compare equal. */` |
|    151 | 1270 | `	if( pLeft->pVm->pClosureClass && pLeft->pClass == pLeft->pVm->pClosureClass ){` |
|      5 | 1271 | `		return 1;` |
|      - | 1272 | `	}` |
|      - | 1273 | `	/* Same class but a different number of attributes ⇒ different property sets` |
|      - | 1274 | `	 * (dynamic properties can give two same-class instances different counts). */` |
|    147 | 1275 | `	if( pLeft->hAttr.nEntry != pRight->hAttr.nEntry ){` |
|      3 | 1276 | `		return 1;` |
|      - | 1277 | `	}` |
|    145 | 1278 | `	PH7_MemObjInit(pLeft->pVm,&sV1);` |
|    145 | 1279 | `	PH7_MemObjInit(pLeft->pVm,&sV2);` |
|    145 | 1280 | `	sV1.nIdx = sV2.nIdx = SXU32_HIGH;` |
|      - | 1281 | `	/* Compare each left attribute against the RIGHT attribute of the SAME NAME` |
|      - | 1282 | `	 * (not in lockstep): dynamic properties may be stored in a different order` |
|      - | 1283 | `	 * on the two instances. Counts already match, so if every left attribute has` |
|      - | 1284 | `	 * an equal-valued same-named right attribute the property sets are equal. */` |
|    145 | 1285 | `	SyHashResetLoopCursor(&pLeft->hAttr);` |
|    169 | 1286 | `	while((pEntry = SyHashGetNextEntry(&pLeft->hAttr)) != 0 ){` |
|    157 | 1287 | `		VmClassAttr *p1 = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1288 | `		VmClassAttr *p2;` |
|      - | 1289 | `		ph7_value *pL,*pR;` |
|      - | 1290 | `		/* Compare only non-static attribute */` |
|    157 | 1291 | `		if( p1->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|    ! 0 | 1292 | `			continue;` |
|      - | 1293 | `		}` |
|    157 | 1294 | `		pEntry2 = SyHashGet(&pRight->hAttr,SyStringData(&p1->pAttr->sName),SyStringLength(&p1->pAttr->sName));` |
|    157 | 1295 | `		if( pEntry2 == 0 ){` |
|      - | 1296 | `			/* Left has a property the right lacks ⇒ not equal. */` |
|    ! 0 | 1297 | `			return 1;` |
|      - | 1298 | `		}` |
|    157 | 1299 | `		p2 = (VmClassAttr *)pEntry2->pUserData;` |
|    157 | 1300 | `		pL = ExtractClassAttrValue(pLeft->pVm,p1);` |
|    157 | 1301 | `		pR = ExtractClassAttrValue(pRight->pVm,p2);` |
|    157 | 1302 | `		if( pL && pR ){` |
|    157 | 1303 | `			PH7_MemObjLoad(pL,&sV1);` |
|    157 | 1304 | `			PH7_MemObjLoad(pR,&sV2);` |
|      - | 1305 | `			/* Compare the two values now */` |
|    157 | 1306 | `			rc = PH7_MemObjCmp(&sV1,&sV2,bStrict,iNest+1);` |
|    157 | 1307 | `			PH7_MemObjRelease(&sV1);` |
|    157 | 1308 | `			PH7_MemObjRelease(&sV2);` |
|    157 | 1309 | `			if( rc != 0 ){` |
|      - | 1310 | `				/* Not equals */` |
|    133 | 1311 | `				return rc;` |
|      - | 1312 | `			}` |
|     12 | 1313 | `		}` |
|      1 | 1314 | `	}` |
|      - | 1315 | `	/* Object are equals */` |
|     13 | 1316 | `	return 0;` |
|    117 | 1317 | `}` |
|      - | 1318 | `/*` |
|      - | 1319 | ` * Dump a class instance and the store the dump in the BLOB given` |
|      - | 1320 | ` * as the first argument.` |
|      - | 1321 | ` * Note that only non-static/non-constants attribute are dumped.` |
|      - | 1322 | ` * This function is typically invoked when the user issue a call` |
|      - | 1323 | ` * to [var_dump(),var_export(),print_r(),...].` |
|      - | 1324 | ` * This function SXRET_OK on success. Any other return value including` |
|      - | 1325 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|      - | 1326 | ` */` |
|      - | 1327 | `/*` |
|      - | 1328 | ` * Emit a class-instance dump header plus its trailing newline. For var_dump` |
|      - | 1329 | ` * (ShowType) it completes the "object(" prefix the caller already emitted as` |
|      - | 1330 | ` *   ClassName)#<id> (<count>) {` |
|      - | 1331 | ` * for print_r it emits the legacy PHL  Object(ClassName) {  (count/id unused).` |
|      - | 1332 | ` */` |
|    134 | 1333 | `static void DumpClassInstanceHeader(SyBlob *pOut,ph7_class *pClass,sxu32 nObjId,int ShowType,sxu32 nCount)` |
|      3 | 1334 | `{` |
|    137 | 1335 | `	if( !ShowType ){` |
|      6 | 1336 | `		SyBlobAppend(&(*pOut),"Object(",sizeof("Object(")-1);` |
|      6 | 1337 | `		SyBlobFormat(&(*pOut),"%z) {",&pClass->sName);` |
|      4 | 1338 | `	}else{` |
|    133 | 1339 | `		SyBlobFormat(&(*pOut),"%z)#%u (%u) {",&pClass->sName,nObjId,nCount);` |
|      - | 1340 | `	}` |
|      - | 1341 | `#ifdef __WINNT__` |
|      3 | 1342 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|      - | 1343 | `#else` |
|    134 | 1344 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      - | 1345 | `#endif` |
|    137 | 1346 | `}` |
|    138 | 1347 | `PH7_PRIVATE sxi32 PH7_ClassInstanceDump(SyBlob *pOut,ph7_class_instance *pThis,int ShowType,int nTab,int nDepth)` |
|      3 | 1348 | `{` |
|      - | 1349 | `	SyHashEntry *pEntry;` |
|      - | 1350 | `	ph7_value *pValue;` |
|      - | 1351 | `	sxi32 rc;` |
|      - | 1352 | `	int i;` |
|    141 | 1353 | `	if( nDepth > 31 ){` |
|      - | 1354 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|      - | 1355 | `		/* Nesting limit reached..halt immediately*/` |
|      5 | 1356 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|      5 | 1357 | `		if( ShowType ){` |
|      5 | 1358 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|      2 | 1359 | `		}` |
|      5 | 1360 | `		return SXERR_LIMIT;` |
|      - | 1361 | `	}` |
|    137 | 1362 | `	rc = SXRET_OK;` |
|      - | 1363 | `	{` |
|      - | 1364 | `		/* Both var_dump and print_r consult __debugInfo() (PHP behavior);` |
|      - | 1365 | `		 * var_export uses a separate renderer and never reaches here. When the` |
|      - | 1366 | `		 * method is present and returns an array, render that array's entries as` |
|      - | 1367 | `		 * the object body, with the header showing the debug array's count. The` |
|      - | 1368 | `		 * nDepth guard above protects against a __debugInfo returning the object` |
|      - | 1369 | `		 * itself. */` |
|    137 | 1370 | `		ph7_class_method *pDbg = PH7_ClassExtractMethod(pThis->pClass,"__debugInfo",sizeof("__debugInfo")-1);` |
|    137 | 1371 | `		if( pDbg ){` |
|      - | 1372 | `			ph7_value sResult;` |
|      5 | 1373 | `			PH7_MemObjInit(pThis->pVm,&sResult);` |
|      5 | 1374 | `			PH7_VmCallClassMethod(pThis->pVm,pThis,pDbg,&sResult,0,0);` |
|      5 | 1375 | `			if( sResult.iFlags & MEMOBJ_HASHMAP ){` |
|      5 | 1376 | `				ph7_hashmap *pMap = (ph7_hashmap *)sResult.x.pOther;` |
|      - | 1377 | `				/* Header count is the debug array's entry count. */` |
|      5 | 1378 | `				DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,pMap->nEntry);` |
|      5 | 1379 | `				rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth);` |
|      9 | 1380 | `				for( i = 0 ; i < nTab ; i++ ){` |
|      5 | 1381 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      3 | 1382 | `				}` |
|      5 | 1383 | `				SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      5 | 1384 | `				PH7_MemObjRelease(&sResult);` |
|      5 | 1385 | `				return rc;` |
|      - | 1386 | `			}` |
|      - | 1387 | `			/* Non-array return: behave as if __debugInfo were absent. */` |
|    ! 0 | 1388 | `			PH7_MemObjRelease(&sResult);` |
|    ! 0 | 1389 | `		}` |
|      - | 1390 | `	}` |
|      - | 1391 | `	{` |
|      - | 1392 | `		/* var_dump's header needs the property count up front, so pre-count the` |
|      - | 1393 | `		 * non-static/non-constant attributes (matching the dump loop below). */` |
|    132 | 1394 | `		sxu32 nProp = 0;` |
|    132 | 1395 | `		if( ShowType ){` |
|    130 | 1396 | `			SyHashResetLoopCursor(&pThis->hAttr);` |
|    266 | 1397 | `			while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|    138 | 1398 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    138 | 1399 | `				if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|    134 | 1400 | `					nProp++;` |
|     66 | 1401 | `				}` |
|      2 | 1402 | `			}` |
|     64 | 1403 | `		}` |
|    132 | 1404 | `		DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,nProp);` |
|      - | 1405 | `	}` |
|      - | 1406 | `	/* Dump object attributes */` |
|    132 | 1407 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|    213 | 1408 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|    142 | 1409 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    142 | 1410 | `		if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|      - | 1411 | `			/* Dump non-static/constant attribute only */` |
|   3994 | 1412 | `			for( i = 0 ; i < nTab ; i++ ){` |
|   3858 | 1413 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|   1930 | 1414 | `			}` |
|    138 | 1415 | `			pValue = ExtractClassAttrValue(pThis->pVm,pVmAttr);` |
|    138 | 1416 | `			if( pValue ){` |
|    138 | 1417 | `				SyBlobFormat(&(*pOut),"['%z'] =>",&pVmAttr->pAttr->sName);` |
|      - | 1418 | `#ifdef __WINNT__` |
|      2 | 1419 | `				SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|      - | 1420 | `#else` |
|    136 | 1421 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      - | 1422 | `#endif` |
|    138 | 1423 | `				rc = PH7_MemObjDump(&(*pOut),pValue,ShowType,nTab+1,nDepth,0);` |
|    138 | 1424 | `				if( rc == SXERR_LIMIT ){` |
|    125 | 1425 | `					break;` |
|      - | 1426 | `				}` |
|      6 | 1427 | `			}` |
|      6 | 1428 | `		}` |
|      2 | 1429 | `	}` |
|   3982 | 1430 | `	for( i = 0 ; i < nTab ; i++ ){` |
|   3852 | 1431 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|   1927 | 1432 | `	}` |
|    132 | 1433 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|    132 | 1434 | `	return rc;` |
|     72 | 1435 | `}` |
|      - | 1436 | `/*` |
|      - | 1437 | ` * Call a magic method [i.e: __toString(),__toBool(),__Invoke()...]` |
|      - | 1438 | ` * Return SXRET_OK on successfull call. Any other return value indicates failure.` |
|      - | 1439 | ` * Notes on magic methods.` |
|      - | 1440 | ` * According to the PHP language reference manual.` |
|      - | 1441 | ` *  The function names __construct(), __destruct(), __call(), __callStatic()` |
|      - | 1442 | ` *  __get(),  __toString(), __invoke(), __clone() are magical in PHP classes.` |
|      - | 1443 | ` * You cannot have functions with these names in any of your classes unless` |
|      - | 1444 | ` * you want the magic functionality associated with them.` |
|      - | 1445 | ` * Example of magical methods:` |
|      - | 1446 | ` * __toString()` |
|      - | 1447 | ` *  The __toString() method allows a class to decide how it will react when it is treated like` |
|      - | 1448 | ` *  a string. For example, what echo $obj; will print. This method must return a string.` |
|      - | 1449 | ` *  Example #2 Simple example` |
|      - | 1450 | ` * <?php` |
|      - | 1451 | ` * // Declare a simple class` |
|      - | 1452 | ` * class TestClass` |
|      - | 1453 | ` * {` |
|      - | 1454 | ` *   public $foo;` |
|      - | 1455 | ` *` |
|      - | 1456 | ` *   public function __construct($foo)` |
|      - | 1457 | ` *   {` |
|      - | 1458 | ` *       $this->foo = $foo;` |
|      - | 1459 | ` *   }` |
|      - | 1460 | ` *` |
|      - | 1461 | ` *   public function __toString()` |
|      - | 1462 | ` *   {` |
|      - | 1463 | ` *       return $this->foo;` |
|      - | 1464 | ` *   }` |
|      - | 1465 | ` * }` |
|      - | 1466 | ` * $class = new TestClass('Hello');` |
|      - | 1467 | ` * echo $class;` |
|      - | 1468 | ` * ?>` |
|      - | 1469 | ` * The above example will output:` |
|      - | 1470 | ` *  Hello` |
|      - | 1471 | ` *` |
|      - | 1472 | ` * Note that PH7 does not support all the magical method and introudces __toFloat(),__toInt()` |
|      - | 1473 | ` * which have the same behaviour as __toString() but for float and integer types` |
|      - | 1474 | ` * respectively.` |
|      - | 1475 | ` * Refer to the official documentation for more information.` |
|      - | 1476 | ` */` |
|     26 | 1477 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallMagicMethod(` |
|      - | 1478 | `	ph7_vm *pVm,               /* VM that own all this stuff */` |
|      - | 1479 | `	ph7_class *pClass,         /* Target class */` |
|      - | 1480 | `	ph7_class_instance *pThis, /* Target object */` |
|      - | 1481 | `	const char *zMethod,       /* Magic method name [i.e: __toString()]*/` |
|      - | 1482 | `	sxu32 nByte,               /* zMethod length*/` |
|      - | 1483 | `	const SyString *pAttrName  /* Attribute name */` |
|      - | 1484 | `	)` |
|      1 | 1485 | `{` |
|     27 | 1486 | `	ph7_value *apArg[2] = { 0 , 0 };` |
|      - | 1487 | `	ph7_class_method *pMeth;` |
|      - | 1488 | `	ph7_value sAttr; /* cc warning */` |
|      - | 1489 | `	sxi32 rc;` |
|      - | 1490 | `	int nArg;` |
|      - | 1491 | `	/* Make sure the magic method is available */` |
|     27 | 1492 | `	pMeth = PH7_ClassExtractMethod(&(*pClass),zMethod,nByte);` |
|     27 | 1493 | `	if( pMeth == 0 ){` |
|      - | 1494 | `		/* No such method,return immediately */` |
|     27 | 1495 | `		return SXERR_NOTFOUND;` |
|      - | 1496 | `	}` |
|    ! 0 | 1497 | `	nArg = 0;` |
|      - | 1498 | `	/* Copy arguments */` |
|    ! 0 | 1499 | `	if( pAttrName ){` |
|    ! 0 | 1500 | `		PH7_MemObjInitFromString(pVm,&sAttr,pAttrName);` |
|    ! 0 | 1501 | `		sAttr.nIdx = SXU32_HIGH; /* Mark as constant */` |
|    ! 0 | 1502 | `		apArg[0] = &sAttr;` |
|    ! 0 | 1503 | `		nArg = 1;` |
|    ! 0 | 1504 | `	}` |
|      - | 1505 | `	/* Call the magic method now */` |
|    ! 0 | 1506 | `	rc = PH7_VmCallClassMethod(pVm,&(*pThis),pMeth,0,nArg,apArg);` |
|      - | 1507 | `	/* Clean up */` |
|    ! 0 | 1508 | `	if( pAttrName ){` |
|    ! 0 | 1509 | `		PH7_MemObjRelease(&sAttr);` |
|    ! 0 | 1510 | `	}` |
|    ! 0 | 1511 | `	return rc;` |
|     14 | 1512 | `}` |
|      - | 1513 | `/*` |
|      - | 1514 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon].` |
|      - | 1515 | ` * This function is simply a wrapper on ExtractClassAttrValue().` |
|      - | 1516 | ` */` |
|    100 | 1517 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceExtractAttrValue(ph7_class_instance *pThis,VmClassAttr *pAttr)` |
|      3 | 1518 | `{` |
|      - | 1519 | `   /* Extract the attribute value */` |
|      - | 1520 | `	ph7_value *pValue;` |
|    103 | 1521 | `	pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|    103 | 1522 | `	return pValue;` |
|      3 | 1523 | `}` |
|      - | 1524 | `/*` |
|      - | 1525 | ` * Convert a class instance [i.e: Object in the PHP jargon] into a hashmap [i.e: array in the PHP jargon].` |
|      - | 1526 | ` * Return SXRET_OK on success. Any other value indicates failure.` |
|      - | 1527 | ` * Note on object conversion to array:` |
|      - | 1528 | ` *  Acccording to the PHP language reference manual` |
|      - | 1529 | ` *  If an object is converted to an array, the result is an array whose elements are the object's properties.` |
|      - | 1530 | ` *  The keys are the member variable names.` |
|      - | 1531 | ` *` |
|      - | 1532 | ` *  The following example:` |
|      - | 1533 | ` *  class Test {` |
|      - | 1534 | ` *   public $A = 25<<1;  // 50` |
|      - | 1535 | ` *	 public $c = rand_str(3);   // Random string of length 3` |
|      - | 1536 | ` *	 public $d = rand() & 1023; // Random number between 0..1023` |
|      - | 1537 | ` *  }` |
|      - | 1538 | ` *  var_dump((array) new Test());` |
|      - | 1539 | ` *	Will output:` |
|      - | 1540 | ` *  array(3) {` |
|      - | 1541 | ` *   [A] =>` |
|      - | 1542 | ` *      int(50)` |
|      - | 1543 | ` *   [c] =>` |
|      - | 1544 | ` *     string(3 'aps')` |
|      - | 1545 | ` *   [d] =>` |
|      - | 1546 | ` *     int(991)` |
|      - | 1547 | ` *  }` |
|      - | 1548 | ` * You have noticed that PH7 allow class attributes [i.e: $a,$c,$d in the example above]` |
|      - | 1549 | ` * have any complex expression (even function calls/Annonymous functions) as their default` |
|      - | 1550 | ` * value unlike the standard PHP engine.` |
|      - | 1551 | ` * This is a very powerful feature that you have to look at.` |
|      - | 1552 | ` */` |
|     12 | 1553 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|      1 | 1554 | `{` |
|      - | 1555 | `	SyHashEntry *pEntry;` |
|      - | 1556 | `	SyString *pAttrName;` |
|      - | 1557 | `	VmClassAttr *pAttr;` |
|      - | 1558 | `	ph7_value *pValue;` |
|      - | 1559 | `	ph7_value sName;` |
|      - | 1560 | `	/* Reset the loop cursor */` |
|     13 | 1561 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|     13 | 1562 | `	PH7_MemObjInitFromString(pThis->pVm,&sName,0);` |
|     47 | 1563 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|      - | 1564 | `		/* Point to the current attribute */` |
|     29 | 1565 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1566 | `		/* Extract attribute value */` |
|     29 | 1567 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     29 | 1568 | `		if( pValue ){` |
|      - | 1569 | `			/* Build attribute name */` |
|     29 | 1570 | `			pAttrName = &pAttr->pAttr->sName;` |
|     29 | 1571 | `			PH7_MemObjStringAppend(&sName,pAttrName->zString,pAttrName->nByte);` |
|      - | 1572 | `			/* Perform the insertion */` |
|     29 | 1573 | `			PH7_HashmapInsert(pMap,&sName,pValue);` |
|      - | 1574 | `			/* Reset the string cursor */` |
|     29 | 1575 | `			SyBlobReset(&sName.sBlob);` |
|     14 | 1576 | `		}` |
|      1 | 1577 | `	}` |
|     13 | 1578 | `	PH7_MemObjRelease(&sName);` |
|     13 | 1579 | `	return SXRET_OK;` |
|      1 | 1580 | `}` |
|      - | 1581 | `/*` |
|      - | 1582 | ` * Iterate throw class attributes and invoke the given callback [i.e: xWalk()] for each` |
|      - | 1583 | ` * retrieved attribute.` |
|      - | 1584 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|      - | 1585 | ` * the attribute value in the callback body will not alter the real attribute value.` |
|      - | 1586 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|      - | 1587 | ` * a value different from PH7_OK.` |
|      - | 1588 | ` * Refer to [ph7_object_walk()] for more information.` |
|      - | 1589 | ` */` |
|     40 | 1590 | `PH7_PRIVATE sxi32 PH7_ClassInstanceWalk(` |
|      - | 1591 | `	ph7_class_instance *pThis, /* Target object */` |
|      - | 1592 | `	int (*xWalk)(const char *,ph7_value *,void *), /* Walker callback */` |
|      - | 1593 | `	void *pUserData /* Last argument to xWalk() */` |
|      - | 1594 | `	)` |
|      2 | 1595 | `{` |
|      - | 1596 | `	SyHashEntry *pEntry; /* Hash entry */` |
|      - | 1597 | `	VmClassAttr *pAttr;  /* Pointer to the attribute */` |
|      - | 1598 | `	ph7_value *pValue;   /* Attribute value */` |
|      - | 1599 | `	ph7_value sValue;    /* Copy of the attribute value */` |
|      - | 1600 | `	int rc;` |
|      - | 1601 | `	/* Reset the loop cursor */` |
|     42 | 1602 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|     42 | 1603 | `	PH7_MemObjInit(pThis->pVm,&sValue);` |
|      - | 1604 | `	/* Start the walk process */` |
|    124 | 1605 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|      - | 1606 | `		/* Point to the current attribute */` |
|     64 | 1607 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1608 | `		/* Extract attribute value */` |
|     64 | 1609 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     64 | 1610 | `		if( pValue ){` |
|     64 | 1611 | `			PH7_MemObjLoad(pValue,&sValue);` |
|      - | 1612 | `			/* Invoke the supplied callback */` |
|     64 | 1613 | `			rc =  xWalk(SyStringData(&pAttr->pAttr->sName),&sValue,pUserData);` |
|     64 | 1614 | `			PH7_MemObjRelease(&sValue);` |
|     64 | 1615 | `			if( rc != PH7_OK){` |
|      - | 1616 | `				/* User callback request an operation abort */` |
|    ! 0 | 1617 | `				return SXERR_ABORT;` |
|      - | 1618 | `			}` |
|     31 | 1619 | `		}` |
|      2 | 1620 | `	}` |
|      - | 1621 | `	/* All done */` |
|     42 | 1622 | `	return SXRET_OK;` |
|     22 | 1623 | `}` |
|      - | 1624 | `/*` |
|      - | 1625 | ` * Extract a class atrribute value.` |
|      - | 1626 | ` * Return a pointer to the attribute value on success. Otherwise NULL.` |
|      - | 1627 | ` * Note:` |
|      - | 1628 | ` *  Access to static and constant attribute is not allowed. That is,the function` |
|      - | 1629 | ` *  will return NULL in case someone (host-application code) try to extract` |
|      - | 1630 | ` *  a static/constant attribute.` |
|      - | 1631 | ` */` |
|   5764 | 1632 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceFetchAttr(ph7_class_instance *pThis,const SyString *pName)` |
|      5 | 1633 | `{` |
|      - | 1634 | `	SyHashEntry *pEntry;` |
|      - | 1635 | `	VmClassAttr *pAttr;` |
|      - | 1636 | `	/* Query the attribute hashtable */` |
|   5769 | 1637 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);` |
|   5769 | 1638 | `	if( pEntry == 0 ){` |
|      - | 1639 | `		/* No such attribute */` |
|    ! 0 | 1640 | `		return 0;` |
|      - | 1641 | `	}` |
|      - | 1642 | `	/* Point to the class atrribute */` |
|   5769 | 1643 | `	pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1644 | `	/* Check if we are dealing with a static/constant attribute */` |
|   5769 | 1645 | `	if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|      - | 1646 | `		/* Access is forbidden */` |
|    ! 0 | 1647 | `		return 0;` |
|      - | 1648 | `	}` |
|      - | 1649 | `	/* Return the attribute value */` |
|   5769 | 1650 | `	return ExtractClassAttrValue(pThis->pVm,pAttr);` |
|   2887 | 1651 | `}` |
|      - | 1652 |  |
