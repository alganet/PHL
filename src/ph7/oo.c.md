# src/ph7/oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 883/1013 lines (87.17%)

[Root index](../../index.md) | [Directory index](index.md)

|     Hits | Line | Source |
| -------: | ---: | :--- |
|        - |    1 | `/**` |
|        - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|        - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|        - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|        - |    5 | ` */` |
|        - |    6 | `#include "ph7int.h"` |
|        - |    7 | `/*` |
|        - |    8 | ` * This file implement an Object Oriented (OO) subsystem for the PH7 engine.` |
|        - |    9 | ` */` |
|        - |   10 | `/*` |
|        - |   11 | ` * Create an empty class.` |
|        - |   12 | ` * Return a pointer to a raw class (ph7_class instance) on success. NULL otherwise.` |
|        - |   13 | ` */` |
|   629476 |   14 | `PH7_PRIVATE ph7_class * PH7_NewRawClass(ph7_vm *pVm,const SyString *pName,sxu32 nLine)` |
|        5 |   15 | `{` |
|        - |   16 | `	ph7_class *pClass;` |
|        - |   17 | `	char *zName;` |
|        - |   18 | `	/* Allocate a new instance */` |
|   629481 |   19 | `	pClass = (ph7_class *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class));` |
|   629481 |   20 | `	if( pClass == 0 ){` |
|      ! 0 |   21 | `		return 0;` |
|        - |   22 | `	}` |
|        - |   23 | `	/* Zero the structure */` |
|   629481 |   24 | `	SyZero(pClass,sizeof(ph7_class));` |
|        - |   25 | `	/* Duplicate class name */` |
|   629481 |   26 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   629481 |   27 | `	if( zName == 0 ){` |
|      ! 0 |   28 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClass);` |
|      ! 0 |   29 | `		return 0;` |
|        - |   30 | `	}` |
|        - |   31 | `	/* Initialize fields */` |
|   629481 |   32 | `	SyStringInitFromBuf(&pClass->sName,zName,pName->nByte);` |
|        - |   33 | `	/* php method names are CASE-INSENSITIVE ($o->FOO() finds foo(), and declaring` |
|        - |   34 | `	 * both is a redeclaration), so the method table matches on them the same way` |
|        - |   35 | `	 * hClass does for class names. Properties and class constants ARE case` |
|        - |   36 | `	 * sensitive in php, so hAttr keeps the default exact comparator. */` |
|   629481 |   37 | `	SyHashInit(&pClass->hMethod,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|   629481 |   38 | `	SyHashInit(&pClass->hAttr,&pVm->sAllocator,0,0);` |
|   629481 |   39 | `	SyHashInit(&pClass->hConst,&pVm->sAllocator,0,0);` |
|   629481 |   40 | `	SyHashInit(&pClass->hDerived,&pVm->sAllocator,0,0);` |
|   629481 |   41 | `	SySetInit(&pClass->aInterface,&pVm->sAllocator,sizeof(ph7_class *));` |
|   629481 |   42 | `	SySetInit(&pClass->aTrait,&pVm->sAllocator,sizeof(ph7_class *));` |
|   629481 |   43 | `	SySetInit(&pClass->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|   629481 |   44 | `	SySetInit(&pClass->aEnumCases,&pVm->sAllocator,sizeof(ph7_class_attr *));` |
|   629481 |   45 | `	pClass->nLine = nLine;` |
|   629481 |   46 | `	if( pVm->bCompilingBuiltin ){` |
|        - |   47 | `		/* Defined by an embedded builtin chunk: internal, no defining file.` |
|        - |   48 | `		 * Class compilers merge further flags with \|= so this survives. */` |
|   625785 |   49 | `		pClass->iFlags \|= PH7_CLASS_INTERNAL;` |
|   312895 |   50 | `	}else{` |
|        - |   51 | `		/* Alias the VM-lifetime path dup on top of the include stack */` |
|     3701 |   52 | `		SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     3701 |   53 | `		if( pFile ){` |
|     3701 |   54 | `			SyStringDupPtr(&pClass->sFile,pFile);` |
|     1848 |   55 | `		}` |
|        - |   56 | `	}` |
|        - |   57 | `	/* All done */` |
|   629481 |   58 | `	return pClass;` |
|   314743 |   59 | `}` |
|        - |   60 | `/*` |
|        - |   61 | ` * Allocate and initialize a new class attribute.` |
|        - |   62 | ` * Return a pointer to the class attribute on success. NULL otherwise.` |
|        - |   63 | ` */` |
|  1221444 |   64 | `PH7_PRIVATE ph7_class_attr * PH7_NewClassAttr(ph7_vm *pVm,const SyString *pName,sxu32 nLine,sxi32 iProtection,sxi32 iFlags)` |
|        5 |   65 | `{` |
|        - |   66 | `	ph7_class_attr *pAttr;` |
|        - |   67 | `	char *zName;` |
|  1221449 |   68 | `	pAttr = (ph7_class_attr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_attr));` |
|  1221449 |   69 | `	if( pAttr == 0 ){` |
|      ! 0 |   70 | `		return 0;` |
|        - |   71 | `	}` |
|        - |   72 | `	/* Zero the structure */` |
|  1221449 |   73 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|  1221449 |   74 | `	SySetInit(&pAttr->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|        - |   75 | `	/* Duplicate attribute name */` |
|  1221449 |   76 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1221449 |   77 | `	if( zName == 0 ){` |
|      ! 0 |   78 | `		SyMemBackendPoolFree(&pVm->sAllocator,pAttr);` |
|      ! 0 |   79 | `		return 0;` |
|        - |   80 | `	}` |
|        - |   81 | `	/* Initialize fields */` |
|  1221449 |   82 | `	SySetInit(&pAttr->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|  1221449 |   83 | `	SySetInit(&pAttr->aUnionAlts,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|  1221449 |   84 | `	SyStringInitFromBuf(&pAttr->sName,zName,pName->nByte);` |
|  1221449 |   85 | `	pAttr->iProtection = iProtection;` |
|  1221449 |   86 | `	pAttr->nIdx = SXU32_HIGH;` |
|  1221449 |   87 | `	pAttr->iFlags = iFlags;` |
|  1221449 |   88 | `	pAttr->nLine = nLine;` |
|  1221449 |   89 | `	return pAttr;` |
|   610727 |   90 | `}` |
|        - |   91 | `/*` |
|        - |   92 | ` * Allocate and initialize a new class method.` |
|        - |   93 | ` * Return a pointer to the class method on success. NULL otherwise` |
|        - |   94 | ` * This function associate with the newly created method an automatically generated` |
|        - |   95 | ` * random unique name.` |
|        - |   96 | ` */` |
|  3664916 |   97 | `PH7_PRIVATE ph7_class_method * PH7_NewClassMethod(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,sxu32 nLine,` |
|        - |   98 | `	sxi32 iProtection,sxi32 iFlags,sxi32 iFuncFlags)` |
|        5 |   99 | `{` |
|        - |  100 | `	ph7_class_method *pMeth;` |
|        - |  101 | `	SyHashEntry *pEntry;` |
|        - |  102 | `	SyString *pNamePtr;` |
|        - |  103 | `	char zSalt[10];` |
|        - |  104 | `	char *zName;` |
|        - |  105 | `	sxu32 nByte;` |
|        - |  106 | `	/* Allocate a new class method instance */` |
|  3664921 |  107 | `	pMeth = (ph7_class_method *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_method));` |
|  3664921 |  108 | `	if( pMeth == 0 ){` |
|      ! 0 |  109 | `		return 0;` |
|        - |  110 | `	}` |
|        - |  111 | `	/* Zero the structure */` |
|  3664921 |  112 | `	SyZero(pMeth,sizeof(ph7_class_method));` |
|        - |  113 | `	/* Check for an already installed method with the same name */` |
|  3664921 |  114 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
|  3664921 |  115 | `	if( pEntry == 0 ){` |
|        - |  116 | `		/* Associate an unique VM name to this method */` |
|  3664917 |  117 | `		nByte = sizeof(zSalt) + pName->nByte + SyStringLength(&pClass->sName)+sizeof(char)*7/*[[__'\0'*/;` |
|  3664917 |  118 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,nByte);` |
|  3664917 |  119 | `		if( zName == 0 ){` |
|      ! 0 |  120 | `			SyMemBackendPoolFree(&pVm->sAllocator,pMeth);` |
|      ! 0 |  121 | `			return 0;` |
|        - |  122 | `		}` |
|  3664917 |  123 | `		pNamePtr = &pMeth->sVmName;` |
|        - |  124 | `		/* Generate a random string */` |
|  3664917 |  125 | `		PH7_VmRandomString(&(*pVm),zSalt,sizeof(zSalt));` |
|  3664917 |  126 | `		pNamePtr->nByte = SyBufferFormat(zName,nByte,"[__%z@%z_%.*s]",&pClass->sName,pName,sizeof(zSalt),zSalt);` |
|  3664917 |  127 | `		pNamePtr->zString = zName;` |
|  1832461 |  128 | `	}else{` |
|        - |  129 | `		/* Method is condidate for 'overloading' */` |
|        6 |  130 | `		ph7_class_method *pCurrent = (ph7_class_method *)pEntry->pUserData;` |
|        6 |  131 | `		pNamePtr = &pMeth->sVmName;` |
|        - |  132 | `		/* Use the same VM name */` |
|        6 |  133 | `		SyStringDupPtr(pNamePtr,&pCurrent->sVmName);` |
|        6 |  134 | `		zName = (char *)pNamePtr->zString;` |
|        - |  135 | `	}` |
|  3664921 |  136 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|    70301 |  137 | `		if( pName->nByte == sizeof("__destruct") - 1 && SyMemcmp(pName->zString,"__destruct",sizeof("__destruct") - 1 ) == 0 ){` |
|        - |  138 | `				/* Switch to public visibility for destructors (the engine invokes them` |
|        - |  139 | `				 * internally, bypassing visibility either way). __construct KEEPS its` |
|        - |  140 | ``				 * declared visibility (band A #4): php enforces it at `new` — a`` |
|        - |  141 | `				 * private/protected ctor from the wrong scope is a catchable Error,` |
|        - |  142 | `				 * checked at OP_NEW — and ReflectionClass::isInstantiable()/` |
|        - |  143 | `				 * newInstance() now see it. A method named like the class is a PLAIN` |
|        - |  144 | `				 * method (PHP-4 constructors removed in 8.0), so it keeps its declared` |
|        - |  145 | `				 * visibility too — no longer forced public. */` |
|      ! 0 |  146 | `				iProtection = PH7_CLASS_PROT_PUBLIC;` |
|      ! 0 |  147 | `		}` |
|    35148 |  148 | `	}` |
|        - |  149 | `	/* Initialize method fields */` |
|  3664921 |  150 | `	pMeth->iProtection = iProtection;` |
|  3664921 |  151 | `	pMeth->iFlags = iFlags;` |
|  3664921 |  152 | `	pMeth->nLine = nLine;` |
|  5497379 |  153 | `	PH7_VmInitFuncState(&(*pVm),&pMeth->sFunc,&zName[sizeof(char)*4/*[__@*/+SyStringLength(&pClass->sName)],` |
|  3664916 |  154 | `		pName->nByte,iFuncFlags\|VM_FUNC_CLASS_METHOD,pClass);` |
|  3664921 |  155 | `	return pMeth;` |
|  1832463 |  156 | `}` |
|        - |  157 | `/*` |
|        - |  158 | ` * Check if the given name have a class method associated with it.` |
|        - |  159 | ` * Return the desired method [i.e: ph7_class_method instance] on success. NULL otherwise.` |
|        - |  160 | ` */` |
|  6971655 |  161 | `PH7_PRIVATE ph7_class_method * PH7_ClassExtractMethod(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|        5 |  162 | `{` |
|        - |  163 | `	SyHashEntry *pEntry;` |
|        - |  164 | `	/* Perform a hash lookup */` |
|  6971660 |  165 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)zName,nByte);` |
|  6971660 |  166 | `	if( pEntry == 0 ){` |
|        - |  167 | `		/* No such entry */` |
|  1774799 |  168 | `		return 0;` |
|        - |  169 | `	}` |
|        - |  170 | `	/* Point to the desired method */` |
|  5196866 |  171 | `	return (ph7_class_method *)pEntry->pUserData;` |
|  3485834 |  172 | `}` |
|        - |  173 | `/*` |
|        - |  174 | ` * Check if the given name is a class attribute.` |
|        - |  175 | ` * Return the desired attribute [i.e: ph7_class_attr instance] on success.NULL otherwise.` |
|        - |  176 | ` */` |
|   102320 |  177 | `PH7_PRIVATE ph7_class_attr * PH7_ClassExtractAttribute(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|        5 |  178 | `{` |
|        - |  179 | `	SyHashEntry *pEntry;` |
|        - |  180 | `	/* Perform a hash lookup */` |
|   102325 |  181 | `	pEntry = SyHashGet(&pClass->hAttr,(const void *)zName,nByte);` |
|   102325 |  182 | `	if( pEntry == 0 ){` |
|        - |  183 | `		/* No such entry */` |
|     2057 |  184 | `		return 0;` |
|        - |  185 | `	}` |
|        - |  186 | `	/* Point to the desierd method */` |
|   100273 |  187 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|    51165 |  188 | `}` |
|        - |  189 | `/*` |
|        - |  190 | ` * Check if the given name is a class CONSTANT (or enum case).` |
|        - |  191 | ` * php keeps constants and properties in separate namespaces, so constants live` |
|        - |  192 | ` * in a dedicated table (hConst) and never collide with a same-named property.` |
|        - |  193 | ` * Return the desired constant [ph7_class_attr with PH7_CLASS_ATTR_CONSTANT] on` |
|        - |  194 | ` * success, NULL otherwise.` |
|        - |  195 | ` */` |
|     1490 |  196 | `PH7_PRIVATE ph7_class_attr * PH7_ClassExtractConstant(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|        5 |  197 | `{` |
|        - |  198 | `	SyHashEntry *pEntry;` |
|     1495 |  199 | `	pEntry = SyHashGet(&pClass->hConst,(const void *)zName,nByte);` |
|     1495 |  200 | `	if( pEntry == 0 ){` |
|      475 |  201 | `		return 0;` |
|        - |  202 | `	}` |
|     1025 |  203 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|      750 |  204 | `}` |
|        - |  205 | `/*` |
|        - |  206 | ` * Install a class attribute in the corresponding container.` |
|        - |  207 | ` * A constant (or enum case) goes to hConst, a property to hAttr — php's two` |
|        - |  208 | `` * separate member namespaces, so `const C` and `public $C` coexist.`` |
|        - |  209 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|        - |  210 | ` */` |
|  1221440 |  211 | `PH7_PRIVATE sxi32 PH7_ClassInstallAttr(ph7_class *pClass,ph7_class_attr *pAttr)` |
|        5 |  212 | `{` |
|  1221445 |  213 | `	SyString *pName = &pAttr->sName;` |
|        - |  214 | `	sxi32 rc;` |
|        - |  215 | `	/* Remember where this attribute was originally declared so that later` |
|        - |  216 | `	 * inheritance/trait copies still know the declaring class (needed for` |
|        - |  217 | `	 * PHP-compatible error messages on typed properties). */` |
|  1221445 |  218 | `	if( pAttr->pDeclClass == 0 ){` |
|    11915 |  219 | `		pAttr->pDeclClass = pClass;` |
|     5955 |  220 | `	}` |
|  1221445 |  221 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|   420879 |  222 | `		rc = SyHashInsertTail(&pClass->hConst,(const void *)pName->zString,pName->nByte,pAttr);` |
|   210442 |  223 | `	}else{` |
|   800571 |  224 | `		rc = SyHashInsertTail(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|        - |  225 | `	}` |
|  1221445 |  226 | `	return rc;` |
|        5 |  227 | `}` |
|        - |  228 | `/*` |
|        - |  229 | ` * Install a class method in the corresponding container.` |
|        - |  230 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|        - |  231 | ` */` |
|  3664878 |  232 | `PH7_PRIVATE sxi32 PH7_ClassInstallMethod(ph7_class *pClass,ph7_class_method *pMeth)` |
|        5 |  233 | `{` |
|  3664883 |  234 | `	SyString *pName = &pMeth->sFunc.sName;` |
|        - |  235 | `	sxi32 rc;` |
|  3664883 |  236 | `	rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|  3664883 |  237 | `	return rc;` |
|        5 |  238 | `}` |
|        - |  239 | `/*` |
|        - |  240 | ` * Method-override compatibility (variance) checking.` |
|        - |  241 | ` *` |
|        - |  242 | ` * PHP rejects an override whose signature is incompatible with the parent's:` |
|        - |  243 | ` * return types are covariant (child may only narrow), parameter types are` |
|        - |  244 | ` * contravariant (child may only widen), and a child may not add a required` |
|        - |  245 | ` * parameter. We add the diagnostic — but conservatively: PHL must keep running` |
|        - |  246 | ` * valid PHP, so the comparator below is SKIP-BY-DEFAULT. It flags only cases that` |
|        - |  247 | ` * are unambiguously invalid and silently accepts anything subtle (unions,` |
|        - |  248 | ` * intersections, pseudo-types, self/parent/static, object, unresolved classes,` |
|        - |  249 | ` * or a missing type), so it can never reject valid code.` |
|        - |  250 | ` */` |
|        - |  251 | `#define OVT_NONE   0  /* no declared type */` |
|        - |  252 | `#define OVT_SCALAR 1  /* a concrete invariant scalar: int/float/string/bool/array */` |
|        - |  253 | `#define OVT_CLASS  2  /* a real, already-loaded class/interface */` |
|        - |  254 | `#define OVT_SKIP   3  /* union/intersection/pseudo/self/object/unresolved — never flag */` |
|        - |  255 |  |
|        - |  256 | `/*` |
|        - |  257 | ` * Classify one declared type (nType + class name + union flag) for override` |
|        - |  258 | ` * comparison. On OVT_CLASS, *ppClass receives the resolved class. Class names are` |
|        - |  259 | ` * resolved by a direct, autoload-free hClass lookup: a miss (forward reference,` |
|        - |  260 | ` * namespaced, or not-yet-loaded) yields OVT_SKIP, which the caller accepts.` |
|        - |  261 | ` */` |
|      340 |  262 | `static int OoClassifyOverrideType(ph7_vm *pVm, sxu32 nType, const SyString *pClass,` |
|        - |  263 | `	int bUnion, ph7_class **ppClass)` |
|        5 |  264 | `{` |
|      345 |  265 | `	*ppClass = 0;` |
|      345 |  266 | `	if( bUnion ){` |
|        3 |  267 | `		return OVT_SKIP; /* union/intersection — full lattice, skip */` |
|        - |  268 | `	}` |
|      343 |  269 | `	if( nType == 0 ){` |
|      184 |  270 | `		return OVT_NONE; /* no declared type */` |
|        - |  271 | `	}` |
|      163 |  272 | `	if( nType == SXU32_HIGH ){` |
|        - |  273 | `		/* A class name OR a pseudo-type stored as a name atom. Skip every pseudo` |
|        - |  274 | `		 * (incl. self/parent/static, which are context-relative). */` |
|        - |  275 | `		static const struct { const char *z; sxu32 n; } aPseudo[] = {` |
|        - |  276 | `			{"mixed",5}, {"never",5}, {"iterable",8}, {"callable",8}, {"true",4},` |
|        - |  277 | `			{"false",5}, {"self",4}, {"parent",6}, {"static",6}` |
|        - |  278 | `		};` |
|       27 |  279 | `		const char *z = pClass->zString;` |
|       27 |  280 | `		sxu32 n = pClass->nByte;` |
|        - |  281 | `		SyHashEntry *pE;` |
|        - |  282 | `		sxu32 i;` |
|      199 |  283 | `		for( i = 0; i < SX_ARRAYSIZE(aPseudo); i++ ){` |
|      183 |  284 | `			if( n == aPseudo[i].n && SyStrnmicmp(z,aPseudo[i].z,n) == 0 ){` |
|       10 |  285 | `				return OVT_SKIP;` |
|        - |  286 | `			}` |
|       89 |  287 | `		}` |
|       19 |  288 | `		pE = SyHashGet(&pVm->hClass,(const void *)z,n);` |
|       19 |  289 | `		if( pE == 0 ){` |
|      ! 0 |  290 | `			return OVT_SKIP; /* not loaded / forward ref / namespaced — accept */` |
|        - |  291 | `		}` |
|       19 |  292 | `		*ppClass = (ph7_class *)pE->pUserData;` |
|       19 |  293 | `		return OVT_CLASS;` |
|        - |  294 | `	}` |
|      134 |  295 | `	if( nType == MEMOBJ_STRING \|\| nType == MEMOBJ_INT \|\| nType == MEMOBJ_REAL` |
|       49 |  296 | `	 \|\| nType == MEMOBJ_BOOL \|\| nType == MEMOBJ_HASHMAP ){` |
|       94 |  297 | `		return OVT_SCALAR;` |
|        - |  298 | `	}` |
|        - |  299 | `	/* MEMOBJ_OBJ (object — subtypes against classes), MEMOBJ_VOID/NULL/RES,` |
|        - |  300 | `	 * or anything unexpected: skip. */` |
|       47 |  301 | `	return OVT_SKIP;` |
|      175 |  302 | `}` |
|        - |  303 |  |
|        - |  304 | `/*` |
|        - |  305 | ` * A declared type normalized for override comparison: the raw type code, the` |
|        - |  306 | ` * class-name string (when a class), and the union/nullable flags. Extracted once` |
|        - |  307 | ` * from each side so the comparator takes two of these instead of eight scalars.` |
|        - |  308 | ` */` |
|        - |  309 | `typedef struct OvType OvType;` |
|        - |  310 | `struct OvType {` |
|        - |  311 | `	sxu32 nType;` |
|        - |  312 | `	const SyString *pClass;` |
|        - |  313 | `	int bUnion;` |
|        - |  314 | `	int bNullable;` |
|        - |  315 | `};` |
|      244 |  316 | `static OvType OoTypeFromReturn(ph7_vm_func *pF)` |
|        5 |  317 | `{` |
|        - |  318 | `	OvType t;` |
|      249 |  319 | `	t.nType = pF->nReturnType;` |
|      249 |  320 | `	t.pClass = &pF->sReturnClass;` |
|      249 |  321 | `	t.bUnion = SySetUsed(&pF->aReturnUnion) > 0;` |
|      249 |  322 | `	t.bNullable = (pF->iFlags & VM_FUNC_RETURN_NULLABLE) != 0;` |
|      249 |  323 | `	return t;` |
|        5 |  324 | `}` |
|       96 |  325 | `static OvType OoTypeFromArg(ph7_vm_func_arg *pA)` |
|        3 |  326 | `{` |
|        - |  327 | `	OvType t;` |
|       99 |  328 | `	t.nType = pA->nType;` |
|       99 |  329 | `	t.pClass = &pA->sClass;` |
|       99 |  330 | `	t.bUnion = (pA->iFlags & VM_FUNC_ARG_UNION) != 0;` |
|       99 |  331 | `	t.bNullable = (pA->iFlags & VM_FUNC_ARG_NULLABLE) != 0;` |
|       99 |  332 | `	return t;` |
|        3 |  333 | `}` |
|        - |  334 | `/*` |
|        - |  335 | ` * Return TRUE if the child type is an unambiguously-invalid override of the` |
|        - |  336 | ` * parent type. bCovariant=1 for a return type (child must be ⊆ parent),` |
|        - |  337 | ` * 0 for a parameter (child must be ⊇ parent). Returns FALSE (accept) on any` |
|        - |  338 | ` * skipped/ambiguous shape.` |
|        - |  339 | ` */` |
|      170 |  340 | `static int OoOverrideTypeBad(ph7_vm *pVm, OvType parent, OvType child, int bCovariant)` |
|        5 |  341 | `{` |
|        - |  342 | `	ph7_class *pParentCls, *pChildCls;` |
|      175 |  343 | `	int kP = OoClassifyOverrideType(pVm, parent.nType, parent.pClass, parent.bUnion, &pParentCls);` |
|      175 |  344 | `	int kC = OoClassifyOverrideType(pVm, child.nType, child.pClass, child.bUnion, &pChildCls);` |
|      175 |  345 | `	if( kP == OVT_SKIP \|\| kC == OVT_SKIP ){` |
|       31 |  346 | `		return 0; /* ambiguous shape — conservatively accept */` |
|        - |  347 | `	}` |
|        - |  348 | `	/* A missing type is the TOP type. covariant (return): a concrete child is a` |
|        - |  349 | `	 * subtype of top, fine; a top child over a concrete parent WIDENS → bad.` |
|        - |  350 | `	 * contravariant (param): a top child is a supertype of anything, fine; a` |
|        - |  351 | `	 * concrete child over a top parent NARROWS → bad. (A union/intersection child` |
|        - |  352 | `	 * already fell into OVT_SKIP above, so a flagged child here is scalar/class.) */` |
|      147 |  353 | `	if( kP == OVT_NONE \|\| kC == OVT_NONE ){` |
|       96 |  354 | `		if( bCovariant && kC == OVT_NONE && kP != OVT_NONE ) return 1;` |
|       96 |  355 | `		if( !bCovariant && kP == OVT_NONE && kC != OVT_NONE ) return 1;` |
|       96 |  356 | `		return 0;` |
|        - |  357 | `	}` |
|        - |  358 | `	/* Nullability: a covariant return may not ADD null; a contravariant param may` |
|        - |  359 | `	 * not REMOVE null. */` |
|       55 |  360 | `	if( bCovariant ){` |
|       34 |  361 | `		if( child.bNullable && !parent.bNullable ) return 1;` |
|       19 |  362 | `	}else{` |
|       23 |  363 | `		if( parent.bNullable && !child.bNullable ) return 1;` |
|        - |  364 | `	}` |
|       55 |  365 | `	if( kP == OVT_SCALAR && kC == OVT_SCALAR ){` |
|        - |  366 | `		/* Scalars are invariant — they must match exactly. */` |
|       46 |  367 | `		return (parent.nType != child.nType) ? 1 : 0;` |
|        - |  368 | `	}` |
|       11 |  369 | `	if( kP == OVT_CLASS && kC == OVT_CLASS ){` |
|       11 |  370 | `		if( bCovariant ){` |
|        6 |  371 | `			return PH7_VmInstanceOf(pChildCls, pParentCls) ? 0 : 1;  /* child ⊆ parent */` |
|        - |  372 | `		}` |
|        6 |  373 | `		return PH7_VmInstanceOf(pParentCls, pChildCls) ? 0 : 1;      /* child ⊇ parent */` |
|        - |  374 | `	}` |
|        - |  375 | `	/* One scalar and one class — disjoint. */` |
|      ! 0 |  376 | `	return 1;` |
|       90 |  377 | `}` |
|        - |  378 |  |
|        - |  379 | `/*` |
|        - |  380 | ` * Check a child method's signature against the parent method it overrides.` |
|        - |  381 | ` * Emits a PHP-style "Declaration of … must be compatible …" fatal on a clear` |
|        - |  382 | `` * incompatibility. `__construct` is exempt (PHP does not apply variance to it).`` |
|        - |  383 | ` */` |
|   233686 |  384 | `static sxi32 OoCheckOverrideCompat(ph7_gen_state *pGen, ph7_class *pBase, ph7_class *pSub,` |
|        - |  385 | `	ph7_class_method *pParent, ph7_class_method *pChild)` |
|        5 |  386 | `{` |
|   233691 |  387 | `	ph7_vm *pVm = pGen->pVm;` |
|   233691 |  388 | `	ph7_vm_func *pPF = &pParent->sFunc;` |
|   233691 |  389 | `	ph7_vm_func *pCF = &pChild->sFunc;` |
|   233691 |  390 | `	SyString *pMName = &pCF->sName;` |
|        - |  391 | `	ph7_vm_func_arg *aP, *aC;` |
|        - |  392 | `	sxu32 nPArg, nCArg, k;` |
|   233691 |  393 | `	int bBad = 0;` |
|   233686 |  394 | `	if( pMName->nByte == sizeof("__construct")-1` |
|   158893 |  395 | `	 && SyStrnmicmp(pMName->zString,"__construct",pMName->nByte) == 0 ){` |
|    74741 |  396 | `		return SXRET_OK;` |
|        - |  397 | `	}` |
|        - |  398 | `	/*` |
|        - |  399 | `	 * A NATIVE method declares its parameters in a zSig STRING, so its aArgs set` |
|        - |  400 | `	 * is empty and there is nothing here to compare against. Reading that as` |
|        - |  401 | `	 * "declares no parameters" made every override of one incompatible: a user` |
|        - |  402 | `	 * class extending DOMDocument, a Reflection class or a native enum's own` |
|        - |  403 | `	 * cases()/from() all fataled on a declaration php accepts. An` |
|        - |  404 | `	 * engine-declared signature is compatible by construction, on either side.` |
|        - |  405 | `	 */` |
|   158955 |  406 | `	if( ((pPF->iFlags \| pCF->iFlags) & VM_FUNC_NATIVE) != 0 ){` |
|   158833 |  407 | `		return SXRET_OK;` |
|        - |  408 | `	}` |
|        - |  409 | `	/* Return type — covariant. */` |
|      127 |  410 | `	bBad = OoOverrideTypeBad(pVm, OoTypeFromReturn(pPF), OoTypeFromReturn(pCF), /* bCovariant */ 1);` |
|        - |  411 | `	/* Each overlapping parameter — contravariant. */` |
|      127 |  412 | `	nPArg = SySetUsed(&pPF->aArgs);` |
|      127 |  413 | `	nCArg = SySetUsed(&pCF->aArgs);` |
|      127 |  414 | `	aP = (ph7_vm_func_arg *)SySetBasePtr(&pPF->aArgs);` |
|      127 |  415 | `	aC = (ph7_vm_func_arg *)SySetBasePtr(&pCF->aArgs);` |
|      175 |  416 | `	for( k = 0; !bBad && k < nPArg && k < nCArg; k++ ){` |
|       51 |  417 | `		bBad = OoOverrideTypeBad(pVm, OoTypeFromArg(&aP[k]), OoTypeFromArg(&aC[k]), /* bCovariant */ 0);` |
|       27 |  418 | `	}` |
|        - |  419 | `	/* Parameter arity: the child must declare at least the parent's parameters and` |
|        - |  420 | `	 * may add only OPTIONAL ones — PHP rejects dropping any param (even an optional` |
|        - |  421 | `	 * one) or adding a required one. Skip the rule if either signature is variadic` |
|        - |  422 | `	 * (arity semantics differ). */` |
|      127 |  423 | `	if( !bBad ){` |
|      122 |  424 | `		int bVariadic = 0;` |
|      168 |  425 | `		for( k = 0; k < nPArg; k++ ){ if( aP[k].iFlags & VM_FUNC_ARG_VARIADIC ) bVariadic = 1; }` |
|      170 |  426 | `		for( k = 0; k < nCArg; k++ ){ if( aC[k].iFlags & VM_FUNC_ARG_VARIADIC ) bVariadic = 1; }` |
|      122 |  427 | `		if( !bVariadic ){` |
|      122 |  428 | `			if( nCArg < nPArg ){` |
|      ! 0 |  429 | `				bBad = 1; /* dropped a parent parameter */` |
|      ! 0 |  430 | `			}else{` |
|      124 |  431 | `				for( k = nPArg; k < nCArg; k++ ){` |
|        3 |  432 | `					if( SySetUsed(&aC[k].aByteCode) == 0 ){ bBad = 1; break; } /* new required */` |
|        2 |  433 | `				}` |
|        - |  434 | `			}` |
|       59 |  435 | `		}` |
|       59 |  436 | `	}` |
|      127 |  437 | `	if( bBad ){` |
|        8 |  438 | `		sxi32 rc = PH7_GenCompileError(&(*pGen),E_ERROR,pChild->nLine,` |
|        - |  439 | `			"Declaration of %z::%z() must be compatible with %z::%z()",` |
|        2 |  440 | `			&pSub->sName,pMName,&pBase->sName,&pParent->sFunc.sName);` |
|        6 |  441 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  442 | `			return SXERR_ABORT;` |
|        - |  443 | `		}` |
|        2 |  444 | `	}` |
|      127 |  445 | `	return SXRET_OK;` |
|   116848 |  446 | `}` |
|        - |  447 | `/*` |
|        - |  448 | ` * Perform an inheritance operation.` |
|        - |  449 | ` * According to the PHP language reference manual` |
|        - |  450 | ` *  When you extend a class, the subclass inherits all of the public and protected methods` |
|        - |  451 | ` *  from the parent class. Unless a class Overwrites those methods, they will retain their original` |
|        - |  452 | ` *  functionality.` |
|        - |  453 | ` *  This is useful for defining and abstracting functionality, and permits the implementation` |
|        - |  454 | ` *  of additional functionality in similar objects without the need to reimplement all of the shared` |
|        - |  455 | ` *  functionality.` |
|        - |  456 | ` *  Example #1 Inheritance Example` |
|        - |  457 | ` * <?php` |
|        - |  458 | ` * class foo` |
|        - |  459 | ` * {` |
|        - |  460 | ` *   public function printItem($string)` |
|        - |  461 | ` *   {` |
|        - |  462 | ` *       echo 'Foo: ' . $string . PHP_EOL;` |
|        - |  463 | ` *   }` |
|        - |  464 | ` *` |
|        - |  465 | ` *   public function printPHP()` |
|        - |  466 | ` *   {` |
|        - |  467 | ` *       echo 'PHP is great.' . PHP_EOL;` |
|        - |  468 | ` *   }` |
|        - |  469 | ` * }` |
|        - |  470 | ` * class bar extends foo` |
|        - |  471 | ` * {` |
|        - |  472 | ` *   public function printItem($string)` |
|        - |  473 | ` *   {` |
|        - |  474 | ` *       echo 'Bar: ' . $string . PHP_EOL;` |
|        - |  475 | ` *   }` |
|        - |  476 | ` * }` |
|        - |  477 | ` * $foo = new foo();` |
|        - |  478 | ` * $bar = new bar();` |
|        - |  479 | ` * $foo->printItem('baz'); // Output: 'Foo: baz'` |
|        - |  480 | ` * $foo->printPHP();       // Output: 'PHP is great'` |
|        - |  481 | ` * $bar->printItem('baz'); // Output: 'Bar: baz'` |
|        - |  482 | ` * $bar->printPHP();       // Output: 'PHP is great'` |
|        - |  483 | ` *` |
|        - |  484 | ` * This function return SXRET_OK if the inheritance operation was successfully performed.` |
|        - |  485 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|        - |  486 | ` * error message.` |
|        - |  487 | ` */` |
|   304106 |  488 | `PH7_PRIVATE sxi32 PH7_ClassInherit(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase)` |
|        5 |  489 | `{` |
|        - |  490 | `	ph7_class_method *pMeth;` |
|        - |  491 | `	ph7_class_attr *pAttr;` |
|        - |  492 | `	SyHashEntry *pEntry;` |
|        - |  493 | `	SyString *pName;` |
|        - |  494 | `	SySet aInherited; /* base attributes to prepend (see the copy loop below) */` |
|        - |  495 | `	sxi32 rc;` |
|   304111 |  496 | `	SySetInit(&aInherited,&pGen->pVm->sAllocator,sizeof(ph7_class_attr *));` |
|        - |  497 | `	/* Install in the derived hashtable */` |
|   304111 |  498 | `	rc = SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|   304111 |  499 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  500 | `		SySetRelease(&aInherited);` |
|      ! 0 |  501 | `		return rc;` |
|        - |  502 | `	}` |
|        - |  503 | `	/* readonly class inheritance (PHP 8.2): a readonly class may only extend a` |
|        - |  504 | `	 * readonly class, and a non-readonly class may not extend a readonly one. */` |
|   304111 |  505 | `	if( (pBase->iFlags & PH7_CLASS_READONLY) != (pSub->iFlags & PH7_CLASS_READONLY) ){` |
|        5 |  506 | `		if( pBase->iFlags & PH7_CLASS_READONLY ){` |
|        4 |  507 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|        - |  508 | `				"Non-readonly class %z cannot extend readonly class %z",` |
|        1 |  509 | `				&pSub->sName,&pBase->sName);` |
|        2 |  510 | `		}else{` |
|        4 |  511 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|        - |  512 | `				"Readonly class %z cannot extend non-readonly class %z",` |
|        1 |  513 | `				&pSub->sName,&pBase->sName);` |
|        - |  514 | `		}` |
|        5 |  515 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  516 | `			SySetRelease(&aInherited);` |
|      ! 0 |  517 | `			return SXERR_ABORT;` |
|        - |  518 | `		}` |
|        2 |  519 | `	}` |
|        - |  520 | `	/* Copy public/protected attributes from the base class */` |
|   304111 |  521 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
|  1902377 |  522 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|        - |  523 | `		/* Make sure the private attributes are not redeclared in the subclass */` |
|  1598271 |  524 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  1598271 |  525 | `		pName = &pAttr->sName;` |
|  1598271 |  526 | `		if( (pEntry = SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|     9366 |  527 | `			if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_FINAL))` |
|     4688 |  528 | `				== (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_FINAL) ){` |
|        - |  529 | `				/* Cannot override a final class constant (PHP 8.1). Report the` |
|        - |  530 | `				 * class that originally declared it (pDeclClass) rather than the` |
|        - |  531 | `				 * immediate base, so a multi-level chain matches PHP. */` |
|      ! 0 |  532 | `				ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pBase;` |
|      ! 0 |  533 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_attr *)pEntry->pUserData)->nLine,` |
|        - |  534 | `					"%z::%z cannot override final constant %z::%z",` |
|      ! 0 |  535 | `					&pSub->sName,pName,&pOwner->sName,pName);` |
|      ! 0 |  536 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  537 | `					SySetRelease(&aInherited);` |
|      ! 0 |  538 | `					return SXERR_ABORT;` |
|        - |  539 | `				}` |
|      ! 0 |  540 | `			}` |
|        - |  541 | `			/* A child MAY redeclare a base's private property: php treats the two` |
|        - |  542 | `			 * as independent members (each private to its declaring class), with no` |
|        - |  543 | `			 * diagnostic. PH7 warned here, which is wrong — the child's entry simply` |
|        - |  544 | `			 * shadows the base's in the by-name attribute table.` |
|        - |  545 | `			 *` |
|        - |  546 | `			 * Ordering: php keeps an overridden INSTANCE property at the position` |
|        - |  547 | `` 			 * the BASE declared it (`class G{$g1;$g2;} class H extends G{$h;$g1;}` `` |
|        - |  548 | `			 * iterates g1,g2,h — not g2,h,g1). Re-collect the CHILD's definition,` |
|        - |  549 | `			 * whose default value wins, and drop its current entry so the prepend` |
|        - |  550 | `			 * below re-inserts it in base order. Statics/constants are not part of` |
|        - |  551 | `			 * instance iteration, so they keep their existing slot. */` |
|     9371 |  552 | `			if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|     9369 |  553 | `				ph7_class_attr *pOwn = (ph7_class_attr *)pEntry->pUserData;` |
|     9369 |  554 | `				SyHashDeleteEntry(&pSub->hAttr,(const void *)pName->zString,pName->nByte,0);` |
|     9369 |  555 | `				rc = SySetPut(&aInherited,(const void *)&pOwn);` |
|     9369 |  556 | `				if( rc != SXRET_OK ){` |
|      ! 0 |  557 | `					SySetRelease(&aInherited);` |
|      ! 0 |  558 | `					return rc;` |
|        - |  559 | `				}` |
|     4682 |  560 | `			}` |
|     9371 |  561 | `			continue;` |
|        - |  562 | `		}` |
|        - |  563 | `		/* Collect the attribute. php: a base class's private INSTANCE property` |
|        - |  564 | `		 * lives on every child instance too (its own methods read/write it` |
|        - |  565 | `		 * through $this on the child; the access check grants private access by` |
|        - |  566 | `		 * DECLARING class, so child methods and outsiders still can't touch it).` |
|        - |  567 | `		 * Private STATICS/CONSTANTS stay uncopied — base methods reach those` |
|        - |  568 | `		 * through self:: against the declaring class directly.` |
|        - |  569 | `		 *` |
|        - |  570 | `		 * These are gathered rather than installed here because php orders an` |
|        - |  571 | `		 * instance's properties BASE-DECLARED FIRST, then the subclass's own,` |
|        - |  572 | `		 * then trait members — while inheritance runs AFTER the subclass body` |
|        - |  573 | `		 * has already filled hAttr. They are prepended below. */` |
|  1588900 |  574 | `		if( pAttr->iProtection != PH7_CLASS_PROT_PRIVATE` |
|  1205709 |  575 | `		 \|\| (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|  1588901 |  576 | `			rc = SySetPut(&aInherited,(const void *)&pAttr);` |
|  1588901 |  577 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  578 | `				SySetRelease(&aInherited);` |
|      ! 0 |  579 | `				return rc;` |
|        - |  580 | `			}` |
|   794448 |  581 | `		}` |
|        5 |  582 | `	}` |
|        - |  583 | `	/* Prepend the collected base attributes so hAttr reads base-first. The` |
|        - |  584 | `	 * table's iteration list is what every object-iteration consumer walks` |
|        - |  585 | `	 * (var_dump/print_r, get_object_vars, foreach, (array) casts, json_encode),` |
|        - |  586 | `	 * so this ordering is user-visible — json_encode emits its keys in exactly` |
|        - |  587 | `	 * this order. SyHashInsert is a HEAD insert, so walking the collected set` |
|        - |  588 | `	 * backwards leaves the base's own declaration order at the front. */` |
|   304111 |  589 | `	if( SySetUsed(&aInherited) > 0 ){` |
|   303851 |  590 | `		ph7_class_attr **apInherited = (ph7_class_attr **)SySetBasePtr(&aInherited);` |
|   303851 |  591 | `		sxu32 n = SySetUsed(&aInherited);` |
|  1902111 |  592 | `		while( n > 0 ){` |
|  1598265 |  593 | `			ph7_class_attr *pIn = apInherited[--n];` |
|  1598265 |  594 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pIn->sName.zString,pIn->sName.nByte,pIn);` |
|  1598265 |  595 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  596 | `				SySetRelease(&aInherited);` |
|      ! 0 |  597 | `				return rc;` |
|        - |  598 | `			}` |
|        5 |  599 | `		}` |
|   151923 |  600 | `	}` |
|        - |  601 | `	/* Inherit the base's CONSTANTS (separate namespace: hConst). Constants are not` |
|        - |  602 | `	 * part of instance iteration, so no base-first ordering dance is needed — a plain` |
|        - |  603 | `	 * copy of every constant the subclass did not itself redeclare. A subclass that` |
|        - |  604 | `	 * redeclares a base FINAL constant is a fatal (PHP 8.1). */` |
|   304111 |  605 | `	SyHashResetLoopCursor(&pBase->hConst);` |
|   500549 |  606 | `	while((pEntry = SyHashGetNextEntry(&pBase->hConst)) != 0 ){` |
|        - |  607 | `		SyHashEntry *pOwn;` |
|   196443 |  608 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   196443 |  609 | `		pName = &pAttr->sName;` |
|   196443 |  610 | `		if( (pOwn = SyHashGet(&pSub->hConst,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|        6 |  611 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_FINAL) ){` |
|        - |  612 | `				/* Cannot override a final class constant. Report the class that` |
|        - |  613 | `				 * originally declared it (pDeclClass) for a multi-level chain. */` |
|        3 |  614 | `				ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pBase;` |
|        4 |  615 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_attr *)pOwn->pUserData)->nLine,` |
|        - |  616 | `					"%z::%z cannot override final constant %z::%z",` |
|        1 |  617 | `					&pSub->sName,pName,&pOwner->sName,pName);` |
|        3 |  618 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  619 | `					SySetRelease(&aInherited);` |
|      ! 0 |  620 | `					return SXERR_ABORT;` |
|        - |  621 | `				}` |
|        1 |  622 | `			}` |
|        6 |  623 | `			continue;` |
|        - |  624 | `		}` |
|   196439 |  625 | `		rc = SyHashInsertTail(&pSub->hConst,(const void *)pName->zString,pName->nByte,pAttr);` |
|   196439 |  626 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  627 | `			SySetRelease(&aInherited);` |
|      ! 0 |  628 | `			return rc;` |
|        - |  629 | `		}` |
|        5 |  630 | `	}` |
|   304111 |  631 | `	SySetRelease(&aInherited);` |
|   304111 |  632 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
|  4688117 |  633 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|        - |  634 | `		SyHashEntry *pOwn;` |
|        - |  635 | `		SyString sKey;` |
|        - |  636 | `		/* Make sure the private/final methods are not redeclared in the subclass.` |
|        - |  637 | `		 * The identity inherited is the base's HASH KEY, not sFunc.sName — the same` |
|        - |  638 | `		 * rule PH7_ClassUseTrait copies a trait by. A trait adaptation leaves the` |
|        - |  639 | `		 * composed class holding entries whose key is the name the class ANSWERS to` |
|        - |  640 | ``		 * while the method struct keeps its original name: `B::m as mB` is the key`` |
|        - |  641 | `` 		 * `mB` over a struct still called `m`, and `A::m insteadof B` is the key `m` `` |
|        - |  642 | ``		 * over A's struct. Keying the copy off sFunc.sName re-filed both under `m`,`` |
|        - |  643 | `		 * so a subclass of the composing class lost the alias entirely and took` |
|        - |  644 | ``		 * whichever of the two the hash walk reached last as its `m` — the insteadof`` |
|        - |  645 | `		 * choice, silently reversed. */` |
|  4384011 |  646 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  4384011 |  647 | `		SyStringInitFromBuf(&sKey,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|  4384011 |  648 | `		pName = &sKey;` |
|  4384011 |  649 | `		if( (pOwn = SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|   233703 |  650 | `			if( pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|        - |  651 | `				/* php: a base's PRIVATE method is never overridden — the child's` |
|        - |  652 | `				 * declaration is an independent member of the same name, so neither` |
|        - |  653 | `				 * the final rule nor the signature-compatibility rule applies to it` |
|        - |  654 | `				 * (zend's do_inherit_method skips both for a private parent). PHL` |
|        - |  655 | ``				 * ran both: `class A { private function m($a){} } class B extends A`` |
|        - |  656 | ``				 * { private function m($a,$b,$c){} }` was a fatal php compiles, and`` |
|        - |  657 | ``				 * `final private` in the base fataled every child that reused the`` |
|        - |  658 | `				 * name — php only WARNS at the final-private DECLARATION and lets` |
|        - |  659 | `				 * the child have the name. */` |
|   233699 |  660 | `			}else if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|        - |  661 | `				/* php: "Cannot override final method A::test()" */` |
|        8 |  662 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_method *)pOwn->pUserData)->nLine,` |
|        - |  663 | `					"Cannot override final method %z::%z()",` |
|        2 |  664 | `					&pBase->sName,pName);` |
|        2 |  665 | `				(void)pSub;` |
|        6 |  666 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  667 | `					return SXERR_ABORT;` |
|        - |  668 | `				}` |
|        4 |  669 | `			}else{` |
|        - |  670 | `				/* Check the override's signature is compatible with the parent's. */` |
|   350534 |  671 | `				rc = OoCheckOverrideCompat(&(*pGen),pBase,pSub,pMeth,` |
|   233686 |  672 | `					(ph7_class_method *)pOwn->pUserData);` |
|   233691 |  673 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  674 | `					return SXERR_ABORT;` |
|        - |  675 | `				}` |
|        - |  676 | `			}` |
|   233703 |  677 | `			continue;` |
|        - |  678 | `		}` |
|        - |  679 | `		/* Install the method. php: a base class's private method is in the child's` |
|        - |  680 | `		 * table too — an inherited public method calling $this->priv() must find it,` |
|        - |  681 | ``		 * and the LOOKUP has to find it for php's answer to `B::p()` to be`` |
|        - |  682 | `		 * "Call to private method A::p() from global scope" rather than` |
|        - |  683 | `		 * "Call to undefined method B::p()". The call-site visibility check binds by` |
|        - |  684 | `		 * DECLARING class (sFunc.pUserData), so child code and outsiders still cannot` |
|        - |  685 | ``		 * reach it; a private ctor copied down blocks `new Child` from outside like`` |
|        - |  686 | `		 * php's; and the surfaces that must NOT show an inherited private say so` |
|        - |  687 | `		 * themselves (method_exists, get_class_methods, ReflectionClass::getMethods).` |
|        - |  688 | `		 *` |
|        - |  689 | `		 * STATIC privates used to be skipped here, on the reasoning that base methods` |
|        - |  690 | `		 * reach them through self:: against the declaring class anyway. They do — but` |
|        - |  691 | ``		 * nothing else could: every spelling of `B::p()` (the call, `['B','p']()`,`` |
|        - |  692 | `		 * call_user_func, a first-class callable) reported the name as UNDEFINED, and` |
|        - |  693 | ``		 * `static::p()` from the base with a subclass as the late-static-binding`` |
|        - |  694 | `		 * target could not find its own method. */` |
|  4150313 |  695 | `		rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|  4150313 |  696 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  697 | `			return rc;` |
|        - |  698 | `		}` |
|        5 |  699 | `	}` |
|        - |  700 | `	/* Mark as subclass */` |
|   304111 |  701 | `	pSub->pBase = pBase;` |
|        - |  702 | `	/* All done */` |
|   304111 |  703 | `	return SXRET_OK;` |
|   152058 |  704 | `}` |
|        - |  705 | `/*` |
|        - |  706 | ` * Apply a trait to a class: copy all methods and attributes from the trait` |
|        - |  707 | ` * into the target class. Unlike inheritance, traits copy ALL members including` |
|        - |  708 | ` * private ones. Members already defined in the class take precedence.` |
|        - |  709 | ` */` |
|      150 |  710 | `PH7_PRIVATE sxi32 PH7_ClassUseTrait(ph7_gen_state *pGen,ph7_class *pClass,ph7_class *pTrait)` |
|        5 |  711 | `{` |
|        - |  712 | `	ph7_class_method *pMeth;` |
|        - |  713 | `	ph7_class_attr *pAttr;` |
|        - |  714 | `	SyHashEntry *pEntry;` |
|        - |  715 | `	SyString *pName;` |
|        - |  716 | `	sxi32 rc;` |
|        - |  717 | `	/* Detect cyclic trait composition (e.g. trait A { use B; } trait B { use A; }) */` |
|      155 |  718 | `	if( pTrait->iFlags & PH7_CLASS_TRAIT_VISITING ){` |
|      ! 0 |  719 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|      ! 0 |  720 | `			"Trait circular reference detected: %z is already being applied",&pTrait->sName);` |
|      ! 0 |  721 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  722 | `			return SXERR_ABORT;` |
|        - |  723 | `		}` |
|      ! 0 |  724 | `		return SXRET_OK;` |
|        - |  725 | `	}` |
|      155 |  726 | `	pTrait->iFlags \|= PH7_CLASS_TRAIT_VISITING;` |
|      155 |  727 | `	rc = SXRET_OK;` |
|        - |  728 | `	/* Copy attributes from the trait */` |
|      155 |  729 | `	SyHashResetLoopCursor(&pTrait->hAttr);` |
|      185 |  730 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hAttr)) != 0 ){` |
|        - |  731 | `		SyHashEntry *pExisting;` |
|       35 |  732 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|       35 |  733 | `		pName = &pAttr->sName;` |
|       35 |  734 | `		pExisting = SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte);` |
|       35 |  735 | `		if( pExisting != 0 ){` |
|        - |  736 | `			/* Attribute already exists. Check if it came from another trait` |
|        - |  737 | `			 * and whether the definitions are compatible (same defaults).` |
|        - |  738 | `			 */` |
|        - |  739 | `			ph7_class **apUsedTraits;` |
|        - |  740 | `			sxu32 nUsed,k;` |
|        6 |  741 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|        6 |  742 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|        6 |  743 | `			for(k = 0; k < nUsed; k++){` |
|        - |  744 | `				ph7_class_attr *pOther;` |
|        3 |  745 | `				pOther = PH7_ClassExtractAttribute(apUsedTraits[k],pName->zString,pName->nByte);` |
|        3 |  746 | `				if( pOther ){` |
|        - |  747 | `					/* Two traits define the same property — check if defaults differ */` |
|        3 |  748 | `					ph7_class_attr *pClassAttr = (ph7_class_attr *)pExisting->pUserData;` |
|        4 |  749 | `					if( SySetUsed(&pAttr->aByteCode) != SySetUsed(&pClassAttr->aByteCode) \|\|` |
|        3 |  750 | `						(SySetUsed(&pAttr->aByteCode) > 0 &&` |
|        3 |  751 | `						 SyMemcmp(SySetBasePtr(&pAttr->aByteCode),SySetBasePtr(&pClassAttr->aByteCode),` |
|        3 |  752 | `							SySetUsed(&pAttr->aByteCode) * SySetElemSize(&pAttr->aByteCode)) != 0) ){` |
|        4 |  753 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine,` |
|        - |  754 | `							"%z and %z define the same property ($%z) in the composition of %z. "` |
|        - |  755 | `							"However, the definition differs and is considered incompatible",` |
|        2 |  756 | `							&apUsedTraits[k]->sName,&pTrait->sName,pName,&pClass->sName);` |
|        3 |  757 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 |  758 | `							goto cleanup;` |
|        - |  759 | `						}` |
|        1 |  760 | `					}` |
|        3 |  761 | `					break;` |
|        - |  762 | `				}` |
|      ! 0 |  763 | `			}` |
|        6 |  764 | `			continue;` |
|        - |  765 | `		}` |
|       31 |  766 | `		rc = SyHashInsertTail(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|       31 |  767 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  768 | `			goto cleanup;` |
|        - |  769 | `		}` |
|        5 |  770 | `	}` |
|        - |  771 | `	/* Copy constants from the trait (PHP 8.2 trait constants; separate hConst` |
|        - |  772 | `	 * namespace). A constant already present in the class wins silently. */` |
|      155 |  773 | `	SyHashResetLoopCursor(&pTrait->hConst);` |
|      155 |  774 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hConst)) != 0 ){` |
|      ! 0 |  775 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      ! 0 |  776 | `		pName = &pAttr->sName;` |
|      ! 0 |  777 | `		if( SyHashGet(&pClass->hConst,(const void *)pName->zString,pName->nByte) != 0 ){` |
|      ! 0 |  778 | `			continue;` |
|        - |  779 | `		}` |
|      ! 0 |  780 | `		rc = SyHashInsertTail(&pClass->hConst,(const void *)pName->zString,pName->nByte,pAttr);` |
|      ! 0 |  781 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  782 | `			goto cleanup;` |
|        - |  783 | `		}` |
|      ! 0 |  784 | `	}` |
|        - |  785 | `	/* Copy methods from the trait. The identity copied is the trait's HASH KEY,` |
|        - |  786 | ``	 * not sFunc.sName: an adaptation alias (`hi as bHi`) is a hash entry whose`` |
|        - |  787 | `	 * key is the alias while the method struct keeps its original name — keying` |
|        - |  788 | ``	 * the copy off sFunc.sName silently re-filed `bHi` under `hi`, so an alias`` |
|        - |  789 | `	 * made inside a TRAIT vanished when that trait was composed into a class. */` |
|      155 |  790 | `	SyHashResetLoopCursor(&pTrait->hMethod);` |
|      409 |  791 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hMethod)) != 0 ){` |
|        - |  792 | `		SyHashEntry *pClassMethEntry;` |
|        - |  793 | `		SyString sKey;` |
|      259 |  794 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      259 |  795 | `		SyStringInitFromBuf(&sKey,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|      259 |  796 | `		pName = &sKey;` |
|      259 |  797 | `		pClassMethEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
|      259 |  798 | `		if( pClassMethEntry != 0 ){` |
|        - |  799 | `			/* Method already exists in the class. An ABSTRACT trait method is a` |
|        - |  800 | `			 * REQUIREMENT, not an implementation: php satisfies it with any concrete` |
|        - |  801 | `			 * method of the same name (from the class body or another trait) — no` |
|        - |  802 | `			 * collision. Only two CONCRETE trait methods actually conflict. */` |
|       18 |  803 | `			ph7_class_method *pExistingMeth = (ph7_class_method *)pClassMethEntry->pUserData;` |
|       18 |  804 | `			int bIncomingAbstract = (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0;` |
|       18 |  805 | `			int bExistingAbstract = (pExistingMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0;` |
|        - |  806 | `			ph7_class **apUsedTraits;` |
|        - |  807 | `			sxu32 nUsed,k;` |
|       18 |  808 | `			if( bIncomingAbstract ){` |
|        - |  809 | `				/* Incoming abstract requirement: the existing (concrete or abstract)` |
|        - |  810 | `				 * method already covers this name — keep it. */` |
|       12 |  811 | `				continue;` |
|        - |  812 | `			}` |
|       11 |  813 | `			if( bExistingAbstract ){` |
|        - |  814 | `				/* Existing entry is only an abstract requirement (from an earlier` |
|        - |  815 | `				 * trait): the incoming concrete method satisfies and replaces it. */` |
|        3 |  816 | `				pClassMethEntry->pUserData = (void *)pMeth;` |
|        3 |  817 | `				continue;` |
|        - |  818 | `			}` |
|        - |  819 | `			/* Both concrete: a genuine collision only when the OTHER definition came` |
|        - |  820 | `			 * from another trait (a concrete one). A class-body method wins silently. */` |
|        8 |  821 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|        8 |  822 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|        8 |  823 | `			for(k = 0; k < nUsed; k++){` |
|        3 |  824 | `				ph7_class_method *pOtherMeth = PH7_ClassExtractMethod(apUsedTraits[k],pName->zString,pName->nByte);` |
|        3 |  825 | `				if( pOtherMeth != 0 && (pOtherMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|        - |  826 | `					/* Two different traits define the same CONCRETE method with no resolution */` |
|        4 |  827 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|        - |  828 | `						"Trait method %z::%z has not been applied as %z::%z, "` |
|        - |  829 | `						"because of collision with %z::%z",` |
|        2 |  830 | `						&pTrait->sName,pName,` |
|        1 |  831 | `						&pClass->sName,pName,` |
|        2 |  832 | `						&apUsedTraits[k]->sName,pName);` |
|        3 |  833 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  834 | `						goto cleanup;` |
|        - |  835 | `					}` |
|        3 |  836 | `					break;` |
|        - |  837 | `				}` |
|      ! 0 |  838 | `			}` |
|        - |  839 | `			/* Class-defined method takes precedence */` |
|        8 |  840 | `			continue;` |
|        - |  841 | `		}` |
|      245 |  842 | `		rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|      245 |  843 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  844 | `			goto cleanup;` |
|        - |  845 | `		}` |
|        5 |  846 | `	}` |
|        - |  847 | `	/* Record trait in the class */` |
|      155 |  848 | `	SySetPut(&pClass->aTrait,(const void *)&pTrait);` |
|       75 |  849 | `cleanup:` |
|        - |  850 | `	/* Always clear visiting flag, even on error paths */` |
|      155 |  851 | `	pTrait->iFlags &= ~PH7_CLASS_TRAIT_VISITING;` |
|       75 |  852 | `	SXUNUSED(pGen);` |
|      155 |  853 | `	return rc;` |
|       80 |  854 | `}` |
|        - |  855 | `/*` |
|        - |  856 | ` * Inherit an object interface from another object interface.` |
|        - |  857 | ` * According to the PHP language reference manual.` |
|        - |  858 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|        - |  859 | ` *  must implement, without having to define how these methods are handled.` |
|        - |  860 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|        - |  861 | ` *  class, but without any of the methods having their contents defined.` |
|        - |  862 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|        - |  863 | ` *` |
|        - |  864 | ` * This function return SXRET_OK if the interface inheritance operation was successfully performed.` |
|        - |  865 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|        - |  866 | ` * error message.` |
|        - |  867 | ` */` |
|    37378 |  868 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceInherit(ph7_class *pSub,ph7_class *pBase)` |
|        5 |  869 | `{` |
|        - |  870 | `	ph7_class_method *pMeth;` |
|        - |  871 | `	ph7_class_attr *pAttr;` |
|        - |  872 | `	SyHashEntry *pEntry;` |
|        - |  873 | `	SyString *pName;` |
|        - |  874 | `	sxi32 rc;` |
|        - |  875 | `	/* Install in the derived hashtable */` |
|    37383 |  876 | `	SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|    37383 |  877 | `	SyHashResetLoopCursor(&pBase->hConst);` |
|        - |  878 | `	/* Copy constants (interfaces carry only constants + method signatures) */` |
|    56074 |  879 | `	while((pEntry = SyHashGetNextEntry(&pBase->hConst)) != 0 ){` |
|        - |  880 | `		/* Make sure the constants are not redeclared in the subclass */` |
|        3 |  881 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|        3 |  882 | `		pName = &pAttr->sName;` |
|        3 |  883 | `		if( SyHashGet(&pSub->hConst,(const void *)pName->zString,pName->nByte) == 0 ){` |
|        - |  884 | `			/* Install the constant in the subclass */` |
|        3 |  885 | `			rc = SyHashInsertTail(&pSub->hConst,(const void *)pName->zString,pName->nByte,pAttr);` |
|        3 |  886 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  887 | `				return rc;` |
|        - |  888 | `			}` |
|        1 |  889 | `		}` |
|        1 |  890 | `	}` |
|    37383 |  891 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
|        - |  892 | `	/* Copy methods signature */` |
|   140188 |  893 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|        - |  894 | `		/* Make sure the method are not redeclared in the subclass */` |
|    84121 |  895 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|    84121 |  896 | `		pName = &pMeth->sFunc.sName;` |
|    84121 |  897 | `		if( SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte) == 0 ){` |
|        - |  898 | `			/* Install the method */` |
|    84121 |  899 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|    84121 |  900 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  901 | `				return rc;` |
|        - |  902 | `			}` |
|    42058 |  903 | `		}` |
|        5 |  904 | `	}` |
|        - |  905 | `	/* Mark as subclass */` |
|    37383 |  906 | `	pSub->pBase = pBase;` |
|        - |  907 | `	/* All done */` |
|    37383 |  908 | `	return SXRET_OK;` |
|    18694 |  909 | `}` |
|        - |  910 | `/*` |
|        - |  911 | ` * Implements an object interface in the given main class.` |
|        - |  912 | ` * According to the PHP language reference manual.` |
|        - |  913 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|        - |  914 | ` *  must implement, without having to define how these methods are handled.` |
|        - |  915 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|        - |  916 | ` *  class, but without any of the methods having their contents defined.` |
|        - |  917 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|        - |  918 | ` *` |
|        - |  919 | ` * This function return SXRET_OK if the interface was successfully implemented.` |
|        - |  920 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|        - |  921 | ` * error message.` |
|        - |  922 | ` */` |
|   266816 |  923 | `PH7_PRIVATE sxi32 PH7_ClassImplement(ph7_class *pMain,ph7_class *pInterface)` |
|        5 |  924 | `{` |
|        - |  925 | `	ph7_class_attr *pAttr;` |
|        - |  926 | `	SyHashEntry *pEntry;` |
|        - |  927 | `	SyString *pName;` |
|        - |  928 | `	sxi32 rc;` |
|        - |  929 | `	/* First off,copy all constants declared inside the interface (hConst namespace) */` |
|   266821 |  930 | `	SyHashResetLoopCursor(&pInterface->hConst);` |
|   531007 |  931 | `	while((pEntry = SyHashGetNextEntry(&pInterface->hConst)) != 0 ){` |
|        - |  932 | `		/* Point to the constant declaration */` |
|   130783 |  933 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   130783 |  934 | `		pName = &pAttr->sName;` |
|        - |  935 | `		/* Make sure the constant is not redeclared in the main class */` |
|   130783 |  936 | `		if( SyHashGet(&pMain->hConst,pName->zString,pName->nByte) == 0 ){` |
|        - |  937 | `			/* Install the constant */` |
|   130783 |  938 | `			rc = SyHashInsertTail(&pMain->hConst,pName->zString,pName->nByte,pAttr);` |
|   130783 |  939 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  940 | `				return rc;` |
|        - |  941 | `			}` |
|    65389 |  942 | `		}` |
|        5 |  943 | `	}` |
|        - |  944 | `	/* Install in the interface container */` |
|   266821 |  945 | `	SySetPut(&pMain->aInterface,(const void *)&pInterface);` |
|        - |  946 | `	/* Install interface method stubs into the implementing class.` |
|        - |  947 | `	 * Methods already defined in the class take precedence (they satisfy` |
|        - |  948 | `	 * the interface contract). Stubs retain PH7_CLASS_ATTR_ABSTRACT so` |
|        - |  949 | `	 * the unified check in GenStateCheckAbstractMethods catches missing ones.` |
|        - |  950 | `	 */` |
|        - |  951 | `	{` |
|        - |  952 | `		ph7_class_method *pMeth;` |
|        - |  953 | `		SyHashEntry *pMEntry;` |
|        - |  954 | `		SyString *pMName;` |
|   266821 |  955 | `		SyHashResetLoopCursor(&pInterface->hMethod);` |
|  1139087 |  956 | `		while((pMEntry = SyHashGetNextEntry(&pInterface->hMethod)) != 0 ){` |
|   738863 |  957 | `			pMeth = (ph7_class_method *)pMEntry->pUserData;` |
|   738863 |  958 | `			pMName = &pMeth->sFunc.sName;` |
|   738863 |  959 | `			if( SyHashGet(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte) == 0 ){` |
|     4695 |  960 | `				rc = SyHashInsert(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte,pMeth);` |
|     4695 |  961 | `				if( rc != SXRET_OK ){` |
|      ! 0 |  962 | `					return rc;` |
|        - |  963 | `				}` |
|     2345 |  964 | `			}` |
|        5 |  965 | `		}` |
|        - |  966 | `	}` |
|   266821 |  967 | `	return SXRET_OK;` |
|   133413 |  968 | `}` |
|        - |  969 | `/*` |
|        - |  970 | ` * Create a class instance [i.e: Object in the PHP jargon] at run-time.` |
|        - |  971 | ` * The following function is called when an object is created at run-time` |
|        - |  972 | ` * typically when the PH7_OP_NEW/PH7_OP_CLONE instructions are executed.` |
|        - |  973 | ` * Notes on object creation.` |
|        - |  974 | ` *` |
|        - |  975 | ` * According to PHP language reference manual.` |
|        - |  976 | ` * To create an instance of a class, the new keyword must be used. An object will always` |
|        - |  977 | ` * be created unless the object has a constructor defined that throws an exception on error.` |
|        - |  978 | ` * Classes should be defined before instantiation (and in some cases this is a requirement).` |
|        - |  979 | ` * If a string containing the name of a class is used with new, a new instance of that class` |
|        - |  980 | ` * will be created. If the class is in a namespace, its fully qualified name must be used when` |
|        - |  981 | ` * doing this.` |
|        - |  982 | ` * Example #3 Creating an instance` |
|        - |  983 | ` * <?php` |
|        - |  984 | ` *  $instance = new SimpleClass();` |
|        - |  985 | ` *   // This can also be done with a variable:` |
|        - |  986 | ` * $className = 'Foo';` |
|        - |  987 | ` * $instance = new $className(); // Foo()` |
|        - |  988 | ` * ?>` |
|        - |  989 | ` * In the class context, it is possible to create a new object by new self and new parent.` |
|        - |  990 | ` * When assigning an already created instance of a class to a new variable, the new variable` |
|        - |  991 | ` * will access the same instance as the object that was assigned. This behaviour is the same` |
|        - |  992 | ` * when passing instances to a function. A copy of an already created object can be made by` |
|        - |  993 | ` * cloning it.` |
|        - |  994 | ` * Example #4 Object Assignment` |
|        - |  995 | ` * <?php` |
|        - |  996 | ` *  class SimpleClass(){` |
|        - |  997 | ` *    public $var;` |
|        - |  998 | ` *  };` |
|        - |  999 | ` *  $instance = new SimpleClass();` |
|        - | 1000 | ` *  $assigned   =  $instance;` |
|        - | 1001 | ` *  $reference  =& $instance;` |
|        - | 1002 | ` *  $instance->var = '$assigned will have this value';` |
|        - | 1003 | ` *  $instance = null; // $instance and $reference become null` |
|        - | 1004 | ` *  var_dump($instance);` |
|        - | 1005 | ` *  var_dump($reference);` |
|        - | 1006 | ` *  var_dump($assigned);` |
|        - | 1007 | ` * ?>` |
|        - | 1008 | ` * The above example will output:` |
|        - | 1009 | ` * NULL` |
|        - | 1010 | ` * NULL` |
|        - | 1011 | ` * object(SimpleClass)#1 (1) {` |
|        - | 1012 | ` *  ["var"]=>` |
|        - | 1013 | ` *    string(30) "$assigned will have this value"` |
|        - | 1014 | ` * }` |
|        - | 1015 | ` * Example #5 Creating new objects` |
|        - | 1016 | ` * <?php` |
|        - | 1017 | ` * class Test` |
|        - | 1018 | ` * {` |
|        - | 1019 | ` *   static public function getNew()` |
|        - | 1020 | ` *   {` |
|        - | 1021 | ` *       return new static;` |
|        - | 1022 | ` *   }` |
|        - | 1023 | ` * }` |
|        - | 1024 | ` * class Child extends Test` |
|        - | 1025 | ` * {}` |
|        - | 1026 | ` * $obj1 = new Test();` |
|        - | 1027 | ` * $obj2 = new $obj1;` |
|        - | 1028 | ` * var_dump($obj1 !== $obj2);` |
|        - | 1029 | ` * $obj3 = Test::getNew();` |
|        - | 1030 | ` * var_dump($obj3 instanceof Test);` |
|        - | 1031 | ` * $obj4 = Child::getNew();` |
|        - | 1032 | ` * var_dump($obj4 instanceof Child);` |
|        - | 1033 | ` * ?>` |
|        - | 1034 | ` * The above example will output:` |
|        - | 1035 | ` * bool(true)` |
|        - | 1036 | ` * bool(true)` |
|        - | 1037 | ` * bool(true)` |
|        - | 1038 | ` * Note that Symisc Systems have introduced powerfull extension to` |
|        - | 1039 | ` * OO subsystem. For example a class attribute may have any complex` |
|        - | 1040 | ` * expression associated with it when declaring the attribute unlike` |
|        - | 1041 | ` * the standard PHP engine which would allow a single value.` |
|        - | 1042 | ` * Example:` |
|        - | 1043 | ` *  class myClass{` |
|        - | 1044 | ` *    public $var = 25<<1+foo()/bar();` |
|        - | 1045 | ` *  };` |
|        - | 1046 | ` * Refer to the official documentation for more information.` |
|        - | 1047 | ` */` |
|  1574342 | 1048 | `static ph7_class_instance * NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|        5 | 1049 | `{` |
|        - | 1050 | `	ph7_class_instance *pThis;` |
|        - | 1051 | `	/* Allocate a new instance */` |
|  1574347 | 1052 | `	pThis = (ph7_class_instance *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_instance));` |
|  1574347 | 1053 | `	if( pThis == 0 ){` |
|      ! 0 | 1054 | `		return 0;` |
|        - | 1055 | `	}` |
|        - | 1056 | `	/* Zero the structure */` |
|  1574347 | 1057 | `	SyZero(pThis,sizeof(ph7_class_instance));` |
|        - | 1058 | `	/* Initialize fields */` |
|  1574347 | 1059 | `	pThis->iRef = 1;` |
|  1574347 | 1060 | `	pThis->pVm = pVm;` |
|  1574347 | 1061 | `	pThis->pClass = pClass;` |
|        - | 1062 | `	/* Assign a fresh monotonic object handle id (clones get their own, like PHP). */` |
|  1574347 | 1063 | `	pThis->nObjId = pVm->nNextObjId++;` |
|  1574347 | 1064 | `	SyHashInit(&pThis->hAttr,&pVm->sAllocator,0,0);` |
|  1574347 | 1065 | `	return pThis;` |
|   787176 | 1066 | `}` |
|        - | 1067 | `/*` |
|        - | 1068 | ` * Wrapper around the NewClassInstance() function defined above.` |
|        - | 1069 | ` * See the block comment above for more information.` |
|        - | 1070 | ` */` |
|  1573854 | 1071 | `PH7_PRIVATE ph7_class_instance * PH7_NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|        5 | 1072 | `{` |
|        - | 1073 | `	ph7_class_instance *pNew;` |
|        - | 1074 | `	sxi32 rc;` |
|  1573859 | 1075 | `	pNew = NewClassInstance(&(*pVm),&(*pClass));` |
|  1573859 | 1076 | `	if( pNew == 0 ){` |
|      ! 0 | 1077 | `		return 0;` |
|        - | 1078 | `	}` |
|        - | 1079 | `	/* Associate a private VM frame with this class instance */` |
|  1573859 | 1080 | `	rc = PH7_VmCreateClassInstanceFrame(&(*pVm),pNew);` |
|  1573859 | 1081 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1082 | `		SyMemBackendPoolFree(&pVm->sAllocator,pNew);` |
|      ! 0 | 1083 | `		return 0;` |
|        - | 1084 | `	}` |
|        - | 1085 | `	/* php stamps a Throwable's file/line at CREATION, not in its constructor, so a` |
|        - | 1086 | `	 * subclass that overrides __construct without calling parent::__construct still` |
|        - | 1087 | `	 * reports the right site. Every instantiation path lands here. */` |
|  1573859 | 1088 | `	PH7_VmStampThrowableSite(&(*pVm),pNew);` |
|  1573859 | 1089 | `	return pNew;` |
|   786932 | 1090 | `}` |
|        - | 1091 | `/*` |
|        - | 1092 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon] attribute.` |
|        - | 1093 | ` * This function never fail.` |
|        - | 1094 | ` */` |
|  7382248 | 1095 | `static ph7_value * ExtractClassAttrValue(ph7_vm *pVm,VmClassAttr *pAttr)` |
|        5 | 1096 | `{` |
|        - | 1097 | `	/* Extract the value */` |
|        - | 1098 | `	ph7_value *pValue;` |
|  7382253 | 1099 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|  7382253 | 1100 | `	return pValue;` |
|        5 | 1101 | `}` |
|        - | 1102 | `/*` |
|        - | 1103 | ` * Perform a clone operation on a class instance [i.e: Object in the PHP jargon].` |
|        - | 1104 | ` * The following function is called when an object is cloned at run-time` |
|        - | 1105 | ` * typically when the PH7_OP_CLONE instruction is executed.` |
|        - | 1106 | ` * Notes on object cloning.` |
|        - | 1107 | ` *` |
|        - | 1108 | ` * According to PHP language reference manual.` |
|        - | 1109 | ` * Creating a copy of an object with fully replicated properties is not always the wanted behavior.` |
|        - | 1110 | ` * A good example of the need for copy constructors. Another example is if your object holds a reference` |
|        - | 1111 | ` * to another object which it uses and when you replicate the parent object you want to create` |
|        - | 1112 | ` * a new instance of this other object so that the replica has its own separate copy.` |
|        - | 1113 | ` * An object copy is created by using the clone keyword (which calls the object's __clone() method if possible).` |
|        - | 1114 | ` * An object's __clone() method cannot be called directly.` |
|        - | 1115 | ` * $copy_of_object = clone $object;` |
|        - | 1116 | ` * When an object is cloned, PHP 5 will perform a shallow copy of all of the object's properties.` |
|        - | 1117 | ` * Any properties that are references to other variables, will remain references.` |
|        - | 1118 | ` * Once the cloning is complete, if a __clone() method is defined, then the newly created object's __clone() method` |
|        - | 1119 | ` * will be called, to allow any necessary properties that need to be changed.` |
|        - | 1120 | ` * Example #1 Cloning an object` |
|        - | 1121 | ` * <?php` |
|        - | 1122 | ` * class SubObject` |
|        - | 1123 | ` * {` |
|        - | 1124 | ` *   static $instances = 0;` |
|        - | 1125 | ` *   public $instance;` |
|        - | 1126 | ` *` |
|        - | 1127 | ` *   public function __construct() {` |
|        - | 1128 | ` *       $this->instance = ++self::$instances;` |
|        - | 1129 | ` *   }` |
|        - | 1130 | ` *` |
|        - | 1131 | ` *   public function __clone() {` |
|        - | 1132 | ` *       $this->instance = ++self::$instances;` |
|        - | 1133 | ` *   }` |
|        - | 1134 | ` * }` |
|        - | 1135 | ` *` |
|        - | 1136 | ` * class MyCloneable` |
|        - | 1137 | ` * {` |
|        - | 1138 | ` *   public $object1;` |
|        - | 1139 | ` *   public $object2;` |
|        - | 1140 | ` *` |
|        - | 1141 | ` *   function __clone()` |
|        - | 1142 | ` *   {` |
|        - | 1143 | ` *       // Force a copy of this->object, otherwise` |
|        - | 1144 | ` *       // it will point to same object.` |
|        - | 1145 | ` *       $this->object1 = clone $this->object1;` |
|        - | 1146 | ` *   }` |
|        - | 1147 | ` * }` |
|        - | 1148 | ` * $obj = new MyCloneable();` |
|        - | 1149 | ` * $obj->object1 = new SubObject();` |
|        - | 1150 | ` * $obj->object2 = new SubObject();` |
|        - | 1151 | ` * $obj2 = clone $obj;` |
|        - | 1152 | ` * print("Original Object:\n");` |
|        - | 1153 | ` * print_r($obj);` |
|        - | 1154 | ` * print("Cloned Object:\n");` |
|        - | 1155 | ` * print_r($obj2);` |
|        - | 1156 | ` * ?>` |
|        - | 1157 | ` * The above example will output:` |
|        - | 1158 | ` * Original Object:` |
|        - | 1159 | ` * MyCloneable Object` |
|        - | 1160 | ` * (` |
|        - | 1161 | ` *   [object1] => SubObject Object` |
|        - | 1162 | ` *       (` |
|        - | 1163 | ` *           [instance] => 1` |
|        - | 1164 | ` *       )` |
|        - | 1165 | ` *` |
|        - | 1166 | ` *   [object2] => SubObject Object` |
|        - | 1167 | ` *       (` |
|        - | 1168 | ` *           [instance] => 2` |
|        - | 1169 | ` *       )` |
|        - | 1170 | ` *` |
|        - | 1171 | ` * )` |
|        - | 1172 | ` * Cloned Object:` |
|        - | 1173 | ` * MyCloneable Object` |
|        - | 1174 | ` * (` |
|        - | 1175 | ` *   [object1] => SubObject Object` |
|        - | 1176 | ` *       (` |
|        - | 1177 | ` *           [instance] => 3` |
|        - | 1178 | ` *       )` |
|        - | 1179 | ` *` |
|        - | 1180 | ` *   [object2] => SubObject Object` |
|        - | 1181 | ` *       (` |
|        - | 1182 | ` *           [instance] => 2` |
|        - | 1183 | ` *       )` |
|        - | 1184 | ` * )` |
|        - | 1185 | ` */` |
|      488 | 1186 | `PH7_PRIVATE ph7_class_instance * PH7_CloneClassInstance(ph7_class_instance *pSrc)` |
|        5 | 1187 | `{` |
|        - | 1188 | `	ph7_class_instance *pClone;` |
|        - | 1189 | `	ph7_class_method *pMethod;` |
|        - | 1190 | `	SyHashEntry *pEntry2;` |
|        - | 1191 | `	SyHashEntry *pEntry;` |
|        - | 1192 | `	ph7_vm *pVm;` |
|        - | 1193 | `	sxi32 rc;` |
|        - | 1194 | `	/* Allocate a new instance */` |
|      493 | 1195 | `	pVm = pSrc->pVm;` |
|      493 | 1196 | `	pClone = NewClassInstance(pVm,pSrc->pClass);` |
|      493 | 1197 | `	if( pClone == 0 ){` |
|      ! 0 | 1198 | `		return 0;` |
|        - | 1199 | `	}` |
|        - | 1200 | `	/* Associate a private VM frame with this class instance */` |
|      493 | 1201 | `	rc = PH7_VmCreateClassInstanceFrame(pVm,pClone);` |
|      493 | 1202 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1203 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClone);` |
|      ! 0 | 1204 | `		return 0;` |
|        - | 1205 | `	}` |
|        - | 1206 | `	/* Duplicate object values. Iterate the SOURCE attributes and copy each into` |
|        - | 1207 | `	 * the clone's same-named slot (looked up by name, so order/count differences` |
|        - | 1208 | `	 * from dynamic properties don't matter). A dynamic (runtime-added) property` |
|        - | 1209 | `	 * has no declared counterpart in the clone, so synthesize it first — without` |
|        - | 1210 | `	 * this, a clone of a stdClass would silently lose all its dynamic properties. */` |
|      493 | 1211 | `	SyHashResetLoopCursor(&pSrc->hAttr);` |
|     1921 | 1212 | `	while((pEntry = SyHashGetNextEntry(&pSrc->hAttr)) != 0 ){` |
|     1433 | 1213 | `		VmClassAttr *pSrcAttr = (VmClassAttr *)pEntry->pUserData;` |
|     1433 | 1214 | `		VmClassAttr *pDestAttr = 0;` |
|     1433 | 1215 | `		ph7_value *pvSrc,*pvDest = 0;` |
|        - | 1216 | `		/* Duplicate non-static attribute */` |
|     1433 | 1217 | `		if( pSrcAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|        3 | 1218 | `			continue;` |
|        - | 1219 | `		}` |
|     1431 | 1220 | `		pEntry2 = SyHashGet(&pClone->hAttr,SyStringData(&pSrcAttr->pAttr->sName),SyStringLength(&pSrcAttr->pAttr->sName));` |
|     1431 | 1221 | `		if( pEntry2 ){` |
|     1409 | 1222 | `			pDestAttr = (VmClassAttr *)pEntry2->pUserData;` |
|     1409 | 1223 | `			pvDest = ExtractClassAttrValue(pVm,pDestAttr);` |
|      725 | 1224 | `		}else if( pSrcAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){` |
|        - | 1225 | `			/* Dynamic property: synthesize the matching slot on the clone. */` |
|       34 | 1226 | `			pvDest = PH7_VmCreateDynamicAttr(pVm,pClone,` |
|       22 | 1227 | `				SyStringData(&pSrcAttr->pAttr->sName),SyStringLength(&pSrcAttr->pAttr->sName),&pDestAttr);` |
|       11 | 1228 | `		}` |
|        - | 1229 | `		/* Fetch the source value LAST: PH7_VmCreateDynamicAttr above may have` |
|        - | 1230 | `		 * reserved a slot and reallocated pVm->aMemObj, which would dangle any` |
|        - | 1231 | `		 * ph7_value* obtained before it. pvDest from the synth path already points` |
|        - | 1232 | `		 * into the post-realloc aMemObj; resolve pvSrc now so both are current. */` |
|     1431 | 1233 | `		pvSrc = ExtractClassAttrValue(pVm,pSrcAttr);` |
|     1431 | 1234 | `		if( (pSrcAttr->iState & VM_CLASS_ATTR_REFBOUND) && pDestAttr ){` |
|        - | 1235 | `			/* php preserves references across clone: the clone shares the SAME slot` |
|        - | 1236 | `			 * as the source property (both alias the referenced variable), rather` |
|        - | 1237 | `			 * than getting an independent value copy. Drop the clone's fresh private` |
|        - | 1238 | `			 * slot and repoint at the (already-pinned) shared slot. The REFBOUND flag` |
|        - | 1239 | `			 * is carried over by the iState copy below, so the clone's release also` |
|        - | 1240 | `			 * leaves the shared slot alone. */` |
|        5 | 1241 | `			if( pDestAttr->nIdx != pSrcAttr->nIdx ){` |
|        5 | 1242 | `				if( pDestAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|      ! 0 | 1243 | `					SyHashDeleteEntry(&pVm->hTypedSlot,(const void *)&pDestAttr->nIdx,sizeof(sxu32),0);` |
|      ! 0 | 1244 | `				}` |
|        5 | 1245 | `				PH7_VmUnsetMemObj(pVm,pDestAttr->nIdx,TRUE);` |
|        5 | 1246 | `				pDestAttr->nIdx = pSrcAttr->nIdx;` |
|        - | 1247 | `				/* The clone is a holder of the shared slot in its own right — take a pin` |
|        - | 1248 | `				 * for it, since its own release will give one back. */` |
|        5 | 1249 | `				VmPinMemObjSlotCounted(pVm,pDestAttr->nIdx);` |
|        3 | 1250 | `			}` |
|     1429 | 1251 | `		}else if( pvSrc && pvDest ){` |
|     1427 | 1252 | `			PH7_MemObjStore(pvSrc,pvDest);` |
|      711 | 1253 | `		}` |
|        - | 1254 | `		/* Carry over the per-instance state so the clone matches the source:` |
|        - | 1255 | `		 * VM_CLASS_ATTR_UNINIT marks a typed property as not-yet-initialized` |
|        - | 1256 | `		 * and doubles as the readonly write-once latch — without this a clone` |
|        - | 1257 | `		 * would reset to uninitialized (losing the value's readiness) and a` |
|        - | 1258 | `		 * readonly property would become writable again. */` |
|     1431 | 1259 | `		if( pDestAttr ){` |
|     1431 | 1260 | `			pDestAttr->iState = pSrcAttr->iState;` |
|      713 | 1261 | `		}` |
|        5 | 1262 | `	}` |
|        - | 1263 | `	/* A declared property unset() on the source is absent from the clone too (PHP). But the clone` |
|        - | 1264 | `	 * frame above materialized ALL declared attrs (with their defaults), so drop any clone attr whose` |
|        - | 1265 | `	 * name is not present on the source. Collect first, then delete — removing an entry mid-walk would` |
|        - | 1266 | `	 * free the node the SyHash loop cursor points at. */` |
|        - | 1267 | `	{` |
|        - | 1268 | `		SySet sDrop;` |
|      493 | 1269 | `		SySetInit(&sDrop,&pVm->sAllocator,sizeof(VmClassAttr *));` |
|      493 | 1270 | `		SyHashResetLoopCursor(&pClone->hAttr);` |
|     1923 | 1271 | `		while((pEntry = SyHashGetNextEntry(&pClone->hAttr)) != 0 ){` |
|     1435 | 1272 | `			VmClassAttr *pCloneAttr = (VmClassAttr *)pEntry->pUserData;` |
|     1435 | 1273 | `			if( pCloneAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|        3 | 1274 | `				continue;` |
|        - | 1275 | `			}` |
|     2142 | 1276 | `			if( SyHashGet(&pSrc->hAttr,SyStringData(&pCloneAttr->pAttr->sName),` |
|     2147 | 1277 | `					SyStringLength(&pCloneAttr->pAttr->sName)) == 0 ){` |
|        3 | 1278 | `				SySetPut(&sDrop,(const void *)&pCloneAttr);` |
|        1 | 1279 | `			}` |
|        5 | 1280 | `		}` |
|      493 | 1281 | `		if( SySetUsed(&sDrop) > 0 ){` |
|        3 | 1282 | `			VmClassAttr **apDrop = (VmClassAttr **)SySetBasePtr(&sDrop);` |
|        - | 1283 | `			sxu32 i;` |
|        5 | 1284 | `			for( i = 0 ; i < SySetUsed(&sDrop) ; ++i ){` |
|        3 | 1285 | `				VmClassAttr *pVmAttr = apDrop[i];` |
|        4 | 1286 | `				SyHashDeleteEntry(&pClone->hAttr,SyStringData(&pVmAttr->pAttr->sName),` |
|        2 | 1287 | `					SyStringLength(&pVmAttr->pAttr->sName),0);` |
|        3 | 1288 | `				PH7_VmReleaseInstanceAttr(pVm,pVmAttr);` |
|        2 | 1289 | `			}` |
|        1 | 1290 | `		}` |
|      493 | 1291 | `		SySetRelease(&sDrop);` |
|        - | 1292 | `	}` |
|        - | 1293 | `	/* call the __clone method on the cloned object if available */` |
|      493 | 1294 | `	pMethod = PH7_ClassExtractMethod(pClone->pClass,"__clone",sizeof("__clone")-1);` |
|      493 | 1295 | `	if( pMethod ){` |
|      101 | 1296 | `		if( pMethod->iCloneDepth < 16 ){` |
|       99 | 1297 | `			pMethod->iCloneDepth++;` |
|        - | 1298 | `			/* PHP 8.3: __clone() may re-initialize the clone's readonly` |
|        - | 1299 | `			 * properties. Flag the instance so the readonly store guard allows` |
|        - | 1300 | `			 * it for the duration of the call. */` |
|       99 | 1301 | `			pClone->iFlags \|= VM_INSTANCE_CLONING;` |
|       99 | 1302 | `			PH7_VmCallClassMethod(pVm,pClone,pMethod,0,0,0);` |
|       99 | 1303 | `			pClone->iFlags &= ~VM_INSTANCE_CLONING;` |
|       51 | 1304 | `		}else{` |
|        - | 1305 | `			/* Nesting limit reached */` |
|        3 | 1306 | `			PH7_VmThrowError(pVm,0,PH7_CTX_ERR,"Object clone limit reached,no more call to __clone()");` |
|        - | 1307 | `		}` |
|        - | 1308 | `		/* Reset the cursor */` |
|      101 | 1309 | `		pMethod->iCloneDepth = 0;` |
|       49 | 1310 | `	}` |
|        - | 1311 | `	/* Return the cloned object */` |
|      493 | 1312 | `	return pClone;` |
|      249 | 1313 | `}` |
|        - | 1314 | `#define CLASS_INSTANCE_DESTROYED 0x001 /* Instance is released */` |
|        - | 1315 | `/*` |
|        - | 1316 | ` * Free the per-instance allocations owned by ONE object attribute: its value slot (+ the typed-slot` |
|        - | 1317 | ` * enforcement entry), the synthesized ph7_class_attr for a dynamic (runtime-added) property, and the` |
|        - | 1318 | ` * VmClassAttr wrapper itself. Does NOT touch the hAttr entry node — the caller removes it` |
|        - | 1319 | `` * (`unset($o->p)` via SyHashDeleteEntry2; instance teardown via the wholesale SyHashRelease, so it must`` |
|        - | 1320 | ` * not delete entries mid-walk). Shared by PH7_ClassInstanceRelease and the OP_MEMBER unset path.` |
|        - | 1321 | ` */` |
|  9506944 | 1322 | `PH7_PRIVATE void PH7_VmReleaseInstanceAttr(ph7_vm *pVm, VmClassAttr *pVmAttr)` |
|        5 | 1323 | `{` |
|  9506949 | 1324 | `	if( pVmAttr->iState & VM_CLASS_ATTR_REFBOUND ){` |
|        - | 1325 | ``		/* Reference-bound property (`$o->p =& $x`): its value slot is SHARED, so it must`` |
|        - | 1326 | `		 * not be released here — but the property WAS one of its holders, so give the pin` |
|        - | 1327 | `		 * back. The slot (and its value, which used to stay alive for the rest of the` |
|        - | 1328 | `		 * script) goes if the property was the last thing holding it. */` |
|       27 | 1329 | `		VmUnpinMemObjSlot(pVm,pVmAttr->nIdx);` |
|  9506936 | 1330 | `	}else if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - | 1331 | `		/* Drop any typed-property enforcement slot registered for this memobj, before the memobj` |
|        - | 1332 | `		 * is returned to the free list, so a future recycled slot does not inherit the stale entry. */` |
|  9506843 | 1333 | `		if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|  6779009 | 1334 | `			SyHashDeleteEntry(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),0);` |
|  3389502 | 1335 | `		}` |
|  9506843 | 1336 | `		PH7_VmUnsetMemObj(pVm,pVmAttr->nIdx,TRUE);` |
|  4753419 | 1337 | `	}` |
|        - | 1338 | `	/* A dynamic property owns its synthesized ph7_class_attr (struct + inline name in one block) —` |
|        - | 1339 | `	 * free it here (the only place a per-instance pAttr is freed; declared attrs are class-owned). */` |
|  9506949 | 1340 | `	if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){` |
|      257 | 1341 | `		SyMemBackendFree(&pVm->sAllocator,pVmAttr->pAttr);` |
|      127 | 1342 | `	}` |
|  9506949 | 1343 | `	SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|  9506949 | 1344 | `}` |
|        - | 1345 | `/*` |
|        - | 1346 | ` * Release a class instance [i.e: Object in the PHP jargon] and invoke any defined destructor.` |
|        - | 1347 | ` * This routine is invoked as soon as there are no other references to a particular` |
|        - | 1348 | ` * class instance.` |
|        - | 1349 | ` */` |
|  1464584 | 1350 | `static void PH7_ClassInstanceRelease(ph7_class_instance *pThis)` |
|        5 | 1351 | `{` |
|        - | 1352 | `	ph7_class_method *pDestr;` |
|        - | 1353 | `	SyHashEntry *pEntry;` |
|        - | 1354 | `	ph7_class *pClass;` |
|        - | 1355 | `	ph7_vm *pVm;` |
|  1464589 | 1356 | `	if( pThis->iFlags & CLASS_INSTANCE_DESTROYED ){` |
|        - | 1357 | `		/*` |
|        - | 1358 | `		 * Already destroyed,return immediately.` |
|        - | 1359 | `		 * This could happend if someone perform unset($this) in the destructor body.` |
|        - | 1360 | `		 */` |
|      ! 0 | 1361 | `		return;` |
|        - | 1362 | `	}` |
|        - | 1363 | `	/* Mark as destroyed */` |
|  1464589 | 1364 | `	pThis->iFlags \|= CLASS_INSTANCE_DESTROYED;` |
|        - | 1365 | `	/* Invoke any defined destructor if available */` |
|  1464589 | 1366 | `	pVm = pThis->pVm;` |
|  1464589 | 1367 | `	pClass = pThis->pClass;` |
|  1464589 | 1368 | `	pDestr = PH7_ClassExtractMethod(pClass,"__destruct",sizeof("__destruct")-1);` |
|  1464589 | 1369 | `	if( pDestr && !pVm->bInReset ){` |
|        - | 1370 | `		/* Invoke the destructor. Skipped during ph7_vm_reset() bulk teardown:` |
|        - | 1371 | `		 * running user PHP against a half-reset VM is unsafe (see bInReset). */` |
|      521 | 1372 | `		pThis->iRef = 2; /* Prevent garbage collection */` |
|      521 | 1373 | `		PH7_VmCallClassMethod(pVm,pThis,pDestr,0,0,0);` |
|      258 | 1374 | `	}` |
|        - | 1375 | `	/* A native class's own teardown, while its slots are still readable. Not a` |
|        - | 1376 | `	 * __destruct: the classes that need this (WeakReference) declare none in php,` |
|        - | 1377 | `	 * and Reflection must not grow one. */` |
|  1464589 | 1378 | `	if( pClass->xRelease ){` |
|       82 | 1379 | `		pClass->xRelease(pVm,pThis);` |
|       40 | 1380 | `	}` |
|        - | 1381 | `	/* Weak-reference registry: kill the cell for this instance so every` |
|        - | 1382 | `	 * WeakReference/WeakMap handle observes the death (the cell outlives the` |
|        - | 1383 | `	 * instance until its own handles drop; removing the hash entry here keeps` |
|        - | 1384 | `	 * a pool-reused address from resurrecting a dead cell). */` |
|  1464589 | 1385 | `	if( SyHashTotalEntry(&pVm->hWeakCell) > 0 ){` |
|     5124 | 1386 | `		void *pCellData = 0;` |
|     5122 | 1387 | `		if( SyHashDeleteEntry(&pVm->hWeakCell,(const void *)&pThis,sizeof(void *),&pCellData) == SXRET_OK` |
|     2577 | 1388 | `		 && pCellData ){` |
|       30 | 1389 | `			((VmWeakCell *)pCellData)->pObj = 0;` |
|       14 | 1390 | `		}` |
|     2561 | 1391 | `	}` |
|        - | 1392 | `	/* Release non-static attributes (the wholesale SyHashRelease below frees the entry nodes,` |
|        - | 1393 | `	 * so the helper must not delete them mid-walk). */` |
|  1464589 | 1394 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
| 10971501 | 1395 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|  9506917 | 1396 | `		PH7_VmReleaseInstanceAttr(pVm,(VmClassAttr *)pEntry->pUserData);` |
|        5 | 1397 | `	}` |
|        - | 1398 | `	/* Release the whole structure */` |
|  1464589 | 1399 | `	SyHashRelease(&pThis->hAttr);` |
|  1464589 | 1400 | `	SyMemBackendPoolFree(&pVm->sAllocator,pThis);` |
|   732297 | 1401 | `}` |
|        - | 1402 | `/*` |
|        - | 1403 | ` * Decrement the reference count of a class instance [i.e Object in the PHP jargon].` |
|        - | 1404 | ` * If the reference count reaches zero,release the whole instance.` |
|        - | 1405 | ` */` |
|  7349378 | 1406 | `PH7_PRIVATE void PH7_ClassInstanceUnref(ph7_class_instance *pThis)` |
|        5 | 1407 | `{` |
|  7349383 | 1408 | `	pThis->iRef--;` |
|  7349383 | 1409 | `	if( pThis->iRef < 1 ){` |
|        - | 1410 | `		/* No more reference to this instance */` |
|  1464589 | 1411 | `		PH7_ClassInstanceRelease(&(*pThis));` |
|   732292 | 1412 | `	}` |
|  7349383 | 1413 | `}` |
|        - | 1414 | `/*` |
|        - | 1415 | ` * Compare two class instances [i.e: Objects in the PHP jargon]` |
|        - | 1416 | ` * Note on objects comparison:` |
|        - | 1417 | ` *  According to the PHP langauge reference manual` |
|        - | 1418 | ` *  When using the comparison operator (==), object variables are compared in a simple manner` |
|        - | 1419 | ` *  namely: Two object instances are equal if they have the same attributes and values, and are` |
|        - | 1420 | ` *  instances of the same class.` |
|        - | 1421 | ` *  On the other hand, when using the identity operator (===), object variables are identical` |
|        - | 1422 | ` *  if and only if they refer to the same instance of the same class.` |
|        - | 1423 | ` *  An example will clarify these rules.` |
|        - | 1424 | ` *  Example #1 Example of object comparison` |
|        - | 1425 | ` *  <?php` |
|        - | 1426 | ` *    function bool2str($bool)` |
|        - | 1427 | ` * {` |
|        - | 1428 | ` *   if ($bool === false) {` |
|        - | 1429 | ` *       return 'FALSE';` |
|        - | 1430 | ` *   } else {` |
|        - | 1431 | ` *       return 'TRUE';` |
|        - | 1432 | ` *   }` |
|        - | 1433 | ` * }` |
|        - | 1434 | ` * function compareObjects(&$o1, &$o2)` |
|        - | 1435 | ` * {` |
|        - | 1436 | ` *   echo 'o1 == o2 : ' . bool2str($o1 == $o2) . "\n";` |
|        - | 1437 | ` *   echo 'o1 != o2 : ' . bool2str($o1 != $o2) . "\n";` |
|        - | 1438 | ` *   echo 'o1 === o2 : ' . bool2str($o1 === $o2) . "\n";` |
|        - | 1439 | ` *   echo 'o1 !== o2 : ' . bool2str($o1 !== $o2) . "\n";` |
|        - | 1440 | ` * }` |
|        - | 1441 | ` * class Flag` |
|        - | 1442 | ` * {` |
|        - | 1443 | ` *   public $flag;` |
|        - | 1444 | ` *` |
|        - | 1445 | ` *   function Flag($flag = true) {` |
|        - | 1446 | ` *       $this->flag = $flag;` |
|        - | 1447 | ` *   }` |
|        - | 1448 | ` * }` |
|        - | 1449 | ` *` |
|        - | 1450 | ` * class OtherFlag` |
|        - | 1451 | ` * {` |
|        - | 1452 | ` *   public $flag;` |
|        - | 1453 | ` *` |
|        - | 1454 | ` *   function OtherFlag($flag = true) {` |
|        - | 1455 | ` *       $this->flag = $flag;` |
|        - | 1456 | ` *   }` |
|        - | 1457 | ` * }` |
|        - | 1458 | ` *` |
|        - | 1459 | ` * $o = new Flag();` |
|        - | 1460 | ` * $p = new Flag();` |
|        - | 1461 | ` * $q = $o;` |
|        - | 1462 | ` * $r = new OtherFlag();` |
|        - | 1463 | ` *` |
|        - | 1464 | ` * echo "Two instances of the same class\n";` |
|        - | 1465 | ` * compareObjects($o, $p);` |
|        - | 1466 | ` * echo "\nTwo references to the same instance\n";` |
|        - | 1467 | ` * compareObjects($o, $q);` |
|        - | 1468 | ` * echo "\nInstances of two different classes\n";` |
|        - | 1469 | ` * compareObjects($o, $r);` |
|        - | 1470 | ` * ?>` |
|        - | 1471 | ` * The above example will output:` |
|        - | 1472 | ` * Two instances of the same class` |
|        - | 1473 | ` * o1 == o2 : TRUE` |
|        - | 1474 | ` * o1 != o2 : FALSE` |
|        - | 1475 | ` * o1 === o2 : FALSE` |
|        - | 1476 | ` * o1 !== o2 : TRUE` |
|        - | 1477 | ` * Two references to the same instance` |
|        - | 1478 | ` * o1 == o2 : TRUE` |
|        - | 1479 | ` * o1 != o2 : FALSE` |
|        - | 1480 | ` * o1 === o2 : TRUE` |
|        - | 1481 | ` * o1 !== o2 : FALSE` |
|        - | 1482 | ` * Instances of two different classes` |
|        - | 1483 | ` * o1 == o2 : FALSE` |
|        - | 1484 | ` * o1 != o2 : TRUE` |
|        - | 1485 | ` * o1 === o2 : FALSE` |
|        - | 1486 | ` * o1 !== o2 : TRUE` |
|        - | 1487 | ` *` |
|        - | 1488 | ` * This function return 0 if the objects are equals according to the comprison rules defined above.` |
|        - | 1489 | ` * Any other return values indicates difference.` |
|        - | 1490 | ` */` |
|      368 | 1491 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest)` |
|        5 | 1492 | `{` |
|        - | 1493 | `	SyHashEntry *pEntry,*pEntry2;` |
|        - | 1494 | `	ph7_value sV1,sV2;` |
|        - | 1495 | `	sxi32 rc;` |
|      373 | 1496 | `	if( iNest > 31 ){` |
|        - | 1497 | `		/* Nesting limit reached */` |
|        6 | 1498 | `		PH7_VmThrowError(pLeft->pVm,0,PH7_CTX_ERR,"Nesting limit reached: Infinite recursion?");` |
|        6 | 1499 | `		return 1;` |
|        - | 1500 | `	}` |
|        - | 1501 | `	/* Comparison is performed only if the objects are instance of the same class */` |
|      369 | 1502 | `	if( pLeft->pClass != pRight->pClass ){` |
|       10 | 1503 | `		return 1;` |
|        - | 1504 | `	}` |
|      361 | 1505 | `	if( bStrict ){` |
|        - | 1506 | `		/*` |
|        - | 1507 | `		 * According to the PHP language reference manual:` |
|        - | 1508 | `		 *  when using the identity operator (===), object variables` |
|        - | 1509 | `		 *  are identical if and only if they refer to the same instance` |
|        - | 1510 | `		 *  of the same class.` |
|        - | 1511 | `		 */` |
|      181 | 1512 | `		return !(pLeft == pRight);` |
|        - | 1513 | `	}` |
|        - | 1514 | `	/*` |
|        - | 1515 | `	 * Attribute comparison.` |
|        - | 1516 | `	 * According to the PHP reference manual:` |
|        - | 1517 | `	 *  When using the comparison operator (==), object variables are compared` |
|        - | 1518 | `	 *  in a simple manner, namely: Two object instances are equal if they have` |
|        - | 1519 | `	 *  the same attributes and values, and are instances of the same class.` |
|        - | 1520 | `	 */` |
|      185 | 1521 | `	if( pLeft == pRight ){` |
|        - | 1522 | `		/* Same instance,don't bother processing,object are equals */` |
|        5 | 1523 | `		return 0;` |
|        - | 1524 | `	}` |
|        - | 1525 | `	/* Closures compare by IDENTITY under == as well (not by attributes): two distinct` |
|        - | 1526 | `	 * Closure instances are never equal, even when they wrap the same underlying function` |
|        - | 1527 | `	 * (PHP semantics). pLeft != pRight here, so a Closure pair is unequal. Without this,` |
|        - | 1528 | `` 	 * two capture-less lambdas of the same `function(){}` share the template's `$__fn` `` |
|        - | 1529 | `	 * name and would compare equal. */` |
|      181 | 1530 | `	if( pLeft->pVm->pClosureClass && pLeft->pClass == pLeft->pVm->pClosureClass ){` |
|        5 | 1531 | `		return 1;` |
|        - | 1532 | `	}` |
|        - | 1533 | `	/* Same class but a different number of attributes ⇒ different property sets` |
|        - | 1534 | `	 * (dynamic properties can give two same-class instances different counts). */` |
|      177 | 1535 | `	if( pLeft->hAttr.nEntry != pRight->hAttr.nEntry ){` |
|        3 | 1536 | `		return 1;` |
|        - | 1537 | `	}` |
|      175 | 1538 | `	PH7_MemObjInit(pLeft->pVm,&sV1);` |
|      175 | 1539 | `	PH7_MemObjInit(pLeft->pVm,&sV2);` |
|      175 | 1540 | `	sV1.nIdx = sV2.nIdx = SXU32_HIGH;` |
|        - | 1541 | `	/* Compare each left attribute against the RIGHT attribute of the SAME NAME` |
|        - | 1542 | `	 * (not in lockstep): dynamic properties may be stored in a different order` |
|        - | 1543 | `	 * on the two instances. Counts already match, so if every left attribute has` |
|        - | 1544 | `	 * an equal-valued same-named right attribute the property sets are equal. */` |
|      175 | 1545 | `	SyHashResetLoopCursor(&pLeft->hAttr);` |
|      239 | 1546 | `	while((pEntry = SyHashGetNextEntry(&pLeft->hAttr)) != 0 ){` |
|      199 | 1547 | `		VmClassAttr *p1 = (VmClassAttr *)pEntry->pUserData;` |
|        - | 1548 | `		VmClassAttr *p2;` |
|        - | 1549 | `		ph7_value *pL,*pR;` |
|        - | 1550 | `		/* Compare only non-static attribute */` |
|      199 | 1551 | `		if( p1->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|      ! 0 | 1552 | `			continue;` |
|        - | 1553 | `		}` |
|      199 | 1554 | `		pEntry2 = SyHashGet(&pRight->hAttr,SyStringData(&p1->pAttr->sName),SyStringLength(&p1->pAttr->sName));` |
|      199 | 1555 | `		if( pEntry2 == 0 ){` |
|        - | 1556 | `			/* Left has a property the right lacks ⇒ not equal. */` |
|      ! 0 | 1557 | `			return 1;` |
|        - | 1558 | `		}` |
|      199 | 1559 | `		p2 = (VmClassAttr *)pEntry2->pUserData;` |
|      199 | 1560 | `		pL = ExtractClassAttrValue(pLeft->pVm,p1);` |
|      199 | 1561 | `		pR = ExtractClassAttrValue(pRight->pVm,p2);` |
|      199 | 1562 | `		if( pL && pR ){` |
|      199 | 1563 | `			PH7_MemObjLoad(pL,&sV1);` |
|      199 | 1564 | `			PH7_MemObjLoad(pR,&sV2);` |
|        - | 1565 | `			/* Compare the two values now */` |
|      199 | 1566 | `			rc = PH7_MemObjCmp(&sV1,&sV2,bStrict,iNest+1);` |
|      199 | 1567 | `			PH7_MemObjRelease(&sV1);` |
|      199 | 1568 | `			PH7_MemObjRelease(&sV2);` |
|      199 | 1569 | `			if( rc != 0 ){` |
|        - | 1570 | `				/* Not equals */` |
|      133 | 1571 | `				return rc;` |
|        - | 1572 | `			}` |
|       32 | 1573 | `		}` |
|        3 | 1574 | `	}` |
|        - | 1575 | `	/* Object are equals */` |
|       43 | 1576 | `	return 0;` |
|      189 | 1577 | `}` |
|        - | 1578 | `/*` |
|        - | 1579 | ` * Dump a class instance and the store the dump in the BLOB given` |
|        - | 1580 | ` * as the first argument.` |
|        - | 1581 | ` * Note that only non-static/non-constants attribute are dumped.` |
|        - | 1582 | ` * This function is typically invoked when the user issue a call` |
|        - | 1583 | ` * to [var_dump(),var_export(),print_r(),...].` |
|        - | 1584 | ` * This function SXRET_OK on success. Any other return value including` |
|        - | 1585 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|        - | 1586 | ` */` |
|        - | 1587 | `/*` |
|        - | 1588 | `` * Return the `name` property value of an enum case instance (the case name),`` |
|        - | 1589 | ` * or 0 when unavailable. Shared by the var_dump/var_export/json/serialize` |
|        - | 1590 | ` * renderers, which all print enum cases as Class::CaseName forms.` |
|        - | 1591 | ` */` |
|       20 | 1592 | `PH7_PRIVATE ph7_value * PH7_EnumCaseNameValue(ph7_class_instance *pThis)` |
|        1 | 1593 | `{` |
|        - | 1594 | `	SyHashEntry *pEntry;` |
|       21 | 1595 | `	if( (pThis->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|      ! 0 | 1596 | `		return 0;` |
|        - | 1597 | `	}` |
|       21 | 1598 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)"name",sizeof("name")-1);` |
|       21 | 1599 | `	if( pEntry == 0 ){` |
|      ! 0 | 1600 | `		return 0;` |
|        - | 1601 | `	}` |
|       21 | 1602 | `	return PH7_ClassInstanceExtractAttrValue(pThis,(VmClassAttr *)pEntry->pUserData);` |
|       11 | 1603 | `}` |
|        - | 1604 | `/*` |
|        - | 1605 | `` * Return the `value` property value (the backing value) of an enum case`` |
|        - | 1606 | ` * instance, or 0 when unavailable (pure enums have none).` |
|        - | 1607 | ` */` |
|       20 | 1608 | `PH7_PRIVATE ph7_value * PH7_EnumCaseBackingValueOf(ph7_class_instance *pThis)` |
|        1 | 1609 | `{` |
|        - | 1610 | `	SyHashEntry *pEntry;` |
|       21 | 1611 | `	if( (pThis->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|      ! 0 | 1612 | `		return 0;` |
|        - | 1613 | `	}` |
|       21 | 1614 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)"value",sizeof("value")-1);` |
|       21 | 1615 | `	if( pEntry == 0 ){` |
|        7 | 1616 | `		return 0;` |
|        - | 1617 | `	}` |
|       15 | 1618 | `	return PH7_ClassInstanceExtractAttrValue(pThis,(VmClassAttr *)pEntry->pUserData);` |
|       11 | 1619 | `}` |
|        - | 1620 | `/*` |
|        - | 1621 | ` * Emit a class-instance dump header plus its trailing newline. For var_dump` |
|        - | 1622 | ` * (ShowType) it completes the "object(" prefix the caller already emitted as` |
|        - | 1623 | ` *   ClassName)#<id> (<count>) {` |
|        - | 1624 | ` * for print_r it emits the legacy PHL  Object(ClassName) {  (count/id unused).` |
|        - | 1625 | `` * Enum cases print php's `ClassName Enum {` print_r header (var_dump never`` |
|        - | 1626 | `` * reaches here for enums — PH7_MemObjDump prints `enum(S::A)` directly).`` |
|        - | 1627 | ` */` |
|      234 | 1628 | `static void DumpClassInstanceHeader(SyBlob *pOut,ph7_class *pClass,sxu32 nObjId,int ShowType,sxu32 nCount)` |
|        5 | 1629 | `{` |
|      239 | 1630 | `	if( ShowType ){` |
|        - | 1631 | ``		/* var_dump: `object(C)#id (n) {` */`` |
|      184 | 1632 | `		SyBlobFormat(&(*pOut),"object(%z)#%u (%u) {",&pClass->sName,nObjId,nCount);` |
|      184 | 1633 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      184 | 1634 | `		return;` |
|        - | 1635 | `	}` |
|        - | 1636 | ``	/* print_r: `C Object` / `E Enum[:backing]` — the '(' line is emitted by`` |
|        - | 1637 | `	 * the body renderer at the container indent. */` |
|       58 | 1638 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|      ! 0 | 1639 | `		SyBlobFormat(&(*pOut),"%z Enum",&pClass->sName);` |
|      ! 0 | 1640 | `		if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|      ! 0 | 1641 | `			SyBlobAppend(&(*pOut),":int",sizeof(":int")-1);` |
|      ! 0 | 1642 | `		}else if( pClass->nEnumBacking == MEMOBJ_STRING ){` |
|      ! 0 | 1643 | `			SyBlobAppend(&(*pOut),":string",sizeof(":string")-1);` |
|      ! 0 | 1644 | `		}` |
|      ! 0 | 1645 | `	}else{` |
|       58 | 1646 | `		SyBlobFormat(&(*pOut),"%z Object",&pClass->sName);` |
|        - | 1647 | `	}` |
|       58 | 1648 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      122 | 1649 | `}` |
|        - | 1650 | `/*` |
|        - | 1651 | ` * The class that DECLARED pAttr: inheritance shares attr pointers down the` |
|        - | 1652 | ` * chain, so the declaring class is the most ANCESTRAL class whose hAttr still` |
|        - | 1653 | ` * maps the name to this exact pointer. php's var_dump/print_r use it for the` |
|        - | 1654 | `` * `["p":"Decl":private]` annotation.`` |
|        - | 1655 | ` */` |
|        6 | 1656 | `static ph7_class * OoAttrDeclaringClass(ph7_class *pClass,ph7_class_attr *pAttr)` |
|        1 | 1657 | `{` |
|        - | 1658 | `	/* Attrs record their declaring class at install time (inheritance/trait` |
|        - | 1659 | `	 * copies share the pointer, so the field survives the chain). */` |
|        7 | 1660 | `	return pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        1 | 1661 | `}` |
|        - | 1662 | `/*` |
|        - | 1663 | ` * php's zend_unmangle_property_name_ex: a property key carries its own` |
|        - | 1664 | ` * visibility when it is MANGLED — "\0*\0name" is protected and "\0Class\0name"` |
|        - | 1665 | ` * is private to Class, which is how the (array) cast, __debugInfo() and every` |
|        - | 1666 | ` * get_debug_info handler say what a plain array key cannot. A key that does not` |
|        - | 1667 | ` * begin with a NUL is a public name and comes back unchanged.` |
|        - | 1668 | ` *` |
|        - | 1669 | ` * Answers 0 for a key that begins with a NUL and is NOT a well-formed mangled` |
|        - | 1670 | ` * name — php's "Illegal member variable name" (nothing after the NUL, or an` |
|        - | 1671 | ` * empty class part) and "Corrupt member variable name" (no second NUL, or` |
|        - | 1672 | ` * nothing after it). php renders those raw, notice aside, and so does the caller.` |
|        - | 1673 | ` */` |
|      178 | 1674 | `PH7_PRIVATE int PH7_UnmangleAttrName(const char *zKey,sxu32 nKey,SyString *pClass,SyString *pName)` |
|        2 | 1675 | `{` |
|        - | 1676 | `	sxu32 nCls,nSrc;` |
|      180 | 1677 | `	SyStringInitFromBuf(pClass,0,0);` |
|      180 | 1678 | `	SyStringInitFromBuf(pName,zKey,nKey);` |
|      180 | 1679 | `	if( nKey < 1 \|\| zKey[0] != 0 ){` |
|      132 | 1680 | `		return 1;   /* a plain public name */` |
|        - | 1681 | `	}` |
|       49 | 1682 | `	if( nKey < 3 \|\| zKey[1] == 0 ){` |
|      ! 0 | 1683 | `		return 0;   /* php: "Illegal member variable name" */` |
|        - | 1684 | `	}` |
|       49 | 1685 | `	nCls = 0;` |
|      429 | 1686 | `	while( nCls < nKey - 2 && zKey[1+nCls] != 0 ){` |
|      381 | 1687 | `		nCls++;` |
|        1 | 1688 | `	}` |
|       49 | 1689 | `	if( nCls >= nKey - 2 ){` |
|      ! 0 | 1690 | `		return 0;   /* php: "Corrupt member variable name" */` |
|        - | 1691 | `	}` |
|        - | 1692 | `	/* An ANONYMOUS class mangles its source location in as a SECOND NUL-separated` |
|        - | 1693 | `	 * part, so the property name is what follows the LAST NUL rather than the` |
|        - | 1694 | `	 * second one (php's anonclass_src_len step). The class STRING php shows is` |
|        - | 1695 | `	 * still only the first part — it prints that one as a C string. */` |
|       49 | 1696 | `	nSrc = 0;` |
|      313 | 1697 | `	while( nCls + 2 + nSrc < nKey && zKey[nCls+2+nSrc] != 0 ){` |
|      265 | 1698 | `		nSrc++;` |
|        1 | 1699 | `	}` |
|       49 | 1700 | `	SyStringInitFromBuf(pClass,&zKey[1],nCls);` |
|       49 | 1701 | `	if( nCls + nSrc + 2 != nKey ){` |
|      ! 0 | 1702 | `		nCls += nSrc + 1;` |
|      ! 0 | 1703 | `	}` |
|       49 | 1704 | `	SyStringInitFromBuf(pName,&zKey[nCls+2],nKey - nCls - 2);` |
|       49 | 1705 | `	return 1;` |
|       91 | 1706 | `}` |
|        - | 1707 | `/*` |
|        - | 1708 | `` * Emit a property's dump key: var_dump `["x"]=>` / `["p":"C":private]=>` /`` |
|        - | 1709 | `` * `["q":protected]=>`; print_r `[x] => ` / `[p:C:private] => ` /`` |
|        - | 1710 | `` * `[q:protected] => ` (php's exact annotations).`` |
|        - | 1711 | ` */` |
|      176 | 1712 | `static void OoDumpPropKey(SyBlob *pOut,ph7_class_instance *pThis,ph7_class_attr *pAttr,int ShowType)` |
|        4 | 1713 | `{` |
|      180 | 1714 | `	const char *zQ = ShowType ? "\"" : "";` |
|      180 | 1715 | `	if( SyStringLength(&pAttr->sName) > 0 && SyStringData(&pAttr->sName)[0] == 0 ){` |
|        - | 1716 | `		/* A MANGLED name stored raw — the __PHP_Incomplete_Class carrier's` |
|        - | 1717 | `		 * private/protected payload keys. php's dump unmangles them exactly as` |
|        - | 1718 | ``		 * it does an (array) cast's: `["bp":"PB":private]` / `["pp":protected]`. */`` |
|        - | 1719 | `		SyString sUnmCls, sUnmName;` |
|        9 | 1720 | `		if( PH7_UnmangleAttrName(SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),` |
|        - | 1721 | `			&sUnmCls,&sUnmName) ){` |
|        9 | 1722 | `			SyBlobFormat(&(*pOut),"[%s%z%s",zQ,&sUnmName,zQ);` |
|        9 | 1723 | `			if( sUnmCls.nByte == 1 && sUnmCls.zString[0] == '*' ){` |
|        5 | 1724 | `				SyBlobAppend(&(*pOut),":protected",sizeof(":protected")-1);` |
|        3 | 1725 | `			}else{` |
|        5 | 1726 | `				SyBlobFormat(&(*pOut),":%s%z%s:private",zQ,&sUnmCls,zQ);` |
|        - | 1727 | `			}` |
|        9 | 1728 | `			SyBlobAppend(&(*pOut),ShowType ? "]=>" : "] => ",ShowType ? 3 : 5);` |
|        9 | 1729 | `			return;` |
|        - | 1730 | `		}` |
|      ! 0 | 1731 | `	}` |
|      172 | 1732 | `	SyBlobFormat(&(*pOut),"[%s%z%s",zQ,&pAttr->sName,zQ);` |
|      172 | 1733 | `	if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|        7 | 1734 | `		ph7_class *pDecl = OoAttrDeclaringClass(pThis->pClass,pAttr);` |
|        7 | 1735 | `		SyBlobFormat(&(*pOut),":%s%z%s:private",zQ,&pDecl->sName,zQ);` |
|      169 | 1736 | `	}else if( pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
|        5 | 1737 | `		SyBlobAppend(&(*pOut),":protected",sizeof(":protected")-1);` |
|        2 | 1738 | `	}` |
|      172 | 1739 | `	SyBlobAppend(&(*pOut),ShowType ? "]=>" : "] => ",ShowType ? 3 : 5);` |
|       92 | 1740 | `}` |
|      238 | 1741 | `PH7_PRIVATE sxi32 PH7_ClassInstanceDump(SyBlob *pOut,ph7_class_instance *pThis,int ShowType,int nTab,int nDepth)` |
|        5 | 1742 | `{` |
|        - | 1743 | `	SyHashEntry *pEntry;` |
|        - | 1744 | `	ph7_value *pValue;` |
|        - | 1745 | `	sxi32 rc;` |
|        - | 1746 | `	int i;` |
|      243 | 1747 | `	if( nDepth > 31 ){` |
|        - | 1748 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|        - | 1749 | `		/* Nesting limit reached..halt immediately*/` |
|        5 | 1750 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|        5 | 1751 | `		return SXERR_LIMIT;` |
|        - | 1752 | `	}` |
|      239 | 1753 | `	rc = SXRET_OK;` |
|        - | 1754 | `	{` |
|        - | 1755 | `		/* A native class's PRESENTATION (php's get_debug_info): the shape it shows` |
|        - | 1756 | `		 * is not its storage — a DateTime shows date/timezone_type/timezone, a` |
|        - | 1757 | `		 * WeakReference shows ["object"] — and the slots underneath are hidden.` |
|        - | 1758 | `		 * Rendered exactly like a __debugInfo() array, which is what php does with` |
|        - | 1759 | `		 * it, and consulted first because php's handler wins over a userland` |
|        - | 1760 | `		 * method a native class cannot declare anyway. */` |
|        - | 1761 | `		ph7_value sPresent;` |
|      239 | 1762 | `		PH7_MemObjInit(pThis->pVm,&sPresent);` |
|      239 | 1763 | `		if( ph7_value_is_array(&sPresent) == 0 ){` |
|      239 | 1764 | `			ph7_hashmap *pPresent = PH7_NewHashmap(pThis->pVm,0,0);` |
|      239 | 1765 | `			if( pPresent ){` |
|      239 | 1766 | `				sPresent.x.pOther = pPresent;` |
|      239 | 1767 | `				MemObjSetType(&sPresent,MEMOBJ_HASHMAP);` |
|      117 | 1768 | `			}` |
|      117 | 1769 | `		}` |
|      234 | 1770 | `		if( (sPresent.iFlags & MEMOBJ_HASHMAP)` |
|      239 | 1771 | `		 && PH7_ClassInstancePresent(pThis,&sPresent,1) ){` |
|       62 | 1772 | `			ph7_hashmap *pMap = (ph7_hashmap *)sPresent.x.pOther;` |
|       62 | 1773 | `			DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,pMap->nEntry);` |
|       62 | 1774 | `			if( !ShowType ){` |
|       34 | 1775 | `				for( i = 0 ; i < nTab ; i++ ){` |
|      ! 0 | 1776 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      ! 0 | 1777 | `				}` |
|       34 | 1778 | `				SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       16 | 1779 | `			}` |
|       62 | 1780 | `			rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth,1);` |
|       62 | 1781 | `			for( i = 0 ; i < nTab ; i++ ){` |
|      ! 0 | 1782 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      ! 0 | 1783 | `			}` |
|       62 | 1784 | `			if( ShowType ){` |
|       29 | 1785 | `				SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|       15 | 1786 | `			}else{` |
|       34 | 1787 | `				SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        - | 1788 | `			}` |
|       62 | 1789 | `			PH7_MemObjRelease(&sPresent);` |
|       62 | 1790 | `			return rc;` |
|        - | 1791 | `		}` |
|      179 | 1792 | `		PH7_MemObjRelease(&sPresent);` |
|        - | 1793 | `	}` |
|        - | 1794 | `	{` |
|        - | 1795 | `		/* Both var_dump and print_r consult __debugInfo() (PHP behavior);` |
|        - | 1796 | `		 * var_export uses a separate renderer and never reaches here. When the` |
|        - | 1797 | `		 * method is present and returns an array, render that array's entries as` |
|        - | 1798 | `		 * the object body, with the header showing the debug array's count. The` |
|        - | 1799 | `		 * nDepth guard above protects against a __debugInfo returning the object` |
|        - | 1800 | `		 * itself. */` |
|      179 | 1801 | `		ph7_class_method *pDbg = PH7_ClassExtractMethod(pThis->pClass,"__debugInfo",sizeof("__debugInfo")-1);` |
|      179 | 1802 | `		if( pDbg ){` |
|        - | 1803 | `			ph7_value sResult;` |
|       14 | 1804 | `			PH7_MemObjInit(pThis->pVm,&sResult);` |
|       14 | 1805 | `			PH7_VmCallMagicMethod(pThis->pVm,pThis,pDbg,&sResult,0,0);` |
|       14 | 1806 | `			if( sResult.iFlags & MEMOBJ_HASHMAP ){` |
|       14 | 1807 | `				ph7_hashmap *pMap = (ph7_hashmap *)sResult.x.pOther;` |
|        - | 1808 | `				/* Header count is the debug array's entry count. */` |
|       14 | 1809 | `				DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,pMap->nEntry);` |
|       14 | 1810 | `				if( !ShowType ){` |
|        6 | 1811 | `					for( i = 0 ; i < nTab ; i++ ){` |
|      ! 0 | 1812 | `						SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      ! 0 | 1813 | `					}` |
|        6 | 1814 | `					SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|        2 | 1815 | `				}` |
|       14 | 1816 | `				rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth,1);` |
|       14 | 1817 | `				for( i = 0 ; i < nTab ; i++ ){` |
|      ! 0 | 1818 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      ! 0 | 1819 | `				}` |
|       14 | 1820 | `				if( ShowType ){` |
|       10 | 1821 | `					SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|        6 | 1822 | `				}else{` |
|        6 | 1823 | `					SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        - | 1824 | `				}` |
|       14 | 1825 | `				PH7_MemObjRelease(&sResult);` |
|       14 | 1826 | `				return rc;` |
|        - | 1827 | `			}` |
|        - | 1828 | `			/* Non-array return: behave as if __debugInfo were absent. */` |
|      ! 0 | 1829 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 | 1830 | `		}` |
|        - | 1831 | `	}` |
|        - | 1832 | `	{` |
|        - | 1833 | `		/* var_dump's header needs the property count up front, so pre-count the` |
|        - | 1834 | `		 * non-static/non-constant attributes (matching the dump loop below). */` |
|      167 | 1835 | `		sxu32 nProp = 0;` |
|      167 | 1836 | `		if( ShowType ){` |
|      147 | 1837 | `			SyHashResetLoopCursor(&pThis->hAttr);` |
|      305 | 1838 | `			while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|      161 | 1839 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      161 | 1840 | `				if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_HOOK_VIRTUAL\|PH7_CLASS_ATTR_HIDDEN)) == 0 ){` |
|      157 | 1841 | `					nProp++;` |
|       77 | 1842 | `				}` |
|        3 | 1843 | `			}` |
|       72 | 1844 | `		}` |
|      167 | 1845 | `		DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,nProp);` |
|        - | 1846 | `	}` |
|      167 | 1847 | `	if( !ShowType ){` |
|        - | 1848 | `		/* print_r body opener: '(' at the container indent */` |
|      134 | 1849 | `		for( i = 0 ; i < nTab ; i++ ){` |
|      115 | 1850 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       59 | 1851 | `		}` |
|       22 | 1852 | `		SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|        9 | 1853 | `	}` |
|        - | 1854 | `	/* Dump object attributes (php 8.4: VIRTUAL hooked properties have no` |
|        - | 1855 | `	 * backing store — excluded from var_dump/print_r) */` |
|      167 | 1856 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      306 | 1857 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|      186 | 1858 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      186 | 1859 | `		if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_HOOK_VIRTUAL\|PH7_CLASS_ATTR_HIDDEN)) == 0 ){` |
|        - | 1860 | `			/* Dump non-static/constant attribute only */` |
|      180 | 1861 | `			pValue = ExtractClassAttrValue(pThis->pVm,pVmAttr);` |
|      180 | 1862 | `			if( pValue == 0 ){` |
|      ! 0 | 1863 | `				continue;` |
|        - | 1864 | `			}` |
|      180 | 1865 | `			if( ShowType ){` |
|        - | 1866 | ``				/* var_dump prop: `["x"(:…)]=>` at nTab+2, the value on the next`` |
|        - | 1867 | `				 * line at the same indent (php). */` |
|     4201 | 1868 | `				for( i = 0 ; i < nTab + 2 ; i++ ){` |
|     4047 | 1869 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     2025 | 1870 | `				}` |
|      157 | 1871 | `				OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,TRUE);` |
|      157 | 1872 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      157 | 1873 | `				rc = PH7_MemObjDump(&(*pOut),pValue,TRUE,nTab+2,nDepth,0);` |
|      157 | 1874 | `				if( rc == SXERR_LIMIT ){` |
|      125 | 1875 | `					break;` |
|        - | 1876 | `				}` |
|       18 | 1877 | `			}else{` |
|        - | 1878 | ``				/* print_r prop: `[x(:…)] => value` at nTab+4; container values`` |
|        - | 1879 | `				 * render their block at nTab+8 followed by php's blank line. */` |
|      177 | 1880 | `				for( i = 0 ; i < nTab + 4 ; i++ ){` |
|      155 | 1881 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       79 | 1882 | `				}` |
|       25 | 1883 | `				OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,FALSE);` |
|       22 | 1884 | `				if( (pValue->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ))` |
|       14 | 1885 | `				 && (pValue->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 | 1886 | `					rc = PH7_MemObjDump(&(*pOut),pValue,FALSE,nTab+8,nDepth,0);` |
|      ! 0 | 1887 | `					SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      ! 0 | 1888 | `					if( rc == SXERR_LIMIT ){` |
|      ! 0 | 1889 | `						break;` |
|        - | 1890 | `					}` |
|      ! 0 | 1891 | `				}else{` |
|       25 | 1892 | `					PH7_MemObjPrintRInline(&(*pOut),pValue);` |
|       25 | 1893 | `					SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        - | 1894 | `				}` |
|        - | 1895 | `			}` |
|       26 | 1896 | `		}` |
|        4 | 1897 | `	}` |
|     4019 | 1898 | `	for( i = 0 ; i < nTab ; i++ ){` |
|     3856 | 1899 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     1930 | 1900 | `	}` |
|      167 | 1901 | `	if( ShowType ){` |
|      147 | 1902 | `		SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|       75 | 1903 | `	}else{` |
|       22 | 1904 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        - | 1905 | `	}` |
|      167 | 1906 | `	return rc;` |
|      124 | 1907 | `}` |
|        - | 1908 | `/*` |
|        - | 1909 | ` * Call a magic method [i.e: __toString(),__toBool(),__Invoke()...]` |
|        - | 1910 | ` * Return SXRET_OK on successfull call. Any other return value indicates failure.` |
|        - | 1911 | ` * Notes on magic methods.` |
|        - | 1912 | ` * According to the PHP language reference manual.` |
|        - | 1913 | ` *  The function names __construct(), __destruct(), __call(), __callStatic()` |
|        - | 1914 | ` *  __get(),  __toString(), __invoke(), __clone() are magical in PHP classes.` |
|        - | 1915 | ` * You cannot have functions with these names in any of your classes unless` |
|        - | 1916 | ` * you want the magic functionality associated with them.` |
|        - | 1917 | ` * Example of magical methods:` |
|        - | 1918 | ` * __toString()` |
|        - | 1919 | ` *  The __toString() method allows a class to decide how it will react when it is treated like` |
|        - | 1920 | ` *  a string. For example, what echo $obj; will print. This method must return a string.` |
|        - | 1921 | ` *  Example #2 Simple example` |
|        - | 1922 | ` * <?php` |
|        - | 1923 | ` * // Declare a simple class` |
|        - | 1924 | ` * class TestClass` |
|        - | 1925 | ` * {` |
|        - | 1926 | ` *   public $foo;` |
|        - | 1927 | ` *` |
|        - | 1928 | ` *   public function __construct($foo)` |
|        - | 1929 | ` *   {` |
|        - | 1930 | ` *       $this->foo = $foo;` |
|        - | 1931 | ` *   }` |
|        - | 1932 | ` *` |
|        - | 1933 | ` *   public function __toString()` |
|        - | 1934 | ` *   {` |
|        - | 1935 | ` *       return $this->foo;` |
|        - | 1936 | ` *   }` |
|        - | 1937 | ` * }` |
|        - | 1938 | ` * $class = new TestClass('Hello');` |
|        - | 1939 | ` * echo $class;` |
|        - | 1940 | ` * ?>` |
|        - | 1941 | ` * The above example will output:` |
|        - | 1942 | ` *  Hello` |
|        - | 1943 | ` *` |
|        - | 1944 | ` * Note that PH7 does not support all the magical method and introudces __toFloat(),__toInt()` |
|        - | 1945 | ` * which have the same behaviour as __toString() but for float and integer types` |
|        - | 1946 | ` * respectively.` |
|        - | 1947 | ` * Refer to the official documentation for more information.` |
|        - | 1948 | ` */` |
|      484 | 1949 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallMagicMethod(` |
|        - | 1950 | `	ph7_vm *pVm,               /* VM that own all this stuff */` |
|        - | 1951 | `	ph7_class *pClass,         /* Target class */` |
|        - | 1952 | `	ph7_class_instance *pThis, /* Target object */` |
|        - | 1953 | `	const char *zMethod,       /* Magic method name [i.e: __toString()]*/` |
|        - | 1954 | `	sxu32 nByte,               /* zMethod length*/` |
|        - | 1955 | `	const SyString *pAttrName, /* Attribute name */` |
|        - | 1956 | `	ph7_value *pResult         /* OUT: magic method return value. NULL to discard */` |
|        - | 1957 | `	)` |
|        5 | 1958 | `{` |
|      489 | 1959 | `	ph7_value *apArg[2] = { 0 , 0 };` |
|        - | 1960 | `	ph7_class_method *pMeth;` |
|        - | 1961 | `	ph7_value sAttr; /* cc warning */` |
|        - | 1962 | `	sxi32 rc;` |
|        - | 1963 | `	int nArg;` |
|        - | 1964 | `	/* Make sure the magic method is available */` |
|      489 | 1965 | `	pMeth = PH7_ClassExtractMethod(&(*pClass),zMethod,nByte);` |
|      489 | 1966 | `	if( pMeth == 0 ){` |
|        - | 1967 | `		/* No such method,return immediately */` |
|      ! 0 | 1968 | `		return SXERR_NOTFOUND;` |
|        - | 1969 | `	}` |
|      489 | 1970 | `	nArg = 0;` |
|        - | 1971 | `	/* Copy arguments */` |
|      489 | 1972 | `	if( pAttrName ){` |
|      489 | 1973 | `		PH7_MemObjInitFromString(pVm,&sAttr,pAttrName);` |
|      489 | 1974 | `		sAttr.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      489 | 1975 | `		apArg[0] = &sAttr;` |
|      489 | 1976 | `		nArg = 1;` |
|      242 | 1977 | `	}` |
|        - | 1978 | `	/* Call the magic method now */` |
|      489 | 1979 | `	rc = PH7_VmCallMagicMethod(pVm,&(*pThis),pMeth,pResult,nArg,apArg);` |
|        - | 1980 | `	/* Clean up */` |
|      489 | 1981 | `	if( pAttrName ){` |
|      489 | 1982 | `		PH7_MemObjRelease(&sAttr);` |
|      242 | 1983 | `	}` |
|      489 | 1984 | `	return rc;` |
|      247 | 1985 | `}` |
|        - | 1986 | `/*` |
|        - | 1987 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon].` |
|        - | 1988 | ` * This function is simply a wrapper on ExtractClassAttrValue().` |
|        - | 1989 | ` */` |
|  5822170 | 1990 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceExtractAttrValue(ph7_class_instance *pThis,VmClassAttr *pAttr)` |
|        5 | 1991 | `{` |
|        - | 1992 | `   /* Extract the attribute value */` |
|        - | 1993 | `	ph7_value *pValue;` |
|  5822175 | 1994 | `	pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|  5822175 | 1995 | `	return pValue;` |
|        5 | 1996 | `}` |
|        - | 1997 | `/*` |
|        - | 1998 | ` * Convert a class instance [i.e: Object in the PHP jargon] into a hashmap [i.e: array in the PHP jargon].` |
|        - | 1999 | ` * Return SXRET_OK on success. Any other value indicates failure.` |
|        - | 2000 | ` * Note on object conversion to array:` |
|        - | 2001 | ` *  Acccording to the PHP language reference manual` |
|        - | 2002 | ` *  If an object is converted to an array, the result is an array whose elements are the object's properties.` |
|        - | 2003 | ` *  The keys are the member variable names.` |
|        - | 2004 | ` *` |
|        - | 2005 | ` *  The following example:` |
|        - | 2006 | ` *  class Test {` |
|        - | 2007 | ` *   public $A = 25<<1;  // 50` |
|        - | 2008 | ` *	 public $c = rand_str(3);   // Random string of length 3` |
|        - | 2009 | ` *	 public $d = rand() & 1023; // Random number between 0..1023` |
|        - | 2010 | ` *  }` |
|        - | 2011 | ` *  var_dump((array) new Test());` |
|        - | 2012 | ` *	Will output:` |
|        - | 2013 | ` *  array(3) {` |
|        - | 2014 | ` *   [A] =>` |
|        - | 2015 | ` *      int(50)` |
|        - | 2016 | ` *   [c] =>` |
|        - | 2017 | ` *     string(3 'aps')` |
|        - | 2018 | ` *   [d] =>` |
|        - | 2019 | ` *     int(991)` |
|        - | 2020 | ` *  }` |
|        - | 2021 | ` * You have noticed that PH7 allow class attributes [i.e: $a,$c,$d in the example above]` |
|        - | 2022 | ` * have any complex expression (even function calls/Annonymous functions) as their default` |
|        - | 2023 | ` * value unlike the standard PHP engine.` |
|        - | 2024 | ` * This is a very powerful feature that you have to look at.` |
|        - | 2025 | ` */` |
|      108 | 2026 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|        4 | 2027 | `{` |
|        - | 2028 | `	SyHashEntry *pEntry;` |
|        - | 2029 | `	SyString *pAttrName;` |
|        - | 2030 | `	VmClassAttr *pAttr;` |
|        - | 2031 | `	ph7_value *pValue;` |
|        - | 2032 | `	ph7_value sName;` |
|        - | 2033 | `	{` |
|        - | 2034 | `		/* php's get_properties handler, which is what the (array) cast reads: a` |
|        - | 2035 | `		 * DateTime casts to date/timezone_type/timezone, not to the hidden slots` |
|        - | 2036 | `		 * holding its timestamp. A class whose hook answers only the DEBUG surface` |
|        - | 2037 | `		 * (WeakReference) fills nothing here, and that EMPTY answer is php's — the` |
|        - | 2038 | `		 * hook is authoritative, so there is no falling back to the slot walk. It` |
|        - | 2039 | `		 * used to fall back when the hook filled nothing, which was invisible while` |
|        - | 2040 | `		 * every hooked class also had no visible property: an ArrayObject SUBCLASS` |
|        - | 2041 | ``		 * with an empty storage would have cast to its own `p` where php casts to`` |
|        - | 2042 | `		 * the (empty) storage. */` |
|        - | 2043 | `		ph7_value sPresent;` |
|      112 | 2044 | `		PH7_MemObjInit(pThis->pVm,&sPresent);` |
|      112 | 2045 | `		sPresent.x.pOther = pMap;` |
|      112 | 2046 | `		MemObjSetType(&sPresent,MEMOBJ_HASHMAP);` |
|      112 | 2047 | `		if( PH7_ClassInstancePresent(pThis,&sPresent,0) ){` |
|        - | 2048 | `			/* The map IS the destination; do not release the carrier's hashmap. */` |
|       41 | 2049 | `			sPresent.x.pOther = 0;` |
|       41 | 2050 | `			sPresent.iFlags = MEMOBJ_NULL;` |
|       41 | 2051 | `			return SXRET_OK;` |
|        - | 2052 | `		}` |
|       72 | 2053 | `		sPresent.x.pOther = 0;` |
|       72 | 2054 | `		sPresent.iFlags = MEMOBJ_NULL;` |
|       72 | 2055 | `		PH7_MemObjRelease(&sPresent);` |
|        - | 2056 | `	}` |
|        - | 2057 | `	/* Reset the loop cursor */` |
|       72 | 2058 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|       72 | 2059 | `	PH7_MemObjInitFromString(pThis->pVm,&sName,0);` |
|      246 | 2060 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|        - | 2061 | `		/* Point to the current attribute */` |
|      178 | 2062 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      178 | 2063 | `		if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_HIDDEN) ){` |
|        - | 2064 | `			/* A static property is the CLASS's, not the object's: php's cast` |
|        - | 2065 | `			 * yields only the instance's own properties. */` |
|       54 | 2066 | `			continue;` |
|        - | 2067 | `		}` |
|      126 | 2068 | `		if( pAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|        - | 2069 | `			/* php 8.4: a VIRTUAL hooked property has no backing store — the` |
|        - | 2070 | `			 * (array) cast excludes it (raw surface, get is NOT dispatched) */` |
|        7 | 2071 | `			continue;` |
|        - | 2072 | `		}` |
|        - | 2073 | `		/* Extract attribute value */` |
|      120 | 2074 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|      120 | 2075 | `		if( pValue ){` |
|        - | 2076 | `			/* Build attribute name. php MANGLES the key of a non-public property` |
|        - | 2077 | `			 * when it casts an object to an array: a private one becomes` |
|        - | 2078 | `			 * "\0DeclaringClass\0name" and a protected one "\0*\0name", so two` |
|        - | 2079 | `			 * same-named members from different visibility levels stay distinct` |
|        - | 2080 | ``			 * and `isset($arr['priv'])` is FALSE — PHL emitted the bare name,`` |
|        - | 2081 | `			 * which collided them and answered TRUE. The NULs are real bytes in` |
|        - | 2082 | `			 * the key (this append is length-based, not NUL-terminated). */` |
|      120 | 2083 | `			pAttrName = &pAttr->pAttr->sName;` |
|      120 | 2084 | `			if( pAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       17 | 2085 | `				ph7_class *pDecl = pAttr->pAttr->pDeclClass` |
|       10 | 2086 | `					? pAttr->pAttr->pDeclClass : pThis->pClass;` |
|       12 | 2087 | `				PH7_MemObjStringAppend(&sName,"\0",1);` |
|       12 | 2088 | `				PH7_MemObjStringAppend(&sName,SyStringData(&pDecl->sName),SyStringLength(&pDecl->sName));` |
|       12 | 2089 | `				PH7_MemObjStringAppend(&sName,"\0",1);` |
|      115 | 2090 | `			}else if( pAttr->pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
|       13 | 2091 | `				PH7_MemObjStringAppend(&sName,"\0*\0",3);` |
|        5 | 2092 | `			}` |
|      120 | 2093 | `			PH7_MemObjStringAppend(&sName,pAttrName->zString,pAttrName->nByte);` |
|        - | 2094 | `			/* Perform the insertion */` |
|      120 | 2095 | `			PH7_HashmapInsert(pMap,&sName,pValue);` |
|        - | 2096 | `			/* Reset the string cursor */` |
|      120 | 2097 | `			SyBlobReset(&sName.sBlob);` |
|       58 | 2098 | `		}` |
|        4 | 2099 | `	}` |
|       72 | 2100 | `	PH7_MemObjRelease(&sName);` |
|       72 | 2101 | `	return SXRET_OK;` |
|       58 | 2102 | `}` |
|        - | 2103 | `/*` |
|        - | 2104 | ` * Iterate throw class attributes and invoke the given callback [i.e: xWalk()] for each` |
|        - | 2105 | ` * retrieved attribute.` |
|        - | 2106 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|        - | 2107 | ` * the attribute value in the callback body will not alter the real attribute value.` |
|        - | 2108 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|        - | 2109 | ` * a value different from PH7_OK.` |
|        - | 2110 | ` * Refer to [ph7_object_walk()] for more information.` |
|        - | 2111 | ` */` |
|      ! 0 | 2112 | `PH7_PRIVATE sxi32 PH7_ClassInstanceWalk(` |
|        - | 2113 | `	ph7_class_instance *pThis, /* Target object */` |
|        - | 2114 | `	int (*xWalk)(const char *,ph7_value *,void *), /* Walker callback */` |
|        - | 2115 | `	void *pUserData /* Last argument to xWalk() */` |
|        - | 2116 | `	)` |
|      ! 0 | 2117 | `{` |
|        - | 2118 | `	SyHashEntry *pEntry; /* Hash entry */` |
|        - | 2119 | `	VmClassAttr *pAttr;  /* Pointer to the attribute */` |
|        - | 2120 | `	ph7_value *pValue;   /* Attribute value */` |
|        - | 2121 | `	ph7_value sValue;    /* Copy of the attribute value */` |
|        - | 2122 | `	int rc;` |
|        - | 2123 | `	/* Reset the loop cursor */` |
|      ! 0 | 2124 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      ! 0 | 2125 | `	PH7_MemObjInit(pThis->pVm,&sValue);` |
|        - | 2126 | `	/* Start the walk process */` |
|      ! 0 | 2127 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|        - | 2128 | `		/* Point to the current attribute */` |
|      ! 0 | 2129 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      ! 0 | 2130 | `		if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_HIDDEN) ){` |
|        - | 2131 | `			/* Class-level members are not part of the object (php) */` |
|      ! 0 | 2132 | `			continue;` |
|        - | 2133 | `		}` |
|        - | 2134 | `		/* Extract attribute value */` |
|      ! 0 | 2135 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|      ! 0 | 2136 | `		if( pValue ){` |
|      ! 0 | 2137 | `			PH7_MemObjLoad(pValue,&sValue);` |
|        - | 2138 | `			/* Invoke the supplied callback */` |
|      ! 0 | 2139 | `			rc =  xWalk(SyStringData(&pAttr->pAttr->sName),&sValue,pUserData);` |
|      ! 0 | 2140 | `			PH7_MemObjRelease(&sValue);` |
|      ! 0 | 2141 | `			if( rc != PH7_OK){` |
|        - | 2142 | `				/* User callback request an operation abort */` |
|      ! 0 | 2143 | `				return SXERR_ABORT;` |
|        - | 2144 | `			}` |
|      ! 0 | 2145 | `		}` |
|      ! 0 | 2146 | `	}` |
|        - | 2147 | `	/* All done */` |
|      ! 0 | 2148 | `	return SXRET_OK;` |
|      ! 0 | 2149 | `}` |
|        - | 2150 | `/*` |
|        - | 2151 | ` * The instance's attribute entry for a property name, INCLUDING the empty one.` |
|        - | 2152 | ` *` |
|        - | 2153 | ` * SyHashGet answers 0 for a zero-length key engine-wide, so a property named ""` |
|        - | 2154 | `` * — php's own `s:0:""` payload builds one, and every surface that walks hAttr`` |
|        - | 2155 | ` * shows it — can only be found by walking the list. Used by the presentation` |
|        - | 2156 | ` * surfaces that snapshot names and re-look-up each one before reading it (a PHP` |
|        - | 2157 | ` * 8.4 get hook may unset a property mid-walk), where the plain hash probe would` |
|        - | 2158 | ` * silently drop it from the output while var_dump, which walks directly, showed` |
|        - | 2159 | ` * it. The walk uses the hash's embedded loop cursor, so it must not run inside` |
|        - | 2160 | ` * another walk of the SAME table — the callers below re-enter only through the` |
|        - | 2161 | ` * hook dispatch, which happens after this returns.` |
|        - | 2162 | ` */` |
|      312 | 2163 | `PH7_PRIVATE SyHashEntry * PH7_ClassInstanceAttrEntry(ph7_class_instance *pThis,const char *zName,sxu32 nName)` |
|        5 | 2164 | `{` |
|        - | 2165 | `	SyHashEntry *pEntry;` |
|      317 | 2166 | `	if( nName > 0 ){` |
|      299 | 2167 | `		return SyHashGet(&pThis->hAttr,(const void *)zName,nName);` |
|        - | 2168 | `	}` |
|       19 | 2169 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|       29 | 2170 | `	while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 | 2171 | `		if( pEntry->nKeyLen == 0 ){` |
|       11 | 2172 | `			return pEntry;` |
|        - | 2173 | `		}` |
|        1 | 2174 | `	}` |
|        9 | 2175 | `	return 0;` |
|      161 | 2176 | `}` |
|        - | 2177 | `/*` |
|        - | 2178 | ` * Extract a class atrribute value.` |
|        - | 2179 | ` * Return a pointer to the attribute value on success. Otherwise NULL.` |
|        - | 2180 | ` * Note:` |
|        - | 2181 | ` *  Access to static and constant attribute is not allowed. That is,the function` |
|        - | 2182 | ` *  will return NULL in case someone (host-application code) try to extract` |
|        - | 2183 | ` *  a static/constant attribute.` |
|        - | 2184 | ` */` |
|  1556646 | 2185 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceFetchAttr(ph7_class_instance *pThis,const SyString *pName)` |
|        5 | 2186 | `{` |
|        - | 2187 | `	SyHashEntry *pEntry;` |
|        - | 2188 | `	VmClassAttr *pAttr;` |
|        - | 2189 | `	/* Query the attribute hashtable */` |
|  1556651 | 2190 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);` |
|  1556651 | 2191 | `	if( pEntry == 0 ){` |
|        - | 2192 | `		/* No such attribute */` |
|       80 | 2193 | `		return 0;` |
|        - | 2194 | `	}` |
|        - | 2195 | `	/* Point to the class atrribute */` |
|  1556573 | 2196 | `	pAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - | 2197 | `	/* Check if we are dealing with a static/constant attribute */` |
|  1556573 | 2198 | `	if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - | 2199 | `		/* Access is forbidden */` |
|      ! 0 | 2200 | `		return 0;` |
|        - | 2201 | `	}` |
|        - | 2202 | `	/* Return the attribute value */` |
|  1556573 | 2203 | `	return ExtractClassAttrValue(pThis->pVm,pAttr);` |
|   778333 | 2204 | `}` |
|        - | 2205 | `/*` |
|        - | 2206 | `` * Does a WRITE through `$obj[k]` land on this class's storage? php answers with`` |
|        - | 2207 | ` * the class's read_dimension handler: an internal class whose own handler hands` |
|        - | 2208 | ` * back the real element supports indirect modification, while everything routed` |
|        - | 2209 | ` * through zend_std_read_dimension — every userland ArrayAccess, and the SPL` |
|        - | 2210 | ` * classes that keep the standard handler (SplFixedArray, the SplDoublyLinkedList` |
|        - | 2211 | ` * family, SplObjectStorage) — gets a TEMPORARY back, so the write is lost and php` |
|        - | 2212 | ` * says so. The engine cannot tell those apart from the interface alone: both` |
|        - | 2213 | ` * implement ArrayAccess.` |
|        - | 2214 | ` *` |
|        - | 2215 | ` * PHL's equivalent evidence is the offsetGet that ANSWERS: the native one on a` |
|        - | 2216 | ` * class carrying PH7_CLASS_DIM_WRITABLE means the storage is reachable, and a` |
|        - | 2217 | ` * userland OVERRIDE of it means it is not — which is php's own rule for an` |
|        - | 2218 | ` * ArrayObject subclass (spl_array_read_dimension steps aside for a declared` |
|        - | 2219 | `` * offsetGet). A `&offsetGet` return is php's other yes: it hands back a reference,`` |
|        - | 2220 | ` * so the write reaches whatever it aliases.` |
|        - | 2221 | ` */` |
|      446 | 2222 | `PH7_PRIVATE int PH7_VmDimFetchWritable(ph7_class *pClass)` |
|        5 | 2223 | `{` |
|        - | 2224 | `	ph7_class_method *pGet;` |
|        - | 2225 | `	ph7_class *pCur;` |
|      451 | 2226 | `	if( pClass == 0 ){` |
|      ! 0 | 2227 | `		return FALSE;` |
|        - | 2228 | `	}` |
|      451 | 2229 | `	pGet = PH7_ClassExtractMethod(pClass,"offsetGet",sizeof("offsetGet")-1);` |
|      451 | 2230 | `	if( pGet == 0 ){` |
|      ! 0 | 2231 | `		return FALSE;` |
|        - | 2232 | `	}` |
|      451 | 2233 | `	if( pGet->sFunc.iFlags & VM_FUNC_REF_RETURN ){` |
|        5 | 2234 | `		return TRUE;` |
|        - | 2235 | `	}` |
|      447 | 2236 | `	if( (pGet->sFunc.iFlags & VM_FUNC_NATIVE) == 0 ){` |
|      159 | 2237 | `		return FALSE;` |
|        - | 2238 | `	}` |
|      434 | 2239 | `	for( pCur = pClass ; pCur ; pCur = pCur->pBase ){` |
|      342 | 2240 | `		if( pCur->iFlags & PH7_CLASS_DIM_WRITABLE ){` |
|      198 | 2241 | `			return TRUE;` |
|        - | 2242 | `		}` |
|       73 | 2243 | `	}` |
|       93 | 2244 | `	return FALSE;` |
|      228 | 2245 | `}` |
|        - | 2246 | `/*` |
|        - | 2247 | ` * The three accessors a VM_FUNC_NATIVE method body uses to reach its receiver.` |
|        - | 2248 | ` *` |
|        - | 2249 | ` * A native method is dispatched down the host-function path, so it is handed the` |
|        - | 2250 | ` * plain (ph7_context*, argc, argv) of any builtin — the receiver rides on the` |
|        - | 2251 | ` * context instead of occupying an argument slot. That is the whole point: the C` |
|        - | 2252 | `` * routine that used to be a global `__prefix_verb($target, ...)` thunk taking its`` |
|        - | 2253 | ` * target explicitly becomes a method whose target is $this.` |
|        - | 2254 | ` */` |
|        - | 2255 | `/*` |
|        - | 2256 | ` * The receiver, or NULL when the method was called statically (and always NULL in` |
|        - | 2257 | ` * a plain host function). Borrowed: the operand stack holds the reference for the` |
|        - | 2258 | ` * duration of the call, so the body must not unref it.` |
|        - | 2259 | ` */` |
|  1488224 | 2260 | `PH7_PRIVATE ph7_class_instance * PH7_ContextThis(ph7_context *pCtx)` |
|        5 | 2261 | `{` |
|  1488229 | 2262 | `	return pCtx->pThis;` |
|        5 | 2263 | `}` |
|        - | 2264 | `/*` |
|        - | 2265 | ` * The class the call was made THROUGH — php's late-static-binding target, so an` |
|        - | 2266 | ` * inherited native method sees the SUBCLASS here, not the class that declared it.` |
|        - | 2267 | ` * NULL in a plain host function.` |
|        - | 2268 | ` */` |
|      314 | 2269 | `PH7_PRIVATE ph7_class * PH7_ContextCalledClass(ph7_context *pCtx)` |
|        2 | 2270 | `{` |
|      316 | 2271 | `	return pCtx->pCalledClass;` |
|        2 | 2272 | `}` |
|        - | 2273 | `/*` |
|        - | 2274 | ` * The receiver as a ph7_value, so the body can use the ordinary object helpers` |
|        - | 2275 | ` * (ph7_object_fetch_attr, ph7_value_is_object, ph7_result_value, ...) instead of` |
|        - | 2276 | ` * reaching into ph7_class_instance. NULL when there is no receiver.` |
|        - | 2277 | ` *` |
|        - | 2278 | ` * The view ALIASES the instance without bumping its refcount: it lives exactly as` |
|        - | 2279 | ` * long as the call, during which the caller's reference is already keeping the` |
|        - | 2280 | ` * object alive, and VmReleaseCallContext drops the alias without unreffing. Handing` |
|        - | 2281 | ` * it to ph7_result_value() is safe — that copies through PH7_MemObjStore, which` |
|        - | 2282 | ` * takes its own reference.` |
|        - | 2283 | ` */` |
|     5430 | 2284 | `PH7_PRIVATE ph7_value * PH7_ContextThisValue(ph7_context *pCtx)` |
|        5 | 2285 | `{` |
|     5435 | 2286 | `	if( pCtx->pThis == 0 ){` |
|      ! 0 | 2287 | `		return 0;` |
|        - | 2288 | `	}` |
|     5435 | 2289 | `	if( !pCtx->bThisInit ){` |
|     5435 | 2290 | `		PH7_MemObjInit(pCtx->pVm,&pCtx->sThis);` |
|     5435 | 2291 | `		pCtx->sThis.x.pOther = pCtx->pThis;` |
|     5435 | 2292 | `		pCtx->sThis.iFlags = MEMOBJ_OBJ;` |
|     5435 | 2293 | `		pCtx->sThis.nIdx = SXU32_HIGH;` |
|     5435 | 2294 | `		pCtx->bThisInit = 1;` |
|     2715 | 2295 | `	}` |
|     5435 | 2296 | `	return &pCtx->sThis;` |
|     2720 | 2297 | `}` |
|        - | 2298 |  |
