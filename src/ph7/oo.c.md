# src/ph7/oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 684/780 lines (87.69%)

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
|  265220 |   14 | `PH7_PRIVATE ph7_class * PH7_NewRawClass(ph7_vm *pVm,const SyString *pName,sxu32 nLine)` |
|       5 |   15 | `{` |
|       - |   16 | `	ph7_class *pClass;` |
|       - |   17 | `	char *zName;` |
|       - |   18 | `	/* Allocate a new instance */` |
|  265225 |   19 | `	pClass = (ph7_class *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class));` |
|  265225 |   20 | `	if( pClass == 0 ){` |
|     ! 0 |   21 | `		return 0;` |
|       - |   22 | `	}` |
|       - |   23 | `	/* Zero the structure */` |
|  265225 |   24 | `	SyZero(pClass,sizeof(ph7_class));` |
|       - |   25 | `	/* Duplicate class name */` |
|  265225 |   26 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  265225 |   27 | `	if( zName == 0 ){` |
|     ! 0 |   28 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClass);` |
|     ! 0 |   29 | `		return 0;` |
|       - |   30 | `	}` |
|       - |   31 | `	/* Initialize fields */` |
|  265225 |   32 | `	SyStringInitFromBuf(&pClass->sName,zName,pName->nByte);` |
|  265225 |   33 | `	SyHashInit(&pClass->hMethod,&pVm->sAllocator,0,0);` |
|  265225 |   34 | `	SyHashInit(&pClass->hAttr,&pVm->sAllocator,0,0);` |
|  265225 |   35 | `	SyHashInit(&pClass->hDerived,&pVm->sAllocator,0,0);` |
|  265225 |   36 | `	SySetInit(&pClass->aInterface,&pVm->sAllocator,sizeof(ph7_class *));` |
|  265225 |   37 | `	SySetInit(&pClass->aTrait,&pVm->sAllocator,sizeof(ph7_class *));` |
|  265225 |   38 | `	SySetInit(&pClass->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|  265225 |   39 | `	SySetInit(&pClass->aEnumCases,&pVm->sAllocator,sizeof(ph7_class_attr *));` |
|  265225 |   40 | `	pClass->nLine = nLine;` |
|  265225 |   41 | `	if( pVm->bCompilingBuiltin ){` |
|       - |   42 | `		/* Defined by an embedded builtin chunk: internal, no defining file.` |
|       - |   43 | `		 * Class compilers merge further flags with \|= so this survives. */` |
|  263301 |   44 | `		pClass->iFlags \|= PH7_CLASS_INTERNAL;` |
|  131653 |   45 | `	}else{` |
|       - |   46 | `		/* Alias the VM-lifetime path dup on top of the include stack */` |
|    1929 |   47 | `		SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|    1929 |   48 | `		if( pFile ){` |
|    1929 |   49 | `			SyStringDupPtr(&pClass->sFile,pFile);` |
|     962 |   50 | `		}` |
|       - |   51 | `	}` |
|       - |   52 | `	/* All done */` |
|  265225 |   53 | `	return pClass;` |
|  132615 |   54 | `}` |
|       - |   55 | `/*` |
|       - |   56 | ` * Allocate and initialize a new class attribute.` |
|       - |   57 | ` * Return a pointer to the class attribute on success. NULL otherwise.` |
|       - |   58 | ` */` |
|  369114 |   59 | `PH7_PRIVATE ph7_class_attr * PH7_NewClassAttr(ph7_vm *pVm,const SyString *pName,sxu32 nLine,sxi32 iProtection,sxi32 iFlags)` |
|       5 |   60 | `{` |
|       - |   61 | `	ph7_class_attr *pAttr;` |
|       - |   62 | `	char *zName;` |
|  369119 |   63 | `	pAttr = (ph7_class_attr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_attr));` |
|  369119 |   64 | `	if( pAttr == 0 ){` |
|     ! 0 |   65 | `		return 0;` |
|       - |   66 | `	}` |
|       - |   67 | `	/* Zero the structure */` |
|  369119 |   68 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|  369119 |   69 | `	SySetInit(&pAttr->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|       - |   70 | `	/* Duplicate attribute name */` |
|  369119 |   71 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  369119 |   72 | `	if( zName == 0 ){` |
|     ! 0 |   73 | `		SyMemBackendPoolFree(&pVm->sAllocator,pAttr);` |
|     ! 0 |   74 | `		return 0;` |
|       - |   75 | `	}` |
|       - |   76 | `	/* Initialize fields */` |
|  369119 |   77 | `	SySetInit(&pAttr->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|  369119 |   78 | `	SySetInit(&pAttr->aUnionAlts,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|  369119 |   79 | `	SyStringInitFromBuf(&pAttr->sName,zName,pName->nByte);` |
|  369119 |   80 | `	pAttr->iProtection = iProtection;` |
|  369119 |   81 | `	pAttr->nIdx = SXU32_HIGH;` |
|  369119 |   82 | `	pAttr->iFlags = iFlags;` |
|  369119 |   83 | `	pAttr->nLine = nLine;` |
|  369119 |   84 | `	return pAttr;` |
|  184562 |   85 | `}` |
|       - |   86 | `/*` |
|       - |   87 | ` * Allocate and initialize a new class method.` |
|       - |   88 | ` * Return a pointer to the class method on success. NULL otherwise` |
|       - |   89 | ` * This function associate with the newly created method an automatically generated` |
|       - |   90 | ` * random unique name.` |
|       - |   91 | ` */` |
| 1395626 |   92 | `PH7_PRIVATE ph7_class_method * PH7_NewClassMethod(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,sxu32 nLine,` |
|       - |   93 | `	sxi32 iProtection,sxi32 iFlags,sxi32 iFuncFlags)` |
|       5 |   94 | `{` |
|       - |   95 | `	ph7_class_method *pMeth;` |
|       - |   96 | `	SyHashEntry *pEntry;` |
|       - |   97 | `	SyString *pNamePtr;` |
|       - |   98 | `	char zSalt[10];` |
|       - |   99 | `	char *zName;` |
|       - |  100 | `	sxu32 nByte;` |
|       - |  101 | `	/* Allocate a new class method instance */` |
| 1395631 |  102 | `	pMeth = (ph7_class_method *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_method));` |
| 1395631 |  103 | `	if( pMeth == 0 ){` |
|     ! 0 |  104 | `		return 0;` |
|       - |  105 | `	}` |
|       - |  106 | `	/* Zero the structure */` |
| 1395631 |  107 | `	SyZero(pMeth,sizeof(ph7_class_method));` |
|       - |  108 | `	/* Check for an already installed method with the same name */` |
| 1395631 |  109 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
| 1395631 |  110 | `	if( pEntry == 0 ){` |
|       - |  111 | `		/* Associate an unique VM name to this method */` |
| 1395629 |  112 | `		nByte = sizeof(zSalt) + pName->nByte + SyStringLength(&pClass->sName)+sizeof(char)*7/*[[__'\0'*/;` |
| 1395629 |  113 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,nByte);` |
| 1395629 |  114 | `		if( zName == 0 ){` |
|     ! 0 |  115 | `			SyMemBackendPoolFree(&pVm->sAllocator,pMeth);` |
|     ! 0 |  116 | `			return 0;` |
|       - |  117 | `		}` |
| 1395629 |  118 | `		pNamePtr = &pMeth->sVmName;` |
|       - |  119 | `		/* Generate a random string */` |
| 1395629 |  120 | `		PH7_VmRandomString(&(*pVm),zSalt,sizeof(zSalt));` |
| 1395629 |  121 | `		pNamePtr->nByte = SyBufferFormat(zName,nByte,"[__%z@%z_%.*s]",&pClass->sName,pName,sizeof(zSalt),zSalt);` |
| 1395629 |  122 | `		pNamePtr->zString = zName;` |
|  697817 |  123 | `	}else{` |
|       - |  124 | `		/* Method is condidate for 'overloading' */` |
|       3 |  125 | `		ph7_class_method *pCurrent = (ph7_class_method *)pEntry->pUserData;` |
|       3 |  126 | `		pNamePtr = &pMeth->sVmName;` |
|       - |  127 | `		/* Use the same VM name */` |
|       3 |  128 | `		SyStringDupPtr(pNamePtr,&pCurrent->sVmName);` |
|       3 |  129 | `		zName = (char *)pNamePtr->zString;` |
|       - |  130 | `	}` |
| 1395631 |  131 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|   58120 |  132 | `		if( (pName->nByte == sizeof("__destruct") - 1 && SyMemcmp(pName->zString,"__destruct",sizeof("__destruct") - 1 ) == 0)` |
|   58125 |  133 | `			\|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|       - |  134 | `				/* Switch to public visibility for destructors and legacy class-name` |
|       - |  135 | `				 * constructors (the engine invokes destructors internally, bypassing` |
|       - |  136 | `				 * visibility either way). __construct KEEPS its declared visibility` |
|       - |  137 | ``				 * (band A #4): php enforces it at `new` — a private/protected ctor`` |
|       - |  138 | `				 * from the wrong scope is a catchable Error, checked at OP_NEW —` |
|       - |  139 | `				 * and ReflectionClass::isInstantiable()/newInstance() now see it. */` |
|     ! 0 |  140 | `				iProtection = PH7_CLASS_PROT_PUBLIC;` |
|     ! 0 |  141 | `		}` |
|   29060 |  142 | `	}` |
|       - |  143 | `	/* Initialize method fields */` |
| 1395631 |  144 | `	pMeth->iProtection = iProtection;` |
| 1395631 |  145 | `	pMeth->iFlags = iFlags;` |
| 1395631 |  146 | `	pMeth->nLine = nLine;` |
| 2093444 |  147 | `	PH7_VmInitFuncState(&(*pVm),&pMeth->sFunc,&zName[sizeof(char)*4/*[__@*/+SyStringLength(&pClass->sName)],` |
| 1395626 |  148 | `		pName->nByte,iFuncFlags\|VM_FUNC_CLASS_METHOD,pClass);` |
| 1395631 |  149 | `	return pMeth;` |
|  697818 |  150 | `}` |
|       - |  151 | `/*` |
|       - |  152 | ` * Check if the given name have a class method associated with it.` |
|       - |  153 | ` * Return the desired method [i.e: ph7_class_method instance] on success. NULL otherwise.` |
|       - |  154 | ` */` |
|  338168 |  155 | `PH7_PRIVATE ph7_class_method * PH7_ClassExtractMethod(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|       5 |  156 | `{` |
|       - |  157 | `	SyHashEntry *pEntry;` |
|       - |  158 | `	/* Perform a hash lookup */` |
|  338173 |  159 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)zName,nByte);` |
|  338173 |  160 | `	if( pEntry == 0 ){` |
|       - |  161 | `		/* No such entry */` |
|    7417 |  162 | `		return 0;` |
|       - |  163 | `	}` |
|       - |  164 | `	/* Point to the desired method */` |
|  330761 |  165 | `	return (ph7_class_method *)pEntry->pUserData;` |
|  169089 |  166 | `}` |
|       - |  167 | `/*` |
|       - |  168 | ` * Check if the given name is a class attribute.` |
|       - |  169 | ` * Return the desired attribute [i.e: ph7_class_attr instance] on success.NULL otherwise.` |
|       - |  170 | ` */` |
|  210586 |  171 | `PH7_PRIVATE ph7_class_attr * PH7_ClassExtractAttribute(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|       5 |  172 | `{` |
|       - |  173 | `	SyHashEntry *pEntry;` |
|       - |  174 | `	/* Perform a hash lookup */` |
|  210591 |  175 | `	pEntry = SyHashGet(&pClass->hAttr,(const void *)zName,nByte);` |
|  210591 |  176 | `	if( pEntry == 0 ){` |
|       - |  177 | `		/* No such entry */` |
|  210205 |  178 | `		return 0;` |
|       - |  179 | `	}` |
|       - |  180 | `	/* Point to the desierd method */` |
|     391 |  181 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|  105298 |  182 | `}` |
|       - |  183 | `/*` |
|       - |  184 | ` * Install a class attribute in the corresponding container.` |
|       - |  185 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|       - |  186 | ` */` |
|  369110 |  187 | `PH7_PRIVATE sxi32 PH7_ClassInstallAttr(ph7_class *pClass,ph7_class_attr *pAttr)` |
|       5 |  188 | `{` |
|  369115 |  189 | `	SyString *pName = &pAttr->sName;` |
|       - |  190 | `	sxi32 rc;` |
|       - |  191 | `	/* Remember where this attribute was originally declared so that later` |
|       - |  192 | `	 * inheritance/trait copies still know the declaring class (needed for` |
|       - |  193 | `	 * PHP-compatible error messages on typed properties). */` |
|  369115 |  194 | `	if( pAttr->pDeclClass == 0 ){` |
|  369115 |  195 | `		pAttr->pDeclClass = pClass;` |
|  184555 |  196 | `	}` |
|  369115 |  197 | `	rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|  369115 |  198 | `	return rc;` |
|       5 |  199 | `}` |
|       - |  200 | `/*` |
|       - |  201 | ` * Install a class method in the corresponding container.` |
|       - |  202 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|       - |  203 | ` */` |
| 1395614 |  204 | `PH7_PRIVATE sxi32 PH7_ClassInstallMethod(ph7_class *pClass,ph7_class_method *pMeth)` |
|       5 |  205 | `{` |
| 1395619 |  206 | `	SyString *pName = &pMeth->sFunc.sName;` |
|       - |  207 | `	sxi32 rc;` |
| 1395619 |  208 | `	rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 1395619 |  209 | `	return rc;` |
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
|   31160 |  234 | `static int OoClassifyOverrideType(ph7_vm *pVm, sxu32 nType, const SyString *pClass,` |
|       - |  235 | `	int bUnion, ph7_class **ppClass)` |
|       5 |  236 | `{` |
|   31165 |  237 | `	*ppClass = 0;` |
|   31165 |  238 | `	if( bUnion ){` |
|       3 |  239 | `		return OVT_SKIP; /* union/intersection — full lattice, skip */` |
|       - |  240 | `	}` |
|   31163 |  241 | `	if( nType == 0 ){` |
|   31079 |  242 | `		return OVT_NONE; /* no declared type */` |
|       - |  243 | `	}` |
|      89 |  244 | `	if( nType == SXU32_HIGH ){` |
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
|      68 |  267 | `	if( nType == MEMOBJ_STRING \|\| nType == MEMOBJ_INT \|\| nType == MEMOBJ_REAL` |
|      29 |  268 | `	 \|\| nType == MEMOBJ_BOOL \|\| nType == MEMOBJ_HASHMAP ){` |
|      48 |  269 | `		return OVT_SCALAR;` |
|       - |  270 | `	}` |
|       - |  271 | `	/* MEMOBJ_OBJ (object — subtypes against classes), MEMOBJ_VOID/NULL/RES,` |
|       - |  272 | `	 * or anything unexpected: skip. */` |
|      26 |  273 | `	return OVT_SKIP;` |
|   15585 |  274 | `}` |
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
|   31108 |  288 | `static OvType OoTypeFromReturn(ph7_vm_func *pF)` |
|       5 |  289 | `{` |
|       - |  290 | `	OvType t;` |
|   31113 |  291 | `	t.nType = pF->nReturnType;` |
|   31113 |  292 | `	t.pClass = &pF->sReturnClass;` |
|   31113 |  293 | `	t.bUnion = SySetUsed(&pF->aReturnUnion) > 0;` |
|   31113 |  294 | `	t.bNullable = (pF->iFlags & VM_FUNC_RETURN_NULLABLE) != 0;` |
|   31113 |  295 | `	return t;` |
|       5 |  296 | `}` |
|      52 |  297 | `static OvType OoTypeFromArg(ph7_vm_func_arg *pA)` |
|       3 |  298 | `{` |
|       - |  299 | `	OvType t;` |
|      55 |  300 | `	t.nType = pA->nType;` |
|      55 |  301 | `	t.pClass = &pA->sClass;` |
|      55 |  302 | `	t.bUnion = (pA->iFlags & VM_FUNC_ARG_UNION) != 0;` |
|      55 |  303 | `	t.bNullable = (pA->iFlags & VM_FUNC_ARG_NULLABLE) != 0;` |
|      55 |  304 | `	return t;` |
|       3 |  305 | `}` |
|       - |  306 | `/*` |
|       - |  307 | ` * Return TRUE if the child type is an unambiguously-invalid override of the` |
|       - |  308 | ` * parent type. bCovariant=1 for a return type (child must be ⊆ parent),` |
|       - |  309 | ` * 0 for a parameter (child must be ⊇ parent). Returns FALSE (accept) on any` |
|       - |  310 | ` * skipped/ambiguous shape.` |
|       - |  311 | ` */` |
|   15580 |  312 | `static int OoOverrideTypeBad(ph7_vm *pVm, OvType parent, OvType child, int bCovariant)` |
|       5 |  313 | `{` |
|       - |  314 | `	ph7_class *pParentCls, *pChildCls;` |
|   15585 |  315 | `	int kP = OoClassifyOverrideType(pVm, parent.nType, parent.pClass, parent.bUnion, &pParentCls);` |
|   15585 |  316 | `	int kC = OoClassifyOverrideType(pVm, child.nType, child.pClass, child.bUnion, &pChildCls);` |
|   15585 |  317 | `	if( kP == OVT_SKIP \|\| kC == OVT_SKIP ){` |
|      18 |  318 | `		return 0; /* ambiguous shape — conservatively accept */` |
|       - |  319 | `	}` |
|       - |  320 | `	/* A missing type is the TOP type. covariant (return): a concrete child is a` |
|       - |  321 | `	 * subtype of top, fine; a top child over a concrete parent WIDENS → bad.` |
|       - |  322 | `	 * contravariant (param): a top child is a supertype of anything, fine; a` |
|       - |  323 | `	 * concrete child over a top parent NARROWS → bad. (A union/intersection child` |
|       - |  324 | `	 * already fell into OVT_SKIP above, so a flagged child here is scalar/class.) */` |
|   15569 |  325 | `	if( kP == OVT_NONE \|\| kC == OVT_NONE ){` |
|   15545 |  326 | `		if( bCovariant && kC == OVT_NONE && kP != OVT_NONE ) return 1;` |
|   15545 |  327 | `		if( !bCovariant && kP == OVT_NONE && kC != OVT_NONE ) return 1;` |
|   15545 |  328 | `		return 0;` |
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
|    7795 |  349 | `}` |
|       - |  350 |  |
|       - |  351 | `/*` |
|       - |  352 | ` * Check a child method's signature against the parent method it overrides.` |
|       - |  353 | ` * Emits a PHP-style "Declaration of … must be compatible …" fatal on a clear` |
|       - |  354 | `` * incompatibility. `__construct` is exempt (PHP does not apply variance to it).`` |
|       - |  355 | ` */` |
|   31048 |  356 | `static sxi32 OoCheckOverrideCompat(ph7_gen_state *pGen, ph7_class *pBase, ph7_class *pSub,` |
|       - |  357 | `	ph7_class_method *pParent, ph7_class_method *pChild)` |
|       5 |  358 | `{` |
|   31053 |  359 | `	ph7_vm *pVm = pGen->pVm;` |
|   31053 |  360 | `	ph7_vm_func *pPF = &pParent->sFunc;` |
|   31053 |  361 | `	ph7_vm_func *pCF = &pChild->sFunc;` |
|   31053 |  362 | `	SyString *pMName = &pCF->sName;` |
|       - |  363 | `	ph7_vm_func_arg *aP, *aC;` |
|       - |  364 | `	sxu32 nPArg, nCArg, k;` |
|   31053 |  365 | `	int bBad = 0;` |
|   31048 |  366 | `	if( pMName->nByte == sizeof("__construct")-1` |
|   23280 |  367 | `	 && SyStrnmicmp(pMName->zString,"__construct",pMName->nByte) == 0 ){` |
|   15499 |  368 | `		return SXRET_OK;` |
|       - |  369 | `	}` |
|       - |  370 | `	/* Return type — covariant. */` |
|   15559 |  371 | `	bBad = OoOverrideTypeBad(pVm, OoTypeFromReturn(pPF), OoTypeFromReturn(pCF), /* bCovariant */ 1);` |
|       - |  372 | `	/* Each overlapping parameter — contravariant. */` |
|   15559 |  373 | `	nPArg = SySetUsed(&pPF->aArgs);` |
|   15559 |  374 | `	nCArg = SySetUsed(&pCF->aArgs);` |
|   15559 |  375 | `	aP = (ph7_vm_func_arg *)SySetBasePtr(&pPF->aArgs);` |
|   15559 |  376 | `	aC = (ph7_vm_func_arg *)SySetBasePtr(&pCF->aArgs);` |
|   15585 |  377 | `	for( k = 0; !bBad && k < nPArg && k < nCArg; k++ ){` |
|      29 |  378 | `		bBad = OoOverrideTypeBad(pVm, OoTypeFromArg(&aP[k]), OoTypeFromArg(&aC[k]), /* bCovariant */ 0);` |
|      16 |  379 | `	}` |
|       - |  380 | `	/* Parameter arity: the child must declare at least the parent's parameters and` |
|       - |  381 | `	 * may add only OPTIONAL ones — PHP rejects dropping any param (even an optional` |
|       - |  382 | `	 * one) or adding a required one. Skip the rule if either signature is variadic` |
|       - |  383 | `	 * (arity semantics differ). */` |
|   15559 |  384 | `	if( !bBad ){` |
|   15555 |  385 | `		int bVariadic = 0;` |
|   15579 |  386 | `		for( k = 0; k < nPArg; k++ ){ if( aP[k].iFlags & VM_FUNC_ARG_VARIADIC ) bVariadic = 1; }` |
|   15581 |  387 | `		for( k = 0; k < nCArg; k++ ){ if( aC[k].iFlags & VM_FUNC_ARG_VARIADIC ) bVariadic = 1; }` |
|   15555 |  388 | `		if( !bVariadic ){` |
|   15555 |  389 | `			if( nCArg < nPArg ){` |
|     ! 0 |  390 | `				bBad = 1; /* dropped a parent parameter */` |
|     ! 0 |  391 | `			}else{` |
|   15557 |  392 | `				for( k = nPArg; k < nCArg; k++ ){` |
|       3 |  393 | `					if( SySetUsed(&aC[k].aByteCode) == 0 ){ bBad = 1; break; } /* new required */` |
|       2 |  394 | `				}` |
|       - |  395 | `			}` |
|    7775 |  396 | `		}` |
|    7775 |  397 | `	}` |
|   15559 |  398 | `	if( bBad ){` |
|       8 |  399 | `		sxi32 rc = PH7_GenCompileError(&(*pGen),E_ERROR,pChild->nLine,` |
|       - |  400 | `			"Declaration of %z::%z() must be compatible with %z::%z()",` |
|       2 |  401 | `			&pSub->sName,pMName,&pBase->sName,&pParent->sFunc.sName);` |
|       6 |  402 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  403 | `			return SXERR_ABORT;` |
|       - |  404 | `		}` |
|       2 |  405 | `	}` |
|   15559 |  406 | `	return SXRET_OK;` |
|   15529 |  407 | `}` |
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
|  124142 |  449 | `PH7_PRIVATE sxi32 PH7_ClassInherit(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase)` |
|       5 |  450 | `{` |
|       - |  451 | `	ph7_class_method *pMeth;` |
|       - |  452 | `	ph7_class_attr *pAttr;` |
|       - |  453 | `	SyHashEntry *pEntry;` |
|       - |  454 | `	SyString *pName;` |
|       - |  455 | `	sxi32 rc;` |
|       - |  456 | `	/* Install in the derived hashtable */` |
|  124147 |  457 | `	rc = SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|  124147 |  458 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  459 | `		return rc;` |
|       - |  460 | `	}` |
|       - |  461 | `	/* readonly class inheritance (PHP 8.2): a readonly class may only extend a` |
|       - |  462 | `	 * readonly class, and a non-readonly class may not extend a readonly one. */` |
|  124147 |  463 | `	if( (pBase->iFlags & PH7_CLASS_READONLY) != (pSub->iFlags & PH7_CLASS_READONLY) ){` |
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
|  124147 |  478 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
|  806095 |  479 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|       - |  480 | `		/* Make sure the private attributes are not redeclared in the subclass */` |
|  681953 |  481 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  681953 |  482 | `		pName = &pAttr->sName;` |
|  681953 |  483 | `		if( (pEntry = SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte)) != 0 ){` |
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
|  681932 |  513 | `		if( pAttr->iProtection != PH7_CLASS_PROT_PRIVATE` |
|  340977 |  514 | `		 \|\| (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|  681933 |  515 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|  681933 |  516 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  517 | `				return rc;` |
|       - |  518 | `			}` |
|  340964 |  519 | `		}` |
|       5 |  520 | `	}` |
|  124147 |  521 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
| 1867355 |  522 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|       - |  523 | `		/* Make sure the private/final methods are not redeclared in the subclass */` |
| 1743213 |  524 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
| 1743213 |  525 | `		pName = &pMeth->sFunc.sName;` |
| 1743213 |  526 | `		if( (pEntry = SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|   31057 |  527 | `			 if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|       - |  528 | `				/* Cannot Overwrite final method */` |
|       8 |  529 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_method *)pEntry->pUserData)->nLine,` |
|       - |  530 | `					"Cannot Overwrite final method '%z:%z' inside child class '%z'",` |
|       2 |  531 | `					&pBase->sName,pName,&pSub->sName);` |
|       6 |  532 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  533 | `					return SXERR_ABORT;` |
|       - |  534 | `				}` |
|       4 |  535 | `			}else{` |
|       - |  536 | `				/* Check the override's signature is compatible with the parent's. */` |
|   46577 |  537 | `				rc = OoCheckOverrideCompat(&(*pGen),pBase,pSub,pMeth,` |
|   31048 |  538 | `					(ph7_class_method *)pEntry->pUserData);` |
|   31053 |  539 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  540 | `					return SXERR_ABORT;` |
|       - |  541 | `				}` |
|       - |  542 | `			}` |
|   31057 |  543 | `			continue;` |
|       - |  544 | `		}` |
|       - |  545 | `		/* Install the method */` |
| 1712161 |  546 | `		if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
| 1712157 |  547 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 1712157 |  548 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  549 | `				return rc;` |
|       - |  550 | `			}` |
|  856076 |  551 | `		}` |
|       5 |  552 | `	}` |
|       - |  553 | `	/* Mark as subclass */` |
|  124147 |  554 | `	pSub->pBase = pBase;` |
|       - |  555 | `	/* All done */` |
|  124147 |  556 | `	return SXRET_OK;` |
|   62076 |  557 | `}` |
|       - |  558 | `/*` |
|       - |  559 | ` * Apply a trait to a class: copy all methods and attributes from the trait` |
|       - |  560 | ` * into the target class. Unlike inheritance, traits copy ALL members including` |
|       - |  561 | ` * private ones. Members already defined in the class take precedence.` |
|       - |  562 | ` */` |
|      58 |  563 | `PH7_PRIVATE sxi32 PH7_ClassUseTrait(ph7_gen_state *pGen,ph7_class *pClass,ph7_class *pTrait)` |
|       5 |  564 | `{` |
|       - |  565 | `	ph7_class_method *pMeth;` |
|       - |  566 | `	ph7_class_attr *pAttr;` |
|       - |  567 | `	SyHashEntry *pEntry;` |
|       - |  568 | `	SyString *pName;` |
|       - |  569 | `	sxi32 rc;` |
|       - |  570 | `	/* Detect cyclic trait composition (e.g. trait A { use B; } trait B { use A; }) */` |
|      63 |  571 | `	if( pTrait->iFlags & PH7_CLASS_TRAIT_VISITING ){` |
|     ! 0 |  572 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|     ! 0 |  573 | `			"Trait circular reference detected: %z is already being applied",&pTrait->sName);` |
|     ! 0 |  574 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  575 | `			return SXERR_ABORT;` |
|       - |  576 | `		}` |
|     ! 0 |  577 | `		return SXRET_OK;` |
|       - |  578 | `	}` |
|      63 |  579 | `	pTrait->iFlags \|= PH7_CLASS_TRAIT_VISITING;` |
|      63 |  580 | `	rc = SXRET_OK;` |
|       - |  581 | `	/* Copy attributes from the trait */` |
|      63 |  582 | `	SyHashResetLoopCursor(&pTrait->hAttr);` |
|      81 |  583 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hAttr)) != 0 ){` |
|       - |  584 | `		SyHashEntry *pExisting;` |
|      22 |  585 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      22 |  586 | `		pName = &pAttr->sName;` |
|      22 |  587 | `		pExisting = SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte);` |
|      22 |  588 | `		if( pExisting != 0 ){` |
|       - |  589 | `			/* Attribute already exists. Check if it came from another trait` |
|       - |  590 | `			 * and whether the definitions are compatible (same defaults).` |
|       - |  591 | `			 */` |
|       - |  592 | `			ph7_class **apUsedTraits;` |
|       - |  593 | `			sxu32 nUsed,k;` |
|       6 |  594 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       6 |  595 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|       6 |  596 | `			for(k = 0; k < nUsed; k++){` |
|       - |  597 | `				ph7_class_attr *pOther;` |
|       3 |  598 | `				pOther = PH7_ClassExtractAttribute(apUsedTraits[k],pName->zString,pName->nByte);` |
|       3 |  599 | `				if( pOther ){` |
|       - |  600 | `					/* Two traits define the same property — check if defaults differ */` |
|       3 |  601 | `					ph7_class_attr *pClassAttr = (ph7_class_attr *)pExisting->pUserData;` |
|       4 |  602 | `					if( SySetUsed(&pAttr->aByteCode) != SySetUsed(&pClassAttr->aByteCode) \|\|` |
|       3 |  603 | `						(SySetUsed(&pAttr->aByteCode) > 0 &&` |
|       3 |  604 | `						 SyMemcmp(SySetBasePtr(&pAttr->aByteCode),SySetBasePtr(&pClassAttr->aByteCode),` |
|       3 |  605 | `							SySetUsed(&pAttr->aByteCode) * SySetElemSize(&pAttr->aByteCode)) != 0) ){` |
|       4 |  606 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine,` |
|       - |  607 | `							"%z and %z define the same property ($%z) in the composition of %z. "` |
|       - |  608 | `							"However, the definition differs and is considered incompatible",` |
|       2 |  609 | `							&apUsedTraits[k]->sName,&pTrait->sName,pName,&pClass->sName);` |
|       3 |  610 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  611 | `							goto cleanup;` |
|       - |  612 | `						}` |
|       1 |  613 | `					}` |
|       3 |  614 | `					break;` |
|       - |  615 | `				}` |
|     ! 0 |  616 | `			}` |
|       6 |  617 | `			continue;` |
|       - |  618 | `		}` |
|      18 |  619 | `		rc = SyHashInsert(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|      18 |  620 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  621 | `			goto cleanup;` |
|       - |  622 | `		}` |
|       4 |  623 | `	}` |
|       - |  624 | `	/* Copy methods from the trait */` |
|      63 |  625 | `	SyHashResetLoopCursor(&pTrait->hMethod);` |
|     117 |  626 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hMethod)) != 0 ){` |
|      59 |  627 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      59 |  628 | `		pName = &pMeth->sFunc.sName;` |
|      59 |  629 | `		if( SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte) != 0 ){` |
|       - |  630 | `			/* Method already exists in the class. Check if it came from another trait` |
|       - |  631 | `			 * (unresolved conflict) vs being defined by the class itself.` |
|       - |  632 | `			 */` |
|       - |  633 | `			ph7_class **apUsedTraits;` |
|       - |  634 | `			sxu32 nUsed,k;` |
|      11 |  635 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|      11 |  636 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|      11 |  637 | `			for(k = 0; k < nUsed; k++){` |
|       3 |  638 | `				if( PH7_ClassExtractMethod(apUsedTraits[k],pName->zString,pName->nByte) != 0 ){` |
|       - |  639 | `					/* Two different traits define the same method with no resolution */` |
|       4 |  640 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|       - |  641 | `						"Trait method %z::%z has not been applied as %z::%z, "` |
|       - |  642 | `						"because of collision with %z::%z",` |
|       2 |  643 | `						&pTrait->sName,pName,` |
|       1 |  644 | `						&pClass->sName,pName,` |
|       2 |  645 | `						&apUsedTraits[k]->sName,pName);` |
|       3 |  646 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  647 | `						goto cleanup;` |
|       - |  648 | `					}` |
|       3 |  649 | `					break;` |
|       - |  650 | `				}` |
|     ! 0 |  651 | `			}` |
|       - |  652 | `			/* Class-defined method takes precedence */` |
|      11 |  653 | `			continue;` |
|       - |  654 | `		}` |
|      51 |  655 | `		rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|      51 |  656 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  657 | `			goto cleanup;` |
|       - |  658 | `		}` |
|       5 |  659 | `	}` |
|       - |  660 | `	/* Record trait in the class */` |
|      63 |  661 | `	SySetPut(&pClass->aTrait,(const void *)&pTrait);` |
|      29 |  662 | `cleanup:` |
|       - |  663 | `	/* Always clear visiting flag, even on error paths */` |
|      63 |  664 | `	pTrait->iFlags &= ~PH7_CLASS_TRAIT_VISITING;` |
|      29 |  665 | `	SXUNUSED(pGen);` |
|      63 |  666 | `	return rc;` |
|      34 |  667 | `}` |
|       - |  668 | `/*` |
|       - |  669 | ` * Inherit an object interface from another object interface.` |
|       - |  670 | ` * According to the PHP language reference manual.` |
|       - |  671 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|       - |  672 | ` *  must implement, without having to define how these methods are handled.` |
|       - |  673 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  674 | ` *  class, but without any of the methods having their contents defined.` |
|       - |  675 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  676 | ` *` |
|       - |  677 | ` * This function return SXRET_OK if the interface inheritance operation was successfully performed.` |
|       - |  678 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|       - |  679 | ` * error message.` |
|       - |  680 | ` */` |
|   15498 |  681 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceInherit(ph7_class *pSub,ph7_class *pBase)` |
|       5 |  682 | `{` |
|       - |  683 | `	ph7_class_method *pMeth;` |
|       - |  684 | `	ph7_class_attr *pAttr;` |
|       - |  685 | `	SyHashEntry *pEntry;` |
|       - |  686 | `	SyString *pName;` |
|       - |  687 | `	sxi32 rc;` |
|       - |  688 | `	/* Install in the derived hashtable */` |
|   15503 |  689 | `	SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|   15503 |  690 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
|       - |  691 | `	/* Copy constants */` |
|   23254 |  692 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|       - |  693 | `		/* Make sure the constants are not redeclared in the subclass */` |
|       3 |  694 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|       3 |  695 | `		pName = &pAttr->sName;` |
|       3 |  696 | `		if( SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte) == 0 ){` |
|       - |  697 | `			/* Install the constant in the subclass */` |
|       3 |  698 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|       3 |  699 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  700 | `				return rc;` |
|       - |  701 | `			}` |
|       1 |  702 | `		}` |
|       1 |  703 | `	}` |
|   15503 |  704 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
|       - |  705 | `	/* Copy methods signature */` |
|   31038 |  706 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|       - |  707 | `		/* Make sure the method are not redeclared in the subclass */` |
|    7791 |  708 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|    7791 |  709 | `		pName = &pMeth->sFunc.sName;` |
|    7791 |  710 | `		if( SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte) == 0 ){` |
|       - |  711 | `			/* Install the method */` |
|    7791 |  712 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|    7791 |  713 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  714 | `				return rc;` |
|       - |  715 | `			}` |
|    3893 |  716 | `		}` |
|       5 |  717 | `	}` |
|       - |  718 | `	/* Mark as subclass */` |
|   15503 |  719 | `	pSub->pBase = pBase;` |
|       - |  720 | `	/* All done */` |
|   15503 |  721 | `	return SXRET_OK;` |
|    7754 |  722 | `}` |
|       - |  723 | `/*` |
|       - |  724 | ` * Implements an object interface in the given main class.` |
|       - |  725 | ` * According to the PHP language reference manual.` |
|       - |  726 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|       - |  727 | ` *  must implement, without having to define how these methods are handled.` |
|       - |  728 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  729 | ` *  class, but without any of the methods having their contents defined.` |
|       - |  730 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  731 | ` *` |
|       - |  732 | ` * This function return SXRET_OK if the interface was successfully implemented.` |
|       - |  733 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|       - |  734 | ` * error message.` |
|       - |  735 | ` */` |
|  221036 |  736 | `PH7_PRIVATE sxi32 PH7_ClassImplement(ph7_class *pMain,ph7_class *pInterface)` |
|       5 |  737 | `{` |
|       - |  738 | `	ph7_class_attr *pAttr;` |
|       - |  739 | `	SyHashEntry *pEntry;` |
|       - |  740 | `	SyString *pName;` |
|       - |  741 | `	sxi32 rc;` |
|       - |  742 | `	/* First off,copy all constants declared inside the interface */` |
|  221041 |  743 | `	SyHashResetLoopCursor(&pInterface->hAttr);` |
|  331573 |  744 | `	while((pEntry = SyHashGetNextEntry(&pInterface->hAttr)) != 0 ){` |
|       - |  745 | `		/* Point to the constant declaration */` |
|      15 |  746 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      15 |  747 | `		pName = &pAttr->sName;` |
|       - |  748 | `		/* Make sure the attribute is not redeclared in the main class */` |
|      15 |  749 | `		if( SyHashGet(&pMain->hAttr,pName->zString,pName->nByte) == 0 ){` |
|       - |  750 | `			/* Install the attribute */` |
|      11 |  751 | `			rc = SyHashInsert(&pMain->hAttr,pName->zString,pName->nByte,pAttr);` |
|      11 |  752 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  753 | `				return rc;` |
|       - |  754 | `			}` |
|       5 |  755 | `		}` |
|       1 |  756 | `	}` |
|       - |  757 | `	/* Install in the interface container */` |
|  221041 |  758 | `	SySetPut(&pMain->aInterface,(const void *)&pInterface);` |
|       - |  759 | `	/* Install interface method stubs into the implementing class.` |
|       - |  760 | `	 * Methods already defined in the class take precedence (they satisfy` |
|       - |  761 | `	 * the interface contract). Stubs retain PH7_CLASS_ATTR_ABSTRACT so` |
|       - |  762 | `	 * the unified check in GenStateCheckAbstractMethods catches missing ones.` |
|       - |  763 | `	 */` |
|       - |  764 | `	{` |
|       - |  765 | `		ph7_class_method *pMeth;` |
|       - |  766 | `		SyHashEntry *pMEntry;` |
|       - |  767 | `		SyString *pMName;` |
|  221041 |  768 | `		SyHashResetLoopCursor(&pInterface->hMethod);` |
|  630283 |  769 | `		while((pMEntry = SyHashGetNextEntry(&pInterface->hMethod)) != 0 ){` |
|  298729 |  770 | `			pMeth = (ph7_class_method *)pMEntry->pUserData;` |
|  298729 |  771 | `			pMName = &pMeth->sFunc.sName;` |
|  298729 |  772 | `			if( SyHashGet(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte) == 0 ){` |
|      23 |  773 | `				rc = SyHashInsert(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte,pMeth);` |
|      23 |  774 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  775 | `					return rc;` |
|       - |  776 | `				}` |
|       9 |  777 | `			}` |
|       5 |  778 | `		}` |
|       - |  779 | `	}` |
|  221041 |  780 | `	return SXRET_OK;` |
|  110523 |  781 | `}` |
|       - |  782 | `/*` |
|       - |  783 | ` * Create a class instance [i.e: Object in the PHP jargon] at run-time.` |
|       - |  784 | ` * The following function is called when an object is created at run-time` |
|       - |  785 | ` * typically when the PH7_OP_NEW/PH7_OP_CLONE instructions are executed.` |
|       - |  786 | ` * Notes on object creation.` |
|       - |  787 | ` *` |
|       - |  788 | ` * According to PHP language reference manual.` |
|       - |  789 | ` * To create an instance of a class, the new keyword must be used. An object will always` |
|       - |  790 | ` * be created unless the object has a constructor defined that throws an exception on error.` |
|       - |  791 | ` * Classes should be defined before instantiation (and in some cases this is a requirement).` |
|       - |  792 | ` * If a string containing the name of a class is used with new, a new instance of that class` |
|       - |  793 | ` * will be created. If the class is in a namespace, its fully qualified name must be used when` |
|       - |  794 | ` * doing this.` |
|       - |  795 | ` * Example #3 Creating an instance` |
|       - |  796 | ` * <?php` |
|       - |  797 | ` *  $instance = new SimpleClass();` |
|       - |  798 | ` *   // This can also be done with a variable:` |
|       - |  799 | ` * $className = 'Foo';` |
|       - |  800 | ` * $instance = new $className(); // Foo()` |
|       - |  801 | ` * ?>` |
|       - |  802 | ` * In the class context, it is possible to create a new object by new self and new parent.` |
|       - |  803 | ` * When assigning an already created instance of a class to a new variable, the new variable` |
|       - |  804 | ` * will access the same instance as the object that was assigned. This behaviour is the same` |
|       - |  805 | ` * when passing instances to a function. A copy of an already created object can be made by` |
|       - |  806 | ` * cloning it.` |
|       - |  807 | ` * Example #4 Object Assignment` |
|       - |  808 | ` * <?php` |
|       - |  809 | ` *  class SimpleClass(){` |
|       - |  810 | ` *    public $var;` |
|       - |  811 | ` *  };` |
|       - |  812 | ` *  $instance = new SimpleClass();` |
|       - |  813 | ` *  $assigned   =  $instance;` |
|       - |  814 | ` *  $reference  =& $instance;` |
|       - |  815 | ` *  $instance->var = '$assigned will have this value';` |
|       - |  816 | ` *  $instance = null; // $instance and $reference become null` |
|       - |  817 | ` *  var_dump($instance);` |
|       - |  818 | ` *  var_dump($reference);` |
|       - |  819 | ` *  var_dump($assigned);` |
|       - |  820 | ` * ?>` |
|       - |  821 | ` * The above example will output:` |
|       - |  822 | ` * NULL` |
|       - |  823 | ` * NULL` |
|       - |  824 | ` * object(SimpleClass)#1 (1) {` |
|       - |  825 | ` *  ["var"]=>` |
|       - |  826 | ` *    string(30) "$assigned will have this value"` |
|       - |  827 | ` * }` |
|       - |  828 | ` * Example #5 Creating new objects` |
|       - |  829 | ` * <?php` |
|       - |  830 | ` * class Test` |
|       - |  831 | ` * {` |
|       - |  832 | ` *   static public function getNew()` |
|       - |  833 | ` *   {` |
|       - |  834 | ` *       return new static;` |
|       - |  835 | ` *   }` |
|       - |  836 | ` * }` |
|       - |  837 | ` * class Child extends Test` |
|       - |  838 | ` * {}` |
|       - |  839 | ` * $obj1 = new Test();` |
|       - |  840 | ` * $obj2 = new $obj1;` |
|       - |  841 | ` * var_dump($obj1 !== $obj2);` |
|       - |  842 | ` * $obj3 = Test::getNew();` |
|       - |  843 | ` * var_dump($obj3 instanceof Test);` |
|       - |  844 | ` * $obj4 = Child::getNew();` |
|       - |  845 | ` * var_dump($obj4 instanceof Child);` |
|       - |  846 | ` * ?>` |
|       - |  847 | ` * The above example will output:` |
|       - |  848 | ` * bool(true)` |
|       - |  849 | ` * bool(true)` |
|       - |  850 | ` * bool(true)` |
|       - |  851 | ` * Note that Symisc Systems have introduced powerfull extension to` |
|       - |  852 | ` * OO subsystem. For example a class attribute may have any complex` |
|       - |  853 | ` * expression associated with it when declaring the attribute unlike` |
|       - |  854 | ` * the standard PHP engine which would allow a single value.` |
|       - |  855 | ` * Example:` |
|       - |  856 | ` *  class myClass{` |
|       - |  857 | ` *    public $var = 25<<1+foo()/bar();` |
|       - |  858 | ` *  };` |
|       - |  859 | ` * Refer to the official documentation for more information.` |
|       - |  860 | ` */` |
|    6502 |  861 | `static ph7_class_instance * NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|       5 |  862 | `{` |
|       - |  863 | `	ph7_class_instance *pThis;` |
|       - |  864 | `	/* Allocate a new instance */` |
|    6507 |  865 | `	pThis = (ph7_class_instance *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_instance));` |
|    6507 |  866 | `	if( pThis == 0 ){` |
|     ! 0 |  867 | `		return 0;` |
|       - |  868 | `	}` |
|       - |  869 | `	/* Zero the structure */` |
|    6507 |  870 | `	SyZero(pThis,sizeof(ph7_class_instance));` |
|       - |  871 | `	/* Initialize fields */` |
|    6507 |  872 | `	pThis->iRef = 1;` |
|    6507 |  873 | `	pThis->pVm = pVm;` |
|    6507 |  874 | `	pThis->pClass = pClass;` |
|       - |  875 | `	/* Assign a fresh monotonic object handle id (clones get their own, like PHP). */` |
|    6507 |  876 | `	pThis->nObjId = pVm->nNextObjId++;` |
|    6507 |  877 | `	SyHashInit(&pThis->hAttr,&pVm->sAllocator,0,0);` |
|    6507 |  878 | `	return pThis;` |
|    3256 |  879 | `}` |
|       - |  880 | `/*` |
|       - |  881 | ` * Wrapper around the NewClassInstance() function defined above.` |
|       - |  882 | ` * See the block comment above for more information.` |
|       - |  883 | ` */` |
|    6382 |  884 | `PH7_PRIVATE ph7_class_instance * PH7_NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|       5 |  885 | `{` |
|       - |  886 | `	ph7_class_instance *pNew;` |
|       - |  887 | `	sxi32 rc;` |
|    6387 |  888 | `	pNew = NewClassInstance(&(*pVm),&(*pClass));` |
|    6387 |  889 | `	if( pNew == 0 ){` |
|     ! 0 |  890 | `		return 0;` |
|       - |  891 | `	}` |
|       - |  892 | `	/* Associate a private VM frame with this class instance */` |
|    6387 |  893 | `	rc = PH7_VmCreateClassInstanceFrame(&(*pVm),pNew);` |
|    6387 |  894 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  895 | `		SyMemBackendPoolFree(&pVm->sAllocator,pNew);` |
|     ! 0 |  896 | `		return 0;` |
|       - |  897 | `	}` |
|    6387 |  898 | `	return pNew;` |
|    3196 |  899 | `}` |
|       - |  900 | `/*` |
|       - |  901 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon] attribute.` |
|       - |  902 | ` * This function never fail.` |
|       - |  903 | ` */` |
|   10744 |  904 | `static ph7_value * ExtractClassAttrValue(ph7_vm *pVm,VmClassAttr *pAttr)` |
|       5 |  905 | `{` |
|       - |  906 | `	/* Extract the value */` |
|       - |  907 | `	ph7_value *pValue;` |
|   10749 |  908 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|   10749 |  909 | `	return pValue;` |
|       5 |  910 | `}` |
|       - |  911 | `/*` |
|       - |  912 | ` * Perform a clone operation on a class instance [i.e: Object in the PHP jargon].` |
|       - |  913 | ` * The following function is called when an object is cloned at run-time` |
|       - |  914 | ` * typically when the PH7_OP_CLONE instruction is executed.` |
|       - |  915 | ` * Notes on object cloning.` |
|       - |  916 | ` *` |
|       - |  917 | ` * According to PHP language reference manual.` |
|       - |  918 | ` * Creating a copy of an object with fully replicated properties is not always the wanted behavior.` |
|       - |  919 | ` * A good example of the need for copy constructors. Another example is if your object holds a reference` |
|       - |  920 | ` * to another object which it uses and when you replicate the parent object you want to create` |
|       - |  921 | ` * a new instance of this other object so that the replica has its own separate copy.` |
|       - |  922 | ` * An object copy is created by using the clone keyword (which calls the object's __clone() method if possible).` |
|       - |  923 | ` * An object's __clone() method cannot be called directly.` |
|       - |  924 | ` * $copy_of_object = clone $object;` |
|       - |  925 | ` * When an object is cloned, PHP 5 will perform a shallow copy of all of the object's properties.` |
|       - |  926 | ` * Any properties that are references to other variables, will remain references.` |
|       - |  927 | ` * Once the cloning is complete, if a __clone() method is defined, then the newly created object's __clone() method` |
|       - |  928 | ` * will be called, to allow any necessary properties that need to be changed.` |
|       - |  929 | ` * Example #1 Cloning an object` |
|       - |  930 | ` * <?php` |
|       - |  931 | ` * class SubObject` |
|       - |  932 | ` * {` |
|       - |  933 | ` *   static $instances = 0;` |
|       - |  934 | ` *   public $instance;` |
|       - |  935 | ` *` |
|       - |  936 | ` *   public function __construct() {` |
|       - |  937 | ` *       $this->instance = ++self::$instances;` |
|       - |  938 | ` *   }` |
|       - |  939 | ` *` |
|       - |  940 | ` *   public function __clone() {` |
|       - |  941 | ` *       $this->instance = ++self::$instances;` |
|       - |  942 | ` *   }` |
|       - |  943 | ` * }` |
|       - |  944 | ` *` |
|       - |  945 | ` * class MyCloneable` |
|       - |  946 | ` * {` |
|       - |  947 | ` *   public $object1;` |
|       - |  948 | ` *   public $object2;` |
|       - |  949 | ` *` |
|       - |  950 | ` *   function __clone()` |
|       - |  951 | ` *   {` |
|       - |  952 | ` *       // Force a copy of this->object, otherwise` |
|       - |  953 | ` *       // it will point to same object.` |
|       - |  954 | ` *       $this->object1 = clone $this->object1;` |
|       - |  955 | ` *   }` |
|       - |  956 | ` * }` |
|       - |  957 | ` * $obj = new MyCloneable();` |
|       - |  958 | ` * $obj->object1 = new SubObject();` |
|       - |  959 | ` * $obj->object2 = new SubObject();` |
|       - |  960 | ` * $obj2 = clone $obj;` |
|       - |  961 | ` * print("Original Object:\n");` |
|       - |  962 | ` * print_r($obj);` |
|       - |  963 | ` * print("Cloned Object:\n");` |
|       - |  964 | ` * print_r($obj2);` |
|       - |  965 | ` * ?>` |
|       - |  966 | ` * The above example will output:` |
|       - |  967 | ` * Original Object:` |
|       - |  968 | ` * MyCloneable Object` |
|       - |  969 | ` * (` |
|       - |  970 | ` *   [object1] => SubObject Object` |
|       - |  971 | ` *       (` |
|       - |  972 | ` *           [instance] => 1` |
|       - |  973 | ` *       )` |
|       - |  974 | ` *` |
|       - |  975 | ` *   [object2] => SubObject Object` |
|       - |  976 | ` *       (` |
|       - |  977 | ` *           [instance] => 2` |
|       - |  978 | ` *       )` |
|       - |  979 | ` *` |
|       - |  980 | ` * )` |
|       - |  981 | ` * Cloned Object:` |
|       - |  982 | ` * MyCloneable Object` |
|       - |  983 | ` * (` |
|       - |  984 | ` *   [object1] => SubObject Object` |
|       - |  985 | ` *       (` |
|       - |  986 | ` *           [instance] => 3` |
|       - |  987 | ` *       )` |
|       - |  988 | ` *` |
|       - |  989 | ` *   [object2] => SubObject Object` |
|       - |  990 | ` *       (` |
|       - |  991 | ` *           [instance] => 2` |
|       - |  992 | ` *       )` |
|       - |  993 | ` * )` |
|       - |  994 | ` */` |
|     120 |  995 | `PH7_PRIVATE ph7_class_instance * PH7_CloneClassInstance(ph7_class_instance *pSrc)` |
|       3 |  996 | `{` |
|       - |  997 | `	ph7_class_instance *pClone;` |
|       - |  998 | `	ph7_class_method *pMethod;` |
|       - |  999 | `	SyHashEntry *pEntry2;` |
|       - | 1000 | `	SyHashEntry *pEntry;` |
|       - | 1001 | `	ph7_vm *pVm;` |
|       - | 1002 | `	sxi32 rc;` |
|       - | 1003 | `	/* Allocate a new instance */` |
|     123 | 1004 | `	pVm = pSrc->pVm;` |
|     123 | 1005 | `	pClone = NewClassInstance(pVm,pSrc->pClass);` |
|     123 | 1006 | `	if( pClone == 0 ){` |
|     ! 0 | 1007 | `		return 0;` |
|       - | 1008 | `	}` |
|       - | 1009 | `	/* Associate a private VM frame with this class instance */` |
|     123 | 1010 | `	rc = PH7_VmCreateClassInstanceFrame(pVm,pClone);` |
|     123 | 1011 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1012 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClone);` |
|     ! 0 | 1013 | `		return 0;` |
|       - | 1014 | `	}` |
|       - | 1015 | `	/* Duplicate object values. Iterate the SOURCE attributes and copy each into` |
|       - | 1016 | `	 * the clone's same-named slot (looked up by name, so order/count differences` |
|       - | 1017 | `	 * from dynamic properties don't matter). A dynamic (runtime-added) property` |
|       - | 1018 | `	 * has no declared counterpart in the clone, so synthesize it first — without` |
|       - | 1019 | `	 * this, a clone of a stdClass would silently lose all its dynamic properties. */` |
|     123 | 1020 | `	SyHashResetLoopCursor(&pSrc->hAttr);` |
|     341 | 1021 | `	while((pEntry = SyHashGetNextEntry(&pSrc->hAttr)) != 0 ){` |
|     221 | 1022 | `		VmClassAttr *pSrcAttr = (VmClassAttr *)pEntry->pUserData;` |
|     221 | 1023 | `		VmClassAttr *pDestAttr = 0;` |
|     221 | 1024 | `		ph7_value *pvSrc,*pvDest = 0;` |
|       - | 1025 | `		/* Duplicate non-static attribute */` |
|     221 | 1026 | `		if( pSrcAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|     ! 0 | 1027 | `			continue;` |
|       - | 1028 | `		}` |
|     221 | 1029 | `		pEntry2 = SyHashGet(&pClone->hAttr,SyStringData(&pSrcAttr->pAttr->sName),SyStringLength(&pSrcAttr->pAttr->sName));` |
|     221 | 1030 | `		if( pEntry2 ){` |
|     207 | 1031 | `			pDestAttr = (VmClassAttr *)pEntry2->pUserData;` |
|     207 | 1032 | `			pvDest = ExtractClassAttrValue(pVm,pDestAttr);` |
|     117 | 1033 | `		}else if( pSrcAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){` |
|       - | 1034 | `			/* Dynamic property: synthesize the matching slot on the clone. */` |
|      22 | 1035 | `			pvDest = PH7_VmCreateDynamicAttr(pVm,pClone,` |
|      14 | 1036 | `				SyStringData(&pSrcAttr->pAttr->sName),SyStringLength(&pSrcAttr->pAttr->sName),&pDestAttr);` |
|       7 | 1037 | `		}` |
|       - | 1038 | `		/* Fetch the source value LAST: PH7_VmCreateDynamicAttr above may have` |
|       - | 1039 | `		 * reserved a slot and reallocated pVm->aMemObj, which would dangle any` |
|       - | 1040 | `		 * ph7_value* obtained before it. pvDest from the synth path already points` |
|       - | 1041 | `		 * into the post-realloc aMemObj; resolve pvSrc now so both are current. */` |
|     221 | 1042 | `		pvSrc = ExtractClassAttrValue(pVm,pSrcAttr);` |
|     221 | 1043 | `		if( pvSrc && pvDest ){` |
|     221 | 1044 | `			PH7_MemObjStore(pvSrc,pvDest);` |
|     109 | 1045 | `		}` |
|       - | 1046 | `		/* Carry over the per-instance state so the clone matches the source:` |
|       - | 1047 | `		 * VM_CLASS_ATTR_UNINIT marks a typed property as not-yet-initialized` |
|       - | 1048 | `		 * and doubles as the readonly write-once latch — without this a clone` |
|       - | 1049 | `		 * would reset to uninitialized (losing the value's readiness) and a` |
|       - | 1050 | `		 * readonly property would become writable again. */` |
|     221 | 1051 | `		if( pDestAttr ){` |
|     221 | 1052 | `			pDestAttr->iState = pSrcAttr->iState;` |
|     109 | 1053 | `		}` |
|       3 | 1054 | `	}` |
|       - | 1055 | `	/* A declared property unset() on the source is absent from the clone too (PHP). But the clone` |
|       - | 1056 | `	 * frame above materialized ALL declared attrs (with their defaults), so drop any clone attr whose` |
|       - | 1057 | `	 * name is not present on the source. Collect first, then delete — removing an entry mid-walk would` |
|       - | 1058 | `	 * free the node the SyHash loop cursor points at. */` |
|       - | 1059 | `	{` |
|       - | 1060 | `		SySet sDrop;` |
|     123 | 1061 | `		SySetInit(&sDrop,&pVm->sAllocator,sizeof(VmClassAttr *));` |
|     123 | 1062 | `		SyHashResetLoopCursor(&pClone->hAttr);` |
|     343 | 1063 | `		while((pEntry = SyHashGetNextEntry(&pClone->hAttr)) != 0 ){` |
|     223 | 1064 | `			VmClassAttr *pCloneAttr = (VmClassAttr *)pEntry->pUserData;` |
|     223 | 1065 | `			if( pCloneAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|     ! 0 | 1066 | `				continue;` |
|       - | 1067 | `			}` |
|     330 | 1068 | `			if( SyHashGet(&pSrc->hAttr,SyStringData(&pCloneAttr->pAttr->sName),` |
|     333 | 1069 | `					SyStringLength(&pCloneAttr->pAttr->sName)) == 0 ){` |
|       3 | 1070 | `				SySetPut(&sDrop,(const void *)&pCloneAttr);` |
|       1 | 1071 | `			}` |
|       3 | 1072 | `		}` |
|     123 | 1073 | `		if( SySetUsed(&sDrop) > 0 ){` |
|       3 | 1074 | `			VmClassAttr **apDrop = (VmClassAttr **)SySetBasePtr(&sDrop);` |
|       - | 1075 | `			sxu32 i;` |
|       5 | 1076 | `			for( i = 0 ; i < SySetUsed(&sDrop) ; ++i ){` |
|       3 | 1077 | `				VmClassAttr *pVmAttr = apDrop[i];` |
|       4 | 1078 | `				SyHashDeleteEntry(&pClone->hAttr,SyStringData(&pVmAttr->pAttr->sName),` |
|       2 | 1079 | `					SyStringLength(&pVmAttr->pAttr->sName),0);` |
|       3 | 1080 | `				PH7_VmReleaseInstanceAttr(pVm,pVmAttr);` |
|       2 | 1081 | `			}` |
|       1 | 1082 | `		}` |
|     123 | 1083 | `		SySetRelease(&sDrop);` |
|       - | 1084 | `	}` |
|       - | 1085 | `	/* call the __clone method on the cloned object if available */` |
|     123 | 1086 | `	pMethod = PH7_ClassExtractMethod(pClone->pClass,"__clone",sizeof("__clone")-1);` |
|     123 | 1087 | `	if( pMethod ){` |
|      56 | 1088 | `		if( pMethod->iCloneDepth < 16 ){` |
|      54 | 1089 | `			pMethod->iCloneDepth++;` |
|      54 | 1090 | `			PH7_VmCallClassMethod(pVm,pClone,pMethod,0,0,0);` |
|      28 | 1091 | `		}else{` |
|       - | 1092 | `			/* Nesting limit reached */` |
|       3 | 1093 | `			PH7_VmThrowError(pVm,0,PH7_CTX_ERR,"Object clone limit reached,no more call to __clone()");` |
|       - | 1094 | `		}` |
|       - | 1095 | `		/* Reset the cursor */` |
|      56 | 1096 | `		pMethod->iCloneDepth = 0;` |
|      27 | 1097 | `	}` |
|       - | 1098 | `	/* Return the cloned object */` |
|     123 | 1099 | `	return pClone;` |
|      63 | 1100 | `}` |
|       - | 1101 | `#define CLASS_INSTANCE_DESTROYED 0x001 /* Instance is released */` |
|       - | 1102 | `/*` |
|       - | 1103 | ` * Free the per-instance allocations owned by ONE object attribute: its value slot (+ the typed-slot` |
|       - | 1104 | ` * enforcement entry), the synthesized ph7_class_attr for a dynamic (runtime-added) property, and the` |
|       - | 1105 | ` * VmClassAttr wrapper itself. Does NOT touch the hAttr entry node — the caller removes it` |
|       - | 1106 | `` * (`unset($o->p)` via SyHashDeleteEntry2; instance teardown via the wholesale SyHashRelease, so it must`` |
|       - | 1107 | ` * not delete entries mid-walk). Shared by PH7_ClassInstanceRelease and the OP_MEMBER unset path.` |
|       - | 1108 | ` */` |
|   17732 | 1109 | `PH7_PRIVATE void PH7_VmReleaseInstanceAttr(ph7_vm *pVm, VmClassAttr *pVmAttr)` |
|       5 | 1110 | `{` |
|   17737 | 1111 | `	if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|       - | 1112 | `		/* Drop any typed-property enforcement slot registered for this memobj, before the memobj` |
|       - | 1113 | `		 * is returned to the free list, so a future recycled slot does not inherit the stale entry. */` |
|   15731 | 1114 | `		if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|     282 | 1115 | `			SyHashDeleteEntry(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),0);` |
|     139 | 1116 | `		}` |
|   15731 | 1117 | `		PH7_VmUnsetMemObj(pVm,pVmAttr->nIdx,TRUE);` |
|    7863 | 1118 | `	}` |
|       - | 1119 | `	/* A dynamic property owns its synthesized ph7_class_attr (struct + inline name in one block) —` |
|       - | 1120 | `	 * free it here (the only place a per-instance pAttr is freed; declared attrs are class-owned). */` |
|   17737 | 1121 | `	if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){` |
|     111 | 1122 | `		SyMemBackendFree(&pVm->sAllocator,pVmAttr->pAttr);` |
|      55 | 1123 | `	}` |
|   17737 | 1124 | `	SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|   17737 | 1125 | `}` |
|       - | 1126 | `/*` |
|       - | 1127 | ` * Release a class instance [i.e: Object in the PHP jargon] and invoke any defined destructor.` |
|       - | 1128 | ` * This routine is invoked as soon as there are no other references to a particular` |
|       - | 1129 | ` * class instance.` |
|       - | 1130 | ` */` |
|    4272 | 1131 | `static void PH7_ClassInstanceRelease(ph7_class_instance *pThis)` |
|       5 | 1132 | `{` |
|       - | 1133 | `	ph7_class_method *pDestr;` |
|       - | 1134 | `	SyHashEntry *pEntry;` |
|       - | 1135 | `	ph7_class *pClass;` |
|       - | 1136 | `	ph7_vm *pVm;` |
|    4277 | 1137 | `	if( pThis->iFlags & CLASS_INSTANCE_DESTROYED ){` |
|       - | 1138 | `		/*` |
|       - | 1139 | `		 * Already destroyed,return immediately.` |
|       - | 1140 | `		 * This could happend if someone perform unset($this) in the destructor body.` |
|       - | 1141 | `		 */` |
|     ! 0 | 1142 | `		return;` |
|       - | 1143 | `	}` |
|       - | 1144 | `	/* Mark as destroyed */` |
|    4277 | 1145 | `	pThis->iFlags \|= CLASS_INSTANCE_DESTROYED;` |
|       - | 1146 | `	/* Invoke any defined destructor if available */` |
|    4277 | 1147 | `	pVm = pThis->pVm;` |
|    4277 | 1148 | `	pClass = pThis->pClass;` |
|    4277 | 1149 | `	pDestr = PH7_ClassExtractMethod(pClass,"__destruct",sizeof("__destruct")-1);` |
|    4277 | 1150 | `	if( pDestr && !pVm->bInReset ){` |
|       - | 1151 | `		/* Invoke the destructor. Skipped during ph7_vm_reset() bulk teardown:` |
|       - | 1152 | `		 * running user PHP against a half-reset VM is unsafe (see bInReset). */` |
|     439 | 1153 | `		pThis->iRef = 2; /* Prevent garbage collection */` |
|     439 | 1154 | `		PH7_VmCallClassMethod(pVm,pThis,pDestr,0,0,0);` |
|     217 | 1155 | `	}` |
|       - | 1156 | `	/* Release non-static attributes (the wholesale SyHashRelease below frees the entry nodes,` |
|       - | 1157 | `	 * so the helper must not delete them mid-walk). */` |
|    4277 | 1158 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|   21979 | 1159 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|   17707 | 1160 | `		PH7_VmReleaseInstanceAttr(pVm,(VmClassAttr *)pEntry->pUserData);` |
|       5 | 1161 | `	}` |
|       - | 1162 | `	/* Release the whole structure */` |
|    4277 | 1163 | `	SyHashRelease(&pThis->hAttr);` |
|    4277 | 1164 | `	SyMemBackendPoolFree(&pVm->sAllocator,pThis);` |
|    2141 | 1165 | `}` |
|       - | 1166 | `/*` |
|       - | 1167 | ` * Decrement the reference count of a class instance [i.e Object in the PHP jargon].` |
|       - | 1168 | ` * If the reference count reaches zero,release the whole instance.` |
|       - | 1169 | ` */` |
|   97536 | 1170 | `PH7_PRIVATE void PH7_ClassInstanceUnref(ph7_class_instance *pThis)` |
|       5 | 1171 | `{` |
|   97541 | 1172 | `	pThis->iRef--;` |
|   97541 | 1173 | `	if( pThis->iRef < 1 ){` |
|       - | 1174 | `		/* No more reference to this instance */` |
|    4277 | 1175 | `		PH7_ClassInstanceRelease(&(*pThis));` |
|    2136 | 1176 | `	}` |
|   97541 | 1177 | `}` |
|       - | 1178 | `/*` |
|       - | 1179 | ` * Compare two class instances [i.e: Objects in the PHP jargon]` |
|       - | 1180 | ` * Note on objects comparison:` |
|       - | 1181 | ` *  According to the PHP langauge reference manual` |
|       - | 1182 | ` *  When using the comparison operator (==), object variables are compared in a simple manner` |
|       - | 1183 | ` *  namely: Two object instances are equal if they have the same attributes and values, and are` |
|       - | 1184 | ` *  instances of the same class.` |
|       - | 1185 | ` *  On the other hand, when using the identity operator (===), object variables are identical` |
|       - | 1186 | ` *  if and only if they refer to the same instance of the same class.` |
|       - | 1187 | ` *  An example will clarify these rules.` |
|       - | 1188 | ` *  Example #1 Example of object comparison` |
|       - | 1189 | ` *  <?php` |
|       - | 1190 | ` *    function bool2str($bool)` |
|       - | 1191 | ` * {` |
|       - | 1192 | ` *   if ($bool === false) {` |
|       - | 1193 | ` *       return 'FALSE';` |
|       - | 1194 | ` *   } else {` |
|       - | 1195 | ` *       return 'TRUE';` |
|       - | 1196 | ` *   }` |
|       - | 1197 | ` * }` |
|       - | 1198 | ` * function compareObjects(&$o1, &$o2)` |
|       - | 1199 | ` * {` |
|       - | 1200 | ` *   echo 'o1 == o2 : ' . bool2str($o1 == $o2) . "\n";` |
|       - | 1201 | ` *   echo 'o1 != o2 : ' . bool2str($o1 != $o2) . "\n";` |
|       - | 1202 | ` *   echo 'o1 === o2 : ' . bool2str($o1 === $o2) . "\n";` |
|       - | 1203 | ` *   echo 'o1 !== o2 : ' . bool2str($o1 !== $o2) . "\n";` |
|       - | 1204 | ` * }` |
|       - | 1205 | ` * class Flag` |
|       - | 1206 | ` * {` |
|       - | 1207 | ` *   public $flag;` |
|       - | 1208 | ` *` |
|       - | 1209 | ` *   function Flag($flag = true) {` |
|       - | 1210 | ` *       $this->flag = $flag;` |
|       - | 1211 | ` *   }` |
|       - | 1212 | ` * }` |
|       - | 1213 | ` *` |
|       - | 1214 | ` * class OtherFlag` |
|       - | 1215 | ` * {` |
|       - | 1216 | ` *   public $flag;` |
|       - | 1217 | ` *` |
|       - | 1218 | ` *   function OtherFlag($flag = true) {` |
|       - | 1219 | ` *       $this->flag = $flag;` |
|       - | 1220 | ` *   }` |
|       - | 1221 | ` * }` |
|       - | 1222 | ` *` |
|       - | 1223 | ` * $o = new Flag();` |
|       - | 1224 | ` * $p = new Flag();` |
|       - | 1225 | ` * $q = $o;` |
|       - | 1226 | ` * $r = new OtherFlag();` |
|       - | 1227 | ` *` |
|       - | 1228 | ` * echo "Two instances of the same class\n";` |
|       - | 1229 | ` * compareObjects($o, $p);` |
|       - | 1230 | ` * echo "\nTwo references to the same instance\n";` |
|       - | 1231 | ` * compareObjects($o, $q);` |
|       - | 1232 | ` * echo "\nInstances of two different classes\n";` |
|       - | 1233 | ` * compareObjects($o, $r);` |
|       - | 1234 | ` * ?>` |
|       - | 1235 | ` * The above example will output:` |
|       - | 1236 | ` * Two instances of the same class` |
|       - | 1237 | ` * o1 == o2 : TRUE` |
|       - | 1238 | ` * o1 != o2 : FALSE` |
|       - | 1239 | ` * o1 === o2 : FALSE` |
|       - | 1240 | ` * o1 !== o2 : TRUE` |
|       - | 1241 | ` * Two references to the same instance` |
|       - | 1242 | ` * o1 == o2 : TRUE` |
|       - | 1243 | ` * o1 != o2 : FALSE` |
|       - | 1244 | ` * o1 === o2 : TRUE` |
|       - | 1245 | ` * o1 !== o2 : FALSE` |
|       - | 1246 | ` * Instances of two different classes` |
|       - | 1247 | ` * o1 == o2 : FALSE` |
|       - | 1248 | ` * o1 != o2 : TRUE` |
|       - | 1249 | ` * o1 === o2 : FALSE` |
|       - | 1250 | ` * o1 !== o2 : TRUE` |
|       - | 1251 | ` *` |
|       - | 1252 | ` * This function return 0 if the objects are equals according to the comprison rules defined above.` |
|       - | 1253 | ` * Any other return values indicates difference.` |
|       - | 1254 | ` */` |
|     252 | 1255 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest)` |
|       5 | 1256 | `{` |
|       - | 1257 | `	SyHashEntry *pEntry,*pEntry2;` |
|       - | 1258 | `	ph7_value sV1,sV2;` |
|       - | 1259 | `	sxi32 rc;` |
|     257 | 1260 | `	if( iNest > 31 ){` |
|       - | 1261 | `		/* Nesting limit reached */` |
|       6 | 1262 | `		PH7_VmThrowError(pLeft->pVm,0,PH7_CTX_ERR,"Nesting limit reached: Infinite recursion?");` |
|       6 | 1263 | `		return 1;` |
|       - | 1264 | `	}` |
|       - | 1265 | `	/* Comparison is performed only if the objects are instance of the same class */` |
|     253 | 1266 | `	if( pLeft->pClass != pRight->pClass ){` |
|       7 | 1267 | `		return 1;` |
|       - | 1268 | `	}` |
|     247 | 1269 | `	if( bStrict ){` |
|       - | 1270 | `		/*` |
|       - | 1271 | `		 * According to the PHP language reference manual:` |
|       - | 1272 | `		 *  when using the identity operator (===), object variables` |
|       - | 1273 | `		 *  are identical if and only if they refer to the same instance` |
|       - | 1274 | `		 *  of the same class.` |
|       - | 1275 | `		 */` |
|      95 | 1276 | `		return !(pLeft == pRight);` |
|       - | 1277 | `	}` |
|       - | 1278 | `	/*` |
|       - | 1279 | `	 * Attribute comparison.` |
|       - | 1280 | `	 * According to the PHP reference manual:` |
|       - | 1281 | `	 *  When using the comparison operator (==), object variables are compared` |
|       - | 1282 | `	 *  in a simple manner, namely: Two object instances are equal if they have` |
|       - | 1283 | `	 *  the same attributes and values, and are instances of the same class.` |
|       - | 1284 | `	 */` |
|     155 | 1285 | `	if( pLeft == pRight ){` |
|       - | 1286 | `		/* Same instance,don't bother processing,object are equals */` |
|       5 | 1287 | `		return 0;` |
|       - | 1288 | `	}` |
|       - | 1289 | `	/* Closures compare by IDENTITY under == as well (not by attributes): two distinct` |
|       - | 1290 | `	 * Closure instances are never equal, even when they wrap the same underlying function` |
|       - | 1291 | `	 * (PHP semantics). pLeft != pRight here, so a Closure pair is unequal. Without this,` |
|       - | 1292 | `` 	 * two capture-less lambdas of the same `function(){}` share the template's `$__fn` `` |
|       - | 1293 | `	 * name and would compare equal. */` |
|     151 | 1294 | `	if( pLeft->pVm->pClosureClass && pLeft->pClass == pLeft->pVm->pClosureClass ){` |
|       5 | 1295 | `		return 1;` |
|       - | 1296 | `	}` |
|       - | 1297 | `	/* Same class but a different number of attributes ⇒ different property sets` |
|       - | 1298 | `	 * (dynamic properties can give two same-class instances different counts). */` |
|     147 | 1299 | `	if( pLeft->hAttr.nEntry != pRight->hAttr.nEntry ){` |
|       3 | 1300 | `		return 1;` |
|       - | 1301 | `	}` |
|     145 | 1302 | `	PH7_MemObjInit(pLeft->pVm,&sV1);` |
|     145 | 1303 | `	PH7_MemObjInit(pLeft->pVm,&sV2);` |
|     145 | 1304 | `	sV1.nIdx = sV2.nIdx = SXU32_HIGH;` |
|       - | 1305 | `	/* Compare each left attribute against the RIGHT attribute of the SAME NAME` |
|       - | 1306 | `	 * (not in lockstep): dynamic properties may be stored in a different order` |
|       - | 1307 | `	 * on the two instances. Counts already match, so if every left attribute has` |
|       - | 1308 | `	 * an equal-valued same-named right attribute the property sets are equal. */` |
|     145 | 1309 | `	SyHashResetLoopCursor(&pLeft->hAttr);` |
|     169 | 1310 | `	while((pEntry = SyHashGetNextEntry(&pLeft->hAttr)) != 0 ){` |
|     157 | 1311 | `		VmClassAttr *p1 = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1312 | `		VmClassAttr *p2;` |
|       - | 1313 | `		ph7_value *pL,*pR;` |
|       - | 1314 | `		/* Compare only non-static attribute */` |
|     157 | 1315 | `		if( p1->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|     ! 0 | 1316 | `			continue;` |
|       - | 1317 | `		}` |
|     157 | 1318 | `		pEntry2 = SyHashGet(&pRight->hAttr,SyStringData(&p1->pAttr->sName),SyStringLength(&p1->pAttr->sName));` |
|     157 | 1319 | `		if( pEntry2 == 0 ){` |
|       - | 1320 | `			/* Left has a property the right lacks ⇒ not equal. */` |
|     ! 0 | 1321 | `			return 1;` |
|       - | 1322 | `		}` |
|     157 | 1323 | `		p2 = (VmClassAttr *)pEntry2->pUserData;` |
|     157 | 1324 | `		pL = ExtractClassAttrValue(pLeft->pVm,p1);` |
|     157 | 1325 | `		pR = ExtractClassAttrValue(pRight->pVm,p2);` |
|     157 | 1326 | `		if( pL && pR ){` |
|     157 | 1327 | `			PH7_MemObjLoad(pL,&sV1);` |
|     157 | 1328 | `			PH7_MemObjLoad(pR,&sV2);` |
|       - | 1329 | `			/* Compare the two values now */` |
|     157 | 1330 | `			rc = PH7_MemObjCmp(&sV1,&sV2,bStrict,iNest+1);` |
|     157 | 1331 | `			PH7_MemObjRelease(&sV1);` |
|     157 | 1332 | `			PH7_MemObjRelease(&sV2);` |
|     157 | 1333 | `			if( rc != 0 ){` |
|       - | 1334 | `				/* Not equals */` |
|     133 | 1335 | `				return rc;` |
|       - | 1336 | `			}` |
|      12 | 1337 | `		}` |
|       1 | 1338 | `	}` |
|       - | 1339 | `	/* Object are equals */` |
|      13 | 1340 | `	return 0;` |
|     131 | 1341 | `}` |
|       - | 1342 | `/*` |
|       - | 1343 | ` * Dump a class instance and the store the dump in the BLOB given` |
|       - | 1344 | ` * as the first argument.` |
|       - | 1345 | ` * Note that only non-static/non-constants attribute are dumped.` |
|       - | 1346 | ` * This function is typically invoked when the user issue a call` |
|       - | 1347 | ` * to [var_dump(),var_export(),print_r(),...].` |
|       - | 1348 | ` * This function SXRET_OK on success. Any other return value including` |
|       - | 1349 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|       - | 1350 | ` */` |
|       - | 1351 | `/*` |
|       - | 1352 | `` * Return the `name` property value of an enum case instance (the case name),`` |
|       - | 1353 | ` * or 0 when unavailable. Shared by the var_dump/var_export/json/serialize` |
|       - | 1354 | ` * renderers, which all print enum cases as Class::CaseName forms.` |
|       - | 1355 | ` */` |
|       6 | 1356 | `PH7_PRIVATE ph7_value * PH7_EnumCaseNameValue(ph7_class_instance *pThis)` |
|       1 | 1357 | `{` |
|       - | 1358 | `	SyHashEntry *pEntry;` |
|       7 | 1359 | `	if( (pThis->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|     ! 0 | 1360 | `		return 0;` |
|       - | 1361 | `	}` |
|       7 | 1362 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)"name",sizeof("name")-1);` |
|       7 | 1363 | `	if( pEntry == 0 ){` |
|     ! 0 | 1364 | `		return 0;` |
|       - | 1365 | `	}` |
|       7 | 1366 | `	return PH7_ClassInstanceExtractAttrValue(pThis,(VmClassAttr *)pEntry->pUserData);` |
|       4 | 1367 | `}` |
|       - | 1368 | `/*` |
|       - | 1369 | `` * Return the `value` property value (the backing value) of an enum case`` |
|       - | 1370 | ` * instance, or 0 when unavailable (pure enums have none).` |
|       - | 1371 | ` */` |
|       8 | 1372 | `PH7_PRIVATE ph7_value * PH7_EnumCaseBackingValueOf(ph7_class_instance *pThis)` |
|       1 | 1373 | `{` |
|       - | 1374 | `	SyHashEntry *pEntry;` |
|       9 | 1375 | `	if( (pThis->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|     ! 0 | 1376 | `		return 0;` |
|       - | 1377 | `	}` |
|       9 | 1378 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)"value",sizeof("value")-1);` |
|       9 | 1379 | `	if( pEntry == 0 ){` |
|       3 | 1380 | `		return 0;` |
|       - | 1381 | `	}` |
|       7 | 1382 | `	return PH7_ClassInstanceExtractAttrValue(pThis,(VmClassAttr *)pEntry->pUserData);` |
|       5 | 1383 | `}` |
|       - | 1384 | `/*` |
|       - | 1385 | ` * Emit a class-instance dump header plus its trailing newline. For var_dump` |
|       - | 1386 | ` * (ShowType) it completes the "object(" prefix the caller already emitted as` |
|       - | 1387 | ` *   ClassName)#<id> (<count>) {` |
|       - | 1388 | ` * for print_r it emits the legacy PHL  Object(ClassName) {  (count/id unused).` |
|       - | 1389 | `` * Enum cases print php's `ClassName Enum {` print_r header (var_dump never`` |
|       - | 1390 | `` * reaches here for enums — PH7_MemObjDump prints `enum(S::A)` directly).`` |
|       - | 1391 | ` */` |
|     136 | 1392 | `static void DumpClassInstanceHeader(SyBlob *pOut,ph7_class *pClass,sxu32 nObjId,int ShowType,sxu32 nCount)` |
|       3 | 1393 | `{` |
|     139 | 1394 | `	if( ShowType ){` |
|       - | 1395 | ``		/* var_dump: `object(C)#id (n) {` */`` |
|     135 | 1396 | `		SyBlobFormat(&(*pOut),"object(%z)#%u (%u) {",&pClass->sName,nObjId,nCount);` |
|     135 | 1397 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|     135 | 1398 | `		return;` |
|       - | 1399 | `	}` |
|       - | 1400 | ``	/* print_r: `C Object` / `E Enum[:backing]` — the '(' line is emitted by`` |
|       - | 1401 | `	 * the body renderer at the container indent. */` |
|       6 | 1402 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|     ! 0 | 1403 | `		SyBlobFormat(&(*pOut),"%z Enum",&pClass->sName);` |
|     ! 0 | 1404 | `		if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|     ! 0 | 1405 | `			SyBlobAppend(&(*pOut),":int",sizeof(":int")-1);` |
|     ! 0 | 1406 | `		}else if( pClass->nEnumBacking == MEMOBJ_STRING ){` |
|     ! 0 | 1407 | `			SyBlobAppend(&(*pOut),":string",sizeof(":string")-1);` |
|     ! 0 | 1408 | `		}` |
|     ! 0 | 1409 | `	}else{` |
|       6 | 1410 | `		SyBlobFormat(&(*pOut),"%z Object",&pClass->sName);` |
|       - | 1411 | `	}` |
|       6 | 1412 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      71 | 1413 | `}` |
|       - | 1414 | `/*` |
|       - | 1415 | ` * The class that DECLARED pAttr: inheritance shares attr pointers down the` |
|       - | 1416 | ` * chain, so the declaring class is the most ANCESTRAL class whose hAttr still` |
|       - | 1417 | ` * maps the name to this exact pointer. php's var_dump/print_r use it for the` |
|       - | 1418 | `` * `["p":"Decl":private]` annotation.`` |
|       - | 1419 | ` */` |
|       6 | 1420 | `static ph7_class * OoAttrDeclaringClass(ph7_class *pClass,ph7_class_attr *pAttr)` |
|       1 | 1421 | `{` |
|       - | 1422 | `	/* Attrs record their declaring class at install time (inheritance/trait` |
|       - | 1423 | `	 * copies share the pointer, so the field survives the chain). */` |
|       7 | 1424 | `	return pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|       1 | 1425 | `}` |
|       - | 1426 | `/*` |
|       - | 1427 | `` * Emit a property's dump key: var_dump `["x"]=>` / `["p":"C":private]=>` /`` |
|       - | 1428 | `` * `["q":protected]=>`; print_r `[x] => ` / `[p:C:private] => ` /`` |
|       - | 1429 | `` * `[q:protected] => ` (php's exact annotations).`` |
|       - | 1430 | ` */` |
|     138 | 1431 | `static void OoDumpPropKey(SyBlob *pOut,ph7_class_instance *pThis,ph7_class_attr *pAttr,int ShowType)` |
|       2 | 1432 | `{` |
|     140 | 1433 | `	const char *zQ = ShowType ? "\"" : "";` |
|     140 | 1434 | `	SyBlobFormat(&(*pOut),"[%s%z%s",zQ,&pAttr->sName,zQ);` |
|     140 | 1435 | `	if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       7 | 1436 | `		ph7_class *pDecl = OoAttrDeclaringClass(pThis->pClass,pAttr);` |
|       7 | 1437 | `		SyBlobFormat(&(*pOut),":%s%z%s:private",zQ,&pDecl->sName,zQ);` |
|     137 | 1438 | `	}else if( pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
|     ! 0 | 1439 | `		SyBlobAppend(&(*pOut),":protected",sizeof(":protected")-1);` |
|     ! 0 | 1440 | `	}` |
|     140 | 1441 | `	SyBlobAppend(&(*pOut),ShowType ? "]=>" : "] => ",ShowType ? 3 : 5);` |
|     140 | 1442 | `}` |
|     140 | 1443 | `PH7_PRIVATE sxi32 PH7_ClassInstanceDump(SyBlob *pOut,ph7_class_instance *pThis,int ShowType,int nTab,int nDepth)` |
|       3 | 1444 | `{` |
|       - | 1445 | `	SyHashEntry *pEntry;` |
|       - | 1446 | `	ph7_value *pValue;` |
|       - | 1447 | `	sxi32 rc;` |
|       - | 1448 | `	int i;` |
|     143 | 1449 | `	if( nDepth > 31 ){` |
|       - | 1450 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|       - | 1451 | `		/* Nesting limit reached..halt immediately*/` |
|       5 | 1452 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|       5 | 1453 | `		return SXERR_LIMIT;` |
|       - | 1454 | `	}` |
|     139 | 1455 | `	rc = SXRET_OK;` |
|       - | 1456 | `	{` |
|       - | 1457 | `		/* Both var_dump and print_r consult __debugInfo() (PHP behavior);` |
|       - | 1458 | `		 * var_export uses a separate renderer and never reaches here. When the` |
|       - | 1459 | `		 * method is present and returns an array, render that array's entries as` |
|       - | 1460 | `		 * the object body, with the header showing the debug array's count. The` |
|       - | 1461 | `		 * nDepth guard above protects against a __debugInfo returning the object` |
|       - | 1462 | `		 * itself. */` |
|     139 | 1463 | `		ph7_class_method *pDbg = PH7_ClassExtractMethod(pThis->pClass,"__debugInfo",sizeof("__debugInfo")-1);` |
|     139 | 1464 | `		if( pDbg ){` |
|       - | 1465 | `			ph7_value sResult;` |
|       5 | 1466 | `			PH7_MemObjInit(pThis->pVm,&sResult);` |
|       5 | 1467 | `			PH7_VmCallClassMethod(pThis->pVm,pThis,pDbg,&sResult,0,0);` |
|       5 | 1468 | `			if( sResult.iFlags & MEMOBJ_HASHMAP ){` |
|       5 | 1469 | `				ph7_hashmap *pMap = (ph7_hashmap *)sResult.x.pOther;` |
|       - | 1470 | `				/* Header count is the debug array's entry count. */` |
|       5 | 1471 | `				DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,pMap->nEntry);` |
|       5 | 1472 | `				if( !ShowType ){` |
|       3 | 1473 | `					for( i = 0 ; i < nTab ; i++ ){` |
|     ! 0 | 1474 | `						SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     ! 0 | 1475 | `					}` |
|       3 | 1476 | `					SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       1 | 1477 | `				}` |
|       5 | 1478 | `				rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth);` |
|       5 | 1479 | `				for( i = 0 ; i < nTab ; i++ ){` |
|     ! 0 | 1480 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     ! 0 | 1481 | `				}` |
|       5 | 1482 | `				if( ShowType ){` |
|       3 | 1483 | `					SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|       2 | 1484 | `				}else{` |
|       3 | 1485 | `					SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       - | 1486 | `				}` |
|       5 | 1487 | `				PH7_MemObjRelease(&sResult);` |
|       5 | 1488 | `				return rc;` |
|       - | 1489 | `			}` |
|       - | 1490 | `			/* Non-array return: behave as if __debugInfo were absent. */` |
|     ! 0 | 1491 | `			PH7_MemObjRelease(&sResult);` |
|     ! 0 | 1492 | `		}` |
|       - | 1493 | `	}` |
|       - | 1494 | `	{` |
|       - | 1495 | `		/* var_dump's header needs the property count up front, so pre-count the` |
|       - | 1496 | `		 * non-static/non-constant attributes (matching the dump loop below). */` |
|     134 | 1497 | `		sxu32 nProp = 0;` |
|     134 | 1498 | `		if( ShowType ){` |
|     132 | 1499 | `			SyHashResetLoopCursor(&pThis->hAttr);` |
|     270 | 1500 | `			while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|     140 | 1501 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     140 | 1502 | `				if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_HOOK_VIRTUAL)) == 0 ){` |
|     136 | 1503 | `					nProp++;` |
|      67 | 1504 | `				}` |
|       2 | 1505 | `			}` |
|      65 | 1506 | `		}` |
|     134 | 1507 | `		DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,nProp);` |
|       - | 1508 | `	}` |
|     134 | 1509 | `	if( !ShowType ){` |
|       - | 1510 | `		/* print_r body opener: '(' at the container indent */` |
|       3 | 1511 | `		for( i = 0 ; i < nTab ; i++ ){` |
|     ! 0 | 1512 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     ! 0 | 1513 | `		}` |
|       3 | 1514 | `		SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       1 | 1515 | `	}` |
|       - | 1516 | `	/* Dump object attributes (php 8.4: VIRTUAL hooked properties have no` |
|       - | 1517 | `	 * backing store — excluded from var_dump/print_r) */` |
|     134 | 1518 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|     218 | 1519 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|     144 | 1520 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     144 | 1521 | `		if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_HOOK_VIRTUAL)) == 0 ){` |
|       - | 1522 | `			/* Dump non-static/constant attribute only */` |
|     140 | 1523 | `			pValue = ExtractClassAttrValue(pThis->pVm,pVmAttr);` |
|     140 | 1524 | `			if( pValue == 0 ){` |
|     ! 0 | 1525 | `				continue;` |
|       - | 1526 | `			}` |
|     140 | 1527 | `			if( ShowType ){` |
|       - | 1528 | ``				/* var_dump prop: `["x"(:…)]=>` at nTab+2, the value on the next`` |
|       - | 1529 | `				 * line at the same indent (php). */` |
|    4124 | 1530 | `				for( i = 0 ; i < nTab + 2 ; i++ ){` |
|    3990 | 1531 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|    1996 | 1532 | `				}` |
|     136 | 1533 | `				OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,TRUE);` |
|     136 | 1534 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|     136 | 1535 | `				rc = PH7_MemObjDump(&(*pOut),pValue,TRUE,nTab+2,nDepth,0);` |
|     136 | 1536 | `				if( rc == SXERR_LIMIT ){` |
|     125 | 1537 | `					break;` |
|       - | 1538 | `				}` |
|       7 | 1539 | `			}else{` |
|       - | 1540 | ``				/* print_r prop: `[x(:…)] => value` at nTab+4; container values`` |
|       - | 1541 | `				 * render their block at nTab+8 followed by php's blank line. */` |
|      21 | 1542 | `				for( i = 0 ; i < nTab + 4 ; i++ ){` |
|      17 | 1543 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       9 | 1544 | `				}` |
|       5 | 1545 | `				OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,FALSE);` |
|       4 | 1546 | `				if( (pValue->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ))` |
|       3 | 1547 | `				 && (pValue->iFlags & MEMOBJ_NULL) == 0 ){` |
|     ! 0 | 1548 | `					rc = PH7_MemObjDump(&(*pOut),pValue,FALSE,nTab+8,nDepth,0);` |
|     ! 0 | 1549 | `					SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|     ! 0 | 1550 | `					if( rc == SXERR_LIMIT ){` |
|     ! 0 | 1551 | `						break;` |
|       - | 1552 | `					}` |
|     ! 0 | 1553 | `				}else{` |
|       5 | 1554 | `					PH7_MemObjPrintRInline(&(*pOut),pValue);` |
|       5 | 1555 | `					SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 1556 | `				}` |
|       - | 1557 | `			}` |
|       7 | 1558 | `		}` |
|       2 | 1559 | `	}` |
|    3854 | 1560 | `	for( i = 0 ; i < nTab ; i++ ){` |
|    3721 | 1561 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|    1861 | 1562 | `	}` |
|     134 | 1563 | `	if( ShowType ){` |
|     132 | 1564 | `		SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      67 | 1565 | `	}else{` |
|       3 | 1566 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       - | 1567 | `	}` |
|     134 | 1568 | `	return rc;` |
|      73 | 1569 | `}` |
|       - | 1570 | `/*` |
|       - | 1571 | ` * Call a magic method [i.e: __toString(),__toBool(),__Invoke()...]` |
|       - | 1572 | ` * Return SXRET_OK on successfull call. Any other return value indicates failure.` |
|       - | 1573 | ` * Notes on magic methods.` |
|       - | 1574 | ` * According to the PHP language reference manual.` |
|       - | 1575 | ` *  The function names __construct(), __destruct(), __call(), __callStatic()` |
|       - | 1576 | ` *  __get(),  __toString(), __invoke(), __clone() are magical in PHP classes.` |
|       - | 1577 | ` * You cannot have functions with these names in any of your classes unless` |
|       - | 1578 | ` * you want the magic functionality associated with them.` |
|       - | 1579 | ` * Example of magical methods:` |
|       - | 1580 | ` * __toString()` |
|       - | 1581 | ` *  The __toString() method allows a class to decide how it will react when it is treated like` |
|       - | 1582 | ` *  a string. For example, what echo $obj; will print. This method must return a string.` |
|       - | 1583 | ` *  Example #2 Simple example` |
|       - | 1584 | ` * <?php` |
|       - | 1585 | ` * // Declare a simple class` |
|       - | 1586 | ` * class TestClass` |
|       - | 1587 | ` * {` |
|       - | 1588 | ` *   public $foo;` |
|       - | 1589 | ` *` |
|       - | 1590 | ` *   public function __construct($foo)` |
|       - | 1591 | ` *   {` |
|       - | 1592 | ` *       $this->foo = $foo;` |
|       - | 1593 | ` *   }` |
|       - | 1594 | ` *` |
|       - | 1595 | ` *   public function __toString()` |
|       - | 1596 | ` *   {` |
|       - | 1597 | ` *       return $this->foo;` |
|       - | 1598 | ` *   }` |
|       - | 1599 | ` * }` |
|       - | 1600 | ` * $class = new TestClass('Hello');` |
|       - | 1601 | ` * echo $class;` |
|       - | 1602 | ` * ?>` |
|       - | 1603 | ` * The above example will output:` |
|       - | 1604 | ` *  Hello` |
|       - | 1605 | ` *` |
|       - | 1606 | ` * Note that PH7 does not support all the magical method and introudces __toFloat(),__toInt()` |
|       - | 1607 | ` * which have the same behaviour as __toString() but for float and integer types` |
|       - | 1608 | ` * respectively.` |
|       - | 1609 | ` * Refer to the official documentation for more information.` |
|       - | 1610 | ` */` |
|      44 | 1611 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallMagicMethod(` |
|       - | 1612 | `	ph7_vm *pVm,               /* VM that own all this stuff */` |
|       - | 1613 | `	ph7_class *pClass,         /* Target class */` |
|       - | 1614 | `	ph7_class_instance *pThis, /* Target object */` |
|       - | 1615 | `	const char *zMethod,       /* Magic method name [i.e: __toString()]*/` |
|       - | 1616 | `	sxu32 nByte,               /* zMethod length*/` |
|       - | 1617 | `	const SyString *pAttrName, /* Attribute name */` |
|       - | 1618 | `	ph7_value *pResult         /* OUT: magic method return value. NULL to discard */` |
|       - | 1619 | `	)` |
|       1 | 1620 | `{` |
|      45 | 1621 | `	ph7_value *apArg[2] = { 0 , 0 };` |
|       - | 1622 | `	ph7_class_method *pMeth;` |
|       - | 1623 | `	ph7_value sAttr; /* cc warning */` |
|       - | 1624 | `	sxi32 rc;` |
|       - | 1625 | `	int nArg;` |
|       - | 1626 | `	/* Make sure the magic method is available */` |
|      45 | 1627 | `	pMeth = PH7_ClassExtractMethod(&(*pClass),zMethod,nByte);` |
|      45 | 1628 | `	if( pMeth == 0 ){` |
|       - | 1629 | `		/* No such method,return immediately */` |
|     ! 0 | 1630 | `		return SXERR_NOTFOUND;` |
|       - | 1631 | `	}` |
|      45 | 1632 | `	nArg = 0;` |
|       - | 1633 | `	/* Copy arguments */` |
|      45 | 1634 | `	if( pAttrName ){` |
|      45 | 1635 | `		PH7_MemObjInitFromString(pVm,&sAttr,pAttrName);` |
|      45 | 1636 | `		sAttr.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      45 | 1637 | `		apArg[0] = &sAttr;` |
|      45 | 1638 | `		nArg = 1;` |
|      22 | 1639 | `	}` |
|       - | 1640 | `	/* Call the magic method now */` |
|      45 | 1641 | `	rc = PH7_VmCallClassMethod(pVm,&(*pThis),pMeth,pResult,nArg,apArg);` |
|       - | 1642 | `	/* Clean up */` |
|      45 | 1643 | `	if( pAttrName ){` |
|      45 | 1644 | `		PH7_MemObjRelease(&sAttr);` |
|      22 | 1645 | `	}` |
|      45 | 1646 | `	return rc;` |
|      23 | 1647 | `}` |
|       - | 1648 | `/*` |
|       - | 1649 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon].` |
|       - | 1650 | ` * This function is simply a wrapper on ExtractClassAttrValue().` |
|       - | 1651 | ` */` |
|     216 | 1652 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceExtractAttrValue(ph7_class_instance *pThis,VmClassAttr *pAttr)` |
|       4 | 1653 | `{` |
|       - | 1654 | `   /* Extract the attribute value */` |
|       - | 1655 | `	ph7_value *pValue;` |
|     220 | 1656 | `	pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     220 | 1657 | `	return pValue;` |
|       4 | 1658 | `}` |
|       - | 1659 | `/*` |
|       - | 1660 | ` * Convert a class instance [i.e: Object in the PHP jargon] into a hashmap [i.e: array in the PHP jargon].` |
|       - | 1661 | ` * Return SXRET_OK on success. Any other value indicates failure.` |
|       - | 1662 | ` * Note on object conversion to array:` |
|       - | 1663 | ` *  Acccording to the PHP language reference manual` |
|       - | 1664 | ` *  If an object is converted to an array, the result is an array whose elements are the object's properties.` |
|       - | 1665 | ` *  The keys are the member variable names.` |
|       - | 1666 | ` *` |
|       - | 1667 | ` *  The following example:` |
|       - | 1668 | ` *  class Test {` |
|       - | 1669 | ` *   public $A = 25<<1;  // 50` |
|       - | 1670 | ` *	 public $c = rand_str(3);   // Random string of length 3` |
|       - | 1671 | ` *	 public $d = rand() & 1023; // Random number between 0..1023` |
|       - | 1672 | ` *  }` |
|       - | 1673 | ` *  var_dump((array) new Test());` |
|       - | 1674 | ` *	Will output:` |
|       - | 1675 | ` *  array(3) {` |
|       - | 1676 | ` *   [A] =>` |
|       - | 1677 | ` *      int(50)` |
|       - | 1678 | ` *   [c] =>` |
|       - | 1679 | ` *     string(3 'aps')` |
|       - | 1680 | ` *   [d] =>` |
|       - | 1681 | ` *     int(991)` |
|       - | 1682 | ` *  }` |
|       - | 1683 | ` * You have noticed that PH7 allow class attributes [i.e: $a,$c,$d in the example above]` |
|       - | 1684 | ` * have any complex expression (even function calls/Annonymous functions) as their default` |
|       - | 1685 | ` * value unlike the standard PHP engine.` |
|       - | 1686 | ` * This is a very powerful feature that you have to look at.` |
|       - | 1687 | ` */` |
|      14 | 1688 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|       1 | 1689 | `{` |
|       - | 1690 | `	SyHashEntry *pEntry;` |
|       - | 1691 | `	SyString *pAttrName;` |
|       - | 1692 | `	VmClassAttr *pAttr;` |
|       - | 1693 | `	ph7_value *pValue;` |
|       - | 1694 | `	ph7_value sName;` |
|       - | 1695 | `	/* Reset the loop cursor */` |
|      15 | 1696 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      15 | 1697 | `	PH7_MemObjInitFromString(pThis->pVm,&sName,0);` |
|      51 | 1698 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       - | 1699 | `		/* Point to the current attribute */` |
|      37 | 1700 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      37 | 1701 | `		if( pAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|       - | 1702 | `			/* php 8.4: a VIRTUAL hooked property has no backing store — the` |
|       - | 1703 | `			 * (array) cast excludes it (raw surface, get is NOT dispatched) */` |
|       7 | 1704 | `			continue;` |
|       - | 1705 | `		}` |
|       - | 1706 | `		/* Extract attribute value */` |
|      31 | 1707 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|      31 | 1708 | `		if( pValue ){` |
|       - | 1709 | `			/* Build attribute name */` |
|      31 | 1710 | `			pAttrName = &pAttr->pAttr->sName;` |
|      31 | 1711 | `			PH7_MemObjStringAppend(&sName,pAttrName->zString,pAttrName->nByte);` |
|       - | 1712 | `			/* Perform the insertion */` |
|      31 | 1713 | `			PH7_HashmapInsert(pMap,&sName,pValue);` |
|       - | 1714 | `			/* Reset the string cursor */` |
|      31 | 1715 | `			SyBlobReset(&sName.sBlob);` |
|      15 | 1716 | `		}` |
|       1 | 1717 | `	}` |
|      15 | 1718 | `	PH7_MemObjRelease(&sName);` |
|      15 | 1719 | `	return SXRET_OK;` |
|       1 | 1720 | `}` |
|       - | 1721 | `/*` |
|       - | 1722 | ` * Iterate throw class attributes and invoke the given callback [i.e: xWalk()] for each` |
|       - | 1723 | ` * retrieved attribute.` |
|       - | 1724 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|       - | 1725 | ` * the attribute value in the callback body will not alter the real attribute value.` |
|       - | 1726 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|       - | 1727 | ` * a value different from PH7_OK.` |
|       - | 1728 | ` * Refer to [ph7_object_walk()] for more information.` |
|       - | 1729 | ` */` |
|     ! 0 | 1730 | `PH7_PRIVATE sxi32 PH7_ClassInstanceWalk(` |
|       - | 1731 | `	ph7_class_instance *pThis, /* Target object */` |
|       - | 1732 | `	int (*xWalk)(const char *,ph7_value *,void *), /* Walker callback */` |
|       - | 1733 | `	void *pUserData /* Last argument to xWalk() */` |
|       - | 1734 | `	)` |
|     ! 0 | 1735 | `{` |
|       - | 1736 | `	SyHashEntry *pEntry; /* Hash entry */` |
|       - | 1737 | `	VmClassAttr *pAttr;  /* Pointer to the attribute */` |
|       - | 1738 | `	ph7_value *pValue;   /* Attribute value */` |
|       - | 1739 | `	ph7_value sValue;    /* Copy of the attribute value */` |
|       - | 1740 | `	int rc;` |
|       - | 1741 | `	/* Reset the loop cursor */` |
|     ! 0 | 1742 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|     ! 0 | 1743 | `	PH7_MemObjInit(pThis->pVm,&sValue);` |
|       - | 1744 | `	/* Start the walk process */` |
|     ! 0 | 1745 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       - | 1746 | `		/* Point to the current attribute */` |
|     ! 0 | 1747 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1748 | `		/* Extract attribute value */` |
|     ! 0 | 1749 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     ! 0 | 1750 | `		if( pValue ){` |
|     ! 0 | 1751 | `			PH7_MemObjLoad(pValue,&sValue);` |
|       - | 1752 | `			/* Invoke the supplied callback */` |
|     ! 0 | 1753 | `			rc =  xWalk(SyStringData(&pAttr->pAttr->sName),&sValue,pUserData);` |
|     ! 0 | 1754 | `			PH7_MemObjRelease(&sValue);` |
|     ! 0 | 1755 | `			if( rc != PH7_OK){` |
|       - | 1756 | `				/* User callback request an operation abort */` |
|     ! 0 | 1757 | `				return SXERR_ABORT;` |
|       - | 1758 | `			}` |
|     ! 0 | 1759 | `		}` |
|     ! 0 | 1760 | `	}` |
|       - | 1761 | `	/* All done */` |
|     ! 0 | 1762 | `	return SXRET_OK;` |
|     ! 0 | 1763 | `}` |
|       - | 1764 | `/*` |
|       - | 1765 | ` * Extract a class atrribute value.` |
|       - | 1766 | ` * Return a pointer to the attribute value on success. Otherwise NULL.` |
|       - | 1767 | ` * Note:` |
|       - | 1768 | ` *  Access to static and constant attribute is not allowed. That is,the function` |
|       - | 1769 | ` *  will return NULL in case someone (host-application code) try to extract` |
|       - | 1770 | ` *  a static/constant attribute.` |
|       - | 1771 | ` */` |
|    9630 | 1772 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceFetchAttr(ph7_class_instance *pThis,const SyString *pName)` |
|       5 | 1773 | `{` |
|       - | 1774 | `	SyHashEntry *pEntry;` |
|       - | 1775 | `	VmClassAttr *pAttr;` |
|       - | 1776 | `	/* Query the attribute hashtable */` |
|    9635 | 1777 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);` |
|    9635 | 1778 | `	if( pEntry == 0 ){` |
|       - | 1779 | `		/* No such attribute */` |
|     ! 0 | 1780 | `		return 0;` |
|       - | 1781 | `	}` |
|       - | 1782 | `	/* Point to the class atrribute */` |
|    9635 | 1783 | `	pAttr = (VmClassAttr *)pEntry->pUserData;` |
|       - | 1784 | `	/* Check if we are dealing with a static/constant attribute */` |
|    9635 | 1785 | `	if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|       - | 1786 | `		/* Access is forbidden */` |
|     ! 0 | 1787 | `		return 0;` |
|       - | 1788 | `	}` |
|       - | 1789 | `	/* Return the attribute value */` |
|    9635 | 1790 | `	return ExtractClassAttrValue(pThis->pVm,pAttr);` |
|    4820 | 1791 | `}` |
|       - | 1792 |  |
