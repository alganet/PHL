# src/ph7/oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1707/1936 lines (88.17%)

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
|  1741489 |   14 | `PH7_PRIVATE ph7_class * PH7_NewRawClass(ph7_vm *pVm,const SyString *pName,sxu32 nLine)` |
|        5 |   15 | `{` |
|        - |   16 | `	ph7_class *pClass;` |
|        - |   17 | `	char *zName;` |
|        - |   18 | `	/* Allocate a new instance */` |
|  1741494 |   19 | `	pClass = (ph7_class *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class));` |
|  1741494 |   20 | `	if( pClass == 0 ){` |
|      ! 0 |   21 | `		return 0;` |
|        - |   22 | `	}` |
|        - |   23 | `	/* Zero the structure */` |
|  1741494 |   24 | `	SyZero(pClass,sizeof(ph7_class));` |
|        - |   25 | `	/* Duplicate class name */` |
|  1741494 |   26 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1741494 |   27 | `	if( zName == 0 ){` |
|      ! 0 |   28 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClass);` |
|      ! 0 |   29 | `		return 0;` |
|        - |   30 | `	}` |
|        - |   31 | `	/* Initialize fields */` |
|  1741494 |   32 | `	SyStringInitFromBuf(&pClass->sName,zName,pName->nByte);` |
|        - |   33 | `	/* The DISPLAY name: sName up to its first NUL. Only an anonymous class has one` |
|        - |   34 | ``	 * (PH7_CompileAnnonClass synthesizes php's `<prefix>@anonymous\0file:line$hex`),`` |
|        - |   35 | `	 * so for every other class this aliases the whole name and costs one scan of it` |
|        - |   36 | `	 * at declaration time. */` |
|        - |   37 | `	{` |
|  1741494 |   38 | `		sxu32 nCut = pName->nByte;` |
|  1741494 |   39 | `		if( pName->nByte > 0 ){` |
|  1741494 |   40 | `			sxu32 nPos = 0;` |
|  1741494 |   41 | `			if( SyByteFind(zName,pName->nByte,0,&nPos) == SXRET_OK ){` |
|      183 |   42 | `				nCut = nPos;` |
|       89 |   43 | `			}` |
|   869540 |   44 | `		}` |
|  1741494 |   45 | `		SyStringInitFromBuf(&pClass->sDisp,zName,nCut);` |
|        - |   46 | `	}` |
|        - |   47 | `	/* php method names are CASE-INSENSITIVE ($o->FOO() finds foo(), and declaring` |
|        - |   48 | `	 * both is a redeclaration), so the method table matches on them the same way` |
|        - |   49 | `	 * hClass does for class names. Properties and class constants ARE case` |
|        - |   50 | `	 * sensitive in php, so hAttr keeps the default exact comparator. */` |
|  1741494 |   51 | `	SyHashInit(&pClass->hMethod,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|  1741494 |   52 | `	SyHashInit(&pClass->hAttr,&pVm->sAllocator,0,0);` |
|  1741494 |   53 | `	SyHashInit(&pClass->hConst,&pVm->sAllocator,0,0);` |
|  1741494 |   54 | `	SyHashInit(&pClass->hDerived,&pVm->sAllocator,0,0);` |
|  1741494 |   55 | `	SySetInit(&pClass->aInterface,&pVm->sAllocator,sizeof(ph7_class *));` |
|  1741494 |   56 | `	SySetInit(&pClass->aTrait,&pVm->sAllocator,sizeof(ph7_class *));` |
|  1741494 |   57 | `	SySetInit(&pClass->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|  1741494 |   58 | `	SySetInit(&pClass->aEnumCases,&pVm->sAllocator,sizeof(ph7_class_attr *));` |
|  1741494 |   59 | `	pClass->nLine = nLine;` |
|  1741494 |   60 | `	if( pVm->bCompilingBuiltin ){` |
|        - |   61 | `		/* Defined by an embedded builtin chunk: internal, no defining file.` |
|        - |   62 | `		 * Class compilers merge further flags with \|= so this survives. */` |
|  1735580 |   63 | `		pClass->iFlags \|= PH7_CLASS_INTERNAL;` |
|   866588 |   64 | `	}else{` |
|        - |   65 | `		/* Alias the VM-lifetime path dup on top of the include stack */` |
|     5919 |   66 | `		SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     5919 |   67 | `		if( pFile ){` |
|     5919 |   68 | `			SyStringDupPtr(&pClass->sFile,pFile);` |
|     2957 |   69 | `		}` |
|        - |   70 | `	}` |
|        - |   71 | `	/* All done */` |
|  1741494 |   72 | `	return pClass;` |
|   869545 |   73 | `}` |
|        - |   74 | `/*` |
|        - |   75 | ` * Allocate and initialize a new class attribute.` |
|        - |   76 | ` * Return a pointer to the class attribute on success. NULL otherwise.` |
|        - |   77 | ` */` |
|  6312224 |   78 | `PH7_PRIVATE ph7_class_attr * PH7_NewClassAttr(ph7_vm *pVm,const SyString *pName,sxu32 nLine,sxi32 iProtection,sxi32 iFlags)` |
|        5 |   79 | `{` |
|        - |   80 | `	ph7_class_attr *pAttr;` |
|        - |   81 | `	char *zName;` |
|  6312229 |   82 | `	pAttr = (ph7_class_attr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_attr));` |
|  6312229 |   83 | `	if( pAttr == 0 ){` |
|      ! 0 |   84 | `		return 0;` |
|        - |   85 | `	}` |
|        - |   86 | `	/* Zero the structure */` |
|  6312229 |   87 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|  6312229 |   88 | `	SySetInit(&pAttr->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|        - |   89 | `	/* Duplicate attribute name */` |
|  6312229 |   90 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  6312229 |   91 | `	if( zName == 0 ){` |
|      ! 0 |   92 | `		SyMemBackendPoolFree(&pVm->sAllocator,pAttr);` |
|      ! 0 |   93 | `		return 0;` |
|        - |   94 | `	}` |
|        - |   95 | `	/* Initialize fields */` |
|  6312229 |   96 | `	SySetInit(&pAttr->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|  6312229 |   97 | `	SySetInit(&pAttr->aUnionAlts,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|  6312229 |   98 | `	SyStringInitFromBuf(&pAttr->sName,zName,pName->nByte);` |
|  6312229 |   99 | `	pAttr->iProtection = iProtection;` |
|  6312229 |  100 | `	pAttr->nIdx = SXU32_HIGH;` |
|  6312229 |  101 | `	pAttr->iFlags = iFlags;` |
|  6312229 |  102 | `	pAttr->nLine = nLine;` |
|  6312229 |  103 | `	return pAttr;` |
|  3151739 |  104 | `}` |
|        - |  105 | `/*` |
|        - |  106 | ` * Allocate and initialize a new class method.` |
|        - |  107 | ` * Return a pointer to the class method on success. NULL otherwise` |
|        - |  108 | ` * This function associate with the newly created method an automatically generated` |
|        - |  109 | ` * random unique name.` |
|        - |  110 | ` */` |
| 11163920 |  111 | `PH7_PRIVATE ph7_class_method * PH7_NewClassMethod(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,sxu32 nLine,` |
|        - |  112 | `	sxi32 iProtection,sxi32 iFlags,sxi32 iFuncFlags)` |
|        5 |  113 | `{` |
|        - |  114 | `	ph7_class_method *pMeth;` |
|        - |  115 | `	SyHashEntry *pEntry;` |
|        - |  116 | `	SyString *pNamePtr;` |
|        - |  117 | `	char zSalt[10];` |
|        - |  118 | `	char *zName;` |
|        - |  119 | `	sxu32 nByte;` |
|        - |  120 | `	/* Allocate a new class method instance */` |
| 11163925 |  121 | `	pMeth = (ph7_class_method *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_method));` |
| 11163925 |  122 | `	if( pMeth == 0 ){` |
|      ! 0 |  123 | `		return 0;` |
|        - |  124 | `	}` |
|        - |  125 | `	/* Zero the structure */` |
| 11163925 |  126 | `	SyZero(pMeth,sizeof(ph7_class_method));` |
|        - |  127 | `	/* Check for an already installed method with the same name */` |
| 11163925 |  128 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
| 11163925 |  129 | `	if( pEntry == 0 ){` |
|        - |  130 | `		/* Associate an unique VM name to this method */` |
| 11163919 |  131 | `		nByte = sizeof(zSalt) + pName->nByte + SyStringLength(&pClass->sName)+sizeof(char)*7/*[[__'\0'*/;` |
| 11163919 |  132 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,nByte);` |
| 11163919 |  133 | `		if( zName == 0 ){` |
|      ! 0 |  134 | `			SyMemBackendPoolFree(&pVm->sAllocator,pMeth);` |
|      ! 0 |  135 | `			return 0;` |
|        - |  136 | `		}` |
| 11163919 |  137 | `		pNamePtr = &pMeth->sVmName;` |
|        - |  138 | `		/* Generate a random string */` |
| 11163919 |  139 | `		PH7_VmRandomString(&(*pVm),zSalt,sizeof(zSalt));` |
| 11163919 |  140 | `		pNamePtr->nByte = SyBufferFormat(zName,nByte,"[__%z@%z_%.*s]",&pClass->sName,pName,sizeof(zSalt),zSalt);` |
| 11163919 |  141 | `		pNamePtr->zString = zName;` |
|  5574218 |  142 | `	}else{` |
|        - |  143 | `		/* Method is condidate for 'overloading' */` |
|        8 |  144 | `		ph7_class_method *pCurrent = (ph7_class_method *)pEntry->pUserData;` |
|        8 |  145 | `		pNamePtr = &pMeth->sVmName;` |
|        - |  146 | `		/* Use the same VM name */` |
|        8 |  147 | `		SyStringDupPtr(pNamePtr,&pCurrent->sVmName);` |
|        8 |  148 | `		zName = (char *)pNamePtr->zString;` |
|        - |  149 | `	}` |
|        - |  150 | `` 	/* Every method keeps the visibility it was DECLARED with, `__destruct` `` |
|        - |  151 | `	 * included. It used to be forced public here "because the engine invokes it` |
|        - |  152 | `	 * internally" -- but the engine's teardown reaches it through` |
|        - |  153 | `	 * PH7_VmCallClassMethod, which never consults the visibility, so the force` |
|        - |  154 | `	 * bought nothing and cost the declaration: a private destructor reflected as` |
|        - |  155 | `	 * public (isPrivate() false, modifiers 1), was listed by get_class_methods()` |
|        - |  156 | ``	 * from outside the class, printed `private` nowhere in its Reflection export,`` |
|        - |  157 | ``	 * and could be called as `$o->__destruct()` from any scope. __construct has`` |
|        - |  158 | `	 * kept its declared visibility since band A #4, and php enforces that one at` |
|        - |  159 | ``	 * `new`; a method named like the class is a PLAIN method (PHP-4 constructors`` |
|        - |  160 | `	 * removed in 8.0) and keeps its own too. */` |
|        - |  161 | `	/* Initialize method fields */` |
| 11163925 |  162 | `	pMeth->iProtection = iProtection;` |
| 11163925 |  163 | `	pMeth->iFlags = iFlags;` |
| 11163925 |  164 | `	pMeth->nLine = nLine;` |
| 16738141 |  165 | `	PH7_VmInitFuncState(&(*pVm),&pMeth->sFunc,&zName[sizeof(char)*4/*[__@*/+SyStringLength(&pClass->sName)],` |
| 11163920 |  166 | `		pName->nByte,iFuncFlags\|VM_FUNC_CLASS_METHOD,pClass);` |
| 11163925 |  167 | `	return pMeth;` |
|  5574221 |  168 | `}` |
|        - |  169 | `/*` |
|        - |  170 | ` * Check if the given name have a class method associated with it.` |
|        - |  171 | ` * Return the desired method [i.e: ph7_class_method instance] on success. NULL otherwise.` |
|        - |  172 | ` */` |
|  6991955 |  173 | `PH7_PRIVATE ph7_class_method * PH7_ClassExtractMethod(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|        5 |  174 | `{` |
|        - |  175 | `	SyHashEntry *pEntry;` |
|        - |  176 | `	/* Perform a hash lookup */` |
|  6991960 |  177 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)zName,nByte);` |
|  6991960 |  178 | `	if( pEntry == 0 ){` |
|        - |  179 | `		/* No such entry */` |
|  1645031 |  180 | `		return 0;` |
|        - |  181 | `	}` |
|        - |  182 | `	/* Point to the desired method */` |
|  5346934 |  183 | `	return (ph7_class_method *)pEntry->pUserData;` |
|  3495595 |  184 | `}` |
|        - |  185 | `/*` |
|        - |  186 | ` * Check if the given name is a class attribute.` |
|        - |  187 | ` * Return the desired attribute [i.e: ph7_class_attr instance] on success.NULL otherwise.` |
|        - |  188 | ` */` |
|   107794 |  189 | `PH7_PRIVATE ph7_class_attr * PH7_ClassExtractAttribute(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|        5 |  190 | `{` |
|        - |  191 | `	SyHashEntry *pEntry;` |
|        - |  192 | `	/* Perform a hash lookup */` |
|   107799 |  193 | `	pEntry = SyHashGet(&pClass->hAttr,(const void *)zName,nByte);` |
|   107799 |  194 | `	if( pEntry == 0 ){` |
|        - |  195 | `		/* No such entry */` |
|     3697 |  196 | `		return 0;` |
|        - |  197 | `	}` |
|        - |  198 | `	/* Point to the desierd method */` |
|   104107 |  199 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|    53902 |  200 | `}` |
|        - |  201 | `/*` |
|        - |  202 | ` * php's MANGLED storage name for one property, as an instance of pClass files it.` |
|        - |  203 | ` *` |
|        - |  204 | `` * php makes a split this engine did not: `ce->properties_info` is keyed by the`` |
|        - |  205 | ` * PLAIN name and is where every visibility decision is made, while the object's` |
|        - |  206 | ` * own slot table is keyed by a MANGLED one -- "\0DeclaringClass\0name" for a` |
|        - |  207 | ` * private property, the bare name for everything else. Two things follow from it,` |
|        - |  208 | ` * and both were wrong here.` |
|        - |  209 | ` *` |
|        - |  210 | `` * `class A { private $q; } class B extends A { private $q; }` has TWO slots on one`` |
|        - |  211 | ` * object, each reachable only from its own declaring class, where PHL had one --` |
|        - |  212 | ` * so a base method reading its own private got the CHILD's value, with nothing to` |
|        - |  213 | ` * announce it. And a base's private is INVISIBLE from outside rather than merely` |
|        - |  214 | ` * inaccessible: a lookup by the plain name finds nothing at all, which is why php` |
|        - |  215 | `` * answers `$b->q` with "Undefined property: B::$q" and not with the visibility`` |
|        - |  216 | `` * refusal it words for `$a->q`.`` |
|        - |  217 | ` *` |
|        - |  218 | ` * The declaring class is fixed per attribute, so the mangled name is built once` |
|        - |  219 | ` * and cached on it. A property this class DECLARED keeps its plain name -- php` |
|        - |  220 | ` * mangles the storage name there too, but nothing else in this engine ever sees` |
|        - |  221 | ` * the difference, and the plain key is what every lookup that does not know about` |
|        - |  222 | ` * a scope already asks for.` |
|        - |  223 | ` */` |
|  6390715 |  224 | `PH7_PRIVATE const SyString * PH7_ClassAttrStorageName(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        5 |  225 | `{` |
|        - |  226 | `	ph7_class *pDecl;` |
|        - |  227 | `	sxu32 nCls,nName;` |
|        - |  228 | `	char *zKey;` |
|  6390715 |  229 | `	if( pAttr->iProtection != PH7_CLASS_PROT_PRIVATE` |
|  4433625 |  230 | `	 \|\| (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_DYNAMIC)) != 0 ){` |
|  3908795 |  231 | `		return &pAttr->sName;` |
|        - |  232 | `	}` |
|  2481930 |  233 | `	pDecl = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|  2481930 |  234 | `	if( pDecl == 0 \|\| pDecl == pClass ){` |
|       17 |  235 | `		return &pAttr->sName;` |
|        - |  236 | `	}` |
|  2481914 |  237 | `	if( pDecl->iFlags & PH7_CLASS_INTERNAL ){` |
|        - |  238 | `		/* An ENGINE class's slot keeps its plain name on every object below it:` |
|        - |  239 | `		 * the C bodies that own that storage address it by name (a DateTime's` |
|        - |  240 | `		 * timestamp, a PDOStatement's handle), and a subclass instance whose slots` |
|        - |  241 | `		 * were renamed read as an object whose parent constructor never ran. php` |
|        - |  242 | `		 * mangles an internal private too; nothing here can see the difference,` |
|        - |  243 | `		 * because an engine class's private is not a name user code declares. */` |
|  2481766 |  244 | `		return &pAttr->sName;` |
|        - |  245 | `	}` |
|      152 |  246 | `	if( SyStringLength(&pAttr->sStoreName) > 0 ){` |
|      102 |  247 | `		return &pAttr->sStoreName;` |
|        - |  248 | `	}` |
|       52 |  249 | `	nCls = SyStringLength(&pDecl->sName);` |
|       52 |  250 | `	nName = SyStringLength(&pAttr->sName);` |
|        - |  251 | `	/* Class-lifetime, like sName's own dup: an attribute outlives every instance` |
|        - |  252 | `	 * whose table points at this key. */` |
|       52 |  253 | `	zKey = (char *)SyMemBackendAlloc(&pVm->sAllocator,nCls + nName + 3);` |
|       52 |  254 | `	if( zKey == 0 ){` |
|      ! 0 |  255 | `		return &pAttr->sName;` |
|        - |  256 | `	}` |
|       52 |  257 | `	zKey[0] = 0;` |
|       52 |  258 | `	SyMemcpy((const void *)SyStringData(&pDecl->sName),(void *)&zKey[1],nCls);` |
|       52 |  259 | `	zKey[1+nCls] = 0;` |
|       52 |  260 | `	SyMemcpy((const void *)SyStringData(&pAttr->sName),(void *)&zKey[nCls+2],nName);` |
|       52 |  261 | `	zKey[nCls+nName+2] = 0;` |
|       52 |  262 | `	SyStringInitFromBuf(&pAttr->sStoreName,zKey,nCls + nName + 2);` |
|       52 |  263 | `	return &pAttr->sStoreName;` |
|  3190935 |  264 | `}` |
|        - |  265 | `/*` |
|        - |  266 | ` * One bit of sixty-four for a property NAME, taken from the name's own hash.` |
|        - |  267 | ` *` |
|        - |  268 | ` * The two masks below are how a property access decides, without a lookup, that no` |
|        - |  269 | ` * mangled slot can be involved. They have to be SHARP: a mask built from a few` |
|        - |  270 | ` * bytes of the name collides with whatever ordinary property a class reads in its` |
|        - |  271 | ` * hot loop, and one unlucky pair then puts the whole workload back on the slow` |
|        - |  272 | ` * path -- measured, 65.3 million times on the phpcs step, out of 65.8 million that` |
|        - |  273 | ` * got past a length-and-three-bytes signature. So the bit comes from the same hash` |
|        - |  274 | ` * the lookup itself uses, which the caller has already computed for the instance` |
|        - |  275 | ` * probe (SyHashKey) and hands down.` |
|        - |  276 | ` *` |
|        - |  277 | ` * Every side of this asks a class's hAttr for the hash rather than naming a hash` |
|        - |  278 | ` * function, because a bit set under one function and tested under another would` |
|        - |  279 | ` * make a scope's private stop resolving -- silently, and only for the names that` |
|        - |  280 | ` * collide. All the property tables share one function; this is what keeps that a` |
|        - |  281 | ` * fact rather than an assumption.` |
|        - |  282 | ` */` |
|  1149713 |  283 | `static sxu64 OoShadowNameBit(sxu32 nHash)` |
|        5 |  284 | `{` |
|  1149718 |  285 | `	return ((sxu64)1) << (nHash & 63);` |
|        5 |  286 | `}` |
|        - |  287 | `/*` |
|        - |  288 | ` * Note, on the class that DECLARES it, that this name is one of its own private` |
|        - |  289 | ` * instance properties. Called from every path that files an attribute in a class's` |
|        - |  290 | ` * hAttr under its plain name.` |
|        - |  291 | ` *` |
|        - |  292 | ` * Only an instance property counts: a private STATIC and a private CONSTANT are` |
|        - |  293 | ` * never reached through an object's slot table, and OoScopePrivateAttr refuses` |
|        - |  294 | ` * both explicitly.` |
|        - |  295 | ` */` |
|  3014710 |  296 | `PH7_PRIVATE void PH7_ClassNotePrivateName(ph7_class *pClass,ph7_class_attr *pAttr)` |
|        5 |  297 | `{` |
|  3014710 |  298 | `	if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|  2080820 |  299 | `	 && (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|  1723429 |  300 | `		pClass->nPrivName \|= OoShadowNameBit(SyHashKey(&pClass->hAttr,` |
|  1149481 |  301 | `			(const void *)SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName)));` |
|   573943 |  302 | `	}` |
|  3014715 |  303 | `}` |
|        - |  304 | `/*` |
|        - |  305 | `` * Which PROPERTY does `name` mean, seen from the class whose code is RUNNING?`` |
|        - |  306 | ` *` |
|        - |  307 | ` * php's zend_get_parent_private_property: a scope that declares a private of this` |
|        - |  308 | ` * name owns a slot of its own on every instance below it, and that slot -- not` |
|        - |  309 | ` * whatever the object's class holds under the plain name -- is what its code` |
|        - |  310 | ` * means. Answers 0 when the executing scope has no such private, which leaves the` |
|        - |  311 | ` * caller on the ordinary plain-name path.` |
|        - |  312 | ` */` |
|   124459 |  313 | `static ph7_class_attr * OoScopePrivateAttr(ph7_vm *pVm,ph7_class *pClass,const char *zName,` |
|        - |  314 | `	sxu32 nName,sxu32 nHash)` |
|        5 |  315 | `{` |
|        - |  316 | `	ph7_class *pScope;` |
|        - |  317 | `	SyHashEntry *pEntry;` |
|        - |  318 | `	ph7_class_attr *pOwn;` |
|        - |  319 | `	sxu64 nBit;` |
|   124464 |  320 | `	if( nName < 1 ){` |
|       65 |  321 | `		return 0;` |
|        - |  322 | `	}` |
|   124400 |  323 | `	if( (pClass->iFlags & PH7_CLASS_SHADOW_PROP) == 0 ){` |
|        - |  324 | `		/* No property of this class is filed under a mangled name, so it holds no` |
|        - |  325 | `		 * slot the plain probe cannot reach. Reaching one at all takes a scope this` |
|        - |  326 | `		 * class DESCENDS from, and inheriting that scope's private is exactly what` |
|        - |  327 | `		 * sets the flag -- so this is the whole test, and every ordinary property` |
|        - |  328 | `		 * access skips the frame walk below on it. */` |
|   124226 |  329 | `		return 0;` |
|        - |  330 | `	}` |
|      178 |  331 | `	nBit = OoShadowNameBit(nHash);` |
|      178 |  332 | `	if( (pClass->nShadowName & nBit) == 0 ){` |
|        - |  333 | `		/* ...and the flag alone is not enough. A class that inherits ONE private` |
|        - |  334 | `		 * property pays for the walk below on every access to every OTHER property` |
|        - |  335 | `		 * it has, and on the phpcs step that was 79.4 million hash lookups made to` |
|        - |  336 | `		 * answer "no" -- 99.4% of the ones this function made, and 14% of every` |
|        - |  337 | `		 * lookup the engine did. The mask names the plain names a mangled slot` |
|        - |  338 | `		 * could be hiding under, so a miss is the complete answer: nothing can be` |
|        - |  339 | `		 * reached under a name this class holds no mangled slot for. */` |
|       24 |  340 | `		return 0;` |
|        - |  341 | `	}` |
|      156 |  342 | `	pScope = PH7_VmCallerScope(&(*pVm));` |
|      156 |  343 | `	if( pScope == 0 \|\| pScope == pClass ){` |
|       68 |  344 | `		return 0;   /* global scope, or the object's own class: the plain name IS the slot */` |
|        - |  345 | `	}` |
|       89 |  346 | `	if( (pScope->nPrivName & nBit) == 0 ){` |
|        - |  347 | `		/* ...and the same question from the other side, which is the one that` |
|        - |  348 | `		 * decides: the scope can only mean a mangled slot for a name it declares` |
|        - |  349 | `		 * PRIVATE itself. A class inherits many more mangled names than it` |
|        - |  350 | `		 * declares private ones, so this mask is the sparser of the two, and the` |
|        - |  351 | `		 * pair of them is what leaves this lookup to the accesses that need it. */` |
|      ! 0 |  352 | `		return 0;` |
|        - |  353 | `	}` |
|       89 |  354 | `	pEntry = SyHashGetHashed(&pScope->hAttr,(const void *)zName,nName,nHash);` |
|       89 |  355 | `	pOwn = pEntry ? (ph7_class_attr *)pEntry->pUserData : 0;` |
|       88 |  356 | `	if( pOwn == 0` |
|       88 |  357 | `	 \|\| pOwn->iProtection != PH7_CLASS_PROT_PRIVATE` |
|       87 |  358 | `	 \|\| (pOwn->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) != 0` |
|       86 |  359 | `	 \|\| PH7_VmMemberOwnerClass(pOwn->pDeclClass,pScope) != pScope` |
|       87 |  360 | `	 \|\| !PH7_VmInstanceOf(pClass,pScope) ){` |
|        3 |  361 | `		return 0;` |
|        - |  362 | `	}` |
|       87 |  363 | `	return pOwn;` |
|    62235 |  364 | `}` |
|        - |  365 | `/*` |
|        - |  366 | ` * php presents an object's properties by their PLAIN names, so two slots that` |
|        - |  367 | ` * unmangle to the same one -- a base's private and the subclass's own property --` |
|        - |  368 | `` * collide on every surface that walks the object BY NAME: `foreach` and`` |
|        - |  369 | ` * get_object_vars(). php keeps the FIRST accessible one in storage order and drops` |
|        - |  370 | ` * the rest, which is base-first, so a base method iterating a subclass instance` |
|        - |  371 | `` * sees its OWN `$q` and never the child's. Only the RAW surfaces show both --`` |
|        - |  372 | ` * (array), serialize(), var_dump(), get_mangled_object_vars() -- and those key by` |
|        - |  373 | ` * the mangled name, where nothing collides.` |
|        - |  374 | ` *` |
|        - |  375 | ` * TRUE when an EARLIER entry of this object's table carries the same plain name` |
|        - |  376 | ` * and is itself accessible from here.` |
|        - |  377 | ` */` |
|      990 |  378 | `PH7_PRIVATE int PH7_ClassInstanceAttrShadowed(ph7_vm *pVm,ph7_class_instance *pThis,SyHashEntry *pEntry)` |
|        5 |  379 | `{` |
|      995 |  380 | `	VmClassAttr *pMe = (VmClassAttr *)pEntry->pUserData;` |
|        - |  381 | `	SyString *pName;` |
|        - |  382 | `	SyHashEntry *pWalk;` |
|      995 |  383 | `	if( (pThis->pClass->iFlags & PH7_CLASS_SHADOW_PROP) == 0 \|\| pMe == 0 ){` |
|      977 |  384 | `		return 0;   /* no mangled slot on this class: no name can collide */` |
|        - |  385 | `	}` |
|       19 |  386 | `	pName = &pMe->pAttr->sName;` |
|       23 |  387 | `	for( pWalk = SyHashFirstEntry(&pThis->hAttr) ; pWalk && pWalk != pEntry ;` |
|        5 |  388 | `	     pWalk = SyHashEntryNext(pWalk) ){` |
|        9 |  389 | `		VmClassAttr *pOther = (VmClassAttr *)pWalk->pUserData;` |
|        9 |  390 | `		if( pOther == 0 \|\| pOther->pAttr == pMe->pAttr ){` |
|      ! 0 |  391 | `			continue;` |
|        - |  392 | `		}` |
|        8 |  393 | `		if( SyStringLength(&pOther->pAttr->sName) != SyStringLength(pName)` |
|        9 |  394 | `		 \|\| SyMemcmp((const void *)SyStringData(&pOther->pAttr->sName),` |
|       12 |  395 | `			(const void *)SyStringData(pName),SyStringLength(pName)) != 0 ){` |
|      ! 0 |  396 | `			continue;` |
|        - |  397 | `		}` |
|        8 |  398 | `		if( PH7_ClassInstanceAttrPresented(pOther)` |
|        9 |  399 | `		 && PH7_VmClassAttrAccess(&(*pVm),pThis->pClass,pOther->pAttr,FALSE) ){` |
|        5 |  400 | `			return 1;` |
|        - |  401 | `		}` |
|        3 |  402 | `	}` |
|       15 |  403 | `	return 0;` |
|      500 |  404 | `}` |
|        - |  405 | `/*` |
|        - |  406 | ` * PH7_ClassExtractAttribute, told which scope is asking: the executing class's own` |
|        - |  407 | ` * private wins over the same name declared further down the chain.` |
|        - |  408 | ` */` |
|     1240 |  409 | `PH7_PRIVATE ph7_class_attr * PH7_ClassScopedAttribute(ph7_vm *pVm,ph7_class *pClass,const char *zName,sxu32 nName)` |
|        5 |  410 | `{` |
|     2475 |  411 | `	ph7_class_attr *pOwn = OoScopePrivateAttr(&(*pVm),pClass,zName,nName,` |
|     1230 |  412 | `		nName > 0 ? SyHashKey(&pClass->hAttr,(const void *)zName,nName) : 0);` |
|     1245 |  413 | `	if( pOwn ){` |
|        3 |  414 | `		return pOwn;` |
|        - |  415 | `	}` |
|     1243 |  416 | `	return PH7_ClassExtractAttribute(pClass,zName,nName);` |
|      625 |  417 | `}` |
|        - |  418 | `/*` |
|        - |  419 | ` * PH7_ClassInstanceAttrEntry, told which scope is asking. When the executing class` |
|        - |  420 | ` * declares a private of this name, its MANGLED slot is the only one it can mean --` |
|        - |  421 | ` * so a miss there is a miss, and never falls back to the plain name (php's fetch` |
|        - |  422 | `` * stops at the property_info it resolved; an `unset()` of that slot reads as`` |
|        - |  423 | ` * undefined even when a public property of the same name sits beside it).` |
|        - |  424 | ` */` |
|   123219 |  425 | `PH7_PRIVATE SyHashEntry * PH7_ClassInstanceScopedAttrEntry(ph7_vm *pVm,ph7_class_instance *pThis,` |
|        - |  426 | `	const char *zName,sxu32 nName,sxu32 nHash)` |
|        5 |  427 | `{` |
|   123224 |  428 | `	ph7_class_attr *pOwn = OoScopePrivateAttr(&(*pVm),pThis->pClass,zName,nName,nHash);` |
|   123224 |  429 | `	if( pOwn ){` |
|        - |  430 | `		/* The MANGLED key is a different string, so the caller's hash says nothing` |
|        - |  431 | `		 * about it -- this is the 0.6% of accesses that really do mean a scope's` |
|        - |  432 | `		 * private, and they hash their own key. */` |
|       85 |  433 | `		const SyString *pKey = PH7_ClassAttrStorageName(&(*pVm),pThis->pClass,pOwn);` |
|       85 |  434 | `		return SyHashGet(&pThis->hAttr,(const void *)SyStringData(pKey),SyStringLength(pKey));` |
|        - |  435 | `	}` |
|   123140 |  436 | `	if( nName > 0 ){` |
|   123096 |  437 | `		return SyHashGetHashed(&pThis->hAttr,(const void *)zName,nName,nHash);` |
|        - |  438 | `	}` |
|       45 |  439 | `	return PH7_ClassInstanceAttrEntry(pThis,zName,nName);` |
|    61615 |  440 | `}` |
|        - |  441 | `/*` |
|        - |  442 | ` * Check if the given name is a class CONSTANT (or enum case).` |
|        - |  443 | ` * php keeps constants and properties in separate namespaces, so constants live` |
|        - |  444 | ` * in a dedicated table (hConst) and never collide with a same-named property.` |
|        - |  445 | ` * Return the desired constant [ph7_class_attr with PH7_CLASS_ATTR_CONSTANT] on` |
|        - |  446 | ` * success, NULL otherwise.` |
|        - |  447 | ` */` |
|     3552 |  448 | `PH7_PRIVATE ph7_class_attr * PH7_ClassExtractConstant(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|        5 |  449 | `{` |
|        - |  450 | `	SyHashEntry *pEntry;` |
|     3557 |  451 | `	pEntry = SyHashGet(&pClass->hConst,(const void *)zName,nByte);` |
|     3557 |  452 | `	if( pEntry == 0 ){` |
|      693 |  453 | `		return 0;` |
|        - |  454 | `	}` |
|     2869 |  455 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|     1781 |  456 | `}` |
|        - |  457 | `/*` |
|        - |  458 | ` * Install a class attribute in the corresponding container.` |
|        - |  459 | ` * A constant (or enum case) goes to hConst, a property to hAttr — php's two` |
|        - |  460 | `` * separate member namespaces, so `const C` and `public $C` coexist.`` |
|        - |  461 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|        - |  462 | ` */` |
|  6312218 |  463 | `PH7_PRIVATE sxi32 PH7_ClassInstallAttr(ph7_class *pClass,ph7_class_attr *pAttr)` |
|        5 |  464 | `{` |
|  6312223 |  465 | `	SyString *pName = &pAttr->sName;` |
|        - |  466 | `	sxi32 rc;` |
|        - |  467 | `	/* Remember where this attribute was originally declared so that later` |
|        - |  468 | `	 * inheritance/trait copies still know the declaring class (needed for` |
|        - |  469 | `	 * PHP-compatible error messages on typed properties). */` |
|  6312223 |  470 | `	if( pAttr->pDeclClass == 0 ){` |
|    43548 |  471 | `		pAttr->pDeclClass = pClass;` |
|    21744 |  472 | `	}` |
|  6312223 |  473 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|  3297617 |  474 | `		rc = SyHashInsertTail(&pClass->hConst,(const void *)pName->zString,pName->nByte,pAttr);` |
|  1646523 |  475 | `	}else{` |
|  3014611 |  476 | `		rc = SyHashInsertTail(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|  3014611 |  477 | `		PH7_ClassNotePrivateName(pClass,pAttr);` |
|        - |  478 | `	}` |
|  6312223 |  479 | `	return rc;` |
|        5 |  480 | `}` |
|        - |  481 | `/*` |
|        - |  482 | ` * Install a class method in the corresponding container.` |
|        - |  483 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|        - |  484 | ` */` |
| 11163872 |  485 | `PH7_PRIVATE sxi32 PH7_ClassInstallMethod(ph7_class *pClass,ph7_class_method *pMeth)` |
|        5 |  486 | `{` |
| 11163877 |  487 | `	SyString *pName = &pMeth->sFunc.sName;` |
|        - |  488 | `	sxi32 rc;` |
| 11163877 |  489 | `	rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 11163877 |  490 | `	return rc;` |
|        5 |  491 | `}` |
|        - |  492 | `/*` |
|        - |  493 | ` * ---------------------------------------------------------------------------` |
|        - |  494 | ` * php's rendering of a USER function's DECLARATION.` |
|        - |  495 | ` *` |
|        - |  496 | ` * The text an incompatible-override fatal prints on either side of "must be` |
|        - |  497 | `` * compatible with": `B::f(int $a, ?string $b = null): string`. It is php's`` |
|        - |  498 | ` * zend_get_function_declaration, and until this shipped both sides of that` |
|        - |  499 | `` * sentence were bare names -- `B::f() must be compatible with A::f()` -- which`` |
|        - |  500 | ` * says the declarations disagree without saying how.` |
|        - |  501 | ` *` |
|        - |  502 | `` * A parameter is `[type ][&][...]$name[ = default]`; the return type follows as`` |
|        - |  503 | `` * `: T` and is omitted entirely when the declaration has none.`` |
|        - |  504 | ` * ---------------------------------------------------------------------------` |
|        - |  505 | ` */` |
|        - |  506 | `/* A character that belongs to a type NAME, as opposed to the punctuation that` |
|        - |  507 | `` * separates the parts of a declared type (`?A`, `A\|B`, `(A&B)\|null`). */`` |
|      406 |  508 | `static int OoDeclNameChar(int c)` |
|        4 |  509 | `{` |
|      812 |  510 | `	return !(c == '\|' \|\| c == '&' \|\| c == '?' \|\| c == '(' \|\| c == ')'` |
|      402 |  511 | `		\|\| c == ' ' \|\| c == '\t');` |
|        4 |  512 | `}` |
|        - |  513 | `/*` |
|        - |  514 | ` * A declared type as php prints it in a declaration.` |
|        - |  515 | ` *` |
|        - |  516 | `` * The stored text is already php's canonical order (`string\|int` for both`` |
|        - |  517 | `` * spellings of it, `?A` for `A\|null`, an intersection as written), so only the`` |
|        - |  518 | ` * names that are relative to WHERE the declaration was written move:` |
|        - |  519 | ` *` |
|        - |  520 | ` *   self / parent  resolved against the DECLARING class -- so the same trait` |
|        - |  521 | `` *                  method reads `A $a` in one composing class and `B $a` in the`` |
|        - |  522 | ` *                  next, which is what php prints.` |
|        - |  523 | ` *   static         left as written; php has no class to resolve it to at link` |
|        - |  524 | ` *                  time either.` |
|        - |  525 | ` *   iterable       expanded to the two types it stands for. Only the standalone` |
|        - |  526 | ` *                  spellings reach here (a COMPOUND type stored it expanded` |
|        - |  527 | `` *                  already), so `?iterable` is `Traversable\|array\|null` rather`` |
|        - |  528 | `` *                  than `?Traversable\|array` -- the whole text, not a token.`` |
|        - |  529 | ` *` |
|        - |  530 | ` * VmHintTextResolvedEx answers the same question for a DIAGNOSTIC and for` |
|        - |  531 | ` * Reflection, and is not reused here for one reason: it writes into a fixed` |
|        - |  532 | ` * caller buffer and truncates. A diagnostic naming a type can afford that; a` |
|        - |  533 | ` * declaration this sentence is asking the reader to COMPARE with another cannot,` |
|        - |  534 | ` * so this one appends to the blob and has no length to run out of.` |
|        - |  535 | ` */` |
|       78 |  536 | `static void OoDeclType(ph7_class *pScope,const SyString *pDeclared,SyBlob *pOut)` |
|        4 |  537 | `{` |
|       82 |  538 | `	const char *z = pDeclared ? SyStringData(pDeclared) : 0;` |
|       82 |  539 | `	sxu32 n = z ? SyStringLength(pDeclared) : 0;` |
|       82 |  540 | `	sxu32 i = 0;` |
|       82 |  541 | `	if( n < 1 ){` |
|      ! 0 |  542 | `		return;` |
|        - |  543 | `	}` |
|       82 |  544 | `	if( n == 8 && SyStrnicmp(z,"iterable",8) == 0 ){` |
|        5 |  545 | `		SyBlobAppend(pOut,"Traversable\|array",sizeof("Traversable\|array")-1);` |
|        5 |  546 | `		return;` |
|        - |  547 | `	}` |
|       78 |  548 | `	if( n == 9 && z[0] == '?' && SyStrnicmp(&z[1],"iterable",8) == 0 ){` |
|        5 |  549 | `		SyBlobAppend(pOut,"Traversable\|array\|null",sizeof("Traversable\|array\|null")-1);` |
|        5 |  550 | `		return;` |
|        - |  551 | `	}` |
|      148 |  552 | `	while( i < n ){` |
|        - |  553 | `		sxu32 nStart;` |
|        - |  554 | `		const SyString *pWrite;` |
|        - |  555 | `		SyString sTok;` |
|       78 |  556 | `		if( !OoDeclNameChar(z[i]) ){` |
|        5 |  557 | `			SyBlobAppend(pOut,&z[i],sizeof(char));` |
|        5 |  558 | `			i++;` |
|        5 |  559 | `			continue;` |
|        - |  560 | `		}` |
|       74 |  561 | `		nStart = i;` |
|      406 |  562 | `		while( i < n && OoDeclNameChar(z[i]) ){` |
|      336 |  563 | `			i++;` |
|        4 |  564 | `		}` |
|       74 |  565 | `		SyStringInitFromBuf(&sTok,&z[nStart],i - nStart);` |
|       74 |  566 | `		pWrite = &sTok;` |
|       74 |  567 | `		if( pScope ){` |
|       70 |  568 | `			if( sTok.nByte == sizeof("self")-1` |
|       50 |  569 | `			 && SyStrnicmp(sTok.zString,"self",sizeof("self")-1) == 0 ){` |
|       10 |  570 | `				pWrite = &pScope->sName;` |
|       68 |  571 | `			}else if( sTok.nByte == sizeof("parent")-1` |
|       45 |  572 | `			 && SyStrnicmp(sTok.zString,"parent",sizeof("parent")-1) == 0` |
|       20 |  573 | `			 && pScope->pBase ){` |
|        5 |  574 | `				pWrite = &pScope->pBase->sName;` |
|        2 |  575 | `			}` |
|       35 |  576 | `		}` |
|       74 |  577 | `		SyBlobAppend(pOut,SyStringData(pWrite),SyStringLength(pWrite));` |
|        4 |  578 | `	}` |
|       43 |  579 | `}` |
|        - |  580 | `/* The instructions of a compiled default, without the OP_DONE the compiler` |
|        - |  581 | ` * terminates every one of them with. */` |
|       22 |  582 | `static sxu32 OoDeclDefLength(SySet *pByteCode)` |
|        2 |  583 | `{` |
|       24 |  584 | `	sxu32 n = SySetUsed(pByteCode);` |
|       46 |  585 | `	while( n > 0 ){` |
|       46 |  586 | `		VmInstr *pIn = (VmInstr *)SySetAt(pByteCode,n - 1);` |
|       46 |  587 | `		if( pIn == 0 \|\| (pIn->iOp != PH7_OP_DONE && pIn->iOp != PH7_OP_NOOP) ){` |
|       13 |  588 | `			break;` |
|        - |  589 | `		}` |
|       24 |  590 | `		n--;` |
|        2 |  591 | `	}` |
|       24 |  592 | `	return n;` |
|        2 |  593 | `}` |
|        - |  594 | `/*` |
|        - |  595 | ` * TRUE when every instruction of a compiled default is a LITERAL load or a pure` |
|        - |  596 | ` * value operator -- the run php's constant folder would try to reduce. A LOADC` |
|        - |  597 | ` * still carrying PH7_LOADC_EXPAND is a constant NAME, which php deliberately` |
|        - |  598 | ` * does NOT fold (it prints the name); anything that reads a variable, calls` |
|        - |  599 | ` * something or builds an object is not a constant expression at all.` |
|        - |  600 | ` *` |
|        - |  601 | ` * This is the screen PH7_VmEvalConstExpr's block comment requires. It is the` |
|        - |  602 | ` * SySet twin of the instanceof folder's GenStateInstanceofFoldsLhs, which asks` |
|        - |  603 | ` * the same question of the generator's live stream.` |
|        - |  604 | ` */` |
|       18 |  605 | `static int OoDeclDefFoldable(SySet *pByteCode,sxu32 nLen)` |
|        2 |  606 | `{` |
|        - |  607 | `	sxu32 n;` |
|       52 |  608 | `	for( n = 0 ; n < nLen ; ++n ){` |
|       36 |  609 | `		VmInstr *pIn = (VmInstr *)SySetAt(pByteCode,n);` |
|       36 |  610 | `		if( pIn == 0 ){` |
|      ! 0 |  611 | `			return 0;` |
|        - |  612 | `		}` |
|       36 |  613 | `		switch( pIn->iOp ){` |
|       13 |  614 | `		case PH7_OP_LOADC:` |
|       28 |  615 | `			if( pIn->iP1 & PH7_LOADC_EXPAND ){` |
|        3 |  616 | `				return 0; /* a constant NAME -- php keeps it unfolded */` |
|        - |  617 | `			}` |
|       26 |  618 | `			break;` |
|        4 |  619 | `		case PH7_OP_LOAD_MAP: case PH7_OP_LOAD_IDX:` |
|        - |  620 | `		case PH7_OP_CAT:` |
|        - |  621 | `		case PH7_OP_CVT_INT: case PH7_OP_CVT_STR: case PH7_OP_CVT_REAL:` |
|        - |  622 | `		case PH7_OP_CVT_BOOL: case PH7_OP_CVT_NUMC: case PH7_OP_CVT_NULL:` |
|        - |  623 | `		case PH7_OP_UMINUS: case PH7_OP_UPLUS: case PH7_OP_BITNOT: case PH7_OP_LNOT:` |
|        - |  624 | `		case PH7_OP_MUL: case PH7_OP_DIV: case PH7_OP_MOD: case PH7_OP_POW:` |
|        - |  625 | `		case PH7_OP_ADD: case PH7_OP_SUB: case PH7_OP_SHL: case PH7_OP_SHR:` |
|        - |  626 | `		case PH7_OP_LT: case PH7_OP_LE: case PH7_OP_GT: case PH7_OP_GE:` |
|        - |  627 | `		case PH7_OP_SPACESHIP: case PH7_OP_EQ: case PH7_OP_NEQ:` |
|        - |  628 | `		case PH7_OP_TEQ: case PH7_OP_TNE:` |
|        - |  629 | `		case PH7_OP_BAND: case PH7_OP_BXOR: case PH7_OP_BOR:` |
|        - |  630 | `		case PH7_OP_LAND: case PH7_OP_LOR: case PH7_OP_LXOR:` |
|        - |  631 | `		case PH7_OP_JMP: case PH7_OP_JZ: case PH7_OP_JNZ:` |
|        - |  632 | `		case PH7_OP_POP: case PH7_OP_DUP: case PH7_OP_NOOP:` |
|        9 |  633 | `			break;` |
|      ! 0 |  634 | `		default:` |
|      ! 0 |  635 | `			return 0;` |
|        - |  636 | `		}` |
|       18 |  637 | `	}` |
|       18 |  638 | `	return nLen > 0;` |
|       11 |  639 | `}` |
|        - |  640 | `/* The literal a LOADC pushes, or 0 when the operand is not a string one. */` |
|        6 |  641 | `static const SyString * OoDeclLiteral(ph7_vm *pVm,VmInstr *pIn,SyString *pOut)` |
|        1 |  642 | `{` |
|        - |  643 | `	ph7_value *pLit;` |
|        7 |  644 | `	if( pIn == 0 \|\| pIn->iOp != PH7_OP_LOADC ){` |
|      ! 0 |  645 | `		return 0;` |
|        - |  646 | `	}` |
|        7 |  647 | `	pLit = (ph7_value *)SySetAt(&pVm->aLitObj,(sxu32)pIn->iP2);` |
|        7 |  648 | `	if( pLit == 0 \|\| (pLit->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  649 | `		return 0;` |
|        - |  650 | `	}` |
|        7 |  651 | `	SyStringInitFromBuf(pOut,SyBlobData(&pLit->sBlob),SyBlobLength(&pLit->sBlob));` |
|        7 |  652 | `	return pOut;` |
|        4 |  653 | `}` |
|        - |  654 | `/*` |
|        - |  655 | ` * A FOLDED default value, spelled php's way.` |
|        - |  656 | ` *` |
|        - |  657 | ` * php's own spellings, and they are not the export's: a string is SINGLE-quoted,` |
|        - |  658 | `` * printed RAW (no escaping at all) and TRUNCATED to ten bytes with `...` inside`` |
|        - |  659 | `` * the quotes; `null` is lower-case; an array shows only whether it is empty.`` |
|        - |  660 | ` */` |
|       16 |  661 | `static void OoDeclValue(SyBlob *pOut,ph7_value *pVal)` |
|        2 |  662 | `{` |
|       18 |  663 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|        3 |  664 | `		SyBlobAppend(pOut,"null",sizeof("null")-1);` |
|        3 |  665 | `		return;` |
|        - |  666 | `	}` |
|       15 |  667 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  668 | `		SyBlobAppend(pOut,pVal->x.iVal ? "true" : "false",pVal->x.iVal ? 4 : 5);` |
|      ! 0 |  669 | `		return;` |
|        - |  670 | `	}` |
|       15 |  671 | `	if( pVal->iFlags & MEMOBJ_STRING ){` |
|        7 |  672 | `		sxu32 nStr = SyBlobLength(&pVal->sBlob);` |
|        7 |  673 | `		SyBlobAppend(pOut,"'",sizeof(char));` |
|        7 |  674 | `		if( nStr > 0 ){` |
|        7 |  675 | `			SyBlobAppend(pOut,SyBlobData(&pVal->sBlob),(nStr > 10 ? (sxu32)10 : nStr));` |
|        3 |  676 | `		}` |
|        7 |  677 | `		if( nStr > 10 ){` |
|        3 |  678 | `			SyBlobAppend(pOut,"...",sizeof("...")-1);` |
|        1 |  679 | `		}` |
|        7 |  680 | `		SyBlobAppend(pOut,"'",sizeof(char));` |
|        7 |  681 | `		return;` |
|        - |  682 | `	}` |
|        9 |  683 | `	if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|        5 |  684 | `		ph7_hashmap *pMap = (ph7_hashmap *)pVal->x.pOther;` |
|       11 |  685 | `		SyBlobAppend(pOut,` |
|        4 |  686 | `			(pMap && pMap->nEntry > 0) ? "[...]" : "[]",` |
|        4 |  687 | `			(pMap && pMap->nEntry > 0) ? sizeof("[...]")-1 : sizeof("[]")-1);` |
|        5 |  688 | `		return;` |
|        - |  689 | `	}` |
|        5 |  690 | `	if( pVal->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|        - |  691 | ``		/* php prints the value's own string cast, which is where `1.0` reads `1`,`` |
|        - |  692 | ``		 * `1e100` reads `1.0E+100` and INF reads `INF`. */`` |
|        5 |  693 | `		PH7_MemObjToString(pVal);` |
|        5 |  694 | `		SyBlobAppend(pOut,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));` |
|        5 |  695 | `		return;` |
|        - |  696 | `	}` |
|      ! 0 |  697 | `	SyBlobAppend(pOut,"<expression>",sizeof("<expression>")-1);` |
|       10 |  698 | `}` |
|        - |  699 | `/*` |
|        - |  700 | `` * The text after `= ` in a parameter default.`` |
|        - |  701 | ` *` |
|        - |  702 | ` * php prints what its compiler FOLDED the expression to, with two deliberate` |
|        - |  703 | ` * exceptions it leaves unfolded and prints as source: a lone constant reference` |
|        - |  704 | `` * keeps its NAME (`= M_PI`, `= PHP_INT_MAX`) and a class constant keeps`` |
|        - |  705 | `` * `Class::NAME` as written (`= self::K`, `= MyEnum::Foo`). `X::class` is not`` |
|        - |  706 | `` * one of those -- it folds to the class-name STRING, so it prints `'X'`.`` |
|        - |  707 | `` * Everything it could not reduce is php's `<expression>`.`` |
|        - |  708 | ` */` |
|       22 |  709 | `static void OoDeclDefault(ph7_vm *pVm,ph7_class *pScope,SySet *pByteCode,SyBlob *pOut)` |
|        2 |  710 | `{` |
|       24 |  711 | `	sxu32 nLen = OoDeclDefLength(pByteCode);` |
|        - |  712 | `	SyString sOne, sTwo;` |
|       24 |  713 | `	if( nLen == 1 ){` |
|       14 |  714 | `		VmInstr *pIn = (VmInstr *)SySetAt(pByteCode,0);` |
|       12 |  715 | `		if( pIn && pIn->iOp == PH7_OP_LOADC && (pIn->iP1 & PH7_LOADC_EXPAND)` |
|        8 |  716 | `		 && OoDeclLiteral(pVm,pIn,&sOne) ){` |
|        - |  717 | `			/* A constant NAME, exactly as the source wrote it. */` |
|        3 |  718 | `			SyBlobAppend(pOut,SyStringData(&sOne),SyStringLength(&sOne));` |
|       12 |  719 | `			return;` |
|        - |  720 | `		}` |
|        5 |  721 | `	}` |
|       22 |  722 | `	if( nLen == 3 ){` |
|        9 |  723 | `		VmInstr *pCls = (VmInstr *)SySetAt(pByteCode,0);` |
|        9 |  724 | `		VmInstr *pMem = (VmInstr *)SySetAt(pByteCode,1);` |
|        9 |  725 | `		VmInstr *pOp  = (VmInstr *)SySetAt(pByteCode,2);` |
|        8 |  726 | `		if( pOp && pOp->iOp == PH7_OP_MEMBER && pOp->iP1 == 1` |
|        2 |  727 | `		 && pOp->iP2 == PH7_MEMBER_READ` |
|        3 |  728 | `		 && OoDeclLiteral(pVm,pCls,&sOne) && OoDeclLiteral(pVm,pMem,&sTwo) ){` |
|        2 |  729 | `			if( sTwo.nByte == sizeof("class")-1` |
|        2 |  730 | `			 && SyStrnicmp(sTwo.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  731 | ``				/* `self::class` -- the only ::class spelling the compiler leaves for`` |
|        - |  732 | `				 * the runtime (a named class folds to its own literal, and lands on` |
|        - |  733 | `				 * the value path below). php folded it too, to the STRING. */` |
|        - |  734 | `				SyBlob sName;` |
|      ! 0 |  735 | `				ph7_class *pCurr = 0;` |
|      ! 0 |  736 | `				if( pScope ){` |
|      ! 0 |  737 | `					pCurr = (sOne.nByte == sizeof("parent")-1` |
|      ! 0 |  738 | `						&& SyStrnicmp(sOne.zString,"parent",sizeof("parent")-1) == 0)` |
|      ! 0 |  739 | `						? pScope->pBase : pScope;` |
|      ! 0 |  740 | `				}` |
|      ! 0 |  741 | `				if( pCurr ){` |
|      ! 0 |  742 | `					SyBlobInit(&sName,&pVm->sAllocator);` |
|      ! 0 |  743 | `					SyBlobAppend(&sName,"'",sizeof(char));` |
|      ! 0 |  744 | `					SyBlobAppend(&sName,SyStringData(&pCurr->sName),SyStringLength(&pCurr->sName));` |
|      ! 0 |  745 | `					SyBlobAppend(&sName,"'",sizeof(char));` |
|      ! 0 |  746 | `					SyBlobAppend(pOut,SyBlobData(&sName),SyBlobLength(&sName));` |
|      ! 0 |  747 | `					SyBlobRelease(&sName);` |
|      ! 0 |  748 | `					return;` |
|        - |  749 | `				}` |
|      ! 0 |  750 | `			}else{` |
|        3 |  751 | `				SyBlobAppend(pOut,SyStringData(&sOne),SyStringLength(&sOne));` |
|        3 |  752 | `				SyBlobAppend(pOut,"::",sizeof("::")-1);` |
|        3 |  753 | `				SyBlobAppend(pOut,SyStringData(&sTwo),SyStringLength(&sTwo));` |
|        3 |  754 | `				return;` |
|        - |  755 | `			}` |
|      ! 0 |  756 | `		}` |
|        3 |  757 | `	}` |
|       20 |  758 | `	if( OoDeclDefFoldable(pByteCode,nLen) ){` |
|        - |  759 | `		ph7_value sVal;` |
|        - |  760 | `		int bFolded;` |
|       18 |  761 | `		PH7_MemObjInit(pVm,&sVal);` |
|       18 |  762 | `		bFolded = PH7_VmEvalConstExpr(pVm,pByteCode,&sVal);` |
|       18 |  763 | `		if( bFolded ){` |
|       18 |  764 | `			OoDeclValue(pOut,&sVal);` |
|        8 |  765 | `		}` |
|       18 |  766 | `		PH7_MemObjRelease(&sVal);` |
|       18 |  767 | `		if( bFolded ){` |
|       18 |  768 | `			return;` |
|        - |  769 | `		}` |
|      ! 0 |  770 | `	}` |
|        3 |  771 | `	SyBlobAppend(pOut,"<expression>",sizeof("<expression>")-1);` |
|       13 |  772 | `}` |
|        - |  773 | `/*` |
|        - |  774 | ` * php hands each rendered declaration to its error formatter as a C STRING, so a` |
|        - |  775 | `` * declaration carrying a NUL byte -- `function f($a = "\0")` -- is cut there and`` |
|        - |  776 | `` * the sentence carries on with what follows it (`A::f($a = '` and then ` in ... on`` |
|        - |  777 | `` * line N`). Reproduced rather than left as a whole-blob write, which is the one`` |
|        - |  778 | ` * shape where the two engines would disagree byte for byte.` |
|        - |  779 | ` */` |
|       60 |  780 | `static int OoDeclCLen(SyBlob *pDecl)` |
|        4 |  781 | `{` |
|       64 |  782 | `	const char *z = (const char *)SyBlobData(pDecl);` |
|       64 |  783 | `	sxu32 n = SyBlobLength(pDecl), i;` |
|     1358 |  784 | `	for( i = 0 ; i < n ; ++i ){` |
|     1298 |  785 | `		if( z[i] == 0 ){` |
|      ! 0 |  786 | `			return (int)i;` |
|        - |  787 | `		}` |
|      651 |  788 | `	}` |
|       64 |  789 | `	return (int)n;` |
|       34 |  790 | `}` |
|        - |  791 | `/*` |
|        - |  792 | `` * `(int $a, ?string $b = null): string` -- everything php prints after the`` |
|        - |  793 | ` * method's name. pScope is the class the declaration was written FOR (a trait` |
|        - |  794 | ``  * method's composing class, not the trait), which is what `self` and `parent` `` |
|        - |  795 | ` * resolve against.` |
|        - |  796 | ` */` |
|       60 |  797 | `PH7_PRIVATE void PH7_ClassRenderDecl(ph7_vm *pVm,ph7_class *pScope,ph7_vm_func *pFunc,SyBlob *pOut)` |
|        4 |  798 | `{` |
|       64 |  799 | `	ph7_vm_func_arg *aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       64 |  800 | `	sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|        - |  801 | `	sxu32 i;` |
|       64 |  802 | `	SyBlobAppend(pOut,"(",sizeof(char));` |
|      130 |  803 | `	for( i = 0 ; i < nArg ; ++i ){` |
|       70 |  804 | `		if( i > 0 ){` |
|       43 |  805 | `			SyBlobAppend(pOut,", ",sizeof(", ")-1);` |
|       20 |  806 | `		}` |
|       70 |  807 | `		if( SyStringLength(&aArgs[i].sTypeName) > 0 ){` |
|       37 |  808 | `			OoDeclType(pScope,&aArgs[i].sTypeName,pOut);` |
|       37 |  809 | `			SyBlobAppend(pOut," ",sizeof(char));` |
|       17 |  810 | `		}` |
|       70 |  811 | `		if( aArgs[i].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        3 |  812 | `			SyBlobAppend(pOut,"&",sizeof(char));` |
|        1 |  813 | `		}` |
|       70 |  814 | `		if( aArgs[i].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        3 |  815 | `			SyBlobAppend(pOut,"...",sizeof("...")-1);` |
|        1 |  816 | `		}` |
|       70 |  817 | `		SyBlobAppend(pOut,"$",sizeof(char));` |
|       70 |  818 | `		SyBlobAppend(pOut,SyStringData(&aArgs[i].sName),SyStringLength(&aArgs[i].sName));` |
|       70 |  819 | `		if( SySetUsed(&aArgs[i].aByteCode) > 0 ){` |
|       24 |  820 | `			SyBlobAppend(pOut," = ",sizeof(" = ")-1);` |
|       24 |  821 | `			OoDeclDefault(pVm,pScope,&aArgs[i].aByteCode,pOut);` |
|       11 |  822 | `		}` |
|       37 |  823 | `	}` |
|       64 |  824 | `	SyBlobAppend(pOut,")",sizeof(char));` |
|       64 |  825 | `	if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|       48 |  826 | `		SyBlobAppend(pOut,": ",sizeof(": ")-1);` |
|       48 |  827 | `		OoDeclType(pScope,&pFunc->sReturnTypeName,pOut);` |
|       22 |  828 | `	}` |
|       64 |  829 | `}` |
|        - |  830 | `/*` |
|        - |  831 | ` * ---------------------------------------------------------------------------` |
|        - |  832 | ` * Method-override compatibility: php's declared-type LATTICE.` |
|        - |  833 | ` *` |
|        - |  834 | ` * php rejects an override whose signature is incompatible with the parent's --` |
|        - |  835 | ` * a return type is COVARIANT (the child may only narrow), a parameter type is` |
|        - |  836 | ` * CONTRAVARIANT (the child may only widen), and the arity/by-reference shape` |
|        - |  837 | ` * must let every call the parent accepts reach the child. This used to be a` |
|        - |  838 | ` * deliberately SKIP-BY-DEFAULT approximation: it decided a bare scalar against a` |
|        - |  839 | ` * bare scalar and a loaded class against a loaded class, and accepted everything` |
|        - |  840 | `` * subtle -- a union, an intersection, `mixed`, `object`, `iterable`, `void`,`` |
|        - |  841 | `` * `never`, `self`/`static`, or a variadic signature. Fourteen shapes php refuses`` |
|        - |  842 | ` * compiled here in silence.` |
|        - |  843 | ` *` |
|        - |  844 | ` * The lattice below is php's, derived from the oracle: a declared type is a` |
|        - |  845 | ` * DISJUNCTION of intersection GROUPS, each group a conjunction of ATOMS, and` |
|        - |  846 | ` *` |
|        - |  847 | ` *     child ⊆ parent   iff   every child group is a subtype of SOME parent group` |
|        - |  848 | ` *     Gc ⊆ Gp          iff   every atom of Gp has SOME atom of Gc under it` |
|        - |  849 | ` *` |
|        - |  850 | ` * which is all a plain union, an intersection and a DNF type need between them.` |
|        - |  851 | `` * `bUnknown` is what is left of the old skip: a shape this cannot model (a type`` |
|        - |  852 | `` * naming a class no autoload-free lookup finds, `parent` with no base, more`` |
|        - |  853 | ` * atoms than the bound) is still ACCEPTED, because refusing valid php is the` |
|        - |  854 | ` * one failure mode that matters here.` |
|        - |  855 | ` * ---------------------------------------------------------------------------` |
|        - |  856 | ` */` |
|        - |  857 | `#define OVB_INT      0x0001` |
|        - |  858 | `#define OVB_FLOAT    0x0002` |
|        - |  859 | `#define OVB_STRING   0x0004` |
|        - |  860 | `#define OVB_BOOL     0x0008` |
|        - |  861 | `#define OVB_FALSE    0x0010` |
|        - |  862 | `#define OVB_TRUE     0x0020` |
|        - |  863 | `#define OVB_ARRAY    0x0040` |
|        - |  864 | ``#define OVB_OBJECT   0x0080  /* the `object` pseudo-type: every class at once */`` |
|        - |  865 | `#define OVB_CALLABLE 0x0100` |
|        - |  866 | `#define OVB_NULL     0x0200` |
|        - |  867 | `#define OVB_VOID     0x0400` |
|        - |  868 | ``#define OVB_STATIC   0x0800  /* `static`: the CALLED class of the declaring one */`` |
|        - |  869 | `#define OVB_CLS      0x1000  /* a named class/interface, resolved into pCls */` |
|        - |  870 | `#define OV_MAX_ATOM  16      /* bounds the on-stack atom array; over it, bUnknown */` |
|        - |  871 |  |
|        - |  872 | `typedef struct OvAtom OvAtom;` |
|        - |  873 | `struct OvAtom {` |
|        - |  874 | `	sxu32 nBit;      /* OVB_* */` |
|        - |  875 | `	ph7_class *pCls; /* the class, when nBit == OVB_CLS */` |
|        - |  876 | `	sxu32 nGroup;    /* intersection group: atoms sharing one are ANDed */` |
|        - |  877 | `};` |
|        - |  878 | `typedef struct OvType OvType;` |
|        - |  879 | `struct OvType {` |
|        - |  880 | `	int bAbsent;  /* no declared type at all -- not a type, an ABSENCE (see below) */` |
|        - |  881 | ``	int bMixed;   /* `mixed`: the top type */`` |
|        - |  882 | ``	int bNever;   /* `never`: the bottom type, a subtype of everything */`` |
|        - |  883 | `	int bUnknown; /* a shape this lattice does not model -- accept whatever it meets */` |
|        - |  884 | `	int nAtom;` |
|        - |  885 | `	sxu32 nNextGroup;` |
|        - |  886 | `	OvAtom a[OV_MAX_ATOM];` |
|        - |  887 | `};` |
|      776 |  888 | `static void OvInit(OvType *pT)` |
|        5 |  889 | `{` |
|      781 |  890 | `	SyZero(pT,sizeof(*pT));` |
|      781 |  891 | `}` |
|      380 |  892 | `static void OvAddAtom(OvType *pT,sxu32 nBit,ph7_class *pCls,sxu32 nGroup)` |
|        5 |  893 | `{` |
|      385 |  894 | `	if( pT->nAtom >= OV_MAX_ATOM ){` |
|      ! 0 |  895 | `		pT->bUnknown = 1;` |
|      ! 0 |  896 | `		return;` |
|        - |  897 | `	}` |
|      385 |  898 | `	pT->a[pT->nAtom].nBit = nBit;` |
|      385 |  899 | `	pT->a[pT->nAtom].pCls = pCls;` |
|      385 |  900 | `	pT->a[pT->nAtom].nGroup = nGroup;` |
|      385 |  901 | `	pT->nAtom++;` |
|      385 |  902 | `	if( nGroup >= pT->nNextGroup ){` |
|      365 |  903 | `		pT->nNextGroup = nGroup + 1;` |
|      180 |  904 | `	}` |
|      195 |  905 | `}` |
|        - |  906 | `/* One atom written as a NAME: a class, or one of the pseudo-types php parses as` |
|        - |  907 | `` * a class-name atom. `iterable` is TWO types, so it contributes two atoms in two`` |
|        - |  908 | ` * groups -- it is a union, never an intersection member (php forbids the latter). */` |
|       90 |  909 | `static void OvAddName(ph7_vm *pVm,ph7_class *pScope,OvType *pT,const SyString *pName,sxu32 nGroup)` |
|        5 |  910 | `{` |
|        - |  911 | `	static const struct { const char *z; sxu32 n; sxu32 nBit; } aWord[] = {` |
|        - |  912 | `		{ "callable",8, OVB_CALLABLE }, { "false",5, OVB_FALSE }, { "true",4, OVB_TRUE },` |
|        - |  913 | `		{ "object",6, OVB_OBJECT },     { "null",4,  OVB_NULL },  { "void",4,  OVB_VOID },` |
|        - |  914 | `		{ "static",6, OVB_STATIC },     { "int",3,   OVB_INT },   { "float",5, OVB_FLOAT },` |
|        - |  915 | `		{ "string",6, OVB_STRING },     { "bool",4,  OVB_BOOL },  { "array",5, OVB_ARRAY }` |
|        - |  916 | `	};` |
|       95 |  917 | `	const char *z = SyStringData(pName);` |
|       95 |  918 | `	sxu32 n = SyStringLength(pName), i;` |
|        - |  919 | `	SyHashEntry *pE;` |
|       95 |  920 | `	if( n < 1 ){` |
|      ! 0 |  921 | `		pT->bUnknown = 1;` |
|      ! 0 |  922 | `		return;` |
|        - |  923 | `	}` |
|       95 |  924 | `	if( n == sizeof("mixed")-1 && SyStrnicmp(z,"mixed",n) == 0 ){` |
|        5 |  925 | `		pT->bMixed = 1;` |
|        5 |  926 | `		return;` |
|        - |  927 | `	}` |
|       91 |  928 | `	if( n == sizeof("never")-1 && SyStrnicmp(z,"never",n) == 0 ){` |
|      ! 0 |  929 | `		pT->bNever = 1;` |
|      ! 0 |  930 | `		return;` |
|        - |  931 | `	}` |
|       91 |  932 | `	if( n == sizeof("iterable")-1 && SyStrnicmp(z,"iterable",n) == 0 ){` |
|        3 |  933 | `		ph7_class *pTrav = PH7_VmExtractClass(pVm,"Traversable",sizeof("Traversable")-1,FALSE,0);` |
|        3 |  934 | `		if( pTrav == 0 ){` |
|      ! 0 |  935 | `			pT->bUnknown = 1;` |
|      ! 0 |  936 | `			return;` |
|        - |  937 | `		}` |
|        3 |  938 | `		OvAddAtom(pT,OVB_ARRAY,0,pT->nNextGroup);` |
|        3 |  939 | `		OvAddAtom(pT,OVB_CLS,pTrav,pT->nNextGroup);` |
|        3 |  940 | `		return;` |
|        - |  941 | `	}` |
|      895 |  942 | `	for( i = 0 ; i < SX_ARRAYSIZE(aWord) ; ++i ){` |
|      843 |  943 | `		if( n == aWord[i].n && SyStrnicmp(z,aWord[i].z,n) == 0 ){` |
|       35 |  944 | `			OvAddAtom(pT,aWord[i].nBit,0,nGroup);` |
|       35 |  945 | `			return;` |
|        - |  946 | `		}` |
|      408 |  947 | `	}` |
|       57 |  948 | `	if( n == sizeof("self")-1 && SyStrnicmp(z,"self",n) == 0 ){` |
|       24 |  949 | `		if( pScope == 0 ){` |
|      ! 0 |  950 | `			pT->bUnknown = 1;` |
|      ! 0 |  951 | `			return;` |
|        - |  952 | `		}` |
|       24 |  953 | `		OvAddAtom(pT,OVB_CLS,pScope,nGroup);` |
|       24 |  954 | `		return;` |
|        - |  955 | `	}` |
|       35 |  956 | `	if( n == sizeof("parent")-1 && SyStrnicmp(z,"parent",n) == 0 ){` |
|      ! 0 |  957 | `		if( pScope == 0 \|\| pScope->pBase == 0 ){` |
|      ! 0 |  958 | `			pT->bUnknown = 1;` |
|      ! 0 |  959 | `			return;` |
|        - |  960 | `		}` |
|      ! 0 |  961 | `		OvAddAtom(pT,OVB_CLS,pScope->pBase,nGroup);` |
|      ! 0 |  962 | `		return;` |
|        - |  963 | `	}` |
|        - |  964 | `	/* A real class name, resolved WITHOUT autoloading: a miss is a forward` |
|        - |  965 | `	 * reference or a class no lookup can produce, and the whole type becomes` |
|        - |  966 | `	 * undecidable rather than wrong. */` |
|       35 |  967 | `	pE = SyHashGet(&pVm->hClass,(const void *)z,n);` |
|       35 |  968 | `	if( pE == 0 ){` |
|      ! 0 |  969 | `		pT->bUnknown = 1;` |
|      ! 0 |  970 | `		return;` |
|        - |  971 | `	}` |
|       35 |  972 | `	OvAddAtom(pT,OVB_CLS,(ph7_class *)pE->pUserData,nGroup);` |
|       50 |  973 | `}` |
|        - |  974 | `/* One atom given as a MEMOBJ_* code plus, for SXU32_HIGH, its name. */` |
|      370 |  975 | `static void OvAddCode(ph7_vm *pVm,ph7_class *pScope,OvType *pT,sxu32 nType,` |
|        - |  976 | `	const SyString *pName,sxu32 nGroup)` |
|        5 |  977 | `{` |
|      375 |  978 | `	switch( nType ){` |
|      119 |  979 | `	case MEMOBJ_INT:     OvAddAtom(pT,OVB_INT,0,nGroup);    return;` |
|      ! 0 |  980 | `	case MEMOBJ_REAL:    OvAddAtom(pT,OVB_FLOAT,0,nGroup);  return;` |
|       98 |  981 | `	case MEMOBJ_STRING:  OvAddAtom(pT,OVB_STRING,0,nGroup); return;` |
|        8 |  982 | `	case MEMOBJ_BOOL:    OvAddAtom(pT,OVB_BOOL,0,nGroup);   return;` |
|        3 |  983 | `	case MEMOBJ_HASHMAP: OvAddAtom(pT,OVB_ARRAY,0,nGroup);  return;` |
|        5 |  984 | `	case MEMOBJ_OBJ:     OvAddAtom(pT,OVB_OBJECT,0,nGroup); return;` |
|       63 |  985 | `	case MEMOBJ_VOID:    OvAddAtom(pT,OVB_VOID,0,nGroup);   return;` |
|        3 |  986 | `	case MEMOBJ_NEVER:   pT->bNever = 1;                    return;` |
|        - |  987 | ``	/* php 8.2's standalone `null`, which the parser records BOTH as this code and`` |
|        - |  988 | `	 * as the nullable flag; the second atom the flag adds is the same type. */` |
|      ! 0 |  989 | `	case MEMOBJ_NULL:    OvAddAtom(pT,OVB_NULL,0,nGroup);   return;` |
|       90 |  990 | `	default: break;` |
|        - |  991 | `	}` |
|       95 |  992 | `	if( nType == SXU32_HIGH ){` |
|       95 |  993 | `		OvAddName(pVm,pScope,pT,pName,nGroup);` |
|       95 |  994 | `		return;` |
|        - |  995 | `	}` |
|      ! 0 |  996 | `	pT->bUnknown = 1;` |
|      190 |  997 | `}` |
|        - |  998 | `/*` |
|        - |  999 | ` * The union alternatives, or the single type, of one declaration.` |
|        - | 1000 | ` *` |
|        - | 1001 | ` * The stored intersection-group ids are RE-MAPPED rather than used as they come:` |
|        - | 1002 | `` * `iterable` is an alternative that expands into TWO groups of its own, so a`` |
|        - | 1003 | ` * later alternative's stored id would otherwise land in the group Traversable` |
|        - | 1004 | `` * had just been given and read as `Traversable&int`. A group with more than one`` |
|        - | 1005 | `` * member is an intersection, which `iterable` may not appear in at all (php`` |
|        - | 1006 | `` * refuses `iterable&X`) -- if one ever did, the whole type is undecidable.`` |
|        - | 1007 | ` */` |
|      776 | 1008 | `static void OvFromDecl(ph7_vm *pVm,ph7_class *pScope,OvType *pT,sxu32 nType,` |
|        - | 1009 | `	const SyString *pClass,SySet *pAlts,int bNullable)` |
|        5 | 1010 | `{` |
|      781 | 1011 | `	OvInit(pT);` |
|      781 | 1012 | `	if( SySetUsed(pAlts) > 0 ){` |
|       13 | 1013 | `		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(pAlts);` |
|        - | 1014 | `		sxu32 aMap[PHL_UNION_MAX_ALTS];` |
|        - | 1015 | `		sxu32 aCount[PHL_UNION_MAX_ALTS];` |
|       13 | 1016 | `		sxu32 i, n = SySetUsed(pAlts);` |
|      333 | 1017 | `		for( i = 0 ; i < PHL_UNION_MAX_ALTS ; ++i ){` |
|      323 | 1018 | `			aMap[i] = SXU32_HIGH;` |
|      323 | 1019 | `			aCount[i] = 0;` |
|      163 | 1020 | `		}` |
|       33 | 1021 | `		for( i = 0 ; i < n ; ++i ){` |
|       23 | 1022 | `			if( aAlt[i].nGroup >= PHL_UNION_MAX_ALTS ){` |
|      ! 0 | 1023 | `				pT->bUnknown = 1;` |
|      ! 0 | 1024 | `				return;` |
|        - | 1025 | `			}` |
|       23 | 1026 | `			aCount[aAlt[i].nGroup]++;` |
|       13 | 1027 | `		}` |
|       33 | 1028 | `		for( i = 0 ; i < n ; ++i ){` |
|       23 | 1029 | `			sxu32 g = aAlt[i].nGroup;` |
|       37 | 1030 | `			int bIter = ( aAlt[i].nType == SXU32_HIGH` |
|       14 | 1031 | `				&& SyStringLength(&aAlt[i].sClass) == sizeof("iterable")-1` |
|       24 | 1032 | `				&& SyStrnicmp(SyStringData(&aAlt[i].sClass),"iterable",sizeof("iterable")-1) == 0 );` |
|       23 | 1033 | `			if( bIter && aCount[g] > 1 ){` |
|      ! 0 | 1034 | `				pT->bUnknown = 1;` |
|      ! 0 | 1035 | `				return;` |
|        - | 1036 | `			}` |
|       23 | 1037 | `			if( aMap[g] == SXU32_HIGH ){` |
|       21 | 1038 | `				aMap[g] = pT->nNextGroup;` |
|       21 | 1039 | `				pT->nNextGroup++;` |
|        9 | 1040 | `			}` |
|       23 | 1041 | `			OvAddCode(pVm,pScope,pT,aAlt[i].nType,&aAlt[i].sClass,aMap[g]);` |
|       13 | 1042 | `		}` |
|      774 | 1043 | `	}else if( nType != 0 ){` |
|      355 | 1044 | `		OvAddCode(pVm,pScope,pT,nType,pClass,pT->nNextGroup);` |
|      175 | 1045 | `	}` |
|      781 | 1046 | `	if( bNullable ){` |
|       17 | 1047 | `		OvAddAtom(pT,OVB_NULL,0,pT->nNextGroup);` |
|        7 | 1048 | `	}` |
|        - | 1049 | ``	/* Nothing written at all: an ABSENCE, which is not the same as `mixed` --`` |
|        - | 1050 | `	 * php skips the check on the side that has none, so a missing PARAMETER type` |
|        - | 1051 | `	 * accepts any parent and a missing RETURN type is refused under a declared` |
|        - | 1052 | ``	 * one (`f(): int` overridden by `f()` is a fatal, `f(): mixed` is not). */`` |
|      781 | 1053 | `	if( pT->nAtom == 0 && !pT->bMixed && !pT->bNever && !pT->bUnknown ){` |
|      421 | 1054 | `		pT->bAbsent = 1;` |
|      208 | 1055 | `	}` |
|      393 | 1056 | `}` |
|      136 | 1057 | `static void OvFromArg(ph7_vm *pVm,ph7_class *pScope,ph7_vm_func_arg *pA,OvType *pT)` |
|        4 | 1058 | `{` |
|      208 | 1059 | `	OvFromDecl(pVm,pScope,pT,pA->nType,&pA->sClass,&pA->aUnionAlts,` |
|      136 | 1060 | `		(pA->iFlags & VM_FUNC_ARG_NULLABLE) != 0);` |
|      140 | 1061 | `}` |
|      640 | 1062 | `static void OvFromReturn(ph7_vm *pVm,ph7_class *pScope,ph7_vm_func *pF,OvType *pT)` |
|        5 | 1063 | `{` |
|      965 | 1064 | `	OvFromDecl(pVm,pScope,pT,pF->nReturnType,&pF->sReturnClass,&pF->aReturnUnion,` |
|      640 | 1065 | `		(pF->iFlags & VM_FUNC_RETURN_NULLABLE) != 0);` |
|      645 | 1066 | `}` |
|        - | 1067 | `/*` |
|        - | 1068 | `` * Is the single atom *pC under the single atom *pP? `object` is over every class`` |
|        - | 1069 | `` * (and over `static`, which IS one), `bool` is over `false` and `true`, and`` |
|        - | 1070 | `` * `static` is under any class the declaring class is an instance of -- but`` |
|        - | 1071 | `` * nothing except another `static` is under IT, since the called class may be a`` |
|        - | 1072 | ` * subclass nobody has written yet.` |
|        - | 1073 | ` */` |
|      192 | 1074 | `static int OvAtomLE(const OvAtom *pC,const OvAtom *pP,ph7_class *pSubScope)` |
|        5 | 1075 | `{` |
|      197 | 1076 | `	if( pP->nBit == OVB_OBJECT ){` |
|        3 | 1077 | `		return pC->nBit == OVB_OBJECT \|\| pC->nBit == OVB_CLS \|\| pC->nBit == OVB_STATIC;` |
|        - | 1078 | `	}` |
|      195 | 1079 | `	if( pP->nBit == OVB_BOOL ){` |
|        6 | 1080 | `		return pC->nBit == OVB_BOOL \|\| pC->nBit == OVB_FALSE \|\| pC->nBit == OVB_TRUE;` |
|        - | 1081 | `	}` |
|      191 | 1082 | `	if( pP->nBit == OVB_STATIC ){` |
|       30 | 1083 | `		if( pC->nBit == OVB_STATIC ){` |
|        3 | 1084 | `			return 1;` |
|        - | 1085 | `		}` |
|        - | 1086 | `		/* php's one exception, and a library really writes it: in a FINAL class` |
|        - | 1087 | ``		 * `self` IS `static`, because no subclass can ever exist for the called`` |
|        - | 1088 | `		 * class to be. It is the class ITSELF and nothing else -- naming the` |
|        - | 1089 | `		 * PARENT is still a fatal, even from a final child (an enum carries the` |
|        - | 1090 | `		 * final flag, so its own name works the same way). */` |
|       26 | 1091 | `		if( pC->nBit == OVB_CLS && pSubScope != 0 && pC->pCls == pSubScope` |
|       20 | 1092 | `		 && (pSubScope->iFlags & PH7_CLASS_FINAL) != 0 ){` |
|       17 | 1093 | `			return 1;` |
|        - | 1094 | `		}` |
|       11 | 1095 | `		return 0;` |
|        - | 1096 | `	}` |
|      163 | 1097 | `	if( pP->nBit == OVB_CLS ){` |
|       26 | 1098 | `		if( pC->nBit == OVB_CLS ){` |
|       18 | 1099 | `			return PH7_VmInstanceOf(pC->pCls,pP->pCls) ? 1 : 0;` |
|        - | 1100 | `		}` |
|       11 | 1101 | `		if( pC->nBit == OVB_STATIC ){` |
|        8 | 1102 | `			return (pSubScope && PH7_VmInstanceOf(pSubScope,pP->pCls)) ? 1 : 0;` |
|        - | 1103 | `		}` |
|        3 | 1104 | `		return 0;` |
|        - | 1105 | `	}` |
|      141 | 1106 | `	return pC->nBit == pP->nBit;` |
|      101 | 1107 | `}` |
|        - | 1108 | `/* Gc ⊆ Gp: every atom of the parent group has some atom of the child group under` |
|        - | 1109 | ` * it (an intersection is under X as soon as ONE of its members is). */` |
|      190 | 1110 | `static int OvGroupLE(const OvType *pC,sxu32 gC,const OvType *pP,sxu32 gP,ph7_class *pSubScope)` |
|        5 | 1111 | `{` |
|        - | 1112 | `	int i, j;` |
|      385 | 1113 | `	for( j = 0 ; j < pP->nAtom ; ++j ){` |
|      219 | 1114 | `		int bCovered = 0;` |
|      219 | 1115 | `		if( pP->a[j].nGroup != gP ){` |
|       26 | 1116 | `			continue;` |
|        - | 1117 | `		}` |
|      407 | 1118 | `		for( i = 0 ; i < pC->nAtom && !bCovered ; ++i ){` |
|      215 | 1119 | `			if( pC->a[i].nGroup == gC && OvAtomLE(&pC->a[i],&pP->a[j],pSubScope) ){` |
|      173 | 1120 | `				bCovered = 1;` |
|       84 | 1121 | `			}` |
|      110 | 1122 | `		}` |
|      197 | 1123 | `		if( !bCovered ){` |
|       29 | 1124 | `			return 0;` |
|        - | 1125 | `		}` |
|       89 | 1126 | `	}` |
|      171 | 1127 | `	return 1;` |
|      100 | 1128 | `}` |
|        - | 1129 | `/* child ⊆ parent (pSubScope is the SUBTYPE side's declaring class, which is what` |
|        - | 1130 | `` * a `static` atom there stands for). Both are normalized and neither is`` |
|        - | 1131 | ` * absent/mixed/never/unknown -- OvCheck settled those. */` |
|      172 | 1132 | `static int OvSubtype(const OvType *pC,const OvType *pP,ph7_class *pSubScope)` |
|        5 | 1133 | `{` |
|        - | 1134 | `	sxu32 gC, gP;` |
|      177 | 1135 | `	int bAnyC = 0;` |
|      343 | 1136 | `	for( gC = 0 ; gC < pC->nNextGroup ; ++gC ){` |
|      187 | 1137 | `		int i, bHasC = 0, bCovered = 0;` |
|      197 | 1138 | `		for( i = 0 ; i < pC->nAtom ; ++i ){` |
|      197 | 1139 | `			if( pC->a[i].nGroup == gC ){ bHasC = 1; break; }` |
|        8 | 1140 | `		}` |
|      187 | 1141 | `		if( !bHasC ){` |
|      ! 0 | 1142 | `			continue;` |
|        - | 1143 | `		}` |
|      187 | 1144 | `		bAnyC = 1;` |
|      377 | 1145 | `		for( gP = 0 ; gP < pP->nNextGroup && !bCovered ; ++gP ){` |
|      195 | 1146 | `			int j, bHasP = 0;` |
|      203 | 1147 | `			for( j = 0 ; j < pP->nAtom ; ++j ){` |
|      203 | 1148 | `				if( pP->a[j].nGroup == gP ){ bHasP = 1; break; }` |
|        7 | 1149 | `			}` |
|      195 | 1150 | `			if( bHasP && OvGroupLE(pC,gC,pP,gP,pSubScope) ){` |
|      171 | 1151 | `				bCovered = 1;` |
|       83 | 1152 | `			}` |
|      100 | 1153 | `		}` |
|      187 | 1154 | `		if( !bCovered ){` |
|       20 | 1155 | `			return 0;` |
|        - | 1156 | `		}` |
|       88 | 1157 | `	}` |
|      161 | 1158 | `	return bAnyC;` |
|       91 | 1159 | `}` |
|        - | 1160 | `#define OV_OK   0 /* the pair is compatible */` |
|        - | 1161 | `#define OV_BAD  1 /* php refuses it */` |
|        - | 1162 | `/*` |
|        - | 1163 | ` * One declared-type pair, in one variance direction. bCovariant = 1 for a return` |
|        - | 1164 | ` * type (the child must be UNDER the parent), 0 for a parameter (over it).` |
|        - | 1165 | ` *` |
|        - | 1166 | ` * The two ABSENCES are asymmetric and that asymmetry is php's: the side with no` |
|        - | 1167 | ` * declared type is simply not checked, so a parameter the CHILD left untyped is` |
|        - | 1168 | ` * always fine and a return the child left untyped is a fatal under any declared` |
|        - | 1169 | `` * parent -- `mixed` included, even though `mixed` is the top type.`` |
|        - | 1170 | ` */` |
|      388 | 1171 | `static int OvCheck(const OvType *pP,const OvType *pC,int bCovariant,` |
|        - | 1172 | `	ph7_class *pParentScope,ph7_class *pChildScope)` |
|        5 | 1173 | `{` |
|      393 | 1174 | `	const OvType *pSub = bCovariant ? pC : pP;   /* must be the subtype */` |
|      393 | 1175 | `	const OvType *pSup = bCovariant ? pP : pC;` |
|        - | 1176 | ``	/* `static` is decided against the scope of whichever side is the SUBTYPE --`` |
|        - | 1177 | `	 * the class whose called-class it stands for. */` |
|      393 | 1178 | `	ph7_class *pSubScope = bCovariant ? pChildScope : pParentScope;` |
|        - | 1179 | `	int bSubVoid, bSupVoid, i;` |
|      393 | 1180 | `	if( bCovariant && pP->bAbsent ){` |
|      179 | 1181 | `		return OV_OK;   /* nothing to be under */` |
|        - | 1182 | `	}` |
|      219 | 1183 | `	if( bCovariant && pC->bAbsent ){` |
|        - | 1184 | `		/* The ONE place an absent type is not simply the top type: a child that` |
|        - | 1185 | `		 * declares no RETURN type is refused under any parent that declares one,` |
|        - | 1186 | ``		 * `mixed` included. Everywhere else absence reads as `mixed` below. */`` |
|      ! 0 | 1187 | `		return OV_BAD;` |
|        - | 1188 | `	}` |
|      219 | 1189 | `	if( !bCovariant && pC->bAbsent ){` |
|       39 | 1190 | `		return OV_OK;   /* an untyped parameter accepts whatever the parent's did */` |
|        - | 1191 | `	}` |
|      183 | 1192 | `	if( pP->bUnknown \|\| pC->bUnknown ){` |
|      ! 0 | 1193 | `		return OV_OK;   /* a shape this lattice does not model -- accept */` |
|        - | 1194 | `	}` |
|        - | 1195 | ``	/* `void` pairs with `void` and with nothing else -- not even with `mixed`,`` |
|        - | 1196 | `	 * which is over every other type. Decided before the top/bottom shortcuts. */` |
|      183 | 1197 | `	bSubVoid = bSupVoid = 0;` |
|      367 | 1198 | `	for( i = 0 ; i < pSub->nAtom ; ++i ){ if( pSub->a[i].nBit == OVB_VOID ) bSubVoid = 1; }` |
|      375 | 1199 | `	for( i = 0 ; i < pSup->nAtom ; ++i ){ if( pSup->a[i].nBit == OVB_VOID ) bSupVoid = 1; }` |
|      183 | 1200 | `	if( bSubVoid != bSupVoid && !pSub->bNever ){` |
|        3 | 1201 | `		return OV_BAD;` |
|        - | 1202 | `	}` |
|      181 | 1203 | `	if( pSub->bNever ){` |
|        3 | 1204 | `		return OV_OK;   /* the bottom type is under everything */` |
|        - | 1205 | `	}` |
|      179 | 1206 | `	if( pSup->bMixed \|\| pSup->bAbsent ){` |
|        3 | 1207 | `		return OV_OK;   /* ...and the top type is over everything */` |
|        - | 1208 | `	}` |
|      177 | 1209 | `	if( pSub->bMixed \|\| pSub->bAbsent \|\| pSup->bNever ){` |
|      ! 0 | 1210 | `		return OV_BAD;` |
|        - | 1211 | `	}` |
|      177 | 1212 | `	return OvSubtype(pSub,pSup,pSubScope) ? OV_OK : OV_BAD;` |
|      199 | 1213 | `}` |
|        - | 1214 | `/*` |
|        - | 1215 | ` * ---------------------------------------------------------------------------` |
|        - | 1216 | ` * The ARITY half, which is not the type lattice's: php asks whether every call` |
|        - | 1217 | ` * the parent's declaration accepts can reach the child.` |
|        - | 1218 | ` * ---------------------------------------------------------------------------` |
|        - | 1219 | ` */` |
|        - | 1220 | `/* php's required_num_args: how many arguments a caller MUST supply. */` |
|      608 | 1221 | `static sxu32 OvReqArgs(ph7_vm_func *pF)` |
|        5 | 1222 | `{` |
|      613 | 1223 | `	ph7_vm_func_arg *a = (ph7_vm_func_arg *)SySetBasePtr(&pF->aArgs);` |
|      613 | 1224 | `	sxu32 n = SySetUsed(&pF->aArgs), i, nReq = 0;` |
|      809 | 1225 | `	for( i = 0 ; i < n ; ++i ){` |
|      201 | 1226 | `		if( (a[i].iFlags & VM_FUNC_ARG_VARIADIC) \|\| SySetUsed(&a[i].aByteCode) > 0 ){` |
|       56 | 1227 | `			continue; /* a variadic tail and a defaulted parameter are both optional */` |
|        - | 1228 | `		}` |
|      149 | 1229 | `		nReq = i + 1;` |
|       77 | 1230 | `	}` |
|      613 | 1231 | `	return nReq;` |
|        5 | 1232 | `}` |
|      640 | 1233 | `static int OvIsVariadic(ph7_vm_func *pF)` |
|        5 | 1234 | `{` |
|      645 | 1235 | `	ph7_vm_func_arg *a = (ph7_vm_func_arg *)SySetBasePtr(&pF->aArgs);` |
|      645 | 1236 | `	sxu32 n = SySetUsed(&pF->aArgs);` |
|      645 | 1237 | `	return n > 0 && (a[n-1].iFlags & VM_FUNC_ARG_VARIADIC) != 0;` |
|        5 | 1238 | `}` |
|        - | 1239 | `/* The parameter that ANSWERS position i: the one declared there, or the variadic` |
|        - | 1240 | ` * tail, which keeps answering for every position past its own. */` |
|      156 | 1241 | `static ph7_vm_func_arg * OvArgAt(ph7_vm_func *pF,sxu32 i)` |
|        4 | 1242 | `{` |
|      160 | 1243 | `	ph7_vm_func_arg *a = (ph7_vm_func_arg *)SySetBasePtr(&pF->aArgs);` |
|      160 | 1244 | `	sxu32 n = SySetUsed(&pF->aArgs);` |
|      160 | 1245 | `	if( i < n ){` |
|      148 | 1246 | `		return &a[i];` |
|        - | 1247 | `	}` |
|       14 | 1248 | `	if( n > 0 && (a[n-1].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        5 | 1249 | `		return &a[n-1];` |
|        - | 1250 | `	}` |
|       10 | 1251 | `	return 0;` |
|       82 | 1252 | `}` |
|        - | 1253 | `/*` |
|        - | 1254 | ` * Check a child method's signature against the parent method it overrides.` |
|        - | 1255 | ` * Emits a PHP-style "Declaration of … must be compatible …" fatal on a clear` |
|        - | 1256 | ` * incompatibility.` |
|        - | 1257 | ` *` |
|        - | 1258 | `` * bCtorExempt tells the two regimes php has for `__construct` apart: an`` |
|        - | 1259 | ` * INHERITED constructor is exempt from variance entirely (a child may declare` |
|        - | 1260 | ` * whatever it likes), while one an INTERFACE declares is checked like any other` |
|        - | 1261 | `` * method -- `interface I { __construct(int $a); }` really does constrain every`` |
|        - | 1262 | ` * implementor's constructor.` |
|        - | 1263 | ` */` |
|   723001 | 1264 | `PH7_PRIVATE sxi32 PH7_ClassCheckOverrideCompat(ph7_gen_state *pGen, ph7_class *pBase, ph7_class *pSub,` |
|        - | 1265 | `	ph7_class_method *pParent, ph7_class_method *pChild, int bCtorExempt)` |
|        5 | 1266 | `{` |
|   723006 | 1267 | `	ph7_vm *pVm = pGen->pVm;` |
|   723006 | 1268 | `	ph7_vm_func *pPF = &pParent->sFunc;` |
|   723006 | 1269 | `	ph7_vm_func *pCF = &pChild->sFunc;` |
|   723006 | 1270 | `	SyString *pMName = &pCF->sName;` |
|        - | 1271 | `	/* php names the class that DECLARED each side, not the one the walk reached it` |
|        - | 1272 | ``	 * through: `class A { f() } class B extends A {} class C extends B { f() }` is`` |
|        - | 1273 | ``	 * `C::f() must be compatible with A::f()`, and a trait method belongs to the`` |
|        - | 1274 | `` 	 * class that composed it. That owner is also what `self`, `parent` and `static` `` |
|        - | 1275 | `	 * in either declaration resolve against, so the two questions are one. */` |
|   723006 | 1276 | `	ph7_class *pChildOwner = PH7_VmMemberOwnerClass((ph7_class *)pCF->pUserData,pSub);` |
|   723006 | 1277 | `	ph7_class *pParentOwner = PH7_VmMemberOwnerClass((ph7_class *)pPF->pUserData,pBase);` |
|        - | 1278 | `	sxu32 nPArg, nCArg, nPos, k;` |
|        - | 1279 | `	int bPVar, bCVar;` |
|   723006 | 1280 | `	int bBad = 0;` |
|        - | 1281 | `	OvType sP, sC;` |
|   723006 | 1282 | `	if( pChildOwner == 0 ){` |
|      ! 0 | 1283 | `		pChildOwner = pSub;` |
|      ! 0 | 1284 | `	}` |
|   723006 | 1285 | `	if( pParentOwner == 0 ){` |
|      ! 0 | 1286 | `		pParentOwner = pBase;` |
|      ! 0 | 1287 | `	}` |
|   723001 | 1288 | `	if( bCtorExempt` |
|   722282 | 1289 | `	 && pMName->nByte == sizeof("__construct")-1` |
|   515081 | 1290 | `	 && SyStrnmicmp(pMName->zString,"__construct",pMName->nByte) == 0 ){` |
|   229892 | 1291 | `		return SXRET_OK;` |
|        - | 1292 | `	}` |
|        - | 1293 | `	/*` |
|        - | 1294 | `	 * A NATIVE method declares its parameters in a zSig STRING, so its aArgs set` |
|        - | 1295 | `	 * is empty and there is nothing here to compare against. Reading that as` |
|        - | 1296 | `	 * "declares no parameters" made every override of one incompatible: a user` |
|        - | 1297 | `	 * class extending DOMDocument, a Reflection class or a native enum's own` |
|        - | 1298 | `	 * cases()/from() all fataled on a declaration php accepts. An` |
|        - | 1299 | `	 * engine-declared signature is compatible by construction, on either side.` |
|        - | 1300 | `	 */` |
|   493119 | 1301 | `	if( ((pPF->iFlags \| pCF->iFlags) & VM_FUNC_NATIVE) != 0 ){` |
|   492799 | 1302 | `		return SXRET_OK;` |
|        - | 1303 | `	}` |
|        - | 1304 | `	/* Return type -- covariant. */` |
|      325 | 1305 | `	OvFromReturn(pVm,pParentOwner,pPF,&sP);` |
|      325 | 1306 | `	OvFromReturn(pVm,pChildOwner,pCF,&sC);` |
|      325 | 1307 | `	bBad = OvCheck(&sP,&sC,/* bCovariant */ 1,pParentOwner,pChildOwner) == OV_BAD;` |
|        - | 1308 | `	/*` |
|        - | 1309 | `	 * Arity, php's three rules -- every call the parent's declaration accepts must` |
|        - | 1310 | `	 * reach the child. A VARIADIC signature is not the exception this used to make` |
|        - | 1311 | `` 	 * of it (the whole rule stood aside, so `f(string ...$b)` overridden by `f()` `` |
|        - | 1312 | `	 * compiled): it is the tail that keeps ANSWERING past its own position.` |
|        - | 1313 | `	 */` |
|      325 | 1314 | `	nPArg = SySetUsed(&pPF->aArgs);` |
|      325 | 1315 | `	nCArg = SySetUsed(&pCF->aArgs);` |
|      325 | 1316 | `	bPVar = OvIsVariadic(pPF);` |
|      325 | 1317 | `	bCVar = OvIsVariadic(pCF);` |
|      325 | 1318 | `	if( !bBad && bPVar && !bCVar ){` |
|        3 | 1319 | `		bBad = 1;  /* the parent takes any number; the child must too */` |
|        1 | 1320 | `	}` |
|      325 | 1321 | `	if( !bBad && OvReqArgs(pCF) > OvReqArgs(pPF) ){` |
|        3 | 1322 | `		bBad = 1;  /* the child DEMANDS an argument the parent's callers do not pass */` |
|        1 | 1323 | `	}` |
|      325 | 1324 | `	if( !bBad && !bCVar && nCArg < nPArg ){` |
|        8 | 1325 | `		bBad = 1;  /* ...and it must still ACCEPT every one they do */` |
|        3 | 1326 | `	}` |
|        - | 1327 | `	/* Every position both signatures answer: the type contravariantly, and the` |
|        - | 1328 | `	 * by-reference-ness php requires to MATCH exactly (nothing checked it here). A` |
|        - | 1329 | `	 * position only the CHILD declares is unconstrained -- the arity rules above` |
|        - | 1330 | `	 * already made it optional. */` |
|      325 | 1331 | `	nPos = nPArg > nCArg ? nPArg : nCArg;` |
|      401 | 1332 | `	for( k = 0 ; !bBad && k < nPos ; ++k ){` |
|       82 | 1333 | `		ph7_vm_func_arg *pPa = OvArgAt(pPF,k);` |
|       82 | 1334 | `		ph7_vm_func_arg *pCa = OvArgAt(pCF,k);` |
|       82 | 1335 | `		if( pPa == 0 \|\| pCa == 0 ){` |
|       10 | 1336 | `			continue;` |
|        - | 1337 | `		}` |
|       74 | 1338 | `		if( ((pPa->iFlags ^ pCa->iFlags) & VM_FUNC_ARG_BY_REF) != 0 ){` |
|        3 | 1339 | `			bBad = 1;` |
|        3 | 1340 | `			break;` |
|        - | 1341 | `		}` |
|       72 | 1342 | `		OvFromArg(pVm,pParentOwner,pPa,&sP);` |
|       72 | 1343 | `		OvFromArg(pVm,pChildOwner,pCa,&sC);` |
|       72 | 1344 | `		bBad = OvCheck(&sP,&sC,/* bCovariant */ 0,pParentOwner,pChildOwner) == OV_BAD;` |
|       38 | 1345 | `	}` |
|      325 | 1346 | `	if( bBad ){` |
|        - | 1347 | `		SyBlob sChild, sParent;` |
|        - | 1348 | `		sxi32 rc;` |
|       34 | 1349 | `		SyBlobInit(&sChild,&pVm->sAllocator);` |
|       34 | 1350 | `		SyBlobInit(&sParent,&pVm->sAllocator);` |
|       34 | 1351 | `		PH7_ClassRenderDecl(pVm,pChildOwner,pCF,&sChild);` |
|       34 | 1352 | `		PH7_ClassRenderDecl(pVm,pParentOwner,pPF,&sParent);` |
|       49 | 1353 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pChild->nLine,` |
|        - | 1354 | `			"Declaration of %z::%z%.*s must be compatible with %z::%z%.*s",` |
|       15 | 1355 | `			&pChildOwner->sDisp,pMName,` |
|       30 | 1356 | `			OoDeclCLen(&sChild),(const char *)SyBlobData(&sChild),` |
|       15 | 1357 | `			&pParentOwner->sName,&pParent->sFunc.sName,` |
|       30 | 1358 | `			OoDeclCLen(&sParent),(const char *)SyBlobData(&sParent));` |
|       34 | 1359 | `		SyBlobRelease(&sChild);` |
|       34 | 1360 | `		SyBlobRelease(&sParent);` |
|       34 | 1361 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1362 | `			return SXERR_ABORT;` |
|        - | 1363 | `		}` |
|       15 | 1364 | `	}` |
|      325 | 1365 | `	return SXRET_OK;` |
|   361005 | 1366 | `}` |
|        - | 1367 | `/*` |
|        - | 1368 | ` * Every method the sub-INTERFACE declares ITSELF, judged against the same name in` |
|        - | 1369 | ` * one parent. php checks a restated interface method exactly as it checks an` |
|        - | 1370 | ` * overriding class method, and words the refusal the same way -- PHL checked` |
|        - | 1371 | ` * neither, and instead refused the restatement outright when it came from a` |
|        - | 1372 | ` * parent past the first (see the collected-parents comment in the interface` |
|        - | 1373 | ` * compiler).` |
|        - | 1374 | ` *` |
|        - | 1375 | ` * Called while hMethod still holds only this interface's own declarations, which` |
|        - | 1376 | ` * is the one moment the two sets are separable.` |
|        - | 1377 | ` */` |
|       70 | 1378 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceCheckRedeclare(ph7_gen_state *pGen,ph7_class *pSub,` |
|        - | 1379 | `	ph7_class *pParent)` |
|        4 | 1380 | `{` |
|        - | 1381 | `	SyHashEntry *pEntry;` |
|       74 | 1382 | `	SyHashResetLoopCursor(&pSub->hMethod);` |
|      149 | 1383 | `	while((pEntry = SyHashGetNextEntry(&pSub->hMethod)) != 0 ){` |
|       44 | 1384 | `		ph7_class_method *pOwn = (ph7_class_method *)pEntry->pUserData;` |
|       44 | 1385 | `		SyString *pName = &pOwn->sFunc.sName;` |
|       64 | 1386 | `		SyHashEntry *pUp = SyHashGet(&pParent->hMethod,` |
|       40 | 1387 | `			(const void *)pName->zString,pName->nByte);` |
|       51 | 1388 | `		if( pUp && PH7_ClassCheckOverrideCompat(&(*pGen),pParent,pSub,` |
|       21 | 1389 | `			(ph7_class_method *)pUp->pUserData,pOwn,0) == SXERR_ABORT ){` |
|      ! 0 | 1390 | `			return SXERR_ABORT;` |
|        - | 1391 | `		}` |
|        4 | 1392 | `	}` |
|       74 | 1393 | `	return SXRET_OK;` |
|       39 | 1394 | `}` |
|        - | 1395 | `/*` |
|        - | 1396 | ` * Perform an inheritance operation.` |
|        - | 1397 | ` * According to the PHP language reference manual` |
|        - | 1398 | ` *  When you extend a class, the subclass inherits all of the public and protected methods` |
|        - | 1399 | ` *  from the parent class. Unless a class Overwrites those methods, they will retain their original` |
|        - | 1400 | ` *  functionality.` |
|        - | 1401 | ` *  This is useful for defining and abstracting functionality, and permits the implementation` |
|        - | 1402 | ` *  of additional functionality in similar objects without the need to reimplement all of the shared` |
|        - | 1403 | ` *  functionality.` |
|        - | 1404 | ` *  Example #1 Inheritance Example` |
|        - | 1405 | ` * <?php` |
|        - | 1406 | ` * class foo` |
|        - | 1407 | ` * {` |
|        - | 1408 | ` *   public function printItem($string)` |
|        - | 1409 | ` *   {` |
|        - | 1410 | ` *       echo 'Foo: ' . $string . PHP_EOL;` |
|        - | 1411 | ` *   }` |
|        - | 1412 | ` *` |
|        - | 1413 | ` *   public function printPHP()` |
|        - | 1414 | ` *   {` |
|        - | 1415 | ` *       echo 'PHP is great.' . PHP_EOL;` |
|        - | 1416 | ` *   }` |
|        - | 1417 | ` * }` |
|        - | 1418 | ` * class bar extends foo` |
|        - | 1419 | ` * {` |
|        - | 1420 | ` *   public function printItem($string)` |
|        - | 1421 | ` *   {` |
|        - | 1422 | ` *       echo 'Bar: ' . $string . PHP_EOL;` |
|        - | 1423 | ` *   }` |
|        - | 1424 | ` * }` |
|        - | 1425 | ` * $foo = new foo();` |
|        - | 1426 | ` * $bar = new bar();` |
|        - | 1427 | ` * $foo->printItem('baz'); // Output: 'Foo: baz'` |
|        - | 1428 | ` * $foo->printPHP();       // Output: 'PHP is great'` |
|        - | 1429 | ` * $bar->printItem('baz'); // Output: 'Bar: baz'` |
|        - | 1430 | ` * $bar->printPHP();       // Output: 'PHP is great'` |
|        - | 1431 | ` *` |
|        - | 1432 | ` * This function return SXRET_OK if the inheritance operation was successfully performed.` |
|        - | 1433 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|        - | 1434 | ` * error message.` |
|        - | 1435 | ` */` |
|   769791 | 1436 | `PH7_PRIVATE sxi32 PH7_ClassInherit(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase)` |
|        5 | 1437 | `{` |
|        - | 1438 | `	ph7_class_method *pMeth;` |
|        - | 1439 | `	ph7_class_attr *pAttr;` |
|        - | 1440 | `	SyHashEntry *pEntry;` |
|        - | 1441 | `	SyString *pName;` |
|        - | 1442 | `	SySet aInherited; /* base attributes to prepend (see the copy loop below) */` |
|        - | 1443 | `	sxi32 rc;` |
|   769796 | 1444 | `	SySetInit(&aInherited,&pGen->pVm->sAllocator,sizeof(ph7_class_attr *));` |
|        - | 1445 | `	/* Install in the derived hashtable */` |
|   769796 | 1446 | `	rc = SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|   769796 | 1447 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1448 | `		SySetRelease(&aInherited);` |
|      ! 0 | 1449 | `		return rc;` |
|        - | 1450 | `	}` |
|        - | 1451 | `	/* readonly class inheritance (PHP 8.2): a readonly class may only extend a` |
|        - | 1452 | `	 * readonly class, and a non-readonly class may not extend a readonly one.` |
|        - | 1453 | `	 * A FINAL base is not one of these cases at all -- it cannot be extended by` |
|        - | 1454 | `	 * anything, and php reports only that. Both diagnostics used to fire for a` |
|        - | 1455 | ``	 * `final readonly` base and the readonly one was reported, which is the wrong`` |
|        - | 1456 | `	 * reason; BcMath\Number is the engine's first such class. */` |
|   769791 | 1457 | `	if( (pBase->iFlags & PH7_CLASS_FINAL) == 0` |
|   769794 | 1458 | `	 && (pBase->iFlags & PH7_CLASS_READONLY) != (pSub->iFlags & PH7_CLASS_READONLY) ){` |
|        5 | 1459 | `		if( pBase->iFlags & PH7_CLASS_READONLY ){` |
|        4 | 1460 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|        - | 1461 | `				"Non-readonly class %z cannot extend readonly class %z",` |
|        1 | 1462 | `				&pSub->sDisp,&pBase->sDisp);` |
|        2 | 1463 | `		}else{` |
|        4 | 1464 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|        - | 1465 | `				"Readonly class %z cannot extend non-readonly class %z",` |
|        1 | 1466 | `				&pSub->sDisp,&pBase->sDisp);` |
|        - | 1467 | `		}` |
|        5 | 1468 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1469 | `			SySetRelease(&aInherited);` |
|      ! 0 | 1470 | `			return SXERR_ABORT;` |
|        - | 1471 | `		}` |
|        2 | 1472 | `	}` |
|        - | 1473 | `	/* Mark as subclass BEFORE the members are copied. php's mangled storage name` |
|        - | 1474 | `	 * for a TRAIT-composed private names the class that composed it, found by` |
|        - | 1475 | `	 * walking the subclass's ANCESTRY (PH7_VmMemberOwnerClass) -- with pBase still` |
|        - | 1476 | `	 * unset the walk stopped at the trait, cached that answer on the attribute,` |
|        - | 1477 | `	 * and every later lookup then asked for a key the object's table did not hold. */` |
|   769796 | 1478 | `	pSub->pBase = pBase;` |
|        - | 1479 | `	/* A native class whose php-visible properties are LAZY passes that on: the` |
|        - | 1480 | `	 * attributes copied below keep their flags, so a subclass of DateInterval has` |
|        - | 1481 | `	 * the same ten to install, and the O(1) gate in front of the materialization` |
|        - | 1482 | `	 * walk has to see it on the SUBCLASS or the constructor's writes land nowhere. */` |
|   769796 | 1483 | `	if( pBase->iFlags & PH7_CLASS_LAZY_ATTR ){` |
|       27 | 1484 | `		pSub->iFlags \|= PH7_CLASS_LAZY_ATTR;` |
|       13 | 1485 | `	}` |
|        - | 1486 | `	/* Copy public/protected attributes from the base class */` |
|   769796 | 1487 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
|  7152939 | 1488 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|        - | 1489 | `		/* Make sure the private attributes are not redeclared in the subclass */` |
|  6383148 | 1490 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  6383148 | 1491 | `		pName = &pAttr->sName;` |
|  6383143 | 1492 | `		if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|  4429786 | 1493 | `		 && (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - | 1494 | `			/* A base's private INSTANCE property is a slot of its own on every` |
|        - | 1495 | `			 * object below it, filed under php's mangled storage name -- so it can` |
|        - | 1496 | `			 * never collide with a subclass member of the same name, and the` |
|        - | 1497 | `			 * redeclaration rules below have nothing to say about it. The subclass` |
|        - | 1498 | `			 * keeps its own declaration exactly where it wrote it. */` |
|  2481824 | 1499 | `			rc = SySetPut(&aInherited,(const void *)&pAttr);` |
|  2481824 | 1500 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1501 | `				SySetRelease(&aInherited);` |
|      ! 0 | 1502 | `				return rc;` |
|        - | 1503 | `			}` |
|  2481824 | 1504 | `			continue;` |
|        - | 1505 | `		}` |
|  3901329 | 1506 | `		if( (pEntry = SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|    23817 | 1507 | `			if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_FINAL))` |
|    11897 | 1508 | `				== (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_FINAL) ){` |
|        - | 1509 | `				/* Cannot override a final class constant (PHP 8.1). Report the` |
|        - | 1510 | `				 * class that originally declared it (pDeclClass) rather than the` |
|        - | 1511 | `				 * immediate base, so a multi-level chain matches PHP -- and a` |
|        - | 1512 | `				 * TRAIT-declared one belongs to the class that composed it.` |
|        - | 1513 | `				 * php reports it on the SUBCLASS's declaration line, not on the` |
|        - | 1514 | `				 * line the offending member sits on: the refusal is inheritance` |
|        - | 1515 | ``				 * talking, and inheritance happens where `extends` is written.`` |
|        - | 1516 | `				 * (Its final-METHOD twin below is the other rule -- php reports` |
|        - | 1517 | `				 * THAT one at the method.) */` |
|      ! 0 | 1518 | `				ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pBase);` |
|      ! 0 | 1519 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|        - | 1520 | `					"%z::%z cannot override final constant %z::%z",` |
|      ! 0 | 1521 | `					&pSub->sDisp,pName,&pOwner->sDisp,pName);` |
|      ! 0 | 1522 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1523 | `					SySetRelease(&aInherited);` |
|      ! 0 | 1524 | `					return SXERR_ABORT;` |
|        - | 1525 | `				}` |
|    23817 | 1526 | `			}else if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_FINAL))` |
|    11897 | 1527 | `				== PH7_CLASS_ATTR_FINAL ){` |
|        - | 1528 | `				/* PHP 8.4's final PROPERTY: no subclass may redeclare it, however the` |
|        - | 1529 | `				 * redeclaration is spelled -- a plain or static property of its own, a` |
|        - | 1530 | `				 * PROMOTED constructor parameter, or a trait it composes -- because all` |
|        - | 1531 | `				 * three land in the subclass's attribute table before inheritance runs.` |
|        - | 1532 | `				 * Same class-line rule and same declaring-class naming as the constant` |
|        - | 1533 | `				 * above. */` |
|        9 | 1534 | `				ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pBase);` |
|       12 | 1535 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|        3 | 1536 | `					"Cannot override final property %z::$%z",&pOwner->sDisp,pName);` |
|        9 | 1537 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1538 | `					SySetRelease(&aInherited);` |
|      ! 0 | 1539 | `					return SXERR_ABORT;` |
|        - | 1540 | `				}` |
|        3 | 1541 | `			}` |
|        - | 1542 | `			/* A child MAY redeclare a base's private property: php treats the two` |
|        - | 1543 | `			 * as independent members (each private to its declaring class), with no` |
|        - | 1544 | `			 * diagnostic. PH7 warned here, which is wrong — the child's entry simply` |
|        - | 1545 | `			 * shadows the base's in the by-name attribute table.` |
|        - | 1546 | `			 *` |
|        - | 1547 | `			 * Ordering: php keeps an overridden INSTANCE property at the position` |
|        - | 1548 | `` 			 * the BASE declared it (`class G{$g1;$g2;} class H extends G{$h;$g1;}` `` |
|        - | 1549 | `			 * iterates g1,g2,h — not g2,h,g1). Re-collect the CHILD's definition,` |
|        - | 1550 | `			 * whose default value wins, and drop its current entry so the prepend` |
|        - | 1551 | `			 * below re-inserts it in base order. Statics/constants are not part of` |
|        - | 1552 | `			 * instance iteration, so they keep their existing slot. */` |
|    23822 | 1553 | `			if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|    23818 | 1554 | `				ph7_class_attr *pOwn = (ph7_class_attr *)pEntry->pUserData;` |
|    23818 | 1555 | `				SyHashDeleteEntry(&pSub->hAttr,(const void *)pName->zString,pName->nByte,0);` |
|    23818 | 1556 | `				rc = SySetPut(&aInherited,(const void *)&pOwn);` |
|    23818 | 1557 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 1558 | `					SySetRelease(&aInherited);` |
|      ! 0 | 1559 | `					return rc;` |
|        - | 1560 | `				}` |
|    11890 | 1561 | `			}` |
|    23822 | 1562 | `			continue;` |
|        - | 1563 | `		}` |
|        - | 1564 | `		/* Collect the attribute. A private STATIC comes down too: php keeps one` |
|        - | 1565 | ``		 * in the child's property table -- `B::$s` on `class A { private static`` |
|        - | 1566 | ``		 * $s; }` is "Cannot access private property B::$s", the visibility`` |
|        - | 1567 | `		 * refusal, and not the undeclared-static one -- and nothing else could` |
|        - | 1568 | ``		 * find it, so `static::$s` from a base method with the subclass as its`` |
|        - | 1569 | `		 * late-static-binding target reported its own static as undeclared. Its` |
|        - | 1570 | `		 * storage is the DECLARING class's slot either way (nIdx is shared), so` |
|        - | 1571 | `		 * this is a second name for one static, exactly as php has it.` |
|        - | 1572 | `		 *` |
|        - | 1573 | `		 * These are gathered rather than installed here because php orders an` |
|        - | 1574 | `		 * instance's properties BASE-DECLARED FIRST, then the subclass's own,` |
|        - | 1575 | `		 * then trait members — while inheritance runs AFTER the subclass body` |
|        - | 1576 | `		 * has already filled hAttr. They are prepended below. */` |
|  3877512 | 1577 | `		rc = SySetPut(&aInherited,(const void *)&pAttr);` |
|  3877512 | 1578 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1579 | `			SySetRelease(&aInherited);` |
|      ! 0 | 1580 | `			return rc;` |
|        - | 1581 | `		}` |
|        5 | 1582 | `	}` |
|        - | 1583 | `	/* Prepend the collected base attributes so hAttr reads base-first. The` |
|        - | 1584 | `	 * table's iteration list is what every object-iteration consumer walks` |
|        - | 1585 | `	 * (var_dump/print_r, get_object_vars, foreach, (array) casts, json_encode),` |
|        - | 1586 | `	 * so this ordering is user-visible — json_encode emits its keys in exactly` |
|        - | 1587 | `	 * this order. SyHashInsert is a HEAD insert, so walking the collected set` |
|        - | 1588 | `	 * backwards leaves the base's own declaration order at the front. */` |
|   769796 | 1589 | `	if( SySetUsed(&aInherited) > 0 ){` |
|   769344 | 1590 | `		ph7_class_attr **apInherited = (ph7_class_attr **)SySetBasePtr(&aInherited);` |
|   769344 | 1591 | `		sxu32 n = SySetUsed(&aInherited);` |
|  7152483 | 1592 | `		while( n > 0 ){` |
|  6383144 | 1593 | `			ph7_class_attr *pIn = apInherited[--n];` |
|        - | 1594 | `			/* Under php's STORAGE name, which is the plain one for everything but` |
|        - | 1595 | `			 * an inherited private instance property. */` |
|  6383144 | 1596 | `			const SyString *pKey = PH7_ClassAttrStorageName(pGen->pVm,pSub,pIn);` |
|  6383144 | 1597 | `			if( pKey != &pIn->sName ){` |
|       62 | 1598 | `				pSub->iFlags \|= PH7_CLASS_SHADOW_PROP;` |
|        - | 1599 | `				/* ...and WHICH plain name it is hidden under, so the per-access` |
|        - | 1600 | `				 * screen in OoScopePrivateAttr can answer without a lookup. */` |
|       91 | 1601 | `				pSub->nShadowName \|= OoShadowNameBit(SyHashKey(&pSub->hAttr,` |
|       58 | 1602 | `					(const void *)SyStringData(&pIn->sName),SyStringLength(&pIn->sName)));` |
|       29 | 1603 | `			}` |
|  6383144 | 1604 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pKey->zString,pKey->nByte,pIn);` |
|  6383144 | 1605 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1606 | `				SySetRelease(&aInherited);` |
|      ! 0 | 1607 | `				return rc;` |
|        - | 1608 | `			}` |
|        5 | 1609 | `		}` |
|   384136 | 1610 | `	}` |
|        - | 1611 | `	/* Inherit the base's CONSTANTS (separate namespace: hConst). Constants are not` |
|        - | 1612 | `	 * part of instance iteration, so no base-first ordering dance is needed — a plain` |
|        - | 1613 | `	 * copy of every constant the subclass did not itself redeclare. A subclass that` |
|        - | 1614 | `	 * redeclares a base FINAL constant is a fatal (PHP 8.1). */` |
|   769796 | 1615 | `	SyHashResetLoopCursor(&pBase->hConst);` |
|  2760347 | 1616 | `	while((pEntry = SyHashGetNextEntry(&pBase->hConst)) != 0 ){` |
|        - | 1617 | `		SyHashEntry *pOwn;` |
|  1990556 | 1618 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  1990556 | 1619 | `		pName = &pAttr->sName;` |
|  1990556 | 1620 | `		if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|        - | 1621 | ``			/* A private CONSTANT is not inherited at all: php answers `B::K` with`` |
|        - | 1622 | `			 * "Undefined constant B::K", never with the visibility refusal it words` |
|        - | 1623 | ``			 * for `A::K`. Copying it down said "Cannot access private constant`` |
|        - | 1624 | ``			 * B::K" -- and let `static::K` from a base method find one php does`` |
|        - | 1625 | ``			 * not. A base method's own `self::K` resolves against A directly. */`` |
|       19 | 1626 | `			continue;` |
|        - | 1627 | `		}` |
|  1990540 | 1628 | `		if( (pOwn = SyHashGet(&pSub->hConst,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|       22 | 1629 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_FINAL) ){` |
|        - | 1630 | `				/* Cannot override a final class constant. Report the class that` |
|        - | 1631 | `				 * originally declared it (pDeclClass) for a multi-level chain -- and a` |
|        - | 1632 | `				 * TRAIT-declared one belongs to the class that composed it. */` |
|        6 | 1633 | `				ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pBase);` |
|        8 | 1634 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|        - | 1635 | `					"%z::%z cannot override final constant %z::%z",` |
|        2 | 1636 | `					&pSub->sDisp,pName,&pOwner->sDisp,pName);` |
|        6 | 1637 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1638 | `					SySetRelease(&aInherited);` |
|      ! 0 | 1639 | `					return SXERR_ABORT;` |
|        - | 1640 | `				}` |
|        2 | 1641 | `			}` |
|       22 | 1642 | `			continue;` |
|        - | 1643 | `		}` |
|  1990522 | 1644 | `		rc = SyHashInsertTail(&pSub->hConst,(const void *)pName->zString,pName->nByte,pAttr);` |
|  1990522 | 1645 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1646 | `			SySetRelease(&aInherited);` |
|      ! 0 | 1647 | `			return rc;` |
|        - | 1648 | `		}` |
|        5 | 1649 | `	}` |
|   769796 | 1650 | `	SySetRelease(&aInherited);` |
|   769796 | 1651 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
| 14275303 | 1652 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|        - | 1653 | `		SyHashEntry *pOwn;` |
|        - | 1654 | `		SyString sKey;` |
|        - | 1655 | `		/* Make sure the private/final methods are not redeclared in the subclass.` |
|        - | 1656 | `		 * The identity inherited is the base's HASH KEY, not sFunc.sName — the same` |
|        - | 1657 | `		 * rule PH7_ClassUseTrait copies a trait by. A trait adaptation leaves the` |
|        - | 1658 | `		 * composed class holding entries whose key is the name the class ANSWERS to` |
|        - | 1659 | ``		 * while the method struct keeps its original name: `B::m as mB` is the key`` |
|        - | 1660 | `` 		 * `mB` over a struct still called `m`, and `A::m insteadof B` is the key `m` `` |
|        - | 1661 | ``		 * over A's struct. Keying the copy off sFunc.sName re-filed both under `m`,`` |
|        - | 1662 | `		 * so a subclass of the composing class lost the alias entirely and took` |
|        - | 1663 | ``		 * whichever of the two the hash walk reached last as its `m` — the insteadof`` |
|        - | 1664 | `		 * choice, silently reversed. */` |
| 13505512 | 1665 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
| 13505512 | 1666 | `		SyStringInitFromBuf(&sKey,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
| 13505512 | 1667 | `		pName = &sKey;` |
| 13505512 | 1668 | `		if( (pOwn = SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|   721584 | 1669 | `			ph7_class_method *pOwnMeth = (ph7_class_method *)pOwn->pUserData;` |
|   721584 | 1670 | `			ph7_class *pOwnDecl = (ph7_class *)pOwnMeth->sFunc.pUserData;` |
|   721579 | 1671 | `			if( (pOwnMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0` |
|   360293 | 1672 | `			 && (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0` |
|       13 | 1673 | `			 && pOwnDecl && (pOwnDecl->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|        - | 1674 | ``				/* A trait's `abstract` is a REQUIREMENT, not a member, and php lets an`` |
|        - | 1675 | ``				 * INHERITED method satisfy it: `trait T { abstract function need(); }`` |
|        - | 1676 | ``				 * class P { function need(){} } class C extends P { use T; }` composes`` |
|        - | 1677 | `				 * there and was "Class C contains 1 abstract method" here, because the` |
|        - | 1678 | `				 * trait is applied before the base is inherited and the requirement then` |
|        - | 1679 | `				 * shadowed the very method that answers it. The satisfying declaration` |
|        - | 1680 | `				 * still has to be COMPATIBLE with the requirement -- and php words that` |
|        - | 1681 | `				 * one the other way round, naming the class that PROVIDES the method and` |
|        - | 1682 | `				 * the trait that asked for it. */` |
|       10 | 1683 | `				rc = PH7_ClassCheckOverrideCompat(&(*pGen),pOwnDecl,pBase,pOwnMeth,pMeth,1);` |
|       10 | 1684 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1685 | `					return SXERR_ABORT;` |
|        - | 1686 | `				}` |
|       10 | 1687 | `				pOwn->pUserData = (void *)pMeth;` |
|   361296 | 1688 | `				continue;` |
|        - | 1689 | `			}` |
|   721576 | 1690 | `			if( pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|        - | 1691 | `				/* php: a base's PRIVATE method is never overridden — the child's` |
|        - | 1692 | `				 * declaration is an independent member of the same name, so neither` |
|        - | 1693 | `				 * the final rule nor the signature-compatibility rule applies to it` |
|        - | 1694 | `				 * (zend's do_inherit_method skips both for a private parent). PHL` |
|        - | 1695 | ``				 * ran both: `class A { private function m($a){} } class B extends A`` |
|        - | 1696 | ``				 * { private function m($a,$b,$c){} }` was a fatal php compiles, and`` |
|        - | 1697 | ``				 * `final private` in the base fataled every child that reused the`` |
|        - | 1698 | `				 * name — php only WARNS at the final-private DECLARATION and lets` |
|        - | 1699 | `				 * the child have the name. */` |
|   721570 | 1700 | `			}else if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|        - | 1701 | `				/* php: "Cannot override final method A::test()" */` |
|        8 | 1702 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_method *)pOwn->pUserData)->nLine,` |
|        - | 1703 | `					"Cannot override final method %z::%z()",` |
|        2 | 1704 | `					&pBase->sDisp,pName);` |
|        2 | 1705 | `				(void)pSub;` |
|        6 | 1706 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1707 | `					return SXERR_ABORT;` |
|        - | 1708 | `				}` |
|        4 | 1709 | `			}else{` |
|        - | 1710 | `				/* Check the override's signature is compatible with the parent's. */` |
|  1081837 | 1711 | `				rc = PH7_ClassCheckOverrideCompat(&(*pGen),pBase,pSub,pMeth,` |
|   721555 | 1712 | `					(ph7_class_method *)pOwn->pUserData,1);` |
|   721560 | 1713 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1714 | `					return SXERR_ABORT;` |
|        - | 1715 | `				}` |
|        - | 1716 | `			}` |
|   721576 | 1717 | `			continue;` |
|        - | 1718 | `		}` |
|        - | 1719 | `		/* Install the method. php: a base class's private method is in the child's` |
|        - | 1720 | `		 * table too — an inherited public method calling $this->priv() must find it,` |
|        - | 1721 | ``		 * and the LOOKUP has to find it for php's answer to `B::p()` to be`` |
|        - | 1722 | `		 * "Call to private method A::p() from global scope" rather than` |
|        - | 1723 | `		 * "Call to undefined method B::p()". The call-site visibility check binds by` |
|        - | 1724 | `		 * DECLARING class (sFunc.pUserData), so child code and outsiders still cannot` |
|        - | 1725 | ``		 * reach it; a private ctor copied down blocks `new Child` from outside like`` |
|        - | 1726 | `		 * php's; and the surfaces that must NOT show an inherited private say so` |
|        - | 1727 | `		 * themselves (method_exists, get_class_methods, ReflectionClass::getMethods).` |
|        - | 1728 | `		 *` |
|        - | 1729 | `		 * STATIC privates used to be skipped here, on the reasoning that base methods` |
|        - | 1730 | `		 * reach them through self:: against the declaring class anyway. They do — but` |
|        - | 1731 | ``		 * nothing else could: every spelling of `B::p()` (the call, `['B','p']()`,`` |
|        - | 1732 | `		 * call_user_func, a first-class callable) reported the name as UNDEFINED, and` |
|        - | 1733 | ``		 * `static::p()` from the base with a subclass as the late-static-binding`` |
|        - | 1734 | `		 * target could not find its own method. */` |
| 12783933 | 1735 | `		rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 12783933 | 1736 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1737 | `			return rc;` |
|        - | 1738 | `		}` |
|        5 | 1739 | `	}` |
|        - | 1740 | `	/* All done */` |
|   769796 | 1741 | `	return SXRET_OK;` |
|   384367 | 1742 | `}` |
|        - | 1743 | `/*` |
|        - | 1744 | ` * Do these two compiled property defaults say the same thing? A raw memcmp of the` |
|        - | 1745 | ` * two instruction buffers is not that question: an instruction carries the LINE it` |
|        - | 1746 | ` * was compiled from and a literal travels as an INDEX into the VM's constant table,` |
|        - | 1747 | `` * so `public $p = 1` written in a trait and the same `public $p = 1` written in the`` |
|        - | 1748 | ` * composing class compare as different bytes and made php's incompatible-property` |
|        - | 1749 | ` * fatal fire on a class php composes without a word. Compare what the instructions` |
|        - | 1750 | ` * MEAN instead: the opcode, its operands, and for a constant load the VALUE behind` |
|        - | 1751 | ` * the index.` |
|        - | 1752 | ` */` |
|       32 | 1753 | `static int VmTraitLiteralSame(ph7_vm *pVm,sxu32 nLeft,sxu32 nRight)` |
|        5 | 1754 | `{` |
|        - | 1755 | `	ph7_value *pLeft,*pRight;` |
|       37 | 1756 | `	if( nLeft == nRight ){` |
|       15 | 1757 | `		return 1;` |
|        - | 1758 | `	}` |
|       23 | 1759 | `	pLeft  = (ph7_value *)SySetAt(&pVm->aLitObj,nLeft);` |
|       23 | 1760 | `	pRight = (ph7_value *)SySetAt(&pVm->aLitObj,nRight);` |
|       23 | 1761 | `	if( pLeft == 0 \|\| pRight == 0 ){` |
|      ! 0 | 1762 | `		return 0;` |
|        - | 1763 | `	}` |
|       23 | 1764 | `	if( (pLeft->iFlags & ~MEMOBJ_AUX) != (pRight->iFlags & ~MEMOBJ_AUX) ){` |
|      ! 0 | 1765 | `		return 0;` |
|        - | 1766 | `	}` |
|       18 | 1767 | `	if( SyBlobLength(&pLeft->sBlob) != SyBlobLength(&pRight->sBlob)` |
|       23 | 1768 | `	 \|\| (SyBlobLength(&pLeft->sBlob) > 0` |
|       11 | 1769 | `	     && SyMemcmp(SyBlobData(&pLeft->sBlob),SyBlobData(&pRight->sBlob),` |
|        4 | 1770 | `	                 SyBlobLength(&pLeft->sBlob)) != 0) ){` |
|        3 | 1771 | `		return 0;` |
|        - | 1772 | `	}` |
|       21 | 1773 | `	if( (pLeft->iFlags & MEMOBJ_INT) && pLeft->x.iVal != pRight->x.iVal ){` |
|        9 | 1774 | `		return 0;` |
|        - | 1775 | `	}` |
|       12 | 1776 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && pLeft->rVal != pRight->rVal ){` |
|      ! 0 | 1777 | `		return 0;` |
|        - | 1778 | `	}` |
|       12 | 1779 | `	return 1;` |
|       21 | 1780 | `}` |
|       28 | 1781 | `static int VmTraitDefaultsMatch(ph7_vm *pVm,SySet *pLeft,SySet *pRight)` |
|        5 | 1782 | `{` |
|        - | 1783 | `	VmInstr *aLeft,*aRight;` |
|        - | 1784 | `	sxu32 n,nUsed;` |
|       33 | 1785 | `	nUsed = SySetUsed(pLeft);` |
|       33 | 1786 | `	if( nUsed != SySetUsed(pRight) ){` |
|      ! 0 | 1787 | `		return 0;` |
|        - | 1788 | `	}` |
|       33 | 1789 | `	if( nUsed < 1 ){` |
|        3 | 1790 | `		return 1;` |
|        - | 1791 | `	}` |
|       31 | 1792 | `	aLeft  = (VmInstr *)SySetBasePtr(pLeft);` |
|       31 | 1793 | `	aRight = (VmInstr *)SySetBasePtr(pRight);` |
|       75 | 1794 | `	for( n = 0 ; n < nUsed ; ++n ){` |
|       57 | 1795 | `		if( aLeft[n].iOp != aRight[n].iOp \|\| aLeft[n].iP1 != aRight[n].iP1 ){` |
|      ! 0 | 1796 | `			return 0;` |
|        - | 1797 | `		}` |
|       57 | 1798 | `		if( aLeft[n].iOp == PH7_OP_LOADC ){` |
|       37 | 1799 | `			if( !VmTraitLiteralSame(pVm,aLeft[n].iP2,aRight[n].iP2) ){` |
|       12 | 1800 | `				return 0;` |
|        - | 1801 | `			}` |
|       26 | 1802 | `			continue;` |
|        - | 1803 | `		}` |
|       22 | 1804 | `		if( aLeft[n].iP2 != aRight[n].iP2 \|\| aLeft[n].p3 != aRight[n].p3 ){` |
|      ! 0 | 1805 | `			return 0;` |
|        - | 1806 | `		}` |
|       12 | 1807 | `	}` |
|       20 | 1808 | `	return 1;` |
|       19 | 1809 | `}` |
|        - | 1810 | `/*` |
|        - | 1811 | ` * Two constant declarations php considers the SAME declaration. Composing a trait over a` |
|        - | 1812 | ` * name that is already taken is only a conflict when the definition differs, and php's` |
|        - | 1813 | ``  * notion of "differs" covers the whole declaration, not just the value: `final const K='x'` `` |
|        - | 1814 | ``  * against `const K='x'` conflicts, and so does `public const K` against `private const K` `` |
|        - | 1815 | `` * and `const int K=1` against `const K=1`.`` |
|        - | 1816 | ` */` |
|        8 | 1817 | `static int VmTraitConstDefsMatch(ph7_vm *pVm,ph7_class_attr *pLeft,ph7_class_attr *pRight)` |
|        3 | 1818 | `{` |
|       11 | 1819 | `	sxi32 iMask = PH7_CLASS_ATTR_FINAL\|PH7_CLASS_ATTR_TYPED\|PH7_CLASS_ATTR_ABSTRACT;` |
|       11 | 1820 | `	if( pLeft->iProtection != pRight->iProtection ){` |
|      ! 0 | 1821 | `		return 0;` |
|        - | 1822 | `	}` |
|       11 | 1823 | `	if( (pLeft->iFlags & iMask) != (pRight->iFlags & iMask) ){` |
|      ! 0 | 1824 | `		return 0;` |
|        - | 1825 | `	}` |
|       11 | 1826 | `	if( SyStringCmp(&pLeft->sTypeName,&pRight->sTypeName,SyMemcmp) != 0 ){` |
|      ! 0 | 1827 | `		return 0;` |
|        - | 1828 | `	}` |
|       11 | 1829 | `	return VmTraitDefaultsMatch(pVm,&pLeft->aByteCode,&pRight->aByteCode);` |
|        7 | 1830 | `}` |
|        - | 1831 | `/*` |
|        - | 1832 | ` * A private copy of a trait member's record for one composing class. php composes a trait` |
|        - | 1833 | ` * into each using class SEPARATELY, so a trait's STATIC property is one slot per class --` |
|        - | 1834 | `` * `trait T { public static $c = 0; } class A { use T; } class B { use T; }` gives A and B a`` |
|        - | 1835 | `` * counter each -- and a trait CONSTANT is evaluated per class, so `const K = self::J` reads`` |
|        - | 1836 | ` * the J of whichever class composed it. Copying the record by POINTER gave every using class` |
|        - | 1837 | ` * the same storage slot and the same memoized value.` |
|        - | 1838 | ` *` |
|        - | 1839 | ` * The copy shares its source's compiled byte-code and attribute sets, which are read-only` |
|        - | 1840 | ` * once compilation is past; what it does NOT share is nIdx, the storage slot, and the` |
|        - | 1841 | ` * per-evaluation flags. pDeclClass stays the TRAIT, so every scope and naming rule still` |
|        - | 1842 | ` * finds the composing class through it.` |
|        - | 1843 | ` */` |
|       50 | 1844 | `static ph7_class_attr * VmCloneTraitAttr(ph7_vm *pVm,ph7_class_attr *pSrc)` |
|        4 | 1845 | `{` |
|       54 | 1846 | `	ph7_class_attr *pNew = (ph7_class_attr *)SyMemBackendPoolAlloc(&pVm->sAllocator,` |
|        - | 1847 | `		sizeof(ph7_class_attr));` |
|       54 | 1848 | `	if( pNew == 0 ){` |
|      ! 0 | 1849 | `		return 0;` |
|        - | 1850 | `	}` |
|       54 | 1851 | `	SyMemcpy((const void *)pSrc,(void *)pNew,sizeof(ph7_class_attr));` |
|       54 | 1852 | `	pNew->nIdx = SXU32_HIGH; /* its own storage slot, reserved at this class's mount */` |
|       54 | 1853 | `	SyZero(&pNew->sStoreName,sizeof(SyString)); /* ...and its own mangled name, which` |
|        - | 1854 | `	                          * names the class that COMPOSED it and not the source's */` |
|       54 | 1855 | `	pNew->iFlags &= ~(PH7_CLASS_ATTR_EVALING\|PH7_CLASS_ATTR_STATIC_DEFER);` |
|       54 | 1856 | `	return pNew;` |
|       29 | 1857 | `}` |
|        - | 1858 | `/*` |
|        - | 1859 | ` * Apply a trait to a class: copy all methods and attributes from the trait` |
|        - | 1860 | ` * into the target class. Unlike inheritance, traits copy ALL members including` |
|        - | 1861 | ` * private ones. Members already defined in the class take precedence.` |
|        - | 1862 | ` */` |
|      330 | 1863 | `PH7_PRIVATE sxi32 PH7_ClassUseTrait(ph7_gen_state *pGen,ph7_class *pClass,ph7_class *pTrait)` |
|        5 | 1864 | `{` |
|        - | 1865 | `	ph7_class_method *pMeth;` |
|        - | 1866 | `	ph7_class_attr *pAttr;` |
|        - | 1867 | `	SyHashEntry *pEntry;` |
|        - | 1868 | `	SyString *pName;` |
|        - | 1869 | `	sxi32 rc;` |
|        - | 1870 | `	/* Detect cyclic trait composition (e.g. trait A { use B; } trait B { use A; }) */` |
|      335 | 1871 | `	if( pTrait->iFlags & PH7_CLASS_TRAIT_VISITING ){` |
|      ! 0 | 1872 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|      ! 0 | 1873 | `			"Trait circular reference detected: %z is already being applied",&pTrait->sDisp);` |
|      ! 0 | 1874 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1875 | `			return SXERR_ABORT;` |
|        - | 1876 | `		}` |
|      ! 0 | 1877 | `		return SXRET_OK;` |
|        - | 1878 | `	}` |
|      335 | 1879 | `	pTrait->iFlags \|= PH7_CLASS_TRAIT_VISITING;` |
|      335 | 1880 | `	rc = SXRET_OK;` |
|        - | 1881 | `	/* Copy attributes from the trait */` |
|      335 | 1882 | `	SyHashResetLoopCursor(&pTrait->hAttr);` |
|      455 | 1883 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hAttr)) != 0 ){` |
|        - | 1884 | `		SyHashEntry *pExisting;` |
|      125 | 1885 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      125 | 1886 | `		pName = &pAttr->sName;` |
|      125 | 1887 | `		pExisting = SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte);` |
|      125 | 1888 | `		if( pExisting != 0 ){` |
|        - | 1889 | `			/* The name is taken. What decides is the definition ALREADY standing --` |
|        - | 1890 | `			 * the class's own body just as much as an earlier trait's -- and whether` |
|        - | 1891 | `			 * its default is the same one. Looking the name up in the traits applied` |
|        - | 1892 | `			 * so far and comparing only THEN let a class-body property through:` |
|        - | 1893 | ``			 * `class M { use TA, TB; public $p = 3; }` said nothing when TA arrived`` |
|        - | 1894 | `			 * (no trait held the name yet) and then blamed the wrong pair when TB did. */` |
|       24 | 1895 | `			ph7_class_attr *pClassAttr = (ph7_class_attr *)pExisting->pUserData;` |
|       24 | 1896 | `			if( !VmTraitDefaultsMatch(pGen->pVm,&pAttr->aByteCode,&pClassAttr->aByteCode) ){` |
|        - | 1897 | `				/* php names the FIRST definition rather than the standing one: when the` |
|        - | 1898 | `				 * holder is the composing class itself, it walks the traits applied so` |
|        - | 1899 | `				 * far and names the first that declares the property, so the same class` |
|        - | 1900 | `				 * body reads "M and TA" with one trait behind it and "TA and TB" with` |
|        - | 1901 | `				 * two. The sentence ends with a clause of its own and lets the fatal's` |
|        - | 1902 | `				 * " in %s on line %u" finish it -- the line is the composing class's. */` |
|        6 | 1903 | `				ph7_class *pHolder = pClassAttr->pDeclClass;` |
|        6 | 1904 | `				if( pHolder == 0 \|\| pHolder == pClass ){` |
|      ! 0 | 1905 | `					ph7_class **apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|      ! 0 | 1906 | `					sxu32 nUsed = SySetUsed(&pClass->aTrait);` |
|        - | 1907 | `					sxu32 k;` |
|      ! 0 | 1908 | `					pHolder = pClass;` |
|      ! 0 | 1909 | `					for(k = 0; k < nUsed; k++){` |
|      ! 0 | 1910 | `						if( PH7_ClassExtractAttribute(apUsedTraits[k],pName->zString,pName->nByte) ){` |
|      ! 0 | 1911 | `							pHolder = apUsedTraits[k];` |
|      ! 0 | 1912 | `							break;` |
|        - | 1913 | `						}` |
|      ! 0 | 1914 | `					}` |
|      ! 0 | 1915 | `				}` |
|        8 | 1916 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,` |
|        - | 1917 | `					"%z and %z define the same property ($%z) in the composition of %z. "` |
|        - | 1918 | `					"However, the definition differs and is considered incompatible. "` |
|        - | 1919 | `					"Class was composed",` |
|        4 | 1920 | `					&pHolder->sDisp,&pTrait->sDisp,pName,&pClass->sDisp);` |
|        6 | 1921 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1922 | `					goto cleanup;` |
|        - | 1923 | `				}` |
|        2 | 1924 | `			}` |
|       24 | 1925 | `			continue;` |
|        - | 1926 | `		}` |
|      105 | 1927 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - | 1928 | `			/* One slot per composing class (see VmCloneTraitAttr). */` |
|       26 | 1929 | `			ph7_class_attr *pOwnCopy = VmCloneTraitAttr(pGen->pVm,pAttr);` |
|       26 | 1930 | `			if( pOwnCopy == 0 ){` |
|      ! 0 | 1931 | `				rc = SXERR_MEM;` |
|      ! 0 | 1932 | `				goto cleanup;` |
|        - | 1933 | `			}` |
|       26 | 1934 | `			pAttr = pOwnCopy;` |
|       12 | 1935 | `		}` |
|      105 | 1936 | `		rc = SyHashInsertTail(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|      105 | 1937 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1938 | `			goto cleanup;` |
|        - | 1939 | `		}` |
|        - | 1940 | `		/* A trait's private is the COMPOSING class's own (php composes it in), so` |
|        - | 1941 | `		 * the mask has to name it here too. */` |
|      105 | 1942 | `		PH7_ClassNotePrivateName(pClass,pAttr);` |
|        5 | 1943 | `	}` |
|        - | 1944 | `	/* Copy constants from the trait (PHP 8.2 trait constants; separate hConst` |
|        - | 1945 | `	 * namespace). The name being taken is only a conflict when the DEFINITION differs,` |
|        - | 1946 | `	 * exactly as for a property above -- php compares the value, the visibility, the` |
|        - | 1947 | ``	 * `final` flag and the declared type, and lets two identical declarations through`` |
|        - | 1948 | ``	 * (`trait A { const K='x'; } trait B { const K='x'; }` composes fine). A definition`` |
|        - | 1949 | `	 * inherited from a BASE class is not part of the comparison: a trait constant` |
|        - | 1950 | `	 * overrides one, silently, the way a class-body constant does. */` |
|      335 | 1951 | `	SyHashResetLoopCursor(&pTrait->hConst);` |
|      369 | 1952 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hConst)) != 0 ){` |
|        - | 1953 | `		SyHashEntry *pExisting;` |
|       37 | 1954 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|       37 | 1955 | `		pName = &pAttr->sName;` |
|       37 | 1956 | `		pExisting = SyHashGet(&pClass->hConst,(const void *)pName->zString,pName->nByte);` |
|       37 | 1957 | `		if( pExisting != 0 ){` |
|       11 | 1958 | `			ph7_class_attr *pHave = (ph7_class_attr *)pExisting->pUserData;` |
|       11 | 1959 | `			ph7_class *pHolder = pHave->pDeclClass;` |
|        8 | 1960 | `			if( pHolder && pHolder != pClass` |
|        6 | 1961 | `			 && (pHolder->iFlags & PH7_CLASS_TRAIT) == 0` |
|        5 | 1962 | `			 && PH7_VmInstanceOf(pClass,pHolder) ){` |
|        - | 1963 | `				/* Inherited from a base class: the trait's definition replaces it. */` |
|      ! 0 | 1964 | `				ph7_class_attr *pOwnCopy = VmCloneTraitAttr(pGen->pVm,pAttr);` |
|      ! 0 | 1965 | `				if( pOwnCopy == 0 ){` |
|      ! 0 | 1966 | `					rc = SXERR_MEM;` |
|      ! 0 | 1967 | `					goto cleanup;` |
|        - | 1968 | `				}` |
|      ! 0 | 1969 | `				SyHashDeleteEntry2(pExisting);` |
|      ! 0 | 1970 | `				rc = SyHashInsertTail(&pClass->hConst,(const void *)pName->zString,pName->nByte,pOwnCopy);` |
|      ! 0 | 1971 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 1972 | `					goto cleanup;` |
|        - | 1973 | `				}` |
|      ! 0 | 1974 | `				continue;` |
|        - | 1975 | `			}` |
|       11 | 1976 | `			if( !VmTraitConstDefsMatch(pGen->pVm,pAttr,pHave) ){` |
|        - | 1977 | `				/* php names the FIRST definition, as the property path does. */` |
|        6 | 1978 | `				if( pHolder == 0 \|\| pHolder == pClass ){` |
|        3 | 1979 | `					ph7_class **apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|        3 | 1980 | `					sxu32 nUsed = SySetUsed(&pClass->aTrait);` |
|        - | 1981 | `					sxu32 k;` |
|        3 | 1982 | `					pHolder = pClass;` |
|        3 | 1983 | `					for(k = 0; k < nUsed; k++){` |
|      ! 0 | 1984 | `						if( PH7_ClassExtractConstant(apUsedTraits[k],pName->zString,pName->nByte) ){` |
|      ! 0 | 1985 | `							pHolder = apUsedTraits[k];` |
|      ! 0 | 1986 | `							break;` |
|        - | 1987 | `						}` |
|      ! 0 | 1988 | `					}` |
|        1 | 1989 | `				}` |
|        8 | 1990 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,` |
|        - | 1991 | `					"%z and %z define the same constant (%z) in the composition of %z. "` |
|        - | 1992 | `					"However, the definition differs and is considered incompatible. "` |
|        - | 1993 | `					"Class was composed",` |
|        4 | 1994 | `					&pHolder->sDisp,&pTrait->sDisp,pName,&pClass->sDisp);` |
|        6 | 1995 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1996 | `					goto cleanup;` |
|        - | 1997 | `				}` |
|        2 | 1998 | `			}` |
|       11 | 1999 | `			continue;` |
|        - | 2000 | `		}` |
|        - | 2001 | `		{` |
|        - | 2002 | ``			/* Evaluated per composing class (`const K = self::J`), so one record each. */`` |
|       29 | 2003 | `			ph7_class_attr *pOwnCopy = VmCloneTraitAttr(pGen->pVm,pAttr);` |
|       29 | 2004 | `			if( pOwnCopy == 0 ){` |
|      ! 0 | 2005 | `				rc = SXERR_MEM;` |
|      ! 0 | 2006 | `				goto cleanup;` |
|        - | 2007 | `			}` |
|       29 | 2008 | `			rc = SyHashInsertTail(&pClass->hConst,(const void *)pName->zString,pName->nByte,pOwnCopy);` |
|        - | 2009 | `		}` |
|       29 | 2010 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 2011 | `			goto cleanup;` |
|        - | 2012 | `		}` |
|        3 | 2013 | `	}` |
|        - | 2014 | `	/* Copy methods from the trait. The identity copied is the trait's HASH KEY,` |
|        - | 2015 | ``	 * not sFunc.sName: an adaptation alias (`hi as bHi`) is a hash entry whose`` |
|        - | 2016 | `	 * key is the alias while the method struct keeps its original name — keying` |
|        - | 2017 | ``	 * the copy off sFunc.sName silently re-filed `bHi` under `hi`, so an alias`` |
|        - | 2018 | `	 * made inside a TRAIT vanished when that trait was composed into a class. */` |
|      335 | 2019 | `	SyHashResetLoopCursor(&pTrait->hMethod);` |
|      791 | 2020 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hMethod)) != 0 ){` |
|        - | 2021 | `		SyHashEntry *pClassMethEntry;` |
|        - | 2022 | `		SyString sKey;` |
|      461 | 2023 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      461 | 2024 | `		SyStringInitFromBuf(&sKey,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|      461 | 2025 | `		pName = &sKey;` |
|      461 | 2026 | `		pClassMethEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
|      461 | 2027 | `		if( pClassMethEntry != 0 ){` |
|        - | 2028 | `			/* Method already exists in the class. An ABSTRACT trait method is a` |
|        - | 2029 | `			 * REQUIREMENT, not an implementation: php satisfies it with any concrete` |
|        - | 2030 | `			 * method of the same name (from the class body or another trait) — no` |
|        - | 2031 | `			 * collision. Only two CONCRETE trait methods actually conflict. */` |
|       48 | 2032 | `			ph7_class_method *pExistingMeth = (ph7_class_method *)pClassMethEntry->pUserData;` |
|       48 | 2033 | `			int bIncomingAbstract = (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0;` |
|       48 | 2034 | `			int bExistingAbstract = (pExistingMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0;` |
|        - | 2035 | `			ph7_class **apUsedTraits;` |
|        - | 2036 | `			sxu32 nUsed,k;` |
|       48 | 2037 | `			if( bIncomingAbstract ){` |
|        - | 2038 | `				/* Incoming abstract requirement: the existing (concrete or abstract)` |
|        - | 2039 | `				 * method already covers this name — keep it. */` |
|       31 | 2040 | `				continue;` |
|        - | 2041 | `			}` |
|       35 | 2042 | `			if( bExistingAbstract ){` |
|        - | 2043 | `				/* Existing entry is only an abstract requirement (from an earlier` |
|        - | 2044 | `				 * trait): the incoming concrete method satisfies and replaces it. */` |
|        5 | 2045 | `				pClassMethEntry->pUserData = (void *)pMeth;` |
|        5 | 2046 | `				continue;` |
|        - | 2047 | `			}` |
|        - | 2048 | ``			/* Two names are not two METHODS. A trait that `use`s another trait`` |
|        - | 2049 | `			 * flattens it by sharing the very ph7_class_method the origin trait` |
|        - | 2050 | `			 * compiled, so a method reaching the class down two composition paths` |
|        - | 2051 | `			 * arrives as the SAME struct both times -- which is php's own test` |
|        - | 2052 | `			 * (zend compares the two functions' op_array.opcodes) and why` |
|        - | 2053 | ``			 * `trait TB { use TA; } class M { use TB, TA; }` composes there and`` |
|        - | 2054 | `			 * fatalled here. Only two genuinely different definitions collide. */` |
|       31 | 2055 | `			if( pExistingMeth == pMeth ){` |
|       15 | 2056 | `				continue;` |
|        - | 2057 | `			}` |
|        - | 2058 | `			/* A method the class declares ITSELF wins over every trait, however many` |
|        - | 2059 | `			 * of them offer the name: php reports no collision at all for` |
|        - | 2060 | ``			 * `class M { use TA, TB; public function m(){} }`, where PHL raised one`` |
|        - | 2061 | `			 * as soon as the second trait arrived. */` |
|       17 | 2062 | `			if( (ph7_class *)pExistingMeth->sFunc.pUserData == pClass ){` |
|       13 | 2063 | `				continue;` |
|        - | 2064 | `			}` |
|        - | 2065 | `			/* Both concrete: a genuine collision only when the OTHER definition came` |
|        - | 2066 | `			 * from another trait (a concrete one). A class-body method wins silently. */` |
|        5 | 2067 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|        5 | 2068 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|        5 | 2069 | `			for(k = 0; k < nUsed; k++){` |
|        5 | 2070 | `				ph7_class_method *pOtherMeth = PH7_ClassExtractMethod(apUsedTraits[k],pName->zString,pName->nByte);` |
|        4 | 2071 | `				if( pOtherMeth != 0 && pOtherMeth != pMeth` |
|        5 | 2072 | `				 && (pOtherMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|        - | 2073 | `					/* Two different traits define the same CONCRETE method with no` |
|        - | 2074 | `					 * resolution. php reports the line of the COMPOSING class, not` |
|        - | 2075 | `					 * the one the losing definition was written on. */` |
|        7 | 2076 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,` |
|        - | 2077 | `						"Trait method %z::%z has not been applied as %z::%z, "` |
|        - | 2078 | `						"because of collision with %z::%z",` |
|        4 | 2079 | `						&pTrait->sDisp,pName,` |
|        2 | 2080 | `						&pClass->sDisp,pName,` |
|        4 | 2081 | `						&apUsedTraits[k]->sDisp,pName);` |
|        5 | 2082 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 2083 | `						goto cleanup;` |
|        - | 2084 | `					}` |
|        5 | 2085 | `					break;` |
|        - | 2086 | `				}` |
|      ! 0 | 2087 | `			}` |
|        - | 2088 | `			/* Class-defined method takes precedence */` |
|        5 | 2089 | `			continue;` |
|        - | 2090 | `		}` |
|      417 | 2091 | `		rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|      417 | 2092 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 2093 | `			goto cleanup;` |
|        - | 2094 | `		}` |
|        5 | 2095 | `	}` |
|        - | 2096 | `	/* Record trait in the class */` |
|      335 | 2097 | `	SySetPut(&pClass->aTrait,(const void *)&pTrait);` |
|      165 | 2098 | `cleanup:` |
|        - | 2099 | `	/* Always clear visiting flag, even on error paths */` |
|      335 | 2100 | `	pTrait->iFlags &= ~PH7_CLASS_TRAIT_VISITING;` |
|      165 | 2101 | `	SXUNUSED(pGen);` |
|      335 | 2102 | `	return rc;` |
|      170 | 2103 | `}` |
|        - | 2104 | `/*` |
|        - | 2105 | ` * Inherit an object interface from another object interface.` |
|        - | 2106 | ` * According to the PHP language reference manual.` |
|        - | 2107 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|        - | 2108 | ` *  must implement, without having to define how these methods are handled.` |
|        - | 2109 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|        - | 2110 | ` *  class, but without any of the methods having their contents defined.` |
|        - | 2111 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|        - | 2112 | ` *` |
|        - | 2113 | ` * This function return SXRET_OK if the interface inheritance operation was successfully performed.` |
|        - | 2114 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|        - | 2115 | ` * error message.` |
|        - | 2116 | ` */` |
|    71377 | 2117 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceInherit(ph7_class *pSub,ph7_class *pBase)` |
|        5 | 2118 | `{` |
|        - | 2119 | `	ph7_class_method *pMeth;` |
|        - | 2120 | `	ph7_class_attr *pAttr;` |
|        - | 2121 | `	SyHashEntry *pEntry;` |
|        - | 2122 | `	SyString *pName;` |
|        - | 2123 | `	sxi32 rc;` |
|        - | 2124 | `	/* Install in the derived hashtable */` |
|    71382 | 2125 | `	SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|    71382 | 2126 | `	SyHashResetLoopCursor(&pBase->hConst);` |
|        - | 2127 | `	/* Copy constants (interfaces carry only constants + method signatures) */` |
|   107126 | 2128 | `	while((pEntry = SyHashGetNextEntry(&pBase->hConst)) != 0 ){` |
|        - | 2129 | `		/* Make sure the constants are not redeclared in the subclass */` |
|        7 | 2130 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|        7 | 2131 | `		pName = &pAttr->sName;` |
|        7 | 2132 | `		if( SyHashGet(&pSub->hConst,(const void *)pName->zString,pName->nByte) == 0 ){` |
|        - | 2133 | `			/* Install the constant in the subclass */` |
|        3 | 2134 | `			rc = SyHashInsertTail(&pSub->hConst,(const void *)pName->zString,pName->nByte,pAttr);` |
|        3 | 2135 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 2136 | `				return rc;` |
|        - | 2137 | `			}` |
|        1 | 2138 | `		}` |
|        1 | 2139 | `	}` |
|    71382 | 2140 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
|        - | 2141 | `	/* Copy methods signature */` |
|   257771 | 2142 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|        - | 2143 | `		/* Make sure the method are not redeclared in the subclass */` |
|   150656 | 2144 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   150656 | 2145 | `		pName = &pMeth->sFunc.sName;` |
|   150656 | 2146 | `		if( SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte) == 0 ){` |
|        - | 2147 | `			/* Install the method */` |
|   150650 | 2148 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|   150650 | 2149 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 2150 | `				return rc;` |
|        - | 2151 | `			}` |
|    75218 | 2152 | `		}` |
|        5 | 2153 | `	}` |
|        - | 2154 | `	/* Mark as subclass */` |
|    71382 | 2155 | `	pSub->pBase = pBase;` |
|        - | 2156 | `	/* All done */` |
|    71382 | 2157 | `	return SXRET_OK;` |
|    35644 | 2158 | `}` |
|        - | 2159 | `/*` |
|        - | 2160 | ` * Implements an object interface in the given main class.` |
|        - | 2161 | ` * According to the PHP language reference manual.` |
|        - | 2162 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|        - | 2163 | ` *  must implement, without having to define how these methods are handled.` |
|        - | 2164 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|        - | 2165 | ` *  class, but without any of the methods having their contents defined.` |
|        - | 2166 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|        - | 2167 | ` *` |
|        - | 2168 | ` * This function return SXRET_OK if the interface was successfully implemented.` |
|        - | 2169 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|        - | 2170 | ` * error message.` |
|        - | 2171 | ` */` |
|   730078 | 2172 | `PH7_PRIVATE sxi32 PH7_ClassImplement(ph7_class *pMain,ph7_class *pInterface)` |
|        5 | 2173 | `{` |
|        - | 2174 | `	ph7_class_attr *pAttr;` |
|        - | 2175 | `	SyHashEntry *pEntry;` |
|        - | 2176 | `	SyString *pName;` |
|        - | 2177 | `	sxi32 rc;` |
|        - | 2178 | `	/* First off,copy all constants declared inside the interface (hConst namespace) */` |
|   730083 | 2179 | `	SyHashResetLoopCursor(&pInterface->hConst);` |
|  1317556 | 2180 | `	while((pEntry = SyHashGetNextEntry(&pInterface->hConst)) != 0 ){` |
|        - | 2181 | `		/* Point to the constant declaration */` |
|   221933 | 2182 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   221933 | 2183 | `		pName = &pAttr->sName;` |
|        - | 2184 | `		/* Make sure the constant is not redeclared in the main class */` |
|   221933 | 2185 | `		if( SyHashGet(&pMain->hConst,pName->zString,pName->nByte) == 0 ){` |
|        - | 2186 | `			/* Install the constant */` |
|   221931 | 2187 | `			rc = SyHashInsertTail(&pMain->hConst,pName->zString,pName->nByte,pAttr);` |
|   221931 | 2188 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 2189 | `				return rc;` |
|        - | 2190 | `			}` |
|   110809 | 2191 | `		}` |
|        5 | 2192 | `	}` |
|        - | 2193 | `	/* Install in the interface container */` |
|   730083 | 2194 | `	SySetPut(&pMain->aInterface,(const void *)&pInterface);` |
|        - | 2195 | `	/* Install interface method stubs into the implementing class.` |
|        - | 2196 | `	 * Methods already defined in the class take precedence (they satisfy` |
|        - | 2197 | `	 * the interface contract). Stubs retain PH7_CLASS_ATTR_ABSTRACT so` |
|        - | 2198 | `	 * the unified check in GenStateCheckAbstractMethods catches missing ones.` |
|        - | 2199 | `	 */` |
|        - | 2200 | `	{` |
|        - | 2201 | `		ph7_class_method *pMeth;` |
|        - | 2202 | `		SyHashEntry *pMEntry;` |
|        - | 2203 | `		SyString *pMName;` |
|   730083 | 2204 | `		SyHashResetLoopCursor(&pInterface->hMethod);` |
|  3300252 | 2205 | `		while((pMEntry = SyHashGetNextEntry(&pInterface->hMethod)) != 0 ){` |
|  2204629 | 2206 | `			pMeth = (ph7_class_method *)pMEntry->pUserData;` |
|  2204629 | 2207 | `			pMName = &pMeth->sFunc.sName;` |
|  2204629 | 2208 | `			if( SyHashGet(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte) == 0 ){` |
|     7972 | 2209 | `				rc = SyHashInsert(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte,pMeth);` |
|     7972 | 2210 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 2211 | `					return rc;` |
|        - | 2212 | `				}` |
|     3978 | 2213 | `			}` |
|        5 | 2214 | `		}` |
|        - | 2215 | `	}` |
|   730083 | 2216 | `	return SXRET_OK;` |
|   364538 | 2217 | `}` |
|        - | 2218 | `/*` |
|        - | 2219 | ` * Create a class instance [i.e: Object in the PHP jargon] at run-time.` |
|        - | 2220 | ` * The following function is called when an object is created at run-time` |
|        - | 2221 | ` * typically when the PH7_OP_NEW/PH7_OP_CLONE instructions are executed.` |
|        - | 2222 | ` * Notes on object creation.` |
|        - | 2223 | ` *` |
|        - | 2224 | ` * According to PHP language reference manual.` |
|        - | 2225 | ` * To create an instance of a class, the new keyword must be used. An object will always` |
|        - | 2226 | ` * be created unless the object has a constructor defined that throws an exception on error.` |
|        - | 2227 | ` * Classes should be defined before instantiation (and in some cases this is a requirement).` |
|        - | 2228 | ` * If a string containing the name of a class is used with new, a new instance of that class` |
|        - | 2229 | ` * will be created. If the class is in a namespace, its fully qualified name must be used when` |
|        - | 2230 | ` * doing this.` |
|        - | 2231 | ` * Example #3 Creating an instance` |
|        - | 2232 | ` * <?php` |
|        - | 2233 | ` *  $instance = new SimpleClass();` |
|        - | 2234 | ` *   // This can also be done with a variable:` |
|        - | 2235 | ` * $className = 'Foo';` |
|        - | 2236 | ` * $instance = new $className(); // Foo()` |
|        - | 2237 | ` * ?>` |
|        - | 2238 | ` * In the class context, it is possible to create a new object by new self and new parent.` |
|        - | 2239 | ` * When assigning an already created instance of a class to a new variable, the new variable` |
|        - | 2240 | ` * will access the same instance as the object that was assigned. This behaviour is the same` |
|        - | 2241 | ` * when passing instances to a function. A copy of an already created object can be made by` |
|        - | 2242 | ` * cloning it.` |
|        - | 2243 | ` * Example #4 Object Assignment` |
|        - | 2244 | ` * <?php` |
|        - | 2245 | ` *  class SimpleClass(){` |
|        - | 2246 | ` *    public $var;` |
|        - | 2247 | ` *  };` |
|        - | 2248 | ` *  $instance = new SimpleClass();` |
|        - | 2249 | ` *  $assigned   =  $instance;` |
|        - | 2250 | ` *  $reference  =& $instance;` |
|        - | 2251 | ` *  $instance->var = '$assigned will have this value';` |
|        - | 2252 | ` *  $instance = null; // $instance and $reference become null` |
|        - | 2253 | ` *  var_dump($instance);` |
|        - | 2254 | ` *  var_dump($reference);` |
|        - | 2255 | ` *  var_dump($assigned);` |
|        - | 2256 | ` * ?>` |
|        - | 2257 | ` * The above example will output:` |
|        - | 2258 | ` * NULL` |
|        - | 2259 | ` * NULL` |
|        - | 2260 | ` * object(SimpleClass)#1 (1) {` |
|        - | 2261 | ` *  ["var"]=>` |
|        - | 2262 | ` *    string(30) "$assigned will have this value"` |
|        - | 2263 | ` * }` |
|        - | 2264 | ` * Example #5 Creating new objects` |
|        - | 2265 | ` * <?php` |
|        - | 2266 | ` * class Test` |
|        - | 2267 | ` * {` |
|        - | 2268 | ` *   static public function getNew()` |
|        - | 2269 | ` *   {` |
|        - | 2270 | ` *       return new static;` |
|        - | 2271 | ` *   }` |
|        - | 2272 | ` * }` |
|        - | 2273 | ` * class Child extends Test` |
|        - | 2274 | ` * {}` |
|        - | 2275 | ` * $obj1 = new Test();` |
|        - | 2276 | ` * $obj2 = new $obj1;` |
|        - | 2277 | ` * var_dump($obj1 !== $obj2);` |
|        - | 2278 | ` * $obj3 = Test::getNew();` |
|        - | 2279 | ` * var_dump($obj3 instanceof Test);` |
|        - | 2280 | ` * $obj4 = Child::getNew();` |
|        - | 2281 | ` * var_dump($obj4 instanceof Child);` |
|        - | 2282 | ` * ?>` |
|        - | 2283 | ` * The above example will output:` |
|        - | 2284 | ` * bool(true)` |
|        - | 2285 | ` * bool(true)` |
|        - | 2286 | ` * bool(true)` |
|        - | 2287 | ` * Note that Symisc Systems have introduced powerfull extension to` |
|        - | 2288 | ` * OO subsystem. For example a class attribute may have any complex` |
|        - | 2289 | ` * expression associated with it when declaring the attribute unlike` |
|        - | 2290 | ` * the standard PHP engine which would allow a single value.` |
|        - | 2291 | ` * Example:` |
|        - | 2292 | ` *  class myClass{` |
|        - | 2293 | ` *    public $var = 25<<1+foo()/bar();` |
|        - | 2294 | ` *  };` |
|        - | 2295 | ` * Refer to the official documentation for more information.` |
|        - | 2296 | ` */` |
|  1628479 | 2297 | `static ph7_class_instance * NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|        5 | 2298 | `{` |
|        - | 2299 | `	ph7_class_instance *pThis;` |
|        - | 2300 | `	/* Allocate a new instance */` |
|  1628484 | 2301 | `	pThis = (ph7_class_instance *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_instance));` |
|  1628484 | 2302 | `	if( pThis == 0 ){` |
|      ! 0 | 2303 | `		return 0;` |
|        - | 2304 | `	}` |
|        - | 2305 | `	/* Zero the structure */` |
|  1628484 | 2306 | `	SyZero(pThis,sizeof(ph7_class_instance));` |
|        - | 2307 | `	/* Initialize fields */` |
|  1628484 | 2308 | `	pThis->iRef = 1;` |
|  1628484 | 2309 | `	pThis->pVm = pVm;` |
|  1628484 | 2310 | `	pThis->pClass = pClass;` |
|        - | 2311 | `	/* Assign a fresh monotonic object handle id (clones get their own, like PHP). */` |
|  1628484 | 2312 | `	pThis->nObjId = pVm->nNextObjId++;` |
|        - | 2313 | `	/* Size the property table to the class, not to the VM's default of sixteen` |
|        - | 2314 | `	 * buckets. An object holds the attributes its class declares -- the frame` |
|        - | 2315 | `	 * builder files one record per entry of pClass->hAttr and nothing else -- so` |
|        - | 2316 | `	 * the count is known here exactly, before a single property is installed.` |
|        - | 2317 | `	 * The census put the average object at 4.6 properties in a table built for` |
|        - | 2318 | `	 * forty-eight, which was 4.40 MB of mostly-zero bucket arrays.` |
|        - | 2319 | `	 *` |
|        - | 2320 | `	 * Asked for one bucket PER ENTRY, not for the fill factor's three. Dividing by` |
|        - | 2321 | `	 * SXHASH_FILL_FACTOR is what the table's own growth rule considers full, and it` |
|        - | 2322 | `	 * would have put the average object at 2.3 properties per bucket where the` |
|        - | 2323 | `	 * sixteen-bucket default had 0.29 -- trading memory this box can measure for` |
|        - | 2324 | `	 * property-lookup time it cannot. One bucket per entry keeps the walk at about` |
|        - | 2325 | `	 * one node, still costs a quarter of the default, and the two sizings differ by` |
|        - | 2326 | `	 * 0.52 MB of peak (136.16 MB against 136.68) -- which is the right 0.52 MB to` |
|        - | 2327 | `	 * leave on the table. SyHashInitSized rounds up to a power of two and floors it` |
|        - | 2328 | `	 * at 2, and a class that outgrows the estimate doubles exactly as before. */` |
|  1628484 | 2329 | `	SyHashInitSized(&pThis->hAttr,&pVm->sAllocator,0,0,pClass->hAttr.nEntry);` |
|  1628484 | 2330 | `	return pThis;` |
|   814077 | 2331 | `}` |
|        - | 2332 | `/*` |
|        - | 2333 | ` * Wrapper around the NewClassInstance() function defined above.` |
|        - | 2334 | ` * See the block comment above for more information.` |
|        - | 2335 | ` */` |
|  1627223 | 2336 | `PH7_PRIVATE ph7_class_instance * PH7_NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|        5 | 2337 | `{` |
|        - | 2338 | `	ph7_class_instance *pNew;` |
|        - | 2339 | `	sxi32 rc;` |
|  1627228 | 2340 | `	pNew = NewClassInstance(&(*pVm),&(*pClass));` |
|  1627228 | 2341 | `	if( pNew == 0 ){` |
|      ! 0 | 2342 | `		return 0;` |
|        - | 2343 | `	}` |
|        - | 2344 | `	/* Associate a private VM frame with this class instance */` |
|  1627228 | 2345 | `	rc = PH7_VmCreateClassInstanceFrame(&(*pVm),pNew);` |
|  1627228 | 2346 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 2347 | `		SyMemBackendPoolFree(&pVm->sAllocator,pNew);` |
|      ! 0 | 2348 | `		return 0;` |
|        - | 2349 | `	}` |
|        - | 2350 | `	/* php stamps a Throwable's file/line at CREATION, not in its constructor, so a` |
|        - | 2351 | `	 * subclass that overrides __construct without calling parent::__construct still` |
|        - | 2352 | `	 * reports the right site. Every instantiation path lands here. */` |
|  1627228 | 2353 | `	PH7_VmStampThrowableSite(&(*pVm),pNew);` |
|        - | 2354 | `	/* php's create_object handler, resolved through the ANCESTORS the way the` |
|        - | 2355 | `	 * teardown one is: a subclass of a native class whose slots are SEEDED at` |
|        - | 2356 | ``	 * `new` (ZipArchive's six) must start with the same six. */`` |
|        - | 2357 | `	{` |
|  1627228 | 2358 | `		ph7_class *pOwner = pClass;` |
|  3389633 | 2359 | `		while( pOwner && pOwner->xNew == 0 ){` |
|  1762410 | 2360 | `			pOwner = pOwner->pBase;` |
|        5 | 2361 | `		}` |
|  1627228 | 2362 | `		if( pOwner && pOwner->xNew ){` |
|      366 | 2363 | `			pOwner->xNew(&(*pVm),pNew);` |
|      181 | 2364 | `		}` |
|        - | 2365 | `	}` |
|  1627228 | 2366 | `	return pNew;` |
|   813449 | 2367 | `}` |
|        - | 2368 | `/*` |
|        - | 2369 | ` * Open a private walk of this object's property table.` |
|        - | 2370 | ` *` |
|        - | 2371 | ` * Every consumer that hands PHP code the control flow between two attributes --` |
|        - | 2372 | `` * `foreach ($o as $k => $v)`, `array_walk($o, $fn)` -- must own its position`` |
|        - | 2373 | ` * rather than share the SyHash's embedded cursor: php iterates each walk` |
|        - | 2374 | ` * independently (nested loops over one object do not rewind each other), and the` |
|        - | 2375 | ` * body it runs in between can add or remove a property. The instance keeps the` |
|        - | 2376 | ` * list of open walks so those two mutations can fix the cursors up; the walker` |
|        - | 2377 | ` * MUST close it on every exit path, or the next mutation walks a recycled slot.` |
|        - | 2378 | ` */` |
|      146 | 2379 | `PH7_PRIVATE void PH7_ClassInstanceIterOpen(ph7_class_instance *pThis,PH7_AttrIter *pIter)` |
|        5 | 2380 | `{` |
|      151 | 2381 | `	pIter->pCursor = SyHashFirstEntry(&pThis->hAttr);` |
|      151 | 2382 | `	pIter->pNextIter = pThis->pActiveIters;` |
|      151 | 2383 | `	pThis->pActiveIters = pIter;` |
|      151 | 2384 | `}` |
|        - | 2385 | `/*` |
|        - | 2386 | ` * The next attribute entry, or 0 when the walk is exhausted. The cursor is` |
|        - | 2387 | ` * advanced BEFORE the entry is handed out, exactly like SyHashGetNextEntry:` |
|        - | 2388 | ` * php's own iteration standing on an entry is free to unset() it.` |
|        - | 2389 | ` */` |
|      670 | 2390 | `PH7_PRIVATE SyHashEntry * PH7_ClassInstanceIterNext(PH7_AttrIter *pIter)` |
|        5 | 2391 | `{` |
|      675 | 2392 | `	SyHashEntry *pEntry = pIter->pCursor;` |
|      675 | 2393 | `	if( pEntry ){` |
|      535 | 2394 | `		pIter->pCursor = SyHashEntryNext(pEntry);` |
|      265 | 2395 | `	}` |
|      675 | 2396 | `	return pEntry;` |
|        5 | 2397 | `}` |
|      144 | 2398 | `PH7_PRIVATE void PH7_ClassInstanceIterClose(ph7_class_instance *pThis,PH7_AttrIter *pIter)` |
|        5 | 2399 | `{` |
|      149 | 2400 | `	PH7_AttrIter **ppLink = &pThis->pActiveIters;` |
|      151 | 2401 | `	while( *ppLink ){` |
|      151 | 2402 | `		if( *ppLink == pIter ){` |
|      149 | 2403 | `			*ppLink = pIter->pNextIter;` |
|      149 | 2404 | `			pIter->pNextIter = 0;` |
|      149 | 2405 | `			pIter->pCursor = 0;` |
|      149 | 2406 | `			return;` |
|        - | 2407 | `		}` |
|        3 | 2408 | `		ppLink = &(*ppLink)->pNextIter;` |
|        1 | 2409 | `	}` |
|       77 | 2410 | `}` |
|        - | 2411 | `/*` |
|        - | 2412 | ` * Remove one attribute entry from the instance, advancing any open walk parked` |
|        - | 2413 | `` * on it first. The ONLY door for an `unset($o->p)`-shaped removal: the entry is`` |
|        - | 2414 | ` * freed here, so a walker still holding it would read a recycled pool slot on` |
|        - | 2415 | ` * its next step (php visits the properties AFTER the deleted one, and so does` |
|        - | 2416 | ` * this).` |
|        - | 2417 | ` */` |
|       72 | 2418 | `PH7_PRIVATE void PH7_ClassInstanceDeleteAttrEntry(ph7_class_instance *pThis,SyHashEntry *pEntry)` |
|        2 | 2419 | `{` |
|        - | 2420 | `	PH7_AttrIter *pIter;` |
|       90 | 2421 | `	for( pIter = pThis->pActiveIters ; pIter ; pIter = pIter->pNextIter ){` |
|       17 | 2422 | `		if( pIter->pCursor == pEntry ){` |
|        9 | 2423 | `			pIter->pCursor = SyHashEntryNext(pEntry);` |
|        4 | 2424 | `		}` |
|        9 | 2425 | `	}` |
|       74 | 2426 | `	SyHashDeleteEntry2(pEntry);` |
|       74 | 2427 | `}` |
|        - | 2428 | `/*` |
|        - | 2429 | ` * The mirror: an attribute APPENDED to the table (a dynamic property created by` |
|        - | 2430 | ` * the loop body, a declared one re-created after unset()) re-arms any walk that` |
|        - | 2431 | `` * has run off the end -- php walks the LIVE table, so `foreach ($o as ...)` over`` |
|        - | 2432 | ` * a stdClass whose body keeps adding properties keeps visiting them. A walker` |
|        - | 2433 | ` * with a NULL cursor is always mid-walk: it unregisters as soon as it stops.` |
|        - | 2434 | ` */` |
|     8034 | 2435 | `PH7_PRIVATE void PH7_ClassInstanceAttrAppended(ph7_class_instance *pThis,SyHashEntry *pEntry)` |
|        5 | 2436 | `{` |
|        - | 2437 | `	PH7_AttrIter *pIter;` |
|     8039 | 2438 | `	if( pEntry == 0 ){` |
|       13 | 2439 | `		return;` |
|        - | 2440 | `	}` |
|     8033 | 2441 | `	for( pIter = pThis->pActiveIters ; pIter ; pIter = pIter->pNextIter ){` |
|        7 | 2442 | `		if( pIter->pCursor == 0 ){` |
|        7 | 2443 | `			pIter->pCursor = pEntry;` |
|        3 | 2444 | `		}` |
|        4 | 2445 | `	}` |
|     4022 | 2446 | `}` |
|        - | 2447 | `/*` |
|        - | 2448 | ` * Spell ONE attribute the way php names it wherever an object's property table is` |
|        - | 2449 | ` * handed out as keys: a private property is "\0DeclaringClass\0name", a protected` |
|        - | 2450 | ` * one "\0*\0name", a public one its bare name. The NULs are real bytes (these` |
|        - | 2451 | ` * appends are length-based), which is what keeps two same-named members from` |
|        - | 2452 | `` * different visibility levels distinct. `pKey` must already be a STRING value; its`` |
|        - | 2453 | ` * buffer is reset first, so one carrier serves a whole walk.` |
|        - | 2454 | ` */` |
|     1236 | 2455 | `PH7_PRIVATE void PH7_ClassInstanceAttrKey(ph7_class_instance *pThis,VmClassAttr *pAttr,ph7_value *pKey)` |
|        5 | 2456 | `{` |
|     1241 | 2457 | `	SyString *pAttrName = &pAttr->pAttr->sName;` |
|     1241 | 2458 | `	SyBlobReset(&pKey->sBlob);` |
|     1241 | 2459 | `	if( pAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|        - | 2460 | `		/* php mangles a private key with the class that OWNS the property, and a` |
|        - | 2461 | `		 * trait's members are owned by the class that composed them -- so the key,` |
|        - | 2462 | `		 * and every wire format built on it (serialize, the (array) cast), names` |
|        - | 2463 | `		 * the class and never the trait. */` |
|      207 | 2464 | `		ph7_class *pDecl = PH7_VmMemberOwnerClass(pAttr->pAttr->pDeclClass,pThis->pClass);` |
|      207 | 2465 | `		PH7_MemObjStringAppend(pKey,"\0",1);` |
|      207 | 2466 | `		PH7_MemObjStringAppend(pKey,SyStringData(&pDecl->sName),SyStringLength(&pDecl->sName));` |
|      207 | 2467 | `		PH7_MemObjStringAppend(pKey,"\0",1);` |
|     1139 | 2468 | `	}else if( pAttr->pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
|      238 | 2469 | `		PH7_MemObjStringAppend(pKey,"\0*\0",3);` |
|      117 | 2470 | `	}` |
|     1241 | 2471 | `	PH7_MemObjStringAppend(pKey,pAttrName->zString,pAttrName->nByte);` |
|     1241 | 2472 | `}` |
|        - | 2473 | `/*` |
|        - | 2474 | ` * Is this slot part of the RAW property table php hands a walker -- the (array)` |
|        - | 2475 | ` * cast's slot walk, get_mangled_object_vars(), array_walk() over an object? A` |
|        - | 2476 | ` * class-level member is not the object's, a typed property never written is not` |
|        - | 2477 | ` * there yet, and a php 8.4 VIRTUAL hooked property has no backing store at all.` |
|        - | 2478 | ` */` |
|     6270 | 2479 | `PH7_PRIVATE int PH7_ClassInstanceAttrPresented(VmClassAttr *pAttr)` |
|        5 | 2480 | `{` |
|     4912 | 2481 | `	return !PH7_ATTR_UNPRESENTED(pAttr)` |
|     1772 | 2482 | `		&& !PH7_ClassAttrUninitialized(pAttr)` |
|     7192 | 2483 | `		&& (pAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL) == 0;` |
|        5 | 2484 | `}` |
|        - | 2485 | `/*` |
|        - | 2486 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon] attribute.` |
|        - | 2487 | ` * This function never fail.` |
|        - | 2488 | ` */` |
|  7980290 | 2489 | `static ph7_value * ExtractClassAttrValue(ph7_vm *pVm,VmClassAttr *pAttr)` |
|        5 | 2490 | `{` |
|        - | 2491 | `	/* Extract the value */` |
|        - | 2492 | `	ph7_value *pValue;` |
|  7980295 | 2493 | `	pValue = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pAttr->nIdx);` |
|  7980295 | 2494 | `	return pValue;` |
|        5 | 2495 | `}` |
|        - | 2496 | `/*` |
|        - | 2497 | ` * Perform a clone operation on a class instance [i.e: Object in the PHP jargon].` |
|        - | 2498 | ` * The following function is called when an object is cloned at run-time` |
|        - | 2499 | ` * typically when the PH7_OP_CLONE instruction is executed.` |
|        - | 2500 | ` * Notes on object cloning.` |
|        - | 2501 | ` *` |
|        - | 2502 | ` * According to PHP language reference manual.` |
|        - | 2503 | ` * Creating a copy of an object with fully replicated properties is not always the wanted behavior.` |
|        - | 2504 | ` * A good example of the need for copy constructors. Another example is if your object holds a reference` |
|        - | 2505 | ` * to another object which it uses and when you replicate the parent object you want to create` |
|        - | 2506 | ` * a new instance of this other object so that the replica has its own separate copy.` |
|        - | 2507 | ` * An object copy is created by using the clone keyword (which calls the object's __clone() method if possible).` |
|        - | 2508 | ` * An object's __clone() method cannot be called directly.` |
|        - | 2509 | ` * $copy_of_object = clone $object;` |
|        - | 2510 | ` * When an object is cloned, PHP 5 will perform a shallow copy of all of the object's properties.` |
|        - | 2511 | ` * Any properties that are references to other variables, will remain references.` |
|        - | 2512 | ` * Once the cloning is complete, if a __clone() method is defined, then the newly created object's __clone() method` |
|        - | 2513 | ` * will be called, to allow any necessary properties that need to be changed.` |
|        - | 2514 | ` * Example #1 Cloning an object` |
|        - | 2515 | ` * <?php` |
|        - | 2516 | ` * class SubObject` |
|        - | 2517 | ` * {` |
|        - | 2518 | ` *   static $instances = 0;` |
|        - | 2519 | ` *   public $instance;` |
|        - | 2520 | ` *` |
|        - | 2521 | ` *   public function __construct() {` |
|        - | 2522 | ` *       $this->instance = ++self::$instances;` |
|        - | 2523 | ` *   }` |
|        - | 2524 | ` *` |
|        - | 2525 | ` *   public function __clone() {` |
|        - | 2526 | ` *       $this->instance = ++self::$instances;` |
|        - | 2527 | ` *   }` |
|        - | 2528 | ` * }` |
|        - | 2529 | ` *` |
|        - | 2530 | ` * class MyCloneable` |
|        - | 2531 | ` * {` |
|        - | 2532 | ` *   public $object1;` |
|        - | 2533 | ` *   public $object2;` |
|        - | 2534 | ` *` |
|        - | 2535 | ` *   function __clone()` |
|        - | 2536 | ` *   {` |
|        - | 2537 | ` *       // Force a copy of this->object, otherwise` |
|        - | 2538 | ` *       // it will point to same object.` |
|        - | 2539 | ` *       $this->object1 = clone $this->object1;` |
|        - | 2540 | ` *   }` |
|        - | 2541 | ` * }` |
|        - | 2542 | ` * $obj = new MyCloneable();` |
|        - | 2543 | ` * $obj->object1 = new SubObject();` |
|        - | 2544 | ` * $obj->object2 = new SubObject();` |
|        - | 2545 | ` * $obj2 = clone $obj;` |
|        - | 2546 | ` * print("Original Object:\n");` |
|        - | 2547 | ` * print_r($obj);` |
|        - | 2548 | ` * print("Cloned Object:\n");` |
|        - | 2549 | ` * print_r($obj2);` |
|        - | 2550 | ` * ?>` |
|        - | 2551 | ` * The above example will output:` |
|        - | 2552 | ` * Original Object:` |
|        - | 2553 | ` * MyCloneable Object` |
|        - | 2554 | ` * (` |
|        - | 2555 | ` *   [object1] => SubObject Object` |
|        - | 2556 | ` *       (` |
|        - | 2557 | ` *           [instance] => 1` |
|        - | 2558 | ` *       )` |
|        - | 2559 | ` *` |
|        - | 2560 | ` *   [object2] => SubObject Object` |
|        - | 2561 | ` *       (` |
|        - | 2562 | ` *           [instance] => 2` |
|        - | 2563 | ` *       )` |
|        - | 2564 | ` *` |
|        - | 2565 | ` * )` |
|        - | 2566 | ` * Cloned Object:` |
|        - | 2567 | ` * MyCloneable Object` |
|        - | 2568 | ` * (` |
|        - | 2569 | ` *   [object1] => SubObject Object` |
|        - | 2570 | ` *       (` |
|        - | 2571 | ` *           [instance] => 3` |
|        - | 2572 | ` *       )` |
|        - | 2573 | ` *` |
|        - | 2574 | ` *   [object2] => SubObject Object` |
|        - | 2575 | ` *       (` |
|        - | 2576 | ` *           [instance] => 2` |
|        - | 2577 | ` *       )` |
|        - | 2578 | ` * )` |
|        - | 2579 | ` */` |
|        - | 2580 | `/*` |
|        - | 2581 | `` * Is `clone` refused for this class? php's uncloneable internal classes refuse`` |
|        - | 2582 | `` * for their USER SUBCLASSES too -- `class M extends IteratorIterator {}` makes`` |
|        - | 2583 | `` * `clone $m` the same catchable Error, named after M -- because the refusal is`` |
|        - | 2584 | ` * the inherited clone_obj handler, not the class's own row. So the flag is` |
|        - | 2585 | ` * consulted up the base chain, not on the instance's class alone. (A subclass` |
|        - | 2586 | ` * declaring its own __clone() changes nothing there either: php never reaches` |
|        - | 2587 | ` * it, and neither does this engine -- the refusal answers first.)` |
|        - | 2588 | ` */` |
|      650 | 2589 | `PH7_PRIVATE int PH7_ClassIsUncloneable(ph7_class *pClass)` |
|        5 | 2590 | `{` |
|        - | 2591 | `	ph7_class *pC;` |
|     1219 | 2592 | `	for( pC = pClass ; pC ; pC = pC->pBase ){` |
|      725 | 2593 | `		if( pC->iFlags & PH7_CLASS_NOCLONE ){` |
|      161 | 2594 | `			return 1;` |
|        - | 2595 | `		}` |
|      287 | 2596 | `	}` |
|      499 | 2597 | `	return 0;` |
|      330 | 2598 | `}` |
|     1256 | 2599 | `PH7_PRIVATE ph7_class_instance * PH7_CloneClassInstance(ph7_class_instance *pSrc)` |
|        5 | 2600 | `{` |
|        - | 2601 | `	ph7_class_instance *pClone;` |
|        - | 2602 | `	ph7_class_method *pMethod;` |
|        - | 2603 | `	SyHashEntry *pEntry2;` |
|        - | 2604 | `	SyHashEntry *pEntry;` |
|        - | 2605 | `	ph7_vm *pVm;` |
|        - | 2606 | `	sxi32 rc;` |
|        - | 2607 | `	/* Allocate a new instance */` |
|     1261 | 2608 | `	pVm = pSrc->pVm;` |
|     1261 | 2609 | `	pClone = NewClassInstance(pVm,pSrc->pClass);` |
|     1261 | 2610 | `	if( pClone == 0 ){` |
|      ! 0 | 2611 | `		return 0;` |
|        - | 2612 | `	}` |
|        - | 2613 | `	/* Associate a private VM frame with this class instance */` |
|     1261 | 2614 | `	rc = PH7_VmCreateClassInstanceFrame(pVm,pClone);` |
|     1261 | 2615 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 2616 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClone);` |
|      ! 0 | 2617 | `		return 0;` |
|        - | 2618 | `	}` |
|        - | 2619 | `	/* A clone of an object whose LAZY native properties are installed has them` |
|        - | 2620 | `	 * too: php clones the C struct the table is written from, so the copy shows` |
|        - | 2621 | ``	 * what the original shows. The frame above skipped them (as it does at `new`),`` |
|        - | 2622 | `	 * so install them before the value copy below looks for the same-named slots. */` |
|     1261 | 2623 | `	if( pSrc->iFlags & VM_INSTANCE_LAZY_DONE ){` |
|        7 | 2624 | `		PH7_NativeMaterializeLazy(pVm,pClone);` |
|        3 | 2625 | `	}` |
|        - | 2626 | `	/* Duplicate object values. Iterate the SOURCE attributes and copy each into` |
|        - | 2627 | `	 * the clone's same-named slot (looked up by name, so order/count differences` |
|        - | 2628 | `	 * from dynamic properties don't matter). A dynamic (runtime-added) property` |
|        - | 2629 | `	 * has no declared counterpart in the clone, so synthesize it first — without` |
|        - | 2630 | `	 * this, a clone of a stdClass would silently lose all its dynamic properties. */` |
|     1261 | 2631 | `	SyHashResetLoopCursor(&pSrc->hAttr);` |
|     7501 | 2632 | `	while((pEntry = SyHashGetNextEntry(&pSrc->hAttr)) != 0 ){` |
|     6245 | 2633 | `		VmClassAttr *pSrcAttr = (VmClassAttr *)pEntry->pUserData;` |
|     6245 | 2634 | `		VmClassAttr *pDestAttr = 0;` |
|     6245 | 2635 | `		ph7_value *pvSrc,*pvDest = 0;` |
|        - | 2636 | `		/* Duplicate non-static attribute */` |
|     6245 | 2637 | `		if( pSrcAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|        6 | 2638 | `			continue;` |
|        - | 2639 | `		}` |
|        - | 2640 | `		/* By the source's own KEY: a private property of a BASE class is filed under` |
|        - | 2641 | `		 * php's mangled storage name, and matching on the attribute's plain name` |
|        - | 2642 | `		 * would copy it over the same-named slot of the object's own class. */` |
|     6241 | 2643 | `		pEntry2 = SyHashGet(&pClone->hAttr,pEntry->pKey,pEntry->nKeyLen);` |
|     6241 | 2644 | `		if( pEntry2 ){` |
|     6215 | 2645 | `			pDestAttr = (VmClassAttr *)pEntry2->pUserData;` |
|     6215 | 2646 | `			pvDest = ExtractClassAttrValue(pVm,pDestAttr);` |
|     3132 | 2647 | `		}else if( pSrcAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){` |
|        - | 2648 | `			/* Dynamic property: synthesize the matching slot on the clone. */` |
|       37 | 2649 | `			pvDest = PH7_VmCreateDynamicAttr(pVm,pClone,` |
|       24 | 2650 | `				SyStringData(&pSrcAttr->pAttr->sName),SyStringLength(&pSrcAttr->pAttr->sName),&pDestAttr);` |
|       15 | 2651 | `		}else if( pSrcAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_ONDEMAND ){` |
|        - | 2652 | `			/* An ON-DEMAND property is installed by the write that names it, so` |
|        - | 2653 | `			 * the clone's frame has no slot for one -- and php's copy carries it` |
|        - | 2654 | ``			 * (a cloned from-string DateInterval keeps its `date_string`). */`` |
|        3 | 2655 | `			VmRecreateDeclaredAttr(pVm,pClone,pSrcAttr->pAttr,&pDestAttr);` |
|        3 | 2656 | `			if( pDestAttr ){` |
|        3 | 2657 | `				pvDest = ExtractClassAttrValue(pVm,pDestAttr);` |
|        1 | 2658 | `			}` |
|        1 | 2659 | `		}` |
|        - | 2660 | `		/* Fetch the source value LAST: PH7_VmCreateDynamicAttr above may have` |
|        - | 2661 | `		 * reserved a slot, which used to reallocate pVm->aMemObj and dangle any` |
|        - | 2662 | `		 * ph7_value* obtained before it. Redundant since P1 (fixed segments);` |
|        - | 2663 | `		 * left for the harvest sweep. */` |
|     6241 | 2664 | `		pvSrc = ExtractClassAttrValue(pVm,pSrcAttr);` |
|     6241 | 2665 | `		if( (pSrcAttr->iState & VM_CLASS_ATTR_REFBOUND) && pDestAttr ){` |
|        - | 2666 | `			/* php preserves references across clone: the clone shares the SAME slot` |
|        - | 2667 | `			 * as the source property (both alias the referenced variable), rather` |
|        - | 2668 | `			 * than getting an independent value copy. Drop the clone's fresh private` |
|        - | 2669 | `			 * slot and repoint at the (already-pinned) shared slot. The REFBOUND flag` |
|        - | 2670 | `			 * is carried over by the iState copy below, so the clone's release also` |
|        - | 2671 | `			 * leaves the shared slot alone. */` |
|        5 | 2672 | `			if( pDestAttr->nIdx != pSrcAttr->nIdx ){` |
|        5 | 2673 | `				PH7_VmStoreFilterDrop(pVm,pDestAttr->pAttr,pDestAttr->nIdx);` |
|        5 | 2674 | `				PH7_VmUnsetMemObj(pVm,pDestAttr->nIdx,TRUE);` |
|        5 | 2675 | `				pDestAttr->nIdx = pSrcAttr->nIdx;` |
|        - | 2676 | `				/* The clone is a holder of the shared slot in its own right — take a pin` |
|        - | 2677 | `				 * for it, since its own release will give one back. */` |
|        5 | 2678 | `				VmPinMemObjSlotCounted(pVm,pDestAttr->nIdx);` |
|        3 | 2679 | `			}` |
|     6239 | 2680 | `		}else if( pvSrc && pvDest ){` |
|     6237 | 2681 | `			PH7_MemObjStore(pvSrc,pvDest);` |
|     3116 | 2682 | `		}` |
|        - | 2683 | `		/* Carry over the per-instance state so the clone matches the source:` |
|        - | 2684 | `		 * VM_CLASS_ATTR_UNINIT marks a typed property as not-yet-initialized` |
|        - | 2685 | `		 * and doubles as the readonly write-once latch — without this a clone` |
|        - | 2686 | `		 * would reset to uninitialized (losing the value's readiness) and a` |
|        - | 2687 | `		 * readonly property would become writable again. */` |
|     6241 | 2688 | `		if( pDestAttr ){` |
|     6241 | 2689 | `			pDestAttr->iState = pSrcAttr->iState;` |
|     3118 | 2690 | `		}` |
|        5 | 2691 | `	}` |
|        - | 2692 | `	/* A declared property unset() on the source is absent from the clone too (PHP). But the clone` |
|        - | 2693 | `	 * frame above materialized ALL declared attrs (with their defaults), so drop any clone attr whose` |
|        - | 2694 | `	 * name is not present on the source. Collect first, then delete — removing an entry mid-walk would` |
|        - | 2695 | `	 * free the node the SyHash loop cursor points at. */` |
|        - | 2696 | `	{` |
|        - | 2697 | `		SySet sDrop;` |
|     1261 | 2698 | `		SySetInit(&sDrop,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|     1261 | 2699 | `		SyHashResetLoopCursor(&pClone->hAttr);` |
|     7505 | 2700 | `		while((pEntry = SyHashGetNextEntry(&pClone->hAttr)) != 0 ){` |
|     6249 | 2701 | `			VmClassAttr *pCloneAttr = (VmClassAttr *)pEntry->pUserData;` |
|     6249 | 2702 | `			if( pCloneAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|        6 | 2703 | `				continue;` |
|        - | 2704 | `			}` |
|     6245 | 2705 | `			if( SyHashGet(&pSrc->hAttr,pEntry->pKey,pEntry->nKeyLen) == 0 ){` |
|        5 | 2706 | `				SySetPut(&sDrop,(const void *)&pEntry);` |
|        2 | 2707 | `			}` |
|        5 | 2708 | `		}` |
|     1261 | 2709 | `		if( SySetUsed(&sDrop) > 0 ){` |
|        5 | 2710 | `			SyHashEntry **apDrop = (SyHashEntry **)SySetBasePtr(&sDrop);` |
|        - | 2711 | `			sxu32 i;` |
|        9 | 2712 | `			for( i = 0 ; i < SySetUsed(&sDrop) ; ++i ){` |
|        5 | 2713 | `				VmClassAttr *pVmAttr = (VmClassAttr *)apDrop[i]->pUserData;` |
|        5 | 2714 | `				SyHashDeleteEntry(&pClone->hAttr,apDrop[i]->pKey,apDrop[i]->nKeyLen,0);` |
|        5 | 2715 | `				PH7_VmReleaseInstanceAttr(pVm,pVmAttr);` |
|        3 | 2716 | `			}` |
|        2 | 2717 | `		}` |
|     1261 | 2718 | `		SySetRelease(&sDrop);` |
|        - | 2719 | `	}` |
|        - | 2720 | `	/* A copy of a Closure names the same function, so it is a new holder of it --` |
|        - | 2721 | ``	 * `clone $f`, and Closure::bindTo()/bind(), which clone. Without this the`` |
|        - | 2722 | `	 * ORIGINAL's death would free a per-instantiation body the copy still calls.` |
|        - | 2723 | `	 * A no-op for every other class (one pointer compare). */` |
|     1261 | 2724 | `	PH7_VmClosureInstanceRef(pVm,pClone,1);` |
|        - | 2725 | `	/* The native clone hook (php's clone_obj handler): what the copy MEANS for a` |
|        - | 2726 | `	 * class whose instances stand for engine-side state -- a DOM wrapper's copy` |
|        - | 2727 | `	 * is a copy of the node. The nearest ancestor's hook serves a user subclass,` |
|        - | 2728 | `	 * which is php's handler inheritance. Runs before any __clone(), as php's` |
|        - | 2729 | `	 * handler does. */` |
|        - | 2730 | `	{` |
|        - | 2731 | `		ph7_class *pHook;` |
|     2497 | 2732 | `		for( pHook = pClone->pClass ; pHook ; pHook = pHook->pBase ){` |
|     1281 | 2733 | `			if( pHook->xClone ){` |
|       41 | 2734 | `				pHook->xClone(pVm,pClone,pSrc);` |
|       41 | 2735 | `				break;` |
|        - | 2736 | `			}` |
|      623 | 2737 | `		}` |
|        - | 2738 | `	}` |
|        - | 2739 | `	/* call the __clone method on the cloned object if available */` |
|     1261 | 2740 | `	pMethod = PH7_ClassExtractMethod(pClone->pClass,"__clone",sizeof("__clone")-1);` |
|     1261 | 2741 | `	if( pMethod ){` |
|      101 | 2742 | `		if( pMethod->iCloneDepth < 16 ){` |
|       99 | 2743 | `			pMethod->iCloneDepth++;` |
|        - | 2744 | `			/* PHP 8.3: __clone() may re-initialize the clone's readonly` |
|        - | 2745 | `			 * properties. Flag the instance so the readonly store guard allows` |
|        - | 2746 | `			 * it for the duration of the call. */` |
|       99 | 2747 | `			pClone->iFlags \|= VM_INSTANCE_CLONING;` |
|       99 | 2748 | `			PH7_VmCallClassMethod(pVm,pClone,pMethod,0,0,0);` |
|       99 | 2749 | `			pClone->iFlags &= ~VM_INSTANCE_CLONING;` |
|       51 | 2750 | `		}else{` |
|        - | 2751 | `			/* Nesting limit reached */` |
|        3 | 2752 | `			PH7_VmThrowError(pVm,0,PH7_CTX_ERR,"Object clone limit reached,no more call to __clone()");` |
|        - | 2753 | `		}` |
|        - | 2754 | `		/* Reset the cursor */` |
|      101 | 2755 | `		pMethod->iCloneDepth = 0;` |
|       49 | 2756 | `	}` |
|        - | 2757 | `	/* Return the cloned object */` |
|     1261 | 2758 | `	return pClone;` |
|      633 | 2759 | `}` |
|        - | 2760 | `/* CLASS_INSTANCE_DESTROYED moved to ph7int.h: the cycle collector has to know` |
|        - | 2761 | ` * an instance that is already mid-release when it walks one. */` |
|        - | 2762 | `/*` |
|        - | 2763 | ` * Free the per-instance allocations owned by ONE object attribute: its value slot (+ the typed-slot` |
|        - | 2764 | ` * enforcement entry), the synthesized ph7_class_attr for a dynamic (runtime-added) property, and the` |
|        - | 2765 | ` * VmClassAttr wrapper itself. Does NOT touch the hAttr entry node — the caller removes it` |
|        - | 2766 | `` * (`unset($o->p)` via SyHashDeleteEntry2; instance teardown via the wholesale SyHashRelease, so it must`` |
|        - | 2767 | ` * not delete entries mid-walk). Shared by PH7_ClassInstanceRelease and the OP_MEMBER unset path.` |
|        - | 2768 | ` */` |
|  9759171 | 2769 | `PH7_PRIVATE void PH7_VmReleaseInstanceAttr(ph7_vm *pVm, VmClassAttr *pVmAttr)` |
|        5 | 2770 | `{` |
|  9759176 | 2771 | `	if( pVmAttr->iState & (VM_CLASS_ATTR_REFBOUND\|VM_CLASS_ATTR_REFSRCPIN) ){` |
|        - | 2772 | `` 		/* A property at either end of a reference (`$o->p =& $x` bound it, `$r =& $o->p` `` |
|        - | 2773 | `		 * made it a source) holds its value slot with a COUNTED PIN and shares it, so it` |
|        - | 2774 | `		 * must not be released here — but the property WAS one of its holders, so give the` |
|        - | 2775 | `		 * pin back. The slot (and its value, which used to stay alive for the rest of the` |
|        - | 2776 | `		 * script) goes if the property was the last thing holding it. A SOURCE still owns` |
|        - | 2777 | `		 * its declaration, so its typed-slot enforcement entry goes with it. */` |
|      188 | 2778 | `		if( (pVmAttr->iState & VM_CLASS_ATTR_REFBOUND) == 0 ){` |
|      132 | 2779 | `			PH7_VmStoreFilterDrop(pVm,pVmAttr->pAttr,pVmAttr->nIdx);` |
|       65 | 2780 | `		}` |
|      188 | 2781 | `		VmUnpinMemObjSlot(pVm,pVmAttr->nIdx);` |
|  9759083 | 2782 | `	}else if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - | 2783 | `		/* Drop any typed-property enforcement slot registered for this memobj, before the memobj` |
|        - | 2784 | `		 * is returned to the free list, so a future recycled slot does not inherit the stale entry. */` |
|  9757664 | 2785 | `		PH7_VmStoreFilterDrop(pVm,pVmAttr->pAttr,pVmAttr->nIdx);` |
|  9757664 | 2786 | `		if( !PH7_VmSlotDropOwnerHold(pVm,pVmAttr->nIdx) ){` |
|        - | 2787 | ``			/* Nobody else names the slot. When somebody does -- `$r =& $o->p`,`` |
|        - | 2788 | ``			 * `$a[] =& $o->p` -- the value is theirs to keep and theirs to release,`` |
|        - | 2789 | `			 * exactly as php's refcount makes it: unlinking it here took the array` |
|        - | 2790 | `			 * element with it and left the variable UNDEFINED. */` |
|  9757658 | 2791 | `			PH7_VmUnsetMemObj(pVm,pVmAttr->nIdx,TRUE);` |
|  4878229 | 2792 | `		}` |
|  4878232 | 2793 | `	}` |
|        - | 2794 | `	/* A dynamic property owns its synthesized ph7_class_attr (struct + inline name in one block) —` |
|        - | 2795 | `	 * free it here (the only place a per-instance pAttr is freed; declared attrs are class-owned). */` |
|  9759176 | 2796 | `	if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){` |
|      633 | 2797 | `		SyMemBackendFree(&pVm->sAllocator,pVmAttr->pAttr);` |
|      314 | 2798 | `	}` |
|  9759176 | 2799 | `	SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|  9759176 | 2800 | `}` |
|        - | 2801 | `/*` |
|        - | 2802 | ` * Run this instance's __destruct exactly once, or raise the refusal that stands in for it.` |
|        - | 2803 | ` *` |
|        - | 2804 | ` * Called from two places: PH7_ClassInstanceRelease, where the object dies because nothing` |
|        - | 2805 | ` * refers to it any more, and the shutdown pass (VmCallShutdownDestructors), which reaches` |
|        - | 2806 | ` * every object a program left alive WITHOUT freeing it -- php's zend_objects_store_call_destructors` |
|        - | 2807 | ` * does exactly that, and the free that follows must not run the body a second time, which` |
|        - | 2808 | ` * is what CLASS_INSTANCE_DTOR_CALLED records.` |
|        - | 2809 | ` *` |
|        - | 2810 | ` * PH7_ClassInstanceCtorFailed below sets that same bit for php's other reason.` |
|        - | 2811 | ` */` |
|  1529642 | 2812 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallDestructor(ph7_class_instance *pThis)` |
|        5 | 2813 | `{` |
|        - | 2814 | `	ph7_class_method *pDestr;` |
|        - | 2815 | `	ph7_class *pClass;` |
|        - | 2816 | `	ph7_vm *pVm;` |
|  1529647 | 2817 | `	sxi32 rc = SXRET_OK;` |
|  1529647 | 2818 | `	if( pThis->iFlags & CLASS_INSTANCE_DTOR_CALLED ){` |
|   102616 | 2819 | `		return SXRET_OK;` |
|        - | 2820 | `	}` |
|        - | 2821 | `	/* Flagged whether or not there is a body to run, exactly as php flags its own` |
|        - | 2822 | `	 * (IS_OBJ_DESTRUCTOR_CALLED is set before the handler is even looked up). The` |
|        - | 2823 | `	 * shutdown pass sweeps the object pool until a round finds nothing unflagged, so` |
|        - | 2824 | `	 * an object with no __destruct at all has to come back flagged too. */` |
|  1427036 | 2825 | `	pThis->iFlags \|= CLASS_INSTANCE_DTOR_CALLED;` |
|  1427036 | 2826 | `	pVm = pThis->pVm;` |
|  1427036 | 2827 | `	pClass = pThis->pClass;` |
|  1427036 | 2828 | `	pDestr = PH7_ClassExtractMethod(pClass,"__destruct",sizeof("__destruct")-1);` |
|  1427036 | 2829 | `	if( pDestr && !pVm->bInReset ){` |
|        - | 2830 | `		/* php checks a non-public destructor's visibility HERE, against the scope` |
|        - | 2831 | `		 * the destruction happened in, and refuses with a sentence of its own: the` |
|        - | 2832 | `		 * engine reached for the method, so the message names the OBJECT's class` |
|        - | 2833 | `		 * and drops the word "method" the ordinary call refusal carries` |
|        - | 2834 | ``		 * (`Call to private B::__destruct() from global scope` for a `class B`` |
|        - | 2835 | ``		 * extends A` whose base declared it). Screening here rather than letting`` |
|        - | 2836 | `		 * the dispatcher speak is what keeps that wording; the call is then made` |
|        - | 2837 | `		 * unchecked, since this IS the check. */` |
|     1838 | 2838 | `		ph7_class *pDestrDecl = pDestr->sFunc.pUserData` |
|     1221 | 2839 | `			? (ph7_class *)pDestr->sFunc.pUserData : pClass;` |
|     1221 | 2840 | `		if( pDestr->iProtection != PH7_CLASS_PROT_PUBLIC` |
|      618 | 2841 | `		 && !PH7_VmClassMemberAccess(&(*pVm),pDestrDecl,&pDestr->sFunc.sName,` |
|        5 | 2842 | `			pDestr->iProtection,FALSE) ){` |
|        - | 2843 | `			SyBlob sErrMsg;` |
|       14 | 2844 | `			const char *zVis = pDestr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|        4 | 2845 | `				? "private" : "protected";` |
|       10 | 2846 | `			ph7_class *pScope = PH7_VmCallerScope(&(*pVm));` |
|       10 | 2847 | `			SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       10 | 2848 | `			if( pVm->bInShutdownDtor ){` |
|        - | 2849 | `				/* Reached from the shutdown pass, with no PHP frame under it. php tests` |
|        - | 2850 | ``				 * exactly that (`EG(current_execute_data) == NULL`) and answers a`` |
|        - | 2851 | `				 * different sentence at a different severity: an E_WARNING saying the` |
|        - | 2852 | `				 * call was ignored, after which the object is simply not destructed and` |
|        - | 2853 | `				 * the program is already over. The Error below is for a refusal a` |
|        - | 2854 | `				 * running program can still catch. */` |
|        8 | 2855 | `				SyBlobFormat(&sErrMsg,` |
|        - | 2856 | `					"Call to %s %z::__destruct() from global scope during shutdown ignored",` |
|        3 | 2857 | `					zVis,&pClass->sDisp);` |
|        8 | 2858 | `				SyBlobAppend(&sErrMsg,"\0",sizeof(char));` |
|        - | 2859 | `				/* Raised between two destructor bodies, so there is no frame to name:` |
|        - | 2860 | ``				 * php reports it `in Unknown on line 0`. */`` |
|        8 | 2861 | `				pVm->bNoFrameLoc = 1;` |
|        8 | 2862 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,(const char *)SyBlobData(&sErrMsg));` |
|        8 | 2863 | `				pVm->bNoFrameLoc = 0;` |
|        8 | 2864 | `				SyBlobRelease(&sErrMsg);` |
|        8 | 2865 | `				return SXRET_OK;` |
|        - | 2866 | `			}` |
|        3 | 2867 | `			if( pScope ){` |
|      ! 0 | 2868 | `				SyBlobFormat(&sErrMsg,"Call to %s %z::__destruct() from scope %z",` |
|      ! 0 | 2869 | `					zVis,&pClass->sDisp,&pScope->sDisp);` |
|      ! 0 | 2870 | `			}else{` |
|        3 | 2871 | `				SyBlobFormat(&sErrMsg,"Call to %s %z::__destruct() from global scope",` |
|        1 | 2872 | `					zVis,&pClass->sDisp);` |
|        - | 2873 | `			}` |
|        - | 2874 | `			/* Parked, not returned: this release has no channel back to the` |
|        - | 2875 | `			 * executor (nothing "called" the destruct), and the dispatcher's own` |
|        - | 2876 | `			 * screen used to do the parking for us through` |
|        - | 2877 | `			 * VmCallClassMethodWithMap. Without it the uncaught Error is printed` |
|        - | 2878 | `			 * and the program carries on past a statement php never reaches. */` |
|        4 | 2879 | `			VmBoundaryPark(&(*pVm),` |
|        1 | 2880 | `				VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|        3 | 2881 | `			SyBlobRelease(&sErrMsg);` |
|        2 | 2882 | `		}else{` |
|        - | 2883 | `			/* Invoke the destructor. Skipped during ph7_vm_reset() bulk teardown:` |
|        - | 2884 | `			 * running user PHP against a half-reset VM is unsafe (see bInReset).` |
|        - | 2885 | `			 *` |
|        - | 2886 | `			 * Pinned across the body rather than SET to a constant: reached from a` |
|        - | 2887 | `			 * release the count is 0 and any value keeps the nested unref off it, but` |
|        - | 2888 | `			 * the shutdown pass calls this on an object other names still hold, and` |
|        - | 2889 | `			 * flattening their count there would free it under them. php pins the same` |
|        - | 2890 | `			 * way (GC_ADDREF/GC_DELREF around dtor_obj), so a body that stores $this` |
|        - | 2891 | `			 * somewhere keeps the reference it gained. */` |
|     1218 | 2892 | `			sxu8 bPhase = pVm->bInShutdownDtor;` |
|        - | 2893 | `			VmResumeTarget sSaveResume;` |
|        - | 2894 | `			/* The body has a frame of its own, so php's "no execute_data" state ends` |
|        - | 2895 | `			 * here and resumes when it returns: an object the body itself drops --` |
|        - | 2896 | ``			 * `$this->p = null` on the last holder of a private-destructor object --`` |
|        - | 2897 | `			 * is refused with the catchable Error naming the running scope, not with` |
|        - | 2898 | `			 * the shutdown warning above. */` |
|     1218 | 2899 | `			pVm->bInShutdownDtor = 0;` |
|     1218 | 2900 | `			pThis->iRef += 2; /* Prevent garbage collection */` |
|        - | 2901 | `			/* A destructor runs in the MIDDLE of somebody else's control flow: the` |
|        - | 2902 | `			 * release that reaches it is usually a frame teardown on an unwind that` |
|        - | 2903 | `			 * is already carrying a throw. php hides the in-flight exception for the` |
|        - | 2904 | `			 * duration (zend_objects_store_del saves EG(exception), clears it, and` |
|        - | 2905 | `			 * puts it back afterwards) precisely so the body cannot observe or` |
|        - | 2906 | `			 * disturb it. PHL's in-place-catch resume record is that same in-flight` |
|        - | 2907 | `			 * state — it says "the throw now unwinding was already caught at frame F,` |
|        - | 2908 | `			 * pad P" — and a destructor body with a try/catch of its own writes a` |
|        - | 2909 | `			 * record when ITS catch finishes, overwriting the one the outer throw is` |
|        - | 2910 | ``			 * still owed. monolog's `Handler::__destruct` is exactly that shape`` |
|        - | 2911 | ``			 * (`try { $this->close(); } catch (Throwable) {}`), and the outer throw`` |
|        - | 2912 | `			 * then never landed: the script that was catching it simply ENDED.` |
|        - | 2913 | `			 * Save, clear, restore — and, like php, let a record the body LEAVES` |
|        - | 2914 | `			 * behind (a throw of its own still in flight) supersede the saved one. */` |
|     1218 | 2915 | `			VmSaveResumeTarget(pVm,&sSaveResume);` |
|     1218 | 2916 | `			VmClearResumeTarget(pVm);` |
|     1218 | 2917 | `			rc = PH7_VmCallMethodUnchecked(pVm,pThis,pDestr,0,0,0);` |
|     1218 | 2918 | `			if( pVm->pResumeFrame == 0 ){` |
|     1216 | 2919 | `				VmRestoreResumeTarget(pVm,&sSaveResume);` |
|      604 | 2920 | `			}` |
|     1218 | 2921 | `			pThis->iRef -= 2;` |
|     1218 | 2922 | `			pVm->bInShutdownDtor = bPhase;` |
|        - | 2923 | `		}` |
|      606 | 2924 | `	}` |
|        - | 2925 | `	/* SXERR_ABORT here means the body left an UNCAUGHT throwable (or exited). php runs` |
|        - | 2926 | `	 * its whole destructor phase under one zend_try, so the first one abandons every` |
|        - | 2927 | `	 * destructor still owed -- including the ones the symbol-table half would have` |
|        - | 2928 | `	 * reached, which is why the decision is recorded on the VM and not just returned. */` |
|  1427030 | 2929 | `	if( rc == SXERR_ABORT && pVm->bInShutdownDtor ){` |
|        6 | 2930 | `		pVm->bShutdownAborted = 1;` |
|        2 | 2931 | `	}` |
|  1427030 | 2932 | `	return rc;` |
|   764660 | 2933 | `}` |
|        - | 2934 | `/*` |
|        - | 2935 | ` * A constructor CALL raised, so this object never became one: php marks it` |
|        - | 2936 | ` * (zend_object_store_ctor_failed sets the very bit that records "the destructor` |
|        - | 2937 | ` * has been reached for") and its __destruct is therefore never run -- not when the` |
|        - | 2938 | `` * half-built object is dropped at the `new`, and not later either, because the mark`` |
|        - | 2939 | ` * lives on the object and follows it wherever the constructor happened to store` |
|        - | 2940 | `` * `$this` before it threw. PHL ran the destructor on both, so monolog's`` |
|        - | 2941 | `` * `Handler::__destruct` -- a `try { $this->close(); } catch {}` -- executed against`` |
|        - | 2942 | ` * an instance whose typed properties were still uninitialised, in the middle of the` |
|        - | 2943 | ` * unwind that was already carrying the constructor's own exception.` |
|        - | 2944 | ` *` |
|        - | 2945 | `` * Called at every door that CALLS a constructor: `new` itself, and Reflection's`` |
|        - | 2946 | ` * newInstance family (php flags at each of those and nowhere else).` |
|        - | 2947 | ` */` |
|   100925 | 2948 | `PH7_PRIVATE void PH7_ClassInstanceCtorFailed(ph7_class_instance *pThis)` |
|        5 | 2949 | `{` |
|   100930 | 2950 | `	if( pThis ){` |
|   100930 | 2951 | `		pThis->iFlags \|= CLASS_INSTANCE_DTOR_CALLED;` |
|    50462 | 2952 | `	}` |
|   100930 | 2953 | `}` |
|        - | 2954 | `/*` |
|        - | 2955 | ` * Release a class instance [i.e: Object in the PHP jargon] and invoke any defined destructor.` |
|        - | 2956 | ` * This routine is invoked as soon as there are no other references to a particular` |
|        - | 2957 | ` * class instance.` |
|        - | 2958 | ` */` |
|  1526488 | 2959 | `static void PH7_ClassInstanceRelease(ph7_class_instance *pThis)` |
|        5 | 2960 | `{` |
|        - | 2961 | `	SyHashEntry *pEntry;` |
|        - | 2962 | `	ph7_class *pClass;` |
|        - | 2963 | `	ph7_vm *pVm;` |
|  1526493 | 2964 | `	if( pThis->iFlags & CLASS_INSTANCE_DESTROYED ){` |
|        - | 2965 | `		/*` |
|        - | 2966 | `		 * Already destroyed,return immediately.` |
|        - | 2967 | `		 * This could happend if someone perform unset($this) in the destructor body.` |
|        - | 2968 | `		 */` |
|      ! 0 | 2969 | `		return;` |
|        - | 2970 | `	}` |
|        - | 2971 | `	/* Mark as destroyed */` |
|  1526493 | 2972 | `	pThis->iFlags \|= CLASS_INSTANCE_DESTROYED;` |
|  1526493 | 2973 | `	pVm = pThis->pVm;` |
|  1526493 | 2974 | `	pClass = pThis->pClass;` |
|        - | 2975 | `	/* Invoke any defined destructor if available (a no-op once the shutdown pass` |
|        - | 2976 | `	 * has already run it) */` |
|  1526493 | 2977 | `	PH7_ClassInstanceCallDestructor(pThis);` |
|        - | 2978 | ``	/* php: a destructor may RESURRECT the object. Anything the body hands `$this` to`` |
|        - | 2979 | `	 * that outlives the release -- a registry, a property of something still alive, a` |
|        - | 2980 | ``	 * closure's `use ($this)` -- is a new reference taken while the refcount was`` |
|        - | 2981 | `	 * already at zero, and zend_objects_store_del re-reads it after dtor_obj and frees` |
|        - | 2982 | `	 * ONLY when it is still zero. PHL freed unconditionally, so every holder the` |
|        - | 2983 | `	 * destructor had just handed the object to was left pointing at freed memory.` |
|        - | 2984 | `	 *` |
|        - | 2985 | `	 * Pest is exactly that shape: a TestCase's teardown registers closures that capture` |
|        - | 2986 | ``	 * `$this`, and the next `new` of a test case read the dead object through one of`` |
|        - | 2987 | `	 * them -- a segfault a fifth of the way into any suite it runs, with the refcount` |
|        - | 2988 | `	 * still reading 7.` |
|        - | 2989 | `	 *` |
|        - | 2990 | `	 * Clearing DESTROYED is what lets the object die properly LATER: when its new` |
|        - | 2991 | `	 * holders drop it to zero this runs again, and CLASS_INSTANCE_DTOR_CALLED (set by` |
|        - | 2992 | `	 * PH7_ClassInstanceCallDestructor, php's IS_OBJ_DESTRUCTOR_CALLED) keeps the` |
|        - | 2993 | `	 * destructor from running a second time -- php's rule for the same case. */` |
|  1526493 | 2994 | `	if( pThis->iRef > 0 ){` |
|      ! 0 | 2995 | `		pThis->iFlags &= ~CLASS_INSTANCE_DESTROYED;` |
|      ! 0 | 2996 | `		return;` |
|        - | 2997 | `	}` |
|        - | 2998 | `	/* A native class's own teardown, while its slots are still readable. Not a` |
|        - | 2999 | `	 * __destruct: the classes that need this (WeakReference) declare none in php,` |
|        - | 3000 | `	 * and Reflection must not grow one.` |
|        - | 3001 | `	 *` |
|        - | 3002 | `	 * Resolved through the ANCESTORS, like php's own free_obj handler: a` |
|        - | 3003 | `	 * subclass inherits it unless it declares one of its own. Reading it off` |
|        - | 3004 | `	 * this class alone left every subclass of a handle-owning native class` |
|        - | 3005 | ``	 * without teardown -- `Pdo\Sqlite` (which is how PDO::connect() answers) and`` |
|        - | 3006 | ``	 * any userland `extends PDO` alike, both of which then died holding engine`` |
|        - | 3007 | `	 * state that believed it was still reachable. */` |
|        - | 3008 | `	{` |
|  1526493 | 3009 | `		ph7_class *pOwner = pClass;` |
|  3153007 | 3010 | `		while( pOwner && pOwner->xRelease == 0 ){` |
|  1626519 | 3011 | `			pOwner = pOwner->pBase;` |
|        5 | 3012 | `		}` |
|  1526493 | 3013 | `		if( pOwner && pOwner->xRelease ){` |
|    28069 | 3014 | `			pOwner->xRelease(pVm,pThis);` |
|    13910 | 3015 | `		}` |
|        - | 3016 | `	}` |
|        - | 3017 | `	/* Weak-reference registry: kill the cell for this instance so every` |
|        - | 3018 | `	 * WeakReference/WeakMap handle observes the death (the cell outlives the` |
|        - | 3019 | `	 * instance until its own handles drop; removing the hash entry here keeps` |
|        - | 3020 | `	 * a pool-reused address from resurrecting a dead cell). */` |
|  1526493 | 3021 | `	if( SyHashTotalEntry(&pVm->hWeakCell) > 0 ){` |
|    18090 | 3022 | `		void *pCellData = 0;` |
|    18088 | 3023 | `		if( SyHashDeleteEntry(&pVm->hWeakCell,(const void *)&pThis,sizeof(void *),&pCellData) == SXRET_OK` |
|     9061 | 3024 | `		 && pCellData ){` |
|       32 | 3025 | `			((VmWeakCell *)pCellData)->pObj = 0;` |
|       15 | 3026 | `		}` |
|     9044 | 3027 | `	}` |
|        - | 3028 | `	/* Release non-static attributes (the wholesale SyHashRelease below frees the entry nodes,` |
|        - | 3029 | `	 * so the helper must not delete them mid-walk). */` |
|  1526493 | 3030 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
| 11285588 | 3031 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|  9759100 | 3032 | `		PH7_VmReleaseInstanceAttr(pVm,(VmClassAttr *)pEntry->pUserData);` |
|        5 | 3033 | `	}` |
|        - | 3034 | `	/* Release the whole structure */` |
|  1526493 | 3035 | `	SyHashRelease(&pThis->hAttr);` |
|        - | 3036 | `	/* ...and stop the collector's root buffer naming memory that is going back` |
|        - | 3037 | `	 * to the pool. */` |
|  1526493 | 3038 | `	PH7_GcForget(pVm,(void *)pThis,0);` |
|  1526493 | 3039 | `	SyMemBackendPoolFree(&pVm->sAllocator,pThis);` |
|   763086 | 3040 | `}` |
|        - | 3041 | `/*` |
|        - | 3042 | ` * Decrement the reference count of a class instance [i.e Object in the PHP jargon].` |
|        - | 3043 | ` * If the reference count reaches zero,release the whole instance.` |
|        - | 3044 | ` */` |
|  7707621 | 3045 | `PH7_PRIVATE void PH7_ClassInstanceUnref(ph7_class_instance *pThis)` |
|        5 | 3046 | `{` |
|  7707626 | 3047 | `	pThis->iRef--;` |
|  7707626 | 3048 | `	if( pThis->iRef < 1 ){` |
|        - | 3049 | `		/* No more reference to this instance */` |
|  1526493 | 3050 | `		PH7_ClassInstanceRelease(&(*pThis));` |
|   763086 | 3051 | `	}else{` |
|        - | 3052 | `		/* Still held -- but by whom? A drop that does NOT reach zero is the only` |
|        - | 3053 | `		 * event that can strand a cycle, so it is what the collector buffers. */` |
|  6181138 | 3054 | `		PH7_GcPossibleRoot(pThis->pVm,(void *)pThis,0);` |
|        - | 3055 | `	}` |
|  7707626 | 3056 | `}` |
|        - | 3057 | `static sxi32 ClassInstanceCmpAttr(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest);` |
|        - | 3058 | `/*` |
|        - | 3059 | ` * Compare two class instances [i.e: Objects in the PHP jargon]` |
|        - | 3060 | ` * Note on objects comparison:` |
|        - | 3061 | ` *  According to the PHP langauge reference manual` |
|        - | 3062 | ` *  When using the comparison operator (==), object variables are compared in a simple manner` |
|        - | 3063 | ` *  namely: Two object instances are equal if they have the same attributes and values, and are` |
|        - | 3064 | ` *  instances of the same class.` |
|        - | 3065 | ` *  On the other hand, when using the identity operator (===), object variables are identical` |
|        - | 3066 | ` *  if and only if they refer to the same instance of the same class.` |
|        - | 3067 | ` *  An example will clarify these rules.` |
|        - | 3068 | ` *  Example #1 Example of object comparison` |
|        - | 3069 | ` *  <?php` |
|        - | 3070 | ` *    function bool2str($bool)` |
|        - | 3071 | ` * {` |
|        - | 3072 | ` *   if ($bool === false) {` |
|        - | 3073 | ` *       return 'FALSE';` |
|        - | 3074 | ` *   } else {` |
|        - | 3075 | ` *       return 'TRUE';` |
|        - | 3076 | ` *   }` |
|        - | 3077 | ` * }` |
|        - | 3078 | ` * function compareObjects(&$o1, &$o2)` |
|        - | 3079 | ` * {` |
|        - | 3080 | ` *   echo 'o1 == o2 : ' . bool2str($o1 == $o2) . "\n";` |
|        - | 3081 | ` *   echo 'o1 != o2 : ' . bool2str($o1 != $o2) . "\n";` |
|        - | 3082 | ` *   echo 'o1 === o2 : ' . bool2str($o1 === $o2) . "\n";` |
|        - | 3083 | ` *   echo 'o1 !== o2 : ' . bool2str($o1 !== $o2) . "\n";` |
|        - | 3084 | ` * }` |
|        - | 3085 | ` * class Flag` |
|        - | 3086 | ` * {` |
|        - | 3087 | ` *   public $flag;` |
|        - | 3088 | ` *` |
|        - | 3089 | ` *   function Flag($flag = true) {` |
|        - | 3090 | ` *       $this->flag = $flag;` |
|        - | 3091 | ` *   }` |
|        - | 3092 | ` * }` |
|        - | 3093 | ` *` |
|        - | 3094 | ` * class OtherFlag` |
|        - | 3095 | ` * {` |
|        - | 3096 | ` *   public $flag;` |
|        - | 3097 | ` *` |
|        - | 3098 | ` *   function OtherFlag($flag = true) {` |
|        - | 3099 | ` *       $this->flag = $flag;` |
|        - | 3100 | ` *   }` |
|        - | 3101 | ` * }` |
|        - | 3102 | ` *` |
|        - | 3103 | ` * $o = new Flag();` |
|        - | 3104 | ` * $p = new Flag();` |
|        - | 3105 | ` * $q = $o;` |
|        - | 3106 | ` * $r = new OtherFlag();` |
|        - | 3107 | ` *` |
|        - | 3108 | ` * echo "Two instances of the same class\n";` |
|        - | 3109 | ` * compareObjects($o, $p);` |
|        - | 3110 | ` * echo "\nTwo references to the same instance\n";` |
|        - | 3111 | ` * compareObjects($o, $q);` |
|        - | 3112 | ` * echo "\nInstances of two different classes\n";` |
|        - | 3113 | ` * compareObjects($o, $r);` |
|        - | 3114 | ` * ?>` |
|        - | 3115 | ` * The above example will output:` |
|        - | 3116 | ` * Two instances of the same class` |
|        - | 3117 | ` * o1 == o2 : TRUE` |
|        - | 3118 | ` * o1 != o2 : FALSE` |
|        - | 3119 | ` * o1 === o2 : FALSE` |
|        - | 3120 | ` * o1 !== o2 : TRUE` |
|        - | 3121 | ` * Two references to the same instance` |
|        - | 3122 | ` * o1 == o2 : TRUE` |
|        - | 3123 | ` * o1 != o2 : FALSE` |
|        - | 3124 | ` * o1 === o2 : TRUE` |
|        - | 3125 | ` * o1 !== o2 : FALSE` |
|        - | 3126 | ` * Instances of two different classes` |
|        - | 3127 | ` * o1 == o2 : FALSE` |
|        - | 3128 | ` * o1 != o2 : TRUE` |
|        - | 3129 | ` * o1 === o2 : FALSE` |
|        - | 3130 | ` * o1 !== o2 : TRUE` |
|        - | 3131 | ` *` |
|        - | 3132 | ` * This function return 0 if the objects are equals according to the comprison rules defined above.` |
|        - | 3133 | ` * Any other return values indicates difference.` |
|        - | 3134 | ` */` |
|     1400 | 3135 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest)` |
|        5 | 3136 | `{` |
|        - | 3137 | `	sxi32 rc;` |
|        - | 3138 | `	/*` |
|        - | 3139 | `	 * php's identity shortcut, and it comes FIRST -- before the same-class screen` |
|        - | 3140 | ``	 * and before any handler: `$i == $i` is 0 for a DateInterval, the one pair of`` |
|        - | 3141 | `	 * intervals php will compare at all.` |
|        - | 3142 | `	 */` |
|     1405 | 3143 | `	if( pLeft == pRight ){` |
|        - | 3144 | `		/* Same instance,don't bother processing,object are equals */` |
|      451 | 3145 | `		return 0;` |
|        - | 3146 | `	}` |
|      959 | 3147 | `	if( bStrict ){` |
|        - | 3148 | `		/*` |
|        - | 3149 | `		 * According to the PHP language reference manual:` |
|        - | 3150 | `		 *  when using the identity operator (===), object variables` |
|        - | 3151 | `		 *  are identical if and only if they refer to the same instance` |
|        - | 3152 | `		 *  of the same class.` |
|        - | 3153 | `		 * Two DISTINCT instances, so this is never identical -- and no compare` |
|        - | 3154 | ``		 * handler is asked, because php's `===` is pointer identity and never`` |
|        - | 3155 | `		 * reaches one.` |
|        - | 3156 | `		 */` |
|      136 | 3157 | `		return 1;` |
|        - | 3158 | `	}` |
|        - | 3159 | `	/*` |
|        - | 3160 | `	 * php's compare handler (ph7_class::xCmp), asked of the LEFT operand and` |
|        - | 3161 | `	 * ABOVE the same-class screen: a DateTime and a DateTimeImmutable of the same` |
|        - | 3162 | `	 * instant are equal there, which no property walk between two different` |
|        - | 3163 | `	 * classes could ever answer. A class with no handler falls through to the` |
|        - | 3164 | `	 * walk, which is php's zend_std_compare_objects.` |
|        - | 3165 | `	 */` |
|      827 | 3166 | `	if( PH7_ClassNativeCmp(pLeft,pRight,&rc) ){` |
|      180 | 3167 | `		return rc;` |
|        - | 3168 | `	}` |
|        - | 3169 | `	/* Comparison is performed only if the objects are instance of the same class */` |
|      651 | 3170 | `	if( pLeft->pClass != pRight->pClass ){` |
|       14 | 3171 | `		return 1;` |
|        - | 3172 | `	}` |
|        - | 3173 | `	/*` |
|        - | 3174 | `	 * Attribute comparison.` |
|        - | 3175 | `	 * According to the PHP reference manual:` |
|        - | 3176 | `	 *  When using the comparison operator (==), object variables are compared` |
|        - | 3177 | `	 *  in a simple manner, namely: Two object instances are equal if they have` |
|        - | 3178 | `	 *  the same attributes and values, and are instances of the same class.` |
|        - | 3179 | `	 */` |
|        - | 3180 | `	/* Closures compare by IDENTITY under == as well (not by attributes): two distinct` |
|        - | 3181 | `	 * Closure instances are never equal, even when they wrap the same underlying function` |
|        - | 3182 | `	 * (PHP semantics). pLeft != pRight here, so a Closure pair is unequal. Without this,` |
|        - | 3183 | `` 	 * two capture-less lambdas of the same `function(){}` share the template's `$__fn` `` |
|        - | 3184 | `	 * name and would compare equal. */` |
|      639 | 3185 | `	if( pLeft->pVm->pClosureClass && pLeft->pClass == pLeft->pVm->pClosureClass ){` |
|        5 | 3186 | `		return 1;` |
|        - | 3187 | `	}` |
|        - | 3188 | `	/* Same class but a different number of attributes ⇒ different property sets` |
|        - | 3189 | `	 * (dynamic properties can give two same-class instances different counts). */` |
|      635 | 3190 | `	if( pLeft->hAttr.nEntry != pRight->hAttr.nEntry ){` |
|        3 | 3191 | `		return 1;` |
|        - | 3192 | `	}` |
|      633 | 3193 | `	if( (pLeft->iFlags & VM_INSTANCE_COMPARING) \|\| iNest > PH7_CMP_MAX_DEPTH ){` |
|        - | 3194 | `		/* This object is its own descendant -- php's Z_IS_RECURSIVE_P(o1) test, asked` |
|        - | 3195 | `		 * exactly here, below every screen that answers without walking and above the` |
|        - | 3196 | `		 * property walk that recurses -- or the finite-nesting backstop tripped. Either` |
|        - | 3197 | `		 * way php's catchable Error, recorded for whichever door onto the comparator` |
|        - | 3198 | `		 * can throw. The old depth counter was neither: it refused a merely-deep graph` |
|        - | 3199 | `		 * php compares fine, and reported a cycle only after walking 31 levels of it. */` |
|        5 | 3200 | `		PH7_CmpRefusalNesting(pLeft->pVm);` |
|        5 | 3201 | `		return 1;` |
|        - | 3202 | `	}` |
|      629 | 3203 | `	pLeft->iFlags \|= VM_INSTANCE_COMPARING;` |
|      629 | 3204 | `	rc = ClassInstanceCmpAttr(&(*pLeft),&(*pRight),bStrict,iNest);` |
|      629 | 3205 | `	pLeft->iFlags &= ~VM_INSTANCE_COMPARING;` |
|      629 | 3206 | `	return rc;` |
|      705 | 3207 | `}` |
|        - | 3208 | `/*` |
|        - | 3209 | ` * php's zend_std_compare_objects property walk: every non-static, non-constant, non-virtual` |
|        - | 3210 | ` * attribute of the left instance against the RIGHT attribute of the same name. Split out of` |
|        - | 3211 | ` * PH7_ClassInstanceCmp so the recursion mark that function sets is cleared on every exit.` |
|        - | 3212 | ` */` |
|      624 | 3213 | `static sxi32 ClassInstanceCmpAttr(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest)` |
|        5 | 3214 | `{` |
|        - | 3215 | `	SyHashEntry *pEntry,*pEntry2;` |
|        - | 3216 | `	ph7_value sV1,sV2;` |
|        - | 3217 | `	sxi32 rc;` |
|      629 | 3218 | `	PH7_MemObjInit(pLeft->pVm,&sV1);` |
|      629 | 3219 | `	PH7_MemObjInit(pLeft->pVm,&sV2);` |
|      629 | 3220 | `	sV1.nIdx = sV2.nIdx = SXU32_HIGH;` |
|        - | 3221 | `	/* Compare each left attribute against the RIGHT attribute of the SAME NAME` |
|        - | 3222 | `	 * (not in lockstep): dynamic properties may be stored in a different order` |
|        - | 3223 | `	 * on the two instances. Counts already match, so if every left attribute has` |
|        - | 3224 | `	 * an equal-valued same-named right attribute the property sets are equal. */` |
|      629 | 3225 | `	SyHashResetLoopCursor(&pLeft->hAttr);` |
|     1251 | 3226 | `	while((pEntry = SyHashGetNextEntry(&pLeft->hAttr)) != 0 ){` |
|      722 | 3227 | `		VmClassAttr *p1 = (VmClassAttr *)pEntry->pUserData;` |
|        - | 3228 | `		VmClassAttr *p2;` |
|        - | 3229 | `		ph7_value *pL,*pR;` |
|        - | 3230 | `		/* Compare only non-static attribute. A native class's VIRTUAL property is` |
|        - | 3231 | `		 * skipped too: php fabricates DatePeriod's seven on demand and its real` |
|        - | 3232 | `		 * property table is empty, so any two DatePeriods are equal there whatever` |
|        - | 3233 | `		 * they contain -- while a subclass's own property, which IS in the table,` |
|        - | 3234 | `		 * still decides. */` |
|      722 | 3235 | `		if( p1->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC` |
|        - | 3236 | `		                        \|PH7_CLASS_ATTR_NATIVE_VIRTUAL) ){` |
|       71 | 3237 | `			continue;` |
|        - | 3238 | `		}` |
|      652 | 3239 | `		pEntry2 = SyHashGet(&pRight->hAttr,SyStringData(&p1->pAttr->sName),SyStringLength(&p1->pAttr->sName));` |
|      652 | 3240 | `		if( pEntry2 == 0 ){` |
|        - | 3241 | `			/* Left has a property the right lacks ⇒ not equal. */` |
|      ! 0 | 3242 | `			return 1;` |
|        - | 3243 | `		}` |
|      652 | 3244 | `		p2 = (VmClassAttr *)pEntry2->pUserData;` |
|      652 | 3245 | `		pL = ExtractClassAttrValue(pLeft->pVm,p1);` |
|      652 | 3246 | `		pR = ExtractClassAttrValue(pRight->pVm,p2);` |
|      652 | 3247 | `		if( pL && pR ){` |
|      652 | 3248 | `			PH7_MemObjLoad(pL,&sV1);` |
|      652 | 3249 | `			PH7_MemObjLoad(pR,&sV2);` |
|        - | 3250 | `			/* Compare the two values now */` |
|      652 | 3251 | `			rc = PH7_MemObjCmp(&sV1,&sV2,bStrict,iNest+1);` |
|      652 | 3252 | `			PH7_MemObjRelease(&sV1);` |
|      652 | 3253 | `			PH7_MemObjRelease(&sV2);` |
|      652 | 3254 | `			if( rc != 0 ){` |
|        - | 3255 | `				/* Not equals */` |
|       99 | 3256 | `				return rc;` |
|        - | 3257 | `			}` |
|      276 | 3258 | `		}` |
|        3 | 3259 | `	}` |
|        - | 3260 | `	/* Object are equals */` |
|      532 | 3261 | `	return 0;` |
|      317 | 3262 | `}` |
|        - | 3263 | `/*` |
|        - | 3264 | ` * Dump a class instance and the store the dump in the BLOB given` |
|        - | 3265 | ` * as the first argument.` |
|        - | 3266 | ` * Note that only non-static/non-constants attribute are dumped.` |
|        - | 3267 | ` * This function is typically invoked when the user issue a call` |
|        - | 3268 | ` * to [var_dump(),var_export(),print_r(),...].` |
|        - | 3269 | ` * This function SXRET_OK on success. Any other return value including` |
|        - | 3270 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|        - | 3271 | ` */` |
|        - | 3272 | `/*` |
|        - | 3273 | `` * Return the `name` property value of an enum case instance (the case name),`` |
|        - | 3274 | ` * or 0 when unavailable. Shared by the var_dump/var_export/json/serialize` |
|        - | 3275 | ` * renderers, which all print enum cases as Class::CaseName forms.` |
|        - | 3276 | ` */` |
|       32 | 3277 | `PH7_PRIVATE ph7_value * PH7_EnumCaseNameValue(ph7_class_instance *pThis)` |
|        1 | 3278 | `{` |
|        - | 3279 | `	SyHashEntry *pEntry;` |
|       33 | 3280 | `	if( (pThis->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|      ! 0 | 3281 | `		return 0;` |
|        - | 3282 | `	}` |
|       33 | 3283 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)"name",sizeof("name")-1);` |
|       33 | 3284 | `	if( pEntry == 0 ){` |
|      ! 0 | 3285 | `		return 0;` |
|        - | 3286 | `	}` |
|       33 | 3287 | `	return PH7_ClassInstanceExtractAttrValue(pThis,(VmClassAttr *)pEntry->pUserData);` |
|       17 | 3288 | `}` |
|        - | 3289 | `/*` |
|        - | 3290 | `` * Return the `value` property value (the backing value) of an enum case`` |
|        - | 3291 | ` * instance, or 0 when unavailable (pure enums have none).` |
|        - | 3292 | ` */` |
|       20 | 3293 | `PH7_PRIVATE ph7_value * PH7_EnumCaseBackingValueOf(ph7_class_instance *pThis)` |
|        1 | 3294 | `{` |
|        - | 3295 | `	SyHashEntry *pEntry;` |
|       21 | 3296 | `	if( (pThis->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|      ! 0 | 3297 | `		return 0;` |
|        - | 3298 | `	}` |
|       21 | 3299 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)"value",sizeof("value")-1);` |
|       21 | 3300 | `	if( pEntry == 0 ){` |
|        7 | 3301 | `		return 0;` |
|        - | 3302 | `	}` |
|       15 | 3303 | `	return PH7_ClassInstanceExtractAttrValue(pThis,(VmClassAttr *)pEntry->pUserData);` |
|       11 | 3304 | `}` |
|        - | 3305 | `/*` |
|        - | 3306 | ` * Emit a class-instance dump header plus its trailing newline. For var_dump` |
|        - | 3307 | ` * (ShowType) it completes the "object(" prefix the caller already emitted as` |
|        - | 3308 | ` *   ClassName)#<id> (<count>) {` |
|        - | 3309 | ` * for print_r it emits the legacy PHL  Object(ClassName) {  (count/id unused).` |
|        - | 3310 | `` * Enum cases print php's `ClassName Enum {` print_r header (var_dump never`` |
|        - | 3311 | `` * reaches here for enums — PH7_MemObjDump prints `enum(S::A)` directly).`` |
|        - | 3312 | ` */` |
|      496 | 3313 | `static void DumpClassInstanceHeader(SyBlob *pOut,ph7_class *pClass,sxu32 nObjId,int ShowType,sxu32 nCount)` |
|        5 | 3314 | `{` |
|      501 | 3315 | `	if( ShowType ){` |
|        - | 3316 | ``		/* var_dump: `object(C)#id (n) {` */`` |
|      343 | 3317 | `		SyBlobFormat(&(*pOut),"object(%z)#%u (%u) {",&pClass->sDisp,nObjId,nCount);` |
|      343 | 3318 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      343 | 3319 | `		return;` |
|        - | 3320 | `	}` |
|        - | 3321 | ``	/* print_r: `C Object` / `E Enum[:backing]` — the '(' line is emitted by`` |
|        - | 3322 | `	 * the body renderer at the container indent. */` |
|      163 | 3323 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|      ! 0 | 3324 | `		SyBlobFormat(&(*pOut),"%z Enum",&pClass->sDisp);` |
|      ! 0 | 3325 | `		if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|      ! 0 | 3326 | `			SyBlobAppend(&(*pOut),":int",sizeof(":int")-1);` |
|      ! 0 | 3327 | `		}else if( pClass->nEnumBacking == MEMOBJ_STRING ){` |
|      ! 0 | 3328 | `			SyBlobAppend(&(*pOut),":string",sizeof(":string")-1);` |
|      ! 0 | 3329 | `		}` |
|      ! 0 | 3330 | `	}else{` |
|      163 | 3331 | `		SyBlobFormat(&(*pOut),"%z Object",&pClass->sDisp);` |
|        - | 3332 | `	}` |
|      163 | 3333 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      253 | 3334 | `}` |
|        - | 3335 | `/*` |
|        - | 3336 | ` * The class that DECLARED pAttr: inheritance shares attr pointers down the` |
|        - | 3337 | ` * chain, so the declaring class is the most ANCESTRAL class whose hAttr still` |
|        - | 3338 | ` * maps the name to this exact pointer. php's var_dump/print_r use it for the` |
|        - | 3339 | `` * `["p":"Decl":private]` annotation.`` |
|        - | 3340 | ` */` |
|       54 | 3341 | `static ph7_class * OoAttrDeclaringClass(ph7_class *pClass,ph7_class_attr *pAttr)` |
|        2 | 3342 | `{` |
|        - | 3343 | `	/* Attrs record their declaring class at install time (inheritance/trait` |
|        - | 3344 | `	 * copies share the pointer, so the field survives the chain) -- and a TRAIT's` |
|        - | 3345 | `	 * members belong to the class that composed them, which is what php names. */` |
|       56 | 3346 | `	return PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|        2 | 3347 | `}` |
|        - | 3348 | `/*` |
|        - | 3349 | ` * php's zend_unmangle_property_name_ex: a property key carries its own` |
|        - | 3350 | ` * visibility when it is MANGLED — "\0*\0name" is protected and "\0Class\0name"` |
|        - | 3351 | ` * is private to Class, which is how the (array) cast, __debugInfo() and every` |
|        - | 3352 | ` * get_debug_info handler say what a plain array key cannot. A key that does not` |
|        - | 3353 | ` * begin with a NUL is a public name and comes back unchanged.` |
|        - | 3354 | ` *` |
|        - | 3355 | ` * Answers 0 for a key that begins with a NUL and is NOT a well-formed mangled` |
|        - | 3356 | ` * name — php's "Illegal member variable name" (nothing after the NUL, or an` |
|        - | 3357 | ` * empty class part) and "Corrupt member variable name" (no second NUL, or` |
|        - | 3358 | ` * nothing after it). php renders those raw, notice aside, and so does the caller.` |
|        - | 3359 | ` */` |
|      734 | 3360 | `PH7_PRIVATE int PH7_UnmangleAttrName(const char *zKey,sxu32 nKey,SyString *pClass,SyString *pName)` |
|        4 | 3361 | `{` |
|        - | 3362 | `	sxu32 nCls,nSrc;` |
|      738 | 3363 | `	SyStringInitFromBuf(pClass,0,0);` |
|      738 | 3364 | `	SyStringInitFromBuf(pName,zKey,nKey);` |
|      738 | 3365 | `	if( nKey < 1 \|\| zKey[0] != 0 ){` |
|      576 | 3366 | `		return 1;   /* a plain public name */` |
|        - | 3367 | `	}` |
|      163 | 3368 | `	if( nKey < 3 \|\| zKey[1] == 0 ){` |
|      ! 0 | 3369 | `		return 0;   /* php: "Illegal member variable name" */` |
|        - | 3370 | `	}` |
|      163 | 3371 | `	nCls = 0;` |
|     1857 | 3372 | `	while( nCls < nKey - 2 && zKey[1+nCls] != 0 ){` |
|     1695 | 3373 | `		nCls++;` |
|        1 | 3374 | `	}` |
|      163 | 3375 | `	if( nCls >= nKey - 2 ){` |
|      ! 0 | 3376 | `		return 0;   /* php: "Corrupt member variable name" */` |
|        - | 3377 | `	}` |
|        - | 3378 | `	/* An ANONYMOUS class mangles its source location in as a SECOND NUL-separated` |
|        - | 3379 | `	 * part, so the property name is what follows the LAST NUL rather than the` |
|        - | 3380 | `	 * second one (php's anonclass_src_len step). The class STRING php shows is` |
|        - | 3381 | `	 * still only the first part — it prints that one as a C string. */` |
|      163 | 3382 | `	nSrc = 0;` |
|     1119 | 3383 | `	while( nCls + 2 + nSrc < nKey && zKey[nCls+2+nSrc] != 0 ){` |
|      957 | 3384 | `		nSrc++;` |
|        1 | 3385 | `	}` |
|      163 | 3386 | `	SyStringInitFromBuf(pClass,&zKey[1],nCls);` |
|      163 | 3387 | `	if( nCls + nSrc + 2 != nKey ){` |
|      ! 0 | 3388 | `		nCls += nSrc + 1;` |
|      ! 0 | 3389 | `	}` |
|      163 | 3390 | `	SyStringInitFromBuf(pName,&zKey[nCls+2],nKey - nCls - 2);` |
|      163 | 3391 | `	return 1;` |
|      371 | 3392 | `}` |
|        - | 3393 | `/*` |
|        - | 3394 | `` * Emit a property's dump key: var_dump `["x"]=>` / `["p":"C":private]=>` /`` |
|        - | 3395 | `` * `["q":protected]=>`; print_r `[x] => ` / `[p:C:private] => ` /`` |
|        - | 3396 | `` * `[q:protected] => ` (php's exact annotations).`` |
|        - | 3397 | ` */` |
|      488 | 3398 | `static void OoDumpPropKey(SyBlob *pOut,ph7_class_instance *pThis,ph7_class_attr *pAttr,int ShowType)` |
|        5 | 3399 | `{` |
|      493 | 3400 | `	const char *zQ = ShowType ? "\"" : "";` |
|      493 | 3401 | `	if( SyStringLength(&pAttr->sName) > 0 && SyStringData(&pAttr->sName)[0] == 0 ){` |
|        - | 3402 | `		/* A MANGLED name stored raw — the __PHP_Incomplete_Class carrier's` |
|        - | 3403 | `		 * private/protected payload keys. php's dump unmangles them exactly as` |
|        - | 3404 | ``		 * it does an (array) cast's: `["bp":"PB":private]` / `["pp":protected]`. */`` |
|        - | 3405 | `		SyString sUnmCls, sUnmName;` |
|        9 | 3406 | `		if( PH7_UnmangleAttrName(SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),` |
|        - | 3407 | `			&sUnmCls,&sUnmName) ){` |
|        9 | 3408 | `			SyBlobFormat(&(*pOut),"[%s%z%s",zQ,&sUnmName,zQ);` |
|        9 | 3409 | `			if( sUnmCls.nByte == 1 && sUnmCls.zString[0] == '*' ){` |
|        5 | 3410 | `				SyBlobAppend(&(*pOut),":protected",sizeof(":protected")-1);` |
|        3 | 3411 | `			}else{` |
|        5 | 3412 | `				SyBlobFormat(&(*pOut),":%s%z%s:private",zQ,&sUnmCls,zQ);` |
|        - | 3413 | `			}` |
|        9 | 3414 | `			SyBlobAppend(&(*pOut),ShowType ? "]=>" : "] => ",ShowType ? 3 : 5);` |
|        9 | 3415 | `			return;` |
|        - | 3416 | `		}` |
|      ! 0 | 3417 | `	}` |
|      485 | 3418 | `	SyBlobFormat(&(*pOut),"[%s%z%s",zQ,&pAttr->sName,zQ);` |
|      485 | 3419 | `	if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       56 | 3420 | `		ph7_class *pDecl = OoAttrDeclaringClass(pThis->pClass,pAttr);` |
|       56 | 3421 | `		SyBlobFormat(&(*pOut),":%s%z%s:private",zQ,&pDecl->sDisp,zQ);` |
|      458 | 3422 | `	}else if( pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
|       64 | 3423 | `		SyBlobAppend(&(*pOut),":protected",sizeof(":protected")-1);` |
|       31 | 3424 | `	}` |
|      485 | 3425 | `	SyBlobAppend(&(*pOut),ShowType ? "]=>" : "] => ",ShowType ? 3 : 5);` |
|      249 | 3426 | `}` |
|        - | 3427 | `/*` |
|        - | 3428 | `` * Is this property's value a REFERENCE? -- php's `Z_ISREF_P`, asked of a property.`` |
|        - | 3429 | ` *` |
|        - | 3430 | `` * Two things turn on it. var_dump prints `&` for a property that IS one -- either end`` |
|        - | 3431 | `` * of the bind, `$o->p =& $x` and `$r =& $o->p` alike -- exactly as the array renderer`` |
|        - | 3432 | ` * marks an element something else holds (PH7_HashmapNodeIsRef). A property differs` |
|        - | 3433 | ` * only in the THRESHOLD: the property itself is not always one of the holders the` |
|        - | 3434 | ` * reference table names -- a DECLARED property holds nothing, a dynamic or re-created` |
|        - | 3435 | ` * one holds a permanent pin, and a bound one holds a counted pin -- so the threshold` |
|        - | 3436 | ` * is that one hold rather than the element renderer's flat two. print_r marks nothing,` |
|        - | 3437 | ` * in either container.` |
|        - | 3438 | ` *` |
|        - | 3439 | ` * The second is what an ARRAY built out of the property table carries:` |
|        - | 3440 | `` * `get_object_vars()`, the `(array)` cast, `get_mangled_object_vars()` and the SPL`` |
|        - | 3441 | ` * storage built from an object hand out the property's own SLOT for a property that` |
|        - | 3442 | ` * is a reference, so a write through the element reaches the object. Everything else` |
|        - | 3443 | ` * stays the copy it has always been.` |
|        - | 3444 | ` */` |
|     2340 | 3445 | `PH7_PRIVATE int PH7_ClassAttrIsRef(ph7_class_instance *pThis,VmClassAttr *pVmAttr)` |
|        5 | 3446 | `{` |
|        - | 3447 | `	sxu32 nSelf;` |
|     2345 | 3448 | `	if( pVmAttr == 0 \|\| pVmAttr->nIdx == SXU32_HIGH ){` |
|      ! 0 | 3449 | `		return 0;` |
|        - | 3450 | `	}` |
|     2345 | 3451 | `	nSelf = PH7_VmSlotSelfPinned(pThis->pVm,pVmAttr->nIdx) ? 1 : 0;` |
|     2345 | 3452 | `	return PH7_VmSlotHolderCount(pThis->pVm,pVmAttr->nIdx) > nSelf;` |
|     1175 | 3453 | `}` |
|      496 | 3454 | `PH7_PRIVATE sxi32 PH7_ClassInstanceDump(SyBlob *pOut,ph7_class_instance *pThis,int ShowType,int nTab,int nDepth)` |
|        5 | 3455 | `{` |
|        - | 3456 | `	SyHashEntry *pEntry;` |
|        - | 3457 | `	ph7_value *pValue;` |
|        - | 3458 | `	sxi32 rc;` |
|        - | 3459 | `	int i;` |
|      501 | 3460 | `	if( nDepth > PH7_DUMP_MAX_DEPTH ){` |
|        - | 3461 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|        - | 3462 | `		/* Nesting limit reached..halt immediately*/` |
|      ! 0 | 3463 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|      ! 0 | 3464 | `		return SXERR_LIMIT;` |
|        - | 3465 | `	}` |
|      501 | 3466 | `	if( pThis->iFlags & VM_INSTANCE_DUMPING ){` |
|        - | 3467 | `		/* php's *RECURSION*: this instance is one the walk is already inside.` |
|        - | 3468 | `		 * print_r prints the header and then the marker in place of the body,` |
|        - | 3469 | `		 * and the entry line the caller is writing supplies the newline. Only` |
|        - | 3470 | `		 * print_r arrives here marked -- var_dump's marker replaces the header` |
|        - | 3471 | `		 * too, so PH7_MemObjDump (the only caller) never descends. */` |
|        7 | 3472 | `		if( !ShowType ){` |
|        7 | 3473 | `			DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,FALSE,0);` |
|        7 | 3474 | `			SyBlobAppend(&(*pOut)," *RECURSION*",sizeof(" *RECURSION*")-1);` |
|        3 | 3475 | `		}` |
|        7 | 3476 | `		return SXRET_OK;` |
|        - | 3477 | `	}` |
|      495 | 3478 | `	pThis->iFlags \|= VM_INSTANCE_DUMPING;` |
|      495 | 3479 | `	rc = SXRET_OK;` |
|        - | 3480 | `	{` |
|        - | 3481 | `		/* A native class's PRESENTATION (php's get_debug_info): the shape it shows` |
|        - | 3482 | `		 * is not its storage — a DateTime shows date/timezone_type/timezone, a` |
|        - | 3483 | `		 * WeakReference shows ["object"] — and the slots underneath are hidden.` |
|        - | 3484 | `		 * Rendered exactly like a __debugInfo() array, which is what php does with` |
|        - | 3485 | `		 * it, and consulted first because php's handler wins over a userland` |
|        - | 3486 | `		 * method a native class cannot declare anyway. */` |
|        - | 3487 | `		ph7_value sPresent;` |
|      495 | 3488 | `		PH7_MemObjInit(pThis->pVm,&sPresent);` |
|      495 | 3489 | `		if( ph7_value_is_array(&sPresent) == 0 ){` |
|      495 | 3490 | `			ph7_hashmap *pPresent = PH7_NewHashmap(pThis->pVm,0,0);` |
|      495 | 3491 | `			if( pPresent ){` |
|      495 | 3492 | `				sPresent.x.pOther = pPresent;` |
|      495 | 3493 | `				MemObjSetType(&sPresent,MEMOBJ_HASHMAP);` |
|      245 | 3494 | `			}` |
|      245 | 3495 | `		}` |
|      490 | 3496 | `		if( (sPresent.iFlags & MEMOBJ_HASHMAP)` |
|      495 | 3497 | `		 && PH7_ClassInstancePresent(pThis,&sPresent,1) ){` |
|      159 | 3498 | `			ph7_hashmap *pMap = (ph7_hashmap *)sPresent.x.pOther;` |
|      159 | 3499 | `			DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,pMap->nEntry);` |
|      159 | 3500 | `			if( !ShowType ){` |
|       85 | 3501 | `				for( i = 0 ; i < nTab ; i++ ){` |
|      ! 0 | 3502 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      ! 0 | 3503 | `				}` |
|       85 | 3504 | `				SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       41 | 3505 | `			}` |
|      159 | 3506 | `			rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth,1);` |
|      159 | 3507 | `			for( i = 0 ; i < nTab ; i++ ){` |
|      ! 0 | 3508 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      ! 0 | 3509 | `			}` |
|      159 | 3510 | `			if( ShowType ){` |
|       75 | 3511 | `				SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|       38 | 3512 | `			}else{` |
|       85 | 3513 | `				SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        - | 3514 | `			}` |
|      159 | 3515 | `			PH7_MemObjRelease(&sPresent);` |
|      159 | 3516 | `			pThis->iFlags &= ~VM_INSTANCE_DUMPING;` |
|      159 | 3517 | `			return rc;` |
|        - | 3518 | `		}` |
|      339 | 3519 | `		PH7_MemObjRelease(&sPresent);` |
|        - | 3520 | `	}` |
|        - | 3521 | `	{` |
|        - | 3522 | `		/* Both var_dump and print_r consult __debugInfo() (PHP behavior);` |
|        - | 3523 | `		 * var_export uses a separate renderer and never reaches here. When the` |
|        - | 3524 | `		 * method is present and returns an array, render that array's entries as` |
|        - | 3525 | `		 * the object body, with the header showing the debug array's count. The` |
|        - | 3526 | `		 * nDepth guard above protects against a __debugInfo returning the object` |
|        - | 3527 | `		 * itself. */` |
|      339 | 3528 | `		ph7_class_method *pDbg = PH7_ClassExtractMethod(pThis->pClass,"__debugInfo",sizeof("__debugInfo")-1);` |
|      339 | 3529 | `		if( pDbg ){` |
|        - | 3530 | `			ph7_value sResult;` |
|       19 | 3531 | `			PH7_MemObjInit(pThis->pVm,&sResult);` |
|       19 | 3532 | `			PH7_VmCallMagicMethod(pThis->pVm,pThis,pDbg,&sResult,0,0);` |
|       19 | 3533 | `			if( sResult.iFlags & MEMOBJ_HASHMAP ){` |
|       19 | 3534 | `				ph7_hashmap *pMap = (ph7_hashmap *)sResult.x.pOther;` |
|        - | 3535 | `				/* Header count is the debug array's entry count. */` |
|       19 | 3536 | `				DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,pMap->nEntry);` |
|       19 | 3537 | `				if( !ShowType ){` |
|        8 | 3538 | `					for( i = 0 ; i < nTab ; i++ ){` |
|      ! 0 | 3539 | `						SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      ! 0 | 3540 | `					}` |
|        8 | 3541 | `					SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|        3 | 3542 | `				}` |
|       19 | 3543 | `				rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth,1);` |
|       19 | 3544 | `				for( i = 0 ; i < nTab ; i++ ){` |
|      ! 0 | 3545 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      ! 0 | 3546 | `				}` |
|       19 | 3547 | `				if( ShowType ){` |
|       13 | 3548 | `					SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|        8 | 3549 | `				}else{` |
|        8 | 3550 | `					SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        - | 3551 | `				}` |
|       19 | 3552 | `				PH7_MemObjRelease(&sResult);` |
|       19 | 3553 | `				pThis->iFlags &= ~VM_INSTANCE_DUMPING;` |
|       19 | 3554 | `				return rc;` |
|        - | 3555 | `			}` |
|        - | 3556 | `			/* Non-array return: behave as if __debugInfo were absent. */` |
|      ! 0 | 3557 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 | 3558 | `		}` |
|        - | 3559 | `	}` |
|        - | 3560 | `	{` |
|        - | 3561 | `		/* var_dump's header needs the property count up front, so pre-count the` |
|        - | 3562 | `		 * non-static/non-constant attributes (matching the dump loop below).` |
|        - | 3563 | `		 * An UNINITIALIZED typed property is still LISTED by var_dump — with php's` |
|        - | 3564 | ``		 * `uninitialized(T)` marker in place of a value — but it does not COUNT,`` |
|        - | 3565 | `` 		 * which is how php's `object(C)#1 (0) { ["a"]=> uninitialized(int) }` `` |
|        - | 3566 | `		 * reads. */` |
|      323 | 3567 | `		sxu32 nProp = 0;` |
|      323 | 3568 | `		if( ShowType ){` |
|      259 | 3569 | `			SyHashResetLoopCursor(&pThis->hAttr);` |
|      677 | 3570 | `			while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|      423 | 3571 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      418 | 3572 | `				if( !PH7_ATTR_UNPRESENTED(pVmAttr)` |
|      394 | 3573 | `				 && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL) == 0` |
|      399 | 3574 | `				 && !PH7_ClassAttrUninitialized(pVmAttr) ){` |
|      361 | 3575 | `					nProp++;` |
|      178 | 3576 | `				}` |
|        5 | 3577 | `			}` |
|      127 | 3578 | `		}` |
|      323 | 3579 | `		DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,nProp);` |
|        - | 3580 | `	}` |
|      323 | 3581 | `	if( !ShowType ){` |
|        - | 3582 | `		/* print_r body opener: '(' at the container indent */` |
|      197 | 3583 | `		for( i = 0 ; i < nTab ; i++ ){` |
|      132 | 3584 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       68 | 3585 | `		}` |
|       69 | 3586 | `		SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       32 | 3587 | `	}` |
|        - | 3588 | `	/* Dump object attributes (php 8.4: VIRTUAL hooked properties have no` |
|        - | 3589 | `	 * backing store — excluded from var_dump/print_r) */` |
|      323 | 3590 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      881 | 3591 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|      563 | 3592 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      558 | 3593 | `		if( !PH7_ATTR_UNPRESENTED(pVmAttr)` |
|      517 | 3594 | `		 && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL) == 0 ){` |
|      517 | 3595 | `			if( PH7_ClassAttrUninitialized(pVmAttr) ){` |
|        - | 3596 | `				/* var_dump names the property and prints php's marker in place of` |
|        - | 3597 | `				 * the value it has not got; print_r has no such marker and leaves` |
|        - | 3598 | `				 * the property out entirely. */` |
|       65 | 3599 | `				if( ShowType ){` |
|        - | 3600 | `					char zType[192];` |
|       60 | 3601 | `					const char *zText = VmHintTextResolved(pThis->pVm,&pVmAttr->pAttr->sTypeName,` |
|       38 | 3602 | `						VmHintScopeDeclared(pVmAttr->pAttr->pDeclClass),` |
|       19 | 3603 | `						zType,sizeof(zType));` |
|      117 | 3604 | `					for( i = 0 ; i < nTab + 2 ; i++ ){` |
|       79 | 3605 | `						SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       41 | 3606 | `					}` |
|       41 | 3607 | `					OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,TRUE);` |
|       41 | 3608 | `					SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      117 | 3609 | `					for( i = 0 ; i < nTab + 2 ; i++ ){` |
|       79 | 3610 | `						SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       41 | 3611 | `					}` |
|       41 | 3612 | `					SyBlobFormat(&(*pOut),"uninitialized(%s)\n",zText);` |
|       19 | 3613 | `				}` |
|       65 | 3614 | `				continue;` |
|        - | 3615 | `			}` |
|        - | 3616 | `			/* Dump non-static/constant attribute only */` |
|      455 | 3617 | `			pValue = ExtractClassAttrValue(pThis->pVm,pVmAttr);` |
|      455 | 3618 | `			if( pValue == 0 ){` |
|      ! 0 | 3619 | `				continue;` |
|        - | 3620 | `			}` |
|      455 | 3621 | `			if( ShowType ){` |
|        - | 3622 | ``				/* var_dump prop: `["x"(:…)]=>` at nTab+2, the value on the next`` |
|        - | 3623 | `				 * line at the same indent (php). */` |
|     6165 | 3624 | `				for( i = 0 ; i < nTab + 2 ; i++ ){` |
|     5809 | 3625 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     2907 | 3626 | `				}` |
|      361 | 3627 | `				OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,TRUE);` |
|      361 | 3628 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      539 | 3629 | `				rc = PH7_MemObjDump(&(*pOut),pValue,TRUE,nTab+2,nDepth,` |
|      178 | 3630 | `					PH7_ClassAttrIsRef(pThis,pVmAttr));` |
|      361 | 3631 | `				if( rc == SXERR_LIMIT ){` |
|      ! 0 | 3632 | `					break;` |
|        - | 3633 | `				}` |
|      183 | 3634 | `			}else{` |
|        - | 3635 | ``				/* print_r prop: `[x(:…)] => value` at nTab+4; container values`` |
|        - | 3636 | `				 * render their block at nTab+8 followed by php's blank line. */` |
|      585 | 3637 | `				for( i = 0 ; i < nTab + 4 ; i++ ){` |
|      491 | 3638 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      247 | 3639 | `				}` |
|       97 | 3640 | `				OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,FALSE);` |
|       94 | 3641 | `				if( (pValue->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ))` |
|       55 | 3642 | `				 && (pValue->iFlags & MEMOBJ_NULL) == 0 ){` |
|       12 | 3643 | `					rc = PH7_MemObjDump(&(*pOut),pValue,FALSE,nTab+8,nDepth,0);` |
|       12 | 3644 | `					SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       12 | 3645 | `					if( rc == SXERR_LIMIT ){` |
|      ! 0 | 3646 | `						break;` |
|        - | 3647 | `					}` |
|        7 | 3648 | `				}else{` |
|       87 | 3649 | `					PH7_MemObjPrintRInline(&(*pOut),pValue);` |
|       87 | 3650 | `					SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        - | 3651 | `				}` |
|        - | 3652 | `			}` |
|      225 | 3653 | `		}` |
|        5 | 3654 | `	}` |
|     5531 | 3655 | `	for( i = 0 ; i < nTab ; i++ ){` |
|     5212 | 3656 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     2608 | 3657 | `	}` |
|      323 | 3658 | `	if( ShowType ){` |
|      259 | 3659 | `		SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      132 | 3660 | `	}else{` |
|       69 | 3661 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        - | 3662 | `	}` |
|      323 | 3663 | `	pThis->iFlags &= ~VM_INSTANCE_DUMPING;` |
|      323 | 3664 | `	return rc;` |
|      253 | 3665 | `}` |
|        - | 3666 | `/*` |
|        - | 3667 | ` * Call a magic method [i.e: __toString(),__toBool(),__Invoke()...]` |
|        - | 3668 | ` * Return SXRET_OK on successfull call. Any other return value indicates failure.` |
|        - | 3669 | ` * Notes on magic methods.` |
|        - | 3670 | ` * According to the PHP language reference manual.` |
|        - | 3671 | ` *  The function names __construct(), __destruct(), __call(), __callStatic()` |
|        - | 3672 | ` *  __get(),  __toString(), __invoke(), __clone() are magical in PHP classes.` |
|        - | 3673 | ` * You cannot have functions with these names in any of your classes unless` |
|        - | 3674 | ` * you want the magic functionality associated with them.` |
|        - | 3675 | ` * Example of magical methods:` |
|        - | 3676 | ` * __toString()` |
|        - | 3677 | ` *  The __toString() method allows a class to decide how it will react when it is treated like` |
|        - | 3678 | ` *  a string. For example, what echo $obj; will print. This method must return a string.` |
|        - | 3679 | ` *  Example #2 Simple example` |
|        - | 3680 | ` * <?php` |
|        - | 3681 | ` * // Declare a simple class` |
|        - | 3682 | ` * class TestClass` |
|        - | 3683 | ` * {` |
|        - | 3684 | ` *   public $foo;` |
|        - | 3685 | ` *` |
|        - | 3686 | ` *   public function __construct($foo)` |
|        - | 3687 | ` *   {` |
|        - | 3688 | ` *       $this->foo = $foo;` |
|        - | 3689 | ` *   }` |
|        - | 3690 | ` *` |
|        - | 3691 | ` *   public function __toString()` |
|        - | 3692 | ` *   {` |
|        - | 3693 | ` *       return $this->foo;` |
|        - | 3694 | ` *   }` |
|        - | 3695 | ` * }` |
|        - | 3696 | ` * $class = new TestClass('Hello');` |
|        - | 3697 | ` * echo $class;` |
|        - | 3698 | ` * ?>` |
|        - | 3699 | ` * The above example will output:` |
|        - | 3700 | ` *  Hello` |
|        - | 3701 | ` *` |
|        - | 3702 | ` * Note that PH7 does not support all the magical method and introudces __toFloat(),__toInt()` |
|        - | 3703 | ` * which have the same behaviour as __toString() but for float and integer types` |
|        - | 3704 | ` * respectively.` |
|        - | 3705 | ` * Refer to the official documentation for more information.` |
|        - | 3706 | ` */` |
|      256 | 3707 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallMagicMethod(` |
|        - | 3708 | `	ph7_vm *pVm,               /* VM that own all this stuff */` |
|        - | 3709 | `	ph7_class *pClass,         /* Target class */` |
|        - | 3710 | `	ph7_class_instance *pThis, /* Target object */` |
|        - | 3711 | `	const char *zMethod,       /* Magic method name [i.e: __toString()]*/` |
|        - | 3712 | `	sxu32 nByte,               /* zMethod length*/` |
|        - | 3713 | `	const SyString *pAttrName, /* Attribute name */` |
|        - | 3714 | `	ph7_value *pResult         /* OUT: magic method return value. NULL to discard */` |
|        - | 3715 | `	)` |
|        4 | 3716 | `{` |
|      260 | 3717 | `	ph7_value *apArg[2] = { 0 , 0 };` |
|        - | 3718 | `	ph7_class_method *pMeth;` |
|        - | 3719 | `	ph7_value sAttr; /* cc warning */` |
|        - | 3720 | `	sxi32 rc;` |
|        - | 3721 | `	int nArg;` |
|      260 | 3722 | `	int bMagicGet = nByte == sizeof("__get")-1 && SyMemcmp(zMethod,"__get",nByte) == 0;` |
|      260 | 3723 | `	int bMagicIsset = nByte == sizeof("__isset")-1 && SyMemcmp(zMethod,"__isset",nByte) == 0;` |
|      256 | 3724 | `	if( (bMagicGet \|\| bMagicIsset) && pAttrName` |
|      255 | 3725 | `	 && PH7_ClassNativePropOwns(pThis,pAttrName) ){` |
|        - | 3726 | `		/* php's read_property / has_property handler for a name the class's own` |
|        - | 3727 | `		 * table carries: it answers before the standard path ever looks for a` |
|        - | 3728 | ``		 * magic accessor, so a subclass's `__get` does not shadow ext/dom's`` |
|        - | 3729 | ``		 * surface. The read-modify-write and `??=` rails reach the handler here;`` |
|        - | 3730 | `		 * the member opcode's own read gate asks it a step earlier. */` |
|        - | 3731 | `		PH7_NativePropCtx sNat;` |
|        - | 3732 | `		ph7_value sNatVal;` |
|        7 | 3733 | `		PH7_MemObjInit(pVm,&sNatVal);` |
|       10 | 3734 | `		if( PH7_ClassNativePropAsk(pThis,&sNat,` |
|        3 | 3735 | `				bMagicIsset ? PH7_NATIVE_PROP_ISSET : PH7_NATIVE_PROP_READ,pAttrName,&sNatVal) ){` |
|        7 | 3736 | `			if( sNat.zThrowClass ){` |
|      ! 0 | 3737 | `				VmBoundaryPark(pVm,VmThrowFixedErrorCode(pVm,sNat.zThrowClass,` |
|      ! 0 | 3738 | `					sNat.iThrowCode,sNat.zThrowMsg));` |
|        7 | 3739 | `			}else if( pResult ){` |
|        7 | 3740 | `				PH7_MemObjStore(&sNatVal,pResult);` |
|        3 | 3741 | `			}` |
|        7 | 3742 | `			PH7_MemObjRelease(&sNatVal);` |
|        7 | 3743 | `			return SXRET_OK;` |
|        - | 3744 | `		}` |
|      ! 0 | 3745 | `		PH7_MemObjRelease(&sNatVal);` |
|      ! 0 | 3746 | `	}` |
|        - | 3747 | `	/* Make sure the magic method is available */` |
|      254 | 3748 | `	pMeth = PH7_ClassExtractMethod(&(*pClass),zMethod,nByte);` |
|      254 | 3749 | `	if( pMeth == 0 ){` |
|        - | 3750 | `		/* No such method,return immediately */` |
|      ! 0 | 3751 | `		return SXERR_NOTFOUND;` |
|        - | 3752 | `	}` |
|      254 | 3753 | `	nArg = 0;` |
|        - | 3754 | `	/* Copy arguments */` |
|      254 | 3755 | `	if( pAttrName ){` |
|      254 | 3756 | `		PH7_MemObjInitFromString(pVm,&sAttr,pAttrName);` |
|      254 | 3757 | `		sAttr.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      254 | 3758 | `		apArg[0] = &sAttr;` |
|      254 | 3759 | `		nArg = 1;` |
|      125 | 3760 | `	}` |
|        - | 3761 | `	/* Call the magic method now */` |
|      254 | 3762 | `	rc = PH7_VmCallMagicMethod(pVm,&(*pThis),pMeth,pResult,nArg,apArg);` |
|        - | 3763 | `	/* Clean up */` |
|      254 | 3764 | `	if( pAttrName ){` |
|      254 | 3765 | `		PH7_MemObjRelease(&sAttr);` |
|      125 | 3766 | `	}` |
|      254 | 3767 | `	return rc;` |
|      132 | 3768 | `}` |
|        - | 3769 | `/*` |
|        - | 3770 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon].` |
|        - | 3771 | ` * This function is simply a wrapper on ExtractClassAttrValue().` |
|        - | 3772 | ` */` |
|  5866250 | 3773 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceExtractAttrValue(ph7_class_instance *pThis,VmClassAttr *pAttr)` |
|        5 | 3774 | `{` |
|        - | 3775 | `   /* Extract the attribute value */` |
|        - | 3776 | `	ph7_value *pValue;` |
|  5866255 | 3777 | `	pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|  5866255 | 3778 | `	return pValue;` |
|        5 | 3779 | `}` |
|        - | 3780 | `/*` |
|        - | 3781 | ` * Convert a class instance [i.e: Object in the PHP jargon] into a hashmap [i.e: array in the PHP jargon].` |
|        - | 3782 | ` * Return SXRET_OK on success. Any other value indicates failure.` |
|        - | 3783 | ` * Note on object conversion to array:` |
|        - | 3784 | ` *  Acccording to the PHP language reference manual` |
|        - | 3785 | ` *  If an object is converted to an array, the result is an array whose elements are the object's properties.` |
|        - | 3786 | ` *  The keys are the member variable names.` |
|        - | 3787 | ` *` |
|        - | 3788 | ` *  The following example:` |
|        - | 3789 | ` *  class Test {` |
|        - | 3790 | ` *   public $A = 25<<1;  // 50` |
|        - | 3791 | ` *	 public $c = rand_str(3);   // Random string of length 3` |
|        - | 3792 | ` *	 public $d = rand() & 1023; // Random number between 0..1023` |
|        - | 3793 | ` *  }` |
|        - | 3794 | ` *  var_dump((array) new Test());` |
|        - | 3795 | ` *	Will output:` |
|        - | 3796 | ` *  array(3) {` |
|        - | 3797 | ` *   [A] =>` |
|        - | 3798 | ` *      int(50)` |
|        - | 3799 | ` *   [c] =>` |
|        - | 3800 | ` *     string(3 'aps')` |
|        - | 3801 | ` *   [d] =>` |
|        - | 3802 | ` *     int(991)` |
|        - | 3803 | ` *  }` |
|        - | 3804 | ` * You have noticed that PH7 allow class attributes [i.e: $a,$c,$d in the example above]` |
|        - | 3805 | ` * have any complex expression (even function calls/Annonymous functions) as their default` |
|        - | 3806 | ` * value unlike the standard PHP engine.` |
|        - | 3807 | ` * This is a very powerful feature that you have to look at.` |
|        - | 3808 | ` */` |
|      846 | 3809 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|        5 | 3810 | `{` |
|        - | 3811 | `	{` |
|        - | 3812 | `		/* php's get_properties handler, which is what the (array) cast reads: a` |
|        - | 3813 | `		 * DateTime casts to date/timezone_type/timezone, not to the hidden slots` |
|        - | 3814 | `		 * holding its timestamp. A class whose hook answers only the DEBUG surface` |
|        - | 3815 | `		 * (WeakReference) fills nothing here, and that EMPTY answer is php's — the` |
|        - | 3816 | `		 * hook is authoritative, so there is no falling back to the slot walk. It` |
|        - | 3817 | `		 * used to fall back when the hook filled nothing, which was invisible while` |
|        - | 3818 | `		 * every hooked class also had no visible property: an ArrayObject SUBCLASS` |
|        - | 3819 | ``		 * with an empty storage would have cast to its own `p` where php casts to`` |
|        - | 3820 | `		 * the (empty) storage. */` |
|        - | 3821 | `		ph7_value sPresent;` |
|      851 | 3822 | `		PH7_MemObjInit(pThis->pVm,&sPresent);` |
|      851 | 3823 | `		sPresent.x.pOther = pMap;` |
|      851 | 3824 | `		MemObjSetType(&sPresent,MEMOBJ_HASHMAP);` |
|      851 | 3825 | `		if( PH7_ClassInstancePresent(pThis,&sPresent,0) ){` |
|        - | 3826 | `			/* The map IS the destination; do not release the carrier's hashmap. */` |
|      677 | 3827 | `			sPresent.x.pOther = 0;` |
|      677 | 3828 | `			sPresent.iFlags = MEMOBJ_NULL;` |
|      677 | 3829 | `			return SXRET_OK;` |
|        - | 3830 | `		}` |
|      177 | 3831 | `		sPresent.x.pOther = 0;` |
|      177 | 3832 | `		sPresent.iFlags = MEMOBJ_NULL;` |
|      177 | 3833 | `		PH7_MemObjRelease(&sPresent);` |
|        - | 3834 | `	}` |
|      177 | 3835 | `	return PH7_ClassInstanceToHashmapRaw(pThis,pMap);` |
|      428 | 3836 | `}` |
|        - | 3837 | `/*` |
|        - | 3838 | ` * Is this property NOT THERE YET?` |
|        - | 3839 | ` *` |
|        - | 3840 | ` * php 7.4's typed properties have a third state beside "holds a value" and "does not` |
|        - | 3841 | `` * exist": a typed property with no default is UNINITIALIZED at `new`, reading it is an`` |
|        - | 3842 | ` * Error rather than a null, and every surface that presents an object's properties` |
|        - | 3843 | ` * leaves it out — get_object_vars(), get_mangled_object_vars(), the (array) cast,` |
|        - | 3844 | ` * foreach, json_encode(), serialize(), print_r() and var_export(). var_dump() is the one` |
|        - | 3845 | ``  * exception and only half of one: it NAMES the property and prints `uninitialized(T)` `` |
|        - | 3846 | ` * where the value would be, and does not count it in the header.` |
|        - | 3847 | ` *` |
|        - | 3848 | ` * The state is already tracked (it is what makes the read throw); this asks it by name` |
|        - | 3849 | ` * so the eight surfaces agree. VM_CLASS_ATTR_UNINIT doubles as the readonly write-once` |
|        - | 3850 | ` * latch, which wants the same answer: a readonly property must be typed and cannot have` |
|        - | 3851 | ` * a default, so before its first write php calls it uninitialized too.` |
|        - | 3852 | ` */` |
|     4584 | 3853 | `PH7_PRIVATE int PH7_ClassAttrUninitialized(VmClassAttr *pVmAttr)` |
|        5 | 3854 | `{` |
|     4589 | 3855 | `	return pVmAttr != 0 && (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) != 0;` |
|        5 | 3856 | `}` |
|        - | 3857 | `/*` |
|        - | 3858 | `` * The same question asked by the four surfaces that read through a php 8.4 `get` HOOK —`` |
|        - | 3859 | ` * get_object_vars(), json_encode(), foreach and var_export().` |
|        - | 3860 | ` *` |
|        - | 3861 | ` * A hooked property's value is whatever its hook answers, so an empty backing slot is` |
|        - | 3862 | ` * not an absence there: a VIRTUAL property has no slot at all and still has a value` |
|        - | 3863 | `` * (php lists `virt` in all four), and a BACKED one whose hook reads its own slot raises`` |
|        - | 3864 | ` * the uninitialized Error from inside the hook — which is php's answer for these four` |
|        - | 3865 | ``  * and not something to pre-empt by skipping the property. Only a property with no `get` `` |
|        - | 3866 | ` * at all is absent.` |
|        - | 3867 | ` */` |
|     1474 | 3868 | `PH7_PRIVATE int PH7_ClassAttrUninitializedForRead(VmClassAttr *pVmAttr)` |
|        5 | 3869 | `{` |
|     1547 | 3870 | `	return PH7_ClassAttrUninitialized(pVmAttr)` |
|     1474 | 3871 | `		&& (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET) == 0;` |
|        5 | 3872 | `}` |
|        - | 3873 | `/*` |
|        - | 3874 | ` * The SLOT walk under the cast above, with php's get_properties handler left out:` |
|        - | 3875 | ` * the instance's own property table, mangled, and nothing else.` |
|        - | 3876 | ` *` |
|        - | 3877 | `` * This is what `get_mangled_object_vars()` answers — php asks the same handler with a`` |
|        - | 3878 | ` * different PURPOSE there, and every class here that has a handler answers the raw` |
|        - | 3879 | `` * table for it: `get_mangled_object_vars(new ArrayObject([1,2]))` is the EMPTY array`` |
|        - | 3880 | `` * where the cast is `[1,2]`, and a DateTime's is empty where the cast has three keys.`` |
|        - | 3881 | ` * Since PHL's engine slots are hidden (and php's equivalents live outside the property` |
|        - | 3882 | ` * table entirely), dropping the handler is the whole difference.` |
|        - | 3883 | ` */` |
|     1088 | 3884 | `static sxi32 ClassInstanceToHashmapRaw(ph7_class_instance *pThis,ph7_hashmap *pMap,int bOwnOnly)` |
|        5 | 3885 | `{` |
|        - | 3886 | `	SyHashEntry *pEntry;` |
|        - | 3887 | `	VmClassAttr *pAttr;` |
|        - | 3888 | `	ph7_value *pValue;` |
|        - | 3889 | `	ph7_value sName;` |
|        - | 3890 | `	/* Reset the loop cursor */` |
|     1093 | 3891 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|     1093 | 3892 | `	PH7_MemObjInitFromString(pThis->pVm,&sName,0);` |
|     7185 | 3893 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|        - | 3894 | `		/* Point to the current attribute */` |
|     6097 | 3895 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|     6097 | 3896 | `		if( !PH7_ClassInstanceAttrPresented(pAttr) ){` |
|        - | 3897 | `			/* Not part of the raw table: a class-level member, a typed property` |
|        - | 3898 | `			 * never written, or a php 8.4 VIRTUAL hooked one. */` |
|     4649 | 3899 | `			continue;` |
|        - | 3900 | `		}` |
|     1453 | 3901 | `		if( bOwnOnly && (pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_NATIVE_SET` |
|        - | 3902 | `			\|PH7_CLASS_ATTR_NATIVE_VIRTUAL\|PH7_CLASS_ATTR_NATIVE_LAZY)) ){` |
|        - | 3903 | `			/* The STATE of a native class whose state happens to be public` |
|        - | 3904 | `			 * (DateInterval's ten, DatePeriod's seven): the caller is building the` |
|        - | 3905 | `			 * shape those belong to, and wants only what the OBJECT added to it. */` |
|      388 | 3906 | `			continue;` |
|        - | 3907 | `		}` |
|        - | 3908 | `		/* Extract attribute value */` |
|     1067 | 3909 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     1067 | 3910 | `		if( pValue ){` |
|     1067 | 3911 | `			PH7_ClassInstanceAttrKey(pThis,pAttr,&sName);` |
|        - | 3912 | `			/* Perform the insertion. An OWN-props walk laid beside a shape the` |
|        - | 3913 | `			 * caller already built ADDS rather than updates: php's` |
|        - | 3914 | `			 * add_common_properties is a zend_hash_add, so a subclass property` |
|        - | 3915 | `			 * named like one of the internal keys loses to the internal value` |
|        - | 3916 | ``			 * there (`class S extends DateTime { public $date; }` serializes the`` |
|        - | 3917 | `			 * DATE). */` |
|     1067 | 3918 | `			if( bOwnOnly ){` |
|      129 | 3919 | `				ph7_hashmap_node *pDup = 0;` |
|      129 | 3920 | `				if( PH7_HashmapLookup(pMap,&sName,&pDup) == SXRET_OK ){` |
|        3 | 3921 | `					SyBlobReset(&sName.sBlob);` |
|        3 | 3922 | `					continue;` |
|        - | 3923 | `				}` |
|       63 | 3924 | `			}` |
|     1065 | 3925 | `			if( PH7_ClassAttrIsRef(pThis,pAttr) ){` |
|        - | 3926 | `				/* php hands out the property's own REFERENCE, not a copy of what it` |
|        - | 3927 | ``				 * holds: `$v = (array)$o; $v['p'] = 9;` reaches the object when `p` is`` |
|        - | 3928 | ``				 * a reference, and `var_dump()` marks the element `&` in both places.`` |
|        - | 3929 | `				 * Only a property that IS one -- something else names its slot -- and` |
|        - | 3930 | `				 * never the ordinary copy every other element still takes. */` |
|       17 | 3931 | `				PH7_HashmapInsertByRef(pMap,&sName,pAttr->nIdx);` |
|        9 | 3932 | `			}else{` |
|     1049 | 3933 | `				PH7_HashmapInsert(pMap,&sName,pValue);` |
|        - | 3934 | `			}` |
|        - | 3935 | `			/* Reset the string cursor */` |
|     1065 | 3936 | `			SyBlobReset(&sName.sBlob);` |
|      530 | 3937 | `		}` |
|        5 | 3938 | `	}` |
|     1093 | 3939 | `	PH7_MemObjRelease(&sName);` |
|     1093 | 3940 | `	return SXRET_OK;` |
|        5 | 3941 | `}` |
|      284 | 3942 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmapRaw(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|        5 | 3943 | `{` |
|      289 | 3944 | `	return ClassInstanceToHashmapRaw(pThis,pMap,0);` |
|        5 | 3945 | `}` |
|        - | 3946 | `/*` |
|        - | 3947 | ` * The same walk, restricted to what the OBJECT added: a native class's own public` |
|        - | 3948 | ` * STATE is left out, so a subclass's properties can be laid beside the shape that` |
|        - | 3949 | ` * state builds rather than inside it. php's add_common_properties.` |
|        - | 3950 | ` */` |
|      804 | 3951 | `PH7_PRIVATE sxi32 PH7_ClassInstanceOwnPropsToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|        2 | 3952 | `{` |
|      806 | 3953 | `	return ClassInstanceToHashmapRaw(pThis,pMap,1);` |
|        2 | 3954 | `}` |
|        - | 3955 | `/*` |
|        - | 3956 | ` * Iterate throw class attributes and invoke the given callback [i.e: xWalk()] for each` |
|        - | 3957 | ` * retrieved attribute.` |
|        - | 3958 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|        - | 3959 | ` * the attribute value in the callback body will not alter the real attribute value.` |
|        - | 3960 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|        - | 3961 | ` * a value different from PH7_OK.` |
|        - | 3962 | ` * Refer to [ph7_object_walk()] for more information.` |
|        - | 3963 | ` */` |
|      ! 0 | 3964 | `PH7_PRIVATE sxi32 PH7_ClassInstanceWalk(` |
|        - | 3965 | `	ph7_class_instance *pThis, /* Target object */` |
|        - | 3966 | `	int (*xWalk)(const char *,ph7_value *,void *), /* Walker callback */` |
|        - | 3967 | `	void *pUserData /* Last argument to xWalk() */` |
|        - | 3968 | `	)` |
|      ! 0 | 3969 | `{` |
|        - | 3970 | `	SyHashEntry *pEntry; /* Hash entry */` |
|        - | 3971 | `	VmClassAttr *pAttr;  /* Pointer to the attribute */` |
|        - | 3972 | `	ph7_value *pValue;   /* Attribute value */` |
|        - | 3973 | `	ph7_value sValue;    /* Copy of the attribute value */` |
|        - | 3974 | `	int rc;` |
|        - | 3975 | `	/* Reset the loop cursor */` |
|      ! 0 | 3976 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      ! 0 | 3977 | `	PH7_MemObjInit(pThis->pVm,&sValue);` |
|        - | 3978 | `	/* Start the walk process */` |
|      ! 0 | 3979 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|        - | 3980 | `		/* Point to the current attribute */` |
|      ! 0 | 3981 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      ! 0 | 3982 | `		if( PH7_ATTR_UNPRESENTED(pAttr) ){` |
|        - | 3983 | `			/* Class-level members are not part of the object (php) */` |
|      ! 0 | 3984 | `			continue;` |
|        - | 3985 | `		}` |
|      ! 0 | 3986 | `		if( PH7_ClassAttrUninitialized(pAttr) ){` |
|      ! 0 | 3987 | `			continue; /* typed, never written: not there yet (php) */` |
|        - | 3988 | `		}` |
|        - | 3989 | `		/* Extract attribute value */` |
|      ! 0 | 3990 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|      ! 0 | 3991 | `		if( pValue ){` |
|      ! 0 | 3992 | `			PH7_MemObjLoad(pValue,&sValue);` |
|        - | 3993 | `			/* Invoke the supplied callback */` |
|      ! 0 | 3994 | `			rc =  xWalk(SyStringData(&pAttr->pAttr->sName),&sValue,pUserData);` |
|      ! 0 | 3995 | `			PH7_MemObjRelease(&sValue);` |
|      ! 0 | 3996 | `			if( rc != PH7_OK){` |
|        - | 3997 | `				/* User callback request an operation abort */` |
|      ! 0 | 3998 | `				return SXERR_ABORT;` |
|        - | 3999 | `			}` |
|      ! 0 | 4000 | `		}` |
|      ! 0 | 4001 | `	}` |
|        - | 4002 | `	/* All done */` |
|      ! 0 | 4003 | `	return SXRET_OK;` |
|      ! 0 | 4004 | `}` |
|        - | 4005 | `/*` |
|        - | 4006 | ` * The instance's attribute entry for a property name, INCLUDING the empty one.` |
|        - | 4007 | ` *` |
|        - | 4008 | ` * SyHashGet answers 0 for a zero-length key engine-wide, so a property named ""` |
|        - | 4009 | `` * — php's own `s:0:""` payload builds one, and every surface that walks hAttr`` |
|        - | 4010 | ` * shows it — can only be found by walking the list. Used by the presentation` |
|        - | 4011 | ` * surfaces that snapshot names and re-look-up each one before reading it (a PHP` |
|        - | 4012 | ` * 8.4 get hook may unset a property mid-walk), where the plain hash probe would` |
|        - | 4013 | ` * silently drop it from the output while var_dump, which walks directly, showed` |
|        - | 4014 | ` * it. The walk uses the hash's embedded loop cursor, so it must not run inside` |
|        - | 4015 | ` * another walk of the SAME table — the callers below re-enter only through the` |
|        - | 4016 | ` * hook dispatch, which happens after this returns.` |
|        - | 4017 | ` */` |
|      718 | 4018 | `PH7_PRIVATE SyHashEntry * PH7_ClassInstanceAttrEntry(ph7_class_instance *pThis,const char *zName,sxu32 nName)` |
|        5 | 4019 | `{` |
|        - | 4020 | `	SyHashEntry *pEntry;` |
|      723 | 4021 | `	if( nName > 0 ){` |
|      665 | 4022 | `		return SyHashGet(&pThis->hAttr,(const void *)zName,nName);` |
|        - | 4023 | `	}` |
|       59 | 4024 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      135 | 4025 | `	while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       91 | 4026 | `		if( pEntry->nKeyLen == 0 ){` |
|       15 | 4027 | `			return pEntry;` |
|        - | 4028 | `		}` |
|        1 | 4029 | `	}` |
|       45 | 4030 | `	return 0;` |
|      364 | 4031 | `}` |
|        - | 4032 | `/*` |
|        - | 4033 | ` * Extract a class atrribute value.` |
|        - | 4034 | ` * Return a pointer to the attribute value on success. Otherwise NULL.` |
|        - | 4035 | ` * Note:` |
|        - | 4036 | ` *  Access to static and constant attribute is not allowed. That is,the function` |
|        - | 4037 | ` *  will return NULL in case someone (host-application code) try to extract` |
|        - | 4038 | ` *  a static/constant attribute.` |
|        - | 4039 | ` */` |
|  2099890 | 4040 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceFetchAttr(ph7_class_instance *pThis,const SyString *pName)` |
|        5 | 4041 | `{` |
|        - | 4042 | `	SyHashEntry *pEntry;` |
|        - | 4043 | `	VmClassAttr *pAttr;` |
|        - | 4044 | `	/* Query the attribute hashtable */` |
|  2099895 | 4045 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);` |
|  2099895 | 4046 | `	if( pEntry == 0 ){` |
|        - | 4047 | `		/* No such attribute */` |
|     1111 | 4048 | `		return 0;` |
|        - | 4049 | `	}` |
|        - | 4050 | `	/* Point to the class atrribute */` |
|  2098789 | 4051 | `	pAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - | 4052 | `	/* Check if we are dealing with a static/constant attribute */` |
|  2098789 | 4053 | `	if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - | 4054 | `		/* Access is forbidden */` |
|      ! 0 | 4055 | `		return 0;` |
|        - | 4056 | `	}` |
|        - | 4057 | `	/* Return the attribute value */` |
|  2098789 | 4058 | `	return ExtractClassAttrValue(pThis->pVm,pAttr);` |
|  1049152 | 4059 | `}` |
|        - | 4060 | `/*` |
|        - | 4061 | `` * Does a WRITE through `$obj[k]` land on this class's storage? php answers with`` |
|        - | 4062 | ` * the class's read_dimension handler: an internal class whose own handler hands` |
|        - | 4063 | ` * back the real element supports indirect modification, while everything routed` |
|        - | 4064 | ` * through zend_std_read_dimension — every userland ArrayAccess, and the SPL` |
|        - | 4065 | ` * classes that keep the standard handler (SplFixedArray, the SplDoublyLinkedList` |
|        - | 4066 | ` * family, SplObjectStorage) — gets a TEMPORARY back, so the write is lost and php` |
|        - | 4067 | ` * says so. The engine cannot tell those apart from the interface alone: both` |
|        - | 4068 | ` * implement ArrayAccess.` |
|        - | 4069 | ` *` |
|        - | 4070 | ` * PHL's equivalent evidence is the offsetGet that ANSWERS: the native one on a` |
|        - | 4071 | ` * class carrying PH7_CLASS_DIM_WRITABLE means the storage is reachable, and a` |
|        - | 4072 | ` * userland OVERRIDE of it means it is not — which is php's own rule for an` |
|        - | 4073 | ` * ArrayObject subclass (spl_array_read_dimension steps aside for a declared` |
|        - | 4074 | `` * offsetGet). A `&offsetGet` return is php's other yes: it hands back a reference,`` |
|        - | 4075 | ` * so the write reaches whatever it aliases.` |
|        - | 4076 | ` */` |
|      536 | 4077 | `PH7_PRIVATE int PH7_VmDimFetchWritable(ph7_class *pClass)` |
|        5 | 4078 | `{` |
|        - | 4079 | `	ph7_class_method *pGet;` |
|        - | 4080 | `	ph7_class *pCur;` |
|      541 | 4081 | `	if( pClass == 0 ){` |
|      ! 0 | 4082 | `		return FALSE;` |
|        - | 4083 | `	}` |
|      541 | 4084 | `	pGet = PH7_ClassExtractMethod(pClass,"offsetGet",sizeof("offsetGet")-1);` |
|      541 | 4085 | `	if( pGet == 0 ){` |
|      ! 0 | 4086 | `		return FALSE;` |
|        - | 4087 | `	}` |
|      541 | 4088 | `	if( pGet->sFunc.iFlags & VM_FUNC_REF_RETURN ){` |
|        5 | 4089 | `		return TRUE;` |
|        - | 4090 | `	}` |
|      537 | 4091 | `	if( (pGet->sFunc.iFlags & VM_FUNC_NATIVE) == 0 ){` |
|      161 | 4092 | `		return FALSE;` |
|        - | 4093 | `	}` |
|      664 | 4094 | `	for( pCur = pClass ; pCur ; pCur = pCur->pBase ){` |
|      534 | 4095 | `		if( pCur->iFlags & PH7_CLASS_DIM_WRITABLE ){` |
|      249 | 4096 | `			return TRUE;` |
|        - | 4097 | `		}` |
|      145 | 4098 | `	}` |
|      133 | 4099 | `	return FALSE;` |
|      273 | 4100 | `}` |
|        - | 4101 | `/*` |
|        - | 4102 | ` * The three accessors a VM_FUNC_NATIVE method body uses to reach its receiver.` |
|        - | 4103 | ` *` |
|        - | 4104 | ` * A native method is dispatched down the host-function path, so it is handed the` |
|        - | 4105 | ` * plain (ph7_context*, argc, argv) of any builtin — the receiver rides on the` |
|        - | 4106 | ` * context instead of occupying an argument slot. That is the whole point: the C` |
|        - | 4107 | `` * routine that used to be a global `__prefix_verb($target, ...)` thunk taking its`` |
|        - | 4108 | ` * target explicitly becomes a method whose target is $this.` |
|        - | 4109 | ` */` |
|        - | 4110 | `/*` |
|        - | 4111 | ` * The receiver, or NULL when the method was called statically (and always NULL in` |
|        - | 4112 | ` * a plain host function). Borrowed: the operand stack holds the reference for the` |
|        - | 4113 | ` * duration of the call, so the body must not unref it.` |
|        - | 4114 | ` */` |
|  1601382 | 4115 | `PH7_PRIVATE ph7_class_instance * PH7_ContextThis(ph7_context *pCtx)` |
|        5 | 4116 | `{` |
|  1601387 | 4117 | `	return pCtx->pThis;` |
|        5 | 4118 | `}` |
|        - | 4119 | `/*` |
|        - | 4120 | ` * The class the call was made THROUGH — php's late-static-binding target, so an` |
|        - | 4121 | ` * inherited native method sees the SUBCLASS here, not the class that declared it.` |
|        - | 4122 | ` * NULL in a plain host function.` |
|        - | 4123 | ` */` |
|     1067 | 4124 | `PH7_PRIVATE ph7_class * PH7_ContextCalledClass(ph7_context *pCtx)` |
|        4 | 4125 | `{` |
|     1071 | 4126 | `	return pCtx->pCalledClass;` |
|        4 | 4127 | `}` |
|        - | 4128 | `/*` |
|        - | 4129 | ` * The receiver as a ph7_value, so the body can use the ordinary object helpers` |
|        - | 4130 | ` * (ph7_object_fetch_attr, ph7_value_is_object, ph7_result_value, ...) instead of` |
|        - | 4131 | ` * reaching into ph7_class_instance. NULL when there is no receiver.` |
|        - | 4132 | ` *` |
|        - | 4133 | ` * The view ALIASES the instance without bumping its refcount: it lives exactly as` |
|        - | 4134 | ` * long as the call, during which the caller's reference is already keeping the` |
|        - | 4135 | ` * object alive, and VmReleaseCallContext drops the alias without unreffing. Handing` |
|        - | 4136 | ` * it to ph7_result_value() is safe — that copies through PH7_MemObjStore, which` |
|        - | 4137 | ` * takes its own reference.` |
|        - | 4138 | ` */` |
|     9444 | 4139 | `PH7_PRIVATE ph7_value * PH7_ContextThisValue(ph7_context *pCtx)` |
|        5 | 4140 | `{` |
|     9449 | 4141 | `	if( pCtx->pThis == 0 ){` |
|      ! 0 | 4142 | `		return 0;` |
|        - | 4143 | `	}` |
|     9449 | 4144 | `	if( !pCtx->bThisInit ){` |
|     9449 | 4145 | `		PH7_MemObjInit(pCtx->pVm,&pCtx->sThis);` |
|     9449 | 4146 | `		pCtx->sThis.x.pOther = pCtx->pThis;` |
|     9449 | 4147 | `		pCtx->sThis.iFlags = MEMOBJ_OBJ;` |
|     9449 | 4148 | `		pCtx->sThis.nIdx = SXU32_HIGH;` |
|     9449 | 4149 | `		pCtx->bThisInit = 1;` |
|     4722 | 4150 | `	}` |
|     9449 | 4151 | `	return &pCtx->sThis;` |
|     4727 | 4152 | `}` |
|        - | 4153 |  |
