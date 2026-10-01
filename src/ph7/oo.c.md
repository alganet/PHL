# src/ph7/oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1685/1911 lines (88.17%)

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
|  1477551 |   14 | `PH7_PRIVATE ph7_class * PH7_NewRawClass(ph7_vm *pVm,const SyString *pName,sxu32 nLine)` |
|        5 |   15 | `{` |
|        - |   16 | `	ph7_class *pClass;` |
|        - |   17 | `	char *zName;` |
|        - |   18 | `	/* Allocate a new instance */` |
|  1477556 |   19 | `	pClass = (ph7_class *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class));` |
|  1477556 |   20 | `	if( pClass == 0 ){` |
|      ! 0 |   21 | `		return 0;` |
|        - |   22 | `	}` |
|        - |   23 | `	/* Zero the structure */` |
|  1477556 |   24 | `	SyZero(pClass,sizeof(ph7_class));` |
|        - |   25 | `	/* Duplicate class name */` |
|  1477556 |   26 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1477556 |   27 | `	if( zName == 0 ){` |
|      ! 0 |   28 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClass);` |
|      ! 0 |   29 | `		return 0;` |
|        - |   30 | `	}` |
|        - |   31 | `	/* Initialize fields */` |
|  1477556 |   32 | `	SyStringInitFromBuf(&pClass->sName,zName,pName->nByte);` |
|        - |   33 | `	/* php method names are CASE-INSENSITIVE ($o->FOO() finds foo(), and declaring` |
|        - |   34 | `	 * both is a redeclaration), so the method table matches on them the same way` |
|        - |   35 | `	 * hClass does for class names. Properties and class constants ARE case` |
|        - |   36 | `	 * sensitive in php, so hAttr keeps the default exact comparator. */` |
|  1477556 |   37 | `	SyHashInit(&pClass->hMethod,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|  1477556 |   38 | `	SyHashInit(&pClass->hAttr,&pVm->sAllocator,0,0);` |
|  1477556 |   39 | `	SyHashInit(&pClass->hConst,&pVm->sAllocator,0,0);` |
|  1477556 |   40 | `	SyHashInit(&pClass->hDerived,&pVm->sAllocator,0,0);` |
|  1477556 |   41 | `	SySetInit(&pClass->aInterface,&pVm->sAllocator,sizeof(ph7_class *));` |
|  1477556 |   42 | `	SySetInit(&pClass->aTrait,&pVm->sAllocator,sizeof(ph7_class *));` |
|  1477556 |   43 | `	SySetInit(&pClass->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|  1477556 |   44 | `	SySetInit(&pClass->aEnumCases,&pVm->sAllocator,sizeof(ph7_class_attr *));` |
|  1477556 |   45 | `	pClass->nLine = nLine;` |
|  1477556 |   46 | `	if( pVm->bCompilingBuiltin ){` |
|        - |   47 | `		/* Defined by an embedded builtin chunk: internal, no defining file.` |
|        - |   48 | `		 * Class compilers merge further flags with \|= so this survives. */` |
|  1471904 |   49 | `		pClass->iFlags \|= PH7_CLASS_INTERNAL;` |
|   734969 |   50 | `	}else{` |
|        - |   51 | `		/* Alias the VM-lifetime path dup on top of the include stack */` |
|     5657 |   52 | `		SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     5657 |   53 | `		if( pFile ){` |
|     5657 |   54 | `			SyStringDupPtr(&pClass->sFile,pFile);` |
|     2826 |   55 | `		}` |
|        - |   56 | `	}` |
|        - |   57 | `	/* All done */` |
|  1477556 |   58 | `	return pClass;` |
|   737795 |   59 | `}` |
|        - |   60 | `/*` |
|        - |   61 | ` * Allocate and initialize a new class attribute.` |
|        - |   62 | ` * Return a pointer to the class attribute on success. NULL otherwise.` |
|        - |   63 | ` */` |
|  5259634 |   64 | `PH7_PRIVATE ph7_class_attr * PH7_NewClassAttr(ph7_vm *pVm,const SyString *pName,sxu32 nLine,sxi32 iProtection,sxi32 iFlags)` |
|        5 |   65 | `{` |
|        - |   66 | `	ph7_class_attr *pAttr;` |
|        - |   67 | `	char *zName;` |
|  5259639 |   68 | `	pAttr = (ph7_class_attr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_attr));` |
|  5259639 |   69 | `	if( pAttr == 0 ){` |
|      ! 0 |   70 | `		return 0;` |
|        - |   71 | `	}` |
|        - |   72 | `	/* Zero the structure */` |
|  5259639 |   73 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|  5259639 |   74 | `	SySetInit(&pAttr->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|        - |   75 | `	/* Duplicate attribute name */` |
|  5259639 |   76 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  5259639 |   77 | `	if( zName == 0 ){` |
|      ! 0 |   78 | `		SyMemBackendPoolFree(&pVm->sAllocator,pAttr);` |
|      ! 0 |   79 | `		return 0;` |
|        - |   80 | `	}` |
|        - |   81 | `	/* Initialize fields */` |
|  5259639 |   82 | `	SySetInit(&pAttr->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|  5259639 |   83 | `	SySetInit(&pAttr->aUnionAlts,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|  5259639 |   84 | `	SyStringInitFromBuf(&pAttr->sName,zName,pName->nByte);` |
|  5259639 |   85 | `	pAttr->iProtection = iProtection;` |
|  5259639 |   86 | `	pAttr->nIdx = SXU32_HIGH;` |
|  5259639 |   87 | `	pAttr->iFlags = iFlags;` |
|  5259639 |   88 | `	pAttr->nLine = nLine;` |
|  5259639 |   89 | `	return pAttr;` |
|  2626303 |   90 | `}` |
|        - |   91 | `/*` |
|        - |   92 | ` * Allocate and initialize a new class method.` |
|        - |   93 | ` * Return a pointer to the class method on success. NULL otherwise` |
|        - |   94 | ` * This function associate with the newly created method an automatically generated` |
|        - |   95 | ` * random unique name.` |
|        - |   96 | ` */` |
|  9434893 |   97 | `PH7_PRIVATE ph7_class_method * PH7_NewClassMethod(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,sxu32 nLine,` |
|        - |   98 | `	sxi32 iProtection,sxi32 iFlags,sxi32 iFuncFlags)` |
|        5 |   99 | `{` |
|        - |  100 | `	ph7_class_method *pMeth;` |
|        - |  101 | `	SyHashEntry *pEntry;` |
|        - |  102 | `	SyString *pNamePtr;` |
|        - |  103 | `	char zSalt[10];` |
|        - |  104 | `	char *zName;` |
|        - |  105 | `	sxu32 nByte;` |
|        - |  106 | `	/* Allocate a new class method instance */` |
|  9434898 |  107 | `	pMeth = (ph7_class_method *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_method));` |
|  9434898 |  108 | `	if( pMeth == 0 ){` |
|      ! 0 |  109 | `		return 0;` |
|        - |  110 | `	}` |
|        - |  111 | `	/* Zero the structure */` |
|  9434898 |  112 | `	SyZero(pMeth,sizeof(ph7_class_method));` |
|        - |  113 | `	/* Check for an already installed method with the same name */` |
|  9434898 |  114 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
|  9434898 |  115 | `	if( pEntry == 0 ){` |
|        - |  116 | `		/* Associate an unique VM name to this method */` |
|  9434892 |  117 | `		nByte = sizeof(zSalt) + pName->nByte + SyStringLength(&pClass->sName)+sizeof(char)*7/*[[__'\0'*/;` |
|  9434892 |  118 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,nByte);` |
|  9434892 |  119 | `		if( zName == 0 ){` |
|      ! 0 |  120 | `			SyMemBackendPoolFree(&pVm->sAllocator,pMeth);` |
|      ! 0 |  121 | `			return 0;` |
|        - |  122 | `		}` |
|  9434892 |  123 | `		pNamePtr = &pMeth->sVmName;` |
|        - |  124 | `		/* Generate a random string */` |
|  9434892 |  125 | `		PH7_VmRandomString(&(*pVm),zSalt,sizeof(zSalt));` |
|  9434892 |  126 | `		pNamePtr->nByte = SyBufferFormat(zName,nByte,"[__%z@%z_%.*s]",&pClass->sName,pName,sizeof(zSalt),zSalt);` |
|  9434892 |  127 | `		pNamePtr->zString = zName;` |
|  4711135 |  128 | `	}else{` |
|        - |  129 | `		/* Method is condidate for 'overloading' */` |
|        8 |  130 | `		ph7_class_method *pCurrent = (ph7_class_method *)pEntry->pUserData;` |
|        8 |  131 | `		pNamePtr = &pMeth->sVmName;` |
|        - |  132 | `		/* Use the same VM name */` |
|        8 |  133 | `		SyStringDupPtr(pNamePtr,&pCurrent->sVmName);` |
|        8 |  134 | `		zName = (char *)pNamePtr->zString;` |
|        - |  135 | `	}` |
|        - |  136 | `` 	/* Every method keeps the visibility it was DECLARED with, `__destruct` `` |
|        - |  137 | `	 * included. It used to be forced public here "because the engine invokes it` |
|        - |  138 | `	 * internally" -- but the engine's teardown reaches it through` |
|        - |  139 | `	 * PH7_VmCallClassMethod, which never consults the visibility, so the force` |
|        - |  140 | `	 * bought nothing and cost the declaration: a private destructor reflected as` |
|        - |  141 | `	 * public (isPrivate() false, modifiers 1), was listed by get_class_methods()` |
|        - |  142 | ``	 * from outside the class, printed `private` nowhere in its Reflection export,`` |
|        - |  143 | ``	 * and could be called as `$o->__destruct()` from any scope. __construct has`` |
|        - |  144 | `	 * kept its declared visibility since band A #4, and php enforces that one at` |
|        - |  145 | ``	 * `new`; a method named like the class is a PLAIN method (PHP-4 constructors`` |
|        - |  146 | `	 * removed in 8.0) and keeps its own too. */` |
|        - |  147 | `	/* Initialize method fields */` |
|  9434898 |  148 | `	pMeth->iProtection = iProtection;` |
|  9434898 |  149 | `	pMeth->iFlags = iFlags;` |
|  9434898 |  150 | `	pMeth->nLine = nLine;` |
| 14146031 |  151 | `	PH7_VmInitFuncState(&(*pVm),&pMeth->sFunc,&zName[sizeof(char)*4/*[__@*/+SyStringLength(&pClass->sName)],` |
|  9434893 |  152 | `		pName->nByte,iFuncFlags\|VM_FUNC_CLASS_METHOD,pClass);` |
|  9434898 |  153 | `	return pMeth;` |
|  4711138 |  154 | `}` |
|        - |  155 | `/*` |
|        - |  156 | ` * Check if the given name have a class method associated with it.` |
|        - |  157 | ` * Return the desired method [i.e: ph7_class_method instance] on success. NULL otherwise.` |
|        - |  158 | ` */` |
|  6951787 |  159 | `PH7_PRIVATE ph7_class_method * PH7_ClassExtractMethod(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|        5 |  160 | `{` |
|        - |  161 | `	SyHashEntry *pEntry;` |
|        - |  162 | `	/* Perform a hash lookup */` |
|  6951792 |  163 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)zName,nByte);` |
|  6951792 |  164 | `	if( pEntry == 0 ){` |
|        - |  165 | `		/* No such entry */` |
|  1636629 |  166 | `		return 0;` |
|        - |  167 | `	}` |
|        - |  168 | `	/* Point to the desired method */` |
|  5315168 |  169 | `	return (ph7_class_method *)pEntry->pUserData;` |
|  3475526 |  170 | `}` |
|        - |  171 | `/*` |
|        - |  172 | ` * Check if the given name is a class attribute.` |
|        - |  173 | ` * Return the desired attribute [i.e: ph7_class_attr instance] on success.NULL otherwise.` |
|        - |  174 | ` */` |
|   107632 |  175 | `PH7_PRIVATE ph7_class_attr * PH7_ClassExtractAttribute(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|        5 |  176 | `{` |
|        - |  177 | `	SyHashEntry *pEntry;` |
|        - |  178 | `	/* Perform a hash lookup */` |
|   107637 |  179 | `	pEntry = SyHashGet(&pClass->hAttr,(const void *)zName,nByte);` |
|   107637 |  180 | `	if( pEntry == 0 ){` |
|        - |  181 | `		/* No such entry */` |
|     3575 |  182 | `		return 0;` |
|        - |  183 | `	}` |
|        - |  184 | `	/* Point to the desierd method */` |
|   104067 |  185 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|    53821 |  186 | `}` |
|        - |  187 | `/*` |
|        - |  188 | ` * php's MANGLED storage name for one property, as an instance of pClass files it.` |
|        - |  189 | ` *` |
|        - |  190 | `` * php makes a split this engine did not: `ce->properties_info` is keyed by the`` |
|        - |  191 | ` * PLAIN name and is where every visibility decision is made, while the object's` |
|        - |  192 | ` * own slot table is keyed by a MANGLED one -- "\0DeclaringClass\0name" for a` |
|        - |  193 | ` * private property, the bare name for everything else. Two things follow from it,` |
|        - |  194 | ` * and both were wrong here.` |
|        - |  195 | ` *` |
|        - |  196 | `` * `class A { private $q; } class B extends A { private $q; }` has TWO slots on one`` |
|        - |  197 | ` * object, each reachable only from its own declaring class, where PHL had one --` |
|        - |  198 | ` * so a base method reading its own private got the CHILD's value, with nothing to` |
|        - |  199 | ` * announce it. And a base's private is INVISIBLE from outside rather than merely` |
|        - |  200 | ` * inaccessible: a lookup by the plain name finds nothing at all, which is why php` |
|        - |  201 | `` * answers `$b->q` with "Undefined property: B::$q" and not with the visibility`` |
|        - |  202 | `` * refusal it words for `$a->q`.`` |
|        - |  203 | ` *` |
|        - |  204 | ` * The declaring class is fixed per attribute, so the mangled name is built once` |
|        - |  205 | ` * and cached on it. A property this class DECLARED keeps its plain name -- php` |
|        - |  206 | ` * mangles the storage name there too, but nothing else in this engine ever sees` |
|        - |  207 | ` * the difference, and the plain key is what every lookup that does not know about` |
|        - |  208 | ` * a scope already asks for.` |
|        - |  209 | ` */` |
|  5419907 |  210 | `PH7_PRIVATE const SyString * PH7_ClassAttrStorageName(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        5 |  211 | `{` |
|        - |  212 | `	ph7_class *pDecl;` |
|        - |  213 | `	sxu32 nCls,nName;` |
|        - |  214 | `	char *zKey;` |
|  5419907 |  215 | `	if( pAttr->iProtection != PH7_CLASS_PROT_PRIVATE` |
|  3760282 |  216 | `	 \|\| (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_DYNAMIC)) != 0 ){` |
|  3314843 |  217 | `		return &pAttr->sName;` |
|        - |  218 | `	}` |
|  2105074 |  219 | `	pDecl = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|  2105074 |  220 | `	if( pDecl == 0 \|\| pDecl == pClass ){` |
|       17 |  221 | `		return &pAttr->sName;` |
|        - |  222 | `	}` |
|  2105058 |  223 | `	if( pDecl->iFlags & PH7_CLASS_INTERNAL ){` |
|        - |  224 | `		/* An ENGINE class's slot keeps its plain name on every object below it:` |
|        - |  225 | `		 * the C bodies that own that storage address it by name (a DateTime's` |
|        - |  226 | `		 * timestamp, a PDOStatement's handle), and a subclass instance whose slots` |
|        - |  227 | `		 * were renamed read as an object whose parent constructor never ran. php` |
|        - |  228 | `		 * mangles an internal private too; nothing here can see the difference,` |
|        - |  229 | `		 * because an engine class's private is not a name user code declares. */` |
|  2104914 |  230 | `		return &pAttr->sName;` |
|        - |  231 | `	}` |
|      148 |  232 | `	if( SyStringLength(&pAttr->sStoreName) > 0 ){` |
|       99 |  233 | `		return &pAttr->sStoreName;` |
|        - |  234 | `	}` |
|       50 |  235 | `	nCls = SyStringLength(&pDecl->sName);` |
|       50 |  236 | `	nName = SyStringLength(&pAttr->sName);` |
|        - |  237 | `	/* Class-lifetime, like sName's own dup: an attribute outlives every instance` |
|        - |  238 | `	 * whose table points at this key. */` |
|       50 |  239 | `	zKey = (char *)SyMemBackendAlloc(&pVm->sAllocator,nCls + nName + 3);` |
|       50 |  240 | `	if( zKey == 0 ){` |
|      ! 0 |  241 | `		return &pAttr->sName;` |
|        - |  242 | `	}` |
|       50 |  243 | `	zKey[0] = 0;` |
|       50 |  244 | `	SyMemcpy((const void *)SyStringData(&pDecl->sName),(void *)&zKey[1],nCls);` |
|       50 |  245 | `	zKey[1+nCls] = 0;` |
|       50 |  246 | `	SyMemcpy((const void *)SyStringData(&pAttr->sName),(void *)&zKey[nCls+2],nName);` |
|       50 |  247 | `	zKey[nCls+nName+2] = 0;` |
|       50 |  248 | `	SyStringInitFromBuf(&pAttr->sStoreName,zKey,nCls + nName + 2);` |
|       50 |  249 | `	return &pAttr->sStoreName;` |
|  2706336 |  250 | `}` |
|        - |  251 | `/*` |
|        - |  252 | ` * One bit of sixty-four for a property NAME, taken from the name's own hash.` |
|        - |  253 | ` *` |
|        - |  254 | ` * The two masks below are how a property access decides, without a lookup, that no` |
|        - |  255 | ` * mangled slot can be involved. They have to be SHARP: a mask built from a few` |
|        - |  256 | ` * bytes of the name collides with whatever ordinary property a class reads in its` |
|        - |  257 | ` * hot loop, and one unlucky pair then puts the whole workload back on the slow` |
|        - |  258 | ` * path -- measured, 65.3 million times on the phpcs step, out of 65.8 million that` |
|        - |  259 | ` * got past a length-and-three-bytes signature. So the bit comes from the same hash` |
|        - |  260 | ` * the lookup itself uses, which the caller has already computed for the instance` |
|        - |  261 | ` * probe (SyHashKey) and hands down.` |
|        - |  262 | ` *` |
|        - |  263 | ` * Every side of this asks a class's hAttr for the hash rather than naming a hash` |
|        - |  264 | ` * function, because a bit set under one function and tested under another would` |
|        - |  265 | ` * make a scope's private stop resolving -- silently, and only for the names that` |
|        - |  266 | ` * collide. All the property tables share one function; this is what keeps that a` |
|        - |  267 | ` * fact rather than an assumption.` |
|        - |  268 | ` */` |
|   975121 |  269 | `static sxu64 OoShadowNameBit(sxu32 nHash)` |
|        5 |  270 | `{` |
|   975126 |  271 | `	return ((sxu64)1) << (nHash & 63);` |
|        5 |  272 | `}` |
|        - |  273 | `/*` |
|        - |  274 | ` * Note, on the class that DECLARES it, that this name is one of its own private` |
|        - |  275 | ` * instance properties. Called from every path that files an attribute in a class's` |
|        - |  276 | ` * hAttr under its plain name.` |
|        - |  277 | ` *` |
|        - |  278 | ` * Only an instance property counts: a private STATIC and a private CONSTANT are` |
|        - |  279 | ` * never reached through an object's slot table, and OoScopePrivateAttr refuses` |
|        - |  280 | ` * both explicitly.` |
|        - |  281 | ` */` |
|  2557082 |  282 | `PH7_PRIVATE void PH7_ClassNotePrivateName(ph7_class *pClass,ph7_class_attr *pAttr)` |
|        5 |  283 | `{` |
|  2557082 |  284 | `	if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|  1764944 |  285 | `	 && (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|  1461692 |  286 | `		pClass->nPrivName \|= OoShadowNameBit(SyHashKey(&pClass->hAttr,` |
|   974893 |  287 | `			(const void *)SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName)));` |
|   486794 |  288 | `	}` |
|  2557087 |  289 | `}` |
|        - |  290 | `/*` |
|        - |  291 | `` * Which PROPERTY does `name` mean, seen from the class whose code is RUNNING?`` |
|        - |  292 | ` *` |
|        - |  293 | ` * php's zend_get_parent_private_property: a scope that declares a private of this` |
|        - |  294 | ` * name owns a slot of its own on every instance below it, and that slot -- not` |
|        - |  295 | ` * whatever the object's class holds under the plain name -- is what its code` |
|        - |  296 | ` * means. Answers 0 when the executing scope has no such private, which leaves the` |
|        - |  297 | ` * caller on the ordinary plain-name path.` |
|        - |  298 | ` */` |
|   123031 |  299 | `static ph7_class_attr * OoScopePrivateAttr(ph7_vm *pVm,ph7_class *pClass,const char *zName,` |
|        - |  300 | `	sxu32 nName,sxu32 nHash)` |
|        5 |  301 | `{` |
|        - |  302 | `	ph7_class *pScope;` |
|        - |  303 | `	SyHashEntry *pEntry;` |
|        - |  304 | `	ph7_class_attr *pOwn;` |
|        - |  305 | `	sxu64 nBit;` |
|   123036 |  306 | `	if( nName < 1 ){` |
|       65 |  307 | `		return 0;` |
|        - |  308 | `	}` |
|   122972 |  309 | `	if( (pClass->iFlags & PH7_CLASS_SHADOW_PROP) == 0 ){` |
|        - |  310 | `		/* No property of this class is filed under a mangled name, so it holds no` |
|        - |  311 | `		 * slot the plain probe cannot reach. Reaching one at all takes a scope this` |
|        - |  312 | `		 * class DESCENDS from, and inheriting that scope's private is exactly what` |
|        - |  313 | `		 * sets the flag -- so this is the whole test, and every ordinary property` |
|        - |  314 | `		 * access skips the frame walk below on it. */` |
|   122798 |  315 | `		return 0;` |
|        - |  316 | `	}` |
|      178 |  317 | `	nBit = OoShadowNameBit(nHash);` |
|      178 |  318 | `	if( (pClass->nShadowName & nBit) == 0 ){` |
|        - |  319 | `		/* ...and the flag alone is not enough. A class that inherits ONE private` |
|        - |  320 | `		 * property pays for the walk below on every access to every OTHER property` |
|        - |  321 | `		 * it has, and on the phpcs step that was 79.4 million hash lookups made to` |
|        - |  322 | `		 * answer "no" -- 99.4% of the ones this function made, and 14% of every` |
|        - |  323 | `		 * lookup the engine did. The mask names the plain names a mangled slot` |
|        - |  324 | `		 * could be hiding under, so a miss is the complete answer: nothing can be` |
|        - |  325 | `		 * reached under a name this class holds no mangled slot for. */` |
|       24 |  326 | `		return 0;` |
|        - |  327 | `	}` |
|      156 |  328 | `	pScope = PH7_VmCallerScope(&(*pVm));` |
|      156 |  329 | `	if( pScope == 0 \|\| pScope == pClass ){` |
|       68 |  330 | `		return 0;   /* global scope, or the object's own class: the plain name IS the slot */` |
|        - |  331 | `	}` |
|       89 |  332 | `	if( (pScope->nPrivName & nBit) == 0 ){` |
|        - |  333 | `		/* ...and the same question from the other side, which is the one that` |
|        - |  334 | `		 * decides: the scope can only mean a mangled slot for a name it declares` |
|        - |  335 | `		 * PRIVATE itself. A class inherits many more mangled names than it` |
|        - |  336 | `		 * declares private ones, so this mask is the sparser of the two, and the` |
|        - |  337 | `		 * pair of them is what leaves this lookup to the accesses that need it. */` |
|      ! 0 |  338 | `		return 0;` |
|        - |  339 | `	}` |
|       89 |  340 | `	pEntry = SyHashGetHashed(&pScope->hAttr,(const void *)zName,nName,nHash);` |
|       89 |  341 | `	pOwn = pEntry ? (ph7_class_attr *)pEntry->pUserData : 0;` |
|       88 |  342 | `	if( pOwn == 0` |
|       88 |  343 | `	 \|\| pOwn->iProtection != PH7_CLASS_PROT_PRIVATE` |
|       87 |  344 | `	 \|\| (pOwn->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) != 0` |
|       86 |  345 | `	 \|\| PH7_VmMemberOwnerClass(pOwn->pDeclClass,pScope) != pScope` |
|       87 |  346 | `	 \|\| !PH7_VmInstanceOf(pClass,pScope) ){` |
|        3 |  347 | `		return 0;` |
|        - |  348 | `	}` |
|       87 |  349 | `	return pOwn;` |
|    61521 |  350 | `}` |
|        - |  351 | `/*` |
|        - |  352 | ` * php presents an object's properties by their PLAIN names, so two slots that` |
|        - |  353 | ` * unmangle to the same one -- a base's private and the subclass's own property --` |
|        - |  354 | `` * collide on every surface that walks the object BY NAME: `foreach` and`` |
|        - |  355 | ` * get_object_vars(). php keeps the FIRST accessible one in storage order and drops` |
|        - |  356 | ` * the rest, which is base-first, so a base method iterating a subclass instance` |
|        - |  357 | `` * sees its OWN `$q` and never the child's. Only the RAW surfaces show both --`` |
|        - |  358 | ` * (array), serialize(), var_dump(), get_mangled_object_vars() -- and those key by` |
|        - |  359 | ` * the mangled name, where nothing collides.` |
|        - |  360 | ` *` |
|        - |  361 | ` * TRUE when an EARLIER entry of this object's table carries the same plain name` |
|        - |  362 | ` * and is itself accessible from here.` |
|        - |  363 | ` */` |
|      990 |  364 | `PH7_PRIVATE int PH7_ClassInstanceAttrShadowed(ph7_vm *pVm,ph7_class_instance *pThis,SyHashEntry *pEntry)` |
|        5 |  365 | `{` |
|      995 |  366 | `	VmClassAttr *pMe = (VmClassAttr *)pEntry->pUserData;` |
|        - |  367 | `	SyString *pName;` |
|        - |  368 | `	SyHashEntry *pWalk;` |
|      995 |  369 | `	if( (pThis->pClass->iFlags & PH7_CLASS_SHADOW_PROP) == 0 \|\| pMe == 0 ){` |
|      977 |  370 | `		return 0;   /* no mangled slot on this class: no name can collide */` |
|        - |  371 | `	}` |
|       19 |  372 | `	pName = &pMe->pAttr->sName;` |
|       23 |  373 | `	for( pWalk = SyHashFirstEntry(&pThis->hAttr) ; pWalk && pWalk != pEntry ;` |
|        5 |  374 | `	     pWalk = SyHashEntryNext(pWalk) ){` |
|        9 |  375 | `		VmClassAttr *pOther = (VmClassAttr *)pWalk->pUserData;` |
|        9 |  376 | `		if( pOther == 0 \|\| pOther->pAttr == pMe->pAttr ){` |
|      ! 0 |  377 | `			continue;` |
|        - |  378 | `		}` |
|        8 |  379 | `		if( SyStringLength(&pOther->pAttr->sName) != SyStringLength(pName)` |
|        9 |  380 | `		 \|\| SyMemcmp((const void *)SyStringData(&pOther->pAttr->sName),` |
|       12 |  381 | `			(const void *)SyStringData(pName),SyStringLength(pName)) != 0 ){` |
|      ! 0 |  382 | `			continue;` |
|        - |  383 | `		}` |
|        8 |  384 | `		if( PH7_ClassInstanceAttrPresented(pOther)` |
|        9 |  385 | `		 && PH7_VmClassAttrAccess(&(*pVm),pThis->pClass,pOther->pAttr,FALSE) ){` |
|        5 |  386 | `			return 1;` |
|        - |  387 | `		}` |
|        3 |  388 | `	}` |
|       15 |  389 | `	return 0;` |
|      500 |  390 | `}` |
|        - |  391 | `/*` |
|        - |  392 | ` * PH7_ClassExtractAttribute, told which scope is asking: the executing class's own` |
|        - |  393 | ` * private wins over the same name declared further down the chain.` |
|        - |  394 | ` */` |
|     1220 |  395 | `PH7_PRIVATE ph7_class_attr * PH7_ClassScopedAttribute(ph7_vm *pVm,ph7_class *pClass,const char *zName,sxu32 nName)` |
|        5 |  396 | `{` |
|     2435 |  397 | `	ph7_class_attr *pOwn = OoScopePrivateAttr(&(*pVm),pClass,zName,nName,` |
|     1210 |  398 | `		nName > 0 ? SyHashKey(&pClass->hAttr,(const void *)zName,nName) : 0);` |
|     1225 |  399 | `	if( pOwn ){` |
|        3 |  400 | `		return pOwn;` |
|        - |  401 | `	}` |
|     1223 |  402 | `	return PH7_ClassExtractAttribute(pClass,zName,nName);` |
|      615 |  403 | `}` |
|        - |  404 | `/*` |
|        - |  405 | ` * PH7_ClassInstanceAttrEntry, told which scope is asking. When the executing class` |
|        - |  406 | ` * declares a private of this name, its MANGLED slot is the only one it can mean --` |
|        - |  407 | ` * so a miss there is a miss, and never falls back to the plain name (php's fetch` |
|        - |  408 | `` * stops at the property_info it resolved; an `unset()` of that slot reads as`` |
|        - |  409 | ` * undefined even when a public property of the same name sits beside it).` |
|        - |  410 | ` */` |
|   121811 |  411 | `PH7_PRIVATE SyHashEntry * PH7_ClassInstanceScopedAttrEntry(ph7_vm *pVm,ph7_class_instance *pThis,` |
|        - |  412 | `	const char *zName,sxu32 nName,sxu32 nHash)` |
|        5 |  413 | `{` |
|   121816 |  414 | `	ph7_class_attr *pOwn = OoScopePrivateAttr(&(*pVm),pThis->pClass,zName,nName,nHash);` |
|   121816 |  415 | `	if( pOwn ){` |
|        - |  416 | `		/* The MANGLED key is a different string, so the caller's hash says nothing` |
|        - |  417 | `		 * about it -- this is the 0.6% of accesses that really do mean a scope's` |
|        - |  418 | `		 * private, and they hash their own key. */` |
|       85 |  419 | `		const SyString *pKey = PH7_ClassAttrStorageName(&(*pVm),pThis->pClass,pOwn);` |
|       85 |  420 | `		return SyHashGet(&pThis->hAttr,(const void *)SyStringData(pKey),SyStringLength(pKey));` |
|        - |  421 | `	}` |
|   121732 |  422 | `	if( nName > 0 ){` |
|   121688 |  423 | `		return SyHashGetHashed(&pThis->hAttr,(const void *)zName,nName,nHash);` |
|        - |  424 | `	}` |
|       45 |  425 | `	return PH7_ClassInstanceAttrEntry(pThis,zName,nName);` |
|    60911 |  426 | `}` |
|        - |  427 | `/*` |
|        - |  428 | ` * Check if the given name is a class CONSTANT (or enum case).` |
|        - |  429 | ` * php keeps constants and properties in separate namespaces, so constants live` |
|        - |  430 | ` * in a dedicated table (hConst) and never collide with a same-named property.` |
|        - |  431 | ` * Return the desired constant [ph7_class_attr with PH7_CLASS_ATTR_CONSTANT] on` |
|        - |  432 | ` * success, NULL otherwise.` |
|        - |  433 | ` */` |
|     3520 |  434 | `PH7_PRIVATE ph7_class_attr * PH7_ClassExtractConstant(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|        5 |  435 | `{` |
|        - |  436 | `	SyHashEntry *pEntry;` |
|     3525 |  437 | `	pEntry = SyHashGet(&pClass->hConst,(const void *)zName,nByte);` |
|     3525 |  438 | `	if( pEntry == 0 ){` |
|      689 |  439 | `		return 0;` |
|        - |  440 | `	}` |
|     2841 |  441 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|     1765 |  442 | `}` |
|        - |  443 | `/*` |
|        - |  444 | ` * Install a class attribute in the corresponding container.` |
|        - |  445 | ` * A constant (or enum case) goes to hConst, a property to hAttr — php's two` |
|        - |  446 | `` * separate member namespaces, so `const C` and `public $C` coexist.`` |
|        - |  447 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|        - |  448 | ` */` |
|  5259628 |  449 | `PH7_PRIVATE sxi32 PH7_ClassInstallAttr(ph7_class *pClass,ph7_class_attr *pAttr)` |
|        5 |  450 | `{` |
|  5259633 |  451 | `	SyString *pName = &pAttr->sName;` |
|        - |  452 | `	sxi32 rc;` |
|        - |  453 | `	/* Remember where this attribute was originally declared so that later` |
|        - |  454 | `	 * inheritance/trait copies still know the declaring class (needed for` |
|        - |  455 | `	 * PHP-compatible error messages on typed properties). */` |
|  5259633 |  456 | `	if( pAttr->pDeclClass == 0 ){` |
|    37416 |  457 | `		pAttr->pDeclClass = pClass;` |
|    18683 |  458 | `	}` |
|  5259633 |  459 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|  2702653 |  460 | `		rc = SyHashInsertTail(&pClass->hConst,(const void *)pName->zString,pName->nByte,pAttr);` |
|  1349520 |  461 | `	}else{` |
|  2556985 |  462 | `		rc = SyHashInsertTail(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|  2556985 |  463 | `		PH7_ClassNotePrivateName(pClass,pAttr);` |
|        - |  464 | `	}` |
|  5259633 |  465 | `	return rc;` |
|        5 |  466 | `}` |
|        - |  467 | `/*` |
|        - |  468 | ` * Install a class method in the corresponding container.` |
|        - |  469 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|        - |  470 | ` */` |
|  9434845 |  471 | `PH7_PRIVATE sxi32 PH7_ClassInstallMethod(ph7_class *pClass,ph7_class_method *pMeth)` |
|        5 |  472 | `{` |
|  9434850 |  473 | `	SyString *pName = &pMeth->sFunc.sName;` |
|        - |  474 | `	sxi32 rc;` |
|  9434850 |  475 | `	rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|  9434850 |  476 | `	return rc;` |
|        5 |  477 | `}` |
|        - |  478 | `/*` |
|        - |  479 | ` * ---------------------------------------------------------------------------` |
|        - |  480 | ` * php's rendering of a USER function's DECLARATION.` |
|        - |  481 | ` *` |
|        - |  482 | ` * The text an incompatible-override fatal prints on either side of "must be` |
|        - |  483 | `` * compatible with": `B::f(int $a, ?string $b = null): string`. It is php's`` |
|        - |  484 | ` * zend_get_function_declaration, and until this shipped both sides of that` |
|        - |  485 | `` * sentence were bare names -- `B::f() must be compatible with A::f()` -- which`` |
|        - |  486 | ` * says the declarations disagree without saying how.` |
|        - |  487 | ` *` |
|        - |  488 | `` * A parameter is `[type ][&][...]$name[ = default]`; the return type follows as`` |
|        - |  489 | `` * `: T` and is omitted entirely when the declaration has none.`` |
|        - |  490 | ` * ---------------------------------------------------------------------------` |
|        - |  491 | ` */` |
|        - |  492 | `/* A character that belongs to a type NAME, as opposed to the punctuation that` |
|        - |  493 | `` * separates the parts of a declared type (`?A`, `A\|B`, `(A&B)\|null`). */`` |
|      406 |  494 | `static int OoDeclNameChar(int c)` |
|        4 |  495 | `{` |
|      812 |  496 | `	return !(c == '\|' \|\| c == '&' \|\| c == '?' \|\| c == '(' \|\| c == ')'` |
|      402 |  497 | `		\|\| c == ' ' \|\| c == '\t');` |
|        4 |  498 | `}` |
|        - |  499 | `/*` |
|        - |  500 | ` * A declared type as php prints it in a declaration.` |
|        - |  501 | ` *` |
|        - |  502 | `` * The stored text is already php's canonical order (`string\|int` for both`` |
|        - |  503 | `` * spellings of it, `?A` for `A\|null`, an intersection as written), so only the`` |
|        - |  504 | ` * names that are relative to WHERE the declaration was written move:` |
|        - |  505 | ` *` |
|        - |  506 | ` *   self / parent  resolved against the DECLARING class -- so the same trait` |
|        - |  507 | `` *                  method reads `A $a` in one composing class and `B $a` in the`` |
|        - |  508 | ` *                  next, which is what php prints.` |
|        - |  509 | ` *   static         left as written; php has no class to resolve it to at link` |
|        - |  510 | ` *                  time either.` |
|        - |  511 | ` *   iterable       expanded to the two types it stands for. Only the standalone` |
|        - |  512 | ` *                  spellings reach here (a COMPOUND type stored it expanded` |
|        - |  513 | `` *                  already), so `?iterable` is `Traversable\|array\|null` rather`` |
|        - |  514 | `` *                  than `?Traversable\|array` -- the whole text, not a token.`` |
|        - |  515 | ` *` |
|        - |  516 | ` * VmHintTextResolvedEx answers the same question for a DIAGNOSTIC and for` |
|        - |  517 | ` * Reflection, and is not reused here for one reason: it writes into a fixed` |
|        - |  518 | ` * caller buffer and truncates. A diagnostic naming a type can afford that; a` |
|        - |  519 | ` * declaration this sentence is asking the reader to COMPARE with another cannot,` |
|        - |  520 | ` * so this one appends to the blob and has no length to run out of.` |
|        - |  521 | ` */` |
|       78 |  522 | `static void OoDeclType(ph7_class *pScope,const SyString *pDeclared,SyBlob *pOut)` |
|        4 |  523 | `{` |
|       82 |  524 | `	const char *z = pDeclared ? SyStringData(pDeclared) : 0;` |
|       82 |  525 | `	sxu32 n = z ? SyStringLength(pDeclared) : 0;` |
|       82 |  526 | `	sxu32 i = 0;` |
|       82 |  527 | `	if( n < 1 ){` |
|      ! 0 |  528 | `		return;` |
|        - |  529 | `	}` |
|       82 |  530 | `	if( n == 8 && SyStrnicmp(z,"iterable",8) == 0 ){` |
|        5 |  531 | `		SyBlobAppend(pOut,"Traversable\|array",sizeof("Traversable\|array")-1);` |
|        5 |  532 | `		return;` |
|        - |  533 | `	}` |
|       78 |  534 | `	if( n == 9 && z[0] == '?' && SyStrnicmp(&z[1],"iterable",8) == 0 ){` |
|        5 |  535 | `		SyBlobAppend(pOut,"Traversable\|array\|null",sizeof("Traversable\|array\|null")-1);` |
|        5 |  536 | `		return;` |
|        - |  537 | `	}` |
|      148 |  538 | `	while( i < n ){` |
|        - |  539 | `		sxu32 nStart;` |
|        - |  540 | `		const SyString *pWrite;` |
|        - |  541 | `		SyString sTok;` |
|       78 |  542 | `		if( !OoDeclNameChar(z[i]) ){` |
|        6 |  543 | `			SyBlobAppend(pOut,&z[i],sizeof(char));` |
|        6 |  544 | `			i++;` |
|        6 |  545 | `			continue;` |
|        - |  546 | `		}` |
|       74 |  547 | `		nStart = i;` |
|      406 |  548 | `		while( i < n && OoDeclNameChar(z[i]) ){` |
|      336 |  549 | `			i++;` |
|        4 |  550 | `		}` |
|       74 |  551 | `		SyStringInitFromBuf(&sTok,&z[nStart],i - nStart);` |
|       74 |  552 | `		pWrite = &sTok;` |
|       74 |  553 | `		if( pScope ){` |
|       70 |  554 | `			if( sTok.nByte == sizeof("self")-1` |
|       50 |  555 | `			 && SyStrnicmp(sTok.zString,"self",sizeof("self")-1) == 0 ){` |
|       10 |  556 | `				pWrite = &pScope->sName;` |
|       68 |  557 | `			}else if( sTok.nByte == sizeof("parent")-1` |
|       45 |  558 | `			 && SyStrnicmp(sTok.zString,"parent",sizeof("parent")-1) == 0` |
|       20 |  559 | `			 && pScope->pBase ){` |
|        5 |  560 | `				pWrite = &pScope->pBase->sName;` |
|        2 |  561 | `			}` |
|       35 |  562 | `		}` |
|       74 |  563 | `		SyBlobAppend(pOut,SyStringData(pWrite),SyStringLength(pWrite));` |
|        4 |  564 | `	}` |
|       43 |  565 | `}` |
|        - |  566 | `/* The instructions of a compiled default, without the OP_DONE the compiler` |
|        - |  567 | ` * terminates every one of them with. */` |
|       22 |  568 | `static sxu32 OoDeclDefLength(SySet *pByteCode)` |
|        2 |  569 | `{` |
|       24 |  570 | `	sxu32 n = SySetUsed(pByteCode);` |
|       46 |  571 | `	while( n > 0 ){` |
|       46 |  572 | `		VmInstr *pIn = (VmInstr *)SySetAt(pByteCode,n - 1);` |
|       46 |  573 | `		if( pIn == 0 \|\| (pIn->iOp != PH7_OP_DONE && pIn->iOp != PH7_OP_NOOP) ){` |
|       13 |  574 | `			break;` |
|        - |  575 | `		}` |
|       24 |  576 | `		n--;` |
|        2 |  577 | `	}` |
|       24 |  578 | `	return n;` |
|        2 |  579 | `}` |
|        - |  580 | `/*` |
|        - |  581 | ` * TRUE when every instruction of a compiled default is a LITERAL load or a pure` |
|        - |  582 | ` * value operator -- the run php's constant folder would try to reduce. A LOADC` |
|        - |  583 | ` * still carrying PH7_LOADC_EXPAND is a constant NAME, which php deliberately` |
|        - |  584 | ` * does NOT fold (it prints the name); anything that reads a variable, calls` |
|        - |  585 | ` * something or builds an object is not a constant expression at all.` |
|        - |  586 | ` *` |
|        - |  587 | ` * This is the screen PH7_VmEvalConstExpr's block comment requires. It is the` |
|        - |  588 | ` * SySet twin of the instanceof folder's GenStateInstanceofFoldsLhs, which asks` |
|        - |  589 | ` * the same question of the generator's live stream.` |
|        - |  590 | ` */` |
|       18 |  591 | `static int OoDeclDefFoldable(SySet *pByteCode,sxu32 nLen)` |
|        2 |  592 | `{` |
|        - |  593 | `	sxu32 n;` |
|       52 |  594 | `	for( n = 0 ; n < nLen ; ++n ){` |
|       36 |  595 | `		VmInstr *pIn = (VmInstr *)SySetAt(pByteCode,n);` |
|       36 |  596 | `		if( pIn == 0 ){` |
|      ! 0 |  597 | `			return 0;` |
|        - |  598 | `		}` |
|       36 |  599 | `		switch( pIn->iOp ){` |
|       13 |  600 | `		case PH7_OP_LOADC:` |
|       28 |  601 | `			if( pIn->iP1 & PH7_LOADC_EXPAND ){` |
|        3 |  602 | `				return 0; /* a constant NAME -- php keeps it unfolded */` |
|        - |  603 | `			}` |
|       26 |  604 | `			break;` |
|        4 |  605 | `		case PH7_OP_LOAD_MAP: case PH7_OP_LOAD_IDX:` |
|        - |  606 | `		case PH7_OP_CAT:` |
|        - |  607 | `		case PH7_OP_CVT_INT: case PH7_OP_CVT_STR: case PH7_OP_CVT_REAL:` |
|        - |  608 | `		case PH7_OP_CVT_BOOL: case PH7_OP_CVT_NUMC: case PH7_OP_CVT_NULL:` |
|        - |  609 | `		case PH7_OP_UMINUS: case PH7_OP_UPLUS: case PH7_OP_BITNOT: case PH7_OP_LNOT:` |
|        - |  610 | `		case PH7_OP_MUL: case PH7_OP_DIV: case PH7_OP_MOD: case PH7_OP_POW:` |
|        - |  611 | `		case PH7_OP_ADD: case PH7_OP_SUB: case PH7_OP_SHL: case PH7_OP_SHR:` |
|        - |  612 | `		case PH7_OP_LT: case PH7_OP_LE: case PH7_OP_GT: case PH7_OP_GE:` |
|        - |  613 | `		case PH7_OP_SPACESHIP: case PH7_OP_EQ: case PH7_OP_NEQ:` |
|        - |  614 | `		case PH7_OP_TEQ: case PH7_OP_TNE:` |
|        - |  615 | `		case PH7_OP_BAND: case PH7_OP_BXOR: case PH7_OP_BOR:` |
|        - |  616 | `		case PH7_OP_LAND: case PH7_OP_LOR: case PH7_OP_LXOR:` |
|        - |  617 | `		case PH7_OP_JMP: case PH7_OP_JZ: case PH7_OP_JNZ:` |
|        - |  618 | `		case PH7_OP_POP: case PH7_OP_DUP: case PH7_OP_NOOP:` |
|        9 |  619 | `			break;` |
|      ! 0 |  620 | `		default:` |
|      ! 0 |  621 | `			return 0;` |
|        - |  622 | `		}` |
|       18 |  623 | `	}` |
|       18 |  624 | `	return nLen > 0;` |
|       11 |  625 | `}` |
|        - |  626 | `/* The literal a LOADC pushes, or 0 when the operand is not a string one. */` |
|        6 |  627 | `static const SyString * OoDeclLiteral(ph7_vm *pVm,VmInstr *pIn,SyString *pOut)` |
|        1 |  628 | `{` |
|        - |  629 | `	ph7_value *pLit;` |
|        7 |  630 | `	if( pIn == 0 \|\| pIn->iOp != PH7_OP_LOADC ){` |
|      ! 0 |  631 | `		return 0;` |
|        - |  632 | `	}` |
|        7 |  633 | `	pLit = (ph7_value *)SySetAt(&pVm->aLitObj,(sxu32)pIn->iP2);` |
|        7 |  634 | `	if( pLit == 0 \|\| (pLit->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  635 | `		return 0;` |
|        - |  636 | `	}` |
|        7 |  637 | `	SyStringInitFromBuf(pOut,SyBlobData(&pLit->sBlob),SyBlobLength(&pLit->sBlob));` |
|        7 |  638 | `	return pOut;` |
|        4 |  639 | `}` |
|        - |  640 | `/*` |
|        - |  641 | ` * A FOLDED default value, spelled php's way.` |
|        - |  642 | ` *` |
|        - |  643 | ` * php's own spellings, and they are not the export's: a string is SINGLE-quoted,` |
|        - |  644 | `` * printed RAW (no escaping at all) and TRUNCATED to ten bytes with `...` inside`` |
|        - |  645 | `` * the quotes; `null` is lower-case; an array shows only whether it is empty.`` |
|        - |  646 | ` */` |
|       16 |  647 | `static void OoDeclValue(SyBlob *pOut,ph7_value *pVal)` |
|        2 |  648 | `{` |
|       18 |  649 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|        3 |  650 | `		SyBlobAppend(pOut,"null",sizeof("null")-1);` |
|        3 |  651 | `		return;` |
|        - |  652 | `	}` |
|       15 |  653 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  654 | `		SyBlobAppend(pOut,pVal->x.iVal ? "true" : "false",pVal->x.iVal ? 4 : 5);` |
|      ! 0 |  655 | `		return;` |
|        - |  656 | `	}` |
|       15 |  657 | `	if( pVal->iFlags & MEMOBJ_STRING ){` |
|        7 |  658 | `		sxu32 nStr = SyBlobLength(&pVal->sBlob);` |
|        7 |  659 | `		SyBlobAppend(pOut,"'",sizeof(char));` |
|        7 |  660 | `		if( nStr > 0 ){` |
|        7 |  661 | `			SyBlobAppend(pOut,SyBlobData(&pVal->sBlob),(nStr > 10 ? (sxu32)10 : nStr));` |
|        3 |  662 | `		}` |
|        7 |  663 | `		if( nStr > 10 ){` |
|        3 |  664 | `			SyBlobAppend(pOut,"...",sizeof("...")-1);` |
|        1 |  665 | `		}` |
|        7 |  666 | `		SyBlobAppend(pOut,"'",sizeof(char));` |
|        7 |  667 | `		return;` |
|        - |  668 | `	}` |
|        9 |  669 | `	if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|        5 |  670 | `		ph7_hashmap *pMap = (ph7_hashmap *)pVal->x.pOther;` |
|       11 |  671 | `		SyBlobAppend(pOut,` |
|        4 |  672 | `			(pMap && pMap->nEntry > 0) ? "[...]" : "[]",` |
|        4 |  673 | `			(pMap && pMap->nEntry > 0) ? sizeof("[...]")-1 : sizeof("[]")-1);` |
|        5 |  674 | `		return;` |
|        - |  675 | `	}` |
|        5 |  676 | `	if( pVal->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|        - |  677 | ``		/* php prints the value's own string cast, which is where `1.0` reads `1`,`` |
|        - |  678 | ``		 * `1e100` reads `1.0E+100` and INF reads `INF`. */`` |
|        5 |  679 | `		PH7_MemObjToString(pVal);` |
|        5 |  680 | `		SyBlobAppend(pOut,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));` |
|        5 |  681 | `		return;` |
|        - |  682 | `	}` |
|      ! 0 |  683 | `	SyBlobAppend(pOut,"<expression>",sizeof("<expression>")-1);` |
|       10 |  684 | `}` |
|        - |  685 | `/*` |
|        - |  686 | `` * The text after `= ` in a parameter default.`` |
|        - |  687 | ` *` |
|        - |  688 | ` * php prints what its compiler FOLDED the expression to, with two deliberate` |
|        - |  689 | ` * exceptions it leaves unfolded and prints as source: a lone constant reference` |
|        - |  690 | `` * keeps its NAME (`= M_PI`, `= PHP_INT_MAX`) and a class constant keeps`` |
|        - |  691 | `` * `Class::NAME` as written (`= self::K`, `= MyEnum::Foo`). `X::class` is not`` |
|        - |  692 | `` * one of those -- it folds to the class-name STRING, so it prints `'X'`.`` |
|        - |  693 | `` * Everything it could not reduce is php's `<expression>`.`` |
|        - |  694 | ` */` |
|       22 |  695 | `static void OoDeclDefault(ph7_vm *pVm,ph7_class *pScope,SySet *pByteCode,SyBlob *pOut)` |
|        2 |  696 | `{` |
|       24 |  697 | `	sxu32 nLen = OoDeclDefLength(pByteCode);` |
|        - |  698 | `	SyString sOne, sTwo;` |
|       24 |  699 | `	if( nLen == 1 ){` |
|       14 |  700 | `		VmInstr *pIn = (VmInstr *)SySetAt(pByteCode,0);` |
|       12 |  701 | `		if( pIn && pIn->iOp == PH7_OP_LOADC && (pIn->iP1 & PH7_LOADC_EXPAND)` |
|        8 |  702 | `		 && OoDeclLiteral(pVm,pIn,&sOne) ){` |
|        - |  703 | `			/* A constant NAME, exactly as the source wrote it. */` |
|        3 |  704 | `			SyBlobAppend(pOut,SyStringData(&sOne),SyStringLength(&sOne));` |
|       12 |  705 | `			return;` |
|        - |  706 | `		}` |
|        5 |  707 | `	}` |
|       22 |  708 | `	if( nLen == 3 ){` |
|        9 |  709 | `		VmInstr *pCls = (VmInstr *)SySetAt(pByteCode,0);` |
|        9 |  710 | `		VmInstr *pMem = (VmInstr *)SySetAt(pByteCode,1);` |
|        9 |  711 | `		VmInstr *pOp  = (VmInstr *)SySetAt(pByteCode,2);` |
|        8 |  712 | `		if( pOp && pOp->iOp == PH7_OP_MEMBER && pOp->iP1 == 1` |
|        2 |  713 | `		 && pOp->iP2 == PH7_MEMBER_READ` |
|        3 |  714 | `		 && OoDeclLiteral(pVm,pCls,&sOne) && OoDeclLiteral(pVm,pMem,&sTwo) ){` |
|        2 |  715 | `			if( sTwo.nByte == sizeof("class")-1` |
|        2 |  716 | `			 && SyStrnicmp(sTwo.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  717 | ``				/* `self::class` -- the only ::class spelling the compiler leaves for`` |
|        - |  718 | `				 * the runtime (a named class folds to its own literal, and lands on` |
|        - |  719 | `				 * the value path below). php folded it too, to the STRING. */` |
|        - |  720 | `				SyBlob sName;` |
|      ! 0 |  721 | `				ph7_class *pCurr = 0;` |
|      ! 0 |  722 | `				if( pScope ){` |
|      ! 0 |  723 | `					pCurr = (sOne.nByte == sizeof("parent")-1` |
|      ! 0 |  724 | `						&& SyStrnicmp(sOne.zString,"parent",sizeof("parent")-1) == 0)` |
|      ! 0 |  725 | `						? pScope->pBase : pScope;` |
|      ! 0 |  726 | `				}` |
|      ! 0 |  727 | `				if( pCurr ){` |
|      ! 0 |  728 | `					SyBlobInit(&sName,&pVm->sAllocator);` |
|      ! 0 |  729 | `					SyBlobAppend(&sName,"'",sizeof(char));` |
|      ! 0 |  730 | `					SyBlobAppend(&sName,SyStringData(&pCurr->sName),SyStringLength(&pCurr->sName));` |
|      ! 0 |  731 | `					SyBlobAppend(&sName,"'",sizeof(char));` |
|      ! 0 |  732 | `					SyBlobAppend(pOut,SyBlobData(&sName),SyBlobLength(&sName));` |
|      ! 0 |  733 | `					SyBlobRelease(&sName);` |
|      ! 0 |  734 | `					return;` |
|        - |  735 | `				}` |
|      ! 0 |  736 | `			}else{` |
|        3 |  737 | `				SyBlobAppend(pOut,SyStringData(&sOne),SyStringLength(&sOne));` |
|        3 |  738 | `				SyBlobAppend(pOut,"::",sizeof("::")-1);` |
|        3 |  739 | `				SyBlobAppend(pOut,SyStringData(&sTwo),SyStringLength(&sTwo));` |
|        3 |  740 | `				return;` |
|        - |  741 | `			}` |
|      ! 0 |  742 | `		}` |
|        3 |  743 | `	}` |
|       20 |  744 | `	if( OoDeclDefFoldable(pByteCode,nLen) ){` |
|        - |  745 | `		ph7_value sVal;` |
|        - |  746 | `		int bFolded;` |
|       18 |  747 | `		PH7_MemObjInit(pVm,&sVal);` |
|       18 |  748 | `		bFolded = PH7_VmEvalConstExpr(pVm,pByteCode,&sVal);` |
|       18 |  749 | `		if( bFolded ){` |
|       18 |  750 | `			OoDeclValue(pOut,&sVal);` |
|        8 |  751 | `		}` |
|       18 |  752 | `		PH7_MemObjRelease(&sVal);` |
|       18 |  753 | `		if( bFolded ){` |
|       18 |  754 | `			return;` |
|        - |  755 | `		}` |
|      ! 0 |  756 | `	}` |
|        3 |  757 | `	SyBlobAppend(pOut,"<expression>",sizeof("<expression>")-1);` |
|       13 |  758 | `}` |
|        - |  759 | `/*` |
|        - |  760 | ` * php hands each rendered declaration to its error formatter as a C STRING, so a` |
|        - |  761 | `` * declaration carrying a NUL byte -- `function f($a = "\0")` -- is cut there and`` |
|        - |  762 | `` * the sentence carries on with what follows it (`A::f($a = '` and then ` in ... on`` |
|        - |  763 | `` * line N`). Reproduced rather than left as a whole-blob write, which is the one`` |
|        - |  764 | ` * shape where the two engines would disagree byte for byte.` |
|        - |  765 | ` */` |
|       60 |  766 | `static int OoDeclCLen(SyBlob *pDecl)` |
|        4 |  767 | `{` |
|       64 |  768 | `	const char *z = (const char *)SyBlobData(pDecl);` |
|       64 |  769 | `	sxu32 n = SyBlobLength(pDecl), i;` |
|     1358 |  770 | `	for( i = 0 ; i < n ; ++i ){` |
|     1298 |  771 | `		if( z[i] == 0 ){` |
|      ! 0 |  772 | `			return (int)i;` |
|        - |  773 | `		}` |
|      651 |  774 | `	}` |
|       64 |  775 | `	return (int)n;` |
|       34 |  776 | `}` |
|        - |  777 | `/*` |
|        - |  778 | `` * `(int $a, ?string $b = null): string` -- everything php prints after the`` |
|        - |  779 | ` * method's name. pScope is the class the declaration was written FOR (a trait` |
|        - |  780 | ``  * method's composing class, not the trait), which is what `self` and `parent` `` |
|        - |  781 | ` * resolve against.` |
|        - |  782 | ` */` |
|       60 |  783 | `PH7_PRIVATE void PH7_ClassRenderDecl(ph7_vm *pVm,ph7_class *pScope,ph7_vm_func *pFunc,SyBlob *pOut)` |
|        4 |  784 | `{` |
|       64 |  785 | `	ph7_vm_func_arg *aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       64 |  786 | `	sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|        - |  787 | `	sxu32 i;` |
|       64 |  788 | `	SyBlobAppend(pOut,"(",sizeof(char));` |
|      130 |  789 | `	for( i = 0 ; i < nArg ; ++i ){` |
|       70 |  790 | `		if( i > 0 ){` |
|       44 |  791 | `			SyBlobAppend(pOut,", ",sizeof(", ")-1);` |
|       20 |  792 | `		}` |
|       70 |  793 | `		if( SyStringLength(&aArgs[i].sTypeName) > 0 ){` |
|       37 |  794 | `			OoDeclType(pScope,&aArgs[i].sTypeName,pOut);` |
|       37 |  795 | `			SyBlobAppend(pOut," ",sizeof(char));` |
|       17 |  796 | `		}` |
|       70 |  797 | `		if( aArgs[i].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        3 |  798 | `			SyBlobAppend(pOut,"&",sizeof(char));` |
|        1 |  799 | `		}` |
|       70 |  800 | `		if( aArgs[i].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        3 |  801 | `			SyBlobAppend(pOut,"...",sizeof("...")-1);` |
|        1 |  802 | `		}` |
|       70 |  803 | `		SyBlobAppend(pOut,"$",sizeof(char));` |
|       70 |  804 | `		SyBlobAppend(pOut,SyStringData(&aArgs[i].sName),SyStringLength(&aArgs[i].sName));` |
|       70 |  805 | `		if( SySetUsed(&aArgs[i].aByteCode) > 0 ){` |
|       24 |  806 | `			SyBlobAppend(pOut," = ",sizeof(" = ")-1);` |
|       24 |  807 | `			OoDeclDefault(pVm,pScope,&aArgs[i].aByteCode,pOut);` |
|       11 |  808 | `		}` |
|       37 |  809 | `	}` |
|       64 |  810 | `	SyBlobAppend(pOut,")",sizeof(char));` |
|       64 |  811 | `	if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|       48 |  812 | `		SyBlobAppend(pOut,": ",sizeof(": ")-1);` |
|       48 |  813 | `		OoDeclType(pScope,&pFunc->sReturnTypeName,pOut);` |
|       22 |  814 | `	}` |
|       64 |  815 | `}` |
|        - |  816 | `/*` |
|        - |  817 | ` * ---------------------------------------------------------------------------` |
|        - |  818 | ` * Method-override compatibility: php's declared-type LATTICE.` |
|        - |  819 | ` *` |
|        - |  820 | ` * php rejects an override whose signature is incompatible with the parent's --` |
|        - |  821 | ` * a return type is COVARIANT (the child may only narrow), a parameter type is` |
|        - |  822 | ` * CONTRAVARIANT (the child may only widen), and the arity/by-reference shape` |
|        - |  823 | ` * must let every call the parent accepts reach the child. This used to be a` |
|        - |  824 | ` * deliberately SKIP-BY-DEFAULT approximation: it decided a bare scalar against a` |
|        - |  825 | ` * bare scalar and a loaded class against a loaded class, and accepted everything` |
|        - |  826 | `` * subtle -- a union, an intersection, `mixed`, `object`, `iterable`, `void`,`` |
|        - |  827 | `` * `never`, `self`/`static`, or a variadic signature. Fourteen shapes php refuses`` |
|        - |  828 | ` * compiled here in silence.` |
|        - |  829 | ` *` |
|        - |  830 | ` * The lattice below is php's, derived from the oracle: a declared type is a` |
|        - |  831 | ` * DISJUNCTION of intersection GROUPS, each group a conjunction of ATOMS, and` |
|        - |  832 | ` *` |
|        - |  833 | ` *     child ⊆ parent   iff   every child group is a subtype of SOME parent group` |
|        - |  834 | ` *     Gc ⊆ Gp          iff   every atom of Gp has SOME atom of Gc under it` |
|        - |  835 | ` *` |
|        - |  836 | ` * which is all a plain union, an intersection and a DNF type need between them.` |
|        - |  837 | `` * `bUnknown` is what is left of the old skip: a shape this cannot model (a type`` |
|        - |  838 | `` * naming a class no autoload-free lookup finds, `parent` with no base, more`` |
|        - |  839 | ` * atoms than the bound) is still ACCEPTED, because refusing valid php is the` |
|        - |  840 | ` * one failure mode that matters here.` |
|        - |  841 | ` * ---------------------------------------------------------------------------` |
|        - |  842 | ` */` |
|        - |  843 | `#define OVB_INT      0x0001` |
|        - |  844 | `#define OVB_FLOAT    0x0002` |
|        - |  845 | `#define OVB_STRING   0x0004` |
|        - |  846 | `#define OVB_BOOL     0x0008` |
|        - |  847 | `#define OVB_FALSE    0x0010` |
|        - |  848 | `#define OVB_TRUE     0x0020` |
|        - |  849 | `#define OVB_ARRAY    0x0040` |
|        - |  850 | ``#define OVB_OBJECT   0x0080  /* the `object` pseudo-type: every class at once */`` |
|        - |  851 | `#define OVB_CALLABLE 0x0100` |
|        - |  852 | `#define OVB_NULL     0x0200` |
|        - |  853 | `#define OVB_VOID     0x0400` |
|        - |  854 | ``#define OVB_STATIC   0x0800  /* `static`: the CALLED class of the declaring one */`` |
|        - |  855 | `#define OVB_CLS      0x1000  /* a named class/interface, resolved into pCls */` |
|        - |  856 | `#define OV_MAX_ATOM  16      /* bounds the on-stack atom array; over it, bUnknown */` |
|        - |  857 |  |
|        - |  858 | `typedef struct OvAtom OvAtom;` |
|        - |  859 | `struct OvAtom {` |
|        - |  860 | `	sxu32 nBit;      /* OVB_* */` |
|        - |  861 | `	ph7_class *pCls; /* the class, when nBit == OVB_CLS */` |
|        - |  862 | `	sxu32 nGroup;    /* intersection group: atoms sharing one are ANDed */` |
|        - |  863 | `};` |
|        - |  864 | `typedef struct OvType OvType;` |
|        - |  865 | `struct OvType {` |
|        - |  866 | `	int bAbsent;  /* no declared type at all -- not a type, an ABSENCE (see below) */` |
|        - |  867 | ``	int bMixed;   /* `mixed`: the top type */`` |
|        - |  868 | ``	int bNever;   /* `never`: the bottom type, a subtype of everything */`` |
|        - |  869 | `	int bUnknown; /* a shape this lattice does not model -- accept whatever it meets */` |
|        - |  870 | `	int nAtom;` |
|        - |  871 | `	sxu32 nNextGroup;` |
|        - |  872 | `	OvAtom a[OV_MAX_ATOM];` |
|        - |  873 | `};` |
|      760 |  874 | `static void OvInit(OvType *pT)` |
|        5 |  875 | `{` |
|      765 |  876 | `	SyZero(pT,sizeof(*pT));` |
|      765 |  877 | `}` |
|      380 |  878 | `static void OvAddAtom(OvType *pT,sxu32 nBit,ph7_class *pCls,sxu32 nGroup)` |
|        5 |  879 | `{` |
|      385 |  880 | `	if( pT->nAtom >= OV_MAX_ATOM ){` |
|      ! 0 |  881 | `		pT->bUnknown = 1;` |
|      ! 0 |  882 | `		return;` |
|        - |  883 | `	}` |
|      385 |  884 | `	pT->a[pT->nAtom].nBit = nBit;` |
|      385 |  885 | `	pT->a[pT->nAtom].pCls = pCls;` |
|      385 |  886 | `	pT->a[pT->nAtom].nGroup = nGroup;` |
|      385 |  887 | `	pT->nAtom++;` |
|      385 |  888 | `	if( nGroup >= pT->nNextGroup ){` |
|      365 |  889 | `		pT->nNextGroup = nGroup + 1;` |
|      180 |  890 | `	}` |
|      195 |  891 | `}` |
|        - |  892 | `/* One atom written as a NAME: a class, or one of the pseudo-types php parses as` |
|        - |  893 | `` * a class-name atom. `iterable` is TWO types, so it contributes two atoms in two`` |
|        - |  894 | ` * groups -- it is a union, never an intersection member (php forbids the latter). */` |
|       90 |  895 | `static void OvAddName(ph7_vm *pVm,ph7_class *pScope,OvType *pT,const SyString *pName,sxu32 nGroup)` |
|        5 |  896 | `{` |
|        - |  897 | `	static const struct { const char *z; sxu32 n; sxu32 nBit; } aWord[] = {` |
|        - |  898 | `		{ "callable",8, OVB_CALLABLE }, { "false",5, OVB_FALSE }, { "true",4, OVB_TRUE },` |
|        - |  899 | `		{ "object",6, OVB_OBJECT },     { "null",4,  OVB_NULL },  { "void",4,  OVB_VOID },` |
|        - |  900 | `		{ "static",6, OVB_STATIC },     { "int",3,   OVB_INT },   { "float",5, OVB_FLOAT },` |
|        - |  901 | `		{ "string",6, OVB_STRING },     { "bool",4,  OVB_BOOL },  { "array",5, OVB_ARRAY }` |
|        - |  902 | `	};` |
|       95 |  903 | `	const char *z = SyStringData(pName);` |
|       95 |  904 | `	sxu32 n = SyStringLength(pName), i;` |
|        - |  905 | `	SyHashEntry *pE;` |
|       95 |  906 | `	if( n < 1 ){` |
|      ! 0 |  907 | `		pT->bUnknown = 1;` |
|      ! 0 |  908 | `		return;` |
|        - |  909 | `	}` |
|       95 |  910 | `	if( n == sizeof("mixed")-1 && SyStrnicmp(z,"mixed",n) == 0 ){` |
|        5 |  911 | `		pT->bMixed = 1;` |
|        5 |  912 | `		return;` |
|        - |  913 | `	}` |
|       91 |  914 | `	if( n == sizeof("never")-1 && SyStrnicmp(z,"never",n) == 0 ){` |
|      ! 0 |  915 | `		pT->bNever = 1;` |
|      ! 0 |  916 | `		return;` |
|        - |  917 | `	}` |
|       91 |  918 | `	if( n == sizeof("iterable")-1 && SyStrnicmp(z,"iterable",n) == 0 ){` |
|        3 |  919 | `		ph7_class *pTrav = PH7_VmExtractClass(pVm,"Traversable",sizeof("Traversable")-1,FALSE,0);` |
|        3 |  920 | `		if( pTrav == 0 ){` |
|      ! 0 |  921 | `			pT->bUnknown = 1;` |
|      ! 0 |  922 | `			return;` |
|        - |  923 | `		}` |
|        3 |  924 | `		OvAddAtom(pT,OVB_ARRAY,0,pT->nNextGroup);` |
|        3 |  925 | `		OvAddAtom(pT,OVB_CLS,pTrav,pT->nNextGroup);` |
|        3 |  926 | `		return;` |
|        - |  927 | `	}` |
|      895 |  928 | `	for( i = 0 ; i < SX_ARRAYSIZE(aWord) ; ++i ){` |
|      843 |  929 | `		if( n == aWord[i].n && SyStrnicmp(z,aWord[i].z,n) == 0 ){` |
|       36 |  930 | `			OvAddAtom(pT,aWord[i].nBit,0,nGroup);` |
|       36 |  931 | `			return;` |
|        - |  932 | `		}` |
|      408 |  933 | `	}` |
|       57 |  934 | `	if( n == sizeof("self")-1 && SyStrnicmp(z,"self",n) == 0 ){` |
|       25 |  935 | `		if( pScope == 0 ){` |
|      ! 0 |  936 | `			pT->bUnknown = 1;` |
|      ! 0 |  937 | `			return;` |
|        - |  938 | `		}` |
|       25 |  939 | `		OvAddAtom(pT,OVB_CLS,pScope,nGroup);` |
|       25 |  940 | `		return;` |
|        - |  941 | `	}` |
|       35 |  942 | `	if( n == sizeof("parent")-1 && SyStrnicmp(z,"parent",n) == 0 ){` |
|      ! 0 |  943 | `		if( pScope == 0 \|\| pScope->pBase == 0 ){` |
|      ! 0 |  944 | `			pT->bUnknown = 1;` |
|      ! 0 |  945 | `			return;` |
|        - |  946 | `		}` |
|      ! 0 |  947 | `		OvAddAtom(pT,OVB_CLS,pScope->pBase,nGroup);` |
|      ! 0 |  948 | `		return;` |
|        - |  949 | `	}` |
|        - |  950 | `	/* A real class name, resolved WITHOUT autoloading: a miss is a forward` |
|        - |  951 | `	 * reference or a class no lookup can produce, and the whole type becomes` |
|        - |  952 | `	 * undecidable rather than wrong. */` |
|       35 |  953 | `	pE = SyHashGet(&pVm->hClass,(const void *)z,n);` |
|       35 |  954 | `	if( pE == 0 ){` |
|      ! 0 |  955 | `		pT->bUnknown = 1;` |
|      ! 0 |  956 | `		return;` |
|        - |  957 | `	}` |
|       35 |  958 | `	OvAddAtom(pT,OVB_CLS,(ph7_class *)pE->pUserData,nGroup);` |
|       50 |  959 | `}` |
|        - |  960 | `/* One atom given as a MEMOBJ_* code plus, for SXU32_HIGH, its name. */` |
|      370 |  961 | `static void OvAddCode(ph7_vm *pVm,ph7_class *pScope,OvType *pT,sxu32 nType,` |
|        - |  962 | `	const SyString *pName,sxu32 nGroup)` |
|        5 |  963 | `{` |
|      375 |  964 | `	switch( nType ){` |
|      119 |  965 | `	case MEMOBJ_INT:     OvAddAtom(pT,OVB_INT,0,nGroup);    return;` |
|      ! 0 |  966 | `	case MEMOBJ_REAL:    OvAddAtom(pT,OVB_FLOAT,0,nGroup);  return;` |
|       98 |  967 | `	case MEMOBJ_STRING:  OvAddAtom(pT,OVB_STRING,0,nGroup); return;` |
|        8 |  968 | `	case MEMOBJ_BOOL:    OvAddAtom(pT,OVB_BOOL,0,nGroup);   return;` |
|        3 |  969 | `	case MEMOBJ_HASHMAP: OvAddAtom(pT,OVB_ARRAY,0,nGroup);  return;` |
|        5 |  970 | `	case MEMOBJ_OBJ:     OvAddAtom(pT,OVB_OBJECT,0,nGroup); return;` |
|       63 |  971 | `	case MEMOBJ_VOID:    OvAddAtom(pT,OVB_VOID,0,nGroup);   return;` |
|        3 |  972 | `	case MEMOBJ_NEVER:   pT->bNever = 1;                    return;` |
|        - |  973 | ``	/* php 8.2's standalone `null`, which the parser records BOTH as this code and`` |
|        - |  974 | `	 * as the nullable flag; the second atom the flag adds is the same type. */` |
|      ! 0 |  975 | `	case MEMOBJ_NULL:    OvAddAtom(pT,OVB_NULL,0,nGroup);   return;` |
|       90 |  976 | `	default: break;` |
|        - |  977 | `	}` |
|       95 |  978 | `	if( nType == SXU32_HIGH ){` |
|       95 |  979 | `		OvAddName(pVm,pScope,pT,pName,nGroup);` |
|       95 |  980 | `		return;` |
|        - |  981 | `	}` |
|      ! 0 |  982 | `	pT->bUnknown = 1;` |
|      190 |  983 | `}` |
|        - |  984 | `/*` |
|        - |  985 | ` * The union alternatives, or the single type, of one declaration.` |
|        - |  986 | ` *` |
|        - |  987 | ` * The stored intersection-group ids are RE-MAPPED rather than used as they come:` |
|        - |  988 | `` * `iterable` is an alternative that expands into TWO groups of its own, so a`` |
|        - |  989 | ` * later alternative's stored id would otherwise land in the group Traversable` |
|        - |  990 | `` * had just been given and read as `Traversable&int`. A group with more than one`` |
|        - |  991 | `` * member is an intersection, which `iterable` may not appear in at all (php`` |
|        - |  992 | `` * refuses `iterable&X`) -- if one ever did, the whole type is undecidable.`` |
|        - |  993 | ` */` |
|      760 |  994 | `static void OvFromDecl(ph7_vm *pVm,ph7_class *pScope,OvType *pT,sxu32 nType,` |
|        - |  995 | `	const SyString *pClass,SySet *pAlts,int bNullable)` |
|        5 |  996 | `{` |
|      765 |  997 | `	OvInit(pT);` |
|      765 |  998 | `	if( SySetUsed(pAlts) > 0 ){` |
|       13 |  999 | `		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(pAlts);` |
|        - | 1000 | `		sxu32 aMap[PHL_UNION_MAX_ALTS];` |
|        - | 1001 | `		sxu32 aCount[PHL_UNION_MAX_ALTS];` |
|       13 | 1002 | `		sxu32 i, n = SySetUsed(pAlts);` |
|      333 | 1003 | `		for( i = 0 ; i < PHL_UNION_MAX_ALTS ; ++i ){` |
|      323 | 1004 | `			aMap[i] = SXU32_HIGH;` |
|      323 | 1005 | `			aCount[i] = 0;` |
|      163 | 1006 | `		}` |
|       33 | 1007 | `		for( i = 0 ; i < n ; ++i ){` |
|       23 | 1008 | `			if( aAlt[i].nGroup >= PHL_UNION_MAX_ALTS ){` |
|      ! 0 | 1009 | `				pT->bUnknown = 1;` |
|      ! 0 | 1010 | `				return;` |
|        - | 1011 | `			}` |
|       23 | 1012 | `			aCount[aAlt[i].nGroup]++;` |
|       13 | 1013 | `		}` |
|       33 | 1014 | `		for( i = 0 ; i < n ; ++i ){` |
|       23 | 1015 | `			sxu32 g = aAlt[i].nGroup;` |
|       37 | 1016 | `			int bIter = ( aAlt[i].nType == SXU32_HIGH` |
|       14 | 1017 | `				&& SyStringLength(&aAlt[i].sClass) == sizeof("iterable")-1` |
|       24 | 1018 | `				&& SyStrnicmp(SyStringData(&aAlt[i].sClass),"iterable",sizeof("iterable")-1) == 0 );` |
|       23 | 1019 | `			if( bIter && aCount[g] > 1 ){` |
|      ! 0 | 1020 | `				pT->bUnknown = 1;` |
|      ! 0 | 1021 | `				return;` |
|        - | 1022 | `			}` |
|       23 | 1023 | `			if( aMap[g] == SXU32_HIGH ){` |
|       21 | 1024 | `				aMap[g] = pT->nNextGroup;` |
|       21 | 1025 | `				pT->nNextGroup++;` |
|        9 | 1026 | `			}` |
|       23 | 1027 | `			OvAddCode(pVm,pScope,pT,aAlt[i].nType,&aAlt[i].sClass,aMap[g]);` |
|       13 | 1028 | `		}` |
|      758 | 1029 | `	}else if( nType != 0 ){` |
|      355 | 1030 | `		OvAddCode(pVm,pScope,pT,nType,pClass,pT->nNextGroup);` |
|      175 | 1031 | `	}` |
|      765 | 1032 | `	if( bNullable ){` |
|       17 | 1033 | `		OvAddAtom(pT,OVB_NULL,0,pT->nNextGroup);` |
|        7 | 1034 | `	}` |
|        - | 1035 | ``	/* Nothing written at all: an ABSENCE, which is not the same as `mixed` --`` |
|        - | 1036 | `	 * php skips the check on the side that has none, so a missing PARAMETER type` |
|        - | 1037 | `	 * accepts any parent and a missing RETURN type is refused under a declared` |
|        - | 1038 | ``	 * one (`f(): int` overridden by `f()` is a fatal, `f(): mixed` is not). */`` |
|      765 | 1039 | `	if( pT->nAtom == 0 && !pT->bMixed && !pT->bNever && !pT->bUnknown ){` |
|      405 | 1040 | `		pT->bAbsent = 1;` |
|      200 | 1041 | `	}` |
|      385 | 1042 | `}` |
|      136 | 1043 | `static void OvFromArg(ph7_vm *pVm,ph7_class *pScope,ph7_vm_func_arg *pA,OvType *pT)` |
|        5 | 1044 | `{` |
|      209 | 1045 | `	OvFromDecl(pVm,pScope,pT,pA->nType,&pA->sClass,&pA->aUnionAlts,` |
|      136 | 1046 | `		(pA->iFlags & VM_FUNC_ARG_NULLABLE) != 0);` |
|      141 | 1047 | `}` |
|      624 | 1048 | `static void OvFromReturn(ph7_vm *pVm,ph7_class *pScope,ph7_vm_func *pF,OvType *pT)` |
|        5 | 1049 | `{` |
|      941 | 1050 | `	OvFromDecl(pVm,pScope,pT,pF->nReturnType,&pF->sReturnClass,&pF->aReturnUnion,` |
|      624 | 1051 | `		(pF->iFlags & VM_FUNC_RETURN_NULLABLE) != 0);` |
|      629 | 1052 | `}` |
|        - | 1053 | `/*` |
|        - | 1054 | `` * Is the single atom *pC under the single atom *pP? `object` is over every class`` |
|        - | 1055 | `` * (and over `static`, which IS one), `bool` is over `false` and `true`, and`` |
|        - | 1056 | `` * `static` is under any class the declaring class is an instance of -- but`` |
|        - | 1057 | `` * nothing except another `static` is under IT, since the called class may be a`` |
|        - | 1058 | ` * subclass nobody has written yet.` |
|        - | 1059 | ` */` |
|      192 | 1060 | `static int OvAtomLE(const OvAtom *pC,const OvAtom *pP,ph7_class *pSubScope)` |
|        5 | 1061 | `{` |
|      197 | 1062 | `	if( pP->nBit == OVB_OBJECT ){` |
|        3 | 1063 | `		return pC->nBit == OVB_OBJECT \|\| pC->nBit == OVB_CLS \|\| pC->nBit == OVB_STATIC;` |
|        - | 1064 | `	}` |
|      195 | 1065 | `	if( pP->nBit == OVB_BOOL ){` |
|        6 | 1066 | `		return pC->nBit == OVB_BOOL \|\| pC->nBit == OVB_FALSE \|\| pC->nBit == OVB_TRUE;` |
|        - | 1067 | `	}` |
|      191 | 1068 | `	if( pP->nBit == OVB_STATIC ){` |
|       30 | 1069 | `		if( pC->nBit == OVB_STATIC ){` |
|        3 | 1070 | `			return 1;` |
|        - | 1071 | `		}` |
|        - | 1072 | `		/* php's one exception, and a library really writes it: in a FINAL class` |
|        - | 1073 | ``		 * `self` IS `static`, because no subclass can ever exist for the called`` |
|        - | 1074 | `		 * class to be. It is the class ITSELF and nothing else -- naming the` |
|        - | 1075 | `		 * PARENT is still a fatal, even from a final child (an enum carries the` |
|        - | 1076 | `		 * final flag, so its own name works the same way). */` |
|       26 | 1077 | `		if( pC->nBit == OVB_CLS && pSubScope != 0 && pC->pCls == pSubScope` |
|       20 | 1078 | `		 && (pSubScope->iFlags & PH7_CLASS_FINAL) != 0 ){` |
|       17 | 1079 | `			return 1;` |
|        - | 1080 | `		}` |
|       11 | 1081 | `		return 0;` |
|        - | 1082 | `	}` |
|      163 | 1083 | `	if( pP->nBit == OVB_CLS ){` |
|       26 | 1084 | `		if( pC->nBit == OVB_CLS ){` |
|       18 | 1085 | `			return PH7_VmInstanceOf(pC->pCls,pP->pCls) ? 1 : 0;` |
|        - | 1086 | `		}` |
|       11 | 1087 | `		if( pC->nBit == OVB_STATIC ){` |
|        8 | 1088 | `			return (pSubScope && PH7_VmInstanceOf(pSubScope,pP->pCls)) ? 1 : 0;` |
|        - | 1089 | `		}` |
|        3 | 1090 | `		return 0;` |
|        - | 1091 | `	}` |
|      141 | 1092 | `	return pC->nBit == pP->nBit;` |
|      101 | 1093 | `}` |
|        - | 1094 | `/* Gc ⊆ Gp: every atom of the parent group has some atom of the child group under` |
|        - | 1095 | ` * it (an intersection is under X as soon as ONE of its members is). */` |
|      190 | 1096 | `static int OvGroupLE(const OvType *pC,sxu32 gC,const OvType *pP,sxu32 gP,ph7_class *pSubScope)` |
|        5 | 1097 | `{` |
|        - | 1098 | `	int i, j;` |
|      385 | 1099 | `	for( j = 0 ; j < pP->nAtom ; ++j ){` |
|      219 | 1100 | `		int bCovered = 0;` |
|      219 | 1101 | `		if( pP->a[j].nGroup != gP ){` |
|       26 | 1102 | `			continue;` |
|        - | 1103 | `		}` |
|      407 | 1104 | `		for( i = 0 ; i < pC->nAtom && !bCovered ; ++i ){` |
|      215 | 1105 | `			if( pC->a[i].nGroup == gC && OvAtomLE(&pC->a[i],&pP->a[j],pSubScope) ){` |
|      173 | 1106 | `				bCovered = 1;` |
|       84 | 1107 | `			}` |
|      110 | 1108 | `		}` |
|      197 | 1109 | `		if( !bCovered ){` |
|       29 | 1110 | `			return 0;` |
|        - | 1111 | `		}` |
|       89 | 1112 | `	}` |
|      171 | 1113 | `	return 1;` |
|      100 | 1114 | `}` |
|        - | 1115 | `/* child ⊆ parent (pSubScope is the SUBTYPE side's declaring class, which is what` |
|        - | 1116 | `` * a `static` atom there stands for). Both are normalized and neither is`` |
|        - | 1117 | ` * absent/mixed/never/unknown -- OvCheck settled those. */` |
|      172 | 1118 | `static int OvSubtype(const OvType *pC,const OvType *pP,ph7_class *pSubScope)` |
|        5 | 1119 | `{` |
|        - | 1120 | `	sxu32 gC, gP;` |
|      177 | 1121 | `	int bAnyC = 0;` |
|      343 | 1122 | `	for( gC = 0 ; gC < pC->nNextGroup ; ++gC ){` |
|      187 | 1123 | `		int i, bHasC = 0, bCovered = 0;` |
|      197 | 1124 | `		for( i = 0 ; i < pC->nAtom ; ++i ){` |
|      197 | 1125 | `			if( pC->a[i].nGroup == gC ){ bHasC = 1; break; }` |
|        8 | 1126 | `		}` |
|      187 | 1127 | `		if( !bHasC ){` |
|      ! 0 | 1128 | `			continue;` |
|        - | 1129 | `		}` |
|      187 | 1130 | `		bAnyC = 1;` |
|      377 | 1131 | `		for( gP = 0 ; gP < pP->nNextGroup && !bCovered ; ++gP ){` |
|      195 | 1132 | `			int j, bHasP = 0;` |
|      203 | 1133 | `			for( j = 0 ; j < pP->nAtom ; ++j ){` |
|      203 | 1134 | `				if( pP->a[j].nGroup == gP ){ bHasP = 1; break; }` |
|        7 | 1135 | `			}` |
|      195 | 1136 | `			if( bHasP && OvGroupLE(pC,gC,pP,gP,pSubScope) ){` |
|      171 | 1137 | `				bCovered = 1;` |
|       83 | 1138 | `			}` |
|      100 | 1139 | `		}` |
|      187 | 1140 | `		if( !bCovered ){` |
|       20 | 1141 | `			return 0;` |
|        - | 1142 | `		}` |
|       88 | 1143 | `	}` |
|      161 | 1144 | `	return bAnyC;` |
|       91 | 1145 | `}` |
|        - | 1146 | `#define OV_OK   0 /* the pair is compatible */` |
|        - | 1147 | `#define OV_BAD  1 /* php refuses it */` |
|        - | 1148 | `/*` |
|        - | 1149 | ` * One declared-type pair, in one variance direction. bCovariant = 1 for a return` |
|        - | 1150 | ` * type (the child must be UNDER the parent), 0 for a parameter (over it).` |
|        - | 1151 | ` *` |
|        - | 1152 | ` * The two ABSENCES are asymmetric and that asymmetry is php's: the side with no` |
|        - | 1153 | ` * declared type is simply not checked, so a parameter the CHILD left untyped is` |
|        - | 1154 | ` * always fine and a return the child left untyped is a fatal under any declared` |
|        - | 1155 | `` * parent -- `mixed` included, even though `mixed` is the top type.`` |
|        - | 1156 | ` */` |
|      380 | 1157 | `static int OvCheck(const OvType *pP,const OvType *pC,int bCovariant,` |
|        - | 1158 | `	ph7_class *pParentScope,ph7_class *pChildScope)` |
|        5 | 1159 | `{` |
|      385 | 1160 | `	const OvType *pSub = bCovariant ? pC : pP;   /* must be the subtype */` |
|      385 | 1161 | `	const OvType *pSup = bCovariant ? pP : pC;` |
|        - | 1162 | ``	/* `static` is decided against the scope of whichever side is the SUBTYPE --`` |
|        - | 1163 | `	 * the class whose called-class it stands for. */` |
|      385 | 1164 | `	ph7_class *pSubScope = bCovariant ? pChildScope : pParentScope;` |
|        - | 1165 | `	int bSubVoid, bSupVoid, i;` |
|      385 | 1166 | `	if( bCovariant && pP->bAbsent ){` |
|      171 | 1167 | `		return OV_OK;   /* nothing to be under */` |
|        - | 1168 | `	}` |
|      219 | 1169 | `	if( bCovariant && pC->bAbsent ){` |
|        - | 1170 | `		/* The ONE place an absent type is not simply the top type: a child that` |
|        - | 1171 | `		 * declares no RETURN type is refused under any parent that declares one,` |
|        - | 1172 | ``		 * `mixed` included. Everywhere else absence reads as `mixed` below. */`` |
|      ! 0 | 1173 | `		return OV_BAD;` |
|        - | 1174 | `	}` |
|      219 | 1175 | `	if( !bCovariant && pC->bAbsent ){` |
|       40 | 1176 | `		return OV_OK;   /* an untyped parameter accepts whatever the parent's did */` |
|        - | 1177 | `	}` |
|      183 | 1178 | `	if( pP->bUnknown \|\| pC->bUnknown ){` |
|      ! 0 | 1179 | `		return OV_OK;   /* a shape this lattice does not model -- accept */` |
|        - | 1180 | `	}` |
|        - | 1181 | ``	/* `void` pairs with `void` and with nothing else -- not even with `mixed`,`` |
|        - | 1182 | `	 * which is over every other type. Decided before the top/bottom shortcuts. */` |
|      183 | 1183 | `	bSubVoid = bSupVoid = 0;` |
|      367 | 1184 | `	for( i = 0 ; i < pSub->nAtom ; ++i ){ if( pSub->a[i].nBit == OVB_VOID ) bSubVoid = 1; }` |
|      375 | 1185 | `	for( i = 0 ; i < pSup->nAtom ; ++i ){ if( pSup->a[i].nBit == OVB_VOID ) bSupVoid = 1; }` |
|      183 | 1186 | `	if( bSubVoid != bSupVoid && !pSub->bNever ){` |
|        3 | 1187 | `		return OV_BAD;` |
|        - | 1188 | `	}` |
|      181 | 1189 | `	if( pSub->bNever ){` |
|        3 | 1190 | `		return OV_OK;   /* the bottom type is under everything */` |
|        - | 1191 | `	}` |
|      179 | 1192 | `	if( pSup->bMixed \|\| pSup->bAbsent ){` |
|        3 | 1193 | `		return OV_OK;   /* ...and the top type is over everything */` |
|        - | 1194 | `	}` |
|      177 | 1195 | `	if( pSub->bMixed \|\| pSub->bAbsent \|\| pSup->bNever ){` |
|      ! 0 | 1196 | `		return OV_BAD;` |
|        - | 1197 | `	}` |
|      177 | 1198 | `	return OvSubtype(pSub,pSup,pSubScope) ? OV_OK : OV_BAD;` |
|      195 | 1199 | `}` |
|        - | 1200 | `/*` |
|        - | 1201 | ` * ---------------------------------------------------------------------------` |
|        - | 1202 | ` * The ARITY half, which is not the type lattice's: php asks whether every call` |
|        - | 1203 | ` * the parent's declaration accepts can reach the child.` |
|        - | 1204 | ` * ---------------------------------------------------------------------------` |
|        - | 1205 | ` */` |
|        - | 1206 | `/* php's required_num_args: how many arguments a caller MUST supply. */` |
|      592 | 1207 | `static sxu32 OvReqArgs(ph7_vm_func *pF)` |
|        5 | 1208 | `{` |
|      597 | 1209 | `	ph7_vm_func_arg *a = (ph7_vm_func_arg *)SySetBasePtr(&pF->aArgs);` |
|      597 | 1210 | `	sxu32 n = SySetUsed(&pF->aArgs), i, nReq = 0;` |
|      793 | 1211 | `	for( i = 0 ; i < n ; ++i ){` |
|      201 | 1212 | `		if( (a[i].iFlags & VM_FUNC_ARG_VARIADIC) \|\| SySetUsed(&a[i].aByteCode) > 0 ){` |
|       56 | 1213 | `			continue; /* a variadic tail and a defaulted parameter are both optional */` |
|        - | 1214 | `		}` |
|      149 | 1215 | `		nReq = i + 1;` |
|       77 | 1216 | `	}` |
|      597 | 1217 | `	return nReq;` |
|        5 | 1218 | `}` |
|      624 | 1219 | `static int OvIsVariadic(ph7_vm_func *pF)` |
|        5 | 1220 | `{` |
|      629 | 1221 | `	ph7_vm_func_arg *a = (ph7_vm_func_arg *)SySetBasePtr(&pF->aArgs);` |
|      629 | 1222 | `	sxu32 n = SySetUsed(&pF->aArgs);` |
|      629 | 1223 | `	return n > 0 && (a[n-1].iFlags & VM_FUNC_ARG_VARIADIC) != 0;` |
|        5 | 1224 | `}` |
|        - | 1225 | `/* The parameter that ANSWERS position i: the one declared there, or the variadic` |
|        - | 1226 | ` * tail, which keeps answering for every position past its own. */` |
|      156 | 1227 | `static ph7_vm_func_arg * OvArgAt(ph7_vm_func *pF,sxu32 i)` |
|        5 | 1228 | `{` |
|      161 | 1229 | `	ph7_vm_func_arg *a = (ph7_vm_func_arg *)SySetBasePtr(&pF->aArgs);` |
|      161 | 1230 | `	sxu32 n = SySetUsed(&pF->aArgs);` |
|      161 | 1231 | `	if( i < n ){` |
|      149 | 1232 | `		return &a[i];` |
|        - | 1233 | `	}` |
|       15 | 1234 | `	if( n > 0 && (a[n-1].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        5 | 1235 | `		return &a[n-1];` |
|        - | 1236 | `	}` |
|       11 | 1237 | `	return 0;` |
|       83 | 1238 | `}` |
|        - | 1239 | `/*` |
|        - | 1240 | ` * Check a child method's signature against the parent method it overrides.` |
|        - | 1241 | ` * Emits a PHP-style "Declaration of … must be compatible …" fatal on a clear` |
|        - | 1242 | ` * incompatibility.` |
|        - | 1243 | ` *` |
|        - | 1244 | `` * bCtorExempt tells the two regimes php has for `__construct` apart: an`` |
|        - | 1245 | ` * INHERITED constructor is exempt from variance entirely (a child may declare` |
|        - | 1246 | ` * whatever it likes), while one an INTERFACE declares is checked like any other` |
|        - | 1247 | `` * method -- `interface I { __construct(int $a); }` really does constrain every`` |
|        - | 1248 | ` * implementor's constructor.` |
|        - | 1249 | ` */` |
|   613413 | 1250 | `PH7_PRIVATE sxi32 PH7_ClassCheckOverrideCompat(ph7_gen_state *pGen, ph7_class *pBase, ph7_class *pSub,` |
|        - | 1251 | `	ph7_class_method *pParent, ph7_class_method *pChild, int bCtorExempt)` |
|        5 | 1252 | `{` |
|   613418 | 1253 | `	ph7_vm *pVm = pGen->pVm;` |
|   613418 | 1254 | `	ph7_vm_func *pPF = &pParent->sFunc;` |
|   613418 | 1255 | `	ph7_vm_func *pCF = &pChild->sFunc;` |
|   613418 | 1256 | `	SyString *pMName = &pCF->sName;` |
|        - | 1257 | `	/* php names the class that DECLARED each side, not the one the walk reached it` |
|        - | 1258 | ``	 * through: `class A { f() } class B extends A {} class C extends B { f() }` is`` |
|        - | 1259 | ``	 * `C::f() must be compatible with A::f()`, and a trait method belongs to the`` |
|        - | 1260 | `` 	 * class that composed it. That owner is also what `self`, `parent` and `static` `` |
|        - | 1261 | `	 * in either declaration resolve against, so the two questions are one. */` |
|   613418 | 1262 | `	ph7_class *pChildOwner = PH7_VmMemberOwnerClass((ph7_class *)pCF->pUserData,pSub);` |
|   613418 | 1263 | `	ph7_class *pParentOwner = PH7_VmMemberOwnerClass((ph7_class *)pPF->pUserData,pBase);` |
|        - | 1264 | `	sxu32 nPArg, nCArg, nPos, k;` |
|        - | 1265 | `	int bPVar, bCVar;` |
|   613418 | 1266 | `	int bBad = 0;` |
|        - | 1267 | `	OvType sP, sC;` |
|   613418 | 1268 | `	if( pChildOwner == 0 ){` |
|      ! 0 | 1269 | `		pChildOwner = pSub;` |
|      ! 0 | 1270 | `	}` |
|   613418 | 1271 | `	if( pParentOwner == 0 ){` |
|      ! 0 | 1272 | `		pParentOwner = pBase;` |
|      ! 0 | 1273 | `	}` |
|   613413 | 1274 | `	if( bCtorExempt` |
|   612701 | 1275 | `	 && pMName->nByte == sizeof("__construct")-1` |
|   436868 | 1276 | `	 && SyStrnmicmp(pMName->zString,"__construct",pMName->nByte) == 0 ){` |
|   194976 | 1277 | `		return SXRET_OK;` |
|        - | 1278 | `	}` |
|        - | 1279 | `	/*` |
|        - | 1280 | `	 * A NATIVE method declares its parameters in a zSig STRING, so its aArgs set` |
|        - | 1281 | `	 * is empty and there is nothing here to compare against. Reading that as` |
|        - | 1282 | `	 * "declares no parameters" made every override of one incompatible: a user` |
|        - | 1283 | `	 * class extending DOMDocument, a Reflection class or a native enum's own` |
|        - | 1284 | `	 * cases()/from() all fataled on a declaration php accepts. An` |
|        - | 1285 | `	 * engine-declared signature is compatible by construction, on either side.` |
|        - | 1286 | `	 */` |
|   418447 | 1287 | `	if( ((pPF->iFlags \| pCF->iFlags) & VM_FUNC_NATIVE) != 0 ){` |
|   418135 | 1288 | `		return SXRET_OK;` |
|        - | 1289 | `	}` |
|        - | 1290 | `	/* Return type -- covariant. */` |
|      317 | 1291 | `	OvFromReturn(pVm,pParentOwner,pPF,&sP);` |
|      317 | 1292 | `	OvFromReturn(pVm,pChildOwner,pCF,&sC);` |
|      317 | 1293 | `	bBad = OvCheck(&sP,&sC,/* bCovariant */ 1,pParentOwner,pChildOwner) == OV_BAD;` |
|        - | 1294 | `	/*` |
|        - | 1295 | `	 * Arity, php's three rules -- every call the parent's declaration accepts must` |
|        - | 1296 | `	 * reach the child. A VARIADIC signature is not the exception this used to make` |
|        - | 1297 | `` 	 * of it (the whole rule stood aside, so `f(string ...$b)` overridden by `f()` `` |
|        - | 1298 | `	 * compiled): it is the tail that keeps ANSWERING past its own position.` |
|        - | 1299 | `	 */` |
|      317 | 1300 | `	nPArg = SySetUsed(&pPF->aArgs);` |
|      317 | 1301 | `	nCArg = SySetUsed(&pCF->aArgs);` |
|      317 | 1302 | `	bPVar = OvIsVariadic(pPF);` |
|      317 | 1303 | `	bCVar = OvIsVariadic(pCF);` |
|      317 | 1304 | `	if( !bBad && bPVar && !bCVar ){` |
|        3 | 1305 | `		bBad = 1;  /* the parent takes any number; the child must too */` |
|        1 | 1306 | `	}` |
|      317 | 1307 | `	if( !bBad && OvReqArgs(pCF) > OvReqArgs(pPF) ){` |
|        3 | 1308 | `		bBad = 1;  /* the child DEMANDS an argument the parent's callers do not pass */` |
|        1 | 1309 | `	}` |
|      317 | 1310 | `	if( !bBad && !bCVar && nCArg < nPArg ){` |
|        9 | 1311 | `		bBad = 1;  /* ...and it must still ACCEPT every one they do */` |
|        3 | 1312 | `	}` |
|        - | 1313 | `	/* Every position both signatures answer: the type contravariantly, and the` |
|        - | 1314 | `	 * by-reference-ness php requires to MATCH exactly (nothing checked it here). A` |
|        - | 1315 | `	 * position only the CHILD declares is unconstrained -- the arity rules above` |
|        - | 1316 | `	 * already made it optional. */` |
|      317 | 1317 | `	nPos = nPArg > nCArg ? nPArg : nCArg;` |
|      393 | 1318 | `	for( k = 0 ; !bBad && k < nPos ; ++k ){` |
|       83 | 1319 | `		ph7_vm_func_arg *pPa = OvArgAt(pPF,k);` |
|       83 | 1320 | `		ph7_vm_func_arg *pCa = OvArgAt(pCF,k);` |
|       83 | 1321 | `		if( pPa == 0 \|\| pCa == 0 ){` |
|       11 | 1322 | `			continue;` |
|        - | 1323 | `		}` |
|       75 | 1324 | `		if( ((pPa->iFlags ^ pCa->iFlags) & VM_FUNC_ARG_BY_REF) != 0 ){` |
|        3 | 1325 | `			bBad = 1;` |
|        3 | 1326 | `			break;` |
|        - | 1327 | `		}` |
|       73 | 1328 | `		OvFromArg(pVm,pParentOwner,pPa,&sP);` |
|       73 | 1329 | `		OvFromArg(pVm,pChildOwner,pCa,&sC);` |
|       73 | 1330 | `		bBad = OvCheck(&sP,&sC,/* bCovariant */ 0,pParentOwner,pChildOwner) == OV_BAD;` |
|       39 | 1331 | `	}` |
|      317 | 1332 | `	if( bBad ){` |
|        - | 1333 | `		SyBlob sChild, sParent;` |
|        - | 1334 | `		sxi32 rc;` |
|       34 | 1335 | `		SyBlobInit(&sChild,&pVm->sAllocator);` |
|       34 | 1336 | `		SyBlobInit(&sParent,&pVm->sAllocator);` |
|       34 | 1337 | `		PH7_ClassRenderDecl(pVm,pChildOwner,pCF,&sChild);` |
|       34 | 1338 | `		PH7_ClassRenderDecl(pVm,pParentOwner,pPF,&sParent);` |
|       49 | 1339 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pChild->nLine,` |
|        - | 1340 | `			"Declaration of %z::%z%.*s must be compatible with %z::%z%.*s",` |
|       15 | 1341 | `			&pChildOwner->sName,pMName,` |
|       30 | 1342 | `			OoDeclCLen(&sChild),(const char *)SyBlobData(&sChild),` |
|       15 | 1343 | `			&pParentOwner->sName,&pParent->sFunc.sName,` |
|       30 | 1344 | `			OoDeclCLen(&sParent),(const char *)SyBlobData(&sParent));` |
|       34 | 1345 | `		SyBlobRelease(&sChild);` |
|       34 | 1346 | `		SyBlobRelease(&sParent);` |
|       34 | 1347 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1348 | `			return SXERR_ABORT;` |
|        - | 1349 | `		}` |
|       15 | 1350 | `	}` |
|      317 | 1351 | `	return SXRET_OK;` |
|   306302 | 1352 | `}` |
|        - | 1353 | `/*` |
|        - | 1354 | ` * Every method the sub-INTERFACE declares ITSELF, judged against the same name in` |
|        - | 1355 | ` * one parent. php checks a restated interface method exactly as it checks an` |
|        - | 1356 | ` * overriding class method, and words the refusal the same way -- PHL checked` |
|        - | 1357 | ` * neither, and instead refused the restatement outright when it came from a` |
|        - | 1358 | ` * parent past the first (see the collected-parents comment in the interface` |
|        - | 1359 | ` * compiler).` |
|        - | 1360 | ` *` |
|        - | 1361 | ` * Called while hMethod still holds only this interface's own declarations, which` |
|        - | 1362 | ` * is the one moment the two sets are separable.` |
|        - | 1363 | ` */` |
|       64 | 1364 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceCheckRedeclare(ph7_gen_state *pGen,ph7_class *pSub,` |
|        - | 1365 | `	ph7_class *pParent)` |
|        4 | 1366 | `{` |
|        - | 1367 | `	SyHashEntry *pEntry;` |
|       68 | 1368 | `	SyHashResetLoopCursor(&pSub->hMethod);` |
|      138 | 1369 | `	while((pEntry = SyHashGetNextEntry(&pSub->hMethod)) != 0 ){` |
|       42 | 1370 | `		ph7_class_method *pOwn = (ph7_class_method *)pEntry->pUserData;` |
|       42 | 1371 | `		SyString *pName = &pOwn->sFunc.sName;` |
|       61 | 1372 | `		SyHashEntry *pUp = SyHashGet(&pParent->hMethod,` |
|       38 | 1373 | `			(const void *)pName->zString,pName->nByte);` |
|       49 | 1374 | `		if( pUp && PH7_ClassCheckOverrideCompat(&(*pGen),pParent,pSub,` |
|       21 | 1375 | `			(ph7_class_method *)pUp->pUserData,pOwn,0) == SXERR_ABORT ){` |
|      ! 0 | 1376 | `			return SXERR_ABORT;` |
|        - | 1377 | `		}` |
|        4 | 1378 | `	}` |
|       68 | 1379 | `	return SXRET_OK;` |
|       36 | 1380 | `}` |
|        - | 1381 | `/*` |
|        - | 1382 | ` * Perform an inheritance operation.` |
|        - | 1383 | ` * According to the PHP language reference manual` |
|        - | 1384 | ` *  When you extend a class, the subclass inherits all of the public and protected methods` |
|        - | 1385 | ` *  from the parent class. Unless a class Overwrites those methods, they will retain their original` |
|        - | 1386 | ` *  functionality.` |
|        - | 1387 | ` *  This is useful for defining and abstracting functionality, and permits the implementation` |
|        - | 1388 | ` *  of additional functionality in similar objects without the need to reimplement all of the shared` |
|        - | 1389 | ` *  functionality.` |
|        - | 1390 | ` *  Example #1 Inheritance Example` |
|        - | 1391 | ` * <?php` |
|        - | 1392 | ` * class foo` |
|        - | 1393 | ` * {` |
|        - | 1394 | ` *   public function printItem($string)` |
|        - | 1395 | ` *   {` |
|        - | 1396 | ` *       echo 'Foo: ' . $string . PHP_EOL;` |
|        - | 1397 | ` *   }` |
|        - | 1398 | ` *` |
|        - | 1399 | ` *   public function printPHP()` |
|        - | 1400 | ` *   {` |
|        - | 1401 | ` *       echo 'PHP is great.' . PHP_EOL;` |
|        - | 1402 | ` *   }` |
|        - | 1403 | ` * }` |
|        - | 1404 | ` * class bar extends foo` |
|        - | 1405 | ` * {` |
|        - | 1406 | ` *   public function printItem($string)` |
|        - | 1407 | ` *   {` |
|        - | 1408 | ` *       echo 'Bar: ' . $string . PHP_EOL;` |
|        - | 1409 | ` *   }` |
|        - | 1410 | ` * }` |
|        - | 1411 | ` * $foo = new foo();` |
|        - | 1412 | ` * $bar = new bar();` |
|        - | 1413 | ` * $foo->printItem('baz'); // Output: 'Foo: baz'` |
|        - | 1414 | ` * $foo->printPHP();       // Output: 'PHP is great'` |
|        - | 1415 | ` * $bar->printItem('baz'); // Output: 'Bar: baz'` |
|        - | 1416 | ` * $bar->printPHP();       // Output: 'PHP is great'` |
|        - | 1417 | ` *` |
|        - | 1418 | ` * This function return SXRET_OK if the inheritance operation was successfully performed.` |
|        - | 1419 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|        - | 1420 | ` * error message.` |
|        - | 1421 | ` */` |
|   652959 | 1422 | `PH7_PRIVATE sxi32 PH7_ClassInherit(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase)` |
|        5 | 1423 | `{` |
|        - | 1424 | `	ph7_class_method *pMeth;` |
|        - | 1425 | `	ph7_class_attr *pAttr;` |
|        - | 1426 | `	SyHashEntry *pEntry;` |
|        - | 1427 | `	SyString *pName;` |
|        - | 1428 | `	SySet aInherited; /* base attributes to prepend (see the copy loop below) */` |
|        - | 1429 | `	sxi32 rc;` |
|   652964 | 1430 | `	SySetInit(&aInherited,&pGen->pVm->sAllocator,sizeof(ph7_class_attr *));` |
|        - | 1431 | `	/* Install in the derived hashtable */` |
|   652964 | 1432 | `	rc = SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|   652964 | 1433 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1434 | `		SySetRelease(&aInherited);` |
|      ! 0 | 1435 | `		return rc;` |
|        - | 1436 | `	}` |
|        - | 1437 | `	/* readonly class inheritance (PHP 8.2): a readonly class may only extend a` |
|        - | 1438 | `	 * readonly class, and a non-readonly class may not extend a readonly one.` |
|        - | 1439 | `	 * A FINAL base is not one of these cases at all -- it cannot be extended by` |
|        - | 1440 | `	 * anything, and php reports only that. Both diagnostics used to fire for a` |
|        - | 1441 | ``	 * `final readonly` base and the readonly one was reported, which is the wrong`` |
|        - | 1442 | `	 * reason; BcMath\Number is the engine's first such class. */` |
|   652959 | 1443 | `	if( (pBase->iFlags & PH7_CLASS_FINAL) == 0` |
|   652962 | 1444 | `	 && (pBase->iFlags & PH7_CLASS_READONLY) != (pSub->iFlags & PH7_CLASS_READONLY) ){` |
|        5 | 1445 | `		if( pBase->iFlags & PH7_CLASS_READONLY ){` |
|        4 | 1446 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|        - | 1447 | `				"Non-readonly class %z cannot extend readonly class %z",` |
|        1 | 1448 | `				&pSub->sName,&pBase->sName);` |
|        2 | 1449 | `		}else{` |
|        4 | 1450 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|        - | 1451 | `				"Readonly class %z cannot extend non-readonly class %z",` |
|        1 | 1452 | `				&pSub->sName,&pBase->sName);` |
|        - | 1453 | `		}` |
|        5 | 1454 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1455 | `			SySetRelease(&aInherited);` |
|      ! 0 | 1456 | `			return SXERR_ABORT;` |
|        - | 1457 | `		}` |
|        2 | 1458 | `	}` |
|        - | 1459 | `	/* Mark as subclass BEFORE the members are copied. php's mangled storage name` |
|        - | 1460 | `	 * for a TRAIT-composed private names the class that composed it, found by` |
|        - | 1461 | `	 * walking the subclass's ANCESTRY (PH7_VmMemberOwnerClass) -- with pBase still` |
|        - | 1462 | `	 * unset the walk stopped at the trait, cached that answer on the attribute,` |
|        - | 1463 | `	 * and every later lookup then asked for a key the object's table did not hold. */` |
|   652964 | 1464 | `	pSub->pBase = pBase;` |
|        - | 1465 | `	/* A native class whose php-visible properties are LAZY passes that on: the` |
|        - | 1466 | `	 * attributes copied below keep their flags, so a subclass of DateInterval has` |
|        - | 1467 | `	 * the same ten to install, and the O(1) gate in front of the materialization` |
|        - | 1468 | `	 * walk has to see it on the SUBCLASS or the constructor's writes land nowhere. */` |
|   652964 | 1469 | `	if( pBase->iFlags & PH7_CLASS_LAZY_ATTR ){` |
|       27 | 1470 | `		pSub->iFlags \|= PH7_CLASS_LAZY_ATTR;` |
|       13 | 1471 | `	}` |
|        - | 1472 | `	/* Copy public/protected attributes from the base class */` |
|   652964 | 1473 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
|  6066847 | 1474 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|        - | 1475 | `		/* Make sure the private attributes are not redeclared in the subclass */` |
|  5413888 | 1476 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  5413888 | 1477 | `		pName = &pAttr->sName;` |
|  5413883 | 1478 | `		if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|  3757217 | 1479 | `		 && (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - | 1480 | `			/* A base's private INSTANCE property is a slot of its own on every` |
|        - | 1481 | `			 * object below it, filed under php's mangled storage name -- so it can` |
|        - | 1482 | `			 * never collide with a subclass member of the same name, and the` |
|        - | 1483 | `			 * redeclaration rules below have nothing to say about it. The subclass` |
|        - | 1484 | `			 * keeps its own declaration exactly where it wrote it. */` |
|  2104968 | 1485 | `			rc = SySetPut(&aInherited,(const void *)&pAttr);` |
|  2104968 | 1486 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1487 | `				SySetRelease(&aInherited);` |
|      ! 0 | 1488 | `				return rc;` |
|        - | 1489 | `			}` |
|  2104968 | 1490 | `			continue;` |
|        - | 1491 | `		}` |
|  3308925 | 1492 | `		if( (pEntry = SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|    20205 | 1493 | `			if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_FINAL))` |
|    10094 | 1494 | `				== (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_FINAL) ){` |
|        - | 1495 | `				/* Cannot override a final class constant (PHP 8.1). Report the` |
|        - | 1496 | `				 * class that originally declared it (pDeclClass) rather than the` |
|        - | 1497 | `				 * immediate base, so a multi-level chain matches PHP -- and a` |
|        - | 1498 | `				 * TRAIT-declared one belongs to the class that composed it.` |
|        - | 1499 | `				 * php reports it on the SUBCLASS's declaration line, not on the` |
|        - | 1500 | `				 * line the offending member sits on: the refusal is inheritance` |
|        - | 1501 | ``				 * talking, and inheritance happens where `extends` is written.`` |
|        - | 1502 | `				 * (Its final-METHOD twin below is the other rule -- php reports` |
|        - | 1503 | `				 * THAT one at the method.) */` |
|      ! 0 | 1504 | `				ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pBase);` |
|      ! 0 | 1505 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|        - | 1506 | `					"%z::%z cannot override final constant %z::%z",` |
|      ! 0 | 1507 | `					&pSub->sName,pName,&pOwner->sName,pName);` |
|      ! 0 | 1508 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1509 | `					SySetRelease(&aInherited);` |
|      ! 0 | 1510 | `					return SXERR_ABORT;` |
|        - | 1511 | `				}` |
|    20205 | 1512 | `			}else if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_FINAL))` |
|    10094 | 1513 | `				== PH7_CLASS_ATTR_FINAL ){` |
|        - | 1514 | `				/* PHP 8.4's final PROPERTY: no subclass may redeclare it, however the` |
|        - | 1515 | `				 * redeclaration is spelled -- a plain or static property of its own, a` |
|        - | 1516 | `				 * PROMOTED constructor parameter, or a trait it composes -- because all` |
|        - | 1517 | `				 * three land in the subclass's attribute table before inheritance runs.` |
|        - | 1518 | `				 * Same class-line rule and same declaring-class naming as the constant` |
|        - | 1519 | `				 * above. */` |
|        9 | 1520 | `				ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pBase);` |
|       12 | 1521 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|        3 | 1522 | `					"Cannot override final property %z::$%z",&pOwner->sName,pName);` |
|        9 | 1523 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1524 | `					SySetRelease(&aInherited);` |
|      ! 0 | 1525 | `					return SXERR_ABORT;` |
|        - | 1526 | `				}` |
|        3 | 1527 | `			}` |
|        - | 1528 | `			/* A child MAY redeclare a base's private property: php treats the two` |
|        - | 1529 | `			 * as independent members (each private to its declaring class), with no` |
|        - | 1530 | `			 * diagnostic. PH7 warned here, which is wrong — the child's entry simply` |
|        - | 1531 | `			 * shadows the base's in the by-name attribute table.` |
|        - | 1532 | `			 *` |
|        - | 1533 | `			 * Ordering: php keeps an overridden INSTANCE property at the position` |
|        - | 1534 | `` 			 * the BASE declared it (`class G{$g1;$g2;} class H extends G{$h;$g1;}` `` |
|        - | 1535 | `			 * iterates g1,g2,h — not g2,h,g1). Re-collect the CHILD's definition,` |
|        - | 1536 | `			 * whose default value wins, and drop its current entry so the prepend` |
|        - | 1537 | `			 * below re-inserts it in base order. Statics/constants are not part of` |
|        - | 1538 | `			 * instance iteration, so they keep their existing slot. */` |
|    20210 | 1539 | `			if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|    20206 | 1540 | `				ph7_class_attr *pOwn = (ph7_class_attr *)pEntry->pUserData;` |
|    20206 | 1541 | `				SyHashDeleteEntry(&pSub->hAttr,(const void *)pName->zString,pName->nByte,0);` |
|    20206 | 1542 | `				rc = SySetPut(&aInherited,(const void *)&pOwn);` |
|    20206 | 1543 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 1544 | `					SySetRelease(&aInherited);` |
|      ! 0 | 1545 | `					return rc;` |
|        - | 1546 | `				}` |
|    10087 | 1547 | `			}` |
|    20210 | 1548 | `			continue;` |
|        - | 1549 | `		}` |
|        - | 1550 | `		/* Collect the attribute. A private STATIC comes down too: php keeps one` |
|        - | 1551 | ``		 * in the child's property table -- `B::$s` on `class A { private static`` |
|        - | 1552 | ``		 * $s; }` is "Cannot access private property B::$s", the visibility`` |
|        - | 1553 | `		 * refusal, and not the undeclared-static one -- and nothing else could` |
|        - | 1554 | ``		 * find it, so `static::$s` from a base method with the subclass as its`` |
|        - | 1555 | `		 * late-static-binding target reported its own static as undeclared. Its` |
|        - | 1556 | `		 * storage is the DECLARING class's slot either way (nIdx is shared), so` |
|        - | 1557 | `		 * this is a second name for one static, exactly as php has it.` |
|        - | 1558 | `		 *` |
|        - | 1559 | `		 * These are gathered rather than installed here because php orders an` |
|        - | 1560 | `		 * instance's properties BASE-DECLARED FIRST, then the subclass's own,` |
|        - | 1561 | `		 * then trait members — while inheritance runs AFTER the subclass body` |
|        - | 1562 | `		 * has already filled hAttr. They are prepended below. */` |
|  3288720 | 1563 | `		rc = SySetPut(&aInherited,(const void *)&pAttr);` |
|  3288720 | 1564 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1565 | `			SySetRelease(&aInherited);` |
|      ! 0 | 1566 | `			return rc;` |
|        - | 1567 | `		}` |
|        5 | 1568 | `	}` |
|        - | 1569 | `	/* Prepend the collected base attributes so hAttr reads base-first. The` |
|        - | 1570 | `	 * table's iteration list is what every object-iteration consumer walks` |
|        - | 1571 | `	 * (var_dump/print_r, get_object_vars, foreach, (array) casts, json_encode),` |
|        - | 1572 | `	 * so this ordering is user-visible — json_encode emits its keys in exactly` |
|        - | 1573 | `	 * this order. SyHashInsert is a HEAD insert, so walking the collected set` |
|        - | 1574 | `	 * backwards leaves the base's own declaration order at the front. */` |
|   652964 | 1575 | `	if( SySetUsed(&aInherited) > 0 ){` |
|   652544 | 1576 | `		ph7_class_attr **apInherited = (ph7_class_attr **)SySetBasePtr(&aInherited);` |
|   652544 | 1577 | `		sxu32 n = SySetUsed(&aInherited);` |
|  6066423 | 1578 | `		while( n > 0 ){` |
|  5413884 | 1579 | `			ph7_class_attr *pIn = apInherited[--n];` |
|        - | 1580 | `			/* Under php's STORAGE name, which is the plain one for everything but` |
|        - | 1581 | `			 * an inherited private instance property. */` |
|  5413884 | 1582 | `			const SyString *pKey = PH7_ClassAttrStorageName(pGen->pVm,pSub,pIn);` |
|  5413884 | 1583 | `			if( pKey != &pIn->sName ){` |
|       58 | 1584 | `				pSub->iFlags \|= PH7_CLASS_SHADOW_PROP;` |
|        - | 1585 | `				/* ...and WHICH plain name it is hidden under, so the per-access` |
|        - | 1586 | `				 * screen in OoScopePrivateAttr can answer without a lookup. */` |
|       85 | 1587 | `				pSub->nShadowName \|= OoShadowNameBit(SyHashKey(&pSub->hAttr,` |
|       54 | 1588 | `					(const void *)SyStringData(&pIn->sName),SyStringLength(&pIn->sName)));` |
|       27 | 1589 | `			}` |
|  5413884 | 1590 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pKey->zString,pKey->nByte,pIn);` |
|  5413884 | 1591 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1592 | `				SySetRelease(&aInherited);` |
|      ! 0 | 1593 | `				return rc;` |
|        - | 1594 | `			}` |
|        5 | 1595 | `		}` |
|   325833 | 1596 | `	}` |
|        - | 1597 | `	/* Inherit the base's CONSTANTS (separate namespace: hConst). Constants are not` |
|        - | 1598 | `	 * part of instance iteration, so no base-first ordering dance is needed — a plain` |
|        - | 1599 | `	 * copy of every constant the subclass did not itself redeclare. A subclass that` |
|        - | 1600 | `	 * redeclares a base FINAL constant is a fatal (PHP 8.1). */` |
|   652964 | 1601 | `	SyHashResetLoopCursor(&pBase->hConst);` |
|  2341255 | 1602 | `	while((pEntry = SyHashGetNextEntry(&pBase->hConst)) != 0 ){` |
|        - | 1603 | `		SyHashEntry *pOwn;` |
|  1688296 | 1604 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  1688296 | 1605 | `		pName = &pAttr->sName;` |
|  1688296 | 1606 | `		if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|        - | 1607 | ``			/* A private CONSTANT is not inherited at all: php answers `B::K` with`` |
|        - | 1608 | `			 * "Undefined constant B::K", never with the visibility refusal it words` |
|        - | 1609 | ``			 * for `A::K`. Copying it down said "Cannot access private constant`` |
|        - | 1610 | ``			 * B::K" -- and let `static::K` from a base method find one php does`` |
|        - | 1611 | ``			 * not. A base method's own `self::K` resolves against A directly. */`` |
|       19 | 1612 | `			continue;` |
|        - | 1613 | `		}` |
|  1688280 | 1614 | `		if( (pOwn = SyHashGet(&pSub->hConst,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|       22 | 1615 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_FINAL) ){` |
|        - | 1616 | `				/* Cannot override a final class constant. Report the class that` |
|        - | 1617 | `				 * originally declared it (pDeclClass) for a multi-level chain -- and a` |
|        - | 1618 | `				 * TRAIT-declared one belongs to the class that composed it. */` |
|        6 | 1619 | `				ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pBase);` |
|        8 | 1620 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|        - | 1621 | `					"%z::%z cannot override final constant %z::%z",` |
|        2 | 1622 | `					&pSub->sName,pName,&pOwner->sName,pName);` |
|        6 | 1623 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1624 | `					SySetRelease(&aInherited);` |
|      ! 0 | 1625 | `					return SXERR_ABORT;` |
|        - | 1626 | `				}` |
|        2 | 1627 | `			}` |
|       22 | 1628 | `			continue;` |
|        - | 1629 | `		}` |
|  1688262 | 1630 | `		rc = SyHashInsertTail(&pSub->hConst,(const void *)pName->zString,pName->nByte,pAttr);` |
|  1688262 | 1631 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1632 | `			SySetRelease(&aInherited);` |
|      ! 0 | 1633 | `			return rc;` |
|        - | 1634 | `		}` |
|        5 | 1635 | `	}` |
|   652964 | 1636 | `	SySetRelease(&aInherited);` |
|   652964 | 1637 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
| 12107991 | 1638 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|        - | 1639 | `		SyHashEntry *pOwn;` |
|        - | 1640 | `		SyString sKey;` |
|        - | 1641 | `		/* Make sure the private/final methods are not redeclared in the subclass.` |
|        - | 1642 | `		 * The identity inherited is the base's HASH KEY, not sFunc.sName — the same` |
|        - | 1643 | `		 * rule PH7_ClassUseTrait copies a trait by. A trait adaptation leaves the` |
|        - | 1644 | `		 * composed class holding entries whose key is the name the class ANSWERS to` |
|        - | 1645 | ``		 * while the method struct keeps its original name: `B::m as mB` is the key`` |
|        - | 1646 | `` 		 * `mB` over a struct still called `m`, and `A::m insteadof B` is the key `m` `` |
|        - | 1647 | ``		 * over A's struct. Keying the copy off sFunc.sName re-filed both under `m`,`` |
|        - | 1648 | `		 * so a subclass of the composing class lost the alias entirely and took` |
|        - | 1649 | ``		 * whichever of the two the hash walk reached last as its `m` — the insteadof`` |
|        - | 1650 | `		 * choice, silently reversed. */` |
| 11455032 | 1651 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
| 11455032 | 1652 | `		SyStringInitFromBuf(&sKey,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
| 11455032 | 1653 | `		pName = &sKey;` |
| 11455032 | 1654 | `		if( (pOwn = SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|   612010 | 1655 | `			ph7_class_method *pOwnMeth = (ph7_class_method *)pOwn->pUserData;` |
|   612010 | 1656 | `			ph7_class *pOwnDecl = (ph7_class *)pOwnMeth->sFunc.pUserData;` |
|   612005 | 1657 | `			if( (pOwnMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0` |
|   305597 | 1658 | `			 && (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0` |
|       13 | 1659 | `			 && pOwnDecl && (pOwnDecl->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|        - | 1660 | ``				/* A trait's `abstract` is a REQUIREMENT, not a member, and php lets an`` |
|        - | 1661 | ``				 * INHERITED method satisfy it: `trait T { abstract function need(); }`` |
|        - | 1662 | ``				 * class P { function need(){} } class C extends P { use T; }` composes`` |
|        - | 1663 | `				 * there and was "Class C contains 1 abstract method" here, because the` |
|        - | 1664 | `				 * trait is applied before the base is inherited and the requirement then` |
|        - | 1665 | `				 * shadowed the very method that answers it. The satisfying declaration` |
|        - | 1666 | `				 * still has to be COMPATIBLE with the requirement -- and php words that` |
|        - | 1667 | `				 * one the other way round, naming the class that PROVIDES the method and` |
|        - | 1668 | `				 * the trait that asked for it. */` |
|       10 | 1669 | `				rc = PH7_ClassCheckOverrideCompat(&(*pGen),pOwnDecl,pBase,pOwnMeth,pMeth,1);` |
|       10 | 1670 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1671 | `					return SXERR_ABORT;` |
|        - | 1672 | `				}` |
|       10 | 1673 | `				pOwn->pUserData = (void *)pMeth;` |
|   306418 | 1674 | `				continue;` |
|        - | 1675 | `			}` |
|   612002 | 1676 | `			if( pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|        - | 1677 | `				/* php: a base's PRIVATE method is never overridden — the child's` |
|        - | 1678 | `				 * declaration is an independent member of the same name, so neither` |
|        - | 1679 | `				 * the final rule nor the signature-compatibility rule applies to it` |
|        - | 1680 | `				 * (zend's do_inherit_method skips both for a private parent). PHL` |
|        - | 1681 | ``				 * ran both: `class A { private function m($a){} } class B extends A`` |
|        - | 1682 | ``				 * { private function m($a,$b,$c){} }` was a fatal php compiles, and`` |
|        - | 1683 | ``				 * `final private` in the base fataled every child that reused the`` |
|        - | 1684 | `				 * name — php only WARNS at the final-private DECLARATION and lets` |
|        - | 1685 | `				 * the child have the name. */` |
|   611996 | 1686 | `			}else if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|        - | 1687 | `				/* php: "Cannot override final method A::test()" */` |
|        8 | 1688 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_method *)pOwn->pUserData)->nLine,` |
|        - | 1689 | `					"Cannot override final method %z::%z()",` |
|        2 | 1690 | `					&pBase->sName,pName);` |
|        2 | 1691 | `				(void)pSub;` |
|        6 | 1692 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1693 | `					return SXERR_ABORT;` |
|        - | 1694 | `				}` |
|        4 | 1695 | `			}else{` |
|        - | 1696 | `				/* Check the override's signature is compatible with the parent's. */` |
|   917567 | 1697 | `				rc = PH7_ClassCheckOverrideCompat(&(*pGen),pBase,pSub,pMeth,` |
|   611981 | 1698 | `					(ph7_class_method *)pOwn->pUserData,1);` |
|   611986 | 1699 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1700 | `					return SXERR_ABORT;` |
|        - | 1701 | `				}` |
|        - | 1702 | `			}` |
|   612002 | 1703 | `			continue;` |
|        - | 1704 | `		}` |
|        - | 1705 | `		/* Install the method. php: a base class's private method is in the child's` |
|        - | 1706 | `		 * table too — an inherited public method calling $this->priv() must find it,` |
|        - | 1707 | ``		 * and the LOOKUP has to find it for php's answer to `B::p()` to be`` |
|        - | 1708 | `		 * "Call to private method A::p() from global scope" rather than` |
|        - | 1709 | `		 * "Call to undefined method B::p()". The call-site visibility check binds by` |
|        - | 1710 | `		 * DECLARING class (sFunc.pUserData), so child code and outsiders still cannot` |
|        - | 1711 | ``		 * reach it; a private ctor copied down blocks `new Child` from outside like`` |
|        - | 1712 | `		 * php's; and the surfaces that must NOT show an inherited private say so` |
|        - | 1713 | `		 * themselves (method_exists, get_class_methods, ReflectionClass::getMethods).` |
|        - | 1714 | `		 *` |
|        - | 1715 | `		 * STATIC privates used to be skipped here, on the reasoning that base methods` |
|        - | 1716 | `		 * reach them through self:: against the declaring class anyway. They do — but` |
|        - | 1717 | ``		 * nothing else could: every spelling of `B::p()` (the call, `['B','p']()`,`` |
|        - | 1718 | `		 * call_user_func, a first-class callable) reported the name as UNDEFINED, and` |
|        - | 1719 | ``		 * `static::p()` from the base with a subclass as the late-static-binding`` |
|        - | 1720 | `		 * target could not find its own method. */` |
| 10843027 | 1721 | `		rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 10843027 | 1722 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1723 | `			return rc;` |
|        - | 1724 | `		}` |
|        5 | 1725 | `	}` |
|        - | 1726 | `	/* All done */` |
|   652964 | 1727 | `	return SXRET_OK;` |
|   326048 | 1728 | `}` |
|        - | 1729 | `/*` |
|        - | 1730 | ` * Do these two compiled property defaults say the same thing? A raw memcmp of the` |
|        - | 1731 | ` * two instruction buffers is not that question: an instruction carries the LINE it` |
|        - | 1732 | ` * was compiled from and a literal travels as an INDEX into the VM's constant table,` |
|        - | 1733 | `` * so `public $p = 1` written in a trait and the same `public $p = 1` written in the`` |
|        - | 1734 | ` * composing class compare as different bytes and made php's incompatible-property` |
|        - | 1735 | ` * fatal fire on a class php composes without a word. Compare what the instructions` |
|        - | 1736 | ` * MEAN instead: the opcode, its operands, and for a constant load the VALUE behind` |
|        - | 1737 | ` * the index.` |
|        - | 1738 | ` */` |
|       32 | 1739 | `static int VmTraitLiteralSame(ph7_vm *pVm,sxu32 nLeft,sxu32 nRight)` |
|        5 | 1740 | `{` |
|        - | 1741 | `	ph7_value *pLeft,*pRight;` |
|       37 | 1742 | `	if( nLeft == nRight ){` |
|       15 | 1743 | `		return 1;` |
|        - | 1744 | `	}` |
|       23 | 1745 | `	pLeft  = (ph7_value *)SySetAt(&pVm->aLitObj,nLeft);` |
|       23 | 1746 | `	pRight = (ph7_value *)SySetAt(&pVm->aLitObj,nRight);` |
|       23 | 1747 | `	if( pLeft == 0 \|\| pRight == 0 ){` |
|      ! 0 | 1748 | `		return 0;` |
|        - | 1749 | `	}` |
|       23 | 1750 | `	if( (pLeft->iFlags & ~MEMOBJ_AUX) != (pRight->iFlags & ~MEMOBJ_AUX) ){` |
|      ! 0 | 1751 | `		return 0;` |
|        - | 1752 | `	}` |
|       18 | 1753 | `	if( SyBlobLength(&pLeft->sBlob) != SyBlobLength(&pRight->sBlob)` |
|       23 | 1754 | `	 \|\| (SyBlobLength(&pLeft->sBlob) > 0` |
|       11 | 1755 | `	     && SyMemcmp(SyBlobData(&pLeft->sBlob),SyBlobData(&pRight->sBlob),` |
|        4 | 1756 | `	                 SyBlobLength(&pLeft->sBlob)) != 0) ){` |
|        3 | 1757 | `		return 0;` |
|        - | 1758 | `	}` |
|       21 | 1759 | `	if( (pLeft->iFlags & MEMOBJ_INT) && pLeft->x.iVal != pRight->x.iVal ){` |
|        9 | 1760 | `		return 0;` |
|        - | 1761 | `	}` |
|       12 | 1762 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && pLeft->rVal != pRight->rVal ){` |
|      ! 0 | 1763 | `		return 0;` |
|        - | 1764 | `	}` |
|       12 | 1765 | `	return 1;` |
|       21 | 1766 | `}` |
|       28 | 1767 | `static int VmTraitDefaultsMatch(ph7_vm *pVm,SySet *pLeft,SySet *pRight)` |
|        5 | 1768 | `{` |
|        - | 1769 | `	VmInstr *aLeft,*aRight;` |
|        - | 1770 | `	sxu32 n,nUsed;` |
|       33 | 1771 | `	nUsed = SySetUsed(pLeft);` |
|       33 | 1772 | `	if( nUsed != SySetUsed(pRight) ){` |
|      ! 0 | 1773 | `		return 0;` |
|        - | 1774 | `	}` |
|       33 | 1775 | `	if( nUsed < 1 ){` |
|        3 | 1776 | `		return 1;` |
|        - | 1777 | `	}` |
|       31 | 1778 | `	aLeft  = (VmInstr *)SySetBasePtr(pLeft);` |
|       31 | 1779 | `	aRight = (VmInstr *)SySetBasePtr(pRight);` |
|       75 | 1780 | `	for( n = 0 ; n < nUsed ; ++n ){` |
|       57 | 1781 | `		if( aLeft[n].iOp != aRight[n].iOp \|\| aLeft[n].iP1 != aRight[n].iP1 ){` |
|      ! 0 | 1782 | `			return 0;` |
|        - | 1783 | `		}` |
|       57 | 1784 | `		if( aLeft[n].iOp == PH7_OP_LOADC ){` |
|       37 | 1785 | `			if( !VmTraitLiteralSame(pVm,aLeft[n].iP2,aRight[n].iP2) ){` |
|       12 | 1786 | `				return 0;` |
|        - | 1787 | `			}` |
|       26 | 1788 | `			continue;` |
|        - | 1789 | `		}` |
|       22 | 1790 | `		if( aLeft[n].iP2 != aRight[n].iP2 \|\| aLeft[n].p3 != aRight[n].p3 ){` |
|      ! 0 | 1791 | `			return 0;` |
|        - | 1792 | `		}` |
|       12 | 1793 | `	}` |
|       20 | 1794 | `	return 1;` |
|       19 | 1795 | `}` |
|        - | 1796 | `/*` |
|        - | 1797 | ` * Two constant declarations php considers the SAME declaration. Composing a trait over a` |
|        - | 1798 | ` * name that is already taken is only a conflict when the definition differs, and php's` |
|        - | 1799 | ``  * notion of "differs" covers the whole declaration, not just the value: `final const K='x'` `` |
|        - | 1800 | ``  * against `const K='x'` conflicts, and so does `public const K` against `private const K` `` |
|        - | 1801 | `` * and `const int K=1` against `const K=1`.`` |
|        - | 1802 | ` */` |
|        8 | 1803 | `static int VmTraitConstDefsMatch(ph7_vm *pVm,ph7_class_attr *pLeft,ph7_class_attr *pRight)` |
|        3 | 1804 | `{` |
|       11 | 1805 | `	sxi32 iMask = PH7_CLASS_ATTR_FINAL\|PH7_CLASS_ATTR_TYPED\|PH7_CLASS_ATTR_ABSTRACT;` |
|       11 | 1806 | `	if( pLeft->iProtection != pRight->iProtection ){` |
|      ! 0 | 1807 | `		return 0;` |
|        - | 1808 | `	}` |
|       11 | 1809 | `	if( (pLeft->iFlags & iMask) != (pRight->iFlags & iMask) ){` |
|      ! 0 | 1810 | `		return 0;` |
|        - | 1811 | `	}` |
|       11 | 1812 | `	if( SyStringCmp(&pLeft->sTypeName,&pRight->sTypeName,SyMemcmp) != 0 ){` |
|      ! 0 | 1813 | `		return 0;` |
|        - | 1814 | `	}` |
|       11 | 1815 | `	return VmTraitDefaultsMatch(pVm,&pLeft->aByteCode,&pRight->aByteCode);` |
|        7 | 1816 | `}` |
|        - | 1817 | `/*` |
|        - | 1818 | ` * A private copy of a trait member's record for one composing class. php composes a trait` |
|        - | 1819 | ` * into each using class SEPARATELY, so a trait's STATIC property is one slot per class --` |
|        - | 1820 | `` * `trait T { public static $c = 0; } class A { use T; } class B { use T; }` gives A and B a`` |
|        - | 1821 | `` * counter each -- and a trait CONSTANT is evaluated per class, so `const K = self::J` reads`` |
|        - | 1822 | ` * the J of whichever class composed it. Copying the record by POINTER gave every using class` |
|        - | 1823 | ` * the same storage slot and the same memoized value.` |
|        - | 1824 | ` *` |
|        - | 1825 | ` * The copy shares its source's compiled byte-code and attribute sets, which are read-only` |
|        - | 1826 | ` * once compilation is past; what it does NOT share is nIdx, the storage slot, and the` |
|        - | 1827 | ` * per-evaluation flags. pDeclClass stays the TRAIT, so every scope and naming rule still` |
|        - | 1828 | ` * finds the composing class through it.` |
|        - | 1829 | ` */` |
|       48 | 1830 | `static ph7_class_attr * VmCloneTraitAttr(ph7_vm *pVm,ph7_class_attr *pSrc)` |
|        4 | 1831 | `{` |
|       52 | 1832 | `	ph7_class_attr *pNew = (ph7_class_attr *)SyMemBackendPoolAlloc(&pVm->sAllocator,` |
|        - | 1833 | `		sizeof(ph7_class_attr));` |
|       52 | 1834 | `	if( pNew == 0 ){` |
|      ! 0 | 1835 | `		return 0;` |
|        - | 1836 | `	}` |
|       52 | 1837 | `	SyMemcpy((const void *)pSrc,(void *)pNew,sizeof(ph7_class_attr));` |
|       52 | 1838 | `	pNew->nIdx = SXU32_HIGH; /* its own storage slot, reserved at this class's mount */` |
|       52 | 1839 | `	SyZero(&pNew->sStoreName,sizeof(SyString)); /* ...and its own mangled name, which` |
|        - | 1840 | `	                          * names the class that COMPOSED it and not the source's */` |
|       52 | 1841 | `	pNew->iFlags &= ~(PH7_CLASS_ATTR_EVALING\|PH7_CLASS_ATTR_STATIC_DEFER);` |
|       52 | 1842 | `	return pNew;` |
|       28 | 1843 | `}` |
|        - | 1844 | `/*` |
|        - | 1845 | ` * Apply a trait to a class: copy all methods and attributes from the trait` |
|        - | 1846 | ` * into the target class. Unlike inheritance, traits copy ALL members including` |
|        - | 1847 | ` * private ones. Members already defined in the class take precedence.` |
|        - | 1848 | ` */` |
|      306 | 1849 | `PH7_PRIVATE sxi32 PH7_ClassUseTrait(ph7_gen_state *pGen,ph7_class *pClass,ph7_class *pTrait)` |
|        5 | 1850 | `{` |
|        - | 1851 | `	ph7_class_method *pMeth;` |
|        - | 1852 | `	ph7_class_attr *pAttr;` |
|        - | 1853 | `	SyHashEntry *pEntry;` |
|        - | 1854 | `	SyString *pName;` |
|        - | 1855 | `	sxi32 rc;` |
|        - | 1856 | `	/* Detect cyclic trait composition (e.g. trait A { use B; } trait B { use A; }) */` |
|      311 | 1857 | `	if( pTrait->iFlags & PH7_CLASS_TRAIT_VISITING ){` |
|      ! 0 | 1858 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|      ! 0 | 1859 | `			"Trait circular reference detected: %z is already being applied",&pTrait->sName);` |
|      ! 0 | 1860 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1861 | `			return SXERR_ABORT;` |
|        - | 1862 | `		}` |
|      ! 0 | 1863 | `		return SXRET_OK;` |
|        - | 1864 | `	}` |
|      311 | 1865 | `	pTrait->iFlags \|= PH7_CLASS_TRAIT_VISITING;` |
|      311 | 1866 | `	rc = SXRET_OK;` |
|        - | 1867 | `	/* Copy attributes from the trait */` |
|      311 | 1868 | `	SyHashResetLoopCursor(&pTrait->hAttr);` |
|      429 | 1869 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hAttr)) != 0 ){` |
|        - | 1870 | `		SyHashEntry *pExisting;` |
|      123 | 1871 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      123 | 1872 | `		pName = &pAttr->sName;` |
|      123 | 1873 | `		pExisting = SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte);` |
|      123 | 1874 | `		if( pExisting != 0 ){` |
|        - | 1875 | `			/* The name is taken. What decides is the definition ALREADY standing --` |
|        - | 1876 | `			 * the class's own body just as much as an earlier trait's -- and whether` |
|        - | 1877 | `			 * its default is the same one. Looking the name up in the traits applied` |
|        - | 1878 | `			 * so far and comparing only THEN let a class-body property through:` |
|        - | 1879 | ``			 * `class M { use TA, TB; public $p = 3; }` said nothing when TA arrived`` |
|        - | 1880 | `			 * (no trait held the name yet) and then blamed the wrong pair when TB did. */` |
|       24 | 1881 | `			ph7_class_attr *pClassAttr = (ph7_class_attr *)pExisting->pUserData;` |
|       24 | 1882 | `			if( !VmTraitDefaultsMatch(pGen->pVm,&pAttr->aByteCode,&pClassAttr->aByteCode) ){` |
|        - | 1883 | `				/* php names the FIRST definition rather than the standing one: when the` |
|        - | 1884 | `				 * holder is the composing class itself, it walks the traits applied so` |
|        - | 1885 | `				 * far and names the first that declares the property, so the same class` |
|        - | 1886 | `				 * body reads "M and TA" with one trait behind it and "TA and TB" with` |
|        - | 1887 | `				 * two. The sentence ends with a clause of its own and lets the fatal's` |
|        - | 1888 | `				 * " in %s on line %u" finish it -- the line is the composing class's. */` |
|        6 | 1889 | `				ph7_class *pHolder = pClassAttr->pDeclClass;` |
|        6 | 1890 | `				if( pHolder == 0 \|\| pHolder == pClass ){` |
|      ! 0 | 1891 | `					ph7_class **apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|      ! 0 | 1892 | `					sxu32 nUsed = SySetUsed(&pClass->aTrait);` |
|        - | 1893 | `					sxu32 k;` |
|      ! 0 | 1894 | `					pHolder = pClass;` |
|      ! 0 | 1895 | `					for(k = 0; k < nUsed; k++){` |
|      ! 0 | 1896 | `						if( PH7_ClassExtractAttribute(apUsedTraits[k],pName->zString,pName->nByte) ){` |
|      ! 0 | 1897 | `							pHolder = apUsedTraits[k];` |
|      ! 0 | 1898 | `							break;` |
|        - | 1899 | `						}` |
|      ! 0 | 1900 | `					}` |
|      ! 0 | 1901 | `				}` |
|        8 | 1902 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,` |
|        - | 1903 | `					"%z and %z define the same property ($%z) in the composition of %z. "` |
|        - | 1904 | `					"However, the definition differs and is considered incompatible. "` |
|        - | 1905 | `					"Class was composed",` |
|        4 | 1906 | `					&pHolder->sName,&pTrait->sName,pName,&pClass->sName);` |
|        6 | 1907 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1908 | `					goto cleanup;` |
|        - | 1909 | `				}` |
|        2 | 1910 | `			}` |
|       24 | 1911 | `			continue;` |
|        - | 1912 | `		}` |
|      103 | 1913 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - | 1914 | `			/* One slot per composing class (see VmCloneTraitAttr). */` |
|       24 | 1915 | `			ph7_class_attr *pOwnCopy = VmCloneTraitAttr(pGen->pVm,pAttr);` |
|       24 | 1916 | `			if( pOwnCopy == 0 ){` |
|      ! 0 | 1917 | `				rc = SXERR_MEM;` |
|      ! 0 | 1918 | `				goto cleanup;` |
|        - | 1919 | `			}` |
|       24 | 1920 | `			pAttr = pOwnCopy;` |
|       11 | 1921 | `		}` |
|      103 | 1922 | `		rc = SyHashInsertTail(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|      103 | 1923 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1924 | `			goto cleanup;` |
|        - | 1925 | `		}` |
|        - | 1926 | `		/* A trait's private is the COMPOSING class's own (php composes it in), so` |
|        - | 1927 | `		 * the mask has to name it here too. */` |
|      103 | 1928 | `		PH7_ClassNotePrivateName(pClass,pAttr);` |
|        5 | 1929 | `	}` |
|        - | 1930 | `	/* Copy constants from the trait (PHP 8.2 trait constants; separate hConst` |
|        - | 1931 | `	 * namespace). The name being taken is only a conflict when the DEFINITION differs,` |
|        - | 1932 | `	 * exactly as for a property above -- php compares the value, the visibility, the` |
|        - | 1933 | ``	 * `final` flag and the declared type, and lets two identical declarations through`` |
|        - | 1934 | ``	 * (`trait A { const K='x'; } trait B { const K='x'; }` composes fine). A definition`` |
|        - | 1935 | `	 * inherited from a BASE class is not part of the comparison: a trait constant` |
|        - | 1936 | `	 * overrides one, silently, the way a class-body constant does. */` |
|      311 | 1937 | `	SyHashResetLoopCursor(&pTrait->hConst);` |
|      345 | 1938 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hConst)) != 0 ){` |
|        - | 1939 | `		SyHashEntry *pExisting;` |
|       37 | 1940 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|       37 | 1941 | `		pName = &pAttr->sName;` |
|       37 | 1942 | `		pExisting = SyHashGet(&pClass->hConst,(const void *)pName->zString,pName->nByte);` |
|       37 | 1943 | `		if( pExisting != 0 ){` |
|       11 | 1944 | `			ph7_class_attr *pHave = (ph7_class_attr *)pExisting->pUserData;` |
|       11 | 1945 | `			ph7_class *pHolder = pHave->pDeclClass;` |
|        8 | 1946 | `			if( pHolder && pHolder != pClass` |
|        6 | 1947 | `			 && (pHolder->iFlags & PH7_CLASS_TRAIT) == 0` |
|        5 | 1948 | `			 && PH7_VmInstanceOf(pClass,pHolder) ){` |
|        - | 1949 | `				/* Inherited from a base class: the trait's definition replaces it. */` |
|      ! 0 | 1950 | `				ph7_class_attr *pOwnCopy = VmCloneTraitAttr(pGen->pVm,pAttr);` |
|      ! 0 | 1951 | `				if( pOwnCopy == 0 ){` |
|      ! 0 | 1952 | `					rc = SXERR_MEM;` |
|      ! 0 | 1953 | `					goto cleanup;` |
|        - | 1954 | `				}` |
|      ! 0 | 1955 | `				SyHashDeleteEntry2(pExisting);` |
|      ! 0 | 1956 | `				rc = SyHashInsertTail(&pClass->hConst,(const void *)pName->zString,pName->nByte,pOwnCopy);` |
|      ! 0 | 1957 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 1958 | `					goto cleanup;` |
|        - | 1959 | `				}` |
|      ! 0 | 1960 | `				continue;` |
|        - | 1961 | `			}` |
|       11 | 1962 | `			if( !VmTraitConstDefsMatch(pGen->pVm,pAttr,pHave) ){` |
|        - | 1963 | `				/* php names the FIRST definition, as the property path does. */` |
|        6 | 1964 | `				if( pHolder == 0 \|\| pHolder == pClass ){` |
|        3 | 1965 | `					ph7_class **apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|        3 | 1966 | `					sxu32 nUsed = SySetUsed(&pClass->aTrait);` |
|        - | 1967 | `					sxu32 k;` |
|        3 | 1968 | `					pHolder = pClass;` |
|        3 | 1969 | `					for(k = 0; k < nUsed; k++){` |
|      ! 0 | 1970 | `						if( PH7_ClassExtractConstant(apUsedTraits[k],pName->zString,pName->nByte) ){` |
|      ! 0 | 1971 | `							pHolder = apUsedTraits[k];` |
|      ! 0 | 1972 | `							break;` |
|        - | 1973 | `						}` |
|      ! 0 | 1974 | `					}` |
|        1 | 1975 | `				}` |
|        8 | 1976 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,` |
|        - | 1977 | `					"%z and %z define the same constant (%z) in the composition of %z. "` |
|        - | 1978 | `					"However, the definition differs and is considered incompatible. "` |
|        - | 1979 | `					"Class was composed",` |
|        4 | 1980 | `					&pHolder->sName,&pTrait->sName,pName,&pClass->sName);` |
|        6 | 1981 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1982 | `					goto cleanup;` |
|        - | 1983 | `				}` |
|        2 | 1984 | `			}` |
|       11 | 1985 | `			continue;` |
|        - | 1986 | `		}` |
|        - | 1987 | `		{` |
|        - | 1988 | ``			/* Evaluated per composing class (`const K = self::J`), so one record each. */`` |
|       29 | 1989 | `			ph7_class_attr *pOwnCopy = VmCloneTraitAttr(pGen->pVm,pAttr);` |
|       29 | 1990 | `			if( pOwnCopy == 0 ){` |
|      ! 0 | 1991 | `				rc = SXERR_MEM;` |
|      ! 0 | 1992 | `				goto cleanup;` |
|        - | 1993 | `			}` |
|       29 | 1994 | `			rc = SyHashInsertTail(&pClass->hConst,(const void *)pName->zString,pName->nByte,pOwnCopy);` |
|        - | 1995 | `		}` |
|       29 | 1996 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1997 | `			goto cleanup;` |
|        - | 1998 | `		}` |
|        3 | 1999 | `	}` |
|        - | 2000 | `	/* Copy methods from the trait. The identity copied is the trait's HASH KEY,` |
|        - | 2001 | ``	 * not sFunc.sName: an adaptation alias (`hi as bHi`) is a hash entry whose`` |
|        - | 2002 | `	 * key is the alias while the method struct keeps its original name — keying` |
|        - | 2003 | ``	 * the copy off sFunc.sName silently re-filed `bHi` under `hi`, so an alias`` |
|        - | 2004 | `	 * made inside a TRAIT vanished when that trait was composed into a class. */` |
|      311 | 2005 | `	SyHashResetLoopCursor(&pTrait->hMethod);` |
|      761 | 2006 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hMethod)) != 0 ){` |
|        - | 2007 | `		SyHashEntry *pClassMethEntry;` |
|        - | 2008 | `		SyString sKey;` |
|      455 | 2009 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      455 | 2010 | `		SyStringInitFromBuf(&sKey,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|      455 | 2011 | `		pName = &sKey;` |
|      455 | 2012 | `		pClassMethEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
|      455 | 2013 | `		if( pClassMethEntry != 0 ){` |
|        - | 2014 | `			/* Method already exists in the class. An ABSTRACT trait method is a` |
|        - | 2015 | `			 * REQUIREMENT, not an implementation: php satisfies it with any concrete` |
|        - | 2016 | `			 * method of the same name (from the class body or another trait) — no` |
|        - | 2017 | `			 * collision. Only two CONCRETE trait methods actually conflict. */` |
|       49 | 2018 | `			ph7_class_method *pExistingMeth = (ph7_class_method *)pClassMethEntry->pUserData;` |
|       49 | 2019 | `			int bIncomingAbstract = (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0;` |
|       49 | 2020 | `			int bExistingAbstract = (pExistingMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0;` |
|        - | 2021 | `			ph7_class **apUsedTraits;` |
|        - | 2022 | `			sxu32 nUsed,k;` |
|       49 | 2023 | `			if( bIncomingAbstract ){` |
|        - | 2024 | `				/* Incoming abstract requirement: the existing (concrete or abstract)` |
|        - | 2025 | `				 * method already covers this name — keep it. */` |
|       31 | 2026 | `				continue;` |
|        - | 2027 | `			}` |
|       36 | 2028 | `			if( bExistingAbstract ){` |
|        - | 2029 | `				/* Existing entry is only an abstract requirement (from an earlier` |
|        - | 2030 | `				 * trait): the incoming concrete method satisfies and replaces it. */` |
|        5 | 2031 | `				pClassMethEntry->pUserData = (void *)pMeth;` |
|        5 | 2032 | `				continue;` |
|        - | 2033 | `			}` |
|        - | 2034 | ``			/* Two names are not two METHODS. A trait that `use`s another trait`` |
|        - | 2035 | `			 * flattens it by sharing the very ph7_class_method the origin trait` |
|        - | 2036 | `			 * compiled, so a method reaching the class down two composition paths` |
|        - | 2037 | `			 * arrives as the SAME struct both times -- which is php's own test` |
|        - | 2038 | `			 * (zend compares the two functions' op_array.opcodes) and why` |
|        - | 2039 | ``			 * `trait TB { use TA; } class M { use TB, TA; }` composes there and`` |
|        - | 2040 | `			 * fatalled here. Only two genuinely different definitions collide. */` |
|       32 | 2041 | `			if( pExistingMeth == pMeth ){` |
|       15 | 2042 | `				continue;` |
|        - | 2043 | `			}` |
|        - | 2044 | `			/* A method the class declares ITSELF wins over every trait, however many` |
|        - | 2045 | `			 * of them offer the name: php reports no collision at all for` |
|        - | 2046 | ``			 * `class M { use TA, TB; public function m(){} }`, where PHL raised one`` |
|        - | 2047 | `			 * as soon as the second trait arrived. */` |
|       18 | 2048 | `			if( (ph7_class *)pExistingMeth->sFunc.pUserData == pClass ){` |
|       13 | 2049 | `				continue;` |
|        - | 2050 | `			}` |
|        - | 2051 | `			/* Both concrete: a genuine collision only when the OTHER definition came` |
|        - | 2052 | `			 * from another trait (a concrete one). A class-body method wins silently. */` |
|        5 | 2053 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|        5 | 2054 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|        5 | 2055 | `			for(k = 0; k < nUsed; k++){` |
|        5 | 2056 | `				ph7_class_method *pOtherMeth = PH7_ClassExtractMethod(apUsedTraits[k],pName->zString,pName->nByte);` |
|        4 | 2057 | `				if( pOtherMeth != 0 && pOtherMeth != pMeth` |
|        5 | 2058 | `				 && (pOtherMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|        - | 2059 | `					/* Two different traits define the same CONCRETE method with no` |
|        - | 2060 | `					 * resolution. php reports the line of the COMPOSING class, not` |
|        - | 2061 | `					 * the one the losing definition was written on. */` |
|        7 | 2062 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,` |
|        - | 2063 | `						"Trait method %z::%z has not been applied as %z::%z, "` |
|        - | 2064 | `						"because of collision with %z::%z",` |
|        4 | 2065 | `						&pTrait->sName,pName,` |
|        2 | 2066 | `						&pClass->sName,pName,` |
|        4 | 2067 | `						&apUsedTraits[k]->sName,pName);` |
|        5 | 2068 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 2069 | `						goto cleanup;` |
|        - | 2070 | `					}` |
|        5 | 2071 | `					break;` |
|        - | 2072 | `				}` |
|      ! 0 | 2073 | `			}` |
|        - | 2074 | `			/* Class-defined method takes precedence */` |
|        5 | 2075 | `			continue;` |
|        - | 2076 | `		}` |
|      411 | 2077 | `		rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|      411 | 2078 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 2079 | `			goto cleanup;` |
|        - | 2080 | `		}` |
|        5 | 2081 | `	}` |
|        - | 2082 | `	/* Record trait in the class */` |
|      311 | 2083 | `	SySetPut(&pClass->aTrait,(const void *)&pTrait);` |
|      153 | 2084 | `cleanup:` |
|        - | 2085 | `	/* Always clear visiting flag, even on error paths */` |
|      311 | 2086 | `	pTrait->iFlags &= ~PH7_CLASS_TRAIT_VISITING;` |
|      153 | 2087 | `	SXUNUSED(pGen);` |
|      311 | 2088 | `	return rc;` |
|      158 | 2089 | `}` |
|        - | 2090 | `/*` |
|        - | 2091 | ` * Inherit an object interface from another object interface.` |
|        - | 2092 | ` * According to the PHP language reference manual.` |
|        - | 2093 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|        - | 2094 | ` *  must implement, without having to define how these methods are handled.` |
|        - | 2095 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|        - | 2096 | ` *  class, but without any of the methods having their contents defined.` |
|        - | 2097 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|        - | 2098 | ` *` |
|        - | 2099 | ` * This function return SXRET_OK if the interface inheritance operation was successfully performed.` |
|        - | 2100 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|        - | 2101 | ` * error message.` |
|        - | 2102 | ` */` |
|    60537 | 2103 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceInherit(ph7_class *pSub,ph7_class *pBase)` |
|        5 | 2104 | `{` |
|        - | 2105 | `	ph7_class_method *pMeth;` |
|        - | 2106 | `	ph7_class_attr *pAttr;` |
|        - | 2107 | `	SyHashEntry *pEntry;` |
|        - | 2108 | `	SyString *pName;` |
|        - | 2109 | `	sxi32 rc;` |
|        - | 2110 | `	/* Install in the derived hashtable */` |
|    60542 | 2111 | `	SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|    60542 | 2112 | `	SyHashResetLoopCursor(&pBase->hConst);` |
|        - | 2113 | `	/* Copy constants (interfaces carry only constants + method signatures) */` |
|    90857 | 2114 | `	while((pEntry = SyHashGetNextEntry(&pBase->hConst)) != 0 ){` |
|        - | 2115 | `		/* Make sure the constants are not redeclared in the subclass */` |
|        7 | 2116 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|        7 | 2117 | `		pName = &pAttr->sName;` |
|        7 | 2118 | `		if( SyHashGet(&pSub->hConst,(const void *)pName->zString,pName->nByte) == 0 ){` |
|        - | 2119 | `			/* Install the constant in the subclass */` |
|        3 | 2120 | `			rc = SyHashInsertTail(&pSub->hConst,(const void *)pName->zString,pName->nByte,pAttr);` |
|        3 | 2121 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 2122 | `				return rc;` |
|        - | 2123 | `			}` |
|        1 | 2124 | `		}` |
|        1 | 2125 | `	}` |
|    60542 | 2126 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
|        - | 2127 | `	/* Copy methods signature */` |
|   218624 | 2128 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|        - | 2129 | `		/* Make sure the method are not redeclared in the subclass */` |
|   127778 | 2130 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   127778 | 2131 | `		pName = &pMeth->sFunc.sName;` |
|   127778 | 2132 | `		if( SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte) == 0 ){` |
|        - | 2133 | `			/* Install the method */` |
|   127772 | 2134 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|   127772 | 2135 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 2136 | `				return rc;` |
|        - | 2137 | `			}` |
|    63798 | 2138 | `		}` |
|        5 | 2139 | `	}` |
|        - | 2140 | `	/* Mark as subclass */` |
|    60542 | 2141 | `	pSub->pBase = pBase;` |
|        - | 2142 | `	/* All done */` |
|    60542 | 2143 | `	return SXRET_OK;` |
|    30233 | 2144 | `}` |
|        - | 2145 | `/*` |
|        - | 2146 | ` * Implements an object interface in the given main class.` |
|        - | 2147 | ` * According to the PHP language reference manual.` |
|        - | 2148 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|        - | 2149 | ` *  must implement, without having to define how these methods are handled.` |
|        - | 2150 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|        - | 2151 | ` *  class, but without any of the methods having their contents defined.` |
|        - | 2152 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|        - | 2153 | ` *` |
|        - | 2154 | ` * This function return SXRET_OK if the interface was successfully implemented.` |
|        - | 2155 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|        - | 2156 | ` * error message.` |
|        - | 2157 | ` */` |
|   619274 | 2158 | `PH7_PRIVATE sxi32 PH7_ClassImplement(ph7_class *pMain,ph7_class *pInterface)` |
|        5 | 2159 | `{` |
|        - | 2160 | `	ph7_class_attr *pAttr;` |
|        - | 2161 | `	SyHashEntry *pEntry;` |
|        - | 2162 | `	SyString *pName;` |
|        - | 2163 | `	sxi32 rc;` |
|        - | 2164 | `	/* First off,copy all constants declared inside the interface (hConst namespace) */` |
|   619279 | 2165 | `	SyHashResetLoopCursor(&pInterface->hConst);` |
|  1117546 | 2166 | `	while((pEntry = SyHashGetNextEntry(&pInterface->hConst)) != 0 ){` |
|        - | 2167 | `		/* Point to the constant declaration */` |
|   188221 | 2168 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   188221 | 2169 | `		pName = &pAttr->sName;` |
|        - | 2170 | `		/* Make sure the constant is not redeclared in the main class */` |
|   188221 | 2171 | `		if( SyHashGet(&pMain->hConst,pName->zString,pName->nByte) == 0 ){` |
|        - | 2172 | `			/* Install the constant */` |
|   188219 | 2173 | `			rc = SyHashInsertTail(&pMain->hConst,pName->zString,pName->nByte,pAttr);` |
|   188219 | 2174 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 2175 | `				return rc;` |
|        - | 2176 | `			}` |
|    93981 | 2177 | `		}` |
|        5 | 2178 | `	}` |
|        - | 2179 | `	/* Install in the interface container */` |
|   619279 | 2180 | `	SySetPut(&pMain->aInterface,(const void *)&pInterface);` |
|        - | 2181 | `	/* Install interface method stubs into the implementing class.` |
|        - | 2182 | `	 * Methods already defined in the class take precedence (they satisfy` |
|        - | 2183 | `	 * the interface contract). Stubs retain PH7_CLASS_ATTR_ABSTRACT so` |
|        - | 2184 | `	 * the unified check in GenStateCheckAbstractMethods catches missing ones.` |
|        - | 2185 | `	 */` |
|        - | 2186 | `	{` |
|        - | 2187 | `		ph7_class_method *pMeth;` |
|        - | 2188 | `		SyHashEntry *pMEntry;` |
|        - | 2189 | `		SyString *pMName;` |
|   619279 | 2190 | `		SyHashResetLoopCursor(&pInterface->hMethod);` |
|  2799224 | 2191 | `		while((pMEntry = SyHashGetNextEntry(&pInterface->hMethod)) != 0 ){` |
|  1869899 | 2192 | `			pMeth = (ph7_class_method *)pMEntry->pUserData;` |
|  1869899 | 2193 | `			pMName = &pMeth->sFunc.sName;` |
|  1869899 | 2194 | `			if( SyHashGet(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte) == 0 ){` |
|     6764 | 2195 | `				rc = SyHashInsert(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte,pMeth);` |
|     6764 | 2196 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 2197 | `					return rc;` |
|        - | 2198 | `				}` |
|     3375 | 2199 | `			}` |
|        5 | 2200 | `		}` |
|        - | 2201 | `	}` |
|   619279 | 2202 | `	return SXRET_OK;` |
|   309228 | 2203 | `}` |
|        - | 2204 | `/*` |
|        - | 2205 | ` * Create a class instance [i.e: Object in the PHP jargon] at run-time.` |
|        - | 2206 | ` * The following function is called when an object is created at run-time` |
|        - | 2207 | ` * typically when the PH7_OP_NEW/PH7_OP_CLONE instructions are executed.` |
|        - | 2208 | ` * Notes on object creation.` |
|        - | 2209 | ` *` |
|        - | 2210 | ` * According to PHP language reference manual.` |
|        - | 2211 | ` * To create an instance of a class, the new keyword must be used. An object will always` |
|        - | 2212 | ` * be created unless the object has a constructor defined that throws an exception on error.` |
|        - | 2213 | ` * Classes should be defined before instantiation (and in some cases this is a requirement).` |
|        - | 2214 | ` * If a string containing the name of a class is used with new, a new instance of that class` |
|        - | 2215 | ` * will be created. If the class is in a namespace, its fully qualified name must be used when` |
|        - | 2216 | ` * doing this.` |
|        - | 2217 | ` * Example #3 Creating an instance` |
|        - | 2218 | ` * <?php` |
|        - | 2219 | ` *  $instance = new SimpleClass();` |
|        - | 2220 | ` *   // This can also be done with a variable:` |
|        - | 2221 | ` * $className = 'Foo';` |
|        - | 2222 | ` * $instance = new $className(); // Foo()` |
|        - | 2223 | ` * ?>` |
|        - | 2224 | ` * In the class context, it is possible to create a new object by new self and new parent.` |
|        - | 2225 | ` * When assigning an already created instance of a class to a new variable, the new variable` |
|        - | 2226 | ` * will access the same instance as the object that was assigned. This behaviour is the same` |
|        - | 2227 | ` * when passing instances to a function. A copy of an already created object can be made by` |
|        - | 2228 | ` * cloning it.` |
|        - | 2229 | ` * Example #4 Object Assignment` |
|        - | 2230 | ` * <?php` |
|        - | 2231 | ` *  class SimpleClass(){` |
|        - | 2232 | ` *    public $var;` |
|        - | 2233 | ` *  };` |
|        - | 2234 | ` *  $instance = new SimpleClass();` |
|        - | 2235 | ` *  $assigned   =  $instance;` |
|        - | 2236 | ` *  $reference  =& $instance;` |
|        - | 2237 | ` *  $instance->var = '$assigned will have this value';` |
|        - | 2238 | ` *  $instance = null; // $instance and $reference become null` |
|        - | 2239 | ` *  var_dump($instance);` |
|        - | 2240 | ` *  var_dump($reference);` |
|        - | 2241 | ` *  var_dump($assigned);` |
|        - | 2242 | ` * ?>` |
|        - | 2243 | ` * The above example will output:` |
|        - | 2244 | ` * NULL` |
|        - | 2245 | ` * NULL` |
|        - | 2246 | ` * object(SimpleClass)#1 (1) {` |
|        - | 2247 | ` *  ["var"]=>` |
|        - | 2248 | ` *    string(30) "$assigned will have this value"` |
|        - | 2249 | ` * }` |
|        - | 2250 | ` * Example #5 Creating new objects` |
|        - | 2251 | ` * <?php` |
|        - | 2252 | ` * class Test` |
|        - | 2253 | ` * {` |
|        - | 2254 | ` *   static public function getNew()` |
|        - | 2255 | ` *   {` |
|        - | 2256 | ` *       return new static;` |
|        - | 2257 | ` *   }` |
|        - | 2258 | ` * }` |
|        - | 2259 | ` * class Child extends Test` |
|        - | 2260 | ` * {}` |
|        - | 2261 | ` * $obj1 = new Test();` |
|        - | 2262 | ` * $obj2 = new $obj1;` |
|        - | 2263 | ` * var_dump($obj1 !== $obj2);` |
|        - | 2264 | ` * $obj3 = Test::getNew();` |
|        - | 2265 | ` * var_dump($obj3 instanceof Test);` |
|        - | 2266 | ` * $obj4 = Child::getNew();` |
|        - | 2267 | ` * var_dump($obj4 instanceof Child);` |
|        - | 2268 | ` * ?>` |
|        - | 2269 | ` * The above example will output:` |
|        - | 2270 | ` * bool(true)` |
|        - | 2271 | ` * bool(true)` |
|        - | 2272 | ` * bool(true)` |
|        - | 2273 | ` * Note that Symisc Systems have introduced powerfull extension to` |
|        - | 2274 | ` * OO subsystem. For example a class attribute may have any complex` |
|        - | 2275 | ` * expression associated with it when declaring the attribute unlike` |
|        - | 2276 | ` * the standard PHP engine which would allow a single value.` |
|        - | 2277 | ` * Example:` |
|        - | 2278 | ` *  class myClass{` |
|        - | 2279 | ` *    public $var = 25<<1+foo()/bar();` |
|        - | 2280 | ` *  };` |
|        - | 2281 | ` * Refer to the official documentation for more information.` |
|        - | 2282 | ` */` |
|  1621673 | 2283 | `static ph7_class_instance * NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|        5 | 2284 | `{` |
|        - | 2285 | `	ph7_class_instance *pThis;` |
|        - | 2286 | `	/* Allocate a new instance */` |
|  1621678 | 2287 | `	pThis = (ph7_class_instance *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_instance));` |
|  1621678 | 2288 | `	if( pThis == 0 ){` |
|      ! 0 | 2289 | `		return 0;` |
|        - | 2290 | `	}` |
|        - | 2291 | `	/* Zero the structure */` |
|  1621678 | 2292 | `	SyZero(pThis,sizeof(ph7_class_instance));` |
|        - | 2293 | `	/* Initialize fields */` |
|  1621678 | 2294 | `	pThis->iRef = 1;` |
|  1621678 | 2295 | `	pThis->pVm = pVm;` |
|  1621678 | 2296 | `	pThis->pClass = pClass;` |
|        - | 2297 | `	/* Assign a fresh monotonic object handle id (clones get their own, like PHP). */` |
|  1621678 | 2298 | `	pThis->nObjId = pVm->nNextObjId++;` |
|  1621678 | 2299 | `	SyHashInit(&pThis->hAttr,&pVm->sAllocator,0,0);` |
|  1621678 | 2300 | `	return pThis;` |
|   810674 | 2301 | `}` |
|        - | 2302 | `/*` |
|        - | 2303 | ` * Wrapper around the NewClassInstance() function defined above.` |
|        - | 2304 | ` * See the block comment above for more information.` |
|        - | 2305 | ` */` |
|  1620607 | 2306 | `PH7_PRIVATE ph7_class_instance * PH7_NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|        5 | 2307 | `{` |
|        - | 2308 | `	ph7_class_instance *pNew;` |
|        - | 2309 | `	sxi32 rc;` |
|  1620612 | 2310 | `	pNew = NewClassInstance(&(*pVm),&(*pClass));` |
|  1620612 | 2311 | `	if( pNew == 0 ){` |
|      ! 0 | 2312 | `		return 0;` |
|        - | 2313 | `	}` |
|        - | 2314 | `	/* Associate a private VM frame with this class instance */` |
|  1620612 | 2315 | `	rc = PH7_VmCreateClassInstanceFrame(&(*pVm),pNew);` |
|  1620612 | 2316 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 2317 | `		SyMemBackendPoolFree(&pVm->sAllocator,pNew);` |
|      ! 0 | 2318 | `		return 0;` |
|        - | 2319 | `	}` |
|        - | 2320 | `	/* php stamps a Throwable's file/line at CREATION, not in its constructor, so a` |
|        - | 2321 | `	 * subclass that overrides __construct without calling parent::__construct still` |
|        - | 2322 | `	 * reports the right site. Every instantiation path lands here. */` |
|  1620612 | 2323 | `	PH7_VmStampThrowableSite(&(*pVm),pNew);` |
|        - | 2324 | `	/* php's create_object handler, resolved through the ANCESTORS the way the` |
|        - | 2325 | `	 * teardown one is: a subclass of a native class whose slots are SEEDED at` |
|        - | 2326 | ``	 * `new` (ZipArchive's six) must start with the same six. */`` |
|        - | 2327 | `	{` |
|  1620612 | 2328 | `		ph7_class *pOwner = pClass;` |
|  3374547 | 2329 | `		while( pOwner && pOwner->xNew == 0 ){` |
|  1753940 | 2330 | `			pOwner = pOwner->pBase;` |
|        5 | 2331 | `		}` |
|  1620612 | 2332 | `		if( pOwner && pOwner->xNew ){` |
|      366 | 2333 | `			pOwner->xNew(&(*pVm),pNew);` |
|      181 | 2334 | `		}` |
|        - | 2335 | `	}` |
|  1620612 | 2336 | `	return pNew;` |
|   810141 | 2337 | `}` |
|        - | 2338 | `/*` |
|        - | 2339 | ` * Open a private walk of this object's property table.` |
|        - | 2340 | ` *` |
|        - | 2341 | ` * Every consumer that hands PHP code the control flow between two attributes --` |
|        - | 2342 | `` * `foreach ($o as $k => $v)`, `array_walk($o, $fn)` -- must own its position`` |
|        - | 2343 | ` * rather than share the SyHash's embedded cursor: php iterates each walk` |
|        - | 2344 | ` * independently (nested loops over one object do not rewind each other), and the` |
|        - | 2345 | ` * body it runs in between can add or remove a property. The instance keeps the` |
|        - | 2346 | ` * list of open walks so those two mutations can fix the cursors up; the walker` |
|        - | 2347 | ` * MUST close it on every exit path, or the next mutation walks a recycled slot.` |
|        - | 2348 | ` */` |
|      146 | 2349 | `PH7_PRIVATE void PH7_ClassInstanceIterOpen(ph7_class_instance *pThis,PH7_AttrIter *pIter)` |
|        4 | 2350 | `{` |
|      150 | 2351 | `	pIter->pCursor = SyHashFirstEntry(&pThis->hAttr);` |
|      150 | 2352 | `	pIter->pNextIter = pThis->pActiveIters;` |
|      150 | 2353 | `	pThis->pActiveIters = pIter;` |
|      150 | 2354 | `}` |
|        - | 2355 | `/*` |
|        - | 2356 | ` * The next attribute entry, or 0 when the walk is exhausted. The cursor is` |
|        - | 2357 | ` * advanced BEFORE the entry is handed out, exactly like SyHashGetNextEntry:` |
|        - | 2358 | ` * php's own iteration standing on an entry is free to unset() it.` |
|        - | 2359 | ` */` |
|      670 | 2360 | `PH7_PRIVATE SyHashEntry * PH7_ClassInstanceIterNext(PH7_AttrIter *pIter)` |
|        4 | 2361 | `{` |
|      674 | 2362 | `	SyHashEntry *pEntry = pIter->pCursor;` |
|      674 | 2363 | `	if( pEntry ){` |
|      534 | 2364 | `		pIter->pCursor = SyHashEntryNext(pEntry);` |
|      265 | 2365 | `	}` |
|      674 | 2366 | `	return pEntry;` |
|        4 | 2367 | `}` |
|      144 | 2368 | `PH7_PRIVATE void PH7_ClassInstanceIterClose(ph7_class_instance *pThis,PH7_AttrIter *pIter)` |
|        4 | 2369 | `{` |
|      148 | 2370 | `	PH7_AttrIter **ppLink = &pThis->pActiveIters;` |
|      150 | 2371 | `	while( *ppLink ){` |
|      150 | 2372 | `		if( *ppLink == pIter ){` |
|      148 | 2373 | `			*ppLink = pIter->pNextIter;` |
|      148 | 2374 | `			pIter->pNextIter = 0;` |
|      148 | 2375 | `			pIter->pCursor = 0;` |
|      148 | 2376 | `			return;` |
|        - | 2377 | `		}` |
|        3 | 2378 | `		ppLink = &(*ppLink)->pNextIter;` |
|        1 | 2379 | `	}` |
|       76 | 2380 | `}` |
|        - | 2381 | `/*` |
|        - | 2382 | ` * Remove one attribute entry from the instance, advancing any open walk parked` |
|        - | 2383 | `` * on it first. The ONLY door for an `unset($o->p)`-shaped removal: the entry is`` |
|        - | 2384 | ` * freed here, so a walker still holding it would read a recycled pool slot on` |
|        - | 2385 | ` * its next step (php visits the properties AFTER the deleted one, and so does` |
|        - | 2386 | ` * this).` |
|        - | 2387 | ` */` |
|       70 | 2388 | `PH7_PRIVATE void PH7_ClassInstanceDeleteAttrEntry(ph7_class_instance *pThis,SyHashEntry *pEntry)` |
|        2 | 2389 | `{` |
|        - | 2390 | `	PH7_AttrIter *pIter;` |
|       88 | 2391 | `	for( pIter = pThis->pActiveIters ; pIter ; pIter = pIter->pNextIter ){` |
|       17 | 2392 | `		if( pIter->pCursor == pEntry ){` |
|        9 | 2393 | `			pIter->pCursor = SyHashEntryNext(pEntry);` |
|        4 | 2394 | `		}` |
|        9 | 2395 | `	}` |
|       72 | 2396 | `	SyHashDeleteEntry2(pEntry);` |
|       72 | 2397 | `}` |
|        - | 2398 | `/*` |
|        - | 2399 | ` * The mirror: an attribute APPENDED to the table (a dynamic property created by` |
|        - | 2400 | ` * the loop body, a declared one re-created after unset()) re-arms any walk that` |
|        - | 2401 | `` * has run off the end -- php walks the LIVE table, so `foreach ($o as ...)` over`` |
|        - | 2402 | ` * a stdClass whose body keeps adding properties keeps visiting them. A walker` |
|        - | 2403 | ` * with a NULL cursor is always mid-walk: it unregisters as soon as it stops.` |
|        - | 2404 | ` */` |
|     6472 | 2405 | `PH7_PRIVATE void PH7_ClassInstanceAttrAppended(ph7_class_instance *pThis,SyHashEntry *pEntry)` |
|        5 | 2406 | `{` |
|        - | 2407 | `	PH7_AttrIter *pIter;` |
|     6477 | 2408 | `	if( pEntry == 0 ){` |
|       13 | 2409 | `		return;` |
|        - | 2410 | `	}` |
|     6471 | 2411 | `	for( pIter = pThis->pActiveIters ; pIter ; pIter = pIter->pNextIter ){` |
|        7 | 2412 | `		if( pIter->pCursor == 0 ){` |
|        7 | 2413 | `			pIter->pCursor = pEntry;` |
|        3 | 2414 | `		}` |
|        4 | 2415 | `	}` |
|     3241 | 2416 | `}` |
|        - | 2417 | `/*` |
|        - | 2418 | ` * Spell ONE attribute the way php names it wherever an object's property table is` |
|        - | 2419 | ` * handed out as keys: a private property is "\0DeclaringClass\0name", a protected` |
|        - | 2420 | ` * one "\0*\0name", a public one its bare name. The NULs are real bytes (these` |
|        - | 2421 | ` * appends are length-based), which is what keeps two same-named members from` |
|        - | 2422 | `` * different visibility levels distinct. `pKey` must already be a STRING value; its`` |
|        - | 2423 | ` * buffer is reset first, so one carrier serves a whole walk.` |
|        - | 2424 | ` */` |
|     1236 | 2425 | `PH7_PRIVATE void PH7_ClassInstanceAttrKey(ph7_class_instance *pThis,VmClassAttr *pAttr,ph7_value *pKey)` |
|        5 | 2426 | `{` |
|     1241 | 2427 | `	SyString *pAttrName = &pAttr->pAttr->sName;` |
|     1241 | 2428 | `	SyBlobReset(&pKey->sBlob);` |
|     1241 | 2429 | `	if( pAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|        - | 2430 | `		/* php mangles a private key with the class that OWNS the property, and a` |
|        - | 2431 | `		 * trait's members are owned by the class that composed them -- so the key,` |
|        - | 2432 | `		 * and every wire format built on it (serialize, the (array) cast), names` |
|        - | 2433 | `		 * the class and never the trait. */` |
|      207 | 2434 | `		ph7_class *pDecl = PH7_VmMemberOwnerClass(pAttr->pAttr->pDeclClass,pThis->pClass);` |
|      207 | 2435 | `		PH7_MemObjStringAppend(pKey,"\0",1);` |
|      207 | 2436 | `		PH7_MemObjStringAppend(pKey,SyStringData(&pDecl->sName),SyStringLength(&pDecl->sName));` |
|      207 | 2437 | `		PH7_MemObjStringAppend(pKey,"\0",1);` |
|     1139 | 2438 | `	}else if( pAttr->pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
|      237 | 2439 | `		PH7_MemObjStringAppend(pKey,"\0*\0",3);` |
|      117 | 2440 | `	}` |
|     1241 | 2441 | `	PH7_MemObjStringAppend(pKey,pAttrName->zString,pAttrName->nByte);` |
|     1241 | 2442 | `}` |
|        - | 2443 | `/*` |
|        - | 2444 | ` * Is this slot part of the RAW property table php hands a walker -- the (array)` |
|        - | 2445 | ` * cast's slot walk, get_mangled_object_vars(), array_walk() over an object? A` |
|        - | 2446 | ` * class-level member is not the object's, a typed property never written is not` |
|        - | 2447 | ` * there yet, and a php 8.4 VIRTUAL hooked property has no backing store at all.` |
|        - | 2448 | ` */` |
|     5698 | 2449 | `PH7_PRIVATE int PH7_ClassInstanceAttrPresented(VmClassAttr *pAttr)` |
|        5 | 2450 | `{` |
|     4626 | 2451 | `	return !PH7_ATTR_UNPRESENTED(pAttr)` |
|     1772 | 2452 | `		&& !PH7_ClassAttrUninitialized(pAttr)` |
|     6620 | 2453 | `		&& (pAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL) == 0;` |
|        5 | 2454 | `}` |
|        - | 2455 | `/*` |
|        - | 2456 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon] attribute.` |
|        - | 2457 | ` * This function never fail.` |
|        - | 2458 | ` */` |
|  7909186 | 2459 | `static ph7_value * ExtractClassAttrValue(ph7_vm *pVm,VmClassAttr *pAttr)` |
|        5 | 2460 | `{` |
|        - | 2461 | `	/* Extract the value */` |
|        - | 2462 | `	ph7_value *pValue;` |
|  7909191 | 2463 | `	pValue = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pAttr->nIdx);` |
|  7909191 | 2464 | `	return pValue;` |
|        5 | 2465 | `}` |
|        - | 2466 | `/*` |
|        - | 2467 | ` * Perform a clone operation on a class instance [i.e: Object in the PHP jargon].` |
|        - | 2468 | ` * The following function is called when an object is cloned at run-time` |
|        - | 2469 | ` * typically when the PH7_OP_CLONE instruction is executed.` |
|        - | 2470 | ` * Notes on object cloning.` |
|        - | 2471 | ` *` |
|        - | 2472 | ` * According to PHP language reference manual.` |
|        - | 2473 | ` * Creating a copy of an object with fully replicated properties is not always the wanted behavior.` |
|        - | 2474 | ` * A good example of the need for copy constructors. Another example is if your object holds a reference` |
|        - | 2475 | ` * to another object which it uses and when you replicate the parent object you want to create` |
|        - | 2476 | ` * a new instance of this other object so that the replica has its own separate copy.` |
|        - | 2477 | ` * An object copy is created by using the clone keyword (which calls the object's __clone() method if possible).` |
|        - | 2478 | ` * An object's __clone() method cannot be called directly.` |
|        - | 2479 | ` * $copy_of_object = clone $object;` |
|        - | 2480 | ` * When an object is cloned, PHP 5 will perform a shallow copy of all of the object's properties.` |
|        - | 2481 | ` * Any properties that are references to other variables, will remain references.` |
|        - | 2482 | ` * Once the cloning is complete, if a __clone() method is defined, then the newly created object's __clone() method` |
|        - | 2483 | ` * will be called, to allow any necessary properties that need to be changed.` |
|        - | 2484 | ` * Example #1 Cloning an object` |
|        - | 2485 | ` * <?php` |
|        - | 2486 | ` * class SubObject` |
|        - | 2487 | ` * {` |
|        - | 2488 | ` *   static $instances = 0;` |
|        - | 2489 | ` *   public $instance;` |
|        - | 2490 | ` *` |
|        - | 2491 | ` *   public function __construct() {` |
|        - | 2492 | ` *       $this->instance = ++self::$instances;` |
|        - | 2493 | ` *   }` |
|        - | 2494 | ` *` |
|        - | 2495 | ` *   public function __clone() {` |
|        - | 2496 | ` *       $this->instance = ++self::$instances;` |
|        - | 2497 | ` *   }` |
|        - | 2498 | ` * }` |
|        - | 2499 | ` *` |
|        - | 2500 | ` * class MyCloneable` |
|        - | 2501 | ` * {` |
|        - | 2502 | ` *   public $object1;` |
|        - | 2503 | ` *   public $object2;` |
|        - | 2504 | ` *` |
|        - | 2505 | ` *   function __clone()` |
|        - | 2506 | ` *   {` |
|        - | 2507 | ` *       // Force a copy of this->object, otherwise` |
|        - | 2508 | ` *       // it will point to same object.` |
|        - | 2509 | ` *       $this->object1 = clone $this->object1;` |
|        - | 2510 | ` *   }` |
|        - | 2511 | ` * }` |
|        - | 2512 | ` * $obj = new MyCloneable();` |
|        - | 2513 | ` * $obj->object1 = new SubObject();` |
|        - | 2514 | ` * $obj->object2 = new SubObject();` |
|        - | 2515 | ` * $obj2 = clone $obj;` |
|        - | 2516 | ` * print("Original Object:\n");` |
|        - | 2517 | ` * print_r($obj);` |
|        - | 2518 | ` * print("Cloned Object:\n");` |
|        - | 2519 | ` * print_r($obj2);` |
|        - | 2520 | ` * ?>` |
|        - | 2521 | ` * The above example will output:` |
|        - | 2522 | ` * Original Object:` |
|        - | 2523 | ` * MyCloneable Object` |
|        - | 2524 | ` * (` |
|        - | 2525 | ` *   [object1] => SubObject Object` |
|        - | 2526 | ` *       (` |
|        - | 2527 | ` *           [instance] => 1` |
|        - | 2528 | ` *       )` |
|        - | 2529 | ` *` |
|        - | 2530 | ` *   [object2] => SubObject Object` |
|        - | 2531 | ` *       (` |
|        - | 2532 | ` *           [instance] => 2` |
|        - | 2533 | ` *       )` |
|        - | 2534 | ` *` |
|        - | 2535 | ` * )` |
|        - | 2536 | ` * Cloned Object:` |
|        - | 2537 | ` * MyCloneable Object` |
|        - | 2538 | ` * (` |
|        - | 2539 | ` *   [object1] => SubObject Object` |
|        - | 2540 | ` *       (` |
|        - | 2541 | ` *           [instance] => 3` |
|        - | 2542 | ` *       )` |
|        - | 2543 | ` *` |
|        - | 2544 | ` *   [object2] => SubObject Object` |
|        - | 2545 | ` *       (` |
|        - | 2546 | ` *           [instance] => 2` |
|        - | 2547 | ` *       )` |
|        - | 2548 | ` * )` |
|        - | 2549 | ` */` |
|        - | 2550 | `/*` |
|        - | 2551 | `` * Is `clone` refused for this class? php's uncloneable internal classes refuse`` |
|        - | 2552 | `` * for their USER SUBCLASSES too -- `class M extends IteratorIterator {}` makes`` |
|        - | 2553 | `` * `clone $m` the same catchable Error, named after M -- because the refusal is`` |
|        - | 2554 | ` * the inherited clone_obj handler, not the class's own row. So the flag is` |
|        - | 2555 | ` * consulted up the base chain, not on the instance's class alone. (A subclass` |
|        - | 2556 | ` * declaring its own __clone() changes nothing there either: php never reaches` |
|        - | 2557 | ` * it, and neither does this engine -- the refusal answers first.)` |
|        - | 2558 | ` */` |
|      488 | 2559 | `PH7_PRIVATE int PH7_ClassIsUncloneable(ph7_class *pClass)` |
|        5 | 2560 | `{` |
|        - | 2561 | `	ph7_class *pC;` |
|      895 | 2562 | `	for( pC = pClass ; pC ; pC = pC->pBase ){` |
|      563 | 2563 | `		if( pC->iFlags & PH7_CLASS_NOCLONE ){` |
|      159 | 2564 | `			return 1;` |
|        - | 2565 | `		}` |
|      206 | 2566 | `	}` |
|      337 | 2567 | `	return 0;` |
|      249 | 2568 | `}` |
|     1066 | 2569 | `PH7_PRIVATE ph7_class_instance * PH7_CloneClassInstance(ph7_class_instance *pSrc)` |
|        5 | 2570 | `{` |
|        - | 2571 | `	ph7_class_instance *pClone;` |
|        - | 2572 | `	ph7_class_method *pMethod;` |
|        - | 2573 | `	SyHashEntry *pEntry2;` |
|        - | 2574 | `	SyHashEntry *pEntry;` |
|        - | 2575 | `	ph7_vm *pVm;` |
|        - | 2576 | `	sxi32 rc;` |
|        - | 2577 | `	/* Allocate a new instance */` |
|     1071 | 2578 | `	pVm = pSrc->pVm;` |
|     1071 | 2579 | `	pClone = NewClassInstance(pVm,pSrc->pClass);` |
|     1071 | 2580 | `	if( pClone == 0 ){` |
|      ! 0 | 2581 | `		return 0;` |
|        - | 2582 | `	}` |
|        - | 2583 | `	/* Associate a private VM frame with this class instance */` |
|     1071 | 2584 | `	rc = PH7_VmCreateClassInstanceFrame(pVm,pClone);` |
|     1071 | 2585 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 2586 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClone);` |
|      ! 0 | 2587 | `		return 0;` |
|        - | 2588 | `	}` |
|        - | 2589 | `	/* A clone of an object whose LAZY native properties are installed has them` |
|        - | 2590 | `	 * too: php clones the C struct the table is written from, so the copy shows` |
|        - | 2591 | ``	 * what the original shows. The frame above skipped them (as it does at `new`),`` |
|        - | 2592 | `	 * so install them before the value copy below looks for the same-named slots. */` |
|     1071 | 2593 | `	if( pSrc->iFlags & VM_INSTANCE_LAZY_DONE ){` |
|        7 | 2594 | `		PH7_NativeMaterializeLazy(pVm,pClone);` |
|        3 | 2595 | `	}` |
|        - | 2596 | `	/* Duplicate object values. Iterate the SOURCE attributes and copy each into` |
|        - | 2597 | `	 * the clone's same-named slot (looked up by name, so order/count differences` |
|        - | 2598 | `	 * from dynamic properties don't matter). A dynamic (runtime-added) property` |
|        - | 2599 | `	 * has no declared counterpart in the clone, so synthesize it first — without` |
|        - | 2600 | `	 * this, a clone of a stdClass would silently lose all its dynamic properties. */` |
|     1071 | 2601 | `	SyHashResetLoopCursor(&pSrc->hAttr);` |
|     6171 | 2602 | `	while((pEntry = SyHashGetNextEntry(&pSrc->hAttr)) != 0 ){` |
|     5105 | 2603 | `		VmClassAttr *pSrcAttr = (VmClassAttr *)pEntry->pUserData;` |
|     5105 | 2604 | `		VmClassAttr *pDestAttr = 0;` |
|     5105 | 2605 | `		ph7_value *pvSrc,*pvDest = 0;` |
|        - | 2606 | `		/* Duplicate non-static attribute */` |
|     5105 | 2607 | `		if( pSrcAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|        6 | 2608 | `			continue;` |
|        - | 2609 | `		}` |
|        - | 2610 | `		/* By the source's own KEY: a private property of a BASE class is filed under` |
|        - | 2611 | `		 * php's mangled storage name, and matching on the attribute's plain name` |
|        - | 2612 | `		 * would copy it over the same-named slot of the object's own class. */` |
|     5101 | 2613 | `		pEntry2 = SyHashGet(&pClone->hAttr,pEntry->pKey,pEntry->nKeyLen);` |
|     5101 | 2614 | `		if( pEntry2 ){` |
|     5075 | 2615 | `			pDestAttr = (VmClassAttr *)pEntry2->pUserData;` |
|     5075 | 2616 | `			pvDest = ExtractClassAttrValue(pVm,pDestAttr);` |
|     2562 | 2617 | `		}else if( pSrcAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){` |
|        - | 2618 | `			/* Dynamic property: synthesize the matching slot on the clone. */` |
|       37 | 2619 | `			pvDest = PH7_VmCreateDynamicAttr(pVm,pClone,` |
|       24 | 2620 | `				SyStringData(&pSrcAttr->pAttr->sName),SyStringLength(&pSrcAttr->pAttr->sName),&pDestAttr);` |
|       15 | 2621 | `		}else if( pSrcAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_ONDEMAND ){` |
|        - | 2622 | `			/* An ON-DEMAND property is installed by the write that names it, so` |
|        - | 2623 | `			 * the clone's frame has no slot for one -- and php's copy carries it` |
|        - | 2624 | ``			 * (a cloned from-string DateInterval keeps its `date_string`). */`` |
|        3 | 2625 | `			VmRecreateDeclaredAttr(pVm,pClone,pSrcAttr->pAttr,&pDestAttr);` |
|        3 | 2626 | `			if( pDestAttr ){` |
|        3 | 2627 | `				pvDest = ExtractClassAttrValue(pVm,pDestAttr);` |
|        1 | 2628 | `			}` |
|        1 | 2629 | `		}` |
|        - | 2630 | `		/* Fetch the source value LAST: PH7_VmCreateDynamicAttr above may have` |
|        - | 2631 | `		 * reserved a slot, which used to reallocate pVm->aMemObj and dangle any` |
|        - | 2632 | `		 * ph7_value* obtained before it. Redundant since P1 (fixed segments);` |
|        - | 2633 | `		 * left for the harvest sweep (PERF.md P1). */` |
|     5101 | 2634 | `		pvSrc = ExtractClassAttrValue(pVm,pSrcAttr);` |
|     5101 | 2635 | `		if( (pSrcAttr->iState & VM_CLASS_ATTR_REFBOUND) && pDestAttr ){` |
|        - | 2636 | `			/* php preserves references across clone: the clone shares the SAME slot` |
|        - | 2637 | `			 * as the source property (both alias the referenced variable), rather` |
|        - | 2638 | `			 * than getting an independent value copy. Drop the clone's fresh private` |
|        - | 2639 | `			 * slot and repoint at the (already-pinned) shared slot. The REFBOUND flag` |
|        - | 2640 | `			 * is carried over by the iState copy below, so the clone's release also` |
|        - | 2641 | `			 * leaves the shared slot alone. */` |
|        5 | 2642 | `			if( pDestAttr->nIdx != pSrcAttr->nIdx ){` |
|        5 | 2643 | `				PH7_VmStoreFilterDrop(pVm,pDestAttr->pAttr,pDestAttr->nIdx);` |
|        5 | 2644 | `				PH7_VmUnsetMemObj(pVm,pDestAttr->nIdx,TRUE);` |
|        5 | 2645 | `				pDestAttr->nIdx = pSrcAttr->nIdx;` |
|        - | 2646 | `				/* The clone is a holder of the shared slot in its own right — take a pin` |
|        - | 2647 | `				 * for it, since its own release will give one back. */` |
|        5 | 2648 | `				VmPinMemObjSlotCounted(pVm,pDestAttr->nIdx);` |
|        3 | 2649 | `			}` |
|     5099 | 2650 | `		}else if( pvSrc && pvDest ){` |
|     5097 | 2651 | `			PH7_MemObjStore(pvSrc,pvDest);` |
|     2546 | 2652 | `		}` |
|        - | 2653 | `		/* Carry over the per-instance state so the clone matches the source:` |
|        - | 2654 | `		 * VM_CLASS_ATTR_UNINIT marks a typed property as not-yet-initialized` |
|        - | 2655 | `		 * and doubles as the readonly write-once latch — without this a clone` |
|        - | 2656 | `		 * would reset to uninitialized (losing the value's readiness) and a` |
|        - | 2657 | `		 * readonly property would become writable again. */` |
|     5101 | 2658 | `		if( pDestAttr ){` |
|     5101 | 2659 | `			pDestAttr->iState = pSrcAttr->iState;` |
|     2548 | 2660 | `		}` |
|        5 | 2661 | `	}` |
|        - | 2662 | `	/* A declared property unset() on the source is absent from the clone too (PHP). But the clone` |
|        - | 2663 | `	 * frame above materialized ALL declared attrs (with their defaults), so drop any clone attr whose` |
|        - | 2664 | `	 * name is not present on the source. Collect first, then delete — removing an entry mid-walk would` |
|        - | 2665 | `	 * free the node the SyHash loop cursor points at. */` |
|        - | 2666 | `	{` |
|        - | 2667 | `		SySet sDrop;` |
|     1071 | 2668 | `		SySetInit(&sDrop,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|     1071 | 2669 | `		SyHashResetLoopCursor(&pClone->hAttr);` |
|     6175 | 2670 | `		while((pEntry = SyHashGetNextEntry(&pClone->hAttr)) != 0 ){` |
|     5109 | 2671 | `			VmClassAttr *pCloneAttr = (VmClassAttr *)pEntry->pUserData;` |
|     5109 | 2672 | `			if( pCloneAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|        6 | 2673 | `				continue;` |
|        - | 2674 | `			}` |
|     5105 | 2675 | `			if( SyHashGet(&pSrc->hAttr,pEntry->pKey,pEntry->nKeyLen) == 0 ){` |
|        5 | 2676 | `				SySetPut(&sDrop,(const void *)&pEntry);` |
|        2 | 2677 | `			}` |
|        5 | 2678 | `		}` |
|     1071 | 2679 | `		if( SySetUsed(&sDrop) > 0 ){` |
|        5 | 2680 | `			SyHashEntry **apDrop = (SyHashEntry **)SySetBasePtr(&sDrop);` |
|        - | 2681 | `			sxu32 i;` |
|        9 | 2682 | `			for( i = 0 ; i < SySetUsed(&sDrop) ; ++i ){` |
|        5 | 2683 | `				VmClassAttr *pVmAttr = (VmClassAttr *)apDrop[i]->pUserData;` |
|        5 | 2684 | `				SyHashDeleteEntry(&pClone->hAttr,apDrop[i]->pKey,apDrop[i]->nKeyLen,0);` |
|        5 | 2685 | `				PH7_VmReleaseInstanceAttr(pVm,pVmAttr);` |
|        3 | 2686 | `			}` |
|        2 | 2687 | `		}` |
|     1071 | 2688 | `		SySetRelease(&sDrop);` |
|        - | 2689 | `	}` |
|        - | 2690 | `	/* A copy of a Closure names the same function, so it is a new holder of it --` |
|        - | 2691 | ``	 * `clone $f`, and Closure::bindTo()/bind(), which clone. Without this the`` |
|        - | 2692 | `	 * ORIGINAL's death would free a per-instantiation body the copy still calls.` |
|        - | 2693 | `	 * A no-op for every other class (one pointer compare). */` |
|     1071 | 2694 | `	PH7_VmClosureInstanceRef(pVm,pClone,1);` |
|        - | 2695 | `	/* The native clone hook (php's clone_obj handler): what the copy MEANS for a` |
|        - | 2696 | `	 * class whose instances stand for engine-side state -- a DOM wrapper's copy` |
|        - | 2697 | `	 * is a copy of the node. The nearest ancestor's hook serves a user subclass,` |
|        - | 2698 | `	 * which is php's handler inheritance. Runs before any __clone(), as php's` |
|        - | 2699 | `	 * handler does. */` |
|        - | 2700 | `	{` |
|        - | 2701 | `		ph7_class *pHook;` |
|     2117 | 2702 | `		for( pHook = pClone->pClass ; pHook ; pHook = pHook->pBase ){` |
|     1091 | 2703 | `			if( pHook->xClone ){` |
|       41 | 2704 | `				pHook->xClone(pVm,pClone,pSrc);` |
|       41 | 2705 | `				break;` |
|        - | 2706 | `			}` |
|      528 | 2707 | `		}` |
|        - | 2708 | `	}` |
|        - | 2709 | `	/* call the __clone method on the cloned object if available */` |
|     1071 | 2710 | `	pMethod = PH7_ClassExtractMethod(pClone->pClass,"__clone",sizeof("__clone")-1);` |
|     1071 | 2711 | `	if( pMethod ){` |
|      101 | 2712 | `		if( pMethod->iCloneDepth < 16 ){` |
|       99 | 2713 | `			pMethod->iCloneDepth++;` |
|        - | 2714 | `			/* PHP 8.3: __clone() may re-initialize the clone's readonly` |
|        - | 2715 | `			 * properties. Flag the instance so the readonly store guard allows` |
|        - | 2716 | `			 * it for the duration of the call. */` |
|       99 | 2717 | `			pClone->iFlags \|= VM_INSTANCE_CLONING;` |
|       99 | 2718 | `			PH7_VmCallClassMethod(pVm,pClone,pMethod,0,0,0);` |
|       99 | 2719 | `			pClone->iFlags &= ~VM_INSTANCE_CLONING;` |
|       51 | 2720 | `		}else{` |
|        - | 2721 | `			/* Nesting limit reached */` |
|        3 | 2722 | `			PH7_VmThrowError(pVm,0,PH7_CTX_ERR,"Object clone limit reached,no more call to __clone()");` |
|        - | 2723 | `		}` |
|        - | 2724 | `		/* Reset the cursor */` |
|      101 | 2725 | `		pMethod->iCloneDepth = 0;` |
|       49 | 2726 | `	}` |
|        - | 2727 | `	/* Return the cloned object */` |
|     1071 | 2728 | `	return pClone;` |
|      538 | 2729 | `}` |
|        - | 2730 | `/* CLASS_INSTANCE_DESTROYED moved to ph7int.h: the cycle collector has to know` |
|        - | 2731 | ` * an instance that is already mid-release when it walks one. */` |
|        - | 2732 | `/*` |
|        - | 2733 | ` * Free the per-instance allocations owned by ONE object attribute: its value slot (+ the typed-slot` |
|        - | 2734 | ` * enforcement entry), the synthesized ph7_class_attr for a dynamic (runtime-added) property, and the` |
|        - | 2735 | ` * VmClassAttr wrapper itself. Does NOT touch the hAttr entry node — the caller removes it` |
|        - | 2736 | `` * (`unset($o->p)` via SyHashDeleteEntry2; instance teardown via the wholesale SyHashRelease, so it must`` |
|        - | 2737 | ` * not delete entries mid-walk). Shared by PH7_ClassInstanceRelease and the OP_MEMBER unset path.` |
|        - | 2738 | ` */` |
|  9731737 | 2739 | `PH7_PRIVATE void PH7_VmReleaseInstanceAttr(ph7_vm *pVm, VmClassAttr *pVmAttr)` |
|        5 | 2740 | `{` |
|  9731742 | 2741 | `	if( pVmAttr->iState & (VM_CLASS_ATTR_REFBOUND\|VM_CLASS_ATTR_REFSRCPIN) ){` |
|        - | 2742 | `` 		/* A property at either end of a reference (`$o->p =& $x` bound it, `$r =& $o->p` `` |
|        - | 2743 | `		 * made it a source) holds its value slot with a COUNTED PIN and shares it, so it` |
|        - | 2744 | `		 * must not be released here — but the property WAS one of its holders, so give the` |
|        - | 2745 | `		 * pin back. The slot (and its value, which used to stay alive for the rest of the` |
|        - | 2746 | `		 * script) goes if the property was the last thing holding it. A SOURCE still owns` |
|        - | 2747 | `		 * its declaration, so its typed-slot enforcement entry goes with it. */` |
|      180 | 2748 | `		if( (pVmAttr->iState & VM_CLASS_ATTR_REFBOUND) == 0 ){` |
|      130 | 2749 | `			PH7_VmStoreFilterDrop(pVm,pVmAttr->pAttr,pVmAttr->nIdx);` |
|       64 | 2750 | `		}` |
|      180 | 2751 | `		VmUnpinMemObjSlot(pVm,pVmAttr->nIdx);` |
|  9731653 | 2752 | `	}else if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - | 2753 | `		/* Drop any typed-property enforcement slot registered for this memobj, before the memobj` |
|        - | 2754 | `		 * is returned to the free list, so a future recycled slot does not inherit the stale entry. */` |
|  9730254 | 2755 | `		PH7_VmStoreFilterDrop(pVm,pVmAttr->pAttr,pVmAttr->nIdx);` |
|  9730254 | 2756 | `		if( !PH7_VmSlotDropOwnerHold(pVm,pVmAttr->nIdx) ){` |
|        - | 2757 | ``			/* Nobody else names the slot. When somebody does -- `$r =& $o->p`,`` |
|        - | 2758 | ``			 * `$a[] =& $o->p` -- the value is theirs to keep and theirs to release,`` |
|        - | 2759 | `			 * exactly as php's refcount makes it: unlinking it here took the array` |
|        - | 2760 | `			 * element with it and left the variable UNDEFINED. */` |
|  9730248 | 2761 | `			PH7_VmUnsetMemObj(pVm,pVmAttr->nIdx,TRUE);` |
|  4864524 | 2762 | `		}` |
|  4864527 | 2763 | `	}` |
|        - | 2764 | `	/* A dynamic property owns its synthesized ph7_class_attr (struct + inline name in one block) —` |
|        - | 2765 | `	 * free it here (the only place a per-instance pAttr is freed; declared attrs are class-owned). */` |
|  9731742 | 2766 | `	if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){` |
|      623 | 2767 | `		SyMemBackendFree(&pVm->sAllocator,pVmAttr->pAttr);` |
|      309 | 2768 | `	}` |
|  9731742 | 2769 | `	SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|  9731742 | 2770 | `}` |
|        - | 2771 | `/*` |
|        - | 2772 | ` * Run this instance's __destruct exactly once, or raise the refusal that stands in for it.` |
|        - | 2773 | ` *` |
|        - | 2774 | ` * Called from two places: PH7_ClassInstanceRelease, where the object dies because nothing` |
|        - | 2775 | ` * refers to it any more, and the shutdown pass (VmCallShutdownDestructors), which reaches` |
|        - | 2776 | ` * every object a program left alive WITHOUT freeing it -- php's zend_objects_store_call_destructors` |
|        - | 2777 | ` * does exactly that, and the free that follows must not run the body a second time, which` |
|        - | 2778 | ` * is what CLASS_INSTANCE_DTOR_CALLED records.` |
|        - | 2779 | ` *` |
|        - | 2780 | ` * PH7_ClassInstanceCtorFailed below sets that same bit for php's other reason.` |
|        - | 2781 | ` */` |
|  1525088 | 2782 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallDestructor(ph7_class_instance *pThis)` |
|        5 | 2783 | `{` |
|        - | 2784 | `	ph7_class_method *pDestr;` |
|        - | 2785 | `	ph7_class *pClass;` |
|        - | 2786 | `	ph7_vm *pVm;` |
|  1525093 | 2787 | `	sxi32 rc = SXRET_OK;` |
|  1525093 | 2788 | `	if( pThis->iFlags & CLASS_INSTANCE_DTOR_CALLED ){` |
|   104802 | 2789 | `		return SXRET_OK;` |
|        - | 2790 | `	}` |
|        - | 2791 | `	/* Flagged whether or not there is a body to run, exactly as php flags its own` |
|        - | 2792 | `	 * (IS_OBJ_DESTRUCTOR_CALLED is set before the handler is even looked up). The` |
|        - | 2793 | `	 * shutdown pass sweeps the object pool until a round finds nothing unflagged, so` |
|        - | 2794 | `	 * an object with no __destruct at all has to come back flagged too. */` |
|  1420296 | 2795 | `	pThis->iFlags \|= CLASS_INSTANCE_DTOR_CALLED;` |
|  1420296 | 2796 | `	pVm = pThis->pVm;` |
|  1420296 | 2797 | `	pClass = pThis->pClass;` |
|  1420296 | 2798 | `	pDestr = PH7_ClassExtractMethod(pClass,"__destruct",sizeof("__destruct")-1);` |
|  1420296 | 2799 | `	if( pDestr && !pVm->bInReset ){` |
|        - | 2800 | `		/* php checks a non-public destructor's visibility HERE, against the scope` |
|        - | 2801 | `		 * the destruction happened in, and refuses with a sentence of its own: the` |
|        - | 2802 | `		 * engine reached for the method, so the message names the OBJECT's class` |
|        - | 2803 | `		 * and drops the word "method" the ordinary call refusal carries` |
|        - | 2804 | ``		 * (`Call to private B::__destruct() from global scope` for a `class B`` |
|        - | 2805 | ``		 * extends A` whose base declared it). Screening here rather than letting`` |
|        - | 2806 | `		 * the dispatcher speak is what keeps that wording; the call is then made` |
|        - | 2807 | `		 * unchecked, since this IS the check. */` |
|     1745 | 2808 | `		ph7_class *pDestrDecl = pDestr->sFunc.pUserData` |
|     1159 | 2809 | `			? (ph7_class *)pDestr->sFunc.pUserData : pClass;` |
|     1159 | 2810 | `		if( pDestr->iProtection != PH7_CLASS_PROT_PUBLIC` |
|      587 | 2811 | `		 && !PH7_VmClassMemberAccess(&(*pVm),pDestrDecl,&pDestr->sFunc.sName,` |
|        5 | 2812 | `			pDestr->iProtection,FALSE) ){` |
|        - | 2813 | `			SyBlob sErrMsg;` |
|       14 | 2814 | `			const char *zVis = pDestr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|        4 | 2815 | `				? "private" : "protected";` |
|       10 | 2816 | `			ph7_class *pScope = PH7_VmCallerScope(&(*pVm));` |
|       10 | 2817 | `			SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       10 | 2818 | `			if( pVm->bInShutdownDtor ){` |
|        - | 2819 | `				/* Reached from the shutdown pass, with no PHP frame under it. php tests` |
|        - | 2820 | ``				 * exactly that (`EG(current_execute_data) == NULL`) and answers a`` |
|        - | 2821 | `				 * different sentence at a different severity: an E_WARNING saying the` |
|        - | 2822 | `				 * call was ignored, after which the object is simply not destructed and` |
|        - | 2823 | `				 * the program is already over. The Error below is for a refusal a` |
|        - | 2824 | `				 * running program can still catch. */` |
|        8 | 2825 | `				SyBlobFormat(&sErrMsg,` |
|        - | 2826 | `					"Call to %s %z::__destruct() from global scope during shutdown ignored",` |
|        3 | 2827 | `					zVis,&pClass->sName);` |
|        8 | 2828 | `				SyBlobAppend(&sErrMsg,"\0",sizeof(char));` |
|        - | 2829 | `				/* Raised between two destructor bodies, so there is no frame to name:` |
|        - | 2830 | ``				 * php reports it `in Unknown on line 0`. */`` |
|        8 | 2831 | `				pVm->bNoFrameLoc = 1;` |
|        8 | 2832 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,(const char *)SyBlobData(&sErrMsg));` |
|        8 | 2833 | `				pVm->bNoFrameLoc = 0;` |
|        8 | 2834 | `				SyBlobRelease(&sErrMsg);` |
|        8 | 2835 | `				return SXRET_OK;` |
|        - | 2836 | `			}` |
|        3 | 2837 | `			if( pScope ){` |
|      ! 0 | 2838 | `				SyBlobFormat(&sErrMsg,"Call to %s %z::__destruct() from scope %z",` |
|      ! 0 | 2839 | `					zVis,&pClass->sName,&pScope->sName);` |
|      ! 0 | 2840 | `			}else{` |
|        3 | 2841 | `				SyBlobFormat(&sErrMsg,"Call to %s %z::__destruct() from global scope",` |
|        1 | 2842 | `					zVis,&pClass->sName);` |
|        - | 2843 | `			}` |
|        - | 2844 | `			/* Parked, not returned: this release has no channel back to the` |
|        - | 2845 | `			 * executor (nothing "called" the destruct), and the dispatcher's own` |
|        - | 2846 | `			 * screen used to do the parking for us through` |
|        - | 2847 | `			 * VmCallClassMethodWithMap. Without it the uncaught Error is printed` |
|        - | 2848 | `			 * and the program carries on past a statement php never reaches. */` |
|        4 | 2849 | `			VmBoundaryPark(&(*pVm),` |
|        1 | 2850 | `				VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|        3 | 2851 | `			SyBlobRelease(&sErrMsg);` |
|        2 | 2852 | `		}else{` |
|        - | 2853 | `			/* Invoke the destructor. Skipped during ph7_vm_reset() bulk teardown:` |
|        - | 2854 | `			 * running user PHP against a half-reset VM is unsafe (see bInReset).` |
|        - | 2855 | `			 *` |
|        - | 2856 | `			 * Pinned across the body rather than SET to a constant: reached from a` |
|        - | 2857 | `			 * release the count is 0 and any value keeps the nested unref off it, but` |
|        - | 2858 | `			 * the shutdown pass calls this on an object other names still hold, and` |
|        - | 2859 | `			 * flattening their count there would free it under them. php pins the same` |
|        - | 2860 | `			 * way (GC_ADDREF/GC_DELREF around dtor_obj), so a body that stores $this` |
|        - | 2861 | `			 * somewhere keeps the reference it gained. */` |
|     1156 | 2862 | `			sxu8 bPhase = pVm->bInShutdownDtor;` |
|        - | 2863 | `			VmResumeTarget sSaveResume;` |
|        - | 2864 | `			/* The body has a frame of its own, so php's "no execute_data" state ends` |
|        - | 2865 | `			 * here and resumes when it returns: an object the body itself drops --` |
|        - | 2866 | ``			 * `$this->p = null` on the last holder of a private-destructor object --`` |
|        - | 2867 | `			 * is refused with the catchable Error naming the running scope, not with` |
|        - | 2868 | `			 * the shutdown warning above. */` |
|     1156 | 2869 | `			pVm->bInShutdownDtor = 0;` |
|     1156 | 2870 | `			pThis->iRef += 2; /* Prevent garbage collection */` |
|        - | 2871 | `			/* A destructor runs in the MIDDLE of somebody else's control flow: the` |
|        - | 2872 | `			 * release that reaches it is usually a frame teardown on an unwind that` |
|        - | 2873 | `			 * is already carrying a throw. php hides the in-flight exception for the` |
|        - | 2874 | `			 * duration (zend_objects_store_del saves EG(exception), clears it, and` |
|        - | 2875 | `			 * puts it back afterwards) precisely so the body cannot observe or` |
|        - | 2876 | `			 * disturb it. PHL's in-place-catch resume record is that same in-flight` |
|        - | 2877 | `			 * state — it says "the throw now unwinding was already caught at frame F,` |
|        - | 2878 | `			 * pad P" — and a destructor body with a try/catch of its own writes a` |
|        - | 2879 | `			 * record when ITS catch finishes, overwriting the one the outer throw is` |
|        - | 2880 | ``			 * still owed. monolog's `Handler::__destruct` is exactly that shape`` |
|        - | 2881 | ``			 * (`try { $this->close(); } catch (Throwable) {}`), and the outer throw`` |
|        - | 2882 | `			 * then never landed: the script that was catching it simply ENDED.` |
|        - | 2883 | `			 * Save, clear, restore — and, like php, let a record the body LEAVES` |
|        - | 2884 | `			 * behind (a throw of its own still in flight) supersede the saved one. */` |
|     1156 | 2885 | `			VmSaveResumeTarget(pVm,&sSaveResume);` |
|     1156 | 2886 | `			VmClearResumeTarget(pVm);` |
|     1156 | 2887 | `			rc = PH7_VmCallMethodUnchecked(pVm,pThis,pDestr,0,0,0);` |
|     1156 | 2888 | `			if( pVm->pResumeFrame == 0 ){` |
|     1154 | 2889 | `				VmRestoreResumeTarget(pVm,&sSaveResume);` |
|      573 | 2890 | `			}` |
|     1156 | 2891 | `			pThis->iRef -= 2;` |
|     1156 | 2892 | `			pVm->bInShutdownDtor = bPhase;` |
|        - | 2893 | `		}` |
|      575 | 2894 | `	}` |
|        - | 2895 | `	/* SXERR_ABORT here means the body left an UNCAUGHT throwable (or exited). php runs` |
|        - | 2896 | `	 * its whole destructor phase under one zend_try, so the first one abandons every` |
|        - | 2897 | `	 * destructor still owed -- including the ones the symbol-table half would have` |
|        - | 2898 | `	 * reached, which is why the decision is recorded on the VM and not just returned. */` |
|  1420290 | 2899 | `	if( rc == SXERR_ABORT && pVm->bInShutdownDtor ){` |
|        6 | 2900 | `		pVm->bShutdownAborted = 1;` |
|        2 | 2901 | `	}` |
|  1420290 | 2902 | `	return rc;` |
|   762383 | 2903 | `}` |
|        - | 2904 | `/*` |
|        - | 2905 | ` * A constructor CALL raised, so this object never became one: php marks it` |
|        - | 2906 | ` * (zend_object_store_ctor_failed sets the very bit that records "the destructor` |
|        - | 2907 | ` * has been reached for") and its __destruct is therefore never run -- not when the` |
|        - | 2908 | `` * half-built object is dropped at the `new`, and not later either, because the mark`` |
|        - | 2909 | ` * lives on the object and follows it wherever the constructor happened to store` |
|        - | 2910 | `` * `$this` before it threw. PHL ran the destructor on both, so monolog's`` |
|        - | 2911 | `` * `Handler::__destruct` -- a `try { $this->close(); } catch {}` -- executed against`` |
|        - | 2912 | ` * an instance whose typed properties were still uninitialised, in the middle of the` |
|        - | 2913 | ` * unwind that was already carrying the constructor's own exception.` |
|        - | 2914 | ` *` |
|        - | 2915 | `` * Called at every door that CALLS a constructor: `new` itself, and Reflection's`` |
|        - | 2916 | ` * newInstance family (php flags at each of those and nowhere else).` |
|        - | 2917 | ` */` |
|   100897 | 2918 | `PH7_PRIVATE void PH7_ClassInstanceCtorFailed(ph7_class_instance *pThis)` |
|        5 | 2919 | `{` |
|   100902 | 2920 | `	if( pThis ){` |
|   100902 | 2921 | `		pThis->iFlags \|= CLASS_INSTANCE_DTOR_CALLED;` |
|    50448 | 2922 | `	}` |
|   100902 | 2923 | `}` |
|        - | 2924 | `/*` |
|        - | 2925 | ` * Release a class instance [i.e: Object in the PHP jargon] and invoke any defined destructor.` |
|        - | 2926 | ` * This routine is invoked as soon as there are no other references to a particular` |
|        - | 2927 | ` * class instance.` |
|        - | 2928 | ` */` |
|  1519236 | 2929 | `static void PH7_ClassInstanceRelease(ph7_class_instance *pThis)` |
|        5 | 2930 | `{` |
|        - | 2931 | `	SyHashEntry *pEntry;` |
|        - | 2932 | `	ph7_class *pClass;` |
|        - | 2933 | `	ph7_vm *pVm;` |
|  1519241 | 2934 | `	if( pThis->iFlags & CLASS_INSTANCE_DESTROYED ){` |
|        - | 2935 | `		/*` |
|        - | 2936 | `		 * Already destroyed,return immediately.` |
|        - | 2937 | `		 * This could happend if someone perform unset($this) in the destructor body.` |
|        - | 2938 | `		 */` |
|      ! 0 | 2939 | `		return;` |
|        - | 2940 | `	}` |
|        - | 2941 | `	/* Mark as destroyed */` |
|  1519241 | 2942 | `	pThis->iFlags \|= CLASS_INSTANCE_DESTROYED;` |
|  1519241 | 2943 | `	pVm = pThis->pVm;` |
|  1519241 | 2944 | `	pClass = pThis->pClass;` |
|        - | 2945 | `	/* Invoke any defined destructor if available (a no-op once the shutdown pass` |
|        - | 2946 | `	 * has already run it) */` |
|  1519241 | 2947 | `	PH7_ClassInstanceCallDestructor(pThis);` |
|        - | 2948 | ``	/* php: a destructor may RESURRECT the object. Anything the body hands `$this` to`` |
|        - | 2949 | `	 * that outlives the release -- a registry, a property of something still alive, a` |
|        - | 2950 | ``	 * closure's `use ($this)` -- is a new reference taken while the refcount was`` |
|        - | 2951 | `	 * already at zero, and zend_objects_store_del re-reads it after dtor_obj and frees` |
|        - | 2952 | `	 * ONLY when it is still zero. PHL freed unconditionally, so every holder the` |
|        - | 2953 | `	 * destructor had just handed the object to was left pointing at freed memory.` |
|        - | 2954 | `	 *` |
|        - | 2955 | `	 * Pest is exactly that shape: a TestCase's teardown registers closures that capture` |
|        - | 2956 | ``	 * `$this`, and the next `new` of a test case read the dead object through one of`` |
|        - | 2957 | `	 * them -- a segfault a fifth of the way into any suite it runs, with the refcount` |
|        - | 2958 | `	 * still reading 7.` |
|        - | 2959 | `	 *` |
|        - | 2960 | `	 * Clearing DESTROYED is what lets the object die properly LATER: when its new` |
|        - | 2961 | `	 * holders drop it to zero this runs again, and CLASS_INSTANCE_DTOR_CALLED (set by` |
|        - | 2962 | `	 * PH7_ClassInstanceCallDestructor, php's IS_OBJ_DESTRUCTOR_CALLED) keeps the` |
|        - | 2963 | `	 * destructor from running a second time -- php's rule for the same case. */` |
|  1519241 | 2964 | `	if( pThis->iRef > 0 ){` |
|      ! 0 | 2965 | `		pThis->iFlags &= ~CLASS_INSTANCE_DESTROYED;` |
|      ! 0 | 2966 | `		return;` |
|        - | 2967 | `	}` |
|        - | 2968 | `	/* A native class's own teardown, while its slots are still readable. Not a` |
|        - | 2969 | `	 * __destruct: the classes that need this (WeakReference) declare none in php,` |
|        - | 2970 | `	 * and Reflection must not grow one.` |
|        - | 2971 | `	 *` |
|        - | 2972 | `	 * Resolved through the ANCESTORS, like php's own free_obj handler: a` |
|        - | 2973 | `	 * subclass inherits it unless it declares one of its own. Reading it off` |
|        - | 2974 | `	 * this class alone left every subclass of a handle-owning native class` |
|        - | 2975 | ``	 * without teardown -- `Pdo\Sqlite` (which is how PDO::connect() answers) and`` |
|        - | 2976 | ``	 * any userland `extends PDO` alike, both of which then died holding engine`` |
|        - | 2977 | `	 * state that believed it was still reachable. */` |
|        - | 2978 | `	{` |
|  1519241 | 2979 | `		ph7_class *pOwner = pClass;` |
|  3145789 | 2980 | `		while( pOwner && pOwner->xRelease == 0 ){` |
|  1626553 | 2981 | `			pOwner = pOwner->pBase;` |
|        5 | 2982 | `		}` |
|  1519241 | 2983 | `		if( pOwner && pOwner->xRelease ){` |
|    22941 | 2984 | `			pOwner->xRelease(pVm,pThis);` |
|    11346 | 2985 | `		}` |
|        - | 2986 | `	}` |
|        - | 2987 | `	/* Weak-reference registry: kill the cell for this instance so every` |
|        - | 2988 | `	 * WeakReference/WeakMap handle observes the death (the cell outlives the` |
|        - | 2989 | `	 * instance until its own handles drop; removing the hash entry here keeps` |
|        - | 2990 | `	 * a pool-reused address from resurrecting a dead cell). */` |
|  1519241 | 2991 | `	if( SyHashTotalEntry(&pVm->hWeakCell) > 0 ){` |
|    18054 | 2992 | `		void *pCellData = 0;` |
|    18052 | 2993 | `		if( SyHashDeleteEntry(&pVm->hWeakCell,(const void *)&pThis,sizeof(void *),&pCellData) == SXRET_OK` |
|     9043 | 2994 | `		 && pCellData ){` |
|       32 | 2995 | `			((VmWeakCell *)pCellData)->pObj = 0;` |
|       15 | 2996 | `		}` |
|     9026 | 2997 | `	}` |
|        - | 2998 | `	/* Release non-static attributes (the wholesale SyHashRelease below frees the entry nodes,` |
|        - | 2999 | `	 * so the helper must not delete them mid-walk). */` |
|  1519241 | 3000 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
| 11250904 | 3001 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|  9731668 | 3002 | `		PH7_VmReleaseInstanceAttr(pVm,(VmClassAttr *)pEntry->pUserData);` |
|        5 | 3003 | `	}` |
|        - | 3004 | `	/* Release the whole structure */` |
|  1519241 | 3005 | `	SyHashRelease(&pThis->hAttr);` |
|        - | 3006 | `	/* ...and stop the collector's root buffer naming memory that is going back` |
|        - | 3007 | `	 * to the pool. */` |
|  1519241 | 3008 | `	PH7_GcForget(pVm,(void *)pThis,0);` |
|  1519241 | 3009 | `	SyMemBackendPoolFree(&pVm->sAllocator,pThis);` |
|   759460 | 3010 | `}` |
|        - | 3011 | `/*` |
|        - | 3012 | ` * Decrement the reference count of a class instance [i.e Object in the PHP jargon].` |
|        - | 3013 | ` * If the reference count reaches zero,release the whole instance.` |
|        - | 3014 | ` */` |
|  7672445 | 3015 | `PH7_PRIVATE void PH7_ClassInstanceUnref(ph7_class_instance *pThis)` |
|        5 | 3016 | `{` |
|  7672450 | 3017 | `	pThis->iRef--;` |
|  7672450 | 3018 | `	if( pThis->iRef < 1 ){` |
|        - | 3019 | `		/* No more reference to this instance */` |
|  1519241 | 3020 | `		PH7_ClassInstanceRelease(&(*pThis));` |
|   759460 | 3021 | `	}else{` |
|        - | 3022 | `		/* Still held -- but by whom? A drop that does NOT reach zero is the only` |
|        - | 3023 | `		 * event that can strand a cycle, so it is what the collector buffers. */` |
|  6153214 | 3024 | `		PH7_GcPossibleRoot(pThis->pVm,(void *)pThis,0);` |
|        - | 3025 | `	}` |
|  7672450 | 3026 | `}` |
|        - | 3027 | `/*` |
|        - | 3028 | ` * Compare two class instances [i.e: Objects in the PHP jargon]` |
|        - | 3029 | ` * Note on objects comparison:` |
|        - | 3030 | ` *  According to the PHP langauge reference manual` |
|        - | 3031 | ` *  When using the comparison operator (==), object variables are compared in a simple manner` |
|        - | 3032 | ` *  namely: Two object instances are equal if they have the same attributes and values, and are` |
|        - | 3033 | ` *  instances of the same class.` |
|        - | 3034 | ` *  On the other hand, when using the identity operator (===), object variables are identical` |
|        - | 3035 | ` *  if and only if they refer to the same instance of the same class.` |
|        - | 3036 | ` *  An example will clarify these rules.` |
|        - | 3037 | ` *  Example #1 Example of object comparison` |
|        - | 3038 | ` *  <?php` |
|        - | 3039 | ` *    function bool2str($bool)` |
|        - | 3040 | ` * {` |
|        - | 3041 | ` *   if ($bool === false) {` |
|        - | 3042 | ` *       return 'FALSE';` |
|        - | 3043 | ` *   } else {` |
|        - | 3044 | ` *       return 'TRUE';` |
|        - | 3045 | ` *   }` |
|        - | 3046 | ` * }` |
|        - | 3047 | ` * function compareObjects(&$o1, &$o2)` |
|        - | 3048 | ` * {` |
|        - | 3049 | ` *   echo 'o1 == o2 : ' . bool2str($o1 == $o2) . "\n";` |
|        - | 3050 | ` *   echo 'o1 != o2 : ' . bool2str($o1 != $o2) . "\n";` |
|        - | 3051 | ` *   echo 'o1 === o2 : ' . bool2str($o1 === $o2) . "\n";` |
|        - | 3052 | ` *   echo 'o1 !== o2 : ' . bool2str($o1 !== $o2) . "\n";` |
|        - | 3053 | ` * }` |
|        - | 3054 | ` * class Flag` |
|        - | 3055 | ` * {` |
|        - | 3056 | ` *   public $flag;` |
|        - | 3057 | ` *` |
|        - | 3058 | ` *   function Flag($flag = true) {` |
|        - | 3059 | ` *       $this->flag = $flag;` |
|        - | 3060 | ` *   }` |
|        - | 3061 | ` * }` |
|        - | 3062 | ` *` |
|        - | 3063 | ` * class OtherFlag` |
|        - | 3064 | ` * {` |
|        - | 3065 | ` *   public $flag;` |
|        - | 3066 | ` *` |
|        - | 3067 | ` *   function OtherFlag($flag = true) {` |
|        - | 3068 | ` *       $this->flag = $flag;` |
|        - | 3069 | ` *   }` |
|        - | 3070 | ` * }` |
|        - | 3071 | ` *` |
|        - | 3072 | ` * $o = new Flag();` |
|        - | 3073 | ` * $p = new Flag();` |
|        - | 3074 | ` * $q = $o;` |
|        - | 3075 | ` * $r = new OtherFlag();` |
|        - | 3076 | ` *` |
|        - | 3077 | ` * echo "Two instances of the same class\n";` |
|        - | 3078 | ` * compareObjects($o, $p);` |
|        - | 3079 | ` * echo "\nTwo references to the same instance\n";` |
|        - | 3080 | ` * compareObjects($o, $q);` |
|        - | 3081 | ` * echo "\nInstances of two different classes\n";` |
|        - | 3082 | ` * compareObjects($o, $r);` |
|        - | 3083 | ` * ?>` |
|        - | 3084 | ` * The above example will output:` |
|        - | 3085 | ` * Two instances of the same class` |
|        - | 3086 | ` * o1 == o2 : TRUE` |
|        - | 3087 | ` * o1 != o2 : FALSE` |
|        - | 3088 | ` * o1 === o2 : FALSE` |
|        - | 3089 | ` * o1 !== o2 : TRUE` |
|        - | 3090 | ` * Two references to the same instance` |
|        - | 3091 | ` * o1 == o2 : TRUE` |
|        - | 3092 | ` * o1 != o2 : FALSE` |
|        - | 3093 | ` * o1 === o2 : TRUE` |
|        - | 3094 | ` * o1 !== o2 : FALSE` |
|        - | 3095 | ` * Instances of two different classes` |
|        - | 3096 | ` * o1 == o2 : FALSE` |
|        - | 3097 | ` * o1 != o2 : TRUE` |
|        - | 3098 | ` * o1 === o2 : FALSE` |
|        - | 3099 | ` * o1 !== o2 : TRUE` |
|        - | 3100 | ` *` |
|        - | 3101 | ` * This function return 0 if the objects are equals according to the comprison rules defined above.` |
|        - | 3102 | ` * Any other return values indicates difference.` |
|        - | 3103 | ` */` |
|      956 | 3104 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest)` |
|        5 | 3105 | `{` |
|        - | 3106 | `	SyHashEntry *pEntry,*pEntry2;` |
|        - | 3107 | `	ph7_value sV1,sV2;` |
|        - | 3108 | `	sxi32 rc;` |
|      961 | 3109 | `	if( iNest > 31 ){` |
|        - | 3110 | `		/* Nesting limit reached */` |
|        6 | 3111 | `		PH7_VmThrowError(pLeft->pVm,0,PH7_CTX_ERR,"Nesting limit reached: Infinite recursion?");` |
|        6 | 3112 | `		return 1;` |
|        - | 3113 | `	}` |
|        - | 3114 | `	/*` |
|        - | 3115 | `	 * php's identity shortcut, and it comes FIRST -- before the same-class screen` |
|        - | 3116 | ``	 * and before any handler: `$i == $i` is 0 for a DateInterval, the one pair of`` |
|        - | 3117 | `	 * intervals php will compare at all.` |
|        - | 3118 | `	 */` |
|      957 | 3119 | `	if( pLeft == pRight ){` |
|        - | 3120 | `		/* Same instance,don't bother processing,object are equals */` |
|      445 | 3121 | `		return 0;` |
|        - | 3122 | `	}` |
|      517 | 3123 | `	if( bStrict ){` |
|        - | 3124 | `		/*` |
|        - | 3125 | `		 * According to the PHP language reference manual:` |
|        - | 3126 | `		 *  when using the identity operator (===), object variables` |
|        - | 3127 | `		 *  are identical if and only if they refer to the same instance` |
|        - | 3128 | `		 *  of the same class.` |
|        - | 3129 | `		 * Two DISTINCT instances, so this is never identical -- and no compare` |
|        - | 3130 | ``		 * handler is asked, because php's `===` is pointer identity and never`` |
|        - | 3131 | `		 * reaches one.` |
|        - | 3132 | `		 */` |
|      134 | 3133 | `		return 1;` |
|        - | 3134 | `	}` |
|        - | 3135 | `	/*` |
|        - | 3136 | `	 * php's compare handler (ph7_class::xCmp), asked of the LEFT operand and` |
|        - | 3137 | `	 * ABOVE the same-class screen: a DateTime and a DateTimeImmutable of the same` |
|        - | 3138 | `	 * instant are equal there, which no property walk between two different` |
|        - | 3139 | `	 * classes could ever answer. A class with no handler falls through to the` |
|        - | 3140 | `	 * walk, which is php's zend_std_compare_objects.` |
|        - | 3141 | `	 */` |
|      387 | 3142 | `	if( PH7_ClassNativeCmp(pLeft,pRight,&rc) ){` |
|      180 | 3143 | `		return rc;` |
|        - | 3144 | `	}` |
|        - | 3145 | `	/* Comparison is performed only if the objects are instance of the same class */` |
|      211 | 3146 | `	if( pLeft->pClass != pRight->pClass ){` |
|       14 | 3147 | `		return 1;` |
|        - | 3148 | `	}` |
|        - | 3149 | `	/*` |
|        - | 3150 | `	 * Attribute comparison.` |
|        - | 3151 | `	 * According to the PHP reference manual:` |
|        - | 3152 | `	 *  When using the comparison operator (==), object variables are compared` |
|        - | 3153 | `	 *  in a simple manner, namely: Two object instances are equal if they have` |
|        - | 3154 | `	 *  the same attributes and values, and are instances of the same class.` |
|        - | 3155 | `	 */` |
|        - | 3156 | `	/* Closures compare by IDENTITY under == as well (not by attributes): two distinct` |
|        - | 3157 | `	 * Closure instances are never equal, even when they wrap the same underlying function` |
|        - | 3158 | `	 * (PHP semantics). pLeft != pRight here, so a Closure pair is unequal. Without this,` |
|        - | 3159 | `` 	 * two capture-less lambdas of the same `function(){}` share the template's `$__fn` `` |
|        - | 3160 | `	 * name and would compare equal. */` |
|      199 | 3161 | `	if( pLeft->pVm->pClosureClass && pLeft->pClass == pLeft->pVm->pClosureClass ){` |
|        5 | 3162 | `		return 1;` |
|        - | 3163 | `	}` |
|        - | 3164 | `	/* Same class but a different number of attributes ⇒ different property sets` |
|        - | 3165 | `	 * (dynamic properties can give two same-class instances different counts). */` |
|      195 | 3166 | `	if( pLeft->hAttr.nEntry != pRight->hAttr.nEntry ){` |
|        3 | 3167 | `		return 1;` |
|        - | 3168 | `	}` |
|      193 | 3169 | `	PH7_MemObjInit(pLeft->pVm,&sV1);` |
|      193 | 3170 | `	PH7_MemObjInit(pLeft->pVm,&sV2);` |
|      193 | 3171 | `	sV1.nIdx = sV2.nIdx = SXU32_HIGH;` |
|        - | 3172 | `	/* Compare each left attribute against the RIGHT attribute of the SAME NAME` |
|        - | 3173 | `	 * (not in lockstep): dynamic properties may be stored in a different order` |
|        - | 3174 | `	 * on the two instances. Counts already match, so if every left attribute has` |
|        - | 3175 | `	 * an equal-valued same-named right attribute the property sets are equal. */` |
|      193 | 3176 | `	SyHashResetLoopCursor(&pLeft->hAttr);` |
|      341 | 3177 | `	while((pEntry = SyHashGetNextEntry(&pLeft->hAttr)) != 0 ){` |
|      286 | 3178 | `		VmClassAttr *p1 = (VmClassAttr *)pEntry->pUserData;` |
|        - | 3179 | `		VmClassAttr *p2;` |
|        - | 3180 | `		ph7_value *pL,*pR;` |
|        - | 3181 | `		/* Compare only non-static attribute. A native class's VIRTUAL property is` |
|        - | 3182 | `		 * skipped too: php fabricates DatePeriod's seven on demand and its real` |
|        - | 3183 | `		 * property table is empty, so any two DatePeriods are equal there whatever` |
|        - | 3184 | `		 * they contain -- while a subclass's own property, which IS in the table,` |
|        - | 3185 | `		 * still decides. */` |
|      286 | 3186 | `		if( p1->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC` |
|        - | 3187 | `		                        \|PH7_CLASS_ATTR_NATIVE_VIRTUAL) ){` |
|       71 | 3188 | `			continue;` |
|        - | 3189 | `		}` |
|      216 | 3190 | `		pEntry2 = SyHashGet(&pRight->hAttr,SyStringData(&p1->pAttr->sName),SyStringLength(&p1->pAttr->sName));` |
|      216 | 3191 | `		if( pEntry2 == 0 ){` |
|        - | 3192 | `			/* Left has a property the right lacks ⇒ not equal. */` |
|      ! 0 | 3193 | `			return 1;` |
|        - | 3194 | `		}` |
|      216 | 3195 | `		p2 = (VmClassAttr *)pEntry2->pUserData;` |
|      216 | 3196 | `		pL = ExtractClassAttrValue(pLeft->pVm,p1);` |
|      216 | 3197 | `		pR = ExtractClassAttrValue(pRight->pVm,p2);` |
|      216 | 3198 | `		if( pL && pR ){` |
|      216 | 3199 | `			PH7_MemObjLoad(pL,&sV1);` |
|      216 | 3200 | `			PH7_MemObjLoad(pR,&sV2);` |
|        - | 3201 | `			/* Compare the two values now */` |
|      216 | 3202 | `			rc = PH7_MemObjCmp(&sV1,&sV2,bStrict,iNest+1);` |
|      216 | 3203 | `			PH7_MemObjRelease(&sV1);` |
|      216 | 3204 | `			PH7_MemObjRelease(&sV2);` |
|      216 | 3205 | `			if( rc != 0 ){` |
|        - | 3206 | `				/* Not equals */` |
|      137 | 3207 | `				return rc;` |
|        - | 3208 | `			}` |
|       39 | 3209 | `		}` |
|        2 | 3210 | `	}` |
|        - | 3211 | `	/* Object are equals */` |
|       57 | 3212 | `	return 0;` |
|      483 | 3213 | `}` |
|        - | 3214 | `/*` |
|        - | 3215 | ` * Dump a class instance and the store the dump in the BLOB given` |
|        - | 3216 | ` * as the first argument.` |
|        - | 3217 | ` * Note that only non-static/non-constants attribute are dumped.` |
|        - | 3218 | ` * This function is typically invoked when the user issue a call` |
|        - | 3219 | ` * to [var_dump(),var_export(),print_r(),...].` |
|        - | 3220 | ` * This function SXRET_OK on success. Any other return value including` |
|        - | 3221 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|        - | 3222 | ` */` |
|        - | 3223 | `/*` |
|        - | 3224 | `` * Return the `name` property value of an enum case instance (the case name),`` |
|        - | 3225 | ` * or 0 when unavailable. Shared by the var_dump/var_export/json/serialize` |
|        - | 3226 | ` * renderers, which all print enum cases as Class::CaseName forms.` |
|        - | 3227 | ` */` |
|       32 | 3228 | `PH7_PRIVATE ph7_value * PH7_EnumCaseNameValue(ph7_class_instance *pThis)` |
|        1 | 3229 | `{` |
|        - | 3230 | `	SyHashEntry *pEntry;` |
|       33 | 3231 | `	if( (pThis->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|      ! 0 | 3232 | `		return 0;` |
|        - | 3233 | `	}` |
|       33 | 3234 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)"name",sizeof("name")-1);` |
|       33 | 3235 | `	if( pEntry == 0 ){` |
|      ! 0 | 3236 | `		return 0;` |
|        - | 3237 | `	}` |
|       33 | 3238 | `	return PH7_ClassInstanceExtractAttrValue(pThis,(VmClassAttr *)pEntry->pUserData);` |
|       17 | 3239 | `}` |
|        - | 3240 | `/*` |
|        - | 3241 | `` * Return the `value` property value (the backing value) of an enum case`` |
|        - | 3242 | ` * instance, or 0 when unavailable (pure enums have none).` |
|        - | 3243 | ` */` |
|       20 | 3244 | `PH7_PRIVATE ph7_value * PH7_EnumCaseBackingValueOf(ph7_class_instance *pThis)` |
|        1 | 3245 | `{` |
|        - | 3246 | `	SyHashEntry *pEntry;` |
|       21 | 3247 | `	if( (pThis->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|      ! 0 | 3248 | `		return 0;` |
|        - | 3249 | `	}` |
|       21 | 3250 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)"value",sizeof("value")-1);` |
|       21 | 3251 | `	if( pEntry == 0 ){` |
|        7 | 3252 | `		return 0;` |
|        - | 3253 | `	}` |
|       15 | 3254 | `	return PH7_ClassInstanceExtractAttrValue(pThis,(VmClassAttr *)pEntry->pUserData);` |
|       11 | 3255 | `}` |
|        - | 3256 | `/*` |
|        - | 3257 | ` * Emit a class-instance dump header plus its trailing newline. For var_dump` |
|        - | 3258 | ` * (ShowType) it completes the "object(" prefix the caller already emitted as` |
|        - | 3259 | ` *   ClassName)#<id> (<count>) {` |
|        - | 3260 | ` * for print_r it emits the legacy PHL  Object(ClassName) {  (count/id unused).` |
|        - | 3261 | `` * Enum cases print php's `ClassName Enum {` print_r header (var_dump never`` |
|        - | 3262 | `` * reaches here for enums — PH7_MemObjDump prints `enum(S::A)` directly).`` |
|        - | 3263 | ` */` |
|      446 | 3264 | `static void DumpClassInstanceHeader(SyBlob *pOut,ph7_class *pClass,sxu32 nObjId,int ShowType,sxu32 nCount)` |
|        5 | 3265 | `{` |
|      451 | 3266 | `	if( ShowType ){` |
|        - | 3267 | ``		/* var_dump: `object(C)#id (n) {` */`` |
|      309 | 3268 | `		SyBlobFormat(&(*pOut),"object(%z)#%u (%u) {",&pClass->sName,nObjId,nCount);` |
|      309 | 3269 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      309 | 3270 | `		return;` |
|        - | 3271 | `	}` |
|        - | 3272 | ``	/* print_r: `C Object` / `E Enum[:backing]` — the '(' line is emitted by`` |
|        - | 3273 | `	 * the body renderer at the container indent. */` |
|      147 | 3274 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|      ! 0 | 3275 | `		SyBlobFormat(&(*pOut),"%z Enum",&pClass->sName);` |
|      ! 0 | 3276 | `		if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|      ! 0 | 3277 | `			SyBlobAppend(&(*pOut),":int",sizeof(":int")-1);` |
|      ! 0 | 3278 | `		}else if( pClass->nEnumBacking == MEMOBJ_STRING ){` |
|      ! 0 | 3279 | `			SyBlobAppend(&(*pOut),":string",sizeof(":string")-1);` |
|      ! 0 | 3280 | `		}` |
|      ! 0 | 3281 | `	}else{` |
|      147 | 3282 | `		SyBlobFormat(&(*pOut),"%z Object",&pClass->sName);` |
|        - | 3283 | `	}` |
|      147 | 3284 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      228 | 3285 | `}` |
|        - | 3286 | `/*` |
|        - | 3287 | ` * The class that DECLARED pAttr: inheritance shares attr pointers down the` |
|        - | 3288 | ` * chain, so the declaring class is the most ANCESTRAL class whose hAttr still` |
|        - | 3289 | ` * maps the name to this exact pointer. php's var_dump/print_r use it for the` |
|        - | 3290 | `` * `["p":"Decl":private]` annotation.`` |
|        - | 3291 | ` */` |
|       54 | 3292 | `static ph7_class * OoAttrDeclaringClass(ph7_class *pClass,ph7_class_attr *pAttr)` |
|        2 | 3293 | `{` |
|        - | 3294 | `	/* Attrs record their declaring class at install time (inheritance/trait` |
|        - | 3295 | `	 * copies share the pointer, so the field survives the chain) -- and a TRAIT's` |
|        - | 3296 | `	 * members belong to the class that composed them, which is what php names. */` |
|       56 | 3297 | `	return PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|        2 | 3298 | `}` |
|        - | 3299 | `/*` |
|        - | 3300 | ` * php's zend_unmangle_property_name_ex: a property key carries its own` |
|        - | 3301 | ` * visibility when it is MANGLED — "\0*\0name" is protected and "\0Class\0name"` |
|        - | 3302 | ` * is private to Class, which is how the (array) cast, __debugInfo() and every` |
|        - | 3303 | ` * get_debug_info handler say what a plain array key cannot. A key that does not` |
|        - | 3304 | ` * begin with a NUL is a public name and comes back unchanged.` |
|        - | 3305 | ` *` |
|        - | 3306 | ` * Answers 0 for a key that begins with a NUL and is NOT a well-formed mangled` |
|        - | 3307 | ` * name — php's "Illegal member variable name" (nothing after the NUL, or an` |
|        - | 3308 | ` * empty class part) and "Corrupt member variable name" (no second NUL, or` |
|        - | 3309 | ` * nothing after it). php renders those raw, notice aside, and so does the caller.` |
|        - | 3310 | ` */` |
|      734 | 3311 | `PH7_PRIVATE int PH7_UnmangleAttrName(const char *zKey,sxu32 nKey,SyString *pClass,SyString *pName)` |
|        4 | 3312 | `{` |
|        - | 3313 | `	sxu32 nCls,nSrc;` |
|      738 | 3314 | `	SyStringInitFromBuf(pClass,0,0);` |
|      738 | 3315 | `	SyStringInitFromBuf(pName,zKey,nKey);` |
|      738 | 3316 | `	if( nKey < 1 \|\| zKey[0] != 0 ){` |
|      576 | 3317 | `		return 1;   /* a plain public name */` |
|        - | 3318 | `	}` |
|      163 | 3319 | `	if( nKey < 3 \|\| zKey[1] == 0 ){` |
|      ! 0 | 3320 | `		return 0;   /* php: "Illegal member variable name" */` |
|        - | 3321 | `	}` |
|      163 | 3322 | `	nCls = 0;` |
|     1857 | 3323 | `	while( nCls < nKey - 2 && zKey[1+nCls] != 0 ){` |
|     1695 | 3324 | `		nCls++;` |
|        1 | 3325 | `	}` |
|      163 | 3326 | `	if( nCls >= nKey - 2 ){` |
|      ! 0 | 3327 | `		return 0;   /* php: "Corrupt member variable name" */` |
|        - | 3328 | `	}` |
|        - | 3329 | `	/* An ANONYMOUS class mangles its source location in as a SECOND NUL-separated` |
|        - | 3330 | `	 * part, so the property name is what follows the LAST NUL rather than the` |
|        - | 3331 | `	 * second one (php's anonclass_src_len step). The class STRING php shows is` |
|        - | 3332 | `	 * still only the first part — it prints that one as a C string. */` |
|      163 | 3333 | `	nSrc = 0;` |
|     1119 | 3334 | `	while( nCls + 2 + nSrc < nKey && zKey[nCls+2+nSrc] != 0 ){` |
|      957 | 3335 | `		nSrc++;` |
|        1 | 3336 | `	}` |
|      163 | 3337 | `	SyStringInitFromBuf(pClass,&zKey[1],nCls);` |
|      163 | 3338 | `	if( nCls + nSrc + 2 != nKey ){` |
|      ! 0 | 3339 | `		nCls += nSrc + 1;` |
|      ! 0 | 3340 | `	}` |
|      163 | 3341 | `	SyStringInitFromBuf(pName,&zKey[nCls+2],nKey - nCls - 2);` |
|      163 | 3342 | `	return 1;` |
|      371 | 3343 | `}` |
|        - | 3344 | `/*` |
|        - | 3345 | `` * Emit a property's dump key: var_dump `["x"]=>` / `["p":"C":private]=>` /`` |
|        - | 3346 | `` * `["q":protected]=>`; print_r `[x] => ` / `[p:C:private] => ` /`` |
|        - | 3347 | `` * `[q:protected] => ` (php's exact annotations).`` |
|        - | 3348 | ` */` |
|      400 | 3349 | `static void OoDumpPropKey(SyBlob *pOut,ph7_class_instance *pThis,ph7_class_attr *pAttr,int ShowType)` |
|        4 | 3350 | `{` |
|      404 | 3351 | `	const char *zQ = ShowType ? "\"" : "";` |
|      404 | 3352 | `	if( SyStringLength(&pAttr->sName) > 0 && SyStringData(&pAttr->sName)[0] == 0 ){` |
|        - | 3353 | `		/* A MANGLED name stored raw — the __PHP_Incomplete_Class carrier's` |
|        - | 3354 | `		 * private/protected payload keys. php's dump unmangles them exactly as` |
|        - | 3355 | ``		 * it does an (array) cast's: `["bp":"PB":private]` / `["pp":protected]`. */`` |
|        - | 3356 | `		SyString sUnmCls, sUnmName;` |
|        9 | 3357 | `		if( PH7_UnmangleAttrName(SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),` |
|        - | 3358 | `			&sUnmCls,&sUnmName) ){` |
|        9 | 3359 | `			SyBlobFormat(&(*pOut),"[%s%z%s",zQ,&sUnmName,zQ);` |
|        9 | 3360 | `			if( sUnmCls.nByte == 1 && sUnmCls.zString[0] == '*' ){` |
|        5 | 3361 | `				SyBlobAppend(&(*pOut),":protected",sizeof(":protected")-1);` |
|        3 | 3362 | `			}else{` |
|        5 | 3363 | `				SyBlobFormat(&(*pOut),":%s%z%s:private",zQ,&sUnmCls,zQ);` |
|        - | 3364 | `			}` |
|        9 | 3365 | `			SyBlobAppend(&(*pOut),ShowType ? "]=>" : "] => ",ShowType ? 3 : 5);` |
|        9 | 3366 | `			return;` |
|        - | 3367 | `		}` |
|      ! 0 | 3368 | `	}` |
|      396 | 3369 | `	SyBlobFormat(&(*pOut),"[%s%z%s",zQ,&pAttr->sName,zQ);` |
|      396 | 3370 | `	if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       56 | 3371 | `		ph7_class *pDecl = OoAttrDeclaringClass(pThis->pClass,pAttr);` |
|       56 | 3372 | `		SyBlobFormat(&(*pOut),":%s%z%s:private",zQ,&pDecl->sName,zQ);` |
|      369 | 3373 | `	}else if( pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
|       65 | 3374 | `		SyBlobAppend(&(*pOut),":protected",sizeof(":protected")-1);` |
|       31 | 3375 | `	}` |
|      396 | 3376 | `	SyBlobAppend(&(*pOut),ShowType ? "]=>" : "] => ",ShowType ? 3 : 5);` |
|      204 | 3377 | `}` |
|        - | 3378 | `/*` |
|        - | 3379 | `` * Is this property's value a REFERENCE? -- php's `Z_ISREF_P`, asked of a property.`` |
|        - | 3380 | ` *` |
|        - | 3381 | `` * Two things turn on it. var_dump prints `&` for a property that IS one -- either end`` |
|        - | 3382 | `` * of the bind, `$o->p =& $x` and `$r =& $o->p` alike -- exactly as the array renderer`` |
|        - | 3383 | ` * marks an element something else holds (PH7_HashmapNodeIsRef). A property differs` |
|        - | 3384 | ` * only in the THRESHOLD: the property itself is not always one of the holders the` |
|        - | 3385 | ` * reference table names -- a DECLARED property holds nothing, a dynamic or re-created` |
|        - | 3386 | ` * one holds a permanent pin, and a bound one holds a counted pin -- so the threshold` |
|        - | 3387 | ` * is that one hold rather than the element renderer's flat two. print_r marks nothing,` |
|        - | 3388 | ` * in either container.` |
|        - | 3389 | ` *` |
|        - | 3390 | ` * The second is what an ARRAY built out of the property table carries:` |
|        - | 3391 | `` * `get_object_vars()`, the `(array)` cast, `get_mangled_object_vars()` and the SPL`` |
|        - | 3392 | ` * storage built from an object hand out the property's own SLOT for a property that` |
|        - | 3393 | ` * is a reference, so a write through the element reaches the object. Everything else` |
|        - | 3394 | ` * stays the copy it has always been.` |
|        - | 3395 | ` */` |
|     1982 | 3396 | `PH7_PRIVATE int PH7_ClassAttrIsRef(ph7_class_instance *pThis,VmClassAttr *pVmAttr)` |
|        5 | 3397 | `{` |
|        - | 3398 | `	sxu32 nSelf;` |
|     1987 | 3399 | `	if( pVmAttr == 0 \|\| pVmAttr->nIdx == SXU32_HIGH ){` |
|      ! 0 | 3400 | `		return 0;` |
|        - | 3401 | `	}` |
|     1987 | 3402 | `	nSelf = PH7_VmSlotSelfPinned(pThis->pVm,pVmAttr->nIdx) ? 1 : 0;` |
|     1987 | 3403 | `	return PH7_VmSlotHolderCount(pThis->pVm,pVmAttr->nIdx) > nSelf;` |
|      996 | 3404 | `}` |
|      450 | 3405 | `PH7_PRIVATE sxi32 PH7_ClassInstanceDump(SyBlob *pOut,ph7_class_instance *pThis,int ShowType,int nTab,int nDepth)` |
|        5 | 3406 | `{` |
|        - | 3407 | `	SyHashEntry *pEntry;` |
|        - | 3408 | `	ph7_value *pValue;` |
|        - | 3409 | `	sxi32 rc;` |
|        - | 3410 | `	int i;` |
|      455 | 3411 | `	if( nDepth > 31 ){` |
|        - | 3412 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|        - | 3413 | `		/* Nesting limit reached..halt immediately*/` |
|        5 | 3414 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|        5 | 3415 | `		return SXERR_LIMIT;` |
|        - | 3416 | `	}` |
|      451 | 3417 | `	rc = SXRET_OK;` |
|        - | 3418 | `	{` |
|        - | 3419 | `		/* A native class's PRESENTATION (php's get_debug_info): the shape it shows` |
|        - | 3420 | `		 * is not its storage — a DateTime shows date/timezone_type/timezone, a` |
|        - | 3421 | `		 * WeakReference shows ["object"] — and the slots underneath are hidden.` |
|        - | 3422 | `		 * Rendered exactly like a __debugInfo() array, which is what php does with` |
|        - | 3423 | `		 * it, and consulted first because php's handler wins over a userland` |
|        - | 3424 | `		 * method a native class cannot declare anyway. */` |
|        - | 3425 | `		ph7_value sPresent;` |
|      451 | 3426 | `		PH7_MemObjInit(pThis->pVm,&sPresent);` |
|      451 | 3427 | `		if( ph7_value_is_array(&sPresent) == 0 ){` |
|      451 | 3428 | `			ph7_hashmap *pPresent = PH7_NewHashmap(pThis->pVm,0,0);` |
|      451 | 3429 | `			if( pPresent ){` |
|      451 | 3430 | `				sPresent.x.pOther = pPresent;` |
|      451 | 3431 | `				MemObjSetType(&sPresent,MEMOBJ_HASHMAP);` |
|      223 | 3432 | `			}` |
|      223 | 3433 | `		}` |
|      446 | 3434 | `		if( (sPresent.iFlags & MEMOBJ_HASHMAP)` |
|      451 | 3435 | `		 && PH7_ClassInstancePresent(pThis,&sPresent,1) ){` |
|      159 | 3436 | `			ph7_hashmap *pMap = (ph7_hashmap *)sPresent.x.pOther;` |
|      159 | 3437 | `			DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,pMap->nEntry);` |
|      159 | 3438 | `			if( !ShowType ){` |
|       85 | 3439 | `				for( i = 0 ; i < nTab ; i++ ){` |
|      ! 0 | 3440 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      ! 0 | 3441 | `				}` |
|       85 | 3442 | `				SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       41 | 3443 | `			}` |
|      159 | 3444 | `			rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth,1);` |
|      159 | 3445 | `			for( i = 0 ; i < nTab ; i++ ){` |
|      ! 0 | 3446 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      ! 0 | 3447 | `			}` |
|      159 | 3448 | `			if( ShowType ){` |
|       75 | 3449 | `				SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|       38 | 3450 | `			}else{` |
|       85 | 3451 | `				SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        - | 3452 | `			}` |
|      159 | 3453 | `			PH7_MemObjRelease(&sPresent);` |
|      159 | 3454 | `			return rc;` |
|        - | 3455 | `		}` |
|      295 | 3456 | `		PH7_MemObjRelease(&sPresent);` |
|        - | 3457 | `	}` |
|        - | 3458 | `	{` |
|        - | 3459 | `		/* Both var_dump and print_r consult __debugInfo() (PHP behavior);` |
|        - | 3460 | `		 * var_export uses a separate renderer and never reaches here. When the` |
|        - | 3461 | `		 * method is present and returns an array, render that array's entries as` |
|        - | 3462 | `		 * the object body, with the header showing the debug array's count. The` |
|        - | 3463 | `		 * nDepth guard above protects against a __debugInfo returning the object` |
|        - | 3464 | `		 * itself. */` |
|      295 | 3465 | `		ph7_class_method *pDbg = PH7_ClassExtractMethod(pThis->pClass,"__debugInfo",sizeof("__debugInfo")-1);` |
|      295 | 3466 | `		if( pDbg ){` |
|        - | 3467 | `			ph7_value sResult;` |
|       18 | 3468 | `			PH7_MemObjInit(pThis->pVm,&sResult);` |
|       18 | 3469 | `			PH7_VmCallMagicMethod(pThis->pVm,pThis,pDbg,&sResult,0,0);` |
|       18 | 3470 | `			if( sResult.iFlags & MEMOBJ_HASHMAP ){` |
|       18 | 3471 | `				ph7_hashmap *pMap = (ph7_hashmap *)sResult.x.pOther;` |
|        - | 3472 | `				/* Header count is the debug array's entry count. */` |
|       18 | 3473 | `				DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,pMap->nEntry);` |
|       18 | 3474 | `				if( !ShowType ){` |
|        8 | 3475 | `					for( i = 0 ; i < nTab ; i++ ){` |
|      ! 0 | 3476 | `						SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      ! 0 | 3477 | `					}` |
|        8 | 3478 | `					SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|        3 | 3479 | `				}` |
|       18 | 3480 | `				rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth,1);` |
|       18 | 3481 | `				for( i = 0 ; i < nTab ; i++ ){` |
|      ! 0 | 3482 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      ! 0 | 3483 | `				}` |
|       18 | 3484 | `				if( ShowType ){` |
|       12 | 3485 | `					SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|        7 | 3486 | `				}else{` |
|        8 | 3487 | `					SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        - | 3488 | `				}` |
|       18 | 3489 | `				PH7_MemObjRelease(&sResult);` |
|       18 | 3490 | `				return rc;` |
|        - | 3491 | `			}` |
|        - | 3492 | `			/* Non-array return: behave as if __debugInfo were absent. */` |
|      ! 0 | 3493 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 | 3494 | `		}` |
|        - | 3495 | `	}` |
|        - | 3496 | `	{` |
|        - | 3497 | `		/* var_dump's header needs the property count up front, so pre-count the` |
|        - | 3498 | `		 * non-static/non-constant attributes (matching the dump loop below).` |
|        - | 3499 | `		 * An UNINITIALIZED typed property is still LISTED by var_dump — with php's` |
|        - | 3500 | ``		 * `uninitialized(T)` marker in place of a value — but it does not COUNT,`` |
|        - | 3501 | `` 		 * which is how php's `object(C)#1 (0) { ["a"]=> uninitialized(int) }` `` |
|        - | 3502 | `		 * reads. */` |
|      279 | 3503 | `		sxu32 nProp = 0;` |
|      279 | 3504 | `		if( ShowType ){` |
|      225 | 3505 | `			SyHashResetLoopCursor(&pThis->hAttr);` |
|      583 | 3506 | `			while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|      362 | 3507 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      358 | 3508 | `				if( !PH7_ATTR_UNPRESENTED(pVmAttr)` |
|      334 | 3509 | `				 && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL) == 0` |
|      338 | 3510 | `				 && !PH7_ClassAttrUninitialized(pVmAttr) ){` |
|      300 | 3511 | `					nProp++;` |
|      148 | 3512 | `				}` |
|        4 | 3513 | `			}` |
|      110 | 3514 | `		}` |
|      279 | 3515 | `		DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,nProp);` |
|        - | 3516 | `	}` |
|      279 | 3517 | `	if( !ShowType ){` |
|        - | 3518 | `		/* print_r body opener: '(' at the container indent */` |
|      170 | 3519 | `		for( i = 0 ; i < nTab ; i++ ){` |
|      115 | 3520 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       59 | 3521 | `		}` |
|       58 | 3522 | `		SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       27 | 3523 | `	}` |
|        - | 3524 | `	/* Dump object attributes (php 8.4: VIRTUAL hooked properties have no` |
|        - | 3525 | `	 * backing store — excluded from var_dump/print_r) */` |
|      279 | 3526 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      625 | 3527 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|      474 | 3528 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      470 | 3529 | `		if( !PH7_ATTR_UNPRESENTED(pVmAttr)` |
|      428 | 3530 | `		 && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL) == 0 ){` |
|      428 | 3531 | `			if( PH7_ClassAttrUninitialized(pVmAttr) ){` |
|        - | 3532 | `				/* var_dump names the property and prints php's marker in place of` |
|        - | 3533 | `				 * the value it has not got; print_r has no such marker and leaves` |
|        - | 3534 | `				 * the property out entirely. */` |
|       65 | 3535 | `				if( ShowType ){` |
|        - | 3536 | `					char zType[192];` |
|       60 | 3537 | `					const char *zText = VmHintTextResolved(pThis->pVm,&pVmAttr->pAttr->sTypeName,` |
|       38 | 3538 | `						VmHintScopeDeclared(pVmAttr->pAttr->pDeclClass),` |
|       19 | 3539 | `						zType,sizeof(zType));` |
|      117 | 3540 | `					for( i = 0 ; i < nTab + 2 ; i++ ){` |
|       79 | 3541 | `						SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       41 | 3542 | `					}` |
|       41 | 3543 | `					OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,TRUE);` |
|       41 | 3544 | `					SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      117 | 3545 | `					for( i = 0 ; i < nTab + 2 ; i++ ){` |
|       79 | 3546 | `						SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       41 | 3547 | `					}` |
|       41 | 3548 | `					SyBlobFormat(&(*pOut),"uninitialized(%s)\n",zText);` |
|       19 | 3549 | `				}` |
|       65 | 3550 | `				continue;` |
|        - | 3551 | `			}` |
|        - | 3552 | `			/* Dump non-static/constant attribute only */` |
|      366 | 3553 | `			pValue = ExtractClassAttrValue(pThis->pVm,pVmAttr);` |
|      366 | 3554 | `			if( pValue == 0 ){` |
|      ! 0 | 3555 | `				continue;` |
|        - | 3556 | `			}` |
|      366 | 3557 | `			if( ShowType ){` |
|        - | 3558 | ``				/* var_dump prop: `["x"(:…)]=>` at nTab+2, the value on the next`` |
|        - | 3559 | `				 * line at the same indent (php). */` |
|     4628 | 3560 | `				for( i = 0 ; i < nTab + 2 ; i++ ){` |
|     4332 | 3561 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     2168 | 3562 | `				}` |
|      300 | 3563 | `				OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,TRUE);` |
|      300 | 3564 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      448 | 3565 | `				rc = PH7_MemObjDump(&(*pOut),pValue,TRUE,nTab+2,nDepth,` |
|      148 | 3566 | `					PH7_ClassAttrIsRef(pThis,pVmAttr));` |
|      300 | 3567 | `				if( rc == SXERR_LIMIT ){` |
|      125 | 3568 | `					break;` |
|        - | 3569 | `				}` |
|       90 | 3570 | `			}else{` |
|        - | 3571 | ``				/* print_r prop: `[x(:…)] => value` at nTab+4; container values`` |
|        - | 3572 | `				 * render their block at nTab+8 followed by php's blank line. */` |
|      398 | 3573 | `				for( i = 0 ; i < nTab + 4 ; i++ ){` |
|      332 | 3574 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      168 | 3575 | `				}` |
|       70 | 3576 | `				OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,FALSE);` |
|       66 | 3577 | `				if( (pValue->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ))` |
|       38 | 3578 | `				 && (pValue->iFlags & MEMOBJ_NULL) == 0 ){` |
|        3 | 3579 | `					rc = PH7_MemObjDump(&(*pOut),pValue,FALSE,nTab+8,nDepth,0);` |
|        3 | 3580 | `					SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        3 | 3581 | `					if( rc == SXERR_LIMIT ){` |
|      ! 0 | 3582 | `						break;` |
|        - | 3583 | `					}` |
|        2 | 3584 | `				}else{` |
|       68 | 3585 | `					PH7_MemObjPrintRInline(&(*pOut),pValue);` |
|       68 | 3586 | `					SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        - | 3587 | `				}` |
|        - | 3588 | `			}` |
|      119 | 3589 | `		}` |
|        4 | 3590 | `	}` |
|     4139 | 3591 | `	for( i = 0 ; i < nTab ; i++ ){` |
|     3864 | 3592 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     1934 | 3593 | `	}` |
|      279 | 3594 | `	if( ShowType ){` |
|      225 | 3595 | `		SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      115 | 3596 | `	}else{` |
|       58 | 3597 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        - | 3598 | `	}` |
|      279 | 3599 | `	return rc;` |
|      230 | 3600 | `}` |
|        - | 3601 | `/*` |
|        - | 3602 | ` * Call a magic method [i.e: __toString(),__toBool(),__Invoke()...]` |
|        - | 3603 | ` * Return SXRET_OK on successfull call. Any other return value indicates failure.` |
|        - | 3604 | ` * Notes on magic methods.` |
|        - | 3605 | ` * According to the PHP language reference manual.` |
|        - | 3606 | ` *  The function names __construct(), __destruct(), __call(), __callStatic()` |
|        - | 3607 | ` *  __get(),  __toString(), __invoke(), __clone() are magical in PHP classes.` |
|        - | 3608 | ` * You cannot have functions with these names in any of your classes unless` |
|        - | 3609 | ` * you want the magic functionality associated with them.` |
|        - | 3610 | ` * Example of magical methods:` |
|        - | 3611 | ` * __toString()` |
|        - | 3612 | ` *  The __toString() method allows a class to decide how it will react when it is treated like` |
|        - | 3613 | ` *  a string. For example, what echo $obj; will print. This method must return a string.` |
|        - | 3614 | ` *  Example #2 Simple example` |
|        - | 3615 | ` * <?php` |
|        - | 3616 | ` * // Declare a simple class` |
|        - | 3617 | ` * class TestClass` |
|        - | 3618 | ` * {` |
|        - | 3619 | ` *   public $foo;` |
|        - | 3620 | ` *` |
|        - | 3621 | ` *   public function __construct($foo)` |
|        - | 3622 | ` *   {` |
|        - | 3623 | ` *       $this->foo = $foo;` |
|        - | 3624 | ` *   }` |
|        - | 3625 | ` *` |
|        - | 3626 | ` *   public function __toString()` |
|        - | 3627 | ` *   {` |
|        - | 3628 | ` *       return $this->foo;` |
|        - | 3629 | ` *   }` |
|        - | 3630 | ` * }` |
|        - | 3631 | ` * $class = new TestClass('Hello');` |
|        - | 3632 | ` * echo $class;` |
|        - | 3633 | ` * ?>` |
|        - | 3634 | ` * The above example will output:` |
|        - | 3635 | ` *  Hello` |
|        - | 3636 | ` *` |
|        - | 3637 | ` * Note that PH7 does not support all the magical method and introudces __toFloat(),__toInt()` |
|        - | 3638 | ` * which have the same behaviour as __toString() but for float and integer types` |
|        - | 3639 | ` * respectively.` |
|        - | 3640 | ` * Refer to the official documentation for more information.` |
|        - | 3641 | ` */` |
|      256 | 3642 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallMagicMethod(` |
|        - | 3643 | `	ph7_vm *pVm,               /* VM that own all this stuff */` |
|        - | 3644 | `	ph7_class *pClass,         /* Target class */` |
|        - | 3645 | `	ph7_class_instance *pThis, /* Target object */` |
|        - | 3646 | `	const char *zMethod,       /* Magic method name [i.e: __toString()]*/` |
|        - | 3647 | `	sxu32 nByte,               /* zMethod length*/` |
|        - | 3648 | `	const SyString *pAttrName, /* Attribute name */` |
|        - | 3649 | `	ph7_value *pResult         /* OUT: magic method return value. NULL to discard */` |
|        - | 3650 | `	)` |
|        4 | 3651 | `{` |
|      260 | 3652 | `	ph7_value *apArg[2] = { 0 , 0 };` |
|        - | 3653 | `	ph7_class_method *pMeth;` |
|        - | 3654 | `	ph7_value sAttr; /* cc warning */` |
|        - | 3655 | `	sxi32 rc;` |
|        - | 3656 | `	int nArg;` |
|      260 | 3657 | `	int bMagicGet = nByte == sizeof("__get")-1 && SyMemcmp(zMethod,"__get",nByte) == 0;` |
|      260 | 3658 | `	int bMagicIsset = nByte == sizeof("__isset")-1 && SyMemcmp(zMethod,"__isset",nByte) == 0;` |
|      256 | 3659 | `	if( (bMagicGet \|\| bMagicIsset) && pAttrName` |
|      255 | 3660 | `	 && PH7_ClassNativePropOwns(pThis,pAttrName) ){` |
|        - | 3661 | `		/* php's read_property / has_property handler for a name the class's own` |
|        - | 3662 | `		 * table carries: it answers before the standard path ever looks for a` |
|        - | 3663 | ``		 * magic accessor, so a subclass's `__get` does not shadow ext/dom's`` |
|        - | 3664 | ``		 * surface. The read-modify-write and `??=` rails reach the handler here;`` |
|        - | 3665 | `		 * the member opcode's own read gate asks it a step earlier. */` |
|        - | 3666 | `		PH7_NativePropCtx sNat;` |
|        - | 3667 | `		ph7_value sNatVal;` |
|        7 | 3668 | `		PH7_MemObjInit(pVm,&sNatVal);` |
|       10 | 3669 | `		if( PH7_ClassNativePropAsk(pThis,&sNat,` |
|        3 | 3670 | `				bMagicIsset ? PH7_NATIVE_PROP_ISSET : PH7_NATIVE_PROP_READ,pAttrName,&sNatVal) ){` |
|        7 | 3671 | `			if( sNat.zThrowClass ){` |
|      ! 0 | 3672 | `				VmBoundaryPark(pVm,VmThrowFixedErrorCode(pVm,sNat.zThrowClass,` |
|      ! 0 | 3673 | `					sNat.iThrowCode,sNat.zThrowMsg));` |
|        7 | 3674 | `			}else if( pResult ){` |
|        7 | 3675 | `				PH7_MemObjStore(&sNatVal,pResult);` |
|        3 | 3676 | `			}` |
|        7 | 3677 | `			PH7_MemObjRelease(&sNatVal);` |
|        7 | 3678 | `			return SXRET_OK;` |
|        - | 3679 | `		}` |
|      ! 0 | 3680 | `		PH7_MemObjRelease(&sNatVal);` |
|      ! 0 | 3681 | `	}` |
|        - | 3682 | `	/* Make sure the magic method is available */` |
|      254 | 3683 | `	pMeth = PH7_ClassExtractMethod(&(*pClass),zMethod,nByte);` |
|      254 | 3684 | `	if( pMeth == 0 ){` |
|        - | 3685 | `		/* No such method,return immediately */` |
|      ! 0 | 3686 | `		return SXERR_NOTFOUND;` |
|        - | 3687 | `	}` |
|      254 | 3688 | `	nArg = 0;` |
|        - | 3689 | `	/* Copy arguments */` |
|      254 | 3690 | `	if( pAttrName ){` |
|      254 | 3691 | `		PH7_MemObjInitFromString(pVm,&sAttr,pAttrName);` |
|      254 | 3692 | `		sAttr.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      254 | 3693 | `		apArg[0] = &sAttr;` |
|      254 | 3694 | `		nArg = 1;` |
|      125 | 3695 | `	}` |
|        - | 3696 | `	/* Call the magic method now */` |
|      254 | 3697 | `	rc = PH7_VmCallMagicMethod(pVm,&(*pThis),pMeth,pResult,nArg,apArg);` |
|        - | 3698 | `	/* Clean up */` |
|      254 | 3699 | `	if( pAttrName ){` |
|      254 | 3700 | `		PH7_MemObjRelease(&sAttr);` |
|      125 | 3701 | `	}` |
|      254 | 3702 | `	return rc;` |
|      132 | 3703 | `}` |
|        - | 3704 | `/*` |
|        - | 3705 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon].` |
|        - | 3706 | ` * This function is simply a wrapper on ExtractClassAttrValue().` |
|        - | 3707 | ` */` |
|  5864166 | 3708 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceExtractAttrValue(ph7_class_instance *pThis,VmClassAttr *pAttr)` |
|        5 | 3709 | `{` |
|        - | 3710 | `   /* Extract the attribute value */` |
|        - | 3711 | `	ph7_value *pValue;` |
|  5864171 | 3712 | `	pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|  5864171 | 3713 | `	return pValue;` |
|        5 | 3714 | `}` |
|        - | 3715 | `/*` |
|        - | 3716 | ` * Convert a class instance [i.e: Object in the PHP jargon] into a hashmap [i.e: array in the PHP jargon].` |
|        - | 3717 | ` * Return SXRET_OK on success. Any other value indicates failure.` |
|        - | 3718 | ` * Note on object conversion to array:` |
|        - | 3719 | ` *  Acccording to the PHP language reference manual` |
|        - | 3720 | ` *  If an object is converted to an array, the result is an array whose elements are the object's properties.` |
|        - | 3721 | ` *  The keys are the member variable names.` |
|        - | 3722 | ` *` |
|        - | 3723 | ` *  The following example:` |
|        - | 3724 | ` *  class Test {` |
|        - | 3725 | ` *   public $A = 25<<1;  // 50` |
|        - | 3726 | ` *	 public $c = rand_str(3);   // Random string of length 3` |
|        - | 3727 | ` *	 public $d = rand() & 1023; // Random number between 0..1023` |
|        - | 3728 | ` *  }` |
|        - | 3729 | ` *  var_dump((array) new Test());` |
|        - | 3730 | ` *	Will output:` |
|        - | 3731 | ` *  array(3) {` |
|        - | 3732 | ` *   [A] =>` |
|        - | 3733 | ` *      int(50)` |
|        - | 3734 | ` *   [c] =>` |
|        - | 3735 | ` *     string(3 'aps')` |
|        - | 3736 | ` *   [d] =>` |
|        - | 3737 | ` *     int(991)` |
|        - | 3738 | ` *  }` |
|        - | 3739 | ` * You have noticed that PH7 allow class attributes [i.e: $a,$c,$d in the example above]` |
|        - | 3740 | ` * have any complex expression (even function calls/Annonymous functions) as their default` |
|        - | 3741 | ` * value unlike the standard PHP engine.` |
|        - | 3742 | ` * This is a very powerful feature that you have to look at.` |
|        - | 3743 | ` */` |
|      726 | 3744 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|        5 | 3745 | `{` |
|        - | 3746 | `	{` |
|        - | 3747 | `		/* php's get_properties handler, which is what the (array) cast reads: a` |
|        - | 3748 | `		 * DateTime casts to date/timezone_type/timezone, not to the hidden slots` |
|        - | 3749 | `		 * holding its timestamp. A class whose hook answers only the DEBUG surface` |
|        - | 3750 | `		 * (WeakReference) fills nothing here, and that EMPTY answer is php's — the` |
|        - | 3751 | `		 * hook is authoritative, so there is no falling back to the slot walk. It` |
|        - | 3752 | `		 * used to fall back when the hook filled nothing, which was invisible while` |
|        - | 3753 | `		 * every hooked class also had no visible property: an ArrayObject SUBCLASS` |
|        - | 3754 | ``		 * with an empty storage would have cast to its own `p` where php casts to`` |
|        - | 3755 | `		 * the (empty) storage. */` |
|        - | 3756 | `		ph7_value sPresent;` |
|      731 | 3757 | `		PH7_MemObjInit(pThis->pVm,&sPresent);` |
|      731 | 3758 | `		sPresent.x.pOther = pMap;` |
|      731 | 3759 | `		MemObjSetType(&sPresent,MEMOBJ_HASHMAP);` |
|      731 | 3760 | `		if( PH7_ClassInstancePresent(pThis,&sPresent,0) ){` |
|        - | 3761 | `			/* The map IS the destination; do not release the carrier's hashmap. */` |
|      557 | 3762 | `			sPresent.x.pOther = 0;` |
|      557 | 3763 | `			sPresent.iFlags = MEMOBJ_NULL;` |
|      557 | 3764 | `			return SXRET_OK;` |
|        - | 3765 | `		}` |
|      177 | 3766 | `		sPresent.x.pOther = 0;` |
|      177 | 3767 | `		sPresent.iFlags = MEMOBJ_NULL;` |
|      177 | 3768 | `		PH7_MemObjRelease(&sPresent);` |
|        - | 3769 | `	}` |
|      177 | 3770 | `	return PH7_ClassInstanceToHashmapRaw(pThis,pMap);` |
|      368 | 3771 | `}` |
|        - | 3772 | `/*` |
|        - | 3773 | ` * Is this property NOT THERE YET?` |
|        - | 3774 | ` *` |
|        - | 3775 | ` * php 7.4's typed properties have a third state beside "holds a value" and "does not` |
|        - | 3776 | `` * exist": a typed property with no default is UNINITIALIZED at `new`, reading it is an`` |
|        - | 3777 | ` * Error rather than a null, and every surface that presents an object's properties` |
|        - | 3778 | ` * leaves it out — get_object_vars(), get_mangled_object_vars(), the (array) cast,` |
|        - | 3779 | ` * foreach, json_encode(), serialize(), print_r() and var_export(). var_dump() is the one` |
|        - | 3780 | ``  * exception and only half of one: it NAMES the property and prints `uninitialized(T)` `` |
|        - | 3781 | ` * where the value would be, and does not count it in the header.` |
|        - | 3782 | ` *` |
|        - | 3783 | ` * The state is already tracked (it is what makes the read throw); this asks it by name` |
|        - | 3784 | ` * so the eight surfaces agree. VM_CLASS_ATTR_UNINIT doubles as the readonly write-once` |
|        - | 3785 | ` * latch, which wants the same answer: a readonly property must be typed and cannot have` |
|        - | 3786 | ` * a default, so before its first write php calls it uninitialized too.` |
|        - | 3787 | ` */` |
|     4344 | 3788 | `PH7_PRIVATE int PH7_ClassAttrUninitialized(VmClassAttr *pVmAttr)` |
|        5 | 3789 | `{` |
|     4349 | 3790 | `	return pVmAttr != 0 && (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) != 0;` |
|        5 | 3791 | `}` |
|        - | 3792 | `/*` |
|        - | 3793 | `` * The same question asked by the four surfaces that read through a php 8.4 `get` HOOK —`` |
|        - | 3794 | ` * get_object_vars(), json_encode(), foreach and var_export().` |
|        - | 3795 | ` *` |
|        - | 3796 | ` * A hooked property's value is whatever its hook answers, so an empty backing slot is` |
|        - | 3797 | ` * not an absence there: a VIRTUAL property has no slot at all and still has a value` |
|        - | 3798 | `` * (php lists `virt` in all four), and a BACKED one whose hook reads its own slot raises`` |
|        - | 3799 | ` * the uninitialized Error from inside the hook — which is php's answer for these four` |
|        - | 3800 | ``  * and not something to pre-empt by skipping the property. Only a property with no `get` `` |
|        - | 3801 | ` * at all is absent.` |
|        - | 3802 | ` */` |
|     1458 | 3803 | `PH7_PRIVATE int PH7_ClassAttrUninitializedForRead(VmClassAttr *pVmAttr)` |
|        5 | 3804 | `{` |
|     1531 | 3805 | `	return PH7_ClassAttrUninitialized(pVmAttr)` |
|     1458 | 3806 | `		&& (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET) == 0;` |
|        5 | 3807 | `}` |
|        - | 3808 | `/*` |
|        - | 3809 | ` * The SLOT walk under the cast above, with php's get_properties handler left out:` |
|        - | 3810 | ` * the instance's own property table, mangled, and nothing else.` |
|        - | 3811 | ` *` |
|        - | 3812 | `` * This is what `get_mangled_object_vars()` answers — php asks the same handler with a`` |
|        - | 3813 | ` * different PURPOSE there, and every class here that has a handler answers the raw` |
|        - | 3814 | `` * table for it: `get_mangled_object_vars(new ArrayObject([1,2]))` is the EMPTY array`` |
|        - | 3815 | `` * where the cast is `[1,2]`, and a DateTime's is empty where the cast has three keys.`` |
|        - | 3816 | ` * Since PHL's engine slots are hidden (and php's equivalents live outside the property` |
|        - | 3817 | ` * table entirely), dropping the handler is the whole difference.` |
|        - | 3818 | ` */` |
|      968 | 3819 | `static sxi32 ClassInstanceToHashmapRaw(ph7_class_instance *pThis,ph7_hashmap *pMap,int bOwnOnly)` |
|        5 | 3820 | `{` |
|        - | 3821 | `	SyHashEntry *pEntry;` |
|        - | 3822 | `	VmClassAttr *pAttr;` |
|        - | 3823 | `	ph7_value *pValue;` |
|        - | 3824 | `	ph7_value sName;` |
|        - | 3825 | `	/* Reset the loop cursor */` |
|      973 | 3826 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      973 | 3827 | `	PH7_MemObjInitFromString(pThis->pVm,&sName,0);` |
|     6493 | 3828 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|        - | 3829 | `		/* Point to the current attribute */` |
|     5525 | 3830 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|     5525 | 3831 | `		if( !PH7_ClassInstanceAttrPresented(pAttr) ){` |
|        - | 3832 | `			/* Not part of the raw table: a class-level member, a typed property` |
|        - | 3833 | `			 * never written, or a php 8.4 VIRTUAL hooked one. */` |
|     4076 | 3834 | `			continue;` |
|        - | 3835 | `		}` |
|     1453 | 3836 | `		if( bOwnOnly && (pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_NATIVE_SET` |
|        - | 3837 | `			\|PH7_CLASS_ATTR_NATIVE_VIRTUAL\|PH7_CLASS_ATTR_NATIVE_LAZY)) ){` |
|        - | 3838 | `			/* The STATE of a native class whose state happens to be public` |
|        - | 3839 | `			 * (DateInterval's ten, DatePeriod's seven): the caller is building the` |
|        - | 3840 | `			 * shape those belong to, and wants only what the OBJECT added to it. */` |
|      388 | 3841 | `			continue;` |
|        - | 3842 | `		}` |
|        - | 3843 | `		/* Extract attribute value */` |
|     1067 | 3844 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     1067 | 3845 | `		if( pValue ){` |
|     1067 | 3846 | `			PH7_ClassInstanceAttrKey(pThis,pAttr,&sName);` |
|        - | 3847 | `			/* Perform the insertion. An OWN-props walk laid beside a shape the` |
|        - | 3848 | `			 * caller already built ADDS rather than updates: php's` |
|        - | 3849 | `			 * add_common_properties is a zend_hash_add, so a subclass property` |
|        - | 3850 | `			 * named like one of the internal keys loses to the internal value` |
|        - | 3851 | ``			 * there (`class S extends DateTime { public $date; }` serializes the`` |
|        - | 3852 | `			 * DATE). */` |
|     1067 | 3853 | `			if( bOwnOnly ){` |
|      129 | 3854 | `				ph7_hashmap_node *pDup = 0;` |
|      129 | 3855 | `				if( PH7_HashmapLookup(pMap,&sName,&pDup) == SXRET_OK ){` |
|        3 | 3856 | `					SyBlobReset(&sName.sBlob);` |
|        3 | 3857 | `					continue;` |
|        - | 3858 | `				}` |
|       63 | 3859 | `			}` |
|     1065 | 3860 | `			if( PH7_ClassAttrIsRef(pThis,pAttr) ){` |
|        - | 3861 | `				/* php hands out the property's own REFERENCE, not a copy of what it` |
|        - | 3862 | ``				 * holds: `$v = (array)$o; $v['p'] = 9;` reaches the object when `p` is`` |
|        - | 3863 | ``				 * a reference, and `var_dump()` marks the element `&` in both places.`` |
|        - | 3864 | `				 * Only a property that IS one -- something else names its slot -- and` |
|        - | 3865 | `				 * never the ordinary copy every other element still takes. */` |
|       17 | 3866 | `				PH7_HashmapInsertByRef(pMap,&sName,pAttr->nIdx);` |
|        9 | 3867 | `			}else{` |
|     1049 | 3868 | `				PH7_HashmapInsert(pMap,&sName,pValue);` |
|        - | 3869 | `			}` |
|        - | 3870 | `			/* Reset the string cursor */` |
|     1065 | 3871 | `			SyBlobReset(&sName.sBlob);` |
|      530 | 3872 | `		}` |
|        5 | 3873 | `	}` |
|      973 | 3874 | `	PH7_MemObjRelease(&sName);` |
|      973 | 3875 | `	return SXRET_OK;` |
|        5 | 3876 | `}` |
|      284 | 3877 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmapRaw(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|        5 | 3878 | `{` |
|      289 | 3879 | `	return ClassInstanceToHashmapRaw(pThis,pMap,0);` |
|        5 | 3880 | `}` |
|        - | 3881 | `/*` |
|        - | 3882 | ` * The same walk, restricted to what the OBJECT added: a native class's own public` |
|        - | 3883 | ` * STATE is left out, so a subclass's properties can be laid beside the shape that` |
|        - | 3884 | ` * state builds rather than inside it. php's add_common_properties.` |
|        - | 3885 | ` */` |
|      684 | 3886 | `PH7_PRIVATE sxi32 PH7_ClassInstanceOwnPropsToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|        2 | 3887 | `{` |
|      686 | 3888 | `	return ClassInstanceToHashmapRaw(pThis,pMap,1);` |
|        2 | 3889 | `}` |
|        - | 3890 | `/*` |
|        - | 3891 | ` * Iterate throw class attributes and invoke the given callback [i.e: xWalk()] for each` |
|        - | 3892 | ` * retrieved attribute.` |
|        - | 3893 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|        - | 3894 | ` * the attribute value in the callback body will not alter the real attribute value.` |
|        - | 3895 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|        - | 3896 | ` * a value different from PH7_OK.` |
|        - | 3897 | ` * Refer to [ph7_object_walk()] for more information.` |
|        - | 3898 | ` */` |
|      ! 0 | 3899 | `PH7_PRIVATE sxi32 PH7_ClassInstanceWalk(` |
|        - | 3900 | `	ph7_class_instance *pThis, /* Target object */` |
|        - | 3901 | `	int (*xWalk)(const char *,ph7_value *,void *), /* Walker callback */` |
|        - | 3902 | `	void *pUserData /* Last argument to xWalk() */` |
|        - | 3903 | `	)` |
|      ! 0 | 3904 | `{` |
|        - | 3905 | `	SyHashEntry *pEntry; /* Hash entry */` |
|        - | 3906 | `	VmClassAttr *pAttr;  /* Pointer to the attribute */` |
|        - | 3907 | `	ph7_value *pValue;   /* Attribute value */` |
|        - | 3908 | `	ph7_value sValue;    /* Copy of the attribute value */` |
|        - | 3909 | `	int rc;` |
|        - | 3910 | `	/* Reset the loop cursor */` |
|      ! 0 | 3911 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      ! 0 | 3912 | `	PH7_MemObjInit(pThis->pVm,&sValue);` |
|        - | 3913 | `	/* Start the walk process */` |
|      ! 0 | 3914 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|        - | 3915 | `		/* Point to the current attribute */` |
|      ! 0 | 3916 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      ! 0 | 3917 | `		if( PH7_ATTR_UNPRESENTED(pAttr) ){` |
|        - | 3918 | `			/* Class-level members are not part of the object (php) */` |
|      ! 0 | 3919 | `			continue;` |
|        - | 3920 | `		}` |
|      ! 0 | 3921 | `		if( PH7_ClassAttrUninitialized(pAttr) ){` |
|      ! 0 | 3922 | `			continue; /* typed, never written: not there yet (php) */` |
|        - | 3923 | `		}` |
|        - | 3924 | `		/* Extract attribute value */` |
|      ! 0 | 3925 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|      ! 0 | 3926 | `		if( pValue ){` |
|      ! 0 | 3927 | `			PH7_MemObjLoad(pValue,&sValue);` |
|        - | 3928 | `			/* Invoke the supplied callback */` |
|      ! 0 | 3929 | `			rc =  xWalk(SyStringData(&pAttr->pAttr->sName),&sValue,pUserData);` |
|      ! 0 | 3930 | `			PH7_MemObjRelease(&sValue);` |
|      ! 0 | 3931 | `			if( rc != PH7_OK){` |
|        - | 3932 | `				/* User callback request an operation abort */` |
|      ! 0 | 3933 | `				return SXERR_ABORT;` |
|        - | 3934 | `			}` |
|      ! 0 | 3935 | `		}` |
|      ! 0 | 3936 | `	}` |
|        - | 3937 | `	/* All done */` |
|      ! 0 | 3938 | `	return SXRET_OK;` |
|      ! 0 | 3939 | `}` |
|        - | 3940 | `/*` |
|        - | 3941 | ` * The instance's attribute entry for a property name, INCLUDING the empty one.` |
|        - | 3942 | ` *` |
|        - | 3943 | ` * SyHashGet answers 0 for a zero-length key engine-wide, so a property named ""` |
|        - | 3944 | `` * — php's own `s:0:""` payload builds one, and every surface that walks hAttr`` |
|        - | 3945 | ` * shows it — can only be found by walking the list. Used by the presentation` |
|        - | 3946 | ` * surfaces that snapshot names and re-look-up each one before reading it (a PHP` |
|        - | 3947 | ` * 8.4 get hook may unset a property mid-walk), where the plain hash probe would` |
|        - | 3948 | ` * silently drop it from the output while var_dump, which walks directly, showed` |
|        - | 3949 | ` * it. The walk uses the hash's embedded loop cursor, so it must not run inside` |
|        - | 3950 | ` * another walk of the SAME table — the callers below re-enter only through the` |
|        - | 3951 | ` * hook dispatch, which happens after this returns.` |
|        - | 3952 | ` */` |
|      708 | 3953 | `PH7_PRIVATE SyHashEntry * PH7_ClassInstanceAttrEntry(ph7_class_instance *pThis,const char *zName,sxu32 nName)` |
|        5 | 3954 | `{` |
|        - | 3955 | `	SyHashEntry *pEntry;` |
|      713 | 3956 | `	if( nName > 0 ){` |
|      651 | 3957 | `		return SyHashGet(&pThis->hAttr,(const void *)zName,nName);` |
|        - | 3958 | `	}` |
|       63 | 3959 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      139 | 3960 | `	while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       91 | 3961 | `		if( pEntry->nKeyLen == 0 ){` |
|       15 | 3962 | `			return pEntry;` |
|        - | 3963 | `		}` |
|        1 | 3964 | `	}` |
|       49 | 3965 | `	return 0;` |
|      359 | 3966 | `}` |
|        - | 3967 | `/*` |
|        - | 3968 | ` * Extract a class atrribute value.` |
|        - | 3969 | ` * Return a pointer to the attribute value on success. Otherwise NULL.` |
|        - | 3970 | ` * Note:` |
|        - | 3971 | ` *  Access to static and constant attribute is not allowed. That is,the function` |
|        - | 3972 | ` *  will return NULL in case someone (host-application code) try to extract` |
|        - | 3973 | ` *  a static/constant attribute.` |
|        - | 3974 | ` */` |
|  2034102 | 3975 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceFetchAttr(ph7_class_instance *pThis,const SyString *pName)` |
|        5 | 3976 | `{` |
|        - | 3977 | `	SyHashEntry *pEntry;` |
|        - | 3978 | `	VmClassAttr *pAttr;` |
|        - | 3979 | `	/* Query the attribute hashtable */` |
|  2034107 | 3980 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);` |
|  2034107 | 3981 | `	if( pEntry == 0 ){` |
|        - | 3982 | `		/* No such attribute */` |
|     1103 | 3983 | `		return 0;` |
|        - | 3984 | `	}` |
|        - | 3985 | `	/* Point to the class atrribute */` |
|  2033009 | 3986 | `	pAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - | 3987 | `	/* Check if we are dealing with a static/constant attribute */` |
|  2033009 | 3988 | `	if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - | 3989 | `		/* Access is forbidden */` |
|      ! 0 | 3990 | `		return 0;` |
|        - | 3991 | `	}` |
|        - | 3992 | `	/* Return the attribute value */` |
|  2033009 | 3993 | `	return ExtractClassAttrValue(pThis->pVm,pAttr);` |
|  1016218 | 3994 | `}` |
|        - | 3995 | `/*` |
|        - | 3996 | `` * Does a WRITE through `$obj[k]` land on this class's storage? php answers with`` |
|        - | 3997 | ` * the class's read_dimension handler: an internal class whose own handler hands` |
|        - | 3998 | ` * back the real element supports indirect modification, while everything routed` |
|        - | 3999 | ` * through zend_std_read_dimension — every userland ArrayAccess, and the SPL` |
|        - | 4000 | ` * classes that keep the standard handler (SplFixedArray, the SplDoublyLinkedList` |
|        - | 4001 | ` * family, SplObjectStorage) — gets a TEMPORARY back, so the write is lost and php` |
|        - | 4002 | ` * says so. The engine cannot tell those apart from the interface alone: both` |
|        - | 4003 | ` * implement ArrayAccess.` |
|        - | 4004 | ` *` |
|        - | 4005 | ` * PHL's equivalent evidence is the offsetGet that ANSWERS: the native one on a` |
|        - | 4006 | ` * class carrying PH7_CLASS_DIM_WRITABLE means the storage is reachable, and a` |
|        - | 4007 | ` * userland OVERRIDE of it means it is not — which is php's own rule for an` |
|        - | 4008 | ` * ArrayObject subclass (spl_array_read_dimension steps aside for a declared` |
|        - | 4009 | `` * offsetGet). A `&offsetGet` return is php's other yes: it hands back a reference,`` |
|        - | 4010 | ` * so the write reaches whatever it aliases.` |
|        - | 4011 | ` */` |
|      536 | 4012 | `PH7_PRIVATE int PH7_VmDimFetchWritable(ph7_class *pClass)` |
|        5 | 4013 | `{` |
|        - | 4014 | `	ph7_class_method *pGet;` |
|        - | 4015 | `	ph7_class *pCur;` |
|      541 | 4016 | `	if( pClass == 0 ){` |
|      ! 0 | 4017 | `		return FALSE;` |
|        - | 4018 | `	}` |
|      541 | 4019 | `	pGet = PH7_ClassExtractMethod(pClass,"offsetGet",sizeof("offsetGet")-1);` |
|      541 | 4020 | `	if( pGet == 0 ){` |
|      ! 0 | 4021 | `		return FALSE;` |
|        - | 4022 | `	}` |
|      541 | 4023 | `	if( pGet->sFunc.iFlags & VM_FUNC_REF_RETURN ){` |
|        5 | 4024 | `		return TRUE;` |
|        - | 4025 | `	}` |
|      537 | 4026 | `	if( (pGet->sFunc.iFlags & VM_FUNC_NATIVE) == 0 ){` |
|      161 | 4027 | `		return FALSE;` |
|        - | 4028 | `	}` |
|      664 | 4029 | `	for( pCur = pClass ; pCur ; pCur = pCur->pBase ){` |
|      534 | 4030 | `		if( pCur->iFlags & PH7_CLASS_DIM_WRITABLE ){` |
|      250 | 4031 | `			return TRUE;` |
|        - | 4032 | `		}` |
|      144 | 4033 | `	}` |
|      132 | 4034 | `	return FALSE;` |
|      273 | 4035 | `}` |
|        - | 4036 | `/*` |
|        - | 4037 | ` * The three accessors a VM_FUNC_NATIVE method body uses to reach its receiver.` |
|        - | 4038 | ` *` |
|        - | 4039 | ` * A native method is dispatched down the host-function path, so it is handed the` |
|        - | 4040 | ` * plain (ph7_context*, argc, argv) of any builtin — the receiver rides on the` |
|        - | 4041 | ` * context instead of occupying an argument slot. That is the whole point: the C` |
|        - | 4042 | `` * routine that used to be a global `__prefix_verb($target, ...)` thunk taking its`` |
|        - | 4043 | ` * target explicitly becomes a method whose target is $this.` |
|        - | 4044 | ` */` |
|        - | 4045 | `/*` |
|        - | 4046 | ` * The receiver, or NULL when the method was called statically (and always NULL in` |
|        - | 4047 | ` * a plain host function). Borrowed: the operand stack holds the reference for the` |
|        - | 4048 | ` * duration of the call, so the body must not unref it.` |
|        - | 4049 | ` */` |
|  1594644 | 4050 | `PH7_PRIVATE ph7_class_instance * PH7_ContextThis(ph7_context *pCtx)` |
|        5 | 4051 | `{` |
|  1594649 | 4052 | `	return pCtx->pThis;` |
|        5 | 4053 | `}` |
|        - | 4054 | `/*` |
|        - | 4055 | ` * The class the call was made THROUGH — php's late-static-binding target, so an` |
|        - | 4056 | ` * inherited native method sees the SUBCLASS here, not the class that declared it.` |
|        - | 4057 | ` * NULL in a plain host function.` |
|        - | 4058 | ` */` |
|     1059 | 4059 | `PH7_PRIVATE ph7_class * PH7_ContextCalledClass(ph7_context *pCtx)` |
|        4 | 4060 | `{` |
|     1063 | 4061 | `	return pCtx->pCalledClass;` |
|        4 | 4062 | `}` |
|        - | 4063 | `/*` |
|        - | 4064 | ` * The receiver as a ph7_value, so the body can use the ordinary object helpers` |
|        - | 4065 | ` * (ph7_object_fetch_attr, ph7_value_is_object, ph7_result_value, ...) instead of` |
|        - | 4066 | ` * reaching into ph7_class_instance. NULL when there is no receiver.` |
|        - | 4067 | ` *` |
|        - | 4068 | ` * The view ALIASES the instance without bumping its refcount: it lives exactly as` |
|        - | 4069 | ` * long as the call, during which the caller's reference is already keeping the` |
|        - | 4070 | ` * object alive, and VmReleaseCallContext drops the alias without unreffing. Handing` |
|        - | 4071 | ` * it to ph7_result_value() is safe — that copies through PH7_MemObjStore, which` |
|        - | 4072 | ` * takes its own reference.` |
|        - | 4073 | ` */` |
|     8522 | 4074 | `PH7_PRIVATE ph7_value * PH7_ContextThisValue(ph7_context *pCtx)` |
|        5 | 4075 | `{` |
|     8527 | 4076 | `	if( pCtx->pThis == 0 ){` |
|      ! 0 | 4077 | `		return 0;` |
|        - | 4078 | `	}` |
|     8527 | 4079 | `	if( !pCtx->bThisInit ){` |
|     8527 | 4080 | `		PH7_MemObjInit(pCtx->pVm,&pCtx->sThis);` |
|     8527 | 4081 | `		pCtx->sThis.x.pOther = pCtx->pThis;` |
|     8527 | 4082 | `		pCtx->sThis.iFlags = MEMOBJ_OBJ;` |
|     8527 | 4083 | `		pCtx->sThis.nIdx = SXU32_HIGH;` |
|     8527 | 4084 | `		pCtx->bThisInit = 1;` |
|     4261 | 4085 | `	}` |
|     8527 | 4086 | `	return &pCtx->sThis;` |
|     4266 | 4087 | `}` |
|        - | 4088 |  |
