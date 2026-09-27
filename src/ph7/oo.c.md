# src/ph7/oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 929/1060 lines (87.64%)

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
|   860366 |   14 | `PH7_PRIVATE ph7_class * PH7_NewRawClass(ph7_vm *pVm,const SyString *pName,sxu32 nLine)` |
|        5 |   15 | `{` |
|        - |   16 | `	ph7_class *pClass;` |
|        - |   17 | `	char *zName;` |
|        - |   18 | `	/* Allocate a new instance */` |
|   860371 |   19 | `	pClass = (ph7_class *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class));` |
|   860371 |   20 | `	if( pClass == 0 ){` |
|      ! 0 |   21 | `		return 0;` |
|        - |   22 | `	}` |
|        - |   23 | `	/* Zero the structure */` |
|   860371 |   24 | `	SyZero(pClass,sizeof(ph7_class));` |
|        - |   25 | `	/* Duplicate class name */` |
|   860371 |   26 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   860371 |   27 | `	if( zName == 0 ){` |
|      ! 0 |   28 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClass);` |
|      ! 0 |   29 | `		return 0;` |
|        - |   30 | `	}` |
|        - |   31 | `	/* Initialize fields */` |
|   860371 |   32 | `	SyStringInitFromBuf(&pClass->sName,zName,pName->nByte);` |
|        - |   33 | `	/* php method names are CASE-INSENSITIVE ($o->FOO() finds foo(), and declaring` |
|        - |   34 | `	 * both is a redeclaration), so the method table matches on them the same way` |
|        - |   35 | `	 * hClass does for class names. Properties and class constants ARE case` |
|        - |   36 | `	 * sensitive in php, so hAttr keeps the default exact comparator. */` |
|   860371 |   37 | `	SyHashInit(&pClass->hMethod,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|   860371 |   38 | `	SyHashInit(&pClass->hAttr,&pVm->sAllocator,0,0);` |
|   860371 |   39 | `	SyHashInit(&pClass->hConst,&pVm->sAllocator,0,0);` |
|   860371 |   40 | `	SyHashInit(&pClass->hDerived,&pVm->sAllocator,0,0);` |
|   860371 |   41 | `	SySetInit(&pClass->aInterface,&pVm->sAllocator,sizeof(ph7_class *));` |
|   860371 |   42 | `	SySetInit(&pClass->aTrait,&pVm->sAllocator,sizeof(ph7_class *));` |
|   860371 |   43 | `	SySetInit(&pClass->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|   860371 |   44 | `	SySetInit(&pClass->aEnumCases,&pVm->sAllocator,sizeof(ph7_class_attr *));` |
|   860371 |   45 | `	pClass->nLine = nLine;` |
|   860371 |   46 | `	if( pVm->bCompilingBuiltin ){` |
|        - |   47 | `		/* Defined by an embedded builtin chunk: internal, no defining file.` |
|        - |   48 | `		 * Class compilers merge further flags with \|= so this survives. */` |
|   856407 |   49 | `		pClass->iFlags \|= PH7_CLASS_INTERNAL;` |
|   428206 |   50 | `	}else{` |
|        - |   51 | `		/* Alias the VM-lifetime path dup on top of the include stack */` |
|     3969 |   52 | `		SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     3969 |   53 | `		if( pFile ){` |
|     3969 |   54 | `			SyStringDupPtr(&pClass->sFile,pFile);` |
|     1982 |   55 | `		}` |
|        - |   56 | `	}` |
|        - |   57 | `	/* All done */` |
|   860371 |   58 | `	return pClass;` |
|   430188 |   59 | `}` |
|        - |   60 | `/*` |
|        - |   61 | ` * Allocate and initialize a new class attribute.` |
|        - |   62 | ` * Return a pointer to the class attribute on success. NULL otherwise.` |
|        - |   63 | ` */` |
|  2067594 |   64 | `PH7_PRIVATE ph7_class_attr * PH7_NewClassAttr(ph7_vm *pVm,const SyString *pName,sxu32 nLine,sxi32 iProtection,sxi32 iFlags)` |
|        5 |   65 | `{` |
|        - |   66 | `	ph7_class_attr *pAttr;` |
|        - |   67 | `	char *zName;` |
|  2067599 |   68 | `	pAttr = (ph7_class_attr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_attr));` |
|  2067599 |   69 | `	if( pAttr == 0 ){` |
|      ! 0 |   70 | `		return 0;` |
|        - |   71 | `	}` |
|        - |   72 | `	/* Zero the structure */` |
|  2067599 |   73 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|  2067599 |   74 | `	SySetInit(&pAttr->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|        - |   75 | `	/* Duplicate attribute name */` |
|  2067599 |   76 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  2067599 |   77 | `	if( zName == 0 ){` |
|      ! 0 |   78 | `		SyMemBackendPoolFree(&pVm->sAllocator,pAttr);` |
|      ! 0 |   79 | `		return 0;` |
|        - |   80 | `	}` |
|        - |   81 | `	/* Initialize fields */` |
|  2067599 |   82 | `	SySetInit(&pAttr->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|  2067599 |   83 | `	SySetInit(&pAttr->aUnionAlts,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|  2067599 |   84 | `	SyStringInitFromBuf(&pAttr->sName,zName,pName->nByte);` |
|  2067599 |   85 | `	pAttr->iProtection = iProtection;` |
|  2067599 |   86 | `	pAttr->nIdx = SXU32_HIGH;` |
|  2067599 |   87 | `	pAttr->iFlags = iFlags;` |
|  2067599 |   88 | `	pAttr->nLine = nLine;` |
|  2067599 |   89 | `	return pAttr;` |
|  1033802 |   90 | `}` |
|        - |   91 | `/*` |
|        - |   92 | ` * Allocate and initialize a new class method.` |
|        - |   93 | ` * Return a pointer to the class method on success. NULL otherwise` |
|        - |   94 | ` * This function associate with the newly created method an automatically generated` |
|        - |   95 | ` * random unique name.` |
|        - |   96 | ` */` |
|  5342110 |   97 | `PH7_PRIVATE ph7_class_method * PH7_NewClassMethod(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,sxu32 nLine,` |
|        - |   98 | `	sxi32 iProtection,sxi32 iFlags,sxi32 iFuncFlags)` |
|        5 |   99 | `{` |
|        - |  100 | `	ph7_class_method *pMeth;` |
|        - |  101 | `	SyHashEntry *pEntry;` |
|        - |  102 | `	SyString *pNamePtr;` |
|        - |  103 | `	char zSalt[10];` |
|        - |  104 | `	char *zName;` |
|        - |  105 | `	sxu32 nByte;` |
|        - |  106 | `	/* Allocate a new class method instance */` |
|  5342115 |  107 | `	pMeth = (ph7_class_method *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_method));` |
|  5342115 |  108 | `	if( pMeth == 0 ){` |
|      ! 0 |  109 | `		return 0;` |
|        - |  110 | `	}` |
|        - |  111 | `	/* Zero the structure */` |
|  5342115 |  112 | `	SyZero(pMeth,sizeof(ph7_class_method));` |
|        - |  113 | `	/* Check for an already installed method with the same name */` |
|  5342115 |  114 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
|  5342115 |  115 | `	if( pEntry == 0 ){` |
|        - |  116 | `		/* Associate an unique VM name to this method */` |
|  5342111 |  117 | `		nByte = sizeof(zSalt) + pName->nByte + SyStringLength(&pClass->sName)+sizeof(char)*7/*[[__'\0'*/;` |
|  5342111 |  118 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,nByte);` |
|  5342111 |  119 | `		if( zName == 0 ){` |
|      ! 0 |  120 | `			SyMemBackendPoolFree(&pVm->sAllocator,pMeth);` |
|      ! 0 |  121 | `			return 0;` |
|        - |  122 | `		}` |
|  5342111 |  123 | `		pNamePtr = &pMeth->sVmName;` |
|        - |  124 | `		/* Generate a random string */` |
|  5342111 |  125 | `		PH7_VmRandomString(&(*pVm),zSalt,sizeof(zSalt));` |
|  5342111 |  126 | `		pNamePtr->nByte = SyBufferFormat(zName,nByte,"[__%z@%z_%.*s]",&pClass->sName,pName,sizeof(zSalt),zSalt);` |
|  5342111 |  127 | `		pNamePtr->zString = zName;` |
|  2671058 |  128 | `	}else{` |
|        - |  129 | `		/* Method is condidate for 'overloading' */` |
|        6 |  130 | `		ph7_class_method *pCurrent = (ph7_class_method *)pEntry->pUserData;` |
|        6 |  131 | `		pNamePtr = &pMeth->sVmName;` |
|        - |  132 | `		/* Use the same VM name */` |
|        6 |  133 | `		SyStringDupPtr(pNamePtr,&pCurrent->sVmName);` |
|        6 |  134 | `		zName = (char *)pNamePtr->zString;` |
|        - |  135 | `	}` |
|  5342115 |  136 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|    84325 |  137 | `		if( pName->nByte == sizeof("__destruct") - 1 && SyMemcmp(pName->zString,"__destruct",sizeof("__destruct") - 1 ) == 0 ){` |
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
|    42160 |  148 | `	}` |
|        - |  149 | `	/* Initialize method fields */` |
|  5342115 |  150 | `	pMeth->iProtection = iProtection;` |
|  5342115 |  151 | `	pMeth->iFlags = iFlags;` |
|  5342115 |  152 | `	pMeth->nLine = nLine;` |
|  8013170 |  153 | `	PH7_VmInitFuncState(&(*pVm),&pMeth->sFunc,&zName[sizeof(char)*4/*[__@*/+SyStringLength(&pClass->sName)],` |
|  5342110 |  154 | `		pName->nByte,iFuncFlags\|VM_FUNC_CLASS_METHOD,pClass);` |
|  5342115 |  155 | `	return pMeth;` |
|  2671060 |  156 | `}` |
|        - |  157 | `/*` |
|        - |  158 | ` * Check if the given name have a class method associated with it.` |
|        - |  159 | ` * Return the desired method [i.e: ph7_class_method instance] on success. NULL otherwise.` |
|        - |  160 | ` */` |
|  7022767 |  161 | `PH7_PRIVATE ph7_class_method * PH7_ClassExtractMethod(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|        5 |  162 | `{` |
|        - |  163 | `	SyHashEntry *pEntry;` |
|        - |  164 | `	/* Perform a hash lookup */` |
|  7022772 |  165 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)zName,nByte);` |
|  7022772 |  166 | `	if( pEntry == 0 ){` |
|        - |  167 | `		/* No such entry */` |
|  1781100 |  168 | `		return 0;` |
|        - |  169 | `	}` |
|        - |  170 | `	/* Point to the desired method */` |
|  5241677 |  171 | `	return (ph7_class_method *)pEntry->pUserData;` |
|  3511389 |  172 | `}` |
|        - |  173 | `/*` |
|        - |  174 | ` * Check if the given name is a class attribute.` |
|        - |  175 | ` * Return the desired attribute [i.e: ph7_class_attr instance] on success.NULL otherwise.` |
|        - |  176 | ` */` |
|   103066 |  177 | `PH7_PRIVATE ph7_class_attr * PH7_ClassExtractAttribute(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|        5 |  178 | `{` |
|        - |  179 | `	SyHashEntry *pEntry;` |
|        - |  180 | `	/* Perform a hash lookup */` |
|   103071 |  181 | `	pEntry = SyHashGet(&pClass->hAttr,(const void *)zName,nByte);` |
|   103071 |  182 | `	if( pEntry == 0 ){` |
|        - |  183 | `		/* No such entry */` |
|     2599 |  184 | `		return 0;` |
|        - |  185 | `	}` |
|        - |  186 | `	/* Point to the desierd method */` |
|   100477 |  187 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|    51538 |  188 | `}` |
|        - |  189 | `/*` |
|        - |  190 | ` * Check if the given name is a class CONSTANT (or enum case).` |
|        - |  191 | ` * php keeps constants and properties in separate namespaces, so constants live` |
|        - |  192 | ` * in a dedicated table (hConst) and never collide with a same-named property.` |
|        - |  193 | ` * Return the desired constant [ph7_class_attr with PH7_CLASS_ATTR_CONSTANT] on` |
|        - |  194 | ` * success, NULL otherwise.` |
|        - |  195 | ` */` |
|     2016 |  196 | `PH7_PRIVATE ph7_class_attr * PH7_ClassExtractConstant(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|        5 |  197 | `{` |
|        - |  198 | `	SyHashEntry *pEntry;` |
|     2021 |  199 | `	pEntry = SyHashGet(&pClass->hConst,(const void *)zName,nByte);` |
|     2021 |  200 | `	if( pEntry == 0 ){` |
|      485 |  201 | `		return 0;` |
|        - |  202 | `	}` |
|     1541 |  203 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|     1013 |  204 | `}` |
|        - |  205 | `/*` |
|        - |  206 | ` * Install a class attribute in the corresponding container.` |
|        - |  207 | ` * A constant (or enum case) goes to hConst, a property to hAttr — php's two` |
|        - |  208 | `` * separate member namespaces, so `const C` and `public $C` coexist.`` |
|        - |  209 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|        - |  210 | ` */` |
|  2067590 |  211 | `PH7_PRIVATE sxi32 PH7_ClassInstallAttr(ph7_class *pClass,ph7_class_attr *pAttr)` |
|        5 |  212 | `{` |
|  2067595 |  213 | `	SyString *pName = &pAttr->sName;` |
|        - |  214 | `	sxi32 rc;` |
|        - |  215 | `	/* Remember where this attribute was originally declared so that later` |
|        - |  216 | `	 * inheritance/trait copies still know the declaring class (needed for` |
|        - |  217 | `	 * PHP-compatible error messages on typed properties). */` |
|  2067595 |  218 | `	if( pAttr->pDeclClass == 0 ){` |
|    13281 |  219 | `		pAttr->pDeclClass = pClass;` |
|     6638 |  220 | `	}` |
|  2067595 |  221 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|   993595 |  222 | `		rc = SyHashInsertTail(&pClass->hConst,(const void *)pName->zString,pName->nByte,pAttr);` |
|   496800 |  223 | `	}else{` |
|  1074005 |  224 | `		rc = SyHashInsertTail(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|        - |  225 | `	}` |
|  2067595 |  226 | `	return rc;` |
|        5 |  227 | `}` |
|        - |  228 | `/*` |
|        - |  229 | ` * Install a class method in the corresponding container.` |
|        - |  230 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|        - |  231 | ` */` |
|  5342072 |  232 | `PH7_PRIVATE sxi32 PH7_ClassInstallMethod(ph7_class *pClass,ph7_class_method *pMeth)` |
|        5 |  233 | `{` |
|  5342077 |  234 | `	SyString *pName = &pMeth->sFunc.sName;` |
|        - |  235 | `	sxi32 rc;` |
|  5342077 |  236 | `	rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|  5342077 |  237 | `	return rc;` |
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
|      185 |  270 | `		return OVT_NONE; /* no declared type */` |
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
|       97 |  354 | `		if( bCovariant && kC == OVT_NONE && kP != OVT_NONE ) return 1;` |
|       97 |  355 | `		if( !bCovariant && kP == OVT_NONE && kC != OVT_NONE ) return 1;` |
|       97 |  356 | `		return 0;` |
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
|   399544 |  384 | `static sxi32 OoCheckOverrideCompat(ph7_gen_state *pGen, ph7_class *pBase, ph7_class *pSub,` |
|        - |  385 | `	ph7_class_method *pParent, ph7_class_method *pChild)` |
|        5 |  386 | `{` |
|   399549 |  387 | `	ph7_vm *pVm = pGen->pVm;` |
|   399549 |  388 | `	ph7_vm_func *pPF = &pParent->sFunc;` |
|   399549 |  389 | `	ph7_vm_func *pCF = &pChild->sFunc;` |
|   399549 |  390 | `	SyString *pMName = &pCF->sName;` |
|        - |  391 | `	ph7_vm_func_arg *aP, *aC;` |
|        - |  392 | `	sxu32 nPArg, nCArg, k;` |
|   399549 |  393 | `	int bBad = 0;` |
|   399544 |  394 | `	if( pMName->nByte == sizeof("__construct")-1` |
|   249708 |  395 | `	 && SyStrnmicmp(pMName->zString,"__construct",pMName->nByte) == 0 ){` |
|    89345 |  396 | `		return SXRET_OK;` |
|        - |  397 | `	}` |
|        - |  398 | `	/*` |
|        - |  399 | `	 * A NATIVE method declares its parameters in a zSig STRING, so its aArgs set` |
|        - |  400 | `	 * is empty and there is nothing here to compare against. Reading that as` |
|        - |  401 | `	 * "declares no parameters" made every override of one incompatible: a user` |
|        - |  402 | `	 * class extending DOMDocument, a Reflection class or a native enum's own` |
|        - |  403 | `	 * cases()/from() all fataled on a declaration php accepts. An` |
|        - |  404 | `	 * engine-declared signature is compatible by construction, on either side.` |
|        - |  405 | `	 */` |
|   310209 |  406 | `	if( ((pPF->iFlags \| pCF->iFlags) & VM_FUNC_NATIVE) != 0 ){` |
|   310087 |  407 | `		return SXRET_OK;` |
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
|      123 |  424 | `		int bVariadic = 0;` |
|      169 |  425 | `		for( k = 0; k < nPArg; k++ ){ if( aP[k].iFlags & VM_FUNC_ARG_VARIADIC ) bVariadic = 1; }` |
|      171 |  426 | `		for( k = 0; k < nCArg; k++ ){ if( aC[k].iFlags & VM_FUNC_ARG_VARIADIC ) bVariadic = 1; }` |
|      123 |  427 | `		if( !bVariadic ){` |
|      123 |  428 | `			if( nCArg < nPArg ){` |
|      ! 0 |  429 | `				bBad = 1; /* dropped a parent parameter */` |
|      ! 0 |  430 | `			}else{` |
|      125 |  431 | `				for( k = nPArg; k < nCArg; k++ ){` |
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
|   199777 |  446 | `}` |
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
|   415706 |  488 | `PH7_PRIVATE sxi32 PH7_ClassInherit(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase)` |
|        5 |  489 | `{` |
|        - |  490 | `	ph7_class_method *pMeth;` |
|        - |  491 | `	ph7_class_attr *pAttr;` |
|        - |  492 | `	SyHashEntry *pEntry;` |
|        - |  493 | `	SyString *pName;` |
|        - |  494 | `	SySet aInherited; /* base attributes to prepend (see the copy loop below) */` |
|        - |  495 | `	sxi32 rc;` |
|   415711 |  496 | `	SySetInit(&aInherited,&pGen->pVm->sAllocator,sizeof(ph7_class_attr *));` |
|        - |  497 | `	/* Install in the derived hashtable */` |
|   415711 |  498 | `	rc = SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|   415711 |  499 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  500 | `		SySetRelease(&aInherited);` |
|      ! 0 |  501 | `		return rc;` |
|        - |  502 | `	}` |
|        - |  503 | `	/* readonly class inheritance (PHP 8.2): a readonly class may only extend a` |
|        - |  504 | `	 * readonly class, and a non-readonly class may not extend a readonly one. */` |
|   415711 |  505 | `	if( (pBase->iFlags & PH7_CLASS_READONLY) != (pSub->iFlags & PH7_CLASS_READONLY) ){` |
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
|   415711 |  521 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
|  2608063 |  522 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|        - |  523 | `		/* Make sure the private attributes are not redeclared in the subclass */` |
|  2192357 |  524 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  2192357 |  525 | `		pName = &pAttr->sName;` |
|  2192357 |  526 | `		if( (pEntry = SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|    21042 |  527 | `			if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_FINAL))` |
|    10526 |  528 | `				== (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_FINAL) ){` |
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
|    21047 |  552 | `			if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|    21045 |  553 | `				ph7_class_attr *pOwn = (ph7_class_attr *)pEntry->pUserData;` |
|    21045 |  554 | `				SyHashDeleteEntry(&pSub->hAttr,(const void *)pName->zString,pName->nByte,0);` |
|    21045 |  555 | `				rc = SySetPut(&aInherited,(const void *)&pOwn);` |
|    21045 |  556 | `				if( rc != SXRET_OK ){` |
|      ! 0 |  557 | `					SySetRelease(&aInherited);` |
|      ! 0 |  558 | `					return rc;` |
|        - |  559 | `				}` |
|    10520 |  560 | `			}` |
|    21047 |  561 | `			continue;` |
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
|  2171310 |  574 | `		if( pAttr->iProtection != PH7_CLASS_PROT_PRIVATE` |
|  1606130 |  575 | `		 \|\| (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|  2171311 |  576 | `			rc = SySetPut(&aInherited,(const void *)&pAttr);` |
|  2171311 |  577 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  578 | `				SySetRelease(&aInherited);` |
|      ! 0 |  579 | `				return rc;` |
|        - |  580 | `			}` |
|  1085653 |  581 | `		}` |
|        5 |  582 | `	}` |
|        - |  583 | `	/* Prepend the collected base attributes so hAttr reads base-first. The` |
|        - |  584 | `	 * table's iteration list is what every object-iteration consumer walks` |
|        - |  585 | `	 * (var_dump/print_r, get_object_vars, foreach, (array) casts, json_encode),` |
|        - |  586 | `	 * so this ordering is user-visible — json_encode emits its keys in exactly` |
|        - |  587 | `	 * this order. SyHashInsert is a HEAD insert, so walking the collected set` |
|        - |  588 | `	 * backwards leaves the base's own declaration order at the front. */` |
|   415711 |  589 | `	if( SySetUsed(&aInherited) > 0 ){` |
|   415445 |  590 | `		ph7_class_attr **apInherited = (ph7_class_attr **)SySetBasePtr(&aInherited);` |
|   415445 |  591 | `		sxu32 n = SySetUsed(&aInherited);` |
|  2607791 |  592 | `		while( n > 0 ){` |
|  2192351 |  593 | `			ph7_class_attr *pIn = apInherited[--n];` |
|  2192351 |  594 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pIn->sName.zString,pIn->sName.nByte,pIn);` |
|  2192351 |  595 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  596 | `				SySetRelease(&aInherited);` |
|      ! 0 |  597 | `				return rc;` |
|        - |  598 | `			}` |
|        5 |  599 | `		}` |
|   207720 |  600 | `	}` |
|        - |  601 | `	/* Inherit the base's CONSTANTS (separate namespace: hConst). Constants are not` |
|        - |  602 | `	 * part of instance iteration, so no base-first ordering dance is needed — a plain` |
|        - |  603 | `	 * copy of every constant the subclass did not itself redeclare. A subclass that` |
|        - |  604 | `	 * redeclares a base FINAL constant is a fatal (PHP 8.1). */` |
|   415711 |  605 | `	SyHashResetLoopCursor(&pBase->hConst);` |
|  1435805 |  606 | `	while((pEntry = SyHashGetNextEntry(&pBase->hConst)) != 0 ){` |
|        - |  607 | `		SyHashEntry *pOwn;` |
|  1020099 |  608 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  1020099 |  609 | `		pName = &pAttr->sName;` |
|  1020099 |  610 | `		if( (pOwn = SyHashGet(&pSub->hConst,(const void *)pName->zString,pName->nByte)) != 0 ){` |
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
|  1020095 |  625 | `		rc = SyHashInsertTail(&pSub->hConst,(const void *)pName->zString,pName->nByte,pAttr);` |
|  1020095 |  626 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  627 | `			SySetRelease(&aInherited);` |
|      ! 0 |  628 | `			return rc;` |
|        - |  629 | `		}` |
|        5 |  630 | `	}` |
|   415711 |  631 | `	SySetRelease(&aInherited);` |
|   415711 |  632 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
|  7409357 |  633 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
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
|  6993651 |  646 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  6993651 |  647 | `		SyStringInitFromBuf(&sKey,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|  6993651 |  648 | `		pName = &sKey;` |
|  6993651 |  649 | `		if( (pOwn = SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|   399563 |  650 | `			if( pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|        - |  651 | `				/* php: a base's PRIVATE method is never overridden — the child's` |
|        - |  652 | `				 * declaration is an independent member of the same name, so neither` |
|        - |  653 | `				 * the final rule nor the signature-compatibility rule applies to it` |
|        - |  654 | `				 * (zend's do_inherit_method skips both for a private parent). PHL` |
|        - |  655 | ``				 * ran both: `class A { private function m($a){} } class B extends A`` |
|        - |  656 | ``				 * { private function m($a,$b,$c){} }` was a fatal php compiles, and`` |
|        - |  657 | ``				 * `final private` in the base fataled every child that reused the`` |
|        - |  658 | `				 * name — php only WARNS at the final-private DECLARATION and lets` |
|        - |  659 | `				 * the child have the name. */` |
|   399558 |  660 | `			}else if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){` |
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
|   599321 |  671 | `				rc = OoCheckOverrideCompat(&(*pGen),pBase,pSub,pMeth,` |
|   399544 |  672 | `					(ph7_class_method *)pOwn->pUserData);` |
|   399549 |  673 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  674 | `					return SXERR_ABORT;` |
|        - |  675 | `				}` |
|        - |  676 | `			}` |
|   399563 |  677 | `			continue;` |
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
|  6594093 |  695 | `		rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|  6594093 |  696 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  697 | `			return rc;` |
|        - |  698 | `		}` |
|        5 |  699 | `	}` |
|        - |  700 | `	/* Mark as subclass */` |
|   415711 |  701 | `	pSub->pBase = pBase;` |
|        - |  702 | `	/* All done */` |
|   415711 |  703 | `	return SXRET_OK;` |
|   207858 |  704 | `}` |
|        - |  705 | `/*` |
|        - |  706 | ` * Apply a trait to a class: copy all methods and attributes from the trait` |
|        - |  707 | ` * into the target class. Unlike inheritance, traits copy ALL members including` |
|        - |  708 | ` * private ones. Members already defined in the class take precedence.` |
|        - |  709 | ` */` |
|      154 |  710 | `PH7_PRIVATE sxi32 PH7_ClassUseTrait(ph7_gen_state *pGen,ph7_class *pClass,ph7_class *pTrait)` |
|        5 |  711 | `{` |
|        - |  712 | `	ph7_class_method *pMeth;` |
|        - |  713 | `	ph7_class_attr *pAttr;` |
|        - |  714 | `	SyHashEntry *pEntry;` |
|        - |  715 | `	SyString *pName;` |
|        - |  716 | `	sxi32 rc;` |
|        - |  717 | `	/* Detect cyclic trait composition (e.g. trait A { use B; } trait B { use A; }) */` |
|      159 |  718 | `	if( pTrait->iFlags & PH7_CLASS_TRAIT_VISITING ){` |
|      ! 0 |  719 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|      ! 0 |  720 | `			"Trait circular reference detected: %z is already being applied",&pTrait->sName);` |
|      ! 0 |  721 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  722 | `			return SXERR_ABORT;` |
|        - |  723 | `		}` |
|      ! 0 |  724 | `		return SXRET_OK;` |
|        - |  725 | `	}` |
|      159 |  726 | `	pTrait->iFlags \|= PH7_CLASS_TRAIT_VISITING;` |
|      159 |  727 | `	rc = SXRET_OK;` |
|        - |  728 | `	/* Copy attributes from the trait */` |
|      159 |  729 | `	SyHashResetLoopCursor(&pTrait->hAttr);` |
|      189 |  730 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hAttr)) != 0 ){` |
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
|      159 |  773 | `	SyHashResetLoopCursor(&pTrait->hConst);` |
|      159 |  774 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hConst)) != 0 ){` |
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
|      159 |  790 | `	SyHashResetLoopCursor(&pTrait->hMethod);` |
|      415 |  791 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hMethod)) != 0 ){` |
|        - |  792 | `		SyHashEntry *pClassMethEntry;` |
|        - |  793 | `		SyString sKey;` |
|      261 |  794 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      261 |  795 | `		SyStringInitFromBuf(&sKey,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|      261 |  796 | `		pName = &sKey;` |
|      261 |  797 | `		pClassMethEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
|      261 |  798 | `		if( pClassMethEntry != 0 ){` |
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
|      247 |  842 | `		rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|      247 |  843 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  844 | `			goto cleanup;` |
|        - |  845 | `		}` |
|        5 |  846 | `	}` |
|        - |  847 | `	/* Record trait in the class */` |
|      159 |  848 | `	SySetPut(&pClass->aTrait,(const void *)&pTrait);` |
|       77 |  849 | `cleanup:` |
|        - |  850 | `	/* Always clear visiting flag, even on error paths */` |
|      159 |  851 | `	pTrait->iFlags &= ~PH7_CLASS_TRAIT_VISITING;` |
|       77 |  852 | `	SXUNUSED(pGen);` |
|      159 |  853 | `	return rc;` |
|       82 |  854 | `}` |
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
|    42052 |  868 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceInherit(ph7_class *pSub,ph7_class *pBase)` |
|        5 |  869 | `{` |
|        - |  870 | `	ph7_class_method *pMeth;` |
|        - |  871 | `	ph7_class_attr *pAttr;` |
|        - |  872 | `	SyHashEntry *pEntry;` |
|        - |  873 | `	SyString *pName;` |
|        - |  874 | `	sxi32 rc;` |
|        - |  875 | `	/* Install in the derived hashtable */` |
|    42057 |  876 | `	SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|    42057 |  877 | `	SyHashResetLoopCursor(&pBase->hConst);` |
|        - |  878 | `	/* Copy constants (interfaces carry only constants + method signatures) */` |
|    63085 |  879 | `	while((pEntry = SyHashGetNextEntry(&pBase->hConst)) != 0 ){` |
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
|    42057 |  891 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
|        - |  892 | `	/* Copy methods signature */` |
|   157711 |  893 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|        - |  894 | `		/* Make sure the method are not redeclared in the subclass */` |
|    94633 |  895 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|    94633 |  896 | `		pName = &pMeth->sFunc.sName;` |
|    94633 |  897 | `		if( SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte) == 0 ){` |
|        - |  898 | `			/* Install the method */` |
|    94633 |  899 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|    94633 |  900 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  901 | `				return rc;` |
|        - |  902 | `			}` |
|    47314 |  903 | `		}` |
|        5 |  904 | `	}` |
|        - |  905 | `	/* Mark as subclass */` |
|    42057 |  906 | `	pSub->pBase = pBase;` |
|        - |  907 | `	/* All done */` |
|    42057 |  908 | `	return SXRET_OK;` |
|    21031 |  909 | `}` |
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
|   342216 |  923 | `PH7_PRIVATE sxi32 PH7_ClassImplement(ph7_class *pMain,ph7_class *pInterface)` |
|        5 |  924 | `{` |
|        - |  925 | `	ph7_class_attr *pAttr;` |
|        - |  926 | `	SyHashEntry *pEntry;` |
|        - |  927 | `	SyString *pName;` |
|        - |  928 | `	sxi32 rc;` |
|        - |  929 | `	/* First off,copy all constants declared inside the interface (hConst namespace) */` |
|   342221 |  930 | `	SyHashResetLoopCursor(&pInterface->hConst);` |
|   660459 |  931 | `	while((pEntry = SyHashGetNextEntry(&pInterface->hConst)) != 0 ){` |
|        - |  932 | `		/* Point to the constant declaration */` |
|   147135 |  933 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   147135 |  934 | `		pName = &pAttr->sName;` |
|        - |  935 | `		/* Make sure the constant is not redeclared in the main class */` |
|   147135 |  936 | `		if( SyHashGet(&pMain->hConst,pName->zString,pName->nByte) == 0 ){` |
|        - |  937 | `			/* Install the constant */` |
|   147135 |  938 | `			rc = SyHashInsertTail(&pMain->hConst,pName->zString,pName->nByte,pAttr);` |
|   147135 |  939 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  940 | `				return rc;` |
|        - |  941 | `			}` |
|    73565 |  942 | `		}` |
|        5 |  943 | `	}` |
|        - |  944 | `	/* Install in the interface container */` |
|   342221 |  945 | `	SySetPut(&pMain->aInterface,(const void *)&pInterface);` |
|        - |  946 | `	/* Install interface method stubs into the implementing class.` |
|        - |  947 | `	 * Methods already defined in the class take precedence (they satisfy` |
|        - |  948 | `	 * the interface contract). Stubs retain PH7_CLASS_ATTR_ABSTRACT so` |
|        - |  949 | `	 * the unified check in GenStateCheckAbstractMethods catches missing ones.` |
|        - |  950 | `	 */` |
|        - |  951 | `	{` |
|        - |  952 | `		ph7_class_method *pMeth;` |
|        - |  953 | `		SyHashEntry *pMEntry;` |
|        - |  954 | `		SyString *pMName;` |
|   342221 |  955 | `		SyHashResetLoopCursor(&pInterface->hMethod);` |
|  1475963 |  956 | `		while((pMEntry = SyHashGetNextEntry(&pInterface->hMethod)) != 0 ){` |
|   962639 |  957 | `			pMeth = (ph7_class_method *)pMEntry->pUserData;` |
|   962639 |  958 | `			pMName = &pMeth->sFunc.sName;` |
|   962639 |  959 | `			if( SyHashGet(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte) == 0 ){` |
|     5279 |  960 | `				rc = SyHashInsert(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte,pMeth);` |
|     5279 |  961 | `				if( rc != SXRET_OK ){` |
|      ! 0 |  962 | `					return rc;` |
|        - |  963 | `				}` |
|     2637 |  964 | `			}` |
|        5 |  965 | `		}` |
|        - |  966 | `	}` |
|   342221 |  967 | `	return SXRET_OK;` |
|   171113 |  968 | `}` |
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
|  1587831 | 1048 | `static ph7_class_instance * NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|        5 | 1049 | `{` |
|        - | 1050 | `	ph7_class_instance *pThis;` |
|        - | 1051 | `	/* Allocate a new instance */` |
|  1587836 | 1052 | `	pThis = (ph7_class_instance *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_instance));` |
|  1587836 | 1053 | `	if( pThis == 0 ){` |
|      ! 0 | 1054 | `		return 0;` |
|        - | 1055 | `	}` |
|        - | 1056 | `	/* Zero the structure */` |
|  1587836 | 1057 | `	SyZero(pThis,sizeof(ph7_class_instance));` |
|        - | 1058 | `	/* Initialize fields */` |
|  1587836 | 1059 | `	pThis->iRef = 1;` |
|  1587836 | 1060 | `	pThis->pVm = pVm;` |
|  1587836 | 1061 | `	pThis->pClass = pClass;` |
|        - | 1062 | `	/* Assign a fresh monotonic object handle id (clones get their own, like PHP). */` |
|  1587836 | 1063 | `	pThis->nObjId = pVm->nNextObjId++;` |
|  1587836 | 1064 | `	SyHashInit(&pThis->hAttr,&pVm->sAllocator,0,0);` |
|  1587836 | 1065 | `	return pThis;` |
|   793920 | 1066 | `}` |
|        - | 1067 | `/*` |
|        - | 1068 | ` * Wrapper around the NewClassInstance() function defined above.` |
|        - | 1069 | ` * See the block comment above for more information.` |
|        - | 1070 | ` */` |
|  1587215 | 1071 | `PH7_PRIVATE ph7_class_instance * PH7_NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|        5 | 1072 | `{` |
|        - | 1073 | `	ph7_class_instance *pNew;` |
|        - | 1074 | `	sxi32 rc;` |
|  1587220 | 1075 | `	pNew = NewClassInstance(&(*pVm),&(*pClass));` |
|  1587220 | 1076 | `	if( pNew == 0 ){` |
|      ! 0 | 1077 | `		return 0;` |
|        - | 1078 | `	}` |
|        - | 1079 | `	/* Associate a private VM frame with this class instance */` |
|  1587220 | 1080 | `	rc = PH7_VmCreateClassInstanceFrame(&(*pVm),pNew);` |
|  1587220 | 1081 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1082 | `		SyMemBackendPoolFree(&pVm->sAllocator,pNew);` |
|      ! 0 | 1083 | `		return 0;` |
|        - | 1084 | `	}` |
|        - | 1085 | `	/* php stamps a Throwable's file/line at CREATION, not in its constructor, so a` |
|        - | 1086 | `	 * subclass that overrides __construct without calling parent::__construct still` |
|        - | 1087 | `	 * reports the right site. Every instantiation path lands here. */` |
|  1587220 | 1088 | `	PH7_VmStampThrowableSite(&(*pVm),pNew);` |
|  1587220 | 1089 | `	return pNew;` |
|   793612 | 1090 | `}` |
|        - | 1091 | `/*` |
|        - | 1092 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon] attribute.` |
|        - | 1093 | ` * This function never fail.` |
|        - | 1094 | ` */` |
|  7505984 | 1095 | `static ph7_value * ExtractClassAttrValue(ph7_vm *pVm,VmClassAttr *pAttr)` |
|        5 | 1096 | `{` |
|        - | 1097 | `	/* Extract the value */` |
|        - | 1098 | `	ph7_value *pValue;` |
|  7505989 | 1099 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|  7505989 | 1100 | `	return pValue;` |
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
|        - | 1186 | `/*` |
|        - | 1187 | `` * Is `clone` refused for this class? php's uncloneable internal classes refuse`` |
|        - | 1188 | `` * for their USER SUBCLASSES too -- `class M extends IteratorIterator {}` makes`` |
|        - | 1189 | `` * `clone $m` the same catchable Error, named after M -- because the refusal is`` |
|        - | 1190 | ` * the inherited clone_obj handler, not the class's own row. So the flag is` |
|        - | 1191 | ` * consulted up the base chain, not on the instance's class alone. (A subclass` |
|        - | 1192 | ` * declaring its own __clone() changes nothing there either: php never reaches` |
|        - | 1193 | ` * it, and neither does this engine -- the refusal answers first.)` |
|        - | 1194 | ` */` |
|      348 | 1195 | `PH7_PRIVATE int PH7_ClassIsUncloneable(ph7_class *pClass)` |
|        5 | 1196 | `{` |
|        - | 1197 | `	ph7_class *pC;` |
|      661 | 1198 | `	for( pC = pClass ; pC ; pC = pC->pBase ){` |
|      419 | 1199 | `		if( pC->iFlags & PH7_CLASS_NOCLONE ){` |
|      108 | 1200 | `			return 1;` |
|        - | 1201 | `		}` |
|      159 | 1202 | `	}` |
|      247 | 1203 | `	return 0;` |
|      179 | 1204 | `}` |
|      616 | 1205 | `PH7_PRIVATE ph7_class_instance * PH7_CloneClassInstance(ph7_class_instance *pSrc)` |
|        5 | 1206 | `{` |
|        - | 1207 | `	ph7_class_instance *pClone;` |
|        - | 1208 | `	ph7_class_method *pMethod;` |
|        - | 1209 | `	SyHashEntry *pEntry2;` |
|        - | 1210 | `	SyHashEntry *pEntry;` |
|        - | 1211 | `	ph7_vm *pVm;` |
|        - | 1212 | `	sxi32 rc;` |
|        - | 1213 | `	/* Allocate a new instance */` |
|      621 | 1214 | `	pVm = pSrc->pVm;` |
|      621 | 1215 | `	pClone = NewClassInstance(pVm,pSrc->pClass);` |
|      621 | 1216 | `	if( pClone == 0 ){` |
|      ! 0 | 1217 | `		return 0;` |
|        - | 1218 | `	}` |
|        - | 1219 | `	/* Associate a private VM frame with this class instance */` |
|      621 | 1220 | `	rc = PH7_VmCreateClassInstanceFrame(pVm,pClone);` |
|      621 | 1221 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1222 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClone);` |
|      ! 0 | 1223 | `		return 0;` |
|        - | 1224 | `	}` |
|        - | 1225 | `	/* Duplicate object values. Iterate the SOURCE attributes and copy each into` |
|        - | 1226 | `	 * the clone's same-named slot (looked up by name, so order/count differences` |
|        - | 1227 | `	 * from dynamic properties don't matter). A dynamic (runtime-added) property` |
|        - | 1228 | `	 * has no declared counterpart in the clone, so synthesize it first — without` |
|        - | 1229 | `	 * this, a clone of a stdClass would silently lose all its dynamic properties. */` |
|      621 | 1230 | `	SyHashResetLoopCursor(&pSrc->hAttr);` |
|     2579 | 1231 | `	while((pEntry = SyHashGetNextEntry(&pSrc->hAttr)) != 0 ){` |
|     1963 | 1232 | `		VmClassAttr *pSrcAttr = (VmClassAttr *)pEntry->pUserData;` |
|     1963 | 1233 | `		VmClassAttr *pDestAttr = 0;` |
|     1963 | 1234 | `		ph7_value *pvSrc,*pvDest = 0;` |
|        - | 1235 | `		/* Duplicate non-static attribute */` |
|     1963 | 1236 | `		if( pSrcAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|        6 | 1237 | `			continue;` |
|        - | 1238 | `		}` |
|     1959 | 1239 | `		pEntry2 = SyHashGet(&pClone->hAttr,SyStringData(&pSrcAttr->pAttr->sName),SyStringLength(&pSrcAttr->pAttr->sName));` |
|     1959 | 1240 | `		if( pEntry2 ){` |
|     1937 | 1241 | `			pDestAttr = (VmClassAttr *)pEntry2->pUserData;` |
|     1937 | 1242 | `			pvDest = ExtractClassAttrValue(pVm,pDestAttr);` |
|      989 | 1243 | `		}else if( pSrcAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){` |
|        - | 1244 | `			/* Dynamic property: synthesize the matching slot on the clone. */` |
|       34 | 1245 | `			pvDest = PH7_VmCreateDynamicAttr(pVm,pClone,` |
|       22 | 1246 | `				SyStringData(&pSrcAttr->pAttr->sName),SyStringLength(&pSrcAttr->pAttr->sName),&pDestAttr);` |
|       11 | 1247 | `		}` |
|        - | 1248 | `		/* Fetch the source value LAST: PH7_VmCreateDynamicAttr above may have` |
|        - | 1249 | `		 * reserved a slot and reallocated pVm->aMemObj, which would dangle any` |
|        - | 1250 | `		 * ph7_value* obtained before it. pvDest from the synth path already points` |
|        - | 1251 | `		 * into the post-realloc aMemObj; resolve pvSrc now so both are current. */` |
|     1959 | 1252 | `		pvSrc = ExtractClassAttrValue(pVm,pSrcAttr);` |
|     1959 | 1253 | `		if( (pSrcAttr->iState & VM_CLASS_ATTR_REFBOUND) && pDestAttr ){` |
|        - | 1254 | `			/* php preserves references across clone: the clone shares the SAME slot` |
|        - | 1255 | `			 * as the source property (both alias the referenced variable), rather` |
|        - | 1256 | `			 * than getting an independent value copy. Drop the clone's fresh private` |
|        - | 1257 | `			 * slot and repoint at the (already-pinned) shared slot. The REFBOUND flag` |
|        - | 1258 | `			 * is carried over by the iState copy below, so the clone's release also` |
|        - | 1259 | `			 * leaves the shared slot alone. */` |
|        5 | 1260 | `			if( pDestAttr->nIdx != pSrcAttr->nIdx ){` |
|        5 | 1261 | `				PH7_VmStoreFilterDrop(pVm,pDestAttr->pAttr,pDestAttr->nIdx);` |
|        5 | 1262 | `				PH7_VmUnsetMemObj(pVm,pDestAttr->nIdx,TRUE);` |
|        5 | 1263 | `				pDestAttr->nIdx = pSrcAttr->nIdx;` |
|        - | 1264 | `				/* The clone is a holder of the shared slot in its own right — take a pin` |
|        - | 1265 | `				 * for it, since its own release will give one back. */` |
|        5 | 1266 | `				VmPinMemObjSlotCounted(pVm,pDestAttr->nIdx);` |
|        3 | 1267 | `			}` |
|     1957 | 1268 | `		}else if( pvSrc && pvDest ){` |
|     1955 | 1269 | `			PH7_MemObjStore(pvSrc,pvDest);` |
|      975 | 1270 | `		}` |
|        - | 1271 | `		/* Carry over the per-instance state so the clone matches the source:` |
|        - | 1272 | `		 * VM_CLASS_ATTR_UNINIT marks a typed property as not-yet-initialized` |
|        - | 1273 | `		 * and doubles as the readonly write-once latch — without this a clone` |
|        - | 1274 | `		 * would reset to uninitialized (losing the value's readiness) and a` |
|        - | 1275 | `		 * readonly property would become writable again. */` |
|     1959 | 1276 | `		if( pDestAttr ){` |
|     1959 | 1277 | `			pDestAttr->iState = pSrcAttr->iState;` |
|      977 | 1278 | `		}` |
|        5 | 1279 | `	}` |
|        - | 1280 | `	/* A declared property unset() on the source is absent from the clone too (PHP). But the clone` |
|        - | 1281 | `	 * frame above materialized ALL declared attrs (with their defaults), so drop any clone attr whose` |
|        - | 1282 | `	 * name is not present on the source. Collect first, then delete — removing an entry mid-walk would` |
|        - | 1283 | `	 * free the node the SyHash loop cursor points at. */` |
|        - | 1284 | `	{` |
|        - | 1285 | `		SySet sDrop;` |
|      621 | 1286 | `		SySetInit(&sDrop,&pVm->sAllocator,sizeof(VmClassAttr *));` |
|      621 | 1287 | `		SyHashResetLoopCursor(&pClone->hAttr);` |
|     2581 | 1288 | `		while((pEntry = SyHashGetNextEntry(&pClone->hAttr)) != 0 ){` |
|     1965 | 1289 | `			VmClassAttr *pCloneAttr = (VmClassAttr *)pEntry->pUserData;` |
|     1965 | 1290 | `			if( pCloneAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|        6 | 1291 | `				continue;` |
|        - | 1292 | `			}` |
|     2934 | 1293 | `			if( SyHashGet(&pSrc->hAttr,SyStringData(&pCloneAttr->pAttr->sName),` |
|     2939 | 1294 | `					SyStringLength(&pCloneAttr->pAttr->sName)) == 0 ){` |
|        3 | 1295 | `				SySetPut(&sDrop,(const void *)&pCloneAttr);` |
|        1 | 1296 | `			}` |
|        5 | 1297 | `		}` |
|      621 | 1298 | `		if( SySetUsed(&sDrop) > 0 ){` |
|        3 | 1299 | `			VmClassAttr **apDrop = (VmClassAttr **)SySetBasePtr(&sDrop);` |
|        - | 1300 | `			sxu32 i;` |
|        5 | 1301 | `			for( i = 0 ; i < SySetUsed(&sDrop) ; ++i ){` |
|        3 | 1302 | `				VmClassAttr *pVmAttr = apDrop[i];` |
|        4 | 1303 | `				SyHashDeleteEntry(&pClone->hAttr,SyStringData(&pVmAttr->pAttr->sName),` |
|        2 | 1304 | `					SyStringLength(&pVmAttr->pAttr->sName),0);` |
|        3 | 1305 | `				PH7_VmReleaseInstanceAttr(pVm,pVmAttr);` |
|        2 | 1306 | `			}` |
|        1 | 1307 | `		}` |
|      621 | 1308 | `		SySetRelease(&sDrop);` |
|        - | 1309 | `	}` |
|        - | 1310 | `	/* The native clone hook (php's clone_obj handler): what the copy MEANS for a` |
|        - | 1311 | `	 * class whose instances stand for engine-side state -- a DOM wrapper's copy` |
|        - | 1312 | `	 * is a copy of the node. The nearest ancestor's hook serves a user subclass,` |
|        - | 1313 | `	 * which is php's handler inheritance. Runs before any __clone(), as php's` |
|        - | 1314 | `	 * handler does. */` |
|        - | 1315 | `	{` |
|        - | 1316 | `		ph7_class *pHook;` |
|     1221 | 1317 | `		for( pHook = pClone->pClass ; pHook ; pHook = pHook->pBase ){` |
|      639 | 1318 | `			if( pHook->xClone ){` |
|       35 | 1319 | `				pHook->xClone(pVm,pClone,pSrc);` |
|       35 | 1320 | `				break;` |
|        - | 1321 | `			}` |
|      305 | 1322 | `		}` |
|        - | 1323 | `	}` |
|        - | 1324 | `	/* call the __clone method on the cloned object if available */` |
|      621 | 1325 | `	pMethod = PH7_ClassExtractMethod(pClone->pClass,"__clone",sizeof("__clone")-1);` |
|      621 | 1326 | `	if( pMethod ){` |
|      101 | 1327 | `		if( pMethod->iCloneDepth < 16 ){` |
|       99 | 1328 | `			pMethod->iCloneDepth++;` |
|        - | 1329 | `			/* PHP 8.3: __clone() may re-initialize the clone's readonly` |
|        - | 1330 | `			 * properties. Flag the instance so the readonly store guard allows` |
|        - | 1331 | `			 * it for the duration of the call. */` |
|       99 | 1332 | `			pClone->iFlags \|= VM_INSTANCE_CLONING;` |
|       99 | 1333 | `			PH7_VmCallClassMethod(pVm,pClone,pMethod,0,0,0);` |
|       99 | 1334 | `			pClone->iFlags &= ~VM_INSTANCE_CLONING;` |
|       51 | 1335 | `		}else{` |
|        - | 1336 | `			/* Nesting limit reached */` |
|        3 | 1337 | `			PH7_VmThrowError(pVm,0,PH7_CTX_ERR,"Object clone limit reached,no more call to __clone()");` |
|        - | 1338 | `		}` |
|        - | 1339 | `		/* Reset the cursor */` |
|      101 | 1340 | `		pMethod->iCloneDepth = 0;` |
|       49 | 1341 | `	}` |
|        - | 1342 | `	/* Return the cloned object */` |
|      621 | 1343 | `	return pClone;` |
|      313 | 1344 | `}` |
|        - | 1345 | `#define CLASS_INSTANCE_DESTROYED 0x001 /* Instance is released */` |
|        - | 1346 | `/*` |
|        - | 1347 | ` * Free the per-instance allocations owned by ONE object attribute: its value slot (+ the typed-slot` |
|        - | 1348 | ` * enforcement entry), the synthesized ph7_class_attr for a dynamic (runtime-added) property, and the` |
|        - | 1349 | ` * VmClassAttr wrapper itself. Does NOT touch the hAttr entry node — the caller removes it` |
|        - | 1350 | `` * (`unset($o->p)` via SyHashDeleteEntry2; instance teardown via the wholesale SyHashRelease, so it must`` |
|        - | 1351 | ` * not delete entries mid-walk). Shared by PH7_ClassInstanceRelease and the OP_MEMBER unset path.` |
|        - | 1352 | ` */` |
|  9531501 | 1353 | `PH7_PRIVATE void PH7_VmReleaseInstanceAttr(ph7_vm *pVm, VmClassAttr *pVmAttr)` |
|        5 | 1354 | `{` |
|  9531506 | 1355 | `	if( pVmAttr->iState & VM_CLASS_ATTR_REFBOUND ){` |
|        - | 1356 | ``		/* Reference-bound property (`$o->p =& $x`): its value slot is SHARED, so it must`` |
|        - | 1357 | `		 * not be released here — but the property WAS one of its holders, so give the pin` |
|        - | 1358 | `		 * back. The slot (and its value, which used to stay alive for the rest of the` |
|        - | 1359 | `		 * script) goes if the property was the last thing holding it. */` |
|       27 | 1360 | `		VmUnpinMemObjSlot(pVm,pVmAttr->nIdx);` |
|  9531493 | 1361 | `	}else if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - | 1362 | `		/* Drop any typed-property enforcement slot registered for this memobj, before the memobj` |
|        - | 1363 | `		 * is returned to the free list, so a future recycled slot does not inherit the stale entry. */` |
|  9531384 | 1364 | `		PH7_VmStoreFilterDrop(pVm,pVmAttr->pAttr,pVmAttr->nIdx);` |
|  9531384 | 1365 | `		PH7_VmUnsetMemObj(pVm,pVmAttr->nIdx,TRUE);` |
|  4765686 | 1366 | `	}` |
|        - | 1367 | `	/* A dynamic property owns its synthesized ph7_class_attr (struct + inline name in one block) —` |
|        - | 1368 | `	 * free it here (the only place a per-instance pAttr is freed; declared attrs are class-owned). */` |
|  9531506 | 1369 | `	if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){` |
|      322 | 1370 | `		SyMemBackendFree(&pVm->sAllocator,pVmAttr->pAttr);` |
|      159 | 1371 | `	}` |
|  9531506 | 1372 | `	SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|  9531506 | 1373 | `}` |
|        - | 1374 | `/*` |
|        - | 1375 | ` * Release a class instance [i.e: Object in the PHP jargon] and invoke any defined destructor.` |
|        - | 1376 | ` * This routine is invoked as soon as there are no other references to a particular` |
|        - | 1377 | ` * class instance.` |
|        - | 1378 | ` */` |
|  1469765 | 1379 | `static void PH7_ClassInstanceRelease(ph7_class_instance *pThis)` |
|        5 | 1380 | `{` |
|        - | 1381 | `	ph7_class_method *pDestr;` |
|        - | 1382 | `	SyHashEntry *pEntry;` |
|        - | 1383 | `	ph7_class *pClass;` |
|        - | 1384 | `	ph7_vm *pVm;` |
|  1469770 | 1385 | `	if( pThis->iFlags & CLASS_INSTANCE_DESTROYED ){` |
|        - | 1386 | `		/*` |
|        - | 1387 | `		 * Already destroyed,return immediately.` |
|        - | 1388 | `		 * This could happend if someone perform unset($this) in the destructor body.` |
|        - | 1389 | `		 */` |
|      ! 0 | 1390 | `		return;` |
|        - | 1391 | `	}` |
|        - | 1392 | `	/* Mark as destroyed */` |
|  1469770 | 1393 | `	pThis->iFlags \|= CLASS_INSTANCE_DESTROYED;` |
|        - | 1394 | `	/* Invoke any defined destructor if available */` |
|  1469770 | 1395 | `	pVm = pThis->pVm;` |
|  1469770 | 1396 | `	pClass = pThis->pClass;` |
|  1469770 | 1397 | `	pDestr = PH7_ClassExtractMethod(pClass,"__destruct",sizeof("__destruct")-1);` |
|  1469770 | 1398 | `	if( pDestr && !pVm->bInReset ){` |
|        - | 1399 | `		/* Invoke the destructor. Skipped during ph7_vm_reset() bulk teardown:` |
|        - | 1400 | `		 * running user PHP against a half-reset VM is unsafe (see bInReset). */` |
|      561 | 1401 | `		pThis->iRef = 2; /* Prevent garbage collection */` |
|      561 | 1402 | `		PH7_VmCallClassMethod(pVm,pThis,pDestr,0,0,0);` |
|      278 | 1403 | `	}` |
|        - | 1404 | `	/* A native class's own teardown, while its slots are still readable. Not a` |
|        - | 1405 | `	 * __destruct: the classes that need this (WeakReference) declare none in php,` |
|        - | 1406 | `	 * and Reflection must not grow one.` |
|        - | 1407 | `	 *` |
|        - | 1408 | `	 * Resolved through the ANCESTORS, like php's own free_obj handler: a` |
|        - | 1409 | `	 * subclass inherits it unless it declares one of its own. Reading it off` |
|        - | 1410 | `	 * this class alone left every subclass of a handle-owning native class` |
|        - | 1411 | ``	 * without teardown -- `Pdo\Sqlite` (which is how PDO::connect() answers) and`` |
|        - | 1412 | ``	 * any userland `extends PDO` alike, both of which then died holding engine`` |
|        - | 1413 | `	 * state that believed it was still reachable. */` |
|        - | 1414 | `	{` |
|  1469770 | 1415 | `		ph7_class *pOwner = pClass;` |
|  3053502 | 1416 | `		while( pOwner && pOwner->xRelease == 0 ){` |
|  1583737 | 1417 | `			pOwner = pOwner->pBase;` |
|        5 | 1418 | `		}` |
|  1469770 | 1419 | `		if( pOwner && pOwner->xRelease ){` |
|      704 | 1420 | `			pOwner->xRelease(pVm,pThis);` |
|      350 | 1421 | `		}` |
|        - | 1422 | `	}` |
|        - | 1423 | `	/* Weak-reference registry: kill the cell for this instance so every` |
|        - | 1424 | `	 * WeakReference/WeakMap handle observes the death (the cell outlives the` |
|        - | 1425 | `	 * instance until its own handles drop; removing the hash entry here keeps` |
|        - | 1426 | `	 * a pool-reused address from resurrecting a dead cell). */` |
|  1469770 | 1427 | `	if( SyHashTotalEntry(&pVm->hWeakCell) > 0 ){` |
|     6630 | 1428 | `		void *pCellData = 0;` |
|     6628 | 1429 | `		if( SyHashDeleteEntry(&pVm->hWeakCell,(const void *)&pThis,sizeof(void *),&pCellData) == SXRET_OK` |
|     3330 | 1430 | `		 && pCellData ){` |
|       30 | 1431 | `			((VmWeakCell *)pCellData)->pObj = 0;` |
|       14 | 1432 | `		}` |
|     3314 | 1433 | `	}` |
|        - | 1434 | `	/* Release non-static attributes (the wholesale SyHashRelease below frees the entry nodes,` |
|        - | 1435 | `	 * so the helper must not delete them mid-walk). */` |
|  1469770 | 1436 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
| 11001231 | 1437 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|  9531466 | 1438 | `		PH7_VmReleaseInstanceAttr(pVm,(VmClassAttr *)pEntry->pUserData);` |
|        5 | 1439 | `	}` |
|        - | 1440 | `	/* Release the whole structure */` |
|  1469770 | 1441 | `	SyHashRelease(&pThis->hAttr);` |
|  1469770 | 1442 | `	SyMemBackendPoolFree(&pVm->sAllocator,pThis);` |
|   734887 | 1443 | `}` |
|        - | 1444 | `/*` |
|        - | 1445 | ` * Decrement the reference count of a class instance [i.e Object in the PHP jargon].` |
|        - | 1446 | ` * If the reference count reaches zero,release the whole instance.` |
|        - | 1447 | ` */` |
|  7485982 | 1448 | `PH7_PRIVATE void PH7_ClassInstanceUnref(ph7_class_instance *pThis)` |
|        5 | 1449 | `{` |
|  7485987 | 1450 | `	pThis->iRef--;` |
|  7485987 | 1451 | `	if( pThis->iRef < 1 ){` |
|        - | 1452 | `		/* No more reference to this instance */` |
|  1469770 | 1453 | `		PH7_ClassInstanceRelease(&(*pThis));` |
|   734882 | 1454 | `	}` |
|  7485987 | 1455 | `}` |
|        - | 1456 | `/*` |
|        - | 1457 | ` * Compare two class instances [i.e: Objects in the PHP jargon]` |
|        - | 1458 | ` * Note on objects comparison:` |
|        - | 1459 | ` *  According to the PHP langauge reference manual` |
|        - | 1460 | ` *  When using the comparison operator (==), object variables are compared in a simple manner` |
|        - | 1461 | ` *  namely: Two object instances are equal if they have the same attributes and values, and are` |
|        - | 1462 | ` *  instances of the same class.` |
|        - | 1463 | ` *  On the other hand, when using the identity operator (===), object variables are identical` |
|        - | 1464 | ` *  if and only if they refer to the same instance of the same class.` |
|        - | 1465 | ` *  An example will clarify these rules.` |
|        - | 1466 | ` *  Example #1 Example of object comparison` |
|        - | 1467 | ` *  <?php` |
|        - | 1468 | ` *    function bool2str($bool)` |
|        - | 1469 | ` * {` |
|        - | 1470 | ` *   if ($bool === false) {` |
|        - | 1471 | ` *       return 'FALSE';` |
|        - | 1472 | ` *   } else {` |
|        - | 1473 | ` *       return 'TRUE';` |
|        - | 1474 | ` *   }` |
|        - | 1475 | ` * }` |
|        - | 1476 | ` * function compareObjects(&$o1, &$o2)` |
|        - | 1477 | ` * {` |
|        - | 1478 | ` *   echo 'o1 == o2 : ' . bool2str($o1 == $o2) . "\n";` |
|        - | 1479 | ` *   echo 'o1 != o2 : ' . bool2str($o1 != $o2) . "\n";` |
|        - | 1480 | ` *   echo 'o1 === o2 : ' . bool2str($o1 === $o2) . "\n";` |
|        - | 1481 | ` *   echo 'o1 !== o2 : ' . bool2str($o1 !== $o2) . "\n";` |
|        - | 1482 | ` * }` |
|        - | 1483 | ` * class Flag` |
|        - | 1484 | ` * {` |
|        - | 1485 | ` *   public $flag;` |
|        - | 1486 | ` *` |
|        - | 1487 | ` *   function Flag($flag = true) {` |
|        - | 1488 | ` *       $this->flag = $flag;` |
|        - | 1489 | ` *   }` |
|        - | 1490 | ` * }` |
|        - | 1491 | ` *` |
|        - | 1492 | ` * class OtherFlag` |
|        - | 1493 | ` * {` |
|        - | 1494 | ` *   public $flag;` |
|        - | 1495 | ` *` |
|        - | 1496 | ` *   function OtherFlag($flag = true) {` |
|        - | 1497 | ` *       $this->flag = $flag;` |
|        - | 1498 | ` *   }` |
|        - | 1499 | ` * }` |
|        - | 1500 | ` *` |
|        - | 1501 | ` * $o = new Flag();` |
|        - | 1502 | ` * $p = new Flag();` |
|        - | 1503 | ` * $q = $o;` |
|        - | 1504 | ` * $r = new OtherFlag();` |
|        - | 1505 | ` *` |
|        - | 1506 | ` * echo "Two instances of the same class\n";` |
|        - | 1507 | ` * compareObjects($o, $p);` |
|        - | 1508 | ` * echo "\nTwo references to the same instance\n";` |
|        - | 1509 | ` * compareObjects($o, $q);` |
|        - | 1510 | ` * echo "\nInstances of two different classes\n";` |
|        - | 1511 | ` * compareObjects($o, $r);` |
|        - | 1512 | ` * ?>` |
|        - | 1513 | ` * The above example will output:` |
|        - | 1514 | ` * Two instances of the same class` |
|        - | 1515 | ` * o1 == o2 : TRUE` |
|        - | 1516 | ` * o1 != o2 : FALSE` |
|        - | 1517 | ` * o1 === o2 : FALSE` |
|        - | 1518 | ` * o1 !== o2 : TRUE` |
|        - | 1519 | ` * Two references to the same instance` |
|        - | 1520 | ` * o1 == o2 : TRUE` |
|        - | 1521 | ` * o1 != o2 : FALSE` |
|        - | 1522 | ` * o1 === o2 : TRUE` |
|        - | 1523 | ` * o1 !== o2 : FALSE` |
|        - | 1524 | ` * Instances of two different classes` |
|        - | 1525 | ` * o1 == o2 : FALSE` |
|        - | 1526 | ` * o1 != o2 : TRUE` |
|        - | 1527 | ` * o1 === o2 : FALSE` |
|        - | 1528 | ` * o1 !== o2 : TRUE` |
|        - | 1529 | ` *` |
|        - | 1530 | ` * This function return 0 if the objects are equals according to the comprison rules defined above.` |
|        - | 1531 | ` * Any other return values indicates difference.` |
|        - | 1532 | ` */` |
|      636 | 1533 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest)` |
|        5 | 1534 | `{` |
|        - | 1535 | `	SyHashEntry *pEntry,*pEntry2;` |
|        - | 1536 | `	ph7_value sV1,sV2;` |
|        - | 1537 | `	sxi32 rc;` |
|      641 | 1538 | `	if( iNest > 31 ){` |
|        - | 1539 | `		/* Nesting limit reached */` |
|        6 | 1540 | `		PH7_VmThrowError(pLeft->pVm,0,PH7_CTX_ERR,"Nesting limit reached: Infinite recursion?");` |
|        6 | 1541 | `		return 1;` |
|        - | 1542 | `	}` |
|        - | 1543 | `	/* Comparison is performed only if the objects are instance of the same class */` |
|      637 | 1544 | `	if( pLeft->pClass != pRight->pClass ){` |
|       10 | 1545 | `		return 1;` |
|        - | 1546 | `	}` |
|      629 | 1547 | `	if( bStrict ){` |
|        - | 1548 | `		/*` |
|        - | 1549 | `		 * According to the PHP language reference manual:` |
|        - | 1550 | `		 *  when using the identity operator (===), object variables` |
|        - | 1551 | `		 *  are identical if and only if they refer to the same instance` |
|        - | 1552 | `		 *  of the same class.` |
|        - | 1553 | `		 */` |
|      431 | 1554 | `		return !(pLeft == pRight);` |
|        - | 1555 | `	}` |
|        - | 1556 | `	/*` |
|        - | 1557 | `	 * Attribute comparison.` |
|        - | 1558 | `	 * According to the PHP reference manual:` |
|        - | 1559 | `	 *  When using the comparison operator (==), object variables are compared` |
|        - | 1560 | `	 *  in a simple manner, namely: Two object instances are equal if they have` |
|        - | 1561 | `	 *  the same attributes and values, and are instances of the same class.` |
|        - | 1562 | `	 */` |
|      203 | 1563 | `	if( pLeft == pRight ){` |
|        - | 1564 | `		/* Same instance,don't bother processing,object are equals */` |
|       13 | 1565 | `		return 0;` |
|        - | 1566 | `	}` |
|        - | 1567 | `	/* Closures compare by IDENTITY under == as well (not by attributes): two distinct` |
|        - | 1568 | `	 * Closure instances are never equal, even when they wrap the same underlying function` |
|        - | 1569 | `	 * (PHP semantics). pLeft != pRight here, so a Closure pair is unequal. Without this,` |
|        - | 1570 | `` 	 * two capture-less lambdas of the same `function(){}` share the template's `$__fn` `` |
|        - | 1571 | `	 * name and would compare equal. */` |
|      191 | 1572 | `	if( pLeft->pVm->pClosureClass && pLeft->pClass == pLeft->pVm->pClosureClass ){` |
|        5 | 1573 | `		return 1;` |
|        - | 1574 | `	}` |
|        - | 1575 | `	/* Same class but a different number of attributes ⇒ different property sets` |
|        - | 1576 | `	 * (dynamic properties can give two same-class instances different counts). */` |
|      187 | 1577 | `	if( pLeft->hAttr.nEntry != pRight->hAttr.nEntry ){` |
|        3 | 1578 | `		return 1;` |
|        - | 1579 | `	}` |
|      185 | 1580 | `	PH7_MemObjInit(pLeft->pVm,&sV1);` |
|      185 | 1581 | `	PH7_MemObjInit(pLeft->pVm,&sV2);` |
|      185 | 1582 | `	sV1.nIdx = sV2.nIdx = SXU32_HIGH;` |
|        - | 1583 | `	/* Compare each left attribute against the RIGHT attribute of the SAME NAME` |
|        - | 1584 | `	 * (not in lockstep): dynamic properties may be stored in a different order` |
|        - | 1585 | `	 * on the two instances. Counts already match, so if every left attribute has` |
|        - | 1586 | `	 * an equal-valued same-named right attribute the property sets are equal. */` |
|      185 | 1587 | `	SyHashResetLoopCursor(&pLeft->hAttr);` |
|      261 | 1588 | `	while((pEntry = SyHashGetNextEntry(&pLeft->hAttr)) != 0 ){` |
|      217 | 1589 | `		VmClassAttr *p1 = (VmClassAttr *)pEntry->pUserData;` |
|        - | 1590 | `		VmClassAttr *p2;` |
|        - | 1591 | `		ph7_value *pL,*pR;` |
|        - | 1592 | `		/* Compare only non-static attribute */` |
|      217 | 1593 | `		if( p1->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|      ! 0 | 1594 | `			continue;` |
|        - | 1595 | `		}` |
|      217 | 1596 | `		pEntry2 = SyHashGet(&pRight->hAttr,SyStringData(&p1->pAttr->sName),SyStringLength(&p1->pAttr->sName));` |
|      217 | 1597 | `		if( pEntry2 == 0 ){` |
|        - | 1598 | `			/* Left has a property the right lacks ⇒ not equal. */` |
|      ! 0 | 1599 | `			return 1;` |
|        - | 1600 | `		}` |
|      217 | 1601 | `		p2 = (VmClassAttr *)pEntry2->pUserData;` |
|      217 | 1602 | `		pL = ExtractClassAttrValue(pLeft->pVm,p1);` |
|      217 | 1603 | `		pR = ExtractClassAttrValue(pRight->pVm,p2);` |
|      217 | 1604 | `		if( pL && pR ){` |
|      217 | 1605 | `			PH7_MemObjLoad(pL,&sV1);` |
|      217 | 1606 | `			PH7_MemObjLoad(pR,&sV2);` |
|        - | 1607 | `			/* Compare the two values now */` |
|      217 | 1608 | `			rc = PH7_MemObjCmp(&sV1,&sV2,bStrict,iNest+1);` |
|      217 | 1609 | `			PH7_MemObjRelease(&sV1);` |
|      217 | 1610 | `			PH7_MemObjRelease(&sV2);` |
|      217 | 1611 | `			if( rc != 0 ){` |
|        - | 1612 | `				/* Not equals */` |
|      140 | 1613 | `				return rc;` |
|        - | 1614 | `			}` |
|       38 | 1615 | `		}` |
|        3 | 1616 | `	}` |
|        - | 1617 | `	/* Object are equals */` |
|       48 | 1618 | `	return 0;` |
|      323 | 1619 | `}` |
|        - | 1620 | `/*` |
|        - | 1621 | ` * Dump a class instance and the store the dump in the BLOB given` |
|        - | 1622 | ` * as the first argument.` |
|        - | 1623 | ` * Note that only non-static/non-constants attribute are dumped.` |
|        - | 1624 | ` * This function is typically invoked when the user issue a call` |
|        - | 1625 | ` * to [var_dump(),var_export(),print_r(),...].` |
|        - | 1626 | ` * This function SXRET_OK on success. Any other return value including` |
|        - | 1627 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|        - | 1628 | ` */` |
|        - | 1629 | `/*` |
|        - | 1630 | `` * Return the `name` property value of an enum case instance (the case name),`` |
|        - | 1631 | ` * or 0 when unavailable. Shared by the var_dump/var_export/json/serialize` |
|        - | 1632 | ` * renderers, which all print enum cases as Class::CaseName forms.` |
|        - | 1633 | ` */` |
|       20 | 1634 | `PH7_PRIVATE ph7_value * PH7_EnumCaseNameValue(ph7_class_instance *pThis)` |
|        1 | 1635 | `{` |
|        - | 1636 | `	SyHashEntry *pEntry;` |
|       21 | 1637 | `	if( (pThis->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|      ! 0 | 1638 | `		return 0;` |
|        - | 1639 | `	}` |
|       21 | 1640 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)"name",sizeof("name")-1);` |
|       21 | 1641 | `	if( pEntry == 0 ){` |
|      ! 0 | 1642 | `		return 0;` |
|        - | 1643 | `	}` |
|       21 | 1644 | `	return PH7_ClassInstanceExtractAttrValue(pThis,(VmClassAttr *)pEntry->pUserData);` |
|       11 | 1645 | `}` |
|        - | 1646 | `/*` |
|        - | 1647 | `` * Return the `value` property value (the backing value) of an enum case`` |
|        - | 1648 | ` * instance, or 0 when unavailable (pure enums have none).` |
|        - | 1649 | ` */` |
|       20 | 1650 | `PH7_PRIVATE ph7_value * PH7_EnumCaseBackingValueOf(ph7_class_instance *pThis)` |
|        1 | 1651 | `{` |
|        - | 1652 | `	SyHashEntry *pEntry;` |
|       21 | 1653 | `	if( (pThis->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|      ! 0 | 1654 | `		return 0;` |
|        - | 1655 | `	}` |
|       21 | 1656 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)"value",sizeof("value")-1);` |
|       21 | 1657 | `	if( pEntry == 0 ){` |
|        7 | 1658 | `		return 0;` |
|        - | 1659 | `	}` |
|       15 | 1660 | `	return PH7_ClassInstanceExtractAttrValue(pThis,(VmClassAttr *)pEntry->pUserData);` |
|       11 | 1661 | `}` |
|        - | 1662 | `/*` |
|        - | 1663 | ` * Emit a class-instance dump header plus its trailing newline. For var_dump` |
|        - | 1664 | ` * (ShowType) it completes the "object(" prefix the caller already emitted as` |
|        - | 1665 | ` *   ClassName)#<id> (<count>) {` |
|        - | 1666 | ` * for print_r it emits the legacy PHL  Object(ClassName) {  (count/id unused).` |
|        - | 1667 | `` * Enum cases print php's `ClassName Enum {` print_r header (var_dump never`` |
|        - | 1668 | `` * reaches here for enums — PH7_MemObjDump prints `enum(S::A)` directly).`` |
|        - | 1669 | ` */` |
|      260 | 1670 | `static void DumpClassInstanceHeader(SyBlob *pOut,ph7_class *pClass,sxu32 nObjId,int ShowType,sxu32 nCount)` |
|        5 | 1671 | `{` |
|      265 | 1672 | `	if( ShowType ){` |
|        - | 1673 | ``		/* var_dump: `object(C)#id (n) {` */`` |
|      201 | 1674 | `		SyBlobFormat(&(*pOut),"object(%z)#%u (%u) {",&pClass->sName,nObjId,nCount);` |
|      201 | 1675 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      201 | 1676 | `		return;` |
|        - | 1677 | `	}` |
|        - | 1678 | ``	/* print_r: `C Object` / `E Enum[:backing]` — the '(' line is emitted by`` |
|        - | 1679 | `	 * the body renderer at the container indent. */` |
|       68 | 1680 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|      ! 0 | 1681 | `		SyBlobFormat(&(*pOut),"%z Enum",&pClass->sName);` |
|      ! 0 | 1682 | `		if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|      ! 0 | 1683 | `			SyBlobAppend(&(*pOut),":int",sizeof(":int")-1);` |
|      ! 0 | 1684 | `		}else if( pClass->nEnumBacking == MEMOBJ_STRING ){` |
|      ! 0 | 1685 | `			SyBlobAppend(&(*pOut),":string",sizeof(":string")-1);` |
|      ! 0 | 1686 | `		}` |
|      ! 0 | 1687 | `	}else{` |
|       68 | 1688 | `		SyBlobFormat(&(*pOut),"%z Object",&pClass->sName);` |
|        - | 1689 | `	}` |
|       68 | 1690 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      135 | 1691 | `}` |
|        - | 1692 | `/*` |
|        - | 1693 | ` * The class that DECLARED pAttr: inheritance shares attr pointers down the` |
|        - | 1694 | ` * chain, so the declaring class is the most ANCESTRAL class whose hAttr still` |
|        - | 1695 | ` * maps the name to this exact pointer. php's var_dump/print_r use it for the` |
|        - | 1696 | `` * `["p":"Decl":private]` annotation.`` |
|        - | 1697 | ` */` |
|        8 | 1698 | `static ph7_class * OoAttrDeclaringClass(ph7_class *pClass,ph7_class_attr *pAttr)` |
|        2 | 1699 | `{` |
|        - | 1700 | `	/* Attrs record their declaring class at install time (inheritance/trait` |
|        - | 1701 | `	 * copies share the pointer, so the field survives the chain). */` |
|       10 | 1702 | `	return pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        2 | 1703 | `}` |
|        - | 1704 | `/*` |
|        - | 1705 | ` * php's zend_unmangle_property_name_ex: a property key carries its own` |
|        - | 1706 | ` * visibility when it is MANGLED — "\0*\0name" is protected and "\0Class\0name"` |
|        - | 1707 | ` * is private to Class, which is how the (array) cast, __debugInfo() and every` |
|        - | 1708 | ` * get_debug_info handler say what a plain array key cannot. A key that does not` |
|        - | 1709 | ` * begin with a NUL is a public name and comes back unchanged.` |
|        - | 1710 | ` *` |
|        - | 1711 | ` * Answers 0 for a key that begins with a NUL and is NOT a well-formed mangled` |
|        - | 1712 | ` * name — php's "Illegal member variable name" (nothing after the NUL, or an` |
|        - | 1713 | ` * empty class part) and "Corrupt member variable name" (no second NUL, or` |
|        - | 1714 | ` * nothing after it). php renders those raw, notice aside, and so does the caller.` |
|        - | 1715 | ` */` |
|      182 | 1716 | `PH7_PRIVATE int PH7_UnmangleAttrName(const char *zKey,sxu32 nKey,SyString *pClass,SyString *pName)` |
|        3 | 1717 | `{` |
|        - | 1718 | `	sxu32 nCls,nSrc;` |
|      185 | 1719 | `	SyStringInitFromBuf(pClass,0,0);` |
|      185 | 1720 | `	SyStringInitFromBuf(pName,zKey,nKey);` |
|      185 | 1721 | `	if( nKey < 1 \|\| zKey[0] != 0 ){` |
|      137 | 1722 | `		return 1;   /* a plain public name */` |
|        - | 1723 | `	}` |
|       49 | 1724 | `	if( nKey < 3 \|\| zKey[1] == 0 ){` |
|      ! 0 | 1725 | `		return 0;   /* php: "Illegal member variable name" */` |
|        - | 1726 | `	}` |
|       49 | 1727 | `	nCls = 0;` |
|      429 | 1728 | `	while( nCls < nKey - 2 && zKey[1+nCls] != 0 ){` |
|      381 | 1729 | `		nCls++;` |
|        1 | 1730 | `	}` |
|       49 | 1731 | `	if( nCls >= nKey - 2 ){` |
|      ! 0 | 1732 | `		return 0;   /* php: "Corrupt member variable name" */` |
|        - | 1733 | `	}` |
|        - | 1734 | `	/* An ANONYMOUS class mangles its source location in as a SECOND NUL-separated` |
|        - | 1735 | `	 * part, so the property name is what follows the LAST NUL rather than the` |
|        - | 1736 | `	 * second one (php's anonclass_src_len step). The class STRING php shows is` |
|        - | 1737 | `	 * still only the first part — it prints that one as a C string. */` |
|       49 | 1738 | `	nSrc = 0;` |
|      313 | 1739 | `	while( nCls + 2 + nSrc < nKey && zKey[nCls+2+nSrc] != 0 ){` |
|      265 | 1740 | `		nSrc++;` |
|        1 | 1741 | `	}` |
|       49 | 1742 | `	SyStringInitFromBuf(pClass,&zKey[1],nCls);` |
|       49 | 1743 | `	if( nCls + nSrc + 2 != nKey ){` |
|      ! 0 | 1744 | `		nCls += nSrc + 1;` |
|      ! 0 | 1745 | `	}` |
|       49 | 1746 | `	SyStringInitFromBuf(pName,&zKey[nCls+2],nKey - nCls - 2);` |
|       49 | 1747 | `	return 1;` |
|       94 | 1748 | `}` |
|        - | 1749 | `/*` |
|        - | 1750 | `` * Emit a property's dump key: var_dump `["x"]=>` / `["p":"C":private]=>` /`` |
|        - | 1751 | `` * `["q":protected]=>`; print_r `[x] => ` / `[p:C:private] => ` /`` |
|        - | 1752 | `` * `[q:protected] => ` (php's exact annotations).`` |
|        - | 1753 | ` */` |
|      202 | 1754 | `static void OoDumpPropKey(SyBlob *pOut,ph7_class_instance *pThis,ph7_class_attr *pAttr,int ShowType)` |
|        3 | 1755 | `{` |
|      205 | 1756 | `	const char *zQ = ShowType ? "\"" : "";` |
|      205 | 1757 | `	if( SyStringLength(&pAttr->sName) > 0 && SyStringData(&pAttr->sName)[0] == 0 ){` |
|        - | 1758 | `		/* A MANGLED name stored raw — the __PHP_Incomplete_Class carrier's` |
|        - | 1759 | `		 * private/protected payload keys. php's dump unmangles them exactly as` |
|        - | 1760 | ``		 * it does an (array) cast's: `["bp":"PB":private]` / `["pp":protected]`. */`` |
|        - | 1761 | `		SyString sUnmCls, sUnmName;` |
|        9 | 1762 | `		if( PH7_UnmangleAttrName(SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),` |
|        - | 1763 | `			&sUnmCls,&sUnmName) ){` |
|        9 | 1764 | `			SyBlobFormat(&(*pOut),"[%s%z%s",zQ,&sUnmName,zQ);` |
|        9 | 1765 | `			if( sUnmCls.nByte == 1 && sUnmCls.zString[0] == '*' ){` |
|        5 | 1766 | `				SyBlobAppend(&(*pOut),":protected",sizeof(":protected")-1);` |
|        3 | 1767 | `			}else{` |
|        5 | 1768 | `				SyBlobFormat(&(*pOut),":%s%z%s:private",zQ,&sUnmCls,zQ);` |
|        - | 1769 | `			}` |
|        9 | 1770 | `			SyBlobAppend(&(*pOut),ShowType ? "]=>" : "] => ",ShowType ? 3 : 5);` |
|        9 | 1771 | `			return;` |
|        - | 1772 | `		}` |
|      ! 0 | 1773 | `	}` |
|      197 | 1774 | `	SyBlobFormat(&(*pOut),"[%s%z%s",zQ,&pAttr->sName,zQ);` |
|      197 | 1775 | `	if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       10 | 1776 | `		ph7_class *pDecl = OoAttrDeclaringClass(pThis->pClass,pAttr);` |
|       10 | 1777 | `		SyBlobFormat(&(*pOut),":%s%z%s:private",zQ,&pDecl->sName,zQ);` |
|      193 | 1778 | `	}else if( pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
|        7 | 1779 | `		SyBlobAppend(&(*pOut),":protected",sizeof(":protected")-1);` |
|        3 | 1780 | `	}` |
|      197 | 1781 | `	SyBlobAppend(&(*pOut),ShowType ? "]=>" : "] => ",ShowType ? 3 : 5);` |
|      104 | 1782 | `}` |
|      264 | 1783 | `PH7_PRIVATE sxi32 PH7_ClassInstanceDump(SyBlob *pOut,ph7_class_instance *pThis,int ShowType,int nTab,int nDepth)` |
|        5 | 1784 | `{` |
|        - | 1785 | `	SyHashEntry *pEntry;` |
|        - | 1786 | `	ph7_value *pValue;` |
|        - | 1787 | `	sxi32 rc;` |
|        - | 1788 | `	int i;` |
|      269 | 1789 | `	if( nDepth > 31 ){` |
|        - | 1790 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|        - | 1791 | `		/* Nesting limit reached..halt immediately*/` |
|        5 | 1792 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|        5 | 1793 | `		return SXERR_LIMIT;` |
|        - | 1794 | `	}` |
|      265 | 1795 | `	rc = SXRET_OK;` |
|        - | 1796 | `	{` |
|        - | 1797 | `		/* A native class's PRESENTATION (php's get_debug_info): the shape it shows` |
|        - | 1798 | `		 * is not its storage — a DateTime shows date/timezone_type/timezone, a` |
|        - | 1799 | `		 * WeakReference shows ["object"] — and the slots underneath are hidden.` |
|        - | 1800 | `		 * Rendered exactly like a __debugInfo() array, which is what php does with` |
|        - | 1801 | `		 * it, and consulted first because php's handler wins over a userland` |
|        - | 1802 | `		 * method a native class cannot declare anyway. */` |
|        - | 1803 | `		ph7_value sPresent;` |
|      265 | 1804 | `		PH7_MemObjInit(pThis->pVm,&sPresent);` |
|      265 | 1805 | `		if( ph7_value_is_array(&sPresent) == 0 ){` |
|      265 | 1806 | `			ph7_hashmap *pPresent = PH7_NewHashmap(pThis->pVm,0,0);` |
|      265 | 1807 | `			if( pPresent ){` |
|      265 | 1808 | `				sPresent.x.pOther = pPresent;` |
|      265 | 1809 | `				MemObjSetType(&sPresent,MEMOBJ_HASHMAP);` |
|      130 | 1810 | `			}` |
|      130 | 1811 | `		}` |
|      260 | 1812 | `		if( (sPresent.iFlags & MEMOBJ_HASHMAP)` |
|      265 | 1813 | `		 && PH7_ClassInstancePresent(pThis,&sPresent,1) ){` |
|       62 | 1814 | `			ph7_hashmap *pMap = (ph7_hashmap *)sPresent.x.pOther;` |
|       62 | 1815 | `			DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,pMap->nEntry);` |
|       62 | 1816 | `			if( !ShowType ){` |
|       34 | 1817 | `				for( i = 0 ; i < nTab ; i++ ){` |
|      ! 0 | 1818 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      ! 0 | 1819 | `				}` |
|       34 | 1820 | `				SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       16 | 1821 | `			}` |
|       62 | 1822 | `			rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth,1);` |
|       62 | 1823 | `			for( i = 0 ; i < nTab ; i++ ){` |
|      ! 0 | 1824 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      ! 0 | 1825 | `			}` |
|       62 | 1826 | `			if( ShowType ){` |
|       29 | 1827 | `				SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|       15 | 1828 | `			}else{` |
|       34 | 1829 | `				SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        - | 1830 | `			}` |
|       62 | 1831 | `			PH7_MemObjRelease(&sPresent);` |
|       62 | 1832 | `			return rc;` |
|        - | 1833 | `		}` |
|      205 | 1834 | `		PH7_MemObjRelease(&sPresent);` |
|        - | 1835 | `	}` |
|        - | 1836 | `	{` |
|        - | 1837 | `		/* Both var_dump and print_r consult __debugInfo() (PHP behavior);` |
|        - | 1838 | `		 * var_export uses a separate renderer and never reaches here. When the` |
|        - | 1839 | `		 * method is present and returns an array, render that array's entries as` |
|        - | 1840 | `		 * the object body, with the header showing the debug array's count. The` |
|        - | 1841 | `		 * nDepth guard above protects against a __debugInfo returning the object` |
|        - | 1842 | `		 * itself. */` |
|      205 | 1843 | `		ph7_class_method *pDbg = PH7_ClassExtractMethod(pThis->pClass,"__debugInfo",sizeof("__debugInfo")-1);` |
|      205 | 1844 | `		if( pDbg ){` |
|        - | 1845 | `			ph7_value sResult;` |
|       19 | 1846 | `			PH7_MemObjInit(pThis->pVm,&sResult);` |
|       19 | 1847 | `			PH7_VmCallMagicMethod(pThis->pVm,pThis,pDbg,&sResult,0,0);` |
|       19 | 1848 | `			if( sResult.iFlags & MEMOBJ_HASHMAP ){` |
|       19 | 1849 | `				ph7_hashmap *pMap = (ph7_hashmap *)sResult.x.pOther;` |
|        - | 1850 | `				/* Header count is the debug array's entry count. */` |
|       19 | 1851 | `				DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,pMap->nEntry);` |
|       19 | 1852 | `				if( !ShowType ){` |
|        8 | 1853 | `					for( i = 0 ; i < nTab ; i++ ){` |
|      ! 0 | 1854 | `						SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      ! 0 | 1855 | `					}` |
|        8 | 1856 | `					SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|        3 | 1857 | `				}` |
|       19 | 1858 | `				rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth,1);` |
|       19 | 1859 | `				for( i = 0 ; i < nTab ; i++ ){` |
|      ! 0 | 1860 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      ! 0 | 1861 | `				}` |
|       19 | 1862 | `				if( ShowType ){` |
|       13 | 1863 | `					SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|        8 | 1864 | `				}else{` |
|        8 | 1865 | `					SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        - | 1866 | `				}` |
|       19 | 1867 | `				PH7_MemObjRelease(&sResult);` |
|       19 | 1868 | `				return rc;` |
|        - | 1869 | `			}` |
|        - | 1870 | `			/* Non-array return: behave as if __debugInfo were absent. */` |
|      ! 0 | 1871 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 | 1872 | `		}` |
|        - | 1873 | `	}` |
|        - | 1874 | `	{` |
|        - | 1875 | `		/* var_dump's header needs the property count up front, so pre-count the` |
|        - | 1876 | `		 * non-static/non-constant attributes (matching the dump loop below).` |
|        - | 1877 | `		 * An UNINITIALIZED typed property is still LISTED by var_dump — with php's` |
|        - | 1878 | ``		 * `uninitialized(T)` marker in place of a value — but it does not COUNT,`` |
|        - | 1879 | `` 		 * which is how php's `object(C)#1 (0) { ["a"]=> uninitialized(int) }` `` |
|        - | 1880 | `		 * reads. */` |
|      189 | 1881 | `		sxu32 nProp = 0;` |
|      189 | 1882 | `		if( ShowType ){` |
|      162 | 1883 | `			SyHashResetLoopCursor(&pThis->hAttr);` |
|      435 | 1884 | `			while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|      198 | 1885 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      194 | 1886 | `				if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_HOOK_VIRTUAL\|PH7_CLASS_ATTR_HIDDEN)) == 0` |
|      189 | 1887 | `				 && !PH7_ClassAttrUninitialized(pVmAttr) ){` |
|      161 | 1888 | `					nProp++;` |
|       79 | 1889 | `				}` |
|        4 | 1890 | `			}` |
|       79 | 1891 | `		}` |
|      189 | 1892 | `		DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,nProp);` |
|        - | 1893 | `	}` |
|      189 | 1894 | `	if( !ShowType ){` |
|        - | 1895 | `		/* print_r body opener: '(' at the container indent */` |
|      142 | 1896 | `		for( i = 0 ; i < nTab ; i++ ){` |
|      114 | 1897 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       58 | 1898 | `		}` |
|       30 | 1899 | `		SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       13 | 1900 | `	}` |
|        - | 1901 | `	/* Dump object attributes (php 8.4: VIRTUAL hooked properties have no` |
|        - | 1902 | `	 * backing store — excluded from var_dump/print_r) */` |
|      189 | 1903 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      405 | 1904 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|      252 | 1905 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      252 | 1906 | `		if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_HOOK_VIRTUAL\|PH7_CLASS_ATTR_HIDDEN)) == 0 ){` |
|      223 | 1907 | `			if( PH7_ClassAttrUninitialized(pVmAttr) ){` |
|        - | 1908 | `				/* var_dump names the property and prints php's marker in place of` |
|        - | 1909 | `				 * the value it has not got; print_r has no such marker and leaves` |
|        - | 1910 | `				 * the property out entirely. */` |
|       37 | 1911 | `				if( ShowType ){` |
|        - | 1912 | `					char zType[192];` |
|       28 | 1913 | `					const char *zText = VmHintTextResolved(pThis->pVm,&pVmAttr->pAttr->sTypeName,` |
|       18 | 1914 | `						VmHintScopeClass(pThis->pVm,pVmAttr->pAttr->pDeclClass,pVmAttr->pOwner),` |
|        9 | 1915 | `						zType,sizeof(zType));` |
|       55 | 1916 | `					for( i = 0 ; i < nTab + 2 ; i++ ){` |
|       37 | 1917 | `						SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       19 | 1918 | `					}` |
|       19 | 1919 | `					OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,TRUE);` |
|       19 | 1920 | `					SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       55 | 1921 | `					for( i = 0 ; i < nTab + 2 ; i++ ){` |
|       37 | 1922 | `						SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       19 | 1923 | `					}` |
|       19 | 1924 | `					SyBlobFormat(&(*pOut),"uninitialized(%s)\n",zText);` |
|        9 | 1925 | `				}` |
|       37 | 1926 | `				continue;` |
|        - | 1927 | `			}` |
|        - | 1928 | `			/* Dump non-static/constant attribute only */` |
|      187 | 1929 | `			pValue = ExtractClassAttrValue(pThis->pVm,pVmAttr);` |
|      187 | 1930 | `			if( pValue == 0 ){` |
|      ! 0 | 1931 | `				continue;` |
|        - | 1932 | `			}` |
|      187 | 1933 | `			if( ShowType ){` |
|        - | 1934 | ``				/* var_dump prop: `["x"(:…)]=>` at nTab+2, the value on the next`` |
|        - | 1935 | `				 * line at the same indent (php). */` |
|     4213 | 1936 | `				for( i = 0 ; i < nTab + 2 ; i++ ){` |
|     4055 | 1937 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     2029 | 1938 | `				}` |
|      161 | 1939 | `				OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,TRUE);` |
|      161 | 1940 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      161 | 1941 | `				rc = PH7_MemObjDump(&(*pOut),pValue,TRUE,nTab+2,nDepth,0);` |
|      161 | 1942 | `				if( rc == SXERR_LIMIT ){` |
|      125 | 1943 | `					break;` |
|        - | 1944 | `				}` |
|       20 | 1945 | `			}else{` |
|        - | 1946 | ``				/* print_r prop: `[x(:…)] => value` at nTab+4; container values`` |
|        - | 1947 | `				 * render their block at nTab+8 followed by php's blank line. */` |
|      197 | 1948 | `				for( i = 0 ; i < nTab + 4 ; i++ ){` |
|      171 | 1949 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       87 | 1950 | `				}` |
|       29 | 1951 | `				OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,FALSE);` |
|       26 | 1952 | `				if( (pValue->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ))` |
|       16 | 1953 | `				 && (pValue->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 | 1954 | `					rc = PH7_MemObjDump(&(*pOut),pValue,FALSE,nTab+8,nDepth,0);` |
|      ! 0 | 1955 | `					SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      ! 0 | 1956 | `					if( rc == SXERR_LIMIT ){` |
|      ! 0 | 1957 | `						break;` |
|        - | 1958 | `					}` |
|      ! 0 | 1959 | `				}else{` |
|       29 | 1960 | `					PH7_MemObjPrintRInline(&(*pOut),pValue);` |
|       29 | 1961 | `					SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        - | 1962 | `				}` |
|        - | 1963 | `			}` |
|       30 | 1964 | `		}` |
|        4 | 1965 | `	}` |
|     4049 | 1966 | `	for( i = 0 ; i < nTab ; i++ ){` |
|     3863 | 1967 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     1933 | 1968 | `	}` |
|      189 | 1969 | `	if( ShowType ){` |
|      162 | 1970 | `		SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|       83 | 1971 | `	}else{` |
|       30 | 1972 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        - | 1973 | `	}` |
|      189 | 1974 | `	return rc;` |
|      137 | 1975 | `}` |
|        - | 1976 | `/*` |
|        - | 1977 | ` * Call a magic method [i.e: __toString(),__toBool(),__Invoke()...]` |
|        - | 1978 | ` * Return SXRET_OK on successfull call. Any other return value indicates failure.` |
|        - | 1979 | ` * Notes on magic methods.` |
|        - | 1980 | ` * According to the PHP language reference manual.` |
|        - | 1981 | ` *  The function names __construct(), __destruct(), __call(), __callStatic()` |
|        - | 1982 | ` *  __get(),  __toString(), __invoke(), __clone() are magical in PHP classes.` |
|        - | 1983 | ` * You cannot have functions with these names in any of your classes unless` |
|        - | 1984 | ` * you want the magic functionality associated with them.` |
|        - | 1985 | ` * Example of magical methods:` |
|        - | 1986 | ` * __toString()` |
|        - | 1987 | ` *  The __toString() method allows a class to decide how it will react when it is treated like` |
|        - | 1988 | ` *  a string. For example, what echo $obj; will print. This method must return a string.` |
|        - | 1989 | ` *  Example #2 Simple example` |
|        - | 1990 | ` * <?php` |
|        - | 1991 | ` * // Declare a simple class` |
|        - | 1992 | ` * class TestClass` |
|        - | 1993 | ` * {` |
|        - | 1994 | ` *   public $foo;` |
|        - | 1995 | ` *` |
|        - | 1996 | ` *   public function __construct($foo)` |
|        - | 1997 | ` *   {` |
|        - | 1998 | ` *       $this->foo = $foo;` |
|        - | 1999 | ` *   }` |
|        - | 2000 | ` *` |
|        - | 2001 | ` *   public function __toString()` |
|        - | 2002 | ` *   {` |
|        - | 2003 | ` *       return $this->foo;` |
|        - | 2004 | ` *   }` |
|        - | 2005 | ` * }` |
|        - | 2006 | ` * $class = new TestClass('Hello');` |
|        - | 2007 | ` * echo $class;` |
|        - | 2008 | ` * ?>` |
|        - | 2009 | ` * The above example will output:` |
|        - | 2010 | ` *  Hello` |
|        - | 2011 | ` *` |
|        - | 2012 | ` * Note that PH7 does not support all the magical method and introudces __toFloat(),__toInt()` |
|        - | 2013 | ` * which have the same behaviour as __toString() but for float and integer types` |
|        - | 2014 | ` * respectively.` |
|        - | 2015 | ` * Refer to the official documentation for more information.` |
|        - | 2016 | ` */` |
|     6145 | 2017 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallMagicMethod(` |
|        - | 2018 | `	ph7_vm *pVm,               /* VM that own all this stuff */` |
|        - | 2019 | `	ph7_class *pClass,         /* Target class */` |
|        - | 2020 | `	ph7_class_instance *pThis, /* Target object */` |
|        - | 2021 | `	const char *zMethod,       /* Magic method name [i.e: __toString()]*/` |
|        - | 2022 | `	sxu32 nByte,               /* zMethod length*/` |
|        - | 2023 | `	const SyString *pAttrName, /* Attribute name */` |
|        - | 2024 | `	ph7_value *pResult         /* OUT: magic method return value. NULL to discard */` |
|        - | 2025 | `	)` |
|        5 | 2026 | `{` |
|     6150 | 2027 | `	ph7_value *apArg[2] = { 0 , 0 };` |
|        - | 2028 | `	ph7_class_method *pMeth;` |
|        - | 2029 | `	ph7_value sAttr; /* cc warning */` |
|        - | 2030 | `	sxi32 rc;` |
|        - | 2031 | `	int nArg;` |
|        - | 2032 | `	/* Make sure the magic method is available */` |
|     6150 | 2033 | `	pMeth = PH7_ClassExtractMethod(&(*pClass),zMethod,nByte);` |
|     6150 | 2034 | `	if( pMeth == 0 ){` |
|        - | 2035 | `		/* No such method,return immediately */` |
|      ! 0 | 2036 | `		return SXERR_NOTFOUND;` |
|        - | 2037 | `	}` |
|     6150 | 2038 | `	nArg = 0;` |
|        - | 2039 | `	/* Copy arguments */` |
|     6150 | 2040 | `	if( pAttrName ){` |
|     6150 | 2041 | `		PH7_MemObjInitFromString(pVm,&sAttr,pAttrName);` |
|     6150 | 2042 | `		sAttr.nIdx = SXU32_HIGH; /* Mark as constant */` |
|     6150 | 2043 | `		apArg[0] = &sAttr;` |
|     6150 | 2044 | `		nArg = 1;` |
|     3073 | 2045 | `	}` |
|        - | 2046 | `	/* Call the magic method now */` |
|     6150 | 2047 | `	rc = PH7_VmCallMagicMethod(pVm,&(*pThis),pMeth,pResult,nArg,apArg);` |
|        - | 2048 | `	/* Clean up */` |
|     6150 | 2049 | `	if( pAttrName ){` |
|     6150 | 2050 | `		PH7_MemObjRelease(&sAttr);` |
|     3073 | 2051 | `	}` |
|     6150 | 2052 | `	return rc;` |
|     3078 | 2053 | `}` |
|        - | 2054 | `/*` |
|        - | 2055 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon].` |
|        - | 2056 | ` * This function is simply a wrapper on ExtractClassAttrValue().` |
|        - | 2057 | ` */` |
|  5830012 | 2058 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceExtractAttrValue(ph7_class_instance *pThis,VmClassAttr *pAttr)` |
|        5 | 2059 | `{` |
|        - | 2060 | `   /* Extract the attribute value */` |
|        - | 2061 | `	ph7_value *pValue;` |
|  5830017 | 2062 | `	pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|  5830017 | 2063 | `	return pValue;` |
|        5 | 2064 | `}` |
|        - | 2065 | `/*` |
|        - | 2066 | ` * Convert a class instance [i.e: Object in the PHP jargon] into a hashmap [i.e: array in the PHP jargon].` |
|        - | 2067 | ` * Return SXRET_OK on success. Any other value indicates failure.` |
|        - | 2068 | ` * Note on object conversion to array:` |
|        - | 2069 | ` *  Acccording to the PHP language reference manual` |
|        - | 2070 | ` *  If an object is converted to an array, the result is an array whose elements are the object's properties.` |
|        - | 2071 | ` *  The keys are the member variable names.` |
|        - | 2072 | ` *` |
|        - | 2073 | ` *  The following example:` |
|        - | 2074 | ` *  class Test {` |
|        - | 2075 | ` *   public $A = 25<<1;  // 50` |
|        - | 2076 | ` *	 public $c = rand_str(3);   // Random string of length 3` |
|        - | 2077 | ` *	 public $d = rand() & 1023; // Random number between 0..1023` |
|        - | 2078 | ` *  }` |
|        - | 2079 | ` *  var_dump((array) new Test());` |
|        - | 2080 | ` *	Will output:` |
|        - | 2081 | ` *  array(3) {` |
|        - | 2082 | ` *   [A] =>` |
|        - | 2083 | ` *      int(50)` |
|        - | 2084 | ` *   [c] =>` |
|        - | 2085 | ` *     string(3 'aps')` |
|        - | 2086 | ` *   [d] =>` |
|        - | 2087 | ` *     int(991)` |
|        - | 2088 | ` *  }` |
|        - | 2089 | ` * You have noticed that PH7 allow class attributes [i.e: $a,$c,$d in the example above]` |
|        - | 2090 | ` * have any complex expression (even function calls/Annonymous functions) as their default` |
|        - | 2091 | ` * value unlike the standard PHP engine.` |
|        - | 2092 | ` * This is a very powerful feature that you have to look at.` |
|        - | 2093 | ` */` |
|      126 | 2094 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|        4 | 2095 | `{` |
|        - | 2096 | `	{` |
|        - | 2097 | `		/* php's get_properties handler, which is what the (array) cast reads: a` |
|        - | 2098 | `		 * DateTime casts to date/timezone_type/timezone, not to the hidden slots` |
|        - | 2099 | `		 * holding its timestamp. A class whose hook answers only the DEBUG surface` |
|        - | 2100 | `		 * (WeakReference) fills nothing here, and that EMPTY answer is php's — the` |
|        - | 2101 | `		 * hook is authoritative, so there is no falling back to the slot walk. It` |
|        - | 2102 | `		 * used to fall back when the hook filled nothing, which was invisible while` |
|        - | 2103 | `		 * every hooked class also had no visible property: an ArrayObject SUBCLASS` |
|        - | 2104 | ``		 * with an empty storage would have cast to its own `p` where php casts to`` |
|        - | 2105 | `		 * the (empty) storage. */` |
|        - | 2106 | `		ph7_value sPresent;` |
|      130 | 2107 | `		PH7_MemObjInit(pThis->pVm,&sPresent);` |
|      130 | 2108 | `		sPresent.x.pOther = pMap;` |
|      130 | 2109 | `		MemObjSetType(&sPresent,MEMOBJ_HASHMAP);` |
|      130 | 2110 | `		if( PH7_ClassInstancePresent(pThis,&sPresent,0) ){` |
|        - | 2111 | `			/* The map IS the destination; do not release the carrier's hashmap. */` |
|       44 | 2112 | `			sPresent.x.pOther = 0;` |
|       44 | 2113 | `			sPresent.iFlags = MEMOBJ_NULL;` |
|       44 | 2114 | `			return SXRET_OK;` |
|        - | 2115 | `		}` |
|       88 | 2116 | `		sPresent.x.pOther = 0;` |
|       88 | 2117 | `		sPresent.iFlags = MEMOBJ_NULL;` |
|       88 | 2118 | `		PH7_MemObjRelease(&sPresent);` |
|        - | 2119 | `	}` |
|       88 | 2120 | `	return PH7_ClassInstanceToHashmapRaw(pThis,pMap);` |
|       67 | 2121 | `}` |
|        - | 2122 | `/*` |
|        - | 2123 | ` * Is this property NOT THERE YET?` |
|        - | 2124 | ` *` |
|        - | 2125 | ` * php 7.4's typed properties have a third state beside "holds a value" and "does not` |
|        - | 2126 | `` * exist": a typed property with no default is UNINITIALIZED at `new`, reading it is an`` |
|        - | 2127 | ` * Error rather than a null, and every surface that presents an object's properties` |
|        - | 2128 | ` * leaves it out — get_object_vars(), get_mangled_object_vars(), the (array) cast,` |
|        - | 2129 | ` * foreach, json_encode(), serialize(), print_r() and var_export(). var_dump() is the one` |
|        - | 2130 | ``  * exception and only half of one: it NAMES the property and prints `uninitialized(T)` `` |
|        - | 2131 | ` * where the value would be, and does not count it in the header.` |
|        - | 2132 | ` *` |
|        - | 2133 | ` * The state is already tracked (it is what makes the read throw); this asks it by name` |
|        - | 2134 | ` * so the eight surfaces agree. VM_CLASS_ATTR_UNINIT doubles as the readonly write-once` |
|        - | 2135 | ` * latch, which wants the same answer: a readonly property must be typed and cannot have` |
|        - | 2136 | ` * a default, so before its first write php calls it uninitialized too.` |
|        - | 2137 | ` */` |
|     1710 | 2138 | `PH7_PRIVATE int PH7_ClassAttrUninitialized(VmClassAttr *pVmAttr)` |
|        5 | 2139 | `{` |
|     1715 | 2140 | `	return pVmAttr != 0 && (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) != 0;` |
|        5 | 2141 | `}` |
|        - | 2142 | `/*` |
|        - | 2143 | `` * The same question asked by the four surfaces that read through a php 8.4 `get` HOOK —`` |
|        - | 2144 | ` * get_object_vars(), json_encode(), foreach and var_export().` |
|        - | 2145 | ` *` |
|        - | 2146 | ` * A hooked property's value is whatever its hook answers, so an empty backing slot is` |
|        - | 2147 | ` * not an absence there: a VIRTUAL property has no slot at all and still has a value` |
|        - | 2148 | `` * (php lists `virt` in all four), and a BACKED one whose hook reads its own slot raises`` |
|        - | 2149 | ` * the uninitialized Error from inside the hook — which is php's answer for these four` |
|        - | 2150 | ``  * and not something to pre-empt by skipping the property. Only a property with no `get` `` |
|        - | 2151 | ` * at all is absent.` |
|        - | 2152 | ` */` |
|      746 | 2153 | `PH7_PRIVATE int PH7_ClassAttrUninitializedForRead(VmClassAttr *pVmAttr)` |
|        5 | 2154 | `{` |
|      807 | 2155 | `	return PH7_ClassAttrUninitialized(pVmAttr)` |
|      746 | 2156 | `		&& (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET) == 0;` |
|        5 | 2157 | `}` |
|        - | 2158 | `/*` |
|        - | 2159 | ` * The SLOT walk under the cast above, with php's get_properties handler left out:` |
|        - | 2160 | ` * the instance's own property table, mangled, and nothing else.` |
|        - | 2161 | ` *` |
|        - | 2162 | `` * This is what `get_mangled_object_vars()` answers — php asks the same handler with a`` |
|        - | 2163 | ` * different PURPOSE there, and every class here that has a handler answers the raw` |
|        - | 2164 | `` * table for it: `get_mangled_object_vars(new ArrayObject([1,2]))` is the EMPTY array`` |
|        - | 2165 | `` * where the cast is `[1,2]`, and a DateTime's is empty where the cast has three keys.`` |
|        - | 2166 | ` * Since PHL's engine slots are hidden (and php's equivalents live outside the property` |
|        - | 2167 | ` * table entirely), dropping the handler is the whole difference.` |
|        - | 2168 | ` */` |
|       98 | 2169 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmapRaw(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|        4 | 2170 | `{` |
|        - | 2171 | `	SyHashEntry *pEntry;` |
|        - | 2172 | `	SyString *pAttrName;` |
|        - | 2173 | `	VmClassAttr *pAttr;` |
|        - | 2174 | `	ph7_value *pValue;` |
|        - | 2175 | `	ph7_value sName;` |
|        - | 2176 | `	/* Reset the loop cursor */` |
|      102 | 2177 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      102 | 2178 | `	PH7_MemObjInitFromString(pThis->pVm,&sName,0);` |
|      436 | 2179 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|        - | 2180 | `		/* Point to the current attribute */` |
|      338 | 2181 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      338 | 2182 | `		if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_HIDDEN) ){` |
|        - | 2183 | `			/* A static property is the CLASS's, not the object's: php's cast` |
|        - | 2184 | `			 * yields only the instance's own properties. */` |
|       80 | 2185 | `			continue;` |
|        - | 2186 | `		}` |
|      262 | 2187 | `		if( PH7_ClassAttrUninitialized(pAttr) ){` |
|       81 | 2188 | `			continue; /* typed, never written: not there yet (php) */` |
|        - | 2189 | `		}` |
|      184 | 2190 | `		if( pAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|        - | 2191 | `			/* php 8.4: a VIRTUAL hooked property has no backing store — the` |
|        - | 2192 | `			 * (array) cast excludes it (raw surface, get is NOT dispatched) */` |
|      ! 0 | 2193 | `			continue;` |
|        - | 2194 | `		}` |
|        - | 2195 | `		/* Extract attribute value */` |
|      184 | 2196 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|      184 | 2197 | `		if( pValue ){` |
|        - | 2198 | `			/* Build attribute name. php MANGLES the key of a non-public property` |
|        - | 2199 | `			 * when it casts an object to an array: a private one becomes` |
|        - | 2200 | `			 * "\0DeclaringClass\0name" and a protected one "\0*\0name", so two` |
|        - | 2201 | `			 * same-named members from different visibility levels stay distinct` |
|        - | 2202 | ``			 * and `isset($arr['priv'])` is FALSE — PHL emitted the bare name,`` |
|        - | 2203 | `			 * which collided them and answered TRUE. The NULs are real bytes in` |
|        - | 2204 | `			 * the key (this append is length-based, not NUL-terminated). */` |
|      184 | 2205 | `			pAttrName = &pAttr->pAttr->sName;` |
|      184 | 2206 | `			if( pAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       35 | 2207 | `				ph7_class *pDecl = pAttr->pAttr->pDeclClass` |
|       22 | 2208 | `					? pAttr->pAttr->pDeclClass : pThis->pClass;` |
|       24 | 2209 | `				PH7_MemObjStringAppend(&sName,"\0",1);` |
|       24 | 2210 | `				PH7_MemObjStringAppend(&sName,SyStringData(&pDecl->sName),SyStringLength(&pDecl->sName));` |
|       24 | 2211 | `				PH7_MemObjStringAppend(&sName,"\0",1);` |
|      173 | 2212 | `			}else if( pAttr->pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
|       23 | 2213 | `				PH7_MemObjStringAppend(&sName,"\0*\0",3);` |
|       10 | 2214 | `			}` |
|      184 | 2215 | `			PH7_MemObjStringAppend(&sName,pAttrName->zString,pAttrName->nByte);` |
|        - | 2216 | `			/* Perform the insertion */` |
|      184 | 2217 | `			PH7_HashmapInsert(pMap,&sName,pValue);` |
|        - | 2218 | `			/* Reset the string cursor */` |
|      184 | 2219 | `			SyBlobReset(&sName.sBlob);` |
|       90 | 2220 | `		}` |
|        4 | 2221 | `	}` |
|      102 | 2222 | `	PH7_MemObjRelease(&sName);` |
|      102 | 2223 | `	return SXRET_OK;` |
|        4 | 2224 | `}` |
|        - | 2225 | `/*` |
|        - | 2226 | ` * Iterate throw class attributes and invoke the given callback [i.e: xWalk()] for each` |
|        - | 2227 | ` * retrieved attribute.` |
|        - | 2228 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|        - | 2229 | ` * the attribute value in the callback body will not alter the real attribute value.` |
|        - | 2230 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|        - | 2231 | ` * a value different from PH7_OK.` |
|        - | 2232 | ` * Refer to [ph7_object_walk()] for more information.` |
|        - | 2233 | ` */` |
|      ! 0 | 2234 | `PH7_PRIVATE sxi32 PH7_ClassInstanceWalk(` |
|        - | 2235 | `	ph7_class_instance *pThis, /* Target object */` |
|        - | 2236 | `	int (*xWalk)(const char *,ph7_value *,void *), /* Walker callback */` |
|        - | 2237 | `	void *pUserData /* Last argument to xWalk() */` |
|        - | 2238 | `	)` |
|      ! 0 | 2239 | `{` |
|        - | 2240 | `	SyHashEntry *pEntry; /* Hash entry */` |
|        - | 2241 | `	VmClassAttr *pAttr;  /* Pointer to the attribute */` |
|        - | 2242 | `	ph7_value *pValue;   /* Attribute value */` |
|        - | 2243 | `	ph7_value sValue;    /* Copy of the attribute value */` |
|        - | 2244 | `	int rc;` |
|        - | 2245 | `	/* Reset the loop cursor */` |
|      ! 0 | 2246 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      ! 0 | 2247 | `	PH7_MemObjInit(pThis->pVm,&sValue);` |
|        - | 2248 | `	/* Start the walk process */` |
|      ! 0 | 2249 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|        - | 2250 | `		/* Point to the current attribute */` |
|      ! 0 | 2251 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      ! 0 | 2252 | `		if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_HIDDEN) ){` |
|        - | 2253 | `			/* Class-level members are not part of the object (php) */` |
|      ! 0 | 2254 | `			continue;` |
|        - | 2255 | `		}` |
|      ! 0 | 2256 | `		if( PH7_ClassAttrUninitialized(pAttr) ){` |
|      ! 0 | 2257 | `			continue; /* typed, never written: not there yet (php) */` |
|        - | 2258 | `		}` |
|        - | 2259 | `		/* Extract attribute value */` |
|      ! 0 | 2260 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|      ! 0 | 2261 | `		if( pValue ){` |
|      ! 0 | 2262 | `			PH7_MemObjLoad(pValue,&sValue);` |
|        - | 2263 | `			/* Invoke the supplied callback */` |
|      ! 0 | 2264 | `			rc =  xWalk(SyStringData(&pAttr->pAttr->sName),&sValue,pUserData);` |
|      ! 0 | 2265 | `			PH7_MemObjRelease(&sValue);` |
|      ! 0 | 2266 | `			if( rc != PH7_OK){` |
|        - | 2267 | `				/* User callback request an operation abort */` |
|      ! 0 | 2268 | `				return SXERR_ABORT;` |
|        - | 2269 | `			}` |
|      ! 0 | 2270 | `		}` |
|      ! 0 | 2271 | `	}` |
|        - | 2272 | `	/* All done */` |
|      ! 0 | 2273 | `	return SXRET_OK;` |
|      ! 0 | 2274 | `}` |
|        - | 2275 | `/*` |
|        - | 2276 | ` * The instance's attribute entry for a property name, INCLUDING the empty one.` |
|        - | 2277 | ` *` |
|        - | 2278 | ` * SyHashGet answers 0 for a zero-length key engine-wide, so a property named ""` |
|        - | 2279 | `` * — php's own `s:0:""` payload builds one, and every surface that walks hAttr`` |
|        - | 2280 | ` * shows it — can only be found by walking the list. Used by the presentation` |
|        - | 2281 | ` * surfaces that snapshot names and re-look-up each one before reading it (a PHP` |
|        - | 2282 | ` * 8.4 get hook may unset a property mid-walk), where the plain hash probe would` |
|        - | 2283 | ` * silently drop it from the output while var_dump, which walks directly, showed` |
|        - | 2284 | ` * it. The walk uses the hash's embedded loop cursor, so it must not run inside` |
|        - | 2285 | ` * another walk of the SAME table — the callers below re-enter only through the` |
|        - | 2286 | ` * hook dispatch, which happens after this returns.` |
|        - | 2287 | ` */` |
|      478 | 2288 | `PH7_PRIVATE SyHashEntry * PH7_ClassInstanceAttrEntry(ph7_class_instance *pThis,const char *zName,sxu32 nName)` |
|        5 | 2289 | `{` |
|        - | 2290 | `	SyHashEntry *pEntry;` |
|      483 | 2291 | `	if( nName > 0 ){` |
|      465 | 2292 | `		return SyHashGet(&pThis->hAttr,(const void *)zName,nName);` |
|        - | 2293 | `	}` |
|       19 | 2294 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|       29 | 2295 | `	while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 | 2296 | `		if( pEntry->nKeyLen == 0 ){` |
|       11 | 2297 | `			return pEntry;` |
|        - | 2298 | `		}` |
|        1 | 2299 | `	}` |
|        9 | 2300 | `	return 0;` |
|      244 | 2301 | `}` |
|        - | 2302 | `/*` |
|        - | 2303 | ` * Extract a class atrribute value.` |
|        - | 2304 | ` * Return a pointer to the attribute value on success. Otherwise NULL.` |
|        - | 2305 | ` * Note:` |
|        - | 2306 | ` *  Access to static and constant attribute is not allowed. That is,the function` |
|        - | 2307 | ` *  will return NULL in case someone (host-application code) try to extract` |
|        - | 2308 | ` *  a static/constant attribute.` |
|        - | 2309 | ` */` |
|  1671446 | 2310 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceFetchAttr(ph7_class_instance *pThis,const SyString *pName)` |
|        5 | 2311 | `{` |
|        - | 2312 | `	SyHashEntry *pEntry;` |
|        - | 2313 | `	VmClassAttr *pAttr;` |
|        - | 2314 | `	/* Query the attribute hashtable */` |
|  1671451 | 2315 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);` |
|  1671451 | 2316 | `	if( pEntry == 0 ){` |
|        - | 2317 | `		/* No such attribute */` |
|      150 | 2318 | `		return 0;` |
|        - | 2319 | `	}` |
|        - | 2320 | `	/* Point to the class atrribute */` |
|  1671303 | 2321 | `	pAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - | 2322 | `	/* Check if we are dealing with a static/constant attribute */` |
|  1671303 | 2323 | `	if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - | 2324 | `		/* Access is forbidden */` |
|      ! 0 | 2325 | `		return 0;` |
|        - | 2326 | `	}` |
|        - | 2327 | `	/* Return the attribute value */` |
|  1671303 | 2328 | `	return ExtractClassAttrValue(pThis->pVm,pAttr);` |
|   835728 | 2329 | `}` |
|        - | 2330 | `/*` |
|        - | 2331 | `` * Does a WRITE through `$obj[k]` land on this class's storage? php answers with`` |
|        - | 2332 | ` * the class's read_dimension handler: an internal class whose own handler hands` |
|        - | 2333 | ` * back the real element supports indirect modification, while everything routed` |
|        - | 2334 | ` * through zend_std_read_dimension — every userland ArrayAccess, and the SPL` |
|        - | 2335 | ` * classes that keep the standard handler (SplFixedArray, the SplDoublyLinkedList` |
|        - | 2336 | ` * family, SplObjectStorage) — gets a TEMPORARY back, so the write is lost and php` |
|        - | 2337 | ` * says so. The engine cannot tell those apart from the interface alone: both` |
|        - | 2338 | ` * implement ArrayAccess.` |
|        - | 2339 | ` *` |
|        - | 2340 | ` * PHL's equivalent evidence is the offsetGet that ANSWERS: the native one on a` |
|        - | 2341 | ` * class carrying PH7_CLASS_DIM_WRITABLE means the storage is reachable, and a` |
|        - | 2342 | ` * userland OVERRIDE of it means it is not — which is php's own rule for an` |
|        - | 2343 | ` * ArrayObject subclass (spl_array_read_dimension steps aside for a declared` |
|        - | 2344 | `` * offsetGet). A `&offsetGet` return is php's other yes: it hands back a reference,`` |
|        - | 2345 | ` * so the write reaches whatever it aliases.` |
|        - | 2346 | ` */` |
|      448 | 2347 | `PH7_PRIVATE int PH7_VmDimFetchWritable(ph7_class *pClass)` |
|        5 | 2348 | `{` |
|        - | 2349 | `	ph7_class_method *pGet;` |
|        - | 2350 | `	ph7_class *pCur;` |
|      453 | 2351 | `	if( pClass == 0 ){` |
|      ! 0 | 2352 | `		return FALSE;` |
|        - | 2353 | `	}` |
|      453 | 2354 | `	pGet = PH7_ClassExtractMethod(pClass,"offsetGet",sizeof("offsetGet")-1);` |
|      453 | 2355 | `	if( pGet == 0 ){` |
|      ! 0 | 2356 | `		return FALSE;` |
|        - | 2357 | `	}` |
|      453 | 2358 | `	if( pGet->sFunc.iFlags & VM_FUNC_REF_RETURN ){` |
|        5 | 2359 | `		return TRUE;` |
|        - | 2360 | `	}` |
|      449 | 2361 | `	if( (pGet->sFunc.iFlags & VM_FUNC_NATIVE) == 0 ){` |
|      161 | 2362 | `		return FALSE;` |
|        - | 2363 | `	}` |
|      434 | 2364 | `	for( pCur = pClass ; pCur ; pCur = pCur->pBase ){` |
|      342 | 2365 | `		if( pCur->iFlags & PH7_CLASS_DIM_WRITABLE ){` |
|      198 | 2366 | `			return TRUE;` |
|        - | 2367 | `		}` |
|       73 | 2368 | `	}` |
|       93 | 2369 | `	return FALSE;` |
|      229 | 2370 | `}` |
|        - | 2371 | `/*` |
|        - | 2372 | ` * The three accessors a VM_FUNC_NATIVE method body uses to reach its receiver.` |
|        - | 2373 | ` *` |
|        - | 2374 | ` * A native method is dispatched down the host-function path, so it is handed the` |
|        - | 2375 | ` * plain (ph7_context*, argc, argv) of any builtin — the receiver rides on the` |
|        - | 2376 | ` * context instead of occupying an argument slot. That is the whole point: the C` |
|        - | 2377 | `` * routine that used to be a global `__prefix_verb($target, ...)` thunk taking its`` |
|        - | 2378 | ` * target explicitly becomes a method whose target is $this.` |
|        - | 2379 | ` */` |
|        - | 2380 | `/*` |
|        - | 2381 | ` * The receiver, or NULL when the method was called statically (and always NULL in` |
|        - | 2382 | ` * a plain host function). Borrowed: the operand stack holds the reference for the` |
|        - | 2383 | ` * duration of the call, so the body must not unref it.` |
|        - | 2384 | ` */` |
|  1526371 | 2385 | `PH7_PRIVATE ph7_class_instance * PH7_ContextThis(ph7_context *pCtx)` |
|        5 | 2386 | `{` |
|  1526376 | 2387 | `	return pCtx->pThis;` |
|        5 | 2388 | `}` |
|        - | 2389 | `/*` |
|        - | 2390 | ` * The class the call was made THROUGH — php's late-static-binding target, so an` |
|        - | 2391 | ` * inherited native method sees the SUBCLASS here, not the class that declared it.` |
|        - | 2392 | ` * NULL in a plain host function.` |
|        - | 2393 | ` */` |
|      354 | 2394 | `PH7_PRIVATE ph7_class * PH7_ContextCalledClass(ph7_context *pCtx)` |
|        2 | 2395 | `{` |
|      356 | 2396 | `	return pCtx->pCalledClass;` |
|        2 | 2397 | `}` |
|        - | 2398 | `/*` |
|        - | 2399 | ` * The receiver as a ph7_value, so the body can use the ordinary object helpers` |
|        - | 2400 | ` * (ph7_object_fetch_attr, ph7_value_is_object, ph7_result_value, ...) instead of` |
|        - | 2401 | ` * reaching into ph7_class_instance. NULL when there is no receiver.` |
|        - | 2402 | ` *` |
|        - | 2403 | ` * The view ALIASES the instance without bumping its refcount: it lives exactly as` |
|        - | 2404 | ` * long as the call, during which the caller's reference is already keeping the` |
|        - | 2405 | ` * object alive, and VmReleaseCallContext drops the alias without unreffing. Handing` |
|        - | 2406 | ` * it to ph7_result_value() is safe — that copies through PH7_MemObjStore, which` |
|        - | 2407 | ` * takes its own reference.` |
|        - | 2408 | ` */` |
|     5636 | 2409 | `PH7_PRIVATE ph7_value * PH7_ContextThisValue(ph7_context *pCtx)` |
|        5 | 2410 | `{` |
|     5641 | 2411 | `	if( pCtx->pThis == 0 ){` |
|      ! 0 | 2412 | `		return 0;` |
|        - | 2413 | `	}` |
|     5641 | 2414 | `	if( !pCtx->bThisInit ){` |
|     5641 | 2415 | `		PH7_MemObjInit(pCtx->pVm,&pCtx->sThis);` |
|     5641 | 2416 | `		pCtx->sThis.x.pOther = pCtx->pThis;` |
|     5641 | 2417 | `		pCtx->sThis.iFlags = MEMOBJ_OBJ;` |
|     5641 | 2418 | `		pCtx->sThis.nIdx = SXU32_HIGH;` |
|     5641 | 2419 | `		pCtx->bThisInit = 1;` |
|     2818 | 2420 | `	}` |
|     5641 | 2421 | `	return &pCtx->sThis;` |
|     2823 | 2422 | `}` |
|        - | 2423 |  |
