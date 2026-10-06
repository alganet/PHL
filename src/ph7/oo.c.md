# src/ph7/oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2114/2357 lines (89.69%)

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
|  2093309 |   14 | `PH7_PRIVATE ph7_class * PH7_NewRawClass(ph7_vm *pVm,const SyString *pName,sxu32 nLine)` |
|        5 |   15 | `{` |
|        - |   16 | `	ph7_class *pClass;` |
|        - |   17 | `	char *zName;` |
|        - |   18 | `	/* Allocate a new instance */` |
|  2093314 |   19 | `	pClass = (ph7_class *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class));` |
|  2093314 |   20 | `	if( pClass == 0 ){` |
|      ! 0 |   21 | `		return 0;` |
|        - |   22 | `	}` |
|        - |   23 | `	/* Zero the structure */` |
|  2093314 |   24 | `	SyZero(pClass,sizeof(ph7_class));` |
|        - |   25 | `	/* Duplicate class name */` |
|  2093314 |   26 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  2093314 |   27 | `	if( zName == 0 ){` |
|      ! 0 |   28 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClass);` |
|      ! 0 |   29 | `		return 0;` |
|        - |   30 | `	}` |
|        - |   31 | `	/* Initialize fields */` |
|  2093314 |   32 | `	SyStringInitFromBuf(&pClass->sName,zName,pName->nByte);` |
|        - |   33 | `	/* The DISPLAY name: sName up to its first NUL. Only an anonymous class has one` |
|        - |   34 | ``	 * (PH7_CompileAnnonClass synthesizes php's `<prefix>@anonymous\0file:line$hex`),`` |
|        - |   35 | `	 * so for every other class this aliases the whole name and costs one scan of it` |
|        - |   36 | `	 * at declaration time. */` |
|        - |   37 | `	{` |
|  2093314 |   38 | `		sxu32 nCut = pName->nByte;` |
|  2093314 |   39 | `		if( pName->nByte > 0 ){` |
|  2093314 |   40 | `			sxu32 nPos = 0;` |
|  2093314 |   41 | `			if( SyByteFind(zName,pName->nByte,0,&nPos) == SXRET_OK ){` |
|      283 |   42 | `				nCut = nPos;` |
|      139 |   43 | `			}` |
|  1045296 |   44 | `		}` |
|  2093314 |   45 | `		SyStringInitFromBuf(&pClass->sDisp,zName,nCut);` |
|        - |   46 | `	}` |
|        - |   47 | `	/* php method names are CASE-INSENSITIVE ($o->FOO() finds foo(), and declaring` |
|        - |   48 | `	 * both is a redeclaration), so the method table matches on them the same way` |
|        - |   49 | `	 * hClass does for class names. Properties and class constants ARE case` |
|        - |   50 | `	 * sensitive in php, so hAttr keeps the default exact comparator. */` |
|  2093314 |   51 | `	SyHashInit(&pClass->hMethod,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|  2093314 |   52 | `	SyHashInit(&pClass->hAttr,&pVm->sAllocator,0,0);` |
|  2093314 |   53 | `	SyHashInit(&pClass->hConst,&pVm->sAllocator,0,0);` |
|  2093314 |   54 | `	SyHashInit(&pClass->hDerived,&pVm->sAllocator,0,0);` |
|  2093314 |   55 | `	SySetInit(&pClass->aInterface,&pVm->sAllocator,sizeof(ph7_class *));` |
|  2093314 |   56 | `	SySetInit(&pClass->aTrait,&pVm->sAllocator,sizeof(ph7_class *));` |
|  2093314 |   57 | `	SySetInit(&pClass->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|  2093314 |   58 | `	SySetInit(&pClass->aEnumCases,&pVm->sAllocator,sizeof(ph7_class_attr *));` |
|  2093314 |   59 | `	pClass->nLine = nLine;` |
|  2093314 |   60 | `	if( pVm->bCompilingBuiltin ){` |
|        - |   61 | `		/* Defined by an embedded builtin chunk: internal, no defining file.` |
|        - |   62 | `		 * Class compilers merge further flags with \|= so this survives. */` |
|  2085920 |   63 | `		pClass->iFlags \|= PH7_CLASS_INTERNAL;` |
|  1041604 |   64 | `	}else{` |
|        - |   65 | `		/* Alias the VM-lifetime path dup on top of the include stack */` |
|     7399 |   66 | `		SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     7399 |   67 | `		if( pFile ){` |
|     7399 |   68 | `			SyStringDupPtr(&pClass->sFile,pFile);` |
|     3697 |   69 | `		}` |
|        - |   70 | `	}` |
|        - |   71 | `	/* All done */` |
|  2093314 |   72 | `	return pClass;` |
|  1045301 |   73 | `}` |
|        - |   74 | `/*` |
|        - |   75 | ` * Allocate and initialize a new class attribute.` |
|        - |   76 | ` * Return a pointer to the class attribute on success. NULL otherwise.` |
|        - |   77 | ` */` |
|  7943014 |   78 | `PH7_PRIVATE ph7_class_attr * PH7_NewClassAttr(ph7_vm *pVm,const SyString *pName,sxu32 nLine,sxi32 iProtection,sxi32 iFlags)` |
|        5 |   79 | `{` |
|        - |   80 | `	ph7_class_attr *pAttr;` |
|        - |   81 | `	char *zName;` |
|  7943019 |   82 | `	pAttr = (ph7_class_attr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_attr));` |
|  7943019 |   83 | `	if( pAttr == 0 ){` |
|      ! 0 |   84 | `		return 0;` |
|        - |   85 | `	}` |
|        - |   86 | `	/* Zero the structure */` |
|  7943019 |   87 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|  7943019 |   88 | `	SySetInit(&pAttr->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|        - |   89 | `	/* Duplicate attribute name */` |
|  7943019 |   90 | `	zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  7943019 |   91 | `	if( zName == 0 ){` |
|      ! 0 |   92 | `		SyMemBackendPoolFree(&pVm->sAllocator,pAttr);` |
|      ! 0 |   93 | `		return 0;` |
|        - |   94 | `	}` |
|        - |   95 | `	/* Initialize fields */` |
|  7943019 |   96 | `	SySetInit(&pAttr->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|  7943019 |   97 | `	SySetInit(&pAttr->aUnionAlts,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|  7943019 |   98 | `	SyStringInitFromBuf(&pAttr->sName,zName,pName->nByte);` |
|  7943019 |   99 | `	pAttr->iProtection = iProtection;` |
|  7943019 |  100 | `	pAttr->nIdx = SXU32_HIGH;` |
|  7943019 |  101 | `	pAttr->iFlags = iFlags;` |
|  7943019 |  102 | `	pAttr->nLine = nLine;` |
|  7943019 |  103 | `	return pAttr;` |
|  3966342 |  104 | `}` |
|        - |  105 | `/*` |
|        - |  106 | ` * Allocate and initialize a new class method.` |
|        - |  107 | ` * Return a pointer to the class method on success. NULL otherwise` |
|        - |  108 | ` * This function associate with the newly created method an automatically generated` |
|        - |  109 | ` * random unique name.` |
|        - |  110 | ` */` |
| 13366686 |  111 | `PH7_PRIVATE ph7_class_method * PH7_NewClassMethod(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,sxu32 nLine,` |
|        - |  112 | `	sxi32 iProtection,sxi32 iFlags,sxi32 iFuncFlags)` |
|        5 |  113 | `{` |
|        - |  114 | `	ph7_class_method *pMeth;` |
|        - |  115 | `	SyHashEntry *pEntry;` |
|        - |  116 | `	SyString *pNamePtr;` |
|        - |  117 | `	char zSalt[10];` |
|        - |  118 | `	char *zName;` |
|        - |  119 | `	sxu32 nByte;` |
|        - |  120 | `	/* Allocate a new class method instance */` |
| 13366691 |  121 | `	pMeth = (ph7_class_method *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_method));` |
| 13366691 |  122 | `	if( pMeth == 0 ){` |
|      ! 0 |  123 | `		return 0;` |
|        - |  124 | `	}` |
|        - |  125 | `	/* Zero the structure */` |
| 13366691 |  126 | `	SyZero(pMeth,sizeof(ph7_class_method));` |
|        - |  127 | `	/* Check for an already installed method with the same name */` |
| 13366691 |  128 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
| 13366691 |  129 | `	if( pEntry == 0 ){` |
|        - |  130 | `		/* Associate an unique VM name to this method */` |
| 13366681 |  131 | `		nByte = sizeof(zSalt) + pName->nByte + SyStringLength(&pClass->sName)+sizeof(char)*7/*[[__'\0'*/;` |
| 13366681 |  132 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,nByte);` |
| 13366681 |  133 | `		if( zName == 0 ){` |
|      ! 0 |  134 | `			SyMemBackendPoolFree(&pVm->sAllocator,pMeth);` |
|      ! 0 |  135 | `			return 0;` |
|        - |  136 | `		}` |
| 13366681 |  137 | `		pNamePtr = &pMeth->sVmName;` |
|        - |  138 | `		/* Generate a random string */` |
| 13366681 |  139 | `		PH7_VmRandomString(&(*pVm),zSalt,sizeof(zSalt));` |
| 13366681 |  140 | `		pNamePtr->nByte = SyBufferFormat(zName,nByte,"[__%z@%z_%.*s]",&pClass->sName,pName,sizeof(zSalt),zSalt);` |
| 13366681 |  141 | `		pNamePtr->zString = zName;` |
|  6674642 |  142 | `	}else{` |
|        - |  143 | `		/* Method is condidate for 'overloading' */` |
|       12 |  144 | `		ph7_class_method *pCurrent = (ph7_class_method *)pEntry->pUserData;` |
|       12 |  145 | `		pNamePtr = &pMeth->sVmName;` |
|        - |  146 | `		/* Use the same VM name */` |
|       12 |  147 | `		SyStringDupPtr(pNamePtr,&pCurrent->sVmName);` |
|       12 |  148 | `		zName = (char *)pNamePtr->zString;` |
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
| 13366691 |  162 | `	pMeth->iProtection = iProtection;` |
| 13366691 |  163 | `	pMeth->iFlags = iFlags;` |
| 13366691 |  164 | `	pMeth->nLine = nLine;` |
| 20041333 |  165 | `	PH7_VmInitFuncState(&(*pVm),&pMeth->sFunc,&zName[sizeof(char)*4/*[__@*/+SyStringLength(&pClass->sName)],` |
| 13366686 |  166 | `		pName->nByte,iFuncFlags\|VM_FUNC_CLASS_METHOD,pClass);` |
| 13366691 |  167 | `	return pMeth;` |
|  6674647 |  168 | `}` |
|        - |  169 | `/*` |
|        - |  170 | ` * Check if the given name have a class method associated with it.` |
|        - |  171 | ` * Return the desired method [i.e: ph7_class_method instance] on success. NULL otherwise.` |
|        - |  172 | ` */` |
|  7153235 |  173 | `PH7_PRIVATE ph7_class_method * PH7_ClassExtractMethod(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|        5 |  174 | `{` |
|        - |  175 | `	SyHashEntry *pEntry;` |
|        - |  176 | `	/* Perform a hash lookup */` |
|  7153240 |  177 | `	pEntry = SyHashGet(&pClass->hMethod,(const void *)zName,nByte);` |
|  7153240 |  178 | `	if( pEntry == 0 ){` |
|        - |  179 | `		/* No such entry */` |
|  1701234 |  180 | `		return 0;` |
|        - |  181 | `	}` |
|        - |  182 | `	/* Point to the desired method */` |
|  5452011 |  183 | `	return (ph7_class_method *)pEntry->pUserData;` |
|  3576218 |  184 | `}` |
|        - |  185 | `/*` |
|        - |  186 | ` * Check if the given name is a class attribute.` |
|        - |  187 | ` * Return the desired attribute [i.e: ph7_class_attr instance] on success.NULL otherwise.` |
|        - |  188 | ` */` |
|   145869 |  189 | `PH7_PRIVATE ph7_class_attr * PH7_ClassExtractAttribute(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|        5 |  190 | `{` |
|        - |  191 | `	SyHashEntry *pEntry;` |
|        - |  192 | `	/* Perform a hash lookup */` |
|   145874 |  193 | `	pEntry = SyHashGet(&pClass->hAttr,(const void *)zName,nByte);` |
|   145874 |  194 | `	if( pEntry == 0 ){` |
|        - |  195 | `		/* No such entry */` |
|     4409 |  196 | `		return 0;` |
|        - |  197 | `	}` |
|        - |  198 | `	/* Point to the desierd method */` |
|   141470 |  199 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|    72940 |  200 | `}` |
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
|  9732185 |  224 | `PH7_PRIVATE const SyString * PH7_ClassAttrStorageName(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        5 |  225 | `{` |
|        - |  226 | `	ph7_class *pDecl;` |
|        - |  227 | `	sxu32 nCls,nName;` |
|        - |  228 | `	char *zKey;` |
|  9732185 |  229 | `	if( pAttr->iProtection != PH7_CLASS_PROT_PRIVATE` |
|  6183862 |  230 | `	 \|\| (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_DYNAMIC)) != 0 ){` |
|  7087455 |  231 | `		return &pAttr->sName;` |
|        - |  232 | `	}` |
|  2644740 |  233 | `	pDecl = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|  2644740 |  234 | `	if( pDecl == 0 \|\| pDecl == pClass ){` |
|       23 |  235 | `		return &pAttr->sName;` |
|        - |  236 | `	}` |
|  2644720 |  237 | `	if( pDecl->iFlags & PH7_CLASS_INTERNAL ){` |
|        - |  238 | `		/* An ENGINE class's slot keeps its plain name on every object below it:` |
|        - |  239 | `		 * the C bodies that own that storage address it by name (a DateTime's` |
|        - |  240 | `		 * timestamp, a PDOStatement's handle), and a subclass instance whose slots` |
|        - |  241 | `		 * were renamed read as an object whose parent constructor never ran. php` |
|        - |  242 | `		 * mangles an internal private too; nothing here can see the difference,` |
|        - |  243 | `		 * because an engine class's private is not a name user code declares. */` |
|  2644562 |  244 | `		return &pAttr->sName;` |
|        - |  245 | `	}` |
|      162 |  246 | `	if( SyStringLength(&pAttr->sStoreName) > 0 ){` |
|      108 |  247 | `		return &pAttr->sStoreName;` |
|        - |  248 | `	}` |
|       58 |  249 | `	nCls = SyStringLength(&pDecl->sName);` |
|       58 |  250 | `	nName = SyStringLength(&pAttr->sName);` |
|        - |  251 | `	/* Class-lifetime, like sName's own dup: an attribute outlives every instance` |
|        - |  252 | `	 * whose table points at this key. */` |
|       58 |  253 | `	zKey = (char *)SyMemBackendAlloc(&pVm->sAllocator,nCls + nName + 3);` |
|       58 |  254 | `	if( zKey == 0 ){` |
|      ! 0 |  255 | `		return &pAttr->sName;` |
|        - |  256 | `	}` |
|       58 |  257 | `	zKey[0] = 0;` |
|       58 |  258 | `	SyMemcpy((const void *)SyStringData(&pDecl->sName),(void *)&zKey[1],nCls);` |
|       58 |  259 | `	zKey[1+nCls] = 0;` |
|       58 |  260 | `	SyMemcpy((const void *)SyStringData(&pAttr->sName),(void *)&zKey[nCls+2],nName);` |
|       58 |  261 | `	zKey[nCls+nName+2] = 0;` |
|       58 |  262 | `	SyStringInitFromBuf(&pAttr->sStoreName,zKey,nCls + nName + 2);` |
|       58 |  263 | `	return &pAttr->sStoreName;` |
|  4859767 |  264 | `}` |
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
|  1233592 |  283 | `static sxu64 OoShadowNameBit(sxu32 nHash)` |
|        5 |  284 | `{` |
|  1233597 |  285 | `	return ((sxu64)1) << (nHash & 63);` |
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
|  4344670 |  296 | `PH7_PRIVATE void PH7_ClassNotePrivateName(ph7_class *pClass,ph7_class_attr *pAttr)` |
|        5 |  297 | `{` |
|  4344670 |  298 | `	if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|  2787002 |  299 | `	 && (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|  1849224 |  300 | `		pClass->nPrivName \|= OoShadowNameBit(SyHashKey(&pClass->hAttr,` |
|  1233348 |  301 | `			(const void *)SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName)));` |
|   615871 |  302 | `	}` |
|  4344675 |  303 | `}` |
|        - |  304 | `/*` |
|        - |  305 | `` * Which PROPERTY does `name` mean, seen from the class whose code is RUNNING?`` |
|        - |  306 | ` *` |
|        - |  307 | ` * php's zend_get_parent_private_property: a scope that declares a private of this` |
|        - |  308 | ` * name owns a slot of its own on every instance below it, and that slot -- not` |
|        - |  309 | ` * whatever the object's class holds under the plain name -- is what its code` |
|        - |  310 | ` * means. Answers 0 when the executing scope has no such private, which leaves the` |
|        - |  311 | ` * caller on the ordinary plain-name path.` |
|        - |  312 | ` */` |
|   156608 |  313 | `static ph7_class_attr * OoScopePrivateAttr(ph7_vm *pVm,ph7_class *pClass,const char *zName,` |
|        - |  314 | `	sxu32 nName,sxu32 nHash)` |
|        5 |  315 | `{` |
|        - |  316 | `	ph7_class *pScope;` |
|        - |  317 | `	SyHashEntry *pEntry;` |
|        - |  318 | `	ph7_class_attr *pOwn;` |
|        - |  319 | `	sxu64 nBit;` |
|   156613 |  320 | `	if( nName < 1 ){` |
|       65 |  321 | `		return 0;` |
|        - |  322 | `	}` |
|   156549 |  323 | `	if( (pClass->iFlags & PH7_CLASS_SHADOW_PROP) == 0 ){` |
|        - |  324 | `		/* No property of this class is filed under a mangled name, so it holds no` |
|        - |  325 | `		 * slot the plain probe cannot reach. Reaching one at all takes a scope this` |
|        - |  326 | `		 * class DESCENDS from, and inheriting that scope's private is exactly what` |
|        - |  327 | `		 * sets the flag -- so this is the whole test, and every ordinary property` |
|        - |  328 | `		 * access skips the frame walk below on it. */` |
|   156371 |  329 | `		return 0;` |
|        - |  330 | `	}` |
|      181 |  331 | `	nBit = OoShadowNameBit(nHash);` |
|      181 |  332 | `	if( (pClass->nShadowName & nBit) == 0 ){` |
|        - |  333 | `		/* ...and the flag alone is not enough. A class that inherits ONE private` |
|        - |  334 | `		 * property pays for the walk below on every access to every OTHER property` |
|        - |  335 | `		 * it has, and on the phpcs step that was 79.4 million hash lookups made to` |
|        - |  336 | `		 * answer "no" -- 99.4% of the ones this function made, and 14% of every` |
|        - |  337 | `		 * lookup the engine did. The mask names the plain names a mangled slot` |
|        - |  338 | `		 * could be hiding under, so a miss is the complete answer: nothing can be` |
|        - |  339 | `		 * reached under a name this class holds no mangled slot for. */` |
|       24 |  340 | `		return 0;` |
|        - |  341 | `	}` |
|      159 |  342 | `	pScope = PH7_VmCallerScope(&(*pVm));` |
|      159 |  343 | `	if( pScope == 0 \|\| pScope == pClass ){` |
|       69 |  344 | `		return 0;   /* global scope, or the object's own class: the plain name IS the slot */` |
|        - |  345 | `	}` |
|       92 |  346 | `	if( (pScope->nPrivName & nBit) == 0 ){` |
|        - |  347 | `		/* ...and the same question from the other side, which is the one that` |
|        - |  348 | `		 * decides: the scope can only mean a mangled slot for a name it declares` |
|        - |  349 | `		 * PRIVATE itself. A class inherits many more mangled names than it` |
|        - |  350 | `		 * declares private ones, so this mask is the sparser of the two, and the` |
|        - |  351 | `		 * pair of them is what leaves this lookup to the accesses that need it. */` |
|      ! 0 |  352 | `		return 0;` |
|        - |  353 | `	}` |
|       92 |  354 | `	pEntry = SyHashGetHashed(&pScope->hAttr,(const void *)zName,nName,nHash);` |
|       92 |  355 | `	pOwn = pEntry ? (ph7_class_attr *)pEntry->pUserData : 0;` |
|       90 |  356 | `	if( pOwn == 0` |
|       90 |  357 | `	 \|\| pOwn->iProtection != PH7_CLASS_PROT_PRIVATE` |
|       89 |  358 | `	 \|\| (pOwn->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) != 0` |
|       88 |  359 | `	 \|\| PH7_VmMemberOwnerClass(pOwn->pDeclClass,pScope) != pScope` |
|       90 |  360 | `	 \|\| !PH7_VmInstanceOf(pClass,pScope) ){` |
|        3 |  361 | `		return 0;` |
|        - |  362 | `	}` |
|       90 |  363 | `	return pOwn;` |
|    78307 |  364 | `}` |
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
|     1044 |  378 | `PH7_PRIVATE int PH7_ClassInstanceAttrShadowed(ph7_vm *pVm,ph7_class_instance *pThis,SyHashEntry *pEntry)` |
|        5 |  379 | `{` |
|     1049 |  380 | `	VmClassAttr *pMe = (VmClassAttr *)pEntry->pUserData;` |
|        - |  381 | `	SyString *pName;` |
|        - |  382 | `	SyHashEntry *pWalk;` |
|     1049 |  383 | `	if( (pThis->pClass->iFlags & PH7_CLASS_SHADOW_PROP) == 0 \|\| pMe == 0 ){` |
|     1031 |  384 | `		return 0;   /* no mangled slot on this class: no name can collide */` |
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
|      527 |  404 | `}` |
|        - |  405 | `/*` |
|        - |  406 | ` * PH7_ClassExtractAttribute, told which scope is asking: the executing class's own` |
|        - |  407 | ` * private wins over the same name declared further down the chain.` |
|        - |  408 | ` */` |
|     1588 |  409 | `PH7_PRIVATE ph7_class_attr * PH7_ClassScopedAttribute(ph7_vm *pVm,ph7_class *pClass,const char *zName,sxu32 nName)` |
|        5 |  410 | `{` |
|     3171 |  411 | `	ph7_class_attr *pOwn = OoScopePrivateAttr(&(*pVm),pClass,zName,nName,` |
|     1578 |  412 | `		nName > 0 ? SyHashKey(&pClass->hAttr,(const void *)zName,nName) : 0);` |
|     1593 |  413 | `	if( pOwn ){` |
|        3 |  414 | `		return pOwn;` |
|        - |  415 | `	}` |
|     1591 |  416 | `	return PH7_ClassExtractAttribute(pClass,zName,nName);` |
|      799 |  417 | `}` |
|        - |  418 | `/*` |
|        - |  419 | ` * PH7_ClassInstanceAttrEntry, told which scope is asking. When the executing class` |
|        - |  420 | ` * declares a private of this name, its MANGLED slot is the only one it can mean --` |
|        - |  421 | ` * so a miss there is a miss, and never falls back to the plain name (php's fetch` |
|        - |  422 | `` * stops at the property_info it resolved; an `unset()` of that slot reads as`` |
|        - |  423 | ` * undefined even when a public property of the same name sits beside it).` |
|        - |  424 | ` */` |
|   155020 |  425 | `PH7_PRIVATE SyHashEntry * PH7_ClassInstanceScopedAttrEntry(ph7_vm *pVm,ph7_class_instance *pThis,` |
|        - |  426 | `	const char *zName,sxu32 nName,sxu32 nHash)` |
|        5 |  427 | `{` |
|   155025 |  428 | `	ph7_class_attr *pOwn = OoScopePrivateAttr(&(*pVm),pThis->pClass,zName,nName,nHash);` |
|   155025 |  429 | `	if( pOwn ){` |
|        - |  430 | `		/* The MANGLED key is a different string, so the caller's hash says nothing` |
|        - |  431 | `		 * about it -- this is the 0.6% of accesses that really do mean a scope's` |
|        - |  432 | `		 * private, and they hash their own key. */` |
|       88 |  433 | `		const SyString *pKey = PH7_ClassAttrStorageName(&(*pVm),pThis->pClass,pOwn);` |
|       88 |  434 | `		return SyHashGet(&pThis->hAttr,(const void *)SyStringData(pKey),SyStringLength(pKey));` |
|        - |  435 | `	}` |
|   154939 |  436 | `	if( nName > 0 ){` |
|   154895 |  437 | `		return SyHashGetHashed(&pThis->hAttr,(const void *)zName,nName,nHash);` |
|        - |  438 | `	}` |
|       45 |  439 | `	return PH7_ClassInstanceAttrEntry(pThis,zName,nName);` |
|    77513 |  440 | `}` |
|        - |  441 | `/*` |
|        - |  442 | ` * Check if the given name is a class CONSTANT (or enum case).` |
|        - |  443 | ` * php keeps constants and properties in separate namespaces, so constants live` |
|        - |  444 | ` * in a dedicated table (hConst) and never collide with a same-named property.` |
|        - |  445 | ` * Return the desired constant [ph7_class_attr with PH7_CLASS_ATTR_CONSTANT] on` |
|        - |  446 | ` * success, NULL otherwise.` |
|        - |  447 | ` */` |
|     3748 |  448 | `PH7_PRIVATE ph7_class_attr * PH7_ClassExtractConstant(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|        5 |  449 | `{` |
|        - |  450 | `	SyHashEntry *pEntry;` |
|     3753 |  451 | `	pEntry = SyHashGet(&pClass->hConst,(const void *)zName,nByte);` |
|     3753 |  452 | `	if( pEntry == 0 ){` |
|      751 |  453 | `		return 0;` |
|        - |  454 | `	}` |
|     3007 |  455 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|     1879 |  456 | `}` |
|        - |  457 | `/*` |
|        - |  458 | ` * Install a class attribute in the corresponding container.` |
|        - |  459 | ` * A constant (or enum case) goes to hConst, a property to hAttr — php's two` |
|        - |  460 | `` * separate member namespaces, so `const C` and `public $C` coexist.`` |
|        - |  461 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|        - |  462 | ` */` |
|  7943008 |  463 | `PH7_PRIVATE sxi32 PH7_ClassInstallAttr(ph7_class *pClass,ph7_class_attr *pAttr)` |
|        5 |  464 | `{` |
|  7943013 |  465 | `	SyString *pName = &pAttr->sName;` |
|        - |  466 | `	sxi32 rc;` |
|        - |  467 | `	/* Remember where this attribute was originally declared so that later` |
|        - |  468 | `	 * inheritance/trait copies still know the declaring class (needed for` |
|        - |  469 | `	 * PHP-compatible error messages on typed properties). */` |
|  7943013 |  470 | `	if( pAttr->pDeclClass == 0 ){` |
|    63828 |  471 | `		pAttr->pDeclClass = pClass;` |
|    31873 |  472 | `	}` |
|  7943013 |  473 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|  3598459 |  474 | `		rc = SyHashInsertTail(&pClass->hConst,(const void *)pName->zString,pName->nByte,pAttr);` |
|  1796889 |  475 | `	}else{` |
|  4344559 |  476 | `		rc = SyHashInsertTail(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|  4344559 |  477 | `		PH7_ClassNotePrivateName(pClass,pAttr);` |
|        - |  478 | `	}` |
|  7943013 |  479 | `	return rc;` |
|        5 |  480 | `}` |
|        - |  481 | `/*` |
|        - |  482 | ` * Install a class method in the corresponding container.` |
|        - |  483 | ` * Return SXRET_OK on success. Any other return value indicates failure.` |
|        - |  484 | ` */` |
| 13366616 |  485 | `PH7_PRIVATE sxi32 PH7_ClassInstallMethod(ph7_class *pClass,ph7_class_method *pMeth)` |
|        5 |  486 | `{` |
| 13366621 |  487 | `	SyString *pName = &pMeth->sFunc.sName;` |
|        - |  488 | `	sxi32 rc;` |
| 13366621 |  489 | `	rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 13366621 |  490 | `	return rc;` |
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
|      734 |  508 | `static int OoDeclNameChar(int c)` |
|        4 |  509 | `{` |
|     1462 |  510 | `	return !(c == '\|' \|\| c == '&' \|\| c == '?' \|\| c == '(' \|\| c == ')'` |
|      724 |  511 | `		\|\| c == ' ' \|\| c == '\t');` |
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
|      110 |  536 | `static void OoDeclType(ph7_class *pScope,const SyString *pDeclared,SyBlob *pOut)` |
|        4 |  537 | `{` |
|      114 |  538 | `	const char *z = pDeclared ? SyStringData(pDeclared) : 0;` |
|      114 |  539 | `	sxu32 n = z ? SyStringLength(pDeclared) : 0;` |
|      114 |  540 | `	sxu32 i = 0;` |
|      114 |  541 | `	if( n < 1 ){` |
|      ! 0 |  542 | `		return;` |
|        - |  543 | `	}` |
|      114 |  544 | `	if( n == 8 && SyStrnicmp(z,"iterable",8) == 0 ){` |
|        5 |  545 | `		SyBlobAppend(pOut,"Traversable\|array",sizeof("Traversable\|array")-1);` |
|        5 |  546 | `		return;` |
|        - |  547 | `	}` |
|      110 |  548 | `	if( n == 9 && z[0] == '?' && SyStrnicmp(&z[1],"iterable",8) == 0 ){` |
|        5 |  549 | `		SyBlobAppend(pOut,"Traversable\|array\|null",sizeof("Traversable\|array\|null")-1);` |
|        5 |  550 | `		return;` |
|        - |  551 | `	}` |
|      218 |  552 | `	while( i < n ){` |
|        - |  553 | `		sxu32 nStart;` |
|        - |  554 | `		const SyString *pWrite;` |
|        - |  555 | `		SyString sTok;` |
|      116 |  556 | `		if( !OoDeclNameChar(z[i]) ){` |
|       10 |  557 | `			SyBlobAppend(pOut,&z[i],sizeof(char));` |
|       10 |  558 | `			i++;` |
|       10 |  559 | `			continue;` |
|        - |  560 | `		}` |
|      108 |  561 | `		nStart = i;` |
|      728 |  562 | `		while( i < n && OoDeclNameChar(z[i]) ){` |
|      624 |  563 | `			i++;` |
|        4 |  564 | `		}` |
|      108 |  565 | `		SyStringInitFromBuf(&sTok,&z[nStart],i - nStart);` |
|      108 |  566 | `		pWrite = &sTok;` |
|      108 |  567 | `		if( pScope ){` |
|      104 |  568 | `			if( sTok.nByte == sizeof("self")-1` |
|       67 |  569 | `			 && SyStrnicmp(sTok.zString,"self",sizeof("self")-1) == 0 ){` |
|        9 |  570 | `				pWrite = &pScope->sName;` |
|      101 |  571 | `			}else if( sTok.nByte == sizeof("parent")-1` |
|       62 |  572 | `			 && SyStrnicmp(sTok.zString,"parent",sizeof("parent")-1) == 0` |
|       20 |  573 | `			 && pScope->pBase ){` |
|        5 |  574 | `				pWrite = &pScope->pBase->sName;` |
|        2 |  575 | `			}` |
|       52 |  576 | `		}` |
|      108 |  577 | `		SyBlobAppend(pOut,SyStringData(pWrite),SyStringLength(pWrite));` |
|        4 |  578 | `	}` |
|       59 |  579 | `}` |
|        - |  580 | `/* The instructions of a compiled default, without the OP_DONE the compiler` |
|        - |  581 | ` * terminates every one of them with. */` |
|    43345 |  582 | `static sxu32 OoDeclDefLength(SySet *pByteCode)` |
|        5 |  583 | `{` |
|    43350 |  584 | `	sxu32 n = SySetUsed(pByteCode);` |
|    86695 |  585 | `	while( n > 0 ){` |
|    86695 |  586 | `		VmInstr *pIn = (VmInstr *)SySetAt(pByteCode,n - 1);` |
|    86695 |  587 | `		if( pIn == 0 \|\| (pIn->iOp != PH7_OP_DONE && pIn->iOp != PH7_OP_NOOP) ){` |
|    21649 |  588 | `			break;` |
|        - |  589 | `		}` |
|    43350 |  590 | `		n--;` |
|        5 |  591 | `	}` |
|    43350 |  592 | `	return n;` |
|        5 |  593 | `}` |
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
|    43341 |  605 | `static int OoDeclDefFoldable(SySet *pByteCode,sxu32 nLen)` |
|        5 |  606 | `{` |
|        - |  607 | `	sxu32 n;` |
|    70015 |  608 | `	for( n = 0 ; n < nLen ; ++n ){` |
|    43732 |  609 | `		VmInstr *pIn = (VmInstr *)SySetAt(pByteCode,n);` |
|    43732 |  610 | `		if( pIn == 0 ){` |
|      ! 0 |  611 | `			return 0;` |
|        - |  612 | `		}` |
|    43732 |  613 | `		switch( pIn->iOp ){` |
|    21785 |  614 | `		case PH7_OP_LOADC:` |
|    43518 |  615 | `			if( pIn->iP1 & PH7_LOADC_EXPAND ){` |
|    17011 |  616 | `				return 0; /* a constant NAME -- php keeps it unfolded */` |
|        - |  617 | `			}` |
|    26512 |  618 | `			break;` |
|       81 |  619 | `		case PH7_OP_LOAD_MAP: case PH7_OP_LOAD_IDX:` |
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
|      167 |  633 | `			break;` |
|       26 |  634 | `		default:` |
|       55 |  635 | `			return 0;` |
|        - |  636 | `		}` |
|    13322 |  637 | `	}` |
|    26288 |  638 | `	return nLen > 0;` |
|    21647 |  639 | `}` |
|        - |  640 | `/*` |
|        - |  641 | ` * The value a compiled default FOLDS to, or FALSE where php's compiler would have` |
|        - |  642 | ` * kept it an expression (a constant name, a class constant, anything that throws` |
|        - |  643 | ` * or warns). *pOut is the caller's, initialized, and released by it.` |
|        - |  644 | ` */` |
|    43323 |  645 | `PH7_PRIVATE int PH7_ClassFoldDefault(ph7_vm *pVm,SySet *pByteCode,ph7_value *pOut)` |
|        5 |  646 | `{` |
|    56444 |  647 | `	return OoDeclDefFoldable(pByteCode,OoDeclDefLength(pByteCode))` |
|    43323 |  648 | `		&& PH7_VmEvalConstExpr(pVm,pByteCode,pOut);` |
|        5 |  649 | `}` |
|        - |  650 | `/* The literal a LOADC pushes, or 0 when the operand is not a string one. */` |
|        6 |  651 | `static const SyString * OoDeclLiteral(ph7_vm *pVm,VmInstr *pIn,SyString *pOut)` |
|        1 |  652 | `{` |
|        - |  653 | `	ph7_value *pLit;` |
|        7 |  654 | `	if( pIn == 0 \|\| pIn->iOp != PH7_OP_LOADC ){` |
|      ! 0 |  655 | `		return 0;` |
|        - |  656 | `	}` |
|        7 |  657 | `	pLit = (ph7_value *)SySetAt(&pVm->aLitObj,(sxu32)pIn->iP2);` |
|        7 |  658 | `	if( pLit == 0 \|\| (pLit->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  659 | `		return 0;` |
|        - |  660 | `	}` |
|        7 |  661 | `	SyStringInitFromBuf(pOut,SyBlobData(&pLit->sBlob),SyBlobLength(&pLit->sBlob));` |
|        7 |  662 | `	return pOut;` |
|        4 |  663 | `}` |
|        - |  664 | `/*` |
|        - |  665 | ` * A FOLDED default value, spelled php's way.` |
|        - |  666 | ` *` |
|        - |  667 | ` * php's own spellings, and they are not the export's: a string is SINGLE-quoted,` |
|        - |  668 | `` * printed RAW (no escaping at all) and TRUNCATED to ten bytes with `...` inside`` |
|        - |  669 | `` * the quotes; `null` is lower-case; an array shows only whether it is empty.`` |
|        - |  670 | ` */` |
|       16 |  671 | `static void OoDeclValue(SyBlob *pOut,ph7_value *pVal)` |
|        2 |  672 | `{` |
|       18 |  673 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|        3 |  674 | `		SyBlobAppend(pOut,"null",sizeof("null")-1);` |
|        3 |  675 | `		return;` |
|        - |  676 | `	}` |
|       15 |  677 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  678 | `		SyBlobAppend(pOut,pVal->x.iVal ? "true" : "false",pVal->x.iVal ? 4 : 5);` |
|      ! 0 |  679 | `		return;` |
|        - |  680 | `	}` |
|       15 |  681 | `	if( pVal->iFlags & MEMOBJ_STRING ){` |
|        7 |  682 | `		sxu32 nStr = SyBlobLength(&pVal->sBlob);` |
|        7 |  683 | `		SyBlobAppend(pOut,"'",sizeof(char));` |
|        7 |  684 | `		if( nStr > 0 ){` |
|        7 |  685 | `			SyBlobAppend(pOut,SyBlobData(&pVal->sBlob),(nStr > 10 ? (sxu32)10 : nStr));` |
|        3 |  686 | `		}` |
|        7 |  687 | `		if( nStr > 10 ){` |
|        3 |  688 | `			SyBlobAppend(pOut,"...",sizeof("...")-1);` |
|        1 |  689 | `		}` |
|        7 |  690 | `		SyBlobAppend(pOut,"'",sizeof(char));` |
|        7 |  691 | `		return;` |
|        - |  692 | `	}` |
|        9 |  693 | `	if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|        5 |  694 | `		ph7_hashmap *pMap = (ph7_hashmap *)pVal->x.pOther;` |
|       11 |  695 | `		SyBlobAppend(pOut,` |
|        4 |  696 | `			(pMap && pMap->nEntry > 0) ? "[...]" : "[]",` |
|        4 |  697 | `			(pMap && pMap->nEntry > 0) ? sizeof("[...]")-1 : sizeof("[]")-1);` |
|        5 |  698 | `		return;` |
|        - |  699 | `	}` |
|        5 |  700 | `	if( pVal->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|        - |  701 | ``		/* php prints the value's own string cast, which is where `1.0` reads `1`,`` |
|        - |  702 | ``		 * `1e100` reads `1.0E+100` and INF reads `INF`. */`` |
|        5 |  703 | `		PH7_MemObjToString(pVal);` |
|        5 |  704 | `		SyBlobAppend(pOut,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));` |
|        5 |  705 | `		return;` |
|        - |  706 | `	}` |
|      ! 0 |  707 | `	SyBlobAppend(pOut,"<expression>",sizeof("<expression>")-1);` |
|       10 |  708 | `}` |
|        - |  709 | `/*` |
|        - |  710 | `` * The text after `= ` in a parameter default.`` |
|        - |  711 | ` *` |
|        - |  712 | ` * php prints what its compiler FOLDED the expression to, with two deliberate` |
|        - |  713 | ` * exceptions it leaves unfolded and prints as source: a lone constant reference` |
|        - |  714 | `` * keeps its NAME (`= M_PI`, `= PHP_INT_MAX`) and a class constant keeps`` |
|        - |  715 | `` * `Class::NAME` as written (`= self::K`, `= MyEnum::Foo`). `X::class` is not`` |
|        - |  716 | `` * one of those -- it folds to the class-name STRING, so it prints `'X'`.`` |
|        - |  717 | `` * Everything it could not reduce is php's `<expression>`.`` |
|        - |  718 | ` */` |
|       22 |  719 | `static void OoDeclDefault(ph7_vm *pVm,ph7_class *pScope,SySet *pByteCode,SyBlob *pOut)` |
|        2 |  720 | `{` |
|       24 |  721 | `	sxu32 nLen = OoDeclDefLength(pByteCode);` |
|        - |  722 | `	SyString sOne, sTwo;` |
|       24 |  723 | `	if( nLen == 1 ){` |
|       14 |  724 | `		VmInstr *pIn = (VmInstr *)SySetAt(pByteCode,0);` |
|       12 |  725 | `		if( pIn && pIn->iOp == PH7_OP_LOADC && (pIn->iP1 & PH7_LOADC_EXPAND)` |
|        8 |  726 | `		 && OoDeclLiteral(pVm,pIn,&sOne) ){` |
|        - |  727 | `			/* A constant NAME, exactly as the source wrote it. */` |
|        3 |  728 | `			SyBlobAppend(pOut,SyStringData(&sOne),SyStringLength(&sOne));` |
|       12 |  729 | `			return;` |
|        - |  730 | `		}` |
|        5 |  731 | `	}` |
|       22 |  732 | `	if( nLen == 3 ){` |
|        9 |  733 | `		VmInstr *pCls = (VmInstr *)SySetAt(pByteCode,0);` |
|        9 |  734 | `		VmInstr *pMem = (VmInstr *)SySetAt(pByteCode,1);` |
|        9 |  735 | `		VmInstr *pOp  = (VmInstr *)SySetAt(pByteCode,2);` |
|        8 |  736 | `		if( pOp && pOp->iOp == PH7_OP_MEMBER && pOp->iP1 == 1` |
|        2 |  737 | `		 && pOp->iP2 == PH7_MEMBER_READ` |
|        3 |  738 | `		 && OoDeclLiteral(pVm,pCls,&sOne) && OoDeclLiteral(pVm,pMem,&sTwo) ){` |
|        2 |  739 | `			if( sTwo.nByte == sizeof("class")-1` |
|        2 |  740 | `			 && SyStrnicmp(sTwo.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  741 | ``				/* `self::class` -- the only ::class spelling the compiler leaves for`` |
|        - |  742 | `				 * the runtime (a named class folds to its own literal, and lands on` |
|        - |  743 | `				 * the value path below). php folded it too, to the STRING. */` |
|        - |  744 | `				SyBlob sName;` |
|      ! 0 |  745 | `				ph7_class *pCurr = 0;` |
|      ! 0 |  746 | `				if( pScope ){` |
|      ! 0 |  747 | `					pCurr = (sOne.nByte == sizeof("parent")-1` |
|      ! 0 |  748 | `						&& SyStrnicmp(sOne.zString,"parent",sizeof("parent")-1) == 0)` |
|      ! 0 |  749 | `						? pScope->pBase : pScope;` |
|      ! 0 |  750 | `				}` |
|      ! 0 |  751 | `				if( pCurr ){` |
|      ! 0 |  752 | `					SyBlobInit(&sName,&pVm->sAllocator);` |
|      ! 0 |  753 | `					SyBlobAppend(&sName,"'",sizeof(char));` |
|      ! 0 |  754 | `					SyBlobAppend(&sName,SyStringData(&pCurr->sName),SyStringLength(&pCurr->sName));` |
|      ! 0 |  755 | `					SyBlobAppend(&sName,"'",sizeof(char));` |
|      ! 0 |  756 | `					SyBlobAppend(pOut,SyBlobData(&sName),SyBlobLength(&sName));` |
|      ! 0 |  757 | `					SyBlobRelease(&sName);` |
|      ! 0 |  758 | `					return;` |
|        - |  759 | `				}` |
|      ! 0 |  760 | `			}else{` |
|        3 |  761 | `				SyBlobAppend(pOut,SyStringData(&sOne),SyStringLength(&sOne));` |
|        3 |  762 | `				SyBlobAppend(pOut,"::",sizeof("::")-1);` |
|        3 |  763 | `				SyBlobAppend(pOut,SyStringData(&sTwo),SyStringLength(&sTwo));` |
|        3 |  764 | `				return;` |
|        - |  765 | `			}` |
|      ! 0 |  766 | `		}` |
|        3 |  767 | `	}` |
|       20 |  768 | `	if( OoDeclDefFoldable(pByteCode,nLen) ){` |
|        - |  769 | `		ph7_value sVal;` |
|        - |  770 | `		int bFolded;` |
|       18 |  771 | `		PH7_MemObjInit(pVm,&sVal);` |
|       18 |  772 | `		bFolded = PH7_VmEvalConstExpr(pVm,pByteCode,&sVal);` |
|       18 |  773 | `		if( bFolded ){` |
|       18 |  774 | `			OoDeclValue(pOut,&sVal);` |
|        8 |  775 | `		}` |
|       18 |  776 | `		PH7_MemObjRelease(&sVal);` |
|       18 |  777 | `		if( bFolded ){` |
|       18 |  778 | `			return;` |
|        - |  779 | `		}` |
|      ! 0 |  780 | `	}` |
|        3 |  781 | `	SyBlobAppend(pOut,"<expression>",sizeof("<expression>")-1);` |
|       13 |  782 | `}` |
|        - |  783 | `/*` |
|        - |  784 | ` * php hands each rendered declaration to its error formatter as a C STRING, so a` |
|        - |  785 | `` * declaration carrying a NUL byte -- `function f($a = "\0")` -- is cut there and`` |
|        - |  786 | `` * the sentence carries on with what follows it (`A::f($a = '` and then ` in ... on`` |
|        - |  787 | `` * line N`). Reproduced rather than left as a whole-blob write, which is the one`` |
|        - |  788 | ` * shape where the two engines would disagree byte for byte.` |
|        - |  789 | ` */` |
|       84 |  790 | `static int OoDeclCLen(SyBlob *pDecl)` |
|        4 |  791 | `{` |
|       88 |  792 | `	const char *z = (const char *)SyBlobData(pDecl);` |
|       88 |  793 | `	sxu32 n = SyBlobLength(pDecl), i;` |
|     1806 |  794 | `	for( i = 0 ; i < n ; ++i ){` |
|     1722 |  795 | `		if( z[i] == 0 ){` |
|      ! 0 |  796 | `			return (int)i;` |
|        - |  797 | `		}` |
|      863 |  798 | `	}` |
|       88 |  799 | `	return (int)n;` |
|       46 |  800 | `}` |
|        - |  801 | `/*` |
|        - |  802 | `` * `(int $a, ?string $b = null): string` -- everything php prints after the`` |
|        - |  803 | ` * method's name. pScope is the class the declaration was written FOR (a trait` |
|        - |  804 | ``  * method's composing class, not the trait), which is what `self` and `parent` `` |
|        - |  805 | ` * resolve against.` |
|        - |  806 | ` */` |
|       84 |  807 | `PH7_PRIVATE void PH7_ClassRenderDecl(ph7_vm *pVm,ph7_class *pScope,ph7_vm_func *pFunc,SyBlob *pOut)` |
|        4 |  808 | `{` |
|       88 |  809 | `	ph7_vm_func_arg *aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       88 |  810 | `	sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|        - |  811 | `	sxu32 i;` |
|       88 |  812 | `	SyBlobAppend(pOut,"(",sizeof(char));` |
|      174 |  813 | `	for( i = 0 ; i < nArg ; ++i ){` |
|       90 |  814 | `		if( i > 0 ){` |
|       43 |  815 | `			SyBlobAppend(pOut,", ",sizeof(", ")-1);` |
|       20 |  816 | `		}` |
|       90 |  817 | `		if( SyStringLength(&aArgs[i].sTypeName) > 0 ){` |
|       58 |  818 | `			OoDeclType(pScope,&aArgs[i].sTypeName,pOut);` |
|       58 |  819 | `			SyBlobAppend(pOut," ",sizeof(char));` |
|       27 |  820 | `		}` |
|       90 |  821 | `		if( aArgs[i].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        3 |  822 | `			SyBlobAppend(pOut,"&",sizeof(char));` |
|        1 |  823 | `		}` |
|       90 |  824 | `		if( aArgs[i].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        3 |  825 | `			SyBlobAppend(pOut,"...",sizeof("...")-1);` |
|        1 |  826 | `		}` |
|       90 |  827 | `		SyBlobAppend(pOut,"$",sizeof(char));` |
|       90 |  828 | `		SyBlobAppend(pOut,SyStringData(&aArgs[i].sName),SyStringLength(&aArgs[i].sName));` |
|       90 |  829 | `		if( SySetUsed(&aArgs[i].aByteCode) > 0 ){` |
|       24 |  830 | `			SyBlobAppend(pOut," = ",sizeof(" = ")-1);` |
|       24 |  831 | `			OoDeclDefault(pVm,pScope,&aArgs[i].aByteCode,pOut);` |
|       11 |  832 | `		}` |
|       47 |  833 | `	}` |
|       88 |  834 | `	SyBlobAppend(pOut,")",sizeof(char));` |
|       88 |  835 | `	if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|       60 |  836 | `		SyBlobAppend(pOut,": ",sizeof(": ")-1);` |
|       60 |  837 | `		OoDeclType(pScope,&pFunc->sReturnTypeName,pOut);` |
|       28 |  838 | `	}` |
|       88 |  839 | `}` |
|        - |  840 | `/*` |
|        - |  841 | ` * ---------------------------------------------------------------------------` |
|        - |  842 | ` * Method-override compatibility: php's declared-type LATTICE.` |
|        - |  843 | ` *` |
|        - |  844 | ` * php rejects an override whose signature is incompatible with the parent's --` |
|        - |  845 | ` * a return type is COVARIANT (the child may only narrow), a parameter type is` |
|        - |  846 | ` * CONTRAVARIANT (the child may only widen), and the arity/by-reference shape` |
|        - |  847 | ` * must let every call the parent accepts reach the child. This used to be a` |
|        - |  848 | ` * deliberately SKIP-BY-DEFAULT approximation: it decided a bare scalar against a` |
|        - |  849 | ` * bare scalar and a loaded class against a loaded class, and accepted everything` |
|        - |  850 | `` * subtle -- a union, an intersection, `mixed`, `object`, `iterable`, `void`,`` |
|        - |  851 | `` * `never`, `self`/`static`, or a variadic signature. Fourteen shapes php refuses`` |
|        - |  852 | ` * compiled here in silence.` |
|        - |  853 | ` *` |
|        - |  854 | ` * The lattice below is php's, derived from the oracle: a declared type is a` |
|        - |  855 | ` * DISJUNCTION of intersection GROUPS, each group a conjunction of ATOMS, and` |
|        - |  856 | ` *` |
|        - |  857 | ` *     child ⊆ parent   iff   every child group is a subtype of SOME parent group` |
|        - |  858 | ` *     Gc ⊆ Gp          iff   every atom of Gp has SOME atom of Gc under it` |
|        - |  859 | ` *` |
|        - |  860 | ` * which is all a plain union, an intersection and a DNF type need between them.` |
|        - |  861 | `` * `bUnknown` is what is left of the old skip: a shape this cannot model (a type`` |
|        - |  862 | `` * naming a class no autoload-free lookup finds, `parent` with no base, more`` |
|        - |  863 | ` * atoms than the bound) is still ACCEPTED, because refusing valid php is the` |
|        - |  864 | ` * one failure mode that matters here.` |
|        - |  865 | ` * ---------------------------------------------------------------------------` |
|        - |  866 | ` */` |
|        - |  867 | `#define OVB_INT      0x0001` |
|        - |  868 | `#define OVB_FLOAT    0x0002` |
|        - |  869 | `#define OVB_STRING   0x0004` |
|        - |  870 | `#define OVB_BOOL     0x0008` |
|        - |  871 | `#define OVB_FALSE    0x0010` |
|        - |  872 | `#define OVB_TRUE     0x0020` |
|        - |  873 | `#define OVB_ARRAY    0x0040` |
|        - |  874 | ``#define OVB_OBJECT   0x0080  /* the `object` pseudo-type: every class at once */`` |
|        - |  875 | `#define OVB_CALLABLE 0x0100` |
|        - |  876 | `#define OVB_NULL     0x0200` |
|        - |  877 | `#define OVB_VOID     0x0400` |
|        - |  878 | ``#define OVB_STATIC   0x0800  /* `static`: the CALLED class of the declaring one */`` |
|        - |  879 | `#define OVB_CLS      0x1000  /* a named class/interface, resolved into pCls */` |
|        - |  880 | `#define OV_MAX_ATOM  16      /* bounds the on-stack atom array; over it, bUnknown */` |
|        - |  881 |  |
|        - |  882 | `typedef struct OvAtom OvAtom;` |
|        - |  883 | `struct OvAtom {` |
|        - |  884 | `	sxu32 nBit;      /* OVB_* */` |
|        - |  885 | `	ph7_class *pCls; /* the class, when nBit == OVB_CLS; 0 for one nothing has loaded */` |
|        - |  886 | `	const SyString *pName; /* ...whose resolved NAME is then all there is */` |
|        - |  887 | `	sxu32 nGroup;    /* intersection group: atoms sharing one are ANDed */` |
|        - |  888 | `};` |
|        - |  889 | `typedef struct OvType OvType;` |
|        - |  890 | `struct OvType {` |
|        - |  891 | `	int bAbsent;  /* no declared type at all -- not a type, an ABSENCE (see below) */` |
|        - |  892 | ``	int bMixed;   /* `mixed`: the top type */`` |
|        - |  893 | ``	int bNever;   /* `never`: the bottom type, a subtype of everything */`` |
|        - |  894 | `	int bUnknown; /* a shape this lattice does not model -- accept whatever it meets */` |
|        - |  895 | `	int bUnres;   /* some class atom names a class nothing has loaded (pCls == 0) */` |
|        - |  896 | `	int nAtom;` |
|        - |  897 | `	sxu32 nNextGroup;` |
|        - |  898 | `	OvAtom a[OV_MAX_ATOM];` |
|        - |  899 | `};` |
|     1376 |  900 | `static void OvInit(OvType *pT)` |
|        5 |  901 | `{` |
|     1381 |  902 | `	SyZero(pT,sizeof(*pT));` |
|     1381 |  903 | `}` |
|      916 |  904 | `static void OvAddAtom(OvType *pT,sxu32 nBit,ph7_class *pCls,sxu32 nGroup)` |
|        5 |  905 | `{` |
|      921 |  906 | `	if( pT->nAtom >= OV_MAX_ATOM ){` |
|      ! 0 |  907 | `		pT->bUnknown = 1;` |
|      ! 0 |  908 | `		return;` |
|        - |  909 | `	}` |
|      921 |  910 | `	pT->a[pT->nAtom].nBit = nBit;` |
|      921 |  911 | `	pT->a[pT->nAtom].pCls = pCls;` |
|      921 |  912 | `	pT->a[pT->nAtom].pName = 0;` |
|      921 |  913 | `	pT->a[pT->nAtom].nGroup = nGroup;` |
|      921 |  914 | `	pT->nAtom++;` |
|      921 |  915 | `	if( nGroup >= pT->nNextGroup ){` |
|      851 |  916 | `		pT->nNextGroup = nGroup + 1;` |
|      423 |  917 | `	}` |
|      463 |  918 | `}` |
|        - |  919 | `/* One atom written as a NAME: a class, or one of the pseudo-types php parses as` |
|        - |  920 | `` * a class-name atom. `iterable` is TWO types, so it contributes two atoms in two`` |
|        - |  921 | ` * groups -- it is a union, never an intersection member (php forbids the latter). */` |
|      298 |  922 | `static void OvAddName(ph7_vm *pVm,ph7_class *pScope,OvType *pT,const SyString *pName,sxu32 nGroup)` |
|        5 |  923 | `{` |
|        - |  924 | `	static const struct { const char *z; sxu32 n; sxu32 nBit; } aWord[] = {` |
|        - |  925 | `		{ "callable",8, OVB_CALLABLE }, { "false",5, OVB_FALSE }, { "true",4, OVB_TRUE },` |
|        - |  926 | `		{ "object",6, OVB_OBJECT },     { "null",4,  OVB_NULL },  { "void",4,  OVB_VOID },` |
|        - |  927 | `		{ "static",6, OVB_STATIC },     { "int",3,   OVB_INT },   { "float",5, OVB_FLOAT },` |
|        - |  928 | `		{ "string",6, OVB_STRING },     { "bool",4,  OVB_BOOL },  { "array",5, OVB_ARRAY }` |
|        - |  929 | `	};` |
|      303 |  930 | `	const char *z = SyStringData(pName);` |
|      303 |  931 | `	sxu32 n = SyStringLength(pName), i;` |
|        - |  932 | `	SyHashEntry *pE;` |
|      303 |  933 | `	if( n < 1 ){` |
|      ! 0 |  934 | `		pT->bUnknown = 1;` |
|      ! 0 |  935 | `		return;` |
|        - |  936 | `	}` |
|      303 |  937 | `	if( n == sizeof("mixed")-1 && SyStrnicmp(z,"mixed",n) == 0 ){` |
|       18 |  938 | `		pT->bMixed = 1;` |
|       18 |  939 | `		return;` |
|        - |  940 | `	}` |
|      289 |  941 | `	if( n == sizeof("never")-1 && SyStrnicmp(z,"never",n) == 0 ){` |
|      ! 0 |  942 | `		pT->bNever = 1;` |
|      ! 0 |  943 | `		return;` |
|        - |  944 | `	}` |
|      289 |  945 | `	if( n == sizeof("iterable")-1 && SyStrnicmp(z,"iterable",n) == 0 ){` |
|        6 |  946 | `		ph7_class *pTrav = PH7_VmExtractClass(pVm,"Traversable",sizeof("Traversable")-1,FALSE,0);` |
|        6 |  947 | `		if( pTrav == 0 ){` |
|      ! 0 |  948 | `			pT->bUnknown = 1;` |
|      ! 0 |  949 | `			return;` |
|        - |  950 | `		}` |
|        6 |  951 | `		OvAddAtom(pT,OVB_ARRAY,0,pT->nNextGroup);` |
|        6 |  952 | `		OvAddAtom(pT,OVB_CLS,pTrav,pT->nNextGroup);` |
|        6 |  953 | `		return;` |
|        - |  954 | `	}` |
|     3467 |  955 | `	for( i = 0 ; i < SX_ARRAYSIZE(aWord) ; ++i ){` |
|     3215 |  956 | `		if( n == aWord[i].n && SyStrnicmp(z,aWord[i].z,n) == 0 ){` |
|       31 |  957 | `			OvAddAtom(pT,aWord[i].nBit,0,nGroup);` |
|       31 |  958 | `			return;` |
|        - |  959 | `		}` |
|     1596 |  960 | `	}` |
|      257 |  961 | `	if( n == sizeof("self")-1 && SyStrnicmp(z,"self",n) == 0 ){` |
|       29 |  962 | `		if( pScope == 0 ){` |
|      ! 0 |  963 | `			pT->bUnknown = 1;` |
|      ! 0 |  964 | `			return;` |
|        - |  965 | `		}` |
|       29 |  966 | `		OvAddAtom(pT,OVB_CLS,pScope,nGroup);` |
|       29 |  967 | `		return;` |
|        - |  968 | `	}` |
|      231 |  969 | `	if( n == sizeof("parent")-1 && SyStrnicmp(z,"parent",n) == 0 ){` |
|      ! 0 |  970 | `		if( pScope == 0 \|\| pScope->pBase == 0 ){` |
|      ! 0 |  971 | `			pT->bUnknown = 1;` |
|      ! 0 |  972 | `			return;` |
|        - |  973 | `		}` |
|      ! 0 |  974 | `		OvAddAtom(pT,OVB_CLS,pScope->pBase,nGroup);` |
|      ! 0 |  975 | `		return;` |
|        - |  976 | `	}` |
|        - |  977 | `	/* A real class name, resolved WITHOUT autoloading: a miss is a forward` |
|        - |  978 | `	 * reference or a class no lookup can produce. It stays an atom of its own,` |
|        - |  979 | `	 * known only by name -- php's unresolved class, which still decides every` |
|        - |  980 | `	 * question its name or its being SOME class answers (OvCheck). */` |
|      231 |  981 | `	pE = PH7_VmClassEntry(pVm,z,n);` |
|      231 |  982 | `	if( pE == 0 ){` |
|        - |  983 | `		/* ...except the class being declared, which is not filed yet while its` |
|        - |  984 | `		 * own inheritance runs. */` |
|      144 |  985 | `		if( pScope != 0 && SyStringLength(&pScope->sName) == n` |
|       78 |  986 | `		 && SyStrnicmp(SyStringData(&pScope->sName),z,n) == 0 ){` |
|      ! 0 |  987 | `			OvAddAtom(pT,OVB_CLS,pScope,nGroup);` |
|      ! 0 |  988 | `			return;` |
|        - |  989 | `		}` |
|      148 |  990 | `		i = (sxu32)pT->nAtom;` |
|      148 |  991 | `		OvAddAtom(pT,OVB_CLS,0,nGroup);` |
|      148 |  992 | `		if( (sxu32)pT->nAtom > i ){` |
|      148 |  993 | `			pT->a[i].pName = pName;` |
|      148 |  994 | `			pT->bUnres = 1;` |
|       72 |  995 | `		}` |
|      148 |  996 | `		return;` |
|        - |  997 | `	}` |
|       87 |  998 | `	OvAddAtom(pT,OVB_CLS,(ph7_class *)pE->pUserData,nGroup);` |
|      154 |  999 | `}` |
|        - | 1000 | `/* One atom given as a MEMOBJ_* code plus, for SXU32_HIGH, its name. */` |
|      866 | 1001 | `static void OvAddCode(ph7_vm *pVm,ph7_class *pScope,OvType *pT,sxu32 nType,` |
|        - | 1002 | `	const SyString *pName,sxu32 nGroup)` |
|        5 | 1003 | `{` |
|      871 | 1004 | `	switch( nType ){` |
|      371 | 1005 | `	case MEMOBJ_INT:     OvAddAtom(pT,OVB_INT,0,nGroup);    return;` |
|        3 | 1006 | `	case MEMOBJ_REAL:    OvAddAtom(pT,OVB_FLOAT,0,nGroup);  return;` |
|      127 | 1007 | `	case MEMOBJ_STRING:  OvAddAtom(pT,OVB_STRING,0,nGroup); return;` |
|        8 | 1008 | `	case MEMOBJ_BOOL:    OvAddAtom(pT,OVB_BOOL,0,nGroup);   return;` |
|        6 | 1009 | `	case MEMOBJ_HASHMAP: OvAddAtom(pT,OVB_ARRAY,0,nGroup);  return;` |
|        5 | 1010 | `	case MEMOBJ_OBJ:     OvAddAtom(pT,OVB_OBJECT,0,nGroup); return;` |
|       66 | 1011 | `	case MEMOBJ_VOID:    OvAddAtom(pT,OVB_VOID,0,nGroup);   return;` |
|        3 | 1012 | `	case MEMOBJ_NEVER:   pT->bNever = 1;                    return;` |
|        - | 1013 | ``	/* php 8.2's standalone `null`, which the parser records BOTH as this code and`` |
|        - | 1014 | `	 * as the nullable flag; the second atom the flag adds is the same type. */` |
|      ! 0 | 1015 | `	case MEMOBJ_NULL:    OvAddAtom(pT,OVB_NULL,0,nGroup);   return;` |
|      298 | 1016 | `	default: break;` |
|        - | 1017 | `	}` |
|      303 | 1018 | `	if( nType == SXU32_HIGH ){` |
|      303 | 1019 | `		OvAddName(pVm,pScope,pT,pName,nGroup);` |
|      303 | 1020 | `		return;` |
|        - | 1021 | `	}` |
|      ! 0 | 1022 | `	pT->bUnknown = 1;` |
|      438 | 1023 | `}` |
|        - | 1024 | `/*` |
|        - | 1025 | ` * The union alternatives, or the single type, of one declaration.` |
|        - | 1026 | ` *` |
|        - | 1027 | ` * The stored intersection-group ids are RE-MAPPED rather than used as they come:` |
|        - | 1028 | `` * `iterable` is an alternative that expands into TWO groups of its own, so a`` |
|        - | 1029 | ` * later alternative's stored id would otherwise land in the group Traversable` |
|        - | 1030 | `` * had just been given and read as `Traversable&int`. A group with more than one`` |
|        - | 1031 | `` * member is an intersection, which `iterable` may not appear in at all (php`` |
|        - | 1032 | `` * refuses `iterable&X`) -- if one ever did, the whole type is undecidable.`` |
|        - | 1033 | ` */` |
|     1374 | 1034 | `static void OvFromDecl(ph7_vm *pVm,ph7_class *pScope,OvType *pT,sxu32 nType,` |
|        - | 1035 | `	const SyString *pClass,SySet *pAlts,int bNullable)` |
|        5 | 1036 | `{` |
|     1379 | 1037 | `	OvInit(pT);` |
|     1379 | 1038 | `	if( SySetUsed(pAlts) > 0 ){` |
|       37 | 1039 | `		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(pAlts);` |
|        - | 1040 | `		sxu32 aMap[PHL_UNION_MAX_ALTS];` |
|        - | 1041 | `		sxu32 aCount[PHL_UNION_MAX_ALTS];` |
|       37 | 1042 | `		sxu32 i, n = SySetUsed(pAlts);` |
|     1125 | 1043 | `		for( i = 0 ; i < PHL_UNION_MAX_ALTS ; ++i ){` |
|     1091 | 1044 | `			aMap[i] = SXU32_HIGH;` |
|     1091 | 1045 | `			aCount[i] = 0;` |
|      547 | 1046 | `		}` |
|      107 | 1047 | `		for( i = 0 ; i < n ; ++i ){` |
|       73 | 1048 | `			if( aAlt[i].nGroup >= PHL_UNION_MAX_ALTS ){` |
|      ! 0 | 1049 | `				pT->bUnknown = 1;` |
|      ! 0 | 1050 | `				return;` |
|        - | 1051 | `			}` |
|       73 | 1052 | `			aCount[aAlt[i].nGroup]++;` |
|       38 | 1053 | `		}` |
|      107 | 1054 | `		for( i = 0 ; i < n ; ++i ){` |
|       73 | 1055 | `			sxu32 g = aAlt[i].nGroup;` |
|      119 | 1056 | `			int bIter = ( aAlt[i].nType == SXU32_HIGH` |
|       46 | 1057 | `				&& SyStringLength(&aAlt[i].sClass) == sizeof("iterable")-1` |
|       81 | 1058 | `				&& SyStrnicmp(SyStringData(&aAlt[i].sClass),"iterable",sizeof("iterable")-1) == 0 );` |
|       73 | 1059 | `			if( bIter && aCount[g] > 1 ){` |
|      ! 0 | 1060 | `				pT->bUnknown = 1;` |
|      ! 0 | 1061 | `				return;` |
|        - | 1062 | `			}` |
|       73 | 1063 | `			if( aMap[g] == SXU32_HIGH ){` |
|       71 | 1064 | `				aMap[g] = pT->nNextGroup;` |
|       71 | 1065 | `				pT->nNextGroup++;` |
|       34 | 1066 | `			}` |
|       73 | 1067 | `			OvAddCode(pVm,pScope,pT,aAlt[i].nType,&aAlt[i].sClass,aMap[g]);` |
|       38 | 1068 | `		}` |
|     1360 | 1069 | `	}else if( nType != 0 ){` |
|      801 | 1070 | `		OvAddCode(pVm,pScope,pT,nType,pClass,pT->nNextGroup);` |
|      398 | 1071 | `	}` |
|     1379 | 1072 | `	if( bNullable ){` |
|       67 | 1073 | `		OvAddAtom(pT,OVB_NULL,0,pT->nNextGroup);` |
|       31 | 1074 | `	}` |
|        - | 1075 | ``	/* Nothing written at all: an ABSENCE, which is not the same as `mixed` --`` |
|        - | 1076 | `	 * php skips the check on the side that has none, so a missing PARAMETER type` |
|        - | 1077 | `	 * accepts any parent and a missing RETURN type is refused under a declared` |
|        - | 1078 | ``	 * one (`f(): int` overridden by `f()` is a fatal, `f(): mixed` is not). */`` |
|     1379 | 1079 | `	if( pT->nAtom == 0 && !pT->bMixed && !pT->bNever && !pT->bUnknown ){` |
|      549 | 1080 | `		pT->bAbsent = 1;` |
|      272 | 1081 | `	}` |
|      692 | 1082 | `}` |
|      268 | 1083 | `static void OvFromArg(ph7_vm *pVm,ph7_class *pScope,ph7_vm_func_arg *pA,OvType *pT)` |
|        5 | 1084 | `{` |
|      407 | 1085 | `	OvFromDecl(pVm,pScope,pT,pA->nType,&pA->sClass,&pA->aUnionAlts,` |
|      268 | 1086 | `		(pA->iFlags & VM_FUNC_ARG_NULLABLE) != 0);` |
|      273 | 1087 | `}` |
|      800 | 1088 | `static void OvFromReturn(ph7_vm *pVm,ph7_class *pScope,ph7_vm_func *pF,OvType *pT)` |
|        5 | 1089 | `{` |
|     1205 | 1090 | `	OvFromDecl(pVm,pScope,pT,pF->nReturnType,&pF->sReturnClass,&pF->aReturnUnion,` |
|      800 | 1091 | `		(pF->iFlags & VM_FUNC_RETURN_NULLABLE) != 0);` |
|      805 | 1092 | `}` |
|        - | 1093 | `/*` |
|        - | 1094 | `` * Is the single atom *pC under the single atom *pP? `object` is over every class`` |
|        - | 1095 | `` * (and over `static`, which IS one), `bool` is over `false` and `true`, and`` |
|        - | 1096 | `` * `static` is under any class the declaring class is an instance of -- but`` |
|        - | 1097 | `` * nothing except another `static` is under IT, since the called class may be a`` |
|        - | 1098 | ` * subclass nobody has written yet.` |
|        - | 1099 | ` */` |
|      666 | 1100 | `static int OvAtomLE(const OvAtom *pC,const OvAtom *pP,ph7_class *pSubScope,int bHope)` |
|        5 | 1101 | `{` |
|      671 | 1102 | `	if( pP->nBit == OVB_CLS && pC->nBit == OVB_CLS && (pP->pCls == 0 \|\| pC->pCls == 0) ){` |
|        - | 1103 | `		/* An unloaded class is under itself by NAME (php's case-insensitive` |
|        - | 1104 | `		 * shortcut, taken before any lookup) and is otherwise whatever bHope says. */` |
|      154 | 1105 | `		if( pP->pCls == 0 && pC->pCls == 0 && SyStringLength(pP->pName) == SyStringLength(pC->pName)` |
|      138 | 1106 | `		 && SyStrnicmp(SyStringData(pP->pName),SyStringData(pC->pName),SyStringLength(pP->pName)) == 0 ){` |
|       33 | 1107 | `			return 1;` |
|        - | 1108 | `		}` |
|      128 | 1109 | `		return bHope;` |
|        - | 1110 | `	}` |
|      517 | 1111 | `	if( pP->nBit == OVB_OBJECT ){` |
|        3 | 1112 | `		return pC->nBit == OVB_OBJECT \|\| pC->nBit == OVB_CLS \|\| pC->nBit == OVB_STATIC;` |
|        - | 1113 | `	}` |
|      515 | 1114 | `	if( pP->nBit == OVB_BOOL ){` |
|        6 | 1115 | `		return pC->nBit == OVB_BOOL \|\| pC->nBit == OVB_FALSE \|\| pC->nBit == OVB_TRUE;` |
|        - | 1116 | `	}` |
|      511 | 1117 | `	if( pP->nBit == OVB_STATIC ){` |
|       27 | 1118 | `		if( pC->nBit == OVB_STATIC ){` |
|      ! 0 | 1119 | `			return 1;` |
|        - | 1120 | `		}` |
|        - | 1121 | `		/* php's one exception, and a library really writes it: in a FINAL class` |
|        - | 1122 | ``		 * `self` IS `static`, because no subclass can ever exist for the called`` |
|        - | 1123 | `		 * class to be. It is the class ITSELF and nothing else -- naming the` |
|        - | 1124 | `		 * PARENT is still a fatal, even from a final child (an enum carries the` |
|        - | 1125 | `		 * final flag, so its own name works the same way). */` |
|       26 | 1126 | `		if( pC->nBit == OVB_CLS && pSubScope != 0 && pC->pCls == pSubScope` |
|       20 | 1127 | `		 && (pSubScope->iFlags & PH7_CLASS_FINAL) != 0 ){` |
|       17 | 1128 | `			return 1;` |
|        - | 1129 | `		}` |
|       11 | 1130 | `		return 0;` |
|        - | 1131 | `	}` |
|      485 | 1132 | `	if( pP->nBit == OVB_CLS ){` |
|       85 | 1133 | `		if( pC->nBit == OVB_CLS ){` |
|       55 | 1134 | `			return PH7_VmInstanceOf(pC->pCls,pP->pCls) ? 1 : 0;` |
|        - | 1135 | `		}` |
|       35 | 1136 | `		if( pC->nBit == OVB_STATIC ){` |
|        8 | 1137 | `			if( pP->pCls == 0 ){` |
|      ! 0 | 1138 | `				return bHope;` |
|        - | 1139 | `			}` |
|        8 | 1140 | `			return (pSubScope && PH7_VmInstanceOf(pSubScope,pP->pCls)) ? 1 : 0;` |
|        - | 1141 | `		}` |
|       28 | 1142 | `		return 0;` |
|        - | 1143 | `	}` |
|      405 | 1144 | `	return pC->nBit == pP->nBit;` |
|      338 | 1145 | `}` |
|        - | 1146 | `/* Gc ⊆ Gp: every atom of the parent group has some atom of the child group under` |
|        - | 1147 | ` * it (an intersection is under X as soon as ONE of its members is). */` |
|      664 | 1148 | `static int OvGroupLE(const OvType *pC,sxu32 gC,const OvType *pP,sxu32 gP,ph7_class *pSubScope,int bHope)` |
|        5 | 1149 | `{` |
|        - | 1150 | `	int i, j;` |
|     1309 | 1151 | `	for( j = 0 ; j < pP->nAtom ; ++j ){` |
|      801 | 1152 | `		int bCovered = 0;` |
|      801 | 1153 | `		if( pP->a[j].nGroup != gP ){` |
|      135 | 1154 | `			continue;` |
|        - | 1155 | `		}` |
|     1473 | 1156 | `		for( i = 0 ; i < pC->nAtom && !bCovered ; ++i ){` |
|      807 | 1157 | `			if( pC->a[i].nGroup == gC && OvAtomLE(&pC->a[i],&pP->a[j],pSubScope,bHope) ){` |
|      515 | 1158 | `				bCovered = 1;` |
|      255 | 1159 | `			}` |
|      406 | 1160 | `		}` |
|      671 | 1161 | `		if( !bCovered ){` |
|      161 | 1162 | `			return 0;` |
|        - | 1163 | `		}` |
|      260 | 1164 | `	}` |
|      513 | 1165 | `	return 1;` |
|      337 | 1166 | `}` |
|        - | 1167 | `/* child ⊆ parent (pSubScope is the SUBTYPE side's declaring class, which is what` |
|        - | 1168 | `` * a `static` atom there stands for). Both are normalized and neither is`` |
|        - | 1169 | ` * absent/mixed/never/unknown -- OvCheck settled those. */` |
|      540 | 1170 | `static int OvSubtype(const OvType *pC,const OvType *pP,ph7_class *pSubScope,int bHope)` |
|        5 | 1171 | `{` |
|        - | 1172 | `	sxu32 gC, gP;` |
|      545 | 1173 | `	int bAnyC = 0;` |
|     1053 | 1174 | `	for( gC = 0 ; gC < pC->nNextGroup ; ++gC ){` |
|      619 | 1175 | `		int i, bHasC = 0, bCovered = 0;` |
|      697 | 1176 | `		for( i = 0 ; i < pC->nAtom ; ++i ){` |
|      697 | 1177 | `			if( pC->a[i].nGroup == gC ){ bHasC = 1; break; }` |
|       44 | 1178 | `		}` |
|      619 | 1179 | `		if( !bHasC ){` |
|      ! 0 | 1180 | `			continue;` |
|        - | 1181 | `		}` |
|      619 | 1182 | `		bAnyC = 1;` |
|     1283 | 1183 | `		for( gP = 0 ; gP < pP->nNextGroup && !bCovered ; ++gP ){` |
|      669 | 1184 | `			int j, bHasP = 0;` |
|      723 | 1185 | `			for( j = 0 ; j < pP->nAtom ; ++j ){` |
|      723 | 1186 | `				if( pP->a[j].nGroup == gP ){ bHasP = 1; break; }` |
|       31 | 1187 | `			}` |
|      669 | 1188 | `			if( bHasP && OvGroupLE(pC,gC,pP,gP,pSubScope,bHope) ){` |
|      513 | 1189 | `				bCovered = 1;` |
|      254 | 1190 | `			}` |
|      337 | 1191 | `		}` |
|      619 | 1192 | `		if( !bCovered ){` |
|      110 | 1193 | `			return 0;` |
|        - | 1194 | `		}` |
|      259 | 1195 | `	}` |
|      439 | 1196 | `	return bAnyC;` |
|      275 | 1197 | `}` |
|        - | 1198 | `#define OV_OK    0 /* the pair is compatible */` |
|        - | 1199 | `#define OV_BAD   1 /* php refuses it */` |
|        - | 1200 | `#define OV_UNRES 2 /* only a class nothing has loaded yet can decide it */` |
|        - | 1201 | `/*` |
|        - | 1202 | ` * One declared-type pair, in one variance direction. bCovariant = 1 for a return` |
|        - | 1203 | ` * type (the child must be UNDER the parent), 0 for a parameter (over it).` |
|        - | 1204 | ` *` |
|        - | 1205 | ` * The two ABSENCES are asymmetric and that asymmetry is php's: the side with no` |
|        - | 1206 | ` * declared type is simply not checked, so a parameter the CHILD left untyped is` |
|        - | 1207 | ` * always fine and a return the child left untyped is a fatal under any declared` |
|        - | 1208 | `` * parent -- `mixed` included, even though `mixed` is the top type.`` |
|        - | 1209 | ` */` |
|      764 | 1210 | `static int OvCheck(const OvType *pP,const OvType *pC,int bCovariant,` |
|        - | 1211 | `	ph7_class *pParentScope,ph7_class *pChildScope)` |
|        5 | 1212 | `{` |
|      769 | 1213 | `	const OvType *pSub = bCovariant ? pC : pP;   /* must be the subtype */` |
|      769 | 1214 | `	const OvType *pSup = bCovariant ? pP : pC;` |
|        - | 1215 | ``	/* `static` is decided against the scope of whichever side is the SUBTYPE --`` |
|        - | 1216 | `	 * the class whose called-class it stands for. */` |
|      769 | 1217 | `	ph7_class *pSubScope = bCovariant ? pChildScope : pParentScope;` |
|        - | 1218 | `	int bSubVoid, bSupVoid, i;` |
|      769 | 1219 | `	if( bCovariant && pP->bAbsent ){` |
|      225 | 1220 | `		return OV_OK;   /* nothing to be under */` |
|        - | 1221 | `	}` |
|      549 | 1222 | `	if( bCovariant && pC->bAbsent ){` |
|        - | 1223 | `		/* The ONE place an absent type is not simply the top type: a child that` |
|        - | 1224 | `		 * declares no RETURN type is refused under any parent that declares one,` |
|        - | 1225 | ``		 * `mixed` included. Everywhere else absence reads as `mixed` below. */`` |
|        3 | 1226 | `		return OV_BAD;` |
|        - | 1227 | `	}` |
|      547 | 1228 | `	if( !bCovariant && pC->bAbsent ){` |
|       57 | 1229 | `		return OV_OK;   /* an untyped parameter accepts whatever the parent's did */` |
|        - | 1230 | `	}` |
|      493 | 1231 | `	if( pP->bUnknown \|\| pC->bUnknown ){` |
|      ! 0 | 1232 | `		return OV_OK;   /* a shape this lattice does not model -- accept */` |
|        - | 1233 | `	}` |
|        - | 1234 | ``	/* `void` pairs with `void` and with nothing else -- not even with `mixed`,`` |
|        - | 1235 | `	 * which is over every other type. Decided before the top/bottom shortcuts. */` |
|      493 | 1236 | `	bSubVoid = bSupVoid = 0;` |
|     1037 | 1237 | `	for( i = 0 ; i < pSub->nAtom ; ++i ){ if( pSub->a[i].nBit == OVB_VOID ) bSubVoid = 1; }` |
|     1045 | 1238 | `	for( i = 0 ; i < pSup->nAtom ; ++i ){ if( pSup->a[i].nBit == OVB_VOID ) bSupVoid = 1; }` |
|      493 | 1239 | `	if( bSubVoid != bSupVoid && !pSub->bNever ){` |
|        3 | 1240 | `		return OV_BAD;` |
|        - | 1241 | `	}` |
|      491 | 1242 | `	if( pSub->bNever ){` |
|        3 | 1243 | `		return OV_OK;   /* the bottom type is under everything */` |
|        - | 1244 | `	}` |
|      489 | 1245 | `	if( pSup->bMixed \|\| pSup->bAbsent ){` |
|       13 | 1246 | `		return OV_OK;   /* ...and the top type is over everything */` |
|        - | 1247 | `	}` |
|      479 | 1248 | `	if( pSub->bMixed \|\| pSub->bAbsent \|\| pSup->bNever ){` |
|      ! 0 | 1249 | `		return OV_BAD;` |
|        - | 1250 | `	}` |
|        - | 1251 | `	/* An unloaded class is decided both ways: as a class that is under nothing` |
|        - | 1252 | `	 * it is not named, then as one that could be under any. Holding the first` |
|        - | 1253 | ``	 * way is php's success, failing the second its error -- a `null` or an`` |
|        - | 1254 | ``	 * `int` no class can stand for -- and between them is php's UNRESOLVED. */`` |
|      479 | 1255 | `	if( OvSubtype(pSub,pSup,pSubScope,0) ){` |
|      377 | 1256 | `		return OV_OK;` |
|        - | 1257 | `	}` |
|      106 | 1258 | `	if( (pSub->bUnres \|\| pSup->bUnres) && OvSubtype(pSub,pSup,pSubScope,1) ){` |
|       66 | 1259 | `		return OV_UNRES;` |
|        - | 1260 | `	}` |
|       44 | 1261 | `	return OV_BAD;` |
|      387 | 1262 | `}` |
|        - | 1263 | `/*` |
|        - | 1264 | ` * A pair left UNRESOLVED is php's variance obligation: the class is declared` |
|        - | 1265 | ` * where its statement runs, and every name the check could not look up is` |
|        - | 1266 | ` * autoloaded there before the pair is asked again. These keep the names, in the` |
|        - | 1267 | ` * order php's checker asks for them -- the SUBTYPE side of a pair first, then` |
|        - | 1268 | ` * the side it must be under -- and say which one a still-open pair names.` |
|        - | 1269 | ` */` |
|       30 | 1270 | `static const SyString * OvFirstUnres(const OvType *pSub,const OvType *pSup)` |
|        4 | 1271 | `{` |
|        - | 1272 | `	const OvType *aSide[2];` |
|        - | 1273 | `	int i, k;` |
|       34 | 1274 | `	aSide[0] = pSub;` |
|       34 | 1275 | `	aSide[1] = pSup;` |
|       34 | 1276 | `	for( k = 0 ; k < 2 ; ++k ){` |
|       34 | 1277 | `		for( i = 0 ; i < aSide[k]->nAtom ; ++i ){` |
|       34 | 1278 | `			if( aSide[k]->a[i].nBit == OVB_CLS && aSide[k]->a[i].pCls == 0 ){` |
|       34 | 1279 | `				return aSide[k]->a[i].pName;` |
|        - | 1280 | `			}` |
|      ! 0 | 1281 | `		}` |
|      ! 0 | 1282 | `	}` |
|      ! 0 | 1283 | `	return 0;` |
|       19 | 1284 | `}` |
|       52 | 1285 | `static void OvNoteUnres(ph7_gen_state *pGen,const OvType *pSub,const OvType *pSup)` |
|        4 | 1286 | `{` |
|        - | 1287 | `	const OvType *aSide[2];` |
|        - | 1288 | `	SySet *pNames;` |
|        - | 1289 | `	int i, k;` |
|       56 | 1290 | `	if( pGen->pOblige == 0 ){` |
|       16 | 1291 | `		return;` |
|        - | 1292 | `	}` |
|       44 | 1293 | `	pNames = &pGen->pOblige->aName;` |
|       44 | 1294 | `	aSide[0] = pSub;` |
|       44 | 1295 | `	aSide[1] = pSup;` |
|      124 | 1296 | `	for( k = 0 ; k < 2 ; ++k ){` |
|      166 | 1297 | `		for( i = 0 ; i < aSide[k]->nAtom ; ++i ){` |
|       86 | 1298 | `			const SyString *pName = aSide[k]->a[i].pName;` |
|       86 | 1299 | `			SyString *aHave = (SyString *)SySetBasePtr(pNames);` |
|        - | 1300 | `			sxu32 n;` |
|       86 | 1301 | `			if( aSide[k]->a[i].nBit != OVB_CLS \|\| aSide[k]->a[i].pCls != 0 \|\| pName == 0 ){` |
|        3 | 1302 | `				continue;` |
|        - | 1303 | `			}` |
|      128 | 1304 | `			for( n = 0 ; n < SySetUsed(pNames) ; ++n ){` |
|       60 | 1305 | `				if( aHave[n].nByte == pName->nByte` |
|       56 | 1306 | `				 && SyStrnicmp(aHave[n].zString,pName->zString,pName->nByte) == 0 ){` |
|       18 | 1307 | `					break;` |
|        - | 1308 | `				}` |
|       26 | 1309 | `			}` |
|       84 | 1310 | `			if( n >= SySetUsed(pNames) ){` |
|       68 | 1311 | `				SySetPut(pNames,(const void *)pName);` |
|       32 | 1312 | `			}` |
|       44 | 1313 | `		}` |
|       44 | 1314 | `	}` |
|       30 | 1315 | `}` |
|       30 | 1316 | `static void OvRecordOblige(ph7_gen_state *pGen,ph7_class *pBase,ph7_class *pSub,` |
|        - | 1317 | `	void *pParent,void *pChild,int bProp,int bCtorExempt)` |
|        4 | 1318 | `{` |
|        - | 1319 | `	VmClassOblige sRec;` |
|       34 | 1320 | `	if( pGen->pOblige == 0 ){` |
|      ! 0 | 1321 | `		return;` |
|        - | 1322 | `	}` |
|       34 | 1323 | `	sRec.pBase = pBase;` |
|       34 | 1324 | `	sRec.pSub = pSub;` |
|       34 | 1325 | `	sRec.pParent = pParent;` |
|       34 | 1326 | `	sRec.pChild = pChild;` |
|       34 | 1327 | `	sRec.bProp = (sxu8)(bProp != 0);` |
|       34 | 1328 | `	sRec.bCtorExempt = (sxu8)(bCtorExempt != 0);` |
|       34 | 1329 | `	SySetPut(&pGen->pOblige->aOblige,(const void *)&sRec);` |
|       19 | 1330 | `}` |
|        - | 1331 | `/*` |
|        - | 1332 | ` * ---------------------------------------------------------------------------` |
|        - | 1333 | ` * The ARITY half, which is not the type lattice's: php asks whether every call` |
|        - | 1334 | ` * the parent's declaration accepts can reach the child.` |
|        - | 1335 | ` * ---------------------------------------------------------------------------` |
|        - | 1336 | ` */` |
|        - | 1337 | `/* php's required_num_args: how many arguments a caller MUST supply. */` |
|      832 | 1338 | `static sxu32 OvReqArgs(ph7_vm_func *pF)` |
|        5 | 1339 | `{` |
|      837 | 1340 | `	ph7_vm_func_arg *a = (ph7_vm_func_arg *)SySetBasePtr(&pF->aArgs);` |
|      837 | 1341 | `	sxu32 n = SySetUsed(&pF->aArgs), i, nReq = 0;` |
|     1165 | 1342 | `	for( i = 0 ; i < n ; ++i ){` |
|      333 | 1343 | `		if( (a[i].iFlags & VM_FUNC_ARG_VARIADIC) \|\| SySetUsed(&a[i].aByteCode) > 0 ){` |
|       62 | 1344 | `			continue; /* a variadic tail and a defaulted parameter are both optional */` |
|        - | 1345 | `		}` |
|      275 | 1346 | `		nReq = i + 1;` |
|      140 | 1347 | `	}` |
|      837 | 1348 | `	return nReq;` |
|        5 | 1349 | `}` |
|      836 | 1350 | `static int OvIsVariadic(ph7_vm_func *pF)` |
|        5 | 1351 | `{` |
|      841 | 1352 | `	ph7_vm_func_arg *a = (ph7_vm_func_arg *)SySetBasePtr(&pF->aArgs);` |
|      841 | 1353 | `	sxu32 n = SySetUsed(&pF->aArgs);` |
|      841 | 1354 | `	return n > 0 && (a[n-1].iFlags & VM_FUNC_ARG_VARIADIC) != 0;` |
|        5 | 1355 | `}` |
|        - | 1356 | `/* The parameter that ANSWERS position i: the one declared there, or the variadic` |
|        - | 1357 | ` * tail, which keeps answering for every position past its own. */` |
|      288 | 1358 | `static ph7_vm_func_arg * OvArgAt(ph7_vm_func *pF,sxu32 i)` |
|        5 | 1359 | `{` |
|      293 | 1360 | `	ph7_vm_func_arg *a = (ph7_vm_func_arg *)SySetBasePtr(&pF->aArgs);` |
|      293 | 1361 | `	sxu32 n = SySetUsed(&pF->aArgs);` |
|      293 | 1362 | `	if( i < n ){` |
|      281 | 1363 | `		return &a[i];` |
|        - | 1364 | `	}` |
|       14 | 1365 | `	if( n > 0 && (a[n-1].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        5 | 1366 | `		return &a[n-1];` |
|        - | 1367 | `	}` |
|       10 | 1368 | `	return 0;` |
|      149 | 1369 | `}` |
|        - | 1370 | `/*` |
|        - | 1371 | ` * Check a child method's signature against the parent method it overrides.` |
|        - | 1372 | ` * Emits a PHP-style "Declaration of … must be compatible …" fatal on a clear` |
|        - | 1373 | ` * incompatibility.` |
|        - | 1374 | ` *` |
|        - | 1375 | `` * bCtorExempt tells the two regimes php has for `__construct` apart: an`` |
|        - | 1376 | ` * INHERITED constructor is exempt from variance entirely (a child may declare` |
|        - | 1377 | ` * whatever it likes), while one an INTERFACE declares is checked like any other` |
|        - | 1378 | `` * method -- `interface I { __construct(int $a); }` really does constrain every`` |
|        - | 1379 | ` * implementor's constructor.` |
|        - | 1380 | ` */` |
|   770483 | 1381 | `PH7_PRIVATE sxi32 PH7_ClassCheckOverrideCompat(ph7_gen_state *pGen, ph7_class *pBase, ph7_class *pSub,` |
|        - | 1382 | `	ph7_class_method *pParent, ph7_class_method *pChild, int bCtorExempt)` |
|        5 | 1383 | `{` |
|   770488 | 1384 | `	ph7_vm *pVm = pGen->pVm;` |
|   770488 | 1385 | `	ph7_vm_func *pPF = &pParent->sFunc;` |
|   770488 | 1386 | `	ph7_vm_func *pCF = &pChild->sFunc;` |
|   770488 | 1387 | `	SyString *pMName = &pCF->sName;` |
|        - | 1388 | `	/* php names the class that DECLARED each side, not the one the walk reached it` |
|        - | 1389 | ``	 * through: `class A { f() } class B extends A {} class C extends B { f() }` is`` |
|        - | 1390 | ``	 * `C::f() must be compatible with A::f()`, and a trait method belongs to the`` |
|        - | 1391 | `` 	 * class that composed it. That owner is also what `self`, `parent` and `static` `` |
|        - | 1392 | `	 * in either declaration resolve against, so the two questions are one. */` |
|   770488 | 1393 | `	ph7_class *pChildOwner = PH7_VmMemberOwnerClass((ph7_class *)pCF->pUserData,pSub);` |
|   770488 | 1394 | `	ph7_class *pParentOwner = PH7_VmMemberOwnerClass((ph7_class *)pPF->pUserData,pBase);` |
|        - | 1395 | `	sxu32 nPArg, nCArg, nPos, k;` |
|        - | 1396 | `	int bPVar, bCVar;` |
|   770488 | 1397 | `	int bBad = 0;` |
|        - | 1398 | `	int iRes;` |
|   770488 | 1399 | `	const SyString *pUnres = 0; /* the first class an unresolved pair names */` |
|        - | 1400 | `	OvType sP, sC;` |
|   770488 | 1401 | `	if( pChildOwner == 0 ){` |
|      ! 0 | 1402 | `		pChildOwner = pSub;` |
|      ! 0 | 1403 | `	}` |
|   770488 | 1404 | `	if( pParentOwner == 0 ){` |
|      ! 0 | 1405 | `		pParentOwner = pBase;` |
|      ! 0 | 1406 | `	}` |
|   770483 | 1407 | `	if( bCtorExempt` |
|   769733 | 1408 | `	 && pMName->nByte == sizeof("__construct")-1` |
|   548936 | 1409 | `	 && SyStrnmicmp(pMName->zString,"__construct",pMName->nByte) == 0 ){` |
|   244982 | 1410 | `		return SXRET_OK;` |
|        - | 1411 | `	}` |
|        - | 1412 | `	/*` |
|        - | 1413 | `	 * A NATIVE method declares its parameters in a zSig STRING, so its aArgs set` |
|        - | 1414 | `	 * is empty and there is nothing here to compare against. Reading that as` |
|        - | 1415 | `	 * "declares no parameters" made every override of one incompatible: a user` |
|        - | 1416 | `	 * class extending DOMDocument, a Reflection class or a native enum's own` |
|        - | 1417 | `	 * cases()/from() all fataled on a declaration php accepts. An` |
|        - | 1418 | `	 * engine-declared signature is compatible by construction, on either side.` |
|        - | 1419 | `	 */` |
|   525511 | 1420 | `	if( ((pPF->iFlags \| pCF->iFlags) & VM_FUNC_NATIVE) != 0 ){` |
|   525093 | 1421 | `		return SXRET_OK;` |
|        - | 1422 | `	}` |
|        - | 1423 | `	/*` |
|        - | 1424 | `	 * Arity, php's three rules -- every call the parent's declaration accepts must` |
|        - | 1425 | `	 * reach the child. A VARIADIC signature is not the exception this used to make` |
|        - | 1426 | `` 	 * of it (the whole rule stood aside, so `f(string ...$b)` overridden by `f()` `` |
|        - | 1427 | `	 * compiled): it is the tail that keeps ANSWERING past its own position.` |
|        - | 1428 | `	 */` |
|      423 | 1429 | `	nPArg = SySetUsed(&pPF->aArgs);` |
|      423 | 1430 | `	nCArg = SySetUsed(&pCF->aArgs);` |
|      423 | 1431 | `	bPVar = OvIsVariadic(pPF);` |
|      423 | 1432 | `	bCVar = OvIsVariadic(pCF);` |
|      423 | 1433 | `	if( !bBad && bPVar && !bCVar ){` |
|        3 | 1434 | `		bBad = 1;  /* the parent takes any number; the child must too */` |
|        1 | 1435 | `	}` |
|      423 | 1436 | `	if( !bBad && OvReqArgs(pCF) > OvReqArgs(pPF) ){` |
|        3 | 1437 | `		bBad = 1;  /* the child DEMANDS an argument the parent's callers do not pass */` |
|        1 | 1438 | `	}` |
|      423 | 1439 | `	if( !bBad && !bCVar && nCArg < nPArg ){` |
|        8 | 1440 | `		bBad = 1;  /* ...and it must still ACCEPT every one they do */` |
|        3 | 1441 | `	}` |
|        - | 1442 | `	/* Every position both signatures answer: the type contravariantly, and the` |
|        - | 1443 | `	 * by-reference-ness php requires to MATCH exactly (nothing checked it here). A` |
|        - | 1444 | `	 * position only the CHILD declares is unconstrained -- the arity rules above` |
|        - | 1445 | `	 * already made it optional. */` |
|      423 | 1446 | `	nPos = nPArg > nCArg ? nPArg : nCArg;` |
|      565 | 1447 | `	for( k = 0 ; !bBad && k < nPos ; ++k ){` |
|      149 | 1448 | `		ph7_vm_func_arg *pPa = OvArgAt(pPF,k);` |
|      149 | 1449 | `		ph7_vm_func_arg *pCa = OvArgAt(pCF,k);` |
|      149 | 1450 | `		if( pPa == 0 \|\| pCa == 0 ){` |
|       10 | 1451 | `			continue;` |
|        - | 1452 | `		}` |
|      141 | 1453 | `		if( ((pPa->iFlags ^ pCa->iFlags) & VM_FUNC_ARG_BY_REF) != 0 ){` |
|        3 | 1454 | `			bBad = 1;` |
|        3 | 1455 | `			break;` |
|        - | 1456 | `		}` |
|      139 | 1457 | `		OvFromArg(pVm,pParentOwner,pPa,&sP);` |
|      139 | 1458 | `		OvFromArg(pVm,pChildOwner,pCa,&sC);` |
|      139 | 1459 | `		iRes = OvCheck(&sP,&sC,/* bCovariant */ 0,pParentOwner,pChildOwner);` |
|      139 | 1460 | `		bBad = iRes == OV_BAD;` |
|      139 | 1461 | `		if( iRes == OV_UNRES ){` |
|        - | 1462 | `			/* the parent's type is the side that must be under the child's */` |
|       32 | 1463 | `			if( pUnres == 0 ){` |
|       32 | 1464 | `				pUnres = OvFirstUnres(&sP,&sC);` |
|       14 | 1465 | `			}` |
|       32 | 1466 | `			OvNoteUnres(&(*pGen),&sP,&sC);` |
|       14 | 1467 | `		}` |
|       72 | 1468 | `	}` |
|        - | 1469 | `	/* Return type -- covariant, and asked after the parameters, as php asks it. */` |
|      423 | 1470 | `	if( !bBad ){` |
|      405 | 1471 | `		OvFromReturn(pVm,pParentOwner,pPF,&sP);` |
|      405 | 1472 | `		OvFromReturn(pVm,pChildOwner,pCF,&sC);` |
|      405 | 1473 | `		iRes = OvCheck(&sP,&sC,/* bCovariant */ 1,pParentOwner,pChildOwner);` |
|      405 | 1474 | `		bBad = iRes == OV_BAD;` |
|      405 | 1475 | `		if( iRes == OV_UNRES ){` |
|       19 | 1476 | `			if( pUnres == 0 ){` |
|        3 | 1477 | `				pUnres = OvFirstUnres(&sC,&sP);` |
|        1 | 1478 | `			}` |
|       19 | 1479 | `			OvNoteUnres(&(*pGen),&sC,&sP);` |
|        8 | 1480 | `		}` |
|      200 | 1481 | `	}` |
|      423 | 1482 | `	if( !bBad && pUnres ){` |
|        - | 1483 | `		/* A pair only a class nothing has loaded can decide. While the file` |
|        - | 1484 | `		 * compiles that is php's obligation, settled where the declaration runs;` |
|        - | 1485 | `		 * once it runs, a class still missing is php's refusal to guess. */` |
|       34 | 1486 | `		if( !pGen->bObligeRun ){` |
|       26 | 1487 | `			OvRecordOblige(&(*pGen),pBase,pSub,(void *)pParent,(void *)pChild,0,bCtorExempt);` |
|       26 | 1488 | `			return SXRET_OK;` |
|        - | 1489 | `		}` |
|        - | 1490 | `		{` |
|        - | 1491 | `			SyBlob sChild, sParent;` |
|        - | 1492 | `			sxi32 rc;` |
|       12 | 1493 | `			SyBlobInit(&sChild,&pVm->sAllocator);` |
|       12 | 1494 | `			SyBlobInit(&sParent,&pVm->sAllocator);` |
|       12 | 1495 | `			PH7_ClassRenderDecl(pVm,pChildOwner,pCF,&sChild);` |
|       12 | 1496 | `			PH7_ClassRenderDecl(pVm,pParentOwner,pPF,&sParent);` |
|       16 | 1497 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pChild->nLine,` |
|        - | 1498 | `				"Could not check compatibility between %z::%z%.*s and %z::%z%.*s, because class %z is not available",` |
|        4 | 1499 | `				&pChildOwner->sDisp,pMName,` |
|        8 | 1500 | `				OoDeclCLen(&sChild),(const char *)SyBlobData(&sChild),` |
|        4 | 1501 | `				&pParentOwner->sName,&pParent->sFunc.sName,` |
|        8 | 1502 | `				OoDeclCLen(&sParent),(const char *)SyBlobData(&sParent),pUnres);` |
|       12 | 1503 | `			SyBlobRelease(&sChild);` |
|       12 | 1504 | `			SyBlobRelease(&sParent);` |
|       12 | 1505 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 1506 | `		}` |
|        - | 1507 | `	}` |
|      393 | 1508 | `	if( bBad ){` |
|        - | 1509 | `		SyBlob sChild, sParent;` |
|        - | 1510 | `		sxi32 rc;` |
|       38 | 1511 | `		SyBlobInit(&sChild,&pVm->sAllocator);` |
|       38 | 1512 | `		SyBlobInit(&sParent,&pVm->sAllocator);` |
|       38 | 1513 | `		PH7_ClassRenderDecl(pVm,pChildOwner,pCF,&sChild);` |
|       38 | 1514 | `		PH7_ClassRenderDecl(pVm,pParentOwner,pPF,&sParent);` |
|       55 | 1515 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pChild->nLine,` |
|        - | 1516 | `			"Declaration of %z::%z%.*s must be compatible with %z::%z%.*s",` |
|       17 | 1517 | `			&pChildOwner->sDisp,pMName,` |
|       34 | 1518 | `			OoDeclCLen(&sChild),(const char *)SyBlobData(&sChild),` |
|       17 | 1519 | `			&pParentOwner->sName,&pParent->sFunc.sName,` |
|       34 | 1520 | `			OoDeclCLen(&sParent),(const char *)SyBlobData(&sParent));` |
|       38 | 1521 | `		SyBlobRelease(&sChild);` |
|       38 | 1522 | `		SyBlobRelease(&sParent);` |
|       38 | 1523 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1524 | `			return SXERR_ABORT;` |
|        - | 1525 | `		}` |
|       17 | 1526 | `	}` |
|      393 | 1527 | `	return SXRET_OK;` |
|   384746 | 1528 | `}` |
|        - | 1529 | `/*` |
|        - | 1530 | ` * Every method the sub-INTERFACE declares ITSELF, judged against the same name in` |
|        - | 1531 | ` * one parent. php checks a restated interface method exactly as it checks an` |
|        - | 1532 | ` * overriding class method, and words the refusal the same way -- PHL checked` |
|        - | 1533 | ` * neither, and instead refused the restatement outright when it came from a` |
|        - | 1534 | ` * parent past the first (see the collected-parents comment in the interface` |
|        - | 1535 | ` * compiler).` |
|        - | 1536 | ` *` |
|        - | 1537 | ` * Called while hMethod still holds only this interface's own declarations, which` |
|        - | 1538 | ` * is the one moment the two sets are separable.` |
|        - | 1539 | ` */` |
|       78 | 1540 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceCheckRedeclare(ph7_gen_state *pGen,ph7_class *pSub,` |
|        - | 1541 | `	ph7_class *pParent)` |
|        5 | 1542 | `{` |
|        - | 1543 | `	SyHashEntry *pEntry;` |
|       83 | 1544 | `	SyHashResetLoopCursor(&pSub->hMethod);` |
|      164 | 1545 | `	while((pEntry = SyHashGetNextEntry(&pSub->hMethod)) != 0 ){` |
|       46 | 1546 | `		ph7_class_method *pOwn = (ph7_class_method *)pEntry->pUserData;` |
|       46 | 1547 | `		SyString *pName = &pOwn->sFunc.sName;` |
|       67 | 1548 | `		SyHashEntry *pUp = SyHashGet(&pParent->hMethod,` |
|       42 | 1549 | `			(const void *)pName->zString,pName->nByte);` |
|       54 | 1550 | `		if( pUp && PH7_ClassCheckOverrideCompat(&(*pGen),pParent,pSub,` |
|       24 | 1551 | `			(ph7_class_method *)pUp->pUserData,pOwn,0) == SXERR_ABORT ){` |
|      ! 0 | 1552 | `			return SXERR_ABORT;` |
|        - | 1553 | `		}` |
|        4 | 1554 | `	}` |
|       83 | 1555 | `	return SXRET_OK;` |
|       44 | 1556 | `}` |
|        - | 1557 | `/*` |
|        - | 1558 | ` * Perform an inheritance operation.` |
|        - | 1559 | ` * According to the PHP language reference manual` |
|        - | 1560 | ` *  When you extend a class, the subclass inherits all of the public and protected methods` |
|        - | 1561 | ` *  from the parent class. Unless a class Overwrites those methods, they will retain their original` |
|        - | 1562 | ` *  functionality.` |
|        - | 1563 | ` *  This is useful for defining and abstracting functionality, and permits the implementation` |
|        - | 1564 | ` *  of additional functionality in similar objects without the need to reimplement all of the shared` |
|        - | 1565 | ` *  functionality.` |
|        - | 1566 | ` *  Example #1 Inheritance Example` |
|        - | 1567 | ` * <?php` |
|        - | 1568 | ` * class foo` |
|        - | 1569 | ` * {` |
|        - | 1570 | ` *   public function printItem($string)` |
|        - | 1571 | ` *   {` |
|        - | 1572 | ` *       echo 'Foo: ' . $string . PHP_EOL;` |
|        - | 1573 | ` *   }` |
|        - | 1574 | ` *` |
|        - | 1575 | ` *   public function printPHP()` |
|        - | 1576 | ` *   {` |
|        - | 1577 | ` *       echo 'PHP is great.' . PHP_EOL;` |
|        - | 1578 | ` *   }` |
|        - | 1579 | ` * }` |
|        - | 1580 | ` * class bar extends foo` |
|        - | 1581 | ` * {` |
|        - | 1582 | ` *   public function printItem($string)` |
|        - | 1583 | ` *   {` |
|        - | 1584 | ` *       echo 'Bar: ' . $string . PHP_EOL;` |
|        - | 1585 | ` *   }` |
|        - | 1586 | ` * }` |
|        - | 1587 | ` * $foo = new foo();` |
|        - | 1588 | ` * $bar = new bar();` |
|        - | 1589 | ` * $foo->printItem('baz'); // Output: 'Foo: baz'` |
|        - | 1590 | ` * $foo->printPHP();       // Output: 'PHP is great'` |
|        - | 1591 | ` * $bar->printItem('baz'); // Output: 'Bar: baz'` |
|        - | 1592 | ` * $bar->printPHP();       // Output: 'PHP is great'` |
|        - | 1593 | ` *` |
|        - | 1594 | ` * This function return SXRET_OK if the inheritance operation was successfully performed.` |
|        - | 1595 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|        - | 1596 | ` * error message.` |
|        - | 1597 | ` */` |
|        - | 1598 | `/*` |
|        - | 1599 | ` * A redeclared property keeps every hook of its parent's that it does not` |
|        - | 1600 | ` * write itself -- php's inherit_property_hook. A hook is a method` |
|        - | 1601 | ` * (__phl_hook_get_NAME), so the method copy that follows brings the body` |
|        - | 1602 | ` * down already; what the redeclaration lost is the FLAG that makes an access` |
|        - | 1603 | ``  * dispatch it, and without it `class C extends P { public $x { set => ...; } }` `` |
|        - | 1604 | ` * read the backing store where php runs P's get. An abstract parent hook is` |
|        - | 1605 | ` * the one exception: a property that already performs the operation -- a` |
|        - | 1606 | ` * backed one always reads, and writes unless readonly -- satisfies it, and` |
|        - | 1607 | ` * nothing is inherited.` |
|        - | 1608 | ` *` |
|        - | 1609 | ` * A child over a BACKED parent is backed too, whatever its own hook bodies` |
|        - | 1610 | ` * reference: php keeps the parent's slot for it. Over a virtual parent the` |
|        - | 1611 | ` * child's own answer stands.` |
|        - | 1612 | ` */` |
|    33926 | 1613 | `static void OoInheritPropertyHooks(ph7_class *pBase,ph7_class_attr *pParent,ph7_class_attr *pChild)` |
|        5 | 1614 | `{` |
|        - | 1615 | `	static const sxi32 aKind[2] = { PH7_CLASS_ATTR_HOOK_GET, PH7_CLASS_ATTR_HOOK_SET };` |
|        - | 1616 | `	int i;` |
|    33931 | 1617 | `	if( (pParent->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL) == 0 ){` |
|    33895 | 1618 | `		pChild->iFlags &= ~PH7_CLASS_ATTR_HOOK_VIRTUAL;` |
|    16923 | 1619 | `	}` |
|   101783 | 1620 | `	for( i = 0 ; i < 2 ; i++ ){` |
|        - | 1621 | `		ph7_class_method *pHook;` |
|        - | 1622 | `		char zHName[128];` |
|        - | 1623 | `		sxu32 nHName;` |
|    67857 | 1624 | `		if( (pParent->iFlags & aKind[i]) == 0 \|\| (pChild->iFlags & aKind[i]) != 0 ){` |
|    67838 | 1625 | `			continue;` |
|        - | 1626 | `		}` |
|       40 | 1627 | `		nHName = SyBufferFormat(zHName,sizeof(zHName),i ? "__phl_hook_set_%z" : "__phl_hook_get_%z",` |
|       12 | 1628 | `			&pParent->sName);` |
|       28 | 1629 | `		pHook = PH7_ClassExtractMethod(pBase,zHName,nHName);` |
|       24 | 1630 | `		if( pHook && (pHook->iFlags & PH7_CLASS_ATTR_ABSTRACT)` |
|       17 | 1631 | `		 && (pChild->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL) == 0` |
|       14 | 1632 | `		 && (i == 0 \|\| (pChild->iFlags & PH7_CLASS_ATTR_READONLY) == 0) ){` |
|       13 | 1633 | `			continue;` |
|        - | 1634 | `		}` |
|       17 | 1635 | `		pChild->iFlags \|= aKind[i];` |
|       10 | 1636 | `	}` |
|    33931 | 1637 | `}` |
|        - | 1638 | `/*` |
|        - | 1639 | ` * A property's SET visibility as php's flags hold it, on the PH7_CLASS_PROT_*` |
|        - | 1640 | ` * scale, or 0 when it has none: an explicit private(set)/protected(set) (the` |
|        - | 1641 | ` * compiler already dropped one equal to the read visibility), else the` |
|        - | 1642 | ` * protected(set) php gives a public readonly property nothing else spelled.` |
|        - | 1643 | ` */` |
|    17116 | 1644 | `static sxi32 OoPropSetLevel(const ph7_class_attr *pAttr)` |
|        5 | 1645 | `{` |
|    17121 | 1646 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET ){` |
|       17 | 1647 | `		return PH7_CLASS_PROT_PRIVATE;` |
|        - | 1648 | `	}` |
|    17107 | 1649 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_PROTECTED_SET ){` |
|       15 | 1650 | `		return PH7_CLASS_PROT_PROTECTED;` |
|        - | 1651 | `	}` |
|    17088 | 1652 | `	if( (pAttr->iFlags & (PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_PUBLIC_SET)) == PH7_CLASS_ATTR_READONLY` |
|     8541 | 1653 | `	 && pAttr->iProtection == PH7_CLASS_PROT_PUBLIC ){` |
|        9 | 1654 | `		return PH7_CLASS_PROT_PROTECTED;` |
|        - | 1655 | `	}` |
|    17087 | 1656 | `	return 0;` |
|     8552 | 1657 | `}` |
|        - | 1658 | `/*` |
|        - | 1659 | ` * php's do_inherit_property screen for a property the subclass REDECLARES over a` |
|        - | 1660 | ` * non-private one of its base, in php's order: static-ness, readonly-ness, the` |
|        - | 1661 | ` * set access level, the access level, then the declared type. A child may add a` |
|        - | 1662 | ` * set visibility only as far as the parent's -- its explicit one, else its read` |
|        - | 1663 | ` * visibility -- and anything under a get-only virtual parent, which has no set. Every one is reported on the subclass's` |
|        - | 1664 | ` * line and names the class that DECLARED the parent property.` |
|        - | 1665 | ` *` |
|        - | 1666 | ``  * The type is INVARIANT -- `int` over `?int` is refused as surely as `string` `` |
|        - | 1667 | `` * over `int` -- except under a virtual hooked parent with one hook: a get-only`` |
|        - | 1668 | ` * one may only be narrowed (covariant), a set-only one only widened. Both` |
|        - | 1669 | ``  * directions go through the override lattice, so `string\|int` over `int\|string` `` |
|        - | 1670 | ` * is the same type and a shape it cannot model is accepted. A class nothing has` |
|        - | 1671 | ` * loaded leaves an invariant pair UNRESOLVED unless its name alone answers it,` |
|        - | 1672 | ` * and php's obligation then refuses it: no class loaded later is both under and` |
|        - | 1673 | ` * over a differently named one. A variant pair it leaves open is accepted, since` |
|        - | 1674 | ` * a class declared further down the file may still answer it. An untyped side is` |
|        - | 1675 | `` * an ABSENCE, never `mixed`: an untyped child under a typed parent is refused`` |
|        - | 1676 | `` * (`mixed` included), and a typed one under an untyped parent has its own`` |
|        - | 1677 | ` * sentence.` |
|        - | 1678 | ` */` |
|    17102 | 1679 | `static sxi32 OoCheckPropRedeclare(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase,` |
|        - | 1680 | `	ph7_class_attr *pParent,ph7_class_attr *pChild)` |
|        5 | 1681 | `{` |
|        - | 1682 | `	static const char *azProt[] = { "", "public", "protected", "private" };` |
|    17107 | 1683 | `	ph7_class *pOwner = PH7_VmMemberOwnerClass(pParent->pDeclClass,pBase);` |
|        - | 1684 | `	/* The class that declared the CHILD property: the subclass for a` |
|        - | 1685 | `	 * redeclaration, but an inherited one answering an interface keeps its own. */` |
|    17107 | 1686 | `	ph7_class *pChildOwner = PH7_VmMemberOwnerClass(pChild->pDeclClass,pSub);` |
|    17107 | 1687 | `	const SyString *pName = &pParent->sName;` |
|    17107 | 1688 | `	sxi32 iPS = pParent->iFlags & PH7_CLASS_ATTR_STATIC;` |
|    17107 | 1689 | `	sxi32 iCS = pChild->iFlags & PH7_CLASS_ATTR_STATIC;` |
|    17107 | 1690 | `	if( iPS != iCS ){` |
|        7 | 1691 | `		return PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|        - | 1692 | `			"Cannot redeclare %s%z::$%z as %s%z::$%z",` |
|        2 | 1693 | `			iPS ? "static " : "non static ",&pOwner->sDisp,pName,` |
|        2 | 1694 | `			iCS ? "static " : "non static ",&pSub->sDisp,pName);` |
|        - | 1695 | `	}` |
|    17098 | 1696 | `	if( (pParent->iFlags & PH7_CLASS_ATTR_READONLY) != (pChild->iFlags & PH7_CLASS_ATTR_READONLY)` |
|     8545 | 1697 | `	 && (pParent->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|        3 | 1698 | `		return PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|        - | 1699 | `			"Cannot redeclare %s property %z::$%z as %s %z::$%z",` |
|        2 | 1700 | `			(pParent->iFlags & PH7_CLASS_ATTR_READONLY) ? "readonly" : "non-readonly",&pOwner->sDisp,pName,` |
|        2 | 1701 | `			(pChild->iFlags & PH7_CLASS_ATTR_READONLY) ? "readonly" : "non-readonly",&pSub->sDisp,pName);` |
|        - | 1702 | `	}` |
|    17096 | 1703 | `	if( OoPropSetLevel(pChild) != 0` |
|     8551 | 1704 | `	 && (pParent->iFlags & (PH7_CLASS_ATTR_HOOK_VIRTUAL\|PH7_CLASS_ATTR_HOOK_SET)) != PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|       13 | 1705 | `		sxi32 iPSet = OoPropSetLevel(pParent);` |
|       13 | 1706 | `		if( OoPropSetLevel(pChild) > (iPSet ? iPSet : pParent->iProtection) ){` |
|       10 | 1707 | `			return PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|        - | 1708 | `				"Set access level of %z::$%z must be %s (as in class %z)%s",` |
|        2 | 1709 | `				&pSub->sDisp,pName,` |
|        4 | 1710 | `				iPSet == PH7_CLASS_PROT_PRIVATE ? "private(set)" : iPSet ? "protected(set)" : "omitted",` |
|        2 | 1711 | `				&pOwner->sDisp,iPSet ? " or weaker" : "");` |
|        - | 1712 | `		}` |
|        3 | 1713 | `	}` |
|    17092 | 1714 | `	if( pChild->iProtection > pParent->iProtection && pParent->iProtection >= PH7_CLASS_PROT_PUBLIC` |
|        9 | 1715 | `	 && pParent->iProtection <= PH7_CLASS_PROT_PRIVATE ){` |
|       10 | 1716 | `		return PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|        - | 1717 | `			"Access level to %z::$%z must be %s (as in class %z)%s",` |
|        4 | 1718 | `			&pSub->sDisp,pName,azProt[pParent->iProtection],&pOwner->sDisp,` |
|        4 | 1719 | `			pParent->iProtection == PH7_CLASS_PROT_PUBLIC ? "" : " or weaker");` |
|        - | 1720 | `	}` |
|    17093 | 1721 | `	if( pParent->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|      159 | 1722 | `		ph7_vm *pVm = pGen->pVm;` |
|      159 | 1723 | `		ph7_class *pPScope = VmHintScopeDeclared(pParent->pDeclClass);` |
|        - | 1724 | `		/* 1: get-only virtual parent (covariant), 2: set-only (contravariant) */` |
|      159 | 1725 | `		int iVariance = 0;` |
|        - | 1726 | `		int iCo, iContra;` |
|        - | 1727 | `		OvType sP, sC;` |
|      154 | 1728 | `		if( (pParent->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL)` |
|      126 | 1729 | `		 && (pParent->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) ){` |
|       93 | 1730 | `			if( (pParent->iFlags & PH7_CLASS_ATTR_HOOK_SET) == 0 ){` |
|       69 | 1731 | `				iVariance = 1;` |
|       60 | 1732 | `			}else if( (pParent->iFlags & PH7_CLASS_ATTR_HOOK_GET) == 0 ){` |
|       16 | 1733 | `				iVariance = 2;` |
|        6 | 1734 | `			}` |
|       44 | 1735 | `		}` |
|      236 | 1736 | `		OvFromDecl(pVm,pPScope,&sP,pParent->nType,&pParent->sClass,&pParent->aUnionAlts,` |
|      154 | 1737 | `			(pParent->iFlags & PH7_CLASS_ATTR_NULLABLE) != 0);` |
|      159 | 1738 | `		if( pChild->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|      233 | 1739 | `			OvFromDecl(pVm,pChildOwner,&sC,pChild->nType,&pChild->sClass,&pChild->aUnionAlts,` |
|      152 | 1740 | `				(pChild->iFlags & PH7_CLASS_ATTR_NULLABLE) != 0);` |
|       81 | 1741 | `		}else{` |
|        3 | 1742 | `			OvInit(&sC);` |
|        3 | 1743 | `			sC.bAbsent = 1;` |
|        - | 1744 | `		}` |
|      159 | 1745 | `		iCo = iVariance == 2 ? OV_OK : OvCheck(&sP,&sC,1,pPScope,pChildOwner);` |
|      159 | 1746 | `		iContra = iVariance == 1 ? OV_OK : sC.bAbsent ? OV_BAD : OvCheck(&sP,&sC,0,pPScope,pChildOwner);` |
|      154 | 1747 | `		if( iVariance != 0 && iCo != OV_BAD && iContra != OV_BAD` |
|       68 | 1748 | `		 && (iCo == OV_UNRES \|\| iContra == OV_UNRES) && !pGen->bObligeRun ){` |
|        - | 1749 | `			/* A variant pair a later class may still answer: php's obligation,` |
|        - | 1750 | `			 * settled where the declaration runs (and refused there, in this same` |
|        - | 1751 | `			 * sentence, if the class is still missing). */` |
|       11 | 1752 | `			if( iVariance == 1 ){` |
|        8 | 1753 | `				OvNoteUnres(&(*pGen),&sC,&sP);` |
|        5 | 1754 | `			}else{` |
|        3 | 1755 | `				OvNoteUnres(&(*pGen),&sP,&sC);` |
|        - | 1756 | `			}` |
|       11 | 1757 | `			OvRecordOblige(&(*pGen),pBase,pSub,(void *)pParent,(void *)pChild,1,0);` |
|       26 | 1758 | `			return SXRET_OK;` |
|        - | 1759 | `		}` |
|      151 | 1760 | `		if( iCo == OV_BAD \|\| iContra == OV_BAD \|\| iCo == OV_UNRES \|\| iContra == OV_UNRES ){` |
|        - | 1761 | `			char zType[192];` |
|       49 | 1762 | `			const char *zTypeText = VmHintTextResolved(pVm,&pParent->sTypeName,pPScope,` |
|       15 | 1763 | `				zType,sizeof(zType));` |
|       57 | 1764 | `			return PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|       15 | 1765 | `				"Type of %z::$%z must be %s%s (as in class %z)",&pChildOwner->sDisp,pName,` |
|       23 | 1766 | `				iVariance == 1 ? "subtype of " : iVariance == 2 ? "supertype of " : "",` |
|       15 | 1767 | `				zTypeText,&pOwner->sDisp);` |
|        5 | 1768 | `		}` |
|    16997 | 1769 | `	}else if( pChild->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        7 | 1770 | `		return PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|        - | 1771 | `			"Type of %z::$%z must be omitted to match the parent definition in class %z",` |
|        2 | 1772 | `			&pSub->sDisp,pName,&pOwner->sDisp);` |
|        - | 1773 | `	}` |
|    17051 | 1774 | `	return SXRET_OK;` |
|     8545 | 1775 | `}` |
|        - | 1776 | `/*` |
|        - | 1777 | ` * php's load_delayed_classes: a loader that throws while a declaration autoloads` |
|        - | 1778 | ` * the names its variance pairs left open is not an exception anybody may catch --` |
|        - | 1779 | ` * the class is half-linked by then -- but zend_exception_uncaught_error's E_ERROR,` |
|        - | 1780 | `` * `During inheritance of C, while autoloading X: Uncaught <the exception as a`` |
|        - | 1781 | `` * string>` (its __toString(), so a `$previous` chain and a user override both`` |
|        - | 1782 | ` * show), reported at the declaration, and the script ends. Takes the reference` |
|        - | 1783 | ` * pExc carries.` |
|        - | 1784 | ` */` |
|        6 | 1785 | `static void OoObligeLoaderFatal(ph7_vm *pVm,VmClassObligeSet *pSet,const SyString *pName,` |
|        - | 1786 | `	ph7_class_instance *pExc)` |
|        3 | 1787 | `{` |
|        9 | 1788 | `	VmClassOblige *pRec = (VmClassOblige *)SySetBasePtr(&pSet->aOblige);` |
|        - | 1789 | `	ph7_value sStr;` |
|        9 | 1790 | `	sxu32 nDisp = 0;` |
|        9 | 1791 | `	PH7_MemObjInit(&(*pVm),&sStr);` |
|        9 | 1792 | `	sStr.x.pOther = pExc; /* the value takes the fence's reference */` |
|        9 | 1793 | `	MemObjSetType(&sStr,MEMOBJ_OBJ);` |
|        9 | 1794 | `	if( PH7_MemObjToString(&sStr) != SXRET_OK \|\| (sStr.iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 | 1795 | `		PH7_MemObjRelease(&sStr);` |
|      ! 0 | 1796 | `		PH7_MemObjInit(&(*pVm),&sStr);` |
|      ! 0 | 1797 | `		MemObjSetType(&sStr,MEMOBJ_STRING);` |
|      ! 0 | 1798 | `	}` |
|        - | 1799 | `	/* php prints the name with %s: an anonymous class's stops at its NUL. */` |
|       51 | 1800 | `	while( nDisp < pRec->pSub->sDisp.nByte && pRec->pSub->sDisp.zString[nDisp] != 0 ){` |
|       45 | 1801 | `		nDisp++;` |
|        3 | 1802 | `	}` |
|       12 | 1803 | `	PH7_VmFatalError(&(*pVm),"During inheritance of %.*s, while autoloading %z: Uncaught %.*s",` |
|        6 | 1804 | `		(int)nDisp,pRec->pSub->sDisp.zString,pName,` |
|        6 | 1805 | `		(int)SyBlobLength(&sStr.sBlob),(const char *)SyBlobData(&sStr.sBlob));` |
|        9 | 1806 | `	PH7_MemObjRelease(&sStr);` |
|        9 | 1807 | `	pVm->iExitStatus = 255;` |
|        9 | 1808 | `	pVm->bHaltRequested = 1;` |
|        9 | 1809 | `}` |
|        - | 1810 | `/*` |
|        - | 1811 | ` * PH7_OP_CLASS_OBLIGE: where a class with unresolved variance pairs is DECLARED,` |
|        - | 1812 | ` * php autoloads every name its checks could not find, in the order they were` |
|        - | 1813 | ` * asked for, and checks each pair again -- now with nothing left to hope for, so` |
|        - | 1814 | ` * a class still missing refuses the pair in the same compile fatal (methods say` |
|        - | 1815 | ` * which class they could not check against). The generator is the VM's own; a` |
|        - | 1816 | ` * refusal halts the script as the compile fatal it is.` |
|        - | 1817 | ` */` |
|       30 | 1818 | `PH7_PRIVATE sxi32 PH7_ClassSettleObligations(ph7_vm *pVm,VmClassObligeSet *pSet)` |
|        4 | 1819 | `{` |
|       34 | 1820 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|        - | 1821 | `	VmClassOblige *aRec;` |
|        - | 1822 | `	SyString *aName;` |
|        - | 1823 | `	sxu32 n, nErr;` |
|        - | 1824 | `	struct VmClassObligeSet *pSavedOblige;` |
|        - | 1825 | `	ProcConsumer xSavedErr;` |
|        - | 1826 | `	void *pSavedErrData;` |
|        - | 1827 | `	int bSavedRun;` |
|       34 | 1828 | `	sxi32 rc = SXRET_OK;` |
|       34 | 1829 | `	if( pSet->bDone ){` |
|      ! 0 | 1830 | `		return SXRET_OK;` |
|        - | 1831 | `	}` |
|       34 | 1832 | `	pSet->bDone = 1;` |
|       34 | 1833 | `	aName = (SyString *)SySetBasePtr(&pSet->aName);` |
|       86 | 1834 | `	for( n = 0 ; n < SySetUsed(&pSet->aName) ; ++n ){` |
|       62 | 1835 | `		sxi32 nBrc = pVm->nBoundaryRc;` |
|       62 | 1836 | `		const void *pResume = (const void *)pVm->pResumeFrame;` |
|       62 | 1837 | `		sxu32 nFenceIn = pVm->nThrowFence;` |
|       62 | 1838 | `		ph7_class_instance *pExcIn = pVm->pFencedExc;` |
|        - | 1839 | `		ph7_class_instance *pExc;` |
|        - | 1840 | `		/* Behind a throw fence: php never lets a try around the declaration` |
|        - | 1841 | `		 * catch what a loader throws here, and runs none of its finally blocks` |
|        - | 1842 | `		 * either -- the throw becomes an uncaught-error fatal (below). Tries` |
|        - | 1843 | `		 * INSIDE the loader still catch normally. */` |
|       62 | 1844 | `		pVm->pFencedExc = 0;` |
|       62 | 1845 | `		pVm->nThrowFence = SySetUsed(&pVm->aException) + 1;` |
|       62 | 1846 | `		PH7_VmExtractClass(&(*pVm),aName[n].zString,aName[n].nByte,FALSE,0);` |
|       62 | 1847 | `		pVm->nThrowFence = nFenceIn;` |
|       62 | 1848 | `		pExc = pVm->pFencedExc;` |
|       62 | 1849 | `		pVm->pFencedExc = pExcIn;` |
|       62 | 1850 | `		if( pExc && pVm->nBoundaryRc == PH7_ABORT && nBrc != PH7_ABORT ){` |
|      ! 0 | 1851 | `			PH7_ClassInstanceUnref(pExc);` |
|      ! 0 | 1852 | `			pExc = 0;` |
|      ! 0 | 1853 | `		}` |
|       62 | 1854 | `		if( pExc ){` |
|        9 | 1855 | `			pVm->nBoundaryRc = nBrc; /* nothing was caught in place: nothing to route */` |
|        9 | 1856 | `			OoObligeLoaderFatal(&(*pVm),pSet,&aName[n],pExc);` |
|        9 | 1857 | `			return SXERR_ABORT;` |
|        - | 1858 | `		}` |
|       56 | 1859 | `		if( PH7_VmClassLookupRaised(&(*pVm),nBrc,pResume) ){` |
|        - | 1860 | `			/* The loader exited: no later name is asked for. */` |
|      ! 0 | 1861 | `			return SXRET_OK;` |
|        - | 1862 | `		}` |
|       30 | 1863 | `	}` |
|       28 | 1864 | `	pSavedOblige = pGen->pOblige;` |
|       28 | 1865 | `	bSavedRun = pGen->bObligeRun;` |
|       28 | 1866 | `	nErr = pGen->nErr;` |
|       28 | 1867 | `	xSavedErr = pGen->xErr;` |
|       28 | 1868 | `	pSavedErrData = pGen->pErrData;` |
|       28 | 1869 | `	pGen->pOblige = 0;` |
|       28 | 1870 | `	pGen->bObligeRun = 1;` |
|       28 | 1871 | `	pGen->nErr = 0; /* a refusal is this statement's own, never a stale unit's next */` |
|        - | 1872 | `	/* An eval() leaves the generator with no consumer; this refusal still prints,` |
|        - | 1873 | `	 * and from the declaration's activation, which is on php's trace. */` |
|       28 | 1874 | `	pGen->xErr = pVm->pEngine->xConf.xErr;` |
|       28 | 1875 | `	pGen->pErrData = pVm->pEngine->xConf.pErrData;` |
|       28 | 1876 | `	pGen->iFatalTrace = PH7_FATAL_TRACE_RUNTIME;` |
|       28 | 1877 | `	aRec = (VmClassOblige *)SySetBasePtr(&pSet->aOblige);` |
|       52 | 1878 | `	for( n = 0 ; n < SySetUsed(&pSet->aOblige) && pGen->nErr == 0 ; ++n ){` |
|       28 | 1879 | `		if( aRec[n].bProp ){` |
|       12 | 1880 | `			rc = OoCheckPropRedeclare(&(*pGen),aRec[n].pSub,aRec[n].pBase,` |
|        6 | 1881 | `				(ph7_class_attr *)aRec[n].pParent,(ph7_class_attr *)aRec[n].pChild);` |
|        6 | 1882 | `		}else{` |
|       31 | 1883 | `			rc = PH7_ClassCheckOverrideCompat(&(*pGen),aRec[n].pBase,aRec[n].pSub,` |
|       18 | 1884 | `				(ph7_class_method *)aRec[n].pParent,(ph7_class_method *)aRec[n].pChild,` |
|       18 | 1885 | `				aRec[n].bCtorExempt);` |
|        - | 1886 | `		}` |
|       28 | 1887 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1888 | `			break;` |
|        - | 1889 | `		}` |
|       16 | 1890 | `	}` |
|       28 | 1891 | `	if( pGen->nErr > 0 ){` |
|       18 | 1892 | `		rc = SXERR_ABORT;` |
|        7 | 1893 | `	}` |
|       28 | 1894 | `	pGen->nErr = nErr;` |
|       28 | 1895 | `	pGen->pOblige = pSavedOblige;` |
|       28 | 1896 | `	pGen->bObligeRun = bSavedRun;` |
|       28 | 1897 | `	pGen->xErr = xSavedErr;` |
|       28 | 1898 | `	pGen->pErrData = pSavedErrData;` |
|       28 | 1899 | `	pGen->iFatalTrace = PH7_FATAL_TRACE_COMPILE;` |
|       28 | 1900 | `	if( rc == SXERR_ABORT ){` |
|       18 | 1901 | `		pVm->iExitStatus = 255;` |
|       18 | 1902 | `		pVm->bHaltRequested = 1;` |
|       18 | 1903 | `		return SXERR_ABORT;` |
|        - | 1904 | `	}` |
|       11 | 1905 | `	return SXRET_OK;` |
|       19 | 1906 | `}` |
|        - | 1907 | `/*` |
|        - | 1908 | ` * php's do_interface_implementation runs the same screen for every property an` |
|        - | 1909 | ` * interface declares, against whatever the class holds under that name by then:` |
|        - | 1910 | ` * its own, one composed from a trait, or one inherited from its parent (which is` |
|        - | 1911 | ` * then named as the declaring class). An interface's properties are never copied` |
|        - | 1912 | ` * into the class, so the screen walks the interface and the interfaces it extends.` |
|        - | 1913 | ` *` |
|        - | 1914 | ` * A property the parent already holds, under an interface the parent already` |
|        - | 1915 | ` * implements, was answered when the parent was linked, and a redeclaration of it` |
|        - | 1916 | ` * was answered against the parent's by PH7_ClassInherit.` |
|        - | 1917 | ` */` |
|        - | 1918 | `#define OO_IFACE_WALK_MAX_DEPTH 64 /* as the instanceof walk's bound */` |
|     1874 | 1919 | `static sxi32 OoCheckIfacePropsOf(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pIface,int iDepth)` |
|        5 | 1920 | `{` |
|     4377 | 1921 | `	while( pIface && iDepth <= OO_IFACE_WALK_MAX_DEPTH ){` |
|     2503 | 1922 | `		ph7_class **apParent = (ph7_class **)SySetBasePtr(&pIface->aInterface);` |
|        - | 1923 | `		SyHashEntry *pEntry;` |
|        - | 1924 | `		sxu32 n;` |
|     2503 | 1925 | `		SyHashResetLoopCursor(&pIface->hAttr);` |
|     2571 | 1926 | `		while((pEntry = SyHashGetNextEntry(&pIface->hAttr)) != 0 ){` |
|       73 | 1927 | `			ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|        - | 1928 | `			ph7_class_attr *pChild;` |
|        - | 1929 | `			SyHashEntry *pOwn;` |
|       73 | 1930 | `			if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_HIDDEN) ){` |
|      ! 0 | 1931 | `				continue;` |
|        - | 1932 | `			}` |
|      107 | 1933 | `			pOwn = SyHashGet(&pSub->hAttr,(const void *)SyStringData(&pAttr->sName),` |
|       34 | 1934 | `				SyStringLength(&pAttr->sName));` |
|       73 | 1935 | `			if( pOwn == 0 ){` |
|        3 | 1936 | `				continue;` |
|        - | 1937 | `			}` |
|       71 | 1938 | `			pChild = (ph7_class_attr *)pOwn->pUserData;` |
|       71 | 1939 | `			if( pChild == pAttr \|\| (pChild->iFlags & PH7_CLASS_ATTR_CONSTANT) ){` |
|      ! 0 | 1940 | `				continue;` |
|        - | 1941 | `			}` |
|       66 | 1942 | `			if( pSub->pBase && (pSub->iFlags & PH7_CLASS_INTERFACE) == 0` |
|       19 | 1943 | `			 && PH7_VmInstanceOf(pSub->pBase,pIface) ){` |
|       16 | 1944 | `				SyHashEntry *pUp = SyHashGet(&pSub->pBase->hAttr,` |
|       10 | 1945 | `					(const void *)SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName));` |
|       10 | 1946 | `				if( pUp && (((ph7_class_attr *)pUp->pUserData)->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0` |
|        9 | 1947 | `				 && ((ph7_class_attr *)pUp->pUserData)->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
|        9 | 1948 | `					continue;` |
|        - | 1949 | `				}` |
|        1 | 1950 | `			}` |
|       63 | 1951 | `			if( OoCheckPropRedeclare(&(*pGen),pSub,pIface,pAttr,pChild) == SXERR_ABORT ){` |
|      ! 0 | 1952 | `				return SXERR_ABORT;` |
|        - | 1953 | `			}` |
|        5 | 1954 | `		}` |
|     2519 | 1955 | `		for( n = 0 ; n < SySetUsed(&pIface->aInterface) ; n++ ){` |
|       18 | 1956 | `			if( OoCheckIfacePropsOf(&(*pGen),pSub,apParent[n],iDepth+1) == SXERR_ABORT ){` |
|      ! 0 | 1957 | `				return SXERR_ABORT;` |
|        - | 1958 | `			}` |
|       10 | 1959 | `		}` |
|     2503 | 1960 | `		pIface = pIface->pBase;` |
|     2503 | 1961 | `		iDepth++;` |
|        5 | 1962 | `	}` |
|     1879 | 1963 | `	return SXRET_OK;` |
|      942 | 1964 | `}` |
|     6340 | 1965 | `PH7_PRIVATE sxi32 PH7_ClassCheckInterfaceProps(ph7_gen_state *pGen,ph7_class *pSub)` |
|        5 | 1966 | `{` |
|        - | 1967 | `	ph7_class *pClass;` |
|        - | 1968 | `	/* The class's own interfaces, then every one its ancestors implement: a` |
|        - | 1969 | `	 * property the parent lacks still answers an interface the parent took. */` |
|    14397 | 1970 | `	for( pClass = pSub ; pClass ; pClass = pClass->pBase ){` |
|     8057 | 1971 | `		ph7_class **apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|        - | 1972 | `		sxu32 n;` |
|     9915 | 1973 | `		for( n = 0 ; n < SySetUsed(&pClass->aInterface) ; n++ ){` |
|     1863 | 1974 | `			if( OoCheckIfacePropsOf(&(*pGen),pSub,apIface[n],0) == SXERR_ABORT ){` |
|      ! 0 | 1975 | `				return SXERR_ABORT;` |
|        - | 1976 | `			}` |
|      934 | 1977 | `		}` |
|     4031 | 1978 | `	}` |
|     6345 | 1979 | `	return SXRET_OK;` |
|     3175 | 1980 | `}` |
|   955699 | 1981 | `PH7_PRIVATE sxi32 PH7_ClassInherit(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase)` |
|        5 | 1982 | `{` |
|        - | 1983 | `	ph7_class_method *pMeth;` |
|        - | 1984 | `	ph7_class_attr *pAttr;` |
|        - | 1985 | `	SyHashEntry *pEntry;` |
|        - | 1986 | `	SyString *pName;` |
|        - | 1987 | `	SySet aInherited; /* base attributes to prepend (see the copy loop below) */` |
|        - | 1988 | `	sxi32 rc;` |
|   955704 | 1989 | `	SySetInit(&aInherited,&pGen->pVm->sAllocator,sizeof(ph7_class_attr *));` |
|        - | 1990 | `	/* Install in the derived hashtable */` |
|   955704 | 1991 | `	rc = SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|   955704 | 1992 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1993 | `		SySetRelease(&aInherited);` |
|      ! 0 | 1994 | `		return rc;` |
|        - | 1995 | `	}` |
|        - | 1996 | `	/* readonly class inheritance (PHP 8.2): a readonly class may only extend a` |
|        - | 1997 | `	 * readonly class, and a non-readonly class may not extend a readonly one.` |
|        - | 1998 | `	 * A FINAL base is not one of these cases at all -- it cannot be extended by` |
|        - | 1999 | `	 * anything, and php reports only that. Both diagnostics used to fire for a` |
|        - | 2000 | ``	 * `final readonly` base and the readonly one was reported, which is the wrong`` |
|        - | 2001 | `	 * reason; BcMath\Number is the engine's first such class. */` |
|   955699 | 2002 | `	if( (pBase->iFlags & PH7_CLASS_FINAL) == 0` |
|   955702 | 2003 | `	 && (pBase->iFlags & PH7_CLASS_READONLY) != (pSub->iFlags & PH7_CLASS_READONLY) ){` |
|        5 | 2004 | `		if( pBase->iFlags & PH7_CLASS_READONLY ){` |
|        4 | 2005 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|        - | 2006 | `				"Non-readonly class %z cannot extend readonly class %z",` |
|        1 | 2007 | `				&pSub->sDisp,&pBase->sDisp);` |
|        2 | 2008 | `		}else{` |
|        4 | 2009 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|        - | 2010 | `				"Readonly class %z cannot extend non-readonly class %z",` |
|        1 | 2011 | `				&pSub->sDisp,&pBase->sDisp);` |
|        - | 2012 | `		}` |
|        5 | 2013 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2014 | `			SySetRelease(&aInherited);` |
|      ! 0 | 2015 | `			return SXERR_ABORT;` |
|        - | 2016 | `		}` |
|        2 | 2017 | `	}` |
|        - | 2018 | `	/* Mark as subclass BEFORE the members are copied. php's mangled storage name` |
|        - | 2019 | `	 * for a TRAIT-composed private names the class that composed it, found by` |
|        - | 2020 | `	 * walking the subclass's ANCESTRY (PH7_VmMemberOwnerClass) -- with pBase still` |
|        - | 2021 | `	 * unset the walk stopped at the trait, cached that answer on the attribute,` |
|        - | 2022 | `	 * and every later lookup then asked for a key the object's table did not hold. */` |
|   955704 | 2023 | `	pSub->pBase = pBase;` |
|        - | 2024 | `	/* A native class whose php-visible properties are LAZY passes that on: the` |
|        - | 2025 | `	 * attributes copied below keep their flags, so a subclass of DateInterval has` |
|        - | 2026 | `	 * the same ten to install, and the O(1) gate in front of the materialization` |
|        - | 2027 | `	 * walk has to see it on the SUBCLASS or the constructor's writes land nowhere. */` |
|   955704 | 2028 | `	if( pBase->iFlags & PH7_CLASS_LAZY_ATTR ){` |
|       27 | 2029 | `		pSub->iFlags \|= PH7_CLASS_LAZY_ATTR;` |
|       13 | 2030 | `	}` |
|        - | 2031 | `	/* Copy public/protected attributes from the base class */` |
|   955704 | 2032 | `	SyHashResetLoopCursor(&pBase->hAttr);` |
| 10680319 | 2033 | `	while((pEntry = SyHashGetNextEntry(&pBase->hAttr)) != 0 ){` |
|        - | 2034 | `		/* Make sure the private attributes are not redeclared in the subclass */` |
|  9724620 | 2035 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  9724620 | 2036 | `		pName = &pAttr->sName;` |
|  9724615 | 2037 | `		if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|  6180021 | 2038 | `		 && (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - | 2039 | `			/* A base's private INSTANCE property is a slot of its own on every` |
|        - | 2040 | `			 * object below it, filed under php's mangled storage name -- so it can` |
|        - | 2041 | `			 * never collide with a subclass member of the same name, and the` |
|        - | 2042 | `			 * redeclaration rules below have nothing to say about it. The subclass` |
|        - | 2043 | `			 * keeps its own declaration exactly where it wrote it. */` |
|  2644628 | 2044 | `			rc = SySetPut(&aInherited,(const void *)&pAttr);` |
|  2644628 | 2045 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 2046 | `				SySetRelease(&aInherited);` |
|      ! 0 | 2047 | `				return rc;` |
|        - | 2048 | `			}` |
|  2644628 | 2049 | `			continue;` |
|        - | 2050 | `		}` |
|  7079997 | 2051 | `		if( (pEntry = SyHashGet(&pSub->hAttr,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|    33936 | 2052 | `			if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_FINAL))` |
|    16951 | 2053 | `				== (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_FINAL) ){` |
|        - | 2054 | `				/* Cannot override a final class constant (PHP 8.1). Report the` |
|        - | 2055 | `				 * class that originally declared it (pDeclClass) rather than the` |
|        - | 2056 | `				 * immediate base, so a multi-level chain matches PHP -- and a` |
|        - | 2057 | `				 * TRAIT-declared one belongs to the class that composed it.` |
|        - | 2058 | `				 * php reports it on the SUBCLASS's declaration line, not on the` |
|        - | 2059 | `				 * line the offending member sits on: the refusal is inheritance` |
|        - | 2060 | ``				 * talking, and inheritance happens where `extends` is written.`` |
|        - | 2061 | `				 * (Its final-METHOD twin below is the other rule -- php reports` |
|        - | 2062 | `				 * THAT one at the method.) */` |
|      ! 0 | 2063 | `				ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pBase);` |
|      ! 0 | 2064 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|        - | 2065 | `					"%z::%z cannot override final constant %z::%z",` |
|      ! 0 | 2066 | `					&pSub->sDisp,pName,&pOwner->sDisp,pName);` |
|      ! 0 | 2067 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2068 | `					SySetRelease(&aInherited);` |
|      ! 0 | 2069 | `					return SXERR_ABORT;` |
|        - | 2070 | `				}` |
|    33936 | 2071 | `			}else if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0` |
|    33945 | 2072 | `				&& (pAttr->iFlags & (PH7_CLASS_ATTR_FINAL\|PH7_CLASS_ATTR_PRIVATE_SET)) ){` |
|        - | 2073 | `				/* PHP 8.4's final PROPERTY: no subclass may redeclare it, however the` |
|        - | 2074 | `				 * redeclaration is spelled -- a plain or static property of its own, a` |
|        - | 2075 | `				 * PROMOTED constructor parameter, or a trait it composes -- because all` |
|        - | 2076 | `				 * three land in the subclass's attribute table before inheritance runs.` |
|        - | 2077 | `				 * A private(set) one is final without saying so: php sets the flag.` |
|        - | 2078 | `				 * Same class-line rule and same declaring-class naming as the constant` |
|        - | 2079 | `				 * above. */` |
|       11 | 2080 | `				ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pBase);` |
|       15 | 2081 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|        4 | 2082 | `					"Cannot override final property %z::$%z",&pOwner->sDisp,pName);` |
|       11 | 2083 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2084 | `					SySetRelease(&aInherited);` |
|      ! 0 | 2085 | `					return SXERR_ABORT;` |
|        - | 2086 | `				}` |
|    33935 | 2087 | `			}else if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_HIDDEN)) == 0` |
|    25472 | 2088 | `			 && pAttr->iProtection != PH7_CLASS_PROT_PRIVATE` |
|    17043 | 2089 | `			 && (((ph7_class_attr *)pEntry->pUserData)->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        - | 2090 | `				/* A redeclaration of a base property php can see. A native class's` |
|        - | 2091 | `				 * HIDDEN engine slot is not one: php declares no such property. */` |
|    17043 | 2092 | `				rc = OoCheckPropRedeclare(&(*pGen),pSub,pBase,pAttr,(ph7_class_attr *)pEntry->pUserData);` |
|    17043 | 2093 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2094 | `					SySetRelease(&aInherited);` |
|      ! 0 | 2095 | `					return SXERR_ABORT;` |
|        - | 2096 | `				}` |
|     8508 | 2097 | `			}` |
|        - | 2098 | `			/* A child MAY redeclare a base's private property: php treats the two` |
|        - | 2099 | `			 * as independent members (each private to its declaring class), with no` |
|        - | 2100 | `			 * diagnostic. PH7 warned here, which is wrong — the child's entry simply` |
|        - | 2101 | `			 * shadows the base's in the by-name attribute table.` |
|        - | 2102 | `			 *` |
|        - | 2103 | `			 * Ordering: php keeps an overridden INSTANCE property at the position` |
|        - | 2104 | `` 			 * the BASE declared it (`class G{$g1;$g2;} class H extends G{$h;$g1;}` `` |
|        - | 2105 | `			 * iterates g1,g2,h — not g2,h,g1). Re-collect the CHILD's definition,` |
|        - | 2106 | `			 * whose default value wins, and drop its current entry so the prepend` |
|        - | 2107 | `			 * below re-inserts it in base order. Statics/constants are not part of` |
|        - | 2108 | `			 * instance iteration, so they keep their existing slot. */` |
|    33941 | 2109 | `			if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|    33931 | 2110 | `				ph7_class_attr *pOwn = (ph7_class_attr *)pEntry->pUserData;` |
|    33931 | 2111 | `				OoInheritPropertyHooks(pBase,pAttr,pOwn);` |
|    33931 | 2112 | `				SyHashDeleteEntry(&pSub->hAttr,(const void *)pName->zString,pName->nByte,0);` |
|    33931 | 2113 | `				rc = SySetPut(&aInherited,(const void *)&pOwn);` |
|    33931 | 2114 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 2115 | `					SySetRelease(&aInherited);` |
|      ! 0 | 2116 | `					return rc;` |
|        - | 2117 | `				}` |
|    16941 | 2118 | `			}` |
|    33941 | 2119 | `			continue;` |
|        - | 2120 | `		}` |
|        - | 2121 | `		/* Collect the attribute. A private STATIC comes down too: php keeps one` |
|        - | 2122 | ``		 * in the child's property table -- `B::$s` on `class A { private static`` |
|        - | 2123 | ``		 * $s; }` is "Cannot access private property B::$s", the visibility`` |
|        - | 2124 | `		 * refusal, and not the undeclared-static one -- and nothing else could` |
|        - | 2125 | ``		 * find it, so `static::$s` from a base method with the subclass as its`` |
|        - | 2126 | `		 * late-static-binding target reported its own static as undeclared. Its` |
|        - | 2127 | `		 * storage is the DECLARING class's slot either way (nIdx is shared), so` |
|        - | 2128 | `		 * this is a second name for one static, exactly as php has it.` |
|        - | 2129 | `		 *` |
|        - | 2130 | `		 * These are gathered rather than installed here because php orders an` |
|        - | 2131 | `		 * instance's properties BASE-DECLARED FIRST, then the subclass's own,` |
|        - | 2132 | `		 * then trait members — while inheritance runs AFTER the subclass body` |
|        - | 2133 | `		 * has already filled hAttr. They are prepended below. */` |
|  7046061 | 2134 | `		rc = SySetPut(&aInherited,(const void *)&pAttr);` |
|  7046061 | 2135 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 2136 | `			SySetRelease(&aInherited);` |
|      ! 0 | 2137 | `			return rc;` |
|        - | 2138 | `		}` |
|        5 | 2139 | `	}` |
|        - | 2140 | `	/* Prepend the collected base attributes so hAttr reads base-first. The` |
|        - | 2141 | `	 * table's iteration list is what every object-iteration consumer walks` |
|        - | 2142 | `	 * (var_dump/print_r, get_object_vars, foreach, (array) casts, json_encode),` |
|        - | 2143 | `	 * so this ordering is user-visible — json_encode emits its keys in exactly` |
|        - | 2144 | `	 * this order. SyHashInsert is a HEAD insert, so walking the collected set` |
|        - | 2145 | `	 * backwards leaves the base's own declaration order at the front. */` |
|   955704 | 2146 | `	if( SySetUsed(&aInherited) > 0 ){` |
|   955056 | 2147 | `		ph7_class_attr **apInherited = (ph7_class_attr **)SySetBasePtr(&aInherited);` |
|   955056 | 2148 | `		sxu32 n = SySetUsed(&aInherited);` |
| 10679661 | 2149 | `		while( n > 0 ){` |
|  9724610 | 2150 | `			ph7_class_attr *pIn = apInherited[--n];` |
|        - | 2151 | `			/* Under php's STORAGE name, which is the plain one for everything but` |
|        - | 2152 | `			 * an inherited private instance property. */` |
|  9724610 | 2153 | `			const SyString *pKey = PH7_ClassAttrStorageName(pGen->pVm,pSub,pIn);` |
|  9724610 | 2154 | `			if( pKey != &pIn->sName ){` |
|       70 | 2155 | `				pSub->iFlags \|= PH7_CLASS_SHADOW_PROP;` |
|        - | 2156 | `				/* ...and WHICH plain name it is hidden under, so the per-access` |
|        - | 2157 | `				 * screen in OoScopePrivateAttr can answer without a lookup. */` |
|      103 | 2158 | `				pSub->nShadowName \|= OoShadowNameBit(SyHashKey(&pSub->hAttr,` |
|       66 | 2159 | `					(const void *)SyStringData(&pIn->sName),SyStringLength(&pIn->sName)));` |
|       33 | 2160 | `			}` |
|  9724610 | 2161 | `			rc = SyHashInsert(&pSub->hAttr,(const void *)pKey->zString,pKey->nByte,pIn);` |
|  9724610 | 2162 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 2163 | `				SySetRelease(&aInherited);` |
|      ! 0 | 2164 | `				return rc;` |
|        - | 2165 | `			}` |
|        5 | 2166 | `		}` |
|   476904 | 2167 | `	}` |
|        - | 2168 | `	/* Inherit the base's CONSTANTS (separate namespace: hConst). Constants are not` |
|        - | 2169 | `	 * part of instance iteration, so no base-first ordering dance is needed — a plain` |
|        - | 2170 | `	 * copy of every constant the subclass did not itself redeclare. A subclass that` |
|        - | 2171 | `	 * redeclares a base FINAL constant is a fatal (PHP 8.1). */` |
|   955704 | 2172 | `	SyHashResetLoopCursor(&pBase->hConst);` |
|  3887659 | 2173 | `	while((pEntry = SyHashGetNextEntry(&pBase->hConst)) != 0 ){` |
|        - | 2174 | `		SyHashEntry *pOwn;` |
|  2931960 | 2175 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  2931960 | 2176 | `		pName = &pAttr->sName;` |
|  2931960 | 2177 | `		if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|        - | 2178 | ``			/* A private CONSTANT is not inherited at all: php answers `B::K` with`` |
|        - | 2179 | `			 * "Undefined constant B::K", never with the visibility refusal it words` |
|        - | 2180 | ``			 * for `A::K`. Copying it down said "Cannot access private constant`` |
|        - | 2181 | ``			 * B::K" -- and let `static::K` from a base method find one php does`` |
|        - | 2182 | ``			 * not. A base method's own `self::K` resolves against A directly. */`` |
|       20 | 2183 | `			continue;` |
|        - | 2184 | `		}` |
|  2931944 | 2185 | `		if( (pOwn = SyHashGet(&pSub->hConst,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|       25 | 2186 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_FINAL) ){` |
|        - | 2187 | `				/* Cannot override a final class constant. Report the class that` |
|        - | 2188 | `				 * originally declared it (pDeclClass) for a multi-level chain -- and a` |
|        - | 2189 | `				 * TRAIT-declared one belongs to the class that composed it. */` |
|        6 | 2190 | `				ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pBase);` |
|        8 | 2191 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|        - | 2192 | `					"%z::%z cannot override final constant %z::%z",` |
|        2 | 2193 | `					&pSub->sDisp,pName,&pOwner->sDisp,pName);` |
|        6 | 2194 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2195 | `					SySetRelease(&aInherited);` |
|      ! 0 | 2196 | `					return SXERR_ABORT;` |
|        - | 2197 | `				}` |
|        2 | 2198 | `			}` |
|       25 | 2199 | `			continue;` |
|        - | 2200 | `		}` |
|  2931922 | 2201 | `		rc = SyHashInsertTail(&pSub->hConst,(const void *)pName->zString,pName->nByte,pAttr);` |
|  2931922 | 2202 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 2203 | `			SySetRelease(&aInherited);` |
|      ! 0 | 2204 | `			return rc;` |
|        - | 2205 | `		}` |
|        5 | 2206 | `	}` |
|   955704 | 2207 | `	SySetRelease(&aInherited);` |
|   955704 | 2208 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
| 19402231 | 2209 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|        - | 2210 | `		SyHashEntry *pOwn;` |
|        - | 2211 | `		SyString sKey;` |
|        - | 2212 | `		/* Make sure the private/final methods are not redeclared in the subclass.` |
|        - | 2213 | `		 * The identity inherited is the base's HASH KEY, not sFunc.sName — the same` |
|        - | 2214 | `		 * rule PH7_ClassUseTrait copies a trait by. A trait adaptation leaves the` |
|        - | 2215 | `		 * composed class holding entries whose key is the name the class ANSWERS to` |
|        - | 2216 | ``		 * while the method struct keeps its original name: `B::m as mB` is the key`` |
|        - | 2217 | `` 		 * `mB` over a struct still called `m`, and `A::m insteadof B` is the key `m` `` |
|        - | 2218 | ``		 * over A's struct. Keying the copy off sFunc.sName re-filed both under `m`,`` |
|        - | 2219 | `		 * so a subclass of the composing class lost the alias entirely and took` |
|        - | 2220 | ``		 * whichever of the two the hash walk reached last as its `m` — the insteadof`` |
|        - | 2221 | `		 * choice, silently reversed. */` |
| 18446532 | 2222 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
| 18446532 | 2223 | `		SyStringInitFromBuf(&sKey,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
| 18446532 | 2224 | `		pName = &sKey;` |
| 18446532 | 2225 | `		if( (pOwn = SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte)) != 0 ){` |
|   769000 | 2226 | `			ph7_class_method *pOwnMeth = (ph7_class_method *)pOwn->pUserData;` |
|   769000 | 2227 | `			ph7_class *pOwnDecl = (ph7_class *)pOwnMeth->sFunc.pUserData;` |
|   768995 | 2228 | `			if( (pOwnMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0` |
|   384001 | 2229 | `			 && (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0` |
|       13 | 2230 | `			 && pOwnDecl && (pOwnDecl->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|        - | 2231 | ``				/* A trait's `abstract` is a REQUIREMENT, not a member, and php lets an`` |
|        - | 2232 | ``				 * INHERITED method satisfy it: `trait T { abstract function need(); }`` |
|        - | 2233 | ``				 * class P { function need(){} } class C extends P { use T; }` composes`` |
|        - | 2234 | `				 * there and was "Class C contains 1 abstract method" here, because the` |
|        - | 2235 | `				 * trait is applied before the base is inherited and the requirement then` |
|        - | 2236 | `				 * shadowed the very method that answers it. The satisfying declaration` |
|        - | 2237 | `				 * still has to be COMPATIBLE with the requirement -- and php words that` |
|        - | 2238 | `				 * one the other way round, naming the class that PROVIDES the method and` |
|        - | 2239 | `				 * the trait that asked for it. */` |
|       10 | 2240 | `				rc = PH7_ClassCheckOverrideCompat(&(*pGen),pOwnDecl,pBase,pOwnMeth,pMeth,1);` |
|       10 | 2241 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2242 | `					return SXERR_ABORT;` |
|        - | 2243 | `				}` |
|       10 | 2244 | `				pOwn->pUserData = (void *)pMeth;` |
|   385004 | 2245 | `				continue;` |
|        - | 2246 | `			}` |
|   768992 | 2247 | `			if( pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|        - | 2248 | `				/* php: a base's PRIVATE method is never overridden — the child's` |
|        - | 2249 | `				 * declaration is an independent member of the same name, so neither` |
|        - | 2250 | `				 * the final rule nor the signature-compatibility rule applies to it` |
|        - | 2251 | `				 * (zend's do_inherit_method skips both for a private parent). PHL` |
|        - | 2252 | ``				 * ran both: `class A { private function m($a){} } class B extends A`` |
|        - | 2253 | ``				 * { private function m($a,$b,$c){} }` was a fatal php compiles, and`` |
|        - | 2254 | ``				 * `final private` in the base fataled every child that reused the`` |
|        - | 2255 | `				 * name — php only WARNS at the final-private DECLARATION and lets` |
|        - | 2256 | `				 * the child have the name. */` |
|   768977 | 2257 | `			}else if( (pMeth->iFlags & PH7_CLASS_ATTR_FINAL)` |
|   383984 | 2258 | `			 && pName->nByte > sizeof("__phl_hook_get_")-1` |
|       10 | 2259 | `			 && SyMemcmp(pName->zString,"__phl_hook_",sizeof("__phl_hook_")-1) == 0 ){` |
|        - | 2260 | ``				/* A `final` property HOOK (__phl_hook_get_x): php names it as`` |
|        - | 2261 | ``				 * `A::$x::get()`, and judges it where the subclass links -- its`` |
|        - | 2262 | `				 * class line, not the overriding hook's. */` |
|        - | 2263 | `				SyString sProp, sKind;` |
|        3 | 2264 | `				SyStringInitFromBuf(&sKind,&pName->zString[sizeof("__phl_hook_")-1],3);` |
|        3 | 2265 | `				SyStringInitFromBuf(&sProp,&pName->zString[sizeof("__phl_hook_get_")-1],` |
|        - | 2266 | `					pName->nByte - (sizeof("__phl_hook_get_")-1));` |
|        4 | 2267 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pSub->nLine,` |
|        - | 2268 | `					"Cannot override final property hook %z::$%z::%z()",` |
|        1 | 2269 | `					&pBase->sDisp,&sProp,&sKind);` |
|        3 | 2270 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2271 | `					return SXERR_ABORT;` |
|        1 | 2272 | `				}` |
|   768967 | 2273 | `			}else if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|        - | 2274 | `				/* php: "Cannot override final method A::test()" */` |
|        8 | 2275 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,((ph7_class_method *)pOwn->pUserData)->nLine,` |
|        - | 2276 | `					"Cannot override final method %z::%z()",` |
|        2 | 2277 | `					&pBase->sDisp,pName);` |
|        2 | 2278 | `				(void)pSub;` |
|        6 | 2279 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2280 | `					return SXERR_ABORT;` |
|        - | 2281 | `				}` |
|        4 | 2282 | `			}else{` |
|        - | 2283 | `				/* Check the override's signature is compatible with the parent's. */` |
|  1152940 | 2284 | `				rc = PH7_ClassCheckOverrideCompat(&(*pGen),pBase,pSub,pMeth,` |
|   768957 | 2285 | `					(ph7_class_method *)pOwn->pUserData,1);` |
|   768962 | 2286 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2287 | `					return SXERR_ABORT;` |
|        - | 2288 | `				}` |
|        - | 2289 | `			}` |
|   768992 | 2290 | `			continue;` |
|        - | 2291 | `		}` |
|        - | 2292 | `		/* Install the method. php: a base class's private method is in the child's` |
|        - | 2293 | `		 * table too — an inherited public method calling $this->priv() must find it,` |
|        - | 2294 | ``		 * and the LOOKUP has to find it for php's answer to `B::p()` to be`` |
|        - | 2295 | `		 * "Call to private method A::p() from global scope" rather than` |
|        - | 2296 | `		 * "Call to undefined method B::p()". The call-site visibility check binds by` |
|        - | 2297 | `		 * DECLARING class (sFunc.pUserData), so child code and outsiders still cannot` |
|        - | 2298 | ``		 * reach it; a private ctor copied down blocks `new Child` from outside like`` |
|        - | 2299 | `		 * php's; and the surfaces that must NOT show an inherited private say so` |
|        - | 2300 | `		 * themselves (method_exists, get_class_methods, ReflectionClass::getMethods).` |
|        - | 2301 | `		 *` |
|        - | 2302 | `		 * STATIC privates used to be skipped here, on the reasoning that base methods` |
|        - | 2303 | `		 * reach them through self:: against the declaring class anyway. They do — but` |
|        - | 2304 | ``		 * nothing else could: every spelling of `B::p()` (the call, `['B','p']()`,`` |
|        - | 2305 | `		 * call_user_func, a first-class callable) reported the name as UNDEFINED, and` |
|        - | 2306 | ``		 * `static::p()` from the base with a subclass as the late-static-binding`` |
|        - | 2307 | `		 * target could not find its own method. */` |
| 17677537 | 2308 | `		rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
| 17677537 | 2309 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 2310 | `			return rc;` |
|        - | 2311 | `		}` |
|        5 | 2312 | `	}` |
|        - | 2313 | `	/* All done */` |
|   955704 | 2314 | `	return SXRET_OK;` |
|   477233 | 2315 | `}` |
|        - | 2316 | `/*` |
|        - | 2317 | ` * Do these two compiled property defaults say the same thing? A raw memcmp of the` |
|        - | 2318 | ` * two instruction buffers is not that question: an instruction carries the LINE it` |
|        - | 2319 | ` * was compiled from and a literal travels as an INDEX into the VM's constant table,` |
|        - | 2320 | `` * so `public $p = 1` written in a trait and the same `public $p = 1` written in the`` |
|        - | 2321 | ` * composing class compare as different bytes and made php's incompatible-property` |
|        - | 2322 | ` * fatal fire on a class php composes without a word. Compare what the instructions` |
|        - | 2323 | ` * MEAN instead: the opcode, its operands, and for a constant load the VALUE behind` |
|        - | 2324 | ` * the index.` |
|        - | 2325 | ` */` |
|       50 | 2326 | `static int VmTraitLiteralSame(ph7_vm *pVm,sxu32 nLeft,sxu32 nRight)` |
|        5 | 2327 | `{` |
|        - | 2328 | `	ph7_value *pLeft,*pRight;` |
|       55 | 2329 | `	if( nLeft == nRight ){` |
|       20 | 2330 | `		return 1;` |
|        - | 2331 | `	}` |
|       37 | 2332 | `	pLeft  = (ph7_value *)SySetAt(&pVm->aLitObj,nLeft);` |
|       37 | 2333 | `	pRight = (ph7_value *)SySetAt(&pVm->aLitObj,nRight);` |
|       37 | 2334 | `	if( pLeft == 0 \|\| pRight == 0 ){` |
|      ! 0 | 2335 | `		return 0;` |
|        - | 2336 | `	}` |
|       37 | 2337 | `	if( (pLeft->iFlags & ~MEMOBJ_AUX) != (pRight->iFlags & ~MEMOBJ_AUX) ){` |
|       16 | 2338 | `		return 0;` |
|        - | 2339 | `	}` |
|       18 | 2340 | `	if( SyBlobLength(&pLeft->sBlob) != SyBlobLength(&pRight->sBlob)` |
|       21 | 2341 | `	 \|\| (SyBlobLength(&pLeft->sBlob) > 0` |
|       11 | 2342 | `	     && SyMemcmp(SyBlobData(&pLeft->sBlob),SyBlobData(&pRight->sBlob),` |
|        4 | 2343 | `	                 SyBlobLength(&pLeft->sBlob)) != 0) ){` |
|        3 | 2344 | `		return 0;` |
|        - | 2345 | `	}` |
|       19 | 2346 | `	if( (pLeft->iFlags & MEMOBJ_INT) && pLeft->x.iVal != pRight->x.iVal ){` |
|        8 | 2347 | `		return 0;` |
|        - | 2348 | `	}` |
|       12 | 2349 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && pLeft->rVal != pRight->rVal ){` |
|      ! 0 | 2350 | `		return 0;` |
|        - | 2351 | `	}` |
|       12 | 2352 | `	return 1;` |
|       30 | 2353 | `}` |
|       78 | 2354 | `static int VmTraitByteCodeSame(ph7_vm *pVm,SySet *pLeft,SySet *pRight)` |
|        5 | 2355 | `{` |
|        - | 2356 | `	VmInstr *aLeft,*aRight;` |
|        - | 2357 | `	sxu32 n,nUsed;` |
|       83 | 2358 | `	nUsed = SySetUsed(pLeft);` |
|       83 | 2359 | `	if( nUsed != SySetUsed(pRight) ){` |
|       34 | 2360 | `		return 0;` |
|        - | 2361 | `	}` |
|       51 | 2362 | `	if( nUsed < 1 ){` |
|        3 | 2363 | `		return 1;` |
|        - | 2364 | `	}` |
|       49 | 2365 | `	aLeft  = (VmInstr *)SySetBasePtr(pLeft);` |
|       49 | 2366 | `	aRight = (VmInstr *)SySetBasePtr(pRight);` |
|      101 | 2367 | `	for( n = 0 ; n < nUsed ; ++n ){` |
|       79 | 2368 | `		if( aLeft[n].iOp != aRight[n].iOp \|\| aLeft[n].iP1 != aRight[n].iP1 ){` |
|      ! 0 | 2369 | `			return 0;` |
|        - | 2370 | `		}` |
|       79 | 2371 | `		if( aLeft[n].iOp == PH7_OP_LOADC ){` |
|       55 | 2372 | `			if( !VmTraitLiteralSame(pVm,aLeft[n].iP2,aRight[n].iP2) ){` |
|       26 | 2373 | `				return 0;` |
|        - | 2374 | `			}` |
|       30 | 2375 | `			continue;` |
|        - | 2376 | `		}` |
|       26 | 2377 | `		if( aLeft[n].iP2 != aRight[n].iP2 \|\| aLeft[n].p3 != aRight[n].p3 ){` |
|      ! 0 | 2378 | `			return 0;` |
|        - | 2379 | `		}` |
|       14 | 2380 | `	}` |
|       24 | 2381 | `	return 1;` |
|       44 | 2382 | `}` |
|        - | 2383 | `/*` |
|        - | 2384 | ` * php converts a typed CONSTANT's int to float when the declaration is checked, as it` |
|        - | 2385 | `` * does a property's, so `const float K = 1` holds float(1) from the start. Here that`` |
|        - | 2386 | ` * conversion waits for the first read, and the folded default is still the int: widen` |
|        - | 2387 | ` * it the same way before comparing. Only a type naming float and not int widens --` |
|        - | 2388 | `` * `int\|float` keeps the int an int.`` |
|        - | 2389 | ` */` |
|       36 | 2390 | `static void VmTraitConstWiden(const ph7_class_attr *pTyped,ph7_value *pVal)` |
|        4 | 2391 | `{` |
|       40 | 2392 | `	sxu32 nTypes = 0;` |
|       36 | 2393 | `	if( pTyped == 0 \|\| (pTyped->iFlags & PH7_CLASS_ATTR_TYPED) == 0` |
|       34 | 2394 | `		\|\| (pVal->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL)) != MEMOBJ_INT ){` |
|       28 | 2395 | `		return;` |
|        - | 2396 | `	}` |
|       14 | 2397 | `	if( pTyped->iFlags & PH7_CLASS_ATTR_UNION ){` |
|        6 | 2398 | `		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(&pTyped->aUnionAlts);` |
|        - | 2399 | `		sxu32 n;` |
|       14 | 2400 | `		for( n = 0 ; n < SySetUsed(&pTyped->aUnionAlts) ; ++n ){` |
|       10 | 2401 | `			if( aAlt[n].nType != SXU32_HIGH ){` |
|       10 | 2402 | `				nTypes \|= aAlt[n].nType;` |
|        4 | 2403 | `			}` |
|        6 | 2404 | `		}` |
|       12 | 2405 | `	}else if( pTyped->nType != SXU32_HIGH ){` |
|        9 | 2406 | `		nTypes = pTyped->nType;` |
|        4 | 2407 | `	}` |
|       14 | 2408 | `	if( (nTypes & MEMOBJ_REAL) && (nTypes & MEMOBJ_INT) == 0 ){` |
|       11 | 2409 | `		PH7_MemObjToReal(pVal);` |
|        5 | 2410 | `	}` |
|       22 | 2411 | `}` |
|        - | 2412 | `/*` |
|        - | 2413 | ` * The value one declaration's default folds to. A declaration with NO default holds null` |
|        - | 2414 | ``  * when it is an untyped property -- php stores null in its slot, the same null `= null` `` |
|        - | 2415 | `` * stores, so `public $p;` against `public $p = null;` composes -- and holds nothing at all`` |
|        - | 2416 | `` * when it is typed: `?int $p;` is uninitialized, which no default equals.`` |
|        - | 2417 | ` */` |
|      108 | 2418 | `static int VmTraitFoldDefault(ph7_vm *pVm,const ph7_class_attr *pAttr,ph7_value *pOut)` |
|        4 | 2419 | `{` |
|      112 | 2420 | `	if( SySetUsed(&pAttr->aByteCode) < 1 ){` |
|       13 | 2421 | `		return (pAttr->iFlags & (PH7_CLASS_ATTR_TYPED\|PH7_CLASS_ATTR_CONSTANT)) == 0;` |
|        - | 2422 | `	}` |
|      100 | 2423 | `	return PH7_ClassFoldDefault(pVm,(SySet *)&pAttr->aByteCode,pOut);` |
|       58 | 2424 | `}` |
|        - | 2425 | `/*` |
|        - | 2426 | ` * Two defaults that are spelled differently can still be the same VALUE, and the value` |
|        - | 2427 | `` * is what php compares: it holds each default as its compiler FOLDED it and asks `===`.`` |
|        - | 2428 | `` * `= 1+1` against `= 2` composes, and so does a trait's `float $t = 1` against the`` |
|        - | 2429 | `` * class's `float $t = 1.0` -- the int was converted to the declared float when the`` |
|        - | 2430 | ` * default was checked, so both hold float(1). Where the program text matches, nothing` |
|        - | 2431 | ` * needs running; where it does not, fold both and compare what they fold to. A default` |
|        - | 2432 | ` * php keeps unfolded (a constant NAME) stays on the text comparison.` |
|        - | 2433 | ` */` |
|       78 | 2434 | `static int VmTraitDefaultsMatch(ph7_vm *pVm,ph7_class_attr *pLeft,ph7_class_attr *pRight,int bConst)` |
|        5 | 2435 | `{` |
|        - | 2436 | `	ph7_value sLeft,sRight;` |
|       83 | 2437 | `	int bSame = 0;` |
|       83 | 2438 | `	if( VmTraitByteCodeSame(pVm,&pLeft->aByteCode,&pRight->aByteCode) ){` |
|       26 | 2439 | `		return 1;` |
|        - | 2440 | `	}` |
|       58 | 2441 | `	PH7_MemObjInit(pVm,&sLeft);` |
|       58 | 2442 | `	PH7_MemObjInit(pVm,&sRight);` |
|       58 | 2443 | `	if( VmTraitFoldDefault(pVm,pLeft,&sLeft) && VmTraitFoldDefault(pVm,pRight,&sRight) ){` |
|       58 | 2444 | `		if( bConst ){` |
|       22 | 2445 | `			VmTraitConstWiden(pLeft,&sLeft);` |
|       22 | 2446 | `			VmTraitConstWiden(pLeft,&sRight);` |
|        9 | 2447 | `		}` |
|       58 | 2448 | `		bSame = PH7_MemObjCmp(&sLeft,&sRight,TRUE,0) == 0;` |
|       27 | 2449 | `	}` |
|       58 | 2450 | `	PH7_MemObjRelease(&sLeft);` |
|       58 | 2451 | `	PH7_MemObjRelease(&sRight);` |
|       58 | 2452 | `	return bSame;` |
|       44 | 2453 | `}` |
|        - | 2454 | `/*` |
|        - | 2455 | ` * Two constant declarations php considers the SAME declaration. Composing a trait over a` |
|        - | 2456 | ` * name that is already taken is only a conflict when the definition differs, and php's` |
|        - | 2457 | ``  * notion of "differs" covers the whole declaration, not just the value: `final const K='x'` `` |
|        - | 2458 | ``  * against `const K='x'` conflicts, and so does `public const K` against `private const K` `` |
|        - | 2459 | `` * and `const int K=1` against `const K=1`.`` |
|        - | 2460 | ` */` |
|       22 | 2461 | `static int VmTraitConstDefsMatch(ph7_vm *pVm,ph7_class_attr *pLeft,ph7_class_attr *pRight)` |
|        5 | 2462 | `{` |
|       27 | 2463 | `	sxi32 iMask = PH7_CLASS_ATTR_FINAL\|PH7_CLASS_ATTR_TYPED\|PH7_CLASS_ATTR_ABSTRACT;` |
|       27 | 2464 | `	if( pLeft->iProtection != pRight->iProtection ){` |
|      ! 0 | 2465 | `		return 0;` |
|        - | 2466 | `	}` |
|       27 | 2467 | `	if( (pLeft->iFlags & iMask) != (pRight->iFlags & iMask) ){` |
|      ! 0 | 2468 | `		return 0;` |
|        - | 2469 | `	}` |
|       27 | 2470 | `	if( SyStringCmp(&pLeft->sTypeName,&pRight->sTypeName,SyMemcmp) != 0 ){` |
|      ! 0 | 2471 | `		return 0;` |
|        - | 2472 | `	}` |
|       27 | 2473 | `	return VmTraitDefaultsMatch(pVm,pLeft,pRight,1);` |
|       16 | 2474 | `}` |
|        - | 2475 | `/*` |
|        - | 2476 | ` * The same test for a PROPERTY. php compares the default only once the declaration` |
|        - | 2477 | ``  * agrees -- visibility, `static`, `readonly` and an invariant type -- so `public $p;` `` |
|        - | 2478 | ``  * against `protected $p = null;`, `public static $p = null;` or `public ?int $p = null;` `` |
|        - | 2479 | ` * conflicts although both hold null. The declared type is held in its canonical text,` |
|        - | 2480 | `` * which folds `int\|null` and `?int` to one spelling; a class name compares without case.`` |
|        - | 2481 | ` */` |
|       62 | 2482 | `static int VmTraitPropDefsMatch(ph7_vm *pVm,ph7_class_attr *pLeft,ph7_class_attr *pRight)` |
|        5 | 2483 | `{` |
|       67 | 2484 | `	sxi32 iMask = PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED;` |
|       67 | 2485 | `	if( pLeft->iProtection != pRight->iProtection ){` |
|        3 | 2486 | `		return 0;` |
|        - | 2487 | `	}` |
|       65 | 2488 | `	if( (pLeft->iFlags & iMask) != (pRight->iFlags & iMask) ){` |
|        6 | 2489 | `		return 0;` |
|        - | 2490 | `	}` |
|       61 | 2491 | `	if( SyStringCmp(&pLeft->sTypeName,&pRight->sTypeName,SyStrnicmp) != 0 ){` |
|      ! 0 | 2492 | `		return 0;` |
|        - | 2493 | `	}` |
|       61 | 2494 | `	return VmTraitDefaultsMatch(pVm,pLeft,pRight,0);` |
|       36 | 2495 | `}` |
|        - | 2496 | `/*` |
|        - | 2497 | ` * A private copy of a trait member's record for one composing class. php composes a trait` |
|        - | 2498 | ` * into each using class SEPARATELY, so a trait's STATIC property is one slot per class --` |
|        - | 2499 | `` * `trait T { public static $c = 0; } class A { use T; } class B { use T; }` gives A and B a`` |
|        - | 2500 | `` * counter each -- and a trait CONSTANT is evaluated per class, so `const K = self::J` reads`` |
|        - | 2501 | ` * the J of whichever class composed it. Copying the record by POINTER gave every using class` |
|        - | 2502 | ` * the same storage slot and the same memoized value.` |
|        - | 2503 | ` *` |
|        - | 2504 | ` * The copy shares its source's compiled byte-code and attribute sets, which are read-only` |
|        - | 2505 | ` * once compilation is past; what it does NOT share is nIdx, the storage slot, and the` |
|        - | 2506 | ` * per-evaluation flags. pDeclClass stays the TRAIT, so every scope and naming rule still` |
|        - | 2507 | ` * finds the composing class through it.` |
|        - | 2508 | ` */` |
|       54 | 2509 | `static ph7_class_attr * VmCloneTraitAttr(ph7_vm *pVm,ph7_class_attr *pSrc)` |
|        4 | 2510 | `{` |
|       58 | 2511 | `	ph7_class_attr *pNew = (ph7_class_attr *)SyMemBackendPoolAlloc(&pVm->sAllocator,` |
|        - | 2512 | `		sizeof(ph7_class_attr));` |
|       58 | 2513 | `	if( pNew == 0 ){` |
|      ! 0 | 2514 | `		return 0;` |
|        - | 2515 | `	}` |
|       58 | 2516 | `	SyMemcpy((const void *)pSrc,(void *)pNew,sizeof(ph7_class_attr));` |
|       58 | 2517 | `	pNew->nIdx = SXU32_HIGH; /* its own storage slot, reserved at this class's mount */` |
|       58 | 2518 | `	SyZero(&pNew->sStoreName,sizeof(SyString)); /* ...and its own mangled name, which` |
|        - | 2519 | `	                          * names the class that COMPOSED it and not the source's */` |
|       58 | 2520 | `	pNew->iFlags &= ~(PH7_CLASS_ATTR_EVALING\|PH7_CLASS_ATTR_STATIC_DEFER);` |
|       58 | 2521 | `	return pNew;` |
|       31 | 2522 | `}` |
|        - | 2523 | `/*` |
|        - | 2524 | ` * Apply a trait to a class: copy all methods and attributes from the trait` |
|        - | 2525 | ` * into the target class. Unlike inheritance, traits copy ALL members including` |
|        - | 2526 | ` * private ones. Members already defined in the class take precedence.` |
|        - | 2527 | ` */` |
|      400 | 2528 | `PH7_PRIVATE sxi32 PH7_ClassUseTrait(ph7_gen_state *pGen,ph7_class *pClass,ph7_class *pTrait)` |
|        5 | 2529 | `{` |
|        - | 2530 | `	ph7_class_method *pMeth;` |
|        - | 2531 | `	ph7_class_attr *pAttr;` |
|        - | 2532 | `	SyHashEntry *pEntry;` |
|        - | 2533 | `	SyString *pName;` |
|        - | 2534 | `	sxi32 rc;` |
|        - | 2535 | `	/* Detect cyclic trait composition (e.g. trait A { use B; } trait B { use A; }) */` |
|      405 | 2536 | `	if( pTrait->iFlags & PH7_CLASS_TRAIT_VISITING ){` |
|      ! 0 | 2537 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pTrait->nLine,` |
|      ! 0 | 2538 | `			"Trait circular reference detected: %z is already being applied",&pTrait->sDisp);` |
|      ! 0 | 2539 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2540 | `			return SXERR_ABORT;` |
|        - | 2541 | `		}` |
|      ! 0 | 2542 | `		return SXRET_OK;` |
|        - | 2543 | `	}` |
|      405 | 2544 | `	pTrait->iFlags \|= PH7_CLASS_TRAIT_VISITING;` |
|      405 | 2545 | `	rc = SXRET_OK;` |
|        - | 2546 | `	/* Copy attributes from the trait */` |
|      405 | 2547 | `	SyHashResetLoopCursor(&pTrait->hAttr);` |
|      579 | 2548 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hAttr)) != 0 ){` |
|        - | 2549 | `		SyHashEntry *pExisting;` |
|      179 | 2550 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      179 | 2551 | `		pName = &pAttr->sName;` |
|      179 | 2552 | `		pExisting = SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte);` |
|      179 | 2553 | `		if( pExisting != 0 ){` |
|        - | 2554 | `			/* The name is taken. What decides is the definition ALREADY standing --` |
|        - | 2555 | `			 * the class's own body just as much as an earlier trait's -- and whether` |
|        - | 2556 | `			 * its default is the same one. Looking the name up in the traits applied` |
|        - | 2557 | `			 * so far and comparing only THEN let a class-body property through:` |
|        - | 2558 | ``			 * `class M { use TA, TB; public $p = 3; }` said nothing when TA arrived`` |
|        - | 2559 | `			 * (no trait held the name yet) and then blamed the wrong pair when TB did. */` |
|       67 | 2560 | `			ph7_class_attr *pClassAttr = (ph7_class_attr *)pExisting->pUserData;` |
|       67 | 2561 | `			if( !VmTraitPropDefsMatch(pGen->pVm,pAttr,pClassAttr) ){` |
|        - | 2562 | `				/* php names the FIRST definition rather than the standing one: when the` |
|        - | 2563 | `				 * holder is the composing class itself, it walks the traits applied so` |
|        - | 2564 | `				 * far and names the first that declares the property, so the same class` |
|        - | 2565 | `				 * body reads "M and TA" with one trait behind it and "TA and TB" with` |
|        - | 2566 | `				 * two. The sentence ends with a clause of its own and lets the fatal's` |
|        - | 2567 | `				 * " in %s on line %u" finish it -- the line is the composing class's. */` |
|       16 | 2568 | `				ph7_class *pHolder = pClassAttr->pDeclClass;` |
|       16 | 2569 | `				if( pHolder == 0 \|\| pHolder == pClass ){` |
|       11 | 2570 | `					ph7_class **apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       11 | 2571 | `					sxu32 nUsed = SySetUsed(&pClass->aTrait);` |
|        - | 2572 | `					sxu32 k;` |
|       11 | 2573 | `					pHolder = pClass;` |
|       11 | 2574 | `					for(k = 0; k < nUsed; k++){` |
|      ! 0 | 2575 | `						if( PH7_ClassExtractAttribute(apUsedTraits[k],pName->zString,pName->nByte) ){` |
|      ! 0 | 2576 | `							pHolder = apUsedTraits[k];` |
|      ! 0 | 2577 | `							break;` |
|        - | 2578 | `						}` |
|      ! 0 | 2579 | `					}` |
|        4 | 2580 | `				}` |
|       22 | 2581 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,` |
|        - | 2582 | `					"%z and %z define the same property ($%z) in the composition of %z. "` |
|        - | 2583 | `					"However, the definition differs and is considered incompatible. "` |
|        - | 2584 | `					"Class was composed",` |
|       12 | 2585 | `					&pHolder->sDisp,&pTrait->sDisp,pName,&pClass->sDisp);` |
|       16 | 2586 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2587 | `					goto cleanup;` |
|        - | 2588 | `				}` |
|        6 | 2589 | `			}` |
|       67 | 2590 | `			continue;` |
|        - | 2591 | `		}` |
|      117 | 2592 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - | 2593 | `			/* One slot per composing class (see VmCloneTraitAttr). */` |
|       29 | 2594 | `			ph7_class_attr *pOwnCopy = VmCloneTraitAttr(pGen->pVm,pAttr);` |
|       29 | 2595 | `			if( pOwnCopy == 0 ){` |
|      ! 0 | 2596 | `				rc = SXERR_MEM;` |
|      ! 0 | 2597 | `				goto cleanup;` |
|        - | 2598 | `			}` |
|       29 | 2599 | `			pAttr = pOwnCopy;` |
|       13 | 2600 | `		}` |
|      117 | 2601 | `		rc = SyHashInsertTail(&pClass->hAttr,(const void *)pName->zString,pName->nByte,pAttr);` |
|      117 | 2602 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 2603 | `			goto cleanup;` |
|        - | 2604 | `		}` |
|        - | 2605 | `		/* A trait's private is the COMPOSING class's own (php composes it in), so` |
|        - | 2606 | `		 * the mask has to name it here too. */` |
|      117 | 2607 | `		PH7_ClassNotePrivateName(pClass,pAttr);` |
|        5 | 2608 | `	}` |
|        - | 2609 | `	/* Copy constants from the trait (PHP 8.2 trait constants; separate hConst` |
|        - | 2610 | `	 * namespace). The name being taken is only a conflict when the DEFINITION differs,` |
|        - | 2611 | `	 * exactly as for a property above -- php compares the value, the visibility, the` |
|        - | 2612 | ``	 * `final` flag and the declared type, and lets two identical declarations through`` |
|        - | 2613 | ``	 * (`trait A { const K='x'; } trait B { const K='x'; }` composes fine). A definition`` |
|        - | 2614 | `	 * inherited from a BASE class is not part of the comparison: a trait constant` |
|        - | 2615 | `	 * overrides one, silently, the way a class-body constant does. */` |
|      405 | 2616 | `	SyHashResetLoopCursor(&pTrait->hConst);` |
|      455 | 2617 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hConst)) != 0 ){` |
|        - | 2618 | `		SyHashEntry *pExisting;` |
|       55 | 2619 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|       55 | 2620 | `		pName = &pAttr->sName;` |
|       55 | 2621 | `		pExisting = SyHashGet(&pClass->hConst,(const void *)pName->zString,pName->nByte);` |
|       55 | 2622 | `		if( pExisting != 0 ){` |
|       27 | 2623 | `			ph7_class_attr *pHave = (ph7_class_attr *)pExisting->pUserData;` |
|       27 | 2624 | `			ph7_class *pHolder = pHave->pDeclClass;` |
|       22 | 2625 | `			if( pHolder && pHolder != pClass` |
|       14 | 2626 | `			 && (pHolder->iFlags & PH7_CLASS_TRAIT) == 0` |
|        8 | 2627 | `			 && PH7_VmInstanceOf(pClass,pHolder) ){` |
|        - | 2628 | `				/* Inherited from a base class: the trait's definition replaces it. */` |
|      ! 0 | 2629 | `				ph7_class_attr *pOwnCopy = VmCloneTraitAttr(pGen->pVm,pAttr);` |
|      ! 0 | 2630 | `				if( pOwnCopy == 0 ){` |
|      ! 0 | 2631 | `					rc = SXERR_MEM;` |
|      ! 0 | 2632 | `					goto cleanup;` |
|        - | 2633 | `				}` |
|      ! 0 | 2634 | `				SyHashDeleteEntry2(pExisting);` |
|      ! 0 | 2635 | `				rc = SyHashInsertTail(&pClass->hConst,(const void *)pName->zString,pName->nByte,pOwnCopy);` |
|      ! 0 | 2636 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 2637 | `					goto cleanup;` |
|        - | 2638 | `				}` |
|      ! 0 | 2639 | `				continue;` |
|        - | 2640 | `			}` |
|       27 | 2641 | `			if( !VmTraitConstDefsMatch(pGen->pVm,pAttr,pHave) ){` |
|        - | 2642 | `				/* php names the FIRST definition, as the property path does. */` |
|        9 | 2643 | `				if( pHolder == 0 \|\| pHolder == pClass ){` |
|        6 | 2644 | `					ph7_class **apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|        6 | 2645 | `					sxu32 nUsed = SySetUsed(&pClass->aTrait);` |
|        - | 2646 | `					sxu32 k;` |
|        6 | 2647 | `					pHolder = pClass;` |
|        6 | 2648 | `					for(k = 0; k < nUsed; k++){` |
|      ! 0 | 2649 | `						if( PH7_ClassExtractConstant(apUsedTraits[k],pName->zString,pName->nByte) ){` |
|      ! 0 | 2650 | `							pHolder = apUsedTraits[k];` |
|      ! 0 | 2651 | `							break;` |
|        - | 2652 | `						}` |
|      ! 0 | 2653 | `					}` |
|        2 | 2654 | `				}` |
|       12 | 2655 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,` |
|        - | 2656 | `					"%z and %z define the same constant (%z) in the composition of %z. "` |
|        - | 2657 | `					"However, the definition differs and is considered incompatible. "` |
|        - | 2658 | `					"Class was composed",` |
|        6 | 2659 | `					&pHolder->sDisp,&pTrait->sDisp,pName,&pClass->sDisp);` |
|        9 | 2660 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2661 | `					goto cleanup;` |
|        - | 2662 | `				}` |
|        3 | 2663 | `			}` |
|       27 | 2664 | `			continue;` |
|        - | 2665 | `		}` |
|        - | 2666 | `		{` |
|        - | 2667 | ``			/* Evaluated per composing class (`const K = self::J`), so one record each. */`` |
|       31 | 2668 | `			ph7_class_attr *pOwnCopy = VmCloneTraitAttr(pGen->pVm,pAttr);` |
|       31 | 2669 | `			if( pOwnCopy == 0 ){` |
|      ! 0 | 2670 | `				rc = SXERR_MEM;` |
|      ! 0 | 2671 | `				goto cleanup;` |
|        - | 2672 | `			}` |
|       31 | 2673 | `			rc = SyHashInsertTail(&pClass->hConst,(const void *)pName->zString,pName->nByte,pOwnCopy);` |
|        - | 2674 | `		}` |
|       31 | 2675 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 2676 | `			goto cleanup;` |
|        - | 2677 | `		}` |
|        3 | 2678 | `	}` |
|        - | 2679 | `	/* Copy methods from the trait. The identity copied is the trait's HASH KEY,` |
|        - | 2680 | ``	 * not sFunc.sName: an adaptation alias (`hi as bHi`) is a hash entry whose`` |
|        - | 2681 | `	 * key is the alias while the method struct keeps its original name — keying` |
|        - | 2682 | ``	 * the copy off sFunc.sName silently re-filed `bHi` under `hi`, so an alias`` |
|        - | 2683 | `	 * made inside a TRAIT vanished when that trait was composed into a class. */` |
|      405 | 2684 | `	SyHashResetLoopCursor(&pTrait->hMethod);` |
|      893 | 2685 | `	while((pEntry = SyHashGetNextEntry(&pTrait->hMethod)) != 0 ){` |
|        - | 2686 | `		SyHashEntry *pClassMethEntry;` |
|        - | 2687 | `		SyString sKey;` |
|      493 | 2688 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      493 | 2689 | `		SyStringInitFromBuf(&sKey,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|      493 | 2690 | `		pName = &sKey;` |
|      493 | 2691 | `		pClassMethEntry = SyHashGet(&pClass->hMethod,(const void *)pName->zString,pName->nByte);` |
|      493 | 2692 | `		if( pClassMethEntry != 0 ){` |
|        - | 2693 | `			/* Method already exists in the class. An ABSTRACT trait method is a` |
|        - | 2694 | `			 * REQUIREMENT, not an implementation: php satisfies it with any concrete` |
|        - | 2695 | `			 * method of the same name (from the class body or another trait) — no` |
|        - | 2696 | `			 * collision. Only two CONCRETE trait methods actually conflict. */` |
|       49 | 2697 | `			ph7_class_method *pExistingMeth = (ph7_class_method *)pClassMethEntry->pUserData;` |
|       49 | 2698 | `			int bIncomingAbstract = (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0;` |
|       49 | 2699 | `			int bExistingAbstract = (pExistingMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0;` |
|        - | 2700 | `			ph7_class **apUsedTraits;` |
|        - | 2701 | `			sxu32 nUsed,k;` |
|       49 | 2702 | `			if( bIncomingAbstract ){` |
|        - | 2703 | `				/* Incoming abstract requirement: the existing (concrete or abstract)` |
|        - | 2704 | `				 * method already covers this name — keep it. */` |
|       31 | 2705 | `				continue;` |
|        - | 2706 | `			}` |
|       37 | 2707 | `			if( bExistingAbstract ){` |
|        - | 2708 | `				/* Existing entry is only an abstract requirement (from an earlier` |
|        - | 2709 | `				 * trait): the incoming concrete method satisfies and replaces it. */` |
|        5 | 2710 | `				pClassMethEntry->pUserData = (void *)pMeth;` |
|        5 | 2711 | `				continue;` |
|        - | 2712 | `			}` |
|        - | 2713 | ``			/* Two names are not two METHODS. A trait that `use`s another trait`` |
|        - | 2714 | `			 * flattens it by sharing the very ph7_class_method the origin trait` |
|        - | 2715 | `			 * compiled, so a method reaching the class down two composition paths` |
|        - | 2716 | `			 * arrives as the SAME struct both times -- which is php's own test` |
|        - | 2717 | `			 * (zend compares the two functions' op_array.opcodes) and why` |
|        - | 2718 | ``			 * `trait TB { use TA; } class M { use TB, TA; }` composes there and`` |
|        - | 2719 | `			 * fatalled here. Only two genuinely different definitions collide. */` |
|       33 | 2720 | `			if( pExistingMeth == pMeth ){` |
|       15 | 2721 | `				continue;` |
|        - | 2722 | `			}` |
|        - | 2723 | `			/* A method the class declares ITSELF wins over every trait, however many` |
|        - | 2724 | `			 * of them offer the name: php reports no collision at all for` |
|        - | 2725 | ``			 * `class M { use TA, TB; public function m(){} }`, where PHL raised one`` |
|        - | 2726 | `			 * as soon as the second trait arrived. */` |
|       19 | 2727 | `			if( (ph7_class *)pExistingMeth->sFunc.pUserData == pClass ){` |
|       13 | 2728 | `				continue;` |
|        - | 2729 | `			}` |
|        - | 2730 | `			/* Both concrete: a genuine collision only when the OTHER definition came` |
|        - | 2731 | `			 * from another trait (a concrete one). A class-body method wins silently. */` |
|        6 | 2732 | `			apUsedTraits = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|        6 | 2733 | `			nUsed = SySetUsed(&pClass->aTrait);` |
|        6 | 2734 | `			for(k = 0; k < nUsed; k++){` |
|        6 | 2735 | `				ph7_class_method *pOtherMeth = PH7_ClassExtractMethod(apUsedTraits[k],pName->zString,pName->nByte);` |
|        4 | 2736 | `				if( pOtherMeth != 0 && pOtherMeth != pMeth` |
|        6 | 2737 | `				 && (pOtherMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|        - | 2738 | `					/* Two different traits define the same CONCRETE method with no` |
|        - | 2739 | `					 * resolution. php reports the line of the COMPOSING class, not` |
|        - | 2740 | `					 * the one the losing definition was written on. */` |
|        8 | 2741 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,` |
|        - | 2742 | `						"Trait method %z::%z has not been applied as %z::%z, "` |
|        - | 2743 | `						"because of collision with %z::%z",` |
|        4 | 2744 | `						&pTrait->sDisp,pName,` |
|        2 | 2745 | `						&pClass->sDisp,pName,` |
|        4 | 2746 | `						&apUsedTraits[k]->sDisp,pName);` |
|        6 | 2747 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 2748 | `						goto cleanup;` |
|        - | 2749 | `					}` |
|        6 | 2750 | `					break;` |
|        - | 2751 | `				}` |
|      ! 0 | 2752 | `			}` |
|        - | 2753 | `			/* Class-defined method takes precedence */` |
|        6 | 2754 | `			continue;` |
|        - | 2755 | `		}` |
|      449 | 2756 | `		rc = SyHashInsert(&pClass->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|      449 | 2757 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 2758 | `			goto cleanup;` |
|        - | 2759 | `		}` |
|        5 | 2760 | `	}` |
|        - | 2761 | `	/* Record trait in the class */` |
|      405 | 2762 | `	SySetPut(&pClass->aTrait,(const void *)&pTrait);` |
|      200 | 2763 | `cleanup:` |
|        - | 2764 | `	/* Always clear visiting flag, even on error paths */` |
|      405 | 2765 | `	pTrait->iFlags &= ~PH7_CLASS_TRAIT_VISITING;` |
|      200 | 2766 | `	SXUNUSED(pGen);` |
|      405 | 2767 | `	return rc;` |
|      205 | 2768 | `}` |
|        - | 2769 | `/*` |
|        - | 2770 | ` * Inherit an object interface from another object interface.` |
|        - | 2771 | ` * According to the PHP language reference manual.` |
|        - | 2772 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|        - | 2773 | ` *  must implement, without having to define how these methods are handled.` |
|        - | 2774 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|        - | 2775 | ` *  class, but without any of the methods having their contents defined.` |
|        - | 2776 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|        - | 2777 | ` *` |
|        - | 2778 | ` * This function return SXRET_OK if the interface inheritance operation was successfully performed.` |
|        - | 2779 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|        - | 2780 | ` * error message.` |
|        - | 2781 | ` */` |
|    76065 | 2782 | `PH7_PRIVATE sxi32 PH7_ClassInterfaceInherit(ph7_class *pSub,ph7_class *pBase)` |
|        5 | 2783 | `{` |
|        - | 2784 | `	ph7_class_method *pMeth;` |
|        - | 2785 | `	ph7_class_attr *pAttr;` |
|        - | 2786 | `	SyHashEntry *pEntry;` |
|        - | 2787 | `	SyString *pName;` |
|        - | 2788 | `	sxi32 rc;` |
|        - | 2789 | `	/* Install in the derived hashtable */` |
|    76070 | 2790 | `	SyHashInsert(&pBase->hDerived,(const void *)SyStringData(&pSub->sName),SyStringLength(&pSub->sName),pSub);` |
|    76070 | 2791 | `	SyHashResetLoopCursor(&pBase->hConst);` |
|        - | 2792 | `	/* Copy constants (interfaces carry only constants + method signatures) */` |
|   114158 | 2793 | `	while((pEntry = SyHashGetNextEntry(&pBase->hConst)) != 0 ){` |
|        - | 2794 | `		/* Make sure the constants are not redeclared in the subclass */` |
|        7 | 2795 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|        7 | 2796 | `		pName = &pAttr->sName;` |
|        7 | 2797 | `		if( SyHashGet(&pSub->hConst,(const void *)pName->zString,pName->nByte) == 0 ){` |
|        - | 2798 | `			/* Install the constant in the subclass */` |
|        3 | 2799 | `			rc = SyHashInsertTail(&pSub->hConst,(const void *)pName->zString,pName->nByte,pAttr);` |
|        3 | 2800 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 2801 | `				return rc;` |
|        - | 2802 | `			}` |
|        1 | 2803 | `		}` |
|        1 | 2804 | `	}` |
|    76070 | 2805 | `	SyHashResetLoopCursor(&pBase->hMethod);` |
|        - | 2806 | `	/* Copy methods signature */` |
|   274687 | 2807 | `	while((pEntry = SyHashGetNextEntry(&pBase->hMethod)) != 0 ){` |
|        - | 2808 | `		/* Make sure the method are not redeclared in the subclass */` |
|   160540 | 2809 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   160540 | 2810 | `		pName = &pMeth->sFunc.sName;` |
|   160540 | 2811 | `		if( SyHashGet(&pSub->hMethod,(const void *)pName->zString,pName->nByte) == 0 ){` |
|        - | 2812 | `			/* Install the method */` |
|   160532 | 2813 | `			rc = SyHashInsert(&pSub->hMethod,(const void *)pName->zString,pName->nByte,pMeth);` |
|   160532 | 2814 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 2815 | `				return rc;` |
|        - | 2816 | `			}` |
|    80159 | 2817 | `		}` |
|        5 | 2818 | `	}` |
|        - | 2819 | `	/* Mark as subclass */` |
|    76070 | 2820 | `	pSub->pBase = pBase;` |
|        - | 2821 | `	/* All done */` |
|    76070 | 2822 | `	return SXRET_OK;` |
|    37988 | 2823 | `}` |
|        - | 2824 | `/*` |
|        - | 2825 | ` * Implements an object interface in the given main class.` |
|        - | 2826 | ` * According to the PHP language reference manual.` |
|        - | 2827 | ` *  Object interfaces allow you to create code which specifies which methods a class` |
|        - | 2828 | ` *  must implement, without having to define how these methods are handled.` |
|        - | 2829 | ` *  Interfaces are defined using the interface keyword, in the same way as a standard` |
|        - | 2830 | ` *  class, but without any of the methods having their contents defined.` |
|        - | 2831 | ` *  All methods declared in an interface must be public, this is the nature of an interface.` |
|        - | 2832 | ` *` |
|        - | 2833 | ` * This function return SXRET_OK if the interface was successfully implemented.` |
|        - | 2834 | ` * Any other return value indicates failure and the upper layer must generate an appropriate` |
|        - | 2835 | ` * error message.` |
|        - | 2836 | ` */` |
|   955371 | 2837 | `PH7_PRIVATE sxi32 PH7_ClassImplement(ph7_class *pMain,ph7_class *pInterface)` |
|        5 | 2838 | `{` |
|        - | 2839 | `	ph7_class_attr *pAttr;` |
|        - | 2840 | `	SyHashEntry *pEntry;` |
|        - | 2841 | `	SyString *pName;` |
|        - | 2842 | `	sxi32 rc;` |
|        - | 2843 | `	/* First off,copy all constants declared inside the interface (hConst namespace) */` |
|   955376 | 2844 | `	SyHashResetLoopCursor(&pInterface->hConst);` |
|  1670171 | 2845 | `	while((pEntry = SyHashGetNextEntry(&pInterface->hConst)) != 0 ){` |
|        - | 2846 | `		/* Point to the constant declaration */` |
|   236493 | 2847 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   236493 | 2848 | `		pName = &pAttr->sName;` |
|        - | 2849 | `		/* Make sure the constant is not redeclared in the main class */` |
|   236493 | 2850 | `		if( SyHashGet(&pMain->hConst,pName->zString,pName->nByte) == 0 ){` |
|        - | 2851 | `			/* Install the constant */` |
|   236491 | 2852 | `			rc = SyHashInsertTail(&pMain->hConst,pName->zString,pName->nByte,pAttr);` |
|   236491 | 2853 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 2854 | `				return rc;` |
|        - | 2855 | `			}` |
|   118089 | 2856 | `		}` |
|        5 | 2857 | `	}` |
|        - | 2858 | `	/* Install in the interface container */` |
|   955376 | 2859 | `	SySetPut(&pMain->aInterface,(const void *)&pInterface);` |
|        - | 2860 | `	/* Install interface method stubs into the implementing class.` |
|        - | 2861 | `	 * Methods already defined in the class take precedence (they satisfy` |
|        - | 2862 | `	 * the interface contract). Stubs retain PH7_CLASS_ATTR_ABSTRACT so` |
|        - | 2863 | `	 * the unified check in GenStateCheckAbstractMethods catches missing ones.` |
|        - | 2864 | `	 */` |
|        - | 2865 | `	{` |
|        - | 2866 | `		ph7_class_method *pMeth;` |
|        - | 2867 | `		SyHashEntry *pMEntry;` |
|        - | 2868 | `		SyString *pMName;` |
|   955376 | 2869 | `		SyHashResetLoopCursor(&pInterface->hMethod);` |
|  4281238 | 2870 | `		while((pMEntry = SyHashGetNextEntry(&pInterface->hMethod)) != 0 ){` |
|  2847560 | 2871 | `			pMeth = (ph7_class_method *)pMEntry->pUserData;` |
|  2847560 | 2872 | `			pMName = &pMeth->sFunc.sName;` |
|  2847560 | 2873 | `			if( SyHashGet(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte) == 0 ){` |
|     8546 | 2874 | `				rc = SyHashInsert(&pMain->hMethod,(const void *)pMName->zString,pMName->nByte,pMeth);` |
|     8546 | 2875 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 2876 | `					return rc;` |
|        - | 2877 | `				}` |
|     4265 | 2878 | `			}` |
|        5 | 2879 | `		}` |
|        - | 2880 | `	}` |
|   955376 | 2881 | `	return SXRET_OK;` |
|   477069 | 2882 | `}` |
|        - | 2883 | `/*` |
|        - | 2884 | ` * Create a class instance [i.e: Object in the PHP jargon] at run-time.` |
|        - | 2885 | ` * The following function is called when an object is created at run-time` |
|        - | 2886 | ` * typically when the PH7_OP_NEW/PH7_OP_CLONE instructions are executed.` |
|        - | 2887 | ` * Notes on object creation.` |
|        - | 2888 | ` *` |
|        - | 2889 | ` * According to PHP language reference manual.` |
|        - | 2890 | ` * To create an instance of a class, the new keyword must be used. An object will always` |
|        - | 2891 | ` * be created unless the object has a constructor defined that throws an exception on error.` |
|        - | 2892 | ` * Classes should be defined before instantiation (and in some cases this is a requirement).` |
|        - | 2893 | ` * If a string containing the name of a class is used with new, a new instance of that class` |
|        - | 2894 | ` * will be created. If the class is in a namespace, its fully qualified name must be used when` |
|        - | 2895 | ` * doing this.` |
|        - | 2896 | ` * Example #3 Creating an instance` |
|        - | 2897 | ` * <?php` |
|        - | 2898 | ` *  $instance = new SimpleClass();` |
|        - | 2899 | ` *   // This can also be done with a variable:` |
|        - | 2900 | ` * $className = 'Foo';` |
|        - | 2901 | ` * $instance = new $className(); // Foo()` |
|        - | 2902 | ` * ?>` |
|        - | 2903 | ` * In the class context, it is possible to create a new object by new self and new parent.` |
|        - | 2904 | ` * When assigning an already created instance of a class to a new variable, the new variable` |
|        - | 2905 | ` * will access the same instance as the object that was assigned. This behaviour is the same` |
|        - | 2906 | ` * when passing instances to a function. A copy of an already created object can be made by` |
|        - | 2907 | ` * cloning it.` |
|        - | 2908 | ` * Example #4 Object Assignment` |
|        - | 2909 | ` * <?php` |
|        - | 2910 | ` *  class SimpleClass(){` |
|        - | 2911 | ` *    public $var;` |
|        - | 2912 | ` *  };` |
|        - | 2913 | ` *  $instance = new SimpleClass();` |
|        - | 2914 | ` *  $assigned   =  $instance;` |
|        - | 2915 | ` *  $reference  =& $instance;` |
|        - | 2916 | ` *  $instance->var = '$assigned will have this value';` |
|        - | 2917 | ` *  $instance = null; // $instance and $reference become null` |
|        - | 2918 | ` *  var_dump($instance);` |
|        - | 2919 | ` *  var_dump($reference);` |
|        - | 2920 | ` *  var_dump($assigned);` |
|        - | 2921 | ` * ?>` |
|        - | 2922 | ` * The above example will output:` |
|        - | 2923 | ` * NULL` |
|        - | 2924 | ` * NULL` |
|        - | 2925 | ` * object(SimpleClass)#1 (1) {` |
|        - | 2926 | ` *  ["var"]=>` |
|        - | 2927 | ` *    string(30) "$assigned will have this value"` |
|        - | 2928 | ` * }` |
|        - | 2929 | ` * Example #5 Creating new objects` |
|        - | 2930 | ` * <?php` |
|        - | 2931 | ` * class Test` |
|        - | 2932 | ` * {` |
|        - | 2933 | ` *   static public function getNew()` |
|        - | 2934 | ` *   {` |
|        - | 2935 | ` *       return new static;` |
|        - | 2936 | ` *   }` |
|        - | 2937 | ` * }` |
|        - | 2938 | ` * class Child extends Test` |
|        - | 2939 | ` * {}` |
|        - | 2940 | ` * $obj1 = new Test();` |
|        - | 2941 | ` * $obj2 = new $obj1;` |
|        - | 2942 | ` * var_dump($obj1 !== $obj2);` |
|        - | 2943 | ` * $obj3 = Test::getNew();` |
|        - | 2944 | ` * var_dump($obj3 instanceof Test);` |
|        - | 2945 | ` * $obj4 = Child::getNew();` |
|        - | 2946 | ` * var_dump($obj4 instanceof Child);` |
|        - | 2947 | ` * ?>` |
|        - | 2948 | ` * The above example will output:` |
|        - | 2949 | ` * bool(true)` |
|        - | 2950 | ` * bool(true)` |
|        - | 2951 | ` * bool(true)` |
|        - | 2952 | ` * Note that Symisc Systems have introduced powerfull extension to` |
|        - | 2953 | ` * OO subsystem. For example a class attribute may have any complex` |
|        - | 2954 | ` * expression associated with it when declaring the attribute unlike` |
|        - | 2955 | ` * the standard PHP engine which would allow a single value.` |
|        - | 2956 | ` * Example:` |
|        - | 2957 | ` *  class myClass{` |
|        - | 2958 | ` *    public $var = 25<<1+foo()/bar();` |
|        - | 2959 | ` *  };` |
|        - | 2960 | ` * Refer to the official documentation for more information.` |
|        - | 2961 | ` */` |
|  1680820 | 2962 | `static ph7_class_instance * NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|        5 | 2963 | `{` |
|        - | 2964 | `	ph7_class_instance *pThis;` |
|        - | 2965 | `	/* Allocate a new instance */` |
|  1680825 | 2966 | `	pThis = (ph7_class_instance *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_class_instance));` |
|  1680825 | 2967 | `	if( pThis == 0 ){` |
|      ! 0 | 2968 | `		return 0;` |
|        - | 2969 | `	}` |
|        - | 2970 | `	/* Zero the structure */` |
|  1680825 | 2971 | `	SyZero(pThis,sizeof(ph7_class_instance));` |
|        - | 2972 | `	/* Initialize fields */` |
|  1680825 | 2973 | `	pThis->iRef = 1;` |
|  1680825 | 2974 | `	pThis->pVm = pVm;` |
|  1680825 | 2975 | `	pThis->pClass = pClass;` |
|        - | 2976 | `	/* Assign a fresh monotonic object handle id (clones get their own, like PHP). */` |
|  1680825 | 2977 | `	pThis->nObjId = pVm->nNextObjId++;` |
|        - | 2978 | `	/* Size the property table to the class, not to the VM's default of sixteen` |
|        - | 2979 | `	 * buckets. An object holds the attributes its class declares -- the frame` |
|        - | 2980 | `	 * builder files one record per entry of pClass->hAttr and nothing else -- so` |
|        - | 2981 | `	 * the count is known here exactly, before a single property is installed.` |
|        - | 2982 | `	 * The census put the average object at 4.6 properties in a table built for` |
|        - | 2983 | `	 * forty-eight, which was 4.40 MB of mostly-zero bucket arrays.` |
|        - | 2984 | `	 *` |
|        - | 2985 | `	 * Asked for one bucket PER ENTRY, not for the fill factor's three. Dividing by` |
|        - | 2986 | `	 * SXHASH_FILL_FACTOR is what the table's own growth rule considers full, and it` |
|        - | 2987 | `	 * would have put the average object at 2.3 properties per bucket where the` |
|        - | 2988 | `	 * sixteen-bucket default had 0.29 -- trading memory this box can measure for` |
|        - | 2989 | `	 * property-lookup time it cannot. One bucket per entry keeps the walk at about` |
|        - | 2990 | `	 * one node, still costs a quarter of the default, and the two sizings differ by` |
|        - | 2991 | `	 * 0.52 MB of peak (136.16 MB against 136.68) -- which is the right 0.52 MB to` |
|        - | 2992 | `	 * leave on the table. SyHashInitSized rounds up to a power of two and floors it` |
|        - | 2993 | `	 * at 2, and a class that outgrows the estimate doubles exactly as before. */` |
|  1680825 | 2994 | `	SyHashInitSized(&pThis->hAttr,&pVm->sAllocator,0,0,pClass->hAttr.nEntry);` |
|  1680825 | 2995 | `	return pThis;` |
|   840247 | 2996 | `}` |
|        - | 2997 | `/*` |
|        - | 2998 | ` * Wrapper around the NewClassInstance() function defined above.` |
|        - | 2999 | ` * See the block comment above for more information.` |
|        - | 3000 | ` */` |
|  1679380 | 3001 | `PH7_PRIVATE ph7_class_instance * PH7_NewClassInstance(ph7_vm *pVm,ph7_class *pClass)` |
|        5 | 3002 | `{` |
|        - | 3003 | `	ph7_class_instance *pNew;` |
|        - | 3004 | `	sxi32 rc;` |
|  1679385 | 3005 | `	pNew = NewClassInstance(&(*pVm),&(*pClass));` |
|  1679385 | 3006 | `	if( pNew == 0 ){` |
|      ! 0 | 3007 | `		return 0;` |
|        - | 3008 | `	}` |
|        - | 3009 | `	/* Associate a private VM frame with this class instance */` |
|  1679385 | 3010 | `	rc = PH7_VmCreateClassInstanceFrame(&(*pVm),pNew);` |
|  1679385 | 3011 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3012 | `		SyMemBackendPoolFree(&pVm->sAllocator,pNew);` |
|      ! 0 | 3013 | `		return 0;` |
|        - | 3014 | `	}` |
|        - | 3015 | `	/* php stamps a Throwable's file/line at CREATION, not in its constructor, so a` |
|        - | 3016 | `	 * subclass that overrides __construct without calling parent::__construct still` |
|        - | 3017 | `	 * reports the right site. Every instantiation path lands here. */` |
|  1679385 | 3018 | `	PH7_VmStampThrowableSite(&(*pVm),pNew);` |
|        - | 3019 | `	/* php's create_object handler, resolved through the ANCESTORS the way the` |
|        - | 3020 | `	 * teardown one is: a subclass of a native class whose slots are SEEDED at` |
|        - | 3021 | ``	 * `new` (ZipArchive's six) must start with the same six. */`` |
|        - | 3022 | `	{` |
|  1679385 | 3023 | `		ph7_class *pOwner = pClass;` |
|  3561346 | 3024 | `		while( pOwner && pOwner->xNew == 0 ){` |
|  1881966 | 3025 | `			pOwner = pOwner->pBase;` |
|        5 | 3026 | `		}` |
|  1679385 | 3027 | `		if( pOwner && pOwner->xNew ){` |
|      366 | 3028 | `			pOwner->xNew(&(*pVm),pNew);` |
|      181 | 3029 | `		}` |
|        - | 3030 | `	}` |
|  1679385 | 3031 | `	return pNew;` |
|   839527 | 3032 | `}` |
|        - | 3033 | `/*` |
|        - | 3034 | ` * Open a private walk of this object's property table.` |
|        - | 3035 | ` *` |
|        - | 3036 | ` * Every consumer that hands PHP code the control flow between two attributes --` |
|        - | 3037 | `` * `foreach ($o as $k => $v)`, `array_walk($o, $fn)` -- must own its position`` |
|        - | 3038 | ` * rather than share the SyHash's embedded cursor: php iterates each walk` |
|        - | 3039 | ` * independently (nested loops over one object do not rewind each other), and the` |
|        - | 3040 | ` * body it runs in between can add or remove a property. The instance keeps the` |
|        - | 3041 | ` * list of open walks so those two mutations can fix the cursors up; the walker` |
|        - | 3042 | ` * MUST close it on every exit path, or the next mutation walks a recycled slot.` |
|        - | 3043 | ` */` |
|      148 | 3044 | `PH7_PRIVATE void PH7_ClassInstanceIterOpen(ph7_class_instance *pThis,PH7_AttrIter *pIter)` |
|        4 | 3045 | `{` |
|      152 | 3046 | `	pIter->pCursor = SyHashFirstEntry(&pThis->hAttr);` |
|      152 | 3047 | `	pIter->pNextIter = pThis->pActiveIters;` |
|      152 | 3048 | `	pThis->pActiveIters = pIter;` |
|      152 | 3049 | `}` |
|        - | 3050 | `/*` |
|        - | 3051 | ` * The next attribute entry, or 0 when the walk is exhausted. The cursor is` |
|        - | 3052 | ` * advanced BEFORE the entry is handed out, exactly like SyHashGetNextEntry:` |
|        - | 3053 | ` * php's own iteration standing on an entry is free to unset() it.` |
|        - | 3054 | ` */` |
|      688 | 3055 | `PH7_PRIVATE SyHashEntry * PH7_ClassInstanceIterNext(PH7_AttrIter *pIter)` |
|        4 | 3056 | `{` |
|      692 | 3057 | `	SyHashEntry *pEntry = pIter->pCursor;` |
|      692 | 3058 | `	if( pEntry ){` |
|      550 | 3059 | `		pIter->pCursor = SyHashEntryNext(pEntry);` |
|      273 | 3060 | `	}` |
|      692 | 3061 | `	return pEntry;` |
|        4 | 3062 | `}` |
|      146 | 3063 | `PH7_PRIVATE void PH7_ClassInstanceIterClose(ph7_class_instance *pThis,PH7_AttrIter *pIter)` |
|        4 | 3064 | `{` |
|      150 | 3065 | `	PH7_AttrIter **ppLink = &pThis->pActiveIters;` |
|      152 | 3066 | `	while( *ppLink ){` |
|      152 | 3067 | `		if( *ppLink == pIter ){` |
|      150 | 3068 | `			*ppLink = pIter->pNextIter;` |
|      150 | 3069 | `			pIter->pNextIter = 0;` |
|      150 | 3070 | `			pIter->pCursor = 0;` |
|      150 | 3071 | `			return;` |
|        - | 3072 | `		}` |
|        3 | 3073 | `		ppLink = &(*ppLink)->pNextIter;` |
|        1 | 3074 | `	}` |
|       77 | 3075 | `}` |
|        - | 3076 | `/*` |
|        - | 3077 | ` * Remove one attribute entry from the instance, advancing any open walk parked` |
|        - | 3078 | `` * on it first. The ONLY door for an `unset($o->p)`-shaped removal: the entry is`` |
|        - | 3079 | ` * freed here, so a walker still holding it would read a recycled pool slot on` |
|        - | 3080 | ` * its next step (php visits the properties AFTER the deleted one, and so does` |
|        - | 3081 | ` * this).` |
|        - | 3082 | ` */` |
|       74 | 3083 | `PH7_PRIVATE void PH7_ClassInstanceDeleteAttrEntry(ph7_class_instance *pThis,SyHashEntry *pEntry)` |
|        3 | 3084 | `{` |
|        - | 3085 | `	PH7_AttrIter *pIter;` |
|       93 | 3086 | `	for( pIter = pThis->pActiveIters ; pIter ; pIter = pIter->pNextIter ){` |
|       17 | 3087 | `		if( pIter->pCursor == pEntry ){` |
|        9 | 3088 | `			pIter->pCursor = SyHashEntryNext(pEntry);` |
|        4 | 3089 | `		}` |
|        9 | 3090 | `	}` |
|       77 | 3091 | `	SyHashDeleteEntry2(pEntry);` |
|       77 | 3092 | `}` |
|        - | 3093 | `/*` |
|        - | 3094 | ` * The mirror: an attribute APPENDED to the table (a dynamic property created by` |
|        - | 3095 | ` * the loop body, a declared one re-created after unset()) re-arms any walk that` |
|        - | 3096 | `` * has run off the end -- php walks the LIVE table, so `foreach ($o as ...)` over`` |
|        - | 3097 | ` * a stdClass whose body keeps adding properties keeps visiting them. A walker` |
|        - | 3098 | ` * with a NULL cursor is always mid-walk: it unregisters as soon as it stops.` |
|        - | 3099 | ` */` |
|     8036 | 3100 | `PH7_PRIVATE void PH7_ClassInstanceAttrAppended(ph7_class_instance *pThis,SyHashEntry *pEntry)` |
|        5 | 3101 | `{` |
|        - | 3102 | `	PH7_AttrIter *pIter;` |
|     8041 | 3103 | `	if( pEntry == 0 ){` |
|       13 | 3104 | `		return;` |
|        - | 3105 | `	}` |
|     8035 | 3106 | `	for( pIter = pThis->pActiveIters ; pIter ; pIter = pIter->pNextIter ){` |
|        7 | 3107 | `		if( pIter->pCursor == 0 ){` |
|        7 | 3108 | `			pIter->pCursor = pEntry;` |
|        3 | 3109 | `		}` |
|        4 | 3110 | `	}` |
|     4023 | 3111 | `}` |
|        - | 3112 | `/*` |
|        - | 3113 | ` * Spell ONE attribute the way php names it wherever an object's property table is` |
|        - | 3114 | ` * handed out as keys: a private property is "\0DeclaringClass\0name", a protected` |
|        - | 3115 | ` * one "\0*\0name", a public one its bare name. The NULs are real bytes (these` |
|        - | 3116 | ` * appends are length-based), which is what keeps two same-named members from` |
|        - | 3117 | `` * different visibility levels distinct. `pKey` must already be a STRING value; its`` |
|        - | 3118 | ` * buffer is reset first, so one carrier serves a whole walk.` |
|        - | 3119 | ` */` |
|     1246 | 3120 | `PH7_PRIVATE void PH7_ClassInstanceAttrKey(ph7_class_instance *pThis,VmClassAttr *pAttr,ph7_value *pKey)` |
|        5 | 3121 | `{` |
|     1251 | 3122 | `	SyString *pAttrName = &pAttr->pAttr->sName;` |
|     1251 | 3123 | `	SyBlobReset(&pKey->sBlob);` |
|     1251 | 3124 | `	if( pAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|        - | 3125 | `		/* php mangles a private key with the class that OWNS the property, and a` |
|        - | 3126 | `		 * trait's members are owned by the class that composed them -- so the key,` |
|        - | 3127 | `		 * and every wire format built on it (serialize, the (array) cast), names` |
|        - | 3128 | `		 * the class and never the trait. */` |
|      207 | 3129 | `		ph7_class *pDecl = PH7_VmMemberOwnerClass(pAttr->pAttr->pDeclClass,pThis->pClass);` |
|      207 | 3130 | `		PH7_MemObjStringAppend(pKey,"\0",1);` |
|      207 | 3131 | `		PH7_MemObjStringAppend(pKey,SyStringData(&pDecl->sName),SyStringLength(&pDecl->sName));` |
|      207 | 3132 | `		PH7_MemObjStringAppend(pKey,"\0",1);` |
|     1149 | 3133 | `	}else if( pAttr->pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
|      238 | 3134 | `		PH7_MemObjStringAppend(pKey,"\0*\0",3);` |
|      117 | 3135 | `	}` |
|     1251 | 3136 | `	PH7_MemObjStringAppend(pKey,pAttrName->zString,pAttrName->nByte);` |
|     1251 | 3137 | `}` |
|        - | 3138 | `/*` |
|        - | 3139 | ` * Is this slot part of the RAW property table php hands a walker -- the (array)` |
|        - | 3140 | ` * cast's slot walk, get_mangled_object_vars(), array_walk() over an object? A` |
|        - | 3141 | ` * class-level member is not the object's, a typed property never written is not` |
|        - | 3142 | ` * there yet, and a php 8.4 VIRTUAL hooked property has no backing store at all.` |
|        - | 3143 | ` */` |
|     6378 | 3144 | `PH7_PRIVATE int PH7_ClassInstanceAttrPresented(VmClassAttr *pAttr)` |
|        5 | 3145 | `{` |
|     4982 | 3146 | `	return !PH7_ATTR_UNPRESENTED(pAttr)` |
|     1788 | 3147 | `		&& !PH7_ClassAttrUninitialized(pAttr)` |
|     7308 | 3148 | `		&& (pAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL) == 0;` |
|        5 | 3149 | `}` |
|        - | 3150 | `/*` |
|        - | 3151 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon] attribute.` |
|        - | 3152 | ` * This function never fail.` |
|        - | 3153 | ` */` |
|  8649245 | 3154 | `static ph7_value * ExtractClassAttrValue(ph7_vm *pVm,VmClassAttr *pAttr)` |
|        5 | 3155 | `{` |
|        - | 3156 | `	/* Extract the value */` |
|        - | 3157 | `	ph7_value *pValue;` |
|  8649250 | 3158 | `	pValue = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pAttr->nIdx);` |
|  8649250 | 3159 | `	return pValue;` |
|        5 | 3160 | `}` |
|        - | 3161 | `/*` |
|        - | 3162 | ` * Perform a clone operation on a class instance [i.e: Object in the PHP jargon].` |
|        - | 3163 | ` * The following function is called when an object is cloned at run-time` |
|        - | 3164 | ` * typically when the PH7_OP_CLONE instruction is executed.` |
|        - | 3165 | ` * Notes on object cloning.` |
|        - | 3166 | ` *` |
|        - | 3167 | ` * According to PHP language reference manual.` |
|        - | 3168 | ` * Creating a copy of an object with fully replicated properties is not always the wanted behavior.` |
|        - | 3169 | ` * A good example of the need for copy constructors. Another example is if your object holds a reference` |
|        - | 3170 | ` * to another object which it uses and when you replicate the parent object you want to create` |
|        - | 3171 | ` * a new instance of this other object so that the replica has its own separate copy.` |
|        - | 3172 | ` * An object copy is created by using the clone keyword (which calls the object's __clone() method if possible).` |
|        - | 3173 | ` * An object's __clone() method cannot be called directly.` |
|        - | 3174 | ` * $copy_of_object = clone $object;` |
|        - | 3175 | ` * When an object is cloned, PHP 5 will perform a shallow copy of all of the object's properties.` |
|        - | 3176 | ` * Any properties that are references to other variables, will remain references.` |
|        - | 3177 | ` * Once the cloning is complete, if a __clone() method is defined, then the newly created object's __clone() method` |
|        - | 3178 | ` * will be called, to allow any necessary properties that need to be changed.` |
|        - | 3179 | ` * Example #1 Cloning an object` |
|        - | 3180 | ` * <?php` |
|        - | 3181 | ` * class SubObject` |
|        - | 3182 | ` * {` |
|        - | 3183 | ` *   static $instances = 0;` |
|        - | 3184 | ` *   public $instance;` |
|        - | 3185 | ` *` |
|        - | 3186 | ` *   public function __construct() {` |
|        - | 3187 | ` *       $this->instance = ++self::$instances;` |
|        - | 3188 | ` *   }` |
|        - | 3189 | ` *` |
|        - | 3190 | ` *   public function __clone() {` |
|        - | 3191 | ` *       $this->instance = ++self::$instances;` |
|        - | 3192 | ` *   }` |
|        - | 3193 | ` * }` |
|        - | 3194 | ` *` |
|        - | 3195 | ` * class MyCloneable` |
|        - | 3196 | ` * {` |
|        - | 3197 | ` *   public $object1;` |
|        - | 3198 | ` *   public $object2;` |
|        - | 3199 | ` *` |
|        - | 3200 | ` *   function __clone()` |
|        - | 3201 | ` *   {` |
|        - | 3202 | ` *       // Force a copy of this->object, otherwise` |
|        - | 3203 | ` *       // it will point to same object.` |
|        - | 3204 | ` *       $this->object1 = clone $this->object1;` |
|        - | 3205 | ` *   }` |
|        - | 3206 | ` * }` |
|        - | 3207 | ` * $obj = new MyCloneable();` |
|        - | 3208 | ` * $obj->object1 = new SubObject();` |
|        - | 3209 | ` * $obj->object2 = new SubObject();` |
|        - | 3210 | ` * $obj2 = clone $obj;` |
|        - | 3211 | ` * print("Original Object:\n");` |
|        - | 3212 | ` * print_r($obj);` |
|        - | 3213 | ` * print("Cloned Object:\n");` |
|        - | 3214 | ` * print_r($obj2);` |
|        - | 3215 | ` * ?>` |
|        - | 3216 | ` * The above example will output:` |
|        - | 3217 | ` * Original Object:` |
|        - | 3218 | ` * MyCloneable Object` |
|        - | 3219 | ` * (` |
|        - | 3220 | ` *   [object1] => SubObject Object` |
|        - | 3221 | ` *       (` |
|        - | 3222 | ` *           [instance] => 1` |
|        - | 3223 | ` *       )` |
|        - | 3224 | ` *` |
|        - | 3225 | ` *   [object2] => SubObject Object` |
|        - | 3226 | ` *       (` |
|        - | 3227 | ` *           [instance] => 2` |
|        - | 3228 | ` *       )` |
|        - | 3229 | ` *` |
|        - | 3230 | ` * )` |
|        - | 3231 | ` * Cloned Object:` |
|        - | 3232 | ` * MyCloneable Object` |
|        - | 3233 | ` * (` |
|        - | 3234 | ` *   [object1] => SubObject Object` |
|        - | 3235 | ` *       (` |
|        - | 3236 | ` *           [instance] => 3` |
|        - | 3237 | ` *       )` |
|        - | 3238 | ` *` |
|        - | 3239 | ` *   [object2] => SubObject Object` |
|        - | 3240 | ` *       (` |
|        - | 3241 | ` *           [instance] => 2` |
|        - | 3242 | ` *       )` |
|        - | 3243 | ` * )` |
|        - | 3244 | ` */` |
|        - | 3245 | `/*` |
|        - | 3246 | `` * Is `clone` refused for this class? php's uncloneable internal classes refuse`` |
|        - | 3247 | `` * for their USER SUBCLASSES too -- `class M extends IteratorIterator {}` makes`` |
|        - | 3248 | `` * `clone $m` the same catchable Error, named after M -- because the refusal is`` |
|        - | 3249 | ` * the inherited clone_obj handler, not the class's own row. So the flag is` |
|        - | 3250 | ` * consulted up the base chain, not on the instance's class alone. (A subclass` |
|        - | 3251 | ` * declaring its own __clone() changes nothing there either: php never reaches` |
|        - | 3252 | ` * it, and neither does this engine -- the refusal answers first.)` |
|        - | 3253 | ` */` |
|      664 | 3254 | `PH7_PRIVATE int PH7_ClassIsUncloneable(ph7_class *pClass)` |
|        5 | 3255 | `{` |
|        - | 3256 | `	ph7_class *pC;` |
|     1239 | 3257 | `	for( pC = pClass ; pC ; pC = pC->pBase ){` |
|      741 | 3258 | `		if( pC->iFlags & PH7_CLASS_NOCLONE ){` |
|      170 | 3259 | `			return 1;` |
|        - | 3260 | `		}` |
|      290 | 3261 | `	}` |
|      503 | 3262 | `	return 0;` |
|      337 | 3263 | `}` |
|     1440 | 3264 | `PH7_PRIVATE ph7_class_instance * PH7_CloneClassInstance(ph7_class_instance *pSrc)` |
|        5 | 3265 | `{` |
|        - | 3266 | `	ph7_class_instance *pClone;` |
|        - | 3267 | `	ph7_class_method *pMethod;` |
|        - | 3268 | `	SyHashEntry *pEntry2;` |
|        - | 3269 | `	SyHashEntry *pEntry;` |
|        - | 3270 | `	ph7_vm *pVm;` |
|        - | 3271 | `	sxi32 rc;` |
|        - | 3272 | `	/* Allocate a new instance */` |
|     1445 | 3273 | `	pVm = pSrc->pVm;` |
|     1445 | 3274 | `	pClone = NewClassInstance(pVm,pSrc->pClass);` |
|     1445 | 3275 | `	if( pClone == 0 ){` |
|      ! 0 | 3276 | `		return 0;` |
|        - | 3277 | `	}` |
|        - | 3278 | `	/* Associate a private VM frame with this class instance */` |
|     1445 | 3279 | `	rc = PH7_VmCreateClassInstanceFrame(pVm,pClone);` |
|     1445 | 3280 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3281 | `		SyMemBackendPoolFree(&pVm->sAllocator,pClone);` |
|      ! 0 | 3282 | `		return 0;` |
|        - | 3283 | `	}` |
|        - | 3284 | `	/* A clone of an object whose LAZY native properties are installed has them` |
|        - | 3285 | `	 * too: php clones the C struct the table is written from, so the copy shows` |
|        - | 3286 | ``	 * what the original shows. The frame above skipped them (as it does at `new`),`` |
|        - | 3287 | `	 * so install them before the value copy below looks for the same-named slots. */` |
|     1445 | 3288 | `	if( pSrc->iFlags & VM_INSTANCE_LAZY_DONE ){` |
|        7 | 3289 | `		PH7_NativeMaterializeLazy(pVm,pClone);` |
|        3 | 3290 | `	}` |
|        - | 3291 | `	/* Duplicate object values. Iterate the SOURCE attributes and copy each into` |
|        - | 3292 | `	 * the clone's same-named slot (looked up by name, so order/count differences` |
|        - | 3293 | `	 * from dynamic properties don't matter). A dynamic (runtime-added) property` |
|        - | 3294 | `	 * has no declared counterpart in the clone, so synthesize it first — without` |
|        - | 3295 | `	 * this, a clone of a stdClass would silently lose all its dynamic properties. */` |
|     1445 | 3296 | `	SyHashResetLoopCursor(&pSrc->hAttr);` |
|     8535 | 3297 | `	while((pEntry = SyHashGetNextEntry(&pSrc->hAttr)) != 0 ){` |
|     7095 | 3298 | `		VmClassAttr *pSrcAttr = (VmClassAttr *)pEntry->pUserData;` |
|     7095 | 3299 | `		VmClassAttr *pDestAttr = 0;` |
|     7095 | 3300 | `		ph7_value *pvSrc,*pvDest = 0;` |
|        - | 3301 | `		/* Duplicate non-static attribute */` |
|     7095 | 3302 | `		if( pSrcAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|        8 | 3303 | `			continue;` |
|        - | 3304 | `		}` |
|        - | 3305 | `		/* A LAZILY-FILLED slot goes back to unfilled: php's clone handler builds the` |
|        - | 3306 | `		 * new object and its own read fills the name again, so a cloned element whose` |
|        - | 3307 | ``		 * `children` had been read carries the name uninitialized -- absent from`` |
|        - | 3308 | `		 * get_object_vars() until the clone is asked for it. */` |
|     7091 | 3309 | `		if( pSrcAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_LAZYSLOT ){` |
|        5 | 3310 | `			continue;` |
|        - | 3311 | `		}` |
|        - | 3312 | `		/* By the source's own KEY: a private property of a BASE class is filed under` |
|        - | 3313 | `		 * php's mangled storage name, and matching on the attribute's plain name` |
|        - | 3314 | `		 * would copy it over the same-named slot of the object's own class. */` |
|     7087 | 3315 | `		pEntry2 = SyHashGet(&pClone->hAttr,pEntry->pKey,pEntry->nKeyLen);` |
|     7087 | 3316 | `		if( pEntry2 ){` |
|     7061 | 3317 | `			pDestAttr = (VmClassAttr *)pEntry2->pUserData;` |
|     7061 | 3318 | `			pvDest = ExtractClassAttrValue(pVm,pDestAttr);` |
|     3555 | 3319 | `		}else if( pSrcAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){` |
|        - | 3320 | `			/* Dynamic property: synthesize the matching slot on the clone. */` |
|       37 | 3321 | `			pvDest = PH7_VmCreateDynamicAttr(pVm,pClone,` |
|       24 | 3322 | `				SyStringData(&pSrcAttr->pAttr->sName),SyStringLength(&pSrcAttr->pAttr->sName),&pDestAttr);` |
|       15 | 3323 | `		}else if( pSrcAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_ONDEMAND ){` |
|        - | 3324 | `			/* An ON-DEMAND property is installed by the write that names it, so` |
|        - | 3325 | `			 * the clone's frame has no slot for one -- and php's copy carries it` |
|        - | 3326 | ``			 * (a cloned from-string DateInterval keeps its `date_string`). */`` |
|        3 | 3327 | `			VmRecreateDeclaredAttr(pVm,pClone,pSrcAttr->pAttr,&pDestAttr);` |
|        3 | 3328 | `			if( pDestAttr ){` |
|        3 | 3329 | `				pvDest = ExtractClassAttrValue(pVm,pDestAttr);` |
|        1 | 3330 | `			}` |
|        1 | 3331 | `		}` |
|        - | 3332 | `		/* Fetch the source value LAST: PH7_VmCreateDynamicAttr above may have` |
|        - | 3333 | `		 * reserved a slot, which used to reallocate pVm->aMemObj and dangle any` |
|        - | 3334 | `		 * ph7_value* obtained before it. Redundant since P1 (fixed segments);` |
|        - | 3335 | `		 * left for the harvest sweep. */` |
|     7087 | 3336 | `		pvSrc = ExtractClassAttrValue(pVm,pSrcAttr);` |
|     7087 | 3337 | `		if( (pSrcAttr->iState & VM_CLASS_ATTR_REFBOUND) && pDestAttr ){` |
|        - | 3338 | `			/* php preserves references across clone: the clone shares the SAME slot` |
|        - | 3339 | `			 * as the source property (both alias the referenced variable), rather` |
|        - | 3340 | `			 * than getting an independent value copy. Drop the clone's fresh private` |
|        - | 3341 | `			 * slot and repoint at the (already-pinned) shared slot. The REFBOUND flag` |
|        - | 3342 | `			 * is carried over by the iState copy below, so the clone's release also` |
|        - | 3343 | `			 * leaves the shared slot alone. */` |
|        5 | 3344 | `			if( pDestAttr->nIdx != pSrcAttr->nIdx ){` |
|        5 | 3345 | `				PH7_VmStoreFilterDrop(pVm,pDestAttr->pAttr,pDestAttr->nIdx);` |
|        5 | 3346 | `				PH7_VmUnsetMemObj(pVm,pDestAttr->nIdx,TRUE);` |
|        5 | 3347 | `				pDestAttr->nIdx = pSrcAttr->nIdx;` |
|        - | 3348 | `				/* The clone is a holder of the shared slot in its own right — take a pin` |
|        - | 3349 | `				 * for it, since its own release will give one back. */` |
|        5 | 3350 | `				VmPinMemObjSlotCounted(pVm,pDestAttr->nIdx);` |
|        3 | 3351 | `			}` |
|     7085 | 3352 | `		}else if( pvSrc && pvDest ){` |
|     7083 | 3353 | `			PH7_MemObjStore(pvSrc,pvDest);` |
|     3539 | 3354 | `		}` |
|        - | 3355 | `		/* Carry over the per-instance state so the clone matches the source:` |
|        - | 3356 | `		 * VM_CLASS_ATTR_UNINIT marks a typed property as not-yet-initialized` |
|        - | 3357 | `		 * and doubles as the readonly write-once latch — without this a clone` |
|        - | 3358 | `		 * would reset to uninitialized (losing the value's readiness) and a` |
|        - | 3359 | `		 * readonly property would become writable again. */` |
|     7087 | 3360 | `		if( pDestAttr ){` |
|     7087 | 3361 | `			pDestAttr->iState = pSrcAttr->iState;` |
|     3541 | 3362 | `		}` |
|        5 | 3363 | `	}` |
|        - | 3364 | `	/* A declared property unset() on the source is absent from the clone too (PHP). But the clone` |
|        - | 3365 | `	 * frame above materialized ALL declared attrs (with their defaults), so drop any clone attr whose` |
|        - | 3366 | `	 * name is not present on the source. Collect first, then delete — removing an entry mid-walk would` |
|        - | 3367 | `	 * free the node the SyHash loop cursor points at. */` |
|        - | 3368 | `	{` |
|        - | 3369 | `		SySet sDrop;` |
|     1445 | 3370 | `		SySetInit(&sDrop,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|     1445 | 3371 | `		SyHashResetLoopCursor(&pClone->hAttr);` |
|     8539 | 3372 | `		while((pEntry = SyHashGetNextEntry(&pClone->hAttr)) != 0 ){` |
|     7099 | 3373 | `			VmClassAttr *pCloneAttr = (VmClassAttr *)pEntry->pUserData;` |
|     7099 | 3374 | `			if( pCloneAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|        6 | 3375 | `				continue;` |
|        - | 3376 | `			}` |
|     7095 | 3377 | `			if( SyHashGet(&pSrc->hAttr,pEntry->pKey,pEntry->nKeyLen) == 0 ){` |
|        5 | 3378 | `				SySetPut(&sDrop,(const void *)&pEntry);` |
|        2 | 3379 | `			}` |
|        5 | 3380 | `		}` |
|     1445 | 3381 | `		if( SySetUsed(&sDrop) > 0 ){` |
|        5 | 3382 | `			SyHashEntry **apDrop = (SyHashEntry **)SySetBasePtr(&sDrop);` |
|        - | 3383 | `			sxu32 i;` |
|        9 | 3384 | `			for( i = 0 ; i < SySetUsed(&sDrop) ; ++i ){` |
|        5 | 3385 | `				VmClassAttr *pVmAttr = (VmClassAttr *)apDrop[i]->pUserData;` |
|        5 | 3386 | `				SyHashDeleteEntry(&pClone->hAttr,apDrop[i]->pKey,apDrop[i]->nKeyLen,0);` |
|        5 | 3387 | `				PH7_VmReleaseInstanceAttr(pVm,pVmAttr);` |
|        3 | 3388 | `			}` |
|        2 | 3389 | `		}` |
|     1445 | 3390 | `		SySetRelease(&sDrop);` |
|        - | 3391 | `	}` |
|        - | 3392 | `	/* A copy of a Closure names the same function, so it is a new holder of it --` |
|        - | 3393 | ``	 * `clone $f`, and Closure::bindTo()/bind(), which clone. Without this the`` |
|        - | 3394 | `	 * ORIGINAL's death would free a per-instantiation body the copy still calls.` |
|        - | 3395 | `	 * A no-op for every other class (one pointer compare). */` |
|     1445 | 3396 | `	PH7_VmClosureInstanceRef(pVm,pClone,1);` |
|        - | 3397 | `	/* The native clone hook (php's clone_obj handler): what the copy MEANS for a` |
|        - | 3398 | `	 * class whose instances stand for engine-side state -- a DOM wrapper's copy` |
|        - | 3399 | `	 * is a copy of the node. The nearest ancestor's hook serves a user subclass,` |
|        - | 3400 | `	 * which is php's handler inheritance. Runs before any __clone(), as php's` |
|        - | 3401 | `	 * handler does. */` |
|        - | 3402 | `	{` |
|        - | 3403 | `		ph7_class *pHook;` |
|     2863 | 3404 | `		for( pHook = pClone->pClass ; pHook ; pHook = pHook->pBase ){` |
|     1465 | 3405 | `			if( pHook->xClone ){` |
|       44 | 3406 | `				pHook->xClone(pVm,pClone,pSrc);` |
|       44 | 3407 | `				break;` |
|        - | 3408 | `			}` |
|      714 | 3409 | `		}` |
|        - | 3410 | `	}` |
|        - | 3411 | `	/* call the __clone method on the cloned object if available */` |
|     1445 | 3412 | `	pMethod = PH7_ClassExtractMethod(pClone->pClass,"__clone",sizeof("__clone")-1);` |
|     1445 | 3413 | `	if( pMethod ){` |
|      101 | 3414 | `		if( pMethod->iCloneDepth < 16 ){` |
|       99 | 3415 | `			pMethod->iCloneDepth++;` |
|        - | 3416 | `			/* PHP 8.3: __clone() may re-initialize the clone's readonly` |
|        - | 3417 | `			 * properties. Flag the instance so the readonly store guard allows` |
|        - | 3418 | `			 * it for the duration of the call. */` |
|       99 | 3419 | `			pClone->iFlags \|= VM_INSTANCE_CLONING;` |
|       99 | 3420 | `			PH7_VmCallClassMethod(pVm,pClone,pMethod,0,0,0);` |
|       99 | 3421 | `			pClone->iFlags &= ~VM_INSTANCE_CLONING;` |
|       51 | 3422 | `		}else{` |
|        - | 3423 | `			/* Nesting limit reached */` |
|        3 | 3424 | `			PH7_VmThrowError(pVm,0,PH7_CTX_ERR,"Object clone limit reached,no more call to __clone()");` |
|        - | 3425 | `		}` |
|        - | 3426 | `		/* Reset the cursor */` |
|      101 | 3427 | `		pMethod->iCloneDepth = 0;` |
|       49 | 3428 | `	}` |
|        - | 3429 | `	/* Return the cloned object */` |
|     1445 | 3430 | `	return pClone;` |
|      725 | 3431 | `}` |
|        - | 3432 | `/* CLASS_INSTANCE_DESTROYED moved to ph7int.h: the cycle collector has to know` |
|        - | 3433 | ` * an instance that is already mid-release when it walks one. */` |
|        - | 3434 | `/*` |
|        - | 3435 | ` * Free the per-instance allocations owned by ONE object attribute: its value slot (+ the typed-slot` |
|        - | 3436 | ` * enforcement entry), the synthesized ph7_class_attr for a dynamic (runtime-added) property, and the` |
|        - | 3437 | ` * VmClassAttr wrapper itself. Does NOT touch the hAttr entry node — the caller removes it` |
|        - | 3438 | `` * (`unset($o->p)` via SyHashDeleteEntry2; instance teardown via the wholesale SyHashRelease, so it must`` |
|        - | 3439 | ` * not delete entries mid-walk). Shared by PH7_ClassInstanceRelease and the OP_MEMBER unset path.` |
|        - | 3440 | ` */` |
| 10112304 | 3441 | `PH7_PRIVATE void PH7_VmReleaseInstanceAttr(ph7_vm *pVm, VmClassAttr *pVmAttr)` |
|        5 | 3442 | `{` |
| 10112309 | 3443 | `	if( pVmAttr->iState & (VM_CLASS_ATTR_REFBOUND\|VM_CLASS_ATTR_REFSRCPIN) ){` |
|        - | 3444 | `` 		/* A property at either end of a reference (`$o->p =& $x` bound it, `$r =& $o->p` `` |
|        - | 3445 | `		 * made it a source) holds its value slot with a COUNTED PIN and shares it, so it` |
|        - | 3446 | `		 * must not be released here — but the property WAS one of its holders, so give the` |
|        - | 3447 | `		 * pin back. The slot (and its value, which used to stay alive for the rest of the` |
|        - | 3448 | `		 * script) goes if the property was the last thing holding it. A SOURCE still owns` |
|        - | 3449 | `		 * its declaration, so its typed-slot enforcement entry goes with it. */` |
|      188 | 3450 | `		if( (pVmAttr->iState & VM_CLASS_ATTR_REFBOUND) == 0 ){` |
|      132 | 3451 | `			PH7_VmStoreFilterDrop(pVm,pVmAttr->pAttr,pVmAttr->nIdx);` |
|       65 | 3452 | `		}` |
|      188 | 3453 | `		VmUnpinMemObjSlot(pVm,pVmAttr->nIdx);` |
| 10112216 | 3454 | `	}else if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - | 3455 | `		/* Drop any typed-property enforcement slot registered for this memobj, before the memobj` |
|        - | 3456 | `		 * is returned to the free list, so a future recycled slot does not inherit the stale entry. */` |
| 10110771 | 3457 | `		PH7_VmStoreFilterDrop(pVm,pVmAttr->pAttr,pVmAttr->nIdx);` |
| 10110771 | 3458 | `		if( !PH7_VmSlotDropOwnerHold(pVm,pVmAttr->nIdx) ){` |
|        - | 3459 | ``			/* Nobody else names the slot. When somebody does -- `$r =& $o->p`,`` |
|        - | 3460 | ``			 * `$a[] =& $o->p` -- the value is theirs to keep and theirs to release,`` |
|        - | 3461 | `			 * exactly as php's refcount makes it: unlinking it here took the array` |
|        - | 3462 | `			 * element with it and left the variable UNDEFINED. */` |
| 10110765 | 3463 | `			PH7_VmUnsetMemObj(pVm,pVmAttr->nIdx,TRUE);` |
|  5054661 | 3464 | `		}` |
|  5054664 | 3465 | `	}` |
|        - | 3466 | `	/* A dynamic property owns its synthesized ph7_class_attr (struct + inline name in one block) —` |
|        - | 3467 | `	 * free it here (the only place a per-instance pAttr is freed; declared attrs are class-owned). */` |
| 10112309 | 3468 | `	if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC ){` |
|      633 | 3469 | `		SyMemBackendFree(&pVm->sAllocator,pVmAttr->pAttr);` |
|      314 | 3470 | `	}` |
| 10112309 | 3471 | `	SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
| 10112309 | 3472 | `}` |
|        - | 3473 | `/*` |
|        - | 3474 | ` * Run this instance's __destruct exactly once, or raise the refusal that stands in for it.` |
|        - | 3475 | ` *` |
|        - | 3476 | ` * Called from two places: PH7_ClassInstanceRelease, where the object dies because nothing` |
|        - | 3477 | ` * refers to it any more, and the shutdown pass (VmCallShutdownDestructors), which reaches` |
|        - | 3478 | ` * every object a program left alive WITHOUT freeing it -- php's zend_objects_store_call_destructors` |
|        - | 3479 | ` * does exactly that, and the free that follows must not run the body a second time, which` |
|        - | 3480 | ` * is what CLASS_INSTANCE_DTOR_CALLED records.` |
|        - | 3481 | ` *` |
|        - | 3482 | ` * PH7_ClassInstanceCtorFailed below sets that same bit for php's other reason.` |
|        - | 3483 | ` */` |
|  1596203 | 3484 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallDestructor(ph7_class_instance *pThis)` |
|        5 | 3485 | `{` |
|        - | 3486 | `	ph7_class_method *pDestr;` |
|        - | 3487 | `	ph7_class *pClass;` |
|        - | 3488 | `	ph7_vm *pVm;` |
|  1596208 | 3489 | `	sxi32 rc = SXRET_OK;` |
|  1596208 | 3490 | `	if( pThis->iFlags & CLASS_INSTANCE_DTOR_CALLED ){` |
|   117006 | 3491 | `		return SXRET_OK;` |
|        - | 3492 | `	}` |
|        - | 3493 | `	/* Flagged whether or not there is a body to run, exactly as php flags its own` |
|        - | 3494 | `	 * (IS_OBJ_DESTRUCTOR_CALLED is set before the handler is even looked up). The` |
|        - | 3495 | `	 * shutdown pass sweeps the object pool until a round finds nothing unflagged, so` |
|        - | 3496 | `	 * an object with no __destruct at all has to come back flagged too. */` |
|  1479207 | 3497 | `	pThis->iFlags \|= CLASS_INSTANCE_DTOR_CALLED;` |
|  1479207 | 3498 | `	pVm = pThis->pVm;` |
|  1479207 | 3499 | `	pClass = pThis->pClass;` |
|  1479207 | 3500 | `	pDestr = PH7_ClassExtractMethod(pClass,"__destruct",sizeof("__destruct")-1);` |
|  1479207 | 3501 | `	if( pDestr && !pVm->bInReset ){` |
|        - | 3502 | `		/* php checks a non-public destructor's visibility HERE, against the scope` |
|        - | 3503 | `		 * the destruction happened in, and refuses with a sentence of its own: the` |
|        - | 3504 | `		 * engine reached for the method, so the message names the OBJECT's class` |
|        - | 3505 | `		 * and drops the word "method" the ordinary call refusal carries` |
|        - | 3506 | ``		 * (`Call to private B::__destruct() from global scope` for a `class B`` |
|        - | 3507 | ``		 * extends A` whose base declared it). Screening here rather than letting`` |
|        - | 3508 | `		 * the dispatcher speak is what keeps that wording; the call is then made` |
|        - | 3509 | `		 * unchecked, since this IS the check. */` |
|     2153 | 3510 | `		ph7_class *pDestrDecl = pDestr->sFunc.pUserData` |
|     1431 | 3511 | `			? (ph7_class *)pDestr->sFunc.pUserData : pClass;` |
|     1431 | 3512 | `		if( pDestr->iProtection != PH7_CLASS_PROT_PUBLIC` |
|      723 | 3513 | `		 && !PH7_VmClassMemberAccess(&(*pVm),pDestrDecl,&pDestr->sFunc.sName,` |
|        5 | 3514 | `			pDestr->iProtection,FALSE) ){` |
|        - | 3515 | `			SyBlob sErrMsg;` |
|       14 | 3516 | `			const char *zVis = pDestr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|        4 | 3517 | `				? "private" : "protected";` |
|       10 | 3518 | `			ph7_class *pScope = PH7_VmCallerScope(&(*pVm));` |
|       10 | 3519 | `			SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       10 | 3520 | `			if( pVm->bInShutdownDtor ){` |
|        - | 3521 | `				/* Reached from the shutdown pass, with no PHP frame under it. php tests` |
|        - | 3522 | ``				 * exactly that (`EG(current_execute_data) == NULL`) and answers a`` |
|        - | 3523 | `				 * different sentence at a different severity: an E_WARNING saying the` |
|        - | 3524 | `				 * call was ignored, after which the object is simply not destructed and` |
|        - | 3525 | `				 * the program is already over. The Error below is for a refusal a` |
|        - | 3526 | `				 * running program can still catch. */` |
|        8 | 3527 | `				SyBlobFormat(&sErrMsg,` |
|        - | 3528 | `					"Call to %s %z::__destruct() from global scope during shutdown ignored",` |
|        3 | 3529 | `					zVis,&pClass->sDisp);` |
|        8 | 3530 | `				SyBlobAppend(&sErrMsg,"\0",sizeof(char));` |
|        - | 3531 | `				/* Raised between two destructor bodies, so there is no frame to name:` |
|        - | 3532 | ``				 * php reports it `in Unknown on line 0`. */`` |
|        8 | 3533 | `				pVm->bNoFrameLoc = 1;` |
|        8 | 3534 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,(const char *)SyBlobData(&sErrMsg));` |
|        8 | 3535 | `				pVm->bNoFrameLoc = 0;` |
|        8 | 3536 | `				SyBlobRelease(&sErrMsg);` |
|        8 | 3537 | `				return SXRET_OK;` |
|        - | 3538 | `			}` |
|        3 | 3539 | `			if( pScope ){` |
|      ! 0 | 3540 | `				SyBlobFormat(&sErrMsg,"Call to %s %z::__destruct() from scope %z",` |
|      ! 0 | 3541 | `					zVis,&pClass->sDisp,&pScope->sDisp);` |
|      ! 0 | 3542 | `			}else{` |
|        3 | 3543 | `				SyBlobFormat(&sErrMsg,"Call to %s %z::__destruct() from global scope",` |
|        1 | 3544 | `					zVis,&pClass->sDisp);` |
|        - | 3545 | `			}` |
|        - | 3546 | `			/* Parked, not returned: this release has no channel back to the` |
|        - | 3547 | `			 * executor (nothing "called" the destruct), and the dispatcher's own` |
|        - | 3548 | `			 * screen used to do the parking for us through` |
|        - | 3549 | `			 * VmCallClassMethodWithMap. Without it the uncaught Error is printed` |
|        - | 3550 | `			 * and the program carries on past a statement php never reaches. */` |
|        4 | 3551 | `			VmBoundaryPark(&(*pVm),` |
|        1 | 3552 | `				VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|        3 | 3553 | `			SyBlobRelease(&sErrMsg);` |
|        2 | 3554 | `		}else{` |
|        - | 3555 | `			/* Invoke the destructor. Skipped during ph7_vm_reset() bulk teardown:` |
|        - | 3556 | `			 * running user PHP against a half-reset VM is unsafe (see bInReset).` |
|        - | 3557 | `			 *` |
|        - | 3558 | `			 * Pinned across the body rather than SET to a constant: reached from a` |
|        - | 3559 | `			 * release the count is 0 and any value keeps the nested unref off it, but` |
|        - | 3560 | `			 * the shutdown pass calls this on an object other names still hold, and` |
|        - | 3561 | `			 * flattening their count there would free it under them. php pins the same` |
|        - | 3562 | `			 * way (GC_ADDREF/GC_DELREF around dtor_obj), so a body that stores $this` |
|        - | 3563 | `			 * somewhere keeps the reference it gained. */` |
|     1428 | 3564 | `			sxu8 bPhase = pVm->bInShutdownDtor;` |
|        - | 3565 | `			VmResumeTarget sSaveResume;` |
|        - | 3566 | `			/* The body has a frame of its own, so php's "no execute_data" state ends` |
|        - | 3567 | `			 * here and resumes when it returns: an object the body itself drops --` |
|        - | 3568 | ``			 * `$this->p = null` on the last holder of a private-destructor object --`` |
|        - | 3569 | `			 * is refused with the catchable Error naming the running scope, not with` |
|        - | 3570 | `			 * the shutdown warning above. */` |
|     1428 | 3571 | `			pVm->bInShutdownDtor = 0;` |
|     1428 | 3572 | `			pThis->iRef += 2; /* Prevent garbage collection */` |
|        - | 3573 | `			/* A destructor runs in the MIDDLE of somebody else's control flow: the` |
|        - | 3574 | `			 * release that reaches it is usually a frame teardown on an unwind that` |
|        - | 3575 | `			 * is already carrying a throw. php hides the in-flight exception for the` |
|        - | 3576 | `			 * duration (zend_objects_store_del saves EG(exception), clears it, and` |
|        - | 3577 | `			 * puts it back afterwards) precisely so the body cannot observe or` |
|        - | 3578 | `			 * disturb it. PHL's in-place-catch resume record is that same in-flight` |
|        - | 3579 | `			 * state — it says "the throw now unwinding was already caught at frame F,` |
|        - | 3580 | `			 * pad P" — and a destructor body with a try/catch of its own writes a` |
|        - | 3581 | `			 * record when ITS catch finishes, overwriting the one the outer throw is` |
|        - | 3582 | ``			 * still owed. monolog's `Handler::__destruct` is exactly that shape`` |
|        - | 3583 | ``			 * (`try { $this->close(); } catch (Throwable) {}`), and the outer throw`` |
|        - | 3584 | `			 * then never landed: the script that was catching it simply ENDED.` |
|        - | 3585 | `			 * Save, clear, restore — and, like php, let a record the body LEAVES` |
|        - | 3586 | `			 * behind (a throw of its own still in flight) supersede the saved one. */` |
|     1428 | 3587 | `			VmSaveResumeTarget(pVm,&sSaveResume);` |
|     1428 | 3588 | `			VmClearResumeTarget(pVm);` |
|     1428 | 3589 | `			rc = PH7_VmCallMethodUnchecked(pVm,pThis,pDestr,0,0,0);` |
|     1428 | 3590 | `			if( pVm->pResumeFrame == 0 ){` |
|     1426 | 3591 | `				VmRestoreResumeTarget(pVm,&sSaveResume);` |
|      709 | 3592 | `			}` |
|     1428 | 3593 | `			pThis->iRef -= 2;` |
|     1428 | 3594 | `			pVm->bInShutdownDtor = bPhase;` |
|        - | 3595 | `		}` |
|      711 | 3596 | `	}` |
|        - | 3597 | `	/* SXERR_ABORT here means the body left an UNCAUGHT throwable (or exited). php runs` |
|        - | 3598 | `	 * its whole destructor phase under one zend_try, so the first one abandons every` |
|        - | 3599 | `	 * destructor still owed -- including the ones the symbol-table half would have` |
|        - | 3600 | `	 * reached, which is why the decision is recorded on the VM and not just returned. */` |
|  1479201 | 3601 | `	if( rc == SXERR_ABORT && pVm->bInShutdownDtor ){` |
|        6 | 3602 | `		pVm->bShutdownAborted = 1;` |
|        2 | 3603 | `	}` |
|  1479201 | 3604 | `	return rc;` |
|   797940 | 3605 | `}` |
|        - | 3606 | `/*` |
|        - | 3607 | ` * A constructor CALL raised, so this object never became one: php marks it` |
|        - | 3608 | ` * (zend_object_store_ctor_failed sets the very bit that records "the destructor` |
|        - | 3609 | ` * has been reached for") and its __destruct is therefore never run -- not when the` |
|        - | 3610 | `` * half-built object is dropped at the `new`, and not later either, because the mark`` |
|        - | 3611 | ` * lives on the object and follows it wherever the constructor happened to store` |
|        - | 3612 | `` * `$this` before it threw. PHL ran the destructor on both, so monolog's`` |
|        - | 3613 | `` * `Handler::__destruct` -- a `try { $this->close(); } catch {}` -- executed against`` |
|        - | 3614 | ` * an instance whose typed properties were still uninitialised, in the middle of the` |
|        - | 3615 | ` * unwind that was already carrying the constructor's own exception.` |
|        - | 3616 | ` *` |
|        - | 3617 | `` * Called at every door that CALLS a constructor: `new` itself, and Reflection's`` |
|        - | 3618 | ` * newInstance family (php flags at each of those and nowhere else).` |
|        - | 3619 | ` */` |
|   100957 | 3620 | `PH7_PRIVATE void PH7_ClassInstanceCtorFailed(ph7_class_instance *pThis)` |
|        5 | 3621 | `{` |
|   100962 | 3622 | `	if( pThis ){` |
|   100962 | 3623 | `		pThis->iFlags \|= CLASS_INSTANCE_DTOR_CALLED;` |
|    50478 | 3624 | `	}` |
|   100962 | 3625 | `}` |
|        - | 3626 | `/*` |
|        - | 3627 | ` * Release a class instance [i.e: Object in the PHP jargon] and invoke any defined destructor.` |
|        - | 3628 | ` * This routine is invoked as soon as there are no other references to a particular` |
|        - | 3629 | ` * class instance.` |
|        - | 3630 | ` */` |
|  1577533 | 3631 | `static void PH7_ClassInstanceRelease(ph7_class_instance *pThis)` |
|        5 | 3632 | `{` |
|        - | 3633 | `	SyHashEntry *pEntry;` |
|        - | 3634 | `	ph7_class *pClass;` |
|        - | 3635 | `	ph7_vm *pVm;` |
|  1577538 | 3636 | `	if( pThis->iFlags & CLASS_INSTANCE_DESTROYED ){` |
|        - | 3637 | `		/*` |
|        - | 3638 | `		 * Already destroyed,return immediately.` |
|        - | 3639 | `		 * This could happend if someone perform unset($this) in the destructor body.` |
|        - | 3640 | `		 */` |
|      ! 0 | 3641 | `		return;` |
|        - | 3642 | `	}` |
|        - | 3643 | `	/* Mark as destroyed */` |
|  1577538 | 3644 | `	pThis->iFlags \|= CLASS_INSTANCE_DESTROYED;` |
|  1577538 | 3645 | `	pVm = pThis->pVm;` |
|  1577538 | 3646 | `	pClass = pThis->pClass;` |
|        - | 3647 | `	/* Invoke any defined destructor if available (a no-op once the shutdown pass` |
|        - | 3648 | `	 * has already run it) */` |
|  1577538 | 3649 | `	PH7_ClassInstanceCallDestructor(pThis);` |
|        - | 3650 | ``	/* php: a destructor may RESURRECT the object. Anything the body hands `$this` to`` |
|        - | 3651 | `	 * that outlives the release -- a registry, a property of something still alive, a` |
|        - | 3652 | ``	 * closure's `use ($this)` -- is a new reference taken while the refcount was`` |
|        - | 3653 | `	 * already at zero, and zend_objects_store_del re-reads it after dtor_obj and frees` |
|        - | 3654 | `	 * ONLY when it is still zero. PHL freed unconditionally, so every holder the` |
|        - | 3655 | `	 * destructor had just handed the object to was left pointing at freed memory.` |
|        - | 3656 | `	 *` |
|        - | 3657 | `	 * Pest is exactly that shape: a TestCase's teardown registers closures that capture` |
|        - | 3658 | ``	 * `$this`, and the next `new` of a test case read the dead object through one of`` |
|        - | 3659 | `	 * them -- a segfault a fifth of the way into any suite it runs, with the refcount` |
|        - | 3660 | `	 * still reading 7.` |
|        - | 3661 | `	 *` |
|        - | 3662 | `	 * Clearing DESTROYED is what lets the object die properly LATER: when its new` |
|        - | 3663 | `	 * holders drop it to zero this runs again, and CLASS_INSTANCE_DTOR_CALLED (set by` |
|        - | 3664 | `	 * PH7_ClassInstanceCallDestructor, php's IS_OBJ_DESTRUCTOR_CALLED) keeps the` |
|        - | 3665 | `	 * destructor from running a second time -- php's rule for the same case. */` |
|  1577538 | 3666 | `	if( pThis->iRef > 0 ){` |
|      ! 0 | 3667 | `		pThis->iFlags &= ~CLASS_INSTANCE_DESTROYED;` |
|      ! 0 | 3668 | `		return;` |
|        - | 3669 | `	}` |
|        - | 3670 | `	/* A native class's own teardown, while its slots are still readable. Not a` |
|        - | 3671 | `	 * __destruct: the classes that need this (WeakReference) declare none in php,` |
|        - | 3672 | `	 * and Reflection must not grow one.` |
|        - | 3673 | `	 *` |
|        - | 3674 | `	 * Resolved through the ANCESTORS, like php's own free_obj handler: a` |
|        - | 3675 | `	 * subclass inherits it unless it declares one of its own. Reading it off` |
|        - | 3676 | `	 * this class alone left every subclass of a handle-owning native class` |
|        - | 3677 | ``	 * without teardown -- `Pdo\Sqlite` (which is how PDO::connect() answers) and`` |
|        - | 3678 | ``	 * any userland `extends PDO` alike, both of which then died holding engine`` |
|        - | 3679 | `	 * state that believed it was still reachable. */` |
|        - | 3680 | `	{` |
|  1577538 | 3681 | `		ph7_class *pOwner = pClass;` |
|  3220403 | 3682 | `		while( pOwner && pOwner->xRelease == 0 ){` |
|  1642870 | 3683 | `			pOwner = pOwner->pBase;` |
|        5 | 3684 | `		}` |
|  1577538 | 3685 | `		if( pOwner && pOwner->xRelease ){` |
|    68740 | 3686 | `			pOwner->xRelease(pVm,pThis);` |
|    34245 | 3687 | `		}` |
|        - | 3688 | `	}` |
|        - | 3689 | `	/* Weak-reference registry: kill the cell for this instance so every` |
|        - | 3690 | `	 * WeakReference/WeakMap handle observes the death (the cell outlives the` |
|        - | 3691 | `	 * instance until its own handles drop; removing the hash entry here keeps` |
|        - | 3692 | `	 * a pool-reused address from resurrecting a dead cell). */` |
|  1577538 | 3693 | `	if( SyHashTotalEntry(&pVm->hWeakCell) > 0 ){` |
|    20400 | 3694 | `		void *pCellData = 0;` |
|    20398 | 3695 | `		if( SyHashDeleteEntry(&pVm->hWeakCell,(const void *)&pThis,sizeof(void *),&pCellData) == SXRET_OK` |
|    10216 | 3696 | `		 && pCellData ){` |
|       32 | 3697 | `			((VmWeakCell *)pCellData)->pObj = 0;` |
|       15 | 3698 | `		}` |
|    10199 | 3699 | `	}` |
|        - | 3700 | `	/* Release non-static attributes (the wholesale SyHashRelease below frees the entry nodes,` |
|        - | 3701 | `	 * so the helper must not delete them mid-walk). */` |
|  1577538 | 3702 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
| 11689764 | 3703 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
| 10112231 | 3704 | `		PH7_VmReleaseInstanceAttr(pVm,(VmClassAttr *)pEntry->pUserData);` |
|        5 | 3705 | `	}` |
|        - | 3706 | `	/* Release the whole structure */` |
|  1577538 | 3707 | `	SyHashRelease(&pThis->hAttr);` |
|        - | 3708 | `	/* ...and stop the collector's root buffer naming memory that is going back` |
|        - | 3709 | `	 * to the pool. */` |
|  1577538 | 3710 | `	PH7_GcForget(pVm,(void *)pThis,0);` |
|  1577538 | 3711 | `	SyMemBackendPoolFree(&pVm->sAllocator,pThis);` |
|   788608 | 3712 | `}` |
|        - | 3713 | `/*` |
|        - | 3714 | ` * Decrement the reference count of a class instance [i.e Object in the PHP jargon].` |
|        - | 3715 | ` * If the reference count reaches zero,release the whole instance.` |
|        - | 3716 | ` */` |
|  8134516 | 3717 | `PH7_PRIVATE void PH7_ClassInstanceUnref(ph7_class_instance *pThis)` |
|        5 | 3718 | `{` |
|  8134521 | 3719 | `	pThis->iRef--;` |
|  8134521 | 3720 | `	if( pThis->iRef < 1 ){` |
|        - | 3721 | `		/* No more reference to this instance */` |
|  1577538 | 3722 | `		PH7_ClassInstanceRelease(&(*pThis));` |
|   788608 | 3723 | `	}else{` |
|        - | 3724 | `		/* Still held -- but by whom? A drop that does NOT reach zero is the only` |
|        - | 3725 | `		 * event that can strand a cycle, so it is what the collector buffers. */` |
|  6556988 | 3726 | `		PH7_GcPossibleRoot(pThis->pVm,(void *)pThis,0);` |
|        - | 3727 | `	}` |
|  8134521 | 3728 | `}` |
|        - | 3729 | `static sxi32 ClassInstanceCmpAttr(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest);` |
|        - | 3730 | `/*` |
|        - | 3731 | ` * Compare two class instances [i.e: Objects in the PHP jargon]` |
|        - | 3732 | ` * Note on objects comparison:` |
|        - | 3733 | ` *  According to the PHP langauge reference manual` |
|        - | 3734 | ` *  When using the comparison operator (==), object variables are compared in a simple manner` |
|        - | 3735 | ` *  namely: Two object instances are equal if they have the same attributes and values, and are` |
|        - | 3736 | ` *  instances of the same class.` |
|        - | 3737 | ` *  On the other hand, when using the identity operator (===), object variables are identical` |
|        - | 3738 | ` *  if and only if they refer to the same instance of the same class.` |
|        - | 3739 | ` *  An example will clarify these rules.` |
|        - | 3740 | ` *  Example #1 Example of object comparison` |
|        - | 3741 | ` *  <?php` |
|        - | 3742 | ` *    function bool2str($bool)` |
|        - | 3743 | ` * {` |
|        - | 3744 | ` *   if ($bool === false) {` |
|        - | 3745 | ` *       return 'FALSE';` |
|        - | 3746 | ` *   } else {` |
|        - | 3747 | ` *       return 'TRUE';` |
|        - | 3748 | ` *   }` |
|        - | 3749 | ` * }` |
|        - | 3750 | ` * function compareObjects(&$o1, &$o2)` |
|        - | 3751 | ` * {` |
|        - | 3752 | ` *   echo 'o1 == o2 : ' . bool2str($o1 == $o2) . "\n";` |
|        - | 3753 | ` *   echo 'o1 != o2 : ' . bool2str($o1 != $o2) . "\n";` |
|        - | 3754 | ` *   echo 'o1 === o2 : ' . bool2str($o1 === $o2) . "\n";` |
|        - | 3755 | ` *   echo 'o1 !== o2 : ' . bool2str($o1 !== $o2) . "\n";` |
|        - | 3756 | ` * }` |
|        - | 3757 | ` * class Flag` |
|        - | 3758 | ` * {` |
|        - | 3759 | ` *   public $flag;` |
|        - | 3760 | ` *` |
|        - | 3761 | ` *   function Flag($flag = true) {` |
|        - | 3762 | ` *       $this->flag = $flag;` |
|        - | 3763 | ` *   }` |
|        - | 3764 | ` * }` |
|        - | 3765 | ` *` |
|        - | 3766 | ` * class OtherFlag` |
|        - | 3767 | ` * {` |
|        - | 3768 | ` *   public $flag;` |
|        - | 3769 | ` *` |
|        - | 3770 | ` *   function OtherFlag($flag = true) {` |
|        - | 3771 | ` *       $this->flag = $flag;` |
|        - | 3772 | ` *   }` |
|        - | 3773 | ` * }` |
|        - | 3774 | ` *` |
|        - | 3775 | ` * $o = new Flag();` |
|        - | 3776 | ` * $p = new Flag();` |
|        - | 3777 | ` * $q = $o;` |
|        - | 3778 | ` * $r = new OtherFlag();` |
|        - | 3779 | ` *` |
|        - | 3780 | ` * echo "Two instances of the same class\n";` |
|        - | 3781 | ` * compareObjects($o, $p);` |
|        - | 3782 | ` * echo "\nTwo references to the same instance\n";` |
|        - | 3783 | ` * compareObjects($o, $q);` |
|        - | 3784 | ` * echo "\nInstances of two different classes\n";` |
|        - | 3785 | ` * compareObjects($o, $r);` |
|        - | 3786 | ` * ?>` |
|        - | 3787 | ` * The above example will output:` |
|        - | 3788 | ` * Two instances of the same class` |
|        - | 3789 | ` * o1 == o2 : TRUE` |
|        - | 3790 | ` * o1 != o2 : FALSE` |
|        - | 3791 | ` * o1 === o2 : FALSE` |
|        - | 3792 | ` * o1 !== o2 : TRUE` |
|        - | 3793 | ` * Two references to the same instance` |
|        - | 3794 | ` * o1 == o2 : TRUE` |
|        - | 3795 | ` * o1 != o2 : FALSE` |
|        - | 3796 | ` * o1 === o2 : TRUE` |
|        - | 3797 | ` * o1 !== o2 : FALSE` |
|        - | 3798 | ` * Instances of two different classes` |
|        - | 3799 | ` * o1 == o2 : FALSE` |
|        - | 3800 | ` * o1 != o2 : TRUE` |
|        - | 3801 | ` * o1 === o2 : FALSE` |
|        - | 3802 | ` * o1 !== o2 : TRUE` |
|        - | 3803 | ` *` |
|        - | 3804 | ` * This function return 0 if the objects are equals according to the comprison rules defined above.` |
|        - | 3805 | ` * Any other return values indicates difference.` |
|        - | 3806 | ` */` |
|     1542 | 3807 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest)` |
|        5 | 3808 | `{` |
|        - | 3809 | `	sxi32 rc;` |
|        - | 3810 | `	/*` |
|        - | 3811 | `	 * php's identity shortcut, and it comes FIRST -- before the same-class screen` |
|        - | 3812 | ``	 * and before any handler: `$i == $i` is 0 for a DateInterval, the one pair of`` |
|        - | 3813 | `	 * intervals php will compare at all.` |
|        - | 3814 | `	 */` |
|     1547 | 3815 | `	if( pLeft == pRight ){` |
|        - | 3816 | `		/* Same instance,don't bother processing,object are equals */` |
|      559 | 3817 | `		return 0;` |
|        - | 3818 | `	}` |
|      993 | 3819 | `	if( bStrict ){` |
|        - | 3820 | `		/*` |
|        - | 3821 | `		 * According to the PHP language reference manual:` |
|        - | 3822 | `		 *  when using the identity operator (===), object variables` |
|        - | 3823 | `		 *  are identical if and only if they refer to the same instance` |
|        - | 3824 | `		 *  of the same class.` |
|        - | 3825 | `		 * Two DISTINCT instances, so this is never identical -- and no compare` |
|        - | 3826 | ``		 * handler is asked, because php's `===` is pointer identity and never`` |
|        - | 3827 | `		 * reaches one.` |
|        - | 3828 | `		 */` |
|      171 | 3829 | `		return 1;` |
|        - | 3830 | `	}` |
|        - | 3831 | `	/*` |
|        - | 3832 | `	 * php's compare handler (ph7_class::xCmp), asked of the LEFT operand and` |
|        - | 3833 | `	 * ABOVE the same-class screen: a DateTime and a DateTimeImmutable of the same` |
|        - | 3834 | `	 * instant are equal there, which no property walk between two different` |
|        - | 3835 | `	 * classes could ever answer. A class with no handler falls through to the` |
|        - | 3836 | `	 * walk, which is php's zend_std_compare_objects.` |
|        - | 3837 | `	 */` |
|      827 | 3838 | `	if( PH7_ClassNativeCmp(pLeft,pRight,&rc) ){` |
|      181 | 3839 | `		return rc;` |
|        - | 3840 | `	}` |
|        - | 3841 | `	/* Comparison is performed only if the objects are instance of the same class */` |
|      651 | 3842 | `	if( pLeft->pClass != pRight->pClass ){` |
|       14 | 3843 | `		return 1;` |
|        - | 3844 | `	}` |
|        - | 3845 | `	/*` |
|        - | 3846 | `	 * Attribute comparison.` |
|        - | 3847 | `	 * According to the PHP reference manual:` |
|        - | 3848 | `	 *  When using the comparison operator (==), object variables are compared` |
|        - | 3849 | `	 *  in a simple manner, namely: Two object instances are equal if they have` |
|        - | 3850 | `	 *  the same attributes and values, and are instances of the same class.` |
|        - | 3851 | `	 */` |
|        - | 3852 | `	/* Closures compare by IDENTITY under == as well (not by attributes): two distinct` |
|        - | 3853 | `	 * Closure instances are never equal, even when they wrap the same underlying function` |
|        - | 3854 | `	 * (PHP semantics). pLeft != pRight here, so a Closure pair is unequal. Without this,` |
|        - | 3855 | `` 	 * two capture-less lambdas of the same `function(){}` share the template's `$__fn` `` |
|        - | 3856 | `	 * name and would compare equal. */` |
|      639 | 3857 | `	if( pLeft->pVm->pClosureClass && pLeft->pClass == pLeft->pVm->pClosureClass ){` |
|        5 | 3858 | `		return 1;` |
|        - | 3859 | `	}` |
|        - | 3860 | `	/* Same class but a different number of attributes ⇒ different property sets` |
|        - | 3861 | `	 * (dynamic properties can give two same-class instances different counts). */` |
|      635 | 3862 | `	if( pLeft->hAttr.nEntry != pRight->hAttr.nEntry ){` |
|        3 | 3863 | `		return 1;` |
|        - | 3864 | `	}` |
|      633 | 3865 | `	if( (pLeft->iFlags & VM_INSTANCE_COMPARING) \|\| iNest > PH7_CMP_MAX_DEPTH ){` |
|        - | 3866 | `		/* This object is its own descendant -- php's Z_IS_RECURSIVE_P(o1) test, asked` |
|        - | 3867 | `		 * exactly here, below every screen that answers without walking and above the` |
|        - | 3868 | `		 * property walk that recurses -- or the finite-nesting backstop tripped. Either` |
|        - | 3869 | `		 * way php's catchable Error, recorded for whichever door onto the comparator` |
|        - | 3870 | `		 * can throw. The old depth counter was neither: it refused a merely-deep graph` |
|        - | 3871 | `		 * php compares fine, and reported a cycle only after walking 31 levels of it. */` |
|        5 | 3872 | `		PH7_CmpRefusalNesting(pLeft->pVm);` |
|        5 | 3873 | `		return 1;` |
|        - | 3874 | `	}` |
|      629 | 3875 | `	pLeft->iFlags \|= VM_INSTANCE_COMPARING;` |
|      629 | 3876 | `	rc = ClassInstanceCmpAttr(&(*pLeft),&(*pRight),bStrict,iNest);` |
|      629 | 3877 | `	pLeft->iFlags &= ~VM_INSTANCE_COMPARING;` |
|      629 | 3878 | `	return rc;` |
|      776 | 3879 | `}` |
|        - | 3880 | `/*` |
|        - | 3881 | ` * php's zend_std_compare_objects property walk: every non-static, non-constant, non-virtual` |
|        - | 3882 | ` * attribute of the left instance against the RIGHT attribute of the same name. Split out of` |
|        - | 3883 | ` * PH7_ClassInstanceCmp so the recursion mark that function sets is cleared on every exit.` |
|        - | 3884 | ` */` |
|      624 | 3885 | `static sxi32 ClassInstanceCmpAttr(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest)` |
|        5 | 3886 | `{` |
|        - | 3887 | `	SyHashEntry *pEntry,*pEntry2;` |
|        - | 3888 | `	ph7_value sV1,sV2;` |
|        - | 3889 | `	sxi32 rc;` |
|      629 | 3890 | `	PH7_MemObjInit(pLeft->pVm,&sV1);` |
|      629 | 3891 | `	PH7_MemObjInit(pLeft->pVm,&sV2);` |
|      629 | 3892 | `	sV1.nIdx = sV2.nIdx = SXU32_HIGH;` |
|        - | 3893 | `	/* Compare each left attribute against the RIGHT attribute of the SAME NAME` |
|        - | 3894 | `	 * (not in lockstep): dynamic properties may be stored in a different order` |
|        - | 3895 | `	 * on the two instances. Counts already match, so if every left attribute has` |
|        - | 3896 | `	 * an equal-valued same-named right attribute the property sets are equal. */` |
|      629 | 3897 | `	SyHashResetLoopCursor(&pLeft->hAttr);` |
|     1251 | 3898 | `	while((pEntry = SyHashGetNextEntry(&pLeft->hAttr)) != 0 ){` |
|      722 | 3899 | `		VmClassAttr *p1 = (VmClassAttr *)pEntry->pUserData;` |
|        - | 3900 | `		VmClassAttr *p2;` |
|        - | 3901 | `		ph7_value *pL,*pR;` |
|        - | 3902 | `		/* Compare only non-static attribute. A native class's VIRTUAL property is` |
|        - | 3903 | `		 * skipped too: php fabricates DatePeriod's seven on demand and its real` |
|        - | 3904 | `		 * property table is empty, so any two DatePeriods are equal there whatever` |
|        - | 3905 | `		 * they contain -- while a subclass's own property, which IS in the table,` |
|        - | 3906 | `		 * still decides. */` |
|      722 | 3907 | `		if( p1->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC` |
|        - | 3908 | `		                        \|PH7_CLASS_ATTR_NATIVE_VIRTUAL) ){` |
|       71 | 3909 | `			continue;` |
|        - | 3910 | `		}` |
|      652 | 3911 | `		pEntry2 = SyHashGet(&pRight->hAttr,SyStringData(&p1->pAttr->sName),SyStringLength(&p1->pAttr->sName));` |
|      652 | 3912 | `		if( pEntry2 == 0 ){` |
|        - | 3913 | `			/* Left has a property the right lacks ⇒ not equal. */` |
|      ! 0 | 3914 | `			return 1;` |
|        - | 3915 | `		}` |
|      652 | 3916 | `		p2 = (VmClassAttr *)pEntry2->pUserData;` |
|      652 | 3917 | `		pL = ExtractClassAttrValue(pLeft->pVm,p1);` |
|      652 | 3918 | `		pR = ExtractClassAttrValue(pRight->pVm,p2);` |
|      652 | 3919 | `		if( pL && pR ){` |
|      652 | 3920 | `			PH7_MemObjLoad(pL,&sV1);` |
|      652 | 3921 | `			PH7_MemObjLoad(pR,&sV2);` |
|        - | 3922 | `			/* Compare the two values now */` |
|      652 | 3923 | `			rc = PH7_MemObjCmp(&sV1,&sV2,bStrict,iNest+1);` |
|      652 | 3924 | `			PH7_MemObjRelease(&sV1);` |
|      652 | 3925 | `			PH7_MemObjRelease(&sV2);` |
|      652 | 3926 | `			if( rc != 0 ){` |
|        - | 3927 | `				/* Not equals */` |
|       99 | 3928 | `				return rc;` |
|        - | 3929 | `			}` |
|      276 | 3930 | `		}` |
|        3 | 3931 | `	}` |
|        - | 3932 | `	/* Object are equals */` |
|      532 | 3933 | `	return 0;` |
|      317 | 3934 | `}` |
|        - | 3935 | `/*` |
|        - | 3936 | ` * Dump a class instance and the store the dump in the BLOB given` |
|        - | 3937 | ` * as the first argument.` |
|        - | 3938 | ` * Note that only non-static/non-constants attribute are dumped.` |
|        - | 3939 | ` * This function is typically invoked when the user issue a call` |
|        - | 3940 | ` * to [var_dump(),var_export(),print_r(),...].` |
|        - | 3941 | ` * This function SXRET_OK on success. Any other return value including` |
|        - | 3942 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|        - | 3943 | ` */` |
|        - | 3944 | `/*` |
|        - | 3945 | `` * Return the `name` property value of an enum case instance (the case name),`` |
|        - | 3946 | ` * or 0 when unavailable. Shared by the var_dump/var_export/json/serialize` |
|        - | 3947 | ` * renderers, which all print enum cases as Class::CaseName forms.` |
|        - | 3948 | ` */` |
|       44 | 3949 | `PH7_PRIVATE ph7_value * PH7_EnumCaseNameValue(ph7_class_instance *pThis)` |
|        2 | 3950 | `{` |
|        - | 3951 | `	SyHashEntry *pEntry;` |
|       46 | 3952 | `	if( (pThis->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|      ! 0 | 3953 | `		return 0;` |
|        - | 3954 | `	}` |
|       46 | 3955 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)"name",sizeof("name")-1);` |
|       46 | 3956 | `	if( pEntry == 0 ){` |
|      ! 0 | 3957 | `		return 0;` |
|        - | 3958 | `	}` |
|       46 | 3959 | `	return PH7_ClassInstanceExtractAttrValue(pThis,(VmClassAttr *)pEntry->pUserData);` |
|       24 | 3960 | `}` |
|        - | 3961 | `/*` |
|        - | 3962 | `` * Return the `value` property value (the backing value) of an enum case`` |
|        - | 3963 | ` * instance, or 0 when unavailable (pure enums have none).` |
|        - | 3964 | ` */` |
|       24 | 3965 | `PH7_PRIVATE ph7_value * PH7_EnumCaseBackingValueOf(ph7_class_instance *pThis)` |
|        2 | 3966 | `{` |
|        - | 3967 | `	SyHashEntry *pEntry;` |
|       26 | 3968 | `	if( (pThis->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|      ! 0 | 3969 | `		return 0;` |
|        - | 3970 | `	}` |
|       26 | 3971 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)"value",sizeof("value")-1);` |
|       26 | 3972 | `	if( pEntry == 0 ){` |
|        7 | 3973 | `		return 0;` |
|        - | 3974 | `	}` |
|       20 | 3975 | `	return PH7_ClassInstanceExtractAttrValue(pThis,(VmClassAttr *)pEntry->pUserData);` |
|       14 | 3976 | `}` |
|        - | 3977 | `/*` |
|        - | 3978 | ` * Emit a class-instance dump header plus its trailing newline. For var_dump` |
|        - | 3979 | ` * (ShowType) it completes the "object(" prefix the caller already emitted as` |
|        - | 3980 | ` *   ClassName)#<id> (<count>) {` |
|        - | 3981 | ` * for print_r it emits the legacy PHL  Object(ClassName) {  (count/id unused).` |
|        - | 3982 | `` * Enum cases print php's `ClassName Enum {` print_r header (var_dump never`` |
|        - | 3983 | `` * reaches here for enums — PH7_MemObjDump prints `enum(S::A)` directly).`` |
|        - | 3984 | ` */` |
|      540 | 3985 | `static void DumpClassInstanceHeader(SyBlob *pOut,ph7_class *pClass,sxu32 nObjId,int ShowType,sxu32 nCount)` |
|        5 | 3986 | `{` |
|      545 | 3987 | `	if( ShowType ){` |
|        - | 3988 | ``		/* var_dump: `object(C)#id (n) {` */`` |
|      387 | 3989 | `		SyBlobFormat(&(*pOut),"object(%z)#%u (%u) {",&pClass->sDisp,nObjId,nCount);` |
|      387 | 3990 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      387 | 3991 | `		return;` |
|        - | 3992 | `	}` |
|        - | 3993 | ``	/* print_r: `C Object` / `E Enum[:backing]` — the '(' line is emitted by`` |
|        - | 3994 | `	 * the body renderer at the container indent. */` |
|      163 | 3995 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|      ! 0 | 3996 | `		SyBlobFormat(&(*pOut),"%z Enum",&pClass->sDisp);` |
|      ! 0 | 3997 | `		if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|      ! 0 | 3998 | `			SyBlobAppend(&(*pOut),":int",sizeof(":int")-1);` |
|      ! 0 | 3999 | `		}else if( pClass->nEnumBacking == MEMOBJ_STRING ){` |
|      ! 0 | 4000 | `			SyBlobAppend(&(*pOut),":string",sizeof(":string")-1);` |
|      ! 0 | 4001 | `		}` |
|      ! 0 | 4002 | `	}else{` |
|      163 | 4003 | `		SyBlobFormat(&(*pOut),"%z Object",&pClass->sDisp);` |
|        - | 4004 | `	}` |
|      163 | 4005 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      275 | 4006 | `}` |
|        - | 4007 | `/*` |
|        - | 4008 | ` * The class that DECLARED pAttr: inheritance shares attr pointers down the` |
|        - | 4009 | ` * chain, so the declaring class is the most ANCESTRAL class whose hAttr still` |
|        - | 4010 | ` * maps the name to this exact pointer. php's var_dump/print_r use it for the` |
|        - | 4011 | `` * `["p":"Decl":private]` annotation.`` |
|        - | 4012 | ` */` |
|       54 | 4013 | `static ph7_class * OoAttrDeclaringClass(ph7_class *pClass,ph7_class_attr *pAttr)` |
|        2 | 4014 | `{` |
|        - | 4015 | `	/* Attrs record their declaring class at install time (inheritance/trait` |
|        - | 4016 | `	 * copies share the pointer, so the field survives the chain) -- and a TRAIT's` |
|        - | 4017 | `	 * members belong to the class that composed them, which is what php names. */` |
|       56 | 4018 | `	return PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|        2 | 4019 | `}` |
|        - | 4020 | `/*` |
|        - | 4021 | ` * php's zend_unmangle_property_name_ex: a property key carries its own` |
|        - | 4022 | ` * visibility when it is MANGLED — "\0*\0name" is protected and "\0Class\0name"` |
|        - | 4023 | ` * is private to Class, which is how the (array) cast, __debugInfo() and every` |
|        - | 4024 | ` * get_debug_info handler say what a plain array key cannot. A key that does not` |
|        - | 4025 | ` * begin with a NUL is a public name and comes back unchanged.` |
|        - | 4026 | ` *` |
|        - | 4027 | ` * Answers 0 for a key that begins with a NUL and is NOT a well-formed mangled` |
|        - | 4028 | ` * name — php's "Illegal member variable name" (nothing after the NUL, or an` |
|        - | 4029 | ` * empty class part) and "Corrupt member variable name" (no second NUL, or` |
|        - | 4030 | ` * nothing after it). php renders those raw, notice aside, and so does the caller.` |
|        - | 4031 | ` */` |
|      948 | 4032 | `PH7_PRIVATE int PH7_UnmangleAttrName(const char *zKey,sxu32 nKey,SyString *pClass,SyString *pName)` |
|        5 | 4033 | `{` |
|        - | 4034 | `	sxu32 nCls,nSrc;` |
|      953 | 4035 | `	SyStringInitFromBuf(pClass,0,0);` |
|      953 | 4036 | `	SyStringInitFromBuf(pName,zKey,nKey);` |
|      953 | 4037 | `	if( nKey < 1 \|\| zKey[0] != 0 ){` |
|      791 | 4038 | `		return 1;   /* a plain public name */` |
|        - | 4039 | `	}` |
|      163 | 4040 | `	if( nKey < 3 \|\| zKey[1] == 0 ){` |
|      ! 0 | 4041 | `		return 0;   /* php: "Illegal member variable name" */` |
|        - | 4042 | `	}` |
|      163 | 4043 | `	nCls = 0;` |
|     1857 | 4044 | `	while( nCls < nKey - 2 && zKey[1+nCls] != 0 ){` |
|     1695 | 4045 | `		nCls++;` |
|        1 | 4046 | `	}` |
|      163 | 4047 | `	if( nCls >= nKey - 2 ){` |
|      ! 0 | 4048 | `		return 0;   /* php: "Corrupt member variable name" */` |
|        - | 4049 | `	}` |
|        - | 4050 | `	/* An ANONYMOUS class mangles its source location in as a SECOND NUL-separated` |
|        - | 4051 | `	 * part, so the property name is what follows the LAST NUL rather than the` |
|        - | 4052 | `	 * second one (php's anonclass_src_len step). The class STRING php shows is` |
|        - | 4053 | `	 * still only the first part — it prints that one as a C string. */` |
|      163 | 4054 | `	nSrc = 0;` |
|     1119 | 4055 | `	while( nCls + 2 + nSrc < nKey && zKey[nCls+2+nSrc] != 0 ){` |
|      957 | 4056 | `		nSrc++;` |
|        1 | 4057 | `	}` |
|      163 | 4058 | `	SyStringInitFromBuf(pClass,&zKey[1],nCls);` |
|      163 | 4059 | `	if( nCls + nSrc + 2 != nKey ){` |
|      ! 0 | 4060 | `		nCls += nSrc + 1;` |
|      ! 0 | 4061 | `	}` |
|      163 | 4062 | `	SyStringInitFromBuf(pName,&zKey[nCls+2],nKey - nCls - 2);` |
|      163 | 4063 | `	return 1;` |
|      479 | 4064 | `}` |
|        - | 4065 | `/*` |
|        - | 4066 | `` * Emit a property's dump key: var_dump `["x"]=>` / `["p":"C":private]=>` /`` |
|        - | 4067 | `` * `["q":protected]=>`; print_r `[x] => ` / `[p:C:private] => ` /`` |
|        - | 4068 | `` * `[q:protected] => ` (php's exact annotations).`` |
|        - | 4069 | ` */` |
|      488 | 4070 | `static void OoDumpPropKey(SyBlob *pOut,ph7_class_instance *pThis,ph7_class_attr *pAttr,int ShowType)` |
|        5 | 4071 | `{` |
|      493 | 4072 | `	const char *zQ = ShowType ? "\"" : "";` |
|      493 | 4073 | `	if( SyStringLength(&pAttr->sName) > 0 && SyStringData(&pAttr->sName)[0] == 0 ){` |
|        - | 4074 | `		/* A MANGLED name stored raw — the __PHP_Incomplete_Class carrier's` |
|        - | 4075 | `		 * private/protected payload keys. php's dump unmangles them exactly as` |
|        - | 4076 | ``		 * it does an (array) cast's: `["bp":"PB":private]` / `["pp":protected]`. */`` |
|        - | 4077 | `		SyString sUnmCls, sUnmName;` |
|        9 | 4078 | `		if( PH7_UnmangleAttrName(SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),` |
|        - | 4079 | `			&sUnmCls,&sUnmName) ){` |
|        9 | 4080 | `			SyBlobFormat(&(*pOut),"[%s%z%s",zQ,&sUnmName,zQ);` |
|        9 | 4081 | `			if( sUnmCls.nByte == 1 && sUnmCls.zString[0] == '*' ){` |
|        5 | 4082 | `				SyBlobAppend(&(*pOut),":protected",sizeof(":protected")-1);` |
|        3 | 4083 | `			}else{` |
|        5 | 4084 | `				SyBlobFormat(&(*pOut),":%s%z%s:private",zQ,&sUnmCls,zQ);` |
|        - | 4085 | `			}` |
|        9 | 4086 | `			SyBlobAppend(&(*pOut),ShowType ? "]=>" : "] => ",ShowType ? 3 : 5);` |
|        9 | 4087 | `			return;` |
|        - | 4088 | `		}` |
|      ! 0 | 4089 | `	}` |
|      485 | 4090 | `	SyBlobFormat(&(*pOut),"[%s%z%s",zQ,&pAttr->sName,zQ);` |
|      485 | 4091 | `	if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       56 | 4092 | `		ph7_class *pDecl = OoAttrDeclaringClass(pThis->pClass,pAttr);` |
|       56 | 4093 | `		SyBlobFormat(&(*pOut),":%s%z%s:private",zQ,&pDecl->sDisp,zQ);` |
|      458 | 4094 | `	}else if( pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
|       65 | 4095 | `		SyBlobAppend(&(*pOut),":protected",sizeof(":protected")-1);` |
|       31 | 4096 | `	}` |
|      485 | 4097 | `	SyBlobAppend(&(*pOut),ShowType ? "]=>" : "] => ",ShowType ? 3 : 5);` |
|      249 | 4098 | `}` |
|        - | 4099 | `/*` |
|        - | 4100 | `` * Is this property's value a REFERENCE? -- php's `Z_ISREF_P`, asked of a property.`` |
|        - | 4101 | ` *` |
|        - | 4102 | `` * Two things turn on it. var_dump prints `&` for a property that IS one -- either end`` |
|        - | 4103 | `` * of the bind, `$o->p =& $x` and `$r =& $o->p` alike -- exactly as the array renderer`` |
|        - | 4104 | ` * marks an element something else holds (PH7_HashmapNodeIsRef). A property differs` |
|        - | 4105 | ` * only in the THRESHOLD: the property itself is not always one of the holders the` |
|        - | 4106 | ` * reference table names -- a DECLARED property holds nothing, a dynamic or re-created` |
|        - | 4107 | ` * one holds a permanent pin, and a bound one holds a counted pin -- so the threshold` |
|        - | 4108 | ` * is that one hold rather than the element renderer's flat two. print_r marks nothing,` |
|        - | 4109 | ` * in either container.` |
|        - | 4110 | ` *` |
|        - | 4111 | ` * The second is what an ARRAY built out of the property table carries:` |
|        - | 4112 | `` * `get_object_vars()`, the `(array)` cast, `get_mangled_object_vars()` and the SPL`` |
|        - | 4113 | ` * storage built from an object hand out the property's own SLOT for a property that` |
|        - | 4114 | ` * is a reference, so a write through the element reaches the object. Everything else` |
|        - | 4115 | ` * stays the copy it has always been.` |
|        - | 4116 | ` */` |
|     2398 | 4117 | `PH7_PRIVATE int PH7_ClassAttrIsRef(ph7_class_instance *pThis,VmClassAttr *pVmAttr)` |
|        5 | 4118 | `{` |
|        - | 4119 | `	sxu32 nSelf;` |
|     2403 | 4120 | `	if( pVmAttr == 0 \|\| pVmAttr->nIdx == SXU32_HIGH ){` |
|      ! 0 | 4121 | `		return 0;` |
|        - | 4122 | `	}` |
|     2403 | 4123 | `	nSelf = PH7_VmSlotSelfPinned(pThis->pVm,pVmAttr->nIdx) ? 1 : 0;` |
|     2403 | 4124 | `	return PH7_VmSlotHolderCount(pThis->pVm,pVmAttr->nIdx) > nSelf;` |
|     1204 | 4125 | `}` |
|      540 | 4126 | `PH7_PRIVATE sxi32 PH7_ClassInstanceDump(SyBlob *pOut,ph7_class_instance *pThis,int ShowType,int nTab,int nDepth)` |
|        5 | 4127 | `{` |
|        - | 4128 | `	SyHashEntry *pEntry;` |
|        - | 4129 | `	ph7_value *pValue;` |
|        - | 4130 | `	sxi32 rc;` |
|        - | 4131 | `	int i;` |
|      545 | 4132 | `	if( nDepth > PH7_DUMP_MAX_DEPTH ){` |
|        - | 4133 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|        - | 4134 | `		/* Nesting limit reached..halt immediately*/` |
|      ! 0 | 4135 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|      ! 0 | 4136 | `		return SXERR_LIMIT;` |
|        - | 4137 | `	}` |
|      545 | 4138 | `	if( pThis->iFlags & VM_INSTANCE_DUMPING ){` |
|        - | 4139 | `		/* php's *RECURSION*: this instance is one the walk is already inside.` |
|        - | 4140 | `		 * print_r prints the header and then the marker in place of the body,` |
|        - | 4141 | `		 * and the entry line the caller is writing supplies the newline. Only` |
|        - | 4142 | `		 * print_r arrives here marked -- var_dump's marker replaces the header` |
|        - | 4143 | `		 * too, so PH7_MemObjDump (the only caller) never descends. */` |
|        7 | 4144 | `		if( !ShowType ){` |
|        7 | 4145 | `			DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,FALSE,0);` |
|        7 | 4146 | `			SyBlobAppend(&(*pOut)," *RECURSION*",sizeof(" *RECURSION*")-1);` |
|        3 | 4147 | `		}` |
|        7 | 4148 | `		return SXRET_OK;` |
|        - | 4149 | `	}` |
|      539 | 4150 | `	pThis->iFlags \|= VM_INSTANCE_DUMPING;` |
|      539 | 4151 | `	rc = SXRET_OK;` |
|        - | 4152 | `	{` |
|        - | 4153 | `		/* A native class's PRESENTATION (php's get_debug_info): the shape it shows` |
|        - | 4154 | `		 * is not its storage — a DateTime shows date/timezone_type/timezone, a` |
|        - | 4155 | `		 * WeakReference shows ["object"] — and the slots underneath are hidden.` |
|        - | 4156 | `		 * Rendered exactly like a __debugInfo() array, which is what php does with` |
|        - | 4157 | `		 * it, and consulted first because php's handler wins over a userland` |
|        - | 4158 | `		 * method a native class cannot declare anyway. */` |
|        - | 4159 | `		ph7_value sPresent;` |
|      539 | 4160 | `		PH7_MemObjInit(pThis->pVm,&sPresent);` |
|      539 | 4161 | `		if( ph7_value_is_array(&sPresent) == 0 ){` |
|      539 | 4162 | `			ph7_hashmap *pPresent = PH7_NewHashmap(pThis->pVm,0,0);` |
|      539 | 4163 | `			if( pPresent ){` |
|      539 | 4164 | `				sPresent.x.pOther = pPresent;` |
|      539 | 4165 | `				MemObjSetType(&sPresent,MEMOBJ_HASHMAP);` |
|      267 | 4166 | `			}` |
|      267 | 4167 | `		}` |
|      534 | 4168 | `		if( (sPresent.iFlags & MEMOBJ_HASHMAP)` |
|      539 | 4169 | `		 && PH7_ClassInstancePresent(pThis,&sPresent,1) ){` |
|      191 | 4170 | `			ph7_hashmap *pMap = (ph7_hashmap *)sPresent.x.pOther;` |
|      191 | 4171 | `			DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,pMap->nEntry);` |
|      191 | 4172 | `			if( !ShowType ){` |
|       85 | 4173 | `				for( i = 0 ; i < nTab ; i++ ){` |
|      ! 0 | 4174 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      ! 0 | 4175 | `				}` |
|       85 | 4176 | `				SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       41 | 4177 | `			}` |
|      191 | 4178 | `			rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth,1);` |
|      191 | 4179 | `			for( i = 0 ; i < nTab ; i++ ){` |
|      ! 0 | 4180 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      ! 0 | 4181 | `			}` |
|      191 | 4182 | `			if( ShowType ){` |
|      109 | 4183 | `				SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|       57 | 4184 | `			}else{` |
|       85 | 4185 | `				SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        - | 4186 | `			}` |
|      191 | 4187 | `			PH7_MemObjRelease(&sPresent);` |
|      191 | 4188 | `			pThis->iFlags &= ~VM_INSTANCE_DUMPING;` |
|      191 | 4189 | `			return rc;` |
|        - | 4190 | `		}` |
|      353 | 4191 | `		PH7_MemObjRelease(&sPresent);` |
|        - | 4192 | `	}` |
|        - | 4193 | `	{` |
|        - | 4194 | `		/* Both var_dump and print_r consult __debugInfo() (PHP behavior);` |
|        - | 4195 | `		 * var_export uses a separate renderer and never reaches here. When the` |
|        - | 4196 | `		 * method is present and returns an array, render that array's entries as` |
|        - | 4197 | `		 * the object body, with the header showing the debug array's count. The` |
|        - | 4198 | `		 * nDepth guard above protects against a __debugInfo returning the object` |
|        - | 4199 | `		 * itself. */` |
|      353 | 4200 | `		ph7_class_method *pDbg = PH7_ClassExtractMethod(pThis->pClass,"__debugInfo",sizeof("__debugInfo")-1);` |
|      353 | 4201 | `		if( pDbg ){` |
|        - | 4202 | `			ph7_value sResult;` |
|       19 | 4203 | `			PH7_MemObjInit(pThis->pVm,&sResult);` |
|       19 | 4204 | `			PH7_VmCallMagicMethod(pThis->pVm,pThis,pDbg,&sResult,0,0);` |
|       19 | 4205 | `			if( sResult.iFlags & MEMOBJ_HASHMAP ){` |
|       19 | 4206 | `				ph7_hashmap *pMap = (ph7_hashmap *)sResult.x.pOther;` |
|        - | 4207 | `				/* Header count is the debug array's entry count. */` |
|       19 | 4208 | `				DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,pMap->nEntry);` |
|       19 | 4209 | `				if( !ShowType ){` |
|        8 | 4210 | `					for( i = 0 ; i < nTab ; i++ ){` |
|      ! 0 | 4211 | `						SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      ! 0 | 4212 | `					}` |
|        8 | 4213 | `					SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|        3 | 4214 | `				}` |
|       19 | 4215 | `				rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth,1);` |
|       19 | 4216 | `				for( i = 0 ; i < nTab ; i++ ){` |
|      ! 0 | 4217 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      ! 0 | 4218 | `				}` |
|       19 | 4219 | `				if( ShowType ){` |
|       13 | 4220 | `					SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|        8 | 4221 | `				}else{` |
|        8 | 4222 | `					SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        - | 4223 | `				}` |
|       19 | 4224 | `				PH7_MemObjRelease(&sResult);` |
|       19 | 4225 | `				pThis->iFlags &= ~VM_INSTANCE_DUMPING;` |
|       19 | 4226 | `				return rc;` |
|        - | 4227 | `			}` |
|        - | 4228 | `			/* Non-array return: behave as if __debugInfo were absent. */` |
|      ! 0 | 4229 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 | 4230 | `		}` |
|        - | 4231 | `	}` |
|        - | 4232 | `	{` |
|        - | 4233 | `		/* var_dump's header needs the property count up front, so pre-count the` |
|        - | 4234 | `		 * non-static/non-constant attributes (matching the dump loop below).` |
|        - | 4235 | `		 * An UNINITIALIZED typed property is still LISTED by var_dump — with php's` |
|        - | 4236 | ``		 * `uninitialized(T)` marker in place of a value — but it does not COUNT,`` |
|        - | 4237 | `` 		 * which is how php's `object(C)#1 (0) { ["a"]=> uninitialized(int) }` `` |
|        - | 4238 | `		 * reads. */` |
|      337 | 4239 | `		sxu32 nProp = 0;` |
|      337 | 4240 | `		if( ShowType ){` |
|      273 | 4241 | `			SyHashResetLoopCursor(&pThis->hAttr);` |
|      691 | 4242 | `			while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|      423 | 4243 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      418 | 4244 | `				if( !PH7_ATTR_UNPRESENTED(pVmAttr)` |
|      394 | 4245 | `				 && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL) == 0` |
|      399 | 4246 | `				 && !PH7_ClassAttrUninitialized(pVmAttr) ){` |
|      361 | 4247 | `					nProp++;` |
|      178 | 4248 | `				}` |
|        5 | 4249 | `			}` |
|      134 | 4250 | `		}` |
|      337 | 4251 | `		DumpClassInstanceHeader(&(*pOut),pThis->pClass,pThis->nObjId,ShowType,nProp);` |
|        - | 4252 | `	}` |
|      337 | 4253 | `	if( !ShowType ){` |
|        - | 4254 | `		/* print_r body opener: '(' at the container indent */` |
|      197 | 4255 | `		for( i = 0 ; i < nTab ; i++ ){` |
|      131 | 4256 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       67 | 4257 | `		}` |
|       69 | 4258 | `		SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       32 | 4259 | `	}` |
|        - | 4260 | `	/* Dump object attributes (php 8.4: VIRTUAL hooked properties have no` |
|        - | 4261 | `	 * backing store — excluded from var_dump/print_r) */` |
|      337 | 4262 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      895 | 4263 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0){` |
|      563 | 4264 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      558 | 4265 | `		if( !PH7_ATTR_UNPRESENTED(pVmAttr)` |
|      517 | 4266 | `		 && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL) == 0 ){` |
|      517 | 4267 | `			if( PH7_ClassAttrUninitialized(pVmAttr) ){` |
|        - | 4268 | `				/* var_dump names the property and prints php's marker in place of` |
|        - | 4269 | `				 * the value it has not got; print_r has no such marker and leaves` |
|        - | 4270 | `				 * the property out entirely. */` |
|       65 | 4271 | `				if( ShowType ){` |
|        - | 4272 | `					char zType[192];` |
|       60 | 4273 | `					const char *zText = VmHintTextResolved(pThis->pVm,&pVmAttr->pAttr->sTypeName,` |
|       38 | 4274 | `						VmHintScopeDeclared(pVmAttr->pAttr->pDeclClass),` |
|       19 | 4275 | `						zType,sizeof(zType));` |
|      117 | 4276 | `					for( i = 0 ; i < nTab + 2 ; i++ ){` |
|       79 | 4277 | `						SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       41 | 4278 | `					}` |
|       41 | 4279 | `					OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,TRUE);` |
|       41 | 4280 | `					SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      117 | 4281 | `					for( i = 0 ; i < nTab + 2 ; i++ ){` |
|       79 | 4282 | `						SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       41 | 4283 | `					}` |
|       41 | 4284 | `					SyBlobFormat(&(*pOut),"uninitialized(%s)\n",zText);` |
|       19 | 4285 | `				}` |
|       65 | 4286 | `				continue;` |
|        - | 4287 | `			}` |
|        - | 4288 | `			/* Dump non-static/constant attribute only */` |
|      455 | 4289 | `			pValue = ExtractClassAttrValue(pThis->pVm,pVmAttr);` |
|      455 | 4290 | `			if( pValue == 0 ){` |
|      ! 0 | 4291 | `				continue;` |
|        - | 4292 | `			}` |
|      455 | 4293 | `			if( ShowType ){` |
|        - | 4294 | ``				/* var_dump prop: `["x"(:…)]=>` at nTab+2, the value on the next`` |
|        - | 4295 | `				 * line at the same indent (php). */` |
|     6165 | 4296 | `				for( i = 0 ; i < nTab + 2 ; i++ ){` |
|     5809 | 4297 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     2907 | 4298 | `				}` |
|      361 | 4299 | `				OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,TRUE);` |
|      361 | 4300 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      539 | 4301 | `				rc = PH7_MemObjDump(&(*pOut),pValue,TRUE,nTab+2,nDepth,` |
|      178 | 4302 | `					PH7_ClassAttrIsRef(pThis,pVmAttr));` |
|      361 | 4303 | `				if( rc == SXERR_LIMIT ){` |
|      ! 0 | 4304 | `					break;` |
|        - | 4305 | `				}` |
|      183 | 4306 | `			}else{` |
|        - | 4307 | ``				/* print_r prop: `[x(:…)] => value` at nTab+4; container values`` |
|        - | 4308 | `				 * render their block at nTab+8 followed by php's blank line. */` |
|      587 | 4309 | `				for( i = 0 ; i < nTab + 4 ; i++ ){` |
|      493 | 4310 | `					SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      249 | 4311 | `				}` |
|       99 | 4312 | `				OoDumpPropKey(&(*pOut),pThis,pVmAttr->pAttr,FALSE);` |
|       94 | 4313 | `				if( (pValue->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ))` |
|       57 | 4314 | `				 && (pValue->iFlags & MEMOBJ_NULL) == 0 ){` |
|       12 | 4315 | `					rc = PH7_MemObjDump(&(*pOut),pValue,FALSE,nTab+8,nDepth,0);` |
|       12 | 4316 | `					SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       12 | 4317 | `					if( rc == SXERR_LIMIT ){` |
|      ! 0 | 4318 | `						break;` |
|        - | 4319 | `					}` |
|        7 | 4320 | `				}else{` |
|       89 | 4321 | `					PH7_MemObjPrintRInline(&(*pOut),pValue);` |
|       89 | 4322 | `					SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        - | 4323 | `				}` |
|        - | 4324 | `			}` |
|      225 | 4325 | `		}` |
|        5 | 4326 | `	}` |
|     5573 | 4327 | `	for( i = 0 ; i < nTab ; i++ ){` |
|     5240 | 4328 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     2622 | 4329 | `	}` |
|      337 | 4330 | `	if( ShowType ){` |
|      273 | 4331 | `		SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      139 | 4332 | `	}else{` |
|       69 | 4333 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        - | 4334 | `	}` |
|      337 | 4335 | `	pThis->iFlags &= ~VM_INSTANCE_DUMPING;` |
|      337 | 4336 | `	return rc;` |
|      275 | 4337 | `}` |
|        - | 4338 | `/*` |
|        - | 4339 | ` * Call a magic method [i.e: __toString(),__toBool(),__Invoke()...]` |
|        - | 4340 | ` * Return SXRET_OK on successfull call. Any other return value indicates failure.` |
|        - | 4341 | ` * Notes on magic methods.` |
|        - | 4342 | ` * According to the PHP language reference manual.` |
|        - | 4343 | ` *  The function names __construct(), __destruct(), __call(), __callStatic()` |
|        - | 4344 | ` *  __get(),  __toString(), __invoke(), __clone() are magical in PHP classes.` |
|        - | 4345 | ` * You cannot have functions with these names in any of your classes unless` |
|        - | 4346 | ` * you want the magic functionality associated with them.` |
|        - | 4347 | ` * Example of magical methods:` |
|        - | 4348 | ` * __toString()` |
|        - | 4349 | ` *  The __toString() method allows a class to decide how it will react when it is treated like` |
|        - | 4350 | ` *  a string. For example, what echo $obj; will print. This method must return a string.` |
|        - | 4351 | ` *  Example #2 Simple example` |
|        - | 4352 | ` * <?php` |
|        - | 4353 | ` * // Declare a simple class` |
|        - | 4354 | ` * class TestClass` |
|        - | 4355 | ` * {` |
|        - | 4356 | ` *   public $foo;` |
|        - | 4357 | ` *` |
|        - | 4358 | ` *   public function __construct($foo)` |
|        - | 4359 | ` *   {` |
|        - | 4360 | ` *       $this->foo = $foo;` |
|        - | 4361 | ` *   }` |
|        - | 4362 | ` *` |
|        - | 4363 | ` *   public function __toString()` |
|        - | 4364 | ` *   {` |
|        - | 4365 | ` *       return $this->foo;` |
|        - | 4366 | ` *   }` |
|        - | 4367 | ` * }` |
|        - | 4368 | ` * $class = new TestClass('Hello');` |
|        - | 4369 | ` * echo $class;` |
|        - | 4370 | ` * ?>` |
|        - | 4371 | ` * The above example will output:` |
|        - | 4372 | ` *  Hello` |
|        - | 4373 | ` *` |
|        - | 4374 | ` * Note that PH7 does not support all the magical method and introudces __toFloat(),__toInt()` |
|        - | 4375 | ` * which have the same behaviour as __toString() but for float and integer types` |
|        - | 4376 | ` * respectively.` |
|        - | 4377 | ` * Refer to the official documentation for more information.` |
|        - | 4378 | ` */` |
|      302 | 4379 | `PH7_PRIVATE sxi32 PH7_ClassInstanceCallMagicMethod(` |
|        - | 4380 | `	ph7_vm *pVm,               /* VM that own all this stuff */` |
|        - | 4381 | `	ph7_class *pClass,         /* Target class */` |
|        - | 4382 | `	ph7_class_instance *pThis, /* Target object */` |
|        - | 4383 | `	const char *zMethod,       /* Magic method name [i.e: __toString()]*/` |
|        - | 4384 | `	sxu32 nByte,               /* zMethod length*/` |
|        - | 4385 | `	const SyString *pAttrName, /* Attribute name */` |
|        - | 4386 | `	ph7_value *pResult         /* OUT: magic method return value. NULL to discard */` |
|        - | 4387 | `	)` |
|        4 | 4388 | `{` |
|      306 | 4389 | `	ph7_value *apArg[2] = { 0 , 0 };` |
|        - | 4390 | `	ph7_class_method *pMeth;` |
|        - | 4391 | `	ph7_value sAttr; /* cc warning */` |
|        - | 4392 | `	sxi32 rc;` |
|        - | 4393 | `	int nArg;` |
|      306 | 4394 | `	int bMagicGet = nByte == sizeof("__get")-1 && SyMemcmp(zMethod,"__get",nByte) == 0;` |
|      306 | 4395 | `	int bMagicIsset = nByte == sizeof("__isset")-1 && SyMemcmp(zMethod,"__isset",nByte) == 0;` |
|      302 | 4396 | `	if( (bMagicGet \|\| bMagicIsset) && pAttrName` |
|      301 | 4397 | `	 && PH7_ClassNativePropOwns(pThis,pAttrName) ){` |
|        - | 4398 | `		/* php's read_property / has_property handler for a name the class's own` |
|        - | 4399 | `		 * table carries: it answers before the standard path ever looks for a` |
|        - | 4400 | ``		 * magic accessor, so a subclass's `__get` does not shadow ext/dom's`` |
|        - | 4401 | ``		 * surface. The read-modify-write and `??=` rails reach the handler here;`` |
|        - | 4402 | `		 * the member opcode's own read gate asks it a step earlier. */` |
|        - | 4403 | `		PH7_NativePropCtx sNat;` |
|        - | 4404 | `		ph7_value sNatVal;` |
|        7 | 4405 | `		PH7_MemObjInit(pVm,&sNatVal);` |
|       10 | 4406 | `		if( PH7_ClassNativePropAsk(pThis,&sNat,` |
|        3 | 4407 | `				bMagicIsset ? PH7_NATIVE_PROP_ISSET : PH7_NATIVE_PROP_READ,pAttrName,&sNatVal) ){` |
|        7 | 4408 | `			if( sNat.zThrowClass ){` |
|      ! 0 | 4409 | `				VmBoundaryPark(pVm,VmThrowFixedErrorCode(pVm,sNat.zThrowClass,` |
|      ! 0 | 4410 | `					sNat.iThrowCode,sNat.zThrowMsg));` |
|        7 | 4411 | `			}else if( pResult ){` |
|        7 | 4412 | `				PH7_MemObjStore(&sNatVal,pResult);` |
|        3 | 4413 | `			}` |
|        7 | 4414 | `			PH7_MemObjRelease(&sNatVal);` |
|        7 | 4415 | `			return SXRET_OK;` |
|        - | 4416 | `		}` |
|      ! 0 | 4417 | `		PH7_MemObjRelease(&sNatVal);` |
|      ! 0 | 4418 | `	}` |
|        - | 4419 | `	/* Make sure the magic method is available */` |
|      300 | 4420 | `	pMeth = PH7_ClassExtractMethod(&(*pClass),zMethod,nByte);` |
|      300 | 4421 | `	if( pMeth == 0 ){` |
|        - | 4422 | `		/* No such method,return immediately */` |
|      ! 0 | 4423 | `		return SXERR_NOTFOUND;` |
|        - | 4424 | `	}` |
|      300 | 4425 | `	nArg = 0;` |
|        - | 4426 | `	/* Copy arguments */` |
|      300 | 4427 | `	if( pAttrName ){` |
|      300 | 4428 | `		PH7_MemObjInitFromString(pVm,&sAttr,pAttrName);` |
|      300 | 4429 | `		sAttr.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      300 | 4430 | `		apArg[0] = &sAttr;` |
|      300 | 4431 | `		nArg = 1;` |
|      148 | 4432 | `	}` |
|        - | 4433 | `	/* Call the magic method now */` |
|      300 | 4434 | `	rc = PH7_VmCallMagicMethod(pVm,&(*pThis),pMeth,pResult,nArg,apArg);` |
|        - | 4435 | `	/* Clean up */` |
|      300 | 4436 | `	if( pAttrName ){` |
|      300 | 4437 | `		PH7_MemObjRelease(&sAttr);` |
|      148 | 4438 | `	}` |
|      300 | 4439 | `	return rc;` |
|      155 | 4440 | `}` |
|        - | 4441 | `/*` |
|        - | 4442 | ` * Extract the value of a class instance [i.e: Object in the PHP jargon].` |
|        - | 4443 | ` * This function is simply a wrapper on ExtractClassAttrValue().` |
|        - | 4444 | ` */` |
|  5882071 | 4445 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceExtractAttrValue(ph7_class_instance *pThis,VmClassAttr *pAttr)` |
|        5 | 4446 | `{` |
|        - | 4447 | `   /* Extract the attribute value */` |
|        - | 4448 | `	ph7_value *pValue;` |
|  5882076 | 4449 | `	pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|  5882076 | 4450 | `	return pValue;` |
|        5 | 4451 | `}` |
|        - | 4452 | `/*` |
|        - | 4453 | ` * Convert a class instance [i.e: Object in the PHP jargon] into a hashmap [i.e: array in the PHP jargon].` |
|        - | 4454 | ` * Return SXRET_OK on success. Any other value indicates failure.` |
|        - | 4455 | ` * Note on object conversion to array:` |
|        - | 4456 | ` *  Acccording to the PHP language reference manual` |
|        - | 4457 | ` *  If an object is converted to an array, the result is an array whose elements are the object's properties.` |
|        - | 4458 | ` *  The keys are the member variable names.` |
|        - | 4459 | ` *` |
|        - | 4460 | ` *  The following example:` |
|        - | 4461 | ` *  class Test {` |
|        - | 4462 | ` *   public $A = 25<<1;  // 50` |
|        - | 4463 | ` *	 public $c = rand_str(3);   // Random string of length 3` |
|        - | 4464 | ` *	 public $d = rand() & 1023; // Random number between 0..1023` |
|        - | 4465 | ` *  }` |
|        - | 4466 | ` *  var_dump((array) new Test());` |
|        - | 4467 | ` *	Will output:` |
|        - | 4468 | ` *  array(3) {` |
|        - | 4469 | ` *   [A] =>` |
|        - | 4470 | ` *      int(50)` |
|        - | 4471 | ` *   [c] =>` |
|        - | 4472 | ` *     string(3 'aps')` |
|        - | 4473 | ` *   [d] =>` |
|        - | 4474 | ` *     int(991)` |
|        - | 4475 | ` *  }` |
|        - | 4476 | ` * You have noticed that PH7 allow class attributes [i.e: $a,$c,$d in the example above]` |
|        - | 4477 | ` * have any complex expression (even function calls/Annonymous functions) as their default` |
|        - | 4478 | ` * value unlike the standard PHP engine.` |
|        - | 4479 | ` * This is a very powerful feature that you have to look at.` |
|        - | 4480 | ` */` |
|      858 | 4481 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|        5 | 4482 | `{` |
|        - | 4483 | `	{` |
|        - | 4484 | `		/* php's get_properties handler, which is what the (array) cast reads: a` |
|        - | 4485 | `		 * DateTime casts to date/timezone_type/timezone, not to the hidden slots` |
|        - | 4486 | `		 * holding its timestamp. A class whose hook answers only the DEBUG surface` |
|        - | 4487 | `		 * (WeakReference) fills nothing here, and that EMPTY answer is php's — the` |
|        - | 4488 | `		 * hook is authoritative, so there is no falling back to the slot walk. It` |
|        - | 4489 | `		 * used to fall back when the hook filled nothing, which was invisible while` |
|        - | 4490 | `		 * every hooked class also had no visible property: an ArrayObject SUBCLASS` |
|        - | 4491 | ``		 * with an empty storage would have cast to its own `p` where php casts to`` |
|        - | 4492 | `		 * the (empty) storage. */` |
|        - | 4493 | `		ph7_value sPresent;` |
|      863 | 4494 | `		PH7_MemObjInit(pThis->pVm,&sPresent);` |
|      863 | 4495 | `		sPresent.x.pOther = pMap;` |
|      863 | 4496 | `		MemObjSetType(&sPresent,MEMOBJ_HASHMAP);` |
|      863 | 4497 | `		if( PH7_ClassInstancePresent(pThis,&sPresent,0) ){` |
|        - | 4498 | `			/* The map IS the destination; do not release the carrier's hashmap. */` |
|      679 | 4499 | `			sPresent.x.pOther = 0;` |
|      679 | 4500 | `			sPresent.iFlags = MEMOBJ_NULL;` |
|      679 | 4501 | `			return SXRET_OK;` |
|        - | 4502 | `		}` |
|      189 | 4503 | `		sPresent.x.pOther = 0;` |
|      189 | 4504 | `		sPresent.iFlags = MEMOBJ_NULL;` |
|      189 | 4505 | `		PH7_MemObjRelease(&sPresent);` |
|        - | 4506 | `	}` |
|      189 | 4507 | `	return PH7_ClassInstanceToHashmapRaw(pThis,pMap);` |
|      434 | 4508 | `}` |
|        - | 4509 | `/*` |
|        - | 4510 | ` * Is this property NOT THERE YET?` |
|        - | 4511 | ` *` |
|        - | 4512 | ` * php 7.4's typed properties have a third state beside "holds a value" and "does not` |
|        - | 4513 | `` * exist": a typed property with no default is UNINITIALIZED at `new`, reading it is an`` |
|        - | 4514 | ` * Error rather than a null, and every surface that presents an object's properties` |
|        - | 4515 | ` * leaves it out — get_object_vars(), get_mangled_object_vars(), the (array) cast,` |
|        - | 4516 | ` * foreach, json_encode(), serialize(), print_r() and var_export(). var_dump() is the one` |
|        - | 4517 | ``  * exception and only half of one: it NAMES the property and prints `uninitialized(T)` `` |
|        - | 4518 | ` * where the value would be, and does not count it in the header.` |
|        - | 4519 | ` *` |
|        - | 4520 | ` * The state is already tracked (it is what makes the read throw); this asks it by name` |
|        - | 4521 | ` * so the eight surfaces agree. VM_CLASS_ATTR_UNINIT doubles as the readonly write-once` |
|        - | 4522 | ` * latch, which wants the same answer: a readonly property must be typed and cannot have` |
|        - | 4523 | ` * a default, so before its first write php calls it uninitialized too.` |
|        - | 4524 | ` */` |
|     5441 | 4525 | `PH7_PRIVATE int PH7_ClassAttrUninitialized(VmClassAttr *pVmAttr)` |
|        5 | 4526 | `{` |
|     5446 | 4527 | `	return pVmAttr != 0 && (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) != 0;` |
|        5 | 4528 | `}` |
|        - | 4529 | `/*` |
|        - | 4530 | `` * The same question asked by the four surfaces that read through a php 8.4 `get` HOOK —`` |
|        - | 4531 | ` * get_object_vars(), json_encode(), foreach and var_export().` |
|        - | 4532 | ` *` |
|        - | 4533 | ` * A hooked property's value is whatever its hook answers, so an empty backing slot is` |
|        - | 4534 | ` * not an absence there: a VIRTUAL property has no slot at all and still has a value` |
|        - | 4535 | `` * (php lists `virt` in all four), and a BACKED one whose hook reads its own slot raises`` |
|        - | 4536 | ` * the uninitialized Error from inside the hook — which is php's answer for these four` |
|        - | 4537 | ``  * and not something to pre-empt by skipping the property. Only a property with no `get` `` |
|        - | 4538 | ` * at all is absent.` |
|        - | 4539 | ` */` |
|     2279 | 4540 | `PH7_PRIVATE int PH7_ClassAttrUninitializedForRead(VmClassAttr *pVmAttr)` |
|        5 | 4541 | `{` |
|     2364 | 4542 | `	return PH7_ClassAttrUninitialized(pVmAttr)` |
|     2279 | 4543 | `		&& (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET) == 0;` |
|        5 | 4544 | `}` |
|        - | 4545 | `/*` |
|        - | 4546 | ` * The SLOT walk under the cast above, with php's get_properties handler left out:` |
|        - | 4547 | ` * the instance's own property table, mangled, and nothing else.` |
|        - | 4548 | ` *` |
|        - | 4549 | `` * This is what `get_mangled_object_vars()` answers — php asks the same handler with a`` |
|        - | 4550 | ` * different PURPOSE there, and every class here that has a handler answers the raw` |
|        - | 4551 | `` * table for it: `get_mangled_object_vars(new ArrayObject([1,2]))` is the EMPTY array`` |
|        - | 4552 | `` * where the cast is `[1,2]`, and a DateTime's is empty where the cast has three keys.`` |
|        - | 4553 | ` * Since PHL's engine slots are hidden (and php's equivalents live outside the property` |
|        - | 4554 | ` * table entirely), dropping the handler is the whole difference.` |
|        - | 4555 | ` */` |
|     1108 | 4556 | `static sxi32 ClassInstanceToHashmapRaw(ph7_class_instance *pThis,ph7_hashmap *pMap,int bOwnOnly)` |
|        5 | 4557 | `{` |
|        - | 4558 | `	SyHashEntry *pEntry;` |
|        - | 4559 | `	VmClassAttr *pAttr;` |
|        - | 4560 | `	ph7_value *pValue;` |
|        - | 4561 | `	ph7_value sName;` |
|        - | 4562 | `	/* Reset the loop cursor */` |
|     1113 | 4563 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|     1113 | 4564 | `	PH7_MemObjInitFromString(pThis->pVm,&sName,0);` |
|     7313 | 4565 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|        - | 4566 | `		/* Point to the current attribute */` |
|     6205 | 4567 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|     6205 | 4568 | `		if( !PH7_ClassInstanceAttrPresented(pAttr) ){` |
|        - | 4569 | `			/* Not part of the raw table: a class-level member, a typed property` |
|        - | 4570 | `			 * never written, or a php 8.4 VIRTUAL hooked one. */` |
|     4747 | 4571 | `			continue;` |
|        - | 4572 | `		}` |
|     1463 | 4573 | `		if( bOwnOnly && (pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_NATIVE_SET` |
|        - | 4574 | `			\|PH7_CLASS_ATTR_NATIVE_VIRTUAL\|PH7_CLASS_ATTR_NATIVE_LAZY)) ){` |
|        - | 4575 | `			/* The STATE of a native class whose state happens to be public` |
|        - | 4576 | `			 * (DateInterval's ten, DatePeriod's seven): the caller is building the` |
|        - | 4577 | `			 * shape those belong to, and wants only what the OBJECT added to it. */` |
|      388 | 4578 | `			continue;` |
|        - | 4579 | `		}` |
|        - | 4580 | `		/* Extract attribute value */` |
|     1077 | 4581 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|     1077 | 4582 | `		if( pValue ){` |
|     1077 | 4583 | `			PH7_ClassInstanceAttrKey(pThis,pAttr,&sName);` |
|        - | 4584 | `			/* Perform the insertion. An OWN-props walk laid beside a shape the` |
|        - | 4585 | `			 * caller already built ADDS rather than updates: php's` |
|        - | 4586 | `			 * add_common_properties is a zend_hash_add, so a subclass property` |
|        - | 4587 | `			 * named like one of the internal keys loses to the internal value` |
|        - | 4588 | ``			 * there (`class S extends DateTime { public $date; }` serializes the`` |
|        - | 4589 | `			 * DATE). */` |
|     1077 | 4590 | `			if( bOwnOnly ){` |
|      129 | 4591 | `				ph7_hashmap_node *pDup = 0;` |
|      129 | 4592 | `				if( PH7_HashmapLookup(pMap,&sName,&pDup) == SXRET_OK ){` |
|        3 | 4593 | `					SyBlobReset(&sName.sBlob);` |
|        3 | 4594 | `					continue;` |
|        - | 4595 | `				}` |
|       63 | 4596 | `			}` |
|     1075 | 4597 | `			if( PH7_ClassAttrIsRef(pThis,pAttr) ){` |
|        - | 4598 | `				/* php hands out the property's own REFERENCE, not a copy of what it` |
|        - | 4599 | ``				 * holds: `$v = (array)$o; $v['p'] = 9;` reaches the object when `p` is`` |
|        - | 4600 | ``				 * a reference, and `var_dump()` marks the element `&` in both places.`` |
|        - | 4601 | `				 * Only a property that IS one -- something else names its slot -- and` |
|        - | 4602 | `				 * never the ordinary copy every other element still takes. */` |
|       17 | 4603 | `				PH7_HashmapInsertByRef(pMap,&sName,pAttr->nIdx);` |
|        9 | 4604 | `			}else{` |
|     1059 | 4605 | `				PH7_HashmapInsert(pMap,&sName,pValue);` |
|        - | 4606 | `			}` |
|        - | 4607 | `			/* Reset the string cursor */` |
|     1075 | 4608 | `			SyBlobReset(&sName.sBlob);` |
|      535 | 4609 | `		}` |
|        5 | 4610 | `	}` |
|     1113 | 4611 | `	PH7_MemObjRelease(&sName);` |
|     1113 | 4612 | `	return SXRET_OK;` |
|        5 | 4613 | `}` |
|      304 | 4614 | `PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmapRaw(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|        5 | 4615 | `{` |
|      309 | 4616 | `	return ClassInstanceToHashmapRaw(pThis,pMap,0);` |
|        5 | 4617 | `}` |
|        - | 4618 | `/*` |
|        - | 4619 | ` * The same walk, restricted to what the OBJECT added: a native class's own public` |
|        - | 4620 | ` * STATE is left out, so a subclass's properties can be laid beside the shape that` |
|        - | 4621 | ` * state builds rather than inside it. php's add_common_properties.` |
|        - | 4622 | ` */` |
|      804 | 4623 | `PH7_PRIVATE sxi32 PH7_ClassInstanceOwnPropsToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap)` |
|        3 | 4624 | `{` |
|      807 | 4625 | `	return ClassInstanceToHashmapRaw(pThis,pMap,1);` |
|        3 | 4626 | `}` |
|        - | 4627 | `/*` |
|        - | 4628 | ` * Iterate throw class attributes and invoke the given callback [i.e: xWalk()] for each` |
|        - | 4629 | ` * retrieved attribute.` |
|        - | 4630 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|        - | 4631 | ` * the attribute value in the callback body will not alter the real attribute value.` |
|        - | 4632 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|        - | 4633 | ` * a value different from PH7_OK.` |
|        - | 4634 | ` * Refer to [ph7_object_walk()] for more information.` |
|        - | 4635 | ` */` |
|      ! 0 | 4636 | `PH7_PRIVATE sxi32 PH7_ClassInstanceWalk(` |
|        - | 4637 | `	ph7_class_instance *pThis, /* Target object */` |
|        - | 4638 | `	int (*xWalk)(const char *,ph7_value *,void *), /* Walker callback */` |
|        - | 4639 | `	void *pUserData /* Last argument to xWalk() */` |
|        - | 4640 | `	)` |
|      ! 0 | 4641 | `{` |
|        - | 4642 | `	SyHashEntry *pEntry; /* Hash entry */` |
|        - | 4643 | `	VmClassAttr *pAttr;  /* Pointer to the attribute */` |
|        - | 4644 | `	ph7_value *pValue;   /* Attribute value */` |
|        - | 4645 | `	ph7_value sValue;    /* Copy of the attribute value */` |
|        - | 4646 | `	int rc;` |
|        - | 4647 | `	/* Reset the loop cursor */` |
|      ! 0 | 4648 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      ! 0 | 4649 | `	PH7_MemObjInit(pThis->pVm,&sValue);` |
|        - | 4650 | `	/* Start the walk process */` |
|      ! 0 | 4651 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|        - | 4652 | `		/* Point to the current attribute */` |
|      ! 0 | 4653 | `		pAttr = (VmClassAttr *)pEntry->pUserData;` |
|      ! 0 | 4654 | `		if( PH7_ATTR_UNPRESENTED(pAttr) ){` |
|        - | 4655 | `			/* Class-level members are not part of the object (php) */` |
|      ! 0 | 4656 | `			continue;` |
|        - | 4657 | `		}` |
|      ! 0 | 4658 | `		if( PH7_ClassAttrUninitialized(pAttr) ){` |
|      ! 0 | 4659 | `			continue; /* typed, never written: not there yet (php) */` |
|        - | 4660 | `		}` |
|        - | 4661 | `		/* Extract attribute value */` |
|      ! 0 | 4662 | `		pValue = ExtractClassAttrValue(pThis->pVm,pAttr);` |
|      ! 0 | 4663 | `		if( pValue ){` |
|      ! 0 | 4664 | `			PH7_MemObjLoad(pValue,&sValue);` |
|        - | 4665 | `			/* Invoke the supplied callback */` |
|      ! 0 | 4666 | `			rc =  xWalk(SyStringData(&pAttr->pAttr->sName),&sValue,pUserData);` |
|      ! 0 | 4667 | `			PH7_MemObjRelease(&sValue);` |
|      ! 0 | 4668 | `			if( rc != PH7_OK){` |
|        - | 4669 | `				/* User callback request an operation abort */` |
|      ! 0 | 4670 | `				return SXERR_ABORT;` |
|        - | 4671 | `			}` |
|      ! 0 | 4672 | `		}` |
|      ! 0 | 4673 | `	}` |
|        - | 4674 | `	/* All done */` |
|      ! 0 | 4675 | `	return SXRET_OK;` |
|      ! 0 | 4676 | `}` |
|        - | 4677 | `/*` |
|        - | 4678 | ` * The instance's attribute entry for a property name, INCLUDING the empty one.` |
|        - | 4679 | ` *` |
|        - | 4680 | ` * SyHashGet answers 0 for a zero-length key engine-wide, so a property named ""` |
|        - | 4681 | `` * — php's own `s:0:""` payload builds one, and every surface that walks hAttr`` |
|        - | 4682 | ` * shows it — can only be found by walking the list. Used by the presentation` |
|        - | 4683 | ` * surfaces that snapshot names and re-look-up each one before reading it (a PHP` |
|        - | 4684 | ` * 8.4 get hook may unset a property mid-walk), where the plain hash probe would` |
|        - | 4685 | ` * silently drop it from the output while var_dump, which walks directly, showed` |
|        - | 4686 | ` * it. The walk uses the hash's embedded loop cursor, so it must not run inside` |
|        - | 4687 | ` * another walk of the SAME table — the callers below re-enter only through the` |
|        - | 4688 | ` * hook dispatch, which happens after this returns.` |
|        - | 4689 | ` */` |
|      720 | 4690 | `PH7_PRIVATE SyHashEntry * PH7_ClassInstanceAttrEntry(ph7_class_instance *pThis,const char *zName,sxu32 nName)` |
|        5 | 4691 | `{` |
|        - | 4692 | `	SyHashEntry *pEntry;` |
|      725 | 4693 | `	if( nName > 0 ){` |
|      667 | 4694 | `		return SyHashGet(&pThis->hAttr,(const void *)zName,nName);` |
|        - | 4695 | `	}` |
|       59 | 4696 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      135 | 4697 | `	while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       91 | 4698 | `		if( pEntry->nKeyLen == 0 ){` |
|       15 | 4699 | `			return pEntry;` |
|        - | 4700 | `		}` |
|        1 | 4701 | `	}` |
|       45 | 4702 | `	return 0;` |
|      365 | 4703 | `}` |
|        - | 4704 | `/*` |
|        - | 4705 | ` * Extract a class atrribute value.` |
|        - | 4706 | ` * Return a pointer to the attribute value on success. Otherwise NULL.` |
|        - | 4707 | ` * Note:` |
|        - | 4708 | ` *  Access to static and constant attribute is not allowed. That is,the function` |
|        - | 4709 | ` *  will return NULL in case someone (host-application code) try to extract` |
|        - | 4710 | ` *  a static/constant attribute.` |
|        - | 4711 | ` */` |
|  2752048 | 4712 | `PH7_PRIVATE ph7_value * PH7_ClassInstanceFetchAttr(ph7_class_instance *pThis,const SyString *pName)` |
|        5 | 4713 | `{` |
|        - | 4714 | `	SyHashEntry *pEntry;` |
|        - | 4715 | `	VmClassAttr *pAttr;` |
|        - | 4716 | `	/* Query the attribute hashtable */` |
|  2752053 | 4717 | `	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);` |
|  2752053 | 4718 | `	if( pEntry == 0 ){` |
|        - | 4719 | `		/* No such attribute */` |
|     1837 | 4720 | `		return 0;` |
|        - | 4721 | `	}` |
|        - | 4722 | `	/* Point to the class atrribute */` |
|  2750221 | 4723 | `	pAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - | 4724 | `	/* Check if we are dealing with a static/constant attribute */` |
|  2750221 | 4725 | `	if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - | 4726 | `		/* Access is forbidden */` |
|      ! 0 | 4727 | `		return 0;` |
|        - | 4728 | `	}` |
|        - | 4729 | `	/* Return the attribute value */` |
|  2750221 | 4730 | `	return ExtractClassAttrValue(pThis->pVm,pAttr);` |
|  1375004 | 4731 | `}` |
|        - | 4732 | `/*` |
|        - | 4733 | `` * Does a WRITE through `$obj[k]` land on this class's storage? php answers with`` |
|        - | 4734 | ` * the class's read_dimension handler: an internal class whose own handler hands` |
|        - | 4735 | ` * back the real element supports indirect modification, while everything routed` |
|        - | 4736 | ` * through zend_std_read_dimension — every userland ArrayAccess, and the SPL` |
|        - | 4737 | ` * classes that keep the standard handler (SplFixedArray, the SplDoublyLinkedList` |
|        - | 4738 | ` * family, SplObjectStorage) — gets a TEMPORARY back, so the write is lost and php` |
|        - | 4739 | ` * says so. The engine cannot tell those apart from the interface alone: both` |
|        - | 4740 | ` * implement ArrayAccess.` |
|        - | 4741 | ` *` |
|        - | 4742 | ` * PHL's equivalent evidence is the offsetGet that ANSWERS: the native one on a` |
|        - | 4743 | ` * class carrying PH7_CLASS_DIM_WRITABLE means the storage is reachable, and a` |
|        - | 4744 | ` * userland OVERRIDE of it means it is not — which is php's own rule for an` |
|        - | 4745 | ` * ArrayObject subclass (spl_array_read_dimension steps aside for a declared` |
|        - | 4746 | `` * offsetGet). A `&offsetGet` return is php's other yes: it hands back a reference,`` |
|        - | 4747 | ` * so the write reaches whatever it aliases.` |
|        - | 4748 | ` */` |
|      536 | 4749 | `PH7_PRIVATE int PH7_VmDimFetchWritable(ph7_class *pClass)` |
|        5 | 4750 | `{` |
|        - | 4751 | `	ph7_class_method *pGet;` |
|        - | 4752 | `	ph7_class *pCur;` |
|      541 | 4753 | `	if( pClass == 0 ){` |
|      ! 0 | 4754 | `		return FALSE;` |
|        - | 4755 | `	}` |
|      541 | 4756 | `	pGet = PH7_ClassExtractMethod(pClass,"offsetGet",sizeof("offsetGet")-1);` |
|      541 | 4757 | `	if( pGet == 0 ){` |
|      ! 0 | 4758 | `		return FALSE;` |
|        - | 4759 | `	}` |
|      541 | 4760 | `	if( pGet->sFunc.iFlags & VM_FUNC_REF_RETURN ){` |
|        5 | 4761 | `		return TRUE;` |
|        - | 4762 | `	}` |
|      537 | 4763 | `	if( (pGet->sFunc.iFlags & VM_FUNC_NATIVE) == 0 ){` |
|      161 | 4764 | `		return FALSE;` |
|        - | 4765 | `	}` |
|      664 | 4766 | `	for( pCur = pClass ; pCur ; pCur = pCur->pBase ){` |
|      534 | 4767 | `		if( pCur->iFlags & PH7_CLASS_DIM_WRITABLE ){` |
|      249 | 4768 | `			return TRUE;` |
|        - | 4769 | `		}` |
|      145 | 4770 | `	}` |
|      133 | 4771 | `	return FALSE;` |
|      273 | 4772 | `}` |
|        - | 4773 | `/*` |
|        - | 4774 | ` * The three accessors a VM_FUNC_NATIVE method body uses to reach its receiver.` |
|        - | 4775 | ` *` |
|        - | 4776 | ` * A native method is dispatched down the host-function path, so it is handed the` |
|        - | 4777 | ` * plain (ph7_context*, argc, argv) of any builtin — the receiver rides on the` |
|        - | 4778 | ` * context instead of occupying an argument slot. That is the whole point: the C` |
|        - | 4779 | `` * routine that used to be a global `__prefix_verb($target, ...)` thunk taking its`` |
|        - | 4780 | ` * target explicitly becomes a method whose target is $this.` |
|        - | 4781 | ` */` |
|        - | 4782 | `/*` |
|        - | 4783 | ` * The receiver, or NULL when the method was called statically (and always NULL in` |
|        - | 4784 | ` * a plain host function). Borrowed: the operand stack holds the reference for the` |
|        - | 4785 | ` * duration of the call, so the body must not unref it.` |
|        - | 4786 | ` */` |
|  1809822 | 4787 | `PH7_PRIVATE ph7_class_instance * PH7_ContextThis(ph7_context *pCtx)` |
|        5 | 4788 | `{` |
|  1809827 | 4789 | `	return pCtx->pThis;` |
|        5 | 4790 | `}` |
|        - | 4791 | `/*` |
|        - | 4792 | ` * The class the call was made THROUGH — php's late-static-binding target, so an` |
|        - | 4793 | ` * inherited native method sees the SUBCLASS here, not the class that declared it.` |
|        - | 4794 | ` * NULL in a plain host function.` |
|        - | 4795 | ` */` |
|     1083 | 4796 | `PH7_PRIVATE ph7_class * PH7_ContextCalledClass(ph7_context *pCtx)` |
|        5 | 4797 | `{` |
|     1088 | 4798 | `	return pCtx->pCalledClass;` |
|        5 | 4799 | `}` |
|        - | 4800 | `/*` |
|        - | 4801 | ` * The receiver as a ph7_value, so the body can use the ordinary object helpers` |
|        - | 4802 | ` * (ph7_object_fetch_attr, ph7_value_is_object, ph7_result_value, ...) instead of` |
|        - | 4803 | ` * reaching into ph7_class_instance. NULL when there is no receiver.` |
|        - | 4804 | ` *` |
|        - | 4805 | ` * The view ALIASES the instance without bumping its refcount: it lives exactly as` |
|        - | 4806 | ` * long as the call, during which the caller's reference is already keeping the` |
|        - | 4807 | ` * object alive, and VmReleaseCallContext drops the alias without unreffing. Handing` |
|        - | 4808 | ` * it to ph7_result_value() is safe — that copies through PH7_MemObjStore, which` |
|        - | 4809 | ` * takes its own reference.` |
|        - | 4810 | ` */` |
|    10668 | 4811 | `PH7_PRIVATE ph7_value * PH7_ContextThisValue(ph7_context *pCtx)` |
|        5 | 4812 | `{` |
|    10673 | 4813 | `	if( pCtx->pThis == 0 ){` |
|      ! 0 | 4814 | `		return 0;` |
|        - | 4815 | `	}` |
|    10673 | 4816 | `	if( !pCtx->bThisInit ){` |
|    10673 | 4817 | `		PH7_MemObjInit(pCtx->pVm,&pCtx->sThis);` |
|    10673 | 4818 | `		pCtx->sThis.x.pOther = pCtx->pThis;` |
|    10673 | 4819 | `		pCtx->sThis.iFlags = MEMOBJ_OBJ;` |
|    10673 | 4820 | `		pCtx->sThis.nIdx = SXU32_HIGH;` |
|    10673 | 4821 | `		pCtx->bThisInit = 1;` |
|     5334 | 4822 | `	}` |
|    10673 | 4823 | `	return &pCtx->sThis;` |
|     5339 | 4824 | `}` |
|        - | 4825 |  |
