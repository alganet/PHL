# src/ph7/vm_builtin_spl.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 7583/8435 lines (89.90%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits |  Line | Source |
| ----: | ----: | :--- |
|     - |     1 | `/**` |
|     - |     2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |     3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |     4 | ` */` |
|     - |     5 | `#include "ph7int.h"` |
|     - |     6 | `#include <errno.h>   /* getLinkTarget names the errno text its readlink failed with */` |
|     - |     7 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|     - |     8 | `/*` |
|     - |     9 | ` * SPL iterators, slice 1 (NEWPLAN band D): SeekableIterator, ArrayIterator,` |
|     - |    10 | ` * ArrayObject. (natsort()/natcasesort() used to be declared here as prelude` |
|     - |    11 | ` * wrappers over uasort(...,'strnatcmp'); they are C builtins in hashmap_sort.c` |
|     - |    12 | ` * now -- see ph7_hashmap_natsort -- and the methods below delegate to them.)` |
|     - |    13 | ` * Embedded-PHP chunk following the Reflection architecture — installed` |
|     - |    14 | ` * inside the bCompilingBuiltin window, backed by the engine's native array` |
|     - |    15 | ` * internal-pointer builtins (reset/next/key/current keep their position on a` |
|     - |    16 | ` * property, so ArrayIterator's cursor IS the backing array's pointer).` |
|     - |    17 | ` */` |
|     - |    18 |  |
|     - |    19 | `/*` |
|     - |    20 | ` * ---------------------------------------------------------------------------` |
|     - |    21 | ` * The weak-reference family: WeakReference and WeakMap, declared and bodied in C.` |
|     - |    22 | ` *` |
|     - |    23 | `` * They used to be PHP classes over three global thunks (`__weak_create()`,`` |
|     - |    24 | `` * `__weak_get()`, `__weak_drop()`) that traded the cell pointer back and forth as`` |
|     - |    25 | ` * an opaque int. The cell never belonged in PHP: it is a C lifetime, and the moment` |
|     - |    26 | ` * a class can have C METHOD bodies the thunks are just the methods, spelled with` |
|     - |    27 | `` * the pointer left in the open. They are gone; `__h` is the only slot left, and it`` |
|     - |    28 | ` * is private to a final, uncloneable class.` |
|     - |    29 | ` * ---------------------------------------------------------------------------` |
|     - |    30 | ` */` |
|     - |    31 | `/* The shared cell for a target, created on first use. Takes ONE handle. */` |
|    74 |    32 | `static VmWeakCell * WkCellFor(ph7_vm *pVm,ph7_class_instance *pObj)` |
|     2 |    33 | `{` |
|    76 |    34 | `	SyHashEntry *pEntry = SyHashGet(&pVm->hWeakCell,(const void *)&pObj,sizeof(void *));` |
|     - |    35 | `	VmWeakCell *pCell;` |
|    76 |    36 | `	if( pEntry ){` |
|    20 |    37 | `		pCell = (VmWeakCell *)pEntry->pUserData;` |
|    20 |    38 | `		pCell->nRef++;` |
|    20 |    39 | `		return pCell;` |
|     - |    40 | `	}` |
|    58 |    41 | `	pCell = (VmWeakCell *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmWeakCell));` |
|    58 |    42 | `	if( pCell == 0 ){` |
|   ! 0 |    43 | `		return 0;` |
|     - |    44 | `	}` |
|    58 |    45 | `	pCell->pObj = pObj;` |
|    58 |    46 | `	pCell->pRef = 0;` |
|    58 |    47 | `	pCell->nRef = 1;` |
|     - |    48 | `	/* SyHash stores the key POINTER (no copy): key off the cell's own pObj field —` |
|     - |    49 | `	 * heap-stable for the entry's whole lifetime, and it holds the live pointer` |
|     - |    50 | `	 * bytes until the release hook nulls it (which happens only after the entry` |
|     - |    51 | `	 * is deleted). */` |
|    58 |    52 | `	if( SyHashInsert(&pVm->hWeakCell,(const void *)&pCell->pObj,sizeof(void *),pCell) != SXRET_OK ){` |
|   ! 0 |    53 | `		SyMemBackendFree(&pVm->sAllocator,pCell);` |
|   ! 0 |    54 | `		return 0;` |
|     - |    55 | `	}` |
|    58 |    56 | `	return pCell;` |
|    39 |    57 | `}` |
|     - |    58 | `/* Give one handle back; the last one frees the cell. */` |
|    74 |    59 | `static void WkCellDrop(ph7_vm *pVm,VmWeakCell *pCell)` |
|     2 |    60 | `{` |
|    76 |    61 | `	if( pCell == 0 \|\| pCell->nRef == 0 ){` |
|   ! 0 |    62 | `		return;` |
|     - |    63 | `	}` |
|    76 |    64 | `	pCell->nRef--;` |
|    76 |    65 | `	if( pCell->nRef == 0 ){` |
|    58 |    66 | `		if( pCell->pObj ){` |
|     - |    67 | `			/* Target still alive: unhook the registry entry before freeing. */` |
|    28 |    68 | `			void *pDummy = 0;` |
|    28 |    69 | `			SyHashDeleteEntry(&pVm->hWeakCell,(const void *)&pCell->pObj,sizeof(void *),&pDummy);` |
|    13 |    70 | `		}` |
|    58 |    71 | `		SyMemBackendFree(&pVm->sAllocator,pCell);` |
|    28 |    72 | `	}` |
|    39 |    73 | `}` |
|     - |    74 | `/* The cell a WeakReference instance holds, or NULL. */` |
|   226 |    75 | `static VmWeakCell * WkCellOf(ph7_class_instance *pRef)` |
|     2 |    76 | `{` |
|   228 |    77 | `	return (VmWeakCell *)(sxuptr)(sxu64)PH7_NativeAttrInt(pRef,"__h");` |
|     2 |    78 | `}` |
|     - |    79 | `/* Hand an instance back without owning a reference of our own (ph7_result_value's` |
|     - |    80 | ` * MemObjStore takes the one the result needs). */` |
|   163 |    81 | `static void SplResultBorrowed(ph7_context *pCtx,ph7_class_instance *pObj)` |
|     2 |    82 | `{` |
|     - |    83 | `	ph7_value sObj;` |
|   165 |    84 | `	PH7_MemObjInit(pCtx->pVm,&sObj);` |
|   165 |    85 | `	sObj.x.pOther = pObj;` |
|   165 |    86 | `	MemObjSetType(&sObj,MEMOBJ_OBJ);` |
|   165 |    87 | `	ph7_result_value(pCtx,&sObj);` |
|   165 |    88 | `}` |
|     - |    89 | `/*` |
|     - |    90 | ` * The ONE WeakReference published for a target, created on first ask.` |
|     - |    91 | ` *` |
|     - |    92 | ` * php answers the same object for the same target every time, so the cell caches` |
|     - |    93 | `` * what it handed out. `*pbOwned` says whether the caller holds the fresh instance's`` |
|     - |    94 | ` * reference (and so must give it up once it has been stored somewhere) or is merely` |
|     - |    95 | ` * borrowing the published one.` |
|     - |    96 | ` */` |
|    74 |    97 | `static ph7_class_instance * WkRefFor(ph7_vm *pVm,ph7_class_instance *pObj,int *pbOwned)` |
|     2 |    98 | `{` |
|    76 |    99 | `	VmWeakCell *pCell = WkCellFor(pVm,pObj);` |
|     - |   100 | `	ph7_class *pClass;` |
|     - |   101 | `	ph7_class_instance *pRef;` |
|    76 |   102 | `	*pbOwned = 0;` |
|    76 |   103 | `	if( pCell == 0 ){` |
|   ! 0 |   104 | `		return 0;` |
|     - |   105 | `	}` |
|    76 |   106 | `	if( pCell->pRef ){` |
|     - |   107 | `		/* Give back the handle WkCellFor just took: the publication owns the only one. */` |
|    20 |   108 | `		WkCellDrop(pVm,pCell);` |
|    20 |   109 | `		return pCell->pRef;` |
|     - |   110 | `	}` |
|     - |   111 | ``	/* Built directly rather than through `new`, whose constructor exists only to`` |
|     - |   112 | `	 * refuse (below). */` |
|    58 |   113 | `	pClass = PH7_VmExtractClass(pVm,"WeakReference",sizeof("WeakReference")-1,FALSE,0);` |
|    58 |   114 | `	pRef = pClass ? PH7_NewClassInstance(pVm,pClass) : 0;` |
|    58 |   115 | `	if( pRef == 0 ){` |
|   ! 0 |   116 | `		WkCellDrop(pVm,pCell);` |
|   ! 0 |   117 | `		return 0;` |
|     - |   118 | `	}` |
|    58 |   119 | `	PH7_NativeSetAttrInt(pVm,pRef,"__h",(sxi64)(sxu64)(sxuptr)pCell);` |
|    58 |   120 | `	pCell->pRef = pRef;` |
|    58 |   121 | `	*pbOwned = 1;` |
|    58 |   122 | `	return pRef;` |
|    39 |   123 | `}` |
|     - |   124 | `/* WeakReference::create(object $object): WeakReference */` |
|    34 |   125 | `static int vm_builtin_WeakReference_create(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |   126 | `{` |
|     - |   127 | `	ph7_class_instance *pRef;` |
|    36 |   128 | `	int bOwned = 0;` |
|    36 |   129 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 |   130 | `		ph7_result_null(pCtx);` |
|   ! 0 |   131 | `		return PH7_OK;` |
|     - |   132 | `	}` |
|    36 |   133 | `	pRef = WkRefFor(pCtx->pVm,(ph7_class_instance *)apArg[0]->x.pOther,&bOwned);` |
|    36 |   134 | `	if( pRef == 0 ){` |
|   ! 0 |   135 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |   136 | `	}` |
|    36 |   137 | `	if( bOwned ){` |
|    28 |   138 | `		PH7_NativeResultObject(pCtx,pRef);` |
|    15 |   139 | `	}else{` |
|    10 |   140 | `		SplResultBorrowed(pCtx,pRef);` |
|     - |   141 | `	}` |
|    36 |   142 | `	return PH7_OK;` |
|    19 |   143 | `}` |
|     - |   144 | `/* WeakReference::get(): ?object — the target, or null once it has died. */` |
|    12 |   145 | `static int vm_builtin_WeakReference_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |   146 | `{` |
|    14 |   147 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    14 |   148 | `	VmWeakCell *pCell = pThis ? WkCellOf(pThis) : 0;` |
|     6 |   149 | `	SXUNUSED(nArg);` |
|     6 |   150 | `	SXUNUSED(apArg);` |
|    14 |   151 | `	if( pCell == 0 \|\| pCell->pObj == 0 ){` |
|    10 |   152 | `		ph7_result_null(pCtx);` |
|    10 |   153 | `		return PH7_OK;` |
|     - |   154 | `	}` |
|     5 |   155 | `	SplResultBorrowed(pCtx,pCell->pObj);` |
|     5 |   156 | `	return PH7_OK;` |
|     8 |   157 | `}` |
|     - |   158 | `/*` |
|     - |   159 | ` * php's DEBUG presentation for a WeakReference (ph7_class::xPresent).` |
|     - |   160 | ` *` |
|     - |   161 | ` * var_dump/print_r show ["object"] => the target, or NULL once it has died. The` |
|     - |   162 | ` * (array) cast and var_export show NOTHING — php's get_debug_info and` |
|     - |   163 | ` * get_properties disagree here, which is why the callback is told which is asking.` |
|     - |   164 | ` */` |
|    16 |   165 | `static sxi32 WkPresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|     1 |   166 | `{` |
|     - |   167 | `	VmWeakCell *pCell;` |
|     - |   168 | `	ph7_value sKey, sVal;` |
|    17 |   169 | `	if( !bDebug ){` |
|    13 |   170 | `		return SXRET_OK; /* get_properties: php presents no property at all */` |
|     - |   171 | `	}` |
|     5 |   172 | `	pCell = WkCellOf(pThis);` |
|     5 |   173 | `	PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|     5 |   174 | `	PH7_MemObjStringAppend(&sKey,"object",sizeof("object")-1);` |
|     5 |   175 | `	PH7_MemObjInit(&(*pVm),&sVal);` |
|     5 |   176 | `	if( pCell && pCell->pObj ){` |
|     5 |   177 | `		sVal.x.pOther = pCell->pObj;` |
|     5 |   178 | `		MemObjSetType(&sVal,MEMOBJ_OBJ);` |
|     2 |   179 | `	}` |
|     5 |   180 | `	ph7_array_add_elem(pOut,&sKey,&sVal); /* takes its OWN reference */` |
|     5 |   181 | `	PH7_MemObjRelease(&sKey);` |
|     - |   182 | `	/* Rule 16, the other half: this carrier never HELD a reference — the cell's` |
|     - |   183 | `	 * is the only one — so releasing it as a MEMOBJ_OBJ would unref the target a` |
|     - |   184 | `	 * second time and free a live object under its owner (a segfault two` |
|     - |   185 | `	 * statements later, not here). Blank the carrier before letting it go. */` |
|     5 |   186 | `	sVal.x.pOther = 0;` |
|     5 |   187 | `	sVal.iFlags = MEMOBJ_NULL;` |
|     5 |   188 | `	PH7_MemObjRelease(&sVal);` |
|     5 |   189 | `	return SXRET_OK;` |
|     9 |   190 | `}` |
|     - |   191 | `/*` |
|     - |   192 | ` * WeakReference::__construct()` |
|     - |   193 | ` *` |
|     - |   194 | ` * php declares it PUBLIC and refuses to run it: the class has no way to be built` |
|     - |   195 | ` * except through create(), and the refusal is an Error rather than a visibility` |
|     - |   196 | `` * failure, so `(new ReflectionClass('WeakReference'))->newInstance()` says the same`` |
|     - |   197 | `` * thing `new WeakReference()` does.`` |
|     - |   198 | ` */` |
|     4 |   199 | `static int vm_builtin_WeakReference_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |   200 | `{` |
|     2 |   201 | `	SXUNUSED(nArg);` |
|     2 |   202 | `	SXUNUSED(apArg);` |
|     5 |   203 | `	return PH7_VmThrowException(pCtx,"Error",` |
|     - |   204 | `		"Direct instantiation of WeakReference is not allowed, use WeakReference::create instead");` |
|     1 |   205 | `}` |
|     - |   206 | `/* The handle dies with the instance. This is xRelease, not __destruct: php's` |
|     - |   207 | ` * WeakReference declares no destructor and Reflection must not grow one. */` |
|    60 |   208 | `static void WkRefRelease(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     2 |   209 | `{` |
|    62 |   210 | `	VmWeakCell *pCell = WkCellOf(pThis);` |
|    62 |   211 | `	if( pCell == 0 ){` |
|     5 |   212 | `		return;` |
|     - |   213 | `	}` |
|    58 |   214 | `	if( pCell->pRef == pThis ){` |
|    58 |   215 | `		pCell->pRef = 0;   /* stop publishing an object that is going away */` |
|    28 |   216 | `	}` |
|    58 |   217 | `	PH7_NativeSetAttrInt(pVm,pThis,"__h",0);` |
|    58 |   218 | `	WkCellDrop(pVm,pCell);` |
|    32 |   219 | `}` |
|     - |   220 | `/*` |
|     - |   221 | ` * WeakMap.` |
|     - |   222 | ` *` |
|     - |   223 | `` * Two private arrays keyed by the target's object id (`spl_object_id`, which this`` |
|     - |   224 | ` * engine never reuses): __r holds the WeakReference for the key, __v the value` |
|     - |   225 | ` * mapped to it. Building the weakness out of WeakReference rather than out of raw` |
|     - |   226 | `` * cells is what makes `clone $map` right for free -- the copied array increments`` |
|     - |   227 | ` * each WeakReference's own reference count, and no cell is dropped twice.` |
|     - |   228 | ` */` |
|     - |   229 | `#define WM_REFS "__r"` |
|     - |   230 | `#define WM_VALS "__v"` |
|     - |   231 | `/* One of the two backing arrays, materialized and separated from any copy that` |
|     - |   232 | ` * shares it (a cloned WeakMap starts out sharing both). */` |
|   380 |   233 | `static ph7_hashmap * WmStore(ph7_vm *pVm,ph7_class_instance *pWm,const char *zSlot)` |
|     2 |   234 | `{` |
|   382 |   235 | `	ph7_value *pSlot = PH7_NativeAttr(pWm,zSlot);` |
|   382 |   236 | `	if( pSlot == 0 ){` |
|   ! 0 |   237 | `		return 0;` |
|     - |   238 | `	}` |
|   382 |   239 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    54 |   240 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|   ! 0 |   241 | `			return 0;` |
|     - |   242 | `		}` |
|    26 |   243 | `	}` |
|   382 |   244 | `	return PH7_HashmapCowSeparate(pVm,pSlot);` |
|   192 |   245 | `}` |
|     - |   246 | `/* The entry for an object id, or NULL. */` |
|   184 |   247 | `static ph7_hashmap_node * WmFind(ph7_vm *pVm,ph7_hashmap *pMap,sxi64 iId)` |
|     2 |   248 | `{` |
|     - |   249 | `	ph7_value sKey;` |
|   186 |   250 | `	ph7_hashmap_node *pNode = 0;` |
|   186 |   251 | `	if( pMap == 0 ){` |
|   ! 0 |   252 | `		return 0;` |
|     - |   253 | `	}` |
|   186 |   254 | `	PH7_MemObjInitFromInt(pVm,&sKey,iId);` |
|   186 |   255 | `	if( PH7_HashmapLookup(pMap,&sKey,&pNode) != SXRET_OK ){` |
|    54 |   256 | `		pNode = 0;` |
|    26 |   257 | `	}` |
|   186 |   258 | `	PH7_MemObjRelease(&sKey);` |
|   186 |   259 | `	return pNode;` |
|    94 |   260 | `}` |
|    80 |   261 | `static void WmPut(ph7_vm *pVm,ph7_hashmap *pMap,sxi64 iId,ph7_value *pVal)` |
|     2 |   262 | `{` |
|     - |   263 | `	ph7_value sKey;` |
|    82 |   264 | `	if( pMap == 0 ){` |
|   ! 0 |   265 | `		return;` |
|     - |   266 | `	}` |
|    82 |   267 | `	PH7_MemObjInitFromInt(pVm,&sKey,iId);` |
|    82 |   268 | `	PH7_HashmapInsert(pMap,&sKey,pVal);` |
|    82 |   269 | `	PH7_MemObjRelease(&sKey);` |
|    42 |   270 | `}` |
|    22 |   271 | `static void WmErase(ph7_vm *pVm,ph7_hashmap *pMap,sxi64 iId)` |
|     2 |   272 | `{` |
|    24 |   273 | `	ph7_hashmap_node *pNode = WmFind(pVm,pMap,iId);` |
|    24 |   274 | `	if( pNode ){` |
|    20 |   275 | `		PH7_HashmapUnlinkNode(pNode,TRUE);` |
|     9 |   276 | `	}` |
|    24 |   277 | `}` |
|     - |   278 | `/* The target a __r entry still points at, or NULL once it has died. */` |
|   158 |   279 | `static ph7_class_instance * WmNodeTarget(ph7_hashmap_node *pNode)` |
|     2 |   280 | `{` |
|   160 |   281 | `	ph7_value *pVal = pNode ? HashmapExtractNodeValue(pNode) : 0;` |
|     - |   282 | `	VmWeakCell *pCell;` |
|   160 |   283 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     9 |   284 | `		return 0;` |
|     - |   285 | `	}` |
|   152 |   286 | `	pCell = WkCellOf((ph7_class_instance *)pVal->x.pOther);` |
|   152 |   287 | `	return pCell ? pCell->pObj : 0;` |
|    81 |   288 | `}` |
|     - |   289 | `/* Forget every entry whose key has died. php prunes on count() and on iteration,` |
|     - |   290 | ` * which is the only reason a WeakMap's size ever changes on its own. */` |
|    54 |   291 | `static void WmPrune(ph7_vm *pVm,ph7_class_instance *pWm)` |
|     2 |   292 | `{` |
|    56 |   293 | `	ph7_hashmap *pRefs = WmStore(pVm,pWm,WM_REFS);` |
|    56 |   294 | `	ph7_hashmap *pVals = WmStore(pVm,pWm,WM_VALS);` |
|     - |   295 | `	ph7_hashmap_node *pNode,*pNext;` |
|    56 |   296 | `	if( pRefs == 0 ){` |
|   ! 0 |   297 | `		return;` |
|     - |   298 | `	}` |
|     - |   299 | `	/* pFirst then the pPrev chain IS insertion order here: MACRO_LD_PUSH links a` |
|     - |   300 | `	 * new node in through pNext, so pNext points at the OLDER neighbour. */` |
|   140 |   301 | `	for( pNode = pRefs->pFirst ; pNode ; pNode = pNext ){` |
|    86 |   302 | `		pNext = pNode->pPrev;` |
|    86 |   303 | `		if( WmNodeTarget(pNode) == 0 ){` |
|     8 |   304 | `			sxi64 iId = pNode->xKey.iKey;` |
|     8 |   305 | `			PH7_HashmapUnlinkNode(pNode,TRUE);` |
|     8 |   306 | `			WmErase(pVm,pVals,iId);` |
|     3 |   307 | `		}` |
|    44 |   308 | `	}` |
|    29 |   309 | `}` |
|     - |   310 | `/* Every WeakMap entry point but count() takes an object key and says so the same` |
|     - |   311 | ` * way php does. Answers the id, or -1 after raising the TypeError. */` |
|    56 |   312 | `static sxi64 WmKeyId(ph7_context *pCtx,int nArg,ph7_value **apArg,ph7_class_instance **ppObj)` |
|     2 |   313 | `{` |
|    58 |   314 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     3 |   315 | `		PH7_VmThrowException(pCtx,"TypeError","WeakMap key must be an object");` |
|     3 |   316 | `		return -1;` |
|     - |   317 | `	}` |
|    56 |   318 | `	*ppObj = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    56 |   319 | `	return (sxi64)(*ppObj)->nObjId;` |
|    30 |   320 | `}` |
|    42 |   321 | `static int vm_builtin_WeakMap_offsetSet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |   322 | `{` |
|    44 |   323 | `	ph7_vm *pVm = pCtx->pVm;` |
|    44 |   324 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    44 |   325 | `	ph7_class_instance *pObj = 0;` |
|     - |   326 | `	ph7_hashmap *pRefs,*pVals;` |
|    44 |   327 | `	sxi64 iId = WmKeyId(pCtx,nArg,apArg,&pObj);` |
|    44 |   328 | `	if( pThis == 0 \|\| iId < 0 ){` |
|     3 |   329 | `		return PH7_OK;` |
|     - |   330 | `	}` |
|    42 |   331 | `	pRefs = WmStore(pVm,pThis,WM_REFS);` |
|    42 |   332 | `	pVals = WmStore(pVm,pThis,WM_VALS);` |
|    42 |   333 | `	if( WmFind(pVm,pRefs,iId) == 0 ){` |
|     - |   334 | `		/* First value for this key: hold it through the very WeakReference` |
|     - |   335 | `		 * create() publishes, so the map and userland share one cell. */` |
|    42 |   336 | `		int bOwned = 0;` |
|    42 |   337 | `		ph7_class_instance *pRef = WkRefFor(pVm,pObj,&bOwned);` |
|     - |   338 | `		ph7_value sRef;` |
|    42 |   339 | `		if( pRef == 0 ){` |
|   ! 0 |   340 | `			return PH7_ContextMemoryError(pCtx);` |
|     - |   341 | `		}` |
|    42 |   342 | `		PH7_MemObjInit(pVm,&sRef);` |
|    42 |   343 | `		sRef.x.pOther = pRef;` |
|    42 |   344 | `		MemObjSetType(&sRef,MEMOBJ_OBJ);` |
|    42 |   345 | `		WmPut(pVm,pRefs,iId,&sRef);   /* takes a reference of its own */` |
|    42 |   346 | `		if( bOwned ){` |
|    32 |   347 | `			PH7_ClassInstanceUnref(pRef);` |
|    15 |   348 | `		}` |
|    20 |   349 | `	}` |
|    42 |   350 | `	WmPut(pVm,pVals,iId,nArg > 1 ? apArg[1] : 0);` |
|    42 |   351 | `	return PH7_OK;` |
|    23 |   352 | `}` |
|     4 |   353 | `static int vm_builtin_WeakMap_offsetGet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |   354 | `{` |
|     5 |   355 | `	ph7_vm *pVm = pCtx->pVm;` |
|     5 |   356 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     5 |   357 | `	ph7_class_instance *pObj = 0;` |
|     - |   358 | `	ph7_hashmap_node *pNode;` |
|     5 |   359 | `	sxi64 iId = WmKeyId(pCtx,nArg,apArg,&pObj);` |
|     5 |   360 | `	if( pThis == 0 \|\| iId < 0 ){` |
|   ! 0 |   361 | `		return PH7_OK;` |
|     - |   362 | `	}` |
|     5 |   363 | `	if( WmNodeTarget(WmFind(pVm,WmStore(pVm,pThis,WM_REFS),iId)) != pObj ){` |
|     7 |   364 | `		return PH7_VmThrowException(pCtx,"Error","Object %z#%d not contained in WeakMap",` |
|     4 |   365 | `			&pObj->pClass->sName,(int)pObj->nObjId);` |
|     - |   366 | `	}` |
|   ! 0 |   367 | `	pNode = WmFind(pVm,WmStore(pVm,pThis,WM_VALS),iId);` |
|   ! 0 |   368 | `	if( pNode ){` |
|   ! 0 |   369 | `		ph7_result_value(pCtx,HashmapExtractNodeValue(pNode));` |
|   ! 0 |   370 | `	}` |
|   ! 0 |   371 | `	return PH7_OK;` |
|     3 |   372 | `}` |
|     2 |   373 | `static int vm_builtin_WeakMap_offsetExists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |   374 | `{` |
|     3 |   375 | `	ph7_vm *pVm = pCtx->pVm;` |
|     3 |   376 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     3 |   377 | `	ph7_class_instance *pObj = 0;` |
|     3 |   378 | `	sxi64 iId = WmKeyId(pCtx,nArg,apArg,&pObj);` |
|     3 |   379 | `	if( pThis == 0 \|\| iId < 0 ){` |
|   ! 0 |   380 | `		return PH7_OK;` |
|     - |   381 | `	}` |
|     3 |   382 | `	ph7_result_bool(pCtx,WmNodeTarget(WmFind(pVm,WmStore(pVm,pThis,WM_REFS),iId)) == pObj);` |
|     3 |   383 | `	return PH7_OK;` |
|     2 |   384 | `}` |
|     8 |   385 | `static int vm_builtin_WeakMap_offsetUnset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |   386 | `{` |
|    10 |   387 | `	ph7_vm *pVm = pCtx->pVm;` |
|    10 |   388 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    10 |   389 | `	ph7_class_instance *pObj = 0;` |
|    10 |   390 | `	sxi64 iId = WmKeyId(pCtx,nArg,apArg,&pObj);` |
|    10 |   391 | `	if( pThis == 0 \|\| iId < 0 ){` |
|   ! 0 |   392 | `		return PH7_OK;` |
|     - |   393 | `	}` |
|    10 |   394 | `	WmErase(pVm,WmStore(pVm,pThis,WM_REFS),iId);` |
|    10 |   395 | `	WmErase(pVm,WmStore(pVm,pThis,WM_VALS),iId);` |
|    10 |   396 | `	return PH7_OK;` |
|     6 |   397 | `}` |
|    20 |   398 | `static int vm_builtin_WeakMap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |   399 | `{` |
|    22 |   400 | `	ph7_vm *pVm = pCtx->pVm;` |
|    22 |   401 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |   402 | `	ph7_hashmap *pRefs;` |
|    10 |   403 | `	SXUNUSED(nArg);` |
|    10 |   404 | `	SXUNUSED(apArg);` |
|    22 |   405 | `	if( pThis == 0 ){` |
|   ! 0 |   406 | `		ph7_result_int(pCtx,0);` |
|   ! 0 |   407 | `		return PH7_OK;` |
|     - |   408 | `	}` |
|    22 |   409 | `	WmPrune(pVm,pThis);` |
|    22 |   410 | `	pRefs = WmStore(pVm,pThis,WM_REFS);` |
|    22 |   411 | `	ph7_result_int64(pCtx,pRefs ? (ph7_int64)pRefs->nEntry : 0);` |
|    22 |   412 | `	return PH7_OK;` |
|    12 |   413 | `}` |
|     - |   414 | `/*` |
|     - |   415 | ` * The WeakMap walk, as the vtable an InternalIterator drives.` |
|     - |   416 | ` *` |
|     - |   417 | ` * The cursor is the object id it sits on (POS), plus the id it expects to move to` |
|     - |   418 | ` * (AUX). Both are looked up fresh at every step, which is what makes the walk LIVE:` |
|     - |   419 | ` * an entry added during a foreach is reached (it is the current node's new pNext),` |
|     - |   420 | ` * one removed ahead of the cursor is skipped, and removing the CURRENT entry -- the` |
|     - |   421 | `` * `foreach($m as $k=>$v) unset($m[$k]);` idiom -- still lands on the successor AUX`` |
|     - |   422 | ` * recorded when the cursor settled.` |
|     - |   423 | ` */` |
|    60 |   424 | `static void WmSettle(ph7_vm *pVm,ph7_class_instance *pIt,ph7_hashmap_node *pNode)` |
|     2 |   425 | `{` |
|    62 |   426 | `	ph7_class_instance *pWm = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);` |
|    62 |   427 | `	ph7_class_instance *pTarget = 0;` |
|     - |   428 | `	ph7_hashmap_node *pVal;` |
|     - |   429 | `	ph7_value *pSlot;` |
|    62 |   430 | `	while( pNode ){` |
|    44 |   431 | `		pTarget = WmNodeTarget(pNode);` |
|    44 |   432 | `		if( pTarget ){` |
|    44 |   433 | `			break;` |
|     - |   434 | `		}` |
|   ! 0 |   435 | `		pNode = pNode->pPrev;   /* a key that died since the last prune */` |
|   ! 0 |   436 | `	}` |
|    62 |   437 | `	if( pNode == 0 \|\| pTarget == 0 \|\| pWm == 0 ){` |
|    20 |   438 | `		PH7_NativeSetAttrBool(pVm,pIt,PH7_NATIVE_IT_DONE,1);` |
|    20 |   439 | `		return;` |
|     - |   440 | `	}` |
|    44 |   441 | `	PH7_NativeSetAttrInt(pVm,pIt,PH7_NATIVE_IT_POS,pNode->xKey.iKey);` |
|    44 |   442 | `	PH7_NativeSetAttrInt(pVm,pIt,PH7_NATIVE_IT_AUX,pNode->pPrev ? pNode->pPrev->xKey.iKey : -1);` |
|    44 |   443 | `	PH7_NativeSetAttrObj(pVm,pIt,PH7_NATIVE_IT_KEY,pTarget);` |
|    44 |   444 | `	pVal = WmFind(pVm,WmStore(pVm,pWm,WM_VALS),pNode->xKey.iKey);` |
|    44 |   445 | `	pSlot = pVal ? PH7_NativeAttr(pIt,PH7_NATIVE_IT_CUR) : 0;` |
|    44 |   446 | `	if( pSlot ){` |
|    44 |   447 | `		PH7_MemObjStore(HashmapExtractNodeValue(pVal),pSlot);` |
|    23 |   448 | `	}else{` |
|   ! 0 |   449 | `		PH7_NativeSetAttrObj(pVm,pIt,PH7_NATIVE_IT_CUR,0);   /* stores NULL */` |
|     - |   450 | `	}` |
|    44 |   451 | `	PH7_NativeSetAttrBool(pVm,pIt,PH7_NATIVE_IT_DONE,0);` |
|    32 |   452 | `}` |
|    34 |   453 | `static void WmRewind(ph7_vm *pVm,ph7_class_instance *pIt)` |
|     2 |   454 | `{` |
|    36 |   455 | `	ph7_class_instance *pWm = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);` |
|     - |   456 | `	ph7_hashmap *pRefs;` |
|    36 |   457 | `	if( pWm == 0 ){` |
|   ! 0 |   458 | `		PH7_NativeSetAttrBool(pVm,pIt,PH7_NATIVE_IT_DONE,1);` |
|   ! 0 |   459 | `		return;` |
|     - |   460 | `	}` |
|    36 |   461 | `	WmPrune(pVm,pWm);` |
|    36 |   462 | `	pRefs = WmStore(pVm,pWm,WM_REFS);` |
|    36 |   463 | `	WmSettle(pVm,pIt,pRefs ? pRefs->pFirst : 0);` |
|    19 |   464 | `}` |
|    26 |   465 | `static void WmNext(ph7_vm *pVm,ph7_class_instance *pIt)` |
|     2 |   466 | `{` |
|    28 |   467 | `	ph7_class_instance *pWm = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);` |
|    28 |   468 | `	ph7_hashmap *pRefs = pWm ? WmStore(pVm,pWm,WM_REFS) : 0;` |
|    28 |   469 | `	ph7_hashmap_node *pNode = WmFind(pVm,pRefs,PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS));` |
|    28 |   470 | `	if( pNode ){` |
|    28 |   471 | `		pNode = pNode->pPrev;   /* the next-inserted entry; see WmPrune */` |
|    15 |   472 | `	}else{` |
|     - |   473 | `		/* The entry we were sitting on is gone: fall back on the successor` |
|     - |   474 | `		 * recorded when it settled. */` |
|   ! 0 |   475 | `		sxi64 iAux = PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_AUX);` |
|   ! 0 |   476 | `		pNode = iAux < 0 ? 0 : WmFind(pVm,pRefs,iAux);` |
|     - |   477 | `	}` |
|    28 |   478 | `	WmSettle(pVm,pIt,pNode);` |
|    28 |   479 | `}` |
|     - |   480 | `static const PH7_NativeIterVtab sWmIterVtab = { WmRewind, WmNext, 0, 0 };` |
|     - |   481 | `/* WeakMap::getIterator(): Iterator — a PHP GENERATOR before, which a C body cannot` |
|     - |   482 | ` * be; php answers an InternalIterator here, and so does this. */` |
|    18 |   483 | `static int vm_builtin_WeakMap_getIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |   484 | `{` |
|    20 |   485 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |   486 | `	ph7_class_instance *pIt;` |
|     9 |   487 | `	SXUNUSED(nArg);` |
|     9 |   488 | `	SXUNUSED(apArg);` |
|    20 |   489 | `	if( pThis == 0 ){` |
|   ! 0 |   490 | `		return PH7_OK;` |
|     - |   491 | `	}` |
|    20 |   492 | `	pIt = PH7_NativeIteratorNew(pCtx->pVm,pThis);` |
|    20 |   493 | `	if( pIt == 0 ){` |
|   ! 0 |   494 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |   495 | `	}` |
|    20 |   496 | `	PH7_NativeResultObject(pCtx,pIt);` |
|    20 |   497 | `	return PH7_OK;` |
|    11 |   498 | `}` |
|     - |   499 | `/*` |
|     - |   500 | ` * Declare both classes. WeakMap's three interfaces all declare METHODS, so they are` |
|     - |   501 | ` * attached only once its own exist -- PH7_ClassImplement stubs a missing one as` |
|     - |   502 | ` * ABSTRACT, which would leave the class uninstantiable.` |
|     - |   503 | ` */` |
|  6721 |   504 | `static sxi32 VmInstallWeak(ph7_vm *pVm)` |
|     5 |   505 | `{` |
|     - |   506 | `	static const PH7_NativePropDef aRefProp[] = {` |
|     - |   507 | `		/* The shared cell, as a pointer. Private to a final class and never handed` |
|     - |   508 | ``		 * to PHP -- what `__weak_create()` used to return into a userland slot. */`` |
|     - |   509 | `		{ "__h", PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |   510 | `	};` |
|     - |   511 | `	static const PH7_NativeMethodDef aRefMethod[] = {` |
|     - |   512 | `		{ "__construct", PH7_MOD_PUBLIC, "", "", vm_builtin_WeakReference_construct },` |
|     - |   513 | `		{ "create",      PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "object $object", "WeakReference",` |
|     - |   514 | `		  vm_builtin_WeakReference_create },` |
|     - |   515 | `		{ "get",         PH7_MOD_PUBLIC, "", "?object", vm_builtin_WeakReference_get },` |
|     - |   516 | `	};` |
|     - |   517 | `	static const PH7_NativePropDef aMapProp[] = {` |
|     - |   518 | `		{ WM_REFS, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |   519 | `		{ WM_VALS, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |   520 | `	};` |
|     - |   521 | `	static const PH7_NativeMethodDef aMapMethod[] = {` |
|     - |   522 | `		/* php leaves the key parameter UNTYPED and screens it in the body, so that` |
|     - |   523 | `		 * a scalar key is "WeakMap key must be an object" rather than a ZPP report. */` |
|     - |   524 | `		/* php's declaration order, which is the order Reflection reports. */` |
|     - |   525 | `		{ "offsetGet",    PH7_MOD_PUBLIC, "$object", "mixed", vm_builtin_WeakMap_offsetGet },` |
|     - |   526 | `		{ "offsetSet",    PH7_MOD_PUBLIC, "$object, mixed $value", "void", vm_builtin_WeakMap_offsetSet },` |
|     - |   527 | `		{ "offsetExists", PH7_MOD_PUBLIC, "$object", "bool", vm_builtin_WeakMap_offsetExists },` |
|     - |   528 | `		{ "offsetUnset",  PH7_MOD_PUBLIC, "$object", "void", vm_builtin_WeakMap_offsetUnset },` |
|     - |   529 | `		{ "count",        PH7_MOD_PUBLIC, "", "int", vm_builtin_WeakMap_count },` |
|     - |   530 | `		{ "getIterator",  PH7_MOD_PUBLIC, "", "Iterator", vm_builtin_WeakMap_getIterator },` |
|     - |   531 | `	};` |
|     - |   532 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - |   533 | ``		/* Uncloneable in php too: the cell `__h` names is shared, and a slot-by-slot`` |
|     - |   534 | `		 * copy would drop it twice. */` |
|     - |   535 | `		{ "WeakReference", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|     - |   536 | `		  aRefMethod, SX_ARRAYSIZE(aRefMethod), 0, 0, aRefProp, SX_ARRAYSIZE(aRefProp),` |
|     - |   537 | `		  WkRefRelease, 0, WkPresent },` |
|     - |   538 | `		/* php's WeakMap read_dimension answers the stored zval itself, so an` |
|     - |   539 | `		 * indirect modification through it lands (PH7_CLASS_DIM_WRITABLE). */` |
|     - |   540 | `		{ "WeakMap", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOSERIALIZE\|PH7_CLASS_DIM_WRITABLE,` |
|     - |   541 | `		  aMapMethod, SX_ARRAYSIZE(aMapMethod), 0, 0, aMapProp, SX_ARRAYSIZE(aMapProp),` |
|     - |   542 | `		  0, &sWmIterVtab, 0 },` |
|     - |   543 | `	};` |
|     - |   544 | `	static const char *azMapIface[] = { "ArrayAccess", "Countable", "IteratorAggregate" };` |
|     - |   545 | `	ph7_class *pMap;` |
|     - |   546 | `	sxu32 n;` |
|  6726 |   547 | `	sxi32 rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|  6726 |   548 | `	if( rc != SXRET_OK ){` |
|   ! 0 |   549 | `		return rc;` |
|     - |   550 | `	}` |
|  6726 |   551 | `	pMap = PH7_VmExtractClass(&(*pVm),"WeakMap",sizeof("WeakMap")-1,FALSE,0);` |
|  6726 |   552 | `	if( pMap == 0 ){` |
|   ! 0 |   553 | `		return SXERR_NOTFOUND;` |
|     - |   554 | `	}` |
| 26889 |   555 | `	for( n = 0 ; n < SX_ARRAYSIZE(azMapIface) ; n++ ){` |
| 30236 |   556 | `		ph7_class *pIface = PH7_VmExtractClass(&(*pVm),azMapIface[n],` |
| 20163 |   557 | `			(sxu32)SyStrlen(azMapIface[n]),FALSE,0);` |
| 20168 |   558 | `		if( pIface == 0 ){` |
|   ! 0 |   559 | `			return SXERR_NOTFOUND;` |
|     - |   560 | `		}` |
| 20168 |   561 | `		rc = PH7_ClassImplement(pMap,pIface);` |
| 20168 |   562 | `		if( rc != SXRET_OK ){` |
|   ! 0 |   563 | `			return rc;` |
|     - |   564 | `		}` |
| 10073 |   565 | `	}` |
|  6726 |   566 | `	return SXRET_OK;` |
|  3361 |   567 | `}` |
|     - |   568 |  |
|     - |   569 | `/*` |
|     - |   570 | ` * ---------------------------------------------------------------------------` |
|     - |   571 | `` * The MEMBERS slot every SPL container's `__serialize()` carries.`` |
|     - |   572 | ` *` |
|     - |   573 | ` * php's payload for these classes is its own state plus a members array, and that` |
|     - |   574 | `` * array is `zend_std_get_properties()` — the instance's REAL properties. For a bare`` |
|     - |   575 | ` * SplObjectStorage or SplStack there are none, which is why every one of these bodies` |
|     - |   576 | ` * shipped with a hardcoded empty array; the moment a user SUBCLASSES one, its declared` |
|     - |   577 | `` * properties belong in the payload and PHL dropped them. `serialize()` then round-`` |
|     - |   578 | ` * tripped a SplStack subclass back to its property DEFAULTS with nothing failing —` |
|     - |   579 | ` * the same shape of silent wrong answer the date family's hidden slots had, one layer` |
|     - |   580 | ` * up. These two are that walk, shared by every container below.` |
|     - |   581 | ` *` |
|     - |   582 | `` * The walk must skip `PH7_CLASS_ATTR_HIDDEN`: a native class's engine slots are`` |
|     - |   583 | ` * exactly what the pair exists to replace. The load side writes only slots the class` |
|     - |   584 | ` * DECLARES, which is what the engine's own unserialize does with an unknown name` |
|     - |   585 | ` * (PHL has no dynamic properties).` |
|     - |   586 | ` * ---------------------------------------------------------------------------` |
|     - |   587 | ` */` |
|   114 |   588 | `static void SplAddMembers(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)` |
|     1 |   589 | `{` |
|     - |   590 | `	SyHashEntry *pEntry;` |
|   115 |   591 | `	if( pThis == 0 ){` |
|   ! 0 |   592 | `		return;` |
|     - |   593 | `	}` |
|   115 |   594 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|   453 |   595 | `	while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|   339 |   596 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     - |   597 | `		ph7_value *pVal;` |
|     - |   598 | `		ph7_value sKey;` |
|   338 |   599 | `		if( PH7_ATTR_UNPRESENTED(pVmAttr)` |
|    31 |   600 | `		 \|\| (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL) ){` |
|   309 |   601 | `			continue;` |
|     - |   602 | `		}` |
|    31 |   603 | `		pVal = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|    31 |   604 | `		if( pVal == 0 ){` |
|   ! 0 |   605 | `			continue;` |
|     - |   606 | `		}` |
|    31 |   607 | `		PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|     - |   608 | ``		/* php's MANGLED name -- `\0*\0p` for a protected member and`` |
|     - |   609 | ``		 * `\0Declaring\0p` for a private one. Every door this walk feeds is one`` |
|     - |   610 | ``		 * php hands the raw table to: the debug array `var_dump()` prints and`` |
|     - |   611 | ``		 * `__debugInfo()` answers, `serialize()`'s member map, and the (array) cast`` |
|     - |   612 | `		 * a STD_PROP_LIST container takes. PHL spelled all four with the bare name,` |
|     - |   613 | ``		 * so a subclass's private property printed `[pri]` and serialized under a`` |
|     - |   614 | `		 * key php's unserializer reads as a PUBLIC one. */` |
|    31 |   615 | `		PH7_ClassInstanceAttrKey(pThis,pVmAttr,&sKey);` |
|    31 |   616 | `		ph7_array_add_elem(pOut,&sKey,pVal);` |
|    31 |   617 | `		PH7_MemObjRelease(&sKey);` |
|     1 |   618 | `	}` |
|    58 |   619 | `}` |
|     - |   620 | `/* The same walk as a standalone array, which is the shape most payloads want. */` |
|    82 |   621 | `static sxi32 SplMembersOf(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)` |
|     1 |   622 | `{` |
|    83 |   623 | `	PH7_MemObjInit(&(*pVm),pOut);` |
|    83 |   624 | `	if( PH7_MemObjToHashmap(pOut) != SXRET_OK ){` |
|   ! 0 |   625 | `		return SXERR_MEM;` |
|     - |   626 | `	}` |
|    83 |   627 | `	SplAddMembers(&(*pVm),pThis,pOut);` |
|    83 |   628 | `	return SXRET_OK;` |
|    42 |   629 | `}` |
|    14 |   630 | `static int SplMembersWalk(ph7_value *pKey,ph7_value *pVal,void *pUserData)` |
|     1 |   631 | `{` |
|    15 |   632 | `	ph7_class_instance *pThis = (ph7_class_instance *)pUserData;` |
|     - |   633 | `	const char *zKey;` |
|     - |   634 | `	int nKey;` |
|    15 |   635 | `	if( !ph7_value_is_string(pKey) ){` |
|   ! 0 |   636 | `		return PH7_OK;` |
|     - |   637 | `	}` |
|    15 |   638 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|    15 |   639 | `	if( nKey < 1 ){` |
|   ! 0 |   640 | `		return PH7_OK;` |
|     - |   641 | `	}` |
|    15 |   642 | `	if( SyHashGet(&pThis->hAttr,(const void *)zKey,(sxu32)nKey) == 0 ){` |
|     - |   643 | `		/* php restores a member the class never declared as a DYNAMIC property;` |
|     - |   644 | `		 * PH7_NativeSetProp only writes a slot that already exists, so a payload` |
|     - |   645 | `		 * whose members were the object's own additions came back empty --` |
|     - |   646 | ``		 * `__unserialize([0, [], ['p' => 7], null])` left `$o->p` unset. */`` |
|   ! 0 |   647 | `		ph7_value *pSlot = PH7_VmCreateDynamicAttr(pThis->pVm,pThis,` |
|   ! 0 |   648 | `			zKey,(sxu32)nKey,0);` |
|   ! 0 |   649 | `		if( pSlot ){` |
|   ! 0 |   650 | `			PH7_MemObjStore(pVal,pSlot);` |
|   ! 0 |   651 | `		}` |
|   ! 0 |   652 | `		return PH7_OK;` |
|     - |   653 | `	}` |
|    15 |   654 | `	PH7_NativeSetProp(pThis->pVm,pThis,zKey,(sxu32)nKey,pVal);` |
|    15 |   655 | `	return PH7_OK;` |
|     8 |   656 | `}` |
|    44 |   657 | `static void SplMembersLoad(ph7_class_instance *pThis,ph7_value *pMembers)` |
|     1 |   658 | `{` |
|    45 |   659 | `	if( pThis == 0 \|\| pMembers == 0 \|\| (pMembers->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 |   660 | `		return;` |
|     - |   661 | `	}` |
|    45 |   662 | `	ph7_array_walk(pMembers,SplMembersWalk,pThis);` |
|    23 |   663 | `}` |
|     - |   664 |  |
|     - |   665 | `/*` |
|     - |   666 | ` * ArrayIterator / ArrayObject — the array STORE, in C.` |
|     - |   667 | ` *` |
|     - |   668 | `` * These two shared one implementation through `trait __SplStoreT`, the last PHL-only`` |
|     - |   669 | ` * TRAIT and the last name in the §4 ledger that was not a function. php shares nothing` |
|     - |   670 | ` * between them at the TYPE level: both have no parent and no common interface beyond` |
|     - |   671 | `` * ArrayAccess/Countable, and the storage lives in ext/spl's own `spl_array_object` struct`` |
|     - |   672 | ` * behind handlers. The recorded decision follows php: no shared type at all —` |
|     - |   673 | ` * one set of C bodies, named by BOTH spec rows. The builder installs a method table per` |
|     - |   674 | ` * class anyway, so "replaying the method table" is a second row and nothing else, and the` |
|     - |   675 | ` * php-visible shape stays exact (a native abstract BASE would have given both classes a` |
|     - |   676 | ` * parent php does not have).` |
|     - |   677 | ` *` |
|     - |   678 | ` * The store itself stays a plain PHP array in a declared private slot, exactly as the trait` |
|     - |   679 | `` * had it: nothing here is a C handle, so `clone` and `serialize()` keep working as php's do`` |
|     - |   680 | ` * and neither class wants the NOCLONE/NOSERIALIZE flags an engine-state class needs. The` |
|     - |   681 | ` * bodies delegate to the engine's OWN array builtins (asort, ksort, uasort, reset, current,` |
|     - |   682 | ` * next, key), which is what the PHP did — one layer down, with no dispatcher round trip.` |
|     - |   683 | ` */` |
|     - |   684 | `#define SPL_D  "__d"  /* the stored array */` |
|     - |   685 | `#define SPL_F  "__f"  /* the flags word */` |
|     - |   686 | `#define SPL_IT "__it" /* ArrayObject's iterator class name */` |
|     - |   687 | `/*` |
|     - |   688 | `` * php's `~SPL_ARRAY_INT_MASK`: the flags word keeps only its low 16 bits, so`` |
|     - |   689 | `` * `setFlags(-1)` then `getFlags()` answers 65535 rather than -1. php masks on the`` |
|     - |   690 | ` * WRITE, which is why every reader — getFlags(), __serialize(), the ARRAY_AS_PROPS` |
|     - |   691 | ` * test — sees the same masked value without asking.` |
|     - |   692 | ` */` |
|     - |   693 | `#define SPL_FLAG_MASK 0xFFFF` |
|     - |   694 | `/* php's SPL_ARRAY_STD_PROP_LIST: the non-debug presentation surfaces answer the` |
|     - |   695 | ` * ordinary property table instead of the storage. */` |
|     - |   696 | `#define SPL_STD_PROP_LIST 0x0001` |
|     - |   697 | `/*` |
|     - |   698 | ` * The instance's storage slot, separated for writing (every caller may mutate it). Answers` |
|     - |   699 | ` * the SLOT rather than the hashmap because that is what the array builtins below take.` |
|     - |   700 | ` *` |
|     - |   701 | `` * The slot is a memobj POOL entry (an instance attribute is `aMemObj[nIdx]`), and the`` |
|     - |   702 | ` * copy-on-write separation reserves one memobj per copied element. That used to grow,` |
|     - |   703 | ` * and therefore MOVE, the pool: holding the pointer across it left every caller reading` |
|     - |   704 | ` * a freed buffer -- harmless while the pool happened not to move, and a hard SIGSEGV` |
|     - |   705 | ` * once a program allocated enough for the mapping to be relocated (twig's suite, at a` |
|     - |   706 | ` * million live slots). Since P1 the pool's segments are fixed and the address is stable,` |
|     - |   707 | ` * so re-asking is no longer load-bearing for THAT reason; it is kept because the lookup` |
|     - |   708 | ` * goes through the attribute by NAME, which is also what survives the property being` |
|     - |   709 | ` * unset and re-created underneath (VmRecreateDeclaredAttr).` |
|     - |   710 | ` */` |
|  9226 |   711 | `static ph7_value * SplStoreSlot(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     4 |   712 | `{` |
|  9230 |   713 | `	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,SPL_D) : 0;` |
|  9230 |   714 | `	if( pSlot == 0 ){` |
|   ! 0 |   715 | `		return 0;` |
|     - |   716 | `	}` |
|  9230 |   717 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 |   718 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|   ! 0 |   719 | `			return 0;` |
|     - |   720 | `		}` |
|   ! 0 |   721 | `	}` |
|  9230 |   722 | `	if( PH7_HashmapCowSeparate(pVm,pSlot) == 0 ){` |
|   ! 0 |   723 | `		return 0;` |
|     - |   724 | `	}` |
|  9230 |   725 | `	return PH7_NativeAttr(pThis,SPL_D);` |
|  4617 |   726 | `}` |
|  5452 |   727 | `static ph7_hashmap * SplStore(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     4 |   728 | `{` |
|  5456 |   729 | `	ph7_value *pSlot = SplStoreSlot(pVm,pThis);` |
|  5456 |   730 | `	return pSlot ? (ph7_hashmap *)pSlot->x.pOther : 0;` |
|     4 |   731 | `}` |
|     - |   732 | `/*` |
|     - |   733 | `` * php's `spl_array_read_dimension` / `zend_weakmap_read_dimension` FAST PATH: a fetch on`` |
|     - |   734 | ` * one of these containers answers with the store's OWN element, not with a copy of it.` |
|     - |   735 | ` * That is the whole difference between a class that supports indirect modification and` |
|     - |   736 | `` * one that does not (PH7_VmDimFetchWritable, oo.c) — `$ao['a']` reached through`` |
|     - |   737 | `` * `offsetGet` is a VALUE however native the method is, so `$r = &$ao['a']`,`` |
|     - |   738 | ``  * `sort($ao['a'])`, `unset($ao['a'][0])`, `foreach ($ao['a'] as &$v)` and `$ao['n']++` `` |
|     - |   739 | ` * all wrote into a temporary and left the store as it was, in silence.` |
|     - |   740 | ` *` |
|     - |   741 | ` * Answers the element's aMemObj index, which the subscript op hands on as the result's` |
|     - |   742 | `` * slot exactly as an array element's `nValIdx` is handed on. SXU32_HIGH means "not`` |
|     - |   743 | `` * available" and the caller falls back to the ordinary `offsetGet` dispatch, which is`` |
|     - |   744 | ` * what keeps every diagnostic (WeakMap's not-contained Error, the store's own` |
|     - |   745 | `` * `Undefined array key`) in the one place that already words it.`` |
|     - |   746 | ` *` |
|     - |   747 | ` * bCreate is php's write-context vivification: a missing ArrayObject/ArrayIterator key` |
|     - |   748 | `` * IS created by a W fetch (`$ao['new']['k'] = 1` works there), while a WeakMap never`` |
|     - |   749 | ` * creates one — its missing key is an Error, raised by the accessor below.` |
|     - |   750 | ` */` |
|   154 |   751 | `PH7_PRIVATE sxu32 PH7_SplDimElemSlot(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pKey,int bCreate)` |
|     4 |   752 | `{` |
|   158 |   753 | `	ph7_hashmap_node *pNode = 0;` |
|   158 |   754 | `	if( pThis == 0 \|\| pKey == 0 ){` |
|   ! 0 |   755 | `		return SXU32_HIGH;` |
|     - |   756 | `	}` |
|   158 |   757 | `	if( pKey->iFlags & MEMOBJ_NULL ){` |
|     - |   758 | `		/* PH7_HashmapLookup folds a NULL key to "" IN PLACE, and a declined fast path` |
|     - |   759 | `		 * has to hand the accessor the key it was written with (php deprecates the null` |
|     - |   760 | `		 * offset there). Cheaper to stand down than to probe on a copy. */` |
|     9 |   761 | `		return SXU32_HIGH;` |
|     - |   762 | `	}` |
|   146 |   763 | `	if( (pKey->iFlags & MEMOBJ_RES)` |
|   146 |   764 | `	 \|\| ((pKey->iFlags & MEMOBJ_REAL) && pKey->rVal != (ph7_real)(sxi64)pKey->rVal) ){` |
|     - |   765 | `		/* Same reason, for the two keys the accessor answers with a DIAGNOSTIC: a` |
|     - |   766 | `		 * resource is php's warning plus its integer id, and a lossy float is §10's` |
|     - |   767 | `		 * refusal. The raw lookup here would string-cast the resource to` |
|     - |   768 | `		 * "Resource id #N" -- a key nothing else writes -- and silently truncate the` |
|     - |   769 | `		 * float, both without a word. */` |
|    12 |   770 | `		return SXU32_HIGH;` |
|     - |   771 | `	}` |
|   138 |   772 | `	if( PH7_NativeAttr(pThis,SPL_D) != 0 ){` |
|     - |   773 | `		ph7_value *pSlot;` |
|   109 |   774 | `		if( pKey->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP) ){` |
|     - |   775 | `			/* Neither shape HAS an array key here: the lookup would fold both to the` |
|     - |   776 | `			 * words "Object"/"Array" and hand back a slot nobody can name again. php` |
|     - |   777 | `			 * refuses them, so stand down and let the accessor -- which words that` |
|     - |   778 | `			 * refusal -- see the key as it was written. (A WeakMap's key IS an object` |
|     - |   779 | `			 * and takes the branch below.) */` |
|     3 |   780 | `			return SXU32_HIGH;` |
|     - |   781 | `		}` |
|   107 |   782 | `		pSlot = SplStoreSlot(pVm,pThis);` |
|   107 |   783 | `		ph7_hashmap *pMap = pSlot ? (ph7_hashmap *)pSlot->x.pOther : 0;` |
|   107 |   784 | `		if( pMap == 0 ){` |
|   ! 0 |   785 | `			return SXU32_HIGH;` |
|     - |   786 | `		}` |
|   107 |   787 | `		if( PH7_HashmapLookup(pMap,pKey,&pNode) != SXRET_OK ){` |
|    11 |   788 | `			if( !bCreate \|\| PH7_HashmapInsert(pMap,pKey,0) != SXRET_OK ){` |
|     3 |   789 | `				return SXU32_HIGH;` |
|     - |   790 | `			}` |
|     9 |   791 | `			pNode = pMap->pLast;` |
|     4 |   792 | `		}` |
|   105 |   793 | `		return pNode ? pNode->nValIdx : SXU32_HIGH;` |
|     - |   794 | `	}` |
|    30 |   795 | `	if( PH7_NativeAttr(pThis,WM_VALS) != 0 && (pKey->iFlags & MEMOBJ_OBJ) ){` |
|    28 |   796 | `		ph7_class_instance *pObj = (ph7_class_instance *)pKey->x.pOther;` |
|    28 |   797 | `		sxi64 iId = (sxi64)pObj->nObjId;` |
|    28 |   798 | `		if( WmNodeTarget(WmFind(pVm,WmStore(pVm,pThis,WM_REFS),iId)) != pObj ){` |
|     5 |   799 | `			return SXU32_HIGH; /* not contained: offsetGet raises php's Error */` |
|     - |   800 | `		}` |
|    24 |   801 | `		pNode = WmFind(pVm,WmStore(pVm,pThis,WM_VALS),iId);` |
|    24 |   802 | `		return pNode ? pNode->nValIdx : SXU32_HIGH;` |
|     - |   803 | `	}` |
|     3 |   804 | `	return SXU32_HIGH;` |
|    81 |   805 | `}` |
|     - |   806 | `/*` |
|     - |   807 | `` * `$this->__d = $array` for the constructor and exchangeArray(), with php's refusal.`` |
|     - |   808 | ` *` |
|     - |   809 | `` * php DECLARES `object\|array $array` — which is what Reflection prints — and then words the`` |
|     - |   810 | `` * refusal as `must be of type array`, so the shared ZPP screen cannot say both (rule 41's`` |
|     - |   811 | ` * shape) and the check is written here. An OBJECT contributes its properties, as the PHP` |
|     - |   812 | ` * did through get_object_vars().` |
|     - |   813 | ` */` |
|  1508 |   814 | `static sxi32 SplInitStore(ph7_context *pCtx,ph7_class_instance *pThis,` |
|     - |   815 | `	ph7_value *pArray,const char *zOwner)` |
|     4 |   816 | `{` |
|     - |   817 | `	char zGiven[64];` |
|  1512 |   818 | `	ph7_vm *pVm = pCtx->pVm;` |
|  1512 |   819 | `	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,SPL_D) : 0;` |
|  1512 |   820 | `	if( pSlot == 0 ){` |
|   ! 0 |   821 | `		return PH7_OK;` |
|     - |   822 | `	}` |
|  1512 |   823 | `	if( pArray == 0 ){` |
|     - |   824 | ``		/* No argument at all: php's `$array = []` default. A native method has no compiled`` |
|     - |   825 | `		 * parameter records for the defaults to live in (rule 33's neighbour), so the body` |
|     - |   826 | `		 * applies it — and an EXPLICIT null still has to reach the refusal below, which is` |
|     - |   827 | `		 * why the two cases are distinguished here rather than by a NULL check. */` |
|   175 |   828 | `		ph7_hashmap *pEmpty = PH7_NewHashmap(pVm,0,0);` |
|   175 |   829 | `		if( pEmpty == 0 ){` |
|   ! 0 |   830 | `			return PH7_ContextMemoryError(pCtx);` |
|     - |   831 | `		}` |
|   175 |   832 | `		PH7_MemObjRelease(pSlot);` |
|   175 |   833 | `		pSlot->x.pOther = pEmpty;` |
|   175 |   834 | `		MemObjSetType(pSlot,MEMOBJ_HASHMAP);` |
|   175 |   835 | `		return PH7_OK;` |
|     - |   836 | `	}` |
|  1340 |   837 | `	if( pArray->iFlags & MEMOBJ_HASHMAP ){` |
|  1324 |   838 | `		PH7_MemObjRelease(pSlot);` |
|  1324 |   839 | `		PH7_MemObjStore(pArray,pSlot); /* a copy: the store is the object's own */` |
|  1324 |   840 | `		return PH7_OK;` |
|     - |   841 | `	}` |
|    17 |   842 | `	if( pArray->iFlags & MEMOBJ_OBJ ){` |
|     - |   843 | `		/* The PHP read get_object_vars($array): the properties this scope can see, by` |
|     - |   844 | `		 * their plain names. php itself keeps the OBJECT and reads its property table` |
|     - |   845 | `		 * live (so getArrayCopy() answers the mangled private names and count() answers` |
|     - |   846 | `		 * the visible ones) — a divergence this conversion carries over unchanged rather` |
|     - |   847 | `		 * than widening, recorded in §7.4. */` |
|     5 |   848 | `		ph7_class_instance *pObj = (ph7_class_instance *)pArray->x.pOther;` |
|     - |   849 | `		ph7_hashmap *pMap;` |
|     - |   850 | `		SyHashEntry *pEntry;` |
|     5 |   851 | `		PH7_MemObjRelease(pSlot);` |
|     5 |   852 | `		pMap = PH7_NewHashmap(pVm,0,0);` |
|     5 |   853 | `		if( pMap == 0 ){` |
|   ! 0 |   854 | `			return PH7_ContextMemoryError(pCtx);` |
|     - |   855 | `		}` |
|     5 |   856 | `		SyHashResetLoopCursor(&pObj->hAttr);` |
|    13 |   857 | `		while( pObj && (pEntry = SyHashGetNextEntry(&pObj->hAttr)) != 0 ){` |
|     9 |   858 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     - |   859 | `			ph7_value sKey;` |
|     - |   860 | `			ph7_value *pVal;` |
|     9 |   861 | `			if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|   ! 0 |   862 | `				continue;` |
|     - |   863 | `			}` |
|     9 |   864 | `			if( pVmAttr->pAttr->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|   ! 0 |   865 | `				continue;` |
|     - |   866 | `			}` |
|     9 |   867 | `			pVal = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pVmAttr->nIdx);` |
|     9 |   868 | `			if( pVal == 0 ){` |
|   ! 0 |   869 | `				continue;` |
|     - |   870 | `			}` |
|     9 |   871 | `			PH7_MemObjInitFromString(pVm,&sKey,&pVmAttr->pAttr->sName);` |
|     9 |   872 | `			if( PH7_ClassAttrIsRef(pObj,pVmAttr) ){` |
|     - |   873 | `				/* A property that IS a reference is handed out AS one, the way every` |
|     - |   874 | `				 * other array built out of a property table hands it out. */` |
|   ! 0 |   875 | `				PH7_HashmapInsertByRef(pMap,&sKey,pVmAttr->nIdx);` |
|   ! 0 |   876 | `			}else{` |
|     9 |   877 | `				PH7_HashmapInsert(pMap,&sKey,pVal);` |
|     - |   878 | `			}` |
|     9 |   879 | `			PH7_MemObjRelease(&sKey);` |
|     1 |   880 | `		}` |
|     5 |   881 | `		pSlot->x.pOther = pMap;` |
|     5 |   882 | `		MemObjSetType(pSlot,MEMOBJ_HASHMAP);` |
|     5 |   883 | `		return PH7_OK;` |
|     - |   884 | `	}` |
|    19 |   885 | `	return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |   886 | `		"%s(): Argument #1 ($array) must be of type array, %s given",` |
|     6 |   887 | `		zOwner,VmValueGivenName(pArray,zGiven,sizeof(zGiven)));` |
|   758 |   888 | `}` |
|     - |   889 | `/* Hand one of the engine's own array builtins the instance's storage slot. */` |
|  3464 |   890 | `static int SplArrayCall(ph7_context *pCtx,ProchHostFunction xFunc,ph7_value *pExtra)` |
|     4 |   891 | `{` |
|  3468 |   892 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |   893 | `	ph7_value *apCall[2];` |
|  3468 |   894 | `	ph7_value *pSlot = SplStoreSlot(pCtx->pVm,pThis);` |
|  3468 |   895 | `	if( pSlot == 0 ){` |
|   ! 0 |   896 | `		return PH7_OK;` |
|     - |   897 | `	}` |
|  3468 |   898 | `	apCall[0] = pSlot;` |
|  3468 |   899 | `	apCall[1] = pExtra;` |
|  3468 |   900 | `	return xFunc(pCtx,pExtra ? 2 : 1,apCall);` |
|  1736 |   901 | `}` |
|     - |   902 | `/*` |
|     - |   903 | ` * The ARRAY-OFFSET rules for the store's four offset methods. php's ArrayObject /` |
|     - |   904 | `` * ArrayIterator hand the key to the same machinery `$a[$k]` uses, so all five of`` |
|     - |   905 | ` * the engine's answers belong here and not only the two refusals:` |
|     - |   906 | ` *` |
|     - |   907 | ` *   object / array   refused — no key exists for either. PHL used to fold them to` |
|     - |   908 | `` *                    the strings "Object"/"Array", so `$ao[$obj] = 1` wrote under`` |
|     - |   909 | ` *                    a key no reader could ask for.` |
|     - |   910 | ``  *   RESOURCE         php's `Resource ID#N used as offset, casting to integer (N)` `` |
|     - |   911 | ` *                    warning, then the id IS the key. The string cast had been` |
|     - |   912 | ` *                    keying it "Resource id #N" — a key php never writes, and one` |
|     - |   913 | `` *                    `$ao[$res]` could then read back while `$a[$res]` could not.`` |
|     - |   914 | `` *   NULL             php's `Using null as an array offset is deprecated` and the`` |
|     - |   915 | ` *                    "" key — except in offsetSet(), where a null key is php's` |
|     - |   916 | `` *                    append form (`$ao[] = $v`) and says nothing.`` |
|     - |   917 | ` *   LOSSY FLOAT      §10: php deprecates and truncates, PHL refuses — but only` |
|     - |   918 | ` *                    where the ENGINE refuses it, which is a READ or a WRITE.` |
|     - |   919 | `` *                    `isset($a[1.9])` and `unset($a[1.9])` truncate quietly on a`` |
|     - |   920 | ` *                    plain array, so they do here, or the store would answer` |
|     - |   921 | ` *                    differently from the array it IS.` |
|     - |   922 | ` *` |
|     - |   923 | ` * The refusal wording is the ENGINE's own three-way split (vm_ops_load.c): a read` |
|     - |   924 | ` * or a write names the receiver's class, isset/empty names none, and unset says` |
|     - |   925 | ` * "Cannot unset". offsetExists() reached by hand is php's isset arm too.` |
|     - |   926 | ` *` |
|     - |   927 | ` * pKey is the method's own argument copy (a native method never aliases the` |
|     - |   928 | ` * caller's variable), so the resource rewrite is in place. Returns 1 when the key` |
|     - |   929 | ` * is refused and *pRc carries the throw's status; 0 when the key is usable, with` |
|     - |   930 | ` * the two non-refusing rules already applied to it.` |
|     - |   931 | ` */` |
|     - |   932 | `#define SPL_OFF_ACCESS 0` |
|     - |   933 | `#define SPL_OFF_ISSET  1` |
|     - |   934 | `#define SPL_OFF_UNSET  2` |
|     - |   935 | `#define SPL_OFF_SET    3   /* offsetSet: a NULL key is the append form, not a key */` |
|   318 |   936 | `static int SplOffsetKeyArg(ph7_context *pCtx,ph7_value *pKey,int iKind,int *pRc)` |
|     3 |   937 | `{` |
|   321 |   938 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   321 |   939 | `	SyString *pOwner = pThis ? &pThis->pClass->sName : 0;` |
|   321 |   940 | `	SyString *pClass = 0;` |
|   321 |   941 | `	const char *zType = 0;` |
|   321 |   942 | `	*pRc = PH7_OK;` |
|   321 |   943 | `	if( pKey->iFlags & MEMOBJ_OBJ ){` |
|    62 |   944 | `		ph7_class_instance *pInst = (ph7_class_instance *)pKey->x.pOther;` |
|    62 |   945 | `		if( pInst && pInst->pClass ){` |
|    62 |   946 | `			pClass = &pInst->pClass->sName;` |
|    30 |   947 | `		}` |
|    62 |   948 | `		zType = "object";` |
|   291 |   949 | `	}else if( pKey->iFlags & MEMOBJ_HASHMAP ){` |
|    60 |   950 | `		zType = "array";` |
|   231 |   951 | `	}else if( (pKey->iFlags & MEMOBJ_REAL)` |
|   110 |   952 | `	       && pKey->rVal != (ph7_real)(sxi64)pKey->rVal` |
|    21 |   953 | `	       && (iKind == SPL_OFF_ACCESS \|\| iKind == SPL_OFF_SET) ){` |
|    11 |   954 | `		zType = "float";` |
|     5 |   955 | `	}` |
|   321 |   956 | `	if( zType == 0 ){` |
|   193 |   957 | `		PH7_VmOffsetResourceWarn(pCtx->pVm,pKey);` |
|   193 |   958 | `		if( iKind != SPL_OFF_SET ){` |
|   117 |   959 | `			PH7_VmNullOffsetDeprecate(pCtx->pVm,pKey);` |
|    57 |   960 | `		}` |
|   193 |   961 | `		return 0;` |
|     - |   962 | `	}` |
|   131 |   963 | `	if( iKind == SPL_OFF_ISSET ){` |
|    46 |   964 | `		*pRc = pClass` |
|    39 |   965 | `			? PH7_VmThrowException(pCtx,"TypeError",` |
|    13 |   966 | `				"Cannot access offset of type %z in isset or empty",pClass)` |
|    40 |   967 | `			: PH7_VmThrowException(pCtx,"TypeError",` |
|     9 |   968 | `				"Cannot access offset of type %s in isset or empty",zType);` |
|    24 |   969 | `	}else{` |
|    87 |   970 | `		const char *zVerb = iKind == SPL_OFF_UNSET ? "Cannot unset" : "Cannot access";` |
|    87 |   971 | `		*pRc = pClass` |
|    51 |   972 | `			? PH7_VmThrowException(pCtx,"TypeError",` |
|    17 |   973 | `				"%s offset of type %z on %z",zVerb,pClass,pOwner)` |
|    92 |   974 | `			: PH7_VmThrowException(pCtx,"TypeError",` |
|    25 |   975 | `				"%s offset of type %s on %z",zVerb,zType,pOwner);` |
|     - |   976 | `	}` |
|   131 |   977 | `	return 1;` |
|   162 |   978 | `}` |
|    80 |   979 | `static int vm_builtin_SplStore_offsetExists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |   980 | `{` |
|    83 |   981 | `	ph7_hashmap *pMap = SplStore(pCtx->pVm,PH7_ContextThis(pCtx));` |
|    83 |   982 | `	ph7_hashmap_node *pNode = 0;` |
|    83 |   983 | `	int bFound = 0, rc;` |
|    83 |   984 | `	if( nArg > 0 && SplOffsetKeyArg(pCtx,apArg[0],SPL_OFF_ISSET,&rc) ){` |
|    46 |   985 | `		return rc;` |
|     - |   986 | `	}` |
|    39 |   987 | `	if( pMap && nArg > 0 ){` |
|     - |   988 | `		/* array_key_exists(), not isset(): php's offsetExists() answers true for a key` |
|     - |   989 | `		 * holding NULL (the PHP said array_key_exists too). */` |
|    39 |   990 | `		bFound = PH7_HashmapLookup(pMap,apArg[0],&pNode) == SXRET_OK;` |
|    18 |   991 | `	}` |
|    39 |   992 | `	ph7_result_bool(pCtx,bFound);` |
|    39 |   993 | `	return PH7_OK;` |
|    43 |   994 | `}` |
|    86 |   995 | `static int vm_builtin_SplStore_offsetGet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |   996 | `{` |
|    89 |   997 | `	ph7_vm *pVm = pCtx->pVm;` |
|    89 |   998 | `	ph7_hashmap *pMap = SplStore(pVm,PH7_ContextThis(pCtx));` |
|    89 |   999 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  1000 | `	int rcKey;` |
|    89 |  1001 | `	if( nArg > 0 && SplOffsetKeyArg(pCtx,apArg[0],SPL_OFF_ACCESS,&rcKey) ){` |
|    34 |  1002 | `		return rcKey;` |
|     - |  1003 | `	}` |
|    56 |  1004 | `	if( pMap == 0 \|\| nArg < 1 \|\| PH7_HashmapLookup(pMap,apArg[0],&pNode) != SXRET_OK ){` |
|     - |  1005 | `		/* php warns "Undefined array key" for a missing offset, with the key rendered` |
|     - |  1006 | `		 * the way the LOOKUP folded it (an integer bare, a string quoted) — the same` |
|     - |  1007 | `		 * pair OP_LOAD_IDX prints. This one now reports the CALLER's line, where the` |
|     - |  1008 | `		 * PHP reported the chunk's. */` |
|     3 |  1009 | `		if( nArg > 0 ){` |
|     - |  1010 | `			SyBlob sMsg;` |
|     3 |  1011 | `			SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     3 |  1012 | `			if( PH7_HashmapKeyIsInt(apArg[0]) ){` |
|   ! 0 |  1013 | `				if( (apArg[0]->iFlags & MEMOBJ_INT) == 0 ){` |
|   ! 0 |  1014 | `					PH7_MemObjToInteger(apArg[0]);` |
|   ! 0 |  1015 | `				}` |
|   ! 0 |  1016 | `				SyBlobFormat(&sMsg,"Undefined array key %qd",apArg[0]->x.iVal);` |
|   ! 0 |  1017 | `			}else{` |
|     - |  1018 | `				SyString sKey;` |
|     3 |  1019 | `				SyStringInitFromBuf(&sKey,SyBlobData(&apArg[0]->sBlob),` |
|     - |  1020 | `					SyBlobLength(&apArg[0]->sBlob));` |
|     3 |  1021 | `				SyBlobFormat(&sMsg,"Undefined array key \"%z\"",&sKey);` |
|     - |  1022 | `			}` |
|     3 |  1023 | `			SyBlobNullAppend(&sMsg);` |
|     3 |  1024 | `			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));` |
|     3 |  1025 | `			SyBlobRelease(&sMsg);` |
|     1 |  1026 | `		}` |
|     3 |  1027 | `		ph7_result_null(pCtx);` |
|     3 |  1028 | `		return PH7_OK;` |
|     - |  1029 | `	}` |
|    54 |  1030 | `	ph7_result_value(pCtx,(ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pNode->nValIdx));` |
|    54 |  1031 | `	return PH7_OK;` |
|    46 |  1032 | `}` |
|     - |  1033 | `/*` |
|     - |  1034 | ` * Insert into the store, keeping php's cursor rule.` |
|     - |  1035 | ` *` |
|     - |  1036 | ` * php's ArrayIterator position is an INTEGER index into the bucket array, so a` |
|     - |  1037 | ` * cursor that ran off the end sits AT the element count: inserting a new key there` |
|     - |  1038 | ` * makes it valid again and the iterator RESUMES on the element just added. PHL` |
|     - |  1039 | ` * carries a node POINTER, which is null past the end and loses that. Re-point it` |
|     - |  1040 | ` * here -- the only place the difference shows, since overwriting an EXISTING key` |
|     - |  1041 | ` * inserts no node and php's dead cursor stays dead. AppendIterator depends on this:` |
|     - |  1042 | ` * php's append() after exhaustion is what makes the walk continue.` |
|     - |  1043 | ` */` |
|   334 |  1044 | `static void SplStoreInsert(ph7_hashmap *pMap,ph7_value *pKey,ph7_value *pVal)` |
|     3 |  1045 | `{` |
|   337 |  1046 | `	sxu32 nBefore = pMap->nEntry;` |
|   337 |  1047 | `	int bPastEnd = pMap->pCur == 0;` |
|   337 |  1048 | `	PH7_HashmapInsert(pMap,pKey,pVal);` |
|   337 |  1049 | `	if( bPastEnd && pMap->nEntry > nBefore ){` |
|   199 |  1050 | `		pMap->pCur = pMap->pLast;` |
|    98 |  1051 | `	}` |
|   337 |  1052 | `}` |
|   108 |  1053 | `static int vm_builtin_SplStore_offsetSet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  1054 | `{` |
|   111 |  1055 | `	ph7_hashmap *pMap = SplStore(pCtx->pVm,PH7_ContextThis(pCtx));` |
|     - |  1056 | `	int rcKey;` |
|   111 |  1057 | `	if( nArg > 0 && SplOffsetKeyArg(pCtx,apArg[0],SPL_OFF_SET,&rcKey) ){` |
|    35 |  1058 | `		return rcKey;` |
|     - |  1059 | `	}` |
|    79 |  1060 | `	if( pMap && nArg > 1 ){` |
|     - |  1061 | ``		/* A NULL key is `$o[] = $v` — the append form, which is how php's offsetSet()`` |
|     - |  1062 | `		 * receives it. */` |
|    79 |  1063 | `		SplStoreInsert(pMap,(apArg[0]->iFlags & MEMOBJ_NULL) ? 0 : apArg[0],apArg[1]);` |
|    38 |  1064 | `	}` |
|    79 |  1065 | `	return PH7_OK;` |
|    57 |  1066 | `}` |
|    44 |  1067 | `static int vm_builtin_SplStore_offsetUnset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  1068 | `{` |
|    47 |  1069 | `	ph7_hashmap *pMap = SplStore(pCtx->pVm,PH7_ContextThis(pCtx));` |
|    47 |  1070 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  1071 | `	int rcKey;` |
|    47 |  1072 | `	if( nArg > 0 && SplOffsetKeyArg(pCtx,apArg[0],SPL_OFF_UNSET,&rcKey) ){` |
|    22 |  1073 | `		return rcKey;` |
|     - |  1074 | `	}` |
|    27 |  1075 | `	if( pMap && nArg > 0 && PH7_HashmapLookup(pMap,apArg[0],&pNode) == SXRET_OK ){` |
|    27 |  1076 | `		PH7_HashmapUnlinkNode(pNode,TRUE);` |
|    12 |  1077 | `	}` |
|    27 |  1078 | `	return PH7_OK;` |
|    25 |  1079 | `}` |
|     8 |  1080 | `static int vm_builtin_SplStore_append(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1081 | `{` |
|     9 |  1082 | `	ph7_hashmap *pMap = SplStore(pCtx->pVm,PH7_ContextThis(pCtx));` |
|     9 |  1083 | `	if( pMap && nArg > 0 ){` |
|     9 |  1084 | `		SplStoreInsert(pMap,0,apArg[0]);` |
|     4 |  1085 | `	}` |
|     9 |  1086 | `	return PH7_OK;` |
|     1 |  1087 | `}` |
|   126 |  1088 | `static int vm_builtin_SplStore_getArrayCopy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 |  1089 | `{` |
|   130 |  1090 | `	ph7_value *pSlot = SplStoreSlot(pCtx->pVm,PH7_ContextThis(pCtx));` |
|    63 |  1091 | `	SXUNUSED(nArg);` |
|    63 |  1092 | `	SXUNUSED(apArg);` |
|   130 |  1093 | `	if( pSlot ){` |
|   130 |  1094 | `		ph7_result_value(pCtx,pSlot); /* a COPY: the caller must not alias the store */` |
|    63 |  1095 | `	}` |
|   130 |  1096 | `	return PH7_OK;` |
|     4 |  1097 | `}` |
|    28 |  1098 | `static int vm_builtin_SplStore_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1099 | `{` |
|    29 |  1100 | `	ph7_hashmap *pMap = SplStore(pCtx->pVm,PH7_ContextThis(pCtx));` |
|    14 |  1101 | `	SXUNUSED(nArg);` |
|    14 |  1102 | `	SXUNUSED(apArg);` |
|    29 |  1103 | `	ph7_result_int64(pCtx,pMap ? (ph7_int64)pMap->nEntry : 0);` |
|    29 |  1104 | `	return PH7_OK;` |
|     1 |  1105 | `}` |
|    16 |  1106 | `static int vm_builtin_SplStore_getFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1107 | `{` |
|    17 |  1108 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     8 |  1109 | `	SXUNUSED(nArg);` |
|     8 |  1110 | `	SXUNUSED(apArg);` |
|    17 |  1111 | `	ph7_result_int64(pCtx,pThis ? PH7_NativeAttrInt(pThis,SPL_F) : 0);` |
|    17 |  1112 | `	return PH7_OK;` |
|     1 |  1113 | `}` |
|     2 |  1114 | `static int vm_builtin_SplStore_setFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1115 | `{` |
|     3 |  1116 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     3 |  1117 | `	if( pThis && nArg > 0 ){` |
|     4 |  1118 | `		PH7_NativeSetAttrInt(pCtx->pVm,pThis,SPL_F,` |
|     2 |  1119 | `			ph7_value_to_int64(apArg[0]) & SPL_FLAG_MASK);` |
|     1 |  1120 | `	}` |
|     3 |  1121 | `	return PH7_OK;` |
|     1 |  1122 | `}` |
|     - |  1123 | `/*` |
|     - |  1124 | ` * The six sorts. Each is the engine's own builtin over the stored array — including the` |
|     - |  1125 | `` * `$flags` the PHP DROPPED on the floor (`asort($this->__d)` ignored its own parameter, so`` |
|     - |  1126 | `` * `$it->asort(SORT_STRING)` sorted numerically). natsort/natcasesort go through asort with`` |
|     - |  1127 | ` * php's own flag pair rather than by name: the shared body reads ph7_function_name() to tell` |
|     - |  1128 | `` * the two apart, and a native method's name is `ArrayIterator::natcasesort`.`` |
|     - |  1129 | ` */` |
|     6 |  1130 | `static int vm_builtin_SplStore_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1131 | `{` |
|     7 |  1132 | `	return SplArrayCall(pCtx,ph7_hashmap_asort,nArg > 0 ? apArg[0] : 0);` |
|     1 |  1133 | `}` |
|     6 |  1134 | `static int vm_builtin_SplStore_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1135 | `{` |
|     7 |  1136 | `	return SplArrayCall(pCtx,ph7_hashmap_ksort,nArg > 0 ? apArg[0] : 0);` |
|     1 |  1137 | `}` |
|     4 |  1138 | `static int vm_builtin_SplStore_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1139 | `{` |
|     5 |  1140 | `	return SplArrayCall(pCtx,ph7_hashmap_uasort,nArg > 0 ? apArg[0] : 0);` |
|     1 |  1141 | `}` |
|     2 |  1142 | `static int vm_builtin_SplStore_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1143 | `{` |
|     3 |  1144 | `	return SplArrayCall(pCtx,ph7_hashmap_uksort,nArg > 0 ? apArg[0] : 0);` |
|     1 |  1145 | `}` |
|    12 |  1146 | `static int SplNatSort(ph7_context *pCtx,int bFold)` |
|     3 |  1147 | `{` |
|     - |  1148 | `	ph7_value sFlags;` |
|     - |  1149 | `	int rc;` |
|     - |  1150 | `	/* SORT_NATURAL (6), plus SORT_FLAG_CASE (8) for the folding twin — the same pair` |
|     - |  1151 | `	 * ph7_hashmap_natsort forwards to asort(). */` |
|    15 |  1152 | `	PH7_MemObjInitFromInt(pCtx->pVm,&sFlags,bFold ? (6\|8) : 6);` |
|    15 |  1153 | `	rc = SplArrayCall(pCtx,ph7_hashmap_asort,&sFlags);` |
|    15 |  1154 | `	PH7_MemObjRelease(&sFlags);` |
|    15 |  1155 | `	return rc;` |
|     3 |  1156 | `}` |
|     8 |  1157 | `static int vm_builtin_SplStore_natsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  1158 | `{` |
|     4 |  1159 | `	SXUNUSED(nArg);` |
|     4 |  1160 | `	SXUNUSED(apArg);` |
|    11 |  1161 | `	return SplNatSort(pCtx,0);` |
|     3 |  1162 | `}` |
|     4 |  1163 | `static int vm_builtin_SplStore_natcasesort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  1164 | `{` |
|     2 |  1165 | `	SXUNUSED(nArg);` |
|     2 |  1166 | `	SXUNUSED(apArg);` |
|     6 |  1167 | `	return SplNatSort(pCtx,1);` |
|     2 |  1168 | `}` |
|     - |  1169 | `/* ArrayIterator's cursor: the stored array's own internal pointer, as the PHP had it. */` |
|  1000 |  1170 | `static int vm_builtin_ArrayIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  1171 | `{` |
|  1003 |  1172 | `	ph7_hashmap *pMap = SplStore(pCtx->pVm,PH7_ContextThis(pCtx));` |
|   500 |  1173 | `	SXUNUSED(nArg);` |
|   500 |  1174 | `	SXUNUSED(apArg);` |
|  1003 |  1175 | `	if( pMap == 0 \|\| pMap->pCur == 0 ){` |
|     - |  1176 | `		/* Past the end php answers NULL, where current() the FUNCTION answers false. */` |
|     7 |  1177 | `		ph7_result_null(pCtx);` |
|     7 |  1178 | `		return PH7_OK;` |
|     - |  1179 | `	}` |
|   997 |  1180 | `	return SplArrayCall(pCtx,ph7_hashmap_current,0);` |
|   503 |  1181 | `}` |
|   954 |  1182 | `static int vm_builtin_ArrayIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  1183 | `{` |
|   477 |  1184 | `	SXUNUSED(nArg);` |
|   477 |  1185 | `	SXUNUSED(apArg);` |
|   957 |  1186 | `	return SplArrayCall(pCtx,ph7_hashmap_simple_key,0);` |
|     3 |  1187 | `}` |
|   900 |  1188 | `static int vm_builtin_ArrayIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  1189 | `{` |
|   450 |  1190 | `	SXUNUSED(nArg);` |
|   450 |  1191 | `	SXUNUSED(apArg);` |
|   903 |  1192 | `	SplArrayCall(pCtx,ph7_hashmap_next,0);` |
|   903 |  1193 | `	ph7_result_null(pCtx); /* next() the METHOD returns void */` |
|   903 |  1194 | `	return PH7_OK;` |
|     3 |  1195 | `}` |
|   586 |  1196 | `static int vm_builtin_ArrayIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  1197 | `{` |
|   293 |  1198 | `	SXUNUSED(nArg);` |
|   293 |  1199 | `	SXUNUSED(apArg);` |
|   589 |  1200 | `	SplArrayCall(pCtx,ph7_hashmap_reset,0);` |
|   589 |  1201 | `	ph7_result_null(pCtx);` |
|   589 |  1202 | `	return PH7_OK;` |
|     3 |  1203 | `}` |
|  1956 |  1204 | `static int vm_builtin_ArrayIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  1205 | `{` |
|  1959 |  1206 | `	ph7_hashmap *pMap = SplStore(pCtx->pVm,PH7_ContextThis(pCtx));` |
|   978 |  1207 | `	SXUNUSED(nArg);` |
|   978 |  1208 | `	SXUNUSED(apArg);` |
|  1959 |  1209 | `	ph7_result_bool(pCtx,pMap && pMap->pCur ? 1 : 0);` |
|  1959 |  1210 | `	return PH7_OK;` |
|     3 |  1211 | `}` |
|    34 |  1212 | `static int vm_builtin_ArrayIterator_seek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1213 | `{` |
|    35 |  1214 | `	ph7_hashmap *pMap = SplStore(pCtx->pVm,PH7_ContextThis(pCtx));` |
|    35 |  1215 | `	ph7_int64 iOffset = nArg > 0 ? ph7_value_to_int(apArg[0]) : 0;` |
|     - |  1216 | `	ph7_int64 i;` |
|    35 |  1217 | `	if( pMap == 0 ){` |
|   ! 0 |  1218 | `		return PH7_OK;` |
|     - |  1219 | `	}` |
|    35 |  1220 | `	if( iOffset < 0 \|\| iOffset >= (ph7_int64)pMap->nEntry ){` |
|    10 |  1221 | `		return PH7_VmThrowException(pCtx,"OutOfBoundsException",` |
|     3 |  1222 | `			"Seek position %qd is out of range",iOffset);` |
|     - |  1223 | `	}` |
|    29 |  1224 | `	pMap->pCur = pMap->pFirst;` |
|    65 |  1225 | `	for( i = 0 ; i < iOffset && pMap->pCur ; ++i ){` |
|    37 |  1226 | `		pMap->pCur = pMap->pCur->pPrev; /* insertion order: pFirst, then the pPrev chain */` |
|    19 |  1227 | `	}` |
|    29 |  1228 | `	return PH7_OK;` |
|    18 |  1229 | `}` |
|  1082 |  1230 | `static int vm_builtin_ArrayIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 |  1231 | `{` |
|  1086 |  1232 | `	ph7_vm *pVm = pCtx->pVm;` |
|  1086 |  1233 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  1234 | `	ph7_hashmap *pMap;` |
|  1086 |  1235 | `	sxi32 rc = SplInitStore(pCtx,pThis,nArg > 0 ? apArg[0] : 0,` |
|     - |  1236 | `		"ArrayIterator::__construct");` |
|  1086 |  1237 | `	if( rc != PH7_OK ){` |
|    11 |  1238 | `		return rc;` |
|     - |  1239 | `	}` |
|  1076 |  1240 | `	if( pThis && nArg > 1 ){` |
|   167 |  1241 | `		PH7_NativeSetAttrInt(pVm,pThis,SPL_F,ph7_value_to_int64(apArg[1]) & SPL_FLAG_MASK);` |
|    83 |  1242 | `	}` |
|  1076 |  1243 | `	pMap = SplStore(pVm,pThis);` |
|  1076 |  1244 | `	if( pMap ){` |
|  1076 |  1245 | `		pMap->pCur = pMap->pFirst; /* reset($this->__d) */` |
|   536 |  1246 | `	}` |
|  1076 |  1247 | `	return PH7_OK;` |
|   545 |  1248 | `}` |
|     - |  1249 | `/* ArrayObject */` |
|     - |  1250 | `/*` |
|     - |  1251 | `` * The iterator class, taken by two doors: `setIteratorClass()` and the`` |
|     - |  1252 | ` * constructor's third argument. php refuses the same names at both but words` |
|     - |  1253 | ` * the refusal from the door it came in by, so the method NAME and the argument` |
|     - |  1254 | ` * POSITION travel with the check -- PHL reported the constructor's refusal as` |
|     - |  1255 | ` * setIteratorClass()'s Argument #1.` |
|     - |  1256 | ` *` |
|     - |  1257 | ` * The cast is php's user-visible one (zend's class-name parameter): an array` |
|     - |  1258 | `` * warns `Array to string conversion` and is refused as the name "Array", and an`` |
|     - |  1259 | `` * object with no __toString() is the catchable `could not be converted to`` |
|     - |  1260 | `` * string` Error rather than a refusal naming the placeholder "Object".`` |
|     - |  1261 | ` */` |
|    42 |  1262 | `static int SplSetIteratorClass(ph7_context *pCtx,ph7_value *pArg,const char *zWhere)` |
|     1 |  1263 | `{` |
|    43 |  1264 | `	ph7_vm *pVm = pCtx->pVm;` |
|    43 |  1265 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  1266 | `	const char *zName;` |
|     - |  1267 | `	int nName;` |
|     - |  1268 | `	sxi32 rcSv;` |
|    43 |  1269 | `	if( pThis == 0 \|\| pArg == 0 ){` |
|   ! 0 |  1270 | `		return PH7_OK;` |
|     - |  1271 | `	}` |
|    43 |  1272 | `	rcSv = PH7_ValueToStringUV(pCtx,pArg,&zName,&nName);` |
|    43 |  1273 | `	if( rcSv != SXRET_OK ){` |
|     5 |  1274 | `		return rcSv;` |
|     - |  1275 | `	}` |
|    38 |  1276 | `	if( nName != (int)sizeof("ArrayIterator")-1` |
|    20 |  1277 | `	 \|\| SyMemcmp(zName,"ArrayIterator",sizeof("ArrayIterator")-1) != 0 ){` |
|    39 |  1278 | `		ph7_class *pClass = PH7_VmExtractClass(pVm,zName,(sxu32)nName,FALSE,0);` |
|    39 |  1279 | `		ph7_class *pBase = PH7_VmExtractClass(pVm,"ArrayIterator",` |
|     - |  1280 | `			sizeof("ArrayIterator")-1,FALSE,0);` |
|    38 |  1281 | `		if( pClass == 0 \|\| pBase == 0 \|\| pClass == pBase` |
|    25 |  1282 | `		 \|\| !PH7_VmInstanceOf(pClass,pBase) ){` |
|    43 |  1283 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  1284 | `				"%s must be a class name derived from ArrayIterator, %.*s given",` |
|    14 |  1285 | `				zWhere,nName,zName);` |
|     - |  1286 | `		}` |
|     5 |  1287 | `	}` |
|    11 |  1288 | `	PH7_NativeSetAttrStr(pVm,pThis,SPL_IT,zName,nName);` |
|    11 |  1289 | `	return PH7_OK;` |
|    22 |  1290 | `}` |
|    24 |  1291 | `static int vm_builtin_ArrayObject_setIteratorClass(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1292 | `{` |
|    25 |  1293 | `	return SplSetIteratorClass(pCtx,nArg > 0 ? apArg[0] : 0,` |
|     - |  1294 | `		"ArrayObject::setIteratorClass(): Argument #1 ($iteratorClass)");` |
|     1 |  1295 | `}` |
|   396 |  1296 | `static int vm_builtin_ArrayObject_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 |  1297 | `{` |
|   400 |  1298 | `	ph7_vm *pVm = pCtx->pVm;` |
|   400 |  1299 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   400 |  1300 | `	sxi32 rc = SplInitStore(pCtx,pThis,nArg > 0 ? apArg[0] : 0,` |
|     - |  1301 | `		"ArrayObject::__construct");` |
|   400 |  1302 | `	if( rc != PH7_OK ){` |
|     3 |  1303 | `		return rc;` |
|     - |  1304 | `	}` |
|   398 |  1305 | `	if( pThis && nArg > 1 ){` |
|    39 |  1306 | `		PH7_NativeSetAttrInt(pVm,pThis,SPL_F,ph7_value_to_int64(apArg[1]) & SPL_FLAG_MASK);` |
|    19 |  1307 | `	}` |
|   398 |  1308 | `	if( nArg > 2 ){` |
|    19 |  1309 | `		return SplSetIteratorClass(pCtx,apArg[2],` |
|     - |  1310 | `			"ArrayObject::__construct(): Argument #3 ($iteratorClass)");` |
|     - |  1311 | `	}` |
|   380 |  1312 | `	return PH7_OK;` |
|   202 |  1313 | `}` |
|     4 |  1314 | `static int vm_builtin_ArrayObject_exchangeArray(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1315 | `{` |
|     5 |  1316 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     5 |  1317 | `	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,SPL_D) : 0;` |
|     - |  1318 | `	sxi32 rc;` |
|     5 |  1319 | `	if( pSlot ){` |
|     5 |  1320 | `		ph7_result_value(pCtx,pSlot); /* the OLD store is the return value */` |
|     2 |  1321 | `	}` |
|     5 |  1322 | `	rc = SplInitStore(pCtx,pThis,nArg > 0 ? apArg[0] : 0,"ArrayObject::exchangeArray");` |
|     5 |  1323 | `	return rc;` |
|     1 |  1324 | `}` |
|    10 |  1325 | `static int vm_builtin_ArrayObject_getIteratorClass(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1326 | `{` |
|    11 |  1327 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    11 |  1328 | `	const char *zName = 0;` |
|    11 |  1329 | `	int nName = 0;` |
|     5 |  1330 | `	SXUNUSED(nArg);` |
|     5 |  1331 | `	SXUNUSED(apArg);` |
|    11 |  1332 | `	if( pThis ){` |
|    11 |  1333 | `		PH7_NativeAttrStr(pThis,SPL_IT,&zName,&nName);` |
|     5 |  1334 | `	}` |
|    11 |  1335 | `	ph7_result_string(pCtx,nName > 0 ? zName : "ArrayIterator",nName > 0 ? nName : -1);` |
|    11 |  1336 | `	return PH7_OK;` |
|     1 |  1337 | `}` |
|     - |  1338 | `/*` |
|     - |  1339 | ` * ---------------------------------------------------------------------------` |
|     - |  1340 | ` * php's __serialize()/__unserialize() for the array store.` |
|     - |  1341 | ` *` |
|     - |  1342 | ` * php's payload is a four-element LIST -- [flags, storage, members, iterator class]` |
|     - |  1343 | ` * -- and nothing else can express it: the state lives in ext/spl's own struct, so` |
|     - |  1344 | ` * there are no properties to walk. PHL walked its HIDDEN slots instead and wrote` |
|     - |  1345 | `` * `O:11:"ArrayObject":3:{s:16:"\0ArrayObject\0__d";…}`, which round-tripped inside`` |
|     - |  1346 | ` * PHL and could not read a byte string php produced (nor be read by php).` |
|     - |  1347 | ` *` |
|     - |  1348 | ` * Two details worth keeping. The last element is NULL when the class is the default` |
|     - |  1349 | ` * ArrayIterator, and it is always NULL for an ArrayIterator payload -- php shares one` |
|     - |  1350 | ` * C body between both classes, so ArrayIterator carries the slot it has no use for.` |
|     - |  1351 | ` * And the iterator-class check here is LOOSER than setIteratorClass()'s: restoring` |
|     - |  1352 | `` * accepts any `Iterator`, while the setter and the constructor demand a class derived`` |
|     - |  1353 | `` * from ArrayIterator. php words the refusal with `ArrayObject` either way, even when`` |
|     - |  1354 | ` * ArrayIterator is the receiver.` |
|     - |  1355 | ` * ---------------------------------------------------------------------------` |
|     - |  1356 | ` */` |
|     8 |  1357 | `static sxi32 SplStoreIllTyped(ph7_context *pCtx)` |
|     1 |  1358 | `{` |
|     9 |  1359 | `	return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - |  1360 | `		"Incomplete or ill-typed serialization data");` |
|     1 |  1361 | `}` |
|    26 |  1362 | `static int vm_builtin_SplStore_serializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1363 | `{` |
|    27 |  1364 | `	ph7_vm *pVm = pCtx->pVm;` |
|    27 |  1365 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  1366 | `	ph7_value sOut,sVal,*pStore;` |
|    27 |  1367 | `	const char *zIt = 0;` |
|    27 |  1368 | `	int nIt = 0;` |
|    13 |  1369 | `	SXUNUSED(nArg);` |
|    13 |  1370 | `	SXUNUSED(apArg);` |
|    27 |  1371 | `	PH7_MemObjInit(pVm,&sOut);` |
|    27 |  1372 | `	if( PH7_MemObjToHashmap(&sOut) != SXRET_OK ){` |
|   ! 0 |  1373 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 |  1374 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  1375 | `	}` |
|     - |  1376 | `	/* [0] the flags word */` |
|    27 |  1377 | `	PH7_MemObjInitFromInt(pVm,&sVal,pThis ? PH7_NativeAttrInt(pThis,SPL_F) : 0);` |
|    27 |  1378 | `	ph7_array_add_elem(&sOut,0,&sVal);` |
|    27 |  1379 | `	PH7_MemObjRelease(&sVal);` |
|     - |  1380 | `	/* [1] the storage */` |
|    27 |  1381 | `	pStore = SplStoreSlot(pVm,pThis);` |
|    27 |  1382 | `	if( pStore ){` |
|    27 |  1383 | `		ph7_array_add_elem(&sOut,0,pStore);` |
|    13 |  1384 | `	}` |
|     - |  1385 | `	/* [2] the instance's own properties */` |
|    27 |  1386 | `	if( SplMembersOf(pVm,pThis,&sVal) != SXRET_OK ){` |
|   ! 0 |  1387 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 |  1388 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  1389 | `	}` |
|    27 |  1390 | `	ph7_array_add_elem(&sOut,0,&sVal);` |
|    27 |  1391 | `	PH7_MemObjRelease(&sVal);` |
|     - |  1392 | `	/* [3] the iterator class, NULL for the default one and for ArrayIterator */` |
|    27 |  1393 | `	if( pThis ){` |
|    27 |  1394 | `		PH7_NativeAttrStr(pThis,SPL_IT,&zIt,&nIt);` |
|    13 |  1395 | `	}` |
|    27 |  1396 | `	PH7_MemObjInit(pVm,&sVal);` |
|    27 |  1397 | `	if( nIt > 0 && (nIt != (int)sizeof("ArrayIterator")-1` |
|    14 |  1398 | `	 \|\| SyMemcmp(zIt,"ArrayIterator",sizeof("ArrayIterator")-1) != 0) ){` |
|     5 |  1399 | `		PH7_MemObjStringAppend(&sVal,zIt,(sxu32)nIt);` |
|     2 |  1400 | `	}` |
|    27 |  1401 | `	ph7_array_add_elem(&sOut,0,&sVal);` |
|    27 |  1402 | `	PH7_MemObjRelease(&sVal);` |
|    27 |  1403 | `	ph7_result_value(pCtx,&sOut);` |
|    27 |  1404 | `	PH7_MemObjRelease(&sOut);` |
|    27 |  1405 | `	return PH7_OK;` |
|    14 |  1406 | `}` |
|     - |  1407 | `static void SplSerializeInto(ph7_context *pCtx,ph7_value **apCall,SyBlob *pOut);` |
|     - |  1408 | `/*` |
|     - |  1409 | ` * php's Serializable pair for the array store -- the LEGACY byte format the` |
|     - |  1410 | `` * magic pair above replaced, which php still declares (`ArrayObject implements`` |
|     - |  1411 | `` * ... Serializable`) and still answers. Neither name existed here, so the`` |
|     - |  1412 | `` * interface could not be declared either: `$ao instanceof Serializable` was`` |
|     - |  1413 | `` * false and `$ao->serialize()` a `Call to undefined method`.`` |
|     - |  1414 | ` *` |
|     - |  1415 | `` * The string is `x:<flags><storage>;m:<members>`, each part php's own`` |
|     - |  1416 | ` * serialize() output -- so a payload written here reads back in php and the` |
|     - |  1417 | ` * other way round. php's reader is a hand-rolled walk and its refusal reports` |
|     - |  1418 | ` * WHERE it gave up, which is why the offsets below are spelled out one by one:` |
|     - |  1419 | ` * a value it could not read at all blames the position it started from, while` |
|     - |  1420 | ` * one it read and then rejected for its TYPE blames the position after it.` |
|     - |  1421 | ` * The storage is screened by its type BYTE before the read, so a well-formed` |
|     - |  1422 | `` * `i:5;` there is refused at the byte rather than after it.`` |
|     - |  1423 | ` */` |
|     8 |  1424 | `static int vm_builtin_SplStore_serialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1425 | `{` |
|     9 |  1426 | `	ph7_vm *pVm = pCtx->pVm;` |
|     9 |  1427 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  1428 | `	ph7_value sVal,*pStore,*apCall[1];` |
|     - |  1429 | `	SyBlob sOut;` |
|     4 |  1430 | `	SXUNUSED(nArg);` |
|     4 |  1431 | `	SXUNUSED(apArg);` |
|     9 |  1432 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|     9 |  1433 | `	SyBlobAppend(&sOut,"x:",sizeof("x:")-1);` |
|     9 |  1434 | `	PH7_MemObjInitFromInt(pVm,&sVal,pThis ? PH7_NativeAttrInt(pThis,SPL_F) : 0);` |
|     9 |  1435 | `	apCall[0] = &sVal;` |
|     9 |  1436 | `	SplSerializeInto(pCtx,apCall,&sOut);` |
|     9 |  1437 | `	PH7_MemObjRelease(&sVal);` |
|     9 |  1438 | `	pStore = SplStoreSlot(pVm,pThis);` |
|     9 |  1439 | `	if( pStore ){` |
|     9 |  1440 | `		apCall[0] = pStore;` |
|     9 |  1441 | `		SplSerializeInto(pCtx,apCall,&sOut);` |
|     4 |  1442 | `	}` |
|     9 |  1443 | `	SyBlobAppend(&sOut,";m:",sizeof(";m:")-1);` |
|     9 |  1444 | `	if( SplMembersOf(pVm,pThis,&sVal) == SXRET_OK ){` |
|     9 |  1445 | `		apCall[0] = &sVal;` |
|     9 |  1446 | `		SplSerializeInto(pCtx,apCall,&sOut);` |
|     9 |  1447 | `		PH7_MemObjRelease(&sVal);` |
|     4 |  1448 | `	}` |
|     - |  1449 | `	/* ph7_result_string APPENDS, and pRet still holds the last nested answer. */` |
|     9 |  1450 | `	if( pCtx->pRet ){` |
|     9 |  1451 | `		PH7_MemObjRelease(pCtx->pRet);` |
|     4 |  1452 | `	}` |
|     9 |  1453 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     9 |  1454 | `	SyBlobRelease(&sOut);` |
|     9 |  1455 | `	return PH7_OK;` |
|     1 |  1456 | `}` |
|     - |  1457 | `/* php reports WHERE its walk gave up, in bytes. */` |
|    30 |  1458 | `static sxi32 SplStoreOffsetErr(ph7_context *pCtx,int nAt,int nTotal)` |
|     1 |  1459 | `{` |
|    46 |  1460 | `	return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|    15 |  1461 | `		"Error at offset %d of %d bytes",nAt,nTotal);` |
|     1 |  1462 | `}` |
|    36 |  1463 | `static int vm_builtin_SplStore_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1464 | `{` |
|    37 |  1465 | `	ph7_vm *pVm = pCtx->pVm;` |
|    37 |  1466 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  1467 | `	const char *zData;` |
|    37 |  1468 | `	int nData = 0,nAt,nRead = 0;` |
|     - |  1469 | `	ph7_value sFlags,sStore,sMembers;` |
|     - |  1470 | `	sxi32 rc;` |
|    37 |  1471 | `	if( nArg < 1 \|\| pThis == 0 ){` |
|   ! 0 |  1472 | `		return PH7_OK;` |
|     - |  1473 | `	}` |
|    37 |  1474 | `	zData = ph7_value_to_string(apArg[0],&nData);` |
|    37 |  1475 | `	if( nData < 1 ){` |
|     3 |  1476 | `		return PH7_OK;   /* php returns without touching the store */` |
|     - |  1477 | `	}` |
|    35 |  1478 | `	if( zData[0] != 'x' ){` |
|     3 |  1479 | `		return SplStoreOffsetErr(pCtx,0,nData);` |
|     - |  1480 | `	}` |
|    33 |  1481 | `	if( nData < 2 \|\| zData[1] != ':' ){` |
|     5 |  1482 | `		return SplStoreOffsetErr(pCtx,1,nData);` |
|     - |  1483 | `	}` |
|    29 |  1484 | `	nAt = 2;` |
|    29 |  1485 | `	PH7_MemObjInit(pVm,&sFlags);` |
|    29 |  1486 | `	PH7_MemObjInit(pVm,&sStore);` |
|    29 |  1487 | `	PH7_MemObjInit(pVm,&sMembers);` |
|    29 |  1488 | `	rc = PH7_VmUnserializeOne(pCtx,&zData[nAt],nData - nAt,&nRead,&sFlags);` |
|    29 |  1489 | `	if( rc == PH7_EXCEPTION ){` |
|   ! 0 |  1490 | `		goto Done;` |
|     - |  1491 | `	}` |
|    29 |  1492 | `	if( rc != SXRET_OK ){` |
|     5 |  1493 | `		rc = SplStoreOffsetErr(pCtx,nAt,nData);` |
|     5 |  1494 | `		goto Done;` |
|     - |  1495 | `	}` |
|    25 |  1496 | `	nAt += nRead;` |
|    25 |  1497 | `	if( (sFlags.iFlags & MEMOBJ_INT) == 0 ){` |
|     3 |  1498 | `		rc = SplStoreOffsetErr(pCtx,nAt,nData);` |
|     3 |  1499 | `		goto Done;` |
|     - |  1500 | `	}` |
|     - |  1501 | `	/* The storage's type BYTE decides before the read: php accepts an array,` |
|     - |  1502 | `	 * an object of any of its three spellings, or a back-reference. */` |
|    26 |  1503 | `	if( nAt >= nData \|\| (zData[nAt] != 'a' && zData[nAt] != 'O'` |
|     6 |  1504 | `	 && zData[nAt] != 'C' && zData[nAt] != 'r') ){` |
|     9 |  1505 | `		rc = SplStoreOffsetErr(pCtx,nAt,nData);` |
|     9 |  1506 | `		goto Done;` |
|     - |  1507 | `	}` |
|    15 |  1508 | `	rc = PH7_VmUnserializeOne(pCtx,&zData[nAt],nData - nAt,&nRead,&sStore);` |
|    15 |  1509 | `	if( rc == PH7_EXCEPTION ){` |
|   ! 0 |  1510 | `		goto Done;` |
|     - |  1511 | `	}` |
|    15 |  1512 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  1513 | `		rc = SplStoreOffsetErr(pCtx,nAt,nData);` |
|   ! 0 |  1514 | `		goto Done;` |
|     - |  1515 | `	}` |
|    15 |  1516 | `	nAt += nRead;` |
|    15 |  1517 | `	if( nAt >= nData \|\| zData[nAt] != ';' ){` |
|     3 |  1518 | `		rc = SplStoreOffsetErr(pCtx,nAt,nData);` |
|     3 |  1519 | `		goto Done;` |
|     - |  1520 | `	}` |
|    13 |  1521 | `	nAt++;` |
|    13 |  1522 | `	if( nAt >= nData \|\| zData[nAt] != 'm' ){` |
|     3 |  1523 | `		rc = SplStoreOffsetErr(pCtx,nAt,nData);` |
|     3 |  1524 | `		goto Done;` |
|     - |  1525 | `	}` |
|    11 |  1526 | `	nAt++;` |
|    11 |  1527 | `	if( nAt >= nData \|\| zData[nAt] != ':' ){` |
|     3 |  1528 | `		rc = SplStoreOffsetErr(pCtx,nAt,nData);` |
|     3 |  1529 | `		goto Done;` |
|     - |  1530 | `	}` |
|     9 |  1531 | `	nAt++;` |
|     9 |  1532 | `	rc = PH7_VmUnserializeOne(pCtx,&zData[nAt],nData - nAt,&nRead,&sMembers);` |
|     9 |  1533 | `	if( rc == PH7_EXCEPTION ){` |
|   ! 0 |  1534 | `		goto Done;` |
|     - |  1535 | `	}` |
|     9 |  1536 | `	if( rc != SXRET_OK ){` |
|     3 |  1537 | `		rc = SplStoreOffsetErr(pCtx,nAt,nData);` |
|     3 |  1538 | `		goto Done;` |
|     - |  1539 | `	}` |
|     7 |  1540 | `	nAt += nRead;` |
|     7 |  1541 | `	if( (sMembers.iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     3 |  1542 | `		rc = SplStoreOffsetErr(pCtx,nAt,nData);` |
|     3 |  1543 | `		goto Done;` |
|     - |  1544 | `	}` |
|     5 |  1545 | `	PH7_NativeSetAttrInt(pVm,pThis,SPL_F,ph7_value_to_int64(&sFlags) & SPL_FLAG_MASK);` |
|     5 |  1546 | `	rc = SplInitStore(pCtx,pThis,&sStore,"ArrayObject::unserialize");` |
|     7 |  1547 | `	if( rc == SXRET_OK ){` |
|     5 |  1548 | `		SplMembersLoad(pThis,&sMembers);` |
|     5 |  1549 | `		rc = PH7_OK;` |
|     2 |  1550 | `	}` |
|   ! 0 |  1551 | `Done:` |
|    29 |  1552 | `	PH7_MemObjRelease(&sFlags);` |
|    29 |  1553 | `	PH7_MemObjRelease(&sStore);` |
|    29 |  1554 | `	PH7_MemObjRelease(&sMembers);` |
|    29 |  1555 | `	return rc;` |
|    19 |  1556 | `}` |
|    38 |  1557 | `static int vm_builtin_SplStore_unserializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1558 | `{` |
|    39 |  1559 | `	ph7_vm *pVm = pCtx->pVm;` |
|    39 |  1560 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  1561 | `	ph7_hashmap *pData;` |
|    39 |  1562 | `	ph7_hashmap_node *pNode = 0;` |
|    39 |  1563 | `	ph7_value *pFlags,*pStorage,*pMembers,*pIt = 0;` |
|     - |  1564 | `	sxi32 rc;` |
|    39 |  1565 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     - |  1566 | `		char zBuf[64];` |
|   ! 0 |  1567 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  1568 | `			"%s(): Argument #1 ($data) must be of type array, %s given",` |
|   ! 0 |  1569 | `			ph7_function_name(pCtx),` |
|   ! 0 |  1570 | `			nArg > 0 ? VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)) : "none");` |
|     - |  1571 | `	}` |
|    39 |  1572 | `	if( pThis == 0 ){` |
|   ! 0 |  1573 | `		return PH7_OK;` |
|     - |  1574 | `	}` |
|    39 |  1575 | `	pData = (ph7_hashmap *)apArg[0]->x.pOther;` |
|    39 |  1576 | `	if( HashmapLookupIntKey(pData,0,&pNode) != SXRET_OK ){` |
|   ! 0 |  1577 | `		return SplStoreIllTyped(pCtx);` |
|     - |  1578 | `	}` |
|    39 |  1579 | `	pFlags = HashmapExtractNodeValue(pNode);` |
|    39 |  1580 | `	pNode = 0;` |
|    39 |  1581 | `	if( HashmapLookupIntKey(pData,1,&pNode) != SXRET_OK ){` |
|   ! 0 |  1582 | `		return SplStoreIllTyped(pCtx);` |
|     - |  1583 | `	}` |
|    39 |  1584 | `	pStorage = HashmapExtractNodeValue(pNode);` |
|    39 |  1585 | `	pNode = 0;` |
|    39 |  1586 | `	if( HashmapLookupIntKey(pData,2,&pNode) != SXRET_OK ){` |
|     3 |  1587 | `		return SplStoreIllTyped(pCtx);` |
|     - |  1588 | `	}` |
|    37 |  1589 | `	pMembers = HashmapExtractNodeValue(pNode);` |
|    37 |  1590 | `	pNode = 0;` |
|    37 |  1591 | `	if( HashmapLookupIntKey(pData,3,&pNode) == SXRET_OK ){` |
|    29 |  1592 | `		pIt = HashmapExtractNodeValue(pNode);` |
|    14 |  1593 | `	}` |
|    36 |  1594 | `	if( pFlags == 0 \|\| (pFlags->iFlags & MEMOBJ_INT) == 0` |
|    35 |  1595 | `	 \|\| pMembers == 0 \|\| (pMembers->iFlags & MEMOBJ_HASHMAP) == 0` |
|    34 |  1596 | `	 \|\| (pIt != 0 && (pIt->iFlags & (MEMOBJ_NULL\|MEMOBJ_STRING)) == 0) ){` |
|     7 |  1597 | `		return SplStoreIllTyped(pCtx);` |
|     - |  1598 | `	}` |
|     - |  1599 | `	/* php's own wording, and its own exception CLASS, for the storage slot. */` |
|    31 |  1600 | `	if( pStorage == 0 \|\| (pStorage->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 ){` |
|     3 |  1601 | `		return PH7_VmThrowException(pCtx,"InvalidArgumentException",` |
|     - |  1602 | `			"Passed variable is not an array or object");` |
|     - |  1603 | `	}` |
|    29 |  1604 | `	if( pIt != 0 && (pIt->iFlags & MEMOBJ_STRING) != 0 && SyBlobLength(&pIt->sBlob) > 0 ){` |
|    11 |  1605 | `		const char *zIt = (const char *)SyBlobData(&pIt->sBlob);` |
|    11 |  1606 | `		int nIt = (int)SyBlobLength(&pIt->sBlob);` |
|    11 |  1607 | `		ph7_class *pClass = PH7_VmExtractClass(pVm,zIt,(sxu32)nIt,FALSE,0);` |
|    11 |  1608 | `		ph7_class *pIface = PH7_VmExtractClass(pVm,"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|    11 |  1609 | `		if( pClass == 0 ){` |
|     4 |  1610 | `			return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - |  1611 | `				"Cannot deserialize ArrayObject with iterator class '%.*s'; "` |
|     1 |  1612 | `				"no such class exists",nIt,zIt);` |
|     - |  1613 | `		}` |
|     9 |  1614 | `		if( pIface == 0 \|\| !PH7_VmInstanceOf(pClass,pIface) ){` |
|     7 |  1615 | `			return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - |  1616 | `				"Cannot deserialize ArrayObject with iterator class '%.*s'; "` |
|     2 |  1617 | `				"this class does not implement the Iterator interface",nIt,zIt);` |
|     - |  1618 | `		}` |
|     5 |  1619 | `		if( PH7_NativeAttr(pThis,SPL_IT) ){` |
|     5 |  1620 | `			PH7_NativeSetAttrStr(pVm,pThis,SPL_IT,zIt,nIt);` |
|     2 |  1621 | `		}` |
|     2 |  1622 | `	}` |
|    23 |  1623 | `	PH7_NativeSetAttrInt(pVm,pThis,SPL_F,ph7_value_to_int64(pFlags) & SPL_FLAG_MASK);` |
|    23 |  1624 | `	rc = SplInitStore(pCtx,pThis,pStorage,"ArrayObject::__unserialize");` |
|    23 |  1625 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  1626 | `		return rc;` |
|     - |  1627 | `	}` |
|    23 |  1628 | `	SplMembersLoad(pThis,pMembers);` |
|    23 |  1629 | `	return PH7_OK;` |
|    20 |  1630 | `}` |
|     - |  1631 | `/*` |
|     - |  1632 | ` * ---------------------------------------------------------------------------` |
|     - |  1633 | ` * php's presentation for the array store (ph7_class::xPresent).` |
|     - |  1634 | ` *` |
|     - |  1635 | ` * php has two handlers here and they DISAGREE, which is the whole reason the hook` |
|     - |  1636 | `` * is told which is asking. `spl_array_get_debug_info` always shows ONE entry —`` |
|     - |  1637 | ` * the storage under its MANGLED private name — after whatever real properties the` |
|     - |  1638 | `` * instance has; `spl_array_get_properties_for` answers the storage's ELEMENTS`` |
|     - |  1639 | `` * directly for the var_export / (array) / json purposes, with no `storage` key at`` |
|     - |  1640 | ` * all, and hands back the ordinary property table when STD_PROP_LIST is set. The` |
|     - |  1641 | ` * flag is therefore visible on one surface and invisible on the other: a` |
|     - |  1642 | ` * STD_PROP_LIST ArrayObject still var_dumps its storage.` |
|     - |  1643 | ` *` |
|     - |  1644 | ` * The mangled name always spells the ROOT class, never the receiver's: a` |
|     - |  1645 | `` * RecursiveArrayIterator shows `["storage":"ArrayIterator":private]`.`` |
|     - |  1646 | ` * ---------------------------------------------------------------------------` |
|     - |  1647 | ` */` |
|    14 |  1648 | `static int SplStoreMangledKey(ph7_class_instance *pThis,char *zBuf,int nBuf)` |
|     1 |  1649 | `{` |
|    15 |  1650 | `	const char *zRoot = "ArrayObject";` |
|     - |  1651 | `	ph7_class *pClass;` |
|    15 |  1652 | `	int nRoot,nOut = 0;` |
|    27 |  1653 | `	for( pClass = pThis->pClass ; pClass ; pClass = pClass->pBase ){` |
|    18 |  1654 | `		if( pClass->sName.nByte == sizeof("ArrayIterator")-1` |
|    13 |  1655 | `		 && SyMemcmp(pClass->sName.zString,"ArrayIterator",sizeof("ArrayIterator")-1) == 0 ){` |
|     7 |  1656 | `			zRoot = "ArrayIterator";` |
|     7 |  1657 | `			break;` |
|     - |  1658 | `		}` |
|     7 |  1659 | `	}` |
|    15 |  1660 | `	nRoot = (int)SyStrlen(zRoot);` |
|    15 |  1661 | `	if( nRoot + (int)sizeof("\0\0storage") > nBuf ){` |
|   ! 0 |  1662 | `		return 0;` |
|     - |  1663 | `	}` |
|    15 |  1664 | `	zBuf[nOut++] = 0;` |
|    15 |  1665 | `	SyMemcpy(zRoot,&zBuf[nOut],(sxu32)nRoot);` |
|    15 |  1666 | `	nOut += nRoot;` |
|    15 |  1667 | `	zBuf[nOut++] = 0;` |
|    15 |  1668 | `	SyMemcpy("storage",&zBuf[nOut],sizeof("storage")-1);` |
|    15 |  1669 | `	nOut += (int)sizeof("storage")-1;` |
|    15 |  1670 | `	return nOut;` |
|     8 |  1671 | `}` |
|    24 |  1672 | `static int SplPresentWalk(ph7_value *pKey,ph7_value *pVal,void *pUserData)` |
|     2 |  1673 | `{` |
|    26 |  1674 | `	ph7_array_add_elem((ph7_value *)pUserData,pKey,pVal);` |
|    26 |  1675 | `	return PH7_OK;` |
|     2 |  1676 | `}` |
|    38 |  1677 | `static sxi32 SplStorePresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|     2 |  1678 | `{` |
|    40 |  1679 | `	ph7_value *pStore = pThis ? PH7_NativeAttr(pThis,SPL_D) : 0;` |
|    40 |  1680 | `	if( bDebug ){` |
|     - |  1681 | `		ph7_value sKey;` |
|     - |  1682 | `		char zKey[64];` |
|     - |  1683 | `		int nKey;` |
|     - |  1684 | `		/* The instance's OWN properties come first — php's debug info starts from` |
|     - |  1685 | `		 * the standard table and appends the storage entry to it. */` |
|    15 |  1686 | `		SplAddMembers(&(*pVm),pThis,pOut);` |
|    15 |  1687 | `		nKey = SplStoreMangledKey(pThis,zKey,(int)sizeof(zKey));` |
|    15 |  1688 | `		if( nKey > 0 && pStore ){` |
|    15 |  1689 | `			PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|    15 |  1690 | `			PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKey);` |
|    15 |  1691 | `			ph7_array_add_elem(pOut,&sKey,pStore);` |
|    15 |  1692 | `			PH7_MemObjRelease(&sKey);` |
|     7 |  1693 | `		}` |
|    15 |  1694 | `		return SXRET_OK;` |
|     - |  1695 | `	}` |
|    26 |  1696 | `	if( (PH7_NativeAttrInt(pThis,SPL_F) & SPL_STD_PROP_LIST) != 0 ){` |
|     5 |  1697 | `		SplAddMembers(&(*pVm),pThis,pOut);` |
|     5 |  1698 | `		return SXRET_OK;` |
|     - |  1699 | `	}` |
|    22 |  1700 | `	if( pStore && (pStore->iFlags & MEMOBJ_HASHMAP) != 0 ){` |
|    22 |  1701 | `		ph7_array_walk(pStore,SplPresentWalk,pOut);` |
|    10 |  1702 | `	}` |
|    22 |  1703 | `	return SXRET_OK;` |
|    21 |  1704 | `}` |
|    18 |  1705 | `static int vm_builtin_ArrayObject_getIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  1706 | `{` |
|    20 |  1707 | `	ph7_vm *pVm = pCtx->pVm;` |
|    20 |  1708 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  1709 | `	ph7_class_instance *pIt;` |
|     - |  1710 | `	ph7_class *pClass;` |
|    20 |  1711 | `	const char *zName = 0;` |
|    20 |  1712 | `	int nName = 0;` |
|     - |  1713 | `	ph7_value *pSlot;` |
|     - |  1714 | `	ph7_class_method *pCons;` |
|     9 |  1715 | `	SXUNUSED(nArg);` |
|     9 |  1716 | `	SXUNUSED(apArg);` |
|    20 |  1717 | `	if( pThis == 0 ){` |
|   ! 0 |  1718 | `		return PH7_OK;` |
|     - |  1719 | `	}` |
|    20 |  1720 | `	PH7_NativeAttrStr(pThis,SPL_IT,&zName,&nName);` |
|    20 |  1721 | `	if( nName < 1 ){` |
|   ! 0 |  1722 | `		zName = "ArrayIterator";` |
|   ! 0 |  1723 | `		nName = (int)sizeof("ArrayIterator")-1;` |
|   ! 0 |  1724 | `	}` |
|    20 |  1725 | `	pClass = PH7_VmExtractClass(pVm,zName,(sxu32)nName,FALSE,0);` |
|    20 |  1726 | `	if( pClass == 0 ){` |
|   ! 0 |  1727 | `		return PH7_OK;` |
|     - |  1728 | `	}` |
|    20 |  1729 | `	pIt = PH7_NewClassInstance(pVm,pClass);` |
|    20 |  1730 | `	if( pIt == 0 ){` |
|   ! 0 |  1731 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  1732 | `	}` |
|    20 |  1733 | `	pIt->iRef++;` |
|    20 |  1734 | `	pSlot = SplStoreSlot(pVm,pThis);` |
|    20 |  1735 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|    20 |  1736 | `	if( pCons && pSlot ){` |
|     - |  1737 | ``		/* `new $c($this->__d)`: the iterator gets a COPY of the store, as the PHP did —`` |
|     - |  1738 | `		 * a user subclass of ArrayIterator runs its own constructor here. */` |
|     - |  1739 | `		ph7_value *apCtor[1];` |
|    20 |  1740 | `		apCtor[0] = pSlot;` |
|    20 |  1741 | `		PH7_VmCallClassMethod(pVm,pIt,pCons,0,1,apCtor);` |
|     9 |  1742 | `	}` |
|    20 |  1743 | `	PH7_NativeResultObject(pCtx,pIt);` |
|    20 |  1744 | `	PH7_ClassInstanceUnref(pIt);` |
|    20 |  1745 | `	return PH7_OK;` |
|    11 |  1746 | `}` |
|     - |  1747 | `/*` |
|     - |  1748 | ` * php's read_property / has_property / write_property / unset_property for the two` |
|     - |  1749 | ` * store containers, as ph7_class::xProp -- and the FLAG is the whole handler.` |
|     - |  1750 | ` *` |
|     - |  1751 | `` * php's `spl_array_get_hash_table` answers the storage only when`` |
|     - |  1752 | ` * SPL_ARRAY_ARRAY_AS_PROPS is set, and its handlers stand down entirely when it is` |
|     - |  1753 | ` * not: the object's ordinary property table answers instead, so a read of a` |
|     - |  1754 | `` * storage key on the DEFAULT object is php's `Undefined property` warning and a`` |
|     - |  1755 | ` * write creates a property BESIDE the storage. PHL reached the storage through` |
|     - |  1756 | `` * `__get`/`__set`/`__isset`/`__unset` -- four methods php does not have, which`` |
|     - |  1757 | `` * `get_class_methods()` reported -- and those bodies could not stand down: with`` |
|     - |  1758 | ` * the flag off they answered null and swallowed the write in silence, and with it` |
|     - |  1759 | ` * on they answered a missing key in silence where php warns.` |
|     - |  1760 | ` *` |
|     - |  1761 | ` * A name the object holds a REAL property for never reaches here at all (the` |
|     - |  1762 | ` * member opcode consults the handler only on a miss), which is php's own order:` |
|     - |  1763 | `` * `class S extends ArrayObject { public $own; }` writes `$s->own` to the slot and`` |
|     - |  1764 | `` * `$s->x` to the storage.`` |
|     - |  1765 | ` */` |
|     - |  1766 | `#define SPL_ARRAY_AS_PROPS 0x0002` |
|     - |  1767 | `/* The store's node for this property name, or 0. bCreate is php's write-context` |
|     - |  1768 | ` * vivification -- the same rule the DIMENSION fast path above follows. */` |
|    64 |  1769 | `static ph7_hashmap_node * SplPropNode(ph7_vm *pVm,ph7_class_instance *pThis,` |
|     - |  1770 | `	const SyString *pName,int bCreate)` |
|     1 |  1771 | `{` |
|    65 |  1772 | `	ph7_hashmap *pMap = SplStore(pVm,pThis);` |
|    65 |  1773 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  1774 | `	ph7_value sKey;` |
|     - |  1775 | `	int bHit;` |
|    65 |  1776 | `	if( pMap == 0 ){` |
|   ! 0 |  1777 | `		return 0;` |
|     - |  1778 | `	}` |
|    65 |  1779 | `	PH7_MemObjInitFromString(pVm,&sKey,pName);` |
|    65 |  1780 | `	bHit = PH7_HashmapLookup(pMap,&sKey,&pNode) == SXRET_OK;` |
|    65 |  1781 | `	if( !bHit && bCreate ){` |
|     5 |  1782 | `		if( PH7_HashmapInsert(pMap,&sKey,0) == SXRET_OK ){` |
|     5 |  1783 | `			pNode = pMap->pLast;` |
|     5 |  1784 | `			bHit = pNode != 0;` |
|     2 |  1785 | `		}` |
|     2 |  1786 | `	}` |
|    65 |  1787 | `	PH7_MemObjRelease(&sKey);` |
|    65 |  1788 | `	return bHit ? pNode : 0;` |
|    33 |  1789 | `}` |
|   134 |  1790 | `static void SplArrayProp(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativePropCtx *pCtx)` |
|     2 |  1791 | `{` |
|     - |  1792 | `	ph7_hashmap_node *pNode;` |
|     - |  1793 | `	ph7_value *pVal;` |
|   136 |  1794 | `	if( pThis == 0 \|\| (PH7_NativeAttrInt(pThis,SPL_F) & SPL_ARRAY_AS_PROPS) == 0 ){` |
|    12 |  1795 | `		return;   /* php's handler stands down: the standard property path answers */` |
|     - |  1796 | `	}` |
|   125 |  1797 | `	if( pCtx->iMode == PH7_NATIVE_PROP_WRITE ){` |
|    15 |  1798 | `		return;   /* the value does not exist yet; OWNS routes it to STORE */` |
|     - |  1799 | `	}` |
|   111 |  1800 | `	if( pCtx->iMode == PH7_NATIVE_PROP_OWNS ){` |
|     - |  1801 | `		/* Every name is the storage's once the flag is on -- php's handler does not` |
|     - |  1802 | `		 * consult the keys to decide, which is why a write to a name no key carries` |
|     - |  1803 | `		 * CREATES one rather than falling through to a dynamic property. */` |
|    33 |  1804 | `		pCtx->bAnswered = 1;` |
|    33 |  1805 | `		return;` |
|     - |  1806 | `	}` |
|    79 |  1807 | `	if( pCtx->iMode == PH7_NATIVE_PROP_STORE ){` |
|    15 |  1808 | `		ph7_hashmap *pMap = SplStore(pVm,pThis);` |
|    15 |  1809 | `		if( pMap ){` |
|     - |  1810 | `			ph7_value sKey;` |
|    15 |  1811 | `			PH7_MemObjInitFromString(pVm,&sKey,pCtx->pName);` |
|    15 |  1812 | `			PH7_HashmapInsert(pMap,&sKey,pCtx->pResult);` |
|    15 |  1813 | `			PH7_MemObjRelease(&sKey);` |
|     7 |  1814 | `		}` |
|    15 |  1815 | `		pCtx->bAnswered = 1;` |
|    15 |  1816 | `		return;` |
|     - |  1817 | `	}` |
|    65 |  1818 | `	if( pCtx->iMode == PH7_NATIVE_PROP_UNSET ){` |
|     5 |  1819 | `		pNode = SplPropNode(pVm,pThis,pCtx->pName,FALSE);` |
|     5 |  1820 | `		if( pNode ){` |
|     5 |  1821 | `			PH7_HashmapUnlinkNode(pNode,TRUE);` |
|     2 |  1822 | `		}` |
|     5 |  1823 | `		pCtx->bAnswered = 1;` |
|     5 |  1824 | `		return;` |
|     - |  1825 | `	}` |
|    61 |  1826 | `	pCtx->bAnswered = 1;` |
|     - |  1827 | `	/* A WRITE-context read is php's get_property_ptr_ptr: it hands back the store's` |
|     - |  1828 | `	 * OWN element -- creating the key when there is none, silently, exactly as the` |
|     - |  1829 | ``	 * dimension form does -- so `$ao->list[] = 1` and `$ao->deep['k'] = 1` land in`` |
|     - |  1830 | `	 * the store rather than in a temporary nothing else can see. An element that` |
|     - |  1831 | `	 * already EXISTS is handed out for a plain read too, which is what makes` |
|     - |  1832 | ``	 * `sort($ao->nums)` and `foreach ($ao->rows as &$r)` write through. */`` |
|    75 |  1833 | `	pNode = SplPropNode(pVm,pThis,pCtx->pName,` |
|    60 |  1834 | `		pCtx->iMode == PH7_NATIVE_PROP_READ && pCtx->bWriteCtx);` |
|    61 |  1835 | `	pVal = pNode ? (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pNode->nValIdx) : 0;` |
|    61 |  1836 | `	if( pCtx->iMode != PH7_NATIVE_PROP_READ ){` |
|     - |  1837 | `		/* php's three has_property questions, each judging the value it just` |
|     - |  1838 | `		 * fetched: NULL-ness for isset(), TRUTH for the check_empty question` |
|     - |  1839 | ``		 * `empty()` NEGATES, and mere EXISTENCE for property_exists(). A key`` |
|     - |  1840 | ``		 * holding 0 answers true, false and true -- so it is `isset()`, it IS`` |
|     - |  1841 | ``		 * `empty()`, and `property_exists()` finds it. */`` |
|     - |  1842 | `		int bSet;` |
|    33 |  1843 | `		if( pCtx->iMode == PH7_NATIVE_PROP_ISSET ){` |
|    17 |  1844 | `			bSet = pVal != 0 && (pVal->iFlags & MEMOBJ_NULL) == 0;` |
|    25 |  1845 | `		}else if( pCtx->iMode == PH7_NATIVE_PROP_NOTEMPTY ){` |
|     - |  1846 | `			/* The truth of a COPY: this element is the store's own slot and` |
|     - |  1847 | `			 * PH7_MemObjToBool retypes what it is handed, so asking the question` |
|     - |  1848 | ``			 * here would turn `$ao->n` from the int it holds into the bool the`` |
|     - |  1849 | `			 * answer is -- and leave it that way for every later read. */` |
|     9 |  1850 | `			bSet = 0;` |
|     9 |  1851 | `			if( pVal ){` |
|     - |  1852 | `				ph7_value sTruth;` |
|     7 |  1853 | `				PH7_MemObjInit(pVm,&sTruth);` |
|     7 |  1854 | `				PH7_MemObjStore(pVal,&sTruth);` |
|     7 |  1855 | `				PH7_MemObjToBool(&sTruth);` |
|     7 |  1856 | `				bSet = sTruth.x.iVal != 0;` |
|     7 |  1857 | `				PH7_MemObjRelease(&sTruth);` |
|     3 |  1858 | `			}` |
|     5 |  1859 | `		}else{` |
|     9 |  1860 | `			bSet = pVal != 0;` |
|     - |  1861 | `		}` |
|    33 |  1862 | `		ph7_value_bool(pCtx->pResult,bSet);` |
|    33 |  1863 | `		return;` |
|     - |  1864 | `	}` |
|    29 |  1865 | `	if( pVal == 0 ){` |
|     - |  1866 | `		/* php reports the missing ARRAY KEY here, not a missing property: the` |
|     - |  1867 | `		 * handler took the access and the storage is where it looked. Silent for` |
|     - |  1868 | ``		 * the lookup `??` makes, as every read-miss diagnostic is. */`` |
|     7 |  1869 | `		if( !pCtx->bQuiet ){` |
|     3 |  1870 | `			VmErrorFormat(pVm,PH7_CTX_WARNING,"Undefined array key \"%z\"",pCtx->pName);` |
|     1 |  1871 | `		}` |
|     7 |  1872 | `		return;` |
|     - |  1873 | `	}` |
|    23 |  1874 | `	PH7_MemObjStore(pVal,pCtx->pResult);` |
|    23 |  1875 | `	pCtx->nSlot = pNode->nValIdx;` |
|    69 |  1876 | `}` |
|     - |  1877 | `/*` |
|     - |  1878 | `` * php's `__debugInfo()`, which both classes declare so a program can read by name`` |
|     - |  1879 | `` * the array `var_dump()` prints: the object's own properties under php's mangled`` |
|     - |  1880 | `` * keys, then the storage under the mangled `storage` key of the class that`` |
|     - |  1881 | `` * DECLARED it (an `ArrayIterator` subclass shows `\0ArrayIterator\0storage`).`` |
|     - |  1882 | ` * The same array the debug presentation hook builds, which is php's own sharing.` |
|     - |  1883 | ` */` |
|     2 |  1884 | `static int vm_builtin_SplStore_debugInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1885 | `{` |
|     3 |  1886 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  1887 | `	ph7_value *pOut;` |
|     1 |  1888 | `	SXUNUSED(nArg);` |
|     1 |  1889 | `	SXUNUSED(apArg);` |
|     3 |  1890 | `	pOut = ph7_context_new_array(pCtx);` |
|     3 |  1891 | `	if( pOut == 0 ){` |
|   ! 0 |  1892 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  1893 | `	}` |
|     3 |  1894 | `	SplStorePresent(pCtx->pVm,pThis,pOut,TRUE);` |
|     3 |  1895 | `	ph7_result_value(pCtx,pOut);` |
|     3 |  1896 | `	return PH7_OK;` |
|     2 |  1897 | `}` |
|     - |  1898 | `/*` |
|     - |  1899 | ` * Declare both classes plus SeekableIterator, which ArrayIterator implements and which` |
|     - |  1900 | ` * therefore cannot wait for the chunk. RecursiveArrayIterator still lives there and extends` |
|     - |  1901 | ` * ArrayIterator, so this install has to run BEFORE the chunk is evaluated.` |
|     - |  1902 | ` */` |
|  6721 |  1903 | `static sxi32 VmInstallSplStore(ph7_vm *pVm)` |
|     5 |  1904 | `{` |
|     - |  1905 | ``	/* php's `@tentative-return-type void`, as on the other SPL contracts`` |
|     - |  1906 | `	 * (VmInstallSplDualIterators). */` |
|     - |  1907 | `	static const PH7_NativeMethodDef aSeekMethod[] = {` |
|     - |  1908 | `		{ "seek", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "int $offset", "@void", 0 },` |
|     - |  1909 | `	};` |
|     - |  1910 | `	static const PH7_NativePropDef aItProp[] = {` |
|     - |  1911 | `		{ SPL_D, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  1912 | `		{ SPL_F, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  1913 | `	};` |
|     - |  1914 | `	static const PH7_NativePropDef aObjProp[] = {` |
|     - |  1915 | `		{ SPL_D,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  1916 | `		{ SPL_F,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  1917 | `		{ SPL_IT, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "ArrayIterator", 0.0 }, 0 },` |
|     - |  1918 | `	};` |
|     - |  1919 | `	static const PH7_NativeConstDef aConst[] = {` |
|     - |  1920 | `		{ "STD_PROP_LIST",  PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1, 0, 0.0 },` |
|     - |  1921 | `		{ "ARRAY_AS_PROPS", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2, 0, 0.0 },` |
|     - |  1922 | `	};` |
|     - |  1923 | `	/* php's declaration order, which is the order Reflection reports. */` |
|     - |  1924 | `	static const PH7_NativeMethodDef aItMethod[] = {` |
|     - |  1925 | `		{ "__construct",  PH7_MOD_PUBLIC, "object\|array $array = [], int $flags = 0", 0,` |
|     - |  1926 | `		  vm_builtin_ArrayIterator_construct },` |
|     - |  1927 | `		{ "offsetExists", PH7_MOD_PUBLIC, "mixed $key", "@bool", vm_builtin_SplStore_offsetExists },` |
|     - |  1928 | `		{ "offsetGet",    PH7_MOD_PUBLIC, "mixed $key", "@mixed", vm_builtin_SplStore_offsetGet },` |
|     - |  1929 | `		{ "offsetSet",    PH7_MOD_PUBLIC, "mixed $key, mixed $value", "@void", vm_builtin_SplStore_offsetSet },` |
|     - |  1930 | `		{ "offsetUnset",  PH7_MOD_PUBLIC, "mixed $key", "@void", vm_builtin_SplStore_offsetUnset },` |
|     - |  1931 | `		{ "append",       PH7_MOD_PUBLIC, "mixed $value", "@void", vm_builtin_SplStore_append },` |
|     - |  1932 | `		{ "getArrayCopy", PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplStore_getArrayCopy },` |
|     - |  1933 | `		{ "count",        PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplStore_count },` |
|     - |  1934 | `		{ "getFlags",     PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplStore_getFlags },` |
|     - |  1935 | `		{ "setFlags",     PH7_MOD_PUBLIC, "int $flags", "@void", vm_builtin_SplStore_setFlags },` |
|     - |  1936 | `		{ "asort",        PH7_MOD_PUBLIC, "int $flags = SORT_REGULAR", "@true", vm_builtin_SplStore_asort },` |
|     - |  1937 | `		{ "ksort",        PH7_MOD_PUBLIC, "int $flags = SORT_REGULAR", "@true", vm_builtin_SplStore_ksort },` |
|     - |  1938 | `		{ "uasort",       PH7_MOD_PUBLIC, "callable $callback", "@true", vm_builtin_SplStore_uasort },` |
|     - |  1939 | `		{ "uksort",       PH7_MOD_PUBLIC, "callable $callback", "@true", vm_builtin_SplStore_uksort },` |
|     - |  1940 | `		{ "natsort",      PH7_MOD_PUBLIC, "", "@true", vm_builtin_SplStore_natsort },` |
|     - |  1941 | `		{ "natcasesort",  PH7_MOD_PUBLIC, "", "@true", vm_builtin_SplStore_natcasesort },` |
|     - |  1942 | `		{ "unserialize",  PH7_MOD_PUBLIC, "string $data", "@void", vm_builtin_SplStore_unserialize },` |
|     - |  1943 | `		{ "serialize",    PH7_MOD_PUBLIC, "", "@string", vm_builtin_SplStore_serialize },` |
|     - |  1944 | `		{ "__serialize",  PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplStore_serializeMagic },` |
|     - |  1945 | `		{ "__unserialize", PH7_MOD_PUBLIC, "array $data", "@void",` |
|     - |  1946 | `		  vm_builtin_SplStore_unserializeMagic },` |
|     - |  1947 | `		/* php's own order for the Iterator five -- rewind FIRST, which is the order` |
|     - |  1948 | `		 * its stub declares them in and therefore the order get_class_methods() and` |
|     - |  1949 | `		 * Reflection report. */` |
|     - |  1950 | `		{ "rewind",       PH7_MOD_PUBLIC, "", "@void", vm_builtin_ArrayIterator_rewind },` |
|     - |  1951 | `		{ "current",      PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_ArrayIterator_current },` |
|     - |  1952 | `		{ "key",          PH7_MOD_PUBLIC, "", "@string\|int\|null", vm_builtin_ArrayIterator_key },` |
|     - |  1953 | `		{ "next",         PH7_MOD_PUBLIC, "", "@void", vm_builtin_ArrayIterator_next },` |
|     - |  1954 | `		{ "valid",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ArrayIterator_valid },` |
|     - |  1955 | `		{ "seek",         PH7_MOD_PUBLIC, "int $offset", "@void", vm_builtin_ArrayIterator_seek },` |
|     - |  1956 | `		{ "__debugInfo",  PH7_MOD_PUBLIC, "", 0, vm_builtin_SplStore_debugInfo },` |
|     - |  1957 | `	};` |
|     - |  1958 | `	static const PH7_NativeMethodDef aObjMethod[] = {` |
|     - |  1959 | `		{ "__construct",      PH7_MOD_PUBLIC,` |
|     - |  1960 | `		  "object\|array $array = [], int $flags = 0, ~string $iteratorClass = ArrayIterator::class", 0,` |
|     - |  1961 | `		  vm_builtin_ArrayObject_construct },` |
|     - |  1962 | `		{ "offsetExists",     PH7_MOD_PUBLIC, "mixed $key", "@bool", vm_builtin_SplStore_offsetExists },` |
|     - |  1963 | `		{ "offsetGet",        PH7_MOD_PUBLIC, "mixed $key", "@mixed", vm_builtin_SplStore_offsetGet },` |
|     - |  1964 | `		{ "offsetSet",        PH7_MOD_PUBLIC, "mixed $key, mixed $value", "@void", vm_builtin_SplStore_offsetSet },` |
|     - |  1965 | `		{ "offsetUnset",      PH7_MOD_PUBLIC, "mixed $key", "@void", vm_builtin_SplStore_offsetUnset },` |
|     - |  1966 | `		{ "append",           PH7_MOD_PUBLIC, "mixed $value", "@void", vm_builtin_SplStore_append },` |
|     - |  1967 | `		{ "getArrayCopy",     PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplStore_getArrayCopy },` |
|     - |  1968 | `		{ "count",            PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplStore_count },` |
|     - |  1969 | `		{ "getFlags",         PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplStore_getFlags },` |
|     - |  1970 | `		{ "setFlags",         PH7_MOD_PUBLIC, "int $flags", "@void", vm_builtin_SplStore_setFlags },` |
|     - |  1971 | `		{ "asort",            PH7_MOD_PUBLIC, "int $flags = SORT_REGULAR", "@true", vm_builtin_SplStore_asort },` |
|     - |  1972 | `		{ "ksort",            PH7_MOD_PUBLIC, "int $flags = SORT_REGULAR", "@true", vm_builtin_SplStore_ksort },` |
|     - |  1973 | `		{ "uasort",           PH7_MOD_PUBLIC, "callable $callback", "@true", vm_builtin_SplStore_uasort },` |
|     - |  1974 | `		{ "uksort",           PH7_MOD_PUBLIC, "callable $callback", "@true", vm_builtin_SplStore_uksort },` |
|     - |  1975 | `		{ "natsort",          PH7_MOD_PUBLIC, "", "@true", vm_builtin_SplStore_natsort },` |
|     - |  1976 | `		{ "natcasesort",      PH7_MOD_PUBLIC, "", "@true", vm_builtin_SplStore_natcasesort },` |
|     - |  1977 | `		{ "unserialize",      PH7_MOD_PUBLIC, "string $data", "@void", vm_builtin_SplStore_unserialize },` |
|     - |  1978 | `		{ "serialize",        PH7_MOD_PUBLIC, "", "@string", vm_builtin_SplStore_serialize },` |
|     - |  1979 | `		{ "__serialize",      PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplStore_serializeMagic },` |
|     - |  1980 | `		{ "__unserialize",    PH7_MOD_PUBLIC, "array $data", "@void",` |
|     - |  1981 | `		  vm_builtin_SplStore_unserializeMagic },` |
|     - |  1982 | `		{ "getIterator",      PH7_MOD_PUBLIC, "", "@Iterator", vm_builtin_ArrayObject_getIterator },` |
|     - |  1983 | `		{ "exchangeArray",    PH7_MOD_PUBLIC, "object\|array $array", "@array", vm_builtin_ArrayObject_exchangeArray },` |
|     - |  1984 | `		{ "setIteratorClass", PH7_MOD_PUBLIC, "~string $iteratorClass", "@void", vm_builtin_ArrayObject_setIteratorClass },` |
|     - |  1985 | `		{ "getIteratorClass", PH7_MOD_PUBLIC, "", "@string", vm_builtin_ArrayObject_getIteratorClass },` |
|     - |  1986 | `		{ "__debugInfo",      PH7_MOD_PUBLIC, "", 0, vm_builtin_SplStore_debugInfo },` |
|     - |  1987 | `	};` |
|     - |  1988 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - |  1989 | ``		/* `interface X extends Iterator` is a PARENT, not an implemented interface:`` |
|     - |  1990 | `		 * the compiler puts it in pBase and Reflection walks pBase to answer which` |
|     - |  1991 | `		 * class DECLARED an inherited method. Naming it in zImplements instead made` |
|     - |  1992 | `		 * current()/key()/next()/rewind()/valid() report this interface as their` |
|     - |  1993 | `		 * declaring class where php reports Iterator. */` |
|     - |  1994 | `		{ "SeekableIterator", "Iterator", 0, PH7_CLASS_INTERFACE,` |
|     - |  1995 | `		  aSeekMethod, SX_ARRAYSIZE(aSeekMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  1996 | `		/* PH7_CLASS_DIM_WRITABLE: php's spl_array read_dimension hands back the` |
|     - |  1997 | ``		 * REAL element for a write fetch, so `$ao['k']['n'] = v` lands — unlike`` |
|     - |  1998 | `		 * SplFixedArray / SplDoublyLinkedList / SplObjectStorage, which keep the` |
|     - |  1999 | `		 * standard handler and get php's indirect-modification notice. */` |
|     - |  2000 | `		{ "ArrayIterator", 0, "SeekableIterator,ArrayAccess,Serializable,Countable", PH7_CLASS_DIM_WRITABLE,` |
|     - |  2001 | `		  aItMethod, SX_ARRAYSIZE(aItMethod), aConst, SX_ARRAYSIZE(aConst),` |
|     - |  2002 | `		  aItProp, SX_ARRAYSIZE(aItProp), 0, 0, SplStorePresent },` |
|     - |  2003 | `		{ "ArrayObject", 0, "IteratorAggregate,ArrayAccess,Serializable,Countable", PH7_CLASS_DIM_WRITABLE,` |
|     - |  2004 | `		  aObjMethod, SX_ARRAYSIZE(aObjMethod), aConst, SX_ARRAYSIZE(aConst),` |
|     - |  2005 | `		  aObjProp, SX_ARRAYSIZE(aObjProp), 0, 0, SplStorePresent },` |
|     - |  2006 | `	};` |
|  6726 |  2007 | `	sxi32 rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|  6726 |  2008 | `	if( rc == SXRET_OK ){` |
|     - |  2009 | `		/* The property handler, assigned here for the same reason the clone and` |
|     - |  2010 | `		 * dimension hooks are: PH7_NativeClassSpec carries no field for one. Both` |
|     - |  2011 | `		 * roots wear it, and RecursiveArrayIterator reaches ArrayIterator's through` |
|     - |  2012 | `		 * the engine's base-chain walk -- php's handler inheritance. */` |
|  6726 |  2013 | `		PH7_NativeClassInstallPropHook(&(*pVm),"ArrayObject",SplArrayProp);` |
|  6726 |  2014 | `		PH7_NativeClassInstallPropHook(&(*pVm),"ArrayIterator",SplArrayProp);` |
|  3356 |  2015 | `	}` |
|  6726 |  2016 | `	return rc;` |
|     5 |  2017 | `}` |
|     - |  2018 | `/*` |
|     - |  2019 | ` * ---------------------------------------------------------------------------` |
|     - |  2020 | ` * The SPL DUAL ITERATORS: IteratorIterator and the decorators built on it.` |
|     - |  2021 | ` *` |
|     - |  2022 | `` * php's `spl_dual_it_object` is a CACHE, and that is the whole design. rewind()`` |
|     - |  2023 | ` * and next() move the INNER iterator and then COPY its current()/key() onto the` |
|     - |  2024 | ` * decorator; valid(), current() and key() answer out of that copy and never reach` |
|     - |  2025 | ` * the inner iterator again. The chunk forwarded all five live, which is three` |
|     - |  2026 | ` * observable divergences at once: a fresh decorator was valid() BEFORE rewind()` |
|     - |  2027 | ` * (php answers false — nothing has been fetched yet), current() followed an inner` |
|     - |  2028 | ` * iterator that had been moved behind the decorator's back (php answers what it` |
|     - |  2029 | ` * cached), and a decorator left past the end still answered the inner's stale` |
|     - |  2030 | ` * key(). Everything below is written around the cache because the cache IS the` |
|     - |  2031 | ` * class.` |
|     - |  2032 | ` *` |
|     - |  2033 | `` * Two slots hold what php holds in two fields: `__in` is `inner.zobject` — the`` |
|     - |  2034 | `` * object getInnerIterator() answers — and `__it` is `inner.iterator`, the Iterator`` |
|     - |  2035 | ` * actually driven. They differ for exactly one input: an IteratorAggregate whose` |
|     - |  2036 | ` * getIterator() answers another IteratorAggregate. php unwraps ONE level in the` |
|     - |  2037 | ` * constructor and lets the engine's get_iterator handler unwrap the rest at` |
|     - |  2038 | `` * iteration time, so `new IteratorIterator($aggOfAgg)` answers the inner AGGREGATE`` |
|     - |  2039 | `` * from getInnerIterator() and still iterates. The chunk's `while` loop unwrapped`` |
|     - |  2040 | ` * to the bottom and answered the ArrayIterator instead.` |
|     - |  2041 | ` */` |
|     - |  2042 | `#define IT_IN  "__in"   /* php's inner.zobject: what getInnerIterator() answers */` |
|     - |  2043 | `#define IT_IT  "__it"   /* php's inner.iterator: the Iterator actually driven */` |
|     - |  2044 | `#define IT_CD  "__cd"   /* the cached current() */` |
|     - |  2045 | `#define IT_CK  "__ck"   /* the cached key() */` |
|     - |  2046 | `#define IT_CF  "__cf"   /* 1 while the cached pair is live (php's IS_UNDEF check) */` |
|     - |  2047 | `#define IT_CP  "__cp"   /* php's current.pos */` |
|     - |  2048 | `#define IT_OFF "__off"  /* LimitIterator's offset */` |
|     - |  2049 | `#define IT_LIM "__lim"  /* LimitIterator's count, -1 for "all" */` |
|     - |  2050 | `#define IT_CB  "__cb"   /* CallbackFilterIterator's callback */` |
|     - |  2051 | ``#define AP_LIST "__ai"  /* AppendIterator's php `u.append.zarrayit`: the real ArrayIterator`` |
|     - |  2052 | `                         * holding everything append()ed, whose OWN cursor is php's` |
|     - |  2053 | ``                         * `u.append.iterator` -- one position, which is why`` |
|     - |  2054 | `                         * getArrayIterator()->rewind() moves getIteratorIndex(). */` |
|     - |  2055 |  |
|     - |  2056 | `/*` |
|     - |  2057 | ` * php's SPL_FETCH_AND_CHECK_DUAL_IT: a subclass whose constructor never called` |
|     - |  2058 | ` * parent::__construct() has no inner iterator, and php refuses every method on it` |
|     - |  2059 | ` * rather than answering a null-flavoured nothing.` |
|     - |  2060 | ` */` |
|    34 |  2061 | `static sxi32 DualNotReady(ph7_context *pCtx)` |
|     1 |  2062 | `{` |
|    35 |  2063 | `	return PH7_VmThrowException(pCtx,"Error",` |
|     - |  2064 | `		"The object is in an invalid state as the parent constructor was not called");` |
|     1 |  2065 | `}` |
|  5060 |  2066 | `static ph7_class_instance * DualDriver(ph7_class_instance *pThis)` |
|     3 |  2067 | `{` |
|  5063 |  2068 | `	return pThis ? PH7_NativeAttrObj(pThis,IT_IT) : 0;` |
|     3 |  2069 | `}` |
|  1966 |  2070 | `static int DualFilled(ph7_class_instance *pThis)` |
|     3 |  2071 | `{` |
|  1969 |  2072 | `	return pThis && PH7_NativeAttrInt(pThis,IT_CF) != 0;` |
|     3 |  2073 | `}` |
|     - |  2074 | `/*` |
|     - |  2075 | `` * php's SPL_FETCH_AND_CHECK_DUAL_IT tests `dit_type`, which means "the constructor`` |
|     - |  2076 | ` * ran" -- and for every decorator but one that is the same thing as "an inner` |
|     - |  2077 | ` * iterator exists". AppendIterator's constructor takes NO iterator: it builds an` |
|     - |  2078 | ` * empty list and is immediately usable (valid() false, current()/key() null, no` |
|     - |  2079 | ` * refusal), so its readiness lives in the list slot instead.` |
|     - |  2080 | ` */` |
|  2706 |  2081 | `static int DualReady(ph7_class_instance *pThis)` |
|     3 |  2082 | `{` |
|  4132 |  2083 | `	return pThis && (PH7_NativeAttrObj(pThis,IT_IT) != 0` |
|  1423 |  2084 | `		\|\| PH7_NativeAttrObj(pThis,AP_LIST) != 0);` |
|     3 |  2085 | `}` |
|     - |  2086 | `/*` |
|     - |  2087 | ` * Assign one of the instance's own slots. The slot is re-resolved BY NAME here on` |
|     - |  2088 | ` * purpose: a call into user code (and every inner->current() is one) can unset and` |
|     - |  2089 | ` * re-create the property underneath, which gives it a different pool slot. It used` |
|     - |  2090 | ` * to matter for a second reason as well -- the pool itself reallocated as the VM` |
|     - |  2091 | ` * reserved objects -- and that one is gone since P1 (fixed segments).` |
|     - |  2092 | ` */` |
|  1464 |  2093 | `static void DualSetSlot(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,ph7_value *pVal)` |
|     3 |  2094 | `{` |
|  1467 |  2095 | `	ph7_value *pSlot = PH7_NativeAttr(pThis,zName);` |
|   732 |  2096 | `	SXUNUSED(pVm);` |
|  1467 |  2097 | `	if( pSlot ){` |
|  1467 |  2098 | `		PH7_MemObjStore(pVal,pSlot);` |
|   732 |  2099 | `	}` |
|  1467 |  2100 | `}` |
|     - |  2101 | `/* php's spl_dual_it_free: drop the cached pair. */` |
|  1728 |  2102 | `static void DualFree(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     3 |  2103 | `{` |
|     - |  2104 | `	ph7_value *pSlot;` |
|  1731 |  2105 | `	if( pThis == 0 ){` |
|   ! 0 |  2106 | `		return;` |
|     - |  2107 | `	}` |
|  1731 |  2108 | `	pSlot = PH7_NativeAttr(pThis,IT_CD);` |
|  1731 |  2109 | `	if( pSlot ){ PH7_MemObjRelease(pSlot); }` |
|  1731 |  2110 | `	pSlot = PH7_NativeAttr(pThis,IT_CK);` |
|  1731 |  2111 | `	if( pSlot ){ PH7_MemObjRelease(pSlot); }` |
|  1731 |  2112 | `	PH7_NativeSetAttrInt(pVm,pThis,IT_CF,0);` |
|   867 |  2113 | `}` |
|     - |  2114 | `/* Call a zero-argument method on the driven iterator, propagating a throw (rule:` |
|     - |  2115 | ` * a native body that answers PH7_OK with an exception in flight lets the caller` |
|     - |  2116 | ` * carry on). A missing method is the foreach opcode's leniency, not an error. */` |
|  3494 |  2117 | `static sxi32 DualCall(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nLen,` |
|     - |  2118 | `	ph7_value *pResult)` |
|     3 |  2119 | `{` |
|  3497 |  2120 | `	ph7_class_instance *pIn = DualDriver(pThis);` |
|  3497 |  2121 | `	if( pIn == 0 ){` |
|    43 |  2122 | `		return SXRET_OK;` |
|     - |  2123 | `	}` |
|  3455 |  2124 | `	return VmIterCallMethod(pVm,pIn,zName,nLen,pResult);` |
|  1750 |  2125 | `}` |
|     - |  2126 | `/* php's spl_dual_it_valid: the INNER's valid(), not the cache's. */` |
|  1234 |  2127 | `static sxi32 DualInnerValid(ph7_vm *pVm,ph7_class_instance *pThis,int *pbValid)` |
|     3 |  2128 | `{` |
|     - |  2129 | `	ph7_value sVal;` |
|     - |  2130 | `	sxi32 rc;` |
|  1237 |  2131 | `	*pbValid = 0;` |
|  1237 |  2132 | `	PH7_MemObjInit(pVm,&sVal);` |
|  1237 |  2133 | `	rc = DualCall(pVm,pThis,"valid",sizeof("valid")-1,&sVal);` |
|  1237 |  2134 | `	if( rc == SXRET_OK ){` |
|  1237 |  2135 | `		PH7_MemObjToBool(&sVal);          /* a STATUS, not the answer */` |
|  1237 |  2136 | `		*pbValid = sVal.x.iVal != 0;` |
|   617 |  2137 | `	}` |
|  1237 |  2138 | `	PH7_MemObjRelease(&sVal);` |
|  1237 |  2139 | `	return rc;` |
|     3 |  2140 | `}` |
|     - |  2141 | `/*` |
|     - |  2142 | ``  * php's spl_dual_it_fetch: refill the cache from the inner iterator. `bCheckMore` `` |
|     - |  2143 | ` * is php's check_more — false means "the caller already knows the inner is valid",` |
|     - |  2144 | ` * which is how LimitIterator's seek and InfiniteIterator's wrap-around fetch.` |
|     - |  2145 | ` */` |
|   862 |  2146 | `static sxi32 DualFetch(ph7_vm *pVm,ph7_class_instance *pThis,int bCheckMore)` |
|     3 |  2147 | `{` |
|     - |  2148 | `	ph7_value sVal;` |
|     - |  2149 | `	sxi32 rc;` |
|   865 |  2150 | `	int bValid = 1;` |
|   865 |  2151 | `	DualFree(pVm,pThis);` |
|   865 |  2152 | `	if( DualDriver(pThis) == 0 ){` |
|     5 |  2153 | `		return SXRET_OK;` |
|     - |  2154 | `	}` |
|   861 |  2155 | `	if( bCheckMore ){` |
|   743 |  2156 | `		rc = DualInnerValid(pVm,pThis,&bValid);` |
|   743 |  2157 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  2158 | `			return rc;` |
|     - |  2159 | `		}` |
|   370 |  2160 | `	}` |
|   861 |  2161 | `	if( !bValid ){` |
|   193 |  2162 | `		return SXRET_OK;` |
|     - |  2163 | `	}` |
|   671 |  2164 | `	PH7_MemObjInit(pVm,&sVal);` |
|   671 |  2165 | `	rc = DualCall(pVm,pThis,"current",sizeof("current")-1,&sVal);` |
|   671 |  2166 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  2167 | `		PH7_MemObjRelease(&sVal);` |
|   ! 0 |  2168 | `		return rc;` |
|     - |  2169 | `	}` |
|   671 |  2170 | `	DualSetSlot(pVm,pThis,IT_CD,&sVal);` |
|   671 |  2171 | `	PH7_MemObjRelease(&sVal);` |
|   671 |  2172 | `	PH7_MemObjInit(pVm,&sVal);` |
|   671 |  2173 | `	rc = DualCall(pVm,pThis,"key",sizeof("key")-1,&sVal);` |
|   671 |  2174 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  2175 | `		PH7_MemObjRelease(&sVal);` |
|   ! 0 |  2176 | `		DualFree(pVm,pThis);   /* php drops the half-filled pair when key() throws */` |
|   ! 0 |  2177 | `		return rc;` |
|     - |  2178 | `	}` |
|   671 |  2179 | `	DualSetSlot(pVm,pThis,IT_CK,&sVal);` |
|   671 |  2180 | `	PH7_MemObjRelease(&sVal);` |
|   671 |  2181 | `	PH7_NativeSetAttrInt(pVm,pThis,IT_CF,1);` |
|   671 |  2182 | `	return SXRET_OK;` |
|   434 |  2183 | `}` |
|     - |  2184 | `/* php's spl_dual_it_rewind: free, position back to zero, rewind the inner. */` |
|   370 |  2185 | `static sxi32 DualRewindInner(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     3 |  2186 | `{` |
|   373 |  2187 | `	DualFree(pVm,pThis);` |
|   373 |  2188 | `	PH7_NativeSetAttrInt(pVm,pThis,IT_CP,0);` |
|   373 |  2189 | `	return DualCall(pVm,pThis,"rewind",sizeof("rewind")-1,0);` |
|     3 |  2190 | `}` |
|     - |  2191 | `/*` |
|     - |  2192 | ``  * php's spl_dual_it_next: free, advance the inner, count the step. Its `do_free` `` |
|     - |  2193 | ` * is FALSE for exactly one caller — CachingIterator, which has just copied the` |
|     - |  2194 | ` * pair it is standing on and steps the inner one ahead of it, so dropping the` |
|     - |  2195 | ` * cache here would erase the very element the decorator answers.` |
|     - |  2196 | ` */` |
|   440 |  2197 | `static sxi32 DualNextInnerEx(ph7_vm *pVm,ph7_class_instance *pThis,int bFree)` |
|     3 |  2198 | `{` |
|     - |  2199 | `	sxi32 rc;` |
|   443 |  2200 | `	if( bFree ){` |
|   257 |  2201 | `		DualFree(pVm,pThis);` |
|   128 |  2202 | `	}` |
|   443 |  2203 | `	rc = DualCall(pVm,pThis,"next",sizeof("next")-1,0);` |
|   443 |  2204 | `	PH7_NativeSetAttrInt(pVm,pThis,IT_CP,PH7_NativeAttrInt(pThis,IT_CP)+1);` |
|   443 |  2205 | `	return rc;` |
|     3 |  2206 | `}` |
|   256 |  2207 | `static sxi32 DualNextInner(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  2208 | `{` |
|   257 |  2209 | `	return DualNextInnerEx(pVm,pThis,TRUE);` |
|     1 |  2210 | `}` |
|     - |  2211 | `/* Hand back a cached slot, or php's null for an empty cache. */` |
|   738 |  2212 | `static int DualResultSlot(ph7_context *pCtx,const char *zName)` |
|     3 |  2213 | `{` |
|   741 |  2214 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2215 | `	ph7_value *pSlot;` |
|   741 |  2216 | `	if( !DualReady(pThis) ){` |
|   ! 0 |  2217 | `		return DualNotReady(pCtx);` |
|     - |  2218 | `	}` |
|   741 |  2219 | `	if( !DualFilled(pThis) ){` |
|    27 |  2220 | `		ph7_result_null(pCtx);` |
|    27 |  2221 | `		return PH7_OK;` |
|     - |  2222 | `	}` |
|   715 |  2223 | `	pSlot = PH7_NativeAttr(pThis,zName);` |
|   715 |  2224 | `	if( pSlot ){` |
|   715 |  2225 | `		ph7_result_value(pCtx,pSlot);` |
|   356 |  2226 | `	}` |
|   715 |  2227 | `	return PH7_OK;` |
|   372 |  2228 | `}` |
|     - |  2229 | `/*` |
|     - |  2230 | ` * The constructor every dual iterator shares. php words the "already built" refusal` |
|     - |  2231 | ` * with the DECLARING class's name and with getIterator() rather than __construct(),` |
|     - |  2232 | ` * so each class hands its own name in.` |
|     - |  2233 | ` */` |
|   442 |  2234 | `static sxi32 DualConstruct(ph7_context *pCtx,const char *zOwner,int nArg,ph7_value **apArg)` |
|     3 |  2235 | `{` |
|   445 |  2236 | `	ph7_vm *pVm = pCtx->pVm;` |
|   445 |  2237 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2238 | `	ph7_class_instance *pObj;` |
|     - |  2239 | `	ph7_class *pIterCls, *pAggCls, *pTravCls;` |
|   445 |  2240 | `	ph7_class *pCast = 0;` |
|   445 |  2241 | `	ph7_class_instance *pHold = 0;   /* the unwrapped iterator, kept alive across levels */` |
|     - |  2242 | `	int nLevel;` |
|   445 |  2243 | `	if( pThis == 0 ){` |
|   ! 0 |  2244 | `		return PH7_OK;` |
|     - |  2245 | `	}` |
|   445 |  2246 | `	if( PH7_NativeAttrObj(pThis,IT_IN) != 0 ){` |
|     4 |  2247 | `		return PH7_VmThrowException(pCtx,"BadMethodCallException",` |
|     1 |  2248 | `			"%s::getIterator() must be called exactly once per instance",zOwner);` |
|     - |  2249 | `	}` |
|   443 |  2250 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 |  2251 | `		return PH7_OK;   /* the shared ZPP screen already refused a non-object */` |
|     - |  2252 | `	}` |
|   443 |  2253 | `	pObj = (ph7_class_instance *)apArg[0]->x.pOther;` |
|   443 |  2254 | `	pIterCls = PH7_VmExtractClass(pVm,"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|   443 |  2255 | `	pAggCls = PH7_VmExtractClass(pVm,"IteratorAggregate",sizeof("IteratorAggregate")-1,FALSE,0);` |
|   443 |  2256 | `	pTravCls = PH7_VmExtractClass(pVm,"Traversable",sizeof("Traversable")-1,FALSE,0);` |
|   443 |  2257 | `	if( pIterCls && PH7_VmInstanceOf(pObj->pClass,pIterCls) ){` |
|     - |  2258 | `		/* Already an Iterator: php ignores $class entirely on this path. */` |
|   433 |  2259 | `		PH7_NativeSetAttrObj(pVm,pThis,IT_IN,pObj);` |
|   433 |  2260 | `		PH7_NativeSetAttrObj(pVm,pThis,IT_IT,pObj);` |
|   433 |  2261 | `		return PH7_OK;` |
|     - |  2262 | `	}` |
|    11 |  2263 | `	if( nArg > 1 && (apArg[1]->iFlags & MEMOBJ_NULL) == 0 ){` |
|     - |  2264 | `		/* php's DOWNCAST: $class names the class whose getIterator() to run, which is` |
|     - |  2265 | `		 * how a subclass asks for its parent's traversal. It must be a base of the` |
|     - |  2266 | `		 * argument AND traversable itself. */` |
|     - |  2267 | `		int nName;` |
|     5 |  2268 | `		const char *zName = ph7_value_to_string(apArg[1],&nName);` |
|     5 |  2269 | `		pCast = nName > 0 ? PH7_VmExtractClass(pVm,zName,(sxu32)nName,FALSE,0) : 0;` |
|     4 |  2270 | `		if( pCast == 0 \|\| !PH7_VmInstanceOf(pObj->pClass,pCast)` |
|     3 |  2271 | `		 \|\| (pTravCls && !PH7_VmInstanceOf(pCast,pTravCls)) ){` |
|     5 |  2272 | `			return PH7_VmThrowException(pCtx,"LogicException",` |
|     - |  2273 | `				"Class to downcast to not found or not base class or does not implement Traversable");` |
|     - |  2274 | `		}` |
|   ! 0 |  2275 | `	}` |
|     - |  2276 | `	/*` |
|     - |  2277 | `	 * An IteratorAggregate: run getIterator() — the DOWNCAST class's when one was` |
|     - |  2278 | `	 * named — and keep its answer as the inner object. php stops after one level` |
|     - |  2279 | `	 * here; the loop below is the engine's get_iterator handler, which resolves the` |
|     - |  2280 | `	 * rest lazily, done eagerly because PHL drives the inner through the METHOD` |
|     - |  2281 | `	 * protocol and nothing else would unwrap it.` |
|     - |  2282 | `	 */` |
|     9 |  2283 | `	for( nLevel = 0 ; nLevel < 16 ; ++nLevel ){` |
|     9 |  2284 | `		ph7_class *pFrom = pCast ? pCast : pObj->pClass;` |
|     - |  2285 | `		ph7_class_method *pMethod;` |
|     - |  2286 | `		ph7_value sInner;` |
|     - |  2287 | `		sxi32 rc;` |
|     9 |  2288 | `		if( pAggCls == 0 \|\| !PH7_VmInstanceOf(pObj->pClass,pAggCls) ){` |
|   ! 0 |  2289 | `			break;` |
|     - |  2290 | `		}` |
|     9 |  2291 | `		pMethod = PH7_ClassExtractMethod(pFrom,"getIterator",sizeof("getIterator")-1);` |
|     9 |  2292 | `		if( pMethod == 0 ){` |
|   ! 0 |  2293 | `			break;` |
|     - |  2294 | `		}` |
|     9 |  2295 | `		PH7_MemObjInit(pVm,&sInner);` |
|     9 |  2296 | `		rc = PH7_VmCallClassMethod(pVm,pObj,pMethod,&sInner,0,0);` |
|     9 |  2297 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  2298 | `			PH7_MemObjRelease(&sInner);` |
|   ! 0 |  2299 | `			return rc;` |
|     - |  2300 | `		}` |
|     8 |  2301 | `		if( (sInner.iFlags & MEMOBJ_OBJ) == 0 \|\| sInner.x.pOther == 0` |
|     9 |  2302 | `		 \|\| (pTravCls && !PH7_VmInstanceOf(((ph7_class_instance *)sInner.x.pOther)->pClass,pTravCls)) ){` |
|   ! 0 |  2303 | `			SyString *pName = &pFrom->sName;` |
|   ! 0 |  2304 | `			PH7_MemObjRelease(&sInner);` |
|   ! 0 |  2305 | `			return PH7_VmThrowException(pCtx,"LogicException",` |
|   ! 0 |  2306 | `				"%z::getIterator() must return an object that implements Traversable",pName);` |
|     - |  2307 | `		}` |
|     9 |  2308 | `		pObj = (ph7_class_instance *)sInner.x.pOther;` |
|     9 |  2309 | `		pObj->iRef++;                 /* survive the release of the call result */` |
|     9 |  2310 | `		PH7_MemObjRelease(&sInner);` |
|     9 |  2311 | `		if( pHold ){` |
|     3 |  2312 | `			PH7_ClassInstanceUnref(pHold);` |
|     1 |  2313 | `		}` |
|     9 |  2314 | `		pHold = pObj;                 /* this function owns exactly one reference */` |
|     9 |  2315 | `		if( nLevel == 0 ){` |
|     - |  2316 | `			/* php's inner.zobject is the FIRST unwrap and nothing deeper. */` |
|     7 |  2317 | `			PH7_NativeSetAttrObj(pVm,pThis,IT_IN,pObj);` |
|     3 |  2318 | `		}` |
|     9 |  2319 | `		pCast = 0;` |
|     9 |  2320 | `		if( pIterCls && PH7_VmInstanceOf(pObj->pClass,pIterCls) ){` |
|     7 |  2321 | `			break;` |
|     - |  2322 | `		}` |
|     2 |  2323 | `	}` |
|     7 |  2324 | `	if( PH7_NativeAttrObj(pThis,IT_IN) == 0 ){` |
|   ! 0 |  2325 | `		PH7_NativeSetAttrObj(pVm,pThis,IT_IN,pObj);` |
|   ! 0 |  2326 | `	}` |
|     7 |  2327 | `	PH7_NativeSetAttrObj(pVm,pThis,IT_IT,pObj);` |
|     7 |  2328 | `	if( pHold ){` |
|     7 |  2329 | `		PH7_ClassInstanceUnref(pHold);   /* both slots hold their own now */` |
|     3 |  2330 | `	}` |
|     7 |  2331 | `	return PH7_OK;` |
|   224 |  2332 | `}` |
|    44 |  2333 | `static int vm_builtin_IteratorIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2334 | `{` |
|    45 |  2335 | `	return DualConstruct(pCtx,"IteratorIterator",nArg,apArg);` |
|     1 |  2336 | `}` |
|     6 |  2337 | `static int vm_builtin_FilterIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2338 | `{` |
|     7 |  2339 | `	return DualConstruct(pCtx,"FilterIterator",nArg,apArg);` |
|     1 |  2340 | `}` |
|    10 |  2341 | `static int vm_builtin_CallbackFilterIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2342 | `{` |
|     - |  2343 | `	ph7_class_instance *pThis;` |
|     - |  2344 | `	sxi32 rc;` |
|    11 |  2345 | `	if( nArg > 1 ){` |
|     - |  2346 | ``		/* The shared ZPP screen leaves `callable` to the builtin's own check (a string`` |
|     - |  2347 | `		 * satisfies the declared type; whether it NAMES a function does not), so php's` |
|     - |  2348 | `		 * "must be a valid callback, function "x" not found" only appears if the body` |
|     - |  2349 | `		 * asks for it — as every callback-taking builtin already does. */` |
|    11 |  2350 | `		rc = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",FALSE);` |
|    11 |  2351 | `		if( rc != PH7_OK ){` |
|     3 |  2352 | `			return rc;` |
|     - |  2353 | `		}` |
|     4 |  2354 | `	}` |
|     9 |  2355 | `	rc = DualConstruct(pCtx,"CallbackFilterIterator",nArg,apArg);` |
|     9 |  2356 | `	pThis = PH7_ContextThis(pCtx);` |
|     9 |  2357 | `	if( rc == PH7_OK && pThis && nArg > 1 ){` |
|     9 |  2358 | `		DualSetSlot(pCtx->pVm,pThis,IT_CB,apArg[1]);` |
|     4 |  2359 | `	}` |
|     9 |  2360 | `	return rc;` |
|     6 |  2361 | `}` |
|    10 |  2362 | `static int vm_builtin_InfiniteIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2363 | `{` |
|    11 |  2364 | `	return DualConstruct(pCtx,"InfiniteIterator",nArg,apArg);` |
|     1 |  2365 | `}` |
|    10 |  2366 | `static int vm_builtin_NoRewindIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2367 | `{` |
|    11 |  2368 | `	return DualConstruct(pCtx,"NoRewindIterator",nArg,apArg);` |
|     1 |  2369 | `}` |
|     - |  2370 | `/*` |
|     - |  2371 | `` * php's `spl_dual_it_call_method`: every iterator that extends IteratorIterator --`` |
|     - |  2372 | ` * FilterIterator and its whole family, LimitIterator, CachingIterator,` |
|     - |  2373 | ` * NoRewindIterator, InfiniteIterator, RegexIterator, AppendIterator -- forwards a` |
|     - |  2374 | ` * method it does not itself have to the iterator it WRAPS. Symfony's Finder is built` |
|     - |  2375 | ``  * on that: `ExcludeDirectoryFilterIterator::accept()` calls `$this->getFilename()` `` |
|     - |  2376 | `` * and means the RecursiveDirectoryIterator's, so composer's `dump-autoload` stopped`` |
|     - |  2377 | `` * with `Call to undefined method …::getFilename()` without it.`` |
|     - |  2378 | ` *` |
|     - |  2379 | `` * It is NOT a `__call` method: php's is an internal handler, so`` |
|     - |  2380 | `` * `method_exists($it,'__call')` is false and `get_class_methods()` never lists one.`` |
|     - |  2381 | ` * RecursiveIteratorIterator, RecursiveTreeIterator and MultipleIterator are not dual` |
|     - |  2382 | ` * iterators and forward nothing -- which is why the test is the CLASS, not the` |
|     - |  2383 | ` * presence of an inner slot.` |
|     - |  2384 | ` *` |
|     - |  2385 | ` * Answers 1 with *ppInner / *ppMeth filled when the call should be re-targeted.` |
|     - |  2386 | ` */` |
|   116 |  2387 | `PH7_PRIVATE int PH7_SplOuterForward(ph7_vm *pVm,ph7_class_instance *pThis,SyString *pName,` |
|     - |  2388 | `	ph7_class_instance **ppInner,ph7_class_method **ppMeth)` |
|     3 |  2389 | `{` |
|     - |  2390 | `	ph7_class *pDual;` |
|     - |  2391 | `	ph7_class_instance *pInner;` |
|     - |  2392 | `	ph7_class_method *pMeth;` |
|   119 |  2393 | `	if( pThis == 0 \|\| pName == 0 \|\| pName->nByte < 1 ){` |
|   ! 0 |  2394 | `		return 0;` |
|     - |  2395 | `	}` |
|   119 |  2396 | `	pDual = PH7_VmExtractClass(pVm,"IteratorIterator",sizeof("IteratorIterator")-1,TRUE,0);` |
|   119 |  2397 | `	if( pDual == 0 \|\| pThis->pClass == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pDual) ){` |
|    75 |  2398 | `		return 0;` |
|     - |  2399 | `	}` |
|     - |  2400 | ``	/* php forwards to `inner.zobject` -- what getInnerIterator() answers -- and an`` |
|     - |  2401 | `	 * instance that has none (a fresh AppendIterator) forwards nothing. */` |
|    45 |  2402 | `	pInner = PH7_NativeAttrObj(pThis,IT_IN);` |
|    45 |  2403 | `	if( pInner == 0 \|\| pInner->pClass == 0 ){` |
|     3 |  2404 | `		return 0;` |
|     - |  2405 | `	}` |
|    43 |  2406 | `	pMeth = PH7_ClassExtractMethod(pInner->pClass,pName->zString,pName->nByte);` |
|    43 |  2407 | `	if( pMeth == 0 ){` |
|     - |  2408 | `		/* php reports the OUTER class in that case, which is what the caller's own` |
|     - |  2409 | `		 * undefined-method path already says. */` |
|    19 |  2410 | `		return 0;` |
|     - |  2411 | `	}` |
|    25 |  2412 | `	*ppInner = pInner;` |
|    25 |  2413 | `	*ppMeth = pMeth;` |
|    25 |  2414 | `	return 1;` |
|    61 |  2415 | `}` |
|    42 |  2416 | `static int vm_builtin_Dual_getInnerIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2417 | `{` |
|    43 |  2418 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2419 | `	ph7_class_instance *pIn;` |
|    21 |  2420 | `	SXUNUSED(nArg);` |
|    21 |  2421 | `	SXUNUSED(apArg);` |
|    43 |  2422 | `	if( !DualReady(pThis) ){` |
|   ! 0 |  2423 | `		return DualNotReady(pCtx);` |
|     - |  2424 | `	}` |
|    43 |  2425 | `	pIn = PH7_NativeAttrObj(pThis,IT_IN);` |
|    43 |  2426 | `	if( pIn ){` |
|    41 |  2427 | `		SplResultBorrowed(pCtx,pIn);` |
|    21 |  2428 | `	}else{` |
|     3 |  2429 | `		ph7_result_null(pCtx);` |
|     - |  2430 | `	}` |
|    43 |  2431 | `	return PH7_OK;` |
|    22 |  2432 | `}` |
|   430 |  2433 | `static int vm_builtin_Dual_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2434 | `{` |
|   431 |  2435 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   215 |  2436 | `	SXUNUSED(nArg);` |
|   215 |  2437 | `	SXUNUSED(apArg);` |
|   431 |  2438 | `	if( !DualReady(pThis) ){` |
|     3 |  2439 | `		return DualNotReady(pCtx);` |
|     - |  2440 | `	}` |
|   429 |  2441 | `	ph7_result_bool(pCtx,DualFilled(pThis));` |
|   429 |  2442 | `	return PH7_OK;` |
|   216 |  2443 | `}` |
|   400 |  2444 | `static int vm_builtin_Dual_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  2445 | `{` |
|   200 |  2446 | `	SXUNUSED(nArg);` |
|   200 |  2447 | `	SXUNUSED(apArg);` |
|   403 |  2448 | `	return DualResultSlot(pCtx,IT_CD);` |
|     3 |  2449 | `}` |
|   274 |  2450 | `static int vm_builtin_Dual_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2451 | `{` |
|   137 |  2452 | `	SXUNUSED(nArg);` |
|   137 |  2453 | `	SXUNUSED(apArg);` |
|   275 |  2454 | `	return DualResultSlot(pCtx,IT_CK);` |
|     1 |  2455 | `}` |
|    22 |  2456 | `static int vm_builtin_IteratorIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2457 | `{` |
|    23 |  2458 | `	ph7_vm *pVm = pCtx->pVm;` |
|    23 |  2459 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2460 | `	sxi32 rc;` |
|    11 |  2461 | `	SXUNUSED(nArg);` |
|    11 |  2462 | `	SXUNUSED(apArg);` |
|    23 |  2463 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  2464 | `		return DualNotReady(pCtx);` |
|     - |  2465 | `	}` |
|    23 |  2466 | `	rc = DualRewindInner(pVm,pThis);` |
|    23 |  2467 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  2468 | `		return rc;` |
|     - |  2469 | `	}` |
|    23 |  2470 | `	return DualFetch(pVm,pThis,TRUE);` |
|    12 |  2471 | `}` |
|    22 |  2472 | `static int vm_builtin_IteratorIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2473 | `{` |
|    23 |  2474 | `	ph7_vm *pVm = pCtx->pVm;` |
|    23 |  2475 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2476 | `	sxi32 rc;` |
|    11 |  2477 | `	SXUNUSED(nArg);` |
|    11 |  2478 | `	SXUNUSED(apArg);` |
|    23 |  2479 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  2480 | `		return DualNotReady(pCtx);` |
|     - |  2481 | `	}` |
|    23 |  2482 | `	rc = DualNextInner(pVm,pThis);` |
|    23 |  2483 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  2484 | `		return rc;` |
|     - |  2485 | `	}` |
|    23 |  2486 | `	return DualFetch(pVm,pThis,TRUE);` |
|    12 |  2487 | `}` |
|     - |  2488 | `/*` |
|     - |  2489 | ` * FilterIterator. php's spl_filter_it_fetch: fetch, ask accept(), and on a refusal` |
|     - |  2490 | ` * step the INNER on directly — without counting the step, which is why a filtered` |
|     - |  2491 | ` * element does not move current.pos. accept() is called on $this, so a user` |
|     - |  2492 | ` * subclass's body is what decides.` |
|     - |  2493 | ` */` |
|   242 |  2494 | `static sxi32 DualAccept(ph7_vm *pVm,ph7_class_instance *pThis,int *pbAccept)` |
|     1 |  2495 | `{` |
|   243 |  2496 | `	ph7_class_method *pMethod = PH7_ClassExtractMethod(pThis->pClass,"accept",sizeof("accept")-1);` |
|     - |  2497 | `	ph7_value sRes;` |
|     - |  2498 | `	sxi32 rc;` |
|   243 |  2499 | `	*pbAccept = 0;` |
|   243 |  2500 | `	if( pMethod == 0 ){` |
|   ! 0 |  2501 | `		return SXRET_OK;` |
|     - |  2502 | `	}` |
|   243 |  2503 | `	PH7_MemObjInit(pVm,&sRes);` |
|   243 |  2504 | `	rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sRes,0,0);` |
|   243 |  2505 | `	if( rc == SXRET_OK ){` |
|   243 |  2506 | `		PH7_MemObjToBool(&sRes);` |
|   243 |  2507 | `		*pbAccept = sRes.x.iVal != 0;` |
|   121 |  2508 | `	}` |
|   243 |  2509 | `	PH7_MemObjRelease(&sRes);` |
|   243 |  2510 | `	return rc;` |
|   122 |  2511 | `}` |
|   246 |  2512 | `static sxi32 DualFilterFetch(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  2513 | `{` |
|   211 |  2514 | `	for(;;){` |
|   335 |  2515 | `		int bAccept = 0;` |
|   335 |  2516 | `		sxi32 rc = DualFetch(pVm,pThis,TRUE);` |
|   335 |  2517 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  2518 | `			return rc;` |
|     - |  2519 | `		}` |
|   335 |  2520 | `		if( !DualFilled(pThis) ){` |
|    93 |  2521 | `			break;` |
|     - |  2522 | `		}` |
|   243 |  2523 | `		rc = DualAccept(pVm,pThis,&bAccept);` |
|   243 |  2524 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  2525 | `			return rc;` |
|     - |  2526 | `		}` |
|   243 |  2527 | `		if( bAccept ){` |
|   155 |  2528 | `			return SXRET_OK;` |
|     - |  2529 | `		}` |
|    89 |  2530 | `		rc = DualCall(pVm,pThis,"next",sizeof("next")-1,0);` |
|    89 |  2531 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  2532 | `			return rc;` |
|     - |  2533 | `		}` |
|     1 |  2534 | `	}` |
|    93 |  2535 | `	DualFree(pVm,pThis);` |
|    93 |  2536 | `	return SXRET_OK;` |
|   124 |  2537 | `}` |
|   108 |  2538 | `static int vm_builtin_FilterIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2539 | `{` |
|   109 |  2540 | `	ph7_vm *pVm = pCtx->pVm;` |
|   109 |  2541 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2542 | `	sxi32 rc;` |
|    54 |  2543 | `	SXUNUSED(nArg);` |
|    54 |  2544 | `	SXUNUSED(apArg);` |
|   109 |  2545 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  2546 | `		return DualNotReady(pCtx);` |
|     - |  2547 | `	}` |
|   109 |  2548 | `	rc = DualRewindInner(pVm,pThis);` |
|   109 |  2549 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  2550 | `		return rc;` |
|     - |  2551 | `	}` |
|   109 |  2552 | `	return DualFilterFetch(pVm,pThis);` |
|    55 |  2553 | `}` |
|   138 |  2554 | `static int vm_builtin_FilterIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2555 | `{` |
|   139 |  2556 | `	ph7_vm *pVm = pCtx->pVm;` |
|   139 |  2557 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2558 | `	sxi32 rc;` |
|    69 |  2559 | `	SXUNUSED(nArg);` |
|    69 |  2560 | `	SXUNUSED(apArg);` |
|   139 |  2561 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  2562 | `		return DualNotReady(pCtx);` |
|     - |  2563 | `	}` |
|   139 |  2564 | `	rc = DualNextInner(pVm,pThis);` |
|   139 |  2565 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  2566 | `		return rc;` |
|     - |  2567 | `	}` |
|   139 |  2568 | `	return DualFilterFetch(pVm,pThis);` |
|    70 |  2569 | `}` |
|     - |  2570 | `/* CallbackFilterIterator::accept(): the callback sees the CACHED pair and the inner` |
|     - |  2571 | ` * iterator, and an empty cache is refused without calling it at all. */` |
|    38 |  2572 | `static int vm_builtin_CallbackFilterIterator_accept(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2573 | `{` |
|    39 |  2574 | `	ph7_vm *pVm = pCtx->pVm;` |
|    39 |  2575 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2576 | `	ph7_value *apCall[3];` |
|     - |  2577 | `	ph7_value sInner,sRes,*pCb;` |
|     - |  2578 | `	sxi32 rc;` |
|    19 |  2579 | `	SXUNUSED(nArg);` |
|    19 |  2580 | `	SXUNUSED(apArg);` |
|    39 |  2581 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|     3 |  2582 | `		return DualNotReady(pCtx);` |
|     - |  2583 | `	}` |
|    37 |  2584 | `	if( !DualFilled(pThis) ){` |
|   ! 0 |  2585 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  2586 | `		return PH7_OK;` |
|     - |  2587 | `	}` |
|    37 |  2588 | `	pCb = PH7_NativeAttr(pThis,IT_CB);` |
|    37 |  2589 | `	if( pCb == 0 ){` |
|   ! 0 |  2590 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  2591 | `		return PH7_OK;` |
|     - |  2592 | `	}` |
|    37 |  2593 | `	PH7_MemObjInit(pVm,&sInner);` |
|    37 |  2594 | `	sInner.x.pOther = PH7_NativeAttrObj(pThis,IT_IN);` |
|    37 |  2595 | `	if( sInner.x.pOther ){` |
|    37 |  2596 | `		MemObjSetType(&sInner,MEMOBJ_OBJ);` |
|    37 |  2597 | `		((ph7_class_instance *)sInner.x.pOther)->iRef++;` |
|    18 |  2598 | `	}` |
|    37 |  2599 | `	apCall[0] = PH7_NativeAttr(pThis,IT_CD);` |
|    37 |  2600 | `	apCall[1] = PH7_NativeAttr(pThis,IT_CK);` |
|    37 |  2601 | `	apCall[2] = &sInner;` |
|    37 |  2602 | `	PH7_MemObjInit(pVm,&sRes);` |
|    37 |  2603 | `	rc = PH7_VmCallUserFunction(pVm,pCb,3,apCall,&sRes);` |
|    37 |  2604 | `	PH7_MemObjRelease(&sInner);` |
|    37 |  2605 | `	if( rc == SXRET_OK ){` |
|    37 |  2606 | `		ph7_result_value(pCtx,&sRes);` |
|    18 |  2607 | `	}` |
|    37 |  2608 | `	PH7_MemObjRelease(&sRes);` |
|    37 |  2609 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    20 |  2610 | `}` |
|     - |  2611 | `/*` |
|     - |  2612 | ` * LimitIterator. The window is (offset, count) over the inner iterator's own` |
|     - |  2613 | `` * positions, and `__cp` counts them: php's valid() is "inside the window AND the`` |
|     - |  2614 | ` * cache is filled", and next() only refills while the window still has room.` |
|     - |  2615 | ` */` |
|    28 |  2616 | `static sxi32 DualLimitSeek(ph7_context *pCtx,ph7_class_instance *pThis,sxi64 iPos)` |
|     1 |  2617 | `{` |
|    29 |  2618 | `	ph7_vm *pVm = pCtx->pVm;` |
|    29 |  2619 | `	sxi64 iOff = PH7_NativeAttrInt(pThis,IT_OFF);` |
|    29 |  2620 | `	sxi64 iLim = PH7_NativeAttrInt(pThis,IT_LIM);` |
|    29 |  2621 | `	ph7_class_instance *pIn = DualDriver(pThis);` |
|     - |  2622 | `	ph7_class *pSeekCls;` |
|     - |  2623 | `	sxi32 rc;` |
|     - |  2624 | `	int bValid;` |
|    29 |  2625 | `	DualFree(pVm,pThis);` |
|    29 |  2626 | `	if( iPos < iOff ){` |
|     7 |  2627 | `		return PH7_VmThrowException(pCtx,"OutOfBoundsException",` |
|     2 |  2628 | `			"Cannot seek to %qd which is below the offset %qd",iPos,iOff);` |
|     - |  2629 | `	}` |
|    25 |  2630 | `	if( iLim != -1 && (iPos - iOff) >= iLim ){` |
|     4 |  2631 | `		return PH7_VmThrowException(pCtx,"OutOfBoundsException",` |
|     1 |  2632 | `			"Cannot seek to %qd which is behind offset %qd plus count %qd",iPos,iOff,iLim);` |
|     - |  2633 | `	}` |
|    23 |  2634 | `	pSeekCls = PH7_VmExtractClass(pVm,"SeekableIterator",sizeof("SeekableIterator")-1,FALSE,0);` |
|    22 |  2635 | `	if( iPos != PH7_NativeAttrInt(pThis,IT_CP) && pIn && pSeekCls` |
|    17 |  2636 | `	 && PH7_VmInstanceOf(pIn->pClass,pSeekCls) ){` |
|     - |  2637 | `		/* The inner knows how to jump: hand it the ABSOLUTE position and let its own` |
|     - |  2638 | `		 * refusal (ArrayIterator's "Seek position N is out of range") surface. */` |
|    17 |  2639 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pIn->pClass,"seek",sizeof("seek")-1);` |
|     - |  2640 | `		ph7_value sPos,*apArg[1];` |
|    17 |  2641 | `		PH7_MemObjInitFromInt(pVm,&sPos,iPos);` |
|    17 |  2642 | `		apArg[0] = &sPos;` |
|    17 |  2643 | `		rc = pMethod ? PH7_VmCallClassMethod(pVm,pIn,pMethod,0,1,apArg) : SXRET_OK;` |
|    17 |  2644 | `		PH7_MemObjRelease(&sPos);` |
|    17 |  2645 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  2646 | `			return rc;` |
|     - |  2647 | `		}` |
|    17 |  2648 | `		PH7_NativeSetAttrInt(pVm,pThis,IT_CP,iPos);` |
|    17 |  2649 | `		rc = DualInnerValid(pVm,pThis,&bValid);` |
|    17 |  2650 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  2651 | `			return rc;` |
|     - |  2652 | `		}` |
|    17 |  2653 | `		if( bValid ){` |
|    17 |  2654 | `			return DualFetch(pVm,pThis,FALSE);` |
|     - |  2655 | `		}` |
|   ! 0 |  2656 | `		return SXRET_OK;` |
|     - |  2657 | `	}` |
|     - |  2658 | `	/* Otherwise emulate: a backward seek is a rewind followed by next() calls. */` |
|     7 |  2659 | `	if( iPos < PH7_NativeAttrInt(pThis,IT_CP) ){` |
|   ! 0 |  2660 | `		rc = DualRewindInner(pVm,pThis);` |
|   ! 0 |  2661 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  2662 | `			return rc;` |
|     - |  2663 | `		}` |
|   ! 0 |  2664 | `	}` |
|     3 |  2665 | `	for(;;){` |
|     7 |  2666 | `		if( iPos <= PH7_NativeAttrInt(pThis,IT_CP) ){` |
|     7 |  2667 | `			break;` |
|     - |  2668 | `		}` |
|   ! 0 |  2669 | `		rc = DualInnerValid(pVm,pThis,&bValid);` |
|   ! 0 |  2670 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  2671 | `			return rc;` |
|     - |  2672 | `		}` |
|   ! 0 |  2673 | `		if( !bValid ){` |
|   ! 0 |  2674 | `			break;` |
|     - |  2675 | `		}` |
|   ! 0 |  2676 | `		rc = DualNextInner(pVm,pThis);` |
|   ! 0 |  2677 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  2678 | `			return rc;` |
|     - |  2679 | `		}` |
|   ! 0 |  2680 | `	}` |
|     7 |  2681 | `	rc = DualInnerValid(pVm,pThis,&bValid);` |
|     7 |  2682 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  2683 | `		return rc;` |
|     - |  2684 | `	}` |
|     7 |  2685 | `	if( bValid ){` |
|     7 |  2686 | `		return DualFetch(pVm,pThis,TRUE);` |
|     - |  2687 | `	}` |
|   ! 0 |  2688 | `	return SXRET_OK;` |
|    15 |  2689 | `}` |
|    48 |  2690 | `static int vm_builtin_LimitIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2691 | `{` |
|    49 |  2692 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  2693 | `	ph7_class_instance *pThis;` |
|    49 |  2694 | `	sxi64 iOff = 0,iLim = -1;` |
|     - |  2695 | `	sxi32 rc;` |
|     - |  2696 | `	/* php screens the two bounds BEFORE it remembers the iterator, so a refused` |
|     - |  2697 | `	 * LimitIterator can still be constructed again. PH7_IntArgResolve is the shared` |
|     - |  2698 | ``	 * `int` ZPP: the central signature screen does not cover a non-numeric STRING`` |
|     - |  2699 | `	 * against an int parameter, and every builtin that takes one calls this. */` |
|    49 |  2700 | `	if( nArg > 1 ){` |
|    41 |  2701 | `		rc = PH7_IntArgResolve(pCtx,apArg[1],ph7_function_name(pCtx),2,"$offset","int",&iOff);` |
|    41 |  2702 | `		if( rc != PH7_OK ){` |
|   ! 0 |  2703 | `			return rc;` |
|     - |  2704 | `		}` |
|    41 |  2705 | `		if( iOff < 0 ){` |
|     7 |  2706 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  2707 | `				"%s(): Argument #2 ($offset) must be greater than or equal to 0",` |
|     2 |  2708 | `				ph7_function_name(pCtx));` |
|     - |  2709 | `		}` |
|    18 |  2710 | `	}` |
|    45 |  2711 | `	if( nArg > 2 ){` |
|    33 |  2712 | `		rc = PH7_IntArgResolve(pCtx,apArg[2],ph7_function_name(pCtx),3,"$limit","int",&iLim);` |
|    33 |  2713 | `		if( rc != PH7_OK ){` |
|   ! 0 |  2714 | `			return rc;` |
|     - |  2715 | `		}` |
|    33 |  2716 | `		if( iLim < -1 ){` |
|     7 |  2717 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  2718 | `				"%s(): Argument #3 ($limit) must be greater than or equal to -1",` |
|     2 |  2719 | `				ph7_function_name(pCtx));` |
|     - |  2720 | `		}` |
|    14 |  2721 | `	}` |
|    41 |  2722 | `	rc = DualConstruct(pCtx,"LimitIterator",nArg,apArg);` |
|    41 |  2723 | `	if( rc != PH7_OK ){` |
|     3 |  2724 | `		return rc;` |
|     - |  2725 | `	}` |
|    39 |  2726 | `	pThis = PH7_ContextThis(pCtx);` |
|    39 |  2727 | `	if( pThis ){` |
|    39 |  2728 | `		PH7_NativeSetAttrInt(pVm,pThis,IT_OFF,iOff);` |
|    39 |  2729 | `		PH7_NativeSetAttrInt(pVm,pThis,IT_LIM,iLim);` |
|    19 |  2730 | `	}` |
|    39 |  2731 | `	return PH7_OK;` |
|    25 |  2732 | `}` |
|    16 |  2733 | `static int vm_builtin_LimitIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2734 | `{` |
|    17 |  2735 | `	ph7_vm *pVm = pCtx->pVm;` |
|    17 |  2736 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2737 | `	sxi32 rc;` |
|     8 |  2738 | `	SXUNUSED(nArg);` |
|     8 |  2739 | `	SXUNUSED(apArg);` |
|    17 |  2740 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  2741 | `		return DualNotReady(pCtx);` |
|     - |  2742 | `	}` |
|    17 |  2743 | `	rc = DualRewindInner(pVm,pThis);` |
|    17 |  2744 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  2745 | `		return rc;` |
|     - |  2746 | `	}` |
|    17 |  2747 | `	return DualLimitSeek(pCtx,pThis,PH7_NativeAttrInt(pThis,IT_OFF));` |
|     9 |  2748 | `}` |
|    40 |  2749 | `static int vm_builtin_LimitIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2750 | `{` |
|    41 |  2751 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2752 | `	sxi64 iLim;` |
|    20 |  2753 | `	SXUNUSED(nArg);` |
|    20 |  2754 | `	SXUNUSED(apArg);` |
|    41 |  2755 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  2756 | `		return DualNotReady(pCtx);` |
|     - |  2757 | `	}` |
|    41 |  2758 | `	iLim = PH7_NativeAttrInt(pThis,IT_LIM);` |
|    81 |  2759 | `	ph7_result_bool(pCtx,` |
|    20 |  2760 | `		(iLim == -1` |
|    33 |  2761 | `		 \|\| (PH7_NativeAttrInt(pThis,IT_CP) - PH7_NativeAttrInt(pThis,IT_OFF)) < iLim)` |
|    35 |  2762 | `		&& DualFilled(pThis));` |
|    41 |  2763 | `	return PH7_OK;` |
|    21 |  2764 | `}` |
|    34 |  2765 | `static int vm_builtin_LimitIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2766 | `{` |
|    35 |  2767 | `	ph7_vm *pVm = pCtx->pVm;` |
|    35 |  2768 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2769 | `	sxi64 iLim;` |
|     - |  2770 | `	sxi32 rc;` |
|    17 |  2771 | `	SXUNUSED(nArg);` |
|    17 |  2772 | `	SXUNUSED(apArg);` |
|    35 |  2773 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  2774 | `		return DualNotReady(pCtx);` |
|     - |  2775 | `	}` |
|    35 |  2776 | `	rc = DualNextInner(pVm,pThis);` |
|    35 |  2777 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  2778 | `		return rc;` |
|     - |  2779 | `	}` |
|    35 |  2780 | `	iLim = PH7_NativeAttrInt(pThis,IT_LIM);` |
|    34 |  2781 | `	if( iLim == -1` |
|    30 |  2782 | `	 \|\| (PH7_NativeAttrInt(pThis,IT_CP) - PH7_NativeAttrInt(pThis,IT_OFF)) < iLim ){` |
|    25 |  2783 | `		return DualFetch(pVm,pThis,TRUE);` |
|     - |  2784 | `	}` |
|    11 |  2785 | `	return PH7_OK;   /* past the window: the cache stays empty, so current() is null */` |
|    18 |  2786 | `}` |
|    12 |  2787 | `static int vm_builtin_LimitIterator_seek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2788 | `{` |
|    13 |  2789 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    13 |  2790 | `	sxi64 iPos = 0;` |
|     - |  2791 | `	sxi32 rc;` |
|    13 |  2792 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  2793 | `		return DualNotReady(pCtx);` |
|     - |  2794 | `	}` |
|    13 |  2795 | `	if( nArg > 0 ){` |
|    13 |  2796 | `		rc = PH7_IntArgResolve(pCtx,apArg[0],ph7_function_name(pCtx),1,"$offset","int",&iPos);` |
|    13 |  2797 | `		if( rc != PH7_OK ){` |
|   ! 0 |  2798 | `			return rc;` |
|     - |  2799 | `		}` |
|     6 |  2800 | `	}` |
|    13 |  2801 | `	rc = DualLimitSeek(pCtx,pThis,iPos);` |
|    13 |  2802 | `	if( rc != PH7_OK ){` |
|     7 |  2803 | `		return rc;` |
|     - |  2804 | `	}` |
|     7 |  2805 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(pThis,IT_CP));` |
|     7 |  2806 | `	return PH7_OK;` |
|     7 |  2807 | `}` |
|    18 |  2808 | `static int vm_builtin_LimitIterator_getPosition(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2809 | `{` |
|    19 |  2810 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     9 |  2811 | `	SXUNUSED(nArg);` |
|     9 |  2812 | `	SXUNUSED(apArg);` |
|    19 |  2813 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  2814 | `		return DualNotReady(pCtx);` |
|     - |  2815 | `	}` |
|    19 |  2816 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(pThis,IT_CP));` |
|    19 |  2817 | `	return PH7_OK;` |
|    10 |  2818 | `}` |
|     - |  2819 | `/* InfiniteIterator::next(): step, and on exhaustion rewind and step into the head` |
|     - |  2820 | ` * again. Both refills are php's check_more=0 form — the validity was just tested. */` |
|    16 |  2821 | `static int vm_builtin_InfiniteIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2822 | `{` |
|    17 |  2823 | `	ph7_vm *pVm = pCtx->pVm;` |
|    17 |  2824 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2825 | `	sxi32 rc;` |
|     - |  2826 | `	int bValid;` |
|     8 |  2827 | `	SXUNUSED(nArg);` |
|     8 |  2828 | `	SXUNUSED(apArg);` |
|    17 |  2829 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  2830 | `		return DualNotReady(pCtx);` |
|     - |  2831 | `	}` |
|    17 |  2832 | `	rc = DualNextInner(pVm,pThis);` |
|    17 |  2833 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  2834 | `		return rc;` |
|     - |  2835 | `	}` |
|    17 |  2836 | `	rc = DualInnerValid(pVm,pThis,&bValid);` |
|    17 |  2837 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  2838 | `		return rc;` |
|     - |  2839 | `	}` |
|    17 |  2840 | `	if( !bValid ){` |
|     9 |  2841 | `		rc = DualRewindInner(pVm,pThis);` |
|     9 |  2842 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  2843 | `			return rc;` |
|     - |  2844 | `		}` |
|     9 |  2845 | `		rc = DualInnerValid(pVm,pThis,&bValid);` |
|     9 |  2846 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  2847 | `			return rc;` |
|     - |  2848 | `		}` |
|     4 |  2849 | `	}` |
|    17 |  2850 | `	if( bValid ){` |
|    17 |  2851 | `		return DualFetch(pVm,pThis,FALSE);` |
|     - |  2852 | `	}` |
|   ! 0 |  2853 | `	return PH7_OK;` |
|     9 |  2854 | `}` |
|     - |  2855 | `/*` |
|     - |  2856 | ` * NoRewindIterator. Its rewind() does nothing at all — and because the four` |
|     - |  2857 | ` * accessors read the INNER live rather than the cache, an instance is usable` |
|     - |  2858 | ` * without ever being rewound, which is the entire point of the class.` |
|     - |  2859 | ` */` |
|     4 |  2860 | `static int vm_builtin_NoRewindIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2861 | `{` |
|     2 |  2862 | `	SXUNUSED(nArg);` |
|     2 |  2863 | `	SXUNUSED(apArg);` |
|     5 |  2864 | `	if( PH7_ContextThis(pCtx) == 0 \|\| DualDriver(PH7_ContextThis(pCtx)) == 0 ){` |
|   ! 0 |  2865 | `		return DualNotReady(pCtx);` |
|     - |  2866 | `	}` |
|     5 |  2867 | `	return PH7_OK;` |
|     3 |  2868 | `}` |
|    14 |  2869 | `static int vm_builtin_NoRewindIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2870 | `{` |
|    15 |  2871 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2872 | `	sxi32 rc;` |
|     - |  2873 | `	int bValid;` |
|     7 |  2874 | `	SXUNUSED(nArg);` |
|     7 |  2875 | `	SXUNUSED(apArg);` |
|    15 |  2876 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  2877 | `		return DualNotReady(pCtx);` |
|     - |  2878 | `	}` |
|    15 |  2879 | `	rc = DualInnerValid(pCtx->pVm,pThis,&bValid);` |
|    15 |  2880 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  2881 | `		return rc;` |
|     - |  2882 | `	}` |
|    15 |  2883 | `	ph7_result_bool(pCtx,bValid);` |
|    15 |  2884 | `	return PH7_OK;` |
|     8 |  2885 | `}` |
|    26 |  2886 | `static int DualForwardLive(ph7_context *pCtx,const char *zName,sxu32 nLen,int bResult)` |
|     1 |  2887 | `{` |
|    27 |  2888 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2889 | `	ph7_value sVal;` |
|     - |  2890 | `	sxi32 rc;` |
|    27 |  2891 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  2892 | `		return DualNotReady(pCtx);` |
|     - |  2893 | `	}` |
|    27 |  2894 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|    27 |  2895 | `	rc = DualCall(pCtx->pVm,pThis,zName,nLen,bResult ? &sVal : 0);` |
|    27 |  2896 | `	if( rc == SXRET_OK && bResult ){` |
|    17 |  2897 | `		ph7_result_value(pCtx,&sVal);` |
|     8 |  2898 | `	}` |
|    27 |  2899 | `	PH7_MemObjRelease(&sVal);` |
|    27 |  2900 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    14 |  2901 | `}` |
|    10 |  2902 | `static int vm_builtin_NoRewindIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2903 | `{` |
|     5 |  2904 | `	SXUNUSED(nArg);` |
|     5 |  2905 | `	SXUNUSED(apArg);` |
|    11 |  2906 | `	return DualForwardLive(pCtx,"current",sizeof("current")-1,TRUE);` |
|     1 |  2907 | `}` |
|     6 |  2908 | `static int vm_builtin_NoRewindIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2909 | `{` |
|     3 |  2910 | `	SXUNUSED(nArg);` |
|     3 |  2911 | `	SXUNUSED(apArg);` |
|     7 |  2912 | `	return DualForwardLive(pCtx,"key",sizeof("key")-1,TRUE);` |
|     1 |  2913 | `}` |
|    10 |  2914 | `static int vm_builtin_NoRewindIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2915 | `{` |
|     5 |  2916 | `	SXUNUSED(nArg);` |
|     5 |  2917 | `	SXUNUSED(apArg);` |
|    11 |  2918 | `	return DualForwardLive(pCtx,"next",sizeof("next")-1,FALSE);` |
|     1 |  2919 | `}` |
|     - |  2920 | `/* EmptyIterator: valid() is false forever, and asking for a value or a key is a` |
|     - |  2921 | ` * BadMethodCallException rather than a null. */` |
|     4 |  2922 | `static int vm_builtin_EmptyIterator_nop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2923 | `{` |
|     2 |  2924 | `	SXUNUSED(nArg);` |
|     2 |  2925 | `	SXUNUSED(apArg);` |
|     5 |  2926 | `	ph7_result_null(pCtx);` |
|     5 |  2927 | `	return PH7_OK;` |
|     1 |  2928 | `}` |
|     8 |  2929 | `static int vm_builtin_EmptyIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2930 | `{` |
|     4 |  2931 | `	SXUNUSED(nArg);` |
|     4 |  2932 | `	SXUNUSED(apArg);` |
|     9 |  2933 | `	ph7_result_bool(pCtx,0);` |
|     9 |  2934 | `	return PH7_OK;` |
|     1 |  2935 | `}` |
|     4 |  2936 | `static int vm_builtin_EmptyIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2937 | `{` |
|     2 |  2938 | `	SXUNUSED(nArg);` |
|     2 |  2939 | `	SXUNUSED(apArg);` |
|     5 |  2940 | `	return PH7_VmThrowException(pCtx,"BadMethodCallException",` |
|     - |  2941 | `		"Accessing the value of an EmptyIterator");` |
|     1 |  2942 | `}` |
|     4 |  2943 | `static int vm_builtin_EmptyIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2944 | `{` |
|     2 |  2945 | `	SXUNUSED(nArg);` |
|     2 |  2946 | `	SXUNUSED(apArg);` |
|     5 |  2947 | `	return PH7_VmThrowException(pCtx,"BadMethodCallException",` |
|     - |  2948 | `		"Accessing the key of an EmptyIterator");` |
|     1 |  2949 | `}` |
|     - |  2950 | `/*` |
|     - |  2951 | ` * ---------------------------------------------------------------------------` |
|     - |  2952 | ` * RegexIterator: a FilterIterator whose accept() runs a regex over the CACHE.` |
|     - |  2953 | ` *` |
|     - |  2954 | ` * Everything that matters here follows from the cache the decorators already` |
|     - |  2955 | `` * keep. php's accept() reads `current.data` (or `current.key` under USE_KEY) and`` |
|     - |  2956 | ` * -- in every mode but MATCH -- WRITES THE RESULT BACK INTO THAT SAME SLOT, which` |
|     - |  2957 | ` * is why the class declares no current() of its own: the inherited one already` |
|     - |  2958 | `` * answers the transformed value. The PHP chunk kept a private `$__cur` and`` |
|     - |  2959 | ` * overrode current(), and that is where its two wrong answers came from: a` |
|     - |  2960 | ` * REPLACE under USE_KEY must replace into the KEY (php leaves current() alone),` |
|     - |  2961 | ` * and an ARRAY current() is refused outright rather than matched as the string` |
|     - |  2962 | ` * "Array".` |
|     - |  2963 | ` */` |
|     - |  2964 | `#define IT_RE  "__re"   /* php's u.regex.regex: the pattern, as given */` |
|     - |  2965 | `#define IT_RM  "__rm"   /* php's u.regex.mode */` |
|     - |  2966 | `#define IT_RF  "__rf"   /* php's u.regex.flags (USE_KEY / INVERT_MATCH) */` |
|     - |  2967 | `#define IT_RP  "__rp"   /* php's u.regex.preg_flags */` |
|     - |  2968 | `#define REGIT_USE_KEY  1` |
|     - |  2969 | `#define REGIT_INVERTED 2` |
|     - |  2970 | `/* php's ValueError for a mode outside the five. The constructor and setMode()` |
|     - |  2971 | ` * word it identically and differ only in the argument they name. */` |
|     8 |  2972 | `static int RegitBadMode(ph7_context *pCtx,const char *zWhere)` |
|     1 |  2973 | `{` |
|    13 |  2974 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  2975 | `		"%s must be RegexIterator::MATCH, RegexIterator::GET_MATCH, "` |
|     - |  2976 | `		"RegexIterator::ALL_MATCHES, RegexIterator::SPLIT, or RegexIterator::REPLACE",` |
|     4 |  2977 | `		zWhere);` |
|     1 |  2978 | `}` |
|     - |  2979 | `/*` |
|     - |  2980 | ` * The constructor both regex iterators run. Every diagnostic it raises names the` |
|     - |  2981 | ` * class that was CONSTRUCTED (php's are its own method's scope), so the owner is a` |
|     - |  2982 | ` * parameter rather than a literal -- and the ValueError still names the mode` |
|     - |  2983 | ` * constants on RegexIterator, which is where php declares them.` |
|     - |  2984 | ` */` |
|    94 |  2985 | `static int RegitConstruct(ph7_context *pCtx,const char *zOwner,int nArg,ph7_value **apArg)` |
|     1 |  2986 | `{` |
|    95 |  2987 | `	ph7_vm *pVm = pCtx->pVm;` |
|    95 |  2988 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2989 | `	const char *zPat;` |
|     - |  2990 | `	char zWhere[128];` |
|     - |  2991 | `	int nPat;` |
|    95 |  2992 | `	sxi64 iMode = PH7_REGIT_MATCH;` |
|     - |  2993 | `	char zErr[288];` |
|     - |  2994 | `	sxi32 rc;` |
|    95 |  2995 | `	if( pThis == 0 ){` |
|   ! 0 |  2996 | `		return PH7_OK;` |
|     - |  2997 | `	}` |
|    95 |  2998 | `	if( PH7_NativeAttrObj(pThis,IT_IN) != 0 ){` |
|     - |  2999 | `		/* php makes the "already built" refusal before it reads any argument, so` |
|     - |  3000 | `		 * hand this straight to the shared constructor, which words it. */` |
|   ! 0 |  3001 | `		return DualConstruct(pCtx,zOwner,nArg,apArg);` |
|     - |  3002 | `	}` |
|    95 |  3003 | `	if( nArg < 2 ){` |
|   ! 0 |  3004 | `		return PH7_OK;   /* the arity screen already refused */` |
|     - |  3005 | `	}` |
|    95 |  3006 | `	if( nArg > 2 ){` |
|    49 |  3007 | `		iMode = ph7_value_to_int(apArg[2]);` |
|    24 |  3008 | `	}` |
|    95 |  3009 | `	if( iMode < PH7_REGIT_MATCH \|\| iMode > PH7_REGIT_REPLACE ){` |
|     5 |  3010 | `		SyBufferFormat(zWhere,sizeof(zWhere),"%s::__construct(): Argument #3 ($mode)",zOwner);` |
|     5 |  3011 | `		return RegitBadMode(pCtx,zWhere);` |
|     - |  3012 | `	}` |
|     - |  3013 | `	/* php compiles the pattern HERE and promotes pcre's warning to an` |
|     - |  3014 | ``	 * InvalidArgumentException, so a bad pattern is refused by `new` rather than`` |
|     - |  3015 | `	 * warning once per element from accept(). */` |
|    91 |  3016 | `	zPat = ph7_value_to_string(apArg[1],&nPat);` |
|    91 |  3017 | `	if( !PH7_PcrePatternCheck(pVm,zPat,nPat,zErr,sizeof(zErr)) ){` |
|    10 |  3018 | `		return PH7_VmThrowException(pCtx,"InvalidArgumentException",` |
|     3 |  3019 | `			"%s::__construct(): %s",zOwner,zErr);` |
|     - |  3020 | `	}` |
|    85 |  3021 | `	rc = DualConstruct(pCtx,zOwner,nArg,apArg);` |
|    85 |  3022 | `	if( rc != PH7_OK ){` |
|   ! 0 |  3023 | `		return rc;` |
|     - |  3024 | `	}` |
|    85 |  3025 | `	PH7_NativeSetAttrStr(pVm,pThis,IT_RE,zPat,(sxu32)nPat);` |
|    85 |  3026 | `	PH7_NativeSetAttrInt(pVm,pThis,IT_RM,iMode);` |
|    85 |  3027 | `	PH7_NativeSetAttrInt(pVm,pThis,IT_RF,nArg > 3 ? ph7_value_to_int(apArg[3]) : 0);` |
|    85 |  3028 | `	PH7_NativeSetAttrInt(pVm,pThis,IT_RP,nArg > 4 ? ph7_value_to_int(apArg[4]) : 0);` |
|    85 |  3029 | `	return PH7_OK;` |
|    48 |  3030 | `}` |
|    68 |  3031 | `static int vm_builtin_RegexIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3032 | `{` |
|    69 |  3033 | `	return RegitConstruct(pCtx,"RegexIterator",nArg,apArg);` |
|     1 |  3034 | `}` |
|   102 |  3035 | `static int vm_builtin_RegexIterator_accept(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3036 | `{` |
|   103 |  3037 | `	ph7_vm *pVm = pCtx->pVm;` |
|   103 |  3038 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3039 | `	ph7_value sSubject,sPattern,sRepl,sOut,*pSlot;` |
|   103 |  3040 | `	int iMode,iFlags,bUseKey,bOk = 0;` |
|     - |  3041 | `	sxi32 rc;` |
|    51 |  3042 | `	SXUNUSED(nArg);` |
|    51 |  3043 | `	SXUNUSED(apArg);` |
|   103 |  3044 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  3045 | `		return DualNotReady(pCtx);` |
|     - |  3046 | `	}` |
|   103 |  3047 | `	if( !DualFilled(pThis) ){` |
|     - |  3048 | `		/* Nothing has been fetched: php answers false without touching the regex. */` |
|     3 |  3049 | `		ph7_result_bool(pCtx,0);` |
|     3 |  3050 | `		return PH7_OK;` |
|     - |  3051 | `	}` |
|   101 |  3052 | `	iMode = (int)PH7_NativeAttrInt(pThis,IT_RM);` |
|   101 |  3053 | `	iFlags = (int)PH7_NativeAttrInt(pThis,IT_RF);` |
|   101 |  3054 | `	bUseKey = (iFlags & REGIT_USE_KEY) != 0;` |
|   101 |  3055 | `	pSlot = PH7_NativeAttr(pThis,bUseKey ? IT_CK : IT_CD);` |
|   101 |  3056 | `	if( pSlot == 0 ){` |
|   ! 0 |  3057 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  3058 | `		return PH7_OK;` |
|     - |  3059 | `	}` |
|   101 |  3060 | `	if( !bUseKey && (pSlot->iFlags & MEMOBJ_HASHMAP) ){` |
|     - |  3061 | ``		/* php's `Z_TYPE(current.data) == IS_ARRAY -> RETURN_FALSE`, ahead of every`` |
|     - |  3062 | `		 * mode. The chunk's (string)$subject matched the word "Array" instead. */` |
|     5 |  3063 | `		ph7_result_bool(pCtx,0);` |
|     5 |  3064 | `		return PH7_OK;` |
|     - |  3065 | `	}` |
|     - |  3066 | `	/* Take the subject as a VALUE: the slot pointer does not survive a call into` |
|     - |  3067 | `	 * user code, and an object subject reaches __toString() below. */` |
|    97 |  3068 | `	PH7_MemObjInit(pVm,&sSubject);` |
|    97 |  3069 | `	PH7_MemObjStore(pSlot,&sSubject);` |
|    97 |  3070 | `	rc = PH7_MemObjToStringUV(&sSubject);` |
|    97 |  3071 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  3072 | `		PH7_MemObjRelease(&sSubject);` |
|   ! 0 |  3073 | `		return rc;` |
|     - |  3074 | `	}` |
|    97 |  3075 | `	PH7_MemObjInit(pVm,&sPattern);` |
|    97 |  3076 | `	PH7_MemObjInit(pVm,&sRepl);` |
|    97 |  3077 | `	PH7_MemObjInit(pVm,&sOut);` |
|     - |  3078 | `	{` |
|    97 |  3079 | `		ph7_value *pRe = PH7_NativeAttr(pThis,IT_RE);` |
|    97 |  3080 | `		if( pRe ){` |
|    97 |  3081 | `			PH7_MemObjStore(pRe,&sPattern);` |
|    48 |  3082 | `		}` |
|     - |  3083 | `	}` |
|    97 |  3084 | `	if( iMode == PH7_REGIT_REPLACE ){` |
|     - |  3085 | `		/* php reads the public $replacement property, whose declared ?string makes` |
|     - |  3086 | `		 * the read total: a null answers the empty string. */` |
|    17 |  3087 | `		ph7_value *pRepl = PH7_NativeAttr(pThis,"replacement");` |
|    17 |  3088 | `		if( pRepl ){` |
|    17 |  3089 | `			PH7_MemObjStore(pRepl,&sRepl);` |
|     8 |  3090 | `		}` |
|    17 |  3091 | `		PH7_MemObjToString(&sRepl);` |
|     8 |  3092 | `	}` |
|   145 |  3093 | `	rc = PH7_PcreRegitApply(pCtx,iMode,&sPattern,&sSubject,` |
|    96 |  3094 | `		(int)PH7_NativeAttrInt(pThis,IT_RP),&sRepl,&sOut,&bOk);` |
|    97 |  3095 | `	if( rc == PH7_OK && iMode != PH7_REGIT_MATCH ){` |
|     - |  3096 | `		/* php writes the transformed value over the cached pair -- into the KEY when` |
|     - |  3097 | `		 * a REPLACE is keyed, into current() otherwise -- so the inherited current()` |
|     - |  3098 | `		 * and key() present it. */` |
|    47 |  3099 | `		DualSetSlot(pVm,pThis,(iMode == PH7_REGIT_REPLACE && bUseKey) ? IT_CK : IT_CD,&sOut);` |
|    23 |  3100 | `	}` |
|    97 |  3101 | `	PH7_MemObjRelease(&sSubject);` |
|    97 |  3102 | `	PH7_MemObjRelease(&sPattern);` |
|    97 |  3103 | `	PH7_MemObjRelease(&sRepl);` |
|    97 |  3104 | `	PH7_MemObjRelease(&sOut);` |
|    97 |  3105 | `	if( rc != PH7_OK ){` |
|   ! 0 |  3106 | `		return rc;` |
|     - |  3107 | `	}` |
|    97 |  3108 | `	ph7_result_bool(pCtx,(iFlags & REGIT_INVERTED) ? !bOk : bOk);` |
|    97 |  3109 | `	return PH7_OK;` |
|    52 |  3110 | `}` |
|     6 |  3111 | `static int vm_builtin_RegexIterator_getRegex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3112 | `{` |
|     7 |  3113 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3114 | `	ph7_value *pRe;` |
|     3 |  3115 | `	SXUNUSED(nArg);` |
|     3 |  3116 | `	SXUNUSED(apArg);` |
|     7 |  3117 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  3118 | `		return DualNotReady(pCtx);` |
|     - |  3119 | `	}` |
|     7 |  3120 | `	pRe = PH7_NativeAttr(pThis,IT_RE);` |
|     7 |  3121 | `	if( pRe ){` |
|     7 |  3122 | `		ph7_result_value(pCtx,pRe);` |
|     3 |  3123 | `	}` |
|     7 |  3124 | `	return PH7_OK;` |
|     4 |  3125 | `}` |
|     - |  3126 | `/* The three getters and the three setters are one pair per slot; only setMode()` |
|     - |  3127 | ` * screens its value, which is php's own asymmetry (setFlags/setPregFlags take` |
|     - |  3128 | ` * any integer). */` |
|    24 |  3129 | `static int RegitGet(ph7_context *pCtx,const char *zSlot)` |
|     1 |  3130 | `{` |
|    25 |  3131 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    25 |  3132 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  3133 | `		return DualNotReady(pCtx);` |
|     - |  3134 | `	}` |
|    25 |  3135 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(pThis,zSlot));` |
|    25 |  3136 | `	return PH7_OK;` |
|    13 |  3137 | `}` |
|     8 |  3138 | `static int RegitSet(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zSlot)` |
|     1 |  3139 | `{` |
|     9 |  3140 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     9 |  3141 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  3142 | `		return DualNotReady(pCtx);` |
|     - |  3143 | `	}` |
|     9 |  3144 | `	if( nArg > 0 ){` |
|     9 |  3145 | `		PH7_NativeSetAttrInt(pCtx->pVm,pThis,zSlot,ph7_value_to_int64(apArg[0]));` |
|     4 |  3146 | `	}` |
|     9 |  3147 | `	return PH7_OK;` |
|     5 |  3148 | `}` |
|     8 |  3149 | `static int vm_builtin_RegexIterator_getMode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3150 | `{` |
|     4 |  3151 | `	SXUNUSED(nArg);` |
|     4 |  3152 | `	SXUNUSED(apArg);` |
|     9 |  3153 | `	return RegitGet(pCtx,IT_RM);` |
|     1 |  3154 | `}` |
|     8 |  3155 | `static int vm_builtin_RegexIterator_setMode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3156 | `{` |
|     9 |  3157 | `	if( nArg > 0 ){` |
|     9 |  3158 | `		sxi64 iMode = ph7_value_to_int64(apArg[0]);` |
|     9 |  3159 | `		if( iMode < PH7_REGIT_MATCH \|\| iMode > PH7_REGIT_REPLACE ){` |
|     - |  3160 | `			/* php screens the VALUE before it even fetches the object. */` |
|     5 |  3161 | `			return RegitBadMode(pCtx,"RegexIterator::setMode(): Argument #1 ($mode)");` |
|     - |  3162 | `		}` |
|     2 |  3163 | `	}` |
|     5 |  3164 | `	return RegitSet(pCtx,nArg,apArg,IT_RM);` |
|     5 |  3165 | `}` |
|     8 |  3166 | `static int vm_builtin_RegexIterator_getFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3167 | `{` |
|     4 |  3168 | `	SXUNUSED(nArg);` |
|     4 |  3169 | `	SXUNUSED(apArg);` |
|     9 |  3170 | `	return RegitGet(pCtx,IT_RF);` |
|     1 |  3171 | `}` |
|     2 |  3172 | `static int vm_builtin_RegexIterator_setFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3173 | `{` |
|     3 |  3174 | `	return RegitSet(pCtx,nArg,apArg,IT_RF);` |
|     1 |  3175 | `}` |
|     8 |  3176 | `static int vm_builtin_RegexIterator_getPregFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3177 | `{` |
|     4 |  3178 | `	SXUNUSED(nArg);` |
|     4 |  3179 | `	SXUNUSED(apArg);` |
|     9 |  3180 | `	return RegitGet(pCtx,IT_RP);` |
|     1 |  3181 | `}` |
|     2 |  3182 | `static int vm_builtin_RegexIterator_setPregFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3183 | `{` |
|     3 |  3184 | `	return RegitSet(pCtx,nArg,apArg,IT_RP);` |
|     1 |  3185 | `}` |
|     - |  3186 | `/*` |
|     - |  3187 | ` * ---------------------------------------------------------------------------` |
|     - |  3188 | ` * AppendIterator: an IteratorIterator whose inner iterator is whatever entry a` |
|     - |  3189 | ` * real ArrayIterator is currently pointing at.` |
|     - |  3190 | ` *` |
|     - |  3191 | `` * php keeps the appended iterators in an actual `ArrayIterator` INSTANCE`` |
|     - |  3192 | `` * (`u.append.zarrayit`, the object getArrayIterator() hands out) and walks it with`` |
|     - |  3193 | `` * a cursor over the SAME storage (`u.append.iterator`). Both halves are`` |
|     - |  3194 | ` * php-visible and the chunk had neither: it kept a private PHP array and answered` |
|     - |  3195 | ` * getArrayIterator() with a fresh ArrayIterator over a COPY, so appending through` |
|     - |  3196 | `` * the returned object iterated nothing and `$ai->rewind()` did not restart the`` |
|     - |  3197 | `` * walk. The list cursor here is that one ArrayIterator's own `pCur`, driven`` |
|     - |  3198 | ` * directly the way php drives its iterator funcs -- not through the class's` |
|     - |  3199 | ` * methods, which php does not call either.` |
|     - |  3200 | ` */` |
|   448 |  3201 | `static ph7_class_instance * ApList(ph7_class_instance *pThis)` |
|     1 |  3202 | `{` |
|   449 |  3203 | `	return pThis ? PH7_NativeAttrObj(pThis,AP_LIST) : 0;` |
|     1 |  3204 | `}` |
|   360 |  3205 | `static ph7_hashmap * ApMap(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  3206 | `{` |
|   361 |  3207 | `	return SplStore(pVm,ApList(pThis));` |
|     1 |  3208 | `}` |
|     - |  3209 | `/* The iterator the list cursor points at, or 0 past the end. */` |
|   120 |  3210 | `static ph7_class_instance * ApCurrent(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  3211 | `{` |
|   121 |  3212 | `	ph7_hashmap *pMap = ApMap(pVm,pThis);` |
|   121 |  3213 | `	ph7_value *pVal = (pMap && pMap->pCur) ? HashmapExtractNodeValue(pMap->pCur) : 0;` |
|   121 |  3214 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    31 |  3215 | `		return 0;` |
|     - |  3216 | `	}` |
|    91 |  3217 | `	return (ph7_class_instance *)pVal->x.pOther;` |
|    61 |  3218 | `}` |
|    28 |  3219 | `static void ApListRewind(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  3220 | `{` |
|    29 |  3221 | `	ph7_hashmap *pMap = ApMap(pVm,pThis);` |
|    29 |  3222 | `	if( pMap ){` |
|    29 |  3223 | `		pMap->pCur = pMap->pFirst;` |
|    14 |  3224 | `	}` |
|    29 |  3225 | `}` |
|    50 |  3226 | `static void ApListNext(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  3227 | `{` |
|    51 |  3228 | `	ph7_hashmap *pMap = ApMap(pVm,pThis);` |
|    51 |  3229 | `	if( pMap && pMap->pCur ){` |
|    51 |  3230 | `		pMap->pCur = pMap->pCur->pPrev;   /* insertion order: pFirst, then the pPrev chain */` |
|    25 |  3231 | `	}` |
|    51 |  3232 | `}` |
|     - |  3233 | `/*` |
|     - |  3234 | ` * php's spl_append_it_next_iterator: drop the cache and the current inner, then` |
|     - |  3235 | ` * adopt whatever the list cursor points at (rewound). *pbOk is php's SUCCESS --` |
|     - |  3236 | ` * false means the list is exhausted and this iterator has nothing left.` |
|     - |  3237 | ` */` |
|   120 |  3238 | `static sxi32 ApAdoptCurrent(ph7_vm *pVm,ph7_class_instance *pThis,int *pbOk)` |
|     1 |  3239 | `{` |
|     - |  3240 | `	ph7_class_instance *pIt;` |
|   121 |  3241 | `	*pbOk = 0;` |
|   121 |  3242 | `	DualFree(pVm,pThis);` |
|   121 |  3243 | `	PH7_NativeSetAttrObj(pVm,pThis,IT_IN,0);` |
|   121 |  3244 | `	PH7_NativeSetAttrObj(pVm,pThis,IT_IT,0);` |
|   121 |  3245 | `	pIt = ApCurrent(pVm,pThis);` |
|   121 |  3246 | `	if( pIt == 0 ){` |
|    31 |  3247 | `		return SXRET_OK;` |
|     - |  3248 | `	}` |
|    91 |  3249 | `	PH7_NativeSetAttrObj(pVm,pThis,IT_IN,pIt);` |
|    91 |  3250 | `	PH7_NativeSetAttrObj(pVm,pThis,IT_IT,pIt);` |
|    91 |  3251 | `	*pbOk = 1;` |
|    91 |  3252 | `	return DualRewindInner(pVm,pThis);` |
|    61 |  3253 | `}` |
|     - |  3254 | `/*` |
|     - |  3255 | ` * php's spl_append_it_fetch: step over every exhausted inner iterator, then fill` |
|     - |  3256 | ` * the cache without re-asking valid() (php's check_more = 0 -- the loop above just` |
|     - |  3257 | ` * established it).` |
|     - |  3258 | ` */` |
|   116 |  3259 | `static sxi32 ApFetch(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  3260 | `{` |
|    78 |  3261 | `	for(;;){` |
|   137 |  3262 | `		int bValid = 0, bOk = 0;` |
|   137 |  3263 | `		sxi32 rc = DualInnerValid(pVm,pThis,&bValid);` |
|   137 |  3264 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  3265 | `			return rc;` |
|     - |  3266 | `		}` |
|   137 |  3267 | `		if( bValid ){` |
|    87 |  3268 | `			break;` |
|     - |  3269 | `		}` |
|    51 |  3270 | `		ApListNext(pVm,pThis);` |
|    51 |  3271 | `		rc = ApAdoptCurrent(pVm,pThis,&bOk);` |
|    51 |  3272 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  3273 | `			return rc;` |
|     - |  3274 | `		}` |
|    51 |  3275 | `		if( !bOk ){` |
|    31 |  3276 | `			return SXRET_OK;   /* nothing left: the cache stays empty and valid() is false */` |
|     - |  3277 | `		}` |
|     1 |  3278 | `	}` |
|    87 |  3279 | `	return DualFetch(pVm,pThis,FALSE);` |
|    59 |  3280 | `}` |
|    50 |  3281 | `static int vm_builtin_AppendIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3282 | `{` |
|    51 |  3283 | `	ph7_vm *pVm = pCtx->pVm;` |
|    51 |  3284 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3285 | `	ph7_class_instance *pList;` |
|     - |  3286 | `	ph7_class *pClass;` |
|     - |  3287 | `	ph7_class_method *pCons;` |
|    25 |  3288 | `	SXUNUSED(nArg);` |
|    25 |  3289 | `	SXUNUSED(apArg);` |
|    51 |  3290 | `	if( pThis == 0 ){` |
|   ! 0 |  3291 | `		return PH7_OK;` |
|     - |  3292 | `	}` |
|    51 |  3293 | `	if( ApList(pThis) != 0 ){` |
|     - |  3294 | `		/* php's "already built" refusal, worded from the DECLARING class as everywhere` |
|     - |  3295 | `		 * else in the family. */` |
|   ! 0 |  3296 | `		return PH7_VmThrowException(pCtx,"BadMethodCallException",` |
|     - |  3297 | `			"AppendIterator::getIterator() must be called exactly once per instance");` |
|     - |  3298 | `	}` |
|    51 |  3299 | `	pClass = PH7_VmExtractClass(pVm,"ArrayIterator",sizeof("ArrayIterator")-1,FALSE,0);` |
|    51 |  3300 | `	if( pClass == 0 ){` |
|   ! 0 |  3301 | `		return PH7_OK;` |
|     - |  3302 | `	}` |
|    51 |  3303 | `	pList = PH7_NewClassInstance(pVm,pClass);` |
|    51 |  3304 | `	if( pList == 0 ){` |
|   ! 0 |  3305 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  3306 | `	}` |
|    51 |  3307 | `	pList->iRef++;` |
|    51 |  3308 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|    51 |  3309 | `	if( pCons ){` |
|    51 |  3310 | `		PH7_VmCallClassMethod(pVm,pList,pCons,0,0,0);` |
|    25 |  3311 | `	}` |
|    51 |  3312 | `	PH7_NativeSetAttrObj(pVm,pThis,AP_LIST,pList);   /* the slot takes its own reference */` |
|    51 |  3313 | `	PH7_ClassInstanceUnref(pList);` |
|    51 |  3314 | `	return PH7_OK;` |
|    26 |  3315 | `}` |
|     - |  3316 | `/*` |
|     - |  3317 | ` * append(). php's own sequence, and every branch of it is observable:` |
|     - |  3318 | ` *   - a list cursor sitting on a LIVE entry whose cache is empty means the walk has` |
|     - |  3319 | ` *     consumed that entry, so the new iterator goes in behind it and the cursor steps` |
|     - |  3320 | ` *     over;` |
|     - |  3321 | ` *   - if nothing is being iterated yet (or the cache is empty), the cursor is walked` |
|     - |  3322 | ` *     forward until it reaches the iterator just appended, and the fetch resumes there.` |
|     - |  3323 | ` * That second half is what makes an AppendIterator RESUME after exhaustion, and it` |
|     - |  3324 | ` * relies on ArrayIterator::append() reviving a cursor that ran off the end (see` |
|     - |  3325 | ` * SplStoreInsert).` |
|     - |  3326 | ` */` |
|    60 |  3327 | `static int vm_builtin_AppendIterator_append(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3328 | `{` |
|    61 |  3329 | `	ph7_vm *pVm = pCtx->pVm;` |
|    61 |  3330 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3331 | `	ph7_class_instance *pIt;` |
|     - |  3332 | `	ph7_hashmap *pMap;` |
|    61 |  3333 | `	int bListValid,bInnerValid = 0,nGuard;` |
|     - |  3334 | `	sxi32 rc;` |
|    61 |  3335 | `	if( !DualReady(pThis) ){` |
|   ! 0 |  3336 | `		return DualNotReady(pCtx);` |
|     - |  3337 | `	}` |
|    61 |  3338 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 |  3339 | `		return PH7_OK;   /* the shared ZPP screen already refused a non-Iterator */` |
|     - |  3340 | `	}` |
|    61 |  3341 | `	pIt = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    61 |  3342 | `	pMap = ApMap(pVm,pThis);` |
|    61 |  3343 | `	bListValid = pMap && pMap->pCur;` |
|     - |  3344 | `	/* php's spl_dual_it_valid, both times it appears below: the INNER iterator's` |
|     - |  3345 | `	 * valid() (false when there is no inner at all), NOT the cache. */` |
|    61 |  3346 | `	rc = DualInnerValid(pVm,pThis,&bInnerValid);` |
|    61 |  3347 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  3348 | `		return rc;` |
|     - |  3349 | `	}` |
|    61 |  3350 | `	pMap = ApMap(pVm,pThis);   /* that call ran user code: re-resolve */` |
|    61 |  3351 | `	if( pMap ){` |
|    61 |  3352 | `		SplStoreInsert(pMap,0,apArg[0]);` |
|    30 |  3353 | `	}` |
|    61 |  3354 | `	if( bListValid && !bInnerValid ){` |
|   ! 0 |  3355 | `		ApListNext(pVm,pThis);` |
|   ! 0 |  3356 | `	}` |
|    61 |  3357 | `	if( PH7_NativeAttrObj(pThis,IT_IT) != 0 && bInnerValid ){` |
|    19 |  3358 | `		return PH7_OK;   /* mid-walk with a live element: the new tail waits its turn */` |
|     - |  3359 | `	}` |
|    43 |  3360 | `	pMap = ApMap(pVm,pThis);` |
|    43 |  3361 | `	if( pMap && pMap->pCur == 0 ){` |
|   ! 0 |  3362 | `		ApListRewind(pVm,pThis);` |
|   ! 0 |  3363 | `	}` |
|    22 |  3364 | `	for( nGuard = 0 ; ; ++nGuard ){` |
|    43 |  3365 | `		int bOk = 0;` |
|    43 |  3366 | `		rc = ApAdoptCurrent(pVm,pThis,&bOk);` |
|    43 |  3367 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  3368 | `			return rc;` |
|     - |  3369 | `		}` |
|    43 |  3370 | `		if( !bOk \|\| PH7_NativeAttrObj(pThis,IT_IN) == pIt ){` |
|    22 |  3371 | `			break;` |
|     - |  3372 | `		}` |
|   ! 0 |  3373 | `		ApListNext(pVm,pThis);` |
|   ! 0 |  3374 | `		if( nGuard > 100000 ){` |
|   ! 0 |  3375 | `			break;   /* php's loop has no bound; ours refuses to spin on a mutated list */` |
|     - |  3376 | `		}` |
|   ! 0 |  3377 | `	}` |
|    43 |  3378 | `	rc = ApFetch(pVm,pThis);` |
|    43 |  3379 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    31 |  3380 | `}` |
|    28 |  3381 | `static int vm_builtin_AppendIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3382 | `{` |
|    29 |  3383 | `	ph7_vm *pVm = pCtx->pVm;` |
|    29 |  3384 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3385 | `	sxi32 rc;` |
|    29 |  3386 | `	int bOk = 0;` |
|    14 |  3387 | `	SXUNUSED(nArg);` |
|    14 |  3388 | `	SXUNUSED(apArg);` |
|    29 |  3389 | `	if( !DualReady(pThis) ){` |
|   ! 0 |  3390 | `		return DualNotReady(pCtx);` |
|     - |  3391 | `	}` |
|    29 |  3392 | `	ApListRewind(pVm,pThis);` |
|    29 |  3393 | `	rc = ApAdoptCurrent(pVm,pThis,&bOk);` |
|    29 |  3394 | `	if( rc == SXRET_OK && bOk ){` |
|    29 |  3395 | `		rc = ApFetch(pVm,pThis);` |
|    14 |  3396 | `	}` |
|    29 |  3397 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    15 |  3398 | `}` |
|    46 |  3399 | `static int vm_builtin_AppendIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3400 | `{` |
|    47 |  3401 | `	ph7_vm *pVm = pCtx->pVm;` |
|    47 |  3402 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3403 | `	sxi32 rc;` |
|    47 |  3404 | `	int bValid = 0;` |
|    23 |  3405 | `	SXUNUSED(nArg);` |
|    23 |  3406 | `	SXUNUSED(apArg);` |
|    47 |  3407 | `	if( !DualReady(pThis) ){` |
|   ! 0 |  3408 | `		return DualNotReady(pCtx);` |
|     - |  3409 | `	}` |
|    47 |  3410 | `	rc = DualInnerValid(pVm,pThis,&bValid);` |
|    47 |  3411 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  3412 | `		return rc;` |
|     - |  3413 | `	}` |
|    47 |  3414 | `	if( bValid ){` |
|    47 |  3415 | `		rc = DualNextInner(pVm,pThis);` |
|    47 |  3416 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  3417 | `			return rc;` |
|     - |  3418 | `		}` |
|    23 |  3419 | `	}` |
|    47 |  3420 | `	rc = ApFetch(pVm,pThis);` |
|    47 |  3421 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    24 |  3422 | `}` |
|     - |  3423 | `/* php re-fetches here (spl_dual_it_fetch with check_more), which is why an` |
|     - |  3424 | ` * AppendIterator FOLLOWS an inner iterator moved behind its back where every other` |
|     - |  3425 | ` * decorator answers its cache. */` |
|    64 |  3426 | `static int vm_builtin_AppendIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3427 | `{` |
|    65 |  3428 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3429 | `	sxi32 rc;` |
|    32 |  3430 | `	SXUNUSED(nArg);` |
|    32 |  3431 | `	SXUNUSED(apArg);` |
|    65 |  3432 | `	if( !DualReady(pThis) ){` |
|   ! 0 |  3433 | `		return DualNotReady(pCtx);` |
|     - |  3434 | `	}` |
|    65 |  3435 | `	rc = DualFetch(pCtx->pVm,pThis,TRUE);` |
|    65 |  3436 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  3437 | `		return rc;` |
|     - |  3438 | `	}` |
|    65 |  3439 | `	return DualResultSlot(pCtx,IT_CD);` |
|    33 |  3440 | `}` |
|     - |  3441 | `/* The list cursor's KEY, which is php's index into the appended iterators -- and` |
|     - |  3442 | ` * NULL once the cursor has run off the end, where the chunk kept answering the last` |
|     - |  3443 | ` * index it had seen. */` |
|    26 |  3444 | `static int vm_builtin_AppendIterator_getIteratorIndex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3445 | `{` |
|    27 |  3446 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3447 | `	ph7_value *pSlot,*apCall[1];` |
|    13 |  3448 | `	SXUNUSED(nArg);` |
|    13 |  3449 | `	SXUNUSED(apArg);` |
|    27 |  3450 | `	if( !DualReady(pThis) ){` |
|   ! 0 |  3451 | `		return DualNotReady(pCtx);` |
|     - |  3452 | `	}` |
|    27 |  3453 | `	pSlot = SplStoreSlot(pCtx->pVm,ApList(pThis));` |
|    27 |  3454 | `	if( pSlot == 0 ){` |
|   ! 0 |  3455 | `		ph7_result_null(pCtx);` |
|   ! 0 |  3456 | `		return PH7_OK;` |
|     - |  3457 | `	}` |
|    27 |  3458 | `	apCall[0] = pSlot;` |
|    27 |  3459 | `	return ph7_hashmap_simple_key(pCtx,1,apCall);` |
|    14 |  3460 | `}` |
|     - |  3461 | `/*` |
|     - |  3462 | ` * ---------------------------------------------------------------------------` |
|     - |  3463 | ` * The RECURSIVE pair: RecursiveArrayIterator (an ArrayIterator that descends into` |
|     - |  3464 | ` * its own entries) and RecursiveFilterIterator (a FilterIterator that forwards the` |
|     - |  3465 | ` * two recursion methods to its inner iterator).` |
|     - |  3466 | ` *` |
|     - |  3467 | ` * RecursiveArrayIterator is where php's CHILD_ARRAYS_ONLY flag lives, and the` |
|     - |  3468 | ``  * chunk's two-line `is_array($c) \|\| is_object($c)` / `new $c($this->current())` `` |
|     - |  3469 | ` * ignored it in both directions: an OBJECT entry claimed children under a flag that` |
|     - |  3470 | ` * exists to say it has none, and the child iterator was built WITHOUT the parent's` |
|     - |  3471 | ` * flags, so the restriction lasted exactly one level. php also answers null rather` |
|     - |  3472 | ` * than descending when there is no current element, and hands back an entry that is` |
|     - |  3473 | ` * ALREADY an instance of the called class instead of wrapping it again.` |
|     - |  3474 | ` */` |
|     - |  3475 | `#define RAI_CHILD_ARRAYS_ONLY 4` |
|     - |  3476 | `/* The entry the store cursor is on, or 0 past the end (php's` |
|     - |  3477 | ` * zend_hash_get_current_data_ex, which every one of these four bodies starts with). */` |
|   598 |  3478 | `static ph7_value * RaiCurrentEntry(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  3479 | `{` |
|   599 |  3480 | `	ph7_hashmap *pMap = SplStore(pVm,pThis);` |
|   599 |  3481 | `	return (pMap && pMap->pCur) ? HashmapExtractNodeValue(pMap->pCur) : 0;` |
|     1 |  3482 | `}` |
|   436 |  3483 | `static int vm_builtin_RecursiveArrayIterator_hasChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3484 | `{` |
|   437 |  3485 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   437 |  3486 | `	ph7_value *pEntry = RaiCurrentEntry(pCtx->pVm,pThis);` |
|   437 |  3487 | `	int bHas = 0;` |
|   218 |  3488 | `	SXUNUSED(nArg);` |
|   218 |  3489 | `	SXUNUSED(apArg);` |
|   437 |  3490 | `	if( pEntry ){` |
|   435 |  3491 | `		if( pEntry->iFlags & MEMOBJ_HASHMAP ){` |
|   165 |  3492 | `			bHas = 1;` |
|   353 |  3493 | `		}else if( pEntry->iFlags & MEMOBJ_OBJ ){` |
|     - |  3494 | `			/* php: an object is a child UNLESS the iterator was told arrays only. */` |
|    13 |  3495 | `			bHas = (PH7_NativeAttrInt(pThis,SPL_F) & RAI_CHILD_ARRAYS_ONLY) == 0;` |
|     6 |  3496 | `		}` |
|   217 |  3497 | `	}` |
|   437 |  3498 | `	ph7_result_bool(pCtx,bHas);` |
|   437 |  3499 | `	return PH7_OK;` |
|     1 |  3500 | `}` |
|   162 |  3501 | `static int vm_builtin_RecursiveArrayIterator_getChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3502 | `{` |
|   163 |  3503 | `	ph7_vm *pVm = pCtx->pVm;` |
|   163 |  3504 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   163 |  3505 | `	ph7_value *pEntry = RaiCurrentEntry(pVm,pThis);` |
|     - |  3506 | `	ph7_class_instance *pChild;` |
|     - |  3507 | `	ph7_class_method *pCons;` |
|     - |  3508 | `	ph7_value sEntry,sFlags,*apCtor[2];` |
|     - |  3509 | `	sxi64 iFlags;` |
|     - |  3510 | `	sxi32 rc;` |
|    81 |  3511 | `	SXUNUSED(nArg);` |
|    81 |  3512 | `	SXUNUSED(apArg);` |
|   163 |  3513 | `	if( pThis == 0 \|\| pEntry == 0 ){` |
|     3 |  3514 | `		ph7_result_null(pCtx);   /* php descends into nothing when nothing is current */` |
|     3 |  3515 | `		return PH7_OK;` |
|     - |  3516 | `	}` |
|   161 |  3517 | `	iFlags = PH7_NativeAttrInt(pThis,SPL_F);` |
|   161 |  3518 | `	if( pEntry->iFlags & MEMOBJ_OBJ ){` |
|     9 |  3519 | `		ph7_class_instance *pObj = (ph7_class_instance *)pEntry->x.pOther;` |
|     9 |  3520 | `		if( iFlags & RAI_CHILD_ARRAYS_ONLY ){` |
|     3 |  3521 | `			ph7_result_null(pCtx);` |
|     3 |  3522 | `			return PH7_OK;` |
|     - |  3523 | `		}` |
|     7 |  3524 | `		if( pObj && PH7_VmInstanceOf(pObj->pClass,pThis->pClass) ){` |
|     - |  3525 | `			/* Already one of us: php hands the entry back rather than wrapping it. */` |
|     3 |  3526 | `			SplResultBorrowed(pCtx,pObj);` |
|     3 |  3527 | `			return PH7_OK;` |
|     - |  3528 | `		}` |
|     2 |  3529 | `	}` |
|     - |  3530 | `	/* php's spl_instantiate_child_arg: the CALLED class, constructed with the entry` |
|     - |  3531 | `	 * AND the parent's flags -- which is what carries CHILD_ARRAYS_ONLY down. */` |
|   157 |  3532 | `	pChild = PH7_NewClassInstance(pVm,pThis->pClass);` |
|   157 |  3533 | `	if( pChild == 0 ){` |
|   ! 0 |  3534 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  3535 | `	}` |
|   157 |  3536 | `	pChild->iRef++;` |
|   157 |  3537 | `	PH7_MemObjInit(pVm,&sEntry);` |
|   157 |  3538 | `	PH7_MemObjStore(pEntry,&sEntry);` |
|   157 |  3539 | `	PH7_MemObjInitFromInt(pVm,&sFlags,iFlags);` |
|   157 |  3540 | `	apCtor[0] = &sEntry;` |
|   157 |  3541 | `	apCtor[1] = &sFlags;` |
|   157 |  3542 | `	pCons = PH7_ClassExtractMethod(pThis->pClass,"__construct",sizeof("__construct")-1);` |
|   157 |  3543 | `	rc = pCons ? PH7_VmCallClassMethod(pVm,pChild,pCons,0,2,apCtor) : SXRET_OK;` |
|   157 |  3544 | `	PH7_MemObjRelease(&sEntry);` |
|   157 |  3545 | `	PH7_MemObjRelease(&sFlags);` |
|   157 |  3546 | `	if( rc != SXRET_OK ){` |
|     7 |  3547 | `		PH7_ClassInstanceUnref(pChild);` |
|     7 |  3548 | `		return rc;` |
|     - |  3549 | `	}` |
|   151 |  3550 | `	PH7_NativeResultObject(pCtx,pChild);` |
|   151 |  3551 | `	PH7_ClassInstanceUnref(pChild);` |
|   151 |  3552 | `	return PH7_OK;` |
|    82 |  3553 | `}` |
|     - |  3554 | `/* RecursiveFilterIterator forwards both methods to the object getInnerIterator()` |
|     - |  3555 | ` * answers (php calls on inner.zobject), and wraps the children in ITS OWN class. */` |
|    20 |  3556 | `static int vm_builtin_RecursiveFilterIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3557 | `{` |
|    21 |  3558 | `	return DualConstruct(pCtx,"RecursiveFilterIterator",nArg,apArg);` |
|     1 |  3559 | `}` |
|   104 |  3560 | `static sxi32 RfiCallInner(ph7_context *pCtx,const char *zName,sxu32 nName,ph7_value *pOut)` |
|     1 |  3561 | `{` |
|   105 |  3562 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   105 |  3563 | `	ph7_class_instance *pIn = pThis ? PH7_NativeAttrObj(pThis,IT_IN) : 0;` |
|   105 |  3564 | `	ph7_class_method *pMethod = pIn ? PH7_ClassExtractMethod(pIn->pClass,zName,nName) : 0;` |
|   105 |  3565 | `	if( pMethod == 0 ){` |
|   ! 0 |  3566 | `		return SXRET_OK;` |
|     - |  3567 | `	}` |
|   105 |  3568 | `	return PH7_VmCallClassMethod(pCtx->pVm,pIn,pMethod,pOut,0,0);` |
|    53 |  3569 | `}` |
|    46 |  3570 | `static int vm_builtin_RecursiveFilterIterator_hasChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3571 | `{` |
|    47 |  3572 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3573 | `	ph7_value sRes;` |
|     - |  3574 | `	sxi32 rc;` |
|    23 |  3575 | `	SXUNUSED(nArg);` |
|    23 |  3576 | `	SXUNUSED(apArg);` |
|    47 |  3577 | `	if( !DualReady(pThis) ){` |
|     5 |  3578 | `		return DualNotReady(pCtx);` |
|     - |  3579 | `	}` |
|    43 |  3580 | `	PH7_MemObjInit(pCtx->pVm,&sRes);` |
|    43 |  3581 | `	rc = RfiCallInner(pCtx,"hasChildren",sizeof("hasChildren")-1,&sRes);` |
|    43 |  3582 | `	if( rc == SXRET_OK ){` |
|    43 |  3583 | `		ph7_result_value(pCtx,&sRes);` |
|    21 |  3584 | `	}` |
|    43 |  3585 | `	PH7_MemObjRelease(&sRes);` |
|    43 |  3586 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    24 |  3587 | `}` |
|     - |  3588 | `/*` |
|     - |  3589 | ` * php's spl_instantiate_arg_ex1/2/3 for the recursive filters: fetch the INNER` |
|     - |  3590 | ` * iterator's children and hand them to a fresh instance of the CALLED class` |
|     - |  3591 | `` * (`Z_OBJCE_P(ZEND_THIS)`, so a user subclass answers its own type), followed by`` |
|     - |  3592 | ` * whatever the subclass's constructor needs after the iterator -- the callback for` |
|     - |  3593 | ` * RecursiveCallbackFilterIterator, the four regex arguments for` |
|     - |  3594 | ` * RecursiveRegexIterator, nothing for the other two.` |
|     - |  3595 | ` */` |
|    26 |  3596 | `static int RfiBuildChild(ph7_context *pCtx,ph7_value **apExtra,int nExtra)` |
|     1 |  3597 | `{` |
|    27 |  3598 | `	ph7_vm *pVm = pCtx->pVm;` |
|    27 |  3599 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3600 | `	ph7_class_instance *pChild;` |
|     - |  3601 | `	ph7_class_method *pCons;` |
|     - |  3602 | `	ph7_value sInner,*apCtor[5];` |
|     - |  3603 | `	int i;` |
|     - |  3604 | `	sxi32 rc;` |
|    27 |  3605 | `	if( !DualReady(pThis) ){` |
|   ! 0 |  3606 | `		return DualNotReady(pCtx);` |
|     - |  3607 | `	}` |
|    27 |  3608 | `	if( nExtra > (int)SX_ARRAYSIZE(apCtor) - 1 ){` |
|   ! 0 |  3609 | `		nExtra = (int)SX_ARRAYSIZE(apCtor) - 1;` |
|   ! 0 |  3610 | `	}` |
|    27 |  3611 | `	PH7_MemObjInit(pVm,&sInner);` |
|    27 |  3612 | `	rc = RfiCallInner(pCtx,"getChildren",sizeof("getChildren")-1,&sInner);` |
|    27 |  3613 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  3614 | `		PH7_MemObjRelease(&sInner);` |
|   ! 0 |  3615 | `		return rc;` |
|     - |  3616 | `	}` |
|    27 |  3617 | `	pChild = PH7_NewClassInstance(pVm,pThis->pClass);` |
|    27 |  3618 | `	if( pChild == 0 ){` |
|   ! 0 |  3619 | `		PH7_MemObjRelease(&sInner);` |
|   ! 0 |  3620 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  3621 | `	}` |
|    27 |  3622 | `	pChild->iRef++;` |
|    27 |  3623 | `	apCtor[0] = &sInner;` |
|    49 |  3624 | `	for( i = 0 ; i < nExtra ; ++i ){` |
|    23 |  3625 | `		apCtor[i+1] = apExtra[i];` |
|    12 |  3626 | `	}` |
|    27 |  3627 | `	pCons = PH7_ClassExtractMethod(pThis->pClass,"__construct",sizeof("__construct")-1);` |
|    27 |  3628 | `	rc = pCons ? PH7_VmCallClassMethod(pVm,pChild,pCons,0,nExtra+1,apCtor) : SXRET_OK;` |
|    27 |  3629 | `	PH7_MemObjRelease(&sInner);` |
|    27 |  3630 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  3631 | `		PH7_ClassInstanceUnref(pChild);` |
|   ! 0 |  3632 | `		return rc;` |
|     - |  3633 | `	}` |
|    27 |  3634 | `	PH7_NativeResultObject(pCtx,pChild);` |
|    27 |  3635 | `	PH7_ClassInstanceUnref(pChild);` |
|    27 |  3636 | `	return PH7_OK;` |
|    14 |  3637 | `}` |
|    16 |  3638 | `static int vm_builtin_RecursiveFilterIterator_getChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3639 | `{` |
|     8 |  3640 | `	SXUNUSED(nArg);` |
|     8 |  3641 | `	SXUNUSED(apArg);` |
|    17 |  3642 | `	return RfiBuildChild(pCtx,0,0);` |
|     1 |  3643 | `}` |
|     - |  3644 | `/*` |
|     - |  3645 | ` * ParentIterator: the RecursiveFilterIterator whose accept() IS the question` |
|     - |  3646 | ` * "does the current element have children?". php asks the INNER iterator` |
|     - |  3647 | `` * (`inner.zobject`), not `$this`, so overriding hasChildren() on the`` |
|     - |  3648 | ` * ParentIterator subclass changes nothing and overriding it on the inner` |
|     - |  3649 | ` * RecursiveIterator changes everything -- and the answer is cast to a bool.` |
|     - |  3650 | ` */` |
|    22 |  3651 | `static int vm_builtin_ParentIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3652 | `{` |
|    23 |  3653 | `	return DualConstruct(pCtx,"ParentIterator",nArg,apArg);` |
|     1 |  3654 | `}` |
|    38 |  3655 | `static int vm_builtin_ParentIterator_accept(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3656 | `{` |
|    39 |  3657 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3658 | `	ph7_value sRes;` |
|     - |  3659 | `	sxi32 rc;` |
|    19 |  3660 | `	SXUNUSED(nArg);` |
|    19 |  3661 | `	SXUNUSED(apArg);` |
|    39 |  3662 | `	if( !DualReady(pThis) ){` |
|     3 |  3663 | `		return DualNotReady(pCtx);` |
|     - |  3664 | `	}` |
|    37 |  3665 | `	PH7_MemObjInit(pCtx->pVm,&sRes);` |
|    37 |  3666 | `	rc = RfiCallInner(pCtx,"hasChildren",sizeof("hasChildren")-1,&sRes);` |
|    37 |  3667 | `	if( rc == SXRET_OK ){` |
|    37 |  3668 | `		PH7_MemObjToBool(&sRes);          /* a STATUS, not the answer */` |
|    37 |  3669 | `		ph7_result_bool(pCtx,sRes.x.iVal != 0);` |
|    18 |  3670 | `	}` |
|    37 |  3671 | `	PH7_MemObjRelease(&sRes);` |
|    37 |  3672 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    20 |  3673 | `}` |
|     - |  3674 | `/*` |
|     - |  3675 | ` * RecursiveCallbackFilterIterator: the callback filter's recursive twin. Both` |
|     - |  3676 | ` * halves are inherited behaviour -- accept() is CallbackFilterIterator's and` |
|     - |  3677 | ` * hasChildren() is RecursiveFilterIterator's -- but php DECLARES all four names on` |
|     - |  3678 | ` * the class, and getChildren() has to carry the callback down to the child.` |
|     - |  3679 | ` */` |
|    14 |  3680 | `static int vm_builtin_RecursiveCallbackFilterIterator_construct(ph7_context *pCtx,int nArg,` |
|     - |  3681 | `	ph7_value **apArg)` |
|     1 |  3682 | `{` |
|     - |  3683 | `	ph7_class_instance *pThis;` |
|     - |  3684 | `	sxi32 rc;` |
|    15 |  3685 | `	if( nArg > 1 ){` |
|    15 |  3686 | `		rc = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",FALSE);` |
|    15 |  3687 | `		if( rc != PH7_OK ){` |
|     3 |  3688 | `			return rc;` |
|     - |  3689 | `		}` |
|     6 |  3690 | `	}` |
|    13 |  3691 | `	rc = DualConstruct(pCtx,"RecursiveCallbackFilterIterator",nArg,apArg);` |
|    13 |  3692 | `	pThis = PH7_ContextThis(pCtx);` |
|    13 |  3693 | `	if( rc == PH7_OK && pThis && nArg > 1 ){` |
|    13 |  3694 | `		DualSetSlot(pCtx->pVm,pThis,IT_CB,apArg[1]);` |
|     6 |  3695 | `	}` |
|    13 |  3696 | `	return rc;` |
|     8 |  3697 | `}` |
|     8 |  3698 | `static int vm_builtin_RecursiveCallbackFilterIterator_getChildren(ph7_context *pCtx,int nArg,` |
|     - |  3699 | `	ph7_value **apArg)` |
|     1 |  3700 | `{` |
|     9 |  3701 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3702 | `	ph7_value sCb,*apExtra[1];` |
|     - |  3703 | `	int rc;` |
|     4 |  3704 | `	SXUNUSED(nArg);` |
|     4 |  3705 | `	SXUNUSED(apArg);` |
|     9 |  3706 | `	if( !DualReady(pThis) ){` |
|     3 |  3707 | `		return DualNotReady(pCtx);` |
|     - |  3708 | `	}` |
|     - |  3709 | `	/* Take the callback as a VALUE: the child's constructor runs user code, and a` |
|     - |  3710 | `	 * pointer into pVm->aMemObj does not survive that. */` |
|     7 |  3711 | `	PH7_MemObjInit(pCtx->pVm,&sCb);` |
|     - |  3712 | `	{` |
|     7 |  3713 | `		ph7_value *pCb = PH7_NativeAttr(pThis,IT_CB);` |
|     7 |  3714 | `		if( pCb ){` |
|     7 |  3715 | `			PH7_MemObjStore(pCb,&sCb);` |
|     3 |  3716 | `		}` |
|     - |  3717 | `	}` |
|     7 |  3718 | `	apExtra[0] = &sCb;` |
|     7 |  3719 | `	rc = RfiBuildChild(pCtx,apExtra,1);` |
|     7 |  3720 | `	PH7_MemObjRelease(&sCb);` |
|     7 |  3721 | `	return rc;` |
|     5 |  3722 | `}` |
|     - |  3723 | `/*` |
|     - |  3724 | ` * RecursiveRegexIterator: the regex filter's recursive twin. Its accept() has one` |
|     - |  3725 | ` * rule of its own, and it comes BEFORE everything RegexIterator does: a current()` |
|     - |  3726 | ` * that is an ARRAY is accepted when it is non-empty, whatever the mode, the` |
|     - |  3727 | ` * pattern, USE_KEY or INVERT_MATCH say -- a container is kept so the walk can` |
|     - |  3728 | ` * descend into it, and only its LEAVES are matched. (RegexIterator itself refuses` |
|     - |  3729 | ` * an array outright, which is what makes the plain class useless recursively.)` |
|     - |  3730 | ``  * getChildren() carries the four regex arguments down; php passes `replacement` `` |
|     - |  3731 | ` * to nothing, so a child starts with the declared NULL.` |
|     - |  3732 | ` */` |
|    26 |  3733 | `static int vm_builtin_RecursiveRegexIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3734 | `{` |
|    27 |  3735 | `	return RegitConstruct(pCtx,"RecursiveRegexIterator",nArg,apArg);` |
|     1 |  3736 | `}` |
|    28 |  3737 | `static int vm_builtin_RecursiveRegexIterator_accept(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3738 | `{` |
|    29 |  3739 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    29 |  3740 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|     3 |  3741 | `		return DualNotReady(pCtx);` |
|     - |  3742 | `	}` |
|    27 |  3743 | `	if( DualFilled(pThis) ){` |
|    27 |  3744 | `		ph7_value *pCur = PH7_NativeAttr(pThis,IT_CD);` |
|    27 |  3745 | `		if( pCur && (pCur->iFlags & MEMOBJ_HASHMAP) && pCur->x.pOther ){` |
|    17 |  3746 | `			ph7_result_bool(pCtx,((ph7_hashmap *)pCur->x.pOther)->nEntry > 0);` |
|    17 |  3747 | `			return PH7_OK;` |
|     - |  3748 | `		}` |
|     5 |  3749 | `	}` |
|    11 |  3750 | `	return vm_builtin_RegexIterator_accept(pCtx,nArg,apArg);` |
|    15 |  3751 | `}` |
|     6 |  3752 | `static int vm_builtin_RecursiveRegexIterator_getChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3753 | `{` |
|     7 |  3754 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 |  3755 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3756 | `	ph7_value sRe,sMode,sFlags,sPreg,*apExtra[4];` |
|     - |  3757 | `	ph7_value *pRe;` |
|     - |  3758 | `	int rc;` |
|     3 |  3759 | `	SXUNUSED(nArg);` |
|     3 |  3760 | `	SXUNUSED(apArg);` |
|     7 |  3761 | `	if( !DualReady(pThis) ){` |
|     3 |  3762 | `		return DualNotReady(pCtx);` |
|     - |  3763 | `	}` |
|     5 |  3764 | `	PH7_MemObjInit(pVm,&sRe);` |
|     5 |  3765 | `	pRe = PH7_NativeAttr(pThis,IT_RE);` |
|     5 |  3766 | `	if( pRe ){` |
|     5 |  3767 | `		PH7_MemObjStore(pRe,&sRe);` |
|     2 |  3768 | `	}` |
|     5 |  3769 | `	PH7_MemObjInitFromInt(pVm,&sMode,PH7_NativeAttrInt(pThis,IT_RM));` |
|     5 |  3770 | `	PH7_MemObjInitFromInt(pVm,&sFlags,PH7_NativeAttrInt(pThis,IT_RF));` |
|     5 |  3771 | `	PH7_MemObjInitFromInt(pVm,&sPreg,PH7_NativeAttrInt(pThis,IT_RP));` |
|     5 |  3772 | `	apExtra[0] = &sRe;` |
|     5 |  3773 | `	apExtra[1] = &sMode;` |
|     5 |  3774 | `	apExtra[2] = &sFlags;` |
|     5 |  3775 | `	apExtra[3] = &sPreg;` |
|     5 |  3776 | `	rc = RfiBuildChild(pCtx,apExtra,4);` |
|     5 |  3777 | `	PH7_MemObjRelease(&sRe);` |
|     5 |  3778 | `	PH7_MemObjRelease(&sMode);` |
|     5 |  3779 | `	PH7_MemObjRelease(&sFlags);` |
|     5 |  3780 | `	PH7_MemObjRelease(&sPreg);` |
|     5 |  3781 | `	return rc;` |
|     4 |  3782 | `}` |
|    12 |  3783 | `static int vm_builtin_AppendIterator_getArrayIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3784 | `{` |
|    13 |  3785 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3786 | `	ph7_class_instance *pList;` |
|     6 |  3787 | `	SXUNUSED(nArg);` |
|     6 |  3788 | `	SXUNUSED(apArg);` |
|    13 |  3789 | `	if( !DualReady(pThis) ){` |
|   ! 0 |  3790 | `		return DualNotReady(pCtx);` |
|     - |  3791 | `	}` |
|    13 |  3792 | `	pList = ApList(pThis);` |
|    13 |  3793 | `	if( pList ){` |
|    13 |  3794 | `		SplResultBorrowed(pCtx,pList);` |
|     7 |  3795 | `	}else{` |
|   ! 0 |  3796 | `		ph7_result_null(pCtx);` |
|     - |  3797 | `	}` |
|    13 |  3798 | `	return PH7_OK;` |
|     7 |  3799 | `}` |
|     - |  3800 | `/*` |
|     - |  3801 | ` * ---------------------------------------------------------------------------` |
|     - |  3802 | ` * CachingIterator and RecursiveCachingIterator.` |
|     - |  3803 | ` *` |
|     - |  3804 | ` * The caching iterator is one step AHEAD of the iterator it decorates: each fetch` |
|     - |  3805 | ` * copies current()/key() into the cache every dual iterator keeps and then ADVANCES` |
|     - |  3806 | ` * the inner one, which is what makes hasNext() answerable at all — it is the` |
|     - |  3807 | ` * inner's live valid(), asked after that step. Everything else this class does` |
|     - |  3808 | ` * happens inside the same fetch, in php's order: the FULL_CACHE entry is written,` |
|     - |  3809 | ` * then (for the recursive twin) the CHILDREN are built, then the string form is` |
|     - |  3810 | ` * computed, then the inner is advanced.` |
|     - |  3811 | ` *` |
|     - |  3812 | ` * That eager string is the class's least obvious rule. CALL_TOSTRING casts the` |
|     - |  3813 | ` * ELEMENT and TOSTRING_USE_INNER casts the inner ITERATOR, both at FETCH time, so` |
|     - |  3814 | `` * the default `new CachingIterator($it)` over objects with no __toString throws`` |
|     - |  3815 | `` * from rewind() and over arrays warns `Array to string conversion` once per`` |
|     - |  3816 | `` * element — neither waits for anyone to write `(string)$it`. The other two`` |
|     - |  3817 | ` * spellings (TOSTRING_USE_KEY / TOSTRING_USE_CURRENT) are read out of the cache at` |
|     - |  3818 | ` * __toString() time instead, and NO spelling at all is a BadMethodCallException` |
|     - |  3819 | ` * that names the RECEIVER's class.` |
|     - |  3820 | ` *` |
|     - |  3821 | ` * getFlags() answers the RAW word, php's private CIT_VALID (0x10000) included, so` |
|     - |  3822 | ` * a fetched iterator reports 65537 where its constructor was handed 1. setFlags()` |
|     - |  3823 | ` * keeps the high half and replaces the low one, and refuses to unset either of the` |
|     - |  3824 | ` * two flags whose machinery cannot be turned off mid-walk; the CONSTRUCTOR masks` |
|     - |  3825 | `` * with CIT_PUBLIC instead, so `new CachingIterator($it, 1024)` reports 1024 while`` |
|     - |  3826 | ` * setFlags(1024) on a default instance is a refusal.` |
|     - |  3827 | ` */` |
|     - |  3828 | `#define CIT_FL   "__cfl"    /* php's u.caching.flags, its private CIT_VALID included */` |
|     - |  3829 | `#define CIT_STR  "__cstr"   /* php's u.caching.zstr: the string computed at fetch */` |
|     - |  3830 | `#define CIT_CCH  "__ccch"   /* php's u.caching.zcache */` |
|     - |  3831 | `#define CIT_KIDS "__ckid"   /* php's u.caching.zchildren, the recursive twin's only state */` |
|     - |  3832 |  |
|     - |  3833 | `#define CIT_CALL_TOSTRING     0x00000001` |
|     - |  3834 | `#define CIT_TOSTRING_USE_KEY  0x00000002` |
|     - |  3835 | `#define CIT_TOSTRING_USE_CUR  0x00000004` |
|     - |  3836 | `#define CIT_TOSTRING_USE_INN  0x00000008` |
|     - |  3837 | `#define CIT_CATCH_GET_CHILD   0x00000010` |
|     - |  3838 | `#define CIT_FULL_CACHE        0x00000100` |
|     - |  3839 | `#define CIT_PUBLIC            0x0000FFFF` |
|     - |  3840 | `#define CIT_VALID             0x00010000` |
|     - |  3841 |  |
|   942 |  3842 | `static sxi64 CitFlags(ph7_class_instance *pThis)` |
|     3 |  3843 | `{` |
|   945 |  3844 | `	return pThis ? PH7_NativeAttrInt(pThis,CIT_FL) : 0;` |
|     3 |  3845 | `}` |
|     - |  3846 | `/* php's spl_cit_check_flags: at most ONE of the four string spellings. */` |
|   198 |  3847 | `static int CitCheckFlags(sxi64 iFlags)` |
|     3 |  3848 | `{` |
|   201 |  3849 | `	int n = 0;` |
|   201 |  3850 | `	if( iFlags & CIT_CALL_TOSTRING ){ n++; }` |
|   201 |  3851 | `	if( iFlags & CIT_TOSTRING_USE_KEY ){ n++; }` |
|   201 |  3852 | `	if( iFlags & CIT_TOSTRING_USE_CUR ){ n++; }` |
|   201 |  3853 | `	if( iFlags & CIT_TOSTRING_USE_INN ){ n++; }` |
|   201 |  3854 | `	return n <= 1;` |
|     3 |  3855 | `}` |
|     - |  3856 | `/* Both of this class's refusals name the RECEIVER's class and point at` |
|     - |  3857 | ` * CachingIterator::__construct whatever that receiver is. */` |
|    16 |  3858 | `static int CitRefuse(ph7_context *pCtx,ph7_class_instance *pThis,const char *zWhat)` |
|     1 |  3859 | `{` |
|    17 |  3860 | `	SyString *pName = &pThis->pClass->sName;` |
|    25 |  3861 | `	return PH7_VmThrowException(pCtx,"BadMethodCallException",` |
|     8 |  3862 | `		"%z does not %s (see CachingIterator::__construct)",pName,zWhat);` |
|     1 |  3863 | `}` |
|     - |  3864 | `/* The cache slot, separated for writing (every caller may mutate it). */` |
|    84 |  3865 | `static ph7_value * CitCacheSlot(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     3 |  3866 | `{` |
|    87 |  3867 | `	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,CIT_CCH) : 0;` |
|    87 |  3868 | `	if( pSlot == 0 ){` |
|   ! 0 |  3869 | `		return 0;` |
|     - |  3870 | `	}` |
|    87 |  3871 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     3 |  3872 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|   ! 0 |  3873 | `			return 0;` |
|     - |  3874 | `		}` |
|     1 |  3875 | `	}` |
|    87 |  3876 | `	if( PH7_HashmapCowSeparate(pVm,pSlot) == 0 ){` |
|   ! 0 |  3877 | `		return 0;` |
|     - |  3878 | `	}` |
|     - |  3879 | `	/* Re-ask: the separation can MOVE the memobj pool this slot lives in` |
|     - |  3880 | `	 * (SplStoreSlot explains it). */` |
|    87 |  3881 | `	return PH7_NativeAttr(pThis,CIT_CCH);` |
|    45 |  3882 | `}` |
|     - |  3883 | `/* Every cache reader is refused outright without FULL_CACHE, php's own guard. */` |
|    74 |  3884 | `static ph7_value * CitCacheChecked(ph7_context *pCtx,int *pRc)` |
|     3 |  3885 | `{` |
|    77 |  3886 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    77 |  3887 | `	*pRc = PH7_OK;` |
|    77 |  3888 | `	if( !DualReady(pThis) ){` |
|     5 |  3889 | `		*pRc = DualNotReady(pCtx);` |
|     5 |  3890 | `		return 0;` |
|     - |  3891 | `	}` |
|    73 |  3892 | `	if( (CitFlags(pThis) & CIT_FULL_CACHE) == 0 ){` |
|    13 |  3893 | `		*pRc = CitRefuse(pCtx,pThis,"use a full cache");` |
|    13 |  3894 | `		return 0;` |
|     - |  3895 | `	}` |
|    61 |  3896 | `	return CitCacheSlot(pCtx->pVm,pThis);` |
|    40 |  3897 | `}` |
|     - |  3898 | `/*` |
|     - |  3899 | ` * php's spl_caching_it_next tail: the string the class will answer from. The two` |
|     - |  3900 | ` * eager spellings are exclusive (spl_cit_check_flags saw to that), and the cast is` |
|     - |  3901 | ` * php's own, warnings and refusals included.` |
|     - |  3902 | ` */` |
|    16 |  3903 | `static sxi32 CitMakeString(ph7_context *pCtx,ph7_class_instance *pThis,sxi64 iFlags)` |
|     1 |  3904 | `{` |
|    17 |  3905 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  3906 | `	ph7_value sVal;` |
|     - |  3907 | `	sxi32 rc;` |
|    17 |  3908 | `	PH7_MemObjInit(pVm,&sVal);` |
|    17 |  3909 | `	if( iFlags & CIT_TOSTRING_USE_INN ){` |
|     3 |  3910 | `		ph7_class_instance *pIn = PH7_NativeAttrObj(pThis,IT_IN);` |
|     3 |  3911 | `		if( pIn ){` |
|     - |  3912 | `			/* The temporary OWNS this reference: PH7_MemObjRelease below drops one,` |
|     - |  3913 | `			 * and the cast itself may retype the slot out from under the object. */` |
|     3 |  3914 | `			pIn->iRef++;` |
|     3 |  3915 | `			sVal.x.pOther = pIn;` |
|     3 |  3916 | `			MemObjSetType(&sVal,MEMOBJ_OBJ);` |
|     1 |  3917 | `		}` |
|     2 |  3918 | `	}else{` |
|    15 |  3919 | `		ph7_value *pCur = PH7_NativeAttr(pThis,IT_CD);` |
|    15 |  3920 | `		if( pCur ){` |
|    15 |  3921 | `			PH7_MemObjStore(pCur,&sVal);` |
|     7 |  3922 | `		}` |
|     - |  3923 | `	}` |
|    17 |  3924 | `	rc = PH7_MemObjToStringUV(&sVal);` |
|    17 |  3925 | `	if( rc == SXRET_OK ){` |
|    15 |  3926 | `		DualSetSlot(pVm,pThis,CIT_STR,&sVal);` |
|     7 |  3927 | `	}` |
|    17 |  3928 | `	PH7_MemObjRelease(&sVal);` |
|    17 |  3929 | `	return rc;` |
|     1 |  3930 | `}` |
|     - |  3931 | `/*` |
|     - |  3932 | ` * php's recursion half of the same fetch: ask the INNER iterator whether the` |
|     - |  3933 | ` * element has children and, if it does, build the child decorator EAGERLY —` |
|     - |  3934 | ` * getChildren() only hands back what this already made. CATCH_GET_CHILD swallows` |
|     - |  3935 | ` * a throw from any of the three steps (hasChildren, getChildren, and the child's` |
|     - |  3936 | ` * own constructor), which is php's clear-the-exception-and-carry-on.` |
|     - |  3937 | ` *` |
|     - |  3938 | ` * The child is a plain RecursiveCachingIterator even when the receiver is a` |
|     - |  3939 | ` * SUBCLASS: php names the class entry here rather than reading ZEND_THIS's, which` |
|     - |  3940 | ` * is the opposite of what the recursive FILTERS do.` |
|     - |  3941 | ` */` |
|   142 |  3942 | `static sxi32 CitBuildChildren(ph7_context *pCtx,ph7_class_instance *pThis)` |
|     1 |  3943 | `{` |
|   143 |  3944 | `	ph7_vm *pVm = pCtx->pVm;` |
|   143 |  3945 | `	ph7_class_instance *pIn = PH7_NativeAttrObj(pThis,IT_IN);` |
|     - |  3946 | `	ph7_class_instance *pChild;` |
|     - |  3947 | `	ph7_class *pCls;` |
|     - |  3948 | `	ph7_class_method *pMethod;` |
|     - |  3949 | `	ph7_value sRes,sFlags,*apCtor[2];` |
|   143 |  3950 | `	int bCatch = (CitFlags(pThis) & CIT_CATCH_GET_CHILD) != 0;` |
|   143 |  3951 | `	int bThrew = FALSE;` |
|     - |  3952 | `	sxi32 rc;` |
|   143 |  3953 | `	PH7_NativeSetAttrObj(pVm,pThis,CIT_KIDS,0);` |
|   143 |  3954 | `	pMethod = pIn ? PH7_ClassExtractMethod(pIn->pClass,"hasChildren",sizeof("hasChildren")-1) : 0;` |
|   143 |  3955 | `	if( pMethod == 0 ){` |
|   ! 0 |  3956 | `		return SXRET_OK;` |
|     - |  3957 | `	}` |
|   143 |  3958 | `	PH7_MemObjInit(pVm,&sRes);` |
|   127 |  3959 | `	rc = bCatch ? PH7_VmCallMethodSwallow(pVm,pIn,pMethod,&sRes,0,0,&bThrew)` |
|    87 |  3960 | `	            : PH7_VmCallClassMethod(pVm,pIn,pMethod,&sRes,0,0);` |
|   143 |  3961 | `	if( rc != SXRET_OK \|\| bThrew ){` |
|     5 |  3962 | `		PH7_MemObjRelease(&sRes);` |
|     5 |  3963 | `		return rc;` |
|     - |  3964 | `	}` |
|   139 |  3965 | `	PH7_MemObjToBool(&sRes);              /* a STATUS, not the answer */` |
|   139 |  3966 | `	if( sRes.x.iVal == 0 ){` |
|    89 |  3967 | `		PH7_MemObjRelease(&sRes);` |
|    89 |  3968 | `		return SXRET_OK;` |
|     - |  3969 | `	}` |
|    51 |  3970 | `	PH7_MemObjRelease(&sRes);` |
|    51 |  3971 | `	pMethod = PH7_ClassExtractMethod(pIn->pClass,"getChildren",sizeof("getChildren")-1);` |
|    51 |  3972 | `	if( pMethod == 0 ){` |
|   ! 0 |  3973 | `		return SXRET_OK;` |
|     - |  3974 | `	}` |
|    51 |  3975 | `	PH7_MemObjInit(pVm,&sRes);` |
|    45 |  3976 | `	rc = bCatch ? PH7_VmCallMethodSwallow(pVm,pIn,pMethod,&sRes,0,0,&bThrew)` |
|    31 |  3977 | `	            : PH7_VmCallClassMethod(pVm,pIn,pMethod,&sRes,0,0);` |
|    51 |  3978 | `	if( rc != SXRET_OK \|\| bThrew ){` |
|   ! 0 |  3979 | `		PH7_MemObjRelease(&sRes);` |
|   ! 0 |  3980 | `		return rc;` |
|     - |  3981 | `	}` |
|    51 |  3982 | `	pCls = PH7_VmExtractClass(pVm,"RecursiveCachingIterator",` |
|     - |  3983 | `		sizeof("RecursiveCachingIterator")-1,FALSE,0);` |
|    51 |  3984 | `	pMethod = pCls ? PH7_ClassExtractMethod(pCls,"__construct",sizeof("__construct")-1) : 0;` |
|    51 |  3985 | `	if( pMethod == 0 ){` |
|   ! 0 |  3986 | `		PH7_MemObjRelease(&sRes);` |
|   ! 0 |  3987 | `		return SXRET_OK;` |
|     - |  3988 | `	}` |
|    51 |  3989 | `	pChild = PH7_NewClassInstance(pVm,pCls);` |
|    51 |  3990 | `	if( pChild == 0 ){` |
|   ! 0 |  3991 | `		PH7_MemObjRelease(&sRes);` |
|   ! 0 |  3992 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  3993 | `	}` |
|    51 |  3994 | `	pChild->iRef++;` |
|    51 |  3995 | `	PH7_MemObjInitFromInt(pVm,&sFlags,CitFlags(pThis) & CIT_PUBLIC);` |
|    51 |  3996 | `	apCtor[0] = &sRes;` |
|    51 |  3997 | `	apCtor[1] = &sFlags;` |
|    45 |  3998 | `	rc = bCatch ? PH7_VmCallMethodSwallow(pVm,pChild,pMethod,0,2,apCtor,&bThrew)` |
|    31 |  3999 | `	            : PH7_VmCallClassMethod(pVm,pChild,pMethod,0,2,apCtor);` |
|    51 |  4000 | `	PH7_MemObjRelease(&sRes);` |
|    51 |  4001 | `	PH7_MemObjRelease(&sFlags);` |
|    51 |  4002 | `	if( rc == SXRET_OK && !bThrew ){` |
|    47 |  4003 | `		PH7_NativeSetAttrObj(pVm,pThis,CIT_KIDS,pChild);` |
|    23 |  4004 | `	}` |
|    51 |  4005 | `	PH7_ClassInstanceUnref(pChild);` |
|    51 |  4006 | `	return rc;` |
|    72 |  4007 | `}` |
|     - |  4008 | `/*` |
|     - |  4009 | `` * php's `intern->dit_type == DIT_RecursiveCachingIterator`. rewind() and next()`` |
|     - |  4010 | ` * are declared on CachingIterator ALONE and inherited by the twin, so the one body` |
|     - |  4011 | ` * they share has to ask what it is standing on.` |
|     - |  4012 | ` */` |
|   272 |  4013 | `static int CitIsRecursive(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     3 |  4014 | `{` |
|   275 |  4015 | `	ph7_class *pCls = PH7_VmExtractClass(pVm,"RecursiveCachingIterator",` |
|     - |  4016 | `		sizeof("RecursiveCachingIterator")-1,FALSE,0);` |
|   275 |  4017 | `	return pThis && pCls && PH7_VmInstanceOf(pThis->pClass,pCls);` |
|     3 |  4018 | `}` |
|     - |  4019 | `/*` |
|     - |  4020 | ` * php's spl_caching_it_next: fetch, record, and step the inner iterator on. The` |
|     - |  4021 | ` * ORDER below is php's and is observable — a loud inner iterator sees` |
|     - |  4022 | ` * valid/current/key, then hasChildren/getChildren, then the __toString cast, then` |
|     - |  4023 | ` * next.` |
|     - |  4024 | ` */` |
|   272 |  4025 | `static sxi32 CitFetch(ph7_context *pCtx)` |
|     3 |  4026 | `{` |
|   275 |  4027 | `	ph7_vm *pVm = pCtx->pVm;` |
|   275 |  4028 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   275 |  4029 | `	int bRecursive = CitIsRecursive(pVm,pThis);` |
|     - |  4030 | `	sxi64 iFlags;` |
|     - |  4031 | `	sxi32 rc,rcStr;` |
|     - |  4032 | `	/* php's spl_dual_it_free for this type drops the string and the children with` |
|     - |  4033 | ``	 * the cached pair, which is what makes `(string)$it` empty and hasChildren()`` |
|     - |  4034 | `	 * false once the walk has run off the end. */` |
|     - |  4035 | `	{` |
|   275 |  4036 | `		ph7_value *pStr = PH7_NativeAttr(pThis,CIT_STR);` |
|   275 |  4037 | `		if( pStr ){` |
|   275 |  4038 | `			PH7_MemObjRelease(pStr);` |
|   136 |  4039 | `		}` |
|     - |  4040 | `	}` |
|   275 |  4041 | `	PH7_NativeSetAttrObj(pVm,pThis,CIT_KIDS,0);` |
|   275 |  4042 | `	rc = DualFetch(pVm,pThis,TRUE);` |
|   275 |  4043 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  4044 | `		return rc;` |
|     - |  4045 | `	}` |
|   275 |  4046 | `	iFlags = CitFlags(pThis);` |
|   275 |  4047 | `	if( !DualFilled(pThis) ){` |
|    81 |  4048 | `		PH7_NativeSetAttrInt(pVm,pThis,CIT_FL,iFlags & ~(sxi64)CIT_VALID);` |
|    81 |  4049 | `		return SXRET_OK;` |
|     - |  4050 | `	}` |
|   197 |  4051 | `	PH7_NativeSetAttrInt(pVm,pThis,CIT_FL,iFlags \| CIT_VALID);` |
|   197 |  4052 | `	if( iFlags & CIT_FULL_CACHE ){` |
|    35 |  4053 | `		ph7_value *pKey = PH7_NativeAttr(pThis,IT_CK);` |
|    35 |  4054 | `		ph7_value *pCur = PH7_NativeAttr(pThis,IT_CD);` |
|    35 |  4055 | `		if( pKey && pCur ){` |
|     - |  4056 | `			/* php's array_set_zval_key: the WHOLE array-key rule set, which is the` |
|     - |  4057 | `			 * engine's own subscript rule set (PH7_VmArrayKeyArg) — an object or an` |
|     - |  4058 | `			 * array key refused, a RESOURCE key warned about and cached under its` |
|     - |  4059 | `			 * integer id (the string cast had been caching it under the literal` |
|     - |  4060 | `			 * "Resource id #N"), a NULL key deprecated and cached under "".` |
|     - |  4061 | `			 * The pair is copied out of the instance first: the rail rewrites a` |
|     - |  4062 | `			 * resource key in place, which must not touch the iterator's own` |
|     - |  4063 | `			 * key() slot, and its diagnostics can reach a user error handler that` |
|     - |  4064 | `			 * no borrowed ph7_value* survives — the cache slot included, which is` |
|     - |  4065 | `			 * why it is fetched only once the screen is through. */` |
|     - |  4066 | `			ph7_value sKey,sVal,*pCache;` |
|    35 |  4067 | `			PH7_MemObjInit(pVm,&sKey);` |
|    35 |  4068 | `			PH7_MemObjInit(pVm,&sVal);` |
|    35 |  4069 | `			PH7_MemObjStore(pKey,&sKey);` |
|    35 |  4070 | `			PH7_MemObjStore(pCur,&sVal);` |
|    35 |  4071 | `			rc = PH7_VmArrayKeyArg(pCtx,&sKey,PH7_ARRAYKEY_OFFSET);` |
|    35 |  4072 | `			if( rc != SXRET_OK ){` |
|     8 |  4073 | `				PH7_MemObjRelease(&sKey);` |
|     8 |  4074 | `				PH7_MemObjRelease(&sVal);` |
|     8 |  4075 | `				return rc;` |
|     - |  4076 | `			}` |
|    29 |  4077 | `			pCache = CitCacheSlot(pVm,pThis);` |
|    29 |  4078 | `			if( pCache ){` |
|    29 |  4079 | `				ph7_array_add_elem(pCache,&sKey,&sVal);` |
|    13 |  4080 | `			}` |
|    29 |  4081 | `			PH7_MemObjRelease(&sKey);` |
|    29 |  4082 | `			PH7_MemObjRelease(&sVal);` |
|    13 |  4083 | `		}` |
|    13 |  4084 | `	}` |
|   191 |  4085 | `	if( bRecursive ){` |
|     - |  4086 | `		/* php checks EG(exception) here and RETURNS, so a throw from the children` |
|     - |  4087 | `		 * half leaves the inner iterator where it stands. */` |
|   143 |  4088 | `		rc = CitBuildChildren(pCtx,pThis);` |
|   143 |  4089 | `		if( rc != SXRET_OK ){` |
|     5 |  4090 | `			return rc;` |
|     - |  4091 | `		}` |
|    69 |  4092 | `	}` |
|   187 |  4093 | `	rcStr = SXRET_OK;` |
|   187 |  4094 | `	if( iFlags & (CIT_CALL_TOSTRING\|CIT_TOSTRING_USE_INN) ){` |
|    17 |  4095 | `		rcStr = CitMakeString(pCtx,pThis,iFlags);` |
|     8 |  4096 | `	}` |
|     - |  4097 | `	/* php makes no such check after the CAST, so an element with no __toString` |
|     - |  4098 | `	 * throws AND leaves the inner iterator one step on: hasNext() answers from` |
|     - |  4099 | `	 * where the walk really is, not from where the throw interrupted it. */` |
|   187 |  4100 | `	rc = DualNextInnerEx(pVm,pThis,FALSE);` |
|   187 |  4101 | `	return rcStr != SXRET_OK ? rcStr : rc;` |
|   139 |  4102 | `}` |
|   188 |  4103 | `static int CitConstruct(ph7_context *pCtx,const char *zOwner,int nArg,ph7_value **apArg)` |
|     3 |  4104 | `{` |
|   191 |  4105 | `	ph7_vm *pVm = pCtx->pVm;` |
|   191 |  4106 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   191 |  4107 | `	sxi64 iFlags = CIT_CALL_TOSTRING;` |
|     - |  4108 | `	sxi32 rc;` |
|   191 |  4109 | `	if( pThis == 0 ){` |
|   ! 0 |  4110 | `		return PH7_OK;` |
|     - |  4111 | `	}` |
|   191 |  4112 | `	if( nArg > 1 ){` |
|   153 |  4113 | `		iFlags = ph7_value_to_int64(apArg[1]);` |
|    75 |  4114 | `	}` |
|   191 |  4115 | `	if( !CitCheckFlags(iFlags) ){` |
|     4 |  4116 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  4117 | `			"%s::__construct(): Argument #2 ($flags) must contain only one of "` |
|     - |  4118 | `			"CachingIterator::CALL_TOSTRING, CachingIterator::TOSTRING_USE_KEY, "` |
|     - |  4119 | `			"CachingIterator::TOSTRING_USE_CURRENT, or CachingIterator::TOSTRING_USE_INNER",` |
|     1 |  4120 | `			zOwner);` |
|     - |  4121 | `	}` |
|     - |  4122 | `	/* Only the ITERATOR reaches the shared constructor: its second argument is` |
|     - |  4123 | ``	 * IteratorIterator's `$class` downcast, and this one's is an int. */`` |
|   189 |  4124 | `	rc = DualConstruct(pCtx,zOwner,nArg > 0 ? 1 : 0,apArg);` |
|   189 |  4125 | `	if( rc != PH7_OK ){` |
|   ! 0 |  4126 | `		return rc;` |
|     - |  4127 | `	}` |
|   189 |  4128 | `	PH7_NativeSetAttrInt(pVm,pThis,CIT_FL,iFlags & CIT_PUBLIC);` |
|   189 |  4129 | `	return PH7_OK;` |
|    97 |  4130 | `}` |
|    80 |  4131 | `static int vm_builtin_CachingIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  4132 | `{` |
|    83 |  4133 | `	return CitConstruct(pCtx,"CachingIterator",nArg,apArg);` |
|     3 |  4134 | `}` |
|     - |  4135 | `/*` |
|     - |  4136 | `` * php's stub DECLARES `Iterator $iterator` here and its body then asks for`` |
|     - |  4137 | ` * spl_ce_RecursiveIterator, so Reflection reports the looser type while the` |
|     - |  4138 | ` * refusal names the tighter one. Both halves are reproduced: the signature above` |
|     - |  4139 | ` * is the stub's, this check is the body's.` |
|     - |  4140 | ` */` |
|   118 |  4141 | `static int vm_builtin_RecursiveCachingIterator_construct(ph7_context *pCtx,int nArg,` |
|     - |  4142 | `	ph7_value **apArg)` |
|     1 |  4143 | `{` |
|   119 |  4144 | `	if( nArg > 0 ){` |
|   119 |  4145 | `		ph7_class *pRec = PH7_VmExtractClass(pCtx->pVm,"RecursiveIterator",` |
|     - |  4146 | `			sizeof("RecursiveIterator")-1,FALSE,0);` |
|   178 |  4147 | `		ph7_class_instance *pObj = (apArg[0]->iFlags & MEMOBJ_OBJ)` |
|   116 |  4148 | `			? (ph7_class_instance *)apArg[0]->x.pOther : 0;` |
|   119 |  4149 | `		if( pRec && (pObj == 0 \|\| !PH7_VmInstanceOf(pObj->pClass,pRec)) ){` |
|     - |  4150 | `			char zBuf[64];` |
|    16 |  4151 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  4152 | `				"RecursiveCachingIterator::__construct(): Argument #1 ($iterator) must be "` |
|     - |  4153 | `				"of type RecursiveIterator, %s given",` |
|     5 |  4154 | `				VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));` |
|     - |  4155 | `		}` |
|    54 |  4156 | `	}` |
|   109 |  4157 | `	return CitConstruct(pCtx,"RecursiveCachingIterator",nArg,apArg);` |
|    60 |  4158 | `}` |
|   128 |  4159 | `static int vm_builtin_CachingIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  4160 | `{` |
|   131 |  4161 | `	ph7_vm *pVm = pCtx->pVm;` |
|   131 |  4162 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  4163 | `	ph7_value *pCache;` |
|     - |  4164 | `	sxi32 rc;` |
|    64 |  4165 | `	SXUNUSED(nArg);` |
|    64 |  4166 | `	SXUNUSED(apArg);` |
|   131 |  4167 | `	if( !DualReady(pThis) ){` |
|     3 |  4168 | `		return DualNotReady(pCtx);` |
|     - |  4169 | `	}` |
|   129 |  4170 | `	pCache = PH7_NativeAttr(pThis,CIT_CCH);` |
|   129 |  4171 | `	if( pCache ){` |
|     - |  4172 | `		/* php's zend_hash_clean: a rewind starts the cache over. */` |
|   129 |  4173 | `		PH7_MemObjRelease(pCache);` |
|   129 |  4174 | `		PH7_MemObjToHashmap(pCache);` |
|    63 |  4175 | `	}` |
|   129 |  4176 | `	rc = DualRewindInner(pVm,pThis);` |
|   129 |  4177 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  4178 | `		return rc;` |
|     - |  4179 | `	}` |
|   129 |  4180 | `	rc = CitFetch(pCtx);` |
|   129 |  4181 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    67 |  4182 | `}` |
|   148 |  4183 | `static int vm_builtin_CachingIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  4184 | `{` |
|   151 |  4185 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  4186 | `	sxi32 rc;` |
|    74 |  4187 | `	SXUNUSED(nArg);` |
|    74 |  4188 | `	SXUNUSED(apArg);` |
|   151 |  4189 | `	if( !DualReady(pThis) ){` |
|     3 |  4190 | `		return DualNotReady(pCtx);` |
|     - |  4191 | `	}` |
|   149 |  4192 | `	rc = CitFetch(pCtx);` |
|   149 |  4193 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    77 |  4194 | `}` |
|     - |  4195 | `/* valid() is the private CIT_VALID bit, not the inner iterator's answer: the` |
|     - |  4196 | ` * decorator stands on what it fetched and the inner has already moved past it. */` |
|   352 |  4197 | `static int vm_builtin_CachingIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  4198 | `{` |
|   355 |  4199 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   176 |  4200 | `	SXUNUSED(nArg);` |
|   176 |  4201 | `	SXUNUSED(apArg);` |
|   355 |  4202 | `	if( !DualReady(pThis) ){` |
|     3 |  4203 | `		return DualNotReady(pCtx);` |
|     - |  4204 | `	}` |
|   353 |  4205 | `	ph7_result_bool(pCtx,(CitFlags(pThis) & CIT_VALID) != 0);` |
|   353 |  4206 | `	return PH7_OK;` |
|   179 |  4207 | `}` |
|     - |  4208 | `/* hasNext() is the inner iterator's LIVE valid(), which is why moving the inner` |
|     - |  4209 | ` * behind the decorator's back changes the answer. */` |
|   194 |  4210 | `static int vm_builtin_CachingIterator_hasNext(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  4211 | `{` |
|   195 |  4212 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   195 |  4213 | `	int bValid = 0;` |
|     - |  4214 | `	sxi32 rc;` |
|    97 |  4215 | `	SXUNUSED(nArg);` |
|    97 |  4216 | `	SXUNUSED(apArg);` |
|   195 |  4217 | `	if( !DualReady(pThis) ){` |
|     3 |  4218 | `		return DualNotReady(pCtx);` |
|     - |  4219 | `	}` |
|   193 |  4220 | `	rc = DualInnerValid(pCtx->pVm,pThis,&bValid);` |
|   193 |  4221 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  4222 | `		return rc;` |
|     - |  4223 | `	}` |
|   193 |  4224 | `	ph7_result_bool(pCtx,bValid);` |
|   193 |  4225 | `	return PH7_OK;` |
|    98 |  4226 | `}` |
|    22 |  4227 | `static int vm_builtin_CachingIterator_toString(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  4228 | `{` |
|    23 |  4229 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  4230 | `	sxi64 iFlags;` |
|     - |  4231 | `	ph7_value sOut,*pSrc;` |
|     - |  4232 | `	sxi32 rc;` |
|    11 |  4233 | `	SXUNUSED(nArg);` |
|    11 |  4234 | `	SXUNUSED(apArg);` |
|    23 |  4235 | `	if( !DualReady(pThis) ){` |
|     3 |  4236 | `		return DualNotReady(pCtx);` |
|     - |  4237 | `	}` |
|    21 |  4238 | `	iFlags = CitFlags(pThis);` |
|    20 |  4239 | `	if( (iFlags & (CIT_CALL_TOSTRING\|CIT_TOSTRING_USE_KEY\|CIT_TOSTRING_USE_CUR` |
|    11 |  4240 | `		\|CIT_TOSTRING_USE_INN)) == 0 ){` |
|     5 |  4241 | `		return CitRefuse(pCtx,pThis,"fetch string value");` |
|     - |  4242 | `	}` |
|    17 |  4243 | `	if( iFlags & (CIT_TOSTRING_USE_KEY\|CIT_TOSTRING_USE_CUR) ){` |
|     - |  4244 | `		/* Read out of the CACHE at call time, converted then and there. */` |
|     5 |  4245 | `		pSrc = PH7_NativeAttr(pThis,(iFlags & CIT_TOSTRING_USE_KEY) ? IT_CK : IT_CD);` |
|     5 |  4246 | `		PH7_MemObjInit(pCtx->pVm,&sOut);` |
|     5 |  4247 | `		if( pSrc ){` |
|     5 |  4248 | `			PH7_MemObjStore(pSrc,&sOut);` |
|     2 |  4249 | `		}` |
|     5 |  4250 | `		rc = PH7_MemObjToStringUV(&sOut);` |
|     5 |  4251 | `		if( rc == SXRET_OK ){` |
|     5 |  4252 | `			ph7_result_value(pCtx,&sOut);` |
|     2 |  4253 | `		}` |
|     5 |  4254 | `		PH7_MemObjRelease(&sOut);` |
|     5 |  4255 | `		return rc == SXRET_OK ? PH7_OK : rc;` |
|     - |  4256 | `	}` |
|    13 |  4257 | `	pSrc = PH7_NativeAttr(pThis,CIT_STR);` |
|    13 |  4258 | `	if( pSrc && (pSrc->iFlags & MEMOBJ_STRING) ){` |
|    11 |  4259 | `		ph7_result_value(pCtx,pSrc);` |
|     6 |  4260 | `	}else{` |
|     - |  4261 | ``		/* php's `zstr is not a string` — nothing has been fetched. */`` |
|     3 |  4262 | `		ph7_result_string(pCtx,"",0);` |
|     - |  4263 | `	}` |
|    13 |  4264 | `	return PH7_OK;` |
|    12 |  4265 | `}` |
|    32 |  4266 | `static int vm_builtin_CachingIterator_getFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  4267 | `{` |
|    33 |  4268 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    16 |  4269 | `	SXUNUSED(nArg);` |
|    16 |  4270 | `	SXUNUSED(apArg);` |
|    33 |  4271 | `	if( !DualReady(pThis) ){` |
|     3 |  4272 | `		return DualNotReady(pCtx);` |
|     - |  4273 | `	}` |
|    31 |  4274 | `	ph7_result_int64(pCtx,CitFlags(pThis));` |
|    31 |  4275 | `	return PH7_OK;` |
|    17 |  4276 | `}` |
|    10 |  4277 | `static int vm_builtin_CachingIterator_setFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  4278 | `{` |
|    11 |  4279 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  4280 | `	sxi64 iOld,iNew;` |
|    11 |  4281 | `	if( nArg < 1 ){` |
|   ! 0 |  4282 | `		return PH7_OK;` |
|     - |  4283 | `	}` |
|    11 |  4284 | `	iNew = ph7_value_to_int64(apArg[0]);` |
|    11 |  4285 | `	if( !CitCheckFlags(iNew) ){` |
|     3 |  4286 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  4287 | `			"CachingIterator::setFlags(): Argument #1 ($flags) must contain only one of "` |
|     - |  4288 | `			"CachingIterator::CALL_TOSTRING, CachingIterator::TOSTRING_USE_KEY, "` |
|     - |  4289 | `			"CachingIterator::TOSTRING_USE_CURRENT, or CachingIterator::TOSTRING_USE_INNER");` |
|     - |  4290 | `	}` |
|     9 |  4291 | `	if( !DualReady(pThis) ){` |
|   ! 0 |  4292 | `		return DualNotReady(pCtx);` |
|     - |  4293 | `	}` |
|     - |  4294 | `	/* The two eager spellings are computed at FETCH time, so php refuses to turn` |
|     - |  4295 | `	 * either off mid-walk rather than leaving a stale string behind. */` |
|     9 |  4296 | `	iOld = CitFlags(pThis);` |
|     9 |  4297 | `	if( (iOld & CIT_CALL_TOSTRING) != 0 && (iNew & CIT_CALL_TOSTRING) == 0 ){` |
|     5 |  4298 | `		return PH7_VmThrowException(pCtx,"InvalidArgumentException",` |
|     - |  4299 | `			"Unsetting flag CALL_TO_STRING is not possible");` |
|     - |  4300 | `	}` |
|     5 |  4301 | `	if( (iOld & CIT_TOSTRING_USE_INN) != 0 && (iNew & CIT_TOSTRING_USE_INN) == 0 ){` |
|     3 |  4302 | `		return PH7_VmThrowException(pCtx,"InvalidArgumentException",` |
|     - |  4303 | `			"Unsetting flag TOSTRING_USE_INNER is not possible");` |
|     - |  4304 | `	}` |
|     3 |  4305 | `	PH7_NativeSetAttrInt(pCtx->pVm,pThis,CIT_FL,(iOld & ~(sxi64)CIT_PUBLIC) \| (iNew & CIT_PUBLIC));` |
|     3 |  4306 | `	return PH7_OK;` |
|     6 |  4307 | `}` |
|    20 |  4308 | `static int vm_builtin_CachingIterator_getCache(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  4309 | `{` |
|     - |  4310 | `	ph7_value *pCache;` |
|     - |  4311 | `	int rc;` |
|    10 |  4312 | `	SXUNUSED(nArg);` |
|    10 |  4313 | `	SXUNUSED(apArg);` |
|    23 |  4314 | `	pCache = CitCacheChecked(pCtx,&rc);` |
|    23 |  4315 | `	if( pCache == 0 ){` |
|     5 |  4316 | `		return rc;` |
|     - |  4317 | `	}` |
|    19 |  4318 | `	ph7_result_value(pCtx,pCache);` |
|    19 |  4319 | `	return PH7_OK;` |
|    13 |  4320 | `}` |
|    14 |  4321 | `static int vm_builtin_CachingIterator_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  4322 | `{` |
|     - |  4323 | `	ph7_value *pCache;` |
|     - |  4324 | `	int rc;` |
|     7 |  4325 | `	SXUNUSED(nArg);` |
|     7 |  4326 | `	SXUNUSED(apArg);` |
|    15 |  4327 | `	pCache = CitCacheChecked(pCtx,&rc);` |
|    15 |  4328 | `	if( pCache == 0 ){` |
|     5 |  4329 | `		return rc;` |
|     - |  4330 | `	}` |
|    11 |  4331 | `	ph7_result_int64(pCtx,pCache->x.pOther ? ((ph7_hashmap *)pCache->x.pOther)->nEntry : 0);` |
|    11 |  4332 | `	return PH7_OK;` |
|     8 |  4333 | `}` |
|     - |  4334 | `/*` |
|     - |  4335 | `` * The four ArrayAccess members read and write that same cache. php's `$key` is`` |
|     - |  4336 | ` * DECLARED untyped and screened as a string by the body, so a non-stringable key` |
|     - |  4337 | `` * is a TypeError naming `string` while an int or a float becomes an array key the`` |
|     - |  4338 | ` * ordinary way.` |
|     - |  4339 | ` */` |
|    32 |  4340 | `static ph7_value * CitOffsetKey(ph7_context *pCtx,const char *zMethod,ph7_value *pKey,` |
|     - |  4341 | `	ph7_value *pOut,int *pRc)` |
|     2 |  4342 | `{` |
|     - |  4343 | `	char zBuf[64];` |
|    34 |  4344 | `	*pRc = PH7_OK;` |
|    34 |  4345 | `	if( !PH7_ArgSatisfiesString(pKey) ){` |
|    20 |  4346 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  4347 | `			"CachingIterator::%s(): Argument #1 ($key) must be of type string, %s given",` |
|     6 |  4348 | `			zMethod,VmValueGivenName(pKey,zBuf,sizeof(zBuf)));` |
|    14 |  4349 | `		return 0;` |
|     - |  4350 | `	}` |
|    22 |  4351 | `	PH7_MemObjInit(pCtx->pVm,pOut);` |
|    22 |  4352 | `	PH7_MemObjStore(pKey,pOut);` |
|    22 |  4353 | `	if( PH7_MemObjToString(pOut) != SXRET_OK ){` |
|   ! 0 |  4354 | `		PH7_MemObjRelease(pOut);` |
|   ! 0 |  4355 | `		*pRc = PH7_OK;` |
|   ! 0 |  4356 | `		return 0;` |
|     - |  4357 | `	}` |
|    22 |  4358 | `	return pOut;` |
|    18 |  4359 | `}` |
|    16 |  4360 | `static int vm_builtin_CachingIterator_offsetGet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  4361 | `{` |
|     - |  4362 | `	ph7_value *pCache,*pKey,sKey;` |
|    18 |  4363 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  4364 | `	int rc;` |
|    18 |  4365 | `	pCache = CitCacheChecked(pCtx,&rc);` |
|    18 |  4366 | `	if( pCache == 0 ){` |
|     3 |  4367 | `		return rc;` |
|     - |  4368 | `	}` |
|    16 |  4369 | `	if( nArg < 1 ){` |
|   ! 0 |  4370 | `		return PH7_OK;` |
|     - |  4371 | `	}` |
|    16 |  4372 | `	pKey = CitOffsetKey(pCtx,"offsetGet",apArg[0],&sKey,&rc);` |
|    16 |  4373 | `	if( pKey == 0 ){` |
|     6 |  4374 | `		return rc;` |
|     - |  4375 | `	}` |
|    11 |  4376 | `	if( PH7_HashmapLookup((ph7_hashmap *)pCache->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|     5 |  4377 | `		ph7_value *pVal = HashmapExtractNodeValue(pNode);` |
|     5 |  4378 | `		if( pVal ){` |
|     5 |  4379 | `			ph7_result_value(pCtx,pVal);` |
|     2 |  4380 | `		}` |
|     3 |  4381 | `	}else{` |
|     - |  4382 | `		/* php reads the cache as an ARRAY here, warning included — but it has already` |
|     - |  4383 | `		 * cast the key to a STRING, so the key is QUOTED even where a plain array read` |
|     - |  4384 | ``		 * would print a bare integer ($c[0] on a missing key says `"0"`). */`` |
|     - |  4385 | `		SyBlob sMsg;` |
|     - |  4386 | `		SyString sKeyText;` |
|     7 |  4387 | `		int nKey = 0;` |
|     7 |  4388 | `		const char *zKey = ph7_value_to_string(pKey,&nKey);` |
|     7 |  4389 | `		SyStringInitFromBuf(&sKeyText,zKey,(sxu32)nKey);` |
|     7 |  4390 | `		SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);` |
|     7 |  4391 | `		SyBlobFormat(&sMsg,"Undefined array key \"%z\"",&sKeyText);` |
|     7 |  4392 | `		SyBlobNullAppend(&sMsg);` |
|     7 |  4393 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));` |
|     7 |  4394 | `		SyBlobRelease(&sMsg);` |
|     - |  4395 | `	}` |
|    11 |  4396 | `	PH7_MemObjRelease(&sKey);` |
|    11 |  4397 | `	return PH7_OK;` |
|    10 |  4398 | `}` |
|    10 |  4399 | `static int vm_builtin_CachingIterator_offsetExists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  4400 | `{` |
|     - |  4401 | `	ph7_value *pCache,*pKey,sKey;` |
|    12 |  4402 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  4403 | `	int rc;` |
|    12 |  4404 | `	pCache = CitCacheChecked(pCtx,&rc);` |
|    12 |  4405 | `	if( pCache == 0 ){` |
|     3 |  4406 | `		return rc;` |
|     - |  4407 | `	}` |
|    10 |  4408 | `	if( nArg < 1 ){` |
|   ! 0 |  4409 | `		return PH7_OK;` |
|     - |  4410 | `	}` |
|    10 |  4411 | `	pKey = CitOffsetKey(pCtx,"offsetExists",apArg[0],&sKey,&rc);` |
|    10 |  4412 | `	if( pKey == 0 ){` |
|     3 |  4413 | `		return rc;` |
|     - |  4414 | `	}` |
|    11 |  4415 | `	ph7_result_bool(pCtx,` |
|     6 |  4416 | `		PH7_HashmapLookup((ph7_hashmap *)pCache->x.pOther,pKey,&pNode) == SXRET_OK);` |
|     8 |  4417 | `	PH7_MemObjRelease(&sKey);` |
|     8 |  4418 | `	return PH7_OK;` |
|     7 |  4419 | `}` |
|     8 |  4420 | `static int vm_builtin_CachingIterator_offsetSet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  4421 | `{` |
|     - |  4422 | `	ph7_value *pCache,*pKey,sKey;` |
|     - |  4423 | `	int rc;` |
|    10 |  4424 | `	pCache = CitCacheChecked(pCtx,&rc);` |
|    10 |  4425 | `	if( pCache == 0 ){` |
|     3 |  4426 | `		return rc;` |
|     - |  4427 | `	}` |
|     8 |  4428 | `	if( nArg < 2 ){` |
|   ! 0 |  4429 | `		return PH7_OK;` |
|     - |  4430 | `	}` |
|     8 |  4431 | `	pKey = CitOffsetKey(pCtx,"offsetSet",apArg[0],&sKey,&rc);` |
|     8 |  4432 | `	if( pKey == 0 ){` |
|     5 |  4433 | `		return rc;` |
|     - |  4434 | `	}` |
|     3 |  4435 | `	ph7_array_add_elem(pCache,pKey,apArg[1]);` |
|     3 |  4436 | `	PH7_MemObjRelease(&sKey);` |
|     3 |  4437 | `	return PH7_OK;` |
|     6 |  4438 | `}` |
|     6 |  4439 | `static int vm_builtin_CachingIterator_offsetUnset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  4440 | `{` |
|     - |  4441 | `	ph7_value *pCache,*pKey,sKey;` |
|     8 |  4442 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  4443 | `	int rc;` |
|     8 |  4444 | `	pCache = CitCacheChecked(pCtx,&rc);` |
|     8 |  4445 | `	if( pCache == 0 ){` |
|     3 |  4446 | `		return rc;` |
|     - |  4447 | `	}` |
|     6 |  4448 | `	if( nArg < 1 ){` |
|   ! 0 |  4449 | `		return PH7_OK;` |
|     - |  4450 | `	}` |
|     6 |  4451 | `	pKey = CitOffsetKey(pCtx,"offsetUnset",apArg[0],&sKey,&rc);` |
|     6 |  4452 | `	if( pKey == 0 ){` |
|     3 |  4453 | `		return rc;` |
|     - |  4454 | `	}` |
|     3 |  4455 | `	if( PH7_HashmapLookup((ph7_hashmap *)pCache->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|     3 |  4456 | `		PH7_HashmapUnlinkNode(pNode,TRUE);` |
|     1 |  4457 | `	}` |
|     3 |  4458 | `	PH7_MemObjRelease(&sKey);` |
|     3 |  4459 | `	return PH7_OK;` |
|     5 |  4460 | `}` |
|     - |  4461 | `/* The recursive twin answers what the FETCH built and nothing else: no children` |
|     - |  4462 | ` * means the fetch found none, and two calls hand back the SAME object. */` |
|   130 |  4463 | `static int vm_builtin_RecursiveCachingIterator_hasChildren(ph7_context *pCtx,int nArg,` |
|     - |  4464 | `	ph7_value **apArg)` |
|     1 |  4465 | `{` |
|   131 |  4466 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    65 |  4467 | `	SXUNUSED(nArg);` |
|    65 |  4468 | `	SXUNUSED(apArg);` |
|   131 |  4469 | `	if( !DualReady(pThis) ){` |
|     3 |  4470 | `		return DualNotReady(pCtx);` |
|     - |  4471 | `	}` |
|   129 |  4472 | `	ph7_result_bool(pCtx,PH7_NativeAttrObj(pThis,CIT_KIDS) != 0);` |
|   129 |  4473 | `	return PH7_OK;` |
|    66 |  4474 | `}` |
|    48 |  4475 | `static int vm_builtin_RecursiveCachingIterator_getChildren(ph7_context *pCtx,int nArg,` |
|     - |  4476 | `	ph7_value **apArg)` |
|     1 |  4477 | `{` |
|    49 |  4478 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  4479 | `	ph7_class_instance *pKids;` |
|    24 |  4480 | `	SXUNUSED(nArg);` |
|    24 |  4481 | `	SXUNUSED(apArg);` |
|    49 |  4482 | `	if( !DualReady(pThis) ){` |
|   ! 0 |  4483 | `		return DualNotReady(pCtx);` |
|     - |  4484 | `	}` |
|    49 |  4485 | `	pKids = PH7_NativeAttrObj(pThis,CIT_KIDS);` |
|    49 |  4486 | `	if( pKids ){` |
|    45 |  4487 | `		SplResultBorrowed(pCtx,pKids);` |
|    23 |  4488 | `	}else{` |
|     5 |  4489 | `		ph7_result_null(pCtx);` |
|     - |  4490 | `	}` |
|    49 |  4491 | `	return PH7_OK;` |
|    25 |  4492 | `}` |
|     - |  4493 | `/*` |
|     - |  4494 | ` * The declarations. php's method ORDER is the order Reflection reports, so each` |
|     - |  4495 | ` * table follows spl_iterators.stub.php line for line; the parameter types are the` |
|     - |  4496 | `` * stub's too, which is what makes `Iterator $iterator` refuse an IteratorAggregate`` |
|     - |  4497 | ` * everywhere except IteratorIterator (the one class that declares Traversable and` |
|     - |  4498 | ` * unwraps).` |
|     - |  4499 | ` *` |
|     - |  4500 | ` * No RETURN type is declared, on purpose: php marks every one of these` |
|     - |  4501 | `` * `@tentative-return-type`, and a tentative type answers NULL from getReturnType()`` |
|     - |  4502 | ` * and false from hasReturnType() — which is exactly what an undeclared zRet answers` |
|     - |  4503 | `` * here. Declaring them would print `Return [ bool ]` where php prints`` |
|     - |  4504 | `` * `Tentative return [ bool ]` AND make getReturnType() disagree; leaving them off`` |
|     - |  4505 | ` * costs only getTentativeReturnType(). PHL has no tentative-return concept at all` |
|     - |  4506 | ` * (§7.4) — DateTime and the reflectors already report a plain return type where php` |
|     - |  4507 | ` * reports a tentative one.` |
|     - |  4508 | ` */` |
|  6721 |  4509 | `static sxi32 VmInstallSplDualIterators(ph7_vm *pVm)` |
|     5 |  4510 | `{` |
|     - |  4511 | `	static const PH7_NativePropDef aDualProp[] = {` |
|     - |  4512 | `		{ IT_IN, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  4513 | `		{ IT_IT, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  4514 | `		{ IT_CD, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  4515 | `		{ IT_CK, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  4516 | `		{ IT_CF, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  4517 | `		{ IT_CP, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  4518 | `	};` |
|     - |  4519 | `	static const PH7_NativePropDef aLimitProp[] = {` |
|     - |  4520 | `		{ IT_OFF, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  4521 | `		{ IT_LIM, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, -1, 0, 0.0 }, 0 },` |
|     - |  4522 | `	};` |
|     - |  4523 | `	static const PH7_NativePropDef aCbProp[] = {` |
|     - |  4524 | `		{ IT_CB, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  4525 | `	};` |
|     - |  4526 | `	/* php types the SPL contracts as it types the core interfaces: a TENTATIVE` |
|     - |  4527 | `	 * return on every method, so an iterator written before php 8.1 still` |
|     - |  4528 | `	 * satisfies them. Both of the ones declared here, and SeekableIterator's` |
|     - |  4529 | ``	 * `seek` above, had no return type at all. */`` |
|     - |  4530 | `	static const PH7_NativeMethodDef aOuterMethod[] = {` |
|     - |  4531 | `		{ "getInnerIterator", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@?Iterator", 0 },` |
|     - |  4532 | `	};` |
|     - |  4533 | `	static const PH7_NativeMethodDef aIterIterMethod[] = {` |
|     - |  4534 | `		{ "__construct",      PH7_MOD_PUBLIC, "Traversable $iterator, ?string $class = null", 0,` |
|     - |  4535 | `		  vm_builtin_IteratorIterator_construct },` |
|     - |  4536 | `		{ "getInnerIterator", PH7_MOD_PUBLIC, "", "@?Iterator", vm_builtin_Dual_getInnerIterator },` |
|     - |  4537 | `		{ "rewind",           PH7_MOD_PUBLIC, "", "@void", vm_builtin_IteratorIterator_rewind },` |
|     - |  4538 | `		{ "valid",            PH7_MOD_PUBLIC, "", "@bool", vm_builtin_Dual_valid },` |
|     - |  4539 | `		{ "key",              PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_Dual_key },` |
|     - |  4540 | `		{ "current",          PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_Dual_current },` |
|     - |  4541 | `		{ "next",             PH7_MOD_PUBLIC, "", "@void", vm_builtin_IteratorIterator_next },` |
|     - |  4542 | `	};` |
|     - |  4543 | `	static const PH7_NativeMethodDef aFilterMethod[] = {` |
|     - |  4544 | `		{ "accept",      PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@bool", 0 },` |
|     - |  4545 | `		{ "__construct", PH7_MOD_PUBLIC, "Iterator $iterator", 0, vm_builtin_FilterIterator_construct },` |
|     - |  4546 | `		{ "rewind",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_FilterIterator_rewind },` |
|     - |  4547 | `		{ "next",        PH7_MOD_PUBLIC, "", "@void", vm_builtin_FilterIterator_next },` |
|     - |  4548 | `	};` |
|     - |  4549 | `	static const PH7_NativeMethodDef aCbFilterMethod[] = {` |
|     - |  4550 | `		{ "__construct", PH7_MOD_PUBLIC, "Iterator $iterator, callable $callback", 0,` |
|     - |  4551 | `		  vm_builtin_CallbackFilterIterator_construct },` |
|     - |  4552 | `		{ "accept",      PH7_MOD_PUBLIC, "", "@bool", vm_builtin_CallbackFilterIterator_accept },` |
|     - |  4553 | `	};` |
|     - |  4554 | `	static const PH7_NativeMethodDef aLimitMethod[] = {` |
|     - |  4555 | `		{ "__construct", PH7_MOD_PUBLIC, "Iterator $iterator, int $offset = 0, int $limit = -1", 0,` |
|     - |  4556 | `		  vm_builtin_LimitIterator_construct },` |
|     - |  4557 | `		{ "rewind",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_LimitIterator_rewind },` |
|     - |  4558 | `		{ "valid",       PH7_MOD_PUBLIC, "", "@bool", vm_builtin_LimitIterator_valid },` |
|     - |  4559 | `		{ "next",        PH7_MOD_PUBLIC, "", "@void", vm_builtin_LimitIterator_next },` |
|     - |  4560 | `		{ "seek",        PH7_MOD_PUBLIC, "int $offset", "@int", vm_builtin_LimitIterator_seek },` |
|     - |  4561 | `		{ "getPosition", PH7_MOD_PUBLIC, "", "@int", vm_builtin_LimitIterator_getPosition },` |
|     - |  4562 | `	};` |
|     - |  4563 | `	static const PH7_NativeMethodDef aInfiniteMethod[] = {` |
|     - |  4564 | `		{ "__construct", PH7_MOD_PUBLIC, "Iterator $iterator", 0,` |
|     - |  4565 | `		  vm_builtin_InfiniteIterator_construct },` |
|     - |  4566 | `		{ "next",        PH7_MOD_PUBLIC, "", "@void", vm_builtin_InfiniteIterator_next },` |
|     - |  4567 | `	};` |
|     - |  4568 | `	static const PH7_NativeMethodDef aNoRewindMethod[] = {` |
|     - |  4569 | `		{ "__construct", PH7_MOD_PUBLIC, "Iterator $iterator", 0,` |
|     - |  4570 | `		  vm_builtin_NoRewindIterator_construct },` |
|     - |  4571 | `		{ "rewind",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_NoRewindIterator_rewind },` |
|     - |  4572 | `		{ "valid",       PH7_MOD_PUBLIC, "", "@bool", vm_builtin_NoRewindIterator_valid },` |
|     - |  4573 | `		{ "key",         PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_NoRewindIterator_key },` |
|     - |  4574 | `		{ "current",     PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_NoRewindIterator_current },` |
|     - |  4575 | `		{ "next",        PH7_MOD_PUBLIC, "", "@void", vm_builtin_NoRewindIterator_next },` |
|     - |  4576 | `	};` |
|     - |  4577 | `	static const PH7_NativePropDef aRegexProp[] = {` |
|     - |  4578 | `		/* The one slot php PRESENTS, declared as php declares it: a ?string, so a` |
|     - |  4579 | ``		 * `$it->replacement = 5` coerces and an array is a TypeError. */`` |
|     - |  4580 | `		{ "replacement", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?string" },` |
|     - |  4581 | `		{ IT_RE, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|     - |  4582 | `		{ IT_RM, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  4583 | `		{ IT_RF, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  4584 | `		{ IT_RP, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  4585 | `	};` |
|     - |  4586 | `	static const PH7_NativeConstDef aRegexConst[] = {` |
|     - |  4587 | `		{ "USE_KEY",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, REGIT_USE_KEY,  0, 0.0 },` |
|     - |  4588 | `		{ "INVERT_MATCH", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, REGIT_INVERTED, 0, 0.0 },` |
|     - |  4589 | `		{ "MATCH",        PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PH7_REGIT_MATCH,       0, 0.0 },` |
|     - |  4590 | `		{ "GET_MATCH",    PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PH7_REGIT_GET_MATCH,   0, 0.0 },` |
|     - |  4591 | `		{ "ALL_MATCHES",  PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PH7_REGIT_ALL_MATCHES, 0, 0.0 },` |
|     - |  4592 | `		{ "SPLIT",        PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PH7_REGIT_SPLIT,       0, 0.0 },` |
|     - |  4593 | `		{ "REPLACE",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PH7_REGIT_REPLACE,     0, 0.0 },` |
|     - |  4594 | `	};` |
|     - |  4595 | `	static const PH7_NativeMethodDef aRegexMethod[] = {` |
|     - |  4596 | `		{ "__construct",  PH7_MOD_PUBLIC,` |
|     - |  4597 | ``		  /* php's stub spells this default `RegexIterator::MATCH`, and one zSig field`` |
|     - |  4598 | `		   * cannot say both the TEXT and the VALUE: the constant spelling prints php's` |
|     - |  4599 | `		   * export line but makes getDefaultValue() a "Failed to retrieve" throw, so the` |
|     - |  4600 | `		   * VALUE wins here, as it does in the aBuiltinSig rows with the same shape. */` |
|     - |  4601 | `		  "Iterator $iterator, string $pattern, int $mode = RegexIterator::MATCH, "` |
|     - |  4602 | `		  "int $flags = 0, int $pregFlags = 0", 0,` |
|     - |  4603 | `		  vm_builtin_RegexIterator_construct },` |
|     - |  4604 | `		{ "accept",       PH7_MOD_PUBLIC, "", "@bool", vm_builtin_RegexIterator_accept },` |
|     - |  4605 | `		{ "getMode",      PH7_MOD_PUBLIC, "", "@int", vm_builtin_RegexIterator_getMode },` |
|     - |  4606 | `		{ "setMode",      PH7_MOD_PUBLIC, "int $mode", "@void", vm_builtin_RegexIterator_setMode },` |
|     - |  4607 | `		{ "getFlags",     PH7_MOD_PUBLIC, "", "@int", vm_builtin_RegexIterator_getFlags },` |
|     - |  4608 | `		{ "setFlags",     PH7_MOD_PUBLIC, "int $flags", "@void", vm_builtin_RegexIterator_setFlags },` |
|     - |  4609 | `		{ "getRegex",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_RegexIterator_getRegex },` |
|     - |  4610 | `		{ "getPregFlags", PH7_MOD_PUBLIC, "", "@int", vm_builtin_RegexIterator_getPregFlags },` |
|     - |  4611 | `		{ "setPregFlags", PH7_MOD_PUBLIC, "int $pregFlags", "@void", vm_builtin_RegexIterator_setPregFlags },` |
|     - |  4612 | `	};` |
|     - |  4613 | `	static const PH7_NativeMethodDef aRecursiveMethod[] = {` |
|     - |  4614 | `		{ "hasChildren", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@bool", 0 },` |
|     - |  4615 | `		{ "getChildren", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@?RecursiveIterator", 0 },` |
|     - |  4616 | `	};` |
|     - |  4617 | `	static const PH7_NativeConstDef aRaiConst[] = {` |
|     - |  4618 | `		{ "CHILD_ARRAYS_ONLY", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RAI_CHILD_ARRAYS_ONLY, 0, 0.0 },` |
|     - |  4619 | `	};` |
|     - |  4620 | `	static const PH7_NativeMethodDef aRaiMethod[] = {` |
|     - |  4621 | `		{ "hasChildren", PH7_MOD_PUBLIC, "", "@bool",` |
|     - |  4622 | `		  vm_builtin_RecursiveArrayIterator_hasChildren },` |
|     - |  4623 | `		{ "getChildren", PH7_MOD_PUBLIC, "", "@?RecursiveArrayIterator",` |
|     - |  4624 | `		  vm_builtin_RecursiveArrayIterator_getChildren },` |
|     - |  4625 | `	};` |
|     - |  4626 | `	static const PH7_NativeMethodDef aRfiMethod[] = {` |
|     - |  4627 | `		{ "__construct", PH7_MOD_PUBLIC, "RecursiveIterator $iterator", 0,` |
|     - |  4628 | `		  vm_builtin_RecursiveFilterIterator_construct },` |
|     - |  4629 | `		{ "hasChildren", PH7_MOD_PUBLIC, "", "@bool",` |
|     - |  4630 | `		  vm_builtin_RecursiveFilterIterator_hasChildren },` |
|     - |  4631 | `		{ "getChildren", PH7_MOD_PUBLIC, "", "@?RecursiveFilterIterator",` |
|     - |  4632 | `		  vm_builtin_RecursiveFilterIterator_getChildren },` |
|     - |  4633 | `	};` |
|     - |  4634 | `	static const PH7_NativeMethodDef aParentMethod[] = {` |
|     - |  4635 | `		{ "__construct", PH7_MOD_PUBLIC, "RecursiveIterator $iterator", 0,` |
|     - |  4636 | `		  vm_builtin_ParentIterator_construct },` |
|     - |  4637 | `		{ "accept",      PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ParentIterator_accept },` |
|     - |  4638 | `	};` |
|     - |  4639 | `	static const PH7_NativeMethodDef aRcbfMethod[] = {` |
|     - |  4640 | `		{ "__construct", PH7_MOD_PUBLIC, "RecursiveIterator $iterator, callable $callback", 0,` |
|     - |  4641 | `		  vm_builtin_RecursiveCallbackFilterIterator_construct },` |
|     - |  4642 | `		{ "hasChildren", PH7_MOD_PUBLIC, "", "@bool",` |
|     - |  4643 | `		  vm_builtin_RecursiveFilterIterator_hasChildren },` |
|     - |  4644 | `		/* NOT nullable, where the RecursiveFilterIterator row above it is: php's` |
|     - |  4645 | `		 * nine getChildren stubs disagree with each other about a child every` |
|     - |  4646 | `		 * one of them builds the same way, and the declaration is what a` |
|     - |  4647 | `		 * program reads. */` |
|     - |  4648 | `		{ "getChildren", PH7_MOD_PUBLIC, "", "@RecursiveCallbackFilterIterator",` |
|     - |  4649 | `		  vm_builtin_RecursiveCallbackFilterIterator_getChildren },` |
|     - |  4650 | `	};` |
|     - |  4651 | `	static const PH7_NativeMethodDef aRregexMethod[] = {` |
|     - |  4652 | `		{ "__construct", PH7_MOD_PUBLIC,` |
|     - |  4653 | `		  "RecursiveIterator $iterator, string $pattern, "` |
|     - |  4654 | `		  "int $mode = RecursiveRegexIterator::MATCH, int $flags = 0, "` |
|     - |  4655 | `		  "int $pregFlags = 0", 0,` |
|     - |  4656 | `		  vm_builtin_RecursiveRegexIterator_construct },` |
|     - |  4657 | `		{ "accept",      PH7_MOD_PUBLIC, "", "@bool", vm_builtin_RecursiveRegexIterator_accept },` |
|     - |  4658 | `		{ "hasChildren", PH7_MOD_PUBLIC, "", "@bool",` |
|     - |  4659 | `		  vm_builtin_RecursiveFilterIterator_hasChildren },` |
|     - |  4660 | `		{ "getChildren", PH7_MOD_PUBLIC, "", "@RecursiveRegexIterator",` |
|     - |  4661 | `		  vm_builtin_RecursiveRegexIterator_getChildren },` |
|     - |  4662 | `	};` |
|     - |  4663 | `	static const PH7_NativeConstDef aCitConst[] = {` |
|     - |  4664 | `		{ "CALL_TOSTRING",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, CIT_CALL_TOSTRING, 0, 0.0 },` |
|     - |  4665 | `		{ "CATCH_GET_CHILD",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, CIT_CATCH_GET_CHILD, 0, 0.0 },` |
|     - |  4666 | `		{ "TOSTRING_USE_KEY",    PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, CIT_TOSTRING_USE_KEY, 0, 0.0 },` |
|     - |  4667 | `		{ "TOSTRING_USE_CURRENT",PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, CIT_TOSTRING_USE_CUR, 0, 0.0 },` |
|     - |  4668 | `		{ "TOSTRING_USE_INNER",  PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, CIT_TOSTRING_USE_INN, 0, 0.0 },` |
|     - |  4669 | `		{ "FULL_CACHE",          PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, CIT_FULL_CACHE, 0, 0.0 },` |
|     - |  4670 | `	};` |
|     - |  4671 | `	static const PH7_NativePropDef aCitProp[] = {` |
|     - |  4672 | `		{ CIT_FL,   PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  4673 | `		{ CIT_STR,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  4674 | `		{ CIT_CCH,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  4675 | `		{ CIT_KIDS, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  4676 | `	};` |
|     - |  4677 | `	static const PH7_NativeMethodDef aCitMethod[] = {` |
|     - |  4678 | `		{ "__construct", PH7_MOD_PUBLIC,` |
|     - |  4679 | `		  "Iterator $iterator, int $flags = CachingIterator::CALL_TOSTRING", 0,` |
|     - |  4680 | `		  vm_builtin_CachingIterator_construct },` |
|     - |  4681 | `		{ "rewind",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_CachingIterator_rewind },` |
|     - |  4682 | `		{ "valid",       PH7_MOD_PUBLIC, "", "@bool", vm_builtin_CachingIterator_valid },` |
|     - |  4683 | `		{ "next",        PH7_MOD_PUBLIC, "", "@void", vm_builtin_CachingIterator_next },` |
|     - |  4684 | `		{ "hasNext",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_CachingIterator_hasNext },` |
|     - |  4685 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_CachingIterator_toString },` |
|     - |  4686 | `		{ "getFlags",    PH7_MOD_PUBLIC, "", "@int", vm_builtin_CachingIterator_getFlags },` |
|     - |  4687 | `		{ "setFlags",    PH7_MOD_PUBLIC, "int $flags", "@void", vm_builtin_CachingIterator_setFlags },` |
|     - |  4688 | `		{ "offsetGet",   PH7_MOD_PUBLIC, "$key", "@mixed", vm_builtin_CachingIterator_offsetGet },` |
|     - |  4689 | `		{ "offsetSet",   PH7_MOD_PUBLIC, "$key, mixed $value", "@void",` |
|     - |  4690 | `		  vm_builtin_CachingIterator_offsetSet },` |
|     - |  4691 | `		{ "offsetUnset", PH7_MOD_PUBLIC, "$key", "@void", vm_builtin_CachingIterator_offsetUnset },` |
|     - |  4692 | `		{ "offsetExists",PH7_MOD_PUBLIC, "$key", "@bool", vm_builtin_CachingIterator_offsetExists },` |
|     - |  4693 | `		{ "getCache",    PH7_MOD_PUBLIC, "", "@array", vm_builtin_CachingIterator_getCache },` |
|     - |  4694 | `		{ "count",       PH7_MOD_PUBLIC, "", "@int", vm_builtin_CachingIterator_count },` |
|     - |  4695 | `	};` |
|     - |  4696 | `	static const PH7_NativeMethodDef aRcitMethod[] = {` |
|     - |  4697 | ``		/* `~Iterator`: php DECLARES Iterator here and its body asks for a`` |
|     - |  4698 | `		 * RecursiveIterator, so the screen stands aside and the constructor below` |
|     - |  4699 | `		 * raises php's own refusal. */` |
|     - |  4700 | `		{ "__construct", PH7_MOD_PUBLIC,` |
|     - |  4701 | `		  "~Iterator $iterator, int $flags = RecursiveCachingIterator::CALL_TOSTRING", 0,` |
|     - |  4702 | `		  vm_builtin_RecursiveCachingIterator_construct },` |
|     - |  4703 | `		{ "hasChildren", PH7_MOD_PUBLIC, "", "@bool",` |
|     - |  4704 | `		  vm_builtin_RecursiveCachingIterator_hasChildren },` |
|     - |  4705 | `		{ "getChildren", PH7_MOD_PUBLIC, "", "@?RecursiveCachingIterator",` |
|     - |  4706 | `		  vm_builtin_RecursiveCachingIterator_getChildren },` |
|     - |  4707 | `	};` |
|     - |  4708 | `	static const PH7_NativePropDef aAppendProp[] = {` |
|     - |  4709 | `		{ AP_LIST, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  4710 | `	};` |
|     - |  4711 | `	static const PH7_NativeMethodDef aAppendMethod[] = {` |
|     - |  4712 | `		{ "__construct",      PH7_MOD_PUBLIC, "", 0, vm_builtin_AppendIterator_construct },` |
|     - |  4713 | `		{ "append",           PH7_MOD_PUBLIC, "Iterator $iterator", "@void",` |
|     - |  4714 | `		  vm_builtin_AppendIterator_append },` |
|     - |  4715 | `		{ "rewind",           PH7_MOD_PUBLIC, "", "@void", vm_builtin_AppendIterator_rewind },` |
|     - |  4716 | `		{ "valid",            PH7_MOD_PUBLIC, "", "@bool", vm_builtin_Dual_valid },` |
|     - |  4717 | `		{ "current",          PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_AppendIterator_current },` |
|     - |  4718 | `		{ "next",             PH7_MOD_PUBLIC, "", "@void", vm_builtin_AppendIterator_next },` |
|     - |  4719 | `		{ "getIteratorIndex", PH7_MOD_PUBLIC, "", "@?int",` |
|     - |  4720 | `		  vm_builtin_AppendIterator_getIteratorIndex },` |
|     - |  4721 | `		{ "getArrayIterator", PH7_MOD_PUBLIC, "", "@ArrayIterator",` |
|     - |  4722 | `		  vm_builtin_AppendIterator_getArrayIterator },` |
|     - |  4723 | `	};` |
|     - |  4724 | `	static const PH7_NativeMethodDef aEmptyMethod[] = {` |
|     - |  4725 | `		{ "current", PH7_MOD_PUBLIC, "", "@never", vm_builtin_EmptyIterator_current },` |
|     - |  4726 | `		{ "next",    PH7_MOD_PUBLIC, "", "@void", vm_builtin_EmptyIterator_nop },` |
|     - |  4727 | `		{ "key",     PH7_MOD_PUBLIC, "", "@never", vm_builtin_EmptyIterator_key },` |
|     - |  4728 | `		{ "valid",   PH7_MOD_PUBLIC, "", "@false", vm_builtin_EmptyIterator_valid },` |
|     - |  4729 | `		{ "rewind",  PH7_MOD_PUBLIC, "", "@void", vm_builtin_EmptyIterator_nop },` |
|     - |  4730 | `	};` |
|     - |  4731 | `	/*` |
|     - |  4732 | ``	 * PH7_CLASS_NOCLONE on every dual iterator: php refuses `clone` for all of them`` |
|     - |  4733 | `	 * (its inner iterator handle cannot be duplicated), and a slot-by-slot copy here` |
|     - |  4734 | `	 * would share the inner iterator's cursor between two decorators. EmptyIterator` |
|     - |  4735 | `	 * has no state and php clones it happily.` |
|     - |  4736 | `	 */` |
|     - |  4737 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - |  4738 | `		{ "OuterIterator", "Iterator", 0, PH7_CLASS_INTERFACE,` |
|     - |  4739 | `		  aOuterMethod, SX_ARRAYSIZE(aOuterMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  4740 | `		{ "IteratorIterator", 0, "OuterIterator", PH7_CLASS_NOCLONE,` |
|     - |  4741 | `		  aIterIterMethod, SX_ARRAYSIZE(aIterIterMethod), 0, 0,` |
|     - |  4742 | `		  aDualProp, SX_ARRAYSIZE(aDualProp), 0, 0, 0 },` |
|     - |  4743 | `		{ "FilterIterator", "IteratorIterator", 0, PH7_CLASS_ABSTRACT\|PH7_CLASS_NOCLONE,` |
|     - |  4744 | `		  aFilterMethod, SX_ARRAYSIZE(aFilterMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  4745 | `		{ "CallbackFilterIterator", "FilterIterator", 0, PH7_CLASS_NOCLONE,` |
|     - |  4746 | `		  aCbFilterMethod, SX_ARRAYSIZE(aCbFilterMethod), 0, 0,` |
|     - |  4747 | `		  aCbProp, SX_ARRAYSIZE(aCbProp), 0, 0, 0 },` |
|     - |  4748 | `		{ "LimitIterator", "IteratorIterator", 0, PH7_CLASS_NOCLONE,` |
|     - |  4749 | `		  aLimitMethod, SX_ARRAYSIZE(aLimitMethod), 0, 0,` |
|     - |  4750 | `		  aLimitProp, SX_ARRAYSIZE(aLimitProp), 0, 0, 0 },` |
|     - |  4751 | `		{ "InfiniteIterator", "IteratorIterator", 0, PH7_CLASS_NOCLONE,` |
|     - |  4752 | `		  aInfiniteMethod, SX_ARRAYSIZE(aInfiniteMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  4753 | `		{ "NoRewindIterator", "IteratorIterator", 0, PH7_CLASS_NOCLONE,` |
|     - |  4754 | `		  aNoRewindMethod, SX_ARRAYSIZE(aNoRewindMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  4755 | `		{ "RegexIterator", "FilterIterator", 0, PH7_CLASS_NOCLONE,` |
|     - |  4756 | `		  aRegexMethod, SX_ARRAYSIZE(aRegexMethod),` |
|     - |  4757 | `		  aRegexConst, SX_ARRAYSIZE(aRegexConst),` |
|     - |  4758 | `		  aRegexProp, SX_ARRAYSIZE(aRegexProp), 0, 0, 0 },` |
|     - |  4759 | `		{ "AppendIterator", "IteratorIterator", 0, PH7_CLASS_NOCLONE,` |
|     - |  4760 | `		  aAppendMethod, SX_ARRAYSIZE(aAppendMethod), 0, 0,` |
|     - |  4761 | `		  aAppendProp, SX_ARRAYSIZE(aAppendProp), 0, 0, 0 },` |
|     - |  4762 | `		{ "RecursiveIterator", "Iterator", 0, PH7_CLASS_INTERFACE,` |
|     - |  4763 | `		  aRecursiveMethod, SX_ARRAYSIZE(aRecursiveMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  4764 | `		/* RecursiveArrayIterator is CLONEABLE (php clones an ArrayIterator happily) and` |
|     - |  4765 | `		 * inherits every one of its parent's C bodies, storage slots included. */` |
|     - |  4766 | `		{ "RecursiveArrayIterator", "ArrayIterator", "RecursiveIterator", 0,` |
|     - |  4767 | `		  aRaiMethod, SX_ARRAYSIZE(aRaiMethod),` |
|     - |  4768 | `		  aRaiConst, SX_ARRAYSIZE(aRaiConst), 0, 0, 0, 0, 0 },` |
|     - |  4769 | `		{ "RecursiveFilterIterator", "FilterIterator", "RecursiveIterator",` |
|     - |  4770 | `		  PH7_CLASS_ABSTRACT\|PH7_CLASS_NOCLONE,` |
|     - |  4771 | `		  aRfiMethod, SX_ARRAYSIZE(aRfiMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  4772 | `		/* The three recursive twins. The two whose parent is a PLAIN filter name` |
|     - |  4773 | `		 * RecursiveIterator themselves; ParentIterator inherits it from` |
|     - |  4774 | `		 * RecursiveFilterIterator, which is where php has it too. */` |
|     - |  4775 | `		{ "ParentIterator", "RecursiveFilterIterator", 0, PH7_CLASS_NOCLONE,` |
|     - |  4776 | `		  aParentMethod, SX_ARRAYSIZE(aParentMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  4777 | `		{ "RecursiveCallbackFilterIterator", "CallbackFilterIterator", "RecursiveIterator",` |
|     - |  4778 | `		  PH7_CLASS_NOCLONE,` |
|     - |  4779 | `		  aRcbfMethod, SX_ARRAYSIZE(aRcbfMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  4780 | `		{ "RecursiveRegexIterator", "RegexIterator", "RecursiveIterator", PH7_CLASS_NOCLONE,` |
|     - |  4781 | `		  aRregexMethod, SX_ARRAYSIZE(aRregexMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  4782 | `		/* CachingIterator is Stringable through __toString, and Countable/ArrayAccess` |
|     - |  4783 | `		 * over the FULL_CACHE array — three interfaces the class refuses to serve` |
|     - |  4784 | `		 * unless it was built with that flag. */` |
|     - |  4785 | `		{ "CachingIterator", "IteratorIterator", "ArrayAccess,Countable,Stringable",` |
|     - |  4786 | `		  PH7_CLASS_NOCLONE,` |
|     - |  4787 | `		  aCitMethod, SX_ARRAYSIZE(aCitMethod), aCitConst, SX_ARRAYSIZE(aCitConst),` |
|     - |  4788 | `		  aCitProp, SX_ARRAYSIZE(aCitProp), 0, 0, 0 },` |
|     - |  4789 | `		{ "RecursiveCachingIterator", "CachingIterator", "RecursiveIterator", PH7_CLASS_NOCLONE,` |
|     - |  4790 | `		  aRcitMethod, SX_ARRAYSIZE(aRcitMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  4791 | `		{ "EmptyIterator", 0, "Iterator", 0,` |
|     - |  4792 | `		  aEmptyMethod, SX_ARRAYSIZE(aEmptyMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  4793 | `	};` |
|  6726 |  4794 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 |  4795 | `}` |
|     - |  4796 | `/*` |
|     - |  4797 | ` * ---------------------------------------------------------------------------` |
|     - |  4798 | ` * RecursiveIteratorIterator.` |
|     - |  4799 | ` *` |
|     - |  4800 | `` * php's `spl_recursive_it_object` is a STACK OF LEVELS plus a five-value state`` |
|     - |  4801 | ` * machine, and reading the struct before the methods (rule 43) is what this` |
|     - |  4802 | ` * conversion turns on. Each level carries the sub-iterator AND its own` |
|     - |  4803 | `` * RecursiveIteratorState; `move_forward` is one loop over that pair, and every`` |
|     - |  4804 | ` * method is a two-line reader of it. The chunk instead kept a stack of iterators` |
|     - |  4805 | `` * with the state implied by two booleans (`__post`, `__live`), which is where all`` |
|     - |  4806 | ` * eight of its divergences came from:` |
|     - |  4807 | ` *` |
|     - |  4808 | ` *   - getDepth()/getSubIterator()/getInnerIterator() answered from an EMPTY stack` |
|     - |  4809 | ` *     before the first rewind(), so they reported -1 and null where php reports 0` |
|     - |  4810 | ` *     and the root -- php seeds level 0 in the CONSTRUCTOR and never unseeds it.` |
|     - |  4811 | `` *   - valid() answered a `__live` flag that only rewind() sets; php ASKS the`` |
|     - |  4812 | ` *     levels (any valid sub-iterator, walking down), so a fresh instance over a` |
|     - |  4813 | ` *     non-empty iterator is already valid().` |
|     - |  4814 | ` *   - LEAVES_ONLY past max depth YIELDED the container; php skips it, which is` |
|     - |  4815 | ``  *     the whole point of the mode (`walk-leaves-maxdepth0` returned the `b` `` |
|     - |  4816 | ` *     array as if it were a leaf).` |
|     - |  4817 | `` *   - the mode was `$mode \| $flags` masked with & 3, so CATCH_GET_CHILD passed`` |
|     - |  4818 | ` *     as $mode descended like LEAVES_ONLY; php compares mode EXACTLY and an` |
|     - |  4819 | ` *     unknown mode matches no arm at all, descending nowhere.` |
|     - |  4820 | ` *   - hasChildren() was called on the sub-iterator DIRECTLY, so a subclass` |
|     - |  4821 | ` *     overriding callHasChildren() -- php's documented hook -- was never asked.` |
|     - |  4822 | ` *   - endChildren() ran AFTER the pop, reporting a depth one too low and firing` |
|     - |  4823 | ` *     a spurious final call at depth -1; php calls it before the pop.` |
|     - |  4824 | ` *   - a second rewind() fired beginIteration() again; php's in_iteration latch` |
|     - |  4825 | ` *     makes it once per iteration.` |
|     - |  4826 | ` *   - getChildren() returning a non-RecursiveIterator was silently treated as` |
|     - |  4827 | ` *     "no children"; php throws UnexpectedValueException.` |
|     - |  4828 | ` *` |
|     - |  4829 | ` * The level stack lives in two parallel arrays indexed by level rather than in a` |
|     - |  4830 | `` * C block behind a handle: php SERIALIZES this class (`O:25:"…":0:{}`), and a raw`` |
|     - |  4831 | ` * pointer in a hidden slot is exactly what rule 19 exists to keep out of` |
|     - |  4832 | ` * serialize() output. Every slot is PH7_MOD_HIDDEN, so php's zero-property` |
|     - |  4833 | ` * presentation holds for var_dump, print_r, var_export, (array) and Reflection.` |
|     - |  4834 | ` */` |
|     - |  4835 | `#define RIT_ST   "__st"   /* php's iterators[level].zobject */` |
|     - |  4836 | `#define RIT_SS   "__ss"   /* php's iterators[level].state */` |
|     - |  4837 | `#define RIT_LVL  "__lvl"  /* php's object->level */` |
|     - |  4838 | `#define RIT_MD   "__md"   /* php's object->mode, stored UNMASKED */` |
|     - |  4839 | `#define RIT_FL   "__fl"   /* php's object->flags */` |
|     - |  4840 | `#define RIT_MX   "__mx"   /* php's object->max_depth, -1 = unlimited */` |
|     - |  4841 | `#define RIT_II   "__ii"   /* php's object->in_iteration */` |
|     - |  4842 | ``#define RIT_RD   "__rd"   /* php's `object->iterators != NULL`: the parent ctor ran */`` |
|     - |  4843 |  |
|     - |  4844 | `/* php's RecursiveIteratorState */` |
|     - |  4845 | `#define RS_NEXT  0` |
|     - |  4846 | `#define RS_TEST  1` |
|     - |  4847 | `#define RS_SELF  2` |
|     - |  4848 | `#define RS_CHILD 3` |
|     - |  4849 | `#define RS_START 4` |
|     - |  4850 |  |
|     - |  4851 | `/* php's RecursiveIteratorMode + the one flag */` |
|     - |  4852 | `#define RIT_LEAVES_ONLY     0` |
|     - |  4853 | `#define RIT_SELF_FIRST      1` |
|     - |  4854 | `#define RIT_CHILD_FIRST     2` |
|     - |  4855 | `#define RIT_CATCH_GET_CHILD 16` |
|     - |  4856 |  |
|     - |  4857 | `/*` |
|     - |  4858 | `` * php's `object->iterators != NULL`. Its get_method handler refuses EVERY method`` |
|     - |  4859 | ` * on an instance whose parent constructor never ran -- not the individual bodies,` |
|     - |  4860 | ` * which is why the refusal is an Error naming the RUNTIME class and why even` |
|     - |  4861 | ` * getDepth() raises it.` |
|     - |  4862 | ` */` |
|  3376 |  4863 | `static int RitReady(ph7_class_instance *pThis)` |
|     3 |  4864 | `{` |
|  3379 |  4865 | `	return pThis && PH7_NativeAttrInt(pThis,RIT_RD) != 0;` |
|     3 |  4866 | `}` |
|    20 |  4867 | `static sxi32 RitNotReady(ph7_context *pCtx)` |
|     1 |  4868 | `{` |
|    21 |  4869 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    21 |  4870 | `	SyString *pName = pThis ? &pThis->pClass->sName : 0;` |
|    31 |  4871 | `	return PH7_VmThrowException(pCtx,"Error",` |
|    10 |  4872 | `		"The %z instance wasn't initialized properly",pName);` |
|     1 |  4873 | `}` |
|  4994 |  4874 | `static int RitInt(ph7_class_instance *pThis,const char *zSlot)` |
|     3 |  4875 | `{` |
|  4997 |  4876 | `	return (int)PH7_NativeAttrInt(pThis,zSlot);` |
|     3 |  4877 | `}` |
|     - |  4878 | `/* One of the two level-indexed arrays, materialized on first use. */` |
|  6476 |  4879 | `static ph7_hashmap * RitMap(ph7_vm *pVm,ph7_class_instance *pThis,const char *zSlot)` |
|     3 |  4880 | `{` |
|  6479 |  4881 | `	ph7_value *pSlot = PH7_NativeAttr(pThis,zSlot);` |
|  6479 |  4882 | `	if( pSlot == 0 ){` |
|   ! 0 |  4883 | `		return 0;` |
|     - |  4884 | `	}` |
|  6479 |  4885 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   291 |  4886 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|   ! 0 |  4887 | `			return 0;` |
|     - |  4888 | `		}` |
|   144 |  4889 | `	}` |
|  6479 |  4890 | `	return PH7_HashmapCowSeparate(pVm,pSlot);` |
|  3241 |  4891 | `}` |
|  4314 |  4892 | `static ph7_value * RitAt(ph7_vm *pVm,ph7_class_instance *pThis,const char *zSlot,int iLevel)` |
|     3 |  4893 | `{` |
|  4317 |  4894 | `	ph7_hashmap *pMap = RitMap(pVm,pThis,zSlot);` |
|  4317 |  4895 | `	ph7_hashmap_node *pNode = 0;` |
|  4317 |  4896 | `	if( pMap == 0 \|\| HashmapLookupIntKey(pMap,(sxi64)iLevel,&pNode) != SXRET_OK ){` |
|   ! 0 |  4897 | `		return 0;` |
|     - |  4898 | `	}` |
|  4317 |  4899 | `	return HashmapExtractNodeValue(pNode);` |
|  2160 |  4900 | `}` |
|  1626 |  4901 | `static void RitPut(ph7_vm *pVm,ph7_class_instance *pThis,const char *zSlot,int iLevel,ph7_value *pVal)` |
|     3 |  4902 | `{` |
|  1629 |  4903 | `	ph7_hashmap *pMap = RitMap(pVm,pThis,zSlot);` |
|     - |  4904 | `	ph7_value sKey;` |
|  1629 |  4905 | `	if( pMap == 0 ){` |
|   ! 0 |  4906 | `		return;` |
|     - |  4907 | `	}` |
|  1629 |  4908 | `	PH7_MemObjInitFromInt(pVm,&sKey,(sxi64)iLevel);` |
|  1629 |  4909 | `	PH7_HashmapInsert(pMap,&sKey,pVal);` |
|  1629 |  4910 | `	PH7_MemObjRelease(&sKey);` |
|   816 |  4911 | `}` |
|   536 |  4912 | `static void RitErase(ph7_vm *pVm,ph7_class_instance *pThis,const char *zSlot,int iLevel)` |
|     3 |  4913 | `{` |
|   539 |  4914 | `	ph7_hashmap *pMap = RitMap(pVm,pThis,zSlot);` |
|   539 |  4915 | `	ph7_hashmap_node *pNode = 0;` |
|   539 |  4916 | `	if( pMap && HashmapLookupIntKey(pMap,(sxi64)iLevel,&pNode) == SXRET_OK ){` |
|   251 |  4917 | `		PH7_HashmapUnlinkNode(pNode,TRUE);` |
|   124 |  4918 | `	}` |
|   539 |  4919 | `}` |
|     - |  4920 | `/*` |
|     - |  4921 | ` * The sub-iterator at a level. Re-resolved on every use on purpose: every call into` |
|     - |  4922 | ` * a user iterator can rewrite the level's own storage (rule 47). It used to matter` |
|     - |  4923 | ` * for a second reason as well -- RitAt() hands back a pointer into pVm->aMemObj and` |
|     - |  4924 | ` * the pool reallocated as the VM reserved objects -- and that one is gone since P1.` |
|     - |  4925 | ` */` |
|  3508 |  4926 | `static ph7_class_instance * RitSub(ph7_vm *pVm,ph7_class_instance *pThis,int iLevel)` |
|     3 |  4927 | `{` |
|  3511 |  4928 | `	ph7_value *pVal = RitAt(pVm,pThis,RIT_ST,iLevel);` |
|  3511 |  4929 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 |  4930 | `		return 0;` |
|     - |  4931 | `	}` |
|  3511 |  4932 | `	return (ph7_class_instance *)pVal->x.pOther;` |
|  1757 |  4933 | `}` |
|   806 |  4934 | `static int RitState(ph7_vm *pVm,ph7_class_instance *pThis,int iLevel)` |
|     3 |  4935 | `{` |
|   809 |  4936 | `	ph7_value *pVal = RitAt(pVm,pThis,RIT_SS,iLevel);` |
|   809 |  4937 | `	return pVal ? (int)ph7_value_to_int64(pVal) : RS_START;` |
|     3 |  4938 | `}` |
|  1358 |  4939 | `static void RitSetState(ph7_vm *pVm,ph7_class_instance *pThis,int iLevel,int iState)` |
|     3 |  4940 | `{` |
|     - |  4941 | `	ph7_value sVal;` |
|  1361 |  4942 | `	PH7_MemObjInitFromInt(pVm,&sVal,(sxi64)iState);` |
|  1361 |  4943 | `	RitPut(pVm,pThis,RIT_SS,iLevel,&sVal);` |
|  1361 |  4944 | `	PH7_MemObjRelease(&sVal);` |
|  1361 |  4945 | `}` |
|     - |  4946 | ``/* php's `iterators = erealloc(…, ++level+1)` plus the two field writes. */`` |
|   124 |  4947 | `static void RitPush(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_instance *pChild)` |
|     3 |  4948 | `{` |
|   127 |  4949 | `	int iLevel = RitInt(pThis,RIT_LVL) + 1;` |
|     - |  4950 | `	ph7_value sObj;` |
|   127 |  4951 | `	PH7_MemObjInit(pVm,&sObj);` |
|   127 |  4952 | `	sObj.x.pOther = pChild;` |
|   127 |  4953 | `	MemObjSetType(&sObj,MEMOBJ_OBJ);` |
|     - |  4954 | `	/* The map takes its OWN reference through the store; the carrier is blanked` |
|     - |  4955 | `	 * rather than released, because releasing a MEMOBJ_OBJ carrier would unref an` |
|     - |  4956 | `	 * instance this frame never referenced (rule 16). */` |
|   127 |  4957 | `	RitPut(pVm,pThis,RIT_ST,iLevel,&sObj);` |
|   127 |  4958 | `	sObj.x.pOther = 0;` |
|   127 |  4959 | `	MemObjSetType(&sObj,MEMOBJ_NULL);` |
|   127 |  4960 | `	PH7_MemObjRelease(&sObj);` |
|   127 |  4961 | `	RitSetState(pVm,pThis,iLevel,RS_START);` |
|   127 |  4962 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_LVL,iLevel);` |
|   127 |  4963 | `}` |
|   124 |  4964 | `static void RitPop(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     3 |  4965 | `{` |
|   127 |  4966 | `	int iLevel = RitInt(pThis,RIT_LVL);` |
|   127 |  4967 | `	if( iLevel <= 0 ){` |
|   ! 0 |  4968 | `		return;` |
|     - |  4969 | `	}` |
|   127 |  4970 | `	RitErase(pVm,pThis,RIT_ST,iLevel);` |
|   127 |  4971 | `	RitErase(pVm,pThis,RIT_SS,iLevel);` |
|   127 |  4972 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_LVL,iLevel-1);` |
|    65 |  4973 | `}` |
|     - |  4974 | `/* Drop every level: php's spl_RecursiveIteratorIterator_free_iterators. */` |
|   144 |  4975 | `static void RitClear(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     3 |  4976 | `{` |
|   147 |  4977 | `	int iLevel = RitInt(pThis,RIT_LVL);` |
|   291 |  4978 | `	while( iLevel >= 0 ){` |
|   147 |  4979 | `		RitErase(pVm,pThis,RIT_ST,iLevel);` |
|   147 |  4980 | `		RitErase(pVm,pThis,RIT_SS,iLevel);` |
|   147 |  4981 | `		iLevel--;` |
|     3 |  4982 | `	}` |
|   147 |  4983 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_LVL,0);` |
|   147 |  4984 | `}` |
|     - |  4985 | `/*` |
|     - |  4986 | ` * Call a method, optionally SWALLOWING what it throws -- php clears the exception` |
|     - |  4987 | ` * at four sites when RIT_CATCH_GET_CHILD is set, and PH7_VmCallMethodSwallow is` |
|     - |  4988 | ` * the only way to spell that here (a throw raised under a C call site is` |
|     - |  4989 | ` * dispatched INLINE, so an enclosing user catch would run before this returns).` |
|     - |  4990 | ` * *pbThrew reports a swallowed throw, which php reads back as "retval is UNDEF".` |
|     - |  4991 | ` */` |
|  3878 |  4992 | `static sxi32 RitCall(ph7_context *pCtx,ph7_class_instance *pObj,const char *zName,sxu32 nName,` |
|     - |  4993 | `	ph7_value *pOut,int bCatch,int *pbThrew)` |
|     3 |  4994 | `{` |
|  3881 |  4995 | `	ph7_class_method *pMethod = pObj ? PH7_ClassExtractMethod(pObj->pClass,zName,nName) : 0;` |
|  3881 |  4996 | `	if( pbThrew ){` |
|  1477 |  4997 | `		*pbThrew = FALSE;` |
|   737 |  4998 | `	}` |
|  3881 |  4999 | `	if( pMethod == 0 ){` |
|   ! 0 |  5000 | `		return SXRET_OK;` |
|     - |  5001 | `	}` |
|  3881 |  5002 | `	if( bCatch ){` |
|    13 |  5003 | `		return PH7_VmCallMethodSwallow(pCtx->pVm,pObj,pMethod,pOut,0,0,pbThrew);` |
|     - |  5004 | `	}` |
|  3869 |  5005 | `	return PH7_VmCallClassMethod(pCtx->pVm,pObj,pMethod,pOut,0,0);` |
|  1942 |  5006 | `}` |
|     - |  5007 | `/*` |
|     - |  5008 | ` * A hook on $this. php caches which of the seven the SUBCLASS overrides and calls` |
|     - |  5009 | ` * the sub-iterator directly when none does; dispatching through $this every time` |
|     - |  5010 | ` * reaches the same body -- the base ones are the no-ops php would have skipped --` |
|     - |  5011 | ` * with the override found automatically.` |
|     - |  5012 | ` */` |
|  1288 |  5013 | `static sxi32 RitHook(ph7_context *pCtx,const char *zName,sxu32 nName,ph7_value *pOut,` |
|     - |  5014 | `	int bCatch,int *pbThrew)` |
|     3 |  5015 | `{` |
|  1291 |  5016 | `	return RitCall(pCtx,PH7_ContextThis(pCtx),zName,nName,pOut,bCatch,pbThrew);` |
|     3 |  5017 | `}` |
|   420 |  5018 | `static int RitCatches(ph7_class_instance *pThis)` |
|     3 |  5019 | `{` |
|   423 |  5020 | `	return (RitInt(pThis,RIT_FL) & RIT_CATCH_GET_CHILD) != 0;` |
|     3 |  5021 | `}` |
|     - |  5022 | `/*` |
|     - |  5023 | ` * php's spl_recursive_it_move_forward_ex, transcribed. The switch's fallthroughs` |
|     - |  5024 | ` * (RS_NEXT into RS_START into RS_TEST) are written as a sequential if-chain, and` |
|     - |  5025 | `` * php's `goto next_step` is this loop's `continue`.`` |
|     - |  5026 | ` */` |
|   420 |  5027 | `static sxi32 RitMoveForward(ph7_context *pCtx)` |
|     3 |  5028 | `{` |
|   423 |  5029 | `	ph7_vm *pVm = pCtx->pVm;` |
|   423 |  5030 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  5031 | `	int bCatch;` |
|   423 |  5032 | `	if( !RitReady(pThis) ){` |
|   ! 0 |  5033 | `		return RitNotReady(pCtx);` |
|     - |  5034 | `	}` |
|   423 |  5035 | `	bCatch = RitCatches(pThis);` |
|   465 |  5036 | `	for(;;){` |
|     - |  5037 | `		ph7_class_instance *pSub;` |
|   809 |  5038 | `		int iLevel = RitInt(pThis,RIT_LVL);` |
|   809 |  5039 | `		int iState = RitState(pVm,pThis,iLevel);` |
|   809 |  5040 | `		int bThrew = 0;` |
|   809 |  5041 | `		int bExhausted = 0;` |
|     - |  5042 | `		sxi32 rc;` |
|   809 |  5043 | `		pSub = RitSub(pVm,pThis,iLevel);` |
|   809 |  5044 | `		if( pSub == 0 ){` |
|   ! 0 |  5045 | `			return PH7_OK;` |
|     - |  5046 | `		}` |
|   809 |  5047 | `		if( iState == RS_NEXT ){` |
|   375 |  5048 | `			rc = RitCall(pCtx,pSub,"next",sizeof("next")-1,0,bCatch,&bThrew);` |
|   375 |  5049 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  5050 | `				return rc;` |
|     - |  5051 | `			}` |
|   375 |  5052 | `			pSub = RitSub(pVm,pThis,iLevel);   /* the call may have rewritten the level */` |
|   375 |  5053 | `			if( pSub == 0 ){` |
|   ! 0 |  5054 | `				return PH7_OK;` |
|     - |  5055 | `			}` |
|   375 |  5056 | `			iState = RS_START;                 /* php's fallthrough */` |
|   186 |  5057 | `		}` |
|   809 |  5058 | `		if( iState == RS_START ){` |
|     - |  5059 | `			ph7_value sValid;` |
|   603 |  5060 | `			PH7_MemObjInit(pVm,&sValid);` |
|   603 |  5061 | `			rc = RitCall(pCtx,pSub,"valid",sizeof("valid")-1,&sValid,FALSE,0);` |
|   603 |  5062 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  5063 | `				PH7_MemObjRelease(&sValid);` |
|   ! 0 |  5064 | `				return rc;` |
|     - |  5065 | `			}` |
|   603 |  5066 | `			bExhausted = !ph7_value_to_bool(&sValid);` |
|   603 |  5067 | `			PH7_MemObjRelease(&sValid);` |
|   603 |  5068 | `			if( !bExhausted ){` |
|     - |  5069 | `				/* php re-reads the level here and returns outright when the valid()` |
|     - |  5070 | `				 * call RE-ENTERED this iterator (a sub-iterator that drove the` |
|     - |  5071 | `				 * decorator behind its back); the stack it was walking is gone. */` |
|   395 |  5072 | `				if( RitInt(pThis,RIT_LVL) != iLevel \|\| RitSub(pVm,pThis,iLevel) != pSub ){` |
|   ! 0 |  5073 | `					return PH7_OK;` |
|     - |  5074 | `				}` |
|   395 |  5075 | `				RitSetState(pVm,pThis,iLevel,RS_TEST);` |
|   395 |  5076 | `				iState = RS_TEST;` |
|   196 |  5077 | `			}` |
|   300 |  5078 | `		}` |
|   809 |  5079 | `		if( !bExhausted && iState == RS_TEST ){` |
|     - |  5080 | `			ph7_value sHas;` |
|   395 |  5081 | `			int bDescend = 0;` |
|   395 |  5082 | `			PH7_MemObjInit(pVm,&sHas);` |
|   395 |  5083 | `			rc = RitHook(pCtx,"callHasChildren",sizeof("callHasChildren")-1,&sHas,bCatch,&bThrew);` |
|   395 |  5084 | `			if( rc != SXRET_OK ){` |
|     - |  5085 | `				/* php leaves the level on RS_NEXT so a caught-and-resumed traversal` |
|     - |  5086 | `				 * moves on rather than re-asking the same element. */` |
|   ! 0 |  5087 | `				RitSetState(pVm,pThis,iLevel,RS_NEXT);` |
|   ! 0 |  5088 | `				PH7_MemObjRelease(&sHas);` |
|   ! 0 |  5089 | `				return rc;` |
|     - |  5090 | `			}` |
|     - |  5091 | `			/* A SWALLOWED throw leaves php's retval UNDEF, which skips the` |
|     - |  5092 | `			 * has-children test entirely and yields the element. */` |
|   395 |  5093 | `			if( !bThrew && ph7_value_to_bool(&sHas) ){` |
|   151 |  5094 | `				int iMax = RitInt(pThis,RIT_MX);` |
|   151 |  5095 | `				int iMode = RitInt(pThis,RIT_MD);` |
|   151 |  5096 | `				if( iMax == -1 \|\| iMax > iLevel ){` |
|     - |  5097 | `					/* php compares the mode EXACTLY: an unrecognized mode matches no` |
|     - |  5098 | `					 * arm, falls out of the switch and yields without descending. */` |
|   139 |  5099 | `					if( iMode == RIT_LEAVES_ONLY \|\| iMode == RIT_CHILD_FIRST ){` |
|    76 |  5100 | `						RitSetState(pVm,pThis,iLevel,RS_CHILD);` |
|    76 |  5101 | `						bDescend = 1;` |
|   101 |  5102 | `					}else if( iMode == RIT_SELF_FIRST ){` |
|    60 |  5103 | `						RitSetState(pVm,pThis,iLevel,RS_SELF);` |
|    60 |  5104 | `						bDescend = 1;` |
|    32 |  5105 | `					}` |
|    81 |  5106 | `				}else if( iMode == RIT_LEAVES_ONLY ){` |
|     - |  5107 | `					/* Too deep to recurse into and NOT a leaf, so php skips it —` |
|     - |  5108 | `					 * the mode's defining rule, and the one the chunk dropped. */` |
|     5 |  5109 | `					RitSetState(pVm,pThis,iLevel,RS_NEXT);` |
|     5 |  5110 | `					bDescend = 1;` |
|     2 |  5111 | `				}` |
|    74 |  5112 | `			}` |
|   395 |  5113 | `			PH7_MemObjRelease(&sHas);` |
|   395 |  5114 | `			if( bDescend ){` |
|   139 |  5115 | `				continue;                      /* php's goto next_step */` |
|     - |  5116 | `			}` |
|   259 |  5117 | `			rc = RitHook(pCtx,"nextElement",sizeof("nextElement")-1,0,bCatch,&bThrew);` |
|   259 |  5118 | `			RitSetState(pVm,pThis,iLevel,RS_NEXT);` |
|   259 |  5119 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  5120 | `				return rc;` |
|     - |  5121 | `			}` |
|   259 |  5122 | `			return PH7_OK;                     /* yield this element */` |
|     - |  5123 | `		}` |
|   417 |  5124 | `		if( !bExhausted && iState == RS_SELF ){` |
|    78 |  5125 | `			int iMode = RitInt(pThis,RIT_MD);` |
|    78 |  5126 | `			if( iMode == RIT_SELF_FIRST \|\| iMode == RIT_CHILD_FIRST ){` |
|    78 |  5127 | `				rc = RitHook(pCtx,"nextElement",sizeof("nextElement")-1,0,bCatch,&bThrew);` |
|    78 |  5128 | `				if( rc != SXRET_OK ){` |
|   ! 0 |  5129 | `					return rc;` |
|     - |  5130 | `				}` |
|    38 |  5131 | `			}` |
|    78 |  5132 | `			RitSetState(pVm,pThis,iLevel,iMode == RIT_SELF_FIRST ? RS_CHILD : RS_NEXT);` |
|    78 |  5133 | `			return PH7_OK;                     /* yield this element */` |
|     - |  5134 | `		}` |
|   341 |  5135 | `		if( !bExhausted && iState == RS_CHILD ){` |
|     - |  5136 | `			ph7_class *pRecCls;` |
|     - |  5137 | `			ph7_class_instance *pChild;` |
|     - |  5138 | `			ph7_value sChild;` |
|   133 |  5139 | `			int iMode = RitInt(pThis,RIT_MD);` |
|   133 |  5140 | `			PH7_MemObjInit(pVm,&sChild);` |
|   133 |  5141 | `			rc = RitHook(pCtx,"callGetChildren",sizeof("callGetChildren")-1,&sChild,bCatch,&bThrew);` |
|   133 |  5142 | `			if( rc != SXRET_OK ){` |
|     3 |  5143 | `				PH7_MemObjRelease(&sChild);` |
|     4 |  5144 | `				return rc;` |
|     - |  5145 | `			}` |
|   131 |  5146 | `			if( bThrew ){` |
|     - |  5147 | `				/* Caught: php drops the element and moves to the next one. */` |
|     3 |  5148 | `				PH7_MemObjRelease(&sChild);` |
|     3 |  5149 | `				RitSetState(pVm,pThis,iLevel,RS_NEXT);` |
|    65 |  5150 | `				continue;` |
|     - |  5151 | `			}` |
|   129 |  5152 | `			pRecCls = PH7_VmExtractClass(pVm,"RecursiveIterator",` |
|     - |  5153 | `				sizeof("RecursiveIterator")-1,FALSE,0);` |
|   192 |  5154 | `			pChild = (sChild.iFlags & MEMOBJ_OBJ) != 0` |
|   125 |  5155 | `				? (ph7_class_instance *)sChild.x.pOther : 0;` |
|   129 |  5156 | `			if( pChild == 0 \|\| (pRecCls && !PH7_VmInstanceOf(pChild->pClass,pRecCls)) ){` |
|     3 |  5157 | `				PH7_MemObjRelease(&sChild);` |
|     3 |  5158 | `				return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - |  5159 | `					"Objects returned by RecursiveIterator::getChildren() must implement RecursiveIterator");` |
|     - |  5160 | `			}` |
|   127 |  5161 | `			pChild->iRef++;                    /* survive the release of the call result */` |
|   127 |  5162 | `			PH7_MemObjRelease(&sChild);` |
|   127 |  5163 | `			RitSetState(pVm,pThis,iLevel,iMode == RIT_CHILD_FIRST ? RS_SELF : RS_NEXT);` |
|   127 |  5164 | `			RitPush(pVm,pThis,pChild);` |
|   127 |  5165 | `			PH7_ClassInstanceUnref(pChild);    /* the level's slot holds it now */` |
|   127 |  5166 | `			rc = RitCall(pCtx,pChild,"rewind",sizeof("rewind")-1,0,FALSE,0);` |
|   127 |  5167 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  5168 | `				return rc;` |
|     - |  5169 | `			}` |
|   127 |  5170 | `			rc = RitHook(pCtx,"beginChildren",sizeof("beginChildren")-1,0,bCatch,&bThrew);` |
|   127 |  5171 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  5172 | `				return rc;` |
|     - |  5173 | `			}` |
|   127 |  5174 | `			continue;                          /* php's goto next_step */` |
|     - |  5175 | `		}` |
|     - |  5176 | `		/* No more elements at this level. */` |
|   211 |  5177 | `		if( iLevel <= 0 ){` |
|    87 |  5178 | `			return PH7_OK;                     /* done completely */` |
|     - |  5179 | `		}` |
|     - |  5180 | `		/* php calls endChildren BEFORE the pop, so the hook sees the depth it is` |
|     - |  5181 | `		 * leaving rather than the one it lands on. */` |
|   127 |  5182 | `		rc = RitHook(pCtx,"endChildren",sizeof("endChildren")-1,0,bCatch,&bThrew);` |
|   127 |  5183 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  5184 | `			return rc;` |
|     - |  5185 | `		}` |
|   127 |  5186 | `		if( RitInt(pThis,RIT_LVL) > 0 && RitSub(pVm,pThis,RitInt(pThis,RIT_LVL)) == pSub ){` |
|   127 |  5187 | `			RitPop(pVm,pThis);` |
|    62 |  5188 | `		}` |
|     3 |  5189 | `	}` |
|   213 |  5190 | `}` |
|     - |  5191 | `/*` |
|     - |  5192 | ` * php's spl_recursive_it_valid_ex: ASK the levels, walking down from the current` |
|     - |  5193 | ` * one, and fire endIteration the first time the answer is no.` |
|     - |  5194 | ` */` |
|   402 |  5195 | `static sxi32 RitValidEx(ph7_context *pCtx,int *pbValid)` |
|     3 |  5196 | `{` |
|   405 |  5197 | `	ph7_vm *pVm = pCtx->pVm;` |
|   405 |  5198 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   405 |  5199 | `	int iLevel = RitInt(pThis,RIT_LVL);` |
|     - |  5200 | `	sxi32 rc;` |
|   405 |  5201 | `	*pbValid = FALSE;` |
|   491 |  5202 | `	while( iLevel >= 0 ){` |
|   405 |  5203 | `		ph7_class_instance *pSub = RitSub(pVm,pThis,iLevel);` |
|     - |  5204 | `		ph7_value sValid;` |
|     - |  5205 | `		int bOk;` |
|   405 |  5206 | `		if( pSub == 0 ){` |
|   ! 0 |  5207 | `			iLevel--;` |
|   ! 0 |  5208 | `			continue;` |
|     - |  5209 | `		}` |
|   405 |  5210 | `		PH7_MemObjInit(pVm,&sValid);` |
|   405 |  5211 | `		rc = RitCall(pCtx,pSub,"valid",sizeof("valid")-1,&sValid,FALSE,0);` |
|   405 |  5212 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  5213 | `			PH7_MemObjRelease(&sValid);` |
|   ! 0 |  5214 | `			return rc;` |
|     - |  5215 | `		}` |
|   405 |  5216 | `		bOk = ph7_value_to_bool(&sValid);` |
|   405 |  5217 | `		PH7_MemObjRelease(&sValid);` |
|   405 |  5218 | `		if( bOk ){` |
|   319 |  5219 | `			*pbValid = TRUE;` |
|   319 |  5220 | `			return PH7_OK;` |
|     - |  5221 | `		}` |
|    89 |  5222 | `		iLevel--;` |
|     3 |  5223 | `	}` |
|    89 |  5224 | `	if( RitInt(pThis,RIT_II) ){` |
|    87 |  5225 | `		rc = RitHook(pCtx,"endIteration",sizeof("endIteration")-1,0,FALSE,0);` |
|    87 |  5226 | `		PH7_NativeSetAttrInt(pVm,pThis,RIT_II,0);` |
|    87 |  5227 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  5228 | `			return rc;` |
|     - |  5229 | `		}` |
|    42 |  5230 | `	}` |
|    89 |  5231 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_II,0);` |
|    89 |  5232 | `	return PH7_OK;` |
|   204 |  5233 | `}` |
|   148 |  5234 | `static int vm_builtin_RecursiveIteratorIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  5235 | `{` |
|   151 |  5236 | `	ph7_vm *pVm = pCtx->pVm;` |
|   151 |  5237 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  5238 | `	ph7_class_instance *pObj;` |
|   151 |  5239 | `	ph7_class_instance *pHold = 0;` |
|     - |  5240 | `	ph7_class *pAggCls,*pRecCls,*pTravCls;` |
|   151 |  5241 | `	sxi64 iMode = RIT_LEAVES_ONLY,iFlags = 0;` |
|     - |  5242 | `	sxi32 rc;` |
|   151 |  5243 | `	if( pThis == 0 ){` |
|   ! 0 |  5244 | `		return PH7_OK;` |
|     - |  5245 | `	}` |
|     - |  5246 | `	/*` |
|     - |  5247 | `	 * php's ZPP here is "o\|ll" -- a bare OBJECT -- while the stub declares` |
|     - |  5248 | ``	 * `Traversable $iterator`, so the declared type and the refusal text disagree`` |
|     - |  5249 | `	 * (rule 41's neighbour). The spec row carries the declared type for Reflection` |
|     - |  5250 | `	 * and this body words both refusals, which is why the method sits on` |
|     - |  5251 | `	 * azSelfChecked[].` |
|     - |  5252 | `	 */` |
|   151 |  5253 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 \|\| apArg[0]->x.pOther == 0 ){` |
|     - |  5254 | `		char zGiven[64];` |
|     5 |  5255 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  5256 | `			"RecursiveIteratorIterator::__construct(): Argument #1 ($iterator) "` |
|     - |  5257 | `			"must be of type object, %s given",` |
|     2 |  5258 | `			nArg < 1 ? "none" : VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|     - |  5259 | `	}` |
|   149 |  5260 | `	if( nArg > 1 ){` |
|    82 |  5261 | `		rc = PH7_IntArgResolve(pCtx,apArg[1],"RecursiveIteratorIterator::__construct",2,"$mode","int",&iMode);` |
|    82 |  5262 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  5263 | `			return rc;` |
|     - |  5264 | `		}` |
|    40 |  5265 | `	}` |
|   149 |  5266 | `	if( nArg > 2 ){` |
|    63 |  5267 | `		rc = PH7_IntArgResolve(pCtx,apArg[2],"RecursiveIteratorIterator::__construct",3,"$flags","int",&iFlags);` |
|    63 |  5268 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  5269 | `			return rc;` |
|     - |  5270 | `		}` |
|    31 |  5271 | `	}` |
|   149 |  5272 | `	pObj = (ph7_class_instance *)apArg[0]->x.pOther;` |
|   149 |  5273 | `	pAggCls = PH7_VmExtractClass(pVm,"IteratorAggregate",sizeof("IteratorAggregate")-1,FALSE,0);` |
|   149 |  5274 | `	pRecCls = PH7_VmExtractClass(pVm,"RecursiveIterator",sizeof("RecursiveIterator")-1,FALSE,0);` |
|   149 |  5275 | `	pTravCls = PH7_VmExtractClass(pVm,"Traversable",sizeof("Traversable")-1,FALSE,0);` |
|     - |  5276 | `	/*` |
|     - |  5277 | `	 * php's spl_get_iterator_from_aggregate: ONE getIterator() and no more. An` |
|     - |  5278 | `	 * IteratorAggregate whose getIterator() answers another aggregate therefore` |
|     - |  5279 | `	 * fails the RecursiveIterator test below rather than being unwrapped further.` |
|     - |  5280 | `	 */` |
|   149 |  5281 | `	if( pAggCls && PH7_VmInstanceOf(pObj->pClass,pAggCls) ){` |
|     3 |  5282 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pObj->pClass,"getIterator",` |
|     - |  5283 | `			sizeof("getIterator")-1);` |
|     - |  5284 | `		ph7_value sInner;` |
|     3 |  5285 | `		PH7_MemObjInit(pVm,&sInner);` |
|     3 |  5286 | `		rc = pMethod ? PH7_VmCallClassMethod(pVm,pObj,pMethod,&sInner,0,0) : SXRET_OK;` |
|     3 |  5287 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  5288 | `			PH7_MemObjRelease(&sInner);` |
|   ! 0 |  5289 | `			return rc;` |
|     - |  5290 | `		}` |
|     2 |  5291 | `		if( (sInner.iFlags & MEMOBJ_OBJ) == 0 \|\| sInner.x.pOther == 0` |
|     3 |  5292 | `		 \|\| (pTravCls && !PH7_VmInstanceOf(((ph7_class_instance *)sInner.x.pOther)->pClass,pTravCls)) ){` |
|   ! 0 |  5293 | `			SyString *pName = &pObj->pClass->sName;` |
|   ! 0 |  5294 | `			PH7_MemObjRelease(&sInner);` |
|   ! 0 |  5295 | `			return PH7_VmThrowException(pCtx,"LogicException",` |
|   ! 0 |  5296 | `				"%z::getIterator() must return an object that implements Traversable",pName);` |
|     - |  5297 | `		}` |
|     3 |  5298 | `		pObj = (ph7_class_instance *)sInner.x.pOther;` |
|     3 |  5299 | `		pObj->iRef++;` |
|     3 |  5300 | `		PH7_MemObjRelease(&sInner);` |
|     3 |  5301 | `		pHold = pObj;` |
|     1 |  5302 | `	}` |
|   149 |  5303 | `	if( pRecCls == 0 \|\| !PH7_VmInstanceOf(pObj->pClass,pRecCls) ){` |
|     3 |  5304 | `		if( pHold ){` |
|   ! 0 |  5305 | `			PH7_ClassInstanceUnref(pHold);` |
|   ! 0 |  5306 | `		}` |
|     - |  5307 | `		/* php refuses here rather than from the declared type, so a plain Iterator` |
|     - |  5308 | `		 * gets this sentence and not a TypeError. */` |
|     3 |  5309 | `		return PH7_VmThrowException(pCtx,"InvalidArgumentException",` |
|     - |  5310 | `			"An instance of RecursiveIterator or IteratorAggregate creating it is required");` |
|     - |  5311 | `	}` |
|   147 |  5312 | `	RitClear(pVm,pThis);` |
|   147 |  5313 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_LVL,0);` |
|   147 |  5314 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_MD,iMode);` |
|   147 |  5315 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_FL,iFlags);` |
|   147 |  5316 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_MX,-1);` |
|   147 |  5317 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_II,0);` |
|     - |  5318 | `	{` |
|     - |  5319 | `		ph7_value sObj;` |
|   147 |  5320 | `		PH7_MemObjInit(pVm,&sObj);` |
|   147 |  5321 | `		sObj.x.pOther = pObj;` |
|   147 |  5322 | `		MemObjSetType(&sObj,MEMOBJ_OBJ);` |
|   147 |  5323 | `		RitPut(pVm,pThis,RIT_ST,0,&sObj);` |
|   147 |  5324 | `		sObj.x.pOther = 0;` |
|   147 |  5325 | `		MemObjSetType(&sObj,MEMOBJ_NULL);` |
|   147 |  5326 | `		PH7_MemObjRelease(&sObj);` |
|     - |  5327 | `	}` |
|   147 |  5328 | `	RitSetState(pVm,pThis,0,RS_START);` |
|     - |  5329 | `	/* Level 0 exists from HERE, which is what makes getDepth() answer 0 and` |
|     - |  5330 | `	 * getSubIterator() answer the root before any rewind(). */` |
|   147 |  5331 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_RD,1);` |
|   147 |  5332 | `	if( pHold ){` |
|     3 |  5333 | `		PH7_ClassInstanceUnref(pHold);` |
|     1 |  5334 | `	}` |
|    72 |  5335 | `	SXUNUSED(nArg);` |
|   147 |  5336 | `	return PH7_OK;` |
|    77 |  5337 | `}` |
|   106 |  5338 | `static int vm_builtin_RecursiveIteratorIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  5339 | `{` |
|   109 |  5340 | `	ph7_vm *pVm = pCtx->pVm;` |
|   109 |  5341 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  5342 | `	ph7_class_instance *pRoot;` |
|     - |  5343 | `	sxi32 rc;` |
|    53 |  5344 | `	SXUNUSED(nArg);` |
|    53 |  5345 | `	SXUNUSED(apArg);` |
|   109 |  5346 | `	if( !RitReady(pThis) ){` |
|     3 |  5347 | `		return RitNotReady(pCtx);` |
|     - |  5348 | `	}` |
|     - |  5349 | `	/* php pops the level FIRST and calls endChildren after, so the hook reports the` |
|     - |  5350 | `	 * depth it has landed on -- the opposite order from the traversal's own pop. */` |
|   107 |  5351 | `	while( RitInt(pThis,RIT_LVL) > 0 ){` |
|   ! 0 |  5352 | `		RitPop(pVm,pThis);` |
|   ! 0 |  5353 | `		rc = RitHook(pCtx,"endChildren",sizeof("endChildren")-1,0,FALSE,0);` |
|   ! 0 |  5354 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  5355 | `			return rc;` |
|     - |  5356 | `		}` |
|   ! 0 |  5357 | `	}` |
|   107 |  5358 | `	RitSetState(pVm,pThis,0,RS_START);` |
|   107 |  5359 | `	pRoot = RitSub(pVm,pThis,0);` |
|   107 |  5360 | `	rc = RitCall(pCtx,pRoot,"rewind",sizeof("rewind")-1,0,FALSE,0);` |
|   107 |  5361 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  5362 | `		return rc;` |
|     - |  5363 | `	}` |
|     - |  5364 | `	/* php's in_iteration latch: a second rewind() does NOT re-announce the` |
|     - |  5365 | `	 * iteration, which is the only reason the flag exists. */` |
|   107 |  5366 | `	if( !RitInt(pThis,RIT_II) ){` |
|   105 |  5367 | `		rc = RitHook(pCtx,"beginIteration",sizeof("beginIteration")-1,0,FALSE,0);` |
|   105 |  5368 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  5369 | `			PH7_NativeSetAttrInt(pVm,pThis,RIT_II,1);` |
|   ! 0 |  5370 | `			return rc;` |
|     - |  5371 | `		}` |
|    51 |  5372 | `	}` |
|   107 |  5373 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_II,1);` |
|   107 |  5374 | `	return RitMoveForward(pCtx);` |
|    56 |  5375 | `}` |
|   404 |  5376 | `static int vm_builtin_RecursiveIteratorIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  5377 | `{` |
|   407 |  5378 | `	int bValid = FALSE;` |
|     - |  5379 | `	sxi32 rc;` |
|   202 |  5380 | `	SXUNUSED(nArg);` |
|   202 |  5381 | `	SXUNUSED(apArg);` |
|   407 |  5382 | `	if( !RitReady(PH7_ContextThis(pCtx)) ){` |
|     3 |  5383 | `		return RitNotReady(pCtx);` |
|     - |  5384 | `	}` |
|   405 |  5385 | `	rc = RitValidEx(pCtx,&bValid);` |
|   405 |  5386 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  5387 | `		return rc;` |
|     - |  5388 | `	}` |
|   405 |  5389 | `	ph7_result_bool(pCtx,bValid);` |
|   405 |  5390 | `	return PH7_OK;` |
|   205 |  5391 | `}` |
|     - |  5392 | `/* current() and key() read the CURRENT LEVEL live -- php keeps no cache here, the` |
|     - |  5393 | ` * one place the recursive iterator differs from every dual iterator (rule 43). */` |
|   462 |  5394 | `static sxi32 RitCurrentLevelCall(ph7_context *pCtx,const char *zName,sxu32 nName)` |
|     3 |  5395 | `{` |
|   465 |  5396 | `	ph7_vm *pVm = pCtx->pVm;` |
|   465 |  5397 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  5398 | `	ph7_class_instance *pSub;` |
|     - |  5399 | `	ph7_value sRes;` |
|     - |  5400 | `	sxi32 rc;` |
|   465 |  5401 | `	if( !RitReady(pThis) ){` |
|   ! 0 |  5402 | `		return RitNotReady(pCtx);` |
|     - |  5403 | `	}` |
|   465 |  5404 | `	pSub = RitSub(pVm,pThis,RitInt(pThis,RIT_LVL));` |
|   465 |  5405 | `	if( pSub == 0 ){` |
|   ! 0 |  5406 | `		ph7_result_null(pCtx);` |
|   ! 0 |  5407 | `		return PH7_OK;` |
|     - |  5408 | `	}` |
|   465 |  5409 | `	PH7_MemObjInit(pVm,&sRes);` |
|   465 |  5410 | `	rc = RitCall(pCtx,pSub,zName,nName,&sRes,FALSE,0);` |
|   465 |  5411 | `	if( rc == SXRET_OK ){` |
|   465 |  5412 | `		ph7_result_value(pCtx,&sRes);` |
|   231 |  5413 | `	}` |
|   465 |  5414 | `	PH7_MemObjRelease(&sRes);` |
|   465 |  5415 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|   234 |  5416 | `}` |
|   180 |  5417 | `static int vm_builtin_RecursiveIteratorIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  5418 | `{` |
|    90 |  5419 | `	SXUNUSED(nArg);` |
|    90 |  5420 | `	SXUNUSED(apArg);` |
|   182 |  5421 | `	return RitCurrentLevelCall(pCtx,"key",sizeof("key")-1);` |
|     2 |  5422 | `}` |
|   220 |  5423 | `static int vm_builtin_RecursiveIteratorIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  5424 | `{` |
|   110 |  5425 | `	SXUNUSED(nArg);` |
|   110 |  5426 | `	SXUNUSED(apArg);` |
|   223 |  5427 | `	return RitCurrentLevelCall(pCtx,"current",sizeof("current")-1);` |
|     3 |  5428 | `}` |
|   316 |  5429 | `static int vm_builtin_RecursiveIteratorIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  5430 | `{` |
|   158 |  5431 | `	SXUNUSED(nArg);` |
|   158 |  5432 | `	SXUNUSED(apArg);` |
|   319 |  5433 | `	if( !RitReady(PH7_ContextThis(pCtx)) ){` |
|   ! 0 |  5434 | `		return RitNotReady(pCtx);` |
|     - |  5435 | `	}` |
|   319 |  5436 | `	return RitMoveForward(pCtx);` |
|   161 |  5437 | `}` |
|   124 |  5438 | `static int vm_builtin_RecursiveIteratorIterator_getDepth(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  5439 | `{` |
|   126 |  5440 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    62 |  5441 | `	SXUNUSED(nArg);` |
|    62 |  5442 | `	SXUNUSED(apArg);` |
|   126 |  5443 | `	if( !RitReady(pThis) ){` |
|     3 |  5444 | `		return RitNotReady(pCtx);` |
|     - |  5445 | `	}` |
|   124 |  5446 | `	ph7_result_int64(pCtx,(ph7_int64)RitInt(pThis,RIT_LVL));` |
|   124 |  5447 | `	return PH7_OK;` |
|    64 |  5448 | `}` |
|    20 |  5449 | `static int vm_builtin_RecursiveIteratorIterator_getSubIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5450 | `{` |
|    21 |  5451 | `	ph7_vm *pVm = pCtx->pVm;` |
|    21 |  5452 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  5453 | `	ph7_class_instance *pSub;` |
|     - |  5454 | `	int iLevel;` |
|    21 |  5455 | `	if( !RitReady(pThis) ){` |
|   ! 0 |  5456 | `		return RitNotReady(pCtx);` |
|     - |  5457 | `	}` |
|    21 |  5458 | `	iLevel = RitInt(pThis,RIT_LVL);` |
|    21 |  5459 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0 ){` |
|     9 |  5460 | `		sxi64 iWant = 0;` |
|     9 |  5461 | `		sxi32 rc = PH7_IntArgResolve(pCtx,apArg[0],"RecursiveIteratorIterator::getSubIterator",` |
|     - |  5462 | `			1,"$level","?int",&iWant);` |
|     9 |  5463 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  5464 | `			return rc;` |
|     - |  5465 | `		}` |
|     9 |  5466 | `		if( iWant < 0 \|\| iWant > (sxi64)iLevel ){` |
|     7 |  5467 | `			ph7_result_null(pCtx);` |
|     7 |  5468 | `			return PH7_OK;` |
|     - |  5469 | `		}` |
|     3 |  5470 | `		iLevel = (int)iWant;` |
|     1 |  5471 | `	}` |
|    15 |  5472 | `	pSub = RitSub(pVm,pThis,iLevel);` |
|    15 |  5473 | `	if( pSub ){` |
|    15 |  5474 | `		SplResultBorrowed(pCtx,pSub);` |
|     8 |  5475 | `	}else{` |
|   ! 0 |  5476 | `		ph7_result_null(pCtx);` |
|     - |  5477 | `	}` |
|    15 |  5478 | `	return PH7_OK;` |
|    11 |  5479 | `}` |
|     8 |  5480 | `static int vm_builtin_RecursiveIteratorIterator_getInnerIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5481 | `{` |
|     9 |  5482 | `	ph7_vm *pVm = pCtx->pVm;` |
|     9 |  5483 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  5484 | `	ph7_class_instance *pSub;` |
|     4 |  5485 | `	SXUNUSED(nArg);` |
|     4 |  5486 | `	SXUNUSED(apArg);` |
|     9 |  5487 | `	if( !RitReady(pThis) ){` |
|     3 |  5488 | `		return RitNotReady(pCtx);` |
|     - |  5489 | `	}` |
|     7 |  5490 | `	pSub = RitSub(pVm,pThis,RitInt(pThis,RIT_LVL));` |
|     7 |  5491 | `	if( pSub ){` |
|     7 |  5492 | `		SplResultBorrowed(pCtx,pSub);` |
|     4 |  5493 | `	}else{` |
|   ! 0 |  5494 | `		ph7_result_null(pCtx);` |
|     - |  5495 | `	}` |
|     7 |  5496 | `	return PH7_OK;` |
|     5 |  5497 | `}` |
|     - |  5498 | `/* The five hooks php declares with empty bodies. They exist to be OVERRIDDEN and` |
|     - |  5499 | ` * to be reachable through parent:: from an override. */` |
|   740 |  5500 | `static int vm_builtin_RecursiveIteratorIterator_nop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  5501 | `{` |
|   370 |  5502 | `	SXUNUSED(nArg);` |
|   370 |  5503 | `	SXUNUSED(apArg);` |
|   743 |  5504 | `	if( !RitReady(PH7_ContextThis(pCtx)) ){` |
|   ! 0 |  5505 | `		return RitNotReady(pCtx);` |
|     - |  5506 | `	}` |
|   743 |  5507 | `	return PH7_OK;` |
|   373 |  5508 | `}` |
|     - |  5509 | `/* php's callHasChildren/callGetChildren ask the CURRENT LEVEL's iterator, which` |
|     - |  5510 | ` * is what makes them the documented interception point for both. */` |
|   396 |  5511 | `static int vm_builtin_RecursiveIteratorIterator_callHasChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  5512 | `{` |
|   399 |  5513 | `	ph7_vm *pVm = pCtx->pVm;` |
|   399 |  5514 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  5515 | `	ph7_class_instance *pSub;` |
|     - |  5516 | `	ph7_value sRes;` |
|     - |  5517 | `	sxi32 rc;` |
|   198 |  5518 | `	SXUNUSED(nArg);` |
|   198 |  5519 | `	SXUNUSED(apArg);` |
|   399 |  5520 | `	if( !RitReady(pThis) ){` |
|     3 |  5521 | `		return RitNotReady(pCtx);` |
|     - |  5522 | `	}` |
|   397 |  5523 | `	pSub = RitSub(pVm,pThis,RitInt(pThis,RIT_LVL));` |
|   397 |  5524 | `	if( pSub == 0 ){` |
|   ! 0 |  5525 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  5526 | `		return PH7_OK;` |
|     - |  5527 | `	}` |
|   397 |  5528 | `	PH7_MemObjInit(pVm,&sRes);` |
|   397 |  5529 | `	rc = RitCall(pCtx,pSub,"hasChildren",sizeof("hasChildren")-1,&sRes,FALSE,0);` |
|   397 |  5530 | `	if( rc == SXRET_OK ){` |
|   397 |  5531 | `		ph7_result_bool(pCtx,ph7_value_to_bool(&sRes));` |
|   197 |  5532 | `	}` |
|   397 |  5533 | `	PH7_MemObjRelease(&sRes);` |
|   397 |  5534 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|   201 |  5535 | `}` |
|   132 |  5536 | `static int vm_builtin_RecursiveIteratorIterator_callGetChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  5537 | `{` |
|   135 |  5538 | `	ph7_vm *pVm = pCtx->pVm;` |
|   135 |  5539 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  5540 | `	ph7_class_instance *pSub;` |
|     - |  5541 | `	ph7_value sRes;` |
|     - |  5542 | `	sxi32 rc;` |
|    66 |  5543 | `	SXUNUSED(nArg);` |
|    66 |  5544 | `	SXUNUSED(apArg);` |
|   135 |  5545 | `	if( !RitReady(pThis) ){` |
|   ! 0 |  5546 | `		return RitNotReady(pCtx);` |
|     - |  5547 | `	}` |
|   135 |  5548 | `	pSub = RitSub(pVm,pThis,RitInt(pThis,RIT_LVL));` |
|   135 |  5549 | `	if( pSub == 0 ){` |
|   ! 0 |  5550 | `		ph7_result_null(pCtx);` |
|   ! 0 |  5551 | `		return PH7_OK;` |
|     - |  5552 | `	}` |
|   135 |  5553 | `	PH7_MemObjInit(pVm,&sRes);` |
|   135 |  5554 | `	rc = RitCall(pCtx,pSub,"getChildren",sizeof("getChildren")-1,&sRes,FALSE,0);` |
|   135 |  5555 | `	if( rc == SXRET_OK ){` |
|   129 |  5556 | `		ph7_result_value(pCtx,&sRes);` |
|    63 |  5557 | `	}` |
|   135 |  5558 | `	PH7_MemObjRelease(&sRes);` |
|   135 |  5559 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    69 |  5560 | `}` |
|    18 |  5561 | `static int vm_builtin_RecursiveIteratorIterator_setMaxDepth(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5562 | `{` |
|    19 |  5563 | `	ph7_vm *pVm = pCtx->pVm;` |
|    19 |  5564 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    19 |  5565 | `	sxi64 iMax = -1;` |
|    19 |  5566 | `	if( !RitReady(pThis) ){` |
|   ! 0 |  5567 | `		return RitNotReady(pCtx);` |
|     - |  5568 | `	}` |
|    19 |  5569 | `	if( nArg > 0 ){` |
|    17 |  5570 | `		sxi32 rc = PH7_IntArgResolve(pCtx,apArg[0],"RecursiveIteratorIterator::setMaxDepth",` |
|     - |  5571 | `			1,"$maxDepth","int",&iMax);` |
|    17 |  5572 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  5573 | `			return rc;` |
|     - |  5574 | `		}` |
|     8 |  5575 | `	}` |
|    19 |  5576 | `	if( iMax < -1 ){` |
|     3 |  5577 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  5578 | `			"RecursiveIteratorIterator::setMaxDepth(): Argument #1 ($maxDepth) "` |
|     - |  5579 | `			"must be greater than or equal to -1");` |
|     - |  5580 | `	}` |
|    17 |  5581 | `	if( iMax > SXI32_HIGH ){` |
|   ! 0 |  5582 | `		iMax = SXI32_HIGH;   /* php clamps to INT_MAX; max_depth is an int there */` |
|   ! 0 |  5583 | `	}` |
|    17 |  5584 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_MX,iMax);` |
|    17 |  5585 | `	return PH7_OK;` |
|    10 |  5586 | `}` |
|    10 |  5587 | `static int vm_builtin_RecursiveIteratorIterator_getMaxDepth(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5588 | `{` |
|    11 |  5589 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  5590 | `	int iMax;` |
|     5 |  5591 | `	SXUNUSED(nArg);` |
|     5 |  5592 | `	SXUNUSED(apArg);` |
|    11 |  5593 | `	if( !RitReady(pThis) ){` |
|   ! 0 |  5594 | `		return RitNotReady(pCtx);` |
|     - |  5595 | `	}` |
|    11 |  5596 | `	iMax = RitInt(pThis,RIT_MX);` |
|    11 |  5597 | `	if( iMax == -1 ){` |
|     5 |  5598 | ``		ph7_result_bool(pCtx,0);   /* php's `int\|false`: false means "any depth" */`` |
|     3 |  5599 | `	}else{` |
|     7 |  5600 | `		ph7_result_int64(pCtx,(ph7_int64)iMax);` |
|     - |  5601 | `	}` |
|    11 |  5602 | `	return PH7_OK;` |
|     6 |  5603 | `}` |
|     - |  5604 | `/*` |
|     - |  5605 | ` * ---------------------------------------------------------------------------` |
|     - |  5606 | ` * RecursiveTreeIterator: the RecursiveIteratorIterator that draws the tree.` |
|     - |  5607 | ` *` |
|     - |  5608 | ` * It is the same traversal with a STRING built around each element, and the` |
|     - |  5609 | ` * drawing is why php wraps the iterator it is handed in a` |
|     - |  5610 | ` * RecursiveCachingIterator: the ASCII branches need to know whether a level has` |
|     - |  5611 | ` * a NEXT element, and hasNext() is the one question only the caching decorator` |
|     - |  5612 | ` * answers. So the sub-iterator at every level here is a` |
|     - |  5613 | ` * RecursiveCachingIterator, getSubIterator()/getInnerIterator() report one, and` |
|     - |  5614 | `` * `$cachingIteratorFlags` is what that wrapper is built with — CATCH_GET_CHILD`` |
|     - |  5615 | ` * when the caller says nothing, and EXACTLY what the caller says otherwise.` |
|     - |  5616 | ` *` |
|     - |  5617 | ` * The prefix is six parts: a fixed LEFT, one MID per level above this one` |
|     - |  5618 | ` * (chosen by whether that level has a next element), one END for this level` |
|     - |  5619 | ` * (chosen the same way) and a fixed RIGHT. current() is prefix + entry + postfix` |
|     - |  5620 | ` * and key() is prefix + key + postfix, each bypassable through its own flag —` |
|     - |  5621 | ` * and those flags live in the SAME word as RecursiveIteratorIterator's` |
|     - |  5622 | ` * CATCH_GET_CHILD, which is why php declares that constant on both classes.` |
|     - |  5623 | ` */` |
|     - |  5624 | `#define RTI_PFX "__pfx"   /* php's prefix[6] */` |
|     - |  5625 | `#define RTI_PST "__pst"   /* php's postfix */` |
|     - |  5626 |  |
|     - |  5627 | `#define RTIT_BYPASS_CURRENT     4` |
|     - |  5628 | `#define RTIT_BYPASS_KEY         8` |
|     - |  5629 | `#define RTIT_PREFIX_LEFT        0` |
|     - |  5630 | `#define RTIT_PREFIX_MID_HAS_NEXT 1` |
|     - |  5631 | `#define RTIT_PREFIX_MID_LAST    2` |
|     - |  5632 | `#define RTIT_PREFIX_END_HAS_NEXT 3` |
|     - |  5633 | `#define RTIT_PREFIX_END_LAST    4` |
|     - |  5634 | `#define RTIT_PREFIX_RIGHT       5` |
|     - |  5635 |  |
|     - |  5636 | ``/* php's `object->prefix[N]` defaults, set in the constructor. */`` |
|     - |  5637 | `static const char * const azRtiPrefix[] = { "", "\| ", "  ", "\|-", "\\-", "" };` |
|     - |  5638 |  |
|   466 |  5639 | `static ph7_value * RtiPrefixSlot(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  5640 | `{` |
|   467 |  5641 | `	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,RTI_PFX) : 0;` |
|   467 |  5642 | `	if( pSlot == 0 ){` |
|   ! 0 |  5643 | `		return 0;` |
|     - |  5644 | `	}` |
|   467 |  5645 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    47 |  5646 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|   ! 0 |  5647 | `			return 0;` |
|     - |  5648 | `		}` |
|    23 |  5649 | `	}` |
|   467 |  5650 | `	if( PH7_HashmapCowSeparate(pVm,pSlot) == 0 ){` |
|   ! 0 |  5651 | `		return 0;` |
|     - |  5652 | `	}` |
|     - |  5653 | `	/* Re-ask: the separation can MOVE the memobj pool this slot lives in` |
|     - |  5654 | `	 * (SplStoreSlot explains it). */` |
|   467 |  5655 | `	return PH7_NativeAttr(pThis,RTI_PFX);` |
|   234 |  5656 | `}` |
|     - |  5657 | `/* One prefix part, appended to pOut. A part nothing has written is php's own` |
|     - |  5658 | ` * default rather than the empty string. */` |
|   404 |  5659 | `static void RtiAppendPart(ph7_vm *pVm,ph7_class_instance *pThis,int iPart,ph7_value *pOut)` |
|     1 |  5660 | `{` |
|   405 |  5661 | `	ph7_value *pSlot = RtiPrefixSlot(pVm,pThis);` |
|   405 |  5662 | `	ph7_hashmap_node *pNode = 0;` |
|   405 |  5663 | `	ph7_value *pVal = 0;` |
|     - |  5664 | `	const char *zTxt;` |
|     - |  5665 | `	int nTxt;` |
|   405 |  5666 | `	if( pSlot && HashmapLookupIntKey((ph7_hashmap *)pSlot->x.pOther,(sxi64)iPart,&pNode) == SXRET_OK ){` |
|   405 |  5667 | `		pVal = HashmapExtractNodeValue(pNode);` |
|   202 |  5668 | `	}` |
|   405 |  5669 | `	if( pVal == 0 ){` |
|   ! 0 |  5670 | `		return;` |
|     - |  5671 | `	}` |
|   405 |  5672 | `	zTxt = ph7_value_to_string(pVal,&nTxt);` |
|   405 |  5673 | `	if( nTxt > 0 ){` |
|   205 |  5674 | `		PH7_MemObjStringAppend(pOut,zTxt,(sxu32)nTxt);` |
|   102 |  5675 | `	}` |
|   203 |  5676 | `}` |
|     - |  5677 | ``/* php's `hasnext` on one level's sub-iterator; a level with no answer draws`` |
|     - |  5678 | ` * nothing at all. */` |
|   180 |  5679 | `static sxi32 RtiLevelHasNext(ph7_context *pCtx,int iLevel,int *pbHas,int *pbAnswered)` |
|     1 |  5680 | `{` |
|   181 |  5681 | `	ph7_vm *pVm = pCtx->pVm;` |
|   181 |  5682 | `	ph7_class_instance *pSub = RitSub(pVm,PH7_ContextThis(pCtx),iLevel);` |
|   181 |  5683 | `	ph7_class_method *pMethod = pSub` |
|   180 |  5684 | `		? PH7_ClassExtractMethod(pSub->pClass,"hasNext",sizeof("hasNext")-1) : 0;` |
|     - |  5685 | `	ph7_value sRes;` |
|     - |  5686 | `	sxi32 rc;` |
|   181 |  5687 | `	*pbHas = 0;` |
|   181 |  5688 | `	*pbAnswered = 0;` |
|   181 |  5689 | `	if( pMethod == 0 ){` |
|   ! 0 |  5690 | `		return SXRET_OK;` |
|     - |  5691 | `	}` |
|   181 |  5692 | `	PH7_MemObjInit(pVm,&sRes);` |
|   181 |  5693 | `	rc = PH7_VmCallClassMethod(pVm,pSub,pMethod,&sRes,0,0);` |
|   181 |  5694 | `	if( rc == SXRET_OK ){` |
|   181 |  5695 | `		PH7_MemObjToBool(&sRes);          /* a STATUS, not the answer */` |
|   181 |  5696 | `		*pbHas = sRes.x.iVal != 0;` |
|   181 |  5697 | `		*pbAnswered = 1;` |
|    90 |  5698 | `	}` |
|   181 |  5699 | `	PH7_MemObjRelease(&sRes);` |
|   181 |  5700 | `	return rc;` |
|    91 |  5701 | `}` |
|   112 |  5702 | `static sxi32 RtiBuildPrefix(ph7_context *pCtx,ph7_value *pOut)` |
|     1 |  5703 | `{` |
|   113 |  5704 | `	ph7_vm *pVm = pCtx->pVm;` |
|   113 |  5705 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   113 |  5706 | `	int iLevel = RitInt(pThis,RIT_LVL);` |
|     - |  5707 | `	int i,bHas,bAnswered;` |
|     - |  5708 | `	sxi32 rc;` |
|   113 |  5709 | `	RtiAppendPart(pVm,pThis,RTIT_PREFIX_LEFT,pOut);` |
|   181 |  5710 | `	for( i = 0 ; i < iLevel ; ++i ){` |
|    69 |  5711 | `		rc = RtiLevelHasNext(pCtx,i,&bHas,&bAnswered);` |
|    69 |  5712 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  5713 | `			return rc;` |
|     - |  5714 | `		}` |
|    69 |  5715 | `		if( bAnswered ){` |
|    69 |  5716 | `			RtiAppendPart(pVm,pThis,bHas ? RTIT_PREFIX_MID_HAS_NEXT : RTIT_PREFIX_MID_LAST,pOut);` |
|    34 |  5717 | `		}` |
|    35 |  5718 | `	}` |
|   113 |  5719 | `	rc = RtiLevelHasNext(pCtx,iLevel,&bHas,&bAnswered);` |
|   113 |  5720 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  5721 | `		return rc;` |
|     - |  5722 | `	}` |
|   113 |  5723 | `	if( bAnswered ){` |
|   113 |  5724 | `		RtiAppendPart(pVm,pThis,bHas ? RTIT_PREFIX_END_HAS_NEXT : RTIT_PREFIX_END_LAST,pOut);` |
|    56 |  5725 | `	}` |
|   113 |  5726 | `	RtiAppendPart(pVm,pThis,RTIT_PREFIX_RIGHT,pOut);` |
|   113 |  5727 | `	return SXRET_OK;` |
|    57 |  5728 | `}` |
|     - |  5729 | `/*` |
|     - |  5730 | ` * php's get_entry: the CACHED current() of this level's caching iterator, as a` |
|     - |  5731 | ` * string. An ARRAY is the word "Array" and says nothing while doing it — php` |
|     - |  5732 | ` * never runs a cast here — and an object with no __toString still raises.` |
|     - |  5733 | ` */` |
|    96 |  5734 | `static sxi32 RtiBuildEntry(ph7_context *pCtx,ph7_value *pOut)` |
|     1 |  5735 | `{` |
|    97 |  5736 | `	ph7_vm *pVm = pCtx->pVm;` |
|    97 |  5737 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    97 |  5738 | `	ph7_class_instance *pSub = RitSub(pVm,pThis,RitInt(pThis,RIT_LVL));` |
|    97 |  5739 | `	ph7_class_method *pMethod = pSub` |
|    96 |  5740 | `		? PH7_ClassExtractMethod(pSub->pClass,"current",sizeof("current")-1) : 0;` |
|     - |  5741 | `	ph7_value sRes;` |
|     - |  5742 | `	sxi32 rc;` |
|    97 |  5743 | `	if( pMethod == 0 ){` |
|   ! 0 |  5744 | `		return SXRET_OK;` |
|     - |  5745 | `	}` |
|    97 |  5746 | `	PH7_MemObjInit(pVm,&sRes);` |
|    97 |  5747 | `	rc = PH7_VmCallClassMethod(pVm,pSub,pMethod,&sRes,0,0);` |
|    97 |  5748 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  5749 | `		PH7_MemObjRelease(&sRes);` |
|   ! 0 |  5750 | `		return rc;` |
|     - |  5751 | `	}` |
|    97 |  5752 | `	if( sRes.iFlags & MEMOBJ_HASHMAP ){` |
|    35 |  5753 | `		PH7_MemObjStringAppend(pOut,"Array",sizeof("Array")-1);` |
|    80 |  5754 | `	}else if( (sRes.iFlags & MEMOBJ_NULL) == 0 ){` |
|    61 |  5755 | `		rc = PH7_MemObjToStringUV(&sRes);` |
|    61 |  5756 | `		if( rc == SXRET_OK ){` |
|     - |  5757 | `			int nTxt;` |
|    59 |  5758 | `			const char *zTxt = ph7_value_to_string(&sRes,&nTxt);` |
|    59 |  5759 | `			if( nTxt > 0 ){` |
|    59 |  5760 | `				PH7_MemObjStringAppend(pOut,zTxt,(sxu32)nTxt);` |
|    29 |  5761 | `			}` |
|    29 |  5762 | `		}` |
|    30 |  5763 | `	}` |
|    97 |  5764 | `	PH7_MemObjRelease(&sRes);` |
|    97 |  5765 | `	return rc;` |
|    49 |  5766 | `}` |
|    98 |  5767 | `static void RtiAppendPostfix(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)` |
|     1 |  5768 | `{` |
|    99 |  5769 | `	ph7_value *pSlot = PH7_NativeAttr(pThis,RTI_PST);` |
|     - |  5770 | `	const char *zTxt;` |
|     - |  5771 | `	int nTxt;` |
|    99 |  5772 | `	if( pSlot == 0 ){` |
|   ! 0 |  5773 | `		return;` |
|     - |  5774 | `	}` |
|    99 |  5775 | `	zTxt = ph7_value_to_string(pSlot,&nTxt);` |
|    99 |  5776 | `	if( nTxt > 0 ){` |
|    13 |  5777 | `		PH7_MemObjStringAppend(pOut,zTxt,(sxu32)nTxt);` |
|     6 |  5778 | `	}` |
|    49 |  5779 | `	SXUNUSED(pVm);` |
|    50 |  5780 | `}` |
|    50 |  5781 | `static int vm_builtin_RecursiveTreeIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5782 | `{` |
|    51 |  5783 | `	ph7_vm *pVm = pCtx->pVm;` |
|    51 |  5784 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  5785 | `	ph7_class *pCls;` |
|     - |  5786 | `	ph7_class_method *pCons;` |
|     - |  5787 | `	ph7_class_instance *pWrap;` |
|     - |  5788 | `	ph7_value sFlags,sMode,sCache,sSrc,sWrap,*apCtor[3],*pSlot;` |
|    51 |  5789 | `	sxi64 iFlags = RTIT_BYPASS_KEY, iCache = RIT_CATCH_GET_CHILD, iMode = RIT_SELF_FIRST;` |
|     - |  5790 | `	int i;` |
|     - |  5791 | `	sxi32 rc;` |
|    51 |  5792 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|   ! 0 |  5793 | `		return PH7_OK;` |
|     - |  5794 | `	}` |
|     - |  5795 | `	/* php's ZPP is "o\|lzl" — a bare OBJECT — while the stub declares the union` |
|     - |  5796 | `` 	 * this row carries for Reflection, so the type screen stands aside (the `~` `` |
|     - |  5797 | `	 * marker) and the refusal is worded here. */` |
|    51 |  5798 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 \|\| apArg[0]->x.pOther == 0 ){` |
|     - |  5799 | `		char zGiven[64];` |
|     7 |  5800 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  5801 | `			"RecursiveTreeIterator::__construct(): Argument #1 ($iterator) "` |
|     - |  5802 | `			"must be of type object, %s given",` |
|     2 |  5803 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|     - |  5804 | `	}` |
|    47 |  5805 | `	if( nArg > 1 ){` |
|    13 |  5806 | `		iFlags = ph7_value_to_int64(apArg[1]);` |
|     6 |  5807 | `	}` |
|    47 |  5808 | `	if( nArg > 2 ){` |
|     - |  5809 | `		/* A caller's value REPLACES the CATCH_GET_CHILD default rather than joining` |
|     - |  5810 | `` 		 * it — `new RecursiveTreeIterator($it, 8, CachingIterator::TOSTRING_USE_KEY)` `` |
|     - |  5811 | `		 * builds a wrapper that does NOT catch. */` |
|     5 |  5812 | `		iCache = ph7_value_to_int64(apArg[2]);` |
|     2 |  5813 | `	}` |
|    47 |  5814 | `	if( nArg > 3 ){` |
|     3 |  5815 | `		iMode = ph7_value_to_int64(apArg[3]);` |
|     1 |  5816 | `	}` |
|     - |  5817 | `	/* The six prefix parts and the postfix php seeds every instance with. */` |
|    47 |  5818 | `	pSlot = RtiPrefixSlot(pVm,pThis);` |
|   323 |  5819 | `	for( i = 0 ; pSlot && i < (int)SX_ARRAYSIZE(azRtiPrefix) ; ++i ){` |
|     - |  5820 | `		ph7_value sPart;` |
|   277 |  5821 | `		PH7_MemObjInitFromString(pVm,&sPart,0);` |
|   277 |  5822 | `		PH7_MemObjStringAppend(&sPart,azRtiPrefix[i],(sxu32)SyStrlen(azRtiPrefix[i]));` |
|   277 |  5823 | `		ph7_array_add_intkey_elem(pSlot,i,&sPart);` |
|   277 |  5824 | `		PH7_MemObjRelease(&sPart);` |
|   139 |  5825 | `	}` |
|     - |  5826 | `	{` |
|     - |  5827 | `		ph7_value sPost;` |
|    47 |  5828 | `		PH7_MemObjInitFromString(pVm,&sPost,0);` |
|    47 |  5829 | `		DualSetSlot(pVm,pThis,RTI_PST,&sPost);` |
|    47 |  5830 | `		PH7_MemObjRelease(&sPost);` |
|     - |  5831 | `	}` |
|     - |  5832 | `	/* php wraps the iterator FIRST, so a source the wrapper refuses is reported by` |
|     - |  5833 | `	 * RecursiveCachingIterator::__construct and never reaches the traversal. */` |
|    47 |  5834 | `	pCls = PH7_VmExtractClass(pVm,"RecursiveCachingIterator",` |
|     - |  5835 | `		sizeof("RecursiveCachingIterator")-1,FALSE,0);` |
|    47 |  5836 | `	pCons = pCls ? PH7_ClassExtractMethod(pCls,"__construct",sizeof("__construct")-1) : 0;` |
|    47 |  5837 | `	if( pCons == 0 ){` |
|   ! 0 |  5838 | `		return PH7_OK;` |
|     - |  5839 | `	}` |
|    47 |  5840 | `	pWrap = PH7_NewClassInstance(pVm,pCls);` |
|    47 |  5841 | `	if( pWrap == 0 ){` |
|   ! 0 |  5842 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  5843 | `	}` |
|    47 |  5844 | `	pWrap->iRef++;` |
|     - |  5845 | `	/* php unwraps an IteratorAggregate BEFORE it wraps: the caching iterator has` |
|     - |  5846 | ``	 * to be handed the RecursiveIterator itself, so `getIterator()` runs here and`` |
|     - |  5847 | `	 * exactly once. */` |
|    47 |  5848 | `	PH7_MemObjInit(pVm,&sSrc);` |
|    47 |  5849 | `	PH7_MemObjStore(apArg[0],&sSrc);` |
|     - |  5850 | `	{` |
|    47 |  5851 | `		ph7_class *pAggCls = PH7_VmExtractClass(pVm,"IteratorAggregate",` |
|     - |  5852 | `			sizeof("IteratorAggregate")-1,FALSE,0);` |
|    47 |  5853 | `		ph7_class_instance *pSrc = (ph7_class_instance *)sSrc.x.pOther;` |
|    47 |  5854 | `		if( pAggCls && PH7_VmInstanceOf(pSrc->pClass,pAggCls) ){` |
|     3 |  5855 | `			ph7_class_method *pGet = PH7_ClassExtractMethod(pSrc->pClass,"getIterator",` |
|     - |  5856 | `				sizeof("getIterator")-1);` |
|     - |  5857 | `			ph7_value sInner;` |
|     3 |  5858 | `			PH7_MemObjInit(pVm,&sInner);` |
|     3 |  5859 | `			rc = pGet ? PH7_VmCallClassMethod(pVm,pSrc,pGet,&sInner,0,0) : SXRET_OK;` |
|     3 |  5860 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  5861 | `				PH7_MemObjRelease(&sInner);` |
|   ! 0 |  5862 | `				PH7_MemObjRelease(&sSrc);` |
|   ! 0 |  5863 | `				PH7_ClassInstanceUnref(pWrap);` |
|   ! 0 |  5864 | `				return rc;` |
|     - |  5865 | `			}` |
|     3 |  5866 | `			PH7_MemObjStore(&sInner,&sSrc);` |
|     3 |  5867 | `			PH7_MemObjRelease(&sInner);` |
|     1 |  5868 | `		}` |
|     - |  5869 | `	}` |
|    47 |  5870 | `	PH7_MemObjInitFromInt(pVm,&sCache,iCache);` |
|    47 |  5871 | `	apCtor[0] = &sSrc;` |
|    47 |  5872 | `	apCtor[1] = &sCache;` |
|    47 |  5873 | `	rc = PH7_VmCallClassMethod(pVm,pWrap,pCons,0,2,apCtor);` |
|    47 |  5874 | `	PH7_MemObjRelease(&sCache);` |
|    47 |  5875 | `	PH7_MemObjRelease(&sSrc);` |
|    47 |  5876 | `	if( rc != SXRET_OK ){` |
|     5 |  5877 | `		PH7_ClassInstanceUnref(pWrap);` |
|     5 |  5878 | `		return rc;` |
|     - |  5879 | `	}` |
|    43 |  5880 | `	PH7_MemObjInit(pVm,&sWrap);` |
|    43 |  5881 | `	sWrap.x.pOther = pWrap;` |
|    43 |  5882 | `	MemObjSetType(&sWrap,MEMOBJ_OBJ);` |
|    43 |  5883 | `	pWrap->iRef++;                    /* the temporary owns one of its own */` |
|    43 |  5884 | `	PH7_MemObjInitFromInt(pVm,&sMode,iMode);` |
|    43 |  5885 | `	PH7_MemObjInitFromInt(pVm,&sFlags,iFlags);` |
|    43 |  5886 | `	apCtor[0] = &sWrap;` |
|    43 |  5887 | `	apCtor[1] = &sMode;` |
|    43 |  5888 | `	apCtor[2] = &sFlags;` |
|    43 |  5889 | `	rc = vm_builtin_RecursiveIteratorIterator_construct(pCtx,3,apCtor);` |
|    43 |  5890 | `	PH7_MemObjRelease(&sWrap);` |
|    43 |  5891 | `	PH7_MemObjRelease(&sMode);` |
|    43 |  5892 | `	PH7_MemObjRelease(&sFlags);` |
|    43 |  5893 | `	PH7_ClassInstanceUnref(pWrap);` |
|    43 |  5894 | `	return rc;` |
|    26 |  5895 | `}` |
|    16 |  5896 | `static int vm_builtin_RecursiveTreeIterator_getPrefix(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5897 | `{` |
|     - |  5898 | `	ph7_value sOut;` |
|     - |  5899 | `	sxi32 rc;` |
|     8 |  5900 | `	SXUNUSED(nArg);` |
|     8 |  5901 | `	SXUNUSED(apArg);` |
|    17 |  5902 | `	if( !RitReady(PH7_ContextThis(pCtx)) ){` |
|     3 |  5903 | `		return RitNotReady(pCtx);` |
|     - |  5904 | `	}` |
|    15 |  5905 | `	PH7_MemObjInitFromString(pCtx->pVm,&sOut,0);` |
|    15 |  5906 | `	rc = RtiBuildPrefix(pCtx,&sOut);` |
|    15 |  5907 | `	if( rc == SXRET_OK ){` |
|    15 |  5908 | `		ph7_result_value(pCtx,&sOut);` |
|     7 |  5909 | `	}` |
|    15 |  5910 | `	PH7_MemObjRelease(&sOut);` |
|    15 |  5911 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|     9 |  5912 | `}` |
|    24 |  5913 | `static int vm_builtin_RecursiveTreeIterator_getEntry(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5914 | `{` |
|     - |  5915 | `	ph7_value sOut;` |
|     - |  5916 | `	sxi32 rc;` |
|    12 |  5917 | `	SXUNUSED(nArg);` |
|    12 |  5918 | `	SXUNUSED(apArg);` |
|    25 |  5919 | `	if( !RitReady(PH7_ContextThis(pCtx)) ){` |
|     3 |  5920 | `		return RitNotReady(pCtx);` |
|     - |  5921 | `	}` |
|    23 |  5922 | `	PH7_MemObjInitFromString(pCtx->pVm,&sOut,0);` |
|    23 |  5923 | `	rc = RtiBuildEntry(pCtx,&sOut);` |
|    23 |  5924 | `	if( rc == SXRET_OK ){` |
|    21 |  5925 | `		ph7_result_value(pCtx,&sOut);` |
|    10 |  5926 | `	}` |
|    23 |  5927 | `	PH7_MemObjRelease(&sOut);` |
|    23 |  5928 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    13 |  5929 | `}` |
|    16 |  5930 | `static int vm_builtin_RecursiveTreeIterator_getPostfix(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5931 | `{` |
|    17 |  5932 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  5933 | `	ph7_value *pSlot;` |
|     8 |  5934 | `	SXUNUSED(nArg);` |
|     8 |  5935 | `	SXUNUSED(apArg);` |
|    17 |  5936 | `	if( !RitReady(pThis) ){` |
|     3 |  5937 | `		return RitNotReady(pCtx);` |
|     - |  5938 | `	}` |
|    15 |  5939 | `	pSlot = PH7_NativeAttr(pThis,RTI_PST);` |
|    15 |  5940 | `	if( pSlot ){` |
|    15 |  5941 | `		ph7_result_value(pCtx,pSlot);` |
|     7 |  5942 | `	}` |
|    15 |  5943 | `	return PH7_OK;` |
|     9 |  5944 | `}` |
|     2 |  5945 | `static int vm_builtin_RecursiveTreeIterator_setPostfix(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5946 | `{` |
|     3 |  5947 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     3 |  5948 | `	if( nArg < 1 \|\| pThis == 0 ){` |
|   ! 0 |  5949 | `		return PH7_OK;` |
|     - |  5950 | `	}` |
|     3 |  5951 | `	DualSetSlot(pCtx->pVm,pThis,RTI_PST,apArg[0]);` |
|     3 |  5952 | `	return PH7_OK;` |
|     2 |  5953 | `}` |
|    20 |  5954 | `static int vm_builtin_RecursiveTreeIterator_setPrefixPart(ph7_context *pCtx,int nArg,` |
|     - |  5955 | `	ph7_value **apArg)` |
|     1 |  5956 | `{` |
|    21 |  5957 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  5958 | `	ph7_value *pSlot;` |
|     - |  5959 | `	sxi64 iPart;` |
|    21 |  5960 | `	if( nArg < 2 \|\| pThis == 0 ){` |
|   ! 0 |  5961 | `		return PH7_OK;` |
|     - |  5962 | `	}` |
|    21 |  5963 | `	iPart = ph7_value_to_int64(apArg[0]);` |
|    21 |  5964 | `	if( iPart < RTIT_PREFIX_LEFT \|\| iPart > RTIT_PREFIX_RIGHT ){` |
|     5 |  5965 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  5966 | `			"RecursiveTreeIterator::setPrefixPart(): Argument #1 ($part) must be a "` |
|     - |  5967 | `			"RecursiveTreeIterator::PREFIX_* constant");` |
|     - |  5968 | `	}` |
|    17 |  5969 | `	pSlot = RtiPrefixSlot(pCtx->pVm,pThis);` |
|    17 |  5970 | `	if( pSlot ){` |
|    17 |  5971 | `		ph7_array_add_intkey_elem(pSlot,(int)iPart,apArg[1]);` |
|     8 |  5972 | `	}` |
|    17 |  5973 | `	return PH7_OK;` |
|    11 |  5974 | `}` |
|     - |  5975 | `/* php's current()/key(): the traversal's own answer, wrapped unless its BYPASS` |
|     - |  5976 | ` * flag is set. The wrapped form is always a STRING, prefix and postfix included. */` |
|   164 |  5977 | `static int RtiWrapped(ph7_context *pCtx,int bKey)` |
|     1 |  5978 | `{` |
|   165 |  5979 | `	ph7_vm *pVm = pCtx->pVm;` |
|   165 |  5980 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  5981 | `	ph7_value sOut;` |
|     - |  5982 | `	sxi32 rc;` |
|   165 |  5983 | `	if( !RitReady(pThis) ){` |
|     5 |  5984 | `		return RitNotReady(pCtx);` |
|     - |  5985 | `	}` |
|   161 |  5986 | `	if( RitInt(pThis,RIT_FL) & (bKey ? RTIT_BYPASS_KEY : RTIT_BYPASS_CURRENT) ){` |
|    51 |  5987 | `		return bKey ? RitCurrentLevelCall(pCtx,"key",sizeof("key")-1)` |
|    62 |  5988 | `		            : RitCurrentLevelCall(pCtx,"current",sizeof("current")-1);` |
|     - |  5989 | `	}` |
|    99 |  5990 | `	PH7_MemObjInitFromString(pVm,&sOut,0);` |
|    99 |  5991 | `	rc = RtiBuildPrefix(pCtx,&sOut);` |
|    99 |  5992 | `	if( rc == SXRET_OK ){` |
|    99 |  5993 | `		if( bKey ){` |
|    25 |  5994 | `			ph7_class_instance *pSub = RitSub(pVm,pThis,RitInt(pThis,RIT_LVL));` |
|    25 |  5995 | `			ph7_class_method *pMethod = pSub` |
|    24 |  5996 | `				? PH7_ClassExtractMethod(pSub->pClass,"key",sizeof("key")-1) : 0;` |
|    25 |  5997 | `			if( pMethod ){` |
|     - |  5998 | `				ph7_value sKey;` |
|    25 |  5999 | `				PH7_MemObjInit(pVm,&sKey);` |
|    25 |  6000 | `				rc = PH7_VmCallClassMethod(pVm,pSub,pMethod,&sKey,0,0);` |
|    25 |  6001 | `				if( rc == SXRET_OK && (sKey.iFlags & MEMOBJ_NULL) == 0 ){` |
|     - |  6002 | `					int nTxt;` |
|     - |  6003 | `					const char *zTxt;` |
|    25 |  6004 | `					rc = PH7_MemObjToStringUV(&sKey);` |
|    25 |  6005 | `					zTxt = ph7_value_to_string(&sKey,&nTxt);` |
|    25 |  6006 | `					if( rc == SXRET_OK && nTxt > 0 ){` |
|    25 |  6007 | `						PH7_MemObjStringAppend(&sOut,zTxt,(sxu32)nTxt);` |
|    12 |  6008 | `					}` |
|    12 |  6009 | `				}` |
|    25 |  6010 | `				PH7_MemObjRelease(&sKey);` |
|    12 |  6011 | `			}` |
|    13 |  6012 | `		}else{` |
|    75 |  6013 | `			rc = RtiBuildEntry(pCtx,&sOut);` |
|     - |  6014 | `		}` |
|    49 |  6015 | `	}` |
|    99 |  6016 | `	if( rc == SXRET_OK ){` |
|    99 |  6017 | `		RtiAppendPostfix(pVm,pThis,&sOut);` |
|    99 |  6018 | `		ph7_result_value(pCtx,&sOut);` |
|    49 |  6019 | `	}` |
|    99 |  6020 | `	PH7_MemObjRelease(&sOut);` |
|    99 |  6021 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    83 |  6022 | `}` |
|   100 |  6023 | `static int vm_builtin_RecursiveTreeIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6024 | `{` |
|    50 |  6025 | `	SXUNUSED(nArg);` |
|    50 |  6026 | `	SXUNUSED(apArg);` |
|   101 |  6027 | `	return RtiWrapped(pCtx,FALSE);` |
|     1 |  6028 | `}` |
|    64 |  6029 | `static int vm_builtin_RecursiveTreeIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6030 | `{` |
|    32 |  6031 | `	SXUNUSED(nArg);` |
|    32 |  6032 | `	SXUNUSED(apArg);` |
|    65 |  6033 | `	return RtiWrapped(pCtx,TRUE);` |
|     1 |  6034 | `}` |
|     - |  6035 | `/*` |
|     - |  6036 | ` * The declaration. Method ORDER follows spl_iterators.stub.php line for line,` |
|     - |  6037 | ` * because that is the order Reflection reports. Every return type is php's` |
|     - |  6038 | `` * `@tentative-return-type` kind (rule 45).`` |
|     - |  6039 | ` */` |
|  6721 |  6040 | `static sxi32 VmInstallSplRecursiveIt(ph7_vm *pVm)` |
|     5 |  6041 | `{` |
|     - |  6042 | `	static const PH7_NativePropDef aRitProp[] = {` |
|     - |  6043 | `		{ RIT_ST,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  6044 | `		{ RIT_SS,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  6045 | `		{ RIT_LVL, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  6046 | `		{ RIT_MD,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  6047 | `		{ RIT_FL,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  6048 | `		{ RIT_MX,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, -1, 0, 0.0 }, 0 },` |
|     - |  6049 | `		{ RIT_II,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  6050 | `		{ RIT_RD,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  6051 | `	};` |
|     - |  6052 | `	static const PH7_NativeConstDef aRitConst[] = {` |
|     - |  6053 | `		{ "LEAVES_ONLY",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RIT_LEAVES_ONLY, 0, 0.0 },` |
|     - |  6054 | `		{ "SELF_FIRST",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RIT_SELF_FIRST, 0, 0.0 },` |
|     - |  6055 | `		{ "CHILD_FIRST",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RIT_CHILD_FIRST, 0, 0.0 },` |
|     - |  6056 | `		{ "CATCH_GET_CHILD", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RIT_CATCH_GET_CHILD, 0, 0.0 },` |
|     - |  6057 | `	};` |
|     - |  6058 | `	static const PH7_NativeMethodDef aRitMethod[] = {` |
|     - |  6059 | `		{ "__construct",      PH7_MOD_PUBLIC,` |
|     - |  6060 | ``		  /* php's stub spells the default `RecursiveIteratorIterator::LEAVES_ONLY`;`` |
|     - |  6061 | `		   * one zSig field cannot carry both the TEXT and the VALUE, and the value` |
|     - |  6062 | `		   * wins here for the same reason it does on RegexIterator's row. */` |
|     - |  6063 | `		  "Traversable $iterator, int $mode = RecursiveIteratorIterator::LEAVES_ONLY, "` |
|     - |  6064 | `		  "int $flags = 0", 0,` |
|     - |  6065 | `		  vm_builtin_RecursiveIteratorIterator_construct },` |
|     - |  6066 | `		{ "rewind",           PH7_MOD_PUBLIC, "", "@void",` |
|     - |  6067 | `		  vm_builtin_RecursiveIteratorIterator_rewind },` |
|     - |  6068 | `		{ "valid",            PH7_MOD_PUBLIC, "", "@bool",` |
|     - |  6069 | `		  vm_builtin_RecursiveIteratorIterator_valid },` |
|     - |  6070 | `		{ "key",              PH7_MOD_PUBLIC, "", "@mixed",` |
|     - |  6071 | `		  vm_builtin_RecursiveIteratorIterator_key },` |
|     - |  6072 | `		{ "current",          PH7_MOD_PUBLIC, "", "@mixed",` |
|     - |  6073 | `		  vm_builtin_RecursiveIteratorIterator_current },` |
|     - |  6074 | `		{ "next",             PH7_MOD_PUBLIC, "", "@void",` |
|     - |  6075 | `		  vm_builtin_RecursiveIteratorIterator_next },` |
|     - |  6076 | `		{ "getDepth",         PH7_MOD_PUBLIC, "", "@int",` |
|     - |  6077 | `		  vm_builtin_RecursiveIteratorIterator_getDepth },` |
|     - |  6078 | `		{ "getSubIterator",   PH7_MOD_PUBLIC, "?int $level = null", "@?RecursiveIterator",` |
|     - |  6079 | `		  vm_builtin_RecursiveIteratorIterator_getSubIterator },` |
|     - |  6080 | `		{ "getInnerIterator", PH7_MOD_PUBLIC, "", "@RecursiveIterator",` |
|     - |  6081 | `		  vm_builtin_RecursiveIteratorIterator_getInnerIterator },` |
|     - |  6082 | `		{ "beginIteration",   PH7_MOD_PUBLIC, "", "@void",` |
|     - |  6083 | `		  vm_builtin_RecursiveIteratorIterator_nop },` |
|     - |  6084 | `		{ "endIteration",     PH7_MOD_PUBLIC, "", "@void",` |
|     - |  6085 | `		  vm_builtin_RecursiveIteratorIterator_nop },` |
|     - |  6086 | `		{ "callHasChildren",  PH7_MOD_PUBLIC, "", "@bool",` |
|     - |  6087 | `		  vm_builtin_RecursiveIteratorIterator_callHasChildren },` |
|     - |  6088 | `		{ "callGetChildren",  PH7_MOD_PUBLIC, "", "@?RecursiveIterator",` |
|     - |  6089 | `		  vm_builtin_RecursiveIteratorIterator_callGetChildren },` |
|     - |  6090 | `		{ "beginChildren",    PH7_MOD_PUBLIC, "", "@void",` |
|     - |  6091 | `		  vm_builtin_RecursiveIteratorIterator_nop },` |
|     - |  6092 | `		{ "endChildren",      PH7_MOD_PUBLIC, "", "@void",` |
|     - |  6093 | `		  vm_builtin_RecursiveIteratorIterator_nop },` |
|     - |  6094 | `		{ "nextElement",      PH7_MOD_PUBLIC, "", "@void",` |
|     - |  6095 | `		  vm_builtin_RecursiveIteratorIterator_nop },` |
|     - |  6096 | `		{ "setMaxDepth",      PH7_MOD_PUBLIC, "int $maxDepth = -1", "@void",` |
|     - |  6097 | `		  vm_builtin_RecursiveIteratorIterator_setMaxDepth },` |
|     - |  6098 | `		{ "getMaxDepth",      PH7_MOD_PUBLIC, "", "@int\|false",` |
|     - |  6099 | `		  vm_builtin_RecursiveIteratorIterator_getMaxDepth },` |
|     - |  6100 | `	};` |
|     - |  6101 | ``	/* PH7_CLASS_NOCLONE: php refuses `clone` outright ("Trying to clone an`` |
|     - |  6102 | `	 * uncloneable object"), and a slot-by-slot copy would share one level stack --` |
|     - |  6103 | `	 * and with it one cursor -- between two traversals. */` |
|     - |  6104 | `	static const PH7_NativeConstDef aRtiConst[] = {` |
|     - |  6105 | `		{ "BYPASS_CURRENT",        PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RTIT_BYPASS_CURRENT, 0, 0.0 },` |
|     - |  6106 | `		{ "BYPASS_KEY",            PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RTIT_BYPASS_KEY, 0, 0.0 },` |
|     - |  6107 | `		{ "PREFIX_LEFT",           PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RTIT_PREFIX_LEFT, 0, 0.0 },` |
|     - |  6108 | `		{ "PREFIX_MID_HAS_NEXT",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT,` |
|     - |  6109 | `		  RTIT_PREFIX_MID_HAS_NEXT, 0, 0.0 },` |
|     - |  6110 | `		{ "PREFIX_MID_LAST",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RTIT_PREFIX_MID_LAST, 0, 0.0 },` |
|     - |  6111 | `		{ "PREFIX_END_HAS_NEXT",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT,` |
|     - |  6112 | `		  RTIT_PREFIX_END_HAS_NEXT, 0, 0.0 },` |
|     - |  6113 | `		{ "PREFIX_END_LAST",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RTIT_PREFIX_END_LAST, 0, 0.0 },` |
|     - |  6114 | `		{ "PREFIX_RIGHT",          PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RTIT_PREFIX_RIGHT, 0, 0.0 },` |
|     - |  6115 | `	};` |
|     - |  6116 | `	static const PH7_NativePropDef aRtiProp[] = {` |
|     - |  6117 | `		{ RTI_PFX, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  6118 | `		{ RTI_PST, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  6119 | `	};` |
|     - |  6120 | `	static const PH7_NativeMethodDef aRtiMethod[] = {` |
|     - |  6121 | `		{ "__construct",   PH7_MOD_PUBLIC,` |
|     - |  6122 | `		  "~RecursiveIterator\|IteratorAggregate $iterator, "` |
|     - |  6123 | `		  "int $flags = RecursiveTreeIterator::BYPASS_KEY, "` |
|     - |  6124 | `		  "int $cachingIteratorFlags = CachingIterator::CATCH_GET_CHILD, "` |
|     - |  6125 | `		  "int $mode = RecursiveTreeIterator::SELF_FIRST", 0,` |
|     - |  6126 | `		  vm_builtin_RecursiveTreeIterator_construct },` |
|     - |  6127 | `		{ "key",           PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_RecursiveTreeIterator_key },` |
|     - |  6128 | `		{ "current",       PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_RecursiveTreeIterator_current },` |
|     - |  6129 | `		{ "getPrefix",     PH7_MOD_PUBLIC, "", "@string",` |
|     - |  6130 | `		  vm_builtin_RecursiveTreeIterator_getPrefix },` |
|     - |  6131 | `		{ "setPostfix",    PH7_MOD_PUBLIC, "string $postfix", "@void",` |
|     - |  6132 | `		  vm_builtin_RecursiveTreeIterator_setPostfix },` |
|     - |  6133 | `		{ "setPrefixPart", PH7_MOD_PUBLIC, "int $part, string $value", "@void",` |
|     - |  6134 | `		  vm_builtin_RecursiveTreeIterator_setPrefixPart },` |
|     - |  6135 | `		{ "getEntry",      PH7_MOD_PUBLIC, "", "@string",` |
|     - |  6136 | `		  vm_builtin_RecursiveTreeIterator_getEntry },` |
|     - |  6137 | `		{ "getPostfix",    PH7_MOD_PUBLIC, "", "@string",` |
|     - |  6138 | `		  vm_builtin_RecursiveTreeIterator_getPostfix },` |
|     - |  6139 | `	};` |
|     - |  6140 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - |  6141 | `		{ "RecursiveIteratorIterator", 0, "OuterIterator", PH7_CLASS_NOCLONE,` |
|     - |  6142 | `		  aRitMethod, SX_ARRAYSIZE(aRitMethod),` |
|     - |  6143 | `		  aRitConst, SX_ARRAYSIZE(aRitConst),` |
|     - |  6144 | `		  aRitProp, SX_ARRAYSIZE(aRitProp), 0, 0, 0 },` |
|     - |  6145 | `		/* Its four inherited mode constants come with the parent; the eight below` |
|     - |  6146 | `		 * are its own, and BYPASS_* share the flags word CATCH_GET_CHILD lives in. */` |
|     - |  6147 | `		{ "RecursiveTreeIterator", "RecursiveIteratorIterator", 0, PH7_CLASS_NOCLONE,` |
|     - |  6148 | `		  aRtiMethod, SX_ARRAYSIZE(aRtiMethod),` |
|     - |  6149 | `		  aRtiConst, SX_ARRAYSIZE(aRtiConst),` |
|     - |  6150 | `		  aRtiProp, SX_ARRAYSIZE(aRtiProp), 0, 0, 0 },` |
|     - |  6151 | `	};` |
|  6726 |  6152 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 |  6153 | `}` |
|     - |  6154 | `/*` |
|     - |  6155 | ` * ---------------------------------------------------------------------------` |
|     - |  6156 | ` * SplDoublyLinkedList, SplStack and SplQueue.` |
|     - |  6157 | ` *` |
|     - |  6158 | `` * php's `spl_dllist_object` is a linked list plus a FLAGS word, and the flags are`` |
|     - |  6159 | ` * where the family's shape lives: SPL_DLLIST_IT_LIFO (2) and IT_DELETE (1) are the` |
|     - |  6160 | ` * iteration mode, and IT_FIX (4) is a bit no CONSTANT names and no user can set --` |
|     - |  6161 | ` * the object handler stamps it at creation for SplStack and SplQueue, and it is` |
|     - |  6162 | ``  * both what freezes their LIFO/FIFO choice and why `(new SplStack)->getIteratorMode()` `` |
|     - |  6163 | ` * answers 6 rather than 2. The chunk had no notion of it, so both classes reported` |
|     - |  6164 | `` * the wrong mode and `setIteratorMode()` reported the wrong result.`` |
|     - |  6165 | ` *` |
|     - |  6166 | ` * Two rules follow from php's own code and neither is guessable from the methods:` |
|     - |  6167 | ` *` |
|     - |  6168 | ` *   - **every ArrayAccess offset is measured from the END of a LIFO list.**` |
|     - |  6169 | `` *     php resolves them through `spl_ptr_llist_offset(llist, offset, flags & LIFO)`,`` |
|     - |  6170 | `` *     so `$stack[0]` is the element `top()` answers, not the one `bottom()` does.`` |
|     - |  6171 | ` *     The chunk indexed the backing array directly and had the whole SplStack` |
|     - |  6172 | ` *     subscript surface reversed.` |
|     - |  6173 | ` *   - **the traverse POSITION is the list index in both modes.** A LIFO rewind` |
|     - |  6174 | `` *     seeds `count-1` and counts down, a FIFO rewind seeds 0 and counts up, so`` |
|     - |  6175 | `` *     `key()` and the element's own place in the list agree either way -- except`` |
|     - |  6176 | ` *     under IT_DELETE in FIFO order, where php consumes the head and deliberately` |
|     - |  6177 | ` *     does NOT advance the position (every element reports key 0).` |
|     - |  6178 | ` *` |
|     - |  6179 | `` * `toArray()` was a PHL INVENTION -- php has no such method on any of the three --`` |
|     - |  6180 | `` * and it is gone. What php has instead, and the chunk had none of: `__debugInfo()`,`` |
|     - |  6181 | `` * the `Serializable` interface with its `serialize()`/`unserialize()` pair, and the`` |
|     - |  6182 | `` * `__serialize()`/`__unserialize()` pair that php actually uses (which is why the`` |
|     - |  6183 | `` * serialized form is `O:19:"SplDoublyLinkedList":3:{i:0;…}` and not a property dump).`` |
|     - |  6184 | ` *` |
|     - |  6185 | ` * The store is a php array in a hidden slot, head->tail, so push/pop/shift/unshift` |
|     - |  6186 | ` * are the engine's OWN array builtins called with the slot (rule 7, and rule 39's` |
|     - |  6187 | ` * reference rule already lives inside them). php's element-POINTER cursor is not` |
|     - |  6188 | ` * modelled: a manual walk that mutates the list under itself resolves by position` |
|     - |  6189 | ` * here and by identity there. That is one probe line (§7.4) and the only one.` |
|     - |  6190 | ` */` |
|     - |  6191 | `#define DLL_Q  "__q"   /* php's llist, head -> tail */` |
|     - |  6192 | `#define DLL_FL "__fl"  /* php's flags word, IT_FIX included */` |
|     - |  6193 | `#define DLL_I  "__i"   /* php's traverse_position */` |
|     - |  6194 |  |
|     - |  6195 | `#define DLL_IT_DELETE 1` |
|     - |  6196 | `#define DLL_IT_LIFO   2` |
|     - |  6197 | `#define DLL_IT_FIX    4   /* php's SPL_DLLIST_IT_FIX: stamped at creation, never by a user */` |
|     - |  6198 | `#define DLL_IT_MASK   3` |
|     - |  6199 |  |
|   614 |  6200 | `static ph7_value * DllSlot(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  6201 | `{` |
|   615 |  6202 | `	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,DLL_Q) : 0;` |
|   615 |  6203 | `	if( pSlot == 0 ){` |
|   ! 0 |  6204 | `		return 0;` |
|     - |  6205 | `	}` |
|   615 |  6206 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    97 |  6207 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|   ! 0 |  6208 | `			return 0;` |
|     - |  6209 | `		}` |
|    48 |  6210 | `	}` |
|   615 |  6211 | `	if( PH7_HashmapCowSeparate(pVm,pSlot) == 0 ){` |
|   ! 0 |  6212 | `		return 0;` |
|     - |  6213 | `	}` |
|     - |  6214 | `	/* Re-ask: the separation can MOVE the memobj pool this slot lives in` |
|     - |  6215 | `	 * (SplStoreSlot explains it). */` |
|   615 |  6216 | `	return PH7_NativeAttr(pThis,DLL_Q);` |
|   308 |  6217 | `}` |
|   328 |  6218 | `static ph7_hashmap * DllMap(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  6219 | `{` |
|   329 |  6220 | `	ph7_value *pSlot = DllSlot(pVm,pThis);` |
|   329 |  6221 | `	return pSlot ? (ph7_hashmap *)pSlot->x.pOther : 0;` |
|     1 |  6222 | `}` |
|   238 |  6223 | `static sxi64 DllCount(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  6224 | `{` |
|   239 |  6225 | `	ph7_hashmap *pMap = DllMap(pVm,pThis);` |
|   239 |  6226 | `	return pMap ? (sxi64)pMap->nEntry : 0;` |
|     1 |  6227 | `}` |
|   186 |  6228 | `static int DllFlags(ph7_class_instance *pThis)` |
|     1 |  6229 | `{` |
|   187 |  6230 | `	return pThis ? (int)PH7_NativeAttrInt(pThis,DLL_FL) : 0;` |
|     1 |  6231 | `}` |
|     - |  6232 | `/* The value at a LIST index (head = 0), or NULL. */` |
|    86 |  6233 | `static ph7_value * DllAt(ph7_vm *pVm,ph7_class_instance *pThis,sxi64 iIndex)` |
|     1 |  6234 | `{` |
|    87 |  6235 | `	ph7_hashmap *pMap = DllMap(pVm,pThis);` |
|    87 |  6236 | `	ph7_hashmap_node *pNode = 0;` |
|    87 |  6237 | `	if( pMap == 0 \|\| HashmapLookupIntKey(pMap,iIndex,&pNode) != SXRET_OK ){` |
|   ! 0 |  6238 | `		return 0;` |
|     - |  6239 | `	}` |
|    87 |  6240 | `	return HashmapExtractNodeValue(pNode);` |
|    44 |  6241 | `}` |
|     - |  6242 | `/*` |
|     - |  6243 | ` * php's spl_ptr_llist_offset: an ArrayAccess offset counts from the TAIL when the` |
|     - |  6244 | ` * list iterates LIFO. Every offsetGet/offsetSet/offsetUnset/add goes through here.` |
|     - |  6245 | ` */` |
|    24 |  6246 | `static sxi64 DllOffsetToIndex(ph7_class_instance *pThis,sxi64 iOffset,sxi64 nCount)` |
|     1 |  6247 | `{` |
|    25 |  6248 | `	if( DllFlags(pThis) & DLL_IT_LIFO ){` |
|    11 |  6249 | `		return nCount - 1 - iOffset;` |
|     - |  6250 | `	}` |
|    15 |  6251 | `	return iOffset;` |
|    13 |  6252 | `}` |
|     - |  6253 | `/* Hand one of the engine's own array builtins this instance's storage slot. */` |
|   242 |  6254 | `static int DllArrayCall(ph7_context *pCtx,ProchHostFunction xFunc,ph7_value **apExtra,int nExtra)` |
|     1 |  6255 | `{` |
|   243 |  6256 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  6257 | `	ph7_value *apCall[4];` |
|   243 |  6258 | `	ph7_value *pSlot = DllSlot(pCtx->pVm,pThis);` |
|     - |  6259 | `	int i;` |
|   243 |  6260 | `	if( pSlot == 0 ){` |
|   ! 0 |  6261 | `		return PH7_OK;` |
|     - |  6262 | `	}` |
|   243 |  6263 | `	apCall[0] = pSlot;` |
|   465 |  6264 | `	for( i = 0 ; i < nExtra && i < 3 ; ++i ){` |
|   223 |  6265 | `		apCall[i+1] = apExtra[i];` |
|   112 |  6266 | `	}` |
|   243 |  6267 | `	return xFunc(pCtx,nExtra+1,apCall);` |
|   122 |  6268 | `}` |
|     - |  6269 | `/* php's four "empty datastructure" refusals, which differ only in the verb. */` |
|    18 |  6270 | `static sxi32 DllEmpty(ph7_context *pCtx,const char *zVerb)` |
|     1 |  6271 | `{` |
|    28 |  6272 | `	return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     9 |  6273 | `		"Can't %s an empty datastructure",zVerb);` |
|     1 |  6274 | `}` |
|     - |  6275 | `/*` |
|     - |  6276 | ` * php words every out-of-range offset from the DECLARING class, not the runtime` |
|     - |  6277 | `` * one: `SplStack::add()` on an out-of-range index still says`` |
|     - |  6278 | `` * `SplDoublyLinkedList::add()`. The chunk used get_class($this) and reported the`` |
|     - |  6279 | ` * subclass.` |
|     - |  6280 | ` */` |
|    14 |  6281 | `static sxi32 DllOutOfRange(ph7_context *pCtx,const char *zMethod)` |
|     1 |  6282 | `{` |
|    22 |  6283 | `	return PH7_VmThrowException(pCtx,"OutOfRangeException",` |
|     7 |  6284 | `		"SplDoublyLinkedList::%s(): Argument #1 ($index) is out of range",zMethod);` |
|     1 |  6285 | `}` |
|     - |  6286 | `/*` |
|     - |  6287 | ``  * php's ZPP for the four ArrayAccess offsets and add(): the stub leaves `$index` `` |
|     - |  6288 | ` * UNTYPED (which is what Reflection prints) while the ZPP is Z_PARAM_LONG, whose` |
|     - |  6289 | `` * TypeError says `must be of type int`. An untyped signature is not screened`` |
|     - |  6290 | ` * centrally, so the rule is applied here — rule 41's disagreement, resolved without` |
|     - |  6291 | ` * an azSelfChecked[] row because the declared type is absent rather than different.` |
|     - |  6292 | ` */` |
|    46 |  6293 | `static sxi32 DllIndexArg(ph7_context *pCtx,const char *zMethod,ph7_value *pArg,sxi64 *piOut)` |
|     1 |  6294 | `{` |
|     - |  6295 | `	char zFunc[64];` |
|    47 |  6296 | `	SyBufferFormat(zFunc,sizeof(zFunc),"SplDoublyLinkedList::%s",zMethod);` |
|    47 |  6297 | `	return PH7_IntArgResolve(pCtx,pArg,zFunc,1,"$index","int",piOut);` |
|     1 |  6298 | `}` |
|   206 |  6299 | `static int vm_builtin_SplDll_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6300 | `{` |
|     - |  6301 | `	ph7_value *apExtra[1];` |
|   207 |  6302 | `	if( nArg < 1 ){` |
|   ! 0 |  6303 | `		return PH7_OK;` |
|     - |  6304 | `	}` |
|   207 |  6305 | `	apExtra[0] = apArg[0];` |
|   207 |  6306 | `	DllArrayCall(pCtx,ph7_hashmap_push,apExtra,1);` |
|   207 |  6307 | `	ph7_result_null(pCtx);   /* array_push answers the new count; php's push is void */` |
|   207 |  6308 | `	return PH7_OK;` |
|   104 |  6309 | `}` |
|     2 |  6310 | `static int vm_builtin_SplDll_unshift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6311 | `{` |
|     - |  6312 | `	ph7_value *apExtra[1];` |
|     3 |  6313 | `	if( nArg < 1 ){` |
|   ! 0 |  6314 | `		return PH7_OK;` |
|     - |  6315 | `	}` |
|     3 |  6316 | `	apExtra[0] = apArg[0];` |
|     3 |  6317 | `	DllArrayCall(pCtx,ph7_hashmap_unshift,apExtra,1);` |
|     3 |  6318 | `	ph7_result_null(pCtx);` |
|     3 |  6319 | `	return PH7_OK;` |
|     2 |  6320 | `}` |
|     6 |  6321 | `static int vm_builtin_SplDll_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6322 | `{` |
|     3 |  6323 | `	SXUNUSED(nArg);` |
|     3 |  6324 | `	SXUNUSED(apArg);` |
|     7 |  6325 | `	if( DllCount(pCtx->pVm,PH7_ContextThis(pCtx)) == 0 ){` |
|     5 |  6326 | `		return DllEmpty(pCtx,"pop from");` |
|     - |  6327 | `	}` |
|     3 |  6328 | `	return DllArrayCall(pCtx,ph7_hashmap_pop,0,0);` |
|     4 |  6329 | `}` |
|    10 |  6330 | `static int vm_builtin_SplDll_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6331 | `{` |
|     5 |  6332 | `	SXUNUSED(nArg);` |
|     5 |  6333 | `	SXUNUSED(apArg);` |
|    11 |  6334 | `	if( DllCount(pCtx->pVm,PH7_ContextThis(pCtx)) == 0 ){` |
|     7 |  6335 | `		return DllEmpty(pCtx,"shift from");` |
|     - |  6336 | `	}` |
|     5 |  6337 | `	return DllArrayCall(pCtx,ph7_hashmap_shift,0,0);` |
|     6 |  6338 | `}` |
|     - |  6339 | `/* top() is the TAIL and bottom() the HEAD, whatever the iteration mode: php reads` |
|     - |  6340 | ` * llist->tail/llist->head directly and never consults the flags here. */` |
|    10 |  6341 | `static int vm_builtin_SplDll_top(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6342 | `{` |
|    11 |  6343 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    11 |  6344 | `	sxi64 nCount = DllCount(pCtx->pVm,pThis);` |
|     - |  6345 | `	ph7_value *pVal;` |
|     5 |  6346 | `	SXUNUSED(nArg);` |
|     5 |  6347 | `	SXUNUSED(apArg);` |
|    11 |  6348 | `	if( nCount == 0 ){` |
|     5 |  6349 | `		return DllEmpty(pCtx,"peek at");` |
|     - |  6350 | `	}` |
|     7 |  6351 | `	pVal = DllAt(pCtx->pVm,pThis,nCount-1);` |
|     7 |  6352 | `	if( pVal ){` |
|     7 |  6353 | `		ph7_result_value(pCtx,pVal);` |
|     3 |  6354 | `	}` |
|     7 |  6355 | `	return PH7_OK;` |
|     6 |  6356 | `}` |
|    10 |  6357 | `static int vm_builtin_SplDll_bottom(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6358 | `{` |
|    11 |  6359 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  6360 | `	ph7_value *pVal;` |
|     5 |  6361 | `	SXUNUSED(nArg);` |
|     5 |  6362 | `	SXUNUSED(apArg);` |
|    11 |  6363 | `	if( DllCount(pCtx->pVm,pThis) == 0 ){` |
|     5 |  6364 | `		return DllEmpty(pCtx,"peek at");` |
|     - |  6365 | `	}` |
|     7 |  6366 | `	pVal = DllAt(pCtx->pVm,pThis,0);` |
|     7 |  6367 | `	if( pVal ){` |
|     7 |  6368 | `		ph7_result_value(pCtx,pVal);` |
|     3 |  6369 | `	}` |
|     7 |  6370 | `	return PH7_OK;` |
|     6 |  6371 | `}` |
|    16 |  6372 | `static int vm_builtin_SplDll_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6373 | `{` |
|     8 |  6374 | `	SXUNUSED(nArg);` |
|     8 |  6375 | `	SXUNUSED(apArg);` |
|    17 |  6376 | `	ph7_result_int64(pCtx,DllCount(pCtx->pVm,PH7_ContextThis(pCtx)));` |
|    17 |  6377 | `	return PH7_OK;` |
|     1 |  6378 | `}` |
|     2 |  6379 | `static int vm_builtin_SplDll_isEmpty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6380 | `{` |
|     1 |  6381 | `	SXUNUSED(nArg);` |
|     1 |  6382 | `	SXUNUSED(apArg);` |
|     3 |  6383 | `	ph7_result_bool(pCtx,DllCount(pCtx->pVm,PH7_ContextThis(pCtx)) == 0);` |
|     3 |  6384 | `	return PH7_OK;` |
|     1 |  6385 | `}` |
|    22 |  6386 | `static int vm_builtin_SplDll_setIteratorMode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6387 | `{` |
|    23 |  6388 | `	ph7_vm *pVm = pCtx->pVm;` |
|    23 |  6389 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    23 |  6390 | `	int iFlags = DllFlags(pThis);` |
|    23 |  6391 | `	sxi64 iMode = 0;` |
|     - |  6392 | `	sxi32 rc;` |
|    23 |  6393 | `	if( pThis == 0 ){` |
|   ! 0 |  6394 | `		return PH7_OK;` |
|     - |  6395 | `	}` |
|    23 |  6396 | `	if( nArg < 1 ){` |
|   ! 0 |  6397 | `		return PH7_OK;   /* the arity screen already refused */` |
|     - |  6398 | `	}` |
|    23 |  6399 | `	rc = PH7_IntArgResolve(pCtx,apArg[0],"SplDoublyLinkedList::setIteratorMode",1,"$mode","int",&iMode);` |
|    23 |  6400 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  6401 | `		return rc;` |
|     - |  6402 | `	}` |
|    23 |  6403 | `	if( (iFlags & DLL_IT_FIX) && (iFlags & DLL_IT_LIFO) != ((int)iMode & DLL_IT_LIFO) ){` |
|     7 |  6404 | `		return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     - |  6405 | `			"Iterators' LIFO/FIFO modes for SplStack/SplQueue objects are frozen");` |
|     - |  6406 | `	}` |
|     - |  6407 | `	/* php MASKS the value to the two mode bits and re-adds IT_FIX, so a nonsense` |
|     - |  6408 | `	 * mode is silently reduced rather than refused — and the ANSWER is the stored` |
|     - |  6409 | `	 * word, which is how a caller sees the fix bit at all. */` |
|    17 |  6410 | `	iFlags = ((int)iMode & DLL_IT_MASK) \| (iFlags & DLL_IT_FIX);` |
|    17 |  6411 | `	PH7_NativeSetAttrInt(pVm,pThis,DLL_FL,iFlags);` |
|    17 |  6412 | `	ph7_result_int64(pCtx,(ph7_int64)iFlags);` |
|    17 |  6413 | `	return PH7_OK;` |
|    12 |  6414 | `}` |
|    12 |  6415 | `static int vm_builtin_SplDll_getIteratorMode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6416 | `{` |
|     6 |  6417 | `	SXUNUSED(nArg);` |
|     6 |  6418 | `	SXUNUSED(apArg);` |
|    13 |  6419 | `	ph7_result_int64(pCtx,(ph7_int64)DllFlags(PH7_ContextThis(pCtx)));` |
|    13 |  6420 | `	return PH7_OK;` |
|     1 |  6421 | `}` |
|     6 |  6422 | `static int vm_builtin_SplDll_add(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6423 | `{` |
|     7 |  6424 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 |  6425 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     7 |  6426 | `	sxi64 nCount = DllCount(pVm,pThis);` |
|     7 |  6427 | `	sxi64 iIndex = 0;` |
|     - |  6428 | `	ph7_value sOff,sLen,sRep,*apExtra[3];` |
|     - |  6429 | `	ph7_hashmap *pRep;` |
|     - |  6430 | `	sxi32 rc;` |
|     7 |  6431 | `	if( nArg < 2 ){` |
|   ! 0 |  6432 | `		return PH7_OK;` |
|     - |  6433 | `	}` |
|     7 |  6434 | `	rc = DllIndexArg(pCtx,"add",apArg[0],&iIndex);` |
|     7 |  6435 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  6436 | `		return rc;` |
|     - |  6437 | `	}` |
|     7 |  6438 | `	if( iIndex < 0 \|\| iIndex > nCount ){` |
|     5 |  6439 | `		return DllOutOfRange(pCtx,"add");` |
|     - |  6440 | `	}` |
|     3 |  6441 | `	if( iIndex == nCount ){` |
|     - |  6442 | `		/* php: "the last entry + 1" is a push, because there is nothing to insert` |
|     - |  6443 | `		 * before. Note this is the LIST tail in both modes. */` |
|     - |  6444 | `		ph7_value *apOne[1];` |
|   ! 0 |  6445 | `		apOne[0] = apArg[1];` |
|   ! 0 |  6446 | `		DllArrayCall(pCtx,ph7_hashmap_push,apOne,1);` |
|   ! 0 |  6447 | `		ph7_result_null(pCtx);` |
|   ! 0 |  6448 | `		return PH7_OK;` |
|     - |  6449 | `	}` |
|     3 |  6450 | `	pRep = PH7_NewHashmap(pVm,0,0);` |
|     3 |  6451 | `	if( pRep == 0 ){` |
|   ! 0 |  6452 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  6453 | `	}` |
|     3 |  6454 | `	PH7_MemObjInit(pVm,&sRep);` |
|     3 |  6455 | `	sRep.x.pOther = pRep;` |
|     3 |  6456 | `	MemObjSetType(&sRep,MEMOBJ_HASHMAP);` |
|     3 |  6457 | `	PH7_HashmapInsert(pRep,0,apArg[1]);` |
|     3 |  6458 | `	PH7_MemObjInitFromInt(pVm,&sOff,DllOffsetToIndex(pThis,iIndex,nCount));` |
|     3 |  6459 | `	PH7_MemObjInitFromInt(pVm,&sLen,0);` |
|     3 |  6460 | `	apExtra[0] = &sOff;` |
|     3 |  6461 | `	apExtra[1] = &sLen;` |
|     3 |  6462 | `	apExtra[2] = &sRep;` |
|     3 |  6463 | `	DllArrayCall(pCtx,ph7_hashmap_splice,apExtra,3);` |
|     3 |  6464 | `	PH7_MemObjRelease(&sOff);` |
|     3 |  6465 | `	PH7_MemObjRelease(&sLen);` |
|     3 |  6466 | `	PH7_MemObjRelease(&sRep);` |
|     3 |  6467 | `	ph7_result_null(pCtx);   /* array_splice answers what it removed; add() is void */` |
|     3 |  6468 | `	return PH7_OK;` |
|     4 |  6469 | `}` |
|     6 |  6470 | `static int vm_builtin_SplDll_offsetExists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6471 | `{` |
|     7 |  6472 | `	sxi64 iIndex = 0;` |
|     - |  6473 | `	sxi32 rc;` |
|     7 |  6474 | `	if( nArg < 1 ){` |
|   ! 0 |  6475 | `		return PH7_OK;` |
|     - |  6476 | `	}` |
|     7 |  6477 | `	rc = DllIndexArg(pCtx,"offsetExists",apArg[0],&iIndex);` |
|     7 |  6478 | `	if( rc != SXRET_OK ){` |
|     3 |  6479 | `		return rc;` |
|     - |  6480 | `	}` |
|     5 |  6481 | `	ph7_result_bool(pCtx,iIndex >= 0 && iIndex < DllCount(pCtx->pVm,PH7_ContextThis(pCtx)));` |
|     5 |  6482 | `	return PH7_OK;` |
|     4 |  6483 | `}` |
|    22 |  6484 | `static int vm_builtin_SplDll_offsetGet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6485 | `{` |
|    23 |  6486 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    23 |  6487 | `	sxi64 nCount = DllCount(pCtx->pVm,pThis);` |
|    23 |  6488 | `	sxi64 iIndex = 0;` |
|     - |  6489 | `	ph7_value *pVal;` |
|     - |  6490 | `	sxi32 rc;` |
|    23 |  6491 | `	if( nArg < 1 ){` |
|   ! 0 |  6492 | `		return PH7_OK;` |
|     - |  6493 | `	}` |
|    23 |  6494 | `	rc = DllIndexArg(pCtx,"offsetGet",apArg[0],&iIndex);` |
|    23 |  6495 | `	if( rc != SXRET_OK ){` |
|     3 |  6496 | `		return rc;` |
|     - |  6497 | `	}` |
|    21 |  6498 | `	if( iIndex < 0 \|\| iIndex >= nCount ){` |
|     5 |  6499 | `		return DllOutOfRange(pCtx,"offsetGet");` |
|     - |  6500 | `	}` |
|    17 |  6501 | `	pVal = DllAt(pCtx->pVm,pThis,DllOffsetToIndex(pThis,iIndex,nCount));` |
|    17 |  6502 | `	if( pVal ){` |
|    17 |  6503 | `		ph7_result_value(pCtx,pVal);` |
|     8 |  6504 | `	}` |
|    17 |  6505 | `	return PH7_OK;` |
|    12 |  6506 | `}` |
|     6 |  6507 | `static int vm_builtin_SplDll_offsetSet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6508 | `{` |
|     7 |  6509 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 |  6510 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     7 |  6511 | `	sxi64 nCount = DllCount(pVm,pThis);` |
|     7 |  6512 | `	sxi64 iIndex = 0;` |
|     - |  6513 | `	ph7_hashmap *pMap;` |
|     7 |  6514 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  6515 | `	ph7_value sKey;` |
|     - |  6516 | `	sxi32 rc;` |
|     7 |  6517 | `	if( nArg < 2 ){` |
|   ! 0 |  6518 | `		return PH7_OK;` |
|     - |  6519 | `	}` |
|     7 |  6520 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|     - |  6521 | ``		/* php: a null offset is `$dll[] = v`, which pushes. */`` |
|     - |  6522 | `		ph7_value *apOne[1];` |
|   ! 0 |  6523 | `		apOne[0] = apArg[1];` |
|   ! 0 |  6524 | `		DllArrayCall(pCtx,ph7_hashmap_push,apOne,1);` |
|   ! 0 |  6525 | `		ph7_result_null(pCtx);` |
|   ! 0 |  6526 | `		return PH7_OK;` |
|     - |  6527 | `	}` |
|     7 |  6528 | `	rc = DllIndexArg(pCtx,"offsetSet",apArg[0],&iIndex);` |
|     7 |  6529 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  6530 | `		return rc;` |
|     - |  6531 | `	}` |
|     7 |  6532 | `	if( iIndex < 0 \|\| iIndex >= nCount ){` |
|     5 |  6533 | `		return DllOutOfRange(pCtx,"offsetSet");` |
|     - |  6534 | `	}` |
|     3 |  6535 | `	pMap = DllMap(pVm,pThis);` |
|     3 |  6536 | `	PH7_MemObjInitFromInt(pVm,&sKey,DllOffsetToIndex(pThis,iIndex,nCount));` |
|     3 |  6537 | `	if( pMap && PH7_HashmapLookup(pMap,&sKey,&pNode) == SXRET_OK ){` |
|     3 |  6538 | `		ph7_value *pDest = HashmapExtractNodeValue(pNode);` |
|     3 |  6539 | `		if( pDest ){` |
|     3 |  6540 | `			PH7_MemObjStore(apArg[1],pDest);` |
|     1 |  6541 | `		}` |
|     1 |  6542 | `	}` |
|     3 |  6543 | `	PH7_MemObjRelease(&sKey);` |
|     3 |  6544 | `	return PH7_OK;` |
|     4 |  6545 | `}` |
|     6 |  6546 | `static int vm_builtin_SplDll_offsetUnset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6547 | `{` |
|     7 |  6548 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 |  6549 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     7 |  6550 | `	sxi64 nCount = DllCount(pVm,pThis);` |
|     7 |  6551 | `	sxi64 iIndex = 0;` |
|     - |  6552 | `	ph7_value sOff,sLen,*apExtra[2];` |
|     - |  6553 | `	sxi32 rc;` |
|     7 |  6554 | `	if( nArg < 1 ){` |
|   ! 0 |  6555 | `		return PH7_OK;` |
|     - |  6556 | `	}` |
|     7 |  6557 | `	rc = DllIndexArg(pCtx,"offsetUnset",apArg[0],&iIndex);` |
|     7 |  6558 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  6559 | `		return rc;` |
|     - |  6560 | `	}` |
|     7 |  6561 | `	if( iIndex < 0 \|\| iIndex >= nCount ){` |
|     3 |  6562 | `		return DllOutOfRange(pCtx,"offsetUnset");` |
|     - |  6563 | `	}` |
|     5 |  6564 | `	PH7_MemObjInitFromInt(pVm,&sOff,DllOffsetToIndex(pThis,iIndex,nCount));` |
|     5 |  6565 | `	PH7_MemObjInitFromInt(pVm,&sLen,1);` |
|     5 |  6566 | `	apExtra[0] = &sOff;` |
|     5 |  6567 | `	apExtra[1] = &sLen;` |
|     5 |  6568 | `	DllArrayCall(pCtx,ph7_hashmap_splice,apExtra,2);` |
|     5 |  6569 | `	PH7_MemObjRelease(&sOff);` |
|     5 |  6570 | `	PH7_MemObjRelease(&sLen);` |
|     5 |  6571 | `	ph7_result_null(pCtx);` |
|     5 |  6572 | `	return PH7_OK;` |
|     4 |  6573 | `}` |
|     - |  6574 | `/*` |
|     - |  6575 | ` * The cursor. php's traverse_position IS the list index in both directions — a` |
|     - |  6576 | ` * LIFO rewind seeds count-1 and counts down — so current() and key() need no mode` |
|     - |  6577 | ` * test at all. IT_DELETE is the exception: in FIFO order php consumes the head and` |
|     - |  6578 | ` * leaves the position alone, so every element of a consuming walk reports key 0.` |
|     - |  6579 | ` */` |
|    24 |  6580 | `static int vm_builtin_SplDll_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6581 | `{` |
|    25 |  6582 | `	ph7_vm *pVm = pCtx->pVm;` |
|    25 |  6583 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    12 |  6584 | `	SXUNUSED(nArg);` |
|    12 |  6585 | `	SXUNUSED(apArg);` |
|    25 |  6586 | `	if( pThis == 0 ){` |
|   ! 0 |  6587 | `		return PH7_OK;` |
|     - |  6588 | `	}` |
|    37 |  6589 | `	PH7_NativeSetAttrInt(pVm,pThis,DLL_I,` |
|    24 |  6590 | `		(DllFlags(pThis) & DLL_IT_LIFO) ? DllCount(pVm,pThis)-1 : 0);` |
|    25 |  6591 | `	return PH7_OK;` |
|    13 |  6592 | `}` |
|    82 |  6593 | `static int vm_builtin_SplDll_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6594 | `{` |
|    83 |  6595 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    83 |  6596 | `	sxi64 iPos = pThis ? PH7_NativeAttrInt(pThis,DLL_I) : 0;` |
|    41 |  6597 | `	SXUNUSED(nArg);` |
|    41 |  6598 | `	SXUNUSED(apArg);` |
|    83 |  6599 | `	ph7_result_bool(pCtx,iPos >= 0 && iPos < DllCount(pCtx->pVm,pThis));` |
|    83 |  6600 | `	return PH7_OK;` |
|     1 |  6601 | `}` |
|    58 |  6602 | `static int vm_builtin_SplDll_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6603 | `{` |
|    59 |  6604 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    59 |  6605 | `	ph7_value *pVal = pThis ? DllAt(pCtx->pVm,pThis,PH7_NativeAttrInt(pThis,DLL_I)) : 0;` |
|    29 |  6606 | `	SXUNUSED(nArg);` |
|    29 |  6607 | `	SXUNUSED(apArg);` |
|    59 |  6608 | `	if( pVal ){` |
|    59 |  6609 | `		ph7_result_value(pCtx,pVal);` |
|    30 |  6610 | `	}else{` |
|   ! 0 |  6611 | `		ph7_result_null(pCtx);` |
|     - |  6612 | `	}` |
|    59 |  6613 | `	return PH7_OK;` |
|     1 |  6614 | `}` |
|    48 |  6615 | `static int vm_builtin_SplDll_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6616 | `{` |
|    49 |  6617 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    24 |  6618 | `	SXUNUSED(nArg);` |
|    24 |  6619 | `	SXUNUSED(apArg);` |
|    49 |  6620 | `	ph7_result_int64(pCtx,pThis ? PH7_NativeAttrInt(pThis,DLL_I) : 0);` |
|    49 |  6621 | `	return PH7_OK;` |
|     1 |  6622 | `}` |
|     - |  6623 | ``/* php's move_forward, with the direction flipped for prev() (its `flags ^ LIFO`). */`` |
|    58 |  6624 | `static int DllStep(ph7_context *pCtx,int bFlip)` |
|     1 |  6625 | `{` |
|    59 |  6626 | `	ph7_vm *pVm = pCtx->pVm;` |
|    59 |  6627 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  6628 | `	int iFlags;` |
|     - |  6629 | `	sxi64 iPos;` |
|    59 |  6630 | `	if( pThis == 0 ){` |
|   ! 0 |  6631 | `		return PH7_OK;` |
|     - |  6632 | `	}` |
|    59 |  6633 | `	iFlags = DllFlags(pThis);` |
|    59 |  6634 | `	if( bFlip ){` |
|   ! 0 |  6635 | `		iFlags ^= DLL_IT_LIFO;` |
|   ! 0 |  6636 | `	}` |
|    59 |  6637 | `	iPos = PH7_NativeAttrInt(pThis,DLL_I);` |
|    59 |  6638 | `	if( iPos < 0 \|\| iPos >= DllCount(pVm,pThis) ){` |
|     - |  6639 | `		/* php only steps a LIVE pointer; off the end nothing moves and nothing is` |
|     - |  6640 | `		 * consumed. The position still has to move for a plain walk, though, or` |
|     - |  6641 | `		 * prev() past the head could never come back. */` |
|   ! 0 |  6642 | `		if( (iFlags & DLL_IT_DELETE) == 0 ){` |
|   ! 0 |  6643 | `			PH7_NativeSetAttrInt(pVm,pThis,DLL_I,` |
|   ! 0 |  6644 | `				iPos + ((iFlags & DLL_IT_LIFO) ? -1 : 1));` |
|   ! 0 |  6645 | `		}` |
|   ! 0 |  6646 | `		return PH7_OK;` |
|     - |  6647 | `	}` |
|    59 |  6648 | `	if( iFlags & DLL_IT_DELETE ){` |
|    23 |  6649 | `		if( iFlags & DLL_IT_LIFO ){` |
|     7 |  6650 | `			DllArrayCall(pCtx,ph7_hashmap_pop,0,0);` |
|     7 |  6651 | `			PH7_NativeSetAttrInt(pVm,pThis,DLL_I,iPos-1);` |
|     4 |  6652 | `		}else{` |
|     - |  6653 | `			/* php consumes the head and does NOT advance: the walk stays at 0. */` |
|    17 |  6654 | `			DllArrayCall(pCtx,ph7_hashmap_shift,0,0);` |
|     - |  6655 | `		}` |
|    23 |  6656 | `		ph7_result_null(pCtx);` |
|    23 |  6657 | `		return PH7_OK;` |
|     - |  6658 | `	}` |
|    37 |  6659 | `	PH7_NativeSetAttrInt(pVm,pThis,DLL_I,iPos + ((iFlags & DLL_IT_LIFO) ? -1 : 1));` |
|    37 |  6660 | `	return PH7_OK;` |
|    30 |  6661 | `}` |
|    58 |  6662 | `static int vm_builtin_SplDll_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6663 | `{` |
|    29 |  6664 | `	SXUNUSED(nArg);` |
|    29 |  6665 | `	SXUNUSED(apArg);` |
|    59 |  6666 | `	return DllStep(pCtx,FALSE);` |
|     1 |  6667 | `}` |
|   ! 0 |  6668 | `static int vm_builtin_SplDll_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 |  6669 | `{` |
|   ! 0 |  6670 | `	SXUNUSED(nArg);` |
|   ! 0 |  6671 | `	SXUNUSED(apArg);` |
|   ! 0 |  6672 | `	return DllStep(pCtx,TRUE);` |
|   ! 0 |  6673 | `}` |
|     - |  6674 | `/*` |
|     - |  6675 | `` * php's get_debug_info: `flags` then `dllist`, and NOTHING for the (array) cast --`` |
|     - |  6676 | ` * the same var_dump/cast disagreement WeakReference has, which is why xPresent is` |
|     - |  6677 | `` * told which surface is asking. `__debugInfo()` is the same array, reachable by`` |
|     - |  6678 | ` * name because php declares it.` |
|     - |  6679 | ` *` |
|     - |  6680 | ` * Every key is php's MANGLED private name, which is what makes the dump print` |
|     - |  6681 | ``  * `[flags:SplDoublyLinkedList:private]` rather than a plain `[flags]` `` |
|     - |  6682 | ` * (SplObjectStorage's storage key has read that way all along).` |
|     - |  6683 | ` */` |
|     - |  6684 | `/*` |
|     - |  6685 | `` * One `"\0Class\0member"` key, under the class php says DECLARES the slot -- which`` |
|     - |  6686 | ` * for every container here is the ROOT of the chain, so SplStack, SplQueue,` |
|     - |  6687 | ` * SplMinHeap and any userland subclass all show the base's name rather than their` |
|     - |  6688 | ` * own, and SplPriorityQueue (which extends nothing) shows itself.` |
|     - |  6689 | ` */` |
|   112 |  6690 | `static sxi32 SplRootDebugKey(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pKey,` |
|     - |  6691 | `	const char *zMember)` |
|     1 |  6692 | `{` |
|   113 |  6693 | `	ph7_class *pRoot = pThis->pClass;` |
|   215 |  6694 | `	while( pRoot->pBase ){` |
|   103 |  6695 | `		pRoot = pRoot->pBase;` |
|     1 |  6696 | `	}` |
|   113 |  6697 | `	PH7_MemObjInitFromString(pVm,pKey,0);` |
|   113 |  6698 | `	PH7_MemObjStringAppend(pKey,"\0",1);` |
|   113 |  6699 | `	PH7_MemObjStringAppend(pKey,SyStringData(&pRoot->sName),SyStringLength(&pRoot->sName));` |
|   113 |  6700 | `	PH7_MemObjStringAppend(pKey,"\0",1);` |
|   113 |  6701 | `	PH7_MemObjStringAppend(pKey,zMember,(sxu32)SyStrlen(zMember));` |
|   113 |  6702 | `	return PH7_OK;` |
|     1 |  6703 | `}` |
|    26 |  6704 | `static sxi32 DllFillDebug(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)` |
|     1 |  6705 | `{` |
|     - |  6706 | `	ph7_value sKey,sVal,*pStore;` |
|    27 |  6707 | `	SplRootDebugKey(pVm,pThis,&sKey,"flags");` |
|    27 |  6708 | `	PH7_MemObjInitFromInt(pVm,&sVal,DllFlags(pThis));` |
|    27 |  6709 | `	ph7_array_add_elem(pOut,&sKey,&sVal);` |
|    27 |  6710 | `	PH7_MemObjRelease(&sKey);` |
|    27 |  6711 | `	PH7_MemObjRelease(&sVal);` |
|    27 |  6712 | `	pStore = DllSlot(pVm,pThis);` |
|    27 |  6713 | `	SplRootDebugKey(pVm,pThis,&sKey,"dllist");` |
|    27 |  6714 | `	if( pStore ){` |
|    27 |  6715 | `		ph7_array_add_elem(pOut,&sKey,pStore);` |
|    13 |  6716 | `	}` |
|    27 |  6717 | `	PH7_MemObjRelease(&sKey);` |
|    27 |  6718 | `	return PH7_OK;` |
|     1 |  6719 | `}` |
|    26 |  6720 | `static sxi32 DllPresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|     1 |  6721 | `{` |
|    27 |  6722 | `	if( !bDebug ){` |
|    11 |  6723 | `		return PH7_OK;   /* php's (array) cast and var_export show nothing */` |
|     - |  6724 | `	}` |
|    17 |  6725 | `	return DllFillDebug(pVm,pThis,pOut);` |
|    14 |  6726 | `}` |
|    10 |  6727 | `static int vm_builtin_SplDll_debugInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6728 | `{` |
|    11 |  6729 | `	ph7_vm *pVm = pCtx->pVm;` |
|    11 |  6730 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  6731 | `	ph7_value sOut;` |
|     5 |  6732 | `	SXUNUSED(nArg);` |
|     5 |  6733 | `	SXUNUSED(apArg);` |
|    11 |  6734 | `	PH7_MemObjInit(pVm,&sOut);` |
|    11 |  6735 | `	if( PH7_MemObjToHashmap(&sOut) != SXRET_OK ){` |
|   ! 0 |  6736 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 |  6737 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  6738 | `	}` |
|    11 |  6739 | `	DllFillDebug(pVm,pThis,&sOut);` |
|    11 |  6740 | `	ph7_result_value(pCtx,&sOut);` |
|    11 |  6741 | `	PH7_MemObjRelease(&sOut);` |
|    11 |  6742 | `	return PH7_OK;` |
|     6 |  6743 | `}` |
|     - |  6744 | `/*` |
|     - |  6745 | ` * php's __serialize(): [flags, elements, dynamic members]. This is what` |
|     - |  6746 | ` * serialize() actually uses -- the Serializable pair below exists because the` |
|     - |  6747 | ` * interface is still declared, and php words its own legacy format there.` |
|     - |  6748 | ` */` |
|    18 |  6749 | `static int vm_builtin_SplDll_serializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6750 | `{` |
|    19 |  6751 | `	ph7_vm *pVm = pCtx->pVm;` |
|    19 |  6752 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  6753 | `	ph7_value sOut,sVal,*pStore;` |
|     9 |  6754 | `	SXUNUSED(nArg);` |
|     9 |  6755 | `	SXUNUSED(apArg);` |
|    19 |  6756 | `	PH7_MemObjInit(pVm,&sOut);` |
|    19 |  6757 | `	if( PH7_MemObjToHashmap(&sOut) != SXRET_OK ){` |
|   ! 0 |  6758 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 |  6759 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  6760 | `	}` |
|    19 |  6761 | `	PH7_MemObjInitFromInt(pVm,&sVal,DllFlags(pThis));` |
|    19 |  6762 | `	ph7_array_add_elem(&sOut,0,&sVal);` |
|    19 |  6763 | `	PH7_MemObjRelease(&sVal);` |
|    19 |  6764 | `	pStore = DllSlot(pVm,pThis);` |
|    19 |  6765 | `	if( pStore ){` |
|    19 |  6766 | `		ph7_array_add_elem(&sOut,0,pStore);` |
|     9 |  6767 | `	}` |
|     - |  6768 | `	/* The members slot: php's own properties — empty for a bare SplStack, a` |
|     - |  6769 | `	 * SUBCLASS's declared slots when there is one. */` |
|    19 |  6770 | `	if( SplMembersOf(pVm,pThis,&sVal) == SXRET_OK ){` |
|    19 |  6771 | `		ph7_array_add_elem(&sOut,0,&sVal);` |
|     9 |  6772 | `	}` |
|    19 |  6773 | `	PH7_MemObjRelease(&sVal);` |
|    19 |  6774 | `	ph7_result_value(pCtx,&sOut);` |
|    19 |  6775 | `	PH7_MemObjRelease(&sOut);` |
|    19 |  6776 | `	return PH7_OK;` |
|    10 |  6777 | `}` |
|     6 |  6778 | `static int vm_builtin_SplDll_unserializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6779 | `{` |
|     7 |  6780 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 |  6781 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  6782 | `	ph7_hashmap *pData;` |
|     7 |  6783 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  6784 | `	ph7_value *pFlags,*pStore,*pSlot;` |
|     7 |  6785 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 \|\| pThis == 0 ){` |
|   ! 0 |  6786 | `		return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - |  6787 | `			"Incomplete or ill-typed serialization data");` |
|     - |  6788 | `	}` |
|     7 |  6789 | `	pData = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     7 |  6790 | `	if( HashmapLookupIntKey(pData,0,&pNode) != SXRET_OK ){` |
|   ! 0 |  6791 | `		return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - |  6792 | `			"Incomplete or ill-typed serialization data");` |
|     - |  6793 | `	}` |
|     7 |  6794 | `	pFlags = HashmapExtractNodeValue(pNode);` |
|     6 |  6795 | `	if( pFlags == 0 \|\| (pFlags->iFlags & MEMOBJ_INT) == 0` |
|     7 |  6796 | `	 \|\| HashmapLookupIntKey(pData,1,&pNode) != SXRET_OK ){` |
|   ! 0 |  6797 | `		return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - |  6798 | `			"Incomplete or ill-typed serialization data");` |
|     - |  6799 | `	}` |
|     7 |  6800 | `	pStore = HashmapExtractNodeValue(pNode);` |
|     7 |  6801 | `	if( pStore == 0 \|\| (pStore->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 |  6802 | `		return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - |  6803 | `			"Incomplete or ill-typed serialization data");` |
|     - |  6804 | `	}` |
|     7 |  6805 | `	PH7_NativeSetAttrInt(pVm,pThis,DLL_FL,ph7_value_to_int64(pFlags));` |
|     7 |  6806 | `	pSlot = PH7_NativeAttr(pThis,DLL_Q);` |
|     7 |  6807 | `	if( pSlot ){` |
|     7 |  6808 | `		PH7_MemObjRelease(pSlot);` |
|     7 |  6809 | `		PH7_MemObjStore(pStore,pSlot);` |
|     3 |  6810 | `	}` |
|     7 |  6811 | `	if( HashmapLookupIntKey(pData,2,&pNode) == SXRET_OK ){` |
|     7 |  6812 | `		SplMembersLoad(pThis,HashmapExtractNodeValue(pNode));` |
|     3 |  6813 | `	}` |
|     7 |  6814 | `	return PH7_OK;` |
|     4 |  6815 | `}` |
|     - |  6816 | `/*` |
|     - |  6817 | ` * php's Serializable pair, kept because the interface is still declared: the` |
|     - |  6818 | ` * format is the serialized FLAGS followed by one ':' + serialized value per` |
|     - |  6819 | ` * element ("i:0;:i:1;:i:2;"), which nothing else in php produces or reads.` |
|     - |  6820 | ` */` |
|     - |  6821 | `/*` |
|     - |  6822 | ` * One serialized value, appended to a blob. The RESET is the point: the engine's` |
|     - |  6823 | ` * serialize() writes through ph7_value_string, which APPENDS to the context's` |
|     - |  6824 | ` * return slot rather than replacing it, so a loop that calls it per element` |
|     - |  6825 | ` * accumulates every previous answer into the next one. Shared with` |
|     - |  6826 | ` * SplObjectStorage's legacy format, which is built the same way.` |
|     - |  6827 | ` */` |
|    56 |  6828 | `static void SplSerializeInto(ph7_context *pCtx,ph7_value **apCall,SyBlob *pOut)` |
|     1 |  6829 | `{` |
|    57 |  6830 | `	int nLen = 0;` |
|     - |  6831 | `	const char *zTxt;` |
|    57 |  6832 | `	if( pCtx->pRet ){` |
|    57 |  6833 | `		PH7_MemObjRelease(pCtx->pRet);` |
|    28 |  6834 | `	}` |
|    57 |  6835 | `	vm_builtin_serialize(pCtx,1,apCall);` |
|    57 |  6836 | `	if( pCtx->pRet == 0 ){` |
|   ! 0 |  6837 | `		return;` |
|     - |  6838 | `	}` |
|    57 |  6839 | `	zTxt = ph7_value_to_string(pCtx->pRet,&nLen);` |
|    57 |  6840 | `	SyBlobAppend(pOut,zTxt,(sxu32)nLen);` |
|    29 |  6841 | `}` |
|     2 |  6842 | `static int vm_builtin_SplDll_serialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6843 | `{` |
|     3 |  6844 | `	ph7_vm *pVm = pCtx->pVm;` |
|     3 |  6845 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     3 |  6846 | `	ph7_hashmap *pMap = DllMap(pVm,pThis);` |
|     - |  6847 | `	ph7_hashmap_node *pNode;` |
|     - |  6848 | `	SyBlob sOut;` |
|     - |  6849 | `	ph7_value sFlags,*apCall[1];` |
|     3 |  6850 | `	sxi64 n,nCount = pMap ? (sxi64)pMap->nEntry : 0;` |
|     1 |  6851 | `	SXUNUSED(nArg);` |
|     1 |  6852 | `	SXUNUSED(apArg);` |
|     3 |  6853 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|     3 |  6854 | `	PH7_MemObjInitFromInt(pVm,&sFlags,DllFlags(pThis));` |
|     3 |  6855 | `	apCall[0] = &sFlags;` |
|     3 |  6856 | `	SplSerializeInto(pCtx,apCall,&sOut);` |
|     3 |  6857 | `	PH7_MemObjRelease(&sFlags);` |
|     9 |  6858 | `	for( n = 0 ; n < nCount ; ++n ){` |
|     - |  6859 | `		ph7_value *pVal;` |
|     7 |  6860 | `		pNode = 0;` |
|     7 |  6861 | `		if( HashmapLookupIntKey(pMap,n,&pNode) != SXRET_OK ){` |
|   ! 0 |  6862 | `			continue;` |
|     - |  6863 | `		}` |
|     7 |  6864 | `		pVal = HashmapExtractNodeValue(pNode);` |
|     7 |  6865 | `		if( pVal == 0 ){` |
|   ! 0 |  6866 | `			continue;` |
|     - |  6867 | `		}` |
|     7 |  6868 | `		apCall[0] = pVal;` |
|     7 |  6869 | `		SyBlobAppend(&sOut,":",1);` |
|     7 |  6870 | `		SplSerializeInto(pCtx,apCall,&sOut);` |
|     4 |  6871 | `	}` |
|     - |  6872 | `	/* ph7_result_string APPENDS too, and pRet still holds the LAST element's` |
|     - |  6873 | `	 * serialization from the loop above — drop it before writing the answer. */` |
|     3 |  6874 | `	if( pCtx->pRet ){` |
|     3 |  6875 | `		PH7_MemObjRelease(pCtx->pRet);` |
|     1 |  6876 | `	}` |
|     3 |  6877 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     3 |  6878 | `	SyBlobRelease(&sOut);` |
|     3 |  6879 | `	return PH7_OK;` |
|     1 |  6880 | `}` |
|   ! 0 |  6881 | `static int vm_builtin_SplDll_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 |  6882 | `{` |
|   ! 0 |  6883 | `	ph7_vm *pVm = pCtx->pVm;` |
|   ! 0 |  6884 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  6885 | `	const char *zData,*zCur,*zEnd;` |
|   ! 0 |  6886 | `	int nData = 0;` |
|   ! 0 |  6887 | `	int bFirst = 1;` |
|     - |  6888 | `	ph7_value *pSlot;` |
|     - |  6889 | `	ph7_hashmap *pMap;` |
|   ! 0 |  6890 | `	if( nArg < 1 \|\| pThis == 0 ){` |
|   ! 0 |  6891 | `		return PH7_OK;` |
|     - |  6892 | `	}` |
|   ! 0 |  6893 | `	zData = ph7_value_to_string(apArg[0],&nData);` |
|   ! 0 |  6894 | `	if( nData < 1 ){` |
|   ! 0 |  6895 | `		return PH7_OK;   /* php returns without touching the list */` |
|     - |  6896 | `	}` |
|   ! 0 |  6897 | `	pSlot = DllSlot(pVm,pThis);` |
|   ! 0 |  6898 | `	pMap = pSlot ? (ph7_hashmap *)pSlot->x.pOther : 0;` |
|   ! 0 |  6899 | `	if( pMap == 0 ){` |
|   ! 0 |  6900 | `		return PH7_OK;` |
|     - |  6901 | `	}` |
|     - |  6902 | `	/* php empties the list first, then reads the flags, then one ':'-prefixed` |
|     - |  6903 | `	 * value per element. A malformed tail is an UnexpectedValueException naming` |
|     - |  6904 | `	 * the byte offset — reproduced here from the same position arithmetic. */` |
|   ! 0 |  6905 | `	while( pMap->pFirst ){` |
|   ! 0 |  6906 | `		PH7_HashmapUnlinkNode(pMap->pFirst,TRUE);` |
|   ! 0 |  6907 | `	}` |
|   ! 0 |  6908 | `	zCur = zData;` |
|   ! 0 |  6909 | `	zEnd = &zData[nData];` |
|   ! 0 |  6910 | `	while( zCur < zEnd ){` |
|     - |  6911 | `		ph7_value sPart,sRes,*apCall[1];` |
|     - |  6912 | `		int nPart;` |
|   ! 0 |  6913 | `		const char *zStop = zCur;` |
|   ! 0 |  6914 | `		if( !bFirst ){` |
|   ! 0 |  6915 | `			if( zCur[0] != ':' ){` |
|   ! 0 |  6916 | `				break;` |
|     - |  6917 | `			}` |
|   ! 0 |  6918 | `			zCur++;` |
|   ! 0 |  6919 | `		}` |
|     - |  6920 | `		/* One serialized scalar reaches up to and including its ';'. */` |
|   ! 0 |  6921 | `		while( zStop < zEnd && zStop[0] != ';' ){` |
|   ! 0 |  6922 | `			zStop++;` |
|   ! 0 |  6923 | `		}` |
|   ! 0 |  6924 | `		if( zStop >= zEnd ){` |
|   ! 0 |  6925 | `			zStop = zEnd;` |
|   ! 0 |  6926 | `		}else{` |
|   ! 0 |  6927 | `			zStop++;` |
|     - |  6928 | `		}` |
|   ! 0 |  6929 | `		nPart = (int)(zStop - zCur);` |
|   ! 0 |  6930 | `		if( nPart <= 0 ){` |
|   ! 0 |  6931 | `			break;` |
|     - |  6932 | `		}` |
|   ! 0 |  6933 | `		PH7_MemObjInitFromString(pVm,&sPart,0);` |
|   ! 0 |  6934 | `		PH7_MemObjStringAppend(&sPart,zCur,(sxu32)nPart);` |
|   ! 0 |  6935 | `		apCall[0] = &sPart;` |
|   ! 0 |  6936 | `		PH7_MemObjInit(pVm,&sRes);` |
|   ! 0 |  6937 | `		if( pCtx->pRet ){` |
|   ! 0 |  6938 | `			PH7_MemObjRelease(pCtx->pRet);   /* see DllSerializeInto: pRet is appended to */` |
|   ! 0 |  6939 | `		}` |
|   ! 0 |  6940 | `		vm_builtin_unserialize(pCtx,1,apCall);` |
|   ! 0 |  6941 | `		if( pCtx->pRet ){` |
|   ! 0 |  6942 | `			PH7_MemObjStore(pCtx->pRet,&sRes);` |
|   ! 0 |  6943 | `		}` |
|   ! 0 |  6944 | `		if( bFirst ){` |
|   ! 0 |  6945 | `			PH7_NativeSetAttrInt(pVm,pThis,DLL_FL,ph7_value_to_int64(&sRes));` |
|   ! 0 |  6946 | `			bFirst = 0;` |
|   ! 0 |  6947 | `		}else{` |
|   ! 0 |  6948 | `			PH7_HashmapInsert(pMap,0,&sRes);` |
|     - |  6949 | `		}` |
|   ! 0 |  6950 | `		PH7_MemObjRelease(&sPart);` |
|   ! 0 |  6951 | `		PH7_MemObjRelease(&sRes);` |
|   ! 0 |  6952 | `		zCur = zStop;` |
|   ! 0 |  6953 | `	}` |
|   ! 0 |  6954 | `	ph7_result_null(pCtx);` |
|   ! 0 |  6955 | `	return PH7_OK;` |
|   ! 0 |  6956 | `}` |
|     - |  6957 | `/*` |
|     - |  6958 | ` * The declaration. Method ORDER is spl_dllist.stub.php's, php declares NO` |
|     - |  6959 | ` * constructor for any of the three, and the IT_FIX bit is a per-class DEFAULT on` |
|     - |  6960 | ` * the flags slot -- which is exactly how php does it (the create handler stamps` |
|     - |  6961 | ` * the flags; there is no constructor to run).` |
|     - |  6962 | ` */` |
|  6721 |  6963 | `static sxi32 VmInstallSplDllist(ph7_vm *pVm)` |
|     5 |  6964 | `{` |
|     - |  6965 | `	static const PH7_NativeMethodDef aDllMethod[] = {` |
|     - |  6966 | `		{ "add",             PH7_MOD_PUBLIC, "int $index, mixed $value", "@void",` |
|     - |  6967 | `		  vm_builtin_SplDll_add },` |
|     - |  6968 | `		{ "pop",             PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplDll_pop },` |
|     - |  6969 | `		{ "shift",           PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplDll_shift },` |
|     - |  6970 | `		{ "push",            PH7_MOD_PUBLIC, "mixed $value", "@void", vm_builtin_SplDll_push },` |
|     - |  6971 | `		{ "unshift",         PH7_MOD_PUBLIC, "mixed $value", "@void", vm_builtin_SplDll_unshift },` |
|     - |  6972 | `		{ "top",             PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplDll_top },` |
|     - |  6973 | `		{ "bottom",          PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplDll_bottom },` |
|     - |  6974 | `		{ "__debugInfo",     PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplDll_debugInfo },` |
|     - |  6975 | `		{ "count",           PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplDll_count },` |
|     - |  6976 | `		{ "isEmpty",         PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplDll_isEmpty },` |
|     - |  6977 | `		{ "setIteratorMode", PH7_MOD_PUBLIC, "int $mode", "@int",` |
|     - |  6978 | `		  vm_builtin_SplDll_setIteratorMode },` |
|     - |  6979 | `		{ "getIteratorMode", PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplDll_getIteratorMode },` |
|     - |  6980 | ``		/* php's stub leaves these four offsets UNTYPED (a `@param int` docblock, which`` |
|     - |  6981 | `		 * Reflection does not print) while the ZPP enforces int -- so the signature says` |
|     - |  6982 | `		 * nothing and each body runs PH7_IntArgResolve itself. */` |
|     - |  6983 | `		{ "offsetExists",    PH7_MOD_PUBLIC, "$index", "@bool", vm_builtin_SplDll_offsetExists },` |
|     - |  6984 | `		{ "offsetGet",       PH7_MOD_PUBLIC, "$index", "@mixed", vm_builtin_SplDll_offsetGet },` |
|     - |  6985 | `		{ "offsetSet",       PH7_MOD_PUBLIC, "$index, mixed $value", "@void",` |
|     - |  6986 | `		  vm_builtin_SplDll_offsetSet },` |
|     - |  6987 | `		{ "offsetUnset",     PH7_MOD_PUBLIC, "$index", "@void", vm_builtin_SplDll_offsetUnset },` |
|     - |  6988 | `		{ "rewind",          PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplDll_rewind },` |
|     - |  6989 | `		{ "current",         PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplDll_current },` |
|     - |  6990 | `		{ "key",             PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplDll_key },` |
|     - |  6991 | `		{ "prev",            PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplDll_prev },` |
|     - |  6992 | `		{ "next",            PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplDll_next },` |
|     - |  6993 | `		{ "valid",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplDll_valid },` |
|     - |  6994 | `		{ "unserialize",     PH7_MOD_PUBLIC, "string $data", "@void",` |
|     - |  6995 | `		  vm_builtin_SplDll_unserialize },` |
|     - |  6996 | `		{ "serialize",       PH7_MOD_PUBLIC, "", "@string", vm_builtin_SplDll_serialize },` |
|     - |  6997 | `		{ "__serialize",     PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplDll_serializeMagic },` |
|     - |  6998 | `		{ "__unserialize",   PH7_MOD_PUBLIC, "array $data", "@void",` |
|     - |  6999 | `		  vm_builtin_SplDll_unserializeMagic },` |
|     - |  7000 | `	};` |
|     - |  7001 | `	static const PH7_NativeMethodDef aQueueMethod[] = {` |
|     - |  7002 | `		/* php's @implementation-alias: the same C bodies under the queue's names. */` |
|     - |  7003 | `		{ "enqueue", PH7_MOD_PUBLIC, "mixed $value", "@void", vm_builtin_SplDll_push },` |
|     - |  7004 | `		{ "dequeue", PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplDll_shift },` |
|     - |  7005 | `	};` |
|     - |  7006 | `	static const PH7_NativeConstDef aDllConst[] = {` |
|     - |  7007 | `		{ "IT_MODE_LIFO",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, DLL_IT_LIFO, 0, 0.0 },` |
|     - |  7008 | `		{ "IT_MODE_FIFO",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 0, 0, 0.0 },` |
|     - |  7009 | `		{ "IT_MODE_DELETE", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, DLL_IT_DELETE, 0, 0.0 },` |
|     - |  7010 | `		{ "IT_MODE_KEEP",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 0, 0, 0.0 },` |
|     - |  7011 | `	};` |
|     - |  7012 | `	static const PH7_NativePropDef aDllProp[] = {` |
|     - |  7013 | `		{ DLL_Q,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  7014 | `		{ DLL_FL, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  7015 | `		{ DLL_I,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  7016 | `	};` |
|     - |  7017 | `	/* php's object handler stamps IT_FIX (and LIFO for a stack) at CREATION, which` |
|     - |  7018 | `	 * is why neither subclass declares a constructor and why the bit survives every` |
|     - |  7019 | `	 * setIteratorMode(). A per-class default on the flags slot says the same thing. */` |
|     - |  7020 | `	static const PH7_NativePropDef aQueueProp[] = {` |
|     - |  7021 | `		{ DLL_FL, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN,` |
|     - |  7022 | `		  { 0, 0, PH7_NATIVE_VAL_INT, DLL_IT_FIX, 0, 0.0 }, 0 },` |
|     - |  7023 | `	};` |
|     - |  7024 | `	static const PH7_NativePropDef aStackProp[] = {` |
|     - |  7025 | `		{ DLL_FL, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN,` |
|     - |  7026 | `		  { 0, 0, PH7_NATIVE_VAL_INT, DLL_IT_FIX\|DLL_IT_LIFO, 0, 0.0 }, 0 },` |
|     - |  7027 | `	};` |
|     - |  7028 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - |  7029 | `		{ "SplDoublyLinkedList", 0, "Iterator,Countable,ArrayAccess,Serializable", 0,` |
|     - |  7030 | `		  aDllMethod, SX_ARRAYSIZE(aDllMethod),` |
|     - |  7031 | `		  aDllConst, SX_ARRAYSIZE(aDllConst),` |
|     - |  7032 | `		  aDllProp, SX_ARRAYSIZE(aDllProp), 0, 0, DllPresent },` |
|     - |  7033 | `		{ "SplQueue", "SplDoublyLinkedList", 0, 0,` |
|     - |  7034 | `		  aQueueMethod, SX_ARRAYSIZE(aQueueMethod), 0, 0,` |
|     - |  7035 | `		  aQueueProp, SX_ARRAYSIZE(aQueueProp), 0, 0, DllPresent },` |
|     - |  7036 | `		{ "SplStack", "SplDoublyLinkedList", 0, 0,` |
|     - |  7037 | `		  0, 0, 0, 0,` |
|     - |  7038 | `		  aStackProp, SX_ARRAYSIZE(aStackProp), 0, 0, DllPresent },` |
|     - |  7039 | `	};` |
|  6726 |  7040 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 |  7041 | `}` |
|     - |  7042 | `/*` |
|     - |  7043 | ` * ---------------------------------------------------------------------------` |
|     - |  7044 | ` * SplHeap, SplMinHeap, SplMaxHeap and SplPriorityQueue.` |
|     - |  7045 | ` *` |
|     - |  7046 | `` * php's `spl_heap_object` is an array plus a FLAGS word, and the flags carry the`` |
|     - |  7047 | ` * thing the chunk could not express at all: **SPL_HEAP_CORRUPTED**. php sets it` |
|     - |  7048 | ` * when an exception escapes the user's compare() mid-sift -- the heap invariant is` |
|     - |  7049 | ` * then unknown -- and every operation that DEPENDS on the invariant refuses with` |
|     - |  7050 | ` * "Heap is corrupted, heap properties are no longer ensured." until` |
|     - |  7051 | `` * recoverFromCorruption() clears it. The chunk hardcoded `isCorrupted()` to false`` |
|     - |  7052 | `` * and `recoverFromCorruption()` to true, so a throwing comparator left a silently`` |
|     - |  7053 | ` * mis-ordered heap that kept answering.` |
|     - |  7054 | ` *` |
|     - |  7055 | ` * Which operations refuse is not guessable and was mapped against the oracle:` |
|     - |  7056 | ` * insert, extract, top, next and __serialize/__unserialize refuse; count,` |
|     - |  7057 | ` * isEmpty, rewind, valid, current, key, isCorrupted, recoverFromCorruption and` |
|     - |  7058 | `` * __debugInfo all keep working. (A `foreach` refuses because it reaches next().)`` |
|     - |  7059 | ` *` |
|     - |  7060 | ` * php's priority-queue node is exactly {data, priority} -- the chunk carried a` |
|     - |  7061 | `` * third field, a descending `__serial` it never compared with, which leaked into`` |
|     - |  7062 | ` * serialize(), var_dump() and the (array) cast as a nonsense PHP_INT_MAX-relative` |
|     - |  7063 | ` * integer. It is gone; equal priorities keep the order php's strictly-greater` |
|     - |  7064 | ` * swap gives them.` |
|     - |  7065 | ` *` |
|     - |  7066 | ` * The other three the chunk lacked, the same three the SplDoublyLinkedList` |
|     - |  7067 | `` * conversion lacked: `__debugInfo()` (flags / isCorrupted / heap, and for the queue`` |
|     - |  7068 | ` * the heap entries are rendered EXTR_BOTH-style whatever the extract flags say),` |
|     - |  7069 | `` * and the `__serialize()`/`__unserialize()` pair, whose payload is`` |
|     - |  7070 | ` * [members, {flags, heap_elements}] and whose reader VALIDATES -- a plain heap` |
|     - |  7071 | ` * refuses a non-zero flags word, the queue refuses a zero one.` |
|     - |  7072 | ` */` |
|     - |  7073 | `#define HP_H  "__h"   /* the heap array, in heap order */` |
|     - |  7074 | `#define HP_FL "__fl"  /* php's intern->flags: the queue's EXTR bits, 0 for a heap */` |
|     - |  7075 | `#define HP_CR "__cr"  /* php's SPL_HEAP_CORRUPTED */` |
|     - |  7076 |  |
|     - |  7077 | `#define PQ_EXTR_DATA     1` |
|     - |  7078 | `#define PQ_EXTR_PRIORITY 2` |
|     - |  7079 | `#define PQ_EXTR_BOTH     3` |
|     - |  7080 | `#define PQ_EXTR_MASK     3` |
|     - |  7081 |  |
|  1918 |  7082 | `static ph7_value * HeapSlot(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  7083 | `{` |
|  1919 |  7084 | `	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,HP_H) : 0;` |
|  1919 |  7085 | `	if( pSlot == 0 ){` |
|   ! 0 |  7086 | `		return 0;` |
|     - |  7087 | `	}` |
|  1919 |  7088 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    93 |  7089 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|   ! 0 |  7090 | `			return 0;` |
|     - |  7091 | `		}` |
|    46 |  7092 | `	}` |
|  1919 |  7093 | `	if( PH7_HashmapCowSeparate(pVm,pSlot) == 0 ){` |
|   ! 0 |  7094 | `		return 0;` |
|     - |  7095 | `	}` |
|     - |  7096 | `	/* Re-ask: the separation can MOVE the memobj pool this slot lives in` |
|     - |  7097 | `	 * (SplStoreSlot explains it). */` |
|  1919 |  7098 | `	return PH7_NativeAttr(pThis,HP_H);` |
|   960 |  7099 | `}` |
|  1880 |  7100 | `static ph7_hashmap * HeapMap(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  7101 | `{` |
|  1881 |  7102 | `	ph7_value *pSlot = HeapSlot(pVm,pThis);` |
|  1881 |  7103 | `	return pSlot ? (ph7_hashmap *)pSlot->x.pOther : 0;` |
|     1 |  7104 | `}` |
|   592 |  7105 | `static sxi64 HeapCount(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  7106 | `{` |
|   593 |  7107 | `	ph7_hashmap *pMap = HeapMap(pVm,pThis);` |
|   593 |  7108 | `	return pMap ? (sxi64)pMap->nEntry : 0;` |
|     1 |  7109 | `}` |
|     - |  7110 | `/* Re-resolved on every use: any call into the user's compare() may have rewritten` |
|     - |  7111 | ` * the heap's own storage under us (rule 47). It also used to move the pool, which` |
|     - |  7112 | ` * P1's fixed segments took care of. */` |
|   754 |  7113 | `static ph7_value * HeapAt(ph7_vm *pVm,ph7_class_instance *pThis,sxi64 i)` |
|     1 |  7114 | `{` |
|   755 |  7115 | `	ph7_hashmap *pMap = HeapMap(pVm,pThis);` |
|   755 |  7116 | `	ph7_hashmap_node *pNode = 0;` |
|   755 |  7117 | `	if( pMap == 0 \|\| HashmapLookupIntKey(pMap,i,&pNode) != SXRET_OK ){` |
|   ! 0 |  7118 | `		return 0;` |
|     - |  7119 | `	}` |
|   755 |  7120 | `	return HashmapExtractNodeValue(pNode);` |
|   378 |  7121 | `}` |
|   224 |  7122 | `static void HeapPut(ph7_vm *pVm,ph7_class_instance *pThis,sxi64 i,ph7_value *pVal)` |
|     1 |  7123 | `{` |
|   225 |  7124 | `	ph7_hashmap *pMap = HeapMap(pVm,pThis);` |
|     - |  7125 | `	ph7_value sKey;` |
|   225 |  7126 | `	if( pMap == 0 ){` |
|   ! 0 |  7127 | `		return;` |
|     - |  7128 | `	}` |
|   225 |  7129 | `	PH7_MemObjInitFromInt(pVm,&sKey,i);` |
|   225 |  7130 | `	PH7_HashmapInsert(pMap,&sKey,pVal);` |
|   225 |  7131 | `	PH7_MemObjRelease(&sKey);` |
|   113 |  7132 | `}` |
|    82 |  7133 | `static void HeapSwap(ph7_vm *pVm,ph7_class_instance *pThis,sxi64 i,sxi64 j)` |
|     1 |  7134 | `{` |
|     - |  7135 | `	ph7_value sI,sJ,*pV;` |
|    83 |  7136 | `	PH7_MemObjInit(pVm,&sI);` |
|    83 |  7137 | `	PH7_MemObjInit(pVm,&sJ);` |
|    83 |  7138 | `	pV = HeapAt(pVm,pThis,i);` |
|    83 |  7139 | `	if( pV ){` |
|    83 |  7140 | `		PH7_MemObjStore(pV,&sI);` |
|    41 |  7141 | `	}` |
|    83 |  7142 | `	pV = HeapAt(pVm,pThis,j);` |
|    83 |  7143 | `	if( pV ){` |
|    83 |  7144 | `		PH7_MemObjStore(pV,&sJ);` |
|    41 |  7145 | `	}` |
|    83 |  7146 | `	HeapPut(pVm,pThis,i,&sJ);` |
|    83 |  7147 | `	HeapPut(pVm,pThis,j,&sI);` |
|    83 |  7148 | `	PH7_MemObjRelease(&sI);` |
|    83 |  7149 | `	PH7_MemObjRelease(&sJ);` |
|    83 |  7150 | `}` |
|   416 |  7151 | `static int HeapCorrupted(ph7_class_instance *pThis)` |
|     1 |  7152 | `{` |
|   417 |  7153 | `	return pThis ? (int)PH7_NativeAttrInt(pThis,HP_CR) : 0;` |
|     1 |  7154 | `}` |
|     - |  7155 | `/*` |
|     - |  7156 | ` * php's spl_heap_consistency_validations. Only the operations that DEPEND on the` |
|     - |  7157 | ` * heap invariant call it -- count()/current()/key() answer from the array and are` |
|     - |  7158 | ` * left alone, which is why a corrupted heap still reports its size.` |
|     - |  7159 | ` */` |
|   390 |  7160 | `static sxi32 HeapCheck(ph7_context *pCtx)` |
|     1 |  7161 | `{` |
|   391 |  7162 | `	if( !HeapCorrupted(PH7_ContextThis(pCtx)) ){` |
|   379 |  7163 | `		return SXRET_OK;` |
|     - |  7164 | `	}` |
|    13 |  7165 | `	return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     - |  7166 | `		"Heap is corrupted, heap properties are no longer ensured.");` |
|   196 |  7167 | `}` |
|     - |  7168 | `/* A priority-queue node is php's {data, priority}: nothing else, and in that order. */` |
|   560 |  7169 | `static int HeapIsPq(ph7_class_instance *pThis)` |
|     1 |  7170 | `{` |
|     - |  7171 | `	ph7_class *pPq;` |
|   561 |  7172 | `	if( pThis == 0 ){` |
|   ! 0 |  7173 | `		return FALSE;` |
|     - |  7174 | `	}` |
|   561 |  7175 | `	pPq = PH7_VmExtractClass(pThis->pVm,"SplPriorityQueue",sizeof("SplPriorityQueue")-1,FALSE,0);` |
|   561 |  7176 | `	return pPq && PH7_VmInstanceOf(pThis->pClass,pPq);` |
|   281 |  7177 | `}` |
|   168 |  7178 | `static ph7_value * HeapNodePart(ph7_value *pNode,const char *zKey)` |
|     1 |  7179 | `{` |
|     - |  7180 | `	ph7_hashmap *pMap;` |
|   169 |  7181 | `	ph7_hashmap_node *pEnt = 0;` |
|   169 |  7182 | `	if( pNode == 0 \|\| (pNode->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 |  7183 | `		return 0;` |
|     - |  7184 | `	}` |
|   169 |  7185 | `	pMap = (ph7_hashmap *)pNode->x.pOther;` |
|   169 |  7186 | `	if( HashmapLookupBlobKey(pMap,zKey,(sxu32)SyStrlen(zKey),&pEnt) != SXRET_OK ){` |
|   ! 0 |  7187 | `		return 0;` |
|     - |  7188 | `	}` |
|   169 |  7189 | `	return HashmapExtractNodeValue(pEnt);` |
|    85 |  7190 | `}` |
|    76 |  7191 | `static sxi32 HeapMakeNode(ph7_vm *pVm,ph7_value *pData,ph7_value *pPrio,ph7_value *pOut)` |
|     1 |  7192 | `{` |
|     - |  7193 | `	ph7_value sKey;` |
|    77 |  7194 | `	if( PH7_MemObjToHashmap(pOut) != SXRET_OK ){` |
|   ! 0 |  7195 | `		return SXERR_MEM;` |
|     - |  7196 | `	}` |
|    77 |  7197 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|    77 |  7198 | `	PH7_MemObjStringAppend(&sKey,"data",sizeof("data")-1);` |
|    77 |  7199 | `	ph7_array_add_elem(pOut,&sKey,pData);` |
|    77 |  7200 | `	PH7_MemObjRelease(&sKey);` |
|    77 |  7201 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|    77 |  7202 | `	PH7_MemObjStringAppend(&sKey,"priority",sizeof("priority")-1);` |
|    77 |  7203 | `	ph7_array_add_elem(pOut,&sKey,pPrio);` |
|    77 |  7204 | `	PH7_MemObjRelease(&sKey);` |
|    77 |  7205 | `	return SXRET_OK;` |
|    39 |  7206 | `}` |
|     - |  7207 | `/*` |
|     - |  7208 | ` * Run the user's compare(). For a queue php compares the PRIORITIES, so the node's` |
|     - |  7209 | `` * `priority` is what is handed over. A throw here is php's corruption trigger: the`` |
|     - |  7210 | ` * bit is set, and the throw still propagates.` |
|     - |  7211 | ` */` |
|   202 |  7212 | `static sxi32 HeapCompare(ph7_context *pCtx,sxi64 iA,sxi64 iB,int *piCmp)` |
|     1 |  7213 | `{` |
|   203 |  7214 | `	ph7_vm *pVm = pCtx->pVm;` |
|   203 |  7215 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  7216 | `	ph7_class_method *pMethod;` |
|     - |  7217 | `	ph7_value sA,sB,sRes,*apArg[2],*pV;` |
|   203 |  7218 | `	int bPq = HeapIsPq(pThis);` |
|     - |  7219 | `	sxi32 rc;` |
|   203 |  7220 | `	*piCmp = 0;` |
|   203 |  7221 | `	pMethod = pThis ? PH7_ClassExtractMethod(pThis->pClass,"compare",sizeof("compare")-1) : 0;` |
|   203 |  7222 | `	if( pMethod == 0 ){` |
|   ! 0 |  7223 | `		return SXRET_OK;` |
|     - |  7224 | `	}` |
|   203 |  7225 | `	PH7_MemObjInit(pVm,&sA);` |
|   203 |  7226 | `	PH7_MemObjInit(pVm,&sB);` |
|   203 |  7227 | `	pV = HeapAt(pVm,pThis,iA);` |
|   203 |  7228 | `	if( bPq ){` |
|    59 |  7229 | `		pV = HeapNodePart(pV,"priority");` |
|    29 |  7230 | `	}` |
|   203 |  7231 | `	if( pV ){` |
|   203 |  7232 | `		PH7_MemObjStore(pV,&sA);` |
|   101 |  7233 | `	}` |
|   203 |  7234 | `	pV = HeapAt(pVm,pThis,iB);` |
|   203 |  7235 | `	if( bPq ){` |
|    59 |  7236 | `		pV = HeapNodePart(pV,"priority");` |
|    29 |  7237 | `	}` |
|   203 |  7238 | `	if( pV ){` |
|   203 |  7239 | `		PH7_MemObjStore(pV,&sB);` |
|   101 |  7240 | `	}` |
|   203 |  7241 | `	apArg[0] = &sA;` |
|   203 |  7242 | `	apArg[1] = &sB;` |
|   203 |  7243 | `	PH7_MemObjInit(pVm,&sRes);` |
|     - |  7244 | `	/* php dispatches through its cached fptr_cmp and never consults visibility --` |
|     - |  7245 | `	 * SplHeap::compare() is PROTECTED and is meant to be called by the heap. */` |
|   203 |  7246 | `	rc = PH7_VmCallMethodUnchecked(pVm,pThis,pMethod,&sRes,2,apArg);` |
|   203 |  7247 | `	if( rc == SXRET_OK ){` |
|   173 |  7248 | `		sxi64 iVal = ph7_value_to_int64(&sRes);` |
|   173 |  7249 | `		*piCmp = iVal < 0 ? -1 : (iVal > 0 ? 1 : 0);` |
|    87 |  7250 | `	}else{` |
|     - |  7251 | `		/* php finishes the sift with the exception in flight and marks the heap` |
|     - |  7252 | `		 * CORRUPTED afterwards; the element it was placing still lands. */` |
|    31 |  7253 | `		PH7_NativeSetAttrInt(pVm,pThis,HP_CR,1);` |
|     - |  7254 | `	}` |
|   203 |  7255 | `	PH7_MemObjRelease(&sA);` |
|   203 |  7256 | `	PH7_MemObjRelease(&sB);` |
|   203 |  7257 | `	PH7_MemObjRelease(&sRes);` |
|   203 |  7258 | `	return rc;` |
|   102 |  7259 | `}` |
|   226 |  7260 | `static sxi32 HeapSiftUp(ph7_context *pCtx,sxi64 i)` |
|     1 |  7261 | `{` |
|   227 |  7262 | `	ph7_vm *pVm = pCtx->pVm;` |
|   227 |  7263 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   297 |  7264 | `	while( i > 0 ){` |
|   149 |  7265 | `		sxi64 p = (i - 1) / 2;` |
|   149 |  7266 | `		int iCmp = 0;` |
|   149 |  7267 | `		sxi32 rc = HeapCompare(pCtx,i,p,&iCmp);` |
|   149 |  7268 | `		if( rc != SXRET_OK ){` |
|    31 |  7269 | `			return rc;` |
|     - |  7270 | `		}` |
|   119 |  7271 | `		if( iCmp <= 0 ){` |
|    49 |  7272 | `			break;` |
|     - |  7273 | `		}` |
|    71 |  7274 | `		HeapSwap(pVm,pThis,i,p);` |
|    71 |  7275 | `		i = p;` |
|     1 |  7276 | `	}` |
|   197 |  7277 | `	return SXRET_OK;` |
|   114 |  7278 | `}` |
|    60 |  7279 | `static sxi32 HeapSiftDown(ph7_context *pCtx,sxi64 i)` |
|     1 |  7280 | `{` |
|    61 |  7281 | `	ph7_vm *pVm = pCtx->pVm;` |
|    61 |  7282 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    42 |  7283 | `	for(;;){` |
|    73 |  7284 | `		sxi64 n = HeapCount(pVm,pThis);` |
|    73 |  7285 | `		sxi64 l = 2*i + 1, r = l + 1, b = i;` |
|    73 |  7286 | `		int iCmp = 0;` |
|     - |  7287 | `		sxi32 rc;` |
|    73 |  7288 | `		if( l < n ){` |
|    43 |  7289 | `			rc = HeapCompare(pCtx,l,b,&iCmp);` |
|    43 |  7290 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  7291 | `				return rc;` |
|     - |  7292 | `			}` |
|    43 |  7293 | `			if( iCmp > 0 ){` |
|    13 |  7294 | `				b = l;` |
|     6 |  7295 | `			}` |
|    21 |  7296 | `		}` |
|    73 |  7297 | `		if( r < n ){` |
|    13 |  7298 | `			rc = HeapCompare(pCtx,r,b,&iCmp);` |
|    13 |  7299 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  7300 | `				return rc;` |
|     - |  7301 | `			}` |
|    13 |  7302 | `			if( iCmp > 0 ){` |
|     3 |  7303 | `				b = r;` |
|     1 |  7304 | `			}` |
|     6 |  7305 | `		}` |
|    73 |  7306 | `		if( b == i ){` |
|    61 |  7307 | `			break;` |
|     - |  7308 | `		}` |
|    13 |  7309 | `		HeapSwap(pVm,pThis,i,b);` |
|    13 |  7310 | `		i = b;` |
|     1 |  7311 | `	}` |
|    61 |  7312 | `	return SXRET_OK;` |
|    31 |  7313 | `}` |
|     - |  7314 | `/* php's spl_pqueue_extract_helper: BOTH wins over either single bit. */` |
|    46 |  7315 | `static void HeapPqShape(ph7_context *pCtx,ph7_value *pNode)` |
|     1 |  7316 | `{` |
|    47 |  7317 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    47 |  7318 | `	int iFlags = pThis ? (int)PH7_NativeAttrInt(pThis,HP_FL) : PQ_EXTR_DATA;` |
|     - |  7319 | `	ph7_value *pPart;` |
|    47 |  7320 | `	if( (iFlags & PQ_EXTR_BOTH) == PQ_EXTR_BOTH ){` |
|     7 |  7321 | `		ph7_result_value(pCtx,pNode);` |
|     7 |  7322 | `		return;` |
|     - |  7323 | `	}` |
|    41 |  7324 | `	pPart = HeapNodePart(pNode,(iFlags & PQ_EXTR_DATA) ? "data" : "priority");` |
|    41 |  7325 | `	if( pPart ){` |
|    41 |  7326 | `		ph7_result_value(pCtx,pPart);` |
|    21 |  7327 | `	}else{` |
|   ! 0 |  7328 | `		ph7_result_null(pCtx);` |
|     - |  7329 | `	}` |
|    24 |  7330 | `}` |
|     - |  7331 | `/* Hand back element 0 the way this class presents it. */` |
|   126 |  7332 | `static void HeapResultTop(ph7_context *pCtx,ph7_value *pNode)` |
|     1 |  7333 | `{` |
|   127 |  7334 | `	if( HeapIsPq(PH7_ContextThis(pCtx)) ){` |
|    47 |  7335 | `		HeapPqShape(pCtx,pNode);` |
|    24 |  7336 | `	}else{` |
|    81 |  7337 | `		ph7_result_value(pCtx,pNode);` |
|     - |  7338 | `	}` |
|   127 |  7339 | `}` |
|   228 |  7340 | `static int vm_builtin_SplHeap_insert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7341 | `{` |
|   229 |  7342 | `	ph7_vm *pVm = pCtx->pVm;` |
|   229 |  7343 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  7344 | `	ph7_hashmap *pMap;` |
|   229 |  7345 | `	sxi32 rc = HeapCheck(pCtx);` |
|   229 |  7346 | `	if( rc != SXRET_OK \|\| nArg < 1 ){` |
|     3 |  7347 | `		return rc;` |
|     - |  7348 | `	}` |
|   227 |  7349 | `	pMap = HeapMap(pVm,pThis);` |
|   227 |  7350 | `	if( pMap == 0 ){` |
|   ! 0 |  7351 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  7352 | `	}` |
|   227 |  7353 | `	if( HeapIsPq(pThis) ){` |
|     - |  7354 | `		ph7_value sNode;` |
|    77 |  7355 | `		if( nArg < 2 ){` |
|   ! 0 |  7356 | `			return PH7_OK;` |
|     - |  7357 | `		}` |
|    77 |  7358 | `		PH7_MemObjInit(pVm,&sNode);` |
|    77 |  7359 | `		if( HeapMakeNode(pVm,apArg[0],apArg[1],&sNode) != SXRET_OK ){` |
|   ! 0 |  7360 | `			PH7_MemObjRelease(&sNode);` |
|   ! 0 |  7361 | `			return PH7_ContextMemoryError(pCtx);` |
|     - |  7362 | `		}` |
|    77 |  7363 | `		PH7_HashmapInsert(pMap,0,&sNode);` |
|    77 |  7364 | `		PH7_MemObjRelease(&sNode);` |
|    39 |  7365 | `	}else{` |
|   151 |  7366 | `		PH7_HashmapInsert(pMap,0,apArg[0]);` |
|     - |  7367 | `	}` |
|   227 |  7368 | `	rc = HeapSiftUp(pCtx,HeapCount(pVm,pThis)-1);` |
|   227 |  7369 | `	if( rc != SXRET_OK ){` |
|    31 |  7370 | `		return rc;` |
|     - |  7371 | `	}` |
|   197 |  7372 | ``	ph7_result_bool(pCtx,1);   /* php's `true` return type */`` |
|   197 |  7373 | `	return PH7_OK;` |
|   115 |  7374 | `}` |
|    90 |  7375 | `static int vm_builtin_SplHeap_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7376 | `{` |
|    91 |  7377 | `	ph7_vm *pVm = pCtx->pVm;` |
|    91 |  7378 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  7379 | `	sxi64 n;` |
|     - |  7380 | `	ph7_value sTop,*pV;` |
|    91 |  7381 | `	sxi32 rc = HeapCheck(pCtx);` |
|    45 |  7382 | `	SXUNUSED(nArg);` |
|    45 |  7383 | `	SXUNUSED(apArg);` |
|    91 |  7384 | `	if( rc != SXRET_OK ){` |
|     3 |  7385 | `		return rc;` |
|     - |  7386 | `	}` |
|    89 |  7387 | `	n = HeapCount(pVm,pThis);` |
|    89 |  7388 | `	if( n == 0 ){` |
|     5 |  7389 | `		return PH7_VmThrowException(pCtx,"RuntimeException","Can't extract from an empty heap");` |
|     - |  7390 | `	}` |
|    85 |  7391 | `	PH7_MemObjInit(pVm,&sTop);` |
|    85 |  7392 | `	pV = HeapAt(pVm,pThis,0);` |
|    85 |  7393 | `	if( pV ){` |
|    85 |  7394 | `		PH7_MemObjStore(pV,&sTop);` |
|    42 |  7395 | `	}` |
|    85 |  7396 | `	if( n > 1 ){` |
|     - |  7397 | `		ph7_value sLast;` |
|    61 |  7398 | `		PH7_MemObjInit(pVm,&sLast);` |
|    61 |  7399 | `		pV = HeapAt(pVm,pThis,n-1);` |
|    61 |  7400 | `		if( pV ){` |
|    61 |  7401 | `			PH7_MemObjStore(pV,&sLast);` |
|    30 |  7402 | `		}` |
|    61 |  7403 | `		HeapPut(pVm,pThis,0,&sLast);` |
|    61 |  7404 | `		PH7_MemObjRelease(&sLast);` |
|    30 |  7405 | `	}` |
|     - |  7406 | `	{` |
|    85 |  7407 | `		ph7_hashmap *pMap = HeapMap(pVm,pThis);` |
|    85 |  7408 | `		ph7_hashmap_node *pNode = 0;` |
|    85 |  7409 | `		if( pMap && HashmapLookupIntKey(pMap,n-1,&pNode) == SXRET_OK ){` |
|    85 |  7410 | `			PH7_HashmapUnlinkNode(pNode,TRUE);` |
|    42 |  7411 | `		}` |
|     - |  7412 | `	}` |
|    85 |  7413 | `	if( n > 1 ){` |
|    61 |  7414 | `		rc = HeapSiftDown(pCtx,0);` |
|    61 |  7415 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  7416 | `			PH7_MemObjRelease(&sTop);` |
|   ! 0 |  7417 | `			return rc;` |
|     - |  7418 | `		}` |
|    30 |  7419 | `	}` |
|    85 |  7420 | `	HeapResultTop(pCtx,&sTop);` |
|    85 |  7421 | `	PH7_MemObjRelease(&sTop);` |
|    85 |  7422 | `	return PH7_OK;` |
|    46 |  7423 | `}` |
|    12 |  7424 | `static int vm_builtin_SplHeap_top(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7425 | `{` |
|    13 |  7426 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  7427 | `	ph7_value *pV;` |
|    13 |  7428 | `	sxi32 rc = HeapCheck(pCtx);` |
|     6 |  7429 | `	SXUNUSED(nArg);` |
|     6 |  7430 | `	SXUNUSED(apArg);` |
|    13 |  7431 | `	if( rc != SXRET_OK ){` |
|     3 |  7432 | `		return rc;` |
|     - |  7433 | `	}` |
|    11 |  7434 | `	if( HeapCount(pCtx->pVm,pThis) == 0 ){` |
|     3 |  7435 | `		return PH7_VmThrowException(pCtx,"RuntimeException","Can't peek at an empty heap");` |
|     - |  7436 | `	}` |
|     9 |  7437 | `	pV = HeapAt(pCtx->pVm,pThis,0);` |
|     9 |  7438 | `	if( pV ){` |
|     9 |  7439 | `		HeapResultTop(pCtx,pV);` |
|     4 |  7440 | `	}` |
|     9 |  7441 | `	return PH7_OK;` |
|     7 |  7442 | `}` |
|    22 |  7443 | `static int vm_builtin_SplHeap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7444 | `{` |
|    11 |  7445 | `	SXUNUSED(nArg);` |
|    11 |  7446 | `	SXUNUSED(apArg);` |
|    23 |  7447 | `	ph7_result_int64(pCtx,HeapCount(pCtx->pVm,PH7_ContextThis(pCtx)));` |
|    23 |  7448 | `	return PH7_OK;` |
|     1 |  7449 | `}` |
|    50 |  7450 | `static int vm_builtin_SplHeap_isEmpty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7451 | `{` |
|    25 |  7452 | `	SXUNUSED(nArg);` |
|    25 |  7453 | `	SXUNUSED(apArg);` |
|    51 |  7454 | `	ph7_result_bool(pCtx,HeapCount(pCtx->pVm,PH7_ContextThis(pCtx)) == 0);` |
|    51 |  7455 | `	return PH7_OK;` |
|     1 |  7456 | `}` |
|    12 |  7457 | `static int vm_builtin_SplHeap_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7458 | `{` |
|     6 |  7459 | `	SXUNUSED(nArg);` |
|     6 |  7460 | `	SXUNUSED(apArg);` |
|     6 |  7461 | `	SXUNUSED(pCtx);` |
|    13 |  7462 | `	return PH7_OK;   /* php's rewind is a no-op: a heap is walked by extraction */` |
|     1 |  7463 | `}` |
|    44 |  7464 | `static int vm_builtin_SplHeap_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7465 | `{` |
|    22 |  7466 | `	SXUNUSED(nArg);` |
|    22 |  7467 | `	SXUNUSED(apArg);` |
|    45 |  7468 | `	ph7_result_bool(pCtx,HeapCount(pCtx->pVm,PH7_ContextThis(pCtx)) > 0);` |
|    45 |  7469 | `	return PH7_OK;` |
|     1 |  7470 | `}` |
|    34 |  7471 | `static int vm_builtin_SplHeap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7472 | `{` |
|    35 |  7473 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  7474 | `	ph7_value *pV;` |
|    17 |  7475 | `	SXUNUSED(nArg);` |
|    17 |  7476 | `	SXUNUSED(apArg);` |
|    35 |  7477 | `	if( HeapCount(pCtx->pVm,pThis) == 0 ){` |
|   ! 0 |  7478 | `		ph7_result_null(pCtx);` |
|   ! 0 |  7479 | `		return PH7_OK;` |
|     - |  7480 | `	}` |
|    35 |  7481 | `	pV = HeapAt(pCtx->pVm,pThis,0);` |
|    35 |  7482 | `	if( pV ){` |
|    35 |  7483 | `		HeapResultTop(pCtx,pV);` |
|    17 |  7484 | `	}` |
|    35 |  7485 | `	return PH7_OK;` |
|    18 |  7486 | `}` |
|    16 |  7487 | `static int vm_builtin_SplHeap_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7488 | `{` |
|     8 |  7489 | `	SXUNUSED(nArg);` |
|     8 |  7490 | `	SXUNUSED(apArg);` |
|    17 |  7491 | `	ph7_result_int64(pCtx,HeapCount(pCtx->pVm,PH7_ContextThis(pCtx))-1);` |
|    17 |  7492 | `	return PH7_OK;` |
|     1 |  7493 | `}` |
|    34 |  7494 | `static int vm_builtin_SplHeap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7495 | `{` |
|    35 |  7496 | `	sxi32 rc = HeapCheck(pCtx);` |
|    35 |  7497 | `	if( rc != SXRET_OK ){` |
|     5 |  7498 | `		return rc;` |
|     - |  7499 | `	}` |
|    31 |  7500 | `	if( HeapCount(pCtx->pVm,PH7_ContextThis(pCtx)) == 0 ){` |
|   ! 0 |  7501 | `		return PH7_OK;` |
|     - |  7502 | `	}` |
|    31 |  7503 | `	rc = vm_builtin_SplHeap_extract(pCtx,nArg,apArg);` |
|    31 |  7504 | `	ph7_result_null(pCtx);   /* php's next() is void; the extracted value is dropped */` |
|    31 |  7505 | `	return rc;` |
|    18 |  7506 | `}` |
|     6 |  7507 | `static int vm_builtin_SplHeap_isCorrupted(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7508 | `{` |
|     3 |  7509 | `	SXUNUSED(nArg);` |
|     3 |  7510 | `	SXUNUSED(apArg);` |
|     7 |  7511 | `	ph7_result_bool(pCtx,HeapCorrupted(PH7_ContextThis(pCtx)));` |
|     7 |  7512 | `	return PH7_OK;` |
|     1 |  7513 | `}` |
|     4 |  7514 | `static int vm_builtin_SplHeap_recover(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7515 | `{` |
|     2 |  7516 | `	SXUNUSED(nArg);` |
|     2 |  7517 | `	SXUNUSED(apArg);` |
|     5 |  7518 | `	PH7_NativeSetAttrInt(pCtx->pVm,PH7_ContextThis(pCtx),HP_CR,0);` |
|     5 |  7519 | ``	ph7_result_bool(pCtx,1);   /* php's `true` return type */`` |
|     5 |  7520 | `	return PH7_OK;` |
|     1 |  7521 | `}` |
|    32 |  7522 | `static int vm_builtin_SplMinHeap_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7523 | `{` |
|     - |  7524 | `	/* php: $value2 <=> $value1 — the SMALLEST value sits on top. */` |
|    33 |  7525 | `	if( nArg < 2 ){` |
|   ! 0 |  7526 | `		return PH7_OK;` |
|     - |  7527 | `	}` |
|    33 |  7528 | `	ph7_result_int64(pCtx,(ph7_int64)PH7_MemObjCmp(apArg[1],apArg[0],FALSE,0));` |
|    33 |  7529 | `	return PH7_OK;` |
|    17 |  7530 | `}` |
|    48 |  7531 | `static int vm_builtin_SplMaxHeap_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7532 | `{` |
|    49 |  7533 | `	if( nArg < 2 ){` |
|   ! 0 |  7534 | `		return PH7_OK;` |
|     - |  7535 | `	}` |
|    49 |  7536 | `	ph7_result_int64(pCtx,(ph7_int64)PH7_MemObjCmp(apArg[0],apArg[1],FALSE,0));` |
|    49 |  7537 | `	return PH7_OK;` |
|    25 |  7538 | `}` |
|    58 |  7539 | `static int vm_builtin_SplPq_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7540 | `{` |
|    59 |  7541 | `	if( nArg < 2 ){` |
|   ! 0 |  7542 | `		return PH7_OK;` |
|     - |  7543 | `	}` |
|    59 |  7544 | `	ph7_result_int64(pCtx,(ph7_int64)PH7_MemObjCmp(apArg[0],apArg[1],FALSE,0));` |
|    59 |  7545 | `	return PH7_OK;` |
|    30 |  7546 | `}` |
|    14 |  7547 | `static int vm_builtin_SplPq_setExtractFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7548 | `{` |
|    15 |  7549 | `	ph7_vm *pVm = pCtx->pVm;` |
|    15 |  7550 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    15 |  7551 | `	sxi64 iFlags = 0;` |
|     - |  7552 | `	sxi32 rc;` |
|    15 |  7553 | `	if( nArg < 1 ){` |
|   ! 0 |  7554 | `		return PH7_OK;` |
|     - |  7555 | `	}` |
|    15 |  7556 | `	rc = PH7_IntArgResolve(pCtx,apArg[0],"SplPriorityQueue::setExtractFlags",1,"$flags","int",&iFlags);` |
|    15 |  7557 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  7558 | `		return rc;` |
|     - |  7559 | `	}` |
|     - |  7560 | `	/* php masks to the two bits and then REFUSES an empty selection — a nonsense` |
|     - |  7561 | `	 * value is reduced, but asking for neither half is an error. */` |
|    15 |  7562 | `	iFlags &= PQ_EXTR_MASK;` |
|    15 |  7563 | `	if( iFlags == 0 ){` |
|     3 |  7564 | `		return PH7_VmThrowException(pCtx,"RuntimeException","Must specify at least one extract flag");` |
|     - |  7565 | `	}` |
|    13 |  7566 | `	PH7_NativeSetAttrInt(pVm,pThis,HP_FL,iFlags);` |
|    13 |  7567 | `	ph7_result_int64(pCtx,(ph7_int64)iFlags);` |
|    13 |  7568 | `	return PH7_OK;` |
|     8 |  7569 | `}` |
|     6 |  7570 | `static int vm_builtin_SplPq_getExtractFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7571 | `{` |
|     3 |  7572 | `	SXUNUSED(nArg);` |
|     3 |  7573 | `	SXUNUSED(apArg);` |
|     7 |  7574 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(PH7_ContextThis(pCtx),HP_FL));` |
|     7 |  7575 | `	return PH7_OK;` |
|     1 |  7576 | `}` |
|     - |  7577 | `/*` |
|     - |  7578 | ` * php's get_debug_info: flags, isCorrupted, heap. The QUEUE renders its entries` |
|     - |  7579 | ` * EXTR_BOTH-style whatever the extract flags say, because the debug view is of the` |
|     - |  7580 | ` * STORAGE rather than of what extract() would hand back. All three keys are php's` |
|     - |  7581 | `` * mangled private names (SplRootDebugKey): `SplHeap` for SplMinHeap/SplMaxHeap and`` |
|     - |  7582 | `` * every subclass of either, `SplPriorityQueue` for the queue.`` |
|     - |  7583 | ` */` |
|    20 |  7584 | `static sxi32 HeapFillDebug(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)` |
|     1 |  7585 | `{` |
|     - |  7586 | `	ph7_value sKey,sVal,*pStore;` |
|    21 |  7587 | `	SplRootDebugKey(pVm,pThis,&sKey,"flags");` |
|    21 |  7588 | `	PH7_MemObjInitFromInt(pVm,&sVal,PH7_NativeAttrInt(pThis,HP_FL));` |
|    21 |  7589 | `	ph7_array_add_elem(pOut,&sKey,&sVal);` |
|    21 |  7590 | `	PH7_MemObjRelease(&sKey);` |
|    21 |  7591 | `	PH7_MemObjRelease(&sVal);` |
|    21 |  7592 | `	SplRootDebugKey(pVm,pThis,&sKey,"isCorrupted");` |
|    21 |  7593 | `	PH7_MemObjInitFromBool(pVm,&sVal,HeapCorrupted(pThis));` |
|    21 |  7594 | `	ph7_array_add_elem(pOut,&sKey,&sVal);` |
|    21 |  7595 | `	PH7_MemObjRelease(&sKey);` |
|    21 |  7596 | `	PH7_MemObjRelease(&sVal);` |
|    21 |  7597 | `	SplRootDebugKey(pVm,pThis,&sKey,"heap");` |
|    21 |  7598 | `	pStore = HeapSlot(pVm,pThis);` |
|    21 |  7599 | `	if( pStore ){` |
|    21 |  7600 | `		ph7_array_add_elem(pOut,&sKey,pStore);` |
|    10 |  7601 | `	}` |
|    21 |  7602 | `	PH7_MemObjRelease(&sKey);` |
|    21 |  7603 | `	return PH7_OK;` |
|     1 |  7604 | `}` |
|    20 |  7605 | `static sxi32 HeapPresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|     1 |  7606 | `{` |
|    21 |  7607 | `	if( !bDebug ){` |
|     9 |  7608 | `		return PH7_OK;   /* php's (array) cast shows nothing */` |
|     - |  7609 | `	}` |
|    13 |  7610 | `	return HeapFillDebug(pVm,pThis,pOut);` |
|    11 |  7611 | `}` |
|     8 |  7612 | `static int vm_builtin_SplHeap_debugInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7613 | `{` |
|     9 |  7614 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  7615 | `	ph7_value sOut;` |
|     4 |  7616 | `	SXUNUSED(nArg);` |
|     4 |  7617 | `	SXUNUSED(apArg);` |
|     9 |  7618 | `	PH7_MemObjInit(pVm,&sOut);` |
|     9 |  7619 | `	if( PH7_MemObjToHashmap(&sOut) != SXRET_OK ){` |
|   ! 0 |  7620 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 |  7621 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  7622 | `	}` |
|     9 |  7623 | `	HeapFillDebug(pVm,PH7_ContextThis(pCtx),&sOut);` |
|     9 |  7624 | `	ph7_result_value(pCtx,&sOut);` |
|     9 |  7625 | `	PH7_MemObjRelease(&sOut);` |
|     9 |  7626 | `	return PH7_OK;` |
|     5 |  7627 | `}` |
|     - |  7628 | `/*` |
|     - |  7629 | ` * php's __serialize(): [members, {flags, heap_elements}]. Note the OUTER array is` |
|     - |  7630 | ` * a two-element list whose first entry is the instance's own property table —` |
|     - |  7631 | ` * empty for a bare heap, a SUBCLASS's declared slots when there is one.` |
|     - |  7632 | ` */` |
|    20 |  7633 | `static int vm_builtin_SplHeap_serializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7634 | `{` |
|    21 |  7635 | `	ph7_vm *pVm = pCtx->pVm;` |
|    21 |  7636 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  7637 | `	ph7_value sOut,sMembers,sState,sKey,sVal,*pStore;` |
|    21 |  7638 | `	sxi32 rc = HeapCheck(pCtx);` |
|    10 |  7639 | `	SXUNUSED(nArg);` |
|    10 |  7640 | `	SXUNUSED(apArg);` |
|    21 |  7641 | `	if( rc != SXRET_OK ){` |
|     3 |  7642 | `		return rc;` |
|     - |  7643 | `	}` |
|    19 |  7644 | `	PH7_MemObjInit(pVm,&sOut);` |
|    19 |  7645 | `	PH7_MemObjInit(pVm,&sState);` |
|    18 |  7646 | `	if( SplMembersOf(pVm,pThis,&sMembers) != SXRET_OK` |
|    18 |  7647 | `	 \|\| PH7_MemObjToHashmap(&sOut) != SXRET_OK` |
|    19 |  7648 | `	 \|\| PH7_MemObjToHashmap(&sState) != SXRET_OK ){` |
|   ! 0 |  7649 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 |  7650 | `		PH7_MemObjRelease(&sMembers);` |
|   ! 0 |  7651 | `		PH7_MemObjRelease(&sState);` |
|   ! 0 |  7652 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  7653 | `	}` |
|    19 |  7654 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|    19 |  7655 | `	PH7_MemObjStringAppend(&sKey,"flags",sizeof("flags")-1);` |
|    19 |  7656 | `	PH7_MemObjInitFromInt(pVm,&sVal,PH7_NativeAttrInt(pThis,HP_FL));` |
|    19 |  7657 | `	ph7_array_add_elem(&sState,&sKey,&sVal);` |
|    19 |  7658 | `	PH7_MemObjRelease(&sKey);` |
|    19 |  7659 | `	PH7_MemObjRelease(&sVal);` |
|    19 |  7660 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|    19 |  7661 | `	PH7_MemObjStringAppend(&sKey,"heap_elements",sizeof("heap_elements")-1);` |
|    19 |  7662 | `	pStore = HeapSlot(pVm,pThis);` |
|    19 |  7663 | `	if( pStore ){` |
|    19 |  7664 | `		ph7_array_add_elem(&sState,&sKey,pStore);` |
|     9 |  7665 | `	}` |
|    19 |  7666 | `	PH7_MemObjRelease(&sKey);` |
|    19 |  7667 | `	ph7_array_add_elem(&sOut,0,&sMembers);` |
|    19 |  7668 | `	ph7_array_add_elem(&sOut,0,&sState);` |
|    19 |  7669 | `	ph7_result_value(pCtx,&sOut);` |
|    19 |  7670 | `	PH7_MemObjRelease(&sOut);` |
|    19 |  7671 | `	PH7_MemObjRelease(&sMembers);` |
|    19 |  7672 | `	PH7_MemObjRelease(&sState);` |
|    19 |  7673 | `	return PH7_OK;` |
|    11 |  7674 | `}` |
|   ! 0 |  7675 | `static sxi32 HeapUnserializeFail(ph7_context *pCtx)` |
|   ! 0 |  7676 | `{` |
|   ! 0 |  7677 | `	return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - |  7678 | `		"Unexpected data found in serialization payload");` |
|   ! 0 |  7679 | `}` |
|     6 |  7680 | `static int vm_builtin_SplHeap_unserializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7681 | `{` |
|     7 |  7682 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 |  7683 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  7684 | `	ph7_hashmap *pData;` |
|     7 |  7685 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  7686 | `	ph7_value *pState,*pFlags,*pElems,*pSlot;` |
|     - |  7687 | `	sxi64 iFlags;` |
|     - |  7688 | `	int bPq;` |
|     7 |  7689 | `	sxi32 rc = HeapCheck(pCtx);` |
|     7 |  7690 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  7691 | `		return rc;` |
|     - |  7692 | `	}` |
|     7 |  7693 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 \|\| pThis == 0 ){` |
|   ! 0 |  7694 | `		return HeapUnserializeFail(pCtx);` |
|     - |  7695 | `	}` |
|     7 |  7696 | `	pData = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     7 |  7697 | `	if( HashmapLookupIntKey(pData,1,&pNode) != SXRET_OK ){` |
|   ! 0 |  7698 | `		return HeapUnserializeFail(pCtx);` |
|     - |  7699 | `	}` |
|     7 |  7700 | `	pState = HashmapExtractNodeValue(pNode);` |
|     7 |  7701 | `	if( pState == 0 \|\| (pState->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 |  7702 | `		return HeapUnserializeFail(pCtx);` |
|     - |  7703 | `	}` |
|     7 |  7704 | `	pFlags = HeapNodePart(pState,"flags");` |
|     7 |  7705 | `	pElems = HeapNodePart(pState,"heap_elements");` |
|     6 |  7706 | `	if( pFlags == 0 \|\| (pFlags->iFlags & MEMOBJ_INT) == 0` |
|     7 |  7707 | `	 \|\| pElems == 0 \|\| (pElems->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 |  7708 | `		return HeapUnserializeFail(pCtx);` |
|     - |  7709 | `	}` |
|     - |  7710 | `	/* php VALIDATES the flags against the class: a plain heap has no user-visible` |
|     - |  7711 | `	 * flags at all, the queue must name at least one half to extract. */` |
|     7 |  7712 | `	iFlags = ph7_value_to_int64(pFlags);` |
|     7 |  7713 | `	bPq = HeapIsPq(pThis);` |
|     7 |  7714 | `	if( bPq ){` |
|     5 |  7715 | `		iFlags &= PQ_EXTR_MASK;` |
|     5 |  7716 | `		if( iFlags == 0 ){` |
|   ! 0 |  7717 | `			return HeapUnserializeFail(pCtx);` |
|     1 |  7718 | `		}` |
|     5 |  7719 | `	}else if( iFlags != 0 ){` |
|   ! 0 |  7720 | `		return HeapUnserializeFail(pCtx);` |
|     - |  7721 | `	}` |
|     7 |  7722 | `	PH7_NativeSetAttrInt(pVm,pThis,HP_FL,iFlags);` |
|     7 |  7723 | `	pSlot = PH7_NativeAttr(pThis,HP_H);` |
|     7 |  7724 | `	if( pSlot ){` |
|     7 |  7725 | `		PH7_MemObjRelease(pSlot);` |
|     7 |  7726 | `		PH7_MemObjStore(pElems,pSlot);` |
|     3 |  7727 | `	}` |
|     7 |  7728 | `	if( HashmapLookupIntKey(pData,0,&pNode) == SXRET_OK ){` |
|     7 |  7729 | `		SplMembersLoad(pThis,HashmapExtractNodeValue(pNode));` |
|     3 |  7730 | `	}` |
|     7 |  7731 | `	return PH7_OK;` |
|     4 |  7732 | `}` |
|     - |  7733 | `/*` |
|     - |  7734 | ` * The declaration. Method ORDER is spl_heap.stub.php's; php declares no` |
|     - |  7735 | ` * constructor for any of the four, SplHeap::compare is ABSTRACT PROTECTED (so` |
|     - |  7736 | ` * SplHeap itself cannot be instantiated) while the queue's is PUBLIC, and the` |
|     - |  7737 | ` * queue's default extract mode is EXTR_DATA, stamped as a property default the` |
|     - |  7738 | ` * way the DLL family's fix bit is.` |
|     - |  7739 | ` */` |
|  6721 |  7740 | `static sxi32 VmInstallSplHeap(ph7_vm *pVm)` |
|     5 |  7741 | `{` |
|     - |  7742 | `	static const PH7_NativePropDef aHeapProp[] = {` |
|     - |  7743 | `		{ HP_H,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  7744 | `		{ HP_FL, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  7745 | `		{ HP_CR, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  7746 | `	};` |
|     - |  7747 | `	static const PH7_NativePropDef aPqProp[] = {` |
|     - |  7748 | `		{ HP_H,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  7749 | `		{ HP_FL, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN,` |
|     - |  7750 | `		  { 0, 0, PH7_NATIVE_VAL_INT, PQ_EXTR_DATA, 0, 0.0 }, 0 },` |
|     - |  7751 | `		{ HP_CR, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  7752 | `	};` |
|     - |  7753 | `	static const PH7_NativeMethodDef aHeapMethod[] = {` |
|     - |  7754 | `		{ "extract",               PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplHeap_extract },` |
|     - |  7755 | `		{ "insert",                PH7_MOD_PUBLIC, "mixed $value", "@true",` |
|     - |  7756 | `		  vm_builtin_SplHeap_insert },` |
|     - |  7757 | `		{ "top",                   PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplHeap_top },` |
|     - |  7758 | `		{ "count",                 PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplHeap_count },` |
|     - |  7759 | `		{ "isEmpty",               PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplHeap_isEmpty },` |
|     - |  7760 | `		{ "rewind",                PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplHeap_rewind },` |
|     - |  7761 | `		{ "current",               PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplHeap_current },` |
|     - |  7762 | `		{ "key",                   PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplHeap_key },` |
|     - |  7763 | `		{ "next",                  PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplHeap_next },` |
|     - |  7764 | `		{ "valid",                 PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplHeap_valid },` |
|     - |  7765 | `		{ "recoverFromCorruption", PH7_MOD_PUBLIC, "", "@true", vm_builtin_SplHeap_recover },` |
|     - |  7766 | `		{ "compare",               PH7_MOD_PROTECTED\|PH7_MOD_ABSTRACT,` |
|     - |  7767 | `		  "mixed $value1, mixed $value2", "@int", 0 },` |
|     - |  7768 | `		{ "isCorrupted",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplHeap_isCorrupted },` |
|     - |  7769 | `		{ "__debugInfo",           PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplHeap_debugInfo },` |
|     - |  7770 | `		{ "__serialize",           PH7_MOD_PUBLIC, "", "@array",` |
|     - |  7771 | `		  vm_builtin_SplHeap_serializeMagic },` |
|     - |  7772 | `		{ "__unserialize",         PH7_MOD_PUBLIC, "array $data", "@void",` |
|     - |  7773 | `		  vm_builtin_SplHeap_unserializeMagic },` |
|     - |  7774 | `	};` |
|     - |  7775 | `	static const PH7_NativeMethodDef aMinMethod[] = {` |
|     - |  7776 | `		{ "compare", PH7_MOD_PROTECTED, "mixed $value1, mixed $value2", "@int",` |
|     - |  7777 | `		  vm_builtin_SplMinHeap_compare },` |
|     - |  7778 | `	};` |
|     - |  7779 | `	static const PH7_NativeMethodDef aMaxMethod[] = {` |
|     - |  7780 | `		{ "compare", PH7_MOD_PROTECTED, "mixed $value1, mixed $value2", "@int",` |
|     - |  7781 | `		  vm_builtin_SplMaxHeap_compare },` |
|     - |  7782 | `	};` |
|     - |  7783 | `	static const PH7_NativeConstDef aPqConst[] = {` |
|     - |  7784 | `		{ "EXTR_BOTH",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PQ_EXTR_BOTH, 0, 0.0 },` |
|     - |  7785 | `		{ "EXTR_PRIORITY", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PQ_EXTR_PRIORITY, 0, 0.0 },` |
|     - |  7786 | `		{ "EXTR_DATA",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PQ_EXTR_DATA, 0, 0.0 },` |
|     - |  7787 | `	};` |
|     - |  7788 | `	static const PH7_NativeMethodDef aPqMethod[] = {` |
|     - |  7789 | `		{ "compare",               PH7_MOD_PUBLIC, "mixed $priority1, mixed $priority2", "@int",` |
|     - |  7790 | `		  vm_builtin_SplPq_compare },` |
|     - |  7791 | `		{ "insert",                PH7_MOD_PUBLIC, "mixed $value, mixed $priority", "@true",` |
|     - |  7792 | `		  vm_builtin_SplHeap_insert },` |
|     - |  7793 | `		{ "setExtractFlags",       PH7_MOD_PUBLIC, "int $flags", "@int",` |
|     - |  7794 | `		  vm_builtin_SplPq_setExtractFlags },` |
|     - |  7795 | `		{ "top",                   PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplHeap_top },` |
|     - |  7796 | `		{ "extract",               PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplHeap_extract },` |
|     - |  7797 | `		{ "count",                 PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplHeap_count },` |
|     - |  7798 | `		{ "isEmpty",               PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplHeap_isEmpty },` |
|     - |  7799 | `		{ "rewind",                PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplHeap_rewind },` |
|     - |  7800 | `		{ "current",               PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplHeap_current },` |
|     - |  7801 | `		{ "key",                   PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplHeap_key },` |
|     - |  7802 | `		{ "next",                  PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplHeap_next },` |
|     - |  7803 | `		{ "valid",                 PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplHeap_valid },` |
|     - |  7804 | `		{ "recoverFromCorruption", PH7_MOD_PUBLIC, "", "@true", vm_builtin_SplHeap_recover },` |
|     - |  7805 | `		{ "isCorrupted",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplHeap_isCorrupted },` |
|     - |  7806 | `		{ "getExtractFlags",       PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplPq_getExtractFlags },` |
|     - |  7807 | `		{ "__debugInfo",           PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplHeap_debugInfo },` |
|     - |  7808 | `		{ "__serialize",           PH7_MOD_PUBLIC, "", "@array",` |
|     - |  7809 | `		  vm_builtin_SplHeap_serializeMagic },` |
|     - |  7810 | `		{ "__unserialize",         PH7_MOD_PUBLIC, "array $data", "@void",` |
|     - |  7811 | `		  vm_builtin_SplHeap_unserializeMagic },` |
|     - |  7812 | `	};` |
|     - |  7813 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - |  7814 | `		{ "SplPriorityQueue", 0, "Iterator,Countable", 0,` |
|     - |  7815 | `		  aPqMethod, SX_ARRAYSIZE(aPqMethod),` |
|     - |  7816 | `		  aPqConst, SX_ARRAYSIZE(aPqConst),` |
|     - |  7817 | `		  aPqProp, SX_ARRAYSIZE(aPqProp), 0, 0, HeapPresent },` |
|     - |  7818 | `		{ "SplHeap", 0, "Iterator,Countable", PH7_CLASS_ABSTRACT,` |
|     - |  7819 | `		  aHeapMethod, SX_ARRAYSIZE(aHeapMethod), 0, 0,` |
|     - |  7820 | `		  aHeapProp, SX_ARRAYSIZE(aHeapProp), 0, 0, HeapPresent },` |
|     - |  7821 | `		{ "SplMinHeap", "SplHeap", 0, 0,` |
|     - |  7822 | `		  aMinMethod, SX_ARRAYSIZE(aMinMethod), 0, 0, 0, 0, 0, 0, HeapPresent },` |
|     - |  7823 | `		{ "SplMaxHeap", "SplHeap", 0, 0,` |
|     - |  7824 | `		  aMaxMethod, SX_ARRAYSIZE(aMaxMethod), 0, 0, 0, 0, 0, 0, HeapPresent },` |
|     - |  7825 | `	};` |
|  6726 |  7826 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 |  7827 | `}` |
|     - |  7828 | `/*` |
|     - |  7829 | ` * ---------------------------------------------------------------------------` |
|     - |  7830 | ` * SplFixedArray.` |
|     - |  7831 | ` *` |
|     - |  7832 | `` * php PRESENTS this one as its own elements: `var_dump` shows`` |
|     - |  7833 | `` * `object(SplFixedArray)#1 (3) { [0]=> … }`, the `(array)` cast yields the`` |
|     - |  7834 | `` * elements with their integer keys, and `serialize()` writes them as INTEGER`` |
|     - |  7835 | ``  * property names (`O:13:"SplFixedArray":3:{i:0;…}`) because `__serialize()` `` |
|     - |  7836 | `` * simply hands the element array back. The chunk exposed `__a`/`__n` on all`` |
|     - |  7837 | ` * three surfaces instead.` |
|     - |  7838 | ` *` |
|     - |  7839 | `` * `getIterator()` answers php's **InternalIterator**, not a Generator. The chunk`` |
|     - |  7840 | ` * yielded, which is one class name wrong on a php-visible surface and also the` |
|     - |  7841 | `` * thing rule 5's InternalIterator exists for — `pIterVtab` plus`` |
|     - |  7842 | `` * `PH7_NativeIteratorNew()` is the whole implementation, and it gets php's`` |
|     - |  7843 | ` * independent-cursor behaviour (two getIterator() calls, or nested foreach, walk` |
|     - |  7844 | ` * separately) for free.` |
|     - |  7845 | ` *` |
|     - |  7846 | ` * php's offset rule is its own: an int, a bool, an INTEGER-LIKE string and a` |
|     - |  7847 | ` * RESOURCE (which warns and becomes its id, php's engine-wide rule) are accepted,` |
|     - |  7848 | `` * everything else is `Cannot access offset of type %s on SplFixedArray`.`` |
|     - |  7849 | ` * The chunk refused bools. A FLOAT offset stays refused here, which is not php's` |
|     - |  7850 | ` * answer (php truncates, with a precision deprecation when it is lossy) but IS` |
|     - |  7851 | `` * PHL's engine-wide one — `$a[1.5]` on a plain array raises the same TypeError,`` |
|     - |  7852 | ` * so the class stays consistent with the engine it lives in rather than uniquely` |
|     - |  7853 | ` * permissive (§10).` |
|     - |  7854 | ` */` |
|     - |  7855 | `#define FA_A "__a"   /* the elements, 0..n-1 */` |
|     - |  7856 | `#define FA_N "__n"   /* php's size */` |
|     - |  7857 |  |
|   886 |  7858 | `static ph7_value * FaSlot(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     3 |  7859 | `{` |
|   889 |  7860 | `	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,FA_A) : 0;` |
|   889 |  7861 | `	if( pSlot == 0 ){` |
|   ! 0 |  7862 | `		return 0;` |
|     - |  7863 | `	}` |
|   889 |  7864 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   137 |  7865 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|   ! 0 |  7866 | `			return 0;` |
|     - |  7867 | `		}` |
|    67 |  7868 | `	}` |
|   889 |  7869 | `	if( PH7_HashmapCowSeparate(pVm,pSlot) == 0 ){` |
|   ! 0 |  7870 | `		return 0;` |
|     - |  7871 | `	}` |
|     - |  7872 | `	/* Re-ask: the separation can MOVE the memobj pool this slot lives in` |
|     - |  7873 | `	 * (SplStoreSlot explains it). */` |
|   889 |  7874 | `	return PH7_NativeAttr(pThis,FA_A);` |
|   446 |  7875 | `}` |
|   836 |  7876 | `static ph7_hashmap * FaMap(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     3 |  7877 | `{` |
|   839 |  7878 | `	ph7_value *pSlot = FaSlot(pVm,pThis);` |
|   839 |  7879 | `	return pSlot ? (ph7_hashmap *)pSlot->x.pOther : 0;` |
|     3 |  7880 | `}` |
|   504 |  7881 | `static sxi64 FaSize(ph7_class_instance *pThis)` |
|     3 |  7882 | `{` |
|   507 |  7883 | `	return pThis ? PH7_NativeAttrInt(pThis,FA_N) : 0;` |
|     3 |  7884 | `}` |
|   102 |  7885 | `static ph7_value * FaAt(ph7_vm *pVm,ph7_class_instance *pThis,sxi64 i)` |
|     2 |  7886 | `{` |
|   104 |  7887 | `	ph7_hashmap *pMap = FaMap(pVm,pThis);` |
|   104 |  7888 | `	ph7_hashmap_node *pNode = 0;` |
|   104 |  7889 | `	if( pMap == 0 \|\| HashmapLookupIntKey(pMap,i,&pNode) != SXRET_OK ){` |
|   ! 0 |  7890 | `		return 0;` |
|     - |  7891 | `	}` |
|   104 |  7892 | `	return HashmapExtractNodeValue(pNode);` |
|    53 |  7893 | `}` |
|   590 |  7894 | `static void FaPut(ph7_vm *pVm,ph7_class_instance *pThis,sxi64 i,ph7_value *pVal)` |
|     3 |  7895 | `{` |
|   593 |  7896 | `	ph7_hashmap *pMap = FaMap(pVm,pThis);` |
|     - |  7897 | `	ph7_value sKey;` |
|   593 |  7898 | `	if( pMap == 0 ){` |
|   ! 0 |  7899 | `		return;` |
|     - |  7900 | `	}` |
|   593 |  7901 | `	PH7_MemObjInitFromInt(pVm,&sKey,i);` |
|   593 |  7902 | `	PH7_HashmapInsert(pMap,&sKey,pVal);` |
|   593 |  7903 | `	PH7_MemObjRelease(&sKey);` |
|   298 |  7904 | `}` |
|     - |  7905 | `/*` |
|     - |  7906 | ` * php's offset decode. An INTEGER-LIKE string is accepted (php's own` |
|     - |  7907 | `` * `ZEND_HANDLE_NUMERIC_STRING`), a bool is its 0/1, and every other type is named`` |
|     - |  7908 | ` * in the refusal. Returns 0 and leaves a TypeError raised when it cannot decode.` |
|     - |  7909 | ` */` |
|   302 |  7910 | `static int FaOffset(ph7_context *pCtx,ph7_value *pArg,sxi64 *piOut,sxi32 *pRc)` |
|     3 |  7911 | `{` |
|   305 |  7912 | `	*pRc = PH7_OK;` |
|   305 |  7913 | `	if( pArg == 0 ){` |
|   ! 0 |  7914 | `		*piOut = 0;` |
|   ! 0 |  7915 | `		return 1;` |
|     - |  7916 | `	}` |
|   305 |  7917 | `	if( pArg->iFlags & MEMOBJ_INT ){` |
|   262 |  7918 | `		*piOut = pArg->x.iVal;` |
|   262 |  7919 | `		return 1;` |
|     - |  7920 | `	}` |
|    45 |  7921 | `	if( pArg->iFlags & MEMOBJ_BOOL ){` |
|     8 |  7922 | `		*piOut = pArg->x.iVal ? 1 : 0;` |
|     8 |  7923 | `		return 1;` |
|     - |  7924 | `	}` |
|    39 |  7925 | `	if( (pArg->iFlags & MEMOBJ_STRING) && PH7_MemObjStringIsNumeric(pArg) ){` |
|     - |  7926 | `		ph7_value sTmp;` |
|     8 |  7927 | `		PH7_MemObjInit(pCtx->pVm,&sTmp);` |
|     8 |  7928 | `		PH7_MemObjStore(pArg,&sTmp);` |
|     8 |  7929 | `		PH7_MemObjToInteger(&sTmp);` |
|     8 |  7930 | `		*piOut = sTmp.x.iVal;` |
|     8 |  7931 | `		PH7_MemObjRelease(&sTmp);` |
|     8 |  7932 | `		return 1;` |
|     - |  7933 | `	}` |
|    33 |  7934 | `	if( pArg->iFlags & MEMOBJ_RES ){` |
|     - |  7935 | `		/* php's offset rule again: a resource is not refused, it WARNS and becomes` |
|     - |  7936 | `		 * its integer id — which for a fixed array is then an ordinary out-of-range` |
|     - |  7937 | ``		 * index. The refusal below had named `resource` instead. Rewrites the`` |
|     - |  7938 | `		 * method's own argument copy, as the store's offsets do. */` |
|     9 |  7939 | `		PH7_VmOffsetResourceWarn(pCtx->pVm,pArg);` |
|     9 |  7940 | `		*piOut = pArg->x.iVal;` |
|     9 |  7941 | `		return 1;` |
|     - |  7942 | `	}` |
|    25 |  7943 | `	*piOut = 0;` |
|    25 |  7944 | `	if( pArg->iFlags & MEMOBJ_OBJ ){` |
|     - |  7945 | `		/* php names the CLASS here, as get_debug_type() does, not the word` |
|     - |  7946 | `		 * "object" — the same rule the store's offsets follow. */` |
|     6 |  7947 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     6 |  7948 | `		SyString *pName = pInst && pInst->pClass ? &pInst->pClass->sName : 0;` |
|     8 |  7949 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|     2 |  7950 | `			"Cannot access offset of type %z on SplFixedArray",pName);` |
|     6 |  7951 | `		return 0;` |
|     - |  7952 | `	}` |
|    30 |  7953 | `	*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|     9 |  7954 | `		"Cannot access offset of type %s on SplFixedArray",ph7_type_name(pArg));` |
|    21 |  7955 | `	return 0;` |
|   154 |  7956 | `}` |
|    12 |  7957 | `static sxi32 FaOutOfBounds(ph7_context *pCtx)` |
|     2 |  7958 | `{` |
|    14 |  7959 | `	return PH7_VmThrowException(pCtx,"OutOfBoundsException","Index invalid or out of range");` |
|     2 |  7960 | `}` |
|     - |  7961 | ``/* php's setSize: grow with nulls, shrink by dropping the tail, answer `true`. */`` |
|   144 |  7962 | `static sxi32 FaResize(ph7_vm *pVm,ph7_class_instance *pThis,sxi64 nNew)` |
|     3 |  7963 | `{` |
|   147 |  7964 | `	sxi64 nOld = FaSize(pThis);` |
|   147 |  7965 | `	ph7_hashmap *pMap = FaMap(pVm,pThis);` |
|     - |  7966 | `	sxi64 i;` |
|   147 |  7967 | `	if( pMap == 0 ){` |
|   ! 0 |  7968 | `		return SXERR_MEM;` |
|     - |  7969 | `	}` |
|   157 |  7970 | `	for( i = nNew ; i < nOld ; ++i ){` |
|    11 |  7971 | `		ph7_hashmap_node *pNode = 0;` |
|    11 |  7972 | `		if( HashmapLookupIntKey(pMap,i,&pNode) == SXRET_OK ){` |
|    11 |  7973 | `			PH7_HashmapUnlinkNode(pNode,TRUE);` |
|     5 |  7974 | `		}` |
|     6 |  7975 | `	}` |
|   487 |  7976 | `	for( i = nOld ; i < nNew ; ++i ){` |
|     - |  7977 | `		ph7_value sNull;` |
|   343 |  7978 | `		PH7_MemObjInit(pVm,&sNull);` |
|   343 |  7979 | `		FaPut(pVm,pThis,i,&sNull);` |
|   343 |  7980 | `		PH7_MemObjRelease(&sNull);` |
|   173 |  7981 | `	}` |
|   147 |  7982 | `	PH7_NativeSetAttrInt(pVm,pThis,FA_N,nNew);` |
|   147 |  7983 | `	return SXRET_OK;` |
|    75 |  7984 | `}` |
|   118 |  7985 | `static int vm_builtin_SplFixedArray_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  7986 | `{` |
|   121 |  7987 | `	ph7_vm *pVm = pCtx->pVm;` |
|   121 |  7988 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   121 |  7989 | `	sxi64 nSize = 0;` |
|   121 |  7990 | `	if( pThis == 0 ){` |
|   ! 0 |  7991 | `		return PH7_OK;` |
|     - |  7992 | `	}` |
|   121 |  7993 | `	if( nArg > 0 ){` |
|   117 |  7994 | `		sxi32 rc = PH7_IntArgResolve(pCtx,apArg[0],"SplFixedArray::__construct",1,"$size","int",&nSize);` |
|   117 |  7995 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  7996 | `			return rc;` |
|     - |  7997 | `		}` |
|    57 |  7998 | `	}` |
|   121 |  7999 | `	if( nSize < 0 ){` |
|     - |  8000 | `		/* php words this from __construct(), not from the setSize() it forwards to —` |
|     - |  8001 | ``		 * which is what the chunk's `$this->setSize()` reported. */`` |
|     3 |  8002 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  8003 | `			"SplFixedArray::__construct(): Argument #1 ($size) must be greater than or equal to 0");` |
|     - |  8004 | `	}` |
|   119 |  8005 | `	FaResize(pVm,pThis,nSize);` |
|   119 |  8006 | `	return PH7_OK;` |
|    62 |  8007 | `}` |
|     6 |  8008 | `static int vm_builtin_SplFixedArray_getSize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8009 | `{` |
|     3 |  8010 | `	SXUNUSED(nArg);` |
|     3 |  8011 | `	SXUNUSED(apArg);` |
|     7 |  8012 | `	ph7_result_int64(pCtx,FaSize(PH7_ContextThis(pCtx)));` |
|     7 |  8013 | `	return PH7_OK;` |
|     1 |  8014 | `}` |
|    12 |  8015 | `static int vm_builtin_SplFixedArray_setSize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8016 | `{` |
|    13 |  8017 | `	sxi64 nSize = 0;` |
|     - |  8018 | `	sxi32 rc;` |
|    13 |  8019 | `	if( nArg < 1 ){` |
|   ! 0 |  8020 | `		return PH7_OK;` |
|     - |  8021 | `	}` |
|    13 |  8022 | `	rc = PH7_IntArgResolve(pCtx,apArg[0],"SplFixedArray::setSize",1,"$size","int",&nSize);` |
|    13 |  8023 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  8024 | `		return rc;` |
|     - |  8025 | `	}` |
|    13 |  8026 | `	if( nSize < 0 ){` |
|     3 |  8027 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  8028 | `			"SplFixedArray::setSize(): Argument #1 ($size) must be greater than or equal to 0");` |
|     - |  8029 | `	}` |
|    11 |  8030 | `	FaResize(pCtx->pVm,PH7_ContextThis(pCtx),nSize);` |
|    11 |  8031 | ``	ph7_result_bool(pCtx,1);   /* php's `true` return type */`` |
|    11 |  8032 | `	return PH7_OK;` |
|     7 |  8033 | `}` |
|    12 |  8034 | `static int vm_builtin_SplFixedArray_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8035 | `{` |
|     6 |  8036 | `	SXUNUSED(nArg);` |
|     6 |  8037 | `	SXUNUSED(apArg);` |
|    13 |  8038 | `	ph7_result_int64(pCtx,FaSize(PH7_ContextThis(pCtx)));` |
|    13 |  8039 | `	return PH7_OK;` |
|     1 |  8040 | `}` |
|    36 |  8041 | `static int vm_builtin_SplFixedArray_toArray(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  8042 | `{` |
|    38 |  8043 | `	ph7_value *pSlot = FaSlot(pCtx->pVm,PH7_ContextThis(pCtx));` |
|    18 |  8044 | `	SXUNUSED(nArg);` |
|    18 |  8045 | `	SXUNUSED(apArg);` |
|    38 |  8046 | `	if( pSlot ){` |
|    38 |  8047 | `		ph7_result_value(pCtx,pSlot);` |
|    18 |  8048 | `	}` |
|    38 |  8049 | `	return PH7_OK;` |
|     2 |  8050 | `}` |
|    22 |  8051 | `static int vm_builtin_SplFixedArray_offsetExists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  8052 | `{` |
|    24 |  8053 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    24 |  8054 | `	sxi64 iIdx = 0;` |
|    24 |  8055 | `	sxi32 rc = PH7_OK;` |
|     - |  8056 | `	ph7_value *pVal;` |
|    24 |  8057 | `	if( nArg < 1 ){` |
|   ! 0 |  8058 | `		return PH7_OK;` |
|     - |  8059 | `	}` |
|     - |  8060 | `	/* offsetExists RAISES for an undecodable offset exactly as the other three do —` |
|     - |  8061 | ``	 * `isset($f['x'])` is a TypeError, not a false — and answers false only for a`` |
|     - |  8062 | `	 * decodable index that is out of range or holds null. */` |
|    24 |  8063 | `	if( !FaOffset(pCtx,apArg[0],&iIdx,&rc) ){` |
|     5 |  8064 | `		return rc;` |
|     - |  8065 | `	}` |
|    20 |  8066 | `	if( iIdx < 0 \|\| iIdx >= FaSize(pThis) ){` |
|     7 |  8067 | `		ph7_result_bool(pCtx,0);` |
|     7 |  8068 | `		return PH7_OK;` |
|     - |  8069 | `	}` |
|     - |  8070 | `	/* php's isset() semantics: an unset slot holds null and is NOT set. */` |
|    14 |  8071 | `	pVal = FaAt(pCtx->pVm,pThis,iIdx);` |
|    14 |  8072 | `	ph7_result_bool(pCtx,pVal != 0 && (pVal->iFlags & MEMOBJ_NULL) == 0);` |
|    14 |  8073 | `	return PH7_OK;` |
|    13 |  8074 | `}` |
|    42 |  8075 | `static int vm_builtin_SplFixedArray_offsetGet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  8076 | `{` |
|    44 |  8077 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    44 |  8078 | `	sxi64 iIdx = 0;` |
|    44 |  8079 | `	sxi32 rc = PH7_OK;` |
|     - |  8080 | `	ph7_value *pVal;` |
|    44 |  8081 | `	if( nArg < 1 ){` |
|   ! 0 |  8082 | `		return PH7_OK;` |
|     - |  8083 | `	}` |
|    44 |  8084 | `	if( !FaOffset(pCtx,apArg[0],&iIdx,&rc) ){` |
|     7 |  8085 | `		return rc;` |
|     - |  8086 | `	}` |
|    38 |  8087 | `	if( iIdx < 0 \|\| iIdx >= FaSize(pThis) ){` |
|     7 |  8088 | `		return FaOutOfBounds(pCtx);` |
|     - |  8089 | `	}` |
|    32 |  8090 | `	pVal = FaAt(pCtx->pVm,pThis,iIdx);` |
|    32 |  8091 | `	if( pVal ){` |
|    32 |  8092 | `		ph7_result_value(pCtx,pVal);` |
|    15 |  8093 | `	}` |
|    32 |  8094 | `	return PH7_OK;` |
|    23 |  8095 | `}` |
|   236 |  8096 | `static int vm_builtin_SplFixedArray_offsetSet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  8097 | `{` |
|   239 |  8098 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   239 |  8099 | `	sxi64 iIdx = 0;` |
|   239 |  8100 | `	sxi32 rc = PH7_OK;` |
|   239 |  8101 | `	if( nArg < 2 ){` |
|   ! 0 |  8102 | `		return PH7_OK;` |
|     - |  8103 | `	}` |
|   239 |  8104 | `	if( !FaOffset(pCtx,apArg[0],&iIdx,&rc) ){` |
|    15 |  8105 | `		return rc;` |
|     - |  8106 | `	}` |
|   226 |  8107 | `	if( iIdx < 0 \|\| iIdx >= FaSize(pThis) ){` |
|     8 |  8108 | `		return FaOutOfBounds(pCtx);` |
|     - |  8109 | `	}` |
|   220 |  8110 | `	FaPut(pCtx->pVm,pThis,iIdx,apArg[1]);` |
|   220 |  8111 | `	return PH7_OK;` |
|   121 |  8112 | `}` |
|     2 |  8113 | `static int vm_builtin_SplFixedArray_offsetUnset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8114 | `{` |
|     3 |  8115 | `	ph7_vm *pVm = pCtx->pVm;` |
|     3 |  8116 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     3 |  8117 | `	sxi64 iIdx = 0;` |
|     3 |  8118 | `	sxi32 rc = PH7_OK;` |
|     - |  8119 | `	ph7_value sNull;` |
|     3 |  8120 | `	if( nArg < 1 ){` |
|   ! 0 |  8121 | `		return PH7_OK;` |
|     - |  8122 | `	}` |
|     3 |  8123 | `	if( !FaOffset(pCtx,apArg[0],&iIdx,&rc) ){` |
|   ! 0 |  8124 | `		return rc;` |
|     - |  8125 | `	}` |
|     3 |  8126 | `	if( iIdx < 0 \|\| iIdx >= FaSize(pThis) ){` |
|   ! 0 |  8127 | `		return FaOutOfBounds(pCtx);` |
|     - |  8128 | `	}` |
|     - |  8129 | `	/* The slot survives at its index and becomes null: the array is FIXED. */` |
|     3 |  8130 | `	PH7_MemObjInit(pVm,&sNull);` |
|     3 |  8131 | `	FaPut(pVm,pThis,iIdx,&sNull);` |
|     3 |  8132 | `	PH7_MemObjRelease(&sNull);` |
|     3 |  8133 | `	return PH7_OK;` |
|     2 |  8134 | `}` |
|    24 |  8135 | `static int vm_builtin_SplFixedArray_fromArray(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8136 | `{` |
|     - |  8137 | `	char zGiven[64];` |
|    25 |  8138 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  8139 | `	ph7_class *pCls;` |
|     - |  8140 | `	ph7_class_instance *pNew;` |
|     - |  8141 | `	ph7_hashmap *pSrc;` |
|     - |  8142 | `	ph7_hashmap_node *pNode,*pPrev;` |
|    25 |  8143 | `	int bPreserve = 1;` |
|    25 |  8144 | `	sxi64 nMax = -1, nNext = 0;` |
|    25 |  8145 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 |  8146 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  8147 | `			"SplFixedArray::fromArray(): Argument #1 ($array) must be of type array, %s given",` |
|   ! 0 |  8148 | `			nArg < 1 ? "none" : VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|     - |  8149 | `	}` |
|    25 |  8150 | `	if( nArg > 1 ){` |
|     7 |  8151 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|     3 |  8152 | `	}` |
|    25 |  8153 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     - |  8154 | `	/* php walks the keys FIRST and refuses the whole call before building anything. */` |
|    25 |  8155 | `	if( bPreserve ){` |
|    39 |  8156 | `		for( pNode = pSrc->pFirst ; pNode ; pNode = pPrev ){` |
|    27 |  8157 | `			pPrev = pNode->pPrev;` |
|    27 |  8158 | `			if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey < 0 ){` |
|     7 |  8159 | `				return PH7_VmThrowException(pCtx,"InvalidArgumentException",` |
|     - |  8160 | `					"array must contain only positive integer keys");` |
|     - |  8161 | `			}` |
|    21 |  8162 | `			if( pNode->xKey.iKey > nMax ){` |
|    19 |  8163 | `				nMax = pNode->xKey.iKey;` |
|     9 |  8164 | `			}` |
|    21 |  8165 | `			if( pNode == pSrc->pFirst && pPrev == 0 ){` |
|   ! 0 |  8166 | `				break;` |
|     - |  8167 | `			}` |
|    11 |  8168 | `		}` |
|     6 |  8169 | `	}` |
|    19 |  8170 | `	pCls = PH7_VmExtractClass(pVm,"SplFixedArray",sizeof("SplFixedArray")-1,FALSE,0);` |
|    19 |  8171 | `	if( pCls == 0 ){` |
|   ! 0 |  8172 | `		return PH7_OK;` |
|     - |  8173 | `	}` |
|    19 |  8174 | `	pNew = PH7_NewClassInstance(pVm,pCls);` |
|    19 |  8175 | `	if( pNew == 0 ){` |
|   ! 0 |  8176 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  8177 | `	}` |
|    19 |  8178 | `	pNew->iRef++;` |
|    19 |  8179 | `	FaResize(pVm,pNew,bPreserve ? nMax + 1 : (sxi64)pSrc->nEntry);` |
|    35 |  8180 | `	for( pNode = pSrc->pFirst ; pNode ; pNode = pPrev ){` |
|    31 |  8181 | `		ph7_value *pVal = HashmapExtractNodeValue(pNode);` |
|    31 |  8182 | `		pPrev = pNode->pPrev;` |
|    31 |  8183 | `		if( pVal ){` |
|    31 |  8184 | `			FaPut(pVm,pNew,bPreserve ? pNode->xKey.iKey : nNext,pVal);` |
|    15 |  8185 | `		}` |
|    31 |  8186 | `		nNext++;` |
|    31 |  8187 | `		if( pPrev == 0 ){` |
|    15 |  8188 | `			break;` |
|     - |  8189 | `		}` |
|     9 |  8190 | `	}` |
|    19 |  8191 | `	PH7_NativeResultObject(pCtx,pNew);` |
|    19 |  8192 | `	PH7_ClassInstanceUnref(pNew);` |
|    19 |  8193 | `	return PH7_OK;` |
|    13 |  8194 | `}` |
|     - |  8195 | `/* php's getIterator() answers an InternalIterator over the elements — the same` |
|     - |  8196 | ` * machinery every native IteratorAggregate here uses, which is also what makes` |
|     - |  8197 | ` * two iterators over one array independent. */` |
|    66 |  8198 | `static void FaIterSettle(ph7_vm *pVm,ph7_class_instance *pIt)` |
|     1 |  8199 | `{` |
|    67 |  8200 | `	ph7_class_instance *pSrc = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);` |
|    67 |  8201 | `	sxi64 iPos = PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS);` |
|     - |  8202 | `	ph7_value *pVal;` |
|    67 |  8203 | `	if( pSrc == 0 \|\| iPos < 0 \|\| iPos >= FaSize(pSrc) ){` |
|    13 |  8204 | `		PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|    13 |  8205 | `		return;` |
|     - |  8206 | `	}` |
|    55 |  8207 | `	pVal = FaAt(&(*pVm),pSrc,iPos);` |
|    55 |  8208 | `	if( pVal ){` |
|    82 |  8209 | `		PH7_NativeSetProp(&(*pVm),pIt,PH7_NATIVE_IT_CUR,` |
|    27 |  8210 | `			(int)SyStrlen(PH7_NATIVE_IT_CUR),pVal);` |
|    27 |  8211 | `	}` |
|    55 |  8212 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_KEY,iPos);` |
|    55 |  8213 | `	PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,0);` |
|    34 |  8214 | `}` |
|    34 |  8215 | `static void FaIterRewind(ph7_vm *pVm,ph7_class_instance *pIt)` |
|     1 |  8216 | `{` |
|    35 |  8217 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_POS,0);` |
|    35 |  8218 | `	FaIterSettle(&(*pVm),pIt);` |
|    35 |  8219 | `}` |
|    32 |  8220 | `static void FaIterNext(ph7_vm *pVm,ph7_class_instance *pIt)` |
|     1 |  8221 | `{` |
|    49 |  8222 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_POS,` |
|    32 |  8223 | `		PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS) + 1);` |
|    33 |  8224 | `	FaIterSettle(&(*pVm),pIt);` |
|    33 |  8225 | `}` |
|     - |  8226 | `static const PH7_NativeIterVtab sFaIterVtab = { FaIterRewind, FaIterNext, 0, 0 };` |
|    18 |  8227 | `static int vm_builtin_SplFixedArray_getIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8228 | `{` |
|    19 |  8229 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  8230 | `	ph7_class_instance *pIt;` |
|     9 |  8231 | `	SXUNUSED(nArg);` |
|     9 |  8232 | `	SXUNUSED(apArg);` |
|    19 |  8233 | `	if( pThis == 0 ){` |
|   ! 0 |  8234 | `		return PH7_OK;` |
|     - |  8235 | `	}` |
|    19 |  8236 | `	pIt = PH7_NativeIteratorNew(pCtx->pVm,pThis);` |
|    19 |  8237 | `	if( pIt == 0 ){` |
|   ! 0 |  8238 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  8239 | `	}` |
|    19 |  8240 | `	PH7_NativeResultObject(pCtx,pIt);` |
|    19 |  8241 | `	return PH7_OK;` |
|    10 |  8242 | `}` |
|     2 |  8243 | `static int vm_builtin_SplFixedArray_wakeup(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8244 | `{` |
|     1 |  8245 | `	SXUNUSED(nArg);` |
|     1 |  8246 | `	SXUNUSED(apArg);` |
|     1 |  8247 | `	SXUNUSED(pCtx);` |
|     3 |  8248 | `	return PH7_OK;   /* php 8.4 keeps it, deprecated, doing nothing */` |
|     1 |  8249 | `}` |
|     - |  8250 | `/*` |
|     - |  8251 | ` * php's __serialize() here is NOT toArray(): the members ride in the SAME array as` |
|     - |  8252 | ` * the elements, told apart by their key — an INT key is an element and a STRING key` |
|     - |  8253 | ` * is a property. That is the whole reason the payload of a SplFixedArray subclass` |
|     - |  8254 | `` * reads `{i:0;N;i:1;N;s:1:"p";i:9;}` and not a nested pair.`` |
|     - |  8255 | ` */` |
|    14 |  8256 | `static int vm_builtin_SplFixedArray_serializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8257 | `{` |
|    15 |  8258 | `	ph7_vm *pVm = pCtx->pVm;` |
|    15 |  8259 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  8260 | `	ph7_value sOut,*pSlot;` |
|     7 |  8261 | `	SXUNUSED(nArg);` |
|     7 |  8262 | `	SXUNUSED(apArg);` |
|    15 |  8263 | `	pSlot = FaSlot(pVm,pThis);` |
|    15 |  8264 | `	PH7_MemObjInit(pVm,&sOut);` |
|    15 |  8265 | `	if( pSlot ){` |
|    15 |  8266 | `		PH7_MemObjStore(pSlot,&sOut);` |
|     7 |  8267 | `	}` |
|    15 |  8268 | `	if( PH7_MemObjToHashmap(&sOut) != SXRET_OK ){` |
|   ! 0 |  8269 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 |  8270 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  8271 | `	}` |
|    15 |  8272 | `	SplAddMembers(pVm,pThis,&sOut);   /* string keys, beside the int-keyed elements */` |
|    15 |  8273 | `	ph7_result_value(pCtx,&sOut);` |
|    15 |  8274 | `	PH7_MemObjRelease(&sOut);` |
|    15 |  8275 | `	return PH7_OK;` |
|     8 |  8276 | `}` |
|     - |  8277 | `/* Split the payload back apart: int keys rebuild the elements, string keys the` |
|     - |  8278 | ` * properties. The element count is what the INT half holds, not the whole array. */` |
|     - |  8279 | `typedef struct fa_unser_ctx fa_unser_ctx;` |
|     - |  8280 | `struct fa_unser_ctx` |
|     - |  8281 | `{` |
|     - |  8282 | `	ph7_class_instance *pThis;` |
|     - |  8283 | `	ph7_value *pElems;` |
|     - |  8284 | `	sxi64 nElem;` |
|     - |  8285 | `};` |
|    18 |  8286 | `static int FaUnserWalk(ph7_value *pKey,ph7_value *pVal,void *pUserData)` |
|     1 |  8287 | `{` |
|    19 |  8288 | `	fa_unser_ctx *pFa = (fa_unser_ctx *)pUserData;` |
|    19 |  8289 | `	if( ph7_value_is_string(pKey) ){` |
|     - |  8290 | `		int nKey;` |
|     5 |  8291 | `		const char *zKey = ph7_value_to_string(pKey,&nKey);` |
|     5 |  8292 | `		PH7_NativeSetProp(pFa->pThis->pVm,pFa->pThis,zKey,(sxu32)nKey,pVal);` |
|     5 |  8293 | `		return PH7_OK;` |
|     - |  8294 | `	}` |
|    15 |  8295 | `	ph7_array_add_elem(pFa->pElems,0,pVal);` |
|    15 |  8296 | `	pFa->nElem++;` |
|    15 |  8297 | `	return PH7_OK;` |
|    10 |  8298 | `}` |
|     6 |  8299 | `static int vm_builtin_SplFixedArray_unserializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8300 | `{` |
|     7 |  8301 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 |  8302 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  8303 | `	fa_unser_ctx sFa;` |
|     - |  8304 | `	ph7_value sElems,*pSlot;` |
|     7 |  8305 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 \|\| pThis == 0 ){` |
|   ! 0 |  8306 | `		return PH7_OK;` |
|     - |  8307 | `	}` |
|     7 |  8308 | `	PH7_MemObjInit(pVm,&sElems);` |
|     7 |  8309 | `	if( PH7_MemObjToHashmap(&sElems) != SXRET_OK ){` |
|   ! 0 |  8310 | `		PH7_MemObjRelease(&sElems);` |
|   ! 0 |  8311 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  8312 | `	}` |
|     7 |  8313 | `	sFa.pThis = pThis;` |
|     7 |  8314 | `	sFa.pElems = &sElems;` |
|     7 |  8315 | `	sFa.nElem = 0;` |
|     7 |  8316 | `	ph7_array_walk(apArg[0],FaUnserWalk,&sFa);` |
|     7 |  8317 | `	pSlot = PH7_NativeAttr(pThis,FA_A);` |
|     7 |  8318 | `	if( pSlot ){` |
|     7 |  8319 | `		PH7_MemObjRelease(pSlot);` |
|     7 |  8320 | `		PH7_MemObjStore(&sElems,pSlot);` |
|     3 |  8321 | `	}` |
|     7 |  8322 | `	PH7_MemObjRelease(&sElems);` |
|     7 |  8323 | `	PH7_NativeSetAttrInt(pVm,pThis,FA_N,sFa.nElem);` |
|     7 |  8324 | `	return PH7_OK;` |
|     4 |  8325 | `}` |
|     - |  8326 | `/*` |
|     - |  8327 | ` * php's get_properties: the ELEMENTS, keyed by index, on every surface —` |
|     - |  8328 | ` * var_dump, print_r, the (array) cast and (through __serialize) serialize(). This` |
|     - |  8329 | ` * is the one native class so far whose presentation is the same for the debug and` |
|     - |  8330 | ` * the cast form.` |
|     - |  8331 | ` */` |
|     2 |  8332 | `static sxi32 FaPresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|     1 |  8333 | `{` |
|     3 |  8334 | `	sxi64 n = FaSize(pThis), i;` |
|     1 |  8335 | `	SXUNUSED(bDebug);` |
|     9 |  8336 | `	for( i = 0 ; i < n ; ++i ){` |
|     7 |  8337 | `		ph7_value sKey,*pVal = FaAt(&(*pVm),pThis,i);` |
|     7 |  8338 | `		if( pVal == 0 ){` |
|   ! 0 |  8339 | `			continue;` |
|     - |  8340 | `		}` |
|     7 |  8341 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,i);` |
|     7 |  8342 | `		ph7_array_add_elem(pOut,&sKey,pVal);` |
|     7 |  8343 | `		PH7_MemObjRelease(&sKey);` |
|     4 |  8344 | `	}` |
|     3 |  8345 | `	return PH7_OK;` |
|     1 |  8346 | `}` |
|     - |  8347 | `/*` |
|     - |  8348 | ` * The declaration. Method ORDER and the interface list are spl_fixedarray.stub's;` |
|     - |  8349 | ` * note that __construct, __serialize, __unserialize, getIterator and jsonSerialize` |
|     - |  8350 | ` * are the FIVE methods php does NOT mark tentative here.` |
|     - |  8351 | ` */` |
|  6721 |  8352 | `static sxi32 VmInstallSplFixedArray(ph7_vm *pVm)` |
|     5 |  8353 | `{` |
|     - |  8354 | `	static const PH7_NativePropDef aFaProp[] = {` |
|     - |  8355 | `		{ FA_A, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  8356 | `		{ FA_N, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  8357 | `	};` |
|     - |  8358 | `	static const PH7_NativeMethodDef aFaMethod[] = {` |
|     - |  8359 | `		{ "__construct",   PH7_MOD_PUBLIC, "int $size = 0", 0,` |
|     - |  8360 | `		  vm_builtin_SplFixedArray_construct },` |
|     - |  8361 | `		{ "__wakeup",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplFixedArray_wakeup },` |
|     - |  8362 | `		{ "__serialize",   PH7_MOD_PUBLIC, "", "array", vm_builtin_SplFixedArray_serializeMagic },` |
|     - |  8363 | `		{ "__unserialize", PH7_MOD_PUBLIC, "array $data", "void",` |
|     - |  8364 | `		  vm_builtin_SplFixedArray_unserializeMagic },` |
|     - |  8365 | `		{ "count",         PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplFixedArray_count },` |
|     - |  8366 | `		{ "toArray",       PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplFixedArray_toArray },` |
|     - |  8367 | `		{ "fromArray",     PH7_MOD_PUBLIC\|PH7_MOD_STATIC,` |
|     - |  8368 | `		  "array $array, bool $preserveKeys = true", "@SplFixedArray",` |
|     - |  8369 | `		  vm_builtin_SplFixedArray_fromArray },` |
|     - |  8370 | `		{ "getSize",       PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplFixedArray_getSize },` |
|     - |  8371 | `		{ "setSize",       PH7_MOD_PUBLIC, "int $size", "@true", vm_builtin_SplFixedArray_setSize },` |
|     - |  8372 | `		/* php's stub leaves the four offsets UNTYPED and decodes them itself, the` |
|     - |  8373 | `		 * same shape SplDoublyLinkedList has — but a different rule and a different` |
|     - |  8374 | `		 * refusal, so FaOffset() rather than the DLL's PH7_IntArgResolve. */` |
|     - |  8375 | `		{ "offsetExists",  PH7_MOD_PUBLIC, "$index", "@bool",` |
|     - |  8376 | `		  vm_builtin_SplFixedArray_offsetExists },` |
|     - |  8377 | `		{ "offsetGet",     PH7_MOD_PUBLIC, "$index", "@mixed", vm_builtin_SplFixedArray_offsetGet },` |
|     - |  8378 | `		{ "offsetSet",     PH7_MOD_PUBLIC, "$index, mixed $value", "@void",` |
|     - |  8379 | `		  vm_builtin_SplFixedArray_offsetSet },` |
|     - |  8380 | `		{ "offsetUnset",   PH7_MOD_PUBLIC, "$index", "@void",` |
|     - |  8381 | `		  vm_builtin_SplFixedArray_offsetUnset },` |
|     - |  8382 | `		{ "getIterator",   PH7_MOD_PUBLIC, "", "Iterator", vm_builtin_SplFixedArray_getIterator },` |
|     - |  8383 | `		{ "jsonSerialize", PH7_MOD_PUBLIC, "", "array", vm_builtin_SplFixedArray_toArray },` |
|     - |  8384 | `	};` |
|     - |  8385 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - |  8386 | `		{ "SplFixedArray", 0, "IteratorAggregate,ArrayAccess,Countable,JsonSerializable", 0,` |
|     - |  8387 | `		  aFaMethod, SX_ARRAYSIZE(aFaMethod), 0, 0,` |
|     - |  8388 | `		  aFaProp, SX_ARRAYSIZE(aFaProp), 0, &sFaIterVtab, FaPresent },` |
|     - |  8389 | `	};` |
|  6726 |  8390 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 |  8391 | `}` |
|     - |  8392 | `/*` |
|     - |  8393 | ` * ---------------------------------------------------------------------------` |
|     - |  8394 | ` * SplObjectStorage, SplObserver and SplSubject.` |
|     - |  8395 | ` *` |
|     - |  8396 | `` * php's `spl_SplObjectStorage` is a hashtable of {obj, inf} pairs keyed by the`` |
|     - |  8397 | `` * object HANDLE, plus TWO cursors that are not the same thing: `pos` walks the`` |
|     - |  8398 | `` * table and `index` is the integer `key()` reports. Every method that changes the`` |
|     - |  8399 | ` * membership resets one or both, and the chunk -- which kept a single integer` |
|     - |  8400 | `` * offset -- had none of that: `detach()` mid-walk left the walk where it was`` |
|     - |  8401 | `` * (php restarts it), and `addAll()` left `key()` counting from wherever it stood.`` |
|     - |  8402 | ` *` |
|     - |  8403 | `` * What the chunk did not have AT ALL, which is most of the class: `seek()` and the`` |
|     - |  8404 | `` * `SeekableIterator` interface it comes from, `Serializable` with its`` |
|     - |  8405 | `` * `serialize()`/`unserialize()` pair, the `__serialize()`/`__unserialize()` pair`` |
|     - |  8406 | `` * php actually uses, and `__debugInfo()`. Six methods and two interfaces missing`` |
|     - |  8407 | ` * from a 25-method class -- rule 53, and the reason a method-by-method reading is` |
|     - |  8408 | ` * not a conversion.` |
|     - |  8409 | ` *` |
|     - |  8410 | `` * Two more the model hides. **`current()` on an invalid iterator RAISES**`` |
|     - |  8411 | `` * (`Called current() on invalid iterator`) where the chunk answered null, and`` |
|     - |  8412 | `` * **an overridden `getHash()` is what keys the table** -- php looks the method up`` |
|     - |  8413 | `` * once per instance (`fptr_get_hash`) and every attach/detach/contains goes`` |
|     - |  8414 | ` * through it, so a subclass that hashes two distinct objects the same stores ONE` |
|     - |  8415 | `` * entry. The chunk called `spl_object_id()` directly and ignored its own`` |
|     - |  8416 | `` * `getHash()`, so overriding it did nothing.`` |
|     - |  8417 | ` *` |
|     - |  8418 | ` * php DEPRECATES attach/detach/contains since 8.5 and PHL says nothing, which is` |
|     - |  8419 | ` * the same non-deprecated-compatibility policy the chunk carried (the notice is` |
|     - |  8420 | ` * the only difference and no valid php depends on it).` |
|     - |  8421 | ` */` |
|     - |  8422 | `#define SOS_S "__s"   /* php's storage: key -> ['obj' => object, 'inf' => info] */` |
|     - |  8423 | `#define SOS_I "__i"   /* php's index: what key() reports, NOT a position */` |
|     - |  8424 |  |
|     - |  8425 | `/* The storage slot, separated for writing (every caller may mutate it). */` |
|   742 |  8426 | `static ph7_value * SosSlot(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  8427 | `{` |
|   743 |  8428 | `	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,SOS_S) : 0;` |
|   743 |  8429 | `	if( pSlot == 0 ){` |
|   ! 0 |  8430 | `		return 0;` |
|     - |  8431 | `	}` |
|   743 |  8432 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   129 |  8433 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|   ! 0 |  8434 | `			return 0;` |
|     - |  8435 | `		}` |
|    64 |  8436 | `	}` |
|   743 |  8437 | `	if( PH7_HashmapCowSeparate(pVm,pSlot) == 0 ){` |
|   ! 0 |  8438 | `		return 0;` |
|     - |  8439 | `	}` |
|     - |  8440 | `	/* Re-ask: the separation can MOVE the memobj pool this slot lives in` |
|     - |  8441 | `	 * (SplStoreSlot explains it). */` |
|   743 |  8442 | `	return PH7_NativeAttr(pThis,SOS_S);` |
|   372 |  8443 | `}` |
|   742 |  8444 | `static ph7_hashmap * SosMap(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  8445 | `{` |
|   743 |  8446 | `	ph7_value *pSlot = SosSlot(pVm,pThis);` |
|   743 |  8447 | `	return pSlot ? (ph7_hashmap *)pSlot->x.pOther : 0;` |
|     1 |  8448 | `}` |
|     - |  8449 | `/* One half of a stored pair: php's element->obj / element->inf. */` |
|   564 |  8450 | `static ph7_value * SosPart(ph7_value *pPair,const char *zKey)` |
|     1 |  8451 | `{` |
|   565 |  8452 | `	ph7_hashmap_node *pNode = 0;` |
|   565 |  8453 | `	if( pPair == 0 \|\| (pPair->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     5 |  8454 | `		return 0;` |
|     - |  8455 | `	}` |
|   840 |  8456 | `	if( HashmapLookupBlobKey((ph7_hashmap *)pPair->x.pOther,zKey,` |
|   841 |  8457 | `		(sxu32)SyStrlen(zKey),&pNode) != SXRET_OK ){` |
|   ! 0 |  8458 | `		return 0;` |
|     - |  8459 | `	}` |
|   561 |  8460 | `	return HashmapExtractNodeValue(pNode);` |
|   283 |  8461 | `}` |
|     - |  8462 | ``/* Write one half of a pair; a NULL value is php's `ZVAL_NULL(&element->inf)`. */`` |
|   390 |  8463 | `static void SosSetPart(ph7_vm *pVm,ph7_value *pPair,const char *zKey,ph7_value *pVal)` |
|     1 |  8464 | `{` |
|     - |  8465 | `	ph7_value sKey,sNull;` |
|   391 |  8466 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|   391 |  8467 | `	PH7_MemObjStringAppend(&sKey,zKey,(sxu32)SyStrlen(zKey));` |
|   391 |  8468 | `	if( pVal ){` |
|   391 |  8469 | `		ph7_array_add_elem(pPair,&sKey,pVal);` |
|   196 |  8470 | `	}else{` |
|   ! 0 |  8471 | `		PH7_MemObjInit(pVm,&sNull);` |
|   ! 0 |  8472 | `		ph7_array_add_elem(pPair,&sKey,&sNull);` |
|   ! 0 |  8473 | `		PH7_MemObjRelease(&sNull);` |
|     - |  8474 | `	}` |
|   391 |  8475 | `	PH7_MemObjRelease(&sKey);` |
|   391 |  8476 | `}` |
|     - |  8477 | ``/* The pair a node holds, separated: php mutates `element->inf` in place, and here`` |
|     - |  8478 | ` * that is a NESTED array whose COW copy has to be broken first -- a pair handed` |
|     - |  8479 | ` * out by __serialize()/__debugInfo() would otherwise change with it. */` |
|    10 |  8480 | `static ph7_value * SosPairForWrite(ph7_vm *pVm,ph7_hashmap_node *pNode)` |
|     1 |  8481 | `{` |
|    11 |  8482 | `	ph7_value *pPair = HashmapExtractNodeValue(pNode);` |
|    11 |  8483 | `	if( pPair == 0 \|\| (pPair->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 |  8484 | `		return 0;` |
|     - |  8485 | `	}` |
|    11 |  8486 | `	return PH7_HashmapCowSeparate(pVm,pPair) ? pPair : 0;` |
|     6 |  8487 | `}` |
|     - |  8488 | `/*` |
|     - |  8489 | `` * php's `fptr_get_hash`: the class caches the method ONLY when a subclass declares`` |
|     - |  8490 | ` * its own, and every keyed operation then runs it. The one native body is this` |
|     - |  8491 | ` * class's own, so a non-native getHash() IS the override.` |
|     - |  8492 | ` */` |
|   266 |  8493 | `static ph7_class_method * SosUserHash(ph7_class_instance *pThis)` |
|     1 |  8494 | `{` |
|     - |  8495 | `	ph7_class_method *pMethod;` |
|   267 |  8496 | `	if( pThis == 0 ){` |
|   ! 0 |  8497 | `		return 0;` |
|     - |  8498 | `	}` |
|   267 |  8499 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"getHash",sizeof("getHash")-1);` |
|   267 |  8500 | `	if( pMethod == 0 \|\| (pMethod->sFunc.iFlags & VM_FUNC_NATIVE) ){` |
|   261 |  8501 | `		return 0;` |
|     - |  8502 | `	}` |
|     7 |  8503 | `	return pMethod;` |
|   134 |  8504 | `}` |
|     - |  8505 | `/*` |
|     - |  8506 | ` * php's spl_object_storage_get_hash: the object HANDLE, or the STRING an` |
|     - |  8507 | ` * overridden getHash() answers. php checks the returned type itself (its own` |
|     - |  8508 | ` * return declaration would coerce first, so this only fires for an untyped` |
|     - |  8509 | ` * override) and names the RUNTIME class in the refusal.` |
|     - |  8510 | ` */` |
|   266 |  8511 | `static sxi32 SosKey(ph7_context *pCtx,ph7_class_instance *pThis,ph7_value *pObj,ph7_value *pKey)` |
|     1 |  8512 | `{` |
|   267 |  8513 | `	ph7_vm *pVm = pCtx->pVm;` |
|   267 |  8514 | `	ph7_class_method *pHash = SosUserHash(pThis);` |
|     - |  8515 | `	ph7_value sRes,*apArg[1];` |
|     - |  8516 | `	sxi32 rc;` |
|   267 |  8517 | `	if( pHash == 0 ){` |
|   261 |  8518 | `		ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|   261 |  8519 | `		PH7_MemObjRelease(pKey);` |
|   261 |  8520 | `		PH7_MemObjInitFromInt(pVm,pKey,(sxi64)pInst->nObjId);` |
|   261 |  8521 | `		return PH7_OK;` |
|     - |  8522 | `	}` |
|     7 |  8523 | `	PH7_MemObjInit(pVm,&sRes);` |
|     7 |  8524 | `	apArg[0] = pObj;` |
|     7 |  8525 | `	rc = PH7_VmCallClassMethod(pVm,pThis,pHash,&sRes,1,apArg);` |
|     7 |  8526 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  8527 | `		PH7_MemObjRelease(&sRes);` |
|   ! 0 |  8528 | `		return rc;` |
|     - |  8529 | `	}` |
|     7 |  8530 | `	if( (sRes.iFlags & MEMOBJ_STRING) == 0 ){` |
|     - |  8531 | `		char zGiven[64];` |
|   ! 0 |  8532 | `		SyString *pName = &pThis->pClass->sName;` |
|   ! 0 |  8533 | `		PH7_MemObjRelease(&sRes);` |
|   ! 0 |  8534 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  8535 | `			"%z::getHash(): Return value must be of type string, %s returned",` |
|   ! 0 |  8536 | `			pName,VmValueGivenName(&sRes,zGiven,sizeof(zGiven)));` |
|     - |  8537 | `	}` |
|     7 |  8538 | `	PH7_MemObjRelease(pKey);` |
|     7 |  8539 | `	PH7_MemObjInit(pVm,pKey);` |
|     7 |  8540 | `	PH7_MemObjStore(&sRes,pKey);` |
|     7 |  8541 | `	PH7_MemObjRelease(&sRes);` |
|     7 |  8542 | `	return PH7_OK;` |
|   134 |  8543 | `}` |
|     - |  8544 | `/*` |
|     - |  8545 | ` * php's Z_PARAM_OBJ for the four ArrayAccess offsets: their stub leaves $object` |
|     - |  8546 | `` * UNTYPED (a `@param object` docblock, which Reflection does not print) while the`` |
|     - |  8547 | ` * ZPP is an object, so the declared type says nothing and each body words the` |
|     - |  8548 | ` * refusal here -- SplDoublyLinkedList's $index has the same shape one type over.` |
|     - |  8549 | ` * The name is always this class's, even from a subclass (php's).` |
|     - |  8550 | ` */` |
|   172 |  8551 | `static sxi32 SosObjectArg(ph7_context *pCtx,const char *zMethod,int nArg,ph7_value **apArg,` |
|     - |  8552 | `	ph7_value **ppObj)` |
|     1 |  8553 | `{` |
|     - |  8554 | `	char zGiven[64];` |
|   173 |  8555 | `	*ppObj = 0;` |
|   173 |  8556 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) && apArg[0]->x.pOther ){` |
|   165 |  8557 | `		*ppObj = apArg[0];` |
|   165 |  8558 | `		return PH7_OK;` |
|     - |  8559 | `	}` |
|    17 |  8560 | `	return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  8561 | `		"SplObjectStorage::%s(): Argument #1 ($object) must be of type object, %s given",` |
|     8 |  8562 | `		zMethod,nArg < 1 ? "none" : VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|    87 |  8563 | `}` |
|     - |  8564 | `/* The other storage a set operation takes; php's ZPP already screened the class. */` |
|    12 |  8565 | `static ph7_class_instance * SosOther(int nArg,ph7_value **apArg)` |
|     1 |  8566 | `{` |
|    13 |  8567 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 |  8568 | `		return 0;` |
|     - |  8569 | `	}` |
|    13 |  8570 | `	return (ph7_class_instance *)apArg[0]->x.pOther;` |
|     7 |  8571 | `}` |
|     - |  8572 | `/*` |
|     - |  8573 | ` * php's spl_object_storage_attach. The two values are COPIED first: computing the` |
|     - |  8574 | ` * key can run an overridden getHash(), and any call into user code moves every` |
|     - |  8575 | ` * ph7_value the caller is holding (rule 47).` |
|     - |  8576 | ` */` |
|   198 |  8577 | `static sxi32 SosAttach(ph7_context *pCtx,ph7_class_instance *pThis,ph7_value *pObj,ph7_value *pInf)` |
|     1 |  8578 | `{` |
|   199 |  8579 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  8580 | `	ph7_hashmap *pMap;` |
|   199 |  8581 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  8582 | `	ph7_value sObj,sInf,sKey,sPair;` |
|     - |  8583 | `	sxi32 rc;` |
|   199 |  8584 | `	PH7_MemObjInit(pVm,&sObj);` |
|   199 |  8585 | `	PH7_MemObjInit(pVm,&sInf);` |
|   199 |  8586 | `	PH7_MemObjInit(pVm,&sKey);` |
|   199 |  8587 | `	PH7_MemObjStore(pObj,&sObj);` |
|   199 |  8588 | `	if( pInf ){` |
|   195 |  8589 | `		PH7_MemObjStore(pInf,&sInf);` |
|    97 |  8590 | `	}` |
|   199 |  8591 | `	rc = SosKey(pCtx,pThis,&sObj,&sKey);` |
|   199 |  8592 | `	if( rc != PH7_OK ){` |
|   ! 0 |  8593 | `		goto done;` |
|     - |  8594 | `	}` |
|   199 |  8595 | `	pMap = SosMap(pVm,pThis);` |
|   199 |  8596 | `	if( pMap == 0 ){` |
|   ! 0 |  8597 | `		goto done;` |
|     - |  8598 | `	}` |
|   199 |  8599 | `	if( PH7_HashmapLookup(pMap,&sKey,&pNode) == SXRET_OK ){` |
|     9 |  8600 | `		ph7_value *pPair = SosPairForWrite(pVm,pNode);` |
|     9 |  8601 | `		if( pPair ){` |
|     9 |  8602 | `			SosSetPart(pVm,pPair,"inf",&sInf);` |
|     4 |  8603 | `		}` |
|     9 |  8604 | `		goto done;` |
|     - |  8605 | `	}` |
|   191 |  8606 | `	PH7_MemObjInit(pVm,&sPair);` |
|   191 |  8607 | `	if( PH7_MemObjToHashmap(&sPair) != SXRET_OK ){` |
|   ! 0 |  8608 | `		PH7_MemObjRelease(&sPair);` |
|   ! 0 |  8609 | `		rc = PH7_ContextMemoryError(pCtx);` |
|   ! 0 |  8610 | `		goto done;` |
|     - |  8611 | `	}` |
|   191 |  8612 | `	SosSetPart(pVm,&sPair,"obj",&sObj);` |
|   191 |  8613 | `	SosSetPart(pVm,&sPair,"inf",&sInf);` |
|     - |  8614 | `	/* php's position is an INTEGER index into the bucket array, so a cursor that` |
|     - |  8615 | `	 * ran off the end is revived by the insert and the walk resumes on the new` |
|     - |  8616 | `	 * element -- what addAll() mid-iteration does there. */` |
|   191 |  8617 | `	SplStoreInsert(pMap,&sKey,&sPair);` |
|   191 |  8618 | `	PH7_MemObjRelease(&sPair);` |
|    99 |  8619 | `done:` |
|   199 |  8620 | `	PH7_MemObjRelease(&sObj);` |
|   199 |  8621 | `	PH7_MemObjRelease(&sInf);` |
|   199 |  8622 | `	PH7_MemObjRelease(&sKey);` |
|   199 |  8623 | `	return rc;` |
|     1 |  8624 | `}` |
|     - |  8625 | `/* php's spl_object_storage_detach: drop the entry, saying whether there was one. */` |
|    20 |  8626 | `static sxi32 SosDetach(ph7_context *pCtx,ph7_class_instance *pThis,ph7_value *pObj,int *pbGone)` |
|     1 |  8627 | `{` |
|    21 |  8628 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  8629 | `	ph7_hashmap *pMap;` |
|    21 |  8630 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  8631 | `	ph7_value sObj,sKey;` |
|     - |  8632 | `	sxi32 rc;` |
|    21 |  8633 | `	if( pbGone ){` |
|   ! 0 |  8634 | `		*pbGone = 0;` |
|   ! 0 |  8635 | `	}` |
|    21 |  8636 | `	PH7_MemObjInit(pVm,&sObj);` |
|    21 |  8637 | `	PH7_MemObjInit(pVm,&sKey);` |
|    21 |  8638 | `	PH7_MemObjStore(pObj,&sObj);` |
|    21 |  8639 | `	rc = SosKey(pCtx,pThis,&sObj,&sKey);` |
|    21 |  8640 | `	if( rc == PH7_OK ){` |
|    21 |  8641 | `		pMap = SosMap(pVm,pThis);` |
|    21 |  8642 | `		if( pMap && PH7_HashmapLookup(pMap,&sKey,&pNode) == SXRET_OK ){` |
|    19 |  8643 | `			PH7_HashmapUnlinkNode(pNode,TRUE);` |
|    19 |  8644 | `			if( pbGone ){` |
|   ! 0 |  8645 | `				*pbGone = 1;` |
|   ! 0 |  8646 | `			}` |
|     9 |  8647 | `		}` |
|    10 |  8648 | `	}` |
|    21 |  8649 | `	PH7_MemObjRelease(&sObj);` |
|    21 |  8650 | `	PH7_MemObjRelease(&sKey);` |
|    21 |  8651 | `	return rc;` |
|     1 |  8652 | `}` |
|     - |  8653 | `/* php's spl_object_storage_contains: an entry EXISTS, whatever its info holds. */` |
|    20 |  8654 | `static sxi32 SosContains(ph7_context *pCtx,ph7_class_instance *pThis,ph7_value *pObj,int *pbFound)` |
|     1 |  8655 | `{` |
|    21 |  8656 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  8657 | `	ph7_hashmap *pMap;` |
|    21 |  8658 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  8659 | `	ph7_value sObj,sKey;` |
|     - |  8660 | `	sxi32 rc;` |
|    21 |  8661 | `	*pbFound = 0;` |
|    21 |  8662 | `	PH7_MemObjInit(pVm,&sObj);` |
|    21 |  8663 | `	PH7_MemObjInit(pVm,&sKey);` |
|    21 |  8664 | `	PH7_MemObjStore(pObj,&sObj);` |
|    21 |  8665 | `	rc = SosKey(pCtx,pThis,&sObj,&sKey);` |
|    21 |  8666 | `	if( rc == PH7_OK ){` |
|    21 |  8667 | `		pMap = SosMap(pVm,pThis);` |
|    21 |  8668 | `		*pbFound = pMap && PH7_HashmapLookup(pMap,&sKey,&pNode) == SXRET_OK;` |
|    10 |  8669 | `	}` |
|    21 |  8670 | `	PH7_MemObjRelease(&sObj);` |
|    21 |  8671 | `	PH7_MemObjRelease(&sKey);` |
|    21 |  8672 | `	return rc;` |
|     1 |  8673 | `}` |
|     - |  8674 | ``/* php's `zend_hash_internal_pointer_reset_ex(&storage, &pos); index = 0`. */`` |
|    36 |  8675 | `static void SosRewind(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  8676 | `{` |
|    37 |  8677 | `	ph7_hashmap *pMap = SosMap(pVm,pThis);` |
|    37 |  8678 | `	if( pMap ){` |
|    37 |  8679 | `		pMap->pCur = pMap->pFirst;` |
|    18 |  8680 | `	}` |
|    37 |  8681 | `	PH7_NativeSetAttrInt(pVm,pThis,SOS_I,0);` |
|    37 |  8682 | `}` |
|     - |  8683 | `/* The pair the cursor is on, or 0 past the end. */` |
|    84 |  8684 | `static ph7_value * SosCurrentPair(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  8685 | `{` |
|    85 |  8686 | `	ph7_hashmap *pMap = SosMap(pVm,pThis);` |
|    85 |  8687 | `	if( pMap == 0 \|\| pMap->pCur == 0 ){` |
|     5 |  8688 | `		return 0;` |
|     - |  8689 | `	}` |
|    81 |  8690 | `	return HashmapExtractNodeValue(pMap->pCur);` |
|    43 |  8691 | `}` |
|     - |  8692 | `/*` |
|     - |  8693 | ` * A SNAPSHOT of one storage's pairs as a plain list. Every set operation walks one` |
|     - |  8694 | ` * storage while writing to another -- and either walk can run an overridden` |
|     - |  8695 | ` * getHash(), which moves things (rule 47) and can even mutate the map being walked.` |
|     - |  8696 | ` * php's own SPL_SAFE_HASH_FOREACH_PTR is the same precaution one layer down.` |
|     - |  8697 | ` */` |
|    12 |  8698 | `static sxi32 SosSnapshot(ph7_vm *pVm,ph7_class_instance *pFrom,ph7_value *pOut)` |
|     1 |  8699 | `{` |
|    13 |  8700 | `	ph7_hashmap *pMap = SosMap(pVm,pFrom);` |
|     - |  8701 | `	ph7_hashmap_node *pNode;` |
|     - |  8702 | `	sxu32 n;` |
|    13 |  8703 | `	if( PH7_MemObjToHashmap(pOut) != SXRET_OK ){` |
|   ! 0 |  8704 | `		return SXERR_MEM;` |
|     - |  8705 | `	}` |
|    13 |  8706 | `	if( pMap == 0 ){` |
|   ! 0 |  8707 | `		return SXRET_OK;` |
|     - |  8708 | `	}` |
|    13 |  8709 | `	pNode = pMap->pFirst;` |
|    37 |  8710 | `	for( n = 0 ; n < pMap->nEntry && pNode ; ++n ){` |
|    25 |  8711 | `		ph7_value *pPair = HashmapExtractNodeValue(pNode);` |
|    25 |  8712 | `		if( pPair ){` |
|    25 |  8713 | `			ph7_array_add_elem(pOut,0,pPair);` |
|    12 |  8714 | `		}` |
|    25 |  8715 | `		pNode = pNode->pPrev;   /* insertion order: pFirst, then the pPrev chain */` |
|    13 |  8716 | `	}` |
|    13 |  8717 | `	return SXRET_OK;` |
|     7 |  8718 | `}` |
|     - |  8719 | `/* One pair of a snapshot, re-resolved by index because a user call may have moved` |
|     - |  8720 | ` * every value in the pool since the last one. */` |
|    28 |  8721 | `static ph7_value * SosSnapAt(ph7_value *pSnap,sxi64 i)` |
|     1 |  8722 | `{` |
|    29 |  8723 | `	ph7_hashmap_node *pNode = 0;` |
|    28 |  8724 | `	if( (pSnap->iFlags & MEMOBJ_HASHMAP) == 0` |
|    29 |  8725 | `	 \|\| HashmapLookupIntKey((ph7_hashmap *)pSnap->x.pOther,i,&pNode) != SXRET_OK ){` |
|   ! 0 |  8726 | `		return 0;` |
|     - |  8727 | `	}` |
|    29 |  8728 | `	return HashmapExtractNodeValue(pNode);` |
|    15 |  8729 | `}` |
|     4 |  8730 | `static int vm_builtin_SplObjectStorage_attach(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8731 | `{` |
|     - |  8732 | `	ph7_value *pObj;` |
|     5 |  8733 | `	sxi32 rc = SosObjectArg(pCtx,"attach",nArg,apArg,&pObj);` |
|     5 |  8734 | `	if( rc != PH7_OK ){` |
|   ! 0 |  8735 | `		return rc;` |
|     - |  8736 | `	}` |
|     5 |  8737 | `	return SosAttach(pCtx,PH7_ContextThis(pCtx),pObj,nArg > 1 ? apArg[1] : 0);` |
|     3 |  8738 | `}` |
|     - |  8739 | `/* php's offsetSet is an @implementation-alias of attach, and its refusal says so. */` |
|   116 |  8740 | `static int vm_builtin_SplObjectStorage_offsetSet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8741 | `{` |
|     - |  8742 | `	ph7_value *pObj;` |
|   117 |  8743 | `	sxi32 rc = SosObjectArg(pCtx,"offsetSet",nArg,apArg,&pObj);` |
|   117 |  8744 | `	if( rc != PH7_OK ){` |
|     3 |  8745 | `		return rc;` |
|     - |  8746 | `	}` |
|   115 |  8747 | `	return SosAttach(pCtx,PH7_ContextThis(pCtx),pObj,nArg > 1 ? apArg[1] : 0);` |
|    59 |  8748 | `}` |
|     - |  8749 | `/* detach() RESTARTS the walk: php resets both the position and the index, whether` |
|     - |  8750 | ` * or not anything was removed. */` |
|     4 |  8751 | `static sxi32 SosDetachMethod(ph7_context *pCtx,const char *zMethod,int nArg,ph7_value **apArg)` |
|     1 |  8752 | `{` |
|     5 |  8753 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  8754 | `	ph7_value *pObj;` |
|     5 |  8755 | `	sxi32 rc = SosObjectArg(pCtx,zMethod,nArg,apArg,&pObj);` |
|     5 |  8756 | `	if( rc != PH7_OK ){` |
|   ! 0 |  8757 | `		return rc;` |
|     - |  8758 | `	}` |
|     5 |  8759 | `	rc = SosDetach(pCtx,pThis,pObj,0);` |
|     5 |  8760 | `	if( rc != PH7_OK ){` |
|   ! 0 |  8761 | `		return rc;` |
|     - |  8762 | `	}` |
|     5 |  8763 | `	SosRewind(pCtx->pVm,pThis);` |
|     5 |  8764 | `	return PH7_OK;` |
|     3 |  8765 | `}` |
|   ! 0 |  8766 | `static int vm_builtin_SplObjectStorage_detach(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 |  8767 | `{` |
|   ! 0 |  8768 | `	return SosDetachMethod(pCtx,"detach",nArg,apArg);` |
|   ! 0 |  8769 | `}` |
|     4 |  8770 | `static int vm_builtin_SplObjectStorage_offsetUnset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8771 | `{` |
|     5 |  8772 | `	return SosDetachMethod(pCtx,"offsetUnset",nArg,apArg);` |
|     1 |  8773 | `}` |
|    16 |  8774 | `static sxi32 SosContainsMethod(ph7_context *pCtx,const char *zMethod,int nArg,ph7_value **apArg)` |
|     1 |  8775 | `{` |
|     - |  8776 | `	ph7_value *pObj;` |
|    17 |  8777 | `	int bFound = 0;` |
|    17 |  8778 | `	sxi32 rc = SosObjectArg(pCtx,zMethod,nArg,apArg,&pObj);` |
|    17 |  8779 | `	if( rc != PH7_OK ){` |
|     3 |  8780 | `		return rc;` |
|     - |  8781 | `	}` |
|    15 |  8782 | `	rc = SosContains(pCtx,PH7_ContextThis(pCtx),pObj,&bFound);` |
|    15 |  8783 | `	if( rc != PH7_OK ){` |
|   ! 0 |  8784 | `		return rc;` |
|     - |  8785 | `	}` |
|    15 |  8786 | `	ph7_result_bool(pCtx,bFound);` |
|    15 |  8787 | `	return PH7_OK;` |
|     9 |  8788 | `}` |
|     6 |  8789 | `static int vm_builtin_SplObjectStorage_contains(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8790 | `{` |
|     7 |  8791 | `	return SosContainsMethod(pCtx,"contains",nArg,apArg);` |
|     1 |  8792 | `}` |
|    10 |  8793 | `static int vm_builtin_SplObjectStorage_offsetExists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8794 | `{` |
|    11 |  8795 | `	return SosContainsMethod(pCtx,"offsetExists",nArg,apArg);` |
|     1 |  8796 | `}` |
|     - |  8797 | ``/* php's offsetGet: the info, or `Object not found` -- NOT null, and not false. */`` |
|    28 |  8798 | `static int vm_builtin_SplObjectStorage_offsetGet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8799 | `{` |
|    29 |  8800 | `	ph7_vm *pVm = pCtx->pVm;` |
|    29 |  8801 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  8802 | `	ph7_hashmap *pMap;` |
|    29 |  8803 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  8804 | `	ph7_value sObj,sKey,*pObj,*pInf;` |
|    29 |  8805 | `	sxi32 rc = SosObjectArg(pCtx,"offsetGet",nArg,apArg,&pObj);` |
|    29 |  8806 | `	if( rc != PH7_OK ){` |
|     5 |  8807 | `		return rc;` |
|     - |  8808 | `	}` |
|    25 |  8809 | `	PH7_MemObjInit(pVm,&sObj);` |
|    25 |  8810 | `	PH7_MemObjInit(pVm,&sKey);` |
|    25 |  8811 | `	PH7_MemObjStore(pObj,&sObj);` |
|    25 |  8812 | `	rc = SosKey(pCtx,pThis,&sObj,&sKey);` |
|    25 |  8813 | `	if( rc == PH7_OK ){` |
|    25 |  8814 | `		pMap = SosMap(pVm,pThis);` |
|    25 |  8815 | `		if( pMap == 0 \|\| PH7_HashmapLookup(pMap,&sKey,&pNode) != SXRET_OK ){` |
|     5 |  8816 | `			rc = PH7_VmThrowException(pCtx,"UnexpectedValueException","Object not found");` |
|     3 |  8817 | `		}else{` |
|    21 |  8818 | `			pInf = SosPart(HashmapExtractNodeValue(pNode),"inf");` |
|    21 |  8819 | `			if( pInf ){` |
|    21 |  8820 | `				ph7_result_value(pCtx,pInf);` |
|    11 |  8821 | `			}else{` |
|   ! 0 |  8822 | `				ph7_result_null(pCtx);` |
|     - |  8823 | `			}` |
|     - |  8824 | `		}` |
|    12 |  8825 | `	}` |
|    25 |  8826 | `	PH7_MemObjRelease(&sObj);` |
|    25 |  8827 | `	PH7_MemObjRelease(&sKey);` |
|    25 |  8828 | `	return rc;` |
|    15 |  8829 | `}` |
|     - |  8830 | `/*` |
|     - |  8831 | ` * php's addAll: attach every pair of the other storage, then reset the INDEX only` |
|     - |  8832 | ` * -- the position is deliberately left where it stood, which is why an insert can` |
|     - |  8833 | ` * revive a walk that had run out.` |
|     - |  8834 | ` */` |
|     8 |  8835 | `static int vm_builtin_SplObjectStorage_addAll(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8836 | `{` |
|     9 |  8837 | `	ph7_vm *pVm = pCtx->pVm;` |
|     9 |  8838 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     9 |  8839 | `	ph7_class_instance *pOther = SosOther(nArg,apArg);` |
|     - |  8840 | `	ph7_hashmap *pMap;` |
|     - |  8841 | `	ph7_value sSnap;` |
|     - |  8842 | `	sxi64 i,n;` |
|     9 |  8843 | `	sxi32 rc = PH7_OK;` |
|     9 |  8844 | `	if( pOther == 0 \|\| pThis == 0 ){` |
|   ! 0 |  8845 | `		return PH7_OK;` |
|     - |  8846 | `	}` |
|     9 |  8847 | `	PH7_MemObjInit(pVm,&sSnap);` |
|     9 |  8848 | `	if( SosSnapshot(pVm,pOther,&sSnap) != SXRET_OK ){` |
|   ! 0 |  8849 | `		PH7_MemObjRelease(&sSnap);` |
|   ! 0 |  8850 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  8851 | `	}` |
|     9 |  8852 | `	n = (sxi64)((ph7_hashmap *)sSnap.x.pOther)->nEntry;` |
|    21 |  8853 | `	for( i = 0 ; i < n && rc == PH7_OK ; ++i ){` |
|    13 |  8854 | `		ph7_value *pPair = SosSnapAt(&sSnap,i);` |
|    13 |  8855 | `		ph7_value *pObj = SosPart(pPair,"obj");` |
|    13 |  8856 | `		ph7_value *pInf = SosPart(pPair,"inf");` |
|    13 |  8857 | `		if( pObj ){` |
|    13 |  8858 | `			rc = SosAttach(pCtx,pThis,pObj,pInf);` |
|     6 |  8859 | `		}` |
|     7 |  8860 | `	}` |
|     9 |  8861 | `	PH7_MemObjRelease(&sSnap);` |
|     9 |  8862 | `	if( rc != PH7_OK ){` |
|   ! 0 |  8863 | `		return rc;` |
|     - |  8864 | `	}` |
|     9 |  8865 | `	PH7_NativeSetAttrInt(pVm,pThis,SOS_I,0);` |
|     9 |  8866 | `	pMap = SosMap(pVm,pThis);` |
|     9 |  8867 | `	ph7_result_int64(pCtx,pMap ? (ph7_int64)pMap->nEntry : 0);` |
|     9 |  8868 | `	return PH7_OK;` |
|     5 |  8869 | `}` |
|     - |  8870 | `/* php's removeAll: detach everything the other storage holds, then RESTART the walk. */` |
|     2 |  8871 | `static int vm_builtin_SplObjectStorage_removeAll(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8872 | `{` |
|     3 |  8873 | `	ph7_vm *pVm = pCtx->pVm;` |
|     3 |  8874 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     3 |  8875 | `	ph7_class_instance *pOther = SosOther(nArg,apArg);` |
|     - |  8876 | `	ph7_hashmap *pMap;` |
|     - |  8877 | `	ph7_value sSnap;` |
|     - |  8878 | `	sxi64 i,n;` |
|     3 |  8879 | `	sxi32 rc = PH7_OK;` |
|     3 |  8880 | `	if( pOther == 0 \|\| pThis == 0 ){` |
|   ! 0 |  8881 | `		return PH7_OK;` |
|     - |  8882 | `	}` |
|     3 |  8883 | `	PH7_MemObjInit(pVm,&sSnap);` |
|     3 |  8884 | `	if( SosSnapshot(pVm,pOther,&sSnap) != SXRET_OK ){` |
|   ! 0 |  8885 | `		PH7_MemObjRelease(&sSnap);` |
|   ! 0 |  8886 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  8887 | `	}` |
|     3 |  8888 | `	n = (sxi64)((ph7_hashmap *)sSnap.x.pOther)->nEntry;` |
|     9 |  8889 | `	for( i = 0 ; i < n && rc == PH7_OK ; ++i ){` |
|     7 |  8890 | `		ph7_value *pObj = SosPart(SosSnapAt(&sSnap,i),"obj");` |
|     7 |  8891 | `		if( pObj ){` |
|     7 |  8892 | `			rc = SosDetach(pCtx,pThis,pObj,0);` |
|     3 |  8893 | `		}` |
|     4 |  8894 | `	}` |
|     3 |  8895 | `	PH7_MemObjRelease(&sSnap);` |
|     3 |  8896 | `	if( rc != PH7_OK ){` |
|   ! 0 |  8897 | `		return rc;` |
|     - |  8898 | `	}` |
|     3 |  8899 | `	SosRewind(pVm,pThis);` |
|     3 |  8900 | `	pMap = SosMap(pVm,pThis);` |
|     3 |  8901 | `	ph7_result_int64(pCtx,pMap ? (ph7_int64)pMap->nEntry : 0);` |
|     3 |  8902 | `	return PH7_OK;` |
|     2 |  8903 | `}` |
|     - |  8904 | `/* php's removeAllExcept: the INTERSECTION, walked over this storage's own pairs. */` |
|     2 |  8905 | `static int vm_builtin_SplObjectStorage_removeAllExcept(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8906 | `{` |
|     3 |  8907 | `	ph7_vm *pVm = pCtx->pVm;` |
|     3 |  8908 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     3 |  8909 | `	ph7_class_instance *pOther = SosOther(nArg,apArg);` |
|     - |  8910 | `	ph7_hashmap *pMap;` |
|     - |  8911 | `	ph7_value sSnap;` |
|     - |  8912 | `	sxi64 i,n;` |
|     3 |  8913 | `	sxi32 rc = PH7_OK;` |
|     3 |  8914 | `	if( pOther == 0 \|\| pThis == 0 ){` |
|   ! 0 |  8915 | `		return PH7_OK;` |
|     - |  8916 | `	}` |
|     3 |  8917 | `	PH7_MemObjInit(pVm,&sSnap);` |
|     3 |  8918 | `	if( SosSnapshot(pVm,pThis,&sSnap) != SXRET_OK ){` |
|   ! 0 |  8919 | `		PH7_MemObjRelease(&sSnap);` |
|   ! 0 |  8920 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  8921 | `	}` |
|     3 |  8922 | `	n = (sxi64)((ph7_hashmap *)sSnap.x.pOther)->nEntry;` |
|     9 |  8923 | `	for( i = 0 ; i < n && rc == PH7_OK ; ++i ){` |
|     7 |  8924 | `		ph7_value *pObj = SosPart(SosSnapAt(&sSnap,i),"obj");` |
|     7 |  8925 | `		int bFound = 0;` |
|     7 |  8926 | `		if( pObj == 0 ){` |
|   ! 0 |  8927 | `			continue;` |
|     - |  8928 | `		}` |
|     7 |  8929 | `		rc = SosContains(pCtx,pOther,pObj,&bFound);` |
|     7 |  8930 | `		if( rc == PH7_OK && !bFound ){` |
|     5 |  8931 | `			pObj = SosPart(SosSnapAt(&sSnap,i),"obj");   /* re-resolved: getHash may have run */` |
|     5 |  8932 | `			if( pObj ){` |
|     5 |  8933 | `				rc = SosDetach(pCtx,pThis,pObj,0);` |
|     2 |  8934 | `			}` |
|     2 |  8935 | `		}` |
|     4 |  8936 | `	}` |
|     3 |  8937 | `	PH7_MemObjRelease(&sSnap);` |
|     3 |  8938 | `	if( rc != PH7_OK ){` |
|   ! 0 |  8939 | `		return rc;` |
|     - |  8940 | `	}` |
|     3 |  8941 | `	SosRewind(pVm,pThis);` |
|     3 |  8942 | `	pMap = SosMap(pVm,pThis);` |
|     3 |  8943 | `	ph7_result_int64(pCtx,pMap ? (ph7_int64)pMap->nEntry : 0);` |
|     3 |  8944 | `	return PH7_OK;` |
|     2 |  8945 | `}` |
|     - |  8946 | `/*` |
|     - |  8947 | ` * php's count(): COUNT_RECURSIVE is accepted and changes nothing -- the storage` |
|     - |  8948 | ` * holds C structs rather than zvals there, so nothing recurses.` |
|     - |  8949 | ` */` |
|    24 |  8950 | `static int vm_builtin_SplObjectStorage_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8951 | `{` |
|    25 |  8952 | `	ph7_hashmap *pMap = SosMap(pCtx->pVm,PH7_ContextThis(pCtx));` |
|    12 |  8953 | `	SXUNUSED(nArg);` |
|    12 |  8954 | `	SXUNUSED(apArg);` |
|    25 |  8955 | `	ph7_result_int64(pCtx,pMap ? (ph7_int64)pMap->nEntry : 0);` |
|    25 |  8956 | `	return PH7_OK;` |
|     1 |  8957 | `}` |
|     4 |  8958 | `static int vm_builtin_SplObjectStorage_getHash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8959 | `{` |
|     - |  8960 | `	ph7_value *pObj;` |
|     5 |  8961 | `	sxi32 rc = SosObjectArg(pCtx,"getHash",nArg,apArg,&pObj);` |
|     5 |  8962 | `	if( rc != PH7_OK ){` |
|   ! 0 |  8963 | `		return rc;` |
|     - |  8964 | `	}` |
|     - |  8965 | `	/* php's getHash() IS php_spl_object_hash(), the same one the function answers. */` |
|     5 |  8966 | `	return vm_builtin_spl_object_hash(pCtx,nArg,apArg);` |
|     3 |  8967 | `}` |
|    28 |  8968 | `static int vm_builtin_SplObjectStorage_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8969 | `{` |
|    14 |  8970 | `	SXUNUSED(nArg);` |
|    14 |  8971 | `	SXUNUSED(apArg);` |
|    29 |  8972 | `	SosRewind(pCtx->pVm,PH7_ContextThis(pCtx));` |
|    29 |  8973 | `	return PH7_OK;` |
|     1 |  8974 | `}` |
|    52 |  8975 | `static int vm_builtin_SplObjectStorage_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8976 | `{` |
|    53 |  8977 | `	ph7_hashmap *pMap = SosMap(pCtx->pVm,PH7_ContextThis(pCtx));` |
|    26 |  8978 | `	SXUNUSED(nArg);` |
|    26 |  8979 | `	SXUNUSED(apArg);` |
|    53 |  8980 | `	ph7_result_bool(pCtx,pMap && pMap->pCur ? 1 : 0);` |
|    53 |  8981 | `	return PH7_OK;` |
|     1 |  8982 | `}` |
|     - |  8983 | `/* php's key() is the INDEX, a counter of its own: next() advances it past the end` |
|     - |  8984 | ` * too, and only rewind()/detach()/addAll() and friends put it back to zero. */` |
|    40 |  8985 | `static int vm_builtin_SplObjectStorage_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8986 | `{` |
|    20 |  8987 | `	SXUNUSED(nArg);` |
|    20 |  8988 | `	SXUNUSED(apArg);` |
|    41 |  8989 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(PH7_ContextThis(pCtx),SOS_I));` |
|    41 |  8990 | `	return PH7_OK;` |
|     1 |  8991 | `}` |
|     - |  8992 | `/* php RAISES here rather than answering null: the chunk's null was a wrong answer` |
|     - |  8993 | `` * every `foreach` hid, because a foreach never asks past valid(). */`` |
|    46 |  8994 | `static int vm_builtin_SplObjectStorage_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8995 | `{` |
|    47 |  8996 | `	ph7_value *pObj = SosPart(SosCurrentPair(pCtx->pVm,PH7_ContextThis(pCtx)),"obj");` |
|    23 |  8997 | `	SXUNUSED(nArg);` |
|    23 |  8998 | `	SXUNUSED(apArg);` |
|    47 |  8999 | `	if( pObj == 0 ){` |
|     3 |  9000 | `		return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     - |  9001 | `			"Called current() on invalid iterator");` |
|     - |  9002 | `	}` |
|    45 |  9003 | `	ph7_result_value(pCtx,pObj);` |
|    45 |  9004 | `	return PH7_OK;` |
|    24 |  9005 | `}` |
|    44 |  9006 | `static int vm_builtin_SplObjectStorage_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9007 | `{` |
|    45 |  9008 | `	ph7_vm *pVm = pCtx->pVm;` |
|    45 |  9009 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    45 |  9010 | `	ph7_hashmap *pMap = SosMap(pVm,pThis);` |
|    22 |  9011 | `	SXUNUSED(nArg);` |
|    22 |  9012 | `	SXUNUSED(apArg);` |
|    45 |  9013 | `	if( pMap && pMap->pCur ){` |
|    45 |  9014 | `		pMap->pCur = pMap->pCur->pPrev;` |
|    22 |  9015 | `	}` |
|    45 |  9016 | `	PH7_NativeSetAttrInt(pVm,pThis,SOS_I,PH7_NativeAttrInt(pThis,SOS_I) + 1);` |
|    45 |  9017 | `	return PH7_OK;` |
|     1 |  9018 | `}` |
|     - |  9019 | `/* php's getInfo(): null past the end, where current() raises. */` |
|    38 |  9020 | `static int vm_builtin_SplObjectStorage_getInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9021 | `{` |
|    39 |  9022 | `	ph7_value *pInf = SosPart(SosCurrentPair(pCtx->pVm,PH7_ContextThis(pCtx)),"inf");` |
|    19 |  9023 | `	SXUNUSED(nArg);` |
|    19 |  9024 | `	SXUNUSED(apArg);` |
|    39 |  9025 | `	if( pInf ){` |
|    37 |  9026 | `		ph7_result_value(pCtx,pInf);` |
|    19 |  9027 | `	}else{` |
|     3 |  9028 | `		ph7_result_null(pCtx);` |
|     - |  9029 | `	}` |
|    39 |  9030 | `	return PH7_OK;` |
|     1 |  9031 | `}` |
|     2 |  9032 | `static int vm_builtin_SplObjectStorage_setInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9033 | `{` |
|     3 |  9034 | `	ph7_vm *pVm = pCtx->pVm;` |
|     3 |  9035 | `	ph7_hashmap *pMap = SosMap(pVm,PH7_ContextThis(pCtx));` |
|     - |  9036 | `	ph7_value *pPair;` |
|     3 |  9037 | `	if( pMap == 0 \|\| pMap->pCur == 0 \|\| nArg < 1 ){` |
|   ! 0 |  9038 | `		return PH7_OK;   /* php returns without touching anything */` |
|     - |  9039 | `	}` |
|     3 |  9040 | `	pPair = SosPairForWrite(pVm,pMap->pCur);` |
|     3 |  9041 | `	if( pPair ){` |
|     3 |  9042 | `		SosSetPart(pVm,pPair,"inf",apArg[0]);` |
|     1 |  9043 | `	}` |
|     3 |  9044 | `	return PH7_OK;` |
|     2 |  9045 | `}` |
|     - |  9046 | `/*` |
|     - |  9047 | ` * php's seek(): a position outside the storage is an OutOfBoundsException, and` |
|     - |  9048 | ` * the index follows the position exactly (php walks its hash cursor either way` |
|     - |  9049 | ` * and counts; the destination is the same).` |
|     - |  9050 | ` */` |
|     4 |  9051 | `static int vm_builtin_SplObjectStorage_seek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9052 | `{` |
|     5 |  9053 | `	ph7_vm *pVm = pCtx->pVm;` |
|     5 |  9054 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     5 |  9055 | `	ph7_hashmap *pMap = SosMap(pVm,pThis);` |
|     5 |  9056 | `	ph7_int64 iPos = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|     - |  9057 | `	ph7_int64 i;` |
|     5 |  9058 | `	if( pMap == 0 ){` |
|   ! 0 |  9059 | `		return PH7_OK;` |
|     - |  9060 | `	}` |
|     5 |  9061 | `	if( iPos < 0 \|\| iPos >= (ph7_int64)pMap->nEntry ){` |
|     4 |  9062 | `		return PH7_VmThrowException(pCtx,"OutOfBoundsException",` |
|     1 |  9063 | `			"Seek position %qd is out of range",iPos);` |
|     - |  9064 | `	}` |
|     3 |  9065 | `	pMap->pCur = pMap->pFirst;` |
|     7 |  9066 | `	for( i = 0 ; i < iPos && pMap->pCur ; ++i ){` |
|     5 |  9067 | `		pMap->pCur = pMap->pCur->pPrev;` |
|     3 |  9068 | `	}` |
|     3 |  9069 | `	PH7_NativeSetAttrInt(pVm,pThis,SOS_I,iPos);` |
|     3 |  9070 | `	return PH7_OK;` |
|     3 |  9071 | `}` |
|     - |  9072 | `/*` |
|     - |  9073 | ` * php's get_debug_info: ONE entry, the storage, under its own MANGLED private key` |
|     - |  9074 | `` * -- `["storage":"SplObjectStorage":private]` on screen. The pairs are re-indexed`` |
|     - |  9075 | ` * from zero and shown as {obj, inf}, which is the shape stored here already.` |
|     - |  9076 | `` * `__debugInfo()` is the same array, reachable by name because php declares it.`` |
|     - |  9077 | ` */` |
|     8 |  9078 | `static sxi32 SosFillDebug(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)` |
|     1 |  9079 | `{` |
|     9 |  9080 | `	ph7_hashmap *pMap = SosMap(pVm,pThis);` |
|     - |  9081 | `	ph7_hashmap_node *pNode;` |
|     - |  9082 | `	ph7_value sKey,sList;` |
|     - |  9083 | `	sxu32 n;` |
|     9 |  9084 | `	PH7_MemObjInit(pVm,&sList);` |
|     9 |  9085 | `	if( PH7_MemObjToHashmap(&sList) != SXRET_OK ){` |
|   ! 0 |  9086 | `		PH7_MemObjRelease(&sList);` |
|   ! 0 |  9087 | `		return SXERR_MEM;` |
|     - |  9088 | `	}` |
|     9 |  9089 | `	if( pMap ){` |
|     9 |  9090 | `		pNode = pMap->pFirst;` |
|    15 |  9091 | `		for( n = 0 ; n < pMap->nEntry && pNode ; ++n ){` |
|     7 |  9092 | `			ph7_value *pPair = HashmapExtractNodeValue(pNode);` |
|     7 |  9093 | `			if( pPair ){` |
|     7 |  9094 | `				ph7_array_add_elem(&sList,0,pPair);` |
|     3 |  9095 | `			}` |
|     7 |  9096 | `			pNode = pNode->pPrev;` |
|     4 |  9097 | `		}` |
|     4 |  9098 | `	}` |
|     9 |  9099 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|     9 |  9100 | `	PH7_MemObjStringAppend(&sKey,"\0SplObjectStorage\0storage",` |
|     - |  9101 | `		sizeof("\0SplObjectStorage\0storage")-1);` |
|     9 |  9102 | `	ph7_array_add_elem(pOut,&sKey,&sList);` |
|     9 |  9103 | `	PH7_MemObjRelease(&sKey);` |
|     9 |  9104 | `	PH7_MemObjRelease(&sList);` |
|     9 |  9105 | `	return PH7_OK;` |
|     5 |  9106 | `}` |
|     8 |  9107 | `static sxi32 SosPresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|     1 |  9108 | `{` |
|     9 |  9109 | `	if( !bDebug ){` |
|     5 |  9110 | `		return PH7_OK;   /* php's (array) cast and var_export show nothing */` |
|     - |  9111 | `	}` |
|     5 |  9112 | `	return SosFillDebug(pVm,pThis,pOut);` |
|     5 |  9113 | `}` |
|     4 |  9114 | `static int vm_builtin_SplObjectStorage_debugInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9115 | `{` |
|     5 |  9116 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  9117 | `	ph7_value sOut;` |
|     2 |  9118 | `	SXUNUSED(nArg);` |
|     2 |  9119 | `	SXUNUSED(apArg);` |
|     5 |  9120 | `	PH7_MemObjInit(pVm,&sOut);` |
|     5 |  9121 | `	if( PH7_MemObjToHashmap(&sOut) != SXRET_OK ){` |
|   ! 0 |  9122 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 |  9123 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  9124 | `	}` |
|     5 |  9125 | `	SosFillDebug(pVm,PH7_ContextThis(pCtx),&sOut);` |
|     5 |  9126 | `	ph7_result_value(pCtx,&sOut);` |
|     5 |  9127 | `	PH7_MemObjRelease(&sOut);` |
|     5 |  9128 | `	return PH7_OK;` |
|     3 |  9129 | `}` |
|     - |  9130 | `/*` |
|     - |  9131 | ` * php's __serialize(): [[obj, inf, obj, inf, …], members]. This is what serialize()` |
|     - |  9132 | ` * actually uses; the Serializable pair below is the legacy format nothing else in` |
|     - |  9133 | ` * php reads or writes. The members slot is the instance's own properties — empty for` |
|     - |  9134 | ` * a bare SplObjectStorage, a SUBCLASS's declared slots when there is one.` |
|     - |  9135 | ` */` |
|    12 |  9136 | `static int vm_builtin_SplObjectStorage_serializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9137 | `{` |
|    13 |  9138 | `	ph7_vm *pVm = pCtx->pVm;` |
|    13 |  9139 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    13 |  9140 | `	ph7_hashmap *pMap = SosMap(pVm,pThis);` |
|     - |  9141 | `	ph7_hashmap_node *pNode;` |
|     - |  9142 | `	ph7_value sOut,sFlat,sMembers;` |
|     - |  9143 | `	sxu32 n;` |
|     6 |  9144 | `	SXUNUSED(nArg);` |
|     6 |  9145 | `	SXUNUSED(apArg);` |
|    13 |  9146 | `	PH7_MemObjInit(pVm,&sOut);` |
|    13 |  9147 | `	PH7_MemObjInit(pVm,&sFlat);` |
|    12 |  9148 | `	if( SplMembersOf(pVm,pThis,&sMembers) != SXRET_OK` |
|    12 |  9149 | `	 \|\| PH7_MemObjToHashmap(&sOut) != SXRET_OK` |
|    13 |  9150 | `	 \|\| PH7_MemObjToHashmap(&sFlat) != SXRET_OK ){` |
|   ! 0 |  9151 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 |  9152 | `		PH7_MemObjRelease(&sFlat);` |
|   ! 0 |  9153 | `		PH7_MemObjRelease(&sMembers);` |
|   ! 0 |  9154 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  9155 | `	}` |
|    13 |  9156 | `	if( pMap ){` |
|    13 |  9157 | `		pNode = pMap->pFirst;` |
|    29 |  9158 | `		for( n = 0 ; n < pMap->nEntry && pNode ; ++n ){` |
|    17 |  9159 | `			ph7_value *pPair = HashmapExtractNodeValue(pNode);` |
|    17 |  9160 | `			ph7_value *pObj = SosPart(pPair,"obj");` |
|    17 |  9161 | `			ph7_value *pInf = SosPart(pPair,"inf");` |
|    17 |  9162 | `			if( pObj ){` |
|    17 |  9163 | `				ph7_array_add_elem(&sFlat,0,pObj);` |
|    17 |  9164 | `				if( pInf ){` |
|    17 |  9165 | `					ph7_array_add_elem(&sFlat,0,pInf);` |
|     8 |  9166 | `				}` |
|     8 |  9167 | `			}` |
|    17 |  9168 | `			pNode = pNode->pPrev;` |
|     9 |  9169 | `		}` |
|     6 |  9170 | `	}` |
|    13 |  9171 | `	ph7_array_add_elem(&sOut,0,&sFlat);` |
|    13 |  9172 | `	ph7_array_add_elem(&sOut,0,&sMembers);` |
|    13 |  9173 | `	ph7_result_value(pCtx,&sOut);` |
|    13 |  9174 | `	PH7_MemObjRelease(&sOut);` |
|    13 |  9175 | `	PH7_MemObjRelease(&sFlat);` |
|    13 |  9176 | `	PH7_MemObjRelease(&sMembers);` |
|    13 |  9177 | `	return PH7_OK;` |
|     7 |  9178 | `}` |
|     2 |  9179 | `static sxi32 SosIllTyped(ph7_context *pCtx)` |
|     1 |  9180 | `{` |
|     3 |  9181 | `	return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - |  9182 | `		"Incomplete or ill-typed serialization data");` |
|     1 |  9183 | `}` |
|    12 |  9184 | `static int vm_builtin_SplObjectStorage_unserializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9185 | `{` |
|    13 |  9186 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  9187 | `	ph7_hashmap *pFlat;` |
|    13 |  9188 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  9189 | `	ph7_value *pStorage,*pMembers;` |
|     - |  9190 | `	sxi64 i,n;` |
|    13 |  9191 | `	sxi32 rc = PH7_OK;` |
|    13 |  9192 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 \|\| pThis == 0 ){` |
|   ! 0 |  9193 | `		return SosIllTyped(pCtx);` |
|     - |  9194 | `	}` |
|    13 |  9195 | `	if( HashmapLookupIntKey((ph7_hashmap *)apArg[0]->x.pOther,0,&pNode) != SXRET_OK ){` |
|   ! 0 |  9196 | `		return SosIllTyped(pCtx);` |
|     - |  9197 | `	}` |
|    13 |  9198 | `	pStorage = HashmapExtractNodeValue(pNode);` |
|    13 |  9199 | `	pNode = 0;` |
|    13 |  9200 | `	if( HashmapLookupIntKey((ph7_hashmap *)apArg[0]->x.pOther,1,&pNode) != SXRET_OK ){` |
|   ! 0 |  9201 | `		return SosIllTyped(pCtx);` |
|     - |  9202 | `	}` |
|    13 |  9203 | `	pMembers = HashmapExtractNodeValue(pNode);` |
|    12 |  9204 | `	if( pStorage == 0 \|\| (pStorage->iFlags & MEMOBJ_HASHMAP) == 0` |
|    12 |  9205 | `	 \|\| pMembers == 0 \|\| (pMembers->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     3 |  9206 | `		return SosIllTyped(pCtx);` |
|     - |  9207 | `	}` |
|    11 |  9208 | `	pFlat = (ph7_hashmap *)pStorage->x.pOther;` |
|    11 |  9209 | `	n = (sxi64)pFlat->nEntry;` |
|    11 |  9210 | `	if( n % 2 != 0 ){` |
|     3 |  9211 | `		return PH7_VmThrowException(pCtx,"UnexpectedValueException","Odd number of elements");` |
|     - |  9212 | `	}` |
|    19 |  9213 | `	for( i = 0 ; i < n && rc == PH7_OK ; i += 2 ){` |
|     - |  9214 | `		ph7_value *pObj,*pInf;` |
|    13 |  9215 | `		pNode = 0;` |
|    13 |  9216 | `		if( HashmapLookupIntKey(pFlat,i,&pNode) != SXRET_OK ){` |
|   ! 0 |  9217 | `			return SosIllTyped(pCtx);` |
|     - |  9218 | `		}` |
|    13 |  9219 | `		pObj = HashmapExtractNodeValue(pNode);` |
|    13 |  9220 | `		if( pObj == 0 \|\| (pObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     3 |  9221 | `			return PH7_VmThrowException(pCtx,"UnexpectedValueException","Non-object key");` |
|     - |  9222 | `		}` |
|    11 |  9223 | `		pNode = 0;` |
|    11 |  9224 | `		pInf = HashmapLookupIntKey(pFlat,i+1,&pNode) == SXRET_OK` |
|    10 |  9225 | `			? HashmapExtractNodeValue(pNode) : 0;` |
|    11 |  9226 | `		rc = SosAttach(pCtx,pThis,pObj,pInf);` |
|     6 |  9227 | `	}` |
|     7 |  9228 | `	if( rc == PH7_OK ){` |
|     7 |  9229 | `		SplMembersLoad(pThis,pMembers);` |
|     3 |  9230 | `	}` |
|     7 |  9231 | `	return rc;` |
|     7 |  9232 | `}` |
|     - |  9233 | `/*` |
|     - |  9234 | ` * php's Serializable pair, kept because the interface is still declared. The` |
|     - |  9235 | `` * format is `x:` + the serialized COUNT, then one `<obj>,<inf>;` per element, then`` |
|     - |  9236 | `` * `m:` + the serialized members -- and php writes it through ONE serializer state,`` |
|     - |  9237 | `` * so an object that appears twice becomes an `r:` back-reference there and a`` |
|     - |  9238 | ` * second copy here (§10; the DLL's legacy pair has the same shape).` |
|     - |  9239 | ` */` |
|     4 |  9240 | `static int vm_builtin_SplObjectStorage_serialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9241 | `{` |
|     5 |  9242 | `	ph7_vm *pVm = pCtx->pVm;` |
|     5 |  9243 | `	ph7_hashmap *pMap = SosMap(pVm,PH7_ContextThis(pCtx));` |
|     - |  9244 | `	ph7_hashmap_node *pNode;` |
|     - |  9245 | `	SyBlob sOut;` |
|     - |  9246 | `	ph7_value sVal,*apCall[1];` |
|     - |  9247 | `	sxu32 n;` |
|     2 |  9248 | `	SXUNUSED(nArg);` |
|     2 |  9249 | `	SXUNUSED(apArg);` |
|     5 |  9250 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|     5 |  9251 | `	SyBlobAppend(&sOut,"x:",sizeof("x:")-1);` |
|     5 |  9252 | `	PH7_MemObjInitFromInt(pVm,&sVal,pMap ? (sxi64)pMap->nEntry : 0);` |
|     5 |  9253 | `	apCall[0] = &sVal;` |
|     5 |  9254 | `	SplSerializeInto(pCtx,apCall,&sOut);` |
|     5 |  9255 | `	PH7_MemObjRelease(&sVal);` |
|     5 |  9256 | `	if( pMap ){` |
|     5 |  9257 | `		pNode = pMap->pFirst;` |
|    13 |  9258 | `		for( n = 0 ; n < pMap->nEntry && pNode ; ++n ){` |
|     9 |  9259 | `			ph7_value *pPair = HashmapExtractNodeValue(pNode);` |
|     9 |  9260 | `			ph7_value *pObj = SosPart(pPair,"obj");` |
|     9 |  9261 | `			ph7_value *pInf = SosPart(pPair,"inf");` |
|     - |  9262 | `			ph7_value sNull;` |
|     9 |  9263 | `			if( pObj ){` |
|     9 |  9264 | `				apCall[0] = pObj;` |
|     9 |  9265 | `				SplSerializeInto(pCtx,apCall,&sOut);` |
|     9 |  9266 | `				SyBlobAppend(&sOut,",",1);` |
|     9 |  9267 | `				PH7_MemObjInit(pVm,&sNull);` |
|     9 |  9268 | `				apCall[0] = pInf ? pInf : &sNull;` |
|     9 |  9269 | `				SplSerializeInto(pCtx,apCall,&sOut);` |
|     9 |  9270 | `				PH7_MemObjRelease(&sNull);` |
|     9 |  9271 | `				SyBlobAppend(&sOut,";",1);` |
|     4 |  9272 | `			}` |
|     9 |  9273 | `			pNode = pNode->pPrev;` |
|     5 |  9274 | `		}` |
|     2 |  9275 | `	}` |
|     5 |  9276 | `	SyBlobAppend(&sOut,"m:",sizeof("m:")-1);` |
|     5 |  9277 | `	PH7_MemObjInit(pVm,&sVal);` |
|     5 |  9278 | `	if( PH7_MemObjToHashmap(&sVal) == SXRET_OK ){` |
|     5 |  9279 | `		apCall[0] = &sVal;` |
|     5 |  9280 | `		SplSerializeInto(pCtx,apCall,&sOut);` |
|     2 |  9281 | `	}` |
|     5 |  9282 | `	PH7_MemObjRelease(&sVal);` |
|     - |  9283 | `	/* ph7_result_string APPENDS too, and pRet still holds the last nested answer` |
|     - |  9284 | `	 * (rule 54) -- drop it before writing this one. */` |
|     5 |  9285 | `	if( pCtx->pRet ){` |
|     5 |  9286 | `		PH7_MemObjRelease(pCtx->pRet);` |
|     2 |  9287 | `	}` |
|     5 |  9288 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     5 |  9289 | `	SyBlobRelease(&sOut);` |
|     5 |  9290 | `	return PH7_OK;` |
|     1 |  9291 | `}` |
|     - |  9292 | `/* php reports WHERE its parse gave up, in bytes, and every failure below is that` |
|     - |  9293 | ` * one exception. */` |
|     2 |  9294 | `static sxi32 SosOffsetErr(ph7_context *pCtx,int nAt,int nTotal)` |
|     1 |  9295 | `{` |
|     4 |  9296 | `	return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     1 |  9297 | `		"Error at offset %d of %d bytes",nAt,nTotal);` |
|     1 |  9298 | `}` |
|     6 |  9299 | `static int vm_builtin_SplObjectStorage_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9300 | `{` |
|     7 |  9301 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 |  9302 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  9303 | `	const char *zData;` |
|     7 |  9304 | `	int nData = 0,nAt = 0,nRead = 0;` |
|     - |  9305 | `	ph7_value sVal;` |
|     - |  9306 | `	sxi64 nCount,i;` |
|     - |  9307 | `	sxi32 rc;` |
|     7 |  9308 | `	if( nArg < 1 \|\| pThis == 0 ){` |
|   ! 0 |  9309 | `		return PH7_OK;` |
|     - |  9310 | `	}` |
|     7 |  9311 | `	zData = ph7_value_to_string(apArg[0],&nData);` |
|     7 |  9312 | `	if( nData < 1 ){` |
|     3 |  9313 | `		return PH7_OK;   /* php returns without touching the storage */` |
|     - |  9314 | `	}` |
|     5 |  9315 | `	if( nData < 2 \|\| zData[0] != 'x' \|\| zData[1] != ':' ){` |
|   ! 0 |  9316 | `		return SosOffsetErr(pCtx,zData[0] == 'x' ? 1 : 0,nData);` |
|     - |  9317 | `	}` |
|     5 |  9318 | `	nAt = 2;` |
|     5 |  9319 | `	PH7_MemObjInit(pVm,&sVal);` |
|     5 |  9320 | `	rc = PH7_VmUnserializeOne(pCtx,&zData[nAt],nData - nAt,&nRead,&sVal);` |
|     5 |  9321 | `	if( rc == PH7_EXCEPTION ){` |
|   ! 0 |  9322 | `		PH7_MemObjRelease(&sVal);` |
|   ! 0 |  9323 | `		return rc;` |
|     - |  9324 | `	}` |
|     5 |  9325 | `	if( rc != SXRET_OK \|\| (sVal.iFlags & MEMOBJ_INT) == 0 ){` |
|     - |  9326 | `		/* php reports where its parser STOPPED, which for a well-formed value of` |
|     - |  9327 | `		 * the wrong type is the byte after it. */` |
|     3 |  9328 | `		PH7_MemObjRelease(&sVal);` |
|     3 |  9329 | `		return SosOffsetErr(pCtx,nAt + nRead,nData);` |
|     - |  9330 | `	}` |
|     3 |  9331 | `	nCount = ph7_value_to_int64(&sVal);` |
|     3 |  9332 | `	PH7_MemObjRelease(&sVal);` |
|     3 |  9333 | `	nAt += nRead - 1;   /* php steps back onto the ';' that ends the count */` |
|     3 |  9334 | `	if( nCount < 0 ){` |
|   ! 0 |  9335 | `		return SosOffsetErr(pCtx,nAt,nData);` |
|     - |  9336 | `	}` |
|     9 |  9337 | `	for( i = 0 ; i < nCount ; ++i ){` |
|     - |  9338 | `		ph7_value sObj,sInf;` |
|     7 |  9339 | `		if( nAt >= nData \|\| zData[nAt] != ';' ){` |
|   ! 0 |  9340 | `			return SosOffsetErr(pCtx,nAt,nData);` |
|     - |  9341 | `		}` |
|     7 |  9342 | `		nAt++;` |
|     7 |  9343 | `		if( nAt >= nData \|\| (zData[nAt] != 'O' && zData[nAt] != 'C' && zData[nAt] != 'r') ){` |
|   ! 0 |  9344 | `			return SosOffsetErr(pCtx,nAt,nData);` |
|     - |  9345 | `		}` |
|     7 |  9346 | `		PH7_MemObjInit(pVm,&sObj);` |
|     7 |  9347 | `		PH7_MemObjInit(pVm,&sInf);` |
|     7 |  9348 | `		rc = PH7_VmUnserializeOne(pCtx,&zData[nAt],nData - nAt,&nRead,&sObj);` |
|     7 |  9349 | `		if( rc == PH7_EXCEPTION ){` |
|   ! 0 |  9350 | `			PH7_MemObjRelease(&sObj);` |
|   ! 0 |  9351 | `			PH7_MemObjRelease(&sInf);` |
|   ! 0 |  9352 | `			return rc;` |
|     - |  9353 | `		}` |
|     7 |  9354 | `		if( rc != SXRET_OK \|\| (sObj.iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 |  9355 | `			PH7_MemObjRelease(&sObj);` |
|   ! 0 |  9356 | `			PH7_MemObjRelease(&sInf);` |
|   ! 0 |  9357 | `			return SosOffsetErr(pCtx,nAt + nRead,nData);` |
|     - |  9358 | `		}` |
|     7 |  9359 | `		nAt += nRead;` |
|     7 |  9360 | `		if( nAt < nData && zData[nAt] == ',' ){` |
|     7 |  9361 | `			nAt++;` |
|     7 |  9362 | `			rc = PH7_VmUnserializeOne(pCtx,&zData[nAt],nData - nAt,&nRead,&sInf);` |
|     7 |  9363 | `			if( rc == PH7_EXCEPTION ){` |
|   ! 0 |  9364 | `				PH7_MemObjRelease(&sObj);` |
|   ! 0 |  9365 | `				PH7_MemObjRelease(&sInf);` |
|   ! 0 |  9366 | `				return rc;` |
|     - |  9367 | `			}` |
|     7 |  9368 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  9369 | `				PH7_MemObjRelease(&sObj);` |
|   ! 0 |  9370 | `				PH7_MemObjRelease(&sInf);` |
|   ! 0 |  9371 | `				return SosOffsetErr(pCtx,nAt + nRead,nData);` |
|     - |  9372 | `			}` |
|     7 |  9373 | `			nAt += nRead;` |
|     3 |  9374 | `		}` |
|     7 |  9375 | `		rc = SosAttach(pCtx,pThis,&sObj,&sInf);` |
|     7 |  9376 | `		PH7_MemObjRelease(&sObj);` |
|     7 |  9377 | `		PH7_MemObjRelease(&sInf);` |
|     7 |  9378 | `		if( rc != PH7_OK ){` |
|   ! 0 |  9379 | `			return rc;` |
|     - |  9380 | `		}` |
|     4 |  9381 | `	}` |
|     3 |  9382 | `	if( nAt >= nData \|\| zData[nAt] != ';' ){` |
|   ! 0 |  9383 | `		return SosOffsetErr(pCtx,nAt,nData);` |
|     - |  9384 | `	}` |
|     3 |  9385 | `	nAt++;` |
|     3 |  9386 | `	if( nAt + 1 >= nData \|\| zData[nAt] != 'm' \|\| zData[nAt+1] != ':' ){` |
|   ! 0 |  9387 | `		return SosOffsetErr(pCtx,nAt < nData && zData[nAt] == 'm' ? nAt + 1 : nAt,nData);` |
|     - |  9388 | `	}` |
|     3 |  9389 | `	nAt += 2;` |
|     3 |  9390 | `	PH7_MemObjInit(pVm,&sVal);` |
|     3 |  9391 | `	rc = PH7_VmUnserializeOne(pCtx,&zData[nAt],nData - nAt,&nRead,&sVal);` |
|     3 |  9392 | `	if( rc == PH7_EXCEPTION ){` |
|   ! 0 |  9393 | `		PH7_MemObjRelease(&sVal);` |
|   ! 0 |  9394 | `		return rc;` |
|     - |  9395 | `	}` |
|     3 |  9396 | `	if( rc != SXRET_OK \|\| (sVal.iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 |  9397 | `		PH7_MemObjRelease(&sVal);` |
|   ! 0 |  9398 | `		return SosOffsetErr(pCtx,nAt + nRead,nData);` |
|     - |  9399 | `	}` |
|     - |  9400 | `	/* php loads the members onto the object here; a native class declares none` |
|     - |  9401 | `	 * that a payload could name and PHL has no dynamic properties to create. */` |
|     3 |  9402 | `	PH7_MemObjRelease(&sVal);` |
|     3 |  9403 | `	return PH7_OK;` |
|     4 |  9404 | `}` |
|     - |  9405 | `/*` |
|     - |  9406 | ` * The declaration. Method ORDER is spl_observer.stub.php's, the two observer` |
|     - |  9407 | ` * interfaces are methodless-but-typed contracts php declares beside it, and` |
|     - |  9408 | ` * seek() is the ONE method php does not mark tentative.` |
|     - |  9409 | ` */` |
|  6721 |  9410 | `static sxi32 VmInstallSplObjectStorage(ph7_vm *pVm)` |
|     5 |  9411 | `{` |
|     - |  9412 | `	static const PH7_NativeMethodDef aObserverMethod[] = {` |
|     - |  9413 | `		{ "update", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "SplSubject $subject", "@void", 0 },` |
|     - |  9414 | `	};` |
|     - |  9415 | `	static const PH7_NativeMethodDef aSubjectMethod[] = {` |
|     - |  9416 | `		{ "attach", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "SplObserver $observer", "@void", 0 },` |
|     - |  9417 | `		{ "detach", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "SplObserver $observer", "@void", 0 },` |
|     - |  9418 | `		{ "notify", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@void", 0 },` |
|     - |  9419 | `	};` |
|     - |  9420 | `	static const PH7_NativePropDef aSosProp[] = {` |
|     - |  9421 | `		{ SOS_S, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  9422 | `		{ SOS_I, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  9423 | `	};` |
|     - |  9424 | `	static const PH7_NativeMethodDef aSosMethod[] = {` |
|     - |  9425 | `		{ "attach",          PH7_MOD_PUBLIC, "object $object, mixed $info = null", "@void",` |
|     - |  9426 | `		  vm_builtin_SplObjectStorage_attach },` |
|     - |  9427 | `		{ "detach",          PH7_MOD_PUBLIC, "object $object", "@void",` |
|     - |  9428 | `		  vm_builtin_SplObjectStorage_detach },` |
|     - |  9429 | `		{ "contains",        PH7_MOD_PUBLIC, "object $object", "@bool",` |
|     - |  9430 | `		  vm_builtin_SplObjectStorage_contains },` |
|     - |  9431 | `		{ "addAll",          PH7_MOD_PUBLIC, "SplObjectStorage $storage", "@int",` |
|     - |  9432 | `		  vm_builtin_SplObjectStorage_addAll },` |
|     - |  9433 | `		{ "removeAll",       PH7_MOD_PUBLIC, "SplObjectStorage $storage", "@int",` |
|     - |  9434 | `		  vm_builtin_SplObjectStorage_removeAll },` |
|     - |  9435 | `		{ "removeAllExcept", PH7_MOD_PUBLIC, "SplObjectStorage $storage", "@int",` |
|     - |  9436 | `		  vm_builtin_SplObjectStorage_removeAllExcept },` |
|     - |  9437 | `		{ "getInfo",         PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplObjectStorage_getInfo },` |
|     - |  9438 | `		{ "setInfo",         PH7_MOD_PUBLIC, "mixed $info", "@void",` |
|     - |  9439 | `		  vm_builtin_SplObjectStorage_setInfo },` |
|     - |  9440 | `		{ "count",           PH7_MOD_PUBLIC, "int $mode = COUNT_NORMAL", "@int",` |
|     - |  9441 | `		  vm_builtin_SplObjectStorage_count },` |
|     - |  9442 | `		{ "rewind",          PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplObjectStorage_rewind },` |
|     - |  9443 | `		{ "valid",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplObjectStorage_valid },` |
|     - |  9444 | `		{ "key",             PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplObjectStorage_key },` |
|     - |  9445 | `		{ "current",         PH7_MOD_PUBLIC, "", "@object", vm_builtin_SplObjectStorage_current },` |
|     - |  9446 | `		{ "next",            PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplObjectStorage_next },` |
|     - |  9447 | `		{ "seek",            PH7_MOD_PUBLIC, "int $offset", "void",` |
|     - |  9448 | `		  vm_builtin_SplObjectStorage_seek },` |
|     - |  9449 | `		{ "unserialize",     PH7_MOD_PUBLIC, "string $data", "@void",` |
|     - |  9450 | `		  vm_builtin_SplObjectStorage_unserialize },` |
|     - |  9451 | `		{ "serialize",       PH7_MOD_PUBLIC, "", "@string",` |
|     - |  9452 | `		  vm_builtin_SplObjectStorage_serialize },` |
|     - |  9453 | ``		/* php's stub leaves these four offsets UNTYPED (a `@param object` docblock`` |
|     - |  9454 | `		 * Reflection does not print) while the ZPP takes an object -- so the` |
|     - |  9455 | `		 * signature says nothing and each body words its own refusal. */` |
|     - |  9456 | `		{ "offsetExists",    PH7_MOD_PUBLIC, "$object", "@bool",` |
|     - |  9457 | `		  vm_builtin_SplObjectStorage_offsetExists },` |
|     - |  9458 | `		{ "offsetGet",       PH7_MOD_PUBLIC, "$object", "@mixed",` |
|     - |  9459 | `		  vm_builtin_SplObjectStorage_offsetGet },` |
|     - |  9460 | `		{ "offsetSet",       PH7_MOD_PUBLIC, "$object, mixed $info = null", "@void",` |
|     - |  9461 | `		  vm_builtin_SplObjectStorage_offsetSet },` |
|     - |  9462 | `		{ "offsetUnset",     PH7_MOD_PUBLIC, "$object", "@void",` |
|     - |  9463 | `		  vm_builtin_SplObjectStorage_offsetUnset },` |
|     - |  9464 | `		{ "getHash",         PH7_MOD_PUBLIC, "object $object", "@string",` |
|     - |  9465 | `		  vm_builtin_SplObjectStorage_getHash },` |
|     - |  9466 | `		{ "__serialize",     PH7_MOD_PUBLIC, "", "@array",` |
|     - |  9467 | `		  vm_builtin_SplObjectStorage_serializeMagic },` |
|     - |  9468 | `		{ "__unserialize",   PH7_MOD_PUBLIC, "array $data", "@void",` |
|     - |  9469 | `		  vm_builtin_SplObjectStorage_unserializeMagic },` |
|     - |  9470 | `		{ "__debugInfo",     PH7_MOD_PUBLIC, "", "@array",` |
|     - |  9471 | `		  vm_builtin_SplObjectStorage_debugInfo },` |
|     - |  9472 | `	};` |
|     - |  9473 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - |  9474 | `		{ "SplObserver", 0, 0, PH7_CLASS_INTERFACE,` |
|     - |  9475 | `		  aObserverMethod, SX_ARRAYSIZE(aObserverMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  9476 | `		{ "SplSubject", 0, 0, PH7_CLASS_INTERFACE,` |
|     - |  9477 | `		  aSubjectMethod, SX_ARRAYSIZE(aSubjectMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  9478 | `		{ "SplObjectStorage", 0, "Countable,SeekableIterator,Serializable,ArrayAccess", 0,` |
|     - |  9479 | `		  aSosMethod, SX_ARRAYSIZE(aSosMethod), 0, 0,` |
|     - |  9480 | `		  aSosProp, SX_ARRAYSIZE(aSosProp), 0, 0, SosPresent },` |
|     - |  9481 | `	};` |
|  6726 |  9482 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 |  9483 | `}` |
|     - |  9484 | `/*` |
|     - |  9485 | ` * ---------------------------------------------------------------------------` |
|     - |  9486 | ` * MultipleIterator: several iterators stepped in LOCKSTEP.` |
|     - |  9487 | ` *` |
|     - |  9488 | ` * php builds it on the very storage above — its C struct IS an` |
|     - |  9489 | `` * spl_SplObjectStorage, which is why `__debugInfo()` answers under`` |
|     - |  9490 | ` * SplObjectStorage's own mangled key — so this class holds the same {obj, inf}` |
|     - |  9491 | ` * table and reuses the same attach/detach/hash routines. What it adds is two` |
|     - |  9492 | ` * flags and the rule they make: MIT_NEED_ALL is valid only while EVERY` |
|     - |  9493 | ` * sub-iterator is, MIT_NEED_ANY while any one is, and an empty set is never` |
|     - |  9494 | ` * valid at all.` |
|     - |  9495 | ` *` |
|     - |  9496 | ` * current() and key() answer an ARRAY built in attach order, and how they treat` |
|     - |  9497 | ` * an exhausted member is the difference between the two modes: under NEED_ANY it` |
|     - |  9498 | ` * contributes NULL and the walk carries on, under NEED_ALL it is php's` |
|     - |  9499 | `` * `Called current() with non valid sub iterator` — a different refusal from the`` |
|     - |  9500 | `` * empty set's `Called current() on an invalid iterator`. MIT_KEYS_ASSOC keys that`` |
|     - |  9501 | `` * array by the `$info` each iterator was attached with, which is what makes a`` |
|     - |  9502 | ` * NULL info an error at KEY time rather than at attach time, and what makes a` |
|     - |  9503 | ` * DUPLICATE info an error at attach.` |
|     - |  9504 | ` */` |
|     - |  9505 | `#define MIT_NEED_ANY      0` |
|     - |  9506 | `#define MIT_NEED_ALL      1` |
|     - |  9507 | `#define MIT_KEYS_NUMERIC  0` |
|     - |  9508 | `#define MIT_KEYS_ASSOC    2` |
|     - |  9509 | `#define MIT_FL "__mfl"   /* php's flags word, stored raw */` |
|     - |  9510 |  |
|    98 |  9511 | `static sxi64 MitFlags(ph7_class_instance *pThis)` |
|     1 |  9512 | `{` |
|    99 |  9513 | `	return pThis ? PH7_NativeAttrInt(pThis,MIT_FL) : 0;` |
|     1 |  9514 | `}` |
|     - |  9515 | `/*` |
|     - |  9516 | `` * php compares two `$info`s with zend_is_identical, not with `==` and not as`` |
|     - |  9517 | ` * array keys: the string "5" and the int 5 are DIFFERENT infos and both may be` |
|     - |  9518 | `` * attached, while `true` and `1.5` are the same one because the ZPP narrowed`` |
|     - |  9519 | ` * both to the int 1.` |
|     - |  9520 | ` */` |
|    20 |  9521 | `static int MitInfoSame(ph7_value *pA,ph7_value *pB)` |
|     1 |  9522 | `{` |
|    21 |  9523 | `	if( (pA->iFlags & MEMOBJ_STRING) != (pB->iFlags & MEMOBJ_STRING) ){` |
|     3 |  9524 | `		return 0;` |
|     - |  9525 | `	}` |
|    19 |  9526 | `	if( pA->iFlags & MEMOBJ_STRING ){` |
|    17 |  9527 | `		sxu32 nA = SyBlobLength(&pA->sBlob), nB = SyBlobLength(&pB->sBlob);` |
|    17 |  9528 | `		return nA == nB` |
|    24 |  9529 | `			&& (nA == 0 \|\| SyMemcmp(SyBlobData(&pA->sBlob),SyBlobData(&pB->sBlob),nA) == 0);` |
|     - |  9530 | `	}` |
|     3 |  9531 | `	if( (pA->iFlags & MEMOBJ_NULL) \|\| (pB->iFlags & MEMOBJ_NULL) ){` |
|   ! 0 |  9532 | `		return 0;   /* a NULL info is never a duplicate: php only checks a given one */` |
|     - |  9533 | `	}` |
|     3 |  9534 | `	return pA->x.iVal == pB->x.iVal;` |
|    11 |  9535 | `}` |
|     - |  9536 | `/* Call a no-argument method on one sub-iterator. */` |
|   258 |  9537 | `static sxi32 MitCallOn(ph7_vm *pVm,ph7_class_instance *pIt,const char *zName,sxu32 nName,` |
|     - |  9538 | `	ph7_value *pOut)` |
|     1 |  9539 | `{` |
|   259 |  9540 | `	ph7_class_method *pMethod = pIt ? PH7_ClassExtractMethod(pIt->pClass,zName,nName) : 0;` |
|   259 |  9541 | `	if( pMethod == 0 ){` |
|   ! 0 |  9542 | `		return SXRET_OK;` |
|     - |  9543 | `	}` |
|   259 |  9544 | `	return PH7_VmCallClassMethod(pVm,pIt,pMethod,pOut,0,0);` |
|   130 |  9545 | `}` |
|     - |  9546 | `/*` |
|     - |  9547 | ` * SNAPSHOT the members before calling into any of them. A sub-iterator's own` |
|     - |  9548 | ` * rewind()/valid()/next() is user code and may attach or detach on this very` |
|     - |  9549 | ` * object, and a hash walk holding a node pointer across that call is reading a` |
|     - |  9550 | ` * table that moved. The snapshot is an ordinary array of the {obj, inf} pairs,` |
|     - |  9551 | ` * so it holds a reference to every member for the length of the pass.` |
|     - |  9552 | ` */` |
|   126 |  9553 | `static sxi32 MitSnapshot(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)` |
|     1 |  9554 | `{` |
|   127 |  9555 | `	ph7_hashmap *pMap = SosMap(pVm,pThis);` |
|     - |  9556 | `	ph7_hashmap_node *pNode;` |
|     - |  9557 | `	sxu32 n;` |
|   127 |  9558 | `	PH7_MemObjInit(pVm,pOut);` |
|   127 |  9559 | `	if( PH7_MemObjToHashmap(pOut) != SXRET_OK ){` |
|   ! 0 |  9560 | `		return SXERR_MEM;` |
|     - |  9561 | `	}` |
|   127 |  9562 | `	if( pMap == 0 ){` |
|   ! 0 |  9563 | `		return SXRET_OK;` |
|     - |  9564 | `	}` |
|   343 |  9565 | `	for( pNode = pMap->pFirst, n = 0 ; pNode && n < pMap->nEntry ; ++n, pNode = pNode->pPrev ){` |
|   217 |  9566 | `		ph7_value *pPair = HashmapExtractNodeValue(pNode);` |
|   217 |  9567 | `		if( pPair ){` |
|   217 |  9568 | `			ph7_array_add_elem(pOut,0,pPair);` |
|   108 |  9569 | `		}` |
|   109 |  9570 | `	}` |
|   127 |  9571 | `	return SXRET_OK;` |
|    64 |  9572 | `}` |
|     - |  9573 | ``/* One snapshot entry's `obj` half as an instance, and its `inf` half. */`` |
|   236 |  9574 | `static ph7_class_instance * MitShotObj(ph7_value *pOut,sxu32 iAt,ph7_value **ppInf)` |
|     1 |  9575 | `{` |
|   237 |  9576 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  9577 | `	ph7_value *pPair,*pObj;` |
|   237 |  9578 | `	if( ppInf ){` |
|   117 |  9579 | `		*ppInf = 0;` |
|    58 |  9580 | `	}` |
|   236 |  9581 | `	if( (pOut->iFlags & MEMOBJ_HASHMAP) == 0` |
|   237 |  9582 | `	 \|\| HashmapLookupIntKey((ph7_hashmap *)pOut->x.pOther,(sxi64)iAt,&pNode) != SXRET_OK ){` |
|   ! 0 |  9583 | `		return 0;` |
|     - |  9584 | `	}` |
|   237 |  9585 | `	pPair = HashmapExtractNodeValue(pNode);` |
|   237 |  9586 | `	if( ppInf ){` |
|   117 |  9587 | `		*ppInf = SosPart(pPair,"inf");` |
|    58 |  9588 | `	}` |
|   237 |  9589 | `	pObj = SosPart(pPair,"obj");` |
|   237 |  9590 | `	if( pObj == 0 \|\| (pObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 |  9591 | `		return 0;` |
|     - |  9592 | `	}` |
|   237 |  9593 | `	return (ph7_class_instance *)pObj->x.pOther;` |
|   119 |  9594 | `}` |
|   126 |  9595 | `static sxu32 MitShotCount(ph7_value *pShot)` |
|     1 |  9596 | `{` |
|   127 |  9597 | `	return (pShot->iFlags & MEMOBJ_HASHMAP) && pShot->x.pOther` |
|   189 |  9598 | `		? ((ph7_hashmap *)pShot->x.pOther)->nEntry : 0;` |
|     1 |  9599 | `}` |
|     - |  9600 | `/* Walk every sub-iterator, calling one no-argument method on each. */` |
|    48 |  9601 | `static sxi32 MitCallAll(ph7_context *pCtx,const char *zName,sxu32 nName)` |
|     1 |  9602 | `{` |
|    49 |  9603 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  9604 | `	ph7_value sShot;` |
|     - |  9605 | `	sxu32 i,nCount;` |
|    49 |  9606 | `	sxi32 rc = MitSnapshot(pVm,PH7_ContextThis(pCtx),&sShot);` |
|    49 |  9607 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  9608 | `		PH7_MemObjRelease(&sShot);` |
|   ! 0 |  9609 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  9610 | `	}` |
|    49 |  9611 | `	nCount = MitShotCount(&sShot);` |
|   133 |  9612 | `	for( i = 0 ; i < nCount ; ++i ){` |
|    85 |  9613 | `		ph7_class_instance *pIt = MitShotObj(&sShot,i,0);` |
|    85 |  9614 | `		rc = MitCallOn(pVm,pIt,zName,nName,0);` |
|    85 |  9615 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  9616 | `			PH7_MemObjRelease(&sShot);` |
|   ! 0 |  9617 | `			return rc;` |
|     - |  9618 | `		}` |
|    43 |  9619 | `	}` |
|    49 |  9620 | `	PH7_MemObjRelease(&sShot);` |
|    49 |  9621 | `	return PH7_OK;` |
|    25 |  9622 | `}` |
|   120 |  9623 | `static sxi32 MitSubValid(ph7_vm *pVm,ph7_class_instance *pIt,int *pbValid)` |
|     1 |  9624 | `{` |
|     - |  9625 | `	ph7_value sVal;` |
|     - |  9626 | `	sxi32 rc;` |
|   121 |  9627 | `	*pbValid = 0;` |
|   121 |  9628 | `	PH7_MemObjInit(pVm,&sVal);` |
|   121 |  9629 | `	rc = MitCallOn(pVm,pIt,"valid",sizeof("valid")-1,&sVal);` |
|   121 |  9630 | `	if( rc == SXRET_OK ){` |
|   121 |  9631 | `		PH7_MemObjToBool(&sVal);          /* a STATUS, not the answer */` |
|   121 |  9632 | `		*pbValid = sVal.x.iVal != 0;` |
|    60 |  9633 | `	}` |
|   121 |  9634 | `	PH7_MemObjRelease(&sVal);` |
|   121 |  9635 | `	return rc;` |
|     1 |  9636 | `}` |
|    66 |  9637 | `static int vm_builtin_MultipleIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9638 | `{` |
|    67 |  9639 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    67 |  9640 | `	if( pThis == 0 ){` |
|   ! 0 |  9641 | `		return PH7_OK;` |
|     - |  9642 | `	}` |
|   115 |  9643 | `	PH7_NativeSetAttrInt(pCtx->pVm,pThis,MIT_FL,` |
|    48 |  9644 | `		nArg > 0 ? ph7_value_to_int64(apArg[0]) : MIT_NEED_ALL);` |
|    67 |  9645 | `	return PH7_OK;` |
|    34 |  9646 | `}` |
|    20 |  9647 | `static int vm_builtin_MultipleIterator_getFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9648 | `{` |
|    10 |  9649 | `	SXUNUSED(nArg);` |
|    10 |  9650 | `	SXUNUSED(apArg);` |
|    21 |  9651 | `	ph7_result_int64(pCtx,MitFlags(PH7_ContextThis(pCtx)));` |
|    21 |  9652 | `	return PH7_OK;` |
|     1 |  9653 | `}` |
|     8 |  9654 | `static int vm_builtin_MultipleIterator_setFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9655 | `{` |
|     9 |  9656 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     9 |  9657 | `	if( nArg > 0 && pThis ){` |
|     - |  9658 | `		/* php screens nothing here: the word is stored as given. */` |
|     9 |  9659 | `		PH7_NativeSetAttrInt(pCtx->pVm,pThis,MIT_FL,ph7_value_to_int64(apArg[0]));` |
|     4 |  9660 | `	}` |
|     9 |  9661 | `	return PH7_OK;` |
|     1 |  9662 | `}` |
|     - |  9663 | `/*` |
|     - |  9664 | ` * php's attachIterator: the $info must be UNIQUE across the table, which is what` |
|     - |  9665 | ` * MIT_KEYS_ASSOC needs to build a key set — checked whatever the flags say, since` |
|     - |  9666 | ` * they can be turned on later.` |
|     - |  9667 | ` */` |
|    56 |  9668 | `static int vm_builtin_MultipleIterator_attachIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9669 | `{` |
|    57 |  9670 | `	ph7_vm *pVm = pCtx->pVm;` |
|    57 |  9671 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  9672 | `	ph7_hashmap *pMap;` |
|     - |  9673 | `	ph7_hashmap_node *pNode;` |
|     - |  9674 | `	ph7_value sInf;` |
|     - |  9675 | `	sxu32 n;` |
|     - |  9676 | `	sxi32 rc;` |
|    57 |  9677 | `	if( nArg < 1 \|\| pThis == 0 ){` |
|   ! 0 |  9678 | `		return PH7_OK;` |
|     - |  9679 | `	}` |
|    57 |  9680 | `	PH7_MemObjInit(pVm,&sInf);` |
|    57 |  9681 | `	if( nArg > 1 && (apArg[1]->iFlags & MEMOBJ_NULL) == 0 ){` |
|    45 |  9682 | `		PH7_MemObjStore(apArg[1],&sInf);` |
|     - |  9683 | ``		/* php's `string\|int` ZPP: a numeric string stays a string, everything else`` |
|     - |  9684 | `		 * that is not already one becomes an int. */` |
|    45 |  9685 | `		if( (sInf.iFlags & MEMOBJ_STRING) == 0 ){` |
|     9 |  9686 | `			PH7_MemObjToInteger(&sInf);` |
|     4 |  9687 | `		}` |
|    45 |  9688 | `		pMap = SosMap(pVm,pThis);` |
|    61 |  9689 | `		for( pNode = pMap ? pMap->pFirst : 0, n = 0 ; pNode && n < pMap->nEntry ; ++n ){` |
|    21 |  9690 | `			ph7_value *pPair = HashmapExtractNodeValue(pNode);` |
|    21 |  9691 | `			ph7_value *pOld = SosPart(pPair,"inf");` |
|    21 |  9692 | `			if( pOld && MitInfoSame(pOld,&sInf) ){` |
|     5 |  9693 | `				PH7_MemObjRelease(&sInf);` |
|     5 |  9694 | `				return PH7_VmThrowException(pCtx,"InvalidArgumentException",` |
|     - |  9695 | `					"Key duplication error");` |
|     - |  9696 | `			}` |
|    17 |  9697 | `			pNode = pNode->pPrev;` |
|     9 |  9698 | `		}` |
|    20 |  9699 | `	}` |
|    53 |  9700 | `	rc = SosAttach(pCtx,pThis,apArg[0],&sInf);` |
|    53 |  9701 | `	PH7_MemObjRelease(&sInf);` |
|    53 |  9702 | `	return rc;` |
|    29 |  9703 | `}` |
|     6 |  9704 | `static int vm_builtin_MultipleIterator_detachIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9705 | `{` |
|     7 |  9706 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     7 |  9707 | `	if( nArg < 1 \|\| pThis == 0 ){` |
|   ! 0 |  9708 | `		return PH7_OK;` |
|     - |  9709 | `	}` |
|     7 |  9710 | `	return SosDetach(pCtx,pThis,apArg[0],0);` |
|     4 |  9711 | `}` |
|     4 |  9712 | `static int vm_builtin_MultipleIterator_containsIterator(ph7_context *pCtx,int nArg,` |
|     - |  9713 | `	ph7_value **apArg)` |
|     1 |  9714 | `{` |
|     5 |  9715 | `	ph7_vm *pVm = pCtx->pVm;` |
|     5 |  9716 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  9717 | `	ph7_hashmap *pMap;` |
|     5 |  9718 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  9719 | `	ph7_value sObj,sKey;` |
|     - |  9720 | `	sxi32 rc;` |
|     5 |  9721 | `	if( nArg < 1 \|\| pThis == 0 ){` |
|   ! 0 |  9722 | `		return PH7_OK;` |
|     - |  9723 | `	}` |
|     5 |  9724 | `	PH7_MemObjInit(pVm,&sObj);` |
|     5 |  9725 | `	PH7_MemObjInit(pVm,&sKey);` |
|     5 |  9726 | `	PH7_MemObjStore(apArg[0],&sObj);` |
|     5 |  9727 | `	rc = SosKey(pCtx,pThis,&sObj,&sKey);` |
|     5 |  9728 | `	if( rc == PH7_OK ){` |
|     5 |  9729 | `		pMap = SosMap(pVm,pThis);` |
|     9 |  9730 | `		ph7_result_bool(pCtx,` |
|     4 |  9731 | `			pMap && PH7_HashmapLookup(pMap,&sKey,&pNode) == SXRET_OK);` |
|     2 |  9732 | `	}` |
|     5 |  9733 | `	PH7_MemObjRelease(&sObj);` |
|     5 |  9734 | `	PH7_MemObjRelease(&sKey);` |
|     5 |  9735 | `	return rc;` |
|     3 |  9736 | `}` |
|    12 |  9737 | `static int vm_builtin_MultipleIterator_countIterators(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9738 | `{` |
|     - |  9739 | `	ph7_hashmap *pMap;` |
|     6 |  9740 | `	SXUNUSED(nArg);` |
|     6 |  9741 | `	SXUNUSED(apArg);` |
|    13 |  9742 | `	pMap = SosMap(pCtx->pVm,PH7_ContextThis(pCtx));` |
|    13 |  9743 | `	ph7_result_int64(pCtx,pMap ? (sxi64)pMap->nEntry : 0);` |
|    13 |  9744 | `	return PH7_OK;` |
|     1 |  9745 | `}` |
|    22 |  9746 | `static int vm_builtin_MultipleIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9747 | `{` |
|    11 |  9748 | `	SXUNUSED(nArg);` |
|    11 |  9749 | `	SXUNUSED(apArg);` |
|    23 |  9750 | `	return MitCallAll(pCtx,"rewind",sizeof("rewind")-1);` |
|     1 |  9751 | `}` |
|    26 |  9752 | `static int vm_builtin_MultipleIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9753 | `{` |
|    13 |  9754 | `	SXUNUSED(nArg);` |
|    13 |  9755 | `	SXUNUSED(apArg);` |
|    27 |  9756 | `	return MitCallAll(pCtx,"next",sizeof("next")-1);` |
|     1 |  9757 | `}` |
|    26 |  9758 | `static int vm_builtin_MultipleIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9759 | `{` |
|    27 |  9760 | `	ph7_vm *pVm = pCtx->pVm;` |
|    27 |  9761 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    27 |  9762 | `	int bExpect = (MitFlags(pThis) & MIT_NEED_ALL) ? 1 : 0;` |
|     - |  9763 | `	ph7_value sShot;` |
|     - |  9764 | `	sxu32 i,nCount;` |
|     - |  9765 | `	sxi32 rc;` |
|    13 |  9766 | `	SXUNUSED(nArg);` |
|    13 |  9767 | `	SXUNUSED(apArg);` |
|    27 |  9768 | `	rc = MitSnapshot(pVm,pThis,&sShot);` |
|    27 |  9769 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  9770 | `		PH7_MemObjRelease(&sShot);` |
|   ! 0 |  9771 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  9772 | `	}` |
|    27 |  9773 | `	nCount = MitShotCount(&sShot);` |
|    27 |  9774 | `	if( nCount < 1 ){` |
|     - |  9775 | `		/* php: an empty set is never valid, whichever mode it is in. */` |
|     3 |  9776 | `		PH7_MemObjRelease(&sShot);` |
|     3 |  9777 | `		ph7_result_bool(pCtx,0);` |
|     3 |  9778 | `		return PH7_OK;` |
|     - |  9779 | `	}` |
|    45 |  9780 | `	for( i = 0 ; i < nCount ; ++i ){` |
|    37 |  9781 | `		int bValid = 0;` |
|    37 |  9782 | `		rc = MitSubValid(pVm,MitShotObj(&sShot,i,0),&bValid);` |
|    37 |  9783 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  9784 | `			PH7_MemObjRelease(&sShot);` |
|   ! 0 |  9785 | `			return rc;` |
|     - |  9786 | `		}` |
|    37 |  9787 | `		if( bValid != bExpect ){` |
|     - |  9788 | `			/* NEED_ALL stops at the first invalid one, NEED_ANY at the first valid` |
|     - |  9789 | `			 * one, and each answers the opposite of what it was looking for. */` |
|    17 |  9790 | `			PH7_MemObjRelease(&sShot);` |
|    17 |  9791 | `			ph7_result_bool(pCtx,!bExpect);` |
|    17 |  9792 | `			return PH7_OK;` |
|     - |  9793 | `		}` |
|    11 |  9794 | `	}` |
|     9 |  9795 | `	PH7_MemObjRelease(&sShot);` |
|     9 |  9796 | `	ph7_result_bool(pCtx,bExpect);` |
|     9 |  9797 | `	return PH7_OK;` |
|    14 |  9798 | `}` |
|     - |  9799 | `/*` |
|     - |  9800 | ` * php's spl_multiple_iterator_get_all, which current() and key() share. The` |
|     - |  9801 | ` * refusals differ by cause: an EMPTY set is "on an invalid iterator", an` |
|     - |  9802 | ` * exhausted member under NEED_ALL is "with non valid sub iterator", and a NULL` |
|     - |  9803 | ` * $info under MIT_KEYS_ASSOC is the InvalidArgumentException this is the only` |
|     - |  9804 | ` * site of.` |
|     - |  9805 | ` */` |
|    52 |  9806 | `static int MitGetAll(ph7_context *pCtx,int bKey)` |
|     1 |  9807 | `{` |
|    53 |  9808 | `	ph7_vm *pVm = pCtx->pVm;` |
|    53 |  9809 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    53 |  9810 | `	const char *zWhat = bKey ? "key" : "current";` |
|    53 |  9811 | `	sxi64 iFlags = MitFlags(pThis);` |
|     - |  9812 | `	ph7_value sShot,sOut,sVal;` |
|     - |  9813 | `	sxu32 i,nCount;` |
|     - |  9814 | `	sxi32 rc;` |
|    53 |  9815 | `	rc = MitSnapshot(pVm,pThis,&sShot);` |
|    53 |  9816 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  9817 | `		PH7_MemObjRelease(&sShot);` |
|   ! 0 |  9818 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  9819 | `	}` |
|    53 |  9820 | `	nCount = MitShotCount(&sShot);` |
|    53 |  9821 | `	if( nCount < 1 ){` |
|     5 |  9822 | `		PH7_MemObjRelease(&sShot);` |
|     7 |  9823 | `		return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     2 |  9824 | `			"Called %s() on an invalid iterator",zWhat);` |
|     - |  9825 | `	}` |
|    49 |  9826 | `	PH7_MemObjInit(pVm,&sOut);` |
|    49 |  9827 | `	if( PH7_MemObjToHashmap(&sOut) != SXRET_OK ){` |
|   ! 0 |  9828 | `		PH7_MemObjRelease(&sShot);` |
|   ! 0 |  9829 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 |  9830 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  9831 | `	}` |
|   125 |  9832 | `	for( i = 0 ; i < nCount ; ++i ){` |
|    85 |  9833 | `		ph7_value *pInf = 0;` |
|    85 |  9834 | `		ph7_class_instance *pIt = MitShotObj(&sShot,i,&pInf);` |
|    85 |  9835 | `		int bValid = 0;` |
|     - |  9836 | `		/* php asks the sub-iterator FIRST and only then looks at the key it would` |
|     - |  9837 | `		 * file the answer under, so an exhausted member under NEED_ALL reports the` |
|     - |  9838 | `		 * iterator rather than the missing $info. */` |
|    85 |  9839 | `		rc = MitSubValid(pVm,pIt,&bValid);` |
|    85 |  9840 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  9841 | `			goto fail;` |
|     - |  9842 | `		}` |
|    85 |  9843 | `		PH7_MemObjInit(pVm,&sVal);` |
|    85 |  9844 | `		if( bValid ){` |
|    55 |  9845 | `			rc = MitCallOn(pVm,pIt,bKey ? "key" : "current",bKey ? 3 : 7,&sVal);` |
|    55 |  9846 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  9847 | `				PH7_MemObjRelease(&sVal);` |
|   ! 0 |  9848 | `				goto fail;` |
|     1 |  9849 | `			}` |
|    58 |  9850 | `		}else if( iFlags & MIT_NEED_ALL ){` |
|     7 |  9851 | `			PH7_MemObjRelease(&sVal);` |
|     7 |  9852 | `			PH7_MemObjRelease(&sShot);` |
|     7 |  9853 | `			PH7_MemObjRelease(&sOut);` |
|    11 |  9854 | `			return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     3 |  9855 | `				"Called %s() with non valid sub iterator",zWhat);` |
|     - |  9856 | `		}` |
|     - |  9857 | `		/* NEED_ANY leaves the null sVal in place: an exhausted member contributes` |
|     - |  9858 | `		 * php's null and the walk carries on. */` |
|    79 |  9859 | `		if( iFlags & MIT_KEYS_ASSOC ){` |
|     - |  9860 | ``			/* The snapshot's own `inf` slot: re-read after the call above, since it`` |
|     - |  9861 | `			 * lives in a hashmap the call may have moved. */` |
|    33 |  9862 | `			MitShotObj(&sShot,i,&pInf);` |
|    33 |  9863 | `			if( pInf == 0 \|\| (pInf->iFlags & MEMOBJ_NULL) ){` |
|     3 |  9864 | `				PH7_MemObjRelease(&sVal);` |
|     3 |  9865 | `				PH7_MemObjRelease(&sShot);` |
|     3 |  9866 | `				PH7_MemObjRelease(&sOut);` |
|     3 |  9867 | `				return PH7_VmThrowException(pCtx,"InvalidArgumentException",` |
|     - |  9868 | `					"Sub-Iterator is associated with NULL");` |
|     - |  9869 | `			}` |
|    15 |  9870 | `		}` |
|    77 |  9871 | `		ph7_array_add_elem(&sOut,(iFlags & MIT_KEYS_ASSOC) ? pInf : 0,&sVal);` |
|    77 |  9872 | `		PH7_MemObjRelease(&sVal);` |
|    39 |  9873 | `	}` |
|    41 |  9874 | `	PH7_MemObjRelease(&sShot);` |
|    41 |  9875 | `	ph7_result_value(pCtx,&sOut);` |
|    41 |  9876 | `	PH7_MemObjRelease(&sOut);` |
|    41 |  9877 | `	return PH7_OK;` |
|   ! 0 |  9878 | `fail:` |
|   ! 0 |  9879 | `	PH7_MemObjRelease(&sShot);` |
|   ! 0 |  9880 | `	PH7_MemObjRelease(&sOut);` |
|   ! 0 |  9881 | `	return rc;` |
|    27 |  9882 | `}` |
|    24 |  9883 | `static int vm_builtin_MultipleIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9884 | `{` |
|    12 |  9885 | `	SXUNUSED(nArg);` |
|    12 |  9886 | `	SXUNUSED(apArg);` |
|    25 |  9887 | `	return MitGetAll(pCtx,FALSE);` |
|     1 |  9888 | `}` |
|    28 |  9889 | `static int vm_builtin_MultipleIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9890 | `{` |
|    14 |  9891 | `	SXUNUSED(nArg);` |
|    14 |  9892 | `	SXUNUSED(apArg);` |
|    29 |  9893 | `	return MitGetAll(pCtx,TRUE);` |
|     1 |  9894 | `}` |
|  6721 |  9895 | `static sxi32 VmInstallSplMultipleIterator(ph7_vm *pVm)` |
|     5 |  9896 | `{` |
|     - |  9897 | `	static const PH7_NativeConstDef aMitConst[] = {` |
|     - |  9898 | `		{ "MIT_NEED_ANY",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, MIT_NEED_ANY, 0, 0.0 },` |
|     - |  9899 | `		{ "MIT_NEED_ALL",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, MIT_NEED_ALL, 0, 0.0 },` |
|     - |  9900 | `		{ "MIT_KEYS_NUMERIC", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, MIT_KEYS_NUMERIC, 0, 0.0 },` |
|     - |  9901 | `		{ "MIT_KEYS_ASSOC",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, MIT_KEYS_ASSOC, 0, 0.0 },` |
|     - |  9902 | `	};` |
|     - |  9903 | `	static const PH7_NativePropDef aMitProp[] = {` |
|     - |  9904 | `		{ SOS_S,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  9905 | `		{ MIT_FL, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  9906 | `	};` |
|     - |  9907 | `	static const PH7_NativeMethodDef aMitMethod[] = {` |
|     - |  9908 | `		{ "__construct",      PH7_MOD_PUBLIC, "int $flags = MultipleIterator::MIT_NEED_ALL \| MultipleIterator::MIT_KEYS_NUMERIC", 0,` |
|     - |  9909 | `		  vm_builtin_MultipleIterator_construct },` |
|     - |  9910 | `		{ "getFlags",         PH7_MOD_PUBLIC, "", "@int", vm_builtin_MultipleIterator_getFlags },` |
|     - |  9911 | `		{ "setFlags",         PH7_MOD_PUBLIC, "int $flags", "@void",` |
|     - |  9912 | `		  vm_builtin_MultipleIterator_setFlags },` |
|     - |  9913 | `		{ "attachIterator",   PH7_MOD_PUBLIC, "Iterator $iterator, string\|int\|null $info = null",` |
|     - |  9914 | `		  "@void", vm_builtin_MultipleIterator_attachIterator },` |
|     - |  9915 | `		{ "detachIterator",   PH7_MOD_PUBLIC, "Iterator $iterator", "@void",` |
|     - |  9916 | `		  vm_builtin_MultipleIterator_detachIterator },` |
|     - |  9917 | `		{ "containsIterator", PH7_MOD_PUBLIC, "Iterator $iterator", "@bool",` |
|     - |  9918 | `		  vm_builtin_MultipleIterator_containsIterator },` |
|     - |  9919 | `		{ "countIterators",   PH7_MOD_PUBLIC, "", "@int",` |
|     - |  9920 | `		  vm_builtin_MultipleIterator_countIterators },` |
|     - |  9921 | `		{ "rewind",           PH7_MOD_PUBLIC, "", "@void", vm_builtin_MultipleIterator_rewind },` |
|     - |  9922 | `		{ "valid",            PH7_MOD_PUBLIC, "", "@bool", vm_builtin_MultipleIterator_valid },` |
|     - |  9923 | `		{ "key",              PH7_MOD_PUBLIC, "", "@array", vm_builtin_MultipleIterator_key },` |
|     - |  9924 | `		{ "current",          PH7_MOD_PUBLIC, "", "@array", vm_builtin_MultipleIterator_current },` |
|     - |  9925 | `		{ "next",             PH7_MOD_PUBLIC, "", "@void", vm_builtin_MultipleIterator_next },` |
|     - |  9926 | `		{ "__debugInfo",      PH7_MOD_PUBLIC, "", "@array",` |
|     - |  9927 | `		  vm_builtin_SplObjectStorage_debugInfo },` |
|     - |  9928 | `	};` |
|     - |  9929 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - |  9930 | `		/* No presenter: php declares no properties and shows none, and the storage` |
|     - |  9931 | `		 * is reachable only through __debugInfo() — which is SplObjectStorage's own,` |
|     - |  9932 | `		 * mangled key included, because the struct behind both classes is one. */` |
|     - |  9933 | `		{ "MultipleIterator", 0, "Iterator", 0,` |
|     - |  9934 | `		  aMitMethod, SX_ARRAYSIZE(aMitMethod), aMitConst, SX_ARRAYSIZE(aMitConst),` |
|     - |  9935 | `		  aMitProp, SX_ARRAYSIZE(aMitProp), 0, 0, 0 },` |
|     - |  9936 | `	};` |
|  6726 |  9937 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 |  9938 | `}` |
|     - |  9939 | `/*` |
|     - |  9940 | ` * ---------------------------------------------------------------------------` |
|     - |  9941 | ` * SplFileInfo.` |
|     - |  9942 | ` *` |
|     - |  9943 | `` * php's `spl_filesystem_object` keeps TWO strings for a path, and which one a`` |
|     - |  9944 | `` * method reads is the whole model: `file_name` is the pathname with any trailing`` |
|     - |  9945 | `` * slashes stripped, and `path` is everything before the LAST slash of it -- which`` |
|     - |  9946 | ` * is EMPTY when the name has no slash before its last component, so` |
|     - |  9947 | `` * `(new SplFileInfo('/a.txt'))->getPath()` is `''` and `getFilename()` answers the`` |
|     - |  9948 | `` * whole `/a.txt`. Every accessor is a slice of that pair (php's own`` |
|     - |  9949 | `` * `spl_filesystem_info_set_filename`), and the chunk, which called `basename()` and`` |
|     - |  9950 | `` * `dirname()` per method instead, disagreed on all of it.`` |
|     - |  9951 | ` *` |
|     - |  9952 | `` * The stat family is php's `FileInfoFunction` macro: `php_stat()` with the error`` |
|     - |  9953 | ` * handler REPLACED, so the warning a failed stat would print becomes a` |
|     - |  9954 | `` * `RuntimeException` instead -- `getSize()` on a missing file RAISES there and`` |
|     - |  9955 | ` * warned-then-answered-false here. Two of the fifteen lstat rather than stat` |
|     - |  9956 | `` * (`getType`, `isLink`), which is php's IS_LINK_OPERATION set.`` |
|     - |  9957 | ` *` |
|     - |  9958 | ` * The two slots are PRIVATE and PRESENTED: php declares no properties at all` |
|     - |  9959 | `` * (`getProperties()`, the `(array)` cast and `get_object_vars()` are empty) while`` |
|     - |  9960 | `` * `var_dump` shows `pathName`/`fileName` under their mangled private keys, and`` |
|     - |  9961 | `` * `__debugInfo()` hands back that same array. The class is `@not-serializable`.`` |
|     - |  9962 | ` *` |
|     - |  9963 | `` * `openFile()` and `setFileClass()` are the doors into SplFileObject and answer`` |
|     - |  9964 | `` * one; `setInfoClass()` and the `?string $class` argument of`` |
|     - |  9965 | `` * `getFileInfo()`/`getPathInfo()` name a class derived from THIS one. The two`` |
|     - |  9966 | ` * class names live in slots of their own, which is what makes them survive a` |
|     - |  9967 | ` * clone and travel to a directory iterator's children.` |
|     - |  9968 | ` */` |
|     - |  9969 | `#define SFI_N  "__n"   /* php's file_name: the pathname, trailing slashes stripped */` |
|     - |  9970 | `#define SFI_P  "__p"   /* php's path: everything before its last slash */` |
|     - |  9971 | `#define SFI_IC "__ic"  /* php's info_class */` |
|     - |  9972 | `#define SFI_FC "__fc"  /* php's file_class, what openFile() builds */` |
|     - |  9973 | `` /* The directory-iterator half of php's struct, on the same instance: its `u.dir` `` |
|     - |  9974 | ` * arm minus the handle, which cannot live in a php-visible slot (see VmDirHandle).` |
|     - |  9975 | `` * Declared by DirectoryIterator, so `SplDirIs()` is what tells the two apart. */`` |
|     - |  9976 | `#define SDI_E  "__e"   /* php's u.dir.entry.d_name; "" once the walk has run out */` |
|     - |  9977 | `#define SDI_I  "__i"   /* php's u.dir.index: what key() answers */` |
|     - |  9978 | `#define SDI_F  "__f"   /* php's flags */` |
|     - |  9979 | `#define SDI_S  "__s"   /* php's u.dir.sub_path (RecursiveDirectoryIterator) */` |
|     - |  9980 | ``/* And the FILE arm, php's `u.file`. Declared by SplFileObject; named up here`` |
|     - |  9981 | ` * because the shared presentation hook shows three of its slots. */` |
|     - |  9982 | `#define SFO_H  "__fh"  /* the open io_private, as a resource */` |
|     - |  9983 | `#define SFO_M  "__fo"  /* php's u.file.open_mode */` |
|     - |  9984 | `#define SFO_FL "__ff"  /* php's flags */` |
|     - |  9985 | `#define SFO_ML "__fm"  /* php's u.file.max_line_len */` |
|     - |  9986 | `#define SFO_D  "__fd"  /* php's u.file.delimiter */` |
|     - |  9987 | `#define SFO_EN "__fn"  /* php's u.file.enclosure */` |
|     - |  9988 | `#define SFO_ES "__fx"  /* php's u.file.escape (PH7_CSV_NO_ESCAPE = disabled) */` |
|     - |  9989 | `#define SFO_ED "__fq"  /* php's u.file.is_escape_default */` |
|     - |  9990 | `#define SFO_L  "__fl"  /* php's u.file.current_line */` |
|     - |  9991 | `#define SFO_Z  "__fz"  /* php's u.file.current_zval */` |
|     - |  9992 | `#define SFO_LS "__fs"  /* which of the two is live (SFO_HAS_*) */` |
|     - |  9993 | `#define SFO_K  "__fk"  /* php's u.file.current_line_num */` |
|     - |  9994 | `/* The open stream behind an SplFileObject, or 0 for one that has none. */` |
|     - |  9995 | `static io_private * SfoDev(ph7_class_instance *pThis);` |
|     - |  9996 |  |
|     - |  9997 | `/* php's IS_SLASH is PLATFORM-dependent: a backslash separates on Windows and is an` |
|     - |  9998 | ``  * ordinary filename byte everywhere else, which is why `new SplFileInfo('C:\\x\\y')` `` |
|     - |  9999 | ` * has an empty path on unix. PH7_ExtractDirName draws the same line. */` |
|     - | 10000 | `#ifdef __WINNT__` |
|     - | 10001 | `# define SFI_IS_SLASH(c) ((c) == '/' \|\| (c) == '\\')` |
|     - | 10002 | `#else` |
|     - | 10003 | `# define SFI_IS_SLASH(c) ((c) == '/')` |
|     - | 10004 | `#endif` |
|     - | 10005 |  |
|     - | 10006 | `/*` |
|     - | 10007 | ` * The directory-iterator half of this family, declared up here because php's` |
|     - | 10008 | `` * SplFileInfo bodies BRANCH on `spl_filesystem_object::type`: a DIR instance`` |
|     - | 10009 | ` * keeps its pathname lazily (path + slash + the current entry, rebuilt after` |
|     - | 10010 | ` * every read) and answers nothing at all once the walk has run out. Exactly` |
|     - | 10011 | ` * five accessors below ask, which is the same five php branches in.` |
|     - | 10012 | ` */` |
|     - | 10013 | `static int SplDirIs(ph7_vm *pVm,ph7_class_instance *pThis);` |
|     - | 10014 | `static const char * SplDirName(ph7_vm *pVm,ph7_class_instance *pThis,int *pnLen);` |
|     - | 10015 | `static int SplDirAtEnd(ph7_class_instance *pThis);` |
|     - | 10016 | `static VmDirHandle * SplDirState(ph7_vm *pVm,ph7_class_instance *pThis);` |
|     - | 10017 | `/* One of the two path slots, as bytes. */` |
|  2327 | 10018 | `static const char * SfiStr(ph7_class_instance *pThis,const char *zSlot,int *pnLen)` |
|     2 | 10019 | `{` |
|  2329 | 10020 | `	ph7_value *pVal = pThis ? PH7_NativeAttr(pThis,zSlot) : 0;` |
|  2329 | 10021 | `	*pnLen = 0;` |
|  2329 | 10022 | `	if( pVal == 0 ){` |
|   ! 0 | 10023 | `		return "";` |
|     - | 10024 | `	}` |
|  2329 | 10025 | `	return ph7_value_to_string(pVal,pnLen);` |
|  1173 | 10026 | `}` |
|     - | 10027 | `/*` |
|     - | 10028 | ` * php's spl_filesystem_info_set_filename: strip the trailing slashes (never the` |
|     - | 10029 | ` * only character), then cut the path at the last slash of what is left. A name` |
|     - | 10030 | ` * with no slash before its final component keeps an EMPTY path, which is what` |
|     - | 10031 | ` * makes getFilename() answer the whole thing.` |
|     - | 10032 | ` */` |
|   302 | 10033 | `static void SfiSetName(ph7_vm *pVm,ph7_class_instance *pThis,const char *zPath,int nPath)` |
|     1 | 10034 | `{` |
|   303 | 10035 | `	int nFile = nPath;` |
|     - | 10036 | `	int nDir;` |
|   303 | 10037 | `	if( nFile > 1 && SFI_IS_SLASH(zPath[nFile-1]) ){` |
|     4 | 10038 | `		do{` |
|     9 | 10039 | `			nFile--;` |
|     9 | 10040 | `		}while( nFile > 1 && SFI_IS_SLASH(zPath[nFile-1]) );` |
|     4 | 10041 | `	}` |
|   303 | 10042 | `	nDir = nFile;` |
|  4531 | 10043 | `	while( nDir > 1 && !SFI_IS_SLASH(zPath[nDir-1]) ){` |
|  4229 | 10044 | `		nDir--;` |
|     1 | 10045 | `	}` |
|   303 | 10046 | `	if( nDir > 0 ){` |
|   299 | 10047 | `		nDir--;` |
|   149 | 10048 | `	}` |
|   303 | 10049 | `	PH7_NativeSetAttrStr(pVm,pThis,SFI_N,zPath,nFile);` |
|   303 | 10050 | `	PH7_NativeSetAttrStr(pVm,pThis,SFI_P,zPath,nDir);` |
|   303 | 10051 | `}` |
|     - | 10052 | `/*` |
|     - | 10053 | `` * php's `file_name`: the slot for a plain SplFileInfo, and the lazily rebuilt`` |
|     - | 10054 | ` * path+slash+entry for a directory iterator. Every accessor that works on the` |
|     - | 10055 | ` * whole pathname goes through here.` |
|     - | 10056 | ` */` |
|   569 | 10057 | `static const char * SfiName(ph7_vm *pVm,ph7_class_instance *pThis,int *pnLen)` |
|     2 | 10058 | `{` |
|   571 | 10059 | `	if( SplDirIs(pVm,pThis) ){` |
|   169 | 10060 | `		return SplDirName(pVm,pThis,pnLen);` |
|     - | 10061 | `	}` |
|   404 | 10062 | `	return SfiStr(pThis,SFI_N,pnLen);` |
|   287 | 10063 | `}` |
|     - | 10064 | `/*` |
|     - | 10065 | `` * php's "the file name without the path": the slice after `path` + its slash when`` |
|     - | 10066 | ` * the path is a real prefix, and the whole name otherwise. getFilename(),` |
|     - | 10067 | ` * getBasename() and getExtension() all start here.` |
|     - | 10068 | ` */` |
|     - | 10069 | `/* A GlobIterator's path comes from its STREAM rather than from its slot` |
|     - | 10070 | ` * (defined with the directory machinery below); 0 for any other object. */` |
|     - | 10071 | `static const char * SplDirGlobPath(ph7_vm *pVm,ph7_class_instance *pThis,int *pnLen);` |
|   140 | 10072 | `static const char * SfiTail(ph7_vm *pVm,ph7_class_instance *pThis,int *pnLen)` |
|     1 | 10073 | `{` |
|   141 | 10074 | `	int nName = 0,nPath = 0;` |
|   141 | 10075 | `	const char *zName = SfiName(pVm,pThis,&nName);` |
|     - | 10076 | `	/* The name was JOINED from the walk's path, which for a glob handle is the` |
|     - | 10077 | ``	 * current match's directory and not the `glob://pattern` in the slot --`` |
|     - | 10078 | `	 * measuring against the slot left the whole joined name here. */` |
|   141 | 10079 | `	if( SplDirGlobPath(pVm,pThis,&nPath) == 0 ){` |
|   137 | 10080 | `		SfiStr(pThis,SFI_P,&nPath);` |
|    68 | 10081 | `	}` |
|   141 | 10082 | `	if( nPath > 0 && nPath < nName ){` |
|    77 | 10083 | `		*pnLen = nName - (nPath + 1);` |
|    77 | 10084 | `		return &zName[nPath + 1];` |
|     - | 10085 | `	}` |
|    65 | 10086 | `	*pnLen = nName;` |
|    65 | 10087 | `	return zName;` |
|    71 | 10088 | `}` |
|     - | 10089 | `/* The path this instance stands for, as a NUL-terminated buffer the VFS can take. */` |
|   151 | 10090 | `static sxi32 SfiPathBuf(ph7_vm *pVm,ph7_class_instance *pThis,char *zBuf,int nBuf)` |
|     2 | 10091 | `{` |
|   153 | 10092 | `	int nName = 0;` |
|   153 | 10093 | `	const char *zName = SfiName(pVm,pThis,&nName);` |
|   153 | 10094 | `	if( nName < 1 \|\| nName >= nBuf ){` |
|   ! 0 | 10095 | `		return SXERR_INVALID;` |
|     - | 10096 | `	}` |
|   153 | 10097 | `	SyMemcpy(zName,zBuf,(sxu32)nName);` |
|   153 | 10098 | `	zBuf[nName] = 0;` |
|   153 | 10099 | `	return SXRET_OK;` |
|    78 | 10100 | `}` |
|     - | 10101 | `/* The two refusals an accessor may owe before it reads anything (below). */` |
|     - | 10102 | `static int SfoChecked(ph7_context *pCtx,sxi32 *pRc);` |
|     - | 10103 | `/* The open the SplFileObject constructor and openFile() share (below). iCtxArg` |
|     - | 10104 | ` * is the php POSITION of the context argument, which differs between the two` |
|     - | 10105 | ` * spellings and is what a refused context is blamed on. */` |
|     - | 10106 | `static sxi32 SfoOpen(ph7_context *pCtx,ph7_class_instance *pThis,ph7_value *pPath,` |
|     - | 10107 | `	const char *zMode,int nMode,int bUseInclude,ph7_value *pCtxArg,int iCtxArg);` |
|     - | 10108 | `/*` |
|     - | 10109 | ` * php's get_file_name() ahead of an accessor that needs a path: an object whose` |
|     - | 10110 | ` * parent constructor never ran has no name AT ALL and raises Error rather than` |
|     - | 10111 | ` * failing a stat -- which for a directory iterator is the case where the open` |
|     - | 10112 | ` * never happened. Answers 0 when the caller must return *pRc.` |
|     - | 10113 | ` *` |
|     - | 10114 | ` * The SplFileObject family reaches these same accessors by inheritance and` |
|     - | 10115 | ` * refuses EARLIER and differently: php gives those classes a get_method handler` |
|     - | 10116 | ` * that stops every method on an uninitialized instance, so that check runs` |
|     - | 10117 | ` * first here too.` |
|     - | 10118 | ` */` |
|   137 | 10119 | `static int SfiDirReady(ph7_context *pCtx,sxi32 *pRc)` |
|     1 | 10120 | `{` |
|   138 | 10121 | `	ph7_vm *pVm = pCtx->pVm;` |
|   138 | 10122 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   138 | 10123 | `	if( !SfoChecked(pCtx,pRc) ){` |
|     3 | 10124 | `		return 0;` |
|     - | 10125 | `	}` |
|   136 | 10126 | `	if( SplDirIs(pVm,pThis) && SplDirState(pVm,pThis) == 0 ){` |
|     7 | 10127 | `		*pRc = PH7_VmThrowException(pCtx,"Error","Object not initialized");` |
|     7 | 10128 | `		return 0;` |
|     - | 10129 | `	}` |
|   130 | 10130 | `	return 1;` |
|    70 | 10131 | `}` |
|     - | 10132 | `/*` |
|     - | 10133 | ` * php's FileInfoFunction: the stat that backs one accessor, with the failure` |
|     - | 10134 | ` * promoted to a RuntimeException carrying the WARNING php would otherwise print.` |
|     - | 10135 | ` * The two lstat users say "Lstat failed" there, which is php's own text.` |
|     - | 10136 | ` */` |
|    64 | 10137 | `static sxi32 SfiStat(ph7_context *pCtx,const char *zMethod,int bLstat,ph7_value *pOut)` |
|     1 | 10138 | `{` |
|    65 | 10139 | `	ph7_vm *pVm = pCtx->pVm;` |
|    65 | 10140 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    65 | 10141 | `	const ph7_vfs *pVfs = pVm->pEngine->pVfs;` |
|     - | 10142 | `	ph7_value sWorker;` |
|     - | 10143 | `	char zPath[4096];` |
|    65 | 10144 | `	int rc = -1;` |
|     - | 10145 | `	sxi32 rcReady;` |
|    65 | 10146 | `	if( !SfiDirReady(pCtx,&rcReady) ){` |
|     5 | 10147 | `		return rcReady;` |
|     - | 10148 | `	}` |
|    61 | 10149 | `	if( PH7_MemObjToHashmap(pOut) != SXRET_OK ){` |
|   ! 0 | 10150 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 10151 | `	}` |
|    61 | 10152 | `	PH7_MemObjInit(pVm,&sWorker);` |
|    61 | 10153 | `	if( SfiPathBuf(pVm,pThis,zPath,(int)sizeof(zPath)) == SXRET_OK ){` |
|     - | 10154 | `		/* A path a userland stream wrapper owns is ITS stat, not the VFS's --` |
|     - | 10155 | `		 * php runs SplFileInfo's accessors through the same` |
|     - | 10156 | `		 * php_stream_url_stat_path every free function uses. zPath is a local` |
|     - | 10157 | `		 * buffer, so it survives the PHP the wrapper runs. */` |
|     - | 10158 | `		ph7_int64 aVal[13];` |
|    91 | 10159 | `		int rcU = PH7_VfsUserStatFields(pCtx,zPath,` |
|    30 | 10160 | `			bLstat ? PH7_STAT_ASK_LSTAT : PH7_STAT_ASK_STAT,aVal);` |
|    61 | 10161 | `		if( PH7_CALLBACK_UNWOUND(rcU) ){` |
|     - | 10162 | `			/* The wrapper threw: that exception is the answer, and stacking` |
|     - | 10163 | `			 * SplFileInfo's own RuntimeException on top of it left the first one` |
|     - | 10164 | `			 * already caught and the second one UNCAUGHT. */` |
|     3 | 10165 | `			PH7_MemObjRelease(&sWorker);` |
|     3 | 10166 | `			return rcU;` |
|     - | 10167 | `		}` |
|    59 | 10168 | `		if( rcU != PHL_URLSTAT_NOWRAP ){` |
|    21 | 10169 | `			rc = rcU == PHL_URLSTAT_OK ? PH7_VfsStatFill(pOut,&sWorker,aVal) : -1;` |
|    49 | 10170 | `		}else if( pVfs ){` |
|    39 | 10171 | `			if( bLstat ){` |
|   ! 0 | 10172 | `				rc = pVfs->xlStat ? pVfs->xlStat(zPath,pOut,&sWorker) : -1;` |
|   ! 0 | 10173 | `			}else{` |
|    39 | 10174 | `				rc = pVfs->xStat ? pVfs->xStat(zPath,pOut,&sWorker) : -1;` |
|     - | 10175 | `			}` |
|    19 | 10176 | `		}` |
|    29 | 10177 | `	}` |
|    59 | 10178 | `	PH7_MemObjRelease(&sWorker);` |
|    59 | 10179 | `	if( rc != PH7_OK ){` |
|    19 | 10180 | `		int nName = 0;` |
|    19 | 10181 | `		const char *zName = SfiName(pVm,pThis,&nName);` |
|    28 | 10182 | `		return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     9 | 10183 | `			"SplFileInfo::%s(): %s failed for %.*s",zMethod,bLstat ? "Lstat" : "stat",` |
|     9 | 10184 | `			nName,zName);` |
|     - | 10185 | `	}` |
|    41 | 10186 | `	return PH7_OK;` |
|    33 | 10187 | `}` |
|     - | 10188 | `/* One field of a stat array, as php's int. */` |
|    64 | 10189 | `static int SfiStatField(ph7_context *pCtx,const char *zMethod,int bLstat,const char *zField,` |
|     - | 10190 | `	sxi64 *piOut)` |
|     1 | 10191 | `{` |
|     - | 10192 | `	ph7_value sStat,*pField;` |
|     - | 10193 | `	sxi32 rc;` |
|    65 | 10194 | `	*piOut = 0;` |
|    65 | 10195 | `	PH7_MemObjInit(pCtx->pVm,&sStat);` |
|    65 | 10196 | `	rc = SfiStat(pCtx,zMethod,bLstat,&sStat);` |
|    65 | 10197 | `	if( rc != PH7_OK ){` |
|    25 | 10198 | `		PH7_MemObjRelease(&sStat);` |
|    25 | 10199 | `		return rc;` |
|     - | 10200 | `	}` |
|    41 | 10201 | `	pField = ph7_array_fetch(&sStat,zField,(int)SyStrlen(zField));` |
|    41 | 10202 | `	if( pField ){` |
|    41 | 10203 | `		*piOut = ph7_value_to_int64(pField);` |
|    20 | 10204 | `	}` |
|    41 | 10205 | `	PH7_MemObjRelease(&sStat);` |
|    41 | 10206 | `	return PH7_OK;` |
|    33 | 10207 | `}` |
|     - | 10208 | `/* The eight stat accessors that answer an int, all with the same body. */` |
|    64 | 10209 | `static int SfiStatInt(ph7_context *pCtx,const char *zMethod,const char *zField)` |
|     1 | 10210 | `{` |
|    65 | 10211 | `	sxi64 iVal = 0;` |
|    65 | 10212 | `	sxi32 rc = SfiStatField(pCtx,zMethod,FALSE,zField,&iVal);` |
|    65 | 10213 | `	if( rc != PH7_OK ){` |
|    25 | 10214 | `		return rc;` |
|     - | 10215 | `	}` |
|    41 | 10216 | `	ph7_result_int64(pCtx,iVal);` |
|    41 | 10217 | `	return PH7_OK;` |
|    33 | 10218 | `}` |
|   156 | 10219 | `static int vm_builtin_SplFileInfo_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10220 | `{` |
|   157 | 10221 | `	ph7_vm *pVm = pCtx->pVm;` |
|   157 | 10222 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 10223 | `	const char *zPath;` |
|   157 | 10224 | `	int nPath = 0;` |
|   157 | 10225 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|   ! 0 | 10226 | `		return PH7_OK;` |
|     - | 10227 | `	}` |
|   157 | 10228 | `	zPath = ph7_value_to_string(apArg[0],&nPath);` |
|   157 | 10229 | `	SfiSetName(pVm,pThis,zPath,nPath);` |
|   157 | 10230 | `	return PH7_OK;` |
|    79 | 10231 | `}` |
|    54 | 10232 | `static int vm_builtin_SplFileInfo_getPath(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10233 | `{` |
|    55 | 10234 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 10235 | `	sxi32 rcChk;` |
|    55 | 10236 | `	int nPath = 0;` |
|     - | 10237 | `	const char *zPath;` |
|    27 | 10238 | `	SXUNUSED(nArg);` |
|    27 | 10239 | `	SXUNUSED(apArg);` |
|    55 | 10240 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|     3 | 10241 | `		return rcChk;` |
|     - | 10242 | `	}` |
|     - | 10243 | `	/* A GlobIterator answers the directory of the CURRENT match (see` |
|     - | 10244 | `	 * SplDirGlobPath): the pattern in its slot names no directory. */` |
|    53 | 10245 | `	zPath = SplDirGlobPath(pCtx->pVm,pThis,&nPath);` |
|    53 | 10246 | `	if( zPath == 0 ){` |
|    49 | 10247 | `		zPath = SfiStr(pThis,SFI_P,&nPath);` |
|    24 | 10248 | `	}` |
|    53 | 10249 | `	ph7_result_string(pCtx,zPath,nPath);` |
|    53 | 10250 | `	return PH7_OK;` |
|    28 | 10251 | `}` |
|     - | 10252 | `/*` |
|     - | 10253 | `` * php's getPathname() is `spl_filesystem_object_get_pathname`, and for a DIR it`` |
|     - | 10254 | ` * answers NOTHING once the walk has run out — the empty string, without` |
|     - | 10255 | ``  * materializing the lazy name the stat family would still build (`getSize()` `` |
|     - | 10256 | `` * past the end stats the directory itself, and `var_dump` shows the difference).`` |
|     - | 10257 | ` */` |
|    72 | 10258 | `static int vm_builtin_SplFileInfo_getPathname(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 10259 | `{` |
|     - | 10260 | `	sxi32 rcChk;` |
|    74 | 10261 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    74 | 10262 | `	int nName = 0;` |
|     - | 10263 | `	const char *zName;` |
|    36 | 10264 | `	SXUNUSED(nArg);` |
|    36 | 10265 | `	SXUNUSED(apArg);` |
|    74 | 10266 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|     5 | 10267 | `		return rcChk;` |
|     - | 10268 | `	}` |
|    70 | 10269 | `	if( SplDirIs(pCtx->pVm,pThis) && SplDirAtEnd(pThis) ){` |
|     7 | 10270 | `		ph7_result_string(pCtx,"",0);` |
|     7 | 10271 | `		return PH7_OK;` |
|     - | 10272 | `	}` |
|    64 | 10273 | `	zName = SfiName(pCtx->pVm,pThis,&nName);` |
|    64 | 10274 | `	ph7_result_string(pCtx,zName,nName);` |
|    64 | 10275 | `	return PH7_OK;` |
|    38 | 10276 | `}` |
|    62 | 10277 | `static int vm_builtin_SplFileInfo_getFilename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10278 | `{` |
|     - | 10279 | `	sxi32 rcChk;` |
|    63 | 10280 | `	int nTail = 0;` |
|    63 | 10281 | `	const char *zTail = SfiTail(pCtx->pVm,PH7_ContextThis(pCtx),&nTail);` |
|    31 | 10282 | `	SXUNUSED(nArg);` |
|    31 | 10283 | `	SXUNUSED(apArg);` |
|    63 | 10284 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|     5 | 10285 | `		return rcChk;` |
|     - | 10286 | `	}` |
|    59 | 10287 | `	ph7_result_string(pCtx,zTail,nTail);` |
|    59 | 10288 | `	return PH7_OK;` |
|    32 | 10289 | `}` |
|     - | 10290 | `/* php's getBasename(): php_basename() of the tail, suffix rule included. */` |
|    32 | 10291 | `static int vm_builtin_SplFileInfo_getBasename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10292 | `{` |
|     - | 10293 | `	sxi32 rcChk;` |
|    33 | 10294 | `	int nTail = 0,nBase = 0;` |
|    33 | 10295 | `	const char *zTail = SfiTail(pCtx->pVm,PH7_ContextThis(pCtx),&nTail);` |
|    33 | 10296 | `	const char *zBase = PH7_ExtractBaseName(zTail,nTail,&nBase);` |
|    33 | 10297 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 10298 | `		return rcChk;` |
|     - | 10299 | `	}` |
|    33 | 10300 | `	if( nArg > 0 ){` |
|     5 | 10301 | `		int nSuffix = 0;` |
|     5 | 10302 | `		const char *zSuffix = ph7_value_to_string(apArg[0],&nSuffix);` |
|     4 | 10303 | `		if( nSuffix > 0 && nSuffix < nBase` |
|     4 | 10304 | `		 && SyMemcmp(&zBase[nBase - nSuffix],zSuffix,(sxu32)nSuffix) == 0 ){` |
|     3 | 10305 | `			nBase -= nSuffix;` |
|     1 | 10306 | `		}` |
|     2 | 10307 | `	}` |
|    33 | 10308 | `	ph7_result_string(pCtx,zBase,nBase);` |
|    33 | 10309 | `	return PH7_OK;` |
|    17 | 10310 | `}` |
|     - | 10311 | `/* php's getExtension(): everything after the LAST dot of the basename, and the` |
|     - | 10312 | ` * empty string when there is none -- a leading dot counts, so '.hidden' has the` |
|     - | 10313 | ` * extension 'hidden'. */` |
|    28 | 10314 | `static int vm_builtin_SplFileInfo_getExtension(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10315 | `{` |
|     - | 10316 | `	sxi32 rcChk;` |
|    29 | 10317 | `	int nTail = 0,nBase = 0,i;` |
|    29 | 10318 | `	const char *zTail = SfiTail(pCtx->pVm,PH7_ContextThis(pCtx),&nTail);` |
|    29 | 10319 | `	const char *zBase = PH7_ExtractBaseName(zTail,nTail,&nBase);` |
|    14 | 10320 | `	SXUNUSED(nArg);` |
|    14 | 10321 | `	SXUNUSED(apArg);` |
|    29 | 10322 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 10323 | `		return rcChk;` |
|     - | 10324 | `	}` |
|    89 | 10325 | `	for( i = nBase - 1 ; i >= 0 ; --i ){` |
|    77 | 10326 | `		if( zBase[i] == '.' ){` |
|    17 | 10327 | `			ph7_result_string(pCtx,&zBase[i+1],nBase - i - 1);` |
|    17 | 10328 | `			return PH7_OK;` |
|     - | 10329 | `		}` |
|    31 | 10330 | `	}` |
|    13 | 10331 | `	ph7_result_string(pCtx,"",0);` |
|    13 | 10332 | `	return PH7_OK;` |
|    15 | 10333 | `}` |
|    10 | 10334 | `static int vm_builtin_SplFileInfo_getPerms(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10335 | `{` |
|     5 | 10336 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    11 | 10337 | `	return SfiStatInt(pCtx,"getPerms","mode");` |
|     1 | 10338 | `}` |
|     4 | 10339 | `static int vm_builtin_SplFileInfo_getInode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10340 | `{` |
|     2 | 10341 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 | 10342 | `	return SfiStatInt(pCtx,"getInode","ino");` |
|     1 | 10343 | `}` |
|    24 | 10344 | `static int vm_builtin_SplFileInfo_getSize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10345 | `{` |
|    12 | 10346 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    25 | 10347 | `	return SfiStatInt(pCtx,"getSize","size");` |
|     1 | 10348 | `}` |
|     4 | 10349 | `static int vm_builtin_SplFileInfo_getOwner(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10350 | `{` |
|     2 | 10351 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 | 10352 | `	return SfiStatInt(pCtx,"getOwner","uid");` |
|     1 | 10353 | `}` |
|     4 | 10354 | `static int vm_builtin_SplFileInfo_getGroup(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10355 | `{` |
|     2 | 10356 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 | 10357 | `	return SfiStatInt(pCtx,"getGroup","gid");` |
|     1 | 10358 | `}` |
|     4 | 10359 | `static int vm_builtin_SplFileInfo_getATime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10360 | `{` |
|     2 | 10361 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 | 10362 | `	return SfiStatInt(pCtx,"getATime","atime");` |
|     1 | 10363 | `}` |
|    10 | 10364 | `static int vm_builtin_SplFileInfo_getMTime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10365 | `{` |
|     5 | 10366 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    11 | 10367 | `	return SfiStatInt(pCtx,"getMTime","mtime");` |
|     1 | 10368 | `}` |
|     4 | 10369 | `static int vm_builtin_SplFileInfo_getCTime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10370 | `{` |
|     2 | 10371 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 | 10372 | `	return SfiStatInt(pCtx,"getCTime","ctime");` |
|     1 | 10373 | `}` |
|     - | 10374 | `/*` |
|     - | 10375 | ` * php's getType() is FS_TYPE: an LSTAT, so a symlink answers "link" rather than` |
|     - | 10376 | ` * what it points at. The VFS's own xFiletype IS that question -- decoding a stat` |
|     - | 10377 | ` * mode here instead would have answered "unknown" on Windows, where the mode` |
|     - | 10378 | ` * field is not filled and the attributes are what carry the answer.` |
|     - | 10379 | ` */` |
|    14 | 10380 | `static int vm_builtin_SplFileInfo_getType(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10381 | `{` |
|    15 | 10382 | `	const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;` |
|    15 | 10383 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 10384 | `	char zPath[4096];` |
|    15 | 10385 | `	int rc = -1;` |
|     - | 10386 | `	sxi32 rcReady;` |
|     7 | 10387 | `	SXUNUSED(nArg);` |
|     7 | 10388 | `	SXUNUSED(apArg);` |
|    15 | 10389 | `	if( !SfiDirReady(pCtx,&rcReady) ){` |
|     3 | 10390 | `		return rcReady;` |
|     - | 10391 | `	}` |
|    13 | 10392 | `	if( SfiPathBuf(pCtx->pVm,pThis,zPath,(int)sizeof(zPath)) == SXRET_OK ){` |
|     - | 10393 | `		ph7_int64 aVal[13];` |
|    13 | 10394 | `		int rcU = PH7_VfsUserStatFields(pCtx,zPath,PH7_STAT_ASK_TYPE,aVal);` |
|    13 | 10395 | `		if( PH7_CALLBACK_UNWOUND(rcU) ){` |
|   ! 0 | 10396 | `			return rcU;` |
|     - | 10397 | `		}` |
|    13 | 10398 | `		if( rcU != PHL_URLSTAT_NOWRAP ){` |
|     7 | 10399 | `			if( rcU == PHL_URLSTAT_OK ){` |
|     7 | 10400 | `				PH7_VfsUserStatResult(pCtx,PH7_STAT_ASK_TYPE,aVal);` |
|     7 | 10401 | `				rc = PH7_OK;` |
|     4 | 10402 | `			}` |
|    10 | 10403 | `		}else if( pVfs && pVfs->xFiletype ){` |
|     7 | 10404 | `			rc = pVfs->xFiletype(zPath,pCtx);` |
|     3 | 10405 | `		}` |
|     6 | 10406 | `	}` |
|    13 | 10407 | `	if( rc != PH7_OK ){` |
|     3 | 10408 | `		int nName = 0;` |
|     3 | 10409 | `		const char *zName = SfiName(pCtx->pVm,pThis,&nName);` |
|     3 | 10410 | `		if( pCtx->pRet ){` |
|     3 | 10411 | `			PH7_MemObjRelease(pCtx->pRet);   /* xFiletype wrote "unknown" (rule 54) */` |
|     1 | 10412 | `		}` |
|     4 | 10413 | `		return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     1 | 10414 | `			"SplFileInfo::getType(): Lstat failed for %.*s",nName,zName);` |
|     - | 10415 | `	}` |
|    11 | 10416 | `	return PH7_OK;` |
|     8 | 10417 | `}` |
|     - | 10418 | `/* The six predicates: a VFS question each, and never a diagnostic -- php answers` |
|     - | 10419 | ` * false for a path that does not exist. */` |
|    57 | 10420 | `static int SfiPredicate(ph7_context *pCtx,int (*xTest)(const char *),int eAsk)` |
|     1 | 10421 | `{` |
|     - | 10422 | `	char zPath[4096];` |
|    58 | 10423 | `	int bYes = 0;` |
|     - | 10424 | `	sxi32 rcReady;` |
|    58 | 10425 | `	if( !SfiDirReady(pCtx,&rcReady) ){` |
|     3 | 10426 | `		return rcReady;` |
|     - | 10427 | `	}` |
|    56 | 10428 | `	if( SfiPathBuf(pCtx->pVm,PH7_ContextThis(pCtx),zPath,(int)sizeof(zPath)) == SXRET_OK ){` |
|     - | 10429 | `		/* Same door as the free functions: a wrapper's record answers the six,` |
|     - | 10430 | `		 * and a miss is the plain false php answers (these asks are QUIET). */` |
|     - | 10431 | `		ph7_int64 aVal[13];` |
|    56 | 10432 | `		int rcU = PH7_VfsUserStatFields(pCtx,zPath,eAsk,aVal);` |
|    56 | 10433 | `		if( PH7_CALLBACK_UNWOUND(rcU) ){` |
|    15 | 10434 | `			return rcU;` |
|     - | 10435 | `		}` |
|    54 | 10436 | `		if( rcU != PHL_URLSTAT_NOWRAP ){` |
|    25 | 10437 | `			if( rcU == PHL_URLSTAT_OK ){` |
|    25 | 10438 | `				PH7_VfsUserStatResult(pCtx,eAsk,aVal);` |
|    13 | 10439 | `			}else{` |
|   ! 0 | 10440 | `				ph7_result_bool(pCtx,0);` |
|     - | 10441 | `			}` |
|    25 | 10442 | `			return PH7_OK;` |
|     - | 10443 | `		}` |
|    30 | 10444 | `		if( xTest ){` |
|    30 | 10445 | `			bYes = xTest(zPath) == PH7_OK;` |
|    15 | 10446 | `		}` |
|    15 | 10447 | `	}` |
|    30 | 10448 | `	ph7_result_bool(pCtx,bYes);` |
|    30 | 10449 | `	return PH7_OK;` |
|    30 | 10450 | `}` |
|     4 | 10451 | `static int vm_builtin_SplFileInfo_isWritable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10452 | `{` |
|     2 | 10453 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 | 10454 | `	return SfiPredicate(pCtx,pCtx->pVm->pEngine->pVfs ? pCtx->pVm->pEngine->pVfs->xWritable : 0,PH7_STAT_ASK_IS_W);` |
|     1 | 10455 | `}` |
|    10 | 10456 | `static int vm_builtin_SplFileInfo_isReadable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10457 | `{` |
|     5 | 10458 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    11 | 10459 | `	return SfiPredicate(pCtx,pCtx->pVm->pEngine->pVfs ? pCtx->pVm->pEngine->pVfs->xReadable : 0,PH7_STAT_ASK_IS_R);` |
|     1 | 10460 | `}` |
|     2 | 10461 | `static int vm_builtin_SplFileInfo_isExecutable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10462 | `{` |
|     1 | 10463 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     3 | 10464 | `	return SfiPredicate(pCtx,pCtx->pVm->pEngine->pVfs ? pCtx->pVm->pEngine->pVfs->xExecutable : 0,PH7_STAT_ASK_IS_X);` |
|     1 | 10465 | `}` |
|    21 | 10466 | `static int vm_builtin_SplFileInfo_isFile(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10467 | `{` |
|    11 | 10468 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    22 | 10469 | `	return SfiPredicate(pCtx,pCtx->pVm->pEngine->pVfs ? pCtx->pVm->pEngine->pVfs->xIsfile : 0,PH7_STAT_ASK_IS_FILE);` |
|     1 | 10470 | `}` |
|    10 | 10471 | `static int vm_builtin_SplFileInfo_isDir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10472 | `{` |
|     5 | 10473 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    11 | 10474 | `	return SfiPredicate(pCtx,pCtx->pVm->pEngine->pVfs ? pCtx->pVm->pEngine->pVfs->xIsdir : 0,PH7_STAT_ASK_IS_DIR);` |
|     1 | 10475 | `}` |
|    10 | 10476 | `static int vm_builtin_SplFileInfo_isLink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10477 | `{` |
|     5 | 10478 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    11 | 10479 | `	return SfiPredicate(pCtx,pCtx->pVm->pEngine->pVfs ? pCtx->pVm->pEngine->pVfs->xIslink : 0,PH7_STAT_ASK_IS_LINK);` |
|     1 | 10480 | `}` |
|     - | 10481 | `/* php's getLinkTarget(): readlink(), and a RuntimeException naming the errno text` |
|     - | 10482 | ` * when it fails -- which includes asking a plain file for its target. */` |
|     2 | 10483 | `static int vm_builtin_SplFileInfo_getLinkTarget(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10484 | `{` |
|     3 | 10485 | `	const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;` |
|     3 | 10486 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 10487 | `	char zPath[4096];` |
|     3 | 10488 | `	int rc = -1;` |
|     - | 10489 | `	sxi32 rcReady;` |
|     1 | 10490 | `	SXUNUSED(nArg);` |
|     1 | 10491 | `	SXUNUSED(apArg);` |
|     3 | 10492 | `	if( !SfiDirReady(pCtx,&rcReady) ){` |
|   ! 0 | 10493 | `		return rcReady;` |
|     - | 10494 | `	}` |
|     2 | 10495 | `	if( pVfs && pVfs->xReadlink` |
|     3 | 10496 | `	 && SfiPathBuf(pCtx->pVm,pThis,zPath,(int)sizeof(zPath)) == SXRET_OK ){` |
|     3 | 10497 | `		rc = pVfs->xReadlink(zPath,pCtx);` |
|     1 | 10498 | `	}` |
|     3 | 10499 | `	if( rc != PH7_OK ){` |
|     2 | 10500 | `		int nName = 0;` |
|     2 | 10501 | `		const char *zName = SfiName(pCtx->pVm,pThis,&nName);` |
|     3 | 10502 | `		return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     2 | 10503 | `			"Unable to read link %.*s, error: %s",nName,zName,VfsStrerror(errno));` |
|     - | 10504 | `	}` |
|     1 | 10505 | `	return PH7_OK;` |
|     2 | 10506 | `}` |
|     6 | 10507 | `static int vm_builtin_SplFileInfo_getRealPath(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10508 | `{` |
|     - | 10509 | `	sxi32 rcChk;` |
|     7 | 10510 | `	const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;` |
|     - | 10511 | `	char zPath[4096];` |
|     7 | 10512 | `	int rc = -1;` |
|     3 | 10513 | `	SXUNUSED(nArg);` |
|     3 | 10514 | `	SXUNUSED(apArg);` |
|     7 | 10515 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 10516 | `		return rcChk;` |
|     - | 10517 | `	}` |
|     6 | 10518 | `	if( pVfs && pVfs->xRealpath` |
|     7 | 10519 | `	 && SfiPathBuf(pCtx->pVm,PH7_ContextThis(pCtx),zPath,(int)sizeof(zPath)) == SXRET_OK ){` |
|     7 | 10520 | `		rc = pVfs->xRealpath(zPath,pCtx);` |
|     3 | 10521 | `	}` |
|     7 | 10522 | `	if( rc != PH7_OK ){` |
|     5 | 10523 | `		ph7_result_bool(pCtx,0);   /* php answers false, with no diagnostic */` |
|     2 | 10524 | `	}` |
|     7 | 10525 | `	return PH7_OK;` |
|     4 | 10526 | `}` |
|     - | 10527 | `/*` |
|     - | 10528 | ` * The class getFileInfo()/getPathInfo() build with: the argument when it names` |
|     - | 10529 | ` * one, this instance's info_class otherwise. php refuses anything not derived` |
|     - | 10530 | ` * from SplFileInfo, and words the refusal from the ARGUMENT position.` |
|     - | 10531 | ` *` |
|     - | 10532 | ` * The two do not word it identically, and the split is php's: getPathInfo()` |
|     - | 10533 | ` * takes the argument through zend's CLASS-NAME parameter, which reports a name` |
|     - | 10534 | `` * NOTHING declares as `must be a valid class name or null` and leaves the`` |
|     - | 10535 | ` * derived-from check to the SPL code behind it, while getFileInfo() reports` |
|     - | 10536 | ` * both failures with the derived-from sentence. bValidFirst says which of the` |
|     - | 10537 | ` * two this door is.` |
|     - | 10538 | ` */` |
|   118 | 10539 | `static sxi32 SfiInfoClass(ph7_context *pCtx,const char *zMethod,ph7_value *pArg,` |
|     - | 10540 | `	int bValidFirst,ph7_class **ppOut)` |
|     2 | 10541 | `{` |
|   120 | 10542 | `	ph7_vm *pVm = pCtx->pVm;` |
|   120 | 10543 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   120 | 10544 | `	ph7_class *pBase = PH7_VmExtractClass(pVm,"SplFileInfo",sizeof("SplFileInfo")-1,FALSE,0);` |
|   120 | 10545 | `	ph7_class *pClass = 0;` |
|     - | 10546 | `	const char *zName;` |
|   120 | 10547 | `	int nName = 0;` |
|   126 | 10548 | `	if( pArg && (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|    35 | 10549 | `		sxi32 rcSv = PH7_ValueToStringUV(pCtx,pArg,&zName,&nName);` |
|    35 | 10550 | `		if( rcSv != SXRET_OK ){` |
|     5 | 10551 | `			return rcSv;` |
|     - | 10552 | `		}` |
|    31 | 10553 | `		pClass = PH7_VmExtractClass(pVm,zName,(sxu32)nName,TRUE,0);` |
|    31 | 10554 | `		if( pClass == 0 && bValidFirst ){` |
|    10 | 10555 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 10556 | `				"SplFileInfo::%s(): Argument #1 ($class) must be a valid class name "` |
|     3 | 10557 | `				"or null, %.*s given",zMethod,nName,zName);` |
|     - | 10558 | `		}` |
|    25 | 10559 | `		if( pClass == 0 \|\| pBase == 0 \|\| !PH7_VmInstanceOf(pClass,pBase) ){` |
|    19 | 10560 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 10561 | `				"SplFileInfo::%s(): Argument #1 ($class) must be a class name derived "` |
|     6 | 10562 | `				"from SplFileInfo or null, %.*s given",zMethod,nName,zName);` |
|     - | 10563 | `		}` |
|     7 | 10564 | `	}else{` |
|    86 | 10565 | `		int nCur = 0;` |
|    86 | 10566 | `		const char *zCur = SfiStr(pThis,SFI_IC,&nCur);` |
|    86 | 10567 | `		pClass = PH7_VmExtractClass(pVm,zCur,(sxu32)nCur,TRUE,0);` |
|     - | 10568 | `	}` |
|    98 | 10569 | `	if( pClass == 0 ){` |
|   ! 0 | 10570 | `		pClass = pBase;` |
|   ! 0 | 10571 | `	}` |
|    98 | 10572 | `	*ppOut = pClass;` |
|    98 | 10573 | `	return pClass ? PH7_OK : PH7_ContextMemoryError(pCtx);` |
|    61 | 10574 | `}` |
|     - | 10575 | `/*` |
|     - | 10576 | ` * Build one of these for a path. php calls the CONSTRUCTOR when the class` |
|     - | 10577 | ` * declares its own (a subclass may want it) and fills the slots directly when it` |
|     - | 10578 | ` * does not -- reproduced here, because a subclass constructor is user code and` |
|     - | 10579 | ` * skipping it would be visible.` |
|     - | 10580 | ` */` |
|    88 | 10581 | `static sxi32 SfiMakeInfoEx(ph7_context *pCtx,ph7_class *pClass,const char *zPath,int nPath,` |
|     - | 10582 | `	const char *zDir,int nDir)` |
|     2 | 10583 | `{` |
|    90 | 10584 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 10585 | `	ph7_class_instance *pNew;` |
|     - | 10586 | `	ph7_class_method *pCons;` |
|    90 | 10587 | `	sxi32 rc = SXRET_OK;` |
|    90 | 10588 | `	pNew = PH7_NewClassInstance(pVm,pClass);` |
|    90 | 10589 | `	if( pNew == 0 ){` |
|   ! 0 | 10590 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 10591 | `	}` |
|    90 | 10592 | `	pNew->iRef++;` |
|    90 | 10593 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|    91 | 10594 | `	if( pCons && (pCons->sFunc.iFlags & VM_FUNC_NATIVE) == 0 ){` |
|     - | 10595 | `		ph7_value sArg,*apArg[1];` |
|     3 | 10596 | `		PH7_MemObjInitFromString(pVm,&sArg,0);` |
|     3 | 10597 | `		PH7_MemObjStringAppend(&sArg,zPath,(sxu32)nPath);` |
|     3 | 10598 | `		apArg[0] = &sArg;` |
|     3 | 10599 | `		rc = PH7_VmCallClassMethod(pVm,pNew,pCons,0,1,apArg);` |
|     3 | 10600 | `		PH7_MemObjRelease(&sArg);` |
|    89 | 10601 | `	}else if( zDir ){` |
|     - | 10602 | `		/* php's create_type for a DIR source hands the child BOTH strings rather` |
|     - | 10603 | `		 * than re-deriving the second: the path is the directory being walked, so` |
|     - | 10604 | ``		 * `new DirectoryIterator('/')`'s entry keeps the path `/` and the name`` |
|     - | 10605 | ``		 * `//x` that the walk itself produced. */`` |
|    68 | 10606 | `		PH7_NativeSetAttrStr(pVm,pNew,SFI_N,zPath,nPath);` |
|    68 | 10607 | `		PH7_NativeSetAttrStr(pVm,pNew,SFI_P,zDir,nDir);` |
|    35 | 10608 | `	}else{` |
|    21 | 10609 | `		SfiSetName(pVm,pNew,zPath,nPath);` |
|     - | 10610 | `	}` |
|    90 | 10611 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 10612 | `		PH7_ClassInstanceUnref(pNew);` |
|   ! 0 | 10613 | `		return rc;` |
|     - | 10614 | `	}` |
|    90 | 10615 | `	PH7_NativeResultObject(pCtx,pNew);` |
|    90 | 10616 | `	PH7_ClassInstanceUnref(pNew);` |
|    90 | 10617 | `	return PH7_OK;` |
|    46 | 10618 | `}` |
|     8 | 10619 | `static sxi32 SfiMakeInfo(ph7_context *pCtx,ph7_class *pClass,const char *zPath,int nPath)` |
|     1 | 10620 | `{` |
|     9 | 10621 | `	return SfiMakeInfoEx(pCtx,pClass,zPath,nPath,0,0);` |
|     1 | 10622 | `}` |
|    34 | 10623 | `static int vm_builtin_SplFileInfo_getFileInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10624 | `{` |
|     - | 10625 | `	sxi32 rcChk,rc;` |
|    35 | 10626 | `	ph7_vm *pVm = pCtx->pVm;` |
|    35 | 10627 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    35 | 10628 | `	ph7_class *pClass = 0;` |
|    35 | 10629 | `	int nName = 0,nDir = 0;` |
|    35 | 10630 | `	const char *zName,*zDir = 0;` |
|    35 | 10631 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 10632 | `		return rcChk;` |
|     - | 10633 | `	}` |
|    35 | 10634 | `	rc = SfiInfoClass(pCtx,"getFileInfo",nArg > 0 ? apArg[0] : 0,0,&pClass);` |
|    35 | 10635 | `	if( rc != PH7_OK ){` |
|    13 | 10636 | `		return rc;` |
|     - | 10637 | `	}` |
|    23 | 10638 | `	if( SplDirIs(pVm,pThis) ){` |
|     9 | 10639 | `		if( SplDirState(pVm,pThis) == 0 ){` |
|     3 | 10640 | `			return PH7_VmThrowException(pCtx,"Error","Object not initialized");` |
|     - | 10641 | `		}` |
|     - | 10642 | `		/* php's create_type refuses to describe an entry that is not there —` |
|     - | 10643 | `		 * the same RuntimeException a FilesystemIterator::current() past the end` |
|     - | 10644 | `		 * raises, because it goes through this. */` |
|     7 | 10645 | `		if( SplDirAtEnd(pThis) ){` |
|     3 | 10646 | `			return PH7_VmThrowException(pCtx,"RuntimeException","Could not open file");` |
|     - | 10647 | `		}` |
|     5 | 10648 | `		zDir = SfiStr(pThis,SFI_P,&nDir);` |
|     2 | 10649 | `	}` |
|    19 | 10650 | `	zName = SfiName(pVm,pThis,&nName);` |
|    19 | 10651 | `	return SfiMakeInfoEx(pCtx,pClass,zName,nName,zDir,nDir);` |
|    18 | 10652 | `}` |
|     - | 10653 | `/* php's getPathInfo(): the DIRNAME of the pathname, and nothing at all (null) for` |
|     - | 10654 | ` * an empty one — which for a directory iterator includes one that has run out. */` |
|    22 | 10655 | `static int vm_builtin_SplFileInfo_getPathInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10656 | `{` |
|     - | 10657 | `	sxi32 rcChk,rc;` |
|    23 | 10658 | `	ph7_vm *pVm = pCtx->pVm;` |
|    23 | 10659 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    23 | 10660 | `	ph7_class *pClass = 0;` |
|    23 | 10661 | `	int nName = 0,nDir = 0;` |
|     - | 10662 | `	const char *zName,*zDir;` |
|    23 | 10663 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 10664 | `		return rcChk;` |
|     - | 10665 | `	}` |
|    23 | 10666 | `	rc = SfiInfoClass(pCtx,"getPathInfo",nArg > 0 ? apArg[0] : 0,1,&pClass);` |
|    23 | 10667 | `	if( rc != PH7_OK ){` |
|    11 | 10668 | `		return rc;` |
|     - | 10669 | `	}` |
|    13 | 10670 | `	if( SplDirIs(pVm,pThis) && SplDirAtEnd(pThis) ){` |
|     3 | 10671 | `		ph7_result_null(pCtx);` |
|     3 | 10672 | `		return PH7_OK;` |
|     - | 10673 | `	}` |
|    11 | 10674 | `	zName = SfiName(pVm,pThis,&nName);` |
|    11 | 10675 | `	if( nName < 1 ){` |
|     3 | 10676 | `		ph7_result_null(pCtx);` |
|     3 | 10677 | `		return PH7_OK;` |
|     - | 10678 | `	}` |
|     9 | 10679 | `	zDir = PH7_ExtractDirName(zName,nName,&nDir);` |
|     9 | 10680 | `	return SfiMakeInfo(pCtx,pClass,zDir,nDir);` |
|    12 | 10681 | `}` |
|    20 | 10682 | `static int vm_builtin_SplFileInfo_setInfoClass(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10683 | `{` |
|     - | 10684 | `	sxi32 rcChk;` |
|    21 | 10685 | `	ph7_vm *pVm = pCtx->pVm;` |
|    21 | 10686 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    21 | 10687 | `	ph7_class *pBase = PH7_VmExtractClass(pVm,"SplFileInfo",sizeof("SplFileInfo")-1,FALSE,0);` |
|     - | 10688 | `	ph7_class *pClass;` |
|    21 | 10689 | `	const char *zName = "SplFileInfo";` |
|    21 | 10690 | `	int nName = (int)sizeof("SplFileInfo")-1;` |
|    21 | 10691 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 10692 | `		return rcChk;` |
|     - | 10693 | `	}` |
|    21 | 10694 | `	if( nArg > 0 ){` |
|     - | 10695 | `		/* php's cast here is the USER-VISIBLE one: an array warns` |
|     - | 10696 | ``		 * `Array to string conversion` and is refused as the name "Array", and an`` |
|     - | 10697 | `		 * object with no __toString() is the catchable` |
|     - | 10698 | ``		 * `Object of class X could not be converted to string` rather than a`` |
|     - | 10699 | `		 * refusal naming the placeholder "Object". */` |
|    21 | 10700 | `		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&zName,&nName);` |
|    21 | 10701 | `		if( rcSv != SXRET_OK ){` |
|     3 | 10702 | `			return rcSv;` |
|     - | 10703 | `		}` |
|     9 | 10704 | `	}` |
|    19 | 10705 | `	pClass = PH7_VmExtractClass(pVm,zName,(sxu32)nName,TRUE,0);` |
|    19 | 10706 | `	if( pClass == 0 \|\| pBase == 0 \|\| !PH7_VmInstanceOf(pClass,pBase) ){` |
|     - | 10707 | `		/* php words this one WITHOUT the "or null" half getFileInfo() has: the` |
|     - | 10708 | `		 * parameter is not nullable here. */` |
|    19 | 10709 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 10710 | `			"SplFileInfo::setInfoClass(): Argument #1 ($class) must be a class name "` |
|     6 | 10711 | `			"derived from SplFileInfo, %.*s given",nName,zName);` |
|     - | 10712 | `	}` |
|     7 | 10713 | `	PH7_NativeSetAttrStr(pVm,pThis,SFI_IC,zName,nName);` |
|     7 | 10714 | `	return PH7_OK;` |
|    11 | 10715 | `}` |
|     - | 10716 | `/*` |
|     - | 10717 | ` * php's setFileClass(): the class openFile() will build. Worded WITHOUT the` |
|     - | 10718 | ` * "or null" half getFileInfo() has, like its info_class twin -- the parameter` |
|     - | 10719 | ` * is not nullable, and a null coerces to the empty name the refusal prints.` |
|     - | 10720 | ` */` |
|    32 | 10721 | `static int vm_builtin_SplFileInfo_setFileClass(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10722 | `{` |
|     - | 10723 | `	sxi32 rcChk;` |
|    33 | 10724 | `	ph7_vm *pVm = pCtx->pVm;` |
|    33 | 10725 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    33 | 10726 | `	ph7_class *pBase = PH7_VmExtractClass(pVm,"SplFileObject",sizeof("SplFileObject")-1,FALSE,0);` |
|     - | 10727 | `	ph7_class *pClass;` |
|    33 | 10728 | `	const char *zName = "SplFileObject";` |
|    33 | 10729 | `	int nName = (int)sizeof("SplFileObject")-1;` |
|    33 | 10730 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 10731 | `		return rcChk;` |
|     - | 10732 | `	}` |
|    33 | 10733 | `	if( nArg > 0 ){` |
|    31 | 10734 | `		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&zName,&nName);` |
|    31 | 10735 | `		if( rcSv != SXRET_OK ){` |
|     3 | 10736 | `			return rcSv;` |
|     - | 10737 | `		}` |
|    14 | 10738 | `	}` |
|    31 | 10739 | `	pClass = PH7_VmExtractClass(pVm,zName,(sxu32)nName,TRUE,0);` |
|    31 | 10740 | `	if( pClass == 0 \|\| pBase == 0 \|\| !PH7_VmInstanceOf(pClass,pBase) ){` |
|    25 | 10741 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 10742 | `			"SplFileInfo::setFileClass(): Argument #1 ($class) must be a class name "` |
|     8 | 10743 | `			"derived from SplFileObject, %.*s given",nName,zName);` |
|     - | 10744 | `	}` |
|    15 | 10745 | `	PH7_NativeSetAttrStr(pVm,pThis,SFI_FC,zName,nName);` |
|    15 | 10746 | `	return PH7_OK;` |
|    17 | 10747 | `}` |
|     - | 10748 | `/*` |
|     - | 10749 | ` * php's openFile(): spl_filesystem_object_create_type for SPL_FS_FILE. The` |
|     - | 10750 | ` * class is this instance's file_class, and php CALLS its constructor when the` |
|     - | 10751 | ` * class declares one of its own -- with the pathname and the MODE, two` |
|     - | 10752 | ` * arguments, which is how a subclass gets to see what it was opened as. When it` |
|     - | 10753 | ` * does not, php fills the slots and opens directly, which is what SfoOpen()` |
|     - | 10754 | ` * does here; the warning that open would print is promoted to a` |
|     - | 10755 | ` * RuntimeException worded from THIS method's name.` |
|     - | 10756 | ` */` |
|    30 | 10757 | `static int vm_builtin_SplFileInfo_openFile(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10758 | `{` |
|     - | 10759 | `	sxi32 rcChk,rc;` |
|    31 | 10760 | `	ph7_vm *pVm = pCtx->pVm;` |
|    31 | 10761 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 10762 | `	ph7_class_instance *pNew;` |
|     - | 10763 | `	ph7_class_method *pCons;` |
|     - | 10764 | `	ph7_class *pClass;` |
|     - | 10765 | `	ph7_value sPath;` |
|     - | 10766 | `	SyBlob sCls,sDir;` |
|    31 | 10767 | `	const char *zMode = "r",*zName,*zDir,*zCls;` |
|    31 | 10768 | `	int nMode = 1,nName = 0,nDir = 0,nCls = 0;` |
|    31 | 10769 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 10770 | `		return rcChk;` |
|     - | 10771 | `	}` |
|    31 | 10772 | `	if( SplDirIs(pVm,pThis) ){` |
|     5 | 10773 | `		if( SplDirState(pVm,pThis) == 0 ){` |
|   ! 0 | 10774 | `			return PH7_VmThrowException(pCtx,"Error","Object not initialized");` |
|     - | 10775 | `		}` |
|     - | 10776 | `		/* php's create_type refuses to describe an entry that is not there. */` |
|     5 | 10777 | `		if( SplDirAtEnd(pThis) ){` |
|     3 | 10778 | `			return PH7_VmThrowException(pCtx,"RuntimeException","Could not open file");` |
|     - | 10779 | `		}` |
|     1 | 10780 | `	}` |
|     - | 10781 | `	/* Both strings are copied OUT before anything allocates or runs user code:` |
|     - | 10782 | `	 * they are slots of the object being read, and a subclass constructor below` |
|     - | 10783 | `	 * can rewrite or unset either one (rule 22). */` |
|    29 | 10784 | `	zCls = SfiStr(pThis,SFI_FC,&nCls);` |
|    29 | 10785 | `	SyBlobInit(&sCls,&pVm->sAllocator);` |
|    29 | 10786 | `	SyBlobAppend(&sCls,zCls,(sxu32)nCls);` |
|    29 | 10787 | `	zDir = SfiStr(pThis,SFI_P,&nDir);` |
|    29 | 10788 | `	SyBlobInit(&sDir,&pVm->sAllocator);` |
|    29 | 10789 | `	SyBlobAppend(&sDir,zDir,(sxu32)nDir);` |
|    43 | 10790 | `	pClass = PH7_VmExtractClass(pVm,(const char *)SyBlobData(&sCls),` |
|    14 | 10791 | `		SyBlobLength(&sCls),TRUE,0);` |
|    29 | 10792 | `	if( pClass == 0 ){` |
|   ! 0 | 10793 | `		pClass = PH7_VmExtractClass(pVm,"SplFileObject",sizeof("SplFileObject")-1,FALSE,0);` |
|   ! 0 | 10794 | `	}` |
|    29 | 10795 | `	if( pClass == 0 ){` |
|   ! 0 | 10796 | `		SyBlobRelease(&sCls);` |
|   ! 0 | 10797 | `		SyBlobRelease(&sDir);` |
|   ! 0 | 10798 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 10799 | `	}` |
|    29 | 10800 | `	if( nArg > 0 ){` |
|    11 | 10801 | `		zMode = ph7_value_to_string(apArg[0],&nMode);` |
|     5 | 10802 | `	}` |
|     - | 10803 | `	/* The path is handed on as a VALUE: the opener needs one, and the slot it` |
|     - | 10804 | `	 * would otherwise borrow belongs to an object about to be written to. */` |
|    29 | 10805 | `	zName = SfiName(pVm,pThis,&nName);` |
|    29 | 10806 | `	PH7_MemObjInitFromString(pVm,&sPath,0);` |
|    29 | 10807 | `	PH7_MemObjStringAppend(&sPath,zName,(sxu32)nName);` |
|    29 | 10808 | `	pNew = PH7_NewClassInstance(pVm,pClass);` |
|    29 | 10809 | `	if( pNew == 0 ){` |
|   ! 0 | 10810 | `		PH7_MemObjRelease(&sPath);` |
|   ! 0 | 10811 | `		SyBlobRelease(&sCls);` |
|   ! 0 | 10812 | `		SyBlobRelease(&sDir);` |
|   ! 0 | 10813 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 10814 | `	}` |
|    29 | 10815 | `	pNew->iRef++;` |
|    29 | 10816 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|    30 | 10817 | `	if( pCons && (pCons->sFunc.iFlags & VM_FUNC_NATIVE) == 0 ){` |
|     - | 10818 | `		ph7_value sMode,*apCtor[2];` |
|     3 | 10819 | `		PH7_MemObjInitFromString(pVm,&sMode,0);` |
|     3 | 10820 | `		PH7_MemObjStringAppend(&sMode,zMode,(sxu32)nMode);` |
|     3 | 10821 | `		apCtor[0] = &sPath;` |
|     3 | 10822 | `		apCtor[1] = &sMode;` |
|     3 | 10823 | `		rc = PH7_VmCallClassMethod(pVm,pNew,pCons,0,2,apCtor);` |
|     3 | 10824 | `		PH7_MemObjRelease(&sMode);` |
|     2 | 10825 | `	}else{` |
|    44 | 10826 | `		rc = SfoOpen(pCtx,pNew,&sPath,zMode,nMode,` |
|    15 | 10827 | `			nArg > 1 ? ph7_value_to_bool(apArg[1]) : FALSE,` |
|    15 | 10828 | `			nArg > 2 && (apArg[2]->iFlags & MEMOBJ_NULL) == 0 ? apArg[2] : 0,3);` |
|    27 | 10829 | `		if( rc == PH7_OK ){` |
|     - | 10830 | `			/* php hands the child the SOURCE's path rather than re-deriving one` |
|     - | 10831 | `			 * from the name: a directory entry's getPath() keeps pointing at the` |
|     - | 10832 | ``			 * directory being walked, and a `php://temp` source keeps the EMPTY`` |
|     - | 10833 | ``			 * path a URI's last slash would otherwise cut to `php:/`. */`` |
|    31 | 10834 | `			PH7_NativeSetAttrStr(pVm,pNew,SFI_P,` |
|    20 | 10835 | `				(const char *)SyBlobData(&sDir),(int)SyBlobLength(&sDir));` |
|    10 | 10836 | `		}` |
|     - | 10837 | `	}` |
|    29 | 10838 | `	PH7_MemObjRelease(&sPath);` |
|    29 | 10839 | `	SyBlobRelease(&sCls);` |
|    29 | 10840 | `	SyBlobRelease(&sDir);` |
|    29 | 10841 | `	if( rc != PH7_OK ){` |
|     7 | 10842 | `		PH7_ClassInstanceUnref(pNew);` |
|     7 | 10843 | `		return rc;` |
|     - | 10844 | `	}` |
|    23 | 10845 | `	PH7_NativeResultObject(pCtx,pNew);` |
|    23 | 10846 | `	PH7_ClassInstanceUnref(pNew);` |
|    23 | 10847 | `	return PH7_OK;` |
|    16 | 10848 | `}` |
|     - | 10849 | ``/* One `"\0Class\0member" => <string>` entry of a debug array. */`` |
|    72 | 10850 | `static void SfiDebugStr(ph7_vm *pVm,ph7_value *pOut,const char *zKey,int nKey,` |
|     - | 10851 | `	const char *zVal,int nVal)` |
|     1 | 10852 | `{` |
|     - | 10853 | `	ph7_value sKey,sVal;` |
|    73 | 10854 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|    73 | 10855 | `	PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKey);` |
|    73 | 10856 | `	PH7_MemObjInitFromString(pVm,&sVal,0);` |
|    73 | 10857 | `	PH7_MemObjStringAppend(&sVal,zVal,(sxu32)nVal);` |
|    73 | 10858 | `	ph7_array_add_elem(pOut,&sKey,&sVal);` |
|    73 | 10859 | `	PH7_MemObjRelease(&sKey);` |
|    73 | 10860 | `	PH7_MemObjRelease(&sVal);` |
|    73 | 10861 | `}` |
|     - | 10862 | `/*` |
|     - | 10863 | ` * php's get_debug_info: the two slots under their MANGLED private names, which is` |
|     - | 10864 | ` * how a class with no declared properties still shows something. __debugInfo()` |
|     - | 10865 | ` * hands back the same array.` |
|     - | 10866 | ` *` |
|     - | 10867 | `` * A DIRECTORY iterator shows two more (`glob`, always false here — PHL has no`` |
|     - | 10868 | `` * GlobIterator — and `subPathName`), and shows `fileName` only if the pathname`` |
|     - | 10869 | `` * has been MATERIALIZED: php's `if (intern->file_name)` is the lazy name's`` |
|     - | 10870 | ` * presence, so an exhausted iterator has one key fewer until something asks it` |
|     - | 10871 | ` * for a path.` |
|     - | 10872 | ` */` |
|    24 | 10873 | `static sxi32 SfiFillDebug(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)` |
|     1 | 10874 | `{` |
|     - | 10875 | ``	/* php's test is `type == SPL_FS_DIR`, which an object whose constructor never`` |
|     - | 10876 | ``	 * ran does NOT satisfy: it shows the single `pathName` key a bare SplFileInfo`` |
|     - | 10877 | `	 * would, and neither of the two directory ones. */` |
|    25 | 10878 | `	int bDir = SplDirIs(pVm,pThis) && SplDirState(pVm,pThis) != 0;` |
|    25 | 10879 | `	int nName = 0,nTail = 0,nSub = 0;` |
|     - | 10880 | `	const char *zName;` |
|    25 | 10881 | `	int bLive = bDir ? !SplDirAtEnd(pThis) : !SplDirIs(pVm,pThis);` |
|    25 | 10882 | `	if( bLive ){` |
|    19 | 10883 | `		zName = SfiName(pVm,pThis,&nName);` |
|    10 | 10884 | `	}else{` |
|     7 | 10885 | `		zName = SfiStr(pThis,SFI_N,&nName);   /* whatever a stat left behind, or "" */` |
|     7 | 10886 | `		nName = 0;` |
|     - | 10887 | `	}` |
|    37 | 10888 | `	SfiDebugStr(pVm,pOut,"\0SplFileInfo\0pathName",` |
|    12 | 10889 | `		(int)sizeof("\0SplFileInfo\0pathName")-1,zName,nName);` |
|     - | 10890 | `	/* Re-read: the append above may have moved the slot the first read borrowed. */` |
|    25 | 10891 | `	SfiStr(pThis,SFI_N,&nName);` |
|    25 | 10892 | `	if( bLive \|\| (bDir && nName > 0) ){` |
|    19 | 10893 | `		const char *zTail = SfiTail(pVm,pThis,&nTail);` |
|    28 | 10894 | `		SfiDebugStr(pVm,pOut,"\0SplFileInfo\0fileName",` |
|     9 | 10895 | `			(int)sizeof("\0SplFileInfo\0fileName")-1,zTail,nTail);` |
|     9 | 10896 | `	}` |
|    25 | 10897 | `	if( bDir ){` |
|     - | 10898 | `		ph7_value sKey,sVal;` |
|     - | 10899 | `		const char *zSub;` |
|    13 | 10900 | `		int nSlot = 0;` |
|    13 | 10901 | `		const char *zSlot = SfiStr(pThis,SFI_P,&nSlot);` |
|    13 | 10902 | `		PH7_MemObjInitFromString(pVm,&sKey,0);` |
|    13 | 10903 | `		PH7_MemObjStringAppend(&sKey,"\0DirectoryIterator\0glob",` |
|     - | 10904 | `			sizeof("\0DirectoryIterator\0glob")-1);` |
|     - | 10905 | `		/* php's own test, on the slot rather than on the stream: the whole` |
|     - | 10906 | ``		 * `glob://pattern` when the path carries that prefix, and FALSE for an`` |
|     - | 10907 | `		 * ordinary directory. A GlobIterator's constructor puts the prefix on` |
|     - | 10908 | `		 * whether or not the caller wrote it, so this is always the pattern. */` |
|    12 | 10909 | `		if( nSlot >= (int)sizeof("glob://")-1` |
|    13 | 10910 | `		 && SyMemcmp(zSlot,"glob://",sizeof("glob://")-1) == 0 ){` |
|     7 | 10911 | `			PH7_MemObjInitFromString(pVm,&sVal,0);` |
|     7 | 10912 | `			PH7_MemObjStringAppend(&sVal,zSlot,(sxu32)nSlot);` |
|     4 | 10913 | `		}else{` |
|     7 | 10914 | `			PH7_MemObjInitFromBool(pVm,&sVal,0);` |
|     - | 10915 | `		}` |
|    13 | 10916 | `		ph7_array_add_elem(pOut,&sKey,&sVal);` |
|    13 | 10917 | `		PH7_MemObjRelease(&sKey);` |
|    13 | 10918 | `		PH7_MemObjRelease(&sVal);` |
|    13 | 10919 | `		zSub = SfiStr(pThis,SDI_S,&nSub);` |
|    19 | 10920 | `		SfiDebugStr(pVm,pOut,"\0RecursiveDirectoryIterator\0subPathName",` |
|     6 | 10921 | `			(int)sizeof("\0RecursiveDirectoryIterator\0subPathName")-1,zSub,nSub);` |
|     6 | 10922 | `	}` |
|    25 | 10923 | `	if( SfoDev(pThis) ){` |
|     - | 10924 | ``		/* php's `type == SPL_FS_FILE` arm: the open mode and the two CSV`` |
|     - | 10925 | `		 * characters, which is the only place any of the three is visible. An` |
|     - | 10926 | `		 * SplFileObject whose constructor never ran is still SPL_FS_INFO and` |
|     - | 10927 | `		 * shows none of them. */` |
|     7 | 10928 | `		int nMode = 0,c;` |
|     7 | 10929 | `		const char *zMode = SfiStr(pThis,SFO_M,&nMode);` |
|    10 | 10930 | `		SfiDebugStr(pVm,pOut,"\0SplFileObject\0openMode",` |
|     3 | 10931 | `			(int)sizeof("\0SplFileObject\0openMode")-1,zMode,nMode);` |
|     7 | 10932 | `		c = (int)PH7_NativeAttrInt(pThis,SFO_D);` |
|     - | 10933 | `		{` |
|     7 | 10934 | `			char zChar = (char)c;` |
|     7 | 10935 | `			SfiDebugStr(pVm,pOut,"\0SplFileObject\0delimiter",` |
|     - | 10936 | `				(int)sizeof("\0SplFileObject\0delimiter")-1,&zChar,1);` |
|     7 | 10937 | `			zChar = (char)PH7_NativeAttrInt(pThis,SFO_EN);` |
|     7 | 10938 | `			SfiDebugStr(pVm,pOut,"\0SplFileObject\0enclosure",` |
|     - | 10939 | `				(int)sizeof("\0SplFileObject\0enclosure")-1,&zChar,1);` |
|     - | 10940 | `		}` |
|     3 | 10941 | `	}` |
|    25 | 10942 | `	return PH7_OK;` |
|     1 | 10943 | `}` |
|    12 | 10944 | `static sxi32 SfiPresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|     1 | 10945 | `{` |
|    13 | 10946 | `	if( !bDebug ){` |
|     7 | 10947 | `		return PH7_OK;   /* php's (array) cast and var_export show nothing */` |
|     - | 10948 | `	}` |
|     7 | 10949 | `	return SfiFillDebug(pVm,pThis,pOut);` |
|     7 | 10950 | `}` |
|    18 | 10951 | `static int vm_builtin_SplFileInfo_debugInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10952 | `{` |
|     - | 10953 | `	sxi32 rcChk;` |
|    19 | 10954 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 10955 | `	ph7_value sOut;` |
|     9 | 10956 | `	SXUNUSED(nArg);` |
|     9 | 10957 | `	SXUNUSED(apArg);` |
|    19 | 10958 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 10959 | `		return rcChk;` |
|     - | 10960 | `	}` |
|    19 | 10961 | `	PH7_MemObjInit(pVm,&sOut);` |
|    19 | 10962 | `	if( PH7_MemObjToHashmap(&sOut) != SXRET_OK ){` |
|   ! 0 | 10963 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 | 10964 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 10965 | `	}` |
|    19 | 10966 | `	SfiFillDebug(pVm,PH7_ContextThis(pCtx),&sOut);` |
|    19 | 10967 | `	ph7_result_value(pCtx,&sOut);` |
|    19 | 10968 | `	PH7_MemObjRelease(&sOut);` |
|    19 | 10969 | `	return PH7_OK;` |
|    10 | 10970 | `}` |
|     - | 10971 | `/* php's own escape hatch for a subclass that forgot to call parent::__construct.` |
|     - | 10972 | ` * It exists to be THROWN, and php marks it deprecated rather than removing it. */` |
|     2 | 10973 | `static int vm_builtin_SplFileInfo_badState(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10974 | `{` |
|     1 | 10975 | `	SXUNUSED(nArg);` |
|     1 | 10976 | `	SXUNUSED(apArg);` |
|     3 | 10977 | `	return PH7_VmThrowException(pCtx,"Error",` |
|     - | 10978 | `		"The parent constructor was not called: the object is in an invalid state");` |
|     1 | 10979 | `}` |
|     - | 10980 | `/*` |
|     - | 10981 | ` * The declaration. Method ORDER is spl_directory.stub.php's; openFile() and` |
|     - | 10982 | ` * setFileClass() are absent because SplFileObject is (§7), and everything else is` |
|     - | 10983 | ` * php's, tentative return types included.` |
|     - | 10984 | ` */` |
|  6721 | 10985 | `static sxi32 VmInstallSplFileInfo(ph7_vm *pVm)` |
|     5 | 10986 | `{` |
|     - | 10987 | `	static const PH7_NativePropDef aSfiProp[] = {` |
|     - | 10988 | `		{ SFI_N,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|     - | 10989 | `		{ SFI_P,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|     - | 10990 | `		{ SFI_IC, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN,` |
|     - | 10991 | `		  { 0, 0, PH7_NATIVE_VAL_STRING, 0, "SplFileInfo", 0.0 }, 0 },` |
|     - | 10992 | `		{ SFI_FC, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN,` |
|     - | 10993 | `		  { 0, 0, PH7_NATIVE_VAL_STRING, 0, "SplFileObject", 0.0 }, 0 },` |
|     - | 10994 | `	};` |
|     - | 10995 | `	static const PH7_NativeMethodDef aSfiMethod[] = {` |
|     - | 10996 | `		{ "__construct",   PH7_MOD_PUBLIC, "string $filename", 0,` |
|     - | 10997 | `		  vm_builtin_SplFileInfo_construct },` |
|     - | 10998 | `		{ "getPath",       PH7_MOD_PUBLIC, "", "@string", vm_builtin_SplFileInfo_getPath },` |
|     - | 10999 | `		{ "getFilename",   PH7_MOD_PUBLIC, "", "@string", vm_builtin_SplFileInfo_getFilename },` |
|     - | 11000 | `		{ "getExtension",  PH7_MOD_PUBLIC, "", "@string", vm_builtin_SplFileInfo_getExtension },` |
|     - | 11001 | `		{ "getBasename",   PH7_MOD_PUBLIC, "string $suffix = \"\"", "@string",` |
|     - | 11002 | `		  vm_builtin_SplFileInfo_getBasename },` |
|     - | 11003 | `		{ "getPathname",   PH7_MOD_PUBLIC, "", "@string", vm_builtin_SplFileInfo_getPathname },` |
|     - | 11004 | `		{ "getPerms",      PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_SplFileInfo_getPerms },` |
|     - | 11005 | `		{ "getInode",      PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_SplFileInfo_getInode },` |
|     - | 11006 | `		{ "getSize",       PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_SplFileInfo_getSize },` |
|     - | 11007 | `		{ "getOwner",      PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_SplFileInfo_getOwner },` |
|     - | 11008 | `		{ "getGroup",      PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_SplFileInfo_getGroup },` |
|     - | 11009 | `		{ "getATime",      PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_SplFileInfo_getATime },` |
|     - | 11010 | `		{ "getMTime",      PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_SplFileInfo_getMTime },` |
|     - | 11011 | `		{ "getCTime",      PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_SplFileInfo_getCTime },` |
|     - | 11012 | `		{ "getType",       PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_SplFileInfo_getType },` |
|     - | 11013 | `		{ "isWritable",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplFileInfo_isWritable },` |
|     - | 11014 | `		{ "isReadable",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplFileInfo_isReadable },` |
|     - | 11015 | `		{ "isExecutable",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplFileInfo_isExecutable },` |
|     - | 11016 | `		{ "isFile",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplFileInfo_isFile },` |
|     - | 11017 | `		{ "isDir",         PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplFileInfo_isDir },` |
|     - | 11018 | `		{ "isLink",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplFileInfo_isLink },` |
|     - | 11019 | `		{ "getLinkTarget", PH7_MOD_PUBLIC, "", "@string\|false",` |
|     - | 11020 | `		  vm_builtin_SplFileInfo_getLinkTarget },` |
|     - | 11021 | `		{ "getRealPath",   PH7_MOD_PUBLIC, "", "@string\|false",` |
|     - | 11022 | `		  vm_builtin_SplFileInfo_getRealPath },` |
|     - | 11023 | `		{ "getFileInfo",   PH7_MOD_PUBLIC, "~?string $class = null", "@SplFileInfo",` |
|     - | 11024 | `		  vm_builtin_SplFileInfo_getFileInfo },` |
|     - | 11025 | `		{ "getPathInfo",   PH7_MOD_PUBLIC, "~?string $class = null", "@?SplFileInfo",` |
|     - | 11026 | `		  vm_builtin_SplFileInfo_getPathInfo },` |
|     - | 11027 | `		/* spl_directory.stub.php's order, which is what every method-enumeration` |
|     - | 11028 | `		 * surface reports: openFile, setFileClass, setInfoClass. */` |
|     - | 11029 | `		{ "openFile",      PH7_MOD_PUBLIC,` |
|     - | 11030 | `		  "string $mode = \"r\", bool $useIncludePath = false, $context = null",` |
|     - | 11031 | `		  "@SplFileObject", vm_builtin_SplFileInfo_openFile },` |
|     - | 11032 | `		{ "setFileClass",  PH7_MOD_PUBLIC, "~string $class = SplFileObject::class", "@void",` |
|     - | 11033 | `		  vm_builtin_SplFileInfo_setFileClass },` |
|     - | 11034 | `		{ "setInfoClass",  PH7_MOD_PUBLIC, "~string $class = SplFileInfo::class", "@void",` |
|     - | 11035 | `		  vm_builtin_SplFileInfo_setInfoClass },` |
|     - | 11036 | `		{ "__toString",    PH7_MOD_PUBLIC, "", "string", vm_builtin_SplFileInfo_getPathname },` |
|     - | 11037 | `		{ "__debugInfo",   PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplFileInfo_debugInfo },` |
|     - | 11038 | `		/* php does NOT mark this one tentative -- it is the only method here that` |
|     - | 11039 | ``		 * prints `Return [ void ]` rather than `Tentative return [ void ]`. */`` |
|     - | 11040 | `		{ "_bad_state_ex", PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "void",` |
|     - | 11041 | `		  vm_builtin_SplFileInfo_badState },` |
|     - | 11042 | `	};` |
|     - | 11043 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - | 11044 | `		{ "SplFileInfo", 0, "Stringable", PH7_CLASS_NOSERIALIZE,` |
|     - | 11045 | `		  aSfiMethod, SX_ARRAYSIZE(aSfiMethod), 0, 0,` |
|     - | 11046 | `		  aSfiProp, SX_ARRAYSIZE(aSfiProp), 0, 0, SfiPresent },` |
|     - | 11047 | `	};` |
|  6726 | 11048 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 | 11049 | `}` |
|     - | 11050 | `/*` |
|     - | 11051 | ` * ---------------------------------------------------------------------------` |
|     - | 11052 | ` * DirectoryIterator, FilesystemIterator and RecursiveDirectoryIterator.` |
|     - | 11053 | ` *` |
|     - | 11054 | `` * php's `spl_filesystem_object` holds an OPEN directory stream and ONE entry at`` |
|     - | 11055 | `` * a time (`u.dir.dirp`, `u.dir.entry`, `u.dir.index`); the chunk read the whole`` |
|     - | 11056 | ` * directory into an array at construction, and every difference followed from` |
|     - | 11057 | `` * that one choice (rule 52). php's `rewind()` re-opens the directory and SEES A`` |
|     - | 11058 | `` * FILE CREATED SINCE, its `key()` is the read index rather than an array offset,`` |
|     - | 11059 | `` * its `seek()` walks FORWARD through the object's own valid()/next() — so a`` |
|     - | 11060 | `` * subclass overriding either is obeyed — and a `clone` opens the directory again`` |
|     - | 11061 | ` * and reads forward to the same index rather than sharing a cursor.` |
|     - | 11062 | ` *` |
|     - | 11063 | ` * The handle cannot live in a property slot, because CLONE copies slots: two` |
|     - | 11064 | ` * objects would share one directory stream and close it twice. It lives in` |
|     - | 11065 | `` * `pVm->hDirHandle` keyed by the instance, with the class's xRelease closing it,`` |
|     - | 11066 | ` * and a clone — finding no entry of its own — re-opens on first use, which IS` |
|     - | 11067 | ` * php's clone handler, deferred. The one thing that deferral costs is a clone` |
|     - | 11068 | ` * whose directory is removed before it is first used: php has the stream open` |
|     - | 11069 | ` * already and answers, PHL raises "Object not initialized" (§7).` |
|     - | 11070 | ` *` |
|     - | 11071 | `` * `file_name` is LAZY here as it is in php: the path, a slash and the current`` |
|     - | 11072 | ` * entry, invalidated by every read and rebuilt on demand. That is php-visible` |
|     - | 11073 | `` * twice over -- `getPathname()` answers "" past the end while `getSize()` stats`` |
|     - | 11074 | `` * the DIRECTORY (the join with an empty entry), and `var_dump` shows one key`` |
|     - | 11075 | ` * fewer until something has asked.` |
|     - | 11076 | ` *` |
|     - | 11077 | `` * The chunk had also INVENTED `DirectoryIterator::getFlags()` (php has no such`` |
|     - | 11078 | ``  * method; only FilesystemIterator does), inherited SplFileInfo's `__toString()` `` |
|     - | 11079 | ` * where php aliases getFilename(), and mis-stated two constants:` |
|     - | 11080 | ` * FOLLOW_SYMLINKS is 16384 (it said 512, colliding with nothing but reading as` |
|     - | 11081 | ` * false for every real flags value) and OTHER_MODE_MASK is 28672.` |
|     - | 11082 | ` * ---------------------------------------------------------------------------` |
|     - | 11083 | ` */` |
|     - | 11084 | `/* php's spl_directory.h flag set, verbatim -- the values the class constants` |
|     - | 11085 | ` * publish and the masks its accessors compare with. */` |
|     - | 11086 | `#define SDI_CURRENT_AS_FILEINFO 0x0000` |
|     - | 11087 | `#define SDI_CURRENT_AS_SELF     0x0010` |
|     - | 11088 | `#define SDI_CURRENT_AS_PATHNAME 0x0020` |
|     - | 11089 | `#define SDI_CURRENT_MODE_MASK   0x00F0` |
|     - | 11090 | `#define SDI_KEY_AS_PATHNAME     0x0000` |
|     - | 11091 | `#define SDI_KEY_AS_FILENAME     0x0100` |
|     - | 11092 | `#define SDI_KEY_MODE_MASK       0x0F00` |
|     - | 11093 | `#define SDI_SKIPDOTS            0x1000` |
|     - | 11094 | `#define SDI_UNIXPATHS           0x2000` |
|     - | 11095 | `#define SDI_FOLLOW_SYMLINKS     0x4000` |
|     - | 11096 | `#define SDI_OTHERS_MASK         0x7000` |
|     - | 11097 | `#define SDI_FLAGS_MASK (SDI_KEY_MODE_MASK\|SDI_CURRENT_MODE_MASK\|SDI_OTHERS_MASK)` |
|     - | 11098 |  |
|     - | 11099 | `/* php's DEFAULT_SLASH, and the UNIX_PATHS flag that overrides it. */` |
|   109 | 11100 | `static char SplDirSlash(sxi64 iFlags)` |
|     2 | 11101 | `{` |
|     - | 11102 | `#ifdef __WINNT__` |
|     2 | 11103 | `	return (iFlags & SDI_UNIXPATHS) ? '/' : '\\';` |
|     - | 11104 | `#else` |
|    55 | 11105 | `	SXUNUSED(iFlags);` |
|   109 | 11106 | `	return '/';` |
|     - | 11107 | `#endif` |
|     2 | 11108 | `}` |
|     - | 11109 | `/* php's spl_filesystem_is_dot. */` |
|   272 | 11110 | `static int SplDirIsDot(const char *zName,int nName)` |
|     2 | 11111 | `{` |
|   289 | 11112 | `	return (nName == 1 && zName[0] == '.')` |
|   314 | 11113 | `		\|\| (nName == 2 && zName[0] == '.' && zName[1] == '.');` |
|     2 | 11114 | `}` |
|     - | 11115 | ``/* Does this instance carry php's `u.dir` arm? Asked by the five SplFileInfo`` |
|     - | 11116 | ` * bodies that branch on the object TYPE, so it has to be the class question and` |
|     - | 11117 | ` * not "does it have a __e slot" — a user class may declare anything. */` |
|   872 | 11118 | `static int SplDirIs(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     2 | 11119 | `{` |
|     - | 11120 | `	ph7_class *pDir;` |
|   874 | 11121 | `	if( pThis == 0 ){` |
|   ! 0 | 11122 | `		return 0;` |
|     - | 11123 | `	}` |
|   874 | 11124 | `	pDir = PH7_VmExtractClass(pVm,"DirectoryIterator",sizeof("DirectoryIterator")-1,FALSE,0);` |
|   874 | 11125 | `	return pDir && PH7_VmInstanceOf(pThis->pClass,pDir);` |
|   439 | 11126 | `}` |
|     - | 11127 | ``/* php's `!intern->u.dir.entry.d_name[0]`: the walk has nothing to describe. */`` |
|   288 | 11128 | `static int SplDirAtEnd(ph7_class_instance *pThis)` |
|     2 | 11129 | `{` |
|   290 | 11130 | `	int nEntry = 0;` |
|   290 | 11131 | `	SfiStr(pThis,SDI_E,&nEntry);` |
|   290 | 11132 | `	return nEntry < 1;` |
|     2 | 11133 | `}` |
|     - | 11134 | `/* The registry entry for this instance, or 0. */` |
|  1228 | 11135 | `static VmDirHandle * SplDirFind(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     2 | 11136 | `{` |
|     - | 11137 | `	SyHashEntry *pEntry;` |
|  1230 | 11138 | `	if( pThis == 0 \|\| SyHashTotalEntry(&pVm->hDirHandle) < 1 ){` |
|    24 | 11139 | `		return 0;` |
|     - | 11140 | `	}` |
|  1208 | 11141 | `	pEntry = SyHashGet(&pVm->hDirHandle,(const void *)&pThis,sizeof(void *));` |
|  1208 | 11142 | `	return pEntry ? (VmDirHandle *)pEntry->pUserData : 0;` |
|   620 | 11143 | `}` |
|     - | 11144 | `/* Is this a GlobIterator? The CLASS question, asked where there may be no` |
|     - | 11145 | ` * handle to ask -- an instance whose parent constructor never ran has one. */` |
|   166 | 11146 | `static int SplGlobIs(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 11147 | `{` |
|     - | 11148 | `	ph7_class *pGlob;` |
|   167 | 11149 | `	if( pThis == 0 ){` |
|   ! 0 | 11150 | `		return 0;` |
|     - | 11151 | `	}` |
|   167 | 11152 | `	pGlob = PH7_VmExtractClass(pVm,"GlobIterator",sizeof("GlobIterator")-1,FALSE,0);` |
|   167 | 11153 | `	return pGlob && PH7_VmInstanceOf(pThis->pClass,pGlob) ? 1 : 0;` |
|    84 | 11154 | `}` |
|     - | 11155 | `/*` |
|     - | 11156 | ` * php's spl_filesystem_object_get_path for a GLOB handle: the directory of the` |
|     - | 11157 | ` * match the last read handed out, which the STREAM tracks and the object does` |
|     - | 11158 | `` * not. It moves with the walk -- `glob://a/` + `*` + `/` + `*.txt` reports `a/sub1`, then`` |
|     - | 11159 | `` * `a/sub2` -- it is the EMPTY string for a match with no slash in it, and it is`` |
|     - | 11160 | ` * cleared when the walk runs out, which is what makes getPathname() answer ""` |
|     - | 11161 | ` * past the end.` |
|     - | 11162 | ` *` |
|     - | 11163 | ` * Answers 0 for any other handle, whose path is the slot the constructor wrote.` |
|     - | 11164 | ` * Asked through SplDirFind() rather than SplDirState(): a handle that is not` |
|     - | 11165 | ` * open has no current match to have a directory OF, and GlobIterator is` |
|     - | 11166 | ` * uncloneable, so the re-open SplDirState() exists for cannot arise here.` |
|     - | 11167 | ` */` |
|   359 | 11168 | `static const char * SplDirGlobPath(ph7_vm *pVm,ph7_class_instance *pThis,int *pnLen)` |
|     2 | 11169 | `{` |
|   361 | 11170 | `	VmDirHandle *pH = SplDirFind(pVm,pThis);` |
|   361 | 11171 | `	*pnLen = 0;` |
|   361 | 11172 | `	if( pH == 0 \|\| pH->pStream == 0 \|\| !PH7_GlobStreamIs(pH->pStream) ){` |
|   301 | 11173 | `		return 0;` |
|     - | 11174 | `	}` |
|    61 | 11175 | `	return PH7_GlobStreamPath(pH->pHandle,pnLen);` |
|   182 | 11176 | `}` |
|     - | 11177 | `/* Close the handle this instance owns, if any. The class's xRelease, and the` |
|     - | 11178 | ` * first half of a re-open. */` |
|   114 | 11179 | `static void SplDirClose(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     2 | 11180 | `{` |
|   116 | 11181 | `	void *pData = 0;` |
|   114 | 11182 | `	if( SyHashDeleteEntry(&pVm->hDirHandle,(const void *)&pThis,sizeof(void *),&pData) == SXRET_OK` |
|   108 | 11183 | `	 && pData ){` |
|   100 | 11184 | `		VmDirHandle *pH = (VmDirHandle *)pData;` |
|   100 | 11185 | `		if( pH->pStream && pH->pStream->xCloseDir ){` |
|   100 | 11186 | `			pH->pStream->xCloseDir(pH->pHandle);` |
|    49 | 11187 | `		}` |
|   100 | 11188 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|    49 | 11189 | `	}` |
|   116 | 11190 | `}` |
|     - | 11191 | `/*` |
|     - | 11192 | ` * Close every DIR the program still held at VM shutdown. xRelease (SplDirClose` |
|     - | 11193 | ` * above) covers an instance the program DESTROYED; an iterator alive at script` |
|     - | 11194 | ` * end reaches PH7_VmRelease with its handle still open, and the OS stream` |
|     - | 11195 | ` * behind it lives outside SyMemBackend -- the wholesale release frees the` |
|     - | 11196 | ` * VmDirHandle record and leaks the DIR (the leak checker is what noticed:` |
|     - | 11197 | ` * glibc's opendir buffer, ~32KB per survivor). Called from PH7_VmRelease` |
|     - | 11198 | ` * before the backend goes; the records themselves are backend memory.` |
|     - | 11199 | ` */` |
|  5629 | 11200 | `PH7_PRIVATE void PH7_SplDirVmRelease(ph7_vm *pVm)` |
|     5 | 11201 | `{` |
|     - | 11202 | `	SyHashEntry *pEntry;` |
|  5634 | 11203 | `	SyHashResetLoopCursor(&pVm->hDirHandle);` |
|  5634 | 11204 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hDirHandle)) != 0 ){` |
|   ! 0 | 11205 | `		VmDirHandle *pH = (VmDirHandle *)pEntry->pUserData;` |
|   ! 0 | 11206 | `		if( pH && pH->pStream && pH->pStream->xCloseDir ){` |
|   ! 0 | 11207 | `			pH->pStream->xCloseDir(pH->pHandle);` |
|   ! 0 | 11208 | `		}` |
|   ! 0 | 11209 | `	}` |
|  5634 | 11210 | `}` |
|     - | 11211 | `/*` |
|     - | 11212 | ` * php's spl_filesystem_dir_read: invalidate the lazy name, then take ONE entry` |
|     - | 11213 | ` * from the stream; running out leaves the entry empty, which is what valid()` |
|     - | 11214 | ` * reports. The read goes through a scratch call context because the VFS reports` |
|     - | 11215 | ` * a name by writing a RESULT -- borrowing the method's own return slot would` |
|     - | 11216 | ` * append to whatever the body is about to answer (rule 54).` |
|     - | 11217 | ` */` |
|   404 | 11218 | `static void SplDirRead(ph7_vm *pVm,ph7_class_instance *pThis,VmDirHandle *pH)` |
|     2 | 11219 | `{` |
|     - | 11220 | `	ph7_context sCtx;` |
|     - | 11221 | `	ph7_value sOut;` |
|   406 | 11222 | `	int rc = -1;` |
|   406 | 11223 | `	PH7_NativeSetAttrStr(pVm,pThis,SFI_N,"",0);` |
|   406 | 11224 | `	PH7_MemObjInit(pVm,&sOut);` |
|   406 | 11225 | `	VmInitCallContext(&sCtx,pVm,0,&sOut,0);` |
|   406 | 11226 | `	if( pH && pH->pStream && pH->pStream->xReadDir ){` |
|   406 | 11227 | `		rc = pH->pStream->xReadDir(pH->pHandle,&sCtx);` |
|   207 | 11228 | `	}` |
|   406 | 11229 | `	if( rc == PH7_OK ){` |
|   358 | 11230 | `		int nName = 0;` |
|   358 | 11231 | `		const char *zName = ph7_value_to_string(&sOut,&nName);` |
|   358 | 11232 | `		PH7_NativeSetAttrStr(pVm,pThis,SDI_E,zName,nName);` |
|   185 | 11233 | `	}else{` |
|    50 | 11234 | `		PH7_NativeSetAttrStr(pVm,pThis,SDI_E,"",0);` |
|     - | 11235 | `	}` |
|   406 | 11236 | `	VmReleaseCallContext(&sCtx);` |
|   406 | 11237 | `	PH7_MemObjRelease(&sOut);` |
|   406 | 11238 | `}` |
|     - | 11239 | `/* php's read loop: one entry, then more while SKIP_DOTS and this is a dot. */` |
|   270 | 11240 | `static void SplDirReadSkip(ph7_vm *pVm,ph7_class_instance *pThis,VmDirHandle *pH)` |
|     2 | 11241 | `{` |
|   272 | 11242 | `	sxi64 iFlags = PH7_NativeAttrInt(pThis,SDI_F);` |
|   260 | 11243 | `	for(;;){` |
|   396 | 11244 | `		int nEntry = 0;` |
|     - | 11245 | `		const char *zEntry;` |
|   396 | 11246 | `		SplDirRead(pVm,pThis,pH);` |
|   396 | 11247 | `		if( (iFlags & SDI_SKIPDOTS) == 0 ){` |
|   211 | 11248 | `			return;` |
|     - | 11249 | `		}` |
|   246 | 11250 | `		zEntry = SfiStr(pThis,SDI_E,&nEntry);` |
|   246 | 11251 | `		if( !SplDirIsDot(zEntry,nEntry) ){` |
|   122 | 11252 | `			return;` |
|     - | 11253 | `		}` |
|     2 | 11254 | `	}` |
|   138 | 11255 | `}` |
|     - | 11256 | `/*` |
|     - | 11257 | ` * php's spl_filesystem_dir_open: open the directory, remember it under the path` |
|     - | 11258 | ` * MINUS one trailing slash, and read the first entry. Answers 0 when the open` |
|     - | 11259 | ` * failed, having still written the path (php sets it either way, so a caught` |
|     - | 11260 | ` * constructor failure leaves the same shape behind).` |
|     - | 11261 | ` */` |
|   100 | 11262 | `static VmDirHandle * SplDirOpen(ph7_vm *pVm,ph7_class_instance *pThis,` |
|     - | 11263 | `	const char *zPath,int nPath)` |
|     2 | 11264 | `{` |
|     - | 11265 | `	const ph7_io_stream *pStream;` |
|     - | 11266 | `	const char *zDevice;` |
|     - | 11267 | `	VmDirHandle *pH;` |
|     - | 11268 | `	char zBuf[4096];` |
|   102 | 11269 | `	void *pHandle = 0;` |
|   102 | 11270 | `	int nKeep = nPath;` |
|   102 | 11271 | `	if( nPath < 1 \|\| nPath >= (int)sizeof(zBuf) ){` |
|   ! 0 | 11272 | `		return 0;` |
|     - | 11273 | `	}` |
|   102 | 11274 | `	SyMemcpy(zPath,zBuf,(sxu32)nPath);` |
|   102 | 11275 | `	zBuf[nPath] = 0;` |
|   102 | 11276 | `	zDevice = zBuf;` |
|   102 | 11277 | `	pStream = PH7_VmGetStreamDevice(pVm,&zDevice,nPath);` |
|   102 | 11278 | `	if( nKeep > 1 && SFI_IS_SLASH(zPath[nKeep-1]) ){` |
|     3 | 11279 | `		nKeep--;` |
|     1 | 11280 | `	}` |
|   102 | 11281 | `	PH7_NativeSetAttrStr(pVm,pThis,SFI_P,zPath,nKeep);` |
|   102 | 11282 | `	PH7_NativeSetAttrInt(pVm,pThis,SDI_I,0);` |
|   102 | 11283 | `	PH7_NativeSetAttrStr(pVm,pThis,SDI_E,"",0);` |
|   102 | 11284 | `	PH7_NativeSetAttrStr(pVm,pThis,SFI_N,"",0);` |
|   102 | 11285 | `	if( pStream == 0 \|\| pStream->xOpenDir == 0 ){` |
|   ! 0 | 11286 | `		return 0;` |
|     - | 11287 | `	}` |
|     - | 11288 | `	{` |
|     - | 11289 | `		/* The device takes the VM through this argument (see opendir). */` |
|     - | 11290 | `		ph7_value sDummy;` |
|     - | 11291 | `		int rc;` |
|   102 | 11292 | `		PH7_MemObjInit(pVm,&sDummy);` |
|   102 | 11293 | `		rc = pStream->xOpenDir(zDevice,&sDummy,&pHandle);` |
|   102 | 11294 | `		PH7_MemObjRelease(&sDummy);` |
|   102 | 11295 | `		if( rc != PH7_OK ){` |
|     3 | 11296 | `			return 0;` |
|     - | 11297 | `		}` |
|     - | 11298 | `	}` |
|   100 | 11299 | `	pH = (VmDirHandle *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmDirHandle));` |
|   100 | 11300 | `	if( pH == 0 ){` |
|   ! 0 | 11301 | `		if( pStream->xCloseDir ){` |
|   ! 0 | 11302 | `			pStream->xCloseDir(pHandle);` |
|   ! 0 | 11303 | `		}` |
|   ! 0 | 11304 | `		return 0;` |
|     - | 11305 | `	}` |
|   100 | 11306 | `	pH->pStream = pStream;` |
|   100 | 11307 | `	pH->pHandle = pHandle;` |
|   100 | 11308 | `	pH->pThis = pThis;` |
|     - | 11309 | `	/* SyHashInsert BORROWS the key bytes: key off the record's own field, which` |
|     - | 11310 | `	 * lives exactly as long as the entry does (rule 22). */` |
|   100 | 11311 | `	if( SyHashInsert(&pVm->hDirHandle,(const void *)&pH->pThis,sizeof(void *),pH) != SXRET_OK ){` |
|   ! 0 | 11312 | `		if( pStream->xCloseDir ){` |
|   ! 0 | 11313 | `			pStream->xCloseDir(pHandle);` |
|   ! 0 | 11314 | `		}` |
|   ! 0 | 11315 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|   ! 0 | 11316 | `		return 0;` |
|     - | 11317 | `	}` |
|   100 | 11318 | `	return pH;` |
|    52 | 11319 | `}` |
|     - | 11320 | `/*` |
|     - | 11321 | ` * The open handle behind this instance, RE-OPENING it for a fresh clone.` |
|     - | 11322 | ` *` |
|     - | 11323 | ` * php's clone handler opens the directory again and reads forward to the` |
|     - | 11324 | ` * source's index, because a directory stream cannot be duplicated; PHL does the` |
|     - | 11325 | ` * same work on first use instead, which is what keeps the handle out of every` |
|     - | 11326 | ``  * php-visible surface — a property slot carrying it would make `$a == clone $a` `` |
|     - | 11327 | ` * false, and php says true.` |
|     - | 11328 | ` */` |
|   659 | 11329 | `static VmDirHandle * SplDirState(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     2 | 11330 | `{` |
|   661 | 11331 | `	VmDirHandle *pH = SplDirFind(pVm,pThis);` |
|     - | 11332 | `	sxi64 iIndex;` |
|   661 | 11333 | `	int nPath = 0;` |
|     - | 11334 | `	const char *zPath;` |
|     - | 11335 | `	SyBlob sPath;` |
|   661 | 11336 | `	if( pH ){` |
|   635 | 11337 | `		return pH;` |
|     - | 11338 | `	}` |
|    27 | 11339 | `	zPath = SfiStr(pThis,SFI_P,&nPath);` |
|    27 | 11340 | `	if( nPath < 1 ){` |
|    25 | 11341 | `		return 0;   /* never constructed: php's "Object not initialized" */` |
|     - | 11342 | `	}` |
|     - | 11343 | `	/* The path slot is about to be rewritten by the open, so copy it out first. */` |
|     3 | 11344 | `	SyBlobInit(&sPath,&pVm->sAllocator);` |
|     3 | 11345 | `	SyBlobAppend(&sPath,zPath,(sxu32)nPath);` |
|     3 | 11346 | `	iIndex = PH7_NativeAttrInt(pThis,SDI_I);` |
|     3 | 11347 | `	pH = SplDirOpen(pVm,pThis,(const char *)SyBlobData(&sPath),(int)SyBlobLength(&sPath));` |
|     3 | 11348 | `	SyBlobRelease(&sPath);` |
|     3 | 11349 | `	if( pH == 0 ){` |
|   ! 0 | 11350 | `		return 0;` |
|     - | 11351 | `	}` |
|     3 | 11352 | `	SplDirReadSkip(pVm,pThis,pH);` |
|     - | 11353 | `	{` |
|     3 | 11354 | `		sxi64 iAt = iIndex;` |
|     5 | 11355 | `		while( iAt-- > 0 ){` |
|     3 | 11356 | `			SplDirReadSkip(pVm,pThis,pH);` |
|     1 | 11357 | `		}` |
|     - | 11358 | `	}` |
|     - | 11359 | `	/* The open above reset the index; the clone stands where the source stood. */` |
|     3 | 11360 | `	PH7_NativeSetAttrInt(pVm,pThis,SDI_I,iIndex);` |
|     3 | 11361 | `	return pH;` |
|   335 | 11362 | `}` |
|     - | 11363 | `/*` |
|     - | 11364 | ` * php's spl_filesystem_object_get_file_name for a DIR: the path, a slash and the` |
|     - | 11365 | ` * current entry, cached until the next read drops it. Called through SfiName(),` |
|     - | 11366 | ` * so every SplFileInfo accessor sees the same lazy value php's do.` |
|     - | 11367 | ` */` |
|   167 | 11368 | `static const char * SplDirName(ph7_vm *pVm,ph7_class_instance *pThis,int *pnLen)` |
|     2 | 11369 | `{` |
|   169 | 11370 | `	int nName = 0,nPath = 0,nEntry = 0;` |
|   169 | 11371 | `	const char *zName = SfiStr(pThis,SFI_N,&nName);` |
|     - | 11372 | `	const char *zPath,*zEntry;` |
|     - | 11373 | `	SyBlob sName;` |
|   169 | 11374 | `	if( nName > 0 ){` |
|    64 | 11375 | `		*pnLen = nName;` |
|    64 | 11376 | `		return zName;` |
|     - | 11377 | `	}` |
|     - | 11378 | `	/* A glob handle's path is the current match's directory, not the slot --` |
|     - | 11379 | ``	 * the slot holds the whole `glob://pattern`, which is not a directory at`` |
|     - | 11380 | `	 * all. php's join then has a branch PHL never needed: when the path is` |
|     - | 11381 | `	 * EMPTY the name is the entry ALONE, which is what makes` |
|     - | 11382 | ``	 * `new GlobIterator('d')` answer `d` for getPathname() rather than `/d`. */`` |
|   107 | 11383 | `	zPath = SplDirGlobPath(pVm,pThis,&nPath);` |
|   107 | 11384 | `	if( zPath == 0 ){` |
|    77 | 11385 | `		zPath = SfiStr(pThis,SFI_P,&nPath);` |
|    77 | 11386 | `		if( nPath < 1 ){` |
|   ! 0 | 11387 | `			*pnLen = 0;` |
|   ! 0 | 11388 | `			return "";` |
|     - | 11389 | `		}` |
|    38 | 11390 | `	}` |
|   107 | 11391 | `	SyBlobInit(&sName,&pVm->sAllocator);` |
|   107 | 11392 | `	if( nPath > 0 ){` |
|   105 | 11393 | `		char cSlash = SplDirSlash(PH7_NativeAttrInt(pThis,SDI_F));` |
|   105 | 11394 | `		SyBlobAppend(&sName,zPath,(sxu32)nPath);` |
|   105 | 11395 | `		SyBlobAppend(&sName,(const void *)&cSlash,sizeof(char));` |
|    52 | 11396 | `	}` |
|   107 | 11397 | `	zEntry = SfiStr(pThis,SDI_E,&nEntry);` |
|   107 | 11398 | `	SyBlobAppend(&sName,zEntry,(sxu32)nEntry);` |
|   160 | 11399 | `	PH7_NativeSetAttrStr(pVm,pThis,SFI_N,` |
|   105 | 11400 | `		(const char *)SyBlobData(&sName),(int)SyBlobLength(&sName));` |
|   107 | 11401 | `	SyBlobRelease(&sName);` |
|   107 | 11402 | `	return SfiStr(pThis,SFI_N,pnLen);` |
|    86 | 11403 | `}` |
|     - | 11404 | `/* php's CHECK_DIRECTORY_ITERATOR_IS_INITIALIZED: every DirectoryIterator method` |
|     - | 11405 | ` * refuses an object whose parent constructor never ran. */` |
|   464 | 11406 | `static VmDirHandle * SplDirChecked(ph7_context *pCtx,sxi32 *pRc)` |
|     2 | 11407 | `{` |
|   466 | 11408 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 11409 | `	VmDirHandle *pH;` |
|     - | 11410 | ``	/* php's `check` object handlers run BEFORE the method does, so a`` |
|     - | 11411 | `	 * GlobIterator whose parent constructor never ran refuses with THAT` |
|     - | 11412 | `	 * sentence rather than this one -- and refuses methods this check would` |
|     - | 11413 | `	 * have let through. A class without those handlers passes straight by. */` |
|   466 | 11414 | `	if( !SfoChecked(pCtx,pRc) ){` |
|     7 | 11415 | `		return 0;` |
|     - | 11416 | `	}` |
|   460 | 11417 | `	pH = SplDirState(pCtx->pVm,pThis);` |
|   460 | 11418 | `	if( pH == 0 ){` |
|    15 | 11419 | `		*pRc = PH7_VmThrowException(pCtx,"Error","Object not initialized");` |
|     7 | 11420 | `	}` |
|   460 | 11421 | `	return pH;` |
|   237 | 11422 | `}` |
|     - | 11423 | `/*` |
|     - | 11424 | ` * The shared constructor: php's spl_filesystem_object_construct, whose two` |
|     - | 11425 | ` * refusals are a ValueError for an empty path and an UnexpectedValueException` |
|     - | 11426 | ` * carrying the OPEN's own errno text (php promotes the opendir warning, so the` |
|     - | 11427 | ` * message is the warning's, prefixed with the constructor that raised it).` |
|     - | 11428 | ` */` |
|   106 | 11429 | `static int SplDirConstructVal(ph7_context *pCtx,const char *zClass,const char *zArg,` |
|     - | 11430 | `	ph7_value *pPath,sxi64 iFlags)` |
|     2 | 11431 | `{` |
|   108 | 11432 | `	ph7_vm *pVm = pCtx->pVm;` |
|   108 | 11433 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 11434 | `	const char *zPath;` |
|   108 | 11435 | `	int nPath = 0;` |
|   108 | 11436 | `	if( pThis == 0 ){` |
|   ! 0 | 11437 | `		return PH7_OK;` |
|     - | 11438 | `	}` |
|   108 | 11439 | `	zPath = ph7_value_to_string(pPath,&nPath);` |
|   108 | 11440 | `	if( nPath < 1 ){` |
|    10 | 11441 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     3 | 11442 | `			"%s::__construct(): Argument #1 ($%s) must not be empty",zClass,zArg);` |
|     - | 11443 | `	}` |
|   102 | 11444 | `	if( SplDirFind(pVm,pThis) ){` |
|     3 | 11445 | `		return PH7_VmThrowException(pCtx,"Error","Directory object is already initialized");` |
|     - | 11446 | `	}` |
|   100 | 11447 | `	PH7_NativeSetAttrInt(pVm,pThis,SDI_F,iFlags);` |
|   100 | 11448 | `	if( SplDirOpen(pVm,pThis,zPath,nPath) == 0 ){` |
|     4 | 11449 | `		return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     1 | 11450 | `			"%s::__construct(%.*s): Failed to open directory: %s",zClass,nPath,zPath,` |
|     2 | 11451 | `			VfsStrerror(errno));` |
|     - | 11452 | `	}` |
|    98 | 11453 | `	SplDirReadSkip(pVm,pThis,SplDirFind(pVm,pThis));` |
|    98 | 11454 | `	return PH7_OK;` |
|    55 | 11455 | `}` |
|     - | 11456 | `/* The three directory classes take their path straight from the argument; only` |
|     - | 11457 | ` * GlobIterator rewrites it first, which is why the open takes a VALUE. */` |
|    78 | 11458 | `static int SplDirConstruct(ph7_context *pCtx,const char *zClass,int nArg,ph7_value **apArg,` |
|     - | 11459 | `	sxi64 iFlags)` |
|     2 | 11460 | `{` |
|    80 | 11461 | `	if( nArg < 1 ){` |
|   ! 0 | 11462 | `		return PH7_OK;` |
|     - | 11463 | `	}` |
|    80 | 11464 | `	return SplDirConstructVal(pCtx,zClass,"directory",apArg[0],iFlags);` |
|    41 | 11465 | `}` |
|     - | 11466 | `/* DirectoryIterator::__construct(string $directory) — php's flags for this one` |
|     - | 11467 | ` * are KEY_AS_PATHNAME\|CURRENT_AS_SELF, and it takes no flags argument. */` |
|    38 | 11468 | `static int vm_builtin_DirectoryIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11469 | `{` |
|    39 | 11470 | `	return SplDirConstruct(pCtx,"DirectoryIterator",nArg,apArg,` |
|     - | 11471 | `		SDI_KEY_AS_PATHNAME\|SDI_CURRENT_AS_SELF);` |
|     1 | 11472 | `}` |
|     - | 11473 | `/* The flags argument the two subclasses share: php's ZPP overwrites the whole` |
|     - | 11474 | ` * default when one is given, so SKIP_DOTS is NOT implied by passing flags. */` |
|    68 | 11475 | `static sxi64 SplDirFlagArg(int nArg,ph7_value **apArg,sxi64 iDefault)` |
|     2 | 11476 | `{` |
|    70 | 11477 | `	return nArg > 1 ? ph7_value_to_int64(apArg[1]) : iDefault;` |
|     2 | 11478 | `}` |
|    22 | 11479 | `static int vm_builtin_FilesystemIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11480 | `{` |
|    34 | 11481 | `	return SplDirConstruct(pCtx,"FilesystemIterator",nArg,apArg,` |
|    11 | 11482 | `		SplDirFlagArg(nArg,apArg,SDI_KEY_AS_PATHNAME\|SDI_CURRENT_AS_FILEINFO\|SDI_SKIPDOTS));` |
|     1 | 11483 | `}` |
|    18 | 11484 | `static int vm_builtin_RecursiveDirectoryIterator_construct(ph7_context *pCtx,int nArg,` |
|     - | 11485 | `	ph7_value **apArg)` |
|     2 | 11486 | `{` |
|    29 | 11487 | `	return SplDirConstruct(pCtx,"RecursiveDirectoryIterator",nArg,apArg,` |
|     9 | 11488 | `		SplDirFlagArg(nArg,apArg,SDI_KEY_AS_PATHNAME\|SDI_CURRENT_AS_FILEINFO));` |
|     2 | 11489 | `}` |
|     - | 11490 | `/* DirectoryIterator::rewind(): php re-opens nothing — it rewinds the STREAM and` |
|     - | 11491 | ` * takes one entry, with no dot skipping at this level. */` |
|    12 | 11492 | `static int vm_builtin_DirectoryIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11493 | `{` |
|     - | 11494 | `	sxi32 rc;` |
|    13 | 11495 | `	VmDirHandle *pH = SplDirChecked(pCtx,&rc);` |
|     6 | 11496 | `	SXUNUSED(nArg);` |
|     6 | 11497 | `	SXUNUSED(apArg);` |
|    13 | 11498 | `	if( pH == 0 ){` |
|     3 | 11499 | `		return rc;` |
|     - | 11500 | `	}` |
|    11 | 11501 | `	PH7_NativeSetAttrInt(pCtx->pVm,PH7_ContextThis(pCtx),SDI_I,0);` |
|    11 | 11502 | `	if( pH->pStream->xRewindDir ){` |
|    11 | 11503 | `		pH->pStream->xRewindDir(pH->pHandle);` |
|     5 | 11504 | `	}` |
|    11 | 11505 | `	SplDirRead(pCtx->pVm,PH7_ContextThis(pCtx),pH);` |
|    11 | 11506 | `	return PH7_OK;` |
|     7 | 11507 | `}` |
|     - | 11508 | `/* FilesystemIterator::rewind(): the same, plus the dot skipping, and php does` |
|     - | 11509 | ` * NOT check the handle here (an uninitialized object simply rewinds to nothing). */` |
|    44 | 11510 | `static int vm_builtin_FilesystemIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 11511 | `{` |
|    46 | 11512 | `	ph7_vm *pVm = pCtx->pVm;` |
|    46 | 11513 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 11514 | `	VmDirHandle *pH;` |
|     - | 11515 | `	sxi32 rcChk;` |
|    22 | 11516 | `	SXUNUSED(nArg);` |
|    22 | 11517 | `	SXUNUSED(apArg);` |
|    46 | 11518 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|     3 | 11519 | `		return rcChk;` |
|     - | 11520 | `	}` |
|    44 | 11521 | `	pH = SplDirState(pVm,pThis);` |
|    44 | 11522 | `	PH7_NativeSetAttrInt(pVm,pThis,SDI_I,0);` |
|    44 | 11523 | `	if( pH && pH->pStream->xRewindDir ){` |
|    44 | 11524 | `		pH->pStream->xRewindDir(pH->pHandle);` |
|    21 | 11525 | `	}` |
|    44 | 11526 | `	SplDirReadSkip(pVm,pThis,pH);` |
|    44 | 11527 | `	return PH7_OK;` |
|    24 | 11528 | `}` |
|   130 | 11529 | `static int vm_builtin_DirectoryIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 11530 | `{` |
|   132 | 11531 | `	ph7_vm *pVm = pCtx->pVm;` |
|   132 | 11532 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 11533 | `	sxi32 rc;` |
|   132 | 11534 | `	VmDirHandle *pH = SplDirChecked(pCtx,&rc);` |
|    66 | 11535 | `	SXUNUSED(nArg);` |
|    66 | 11536 | `	SXUNUSED(apArg);` |
|   132 | 11537 | `	if( pH == 0 ){` |
|     3 | 11538 | `		return rc;` |
|     - | 11539 | `	}` |
|     - | 11540 | `	/* php advances the index PAST the end too, which is why key() keeps counting` |
|     - | 11541 | `	 * once valid() is false. */` |
|   130 | 11542 | `	PH7_NativeSetAttrInt(pVm,pThis,SDI_I,PH7_NativeAttrInt(pThis,SDI_I) + 1);` |
|   130 | 11543 | `	SplDirReadSkip(pVm,pThis,pH);` |
|   130 | 11544 | `	return PH7_OK;` |
|    68 | 11545 | `}` |
|   198 | 11546 | `static int vm_builtin_DirectoryIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 11547 | `{` |
|     - | 11548 | `	sxi32 rc;` |
|   200 | 11549 | `	VmDirHandle *pH = SplDirChecked(pCtx,&rc);` |
|   100 | 11550 | `	SXUNUSED(nArg);` |
|   100 | 11551 | `	SXUNUSED(apArg);` |
|   200 | 11552 | `	if( pH == 0 ){` |
|     5 | 11553 | `		return rc;` |
|     - | 11554 | `	}` |
|   196 | 11555 | `	ph7_result_bool(pCtx,!SplDirAtEnd(PH7_ContextThis(pCtx)));` |
|   196 | 11556 | `	return PH7_OK;` |
|   102 | 11557 | `}` |
|    26 | 11558 | `static int vm_builtin_DirectoryIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11559 | `{` |
|     - | 11560 | `	sxi32 rc;` |
|    27 | 11561 | `	VmDirHandle *pH = SplDirChecked(pCtx,&rc);` |
|    13 | 11562 | `	SXUNUSED(nArg);` |
|    13 | 11563 | `	SXUNUSED(apArg);` |
|    27 | 11564 | `	if( pH == 0 ){` |
|     3 | 11565 | `		return rc;` |
|     - | 11566 | `	}` |
|    25 | 11567 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(PH7_ContextThis(pCtx),SDI_I));` |
|    25 | 11568 | `	return PH7_OK;` |
|    14 | 11569 | `}` |
|    21 | 11570 | `static int vm_builtin_DirectoryIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11571 | `{` |
|     - | 11572 | `	sxi32 rc;` |
|    22 | 11573 | `	VmDirHandle *pH = SplDirChecked(pCtx,&rc);` |
|    11 | 11574 | `	SXUNUSED(nArg);` |
|    11 | 11575 | `	SXUNUSED(apArg);` |
|    22 | 11576 | `	if( pH == 0 ){` |
|     3 | 11577 | `		return rc;` |
|     - | 11578 | `	}` |
|    20 | 11579 | `	SplResultBorrowed(pCtx,PH7_ContextThis(pCtx));` |
|    20 | 11580 | `	return PH7_OK;` |
|    12 | 11581 | `}` |
|    14 | 11582 | `static int vm_builtin_DirectoryIterator_isDot(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11583 | `{` |
|    15 | 11584 | `	int nEntry = 0;` |
|     - | 11585 | `	const char *zEntry;` |
|     - | 11586 | `	sxi32 rc;` |
|    15 | 11587 | `	VmDirHandle *pH = SplDirChecked(pCtx,&rc);` |
|     7 | 11588 | `	SXUNUSED(nArg);` |
|     7 | 11589 | `	SXUNUSED(apArg);` |
|    15 | 11590 | `	if( pH == 0 ){` |
|     5 | 11591 | `		return rc;` |
|     - | 11592 | `	}` |
|    11 | 11593 | `	zEntry = SfiStr(PH7_ContextThis(pCtx),SDI_E,&nEntry);` |
|    11 | 11594 | `	ph7_result_bool(pCtx,SplDirIsDot(zEntry,nEntry));` |
|    11 | 11595 | `	return PH7_OK;` |
|     8 | 11596 | `}` |
|     - | 11597 | `/*` |
|     - | 11598 | ` * php's seek(): rewind if the target is behind us, then walk forward through` |
|     - | 11599 | ` * the OBJECT's own valid()/next() — a subclass overriding either is obeyed, and` |
|     - | 11600 | ` * running out raises php's OutOfBoundsException with the iterator left standing` |
|     - | 11601 | ` * where the walk stopped.` |
|     - | 11602 | ` */` |
|    10 | 11603 | `static int vm_builtin_DirectoryIterator_seek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11604 | `{` |
|    11 | 11605 | `	ph7_vm *pVm = pCtx->pVm;` |
|    11 | 11606 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 11607 | `	ph7_class_method *pMethod;` |
|     - | 11608 | `	sxi64 iPos;` |
|     - | 11609 | `	sxi32 rc;` |
|    11 | 11610 | `	VmDirHandle *pH = SplDirChecked(pCtx,&rc);` |
|    11 | 11611 | `	if( pH == 0 ){` |
|   ! 0 | 11612 | `		return rc;` |
|     - | 11613 | `	}` |
|    11 | 11614 | `	iPos = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|    11 | 11615 | `	if( PH7_NativeAttrInt(pThis,SDI_I) > iPos ){` |
|     5 | 11616 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|     5 | 11617 | `		if( pMethod ){` |
|     5 | 11618 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,0,0,0);` |
|     5 | 11619 | `			if( rc != SXRET_OK ){` |
|   ! 0 | 11620 | `				return rc;` |
|     - | 11621 | `			}` |
|     2 | 11622 | `		}` |
|     2 | 11623 | `	}` |
|    27 | 11624 | `	while( PH7_NativeAttrInt(pThis,SDI_I) < iPos ){` |
|     - | 11625 | `		ph7_value sRet;` |
|     - | 11626 | `		int bValid;` |
|    19 | 11627 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|    19 | 11628 | `		if( pMethod == 0 ){` |
|   ! 0 | 11629 | `			break;` |
|     - | 11630 | `		}` |
|    19 | 11631 | `		PH7_MemObjInit(pVm,&sRet);` |
|    19 | 11632 | `		rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sRet,0,0);` |
|    19 | 11633 | `		bValid = rc == SXRET_OK && ph7_value_to_bool(&sRet);` |
|    19 | 11634 | `		PH7_MemObjRelease(&sRet);` |
|    19 | 11635 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 11636 | `			return rc;` |
|     - | 11637 | `		}` |
|    19 | 11638 | `		if( !bValid ){` |
|     4 | 11639 | `			return PH7_VmThrowException(pCtx,"OutOfBoundsException",` |
|     1 | 11640 | `				"Seek position %qd is out of range",iPos);` |
|     - | 11641 | `		}` |
|    17 | 11642 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|    17 | 11643 | `		if( pMethod == 0 ){` |
|   ! 0 | 11644 | `			break;` |
|     - | 11645 | `		}` |
|    17 | 11646 | `		rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,0,0,0);` |
|    17 | 11647 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 11648 | `			return rc;` |
|     - | 11649 | `		}` |
|     1 | 11650 | `	}` |
|     9 | 11651 | `	return PH7_OK;` |
|     6 | 11652 | `}` |
|     - | 11653 | `/* DirectoryIterator's three name accessors read the ENTRY, not the pathname —` |
|     - | 11654 | `` * which is why `getFilename()` answers `..` where SplFileInfo's would answer the`` |
|     - | 11655 | `` * whole path, and why `__toString()` is aliased to this one rather than to`` |
|     - | 11656 | ` * getPathname(). */` |
|    49 | 11657 | `static int vm_builtin_DirectoryIterator_getFilename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11658 | `{` |
|    50 | 11659 | `	int nEntry = 0;` |
|     - | 11660 | `	const char *zEntry;` |
|     - | 11661 | `	sxi32 rc;` |
|    50 | 11662 | `	VmDirHandle *pH = SplDirChecked(pCtx,&rc);` |
|    25 | 11663 | `	SXUNUSED(nArg);` |
|    25 | 11664 | `	SXUNUSED(apArg);` |
|    50 | 11665 | `	if( pH == 0 ){` |
|     5 | 11666 | `		return rc;` |
|     - | 11667 | `	}` |
|    46 | 11668 | `	zEntry = SfiStr(PH7_ContextThis(pCtx),SDI_E,&nEntry);` |
|    46 | 11669 | `	ph7_result_string(pCtx,zEntry,nEntry);` |
|    46 | 11670 | `	return PH7_OK;` |
|    26 | 11671 | `}` |
|     2 | 11672 | `static int vm_builtin_DirectoryIterator_getBasename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11673 | `{` |
|     3 | 11674 | `	int nEntry = 0,nBase = 0;` |
|     - | 11675 | `	const char *zEntry,*zBase;` |
|     - | 11676 | `	sxi32 rc;` |
|     3 | 11677 | `	VmDirHandle *pH = SplDirChecked(pCtx,&rc);` |
|     3 | 11678 | `	if( pH == 0 ){` |
|   ! 0 | 11679 | `		return rc;` |
|     - | 11680 | `	}` |
|     3 | 11681 | `	zEntry = SfiStr(PH7_ContextThis(pCtx),SDI_E,&nEntry);` |
|     3 | 11682 | `	zBase = PH7_ExtractBaseName(zEntry,nEntry,&nBase);` |
|     3 | 11683 | `	if( nArg > 0 ){` |
|     3 | 11684 | `		int nSuffix = 0;` |
|     3 | 11685 | `		const char *zSuffix = ph7_value_to_string(apArg[0],&nSuffix);` |
|     2 | 11686 | `		if( nSuffix > 0 && nSuffix < nBase` |
|     3 | 11687 | `		 && SyMemcmp(&zBase[nBase - nSuffix],zSuffix,(sxu32)nSuffix) == 0 ){` |
|     3 | 11688 | `			nBase -= nSuffix;` |
|     1 | 11689 | `		}` |
|     1 | 11690 | `	}` |
|     3 | 11691 | `	ph7_result_string(pCtx,zBase,nBase);` |
|     3 | 11692 | `	return PH7_OK;` |
|     2 | 11693 | `}` |
|     2 | 11694 | `static int vm_builtin_DirectoryIterator_getExtension(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11695 | `{` |
|     3 | 11696 | `	int nEntry = 0,nBase = 0,i;` |
|     - | 11697 | `	const char *zEntry,*zBase;` |
|     - | 11698 | `	sxi32 rc;` |
|     3 | 11699 | `	VmDirHandle *pH = SplDirChecked(pCtx,&rc);` |
|     1 | 11700 | `	SXUNUSED(nArg);` |
|     1 | 11701 | `	SXUNUSED(apArg);` |
|     3 | 11702 | `	if( pH == 0 ){` |
|   ! 0 | 11703 | `		return rc;` |
|     - | 11704 | `	}` |
|     3 | 11705 | `	zEntry = SfiStr(PH7_ContextThis(pCtx),SDI_E,&nEntry);` |
|     3 | 11706 | `	zBase = PH7_ExtractBaseName(zEntry,nEntry,&nBase);` |
|     9 | 11707 | `	for( i = nBase - 1 ; i >= 0 ; --i ){` |
|     9 | 11708 | `		if( zBase[i] == '.' ){` |
|     3 | 11709 | `			ph7_result_string(pCtx,&zBase[i+1],nBase - i - 1);` |
|     3 | 11710 | `			return PH7_OK;` |
|     - | 11711 | `		}` |
|     4 | 11712 | `	}` |
|   ! 0 | 11713 | `	ph7_result_string(pCtx,"",0);` |
|   ! 0 | 11714 | `	return PH7_OK;` |
|     2 | 11715 | `}` |
|     - | 11716 | `/*` |
|     - | 11717 | ` * FilesystemIterator::key()/current(): php compares the flag against its MASK` |
|     - | 11718 | `` * (`(flags & MODE_MASK) == mode`) rather than testing a bit, so a stray bit in`` |
|     - | 11719 | ``  * another field cannot change either answer — which the chunk's `& KEY_AS_FILENAME` `` |
|     - | 11720 | `` * and `=== CURRENT_AS_PATHNAME` both got wrong in one direction or the other.`` |
|     - | 11721 | ` */` |
|    50 | 11722 | `static int vm_builtin_FilesystemIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11723 | `{` |
|    51 | 11724 | `	ph7_vm *pVm = pCtx->pVm;` |
|    51 | 11725 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    51 | 11726 | `	sxi64 iFlags = PH7_NativeAttrInt(pThis,SDI_F);` |
|    51 | 11727 | `	int nOut = 0;` |
|     - | 11728 | `	const char *zOut;` |
|     - | 11729 | `	sxi32 rcChk;` |
|    25 | 11730 | `	SXUNUSED(nArg);` |
|    25 | 11731 | `	SXUNUSED(apArg);` |
|    51 | 11732 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|     3 | 11733 | `		return rcChk;` |
|     - | 11734 | `	}` |
|    49 | 11735 | `	if( (iFlags & SDI_KEY_MODE_MASK) == SDI_KEY_AS_FILENAME ){` |
|     7 | 11736 | `		zOut = SfiStr(pThis,SDI_E,&nOut);` |
|     7 | 11737 | `		ph7_result_string(pCtx,zOut,nOut);` |
|     7 | 11738 | `		return PH7_OK;` |
|     - | 11739 | `	}` |
|    43 | 11740 | `	if( SplDirState(pVm,pThis) == 0 ){` |
|   ! 0 | 11741 | `		return PH7_VmThrowException(pCtx,"Error","Object not initialized");` |
|     - | 11742 | `	}` |
|    43 | 11743 | `	zOut = SfiName(pVm,pThis,&nOut);` |
|    43 | 11744 | `	ph7_result_string(pCtx,zOut,nOut);` |
|    43 | 11745 | `	return PH7_OK;` |
|    26 | 11746 | `}` |
|    84 | 11747 | `static int vm_builtin_FilesystemIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 11748 | `{` |
|    86 | 11749 | `	ph7_vm *pVm = pCtx->pVm;` |
|    86 | 11750 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    86 | 11751 | `	sxi64 iMode = PH7_NativeAttrInt(pThis,SDI_F) & SDI_CURRENT_MODE_MASK;` |
|     - | 11752 | `	sxi32 rcChk;` |
|    42 | 11753 | `	SXUNUSED(nArg);` |
|    42 | 11754 | `	SXUNUSED(apArg);` |
|    86 | 11755 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|     3 | 11756 | `		return rcChk;` |
|     - | 11757 | `	}` |
|    84 | 11758 | `	if( iMode == SDI_CURRENT_AS_PATHNAME \|\| iMode == SDI_CURRENT_AS_FILEINFO ){` |
|    70 | 11759 | `		if( SplDirState(pVm,pThis) == 0 ){` |
|   ! 0 | 11760 | `			return PH7_VmThrowException(pCtx,"Error","Object not initialized");` |
|     - | 11761 | `		}` |
|    34 | 11762 | `	}` |
|    84 | 11763 | `	if( iMode == SDI_CURRENT_AS_PATHNAME ){` |
|     7 | 11764 | `		int nName = 0;` |
|     7 | 11765 | `		const char *zName = SfiName(pVm,pThis,&nName);` |
|     7 | 11766 | `		ph7_result_string(pCtx,zName,nName);` |
|     7 | 11767 | `		return PH7_OK;` |
|     - | 11768 | `	}` |
|    78 | 11769 | `	if( iMode == SDI_CURRENT_AS_FILEINFO ){` |
|    64 | 11770 | `		ph7_class *pClass = 0;` |
|    64 | 11771 | `		int nName = 0,nDir = 0;` |
|     - | 11772 | `		const char *zName,*zDir;` |
|     - | 11773 | `		sxi32 rc;` |
|    64 | 11774 | `		if( SplDirAtEnd(pThis) ){` |
|     - | 11775 | `			/* php's create_type again: there is no entry to describe. */` |
|   ! 0 | 11776 | `			return PH7_VmThrowException(pCtx,"RuntimeException","Could not open file");` |
|     - | 11777 | `		}` |
|    64 | 11778 | `		rc = SfiInfoClass(pCtx,"current",0,0,&pClass);` |
|    64 | 11779 | `		if( rc != PH7_OK ){` |
|   ! 0 | 11780 | `			return rc;` |
|     - | 11781 | `		}` |
|     - | 11782 | `		/* The child's PATH is the directory the walk is in, which for a glob` |
|     - | 11783 | `		 * handle is the current match's own -- the slot holds the whole` |
|     - | 11784 | ``		 * `glob://pattern`, and handing THAT over made every SplFileInfo the`` |
|     - | 11785 | `		 * iterator produced answer the pattern for getPath() and the joined` |
|     - | 11786 | `		 * name for getFilename(). */` |
|    64 | 11787 | `		zDir = SplDirGlobPath(pVm,pThis,&nDir);` |
|    64 | 11788 | `		if( zDir == 0 ){` |
|    42 | 11789 | `			zDir = SfiStr(pThis,SFI_P,&nDir);` |
|    20 | 11790 | `		}` |
|    64 | 11791 | `		zName = SfiName(pVm,pThis,&nName);` |
|    64 | 11792 | `		return SfiMakeInfoEx(pCtx,pClass,zName,nName,zDir,nDir);` |
|     - | 11793 | `	}` |
|    15 | 11794 | `	SplResultBorrowed(pCtx,pThis);` |
|    15 | 11795 | `	return PH7_OK;` |
|    44 | 11796 | `}` |
|     - | 11797 | `/* php's getFlags()/setFlags() answer and accept only the three mode fields;` |
|     - | 11798 | ` * everything else in the word is engine state the class keeps to itself. */` |
|    12 | 11799 | `static int vm_builtin_FilesystemIterator_getFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11800 | `{` |
|     - | 11801 | `	sxi32 rcChk;` |
|     6 | 11802 | `	SXUNUSED(nArg);` |
|     6 | 11803 | `	SXUNUSED(apArg);` |
|    13 | 11804 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|     3 | 11805 | `		return rcChk;` |
|     - | 11806 | `	}` |
|    11 | 11807 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(PH7_ContextThis(pCtx),SDI_F) & SDI_FLAGS_MASK);` |
|    11 | 11808 | `	return PH7_OK;` |
|     7 | 11809 | `}` |
|     2 | 11810 | `static int vm_builtin_FilesystemIterator_setFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11811 | `{` |
|     3 | 11812 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 11813 | `	sxi32 rcChk;` |
|     3 | 11814 | `	sxi64 iFlags = PH7_NativeAttrInt(pThis,SDI_F);` |
|     3 | 11815 | `	sxi64 iNew = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|     3 | 11816 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 11817 | `		return rcChk;` |
|     - | 11818 | `	}` |
|     4 | 11819 | `	PH7_NativeSetAttrInt(pCtx->pVm,pThis,SDI_F,(iFlags & ~(sxi64)SDI_FLAGS_MASK)` |
|     2 | 11820 | `		\| (iNew & (sxi64)SDI_FLAGS_MASK));` |
|     3 | 11821 | `	return PH7_OK;` |
|     2 | 11822 | `}` |
|     - | 11823 | `/*` |
|     - | 11824 | ` * RecursiveDirectoryIterator::hasChildren(bool $allowLinks = false).` |
|     - | 11825 | ` *` |
|     - | 11826 | ` * php lstats the entry and then asks two separate questions of it: a plain` |
|     - | 11827 | ` * directory has children, and a SYMLINK has them only when the walk was told to` |
|     - | 11828 | ` * follow links. Asked of the VFS rather than of a mode word, because the mode is` |
|     - | 11829 | ` * not filled on Windows (the same lesson getType() learned).` |
|     - | 11830 | ` */` |
|    18 | 11831 | `static int vm_builtin_RecursiveDirectoryIterator_hasChildren(ph7_context *pCtx,int nArg,` |
|     - | 11832 | `	ph7_value **apArg)` |
|     2 | 11833 | `{` |
|    20 | 11834 | `	ph7_vm *pVm = pCtx->pVm;` |
|    20 | 11835 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    20 | 11836 | `	const ph7_vfs *pVfs = pVm->pEngine->pVfs;` |
|    20 | 11837 | `	int nEntry = 0;` |
|    20 | 11838 | `	const char *zEntry = SfiStr(pThis,SDI_E,&nEntry);` |
|     - | 11839 | `	char zPath[4096];` |
|    20 | 11840 | `	int bAllow = nArg > 0 ? ph7_value_to_bool(apArg[0]) : 0;` |
|    20 | 11841 | `	sxi64 iFlags = PH7_NativeAttrInt(pThis,SDI_F);` |
|    18 | 11842 | `	if( nEntry < 1 \|\| SplDirIsDot(zEntry,nEntry) \|\| pVfs == 0` |
|    18 | 11843 | `	 \|\| SfiPathBuf(pVm,pThis,zPath,(int)sizeof(zPath)) != SXRET_OK ){` |
|     3 | 11844 | `		ph7_result_bool(pCtx,0);` |
|     3 | 11845 | `		return PH7_OK;` |
|     - | 11846 | `	}` |
|     - | 11847 | `	{` |
|     - | 11848 | `		/* A path a userland wrapper owns is ITS answer, asked exactly as the two` |
|     - | 11849 | `		 * VFS questions below are: the link question of an lstat record, the` |
|     - | 11850 | `		 * directory question of a stat one. Without this the walk treated every` |
|     - | 11851 | `		 * directory a wrapper reported as a LEAF, so a recursive iteration over` |
|     - | 11852 | `		 * one never descended. */` |
|     - | 11853 | `		ph7_int64 aVal[13];` |
|    18 | 11854 | `		int rcU = PH7_VfsUserStatFields(pCtx,zPath,PH7_STAT_ASK_IS_LINK,aVal);` |
|    18 | 11855 | `		if( PH7_CALLBACK_UNWOUND(rcU) ){` |
|   ! 0 | 11856 | `			return rcU;` |
|     - | 11857 | `		}` |
|    18 | 11858 | `		if( rcU != PHL_URLSTAT_NOWRAP ){` |
|   ! 0 | 11859 | `			if( rcU == PHL_URLSTAT_OK && (aVal[2] & PH7_S_IFMT) == PH7_S_IFLNK` |
|   ! 0 | 11860 | `			 && !bAllow && (iFlags & SDI_FOLLOW_SYMLINKS) == 0 ){` |
|   ! 0 | 11861 | `				ph7_result_bool(pCtx,0);` |
|   ! 0 | 11862 | `				return PH7_OK;` |
|     - | 11863 | `			}` |
|   ! 0 | 11864 | `			rcU = PH7_VfsUserStatFields(pCtx,zPath,PH7_STAT_ASK_IS_DIR,aVal);` |
|   ! 0 | 11865 | `			if( PH7_CALLBACK_UNWOUND(rcU) ){` |
|   ! 0 | 11866 | `				return rcU;` |
|     - | 11867 | `			}` |
|   ! 0 | 11868 | `			ph7_result_bool(pCtx,rcU == PHL_URLSTAT_OK` |
|   ! 0 | 11869 | `				&& (aVal[2] & PH7_S_IFMT) == PH7_S_IFDIR);` |
|   ! 0 | 11870 | `			return PH7_OK;` |
|     - | 11871 | `		}` |
|     - | 11872 | `	}` |
|    16 | 11873 | `	if( pVfs->xIslink && pVfs->xIslink(zPath) == PH7_OK` |
|    10 | 11874 | `	 && !bAllow && (iFlags & SDI_FOLLOW_SYMLINKS) == 0 ){` |
|   ! 0 | 11875 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 11876 | `		return PH7_OK;` |
|     - | 11877 | `	}` |
|    18 | 11878 | `	ph7_result_bool(pCtx,pVfs->xIsdir && pVfs->xIsdir(zPath) == PH7_OK);` |
|    18 | 11879 | `	return PH7_OK;` |
|    11 | 11880 | `}` |
|     - | 11881 | `/*` |
|     - | 11882 | ` * getChildren(): php builds an instance of the RUNTIME class through its` |
|     - | 11883 | ` * constructor with (pathname, flags), then hands it the sub path — which is what` |
|     - | 11884 | ` * makes getSubPathname() name the whole nested route rather than just the entry` |
|     - | 11885 | `` * (the chunk answered `''` and the filename, wrong at every depth below one).`` |
|     - | 11886 | ` */` |
|     8 | 11887 | `static int vm_builtin_RecursiveDirectoryIterator_getChildren(ph7_context *pCtx,int nArg,` |
|     - | 11888 | `	ph7_value **apArg)` |
|     2 | 11889 | `{` |
|    10 | 11890 | `	ph7_vm *pVm = pCtx->pVm;` |
|    10 | 11891 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 11892 | `	ph7_class_instance *pNew;` |
|     - | 11893 | `	ph7_class_method *pCons;` |
|     - | 11894 | `	ph7_value sPath,sFlags,*apCall[2];` |
|    10 | 11895 | `	int nName = 0,nSub = 0,nEntry = 0;` |
|     - | 11896 | `	const char *zName;` |
|    10 | 11897 | `	sxi64 iFlags = PH7_NativeAttrInt(pThis,SDI_F);` |
|     - | 11898 | `	sxi32 rc;` |
|     - | 11899 | `	SyBlob sSub;` |
|     4 | 11900 | `	SXUNUSED(nArg);` |
|     4 | 11901 | `	SXUNUSED(apArg);` |
|    10 | 11902 | `	if( SplDirState(pVm,pThis) == 0 ){` |
|   ! 0 | 11903 | `		return PH7_VmThrowException(pCtx,"Error","Object not initialized");` |
|     - | 11904 | `	}` |
|    10 | 11905 | `	pNew = PH7_NewClassInstance(pVm,pThis->pClass);` |
|    10 | 11906 | `	if( pNew == 0 ){` |
|   ! 0 | 11907 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 11908 | `	}` |
|    10 | 11909 | `	pNew->iRef++;` |
|    10 | 11910 | `	zName = SfiName(pVm,pThis,&nName);` |
|    10 | 11911 | `	PH7_MemObjInitFromString(pVm,&sPath,0);` |
|    10 | 11912 | `	PH7_MemObjStringAppend(&sPath,zName,(sxu32)nName);` |
|    10 | 11913 | `	PH7_MemObjInitFromInt(pVm,&sFlags,iFlags);` |
|    10 | 11914 | `	apCall[0] = &sPath;` |
|    10 | 11915 | `	apCall[1] = &sFlags;` |
|    10 | 11916 | `	pCons = PH7_ClassExtractMethod(pThis->pClass,"__construct",sizeof("__construct")-1);` |
|    10 | 11917 | `	rc = pCons ? PH7_VmCallClassMethod(pVm,pNew,pCons,0,2,apCall) : SXRET_OK;` |
|    10 | 11918 | `	PH7_MemObjRelease(&sPath);` |
|    10 | 11919 | `	PH7_MemObjRelease(&sFlags);` |
|    10 | 11920 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 11921 | `		PH7_ClassInstanceUnref(pNew);` |
|   ! 0 | 11922 | `		return rc;` |
|     - | 11923 | `	}` |
|     - | 11924 | `	/* php's sub_path: the parent's, this entry appended. */` |
|    10 | 11925 | `	SyBlobInit(&sSub,&pVm->sAllocator);` |
|     - | 11926 | `	{` |
|    10 | 11927 | `		const char *zSub = SfiStr(pThis,SDI_S,&nSub);` |
|    10 | 11928 | `		SyBlobAppend(&sSub,zSub,(sxu32)nSub);` |
|     - | 11929 | `	}` |
|    10 | 11930 | `	if( nSub > 0 ){` |
|     5 | 11931 | `		char cSlash = SplDirSlash(iFlags);` |
|     5 | 11932 | `		SyBlobAppend(&sSub,(const void *)&cSlash,sizeof(char));` |
|     2 | 11933 | `	}` |
|     - | 11934 | `	{` |
|    10 | 11935 | `		const char *zEntry = SfiStr(pThis,SDI_E,&nEntry);` |
|    10 | 11936 | `		SyBlobAppend(&sSub,zEntry,(sxu32)nEntry);` |
|     - | 11937 | `	}` |
|    14 | 11938 | `	PH7_NativeSetAttrStr(pVm,pNew,SDI_S,` |
|     8 | 11939 | `		(const char *)SyBlobData(&sSub),(int)SyBlobLength(&sSub));` |
|    10 | 11940 | `	SyBlobRelease(&sSub);` |
|     - | 11941 | `	{` |
|    10 | 11942 | `		int nInfo = 0;` |
|    10 | 11943 | `		const char *zInfo = SfiStr(pThis,SFI_IC,&nInfo);` |
|    10 | 11944 | `		PH7_NativeSetAttrStr(pVm,pNew,SFI_IC,zInfo,nInfo);` |
|     - | 11945 | `	}` |
|    10 | 11946 | `	PH7_NativeResultObject(pCtx,pNew);` |
|    10 | 11947 | `	PH7_ClassInstanceUnref(pNew);` |
|    10 | 11948 | `	return PH7_OK;` |
|     6 | 11949 | `}` |
|     4 | 11950 | `static int vm_builtin_RecursiveDirectoryIterator_getSubPath(ph7_context *pCtx,int nArg,` |
|     - | 11951 | `	ph7_value **apArg)` |
|     1 | 11952 | `{` |
|     5 | 11953 | `	int nSub = 0;` |
|     5 | 11954 | `	const char *zSub = SfiStr(PH7_ContextThis(pCtx),SDI_S,&nSub);` |
|     2 | 11955 | `	SXUNUSED(nArg);` |
|     2 | 11956 | `	SXUNUSED(apArg);` |
|     5 | 11957 | `	ph7_result_string(pCtx,zSub,nSub);` |
|     5 | 11958 | `	return PH7_OK;` |
|     1 | 11959 | `}` |
|     4 | 11960 | `static int vm_builtin_RecursiveDirectoryIterator_getSubPathname(ph7_context *pCtx,int nArg,` |
|     - | 11961 | `	ph7_value **apArg)` |
|     1 | 11962 | `{` |
|     5 | 11963 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     5 | 11964 | `	int nSub = 0,nEntry = 0;` |
|     5 | 11965 | `	const char *zSub = SfiStr(pThis,SDI_S,&nSub);` |
|     - | 11966 | `	SyBlob sOut;` |
|     2 | 11967 | `	SXUNUSED(nArg);` |
|     2 | 11968 | `	SXUNUSED(apArg);` |
|     5 | 11969 | `	if( nSub < 1 ){` |
|     3 | 11970 | `		const char *zEntry = SfiStr(pThis,SDI_E,&nEntry);` |
|     3 | 11971 | `		ph7_result_string(pCtx,zEntry,nEntry);` |
|     3 | 11972 | `		return PH7_OK;` |
|     - | 11973 | `	}` |
|     3 | 11974 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|     3 | 11975 | `	SyBlobAppend(&sOut,zSub,(sxu32)nSub);` |
|     - | 11976 | `	{` |
|     3 | 11977 | `		char cSlash = SplDirSlash(PH7_NativeAttrInt(pThis,SDI_F));` |
|     3 | 11978 | `		SyBlobAppend(&sOut,(const void *)&cSlash,sizeof(char));` |
|     - | 11979 | `	}` |
|     - | 11980 | `	{` |
|     3 | 11981 | `		const char *zEntry = SfiStr(pThis,SDI_E,&nEntry);` |
|     3 | 11982 | `		SyBlobAppend(&sOut,zEntry,(sxu32)nEntry);` |
|     - | 11983 | `	}` |
|     3 | 11984 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     3 | 11985 | `	SyBlobRelease(&sOut);` |
|     3 | 11986 | `	return PH7_OK;` |
|     3 | 11987 | `}` |
|     - | 11988 | `/*` |
|     - | 11989 | ` * GlobIterator::__construct(string $pattern, int $flags = 0)` |
|     - | 11990 | ` *` |
|     - | 11991 | `` * php's DIT_CTOR_GLOB: the `glob://` prefix goes on when the caller did not`` |
|     - | 11992 | `` * write one, and the whole `glob://pattern` is what the path slot keeps -- so`` |
|     - | 11993 | `` * the `glob` debug key shows it, and getPath() has to ask the STREAM instead`` |
|     - | 11994 | ` * (SplDirGlobPath). The default flags are 0, which is` |
|     - | 11995 | ` * KEY_AS_PATHNAME\|CURRENT_AS_FILEINFO with SKIP_DOTS OFF where` |
|     - | 11996 | `` * FilesystemIterator has it on: `glob('d/.*')` yields `.` and `..` through the`` |
|     - | 11997 | ` * iterator exactly as it does through the function.` |
|     - | 11998 | ` */` |
|    30 | 11999 | `static int vm_builtin_GlobIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12000 | `{` |
|    31 | 12001 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 12002 | `	const char *zPat;` |
|    31 | 12003 | `	int nPat = 0;` |
|     - | 12004 | `	ph7_value sUri;` |
|     - | 12005 | `	sxi32 rc;` |
|    31 | 12006 | `	if( PH7_ContextThis(pCtx) == 0 \|\| nArg < 1 ){` |
|   ! 0 | 12007 | `		return PH7_OK;` |
|     - | 12008 | `	}` |
|    31 | 12009 | `	zPat = ph7_value_to_string(apArg[0],&nPat);` |
|    31 | 12010 | `	if( nPat < 1 ){` |
|     - | 12011 | `		/* php's empty check runs on the ARGUMENT, before the prefix goes on. */` |
|     3 | 12012 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 12013 | `			"GlobIterator::__construct(): Argument #1 ($pattern) must not be empty");` |
|     - | 12014 | `	}` |
|    29 | 12015 | `	PH7_MemObjInitFromString(pVm,&sUri,0);` |
|     - | 12016 | `	/* Both of php's prefix tests are CASE-SENSITIVE, where the wrapper LOOKUP` |
|     - | 12017 | ``	 * that follows is not: `GLOB://x` is prefixed again, so the pattern the`` |
|     - | 12018 | ``	 * device is finally handed still spells `GLOB://x` and matches nothing --`` |
|     - | 12019 | `	 * and the debug key shows the doubled string. */` |
|    28 | 12020 | `	if( nPat < (int)sizeof("glob://")-1` |
|    29 | 12021 | `	 \|\| SyMemcmp(zPat,"glob://",sizeof("glob://")-1) != 0 ){` |
|    27 | 12022 | `		PH7_MemObjStringAppend(&sUri,"glob://",sizeof("glob://")-1);` |
|    13 | 12023 | `	}` |
|    29 | 12024 | `	PH7_MemObjStringAppend(&sUri,zPat,(sxu32)nPat);` |
|    43 | 12025 | `	rc = SplDirConstructVal(pCtx,"GlobIterator","pattern",&sUri,` |
|    14 | 12026 | `		SplDirFlagArg(nArg,apArg,SDI_KEY_AS_PATHNAME\|SDI_CURRENT_AS_FILEINFO));` |
|    29 | 12027 | `	PH7_MemObjRelease(&sUri);` |
|    29 | 12028 | `	return rc;` |
|    16 | 12029 | `}` |
|     - | 12030 | `/*` |
|     - | 12031 | ` * GlobIterator::count(): php's php_glob_stream_get_count, which is the number` |
|     - | 12032 | ` * of MATCHES and does not move with the walk -- a pattern ending in a slash` |
|     - | 12033 | ` * counts its directories and yields none of them, because their entry is the` |
|     - | 12034 | ` * empty string a walk reads as the end.` |
|     - | 12035 | ` */` |
|    16 | 12036 | `static int vm_builtin_GlobIterator_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12037 | `{` |
|    17 | 12038 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 12039 | `	VmDirHandle *pH;` |
|     - | 12040 | `	sxi32 rcChk;` |
|     8 | 12041 | `	SXUNUSED(nArg);` |
|     8 | 12042 | `	SXUNUSED(apArg);` |
|    17 | 12043 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|     3 | 12044 | `		return rcChk;` |
|     - | 12045 | `	}` |
|    15 | 12046 | `	pH = SplDirFind(pCtx->pVm,pThis);` |
|    15 | 12047 | `	if( pH == 0 \|\| pH->pStream == 0 \|\| !PH7_GlobStreamIs(pH->pStream) ){` |
|     - | 12048 | `		/* php's own "should not happen", raised as a fatal there. */` |
|   ! 0 | 12049 | `		return PH7_VmThrowException(pCtx,"Error","GlobIterator lost glob state");` |
|     - | 12050 | `	}` |
|    15 | 12051 | `	ph7_result_int64(pCtx,PH7_GlobStreamCount(pH->pHandle));` |
|    15 | 12052 | `	return PH7_OK;` |
|     9 | 12053 | `}` |
|     - | 12054 | `/*` |
|     - | 12055 | ` * The four declarations. Method ORDER, signatures and tentative return types` |
|     - | 12056 | `` * are spl_directory.stub.php's; the four slots are php's `u.dir` arm and carry`` |
|     - | 12057 | ` * PH7_MOD_HIDDEN because php declares no property at all here. Each class` |
|     - | 12058 | ` * restates NOSERIALIZE and the presentation hook: a native subclass inherits` |
|     - | 12059 | ` * neither (rule 29).` |
|     - | 12060 | ` */` |
|  6721 | 12061 | `static sxi32 VmInstallSplDirIterators(ph7_vm *pVm)` |
|     5 | 12062 | `{` |
|     - | 12063 | `	static const PH7_NativePropDef aDirProp[] = {` |
|     - | 12064 | `		{ SDI_E, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|     - | 12065 | `		{ SDI_I, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 12066 | `		{ SDI_F, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 12067 | `		{ SDI_S, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|     - | 12068 | `	};` |
|     - | 12069 | `	static const PH7_NativeMethodDef aDirMethod[] = {` |
|     - | 12070 | `		{ "__construct",  PH7_MOD_PUBLIC, "string $directory", 0,` |
|     - | 12071 | `		  vm_builtin_DirectoryIterator_construct },` |
|     - | 12072 | `		{ "getFilename",  PH7_MOD_PUBLIC, "", "@string",` |
|     - | 12073 | `		  vm_builtin_DirectoryIterator_getFilename },` |
|     - | 12074 | `		{ "getExtension", PH7_MOD_PUBLIC, "", "@string",` |
|     - | 12075 | `		  vm_builtin_DirectoryIterator_getExtension },` |
|     - | 12076 | `		{ "getBasename",  PH7_MOD_PUBLIC, "string $suffix = \"\"", "@string",` |
|     - | 12077 | `		  vm_builtin_DirectoryIterator_getBasename },` |
|     - | 12078 | `		{ "isDot",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_DirectoryIterator_isDot },` |
|     - | 12079 | `		{ "rewind",       PH7_MOD_PUBLIC, "", "@void", vm_builtin_DirectoryIterator_rewind },` |
|     - | 12080 | `		{ "valid",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_DirectoryIterator_valid },` |
|     - | 12081 | `		{ "key",          PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_DirectoryIterator_key },` |
|     - | 12082 | `		{ "current",      PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_DirectoryIterator_current },` |
|     - | 12083 | `		{ "next",         PH7_MOD_PUBLIC, "", "@void", vm_builtin_DirectoryIterator_next },` |
|     - | 12084 | `		{ "seek",         PH7_MOD_PUBLIC, "int $offset", "@void",` |
|     - | 12085 | `		  vm_builtin_DirectoryIterator_seek },` |
|     - | 12086 | ``		/* php aliases this one to getFilename(), so `echo $it` prints the ENTRY where`` |
|     - | 12087 | `		 * SplFileInfo's __toString prints the whole pathname. Not tentative. */` |
|     - | 12088 | `		{ "__toString",   PH7_MOD_PUBLIC, "", "string",` |
|     - | 12089 | `		  vm_builtin_DirectoryIterator_getFilename },` |
|     - | 12090 | `	};` |
|     - | 12091 | `	static const PH7_NativeConstDef aFsConst[] = {` |
|     - | 12092 | `		{ "CURRENT_MODE_MASK",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_CURRENT_MODE_MASK, 0, 0.0 },` |
|     - | 12093 | `		{ "CURRENT_AS_PATHNAME", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_CURRENT_AS_PATHNAME, 0, 0.0 },` |
|     - | 12094 | `		{ "CURRENT_AS_FILEINFO", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_CURRENT_AS_FILEINFO, 0, 0.0 },` |
|     - | 12095 | `		{ "CURRENT_AS_SELF",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_CURRENT_AS_SELF, 0, 0.0 },` |
|     - | 12096 | `		{ "KEY_MODE_MASK",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_KEY_MODE_MASK, 0, 0.0 },` |
|     - | 12097 | `		{ "KEY_AS_PATHNAME",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_KEY_AS_PATHNAME, 0, 0.0 },` |
|     - | 12098 | `		{ "FOLLOW_SYMLINKS",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_FOLLOW_SYMLINKS, 0, 0.0 },` |
|     - | 12099 | `		{ "KEY_AS_FILENAME",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_KEY_AS_FILENAME, 0, 0.0 },` |
|     - | 12100 | `		{ "NEW_CURRENT_AND_KEY", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_KEY_AS_FILENAME\|SDI_CURRENT_AS_FILEINFO, 0, 0.0 },` |
|     - | 12101 | `		{ "OTHER_MODE_MASK",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_OTHERS_MASK, 0, 0.0 },` |
|     - | 12102 | `		{ "SKIP_DOTS",           PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_SKIPDOTS, 0, 0.0 },` |
|     - | 12103 | `		{ "UNIX_PATHS",          PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_UNIXPATHS, 0, 0.0 },` |
|     - | 12104 | `	};` |
|     - | 12105 | `	static const PH7_NativeMethodDef aFsMethod[] = {` |
|     - | 12106 | `		{ "__construct", PH7_MOD_PUBLIC,` |
|     - | 12107 | `		  "string $directory, int $flags = FilesystemIterator::KEY_AS_PATHNAME \| "` |
|     - | 12108 | `		  "FilesystemIterator::CURRENT_AS_FILEINFO \| FilesystemIterator::SKIP_DOTS", 0,` |
|     - | 12109 | `		  vm_builtin_FilesystemIterator_construct },` |
|     - | 12110 | `		{ "rewind",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_FilesystemIterator_rewind },` |
|     - | 12111 | `		{ "key",         PH7_MOD_PUBLIC, "", "@string", vm_builtin_FilesystemIterator_key },` |
|     - | 12112 | `		{ "current",     PH7_MOD_PUBLIC, "", "@SplFileInfo\|FilesystemIterator\|string",` |
|     - | 12113 | `		  vm_builtin_FilesystemIterator_current },` |
|     - | 12114 | `		{ "getFlags",    PH7_MOD_PUBLIC, "", "@int", vm_builtin_FilesystemIterator_getFlags },` |
|     - | 12115 | `		{ "setFlags",    PH7_MOD_PUBLIC, "int $flags", "@void",` |
|     - | 12116 | `		  vm_builtin_FilesystemIterator_setFlags },` |
|     - | 12117 | `	};` |
|     - | 12118 | `	static const PH7_NativeMethodDef aRdiMethod[] = {` |
|     - | 12119 | `		{ "__construct",    PH7_MOD_PUBLIC,` |
|     - | 12120 | `		  "string $directory, int $flags = FilesystemIterator::KEY_AS_PATHNAME \| "` |
|     - | 12121 | `		  "FilesystemIterator::CURRENT_AS_FILEINFO", 0,` |
|     - | 12122 | `		  vm_builtin_RecursiveDirectoryIterator_construct },` |
|     - | 12123 | `		{ "hasChildren",    PH7_MOD_PUBLIC, "bool $allowLinks = false", "@bool",` |
|     - | 12124 | `		  vm_builtin_RecursiveDirectoryIterator_hasChildren },` |
|     - | 12125 | `		{ "getChildren",    PH7_MOD_PUBLIC, "", "@RecursiveDirectoryIterator",` |
|     - | 12126 | `		  vm_builtin_RecursiveDirectoryIterator_getChildren },` |
|     - | 12127 | `		{ "getSubPath",     PH7_MOD_PUBLIC, "", "@string",` |
|     - | 12128 | `		  vm_builtin_RecursiveDirectoryIterator_getSubPath },` |
|     - | 12129 | `		{ "getSubPathname", PH7_MOD_PUBLIC, "", "@string",` |
|     - | 12130 | `		  vm_builtin_RecursiveDirectoryIterator_getSubPathname },` |
|     - | 12131 | `	};` |
|     - | 12132 | `	static const PH7_NativeMethodDef aGlobMethod[] = {` |
|     - | 12133 | `		{ "__construct", PH7_MOD_PUBLIC,` |
|     - | 12134 | `		  "string $pattern, int $flags = FilesystemIterator::KEY_AS_PATHNAME \| "` |
|     - | 12135 | `		  "FilesystemIterator::CURRENT_AS_FILEINFO", 0,` |
|     - | 12136 | `		  vm_builtin_GlobIterator_construct },` |
|     - | 12137 | `		{ "count",       PH7_MOD_PUBLIC, "", "@int", vm_builtin_GlobIterator_count },` |
|     - | 12138 | `	};` |
|     - | 12139 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - | 12140 | `		{ "DirectoryIterator", "SplFileInfo", "SeekableIterator", PH7_CLASS_NOSERIALIZE,` |
|     - | 12141 | `		  aDirMethod, SX_ARRAYSIZE(aDirMethod), 0, 0,` |
|     - | 12142 | `		  aDirProp, SX_ARRAYSIZE(aDirProp), SplDirClose, 0, SfiPresent },` |
|     - | 12143 | `		{ "FilesystemIterator", "DirectoryIterator", 0, PH7_CLASS_NOSERIALIZE,` |
|     - | 12144 | `		  aFsMethod, SX_ARRAYSIZE(aFsMethod), aFsConst, SX_ARRAYSIZE(aFsConst),` |
|     - | 12145 | `		  0, 0, SplDirClose, 0, SfiPresent },` |
|     - | 12146 | `		{ "RecursiveDirectoryIterator", "FilesystemIterator", "RecursiveIterator",` |
|     - | 12147 | `		  PH7_CLASS_NOSERIALIZE,` |
|     - | 12148 | `		  aRdiMethod, SX_ARRAYSIZE(aRdiMethod), 0, 0,` |
|     - | 12149 | `		  0, 0, SplDirClose, 0, SfiPresent },` |
|     - | 12150 | ``		/* php gives this one the `check` object handlers SplFileObject has, so it`` |
|     - | 12151 | `		 * is UNCLONEABLE and refuses every method -- inherited ones included --` |
|     - | 12152 | `		 * on an instance whose parent constructor never ran (SfoIsChecked). A` |
|     - | 12153 | `		 * native class inherits neither the refusals nor the hooks (rule 29), so` |
|     - | 12154 | `		 * both are restated here. */` |
|     - | 12155 | `		{ "GlobIterator", "FilesystemIterator", "Countable",` |
|     - | 12156 | `		  PH7_CLASS_NOSERIALIZE\|PH7_CLASS_NOCLONE,` |
|     - | 12157 | `		  aGlobMethod, SX_ARRAYSIZE(aGlobMethod), 0, 0,` |
|     - | 12158 | `		  0, 0, SplDirClose, 0, SfiPresent },` |
|     - | 12159 | `	};` |
|  6726 | 12160 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 | 12161 | `}` |
|     - | 12162 | `/*` |
|     - | 12163 | ` * ---------------------------------------------------------------------------` |
|     - | 12164 | ` * SplFileObject.` |
|     - | 12165 | ` *` |
|     - | 12166 | `` * php's SPL_FS_FILE arm of the same `spl_filesystem_object`: an OPEN stream plus`` |
|     - | 12167 | ` * ONE line of look-ahead. The line is the whole model, and nearly every rule` |
|     - | 12168 | ` * below is about which of the two current values is live rather than about IO.` |
|     - | 12169 | `` * php keeps `current_line` (a string) and `current_zval` (the READ_CSV array)`` |
|     - | 12170 | `` * side by side, either or both present, and `current()` picks between them; the`` |
|     - | 12171 | ``  * line NUMBER is bumped by whatever read produced them, which is why `key()` `` |
|     - | 12172 | ``  * counts differently for `fgetc()` (only a `\n` advances it), for `fgets()` `` |
|     - | 12173 | `` * (always) and for `current()` (only when a line was already there).`` |
|     - | 12174 | ` *` |
|     - | 12175 | ` * The stream lives in a hidden slot as a RESOURCE rather than on a registry the` |
|     - | 12176 | ` * way a DirectoryIterator's DIR does, because php makes this class UNCLONEABLE` |
|     - | 12177 | `` * (its `check` object handlers null the clone handler out) -- so no second`` |
|     - | 12178 | ` * object can ever reach the same handle, and the class's xRelease is the only` |
|     - | 12179 | ` * teardown there is. That is also php's flush point: a file written through an` |
|     - | 12180 | ` * SplFileObject is finished when the OBJECT dies.` |
|     - | 12181 | ` *` |
|     - | 12182 | `` * Those same `check` handlers give the class a get_method that refuses EVERY`` |
|     - | 12183 | ` * method -- the ones inherited from SplFileInfo included -- on an instance whose` |
|     - | 12184 | ` * parent constructor never ran (a subclass that forgets it, a caught` |
|     - | 12185 | ` * constructor failure). SfiReady() below is where that check lives, so the` |
|     - | 12186 | ` * inherited accessors carry it too.` |
|     - | 12187 | ` * ---------------------------------------------------------------------------` |
|     - | 12188 | ` */` |
|     - | 12189 | `/* php's spl_directory.h SPL_FILE_OBJECT_* flags, and the mask getFlags() cuts` |
|     - | 12190 | ` * the stored word with. */` |
|     - | 12191 | `#define SFO_DROP_NEW_LINE 0x0001` |
|     - | 12192 | `#define SFO_READ_AHEAD    0x0002` |
|     - | 12193 | `#define SFO_SKIP_EMPTY    0x0004` |
|     - | 12194 | `#define SFO_READ_CSV      0x0008` |
|     - | 12195 | `#define SFO_FLAGS_MASK    0x000F` |
|     - | 12196 | `/* Which of php's two current values this instance is holding. Both may be live` |
|     - | 12197 | ` * at once: a CSV read keeps the raw line beside the parsed array. */` |
|     - | 12198 | `#define SFO_HAS_LINE 0x1` |
|     - | 12199 | `#define SFO_HAS_ZVAL 0x2` |
|     - | 12200 | ``/* The state slots are named beside SplFileInfo's, up with the `u.dir` arm: they`` |
|     - | 12201 | ` * are all hidden (php declares no property on this class either) and three of` |
|     - | 12202 | ` * them are what the shared presentation hook shows.` |
|     - | 12203 | ` *` |
|     - | 12204 | ` * What a read attempt did. php's zend_result plus the third case a THROW is:` |
|     - | 12205 | ` * the read routines run user code (a subclass's getCurrentLine) and raise` |
|     - | 12206 | ` * php's own RuntimeException, so a caller has to be able to abandon. */` |
|     - | 12207 | `#define SFO_READ_OK    0` |
|     - | 12208 | `#define SFO_READ_FAIL  1` |
|     - | 12209 | `#define SFO_READ_THROW 2` |
|     - | 12210 |  |
|     - | 12211 | `/* This class's own getCurrentLine body, named ahead of the read routine that` |
|     - | 12212 | ` * has to tell it apart from a subclass's override. */` |
|     - | 12213 | `static int vm_builtin_SplFileObject_fgets(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|     - | 12214 | ``/* php's `!intern->u.file.stream`: the uninitialized object. */`` |
|  1532 | 12215 | `static io_private * SfoDev(ph7_class_instance *pThis)` |
|     1 | 12216 | `{` |
|  1533 | 12217 | `	ph7_value *pVal = pThis ? PH7_NativeAttr(pThis,SFO_H) : 0;` |
|     - | 12218 | `	io_private *pDev;` |
|  1533 | 12219 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_RES) == 0 ){` |
|   351 | 12220 | `		return 0;` |
|     - | 12221 | `	}` |
|  1183 | 12222 | `	pDev = (io_private *)pVal->x.pOther;` |
|  1183 | 12223 | `	return IO_PRIVATE_INVALID(pDev) ? 0 : pDev;` |
|   767 | 12224 | `}` |
|     - | 12225 | ``/* Is this instance one of the classes php gives the `check` handlers to? Asked`` |
|     - | 12226 | ` * as a CLASS question for the same reason SplDirIs() is: a user class may` |
|     - | 12227 | ` * declare anything it likes. php gives them to TWO: SplFileObject and` |
|     - | 12228 | ` * GlobIterator, which is why an unconstructed GlobIterator refuses every method` |
|     - | 12229 | ` * with this sentence where an unconstructed FilesystemIterator answers` |
|     - | 12230 | `` * `Object not initialized` -- and why neither of the two can be cloned. */`` |
|  1523 | 12231 | `static int SfoIsChecked(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     2 | 12232 | `{` |
|     - | 12233 | `	ph7_class *pFile;` |
|  1525 | 12234 | `	if( pThis == 0 ){` |
|   ! 0 | 12235 | `		return 0;` |
|     - | 12236 | `	}` |
|  1525 | 12237 | `	pFile = PH7_VmExtractClass(pVm,"SplFileObject",sizeof("SplFileObject")-1,FALSE,0);` |
|  1525 | 12238 | `	if( pFile && PH7_VmInstanceOf(pThis->pClass,pFile) ){` |
|   345 | 12239 | `		return 1;` |
|     - | 12240 | `	}` |
|  1181 | 12241 | `	pFile = PH7_VmExtractClass(pVm,"GlobIterator",sizeof("GlobIterator")-1,FALSE,0);` |
|  1181 | 12242 | `	return pFile && PH7_VmInstanceOf(pThis->pClass,pFile) ? 1 : 0;` |
|   767 | 12243 | `}` |
|     - | 12244 | `/*` |
|     - | 12245 | ` * php's spl_filesystem_object_get_method_check, as a guard the bodies call:` |
|     - | 12246 | `` * an instance of a `check` class with nothing open answers NO method at all.`` |
|     - | 12247 | ` * Answers 0 when the caller must return *pRc.` |
|     - | 12248 | ` */` |
|     - | 12249 | `/*` |
|     - | 12250 | `` * php's `u.file.stream == NULL && orig_path == NULL`. The two classes that`` |
|     - | 12251 | ` * carry the check handlers fill different halves of it. SplFileObject has an` |
|     - | 12252 | ` * open BYTE stream -- and NO orig_path when its open FAILED, which is why a` |
|     - | 12253 | ` * subclass that catches its own parent constructor's exception is left` |
|     - | 12254 | ` * refusing every method. GlobIterator has a path instead: the whole` |
|     - | 12255 | `` * `glob://pattern`, written by the open whether the pattern matched anything`` |
|     - | 12256 | ` * or not, so an iterator over nothing is still a working object.` |
|     - | 12257 | ` */` |
|   378 | 12258 | `static int SfoCheckReady(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 12259 | `{` |
|   379 | 12260 | `	int nPath = 0;` |
|   379 | 12261 | `	if( SfoDev(pThis) != 0 ){` |
|   213 | 12262 | `		return 1;` |
|     - | 12263 | `	}` |
|   167 | 12264 | `	if( !SplGlobIs(pVm,pThis) ){` |
|    11 | 12265 | `		return 0;` |
|     - | 12266 | `	}` |
|   157 | 12267 | `	SfiStr(pThis,SFI_P,&nPath);` |
|   157 | 12268 | `	return nPath > 0;` |
|   190 | 12269 | `}` |
|  1401 | 12270 | `static int SfoChecked(ph7_context *pCtx,sxi32 *pRc)` |
|     2 | 12271 | `{` |
|  1403 | 12272 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|  1403 | 12273 | `	*pRc = PH7_OK;` |
|  1403 | 12274 | `	if( SfoIsChecked(pCtx->pVm,pThis) && !SfoCheckReady(pCtx->pVm,pThis) ){` |
|    31 | 12275 | `		*pRc = PH7_VmThrowException(pCtx,"Error",` |
|     - | 12276 | `			"The parent constructor was not called: the object is in an invalid state");` |
|    31 | 12277 | `		return 0;` |
|     - | 12278 | `	}` |
|  1373 | 12279 | `	return 1;` |
|   706 | 12280 | `}` |
|   804 | 12281 | `static sxi64 SfoFlags(ph7_class_instance *pThis)` |
|     1 | 12282 | `{` |
|   805 | 12283 | `	return PH7_NativeAttrInt(pThis,SFO_FL);` |
|     1 | 12284 | `}` |
|     - | 12285 | `/* php's spl_filesystem_file_free_line: BOTH current values go. */` |
|   360 | 12286 | `static void SfoFreeLine(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 12287 | `{` |
|     - | 12288 | `	ph7_value *pZ;` |
|   361 | 12289 | `	PH7_NativeSetAttrStr(pVm,pThis,SFO_L,"",0);` |
|   361 | 12290 | `	pZ = PH7_NativeAttr(pThis,SFO_Z);` |
|   361 | 12291 | `	if( pZ ){` |
|   361 | 12292 | `		PH7_MemObjRelease(pZ);` |
|   180 | 12293 | `	}` |
|   361 | 12294 | `	PH7_NativeSetAttrInt(pVm,pThis,SFO_LS,0);` |
|   361 | 12295 | `}` |
|     - | 12296 | `/* The current line's bytes. Only meaningful while SFO_HAS_LINE is set. */` |
|   212 | 12297 | `static const char * SfoLine(ph7_class_instance *pThis,int *pnLen)` |
|     1 | 12298 | `{` |
|   213 | 12299 | `	return SfiStr(pThis,SFO_L,pnLen);` |
|     1 | 12300 | `}` |
|     - | 12301 | `/*` |
|     - | 12302 | ` * php's spl_filesystem_file_read_ex: drop what is held, refuse at EOF (loudly` |
|     - | 12303 | ` * unless silent), take ONE line, apply DROP_NEW_LINE unless this is the CSV` |
|     - | 12304 | `` * path, and add `iLineAdd` to the line number.`` |
|     - | 12305 | ` */` |
|   176 | 12306 | `static int SfoReadEx(ph7_context *pCtx,int bSilent,int iLineAdd,int bCsv,sxi32 *pRc)` |
|     1 | 12307 | `{` |
|   177 | 12308 | `	ph7_vm *pVm = pCtx->pVm;` |
|   177 | 12309 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   177 | 12310 | `	io_private *pDev = SfoDev(pThis);` |
|   177 | 12311 | `	sxi64 iMax = PH7_NativeAttrInt(pThis,SFO_ML);` |
|   177 | 12312 | `	const char *zLine = 0;` |
|     - | 12313 | `	ph7_int64 n;` |
|   177 | 12314 | `	*pRc = PH7_OK;` |
|   177 | 12315 | `	SfoFreeLine(pVm,pThis);` |
|   177 | 12316 | `	if( pDev == 0 \|\| PH7_StreamAtEof(pDev) ){` |
|    17 | 12317 | `		if( !bSilent ){` |
|     3 | 12318 | `			int nName = 0;` |
|     3 | 12319 | `			const char *zName = SfiName(pVm,pThis,&nName);` |
|     4 | 12320 | `			*pRc = PH7_VmThrowException(pCtx,"RuntimeException",` |
|     1 | 12321 | `				"Cannot read from file %.*s",nName,zName);` |
|     3 | 12322 | `			return SFO_READ_THROW;` |
|     - | 12323 | `		}` |
|    15 | 12324 | `		return SFO_READ_FAIL;` |
|     - | 12325 | `	}` |
|   161 | 12326 | `	n = StreamReadLine(pDev,&zLine,iMax > 0 ? (ph7_int64)iMax : 0);` |
|   161 | 12327 | `	if( n < 1 ){` |
|     - | 12328 | `		/* The device's own notice, worded from THIS method: a read on a handle` |
|     - | 12329 | `		 * opened write-only says so here as it does from fgets(). */` |
|    27 | 12330 | `		StreamReportReadFailure(pCtx,pDev);` |
|     - | 12331 | `		/* php's buf == NULL: the line is the EMPTY string, not an absence. */` |
|    27 | 12332 | `		PH7_NativeSetAttrStr(pVm,pThis,SFO_L,"",0);` |
|    14 | 12333 | `	}else{` |
|   135 | 12334 | `		if( !bCsv && (SfoFlags(pThis) & SFO_DROP_NEW_LINE) ){` |
|    25 | 12335 | `			if( zLine[n-1] == '\n' ){` |
|    25 | 12336 | `				n--;` |
|    25 | 12337 | `				if( n > 0 && zLine[n-1] == '\r' ){` |
|   ! 0 | 12338 | `					n--;` |
|   ! 0 | 12339 | `				}` |
|    12 | 12340 | `			}` |
|    12 | 12341 | `		}` |
|   135 | 12342 | `		PH7_NativeSetAttrStr(pVm,pThis,SFO_L,zLine,(int)n);` |
|     - | 12343 | `	}` |
|   161 | 12344 | `	PH7_NativeSetAttrInt(pVm,pThis,SFO_LS,SFO_HAS_LINE);` |
|   161 | 12345 | `	PH7_NativeSetAttrInt(pVm,pThis,SFO_K,PH7_NativeAttrInt(pThis,SFO_K) + iLineAdd);` |
|   161 | 12346 | `	return SFO_READ_OK;` |
|    89 | 12347 | `}` |
|     - | 12348 | `/* php's spl_filesystem_file_read: the line number advances only when a line was` |
|     - | 12349 | ` * ALREADY there, which is what makes the first current() answer key 0. */` |
|   136 | 12350 | `static int SfoReadOne(ph7_context *pCtx,int bSilent,int bCsv,sxi32 *pRc)` |
|     1 | 12351 | `{` |
|   137 | 12352 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   137 | 12353 | `	int iAdd = (PH7_NativeAttrInt(pThis,SFO_LS) & SFO_HAS_LINE) ? 1 : 0;` |
|   137 | 12354 | `	return SfoReadEx(pCtx,bSilent,iAdd,bCsv,pRc);` |
|     1 | 12355 | `}` |
|     - | 12356 | `/* php's is_line_empty: an empty line, or -- under READ_CSV\|DROP_NEW_LINE, whose` |
|     - | 12357 | ` * combination does NOT strip the newline -- a line that is only one. */` |
|    56 | 12358 | `static int SfoLineEmpty(ph7_class_instance *pThis)` |
|     1 | 12359 | `{` |
|    57 | 12360 | `	int nLine = 0;` |
|    57 | 12361 | `	const char *zLine = SfoLine(pThis,&nLine);` |
|    57 | 12362 | `	sxi64 iFlags = SfoFlags(pThis);` |
|    57 | 12363 | `	if( nLine == 0 ){` |
|    13 | 12364 | `		return 1;` |
|     - | 12365 | `	}` |
|    45 | 12366 | `	if( (iFlags & SFO_READ_CSV) && (iFlags & SFO_DROP_NEW_LINE) ){` |
|    15 | 12367 | `		return (nLine == 1 && zLine[0] == '\n')` |
|    15 | 12368 | `			\|\| (nLine == 2 && zLine[0] == '\r' && zLine[1] == '\n');` |
|     - | 12369 | `	}` |
|    31 | 12370 | `	return 0;` |
|    29 | 12371 | `}` |
|     - | 12372 | `/*` |
|     - | 12373 | ` * php's spl_filesystem_file_read_csv: read lines until one is not empty (when` |
|     - | 12374 | ` * SKIP_EMPTY says so), then parse the RECORD -- which may run past the line,` |
|     - | 12375 | ` * because an open enclosure carries the newline inside the value.` |
|     - | 12376 | ` */` |
|    28 | 12377 | `static int SfoReadCsv(ph7_context *pCtx,int delim,int encl,int escape,ph7_value *pOut,` |
|     - | 12378 | `	int bSilent,sxi32 *pRc)` |
|     1 | 12379 | `{` |
|    29 | 12380 | `	ph7_vm *pVm = pCtx->pVm;` |
|    29 | 12381 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 12382 | `	io_private *pDev;` |
|     - | 12383 | `	ph7_value *pSlot;` |
|     - | 12384 | `	ph7_value sArray;` |
|     - | 12385 | `	SyBlob sRec;` |
|     - | 12386 | `	PH7_CsvScan sScan;` |
|    29 | 12387 | `	int nLine = 0,rc;` |
|     - | 12388 | `	const char *zLine;` |
|    14 | 12389 | `	do{` |
|    33 | 12390 | `		rc = SfoReadOne(pCtx,bSilent,TRUE,pRc);` |
|    33 | 12391 | `		if( rc != SFO_READ_OK ){` |
|     3 | 12392 | `			return rc;` |
|     - | 12393 | `		}` |
|    31 | 12394 | `	}while( SfoLineEmpty(pThis) && (SfoFlags(pThis) & SFO_SKIP_EMPTY) );` |
|    27 | 12395 | `	PH7_MemObjInit(pVm,&sArray);` |
|    27 | 12396 | `	if( PH7_MemObjToHashmap(&sArray) != SXRET_OK ){` |
|   ! 0 | 12397 | `		PH7_MemObjRelease(&sArray);` |
|   ! 0 | 12398 | `		*pRc = PH7_ContextMemoryError(pCtx);` |
|   ! 0 | 12399 | `		return SFO_READ_THROW;` |
|     - | 12400 | `	}` |
|    27 | 12401 | `	zLine = SfoLine(pThis,&nLine);` |
|    27 | 12402 | `	SyBlobInit(&sRec,&pVm->sAllocator);` |
|    27 | 12403 | `	SyBlobAppend(&sRec,(const void *)zLine,(sxu32)nLine);` |
|    27 | 12404 | `	pDev = SfoDev(pThis);` |
|    27 | 12405 | `	PH7_CsvScanInit(&sScan);` |
|    27 | 12406 | `	while( pDev && PH7_CsvScanOpen(&sScan,(const char *)SyBlobData(&sRec),` |
|    13 | 12407 | `			SyBlobLength(&sRec),delim,encl,escape) ){` |
|     - | 12408 | `		ph7_int64 n;` |
|   ! 0 | 12409 | `		if( SyBlobLength(&sRec) >= (sxu32)SXI32_HIGH ){` |
|   ! 0 | 12410 | `			break;   /* the parser measures in int; stop rather than wrap negative */` |
|     - | 12411 | `		}` |
|   ! 0 | 12412 | `		n = StreamReadLine(pDev,&zLine,0);` |
|   ! 0 | 12413 | `		if( n < 1 ){` |
|   ! 0 | 12414 | `			break;   /* EOF inside the enclosure: php answers what it has */` |
|     - | 12415 | `		}` |
|   ! 0 | 12416 | `		SyBlobAppend(&sRec,(const void *)zLine,(sxu32)n);` |
|   ! 0 | 12417 | `	}` |
|    40 | 12418 | `	PH7_ProcessCsv(&sArray,(const char *)SyBlobData(&sRec),(int)SyBlobLength(&sRec),` |
|    13 | 12419 | `		delim,encl,escape,0);` |
|    27 | 12420 | `	SyBlobRelease(&sRec);` |
|    27 | 12421 | `	pSlot = PH7_NativeAttr(pThis,SFO_Z);` |
|    27 | 12422 | `	if( pSlot ){` |
|    27 | 12423 | `		PH7_MemObjStore(&sArray,pSlot);` |
|    13 | 12424 | `	}` |
|    40 | 12425 | `	PH7_NativeSetAttrInt(pVm,pThis,SFO_LS,` |
|    26 | 12426 | `		(sxi64)(PH7_NativeAttrInt(pThis,SFO_LS) \| SFO_HAS_ZVAL));` |
|    27 | 12427 | `	if( pOut ){` |
|    11 | 12428 | `		ph7_value *pKeep = PH7_NativeAttr(pThis,SFO_Z);` |
|    11 | 12429 | `		if( pKeep ){` |
|    11 | 12430 | `			PH7_MemObjLoad(pKeep,pOut);` |
|     5 | 12431 | `		}` |
|     5 | 12432 | `	}` |
|    27 | 12433 | `	PH7_MemObjRelease(&sArray);` |
|    27 | 12434 | `	return SFO_READ_OK;` |
|    15 | 12435 | `}` |
|     - | 12436 | `/* This instance's three CSV settings. */` |
|    38 | 12437 | `static void SfoCsvControl(ph7_class_instance *pThis,int *pDelim,int *pEncl,int *pEsc)` |
|     1 | 12438 | `{` |
|    39 | 12439 | `	*pDelim = (int)PH7_NativeAttrInt(pThis,SFO_D);` |
|    39 | 12440 | `	*pEncl  = (int)PH7_NativeAttrInt(pThis,SFO_EN);` |
|    39 | 12441 | `	*pEsc   = (int)PH7_NativeAttrInt(pThis,SFO_ES);` |
|    39 | 12442 | `}` |
|     - | 12443 | `/*` |
|     - | 12444 | ` * php's spl_filesystem_file_read_line_ex, whose middle branch is the one only a` |
|     - | 12445 | ` * differential finds: a SUBCLASS that overrides getCurrentLine() drives the` |
|     - | 12446 | ` * iteration, so the line every accessor sees is whatever that method answered` |
|     - | 12447 | ` * -- and the line number is bumped once by the read the method made and again` |
|     - | 12448 | ` * here, which is why such a subclass counts in threes.` |
|     - | 12449 | ` */` |
|   136 | 12450 | `static int SfoReadLineEx(ph7_context *pCtx,int bSilent,sxi32 *pRc)` |
|     1 | 12451 | `{` |
|   137 | 12452 | `	ph7_vm *pVm = pCtx->pVm;` |
|   137 | 12453 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 12454 | `	ph7_class_method *pCur;` |
|     - | 12455 | `	int delim,encl,escape;` |
|   137 | 12456 | `	*pRc = PH7_OK;` |
|   137 | 12457 | `	if( SfoFlags(pThis) & SFO_READ_CSV ){` |
|    19 | 12458 | `		SfoCsvControl(pThis,&delim,&encl,&escape);` |
|    19 | 12459 | `		return SfoReadCsv(pCtx,delim,encl,escape,0,bSilent,pRc);` |
|     - | 12460 | `	}` |
|     - | 12461 | `	/* php compares the resolved method's declaring SCOPE with SplFileObject; a` |
|     - | 12462 | `	 * ph7_class_method carries no such pointer, and the C BODY answers the same` |
|     - | 12463 | `	 * question -- this class's getCurrentLine IS its fgets. */` |
|   119 | 12464 | `	pCur = PH7_ClassExtractMethod(pThis->pClass,"getCurrentLine",sizeof("getCurrentLine")-1);` |
|   119 | 12465 | `	if( pCur && (pCur->sFunc.pNative == 0` |
|   109 | 12466 | `	          \|\| pCur->sFunc.pNative->xFunc != vm_builtin_SplFileObject_fgets) ){` |
|     - | 12467 | `		io_private *pDev;` |
|     - | 12468 | `		ph7_value sRet;` |
|     - | 12469 | `		sxi32 rc;` |
|    19 | 12470 | `		SfoFreeLine(pVm,pThis);` |
|    19 | 12471 | `		pDev = SfoDev(pThis);` |
|    19 | 12472 | `		if( pDev == 0 \|\| PH7_StreamAtEof(pDev) ){` |
|   ! 0 | 12473 | `			if( !bSilent ){` |
|   ! 0 | 12474 | `				int nName = 0;` |
|   ! 0 | 12475 | `				const char *zName = SfiName(pVm,pThis,&nName);` |
|   ! 0 | 12476 | `				*pRc = PH7_VmThrowException(pCtx,"RuntimeException",` |
|   ! 0 | 12477 | `					"Cannot read from file %.*s",nName,zName);` |
|   ! 0 | 12478 | `				return SFO_READ_THROW;` |
|     - | 12479 | `			}` |
|   ! 0 | 12480 | `			return SFO_READ_FAIL;` |
|     - | 12481 | `		}` |
|    19 | 12482 | `		PH7_MemObjInit(pVm,&sRet);` |
|    19 | 12483 | `		rc = PH7_VmCallClassMethod(pVm,pThis,pCur,&sRet,0,0);` |
|    19 | 12484 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 12485 | `			PH7_MemObjRelease(&sRet);` |
|   ! 0 | 12486 | `			*pRc = rc;` |
|   ! 0 | 12487 | `			return SFO_READ_THROW;` |
|     - | 12488 | `		}` |
|    19 | 12489 | `		if( (sRet.iFlags & MEMOBJ_STRING) == 0 ){` |
|     - | 12490 | ``			/* php's own TypeError: the declared `: string` return is checked by`` |
|     - | 12491 | `			 * the CALLER here, because the method may have no declared type. */` |
|   ! 0 | 12492 | `			const char *zGot = PH7_MemObjTypeDump(&sRet);` |
|   ! 0 | 12493 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 12494 | `				"%z::getCurrentLine(): Return value must be of type string, %s returned",` |
|   ! 0 | 12495 | `				&pThis->pClass->sName,zGot);` |
|   ! 0 | 12496 | `			PH7_MemObjRelease(&sRet);` |
|   ! 0 | 12497 | `			return SFO_READ_THROW;` |
|     - | 12498 | `		}` |
|    19 | 12499 | `		if( PH7_NativeAttrInt(pThis,SFO_LS) != 0 ){` |
|    11 | 12500 | `			PH7_NativeSetAttrInt(pVm,pThis,SFO_K,PH7_NativeAttrInt(pThis,SFO_K) + 1);` |
|     5 | 12501 | `		}` |
|    19 | 12502 | `		SfoFreeLine(pVm,pThis);` |
|     - | 12503 | `		{` |
|    19 | 12504 | `			int nRet = 0;` |
|    19 | 12505 | `			const char *zRet = ph7_value_to_string(&sRet,&nRet);` |
|    19 | 12506 | `			PH7_NativeSetAttrStr(pVm,pThis,SFO_L,zRet,nRet);` |
|     - | 12507 | `		}` |
|    19 | 12508 | `		PH7_NativeSetAttrInt(pVm,pThis,SFO_LS,SFO_HAS_LINE);` |
|    19 | 12509 | `		PH7_MemObjRelease(&sRet);` |
|    19 | 12510 | `		return SFO_READ_OK;` |
|     - | 12511 | `	}` |
|   101 | 12512 | `	return SfoReadOne(pCtx,bSilent,FALSE,pRc);` |
|    69 | 12513 | `}` |
|     - | 12514 | `/* php's spl_filesystem_file_read_line: the SKIP_EMPTY loop around it. */` |
|   130 | 12515 | `static int SfoReadLine(ph7_context *pCtx,int bSilent,sxi32 *pRc)` |
|     1 | 12516 | `{` |
|   131 | 12517 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   131 | 12518 | `	int rc = SfoReadLineEx(pCtx,bSilent,pRc);` |
|   137 | 12519 | `	while( (SfoFlags(pThis) & SFO_SKIP_EMPTY) && rc == SFO_READ_OK && SfoLineEmpty(pThis) ){` |
|     7 | 12520 | `		SfoFreeLine(pCtx->pVm,pThis);` |
|     7 | 12521 | `		rc = SfoReadLineEx(pCtx,bSilent,pRc);` |
|     1 | 12522 | `	}` |
|   131 | 12523 | `	return rc;` |
|     1 | 12524 | `}` |
|     - | 12525 | `/* php's spl_filesystem_file_rewind: seek to 0, drop the line, reset the count,` |
|     - | 12526 | ` * and read one ahead when READ_AHEAD asks. */` |
|    30 | 12527 | `static sxi32 SfoRewind(ph7_context *pCtx)` |
|     1 | 12528 | `{` |
|    31 | 12529 | `	ph7_vm *pVm = pCtx->pVm;` |
|    31 | 12530 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    31 | 12531 | `	io_private *pDev = SfoDev(pThis);` |
|    31 | 12532 | `	sxi32 rc = PH7_OK;` |
|    31 | 12533 | `	if( pDev == 0 ){` |
|   ! 0 | 12534 | `		return PH7_VmThrowException(pCtx,"Error","Object not initialized");` |
|     - | 12535 | `	}` |
|    31 | 12536 | `	if( PH7_StreamSeekWrapped(pDev,0,0 /* SEEK_SET */) != PH7_OK ){` |
|   ! 0 | 12537 | `		int nName = 0;` |
|   ! 0 | 12538 | `		const char *zName = SfiName(pVm,pThis,&nName);` |
|   ! 0 | 12539 | `		return PH7_VmThrowException(pCtx,"RuntimeException","Cannot rewind file %.*s",` |
|   ! 0 | 12540 | `			nName,zName);` |
|     - | 12541 | `	}` |
|    31 | 12542 | `	SfoFreeLine(pVm,pThis);` |
|    31 | 12543 | `	PH7_NativeSetAttrInt(pVm,pThis,SFO_K,0);` |
|    31 | 12544 | `	if( SfoFlags(pThis) & SFO_READ_AHEAD ){` |
|     5 | 12545 | `		SfoReadLine(pCtx,TRUE,&rc);` |
|     2 | 12546 | `	}` |
|    31 | 12547 | `	return rc;` |
|    16 | 12548 | `}` |
|     - | 12549 | `/*` |
|     - | 12550 | ` * The open both SplFileObject::__construct and SplFileInfo::openFile() are.` |
|     - | 12551 | ` * php promotes the warning its stream opener would print to a RuntimeException` |
|     - | 12552 | ` * (zend_replace_error_handling), so the text is the warning's -- worded from` |
|     - | 12553 | ` * this context's own qualified name, which is what the caller reports.` |
|     - | 12554 | ` */` |
|   150 | 12555 | `static sxi32 SfoOpen(ph7_context *pCtx,ph7_class_instance *pThis,ph7_value *pPath,` |
|     - | 12556 | `	const char *zMode,int nMode,int bUseInclude,ph7_value *pCtxArg,int iCtxArg)` |
|     1 | 12557 | `{` |
|   151 | 12558 | `	ph7_vm *pVm = pCtx->pVm;` |
|   151 | 12559 | `	const ph7_vfs *pVfs = pVm->pEngine->pVfs;` |
|     - | 12560 | `	phl_stream_ctx *pCtxRes;` |
|     - | 12561 | `	io_private *pDev;` |
|     - | 12562 | `	ph7_value *pSlot;` |
|     - | 12563 | `	const char *zErrUri,*zPath;` |
|     - | 12564 | `	ph7_value *apCtx[4];` |
|   151 | 12565 | `	int nPath = 0,iErr = 0,bThrew = 0,i;` |
|   151 | 12566 | `	zPath = ph7_value_to_string(pPath,&nPath);` |
|     - | 12567 | `	/* PH7_StreamCtxFromArg words its refusal from an argument VECTOR position,` |
|     - | 12568 | `	 * so the context is presented at the index php blames. */` |
|   751 | 12569 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apCtx) ; ++i ){` |
|   601 | 12570 | `		apCtx[i] = pPath;` |
|   301 | 12571 | `	}` |
|   151 | 12572 | `	if( iCtxArg >= 1 && iCtxArg <= (int)SX_ARRAYSIZE(apCtx) ){` |
|   127 | 12573 | `		apCtx[iCtxArg - 1] = pCtxArg;` |
|    63 | 12574 | `	}` |
|     - | 12575 | `	/* php's order, and each step is observable from the one before it: the NUL` |
|     - | 12576 | `	 * is ZPP's and comes first, then the already-open refusal, then the` |
|     - | 12577 | `	 * directory stat, and only then the EMPTY path -- which is the stream` |
|     - | 12578 | `	 * opener's ValueError rather than the constructor's. */` |
|   151 | 12579 | `	if( SyByteFind(zPath,(sxu32)nPath,'\0',0) == SXRET_OK ){` |
|     4 | 12580 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 12581 | `			"%s(): Argument #1 ($filename) must not contain any null bytes",` |
|     1 | 12582 | `			ph7_function_name(pCtx));` |
|     - | 12583 | `	}` |
|   149 | 12584 | `	if( SfoDev(pThis) != 0 ){` |
|     5 | 12585 | `		return PH7_VmThrowException(pCtx,"Error","Cannot call constructor twice");` |
|     - | 12586 | `	}` |
|   145 | 12587 | `	if( pVfs && pVfs->xIsdir && nPath > 0 && nPath < 4096 ){` |
|     - | 12588 | `		char zBuf[4096];` |
|   141 | 12589 | `		SyMemcpy(zPath,zBuf,(sxu32)nPath);` |
|   141 | 12590 | `		zBuf[nPath] = 0;` |
|   141 | 12591 | `		if( pVfs->xIsdir(zBuf) == PH7_OK ){` |
|     5 | 12592 | `			return PH7_VmThrowException(pCtx,"LogicException",` |
|     - | 12593 | `				"Cannot use SplFileObject with directories");` |
|     - | 12594 | `		}` |
|    68 | 12595 | `	}` |
|   141 | 12596 | `	if( nPath < 1 ){` |
|     5 | 12597 | `		return PH7_VmThrowException(pCtx,"ValueError","Path must not be empty");` |
|     - | 12598 | `	}` |
|   137 | 12599 | `	pCtxRes = pCtxArg` |
|     6 | 12600 | `		? PH7_StreamCtxFromArg(pCtx,iCtxArg,apCtx,iCtxArg - 1,"$context",0,&bThrew)` |
|   133 | 12601 | `		: PH7_StreamCtxDefault(pVm);` |
|   137 | 12602 | `	if( bThrew ){` |
|     5 | 12603 | `		return PH7_OK;` |
|     - | 12604 | `	}` |
|   133 | 12605 | `	pDev = PH7_StreamOpenPath(pCtx,pPath,zMode,nMode,bUseInclude,pCtxRes,pCtxArg,` |
|     - | 12606 | `		&iErr,&zErrUri);` |
|   133 | 12607 | `	if( pDev == 0 ){` |
|     7 | 12608 | `		if( iErr == PH7_STREAM_OPEN_NODEVICE ){` |
|   ! 0 | 12609 | `			int nScheme = 0;` |
|   ! 0 | 12610 | `			if( PH7_VmStreamDeviceIsRemoteHost(zPath,nPath,&nScheme) ){` |
|   ! 0 | 12611 | `				return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     - | 12612 | `					"%s(%.*s): Failed to open stream: no suitable wrapper could be found",` |
|   ! 0 | 12613 | `					ph7_function_name(pCtx),nPath,zPath);` |
|     - | 12614 | `			}` |
|   ! 0 | 12615 | `			return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     - | 12616 | `				"%s(): Unable to find the wrapper \"%.*s\" - did you forget to enable it "` |
|   ! 0 | 12617 | `				"when you configured PHP?",ph7_function_name(pCtx),nScheme,zPath);` |
|     - | 12618 | `		}` |
|     7 | 12619 | `		if( iErr == PH7_STREAM_OPEN_BADMODE ){` |
|     3 | 12620 | `			return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     - | 12621 | ``				"%s(%s): Failed to open stream: `%.*s' is not a valid mode for fopen",`` |
|     2 | 12622 | `				ph7_function_name(pCtx),zErrUri ? zErrUri : "",nMode,zMode);` |
|     - | 12623 | `		}` |
|     5 | 12624 | `		if( iErr == PH7_STREAM_OPEN_NOMEM ){` |
|   ! 0 | 12625 | `			return PH7_ContextMemoryError(pCtx);` |
|     - | 12626 | `		}` |
|     5 | 12627 | `		return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     2 | 12628 | `			"%s(%s): Failed to open stream: %s",ph7_function_name(pCtx),` |
|     4 | 12629 | `			zErrUri ? zErrUri : "",VfsStrerror(errno));` |
|     - | 12630 | `	}` |
|   127 | 12631 | `	pSlot = PH7_NativeAttr(pThis,SFO_H);` |
|   127 | 12632 | `	if( pSlot == 0 ){` |
|   ! 0 | 12633 | `		PH7_StreamCloseHandle(pDev->pStream,pDev->pHandle);` |
|   ! 0 | 12634 | `		MarkIOPrivateClosed(pDev);` |
|   ! 0 | 12635 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 12636 | `	}` |
|   127 | 12637 | `	PH7_MemObjRelease(pSlot);` |
|   127 | 12638 | `	pSlot->x.pOther = pDev;` |
|   127 | 12639 | `	MemObjSetType(pSlot,MEMOBJ_RES);` |
|   127 | 12640 | `	PH7_NativeSetAttrStr(pVm,pThis,SFO_M,zMode,nMode);` |
|   127 | 12641 | `	SfiSetName(pVm,pThis,zPath,nPath);` |
|   127 | 12642 | `	return PH7_OK;` |
|    76 | 12643 | `}` |
|     - | 12644 | `/* The class's teardown: php closes the stream with the OBJECT, which is when a` |
|     - | 12645 | ` * file written through one gets its last bytes. */` |
|   144 | 12646 | `static void SfoRelease(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 12647 | `{` |
|   145 | 12648 | `	io_private *pDev = SfoDev(pThis);` |
|    72 | 12649 | `	SXUNUSED(pVm);` |
|   145 | 12650 | `	if( pDev == 0 ){` |
|    19 | 12651 | `		return;` |
|     - | 12652 | `	}` |
|   127 | 12653 | `	PH7_StreamFilterReleaseChains(pDev);` |
|   127 | 12654 | `	PH7_StreamCloseHandle(pDev->pStream,pDev->pHandle);` |
|   127 | 12655 | `	MarkIOPrivateClosed(pDev);` |
|    73 | 12656 | `}` |
|     - | 12657 | `/*` |
|     - | 12658 | ` * SplFileObject::__construct(string $filename, string $mode = 'r',` |
|     - | 12659 | ` *                            bool $useIncludePath = false, $context = null)` |
|     - | 12660 | ` */` |
|   100 | 12661 | `static int vm_builtin_SplFileObject_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12662 | `{` |
|   101 | 12663 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   101 | 12664 | `	const char *zMode = "r";` |
|   101 | 12665 | `	int nMode = 1;` |
|   101 | 12666 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|   ! 0 | 12667 | `		return PH7_OK;` |
|     - | 12668 | `	}` |
|   101 | 12669 | `	if( nArg > 1 ){` |
|    13 | 12670 | `		zMode = ph7_value_to_string(apArg[1],&nMode);` |
|     6 | 12671 | `	}` |
|   153 | 12672 | `	return SfoOpen(pCtx,pThis,apArg[0],zMode,nMode,` |
|    51 | 12673 | `		nArg > 2 ? ph7_value_to_bool(apArg[2]) : FALSE,` |
|    51 | 12674 | `		nArg > 3 && (apArg[3]->iFlags & MEMOBJ_NULL) == 0 ? apArg[3] : 0,4);` |
|    51 | 12675 | `}` |
|     - | 12676 | `/*` |
|     - | 12677 | ` * SplTempFileObject::__construct(int $maxMemory = 2097152)` |
|     - | 12678 | ` *` |
|     - | 12679 | `` * php builds a php:// URI from the argument and opens it `wb`, and the three`` |
|     - | 12680 | ` * arms are visible from outside because getPathname() answers the URI: a` |
|     - | 12681 | `` * NEGATIVE budget is `php://memory` (never spilled to disk), a budget NAMED is`` |
|     - | 12682 | `` * `php://temp/maxmemory:N` -- including 0, which spills immediately -- and no`` |
|     - | 12683 | `` * argument at all is a plain `php://temp` carrying php's own default. The path`` |
|     - | 12684 | `` * is the EMPTY string rather than the `php:/` a URI's last slash would cut.`` |
|     - | 12685 | ` */` |
|    24 | 12686 | `static int vm_builtin_SplTempFileObject_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12687 | `{` |
|    25 | 12688 | `	ph7_vm *pVm = pCtx->pVm;` |
|    25 | 12689 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    25 | 12690 | `	sxi64 iMax = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|     - | 12691 | `	ph7_value sUri;` |
|     - | 12692 | `	sxi32 rc;` |
|    25 | 12693 | `	if( pThis == 0 ){` |
|   ! 0 | 12694 | `		return PH7_OK;` |
|     - | 12695 | `	}` |
|    25 | 12696 | `	PH7_MemObjInitFromString(pVm,&sUri,0);` |
|    25 | 12697 | `	if( iMax < 0 ){` |
|     3 | 12698 | `		PH7_MemObjStringAppend(&sUri,"php://memory",sizeof("php://memory")-1);` |
|    24 | 12699 | `	}else if( nArg > 0 ){` |
|     - | 12700 | `		char zBuf[64];` |
|     5 | 12701 | `		int n = SyBufferFormat(zBuf,sizeof(zBuf),"php://temp/maxmemory:%qd",iMax);` |
|     5 | 12702 | `		PH7_MemObjStringAppend(&sUri,zBuf,(sxu32)n);` |
|     3 | 12703 | `	}else{` |
|    19 | 12704 | `		PH7_MemObjStringAppend(&sUri,"php://temp",sizeof("php://temp")-1);` |
|     - | 12705 | `	}` |
|    25 | 12706 | `	rc = SfoOpen(pCtx,pThis,&sUri,"wb",2,FALSE,0,0);` |
|    25 | 12707 | `	PH7_MemObjRelease(&sUri);` |
|    25 | 12708 | `	if( rc == PH7_OK ){` |
|    23 | 12709 | `		PH7_NativeSetAttrStr(pVm,pThis,SFI_P,"",0);` |
|    11 | 12710 | `	}` |
|    25 | 12711 | `	return rc;` |
|    13 | 12712 | `}` |
|     - | 12713 | `/* The guard every method below opens with: the handle, or the refusal. */` |
|   466 | 12714 | `static io_private * SfoNeed(ph7_context *pCtx,sxi32 *pRc)` |
|     1 | 12715 | `{` |
|   467 | 12716 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   467 | 12717 | `	io_private *pDev = SfoDev(pThis);` |
|   467 | 12718 | `	*pRc = PH7_OK;` |
|   467 | 12719 | `	if( pDev == 0 ){` |
|     5 | 12720 | `		*pRc = PH7_VmThrowException(pCtx,"Error",` |
|     - | 12721 | `			"The parent constructor was not called: the object is in an invalid state");` |
|     2 | 12722 | `	}` |
|   467 | 12723 | `	return pDev;` |
|     1 | 12724 | `}` |
|    26 | 12725 | `static int vm_builtin_SplFileObject_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12726 | `{` |
|     - | 12727 | `	sxi32 rc;` |
|    13 | 12728 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    27 | 12729 | `	if( SfoNeed(pCtx,&rc) == 0 ){` |
|   ! 0 | 12730 | `		return rc;` |
|     - | 12731 | `	}` |
|    27 | 12732 | `	return SfoRewind(pCtx);` |
|    14 | 12733 | `}` |
|    18 | 12734 | `static int vm_builtin_SplFileObject_eof(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12735 | `{` |
|     - | 12736 | `	sxi32 rc;` |
|    19 | 12737 | `	io_private *pDev = SfoNeed(pCtx,&rc);` |
|     9 | 12738 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    19 | 12739 | `	if( pDev == 0 ){` |
|   ! 0 | 12740 | `		return rc;` |
|     - | 12741 | `	}` |
|    19 | 12742 | `	ph7_result_bool(pCtx,PH7_StreamAtEof(pDev) != 0);` |
|    19 | 12743 | `	return PH7_OK;` |
|    10 | 12744 | `}` |
|     - | 12745 | `/* php's valid(): the LINE decides under READ_AHEAD, the stream otherwise. */` |
|   122 | 12746 | `static int vm_builtin_SplFileObject_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12747 | `{` |
|   123 | 12748 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 12749 | `	sxi32 rc;` |
|     - | 12750 | `	io_private *pDev;` |
|    61 | 12751 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   123 | 12752 | `	if( SfoIsChecked(pCtx->pVm,pThis) && SfoDev(pThis) == 0 ){` |
|   ! 0 | 12753 | `		return PH7_VmThrowException(pCtx,"Error",` |
|     - | 12754 | `			"The parent constructor was not called: the object is in an invalid state");` |
|     - | 12755 | `	}` |
|   123 | 12756 | `	if( SfoFlags(pThis) & SFO_READ_AHEAD ){` |
|    25 | 12757 | `		ph7_result_bool(pCtx,PH7_NativeAttrInt(pThis,SFO_LS) != 0);` |
|    25 | 12758 | `		return PH7_OK;` |
|     - | 12759 | `	}` |
|    99 | 12760 | `	pDev = SfoNeed(pCtx,&rc);` |
|    99 | 12761 | `	if( pDev == 0 ){` |
|   ! 0 | 12762 | `		return rc;` |
|     - | 12763 | `	}` |
|    99 | 12764 | `	ph7_result_bool(pCtx,PH7_StreamAtEof(pDev) == 0);` |
|    99 | 12765 | `	return PH7_OK;` |
|    62 | 12766 | `}` |
|     - | 12767 | `/* php's fgets(), which is also this class's getCurrentLine(): a LOUD read that` |
|     - | 12768 | ` * always advances the line number. */` |
|    42 | 12769 | `static int vm_builtin_SplFileObject_fgets(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12770 | `{` |
|    43 | 12771 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 12772 | `	sxi32 rc;` |
|    43 | 12773 | `	int nLine = 0;` |
|     - | 12774 | `	const char *zLine;` |
|    21 | 12775 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    43 | 12776 | `	if( SfoNeed(pCtx,&rc) == 0 ){` |
|     3 | 12777 | `		return rc;` |
|     - | 12778 | `	}` |
|    41 | 12779 | `	if( SfoReadEx(pCtx,FALSE,1,FALSE,&rc) != SFO_READ_OK ){` |
|     3 | 12780 | `		return rc;` |
|     - | 12781 | `	}` |
|    39 | 12782 | `	zLine = SfoLine(pThis,&nLine);` |
|    39 | 12783 | `	ph7_result_string(pCtx,zLine,nLine);` |
|    39 | 12784 | `	return PH7_OK;` |
|    22 | 12785 | `}` |
|     - | 12786 | `/*` |
|     - | 12787 | ` * php's current(): read one line if nothing is held, then pick between the two` |
|     - | 12788 | ` * current values -- the STRING wins unless READ_CSV has an array beside it.` |
|     - | 12789 | ` */` |
|   110 | 12790 | `static int vm_builtin_SplFileObject_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12791 | `{` |
|   111 | 12792 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 12793 | `	sxi32 rc;` |
|     - | 12794 | `	sxi64 iHas;` |
|    55 | 12795 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   111 | 12796 | `	if( SfoNeed(pCtx,&rc) == 0 ){` |
|     3 | 12797 | `		return rc;` |
|     - | 12798 | `	}` |
|   109 | 12799 | `	if( PH7_NativeAttrInt(pThis,SFO_LS) == 0 ){` |
|    87 | 12800 | `		if( SfoReadLine(pCtx,TRUE,&rc) == SFO_READ_THROW ){` |
|   ! 0 | 12801 | `			return rc;` |
|     - | 12802 | `		}` |
|    43 | 12803 | `	}` |
|   109 | 12804 | `	iHas = PH7_NativeAttrInt(pThis,SFO_LS);` |
|   108 | 12805 | `	if( (iHas & SFO_HAS_LINE)` |
|   147 | 12806 | `	 && (!(SfoFlags(pThis) & SFO_READ_CSV) \|\| (iHas & SFO_HAS_ZVAL) == 0) ){` |
|    85 | 12807 | `		int nLine = 0;` |
|    85 | 12808 | `		const char *zLine = SfoLine(pThis,&nLine);` |
|    85 | 12809 | `		ph7_result_string(pCtx,zLine,nLine);` |
|    67 | 12810 | `	}else if( iHas & SFO_HAS_ZVAL ){` |
|    17 | 12811 | `		ph7_value *pZ = PH7_NativeAttr(pThis,SFO_Z);` |
|    17 | 12812 | `		if( pZ ){` |
|    17 | 12813 | `			ph7_result_value(pCtx,pZ);` |
|     9 | 12814 | `		}else{` |
|   ! 0 | 12815 | `			ph7_result_bool(pCtx,0);` |
|     - | 12816 | `		}` |
|     9 | 12817 | `	}else{` |
|     9 | 12818 | `		ph7_result_bool(pCtx,0);` |
|     - | 12819 | `	}` |
|   109 | 12820 | `	return PH7_OK;` |
|    56 | 12821 | `}` |
|     - | 12822 | `/* php's key(): the stored count, deliberately WITHOUT reading ahead -- which is` |
|     - | 12823 | ` * what lets fgetc() count newlines instead of lines. */` |
|   126 | 12824 | `static int vm_builtin_SplFileObject_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12825 | `{` |
|     - | 12826 | `	sxi32 rcChk;` |
|    63 | 12827 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   127 | 12828 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|     3 | 12829 | `		return rcChk;` |
|     - | 12830 | `	}` |
|   125 | 12831 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(PH7_ContextThis(pCtx),SFO_K));` |
|   125 | 12832 | `	return PH7_OK;` |
|    64 | 12833 | `}` |
|   100 | 12834 | `static int vm_builtin_SplFileObject_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12835 | `{` |
|   101 | 12836 | `	ph7_vm *pVm = pCtx->pVm;` |
|   101 | 12837 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 12838 | `	sxi32 rc;` |
|    50 | 12839 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   101 | 12840 | `	if( SfoNeed(pCtx,&rc) == 0 ){` |
|   ! 0 | 12841 | `		return rc;` |
|     - | 12842 | `	}` |
|   101 | 12843 | `	SfoFreeLine(pVm,pThis);` |
|   101 | 12844 | `	if( SfoFlags(pThis) & SFO_READ_AHEAD ){` |
|    21 | 12845 | `		if( SfoReadLine(pCtx,TRUE,&rc) == SFO_READ_THROW ){` |
|   ! 0 | 12846 | `			return rc;` |
|     - | 12847 | `		}` |
|    10 | 12848 | `	}` |
|   101 | 12849 | `	PH7_NativeSetAttrInt(pVm,pThis,SFO_K,PH7_NativeAttrInt(pThis,SFO_K) + 1);` |
|   101 | 12850 | `	return PH7_OK;` |
|    51 | 12851 | `}` |
|    18 | 12852 | `static int vm_builtin_SplFileObject_setFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12853 | `{` |
|     - | 12854 | `	sxi32 rcChk;` |
|    19 | 12855 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 12856 | `		return rcChk;` |
|     - | 12857 | `	}` |
|     - | 12858 | `	/* php stores the WHOLE word and masks only on the way out. */` |
|    37 | 12859 | `	PH7_NativeSetAttrInt(pCtx->pVm,PH7_ContextThis(pCtx),SFO_FL,` |
|    18 | 12860 | `		nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0);` |
|    19 | 12861 | `	return PH7_OK;` |
|    10 | 12862 | `}` |
|     4 | 12863 | `static int vm_builtin_SplFileObject_getFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12864 | `{` |
|     - | 12865 | `	sxi32 rcChk;` |
|     2 | 12866 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 | 12867 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 12868 | `		return rcChk;` |
|     - | 12869 | `	}` |
|     5 | 12870 | `	ph7_result_int64(pCtx,SfoFlags(PH7_ContextThis(pCtx)) & SFO_FLAGS_MASK);` |
|     5 | 12871 | `	return PH7_OK;` |
|     3 | 12872 | `}` |
|     4 | 12873 | `static int vm_builtin_SplFileObject_setMaxLineLen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12874 | `{` |
|     5 | 12875 | `	sxi64 iLen = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|     - | 12876 | `	sxi32 rcChk;` |
|     5 | 12877 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 12878 | `		return rcChk;` |
|     - | 12879 | `	}` |
|     5 | 12880 | `	if( iLen < 0 ){` |
|     4 | 12881 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 12882 | `			"%s(): Argument #1 ($maxLength) must be greater than or equal to 0",` |
|     1 | 12883 | `			ph7_function_name(pCtx));` |
|     - | 12884 | `	}` |
|     3 | 12885 | `	PH7_NativeSetAttrInt(pCtx->pVm,PH7_ContextThis(pCtx),SFO_ML,iLen);` |
|     3 | 12886 | `	return PH7_OK;` |
|     3 | 12887 | `}` |
|     2 | 12888 | `static int vm_builtin_SplFileObject_getMaxLineLen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12889 | `{` |
|     - | 12890 | `	sxi32 rcChk;` |
|     1 | 12891 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     3 | 12892 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 12893 | `		return rcChk;` |
|     - | 12894 | `	}` |
|     3 | 12895 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(PH7_ContextThis(pCtx),SFO_ML));` |
|     3 | 12896 | `	return PH7_OK;` |
|     2 | 12897 | `}` |
|     - | 12898 | `/* php's RecursiveIterator half: a file has no children and says so. */` |
|     4 | 12899 | `static int vm_builtin_SplFileObject_hasChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12900 | `{` |
|     - | 12901 | `	sxi32 rcChk;` |
|     2 | 12902 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 | 12903 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 12904 | `		return rcChk;` |
|     - | 12905 | `	}` |
|     5 | 12906 | `	ph7_result_bool(pCtx,0);` |
|     5 | 12907 | `	return PH7_OK;` |
|     3 | 12908 | `}` |
|     4 | 12909 | `static int vm_builtin_SplFileObject_getChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12910 | `{` |
|     - | 12911 | `	sxi32 rcChk;` |
|     2 | 12912 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 | 12913 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 12914 | `		return rcChk;` |
|     - | 12915 | `	}` |
|     5 | 12916 | `	ph7_result_null(pCtx);` |
|     5 | 12917 | `	return PH7_OK;` |
|     3 | 12918 | `}` |
|     - | 12919 | `/*` |
|     - | 12920 | ` * php's fgetcsv(): the three arguments override this instance's settings for` |
|     - | 12921 | ` * ONE call, and the argument NUMBERS are this spelling's own.` |
|     - | 12922 | ` */` |
|    12 | 12923 | `static int vm_builtin_SplFileObject_fgetcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12924 | `{` |
|    13 | 12925 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 12926 | `	int delim,encl,escape;` |
|     - | 12927 | `	sxi32 rc;` |
|    13 | 12928 | `	if( SfoNeed(pCtx,&rc) == 0 ){` |
|   ! 0 | 12929 | `		return rc;` |
|     - | 12930 | `	}` |
|    13 | 12931 | `	SfoCsvControl(pThis,&delim,&encl,&escape);` |
|    13 | 12932 | `	if( nArg > 0 ){` |
|    13 | 12933 | `		rc = PH7_CsvCharArg(pCtx,apArg[0],1,"separator",0,&delim);` |
|    13 | 12934 | `		if( rc != PH7_OK ){` |
|   ! 0 | 12935 | `			return rc;` |
|     - | 12936 | `		}` |
|     6 | 12937 | `	}` |
|    13 | 12938 | `	if( nArg > 1 ){` |
|    13 | 12939 | `		rc = PH7_CsvCharArg(pCtx,apArg[1],2,"enclosure",0,&encl);` |
|    13 | 12940 | `		if( rc != PH7_OK ){` |
|     3 | 12941 | `			return rc;` |
|     - | 12942 | `		}` |
|     5 | 12943 | `	}` |
|    11 | 12944 | `	if( nArg > 2 ){` |
|    11 | 12945 | `		rc = PH7_CsvCharArg(pCtx,apArg[2],3,"escape",1,&escape);` |
|    11 | 12946 | `		if( rc != PH7_OK ){` |
|   ! 0 | 12947 | `			return rc;` |
|     - | 12948 | `		}` |
|     5 | 12949 | `	}` |
|     - | 12950 | `	{` |
|     - | 12951 | `		ph7_value sOut;` |
|     - | 12952 | `		int r;` |
|    11 | 12953 | `		PH7_MemObjInit(pCtx->pVm,&sOut);` |
|    11 | 12954 | `		r = SfoReadCsv(pCtx,delim,encl,escape,&sOut,TRUE,&rc);` |
|    11 | 12955 | `		if( r == SFO_READ_OK ){` |
|    11 | 12956 | `			ph7_result_value(pCtx,&sOut);` |
|     5 | 12957 | `		}else if( r == SFO_READ_FAIL ){` |
|   ! 0 | 12958 | `			ph7_result_bool(pCtx,0);` |
|   ! 0 | 12959 | `		}` |
|    11 | 12960 | `		PH7_MemObjRelease(&sOut);` |
|    11 | 12961 | `		return r == SFO_READ_THROW ? rc : PH7_OK;` |
|     - | 12962 | `	}` |
|     7 | 12963 | `}` |
|     - | 12964 | `/* php's fputcsv(): the same overrides, at the positions THIS method numbers. */` |
|     4 | 12965 | `static int vm_builtin_SplFileObject_fputcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12966 | `{` |
|     5 | 12967 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 12968 | `	ph7_value *apOut[6];` |
|     - | 12969 | `	ph7_value sDelim,sEncl,sEsc;` |
|     - | 12970 | `	int delim,encl,escape,nOut,r;` |
|     - | 12971 | `	sxi32 rc;` |
|     5 | 12972 | `	if( SfoNeed(pCtx,&rc) == 0 ){` |
|   ! 0 | 12973 | `		return rc;` |
|     - | 12974 | `	}` |
|     5 | 12975 | `	if( nArg < 1 ){` |
|   ! 0 | 12976 | `		return PH7_OK;` |
|     - | 12977 | `	}` |
|     5 | 12978 | `	SfoCsvControl(pThis,&delim,&encl,&escape);` |
|     5 | 12979 | `	if( nArg > 1 ){` |
|     5 | 12980 | `		rc = PH7_CsvCharArg(pCtx,apArg[1],2,"separator",0,&delim);` |
|     5 | 12981 | `		if( rc != PH7_OK ){` |
|   ! 0 | 12982 | `			return rc;` |
|     - | 12983 | `		}` |
|     2 | 12984 | `	}` |
|     5 | 12985 | `	if( nArg > 2 ){` |
|     5 | 12986 | `		rc = PH7_CsvCharArg(pCtx,apArg[2],3,"enclosure",0,&encl);` |
|     5 | 12987 | `		if( rc != PH7_OK ){` |
|     3 | 12988 | `			return rc;` |
|     - | 12989 | `		}` |
|     1 | 12990 | `	}` |
|     3 | 12991 | `	if( nArg > 3 ){` |
|     3 | 12992 | `		rc = PH7_CsvCharArg(pCtx,apArg[3],4,"escape",1,&escape);` |
|     3 | 12993 | `		if( rc != PH7_OK ){` |
|   ! 0 | 12994 | `			return rc;` |
|     - | 12995 | `		}` |
|     1 | 12996 | `	}` |
|     - | 12997 | `	/* The writer lives in vfs_stream.c and takes the three as STRINGS, at` |
|     - | 12998 | `	 * fputcsv()'s own positions; the settings resolved above are handed over in` |
|     - | 12999 | `	 * that shape rather than re-parsed there. */` |
|     3 | 13000 | `	PH7_MemObjInitFromString(pCtx->pVm,&sDelim,0);` |
|     3 | 13001 | `	PH7_MemObjInitFromString(pCtx->pVm,&sEncl,0);` |
|     3 | 13002 | `	PH7_MemObjInitFromString(pCtx->pVm,&sEsc,0);` |
|     - | 13003 | `	{` |
|     3 | 13004 | `		char c = (char)delim;` |
|     3 | 13005 | `		PH7_MemObjStringAppend(&sDelim,&c,sizeof(char));` |
|     3 | 13006 | `		c = (char)encl;` |
|     3 | 13007 | `		PH7_MemObjStringAppend(&sEncl,&c,sizeof(char));` |
|     3 | 13008 | `		if( escape != PH7_CSV_NO_ESCAPE ){` |
|     3 | 13009 | `			c = (char)escape;` |
|     3 | 13010 | `			PH7_MemObjStringAppend(&sEsc,&c,sizeof(char));` |
|     1 | 13011 | `		}` |
|     - | 13012 | `	}` |
|     3 | 13013 | `	apOut[0] = PH7_NativeAttr(pThis,SFO_H);` |
|     3 | 13014 | `	apOut[1] = apArg[0];` |
|     3 | 13015 | `	apOut[2] = &sDelim;` |
|     3 | 13016 | `	apOut[3] = &sEncl;` |
|     3 | 13017 | `	apOut[4] = &sEsc;` |
|     3 | 13018 | `	nOut = 5;` |
|     3 | 13019 | `	if( nArg > 4 ){` |
|     3 | 13020 | `		apOut[5] = apArg[4];` |
|     3 | 13021 | `		nOut = 6;` |
|     1 | 13022 | `	}` |
|     3 | 13023 | `	r = PH7_builtin_fputcsv(pCtx,nOut,apOut);` |
|     3 | 13024 | `	PH7_MemObjRelease(&sDelim);` |
|     3 | 13025 | `	PH7_MemObjRelease(&sEncl);` |
|     3 | 13026 | `	PH7_MemObjRelease(&sEsc);` |
|     3 | 13027 | `	return r;` |
|     3 | 13028 | `}` |
|     4 | 13029 | `static int vm_builtin_SplFileObject_setCsvControl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 13030 | `{` |
|     5 | 13031 | `	ph7_vm *pVm = pCtx->pVm;` |
|     5 | 13032 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     5 | 13033 | `	int delim = ',',encl = '"',escape = '\\';` |
|     - | 13034 | `	sxi32 rc,rcChk;` |
|     5 | 13035 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 13036 | `		return rcChk;` |
|     - | 13037 | `	}` |
|     5 | 13038 | `	if( nArg > 0 ){` |
|     5 | 13039 | `		rc = PH7_CsvCharArg(pCtx,apArg[0],1,"separator",0,&delim);` |
|     5 | 13040 | `		if( rc != PH7_OK ){` |
|     3 | 13041 | `			return rc;` |
|     - | 13042 | `		}` |
|     1 | 13043 | `	}` |
|     3 | 13044 | `	if( nArg > 1 ){` |
|     3 | 13045 | `		rc = PH7_CsvCharArg(pCtx,apArg[1],2,"enclosure",0,&encl);` |
|     3 | 13046 | `		if( rc != PH7_OK ){` |
|   ! 0 | 13047 | `			return rc;` |
|     - | 13048 | `		}` |
|     1 | 13049 | `	}` |
|     3 | 13050 | `	if( nArg > 2 ){` |
|     3 | 13051 | `		rc = PH7_CsvCharArg(pCtx,apArg[2],3,"escape",1,&escape);` |
|     3 | 13052 | `		if( rc != PH7_OK ){` |
|   ! 0 | 13053 | `			return rc;` |
|     - | 13054 | `		}` |
|     - | 13055 | `		/* Naming the escape at all is what stops php's deprecation notice. */` |
|     3 | 13056 | `		PH7_NativeSetAttrInt(pVm,pThis,SFO_ED,0);` |
|     1 | 13057 | `	}` |
|     3 | 13058 | `	PH7_NativeSetAttrInt(pVm,pThis,SFO_D,delim);` |
|     3 | 13059 | `	PH7_NativeSetAttrInt(pVm,pThis,SFO_EN,encl);` |
|     3 | 13060 | `	PH7_NativeSetAttrInt(pVm,pThis,SFO_ES,escape);` |
|     3 | 13061 | `	return PH7_OK;` |
|     3 | 13062 | `}` |
|     4 | 13063 | `static int vm_builtin_SplFileObject_getCsvControl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 13064 | `{` |
|     5 | 13065 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 13066 | `	ph7_value *pArr,*pVal;` |
|     - | 13067 | `	int delim,encl,escape;` |
|     - | 13068 | `	char c;` |
|     - | 13069 | `	sxi32 rcChk;` |
|     2 | 13070 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 | 13071 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 13072 | `		return rcChk;` |
|     - | 13073 | `	}` |
|     5 | 13074 | `	SfoCsvControl(pThis,&delim,&encl,&escape);` |
|     5 | 13075 | `	pArr = ph7_context_new_array(pCtx);` |
|     5 | 13076 | `	pVal = ph7_context_new_scalar(pCtx);` |
|     5 | 13077 | `	if( pArr == 0 \|\| pVal == 0 ){` |
|   ! 0 | 13078 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 13079 | `	}` |
|     5 | 13080 | `	c = (char)delim;` |
|     5 | 13081 | `	ph7_value_string(pVal,&c,1);` |
|     5 | 13082 | `	ph7_array_add_elem(pArr,0,pVal);` |
|     5 | 13083 | `	ph7_value_reset_string_cursor(pVal);` |
|     5 | 13084 | `	c = (char)encl;` |
|     5 | 13085 | `	ph7_value_string(pVal,&c,1);` |
|     5 | 13086 | `	ph7_array_add_elem(pArr,0,pVal);` |
|     5 | 13087 | `	ph7_value_reset_string_cursor(pVal);` |
|     - | 13088 | `	/* A disabled escape is reported as the EMPTY string, not as a byte. */` |
|     5 | 13089 | `	if( escape != PH7_CSV_NO_ESCAPE ){` |
|     3 | 13090 | `		c = (char)escape;` |
|     3 | 13091 | `		ph7_value_string(pVal,&c,1);` |
|     2 | 13092 | `	}else{` |
|     3 | 13093 | `		ph7_value_string(pVal,"",0);` |
|     - | 13094 | `	}` |
|     5 | 13095 | `	ph7_array_add_elem(pArr,0,pVal);` |
|     5 | 13096 | `	ph7_result_value(pCtx,pArr);` |
|     5 | 13097 | `	return PH7_OK;` |
|     3 | 13098 | `}` |
|     - | 13099 | `/*` |
|     - | 13100 | ` * The eight methods that are the corresponding builtin over this object's own` |
|     - | 13101 | ` * handle. Each pre-validates what php validates IN THE METHOD -- the argument` |
|     - | 13102 | ` * numbers are the method's, one lower than the function's -- and then hands the` |
|     - | 13103 | ` * work to the one implementation there is. Every diagnostic the builtin raises` |
|     - | 13104 | ` * itself is worded from ph7_function_name(), which in here is the qualified` |
|     - | 13105 | ` * method name php prints.` |
|     - | 13106 | ` */` |
|    32 | 13107 | `static int SfoDelegate(ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|     - | 13108 | `	int (*xFunc)(ph7_context *,int,ph7_value **))` |
|     1 | 13109 | `{` |
|     - | 13110 | `	ph7_value *apOut[8];` |
|     - | 13111 | `	sxi32 rc;` |
|     - | 13112 | `	int i;` |
|    33 | 13113 | `	if( SfoNeed(pCtx,&rc) == 0 ){` |
|   ! 0 | 13114 | `		return rc;` |
|     - | 13115 | `	}` |
|    33 | 13116 | `	if( nArg > (int)(SX_ARRAYSIZE(apOut) - 1) ){` |
|   ! 0 | 13117 | `		nArg = (int)(SX_ARRAYSIZE(apOut) - 1);` |
|   ! 0 | 13118 | `	}` |
|    33 | 13119 | `	apOut[0] = PH7_NativeAttr(PH7_ContextThis(pCtx),SFO_H);` |
|    61 | 13120 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    29 | 13121 | `		apOut[i+1] = apArg[i];` |
|    15 | 13122 | `	}` |
|    33 | 13123 | `	return xFunc(pCtx,nArg + 1,apOut);` |
|    17 | 13124 | `}` |
|   ! 0 | 13125 | `static int vm_builtin_SplFileObject_fflush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 13126 | `{` |
|   ! 0 | 13127 | `	return SfoDelegate(pCtx,nArg,apArg,PH7_builtin_fflush);` |
|   ! 0 | 13128 | `}` |
|     6 | 13129 | `static int vm_builtin_SplFileObject_ftell(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 13130 | `{` |
|     7 | 13131 | `	return SfoDelegate(pCtx,nArg,apArg,PH7_builtin_ftell);` |
|     1 | 13132 | `}` |
|     2 | 13133 | `static int vm_builtin_SplFileObject_fstat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 13134 | `{` |
|     3 | 13135 | `	return SfoDelegate(pCtx,nArg,apArg,PH7_builtin_fstat);` |
|     1 | 13136 | `}` |
|     2 | 13137 | `static int vm_builtin_SplFileObject_fpassthru(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 13138 | `{` |
|     3 | 13139 | `	return SfoDelegate(pCtx,nArg,apArg,PH7_builtin_fpassthru);` |
|     1 | 13140 | `}` |
|    14 | 13141 | `static int vm_builtin_SplFileObject_fwrite(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 13142 | `{` |
|    15 | 13143 | `	return SfoDelegate(pCtx,nArg,apArg,PH7_builtin_fwrite);` |
|     1 | 13144 | `}` |
|     - | 13145 | `/* php validates the operation in the METHOD, so the refusal names argument #1. */` |
|     6 | 13146 | `static int vm_builtin_SplFileObject_flock(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 13147 | `{` |
|     - | 13148 | `	sxi32 rcChk;` |
|     7 | 13149 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 13150 | `		return rcChk;` |
|     - | 13151 | `	}` |
|     7 | 13152 | `	if( nArg > 0 && (ph7_value_to_int(apArg[0]) & 3) == 0 ){` |
|     4 | 13153 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 13154 | `			"%s(): Argument #1 ($operation) must be one of LOCK_SH, LOCK_EX, or LOCK_UN",` |
|     1 | 13155 | `			ph7_function_name(pCtx));` |
|     - | 13156 | `	}` |
|     5 | 13157 | `	return SfoDelegate(pCtx,nArg,apArg,PH7_builtin_flock);` |
|     4 | 13158 | `}` |
|     2 | 13159 | `static int vm_builtin_SplFileObject_ftruncate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 13160 | `{` |
|     - | 13161 | `	sxi32 rcChk;` |
|     3 | 13162 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 13163 | `		return rcChk;` |
|     - | 13164 | `	}` |
|     3 | 13165 | `	if( nArg > 0 && ph7_value_to_int64(apArg[0]) < 0 ){` |
|     4 | 13166 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 13167 | `			"%s(): Argument #1 ($size) must be greater than or equal to 0",` |
|     1 | 13168 | `			ph7_function_name(pCtx));` |
|     - | 13169 | `	}` |
|   ! 0 | 13170 | `	return SfoDelegate(pCtx,nArg,apArg,PH7_builtin_ftruncate);` |
|     2 | 13171 | `}` |
|     4 | 13172 | `static int vm_builtin_SplFileObject_fread(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 13173 | `{` |
|     - | 13174 | `	sxi32 rcChk;` |
|     5 | 13175 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 13176 | `		return rcChk;` |
|     - | 13177 | `	}` |
|     5 | 13178 | `	if( nArg < 1 \|\| ph7_value_to_int64(apArg[0]) <= 0 ){` |
|     4 | 13179 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 13180 | `			"%s(): Argument #1 ($length) must be greater than 0",` |
|     1 | 13181 | `			ph7_function_name(pCtx));` |
|     - | 13182 | `	}` |
|     3 | 13183 | `	return SfoDelegate(pCtx,nArg,apArg,PH7_builtin_fread);` |
|     3 | 13184 | `}` |
|     - | 13185 | `/* php's fseek() drops the held line first: the position moved, so what was read` |
|     - | 13186 | ` * ahead no longer describes it. */` |
|     2 | 13187 | `static int vm_builtin_SplFileObject_fseek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 13188 | `{` |
|     - | 13189 | `	sxi32 rc;` |
|     3 | 13190 | `	if( SfoNeed(pCtx,&rc) == 0 ){` |
|   ! 0 | 13191 | `		return rc;` |
|     - | 13192 | `	}` |
|     3 | 13193 | `	SfoFreeLine(pCtx->pVm,PH7_ContextThis(pCtx));` |
|     3 | 13194 | `	return SfoDelegate(pCtx,nArg,apArg,PH7_builtin_fseek);` |
|     2 | 13195 | `}` |
|     - | 13196 | `/*` |
|     - | 13197 | ` * php's fgetc(): one byte, the held line dropped, and the line number advanced` |
|     - | 13198 | ` * only when the byte IS a newline.` |
|     - | 13199 | ` */` |
|     8 | 13200 | `static int vm_builtin_SplFileObject_fgetc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 13201 | `{` |
|     9 | 13202 | `	ph7_vm *pVm = pCtx->pVm;` |
|     9 | 13203 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 13204 | `	io_private *pDev;` |
|     - | 13205 | `	char c;` |
|     - | 13206 | `	sxi32 rc;` |
|     4 | 13207 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     9 | 13208 | `	pDev = SfoNeed(pCtx,&rc);` |
|     9 | 13209 | `	if( pDev == 0 ){` |
|   ! 0 | 13210 | `		return rc;` |
|     - | 13211 | `	}` |
|     9 | 13212 | `	SfoFreeLine(pVm,pThis);` |
|     9 | 13213 | `	if( PH7_StreamRead(pDev,&c,sizeof(char)) < 1 ){` |
|   ! 0 | 13214 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 13215 | `		return PH7_OK;` |
|     - | 13216 | `	}` |
|     9 | 13217 | `	if( c == '\n' ){` |
|     3 | 13218 | `		PH7_NativeSetAttrInt(pVm,pThis,SFO_K,PH7_NativeAttrInt(pThis,SFO_K) + 1);` |
|     1 | 13219 | `	}` |
|     9 | 13220 | `	ph7_result_string(pCtx,&c,sizeof(char));` |
|     9 | 13221 | `	return PH7_OK;` |
|     5 | 13222 | `}` |
|     - | 13223 | `/* php's fscanf(): the format is run over the NEXT LINE, read loudly. */` |
|     4 | 13224 | `static int vm_builtin_SplFileObject_fscanf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 13225 | `{` |
|     5 | 13226 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 13227 | `	const char *zFmt,*zLine;` |
|     5 | 13228 | `	int nFmt = 0,nLine = 0;` |
|     - | 13229 | `	SyBlob sLine;` |
|     - | 13230 | `	sxi32 rc;` |
|     5 | 13231 | `	if( SfoNeed(pCtx,&rc) == 0 ){` |
|   ! 0 | 13232 | `		return rc;` |
|     - | 13233 | `	}` |
|     5 | 13234 | `	if( nArg < 1 ){` |
|   ! 0 | 13235 | `		return PH7_OK;` |
|     - | 13236 | `	}` |
|     5 | 13237 | `	if( SfoReadOne(pCtx,FALSE,FALSE,&rc) != SFO_READ_OK ){` |
|   ! 0 | 13238 | `		return rc;` |
|     - | 13239 | `	}` |
|     - | 13240 | `	/* The scan allocates, and allocating moves the slot the line lives in. The` |
|     - | 13241 | `	 * length is read in its own statement: as one argument beside the call that` |
|     - | 13242 | `	 * WRITES it, nothing orders the two. */` |
|     5 | 13243 | `	zLine = SfoLine(pThis,&nLine);` |
|     5 | 13244 | `	SyBlobInit(&sLine,&pCtx->pVm->sAllocator);` |
|     5 | 13245 | `	SyBlobAppend(&sLine,zLine,(sxu32)nLine);` |
|     5 | 13246 | `	zFmt = ph7_value_to_string(apArg[0],&nFmt);` |
|     7 | 13247 | `	rc = PH7_ScanfRun(pCtx,(const char *)SyBlobData(&sLine),(int)SyBlobLength(&sLine),` |
|     2 | 13248 | `		zFmt,nFmt,&apArg[1],nArg - 1);` |
|     5 | 13249 | `	SyBlobRelease(&sLine);` |
|     5 | 13250 | `	return (int)rc;` |
|     3 | 13251 | `}` |
|     - | 13252 | `/*` |
|     - | 13253 | ` * php's seek(): rewind, then walk FORWARD through the object's own read -- so a` |
|     - | 13254 | ` * subclass's getCurrentLine() is obeyed -- and, without READ_AHEAD, land one` |
|     - | 13255 | ` * past with nothing held.` |
|     - | 13256 | ` */` |
|     6 | 13257 | `static int vm_builtin_SplFileObject_seek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 13258 | `{` |
|     7 | 13259 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 | 13260 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 13261 | `	sxi64 iLine,i;` |
|     - | 13262 | `	sxi32 rc;` |
|     7 | 13263 | `	if( SfoNeed(pCtx,&rc) == 0 ){` |
|   ! 0 | 13264 | `		return rc;` |
|     - | 13265 | `	}` |
|     7 | 13266 | `	iLine = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|     7 | 13267 | `	if( iLine < 0 ){` |
|     4 | 13268 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 13269 | `			"%s(): Argument #1 ($line) must be greater than or equal to 0",` |
|     1 | 13270 | `			ph7_function_name(pCtx));` |
|     - | 13271 | `	}` |
|     5 | 13272 | `	rc = SfoRewind(pCtx);` |
|     5 | 13273 | `	if( rc != PH7_OK ){` |
|   ! 0 | 13274 | `		return rc;` |
|     - | 13275 | `	}` |
|    19 | 13276 | `	for( i = 0 ; i < iLine ; ++i ){` |
|    17 | 13277 | `		int r = SfoReadLine(pCtx,TRUE,&rc);` |
|    17 | 13278 | `		if( r == SFO_READ_THROW ){` |
|   ! 0 | 13279 | `			return rc;` |
|     - | 13280 | `		}` |
|    17 | 13281 | `		if( r != SFO_READ_OK ){` |
|     3 | 13282 | `			return PH7_OK;` |
|     - | 13283 | `		}` |
|     8 | 13284 | `	}` |
|     3 | 13285 | `	if( iLine > 0 && (SfoFlags(pThis) & SFO_READ_AHEAD) == 0 ){` |
|     3 | 13286 | `		PH7_NativeSetAttrInt(pVm,pThis,SFO_K,PH7_NativeAttrInt(pThis,SFO_K) + 1);` |
|     3 | 13287 | `		SfoFreeLine(pVm,pThis);` |
|     1 | 13288 | `	}` |
|     3 | 13289 | `	return PH7_OK;` |
|     4 | 13290 | `}` |
|     - | 13291 | `/* php's __toString(): the current line, read LOUDLY when there is none. */` |
|     4 | 13292 | `static int vm_builtin_SplFileObject_toString(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 13293 | `{` |
|     5 | 13294 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     5 | 13295 | `	int nLine = 0;` |
|     - | 13296 | `	const char *zLine;` |
|     - | 13297 | `	sxi32 rc;` |
|     2 | 13298 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 | 13299 | `	if( SfoNeed(pCtx,&rc) == 0 ){` |
|   ! 0 | 13300 | `		return rc;` |
|     - | 13301 | `	}` |
|     5 | 13302 | `	if( (PH7_NativeAttrInt(pThis,SFO_LS) & SFO_HAS_LINE) == 0 ){` |
|     5 | 13303 | `		if( SfoReadLine(pCtx,FALSE,&rc) != SFO_READ_OK ){` |
|   ! 0 | 13304 | `			return rc;` |
|     - | 13305 | `		}` |
|     2 | 13306 | `	}` |
|     5 | 13307 | `	zLine = SfoLine(pThis,&nLine);` |
|     5 | 13308 | `	ph7_result_string(pCtx,zLine,nLine);` |
|     5 | 13309 | `	return PH7_OK;` |
|     3 | 13310 | `}` |
|     - | 13311 | `/*` |
|     - | 13312 | ` * The declaration. Method ORDER, signatures and tentative return types are` |
|     - | 13313 | ` * spl_directory.stub.php's. php gives this class NO clone handler at all, which` |
|     - | 13314 | ` * is its "uncloneable" -- and the same @not-serializable SplFileInfo carries.` |
|     - | 13315 | ` */` |
|  6721 | 13316 | `static sxi32 VmInstallSplFileObject(ph7_vm *pVm)` |
|     5 | 13317 | `{` |
|     - | 13318 | `	static const PH7_NativePropDef aFileProp[] = {` |
|     - | 13319 | `		{ SFO_H,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 13320 | `		{ SFO_M,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "r", 0.0 }, 0 },` |
|     - | 13321 | `		{ SFO_FL, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 13322 | `		{ SFO_ML, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 13323 | `		{ SFO_D,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, ',', 0, 0.0 }, 0 },` |
|     - | 13324 | `		{ SFO_EN, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, '"', 0, 0.0 }, 0 },` |
|     - | 13325 | `		{ SFO_ES, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, '\\', 0, 0.0 }, 0 },` |
|     - | 13326 | `		{ SFO_ED, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 1, 0, 0.0 }, 0 },` |
|     - | 13327 | `		{ SFO_L,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|     - | 13328 | `		{ SFO_Z,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 13329 | `		{ SFO_LS, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 13330 | `		{ SFO_K,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 13331 | `	};` |
|     - | 13332 | `	static const PH7_NativeConstDef aFileConst[] = {` |
|     - | 13333 | `		{ "DROP_NEW_LINE", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SFO_DROP_NEW_LINE, 0, 0.0 },` |
|     - | 13334 | `		{ "READ_AHEAD",    PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SFO_READ_AHEAD, 0, 0.0 },` |
|     - | 13335 | `		{ "SKIP_EMPTY",    PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SFO_SKIP_EMPTY, 0, 0.0 },` |
|     - | 13336 | `		{ "READ_CSV",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SFO_READ_CSV, 0, 0.0 },` |
|     - | 13337 | `	};` |
|     - | 13338 | `	static const PH7_NativeMethodDef aFileMethod[] = {` |
|     - | 13339 | `		{ "__construct",   PH7_MOD_PUBLIC,` |
|     - | 13340 | `		  "string $filename, string $mode = \"r\", bool $useIncludePath = false, "` |
|     - | 13341 | `		  "$context = null", 0, vm_builtin_SplFileObject_construct },` |
|     - | 13342 | `		{ "rewind",        PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplFileObject_rewind },` |
|     - | 13343 | `		{ "eof",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplFileObject_eof },` |
|     - | 13344 | `		{ "valid",         PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplFileObject_valid },` |
|     - | 13345 | `		{ "fgets",         PH7_MOD_PUBLIC, "", "@string", vm_builtin_SplFileObject_fgets },` |
|     - | 13346 | `		{ "fread",         PH7_MOD_PUBLIC, "int $length", "@string\|false",` |
|     - | 13347 | `		  vm_builtin_SplFileObject_fread },` |
|     - | 13348 | `		{ "fgetcsv",       PH7_MOD_PUBLIC,` |
|     - | 13349 | `		  "string $separator = \",\", string $enclosure = \"\\\"\", string $escape = \"\\\\\"",` |
|     - | 13350 | `		  "@array\|false", vm_builtin_SplFileObject_fgetcsv },` |
|     - | 13351 | `		{ "fputcsv",       PH7_MOD_PUBLIC,` |
|     - | 13352 | `		  "array $fields, string $separator = \",\", string $enclosure = \"\\\"\", "` |
|     - | 13353 | `		  "string $escape = \"\\\\\", string $eol = \"\n\"", "@int\|false",` |
|     - | 13354 | `		  vm_builtin_SplFileObject_fputcsv },` |
|     - | 13355 | `		{ "setCsvControl", PH7_MOD_PUBLIC,` |
|     - | 13356 | `		  "string $separator = \",\", string $enclosure = \"\\\"\", string $escape = \"\\\\\"",` |
|     - | 13357 | `		  "@void", vm_builtin_SplFileObject_setCsvControl },` |
|     - | 13358 | `		{ "getCsvControl", PH7_MOD_PUBLIC, "", "@array",` |
|     - | 13359 | `		  vm_builtin_SplFileObject_getCsvControl },` |
|     - | 13360 | `		{ "flock",         PH7_MOD_PUBLIC, "int $operation, &$wouldBlock = null", "@bool",` |
|     - | 13361 | `		  vm_builtin_SplFileObject_flock },` |
|     - | 13362 | `		{ "fflush",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplFileObject_fflush },` |
|     - | 13363 | `		{ "ftell",         PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_SplFileObject_ftell },` |
|     - | 13364 | `		{ "fseek",         PH7_MOD_PUBLIC, "int $offset, int $whence = SEEK_SET", "@int",` |
|     - | 13365 | `		  vm_builtin_SplFileObject_fseek },` |
|     - | 13366 | `		{ "fgetc",         PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_SplFileObject_fgetc },` |
|     - | 13367 | `		{ "fpassthru",     PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplFileObject_fpassthru },` |
|     - | 13368 | `		{ "fscanf",        PH7_MOD_PUBLIC, "string $format, mixed &...$vars", "@array\|int\|null",` |
|     - | 13369 | `		  vm_builtin_SplFileObject_fscanf },` |
|     - | 13370 | `		{ "fwrite",        PH7_MOD_PUBLIC, "string $data, ?int $length = null", "@int\|false",` |
|     - | 13371 | `		  vm_builtin_SplFileObject_fwrite },` |
|     - | 13372 | `		{ "fstat",         PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplFileObject_fstat },` |
|     - | 13373 | `		{ "ftruncate",     PH7_MOD_PUBLIC, "int $size", "@bool",` |
|     - | 13374 | `		  vm_builtin_SplFileObject_ftruncate },` |
|     - | 13375 | `		{ "current",       PH7_MOD_PUBLIC, "", "@array\|string\|false",` |
|     - | 13376 | `		  vm_builtin_SplFileObject_current },` |
|     - | 13377 | `		{ "key",           PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplFileObject_key },` |
|     - | 13378 | `		{ "next",          PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplFileObject_next },` |
|     - | 13379 | `		{ "setFlags",      PH7_MOD_PUBLIC, "int $flags", "@void",` |
|     - | 13380 | `		  vm_builtin_SplFileObject_setFlags },` |
|     - | 13381 | `		{ "getFlags",      PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplFileObject_getFlags },` |
|     - | 13382 | `		{ "setMaxLineLen", PH7_MOD_PUBLIC, "int $maxLength", "@void",` |
|     - | 13383 | `		  vm_builtin_SplFileObject_setMaxLineLen },` |
|     - | 13384 | `		{ "getMaxLineLen", PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplFileObject_getMaxLineLen },` |
|     - | 13385 | `		/* php types the two it answers CONSTANTLY with the constant itself:` |
|     - | 13386 | ``		 * `false` and `null`, not `bool` and `?RecursiveIterator`. A file has no`` |
|     - | 13387 | `		 * children, and both bodies say so on every path. */` |
|     - | 13388 | `		{ "hasChildren",   PH7_MOD_PUBLIC, "", "@false", vm_builtin_SplFileObject_hasChildren },` |
|     - | 13389 | `		{ "getChildren",   PH7_MOD_PUBLIC, "", "@null",` |
|     - | 13390 | `		  vm_builtin_SplFileObject_getChildren },` |
|     - | 13391 | `		{ "seek",          PH7_MOD_PUBLIC, "int $line", "@void", vm_builtin_SplFileObject_seek },` |
|     - | 13392 | `		/* php aliases getCurrentLine() to fgets(); the override branch in` |
|     - | 13393 | `		 * SfoReadLineEx() is what makes REPLACING it mean something. */` |
|     - | 13394 | `		{ "getCurrentLine",PH7_MOD_PUBLIC, "", "@string", vm_builtin_SplFileObject_fgets },` |
|     - | 13395 | `		{ "__toString",    PH7_MOD_PUBLIC, "", "string", vm_builtin_SplFileObject_toString },` |
|     - | 13396 | `	};` |
|     - | 13397 | `	static const PH7_NativeMethodDef aTempMethod[] = {` |
|     - | 13398 | `		{ "__construct", PH7_MOD_PUBLIC, "int $maxMemory = 2 * 1024 * 1024", 0,` |
|     - | 13399 | `		  vm_builtin_SplTempFileObject_construct },` |
|     - | 13400 | `	};` |
|     - | 13401 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - | 13402 | `		{ "SplFileObject", "SplFileInfo", "RecursiveIterator,SeekableIterator",` |
|     - | 13403 | `		  PH7_CLASS_NOSERIALIZE\|PH7_CLASS_NOCLONE,` |
|     - | 13404 | `		  aFileMethod, SX_ARRAYSIZE(aFileMethod), aFileConst, SX_ARRAYSIZE(aFileConst),` |
|     - | 13405 | `		  aFileProp, SX_ARRAYSIZE(aFileProp), SfoRelease, 0, SfiPresent },` |
|     - | 13406 | `		/* php gives SplTempFileObject no handlers of its own, so it INHERITS the` |
|     - | 13407 | `		 * check pair -- uncloneable, and every method refused on an instance` |
|     - | 13408 | `		 * whose parent constructor never ran. A native class here inherits` |
|     - | 13409 | `		 * neither the refusals nor the hooks (rule 29), so both are restated. */` |
|     - | 13410 | `		{ "SplTempFileObject", "SplFileObject", 0,` |
|     - | 13411 | `		  PH7_CLASS_NOSERIALIZE\|PH7_CLASS_NOCLONE,` |
|     - | 13412 | `		  aTempMethod, SX_ARRAYSIZE(aTempMethod), 0, 0,` |
|     - | 13413 | `		  0, 0, SfoRelease, 0, SfiPresent },` |
|     - | 13414 | `	};` |
|  6726 | 13415 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 | 13416 | `}` |
|  6721 | 13417 | `PH7_PRIVATE sxi32 PH7_VmInstallSpl(ph7_vm *pVm)` |
|     5 | 13418 | `{` |
|  6726 | 13419 | `	sxi32 rc = VmInstallWeak(&(*pVm));` |
|  6726 | 13420 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 13421 | `		return rc;` |
|     - | 13422 | `	}` |
|     - | 13423 | `	/* Ordering, now that zSplLib is gone: the remaining PHP in this subsystem is` |
|     - | 13424 | `	 * the tokenizer chunk's, so these only have to satisfy each OTHER. */` |
|  6726 | 13425 | `	rc = VmInstallSplStore(&(*pVm));` |
|  6726 | 13426 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 13427 | `		return rc;` |
|     - | 13428 | `	}` |
|  6726 | 13429 | `	rc = VmInstallSplDualIterators(&(*pVm));` |
|  6726 | 13430 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 13431 | `		return rc;` |
|     - | 13432 | `	}` |
|     - | 13433 | `	/* After the dual iterators: RecursiveIteratorIterator names OuterIterator and` |
|     - | 13434 | `	 * RecursiveIterator, both declared by that table. */` |
|  6726 | 13435 | `	rc = VmInstallSplRecursiveIt(&(*pVm));` |
|  6726 | 13436 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 13437 | `		return rc;` |
|     - | 13438 | `	}` |
|  6726 | 13439 | `	rc = VmInstallSplDllist(&(*pVm));` |
|  6726 | 13440 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 13441 | `		return rc;` |
|     - | 13442 | `	}` |
|  6726 | 13443 | `	rc = VmInstallSplHeap(&(*pVm));` |
|  6726 | 13444 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 13445 | `		return rc;` |
|     - | 13446 | `	}` |
|  6726 | 13447 | `	rc = VmInstallSplFixedArray(&(*pVm));` |
|  6726 | 13448 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 13449 | `		return rc;` |
|     - | 13450 | `	}` |
|  6726 | 13451 | `	rc = VmInstallSplObjectStorage(&(*pVm));` |
|  6726 | 13452 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 13453 | `		return rc;` |
|     - | 13454 | `	}` |
|     - | 13455 | `	/* After SplObjectStorage: MultipleIterator holds the same storage and reaches` |
|     - | 13456 | `	 * its attach/detach/debug routines. */` |
|  6726 | 13457 | `	rc = VmInstallSplMultipleIterator(&(*pVm));` |
|  6726 | 13458 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 13459 | `		return rc;` |
|     - | 13460 | `	}` |
|  6726 | 13461 | `	rc = VmInstallSplFileInfo(&(*pVm));` |
|  6726 | 13462 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 13463 | `		return rc;` |
|     - | 13464 | `	}` |
|     - | 13465 | `	/* After SplFileInfo: DirectoryIterator extends it, and PH7_ClassInherit copies` |
|     - | 13466 | `	 * the base's methods DOWN (rule 14). */` |
|  6726 | 13467 | `	rc = VmInstallSplDirIterators(&(*pVm));` |
|  6726 | 13468 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 13469 | `		return rc;` |
|     - | 13470 | `	}` |
|     - | 13471 | `	/* After SplFileInfo for the same reason; the dir iterators are ahead of it` |
|     - | 13472 | `	 * only because they are declared together. */` |
|  6726 | 13473 | `	return VmInstallSplFileObject(&(*pVm));` |
|  3361 | 13474 | `}` |
|     - | 13475 |  |
|     - | 13476 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     - | 13477 |  |
|     - | 13478 | `#ifdef PH7_DISABLE_BUILTIN_FUNC` |
|     - | 13479 | `/* Tiny build: no SPL (builtin layer disabled) */` |
|     - | 13480 | `PH7_PRIVATE sxi32 PH7_VmInstallSpl(ph7_vm *pVm){ (void)pVm; return SXRET_OK; }` |
|     - | 13481 | `/* No directory iterators either, so shutdown has nothing to close. */` |
|     - | 13482 | `PH7_PRIVATE void PH7_SplDirVmRelease(ph7_vm *pVm){ (void)pVm; }` |
|     - | 13483 | `/* The writable-container fast path is called unconditionally by OP_LOAD_IDX, and its` |
|     - | 13484 | ` * SXU32_HIGH answer already means "no slot available — take the ordinary offsetGet` |
|     - | 13485 | ` * dispatch". With no SPL classes in this build that is the only answer there is, so the` |
|     - | 13486 | ` * stub keeps the tiny target LINKING without a second #ifdef at the call site. */` |
|     - | 13487 | `PH7_PRIVATE sxu32 PH7_SplDimElemSlot(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pKey,int bCreate)` |
|     - | 13488 | `{` |
|     - | 13489 | `	(void)pVm; (void)pThis; (void)pKey; (void)bCreate;` |
|     - | 13490 | `	return SXU32_HIGH;` |
|     - | 13491 | `}` |
|     - | 13492 | `/* Same reasoning for the dual-iterator method forward: OP_MEMBER asks it on every` |
|     - | 13493 | ` * missing method, and with no SPL classes in this build the answer is always "not` |
|     - | 13494 | ` * one of mine". */` |
|     - | 13495 | `PH7_PRIVATE int PH7_SplOuterForward(ph7_vm *pVm,ph7_class_instance *pThis,SyString *pName,` |
|     - | 13496 | `	ph7_class_instance **ppInner,ph7_class_method **ppMeth)` |
|     - | 13497 | `{` |
|     - | 13498 | `	(void)pVm; (void)pThis; (void)pName; (void)ppInner; (void)ppMeth;` |
|     - | 13499 | `	return 0;` |
|     - | 13500 | `}` |
|     - | 13501 | `#endif` |
|     - | 13502 |  |
