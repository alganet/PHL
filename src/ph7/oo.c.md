# src/ph7/oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 782/905 lines (86.41%)

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
|   618880 |   14 | `PH7_PRIVATE ph7_class * PH7_NewRawClass(ph7_vm *pVm,const SyString *pName,sxu32 nLine)` |
|        5 |   15 | `{` |
|        - |   16 | `	ph7_class *pClass;` |
|        - |   17 | `	char *zName;` |
|        - |   18 | `	/* Allocate a new instance */` |
|   618885 |   19 | `	pClass = (ph7_class *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class));` |
|   618885 |   20 | `	if( pClass == 0 ){` |
|      ! 0 |   21 | `		return 0;` |
|        - |   22 | `	}` |
|        - |   23 | `	/* Zero the structure */` |
|   618885 |   24 | `	SyZero(pClass,sizeof(ph7_class));` |
|        - |   25 | `	/* Duplicate class name */` |
|   618885 |   26 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   618885 |   27 | `	if( zName == 0 ){` |
|      ! 0 |   28 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClass);` |
|      ! 0 |   29 | `		return 0;` |
|        - |   30 | `	}` |
|        - |   31 | `	/* Initialize fields */` |
|   618885 |   32 | `	SyStringInitFromBuf(&pClass->sName,zName,pName->nByte);` |
|        - |   33 | `	/* php method names are CASE-INSENSITIVE ($o->FOO() finds foo(), and declaring` |
|        - |   34 | `	 * both is a redeclaration), so the method table matches on them the same way` |
|        - |   35 | `	 * hClass does for class names. Properties and class constants ARE case` |
|        - |   36 | `	 * sensitive in php, so hAttr keeps the default exact comparator. */` |
|   618885 |   37 | `	SyHashInit(&pClass->hMethod,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|   618885 |   38 | `	SyHashInit(&pClass->hAttr,&pVm->sAllocator,0,0);` |
|   618885 |   39 | `	SyHashInit(&pClass->hConst,&pVm->sAllocator,0,0);` |
|   618885 |   40 | `	SyHashInit(&pClass->hDerived,&pVm->sAllocator,0,0);` |
|   618885 |   41 | `	SySetInit(&pClass->aInterface,&pVm->sAllocator,sizeof(ph7_class *));` |
|   618885 |   42 | `	SySetInit(&pClass->aTrait,&pVm->sAllocator,sizeof(ph7_class *));` |
|   618885 |   43 | `	SySetInit(&pClass->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|   618885 |   44 | `	SySetInit(&pClass->aEnumCases,&pVm->sAllocator,sizeof(ph7_class_attr *));` |
|   618885 |   45 | `	pClass->nLine = nLine;` |
|   618885 |   46 | `	if( pVm->bCompilingBuiltin ){` |
|        - |   47 | `		/* Defined by an embedded builtin chunk: internal, no defining file.` |
|        - |   48 | `		 * Class compilers merge further flags with \|= so this survives. */` |
|   615813 |   49 | `		pClass->iFlags \|= PH7_CLASS_INTERNAL;` |
|   307909 |   50 | `	}else{` |
|        - |   51 | `		/* Alias the VM-lifetime path dup on top of the include stack */` |
|     3077 |   52 | `		SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     3077 |   53 | `		if( pFile ){` |
|     3077 |   54 | `			SyStringDupPtr(&pClass->sFile,pFile);` |
|     1536 |   55 | `		}` |
|        - |   56 | `	}` |
|        - |   57 | `	/* All done */` |
|   618885 |   58 | `	return pClass;` |
|   309445 |   59 | `}` |
|        - |   60 | `/*` |
|        - |   61 | ` * Allocate and initialize a new class attribute.` |
|        - |   62 | ` * Return a pointer to the class attribute on success. NULL otherwise.` |
|        - |   63 | ` */` |
|  1106918 |   64 | `PH7_PRIVATE ph7_class_attr * PH7_NewClassAttr(ph7_vm *pVm,const SyString *pName,sxu32 nLine,sxi32 iProtection,sxi32 iFlags)` |
|        5 |   65 | `{` |
|        - |   66 | `	ph7_class_attr *pAttr;` |
|        - |   67 | `	char *zName;` |
|  1106923 |   68 | `	pAttr = (ph7_class_attr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_attr));` |
|  1106923 |   69 | `	if( pAttr == 0 ){` |
|      ! 0 |   70 | `		return 0;` |
|        - |   71 | `	}` |
|        - |   72 | `	/* Zero the structure */` |
|  1106923 |   73 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|  1106923 |   74 | `	SySetInit(&pAttr->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|        - |   75 | `	/* Duplicate attribute name */` |
|  1106923 |   76 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1106923 |   77 | `	if( zName == 0 ){` |
|      ! 0 |   78 | `		SyMemBackendPoolFree(&pVm->sAllocator,pAttr);` |
|      ! 0 |   79 | `		return 0;` |
|        - |   80 | `	}` |
|        - |   81 | `	/* Initialize fields */` |
|  1106923 |   82 | `	SySetInit(&pAttr->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|  1106923 |   83 | `	SySetInit(&pAttr->aUnionAlts,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|  1106923 |   84 | `	SyStringInitFromBuf(&pAttr->sName,zName,pName->nByte);` |
|  1106923 |   85 | `	pAttr->iProtection = iProtection;` |
|  1106923 |   86 | `	pAttr->nIdx = SXU32_HIGH;` |
|  1106923 |   87 | `	pAttr->iFlags = iFlags;` |
|  1106923 |   88 | `	pAttr->nLine = nLine;` |
|  1106923 |   89 | `	return pAttr;` |
|   553464 |   90 | `}` |
|        - |   91 | `/*` |
|        - |   92 | ` * Allocate and initialize a new class method.` |
|        - |   93 | ` * Return a pointer to the class method on success. NULL otherwise` |
|        - |   94 | ` * This function associate with the newly created method an automatically generated` |
|        - |   95 | ` * random unique name.` |
|        - |   96 | ` */` |
|  3380618 |   97 | `PH7_PRIVATE ph7_class_method * PH7_NewClassMethod(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,sxu32 nLine,` |
|        - |   98 | `	sxi32 iProtection,sxi32 iFlags,sxi32 iFuncFlags)` |
|        5 |   99 | `{` |
|        - |  100 | `	ph7_class_method *pMeth;` |
|        - |  101 | `	SyHashEntry *pEntry;` |
|        - |  102 | `	SyString *pNamePtr;` |
|        - |  103 | `	char zSalt[10];` |
|        - |  104 | `	char *zName;` |
|        - |  105 | `	sxu32 nByte;` |
|        - |  106 | `	/* Allocate a new class method instance */` |
|  3380623 |  107 | `	pMeth = (ph7_class_method *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_method));` |
|  3380623 |  108 | `	if( pMeth == 0 ){` |
|      ! 0 |  109 | `		return 0;` |
|        - |  110 | `	}` |
|        - |  111 | `	/* Zero the structure */` |
|  3380623 |  112 | `	SyZero(pMeth,sizeof(ph7_class_method));` |
|        - |  113 | `	/* Check for an already installed method with the same name */` |
|  3380623 |  114 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
|  3380623 |  115 | `	if( pEntry == 0 ){` |
|        - |  116 | `		/* Associate an unique VM name to this method */` |
|  3380619 |  117 | `		nByte = sizeof(zSalt) + pName->nByte + SyStringLength(&pClass->sName)+sizeof(char)*7/*[[__'\0'*/;` |
|  3380619 |  118 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,nByte);` |
|  3380619 |  119 | `		if( zName == 0 ){` |
|      ! 0 |  120 | `			SyMemBackendPoolFree(&pVm->sAllocator,pMeth);` |
|      ! 0 |  121 | `			return 0;` |
|        - |  122 | `		}` |
|  3380619 |  123 | `		pNamePtr = &pMeth->sVmName;` |
|        - |  124 | `		/* Generate a random string */` |
|  3380619 |  125 | `		PH7_VmRandomString(&(*pVm),zSalt,sizeof(zSalt));` |
|  3380619 |  126 | `		pNamePtr->nByte = SyBufferFormat(zName,nByte,"[__%z@%z_%.*s]",&pClass->sName,pName,sizeof(zSalt),zSalt);` |
|  3380619 |  127 | `		pNamePtr->zString = zName;` |
|  1690312 |  128 | `	}else{` |
|        - |  129 | `		/* Method is condidate for 'overloading' */` |
|        6 |  130 | `		ph7_class_method *pCurrent = (ph7_class_method *)pEntry->pUserData;` |
|        6 |  131 | `		pNamePtr = &pMeth->sVmName;` |
|        - |  132 | `		/* Use the same VM name */` |
|        6 |  133 | `		SyStringDupPtr(pNamePtr,&pCurrent->sVmName);` |
|        6 |  134 | `		zName = (char *)pNamePtr->zString;` |
|        - |  135 | `	}` |
|  3380623 |  136 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|   194853 |  137 | `		if( pName->nByte == sizeof("__destruct") - 1 && SyMemcmp(pName->zString,"__destruct",sizeof("__destruct") - 1 ) == 0 ){` |
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
|    97424 |  148 | `	}` |
|        - |  149 | `	/* Initialize method fields */` |
|  3380623 |  150 | `	pMeth->iProtection = iProtection;` |
|  3380623 |  151 | `	pMeth->iFlags = iFlags;` |
|  3380623 |  152 | `	pMeth->nLine = nLine;` |
|  5070932 |  153 | `	PH7_VmInitFuncState(&(*pVm),&pMeth->sFunc,&zName[sizeof(char)*4/*[__@*/+SyStringLength(&pClass->sName)],` |
|  3380618 |  154 | `		pName->nByte,iFuncFlags\|VM_FUNC_CLASS_METHOD,pClass);` |
|  3380623 |  155 | `	return pMeth;` |
|  1690314 |  156 | `}` |
|        - |  157 | `/*` |
|        - |  158 | ` * Check if the given name have a class method associated with it.` |
|        - |  159 | ` * Return the desired method [i.e: ph7_class_method instance] on success. NULL otherwise.` |
|        - |  160 | ` */` |
| 12969496 |  161 | `PH7_PRIVATE ph7_class_method * PH7_ClassExtractMethod(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|        5 |  162 | `{` |
|        - |  163 | `	SyHashEntry *pEntry;` |
|        - |  164 | `	/* Perform a hash lookup */` |
| 12969501 |  165 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)zName,nByte);` |
| 12969501 |  166 | `	if( pEntry == 0 ){` |
|        - |  167 | `		/* No such entry */` |
|  7970929 |  168 | `		return 0;` |
|        - |  169 | `	}` |
|        - |  170 | `	/* Point to the desired method */` |
|  4998577 |  171 | `	return (ph7_class_method *)pEntry->pUserData;` |
|  6484753 |  172 | `}` |
|        - |  173 | `/*` |
|        - |  174 | ` * Check if the given name is a class attribute.` |
|        - |  175 | ` * Return the desired attribute [i.e: ph7_class_attr instance] on success.NULL otherwise.` |
|        - |  176 | ` */` |
|   777006 |  177 | `PH7_PRIVATE ph7_class_attr * PH7_ClassExtractAttribute(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|        5 |  178 | `{` |
|        - |  179 | `	SyHashEntry *pEntry;` |
|        - |  180 | `	/* Perform a hash lookup */` |
|   777011 |  181 | `	pEntry = SyHashGet(&pClass->hAttr,(const void *)zName,nByte);` |
|   777011 |  182 | `	if( pEntry == 0 ){` |
|        - |  183 | `		/* No such entry */` |
|   676307 |  184 | `		return 0;` |
|        - |  185 | `	}` |
|        - |  186 | `	/* Point to the desierd method */` |
|   100709 |  187 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|   388508 |  188 | `}` |
|        - |  189 | `/*` |
|        - |  190 | ` * Check if the given name is a class CONSTANT (or enum case).` |
|        - |  191 | ` * php keeps constants and properties in separate namespaces, so constants live` |
|        - |  192 | ` * in a dedicated table (hConst) and never collide with a same-named property.` |
|        - |  193 | ` * Return the desired constant [ph7_class_attr with PH7_CLASS_ATTR_CONSTANT] on` |
|        - |  194 | ` * success, NULL otherwise.` |
|        - |  195 | ` */` |
|   400170 |  196 | `PH7_PRIVATE ph7_class_attr * PH7_ClassExtractConstant(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|        5 |  197 | `{` |
|        - |  198 | `	SyHashEntry *pEntry;` |
|   400175 |  199 | `	pEntry = SyHashGet(&pClass->hConst,(const void *)zName,nByte);` |
|   400175 |  200 | `	if( pEntry == 0 ){` |
|   398897 |  201 | `		return 0;` |
|        - |  202 | `	}` |
|     1283 |  203 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|   200090 |  204 | `}` |
|        - |  205 | `/*` |
|        - |  206 | ` * Install a class attribute in the corresponding container.` |
|        - |  207 | ` * A constant (or enum case) goes to hConst, a property to hAttr — php's two` |
|        - |  208 | `` * separate member namespaces, so `const C` and `public $C` coexist.`` |
|        - |  209 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|        - |  210 | ` */` |
|  1106914 |  211 | `PH7_PRIVATE sxi32 PH7_ClassInstallAttr(ph7_class *pClass,ph7_class_attr *pAttr)` |
|        5 |  212 | `{` |
|  1106919 |  213 | `	SyString *pName = &pAttr->sName;` |
|        - |  214 | `	sxi32 rc;` |
|        - |  215 | `	/* Remember where this attribute was originally declared so that later` |
|        - |  216 | `	 * inheritance/trait copies still know the declaring class (needed for` |
|        - |  217 | `	 * PHP-compatible error messages on typed properties). */` |
|  1106919 |  218 | `	if( pAttr->pDeclClass == 0 ){` |
|  1093335 |  219 | `		pAttr->pDeclClass = pClass;` |
|   546665 |  220 | `	}` |
|  1106919 |  221 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|   408009 |  222 | `		rc = SyHashInsertTail(&pClass->hConst,(const void *)pName->zString,pName->nByte,pAttr);` |
|   204007 |  223 | `	}else{` |
|   698915 |  224 | `		rc = SyHashInsertTail(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|        - |  225 | `	}` |
|  1106919 |  226 | `	return rc;` |
|        5 |  227 | `}` |
|        - |  228 | `/*` |
|        - |  229 | ` * Install a class method in the corresponding container.` |
|        - |  230 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|        - |  231 | ` */` |
|  3380580 |  232 | `PH7_PRIVATE sxi32 PH7_ClassInstallMethod(ph7_class *pClass,ph7_class_method *pMeth)` |
|        5 |  233 | `{` |
|  3380585 |  234 | `	SyString *pName = &pMeth->sFunc.sName;` |
|        - |  235 | `	sxi32 rc;` |
|  3380585 |  236 | `	rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|  3380585 |  237 | `	return rc;` |
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
|   326284 |  262 | `static int OoClassifyOverrideType(ph7_vm *pVm, sxu32 nType, const SyString *pClass,` |
|        - |  263 | `	int bUnion, ph7_class **ppClass)` |
|        5 |  264 | `{` |
|   326289 |  265 | `	*ppClass = 0;` |
|   326289 |  266 | `	if( bUnion ){` |
|        3 |  267 | `		return OVT_SKIP; /* union/intersection — full lattice, skip */` |
|        - |  268 | `	}` |
|   326287 |  269 | `	if( nType == 0 ){` |
|   317077 |  270 | `		return OVT_NONE; /* no declared type */` |
|        - |  271 | `	}` |
|     9215 |  272 | `	if( nType == SXU32_HIGH ){` |
|        - |  273 | `		/* A class name OR a pseudo-type stored as a name atom. Skip every pseudo` |
|        - |  274 | `		 * (incl. self/parent/static, which are context-relative). */` |
|        - |  275 | `		static const struct { const char *z; sxu32 n; } aPseudo[] = {` |
|        - |  276 | `			{"mixed",5}, {"never",5}, {"iterable",8}, {"callable",8}, {"true",4},` |
|        - |  277 | `			{"false",5}, {"self",4}, {"parent",6}, {"static",6}` |
|        - |  278 | `		};` |
|       23 |  279 | `		const char *z = pClass->zString;` |
|       23 |  280 | `		sxu32 n = pClass->nByte;` |
|        - |  281 | `		SyHashEntry *pE;` |
|        - |  282 | `		sxu32 i;` |
|      159 |  283 | `		for( i = 0; i < SX_ARRAYSIZE(aPseudo); i++ ){` |
|      147 |  284 | `			if( n == aPseudo[i].n && SyStrnmicmp(z,aPseudo[i].z,n) == 0 ){` |
|       10 |  285 | `				return OVT_SKIP;` |
|        - |  286 | `			}` |
|       70 |  287 | `		}` |
|       14 |  288 | `		pE = SyHashGet(&pVm->hClass,(const void *)z,n);` |
|       14 |  289 | `		if( pE == 0 ){` |
|      ! 0 |  290 | `			return OVT_SKIP; /* not loaded / forward ref / namespaced — accept */` |
|        - |  291 | `		}` |
|       14 |  292 | `		*ppClass = (ph7_class *)pE->pUserData;` |
|       14 |  293 | `		return OVT_CLASS;` |
|        - |  294 | `	}` |
|     9190 |  295 | `	if( nType == MEMOBJ_STRING \|\| nType == MEMOBJ_INT \|\| nType == MEMOBJ_REAL` |
|       47 |  296 | `	 \|\| nType == MEMOBJ_BOOL \|\| nType == MEMOBJ_HASHMAP ){` |
|     9155 |  297 | `		return OVT_SCALAR;` |
|        - |  298 | `	}` |
|        - |  299 | `	/* MEMOBJ_OBJ (object — subtypes against classes), MEMOBJ_VOID/NULL/RES,` |
|        - |  300 | `	 * or anything unexpected: skip. */` |
|       43 |  301 | `	return OVT_SKIP;` |
|   163147 |  302 | `}` |
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
|   244716 |  316 | `static OvType OoTypeFromReturn(ph7_vm_func *pF)` |
|        5 |  317 | `{` |
|        - |  318 | `	OvType t;` |
|   244721 |  319 | `	t.nType = pF->nReturnType;` |
|   244721 |  320 | `	t.pClass = &pF->sReturnClass;` |
|   244721 |  321 | `	t.bUnion = SySetUsed(&pF->aReturnUnion) > 0;` |
|   244721 |  322 | `	t.bNullable = (pF->iFlags & VM_FUNC_RETURN_NULLABLE) != 0;` |
|   244721 |  323 | `	return t;` |
|        5 |  324 | `}` |
|    81568 |  325 | `static OvType OoTypeFromArg(ph7_vm_func_arg *pA)` |
|        5 |  326 | `{` |
|        - |  327 | `	OvType t;` |
|    81573 |  328 | `	t.nType = pA->nType;` |
|    81573 |  329 | `	t.pClass = &pA->sClass;` |
|    81573 |  330 | `	t.bUnion = (pA->iFlags & VM_FUNC_ARG_UNION) != 0;` |
|    81573 |  331 | `	t.bNullable = (pA->iFlags & VM_FUNC_ARG_NULLABLE) != 0;` |
|    81573 |  332 | `	return t;` |
|        5 |  333 | `}` |
|        - |  334 | `/*` |
|        - |  335 | ` * Return TRUE if the child type is an unambiguously-invalid override of the` |
|        - |  336 | ` * parent type. bCovariant=1 for a return type (child must be ⊆ parent),` |
|        - |  337 | ` * 0 for a parameter (child must be ⊇ parent). Returns FALSE (accept) on any` |
|        - |  338 | ` * skipped/ambiguous shape.` |
|        - |  339 | ` */` |
|   163142 |  340 | `static int OoOverrideTypeBad(ph7_vm *pVm, OvType parent, OvType child, int bCovariant)` |
|        5 |  341 | `{` |
|        - |  342 | `	ph7_class *pParentCls, *pChildCls;` |
|   163147 |  343 | `	int kP = OoClassifyOverrideType(pVm, parent.nType, parent.pClass, parent.bUnion, &pParentCls);` |
|   163147 |  344 | `	int kC = OoClassifyOverrideType(pVm, child.nType, child.pClass, child.bUnion, &pChildCls);` |
|   163147 |  345 | `	if( kP == OVT_SKIP \|\| kC == OVT_SKIP ){` |
|       29 |  346 | `		return 0; /* ambiguous shape — conservatively accept */` |
|        - |  347 | `	}` |
|        - |  348 | `	/* A missing type is the TOP type. covariant (return): a concrete child is a` |
|        - |  349 | `	 * subtype of top, fine; a top child over a concrete parent WIDENS → bad.` |
|        - |  350 | `	 * contravariant (param): a top child is a supertype of anything, fine; a` |
|        - |  351 | `	 * concrete child over a top parent NARROWS → bad. (A union/intersection child` |
|        - |  352 | `	 * already fell into OVT_SKIP above, so a flagged child here is scalar/class.) */` |
|   163121 |  353 | `	if( kP == OVT_NONE \|\| kC == OVT_NONE ){` |
|   158545 |  354 | `		if( bCovariant && kC == OVT_NONE && kP != OVT_NONE ) return 1;` |
|   158545 |  355 | `		if( !bCovariant && kP == OVT_NONE && kC != OVT_NONE ) return 1;` |
|   158545 |  356 | `		return 0;` |
|        - |  357 | `	}` |
|        - |  358 | `	/* Nullability: a covariant return may not ADD null; a contravariant param may` |
|        - |  359 | `	 * not REMOVE null. */` |
|     4581 |  360 | `	if( bCovariant ){` |
|     4561 |  361 | `		if( child.bNullable && !parent.bNullable ) return 1;` |
|     2283 |  362 | `	}else{` |
|       23 |  363 | `		if( parent.bNullable && !child.bNullable ) return 1;` |
|        - |  364 | `	}` |
|     4581 |  365 | `	if( kP == OVT_SCALAR && kC == OVT_SCALAR ){` |
|        - |  366 | `		/* Scalars are invariant — they must match exactly. */` |
|     4575 |  367 | `		return (parent.nType != child.nType) ? 1 : 0;` |
|        - |  368 | `	}` |
|        8 |  369 | `	if( kP == OVT_CLASS && kC == OVT_CLASS ){` |
|        8 |  370 | `		if( bCovariant ){` |
|        3 |  371 | `			return PH7_VmInstanceOf(pChildCls, pParentCls) ? 0 : 1;  /* child ⊆ parent */` |
|        - |  372 | `		}` |
|        6 |  373 | `		return PH7_VmInstanceOf(pParentCls, pChildCls) ? 0 : 1;      /* child ⊇ parent */` |
|        - |  374 | `	}` |
|        - |  375 | `	/* One scalar and one class — disjoint. */` |
|      ! 0 |  376 | `	return 1;` |
|    81576 |  377 | `}` |
|        - |  378 |  |
|        - |  379 | `/*` |
|        - |  380 | ` * Check a child method's signature against the parent method it overrides.` |
|        - |  381 | ` * Emits a PHP-style "Declaration of … must be compatible …" fatal on a clear` |
|        - |  382 | `` * incompatibility. `__construct` is exempt (PHP does not apply variance to it).`` |
|        - |  383 | ` */` |
|   172174 |  384 | `static sxi32 OoCheckOverrideCompat(ph7_gen_state *pGen, ph7_class *pBase, ph7_class *pSub,` |
|        - |  385 | `	ph7_class_method *pParent, ph7_class_method *pChild)` |
|        5 |  386 | `{` |
|   172179 |  387 | `	ph7_vm *pVm = pGen->pVm;` |
|   172179 |  388 | `	ph7_vm_func *pPF = &pParent->sFunc;` |
|   172179 |  389 | `	ph7_vm_func *pCF = &pChild->sFunc;` |
|   172179 |  390 | `	SyString *pMName = &pCF->sName;` |
|        - |  391 | `	ph7_vm_func_arg *aP, *aC;` |
|        - |  392 | `	sxu32 nPArg, nCArg, k;` |
|   172179 |  393 | `	int bBad = 0;` |
|   172174 |  394 | `	if( pMName->nByte == sizeof("__construct")-1` |
|   113268 |  395 | `	 && SyStrnmicmp(pMName->zString,"__construct",pMName->nByte) == 0 ){` |
|    49821 |  396 | `		return SXRET_OK;` |
|        - |  397 | `	}` |
|        - |  398 | `	/* Return type — covariant. */` |
|   122363 |  399 | `	bBad = OoOverrideTypeBad(pVm, OoTypeFromReturn(pPF), OoTypeFromReturn(pCF), /* bCovariant */ 1);` |
|        - |  400 | `	/* Each overlapping parameter — contravariant. */` |
|   122363 |  401 | `	nPArg = SySetUsed(&pPF->aArgs);` |
|   122363 |  402 | `	nCArg = SySetUsed(&pCF->aArgs);` |
|   122363 |  403 | `	aP = (ph7_vm_func_arg *)SySetBasePtr(&pPF->aArgs);` |
|   122363 |  404 | `	aC = (ph7_vm_func_arg *)SySetBasePtr(&pCF->aArgs);` |
|   163147 |  405 | `	for( k = 0; !bBad && k < nPArg && k < nCArg; k++ ){` |
|    40789 |  406 | `		bBad = OoOverrideTypeBad(pVm, OoTypeFromArg(&aP[k]), OoTypeFromArg(&aC[k]), /* bCovariant */ 0);` |
|    20397 |  407 | `	}` |
|        - |  408 | `	/* Parameter arity: the child must declare at least the parent's parameters and` |
|        - |  409 | `	 * may add only OPTIONAL ones — PHP rejects dropping any param (even an optional` |
|        - |  410 | `	 * one) or adding a required one. Skip the rule if either signature is variadic` |
|        - |  411 | `	 * (arity semantics differ). */` |
|   122363 |  412 | `	if( !bBad ){` |
|   122359 |  413 | `		int bVariadic = 0;` |
|   163141 |  414 | `		for( k = 0; k < nPArg; k++ ){ if( aP[k].iFlags & VM_FUNC_ARG_VARIADIC ) bVariadic = 1; }` |
|   163143 |  415 | `		for( k = 0; k < nCArg; k++ ){ if( aC[k].iFlags & VM_FUNC_ARG_VARIADIC ) bVariadic = 1; }` |
|   122359 |  416 | `		if( !bVariadic ){` |
|   122359 |  417 | `			if( nCArg < nPArg ){` |
|      ! 0 |  418 | `				bBad = 1; /* dropped a parent parameter */` |
|      ! 0 |  419 | `			}else{` |
|   122361 |  420 | `				for( k = nPArg; k < nCArg; k++ ){` |
|        3 |  421 | `					if( SySetUsed(&aC[k].aByteCode) == 0 ){ bBad = 1; break; } /* new required */` |
|        2 |  422 | `				}` |
|        - |  423 | `			}` |
|    61177 |  424 | `		}` |
|    61177 |  425 | `	}` |
|   122363 |  426 | `	if( bBad ){` |
|        8 |  427 | `		sxi32 rc = PH7_GenCompileError(&(*pGen),E_ERROR,pChild->nLine,` |
|        - |  428 | `			"Declaration of %z::%z() must be compatible with %z::%z()",` |
|        2 |  429 | `			&pSub->sName,pMName,&pBase->sName,&pParent->sFunc.sName);` |
|        6 |  430 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  431 | `			return SXERR_ABORT;` |
|        - |  432 | `		}` |
|        2 |  433 | `	}` |
|   122363 |  434 | `	return SXRET_OK;` |
|    86092 |  435 | `}` |
|        - |  436 | `/*` |
|        - |  437 | ` * Perform an inheritance operation.` |
|        - |  438 | ` * According to the PHP language reference manual` |
|        - |  439 | ` *  When you extend a class, the subclass inherits all of the public and protected methods` |
|        - |  440 | ` *  from the parent class. Unless a class Overwrites those methods, they will retain their original` |
|        - |  441 | ` *  functionality.` |
|        - |  442 | ` *  This is useful for defining and abstracting functionality, and permits the implementation` |
|        - |  443 | ` *  of additional functionality in similar objects without the need to reimplement all of the shared` |
|        - |  444 | ` *  functionality.` |
|        - |  445 | ` *  Example #1 Inheritance Example` |
|        - |  446 | ` * <?php` |
|        - |  447 | ` * class foo` |
|        - |  448 | ` * {` |
|        - |  449 | ` *   public function printItem($string)` |
|        - |  450 | ` *   {` |
|        - |  451 | ` *       echo 'Foo: ' . $string . PHP_EOL;` |
|        - |  452 | ` *   }` |
|        - |  453 | ` *` |
|        - |  454 | ` *   public function printPHP()` |
|        - |  455 | ` *   {` |
|        - |  456 | ` *       echo 'PHP is great.' . PHP_EOL;` |
|        - |  457 | ` *   }` |
|        - |  458 | ` * }` |
|        - |  459 | ` * class bar extends foo` |
|        - |  460 | ` * {` |
|        - |  461 | ` *   public function printItem($string)` |
|        - |  462 | ` *   {` |
|        - |  463 | ` *       echo 'Bar: ' . $string . PHP_EOL;` |
|        - |  464 | ` *   }` |
|        - |  465 | ` * }` |
|        - |  466 | ` * $foo = new foo();` |
|        - |  467 | ` * $bar = new bar();` |
|        - |  468 | ` * $foo->printItem('baz'); // Output: 'Foo: baz'` |
|        - |  469 | ` * $foo->printPHP();       // Output: 'PHP is great'` |
|        - |  470 | ` * $bar->printItem('baz'); // Output: 'Bar: baz'` |
|        - |  471 | ` * $bar->printPHP();       // Output: 'PHP is great'` |
|        - |  472 | ` *` |
|        - |  473 | ` * This function return SXRET_OK if the inheritance operation was successfully performed.` |
|        - |  474 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|        - |  475 | ` * error message.` |
|        - |  476 | ` */` |
|   290192 |  477 | `PH7_PRIVATE sxi32 PH7_ClassInherit(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase)` |
|        5 |  478 | `{` |
|        - |  479 | `	ph7_class_method *pMeth;` |
|        - |  480 | `	ph7_class_attr *pAttr;` |
|        - |  481 | `	SyHashEntry *pEntry;` |
|        - |  482 | `	SyString *pName;` |
|        - |  483 | `	SySet aInherited; /* base attributes to prepend (see the copy loop below) */` |
|        - |  484 | `	sxi32 rc;` |
|   290197 |  485 | `	SySetInit(&aInherited,&pGen->pVm->sAllocator,sizeof(ph7_class_attr *));` |
|        - |  486 | `	/* Install in the derived hashtable */` |
|   290197 |  487 | `	rc = SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|   290197 |  488 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  489 | `		SySetRelease(&aInherited);` |
|      ! 0 |  490 | `		return rc;` |
|        - |  491 | `	}` |
|        - |  492 | `	/* readonly class inheritance (PHP 8.2): a readonly class may only extend a` |
|        - |  493 | `	 * readonly class, and a non-readonly class may not extend a readonly one. */` |
|   290197 |  494 | `	if( (pBase->iFlags & PH7_CLASS_READONLY) != (pSub->iFlags & PH7_CLASS_READONLY) ){` |
|        5 |  495 | `		if( pBase->iFlags & PH7_CLASS_READONLY ){` |
|        4 |  496 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|        - |  497 | `				"Non-readonly class %z cannot extend readonly class %z",` |
|        1 |  498 | `				&pSub->sName,&pBase->sName);` |
|        2 |  499 | `		}else{` |
|        4 |  500 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|        - |  501 | `				"Readonly class %z cannot extend non-readonly class %z",` |
|        1 |  502 | `				&pSub->sName,&pBase->sName);` |
|        - |  503 | `		}` |
|        5 |  504 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  505 | `			SySetRelease(&aInherited);` |
|      ! 0 |  506 | `			return SXERR_ABORT;` |
|        - |  507 | `		}` |
|        2 |  508 | `	}` |
|        - |  509 | `	/* Copy public/protected attributes from the base class */` |
|   290197 |  510 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
|  1468091 |  511 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|        - |  512 | `		/* Make sure the private attributes are not redeclared in the subclass */` |
|  1177899 |  513 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  1177899 |  514 | `		pName = &pAttr->sName;` |
|  1177899 |  515 | `		if( (pEntry = SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|       22 |  516 | `			if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_FINAL))` |
|       15 |  517 | `				== (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_FINAL) ){` |
|        - |  518 | `				/* Cannot override a final class constant (PHP 8.1). Report the` |
|        - |  519 | `				 * class that originally declared it (pDeclClass) rather than the` |
|        - |  520 | `				 * immediate base, so a multi-level chain matches PHP. */` |
|      ! 0 |  521 | `				ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pBase;` |
|      ! 0 |  522 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_attr *)pEntry->pUserData)->nLine,` |
|        - |  523 | `					"%z::%z cannot override final constant %z::%z",` |
|      ! 0 |  524 | `					&pSub->sName,pName,&pOwner->sName,pName);` |
|      ! 0 |  525 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  526 | `					SySetRelease(&aInherited);` |
|      ! 0 |  527 | `					return SXERR_ABORT;` |
|        - |  528 | `				}` |
|      ! 0 |  529 | `			}` |
|        - |  530 | `			/* A child MAY redeclare a base's private property: php treats the two` |
|        - |  531 | `			 * as independent members (each private to its declaring class), with no` |
|        - |  532 | `			 * diagnostic. PH7 warned here, which is wrong — the child's entry simply` |
|        - |  533 | `			 * shadows the base's in the by-name attribute table.` |
|        - |  534 | `			 *` |
|        - |  535 | `			 * Ordering: php keeps an overridden INSTANCE property at the position` |
|        - |  536 | `` 			 * the BASE declared it (`class G{$g1;$g2;} class H extends G{$h;$g1;}` `` |
|        - |  537 | `			 * iterates g1,g2,h — not g2,h,g1). Re-collect the CHILD's definition,` |
|        - |  538 | `			 * whose default value wins, and drop its current entry so the prepend` |
|        - |  539 | `			 * below re-inserts it in base order. Statics/constants are not part of` |
|        - |  540 | `			 * instance iteration, so they keep their existing slot. */` |
|       26 |  541 | `			if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|       26 |  542 | `				ph7_class_attr *pOwn = (ph7_class_attr *)pEntry->pUserData;` |
|       26 |  543 | `				SyHashDeleteEntry(&pSub->hAttr,(const void *)pName->zString,pName->nByte,0);` |
|       26 |  544 | `				rc = SySetPut(&aInherited,(const void *)&pOwn);` |
|       26 |  545 | `				if( rc != SXRET_OK ){` |
|      ! 0 |  546 | `					SySetRelease(&aInherited);` |
|      ! 0 |  547 | `					return rc;` |
|        - |  548 | `				}` |
|       11 |  549 | `			}` |
|       26 |  550 | `			continue;` |
|        - |  551 | `		}` |
|        - |  552 | `		/* Collect the attribute. php: a base class's private INSTANCE property` |
|        - |  553 | `		 * lives on every child instance too (its own methods read/write it` |
|        - |  554 | `		 * through $this on the child; the access check grants private access by` |
|        - |  555 | `		 * DECLARING class, so child methods and outsiders still can't touch it).` |
|        - |  556 | `		 * Private STATICS/CONSTANTS stay uncopied — base methods reach those` |
|        - |  557 | `		 * through self:: against the declaring class directly.` |
|        - |  558 | `		 *` |
|        - |  559 | `		 * These are gathered rather than installed here because php orders an` |
|        - |  560 | `		 * instance's properties BASE-DECLARED FIRST, then the subclass's own,` |
|        - |  561 | `		 * then trait members — while inheritance runs AFTER the subclass body` |
|        - |  562 | `		 * has already filled hAttr. They are prepended below. */` |
|  1177872 |  563 | `		if( pAttr->iProtection != PH7_CLASS_PROT_PRIVATE` |
|   776939 |  564 | `		 \|\| (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|  1177875 |  565 | `			rc = SySetPut(&aInherited,(const void *)&pAttr);` |
|  1177875 |  566 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  567 | `				SySetRelease(&aInherited);` |
|      ! 0 |  568 | `				return rc;` |
|        - |  569 | `			}` |
|   588935 |  570 | `		}` |
|        5 |  571 | `	}` |
|        - |  572 | `	/* Prepend the collected base attributes so hAttr reads base-first. The` |
|        - |  573 | `	 * table's iteration list is what every object-iteration consumer walks` |
|        - |  574 | `	 * (var_dump/print_r, get_object_vars, foreach, (array) casts, json_encode),` |
|        - |  575 | `	 * so this ordering is user-visible — json_encode emits its keys in exactly` |
|        - |  576 | `	 * this order. SyHashInsert is a HEAD insert, so walking the collected set` |
|        - |  577 | `	 * backwards leaves the base's own declaration order at the front. */` |
|   290197 |  578 | `	if( SySetUsed(&aInherited) > 0 ){` |
|   289983 |  579 | `		ph7_class_attr **apInherited = (ph7_class_attr **)SySetBasePtr(&aInherited);` |
|   289983 |  580 | `		sxu32 n = SySetUsed(&aInherited);` |
|  1467875 |  581 | `		while( n > 0 ){` |
|  1177897 |  582 | `			ph7_class_attr *pIn = apInherited[--n];` |
|  1177897 |  583 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pIn->sName.zString,pIn->sName.nByte,pIn);` |
|  1177897 |  584 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  585 | `				SySetRelease(&aInherited);` |
|      ! 0 |  586 | `				return rc;` |
|        - |  587 | `			}` |
|        5 |  588 | `		}` |
|   144989 |  589 | `	}` |
|        - |  590 | `	/* Inherit the base's CONSTANTS (separate namespace: hConst). Constants are not` |
|        - |  591 | `	 * part of instance iteration, so no base-first ordering dance is needed — a plain` |
|        - |  592 | `	 * copy of every constant the subclass did not itself redeclare. A subclass that` |
|        - |  593 | `	 * redeclares a base FINAL constant is a fatal (PHP 8.1). */` |
|   290197 |  594 | `	SyHashResetLoopCursor(&pBase->hConst);` |
|   480441 |  595 | `	while((pEntry = SyHashGetNextEntry(&pBase->hConst)) != 0 ){` |
|        - |  596 | `		SyHashEntry *pOwn;` |
|   190249 |  597 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   190249 |  598 | `		pName = &pAttr->sName;` |
|   190249 |  599 | `		if( (pOwn = SyHashGet(&pSub->hConst,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|        3 |  600 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_FINAL) ){` |
|        - |  601 | `				/* Cannot override a final class constant. Report the class that` |
|        - |  602 | `				 * originally declared it (pDeclClass) for a multi-level chain. */` |
|        3 |  603 | `				ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pBase;` |
|        4 |  604 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_attr *)pOwn->pUserData)->nLine,` |
|        - |  605 | `					"%z::%z cannot override final constant %z::%z",` |
|        1 |  606 | `					&pSub->sName,pName,&pOwner->sName,pName);` |
|        3 |  607 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  608 | `					SySetRelease(&aInherited);` |
|      ! 0 |  609 | `					return SXERR_ABORT;` |
|        - |  610 | `				}` |
|        1 |  611 | `			}` |
|        3 |  612 | `			continue;` |
|        - |  613 | `		}` |
|   190247 |  614 | `		rc = SyHashInsertTail(&pSub->hConst,(const void *)pName->zString,pName->nByte,pAttr);` |
|   190247 |  615 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  616 | `			SySetRelease(&aInherited);` |
|      ! 0 |  617 | `			return rc;` |
|        - |  618 | `		}` |
|        5 |  619 | `	}` |
|   290197 |  620 | `	SySetRelease(&aInherited);` |
|   290197 |  621 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
|  4226255 |  622 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|        - |  623 | `		/* Make sure the private/final methods are not redeclared in the subclass */` |
|  3936063 |  624 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  3936063 |  625 | `		pName = &pMeth->sFunc.sName;` |
|  3936063 |  626 | `		if( (pEntry = SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|   172183 |  627 | `			 if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|        - |  628 | `				/* php: "Cannot override final method A::test()" */` |
|        8 |  629 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_method *)pEntry->pUserData)->nLine,` |
|        - |  630 | `					"Cannot override final method %z::%z()",` |
|        2 |  631 | `					&pBase->sName,pName);` |
|        2 |  632 | `				(void)pSub;` |
|        6 |  633 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  634 | `					return SXERR_ABORT;` |
|        - |  635 | `				}` |
|        4 |  636 | `			}else{` |
|        - |  637 | `				/* Check the override's signature is compatible with the parent's. */` |
|   258266 |  638 | `				rc = OoCheckOverrideCompat(&(*pGen),pBase,pSub,pMeth,` |
|   172174 |  639 | `					(ph7_class_method *)pEntry->pUserData);` |
|   172179 |  640 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  641 | `					return SXERR_ABORT;` |
|        - |  642 | `				}` |
|        - |  643 | `			}` |
|   172183 |  644 | `			continue;` |
|        - |  645 | `		}` |
|        - |  646 | `		/* Install the method. php: a base class's private INSTANCE method is` |
|        - |  647 | `		 * dispatchable on child instances too — an inherited public method` |
|        - |  648 | `		 * calling $this->priv() must find it (the call-site visibility check` |
|        - |  649 | `		 * binds by DECLARING class, sFunc.pUserData, so child code and` |
|        - |  650 | `		 * outsiders still can't call it; a private ctor copied down also` |
|        - |  651 | ``		 * blocks `new Child` from outside like php). Private STATICS stay`` |
|        - |  652 | `		 * uncopied — base methods reach those through self:: against the` |
|        - |  653 | `		 * declaring class directly. */` |
|  3763880 |  654 | `		if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE` |
|  1900069 |  655 | `		 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
|  3763885 |  656 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|  3763885 |  657 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  658 | `				return rc;` |
|        - |  659 | `			}` |
|  1881940 |  660 | `		}` |
|        5 |  661 | `	}` |
|        - |  662 | `	/* Mark as subclass */` |
|   290197 |  663 | `	pSub->pBase = pBase;` |
|        - |  664 | `	/* All done */` |
|   290197 |  665 | `	return SXRET_OK;` |
|   145101 |  666 | `}` |
|        - |  667 | `/*` |
|        - |  668 | ` * Apply a trait to a class: copy all methods and attributes from the trait` |
|        - |  669 | ` * into the target class. Unlike inheritance, traits copy ALL members including` |
|        - |  670 | ` * private ones. Members already defined in the class take precedence.` |
|        - |  671 | ` */` |
|    18246 |  672 | `PH7_PRIVATE sxi32 PH7_ClassUseTrait(ph7_gen_state *pGen,ph7_class *pClass,ph7_class *pTrait)` |
|        5 |  673 | `{` |
|        - |  674 | `	ph7_class_method *pMeth;` |
|        - |  675 | `	ph7_class_attr *pAttr;` |
|        - |  676 | `	SyHashEntry *pEntry;` |
|        - |  677 | `	SyString *pName;` |
|        - |  678 | `	sxi32 rc;` |
|        - |  679 | `	/* Detect cyclic trait composition (e.g. trait A { use B; } trait B { use A; }) */` |
|    18251 |  680 | `	if( pTrait->iFlags & PH7_CLASS_TRAIT_VISITING ){` |
|      ! 0 |  681 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|      ! 0 |  682 | `			"Trait circular reference detected: %z is already being applied",&pTrait->sName);` |
|      ! 0 |  683 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  684 | `			return SXERR_ABORT;` |
|        - |  685 | `		}` |
|      ! 0 |  686 | `		return SXRET_OK;` |
|        - |  687 | `	}` |
|    18251 |  688 | `	pTrait->iFlags \|= PH7_CLASS_TRAIT_VISITING;` |
|    18251 |  689 | `	rc = SXRET_OK;` |
|        - |  690 | `	/* Copy attributes from the trait */` |
|    18251 |  691 | `	SyHashResetLoopCursor(&pTrait->hAttr);` |
|    72615 |  692 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hAttr)) != 0 ){` |
|        - |  693 | `		SyHashEntry *pExisting;` |
|    54369 |  694 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|    54369 |  695 | `		pName = &pAttr->sName;` |
|    54369 |  696 | `		pExisting = SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte);` |
|    54369 |  697 | `		if( pExisting != 0 ){` |
|        - |  698 | `			/* Attribute already exists. Check if it came from another trait` |
|        - |  699 | `			 * and whether the definitions are compatible (same defaults).` |
|        - |  700 | `			 */` |
|        - |  701 | `			ph7_class **apUsedTraits;` |
|        - |  702 | `			sxu32 nUsed,k;` |
|        6 |  703 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|        6 |  704 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|        6 |  705 | `			for(k = 0; k < nUsed; k++){` |
|        - |  706 | `				ph7_class_attr *pOther;` |
|        3 |  707 | `				pOther = PH7_ClassExtractAttribute(apUsedTraits[k],pName->zString,pName->nByte);` |
|        3 |  708 | `				if( pOther ){` |
|        - |  709 | `					/* Two traits define the same property — check if defaults differ */` |
|        3 |  710 | `					ph7_class_attr *pClassAttr = (ph7_class_attr *)pExisting->pUserData;` |
|        4 |  711 | `					if( SySetUsed(&pAttr->aByteCode) != SySetUsed(&pClassAttr->aByteCode) \|\|` |
|        3 |  712 | `						(SySetUsed(&pAttr->aByteCode) > 0 &&` |
|        3 |  713 | `						 SyMemcmp(SySetBasePtr(&pAttr->aByteCode),SySetBasePtr(&pClassAttr->aByteCode),` |
|        3 |  714 | `							SySetUsed(&pAttr->aByteCode) * SySetElemSize(&pAttr->aByteCode)) != 0) ){` |
|        4 |  715 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine,` |
|        - |  716 | `							"%z and %z define the same property ($%z) in the composition of %z. "` |
|        - |  717 | `							"However, the definition differs and is considered incompatible",` |
|        2 |  718 | `							&apUsedTraits[k]->sName,&pTrait->sName,pName,&pClass->sName);` |
|        3 |  719 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 |  720 | `							goto cleanup;` |
|        - |  721 | `						}` |
|        1 |  722 | `					}` |
|        3 |  723 | `					break;` |
|        - |  724 | `				}` |
|      ! 0 |  725 | `			}` |
|        6 |  726 | `			continue;` |
|        - |  727 | `		}` |
|    54365 |  728 | `		rc = SyHashInsertTail(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|    54365 |  729 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  730 | `			goto cleanup;` |
|        - |  731 | `		}` |
|        5 |  732 | `	}` |
|        - |  733 | `	/* Copy constants from the trait (PHP 8.2 trait constants; separate hConst` |
|        - |  734 | `	 * namespace). A constant already present in the class wins silently. */` |
|    18251 |  735 | `	SyHashResetLoopCursor(&pTrait->hConst);` |
|    18251 |  736 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hConst)) != 0 ){` |
|      ! 0 |  737 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      ! 0 |  738 | `		pName = &pAttr->sName;` |
|      ! 0 |  739 | `		if( SyHashGet(&pClass->hConst,(const void *)pName->zString,pName->nByte) != 0 ){` |
|      ! 0 |  740 | `			continue;` |
|        - |  741 | `		}` |
|      ! 0 |  742 | `		rc = SyHashInsertTail(&pClass->hConst,(const void *)pName->zString,pName->nByte,pAttr);` |
|      ! 0 |  743 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  744 | `			goto cleanup;` |
|        - |  745 | `		}` |
|      ! 0 |  746 | `	}` |
|        - |  747 | `	/* Copy methods from the trait. The identity copied is the trait's HASH KEY,` |
|        - |  748 | ``	 * not sFunc.sName: an adaptation alias (`hi as bHi`) is a hash entry whose`` |
|        - |  749 | `	 * key is the alias while the method struct keeps its original name — keying` |
|        - |  750 | ``	 * the copy off sFunc.sName silently re-filed `bHi` under `hi`, so an alias`` |
|        - |  751 | `	 * made inside a TRAIT vanished when that trait was composed into a class. */` |
|    18251 |  752 | `	SyHashResetLoopCursor(&pTrait->hMethod);` |
|   262959 |  753 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hMethod)) != 0 ){` |
|        - |  754 | `		SyHashEntry *pClassMethEntry;` |
|        - |  755 | `		SyString sKey;` |
|   244713 |  756 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   244713 |  757 | `		SyStringInitFromBuf(&sKey,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|   244713 |  758 | `		pName = &sKey;` |
|   244713 |  759 | `		pClassMethEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
|   244713 |  760 | `		if( pClassMethEntry != 0 ){` |
|        - |  761 | `			/* Method already exists in the class. An ABSTRACT trait method is a` |
|        - |  762 | `			 * REQUIREMENT, not an implementation: php satisfies it with any concrete` |
|        - |  763 | `			 * method of the same name (from the class body or another trait) — no` |
|        - |  764 | `			 * collision. Only two CONCRETE trait methods actually conflict. */` |
|       18 |  765 | `			ph7_class_method *pExistingMeth = (ph7_class_method *)pClassMethEntry->pUserData;` |
|       18 |  766 | `			int bIncomingAbstract = (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0;` |
|       18 |  767 | `			int bExistingAbstract = (pExistingMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0;` |
|        - |  768 | `			ph7_class **apUsedTraits;` |
|        - |  769 | `			sxu32 nUsed,k;` |
|       18 |  770 | `			if( bIncomingAbstract ){` |
|        - |  771 | `				/* Incoming abstract requirement: the existing (concrete or abstract)` |
|        - |  772 | `				 * method already covers this name — keep it. */` |
|       12 |  773 | `				continue;` |
|        - |  774 | `			}` |
|       11 |  775 | `			if( bExistingAbstract ){` |
|        - |  776 | `				/* Existing entry is only an abstract requirement (from an earlier` |
|        - |  777 | `				 * trait): the incoming concrete method satisfies and replaces it. */` |
|        3 |  778 | `				pClassMethEntry->pUserData = (void *)pMeth;` |
|        3 |  779 | `				continue;` |
|        - |  780 | `			}` |
|        - |  781 | `			/* Both concrete: a genuine collision only when the OTHER definition came` |
|        - |  782 | `			 * from another trait (a concrete one). A class-body method wins silently. */` |
|        8 |  783 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|        8 |  784 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|        8 |  785 | `			for(k = 0; k < nUsed; k++){` |
|        3 |  786 | `				ph7_class_method *pOtherMeth = PH7_ClassExtractMethod(apUsedTraits[k],pName->zString,pName->nByte);` |
|        3 |  787 | `				if( pOtherMeth != 0 && (pOtherMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|        - |  788 | `					/* Two different traits define the same CONCRETE method with no resolution */` |
|        4 |  789 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|        - |  790 | `						"Trait method %z::%z has not been applied as %z::%z, "` |
|        - |  791 | `						"because of collision with %z::%z",` |
|        2 |  792 | `						&pTrait->sName,pName,` |
|        1 |  793 | `						&pClass->sName,pName,` |
|        2 |  794 | `						&apUsedTraits[k]->sName,pName);` |
|        3 |  795 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  796 | `						goto cleanup;` |
|        - |  797 | `					}` |
|        3 |  798 | `					break;` |
|        - |  799 | `				}` |
|      ! 0 |  800 | `			}` |
|        - |  801 | `			/* Class-defined method takes precedence */` |
|        8 |  802 | `			continue;` |
|        - |  803 | `		}` |
|   244699 |  804 | `		rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|   244699 |  805 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  806 | `			goto cleanup;` |
|        - |  807 | `		}` |
|        5 |  808 | `	}` |
|        - |  809 | `	/* Record trait in the class */` |
|    18251 |  810 | `	SySetPut(&pClass->aTrait,(const void *)&pTrait);` |
|     9123 |  811 | `cleanup:` |
|        - |  812 | `	/* Always clear visiting flag, even on error paths */` |
|    18251 |  813 | `	pTrait->iFlags &= ~PH7_CLASS_TRAIT_VISITING;` |
|     9123 |  814 | `	SXUNUSED(pGen);` |
|    18251 |  815 | `	return rc;` |
|     9128 |  816 | `}` |
|        - |  817 | `/*` |
|        - |  818 | ` * Inherit an object interface from another object interface.` |
|        - |  819 | ` * According to the PHP language reference manual.` |
|        - |  820 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|        - |  821 | ` *  must implement, without having to define how these methods are handled.` |
|        - |  822 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|        - |  823 | ` *  class, but without any of the methods having their contents defined.` |
|        - |  824 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|        - |  825 | ` *` |
|        - |  826 | ` * This function return SXRET_OK if the interface inheritance operation was successfully performed.` |
|        - |  827 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|        - |  828 | ` * error message.` |
|        - |  829 | ` */` |
|    31712 |  830 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceInherit(ph7_class *pSub,ph7_class *pBase)` |
|        5 |  831 | `{` |
|        - |  832 | `	ph7_class_method *pMeth;` |
|        - |  833 | `	ph7_class_attr *pAttr;` |
|        - |  834 | `	SyHashEntry *pEntry;` |
|        - |  835 | `	SyString *pName;` |
|        - |  836 | `	sxi32 rc;` |
|        - |  837 | `	/* Install in the derived hashtable */` |
|    31717 |  838 | `	SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|    31717 |  839 | `	SyHashResetLoopCursor(&pBase->hConst);` |
|        - |  840 | `	/* Copy constants (interfaces carry only constants + method signatures) */` |
|    47575 |  841 | `	while((pEntry = SyHashGetNextEntry(&pBase->hConst)) != 0 ){` |
|        - |  842 | `		/* Make sure the constants are not redeclared in the subclass */` |
|        3 |  843 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|        3 |  844 | `		pName = &pAttr->sName;` |
|        3 |  845 | `		if( SyHashGet(&pSub->hConst,(const void *)pName->zString,pName->nByte) == 0 ){` |
|        - |  846 | `			/* Install the constant in the subclass */` |
|        3 |  847 | `			rc = SyHashInsertTail(&pSub->hConst,(const void *)pName->zString,pName->nByte,pAttr);` |
|        3 |  848 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  849 | `				return rc;` |
|        - |  850 | `			}` |
|        1 |  851 | `		}` |
|        1 |  852 | `	}` |
|    31717 |  853 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
|        - |  854 | `	/* Copy methods signature */` |
|   124595 |  855 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|        - |  856 | `		/* Make sure the method are not redeclared in the subclass */` |
|    77027 |  857 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|    77027 |  858 | `		pName = &pMeth->sFunc.sName;` |
|    77027 |  859 | `		if( SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte) == 0 ){` |
|        - |  860 | `			/* Install the method */` |
|    77027 |  861 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|    77027 |  862 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  863 | `				return rc;` |
|        - |  864 | `			}` |
|    38511 |  865 | `		}` |
|        5 |  866 | `	}` |
|        - |  867 | `	/* Mark as subclass */` |
|    31717 |  868 | `	pSub->pBase = pBase;` |
|        - |  869 | `	/* All done */` |
|    31717 |  870 | `	return SXRET_OK;` |
|    15861 |  871 | `}` |
|        - |  872 | `/*` |
|        - |  873 | ` * Implements an object interface in the given main class.` |
|        - |  874 | ` * According to the PHP language reference manual.` |
|        - |  875 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|        - |  876 | ` *  must implement, without having to define how these methods are handled.` |
|        - |  877 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|        - |  878 | ` *  class, but without any of the methods having their contents defined.` |
|        - |  879 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|        - |  880 | ` *` |
|        - |  881 | ` * This function return SXRET_OK if the interface was successfully implemented.` |
|        - |  882 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|        - |  883 | ` * error message.` |
|        - |  884 | ` */` |
|   494050 |  885 | `PH7_PRIVATE sxi32 PH7_ClassImplement(ph7_class *pMain,ph7_class *pInterface)` |
|        5 |  886 | `{` |
|        - |  887 | `	ph7_class_attr *pAttr;` |
|        - |  888 | `	SyHashEntry *pEntry;` |
|        - |  889 | `	SyString *pName;` |
|        - |  890 | `	sxi32 rc;` |
|        - |  891 | `	/* First off,copy all constants declared inside the interface (hConst namespace) */` |
|   494055 |  892 | `	SyHashResetLoopCursor(&pInterface->hConst);` |
|   867882 |  893 | `	while((pEntry = SyHashGetNextEntry(&pInterface->hConst)) != 0 ){` |
|        - |  894 | `		/* Point to the constant declaration */` |
|   126807 |  895 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   126807 |  896 | `		pName = &pAttr->sName;` |
|        - |  897 | `		/* Make sure the constant is not redeclared in the main class */` |
|   126807 |  898 | `		if( SyHashGet(&pMain->hConst,pName->zString,pName->nByte) == 0 ){` |
|        - |  899 | `			/* Install the constant */` |
|   126807 |  900 | `			rc = SyHashInsertTail(&pMain->hConst,pName->zString,pName->nByte,pAttr);` |
|   126807 |  901 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  902 | `				return rc;` |
|        - |  903 | `			}` |
|    63401 |  904 | `		}` |
|        5 |  905 | `	}` |
|        - |  906 | `	/* Install in the interface container */` |
|   494055 |  907 | `	SySetPut(&pMain->aInterface,(const void *)&pInterface);` |
|        - |  908 | `	/* Install interface method stubs into the implementing class.` |
|        - |  909 | `	 * Methods already defined in the class take precedence (they satisfy` |
|        - |  910 | `	 * the interface contract). Stubs retain PH7_CLASS_ATTR_ABSTRACT so` |
|        - |  911 | `	 * the unified check in GenStateCheckAbstractMethods catches missing ones.` |
|        - |  912 | `	 */` |
|        - |  913 | `	{` |
|        - |  914 | `		ph7_class_method *pMeth;` |
|        - |  915 | `		SyHashEntry *pMEntry;` |
|        - |  916 | `		SyString *pMName;` |
|   494055 |  917 | `		SyHashResetLoopCursor(&pInterface->hMethod);` |
|  1701772 |  918 | `		while((pMEntry = SyHashGetNextEntry(&pInterface->hMethod)) != 0 ){` |
|   960697 |  919 | `			pMeth = (ph7_class_method *)pMEntry->pUserData;` |
|   960697 |  920 | `			pMName = &pMeth->sFunc.sName;` |
|   960697 |  921 | `			if( SyHashGet(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte) == 0 ){` |
|       25 |  922 | `				rc = SyHashInsert(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte,pMeth);` |
|       25 |  923 | `				if( rc != SXRET_OK ){` |
|      ! 0 |  924 | `					return rc;` |
|        - |  925 | `				}` |
|       10 |  926 | `			}` |
|        5 |  927 | `		}` |
|        - |  928 | `	}` |
|   494055 |  929 | `	return SXRET_OK;` |
|   247030 |  930 | `}` |
|        - |  931 | `/*` |
|        - |  932 | ` * Create a class instance [i.e: Object in the PHP jargon] at run-time.` |
|        - |  933 | ` * The following function is called when an object is created at run-time` |
|        - |  934 | ` * typically when the PH7_OP_NEW/PH7_OP_CLONE instructions are executed.` |
|        - |  935 | ` * Notes on object creation.` |
|        - |  936 | ` *` |
|        - |  937 | ` * According to PHP language reference manual.` |
|        - |  938 | ` * To create an instance of a class, the new keyword must be used. An object will always` |
|        - |  939 | ` * be created unless the object has a constructor defined that throws an exception on error.` |
|        - |  940 | ` * Classes should be defined before instantiation (and in some cases this is a requirement).` |
|        - |  941 | ` * If a string containing the name of a class is used with new, a new instance of that class` |
|        - |  942 | ` * will be created. If the class is in a namespace, its fully qualified name must be used when` |
|        - |  943 | ` * doing this.` |
|        - |  944 | ` * Example #3 Creating an instance` |
|        - |  945 | ` * <?php` |
|        - |  946 | ` *  $instance = new SimpleClass();` |
|        - |  947 | ` *   // This can also be done with a variable:` |
|        - |  948 | ` * $className = 'Foo';` |
|        - |  949 | ` * $instance = new $className(); // Foo()` |
|        - |  950 | ` * ?>` |
|        - |  951 | ` * In the class context, it is possible to create a new object by new self and new parent.` |
|        - |  952 | ` * When assigning an already created instance of a class to a new variable, the new variable` |
|        - |  953 | ` * will access the same instance as the object that was assigned. This behaviour is the same` |
|        - |  954 | ` * when passing instances to a function. A copy of an already created object can be made by` |
|        - |  955 | ` * cloning it.` |
|        - |  956 | ` * Example #4 Object Assignment` |
|        - |  957 | ` * <?php` |
|        - |  958 | ` *  class SimpleClass(){` |
|        - |  959 | ` *    public $var;` |
|        - |  960 | ` *  };` |
|        - |  961 | ` *  $instance = new SimpleClass();` |
|        - |  962 | ` *  $assigned   =  $instance;` |
|        - |  963 | ` *  $reference  =& $instance;` |
|        - |  964 | ` *  $instance->var = '$assigned will have this value';` |
|        - |  965 | ` *  $instance = null; // $instance and $reference become null` |
|        - |  966 | ` *  var_dump($instance);` |
|        - |  967 | ` *  var_dump($reference);` |
|        - |  968 | ` *  var_dump($assigned);` |
|        - |  969 | ` * ?>` |
|        - |  970 | ` * The above example will output:` |
|        - |  971 | ` * NULL` |
|        - |  972 | ` * NULL` |
|        - |  973 | ` * object(SimpleClass)#1 (1) {` |
|        - |  974 | ` *  ["var"]=>` |
|        - |  975 | ` *    string(30) "$assigned will have this value"` |
|        - |  976 | ` * }` |
|        - |  977 | ` * Example #5 Creating new objects` |
|        - |  978 | ` * <?php` |
|        - |  979 | ` * class Test` |
|        - |  980 | ` * {` |
|        - |  981 | ` *   static public function getNew()` |
|        - |  982 | ` *   {` |
|        - |  983 | ` *       return new static;` |
|        - |  984 | ` *   }` |
|        - |  985 | ` * }` |
|        - |  986 | ` * class Child extends Test` |
|        - |  987 | ` * {}` |
|        - |  988 | ` * $obj1 = new Test();` |
|        - |  989 | ` * $obj2 = new $obj1;` |
|        - |  990 | ` * var_dump($obj1 !== $obj2);` |
|        - |  991 | ` * $obj3 = Test::getNew();` |
|        - |  992 | ` * var_dump($obj3 instanceof Test);` |
|        - |  993 | ` * $obj4 = Child::getNew();` |
|        - |  994 | ` * var_dump($obj4 instanceof Child);` |
|        - |  995 | ` * ?>` |
|        - |  996 | ` * The above example will output:` |
|        - |  997 | ` * bool(true)` |
|        - |  998 | ` * bool(true)` |
|        - |  999 | ` * bool(true)` |
|        - | 1000 | ` * Note that Symisc Systems have introduced powerfull extension to` |
|        - | 1001 | ` * OO subsystem. For example a class attribute may have any complex` |
|        - | 1002 | ` * expression associated with it when declaring the attribute unlike` |
|        - | 1003 | ` * the standard PHP engine which would allow a single value.` |
|        - | 1004 | ` * Example:` |
|        - | 1005 | ` *  class myClass{` |
|        - | 1006 | ` *    public $var = 25<<1+foo()/bar();` |
|        - | 1007 | ` *  };` |
|        - | 1008 | ` * Refer to the official documentation for more information.` |
|        - | 1009 | ` */` |
|  1560104 | 1010 | `static ph7_class_instance * NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|        5 | 1011 | `{` |
|        - | 1012 | `	ph7_class_instance *pThis;` |
|        - | 1013 | `	/* Allocate a new instance */` |
|  1560109 | 1014 | `	pThis = (ph7_class_instance *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_instance));` |
|  1560109 | 1015 | `	if( pThis == 0 ){` |
|      ! 0 | 1016 | `		return 0;` |
|        - | 1017 | `	}` |
|        - | 1018 | `	/* Zero the structure */` |
|  1560109 | 1019 | `	SyZero(pThis,sizeof(ph7_class_instance));` |
|        - | 1020 | `	/* Initialize fields */` |
|  1560109 | 1021 | `	pThis->iRef = 1;` |
|  1560109 | 1022 | `	pThis->pVm = pVm;` |
|  1560109 | 1023 | `	pThis->pClass = pClass;` |
|        - | 1024 | `	/* Assign a fresh monotonic object handle id (clones get their own, like PHP). */` |
|  1560109 | 1025 | `	pThis->nObjId = pVm->nNextObjId++;` |
|  1560109 | 1026 | `	SyHashInit(&pThis->hAttr,&pVm->sAllocator,0,0);` |
|  1560109 | 1027 | `	return pThis;` |
|   780057 | 1028 | `}` |
|        - | 1029 | `/*` |
|        - | 1030 | ` * Wrapper around the NewClassInstance() function defined above.` |
|        - | 1031 | ` * See the block comment above for more information.` |
|        - | 1032 | ` */` |
|  1559842 | 1033 | `PH7_PRIVATE ph7_class_instance * PH7_NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|        5 | 1034 | `{` |
|        - | 1035 | `	ph7_class_instance *pNew;` |
|        - | 1036 | `	sxi32 rc;` |
|  1559847 | 1037 | `	pNew = NewClassInstance(&(*pVm),&(*pClass));` |
|  1559847 | 1038 | `	if( pNew == 0 ){` |
|      ! 0 | 1039 | `		return 0;` |
|        - | 1040 | `	}` |
|        - | 1041 | `	/* Associate a private VM frame with this class instance */` |
|  1559847 | 1042 | `	rc = PH7_VmCreateClassInstanceFrame(&(*pVm),pNew);` |
|  1559847 | 1043 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1044 | `		SyMemBackendPoolFree(&pVm->sAllocator,pNew);` |
|      ! 0 | 1045 | `		return 0;` |
|        - | 1046 | `	}` |
|        - | 1047 | `	/* php stamps a Throwable's file/line at CREATION, not in its constructor, so a` |
|        - | 1048 | `	 * subclass that overrides __construct without calling parent::__construct still` |
|        - | 1049 | `	 * reports the right site. Every instantiation path lands here. */` |
|  1559847 | 1050 | `	PH7_VmStampThrowableSite(&(*pVm),pNew);` |
|  1559847 | 1051 | `	return pNew;` |
|   779926 | 1052 | `}` |
|        - | 1053 | `/*` |
|        - | 1054 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon] attribute.` |
|        - | 1055 | ` * This function never fail.` |
|        - | 1056 | ` */` |
|  5817096 | 1057 | `static ph7_value * ExtractClassAttrValue(ph7_vm *pVm,VmClassAttr *pAttr)` |
|        5 | 1058 | `{` |
|        - | 1059 | `	/* Extract the value */` |
|        - | 1060 | `	ph7_value *pValue;` |
|  5817101 | 1061 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|  5817101 | 1062 | `	return pValue;` |
|        5 | 1063 | `}` |
|        - | 1064 | `/*` |
|        - | 1065 | ` * Perform a clone operation on a class instance [i.e: Object in the PHP jargon].` |
|        - | 1066 | ` * The following function is called when an object is cloned at run-time` |
|        - | 1067 | ` * typically when the PH7_OP_CLONE instruction is executed.` |
|        - | 1068 | ` * Notes on object cloning.` |
|        - | 1069 | ` *` |
|        - | 1070 | ` * According to PHP language reference manual.` |
|        - | 1071 | ` * Creating a copy of an object with fully replicated properties is not always the wanted behavior.` |
|        - | 1072 | ` * A good example of the need for copy constructors. Another example is if your object holds a reference` |
|        - | 1073 | ` * to another object which it uses and when you replicate the parent object you want to create` |
|        - | 1074 | ` * a new instance of this other object so that the replica has its own separate copy.` |
|        - | 1075 | ` * An object copy is created by using the clone keyword (which calls the object's __clone() method if possible).` |
|        - | 1076 | ` * An object's __clone() method cannot be called directly.` |
|        - | 1077 | ` * $copy_of_object = clone $object;` |
|        - | 1078 | ` * When an object is cloned, PHP 5 will perform a shallow copy of all of the object's properties.` |
|        - | 1079 | ` * Any properties that are references to other variables, will remain references.` |
|        - | 1080 | ` * Once the cloning is complete, if a __clone() method is defined, then the newly created object's __clone() method` |
|        - | 1081 | ` * will be called, to allow any necessary properties that need to be changed.` |
|        - | 1082 | ` * Example #1 Cloning an object` |
|        - | 1083 | ` * <?php` |
|        - | 1084 | ` * class SubObject` |
|        - | 1085 | ` * {` |
|        - | 1086 | ` *   static $instances = 0;` |
|        - | 1087 | ` *   public $instance;` |
|        - | 1088 | ` *` |
|        - | 1089 | ` *   public function __construct() {` |
|        - | 1090 | ` *       $this->instance = ++self::$instances;` |
|        - | 1091 | ` *   }` |
|        - | 1092 | ` *` |
|        - | 1093 | ` *   public function __clone() {` |
|        - | 1094 | ` *       $this->instance = ++self::$instances;` |
|        - | 1095 | ` *   }` |
|        - | 1096 | ` * }` |
|        - | 1097 | ` *` |
|        - | 1098 | ` * class MyCloneable` |
|        - | 1099 | ` * {` |
|        - | 1100 | ` *   public $object1;` |
|        - | 1101 | ` *   public $object2;` |
|        - | 1102 | ` *` |
|        - | 1103 | ` *   function __clone()` |
|        - | 1104 | ` *   {` |
|        - | 1105 | ` *       // Force a copy of this->object, otherwise` |
|        - | 1106 | ` *       // it will point to same object.` |
|        - | 1107 | ` *       $this->object1 = clone $this->object1;` |
|        - | 1108 | ` *   }` |
|        - | 1109 | ` * }` |
|        - | 1110 | ` * $obj = new MyCloneable();` |
|        - | 1111 | ` * $obj->object1 = new SubObject();` |
|        - | 1112 | ` * $obj->object2 = new SubObject();` |
|        - | 1113 | ` * $obj2 = clone $obj;` |
|        - | 1114 | ` * print("Original Object:\n");` |
|        - | 1115 | ` * print_r($obj);` |
|        - | 1116 | ` * print("Cloned Object:\n");` |
|        - | 1117 | ` * print_r($obj2);` |
|        - | 1118 | ` * ?>` |
|        - | 1119 | ` * The above example will output:` |
|        - | 1120 | ` * Original Object:` |
|        - | 1121 | ` * MyCloneable Object` |
|        - | 1122 | ` * (` |
|        - | 1123 | ` *   [object1] => SubObject Object` |
|        - | 1124 | ` *       (` |
|        - | 1125 | ` *           [instance] => 1` |
|        - | 1126 | ` *       )` |
|        - | 1127 | ` *` |
|        - | 1128 | ` *   [object2] => SubObject Object` |
|        - | 1129 | ` *       (` |
|        - | 1130 | ` *           [instance] => 2` |
|        - | 1131 | ` *       )` |
|        - | 1132 | ` *` |
|        - | 1133 | ` * )` |
|        - | 1134 | ` * Cloned Object:` |
|        - | 1135 | ` * MyCloneable Object` |
|        - | 1136 | ` * (` |
|        - | 1137 | ` *   [object1] => SubObject Object` |
|        - | 1138 | ` *       (` |
|        - | 1139 | ` *           [instance] => 3` |
|        - | 1140 | ` *       )` |
|        - | 1141 | ` *` |
|        - | 1142 | ` *   [object2] => SubObject Object` |
|        - | 1143 | ` *       (` |
|        - | 1144 | ` *           [instance] => 2` |
|        - | 1145 | ` *       )` |
|        - | 1146 | ` * )` |
|        - | 1147 | ` */` |
|      262 | 1148 | `PH7_PRIVATE ph7_class_instance * PH7_CloneClassInstance(ph7_class_instance *pSrc)` |
|        5 | 1149 | `{` |
|        - | 1150 | `	ph7_class_instance *pClone;` |
|        - | 1151 | `	ph7_class_method *pMethod;` |
|        - | 1152 | `	SyHashEntry *pEntry2;` |
|        - | 1153 | `	SyHashEntry *pEntry;` |
|        - | 1154 | `	ph7_vm *pVm;` |
|        - | 1155 | `	sxi32 rc;` |
|        - | 1156 | `	/* Allocate a new instance */` |
|      267 | 1157 | `	pVm = pSrc->pVm;` |
|      267 | 1158 | `	pClone = NewClassInstance(pVm,pSrc->pClass);` |
|      267 | 1159 | `	if( pClone == 0 ){` |
|      ! 0 | 1160 | `		return 0;` |
|        - | 1161 | `	}` |
|        - | 1162 | `	/* Associate a private VM frame with this class instance */` |
|      267 | 1163 | `	rc = PH7_VmCreateClassInstanceFrame(pVm,pClone);` |
|      267 | 1164 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1165 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClone);` |
|      ! 0 | 1166 | `		return 0;` |
|        - | 1167 | `	}` |
|        - | 1168 | `	/* Duplicate object values. Iterate the SOURCE attributes and copy each into` |
|        - | 1169 | `	 * the clone's same-named slot (looked up by name, so order/count differences` |
|        - | 1170 | `	 * from dynamic properties don't matter). A dynamic (runtime-added) property` |
|        - | 1171 | `	 * has no declared counterpart in the clone, so synthesize it first — without` |
|        - | 1172 | `	 * this, a clone of a stdClass would silently lose all its dynamic properties. */` |
|      267 | 1173 | `	SyHashResetLoopCursor(&pSrc->hAttr);` |
|     1077 | 1174 | `	while((pEntry = SyHashGetNextEntry(&pSrc->hAttr)) != 0 ){` |
|      815 | 1175 | `		VmClassAttr *pSrcAttr = (VmClassAttr *)pEntry->pUserData;` |
|      815 | 1176 | `		VmClassAttr *pDestAttr = 0;` |
|      815 | 1177 | `		ph7_value *pvSrc,*pvDest = 0;` |
|        - | 1178 | `		/* Duplicate non-static attribute */` |
|      815 | 1179 | `		if( pSrcAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|       71 | 1180 | `			continue;` |
|        - | 1181 | `		}` |
|      745 | 1182 | `		pEntry2 = SyHashGet(&pClone->hAttr,SyStringData(&pSrcAttr->pAttr->sName),SyStringLength(&pSrcAttr->pAttr->sName));` |
|      745 | 1183 | `		if( pEntry2 ){` |
|      731 | 1184 | `			pDestAttr = (VmClassAttr *)pEntry2->pUserData;` |
|      731 | 1185 | `			pvDest = ExtractClassAttrValue(pVm,pDestAttr);` |
|      378 | 1186 | `		}else if( pSrcAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){` |
|        - | 1187 | `			/* Dynamic property: synthesize the matching slot on the clone. */` |
|       22 | 1188 | `			pvDest = PH7_VmCreateDynamicAttr(pVm,pClone,` |
|       14 | 1189 | `				SyStringData(&pSrcAttr->pAttr->sName),SyStringLength(&pSrcAttr->pAttr->sName),&pDestAttr);` |
|        7 | 1190 | `		}` |
|        - | 1191 | `		/* Fetch the source value LAST: PH7_VmCreateDynamicAttr above may have` |
|        - | 1192 | `		 * reserved a slot and reallocated pVm->aMemObj, which would dangle any` |
|        - | 1193 | `		 * ph7_value* obtained before it. pvDest from the synth path already points` |
|        - | 1194 | `		 * into the post-realloc aMemObj; resolve pvSrc now so both are current. */` |
|      745 | 1195 | `		pvSrc = ExtractClassAttrValue(pVm,pSrcAttr);` |
|      745 | 1196 | `		if( (pSrcAttr->iState & VM_CLASS_ATTR_REFBOUND) && pDestAttr ){` |
|        - | 1197 | `			/* php preserves references across clone: the clone shares the SAME slot` |
|        - | 1198 | `			 * as the source property (both alias the referenced variable), rather` |
|        - | 1199 | `			 * than getting an independent value copy. Drop the clone's fresh private` |
|        - | 1200 | `			 * slot and repoint at the (already-pinned) shared slot. The REFBOUND flag` |
|        - | 1201 | `			 * is carried over by the iState copy below, so the clone's release also` |
|        - | 1202 | `			 * leaves the shared slot alone. */` |
|        3 | 1203 | `			if( pDestAttr->nIdx != pSrcAttr->nIdx ){` |
|        3 | 1204 | `				if( pDestAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|      ! 0 | 1205 | `					SyHashDeleteEntry(&pVm->hTypedSlot,(const void *)&pDestAttr->nIdx,sizeof(sxu32),0);` |
|      ! 0 | 1206 | `				}` |
|        3 | 1207 | `				PH7_VmUnsetMemObj(pVm,pDestAttr->nIdx,TRUE);` |
|        3 | 1208 | `				pDestAttr->nIdx = pSrcAttr->nIdx;` |
|        2 | 1209 | `			}` |
|      744 | 1210 | `		}else if( pvSrc && pvDest ){` |
|      743 | 1211 | `			PH7_MemObjStore(pvSrc,pvDest);` |
|      369 | 1212 | `		}` |
|        - | 1213 | `		/* Carry over the per-instance state so the clone matches the source:` |
|        - | 1214 | `		 * VM_CLASS_ATTR_UNINIT marks a typed property as not-yet-initialized` |
|        - | 1215 | `		 * and doubles as the readonly write-once latch — without this a clone` |
|        - | 1216 | `		 * would reset to uninitialized (losing the value's readiness) and a` |
|        - | 1217 | `		 * readonly property would become writable again. */` |
|      745 | 1218 | `		if( pDestAttr ){` |
|      745 | 1219 | `			pDestAttr->iState = pSrcAttr->iState;` |
|      370 | 1220 | `		}` |
|        5 | 1221 | `	}` |
|        - | 1222 | `	/* A declared property unset() on the source is absent from the clone too (PHP). But the clone` |
|        - | 1223 | `	 * frame above materialized ALL declared attrs (with their defaults), so drop any clone attr whose` |
|        - | 1224 | `	 * name is not present on the source. Collect first, then delete — removing an entry mid-walk would` |
|        - | 1225 | `	 * free the node the SyHash loop cursor points at. */` |
|        - | 1226 | `	{` |
|        - | 1227 | `		SySet sDrop;` |
|      267 | 1228 | `		SySetInit(&sDrop,&pVm->sAllocator,sizeof(VmClassAttr *));` |
|      267 | 1229 | `		SyHashResetLoopCursor(&pClone->hAttr);` |
|     1079 | 1230 | `		while((pEntry = SyHashGetNextEntry(&pClone->hAttr)) != 0 ){` |
|      817 | 1231 | `			VmClassAttr *pCloneAttr = (VmClassAttr *)pEntry->pUserData;` |
|      817 | 1232 | `			if( pCloneAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|       71 | 1233 | `				continue;` |
|        - | 1234 | `			}` |
|     1113 | 1235 | `			if( SyHashGet(&pSrc->hAttr,SyStringData(&pCloneAttr->pAttr->sName),` |
|     1118 | 1236 | `					SyStringLength(&pCloneAttr->pAttr->sName)) == 0 ){` |
|        3 | 1237 | `				SySetPut(&sDrop,(const void *)&pCloneAttr);` |
|        1 | 1238 | `			}` |
|        5 | 1239 | `		}` |
|      267 | 1240 | `		if( SySetUsed(&sDrop) > 0 ){` |
|        3 | 1241 | `			VmClassAttr **apDrop = (VmClassAttr **)SySetBasePtr(&sDrop);` |
|        - | 1242 | `			sxu32 i;` |
|        5 | 1243 | `			for( i = 0 ; i < SySetUsed(&sDrop) ; ++i ){` |
|        3 | 1244 | `				VmClassAttr *pVmAttr = apDrop[i];` |
|        4 | 1245 | `				SyHashDeleteEntry(&pClone->hAttr,SyStringData(&pVmAttr->pAttr->sName),` |
|        2 | 1246 | `					SyStringLength(&pVmAttr->pAttr->sName),0);` |
|        3 | 1247 | `				PH7_VmReleaseInstanceAttr(pVm,pVmAttr);` |
|        2 | 1248 | `			}` |
|        1 | 1249 | `		}` |
|      267 | 1250 | `		SySetRelease(&sDrop);` |
|        - | 1251 | `	}` |
|        - | 1252 | `	/* call the __clone method on the cloned object if available */` |
|      267 | 1253 | `	pMethod = PH7_ClassExtractMethod(pClone->pClass,"__clone",sizeof("__clone")-1);` |
|      267 | 1254 | `	if( pMethod ){` |
|       63 | 1255 | `		if( pMethod->iCloneDepth < 16 ){` |
|       61 | 1256 | `			pMethod->iCloneDepth++;` |
|        - | 1257 | `			/* PHP 8.3: __clone() may re-initialize the clone's readonly` |
|        - | 1258 | `			 * properties. Flag the instance so the readonly store guard allows` |
|        - | 1259 | `			 * it for the duration of the call. */` |
|       61 | 1260 | `			pClone->iFlags \|= VM_INSTANCE_CLONING;` |
|       61 | 1261 | `			PH7_VmCallClassMethod(pVm,pClone,pMethod,0,0,0);` |
|       61 | 1262 | `			pClone->iFlags &= ~VM_INSTANCE_CLONING;` |
|       32 | 1263 | `		}else{` |
|        - | 1264 | `			/* Nesting limit reached */` |
|        3 | 1265 | `			PH7_VmThrowError(pVm,0,PH7_CTX_ERR,"Object clone limit reached,no more call to __clone()");` |
|        - | 1266 | `		}` |
|        - | 1267 | `		/* Reset the cursor */` |
|       63 | 1268 | `		pMethod->iCloneDepth = 0;` |
|       30 | 1269 | `	}` |
|        - | 1270 | `	/* Return the cloned object */` |
|      267 | 1271 | `	return pClone;` |
|      136 | 1272 | `}` |
|        - | 1273 | `#define CLASS_INSTANCE_DESTROYED 0x001 /* Instance is released */` |
|        - | 1274 | `/*` |
|        - | 1275 | ` * Free the per-instance allocations owned by ONE object attribute: its value slot (+ the typed-slot` |
|        - | 1276 | ` * enforcement entry), the synthesized ph7_class_attr for a dynamic (runtime-added) property, and the` |
|        - | 1277 | ` * VmClassAttr wrapper itself. Does NOT touch the hAttr entry node — the caller removes it` |
|        - | 1278 | `` * (`unset($o->p)` via SyHashDeleteEntry2; instance teardown via the wholesale SyHashRelease, so it must`` |
|        - | 1279 | ` * not delete entries mid-walk). Shared by PH7_ClassInstanceRelease and the OP_MEMBER unset path.` |
|        - | 1280 | ` */` |
|  8103690 | 1281 | `PH7_PRIVATE void PH7_VmReleaseInstanceAttr(ph7_vm *pVm, VmClassAttr *pVmAttr)` |
|        5 | 1282 | `{` |
|  8103695 | 1283 | `	if( pVmAttr->iState & VM_CLASS_ATTR_REFBOUND ){` |
|        - | 1284 | ``		/* Reference-bound property (`$o->p =& $x`): its value slot is SHARED with`` |
|        - | 1285 | `		 * (and pinned by) the source variable — releasing/recycling it here would` |
|        - | 1286 | `		 * dangle the surviving alias. Leave the slot alone (script-lifetime pin,` |
|        - | 1287 | `		 * matching the use(&$x) capture tradeoff); just free the bookkeeping below. */` |
|  8103690 | 1288 | `	}else if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - | 1289 | `		/* Drop any typed-property enforcement slot registered for this memobj, before the memobj` |
|        - | 1290 | `		 * is returned to the free list, so a future recycled slot does not inherit the stale entry. */` |
|  8103423 | 1291 | `		if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|      353 | 1292 | `			SyHashDeleteEntry(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),0);` |
|      174 | 1293 | `		}` |
|  8103423 | 1294 | `		PH7_VmUnsetMemObj(pVm,pVmAttr->nIdx,TRUE);` |
|  4051709 | 1295 | `	}` |
|        - | 1296 | `	/* A dynamic property owns its synthesized ph7_class_attr (struct + inline name in one block) —` |
|        - | 1297 | `	 * free it here (the only place a per-instance pAttr is freed; declared attrs are class-owned). */` |
|  8103695 | 1298 | `	if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){` |
|      143 | 1299 | `		SyMemBackendFree(&pVm->sAllocator,pVmAttr->pAttr);` |
|       70 | 1300 | `	}` |
|  8103695 | 1301 | `	SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|  8103695 | 1302 | `}` |
|        - | 1303 | `/*` |
|        - | 1304 | ` * Release a class instance [i.e: Object in the PHP jargon] and invoke any defined destructor.` |
|        - | 1305 | ` * This routine is invoked as soon as there are no other references to a particular` |
|        - | 1306 | ` * class instance.` |
|        - | 1307 | ` */` |
|  1454368 | 1308 | `static void PH7_ClassInstanceRelease(ph7_class_instance *pThis)` |
|        5 | 1309 | `{` |
|        - | 1310 | `	ph7_class_method *pDestr;` |
|        - | 1311 | `	SyHashEntry *pEntry;` |
|        - | 1312 | `	ph7_class *pClass;` |
|        - | 1313 | `	ph7_vm *pVm;` |
|  1454373 | 1314 | `	if( pThis->iFlags & CLASS_INSTANCE_DESTROYED ){` |
|        - | 1315 | `		/*` |
|        - | 1316 | `		 * Already destroyed,return immediately.` |
|        - | 1317 | `		 * This could happend if someone perform unset($this) in the destructor body.` |
|        - | 1318 | `		 */` |
|      ! 0 | 1319 | `		return;` |
|        - | 1320 | `	}` |
|        - | 1321 | `	/* Mark as destroyed */` |
|  1454373 | 1322 | `	pThis->iFlags \|= CLASS_INSTANCE_DESTROYED;` |
|        - | 1323 | `	/* Invoke any defined destructor if available */` |
|  1454373 | 1324 | `	pVm = pThis->pVm;` |
|  1454373 | 1325 | `	pClass = pThis->pClass;` |
|  1454373 | 1326 | `	pDestr = PH7_ClassExtractMethod(pClass,"__destruct",sizeof("__destruct")-1);` |
|  1454373 | 1327 | `	if( pDestr && !pVm->bInReset ){` |
|        - | 1328 | `		/* Invoke the destructor. Skipped during ph7_vm_reset() bulk teardown:` |
|        - | 1329 | `		 * running user PHP against a half-reset VM is unsafe (see bInReset). */` |
|      521 | 1330 | `		pThis->iRef = 2; /* Prevent garbage collection */` |
|      521 | 1331 | `		PH7_VmCallClassMethod(pVm,pThis,pDestr,0,0,0);` |
|      258 | 1332 | `	}` |
|        - | 1333 | `	/* Weak-reference registry: kill the cell for this instance so every` |
|        - | 1334 | `	 * WeakReference/WeakMap handle observes the death (the cell outlives the` |
|        - | 1335 | `	 * instance until its own handles drop; removing the hash entry here keeps` |
|        - | 1336 | `	 * a pool-reused address from resurrecting a dead cell). */` |
|  1454373 | 1337 | `	if( SyHashTotalEntry(&pVm->hWeakCell) > 0 ){` |
|       27 | 1338 | `		void *pCellData = 0;` |
|       26 | 1339 | `		if( SyHashDeleteEntry(&pVm->hWeakCell,(const void *)&pThis,sizeof(void *),&pCellData) == SXRET_OK` |
|       18 | 1340 | `		 && pCellData ){` |
|        9 | 1341 | `			((VmWeakCell *)pCellData)->pObj = 0;` |
|        4 | 1342 | `		}` |
|       13 | 1343 | `	}` |
|        - | 1344 | `	/* Release non-static attributes (the wholesale SyHashRelease below frees the entry nodes,` |
|        - | 1345 | `	 * so the helper must not delete them mid-walk). */` |
|  1454373 | 1346 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|  9558035 | 1347 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|  8103667 | 1348 | `		PH7_VmReleaseInstanceAttr(pVm,(VmClassAttr *)pEntry->pUserData);` |
|        5 | 1349 | `	}` |
|        - | 1350 | `	/* Release the whole structure */` |
|  1454373 | 1351 | `	SyHashRelease(&pThis->hAttr);` |
|  1454373 | 1352 | `	SyMemBackendPoolFree(&pVm->sAllocator,pThis);` |
|   727189 | 1353 | `}` |
|        - | 1354 | `/*` |
|        - | 1355 | ` * Decrement the reference count of a class instance [i.e Object in the PHP jargon].` |
|        - | 1356 | ` * If the reference count reaches zero,release the whole instance.` |
|        - | 1357 | ` */` |
| 13127438 | 1358 | `PH7_PRIVATE void PH7_ClassInstanceUnref(ph7_class_instance *pThis)` |
|        5 | 1359 | `{` |
| 13127443 | 1360 | `	pThis->iRef--;` |
| 13127443 | 1361 | `	if( pThis->iRef < 1 ){` |
|        - | 1362 | `		/* No more reference to this instance */` |
|  1454373 | 1363 | `		PH7_ClassInstanceRelease(&(*pThis));` |
|   727184 | 1364 | `	}` |
| 13127443 | 1365 | `}` |
|        - | 1366 | `/*` |
|        - | 1367 | ` * Compare two class instances [i.e: Objects in the PHP jargon]` |
|        - | 1368 | ` * Note on objects comparison:` |
|        - | 1369 | ` *  According to the PHP langauge reference manual` |
|        - | 1370 | ` *  When using the comparison operator (==), object variables are compared in a simple manner` |
|        - | 1371 | ` *  namely: Two object instances are equal if they have the same attributes and values, and are` |
|        - | 1372 | ` *  instances of the same class.` |
|        - | 1373 | ` *  On the other hand, when using the identity operator (===), object variables are identical` |
|        - | 1374 | ` *  if and only if they refer to the same instance of the same class.` |
|        - | 1375 | ` *  An example will clarify these rules.` |
|        - | 1376 | ` *  Example #1 Example of object comparison` |
|        - | 1377 | ` *  <?php` |
|        - | 1378 | ` *    function bool2str($bool)` |
|        - | 1379 | ` * {` |
|        - | 1380 | ` *   if ($bool === false) {` |
|        - | 1381 | ` *       return 'FALSE';` |
|        - | 1382 | ` *   } else {` |
|        - | 1383 | ` *       return 'TRUE';` |
|        - | 1384 | ` *   }` |
|        - | 1385 | ` * }` |
|        - | 1386 | ` * function compareObjects(&$o1, &$o2)` |
|        - | 1387 | ` * {` |
|        - | 1388 | ` *   echo 'o1 == o2 : ' . bool2str($o1 == $o2) . "\n";` |
|        - | 1389 | ` *   echo 'o1 != o2 : ' . bool2str($o1 != $o2) . "\n";` |
|        - | 1390 | ` *   echo 'o1 === o2 : ' . bool2str($o1 === $o2) . "\n";` |
|        - | 1391 | ` *   echo 'o1 !== o2 : ' . bool2str($o1 !== $o2) . "\n";` |
|        - | 1392 | ` * }` |
|        - | 1393 | ` * class Flag` |
|        - | 1394 | ` * {` |
|        - | 1395 | ` *   public $flag;` |
|        - | 1396 | ` *` |
|        - | 1397 | ` *   function Flag($flag = true) {` |
|        - | 1398 | ` *       $this->flag = $flag;` |
|        - | 1399 | ` *   }` |
|        - | 1400 | ` * }` |
|        - | 1401 | ` *` |
|        - | 1402 | ` * class OtherFlag` |
|        - | 1403 | ` * {` |
|        - | 1404 | ` *   public $flag;` |
|        - | 1405 | ` *` |
|        - | 1406 | ` *   function OtherFlag($flag = true) {` |
|        - | 1407 | ` *       $this->flag = $flag;` |
|        - | 1408 | ` *   }` |
|        - | 1409 | ` * }` |
|        - | 1410 | ` *` |
|        - | 1411 | ` * $o = new Flag();` |
|        - | 1412 | ` * $p = new Flag();` |
|        - | 1413 | ` * $q = $o;` |
|        - | 1414 | ` * $r = new OtherFlag();` |
|        - | 1415 | ` *` |
|        - | 1416 | ` * echo "Two instances of the same class\n";` |
|        - | 1417 | ` * compareObjects($o, $p);` |
|        - | 1418 | ` * echo "\nTwo references to the same instance\n";` |
|        - | 1419 | ` * compareObjects($o, $q);` |
|        - | 1420 | ` * echo "\nInstances of two different classes\n";` |
|        - | 1421 | ` * compareObjects($o, $r);` |
|        - | 1422 | ` * ?>` |
|        - | 1423 | ` * The above example will output:` |
|        - | 1424 | ` * Two instances of the same class` |
|        - | 1425 | ` * o1 == o2 : TRUE` |
|        - | 1426 | ` * o1 != o2 : FALSE` |
|        - | 1427 | ` * o1 === o2 : FALSE` |
|        - | 1428 | ` * o1 !== o2 : TRUE` |
|        - | 1429 | ` * Two references to the same instance` |
|        - | 1430 | ` * o1 == o2 : TRUE` |
|        - | 1431 | ` * o1 != o2 : FALSE` |
|        - | 1432 | ` * o1 === o2 : TRUE` |
|        - | 1433 | ` * o1 !== o2 : FALSE` |
|        - | 1434 | ` * Instances of two different classes` |
|        - | 1435 | ` * o1 == o2 : FALSE` |
|        - | 1436 | ` * o1 != o2 : TRUE` |
|        - | 1437 | ` * o1 === o2 : FALSE` |
|        - | 1438 | ` * o1 !== o2 : TRUE` |
|        - | 1439 | ` *` |
|        - | 1440 | ` * This function return 0 if the objects are equals according to the comprison rules defined above.` |
|        - | 1441 | ` * Any other return values indicates difference.` |
|        - | 1442 | ` */` |
|      304 | 1443 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest)` |
|        5 | 1444 | `{` |
|        - | 1445 | `	SyHashEntry *pEntry,*pEntry2;` |
|        - | 1446 | `	ph7_value sV1,sV2;` |
|        - | 1447 | `	sxi32 rc;` |
|      309 | 1448 | `	if( iNest > 31 ){` |
|        - | 1449 | `		/* Nesting limit reached */` |
|        6 | 1450 | `		PH7_VmThrowError(pLeft->pVm,0,PH7_CTX_ERR,"Nesting limit reached: Infinite recursion?");` |
|        6 | 1451 | `		return 1;` |
|        - | 1452 | `	}` |
|        - | 1453 | `	/* Comparison is performed only if the objects are instance of the same class */` |
|      305 | 1454 | `	if( pLeft->pClass != pRight->pClass ){` |
|       10 | 1455 | `		return 1;` |
|        - | 1456 | `	}` |
|      297 | 1457 | `	if( bStrict ){` |
|        - | 1458 | `		/*` |
|        - | 1459 | `		 * According to the PHP language reference manual:` |
|        - | 1460 | `		 *  when using the identity operator (===), object variables` |
|        - | 1461 | `		 *  are identical if and only if they refer to the same instance` |
|        - | 1462 | `		 *  of the same class.` |
|        - | 1463 | `		 */` |
|      127 | 1464 | `		return !(pLeft == pRight);` |
|        - | 1465 | `	}` |
|        - | 1466 | `	/*` |
|        - | 1467 | `	 * Attribute comparison.` |
|        - | 1468 | `	 * According to the PHP reference manual:` |
|        - | 1469 | `	 *  When using the comparison operator (==), object variables are compared` |
|        - | 1470 | `	 *  in a simple manner, namely: Two object instances are equal if they have` |
|        - | 1471 | `	 *  the same attributes and values, and are instances of the same class.` |
|        - | 1472 | `	 */` |
|      175 | 1473 | `	if( pLeft == pRight ){` |
|        - | 1474 | `		/* Same instance,don't bother processing,object are equals */` |
|        5 | 1475 | `		return 0;` |
|        - | 1476 | `	}` |
|        - | 1477 | `	/* Closures compare by IDENTITY under == as well (not by attributes): two distinct` |
|        - | 1478 | `	 * Closure instances are never equal, even when they wrap the same underlying function` |
|        - | 1479 | `	 * (PHP semantics). pLeft != pRight here, so a Closure pair is unequal. Without this,` |
|        - | 1480 | `` 	 * two capture-less lambdas of the same `function(){}` share the template's `$__fn` `` |
|        - | 1481 | `	 * name and would compare equal. */` |
|      171 | 1482 | `	if( pLeft->pVm->pClosureClass && pLeft->pClass == pLeft->pVm->pClosureClass ){` |
|        5 | 1483 | `		return 1;` |
|        - | 1484 | `	}` |
|        - | 1485 | `	/* Same class but a different number of attributes ⇒ different property sets` |
|        - | 1486 | `	 * (dynamic properties can give two same-class instances different counts). */` |
|      167 | 1487 | `	if( pLeft->hAttr.nEntry != pRight->hAttr.nEntry ){` |
|        3 | 1488 | `		return 1;` |
|        - | 1489 | `	}` |
|      165 | 1490 | `	PH7_MemObjInit(pLeft->pVm,&sV1);` |
|      165 | 1491 | `	PH7_MemObjInit(pLeft->pVm,&sV2);` |
|      165 | 1492 | `	sV1.nIdx = sV2.nIdx = SXU32_HIGH;` |
|        - | 1493 | `	/* Compare each left attribute against the RIGHT attribute of the SAME NAME` |
|        - | 1494 | `	 * (not in lockstep): dynamic properties may be stored in a different order` |
|        - | 1495 | `	 * on the two instances. Counts already match, so if every left attribute has` |
|        - | 1496 | `	 * an equal-valued same-named right attribute the property sets are equal. */` |
|      165 | 1497 | `	SyHashResetLoopCursor(&pLeft->hAttr);` |
|      211 | 1498 | `	while((pEntry = SyHashGetNextEntry(&pLeft->hAttr)) != 0 ){` |
|      181 | 1499 | `		VmClassAttr *p1 = (VmClassAttr *)pEntry->pUserData;` |
|        - | 1500 | `		VmClassAttr *p2;` |
|        - | 1501 | `		ph7_value *pL,*pR;` |
|        - | 1502 | `		/* Compare only non-static attribute */` |
|      181 | 1503 | `		if( p1->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        3 | 1504 | `			continue;` |
|        - | 1505 | `		}` |
|      179 | 1506 | `		pEntry2 = SyHashGet(&pRight->hAttr,SyStringData(&p1->pAttr->sName),SyStringLength(&p1->pAttr->sName));` |
|      179 | 1507 | `		if( pEntry2 == 0 ){` |
|        - | 1508 | `			/* Left has a property the right lacks ⇒ not equal. */` |
|      ! 0 | 1509 | `			return 1;` |
|        - | 1510 | `		}` |
|      179 | 1511 | `		p2 = (VmClassAttr *)pEntry2->pUserData;` |
|      179 | 1512 | `		pL = ExtractClassAttrValue(pLeft->pVm,p1);` |
|      179 | 1513 | `		pR = ExtractClassAttrValue(pRight->pVm,p2);` |
|      179 | 1514 | `		if( pL && pR ){` |
|      179 | 1515 | `			PH7_MemObjLoad(pL,&sV1);` |
|      179 | 1516 | `			PH7_MemObjLoad(pR,&sV2);` |
|        - | 1517 | `			/* Compare the two values now */` |
|      179 | 1518 | `			rc = PH7_MemObjCmp(&sV1,&sV2,bStrict,iNest+1);` |
|      179 | 1519 | `			PH7_MemObjRelease(&sV1);` |
|      179 | 1520 | `			PH7_MemObjRelease(&sV2);` |
|      179 | 1521 | `			if( rc != 0 ){` |
|        - | 1522 | `				/* Not equals */` |
|      133 | 1523 | `				return rc;` |
|        - | 1524 | `			}` |
|       22 | 1525 | `		}` |
|        3 | 1526 | `	}` |
|        - | 1527 | `	/* Object are equals */` |
|       33 | 1528 | `	return 0;` |
|      157 | 1529 | `}` |
|        - | 1530 | `/*` |
|        - | 1531 | ` * Dump a class instance and the store the dump in the BLOB given` |
|        - | 1532 | ` * as the first argument.` |
|        - | 1533 | ` * Note that only non-static/non-constants attribute are dumped.` |
|        - | 1534 | ` * This function is typically invoked when the user issue a call` |
|        - | 1535 | ` * to [var_dump(),var_export(),print_r(),...].` |
|        - | 1536 | ` * This function SXRET_OK on success. Any other return value including` |
|        - | 1537 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|        - | 1538 | ` */` |
|        - | 1539 | `/*` |
|        - | 1540 | `` * Return the `name` property value of an enum case instance (the case name),`` |
|        - | 1541 | ` * or 0 when unavailable. Shared by the var_dump/var_export/json/serialize` |
|        - | 1542 | ` * renderers, which all print enum cases as Class::CaseName forms.` |
|        - | 1543 | ` */` |
|        6 | 1544 | `PH7_PRIVATE ph7_value * PH7_EnumCaseNameValue(ph7_class_instance *pThis)` |
|        1 | 1545 | `{` |
|        - | 1546 | `	SyHashEntry *pEntry;` |
|        7 | 1547 | `	if( (pThis->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|      ! 0 | 1548 | `		return 0;` |
|        - | 1549 | `	}` |
|        7 | 1550 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)"name",sizeof("name")-1);` |
|        7 | 1551 | `	if( pEntry == 0 ){` |
|      ! 0 | 1552 | `		return 0;` |
|        - | 1553 | `	}` |
|        7 | 1554 | `	return PH7_ClassInstanceExtractAttrValue(pThis,(VmClassAttr *)pEntry->pUserData);` |
|        4 | 1555 | `}` |
|        - | 1556 | `/*` |
|        - | 1557 | `` * Return the `value` property value (the backing value) of an enum case`` |
|        - | 1558 | ` * instance, or 0 when unavailable (pure enums have none).` |
|        - | 1559 | ` */` |
|        8 | 1560 | `PH7_PRIVATE ph7_value * PH7_EnumCaseBackingValueOf(ph7_class_instance *pThis)` |
|        1 | 1561 | `{` |
|        - | 1562 | `	SyHashEntry *pEntry;` |
|        9 | 1563 | `	if( (pThis->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|      ! 0 | 1564 | `		return 0;` |
|        - | 1565 | `	}` |
|        9 | 1566 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)"value",sizeof("value")-1);` |
|        9 | 1567 | `	if( pEntry == 0 ){` |
|        3 | 1568 | `		return 0;` |
|        - | 1569 | `	}` |
|        7 | 1570 | `	return PH7_ClassInstanceExtractAttrValue(pThis,(VmClassAttr *)pEntry->pUserData);` |
|        5 | 1571 | `}` |
|        - | 1572 | `/*` |
|        - | 1573 | ` * Emit a class-instance dump header plus its trailing newline. For var_dump` |
|        - | 1574 | ` * (ShowType) it completes the "object(" prefix the caller already emitted as` |
|        - | 1575 | ` *   ClassName)#<id> (<count>) {` |
|        - | 1576 | ` * for print_r it emits the legacy PHL  Object(ClassName) {  (count/id unused).` |
|        - | 1577 | `` * Enum cases print php's `ClassName Enum {` print_r header (var_dump never`` |
|        - | 1578 | `` * reaches here for enums — PH7_MemObjDump prints `enum(S::A)` directly).`` |
|        - | 1579 | ` */` |
|      150 | 1580 | `static void DumpClassInstanceHeader(SyBlob *pOut,ph7_class *pClass,sxu32 nObjId,int ShowType,sxu32 nCount)` |
|        4 | 1581 | `{` |
|      154 | 1582 | `	if( ShowType ){` |
|        - | 1583 | ``		/* var_dump: `object(C)#id (n) {` */`` |
|      144 | 1584 | `		SyBlobFormat(&(*pOut),"object(%z)#%u (%u) {",&pClass->sName,nObjId,nCount);` |
|      144 | 1585 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      144 | 1586 | `		return;` |
|        - | 1587 | `	}` |
|        - | 1588 | ``	/* print_r: `C Object` / `E Enum[:backing]` — the '(' line is emitted by`` |
|        - | 1589 | `	 * the body renderer at the container indent. */` |
|       13 | 1590 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|      ! 0 | 1591 | `		SyBlobFormat(&(*pOut),"%z Enum",&pClass->sName);` |
|      ! 0 | 1592 | `		if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|      ! 0 | 1593 | `			SyBlobAppend(&(*pOut),":int",sizeof(":int")-1);` |
|      ! 0 | 1594 | `		}else if( pClass->nEnumBacking == MEMOBJ_STRING ){` |
|      ! 0 | 1595 | `			SyBlobAppend(&(*pOut),":string",sizeof(":string")-1);` |
|      ! 0 | 1596 | `		}` |
|      ! 0 | 1597 | `	}else{` |
|       13 | 1598 | `		SyBlobFormat(&(*pOut),"%z Object",&pClass->sName);` |
|        - | 1599 | `	}` |
|       13 | 1600 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       79 | 1601 | `}` |
|        - | 1602 | `/*` |
|        - | 1603 | ` * The class that DECLARED pAttr: inheritance shares attr pointers down the` |
|        - | 1604 | ` * chain, so the declaring class is the most ANCESTRAL class whose hAttr still` |
|        - | 1605 | ` * maps the name to this exact pointer. php's var_dump/print_r use it for the` |
|        - | 1606 | `` * `["p":"Decl":private]` annotation.`` |
|        - | 1607 | ` */` |
|        6 | 1608 | `static ph7_class * OoAttrDeclaringClass(ph7_class *pClass,ph7_class_attr *pAttr)` |
|        1 | 1609 | `{` |
|        - | 1610 | `	/* Attrs record their declaring class at install time (inheritance/trait` |
|        - | 1611 | `	 * copies share the pointer, so the field survives the chain). */` |
|        7 | 1612 | `	return pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        1 | 1613 | `}` |
|        - | 1614 | `/*` |
|        - | 1615 | `` * Emit a property's dump key: var_dump `["x"]=>` / `["p":"C":private]=>` /`` |
|        - | 1616 | `` * `["q":protected]=>`; print_r `[x] => ` / `[p:C:private] => ` /`` |
|        - | 1617 | `` * `[q:protected] => ` (php's exact annotations).`` |
|        - | 1618 | ` */` |
|      150 | 1619 | `static void OoDumpPropKey(SyBlob *pOut,ph7_class_instance *pThis,ph7_class_attr *pAttr,int ShowType)` |
|        4 | 1620 | `{` |
|      154 | 1621 | `	const char *zQ = ShowType ? "\"" : "";` |
|      154 | 1622 | `	SyBlobFormat(&(*pOut),"[%s%z%s",zQ,&pAttr->sName,zQ);` |
|      154 | 1623 | `	if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|        7 | 1624 | `		ph7_class *pDecl = OoAttrDeclaringClass(pThis->pClass,pAttr);` |
|        7 | 1625 | `		SyBlobFormat(&(*pOut),":%s%z%s:private",zQ,&pDecl->sName,zQ);` |
|      151 | 1626 | `	}else if( pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
|        5 | 1627 | `		SyBlobAppend(&(*pOut),":protected",sizeof(":protected")-1);` |
|        2 | 1628 | `	}` |
|      154 | 1629 | `	SyBlobAppend(&(*pOut),ShowType ? "]=>" : "] => ",ShowType ? 3 : 5);` |
|      154 | 1630 | `}` |
|      154 | 1631 | `PH7_PRIVATE sxi32 PH7_ClassInstanceDump(SyBlob *pOut,ph7_class_instance *pThis,int ShowType,int nTab,int nDepth)` |
|        4 | 1632 | `{` |
|        - | 1633 | `	SyHashEntry *pEntry;` |
|        - | 1634 | `	ph7_value *pValue;` |
|        - | 1635 | `	sxi32 rc;` |
|        - | 1636 | `	int i;` |
|      158 | 1637 | `	if( nDepth > 31 ){` |
|        - | 1638 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|        - | 1639 | `		/* Nesting limit reached..halt immediately*/` |
|        5 | 1640 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|        5 | 1641 | `		return SXERR_LIMIT;` |
|        - | 1642 | `	}` |
|      154 | 1643 | `	rc = SXRET_OK;` |
|        - | 1644 | `	{` |
|        - | 1645 | `		/* Both var_dump and print_r consult __debugInfo() (PHP behavior);` |
|        - | 1646 | `		 * var_export uses a separate renderer and never reaches here. When the` |
|        - | 1647 | `		 * method is present and returns an array, render that array's entries as` |
|        - | 1648 | `		 * the object body, with the header showing the debug array's count. The` |
|        - | 1649 | `		 * nDepth guard above protects against a __debugInfo returning the object` |
|        - | 1650 | `		 * itself. */` |
|      154 | 1651 | `		ph7_class_method *pDbg = PH7_ClassExtractMethod(pThis->pClass,"__debugInfo",sizeof("__debugInfo")-1);` |
|      154 | 1652 | `		if( pDbg ){` |
|        - | 1653 | `			ph7_value sResult;` |
|        8 | 1654 | `			PH7_MemObjInit(pThis->pVm,&sResult);` |
|        8 | 1655 | `			PH7_VmCallMagicMethod(pThis->pVm,pThis,pDbg,&sResult,0,0);` |
|        8 | 1656 | `			if( sResult.iFlags & MEMOBJ_HASHMAP ){` |
|        8 | 1657 | `				ph7_hashmap *pMap = (ph7_hashmap *)sResult.x.pOther;` |
|        - | 1658 | `				/* Header count is the debug array's entry count. */` |
|        8 | 1659 | `				DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,pMap->nEntry);` |
|        8 | 1660 | `				if( !ShowType ){` |
|        3 | 1661 | `					for( i = 0 ; i < nTab ; i++ ){` |
|      ! 0 | 1662 | `						SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      ! 0 | 1663 | `					}` |
|        3 | 1664 | `					SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|        1 | 1665 | `				}` |
|        8 | 1666 | `				rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth);` |
|        8 | 1667 | `				for( i = 0 ; i < nTab ; i++ ){` |
|      ! 0 | 1668 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      ! 0 | 1669 | `				}` |
|        8 | 1670 | `				if( ShowType ){` |
|        6 | 1671 | `					SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|        4 | 1672 | `				}else{` |
|        3 | 1673 | `					SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        - | 1674 | `				}` |
|        8 | 1675 | `				PH7_MemObjRelease(&sResult);` |
|        8 | 1676 | `				return rc;` |
|        - | 1677 | `			}` |
|        - | 1678 | `			/* Non-array return: behave as if __debugInfo were absent. */` |
|      ! 0 | 1679 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 | 1680 | `		}` |
|        - | 1681 | `	}` |
|        - | 1682 | `	{` |
|        - | 1683 | `		/* var_dump's header needs the property count up front, so pre-count the` |
|        - | 1684 | `		 * non-static/non-constant attributes (matching the dump loop below). */` |
|      148 | 1685 | `		sxu32 nProp = 0;` |
|      148 | 1686 | `		if( ShowType ){` |
|      139 | 1687 | `			SyHashResetLoopCursor(&pThis->hAttr);` |
|      283 | 1688 | `			while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|      147 | 1689 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      147 | 1690 | `				if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_HOOK_VIRTUAL)) == 0 ){` |
|      143 | 1691 | `					nProp++;` |
|       70 | 1692 | `				}` |
|        3 | 1693 | `			}` |
|       68 | 1694 | `		}` |
|      148 | 1695 | `		DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,nProp);` |
|        - | 1696 | `	}` |
|      148 | 1697 | `	if( !ShowType ){` |
|        - | 1698 | `		/* print_r body opener: '(' at the container indent */` |
|       27 | 1699 | `		for( i = 0 ; i < nTab ; i++ ){` |
|       17 | 1700 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        9 | 1701 | `		}` |
|       11 | 1702 | `		SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|        4 | 1703 | `	}` |
|        - | 1704 | `	/* Dump object attributes (php 8.4: VIRTUAL hooked properties have no` |
|        - | 1705 | `	 * backing store — excluded from var_dump/print_r) */` |
|      148 | 1706 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      252 | 1707 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|      160 | 1708 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      160 | 1709 | `		if((pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_HOOK_VIRTUAL)) == 0 ){` |
|        - | 1710 | `			/* Dump non-static/constant attribute only */` |
|      154 | 1711 | `			pValue = ExtractClassAttrValue(pThis->pVm,pVmAttr);` |
|      154 | 1712 | `			if( pValue == 0 ){` |
|      ! 0 | 1713 | `				continue;` |
|        - | 1714 | `			}` |
|      154 | 1715 | `			if( ShowType ){` |
|        - | 1716 | ``				/* var_dump prop: `["x"(:…)]=>` at nTab+2, the value on the next`` |
|        - | 1717 | `				 * line at the same indent (php). */` |
|     4143 | 1718 | `				for( i = 0 ; i < nTab + 2 ; i++ ){` |
|     4003 | 1719 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     2003 | 1720 | `				}` |
|      143 | 1721 | `				OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,TRUE);` |
|      143 | 1722 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      143 | 1723 | `				rc = PH7_MemObjDump(&(*pOut),pValue,TRUE,nTab+2,nDepth,0);` |
|      143 | 1724 | `				if( rc == SXERR_LIMIT ){` |
|      125 | 1725 | `					break;` |
|        - | 1726 | `				}` |
|       11 | 1727 | `			}else{` |
|        - | 1728 | ``				/* print_r prop: `[x(:…)] => value` at nTab+4; container values`` |
|        - | 1729 | `				 * render their block at nTab+8 followed by php's blank line. */` |
|       53 | 1730 | `				for( i = 0 ; i < nTab + 4 ; i++ ){` |
|       43 | 1731 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       23 | 1732 | `				}` |
|       13 | 1733 | `				OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,FALSE);` |
|       10 | 1734 | `				if( (pValue->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ))` |
|        8 | 1735 | `				 && (pValue->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 | 1736 | `					rc = PH7_MemObjDump(&(*pOut),pValue,FALSE,nTab+8,nDepth,0);` |
|      ! 0 | 1737 | `					SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      ! 0 | 1738 | `					if( rc == SXERR_LIMIT ){` |
|      ! 0 | 1739 | `						break;` |
|        - | 1740 | `					}` |
|      ! 0 | 1741 | `				}else{` |
|       13 | 1742 | `					PH7_MemObjPrintRInline(&(*pOut),pValue);` |
|       13 | 1743 | `					SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        - | 1744 | `				}` |
|        - | 1745 | `			}` |
|       13 | 1746 | `		}` |
|        4 | 1747 | `	}` |
|     3888 | 1748 | `	for( i = 0 ; i < nTab ; i++ ){` |
|     3742 | 1749 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     1872 | 1750 | `	}` |
|      148 | 1751 | `	if( ShowType ){` |
|      139 | 1752 | `		SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|       71 | 1753 | `	}else{` |
|       11 | 1754 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        - | 1755 | `	}` |
|      148 | 1756 | `	return rc;` |
|       81 | 1757 | `}` |
|        - | 1758 | `/*` |
|        - | 1759 | ` * Call a magic method [i.e: __toString(),__toBool(),__Invoke()...]` |
|        - | 1760 | ` * Return SXRET_OK on successfull call. Any other return value indicates failure.` |
|        - | 1761 | ` * Notes on magic methods.` |
|        - | 1762 | ` * According to the PHP language reference manual.` |
|        - | 1763 | ` *  The function names __construct(), __destruct(), __call(), __callStatic()` |
|        - | 1764 | ` *  __get(),  __toString(), __invoke(), __clone() are magical in PHP classes.` |
|        - | 1765 | ` * You cannot have functions with these names in any of your classes unless` |
|        - | 1766 | ` * you want the magic functionality associated with them.` |
|        - | 1767 | ` * Example of magical methods:` |
|        - | 1768 | ` * __toString()` |
|        - | 1769 | ` *  The __toString() method allows a class to decide how it will react when it is treated like` |
|        - | 1770 | ` *  a string. For example, what echo $obj; will print. This method must return a string.` |
|        - | 1771 | ` *  Example #2 Simple example` |
|        - | 1772 | ` * <?php` |
|        - | 1773 | ` * // Declare a simple class` |
|        - | 1774 | ` * class TestClass` |
|        - | 1775 | ` * {` |
|        - | 1776 | ` *   public $foo;` |
|        - | 1777 | ` *` |
|        - | 1778 | ` *   public function __construct($foo)` |
|        - | 1779 | ` *   {` |
|        - | 1780 | ` *       $this->foo = $foo;` |
|        - | 1781 | ` *   }` |
|        - | 1782 | ` *` |
|        - | 1783 | ` *   public function __toString()` |
|        - | 1784 | ` *   {` |
|        - | 1785 | ` *       return $this->foo;` |
|        - | 1786 | ` *   }` |
|        - | 1787 | ` * }` |
|        - | 1788 | ` * $class = new TestClass('Hello');` |
|        - | 1789 | ` * echo $class;` |
|        - | 1790 | ` * ?>` |
|        - | 1791 | ` * The above example will output:` |
|        - | 1792 | ` *  Hello` |
|        - | 1793 | ` *` |
|        - | 1794 | ` * Note that PH7 does not support all the magical method and introudces __toFloat(),__toInt()` |
|        - | 1795 | ` * which have the same behaviour as __toString() but for float and integer types` |
|        - | 1796 | ` * respectively.` |
|        - | 1797 | ` * Refer to the official documentation for more information.` |
|        - | 1798 | ` */` |
|      346 | 1799 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallMagicMethod(` |
|        - | 1800 | `	ph7_vm *pVm,               /* VM that own all this stuff */` |
|        - | 1801 | `	ph7_class *pClass,         /* Target class */` |
|        - | 1802 | `	ph7_class_instance *pThis, /* Target object */` |
|        - | 1803 | `	const char *zMethod,       /* Magic method name [i.e: __toString()]*/` |
|        - | 1804 | `	sxu32 nByte,               /* zMethod length*/` |
|        - | 1805 | `	const SyString *pAttrName, /* Attribute name */` |
|        - | 1806 | `	ph7_value *pResult         /* OUT: magic method return value. NULL to discard */` |
|        - | 1807 | `	)` |
|        5 | 1808 | `{` |
|      351 | 1809 | `	ph7_value *apArg[2] = { 0 , 0 };` |
|        - | 1810 | `	ph7_class_method *pMeth;` |
|        - | 1811 | `	ph7_value sAttr; /* cc warning */` |
|        - | 1812 | `	sxi32 rc;` |
|        - | 1813 | `	int nArg;` |
|        - | 1814 | `	/* Make sure the magic method is available */` |
|      351 | 1815 | `	pMeth = PH7_ClassExtractMethod(&(*pClass),zMethod,nByte);` |
|      351 | 1816 | `	if( pMeth == 0 ){` |
|        - | 1817 | `		/* No such method,return immediately */` |
|      ! 0 | 1818 | `		return SXERR_NOTFOUND;` |
|        - | 1819 | `	}` |
|      351 | 1820 | `	nArg = 0;` |
|        - | 1821 | `	/* Copy arguments */` |
|      351 | 1822 | `	if( pAttrName ){` |
|      351 | 1823 | `		PH7_MemObjInitFromString(pVm,&sAttr,pAttrName);` |
|      351 | 1824 | `		sAttr.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      351 | 1825 | `		apArg[0] = &sAttr;` |
|      351 | 1826 | `		nArg = 1;` |
|      173 | 1827 | `	}` |
|        - | 1828 | `	/* Call the magic method now */` |
|      351 | 1829 | `	rc = PH7_VmCallMagicMethod(pVm,&(*pThis),pMeth,pResult,nArg,apArg);` |
|        - | 1830 | `	/* Clean up */` |
|      351 | 1831 | `	if( pAttrName ){` |
|      351 | 1832 | `		PH7_MemObjRelease(&sAttr);` |
|      173 | 1833 | `	}` |
|      351 | 1834 | `	return rc;` |
|      178 | 1835 | `}` |
|        - | 1836 | `/*` |
|        - | 1837 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon].` |
|        - | 1838 | ` * This function is simply a wrapper on ExtractClassAttrValue().` |
|        - | 1839 | ` */` |
|  5798120 | 1840 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceExtractAttrValue(ph7_class_instance *pThis,VmClassAttr *pAttr)` |
|        5 | 1841 | `{` |
|        - | 1842 | `   /* Extract the attribute value */` |
|        - | 1843 | `	ph7_value *pValue;` |
|  5798125 | 1844 | `	pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|  5798125 | 1845 | `	return pValue;` |
|        5 | 1846 | `}` |
|        - | 1847 | `/*` |
|        - | 1848 | ` * Convert a class instance [i.e: Object in the PHP jargon] into a hashmap [i.e: array in the PHP jargon].` |
|        - | 1849 | ` * Return SXRET_OK on success. Any other value indicates failure.` |
|        - | 1850 | ` * Note on object conversion to array:` |
|        - | 1851 | ` *  Acccording to the PHP language reference manual` |
|        - | 1852 | ` *  If an object is converted to an array, the result is an array whose elements are the object's properties.` |
|        - | 1853 | ` *  The keys are the member variable names.` |
|        - | 1854 | ` *` |
|        - | 1855 | ` *  The following example:` |
|        - | 1856 | ` *  class Test {` |
|        - | 1857 | ` *   public $A = 25<<1;  // 50` |
|        - | 1858 | ` *	 public $c = rand_str(3);   // Random string of length 3` |
|        - | 1859 | ` *	 public $d = rand() & 1023; // Random number between 0..1023` |
|        - | 1860 | ` *  }` |
|        - | 1861 | ` *  var_dump((array) new Test());` |
|        - | 1862 | ` *	Will output:` |
|        - | 1863 | ` *  array(3) {` |
|        - | 1864 | ` *   [A] =>` |
|        - | 1865 | ` *      int(50)` |
|        - | 1866 | ` *   [c] =>` |
|        - | 1867 | ` *     string(3 'aps')` |
|        - | 1868 | ` *   [d] =>` |
|        - | 1869 | ` *     int(991)` |
|        - | 1870 | ` *  }` |
|        - | 1871 | ` * You have noticed that PH7 allow class attributes [i.e: $a,$c,$d in the example above]` |
|        - | 1872 | ` * have any complex expression (even function calls/Annonymous functions) as their default` |
|        - | 1873 | ` * value unlike the standard PHP engine.` |
|        - | 1874 | ` * This is a very powerful feature that you have to look at.` |
|        - | 1875 | ` */` |
|       30 | 1876 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|        5 | 1877 | `{` |
|        - | 1878 | `	SyHashEntry *pEntry;` |
|        - | 1879 | `	SyString *pAttrName;` |
|        - | 1880 | `	VmClassAttr *pAttr;` |
|        - | 1881 | `	ph7_value *pValue;` |
|        - | 1882 | `	ph7_value sName;` |
|        - | 1883 | `	/* Reset the loop cursor */` |
|       35 | 1884 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|       35 | 1885 | `	PH7_MemObjInitFromString(pThis->pVm,&sName,0);` |
|      103 | 1886 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|        - | 1887 | `		/* Point to the current attribute */` |
|       73 | 1888 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|       73 | 1889 | `		if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|        - | 1890 | `			/* A static property is the CLASS's, not the object's: php's cast` |
|        - | 1891 | `			 * yields only the instance's own properties. */` |
|        3 | 1892 | `			continue;` |
|        - | 1893 | `		}` |
|       71 | 1894 | `		if( pAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|        - | 1895 | `			/* php 8.4: a VIRTUAL hooked property has no backing store — the` |
|        - | 1896 | `			 * (array) cast excludes it (raw surface, get is NOT dispatched) */` |
|        7 | 1897 | `			continue;` |
|        - | 1898 | `		}` |
|        - | 1899 | `		/* Extract attribute value */` |
|       65 | 1900 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|       65 | 1901 | `		if( pValue ){` |
|        - | 1902 | `			/* Build attribute name. php MANGLES the key of a non-public property` |
|        - | 1903 | `			 * when it casts an object to an array: a private one becomes` |
|        - | 1904 | `			 * "\0DeclaringClass\0name" and a protected one "\0*\0name", so two` |
|        - | 1905 | `			 * same-named members from different visibility levels stay distinct` |
|        - | 1906 | ``			 * and `isset($arr['priv'])` is FALSE — PHL emitted the bare name,`` |
|        - | 1907 | `			 * which collided them and answered TRUE. The NULs are real bytes in` |
|        - | 1908 | `			 * the key (this append is length-based, not NUL-terminated). */` |
|       65 | 1909 | `			pAttrName = &pAttr->pAttr->sName;` |
|       65 | 1910 | `			if( pAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       14 | 1911 | `				ph7_class *pDecl = pAttr->pAttr->pDeclClass` |
|        8 | 1912 | `					? pAttr->pAttr->pDeclClass : pThis->pClass;` |
|       10 | 1913 | `				PH7_MemObjStringAppend(&sName,"\0",1);` |
|       10 | 1914 | `				PH7_MemObjStringAppend(&sName,SyStringData(&pDecl->sName),SyStringLength(&pDecl->sName));` |
|       10 | 1915 | `				PH7_MemObjStringAppend(&sName,"\0",1);` |
|       61 | 1916 | `			}else if( pAttr->pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
|       11 | 1917 | `				PH7_MemObjStringAppend(&sName,"\0*\0",3);` |
|        4 | 1918 | `			}` |
|       65 | 1919 | `			PH7_MemObjStringAppend(&sName,pAttrName->zString,pAttrName->nByte);` |
|        - | 1920 | `			/* Perform the insertion */` |
|       65 | 1921 | `			PH7_HashmapInsert(pMap,&sName,pValue);` |
|        - | 1922 | `			/* Reset the string cursor */` |
|       65 | 1923 | `			SyBlobReset(&sName.sBlob);` |
|       30 | 1924 | `		}` |
|        5 | 1925 | `	}` |
|       35 | 1926 | `	PH7_MemObjRelease(&sName);` |
|       35 | 1927 | `	return SXRET_OK;` |
|        5 | 1928 | `}` |
|        - | 1929 | `/*` |
|        - | 1930 | ` * Iterate throw class attributes and invoke the given callback [i.e: xWalk()] for each` |
|        - | 1931 | ` * retrieved attribute.` |
|        - | 1932 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|        - | 1933 | ` * the attribute value in the callback body will not alter the real attribute value.` |
|        - | 1934 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|        - | 1935 | ` * a value different from PH7_OK.` |
|        - | 1936 | ` * Refer to [ph7_object_walk()] for more information.` |
|        - | 1937 | ` */` |
|      ! 0 | 1938 | `PH7_PRIVATE sxi32 PH7_ClassInstanceWalk(` |
|        - | 1939 | `	ph7_class_instance *pThis, /* Target object */` |
|        - | 1940 | `	int (*xWalk)(const char *,ph7_value *,void *), /* Walker callback */` |
|        - | 1941 | `	void *pUserData /* Last argument to xWalk() */` |
|        - | 1942 | `	)` |
|      ! 0 | 1943 | `{` |
|        - | 1944 | `	SyHashEntry *pEntry; /* Hash entry */` |
|        - | 1945 | `	VmClassAttr *pAttr;  /* Pointer to the attribute */` |
|        - | 1946 | `	ph7_value *pValue;   /* Attribute value */` |
|        - | 1947 | `	ph7_value sValue;    /* Copy of the attribute value */` |
|        - | 1948 | `	int rc;` |
|        - | 1949 | `	/* Reset the loop cursor */` |
|      ! 0 | 1950 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      ! 0 | 1951 | `	PH7_MemObjInit(pThis->pVm,&sValue);` |
|        - | 1952 | `	/* Start the walk process */` |
|      ! 0 | 1953 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|        - | 1954 | `		/* Point to the current attribute */` |
|      ! 0 | 1955 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      ! 0 | 1956 | `		if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|        - | 1957 | `			/* Class-level members are not part of the object (php) */` |
|      ! 0 | 1958 | `			continue;` |
|        - | 1959 | `		}` |
|        - | 1960 | `		/* Extract attribute value */` |
|      ! 0 | 1961 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|      ! 0 | 1962 | `		if( pValue ){` |
|      ! 0 | 1963 | `			PH7_MemObjLoad(pValue,&sValue);` |
|        - | 1964 | `			/* Invoke the supplied callback */` |
|      ! 0 | 1965 | `			rc =  xWalk(SyStringData(&pAttr->pAttr->sName),&sValue,pUserData);` |
|      ! 0 | 1966 | `			PH7_MemObjRelease(&sValue);` |
|      ! 0 | 1967 | `			if( rc != PH7_OK){` |
|        - | 1968 | `				/* User callback request an operation abort */` |
|      ! 0 | 1969 | `				return SXERR_ABORT;` |
|        - | 1970 | `			}` |
|      ! 0 | 1971 | `		}` |
|      ! 0 | 1972 | `	}` |
|        - | 1973 | `	/* All done */` |
|      ! 0 | 1974 | `	return SXRET_OK;` |
|      ! 0 | 1975 | `}` |
|        - | 1976 | `/*` |
|        - | 1977 | ` * Extract a class atrribute value.` |
|        - | 1978 | ` * Return a pointer to the attribute value on success. Otherwise NULL.` |
|        - | 1979 | ` * Note:` |
|        - | 1980 | ` *  Access to static and constant attribute is not allowed. That is,the function` |
|        - | 1981 | ` *  will return NULL in case someone (host-application code) try to extract` |
|        - | 1982 | ` *  a static/constant attribute.` |
|        - | 1983 | ` */` |
|    16952 | 1984 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceFetchAttr(ph7_class_instance *pThis,const SyString *pName)` |
|        5 | 1985 | `{` |
|        - | 1986 | `	SyHashEntry *pEntry;` |
|        - | 1987 | `	VmClassAttr *pAttr;` |
|        - | 1988 | `	/* Query the attribute hashtable */` |
|    16957 | 1989 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);` |
|    16957 | 1990 | `	if( pEntry == 0 ){` |
|        - | 1991 | `		/* No such attribute */` |
|      ! 0 | 1992 | `		return 0;` |
|        - | 1993 | `	}` |
|        - | 1994 | `	/* Point to the class atrribute */` |
|    16957 | 1995 | `	pAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - | 1996 | `	/* Check if we are dealing with a static/constant attribute */` |
|    16957 | 1997 | `	if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - | 1998 | `		/* Access is forbidden */` |
|      ! 0 | 1999 | `		return 0;` |
|        - | 2000 | `	}` |
|        - | 2001 | `	/* Return the attribute value */` |
|    16957 | 2002 | `	return ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     8481 | 2003 | `}` |
|        - | 2004 | `/*` |
|        - | 2005 | ` * The three accessors a VM_FUNC_NATIVE method body uses to reach its receiver.` |
|        - | 2006 | ` *` |
|        - | 2007 | ` * A native method is dispatched down the host-function path, so it is handed the` |
|        - | 2008 | ` * plain (ph7_context*, argc, argv) of any builtin — the receiver rides on the` |
|        - | 2009 | ` * context instead of occupying an argument slot. That is the whole point: the C` |
|        - | 2010 | `` * routine that used to be a global `__prefix_verb($target, ...)` thunk taking its`` |
|        - | 2011 | ` * target explicitly becomes a method whose target is $this.` |
|        - | 2012 | ` */` |
|        - | 2013 | `/*` |
|        - | 2014 | ` * The receiver, or NULL when the method was called statically (and always NULL in` |
|        - | 2015 | ` * a plain host function). Borrowed: the operand stack holds the reference for the` |
|        - | 2016 | ` * duration of the call, so the body must not unref it.` |
|        - | 2017 | ` */` |
|       52 | 2018 | `PH7_PRIVATE ph7_class_instance * PH7_ContextThis(ph7_context *pCtx)` |
|        2 | 2019 | `{` |
|       54 | 2020 | `	return pCtx->pThis;` |
|        2 | 2021 | `}` |
|        - | 2022 | `/*` |
|        - | 2023 | ` * The class the call was made THROUGH — php's late-static-binding target, so an` |
|        - | 2024 | ` * inherited native method sees the SUBCLASS here, not the class that declared it.` |
|        - | 2025 | ` * NULL in a plain host function.` |
|        - | 2026 | ` */` |
|      ! 0 | 2027 | `PH7_PRIVATE ph7_class * PH7_ContextCalledClass(ph7_context *pCtx)` |
|      ! 0 | 2028 | `{` |
|      ! 0 | 2029 | `	return pCtx->pCalledClass;` |
|      ! 0 | 2030 | `}` |
|        - | 2031 | `/*` |
|        - | 2032 | ` * The receiver as a ph7_value, so the body can use the ordinary object helpers` |
|        - | 2033 | ` * (ph7_object_fetch_attr, ph7_value_is_object, ph7_result_value, ...) instead of` |
|        - | 2034 | ` * reaching into ph7_class_instance. NULL when there is no receiver.` |
|        - | 2035 | ` *` |
|        - | 2036 | ` * The view ALIASES the instance without bumping its refcount: it lives exactly as` |
|        - | 2037 | ` * long as the call, during which the caller's reference is already keeping the` |
|        - | 2038 | ` * object alive, and VmReleaseCallContext drops the alias without unreffing. Handing` |
|        - | 2039 | ` * it to ph7_result_value() is safe — that copies through PH7_MemObjStore, which` |
|        - | 2040 | ` * takes its own reference.` |
|        - | 2041 | ` */` |
|     5468 | 2042 | `PH7_PRIVATE ph7_value * PH7_ContextThisValue(ph7_context *pCtx)` |
|        5 | 2043 | `{` |
|     5473 | 2044 | `	if( pCtx->pThis == 0 ){` |
|      ! 0 | 2045 | `		return 0;` |
|        - | 2046 | `	}` |
|     5473 | 2047 | `	if( !pCtx->bThisInit ){` |
|     5473 | 2048 | `		PH7_MemObjInit(pCtx->pVm,&pCtx->sThis);` |
|     5473 | 2049 | `		pCtx->sThis.x.pOther = pCtx->pThis;` |
|     5473 | 2050 | `		pCtx->sThis.iFlags = MEMOBJ_OBJ;` |
|     5473 | 2051 | `		pCtx->sThis.nIdx = SXU32_HIGH;` |
|     5473 | 2052 | `		pCtx->bThisInit = 1;` |
|     2734 | 2053 | `	}` |
|     5473 | 2054 | `	return &pCtx->sThis;` |
|     2739 | 2055 | `}` |
|        - | 2056 |  |
