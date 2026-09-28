# src/ph7/vm_builtin_spl.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 7328/8184 lines (89.54%)

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
|    58 |    59 | `static void WkCellDrop(ph7_vm *pVm,VmWeakCell *pCell)` |
|     2 |    60 | `{` |
|    60 |    61 | `	if( pCell == 0 \|\| pCell->nRef == 0 ){` |
|   ! 0 |    62 | `		return;` |
|     - |    63 | `	}` |
|    60 |    64 | `	pCell->nRef--;` |
|    60 |    65 | `	if( pCell->nRef == 0 ){` |
|    42 |    66 | `		if( pCell->pObj ){` |
|     - |    67 | `			/* Target still alive: unhook the registry entry before freeing. */` |
|    16 |    68 | `			void *pDummy = 0;` |
|    16 |    69 | `			SyHashDeleteEntry(&pVm->hWeakCell,(const void *)&pCell->pObj,sizeof(void *),&pDummy);` |
|     7 |    70 | `		}` |
|    42 |    71 | `		SyMemBackendFree(&pVm->sAllocator,pCell);` |
|    20 |    72 | `	}` |
|    31 |    73 | `}` |
|     - |    74 | `/* The cell a WeakReference instance holds, or NULL. */` |
|   210 |    75 | `static VmWeakCell * WkCellOf(ph7_class_instance *pRef)` |
|     2 |    76 | `{` |
|   212 |    77 | `	return (VmWeakCell *)(sxuptr)(sxu64)PH7_NativeAttrInt(pRef,"__h");` |
|     2 |    78 | `}` |
|     - |    79 | `/* Hand an instance back without owning a reference of our own (ph7_result_value's` |
|     - |    80 | ` * MemObjStore takes the one the result needs). */` |
|   155 |    81 | `static void SplResultBorrowed(ph7_context *pCtx,ph7_class_instance *pObj)` |
|     2 |    82 | `{` |
|     - |    83 | `	ph7_value sObj;` |
|   157 |    84 | `	PH7_MemObjInit(pCtx->pVm,&sObj);` |
|   157 |    85 | `	sObj.x.pOther = pObj;` |
|   157 |    86 | `	MemObjSetType(&sObj,MEMOBJ_OBJ);` |
|   157 |    87 | `	ph7_result_value(pCtx,&sObj);` |
|   157 |    88 | `}` |
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
|    44 |   208 | `static void WkRefRelease(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     2 |   209 | `{` |
|    46 |   210 | `	VmWeakCell *pCell = WkCellOf(pThis);` |
|    46 |   211 | `	if( pCell == 0 ){` |
|     5 |   212 | `		return;` |
|     - |   213 | `	}` |
|    42 |   214 | `	if( pCell->pRef == pThis ){` |
|    42 |   215 | `		pCell->pRef = 0;   /* stop publishing an object that is going away */` |
|    20 |   216 | `	}` |
|    42 |   217 | `	PH7_NativeSetAttrInt(pVm,pThis,"__h",0);` |
|    42 |   218 | `	WkCellDrop(pVm,pCell);` |
|    24 |   219 | `}` |
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
|  5740 |   504 | `static sxi32 VmInstallWeak(ph7_vm *pVm)` |
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
|  5745 |   547 | `	sxi32 rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|  5745 |   548 | `	if( rc != SXRET_OK ){` |
|   ! 0 |   549 | `		return rc;` |
|     - |   550 | `	}` |
|  5745 |   551 | `	pMap = PH7_VmExtractClass(&(*pVm),"WeakMap",sizeof("WeakMap")-1,FALSE,0);` |
|  5745 |   552 | `	if( pMap == 0 ){` |
|   ! 0 |   553 | `		return SXERR_NOTFOUND;` |
|     - |   554 | `	}` |
| 22965 |   555 | `	for( n = 0 ; n < SX_ARRAYSIZE(azMapIface) ; n++ ){` |
| 25835 |   556 | `		ph7_class *pIface = PH7_VmExtractClass(&(*pVm),azMapIface[n],` |
| 17220 |   557 | `			(sxu32)SyStrlen(azMapIface[n]),FALSE,0);` |
| 17225 |   558 | `		if( pIface == 0 ){` |
|   ! 0 |   559 | `			return SXERR_NOTFOUND;` |
|     - |   560 | `		}` |
| 17225 |   561 | `		rc = PH7_ClassImplement(pMap,pIface);` |
| 17225 |   562 | `		if( rc != SXRET_OK ){` |
|   ! 0 |   563 | `			return rc;` |
|     - |   564 | `		}` |
|  8615 |   565 | `	}` |
|  5745 |   566 | `	return SXRET_OK;` |
|  2875 |   567 | `}` |
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
|   104 |   588 | `static void SplAddMembers(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)` |
|     1 |   589 | `{` |
|     - |   590 | `	SyHashEntry *pEntry;` |
|   105 |   591 | `	if( pThis == 0 ){` |
|   ! 0 |   592 | `		return;` |
|     - |   593 | `	}` |
|   105 |   594 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|   405 |   595 | `	while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|   301 |   596 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|   301 |   597 | `		SyString *pName = &pVmAttr->pAttr->sName;` |
|     - |   598 | `		ph7_value *pVal;` |
|     - |   599 | `		ph7_value sKey;` |
|   300 |   600 | `		if( PH7_ATTR_UNPRESENTED(pVmAttr)` |
|    29 |   601 | `		 \|\| (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL) ){` |
|   273 |   602 | `			continue;` |
|     - |   603 | `		}` |
|    29 |   604 | `		pVal = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|    29 |   605 | `		if( pVal == 0 ){` |
|   ! 0 |   606 | `			continue;` |
|     - |   607 | `		}` |
|    29 |   608 | `		PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|    29 |   609 | `		PH7_MemObjStringAppend(&sKey,pName->zString,pName->nByte);` |
|    29 |   610 | `		ph7_array_add_elem(pOut,&sKey,pVal);` |
|    29 |   611 | `		PH7_MemObjRelease(&sKey);` |
|     1 |   612 | `	}` |
|    53 |   613 | `}` |
|     - |   614 | `/* The same walk as a standalone array, which is the shape most payloads want. */` |
|    74 |   615 | `static sxi32 SplMembersOf(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)` |
|     1 |   616 | `{` |
|    75 |   617 | `	PH7_MemObjInit(&(*pVm),pOut);` |
|    75 |   618 | `	if( PH7_MemObjToHashmap(pOut) != SXRET_OK ){` |
|   ! 0 |   619 | `		return SXERR_MEM;` |
|     - |   620 | `	}` |
|    75 |   621 | `	SplAddMembers(&(*pVm),pThis,pOut);` |
|    75 |   622 | `	return SXRET_OK;` |
|    38 |   623 | `}` |
|    10 |   624 | `static int SplMembersWalk(ph7_value *pKey,ph7_value *pVal,void *pUserData)` |
|     1 |   625 | `{` |
|    11 |   626 | `	ph7_class_instance *pThis = (ph7_class_instance *)pUserData;` |
|     - |   627 | `	const char *zKey;` |
|     - |   628 | `	int nKey;` |
|    11 |   629 | `	if( !ph7_value_is_string(pKey) ){` |
|   ! 0 |   630 | `		return PH7_OK;` |
|     - |   631 | `	}` |
|    11 |   632 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|    11 |   633 | `	PH7_NativeSetProp(pThis->pVm,pThis,zKey,(sxu32)nKey,pVal);` |
|    11 |   634 | `	return PH7_OK;` |
|     6 |   635 | `}` |
|    38 |   636 | `static void SplMembersLoad(ph7_class_instance *pThis,ph7_value *pMembers)` |
|     1 |   637 | `{` |
|    39 |   638 | `	if( pThis == 0 \|\| pMembers == 0 \|\| (pMembers->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 |   639 | `		return;` |
|     - |   640 | `	}` |
|    39 |   641 | `	ph7_array_walk(pMembers,SplMembersWalk,pThis);` |
|    20 |   642 | `}` |
|     - |   643 |  |
|     - |   644 | `/*` |
|     - |   645 | ` * ArrayIterator / ArrayObject — the array STORE, in C.` |
|     - |   646 | ` *` |
|     - |   647 | `` * These two shared one implementation through `trait __SplStoreT`, the last PHL-only`` |
|     - |   648 | ` * TRAIT and the last name in the §4 ledger that was not a function. php shares nothing` |
|     - |   649 | ` * between them at the TYPE level: both have no parent and no common interface beyond` |
|     - |   650 | `` * ArrayAccess/Countable, and the storage lives in ext/spl's own `spl_array_object` struct`` |
|     - |   651 | ` * behind handlers. The recorded decision follows php: no shared type at all —` |
|     - |   652 | ` * one set of C bodies, named by BOTH spec rows. The builder installs a method table per` |
|     - |   653 | ` * class anyway, so "replaying the method table" is a second row and nothing else, and the` |
|     - |   654 | ` * php-visible shape stays exact (a native abstract BASE would have given both classes a` |
|     - |   655 | ` * parent php does not have).` |
|     - |   656 | ` *` |
|     - |   657 | ` * The store itself stays a plain PHP array in a declared private slot, exactly as the trait` |
|     - |   658 | `` * had it: nothing here is a C handle, so `clone` and `serialize()` keep working as php's do`` |
|     - |   659 | ` * and neither class wants the NOCLONE/NOSERIALIZE flags an engine-state class needs. The` |
|     - |   660 | ` * bodies delegate to the engine's OWN array builtins (asort, ksort, uasort, reset, current,` |
|     - |   661 | ` * next, key), which is what the PHP did — one layer down, with no dispatcher round trip.` |
|     - |   662 | ` */` |
|     - |   663 | `#define SPL_D  "__d"  /* the stored array */` |
|     - |   664 | `#define SPL_F  "__f"  /* the flags word */` |
|     - |   665 | `#define SPL_IT "__it" /* ArrayObject's iterator class name */` |
|     - |   666 | `/*` |
|     - |   667 | `` * php's `~SPL_ARRAY_INT_MASK`: the flags word keeps only its low 16 bits, so`` |
|     - |   668 | `` * `setFlags(-1)` then `getFlags()` answers 65535 rather than -1. php masks on the`` |
|     - |   669 | ` * WRITE, which is why every reader — getFlags(), __serialize(), the ARRAY_AS_PROPS` |
|     - |   670 | ` * test — sees the same masked value without asking.` |
|     - |   671 | ` */` |
|     - |   672 | `#define SPL_FLAG_MASK 0xFFFF` |
|     - |   673 | `/* php's SPL_ARRAY_STD_PROP_LIST: the non-debug presentation surfaces answer the` |
|     - |   674 | ` * ordinary property table instead of the storage. */` |
|     - |   675 | `#define SPL_STD_PROP_LIST 0x0001` |
|     - |   676 | `/*` |
|     - |   677 | ` * The instance's storage slot, separated for writing (every caller may mutate it). Answers` |
|     - |   678 | ` * the SLOT rather than the hashmap because that is what the array builtins below take.` |
|     - |   679 | ` */` |
|  8596 |   680 | `static ph7_value * SplStoreSlot(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     5 |   681 | `{` |
|  8601 |   682 | `	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,SPL_D) : 0;` |
|  8601 |   683 | `	if( pSlot == 0 ){` |
|   ! 0 |   684 | `		return 0;` |
|     - |   685 | `	}` |
|  8601 |   686 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 |   687 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|   ! 0 |   688 | `			return 0;` |
|     - |   689 | `		}` |
|   ! 0 |   690 | `	}` |
|  8601 |   691 | `	if( PH7_HashmapCowSeparate(pVm,pSlot) == 0 ){` |
|   ! 0 |   692 | `		return 0;` |
|     - |   693 | `	}` |
|  8601 |   694 | `	return pSlot;` |
|  4303 |   695 | `}` |
|  5026 |   696 | `static ph7_hashmap * SplStore(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     4 |   697 | `{` |
|  5030 |   698 | `	ph7_value *pSlot = SplStoreSlot(pVm,pThis);` |
|  5030 |   699 | `	return pSlot ? (ph7_hashmap *)pSlot->x.pOther : 0;` |
|     4 |   700 | `}` |
|     - |   701 | `/*` |
|     - |   702 | `` * php's `spl_array_read_dimension` / `zend_weakmap_read_dimension` FAST PATH: a fetch on`` |
|     - |   703 | ` * one of these containers answers with the store's OWN element, not with a copy of it.` |
|     - |   704 | ` * That is the whole difference between a class that supports indirect modification and` |
|     - |   705 | `` * one that does not (PH7_VmDimFetchWritable, oo.c) — `$ao['a']` reached through`` |
|     - |   706 | `` * `offsetGet` is a VALUE however native the method is, so `$r = &$ao['a']`,`` |
|     - |   707 | ``  * `sort($ao['a'])`, `unset($ao['a'][0])`, `foreach ($ao['a'] as &$v)` and `$ao['n']++` `` |
|     - |   708 | ` * all wrote into a temporary and left the store as it was, in silence.` |
|     - |   709 | ` *` |
|     - |   710 | ` * Answers the element's aMemObj index, which the subscript op hands on as the result's` |
|     - |   711 | `` * slot exactly as an array element's `nValIdx` is handed on. SXU32_HIGH means "not`` |
|     - |   712 | `` * available" and the caller falls back to the ordinary `offsetGet` dispatch, which is`` |
|     - |   713 | ` * what keeps every diagnostic (WeakMap's not-contained Error, the store's own` |
|     - |   714 | `` * `Undefined array key`) in the one place that already words it.`` |
|     - |   715 | ` *` |
|     - |   716 | ` * bCreate is php's write-context vivification: a missing ArrayObject/ArrayIterator key` |
|     - |   717 | `` * IS created by a W fetch (`$ao['new']['k'] = 1` works there), while a WeakMap never`` |
|     - |   718 | ` * creates one — its missing key is an Error, raised by the accessor below.` |
|     - |   719 | ` */` |
|   134 |   720 | `PH7_PRIVATE sxu32 PH7_SplDimElemSlot(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pKey,int bCreate)` |
|     2 |   721 | `{` |
|   136 |   722 | `	ph7_hashmap_node *pNode = 0;` |
|   136 |   723 | `	if( pThis == 0 \|\| pKey == 0 ){` |
|   ! 0 |   724 | `		return SXU32_HIGH;` |
|     - |   725 | `	}` |
|   136 |   726 | `	if( pKey->iFlags & MEMOBJ_NULL ){` |
|     - |   727 | `		/* PH7_HashmapLookup folds a NULL key to "" IN PLACE, and a declined fast path` |
|     - |   728 | `		 * has to hand the accessor the key it was written with (php deprecates the null` |
|     - |   729 | `		 * offset there). Cheaper to stand down than to probe on a copy. */` |
|   ! 0 |   730 | `		return SXU32_HIGH;` |
|     - |   731 | `	}` |
|   136 |   732 | `	if( PH7_NativeAttr(pThis,SPL_D) != 0 ){` |
|     - |   733 | `		ph7_value *pSlot;` |
|   107 |   734 | `		if( pKey->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP) ){` |
|     - |   735 | `			/* Neither shape HAS an array key here: the lookup would fold both to the` |
|     - |   736 | `			 * words "Object"/"Array" and hand back a slot nobody can name again. php` |
|     - |   737 | `			 * refuses them, so stand down and let the accessor -- which words that` |
|     - |   738 | `			 * refusal -- see the key as it was written. (A WeakMap's key IS an object` |
|     - |   739 | `			 * and takes the branch below.) */` |
|     3 |   740 | `			return SXU32_HIGH;` |
|     - |   741 | `		}` |
|   105 |   742 | `		pSlot = SplStoreSlot(pVm,pThis);` |
|   105 |   743 | `		ph7_hashmap *pMap = pSlot ? (ph7_hashmap *)pSlot->x.pOther : 0;` |
|   105 |   744 | `		if( pMap == 0 ){` |
|   ! 0 |   745 | `			return SXU32_HIGH;` |
|     - |   746 | `		}` |
|   105 |   747 | `		if( PH7_HashmapLookup(pMap,pKey,&pNode) != SXRET_OK ){` |
|    11 |   748 | `			if( !bCreate \|\| PH7_HashmapInsert(pMap,pKey,0) != SXRET_OK ){` |
|     3 |   749 | `				return SXU32_HIGH;` |
|     - |   750 | `			}` |
|     9 |   751 | `			pNode = pMap->pLast;` |
|     4 |   752 | `		}` |
|   103 |   753 | `		return pNode ? pNode->nValIdx : SXU32_HIGH;` |
|     - |   754 | `	}` |
|    30 |   755 | `	if( PH7_NativeAttr(pThis,WM_VALS) != 0 && (pKey->iFlags & MEMOBJ_OBJ) ){` |
|    28 |   756 | `		ph7_class_instance *pObj = (ph7_class_instance *)pKey->x.pOther;` |
|    28 |   757 | `		sxi64 iId = (sxi64)pObj->nObjId;` |
|    28 |   758 | `		if( WmNodeTarget(WmFind(pVm,WmStore(pVm,pThis,WM_REFS),iId)) != pObj ){` |
|     5 |   759 | `			return SXU32_HIGH; /* not contained: offsetGet raises php's Error */` |
|     - |   760 | `		}` |
|    24 |   761 | `		pNode = WmFind(pVm,WmStore(pVm,pThis,WM_VALS),iId);` |
|    24 |   762 | `		return pNode ? pNode->nValIdx : SXU32_HIGH;` |
|     - |   763 | `	}` |
|     3 |   764 | `	return SXU32_HIGH;` |
|    69 |   765 | `}` |
|     - |   766 | `/*` |
|     - |   767 | `` * `$this->__d = $array` for the constructor and exchangeArray(), with php's refusal.`` |
|     - |   768 | ` *` |
|     - |   769 | `` * php DECLARES `object\|array $array` — which is what Reflection prints — and then words the`` |
|     - |   770 | `` * refusal as `must be of type array`, so the shared ZPP screen cannot say both (rule 41's`` |
|     - |   771 | ` * shape) and the check is written here. An OBJECT contributes its properties, as the PHP` |
|     - |   772 | ` * did through get_object_vars().` |
|     - |   773 | ` */` |
|  1222 |   774 | `static sxi32 SplInitStore(ph7_context *pCtx,ph7_class_instance *pThis,` |
|     - |   775 | `	ph7_value *pArray,const char *zOwner)` |
|     5 |   776 | `{` |
|  1227 |   777 | `	ph7_vm *pVm = pCtx->pVm;` |
|  1227 |   778 | `	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,SPL_D) : 0;` |
|  1227 |   779 | `	if( pSlot == 0 ){` |
|   ! 0 |   780 | `		return PH7_OK;` |
|     - |   781 | `	}` |
|  1227 |   782 | `	if( pArray == 0 ){` |
|     - |   783 | ``		/* No argument at all: php's `$array = []` default. A native method has no compiled`` |
|     - |   784 | `		 * parameter records for the defaults to live in (rule 33's neighbour), so the body` |
|     - |   785 | `		 * applies it — and an EXPLICIT null still has to reach the refusal below, which is` |
|     - |   786 | `		 * why the two cases are distinguished here rather than by a NULL check. */` |
|    97 |   787 | `		ph7_hashmap *pEmpty = PH7_NewHashmap(pVm,0,0);` |
|    97 |   788 | `		if( pEmpty == 0 ){` |
|   ! 0 |   789 | `			return PH7_ContextMemoryError(pCtx);` |
|     - |   790 | `		}` |
|    97 |   791 | `		PH7_MemObjRelease(pSlot);` |
|    97 |   792 | `		pSlot->x.pOther = pEmpty;` |
|    97 |   793 | `		MemObjSetType(pSlot,MEMOBJ_HASHMAP);` |
|    97 |   794 | `		return PH7_OK;` |
|     - |   795 | `	}` |
|  1131 |   796 | `	if( pArray->iFlags & MEMOBJ_HASHMAP ){` |
|  1115 |   797 | `		PH7_MemObjRelease(pSlot);` |
|  1115 |   798 | `		PH7_MemObjStore(pArray,pSlot); /* a copy: the store is the object's own */` |
|  1115 |   799 | `		return PH7_OK;` |
|     - |   800 | `	}` |
|    17 |   801 | `	if( pArray->iFlags & MEMOBJ_OBJ ){` |
|     - |   802 | `		/* The PHP read get_object_vars($array): the properties this scope can see, by` |
|     - |   803 | `		 * their plain names. php itself keeps the OBJECT and reads its property table` |
|     - |   804 | `		 * live (so getArrayCopy() answers the mangled private names and count() answers` |
|     - |   805 | `		 * the visible ones) — a divergence this conversion carries over unchanged rather` |
|     - |   806 | `		 * than widening, recorded in §7.4. */` |
|     5 |   807 | `		ph7_class_instance *pObj = (ph7_class_instance *)pArray->x.pOther;` |
|     - |   808 | `		ph7_hashmap *pMap;` |
|     - |   809 | `		SyHashEntry *pEntry;` |
|     5 |   810 | `		PH7_MemObjRelease(pSlot);` |
|     5 |   811 | `		pMap = PH7_NewHashmap(pVm,0,0);` |
|     5 |   812 | `		if( pMap == 0 ){` |
|   ! 0 |   813 | `			return PH7_ContextMemoryError(pCtx);` |
|     - |   814 | `		}` |
|     5 |   815 | `		SyHashResetLoopCursor(&pObj->hAttr);` |
|    13 |   816 | `		while( pObj && (pEntry = SyHashGetNextEntry(&pObj->hAttr)) != 0 ){` |
|     9 |   817 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     - |   818 | `			ph7_value sKey;` |
|     - |   819 | `			ph7_value *pVal;` |
|     9 |   820 | `			if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|   ! 0 |   821 | `				continue;` |
|     - |   822 | `			}` |
|     9 |   823 | `			if( pVmAttr->pAttr->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|   ! 0 |   824 | `				continue;` |
|     - |   825 | `			}` |
|     9 |   826 | `			pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pVmAttr->nIdx);` |
|     9 |   827 | `			if( pVal == 0 ){` |
|   ! 0 |   828 | `				continue;` |
|     - |   829 | `			}` |
|     9 |   830 | `			PH7_MemObjInitFromString(pVm,&sKey,&pVmAttr->pAttr->sName);` |
|     9 |   831 | `			PH7_HashmapInsert(pMap,&sKey,pVal);` |
|     9 |   832 | `			PH7_MemObjRelease(&sKey);` |
|     1 |   833 | `		}` |
|     5 |   834 | `		pSlot->x.pOther = pMap;` |
|     5 |   835 | `		MemObjSetType(pSlot,MEMOBJ_HASHMAP);` |
|     5 |   836 | `		return PH7_OK;` |
|     - |   837 | `	}` |
|    19 |   838 | `	return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |   839 | `		"%s(): Argument #1 ($array) must be of type array, %s given",` |
|     6 |   840 | `		zOwner,ph7_type_name(pArray));` |
|   616 |   841 | `}` |
|     - |   842 | `/* Hand one of the engine's own array builtins the instance's storage slot. */` |
|  3324 |   843 | `static int SplArrayCall(ph7_context *pCtx,ProchHostFunction xFunc,ph7_value *pExtra)` |
|     5 |   844 | `{` |
|  3329 |   845 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |   846 | `	ph7_value *apCall[2];` |
|  3329 |   847 | `	ph7_value *pSlot = SplStoreSlot(pCtx->pVm,pThis);` |
|  3329 |   848 | `	if( pSlot == 0 ){` |
|   ! 0 |   849 | `		return PH7_OK;` |
|     - |   850 | `	}` |
|  3329 |   851 | `	apCall[0] = pSlot;` |
|  3329 |   852 | `	apCall[1] = pExtra;` |
|  3329 |   853 | `	return xFunc(pCtx,pExtra ? 2 : 1,apCall);` |
|  1667 |   854 | `}` |
|     - |   855 | `/*` |
|     - |   856 | `` * php's `Cannot access offset of type X on <class>` for the store's four`` |
|     - |   857 | ` * offsets. An array offset here goes through the ordinary array-key rules, and` |
|     - |   858 | ` * php refuses the two shapes that have no key at all — an OBJECT (named by its` |
|     - |   859 | ` * CLASS, as get_debug_type() names it) and an ARRAY — rather than folding them:` |
|     - |   860 | `` * PHL used to fold both to the string "Object"/"Array", so `$ao[$obj] = 1` wrote`` |
|     - |   861 | `` * under a key no reader could ever ask for and `$ao[$obj]` warned about a key the`` |
|     - |   862 | ` * caller never wrote.` |
|     - |   863 | ` *` |
|     - |   864 | ` * The wording is the ENGINE's own three-way split (vm_ops_load.c): a read or a` |
|     - |   865 | ` * write names the receiver's class, isset/empty names none, and unset says` |
|     - |   866 | ` * "Cannot unset". offsetExists() reached by hand is php's isset arm too.` |
|     - |   867 | ` */` |
|     - |   868 | `#define SPL_OFF_ACCESS 0` |
|     - |   869 | `#define SPL_OFF_ISSET  1` |
|     - |   870 | `#define SPL_OFF_UNSET  2` |
|   180 |   871 | `static int SplOffsetKeyRefused(ph7_context *pCtx,ph7_value *pKey,int iKind,int *pRc)` |
|     2 |   872 | `{` |
|   182 |   873 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   182 |   874 | `	SyString *pOwner = pThis ? &pThis->pClass->sName : 0;` |
|   182 |   875 | `	SyString *pClass = 0;` |
|   182 |   876 | `	const char *zType = "array";` |
|   182 |   877 | `	*pRc = PH7_OK;` |
|   182 |   878 | `	if( pKey->iFlags & MEMOBJ_OBJ ){` |
|    45 |   879 | `		ph7_class_instance *pInst = (ph7_class_instance *)pKey->x.pOther;` |
|    45 |   880 | `		if( pInst && pInst->pClass ){` |
|    45 |   881 | `			pClass = &pInst->pClass->sName;` |
|    22 |   882 | `		}` |
|    45 |   883 | `		zType = "object";` |
|   160 |   884 | `	}else if( (pKey->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    96 |   885 | `		return 0;` |
|     - |   886 | `	}` |
|    87 |   887 | `	if( iKind == SPL_OFF_ISSET ){` |
|    37 |   888 | `		*pRc = pClass` |
|    27 |   889 | `			? PH7_VmThrowException(pCtx,"TypeError",` |
|     9 |   890 | `				"Cannot access offset of type %z in isset or empty",pClass)` |
|    36 |   891 | `			: PH7_VmThrowException(pCtx,"TypeError",` |
|     9 |   892 | `				"Cannot access offset of type %s in isset or empty",zType);` |
|    19 |   893 | `	}else{` |
|    51 |   894 | `		const char *zVerb = iKind == SPL_OFF_UNSET ? "Cannot unset" : "Cannot access";` |
|    51 |   895 | `		*pRc = pClass` |
|    39 |   896 | `			? PH7_VmThrowException(pCtx,"TypeError",` |
|    13 |   897 | `				"%s offset of type %z on %z",zVerb,pClass,pOwner)` |
|    49 |   898 | `			: PH7_VmThrowException(pCtx,"TypeError",` |
|    12 |   899 | `				"%s offset of type %s on %z",zVerb,zType,pOwner);` |
|     - |   900 | `	}` |
|    87 |   901 | `	return 1;` |
|    92 |   902 | `}` |
|    52 |   903 | `static int vm_builtin_SplStore_offsetExists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |   904 | `{` |
|    54 |   905 | `	ph7_hashmap *pMap = SplStore(pCtx->pVm,PH7_ContextThis(pCtx));` |
|    54 |   906 | `	ph7_hashmap_node *pNode = 0;` |
|    54 |   907 | `	int bFound = 0, rc;` |
|    54 |   908 | `	if( nArg > 0 && SplOffsetKeyRefused(pCtx,apArg[0],SPL_OFF_ISSET,&rc) ){` |
|    37 |   909 | `		return rc;` |
|     - |   910 | `	}` |
|    18 |   911 | `	if( pMap && nArg > 0 ){` |
|     - |   912 | `		/* array_key_exists(), not isset(): php's offsetExists() answers true for a key` |
|     - |   913 | `		 * holding NULL (the PHP said array_key_exists too). */` |
|    18 |   914 | `		bFound = PH7_HashmapLookup(pMap,apArg[0],&pNode) == SXRET_OK;` |
|     8 |   915 | `	}` |
|    18 |   916 | `	ph7_result_bool(pCtx,bFound);` |
|    18 |   917 | `	return PH7_OK;` |
|    28 |   918 | `}` |
|    56 |   919 | `static int vm_builtin_SplStore_offsetGet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |   920 | `{` |
|    58 |   921 | `	ph7_vm *pVm = pCtx->pVm;` |
|    58 |   922 | `	ph7_hashmap *pMap = SplStore(pVm,PH7_ContextThis(pCtx));` |
|    58 |   923 | `	ph7_hashmap_node *pNode = 0;` |
|     - |   924 | `	int rcKey;` |
|    58 |   925 | `	if( nArg > 0 && SplOffsetKeyRefused(pCtx,apArg[0],SPL_OFF_ACCESS,&rcKey) ){` |
|    27 |   926 | `		return rcKey;` |
|     - |   927 | `	}` |
|    32 |   928 | `	if( pMap == 0 \|\| nArg < 1 \|\| PH7_HashmapLookup(pMap,apArg[0],&pNode) != SXRET_OK ){` |
|     - |   929 | `		/* php warns "Undefined array key" for a missing offset, with the key rendered` |
|     - |   930 | `		 * the way the LOOKUP folded it (an integer bare, a string quoted) — the same` |
|     - |   931 | `		 * pair OP_LOAD_IDX prints. This one now reports the CALLER's line, where the` |
|     - |   932 | `		 * PHP reported the chunk's. */` |
|     3 |   933 | `		if( nArg > 0 ){` |
|     - |   934 | `			SyBlob sMsg;` |
|     3 |   935 | `			SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     3 |   936 | `			if( PH7_HashmapKeyIsInt(apArg[0]) ){` |
|   ! 0 |   937 | `				if( (apArg[0]->iFlags & MEMOBJ_INT) == 0 ){` |
|   ! 0 |   938 | `					PH7_MemObjToInteger(apArg[0]);` |
|   ! 0 |   939 | `				}` |
|   ! 0 |   940 | `				SyBlobFormat(&sMsg,"Undefined array key %qd",apArg[0]->x.iVal);` |
|   ! 0 |   941 | `			}else{` |
|     - |   942 | `				SyString sKey;` |
|     3 |   943 | `				SyStringInitFromBuf(&sKey,SyBlobData(&apArg[0]->sBlob),` |
|     - |   944 | `					SyBlobLength(&apArg[0]->sBlob));` |
|     3 |   945 | `				SyBlobFormat(&sMsg,"Undefined array key \"%z\"",&sKey);` |
|     - |   946 | `			}` |
|     3 |   947 | `			SyBlobNullAppend(&sMsg);` |
|     3 |   948 | `			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));` |
|     3 |   949 | `			SyBlobRelease(&sMsg);` |
|     1 |   950 | `		}` |
|     3 |   951 | `		ph7_result_null(pCtx);` |
|     3 |   952 | `		return PH7_OK;` |
|     - |   953 | `	}` |
|    30 |   954 | `	ph7_result_value(pCtx,(ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx));` |
|    30 |   955 | `	return PH7_OK;` |
|    30 |   956 | `}` |
|     - |   957 | `/*` |
|     - |   958 | ` * Insert into the store, keeping php's cursor rule.` |
|     - |   959 | ` *` |
|     - |   960 | ` * php's ArrayIterator position is an INTEGER index into the bucket array, so a` |
|     - |   961 | ` * cursor that ran off the end sits AT the element count: inserting a new key there` |
|     - |   962 | ` * makes it valid again and the iterator RESUMES on the element just added. PHL` |
|     - |   963 | ` * carries a node POINTER, which is null past the end and loses that. Re-point it` |
|     - |   964 | ` * here -- the only place the difference shows, since overwriting an EXISTING key` |
|     - |   965 | ` * inserts no node and php's dead cursor stays dead. AppendIterator depends on this:` |
|     - |   966 | ` * php's append() after exhaustion is what makes the walk continue.` |
|     - |   967 | ` */` |
|   294 |   968 | `static void SplStoreInsert(ph7_hashmap *pMap,ph7_value *pKey,ph7_value *pVal)` |
|     1 |   969 | `{` |
|   295 |   970 | `	sxu32 nBefore = pMap->nEntry;` |
|   295 |   971 | `	int bPastEnd = pMap->pCur == 0;` |
|   295 |   972 | `	PH7_HashmapInsert(pMap,pKey,pVal);` |
|   295 |   973 | `	if( bPastEnd && pMap->nEntry > nBefore ){` |
|   159 |   974 | `		pMap->pCur = pMap->pLast;` |
|    79 |   975 | `	}` |
|   295 |   976 | `}` |
|    54 |   977 | `static int vm_builtin_SplStore_offsetSet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |   978 | `{` |
|    55 |   979 | `	ph7_hashmap *pMap = SplStore(pCtx->pVm,PH7_ContextThis(pCtx));` |
|     - |   980 | `	int rcKey;` |
|    55 |   981 | `	if( nArg > 0 && SplOffsetKeyRefused(pCtx,apArg[0],SPL_OFF_ACCESS,&rcKey) ){` |
|    13 |   982 | `		return rcKey;` |
|     - |   983 | `	}` |
|    43 |   984 | `	if( pMap && nArg > 1 ){` |
|     - |   985 | ``		/* A NULL key is `$o[] = $v` — the append form, which is how php's offsetSet()`` |
|     - |   986 | `		 * receives it. */` |
|    43 |   987 | `		SplStoreInsert(pMap,(apArg[0]->iFlags & MEMOBJ_NULL) ? 0 : apArg[0],apArg[1]);` |
|    21 |   988 | `	}` |
|    43 |   989 | `	return PH7_OK;` |
|    28 |   990 | `}` |
|    18 |   991 | `static int vm_builtin_SplStore_offsetUnset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |   992 | `{` |
|    19 |   993 | `	ph7_hashmap *pMap = SplStore(pCtx->pVm,PH7_ContextThis(pCtx));` |
|    19 |   994 | `	ph7_hashmap_node *pNode = 0;` |
|     - |   995 | `	int rcKey;` |
|    19 |   996 | `	if( nArg > 0 && SplOffsetKeyRefused(pCtx,apArg[0],SPL_OFF_UNSET,&rcKey) ){` |
|    13 |   997 | `		return rcKey;` |
|     - |   998 | `	}` |
|     7 |   999 | `	if( pMap && nArg > 0 && PH7_HashmapLookup(pMap,apArg[0],&pNode) == SXRET_OK ){` |
|     7 |  1000 | `		PH7_HashmapUnlinkNode(pNode,TRUE);` |
|     3 |  1001 | `	}` |
|     7 |  1002 | `	return PH7_OK;` |
|    10 |  1003 | `}` |
|     8 |  1004 | `static int vm_builtin_SplStore_append(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1005 | `{` |
|     9 |  1006 | `	ph7_hashmap *pMap = SplStore(pCtx->pVm,PH7_ContextThis(pCtx));` |
|     9 |  1007 | `	if( pMap && nArg > 0 ){` |
|     9 |  1008 | `		SplStoreInsert(pMap,0,apArg[0]);` |
|     4 |  1009 | `	}` |
|     9 |  1010 | `	return PH7_OK;` |
|     1 |  1011 | `}` |
|    74 |  1012 | `static int vm_builtin_SplStore_getArrayCopy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  1013 | `{` |
|    77 |  1014 | `	ph7_value *pSlot = SplStoreSlot(pCtx->pVm,PH7_ContextThis(pCtx));` |
|    37 |  1015 | `	SXUNUSED(nArg);` |
|    37 |  1016 | `	SXUNUSED(apArg);` |
|    77 |  1017 | `	if( pSlot ){` |
|    77 |  1018 | `		ph7_result_value(pCtx,pSlot); /* a COPY: the caller must not alias the store */` |
|    37 |  1019 | `	}` |
|    77 |  1020 | `	return PH7_OK;` |
|     3 |  1021 | `}` |
|    26 |  1022 | `static int vm_builtin_SplStore_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1023 | `{` |
|    27 |  1024 | `	ph7_hashmap *pMap = SplStore(pCtx->pVm,PH7_ContextThis(pCtx));` |
|    13 |  1025 | `	SXUNUSED(nArg);` |
|    13 |  1026 | `	SXUNUSED(apArg);` |
|    27 |  1027 | `	ph7_result_int64(pCtx,pMap ? (ph7_int64)pMap->nEntry : 0);` |
|    27 |  1028 | `	return PH7_OK;` |
|     1 |  1029 | `}` |
|    14 |  1030 | `static int vm_builtin_SplStore_getFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1031 | `{` |
|    15 |  1032 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     7 |  1033 | `	SXUNUSED(nArg);` |
|     7 |  1034 | `	SXUNUSED(apArg);` |
|    15 |  1035 | `	ph7_result_int64(pCtx,pThis ? PH7_NativeAttrInt(pThis,SPL_F) : 0);` |
|    15 |  1036 | `	return PH7_OK;` |
|     1 |  1037 | `}` |
|     2 |  1038 | `static int vm_builtin_SplStore_setFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1039 | `{` |
|     3 |  1040 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     3 |  1041 | `	if( pThis && nArg > 0 ){` |
|     4 |  1042 | `		PH7_NativeSetAttrInt(pCtx->pVm,pThis,SPL_F,` |
|     2 |  1043 | `			ph7_value_to_int64(apArg[0]) & SPL_FLAG_MASK);` |
|     1 |  1044 | `	}` |
|     3 |  1045 | `	return PH7_OK;` |
|     1 |  1046 | `}` |
|     - |  1047 | `/*` |
|     - |  1048 | ` * The six sorts. Each is the engine's own builtin over the stored array — including the` |
|     - |  1049 | `` * `$flags` the PHP DROPPED on the floor (`asort($this->__d)` ignored its own parameter, so`` |
|     - |  1050 | `` * `$it->asort(SORT_STRING)` sorted numerically). natsort/natcasesort go through asort with`` |
|     - |  1051 | ` * php's own flag pair rather than by name: the shared body reads ph7_function_name() to tell` |
|     - |  1052 | `` * the two apart, and a native method's name is `ArrayIterator::natcasesort`.`` |
|     - |  1053 | ` */` |
|     6 |  1054 | `static int vm_builtin_SplStore_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1055 | `{` |
|     7 |  1056 | `	return SplArrayCall(pCtx,ph7_hashmap_asort,nArg > 0 ? apArg[0] : 0);` |
|     1 |  1057 | `}` |
|     6 |  1058 | `static int vm_builtin_SplStore_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1059 | `{` |
|     7 |  1060 | `	return SplArrayCall(pCtx,ph7_hashmap_ksort,nArg > 0 ? apArg[0] : 0);` |
|     1 |  1061 | `}` |
|     4 |  1062 | `static int vm_builtin_SplStore_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1063 | `{` |
|     5 |  1064 | `	return SplArrayCall(pCtx,ph7_hashmap_uasort,nArg > 0 ? apArg[0] : 0);` |
|     1 |  1065 | `}` |
|     2 |  1066 | `static int vm_builtin_SplStore_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1067 | `{` |
|     3 |  1068 | `	return SplArrayCall(pCtx,ph7_hashmap_uksort,nArg > 0 ? apArg[0] : 0);` |
|     1 |  1069 | `}` |
|    12 |  1070 | `static int SplNatSort(ph7_context *pCtx,int bFold)` |
|     3 |  1071 | `{` |
|     - |  1072 | `	ph7_value sFlags;` |
|     - |  1073 | `	int rc;` |
|     - |  1074 | `	/* SORT_NATURAL (6), plus SORT_FLAG_CASE (8) for the folding twin — the same pair` |
|     - |  1075 | `	 * ph7_hashmap_natsort forwards to asort(). */` |
|    15 |  1076 | `	PH7_MemObjInitFromInt(pCtx->pVm,&sFlags,bFold ? (6\|8) : 6);` |
|    15 |  1077 | `	rc = SplArrayCall(pCtx,ph7_hashmap_asort,&sFlags);` |
|    15 |  1078 | `	PH7_MemObjRelease(&sFlags);` |
|    15 |  1079 | `	return rc;` |
|     3 |  1080 | `}` |
|     8 |  1081 | `static int vm_builtin_SplStore_natsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  1082 | `{` |
|     4 |  1083 | `	SXUNUSED(nArg);` |
|     4 |  1084 | `	SXUNUSED(apArg);` |
|    11 |  1085 | `	return SplNatSort(pCtx,0);` |
|     3 |  1086 | `}` |
|     4 |  1087 | `static int vm_builtin_SplStore_natcasesort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  1088 | `{` |
|     2 |  1089 | `	SXUNUSED(nArg);` |
|     2 |  1090 | `	SXUNUSED(apArg);` |
|     6 |  1091 | `	return SplNatSort(pCtx,1);` |
|     2 |  1092 | `}` |
|     - |  1093 | `/* ArrayIterator's cursor: the stored array's own internal pointer, as the PHP had it. */` |
|   960 |  1094 | `static int vm_builtin_ArrayIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 |  1095 | `{` |
|   964 |  1096 | `	ph7_hashmap *pMap = SplStore(pCtx->pVm,PH7_ContextThis(pCtx));` |
|   480 |  1097 | `	SXUNUSED(nArg);` |
|   480 |  1098 | `	SXUNUSED(apArg);` |
|   964 |  1099 | `	if( pMap == 0 \|\| pMap->pCur == 0 ){` |
|     - |  1100 | `		/* Past the end php answers NULL, where current() the FUNCTION answers false. */` |
|     7 |  1101 | `		ph7_result_null(pCtx);` |
|     7 |  1102 | `		return PH7_OK;` |
|     - |  1103 | `	}` |
|   958 |  1104 | `	return SplArrayCall(pCtx,ph7_hashmap_current,0);` |
|   484 |  1105 | `}` |
|   914 |  1106 | `static int vm_builtin_ArrayIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  1107 | `{` |
|   457 |  1108 | `	SXUNUSED(nArg);` |
|   457 |  1109 | `	SXUNUSED(apArg);` |
|   917 |  1110 | `	return SplArrayCall(pCtx,ph7_hashmap_simple_key,0);` |
|     3 |  1111 | `}` |
|   864 |  1112 | `static int vm_builtin_ArrayIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  1113 | `{` |
|   432 |  1114 | `	SXUNUSED(nArg);` |
|   432 |  1115 | `	SXUNUSED(apArg);` |
|   866 |  1116 | `	SplArrayCall(pCtx,ph7_hashmap_next,0);` |
|   866 |  1117 | `	ph7_result_null(pCtx); /* next() the METHOD returns void */` |
|   866 |  1118 | `	return PH7_OK;` |
|     2 |  1119 | `}` |
|   562 |  1120 | `static int vm_builtin_ArrayIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 |  1121 | `{` |
|   281 |  1122 | `	SXUNUSED(nArg);` |
|   281 |  1123 | `	SXUNUSED(apArg);` |
|   566 |  1124 | `	SplArrayCall(pCtx,ph7_hashmap_reset,0);` |
|   566 |  1125 | `	ph7_result_null(pCtx);` |
|   566 |  1126 | `	return PH7_OK;` |
|     4 |  1127 | `}` |
|  1896 |  1128 | `static int vm_builtin_ArrayIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 |  1129 | `{` |
|  1900 |  1130 | `	ph7_hashmap *pMap = SplStore(pCtx->pVm,PH7_ContextThis(pCtx));` |
|   948 |  1131 | `	SXUNUSED(nArg);` |
|   948 |  1132 | `	SXUNUSED(apArg);` |
|  1900 |  1133 | `	ph7_result_bool(pCtx,pMap && pMap->pCur ? 1 : 0);` |
|  1900 |  1134 | `	return PH7_OK;` |
|     4 |  1135 | `}` |
|    34 |  1136 | `static int vm_builtin_ArrayIterator_seek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1137 | `{` |
|    35 |  1138 | `	ph7_hashmap *pMap = SplStore(pCtx->pVm,PH7_ContextThis(pCtx));` |
|    35 |  1139 | `	ph7_int64 iOffset = nArg > 0 ? ph7_value_to_int(apArg[0]) : 0;` |
|     - |  1140 | `	ph7_int64 i;` |
|    35 |  1141 | `	if( pMap == 0 ){` |
|   ! 0 |  1142 | `		return PH7_OK;` |
|     - |  1143 | `	}` |
|    35 |  1144 | `	if( iOffset < 0 \|\| iOffset >= (ph7_int64)pMap->nEntry ){` |
|    10 |  1145 | `		return PH7_VmThrowException(pCtx,"OutOfBoundsException",` |
|     3 |  1146 | `			"Seek position %qd is out of range",iOffset);` |
|     - |  1147 | `	}` |
|    29 |  1148 | `	pMap->pCur = pMap->pFirst;` |
|    65 |  1149 | `	for( i = 0 ; i < iOffset && pMap->pCur ; ++i ){` |
|    37 |  1150 | `		pMap->pCur = pMap->pCur->pPrev; /* insertion order: pFirst, then the pPrev chain */` |
|    19 |  1151 | `	}` |
|    29 |  1152 | `	return PH7_OK;` |
|    18 |  1153 | `}` |
|   962 |  1154 | `static int vm_builtin_ArrayIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 |  1155 | `{` |
|   966 |  1156 | `	ph7_vm *pVm = pCtx->pVm;` |
|   966 |  1157 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  1158 | `	ph7_hashmap *pMap;` |
|   966 |  1159 | `	sxi32 rc = SplInitStore(pCtx,pThis,nArg > 0 ? apArg[0] : 0,` |
|     - |  1160 | `		"ArrayIterator::__construct");` |
|   966 |  1161 | `	if( rc != PH7_OK ){` |
|    11 |  1162 | `		return rc;` |
|     - |  1163 | `	}` |
|   956 |  1164 | `	if( pThis && nArg > 1 ){` |
|   165 |  1165 | `		PH7_NativeSetAttrInt(pVm,pThis,SPL_F,ph7_value_to_int64(apArg[1]) & SPL_FLAG_MASK);` |
|    82 |  1166 | `	}` |
|   956 |  1167 | `	pMap = SplStore(pVm,pThis);` |
|   956 |  1168 | `	if( pMap ){` |
|   956 |  1169 | `		pMap->pCur = pMap->pFirst; /* reset($this->__d) */` |
|   476 |  1170 | `	}` |
|   956 |  1171 | `	return PH7_OK;` |
|   485 |  1172 | `}` |
|     - |  1173 | `/* ArrayObject */` |
|    16 |  1174 | `static int vm_builtin_ArrayObject_setIteratorClass(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1175 | `{` |
|    17 |  1176 | `	ph7_vm *pVm = pCtx->pVm;` |
|    17 |  1177 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  1178 | `	const char *zName;` |
|     - |  1179 | `	int nName;` |
|    17 |  1180 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|   ! 0 |  1181 | `		return PH7_OK;` |
|     - |  1182 | `	}` |
|    17 |  1183 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|    16 |  1184 | `	if( nName != (int)sizeof("ArrayIterator")-1` |
|     9 |  1185 | `	 \|\| SyMemcmp(zName,"ArrayIterator",sizeof("ArrayIterator")-1) != 0 ){` |
|    17 |  1186 | `		ph7_class *pClass = PH7_VmExtractClass(pVm,zName,(sxu32)nName,FALSE,0);` |
|    17 |  1187 | `		ph7_class *pBase = PH7_VmExtractClass(pVm,"ArrayIterator",` |
|     - |  1188 | `			sizeof("ArrayIterator")-1,FALSE,0);` |
|    16 |  1189 | `		if( pClass == 0 \|\| pBase == 0 \|\| pClass == pBase` |
|    15 |  1190 | `		 \|\| !PH7_VmInstanceOf(pClass,pBase) ){` |
|    13 |  1191 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  1192 | `				"ArrayObject::setIteratorClass(): Argument #1 ($iteratorClass) must be "` |
|     4 |  1193 | `				"a class name derived from ArrayIterator, %.*s given",nName,zName);` |
|     - |  1194 | `		}` |
|     4 |  1195 | `	}` |
|     9 |  1196 | `	PH7_NativeSetAttrStr(pVm,pThis,SPL_IT,zName,nName);` |
|     9 |  1197 | `	return PH7_OK;` |
|     9 |  1198 | `}` |
|   236 |  1199 | `static int vm_builtin_ArrayObject_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |  1200 | `{` |
|   241 |  1201 | `	ph7_vm *pVm = pCtx->pVm;` |
|   241 |  1202 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   241 |  1203 | `	sxi32 rc = SplInitStore(pCtx,pThis,nArg > 0 ? apArg[0] : 0,` |
|     - |  1204 | `		"ArrayObject::__construct");` |
|   241 |  1205 | `	if( rc != PH7_OK ){` |
|     3 |  1206 | `		return rc;` |
|     - |  1207 | `	}` |
|   239 |  1208 | `	if( pThis && nArg > 1 ){` |
|    19 |  1209 | `		PH7_NativeSetAttrInt(pVm,pThis,SPL_F,ph7_value_to_int64(apArg[1]) & SPL_FLAG_MASK);` |
|     9 |  1210 | `	}` |
|   239 |  1211 | `	if( nArg > 2 ){` |
|     7 |  1212 | `		return vm_builtin_ArrayObject_setIteratorClass(pCtx,1,&apArg[2]);` |
|     - |  1213 | `	}` |
|   233 |  1214 | `	return PH7_OK;` |
|   123 |  1215 | `}` |
|     4 |  1216 | `static int vm_builtin_ArrayObject_exchangeArray(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1217 | `{` |
|     5 |  1218 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     5 |  1219 | `	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,SPL_D) : 0;` |
|     - |  1220 | `	sxi32 rc;` |
|     5 |  1221 | `	if( pSlot ){` |
|     5 |  1222 | `		ph7_result_value(pCtx,pSlot); /* the OLD store is the return value */` |
|     2 |  1223 | `	}` |
|     5 |  1224 | `	rc = SplInitStore(pCtx,pThis,nArg > 0 ? apArg[0] : 0,"ArrayObject::exchangeArray");` |
|     5 |  1225 | `	return rc;` |
|     1 |  1226 | `}` |
|    10 |  1227 | `static int vm_builtin_ArrayObject_getIteratorClass(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1228 | `{` |
|    11 |  1229 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    11 |  1230 | `	const char *zName = 0;` |
|    11 |  1231 | `	int nName = 0;` |
|     5 |  1232 | `	SXUNUSED(nArg);` |
|     5 |  1233 | `	SXUNUSED(apArg);` |
|    11 |  1234 | `	if( pThis ){` |
|    11 |  1235 | `		PH7_NativeAttrStr(pThis,SPL_IT,&zName,&nName);` |
|     5 |  1236 | `	}` |
|    11 |  1237 | `	ph7_result_string(pCtx,nName > 0 ? zName : "ArrayIterator",nName > 0 ? nName : -1);` |
|    11 |  1238 | `	return PH7_OK;` |
|     1 |  1239 | `}` |
|     - |  1240 | `/*` |
|     - |  1241 | ` * ---------------------------------------------------------------------------` |
|     - |  1242 | ` * php's __serialize()/__unserialize() for the array store.` |
|     - |  1243 | ` *` |
|     - |  1244 | ` * php's payload is a four-element LIST -- [flags, storage, members, iterator class]` |
|     - |  1245 | ` * -- and nothing else can express it: the state lives in ext/spl's own struct, so` |
|     - |  1246 | ` * there are no properties to walk. PHL walked its HIDDEN slots instead and wrote` |
|     - |  1247 | `` * `O:11:"ArrayObject":3:{s:16:"\0ArrayObject\0__d";…}`, which round-tripped inside`` |
|     - |  1248 | ` * PHL and could not read a byte string php produced (nor be read by php).` |
|     - |  1249 | ` *` |
|     - |  1250 | ` * Two details worth keeping. The last element is NULL when the class is the default` |
|     - |  1251 | ` * ArrayIterator, and it is always NULL for an ArrayIterator payload -- php shares one` |
|     - |  1252 | ` * C body between both classes, so ArrayIterator carries the slot it has no use for.` |
|     - |  1253 | ` * And the iterator-class check here is LOOSER than setIteratorClass()'s: restoring` |
|     - |  1254 | `` * accepts any `Iterator`, while the setter and the constructor demand a class derived`` |
|     - |  1255 | `` * from ArrayIterator. php words the refusal with `ArrayObject` either way, even when`` |
|     - |  1256 | ` * ArrayIterator is the receiver.` |
|     - |  1257 | ` * ---------------------------------------------------------------------------` |
|     - |  1258 | ` */` |
|     8 |  1259 | `static sxi32 SplStoreIllTyped(ph7_context *pCtx)` |
|     1 |  1260 | `{` |
|     9 |  1261 | `	return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - |  1262 | `		"Incomplete or ill-typed serialization data");` |
|     1 |  1263 | `}` |
|    26 |  1264 | `static int vm_builtin_SplStore_serializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1265 | `{` |
|    27 |  1266 | `	ph7_vm *pVm = pCtx->pVm;` |
|    27 |  1267 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  1268 | `	ph7_value sOut,sVal,*pStore;` |
|    27 |  1269 | `	const char *zIt = 0;` |
|    27 |  1270 | `	int nIt = 0;` |
|    13 |  1271 | `	SXUNUSED(nArg);` |
|    13 |  1272 | `	SXUNUSED(apArg);` |
|    27 |  1273 | `	PH7_MemObjInit(pVm,&sOut);` |
|    27 |  1274 | `	if( PH7_MemObjToHashmap(&sOut) != SXRET_OK ){` |
|   ! 0 |  1275 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 |  1276 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  1277 | `	}` |
|     - |  1278 | `	/* [0] the flags word */` |
|    27 |  1279 | `	PH7_MemObjInitFromInt(pVm,&sVal,pThis ? PH7_NativeAttrInt(pThis,SPL_F) : 0);` |
|    27 |  1280 | `	ph7_array_add_elem(&sOut,0,&sVal);` |
|    27 |  1281 | `	PH7_MemObjRelease(&sVal);` |
|     - |  1282 | `	/* [1] the storage */` |
|    27 |  1283 | `	pStore = SplStoreSlot(pVm,pThis);` |
|    27 |  1284 | `	if( pStore ){` |
|    27 |  1285 | `		ph7_array_add_elem(&sOut,0,pStore);` |
|    13 |  1286 | `	}` |
|     - |  1287 | `	/* [2] the instance's own properties */` |
|    27 |  1288 | `	if( SplMembersOf(pVm,pThis,&sVal) != SXRET_OK ){` |
|   ! 0 |  1289 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 |  1290 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  1291 | `	}` |
|    27 |  1292 | `	ph7_array_add_elem(&sOut,0,&sVal);` |
|    27 |  1293 | `	PH7_MemObjRelease(&sVal);` |
|     - |  1294 | `	/* [3] the iterator class, NULL for the default one and for ArrayIterator */` |
|    27 |  1295 | `	if( pThis ){` |
|    27 |  1296 | `		PH7_NativeAttrStr(pThis,SPL_IT,&zIt,&nIt);` |
|    13 |  1297 | `	}` |
|    27 |  1298 | `	PH7_MemObjInit(pVm,&sVal);` |
|    27 |  1299 | `	if( nIt > 0 && (nIt != (int)sizeof("ArrayIterator")-1` |
|    14 |  1300 | `	 \|\| SyMemcmp(zIt,"ArrayIterator",sizeof("ArrayIterator")-1) != 0) ){` |
|     5 |  1301 | `		PH7_MemObjStringAppend(&sVal,zIt,(sxu32)nIt);` |
|     2 |  1302 | `	}` |
|    27 |  1303 | `	ph7_array_add_elem(&sOut,0,&sVal);` |
|    27 |  1304 | `	PH7_MemObjRelease(&sVal);` |
|    27 |  1305 | `	ph7_result_value(pCtx,&sOut);` |
|    27 |  1306 | `	PH7_MemObjRelease(&sOut);` |
|    27 |  1307 | `	return PH7_OK;` |
|    14 |  1308 | `}` |
|    36 |  1309 | `static int vm_builtin_SplStore_unserializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1310 | `{` |
|    37 |  1311 | `	ph7_vm *pVm = pCtx->pVm;` |
|    37 |  1312 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  1313 | `	ph7_hashmap *pData;` |
|    37 |  1314 | `	ph7_hashmap_node *pNode = 0;` |
|    37 |  1315 | `	ph7_value *pFlags,*pStorage,*pMembers,*pIt = 0;` |
|     - |  1316 | `	sxi32 rc;` |
|    37 |  1317 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     - |  1318 | `		char zBuf[64];` |
|   ! 0 |  1319 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  1320 | `			"%s(): Argument #1 ($data) must be of type array, %s given",` |
|   ! 0 |  1321 | `			ph7_function_name(pCtx),` |
|   ! 0 |  1322 | `			nArg > 0 ? VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)) : "none");` |
|     - |  1323 | `	}` |
|    37 |  1324 | `	if( pThis == 0 ){` |
|   ! 0 |  1325 | `		return PH7_OK;` |
|     - |  1326 | `	}` |
|    37 |  1327 | `	pData = (ph7_hashmap *)apArg[0]->x.pOther;` |
|    37 |  1328 | `	if( HashmapLookupIntKey(pData,0,&pNode) != SXRET_OK ){` |
|   ! 0 |  1329 | `		return SplStoreIllTyped(pCtx);` |
|     - |  1330 | `	}` |
|    37 |  1331 | `	pFlags = HashmapExtractNodeValue(pNode);` |
|    37 |  1332 | `	pNode = 0;` |
|    37 |  1333 | `	if( HashmapLookupIntKey(pData,1,&pNode) != SXRET_OK ){` |
|   ! 0 |  1334 | `		return SplStoreIllTyped(pCtx);` |
|     - |  1335 | `	}` |
|    37 |  1336 | `	pStorage = HashmapExtractNodeValue(pNode);` |
|    37 |  1337 | `	pNode = 0;` |
|    37 |  1338 | `	if( HashmapLookupIntKey(pData,2,&pNode) != SXRET_OK ){` |
|     3 |  1339 | `		return SplStoreIllTyped(pCtx);` |
|     - |  1340 | `	}` |
|    35 |  1341 | `	pMembers = HashmapExtractNodeValue(pNode);` |
|    35 |  1342 | `	pNode = 0;` |
|    35 |  1343 | `	if( HashmapLookupIntKey(pData,3,&pNode) == SXRET_OK ){` |
|    27 |  1344 | `		pIt = HashmapExtractNodeValue(pNode);` |
|    13 |  1345 | `	}` |
|    34 |  1346 | `	if( pFlags == 0 \|\| (pFlags->iFlags & MEMOBJ_INT) == 0` |
|    33 |  1347 | `	 \|\| pMembers == 0 \|\| (pMembers->iFlags & MEMOBJ_HASHMAP) == 0` |
|    32 |  1348 | `	 \|\| (pIt != 0 && (pIt->iFlags & (MEMOBJ_NULL\|MEMOBJ_STRING)) == 0) ){` |
|     7 |  1349 | `		return SplStoreIllTyped(pCtx);` |
|     - |  1350 | `	}` |
|     - |  1351 | `	/* php's own wording, and its own exception CLASS, for the storage slot. */` |
|    29 |  1352 | `	if( pStorage == 0 \|\| (pStorage->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 ){` |
|     3 |  1353 | `		return PH7_VmThrowException(pCtx,"InvalidArgumentException",` |
|     - |  1354 | `			"Passed variable is not an array or object");` |
|     - |  1355 | `	}` |
|    27 |  1356 | `	if( pIt != 0 && (pIt->iFlags & MEMOBJ_STRING) != 0 && SyBlobLength(&pIt->sBlob) > 0 ){` |
|    11 |  1357 | `		const char *zIt = (const char *)SyBlobData(&pIt->sBlob);` |
|    11 |  1358 | `		int nIt = (int)SyBlobLength(&pIt->sBlob);` |
|    11 |  1359 | `		ph7_class *pClass = PH7_VmExtractClass(pVm,zIt,(sxu32)nIt,FALSE,0);` |
|    11 |  1360 | `		ph7_class *pIface = PH7_VmExtractClass(pVm,"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|    11 |  1361 | `		if( pClass == 0 ){` |
|     4 |  1362 | `			return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - |  1363 | `				"Cannot deserialize ArrayObject with iterator class '%.*s'; "` |
|     1 |  1364 | `				"no such class exists",nIt,zIt);` |
|     - |  1365 | `		}` |
|     9 |  1366 | `		if( pIface == 0 \|\| !PH7_VmInstanceOf(pClass,pIface) ){` |
|     7 |  1367 | `			return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - |  1368 | `				"Cannot deserialize ArrayObject with iterator class '%.*s'; "` |
|     2 |  1369 | `				"this class does not implement the Iterator interface",nIt,zIt);` |
|     - |  1370 | `		}` |
|     5 |  1371 | `		if( PH7_NativeAttr(pThis,SPL_IT) ){` |
|     5 |  1372 | `			PH7_NativeSetAttrStr(pVm,pThis,SPL_IT,zIt,nIt);` |
|     2 |  1373 | `		}` |
|     2 |  1374 | `	}` |
|    21 |  1375 | `	PH7_NativeSetAttrInt(pVm,pThis,SPL_F,ph7_value_to_int64(pFlags) & SPL_FLAG_MASK);` |
|    21 |  1376 | `	rc = SplInitStore(pCtx,pThis,pStorage,"ArrayObject::__unserialize");` |
|    21 |  1377 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  1378 | `		return rc;` |
|     - |  1379 | `	}` |
|    21 |  1380 | `	SplMembersLoad(pThis,pMembers);` |
|    21 |  1381 | `	return PH7_OK;` |
|    19 |  1382 | `}` |
|     - |  1383 | `/*` |
|     - |  1384 | ` * ---------------------------------------------------------------------------` |
|     - |  1385 | ` * php's presentation for the array store (ph7_class::xPresent).` |
|     - |  1386 | ` *` |
|     - |  1387 | ` * php has two handlers here and they DISAGREE, which is the whole reason the hook` |
|     - |  1388 | `` * is told which is asking. `spl_array_get_debug_info` always shows ONE entry —`` |
|     - |  1389 | ` * the storage under its MANGLED private name — after whatever real properties the` |
|     - |  1390 | `` * instance has; `spl_array_get_properties_for` answers the storage's ELEMENTS`` |
|     - |  1391 | `` * directly for the var_export / (array) / json purposes, with no `storage` key at`` |
|     - |  1392 | ` * all, and hands back the ordinary property table when STD_PROP_LIST is set. The` |
|     - |  1393 | ` * flag is therefore visible on one surface and invisible on the other: a` |
|     - |  1394 | ` * STD_PROP_LIST ArrayObject still var_dumps its storage.` |
|     - |  1395 | ` *` |
|     - |  1396 | ` * The mangled name always spells the ROOT class, never the receiver's: a` |
|     - |  1397 | `` * RecursiveArrayIterator shows `["storage":"ArrayIterator":private]`.`` |
|     - |  1398 | ` * ---------------------------------------------------------------------------` |
|     - |  1399 | ` */` |
|    12 |  1400 | `static int SplStoreMangledKey(ph7_class_instance *pThis,char *zBuf,int nBuf)` |
|     1 |  1401 | `{` |
|    13 |  1402 | `	const char *zRoot = "ArrayObject";` |
|     - |  1403 | `	ph7_class *pClass;` |
|    13 |  1404 | `	int nRoot,nOut = 0;` |
|    25 |  1405 | `	for( pClass = pThis->pClass ; pClass ; pClass = pClass->pBase ){` |
|    16 |  1406 | `		if( pClass->sName.nByte == sizeof("ArrayIterator")-1` |
|    11 |  1407 | `		 && SyMemcmp(pClass->sName.zString,"ArrayIterator",sizeof("ArrayIterator")-1) == 0 ){` |
|     5 |  1408 | `			zRoot = "ArrayIterator";` |
|     5 |  1409 | `			break;` |
|     - |  1410 | `		}` |
|     7 |  1411 | `	}` |
|    13 |  1412 | `	nRoot = (int)SyStrlen(zRoot);` |
|    13 |  1413 | `	if( nRoot + (int)sizeof("\0\0storage") > nBuf ){` |
|   ! 0 |  1414 | `		return 0;` |
|     - |  1415 | `	}` |
|    13 |  1416 | `	zBuf[nOut++] = 0;` |
|    13 |  1417 | `	SyMemcpy(zRoot,&zBuf[nOut],(sxu32)nRoot);` |
|    13 |  1418 | `	nOut += nRoot;` |
|    13 |  1419 | `	zBuf[nOut++] = 0;` |
|    13 |  1420 | `	SyMemcpy("storage",&zBuf[nOut],sizeof("storage")-1);` |
|    13 |  1421 | `	nOut += (int)sizeof("storage")-1;` |
|    13 |  1422 | `	return nOut;` |
|     7 |  1423 | `}` |
|    24 |  1424 | `static int SplPresentWalk(ph7_value *pKey,ph7_value *pVal,void *pUserData)` |
|     2 |  1425 | `{` |
|    26 |  1426 | `	ph7_array_add_elem((ph7_value *)pUserData,pKey,pVal);` |
|    26 |  1427 | `	return PH7_OK;` |
|     2 |  1428 | `}` |
|    36 |  1429 | `static sxi32 SplStorePresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|     2 |  1430 | `{` |
|    38 |  1431 | `	ph7_value *pStore = pThis ? PH7_NativeAttr(pThis,SPL_D) : 0;` |
|    38 |  1432 | `	if( bDebug ){` |
|     - |  1433 | `		ph7_value sKey;` |
|     - |  1434 | `		char zKey[64];` |
|     - |  1435 | `		int nKey;` |
|     - |  1436 | `		/* The instance's OWN properties come first — php's debug info starts from` |
|     - |  1437 | `		 * the standard table and appends the storage entry to it. */` |
|    13 |  1438 | `		SplAddMembers(&(*pVm),pThis,pOut);` |
|    13 |  1439 | `		nKey = SplStoreMangledKey(pThis,zKey,(int)sizeof(zKey));` |
|    13 |  1440 | `		if( nKey > 0 && pStore ){` |
|    13 |  1441 | `			PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|    13 |  1442 | `			PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKey);` |
|    13 |  1443 | `			ph7_array_add_elem(pOut,&sKey,pStore);` |
|    13 |  1444 | `			PH7_MemObjRelease(&sKey);` |
|     6 |  1445 | `		}` |
|    13 |  1446 | `		return SXRET_OK;` |
|     - |  1447 | `	}` |
|    26 |  1448 | `	if( (PH7_NativeAttrInt(pThis,SPL_F) & SPL_STD_PROP_LIST) != 0 ){` |
|     5 |  1449 | `		SplAddMembers(&(*pVm),pThis,pOut);` |
|     5 |  1450 | `		return SXRET_OK;` |
|     - |  1451 | `	}` |
|    22 |  1452 | `	if( pStore && (pStore->iFlags & MEMOBJ_HASHMAP) != 0 ){` |
|    22 |  1453 | `		ph7_array_walk(pStore,SplPresentWalk,pOut);` |
|    10 |  1454 | `	}` |
|    22 |  1455 | `	return SXRET_OK;` |
|    20 |  1456 | `}` |
|    16 |  1457 | `static int vm_builtin_ArrayObject_getIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  1458 | `{` |
|    18 |  1459 | `	ph7_vm *pVm = pCtx->pVm;` |
|    18 |  1460 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  1461 | `	ph7_class_instance *pIt;` |
|     - |  1462 | `	ph7_class *pClass;` |
|    18 |  1463 | `	const char *zName = 0;` |
|    18 |  1464 | `	int nName = 0;` |
|     - |  1465 | `	ph7_value *pSlot;` |
|     - |  1466 | `	ph7_class_method *pCons;` |
|     8 |  1467 | `	SXUNUSED(nArg);` |
|     8 |  1468 | `	SXUNUSED(apArg);` |
|    18 |  1469 | `	if( pThis == 0 ){` |
|   ! 0 |  1470 | `		return PH7_OK;` |
|     - |  1471 | `	}` |
|    18 |  1472 | `	PH7_NativeAttrStr(pThis,SPL_IT,&zName,&nName);` |
|    18 |  1473 | `	if( nName < 1 ){` |
|   ! 0 |  1474 | `		zName = "ArrayIterator";` |
|   ! 0 |  1475 | `		nName = (int)sizeof("ArrayIterator")-1;` |
|   ! 0 |  1476 | `	}` |
|    18 |  1477 | `	pClass = PH7_VmExtractClass(pVm,zName,(sxu32)nName,FALSE,0);` |
|    18 |  1478 | `	if( pClass == 0 ){` |
|   ! 0 |  1479 | `		return PH7_OK;` |
|     - |  1480 | `	}` |
|    18 |  1481 | `	pIt = PH7_NewClassInstance(pVm,pClass);` |
|    18 |  1482 | `	if( pIt == 0 ){` |
|   ! 0 |  1483 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  1484 | `	}` |
|    18 |  1485 | `	pIt->iRef++;` |
|    18 |  1486 | `	pSlot = SplStoreSlot(pVm,pThis);` |
|    18 |  1487 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|    18 |  1488 | `	if( pCons && pSlot ){` |
|     - |  1489 | ``		/* `new $c($this->__d)`: the iterator gets a COPY of the store, as the PHP did —`` |
|     - |  1490 | `		 * a user subclass of ArrayIterator runs its own constructor here. */` |
|     - |  1491 | `		ph7_value *apCtor[1];` |
|    18 |  1492 | `		apCtor[0] = pSlot;` |
|    18 |  1493 | `		PH7_VmCallClassMethod(pVm,pIt,pCons,0,1,apCtor);` |
|     8 |  1494 | `	}` |
|    18 |  1495 | `	PH7_NativeResultObject(pCtx,pIt);` |
|    18 |  1496 | `	PH7_ClassInstanceUnref(pIt);` |
|    18 |  1497 | `	return PH7_OK;` |
|    10 |  1498 | `}` |
|     - |  1499 | `/*` |
|     - |  1500 | ` * ARRAY_AS_PROPS (flag 2) reaches the store through the four magic accessors, which is how` |
|     - |  1501 | ` * the PHP did it. php has no such methods — it implements the flag in its property handler,` |
|     - |  1502 | `` * so `getMethods()` does not list them (a surface divergence carried over, §7.4).`` |
|     - |  1503 | ` */` |
|     4 |  1504 | `static int vm_builtin_ArrayObject_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1505 | `{` |
|     5 |  1506 | `	ph7_vm *pVm = pCtx->pVm;` |
|     5 |  1507 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  1508 | `	ph7_hashmap *pMap;` |
|     5 |  1509 | `	ph7_hashmap_node *pNode = 0;` |
|     5 |  1510 | `	if( pThis == 0 \|\| nArg < 1 \|\| (PH7_NativeAttrInt(pThis,SPL_F) & 2) == 0 ){` |
|   ! 0 |  1511 | `		ph7_result_null(pCtx);` |
|   ! 0 |  1512 | `		return PH7_OK;` |
|     - |  1513 | `	}` |
|     5 |  1514 | `	pMap = SplStore(pVm,pThis);` |
|     5 |  1515 | `	if( pMap && PH7_HashmapLookup(pMap,apArg[0],&pNode) == SXRET_OK ){` |
|     5 |  1516 | `		ph7_result_value(pCtx,(ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx));` |
|     5 |  1517 | `		return PH7_OK;` |
|     - |  1518 | `	}` |
|     - |  1519 | ``	/* `?? null`: a missing key must not raise the undefined-key warning from in here —`` |
|     - |  1520 | `	 * php reports the missing PROPERTY, and PHL's magic-read path already does. */` |
|   ! 0 |  1521 | `	ph7_result_null(pCtx);` |
|   ! 0 |  1522 | `	return PH7_OK;` |
|     3 |  1523 | `}` |
|     4 |  1524 | `static int vm_builtin_ArrayObject_set(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1525 | `{` |
|     5 |  1526 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  1527 | `	ph7_hashmap *pMap;` |
|     5 |  1528 | `	if( pThis == 0 \|\| nArg < 2 \|\| (PH7_NativeAttrInt(pThis,SPL_F) & 2) == 0 ){` |
|   ! 0 |  1529 | `		return PH7_OK;` |
|     - |  1530 | `	}` |
|     5 |  1531 | `	pMap = SplStore(pCtx->pVm,pThis);` |
|     5 |  1532 | `	if( pMap ){` |
|     5 |  1533 | `		PH7_HashmapInsert(pMap,apArg[0],apArg[1]);` |
|     2 |  1534 | `	}` |
|     5 |  1535 | `	return PH7_OK;` |
|     3 |  1536 | `}` |
|    10 |  1537 | `static int vm_builtin_ArrayObject_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1538 | `{` |
|    11 |  1539 | `	ph7_vm *pVm = pCtx->pVm;` |
|    11 |  1540 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  1541 | `	ph7_hashmap *pMap;` |
|    11 |  1542 | `	ph7_hashmap_node *pNode = 0;` |
|    11 |  1543 | `	int bSet = 0;` |
|    11 |  1544 | `	if( pThis && nArg > 0 && (PH7_NativeAttrInt(pThis,SPL_F) & 2) ){` |
|    11 |  1545 | `		pMap = SplStore(pVm,pThis);` |
|    11 |  1546 | `		if( pMap && PH7_HashmapLookup(pMap,apArg[0],&pNode) == SXRET_OK ){` |
|     5 |  1547 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|     5 |  1548 | `			bSet = pVal && (pVal->iFlags & MEMOBJ_NULL) == 0; /* isset(), not exists */` |
|     2 |  1549 | `		}` |
|     5 |  1550 | `	}` |
|    11 |  1551 | `	ph7_result_bool(pCtx,bSet);` |
|    11 |  1552 | `	return PH7_OK;` |
|     1 |  1553 | `}` |
|     2 |  1554 | `static int vm_builtin_ArrayObject_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1555 | `{` |
|     3 |  1556 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  1557 | `	ph7_hashmap *pMap;` |
|     3 |  1558 | `	ph7_hashmap_node *pNode = 0;` |
|     3 |  1559 | `	if( pThis && nArg > 0 && (PH7_NativeAttrInt(pThis,SPL_F) & 2) ){` |
|     3 |  1560 | `		pMap = SplStore(pCtx->pVm,pThis);` |
|     3 |  1561 | `		if( pMap && PH7_HashmapLookup(pMap,apArg[0],&pNode) == SXRET_OK ){` |
|     3 |  1562 | `			PH7_HashmapUnlinkNode(pNode,TRUE);` |
|     1 |  1563 | `		}` |
|     1 |  1564 | `	}` |
|     3 |  1565 | `	return PH7_OK;` |
|     1 |  1566 | `}` |
|     - |  1567 | `/*` |
|     - |  1568 | ` * Declare both classes plus SeekableIterator, which ArrayIterator implements and which` |
|     - |  1569 | ` * therefore cannot wait for the chunk. RecursiveArrayIterator still lives there and extends` |
|     - |  1570 | ` * ArrayIterator, so this install has to run BEFORE the chunk is evaluated.` |
|     - |  1571 | ` */` |
|  5740 |  1572 | `static sxi32 VmInstallSplStore(ph7_vm *pVm)` |
|     5 |  1573 | `{` |
|     - |  1574 | `	static const PH7_NativeMethodDef aSeekMethod[] = {` |
|     - |  1575 | `		{ "seek", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "int $offset", 0, 0 },` |
|     - |  1576 | `	};` |
|     - |  1577 | `	static const PH7_NativePropDef aItProp[] = {` |
|     - |  1578 | `		{ SPL_D, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  1579 | `		{ SPL_F, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  1580 | `	};` |
|     - |  1581 | `	static const PH7_NativePropDef aObjProp[] = {` |
|     - |  1582 | `		{ SPL_D,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  1583 | `		{ SPL_F,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  1584 | `		{ SPL_IT, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "ArrayIterator", 0.0 }, 0 },` |
|     - |  1585 | `	};` |
|     - |  1586 | `	static const PH7_NativeConstDef aConst[] = {` |
|     - |  1587 | `		{ "STD_PROP_LIST",  PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1, 0, 0.0 },` |
|     - |  1588 | `		{ "ARRAY_AS_PROPS", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2, 0, 0.0 },` |
|     - |  1589 | `	};` |
|     - |  1590 | `	/* php's declaration order, which is the order Reflection reports. */` |
|     - |  1591 | `	static const PH7_NativeMethodDef aItMethod[] = {` |
|     - |  1592 | `		{ "__construct",  PH7_MOD_PUBLIC, "object\|array $array = [], int $flags = 0", 0,` |
|     - |  1593 | `		  vm_builtin_ArrayIterator_construct },` |
|     - |  1594 | `		{ "offsetExists", PH7_MOD_PUBLIC, "mixed $key", "@bool", vm_builtin_SplStore_offsetExists },` |
|     - |  1595 | `		{ "offsetGet",    PH7_MOD_PUBLIC, "mixed $key", "@mixed", vm_builtin_SplStore_offsetGet },` |
|     - |  1596 | `		{ "offsetSet",    PH7_MOD_PUBLIC, "mixed $key, mixed $value", "@void", vm_builtin_SplStore_offsetSet },` |
|     - |  1597 | `		{ "offsetUnset",  PH7_MOD_PUBLIC, "mixed $key", "@void", vm_builtin_SplStore_offsetUnset },` |
|     - |  1598 | `		{ "append",       PH7_MOD_PUBLIC, "mixed $value", "@void", vm_builtin_SplStore_append },` |
|     - |  1599 | `		{ "getArrayCopy", PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplStore_getArrayCopy },` |
|     - |  1600 | `		{ "count",        PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplStore_count },` |
|     - |  1601 | `		{ "getFlags",     PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplStore_getFlags },` |
|     - |  1602 | `		{ "setFlags",     PH7_MOD_PUBLIC, "int $flags", "@void", vm_builtin_SplStore_setFlags },` |
|     - |  1603 | `		{ "asort",        PH7_MOD_PUBLIC, "int $flags = 0", "@true", vm_builtin_SplStore_asort },` |
|     - |  1604 | `		{ "ksort",        PH7_MOD_PUBLIC, "int $flags = 0", "@true", vm_builtin_SplStore_ksort },` |
|     - |  1605 | `		{ "uasort",       PH7_MOD_PUBLIC, "callable $callback", "@true", vm_builtin_SplStore_uasort },` |
|     - |  1606 | `		{ "uksort",       PH7_MOD_PUBLIC, "callable $callback", "@true", vm_builtin_SplStore_uksort },` |
|     - |  1607 | `		{ "natsort",      PH7_MOD_PUBLIC, "", "@true", vm_builtin_SplStore_natsort },` |
|     - |  1608 | `		{ "natcasesort",  PH7_MOD_PUBLIC, "", "@true", vm_builtin_SplStore_natcasesort },` |
|     - |  1609 | `		{ "current",      PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_ArrayIterator_current },` |
|     - |  1610 | `		{ "key",          PH7_MOD_PUBLIC, "", "@string\|int\|null", vm_builtin_ArrayIterator_key },` |
|     - |  1611 | `		{ "next",         PH7_MOD_PUBLIC, "", "@void", vm_builtin_ArrayIterator_next },` |
|     - |  1612 | `		{ "rewind",       PH7_MOD_PUBLIC, "", "@void", vm_builtin_ArrayIterator_rewind },` |
|     - |  1613 | `		{ "valid",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ArrayIterator_valid },` |
|     - |  1614 | `		{ "seek",         PH7_MOD_PUBLIC, "int $offset", "@void", vm_builtin_ArrayIterator_seek },` |
|     - |  1615 | `		{ "__serialize",  PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplStore_serializeMagic },` |
|     - |  1616 | `		{ "__unserialize", PH7_MOD_PUBLIC, "array $data", "@void",` |
|     - |  1617 | `		  vm_builtin_SplStore_unserializeMagic },` |
|     - |  1618 | `	};` |
|     - |  1619 | `	static const PH7_NativeMethodDef aObjMethod[] = {` |
|     - |  1620 | `		{ "__construct",      PH7_MOD_PUBLIC,` |
|     - |  1621 | `		  "object\|array $array = [], int $flags = 0, string $iteratorClass = 'ArrayIterator'", 0,` |
|     - |  1622 | `		  vm_builtin_ArrayObject_construct },` |
|     - |  1623 | `		{ "offsetExists",     PH7_MOD_PUBLIC, "mixed $key", "@bool", vm_builtin_SplStore_offsetExists },` |
|     - |  1624 | `		{ "offsetGet",        PH7_MOD_PUBLIC, "mixed $key", "@mixed", vm_builtin_SplStore_offsetGet },` |
|     - |  1625 | `		{ "offsetSet",        PH7_MOD_PUBLIC, "mixed $key, mixed $value", "@void", vm_builtin_SplStore_offsetSet },` |
|     - |  1626 | `		{ "offsetUnset",      PH7_MOD_PUBLIC, "mixed $key", "@void", vm_builtin_SplStore_offsetUnset },` |
|     - |  1627 | `		{ "append",           PH7_MOD_PUBLIC, "mixed $value", "@void", vm_builtin_SplStore_append },` |
|     - |  1628 | `		{ "getArrayCopy",     PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplStore_getArrayCopy },` |
|     - |  1629 | `		{ "count",            PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplStore_count },` |
|     - |  1630 | `		{ "getFlags",         PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplStore_getFlags },` |
|     - |  1631 | `		{ "setFlags",         PH7_MOD_PUBLIC, "int $flags", "@void", vm_builtin_SplStore_setFlags },` |
|     - |  1632 | `		{ "asort",            PH7_MOD_PUBLIC, "int $flags = 0", "@true", vm_builtin_SplStore_asort },` |
|     - |  1633 | `		{ "ksort",            PH7_MOD_PUBLIC, "int $flags = 0", "@true", vm_builtin_SplStore_ksort },` |
|     - |  1634 | `		{ "uasort",           PH7_MOD_PUBLIC, "callable $callback", "@true", vm_builtin_SplStore_uasort },` |
|     - |  1635 | `		{ "uksort",           PH7_MOD_PUBLIC, "callable $callback", "@true", vm_builtin_SplStore_uksort },` |
|     - |  1636 | `		{ "natsort",          PH7_MOD_PUBLIC, "", "@true", vm_builtin_SplStore_natsort },` |
|     - |  1637 | `		{ "natcasesort",      PH7_MOD_PUBLIC, "", "@true", vm_builtin_SplStore_natcasesort },` |
|     - |  1638 | `		{ "exchangeArray",    PH7_MOD_PUBLIC, "object\|array $array", "@array", vm_builtin_ArrayObject_exchangeArray },` |
|     - |  1639 | `		{ "getIterator",      PH7_MOD_PUBLIC, "", "@Iterator", vm_builtin_ArrayObject_getIterator },` |
|     - |  1640 | `		{ "setIteratorClass", PH7_MOD_PUBLIC, "string $iteratorClass", "@void", vm_builtin_ArrayObject_setIteratorClass },` |
|     - |  1641 | `		{ "getIteratorClass", PH7_MOD_PUBLIC, "", "@string", vm_builtin_ArrayObject_getIteratorClass },` |
|     - |  1642 | `		{ "__get",            PH7_MOD_PUBLIC, "$name", 0, vm_builtin_ArrayObject_get },` |
|     - |  1643 | `		{ "__set",            PH7_MOD_PUBLIC, "$name, $value", 0, vm_builtin_ArrayObject_set },` |
|     - |  1644 | `		{ "__isset",          PH7_MOD_PUBLIC, "$name", 0, vm_builtin_ArrayObject_isset },` |
|     - |  1645 | `		{ "__unset",          PH7_MOD_PUBLIC, "$name", 0, vm_builtin_ArrayObject_unset },` |
|     - |  1646 | `		{ "__serialize",      PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplStore_serializeMagic },` |
|     - |  1647 | `		{ "__unserialize",    PH7_MOD_PUBLIC, "array $data", "@void",` |
|     - |  1648 | `		  vm_builtin_SplStore_unserializeMagic },` |
|     - |  1649 | `	};` |
|     - |  1650 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - |  1651 | ``		/* `interface X extends Iterator` is a PARENT, not an implemented interface:`` |
|     - |  1652 | `		 * the compiler puts it in pBase and Reflection walks pBase to answer which` |
|     - |  1653 | `		 * class DECLARED an inherited method. Naming it in zImplements instead made` |
|     - |  1654 | `		 * current()/key()/next()/rewind()/valid() report this interface as their` |
|     - |  1655 | `		 * declaring class where php reports Iterator. */` |
|     - |  1656 | `		{ "SeekableIterator", "Iterator", 0, PH7_CLASS_INTERFACE,` |
|     - |  1657 | `		  aSeekMethod, SX_ARRAYSIZE(aSeekMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  1658 | `		/* PH7_CLASS_DIM_WRITABLE: php's spl_array read_dimension hands back the` |
|     - |  1659 | ``		 * REAL element for a write fetch, so `$ao['k']['n'] = v` lands — unlike`` |
|     - |  1660 | `		 * SplFixedArray / SplDoublyLinkedList / SplObjectStorage, which keep the` |
|     - |  1661 | `		 * standard handler and get php's indirect-modification notice. */` |
|     - |  1662 | `		{ "ArrayIterator", 0, "SeekableIterator,ArrayAccess,Countable", PH7_CLASS_DIM_WRITABLE,` |
|     - |  1663 | `		  aItMethod, SX_ARRAYSIZE(aItMethod), aConst, SX_ARRAYSIZE(aConst),` |
|     - |  1664 | `		  aItProp, SX_ARRAYSIZE(aItProp), 0, 0, SplStorePresent },` |
|     - |  1665 | `		{ "ArrayObject", 0, "IteratorAggregate,ArrayAccess,Countable", PH7_CLASS_DIM_WRITABLE,` |
|     - |  1666 | `		  aObjMethod, SX_ARRAYSIZE(aObjMethod), aConst, SX_ARRAYSIZE(aConst),` |
|     - |  1667 | `		  aObjProp, SX_ARRAYSIZE(aObjProp), 0, 0, SplStorePresent },` |
|     - |  1668 | `	};` |
|  5745 |  1669 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 |  1670 | `}` |
|     - |  1671 | `/*` |
|     - |  1672 | ` * ---------------------------------------------------------------------------` |
|     - |  1673 | ` * The SPL DUAL ITERATORS: IteratorIterator and the decorators built on it.` |
|     - |  1674 | ` *` |
|     - |  1675 | `` * php's `spl_dual_it_object` is a CACHE, and that is the whole design. rewind()`` |
|     - |  1676 | ` * and next() move the INNER iterator and then COPY its current()/key() onto the` |
|     - |  1677 | ` * decorator; valid(), current() and key() answer out of that copy and never reach` |
|     - |  1678 | ` * the inner iterator again. The chunk forwarded all five live, which is three` |
|     - |  1679 | ` * observable divergences at once: a fresh decorator was valid() BEFORE rewind()` |
|     - |  1680 | ` * (php answers false — nothing has been fetched yet), current() followed an inner` |
|     - |  1681 | ` * iterator that had been moved behind the decorator's back (php answers what it` |
|     - |  1682 | ` * cached), and a decorator left past the end still answered the inner's stale` |
|     - |  1683 | ` * key(). Everything below is written around the cache because the cache IS the` |
|     - |  1684 | ` * class.` |
|     - |  1685 | ` *` |
|     - |  1686 | `` * Two slots hold what php holds in two fields: `__in` is `inner.zobject` — the`` |
|     - |  1687 | `` * object getInnerIterator() answers — and `__it` is `inner.iterator`, the Iterator`` |
|     - |  1688 | ` * actually driven. They differ for exactly one input: an IteratorAggregate whose` |
|     - |  1689 | ` * getIterator() answers another IteratorAggregate. php unwraps ONE level in the` |
|     - |  1690 | ` * constructor and lets the engine's get_iterator handler unwrap the rest at` |
|     - |  1691 | `` * iteration time, so `new IteratorIterator($aggOfAgg)` answers the inner AGGREGATE`` |
|     - |  1692 | `` * from getInnerIterator() and still iterates. The chunk's `while` loop unwrapped`` |
|     - |  1693 | ` * to the bottom and answered the ArrayIterator instead.` |
|     - |  1694 | ` */` |
|     - |  1695 | `#define IT_IN  "__in"   /* php's inner.zobject: what getInnerIterator() answers */` |
|     - |  1696 | `#define IT_IT  "__it"   /* php's inner.iterator: the Iterator actually driven */` |
|     - |  1697 | `#define IT_CD  "__cd"   /* the cached current() */` |
|     - |  1698 | `#define IT_CK  "__ck"   /* the cached key() */` |
|     - |  1699 | `#define IT_CF  "__cf"   /* 1 while the cached pair is live (php's IS_UNDEF check) */` |
|     - |  1700 | `#define IT_CP  "__cp"   /* php's current.pos */` |
|     - |  1701 | `#define IT_OFF "__off"  /* LimitIterator's offset */` |
|     - |  1702 | `#define IT_LIM "__lim"  /* LimitIterator's count, -1 for "all" */` |
|     - |  1703 | `#define IT_CB  "__cb"   /* CallbackFilterIterator's callback */` |
|     - |  1704 | ``#define AP_LIST "__ai"  /* AppendIterator's php `u.append.zarrayit`: the real ArrayIterator`` |
|     - |  1705 | `                         * holding everything append()ed, whose OWN cursor is php's` |
|     - |  1706 | ``                         * `u.append.iterator` -- one position, which is why`` |
|     - |  1707 | `                         * getArrayIterator()->rewind() moves getIteratorIndex(). */` |
|     - |  1708 |  |
|     - |  1709 | `/*` |
|     - |  1710 | ` * php's SPL_FETCH_AND_CHECK_DUAL_IT: a subclass whose constructor never called` |
|     - |  1711 | ` * parent::__construct() has no inner iterator, and php refuses every method on it` |
|     - |  1712 | ` * rather than answering a null-flavoured nothing.` |
|     - |  1713 | ` */` |
|    34 |  1714 | `static sxi32 DualNotReady(ph7_context *pCtx)` |
|     1 |  1715 | `{` |
|    35 |  1716 | `	return PH7_VmThrowException(pCtx,"Error",` |
|     - |  1717 | `		"The object is in an invalid state as the parent constructor was not called");` |
|     1 |  1718 | `}` |
|  4952 |  1719 | `static ph7_class_instance * DualDriver(ph7_class_instance *pThis)` |
|     2 |  1720 | `{` |
|  4954 |  1721 | `	return pThis ? PH7_NativeAttrObj(pThis,IT_IT) : 0;` |
|     2 |  1722 | `}` |
|  1932 |  1723 | `static int DualFilled(ph7_class_instance *pThis)` |
|     2 |  1724 | `{` |
|  1934 |  1725 | `	return pThis && PH7_NativeAttrInt(pThis,IT_CF) != 0;` |
|     2 |  1726 | `}` |
|     - |  1727 | `/*` |
|     - |  1728 | `` * php's SPL_FETCH_AND_CHECK_DUAL_IT tests `dit_type`, which means "the constructor`` |
|     - |  1729 | ` * ran" -- and for every decorator but one that is the same thing as "an inner` |
|     - |  1730 | ` * iterator exists". AppendIterator's constructor takes NO iterator: it builds an` |
|     - |  1731 | ` * empty list and is immediately usable (valid() false, current()/key() null, no` |
|     - |  1732 | ` * refusal), so its readiness lives in the list slot instead.` |
|     - |  1733 | ` */` |
|  2666 |  1734 | `static int DualReady(ph7_class_instance *pThis)` |
|     2 |  1735 | `{` |
|  4070 |  1736 | `	return pThis && (PH7_NativeAttrObj(pThis,IT_IT) != 0` |
|  1402 |  1737 | `		\|\| PH7_NativeAttrObj(pThis,AP_LIST) != 0);` |
|     2 |  1738 | `}` |
|     - |  1739 | `/*` |
|     - |  1740 | ` * Assign one of the instance's own slots. The slot pointer is re-resolved here on` |
|     - |  1741 | ` * purpose: it lives inside pVm->aMemObj, a SySet that REALLOCATES as the VM` |
|     - |  1742 | ` * reserves objects, so any pointer taken before a call into user code (and every` |
|     - |  1743 | ` * inner->current() is one) may be stale by the time the call returns.` |
|     - |  1744 | ` */` |
|  1430 |  1745 | `static void DualSetSlot(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,ph7_value *pVal)` |
|     2 |  1746 | `{` |
|  1432 |  1747 | `	ph7_value *pSlot = PH7_NativeAttr(pThis,zName);` |
|   715 |  1748 | `	SXUNUSED(pVm);` |
|  1432 |  1749 | `	if( pSlot ){` |
|  1432 |  1750 | `		PH7_MemObjStore(pVal,pSlot);` |
|   715 |  1751 | `	}` |
|  1432 |  1752 | `}` |
|     - |  1753 | `/* php's spl_dual_it_free: drop the cached pair. */` |
|  1686 |  1754 | `static void DualFree(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     2 |  1755 | `{` |
|     - |  1756 | `	ph7_value *pSlot;` |
|  1688 |  1757 | `	if( pThis == 0 ){` |
|   ! 0 |  1758 | `		return;` |
|     - |  1759 | `	}` |
|  1688 |  1760 | `	pSlot = PH7_NativeAttr(pThis,IT_CD);` |
|  1688 |  1761 | `	if( pSlot ){ PH7_MemObjRelease(pSlot); }` |
|  1688 |  1762 | `	pSlot = PH7_NativeAttr(pThis,IT_CK);` |
|  1688 |  1763 | `	if( pSlot ){ PH7_MemObjRelease(pSlot); }` |
|  1688 |  1764 | `	PH7_NativeSetAttrInt(pVm,pThis,IT_CF,0);` |
|   845 |  1765 | `}` |
|     - |  1766 | `/* Call a zero-argument method on the driven iterator, propagating a throw (rule:` |
|     - |  1767 | ` * a native body that answers PH7_OK with an exception in flight lets the caller` |
|     - |  1768 | ` * carry on). A missing method is the foreach opcode's leniency, not an error. */` |
|  3416 |  1769 | `static sxi32 DualCall(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nLen,` |
|     - |  1770 | `	ph7_value *pResult)` |
|     2 |  1771 | `{` |
|  3418 |  1772 | `	ph7_class_instance *pIn = DualDriver(pThis);` |
|  3418 |  1773 | `	if( pIn == 0 ){` |
|    41 |  1774 | `		return SXRET_OK;` |
|     - |  1775 | `	}` |
|  3378 |  1776 | `	return VmIterCallMethod(pVm,pIn,zName,nLen,pResult);` |
|  1710 |  1777 | `}` |
|     - |  1778 | `/* php's spl_dual_it_valid: the INNER's valid(), not the cache's. */` |
|  1210 |  1779 | `static sxi32 DualInnerValid(ph7_vm *pVm,ph7_class_instance *pThis,int *pbValid)` |
|     2 |  1780 | `{` |
|     - |  1781 | `	ph7_value sVal;` |
|     - |  1782 | `	sxi32 rc;` |
|  1212 |  1783 | `	*pbValid = 0;` |
|  1212 |  1784 | `	PH7_MemObjInit(pVm,&sVal);` |
|  1212 |  1785 | `	rc = DualCall(pVm,pThis,"valid",sizeof("valid")-1,&sVal);` |
|  1212 |  1786 | `	if( rc == SXRET_OK ){` |
|  1212 |  1787 | `		PH7_MemObjToBool(&sVal);          /* a STATUS, not the answer */` |
|  1212 |  1788 | `		*pbValid = sVal.x.iVal != 0;` |
|   605 |  1789 | `	}` |
|  1212 |  1790 | `	PH7_MemObjRelease(&sVal);` |
|  1212 |  1791 | `	return rc;` |
|     2 |  1792 | `}` |
|     - |  1793 | `/*` |
|     - |  1794 | ``  * php's spl_dual_it_fetch: refill the cache from the inner iterator. `bCheckMore` `` |
|     - |  1795 | ` * is php's check_more — false means "the caller already knows the inner is valid",` |
|     - |  1796 | ` * which is how LimitIterator's seek and InfiniteIterator's wrap-around fetch.` |
|     - |  1797 | ` */` |
|   840 |  1798 | `static sxi32 DualFetch(ph7_vm *pVm,ph7_class_instance *pThis,int bCheckMore)` |
|     2 |  1799 | `{` |
|     - |  1800 | `	ph7_value sVal;` |
|     - |  1801 | `	sxi32 rc;` |
|   842 |  1802 | `	int bValid = 1;` |
|   842 |  1803 | `	DualFree(pVm,pThis);` |
|   842 |  1804 | `	if( DualDriver(pThis) == 0 ){` |
|     5 |  1805 | `		return SXRET_OK;` |
|     - |  1806 | `	}` |
|   838 |  1807 | `	if( bCheckMore ){` |
|   722 |  1808 | `		rc = DualInnerValid(pVm,pThis,&bValid);` |
|   722 |  1809 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  1810 | `			return rc;` |
|     - |  1811 | `		}` |
|   360 |  1812 | `	}` |
|   838 |  1813 | `	if( !bValid ){` |
|   186 |  1814 | `		return SXRET_OK;` |
|     - |  1815 | `	}` |
|   654 |  1816 | `	PH7_MemObjInit(pVm,&sVal);` |
|   654 |  1817 | `	rc = DualCall(pVm,pThis,"current",sizeof("current")-1,&sVal);` |
|   654 |  1818 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  1819 | `		PH7_MemObjRelease(&sVal);` |
|   ! 0 |  1820 | `		return rc;` |
|     - |  1821 | `	}` |
|   654 |  1822 | `	DualSetSlot(pVm,pThis,IT_CD,&sVal);` |
|   654 |  1823 | `	PH7_MemObjRelease(&sVal);` |
|   654 |  1824 | `	PH7_MemObjInit(pVm,&sVal);` |
|   654 |  1825 | `	rc = DualCall(pVm,pThis,"key",sizeof("key")-1,&sVal);` |
|   654 |  1826 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  1827 | `		PH7_MemObjRelease(&sVal);` |
|   ! 0 |  1828 | `		DualFree(pVm,pThis);   /* php drops the half-filled pair when key() throws */` |
|   ! 0 |  1829 | `		return rc;` |
|     - |  1830 | `	}` |
|   654 |  1831 | `	DualSetSlot(pVm,pThis,IT_CK,&sVal);` |
|   654 |  1832 | `	PH7_MemObjRelease(&sVal);` |
|   654 |  1833 | `	PH7_NativeSetAttrInt(pVm,pThis,IT_CF,1);` |
|   654 |  1834 | `	return SXRET_OK;` |
|   422 |  1835 | `}` |
|     - |  1836 | `/* php's spl_dual_it_rewind: free, position back to zero, rewind the inner. */` |
|   356 |  1837 | `static sxi32 DualRewindInner(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     2 |  1838 | `{` |
|   358 |  1839 | `	DualFree(pVm,pThis);` |
|   358 |  1840 | `	PH7_NativeSetAttrInt(pVm,pThis,IT_CP,0);` |
|   358 |  1841 | `	return DualCall(pVm,pThis,"rewind",sizeof("rewind")-1,0);` |
|     2 |  1842 | `}` |
|     - |  1843 | `/*` |
|     - |  1844 | ``  * php's spl_dual_it_next: free, advance the inner, count the step. Its `do_free` `` |
|     - |  1845 | ` * is FALSE for exactly one caller — CachingIterator, which has just copied the` |
|     - |  1846 | ` * pair it is standing on and steps the inner one ahead of it, so dropping the` |
|     - |  1847 | ` * cache here would erase the very element the decorator answers.` |
|     - |  1848 | ` */` |
|   434 |  1849 | `static sxi32 DualNextInnerEx(ph7_vm *pVm,ph7_class_instance *pThis,int bFree)` |
|     2 |  1850 | `{` |
|     - |  1851 | `	sxi32 rc;` |
|   436 |  1852 | `	if( bFree ){` |
|   255 |  1853 | `		DualFree(pVm,pThis);` |
|   127 |  1854 | `	}` |
|   436 |  1855 | `	rc = DualCall(pVm,pThis,"next",sizeof("next")-1,0);` |
|   436 |  1856 | `	PH7_NativeSetAttrInt(pVm,pThis,IT_CP,PH7_NativeAttrInt(pThis,IT_CP)+1);` |
|   436 |  1857 | `	return rc;` |
|     2 |  1858 | `}` |
|   254 |  1859 | `static sxi32 DualNextInner(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  1860 | `{` |
|   255 |  1861 | `	return DualNextInnerEx(pVm,pThis,TRUE);` |
|     1 |  1862 | `}` |
|     - |  1863 | `/* Hand back a cached slot, or php's null for an empty cache. */` |
|   732 |  1864 | `static int DualResultSlot(ph7_context *pCtx,const char *zName)` |
|     2 |  1865 | `{` |
|   734 |  1866 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  1867 | `	ph7_value *pSlot;` |
|   734 |  1868 | `	if( !DualReady(pThis) ){` |
|   ! 0 |  1869 | `		return DualNotReady(pCtx);` |
|     - |  1870 | `	}` |
|   734 |  1871 | `	if( !DualFilled(pThis) ){` |
|    27 |  1872 | `		ph7_result_null(pCtx);` |
|    27 |  1873 | `		return PH7_OK;` |
|     - |  1874 | `	}` |
|   708 |  1875 | `	pSlot = PH7_NativeAttr(pThis,zName);` |
|   708 |  1876 | `	if( pSlot ){` |
|   708 |  1877 | `		ph7_result_value(pCtx,pSlot);` |
|   353 |  1878 | `	}` |
|   708 |  1879 | `	return PH7_OK;` |
|   368 |  1880 | `}` |
|     - |  1881 | `/*` |
|     - |  1882 | ` * The constructor every dual iterator shares. php words the "already built" refusal` |
|     - |  1883 | ` * with the DECLARING class's name and with getIterator() rather than __construct(),` |
|     - |  1884 | ` * so each class hands its own name in.` |
|     - |  1885 | ` */` |
|   408 |  1886 | `static sxi32 DualConstruct(ph7_context *pCtx,const char *zOwner,int nArg,ph7_value **apArg)` |
|     2 |  1887 | `{` |
|   410 |  1888 | `	ph7_vm *pVm = pCtx->pVm;` |
|   410 |  1889 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  1890 | `	ph7_class_instance *pObj;` |
|     - |  1891 | `	ph7_class *pIterCls, *pAggCls, *pTravCls;` |
|   410 |  1892 | `	ph7_class *pCast = 0;` |
|   410 |  1893 | `	ph7_class_instance *pHold = 0;   /* the unwrapped iterator, kept alive across levels */` |
|     - |  1894 | `	int nLevel;` |
|   410 |  1895 | `	if( pThis == 0 ){` |
|   ! 0 |  1896 | `		return PH7_OK;` |
|     - |  1897 | `	}` |
|   410 |  1898 | `	if( PH7_NativeAttrObj(pThis,IT_IN) != 0 ){` |
|     4 |  1899 | `		return PH7_VmThrowException(pCtx,"BadMethodCallException",` |
|     1 |  1900 | `			"%s::getIterator() must be called exactly once per instance",zOwner);` |
|     - |  1901 | `	}` |
|   408 |  1902 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 |  1903 | `		return PH7_OK;   /* the shared ZPP screen already refused a non-object */` |
|     - |  1904 | `	}` |
|   408 |  1905 | `	pObj = (ph7_class_instance *)apArg[0]->x.pOther;` |
|   408 |  1906 | `	pIterCls = PH7_VmExtractClass(pVm,"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|   408 |  1907 | `	pAggCls = PH7_VmExtractClass(pVm,"IteratorAggregate",sizeof("IteratorAggregate")-1,FALSE,0);` |
|   408 |  1908 | `	pTravCls = PH7_VmExtractClass(pVm,"Traversable",sizeof("Traversable")-1,FALSE,0);` |
|   408 |  1909 | `	if( pIterCls && PH7_VmInstanceOf(pObj->pClass,pIterCls) ){` |
|     - |  1910 | `		/* Already an Iterator: php ignores $class entirely on this path. */` |
|   398 |  1911 | `		PH7_NativeSetAttrObj(pVm,pThis,IT_IN,pObj);` |
|   398 |  1912 | `		PH7_NativeSetAttrObj(pVm,pThis,IT_IT,pObj);` |
|   398 |  1913 | `		return PH7_OK;` |
|     - |  1914 | `	}` |
|    11 |  1915 | `	if( nArg > 1 && (apArg[1]->iFlags & MEMOBJ_NULL) == 0 ){` |
|     - |  1916 | `		/* php's DOWNCAST: $class names the class whose getIterator() to run, which is` |
|     - |  1917 | `		 * how a subclass asks for its parent's traversal. It must be a base of the` |
|     - |  1918 | `		 * argument AND traversable itself. */` |
|     - |  1919 | `		int nName;` |
|     5 |  1920 | `		const char *zName = ph7_value_to_string(apArg[1],&nName);` |
|     5 |  1921 | `		pCast = nName > 0 ? PH7_VmExtractClass(pVm,zName,(sxu32)nName,FALSE,0) : 0;` |
|     4 |  1922 | `		if( pCast == 0 \|\| !PH7_VmInstanceOf(pObj->pClass,pCast)` |
|     3 |  1923 | `		 \|\| (pTravCls && !PH7_VmInstanceOf(pCast,pTravCls)) ){` |
|     5 |  1924 | `			return PH7_VmThrowException(pCtx,"LogicException",` |
|     - |  1925 | `				"Class to downcast to not found or not base class or does not implement Traversable");` |
|     - |  1926 | `		}` |
|   ! 0 |  1927 | `	}` |
|     - |  1928 | `	/*` |
|     - |  1929 | `	 * An IteratorAggregate: run getIterator() — the DOWNCAST class's when one was` |
|     - |  1930 | `	 * named — and keep its answer as the inner object. php stops after one level` |
|     - |  1931 | `	 * here; the loop below is the engine's get_iterator handler, which resolves the` |
|     - |  1932 | `	 * rest lazily, done eagerly because PHL drives the inner through the METHOD` |
|     - |  1933 | `	 * protocol and nothing else would unwrap it.` |
|     - |  1934 | `	 */` |
|     9 |  1935 | `	for( nLevel = 0 ; nLevel < 16 ; ++nLevel ){` |
|     9 |  1936 | `		ph7_class *pFrom = pCast ? pCast : pObj->pClass;` |
|     - |  1937 | `		ph7_class_method *pMethod;` |
|     - |  1938 | `		ph7_value sInner;` |
|     - |  1939 | `		sxi32 rc;` |
|     9 |  1940 | `		if( pAggCls == 0 \|\| !PH7_VmInstanceOf(pObj->pClass,pAggCls) ){` |
|   ! 0 |  1941 | `			break;` |
|     - |  1942 | `		}` |
|     9 |  1943 | `		pMethod = PH7_ClassExtractMethod(pFrom,"getIterator",sizeof("getIterator")-1);` |
|     9 |  1944 | `		if( pMethod == 0 ){` |
|   ! 0 |  1945 | `			break;` |
|     - |  1946 | `		}` |
|     9 |  1947 | `		PH7_MemObjInit(pVm,&sInner);` |
|     9 |  1948 | `		rc = PH7_VmCallClassMethod(pVm,pObj,pMethod,&sInner,0,0);` |
|     9 |  1949 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  1950 | `			PH7_MemObjRelease(&sInner);` |
|   ! 0 |  1951 | `			return rc;` |
|     - |  1952 | `		}` |
|     8 |  1953 | `		if( (sInner.iFlags & MEMOBJ_OBJ) == 0 \|\| sInner.x.pOther == 0` |
|     9 |  1954 | `		 \|\| (pTravCls && !PH7_VmInstanceOf(((ph7_class_instance *)sInner.x.pOther)->pClass,pTravCls)) ){` |
|   ! 0 |  1955 | `			SyString *pName = &pFrom->sName;` |
|   ! 0 |  1956 | `			PH7_MemObjRelease(&sInner);` |
|   ! 0 |  1957 | `			return PH7_VmThrowException(pCtx,"LogicException",` |
|   ! 0 |  1958 | `				"%z::getIterator() must return an object that implements Traversable",pName);` |
|     - |  1959 | `		}` |
|     9 |  1960 | `		pObj = (ph7_class_instance *)sInner.x.pOther;` |
|     9 |  1961 | `		pObj->iRef++;                 /* survive the release of the call result */` |
|     9 |  1962 | `		PH7_MemObjRelease(&sInner);` |
|     9 |  1963 | `		if( pHold ){` |
|     3 |  1964 | `			PH7_ClassInstanceUnref(pHold);` |
|     1 |  1965 | `		}` |
|     9 |  1966 | `		pHold = pObj;                 /* this function owns exactly one reference */` |
|     9 |  1967 | `		if( nLevel == 0 ){` |
|     - |  1968 | `			/* php's inner.zobject is the FIRST unwrap and nothing deeper. */` |
|     7 |  1969 | `			PH7_NativeSetAttrObj(pVm,pThis,IT_IN,pObj);` |
|     3 |  1970 | `		}` |
|     9 |  1971 | `		pCast = 0;` |
|     9 |  1972 | `		if( pIterCls && PH7_VmInstanceOf(pObj->pClass,pIterCls) ){` |
|     7 |  1973 | `			break;` |
|     - |  1974 | `		}` |
|     2 |  1975 | `	}` |
|     7 |  1976 | `	if( PH7_NativeAttrObj(pThis,IT_IN) == 0 ){` |
|   ! 0 |  1977 | `		PH7_NativeSetAttrObj(pVm,pThis,IT_IN,pObj);` |
|   ! 0 |  1978 | `	}` |
|     7 |  1979 | `	PH7_NativeSetAttrObj(pVm,pThis,IT_IT,pObj);` |
|     7 |  1980 | `	if( pHold ){` |
|     7 |  1981 | `		PH7_ClassInstanceUnref(pHold);   /* both slots hold their own now */` |
|     3 |  1982 | `	}` |
|     7 |  1983 | `	return PH7_OK;` |
|   206 |  1984 | `}` |
|    38 |  1985 | `static int vm_builtin_IteratorIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1986 | `{` |
|    39 |  1987 | `	return DualConstruct(pCtx,"IteratorIterator",nArg,apArg);` |
|     1 |  1988 | `}` |
|     2 |  1989 | `static int vm_builtin_FilterIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1990 | `{` |
|     3 |  1991 | `	return DualConstruct(pCtx,"FilterIterator",nArg,apArg);` |
|     1 |  1992 | `}` |
|     8 |  1993 | `static int vm_builtin_CallbackFilterIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1994 | `{` |
|     - |  1995 | `	ph7_class_instance *pThis;` |
|     - |  1996 | `	sxi32 rc;` |
|     9 |  1997 | `	if( nArg > 1 ){` |
|     - |  1998 | ``		/* The shared ZPP screen leaves `callable` to the builtin's own check (a string`` |
|     - |  1999 | `		 * satisfies the declared type; whether it NAMES a function does not), so php's` |
|     - |  2000 | `		 * "must be a valid callback, function "x" not found" only appears if the body` |
|     - |  2001 | `		 * asks for it — as every callback-taking builtin already does. */` |
|     9 |  2002 | `		rc = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",FALSE);` |
|     9 |  2003 | `		if( rc != PH7_OK ){` |
|     3 |  2004 | `			return rc;` |
|     - |  2005 | `		}` |
|     3 |  2006 | `	}` |
|     7 |  2007 | `	rc = DualConstruct(pCtx,"CallbackFilterIterator",nArg,apArg);` |
|     7 |  2008 | `	pThis = PH7_ContextThis(pCtx);` |
|     7 |  2009 | `	if( rc == PH7_OK && pThis && nArg > 1 ){` |
|     7 |  2010 | `		DualSetSlot(pCtx->pVm,pThis,IT_CB,apArg[1]);` |
|     3 |  2011 | `	}` |
|     7 |  2012 | `	return rc;` |
|     5 |  2013 | `}` |
|     8 |  2014 | `static int vm_builtin_InfiniteIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2015 | `{` |
|     9 |  2016 | `	return DualConstruct(pCtx,"InfiniteIterator",nArg,apArg);` |
|     1 |  2017 | `}` |
|     8 |  2018 | `static int vm_builtin_NoRewindIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2019 | `{` |
|     9 |  2020 | `	return DualConstruct(pCtx,"NoRewindIterator",nArg,apArg);` |
|     1 |  2021 | `}` |
|    40 |  2022 | `static int vm_builtin_Dual_getInnerIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2023 | `{` |
|    41 |  2024 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2025 | `	ph7_class_instance *pIn;` |
|    20 |  2026 | `	SXUNUSED(nArg);` |
|    20 |  2027 | `	SXUNUSED(apArg);` |
|    41 |  2028 | `	if( !DualReady(pThis) ){` |
|   ! 0 |  2029 | `		return DualNotReady(pCtx);` |
|     - |  2030 | `	}` |
|    41 |  2031 | `	pIn = PH7_NativeAttrObj(pThis,IT_IN);` |
|    41 |  2032 | `	if( pIn ){` |
|    39 |  2033 | `		SplResultBorrowed(pCtx,pIn);` |
|    20 |  2034 | `	}else{` |
|     3 |  2035 | `		ph7_result_null(pCtx);` |
|     - |  2036 | `	}` |
|    41 |  2037 | `	return PH7_OK;` |
|    21 |  2038 | `}` |
|   426 |  2039 | `static int vm_builtin_Dual_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2040 | `{` |
|   427 |  2041 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   213 |  2042 | `	SXUNUSED(nArg);` |
|   213 |  2043 | `	SXUNUSED(apArg);` |
|   427 |  2044 | `	if( !DualReady(pThis) ){` |
|     3 |  2045 | `		return DualNotReady(pCtx);` |
|     - |  2046 | `	}` |
|   425 |  2047 | `	ph7_result_bool(pCtx,DualFilled(pThis));` |
|   425 |  2048 | `	return PH7_OK;` |
|   214 |  2049 | `}` |
|   394 |  2050 | `static int vm_builtin_Dual_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  2051 | `{` |
|   197 |  2052 | `	SXUNUSED(nArg);` |
|   197 |  2053 | `	SXUNUSED(apArg);` |
|   396 |  2054 | `	return DualResultSlot(pCtx,IT_CD);` |
|     2 |  2055 | `}` |
|   274 |  2056 | `static int vm_builtin_Dual_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2057 | `{` |
|   137 |  2058 | `	SXUNUSED(nArg);` |
|   137 |  2059 | `	SXUNUSED(apArg);` |
|   275 |  2060 | `	return DualResultSlot(pCtx,IT_CK);` |
|     1 |  2061 | `}` |
|    22 |  2062 | `static int vm_builtin_IteratorIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2063 | `{` |
|    23 |  2064 | `	ph7_vm *pVm = pCtx->pVm;` |
|    23 |  2065 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2066 | `	sxi32 rc;` |
|    11 |  2067 | `	SXUNUSED(nArg);` |
|    11 |  2068 | `	SXUNUSED(apArg);` |
|    23 |  2069 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  2070 | `		return DualNotReady(pCtx);` |
|     - |  2071 | `	}` |
|    23 |  2072 | `	rc = DualRewindInner(pVm,pThis);` |
|    23 |  2073 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  2074 | `		return rc;` |
|     - |  2075 | `	}` |
|    23 |  2076 | `	return DualFetch(pVm,pThis,TRUE);` |
|    12 |  2077 | `}` |
|    22 |  2078 | `static int vm_builtin_IteratorIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2079 | `{` |
|    23 |  2080 | `	ph7_vm *pVm = pCtx->pVm;` |
|    23 |  2081 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2082 | `	sxi32 rc;` |
|    11 |  2083 | `	SXUNUSED(nArg);` |
|    11 |  2084 | `	SXUNUSED(apArg);` |
|    23 |  2085 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  2086 | `		return DualNotReady(pCtx);` |
|     - |  2087 | `	}` |
|    23 |  2088 | `	rc = DualNextInner(pVm,pThis);` |
|    23 |  2089 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  2090 | `		return rc;` |
|     - |  2091 | `	}` |
|    23 |  2092 | `	return DualFetch(pVm,pThis,TRUE);` |
|    12 |  2093 | `}` |
|     - |  2094 | `/*` |
|     - |  2095 | ` * FilterIterator. php's spl_filter_it_fetch: fetch, ask accept(), and on a refusal` |
|     - |  2096 | ` * step the INNER on directly — without counting the step, which is why a filtered` |
|     - |  2097 | ` * element does not move current.pos. accept() is called on $this, so a user` |
|     - |  2098 | ` * subclass's body is what decides.` |
|     - |  2099 | ` */` |
|   238 |  2100 | `static sxi32 DualAccept(ph7_vm *pVm,ph7_class_instance *pThis,int *pbAccept)` |
|     1 |  2101 | `{` |
|   239 |  2102 | `	ph7_class_method *pMethod = PH7_ClassExtractMethod(pThis->pClass,"accept",sizeof("accept")-1);` |
|     - |  2103 | `	ph7_value sRes;` |
|     - |  2104 | `	sxi32 rc;` |
|   239 |  2105 | `	*pbAccept = 0;` |
|   239 |  2106 | `	if( pMethod == 0 ){` |
|   ! 0 |  2107 | `		return SXRET_OK;` |
|     - |  2108 | `	}` |
|   239 |  2109 | `	PH7_MemObjInit(pVm,&sRes);` |
|   239 |  2110 | `	rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sRes,0,0);` |
|   239 |  2111 | `	if( rc == SXRET_OK ){` |
|   239 |  2112 | `		PH7_MemObjToBool(&sRes);` |
|   239 |  2113 | `		*pbAccept = sRes.x.iVal != 0;` |
|   119 |  2114 | `	}` |
|   239 |  2115 | `	PH7_MemObjRelease(&sRes);` |
|   239 |  2116 | `	return rc;` |
|   120 |  2117 | `}` |
|   242 |  2118 | `static sxi32 DualFilterFetch(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  2119 | `{` |
|   207 |  2120 | `	for(;;){` |
|   329 |  2121 | `		int bAccept = 0;` |
|   329 |  2122 | `		sxi32 rc = DualFetch(pVm,pThis,TRUE);` |
|   329 |  2123 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  2124 | `			return rc;` |
|     - |  2125 | `		}` |
|   329 |  2126 | `		if( !DualFilled(pThis) ){` |
|    91 |  2127 | `			break;` |
|     - |  2128 | `		}` |
|   239 |  2129 | `		rc = DualAccept(pVm,pThis,&bAccept);` |
|   239 |  2130 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  2131 | `			return rc;` |
|     - |  2132 | `		}` |
|   239 |  2133 | `		if( bAccept ){` |
|   153 |  2134 | `			return SXRET_OK;` |
|     - |  2135 | `		}` |
|    87 |  2136 | `		rc = DualCall(pVm,pThis,"next",sizeof("next")-1,0);` |
|    87 |  2137 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  2138 | `			return rc;` |
|     - |  2139 | `		}` |
|     1 |  2140 | `	}` |
|    91 |  2141 | `	DualFree(pVm,pThis);` |
|    91 |  2142 | `	return SXRET_OK;` |
|   122 |  2143 | `}` |
|   106 |  2144 | `static int vm_builtin_FilterIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2145 | `{` |
|   107 |  2146 | `	ph7_vm *pVm = pCtx->pVm;` |
|   107 |  2147 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2148 | `	sxi32 rc;` |
|    53 |  2149 | `	SXUNUSED(nArg);` |
|    53 |  2150 | `	SXUNUSED(apArg);` |
|   107 |  2151 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  2152 | `		return DualNotReady(pCtx);` |
|     - |  2153 | `	}` |
|   107 |  2154 | `	rc = DualRewindInner(pVm,pThis);` |
|   107 |  2155 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  2156 | `		return rc;` |
|     - |  2157 | `	}` |
|   107 |  2158 | `	return DualFilterFetch(pVm,pThis);` |
|    54 |  2159 | `}` |
|   136 |  2160 | `static int vm_builtin_FilterIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2161 | `{` |
|   137 |  2162 | `	ph7_vm *pVm = pCtx->pVm;` |
|   137 |  2163 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2164 | `	sxi32 rc;` |
|    68 |  2165 | `	SXUNUSED(nArg);` |
|    68 |  2166 | `	SXUNUSED(apArg);` |
|   137 |  2167 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  2168 | `		return DualNotReady(pCtx);` |
|     - |  2169 | `	}` |
|   137 |  2170 | `	rc = DualNextInner(pVm,pThis);` |
|   137 |  2171 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  2172 | `		return rc;` |
|     - |  2173 | `	}` |
|   137 |  2174 | `	return DualFilterFetch(pVm,pThis);` |
|    69 |  2175 | `}` |
|     - |  2176 | `/* CallbackFilterIterator::accept(): the callback sees the CACHED pair and the inner` |
|     - |  2177 | ` * iterator, and an empty cache is refused without calling it at all. */` |
|    38 |  2178 | `static int vm_builtin_CallbackFilterIterator_accept(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2179 | `{` |
|    39 |  2180 | `	ph7_vm *pVm = pCtx->pVm;` |
|    39 |  2181 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2182 | `	ph7_value *apCall[3];` |
|     - |  2183 | `	ph7_value sInner,sRes,*pCb;` |
|     - |  2184 | `	sxi32 rc;` |
|    19 |  2185 | `	SXUNUSED(nArg);` |
|    19 |  2186 | `	SXUNUSED(apArg);` |
|    39 |  2187 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|     3 |  2188 | `		return DualNotReady(pCtx);` |
|     - |  2189 | `	}` |
|    37 |  2190 | `	if( !DualFilled(pThis) ){` |
|   ! 0 |  2191 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  2192 | `		return PH7_OK;` |
|     - |  2193 | `	}` |
|    37 |  2194 | `	pCb = PH7_NativeAttr(pThis,IT_CB);` |
|    37 |  2195 | `	if( pCb == 0 ){` |
|   ! 0 |  2196 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  2197 | `		return PH7_OK;` |
|     - |  2198 | `	}` |
|    37 |  2199 | `	PH7_MemObjInit(pVm,&sInner);` |
|    37 |  2200 | `	sInner.x.pOther = PH7_NativeAttrObj(pThis,IT_IN);` |
|    37 |  2201 | `	if( sInner.x.pOther ){` |
|    37 |  2202 | `		MemObjSetType(&sInner,MEMOBJ_OBJ);` |
|    37 |  2203 | `		((ph7_class_instance *)sInner.x.pOther)->iRef++;` |
|    18 |  2204 | `	}` |
|    37 |  2205 | `	apCall[0] = PH7_NativeAttr(pThis,IT_CD);` |
|    37 |  2206 | `	apCall[1] = PH7_NativeAttr(pThis,IT_CK);` |
|    37 |  2207 | `	apCall[2] = &sInner;` |
|    37 |  2208 | `	PH7_MemObjInit(pVm,&sRes);` |
|    37 |  2209 | `	rc = PH7_VmCallUserFunction(pVm,pCb,3,apCall,&sRes);` |
|    37 |  2210 | `	PH7_MemObjRelease(&sInner);` |
|    37 |  2211 | `	if( rc == SXRET_OK ){` |
|    37 |  2212 | `		ph7_result_value(pCtx,&sRes);` |
|    18 |  2213 | `	}` |
|    37 |  2214 | `	PH7_MemObjRelease(&sRes);` |
|    37 |  2215 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    20 |  2216 | `}` |
|     - |  2217 | `/*` |
|     - |  2218 | ` * LimitIterator. The window is (offset, count) over the inner iterator's own` |
|     - |  2219 | `` * positions, and `__cp` counts them: php's valid() is "inside the window AND the`` |
|     - |  2220 | ` * cache is filled", and next() only refills while the window still has room.` |
|     - |  2221 | ` */` |
|    28 |  2222 | `static sxi32 DualLimitSeek(ph7_context *pCtx,ph7_class_instance *pThis,sxi64 iPos)` |
|     1 |  2223 | `{` |
|    29 |  2224 | `	ph7_vm *pVm = pCtx->pVm;` |
|    29 |  2225 | `	sxi64 iOff = PH7_NativeAttrInt(pThis,IT_OFF);` |
|    29 |  2226 | `	sxi64 iLim = PH7_NativeAttrInt(pThis,IT_LIM);` |
|    29 |  2227 | `	ph7_class_instance *pIn = DualDriver(pThis);` |
|     - |  2228 | `	ph7_class *pSeekCls;` |
|     - |  2229 | `	sxi32 rc;` |
|     - |  2230 | `	int bValid;` |
|    29 |  2231 | `	DualFree(pVm,pThis);` |
|    29 |  2232 | `	if( iPos < iOff ){` |
|     7 |  2233 | `		return PH7_VmThrowException(pCtx,"OutOfBoundsException",` |
|     2 |  2234 | `			"Cannot seek to %qd which is below the offset %qd",iPos,iOff);` |
|     - |  2235 | `	}` |
|    25 |  2236 | `	if( iLim != -1 && (iPos - iOff) >= iLim ){` |
|     4 |  2237 | `		return PH7_VmThrowException(pCtx,"OutOfBoundsException",` |
|     1 |  2238 | `			"Cannot seek to %qd which is behind offset %qd plus count %qd",iPos,iOff,iLim);` |
|     - |  2239 | `	}` |
|    23 |  2240 | `	pSeekCls = PH7_VmExtractClass(pVm,"SeekableIterator",sizeof("SeekableIterator")-1,FALSE,0);` |
|    22 |  2241 | `	if( iPos != PH7_NativeAttrInt(pThis,IT_CP) && pIn && pSeekCls` |
|    17 |  2242 | `	 && PH7_VmInstanceOf(pIn->pClass,pSeekCls) ){` |
|     - |  2243 | `		/* The inner knows how to jump: hand it the ABSOLUTE position and let its own` |
|     - |  2244 | `		 * refusal (ArrayIterator's "Seek position N is out of range") surface. */` |
|    17 |  2245 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pIn->pClass,"seek",sizeof("seek")-1);` |
|     - |  2246 | `		ph7_value sPos,*apArg[1];` |
|    17 |  2247 | `		PH7_MemObjInitFromInt(pVm,&sPos,iPos);` |
|    17 |  2248 | `		apArg[0] = &sPos;` |
|    17 |  2249 | `		rc = pMethod ? PH7_VmCallClassMethod(pVm,pIn,pMethod,0,1,apArg) : SXRET_OK;` |
|    17 |  2250 | `		PH7_MemObjRelease(&sPos);` |
|    17 |  2251 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  2252 | `			return rc;` |
|     - |  2253 | `		}` |
|    17 |  2254 | `		PH7_NativeSetAttrInt(pVm,pThis,IT_CP,iPos);` |
|    17 |  2255 | `		rc = DualInnerValid(pVm,pThis,&bValid);` |
|    17 |  2256 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  2257 | `			return rc;` |
|     - |  2258 | `		}` |
|    17 |  2259 | `		if( bValid ){` |
|    17 |  2260 | `			return DualFetch(pVm,pThis,FALSE);` |
|     - |  2261 | `		}` |
|   ! 0 |  2262 | `		return SXRET_OK;` |
|     - |  2263 | `	}` |
|     - |  2264 | `	/* Otherwise emulate: a backward seek is a rewind followed by next() calls. */` |
|     7 |  2265 | `	if( iPos < PH7_NativeAttrInt(pThis,IT_CP) ){` |
|   ! 0 |  2266 | `		rc = DualRewindInner(pVm,pThis);` |
|   ! 0 |  2267 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  2268 | `			return rc;` |
|     - |  2269 | `		}` |
|   ! 0 |  2270 | `	}` |
|     3 |  2271 | `	for(;;){` |
|     7 |  2272 | `		if( iPos <= PH7_NativeAttrInt(pThis,IT_CP) ){` |
|     7 |  2273 | `			break;` |
|     - |  2274 | `		}` |
|   ! 0 |  2275 | `		rc = DualInnerValid(pVm,pThis,&bValid);` |
|   ! 0 |  2276 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  2277 | `			return rc;` |
|     - |  2278 | `		}` |
|   ! 0 |  2279 | `		if( !bValid ){` |
|   ! 0 |  2280 | `			break;` |
|     - |  2281 | `		}` |
|   ! 0 |  2282 | `		rc = DualNextInner(pVm,pThis);` |
|   ! 0 |  2283 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  2284 | `			return rc;` |
|     - |  2285 | `		}` |
|   ! 0 |  2286 | `	}` |
|     7 |  2287 | `	rc = DualInnerValid(pVm,pThis,&bValid);` |
|     7 |  2288 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  2289 | `		return rc;` |
|     - |  2290 | `	}` |
|     7 |  2291 | `	if( bValid ){` |
|     7 |  2292 | `		return DualFetch(pVm,pThis,TRUE);` |
|     - |  2293 | `	}` |
|   ! 0 |  2294 | `	return SXRET_OK;` |
|    15 |  2295 | `}` |
|    46 |  2296 | `static int vm_builtin_LimitIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2297 | `{` |
|    47 |  2298 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  2299 | `	ph7_class_instance *pThis;` |
|    47 |  2300 | `	sxi64 iOff = 0,iLim = -1;` |
|     - |  2301 | `	sxi32 rc;` |
|     - |  2302 | `	/* php screens the two bounds BEFORE it remembers the iterator, so a refused` |
|     - |  2303 | `	 * LimitIterator can still be constructed again. PH7_IntArgResolve is the shared` |
|     - |  2304 | ``	 * `int` ZPP: the central signature screen does not cover a non-numeric STRING`` |
|     - |  2305 | `	 * against an int parameter, and every builtin that takes one calls this. */` |
|    47 |  2306 | `	if( nArg > 1 ){` |
|    41 |  2307 | `		rc = PH7_IntArgResolve(pCtx,apArg[1],ph7_function_name(pCtx),2,"$offset","int",&iOff);` |
|    41 |  2308 | `		if( rc != PH7_OK ){` |
|   ! 0 |  2309 | `			return rc;` |
|     - |  2310 | `		}` |
|    41 |  2311 | `		if( iOff < 0 ){` |
|     7 |  2312 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  2313 | `				"%s(): Argument #2 ($offset) must be greater than or equal to 0",` |
|     2 |  2314 | `				ph7_function_name(pCtx));` |
|     - |  2315 | `		}` |
|    18 |  2316 | `	}` |
|    43 |  2317 | `	if( nArg > 2 ){` |
|    33 |  2318 | `		rc = PH7_IntArgResolve(pCtx,apArg[2],ph7_function_name(pCtx),3,"$limit","int",&iLim);` |
|    33 |  2319 | `		if( rc != PH7_OK ){` |
|   ! 0 |  2320 | `			return rc;` |
|     - |  2321 | `		}` |
|    33 |  2322 | `		if( iLim < -1 ){` |
|     7 |  2323 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  2324 | `				"%s(): Argument #3 ($limit) must be greater than or equal to -1",` |
|     2 |  2325 | `				ph7_function_name(pCtx));` |
|     - |  2326 | `		}` |
|    14 |  2327 | `	}` |
|    39 |  2328 | `	rc = DualConstruct(pCtx,"LimitIterator",nArg,apArg);` |
|    39 |  2329 | `	if( rc != PH7_OK ){` |
|     3 |  2330 | `		return rc;` |
|     - |  2331 | `	}` |
|    37 |  2332 | `	pThis = PH7_ContextThis(pCtx);` |
|    37 |  2333 | `	if( pThis ){` |
|    37 |  2334 | `		PH7_NativeSetAttrInt(pVm,pThis,IT_OFF,iOff);` |
|    37 |  2335 | `		PH7_NativeSetAttrInt(pVm,pThis,IT_LIM,iLim);` |
|    18 |  2336 | `	}` |
|    37 |  2337 | `	return PH7_OK;` |
|    24 |  2338 | `}` |
|    16 |  2339 | `static int vm_builtin_LimitIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2340 | `{` |
|    17 |  2341 | `	ph7_vm *pVm = pCtx->pVm;` |
|    17 |  2342 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2343 | `	sxi32 rc;` |
|     8 |  2344 | `	SXUNUSED(nArg);` |
|     8 |  2345 | `	SXUNUSED(apArg);` |
|    17 |  2346 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  2347 | `		return DualNotReady(pCtx);` |
|     - |  2348 | `	}` |
|    17 |  2349 | `	rc = DualRewindInner(pVm,pThis);` |
|    17 |  2350 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  2351 | `		return rc;` |
|     - |  2352 | `	}` |
|    17 |  2353 | `	return DualLimitSeek(pCtx,pThis,PH7_NativeAttrInt(pThis,IT_OFF));` |
|     9 |  2354 | `}` |
|    40 |  2355 | `static int vm_builtin_LimitIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2356 | `{` |
|    41 |  2357 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2358 | `	sxi64 iLim;` |
|    20 |  2359 | `	SXUNUSED(nArg);` |
|    20 |  2360 | `	SXUNUSED(apArg);` |
|    41 |  2361 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  2362 | `		return DualNotReady(pCtx);` |
|     - |  2363 | `	}` |
|    41 |  2364 | `	iLim = PH7_NativeAttrInt(pThis,IT_LIM);` |
|    81 |  2365 | `	ph7_result_bool(pCtx,` |
|    20 |  2366 | `		(iLim == -1` |
|    33 |  2367 | `		 \|\| (PH7_NativeAttrInt(pThis,IT_CP) - PH7_NativeAttrInt(pThis,IT_OFF)) < iLim)` |
|    35 |  2368 | `		&& DualFilled(pThis));` |
|    41 |  2369 | `	return PH7_OK;` |
|    21 |  2370 | `}` |
|    34 |  2371 | `static int vm_builtin_LimitIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2372 | `{` |
|    35 |  2373 | `	ph7_vm *pVm = pCtx->pVm;` |
|    35 |  2374 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2375 | `	sxi64 iLim;` |
|     - |  2376 | `	sxi32 rc;` |
|    17 |  2377 | `	SXUNUSED(nArg);` |
|    17 |  2378 | `	SXUNUSED(apArg);` |
|    35 |  2379 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  2380 | `		return DualNotReady(pCtx);` |
|     - |  2381 | `	}` |
|    35 |  2382 | `	rc = DualNextInner(pVm,pThis);` |
|    35 |  2383 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  2384 | `		return rc;` |
|     - |  2385 | `	}` |
|    35 |  2386 | `	iLim = PH7_NativeAttrInt(pThis,IT_LIM);` |
|    34 |  2387 | `	if( iLim == -1` |
|    30 |  2388 | `	 \|\| (PH7_NativeAttrInt(pThis,IT_CP) - PH7_NativeAttrInt(pThis,IT_OFF)) < iLim ){` |
|    25 |  2389 | `		return DualFetch(pVm,pThis,TRUE);` |
|     - |  2390 | `	}` |
|    11 |  2391 | `	return PH7_OK;   /* past the window: the cache stays empty, so current() is null */` |
|    18 |  2392 | `}` |
|    12 |  2393 | `static int vm_builtin_LimitIterator_seek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2394 | `{` |
|    13 |  2395 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    13 |  2396 | `	sxi64 iPos = 0;` |
|     - |  2397 | `	sxi32 rc;` |
|    13 |  2398 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  2399 | `		return DualNotReady(pCtx);` |
|     - |  2400 | `	}` |
|    13 |  2401 | `	if( nArg > 0 ){` |
|    13 |  2402 | `		rc = PH7_IntArgResolve(pCtx,apArg[0],ph7_function_name(pCtx),1,"$offset","int",&iPos);` |
|    13 |  2403 | `		if( rc != PH7_OK ){` |
|   ! 0 |  2404 | `			return rc;` |
|     - |  2405 | `		}` |
|     6 |  2406 | `	}` |
|    13 |  2407 | `	rc = DualLimitSeek(pCtx,pThis,iPos);` |
|    13 |  2408 | `	if( rc != PH7_OK ){` |
|     7 |  2409 | `		return rc;` |
|     - |  2410 | `	}` |
|     7 |  2411 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(pThis,IT_CP));` |
|     7 |  2412 | `	return PH7_OK;` |
|     7 |  2413 | `}` |
|    18 |  2414 | `static int vm_builtin_LimitIterator_getPosition(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2415 | `{` |
|    19 |  2416 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     9 |  2417 | `	SXUNUSED(nArg);` |
|     9 |  2418 | `	SXUNUSED(apArg);` |
|    19 |  2419 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  2420 | `		return DualNotReady(pCtx);` |
|     - |  2421 | `	}` |
|    19 |  2422 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(pThis,IT_CP));` |
|    19 |  2423 | `	return PH7_OK;` |
|    10 |  2424 | `}` |
|     - |  2425 | `/* InfiniteIterator::next(): step, and on exhaustion rewind and step into the head` |
|     - |  2426 | ` * again. Both refills are php's check_more=0 form — the validity was just tested. */` |
|    16 |  2427 | `static int vm_builtin_InfiniteIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2428 | `{` |
|    17 |  2429 | `	ph7_vm *pVm = pCtx->pVm;` |
|    17 |  2430 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2431 | `	sxi32 rc;` |
|     - |  2432 | `	int bValid;` |
|     8 |  2433 | `	SXUNUSED(nArg);` |
|     8 |  2434 | `	SXUNUSED(apArg);` |
|    17 |  2435 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  2436 | `		return DualNotReady(pCtx);` |
|     - |  2437 | `	}` |
|    17 |  2438 | `	rc = DualNextInner(pVm,pThis);` |
|    17 |  2439 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  2440 | `		return rc;` |
|     - |  2441 | `	}` |
|    17 |  2442 | `	rc = DualInnerValid(pVm,pThis,&bValid);` |
|    17 |  2443 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  2444 | `		return rc;` |
|     - |  2445 | `	}` |
|    17 |  2446 | `	if( !bValid ){` |
|     9 |  2447 | `		rc = DualRewindInner(pVm,pThis);` |
|     9 |  2448 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  2449 | `			return rc;` |
|     - |  2450 | `		}` |
|     9 |  2451 | `		rc = DualInnerValid(pVm,pThis,&bValid);` |
|     9 |  2452 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  2453 | `			return rc;` |
|     - |  2454 | `		}` |
|     4 |  2455 | `	}` |
|    17 |  2456 | `	if( bValid ){` |
|    17 |  2457 | `		return DualFetch(pVm,pThis,FALSE);` |
|     - |  2458 | `	}` |
|   ! 0 |  2459 | `	return PH7_OK;` |
|     9 |  2460 | `}` |
|     - |  2461 | `/*` |
|     - |  2462 | ` * NoRewindIterator. Its rewind() does nothing at all — and because the four` |
|     - |  2463 | ` * accessors read the INNER live rather than the cache, an instance is usable` |
|     - |  2464 | ` * without ever being rewound, which is the entire point of the class.` |
|     - |  2465 | ` */` |
|     4 |  2466 | `static int vm_builtin_NoRewindIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2467 | `{` |
|     2 |  2468 | `	SXUNUSED(nArg);` |
|     2 |  2469 | `	SXUNUSED(apArg);` |
|     5 |  2470 | `	if( PH7_ContextThis(pCtx) == 0 \|\| DualDriver(PH7_ContextThis(pCtx)) == 0 ){` |
|   ! 0 |  2471 | `		return DualNotReady(pCtx);` |
|     - |  2472 | `	}` |
|     5 |  2473 | `	return PH7_OK;` |
|     3 |  2474 | `}` |
|    14 |  2475 | `static int vm_builtin_NoRewindIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2476 | `{` |
|    15 |  2477 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2478 | `	sxi32 rc;` |
|     - |  2479 | `	int bValid;` |
|     7 |  2480 | `	SXUNUSED(nArg);` |
|     7 |  2481 | `	SXUNUSED(apArg);` |
|    15 |  2482 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  2483 | `		return DualNotReady(pCtx);` |
|     - |  2484 | `	}` |
|    15 |  2485 | `	rc = DualInnerValid(pCtx->pVm,pThis,&bValid);` |
|    15 |  2486 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  2487 | `		return rc;` |
|     - |  2488 | `	}` |
|    15 |  2489 | `	ph7_result_bool(pCtx,bValid);` |
|    15 |  2490 | `	return PH7_OK;` |
|     8 |  2491 | `}` |
|    26 |  2492 | `static int DualForwardLive(ph7_context *pCtx,const char *zName,sxu32 nLen,int bResult)` |
|     1 |  2493 | `{` |
|    27 |  2494 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2495 | `	ph7_value sVal;` |
|     - |  2496 | `	sxi32 rc;` |
|    27 |  2497 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  2498 | `		return DualNotReady(pCtx);` |
|     - |  2499 | `	}` |
|    27 |  2500 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|    27 |  2501 | `	rc = DualCall(pCtx->pVm,pThis,zName,nLen,bResult ? &sVal : 0);` |
|    27 |  2502 | `	if( rc == SXRET_OK && bResult ){` |
|    17 |  2503 | `		ph7_result_value(pCtx,&sVal);` |
|     8 |  2504 | `	}` |
|    27 |  2505 | `	PH7_MemObjRelease(&sVal);` |
|    27 |  2506 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    14 |  2507 | `}` |
|    10 |  2508 | `static int vm_builtin_NoRewindIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2509 | `{` |
|     5 |  2510 | `	SXUNUSED(nArg);` |
|     5 |  2511 | `	SXUNUSED(apArg);` |
|    11 |  2512 | `	return DualForwardLive(pCtx,"current",sizeof("current")-1,TRUE);` |
|     1 |  2513 | `}` |
|     6 |  2514 | `static int vm_builtin_NoRewindIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2515 | `{` |
|     3 |  2516 | `	SXUNUSED(nArg);` |
|     3 |  2517 | `	SXUNUSED(apArg);` |
|     7 |  2518 | `	return DualForwardLive(pCtx,"key",sizeof("key")-1,TRUE);` |
|     1 |  2519 | `}` |
|    10 |  2520 | `static int vm_builtin_NoRewindIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2521 | `{` |
|     5 |  2522 | `	SXUNUSED(nArg);` |
|     5 |  2523 | `	SXUNUSED(apArg);` |
|    11 |  2524 | `	return DualForwardLive(pCtx,"next",sizeof("next")-1,FALSE);` |
|     1 |  2525 | `}` |
|     - |  2526 | `/* EmptyIterator: valid() is false forever, and asking for a value or a key is a` |
|     - |  2527 | ` * BadMethodCallException rather than a null. */` |
|     4 |  2528 | `static int vm_builtin_EmptyIterator_nop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2529 | `{` |
|     2 |  2530 | `	SXUNUSED(nArg);` |
|     2 |  2531 | `	SXUNUSED(apArg);` |
|     5 |  2532 | `	ph7_result_null(pCtx);` |
|     5 |  2533 | `	return PH7_OK;` |
|     1 |  2534 | `}` |
|     8 |  2535 | `static int vm_builtin_EmptyIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2536 | `{` |
|     4 |  2537 | `	SXUNUSED(nArg);` |
|     4 |  2538 | `	SXUNUSED(apArg);` |
|     9 |  2539 | `	ph7_result_bool(pCtx,0);` |
|     9 |  2540 | `	return PH7_OK;` |
|     1 |  2541 | `}` |
|     4 |  2542 | `static int vm_builtin_EmptyIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2543 | `{` |
|     2 |  2544 | `	SXUNUSED(nArg);` |
|     2 |  2545 | `	SXUNUSED(apArg);` |
|     5 |  2546 | `	return PH7_VmThrowException(pCtx,"BadMethodCallException",` |
|     - |  2547 | `		"Accessing the value of an EmptyIterator");` |
|     1 |  2548 | `}` |
|     4 |  2549 | `static int vm_builtin_EmptyIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2550 | `{` |
|     2 |  2551 | `	SXUNUSED(nArg);` |
|     2 |  2552 | `	SXUNUSED(apArg);` |
|     5 |  2553 | `	return PH7_VmThrowException(pCtx,"BadMethodCallException",` |
|     - |  2554 | `		"Accessing the key of an EmptyIterator");` |
|     1 |  2555 | `}` |
|     - |  2556 | `/*` |
|     - |  2557 | ` * ---------------------------------------------------------------------------` |
|     - |  2558 | ` * RegexIterator: a FilterIterator whose accept() runs a regex over the CACHE.` |
|     - |  2559 | ` *` |
|     - |  2560 | ` * Everything that matters here follows from the cache the decorators already` |
|     - |  2561 | `` * keep. php's accept() reads `current.data` (or `current.key` under USE_KEY) and`` |
|     - |  2562 | ` * -- in every mode but MATCH -- WRITES THE RESULT BACK INTO THAT SAME SLOT, which` |
|     - |  2563 | ` * is why the class declares no current() of its own: the inherited one already` |
|     - |  2564 | `` * answers the transformed value. The PHP chunk kept a private `$__cur` and`` |
|     - |  2565 | ` * overrode current(), and that is where its two wrong answers came from: a` |
|     - |  2566 | ` * REPLACE under USE_KEY must replace into the KEY (php leaves current() alone),` |
|     - |  2567 | ` * and an ARRAY current() is refused outright rather than matched as the string` |
|     - |  2568 | ` * "Array".` |
|     - |  2569 | ` */` |
|     - |  2570 | `#define IT_RE  "__re"   /* php's u.regex.regex: the pattern, as given */` |
|     - |  2571 | `#define IT_RM  "__rm"   /* php's u.regex.mode */` |
|     - |  2572 | `#define IT_RF  "__rf"   /* php's u.regex.flags (USE_KEY / INVERT_MATCH) */` |
|     - |  2573 | `#define IT_RP  "__rp"   /* php's u.regex.preg_flags */` |
|     - |  2574 | `#define REGIT_USE_KEY  1` |
|     - |  2575 | `#define REGIT_INVERTED 2` |
|     - |  2576 | `/* php's ValueError for a mode outside the five. The constructor and setMode()` |
|     - |  2577 | ` * word it identically and differ only in the argument they name. */` |
|     8 |  2578 | `static int RegitBadMode(ph7_context *pCtx,const char *zWhere)` |
|     1 |  2579 | `{` |
|    13 |  2580 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  2581 | `		"%s must be RegexIterator::MATCH, RegexIterator::GET_MATCH, "` |
|     - |  2582 | `		"RegexIterator::ALL_MATCHES, RegexIterator::SPLIT, or RegexIterator::REPLACE",` |
|     4 |  2583 | `		zWhere);` |
|     1 |  2584 | `}` |
|     - |  2585 | `/*` |
|     - |  2586 | ` * The constructor both regex iterators run. Every diagnostic it raises names the` |
|     - |  2587 | ` * class that was CONSTRUCTED (php's are its own method's scope), so the owner is a` |
|     - |  2588 | ` * parameter rather than a literal -- and the ValueError still names the mode` |
|     - |  2589 | ` * constants on RegexIterator, which is where php declares them.` |
|     - |  2590 | ` */` |
|    88 |  2591 | `static int RegitConstruct(ph7_context *pCtx,const char *zOwner,int nArg,ph7_value **apArg)` |
|     1 |  2592 | `{` |
|    89 |  2593 | `	ph7_vm *pVm = pCtx->pVm;` |
|    89 |  2594 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2595 | `	const char *zPat;` |
|     - |  2596 | `	char zWhere[128];` |
|     - |  2597 | `	int nPat;` |
|    89 |  2598 | `	sxi64 iMode = PH7_REGIT_MATCH;` |
|     - |  2599 | `	char zErr[288];` |
|     - |  2600 | `	sxi32 rc;` |
|    89 |  2601 | `	if( pThis == 0 ){` |
|   ! 0 |  2602 | `		return PH7_OK;` |
|     - |  2603 | `	}` |
|    89 |  2604 | `	if( PH7_NativeAttrObj(pThis,IT_IN) != 0 ){` |
|     - |  2605 | `		/* php makes the "already built" refusal before it reads any argument, so` |
|     - |  2606 | `		 * hand this straight to the shared constructor, which words it. */` |
|   ! 0 |  2607 | `		return DualConstruct(pCtx,zOwner,nArg,apArg);` |
|     - |  2608 | `	}` |
|    89 |  2609 | `	if( nArg < 2 ){` |
|   ! 0 |  2610 | `		return PH7_OK;   /* the arity screen already refused */` |
|     - |  2611 | `	}` |
|    89 |  2612 | `	if( nArg > 2 ){` |
|    49 |  2613 | `		iMode = ph7_value_to_int(apArg[2]);` |
|    24 |  2614 | `	}` |
|    89 |  2615 | `	if( iMode < PH7_REGIT_MATCH \|\| iMode > PH7_REGIT_REPLACE ){` |
|     5 |  2616 | `		SyBufferFormat(zWhere,sizeof(zWhere),"%s::__construct(): Argument #3 ($mode)",zOwner);` |
|     5 |  2617 | `		return RegitBadMode(pCtx,zWhere);` |
|     - |  2618 | `	}` |
|     - |  2619 | `	/* php compiles the pattern HERE and promotes pcre's warning to an` |
|     - |  2620 | ``	 * InvalidArgumentException, so a bad pattern is refused by `new` rather than`` |
|     - |  2621 | `	 * warning once per element from accept(). */` |
|    85 |  2622 | `	zPat = ph7_value_to_string(apArg[1],&nPat);` |
|    85 |  2623 | `	if( !PH7_PcrePatternCheck(pVm,zPat,nPat,zErr,sizeof(zErr)) ){` |
|     7 |  2624 | `		return PH7_VmThrowException(pCtx,"InvalidArgumentException",` |
|     2 |  2625 | `			"%s::__construct(): %s",zOwner,zErr);` |
|     - |  2626 | `	}` |
|    81 |  2627 | `	rc = DualConstruct(pCtx,zOwner,nArg,apArg);` |
|    81 |  2628 | `	if( rc != PH7_OK ){` |
|   ! 0 |  2629 | `		return rc;` |
|     - |  2630 | `	}` |
|    81 |  2631 | `	PH7_NativeSetAttrStr(pVm,pThis,IT_RE,zPat,(sxu32)nPat);` |
|    81 |  2632 | `	PH7_NativeSetAttrInt(pVm,pThis,IT_RM,iMode);` |
|    81 |  2633 | `	PH7_NativeSetAttrInt(pVm,pThis,IT_RF,nArg > 3 ? ph7_value_to_int(apArg[3]) : 0);` |
|    81 |  2634 | `	PH7_NativeSetAttrInt(pVm,pThis,IT_RP,nArg > 4 ? ph7_value_to_int(apArg[4]) : 0);` |
|    81 |  2635 | `	return PH7_OK;` |
|    45 |  2636 | `}` |
|    62 |  2637 | `static int vm_builtin_RegexIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2638 | `{` |
|    63 |  2639 | `	return RegitConstruct(pCtx,"RegexIterator",nArg,apArg);` |
|     1 |  2640 | `}` |
|    98 |  2641 | `static int vm_builtin_RegexIterator_accept(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2642 | `{` |
|    99 |  2643 | `	ph7_vm *pVm = pCtx->pVm;` |
|    99 |  2644 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2645 | `	ph7_value sSubject,sPattern,sRepl,sOut,*pSlot;` |
|    99 |  2646 | `	int iMode,iFlags,bUseKey,bOk = 0;` |
|     - |  2647 | `	sxi32 rc;` |
|    49 |  2648 | `	SXUNUSED(nArg);` |
|    49 |  2649 | `	SXUNUSED(apArg);` |
|    99 |  2650 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  2651 | `		return DualNotReady(pCtx);` |
|     - |  2652 | `	}` |
|    99 |  2653 | `	if( !DualFilled(pThis) ){` |
|     - |  2654 | `		/* Nothing has been fetched: php answers false without touching the regex. */` |
|     3 |  2655 | `		ph7_result_bool(pCtx,0);` |
|     3 |  2656 | `		return PH7_OK;` |
|     - |  2657 | `	}` |
|    97 |  2658 | `	iMode = (int)PH7_NativeAttrInt(pThis,IT_RM);` |
|    97 |  2659 | `	iFlags = (int)PH7_NativeAttrInt(pThis,IT_RF);` |
|    97 |  2660 | `	bUseKey = (iFlags & REGIT_USE_KEY) != 0;` |
|    97 |  2661 | `	pSlot = PH7_NativeAttr(pThis,bUseKey ? IT_CK : IT_CD);` |
|    97 |  2662 | `	if( pSlot == 0 ){` |
|   ! 0 |  2663 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  2664 | `		return PH7_OK;` |
|     - |  2665 | `	}` |
|    97 |  2666 | `	if( !bUseKey && (pSlot->iFlags & MEMOBJ_HASHMAP) ){` |
|     - |  2667 | ``		/* php's `Z_TYPE(current.data) == IS_ARRAY -> RETURN_FALSE`, ahead of every`` |
|     - |  2668 | `		 * mode. The chunk's (string)$subject matched the word "Array" instead. */` |
|     5 |  2669 | `		ph7_result_bool(pCtx,0);` |
|     5 |  2670 | `		return PH7_OK;` |
|     - |  2671 | `	}` |
|     - |  2672 | `	/* Take the subject as a VALUE: the slot pointer does not survive a call into` |
|     - |  2673 | `	 * user code, and an object subject reaches __toString() below. */` |
|    93 |  2674 | `	PH7_MemObjInit(pVm,&sSubject);` |
|    93 |  2675 | `	PH7_MemObjStore(pSlot,&sSubject);` |
|    93 |  2676 | `	rc = PH7_MemObjToStringUV(&sSubject);` |
|    93 |  2677 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  2678 | `		PH7_MemObjRelease(&sSubject);` |
|   ! 0 |  2679 | `		return rc;` |
|     - |  2680 | `	}` |
|    93 |  2681 | `	PH7_MemObjInit(pVm,&sPattern);` |
|    93 |  2682 | `	PH7_MemObjInit(pVm,&sRepl);` |
|    93 |  2683 | `	PH7_MemObjInit(pVm,&sOut);` |
|     - |  2684 | `	{` |
|    93 |  2685 | `		ph7_value *pRe = PH7_NativeAttr(pThis,IT_RE);` |
|    93 |  2686 | `		if( pRe ){` |
|    93 |  2687 | `			PH7_MemObjStore(pRe,&sPattern);` |
|    46 |  2688 | `		}` |
|     - |  2689 | `	}` |
|    93 |  2690 | `	if( iMode == PH7_REGIT_REPLACE ){` |
|     - |  2691 | `		/* php reads the public $replacement property, whose declared ?string makes` |
|     - |  2692 | `		 * the read total: a null answers the empty string. */` |
|    17 |  2693 | `		ph7_value *pRepl = PH7_NativeAttr(pThis,"replacement");` |
|    17 |  2694 | `		if( pRepl ){` |
|    17 |  2695 | `			PH7_MemObjStore(pRepl,&sRepl);` |
|     8 |  2696 | `		}` |
|    17 |  2697 | `		PH7_MemObjToString(&sRepl);` |
|     8 |  2698 | `	}` |
|   139 |  2699 | `	rc = PH7_PcreRegitApply(pCtx,iMode,&sPattern,&sSubject,` |
|    92 |  2700 | `		(int)PH7_NativeAttrInt(pThis,IT_RP),&sRepl,&sOut,&bOk);` |
|    93 |  2701 | `	if( rc == PH7_OK && iMode != PH7_REGIT_MATCH ){` |
|     - |  2702 | `		/* php writes the transformed value over the cached pair -- into the KEY when` |
|     - |  2703 | `		 * a REPLACE is keyed, into current() otherwise -- so the inherited current()` |
|     - |  2704 | `		 * and key() present it. */` |
|    47 |  2705 | `		DualSetSlot(pVm,pThis,(iMode == PH7_REGIT_REPLACE && bUseKey) ? IT_CK : IT_CD,&sOut);` |
|    23 |  2706 | `	}` |
|    93 |  2707 | `	PH7_MemObjRelease(&sSubject);` |
|    93 |  2708 | `	PH7_MemObjRelease(&sPattern);` |
|    93 |  2709 | `	PH7_MemObjRelease(&sRepl);` |
|    93 |  2710 | `	PH7_MemObjRelease(&sOut);` |
|    93 |  2711 | `	if( rc != PH7_OK ){` |
|   ! 0 |  2712 | `		return rc;` |
|     - |  2713 | `	}` |
|    93 |  2714 | `	ph7_result_bool(pCtx,(iFlags & REGIT_INVERTED) ? !bOk : bOk);` |
|    93 |  2715 | `	return PH7_OK;` |
|    50 |  2716 | `}` |
|     6 |  2717 | `static int vm_builtin_RegexIterator_getRegex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2718 | `{` |
|     7 |  2719 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2720 | `	ph7_value *pRe;` |
|     3 |  2721 | `	SXUNUSED(nArg);` |
|     3 |  2722 | `	SXUNUSED(apArg);` |
|     7 |  2723 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  2724 | `		return DualNotReady(pCtx);` |
|     - |  2725 | `	}` |
|     7 |  2726 | `	pRe = PH7_NativeAttr(pThis,IT_RE);` |
|     7 |  2727 | `	if( pRe ){` |
|     7 |  2728 | `		ph7_result_value(pCtx,pRe);` |
|     3 |  2729 | `	}` |
|     7 |  2730 | `	return PH7_OK;` |
|     4 |  2731 | `}` |
|     - |  2732 | `/* The three getters and the three setters are one pair per slot; only setMode()` |
|     - |  2733 | ` * screens its value, which is php's own asymmetry (setFlags/setPregFlags take` |
|     - |  2734 | ` * any integer). */` |
|    24 |  2735 | `static int RegitGet(ph7_context *pCtx,const char *zSlot)` |
|     1 |  2736 | `{` |
|    25 |  2737 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    25 |  2738 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  2739 | `		return DualNotReady(pCtx);` |
|     - |  2740 | `	}` |
|    25 |  2741 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(pThis,zSlot));` |
|    25 |  2742 | `	return PH7_OK;` |
|    13 |  2743 | `}` |
|     8 |  2744 | `static int RegitSet(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zSlot)` |
|     1 |  2745 | `{` |
|     9 |  2746 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     9 |  2747 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|   ! 0 |  2748 | `		return DualNotReady(pCtx);` |
|     - |  2749 | `	}` |
|     9 |  2750 | `	if( nArg > 0 ){` |
|     9 |  2751 | `		PH7_NativeSetAttrInt(pCtx->pVm,pThis,zSlot,ph7_value_to_int64(apArg[0]));` |
|     4 |  2752 | `	}` |
|     9 |  2753 | `	return PH7_OK;` |
|     5 |  2754 | `}` |
|     8 |  2755 | `static int vm_builtin_RegexIterator_getMode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2756 | `{` |
|     4 |  2757 | `	SXUNUSED(nArg);` |
|     4 |  2758 | `	SXUNUSED(apArg);` |
|     9 |  2759 | `	return RegitGet(pCtx,IT_RM);` |
|     1 |  2760 | `}` |
|     8 |  2761 | `static int vm_builtin_RegexIterator_setMode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2762 | `{` |
|     9 |  2763 | `	if( nArg > 0 ){` |
|     9 |  2764 | `		sxi64 iMode = ph7_value_to_int64(apArg[0]);` |
|     9 |  2765 | `		if( iMode < PH7_REGIT_MATCH \|\| iMode > PH7_REGIT_REPLACE ){` |
|     - |  2766 | `			/* php screens the VALUE before it even fetches the object. */` |
|     5 |  2767 | `			return RegitBadMode(pCtx,"RegexIterator::setMode(): Argument #1 ($mode)");` |
|     - |  2768 | `		}` |
|     2 |  2769 | `	}` |
|     5 |  2770 | `	return RegitSet(pCtx,nArg,apArg,IT_RM);` |
|     5 |  2771 | `}` |
|     8 |  2772 | `static int vm_builtin_RegexIterator_getFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2773 | `{` |
|     4 |  2774 | `	SXUNUSED(nArg);` |
|     4 |  2775 | `	SXUNUSED(apArg);` |
|     9 |  2776 | `	return RegitGet(pCtx,IT_RF);` |
|     1 |  2777 | `}` |
|     2 |  2778 | `static int vm_builtin_RegexIterator_setFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2779 | `{` |
|     3 |  2780 | `	return RegitSet(pCtx,nArg,apArg,IT_RF);` |
|     1 |  2781 | `}` |
|     8 |  2782 | `static int vm_builtin_RegexIterator_getPregFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2783 | `{` |
|     4 |  2784 | `	SXUNUSED(nArg);` |
|     4 |  2785 | `	SXUNUSED(apArg);` |
|     9 |  2786 | `	return RegitGet(pCtx,IT_RP);` |
|     1 |  2787 | `}` |
|     2 |  2788 | `static int vm_builtin_RegexIterator_setPregFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2789 | `{` |
|     3 |  2790 | `	return RegitSet(pCtx,nArg,apArg,IT_RP);` |
|     1 |  2791 | `}` |
|     - |  2792 | `/*` |
|     - |  2793 | ` * ---------------------------------------------------------------------------` |
|     - |  2794 | ` * AppendIterator: an IteratorIterator whose inner iterator is whatever entry a` |
|     - |  2795 | ` * real ArrayIterator is currently pointing at.` |
|     - |  2796 | ` *` |
|     - |  2797 | `` * php keeps the appended iterators in an actual `ArrayIterator` INSTANCE`` |
|     - |  2798 | `` * (`u.append.zarrayit`, the object getArrayIterator() hands out) and walks it with`` |
|     - |  2799 | `` * a cursor over the SAME storage (`u.append.iterator`). Both halves are`` |
|     - |  2800 | ` * php-visible and the chunk had neither: it kept a private PHP array and answered` |
|     - |  2801 | ` * getArrayIterator() with a fresh ArrayIterator over a COPY, so appending through` |
|     - |  2802 | `` * the returned object iterated nothing and `$ai->rewind()` did not restart the`` |
|     - |  2803 | `` * walk. The list cursor here is that one ArrayIterator's own `pCur`, driven`` |
|     - |  2804 | ` * directly the way php drives its iterator funcs -- not through the class's` |
|     - |  2805 | ` * methods, which php does not call either.` |
|     - |  2806 | ` */` |
|   438 |  2807 | `static ph7_class_instance * ApList(ph7_class_instance *pThis)` |
|     1 |  2808 | `{` |
|   439 |  2809 | `	return pThis ? PH7_NativeAttrObj(pThis,AP_LIST) : 0;` |
|     1 |  2810 | `}` |
|   352 |  2811 | `static ph7_hashmap * ApMap(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  2812 | `{` |
|   353 |  2813 | `	return SplStore(pVm,ApList(pThis));` |
|     1 |  2814 | `}` |
|     - |  2815 | `/* The iterator the list cursor points at, or 0 past the end. */` |
|   118 |  2816 | `static ph7_class_instance * ApCurrent(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  2817 | `{` |
|   119 |  2818 | `	ph7_hashmap *pMap = ApMap(pVm,pThis);` |
|   119 |  2819 | `	ph7_value *pVal = (pMap && pMap->pCur) ? HashmapExtractNodeValue(pMap->pCur) : 0;` |
|   119 |  2820 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    31 |  2821 | `		return 0;` |
|     - |  2822 | `	}` |
|    89 |  2823 | `	return (ph7_class_instance *)pVal->x.pOther;` |
|    60 |  2824 | `}` |
|    28 |  2825 | `static void ApListRewind(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  2826 | `{` |
|    29 |  2827 | `	ph7_hashmap *pMap = ApMap(pVm,pThis);` |
|    29 |  2828 | `	if( pMap ){` |
|    29 |  2829 | `		pMap->pCur = pMap->pFirst;` |
|    14 |  2830 | `	}` |
|    29 |  2831 | `}` |
|    50 |  2832 | `static void ApListNext(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  2833 | `{` |
|    51 |  2834 | `	ph7_hashmap *pMap = ApMap(pVm,pThis);` |
|    51 |  2835 | `	if( pMap && pMap->pCur ){` |
|    51 |  2836 | `		pMap->pCur = pMap->pCur->pPrev;   /* insertion order: pFirst, then the pPrev chain */` |
|    25 |  2837 | `	}` |
|    51 |  2838 | `}` |
|     - |  2839 | `/*` |
|     - |  2840 | ` * php's spl_append_it_next_iterator: drop the cache and the current inner, then` |
|     - |  2841 | ` * adopt whatever the list cursor points at (rewound). *pbOk is php's SUCCESS --` |
|     - |  2842 | ` * false means the list is exhausted and this iterator has nothing left.` |
|     - |  2843 | ` */` |
|   118 |  2844 | `static sxi32 ApAdoptCurrent(ph7_vm *pVm,ph7_class_instance *pThis,int *pbOk)` |
|     1 |  2845 | `{` |
|     - |  2846 | `	ph7_class_instance *pIt;` |
|   119 |  2847 | `	*pbOk = 0;` |
|   119 |  2848 | `	DualFree(pVm,pThis);` |
|   119 |  2849 | `	PH7_NativeSetAttrObj(pVm,pThis,IT_IN,0);` |
|   119 |  2850 | `	PH7_NativeSetAttrObj(pVm,pThis,IT_IT,0);` |
|   119 |  2851 | `	pIt = ApCurrent(pVm,pThis);` |
|   119 |  2852 | `	if( pIt == 0 ){` |
|    31 |  2853 | `		return SXRET_OK;` |
|     - |  2854 | `	}` |
|    89 |  2855 | `	PH7_NativeSetAttrObj(pVm,pThis,IT_IN,pIt);` |
|    89 |  2856 | `	PH7_NativeSetAttrObj(pVm,pThis,IT_IT,pIt);` |
|    89 |  2857 | `	*pbOk = 1;` |
|    89 |  2858 | `	return DualRewindInner(pVm,pThis);` |
|    60 |  2859 | `}` |
|     - |  2860 | `/*` |
|     - |  2861 | ` * php's spl_append_it_fetch: step over every exhausted inner iterator, then fill` |
|     - |  2862 | ` * the cache without re-asking valid() (php's check_more = 0 -- the loop above just` |
|     - |  2863 | ` * established it).` |
|     - |  2864 | ` */` |
|   114 |  2865 | `static sxi32 ApFetch(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  2866 | `{` |
|    77 |  2867 | `	for(;;){` |
|   135 |  2868 | `		int bValid = 0, bOk = 0;` |
|   135 |  2869 | `		sxi32 rc = DualInnerValid(pVm,pThis,&bValid);` |
|   135 |  2870 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  2871 | `			return rc;` |
|     - |  2872 | `		}` |
|   135 |  2873 | `		if( bValid ){` |
|    85 |  2874 | `			break;` |
|     - |  2875 | `		}` |
|    51 |  2876 | `		ApListNext(pVm,pThis);` |
|    51 |  2877 | `		rc = ApAdoptCurrent(pVm,pThis,&bOk);` |
|    51 |  2878 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  2879 | `			return rc;` |
|     - |  2880 | `		}` |
|    51 |  2881 | `		if( !bOk ){` |
|    31 |  2882 | `			return SXRET_OK;   /* nothing left: the cache stays empty and valid() is false */` |
|     - |  2883 | `		}` |
|     1 |  2884 | `	}` |
|    85 |  2885 | `	return DualFetch(pVm,pThis,FALSE);` |
|    58 |  2886 | `}` |
|    48 |  2887 | `static int vm_builtin_AppendIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2888 | `{` |
|    49 |  2889 | `	ph7_vm *pVm = pCtx->pVm;` |
|    49 |  2890 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2891 | `	ph7_class_instance *pList;` |
|     - |  2892 | `	ph7_class *pClass;` |
|     - |  2893 | `	ph7_class_method *pCons;` |
|    24 |  2894 | `	SXUNUSED(nArg);` |
|    24 |  2895 | `	SXUNUSED(apArg);` |
|    49 |  2896 | `	if( pThis == 0 ){` |
|   ! 0 |  2897 | `		return PH7_OK;` |
|     - |  2898 | `	}` |
|    49 |  2899 | `	if( ApList(pThis) != 0 ){` |
|     - |  2900 | `		/* php's "already built" refusal, worded from the DECLARING class as everywhere` |
|     - |  2901 | `		 * else in the family. */` |
|   ! 0 |  2902 | `		return PH7_VmThrowException(pCtx,"BadMethodCallException",` |
|     - |  2903 | `			"AppendIterator::getIterator() must be called exactly once per instance");` |
|     - |  2904 | `	}` |
|    49 |  2905 | `	pClass = PH7_VmExtractClass(pVm,"ArrayIterator",sizeof("ArrayIterator")-1,FALSE,0);` |
|    49 |  2906 | `	if( pClass == 0 ){` |
|   ! 0 |  2907 | `		return PH7_OK;` |
|     - |  2908 | `	}` |
|    49 |  2909 | `	pList = PH7_NewClassInstance(pVm,pClass);` |
|    49 |  2910 | `	if( pList == 0 ){` |
|   ! 0 |  2911 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  2912 | `	}` |
|    49 |  2913 | `	pList->iRef++;` |
|    49 |  2914 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|    49 |  2915 | `	if( pCons ){` |
|    49 |  2916 | `		PH7_VmCallClassMethod(pVm,pList,pCons,0,0,0);` |
|    24 |  2917 | `	}` |
|    49 |  2918 | `	PH7_NativeSetAttrObj(pVm,pThis,AP_LIST,pList);   /* the slot takes its own reference */` |
|    49 |  2919 | `	PH7_ClassInstanceUnref(pList);` |
|    49 |  2920 | `	return PH7_OK;` |
|    25 |  2921 | `}` |
|     - |  2922 | `/*` |
|     - |  2923 | ` * append(). php's own sequence, and every branch of it is observable:` |
|     - |  2924 | ` *   - a list cursor sitting on a LIVE entry whose cache is empty means the walk has` |
|     - |  2925 | ` *     consumed that entry, so the new iterator goes in behind it and the cursor steps` |
|     - |  2926 | ` *     over;` |
|     - |  2927 | ` *   - if nothing is being iterated yet (or the cache is empty), the cursor is walked` |
|     - |  2928 | ` *     forward until it reaches the iterator just appended, and the fetch resumes there.` |
|     - |  2929 | ` * That second half is what makes an AppendIterator RESUME after exhaustion, and it` |
|     - |  2930 | ` * relies on ArrayIterator::append() reviving a cursor that ran off the end (see` |
|     - |  2931 | ` * SplStoreInsert).` |
|     - |  2932 | ` */` |
|    58 |  2933 | `static int vm_builtin_AppendIterator_append(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2934 | `{` |
|    59 |  2935 | `	ph7_vm *pVm = pCtx->pVm;` |
|    59 |  2936 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2937 | `	ph7_class_instance *pIt;` |
|     - |  2938 | `	ph7_hashmap *pMap;` |
|    59 |  2939 | `	int bListValid,bInnerValid = 0,nGuard;` |
|     - |  2940 | `	sxi32 rc;` |
|    59 |  2941 | `	if( !DualReady(pThis) ){` |
|   ! 0 |  2942 | `		return DualNotReady(pCtx);` |
|     - |  2943 | `	}` |
|    59 |  2944 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 |  2945 | `		return PH7_OK;   /* the shared ZPP screen already refused a non-Iterator */` |
|     - |  2946 | `	}` |
|    59 |  2947 | `	pIt = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    59 |  2948 | `	pMap = ApMap(pVm,pThis);` |
|    59 |  2949 | `	bListValid = pMap && pMap->pCur;` |
|     - |  2950 | `	/* php's spl_dual_it_valid, both times it appears below: the INNER iterator's` |
|     - |  2951 | `	 * valid() (false when there is no inner at all), NOT the cache. */` |
|    59 |  2952 | `	rc = DualInnerValid(pVm,pThis,&bInnerValid);` |
|    59 |  2953 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  2954 | `		return rc;` |
|     - |  2955 | `	}` |
|    59 |  2956 | `	pMap = ApMap(pVm,pThis);   /* that call ran user code: re-resolve */` |
|    59 |  2957 | `	if( pMap ){` |
|    59 |  2958 | `		SplStoreInsert(pMap,0,apArg[0]);` |
|    29 |  2959 | `	}` |
|    59 |  2960 | `	if( bListValid && !bInnerValid ){` |
|   ! 0 |  2961 | `		ApListNext(pVm,pThis);` |
|   ! 0 |  2962 | `	}` |
|    59 |  2963 | `	if( PH7_NativeAttrObj(pThis,IT_IT) != 0 && bInnerValid ){` |
|    19 |  2964 | `		return PH7_OK;   /* mid-walk with a live element: the new tail waits its turn */` |
|     - |  2965 | `	}` |
|    41 |  2966 | `	pMap = ApMap(pVm,pThis);` |
|    41 |  2967 | `	if( pMap && pMap->pCur == 0 ){` |
|   ! 0 |  2968 | `		ApListRewind(pVm,pThis);` |
|   ! 0 |  2969 | `	}` |
|    21 |  2970 | `	for( nGuard = 0 ; ; ++nGuard ){` |
|    41 |  2971 | `		int bOk = 0;` |
|    41 |  2972 | `		rc = ApAdoptCurrent(pVm,pThis,&bOk);` |
|    41 |  2973 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  2974 | `			return rc;` |
|     - |  2975 | `		}` |
|    41 |  2976 | `		if( !bOk \|\| PH7_NativeAttrObj(pThis,IT_IN) == pIt ){` |
|    21 |  2977 | `			break;` |
|     - |  2978 | `		}` |
|   ! 0 |  2979 | `		ApListNext(pVm,pThis);` |
|   ! 0 |  2980 | `		if( nGuard > 100000 ){` |
|   ! 0 |  2981 | `			break;   /* php's loop has no bound; ours refuses to spin on a mutated list */` |
|     - |  2982 | `		}` |
|   ! 0 |  2983 | `	}` |
|    41 |  2984 | `	rc = ApFetch(pVm,pThis);` |
|    41 |  2985 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    30 |  2986 | `}` |
|    28 |  2987 | `static int vm_builtin_AppendIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  2988 | `{` |
|    29 |  2989 | `	ph7_vm *pVm = pCtx->pVm;` |
|    29 |  2990 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2991 | `	sxi32 rc;` |
|    29 |  2992 | `	int bOk = 0;` |
|    14 |  2993 | `	SXUNUSED(nArg);` |
|    14 |  2994 | `	SXUNUSED(apArg);` |
|    29 |  2995 | `	if( !DualReady(pThis) ){` |
|   ! 0 |  2996 | `		return DualNotReady(pCtx);` |
|     - |  2997 | `	}` |
|    29 |  2998 | `	ApListRewind(pVm,pThis);` |
|    29 |  2999 | `	rc = ApAdoptCurrent(pVm,pThis,&bOk);` |
|    29 |  3000 | `	if( rc == SXRET_OK && bOk ){` |
|    29 |  3001 | `		rc = ApFetch(pVm,pThis);` |
|    14 |  3002 | `	}` |
|    29 |  3003 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    15 |  3004 | `}` |
|    46 |  3005 | `static int vm_builtin_AppendIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3006 | `{` |
|    47 |  3007 | `	ph7_vm *pVm = pCtx->pVm;` |
|    47 |  3008 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3009 | `	sxi32 rc;` |
|    47 |  3010 | `	int bValid = 0;` |
|    23 |  3011 | `	SXUNUSED(nArg);` |
|    23 |  3012 | `	SXUNUSED(apArg);` |
|    47 |  3013 | `	if( !DualReady(pThis) ){` |
|   ! 0 |  3014 | `		return DualNotReady(pCtx);` |
|     - |  3015 | `	}` |
|    47 |  3016 | `	rc = DualInnerValid(pVm,pThis,&bValid);` |
|    47 |  3017 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  3018 | `		return rc;` |
|     - |  3019 | `	}` |
|    47 |  3020 | `	if( bValid ){` |
|    47 |  3021 | `		rc = DualNextInner(pVm,pThis);` |
|    47 |  3022 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  3023 | `			return rc;` |
|     - |  3024 | `		}` |
|    23 |  3025 | `	}` |
|    47 |  3026 | `	rc = ApFetch(pVm,pThis);` |
|    47 |  3027 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    24 |  3028 | `}` |
|     - |  3029 | `/* php re-fetches here (spl_dual_it_fetch with check_more), which is why an` |
|     - |  3030 | ` * AppendIterator FOLLOWS an inner iterator moved behind its back where every other` |
|     - |  3031 | ` * decorator answers its cache. */` |
|    64 |  3032 | `static int vm_builtin_AppendIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3033 | `{` |
|    65 |  3034 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3035 | `	sxi32 rc;` |
|    32 |  3036 | `	SXUNUSED(nArg);` |
|    32 |  3037 | `	SXUNUSED(apArg);` |
|    65 |  3038 | `	if( !DualReady(pThis) ){` |
|   ! 0 |  3039 | `		return DualNotReady(pCtx);` |
|     - |  3040 | `	}` |
|    65 |  3041 | `	rc = DualFetch(pCtx->pVm,pThis,TRUE);` |
|    65 |  3042 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  3043 | `		return rc;` |
|     - |  3044 | `	}` |
|    65 |  3045 | `	return DualResultSlot(pCtx,IT_CD);` |
|    33 |  3046 | `}` |
|     - |  3047 | `/* The list cursor's KEY, which is php's index into the appended iterators -- and` |
|     - |  3048 | ` * NULL once the cursor has run off the end, where the chunk kept answering the last` |
|     - |  3049 | ` * index it had seen. */` |
|    26 |  3050 | `static int vm_builtin_AppendIterator_getIteratorIndex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3051 | `{` |
|    27 |  3052 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3053 | `	ph7_value *pSlot,*apCall[1];` |
|    13 |  3054 | `	SXUNUSED(nArg);` |
|    13 |  3055 | `	SXUNUSED(apArg);` |
|    27 |  3056 | `	if( !DualReady(pThis) ){` |
|   ! 0 |  3057 | `		return DualNotReady(pCtx);` |
|     - |  3058 | `	}` |
|    27 |  3059 | `	pSlot = SplStoreSlot(pCtx->pVm,ApList(pThis));` |
|    27 |  3060 | `	if( pSlot == 0 ){` |
|   ! 0 |  3061 | `		ph7_result_null(pCtx);` |
|   ! 0 |  3062 | `		return PH7_OK;` |
|     - |  3063 | `	}` |
|    27 |  3064 | `	apCall[0] = pSlot;` |
|    27 |  3065 | `	return ph7_hashmap_simple_key(pCtx,1,apCall);` |
|    14 |  3066 | `}` |
|     - |  3067 | `/*` |
|     - |  3068 | ` * ---------------------------------------------------------------------------` |
|     - |  3069 | ` * The RECURSIVE pair: RecursiveArrayIterator (an ArrayIterator that descends into` |
|     - |  3070 | ` * its own entries) and RecursiveFilterIterator (a FilterIterator that forwards the` |
|     - |  3071 | ` * two recursion methods to its inner iterator).` |
|     - |  3072 | ` *` |
|     - |  3073 | ` * RecursiveArrayIterator is where php's CHILD_ARRAYS_ONLY flag lives, and the` |
|     - |  3074 | ``  * chunk's two-line `is_array($c) \|\| is_object($c)` / `new $c($this->current())` `` |
|     - |  3075 | ` * ignored it in both directions: an OBJECT entry claimed children under a flag that` |
|     - |  3076 | ` * exists to say it has none, and the child iterator was built WITHOUT the parent's` |
|     - |  3077 | ` * flags, so the restriction lasted exactly one level. php also answers null rather` |
|     - |  3078 | ` * than descending when there is no current element, and hands back an entry that is` |
|     - |  3079 | ` * ALREADY an instance of the called class instead of wrapping it again.` |
|     - |  3080 | ` */` |
|     - |  3081 | `#define RAI_CHILD_ARRAYS_ONLY 4` |
|     - |  3082 | `/* The entry the store cursor is on, or 0 past the end (php's` |
|     - |  3083 | ` * zend_hash_get_current_data_ex, which every one of these four bodies starts with). */` |
|   598 |  3084 | `static ph7_value * RaiCurrentEntry(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  3085 | `{` |
|   599 |  3086 | `	ph7_hashmap *pMap = SplStore(pVm,pThis);` |
|   599 |  3087 | `	return (pMap && pMap->pCur) ? HashmapExtractNodeValue(pMap->pCur) : 0;` |
|     1 |  3088 | `}` |
|   436 |  3089 | `static int vm_builtin_RecursiveArrayIterator_hasChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3090 | `{` |
|   437 |  3091 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   437 |  3092 | `	ph7_value *pEntry = RaiCurrentEntry(pCtx->pVm,pThis);` |
|   437 |  3093 | `	int bHas = 0;` |
|   218 |  3094 | `	SXUNUSED(nArg);` |
|   218 |  3095 | `	SXUNUSED(apArg);` |
|   437 |  3096 | `	if( pEntry ){` |
|   435 |  3097 | `		if( pEntry->iFlags & MEMOBJ_HASHMAP ){` |
|   165 |  3098 | `			bHas = 1;` |
|   353 |  3099 | `		}else if( pEntry->iFlags & MEMOBJ_OBJ ){` |
|     - |  3100 | `			/* php: an object is a child UNLESS the iterator was told arrays only. */` |
|    13 |  3101 | `			bHas = (PH7_NativeAttrInt(pThis,SPL_F) & RAI_CHILD_ARRAYS_ONLY) == 0;` |
|     6 |  3102 | `		}` |
|   217 |  3103 | `	}` |
|   437 |  3104 | `	ph7_result_bool(pCtx,bHas);` |
|   437 |  3105 | `	return PH7_OK;` |
|     1 |  3106 | `}` |
|   162 |  3107 | `static int vm_builtin_RecursiveArrayIterator_getChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3108 | `{` |
|   163 |  3109 | `	ph7_vm *pVm = pCtx->pVm;` |
|   163 |  3110 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   163 |  3111 | `	ph7_value *pEntry = RaiCurrentEntry(pVm,pThis);` |
|     - |  3112 | `	ph7_class_instance *pChild;` |
|     - |  3113 | `	ph7_class_method *pCons;` |
|     - |  3114 | `	ph7_value sEntry,sFlags,*apCtor[2];` |
|     - |  3115 | `	sxi64 iFlags;` |
|     - |  3116 | `	sxi32 rc;` |
|    81 |  3117 | `	SXUNUSED(nArg);` |
|    81 |  3118 | `	SXUNUSED(apArg);` |
|   163 |  3119 | `	if( pThis == 0 \|\| pEntry == 0 ){` |
|     3 |  3120 | `		ph7_result_null(pCtx);   /* php descends into nothing when nothing is current */` |
|     3 |  3121 | `		return PH7_OK;` |
|     - |  3122 | `	}` |
|   161 |  3123 | `	iFlags = PH7_NativeAttrInt(pThis,SPL_F);` |
|   161 |  3124 | `	if( pEntry->iFlags & MEMOBJ_OBJ ){` |
|     9 |  3125 | `		ph7_class_instance *pObj = (ph7_class_instance *)pEntry->x.pOther;` |
|     9 |  3126 | `		if( iFlags & RAI_CHILD_ARRAYS_ONLY ){` |
|     3 |  3127 | `			ph7_result_null(pCtx);` |
|     3 |  3128 | `			return PH7_OK;` |
|     - |  3129 | `		}` |
|     7 |  3130 | `		if( pObj && PH7_VmInstanceOf(pObj->pClass,pThis->pClass) ){` |
|     - |  3131 | `			/* Already one of us: php hands the entry back rather than wrapping it. */` |
|     3 |  3132 | `			SplResultBorrowed(pCtx,pObj);` |
|     3 |  3133 | `			return PH7_OK;` |
|     - |  3134 | `		}` |
|     2 |  3135 | `	}` |
|     - |  3136 | `	/* php's spl_instantiate_child_arg: the CALLED class, constructed with the entry` |
|     - |  3137 | `	 * AND the parent's flags -- which is what carries CHILD_ARRAYS_ONLY down. */` |
|   157 |  3138 | `	pChild = PH7_NewClassInstance(pVm,pThis->pClass);` |
|   157 |  3139 | `	if( pChild == 0 ){` |
|   ! 0 |  3140 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  3141 | `	}` |
|   157 |  3142 | `	pChild->iRef++;` |
|   157 |  3143 | `	PH7_MemObjInit(pVm,&sEntry);` |
|   157 |  3144 | `	PH7_MemObjStore(pEntry,&sEntry);` |
|   157 |  3145 | `	PH7_MemObjInitFromInt(pVm,&sFlags,iFlags);` |
|   157 |  3146 | `	apCtor[0] = &sEntry;` |
|   157 |  3147 | `	apCtor[1] = &sFlags;` |
|   157 |  3148 | `	pCons = PH7_ClassExtractMethod(pThis->pClass,"__construct",sizeof("__construct")-1);` |
|   157 |  3149 | `	rc = pCons ? PH7_VmCallClassMethod(pVm,pChild,pCons,0,2,apCtor) : SXRET_OK;` |
|   157 |  3150 | `	PH7_MemObjRelease(&sEntry);` |
|   157 |  3151 | `	PH7_MemObjRelease(&sFlags);` |
|   157 |  3152 | `	if( rc != SXRET_OK ){` |
|     7 |  3153 | `		PH7_ClassInstanceUnref(pChild);` |
|     7 |  3154 | `		return rc;` |
|     - |  3155 | `	}` |
|   151 |  3156 | `	PH7_NativeResultObject(pCtx,pChild);` |
|   151 |  3157 | `	PH7_ClassInstanceUnref(pChild);` |
|   151 |  3158 | `	return PH7_OK;` |
|    82 |  3159 | `}` |
|     - |  3160 | `/* RecursiveFilterIterator forwards both methods to the object getInnerIterator()` |
|     - |  3161 | ` * answers (php calls on inner.zobject), and wraps the children in ITS OWN class. */` |
|    20 |  3162 | `static int vm_builtin_RecursiveFilterIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3163 | `{` |
|    21 |  3164 | `	return DualConstruct(pCtx,"RecursiveFilterIterator",nArg,apArg);` |
|     1 |  3165 | `}` |
|   104 |  3166 | `static sxi32 RfiCallInner(ph7_context *pCtx,const char *zName,sxu32 nName,ph7_value *pOut)` |
|     1 |  3167 | `{` |
|   105 |  3168 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   105 |  3169 | `	ph7_class_instance *pIn = pThis ? PH7_NativeAttrObj(pThis,IT_IN) : 0;` |
|   105 |  3170 | `	ph7_class_method *pMethod = pIn ? PH7_ClassExtractMethod(pIn->pClass,zName,nName) : 0;` |
|   105 |  3171 | `	if( pMethod == 0 ){` |
|   ! 0 |  3172 | `		return SXRET_OK;` |
|     - |  3173 | `	}` |
|   105 |  3174 | `	return PH7_VmCallClassMethod(pCtx->pVm,pIn,pMethod,pOut,0,0);` |
|    53 |  3175 | `}` |
|    46 |  3176 | `static int vm_builtin_RecursiveFilterIterator_hasChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3177 | `{` |
|    47 |  3178 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3179 | `	ph7_value sRes;` |
|     - |  3180 | `	sxi32 rc;` |
|    23 |  3181 | `	SXUNUSED(nArg);` |
|    23 |  3182 | `	SXUNUSED(apArg);` |
|    47 |  3183 | `	if( !DualReady(pThis) ){` |
|     5 |  3184 | `		return DualNotReady(pCtx);` |
|     - |  3185 | `	}` |
|    43 |  3186 | `	PH7_MemObjInit(pCtx->pVm,&sRes);` |
|    43 |  3187 | `	rc = RfiCallInner(pCtx,"hasChildren",sizeof("hasChildren")-1,&sRes);` |
|    43 |  3188 | `	if( rc == SXRET_OK ){` |
|    43 |  3189 | `		ph7_result_value(pCtx,&sRes);` |
|    21 |  3190 | `	}` |
|    43 |  3191 | `	PH7_MemObjRelease(&sRes);` |
|    43 |  3192 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    24 |  3193 | `}` |
|     - |  3194 | `/*` |
|     - |  3195 | ` * php's spl_instantiate_arg_ex1/2/3 for the recursive filters: fetch the INNER` |
|     - |  3196 | ` * iterator's children and hand them to a fresh instance of the CALLED class` |
|     - |  3197 | `` * (`Z_OBJCE_P(ZEND_THIS)`, so a user subclass answers its own type), followed by`` |
|     - |  3198 | ` * whatever the subclass's constructor needs after the iterator -- the callback for` |
|     - |  3199 | ` * RecursiveCallbackFilterIterator, the four regex arguments for` |
|     - |  3200 | ` * RecursiveRegexIterator, nothing for the other two.` |
|     - |  3201 | ` */` |
|    26 |  3202 | `static int RfiBuildChild(ph7_context *pCtx,ph7_value **apExtra,int nExtra)` |
|     1 |  3203 | `{` |
|    27 |  3204 | `	ph7_vm *pVm = pCtx->pVm;` |
|    27 |  3205 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3206 | `	ph7_class_instance *pChild;` |
|     - |  3207 | `	ph7_class_method *pCons;` |
|     - |  3208 | `	ph7_value sInner,*apCtor[5];` |
|     - |  3209 | `	int i;` |
|     - |  3210 | `	sxi32 rc;` |
|    27 |  3211 | `	if( !DualReady(pThis) ){` |
|   ! 0 |  3212 | `		return DualNotReady(pCtx);` |
|     - |  3213 | `	}` |
|    27 |  3214 | `	if( nExtra > (int)SX_ARRAYSIZE(apCtor) - 1 ){` |
|   ! 0 |  3215 | `		nExtra = (int)SX_ARRAYSIZE(apCtor) - 1;` |
|   ! 0 |  3216 | `	}` |
|    27 |  3217 | `	PH7_MemObjInit(pVm,&sInner);` |
|    27 |  3218 | `	rc = RfiCallInner(pCtx,"getChildren",sizeof("getChildren")-1,&sInner);` |
|    27 |  3219 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  3220 | `		PH7_MemObjRelease(&sInner);` |
|   ! 0 |  3221 | `		return rc;` |
|     - |  3222 | `	}` |
|    27 |  3223 | `	pChild = PH7_NewClassInstance(pVm,pThis->pClass);` |
|    27 |  3224 | `	if( pChild == 0 ){` |
|   ! 0 |  3225 | `		PH7_MemObjRelease(&sInner);` |
|   ! 0 |  3226 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  3227 | `	}` |
|    27 |  3228 | `	pChild->iRef++;` |
|    27 |  3229 | `	apCtor[0] = &sInner;` |
|    49 |  3230 | `	for( i = 0 ; i < nExtra ; ++i ){` |
|    23 |  3231 | `		apCtor[i+1] = apExtra[i];` |
|    12 |  3232 | `	}` |
|    27 |  3233 | `	pCons = PH7_ClassExtractMethod(pThis->pClass,"__construct",sizeof("__construct")-1);` |
|    27 |  3234 | `	rc = pCons ? PH7_VmCallClassMethod(pVm,pChild,pCons,0,nExtra+1,apCtor) : SXRET_OK;` |
|    27 |  3235 | `	PH7_MemObjRelease(&sInner);` |
|    27 |  3236 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  3237 | `		PH7_ClassInstanceUnref(pChild);` |
|   ! 0 |  3238 | `		return rc;` |
|     - |  3239 | `	}` |
|    27 |  3240 | `	PH7_NativeResultObject(pCtx,pChild);` |
|    27 |  3241 | `	PH7_ClassInstanceUnref(pChild);` |
|    27 |  3242 | `	return PH7_OK;` |
|    14 |  3243 | `}` |
|    16 |  3244 | `static int vm_builtin_RecursiveFilterIterator_getChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3245 | `{` |
|     8 |  3246 | `	SXUNUSED(nArg);` |
|     8 |  3247 | `	SXUNUSED(apArg);` |
|    17 |  3248 | `	return RfiBuildChild(pCtx,0,0);` |
|     1 |  3249 | `}` |
|     - |  3250 | `/*` |
|     - |  3251 | ` * ParentIterator: the RecursiveFilterIterator whose accept() IS the question` |
|     - |  3252 | ` * "does the current element have children?". php asks the INNER iterator` |
|     - |  3253 | `` * (`inner.zobject`), not `$this`, so overriding hasChildren() on the`` |
|     - |  3254 | ` * ParentIterator subclass changes nothing and overriding it on the inner` |
|     - |  3255 | ` * RecursiveIterator changes everything -- and the answer is cast to a bool.` |
|     - |  3256 | ` */` |
|    22 |  3257 | `static int vm_builtin_ParentIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3258 | `{` |
|    23 |  3259 | `	return DualConstruct(pCtx,"ParentIterator",nArg,apArg);` |
|     1 |  3260 | `}` |
|    38 |  3261 | `static int vm_builtin_ParentIterator_accept(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3262 | `{` |
|    39 |  3263 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3264 | `	ph7_value sRes;` |
|     - |  3265 | `	sxi32 rc;` |
|    19 |  3266 | `	SXUNUSED(nArg);` |
|    19 |  3267 | `	SXUNUSED(apArg);` |
|    39 |  3268 | `	if( !DualReady(pThis) ){` |
|     3 |  3269 | `		return DualNotReady(pCtx);` |
|     - |  3270 | `	}` |
|    37 |  3271 | `	PH7_MemObjInit(pCtx->pVm,&sRes);` |
|    37 |  3272 | `	rc = RfiCallInner(pCtx,"hasChildren",sizeof("hasChildren")-1,&sRes);` |
|    37 |  3273 | `	if( rc == SXRET_OK ){` |
|    37 |  3274 | `		PH7_MemObjToBool(&sRes);          /* a STATUS, not the answer */` |
|    37 |  3275 | `		ph7_result_bool(pCtx,sRes.x.iVal != 0);` |
|    18 |  3276 | `	}` |
|    37 |  3277 | `	PH7_MemObjRelease(&sRes);` |
|    37 |  3278 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    20 |  3279 | `}` |
|     - |  3280 | `/*` |
|     - |  3281 | ` * RecursiveCallbackFilterIterator: the callback filter's recursive twin. Both` |
|     - |  3282 | ` * halves are inherited behaviour -- accept() is CallbackFilterIterator's and` |
|     - |  3283 | ` * hasChildren() is RecursiveFilterIterator's -- but php DECLARES all four names on` |
|     - |  3284 | ` * the class, and getChildren() has to carry the callback down to the child.` |
|     - |  3285 | ` */` |
|    14 |  3286 | `static int vm_builtin_RecursiveCallbackFilterIterator_construct(ph7_context *pCtx,int nArg,` |
|     - |  3287 | `	ph7_value **apArg)` |
|     1 |  3288 | `{` |
|     - |  3289 | `	ph7_class_instance *pThis;` |
|     - |  3290 | `	sxi32 rc;` |
|    15 |  3291 | `	if( nArg > 1 ){` |
|    15 |  3292 | `		rc = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",FALSE);` |
|    15 |  3293 | `		if( rc != PH7_OK ){` |
|     3 |  3294 | `			return rc;` |
|     - |  3295 | `		}` |
|     6 |  3296 | `	}` |
|    13 |  3297 | `	rc = DualConstruct(pCtx,"RecursiveCallbackFilterIterator",nArg,apArg);` |
|    13 |  3298 | `	pThis = PH7_ContextThis(pCtx);` |
|    13 |  3299 | `	if( rc == PH7_OK && pThis && nArg > 1 ){` |
|    13 |  3300 | `		DualSetSlot(pCtx->pVm,pThis,IT_CB,apArg[1]);` |
|     6 |  3301 | `	}` |
|    13 |  3302 | `	return rc;` |
|     8 |  3303 | `}` |
|     8 |  3304 | `static int vm_builtin_RecursiveCallbackFilterIterator_getChildren(ph7_context *pCtx,int nArg,` |
|     - |  3305 | `	ph7_value **apArg)` |
|     1 |  3306 | `{` |
|     9 |  3307 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3308 | `	ph7_value sCb,*apExtra[1];` |
|     - |  3309 | `	int rc;` |
|     4 |  3310 | `	SXUNUSED(nArg);` |
|     4 |  3311 | `	SXUNUSED(apArg);` |
|     9 |  3312 | `	if( !DualReady(pThis) ){` |
|     3 |  3313 | `		return DualNotReady(pCtx);` |
|     - |  3314 | `	}` |
|     - |  3315 | `	/* Take the callback as a VALUE: the child's constructor runs user code, and a` |
|     - |  3316 | `	 * pointer into pVm->aMemObj does not survive that. */` |
|     7 |  3317 | `	PH7_MemObjInit(pCtx->pVm,&sCb);` |
|     - |  3318 | `	{` |
|     7 |  3319 | `		ph7_value *pCb = PH7_NativeAttr(pThis,IT_CB);` |
|     7 |  3320 | `		if( pCb ){` |
|     7 |  3321 | `			PH7_MemObjStore(pCb,&sCb);` |
|     3 |  3322 | `		}` |
|     - |  3323 | `	}` |
|     7 |  3324 | `	apExtra[0] = &sCb;` |
|     7 |  3325 | `	rc = RfiBuildChild(pCtx,apExtra,1);` |
|     7 |  3326 | `	PH7_MemObjRelease(&sCb);` |
|     7 |  3327 | `	return rc;` |
|     5 |  3328 | `}` |
|     - |  3329 | `/*` |
|     - |  3330 | ` * RecursiveRegexIterator: the regex filter's recursive twin. Its accept() has one` |
|     - |  3331 | ` * rule of its own, and it comes BEFORE everything RegexIterator does: a current()` |
|     - |  3332 | ` * that is an ARRAY is accepted when it is non-empty, whatever the mode, the` |
|     - |  3333 | ` * pattern, USE_KEY or INVERT_MATCH say -- a container is kept so the walk can` |
|     - |  3334 | ` * descend into it, and only its LEAVES are matched. (RegexIterator itself refuses` |
|     - |  3335 | ` * an array outright, which is what makes the plain class useless recursively.)` |
|     - |  3336 | ``  * getChildren() carries the four regex arguments down; php passes `replacement` `` |
|     - |  3337 | ` * to nothing, so a child starts with the declared NULL.` |
|     - |  3338 | ` */` |
|    26 |  3339 | `static int vm_builtin_RecursiveRegexIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3340 | `{` |
|    27 |  3341 | `	return RegitConstruct(pCtx,"RecursiveRegexIterator",nArg,apArg);` |
|     1 |  3342 | `}` |
|    28 |  3343 | `static int vm_builtin_RecursiveRegexIterator_accept(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3344 | `{` |
|    29 |  3345 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    29 |  3346 | `	if( pThis == 0 \|\| DualDriver(pThis) == 0 ){` |
|     3 |  3347 | `		return DualNotReady(pCtx);` |
|     - |  3348 | `	}` |
|    27 |  3349 | `	if( DualFilled(pThis) ){` |
|    27 |  3350 | `		ph7_value *pCur = PH7_NativeAttr(pThis,IT_CD);` |
|    27 |  3351 | `		if( pCur && (pCur->iFlags & MEMOBJ_HASHMAP) && pCur->x.pOther ){` |
|    17 |  3352 | `			ph7_result_bool(pCtx,((ph7_hashmap *)pCur->x.pOther)->nEntry > 0);` |
|    17 |  3353 | `			return PH7_OK;` |
|     - |  3354 | `		}` |
|     5 |  3355 | `	}` |
|    11 |  3356 | `	return vm_builtin_RegexIterator_accept(pCtx,nArg,apArg);` |
|    15 |  3357 | `}` |
|     6 |  3358 | `static int vm_builtin_RecursiveRegexIterator_getChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3359 | `{` |
|     7 |  3360 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 |  3361 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3362 | `	ph7_value sRe,sMode,sFlags,sPreg,*apExtra[4];` |
|     - |  3363 | `	ph7_value *pRe;` |
|     - |  3364 | `	int rc;` |
|     3 |  3365 | `	SXUNUSED(nArg);` |
|     3 |  3366 | `	SXUNUSED(apArg);` |
|     7 |  3367 | `	if( !DualReady(pThis) ){` |
|     3 |  3368 | `		return DualNotReady(pCtx);` |
|     - |  3369 | `	}` |
|     5 |  3370 | `	PH7_MemObjInit(pVm,&sRe);` |
|     5 |  3371 | `	pRe = PH7_NativeAttr(pThis,IT_RE);` |
|     5 |  3372 | `	if( pRe ){` |
|     5 |  3373 | `		PH7_MemObjStore(pRe,&sRe);` |
|     2 |  3374 | `	}` |
|     5 |  3375 | `	PH7_MemObjInitFromInt(pVm,&sMode,PH7_NativeAttrInt(pThis,IT_RM));` |
|     5 |  3376 | `	PH7_MemObjInitFromInt(pVm,&sFlags,PH7_NativeAttrInt(pThis,IT_RF));` |
|     5 |  3377 | `	PH7_MemObjInitFromInt(pVm,&sPreg,PH7_NativeAttrInt(pThis,IT_RP));` |
|     5 |  3378 | `	apExtra[0] = &sRe;` |
|     5 |  3379 | `	apExtra[1] = &sMode;` |
|     5 |  3380 | `	apExtra[2] = &sFlags;` |
|     5 |  3381 | `	apExtra[3] = &sPreg;` |
|     5 |  3382 | `	rc = RfiBuildChild(pCtx,apExtra,4);` |
|     5 |  3383 | `	PH7_MemObjRelease(&sRe);` |
|     5 |  3384 | `	PH7_MemObjRelease(&sMode);` |
|     5 |  3385 | `	PH7_MemObjRelease(&sFlags);` |
|     5 |  3386 | `	PH7_MemObjRelease(&sPreg);` |
|     5 |  3387 | `	return rc;` |
|     4 |  3388 | `}` |
|    12 |  3389 | `static int vm_builtin_AppendIterator_getArrayIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3390 | `{` |
|    13 |  3391 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3392 | `	ph7_class_instance *pList;` |
|     6 |  3393 | `	SXUNUSED(nArg);` |
|     6 |  3394 | `	SXUNUSED(apArg);` |
|    13 |  3395 | `	if( !DualReady(pThis) ){` |
|   ! 0 |  3396 | `		return DualNotReady(pCtx);` |
|     - |  3397 | `	}` |
|    13 |  3398 | `	pList = ApList(pThis);` |
|    13 |  3399 | `	if( pList ){` |
|    13 |  3400 | `		SplResultBorrowed(pCtx,pList);` |
|     7 |  3401 | `	}else{` |
|   ! 0 |  3402 | `		ph7_result_null(pCtx);` |
|     - |  3403 | `	}` |
|    13 |  3404 | `	return PH7_OK;` |
|     7 |  3405 | `}` |
|     - |  3406 | `/*` |
|     - |  3407 | ` * ---------------------------------------------------------------------------` |
|     - |  3408 | ` * CachingIterator and RecursiveCachingIterator.` |
|     - |  3409 | ` *` |
|     - |  3410 | ` * The caching iterator is one step AHEAD of the iterator it decorates: each fetch` |
|     - |  3411 | ` * copies current()/key() into the cache every dual iterator keeps and then ADVANCES` |
|     - |  3412 | ` * the inner one, which is what makes hasNext() answerable at all — it is the` |
|     - |  3413 | ` * inner's live valid(), asked after that step. Everything else this class does` |
|     - |  3414 | ` * happens inside the same fetch, in php's order: the FULL_CACHE entry is written,` |
|     - |  3415 | ` * then (for the recursive twin) the CHILDREN are built, then the string form is` |
|     - |  3416 | ` * computed, then the inner is advanced.` |
|     - |  3417 | ` *` |
|     - |  3418 | ` * That eager string is the class's least obvious rule. CALL_TOSTRING casts the` |
|     - |  3419 | ` * ELEMENT and TOSTRING_USE_INNER casts the inner ITERATOR, both at FETCH time, so` |
|     - |  3420 | `` * the default `new CachingIterator($it)` over objects with no __toString throws`` |
|     - |  3421 | `` * from rewind() and over arrays warns `Array to string conversion` once per`` |
|     - |  3422 | `` * element — neither waits for anyone to write `(string)$it`. The other two`` |
|     - |  3423 | ` * spellings (TOSTRING_USE_KEY / TOSTRING_USE_CURRENT) are read out of the cache at` |
|     - |  3424 | ` * __toString() time instead, and NO spelling at all is a BadMethodCallException` |
|     - |  3425 | ` * that names the RECEIVER's class.` |
|     - |  3426 | ` *` |
|     - |  3427 | ` * getFlags() answers the RAW word, php's private CIT_VALID (0x10000) included, so` |
|     - |  3428 | ` * a fetched iterator reports 65537 where its constructor was handed 1. setFlags()` |
|     - |  3429 | ` * keeps the high half and replaces the low one, and refuses to unset either of the` |
|     - |  3430 | ` * two flags whose machinery cannot be turned off mid-walk; the CONSTRUCTOR masks` |
|     - |  3431 | `` * with CIT_PUBLIC instead, so `new CachingIterator($it, 1024)` reports 1024 while`` |
|     - |  3432 | ` * setFlags(1024) on a default instance is a refusal.` |
|     - |  3433 | ` */` |
|     - |  3434 | `#define CIT_FL   "__cfl"    /* php's u.caching.flags, its private CIT_VALID included */` |
|     - |  3435 | `#define CIT_STR  "__cstr"   /* php's u.caching.zstr: the string computed at fetch */` |
|     - |  3436 | `#define CIT_CCH  "__ccch"   /* php's u.caching.zcache */` |
|     - |  3437 | `#define CIT_KIDS "__ckid"   /* php's u.caching.zchildren, the recursive twin's only state */` |
|     - |  3438 |  |
|     - |  3439 | `#define CIT_CALL_TOSTRING     0x00000001` |
|     - |  3440 | `#define CIT_TOSTRING_USE_KEY  0x00000002` |
|     - |  3441 | `#define CIT_TOSTRING_USE_CUR  0x00000004` |
|     - |  3442 | `#define CIT_TOSTRING_USE_INN  0x00000008` |
|     - |  3443 | `#define CIT_CATCH_GET_CHILD   0x00000010` |
|     - |  3444 | `#define CIT_FULL_CACHE        0x00000100` |
|     - |  3445 | `#define CIT_PUBLIC            0x0000FFFF` |
|     - |  3446 | `#define CIT_VALID             0x00010000` |
|     - |  3447 |  |
|   916 |  3448 | `static sxi64 CitFlags(ph7_class_instance *pThis)` |
|     2 |  3449 | `{` |
|   918 |  3450 | `	return pThis ? PH7_NativeAttrInt(pThis,CIT_FL) : 0;` |
|     2 |  3451 | `}` |
|     - |  3452 | `/* php's spl_cit_check_flags: at most ONE of the four string spellings. */` |
|   186 |  3453 | `static int CitCheckFlags(sxi64 iFlags)` |
|     2 |  3454 | `{` |
|   188 |  3455 | `	int n = 0;` |
|   188 |  3456 | `	if( iFlags & CIT_CALL_TOSTRING ){ n++; }` |
|   188 |  3457 | `	if( iFlags & CIT_TOSTRING_USE_KEY ){ n++; }` |
|   188 |  3458 | `	if( iFlags & CIT_TOSTRING_USE_CUR ){ n++; }` |
|   188 |  3459 | `	if( iFlags & CIT_TOSTRING_USE_INN ){ n++; }` |
|   188 |  3460 | `	return n <= 1;` |
|     2 |  3461 | `}` |
|     - |  3462 | `/* Both of this class's refusals name the RECEIVER's class and point at` |
|     - |  3463 | ` * CachingIterator::__construct whatever that receiver is. */` |
|    16 |  3464 | `static int CitRefuse(ph7_context *pCtx,ph7_class_instance *pThis,const char *zWhat)` |
|     1 |  3465 | `{` |
|    17 |  3466 | `	SyString *pName = &pThis->pClass->sName;` |
|    25 |  3467 | `	return PH7_VmThrowException(pCtx,"BadMethodCallException",` |
|     8 |  3468 | `		"%z does not %s (see CachingIterator::__construct)",pName,zWhat);` |
|     1 |  3469 | `}` |
|     - |  3470 | `/* The cache slot, separated for writing (every caller may mutate it). */` |
|    76 |  3471 | `static ph7_value * CitCacheSlot(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     2 |  3472 | `{` |
|    78 |  3473 | `	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,CIT_CCH) : 0;` |
|    78 |  3474 | `	if( pSlot == 0 ){` |
|   ! 0 |  3475 | `		return 0;` |
|     - |  3476 | `	}` |
|    78 |  3477 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     3 |  3478 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|   ! 0 |  3479 | `			return 0;` |
|     - |  3480 | `		}` |
|     1 |  3481 | `	}` |
|    78 |  3482 | `	if( PH7_HashmapCowSeparate(pVm,pSlot) == 0 ){` |
|   ! 0 |  3483 | `		return 0;` |
|     - |  3484 | `	}` |
|    78 |  3485 | `	return pSlot;` |
|    40 |  3486 | `}` |
|     - |  3487 | `/* Every cache reader is refused outright without FULL_CACHE, php's own guard. */` |
|    70 |  3488 | `static ph7_value * CitCacheChecked(ph7_context *pCtx,int *pRc)` |
|     2 |  3489 | `{` |
|    72 |  3490 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    72 |  3491 | `	*pRc = PH7_OK;` |
|    72 |  3492 | `	if( !DualReady(pThis) ){` |
|     5 |  3493 | `		*pRc = DualNotReady(pCtx);` |
|     5 |  3494 | `		return 0;` |
|     - |  3495 | `	}` |
|    68 |  3496 | `	if( (CitFlags(pThis) & CIT_FULL_CACHE) == 0 ){` |
|    13 |  3497 | `		*pRc = CitRefuse(pCtx,pThis,"use a full cache");` |
|    13 |  3498 | `		return 0;` |
|     - |  3499 | `	}` |
|    56 |  3500 | `	return CitCacheSlot(pCtx->pVm,pThis);` |
|    37 |  3501 | `}` |
|     - |  3502 | `/*` |
|     - |  3503 | `` * php's array_set_zval_key screens the key exactly as `$a[$k] = v` does, so an`` |
|     - |  3504 | ` * OBJECT or ARRAY key raises rather than folding to anything — the same wording` |
|     - |  3505 | ` * the engine's own subscript store uses.` |
|     - |  3506 | ` */` |
|    22 |  3507 | `static int CitCacheKeyCheck(ph7_context *pCtx,ph7_value *pKey)` |
|     2 |  3508 | `{` |
|    24 |  3509 | `	if( pKey->iFlags & MEMOBJ_OBJ ){` |
|   ! 0 |  3510 | `		ph7_class_instance *pInst = (ph7_class_instance *)pKey->x.pOther;` |
|   ! 0 |  3511 | `		SyString *pName = pInst && pInst->pClass ? &pInst->pClass->sName : 0;` |
|   ! 0 |  3512 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|   ! 0 |  3513 | `			"Cannot access offset of type %z on array",pName);` |
|     - |  3514 | `	}` |
|    24 |  3515 | `	if( pKey->iFlags & MEMOBJ_HASHMAP ){` |
|   ! 0 |  3516 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  3517 | `			"Cannot access offset of type array on array");` |
|     - |  3518 | `	}` |
|    24 |  3519 | `	return PH7_OK;` |
|    13 |  3520 | `}` |
|     - |  3521 | `/*` |
|     - |  3522 | ` * php's spl_caching_it_next tail: the string the class will answer from. The two` |
|     - |  3523 | ` * eager spellings are exclusive (spl_cit_check_flags saw to that), and the cast is` |
|     - |  3524 | ` * php's own, warnings and refusals included.` |
|     - |  3525 | ` */` |
|    16 |  3526 | `static sxi32 CitMakeString(ph7_context *pCtx,ph7_class_instance *pThis,sxi64 iFlags)` |
|     1 |  3527 | `{` |
|    17 |  3528 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  3529 | `	ph7_value sVal;` |
|     - |  3530 | `	sxi32 rc;` |
|    17 |  3531 | `	PH7_MemObjInit(pVm,&sVal);` |
|    17 |  3532 | `	if( iFlags & CIT_TOSTRING_USE_INN ){` |
|     3 |  3533 | `		ph7_class_instance *pIn = PH7_NativeAttrObj(pThis,IT_IN);` |
|     3 |  3534 | `		if( pIn ){` |
|     - |  3535 | `			/* The temporary OWNS this reference: PH7_MemObjRelease below drops one,` |
|     - |  3536 | `			 * and the cast itself may retype the slot out from under the object. */` |
|     3 |  3537 | `			pIn->iRef++;` |
|     3 |  3538 | `			sVal.x.pOther = pIn;` |
|     3 |  3539 | `			MemObjSetType(&sVal,MEMOBJ_OBJ);` |
|     1 |  3540 | `		}` |
|     2 |  3541 | `	}else{` |
|    15 |  3542 | `		ph7_value *pCur = PH7_NativeAttr(pThis,IT_CD);` |
|    15 |  3543 | `		if( pCur ){` |
|    15 |  3544 | `			PH7_MemObjStore(pCur,&sVal);` |
|     7 |  3545 | `		}` |
|     - |  3546 | `	}` |
|    17 |  3547 | `	rc = PH7_MemObjToStringUV(&sVal);` |
|    17 |  3548 | `	if( rc == SXRET_OK ){` |
|    15 |  3549 | `		DualSetSlot(pVm,pThis,CIT_STR,&sVal);` |
|     7 |  3550 | `	}` |
|    17 |  3551 | `	PH7_MemObjRelease(&sVal);` |
|    17 |  3552 | `	return rc;` |
|     1 |  3553 | `}` |
|     - |  3554 | `/*` |
|     - |  3555 | ` * php's recursion half of the same fetch: ask the INNER iterator whether the` |
|     - |  3556 | ` * element has children and, if it does, build the child decorator EAGERLY —` |
|     - |  3557 | ` * getChildren() only hands back what this already made. CATCH_GET_CHILD swallows` |
|     - |  3558 | ` * a throw from any of the three steps (hasChildren, getChildren, and the child's` |
|     - |  3559 | ` * own constructor), which is php's clear-the-exception-and-carry-on.` |
|     - |  3560 | ` *` |
|     - |  3561 | ` * The child is a plain RecursiveCachingIterator even when the receiver is a` |
|     - |  3562 | ` * SUBCLASS: php names the class entry here rather than reading ZEND_THIS's, which` |
|     - |  3563 | ` * is the opposite of what the recursive FILTERS do.` |
|     - |  3564 | ` */` |
|   142 |  3565 | `static sxi32 CitBuildChildren(ph7_context *pCtx,ph7_class_instance *pThis)` |
|     1 |  3566 | `{` |
|   143 |  3567 | `	ph7_vm *pVm = pCtx->pVm;` |
|   143 |  3568 | `	ph7_class_instance *pIn = PH7_NativeAttrObj(pThis,IT_IN);` |
|     - |  3569 | `	ph7_class_instance *pChild;` |
|     - |  3570 | `	ph7_class *pCls;` |
|     - |  3571 | `	ph7_class_method *pMethod;` |
|     - |  3572 | `	ph7_value sRes,sFlags,*apCtor[2];` |
|   143 |  3573 | `	int bCatch = (CitFlags(pThis) & CIT_CATCH_GET_CHILD) != 0;` |
|   143 |  3574 | `	int bThrew = FALSE;` |
|     - |  3575 | `	sxi32 rc;` |
|   143 |  3576 | `	PH7_NativeSetAttrObj(pVm,pThis,CIT_KIDS,0);` |
|   143 |  3577 | `	pMethod = pIn ? PH7_ClassExtractMethod(pIn->pClass,"hasChildren",sizeof("hasChildren")-1) : 0;` |
|   143 |  3578 | `	if( pMethod == 0 ){` |
|   ! 0 |  3579 | `		return SXRET_OK;` |
|     - |  3580 | `	}` |
|   143 |  3581 | `	PH7_MemObjInit(pVm,&sRes);` |
|   127 |  3582 | `	rc = bCatch ? PH7_VmCallMethodSwallow(pVm,pIn,pMethod,&sRes,0,0,&bThrew)` |
|    87 |  3583 | `	            : PH7_VmCallClassMethod(pVm,pIn,pMethod,&sRes,0,0);` |
|   143 |  3584 | `	if( rc != SXRET_OK \|\| bThrew ){` |
|     5 |  3585 | `		PH7_MemObjRelease(&sRes);` |
|     5 |  3586 | `		return rc;` |
|     - |  3587 | `	}` |
|   139 |  3588 | `	PH7_MemObjToBool(&sRes);              /* a STATUS, not the answer */` |
|   139 |  3589 | `	if( sRes.x.iVal == 0 ){` |
|    89 |  3590 | `		PH7_MemObjRelease(&sRes);` |
|    89 |  3591 | `		return SXRET_OK;` |
|     - |  3592 | `	}` |
|    51 |  3593 | `	PH7_MemObjRelease(&sRes);` |
|    51 |  3594 | `	pMethod = PH7_ClassExtractMethod(pIn->pClass,"getChildren",sizeof("getChildren")-1);` |
|    51 |  3595 | `	if( pMethod == 0 ){` |
|   ! 0 |  3596 | `		return SXRET_OK;` |
|     - |  3597 | `	}` |
|    51 |  3598 | `	PH7_MemObjInit(pVm,&sRes);` |
|    45 |  3599 | `	rc = bCatch ? PH7_VmCallMethodSwallow(pVm,pIn,pMethod,&sRes,0,0,&bThrew)` |
|    31 |  3600 | `	            : PH7_VmCallClassMethod(pVm,pIn,pMethod,&sRes,0,0);` |
|    51 |  3601 | `	if( rc != SXRET_OK \|\| bThrew ){` |
|   ! 0 |  3602 | `		PH7_MemObjRelease(&sRes);` |
|   ! 0 |  3603 | `		return rc;` |
|     - |  3604 | `	}` |
|    51 |  3605 | `	pCls = PH7_VmExtractClass(pVm,"RecursiveCachingIterator",` |
|     - |  3606 | `		sizeof("RecursiveCachingIterator")-1,FALSE,0);` |
|    51 |  3607 | `	pMethod = pCls ? PH7_ClassExtractMethod(pCls,"__construct",sizeof("__construct")-1) : 0;` |
|    51 |  3608 | `	if( pMethod == 0 ){` |
|   ! 0 |  3609 | `		PH7_MemObjRelease(&sRes);` |
|   ! 0 |  3610 | `		return SXRET_OK;` |
|     - |  3611 | `	}` |
|    51 |  3612 | `	pChild = PH7_NewClassInstance(pVm,pCls);` |
|    51 |  3613 | `	if( pChild == 0 ){` |
|   ! 0 |  3614 | `		PH7_MemObjRelease(&sRes);` |
|   ! 0 |  3615 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  3616 | `	}` |
|    51 |  3617 | `	pChild->iRef++;` |
|    51 |  3618 | `	PH7_MemObjInitFromInt(pVm,&sFlags,CitFlags(pThis) & CIT_PUBLIC);` |
|    51 |  3619 | `	apCtor[0] = &sRes;` |
|    51 |  3620 | `	apCtor[1] = &sFlags;` |
|    45 |  3621 | `	rc = bCatch ? PH7_VmCallMethodSwallow(pVm,pChild,pMethod,0,2,apCtor,&bThrew)` |
|    31 |  3622 | `	            : PH7_VmCallClassMethod(pVm,pChild,pMethod,0,2,apCtor);` |
|    51 |  3623 | `	PH7_MemObjRelease(&sRes);` |
|    51 |  3624 | `	PH7_MemObjRelease(&sFlags);` |
|    51 |  3625 | `	if( rc == SXRET_OK && !bThrew ){` |
|    47 |  3626 | `		PH7_NativeSetAttrObj(pVm,pThis,CIT_KIDS,pChild);` |
|    23 |  3627 | `	}` |
|    51 |  3628 | `	PH7_ClassInstanceUnref(pChild);` |
|    51 |  3629 | `	return rc;` |
|    72 |  3630 | `}` |
|     - |  3631 | `/*` |
|     - |  3632 | `` * php's `intern->dit_type == DIT_RecursiveCachingIterator`. rewind() and next()`` |
|     - |  3633 | ` * are declared on CachingIterator ALONE and inherited by the twin, so the one body` |
|     - |  3634 | ` * they share has to ask what it is standing on.` |
|     - |  3635 | ` */` |
|   258 |  3636 | `static int CitIsRecursive(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     2 |  3637 | `{` |
|   260 |  3638 | `	ph7_class *pCls = PH7_VmExtractClass(pVm,"RecursiveCachingIterator",` |
|     - |  3639 | `		sizeof("RecursiveCachingIterator")-1,FALSE,0);` |
|   260 |  3640 | `	return pThis && pCls && PH7_VmInstanceOf(pThis->pClass,pCls);` |
|     2 |  3641 | `}` |
|     - |  3642 | `/*` |
|     - |  3643 | ` * php's spl_caching_it_next: fetch, record, and step the inner iterator on. The` |
|     - |  3644 | ` * ORDER below is php's and is observable — a loud inner iterator sees` |
|     - |  3645 | ` * valid/current/key, then hasChildren/getChildren, then the __toString cast, then` |
|     - |  3646 | ` * next.` |
|     - |  3647 | ` */` |
|   258 |  3648 | `static sxi32 CitFetch(ph7_context *pCtx)` |
|     2 |  3649 | `{` |
|   260 |  3650 | `	ph7_vm *pVm = pCtx->pVm;` |
|   260 |  3651 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   260 |  3652 | `	int bRecursive = CitIsRecursive(pVm,pThis);` |
|     - |  3653 | `	sxi64 iFlags;` |
|     - |  3654 | `	sxi32 rc,rcStr;` |
|     - |  3655 | `	/* php's spl_dual_it_free for this type drops the string and the children with` |
|     - |  3656 | ``	 * the cached pair, which is what makes `(string)$it` empty and hasChildren()`` |
|     - |  3657 | `	 * false once the walk has run off the end. */` |
|     - |  3658 | `	{` |
|   260 |  3659 | `		ph7_value *pStr = PH7_NativeAttr(pThis,CIT_STR);` |
|   260 |  3660 | `		if( pStr ){` |
|   260 |  3661 | `			PH7_MemObjRelease(pStr);` |
|   129 |  3662 | `		}` |
|     - |  3663 | `	}` |
|   260 |  3664 | `	PH7_NativeSetAttrObj(pVm,pThis,CIT_KIDS,0);` |
|   260 |  3665 | `	rc = DualFetch(pVm,pThis,TRUE);` |
|   260 |  3666 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  3667 | `		return rc;` |
|     - |  3668 | `	}` |
|   260 |  3669 | `	iFlags = CitFlags(pThis);` |
|   260 |  3670 | `	if( !DualFilled(pThis) ){` |
|    76 |  3671 | `		PH7_NativeSetAttrInt(pVm,pThis,CIT_FL,iFlags & ~(sxi64)CIT_VALID);` |
|    76 |  3672 | `		return SXRET_OK;` |
|     - |  3673 | `	}` |
|   186 |  3674 | `	PH7_NativeSetAttrInt(pVm,pThis,CIT_FL,iFlags \| CIT_VALID);` |
|   186 |  3675 | `	if( iFlags & CIT_FULL_CACHE ){` |
|    24 |  3676 | `		ph7_value *pCache = CitCacheSlot(pVm,pThis);` |
|    24 |  3677 | `		ph7_value *pKey = PH7_NativeAttr(pThis,IT_CK);` |
|    24 |  3678 | `		ph7_value *pCur = PH7_NativeAttr(pThis,IT_CD);` |
|    24 |  3679 | `		if( pCache && pKey && pCur ){` |
|     - |  3680 | `			/* php's array_set_zval_key: the ordinary array-key rules, an object` |
|     - |  3681 | `			 * key's refusal included. */` |
|    24 |  3682 | `			rc = CitCacheKeyCheck(pCtx,pKey);` |
|    24 |  3683 | `			if( rc != PH7_OK ){` |
|   ! 0 |  3684 | `				return rc;` |
|     - |  3685 | `			}` |
|    24 |  3686 | `			ph7_array_add_elem(pCache,pKey,pCur);` |
|    11 |  3687 | `		}` |
|    11 |  3688 | `	}` |
|   186 |  3689 | `	if( bRecursive ){` |
|     - |  3690 | `		/* php checks EG(exception) here and RETURNS, so a throw from the children` |
|     - |  3691 | `		 * half leaves the inner iterator where it stands. */` |
|   143 |  3692 | `		rc = CitBuildChildren(pCtx,pThis);` |
|   143 |  3693 | `		if( rc != SXRET_OK ){` |
|     5 |  3694 | `			return rc;` |
|     - |  3695 | `		}` |
|    69 |  3696 | `	}` |
|   182 |  3697 | `	rcStr = SXRET_OK;` |
|   182 |  3698 | `	if( iFlags & (CIT_CALL_TOSTRING\|CIT_TOSTRING_USE_INN) ){` |
|    17 |  3699 | `		rcStr = CitMakeString(pCtx,pThis,iFlags);` |
|     8 |  3700 | `	}` |
|     - |  3701 | `	/* php makes no such check after the CAST, so an element with no __toString` |
|     - |  3702 | `	 * throws AND leaves the inner iterator one step on: hasNext() answers from` |
|     - |  3703 | `	 * where the walk really is, not from where the throw interrupted it. */` |
|   182 |  3704 | `	rc = DualNextInnerEx(pVm,pThis,FALSE);` |
|   182 |  3705 | `	return rcStr != SXRET_OK ? rcStr : rc;` |
|   131 |  3706 | `}` |
|   176 |  3707 | `static int CitConstruct(ph7_context *pCtx,const char *zOwner,int nArg,ph7_value **apArg)` |
|     2 |  3708 | `{` |
|   178 |  3709 | `	ph7_vm *pVm = pCtx->pVm;` |
|   178 |  3710 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   178 |  3711 | `	sxi64 iFlags = CIT_CALL_TOSTRING;` |
|     - |  3712 | `	sxi32 rc;` |
|   178 |  3713 | `	if( pThis == 0 ){` |
|   ! 0 |  3714 | `		return PH7_OK;` |
|     - |  3715 | `	}` |
|   178 |  3716 | `	if( nArg > 1 ){` |
|   142 |  3717 | `		iFlags = ph7_value_to_int64(apArg[1]);` |
|    70 |  3718 | `	}` |
|   178 |  3719 | `	if( !CitCheckFlags(iFlags) ){` |
|     4 |  3720 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  3721 | `			"%s::__construct(): Argument #2 ($flags) must contain only one of "` |
|     - |  3722 | `			"CachingIterator::CALL_TOSTRING, CachingIterator::TOSTRING_USE_KEY, "` |
|     - |  3723 | `			"CachingIterator::TOSTRING_USE_CURRENT, or CachingIterator::TOSTRING_USE_INNER",` |
|     1 |  3724 | `			zOwner);` |
|     - |  3725 | `	}` |
|     - |  3726 | `	/* Only the ITERATOR reaches the shared constructor: its second argument is` |
|     - |  3727 | ``	 * IteratorIterator's `$class` downcast, and this one's is an int. */`` |
|   176 |  3728 | `	rc = DualConstruct(pCtx,zOwner,nArg > 0 ? 1 : 0,apArg);` |
|   176 |  3729 | `	if( rc != PH7_OK ){` |
|   ! 0 |  3730 | `		return rc;` |
|     - |  3731 | `	}` |
|   176 |  3732 | `	PH7_NativeSetAttrInt(pVm,pThis,CIT_FL,iFlags & CIT_PUBLIC);` |
|   176 |  3733 | `	return PH7_OK;` |
|    90 |  3734 | `}` |
|    68 |  3735 | `static int vm_builtin_CachingIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  3736 | `{` |
|    70 |  3737 | `	return CitConstruct(pCtx,"CachingIterator",nArg,apArg);` |
|     2 |  3738 | `}` |
|     - |  3739 | `/*` |
|     - |  3740 | `` * php's stub DECLARES `Iterator $iterator` here and its body then asks for`` |
|     - |  3741 | ` * spl_ce_RecursiveIterator, so Reflection reports the looser type while the` |
|     - |  3742 | ` * refusal names the tighter one. Both halves are reproduced: the signature above` |
|     - |  3743 | ` * is the stub's, this check is the body's.` |
|     - |  3744 | ` */` |
|   118 |  3745 | `static int vm_builtin_RecursiveCachingIterator_construct(ph7_context *pCtx,int nArg,` |
|     - |  3746 | `	ph7_value **apArg)` |
|     1 |  3747 | `{` |
|   119 |  3748 | `	if( nArg > 0 ){` |
|   119 |  3749 | `		ph7_class *pRec = PH7_VmExtractClass(pCtx->pVm,"RecursiveIterator",` |
|     - |  3750 | `			sizeof("RecursiveIterator")-1,FALSE,0);` |
|   178 |  3751 | `		ph7_class_instance *pObj = (apArg[0]->iFlags & MEMOBJ_OBJ)` |
|   116 |  3752 | `			? (ph7_class_instance *)apArg[0]->x.pOther : 0;` |
|   119 |  3753 | `		if( pRec && (pObj == 0 \|\| !PH7_VmInstanceOf(pObj->pClass,pRec)) ){` |
|     - |  3754 | `			char zBuf[64];` |
|    16 |  3755 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  3756 | `				"RecursiveCachingIterator::__construct(): Argument #1 ($iterator) must be "` |
|     - |  3757 | `				"of type RecursiveIterator, %s given",` |
|     5 |  3758 | `				VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));` |
|     - |  3759 | `		}` |
|    54 |  3760 | `	}` |
|   109 |  3761 | `	return CitConstruct(pCtx,"RecursiveCachingIterator",nArg,apArg);` |
|    60 |  3762 | `}` |
|   118 |  3763 | `static int vm_builtin_CachingIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  3764 | `{` |
|   120 |  3765 | `	ph7_vm *pVm = pCtx->pVm;` |
|   120 |  3766 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3767 | `	ph7_value *pCache;` |
|     - |  3768 | `	sxi32 rc;` |
|    59 |  3769 | `	SXUNUSED(nArg);` |
|    59 |  3770 | `	SXUNUSED(apArg);` |
|   120 |  3771 | `	if( !DualReady(pThis) ){` |
|     3 |  3772 | `		return DualNotReady(pCtx);` |
|     - |  3773 | `	}` |
|   118 |  3774 | `	pCache = PH7_NativeAttr(pThis,CIT_CCH);` |
|   118 |  3775 | `	if( pCache ){` |
|     - |  3776 | `		/* php's zend_hash_clean: a rewind starts the cache over. */` |
|   118 |  3777 | `		PH7_MemObjRelease(pCache);` |
|   118 |  3778 | `		PH7_MemObjToHashmap(pCache);` |
|    58 |  3779 | `	}` |
|   118 |  3780 | `	rc = DualRewindInner(pVm,pThis);` |
|   118 |  3781 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  3782 | `		return rc;` |
|     - |  3783 | `	}` |
|   118 |  3784 | `	rc = CitFetch(pCtx);` |
|   118 |  3785 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    61 |  3786 | `}` |
|   144 |  3787 | `static int vm_builtin_CachingIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  3788 | `{` |
|   146 |  3789 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3790 | `	sxi32 rc;` |
|    72 |  3791 | `	SXUNUSED(nArg);` |
|    72 |  3792 | `	SXUNUSED(apArg);` |
|   146 |  3793 | `	if( !DualReady(pThis) ){` |
|     3 |  3794 | `		return DualNotReady(pCtx);` |
|     - |  3795 | `	}` |
|   144 |  3796 | `	rc = CitFetch(pCtx);` |
|   144 |  3797 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    74 |  3798 | `}` |
|     - |  3799 | `/* valid() is the private CIT_VALID bit, not the inner iterator's answer: the` |
|     - |  3800 | ` * decorator stands on what it fetched and the inner has already moved past it. */` |
|   344 |  3801 | `static int vm_builtin_CachingIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  3802 | `{` |
|   346 |  3803 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   172 |  3804 | `	SXUNUSED(nArg);` |
|   172 |  3805 | `	SXUNUSED(apArg);` |
|   346 |  3806 | `	if( !DualReady(pThis) ){` |
|     3 |  3807 | `		return DualNotReady(pCtx);` |
|     - |  3808 | `	}` |
|   344 |  3809 | `	ph7_result_bool(pCtx,(CitFlags(pThis) & CIT_VALID) != 0);` |
|   344 |  3810 | `	return PH7_OK;` |
|   174 |  3811 | `}` |
|     - |  3812 | `/* hasNext() is the inner iterator's LIVE valid(), which is why moving the inner` |
|     - |  3813 | ` * behind the decorator's back changes the answer. */` |
|   194 |  3814 | `static int vm_builtin_CachingIterator_hasNext(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3815 | `{` |
|   195 |  3816 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   195 |  3817 | `	int bValid = 0;` |
|     - |  3818 | `	sxi32 rc;` |
|    97 |  3819 | `	SXUNUSED(nArg);` |
|    97 |  3820 | `	SXUNUSED(apArg);` |
|   195 |  3821 | `	if( !DualReady(pThis) ){` |
|     3 |  3822 | `		return DualNotReady(pCtx);` |
|     - |  3823 | `	}` |
|   193 |  3824 | `	rc = DualInnerValid(pCtx->pVm,pThis,&bValid);` |
|   193 |  3825 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  3826 | `		return rc;` |
|     - |  3827 | `	}` |
|   193 |  3828 | `	ph7_result_bool(pCtx,bValid);` |
|   193 |  3829 | `	return PH7_OK;` |
|    98 |  3830 | `}` |
|    22 |  3831 | `static int vm_builtin_CachingIterator_toString(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3832 | `{` |
|    23 |  3833 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3834 | `	sxi64 iFlags;` |
|     - |  3835 | `	ph7_value sOut,*pSrc;` |
|     - |  3836 | `	sxi32 rc;` |
|    11 |  3837 | `	SXUNUSED(nArg);` |
|    11 |  3838 | `	SXUNUSED(apArg);` |
|    23 |  3839 | `	if( !DualReady(pThis) ){` |
|     3 |  3840 | `		return DualNotReady(pCtx);` |
|     - |  3841 | `	}` |
|    21 |  3842 | `	iFlags = CitFlags(pThis);` |
|    20 |  3843 | `	if( (iFlags & (CIT_CALL_TOSTRING\|CIT_TOSTRING_USE_KEY\|CIT_TOSTRING_USE_CUR` |
|    11 |  3844 | `		\|CIT_TOSTRING_USE_INN)) == 0 ){` |
|     5 |  3845 | `		return CitRefuse(pCtx,pThis,"fetch string value");` |
|     - |  3846 | `	}` |
|    17 |  3847 | `	if( iFlags & (CIT_TOSTRING_USE_KEY\|CIT_TOSTRING_USE_CUR) ){` |
|     - |  3848 | `		/* Read out of the CACHE at call time, converted then and there. */` |
|     5 |  3849 | `		pSrc = PH7_NativeAttr(pThis,(iFlags & CIT_TOSTRING_USE_KEY) ? IT_CK : IT_CD);` |
|     5 |  3850 | `		PH7_MemObjInit(pCtx->pVm,&sOut);` |
|     5 |  3851 | `		if( pSrc ){` |
|     5 |  3852 | `			PH7_MemObjStore(pSrc,&sOut);` |
|     2 |  3853 | `		}` |
|     5 |  3854 | `		rc = PH7_MemObjToStringUV(&sOut);` |
|     5 |  3855 | `		if( rc == SXRET_OK ){` |
|     5 |  3856 | `			ph7_result_value(pCtx,&sOut);` |
|     2 |  3857 | `		}` |
|     5 |  3858 | `		PH7_MemObjRelease(&sOut);` |
|     5 |  3859 | `		return rc == SXRET_OK ? PH7_OK : rc;` |
|     - |  3860 | `	}` |
|    13 |  3861 | `	pSrc = PH7_NativeAttr(pThis,CIT_STR);` |
|    13 |  3862 | `	if( pSrc && (pSrc->iFlags & MEMOBJ_STRING) ){` |
|    11 |  3863 | `		ph7_result_value(pCtx,pSrc);` |
|     6 |  3864 | `	}else{` |
|     - |  3865 | ``		/* php's `zstr is not a string` — nothing has been fetched. */`` |
|     3 |  3866 | `		ph7_result_string(pCtx,"",0);` |
|     - |  3867 | `	}` |
|    13 |  3868 | `	return PH7_OK;` |
|    12 |  3869 | `}` |
|    32 |  3870 | `static int vm_builtin_CachingIterator_getFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3871 | `{` |
|    33 |  3872 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    16 |  3873 | `	SXUNUSED(nArg);` |
|    16 |  3874 | `	SXUNUSED(apArg);` |
|    33 |  3875 | `	if( !DualReady(pThis) ){` |
|     3 |  3876 | `		return DualNotReady(pCtx);` |
|     - |  3877 | `	}` |
|    31 |  3878 | `	ph7_result_int64(pCtx,CitFlags(pThis));` |
|    31 |  3879 | `	return PH7_OK;` |
|    17 |  3880 | `}` |
|    10 |  3881 | `static int vm_builtin_CachingIterator_setFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3882 | `{` |
|    11 |  3883 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  3884 | `	sxi64 iOld,iNew;` |
|    11 |  3885 | `	if( nArg < 1 ){` |
|   ! 0 |  3886 | `		return PH7_OK;` |
|     - |  3887 | `	}` |
|    11 |  3888 | `	iNew = ph7_value_to_int64(apArg[0]);` |
|    11 |  3889 | `	if( !CitCheckFlags(iNew) ){` |
|     3 |  3890 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  3891 | `			"CachingIterator::setFlags(): Argument #1 ($flags) must contain only one of "` |
|     - |  3892 | `			"CachingIterator::CALL_TOSTRING, CachingIterator::TOSTRING_USE_KEY, "` |
|     - |  3893 | `			"CachingIterator::TOSTRING_USE_CURRENT, or CachingIterator::TOSTRING_USE_INNER");` |
|     - |  3894 | `	}` |
|     9 |  3895 | `	if( !DualReady(pThis) ){` |
|   ! 0 |  3896 | `		return DualNotReady(pCtx);` |
|     - |  3897 | `	}` |
|     - |  3898 | `	/* The two eager spellings are computed at FETCH time, so php refuses to turn` |
|     - |  3899 | `	 * either off mid-walk rather than leaving a stale string behind. */` |
|     9 |  3900 | `	iOld = CitFlags(pThis);` |
|     9 |  3901 | `	if( (iOld & CIT_CALL_TOSTRING) != 0 && (iNew & CIT_CALL_TOSTRING) == 0 ){` |
|     5 |  3902 | `		return PH7_VmThrowException(pCtx,"InvalidArgumentException",` |
|     - |  3903 | `			"Unsetting flag CALL_TO_STRING is not possible");` |
|     - |  3904 | `	}` |
|     5 |  3905 | `	if( (iOld & CIT_TOSTRING_USE_INN) != 0 && (iNew & CIT_TOSTRING_USE_INN) == 0 ){` |
|     3 |  3906 | `		return PH7_VmThrowException(pCtx,"InvalidArgumentException",` |
|     - |  3907 | `			"Unsetting flag TOSTRING_USE_INNER is not possible");` |
|     - |  3908 | `	}` |
|     3 |  3909 | `	PH7_NativeSetAttrInt(pCtx->pVm,pThis,CIT_FL,(iOld & ~(sxi64)CIT_PUBLIC) \| (iNew & CIT_PUBLIC));` |
|     3 |  3910 | `	return PH7_OK;` |
|     6 |  3911 | `}` |
|    16 |  3912 | `static int vm_builtin_CachingIterator_getCache(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  3913 | `{` |
|     - |  3914 | `	ph7_value *pCache;` |
|     - |  3915 | `	int rc;` |
|     8 |  3916 | `	SXUNUSED(nArg);` |
|     8 |  3917 | `	SXUNUSED(apArg);` |
|    18 |  3918 | `	pCache = CitCacheChecked(pCtx,&rc);` |
|    18 |  3919 | `	if( pCache == 0 ){` |
|     5 |  3920 | `		return rc;` |
|     - |  3921 | `	}` |
|    14 |  3922 | `	ph7_result_value(pCtx,pCache);` |
|    14 |  3923 | `	return PH7_OK;` |
|    10 |  3924 | `}` |
|    14 |  3925 | `static int vm_builtin_CachingIterator_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  3926 | `{` |
|     - |  3927 | `	ph7_value *pCache;` |
|     - |  3928 | `	int rc;` |
|     7 |  3929 | `	SXUNUSED(nArg);` |
|     7 |  3930 | `	SXUNUSED(apArg);` |
|    15 |  3931 | `	pCache = CitCacheChecked(pCtx,&rc);` |
|    15 |  3932 | `	if( pCache == 0 ){` |
|     5 |  3933 | `		return rc;` |
|     - |  3934 | `	}` |
|    11 |  3935 | `	ph7_result_int64(pCtx,pCache->x.pOther ? ((ph7_hashmap *)pCache->x.pOther)->nEntry : 0);` |
|    11 |  3936 | `	return PH7_OK;` |
|     8 |  3937 | `}` |
|     - |  3938 | `/*` |
|     - |  3939 | `` * The four ArrayAccess members read and write that same cache. php's `$key` is`` |
|     - |  3940 | ` * DECLARED untyped and screened as a string by the body, so a non-stringable key` |
|     - |  3941 | `` * is a TypeError naming `string` while an int or a float becomes an array key the`` |
|     - |  3942 | ` * ordinary way.` |
|     - |  3943 | ` */` |
|    32 |  3944 | `static ph7_value * CitOffsetKey(ph7_context *pCtx,const char *zMethod,ph7_value *pKey,` |
|     - |  3945 | `	ph7_value *pOut,int *pRc)` |
|     2 |  3946 | `{` |
|     - |  3947 | `	char zBuf[64];` |
|    34 |  3948 | `	*pRc = PH7_OK;` |
|    34 |  3949 | `	if( !PH7_ArgSatisfiesString(pKey) ){` |
|    20 |  3950 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  3951 | `			"CachingIterator::%s(): Argument #1 ($key) must be of type string, %s given",` |
|     6 |  3952 | `			zMethod,VmValueGivenName(pKey,zBuf,sizeof(zBuf)));` |
|    14 |  3953 | `		return 0;` |
|     - |  3954 | `	}` |
|    22 |  3955 | `	PH7_MemObjInit(pCtx->pVm,pOut);` |
|    22 |  3956 | `	PH7_MemObjStore(pKey,pOut);` |
|    22 |  3957 | `	if( PH7_MemObjToString(pOut) != SXRET_OK ){` |
|   ! 0 |  3958 | `		PH7_MemObjRelease(pOut);` |
|   ! 0 |  3959 | `		*pRc = PH7_OK;` |
|   ! 0 |  3960 | `		return 0;` |
|     - |  3961 | `	}` |
|    22 |  3962 | `	return pOut;` |
|    18 |  3963 | `}` |
|    16 |  3964 | `static int vm_builtin_CachingIterator_offsetGet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  3965 | `{` |
|     - |  3966 | `	ph7_value *pCache,*pKey,sKey;` |
|    18 |  3967 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  3968 | `	int rc;` |
|    18 |  3969 | `	pCache = CitCacheChecked(pCtx,&rc);` |
|    18 |  3970 | `	if( pCache == 0 ){` |
|     3 |  3971 | `		return rc;` |
|     - |  3972 | `	}` |
|    16 |  3973 | `	if( nArg < 1 ){` |
|   ! 0 |  3974 | `		return PH7_OK;` |
|     - |  3975 | `	}` |
|    16 |  3976 | `	pKey = CitOffsetKey(pCtx,"offsetGet",apArg[0],&sKey,&rc);` |
|    16 |  3977 | `	if( pKey == 0 ){` |
|     6 |  3978 | `		return rc;` |
|     - |  3979 | `	}` |
|    11 |  3980 | `	if( PH7_HashmapLookup((ph7_hashmap *)pCache->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|     5 |  3981 | `		ph7_value *pVal = HashmapExtractNodeValue(pNode);` |
|     5 |  3982 | `		if( pVal ){` |
|     5 |  3983 | `			ph7_result_value(pCtx,pVal);` |
|     2 |  3984 | `		}` |
|     3 |  3985 | `	}else{` |
|     - |  3986 | `		/* php reads the cache as an ARRAY here, warning included — but it has already` |
|     - |  3987 | `		 * cast the key to a STRING, so the key is QUOTED even where a plain array read` |
|     - |  3988 | ``		 * would print a bare integer ($c[0] on a missing key says `"0"`). */`` |
|     - |  3989 | `		SyBlob sMsg;` |
|     - |  3990 | `		SyString sKeyText;` |
|     7 |  3991 | `		int nKey = 0;` |
|     7 |  3992 | `		const char *zKey = ph7_value_to_string(pKey,&nKey);` |
|     7 |  3993 | `		SyStringInitFromBuf(&sKeyText,zKey,(sxu32)nKey);` |
|     7 |  3994 | `		SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);` |
|     7 |  3995 | `		SyBlobFormat(&sMsg,"Undefined array key \"%z\"",&sKeyText);` |
|     7 |  3996 | `		SyBlobNullAppend(&sMsg);` |
|     7 |  3997 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));` |
|     7 |  3998 | `		SyBlobRelease(&sMsg);` |
|     - |  3999 | `	}` |
|    11 |  4000 | `	PH7_MemObjRelease(&sKey);` |
|    11 |  4001 | `	return PH7_OK;` |
|    10 |  4002 | `}` |
|    10 |  4003 | `static int vm_builtin_CachingIterator_offsetExists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  4004 | `{` |
|     - |  4005 | `	ph7_value *pCache,*pKey,sKey;` |
|    12 |  4006 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  4007 | `	int rc;` |
|    12 |  4008 | `	pCache = CitCacheChecked(pCtx,&rc);` |
|    12 |  4009 | `	if( pCache == 0 ){` |
|     3 |  4010 | `		return rc;` |
|     - |  4011 | `	}` |
|    10 |  4012 | `	if( nArg < 1 ){` |
|   ! 0 |  4013 | `		return PH7_OK;` |
|     - |  4014 | `	}` |
|    10 |  4015 | `	pKey = CitOffsetKey(pCtx,"offsetExists",apArg[0],&sKey,&rc);` |
|    10 |  4016 | `	if( pKey == 0 ){` |
|     3 |  4017 | `		return rc;` |
|     - |  4018 | `	}` |
|    11 |  4019 | `	ph7_result_bool(pCtx,` |
|     6 |  4020 | `		PH7_HashmapLookup((ph7_hashmap *)pCache->x.pOther,pKey,&pNode) == SXRET_OK);` |
|     8 |  4021 | `	PH7_MemObjRelease(&sKey);` |
|     8 |  4022 | `	return PH7_OK;` |
|     7 |  4023 | `}` |
|     8 |  4024 | `static int vm_builtin_CachingIterator_offsetSet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  4025 | `{` |
|     - |  4026 | `	ph7_value *pCache,*pKey,sKey;` |
|     - |  4027 | `	int rc;` |
|    10 |  4028 | `	pCache = CitCacheChecked(pCtx,&rc);` |
|    10 |  4029 | `	if( pCache == 0 ){` |
|     3 |  4030 | `		return rc;` |
|     - |  4031 | `	}` |
|     8 |  4032 | `	if( nArg < 2 ){` |
|   ! 0 |  4033 | `		return PH7_OK;` |
|     - |  4034 | `	}` |
|     8 |  4035 | `	pKey = CitOffsetKey(pCtx,"offsetSet",apArg[0],&sKey,&rc);` |
|     8 |  4036 | `	if( pKey == 0 ){` |
|     5 |  4037 | `		return rc;` |
|     - |  4038 | `	}` |
|     3 |  4039 | `	ph7_array_add_elem(pCache,pKey,apArg[1]);` |
|     3 |  4040 | `	PH7_MemObjRelease(&sKey);` |
|     3 |  4041 | `	return PH7_OK;` |
|     6 |  4042 | `}` |
|     6 |  4043 | `static int vm_builtin_CachingIterator_offsetUnset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  4044 | `{` |
|     - |  4045 | `	ph7_value *pCache,*pKey,sKey;` |
|     8 |  4046 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  4047 | `	int rc;` |
|     8 |  4048 | `	pCache = CitCacheChecked(pCtx,&rc);` |
|     8 |  4049 | `	if( pCache == 0 ){` |
|     3 |  4050 | `		return rc;` |
|     - |  4051 | `	}` |
|     6 |  4052 | `	if( nArg < 1 ){` |
|   ! 0 |  4053 | `		return PH7_OK;` |
|     - |  4054 | `	}` |
|     6 |  4055 | `	pKey = CitOffsetKey(pCtx,"offsetUnset",apArg[0],&sKey,&rc);` |
|     6 |  4056 | `	if( pKey == 0 ){` |
|     3 |  4057 | `		return rc;` |
|     - |  4058 | `	}` |
|     3 |  4059 | `	if( PH7_HashmapLookup((ph7_hashmap *)pCache->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|     3 |  4060 | `		PH7_HashmapUnlinkNode(pNode,TRUE);` |
|     1 |  4061 | `	}` |
|     3 |  4062 | `	PH7_MemObjRelease(&sKey);` |
|     3 |  4063 | `	return PH7_OK;` |
|     5 |  4064 | `}` |
|     - |  4065 | `/* The recursive twin answers what the FETCH built and nothing else: no children` |
|     - |  4066 | ` * means the fetch found none, and two calls hand back the SAME object. */` |
|   130 |  4067 | `static int vm_builtin_RecursiveCachingIterator_hasChildren(ph7_context *pCtx,int nArg,` |
|     - |  4068 | `	ph7_value **apArg)` |
|     1 |  4069 | `{` |
|   131 |  4070 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    65 |  4071 | `	SXUNUSED(nArg);` |
|    65 |  4072 | `	SXUNUSED(apArg);` |
|   131 |  4073 | `	if( !DualReady(pThis) ){` |
|     3 |  4074 | `		return DualNotReady(pCtx);` |
|     - |  4075 | `	}` |
|   129 |  4076 | `	ph7_result_bool(pCtx,PH7_NativeAttrObj(pThis,CIT_KIDS) != 0);` |
|   129 |  4077 | `	return PH7_OK;` |
|    66 |  4078 | `}` |
|    48 |  4079 | `static int vm_builtin_RecursiveCachingIterator_getChildren(ph7_context *pCtx,int nArg,` |
|     - |  4080 | `	ph7_value **apArg)` |
|     1 |  4081 | `{` |
|    49 |  4082 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  4083 | `	ph7_class_instance *pKids;` |
|    24 |  4084 | `	SXUNUSED(nArg);` |
|    24 |  4085 | `	SXUNUSED(apArg);` |
|    49 |  4086 | `	if( !DualReady(pThis) ){` |
|   ! 0 |  4087 | `		return DualNotReady(pCtx);` |
|     - |  4088 | `	}` |
|    49 |  4089 | `	pKids = PH7_NativeAttrObj(pThis,CIT_KIDS);` |
|    49 |  4090 | `	if( pKids ){` |
|    45 |  4091 | `		SplResultBorrowed(pCtx,pKids);` |
|    23 |  4092 | `	}else{` |
|     5 |  4093 | `		ph7_result_null(pCtx);` |
|     - |  4094 | `	}` |
|    49 |  4095 | `	return PH7_OK;` |
|    25 |  4096 | `}` |
|     - |  4097 | `/*` |
|     - |  4098 | ` * The declarations. php's method ORDER is the order Reflection reports, so each` |
|     - |  4099 | ` * table follows spl_iterators.stub.php line for line; the parameter types are the` |
|     - |  4100 | `` * stub's too, which is what makes `Iterator $iterator` refuse an IteratorAggregate`` |
|     - |  4101 | ` * everywhere except IteratorIterator (the one class that declares Traversable and` |
|     - |  4102 | ` * unwraps).` |
|     - |  4103 | ` *` |
|     - |  4104 | ` * No RETURN type is declared, on purpose: php marks every one of these` |
|     - |  4105 | `` * `@tentative-return-type`, and a tentative type answers NULL from getReturnType()`` |
|     - |  4106 | ` * and false from hasReturnType() — which is exactly what an undeclared zRet answers` |
|     - |  4107 | `` * here. Declaring them would print `Return [ bool ]` where php prints`` |
|     - |  4108 | `` * `Tentative return [ bool ]` AND make getReturnType() disagree; leaving them off`` |
|     - |  4109 | ` * costs only getTentativeReturnType(). PHL has no tentative-return concept at all` |
|     - |  4110 | ` * (§7.4) — DateTime and the reflectors already report a plain return type where php` |
|     - |  4111 | ` * reports a tentative one.` |
|     - |  4112 | ` */` |
|  5740 |  4113 | `static sxi32 VmInstallSplDualIterators(ph7_vm *pVm)` |
|     5 |  4114 | `{` |
|     - |  4115 | `	static const PH7_NativePropDef aDualProp[] = {` |
|     - |  4116 | `		{ IT_IN, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  4117 | `		{ IT_IT, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  4118 | `		{ IT_CD, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  4119 | `		{ IT_CK, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  4120 | `		{ IT_CF, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  4121 | `		{ IT_CP, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  4122 | `	};` |
|     - |  4123 | `	static const PH7_NativePropDef aLimitProp[] = {` |
|     - |  4124 | `		{ IT_OFF, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  4125 | `		{ IT_LIM, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, -1, 0, 0.0 }, 0 },` |
|     - |  4126 | `	};` |
|     - |  4127 | `	static const PH7_NativePropDef aCbProp[] = {` |
|     - |  4128 | `		{ IT_CB, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  4129 | `	};` |
|     - |  4130 | `	static const PH7_NativeMethodDef aOuterMethod[] = {` |
|     - |  4131 | `		{ "getInnerIterator", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", 0, 0 },` |
|     - |  4132 | `	};` |
|     - |  4133 | `	static const PH7_NativeMethodDef aIterIterMethod[] = {` |
|     - |  4134 | `		{ "__construct",      PH7_MOD_PUBLIC, "Traversable $iterator, ?string $class = null", 0,` |
|     - |  4135 | `		  vm_builtin_IteratorIterator_construct },` |
|     - |  4136 | `		{ "getInnerIterator", PH7_MOD_PUBLIC, "", "@?Iterator", vm_builtin_Dual_getInnerIterator },` |
|     - |  4137 | `		{ "rewind",           PH7_MOD_PUBLIC, "", "@void", vm_builtin_IteratorIterator_rewind },` |
|     - |  4138 | `		{ "valid",            PH7_MOD_PUBLIC, "", "@bool", vm_builtin_Dual_valid },` |
|     - |  4139 | `		{ "key",              PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_Dual_key },` |
|     - |  4140 | `		{ "current",          PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_Dual_current },` |
|     - |  4141 | `		{ "next",             PH7_MOD_PUBLIC, "", "@void", vm_builtin_IteratorIterator_next },` |
|     - |  4142 | `	};` |
|     - |  4143 | `	static const PH7_NativeMethodDef aFilterMethod[] = {` |
|     - |  4144 | `		{ "accept",      PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@bool", 0 },` |
|     - |  4145 | `		{ "__construct", PH7_MOD_PUBLIC, "Iterator $iterator", 0, vm_builtin_FilterIterator_construct },` |
|     - |  4146 | `		{ "rewind",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_FilterIterator_rewind },` |
|     - |  4147 | `		{ "next",        PH7_MOD_PUBLIC, "", "@void", vm_builtin_FilterIterator_next },` |
|     - |  4148 | `	};` |
|     - |  4149 | `	static const PH7_NativeMethodDef aCbFilterMethod[] = {` |
|     - |  4150 | `		{ "__construct", PH7_MOD_PUBLIC, "Iterator $iterator, callable $callback", 0,` |
|     - |  4151 | `		  vm_builtin_CallbackFilterIterator_construct },` |
|     - |  4152 | `		{ "accept",      PH7_MOD_PUBLIC, "", "@bool", vm_builtin_CallbackFilterIterator_accept },` |
|     - |  4153 | `	};` |
|     - |  4154 | `	static const PH7_NativeMethodDef aLimitMethod[] = {` |
|     - |  4155 | `		{ "__construct", PH7_MOD_PUBLIC, "Iterator $iterator, int $offset = 0, int $limit = -1", 0,` |
|     - |  4156 | `		  vm_builtin_LimitIterator_construct },` |
|     - |  4157 | `		{ "rewind",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_LimitIterator_rewind },` |
|     - |  4158 | `		{ "valid",       PH7_MOD_PUBLIC, "", "@bool", vm_builtin_LimitIterator_valid },` |
|     - |  4159 | `		{ "next",        PH7_MOD_PUBLIC, "", "@void", vm_builtin_LimitIterator_next },` |
|     - |  4160 | `		{ "seek",        PH7_MOD_PUBLIC, "int $offset", "@int", vm_builtin_LimitIterator_seek },` |
|     - |  4161 | `		{ "getPosition", PH7_MOD_PUBLIC, "", "@int", vm_builtin_LimitIterator_getPosition },` |
|     - |  4162 | `	};` |
|     - |  4163 | `	static const PH7_NativeMethodDef aInfiniteMethod[] = {` |
|     - |  4164 | `		{ "__construct", PH7_MOD_PUBLIC, "Iterator $iterator", 0,` |
|     - |  4165 | `		  vm_builtin_InfiniteIterator_construct },` |
|     - |  4166 | `		{ "next",        PH7_MOD_PUBLIC, "", "@void", vm_builtin_InfiniteIterator_next },` |
|     - |  4167 | `	};` |
|     - |  4168 | `	static const PH7_NativeMethodDef aNoRewindMethod[] = {` |
|     - |  4169 | `		{ "__construct", PH7_MOD_PUBLIC, "Iterator $iterator", 0,` |
|     - |  4170 | `		  vm_builtin_NoRewindIterator_construct },` |
|     - |  4171 | `		{ "rewind",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_NoRewindIterator_rewind },` |
|     - |  4172 | `		{ "valid",       PH7_MOD_PUBLIC, "", "@bool", vm_builtin_NoRewindIterator_valid },` |
|     - |  4173 | `		{ "key",         PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_NoRewindIterator_key },` |
|     - |  4174 | `		{ "current",     PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_NoRewindIterator_current },` |
|     - |  4175 | `		{ "next",        PH7_MOD_PUBLIC, "", "@void", vm_builtin_NoRewindIterator_next },` |
|     - |  4176 | `	};` |
|     - |  4177 | `	static const PH7_NativePropDef aRegexProp[] = {` |
|     - |  4178 | `		/* The one slot php PRESENTS, declared as php declares it: a ?string, so a` |
|     - |  4179 | ``		 * `$it->replacement = 5` coerces and an array is a TypeError. */`` |
|     - |  4180 | `		{ "replacement", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?string" },` |
|     - |  4181 | `		{ IT_RE, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|     - |  4182 | `		{ IT_RM, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  4183 | `		{ IT_RF, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  4184 | `		{ IT_RP, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  4185 | `	};` |
|     - |  4186 | `	static const PH7_NativeConstDef aRegexConst[] = {` |
|     - |  4187 | `		{ "USE_KEY",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, REGIT_USE_KEY,  0, 0.0 },` |
|     - |  4188 | `		{ "INVERT_MATCH", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, REGIT_INVERTED, 0, 0.0 },` |
|     - |  4189 | `		{ "MATCH",        PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PH7_REGIT_MATCH,       0, 0.0 },` |
|     - |  4190 | `		{ "GET_MATCH",    PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PH7_REGIT_GET_MATCH,   0, 0.0 },` |
|     - |  4191 | `		{ "ALL_MATCHES",  PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PH7_REGIT_ALL_MATCHES, 0, 0.0 },` |
|     - |  4192 | `		{ "SPLIT",        PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PH7_REGIT_SPLIT,       0, 0.0 },` |
|     - |  4193 | `		{ "REPLACE",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PH7_REGIT_REPLACE,     0, 0.0 },` |
|     - |  4194 | `	};` |
|     - |  4195 | `	static const PH7_NativeMethodDef aRegexMethod[] = {` |
|     - |  4196 | `		{ "__construct",  PH7_MOD_PUBLIC,` |
|     - |  4197 | ``		  /* php's stub spells this default `RegexIterator::MATCH`, and one zSig field`` |
|     - |  4198 | `		   * cannot say both the TEXT and the VALUE: the constant spelling prints php's` |
|     - |  4199 | `		   * export line but makes getDefaultValue() a "Failed to retrieve" throw, so the` |
|     - |  4200 | `		   * VALUE wins here, as it does in the aBuiltinSig rows with the same shape. */` |
|     - |  4201 | `		  "Iterator $iterator, string $pattern, int $mode = 0, int $flags = 0, int $pregFlags = 0", 0,` |
|     - |  4202 | `		  vm_builtin_RegexIterator_construct },` |
|     - |  4203 | `		{ "accept",       PH7_MOD_PUBLIC, "", "@bool", vm_builtin_RegexIterator_accept },` |
|     - |  4204 | `		{ "getMode",      PH7_MOD_PUBLIC, "", "@int", vm_builtin_RegexIterator_getMode },` |
|     - |  4205 | `		{ "setMode",      PH7_MOD_PUBLIC, "int $mode", "@void", vm_builtin_RegexIterator_setMode },` |
|     - |  4206 | `		{ "getFlags",     PH7_MOD_PUBLIC, "", "@int", vm_builtin_RegexIterator_getFlags },` |
|     - |  4207 | `		{ "setFlags",     PH7_MOD_PUBLIC, "int $flags", "@void", vm_builtin_RegexIterator_setFlags },` |
|     - |  4208 | `		{ "getRegex",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_RegexIterator_getRegex },` |
|     - |  4209 | `		{ "getPregFlags", PH7_MOD_PUBLIC, "", "@int", vm_builtin_RegexIterator_getPregFlags },` |
|     - |  4210 | `		{ "setPregFlags", PH7_MOD_PUBLIC, "int $pregFlags", "@void", vm_builtin_RegexIterator_setPregFlags },` |
|     - |  4211 | `	};` |
|     - |  4212 | `	static const PH7_NativeMethodDef aRecursiveMethod[] = {` |
|     - |  4213 | `		{ "hasChildren", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", 0, 0 },` |
|     - |  4214 | `		{ "getChildren", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", 0, 0 },` |
|     - |  4215 | `	};` |
|     - |  4216 | `	static const PH7_NativeConstDef aRaiConst[] = {` |
|     - |  4217 | `		{ "CHILD_ARRAYS_ONLY", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RAI_CHILD_ARRAYS_ONLY, 0, 0.0 },` |
|     - |  4218 | `	};` |
|     - |  4219 | `	static const PH7_NativeMethodDef aRaiMethod[] = {` |
|     - |  4220 | `		{ "hasChildren", PH7_MOD_PUBLIC, "", "@bool",` |
|     - |  4221 | `		  vm_builtin_RecursiveArrayIterator_hasChildren },` |
|     - |  4222 | `		{ "getChildren", PH7_MOD_PUBLIC, "", "@?RecursiveArrayIterator",` |
|     - |  4223 | `		  vm_builtin_RecursiveArrayIterator_getChildren },` |
|     - |  4224 | `	};` |
|     - |  4225 | `	static const PH7_NativeMethodDef aRfiMethod[] = {` |
|     - |  4226 | `		{ "__construct", PH7_MOD_PUBLIC, "RecursiveIterator $iterator", 0,` |
|     - |  4227 | `		  vm_builtin_RecursiveFilterIterator_construct },` |
|     - |  4228 | `		{ "hasChildren", PH7_MOD_PUBLIC, "", "@bool",` |
|     - |  4229 | `		  vm_builtin_RecursiveFilterIterator_hasChildren },` |
|     - |  4230 | `		{ "getChildren", PH7_MOD_PUBLIC, "", "@?RecursiveFilterIterator",` |
|     - |  4231 | `		  vm_builtin_RecursiveFilterIterator_getChildren },` |
|     - |  4232 | `	};` |
|     - |  4233 | `	static const PH7_NativeMethodDef aParentMethod[] = {` |
|     - |  4234 | `		{ "__construct", PH7_MOD_PUBLIC, "RecursiveIterator $iterator", 0,` |
|     - |  4235 | `		  vm_builtin_ParentIterator_construct },` |
|     - |  4236 | `		{ "accept",      PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ParentIterator_accept },` |
|     - |  4237 | `	};` |
|     - |  4238 | `	static const PH7_NativeMethodDef aRcbfMethod[] = {` |
|     - |  4239 | `		{ "__construct", PH7_MOD_PUBLIC, "RecursiveIterator $iterator, callable $callback", 0,` |
|     - |  4240 | `		  vm_builtin_RecursiveCallbackFilterIterator_construct },` |
|     - |  4241 | `		{ "hasChildren", PH7_MOD_PUBLIC, "", "@bool",` |
|     - |  4242 | `		  vm_builtin_RecursiveFilterIterator_hasChildren },` |
|     - |  4243 | `		{ "getChildren", PH7_MOD_PUBLIC, "", "@?RecursiveCallbackFilterIterator",` |
|     - |  4244 | `		  vm_builtin_RecursiveCallbackFilterIterator_getChildren },` |
|     - |  4245 | `	};` |
|     - |  4246 | `	static const PH7_NativeMethodDef aRregexMethod[] = {` |
|     - |  4247 | `		{ "__construct", PH7_MOD_PUBLIC,` |
|     - |  4248 | `		  "RecursiveIterator $iterator, string $pattern, int $mode = 0, int $flags = 0, "` |
|     - |  4249 | `		  "int $pregFlags = 0", 0,` |
|     - |  4250 | `		  vm_builtin_RecursiveRegexIterator_construct },` |
|     - |  4251 | `		{ "accept",      PH7_MOD_PUBLIC, "", "@bool", vm_builtin_RecursiveRegexIterator_accept },` |
|     - |  4252 | `		{ "hasChildren", PH7_MOD_PUBLIC, "", "@bool",` |
|     - |  4253 | `		  vm_builtin_RecursiveFilterIterator_hasChildren },` |
|     - |  4254 | `		{ "getChildren", PH7_MOD_PUBLIC, "", "@?RecursiveRegexIterator",` |
|     - |  4255 | `		  vm_builtin_RecursiveRegexIterator_getChildren },` |
|     - |  4256 | `	};` |
|     - |  4257 | `	static const PH7_NativeConstDef aCitConst[] = {` |
|     - |  4258 | `		{ "CALL_TOSTRING",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, CIT_CALL_TOSTRING, 0, 0.0 },` |
|     - |  4259 | `		{ "CATCH_GET_CHILD",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, CIT_CATCH_GET_CHILD, 0, 0.0 },` |
|     - |  4260 | `		{ "TOSTRING_USE_KEY",    PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, CIT_TOSTRING_USE_KEY, 0, 0.0 },` |
|     - |  4261 | `		{ "TOSTRING_USE_CURRENT",PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, CIT_TOSTRING_USE_CUR, 0, 0.0 },` |
|     - |  4262 | `		{ "TOSTRING_USE_INNER",  PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, CIT_TOSTRING_USE_INN, 0, 0.0 },` |
|     - |  4263 | `		{ "FULL_CACHE",          PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, CIT_FULL_CACHE, 0, 0.0 },` |
|     - |  4264 | `	};` |
|     - |  4265 | `	static const PH7_NativePropDef aCitProp[] = {` |
|     - |  4266 | `		{ CIT_FL,   PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  4267 | `		{ CIT_STR,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  4268 | `		{ CIT_CCH,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  4269 | `		{ CIT_KIDS, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  4270 | `	};` |
|     - |  4271 | `	static const PH7_NativeMethodDef aCitMethod[] = {` |
|     - |  4272 | `		{ "__construct", PH7_MOD_PUBLIC, "Iterator $iterator, int $flags = 1", 0,` |
|     - |  4273 | `		  vm_builtin_CachingIterator_construct },` |
|     - |  4274 | `		{ "rewind",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_CachingIterator_rewind },` |
|     - |  4275 | `		{ "valid",       PH7_MOD_PUBLIC, "", "@bool", vm_builtin_CachingIterator_valid },` |
|     - |  4276 | `		{ "next",        PH7_MOD_PUBLIC, "", "@void", vm_builtin_CachingIterator_next },` |
|     - |  4277 | `		{ "hasNext",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_CachingIterator_hasNext },` |
|     - |  4278 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_CachingIterator_toString },` |
|     - |  4279 | `		{ "getFlags",    PH7_MOD_PUBLIC, "", "@int", vm_builtin_CachingIterator_getFlags },` |
|     - |  4280 | `		{ "setFlags",    PH7_MOD_PUBLIC, "int $flags", "@void", vm_builtin_CachingIterator_setFlags },` |
|     - |  4281 | `		{ "offsetGet",   PH7_MOD_PUBLIC, "$key", "@mixed", vm_builtin_CachingIterator_offsetGet },` |
|     - |  4282 | `		{ "offsetSet",   PH7_MOD_PUBLIC, "$key, mixed $value", "@void",` |
|     - |  4283 | `		  vm_builtin_CachingIterator_offsetSet },` |
|     - |  4284 | `		{ "offsetUnset", PH7_MOD_PUBLIC, "$key", "@void", vm_builtin_CachingIterator_offsetUnset },` |
|     - |  4285 | `		{ "offsetExists",PH7_MOD_PUBLIC, "$key", "@bool", vm_builtin_CachingIterator_offsetExists },` |
|     - |  4286 | `		{ "getCache",    PH7_MOD_PUBLIC, "", "@array", vm_builtin_CachingIterator_getCache },` |
|     - |  4287 | `		{ "count",       PH7_MOD_PUBLIC, "", "@int", vm_builtin_CachingIterator_count },` |
|     - |  4288 | `	};` |
|     - |  4289 | `	static const PH7_NativeMethodDef aRcitMethod[] = {` |
|     - |  4290 | ``		/* `~Iterator`: php DECLARES Iterator here and its body asks for a`` |
|     - |  4291 | `		 * RecursiveIterator, so the screen stands aside and the constructor below` |
|     - |  4292 | `		 * raises php's own refusal. */` |
|     - |  4293 | `		{ "__construct", PH7_MOD_PUBLIC, "~Iterator $iterator, int $flags = 1", 0,` |
|     - |  4294 | `		  vm_builtin_RecursiveCachingIterator_construct },` |
|     - |  4295 | `		{ "hasChildren", PH7_MOD_PUBLIC, "", "@bool",` |
|     - |  4296 | `		  vm_builtin_RecursiveCachingIterator_hasChildren },` |
|     - |  4297 | `		{ "getChildren", PH7_MOD_PUBLIC, "", "@?RecursiveCachingIterator",` |
|     - |  4298 | `		  vm_builtin_RecursiveCachingIterator_getChildren },` |
|     - |  4299 | `	};` |
|     - |  4300 | `	static const PH7_NativePropDef aAppendProp[] = {` |
|     - |  4301 | `		{ AP_LIST, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  4302 | `	};` |
|     - |  4303 | `	static const PH7_NativeMethodDef aAppendMethod[] = {` |
|     - |  4304 | `		{ "__construct",      PH7_MOD_PUBLIC, "", 0, vm_builtin_AppendIterator_construct },` |
|     - |  4305 | `		{ "append",           PH7_MOD_PUBLIC, "Iterator $iterator", "@void",` |
|     - |  4306 | `		  vm_builtin_AppendIterator_append },` |
|     - |  4307 | `		{ "rewind",           PH7_MOD_PUBLIC, "", "@void", vm_builtin_AppendIterator_rewind },` |
|     - |  4308 | `		{ "valid",            PH7_MOD_PUBLIC, "", "@bool", vm_builtin_Dual_valid },` |
|     - |  4309 | `		{ "current",          PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_AppendIterator_current },` |
|     - |  4310 | `		{ "next",             PH7_MOD_PUBLIC, "", "@void", vm_builtin_AppendIterator_next },` |
|     - |  4311 | `		{ "getIteratorIndex", PH7_MOD_PUBLIC, "", "@?int",` |
|     - |  4312 | `		  vm_builtin_AppendIterator_getIteratorIndex },` |
|     - |  4313 | `		{ "getArrayIterator", PH7_MOD_PUBLIC, "", "@ArrayIterator",` |
|     - |  4314 | `		  vm_builtin_AppendIterator_getArrayIterator },` |
|     - |  4315 | `	};` |
|     - |  4316 | `	static const PH7_NativeMethodDef aEmptyMethod[] = {` |
|     - |  4317 | `		{ "current", PH7_MOD_PUBLIC, "", "@never", vm_builtin_EmptyIterator_current },` |
|     - |  4318 | `		{ "next",    PH7_MOD_PUBLIC, "", "@void", vm_builtin_EmptyIterator_nop },` |
|     - |  4319 | `		{ "key",     PH7_MOD_PUBLIC, "", "@never", vm_builtin_EmptyIterator_key },` |
|     - |  4320 | `		{ "valid",   PH7_MOD_PUBLIC, "", "@false", vm_builtin_EmptyIterator_valid },` |
|     - |  4321 | `		{ "rewind",  PH7_MOD_PUBLIC, "", "@void", vm_builtin_EmptyIterator_nop },` |
|     - |  4322 | `	};` |
|     - |  4323 | `	/*` |
|     - |  4324 | ``	 * PH7_CLASS_NOCLONE on every dual iterator: php refuses `clone` for all of them`` |
|     - |  4325 | `	 * (its inner iterator handle cannot be duplicated), and a slot-by-slot copy here` |
|     - |  4326 | `	 * would share the inner iterator's cursor between two decorators. EmptyIterator` |
|     - |  4327 | `	 * has no state and php clones it happily.` |
|     - |  4328 | `	 */` |
|     - |  4329 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - |  4330 | `		{ "OuterIterator", "Iterator", 0, PH7_CLASS_INTERFACE,` |
|     - |  4331 | `		  aOuterMethod, SX_ARRAYSIZE(aOuterMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  4332 | `		{ "IteratorIterator", 0, "OuterIterator", PH7_CLASS_NOCLONE,` |
|     - |  4333 | `		  aIterIterMethod, SX_ARRAYSIZE(aIterIterMethod), 0, 0,` |
|     - |  4334 | `		  aDualProp, SX_ARRAYSIZE(aDualProp), 0, 0, 0 },` |
|     - |  4335 | `		{ "FilterIterator", "IteratorIterator", 0, PH7_CLASS_ABSTRACT\|PH7_CLASS_NOCLONE,` |
|     - |  4336 | `		  aFilterMethod, SX_ARRAYSIZE(aFilterMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  4337 | `		{ "CallbackFilterIterator", "FilterIterator", 0, PH7_CLASS_NOCLONE,` |
|     - |  4338 | `		  aCbFilterMethod, SX_ARRAYSIZE(aCbFilterMethod), 0, 0,` |
|     - |  4339 | `		  aCbProp, SX_ARRAYSIZE(aCbProp), 0, 0, 0 },` |
|     - |  4340 | `		{ "LimitIterator", "IteratorIterator", 0, PH7_CLASS_NOCLONE,` |
|     - |  4341 | `		  aLimitMethod, SX_ARRAYSIZE(aLimitMethod), 0, 0,` |
|     - |  4342 | `		  aLimitProp, SX_ARRAYSIZE(aLimitProp), 0, 0, 0 },` |
|     - |  4343 | `		{ "InfiniteIterator", "IteratorIterator", 0, PH7_CLASS_NOCLONE,` |
|     - |  4344 | `		  aInfiniteMethod, SX_ARRAYSIZE(aInfiniteMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  4345 | `		{ "NoRewindIterator", "IteratorIterator", 0, PH7_CLASS_NOCLONE,` |
|     - |  4346 | `		  aNoRewindMethod, SX_ARRAYSIZE(aNoRewindMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  4347 | `		{ "RegexIterator", "FilterIterator", 0, PH7_CLASS_NOCLONE,` |
|     - |  4348 | `		  aRegexMethod, SX_ARRAYSIZE(aRegexMethod),` |
|     - |  4349 | `		  aRegexConst, SX_ARRAYSIZE(aRegexConst),` |
|     - |  4350 | `		  aRegexProp, SX_ARRAYSIZE(aRegexProp), 0, 0, 0 },` |
|     - |  4351 | `		{ "AppendIterator", "IteratorIterator", 0, PH7_CLASS_NOCLONE,` |
|     - |  4352 | `		  aAppendMethod, SX_ARRAYSIZE(aAppendMethod), 0, 0,` |
|     - |  4353 | `		  aAppendProp, SX_ARRAYSIZE(aAppendProp), 0, 0, 0 },` |
|     - |  4354 | `		{ "RecursiveIterator", "Iterator", 0, PH7_CLASS_INTERFACE,` |
|     - |  4355 | `		  aRecursiveMethod, SX_ARRAYSIZE(aRecursiveMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  4356 | `		/* RecursiveArrayIterator is CLONEABLE (php clones an ArrayIterator happily) and` |
|     - |  4357 | `		 * inherits every one of its parent's C bodies, storage slots included. */` |
|     - |  4358 | `		{ "RecursiveArrayIterator", "ArrayIterator", "RecursiveIterator", 0,` |
|     - |  4359 | `		  aRaiMethod, SX_ARRAYSIZE(aRaiMethod),` |
|     - |  4360 | `		  aRaiConst, SX_ARRAYSIZE(aRaiConst), 0, 0, 0, 0, 0 },` |
|     - |  4361 | `		{ "RecursiveFilterIterator", "FilterIterator", "RecursiveIterator",` |
|     - |  4362 | `		  PH7_CLASS_ABSTRACT\|PH7_CLASS_NOCLONE,` |
|     - |  4363 | `		  aRfiMethod, SX_ARRAYSIZE(aRfiMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  4364 | `		/* The three recursive twins. The two whose parent is a PLAIN filter name` |
|     - |  4365 | `		 * RecursiveIterator themselves; ParentIterator inherits it from` |
|     - |  4366 | `		 * RecursiveFilterIterator, which is where php has it too. */` |
|     - |  4367 | `		{ "ParentIterator", "RecursiveFilterIterator", 0, PH7_CLASS_NOCLONE,` |
|     - |  4368 | `		  aParentMethod, SX_ARRAYSIZE(aParentMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  4369 | `		{ "RecursiveCallbackFilterIterator", "CallbackFilterIterator", "RecursiveIterator",` |
|     - |  4370 | `		  PH7_CLASS_NOCLONE,` |
|     - |  4371 | `		  aRcbfMethod, SX_ARRAYSIZE(aRcbfMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  4372 | `		{ "RecursiveRegexIterator", "RegexIterator", "RecursiveIterator", PH7_CLASS_NOCLONE,` |
|     - |  4373 | `		  aRregexMethod, SX_ARRAYSIZE(aRregexMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  4374 | `		/* CachingIterator is Stringable through __toString, and Countable/ArrayAccess` |
|     - |  4375 | `		 * over the FULL_CACHE array — three interfaces the class refuses to serve` |
|     - |  4376 | `		 * unless it was built with that flag. */` |
|     - |  4377 | `		{ "CachingIterator", "IteratorIterator", "ArrayAccess,Countable,Stringable",` |
|     - |  4378 | `		  PH7_CLASS_NOCLONE,` |
|     - |  4379 | `		  aCitMethod, SX_ARRAYSIZE(aCitMethod), aCitConst, SX_ARRAYSIZE(aCitConst),` |
|     - |  4380 | `		  aCitProp, SX_ARRAYSIZE(aCitProp), 0, 0, 0 },` |
|     - |  4381 | `		{ "RecursiveCachingIterator", "CachingIterator", "RecursiveIterator", PH7_CLASS_NOCLONE,` |
|     - |  4382 | `		  aRcitMethod, SX_ARRAYSIZE(aRcitMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  4383 | `		{ "EmptyIterator", 0, "Iterator", 0,` |
|     - |  4384 | `		  aEmptyMethod, SX_ARRAYSIZE(aEmptyMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  4385 | `	};` |
|  5745 |  4386 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 |  4387 | `}` |
|     - |  4388 | `/*` |
|     - |  4389 | ` * ---------------------------------------------------------------------------` |
|     - |  4390 | ` * RecursiveIteratorIterator.` |
|     - |  4391 | ` *` |
|     - |  4392 | `` * php's `spl_recursive_it_object` is a STACK OF LEVELS plus a five-value state`` |
|     - |  4393 | ` * machine, and reading the struct before the methods (rule 43) is what this` |
|     - |  4394 | ` * conversion turns on. Each level carries the sub-iterator AND its own` |
|     - |  4395 | `` * RecursiveIteratorState; `move_forward` is one loop over that pair, and every`` |
|     - |  4396 | ` * method is a two-line reader of it. The chunk instead kept a stack of iterators` |
|     - |  4397 | `` * with the state implied by two booleans (`__post`, `__live`), which is where all`` |
|     - |  4398 | ` * eight of its divergences came from:` |
|     - |  4399 | ` *` |
|     - |  4400 | ` *   - getDepth()/getSubIterator()/getInnerIterator() answered from an EMPTY stack` |
|     - |  4401 | ` *     before the first rewind(), so they reported -1 and null where php reports 0` |
|     - |  4402 | ` *     and the root -- php seeds level 0 in the CONSTRUCTOR and never unseeds it.` |
|     - |  4403 | `` *   - valid() answered a `__live` flag that only rewind() sets; php ASKS the`` |
|     - |  4404 | ` *     levels (any valid sub-iterator, walking down), so a fresh instance over a` |
|     - |  4405 | ` *     non-empty iterator is already valid().` |
|     - |  4406 | ` *   - LEAVES_ONLY past max depth YIELDED the container; php skips it, which is` |
|     - |  4407 | ``  *     the whole point of the mode (`walk-leaves-maxdepth0` returned the `b` `` |
|     - |  4408 | ` *     array as if it were a leaf).` |
|     - |  4409 | `` *   - the mode was `$mode \| $flags` masked with & 3, so CATCH_GET_CHILD passed`` |
|     - |  4410 | ` *     as $mode descended like LEAVES_ONLY; php compares mode EXACTLY and an` |
|     - |  4411 | ` *     unknown mode matches no arm at all, descending nowhere.` |
|     - |  4412 | ` *   - hasChildren() was called on the sub-iterator DIRECTLY, so a subclass` |
|     - |  4413 | ` *     overriding callHasChildren() -- php's documented hook -- was never asked.` |
|     - |  4414 | ` *   - endChildren() ran AFTER the pop, reporting a depth one too low and firing` |
|     - |  4415 | ` *     a spurious final call at depth -1; php calls it before the pop.` |
|     - |  4416 | ` *   - a second rewind() fired beginIteration() again; php's in_iteration latch` |
|     - |  4417 | ` *     makes it once per iteration.` |
|     - |  4418 | ` *   - getChildren() returning a non-RecursiveIterator was silently treated as` |
|     - |  4419 | ` *     "no children"; php throws UnexpectedValueException.` |
|     - |  4420 | ` *` |
|     - |  4421 | ` * The level stack lives in two parallel arrays indexed by level rather than in a` |
|     - |  4422 | `` * C block behind a handle: php SERIALIZES this class (`O:25:"…":0:{}`), and a raw`` |
|     - |  4423 | ` * pointer in a hidden slot is exactly what rule 19 exists to keep out of` |
|     - |  4424 | ` * serialize() output. Every slot is PH7_MOD_HIDDEN, so php's zero-property` |
|     - |  4425 | ` * presentation holds for var_dump, print_r, var_export, (array) and Reflection.` |
|     - |  4426 | ` */` |
|     - |  4427 | `#define RIT_ST   "__st"   /* php's iterators[level].zobject */` |
|     - |  4428 | `#define RIT_SS   "__ss"   /* php's iterators[level].state */` |
|     - |  4429 | `#define RIT_LVL  "__lvl"  /* php's object->level */` |
|     - |  4430 | `#define RIT_MD   "__md"   /* php's object->mode, stored UNMASKED */` |
|     - |  4431 | `#define RIT_FL   "__fl"   /* php's object->flags */` |
|     - |  4432 | `#define RIT_MX   "__mx"   /* php's object->max_depth, -1 = unlimited */` |
|     - |  4433 | `#define RIT_II   "__ii"   /* php's object->in_iteration */` |
|     - |  4434 | ``#define RIT_RD   "__rd"   /* php's `object->iterators != NULL`: the parent ctor ran */`` |
|     - |  4435 |  |
|     - |  4436 | `/* php's RecursiveIteratorState */` |
|     - |  4437 | `#define RS_NEXT  0` |
|     - |  4438 | `#define RS_TEST  1` |
|     - |  4439 | `#define RS_SELF  2` |
|     - |  4440 | `#define RS_CHILD 3` |
|     - |  4441 | `#define RS_START 4` |
|     - |  4442 |  |
|     - |  4443 | `/* php's RecursiveIteratorMode + the one flag */` |
|     - |  4444 | `#define RIT_LEAVES_ONLY     0` |
|     - |  4445 | `#define RIT_SELF_FIRST      1` |
|     - |  4446 | `#define RIT_CHILD_FIRST     2` |
|     - |  4447 | `#define RIT_CATCH_GET_CHILD 16` |
|     - |  4448 |  |
|     - |  4449 | `/*` |
|     - |  4450 | `` * php's `object->iterators != NULL`. Its get_method handler refuses EVERY method`` |
|     - |  4451 | ` * on an instance whose parent constructor never ran -- not the individual bodies,` |
|     - |  4452 | ` * which is why the refusal is an Error naming the RUNTIME class and why even` |
|     - |  4453 | ` * getDepth() raises it.` |
|     - |  4454 | ` */` |
|  3152 |  4455 | `static int RitReady(ph7_class_instance *pThis)` |
|     1 |  4456 | `{` |
|  3153 |  4457 | `	return pThis && PH7_NativeAttrInt(pThis,RIT_RD) != 0;` |
|     1 |  4458 | `}` |
|    20 |  4459 | `static sxi32 RitNotReady(ph7_context *pCtx)` |
|     1 |  4460 | `{` |
|    21 |  4461 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    21 |  4462 | `	SyString *pName = pThis ? &pThis->pClass->sName : 0;` |
|    31 |  4463 | `	return PH7_VmThrowException(pCtx,"Error",` |
|    10 |  4464 | `		"The %z instance wasn't initialized properly",pName);` |
|     1 |  4465 | `}` |
|  4648 |  4466 | `static int RitInt(ph7_class_instance *pThis,const char *zSlot)` |
|     1 |  4467 | `{` |
|  4649 |  4468 | `	return (int)PH7_NativeAttrInt(pThis,zSlot);` |
|     1 |  4469 | `}` |
|     - |  4470 | `/* One of the two level-indexed arrays, materialized on first use. */` |
|  6000 |  4471 | `static ph7_hashmap * RitMap(ph7_vm *pVm,ph7_class_instance *pThis,const char *zSlot)` |
|     1 |  4472 | `{` |
|  6001 |  4473 | `	ph7_value *pSlot = PH7_NativeAttr(pThis,zSlot);` |
|  6001 |  4474 | `	if( pSlot == 0 ){` |
|   ! 0 |  4475 | `		return 0;` |
|     - |  4476 | `	}` |
|  6001 |  4477 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   273 |  4478 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|   ! 0 |  4479 | `			return 0;` |
|     - |  4480 | `		}` |
|   136 |  4481 | `	}` |
|  6001 |  4482 | `	return PH7_HashmapCowSeparate(pVm,pSlot);` |
|  3001 |  4483 | `}` |
|  4002 |  4484 | `static ph7_value * RitAt(ph7_vm *pVm,ph7_class_instance *pThis,const char *zSlot,int iLevel)` |
|     1 |  4485 | `{` |
|  4003 |  4486 | `	ph7_hashmap *pMap = RitMap(pVm,pThis,zSlot);` |
|  4003 |  4487 | `	ph7_hashmap_node *pNode = 0;` |
|  4003 |  4488 | `	if( pMap == 0 \|\| HashmapLookupIntKey(pMap,(sxi64)iLevel,&pNode) != SXRET_OK ){` |
|   ! 0 |  4489 | `		return 0;` |
|     - |  4490 | `	}` |
|  4003 |  4491 | `	return HashmapExtractNodeValue(pNode);` |
|  2002 |  4492 | `}` |
|  1502 |  4493 | `static void RitPut(ph7_vm *pVm,ph7_class_instance *pThis,const char *zSlot,int iLevel,ph7_value *pVal)` |
|     1 |  4494 | `{` |
|  1503 |  4495 | `	ph7_hashmap *pMap = RitMap(pVm,pThis,zSlot);` |
|     - |  4496 | `	ph7_value sKey;` |
|  1503 |  4497 | `	if( pMap == 0 ){` |
|   ! 0 |  4498 | `		return;` |
|     - |  4499 | `	}` |
|  1503 |  4500 | `	PH7_MemObjInitFromInt(pVm,&sKey,(sxi64)iLevel);` |
|  1503 |  4501 | `	PH7_HashmapInsert(pMap,&sKey,pVal);` |
|  1503 |  4502 | `	PH7_MemObjRelease(&sKey);` |
|   752 |  4503 | `}` |
|   496 |  4504 | `static void RitErase(ph7_vm *pVm,ph7_class_instance *pThis,const char *zSlot,int iLevel)` |
|     1 |  4505 | `{` |
|   497 |  4506 | `	ph7_hashmap *pMap = RitMap(pVm,pThis,zSlot);` |
|   497 |  4507 | `	ph7_hashmap_node *pNode = 0;` |
|   497 |  4508 | `	if( pMap && HashmapLookupIntKey(pMap,(sxi64)iLevel,&pNode) == SXRET_OK ){` |
|   225 |  4509 | `		PH7_HashmapUnlinkNode(pNode,TRUE);` |
|   112 |  4510 | `	}` |
|   497 |  4511 | `}` |
|     - |  4512 | `/*` |
|     - |  4513 | ` * The sub-iterator at a level. Re-resolved on every use on purpose: RitAt()` |
|     - |  4514 | ` * hands back a pointer into pVm->aMemObj, which REALLOCATES as the VM reserves` |
|     - |  4515 | ` * objects, and every call into a user iterator reserves some (rule 47).` |
|     - |  4516 | ` */` |
|  3260 |  4517 | `static ph7_class_instance * RitSub(ph7_vm *pVm,ph7_class_instance *pThis,int iLevel)` |
|     1 |  4518 | `{` |
|  3261 |  4519 | `	ph7_value *pVal = RitAt(pVm,pThis,RIT_ST,iLevel);` |
|  3261 |  4520 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 |  4521 | `		return 0;` |
|     - |  4522 | `	}` |
|  3261 |  4523 | `	return (ph7_class_instance *)pVal->x.pOther;` |
|  1631 |  4524 | `}` |
|   742 |  4525 | `static int RitState(ph7_vm *pVm,ph7_class_instance *pThis,int iLevel)` |
|     1 |  4526 | `{` |
|   743 |  4527 | `	ph7_value *pVal = RitAt(pVm,pThis,RIT_SS,iLevel);` |
|   743 |  4528 | `	return pVal ? (int)ph7_value_to_int64(pVal) : RS_START;` |
|     1 |  4529 | `}` |
|  1254 |  4530 | `static void RitSetState(ph7_vm *pVm,ph7_class_instance *pThis,int iLevel,int iState)` |
|     1 |  4531 | `{` |
|     - |  4532 | `	ph7_value sVal;` |
|  1255 |  4533 | `	PH7_MemObjInitFromInt(pVm,&sVal,(sxi64)iState);` |
|  1255 |  4534 | `	RitPut(pVm,pThis,RIT_SS,iLevel,&sVal);` |
|  1255 |  4535 | `	PH7_MemObjRelease(&sVal);` |
|  1255 |  4536 | `}` |
|     - |  4537 | ``/* php's `iterators = erealloc(…, ++level+1)` plus the two field writes. */`` |
|   112 |  4538 | `static void RitPush(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_instance *pChild)` |
|     1 |  4539 | `{` |
|   113 |  4540 | `	int iLevel = RitInt(pThis,RIT_LVL) + 1;` |
|     - |  4541 | `	ph7_value sObj;` |
|   113 |  4542 | `	PH7_MemObjInit(pVm,&sObj);` |
|   113 |  4543 | `	sObj.x.pOther = pChild;` |
|   113 |  4544 | `	MemObjSetType(&sObj,MEMOBJ_OBJ);` |
|     - |  4545 | `	/* The map takes its OWN reference through the store; the carrier is blanked` |
|     - |  4546 | `	 * rather than released, because releasing a MEMOBJ_OBJ carrier would unref an` |
|     - |  4547 | `	 * instance this frame never referenced (rule 16). */` |
|   113 |  4548 | `	RitPut(pVm,pThis,RIT_ST,iLevel,&sObj);` |
|   113 |  4549 | `	sObj.x.pOther = 0;` |
|   113 |  4550 | `	MemObjSetType(&sObj,MEMOBJ_NULL);` |
|   113 |  4551 | `	PH7_MemObjRelease(&sObj);` |
|   113 |  4552 | `	RitSetState(pVm,pThis,iLevel,RS_START);` |
|   113 |  4553 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_LVL,iLevel);` |
|   113 |  4554 | `}` |
|   112 |  4555 | `static void RitPop(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  4556 | `{` |
|   113 |  4557 | `	int iLevel = RitInt(pThis,RIT_LVL);` |
|   113 |  4558 | `	if( iLevel <= 0 ){` |
|   ! 0 |  4559 | `		return;` |
|     - |  4560 | `	}` |
|   113 |  4561 | `	RitErase(pVm,pThis,RIT_ST,iLevel);` |
|   113 |  4562 | `	RitErase(pVm,pThis,RIT_SS,iLevel);` |
|   113 |  4563 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_LVL,iLevel-1);` |
|    57 |  4564 | `}` |
|     - |  4565 | `/* Drop every level: php's spl_RecursiveIteratorIterator_free_iterators. */` |
|   136 |  4566 | `static void RitClear(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  4567 | `{` |
|   137 |  4568 | `	int iLevel = RitInt(pThis,RIT_LVL);` |
|   273 |  4569 | `	while( iLevel >= 0 ){` |
|   137 |  4570 | `		RitErase(pVm,pThis,RIT_ST,iLevel);` |
|   137 |  4571 | `		RitErase(pVm,pThis,RIT_SS,iLevel);` |
|   137 |  4572 | `		iLevel--;` |
|     1 |  4573 | `	}` |
|   137 |  4574 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_LVL,0);` |
|   137 |  4575 | `}` |
|     - |  4576 | `/*` |
|     - |  4577 | ` * Call a method, optionally SWALLOWING what it throws -- php clears the exception` |
|     - |  4578 | ` * at four sites when RIT_CATCH_GET_CHILD is set, and PH7_VmCallMethodSwallow is` |
|     - |  4579 | ` * the only way to spell that here (a throw raised under a C call site is` |
|     - |  4580 | ` * dispatched INLINE, so an enclosing user catch would run before this returns).` |
|     - |  4581 | ` * *pbThrew reports a swallowed throw, which php reads back as "retval is UNDEF".` |
|     - |  4582 | ` */` |
|  3574 |  4583 | `static sxi32 RitCall(ph7_context *pCtx,ph7_class_instance *pObj,const char *zName,sxu32 nName,` |
|     - |  4584 | `	ph7_value *pOut,int bCatch,int *pbThrew)` |
|     1 |  4585 | `{` |
|  3575 |  4586 | `	ph7_class_method *pMethod = pObj ? PH7_ClassExtractMethod(pObj->pClass,zName,nName) : 0;` |
|  3575 |  4587 | `	if( pbThrew ){` |
|  1353 |  4588 | `		*pbThrew = FALSE;` |
|   676 |  4589 | `	}` |
|  3575 |  4590 | `	if( pMethod == 0 ){` |
|   ! 0 |  4591 | `		return SXRET_OK;` |
|     - |  4592 | `	}` |
|  3575 |  4593 | `	if( bCatch ){` |
|    13 |  4594 | `		return PH7_VmCallMethodSwallow(pCtx->pVm,pObj,pMethod,pOut,0,0,pbThrew);` |
|     - |  4595 | `	}` |
|  3563 |  4596 | `	return PH7_VmCallClassMethod(pCtx->pVm,pObj,pMethod,pOut,0,0);` |
|  1788 |  4597 | `}` |
|     - |  4598 | `/*` |
|     - |  4599 | ` * A hook on $this. php caches which of the seven the SUBCLASS overrides and calls` |
|     - |  4600 | ` * the sub-iterator directly when none does; dispatching through $this every time` |
|     - |  4601 | ` * reaches the same body -- the base ones are the no-ops php would have skipped --` |
|     - |  4602 | ` * with the override found automatically.` |
|     - |  4603 | ` */` |
|  1186 |  4604 | `static sxi32 RitHook(ph7_context *pCtx,const char *zName,sxu32 nName,ph7_value *pOut,` |
|     - |  4605 | `	int bCatch,int *pbThrew)` |
|     1 |  4606 | `{` |
|  1187 |  4607 | `	return RitCall(pCtx,PH7_ContextThis(pCtx),zName,nName,pOut,bCatch,pbThrew);` |
|     1 |  4608 | `}` |
|   392 |  4609 | `static int RitCatches(ph7_class_instance *pThis)` |
|     1 |  4610 | `{` |
|   393 |  4611 | `	return (RitInt(pThis,RIT_FL) & RIT_CATCH_GET_CHILD) != 0;` |
|     1 |  4612 | `}` |
|     - |  4613 | `/*` |
|     - |  4614 | ` * php's spl_recursive_it_move_forward_ex, transcribed. The switch's fallthroughs` |
|     - |  4615 | ` * (RS_NEXT into RS_START into RS_TEST) are written as a sequential if-chain, and` |
|     - |  4616 | `` * php's `goto next_step` is this loop's `continue`.`` |
|     - |  4617 | ` */` |
|   392 |  4618 | `static sxi32 RitMoveForward(ph7_context *pCtx)` |
|     1 |  4619 | `{` |
|   393 |  4620 | `	ph7_vm *pVm = pCtx->pVm;` |
|   393 |  4621 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  4622 | `	int bCatch;` |
|   393 |  4623 | `	if( !RitReady(pThis) ){` |
|   ! 0 |  4624 | `		return RitNotReady(pCtx);` |
|     - |  4625 | `	}` |
|   393 |  4626 | `	bCatch = RitCatches(pThis);` |
|   427 |  4627 | `	for(;;){` |
|     - |  4628 | `		ph7_class_instance *pSub;` |
|   743 |  4629 | `		int iLevel = RitInt(pThis,RIT_LVL);` |
|   743 |  4630 | `		int iState = RitState(pVm,pThis,iLevel);` |
|   743 |  4631 | `		int bThrew = 0;` |
|   743 |  4632 | `		int bExhausted = 0;` |
|     - |  4633 | `		sxi32 rc;` |
|   743 |  4634 | `		pSub = RitSub(pVm,pThis,iLevel);` |
|   743 |  4635 | `		if( pSub == 0 ){` |
|   ! 0 |  4636 | `			return PH7_OK;` |
|     - |  4637 | `		}` |
|   743 |  4638 | `		if( iState == RS_NEXT ){` |
|   341 |  4639 | `			rc = RitCall(pCtx,pSub,"next",sizeof("next")-1,0,bCatch,&bThrew);` |
|   341 |  4640 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  4641 | `				return rc;` |
|     - |  4642 | `			}` |
|   341 |  4643 | `			pSub = RitSub(pVm,pThis,iLevel);   /* the call may have moved aMemObj */` |
|   341 |  4644 | `			if( pSub == 0 ){` |
|   ! 0 |  4645 | `				return PH7_OK;` |
|     - |  4646 | `			}` |
|   341 |  4647 | `			iState = RS_START;                 /* php's fallthrough */` |
|   170 |  4648 | `		}` |
|   743 |  4649 | `		if( iState == RS_START ){` |
|     - |  4650 | `			ph7_value sValid;` |
|   551 |  4651 | `			PH7_MemObjInit(pVm,&sValid);` |
|   551 |  4652 | `			rc = RitCall(pCtx,pSub,"valid",sizeof("valid")-1,&sValid,FALSE,0);` |
|   551 |  4653 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  4654 | `				PH7_MemObjRelease(&sValid);` |
|   ! 0 |  4655 | `				return rc;` |
|     - |  4656 | `			}` |
|   551 |  4657 | `			bExhausted = !ph7_value_to_bool(&sValid);` |
|   551 |  4658 | `			PH7_MemObjRelease(&sValid);` |
|   551 |  4659 | `			if( !bExhausted ){` |
|     - |  4660 | `				/* php re-reads the level here and returns outright when the valid()` |
|     - |  4661 | `				 * call RE-ENTERED this iterator (a sub-iterator that drove the` |
|     - |  4662 | `				 * decorator behind its back); the stack it was walking is gone. */` |
|   361 |  4663 | `				if( RitInt(pThis,RIT_LVL) != iLevel \|\| RitSub(pVm,pThis,iLevel) != pSub ){` |
|   ! 0 |  4664 | `					return PH7_OK;` |
|     - |  4665 | `				}` |
|   361 |  4666 | `				RitSetState(pVm,pThis,iLevel,RS_TEST);` |
|   361 |  4667 | `				iState = RS_TEST;` |
|   180 |  4668 | `			}` |
|   275 |  4669 | `		}` |
|   743 |  4670 | `		if( !bExhausted && iState == RS_TEST ){` |
|     - |  4671 | `			ph7_value sHas;` |
|   361 |  4672 | `			int bDescend = 0;` |
|   361 |  4673 | `			PH7_MemObjInit(pVm,&sHas);` |
|   361 |  4674 | `			rc = RitHook(pCtx,"callHasChildren",sizeof("callHasChildren")-1,&sHas,bCatch,&bThrew);` |
|   361 |  4675 | `			if( rc != SXRET_OK ){` |
|     - |  4676 | `				/* php leaves the level on RS_NEXT so a caught-and-resumed traversal` |
|     - |  4677 | `				 * moves on rather than re-asking the same element. */` |
|   ! 0 |  4678 | `				RitSetState(pVm,pThis,iLevel,RS_NEXT);` |
|   ! 0 |  4679 | `				PH7_MemObjRelease(&sHas);` |
|   ! 0 |  4680 | `				return rc;` |
|     - |  4681 | `			}` |
|     - |  4682 | `			/* A SWALLOWED throw leaves php's retval UNDEF, which skips the` |
|     - |  4683 | `			 * has-children test entirely and yields the element. */` |
|   361 |  4684 | `			if( !bThrew && ph7_value_to_bool(&sHas) ){` |
|   137 |  4685 | `				int iMax = RitInt(pThis,RIT_MX);` |
|   137 |  4686 | `				int iMode = RitInt(pThis,RIT_MD);` |
|   137 |  4687 | `				if( iMax == -1 \|\| iMax > iLevel ){` |
|     - |  4688 | `					/* php compares the mode EXACTLY: an unrecognized mode matches no` |
|     - |  4689 | `					 * arm, falls out of the switch and yields without descending. */` |
|   125 |  4690 | `					if( iMode == RIT_LEAVES_ONLY \|\| iMode == RIT_CHILD_FIRST ){` |
|    65 |  4691 | `						RitSetState(pVm,pThis,iLevel,RS_CHILD);` |
|    65 |  4692 | `						bDescend = 1;` |
|    93 |  4693 | `					}else if( iMode == RIT_SELF_FIRST ){` |
|    57 |  4694 | `						RitSetState(pVm,pThis,iLevel,RS_SELF);` |
|    57 |  4695 | `						bDescend = 1;` |
|    29 |  4696 | `					}` |
|    75 |  4697 | `				}else if( iMode == RIT_LEAVES_ONLY ){` |
|     - |  4698 | `					/* Too deep to recurse into and NOT a leaf, so php skips it —` |
|     - |  4699 | `					 * the mode's defining rule, and the one the chunk dropped. */` |
|     5 |  4700 | `					RitSetState(pVm,pThis,iLevel,RS_NEXT);` |
|     5 |  4701 | `					bDescend = 1;` |
|     2 |  4702 | `				}` |
|    68 |  4703 | `			}` |
|   361 |  4704 | `			PH7_MemObjRelease(&sHas);` |
|   361 |  4705 | `			if( bDescend ){` |
|   125 |  4706 | `				continue;                      /* php's goto next_step */` |
|     - |  4707 | `			}` |
|   237 |  4708 | `			rc = RitHook(pCtx,"nextElement",sizeof("nextElement")-1,0,bCatch,&bThrew);` |
|   237 |  4709 | `			RitSetState(pVm,pThis,iLevel,RS_NEXT);` |
|   237 |  4710 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  4711 | `				return rc;` |
|     - |  4712 | `			}` |
|   237 |  4713 | `			return PH7_OK;                     /* yield this element */` |
|     - |  4714 | `		}` |
|   383 |  4715 | `		if( !bExhausted && iState == RS_SELF ){` |
|    75 |  4716 | `			int iMode = RitInt(pThis,RIT_MD);` |
|    75 |  4717 | `			if( iMode == RIT_SELF_FIRST \|\| iMode == RIT_CHILD_FIRST ){` |
|    75 |  4718 | `				rc = RitHook(pCtx,"nextElement",sizeof("nextElement")-1,0,bCatch,&bThrew);` |
|    75 |  4719 | `				if( rc != SXRET_OK ){` |
|   ! 0 |  4720 | `					return rc;` |
|     - |  4721 | `				}` |
|    37 |  4722 | `			}` |
|    75 |  4723 | `			RitSetState(pVm,pThis,iLevel,iMode == RIT_SELF_FIRST ? RS_CHILD : RS_NEXT);` |
|    75 |  4724 | `			return PH7_OK;                     /* yield this element */` |
|     - |  4725 | `		}` |
|   309 |  4726 | `		if( !bExhausted && iState == RS_CHILD ){` |
|     - |  4727 | `			ph7_class *pRecCls;` |
|     - |  4728 | `			ph7_class_instance *pChild;` |
|     - |  4729 | `			ph7_value sChild;` |
|   119 |  4730 | `			int iMode = RitInt(pThis,RIT_MD);` |
|   119 |  4731 | `			PH7_MemObjInit(pVm,&sChild);` |
|   119 |  4732 | `			rc = RitHook(pCtx,"callGetChildren",sizeof("callGetChildren")-1,&sChild,bCatch,&bThrew);` |
|   119 |  4733 | `			if( rc != SXRET_OK ){` |
|     3 |  4734 | `				PH7_MemObjRelease(&sChild);` |
|     4 |  4735 | `				return rc;` |
|     - |  4736 | `			}` |
|   117 |  4737 | `			if( bThrew ){` |
|     - |  4738 | `				/* Caught: php drops the element and moves to the next one. */` |
|     3 |  4739 | `				PH7_MemObjRelease(&sChild);` |
|     3 |  4740 | `				RitSetState(pVm,pThis,iLevel,RS_NEXT);` |
|    59 |  4741 | `				continue;` |
|     - |  4742 | `			}` |
|   115 |  4743 | `			pRecCls = PH7_VmExtractClass(pVm,"RecursiveIterator",` |
|     - |  4744 | `				sizeof("RecursiveIterator")-1,FALSE,0);` |
|   172 |  4745 | `			pChild = (sChild.iFlags & MEMOBJ_OBJ) != 0` |
|   113 |  4746 | `				? (ph7_class_instance *)sChild.x.pOther : 0;` |
|   115 |  4747 | `			if( pChild == 0 \|\| (pRecCls && !PH7_VmInstanceOf(pChild->pClass,pRecCls)) ){` |
|     3 |  4748 | `				PH7_MemObjRelease(&sChild);` |
|     3 |  4749 | `				return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - |  4750 | `					"Objects returned by RecursiveIterator::getChildren() must implement RecursiveIterator");` |
|     - |  4751 | `			}` |
|   113 |  4752 | `			pChild->iRef++;                    /* survive the release of the call result */` |
|   113 |  4753 | `			PH7_MemObjRelease(&sChild);` |
|   113 |  4754 | `			RitSetState(pVm,pThis,iLevel,iMode == RIT_CHILD_FIRST ? RS_SELF : RS_NEXT);` |
|   113 |  4755 | `			RitPush(pVm,pThis,pChild);` |
|   113 |  4756 | `			PH7_ClassInstanceUnref(pChild);    /* the level's slot holds it now */` |
|   113 |  4757 | `			rc = RitCall(pCtx,pChild,"rewind",sizeof("rewind")-1,0,FALSE,0);` |
|   113 |  4758 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  4759 | `				return rc;` |
|     - |  4760 | `			}` |
|   113 |  4761 | `			rc = RitHook(pCtx,"beginChildren",sizeof("beginChildren")-1,0,bCatch,&bThrew);` |
|   113 |  4762 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  4763 | `				return rc;` |
|     - |  4764 | `			}` |
|   113 |  4765 | `			continue;                          /* php's goto next_step */` |
|     - |  4766 | `		}` |
|     - |  4767 | `		/* No more elements at this level. */` |
|   191 |  4768 | `		if( iLevel <= 0 ){` |
|    79 |  4769 | `			return PH7_OK;                     /* done completely */` |
|     - |  4770 | `		}` |
|     - |  4771 | `		/* php calls endChildren BEFORE the pop, so the hook sees the depth it is` |
|     - |  4772 | `		 * leaving rather than the one it lands on. */` |
|   113 |  4773 | `		rc = RitHook(pCtx,"endChildren",sizeof("endChildren")-1,0,bCatch,&bThrew);` |
|   113 |  4774 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  4775 | `			return rc;` |
|     - |  4776 | `		}` |
|   113 |  4777 | `		if( RitInt(pThis,RIT_LVL) > 0 && RitSub(pVm,pThis,RitInt(pThis,RIT_LVL)) == pSub ){` |
|   113 |  4778 | `			RitPop(pVm,pThis);` |
|    56 |  4779 | `		}` |
|     1 |  4780 | `	}` |
|   197 |  4781 | `}` |
|     - |  4782 | `/*` |
|     - |  4783 | ` * php's spl_recursive_it_valid_ex: ASK the levels, walking down from the current` |
|     - |  4784 | ` * one, and fire endIteration the first time the answer is no.` |
|     - |  4785 | ` */` |
|   374 |  4786 | `static sxi32 RitValidEx(ph7_context *pCtx,int *pbValid)` |
|     1 |  4787 | `{` |
|   375 |  4788 | `	ph7_vm *pVm = pCtx->pVm;` |
|   375 |  4789 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   375 |  4790 | `	int iLevel = RitInt(pThis,RIT_LVL);` |
|     - |  4791 | `	sxi32 rc;` |
|   375 |  4792 | `	*pbValid = FALSE;` |
|   455 |  4793 | `	while( iLevel >= 0 ){` |
|   375 |  4794 | `		ph7_class_instance *pSub = RitSub(pVm,pThis,iLevel);` |
|     - |  4795 | `		ph7_value sValid;` |
|     - |  4796 | `		int bOk;` |
|   375 |  4797 | `		if( pSub == 0 ){` |
|   ! 0 |  4798 | `			iLevel--;` |
|   ! 0 |  4799 | `			continue;` |
|     - |  4800 | `		}` |
|   375 |  4801 | `		PH7_MemObjInit(pVm,&sValid);` |
|   375 |  4802 | `		rc = RitCall(pCtx,pSub,"valid",sizeof("valid")-1,&sValid,FALSE,0);` |
|   375 |  4803 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  4804 | `			PH7_MemObjRelease(&sValid);` |
|   ! 0 |  4805 | `			return rc;` |
|     - |  4806 | `		}` |
|   375 |  4807 | `		bOk = ph7_value_to_bool(&sValid);` |
|   375 |  4808 | `		PH7_MemObjRelease(&sValid);` |
|   375 |  4809 | `		if( bOk ){` |
|   295 |  4810 | `			*pbValid = TRUE;` |
|   295 |  4811 | `			return PH7_OK;` |
|     - |  4812 | `		}` |
|    81 |  4813 | `		iLevel--;` |
|     1 |  4814 | `	}` |
|    81 |  4815 | `	if( RitInt(pThis,RIT_II) ){` |
|    79 |  4816 | `		rc = RitHook(pCtx,"endIteration",sizeof("endIteration")-1,0,FALSE,0);` |
|    79 |  4817 | `		PH7_NativeSetAttrInt(pVm,pThis,RIT_II,0);` |
|    79 |  4818 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  4819 | `			return rc;` |
|     - |  4820 | `		}` |
|    39 |  4821 | `	}` |
|    81 |  4822 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_II,0);` |
|    81 |  4823 | `	return PH7_OK;` |
|   188 |  4824 | `}` |
|   140 |  4825 | `static int vm_builtin_RecursiveIteratorIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  4826 | `{` |
|   141 |  4827 | `	ph7_vm *pVm = pCtx->pVm;` |
|   141 |  4828 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  4829 | `	ph7_class_instance *pObj;` |
|   141 |  4830 | `	ph7_class_instance *pHold = 0;` |
|     - |  4831 | `	ph7_class *pAggCls,*pRecCls,*pTravCls;` |
|   141 |  4832 | `	sxi64 iMode = RIT_LEAVES_ONLY,iFlags = 0;` |
|     - |  4833 | `	sxi32 rc;` |
|   141 |  4834 | `	if( pThis == 0 ){` |
|   ! 0 |  4835 | `		return PH7_OK;` |
|     - |  4836 | `	}` |
|     - |  4837 | `	/*` |
|     - |  4838 | `	 * php's ZPP here is "o\|ll" -- a bare OBJECT -- while the stub declares` |
|     - |  4839 | ``	 * `Traversable $iterator`, so the declared type and the refusal text disagree`` |
|     - |  4840 | `	 * (rule 41's neighbour). The spec row carries the declared type for Reflection` |
|     - |  4841 | `	 * and this body words both refusals, which is why the method sits on` |
|     - |  4842 | `	 * azSelfChecked[].` |
|     - |  4843 | `	 */` |
|   141 |  4844 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 \|\| apArg[0]->x.pOther == 0 ){` |
|     - |  4845 | `		char zGiven[64];` |
|     5 |  4846 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  4847 | `			"RecursiveIteratorIterator::__construct(): Argument #1 ($iterator) "` |
|     - |  4848 | `			"must be of type object, %s given",` |
|     2 |  4849 | `			nArg < 1 ? "none" : VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|     - |  4850 | `	}` |
|   139 |  4851 | `	if( nArg > 1 ){` |
|    79 |  4852 | `		rc = PH7_IntArgResolve(pCtx,apArg[1],"RecursiveIteratorIterator::__construct",2,"$mode","int",&iMode);` |
|    79 |  4853 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  4854 | `			return rc;` |
|     - |  4855 | `		}` |
|    39 |  4856 | `	}` |
|   139 |  4857 | `	if( nArg > 2 ){` |
|    63 |  4858 | `		rc = PH7_IntArgResolve(pCtx,apArg[2],"RecursiveIteratorIterator::__construct",3,"$flags","int",&iFlags);` |
|    63 |  4859 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  4860 | `			return rc;` |
|     - |  4861 | `		}` |
|    31 |  4862 | `	}` |
|   139 |  4863 | `	pObj = (ph7_class_instance *)apArg[0]->x.pOther;` |
|   139 |  4864 | `	pAggCls = PH7_VmExtractClass(pVm,"IteratorAggregate",sizeof("IteratorAggregate")-1,FALSE,0);` |
|   139 |  4865 | `	pRecCls = PH7_VmExtractClass(pVm,"RecursiveIterator",sizeof("RecursiveIterator")-1,FALSE,0);` |
|   139 |  4866 | `	pTravCls = PH7_VmExtractClass(pVm,"Traversable",sizeof("Traversable")-1,FALSE,0);` |
|     - |  4867 | `	/*` |
|     - |  4868 | `	 * php's spl_get_iterator_from_aggregate: ONE getIterator() and no more. An` |
|     - |  4869 | `	 * IteratorAggregate whose getIterator() answers another aggregate therefore` |
|     - |  4870 | `	 * fails the RecursiveIterator test below rather than being unwrapped further.` |
|     - |  4871 | `	 */` |
|   139 |  4872 | `	if( pAggCls && PH7_VmInstanceOf(pObj->pClass,pAggCls) ){` |
|     3 |  4873 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pObj->pClass,"getIterator",` |
|     - |  4874 | `			sizeof("getIterator")-1);` |
|     - |  4875 | `		ph7_value sInner;` |
|     3 |  4876 | `		PH7_MemObjInit(pVm,&sInner);` |
|     3 |  4877 | `		rc = pMethod ? PH7_VmCallClassMethod(pVm,pObj,pMethod,&sInner,0,0) : SXRET_OK;` |
|     3 |  4878 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  4879 | `			PH7_MemObjRelease(&sInner);` |
|   ! 0 |  4880 | `			return rc;` |
|     - |  4881 | `		}` |
|     2 |  4882 | `		if( (sInner.iFlags & MEMOBJ_OBJ) == 0 \|\| sInner.x.pOther == 0` |
|     3 |  4883 | `		 \|\| (pTravCls && !PH7_VmInstanceOf(((ph7_class_instance *)sInner.x.pOther)->pClass,pTravCls)) ){` |
|   ! 0 |  4884 | `			SyString *pName = &pObj->pClass->sName;` |
|   ! 0 |  4885 | `			PH7_MemObjRelease(&sInner);` |
|   ! 0 |  4886 | `			return PH7_VmThrowException(pCtx,"LogicException",` |
|   ! 0 |  4887 | `				"%z::getIterator() must return an object that implements Traversable",pName);` |
|     - |  4888 | `		}` |
|     3 |  4889 | `		pObj = (ph7_class_instance *)sInner.x.pOther;` |
|     3 |  4890 | `		pObj->iRef++;` |
|     3 |  4891 | `		PH7_MemObjRelease(&sInner);` |
|     3 |  4892 | `		pHold = pObj;` |
|     1 |  4893 | `	}` |
|   139 |  4894 | `	if( pRecCls == 0 \|\| !PH7_VmInstanceOf(pObj->pClass,pRecCls) ){` |
|     3 |  4895 | `		if( pHold ){` |
|   ! 0 |  4896 | `			PH7_ClassInstanceUnref(pHold);` |
|   ! 0 |  4897 | `		}` |
|     - |  4898 | `		/* php refuses here rather than from the declared type, so a plain Iterator` |
|     - |  4899 | `		 * gets this sentence and not a TypeError. */` |
|     3 |  4900 | `		return PH7_VmThrowException(pCtx,"InvalidArgumentException",` |
|     - |  4901 | `			"An instance of RecursiveIterator or IteratorAggregate creating it is required");` |
|     - |  4902 | `	}` |
|   137 |  4903 | `	RitClear(pVm,pThis);` |
|   137 |  4904 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_LVL,0);` |
|   137 |  4905 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_MD,iMode);` |
|   137 |  4906 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_FL,iFlags);` |
|   137 |  4907 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_MX,-1);` |
|   137 |  4908 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_II,0);` |
|     - |  4909 | `	{` |
|     - |  4910 | `		ph7_value sObj;` |
|   137 |  4911 | `		PH7_MemObjInit(pVm,&sObj);` |
|   137 |  4912 | `		sObj.x.pOther = pObj;` |
|   137 |  4913 | `		MemObjSetType(&sObj,MEMOBJ_OBJ);` |
|   137 |  4914 | `		RitPut(pVm,pThis,RIT_ST,0,&sObj);` |
|   137 |  4915 | `		sObj.x.pOther = 0;` |
|   137 |  4916 | `		MemObjSetType(&sObj,MEMOBJ_NULL);` |
|   137 |  4917 | `		PH7_MemObjRelease(&sObj);` |
|     - |  4918 | `	}` |
|   137 |  4919 | `	RitSetState(pVm,pThis,0,RS_START);` |
|     - |  4920 | `	/* Level 0 exists from HERE, which is what makes getDepth() answer 0 and` |
|     - |  4921 | `	 * getSubIterator() answer the root before any rewind(). */` |
|   137 |  4922 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_RD,1);` |
|   137 |  4923 | `	if( pHold ){` |
|     3 |  4924 | `		PH7_ClassInstanceUnref(pHold);` |
|     1 |  4925 | `	}` |
|    68 |  4926 | `	SXUNUSED(nArg);` |
|   137 |  4927 | `	return PH7_OK;` |
|    71 |  4928 | `}` |
|   100 |  4929 | `static int vm_builtin_RecursiveIteratorIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  4930 | `{` |
|   101 |  4931 | `	ph7_vm *pVm = pCtx->pVm;` |
|   101 |  4932 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  4933 | `	ph7_class_instance *pRoot;` |
|     - |  4934 | `	sxi32 rc;` |
|    50 |  4935 | `	SXUNUSED(nArg);` |
|    50 |  4936 | `	SXUNUSED(apArg);` |
|   101 |  4937 | `	if( !RitReady(pThis) ){` |
|     3 |  4938 | `		return RitNotReady(pCtx);` |
|     - |  4939 | `	}` |
|     - |  4940 | `	/* php pops the level FIRST and calls endChildren after, so the hook reports the` |
|     - |  4941 | `	 * depth it has landed on -- the opposite order from the traversal's own pop. */` |
|    99 |  4942 | `	while( RitInt(pThis,RIT_LVL) > 0 ){` |
|   ! 0 |  4943 | `		RitPop(pVm,pThis);` |
|   ! 0 |  4944 | `		rc = RitHook(pCtx,"endChildren",sizeof("endChildren")-1,0,FALSE,0);` |
|   ! 0 |  4945 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  4946 | `			return rc;` |
|     - |  4947 | `		}` |
|   ! 0 |  4948 | `	}` |
|    99 |  4949 | `	RitSetState(pVm,pThis,0,RS_START);` |
|    99 |  4950 | `	pRoot = RitSub(pVm,pThis,0);` |
|    99 |  4951 | `	rc = RitCall(pCtx,pRoot,"rewind",sizeof("rewind")-1,0,FALSE,0);` |
|    99 |  4952 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  4953 | `		return rc;` |
|     - |  4954 | `	}` |
|     - |  4955 | `	/* php's in_iteration latch: a second rewind() does NOT re-announce the` |
|     - |  4956 | `	 * iteration, which is the only reason the flag exists. */` |
|    99 |  4957 | `	if( !RitInt(pThis,RIT_II) ){` |
|    97 |  4958 | `		rc = RitHook(pCtx,"beginIteration",sizeof("beginIteration")-1,0,FALSE,0);` |
|    97 |  4959 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  4960 | `			PH7_NativeSetAttrInt(pVm,pThis,RIT_II,1);` |
|   ! 0 |  4961 | `			return rc;` |
|     - |  4962 | `		}` |
|    48 |  4963 | `	}` |
|    99 |  4964 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_II,1);` |
|    99 |  4965 | `	return RitMoveForward(pCtx);` |
|    51 |  4966 | `}` |
|   376 |  4967 | `static int vm_builtin_RecursiveIteratorIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  4968 | `{` |
|   377 |  4969 | `	int bValid = FALSE;` |
|     - |  4970 | `	sxi32 rc;` |
|   188 |  4971 | `	SXUNUSED(nArg);` |
|   188 |  4972 | `	SXUNUSED(apArg);` |
|   377 |  4973 | `	if( !RitReady(PH7_ContextThis(pCtx)) ){` |
|     3 |  4974 | `		return RitNotReady(pCtx);` |
|     - |  4975 | `	}` |
|   375 |  4976 | `	rc = RitValidEx(pCtx,&bValid);` |
|   375 |  4977 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  4978 | `		return rc;` |
|     - |  4979 | `	}` |
|   375 |  4980 | `	ph7_result_bool(pCtx,bValid);` |
|   375 |  4981 | `	return PH7_OK;` |
|   189 |  4982 | `}` |
|     - |  4983 | `/* current() and key() read the CURRENT LEVEL live -- php keeps no cache here, the` |
|     - |  4984 | ` * one place the recursive iterator differs from every dual iterator (rule 43). */` |
|   432 |  4985 | `static sxi32 RitCurrentLevelCall(ph7_context *pCtx,const char *zName,sxu32 nName)` |
|     1 |  4986 | `{` |
|   433 |  4987 | `	ph7_vm *pVm = pCtx->pVm;` |
|   433 |  4988 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  4989 | `	ph7_class_instance *pSub;` |
|     - |  4990 | `	ph7_value sRes;` |
|     - |  4991 | `	sxi32 rc;` |
|   433 |  4992 | `	if( !RitReady(pThis) ){` |
|   ! 0 |  4993 | `		return RitNotReady(pCtx);` |
|     - |  4994 | `	}` |
|   433 |  4995 | `	pSub = RitSub(pVm,pThis,RitInt(pThis,RIT_LVL));` |
|   433 |  4996 | `	if( pSub == 0 ){` |
|   ! 0 |  4997 | `		ph7_result_null(pCtx);` |
|   ! 0 |  4998 | `		return PH7_OK;` |
|     - |  4999 | `	}` |
|   433 |  5000 | `	PH7_MemObjInit(pVm,&sRes);` |
|   433 |  5001 | `	rc = RitCall(pCtx,pSub,zName,nName,&sRes,FALSE,0);` |
|   433 |  5002 | `	if( rc == SXRET_OK ){` |
|   433 |  5003 | `		ph7_result_value(pCtx,&sRes);` |
|   216 |  5004 | `	}` |
|   433 |  5005 | `	PH7_MemObjRelease(&sRes);` |
|   433 |  5006 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|   217 |  5007 | `}` |
|   172 |  5008 | `static int vm_builtin_RecursiveIteratorIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5009 | `{` |
|    86 |  5010 | `	SXUNUSED(nArg);` |
|    86 |  5011 | `	SXUNUSED(apArg);` |
|   173 |  5012 | `	return RitCurrentLevelCall(pCtx,"key",sizeof("key")-1);` |
|     1 |  5013 | `}` |
|   198 |  5014 | `static int vm_builtin_RecursiveIteratorIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5015 | `{` |
|    99 |  5016 | `	SXUNUSED(nArg);` |
|    99 |  5017 | `	SXUNUSED(apArg);` |
|   199 |  5018 | `	return RitCurrentLevelCall(pCtx,"current",sizeof("current")-1);` |
|     1 |  5019 | `}` |
|   294 |  5020 | `static int vm_builtin_RecursiveIteratorIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5021 | `{` |
|   147 |  5022 | `	SXUNUSED(nArg);` |
|   147 |  5023 | `	SXUNUSED(apArg);` |
|   295 |  5024 | `	if( !RitReady(PH7_ContextThis(pCtx)) ){` |
|   ! 0 |  5025 | `		return RitNotReady(pCtx);` |
|     - |  5026 | `	}` |
|   295 |  5027 | `	return RitMoveForward(pCtx);` |
|   148 |  5028 | `}` |
|   116 |  5029 | `static int vm_builtin_RecursiveIteratorIterator_getDepth(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5030 | `{` |
|   117 |  5031 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    58 |  5032 | `	SXUNUSED(nArg);` |
|    58 |  5033 | `	SXUNUSED(apArg);` |
|   117 |  5034 | `	if( !RitReady(pThis) ){` |
|     3 |  5035 | `		return RitNotReady(pCtx);` |
|     - |  5036 | `	}` |
|   115 |  5037 | `	ph7_result_int64(pCtx,(ph7_int64)RitInt(pThis,RIT_LVL));` |
|   115 |  5038 | `	return PH7_OK;` |
|    59 |  5039 | `}` |
|    20 |  5040 | `static int vm_builtin_RecursiveIteratorIterator_getSubIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5041 | `{` |
|    21 |  5042 | `	ph7_vm *pVm = pCtx->pVm;` |
|    21 |  5043 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  5044 | `	ph7_class_instance *pSub;` |
|     - |  5045 | `	int iLevel;` |
|    21 |  5046 | `	if( !RitReady(pThis) ){` |
|   ! 0 |  5047 | `		return RitNotReady(pCtx);` |
|     - |  5048 | `	}` |
|    21 |  5049 | `	iLevel = RitInt(pThis,RIT_LVL);` |
|    21 |  5050 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0 ){` |
|     9 |  5051 | `		sxi64 iWant = 0;` |
|     9 |  5052 | `		sxi32 rc = PH7_IntArgResolve(pCtx,apArg[0],"RecursiveIteratorIterator::getSubIterator",` |
|     - |  5053 | `			1,"$level","?int",&iWant);` |
|     9 |  5054 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  5055 | `			return rc;` |
|     - |  5056 | `		}` |
|     9 |  5057 | `		if( iWant < 0 \|\| iWant > (sxi64)iLevel ){` |
|     7 |  5058 | `			ph7_result_null(pCtx);` |
|     7 |  5059 | `			return PH7_OK;` |
|     - |  5060 | `		}` |
|     3 |  5061 | `		iLevel = (int)iWant;` |
|     1 |  5062 | `	}` |
|    15 |  5063 | `	pSub = RitSub(pVm,pThis,iLevel);` |
|    15 |  5064 | `	if( pSub ){` |
|    15 |  5065 | `		SplResultBorrowed(pCtx,pSub);` |
|     8 |  5066 | `	}else{` |
|   ! 0 |  5067 | `		ph7_result_null(pCtx);` |
|     - |  5068 | `	}` |
|    15 |  5069 | `	return PH7_OK;` |
|    11 |  5070 | `}` |
|     8 |  5071 | `static int vm_builtin_RecursiveIteratorIterator_getInnerIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5072 | `{` |
|     9 |  5073 | `	ph7_vm *pVm = pCtx->pVm;` |
|     9 |  5074 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  5075 | `	ph7_class_instance *pSub;` |
|     4 |  5076 | `	SXUNUSED(nArg);` |
|     4 |  5077 | `	SXUNUSED(apArg);` |
|     9 |  5078 | `	if( !RitReady(pThis) ){` |
|     3 |  5079 | `		return RitNotReady(pCtx);` |
|     - |  5080 | `	}` |
|     7 |  5081 | `	pSub = RitSub(pVm,pThis,RitInt(pThis,RIT_LVL));` |
|     7 |  5082 | `	if( pSub ){` |
|     7 |  5083 | `		SplResultBorrowed(pCtx,pSub);` |
|     4 |  5084 | `	}else{` |
|   ! 0 |  5085 | `		ph7_result_null(pCtx);` |
|     - |  5086 | `	}` |
|     7 |  5087 | `	return PH7_OK;` |
|     5 |  5088 | `}` |
|     - |  5089 | `/* The five hooks php declares with empty bodies. They exist to be OVERRIDDEN and` |
|     - |  5090 | ` * to be reachable through parent:: from an override. */` |
|   682 |  5091 | `static int vm_builtin_RecursiveIteratorIterator_nop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5092 | `{` |
|   341 |  5093 | `	SXUNUSED(nArg);` |
|   341 |  5094 | `	SXUNUSED(apArg);` |
|   683 |  5095 | `	if( !RitReady(PH7_ContextThis(pCtx)) ){` |
|   ! 0 |  5096 | `		return RitNotReady(pCtx);` |
|     - |  5097 | `	}` |
|   683 |  5098 | `	return PH7_OK;` |
|   342 |  5099 | `}` |
|     - |  5100 | `/* php's callHasChildren/callGetChildren ask the CURRENT LEVEL's iterator, which` |
|     - |  5101 | ` * is what makes them the documented interception point for both. */` |
|   364 |  5102 | `static int vm_builtin_RecursiveIteratorIterator_callHasChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5103 | `{` |
|   365 |  5104 | `	ph7_vm *pVm = pCtx->pVm;` |
|   365 |  5105 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  5106 | `	ph7_class_instance *pSub;` |
|     - |  5107 | `	ph7_value sRes;` |
|     - |  5108 | `	sxi32 rc;` |
|   182 |  5109 | `	SXUNUSED(nArg);` |
|   182 |  5110 | `	SXUNUSED(apArg);` |
|   365 |  5111 | `	if( !RitReady(pThis) ){` |
|     3 |  5112 | `		return RitNotReady(pCtx);` |
|     - |  5113 | `	}` |
|   363 |  5114 | `	pSub = RitSub(pVm,pThis,RitInt(pThis,RIT_LVL));` |
|   363 |  5115 | `	if( pSub == 0 ){` |
|   ! 0 |  5116 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  5117 | `		return PH7_OK;` |
|     - |  5118 | `	}` |
|   363 |  5119 | `	PH7_MemObjInit(pVm,&sRes);` |
|   363 |  5120 | `	rc = RitCall(pCtx,pSub,"hasChildren",sizeof("hasChildren")-1,&sRes,FALSE,0);` |
|   363 |  5121 | `	if( rc == SXRET_OK ){` |
|   363 |  5122 | `		ph7_result_bool(pCtx,ph7_value_to_bool(&sRes));` |
|   181 |  5123 | `	}` |
|   363 |  5124 | `	PH7_MemObjRelease(&sRes);` |
|   363 |  5125 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|   183 |  5126 | `}` |
|   120 |  5127 | `static int vm_builtin_RecursiveIteratorIterator_callGetChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5128 | `{` |
|   121 |  5129 | `	ph7_vm *pVm = pCtx->pVm;` |
|   121 |  5130 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  5131 | `	ph7_class_instance *pSub;` |
|     - |  5132 | `	ph7_value sRes;` |
|     - |  5133 | `	sxi32 rc;` |
|    60 |  5134 | `	SXUNUSED(nArg);` |
|    60 |  5135 | `	SXUNUSED(apArg);` |
|   121 |  5136 | `	if( !RitReady(pThis) ){` |
|   ! 0 |  5137 | `		return RitNotReady(pCtx);` |
|     - |  5138 | `	}` |
|   121 |  5139 | `	pSub = RitSub(pVm,pThis,RitInt(pThis,RIT_LVL));` |
|   121 |  5140 | `	if( pSub == 0 ){` |
|   ! 0 |  5141 | `		ph7_result_null(pCtx);` |
|   ! 0 |  5142 | `		return PH7_OK;` |
|     - |  5143 | `	}` |
|   121 |  5144 | `	PH7_MemObjInit(pVm,&sRes);` |
|   121 |  5145 | `	rc = RitCall(pCtx,pSub,"getChildren",sizeof("getChildren")-1,&sRes,FALSE,0);` |
|   121 |  5146 | `	if( rc == SXRET_OK ){` |
|   115 |  5147 | `		ph7_result_value(pCtx,&sRes);` |
|    57 |  5148 | `	}` |
|   121 |  5149 | `	PH7_MemObjRelease(&sRes);` |
|   121 |  5150 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    61 |  5151 | `}` |
|    18 |  5152 | `static int vm_builtin_RecursiveIteratorIterator_setMaxDepth(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5153 | `{` |
|    19 |  5154 | `	ph7_vm *pVm = pCtx->pVm;` |
|    19 |  5155 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    19 |  5156 | `	sxi64 iMax = -1;` |
|    19 |  5157 | `	if( !RitReady(pThis) ){` |
|   ! 0 |  5158 | `		return RitNotReady(pCtx);` |
|     - |  5159 | `	}` |
|    19 |  5160 | `	if( nArg > 0 ){` |
|    17 |  5161 | `		sxi32 rc = PH7_IntArgResolve(pCtx,apArg[0],"RecursiveIteratorIterator::setMaxDepth",` |
|     - |  5162 | `			1,"$maxDepth","int",&iMax);` |
|    17 |  5163 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  5164 | `			return rc;` |
|     - |  5165 | `		}` |
|     8 |  5166 | `	}` |
|    19 |  5167 | `	if( iMax < -1 ){` |
|     3 |  5168 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  5169 | `			"RecursiveIteratorIterator::setMaxDepth(): Argument #1 ($maxDepth) "` |
|     - |  5170 | `			"must be greater than or equal to -1");` |
|     - |  5171 | `	}` |
|    17 |  5172 | `	if( iMax > SXI32_HIGH ){` |
|   ! 0 |  5173 | `		iMax = SXI32_HIGH;   /* php clamps to INT_MAX; max_depth is an int there */` |
|   ! 0 |  5174 | `	}` |
|    17 |  5175 | `	PH7_NativeSetAttrInt(pVm,pThis,RIT_MX,iMax);` |
|    17 |  5176 | `	return PH7_OK;` |
|    10 |  5177 | `}` |
|    10 |  5178 | `static int vm_builtin_RecursiveIteratorIterator_getMaxDepth(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5179 | `{` |
|    11 |  5180 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  5181 | `	int iMax;` |
|     5 |  5182 | `	SXUNUSED(nArg);` |
|     5 |  5183 | `	SXUNUSED(apArg);` |
|    11 |  5184 | `	if( !RitReady(pThis) ){` |
|   ! 0 |  5185 | `		return RitNotReady(pCtx);` |
|     - |  5186 | `	}` |
|    11 |  5187 | `	iMax = RitInt(pThis,RIT_MX);` |
|    11 |  5188 | `	if( iMax == -1 ){` |
|     5 |  5189 | ``		ph7_result_bool(pCtx,0);   /* php's `int\|false`: false means "any depth" */`` |
|     3 |  5190 | `	}else{` |
|     7 |  5191 | `		ph7_result_int64(pCtx,(ph7_int64)iMax);` |
|     - |  5192 | `	}` |
|    11 |  5193 | `	return PH7_OK;` |
|     6 |  5194 | `}` |
|     - |  5195 | `/*` |
|     - |  5196 | ` * ---------------------------------------------------------------------------` |
|     - |  5197 | ` * RecursiveTreeIterator: the RecursiveIteratorIterator that draws the tree.` |
|     - |  5198 | ` *` |
|     - |  5199 | ` * It is the same traversal with a STRING built around each element, and the` |
|     - |  5200 | ` * drawing is why php wraps the iterator it is handed in a` |
|     - |  5201 | ` * RecursiveCachingIterator: the ASCII branches need to know whether a level has` |
|     - |  5202 | ` * a NEXT element, and hasNext() is the one question only the caching decorator` |
|     - |  5203 | ` * answers. So the sub-iterator at every level here is a` |
|     - |  5204 | ` * RecursiveCachingIterator, getSubIterator()/getInnerIterator() report one, and` |
|     - |  5205 | `` * `$cachingIteratorFlags` is what that wrapper is built with — CATCH_GET_CHILD`` |
|     - |  5206 | ` * when the caller says nothing, and EXACTLY what the caller says otherwise.` |
|     - |  5207 | ` *` |
|     - |  5208 | ` * The prefix is six parts: a fixed LEFT, one MID per level above this one` |
|     - |  5209 | ` * (chosen by whether that level has a next element), one END for this level` |
|     - |  5210 | ` * (chosen the same way) and a fixed RIGHT. current() is prefix + entry + postfix` |
|     - |  5211 | ` * and key() is prefix + key + postfix, each bypassable through its own flag —` |
|     - |  5212 | ` * and those flags live in the SAME word as RecursiveIteratorIterator's` |
|     - |  5213 | ` * CATCH_GET_CHILD, which is why php declares that constant on both classes.` |
|     - |  5214 | ` */` |
|     - |  5215 | `#define RTI_PFX "__pfx"   /* php's prefix[6] */` |
|     - |  5216 | `#define RTI_PST "__pst"   /* php's postfix */` |
|     - |  5217 |  |
|     - |  5218 | `#define RTIT_BYPASS_CURRENT     4` |
|     - |  5219 | `#define RTIT_BYPASS_KEY         8` |
|     - |  5220 | `#define RTIT_PREFIX_LEFT        0` |
|     - |  5221 | `#define RTIT_PREFIX_MID_HAS_NEXT 1` |
|     - |  5222 | `#define RTIT_PREFIX_MID_LAST    2` |
|     - |  5223 | `#define RTIT_PREFIX_END_HAS_NEXT 3` |
|     - |  5224 | `#define RTIT_PREFIX_END_LAST    4` |
|     - |  5225 | `#define RTIT_PREFIX_RIGHT       5` |
|     - |  5226 |  |
|     - |  5227 | ``/* php's `object->prefix[N]` defaults, set in the constructor. */`` |
|     - |  5228 | `static const char * const azRtiPrefix[] = { "", "\| ", "  ", "\|-", "\\-", "" };` |
|     - |  5229 |  |
|   466 |  5230 | `static ph7_value * RtiPrefixSlot(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  5231 | `{` |
|   467 |  5232 | `	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,RTI_PFX) : 0;` |
|   467 |  5233 | `	if( pSlot == 0 ){` |
|   ! 0 |  5234 | `		return 0;` |
|     - |  5235 | `	}` |
|   467 |  5236 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    47 |  5237 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|   ! 0 |  5238 | `			return 0;` |
|     - |  5239 | `		}` |
|    23 |  5240 | `	}` |
|   467 |  5241 | `	if( PH7_HashmapCowSeparate(pVm,pSlot) == 0 ){` |
|   ! 0 |  5242 | `		return 0;` |
|     - |  5243 | `	}` |
|   467 |  5244 | `	return pSlot;` |
|   234 |  5245 | `}` |
|     - |  5246 | `/* One prefix part, appended to pOut. A part nothing has written is php's own` |
|     - |  5247 | ` * default rather than the empty string. */` |
|   404 |  5248 | `static void RtiAppendPart(ph7_vm *pVm,ph7_class_instance *pThis,int iPart,ph7_value *pOut)` |
|     1 |  5249 | `{` |
|   405 |  5250 | `	ph7_value *pSlot = RtiPrefixSlot(pVm,pThis);` |
|   405 |  5251 | `	ph7_hashmap_node *pNode = 0;` |
|   405 |  5252 | `	ph7_value *pVal = 0;` |
|     - |  5253 | `	const char *zTxt;` |
|     - |  5254 | `	int nTxt;` |
|   405 |  5255 | `	if( pSlot && HashmapLookupIntKey((ph7_hashmap *)pSlot->x.pOther,(sxi64)iPart,&pNode) == SXRET_OK ){` |
|   405 |  5256 | `		pVal = HashmapExtractNodeValue(pNode);` |
|   202 |  5257 | `	}` |
|   405 |  5258 | `	if( pVal == 0 ){` |
|   ! 0 |  5259 | `		return;` |
|     - |  5260 | `	}` |
|   405 |  5261 | `	zTxt = ph7_value_to_string(pVal,&nTxt);` |
|   405 |  5262 | `	if( nTxt > 0 ){` |
|   205 |  5263 | `		PH7_MemObjStringAppend(pOut,zTxt,(sxu32)nTxt);` |
|   102 |  5264 | `	}` |
|   203 |  5265 | `}` |
|     - |  5266 | ``/* php's `hasnext` on one level's sub-iterator; a level with no answer draws`` |
|     - |  5267 | ` * nothing at all. */` |
|   180 |  5268 | `static sxi32 RtiLevelHasNext(ph7_context *pCtx,int iLevel,int *pbHas,int *pbAnswered)` |
|     1 |  5269 | `{` |
|   181 |  5270 | `	ph7_vm *pVm = pCtx->pVm;` |
|   181 |  5271 | `	ph7_class_instance *pSub = RitSub(pVm,PH7_ContextThis(pCtx),iLevel);` |
|   181 |  5272 | `	ph7_class_method *pMethod = pSub` |
|   180 |  5273 | `		? PH7_ClassExtractMethod(pSub->pClass,"hasNext",sizeof("hasNext")-1) : 0;` |
|     - |  5274 | `	ph7_value sRes;` |
|     - |  5275 | `	sxi32 rc;` |
|   181 |  5276 | `	*pbHas = 0;` |
|   181 |  5277 | `	*pbAnswered = 0;` |
|   181 |  5278 | `	if( pMethod == 0 ){` |
|   ! 0 |  5279 | `		return SXRET_OK;` |
|     - |  5280 | `	}` |
|   181 |  5281 | `	PH7_MemObjInit(pVm,&sRes);` |
|   181 |  5282 | `	rc = PH7_VmCallClassMethod(pVm,pSub,pMethod,&sRes,0,0);` |
|   181 |  5283 | `	if( rc == SXRET_OK ){` |
|   181 |  5284 | `		PH7_MemObjToBool(&sRes);          /* a STATUS, not the answer */` |
|   181 |  5285 | `		*pbHas = sRes.x.iVal != 0;` |
|   181 |  5286 | `		*pbAnswered = 1;` |
|    90 |  5287 | `	}` |
|   181 |  5288 | `	PH7_MemObjRelease(&sRes);` |
|   181 |  5289 | `	return rc;` |
|    91 |  5290 | `}` |
|   112 |  5291 | `static sxi32 RtiBuildPrefix(ph7_context *pCtx,ph7_value *pOut)` |
|     1 |  5292 | `{` |
|   113 |  5293 | `	ph7_vm *pVm = pCtx->pVm;` |
|   113 |  5294 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   113 |  5295 | `	int iLevel = RitInt(pThis,RIT_LVL);` |
|     - |  5296 | `	int i,bHas,bAnswered;` |
|     - |  5297 | `	sxi32 rc;` |
|   113 |  5298 | `	RtiAppendPart(pVm,pThis,RTIT_PREFIX_LEFT,pOut);` |
|   181 |  5299 | `	for( i = 0 ; i < iLevel ; ++i ){` |
|    69 |  5300 | `		rc = RtiLevelHasNext(pCtx,i,&bHas,&bAnswered);` |
|    69 |  5301 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  5302 | `			return rc;` |
|     - |  5303 | `		}` |
|    69 |  5304 | `		if( bAnswered ){` |
|    69 |  5305 | `			RtiAppendPart(pVm,pThis,bHas ? RTIT_PREFIX_MID_HAS_NEXT : RTIT_PREFIX_MID_LAST,pOut);` |
|    34 |  5306 | `		}` |
|    35 |  5307 | `	}` |
|   113 |  5308 | `	rc = RtiLevelHasNext(pCtx,iLevel,&bHas,&bAnswered);` |
|   113 |  5309 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  5310 | `		return rc;` |
|     - |  5311 | `	}` |
|   113 |  5312 | `	if( bAnswered ){` |
|   113 |  5313 | `		RtiAppendPart(pVm,pThis,bHas ? RTIT_PREFIX_END_HAS_NEXT : RTIT_PREFIX_END_LAST,pOut);` |
|    56 |  5314 | `	}` |
|   113 |  5315 | `	RtiAppendPart(pVm,pThis,RTIT_PREFIX_RIGHT,pOut);` |
|   113 |  5316 | `	return SXRET_OK;` |
|    57 |  5317 | `}` |
|     - |  5318 | `/*` |
|     - |  5319 | ` * php's get_entry: the CACHED current() of this level's caching iterator, as a` |
|     - |  5320 | ` * string. An ARRAY is the word "Array" and says nothing while doing it — php` |
|     - |  5321 | ` * never runs a cast here — and an object with no __toString still raises.` |
|     - |  5322 | ` */` |
|    96 |  5323 | `static sxi32 RtiBuildEntry(ph7_context *pCtx,ph7_value *pOut)` |
|     1 |  5324 | `{` |
|    97 |  5325 | `	ph7_vm *pVm = pCtx->pVm;` |
|    97 |  5326 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    97 |  5327 | `	ph7_class_instance *pSub = RitSub(pVm,pThis,RitInt(pThis,RIT_LVL));` |
|    97 |  5328 | `	ph7_class_method *pMethod = pSub` |
|    96 |  5329 | `		? PH7_ClassExtractMethod(pSub->pClass,"current",sizeof("current")-1) : 0;` |
|     - |  5330 | `	ph7_value sRes;` |
|     - |  5331 | `	sxi32 rc;` |
|    97 |  5332 | `	if( pMethod == 0 ){` |
|   ! 0 |  5333 | `		return SXRET_OK;` |
|     - |  5334 | `	}` |
|    97 |  5335 | `	PH7_MemObjInit(pVm,&sRes);` |
|    97 |  5336 | `	rc = PH7_VmCallClassMethod(pVm,pSub,pMethod,&sRes,0,0);` |
|    97 |  5337 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  5338 | `		PH7_MemObjRelease(&sRes);` |
|   ! 0 |  5339 | `		return rc;` |
|     - |  5340 | `	}` |
|    97 |  5341 | `	if( sRes.iFlags & MEMOBJ_HASHMAP ){` |
|    35 |  5342 | `		PH7_MemObjStringAppend(pOut,"Array",sizeof("Array")-1);` |
|    80 |  5343 | `	}else if( (sRes.iFlags & MEMOBJ_NULL) == 0 ){` |
|    61 |  5344 | `		rc = PH7_MemObjToStringUV(&sRes);` |
|    61 |  5345 | `		if( rc == SXRET_OK ){` |
|     - |  5346 | `			int nTxt;` |
|    59 |  5347 | `			const char *zTxt = ph7_value_to_string(&sRes,&nTxt);` |
|    59 |  5348 | `			if( nTxt > 0 ){` |
|    59 |  5349 | `				PH7_MemObjStringAppend(pOut,zTxt,(sxu32)nTxt);` |
|    29 |  5350 | `			}` |
|    29 |  5351 | `		}` |
|    30 |  5352 | `	}` |
|    97 |  5353 | `	PH7_MemObjRelease(&sRes);` |
|    97 |  5354 | `	return rc;` |
|    49 |  5355 | `}` |
|    98 |  5356 | `static void RtiAppendPostfix(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)` |
|     1 |  5357 | `{` |
|    99 |  5358 | `	ph7_value *pSlot = PH7_NativeAttr(pThis,RTI_PST);` |
|     - |  5359 | `	const char *zTxt;` |
|     - |  5360 | `	int nTxt;` |
|    99 |  5361 | `	if( pSlot == 0 ){` |
|   ! 0 |  5362 | `		return;` |
|     - |  5363 | `	}` |
|    99 |  5364 | `	zTxt = ph7_value_to_string(pSlot,&nTxt);` |
|    99 |  5365 | `	if( nTxt > 0 ){` |
|    13 |  5366 | `		PH7_MemObjStringAppend(pOut,zTxt,(sxu32)nTxt);` |
|     6 |  5367 | `	}` |
|    49 |  5368 | `	SXUNUSED(pVm);` |
|    50 |  5369 | `}` |
|    50 |  5370 | `static int vm_builtin_RecursiveTreeIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5371 | `{` |
|    51 |  5372 | `	ph7_vm *pVm = pCtx->pVm;` |
|    51 |  5373 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  5374 | `	ph7_class *pCls;` |
|     - |  5375 | `	ph7_class_method *pCons;` |
|     - |  5376 | `	ph7_class_instance *pWrap;` |
|     - |  5377 | `	ph7_value sFlags,sMode,sCache,sSrc,sWrap,*apCtor[3],*pSlot;` |
|    51 |  5378 | `	sxi64 iFlags = RTIT_BYPASS_KEY, iCache = RIT_CATCH_GET_CHILD, iMode = RIT_SELF_FIRST;` |
|     - |  5379 | `	int i;` |
|     - |  5380 | `	sxi32 rc;` |
|    51 |  5381 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|   ! 0 |  5382 | `		return PH7_OK;` |
|     - |  5383 | `	}` |
|     - |  5384 | `	/* php's ZPP is "o\|lzl" — a bare OBJECT — while the stub declares the union` |
|     - |  5385 | `` 	 * this row carries for Reflection, so the type screen stands aside (the `~` `` |
|     - |  5386 | `	 * marker) and the refusal is worded here. */` |
|    51 |  5387 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 \|\| apArg[0]->x.pOther == 0 ){` |
|     - |  5388 | `		char zGiven[64];` |
|     7 |  5389 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  5390 | `			"RecursiveTreeIterator::__construct(): Argument #1 ($iterator) "` |
|     - |  5391 | `			"must be of type object, %s given",` |
|     2 |  5392 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|     - |  5393 | `	}` |
|    47 |  5394 | `	if( nArg > 1 ){` |
|    13 |  5395 | `		iFlags = ph7_value_to_int64(apArg[1]);` |
|     6 |  5396 | `	}` |
|    47 |  5397 | `	if( nArg > 2 ){` |
|     - |  5398 | `		/* A caller's value REPLACES the CATCH_GET_CHILD default rather than joining` |
|     - |  5399 | `` 		 * it — `new RecursiveTreeIterator($it, 8, CachingIterator::TOSTRING_USE_KEY)` `` |
|     - |  5400 | `		 * builds a wrapper that does NOT catch. */` |
|     5 |  5401 | `		iCache = ph7_value_to_int64(apArg[2]);` |
|     2 |  5402 | `	}` |
|    47 |  5403 | `	if( nArg > 3 ){` |
|     3 |  5404 | `		iMode = ph7_value_to_int64(apArg[3]);` |
|     1 |  5405 | `	}` |
|     - |  5406 | `	/* The six prefix parts and the postfix php seeds every instance with. */` |
|    47 |  5407 | `	pSlot = RtiPrefixSlot(pVm,pThis);` |
|   323 |  5408 | `	for( i = 0 ; pSlot && i < (int)SX_ARRAYSIZE(azRtiPrefix) ; ++i ){` |
|     - |  5409 | `		ph7_value sPart;` |
|   277 |  5410 | `		PH7_MemObjInitFromString(pVm,&sPart,0);` |
|   277 |  5411 | `		PH7_MemObjStringAppend(&sPart,azRtiPrefix[i],(sxu32)SyStrlen(azRtiPrefix[i]));` |
|   277 |  5412 | `		ph7_array_add_intkey_elem(pSlot,i,&sPart);` |
|   277 |  5413 | `		PH7_MemObjRelease(&sPart);` |
|   139 |  5414 | `	}` |
|     - |  5415 | `	{` |
|     - |  5416 | `		ph7_value sPost;` |
|    47 |  5417 | `		PH7_MemObjInitFromString(pVm,&sPost,0);` |
|    47 |  5418 | `		DualSetSlot(pVm,pThis,RTI_PST,&sPost);` |
|    47 |  5419 | `		PH7_MemObjRelease(&sPost);` |
|     - |  5420 | `	}` |
|     - |  5421 | `	/* php wraps the iterator FIRST, so a source the wrapper refuses is reported by` |
|     - |  5422 | `	 * RecursiveCachingIterator::__construct and never reaches the traversal. */` |
|    47 |  5423 | `	pCls = PH7_VmExtractClass(pVm,"RecursiveCachingIterator",` |
|     - |  5424 | `		sizeof("RecursiveCachingIterator")-1,FALSE,0);` |
|    47 |  5425 | `	pCons = pCls ? PH7_ClassExtractMethod(pCls,"__construct",sizeof("__construct")-1) : 0;` |
|    47 |  5426 | `	if( pCons == 0 ){` |
|   ! 0 |  5427 | `		return PH7_OK;` |
|     - |  5428 | `	}` |
|    47 |  5429 | `	pWrap = PH7_NewClassInstance(pVm,pCls);` |
|    47 |  5430 | `	if( pWrap == 0 ){` |
|   ! 0 |  5431 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  5432 | `	}` |
|    47 |  5433 | `	pWrap->iRef++;` |
|     - |  5434 | `	/* php unwraps an IteratorAggregate BEFORE it wraps: the caching iterator has` |
|     - |  5435 | ``	 * to be handed the RecursiveIterator itself, so `getIterator()` runs here and`` |
|     - |  5436 | `	 * exactly once. */` |
|    47 |  5437 | `	PH7_MemObjInit(pVm,&sSrc);` |
|    47 |  5438 | `	PH7_MemObjStore(apArg[0],&sSrc);` |
|     - |  5439 | `	{` |
|    47 |  5440 | `		ph7_class *pAggCls = PH7_VmExtractClass(pVm,"IteratorAggregate",` |
|     - |  5441 | `			sizeof("IteratorAggregate")-1,FALSE,0);` |
|    47 |  5442 | `		ph7_class_instance *pSrc = (ph7_class_instance *)sSrc.x.pOther;` |
|    47 |  5443 | `		if( pAggCls && PH7_VmInstanceOf(pSrc->pClass,pAggCls) ){` |
|     3 |  5444 | `			ph7_class_method *pGet = PH7_ClassExtractMethod(pSrc->pClass,"getIterator",` |
|     - |  5445 | `				sizeof("getIterator")-1);` |
|     - |  5446 | `			ph7_value sInner;` |
|     3 |  5447 | `			PH7_MemObjInit(pVm,&sInner);` |
|     3 |  5448 | `			rc = pGet ? PH7_VmCallClassMethod(pVm,pSrc,pGet,&sInner,0,0) : SXRET_OK;` |
|     3 |  5449 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  5450 | `				PH7_MemObjRelease(&sInner);` |
|   ! 0 |  5451 | `				PH7_MemObjRelease(&sSrc);` |
|   ! 0 |  5452 | `				PH7_ClassInstanceUnref(pWrap);` |
|   ! 0 |  5453 | `				return rc;` |
|     - |  5454 | `			}` |
|     3 |  5455 | `			PH7_MemObjStore(&sInner,&sSrc);` |
|     3 |  5456 | `			PH7_MemObjRelease(&sInner);` |
|     1 |  5457 | `		}` |
|     - |  5458 | `	}` |
|    47 |  5459 | `	PH7_MemObjInitFromInt(pVm,&sCache,iCache);` |
|    47 |  5460 | `	apCtor[0] = &sSrc;` |
|    47 |  5461 | `	apCtor[1] = &sCache;` |
|    47 |  5462 | `	rc = PH7_VmCallClassMethod(pVm,pWrap,pCons,0,2,apCtor);` |
|    47 |  5463 | `	PH7_MemObjRelease(&sCache);` |
|    47 |  5464 | `	PH7_MemObjRelease(&sSrc);` |
|    47 |  5465 | `	if( rc != SXRET_OK ){` |
|     5 |  5466 | `		PH7_ClassInstanceUnref(pWrap);` |
|     5 |  5467 | `		return rc;` |
|     - |  5468 | `	}` |
|    43 |  5469 | `	PH7_MemObjInit(pVm,&sWrap);` |
|    43 |  5470 | `	sWrap.x.pOther = pWrap;` |
|    43 |  5471 | `	MemObjSetType(&sWrap,MEMOBJ_OBJ);` |
|    43 |  5472 | `	pWrap->iRef++;                    /* the temporary owns one of its own */` |
|    43 |  5473 | `	PH7_MemObjInitFromInt(pVm,&sMode,iMode);` |
|    43 |  5474 | `	PH7_MemObjInitFromInt(pVm,&sFlags,iFlags);` |
|    43 |  5475 | `	apCtor[0] = &sWrap;` |
|    43 |  5476 | `	apCtor[1] = &sMode;` |
|    43 |  5477 | `	apCtor[2] = &sFlags;` |
|    43 |  5478 | `	rc = vm_builtin_RecursiveIteratorIterator_construct(pCtx,3,apCtor);` |
|    43 |  5479 | `	PH7_MemObjRelease(&sWrap);` |
|    43 |  5480 | `	PH7_MemObjRelease(&sMode);` |
|    43 |  5481 | `	PH7_MemObjRelease(&sFlags);` |
|    43 |  5482 | `	PH7_ClassInstanceUnref(pWrap);` |
|    43 |  5483 | `	return rc;` |
|    26 |  5484 | `}` |
|    16 |  5485 | `static int vm_builtin_RecursiveTreeIterator_getPrefix(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5486 | `{` |
|     - |  5487 | `	ph7_value sOut;` |
|     - |  5488 | `	sxi32 rc;` |
|     8 |  5489 | `	SXUNUSED(nArg);` |
|     8 |  5490 | `	SXUNUSED(apArg);` |
|    17 |  5491 | `	if( !RitReady(PH7_ContextThis(pCtx)) ){` |
|     3 |  5492 | `		return RitNotReady(pCtx);` |
|     - |  5493 | `	}` |
|    15 |  5494 | `	PH7_MemObjInitFromString(pCtx->pVm,&sOut,0);` |
|    15 |  5495 | `	rc = RtiBuildPrefix(pCtx,&sOut);` |
|    15 |  5496 | `	if( rc == SXRET_OK ){` |
|    15 |  5497 | `		ph7_result_value(pCtx,&sOut);` |
|     7 |  5498 | `	}` |
|    15 |  5499 | `	PH7_MemObjRelease(&sOut);` |
|    15 |  5500 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|     9 |  5501 | `}` |
|    24 |  5502 | `static int vm_builtin_RecursiveTreeIterator_getEntry(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5503 | `{` |
|     - |  5504 | `	ph7_value sOut;` |
|     - |  5505 | `	sxi32 rc;` |
|    12 |  5506 | `	SXUNUSED(nArg);` |
|    12 |  5507 | `	SXUNUSED(apArg);` |
|    25 |  5508 | `	if( !RitReady(PH7_ContextThis(pCtx)) ){` |
|     3 |  5509 | `		return RitNotReady(pCtx);` |
|     - |  5510 | `	}` |
|    23 |  5511 | `	PH7_MemObjInitFromString(pCtx->pVm,&sOut,0);` |
|    23 |  5512 | `	rc = RtiBuildEntry(pCtx,&sOut);` |
|    23 |  5513 | `	if( rc == SXRET_OK ){` |
|    21 |  5514 | `		ph7_result_value(pCtx,&sOut);` |
|    10 |  5515 | `	}` |
|    23 |  5516 | `	PH7_MemObjRelease(&sOut);` |
|    23 |  5517 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    13 |  5518 | `}` |
|    16 |  5519 | `static int vm_builtin_RecursiveTreeIterator_getPostfix(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5520 | `{` |
|    17 |  5521 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  5522 | `	ph7_value *pSlot;` |
|     8 |  5523 | `	SXUNUSED(nArg);` |
|     8 |  5524 | `	SXUNUSED(apArg);` |
|    17 |  5525 | `	if( !RitReady(pThis) ){` |
|     3 |  5526 | `		return RitNotReady(pCtx);` |
|     - |  5527 | `	}` |
|    15 |  5528 | `	pSlot = PH7_NativeAttr(pThis,RTI_PST);` |
|    15 |  5529 | `	if( pSlot ){` |
|    15 |  5530 | `		ph7_result_value(pCtx,pSlot);` |
|     7 |  5531 | `	}` |
|    15 |  5532 | `	return PH7_OK;` |
|     9 |  5533 | `}` |
|     2 |  5534 | `static int vm_builtin_RecursiveTreeIterator_setPostfix(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5535 | `{` |
|     3 |  5536 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     3 |  5537 | `	if( nArg < 1 \|\| pThis == 0 ){` |
|   ! 0 |  5538 | `		return PH7_OK;` |
|     - |  5539 | `	}` |
|     3 |  5540 | `	DualSetSlot(pCtx->pVm,pThis,RTI_PST,apArg[0]);` |
|     3 |  5541 | `	return PH7_OK;` |
|     2 |  5542 | `}` |
|    20 |  5543 | `static int vm_builtin_RecursiveTreeIterator_setPrefixPart(ph7_context *pCtx,int nArg,` |
|     - |  5544 | `	ph7_value **apArg)` |
|     1 |  5545 | `{` |
|    21 |  5546 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  5547 | `	ph7_value *pSlot;` |
|     - |  5548 | `	sxi64 iPart;` |
|    21 |  5549 | `	if( nArg < 2 \|\| pThis == 0 ){` |
|   ! 0 |  5550 | `		return PH7_OK;` |
|     - |  5551 | `	}` |
|    21 |  5552 | `	iPart = ph7_value_to_int64(apArg[0]);` |
|    21 |  5553 | `	if( iPart < RTIT_PREFIX_LEFT \|\| iPart > RTIT_PREFIX_RIGHT ){` |
|     5 |  5554 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  5555 | `			"RecursiveTreeIterator::setPrefixPart(): Argument #1 ($part) must be a "` |
|     - |  5556 | `			"RecursiveTreeIterator::PREFIX_* constant");` |
|     - |  5557 | `	}` |
|    17 |  5558 | `	pSlot = RtiPrefixSlot(pCtx->pVm,pThis);` |
|    17 |  5559 | `	if( pSlot ){` |
|    17 |  5560 | `		ph7_array_add_intkey_elem(pSlot,(int)iPart,apArg[1]);` |
|     8 |  5561 | `	}` |
|    17 |  5562 | `	return PH7_OK;` |
|    11 |  5563 | `}` |
|     - |  5564 | `/* php's current()/key(): the traversal's own answer, wrapped unless its BYPASS` |
|     - |  5565 | ` * flag is set. The wrapped form is always a STRING, prefix and postfix included. */` |
|   164 |  5566 | `static int RtiWrapped(ph7_context *pCtx,int bKey)` |
|     1 |  5567 | `{` |
|   165 |  5568 | `	ph7_vm *pVm = pCtx->pVm;` |
|   165 |  5569 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  5570 | `	ph7_value sOut;` |
|     - |  5571 | `	sxi32 rc;` |
|   165 |  5572 | `	if( !RitReady(pThis) ){` |
|     5 |  5573 | `		return RitNotReady(pCtx);` |
|     - |  5574 | `	}` |
|   161 |  5575 | `	if( RitInt(pThis,RIT_FL) & (bKey ? RTIT_BYPASS_KEY : RTIT_BYPASS_CURRENT) ){` |
|    51 |  5576 | `		return bKey ? RitCurrentLevelCall(pCtx,"key",sizeof("key")-1)` |
|    62 |  5577 | `		            : RitCurrentLevelCall(pCtx,"current",sizeof("current")-1);` |
|     - |  5578 | `	}` |
|    99 |  5579 | `	PH7_MemObjInitFromString(pVm,&sOut,0);` |
|    99 |  5580 | `	rc = RtiBuildPrefix(pCtx,&sOut);` |
|    99 |  5581 | `	if( rc == SXRET_OK ){` |
|    99 |  5582 | `		if( bKey ){` |
|    25 |  5583 | `			ph7_class_instance *pSub = RitSub(pVm,pThis,RitInt(pThis,RIT_LVL));` |
|    25 |  5584 | `			ph7_class_method *pMethod = pSub` |
|    24 |  5585 | `				? PH7_ClassExtractMethod(pSub->pClass,"key",sizeof("key")-1) : 0;` |
|    25 |  5586 | `			if( pMethod ){` |
|     - |  5587 | `				ph7_value sKey;` |
|    25 |  5588 | `				PH7_MemObjInit(pVm,&sKey);` |
|    25 |  5589 | `				rc = PH7_VmCallClassMethod(pVm,pSub,pMethod,&sKey,0,0);` |
|    25 |  5590 | `				if( rc == SXRET_OK && (sKey.iFlags & MEMOBJ_NULL) == 0 ){` |
|     - |  5591 | `					int nTxt;` |
|     - |  5592 | `					const char *zTxt;` |
|    25 |  5593 | `					rc = PH7_MemObjToStringUV(&sKey);` |
|    25 |  5594 | `					zTxt = ph7_value_to_string(&sKey,&nTxt);` |
|    25 |  5595 | `					if( rc == SXRET_OK && nTxt > 0 ){` |
|    25 |  5596 | `						PH7_MemObjStringAppend(&sOut,zTxt,(sxu32)nTxt);` |
|    12 |  5597 | `					}` |
|    12 |  5598 | `				}` |
|    25 |  5599 | `				PH7_MemObjRelease(&sKey);` |
|    12 |  5600 | `			}` |
|    13 |  5601 | `		}else{` |
|    75 |  5602 | `			rc = RtiBuildEntry(pCtx,&sOut);` |
|     - |  5603 | `		}` |
|    49 |  5604 | `	}` |
|    99 |  5605 | `	if( rc == SXRET_OK ){` |
|    99 |  5606 | `		RtiAppendPostfix(pVm,pThis,&sOut);` |
|    99 |  5607 | `		ph7_result_value(pCtx,&sOut);` |
|    49 |  5608 | `	}` |
|    99 |  5609 | `	PH7_MemObjRelease(&sOut);` |
|    99 |  5610 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    83 |  5611 | `}` |
|   100 |  5612 | `static int vm_builtin_RecursiveTreeIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5613 | `{` |
|    50 |  5614 | `	SXUNUSED(nArg);` |
|    50 |  5615 | `	SXUNUSED(apArg);` |
|   101 |  5616 | `	return RtiWrapped(pCtx,FALSE);` |
|     1 |  5617 | `}` |
|    64 |  5618 | `static int vm_builtin_RecursiveTreeIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5619 | `{` |
|    32 |  5620 | `	SXUNUSED(nArg);` |
|    32 |  5621 | `	SXUNUSED(apArg);` |
|    65 |  5622 | `	return RtiWrapped(pCtx,TRUE);` |
|     1 |  5623 | `}` |
|     - |  5624 | `/*` |
|     - |  5625 | ` * The declaration. Method ORDER follows spl_iterators.stub.php line for line,` |
|     - |  5626 | ` * because that is the order Reflection reports. Every return type is php's` |
|     - |  5627 | `` * `@tentative-return-type` kind (rule 45).`` |
|     - |  5628 | ` */` |
|  5740 |  5629 | `static sxi32 VmInstallSplRecursiveIt(ph7_vm *pVm)` |
|     5 |  5630 | `{` |
|     - |  5631 | `	static const PH7_NativePropDef aRitProp[] = {` |
|     - |  5632 | `		{ RIT_ST,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  5633 | `		{ RIT_SS,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  5634 | `		{ RIT_LVL, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  5635 | `		{ RIT_MD,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  5636 | `		{ RIT_FL,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  5637 | `		{ RIT_MX,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, -1, 0, 0.0 }, 0 },` |
|     - |  5638 | `		{ RIT_II,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  5639 | `		{ RIT_RD,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  5640 | `	};` |
|     - |  5641 | `	static const PH7_NativeConstDef aRitConst[] = {` |
|     - |  5642 | `		{ "LEAVES_ONLY",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RIT_LEAVES_ONLY, 0, 0.0 },` |
|     - |  5643 | `		{ "SELF_FIRST",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RIT_SELF_FIRST, 0, 0.0 },` |
|     - |  5644 | `		{ "CHILD_FIRST",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RIT_CHILD_FIRST, 0, 0.0 },` |
|     - |  5645 | `		{ "CATCH_GET_CHILD", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RIT_CATCH_GET_CHILD, 0, 0.0 },` |
|     - |  5646 | `	};` |
|     - |  5647 | `	static const PH7_NativeMethodDef aRitMethod[] = {` |
|     - |  5648 | `		{ "__construct",      PH7_MOD_PUBLIC,` |
|     - |  5649 | ``		  /* php's stub spells the default `RecursiveIteratorIterator::LEAVES_ONLY`;`` |
|     - |  5650 | `		   * one zSig field cannot carry both the TEXT and the VALUE, and the value` |
|     - |  5651 | `		   * wins here for the same reason it does on RegexIterator's row. */` |
|     - |  5652 | `		  "Traversable $iterator, int $mode = 0, int $flags = 0", 0,` |
|     - |  5653 | `		  vm_builtin_RecursiveIteratorIterator_construct },` |
|     - |  5654 | `		{ "rewind",           PH7_MOD_PUBLIC, "", "@void",` |
|     - |  5655 | `		  vm_builtin_RecursiveIteratorIterator_rewind },` |
|     - |  5656 | `		{ "valid",            PH7_MOD_PUBLIC, "", "@bool",` |
|     - |  5657 | `		  vm_builtin_RecursiveIteratorIterator_valid },` |
|     - |  5658 | `		{ "key",              PH7_MOD_PUBLIC, "", "@mixed",` |
|     - |  5659 | `		  vm_builtin_RecursiveIteratorIterator_key },` |
|     - |  5660 | `		{ "current",          PH7_MOD_PUBLIC, "", "@mixed",` |
|     - |  5661 | `		  vm_builtin_RecursiveIteratorIterator_current },` |
|     - |  5662 | `		{ "next",             PH7_MOD_PUBLIC, "", "@void",` |
|     - |  5663 | `		  vm_builtin_RecursiveIteratorIterator_next },` |
|     - |  5664 | `		{ "getDepth",         PH7_MOD_PUBLIC, "", "@int",` |
|     - |  5665 | `		  vm_builtin_RecursiveIteratorIterator_getDepth },` |
|     - |  5666 | `		{ "getSubIterator",   PH7_MOD_PUBLIC, "?int $level = null", "@?RecursiveIterator",` |
|     - |  5667 | `		  vm_builtin_RecursiveIteratorIterator_getSubIterator },` |
|     - |  5668 | `		{ "getInnerIterator", PH7_MOD_PUBLIC, "", "@RecursiveIterator",` |
|     - |  5669 | `		  vm_builtin_RecursiveIteratorIterator_getInnerIterator },` |
|     - |  5670 | `		{ "beginIteration",   PH7_MOD_PUBLIC, "", "@void",` |
|     - |  5671 | `		  vm_builtin_RecursiveIteratorIterator_nop },` |
|     - |  5672 | `		{ "endIteration",     PH7_MOD_PUBLIC, "", "@void",` |
|     - |  5673 | `		  vm_builtin_RecursiveIteratorIterator_nop },` |
|     - |  5674 | `		{ "callHasChildren",  PH7_MOD_PUBLIC, "", "@bool",` |
|     - |  5675 | `		  vm_builtin_RecursiveIteratorIterator_callHasChildren },` |
|     - |  5676 | `		{ "callGetChildren",  PH7_MOD_PUBLIC, "", "@?RecursiveIterator",` |
|     - |  5677 | `		  vm_builtin_RecursiveIteratorIterator_callGetChildren },` |
|     - |  5678 | `		{ "beginChildren",    PH7_MOD_PUBLIC, "", "@void",` |
|     - |  5679 | `		  vm_builtin_RecursiveIteratorIterator_nop },` |
|     - |  5680 | `		{ "endChildren",      PH7_MOD_PUBLIC, "", "@void",` |
|     - |  5681 | `		  vm_builtin_RecursiveIteratorIterator_nop },` |
|     - |  5682 | `		{ "nextElement",      PH7_MOD_PUBLIC, "", "@void",` |
|     - |  5683 | `		  vm_builtin_RecursiveIteratorIterator_nop },` |
|     - |  5684 | `		{ "setMaxDepth",      PH7_MOD_PUBLIC, "int $maxDepth = -1", "@void",` |
|     - |  5685 | `		  vm_builtin_RecursiveIteratorIterator_setMaxDepth },` |
|     - |  5686 | `		{ "getMaxDepth",      PH7_MOD_PUBLIC, "", "@int\|false",` |
|     - |  5687 | `		  vm_builtin_RecursiveIteratorIterator_getMaxDepth },` |
|     - |  5688 | `	};` |
|     - |  5689 | ``	/* PH7_CLASS_NOCLONE: php refuses `clone` outright ("Trying to clone an`` |
|     - |  5690 | `	 * uncloneable object"), and a slot-by-slot copy would share one level stack --` |
|     - |  5691 | `	 * and with it one cursor -- between two traversals. */` |
|     - |  5692 | `	static const PH7_NativeConstDef aRtiConst[] = {` |
|     - |  5693 | `		{ "BYPASS_CURRENT",        PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RTIT_BYPASS_CURRENT, 0, 0.0 },` |
|     - |  5694 | `		{ "BYPASS_KEY",            PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RTIT_BYPASS_KEY, 0, 0.0 },` |
|     - |  5695 | `		{ "PREFIX_LEFT",           PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RTIT_PREFIX_LEFT, 0, 0.0 },` |
|     - |  5696 | `		{ "PREFIX_MID_HAS_NEXT",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT,` |
|     - |  5697 | `		  RTIT_PREFIX_MID_HAS_NEXT, 0, 0.0 },` |
|     - |  5698 | `		{ "PREFIX_MID_LAST",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RTIT_PREFIX_MID_LAST, 0, 0.0 },` |
|     - |  5699 | `		{ "PREFIX_END_HAS_NEXT",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT,` |
|     - |  5700 | `		  RTIT_PREFIX_END_HAS_NEXT, 0, 0.0 },` |
|     - |  5701 | `		{ "PREFIX_END_LAST",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RTIT_PREFIX_END_LAST, 0, 0.0 },` |
|     - |  5702 | `		{ "PREFIX_RIGHT",          PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RTIT_PREFIX_RIGHT, 0, 0.0 },` |
|     - |  5703 | `	};` |
|     - |  5704 | `	static const PH7_NativePropDef aRtiProp[] = {` |
|     - |  5705 | `		{ RTI_PFX, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  5706 | `		{ RTI_PST, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  5707 | `	};` |
|     - |  5708 | `	static const PH7_NativeMethodDef aRtiMethod[] = {` |
|     - |  5709 | `		{ "__construct",   PH7_MOD_PUBLIC,` |
|     - |  5710 | `		  "~RecursiveIterator\|IteratorAggregate $iterator, int $flags = 8, "` |
|     - |  5711 | `		  "int $cachingIteratorFlags = 16, int $mode = 1", 0,` |
|     - |  5712 | `		  vm_builtin_RecursiveTreeIterator_construct },` |
|     - |  5713 | `		{ "key",           PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_RecursiveTreeIterator_key },` |
|     - |  5714 | `		{ "current",       PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_RecursiveTreeIterator_current },` |
|     - |  5715 | `		{ "getPrefix",     PH7_MOD_PUBLIC, "", "@string",` |
|     - |  5716 | `		  vm_builtin_RecursiveTreeIterator_getPrefix },` |
|     - |  5717 | `		{ "setPostfix",    PH7_MOD_PUBLIC, "string $postfix", "@void",` |
|     - |  5718 | `		  vm_builtin_RecursiveTreeIterator_setPostfix },` |
|     - |  5719 | `		{ "setPrefixPart", PH7_MOD_PUBLIC, "int $part, string $value", "@void",` |
|     - |  5720 | `		  vm_builtin_RecursiveTreeIterator_setPrefixPart },` |
|     - |  5721 | `		{ "getEntry",      PH7_MOD_PUBLIC, "", "@string",` |
|     - |  5722 | `		  vm_builtin_RecursiveTreeIterator_getEntry },` |
|     - |  5723 | `		{ "getPostfix",    PH7_MOD_PUBLIC, "", "@string",` |
|     - |  5724 | `		  vm_builtin_RecursiveTreeIterator_getPostfix },` |
|     - |  5725 | `	};` |
|     - |  5726 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - |  5727 | `		{ "RecursiveIteratorIterator", 0, "OuterIterator", PH7_CLASS_NOCLONE,` |
|     - |  5728 | `		  aRitMethod, SX_ARRAYSIZE(aRitMethod),` |
|     - |  5729 | `		  aRitConst, SX_ARRAYSIZE(aRitConst),` |
|     - |  5730 | `		  aRitProp, SX_ARRAYSIZE(aRitProp), 0, 0, 0 },` |
|     - |  5731 | `		/* Its four inherited mode constants come with the parent; the eight below` |
|     - |  5732 | `		 * are its own, and BYPASS_* share the flags word CATCH_GET_CHILD lives in. */` |
|     - |  5733 | `		{ "RecursiveTreeIterator", "RecursiveIteratorIterator", 0, PH7_CLASS_NOCLONE,` |
|     - |  5734 | `		  aRtiMethod, SX_ARRAYSIZE(aRtiMethod),` |
|     - |  5735 | `		  aRtiConst, SX_ARRAYSIZE(aRtiConst),` |
|     - |  5736 | `		  aRtiProp, SX_ARRAYSIZE(aRtiProp), 0, 0, 0 },` |
|     - |  5737 | `	};` |
|  5745 |  5738 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 |  5739 | `}` |
|     - |  5740 | `/*` |
|     - |  5741 | ` * ---------------------------------------------------------------------------` |
|     - |  5742 | ` * SplDoublyLinkedList, SplStack and SplQueue.` |
|     - |  5743 | ` *` |
|     - |  5744 | `` * php's `spl_dllist_object` is a linked list plus a FLAGS word, and the flags are`` |
|     - |  5745 | ` * where the family's shape lives: SPL_DLLIST_IT_LIFO (2) and IT_DELETE (1) are the` |
|     - |  5746 | ` * iteration mode, and IT_FIX (4) is a bit no CONSTANT names and no user can set --` |
|     - |  5747 | ` * the object handler stamps it at creation for SplStack and SplQueue, and it is` |
|     - |  5748 | ``  * both what freezes their LIFO/FIFO choice and why `(new SplStack)->getIteratorMode()` `` |
|     - |  5749 | ` * answers 6 rather than 2. The chunk had no notion of it, so both classes reported` |
|     - |  5750 | `` * the wrong mode and `setIteratorMode()` reported the wrong result.`` |
|     - |  5751 | ` *` |
|     - |  5752 | ` * Two rules follow from php's own code and neither is guessable from the methods:` |
|     - |  5753 | ` *` |
|     - |  5754 | ` *   - **every ArrayAccess offset is measured from the END of a LIFO list.**` |
|     - |  5755 | `` *     php resolves them through `spl_ptr_llist_offset(llist, offset, flags & LIFO)`,`` |
|     - |  5756 | `` *     so `$stack[0]` is the element `top()` answers, not the one `bottom()` does.`` |
|     - |  5757 | ` *     The chunk indexed the backing array directly and had the whole SplStack` |
|     - |  5758 | ` *     subscript surface reversed.` |
|     - |  5759 | ` *   - **the traverse POSITION is the list index in both modes.** A LIFO rewind` |
|     - |  5760 | `` *     seeds `count-1` and counts down, a FIFO rewind seeds 0 and counts up, so`` |
|     - |  5761 | `` *     `key()` and the element's own place in the list agree either way -- except`` |
|     - |  5762 | ` *     under IT_DELETE in FIFO order, where php consumes the head and deliberately` |
|     - |  5763 | ` *     does NOT advance the position (every element reports key 0).` |
|     - |  5764 | ` *` |
|     - |  5765 | `` * `toArray()` was a PHL INVENTION -- php has no such method on any of the three --`` |
|     - |  5766 | `` * and it is gone. What php has instead, and the chunk had none of: `__debugInfo()`,`` |
|     - |  5767 | `` * the `Serializable` interface with its `serialize()`/`unserialize()` pair, and the`` |
|     - |  5768 | `` * `__serialize()`/`__unserialize()` pair that php actually uses (which is why the`` |
|     - |  5769 | `` * serialized form is `O:19:"SplDoublyLinkedList":3:{i:0;…}` and not a property dump).`` |
|     - |  5770 | ` *` |
|     - |  5771 | ` * The store is a php array in a hidden slot, head->tail, so push/pop/shift/unshift` |
|     - |  5772 | ` * are the engine's OWN array builtins called with the slot (rule 7, and rule 39's` |
|     - |  5773 | ` * reference rule already lives inside them). php's element-POINTER cursor is not` |
|     - |  5774 | ` * modelled: a manual walk that mutates the list under itself resolves by position` |
|     - |  5775 | ` * here and by identity there. That is one probe line (§7.4) and the only one.` |
|     - |  5776 | ` */` |
|     - |  5777 | `#define DLL_Q  "__q"   /* php's llist, head -> tail */` |
|     - |  5778 | `#define DLL_FL "__fl"  /* php's flags word, IT_FIX included */` |
|     - |  5779 | `#define DLL_I  "__i"   /* php's traverse_position */` |
|     - |  5780 |  |
|     - |  5781 | `#define DLL_IT_DELETE 1` |
|     - |  5782 | `#define DLL_IT_LIFO   2` |
|     - |  5783 | `#define DLL_IT_FIX    4   /* php's SPL_DLLIST_IT_FIX: stamped at creation, never by a user */` |
|     - |  5784 | `#define DLL_IT_MASK   3` |
|     - |  5785 |  |
|   582 |  5786 | `static ph7_value * DllSlot(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  5787 | `{` |
|   583 |  5788 | `	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,DLL_Q) : 0;` |
|   583 |  5789 | `	if( pSlot == 0 ){` |
|   ! 0 |  5790 | `		return 0;` |
|     - |  5791 | `	}` |
|   583 |  5792 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    89 |  5793 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|   ! 0 |  5794 | `			return 0;` |
|     - |  5795 | `		}` |
|    44 |  5796 | `	}` |
|   583 |  5797 | `	if( PH7_HashmapCowSeparate(pVm,pSlot) == 0 ){` |
|   ! 0 |  5798 | `		return 0;` |
|     - |  5799 | `	}` |
|   583 |  5800 | `	return pSlot;` |
|   292 |  5801 | `}` |
|   328 |  5802 | `static ph7_hashmap * DllMap(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  5803 | `{` |
|   329 |  5804 | `	ph7_value *pSlot = DllSlot(pVm,pThis);` |
|   329 |  5805 | `	return pSlot ? (ph7_hashmap *)pSlot->x.pOther : 0;` |
|     1 |  5806 | `}` |
|   238 |  5807 | `static sxi64 DllCount(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  5808 | `{` |
|   239 |  5809 | `	ph7_hashmap *pMap = DllMap(pVm,pThis);` |
|   239 |  5810 | `	return pMap ? (sxi64)pMap->nEntry : 0;` |
|     1 |  5811 | `}` |
|   162 |  5812 | `static int DllFlags(ph7_class_instance *pThis)` |
|     1 |  5813 | `{` |
|   163 |  5814 | `	return pThis ? (int)PH7_NativeAttrInt(pThis,DLL_FL) : 0;` |
|     1 |  5815 | `}` |
|     - |  5816 | `/* The value at a LIST index (head = 0), or NULL. */` |
|    86 |  5817 | `static ph7_value * DllAt(ph7_vm *pVm,ph7_class_instance *pThis,sxi64 iIndex)` |
|     1 |  5818 | `{` |
|    87 |  5819 | `	ph7_hashmap *pMap = DllMap(pVm,pThis);` |
|    87 |  5820 | `	ph7_hashmap_node *pNode = 0;` |
|    87 |  5821 | `	if( pMap == 0 \|\| HashmapLookupIntKey(pMap,iIndex,&pNode) != SXRET_OK ){` |
|   ! 0 |  5822 | `		return 0;` |
|     - |  5823 | `	}` |
|    87 |  5824 | `	return HashmapExtractNodeValue(pNode);` |
|    44 |  5825 | `}` |
|     - |  5826 | `/*` |
|     - |  5827 | ` * php's spl_ptr_llist_offset: an ArrayAccess offset counts from the TAIL when the` |
|     - |  5828 | ` * list iterates LIFO. Every offsetGet/offsetSet/offsetUnset/add goes through here.` |
|     - |  5829 | ` */` |
|    24 |  5830 | `static sxi64 DllOffsetToIndex(ph7_class_instance *pThis,sxi64 iOffset,sxi64 nCount)` |
|     1 |  5831 | `{` |
|    25 |  5832 | `	if( DllFlags(pThis) & DLL_IT_LIFO ){` |
|    11 |  5833 | `		return nCount - 1 - iOffset;` |
|     - |  5834 | `	}` |
|    15 |  5835 | `	return iOffset;` |
|    13 |  5836 | `}` |
|     - |  5837 | `/* Hand one of the engine's own array builtins this instance's storage slot. */` |
|   234 |  5838 | `static int DllArrayCall(ph7_context *pCtx,ProchHostFunction xFunc,ph7_value **apExtra,int nExtra)` |
|     1 |  5839 | `{` |
|   235 |  5840 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  5841 | `	ph7_value *apCall[4];` |
|   235 |  5842 | `	ph7_value *pSlot = DllSlot(pCtx->pVm,pThis);` |
|     - |  5843 | `	int i;` |
|   235 |  5844 | `	if( pSlot == 0 ){` |
|   ! 0 |  5845 | `		return PH7_OK;` |
|     - |  5846 | `	}` |
|   235 |  5847 | `	apCall[0] = pSlot;` |
|   449 |  5848 | `	for( i = 0 ; i < nExtra && i < 3 ; ++i ){` |
|   215 |  5849 | `		apCall[i+1] = apExtra[i];` |
|   108 |  5850 | `	}` |
|   235 |  5851 | `	return xFunc(pCtx,nExtra+1,apCall);` |
|   118 |  5852 | `}` |
|     - |  5853 | `/* php's four "empty datastructure" refusals, which differ only in the verb. */` |
|    18 |  5854 | `static sxi32 DllEmpty(ph7_context *pCtx,const char *zVerb)` |
|     1 |  5855 | `{` |
|    28 |  5856 | `	return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     9 |  5857 | `		"Can't %s an empty datastructure",zVerb);` |
|     1 |  5858 | `}` |
|     - |  5859 | `/*` |
|     - |  5860 | ` * php words every out-of-range offset from the DECLARING class, not the runtime` |
|     - |  5861 | `` * one: `SplStack::add()` on an out-of-range index still says`` |
|     - |  5862 | `` * `SplDoublyLinkedList::add()`. The chunk used get_class($this) and reported the`` |
|     - |  5863 | ` * subclass.` |
|     - |  5864 | ` */` |
|    14 |  5865 | `static sxi32 DllOutOfRange(ph7_context *pCtx,const char *zMethod)` |
|     1 |  5866 | `{` |
|    22 |  5867 | `	return PH7_VmThrowException(pCtx,"OutOfRangeException",` |
|     7 |  5868 | `		"SplDoublyLinkedList::%s(): Argument #1 ($index) is out of range",zMethod);` |
|     1 |  5869 | `}` |
|     - |  5870 | `/*` |
|     - |  5871 | ``  * php's ZPP for the four ArrayAccess offsets and add(): the stub leaves `$index` `` |
|     - |  5872 | ` * UNTYPED (which is what Reflection prints) while the ZPP is Z_PARAM_LONG, whose` |
|     - |  5873 | `` * TypeError says `must be of type int`. An untyped signature is not screened`` |
|     - |  5874 | ` * centrally, so the rule is applied here — rule 41's disagreement, resolved without` |
|     - |  5875 | ` * an azSelfChecked[] row because the declared type is absent rather than different.` |
|     - |  5876 | ` */` |
|    46 |  5877 | `static sxi32 DllIndexArg(ph7_context *pCtx,const char *zMethod,ph7_value *pArg,sxi64 *piOut)` |
|     1 |  5878 | `{` |
|     - |  5879 | `	char zFunc[64];` |
|    47 |  5880 | `	SyBufferFormat(zFunc,sizeof(zFunc),"SplDoublyLinkedList::%s",zMethod);` |
|    47 |  5881 | `	return PH7_IntArgResolve(pCtx,pArg,zFunc,1,"$index","int",piOut);` |
|     1 |  5882 | `}` |
|   198 |  5883 | `static int vm_builtin_SplDll_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5884 | `{` |
|     - |  5885 | `	ph7_value *apExtra[1];` |
|   199 |  5886 | `	if( nArg < 1 ){` |
|   ! 0 |  5887 | `		return PH7_OK;` |
|     - |  5888 | `	}` |
|   199 |  5889 | `	apExtra[0] = apArg[0];` |
|   199 |  5890 | `	DllArrayCall(pCtx,ph7_hashmap_push,apExtra,1);` |
|   199 |  5891 | `	ph7_result_null(pCtx);   /* array_push answers the new count; php's push is void */` |
|   199 |  5892 | `	return PH7_OK;` |
|   100 |  5893 | `}` |
|     2 |  5894 | `static int vm_builtin_SplDll_unshift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5895 | `{` |
|     - |  5896 | `	ph7_value *apExtra[1];` |
|     3 |  5897 | `	if( nArg < 1 ){` |
|   ! 0 |  5898 | `		return PH7_OK;` |
|     - |  5899 | `	}` |
|     3 |  5900 | `	apExtra[0] = apArg[0];` |
|     3 |  5901 | `	DllArrayCall(pCtx,ph7_hashmap_unshift,apExtra,1);` |
|     3 |  5902 | `	ph7_result_null(pCtx);` |
|     3 |  5903 | `	return PH7_OK;` |
|     2 |  5904 | `}` |
|     6 |  5905 | `static int vm_builtin_SplDll_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5906 | `{` |
|     3 |  5907 | `	SXUNUSED(nArg);` |
|     3 |  5908 | `	SXUNUSED(apArg);` |
|     7 |  5909 | `	if( DllCount(pCtx->pVm,PH7_ContextThis(pCtx)) == 0 ){` |
|     5 |  5910 | `		return DllEmpty(pCtx,"pop from");` |
|     - |  5911 | `	}` |
|     3 |  5912 | `	return DllArrayCall(pCtx,ph7_hashmap_pop,0,0);` |
|     4 |  5913 | `}` |
|    10 |  5914 | `static int vm_builtin_SplDll_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5915 | `{` |
|     5 |  5916 | `	SXUNUSED(nArg);` |
|     5 |  5917 | `	SXUNUSED(apArg);` |
|    11 |  5918 | `	if( DllCount(pCtx->pVm,PH7_ContextThis(pCtx)) == 0 ){` |
|     7 |  5919 | `		return DllEmpty(pCtx,"shift from");` |
|     - |  5920 | `	}` |
|     5 |  5921 | `	return DllArrayCall(pCtx,ph7_hashmap_shift,0,0);` |
|     6 |  5922 | `}` |
|     - |  5923 | `/* top() is the TAIL and bottom() the HEAD, whatever the iteration mode: php reads` |
|     - |  5924 | ` * llist->tail/llist->head directly and never consults the flags here. */` |
|    10 |  5925 | `static int vm_builtin_SplDll_top(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5926 | `{` |
|    11 |  5927 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    11 |  5928 | `	sxi64 nCount = DllCount(pCtx->pVm,pThis);` |
|     - |  5929 | `	ph7_value *pVal;` |
|     5 |  5930 | `	SXUNUSED(nArg);` |
|     5 |  5931 | `	SXUNUSED(apArg);` |
|    11 |  5932 | `	if( nCount == 0 ){` |
|     5 |  5933 | `		return DllEmpty(pCtx,"peek at");` |
|     - |  5934 | `	}` |
|     7 |  5935 | `	pVal = DllAt(pCtx->pVm,pThis,nCount-1);` |
|     7 |  5936 | `	if( pVal ){` |
|     7 |  5937 | `		ph7_result_value(pCtx,pVal);` |
|     3 |  5938 | `	}` |
|     7 |  5939 | `	return PH7_OK;` |
|     6 |  5940 | `}` |
|    10 |  5941 | `static int vm_builtin_SplDll_bottom(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5942 | `{` |
|    11 |  5943 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  5944 | `	ph7_value *pVal;` |
|     5 |  5945 | `	SXUNUSED(nArg);` |
|     5 |  5946 | `	SXUNUSED(apArg);` |
|    11 |  5947 | `	if( DllCount(pCtx->pVm,pThis) == 0 ){` |
|     5 |  5948 | `		return DllEmpty(pCtx,"peek at");` |
|     - |  5949 | `	}` |
|     7 |  5950 | `	pVal = DllAt(pCtx->pVm,pThis,0);` |
|     7 |  5951 | `	if( pVal ){` |
|     7 |  5952 | `		ph7_result_value(pCtx,pVal);` |
|     3 |  5953 | `	}` |
|     7 |  5954 | `	return PH7_OK;` |
|     6 |  5955 | `}` |
|    16 |  5956 | `static int vm_builtin_SplDll_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5957 | `{` |
|     8 |  5958 | `	SXUNUSED(nArg);` |
|     8 |  5959 | `	SXUNUSED(apArg);` |
|    17 |  5960 | `	ph7_result_int64(pCtx,DllCount(pCtx->pVm,PH7_ContextThis(pCtx)));` |
|    17 |  5961 | `	return PH7_OK;` |
|     1 |  5962 | `}` |
|     2 |  5963 | `static int vm_builtin_SplDll_isEmpty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5964 | `{` |
|     1 |  5965 | `	SXUNUSED(nArg);` |
|     1 |  5966 | `	SXUNUSED(apArg);` |
|     3 |  5967 | `	ph7_result_bool(pCtx,DllCount(pCtx->pVm,PH7_ContextThis(pCtx)) == 0);` |
|     3 |  5968 | `	return PH7_OK;` |
|     1 |  5969 | `}` |
|    22 |  5970 | `static int vm_builtin_SplDll_setIteratorMode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  5971 | `{` |
|    23 |  5972 | `	ph7_vm *pVm = pCtx->pVm;` |
|    23 |  5973 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    23 |  5974 | `	int iFlags = DllFlags(pThis);` |
|    23 |  5975 | `	sxi64 iMode = 0;` |
|     - |  5976 | `	sxi32 rc;` |
|    23 |  5977 | `	if( pThis == 0 ){` |
|   ! 0 |  5978 | `		return PH7_OK;` |
|     - |  5979 | `	}` |
|    23 |  5980 | `	if( nArg < 1 ){` |
|   ! 0 |  5981 | `		return PH7_OK;   /* the arity screen already refused */` |
|     - |  5982 | `	}` |
|    23 |  5983 | `	rc = PH7_IntArgResolve(pCtx,apArg[0],"SplDoublyLinkedList::setIteratorMode",1,"$mode","int",&iMode);` |
|    23 |  5984 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  5985 | `		return rc;` |
|     - |  5986 | `	}` |
|    23 |  5987 | `	if( (iFlags & DLL_IT_FIX) && (iFlags & DLL_IT_LIFO) != ((int)iMode & DLL_IT_LIFO) ){` |
|     7 |  5988 | `		return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     - |  5989 | `			"Iterators' LIFO/FIFO modes for SplStack/SplQueue objects are frozen");` |
|     - |  5990 | `	}` |
|     - |  5991 | `	/* php MASKS the value to the two mode bits and re-adds IT_FIX, so a nonsense` |
|     - |  5992 | `	 * mode is silently reduced rather than refused — and the ANSWER is the stored` |
|     - |  5993 | `	 * word, which is how a caller sees the fix bit at all. */` |
|    17 |  5994 | `	iFlags = ((int)iMode & DLL_IT_MASK) \| (iFlags & DLL_IT_FIX);` |
|    17 |  5995 | `	PH7_NativeSetAttrInt(pVm,pThis,DLL_FL,iFlags);` |
|    17 |  5996 | `	ph7_result_int64(pCtx,(ph7_int64)iFlags);` |
|    17 |  5997 | `	return PH7_OK;` |
|    12 |  5998 | `}` |
|    12 |  5999 | `static int vm_builtin_SplDll_getIteratorMode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6000 | `{` |
|     6 |  6001 | `	SXUNUSED(nArg);` |
|     6 |  6002 | `	SXUNUSED(apArg);` |
|    13 |  6003 | `	ph7_result_int64(pCtx,(ph7_int64)DllFlags(PH7_ContextThis(pCtx)));` |
|    13 |  6004 | `	return PH7_OK;` |
|     1 |  6005 | `}` |
|     6 |  6006 | `static int vm_builtin_SplDll_add(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6007 | `{` |
|     7 |  6008 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 |  6009 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     7 |  6010 | `	sxi64 nCount = DllCount(pVm,pThis);` |
|     7 |  6011 | `	sxi64 iIndex = 0;` |
|     - |  6012 | `	ph7_value sOff,sLen,sRep,*apExtra[3];` |
|     - |  6013 | `	ph7_hashmap *pRep;` |
|     - |  6014 | `	sxi32 rc;` |
|     7 |  6015 | `	if( nArg < 2 ){` |
|   ! 0 |  6016 | `		return PH7_OK;` |
|     - |  6017 | `	}` |
|     7 |  6018 | `	rc = DllIndexArg(pCtx,"add",apArg[0],&iIndex);` |
|     7 |  6019 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  6020 | `		return rc;` |
|     - |  6021 | `	}` |
|     7 |  6022 | `	if( iIndex < 0 \|\| iIndex > nCount ){` |
|     5 |  6023 | `		return DllOutOfRange(pCtx,"add");` |
|     - |  6024 | `	}` |
|     3 |  6025 | `	if( iIndex == nCount ){` |
|     - |  6026 | `		/* php: "the last entry + 1" is a push, because there is nothing to insert` |
|     - |  6027 | `		 * before. Note this is the LIST tail in both modes. */` |
|     - |  6028 | `		ph7_value *apOne[1];` |
|   ! 0 |  6029 | `		apOne[0] = apArg[1];` |
|   ! 0 |  6030 | `		DllArrayCall(pCtx,ph7_hashmap_push,apOne,1);` |
|   ! 0 |  6031 | `		ph7_result_null(pCtx);` |
|   ! 0 |  6032 | `		return PH7_OK;` |
|     - |  6033 | `	}` |
|     3 |  6034 | `	pRep = PH7_NewHashmap(pVm,0,0);` |
|     3 |  6035 | `	if( pRep == 0 ){` |
|   ! 0 |  6036 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  6037 | `	}` |
|     3 |  6038 | `	PH7_MemObjInit(pVm,&sRep);` |
|     3 |  6039 | `	sRep.x.pOther = pRep;` |
|     3 |  6040 | `	MemObjSetType(&sRep,MEMOBJ_HASHMAP);` |
|     3 |  6041 | `	PH7_HashmapInsert(pRep,0,apArg[1]);` |
|     3 |  6042 | `	PH7_MemObjInitFromInt(pVm,&sOff,DllOffsetToIndex(pThis,iIndex,nCount));` |
|     3 |  6043 | `	PH7_MemObjInitFromInt(pVm,&sLen,0);` |
|     3 |  6044 | `	apExtra[0] = &sOff;` |
|     3 |  6045 | `	apExtra[1] = &sLen;` |
|     3 |  6046 | `	apExtra[2] = &sRep;` |
|     3 |  6047 | `	DllArrayCall(pCtx,ph7_hashmap_splice,apExtra,3);` |
|     3 |  6048 | `	PH7_MemObjRelease(&sOff);` |
|     3 |  6049 | `	PH7_MemObjRelease(&sLen);` |
|     3 |  6050 | `	PH7_MemObjRelease(&sRep);` |
|     3 |  6051 | `	ph7_result_null(pCtx);   /* array_splice answers what it removed; add() is void */` |
|     3 |  6052 | `	return PH7_OK;` |
|     4 |  6053 | `}` |
|     6 |  6054 | `static int vm_builtin_SplDll_offsetExists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6055 | `{` |
|     7 |  6056 | `	sxi64 iIndex = 0;` |
|     - |  6057 | `	sxi32 rc;` |
|     7 |  6058 | `	if( nArg < 1 ){` |
|   ! 0 |  6059 | `		return PH7_OK;` |
|     - |  6060 | `	}` |
|     7 |  6061 | `	rc = DllIndexArg(pCtx,"offsetExists",apArg[0],&iIndex);` |
|     7 |  6062 | `	if( rc != SXRET_OK ){` |
|     3 |  6063 | `		return rc;` |
|     - |  6064 | `	}` |
|     5 |  6065 | `	ph7_result_bool(pCtx,iIndex >= 0 && iIndex < DllCount(pCtx->pVm,PH7_ContextThis(pCtx)));` |
|     5 |  6066 | `	return PH7_OK;` |
|     4 |  6067 | `}` |
|    22 |  6068 | `static int vm_builtin_SplDll_offsetGet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6069 | `{` |
|    23 |  6070 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    23 |  6071 | `	sxi64 nCount = DllCount(pCtx->pVm,pThis);` |
|    23 |  6072 | `	sxi64 iIndex = 0;` |
|     - |  6073 | `	ph7_value *pVal;` |
|     - |  6074 | `	sxi32 rc;` |
|    23 |  6075 | `	if( nArg < 1 ){` |
|   ! 0 |  6076 | `		return PH7_OK;` |
|     - |  6077 | `	}` |
|    23 |  6078 | `	rc = DllIndexArg(pCtx,"offsetGet",apArg[0],&iIndex);` |
|    23 |  6079 | `	if( rc != SXRET_OK ){` |
|     3 |  6080 | `		return rc;` |
|     - |  6081 | `	}` |
|    21 |  6082 | `	if( iIndex < 0 \|\| iIndex >= nCount ){` |
|     5 |  6083 | `		return DllOutOfRange(pCtx,"offsetGet");` |
|     - |  6084 | `	}` |
|    17 |  6085 | `	pVal = DllAt(pCtx->pVm,pThis,DllOffsetToIndex(pThis,iIndex,nCount));` |
|    17 |  6086 | `	if( pVal ){` |
|    17 |  6087 | `		ph7_result_value(pCtx,pVal);` |
|     8 |  6088 | `	}` |
|    17 |  6089 | `	return PH7_OK;` |
|    12 |  6090 | `}` |
|     6 |  6091 | `static int vm_builtin_SplDll_offsetSet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6092 | `{` |
|     7 |  6093 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 |  6094 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     7 |  6095 | `	sxi64 nCount = DllCount(pVm,pThis);` |
|     7 |  6096 | `	sxi64 iIndex = 0;` |
|     - |  6097 | `	ph7_hashmap *pMap;` |
|     7 |  6098 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  6099 | `	ph7_value sKey;` |
|     - |  6100 | `	sxi32 rc;` |
|     7 |  6101 | `	if( nArg < 2 ){` |
|   ! 0 |  6102 | `		return PH7_OK;` |
|     - |  6103 | `	}` |
|     7 |  6104 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|     - |  6105 | ``		/* php: a null offset is `$dll[] = v`, which pushes. */`` |
|     - |  6106 | `		ph7_value *apOne[1];` |
|   ! 0 |  6107 | `		apOne[0] = apArg[1];` |
|   ! 0 |  6108 | `		DllArrayCall(pCtx,ph7_hashmap_push,apOne,1);` |
|   ! 0 |  6109 | `		ph7_result_null(pCtx);` |
|   ! 0 |  6110 | `		return PH7_OK;` |
|     - |  6111 | `	}` |
|     7 |  6112 | `	rc = DllIndexArg(pCtx,"offsetSet",apArg[0],&iIndex);` |
|     7 |  6113 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  6114 | `		return rc;` |
|     - |  6115 | `	}` |
|     7 |  6116 | `	if( iIndex < 0 \|\| iIndex >= nCount ){` |
|     5 |  6117 | `		return DllOutOfRange(pCtx,"offsetSet");` |
|     - |  6118 | `	}` |
|     3 |  6119 | `	pMap = DllMap(pVm,pThis);` |
|     3 |  6120 | `	PH7_MemObjInitFromInt(pVm,&sKey,DllOffsetToIndex(pThis,iIndex,nCount));` |
|     3 |  6121 | `	if( pMap && PH7_HashmapLookup(pMap,&sKey,&pNode) == SXRET_OK ){` |
|     3 |  6122 | `		ph7_value *pDest = HashmapExtractNodeValue(pNode);` |
|     3 |  6123 | `		if( pDest ){` |
|     3 |  6124 | `			PH7_MemObjStore(apArg[1],pDest);` |
|     1 |  6125 | `		}` |
|     1 |  6126 | `	}` |
|     3 |  6127 | `	PH7_MemObjRelease(&sKey);` |
|     3 |  6128 | `	return PH7_OK;` |
|     4 |  6129 | `}` |
|     6 |  6130 | `static int vm_builtin_SplDll_offsetUnset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6131 | `{` |
|     7 |  6132 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 |  6133 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     7 |  6134 | `	sxi64 nCount = DllCount(pVm,pThis);` |
|     7 |  6135 | `	sxi64 iIndex = 0;` |
|     - |  6136 | `	ph7_value sOff,sLen,*apExtra[2];` |
|     - |  6137 | `	sxi32 rc;` |
|     7 |  6138 | `	if( nArg < 1 ){` |
|   ! 0 |  6139 | `		return PH7_OK;` |
|     - |  6140 | `	}` |
|     7 |  6141 | `	rc = DllIndexArg(pCtx,"offsetUnset",apArg[0],&iIndex);` |
|     7 |  6142 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  6143 | `		return rc;` |
|     - |  6144 | `	}` |
|     7 |  6145 | `	if( iIndex < 0 \|\| iIndex >= nCount ){` |
|     3 |  6146 | `		return DllOutOfRange(pCtx,"offsetUnset");` |
|     - |  6147 | `	}` |
|     5 |  6148 | `	PH7_MemObjInitFromInt(pVm,&sOff,DllOffsetToIndex(pThis,iIndex,nCount));` |
|     5 |  6149 | `	PH7_MemObjInitFromInt(pVm,&sLen,1);` |
|     5 |  6150 | `	apExtra[0] = &sOff;` |
|     5 |  6151 | `	apExtra[1] = &sLen;` |
|     5 |  6152 | `	DllArrayCall(pCtx,ph7_hashmap_splice,apExtra,2);` |
|     5 |  6153 | `	PH7_MemObjRelease(&sOff);` |
|     5 |  6154 | `	PH7_MemObjRelease(&sLen);` |
|     5 |  6155 | `	ph7_result_null(pCtx);` |
|     5 |  6156 | `	return PH7_OK;` |
|     4 |  6157 | `}` |
|     - |  6158 | `/*` |
|     - |  6159 | ` * The cursor. php's traverse_position IS the list index in both directions — a` |
|     - |  6160 | ` * LIFO rewind seeds count-1 and counts down — so current() and key() need no mode` |
|     - |  6161 | ` * test at all. IT_DELETE is the exception: in FIFO order php consumes the head and` |
|     - |  6162 | ` * leaves the position alone, so every element of a consuming walk reports key 0.` |
|     - |  6163 | ` */` |
|    24 |  6164 | `static int vm_builtin_SplDll_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6165 | `{` |
|    25 |  6166 | `	ph7_vm *pVm = pCtx->pVm;` |
|    25 |  6167 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    12 |  6168 | `	SXUNUSED(nArg);` |
|    12 |  6169 | `	SXUNUSED(apArg);` |
|    25 |  6170 | `	if( pThis == 0 ){` |
|   ! 0 |  6171 | `		return PH7_OK;` |
|     - |  6172 | `	}` |
|    37 |  6173 | `	PH7_NativeSetAttrInt(pVm,pThis,DLL_I,` |
|    24 |  6174 | `		(DllFlags(pThis) & DLL_IT_LIFO) ? DllCount(pVm,pThis)-1 : 0);` |
|    25 |  6175 | `	return PH7_OK;` |
|    13 |  6176 | `}` |
|    82 |  6177 | `static int vm_builtin_SplDll_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6178 | `{` |
|    83 |  6179 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    83 |  6180 | `	sxi64 iPos = pThis ? PH7_NativeAttrInt(pThis,DLL_I) : 0;` |
|    41 |  6181 | `	SXUNUSED(nArg);` |
|    41 |  6182 | `	SXUNUSED(apArg);` |
|    83 |  6183 | `	ph7_result_bool(pCtx,iPos >= 0 && iPos < DllCount(pCtx->pVm,pThis));` |
|    83 |  6184 | `	return PH7_OK;` |
|     1 |  6185 | `}` |
|    58 |  6186 | `static int vm_builtin_SplDll_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6187 | `{` |
|    59 |  6188 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    59 |  6189 | `	ph7_value *pVal = pThis ? DllAt(pCtx->pVm,pThis,PH7_NativeAttrInt(pThis,DLL_I)) : 0;` |
|    29 |  6190 | `	SXUNUSED(nArg);` |
|    29 |  6191 | `	SXUNUSED(apArg);` |
|    59 |  6192 | `	if( pVal ){` |
|    59 |  6193 | `		ph7_result_value(pCtx,pVal);` |
|    30 |  6194 | `	}else{` |
|   ! 0 |  6195 | `		ph7_result_null(pCtx);` |
|     - |  6196 | `	}` |
|    59 |  6197 | `	return PH7_OK;` |
|     1 |  6198 | `}` |
|    48 |  6199 | `static int vm_builtin_SplDll_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6200 | `{` |
|    49 |  6201 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    24 |  6202 | `	SXUNUSED(nArg);` |
|    24 |  6203 | `	SXUNUSED(apArg);` |
|    49 |  6204 | `	ph7_result_int64(pCtx,pThis ? PH7_NativeAttrInt(pThis,DLL_I) : 0);` |
|    49 |  6205 | `	return PH7_OK;` |
|     1 |  6206 | `}` |
|     - |  6207 | ``/* php's move_forward, with the direction flipped for prev() (its `flags ^ LIFO`). */`` |
|    58 |  6208 | `static int DllStep(ph7_context *pCtx,int bFlip)` |
|     1 |  6209 | `{` |
|    59 |  6210 | `	ph7_vm *pVm = pCtx->pVm;` |
|    59 |  6211 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  6212 | `	int iFlags;` |
|     - |  6213 | `	sxi64 iPos;` |
|    59 |  6214 | `	if( pThis == 0 ){` |
|   ! 0 |  6215 | `		return PH7_OK;` |
|     - |  6216 | `	}` |
|    59 |  6217 | `	iFlags = DllFlags(pThis);` |
|    59 |  6218 | `	if( bFlip ){` |
|   ! 0 |  6219 | `		iFlags ^= DLL_IT_LIFO;` |
|   ! 0 |  6220 | `	}` |
|    59 |  6221 | `	iPos = PH7_NativeAttrInt(pThis,DLL_I);` |
|    59 |  6222 | `	if( iPos < 0 \|\| iPos >= DllCount(pVm,pThis) ){` |
|     - |  6223 | `		/* php only steps a LIVE pointer; off the end nothing moves and nothing is` |
|     - |  6224 | `		 * consumed. The position still has to move for a plain walk, though, or` |
|     - |  6225 | `		 * prev() past the head could never come back. */` |
|   ! 0 |  6226 | `		if( (iFlags & DLL_IT_DELETE) == 0 ){` |
|   ! 0 |  6227 | `			PH7_NativeSetAttrInt(pVm,pThis,DLL_I,` |
|   ! 0 |  6228 | `				iPos + ((iFlags & DLL_IT_LIFO) ? -1 : 1));` |
|   ! 0 |  6229 | `		}` |
|   ! 0 |  6230 | `		return PH7_OK;` |
|     - |  6231 | `	}` |
|    59 |  6232 | `	if( iFlags & DLL_IT_DELETE ){` |
|    23 |  6233 | `		if( iFlags & DLL_IT_LIFO ){` |
|     7 |  6234 | `			DllArrayCall(pCtx,ph7_hashmap_pop,0,0);` |
|     7 |  6235 | `			PH7_NativeSetAttrInt(pVm,pThis,DLL_I,iPos-1);` |
|     4 |  6236 | `		}else{` |
|     - |  6237 | `			/* php consumes the head and does NOT advance: the walk stays at 0. */` |
|    17 |  6238 | `			DllArrayCall(pCtx,ph7_hashmap_shift,0,0);` |
|     - |  6239 | `		}` |
|    23 |  6240 | `		ph7_result_null(pCtx);` |
|    23 |  6241 | `		return PH7_OK;` |
|     - |  6242 | `	}` |
|    37 |  6243 | `	PH7_NativeSetAttrInt(pVm,pThis,DLL_I,iPos + ((iFlags & DLL_IT_LIFO) ? -1 : 1));` |
|    37 |  6244 | `	return PH7_OK;` |
|    30 |  6245 | `}` |
|    58 |  6246 | `static int vm_builtin_SplDll_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6247 | `{` |
|    29 |  6248 | `	SXUNUSED(nArg);` |
|    29 |  6249 | `	SXUNUSED(apArg);` |
|    59 |  6250 | `	return DllStep(pCtx,FALSE);` |
|     1 |  6251 | `}` |
|   ! 0 |  6252 | `static int vm_builtin_SplDll_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 |  6253 | `{` |
|   ! 0 |  6254 | `	SXUNUSED(nArg);` |
|   ! 0 |  6255 | `	SXUNUSED(apArg);` |
|   ! 0 |  6256 | `	return DllStep(pCtx,TRUE);` |
|   ! 0 |  6257 | `}` |
|     - |  6258 | `/*` |
|     - |  6259 | `` * php's get_debug_info: `flags` then `dllist`, and NOTHING for the (array) cast --`` |
|     - |  6260 | ` * the same var_dump/cast disagreement WeakReference has, which is why xPresent is` |
|     - |  6261 | `` * told which surface is asking. `__debugInfo()` is the same array, reachable by`` |
|     - |  6262 | ` * name because php declares it.` |
|     - |  6263 | ` */` |
|     2 |  6264 | `static sxi32 DllFillDebug(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)` |
|     1 |  6265 | `{` |
|     - |  6266 | `	ph7_value sKey,sVal,*pStore;` |
|     3 |  6267 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|     3 |  6268 | `	PH7_MemObjStringAppend(&sKey,"flags",sizeof("flags")-1);` |
|     3 |  6269 | `	PH7_MemObjInitFromInt(pVm,&sVal,DllFlags(pThis));` |
|     3 |  6270 | `	ph7_array_add_elem(pOut,&sKey,&sVal);` |
|     3 |  6271 | `	PH7_MemObjRelease(&sKey);` |
|     3 |  6272 | `	PH7_MemObjRelease(&sVal);` |
|     3 |  6273 | `	pStore = DllSlot(pVm,pThis);` |
|     3 |  6274 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|     3 |  6275 | `	PH7_MemObjStringAppend(&sKey,"dllist",sizeof("dllist")-1);` |
|     3 |  6276 | `	if( pStore ){` |
|     3 |  6277 | `		ph7_array_add_elem(pOut,&sKey,pStore);` |
|     1 |  6278 | `	}` |
|     3 |  6279 | `	PH7_MemObjRelease(&sKey);` |
|     3 |  6280 | `	return PH7_OK;` |
|     1 |  6281 | `}` |
|     2 |  6282 | `static sxi32 DllPresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|     1 |  6283 | `{` |
|     3 |  6284 | `	if( !bDebug ){` |
|     3 |  6285 | `		return PH7_OK;   /* php's (array) cast and var_export show nothing */` |
|     - |  6286 | `	}` |
|   ! 0 |  6287 | `	return DllFillDebug(pVm,pThis,pOut);` |
|     2 |  6288 | `}` |
|     2 |  6289 | `static int vm_builtin_SplDll_debugInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6290 | `{` |
|     3 |  6291 | `	ph7_vm *pVm = pCtx->pVm;` |
|     3 |  6292 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  6293 | `	ph7_value sOut;` |
|     1 |  6294 | `	SXUNUSED(nArg);` |
|     1 |  6295 | `	SXUNUSED(apArg);` |
|     3 |  6296 | `	PH7_MemObjInit(pVm,&sOut);` |
|     3 |  6297 | `	if( PH7_MemObjToHashmap(&sOut) != SXRET_OK ){` |
|   ! 0 |  6298 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 |  6299 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  6300 | `	}` |
|     3 |  6301 | `	DllFillDebug(pVm,pThis,&sOut);` |
|     3 |  6302 | `	ph7_result_value(pCtx,&sOut);` |
|     3 |  6303 | `	PH7_MemObjRelease(&sOut);` |
|     3 |  6304 | `	return PH7_OK;` |
|     2 |  6305 | `}` |
|     - |  6306 | `/*` |
|     - |  6307 | ` * php's __serialize(): [flags, elements, dynamic members]. This is what` |
|     - |  6308 | ` * serialize() actually uses -- the Serializable pair below exists because the` |
|     - |  6309 | ` * interface is still declared, and php words its own legacy format there.` |
|     - |  6310 | ` */` |
|    18 |  6311 | `static int vm_builtin_SplDll_serializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6312 | `{` |
|    19 |  6313 | `	ph7_vm *pVm = pCtx->pVm;` |
|    19 |  6314 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  6315 | `	ph7_value sOut,sVal,*pStore;` |
|     9 |  6316 | `	SXUNUSED(nArg);` |
|     9 |  6317 | `	SXUNUSED(apArg);` |
|    19 |  6318 | `	PH7_MemObjInit(pVm,&sOut);` |
|    19 |  6319 | `	if( PH7_MemObjToHashmap(&sOut) != SXRET_OK ){` |
|   ! 0 |  6320 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 |  6321 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  6322 | `	}` |
|    19 |  6323 | `	PH7_MemObjInitFromInt(pVm,&sVal,DllFlags(pThis));` |
|    19 |  6324 | `	ph7_array_add_elem(&sOut,0,&sVal);` |
|    19 |  6325 | `	PH7_MemObjRelease(&sVal);` |
|    19 |  6326 | `	pStore = DllSlot(pVm,pThis);` |
|    19 |  6327 | `	if( pStore ){` |
|    19 |  6328 | `		ph7_array_add_elem(&sOut,0,pStore);` |
|     9 |  6329 | `	}` |
|     - |  6330 | `	/* The members slot: php's own properties — empty for a bare SplStack, a` |
|     - |  6331 | `	 * SUBCLASS's declared slots when there is one. */` |
|    19 |  6332 | `	if( SplMembersOf(pVm,pThis,&sVal) == SXRET_OK ){` |
|    19 |  6333 | `		ph7_array_add_elem(&sOut,0,&sVal);` |
|     9 |  6334 | `	}` |
|    19 |  6335 | `	PH7_MemObjRelease(&sVal);` |
|    19 |  6336 | `	ph7_result_value(pCtx,&sOut);` |
|    19 |  6337 | `	PH7_MemObjRelease(&sOut);` |
|    19 |  6338 | `	return PH7_OK;` |
|    10 |  6339 | `}` |
|     6 |  6340 | `static int vm_builtin_SplDll_unserializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6341 | `{` |
|     7 |  6342 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 |  6343 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  6344 | `	ph7_hashmap *pData;` |
|     7 |  6345 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  6346 | `	ph7_value *pFlags,*pStore,*pSlot;` |
|     7 |  6347 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 \|\| pThis == 0 ){` |
|   ! 0 |  6348 | `		return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - |  6349 | `			"Incomplete or ill-typed serialization data");` |
|     - |  6350 | `	}` |
|     7 |  6351 | `	pData = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     7 |  6352 | `	if( HashmapLookupIntKey(pData,0,&pNode) != SXRET_OK ){` |
|   ! 0 |  6353 | `		return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - |  6354 | `			"Incomplete or ill-typed serialization data");` |
|     - |  6355 | `	}` |
|     7 |  6356 | `	pFlags = HashmapExtractNodeValue(pNode);` |
|     6 |  6357 | `	if( pFlags == 0 \|\| (pFlags->iFlags & MEMOBJ_INT) == 0` |
|     7 |  6358 | `	 \|\| HashmapLookupIntKey(pData,1,&pNode) != SXRET_OK ){` |
|   ! 0 |  6359 | `		return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - |  6360 | `			"Incomplete or ill-typed serialization data");` |
|     - |  6361 | `	}` |
|     7 |  6362 | `	pStore = HashmapExtractNodeValue(pNode);` |
|     7 |  6363 | `	if( pStore == 0 \|\| (pStore->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 |  6364 | `		return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - |  6365 | `			"Incomplete or ill-typed serialization data");` |
|     - |  6366 | `	}` |
|     7 |  6367 | `	PH7_NativeSetAttrInt(pVm,pThis,DLL_FL,ph7_value_to_int64(pFlags));` |
|     7 |  6368 | `	pSlot = PH7_NativeAttr(pThis,DLL_Q);` |
|     7 |  6369 | `	if( pSlot ){` |
|     7 |  6370 | `		PH7_MemObjRelease(pSlot);` |
|     7 |  6371 | `		PH7_MemObjStore(pStore,pSlot);` |
|     3 |  6372 | `	}` |
|     7 |  6373 | `	if( HashmapLookupIntKey(pData,2,&pNode) == SXRET_OK ){` |
|     7 |  6374 | `		SplMembersLoad(pThis,HashmapExtractNodeValue(pNode));` |
|     3 |  6375 | `	}` |
|     7 |  6376 | `	return PH7_OK;` |
|     4 |  6377 | `}` |
|     - |  6378 | `/*` |
|     - |  6379 | ` * php's Serializable pair, kept because the interface is still declared: the` |
|     - |  6380 | ` * format is the serialized FLAGS followed by one ':' + serialized value per` |
|     - |  6381 | ` * element ("i:0;:i:1;:i:2;"), which nothing else in php produces or reads.` |
|     - |  6382 | ` */` |
|     - |  6383 | `/*` |
|     - |  6384 | ` * One serialized value, appended to a blob. The RESET is the point: the engine's` |
|     - |  6385 | ` * serialize() writes through ph7_value_string, which APPENDS to the context's` |
|     - |  6386 | ` * return slot rather than replacing it, so a loop that calls it per element` |
|     - |  6387 | ` * accumulates every previous answer into the next one. Shared with` |
|     - |  6388 | ` * SplObjectStorage's legacy format, which is built the same way.` |
|     - |  6389 | ` */` |
|    32 |  6390 | `static void SplSerializeInto(ph7_context *pCtx,ph7_value **apCall,SyBlob *pOut)` |
|     1 |  6391 | `{` |
|    33 |  6392 | `	int nLen = 0;` |
|     - |  6393 | `	const char *zTxt;` |
|    33 |  6394 | `	if( pCtx->pRet ){` |
|    33 |  6395 | `		PH7_MemObjRelease(pCtx->pRet);` |
|    16 |  6396 | `	}` |
|    33 |  6397 | `	vm_builtin_serialize(pCtx,1,apCall);` |
|    33 |  6398 | `	if( pCtx->pRet == 0 ){` |
|   ! 0 |  6399 | `		return;` |
|     - |  6400 | `	}` |
|    33 |  6401 | `	zTxt = ph7_value_to_string(pCtx->pRet,&nLen);` |
|    33 |  6402 | `	SyBlobAppend(pOut,zTxt,(sxu32)nLen);` |
|    17 |  6403 | `}` |
|     2 |  6404 | `static int vm_builtin_SplDll_serialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6405 | `{` |
|     3 |  6406 | `	ph7_vm *pVm = pCtx->pVm;` |
|     3 |  6407 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     3 |  6408 | `	ph7_hashmap *pMap = DllMap(pVm,pThis);` |
|     - |  6409 | `	ph7_hashmap_node *pNode;` |
|     - |  6410 | `	SyBlob sOut;` |
|     - |  6411 | `	ph7_value sFlags,*apCall[1];` |
|     3 |  6412 | `	sxi64 n,nCount = pMap ? (sxi64)pMap->nEntry : 0;` |
|     1 |  6413 | `	SXUNUSED(nArg);` |
|     1 |  6414 | `	SXUNUSED(apArg);` |
|     3 |  6415 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|     3 |  6416 | `	PH7_MemObjInitFromInt(pVm,&sFlags,DllFlags(pThis));` |
|     3 |  6417 | `	apCall[0] = &sFlags;` |
|     3 |  6418 | `	SplSerializeInto(pCtx,apCall,&sOut);` |
|     3 |  6419 | `	PH7_MemObjRelease(&sFlags);` |
|     9 |  6420 | `	for( n = 0 ; n < nCount ; ++n ){` |
|     - |  6421 | `		ph7_value *pVal;` |
|     7 |  6422 | `		pNode = 0;` |
|     7 |  6423 | `		if( HashmapLookupIntKey(pMap,n,&pNode) != SXRET_OK ){` |
|   ! 0 |  6424 | `			continue;` |
|     - |  6425 | `		}` |
|     7 |  6426 | `		pVal = HashmapExtractNodeValue(pNode);` |
|     7 |  6427 | `		if( pVal == 0 ){` |
|   ! 0 |  6428 | `			continue;` |
|     - |  6429 | `		}` |
|     7 |  6430 | `		apCall[0] = pVal;` |
|     7 |  6431 | `		SyBlobAppend(&sOut,":",1);` |
|     7 |  6432 | `		SplSerializeInto(pCtx,apCall,&sOut);` |
|     4 |  6433 | `	}` |
|     - |  6434 | `	/* ph7_result_string APPENDS too, and pRet still holds the LAST element's` |
|     - |  6435 | `	 * serialization from the loop above — drop it before writing the answer. */` |
|     3 |  6436 | `	if( pCtx->pRet ){` |
|     3 |  6437 | `		PH7_MemObjRelease(pCtx->pRet);` |
|     1 |  6438 | `	}` |
|     3 |  6439 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     3 |  6440 | `	SyBlobRelease(&sOut);` |
|     3 |  6441 | `	return PH7_OK;` |
|     1 |  6442 | `}` |
|   ! 0 |  6443 | `static int vm_builtin_SplDll_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 |  6444 | `{` |
|   ! 0 |  6445 | `	ph7_vm *pVm = pCtx->pVm;` |
|   ! 0 |  6446 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  6447 | `	const char *zData,*zCur,*zEnd;` |
|   ! 0 |  6448 | `	int nData = 0;` |
|   ! 0 |  6449 | `	int bFirst = 1;` |
|     - |  6450 | `	ph7_value *pSlot;` |
|     - |  6451 | `	ph7_hashmap *pMap;` |
|   ! 0 |  6452 | `	if( nArg < 1 \|\| pThis == 0 ){` |
|   ! 0 |  6453 | `		return PH7_OK;` |
|     - |  6454 | `	}` |
|   ! 0 |  6455 | `	zData = ph7_value_to_string(apArg[0],&nData);` |
|   ! 0 |  6456 | `	if( nData < 1 ){` |
|   ! 0 |  6457 | `		return PH7_OK;   /* php returns without touching the list */` |
|     - |  6458 | `	}` |
|   ! 0 |  6459 | `	pSlot = DllSlot(pVm,pThis);` |
|   ! 0 |  6460 | `	pMap = pSlot ? (ph7_hashmap *)pSlot->x.pOther : 0;` |
|   ! 0 |  6461 | `	if( pMap == 0 ){` |
|   ! 0 |  6462 | `		return PH7_OK;` |
|     - |  6463 | `	}` |
|     - |  6464 | `	/* php empties the list first, then reads the flags, then one ':'-prefixed` |
|     - |  6465 | `	 * value per element. A malformed tail is an UnexpectedValueException naming` |
|     - |  6466 | `	 * the byte offset — reproduced here from the same position arithmetic. */` |
|   ! 0 |  6467 | `	while( pMap->pFirst ){` |
|   ! 0 |  6468 | `		PH7_HashmapUnlinkNode(pMap->pFirst,TRUE);` |
|   ! 0 |  6469 | `	}` |
|   ! 0 |  6470 | `	zCur = zData;` |
|   ! 0 |  6471 | `	zEnd = &zData[nData];` |
|   ! 0 |  6472 | `	while( zCur < zEnd ){` |
|     - |  6473 | `		ph7_value sPart,sRes,*apCall[1];` |
|     - |  6474 | `		int nPart;` |
|   ! 0 |  6475 | `		const char *zStop = zCur;` |
|   ! 0 |  6476 | `		if( !bFirst ){` |
|   ! 0 |  6477 | `			if( zCur[0] != ':' ){` |
|   ! 0 |  6478 | `				break;` |
|     - |  6479 | `			}` |
|   ! 0 |  6480 | `			zCur++;` |
|   ! 0 |  6481 | `		}` |
|     - |  6482 | `		/* One serialized scalar reaches up to and including its ';'. */` |
|   ! 0 |  6483 | `		while( zStop < zEnd && zStop[0] != ';' ){` |
|   ! 0 |  6484 | `			zStop++;` |
|   ! 0 |  6485 | `		}` |
|   ! 0 |  6486 | `		if( zStop >= zEnd ){` |
|   ! 0 |  6487 | `			zStop = zEnd;` |
|   ! 0 |  6488 | `		}else{` |
|   ! 0 |  6489 | `			zStop++;` |
|     - |  6490 | `		}` |
|   ! 0 |  6491 | `		nPart = (int)(zStop - zCur);` |
|   ! 0 |  6492 | `		if( nPart <= 0 ){` |
|   ! 0 |  6493 | `			break;` |
|     - |  6494 | `		}` |
|   ! 0 |  6495 | `		PH7_MemObjInitFromString(pVm,&sPart,0);` |
|   ! 0 |  6496 | `		PH7_MemObjStringAppend(&sPart,zCur,(sxu32)nPart);` |
|   ! 0 |  6497 | `		apCall[0] = &sPart;` |
|   ! 0 |  6498 | `		PH7_MemObjInit(pVm,&sRes);` |
|   ! 0 |  6499 | `		if( pCtx->pRet ){` |
|   ! 0 |  6500 | `			PH7_MemObjRelease(pCtx->pRet);   /* see DllSerializeInto: pRet is appended to */` |
|   ! 0 |  6501 | `		}` |
|   ! 0 |  6502 | `		vm_builtin_unserialize(pCtx,1,apCall);` |
|   ! 0 |  6503 | `		if( pCtx->pRet ){` |
|   ! 0 |  6504 | `			PH7_MemObjStore(pCtx->pRet,&sRes);` |
|   ! 0 |  6505 | `		}` |
|   ! 0 |  6506 | `		if( bFirst ){` |
|   ! 0 |  6507 | `			PH7_NativeSetAttrInt(pVm,pThis,DLL_FL,ph7_value_to_int64(&sRes));` |
|   ! 0 |  6508 | `			bFirst = 0;` |
|   ! 0 |  6509 | `		}else{` |
|   ! 0 |  6510 | `			PH7_HashmapInsert(pMap,0,&sRes);` |
|     - |  6511 | `		}` |
|   ! 0 |  6512 | `		PH7_MemObjRelease(&sPart);` |
|   ! 0 |  6513 | `		PH7_MemObjRelease(&sRes);` |
|   ! 0 |  6514 | `		zCur = zStop;` |
|   ! 0 |  6515 | `	}` |
|   ! 0 |  6516 | `	ph7_result_null(pCtx);` |
|   ! 0 |  6517 | `	return PH7_OK;` |
|   ! 0 |  6518 | `}` |
|     - |  6519 | `/*` |
|     - |  6520 | ` * The declaration. Method ORDER is spl_dllist.stub.php's, php declares NO` |
|     - |  6521 | ` * constructor for any of the three, and the IT_FIX bit is a per-class DEFAULT on` |
|     - |  6522 | ` * the flags slot -- which is exactly how php does it (the create handler stamps` |
|     - |  6523 | ` * the flags; there is no constructor to run).` |
|     - |  6524 | ` */` |
|  5740 |  6525 | `static sxi32 VmInstallSplDllist(ph7_vm *pVm)` |
|     5 |  6526 | `{` |
|     - |  6527 | `	static const PH7_NativeMethodDef aDllMethod[] = {` |
|     - |  6528 | `		{ "add",             PH7_MOD_PUBLIC, "int $index, mixed $value", "@void",` |
|     - |  6529 | `		  vm_builtin_SplDll_add },` |
|     - |  6530 | `		{ "pop",             PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplDll_pop },` |
|     - |  6531 | `		{ "shift",           PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplDll_shift },` |
|     - |  6532 | `		{ "push",            PH7_MOD_PUBLIC, "mixed $value", "@void", vm_builtin_SplDll_push },` |
|     - |  6533 | `		{ "unshift",         PH7_MOD_PUBLIC, "mixed $value", "@void", vm_builtin_SplDll_unshift },` |
|     - |  6534 | `		{ "top",             PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplDll_top },` |
|     - |  6535 | `		{ "bottom",          PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplDll_bottom },` |
|     - |  6536 | `		{ "__debugInfo",     PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplDll_debugInfo },` |
|     - |  6537 | `		{ "count",           PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplDll_count },` |
|     - |  6538 | `		{ "isEmpty",         PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplDll_isEmpty },` |
|     - |  6539 | `		{ "setIteratorMode", PH7_MOD_PUBLIC, "int $mode", "@int",` |
|     - |  6540 | `		  vm_builtin_SplDll_setIteratorMode },` |
|     - |  6541 | `		{ "getIteratorMode", PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplDll_getIteratorMode },` |
|     - |  6542 | ``		/* php's stub leaves these four offsets UNTYPED (a `@param int` docblock, which`` |
|     - |  6543 | `		 * Reflection does not print) while the ZPP enforces int -- so the signature says` |
|     - |  6544 | `		 * nothing and each body runs PH7_IntArgResolve itself. */` |
|     - |  6545 | `		{ "offsetExists",    PH7_MOD_PUBLIC, "$index", "@bool", vm_builtin_SplDll_offsetExists },` |
|     - |  6546 | `		{ "offsetGet",       PH7_MOD_PUBLIC, "$index", "@mixed", vm_builtin_SplDll_offsetGet },` |
|     - |  6547 | `		{ "offsetSet",       PH7_MOD_PUBLIC, "$index, mixed $value", "@void",` |
|     - |  6548 | `		  vm_builtin_SplDll_offsetSet },` |
|     - |  6549 | `		{ "offsetUnset",     PH7_MOD_PUBLIC, "$index", "@void", vm_builtin_SplDll_offsetUnset },` |
|     - |  6550 | `		{ "rewind",          PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplDll_rewind },` |
|     - |  6551 | `		{ "current",         PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplDll_current },` |
|     - |  6552 | `		{ "key",             PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplDll_key },` |
|     - |  6553 | `		{ "prev",            PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplDll_prev },` |
|     - |  6554 | `		{ "next",            PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplDll_next },` |
|     - |  6555 | `		{ "valid",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplDll_valid },` |
|     - |  6556 | `		{ "unserialize",     PH7_MOD_PUBLIC, "string $data", "@void",` |
|     - |  6557 | `		  vm_builtin_SplDll_unserialize },` |
|     - |  6558 | `		{ "serialize",       PH7_MOD_PUBLIC, "", "@string", vm_builtin_SplDll_serialize },` |
|     - |  6559 | `		{ "__serialize",     PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplDll_serializeMagic },` |
|     - |  6560 | `		{ "__unserialize",   PH7_MOD_PUBLIC, "array $data", "@void",` |
|     - |  6561 | `		  vm_builtin_SplDll_unserializeMagic },` |
|     - |  6562 | `	};` |
|     - |  6563 | `	static const PH7_NativeMethodDef aQueueMethod[] = {` |
|     - |  6564 | `		/* php's @implementation-alias: the same C bodies under the queue's names. */` |
|     - |  6565 | `		{ "enqueue", PH7_MOD_PUBLIC, "mixed $value", "@void", vm_builtin_SplDll_push },` |
|     - |  6566 | `		{ "dequeue", PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplDll_shift },` |
|     - |  6567 | `	};` |
|     - |  6568 | `	static const PH7_NativeConstDef aDllConst[] = {` |
|     - |  6569 | `		{ "IT_MODE_LIFO",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, DLL_IT_LIFO, 0, 0.0 },` |
|     - |  6570 | `		{ "IT_MODE_FIFO",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 0, 0, 0.0 },` |
|     - |  6571 | `		{ "IT_MODE_DELETE", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, DLL_IT_DELETE, 0, 0.0 },` |
|     - |  6572 | `		{ "IT_MODE_KEEP",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 0, 0, 0.0 },` |
|     - |  6573 | `	};` |
|     - |  6574 | `	static const PH7_NativePropDef aDllProp[] = {` |
|     - |  6575 | `		{ DLL_Q,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  6576 | `		{ DLL_FL, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  6577 | `		{ DLL_I,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  6578 | `	};` |
|     - |  6579 | `	/* php's object handler stamps IT_FIX (and LIFO for a stack) at CREATION, which` |
|     - |  6580 | `	 * is why neither subclass declares a constructor and why the bit survives every` |
|     - |  6581 | `	 * setIteratorMode(). A per-class default on the flags slot says the same thing. */` |
|     - |  6582 | `	static const PH7_NativePropDef aQueueProp[] = {` |
|     - |  6583 | `		{ DLL_FL, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN,` |
|     - |  6584 | `		  { 0, 0, PH7_NATIVE_VAL_INT, DLL_IT_FIX, 0, 0.0 }, 0 },` |
|     - |  6585 | `	};` |
|     - |  6586 | `	static const PH7_NativePropDef aStackProp[] = {` |
|     - |  6587 | `		{ DLL_FL, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN,` |
|     - |  6588 | `		  { 0, 0, PH7_NATIVE_VAL_INT, DLL_IT_FIX\|DLL_IT_LIFO, 0, 0.0 }, 0 },` |
|     - |  6589 | `	};` |
|     - |  6590 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - |  6591 | `		{ "SplDoublyLinkedList", 0, "Iterator,Countable,ArrayAccess,Serializable", 0,` |
|     - |  6592 | `		  aDllMethod, SX_ARRAYSIZE(aDllMethod),` |
|     - |  6593 | `		  aDllConst, SX_ARRAYSIZE(aDllConst),` |
|     - |  6594 | `		  aDllProp, SX_ARRAYSIZE(aDllProp), 0, 0, DllPresent },` |
|     - |  6595 | `		{ "SplQueue", "SplDoublyLinkedList", 0, 0,` |
|     - |  6596 | `		  aQueueMethod, SX_ARRAYSIZE(aQueueMethod), 0, 0,` |
|     - |  6597 | `		  aQueueProp, SX_ARRAYSIZE(aQueueProp), 0, 0, DllPresent },` |
|     - |  6598 | `		{ "SplStack", "SplDoublyLinkedList", 0, 0,` |
|     - |  6599 | `		  0, 0, 0, 0,` |
|     - |  6600 | `		  aStackProp, SX_ARRAYSIZE(aStackProp), 0, 0, DllPresent },` |
|     - |  6601 | `	};` |
|  5745 |  6602 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 |  6603 | `}` |
|     - |  6604 | `/*` |
|     - |  6605 | ` * ---------------------------------------------------------------------------` |
|     - |  6606 | ` * SplHeap, SplMinHeap, SplMaxHeap and SplPriorityQueue.` |
|     - |  6607 | ` *` |
|     - |  6608 | `` * php's `spl_heap_object` is an array plus a FLAGS word, and the flags carry the`` |
|     - |  6609 | ` * thing the chunk could not express at all: **SPL_HEAP_CORRUPTED**. php sets it` |
|     - |  6610 | ` * when an exception escapes the user's compare() mid-sift -- the heap invariant is` |
|     - |  6611 | ` * then unknown -- and every operation that DEPENDS on the invariant refuses with` |
|     - |  6612 | ` * "Heap is corrupted, heap properties are no longer ensured." until` |
|     - |  6613 | `` * recoverFromCorruption() clears it. The chunk hardcoded `isCorrupted()` to false`` |
|     - |  6614 | `` * and `recoverFromCorruption()` to true, so a throwing comparator left a silently`` |
|     - |  6615 | ` * mis-ordered heap that kept answering.` |
|     - |  6616 | ` *` |
|     - |  6617 | ` * Which operations refuse is not guessable and was mapped against the oracle:` |
|     - |  6618 | ` * insert, extract, top, next and __serialize/__unserialize refuse; count,` |
|     - |  6619 | ` * isEmpty, rewind, valid, current, key, isCorrupted, recoverFromCorruption and` |
|     - |  6620 | `` * __debugInfo all keep working. (A `foreach` refuses because it reaches next().)`` |
|     - |  6621 | ` *` |
|     - |  6622 | ` * php's priority-queue node is exactly {data, priority} -- the chunk carried a` |
|     - |  6623 | `` * third field, a descending `__serial` it never compared with, which leaked into`` |
|     - |  6624 | ` * serialize(), var_dump() and the (array) cast as a nonsense PHP_INT_MAX-relative` |
|     - |  6625 | ` * integer. It is gone; equal priorities keep the order php's strictly-greater` |
|     - |  6626 | ` * swap gives them.` |
|     - |  6627 | ` *` |
|     - |  6628 | ` * The other three the chunk lacked, the same three the SplDoublyLinkedList` |
|     - |  6629 | `` * conversion lacked: `__debugInfo()` (flags / isCorrupted / heap, and for the queue`` |
|     - |  6630 | ` * the heap entries are rendered EXTR_BOTH-style whatever the extract flags say),` |
|     - |  6631 | `` * and the `__serialize()`/`__unserialize()` pair, whose payload is`` |
|     - |  6632 | ` * [members, {flags, heap_elements}] and whose reader VALIDATES -- a plain heap` |
|     - |  6633 | ` * refuses a non-zero flags word, the queue refuses a zero one.` |
|     - |  6634 | ` */` |
|     - |  6635 | `#define HP_H  "__h"   /* the heap array, in heap order */` |
|     - |  6636 | `#define HP_FL "__fl"  /* php's intern->flags: the queue's EXTR bits, 0 for a heap */` |
|     - |  6637 | `#define HP_CR "__cr"  /* php's SPL_HEAP_CORRUPTED */` |
|     - |  6638 |  |
|     - |  6639 | `#define PQ_EXTR_DATA     1` |
|     - |  6640 | `#define PQ_EXTR_PRIORITY 2` |
|     - |  6641 | `#define PQ_EXTR_BOTH     3` |
|     - |  6642 | `#define PQ_EXTR_MASK     3` |
|     - |  6643 |  |
|  1872 |  6644 | `static ph7_value * HeapSlot(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  6645 | `{` |
|  1873 |  6646 | `	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,HP_H) : 0;` |
|  1873 |  6647 | `	if( pSlot == 0 ){` |
|   ! 0 |  6648 | `		return 0;` |
|     - |  6649 | `	}` |
|  1873 |  6650 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    87 |  6651 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|   ! 0 |  6652 | `			return 0;` |
|     - |  6653 | `		}` |
|    43 |  6654 | `	}` |
|  1873 |  6655 | `	if( PH7_HashmapCowSeparate(pVm,pSlot) == 0 ){` |
|   ! 0 |  6656 | `		return 0;` |
|     - |  6657 | `	}` |
|  1873 |  6658 | `	return pSlot;` |
|   937 |  6659 | `}` |
|  1852 |  6660 | `static ph7_hashmap * HeapMap(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  6661 | `{` |
|  1853 |  6662 | `	ph7_value *pSlot = HeapSlot(pVm,pThis);` |
|  1853 |  6663 | `	return pSlot ? (ph7_hashmap *)pSlot->x.pOther : 0;` |
|     1 |  6664 | `}` |
|   584 |  6665 | `static sxi64 HeapCount(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  6666 | `{` |
|   585 |  6667 | `	ph7_hashmap *pMap = HeapMap(pVm,pThis);` |
|   585 |  6668 | `	return pMap ? (sxi64)pMap->nEntry : 0;` |
|     1 |  6669 | `}` |
|     - |  6670 | `/* Re-resolved on every use: any call into the user's compare() may have moved` |
|     - |  6671 | ` * pVm->aMemObj under us (rule 47). */` |
|   746 |  6672 | `static ph7_value * HeapAt(ph7_vm *pVm,ph7_class_instance *pThis,sxi64 i)` |
|     1 |  6673 | `{` |
|   747 |  6674 | `	ph7_hashmap *pMap = HeapMap(pVm,pThis);` |
|   747 |  6675 | `	ph7_hashmap_node *pNode = 0;` |
|   747 |  6676 | `	if( pMap == 0 \|\| HashmapLookupIntKey(pMap,i,&pNode) != SXRET_OK ){` |
|   ! 0 |  6677 | `		return 0;` |
|     - |  6678 | `	}` |
|   747 |  6679 | `	return HashmapExtractNodeValue(pNode);` |
|   374 |  6680 | `}` |
|   220 |  6681 | `static void HeapPut(ph7_vm *pVm,ph7_class_instance *pThis,sxi64 i,ph7_value *pVal)` |
|     1 |  6682 | `{` |
|   221 |  6683 | `	ph7_hashmap *pMap = HeapMap(pVm,pThis);` |
|     - |  6684 | `	ph7_value sKey;` |
|   221 |  6685 | `	if( pMap == 0 ){` |
|   ! 0 |  6686 | `		return;` |
|     - |  6687 | `	}` |
|   221 |  6688 | `	PH7_MemObjInitFromInt(pVm,&sKey,i);` |
|   221 |  6689 | `	PH7_HashmapInsert(pMap,&sKey,pVal);` |
|   221 |  6690 | `	PH7_MemObjRelease(&sKey);` |
|   111 |  6691 | `}` |
|    80 |  6692 | `static void HeapSwap(ph7_vm *pVm,ph7_class_instance *pThis,sxi64 i,sxi64 j)` |
|     1 |  6693 | `{` |
|     - |  6694 | `	ph7_value sI,sJ,*pV;` |
|    81 |  6695 | `	PH7_MemObjInit(pVm,&sI);` |
|    81 |  6696 | `	PH7_MemObjInit(pVm,&sJ);` |
|    81 |  6697 | `	pV = HeapAt(pVm,pThis,i);` |
|    81 |  6698 | `	if( pV ){` |
|    81 |  6699 | `		PH7_MemObjStore(pV,&sI);` |
|    40 |  6700 | `	}` |
|    81 |  6701 | `	pV = HeapAt(pVm,pThis,j);` |
|    81 |  6702 | `	if( pV ){` |
|    81 |  6703 | `		PH7_MemObjStore(pV,&sJ);` |
|    40 |  6704 | `	}` |
|    81 |  6705 | `	HeapPut(pVm,pThis,i,&sJ);` |
|    81 |  6706 | `	HeapPut(pVm,pThis,j,&sI);` |
|    81 |  6707 | `	PH7_MemObjRelease(&sI);` |
|    81 |  6708 | `	PH7_MemObjRelease(&sJ);` |
|    81 |  6709 | `}` |
|   390 |  6710 | `static int HeapCorrupted(ph7_class_instance *pThis)` |
|     1 |  6711 | `{` |
|   391 |  6712 | `	return pThis ? (int)PH7_NativeAttrInt(pThis,HP_CR) : 0;` |
|     1 |  6713 | `}` |
|     - |  6714 | `/*` |
|     - |  6715 | ` * php's spl_heap_consistency_validations. Only the operations that DEPEND on the` |
|     - |  6716 | ` * heap invariant call it -- count()/current()/key() answer from the array and are` |
|     - |  6717 | ` * left alone, which is why a corrupted heap still reports its size.` |
|     - |  6718 | ` */` |
|   382 |  6719 | `static sxi32 HeapCheck(ph7_context *pCtx)` |
|     1 |  6720 | `{` |
|   383 |  6721 | `	if( !HeapCorrupted(PH7_ContextThis(pCtx)) ){` |
|   371 |  6722 | `		return SXRET_OK;` |
|     - |  6723 | `	}` |
|    13 |  6724 | `	return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     - |  6725 | `		"Heap is corrupted, heap properties are no longer ensured.");` |
|   192 |  6726 | `}` |
|     - |  6727 | `/* A priority-queue node is php's {data, priority}: nothing else, and in that order. */` |
|   550 |  6728 | `static int HeapIsPq(ph7_class_instance *pThis)` |
|     1 |  6729 | `{` |
|     - |  6730 | `	ph7_class *pPq;` |
|   551 |  6731 | `	if( pThis == 0 ){` |
|   ! 0 |  6732 | `		return FALSE;` |
|     - |  6733 | `	}` |
|   551 |  6734 | `	pPq = PH7_VmExtractClass(pThis->pVm,"SplPriorityQueue",sizeof("SplPriorityQueue")-1,FALSE,0);` |
|   551 |  6735 | `	return pPq && PH7_VmInstanceOf(pThis->pClass,pPq);` |
|   276 |  6736 | `}` |
|   168 |  6737 | `static ph7_value * HeapNodePart(ph7_value *pNode,const char *zKey)` |
|     1 |  6738 | `{` |
|     - |  6739 | `	ph7_hashmap *pMap;` |
|   169 |  6740 | `	ph7_hashmap_node *pEnt = 0;` |
|   169 |  6741 | `	if( pNode == 0 \|\| (pNode->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 |  6742 | `		return 0;` |
|     - |  6743 | `	}` |
|   169 |  6744 | `	pMap = (ph7_hashmap *)pNode->x.pOther;` |
|   169 |  6745 | `	if( HashmapLookupBlobKey(pMap,zKey,(sxu32)SyStrlen(zKey),&pEnt) != SXRET_OK ){` |
|   ! 0 |  6746 | `		return 0;` |
|     - |  6747 | `	}` |
|   169 |  6748 | `	return HashmapExtractNodeValue(pEnt);` |
|    85 |  6749 | `}` |
|    74 |  6750 | `static sxi32 HeapMakeNode(ph7_vm *pVm,ph7_value *pData,ph7_value *pPrio,ph7_value *pOut)` |
|     1 |  6751 | `{` |
|     - |  6752 | `	ph7_value sKey;` |
|    75 |  6753 | `	if( PH7_MemObjToHashmap(pOut) != SXRET_OK ){` |
|   ! 0 |  6754 | `		return SXERR_MEM;` |
|     - |  6755 | `	}` |
|    75 |  6756 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|    75 |  6757 | `	PH7_MemObjStringAppend(&sKey,"data",sizeof("data")-1);` |
|    75 |  6758 | `	ph7_array_add_elem(pOut,&sKey,pData);` |
|    75 |  6759 | `	PH7_MemObjRelease(&sKey);` |
|    75 |  6760 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|    75 |  6761 | `	PH7_MemObjStringAppend(&sKey,"priority",sizeof("priority")-1);` |
|    75 |  6762 | `	ph7_array_add_elem(pOut,&sKey,pPrio);` |
|    75 |  6763 | `	PH7_MemObjRelease(&sKey);` |
|    75 |  6764 | `	return SXRET_OK;` |
|    38 |  6765 | `}` |
|     - |  6766 | `/*` |
|     - |  6767 | ` * Run the user's compare(). For a queue php compares the PRIORITIES, so the node's` |
|     - |  6768 | `` * `priority` is what is handed over. A throw here is php's corruption trigger: the`` |
|     - |  6769 | ` * bit is set, and the throw still propagates.` |
|     - |  6770 | ` */` |
|   200 |  6771 | `static sxi32 HeapCompare(ph7_context *pCtx,sxi64 iA,sxi64 iB,int *piCmp)` |
|     1 |  6772 | `{` |
|   201 |  6773 | `	ph7_vm *pVm = pCtx->pVm;` |
|   201 |  6774 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  6775 | `	ph7_class_method *pMethod;` |
|     - |  6776 | `	ph7_value sA,sB,sRes,*apArg[2],*pV;` |
|   201 |  6777 | `	int bPq = HeapIsPq(pThis);` |
|     - |  6778 | `	sxi32 rc;` |
|   201 |  6779 | `	*piCmp = 0;` |
|   201 |  6780 | `	pMethod = pThis ? PH7_ClassExtractMethod(pThis->pClass,"compare",sizeof("compare")-1) : 0;` |
|   201 |  6781 | `	if( pMethod == 0 ){` |
|   ! 0 |  6782 | `		return SXRET_OK;` |
|     - |  6783 | `	}` |
|   201 |  6784 | `	PH7_MemObjInit(pVm,&sA);` |
|   201 |  6785 | `	PH7_MemObjInit(pVm,&sB);` |
|   201 |  6786 | `	pV = HeapAt(pVm,pThis,iA);` |
|   201 |  6787 | `	if( bPq ){` |
|    59 |  6788 | `		pV = HeapNodePart(pV,"priority");` |
|    29 |  6789 | `	}` |
|   201 |  6790 | `	if( pV ){` |
|   201 |  6791 | `		PH7_MemObjStore(pV,&sA);` |
|   100 |  6792 | `	}` |
|   201 |  6793 | `	pV = HeapAt(pVm,pThis,iB);` |
|   201 |  6794 | `	if( bPq ){` |
|    59 |  6795 | `		pV = HeapNodePart(pV,"priority");` |
|    29 |  6796 | `	}` |
|   201 |  6797 | `	if( pV ){` |
|   201 |  6798 | `		PH7_MemObjStore(pV,&sB);` |
|   100 |  6799 | `	}` |
|   201 |  6800 | `	apArg[0] = &sA;` |
|   201 |  6801 | `	apArg[1] = &sB;` |
|   201 |  6802 | `	PH7_MemObjInit(pVm,&sRes);` |
|     - |  6803 | `	/* php dispatches through its cached fptr_cmp and never consults visibility --` |
|     - |  6804 | `	 * SplHeap::compare() is PROTECTED and is meant to be called by the heap. */` |
|   201 |  6805 | `	rc = PH7_VmCallMethodUnchecked(pVm,pThis,pMethod,&sRes,2,apArg);` |
|   201 |  6806 | `	if( rc == SXRET_OK ){` |
|   171 |  6807 | `		sxi64 iVal = ph7_value_to_int64(&sRes);` |
|   171 |  6808 | `		*piCmp = iVal < 0 ? -1 : (iVal > 0 ? 1 : 0);` |
|    86 |  6809 | `	}else{` |
|     - |  6810 | `		/* php finishes the sift with the exception in flight and marks the heap` |
|     - |  6811 | `		 * CORRUPTED afterwards; the element it was placing still lands. */` |
|    31 |  6812 | `		PH7_NativeSetAttrInt(pVm,pThis,HP_CR,1);` |
|     - |  6813 | `	}` |
|   201 |  6814 | `	PH7_MemObjRelease(&sA);` |
|   201 |  6815 | `	PH7_MemObjRelease(&sB);` |
|   201 |  6816 | `	PH7_MemObjRelease(&sRes);` |
|   201 |  6817 | `	return rc;` |
|   101 |  6818 | `}` |
|   218 |  6819 | `static sxi32 HeapSiftUp(ph7_context *pCtx,sxi64 i)` |
|     1 |  6820 | `{` |
|   219 |  6821 | `	ph7_vm *pVm = pCtx->pVm;` |
|   219 |  6822 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   287 |  6823 | `	while( i > 0 ){` |
|   147 |  6824 | `		sxi64 p = (i - 1) / 2;` |
|   147 |  6825 | `		int iCmp = 0;` |
|   147 |  6826 | `		sxi32 rc = HeapCompare(pCtx,i,p,&iCmp);` |
|   147 |  6827 | `		if( rc != SXRET_OK ){` |
|    31 |  6828 | `			return rc;` |
|     - |  6829 | `		}` |
|   117 |  6830 | `		if( iCmp <= 0 ){` |
|    49 |  6831 | `			break;` |
|     - |  6832 | `		}` |
|    69 |  6833 | `		HeapSwap(pVm,pThis,i,p);` |
|    69 |  6834 | `		i = p;` |
|     1 |  6835 | `	}` |
|   189 |  6836 | `	return SXRET_OK;` |
|   110 |  6837 | `}` |
|    60 |  6838 | `static sxi32 HeapSiftDown(ph7_context *pCtx,sxi64 i)` |
|     1 |  6839 | `{` |
|    61 |  6840 | `	ph7_vm *pVm = pCtx->pVm;` |
|    61 |  6841 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    42 |  6842 | `	for(;;){` |
|    73 |  6843 | `		sxi64 n = HeapCount(pVm,pThis);` |
|    73 |  6844 | `		sxi64 l = 2*i + 1, r = l + 1, b = i;` |
|    73 |  6845 | `		int iCmp = 0;` |
|     - |  6846 | `		sxi32 rc;` |
|    73 |  6847 | `		if( l < n ){` |
|    43 |  6848 | `			rc = HeapCompare(pCtx,l,b,&iCmp);` |
|    43 |  6849 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  6850 | `				return rc;` |
|     - |  6851 | `			}` |
|    43 |  6852 | `			if( iCmp > 0 ){` |
|    13 |  6853 | `				b = l;` |
|     6 |  6854 | `			}` |
|    21 |  6855 | `		}` |
|    73 |  6856 | `		if( r < n ){` |
|    13 |  6857 | `			rc = HeapCompare(pCtx,r,b,&iCmp);` |
|    13 |  6858 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  6859 | `				return rc;` |
|     - |  6860 | `			}` |
|    13 |  6861 | `			if( iCmp > 0 ){` |
|     3 |  6862 | `				b = r;` |
|     1 |  6863 | `			}` |
|     6 |  6864 | `		}` |
|    73 |  6865 | `		if( b == i ){` |
|    61 |  6866 | `			break;` |
|     - |  6867 | `		}` |
|    13 |  6868 | `		HeapSwap(pVm,pThis,i,b);` |
|    13 |  6869 | `		i = b;` |
|     1 |  6870 | `	}` |
|    61 |  6871 | `	return SXRET_OK;` |
|    31 |  6872 | `}` |
|     - |  6873 | `/* php's spl_pqueue_extract_helper: BOTH wins over either single bit. */` |
|    46 |  6874 | `static void HeapPqShape(ph7_context *pCtx,ph7_value *pNode)` |
|     1 |  6875 | `{` |
|    47 |  6876 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    47 |  6877 | `	int iFlags = pThis ? (int)PH7_NativeAttrInt(pThis,HP_FL) : PQ_EXTR_DATA;` |
|     - |  6878 | `	ph7_value *pPart;` |
|    47 |  6879 | `	if( (iFlags & PQ_EXTR_BOTH) == PQ_EXTR_BOTH ){` |
|     7 |  6880 | `		ph7_result_value(pCtx,pNode);` |
|     7 |  6881 | `		return;` |
|     - |  6882 | `	}` |
|    41 |  6883 | `	pPart = HeapNodePart(pNode,(iFlags & PQ_EXTR_DATA) ? "data" : "priority");` |
|    41 |  6884 | `	if( pPart ){` |
|    41 |  6885 | `		ph7_result_value(pCtx,pPart);` |
|    21 |  6886 | `	}else{` |
|   ! 0 |  6887 | `		ph7_result_null(pCtx);` |
|     - |  6888 | `	}` |
|    24 |  6889 | `}` |
|     - |  6890 | `/* Hand back element 0 the way this class presents it. */` |
|   126 |  6891 | `static void HeapResultTop(ph7_context *pCtx,ph7_value *pNode)` |
|     1 |  6892 | `{` |
|   127 |  6893 | `	if( HeapIsPq(PH7_ContextThis(pCtx)) ){` |
|    47 |  6894 | `		HeapPqShape(pCtx,pNode);` |
|    24 |  6895 | `	}else{` |
|    81 |  6896 | `		ph7_result_value(pCtx,pNode);` |
|     - |  6897 | `	}` |
|   127 |  6898 | `}` |
|   220 |  6899 | `static int vm_builtin_SplHeap_insert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6900 | `{` |
|   221 |  6901 | `	ph7_vm *pVm = pCtx->pVm;` |
|   221 |  6902 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  6903 | `	ph7_hashmap *pMap;` |
|   221 |  6904 | `	sxi32 rc = HeapCheck(pCtx);` |
|   221 |  6905 | `	if( rc != SXRET_OK \|\| nArg < 1 ){` |
|     3 |  6906 | `		return rc;` |
|     - |  6907 | `	}` |
|   219 |  6908 | `	pMap = HeapMap(pVm,pThis);` |
|   219 |  6909 | `	if( pMap == 0 ){` |
|   ! 0 |  6910 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  6911 | `	}` |
|   219 |  6912 | `	if( HeapIsPq(pThis) ){` |
|     - |  6913 | `		ph7_value sNode;` |
|    75 |  6914 | `		if( nArg < 2 ){` |
|   ! 0 |  6915 | `			return PH7_OK;` |
|     - |  6916 | `		}` |
|    75 |  6917 | `		PH7_MemObjInit(pVm,&sNode);` |
|    75 |  6918 | `		if( HeapMakeNode(pVm,apArg[0],apArg[1],&sNode) != SXRET_OK ){` |
|   ! 0 |  6919 | `			PH7_MemObjRelease(&sNode);` |
|   ! 0 |  6920 | `			return PH7_ContextMemoryError(pCtx);` |
|     - |  6921 | `		}` |
|    75 |  6922 | `		PH7_HashmapInsert(pMap,0,&sNode);` |
|    75 |  6923 | `		PH7_MemObjRelease(&sNode);` |
|    38 |  6924 | `	}else{` |
|   145 |  6925 | `		PH7_HashmapInsert(pMap,0,apArg[0]);` |
|     - |  6926 | `	}` |
|   219 |  6927 | `	rc = HeapSiftUp(pCtx,HeapCount(pVm,pThis)-1);` |
|   219 |  6928 | `	if( rc != SXRET_OK ){` |
|    31 |  6929 | `		return rc;` |
|     - |  6930 | `	}` |
|   189 |  6931 | ``	ph7_result_bool(pCtx,1);   /* php's `true` return type */`` |
|   189 |  6932 | `	return PH7_OK;` |
|   111 |  6933 | `}` |
|    90 |  6934 | `static int vm_builtin_SplHeap_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6935 | `{` |
|    91 |  6936 | `	ph7_vm *pVm = pCtx->pVm;` |
|    91 |  6937 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  6938 | `	sxi64 n;` |
|     - |  6939 | `	ph7_value sTop,*pV;` |
|    91 |  6940 | `	sxi32 rc = HeapCheck(pCtx);` |
|    45 |  6941 | `	SXUNUSED(nArg);` |
|    45 |  6942 | `	SXUNUSED(apArg);` |
|    91 |  6943 | `	if( rc != SXRET_OK ){` |
|     3 |  6944 | `		return rc;` |
|     - |  6945 | `	}` |
|    89 |  6946 | `	n = HeapCount(pVm,pThis);` |
|    89 |  6947 | `	if( n == 0 ){` |
|     5 |  6948 | `		return PH7_VmThrowException(pCtx,"RuntimeException","Can't extract from an empty heap");` |
|     - |  6949 | `	}` |
|    85 |  6950 | `	PH7_MemObjInit(pVm,&sTop);` |
|    85 |  6951 | `	pV = HeapAt(pVm,pThis,0);` |
|    85 |  6952 | `	if( pV ){` |
|    85 |  6953 | `		PH7_MemObjStore(pV,&sTop);` |
|    42 |  6954 | `	}` |
|    85 |  6955 | `	if( n > 1 ){` |
|     - |  6956 | `		ph7_value sLast;` |
|    61 |  6957 | `		PH7_MemObjInit(pVm,&sLast);` |
|    61 |  6958 | `		pV = HeapAt(pVm,pThis,n-1);` |
|    61 |  6959 | `		if( pV ){` |
|    61 |  6960 | `			PH7_MemObjStore(pV,&sLast);` |
|    30 |  6961 | `		}` |
|    61 |  6962 | `		HeapPut(pVm,pThis,0,&sLast);` |
|    61 |  6963 | `		PH7_MemObjRelease(&sLast);` |
|    30 |  6964 | `	}` |
|     - |  6965 | `	{` |
|    85 |  6966 | `		ph7_hashmap *pMap = HeapMap(pVm,pThis);` |
|    85 |  6967 | `		ph7_hashmap_node *pNode = 0;` |
|    85 |  6968 | `		if( pMap && HashmapLookupIntKey(pMap,n-1,&pNode) == SXRET_OK ){` |
|    85 |  6969 | `			PH7_HashmapUnlinkNode(pNode,TRUE);` |
|    42 |  6970 | `		}` |
|     - |  6971 | `	}` |
|    85 |  6972 | `	if( n > 1 ){` |
|    61 |  6973 | `		rc = HeapSiftDown(pCtx,0);` |
|    61 |  6974 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  6975 | `			PH7_MemObjRelease(&sTop);` |
|   ! 0 |  6976 | `			return rc;` |
|     - |  6977 | `		}` |
|    30 |  6978 | `	}` |
|    85 |  6979 | `	HeapResultTop(pCtx,&sTop);` |
|    85 |  6980 | `	PH7_MemObjRelease(&sTop);` |
|    85 |  6981 | `	return PH7_OK;` |
|    46 |  6982 | `}` |
|    12 |  6983 | `static int vm_builtin_SplHeap_top(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  6984 | `{` |
|    13 |  6985 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  6986 | `	ph7_value *pV;` |
|    13 |  6987 | `	sxi32 rc = HeapCheck(pCtx);` |
|     6 |  6988 | `	SXUNUSED(nArg);` |
|     6 |  6989 | `	SXUNUSED(apArg);` |
|    13 |  6990 | `	if( rc != SXRET_OK ){` |
|     3 |  6991 | `		return rc;` |
|     - |  6992 | `	}` |
|    11 |  6993 | `	if( HeapCount(pCtx->pVm,pThis) == 0 ){` |
|     3 |  6994 | `		return PH7_VmThrowException(pCtx,"RuntimeException","Can't peek at an empty heap");` |
|     - |  6995 | `	}` |
|     9 |  6996 | `	pV = HeapAt(pCtx->pVm,pThis,0);` |
|     9 |  6997 | `	if( pV ){` |
|     9 |  6998 | `		HeapResultTop(pCtx,pV);` |
|     4 |  6999 | `	}` |
|     9 |  7000 | `	return PH7_OK;` |
|     7 |  7001 | `}` |
|    22 |  7002 | `static int vm_builtin_SplHeap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7003 | `{` |
|    11 |  7004 | `	SXUNUSED(nArg);` |
|    11 |  7005 | `	SXUNUSED(apArg);` |
|    23 |  7006 | `	ph7_result_int64(pCtx,HeapCount(pCtx->pVm,PH7_ContextThis(pCtx)));` |
|    23 |  7007 | `	return PH7_OK;` |
|     1 |  7008 | `}` |
|    50 |  7009 | `static int vm_builtin_SplHeap_isEmpty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7010 | `{` |
|    25 |  7011 | `	SXUNUSED(nArg);` |
|    25 |  7012 | `	SXUNUSED(apArg);` |
|    51 |  7013 | `	ph7_result_bool(pCtx,HeapCount(pCtx->pVm,PH7_ContextThis(pCtx)) == 0);` |
|    51 |  7014 | `	return PH7_OK;` |
|     1 |  7015 | `}` |
|    12 |  7016 | `static int vm_builtin_SplHeap_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7017 | `{` |
|     6 |  7018 | `	SXUNUSED(nArg);` |
|     6 |  7019 | `	SXUNUSED(apArg);` |
|     6 |  7020 | `	SXUNUSED(pCtx);` |
|    13 |  7021 | `	return PH7_OK;   /* php's rewind is a no-op: a heap is walked by extraction */` |
|     1 |  7022 | `}` |
|    44 |  7023 | `static int vm_builtin_SplHeap_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7024 | `{` |
|    22 |  7025 | `	SXUNUSED(nArg);` |
|    22 |  7026 | `	SXUNUSED(apArg);` |
|    45 |  7027 | `	ph7_result_bool(pCtx,HeapCount(pCtx->pVm,PH7_ContextThis(pCtx)) > 0);` |
|    45 |  7028 | `	return PH7_OK;` |
|     1 |  7029 | `}` |
|    34 |  7030 | `static int vm_builtin_SplHeap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7031 | `{` |
|    35 |  7032 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  7033 | `	ph7_value *pV;` |
|    17 |  7034 | `	SXUNUSED(nArg);` |
|    17 |  7035 | `	SXUNUSED(apArg);` |
|    35 |  7036 | `	if( HeapCount(pCtx->pVm,pThis) == 0 ){` |
|   ! 0 |  7037 | `		ph7_result_null(pCtx);` |
|   ! 0 |  7038 | `		return PH7_OK;` |
|     - |  7039 | `	}` |
|    35 |  7040 | `	pV = HeapAt(pCtx->pVm,pThis,0);` |
|    35 |  7041 | `	if( pV ){` |
|    35 |  7042 | `		HeapResultTop(pCtx,pV);` |
|    17 |  7043 | `	}` |
|    35 |  7044 | `	return PH7_OK;` |
|    18 |  7045 | `}` |
|    16 |  7046 | `static int vm_builtin_SplHeap_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7047 | `{` |
|     8 |  7048 | `	SXUNUSED(nArg);` |
|     8 |  7049 | `	SXUNUSED(apArg);` |
|    17 |  7050 | `	ph7_result_int64(pCtx,HeapCount(pCtx->pVm,PH7_ContextThis(pCtx))-1);` |
|    17 |  7051 | `	return PH7_OK;` |
|     1 |  7052 | `}` |
|    34 |  7053 | `static int vm_builtin_SplHeap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7054 | `{` |
|    35 |  7055 | `	sxi32 rc = HeapCheck(pCtx);` |
|    35 |  7056 | `	if( rc != SXRET_OK ){` |
|     5 |  7057 | `		return rc;` |
|     - |  7058 | `	}` |
|    31 |  7059 | `	if( HeapCount(pCtx->pVm,PH7_ContextThis(pCtx)) == 0 ){` |
|   ! 0 |  7060 | `		return PH7_OK;` |
|     - |  7061 | `	}` |
|    31 |  7062 | `	rc = vm_builtin_SplHeap_extract(pCtx,nArg,apArg);` |
|    31 |  7063 | `	ph7_result_null(pCtx);   /* php's next() is void; the extracted value is dropped */` |
|    31 |  7064 | `	return rc;` |
|    18 |  7065 | `}` |
|     6 |  7066 | `static int vm_builtin_SplHeap_isCorrupted(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7067 | `{` |
|     3 |  7068 | `	SXUNUSED(nArg);` |
|     3 |  7069 | `	SXUNUSED(apArg);` |
|     7 |  7070 | `	ph7_result_bool(pCtx,HeapCorrupted(PH7_ContextThis(pCtx)));` |
|     7 |  7071 | `	return PH7_OK;` |
|     1 |  7072 | `}` |
|     4 |  7073 | `static int vm_builtin_SplHeap_recover(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7074 | `{` |
|     2 |  7075 | `	SXUNUSED(nArg);` |
|     2 |  7076 | `	SXUNUSED(apArg);` |
|     5 |  7077 | `	PH7_NativeSetAttrInt(pCtx->pVm,PH7_ContextThis(pCtx),HP_CR,0);` |
|     5 |  7078 | ``	ph7_result_bool(pCtx,1);   /* php's `true` return type */`` |
|     5 |  7079 | `	return PH7_OK;` |
|     1 |  7080 | `}` |
|    30 |  7081 | `static int vm_builtin_SplMinHeap_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7082 | `{` |
|     - |  7083 | `	/* php: $value2 <=> $value1 — the SMALLEST value sits on top. */` |
|    31 |  7084 | `	if( nArg < 2 ){` |
|   ! 0 |  7085 | `		return PH7_OK;` |
|     - |  7086 | `	}` |
|    31 |  7087 | `	ph7_result_int64(pCtx,(ph7_int64)PH7_MemObjCmp(apArg[1],apArg[0],FALSE,0));` |
|    31 |  7088 | `	return PH7_OK;` |
|    16 |  7089 | `}` |
|    48 |  7090 | `static int vm_builtin_SplMaxHeap_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7091 | `{` |
|    49 |  7092 | `	if( nArg < 2 ){` |
|   ! 0 |  7093 | `		return PH7_OK;` |
|     - |  7094 | `	}` |
|    49 |  7095 | `	ph7_result_int64(pCtx,(ph7_int64)PH7_MemObjCmp(apArg[0],apArg[1],FALSE,0));` |
|    49 |  7096 | `	return PH7_OK;` |
|    25 |  7097 | `}` |
|    58 |  7098 | `static int vm_builtin_SplPq_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7099 | `{` |
|    59 |  7100 | `	if( nArg < 2 ){` |
|   ! 0 |  7101 | `		return PH7_OK;` |
|     - |  7102 | `	}` |
|    59 |  7103 | `	ph7_result_int64(pCtx,(ph7_int64)PH7_MemObjCmp(apArg[0],apArg[1],FALSE,0));` |
|    59 |  7104 | `	return PH7_OK;` |
|    30 |  7105 | `}` |
|    14 |  7106 | `static int vm_builtin_SplPq_setExtractFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7107 | `{` |
|    15 |  7108 | `	ph7_vm *pVm = pCtx->pVm;` |
|    15 |  7109 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    15 |  7110 | `	sxi64 iFlags = 0;` |
|     - |  7111 | `	sxi32 rc;` |
|    15 |  7112 | `	if( nArg < 1 ){` |
|   ! 0 |  7113 | `		return PH7_OK;` |
|     - |  7114 | `	}` |
|    15 |  7115 | `	rc = PH7_IntArgResolve(pCtx,apArg[0],"SplPriorityQueue::setExtractFlags",1,"$flags","int",&iFlags);` |
|    15 |  7116 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  7117 | `		return rc;` |
|     - |  7118 | `	}` |
|     - |  7119 | `	/* php masks to the two bits and then REFUSES an empty selection — a nonsense` |
|     - |  7120 | `	 * value is reduced, but asking for neither half is an error. */` |
|    15 |  7121 | `	iFlags &= PQ_EXTR_MASK;` |
|    15 |  7122 | `	if( iFlags == 0 ){` |
|     3 |  7123 | `		return PH7_VmThrowException(pCtx,"RuntimeException","Must specify at least one extract flag");` |
|     - |  7124 | `	}` |
|    13 |  7125 | `	PH7_NativeSetAttrInt(pVm,pThis,HP_FL,iFlags);` |
|    13 |  7126 | `	ph7_result_int64(pCtx,(ph7_int64)iFlags);` |
|    13 |  7127 | `	return PH7_OK;` |
|     8 |  7128 | `}` |
|     6 |  7129 | `static int vm_builtin_SplPq_getExtractFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7130 | `{` |
|     3 |  7131 | `	SXUNUSED(nArg);` |
|     3 |  7132 | `	SXUNUSED(apArg);` |
|     7 |  7133 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(PH7_ContextThis(pCtx),HP_FL));` |
|     7 |  7134 | `	return PH7_OK;` |
|     1 |  7135 | `}` |
|     - |  7136 | `/*` |
|     - |  7137 | ` * php's get_debug_info: flags, isCorrupted, heap. The QUEUE renders its entries` |
|     - |  7138 | ` * EXTR_BOTH-style whatever the extract flags say, because the debug view is of the` |
|     - |  7139 | ` * STORAGE rather than of what extract() would hand back.` |
|     - |  7140 | ` */` |
|     2 |  7141 | `static sxi32 HeapFillDebug(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)` |
|     1 |  7142 | `{` |
|     - |  7143 | `	ph7_value sKey,sVal,*pStore;` |
|     3 |  7144 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|     3 |  7145 | `	PH7_MemObjStringAppend(&sKey,"flags",sizeof("flags")-1);` |
|     3 |  7146 | `	PH7_MemObjInitFromInt(pVm,&sVal,PH7_NativeAttrInt(pThis,HP_FL));` |
|     3 |  7147 | `	ph7_array_add_elem(pOut,&sKey,&sVal);` |
|     3 |  7148 | `	PH7_MemObjRelease(&sKey);` |
|     3 |  7149 | `	PH7_MemObjRelease(&sVal);` |
|     3 |  7150 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|     3 |  7151 | `	PH7_MemObjStringAppend(&sKey,"isCorrupted",sizeof("isCorrupted")-1);` |
|     3 |  7152 | `	PH7_MemObjInitFromBool(pVm,&sVal,HeapCorrupted(pThis));` |
|     3 |  7153 | `	ph7_array_add_elem(pOut,&sKey,&sVal);` |
|     3 |  7154 | `	PH7_MemObjRelease(&sKey);` |
|     3 |  7155 | `	PH7_MemObjRelease(&sVal);` |
|     3 |  7156 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|     3 |  7157 | `	PH7_MemObjStringAppend(&sKey,"heap",sizeof("heap")-1);` |
|     3 |  7158 | `	pStore = HeapSlot(pVm,pThis);` |
|     3 |  7159 | `	if( pStore ){` |
|     3 |  7160 | `		ph7_array_add_elem(pOut,&sKey,pStore);` |
|     1 |  7161 | `	}` |
|     3 |  7162 | `	PH7_MemObjRelease(&sKey);` |
|     3 |  7163 | `	return PH7_OK;` |
|     1 |  7164 | `}` |
|     2 |  7165 | `static sxi32 HeapPresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|     1 |  7166 | `{` |
|     3 |  7167 | `	if( !bDebug ){` |
|     3 |  7168 | `		return PH7_OK;   /* php's (array) cast shows nothing */` |
|     - |  7169 | `	}` |
|   ! 0 |  7170 | `	return HeapFillDebug(pVm,pThis,pOut);` |
|     2 |  7171 | `}` |
|     2 |  7172 | `static int vm_builtin_SplHeap_debugInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7173 | `{` |
|     3 |  7174 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  7175 | `	ph7_value sOut;` |
|     1 |  7176 | `	SXUNUSED(nArg);` |
|     1 |  7177 | `	SXUNUSED(apArg);` |
|     3 |  7178 | `	PH7_MemObjInit(pVm,&sOut);` |
|     3 |  7179 | `	if( PH7_MemObjToHashmap(&sOut) != SXRET_OK ){` |
|   ! 0 |  7180 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 |  7181 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  7182 | `	}` |
|     3 |  7183 | `	HeapFillDebug(pVm,PH7_ContextThis(pCtx),&sOut);` |
|     3 |  7184 | `	ph7_result_value(pCtx,&sOut);` |
|     3 |  7185 | `	PH7_MemObjRelease(&sOut);` |
|     3 |  7186 | `	return PH7_OK;` |
|     2 |  7187 | `}` |
|     - |  7188 | `/*` |
|     - |  7189 | ` * php's __serialize(): [members, {flags, heap_elements}]. Note the OUTER array is` |
|     - |  7190 | ` * a two-element list whose first entry is the instance's own property table —` |
|     - |  7191 | ` * empty for a bare heap, a SUBCLASS's declared slots when there is one.` |
|     - |  7192 | ` */` |
|    20 |  7193 | `static int vm_builtin_SplHeap_serializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7194 | `{` |
|    21 |  7195 | `	ph7_vm *pVm = pCtx->pVm;` |
|    21 |  7196 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  7197 | `	ph7_value sOut,sMembers,sState,sKey,sVal,*pStore;` |
|    21 |  7198 | `	sxi32 rc = HeapCheck(pCtx);` |
|    10 |  7199 | `	SXUNUSED(nArg);` |
|    10 |  7200 | `	SXUNUSED(apArg);` |
|    21 |  7201 | `	if( rc != SXRET_OK ){` |
|     3 |  7202 | `		return rc;` |
|     - |  7203 | `	}` |
|    19 |  7204 | `	PH7_MemObjInit(pVm,&sOut);` |
|    19 |  7205 | `	PH7_MemObjInit(pVm,&sState);` |
|    18 |  7206 | `	if( SplMembersOf(pVm,pThis,&sMembers) != SXRET_OK` |
|    18 |  7207 | `	 \|\| PH7_MemObjToHashmap(&sOut) != SXRET_OK` |
|    19 |  7208 | `	 \|\| PH7_MemObjToHashmap(&sState) != SXRET_OK ){` |
|   ! 0 |  7209 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 |  7210 | `		PH7_MemObjRelease(&sMembers);` |
|   ! 0 |  7211 | `		PH7_MemObjRelease(&sState);` |
|   ! 0 |  7212 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  7213 | `	}` |
|    19 |  7214 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|    19 |  7215 | `	PH7_MemObjStringAppend(&sKey,"flags",sizeof("flags")-1);` |
|    19 |  7216 | `	PH7_MemObjInitFromInt(pVm,&sVal,PH7_NativeAttrInt(pThis,HP_FL));` |
|    19 |  7217 | `	ph7_array_add_elem(&sState,&sKey,&sVal);` |
|    19 |  7218 | `	PH7_MemObjRelease(&sKey);` |
|    19 |  7219 | `	PH7_MemObjRelease(&sVal);` |
|    19 |  7220 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|    19 |  7221 | `	PH7_MemObjStringAppend(&sKey,"heap_elements",sizeof("heap_elements")-1);` |
|    19 |  7222 | `	pStore = HeapSlot(pVm,pThis);` |
|    19 |  7223 | `	if( pStore ){` |
|    19 |  7224 | `		ph7_array_add_elem(&sState,&sKey,pStore);` |
|     9 |  7225 | `	}` |
|    19 |  7226 | `	PH7_MemObjRelease(&sKey);` |
|    19 |  7227 | `	ph7_array_add_elem(&sOut,0,&sMembers);` |
|    19 |  7228 | `	ph7_array_add_elem(&sOut,0,&sState);` |
|    19 |  7229 | `	ph7_result_value(pCtx,&sOut);` |
|    19 |  7230 | `	PH7_MemObjRelease(&sOut);` |
|    19 |  7231 | `	PH7_MemObjRelease(&sMembers);` |
|    19 |  7232 | `	PH7_MemObjRelease(&sState);` |
|    19 |  7233 | `	return PH7_OK;` |
|    11 |  7234 | `}` |
|   ! 0 |  7235 | `static sxi32 HeapUnserializeFail(ph7_context *pCtx)` |
|   ! 0 |  7236 | `{` |
|   ! 0 |  7237 | `	return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - |  7238 | `		"Unexpected data found in serialization payload");` |
|   ! 0 |  7239 | `}` |
|     6 |  7240 | `static int vm_builtin_SplHeap_unserializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7241 | `{` |
|     7 |  7242 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 |  7243 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  7244 | `	ph7_hashmap *pData;` |
|     7 |  7245 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  7246 | `	ph7_value *pState,*pFlags,*pElems,*pSlot;` |
|     - |  7247 | `	sxi64 iFlags;` |
|     - |  7248 | `	int bPq;` |
|     7 |  7249 | `	sxi32 rc = HeapCheck(pCtx);` |
|     7 |  7250 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  7251 | `		return rc;` |
|     - |  7252 | `	}` |
|     7 |  7253 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 \|\| pThis == 0 ){` |
|   ! 0 |  7254 | `		return HeapUnserializeFail(pCtx);` |
|     - |  7255 | `	}` |
|     7 |  7256 | `	pData = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     7 |  7257 | `	if( HashmapLookupIntKey(pData,1,&pNode) != SXRET_OK ){` |
|   ! 0 |  7258 | `		return HeapUnserializeFail(pCtx);` |
|     - |  7259 | `	}` |
|     7 |  7260 | `	pState = HashmapExtractNodeValue(pNode);` |
|     7 |  7261 | `	if( pState == 0 \|\| (pState->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 |  7262 | `		return HeapUnserializeFail(pCtx);` |
|     - |  7263 | `	}` |
|     7 |  7264 | `	pFlags = HeapNodePart(pState,"flags");` |
|     7 |  7265 | `	pElems = HeapNodePart(pState,"heap_elements");` |
|     6 |  7266 | `	if( pFlags == 0 \|\| (pFlags->iFlags & MEMOBJ_INT) == 0` |
|     7 |  7267 | `	 \|\| pElems == 0 \|\| (pElems->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 |  7268 | `		return HeapUnserializeFail(pCtx);` |
|     - |  7269 | `	}` |
|     - |  7270 | `	/* php VALIDATES the flags against the class: a plain heap has no user-visible` |
|     - |  7271 | `	 * flags at all, the queue must name at least one half to extract. */` |
|     7 |  7272 | `	iFlags = ph7_value_to_int64(pFlags);` |
|     7 |  7273 | `	bPq = HeapIsPq(pThis);` |
|     7 |  7274 | `	if( bPq ){` |
|     5 |  7275 | `		iFlags &= PQ_EXTR_MASK;` |
|     5 |  7276 | `		if( iFlags == 0 ){` |
|   ! 0 |  7277 | `			return HeapUnserializeFail(pCtx);` |
|     1 |  7278 | `		}` |
|     5 |  7279 | `	}else if( iFlags != 0 ){` |
|   ! 0 |  7280 | `		return HeapUnserializeFail(pCtx);` |
|     - |  7281 | `	}` |
|     7 |  7282 | `	PH7_NativeSetAttrInt(pVm,pThis,HP_FL,iFlags);` |
|     7 |  7283 | `	pSlot = PH7_NativeAttr(pThis,HP_H);` |
|     7 |  7284 | `	if( pSlot ){` |
|     7 |  7285 | `		PH7_MemObjRelease(pSlot);` |
|     7 |  7286 | `		PH7_MemObjStore(pElems,pSlot);` |
|     3 |  7287 | `	}` |
|     7 |  7288 | `	if( HashmapLookupIntKey(pData,0,&pNode) == SXRET_OK ){` |
|     7 |  7289 | `		SplMembersLoad(pThis,HashmapExtractNodeValue(pNode));` |
|     3 |  7290 | `	}` |
|     7 |  7291 | `	return PH7_OK;` |
|     4 |  7292 | `}` |
|     - |  7293 | `/*` |
|     - |  7294 | ` * The declaration. Method ORDER is spl_heap.stub.php's; php declares no` |
|     - |  7295 | ` * constructor for any of the four, SplHeap::compare is ABSTRACT PROTECTED (so` |
|     - |  7296 | ` * SplHeap itself cannot be instantiated) while the queue's is PUBLIC, and the` |
|     - |  7297 | ` * queue's default extract mode is EXTR_DATA, stamped as a property default the` |
|     - |  7298 | ` * way the DLL family's fix bit is.` |
|     - |  7299 | ` */` |
|  5740 |  7300 | `static sxi32 VmInstallSplHeap(ph7_vm *pVm)` |
|     5 |  7301 | `{` |
|     - |  7302 | `	static const PH7_NativePropDef aHeapProp[] = {` |
|     - |  7303 | `		{ HP_H,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  7304 | `		{ HP_FL, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  7305 | `		{ HP_CR, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  7306 | `	};` |
|     - |  7307 | `	static const PH7_NativePropDef aPqProp[] = {` |
|     - |  7308 | `		{ HP_H,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  7309 | `		{ HP_FL, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN,` |
|     - |  7310 | `		  { 0, 0, PH7_NATIVE_VAL_INT, PQ_EXTR_DATA, 0, 0.0 }, 0 },` |
|     - |  7311 | `		{ HP_CR, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  7312 | `	};` |
|     - |  7313 | `	static const PH7_NativeMethodDef aHeapMethod[] = {` |
|     - |  7314 | `		{ "extract",               PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplHeap_extract },` |
|     - |  7315 | `		{ "insert",                PH7_MOD_PUBLIC, "mixed $value", "@true",` |
|     - |  7316 | `		  vm_builtin_SplHeap_insert },` |
|     - |  7317 | `		{ "top",                   PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplHeap_top },` |
|     - |  7318 | `		{ "count",                 PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplHeap_count },` |
|     - |  7319 | `		{ "isEmpty",               PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplHeap_isEmpty },` |
|     - |  7320 | `		{ "rewind",                PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplHeap_rewind },` |
|     - |  7321 | `		{ "current",               PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplHeap_current },` |
|     - |  7322 | `		{ "key",                   PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplHeap_key },` |
|     - |  7323 | `		{ "next",                  PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplHeap_next },` |
|     - |  7324 | `		{ "valid",                 PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplHeap_valid },` |
|     - |  7325 | `		{ "recoverFromCorruption", PH7_MOD_PUBLIC, "", "@true", vm_builtin_SplHeap_recover },` |
|     - |  7326 | `		{ "compare",               PH7_MOD_PROTECTED\|PH7_MOD_ABSTRACT,` |
|     - |  7327 | `		  "mixed $value1, mixed $value2", "@int", 0 },` |
|     - |  7328 | `		{ "isCorrupted",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplHeap_isCorrupted },` |
|     - |  7329 | `		{ "__debugInfo",           PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplHeap_debugInfo },` |
|     - |  7330 | `		{ "__serialize",           PH7_MOD_PUBLIC, "", "@array",` |
|     - |  7331 | `		  vm_builtin_SplHeap_serializeMagic },` |
|     - |  7332 | `		{ "__unserialize",         PH7_MOD_PUBLIC, "array $data", "@void",` |
|     - |  7333 | `		  vm_builtin_SplHeap_unserializeMagic },` |
|     - |  7334 | `	};` |
|     - |  7335 | `	static const PH7_NativeMethodDef aMinMethod[] = {` |
|     - |  7336 | `		{ "compare", PH7_MOD_PROTECTED, "mixed $value1, mixed $value2", "@int",` |
|     - |  7337 | `		  vm_builtin_SplMinHeap_compare },` |
|     - |  7338 | `	};` |
|     - |  7339 | `	static const PH7_NativeMethodDef aMaxMethod[] = {` |
|     - |  7340 | `		{ "compare", PH7_MOD_PROTECTED, "mixed $value1, mixed $value2", "@int",` |
|     - |  7341 | `		  vm_builtin_SplMaxHeap_compare },` |
|     - |  7342 | `	};` |
|     - |  7343 | `	static const PH7_NativeConstDef aPqConst[] = {` |
|     - |  7344 | `		{ "EXTR_BOTH",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PQ_EXTR_BOTH, 0, 0.0 },` |
|     - |  7345 | `		{ "EXTR_PRIORITY", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PQ_EXTR_PRIORITY, 0, 0.0 },` |
|     - |  7346 | `		{ "EXTR_DATA",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PQ_EXTR_DATA, 0, 0.0 },` |
|     - |  7347 | `	};` |
|     - |  7348 | `	static const PH7_NativeMethodDef aPqMethod[] = {` |
|     - |  7349 | `		{ "compare",               PH7_MOD_PUBLIC, "mixed $priority1, mixed $priority2", "@int",` |
|     - |  7350 | `		  vm_builtin_SplPq_compare },` |
|     - |  7351 | `		{ "insert",                PH7_MOD_PUBLIC, "mixed $value, mixed $priority", "@true",` |
|     - |  7352 | `		  vm_builtin_SplHeap_insert },` |
|     - |  7353 | `		{ "setExtractFlags",       PH7_MOD_PUBLIC, "int $flags", "@int",` |
|     - |  7354 | `		  vm_builtin_SplPq_setExtractFlags },` |
|     - |  7355 | `		{ "top",                   PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplHeap_top },` |
|     - |  7356 | `		{ "extract",               PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplHeap_extract },` |
|     - |  7357 | `		{ "count",                 PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplHeap_count },` |
|     - |  7358 | `		{ "isEmpty",               PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplHeap_isEmpty },` |
|     - |  7359 | `		{ "rewind",                PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplHeap_rewind },` |
|     - |  7360 | `		{ "current",               PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplHeap_current },` |
|     - |  7361 | `		{ "key",                   PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplHeap_key },` |
|     - |  7362 | `		{ "next",                  PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplHeap_next },` |
|     - |  7363 | `		{ "valid",                 PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplHeap_valid },` |
|     - |  7364 | `		{ "recoverFromCorruption", PH7_MOD_PUBLIC, "", "@true", vm_builtin_SplHeap_recover },` |
|     - |  7365 | `		{ "isCorrupted",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplHeap_isCorrupted },` |
|     - |  7366 | `		{ "getExtractFlags",       PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplPq_getExtractFlags },` |
|     - |  7367 | `		{ "__debugInfo",           PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplHeap_debugInfo },` |
|     - |  7368 | `		{ "__serialize",           PH7_MOD_PUBLIC, "", "@array",` |
|     - |  7369 | `		  vm_builtin_SplHeap_serializeMagic },` |
|     - |  7370 | `		{ "__unserialize",         PH7_MOD_PUBLIC, "array $data", "@void",` |
|     - |  7371 | `		  vm_builtin_SplHeap_unserializeMagic },` |
|     - |  7372 | `	};` |
|     - |  7373 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - |  7374 | `		{ "SplPriorityQueue", 0, "Iterator,Countable", 0,` |
|     - |  7375 | `		  aPqMethod, SX_ARRAYSIZE(aPqMethod),` |
|     - |  7376 | `		  aPqConst, SX_ARRAYSIZE(aPqConst),` |
|     - |  7377 | `		  aPqProp, SX_ARRAYSIZE(aPqProp), 0, 0, HeapPresent },` |
|     - |  7378 | `		{ "SplHeap", 0, "Iterator,Countable", PH7_CLASS_ABSTRACT,` |
|     - |  7379 | `		  aHeapMethod, SX_ARRAYSIZE(aHeapMethod), 0, 0,` |
|     - |  7380 | `		  aHeapProp, SX_ARRAYSIZE(aHeapProp), 0, 0, HeapPresent },` |
|     - |  7381 | `		{ "SplMinHeap", "SplHeap", 0, 0,` |
|     - |  7382 | `		  aMinMethod, SX_ARRAYSIZE(aMinMethod), 0, 0, 0, 0, 0, 0, HeapPresent },` |
|     - |  7383 | `		{ "SplMaxHeap", "SplHeap", 0, 0,` |
|     - |  7384 | `		  aMaxMethod, SX_ARRAYSIZE(aMaxMethod), 0, 0, 0, 0, 0, 0, HeapPresent },` |
|     - |  7385 | `	};` |
|  5745 |  7386 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 |  7387 | `}` |
|     - |  7388 | `/*` |
|     - |  7389 | ` * ---------------------------------------------------------------------------` |
|     - |  7390 | ` * SplFixedArray.` |
|     - |  7391 | ` *` |
|     - |  7392 | `` * php PRESENTS this one as its own elements: `var_dump` shows`` |
|     - |  7393 | `` * `object(SplFixedArray)#1 (3) { [0]=> … }`, the `(array)` cast yields the`` |
|     - |  7394 | `` * elements with their integer keys, and `serialize()` writes them as INTEGER`` |
|     - |  7395 | ``  * property names (`O:13:"SplFixedArray":3:{i:0;…}`) because `__serialize()` `` |
|     - |  7396 | `` * simply hands the element array back. The chunk exposed `__a`/`__n` on all`` |
|     - |  7397 | ` * three surfaces instead.` |
|     - |  7398 | ` *` |
|     - |  7399 | `` * `getIterator()` answers php's **InternalIterator**, not a Generator. The chunk`` |
|     - |  7400 | ` * yielded, which is one class name wrong on a php-visible surface and also the` |
|     - |  7401 | `` * thing rule 5's InternalIterator exists for — `pIterVtab` plus`` |
|     - |  7402 | `` * `PH7_NativeIteratorNew()` is the whole implementation, and it gets php's`` |
|     - |  7403 | ` * independent-cursor behaviour (two getIterator() calls, or nested foreach, walk` |
|     - |  7404 | ` * separately) for free.` |
|     - |  7405 | ` *` |
|     - |  7406 | ` * php's offset rule is its own: an int, a bool and an INTEGER-LIKE string are` |
|     - |  7407 | `` * accepted, everything else is `Cannot access offset of type %s on SplFixedArray`.`` |
|     - |  7408 | ` * The chunk refused bools. A FLOAT offset stays refused here, which is not php's` |
|     - |  7409 | ` * answer (php truncates, with a precision deprecation when it is lossy) but IS` |
|     - |  7410 | `` * PHL's engine-wide one — `$a[1.5]` on a plain array raises the same TypeError,`` |
|     - |  7411 | ` * so the class stays consistent with the engine it lives in rather than uniquely` |
|     - |  7412 | ` * permissive (§10).` |
|     - |  7413 | ` */` |
|     - |  7414 | `#define FA_A "__a"   /* the elements, 0..n-1 */` |
|     - |  7415 | `#define FA_N "__n"   /* php's size */` |
|     - |  7416 |  |
|   786 |  7417 | `static ph7_value * FaSlot(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  7418 | `{` |
|   787 |  7419 | `	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,FA_A) : 0;` |
|   787 |  7420 | `	if( pSlot == 0 ){` |
|   ! 0 |  7421 | `		return 0;` |
|     - |  7422 | `	}` |
|   787 |  7423 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   113 |  7424 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|   ! 0 |  7425 | `			return 0;` |
|     - |  7426 | `		}` |
|    56 |  7427 | `	}` |
|   787 |  7428 | `	if( PH7_HashmapCowSeparate(pVm,pSlot) == 0 ){` |
|   ! 0 |  7429 | `		return 0;` |
|     - |  7430 | `	}` |
|   787 |  7431 | `	return pSlot;` |
|   394 |  7432 | `}` |
|   740 |  7433 | `static ph7_hashmap * FaMap(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  7434 | `{` |
|   741 |  7435 | `	ph7_value *pSlot = FaSlot(pVm,pThis);` |
|   741 |  7436 | `	return pSlot ? (ph7_hashmap *)pSlot->x.pOther : 0;` |
|     1 |  7437 | `}` |
|   462 |  7438 | `static sxi64 FaSize(ph7_class_instance *pThis)` |
|     1 |  7439 | `{` |
|   463 |  7440 | `	return pThis ? PH7_NativeAttrInt(pThis,FA_N) : 0;` |
|     1 |  7441 | `}` |
|    96 |  7442 | `static ph7_value * FaAt(ph7_vm *pVm,ph7_class_instance *pThis,sxi64 i)` |
|     1 |  7443 | `{` |
|    97 |  7444 | `	ph7_hashmap *pMap = FaMap(pVm,pThis);` |
|    97 |  7445 | `	ph7_hashmap_node *pNode = 0;` |
|    97 |  7446 | `	if( pMap == 0 \|\| HashmapLookupIntKey(pMap,i,&pNode) != SXRET_OK ){` |
|   ! 0 |  7447 | `		return 0;` |
|     - |  7448 | `	}` |
|    97 |  7449 | `	return HashmapExtractNodeValue(pNode);` |
|    49 |  7450 | `}` |
|   522 |  7451 | `static void FaPut(ph7_vm *pVm,ph7_class_instance *pThis,sxi64 i,ph7_value *pVal)` |
|     1 |  7452 | `{` |
|   523 |  7453 | `	ph7_hashmap *pMap = FaMap(pVm,pThis);` |
|     - |  7454 | `	ph7_value sKey;` |
|   523 |  7455 | `	if( pMap == 0 ){` |
|   ! 0 |  7456 | `		return;` |
|     - |  7457 | `	}` |
|   523 |  7458 | `	PH7_MemObjInitFromInt(pVm,&sKey,i);` |
|   523 |  7459 | `	PH7_HashmapInsert(pMap,&sKey,pVal);` |
|   523 |  7460 | `	PH7_MemObjRelease(&sKey);` |
|   262 |  7461 | `}` |
|     - |  7462 | `/*` |
|     - |  7463 | ` * php's offset decode. An INTEGER-LIKE string is accepted (php's own` |
|     - |  7464 | `` * `ZEND_HANDLE_NUMERIC_STRING`), a bool is its 0/1, and every other type is named`` |
|     - |  7465 | ` * in the refusal. Returns 0 and leaves a TypeError raised when it cannot decode.` |
|     - |  7466 | ` */` |
|   276 |  7467 | `static int FaOffset(ph7_context *pCtx,ph7_value *pArg,sxi64 *piOut,sxi32 *pRc)` |
|     1 |  7468 | `{` |
|   277 |  7469 | `	*pRc = PH7_OK;` |
|   277 |  7470 | `	if( pArg == 0 ){` |
|   ! 0 |  7471 | `		*piOut = 0;` |
|   ! 0 |  7472 | `		return 1;` |
|     - |  7473 | `	}` |
|   277 |  7474 | `	if( pArg->iFlags & MEMOBJ_INT ){` |
|   253 |  7475 | `		*piOut = pArg->x.iVal;` |
|   253 |  7476 | `		return 1;` |
|     - |  7477 | `	}` |
|    25 |  7478 | `	if( pArg->iFlags & MEMOBJ_BOOL ){` |
|     5 |  7479 | `		*piOut = pArg->x.iVal ? 1 : 0;` |
|     5 |  7480 | `		return 1;` |
|     - |  7481 | `	}` |
|    21 |  7482 | `	if( (pArg->iFlags & MEMOBJ_STRING) && PH7_MemObjStringIsNumeric(pArg) ){` |
|     - |  7483 | `		ph7_value sTmp;` |
|     5 |  7484 | `		PH7_MemObjInit(pCtx->pVm,&sTmp);` |
|     5 |  7485 | `		PH7_MemObjStore(pArg,&sTmp);` |
|     5 |  7486 | `		PH7_MemObjToInteger(&sTmp);` |
|     5 |  7487 | `		*piOut = sTmp.x.iVal;` |
|     5 |  7488 | `		PH7_MemObjRelease(&sTmp);` |
|     5 |  7489 | `		return 1;` |
|     - |  7490 | `	}` |
|    17 |  7491 | `	*piOut = 0;` |
|    17 |  7492 | `	if( pArg->iFlags & MEMOBJ_OBJ ){` |
|     - |  7493 | `		/* php names the CLASS here, as get_debug_type() does, not the word` |
|     - |  7494 | `		 * "object" — the same rule the store's offsets follow. */` |
|     3 |  7495 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     3 |  7496 | `		SyString *pName = pInst && pInst->pClass ? &pInst->pClass->sName : 0;` |
|     4 |  7497 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|     1 |  7498 | `			"Cannot access offset of type %z on SplFixedArray",pName);` |
|     3 |  7499 | `		return 0;` |
|     - |  7500 | `	}` |
|    22 |  7501 | `	*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|     7 |  7502 | `		"Cannot access offset of type %s on SplFixedArray",ph7_type_name(pArg));` |
|    15 |  7503 | `	return 0;` |
|   139 |  7504 | `}` |
|    10 |  7505 | `static sxi32 FaOutOfBounds(ph7_context *pCtx)` |
|     1 |  7506 | `{` |
|    11 |  7507 | `	return PH7_VmThrowException(pCtx,"OutOfBoundsException","Index invalid or out of range");` |
|     1 |  7508 | `}` |
|     - |  7509 | ``/* php's setSize: grow with nulls, shrink by dropping the tail, answer `true`. */`` |
|   122 |  7510 | `static sxi32 FaResize(ph7_vm *pVm,ph7_class_instance *pThis,sxi64 nNew)` |
|     1 |  7511 | `{` |
|   123 |  7512 | `	sxi64 nOld = FaSize(pThis);` |
|   123 |  7513 | `	ph7_hashmap *pMap = FaMap(pVm,pThis);` |
|     - |  7514 | `	sxi64 i;` |
|   123 |  7515 | `	if( pMap == 0 ){` |
|   ! 0 |  7516 | `		return SXERR_MEM;` |
|     - |  7517 | `	}` |
|   133 |  7518 | `	for( i = nNew ; i < nOld ; ++i ){` |
|    11 |  7519 | `		ph7_hashmap_node *pNode = 0;` |
|    11 |  7520 | `		if( HashmapLookupIntKey(pMap,i,&pNode) == SXRET_OK ){` |
|    11 |  7521 | `			PH7_HashmapUnlinkNode(pNode,TRUE);` |
|     5 |  7522 | `		}` |
|     6 |  7523 | `	}` |
|   407 |  7524 | `	for( i = nOld ; i < nNew ; ++i ){` |
|     - |  7525 | `		ph7_value sNull;` |
|   285 |  7526 | `		PH7_MemObjInit(pVm,&sNull);` |
|   285 |  7527 | `		FaPut(pVm,pThis,i,&sNull);` |
|   285 |  7528 | `		PH7_MemObjRelease(&sNull);` |
|   143 |  7529 | `	}` |
|   123 |  7530 | `	PH7_NativeSetAttrInt(pVm,pThis,FA_N,nNew);` |
|   123 |  7531 | `	return SXRET_OK;` |
|    62 |  7532 | `}` |
|    96 |  7533 | `static int vm_builtin_SplFixedArray_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7534 | `{` |
|    97 |  7535 | `	ph7_vm *pVm = pCtx->pVm;` |
|    97 |  7536 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    97 |  7537 | `	sxi64 nSize = 0;` |
|    97 |  7538 | `	if( pThis == 0 ){` |
|   ! 0 |  7539 | `		return PH7_OK;` |
|     - |  7540 | `	}` |
|    97 |  7541 | `	if( nArg > 0 ){` |
|    93 |  7542 | `		sxi32 rc = PH7_IntArgResolve(pCtx,apArg[0],"SplFixedArray::__construct",1,"$size","int",&nSize);` |
|    93 |  7543 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  7544 | `			return rc;` |
|     - |  7545 | `		}` |
|    46 |  7546 | `	}` |
|    97 |  7547 | `	if( nSize < 0 ){` |
|     - |  7548 | `		/* php words this from __construct(), not from the setSize() it forwards to —` |
|     - |  7549 | ``		 * which is what the chunk's `$this->setSize()` reported. */`` |
|     3 |  7550 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  7551 | `			"SplFixedArray::__construct(): Argument #1 ($size) must be greater than or equal to 0");` |
|     - |  7552 | `	}` |
|    95 |  7553 | `	FaResize(pVm,pThis,nSize);` |
|    95 |  7554 | `	return PH7_OK;` |
|    49 |  7555 | `}` |
|     6 |  7556 | `static int vm_builtin_SplFixedArray_getSize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7557 | `{` |
|     3 |  7558 | `	SXUNUSED(nArg);` |
|     3 |  7559 | `	SXUNUSED(apArg);` |
|     7 |  7560 | `	ph7_result_int64(pCtx,FaSize(PH7_ContextThis(pCtx)));` |
|     7 |  7561 | `	return PH7_OK;` |
|     1 |  7562 | `}` |
|    12 |  7563 | `static int vm_builtin_SplFixedArray_setSize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7564 | `{` |
|    13 |  7565 | `	sxi64 nSize = 0;` |
|     - |  7566 | `	sxi32 rc;` |
|    13 |  7567 | `	if( nArg < 1 ){` |
|   ! 0 |  7568 | `		return PH7_OK;` |
|     - |  7569 | `	}` |
|    13 |  7570 | `	rc = PH7_IntArgResolve(pCtx,apArg[0],"SplFixedArray::setSize",1,"$size","int",&nSize);` |
|    13 |  7571 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  7572 | `		return rc;` |
|     - |  7573 | `	}` |
|    13 |  7574 | `	if( nSize < 0 ){` |
|     3 |  7575 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  7576 | `			"SplFixedArray::setSize(): Argument #1 ($size) must be greater than or equal to 0");` |
|     - |  7577 | `	}` |
|    11 |  7578 | `	FaResize(pCtx->pVm,PH7_ContextThis(pCtx),nSize);` |
|    11 |  7579 | ``	ph7_result_bool(pCtx,1);   /* php's `true` return type */`` |
|    11 |  7580 | `	return PH7_OK;` |
|     7 |  7581 | `}` |
|    12 |  7582 | `static int vm_builtin_SplFixedArray_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7583 | `{` |
|     6 |  7584 | `	SXUNUSED(nArg);` |
|     6 |  7585 | `	SXUNUSED(apArg);` |
|    13 |  7586 | `	ph7_result_int64(pCtx,FaSize(PH7_ContextThis(pCtx)));` |
|    13 |  7587 | `	return PH7_OK;` |
|     1 |  7588 | `}` |
|    32 |  7589 | `static int vm_builtin_SplFixedArray_toArray(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7590 | `{` |
|    33 |  7591 | `	ph7_value *pSlot = FaSlot(pCtx->pVm,PH7_ContextThis(pCtx));` |
|    16 |  7592 | `	SXUNUSED(nArg);` |
|    16 |  7593 | `	SXUNUSED(apArg);` |
|    33 |  7594 | `	if( pSlot ){` |
|    33 |  7595 | `		ph7_result_value(pCtx,pSlot);` |
|    16 |  7596 | `	}` |
|    33 |  7597 | `	return PH7_OK;` |
|     1 |  7598 | `}` |
|    20 |  7599 | `static int vm_builtin_SplFixedArray_offsetExists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7600 | `{` |
|    21 |  7601 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    21 |  7602 | `	sxi64 iIdx = 0;` |
|    21 |  7603 | `	sxi32 rc = PH7_OK;` |
|     - |  7604 | `	ph7_value *pVal;` |
|    21 |  7605 | `	if( nArg < 1 ){` |
|   ! 0 |  7606 | `		return PH7_OK;` |
|     - |  7607 | `	}` |
|     - |  7608 | `	/* offsetExists RAISES for an undecodable offset exactly as the other three do —` |
|     - |  7609 | ``	 * `isset($f['x'])` is a TypeError, not a false — and answers false only for a`` |
|     - |  7610 | `	 * decodable index that is out of range or holds null. */` |
|    21 |  7611 | `	if( !FaOffset(pCtx,apArg[0],&iIdx,&rc) ){` |
|     5 |  7612 | `		return rc;` |
|     - |  7613 | `	}` |
|    17 |  7614 | `	if( iIdx < 0 \|\| iIdx >= FaSize(pThis) ){` |
|     7 |  7615 | `		ph7_result_bool(pCtx,0);` |
|     7 |  7616 | `		return PH7_OK;` |
|     - |  7617 | `	}` |
|     - |  7618 | `	/* php's isset() semantics: an unset slot holds null and is NOT set. */` |
|    11 |  7619 | `	pVal = FaAt(pCtx->pVm,pThis,iIdx);` |
|    11 |  7620 | `	ph7_result_bool(pCtx,pVal != 0 && (pVal->iFlags & MEMOBJ_NULL) == 0);` |
|    11 |  7621 | `	return PH7_OK;` |
|    11 |  7622 | `}` |
|    38 |  7623 | `static int vm_builtin_SplFixedArray_offsetGet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7624 | `{` |
|    39 |  7625 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    39 |  7626 | `	sxi64 iIdx = 0;` |
|    39 |  7627 | `	sxi32 rc = PH7_OK;` |
|     - |  7628 | `	ph7_value *pVal;` |
|    39 |  7629 | `	if( nArg < 1 ){` |
|   ! 0 |  7630 | `		return PH7_OK;` |
|     - |  7631 | `	}` |
|    39 |  7632 | `	if( !FaOffset(pCtx,apArg[0],&iIdx,&rc) ){` |
|     7 |  7633 | `		return rc;` |
|     - |  7634 | `	}` |
|    33 |  7635 | `	if( iIdx < 0 \|\| iIdx >= FaSize(pThis) ){` |
|     7 |  7636 | `		return FaOutOfBounds(pCtx);` |
|     - |  7637 | `	}` |
|    27 |  7638 | `	pVal = FaAt(pCtx->pVm,pThis,iIdx);` |
|    27 |  7639 | `	if( pVal ){` |
|    27 |  7640 | `		ph7_result_value(pCtx,pVal);` |
|    13 |  7641 | `	}` |
|    27 |  7642 | `	return PH7_OK;` |
|    20 |  7643 | `}` |
|   216 |  7644 | `static int vm_builtin_SplFixedArray_offsetSet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7645 | `{` |
|   217 |  7646 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   217 |  7647 | `	sxi64 iIdx = 0;` |
|   217 |  7648 | `	sxi32 rc = PH7_OK;` |
|   217 |  7649 | `	if( nArg < 2 ){` |
|   ! 0 |  7650 | `		return PH7_OK;` |
|     - |  7651 | `	}` |
|   217 |  7652 | `	if( !FaOffset(pCtx,apArg[0],&iIdx,&rc) ){` |
|     7 |  7653 | `		return rc;` |
|     - |  7654 | `	}` |
|   211 |  7655 | `	if( iIdx < 0 \|\| iIdx >= FaSize(pThis) ){` |
|     5 |  7656 | `		return FaOutOfBounds(pCtx);` |
|     - |  7657 | `	}` |
|   207 |  7658 | `	FaPut(pCtx->pVm,pThis,iIdx,apArg[1]);` |
|   207 |  7659 | `	return PH7_OK;` |
|   109 |  7660 | `}` |
|     2 |  7661 | `static int vm_builtin_SplFixedArray_offsetUnset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7662 | `{` |
|     3 |  7663 | `	ph7_vm *pVm = pCtx->pVm;` |
|     3 |  7664 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     3 |  7665 | `	sxi64 iIdx = 0;` |
|     3 |  7666 | `	sxi32 rc = PH7_OK;` |
|     - |  7667 | `	ph7_value sNull;` |
|     3 |  7668 | `	if( nArg < 1 ){` |
|   ! 0 |  7669 | `		return PH7_OK;` |
|     - |  7670 | `	}` |
|     3 |  7671 | `	if( !FaOffset(pCtx,apArg[0],&iIdx,&rc) ){` |
|   ! 0 |  7672 | `		return rc;` |
|     - |  7673 | `	}` |
|     3 |  7674 | `	if( iIdx < 0 \|\| iIdx >= FaSize(pThis) ){` |
|   ! 0 |  7675 | `		return FaOutOfBounds(pCtx);` |
|     - |  7676 | `	}` |
|     - |  7677 | `	/* The slot survives at its index and becomes null: the array is FIXED. */` |
|     3 |  7678 | `	PH7_MemObjInit(pVm,&sNull);` |
|     3 |  7679 | `	FaPut(pVm,pThis,iIdx,&sNull);` |
|     3 |  7680 | `	PH7_MemObjRelease(&sNull);` |
|     3 |  7681 | `	return PH7_OK;` |
|     2 |  7682 | `}` |
|    24 |  7683 | `static int vm_builtin_SplFixedArray_fromArray(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7684 | `{` |
|    25 |  7685 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  7686 | `	ph7_class *pCls;` |
|     - |  7687 | `	ph7_class_instance *pNew;` |
|     - |  7688 | `	ph7_hashmap *pSrc;` |
|     - |  7689 | `	ph7_hashmap_node *pNode,*pPrev;` |
|    25 |  7690 | `	int bPreserve = 1;` |
|    25 |  7691 | `	sxi64 nMax = -1, nNext = 0;` |
|    25 |  7692 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 |  7693 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  7694 | `			"SplFixedArray::fromArray(): Argument #1 ($array) must be of type array, %s given",` |
|   ! 0 |  7695 | `			nArg < 1 ? "none" : ph7_type_name(apArg[0]));` |
|     - |  7696 | `	}` |
|    25 |  7697 | `	if( nArg > 1 ){` |
|     7 |  7698 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|     3 |  7699 | `	}` |
|    25 |  7700 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     - |  7701 | `	/* php walks the keys FIRST and refuses the whole call before building anything. */` |
|    25 |  7702 | `	if( bPreserve ){` |
|    39 |  7703 | `		for( pNode = pSrc->pFirst ; pNode ; pNode = pPrev ){` |
|    27 |  7704 | `			pPrev = pNode->pPrev;` |
|    27 |  7705 | `			if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey < 0 ){` |
|     7 |  7706 | `				return PH7_VmThrowException(pCtx,"InvalidArgumentException",` |
|     - |  7707 | `					"array must contain only positive integer keys");` |
|     - |  7708 | `			}` |
|    21 |  7709 | `			if( pNode->xKey.iKey > nMax ){` |
|    19 |  7710 | `				nMax = pNode->xKey.iKey;` |
|     9 |  7711 | `			}` |
|    21 |  7712 | `			if( pNode == pSrc->pFirst && pPrev == 0 ){` |
|   ! 0 |  7713 | `				break;` |
|     - |  7714 | `			}` |
|    11 |  7715 | `		}` |
|     6 |  7716 | `	}` |
|    19 |  7717 | `	pCls = PH7_VmExtractClass(pVm,"SplFixedArray",sizeof("SplFixedArray")-1,FALSE,0);` |
|    19 |  7718 | `	if( pCls == 0 ){` |
|   ! 0 |  7719 | `		return PH7_OK;` |
|     - |  7720 | `	}` |
|    19 |  7721 | `	pNew = PH7_NewClassInstance(pVm,pCls);` |
|    19 |  7722 | `	if( pNew == 0 ){` |
|   ! 0 |  7723 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  7724 | `	}` |
|    19 |  7725 | `	pNew->iRef++;` |
|    19 |  7726 | `	FaResize(pVm,pNew,bPreserve ? nMax + 1 : (sxi64)pSrc->nEntry);` |
|    35 |  7727 | `	for( pNode = pSrc->pFirst ; pNode ; pNode = pPrev ){` |
|    31 |  7728 | `		ph7_value *pVal = HashmapExtractNodeValue(pNode);` |
|    31 |  7729 | `		pPrev = pNode->pPrev;` |
|    31 |  7730 | `		if( pVal ){` |
|    31 |  7731 | `			FaPut(pVm,pNew,bPreserve ? pNode->xKey.iKey : nNext,pVal);` |
|    15 |  7732 | `		}` |
|    31 |  7733 | `		nNext++;` |
|    31 |  7734 | `		if( pPrev == 0 ){` |
|    15 |  7735 | `			break;` |
|     - |  7736 | `		}` |
|     9 |  7737 | `	}` |
|    19 |  7738 | `	PH7_NativeResultObject(pCtx,pNew);` |
|    19 |  7739 | `	PH7_ClassInstanceUnref(pNew);` |
|    19 |  7740 | `	return PH7_OK;` |
|    13 |  7741 | `}` |
|     - |  7742 | `/* php's getIterator() answers an InternalIterator over the elements — the same` |
|     - |  7743 | ` * machinery every native IteratorAggregate here uses, which is also what makes` |
|     - |  7744 | ` * two iterators over one array independent. */` |
|    66 |  7745 | `static void FaIterSettle(ph7_vm *pVm,ph7_class_instance *pIt)` |
|     1 |  7746 | `{` |
|    67 |  7747 | `	ph7_class_instance *pSrc = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);` |
|    67 |  7748 | `	sxi64 iPos = PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS);` |
|     - |  7749 | `	ph7_value *pVal;` |
|    67 |  7750 | `	if( pSrc == 0 \|\| iPos < 0 \|\| iPos >= FaSize(pSrc) ){` |
|    13 |  7751 | `		PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|    13 |  7752 | `		return;` |
|     - |  7753 | `	}` |
|    55 |  7754 | `	pVal = FaAt(&(*pVm),pSrc,iPos);` |
|    55 |  7755 | `	if( pVal ){` |
|    82 |  7756 | `		PH7_NativeSetProp(&(*pVm),pIt,PH7_NATIVE_IT_CUR,` |
|    27 |  7757 | `			(int)SyStrlen(PH7_NATIVE_IT_CUR),pVal);` |
|    27 |  7758 | `	}` |
|    55 |  7759 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_KEY,iPos);` |
|    55 |  7760 | `	PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,0);` |
|    34 |  7761 | `}` |
|    34 |  7762 | `static void FaIterRewind(ph7_vm *pVm,ph7_class_instance *pIt)` |
|     1 |  7763 | `{` |
|    35 |  7764 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_POS,0);` |
|    35 |  7765 | `	FaIterSettle(&(*pVm),pIt);` |
|    35 |  7766 | `}` |
|    32 |  7767 | `static void FaIterNext(ph7_vm *pVm,ph7_class_instance *pIt)` |
|     1 |  7768 | `{` |
|    49 |  7769 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_POS,` |
|    32 |  7770 | `		PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS) + 1);` |
|    33 |  7771 | `	FaIterSettle(&(*pVm),pIt);` |
|    33 |  7772 | `}` |
|     - |  7773 | `static const PH7_NativeIterVtab sFaIterVtab = { FaIterRewind, FaIterNext, 0, 0 };` |
|    18 |  7774 | `static int vm_builtin_SplFixedArray_getIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7775 | `{` |
|    19 |  7776 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  7777 | `	ph7_class_instance *pIt;` |
|     9 |  7778 | `	SXUNUSED(nArg);` |
|     9 |  7779 | `	SXUNUSED(apArg);` |
|    19 |  7780 | `	if( pThis == 0 ){` |
|   ! 0 |  7781 | `		return PH7_OK;` |
|     - |  7782 | `	}` |
|    19 |  7783 | `	pIt = PH7_NativeIteratorNew(pCtx->pVm,pThis);` |
|    19 |  7784 | `	if( pIt == 0 ){` |
|   ! 0 |  7785 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  7786 | `	}` |
|    19 |  7787 | `	PH7_NativeResultObject(pCtx,pIt);` |
|    19 |  7788 | `	return PH7_OK;` |
|    10 |  7789 | `}` |
|   ! 0 |  7790 | `static int vm_builtin_SplFixedArray_wakeup(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 |  7791 | `{` |
|   ! 0 |  7792 | `	SXUNUSED(nArg);` |
|   ! 0 |  7793 | `	SXUNUSED(apArg);` |
|   ! 0 |  7794 | `	SXUNUSED(pCtx);` |
|   ! 0 |  7795 | `	return PH7_OK;   /* php 8.4 keeps it, deprecated, doing nothing */` |
|   ! 0 |  7796 | `}` |
|     - |  7797 | `/*` |
|     - |  7798 | ` * php's __serialize() here is NOT toArray(): the members ride in the SAME array as` |
|     - |  7799 | ` * the elements, told apart by their key — an INT key is an element and a STRING key` |
|     - |  7800 | ` * is a property. That is the whole reason the payload of a SplFixedArray subclass` |
|     - |  7801 | `` * reads `{i:0;N;i:1;N;s:1:"p";i:9;}` and not a nested pair.`` |
|     - |  7802 | ` */` |
|    14 |  7803 | `static int vm_builtin_SplFixedArray_serializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7804 | `{` |
|    15 |  7805 | `	ph7_vm *pVm = pCtx->pVm;` |
|    15 |  7806 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  7807 | `	ph7_value sOut,*pSlot;` |
|     7 |  7808 | `	SXUNUSED(nArg);` |
|     7 |  7809 | `	SXUNUSED(apArg);` |
|    15 |  7810 | `	pSlot = FaSlot(pVm,pThis);` |
|    15 |  7811 | `	PH7_MemObjInit(pVm,&sOut);` |
|    15 |  7812 | `	if( pSlot ){` |
|    15 |  7813 | `		PH7_MemObjStore(pSlot,&sOut);` |
|     7 |  7814 | `	}` |
|    15 |  7815 | `	if( PH7_MemObjToHashmap(&sOut) != SXRET_OK ){` |
|   ! 0 |  7816 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 |  7817 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  7818 | `	}` |
|    15 |  7819 | `	SplAddMembers(pVm,pThis,&sOut);   /* string keys, beside the int-keyed elements */` |
|    15 |  7820 | `	ph7_result_value(pCtx,&sOut);` |
|    15 |  7821 | `	PH7_MemObjRelease(&sOut);` |
|    15 |  7822 | `	return PH7_OK;` |
|     8 |  7823 | `}` |
|     - |  7824 | `/* Split the payload back apart: int keys rebuild the elements, string keys the` |
|     - |  7825 | ` * properties. The element count is what the INT half holds, not the whole array. */` |
|     - |  7826 | `typedef struct fa_unser_ctx fa_unser_ctx;` |
|     - |  7827 | `struct fa_unser_ctx` |
|     - |  7828 | `{` |
|     - |  7829 | `	ph7_class_instance *pThis;` |
|     - |  7830 | `	ph7_value *pElems;` |
|     - |  7831 | `	sxi64 nElem;` |
|     - |  7832 | `};` |
|    18 |  7833 | `static int FaUnserWalk(ph7_value *pKey,ph7_value *pVal,void *pUserData)` |
|     1 |  7834 | `{` |
|    19 |  7835 | `	fa_unser_ctx *pFa = (fa_unser_ctx *)pUserData;` |
|    19 |  7836 | `	if( ph7_value_is_string(pKey) ){` |
|     - |  7837 | `		int nKey;` |
|     5 |  7838 | `		const char *zKey = ph7_value_to_string(pKey,&nKey);` |
|     5 |  7839 | `		PH7_NativeSetProp(pFa->pThis->pVm,pFa->pThis,zKey,(sxu32)nKey,pVal);` |
|     5 |  7840 | `		return PH7_OK;` |
|     - |  7841 | `	}` |
|    15 |  7842 | `	ph7_array_add_elem(pFa->pElems,0,pVal);` |
|    15 |  7843 | `	pFa->nElem++;` |
|    15 |  7844 | `	return PH7_OK;` |
|    10 |  7845 | `}` |
|     6 |  7846 | `static int vm_builtin_SplFixedArray_unserializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  7847 | `{` |
|     7 |  7848 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 |  7849 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  7850 | `	fa_unser_ctx sFa;` |
|     - |  7851 | `	ph7_value sElems,*pSlot;` |
|     7 |  7852 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 \|\| pThis == 0 ){` |
|   ! 0 |  7853 | `		return PH7_OK;` |
|     - |  7854 | `	}` |
|     7 |  7855 | `	PH7_MemObjInit(pVm,&sElems);` |
|     7 |  7856 | `	if( PH7_MemObjToHashmap(&sElems) != SXRET_OK ){` |
|   ! 0 |  7857 | `		PH7_MemObjRelease(&sElems);` |
|   ! 0 |  7858 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  7859 | `	}` |
|     7 |  7860 | `	sFa.pThis = pThis;` |
|     7 |  7861 | `	sFa.pElems = &sElems;` |
|     7 |  7862 | `	sFa.nElem = 0;` |
|     7 |  7863 | `	ph7_array_walk(apArg[0],FaUnserWalk,&sFa);` |
|     7 |  7864 | `	pSlot = PH7_NativeAttr(pThis,FA_A);` |
|     7 |  7865 | `	if( pSlot ){` |
|     7 |  7866 | `		PH7_MemObjRelease(pSlot);` |
|     7 |  7867 | `		PH7_MemObjStore(&sElems,pSlot);` |
|     3 |  7868 | `	}` |
|     7 |  7869 | `	PH7_MemObjRelease(&sElems);` |
|     7 |  7870 | `	PH7_NativeSetAttrInt(pVm,pThis,FA_N,sFa.nElem);` |
|     7 |  7871 | `	return PH7_OK;` |
|     4 |  7872 | `}` |
|     - |  7873 | `/*` |
|     - |  7874 | ` * php's get_properties: the ELEMENTS, keyed by index, on every surface —` |
|     - |  7875 | ` * var_dump, print_r, the (array) cast and (through __serialize) serialize(). This` |
|     - |  7876 | ` * is the one native class so far whose presentation is the same for the debug and` |
|     - |  7877 | ` * the cast form.` |
|     - |  7878 | ` */` |
|     2 |  7879 | `static sxi32 FaPresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|     1 |  7880 | `{` |
|     3 |  7881 | `	sxi64 n = FaSize(pThis), i;` |
|     1 |  7882 | `	SXUNUSED(bDebug);` |
|     9 |  7883 | `	for( i = 0 ; i < n ; ++i ){` |
|     7 |  7884 | `		ph7_value sKey,*pVal = FaAt(&(*pVm),pThis,i);` |
|     7 |  7885 | `		if( pVal == 0 ){` |
|   ! 0 |  7886 | `			continue;` |
|     - |  7887 | `		}` |
|     7 |  7888 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,i);` |
|     7 |  7889 | `		ph7_array_add_elem(pOut,&sKey,pVal);` |
|     7 |  7890 | `		PH7_MemObjRelease(&sKey);` |
|     4 |  7891 | `	}` |
|     3 |  7892 | `	return PH7_OK;` |
|     1 |  7893 | `}` |
|     - |  7894 | `/*` |
|     - |  7895 | ` * The declaration. Method ORDER and the interface list are spl_fixedarray.stub's;` |
|     - |  7896 | ` * note that __construct, __serialize, __unserialize, getIterator and jsonSerialize` |
|     - |  7897 | ` * are the FIVE methods php does NOT mark tentative here.` |
|     - |  7898 | ` */` |
|  5740 |  7899 | `static sxi32 VmInstallSplFixedArray(ph7_vm *pVm)` |
|     5 |  7900 | `{` |
|     - |  7901 | `	static const PH7_NativePropDef aFaProp[] = {` |
|     - |  7902 | `		{ FA_A, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  7903 | `		{ FA_N, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  7904 | `	};` |
|     - |  7905 | `	static const PH7_NativeMethodDef aFaMethod[] = {` |
|     - |  7906 | `		{ "__construct",   PH7_MOD_PUBLIC, "int $size = 0", 0,` |
|     - |  7907 | `		  vm_builtin_SplFixedArray_construct },` |
|     - |  7908 | `		{ "__wakeup",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplFixedArray_wakeup },` |
|     - |  7909 | `		{ "__serialize",   PH7_MOD_PUBLIC, "", "array", vm_builtin_SplFixedArray_serializeMagic },` |
|     - |  7910 | `		{ "__unserialize", PH7_MOD_PUBLIC, "array $data", "void",` |
|     - |  7911 | `		  vm_builtin_SplFixedArray_unserializeMagic },` |
|     - |  7912 | `		{ "count",         PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplFixedArray_count },` |
|     - |  7913 | `		{ "toArray",       PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplFixedArray_toArray },` |
|     - |  7914 | `		{ "fromArray",     PH7_MOD_PUBLIC\|PH7_MOD_STATIC,` |
|     - |  7915 | `		  "array $array, bool $preserveKeys = true", "@SplFixedArray",` |
|     - |  7916 | `		  vm_builtin_SplFixedArray_fromArray },` |
|     - |  7917 | `		{ "getSize",       PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplFixedArray_getSize },` |
|     - |  7918 | `		{ "setSize",       PH7_MOD_PUBLIC, "int $size", "@true", vm_builtin_SplFixedArray_setSize },` |
|     - |  7919 | `		/* php's stub leaves the four offsets UNTYPED and decodes them itself, the` |
|     - |  7920 | `		 * same shape SplDoublyLinkedList has — but a different rule and a different` |
|     - |  7921 | `		 * refusal, so FaOffset() rather than the DLL's PH7_IntArgResolve. */` |
|     - |  7922 | `		{ "offsetExists",  PH7_MOD_PUBLIC, "$index", "@bool",` |
|     - |  7923 | `		  vm_builtin_SplFixedArray_offsetExists },` |
|     - |  7924 | `		{ "offsetGet",     PH7_MOD_PUBLIC, "$index", "@mixed", vm_builtin_SplFixedArray_offsetGet },` |
|     - |  7925 | `		{ "offsetSet",     PH7_MOD_PUBLIC, "$index, mixed $value", "@void",` |
|     - |  7926 | `		  vm_builtin_SplFixedArray_offsetSet },` |
|     - |  7927 | `		{ "offsetUnset",   PH7_MOD_PUBLIC, "$index", "@void",` |
|     - |  7928 | `		  vm_builtin_SplFixedArray_offsetUnset },` |
|     - |  7929 | `		{ "getIterator",   PH7_MOD_PUBLIC, "", "Iterator", vm_builtin_SplFixedArray_getIterator },` |
|     - |  7930 | `		{ "jsonSerialize", PH7_MOD_PUBLIC, "", "array", vm_builtin_SplFixedArray_toArray },` |
|     - |  7931 | `	};` |
|     - |  7932 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - |  7933 | `		{ "SplFixedArray", 0, "IteratorAggregate,ArrayAccess,Countable,JsonSerializable", 0,` |
|     - |  7934 | `		  aFaMethod, SX_ARRAYSIZE(aFaMethod), 0, 0,` |
|     - |  7935 | `		  aFaProp, SX_ARRAYSIZE(aFaProp), 0, &sFaIterVtab, FaPresent },` |
|     - |  7936 | `	};` |
|  5745 |  7937 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 |  7938 | `}` |
|     - |  7939 | `/*` |
|     - |  7940 | ` * ---------------------------------------------------------------------------` |
|     - |  7941 | ` * SplObjectStorage, SplObserver and SplSubject.` |
|     - |  7942 | ` *` |
|     - |  7943 | `` * php's `spl_SplObjectStorage` is a hashtable of {obj, inf} pairs keyed by the`` |
|     - |  7944 | `` * object HANDLE, plus TWO cursors that are not the same thing: `pos` walks the`` |
|     - |  7945 | `` * table and `index` is the integer `key()` reports. Every method that changes the`` |
|     - |  7946 | ` * membership resets one or both, and the chunk -- which kept a single integer` |
|     - |  7947 | `` * offset -- had none of that: `detach()` mid-walk left the walk where it was`` |
|     - |  7948 | `` * (php restarts it), and `addAll()` left `key()` counting from wherever it stood.`` |
|     - |  7949 | ` *` |
|     - |  7950 | `` * What the chunk did not have AT ALL, which is most of the class: `seek()` and the`` |
|     - |  7951 | `` * `SeekableIterator` interface it comes from, `Serializable` with its`` |
|     - |  7952 | `` * `serialize()`/`unserialize()` pair, the `__serialize()`/`__unserialize()` pair`` |
|     - |  7953 | `` * php actually uses, and `__debugInfo()`. Six methods and two interfaces missing`` |
|     - |  7954 | ` * from a 25-method class -- rule 53, and the reason a method-by-method reading is` |
|     - |  7955 | ` * not a conversion.` |
|     - |  7956 | ` *` |
|     - |  7957 | `` * Two more the model hides. **`current()` on an invalid iterator RAISES**`` |
|     - |  7958 | `` * (`Called current() on invalid iterator`) where the chunk answered null, and`` |
|     - |  7959 | `` * **an overridden `getHash()` is what keys the table** -- php looks the method up`` |
|     - |  7960 | `` * once per instance (`fptr_get_hash`) and every attach/detach/contains goes`` |
|     - |  7961 | ` * through it, so a subclass that hashes two distinct objects the same stores ONE` |
|     - |  7962 | `` * entry. The chunk called `spl_object_id()` directly and ignored its own`` |
|     - |  7963 | `` * `getHash()`, so overriding it did nothing.`` |
|     - |  7964 | ` *` |
|     - |  7965 | ` * php DEPRECATES attach/detach/contains since 8.5 and PHL says nothing, which is` |
|     - |  7966 | ` * the same non-deprecated-compatibility policy the chunk carried (the notice is` |
|     - |  7967 | ` * the only difference and no valid php depends on it).` |
|     - |  7968 | ` */` |
|     - |  7969 | `#define SOS_S "__s"   /* php's storage: key -> ['obj' => object, 'inf' => info] */` |
|     - |  7970 | `#define SOS_I "__i"   /* php's index: what key() reports, NOT a position */` |
|     - |  7971 |  |
|     - |  7972 | `/* The storage slot, separated for writing (every caller may mutate it). */` |
|   732 |  7973 | `static ph7_value * SosSlot(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  7974 | `{` |
|   733 |  7975 | `	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,SOS_S) : 0;` |
|   733 |  7976 | `	if( pSlot == 0 ){` |
|   ! 0 |  7977 | `		return 0;` |
|     - |  7978 | `	}` |
|   733 |  7979 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   123 |  7980 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|   ! 0 |  7981 | `			return 0;` |
|     - |  7982 | `		}` |
|    61 |  7983 | `	}` |
|   733 |  7984 | `	if( PH7_HashmapCowSeparate(pVm,pSlot) == 0 ){` |
|   ! 0 |  7985 | `		return 0;` |
|     - |  7986 | `	}` |
|   733 |  7987 | `	return pSlot;` |
|   367 |  7988 | `}` |
|   732 |  7989 | `static ph7_hashmap * SosMap(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  7990 | `{` |
|   733 |  7991 | `	ph7_value *pSlot = SosSlot(pVm,pThis);` |
|   733 |  7992 | `	return pSlot ? (ph7_hashmap *)pSlot->x.pOther : 0;` |
|     1 |  7993 | `}` |
|     - |  7994 | `/* One half of a stored pair: php's element->obj / element->inf. */` |
|   564 |  7995 | `static ph7_value * SosPart(ph7_value *pPair,const char *zKey)` |
|     1 |  7996 | `{` |
|   565 |  7997 | `	ph7_hashmap_node *pNode = 0;` |
|   565 |  7998 | `	if( pPair == 0 \|\| (pPair->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     5 |  7999 | `		return 0;` |
|     - |  8000 | `	}` |
|   840 |  8001 | `	if( HashmapLookupBlobKey((ph7_hashmap *)pPair->x.pOther,zKey,` |
|   841 |  8002 | `		(sxu32)SyStrlen(zKey),&pNode) != SXRET_OK ){` |
|   ! 0 |  8003 | `		return 0;` |
|     - |  8004 | `	}` |
|   561 |  8005 | `	return HashmapExtractNodeValue(pNode);` |
|   283 |  8006 | `}` |
|     - |  8007 | ``/* Write one half of a pair; a NULL value is php's `ZVAL_NULL(&element->inf)`. */`` |
|   382 |  8008 | `static void SosSetPart(ph7_vm *pVm,ph7_value *pPair,const char *zKey,ph7_value *pVal)` |
|     1 |  8009 | `{` |
|     - |  8010 | `	ph7_value sKey,sNull;` |
|   383 |  8011 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|   383 |  8012 | `	PH7_MemObjStringAppend(&sKey,zKey,(sxu32)SyStrlen(zKey));` |
|   383 |  8013 | `	if( pVal ){` |
|   383 |  8014 | `		ph7_array_add_elem(pPair,&sKey,pVal);` |
|   192 |  8015 | `	}else{` |
|   ! 0 |  8016 | `		PH7_MemObjInit(pVm,&sNull);` |
|   ! 0 |  8017 | `		ph7_array_add_elem(pPair,&sKey,&sNull);` |
|   ! 0 |  8018 | `		PH7_MemObjRelease(&sNull);` |
|     - |  8019 | `	}` |
|   383 |  8020 | `	PH7_MemObjRelease(&sKey);` |
|   383 |  8021 | `}` |
|     - |  8022 | ``/* The pair a node holds, separated: php mutates `element->inf` in place, and here`` |
|     - |  8023 | ` * that is a NESTED array whose COW copy has to be broken first -- a pair handed` |
|     - |  8024 | ` * out by __serialize()/__debugInfo() would otherwise change with it. */` |
|    10 |  8025 | `static ph7_value * SosPairForWrite(ph7_vm *pVm,ph7_hashmap_node *pNode)` |
|     1 |  8026 | `{` |
|    11 |  8027 | `	ph7_value *pPair = HashmapExtractNodeValue(pNode);` |
|    11 |  8028 | `	if( pPair == 0 \|\| (pPair->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 |  8029 | `		return 0;` |
|     - |  8030 | `	}` |
|    11 |  8031 | `	return PH7_HashmapCowSeparate(pVm,pPair) ? pPair : 0;` |
|     6 |  8032 | `}` |
|     - |  8033 | `/*` |
|     - |  8034 | `` * php's `fptr_get_hash`: the class caches the method ONLY when a subclass declares`` |
|     - |  8035 | ` * its own, and every keyed operation then runs it. The one native body is this` |
|     - |  8036 | ` * class's own, so a non-native getHash() IS the override.` |
|     - |  8037 | ` */` |
|   256 |  8038 | `static ph7_class_method * SosUserHash(ph7_class_instance *pThis)` |
|     1 |  8039 | `{` |
|     - |  8040 | `	ph7_class_method *pMethod;` |
|   257 |  8041 | `	if( pThis == 0 ){` |
|   ! 0 |  8042 | `		return 0;` |
|     - |  8043 | `	}` |
|   257 |  8044 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"getHash",sizeof("getHash")-1);` |
|   257 |  8045 | `	if( pMethod == 0 \|\| (pMethod->sFunc.iFlags & VM_FUNC_NATIVE) ){` |
|   251 |  8046 | `		return 0;` |
|     - |  8047 | `	}` |
|     7 |  8048 | `	return pMethod;` |
|   129 |  8049 | `}` |
|     - |  8050 | `/*` |
|     - |  8051 | ` * php's spl_object_storage_get_hash: the object HANDLE, or the STRING an` |
|     - |  8052 | ` * overridden getHash() answers. php checks the returned type itself (its own` |
|     - |  8053 | ` * return declaration would coerce first, so this only fires for an untyped` |
|     - |  8054 | ` * override) and names the RUNTIME class in the refusal.` |
|     - |  8055 | ` */` |
|   256 |  8056 | `static sxi32 SosKey(ph7_context *pCtx,ph7_class_instance *pThis,ph7_value *pObj,ph7_value *pKey)` |
|     1 |  8057 | `{` |
|   257 |  8058 | `	ph7_vm *pVm = pCtx->pVm;` |
|   257 |  8059 | `	ph7_class_method *pHash = SosUserHash(pThis);` |
|     - |  8060 | `	ph7_value sRes,*apArg[1];` |
|     - |  8061 | `	sxi32 rc;` |
|   257 |  8062 | `	if( pHash == 0 ){` |
|   251 |  8063 | `		ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|   251 |  8064 | `		PH7_MemObjRelease(pKey);` |
|   251 |  8065 | `		PH7_MemObjInitFromInt(pVm,pKey,(sxi64)pInst->nObjId);` |
|   251 |  8066 | `		return PH7_OK;` |
|     - |  8067 | `	}` |
|     7 |  8068 | `	PH7_MemObjInit(pVm,&sRes);` |
|     7 |  8069 | `	apArg[0] = pObj;` |
|     7 |  8070 | `	rc = PH7_VmCallClassMethod(pVm,pThis,pHash,&sRes,1,apArg);` |
|     7 |  8071 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  8072 | `		PH7_MemObjRelease(&sRes);` |
|   ! 0 |  8073 | `		return rc;` |
|     - |  8074 | `	}` |
|     7 |  8075 | `	if( (sRes.iFlags & MEMOBJ_STRING) == 0 ){` |
|     - |  8076 | `		char zGiven[64];` |
|   ! 0 |  8077 | `		SyString *pName = &pThis->pClass->sName;` |
|   ! 0 |  8078 | `		PH7_MemObjRelease(&sRes);` |
|   ! 0 |  8079 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  8080 | `			"%z::getHash(): Return value must be of type string, %s returned",` |
|   ! 0 |  8081 | `			pName,VmValueGivenName(&sRes,zGiven,sizeof(zGiven)));` |
|     - |  8082 | `	}` |
|     7 |  8083 | `	PH7_MemObjRelease(pKey);` |
|     7 |  8084 | `	PH7_MemObjInit(pVm,pKey);` |
|     7 |  8085 | `	PH7_MemObjStore(&sRes,pKey);` |
|     7 |  8086 | `	PH7_MemObjRelease(&sRes);` |
|     7 |  8087 | `	return PH7_OK;` |
|   129 |  8088 | `}` |
|     - |  8089 | `/*` |
|     - |  8090 | ` * php's Z_PARAM_OBJ for the four ArrayAccess offsets: their stub leaves $object` |
|     - |  8091 | `` * UNTYPED (a `@param object` docblock, which Reflection does not print) while the`` |
|     - |  8092 | ` * ZPP is an object, so the declared type says nothing and each body words the` |
|     - |  8093 | ` * refusal here -- SplDoublyLinkedList's $index has the same shape one type over.` |
|     - |  8094 | ` * The name is always this class's, even from a subclass (php's).` |
|     - |  8095 | ` */` |
|   162 |  8096 | `static sxi32 SosObjectArg(ph7_context *pCtx,const char *zMethod,int nArg,ph7_value **apArg,` |
|     - |  8097 | `	ph7_value **ppObj)` |
|     1 |  8098 | `{` |
|     - |  8099 | `	char zGiven[64];` |
|   163 |  8100 | `	*ppObj = 0;` |
|   163 |  8101 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) && apArg[0]->x.pOther ){` |
|   155 |  8102 | `		*ppObj = apArg[0];` |
|   155 |  8103 | `		return PH7_OK;` |
|     - |  8104 | `	}` |
|    17 |  8105 | `	return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  8106 | `		"SplObjectStorage::%s(): Argument #1 ($object) must be of type object, %s given",` |
|     8 |  8107 | `		zMethod,nArg < 1 ? "none" : VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|    82 |  8108 | `}` |
|     - |  8109 | `/* The other storage a set operation takes; php's ZPP already screened the class. */` |
|    12 |  8110 | `static ph7_class_instance * SosOther(int nArg,ph7_value **apArg)` |
|     1 |  8111 | `{` |
|    13 |  8112 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 |  8113 | `		return 0;` |
|     - |  8114 | `	}` |
|    13 |  8115 | `	return (ph7_class_instance *)apArg[0]->x.pOther;` |
|     7 |  8116 | `}` |
|     - |  8117 | `/*` |
|     - |  8118 | ` * php's spl_object_storage_attach. The two values are COPIED first: computing the` |
|     - |  8119 | ` * key can run an overridden getHash(), and any call into user code moves every` |
|     - |  8120 | ` * ph7_value the caller is holding (rule 47).` |
|     - |  8121 | ` */` |
|   194 |  8122 | `static sxi32 SosAttach(ph7_context *pCtx,ph7_class_instance *pThis,ph7_value *pObj,ph7_value *pInf)` |
|     1 |  8123 | `{` |
|   195 |  8124 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  8125 | `	ph7_hashmap *pMap;` |
|   195 |  8126 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  8127 | `	ph7_value sObj,sInf,sKey,sPair;` |
|     - |  8128 | `	sxi32 rc;` |
|   195 |  8129 | `	PH7_MemObjInit(pVm,&sObj);` |
|   195 |  8130 | `	PH7_MemObjInit(pVm,&sInf);` |
|   195 |  8131 | `	PH7_MemObjInit(pVm,&sKey);` |
|   195 |  8132 | `	PH7_MemObjStore(pObj,&sObj);` |
|   195 |  8133 | `	if( pInf ){` |
|   195 |  8134 | `		PH7_MemObjStore(pInf,&sInf);` |
|    97 |  8135 | `	}` |
|   195 |  8136 | `	rc = SosKey(pCtx,pThis,&sObj,&sKey);` |
|   195 |  8137 | `	if( rc != PH7_OK ){` |
|   ! 0 |  8138 | `		goto done;` |
|     - |  8139 | `	}` |
|   195 |  8140 | `	pMap = SosMap(pVm,pThis);` |
|   195 |  8141 | `	if( pMap == 0 ){` |
|   ! 0 |  8142 | `		goto done;` |
|     - |  8143 | `	}` |
|   195 |  8144 | `	if( PH7_HashmapLookup(pMap,&sKey,&pNode) == SXRET_OK ){` |
|     9 |  8145 | `		ph7_value *pPair = SosPairForWrite(pVm,pNode);` |
|     9 |  8146 | `		if( pPair ){` |
|     9 |  8147 | `			SosSetPart(pVm,pPair,"inf",&sInf);` |
|     4 |  8148 | `		}` |
|     9 |  8149 | `		goto done;` |
|     - |  8150 | `	}` |
|   187 |  8151 | `	PH7_MemObjInit(pVm,&sPair);` |
|   187 |  8152 | `	if( PH7_MemObjToHashmap(&sPair) != SXRET_OK ){` |
|   ! 0 |  8153 | `		PH7_MemObjRelease(&sPair);` |
|   ! 0 |  8154 | `		rc = PH7_ContextMemoryError(pCtx);` |
|   ! 0 |  8155 | `		goto done;` |
|     - |  8156 | `	}` |
|   187 |  8157 | `	SosSetPart(pVm,&sPair,"obj",&sObj);` |
|   187 |  8158 | `	SosSetPart(pVm,&sPair,"inf",&sInf);` |
|     - |  8159 | `	/* php's position is an INTEGER index into the bucket array, so a cursor that` |
|     - |  8160 | `	 * ran off the end is revived by the insert and the walk resumes on the new` |
|     - |  8161 | `	 * element -- what addAll() mid-iteration does there. */` |
|   187 |  8162 | `	SplStoreInsert(pMap,&sKey,&sPair);` |
|   187 |  8163 | `	PH7_MemObjRelease(&sPair);` |
|    97 |  8164 | `done:` |
|   195 |  8165 | `	PH7_MemObjRelease(&sObj);` |
|   195 |  8166 | `	PH7_MemObjRelease(&sInf);` |
|   195 |  8167 | `	PH7_MemObjRelease(&sKey);` |
|   195 |  8168 | `	return rc;` |
|     1 |  8169 | `}` |
|     - |  8170 | `/* php's spl_object_storage_detach: drop the entry, saying whether there was one. */` |
|    20 |  8171 | `static sxi32 SosDetach(ph7_context *pCtx,ph7_class_instance *pThis,ph7_value *pObj,int *pbGone)` |
|     1 |  8172 | `{` |
|    21 |  8173 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  8174 | `	ph7_hashmap *pMap;` |
|    21 |  8175 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  8176 | `	ph7_value sObj,sKey;` |
|     - |  8177 | `	sxi32 rc;` |
|    21 |  8178 | `	if( pbGone ){` |
|   ! 0 |  8179 | `		*pbGone = 0;` |
|   ! 0 |  8180 | `	}` |
|    21 |  8181 | `	PH7_MemObjInit(pVm,&sObj);` |
|    21 |  8182 | `	PH7_MemObjInit(pVm,&sKey);` |
|    21 |  8183 | `	PH7_MemObjStore(pObj,&sObj);` |
|    21 |  8184 | `	rc = SosKey(pCtx,pThis,&sObj,&sKey);` |
|    21 |  8185 | `	if( rc == PH7_OK ){` |
|    21 |  8186 | `		pMap = SosMap(pVm,pThis);` |
|    21 |  8187 | `		if( pMap && PH7_HashmapLookup(pMap,&sKey,&pNode) == SXRET_OK ){` |
|    19 |  8188 | `			PH7_HashmapUnlinkNode(pNode,TRUE);` |
|    19 |  8189 | `			if( pbGone ){` |
|   ! 0 |  8190 | `				*pbGone = 1;` |
|   ! 0 |  8191 | `			}` |
|     9 |  8192 | `		}` |
|    10 |  8193 | `	}` |
|    21 |  8194 | `	PH7_MemObjRelease(&sObj);` |
|    21 |  8195 | `	PH7_MemObjRelease(&sKey);` |
|    21 |  8196 | `	return rc;` |
|     1 |  8197 | `}` |
|     - |  8198 | `/* php's spl_object_storage_contains: an entry EXISTS, whatever its info holds. */` |
|    14 |  8199 | `static sxi32 SosContains(ph7_context *pCtx,ph7_class_instance *pThis,ph7_value *pObj,int *pbFound)` |
|     1 |  8200 | `{` |
|    15 |  8201 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  8202 | `	ph7_hashmap *pMap;` |
|    15 |  8203 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  8204 | `	ph7_value sObj,sKey;` |
|     - |  8205 | `	sxi32 rc;` |
|    15 |  8206 | `	*pbFound = 0;` |
|    15 |  8207 | `	PH7_MemObjInit(pVm,&sObj);` |
|    15 |  8208 | `	PH7_MemObjInit(pVm,&sKey);` |
|    15 |  8209 | `	PH7_MemObjStore(pObj,&sObj);` |
|    15 |  8210 | `	rc = SosKey(pCtx,pThis,&sObj,&sKey);` |
|    15 |  8211 | `	if( rc == PH7_OK ){` |
|    15 |  8212 | `		pMap = SosMap(pVm,pThis);` |
|    15 |  8213 | `		*pbFound = pMap && PH7_HashmapLookup(pMap,&sKey,&pNode) == SXRET_OK;` |
|     7 |  8214 | `	}` |
|    15 |  8215 | `	PH7_MemObjRelease(&sObj);` |
|    15 |  8216 | `	PH7_MemObjRelease(&sKey);` |
|    15 |  8217 | `	return rc;` |
|     1 |  8218 | `}` |
|     - |  8219 | ``/* php's `zend_hash_internal_pointer_reset_ex(&storage, &pos); index = 0`. */`` |
|    36 |  8220 | `static void SosRewind(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  8221 | `{` |
|    37 |  8222 | `	ph7_hashmap *pMap = SosMap(pVm,pThis);` |
|    37 |  8223 | `	if( pMap ){` |
|    37 |  8224 | `		pMap->pCur = pMap->pFirst;` |
|    18 |  8225 | `	}` |
|    37 |  8226 | `	PH7_NativeSetAttrInt(pVm,pThis,SOS_I,0);` |
|    37 |  8227 | `}` |
|     - |  8228 | `/* The pair the cursor is on, or 0 past the end. */` |
|    84 |  8229 | `static ph7_value * SosCurrentPair(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  8230 | `{` |
|    85 |  8231 | `	ph7_hashmap *pMap = SosMap(pVm,pThis);` |
|    85 |  8232 | `	if( pMap == 0 \|\| pMap->pCur == 0 ){` |
|     5 |  8233 | `		return 0;` |
|     - |  8234 | `	}` |
|    81 |  8235 | `	return HashmapExtractNodeValue(pMap->pCur);` |
|    43 |  8236 | `}` |
|     - |  8237 | `/*` |
|     - |  8238 | ` * A SNAPSHOT of one storage's pairs as a plain list. Every set operation walks one` |
|     - |  8239 | ` * storage while writing to another -- and either walk can run an overridden` |
|     - |  8240 | ` * getHash(), which moves things (rule 47) and can even mutate the map being walked.` |
|     - |  8241 | ` * php's own SPL_SAFE_HASH_FOREACH_PTR is the same precaution one layer down.` |
|     - |  8242 | ` */` |
|    12 |  8243 | `static sxi32 SosSnapshot(ph7_vm *pVm,ph7_class_instance *pFrom,ph7_value *pOut)` |
|     1 |  8244 | `{` |
|    13 |  8245 | `	ph7_hashmap *pMap = SosMap(pVm,pFrom);` |
|     - |  8246 | `	ph7_hashmap_node *pNode;` |
|     - |  8247 | `	sxu32 n;` |
|    13 |  8248 | `	if( PH7_MemObjToHashmap(pOut) != SXRET_OK ){` |
|   ! 0 |  8249 | `		return SXERR_MEM;` |
|     - |  8250 | `	}` |
|    13 |  8251 | `	if( pMap == 0 ){` |
|   ! 0 |  8252 | `		return SXRET_OK;` |
|     - |  8253 | `	}` |
|    13 |  8254 | `	pNode = pMap->pFirst;` |
|    37 |  8255 | `	for( n = 0 ; n < pMap->nEntry && pNode ; ++n ){` |
|    25 |  8256 | `		ph7_value *pPair = HashmapExtractNodeValue(pNode);` |
|    25 |  8257 | `		if( pPair ){` |
|    25 |  8258 | `			ph7_array_add_elem(pOut,0,pPair);` |
|    12 |  8259 | `		}` |
|    25 |  8260 | `		pNode = pNode->pPrev;   /* insertion order: pFirst, then the pPrev chain */` |
|    13 |  8261 | `	}` |
|    13 |  8262 | `	return SXRET_OK;` |
|     7 |  8263 | `}` |
|     - |  8264 | `/* One pair of a snapshot, re-resolved by index because a user call may have moved` |
|     - |  8265 | ` * every value in the pool since the last one. */` |
|    28 |  8266 | `static ph7_value * SosSnapAt(ph7_value *pSnap,sxi64 i)` |
|     1 |  8267 | `{` |
|    29 |  8268 | `	ph7_hashmap_node *pNode = 0;` |
|    28 |  8269 | `	if( (pSnap->iFlags & MEMOBJ_HASHMAP) == 0` |
|    29 |  8270 | `	 \|\| HashmapLookupIntKey((ph7_hashmap *)pSnap->x.pOther,i,&pNode) != SXRET_OK ){` |
|   ! 0 |  8271 | `		return 0;` |
|     - |  8272 | `	}` |
|    29 |  8273 | `	return HashmapExtractNodeValue(pNode);` |
|    15 |  8274 | `}` |
|   ! 0 |  8275 | `static int vm_builtin_SplObjectStorage_attach(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 |  8276 | `{` |
|     - |  8277 | `	ph7_value *pObj;` |
|   ! 0 |  8278 | `	sxi32 rc = SosObjectArg(pCtx,"attach",nArg,apArg,&pObj);` |
|   ! 0 |  8279 | `	if( rc != PH7_OK ){` |
|   ! 0 |  8280 | `		return rc;` |
|     - |  8281 | `	}` |
|   ! 0 |  8282 | `	return SosAttach(pCtx,PH7_ContextThis(pCtx),pObj,nArg > 1 ? apArg[1] : 0);` |
|   ! 0 |  8283 | `}` |
|     - |  8284 | `/* php's offsetSet is an @implementation-alias of attach, and its refusal says so. */` |
|   116 |  8285 | `static int vm_builtin_SplObjectStorage_offsetSet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8286 | `{` |
|     - |  8287 | `	ph7_value *pObj;` |
|   117 |  8288 | `	sxi32 rc = SosObjectArg(pCtx,"offsetSet",nArg,apArg,&pObj);` |
|   117 |  8289 | `	if( rc != PH7_OK ){` |
|     3 |  8290 | `		return rc;` |
|     - |  8291 | `	}` |
|   115 |  8292 | `	return SosAttach(pCtx,PH7_ContextThis(pCtx),pObj,nArg > 1 ? apArg[1] : 0);` |
|    59 |  8293 | `}` |
|     - |  8294 | `/* detach() RESTARTS the walk: php resets both the position and the index, whether` |
|     - |  8295 | ` * or not anything was removed. */` |
|     4 |  8296 | `static sxi32 SosDetachMethod(ph7_context *pCtx,const char *zMethod,int nArg,ph7_value **apArg)` |
|     1 |  8297 | `{` |
|     5 |  8298 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  8299 | `	ph7_value *pObj;` |
|     5 |  8300 | `	sxi32 rc = SosObjectArg(pCtx,zMethod,nArg,apArg,&pObj);` |
|     5 |  8301 | `	if( rc != PH7_OK ){` |
|   ! 0 |  8302 | `		return rc;` |
|     - |  8303 | `	}` |
|     5 |  8304 | `	rc = SosDetach(pCtx,pThis,pObj,0);` |
|     5 |  8305 | `	if( rc != PH7_OK ){` |
|   ! 0 |  8306 | `		return rc;` |
|     - |  8307 | `	}` |
|     5 |  8308 | `	SosRewind(pCtx->pVm,pThis);` |
|     5 |  8309 | `	return PH7_OK;` |
|     3 |  8310 | `}` |
|   ! 0 |  8311 | `static int vm_builtin_SplObjectStorage_detach(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 |  8312 | `{` |
|   ! 0 |  8313 | `	return SosDetachMethod(pCtx,"detach",nArg,apArg);` |
|   ! 0 |  8314 | `}` |
|     4 |  8315 | `static int vm_builtin_SplObjectStorage_offsetUnset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8316 | `{` |
|     5 |  8317 | `	return SosDetachMethod(pCtx,"offsetUnset",nArg,apArg);` |
|     1 |  8318 | `}` |
|    10 |  8319 | `static sxi32 SosContainsMethod(ph7_context *pCtx,const char *zMethod,int nArg,ph7_value **apArg)` |
|     1 |  8320 | `{` |
|     - |  8321 | `	ph7_value *pObj;` |
|    11 |  8322 | `	int bFound = 0;` |
|    11 |  8323 | `	sxi32 rc = SosObjectArg(pCtx,zMethod,nArg,apArg,&pObj);` |
|    11 |  8324 | `	if( rc != PH7_OK ){` |
|     3 |  8325 | `		return rc;` |
|     - |  8326 | `	}` |
|     9 |  8327 | `	rc = SosContains(pCtx,PH7_ContextThis(pCtx),pObj,&bFound);` |
|     9 |  8328 | `	if( rc != PH7_OK ){` |
|   ! 0 |  8329 | `		return rc;` |
|     - |  8330 | `	}` |
|     9 |  8331 | `	ph7_result_bool(pCtx,bFound);` |
|     9 |  8332 | `	return PH7_OK;` |
|     6 |  8333 | `}` |
|   ! 0 |  8334 | `static int vm_builtin_SplObjectStorage_contains(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 |  8335 | `{` |
|   ! 0 |  8336 | `	return SosContainsMethod(pCtx,"contains",nArg,apArg);` |
|   ! 0 |  8337 | `}` |
|    10 |  8338 | `static int vm_builtin_SplObjectStorage_offsetExists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8339 | `{` |
|    11 |  8340 | `	return SosContainsMethod(pCtx,"offsetExists",nArg,apArg);` |
|     1 |  8341 | `}` |
|     - |  8342 | ``/* php's offsetGet: the info, or `Object not found` -- NOT null, and not false. */`` |
|    28 |  8343 | `static int vm_builtin_SplObjectStorage_offsetGet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8344 | `{` |
|    29 |  8345 | `	ph7_vm *pVm = pCtx->pVm;` |
|    29 |  8346 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  8347 | `	ph7_hashmap *pMap;` |
|    29 |  8348 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  8349 | `	ph7_value sObj,sKey,*pObj,*pInf;` |
|    29 |  8350 | `	sxi32 rc = SosObjectArg(pCtx,"offsetGet",nArg,apArg,&pObj);` |
|    29 |  8351 | `	if( rc != PH7_OK ){` |
|     5 |  8352 | `		return rc;` |
|     - |  8353 | `	}` |
|    25 |  8354 | `	PH7_MemObjInit(pVm,&sObj);` |
|    25 |  8355 | `	PH7_MemObjInit(pVm,&sKey);` |
|    25 |  8356 | `	PH7_MemObjStore(pObj,&sObj);` |
|    25 |  8357 | `	rc = SosKey(pCtx,pThis,&sObj,&sKey);` |
|    25 |  8358 | `	if( rc == PH7_OK ){` |
|    25 |  8359 | `		pMap = SosMap(pVm,pThis);` |
|    25 |  8360 | `		if( pMap == 0 \|\| PH7_HashmapLookup(pMap,&sKey,&pNode) != SXRET_OK ){` |
|     5 |  8361 | `			rc = PH7_VmThrowException(pCtx,"UnexpectedValueException","Object not found");` |
|     3 |  8362 | `		}else{` |
|    21 |  8363 | `			pInf = SosPart(HashmapExtractNodeValue(pNode),"inf");` |
|    21 |  8364 | `			if( pInf ){` |
|    21 |  8365 | `				ph7_result_value(pCtx,pInf);` |
|    11 |  8366 | `			}else{` |
|   ! 0 |  8367 | `				ph7_result_null(pCtx);` |
|     - |  8368 | `			}` |
|     - |  8369 | `		}` |
|    12 |  8370 | `	}` |
|    25 |  8371 | `	PH7_MemObjRelease(&sObj);` |
|    25 |  8372 | `	PH7_MemObjRelease(&sKey);` |
|    25 |  8373 | `	return rc;` |
|    15 |  8374 | `}` |
|     - |  8375 | `/*` |
|     - |  8376 | ` * php's addAll: attach every pair of the other storage, then reset the INDEX only` |
|     - |  8377 | ` * -- the position is deliberately left where it stood, which is why an insert can` |
|     - |  8378 | ` * revive a walk that had run out.` |
|     - |  8379 | ` */` |
|     8 |  8380 | `static int vm_builtin_SplObjectStorage_addAll(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8381 | `{` |
|     9 |  8382 | `	ph7_vm *pVm = pCtx->pVm;` |
|     9 |  8383 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     9 |  8384 | `	ph7_class_instance *pOther = SosOther(nArg,apArg);` |
|     - |  8385 | `	ph7_hashmap *pMap;` |
|     - |  8386 | `	ph7_value sSnap;` |
|     - |  8387 | `	sxi64 i,n;` |
|     9 |  8388 | `	sxi32 rc = PH7_OK;` |
|     9 |  8389 | `	if( pOther == 0 \|\| pThis == 0 ){` |
|   ! 0 |  8390 | `		return PH7_OK;` |
|     - |  8391 | `	}` |
|     9 |  8392 | `	PH7_MemObjInit(pVm,&sSnap);` |
|     9 |  8393 | `	if( SosSnapshot(pVm,pOther,&sSnap) != SXRET_OK ){` |
|   ! 0 |  8394 | `		PH7_MemObjRelease(&sSnap);` |
|   ! 0 |  8395 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  8396 | `	}` |
|     9 |  8397 | `	n = (sxi64)((ph7_hashmap *)sSnap.x.pOther)->nEntry;` |
|    21 |  8398 | `	for( i = 0 ; i < n && rc == PH7_OK ; ++i ){` |
|    13 |  8399 | `		ph7_value *pPair = SosSnapAt(&sSnap,i);` |
|    13 |  8400 | `		ph7_value *pObj = SosPart(pPair,"obj");` |
|    13 |  8401 | `		ph7_value *pInf = SosPart(pPair,"inf");` |
|    13 |  8402 | `		if( pObj ){` |
|    13 |  8403 | `			rc = SosAttach(pCtx,pThis,pObj,pInf);` |
|     6 |  8404 | `		}` |
|     7 |  8405 | `	}` |
|     9 |  8406 | `	PH7_MemObjRelease(&sSnap);` |
|     9 |  8407 | `	if( rc != PH7_OK ){` |
|   ! 0 |  8408 | `		return rc;` |
|     - |  8409 | `	}` |
|     9 |  8410 | `	PH7_NativeSetAttrInt(pVm,pThis,SOS_I,0);` |
|     9 |  8411 | `	pMap = SosMap(pVm,pThis);` |
|     9 |  8412 | `	ph7_result_int64(pCtx,pMap ? (ph7_int64)pMap->nEntry : 0);` |
|     9 |  8413 | `	return PH7_OK;` |
|     5 |  8414 | `}` |
|     - |  8415 | `/* php's removeAll: detach everything the other storage holds, then RESTART the walk. */` |
|     2 |  8416 | `static int vm_builtin_SplObjectStorage_removeAll(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8417 | `{` |
|     3 |  8418 | `	ph7_vm *pVm = pCtx->pVm;` |
|     3 |  8419 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     3 |  8420 | `	ph7_class_instance *pOther = SosOther(nArg,apArg);` |
|     - |  8421 | `	ph7_hashmap *pMap;` |
|     - |  8422 | `	ph7_value sSnap;` |
|     - |  8423 | `	sxi64 i,n;` |
|     3 |  8424 | `	sxi32 rc = PH7_OK;` |
|     3 |  8425 | `	if( pOther == 0 \|\| pThis == 0 ){` |
|   ! 0 |  8426 | `		return PH7_OK;` |
|     - |  8427 | `	}` |
|     3 |  8428 | `	PH7_MemObjInit(pVm,&sSnap);` |
|     3 |  8429 | `	if( SosSnapshot(pVm,pOther,&sSnap) != SXRET_OK ){` |
|   ! 0 |  8430 | `		PH7_MemObjRelease(&sSnap);` |
|   ! 0 |  8431 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  8432 | `	}` |
|     3 |  8433 | `	n = (sxi64)((ph7_hashmap *)sSnap.x.pOther)->nEntry;` |
|     9 |  8434 | `	for( i = 0 ; i < n && rc == PH7_OK ; ++i ){` |
|     7 |  8435 | `		ph7_value *pObj = SosPart(SosSnapAt(&sSnap,i),"obj");` |
|     7 |  8436 | `		if( pObj ){` |
|     7 |  8437 | `			rc = SosDetach(pCtx,pThis,pObj,0);` |
|     3 |  8438 | `		}` |
|     4 |  8439 | `	}` |
|     3 |  8440 | `	PH7_MemObjRelease(&sSnap);` |
|     3 |  8441 | `	if( rc != PH7_OK ){` |
|   ! 0 |  8442 | `		return rc;` |
|     - |  8443 | `	}` |
|     3 |  8444 | `	SosRewind(pVm,pThis);` |
|     3 |  8445 | `	pMap = SosMap(pVm,pThis);` |
|     3 |  8446 | `	ph7_result_int64(pCtx,pMap ? (ph7_int64)pMap->nEntry : 0);` |
|     3 |  8447 | `	return PH7_OK;` |
|     2 |  8448 | `}` |
|     - |  8449 | `/* php's removeAllExcept: the INTERSECTION, walked over this storage's own pairs. */` |
|     2 |  8450 | `static int vm_builtin_SplObjectStorage_removeAllExcept(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8451 | `{` |
|     3 |  8452 | `	ph7_vm *pVm = pCtx->pVm;` |
|     3 |  8453 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     3 |  8454 | `	ph7_class_instance *pOther = SosOther(nArg,apArg);` |
|     - |  8455 | `	ph7_hashmap *pMap;` |
|     - |  8456 | `	ph7_value sSnap;` |
|     - |  8457 | `	sxi64 i,n;` |
|     3 |  8458 | `	sxi32 rc = PH7_OK;` |
|     3 |  8459 | `	if( pOther == 0 \|\| pThis == 0 ){` |
|   ! 0 |  8460 | `		return PH7_OK;` |
|     - |  8461 | `	}` |
|     3 |  8462 | `	PH7_MemObjInit(pVm,&sSnap);` |
|     3 |  8463 | `	if( SosSnapshot(pVm,pThis,&sSnap) != SXRET_OK ){` |
|   ! 0 |  8464 | `		PH7_MemObjRelease(&sSnap);` |
|   ! 0 |  8465 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  8466 | `	}` |
|     3 |  8467 | `	n = (sxi64)((ph7_hashmap *)sSnap.x.pOther)->nEntry;` |
|     9 |  8468 | `	for( i = 0 ; i < n && rc == PH7_OK ; ++i ){` |
|     7 |  8469 | `		ph7_value *pObj = SosPart(SosSnapAt(&sSnap,i),"obj");` |
|     7 |  8470 | `		int bFound = 0;` |
|     7 |  8471 | `		if( pObj == 0 ){` |
|   ! 0 |  8472 | `			continue;` |
|     - |  8473 | `		}` |
|     7 |  8474 | `		rc = SosContains(pCtx,pOther,pObj,&bFound);` |
|     7 |  8475 | `		if( rc == PH7_OK && !bFound ){` |
|     5 |  8476 | `			pObj = SosPart(SosSnapAt(&sSnap,i),"obj");   /* re-resolved: getHash may have run */` |
|     5 |  8477 | `			if( pObj ){` |
|     5 |  8478 | `				rc = SosDetach(pCtx,pThis,pObj,0);` |
|     2 |  8479 | `			}` |
|     2 |  8480 | `		}` |
|     4 |  8481 | `	}` |
|     3 |  8482 | `	PH7_MemObjRelease(&sSnap);` |
|     3 |  8483 | `	if( rc != PH7_OK ){` |
|   ! 0 |  8484 | `		return rc;` |
|     - |  8485 | `	}` |
|     3 |  8486 | `	SosRewind(pVm,pThis);` |
|     3 |  8487 | `	pMap = SosMap(pVm,pThis);` |
|     3 |  8488 | `	ph7_result_int64(pCtx,pMap ? (ph7_int64)pMap->nEntry : 0);` |
|     3 |  8489 | `	return PH7_OK;` |
|     2 |  8490 | `}` |
|     - |  8491 | `/*` |
|     - |  8492 | ` * php's count(): COUNT_RECURSIVE is accepted and changes nothing -- the storage` |
|     - |  8493 | ` * holds C structs rather than zvals there, so nothing recurses.` |
|     - |  8494 | ` */` |
|    24 |  8495 | `static int vm_builtin_SplObjectStorage_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8496 | `{` |
|    25 |  8497 | `	ph7_hashmap *pMap = SosMap(pCtx->pVm,PH7_ContextThis(pCtx));` |
|    12 |  8498 | `	SXUNUSED(nArg);` |
|    12 |  8499 | `	SXUNUSED(apArg);` |
|    25 |  8500 | `	ph7_result_int64(pCtx,pMap ? (ph7_int64)pMap->nEntry : 0);` |
|    25 |  8501 | `	return PH7_OK;` |
|     1 |  8502 | `}` |
|     4 |  8503 | `static int vm_builtin_SplObjectStorage_getHash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8504 | `{` |
|     - |  8505 | `	ph7_value *pObj;` |
|     5 |  8506 | `	sxi32 rc = SosObjectArg(pCtx,"getHash",nArg,apArg,&pObj);` |
|     5 |  8507 | `	if( rc != PH7_OK ){` |
|   ! 0 |  8508 | `		return rc;` |
|     - |  8509 | `	}` |
|     - |  8510 | `	/* php's getHash() IS php_spl_object_hash(), the same one the function answers. */` |
|     5 |  8511 | `	return vm_builtin_spl_object_hash(pCtx,nArg,apArg);` |
|     3 |  8512 | `}` |
|    28 |  8513 | `static int vm_builtin_SplObjectStorage_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8514 | `{` |
|    14 |  8515 | `	SXUNUSED(nArg);` |
|    14 |  8516 | `	SXUNUSED(apArg);` |
|    29 |  8517 | `	SosRewind(pCtx->pVm,PH7_ContextThis(pCtx));` |
|    29 |  8518 | `	return PH7_OK;` |
|     1 |  8519 | `}` |
|    52 |  8520 | `static int vm_builtin_SplObjectStorage_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8521 | `{` |
|    53 |  8522 | `	ph7_hashmap *pMap = SosMap(pCtx->pVm,PH7_ContextThis(pCtx));` |
|    26 |  8523 | `	SXUNUSED(nArg);` |
|    26 |  8524 | `	SXUNUSED(apArg);` |
|    53 |  8525 | `	ph7_result_bool(pCtx,pMap && pMap->pCur ? 1 : 0);` |
|    53 |  8526 | `	return PH7_OK;` |
|     1 |  8527 | `}` |
|     - |  8528 | `/* php's key() is the INDEX, a counter of its own: next() advances it past the end` |
|     - |  8529 | ` * too, and only rewind()/detach()/addAll() and friends put it back to zero. */` |
|    40 |  8530 | `static int vm_builtin_SplObjectStorage_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8531 | `{` |
|    20 |  8532 | `	SXUNUSED(nArg);` |
|    20 |  8533 | `	SXUNUSED(apArg);` |
|    41 |  8534 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(PH7_ContextThis(pCtx),SOS_I));` |
|    41 |  8535 | `	return PH7_OK;` |
|     1 |  8536 | `}` |
|     - |  8537 | `/* php RAISES here rather than answering null: the chunk's null was a wrong answer` |
|     - |  8538 | `` * every `foreach` hid, because a foreach never asks past valid(). */`` |
|    46 |  8539 | `static int vm_builtin_SplObjectStorage_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8540 | `{` |
|    47 |  8541 | `	ph7_value *pObj = SosPart(SosCurrentPair(pCtx->pVm,PH7_ContextThis(pCtx)),"obj");` |
|    23 |  8542 | `	SXUNUSED(nArg);` |
|    23 |  8543 | `	SXUNUSED(apArg);` |
|    47 |  8544 | `	if( pObj == 0 ){` |
|     3 |  8545 | `		return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     - |  8546 | `			"Called current() on invalid iterator");` |
|     - |  8547 | `	}` |
|    45 |  8548 | `	ph7_result_value(pCtx,pObj);` |
|    45 |  8549 | `	return PH7_OK;` |
|    24 |  8550 | `}` |
|    44 |  8551 | `static int vm_builtin_SplObjectStorage_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8552 | `{` |
|    45 |  8553 | `	ph7_vm *pVm = pCtx->pVm;` |
|    45 |  8554 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    45 |  8555 | `	ph7_hashmap *pMap = SosMap(pVm,pThis);` |
|    22 |  8556 | `	SXUNUSED(nArg);` |
|    22 |  8557 | `	SXUNUSED(apArg);` |
|    45 |  8558 | `	if( pMap && pMap->pCur ){` |
|    45 |  8559 | `		pMap->pCur = pMap->pCur->pPrev;` |
|    22 |  8560 | `	}` |
|    45 |  8561 | `	PH7_NativeSetAttrInt(pVm,pThis,SOS_I,PH7_NativeAttrInt(pThis,SOS_I) + 1);` |
|    45 |  8562 | `	return PH7_OK;` |
|     1 |  8563 | `}` |
|     - |  8564 | `/* php's getInfo(): null past the end, where current() raises. */` |
|    38 |  8565 | `static int vm_builtin_SplObjectStorage_getInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8566 | `{` |
|    39 |  8567 | `	ph7_value *pInf = SosPart(SosCurrentPair(pCtx->pVm,PH7_ContextThis(pCtx)),"inf");` |
|    19 |  8568 | `	SXUNUSED(nArg);` |
|    19 |  8569 | `	SXUNUSED(apArg);` |
|    39 |  8570 | `	if( pInf ){` |
|    37 |  8571 | `		ph7_result_value(pCtx,pInf);` |
|    19 |  8572 | `	}else{` |
|     3 |  8573 | `		ph7_result_null(pCtx);` |
|     - |  8574 | `	}` |
|    39 |  8575 | `	return PH7_OK;` |
|     1 |  8576 | `}` |
|     2 |  8577 | `static int vm_builtin_SplObjectStorage_setInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8578 | `{` |
|     3 |  8579 | `	ph7_vm *pVm = pCtx->pVm;` |
|     3 |  8580 | `	ph7_hashmap *pMap = SosMap(pVm,PH7_ContextThis(pCtx));` |
|     - |  8581 | `	ph7_value *pPair;` |
|     3 |  8582 | `	if( pMap == 0 \|\| pMap->pCur == 0 \|\| nArg < 1 ){` |
|   ! 0 |  8583 | `		return PH7_OK;   /* php returns without touching anything */` |
|     - |  8584 | `	}` |
|     3 |  8585 | `	pPair = SosPairForWrite(pVm,pMap->pCur);` |
|     3 |  8586 | `	if( pPair ){` |
|     3 |  8587 | `		SosSetPart(pVm,pPair,"inf",apArg[0]);` |
|     1 |  8588 | `	}` |
|     3 |  8589 | `	return PH7_OK;` |
|     2 |  8590 | `}` |
|     - |  8591 | `/*` |
|     - |  8592 | ` * php's seek(): a position outside the storage is an OutOfBoundsException, and` |
|     - |  8593 | ` * the index follows the position exactly (php walks its hash cursor either way` |
|     - |  8594 | ` * and counts; the destination is the same).` |
|     - |  8595 | ` */` |
|     4 |  8596 | `static int vm_builtin_SplObjectStorage_seek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8597 | `{` |
|     5 |  8598 | `	ph7_vm *pVm = pCtx->pVm;` |
|     5 |  8599 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     5 |  8600 | `	ph7_hashmap *pMap = SosMap(pVm,pThis);` |
|     5 |  8601 | `	ph7_int64 iPos = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|     - |  8602 | `	ph7_int64 i;` |
|     5 |  8603 | `	if( pMap == 0 ){` |
|   ! 0 |  8604 | `		return PH7_OK;` |
|     - |  8605 | `	}` |
|     5 |  8606 | `	if( iPos < 0 \|\| iPos >= (ph7_int64)pMap->nEntry ){` |
|     4 |  8607 | `		return PH7_VmThrowException(pCtx,"OutOfBoundsException",` |
|     1 |  8608 | `			"Seek position %qd is out of range",iPos);` |
|     - |  8609 | `	}` |
|     3 |  8610 | `	pMap->pCur = pMap->pFirst;` |
|     7 |  8611 | `	for( i = 0 ; i < iPos && pMap->pCur ; ++i ){` |
|     5 |  8612 | `		pMap->pCur = pMap->pCur->pPrev;` |
|     3 |  8613 | `	}` |
|     3 |  8614 | `	PH7_NativeSetAttrInt(pVm,pThis,SOS_I,iPos);` |
|     3 |  8615 | `	return PH7_OK;` |
|     3 |  8616 | `}` |
|     - |  8617 | `/*` |
|     - |  8618 | ` * php's get_debug_info: ONE entry, the storage, under its own MANGLED private key` |
|     - |  8619 | `` * -- `["storage":"SplObjectStorage":private]` on screen. The pairs are re-indexed`` |
|     - |  8620 | ` * from zero and shown as {obj, inf}, which is the shape stored here already.` |
|     - |  8621 | `` * `__debugInfo()` is the same array, reachable by name because php declares it.`` |
|     - |  8622 | ` */` |
|     8 |  8623 | `static sxi32 SosFillDebug(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)` |
|     1 |  8624 | `{` |
|     9 |  8625 | `	ph7_hashmap *pMap = SosMap(pVm,pThis);` |
|     - |  8626 | `	ph7_hashmap_node *pNode;` |
|     - |  8627 | `	ph7_value sKey,sList;` |
|     - |  8628 | `	sxu32 n;` |
|     9 |  8629 | `	PH7_MemObjInit(pVm,&sList);` |
|     9 |  8630 | `	if( PH7_MemObjToHashmap(&sList) != SXRET_OK ){` |
|   ! 0 |  8631 | `		PH7_MemObjRelease(&sList);` |
|   ! 0 |  8632 | `		return SXERR_MEM;` |
|     - |  8633 | `	}` |
|     9 |  8634 | `	if( pMap ){` |
|     9 |  8635 | `		pNode = pMap->pFirst;` |
|    15 |  8636 | `		for( n = 0 ; n < pMap->nEntry && pNode ; ++n ){` |
|     7 |  8637 | `			ph7_value *pPair = HashmapExtractNodeValue(pNode);` |
|     7 |  8638 | `			if( pPair ){` |
|     7 |  8639 | `				ph7_array_add_elem(&sList,0,pPair);` |
|     3 |  8640 | `			}` |
|     7 |  8641 | `			pNode = pNode->pPrev;` |
|     4 |  8642 | `		}` |
|     4 |  8643 | `	}` |
|     9 |  8644 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|     9 |  8645 | `	PH7_MemObjStringAppend(&sKey,"\0SplObjectStorage\0storage",` |
|     - |  8646 | `		sizeof("\0SplObjectStorage\0storage")-1);` |
|     9 |  8647 | `	ph7_array_add_elem(pOut,&sKey,&sList);` |
|     9 |  8648 | `	PH7_MemObjRelease(&sKey);` |
|     9 |  8649 | `	PH7_MemObjRelease(&sList);` |
|     9 |  8650 | `	return PH7_OK;` |
|     5 |  8651 | `}` |
|     8 |  8652 | `static sxi32 SosPresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|     1 |  8653 | `{` |
|     9 |  8654 | `	if( !bDebug ){` |
|     5 |  8655 | `		return PH7_OK;   /* php's (array) cast and var_export show nothing */` |
|     - |  8656 | `	}` |
|     5 |  8657 | `	return SosFillDebug(pVm,pThis,pOut);` |
|     5 |  8658 | `}` |
|     4 |  8659 | `static int vm_builtin_SplObjectStorage_debugInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8660 | `{` |
|     5 |  8661 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  8662 | `	ph7_value sOut;` |
|     2 |  8663 | `	SXUNUSED(nArg);` |
|     2 |  8664 | `	SXUNUSED(apArg);` |
|     5 |  8665 | `	PH7_MemObjInit(pVm,&sOut);` |
|     5 |  8666 | `	if( PH7_MemObjToHashmap(&sOut) != SXRET_OK ){` |
|   ! 0 |  8667 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 |  8668 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  8669 | `	}` |
|     5 |  8670 | `	SosFillDebug(pVm,PH7_ContextThis(pCtx),&sOut);` |
|     5 |  8671 | `	ph7_result_value(pCtx,&sOut);` |
|     5 |  8672 | `	PH7_MemObjRelease(&sOut);` |
|     5 |  8673 | `	return PH7_OK;` |
|     3 |  8674 | `}` |
|     - |  8675 | `/*` |
|     - |  8676 | ` * php's __serialize(): [[obj, inf, obj, inf, …], members]. This is what serialize()` |
|     - |  8677 | ` * actually uses; the Serializable pair below is the legacy format nothing else in` |
|     - |  8678 | ` * php reads or writes. The members slot is the instance's own properties — empty for` |
|     - |  8679 | ` * a bare SplObjectStorage, a SUBCLASS's declared slots when there is one.` |
|     - |  8680 | ` */` |
|    12 |  8681 | `static int vm_builtin_SplObjectStorage_serializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8682 | `{` |
|    13 |  8683 | `	ph7_vm *pVm = pCtx->pVm;` |
|    13 |  8684 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    13 |  8685 | `	ph7_hashmap *pMap = SosMap(pVm,pThis);` |
|     - |  8686 | `	ph7_hashmap_node *pNode;` |
|     - |  8687 | `	ph7_value sOut,sFlat,sMembers;` |
|     - |  8688 | `	sxu32 n;` |
|     6 |  8689 | `	SXUNUSED(nArg);` |
|     6 |  8690 | `	SXUNUSED(apArg);` |
|    13 |  8691 | `	PH7_MemObjInit(pVm,&sOut);` |
|    13 |  8692 | `	PH7_MemObjInit(pVm,&sFlat);` |
|    12 |  8693 | `	if( SplMembersOf(pVm,pThis,&sMembers) != SXRET_OK` |
|    12 |  8694 | `	 \|\| PH7_MemObjToHashmap(&sOut) != SXRET_OK` |
|    13 |  8695 | `	 \|\| PH7_MemObjToHashmap(&sFlat) != SXRET_OK ){` |
|   ! 0 |  8696 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 |  8697 | `		PH7_MemObjRelease(&sFlat);` |
|   ! 0 |  8698 | `		PH7_MemObjRelease(&sMembers);` |
|   ! 0 |  8699 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  8700 | `	}` |
|    13 |  8701 | `	if( pMap ){` |
|    13 |  8702 | `		pNode = pMap->pFirst;` |
|    29 |  8703 | `		for( n = 0 ; n < pMap->nEntry && pNode ; ++n ){` |
|    17 |  8704 | `			ph7_value *pPair = HashmapExtractNodeValue(pNode);` |
|    17 |  8705 | `			ph7_value *pObj = SosPart(pPair,"obj");` |
|    17 |  8706 | `			ph7_value *pInf = SosPart(pPair,"inf");` |
|    17 |  8707 | `			if( pObj ){` |
|    17 |  8708 | `				ph7_array_add_elem(&sFlat,0,pObj);` |
|    17 |  8709 | `				if( pInf ){` |
|    17 |  8710 | `					ph7_array_add_elem(&sFlat,0,pInf);` |
|     8 |  8711 | `				}` |
|     8 |  8712 | `			}` |
|    17 |  8713 | `			pNode = pNode->pPrev;` |
|     9 |  8714 | `		}` |
|     6 |  8715 | `	}` |
|    13 |  8716 | `	ph7_array_add_elem(&sOut,0,&sFlat);` |
|    13 |  8717 | `	ph7_array_add_elem(&sOut,0,&sMembers);` |
|    13 |  8718 | `	ph7_result_value(pCtx,&sOut);` |
|    13 |  8719 | `	PH7_MemObjRelease(&sOut);` |
|    13 |  8720 | `	PH7_MemObjRelease(&sFlat);` |
|    13 |  8721 | `	PH7_MemObjRelease(&sMembers);` |
|    13 |  8722 | `	return PH7_OK;` |
|     7 |  8723 | `}` |
|     2 |  8724 | `static sxi32 SosIllTyped(ph7_context *pCtx)` |
|     1 |  8725 | `{` |
|     3 |  8726 | `	return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - |  8727 | `		"Incomplete or ill-typed serialization data");` |
|     1 |  8728 | `}` |
|    12 |  8729 | `static int vm_builtin_SplObjectStorage_unserializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8730 | `{` |
|    13 |  8731 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  8732 | `	ph7_hashmap *pFlat;` |
|    13 |  8733 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  8734 | `	ph7_value *pStorage,*pMembers;` |
|     - |  8735 | `	sxi64 i,n;` |
|    13 |  8736 | `	sxi32 rc = PH7_OK;` |
|    13 |  8737 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 \|\| pThis == 0 ){` |
|   ! 0 |  8738 | `		return SosIllTyped(pCtx);` |
|     - |  8739 | `	}` |
|    13 |  8740 | `	if( HashmapLookupIntKey((ph7_hashmap *)apArg[0]->x.pOther,0,&pNode) != SXRET_OK ){` |
|   ! 0 |  8741 | `		return SosIllTyped(pCtx);` |
|     - |  8742 | `	}` |
|    13 |  8743 | `	pStorage = HashmapExtractNodeValue(pNode);` |
|    13 |  8744 | `	pNode = 0;` |
|    13 |  8745 | `	if( HashmapLookupIntKey((ph7_hashmap *)apArg[0]->x.pOther,1,&pNode) != SXRET_OK ){` |
|   ! 0 |  8746 | `		return SosIllTyped(pCtx);` |
|     - |  8747 | `	}` |
|    13 |  8748 | `	pMembers = HashmapExtractNodeValue(pNode);` |
|    12 |  8749 | `	if( pStorage == 0 \|\| (pStorage->iFlags & MEMOBJ_HASHMAP) == 0` |
|    12 |  8750 | `	 \|\| pMembers == 0 \|\| (pMembers->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     3 |  8751 | `		return SosIllTyped(pCtx);` |
|     - |  8752 | `	}` |
|    11 |  8753 | `	pFlat = (ph7_hashmap *)pStorage->x.pOther;` |
|    11 |  8754 | `	n = (sxi64)pFlat->nEntry;` |
|    11 |  8755 | `	if( n % 2 != 0 ){` |
|     3 |  8756 | `		return PH7_VmThrowException(pCtx,"UnexpectedValueException","Odd number of elements");` |
|     - |  8757 | `	}` |
|    19 |  8758 | `	for( i = 0 ; i < n && rc == PH7_OK ; i += 2 ){` |
|     - |  8759 | `		ph7_value *pObj,*pInf;` |
|    13 |  8760 | `		pNode = 0;` |
|    13 |  8761 | `		if( HashmapLookupIntKey(pFlat,i,&pNode) != SXRET_OK ){` |
|   ! 0 |  8762 | `			return SosIllTyped(pCtx);` |
|     - |  8763 | `		}` |
|    13 |  8764 | `		pObj = HashmapExtractNodeValue(pNode);` |
|    13 |  8765 | `		if( pObj == 0 \|\| (pObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     3 |  8766 | `			return PH7_VmThrowException(pCtx,"UnexpectedValueException","Non-object key");` |
|     - |  8767 | `		}` |
|    11 |  8768 | `		pNode = 0;` |
|    11 |  8769 | `		pInf = HashmapLookupIntKey(pFlat,i+1,&pNode) == SXRET_OK` |
|    10 |  8770 | `			? HashmapExtractNodeValue(pNode) : 0;` |
|    11 |  8771 | `		rc = SosAttach(pCtx,pThis,pObj,pInf);` |
|     6 |  8772 | `	}` |
|     7 |  8773 | `	if( rc == PH7_OK ){` |
|     7 |  8774 | `		SplMembersLoad(pThis,pMembers);` |
|     3 |  8775 | `	}` |
|     7 |  8776 | `	return rc;` |
|     7 |  8777 | `}` |
|     - |  8778 | `/*` |
|     - |  8779 | ` * php's Serializable pair, kept because the interface is still declared. The` |
|     - |  8780 | `` * format is `x:` + the serialized COUNT, then one `<obj>,<inf>;` per element, then`` |
|     - |  8781 | `` * `m:` + the serialized members -- and php writes it through ONE serializer state,`` |
|     - |  8782 | `` * so an object that appears twice becomes an `r:` back-reference there and a`` |
|     - |  8783 | ` * second copy here (§10; the DLL's legacy pair has the same shape).` |
|     - |  8784 | ` */` |
|     4 |  8785 | `static int vm_builtin_SplObjectStorage_serialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8786 | `{` |
|     5 |  8787 | `	ph7_vm *pVm = pCtx->pVm;` |
|     5 |  8788 | `	ph7_hashmap *pMap = SosMap(pVm,PH7_ContextThis(pCtx));` |
|     - |  8789 | `	ph7_hashmap_node *pNode;` |
|     - |  8790 | `	SyBlob sOut;` |
|     - |  8791 | `	ph7_value sVal,*apCall[1];` |
|     - |  8792 | `	sxu32 n;` |
|     2 |  8793 | `	SXUNUSED(nArg);` |
|     2 |  8794 | `	SXUNUSED(apArg);` |
|     5 |  8795 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|     5 |  8796 | `	SyBlobAppend(&sOut,"x:",sizeof("x:")-1);` |
|     5 |  8797 | `	PH7_MemObjInitFromInt(pVm,&sVal,pMap ? (sxi64)pMap->nEntry : 0);` |
|     5 |  8798 | `	apCall[0] = &sVal;` |
|     5 |  8799 | `	SplSerializeInto(pCtx,apCall,&sOut);` |
|     5 |  8800 | `	PH7_MemObjRelease(&sVal);` |
|     5 |  8801 | `	if( pMap ){` |
|     5 |  8802 | `		pNode = pMap->pFirst;` |
|    13 |  8803 | `		for( n = 0 ; n < pMap->nEntry && pNode ; ++n ){` |
|     9 |  8804 | `			ph7_value *pPair = HashmapExtractNodeValue(pNode);` |
|     9 |  8805 | `			ph7_value *pObj = SosPart(pPair,"obj");` |
|     9 |  8806 | `			ph7_value *pInf = SosPart(pPair,"inf");` |
|     - |  8807 | `			ph7_value sNull;` |
|     9 |  8808 | `			if( pObj ){` |
|     9 |  8809 | `				apCall[0] = pObj;` |
|     9 |  8810 | `				SplSerializeInto(pCtx,apCall,&sOut);` |
|     9 |  8811 | `				SyBlobAppend(&sOut,",",1);` |
|     9 |  8812 | `				PH7_MemObjInit(pVm,&sNull);` |
|     9 |  8813 | `				apCall[0] = pInf ? pInf : &sNull;` |
|     9 |  8814 | `				SplSerializeInto(pCtx,apCall,&sOut);` |
|     9 |  8815 | `				PH7_MemObjRelease(&sNull);` |
|     9 |  8816 | `				SyBlobAppend(&sOut,";",1);` |
|     4 |  8817 | `			}` |
|     9 |  8818 | `			pNode = pNode->pPrev;` |
|     5 |  8819 | `		}` |
|     2 |  8820 | `	}` |
|     5 |  8821 | `	SyBlobAppend(&sOut,"m:",sizeof("m:")-1);` |
|     5 |  8822 | `	PH7_MemObjInit(pVm,&sVal);` |
|     5 |  8823 | `	if( PH7_MemObjToHashmap(&sVal) == SXRET_OK ){` |
|     5 |  8824 | `		apCall[0] = &sVal;` |
|     5 |  8825 | `		SplSerializeInto(pCtx,apCall,&sOut);` |
|     2 |  8826 | `	}` |
|     5 |  8827 | `	PH7_MemObjRelease(&sVal);` |
|     - |  8828 | `	/* ph7_result_string APPENDS too, and pRet still holds the last nested answer` |
|     - |  8829 | `	 * (rule 54) -- drop it before writing this one. */` |
|     5 |  8830 | `	if( pCtx->pRet ){` |
|     5 |  8831 | `		PH7_MemObjRelease(pCtx->pRet);` |
|     2 |  8832 | `	}` |
|     5 |  8833 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     5 |  8834 | `	SyBlobRelease(&sOut);` |
|     5 |  8835 | `	return PH7_OK;` |
|     1 |  8836 | `}` |
|     - |  8837 | `/* php reports WHERE its parse gave up, in bytes, and every failure below is that` |
|     - |  8838 | ` * one exception. */` |
|     2 |  8839 | `static sxi32 SosOffsetErr(ph7_context *pCtx,int nAt,int nTotal)` |
|     1 |  8840 | `{` |
|     4 |  8841 | `	return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     1 |  8842 | `		"Error at offset %d of %d bytes",nAt,nTotal);` |
|     1 |  8843 | `}` |
|     6 |  8844 | `static int vm_builtin_SplObjectStorage_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  8845 | `{` |
|     7 |  8846 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 |  8847 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  8848 | `	const char *zData;` |
|     7 |  8849 | `	int nData = 0,nAt = 0,nRead = 0;` |
|     - |  8850 | `	ph7_value sVal;` |
|     - |  8851 | `	sxi64 nCount,i;` |
|     - |  8852 | `	sxi32 rc;` |
|     7 |  8853 | `	if( nArg < 1 \|\| pThis == 0 ){` |
|   ! 0 |  8854 | `		return PH7_OK;` |
|     - |  8855 | `	}` |
|     7 |  8856 | `	zData = ph7_value_to_string(apArg[0],&nData);` |
|     7 |  8857 | `	if( nData < 1 ){` |
|     3 |  8858 | `		return PH7_OK;   /* php returns without touching the storage */` |
|     - |  8859 | `	}` |
|     5 |  8860 | `	if( nData < 2 \|\| zData[0] != 'x' \|\| zData[1] != ':' ){` |
|   ! 0 |  8861 | `		return SosOffsetErr(pCtx,zData[0] == 'x' ? 1 : 0,nData);` |
|     - |  8862 | `	}` |
|     5 |  8863 | `	nAt = 2;` |
|     5 |  8864 | `	PH7_MemObjInit(pVm,&sVal);` |
|     5 |  8865 | `	rc = PH7_VmUnserializeOne(pCtx,&zData[nAt],nData - nAt,&nRead,&sVal);` |
|     5 |  8866 | `	if( rc == PH7_EXCEPTION ){` |
|   ! 0 |  8867 | `		PH7_MemObjRelease(&sVal);` |
|   ! 0 |  8868 | `		return rc;` |
|     - |  8869 | `	}` |
|     5 |  8870 | `	if( rc != SXRET_OK \|\| (sVal.iFlags & MEMOBJ_INT) == 0 ){` |
|     - |  8871 | `		/* php reports where its parser STOPPED, which for a well-formed value of` |
|     - |  8872 | `		 * the wrong type is the byte after it. */` |
|     3 |  8873 | `		PH7_MemObjRelease(&sVal);` |
|     3 |  8874 | `		return SosOffsetErr(pCtx,nAt + nRead,nData);` |
|     - |  8875 | `	}` |
|     3 |  8876 | `	nCount = ph7_value_to_int64(&sVal);` |
|     3 |  8877 | `	PH7_MemObjRelease(&sVal);` |
|     3 |  8878 | `	nAt += nRead - 1;   /* php steps back onto the ';' that ends the count */` |
|     3 |  8879 | `	if( nCount < 0 ){` |
|   ! 0 |  8880 | `		return SosOffsetErr(pCtx,nAt,nData);` |
|     - |  8881 | `	}` |
|     9 |  8882 | `	for( i = 0 ; i < nCount ; ++i ){` |
|     - |  8883 | `		ph7_value sObj,sInf;` |
|     7 |  8884 | `		if( nAt >= nData \|\| zData[nAt] != ';' ){` |
|   ! 0 |  8885 | `			return SosOffsetErr(pCtx,nAt,nData);` |
|     - |  8886 | `		}` |
|     7 |  8887 | `		nAt++;` |
|     7 |  8888 | `		if( nAt >= nData \|\| (zData[nAt] != 'O' && zData[nAt] != 'C' && zData[nAt] != 'r') ){` |
|   ! 0 |  8889 | `			return SosOffsetErr(pCtx,nAt,nData);` |
|     - |  8890 | `		}` |
|     7 |  8891 | `		PH7_MemObjInit(pVm,&sObj);` |
|     7 |  8892 | `		PH7_MemObjInit(pVm,&sInf);` |
|     7 |  8893 | `		rc = PH7_VmUnserializeOne(pCtx,&zData[nAt],nData - nAt,&nRead,&sObj);` |
|     7 |  8894 | `		if( rc == PH7_EXCEPTION ){` |
|   ! 0 |  8895 | `			PH7_MemObjRelease(&sObj);` |
|   ! 0 |  8896 | `			PH7_MemObjRelease(&sInf);` |
|   ! 0 |  8897 | `			return rc;` |
|     - |  8898 | `		}` |
|     7 |  8899 | `		if( rc != SXRET_OK \|\| (sObj.iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 |  8900 | `			PH7_MemObjRelease(&sObj);` |
|   ! 0 |  8901 | `			PH7_MemObjRelease(&sInf);` |
|   ! 0 |  8902 | `			return SosOffsetErr(pCtx,nAt + nRead,nData);` |
|     - |  8903 | `		}` |
|     7 |  8904 | `		nAt += nRead;` |
|     7 |  8905 | `		if( nAt < nData && zData[nAt] == ',' ){` |
|     7 |  8906 | `			nAt++;` |
|     7 |  8907 | `			rc = PH7_VmUnserializeOne(pCtx,&zData[nAt],nData - nAt,&nRead,&sInf);` |
|     7 |  8908 | `			if( rc == PH7_EXCEPTION ){` |
|   ! 0 |  8909 | `				PH7_MemObjRelease(&sObj);` |
|   ! 0 |  8910 | `				PH7_MemObjRelease(&sInf);` |
|   ! 0 |  8911 | `				return rc;` |
|     - |  8912 | `			}` |
|     7 |  8913 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  8914 | `				PH7_MemObjRelease(&sObj);` |
|   ! 0 |  8915 | `				PH7_MemObjRelease(&sInf);` |
|   ! 0 |  8916 | `				return SosOffsetErr(pCtx,nAt + nRead,nData);` |
|     - |  8917 | `			}` |
|     7 |  8918 | `			nAt += nRead;` |
|     3 |  8919 | `		}` |
|     7 |  8920 | `		rc = SosAttach(pCtx,pThis,&sObj,&sInf);` |
|     7 |  8921 | `		PH7_MemObjRelease(&sObj);` |
|     7 |  8922 | `		PH7_MemObjRelease(&sInf);` |
|     7 |  8923 | `		if( rc != PH7_OK ){` |
|   ! 0 |  8924 | `			return rc;` |
|     - |  8925 | `		}` |
|     4 |  8926 | `	}` |
|     3 |  8927 | `	if( nAt >= nData \|\| zData[nAt] != ';' ){` |
|   ! 0 |  8928 | `		return SosOffsetErr(pCtx,nAt,nData);` |
|     - |  8929 | `	}` |
|     3 |  8930 | `	nAt++;` |
|     3 |  8931 | `	if( nAt + 1 >= nData \|\| zData[nAt] != 'm' \|\| zData[nAt+1] != ':' ){` |
|   ! 0 |  8932 | `		return SosOffsetErr(pCtx,nAt < nData && zData[nAt] == 'm' ? nAt + 1 : nAt,nData);` |
|     - |  8933 | `	}` |
|     3 |  8934 | `	nAt += 2;` |
|     3 |  8935 | `	PH7_MemObjInit(pVm,&sVal);` |
|     3 |  8936 | `	rc = PH7_VmUnserializeOne(pCtx,&zData[nAt],nData - nAt,&nRead,&sVal);` |
|     3 |  8937 | `	if( rc == PH7_EXCEPTION ){` |
|   ! 0 |  8938 | `		PH7_MemObjRelease(&sVal);` |
|   ! 0 |  8939 | `		return rc;` |
|     - |  8940 | `	}` |
|     3 |  8941 | `	if( rc != SXRET_OK \|\| (sVal.iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 |  8942 | `		PH7_MemObjRelease(&sVal);` |
|   ! 0 |  8943 | `		return SosOffsetErr(pCtx,nAt + nRead,nData);` |
|     - |  8944 | `	}` |
|     - |  8945 | `	/* php loads the members onto the object here; a native class declares none` |
|     - |  8946 | `	 * that a payload could name and PHL has no dynamic properties to create. */` |
|     3 |  8947 | `	PH7_MemObjRelease(&sVal);` |
|     3 |  8948 | `	return PH7_OK;` |
|     4 |  8949 | `}` |
|     - |  8950 | `/*` |
|     - |  8951 | ` * The declaration. Method ORDER is spl_observer.stub.php's, the two observer` |
|     - |  8952 | ` * interfaces are methodless-but-typed contracts php declares beside it, and` |
|     - |  8953 | ` * seek() is the ONE method php does not mark tentative.` |
|     - |  8954 | ` */` |
|  5740 |  8955 | `static sxi32 VmInstallSplObjectStorage(ph7_vm *pVm)` |
|     5 |  8956 | `{` |
|     - |  8957 | `	static const PH7_NativeMethodDef aObserverMethod[] = {` |
|     - |  8958 | `		{ "update", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "SplSubject $subject", "@void", 0 },` |
|     - |  8959 | `	};` |
|     - |  8960 | `	static const PH7_NativeMethodDef aSubjectMethod[] = {` |
|     - |  8961 | `		{ "attach", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "SplObserver $observer", "@void", 0 },` |
|     - |  8962 | `		{ "detach", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "SplObserver $observer", "@void", 0 },` |
|     - |  8963 | `		{ "notify", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@void", 0 },` |
|     - |  8964 | `	};` |
|     - |  8965 | `	static const PH7_NativePropDef aSosProp[] = {` |
|     - |  8966 | `		{ SOS_S, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  8967 | `		{ SOS_I, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  8968 | `	};` |
|     - |  8969 | `	static const PH7_NativeMethodDef aSosMethod[] = {` |
|     - |  8970 | `		{ "attach",          PH7_MOD_PUBLIC, "object $object, mixed $info = null", "@void",` |
|     - |  8971 | `		  vm_builtin_SplObjectStorage_attach },` |
|     - |  8972 | `		{ "detach",          PH7_MOD_PUBLIC, "object $object", "@void",` |
|     - |  8973 | `		  vm_builtin_SplObjectStorage_detach },` |
|     - |  8974 | `		{ "contains",        PH7_MOD_PUBLIC, "object $object", "@bool",` |
|     - |  8975 | `		  vm_builtin_SplObjectStorage_contains },` |
|     - |  8976 | `		{ "addAll",          PH7_MOD_PUBLIC, "SplObjectStorage $storage", "@int",` |
|     - |  8977 | `		  vm_builtin_SplObjectStorage_addAll },` |
|     - |  8978 | `		{ "removeAll",       PH7_MOD_PUBLIC, "SplObjectStorage $storage", "@int",` |
|     - |  8979 | `		  vm_builtin_SplObjectStorage_removeAll },` |
|     - |  8980 | `		{ "removeAllExcept", PH7_MOD_PUBLIC, "SplObjectStorage $storage", "@int",` |
|     - |  8981 | `		  vm_builtin_SplObjectStorage_removeAllExcept },` |
|     - |  8982 | `		{ "getInfo",         PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplObjectStorage_getInfo },` |
|     - |  8983 | `		{ "setInfo",         PH7_MOD_PUBLIC, "mixed $info", "@void",` |
|     - |  8984 | `		  vm_builtin_SplObjectStorage_setInfo },` |
|     - |  8985 | `		{ "count",           PH7_MOD_PUBLIC, "int $mode = 0", "@int",` |
|     - |  8986 | `		  vm_builtin_SplObjectStorage_count },` |
|     - |  8987 | `		{ "rewind",          PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplObjectStorage_rewind },` |
|     - |  8988 | `		{ "valid",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplObjectStorage_valid },` |
|     - |  8989 | `		{ "key",             PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplObjectStorage_key },` |
|     - |  8990 | `		{ "current",         PH7_MOD_PUBLIC, "", "@object", vm_builtin_SplObjectStorage_current },` |
|     - |  8991 | `		{ "next",            PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplObjectStorage_next },` |
|     - |  8992 | `		{ "seek",            PH7_MOD_PUBLIC, "int $offset", "void",` |
|     - |  8993 | `		  vm_builtin_SplObjectStorage_seek },` |
|     - |  8994 | `		{ "unserialize",     PH7_MOD_PUBLIC, "string $data", "@void",` |
|     - |  8995 | `		  vm_builtin_SplObjectStorage_unserialize },` |
|     - |  8996 | `		{ "serialize",       PH7_MOD_PUBLIC, "", "@string",` |
|     - |  8997 | `		  vm_builtin_SplObjectStorage_serialize },` |
|     - |  8998 | ``		/* php's stub leaves these four offsets UNTYPED (a `@param object` docblock`` |
|     - |  8999 | `		 * Reflection does not print) while the ZPP takes an object -- so the` |
|     - |  9000 | `		 * signature says nothing and each body words its own refusal. */` |
|     - |  9001 | `		{ "offsetExists",    PH7_MOD_PUBLIC, "$object", "@bool",` |
|     - |  9002 | `		  vm_builtin_SplObjectStorage_offsetExists },` |
|     - |  9003 | `		{ "offsetGet",       PH7_MOD_PUBLIC, "$object", "@mixed",` |
|     - |  9004 | `		  vm_builtin_SplObjectStorage_offsetGet },` |
|     - |  9005 | `		{ "offsetSet",       PH7_MOD_PUBLIC, "$object, mixed $info = null", "@void",` |
|     - |  9006 | `		  vm_builtin_SplObjectStorage_offsetSet },` |
|     - |  9007 | `		{ "offsetUnset",     PH7_MOD_PUBLIC, "$object", "@void",` |
|     - |  9008 | `		  vm_builtin_SplObjectStorage_offsetUnset },` |
|     - |  9009 | `		{ "getHash",         PH7_MOD_PUBLIC, "object $object", "@string",` |
|     - |  9010 | `		  vm_builtin_SplObjectStorage_getHash },` |
|     - |  9011 | `		{ "__serialize",     PH7_MOD_PUBLIC, "", "@array",` |
|     - |  9012 | `		  vm_builtin_SplObjectStorage_serializeMagic },` |
|     - |  9013 | `		{ "__unserialize",   PH7_MOD_PUBLIC, "array $data", "@void",` |
|     - |  9014 | `		  vm_builtin_SplObjectStorage_unserializeMagic },` |
|     - |  9015 | `		{ "__debugInfo",     PH7_MOD_PUBLIC, "", "@array",` |
|     - |  9016 | `		  vm_builtin_SplObjectStorage_debugInfo },` |
|     - |  9017 | `	};` |
|     - |  9018 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - |  9019 | `		{ "SplObserver", 0, 0, PH7_CLASS_INTERFACE,` |
|     - |  9020 | `		  aObserverMethod, SX_ARRAYSIZE(aObserverMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  9021 | `		{ "SplSubject", 0, 0, PH7_CLASS_INTERFACE,` |
|     - |  9022 | `		  aSubjectMethod, SX_ARRAYSIZE(aSubjectMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - |  9023 | `		{ "SplObjectStorage", 0, "Countable,SeekableIterator,Serializable,ArrayAccess", 0,` |
|     - |  9024 | `		  aSosMethod, SX_ARRAYSIZE(aSosMethod), 0, 0,` |
|     - |  9025 | `		  aSosProp, SX_ARRAYSIZE(aSosProp), 0, 0, SosPresent },` |
|     - |  9026 | `	};` |
|  5745 |  9027 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 |  9028 | `}` |
|     - |  9029 | `/*` |
|     - |  9030 | ` * ---------------------------------------------------------------------------` |
|     - |  9031 | ` * MultipleIterator: several iterators stepped in LOCKSTEP.` |
|     - |  9032 | ` *` |
|     - |  9033 | ` * php builds it on the very storage above — its C struct IS an` |
|     - |  9034 | `` * spl_SplObjectStorage, which is why `__debugInfo()` answers under`` |
|     - |  9035 | ` * SplObjectStorage's own mangled key — so this class holds the same {obj, inf}` |
|     - |  9036 | ` * table and reuses the same attach/detach/hash routines. What it adds is two` |
|     - |  9037 | ` * flags and the rule they make: MIT_NEED_ALL is valid only while EVERY` |
|     - |  9038 | ` * sub-iterator is, MIT_NEED_ANY while any one is, and an empty set is never` |
|     - |  9039 | ` * valid at all.` |
|     - |  9040 | ` *` |
|     - |  9041 | ` * current() and key() answer an ARRAY built in attach order, and how they treat` |
|     - |  9042 | ` * an exhausted member is the difference between the two modes: under NEED_ANY it` |
|     - |  9043 | ` * contributes NULL and the walk carries on, under NEED_ALL it is php's` |
|     - |  9044 | `` * `Called current() with non valid sub iterator` — a different refusal from the`` |
|     - |  9045 | `` * empty set's `Called current() on an invalid iterator`. MIT_KEYS_ASSOC keys that`` |
|     - |  9046 | `` * array by the `$info` each iterator was attached with, which is what makes a`` |
|     - |  9047 | ` * NULL info an error at KEY time rather than at attach time, and what makes a` |
|     - |  9048 | ` * DUPLICATE info an error at attach.` |
|     - |  9049 | ` */` |
|     - |  9050 | `#define MIT_NEED_ANY      0` |
|     - |  9051 | `#define MIT_NEED_ALL      1` |
|     - |  9052 | `#define MIT_KEYS_NUMERIC  0` |
|     - |  9053 | `#define MIT_KEYS_ASSOC    2` |
|     - |  9054 | `#define MIT_FL "__mfl"   /* php's flags word, stored raw */` |
|     - |  9055 |  |
|    98 |  9056 | `static sxi64 MitFlags(ph7_class_instance *pThis)` |
|     1 |  9057 | `{` |
|    99 |  9058 | `	return pThis ? PH7_NativeAttrInt(pThis,MIT_FL) : 0;` |
|     1 |  9059 | `}` |
|     - |  9060 | `/*` |
|     - |  9061 | `` * php compares two `$info`s with zend_is_identical, not with `==` and not as`` |
|     - |  9062 | ` * array keys: the string "5" and the int 5 are DIFFERENT infos and both may be` |
|     - |  9063 | `` * attached, while `true` and `1.5` are the same one because the ZPP narrowed`` |
|     - |  9064 | ` * both to the int 1.` |
|     - |  9065 | ` */` |
|    20 |  9066 | `static int MitInfoSame(ph7_value *pA,ph7_value *pB)` |
|     1 |  9067 | `{` |
|    21 |  9068 | `	if( (pA->iFlags & MEMOBJ_STRING) != (pB->iFlags & MEMOBJ_STRING) ){` |
|     3 |  9069 | `		return 0;` |
|     - |  9070 | `	}` |
|    19 |  9071 | `	if( pA->iFlags & MEMOBJ_STRING ){` |
|    17 |  9072 | `		sxu32 nA = SyBlobLength(&pA->sBlob), nB = SyBlobLength(&pB->sBlob);` |
|    17 |  9073 | `		return nA == nB` |
|    24 |  9074 | `			&& (nA == 0 \|\| SyMemcmp(SyBlobData(&pA->sBlob),SyBlobData(&pB->sBlob),nA) == 0);` |
|     - |  9075 | `	}` |
|     3 |  9076 | `	if( (pA->iFlags & MEMOBJ_NULL) \|\| (pB->iFlags & MEMOBJ_NULL) ){` |
|   ! 0 |  9077 | `		return 0;   /* a NULL info is never a duplicate: php only checks a given one */` |
|     - |  9078 | `	}` |
|     3 |  9079 | `	return pA->x.iVal == pB->x.iVal;` |
|    11 |  9080 | `}` |
|     - |  9081 | `/* Call a no-argument method on one sub-iterator. */` |
|   258 |  9082 | `static sxi32 MitCallOn(ph7_vm *pVm,ph7_class_instance *pIt,const char *zName,sxu32 nName,` |
|     - |  9083 | `	ph7_value *pOut)` |
|     1 |  9084 | `{` |
|   259 |  9085 | `	ph7_class_method *pMethod = pIt ? PH7_ClassExtractMethod(pIt->pClass,zName,nName) : 0;` |
|   259 |  9086 | `	if( pMethod == 0 ){` |
|   ! 0 |  9087 | `		return SXRET_OK;` |
|     - |  9088 | `	}` |
|   259 |  9089 | `	return PH7_VmCallClassMethod(pVm,pIt,pMethod,pOut,0,0);` |
|   130 |  9090 | `}` |
|     - |  9091 | `/*` |
|     - |  9092 | ` * SNAPSHOT the members before calling into any of them. A sub-iterator's own` |
|     - |  9093 | ` * rewind()/valid()/next() is user code and may attach or detach on this very` |
|     - |  9094 | ` * object, and a hash walk holding a node pointer across that call is reading a` |
|     - |  9095 | ` * table that moved. The snapshot is an ordinary array of the {obj, inf} pairs,` |
|     - |  9096 | ` * so it holds a reference to every member for the length of the pass.` |
|     - |  9097 | ` */` |
|   126 |  9098 | `static sxi32 MitSnapshot(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)` |
|     1 |  9099 | `{` |
|   127 |  9100 | `	ph7_hashmap *pMap = SosMap(pVm,pThis);` |
|     - |  9101 | `	ph7_hashmap_node *pNode;` |
|     - |  9102 | `	sxu32 n;` |
|   127 |  9103 | `	PH7_MemObjInit(pVm,pOut);` |
|   127 |  9104 | `	if( PH7_MemObjToHashmap(pOut) != SXRET_OK ){` |
|   ! 0 |  9105 | `		return SXERR_MEM;` |
|     - |  9106 | `	}` |
|   127 |  9107 | `	if( pMap == 0 ){` |
|   ! 0 |  9108 | `		return SXRET_OK;` |
|     - |  9109 | `	}` |
|   343 |  9110 | `	for( pNode = pMap->pFirst, n = 0 ; pNode && n < pMap->nEntry ; ++n, pNode = pNode->pPrev ){` |
|   217 |  9111 | `		ph7_value *pPair = HashmapExtractNodeValue(pNode);` |
|   217 |  9112 | `		if( pPair ){` |
|   217 |  9113 | `			ph7_array_add_elem(pOut,0,pPair);` |
|   108 |  9114 | `		}` |
|   109 |  9115 | `	}` |
|   127 |  9116 | `	return SXRET_OK;` |
|    64 |  9117 | `}` |
|     - |  9118 | ``/* One snapshot entry's `obj` half as an instance, and its `inf` half. */`` |
|   236 |  9119 | `static ph7_class_instance * MitShotObj(ph7_value *pOut,sxu32 iAt,ph7_value **ppInf)` |
|     1 |  9120 | `{` |
|   237 |  9121 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  9122 | `	ph7_value *pPair,*pObj;` |
|   237 |  9123 | `	if( ppInf ){` |
|   117 |  9124 | `		*ppInf = 0;` |
|    58 |  9125 | `	}` |
|   236 |  9126 | `	if( (pOut->iFlags & MEMOBJ_HASHMAP) == 0` |
|   237 |  9127 | `	 \|\| HashmapLookupIntKey((ph7_hashmap *)pOut->x.pOther,(sxi64)iAt,&pNode) != SXRET_OK ){` |
|   ! 0 |  9128 | `		return 0;` |
|     - |  9129 | `	}` |
|   237 |  9130 | `	pPair = HashmapExtractNodeValue(pNode);` |
|   237 |  9131 | `	if( ppInf ){` |
|   117 |  9132 | `		*ppInf = SosPart(pPair,"inf");` |
|    58 |  9133 | `	}` |
|   237 |  9134 | `	pObj = SosPart(pPair,"obj");` |
|   237 |  9135 | `	if( pObj == 0 \|\| (pObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 |  9136 | `		return 0;` |
|     - |  9137 | `	}` |
|   237 |  9138 | `	return (ph7_class_instance *)pObj->x.pOther;` |
|   119 |  9139 | `}` |
|   126 |  9140 | `static sxu32 MitShotCount(ph7_value *pShot)` |
|     1 |  9141 | `{` |
|   127 |  9142 | `	return (pShot->iFlags & MEMOBJ_HASHMAP) && pShot->x.pOther` |
|   189 |  9143 | `		? ((ph7_hashmap *)pShot->x.pOther)->nEntry : 0;` |
|     1 |  9144 | `}` |
|     - |  9145 | `/* Walk every sub-iterator, calling one no-argument method on each. */` |
|    48 |  9146 | `static sxi32 MitCallAll(ph7_context *pCtx,const char *zName,sxu32 nName)` |
|     1 |  9147 | `{` |
|    49 |  9148 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  9149 | `	ph7_value sShot;` |
|     - |  9150 | `	sxu32 i,nCount;` |
|    49 |  9151 | `	sxi32 rc = MitSnapshot(pVm,PH7_ContextThis(pCtx),&sShot);` |
|    49 |  9152 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  9153 | `		PH7_MemObjRelease(&sShot);` |
|   ! 0 |  9154 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  9155 | `	}` |
|    49 |  9156 | `	nCount = MitShotCount(&sShot);` |
|   133 |  9157 | `	for( i = 0 ; i < nCount ; ++i ){` |
|    85 |  9158 | `		ph7_class_instance *pIt = MitShotObj(&sShot,i,0);` |
|    85 |  9159 | `		rc = MitCallOn(pVm,pIt,zName,nName,0);` |
|    85 |  9160 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  9161 | `			PH7_MemObjRelease(&sShot);` |
|   ! 0 |  9162 | `			return rc;` |
|     - |  9163 | `		}` |
|    43 |  9164 | `	}` |
|    49 |  9165 | `	PH7_MemObjRelease(&sShot);` |
|    49 |  9166 | `	return PH7_OK;` |
|    25 |  9167 | `}` |
|   120 |  9168 | `static sxi32 MitSubValid(ph7_vm *pVm,ph7_class_instance *pIt,int *pbValid)` |
|     1 |  9169 | `{` |
|     - |  9170 | `	ph7_value sVal;` |
|     - |  9171 | `	sxi32 rc;` |
|   121 |  9172 | `	*pbValid = 0;` |
|   121 |  9173 | `	PH7_MemObjInit(pVm,&sVal);` |
|   121 |  9174 | `	rc = MitCallOn(pVm,pIt,"valid",sizeof("valid")-1,&sVal);` |
|   121 |  9175 | `	if( rc == SXRET_OK ){` |
|   121 |  9176 | `		PH7_MemObjToBool(&sVal);          /* a STATUS, not the answer */` |
|   121 |  9177 | `		*pbValid = sVal.x.iVal != 0;` |
|    60 |  9178 | `	}` |
|   121 |  9179 | `	PH7_MemObjRelease(&sVal);` |
|   121 |  9180 | `	return rc;` |
|     1 |  9181 | `}` |
|    64 |  9182 | `static int vm_builtin_MultipleIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9183 | `{` |
|    65 |  9184 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    65 |  9185 | `	if( pThis == 0 ){` |
|   ! 0 |  9186 | `		return PH7_OK;` |
|     - |  9187 | `	}` |
|   112 |  9188 | `	PH7_NativeSetAttrInt(pCtx->pVm,pThis,MIT_FL,` |
|    47 |  9189 | `		nArg > 0 ? ph7_value_to_int64(apArg[0]) : MIT_NEED_ALL);` |
|    65 |  9190 | `	return PH7_OK;` |
|    33 |  9191 | `}` |
|    20 |  9192 | `static int vm_builtin_MultipleIterator_getFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9193 | `{` |
|    10 |  9194 | `	SXUNUSED(nArg);` |
|    10 |  9195 | `	SXUNUSED(apArg);` |
|    21 |  9196 | `	ph7_result_int64(pCtx,MitFlags(PH7_ContextThis(pCtx)));` |
|    21 |  9197 | `	return PH7_OK;` |
|     1 |  9198 | `}` |
|     8 |  9199 | `static int vm_builtin_MultipleIterator_setFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9200 | `{` |
|     9 |  9201 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     9 |  9202 | `	if( nArg > 0 && pThis ){` |
|     - |  9203 | `		/* php screens nothing here: the word is stored as given. */` |
|     9 |  9204 | `		PH7_NativeSetAttrInt(pCtx->pVm,pThis,MIT_FL,ph7_value_to_int64(apArg[0]));` |
|     4 |  9205 | `	}` |
|     9 |  9206 | `	return PH7_OK;` |
|     1 |  9207 | `}` |
|     - |  9208 | `/*` |
|     - |  9209 | ` * php's attachIterator: the $info must be UNIQUE across the table, which is what` |
|     - |  9210 | ` * MIT_KEYS_ASSOC needs to build a key set — checked whatever the flags say, since` |
|     - |  9211 | ` * they can be turned on later.` |
|     - |  9212 | ` */` |
|    56 |  9213 | `static int vm_builtin_MultipleIterator_attachIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9214 | `{` |
|    57 |  9215 | `	ph7_vm *pVm = pCtx->pVm;` |
|    57 |  9216 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  9217 | `	ph7_hashmap *pMap;` |
|     - |  9218 | `	ph7_hashmap_node *pNode;` |
|     - |  9219 | `	ph7_value sInf;` |
|     - |  9220 | `	sxu32 n;` |
|     - |  9221 | `	sxi32 rc;` |
|    57 |  9222 | `	if( nArg < 1 \|\| pThis == 0 ){` |
|   ! 0 |  9223 | `		return PH7_OK;` |
|     - |  9224 | `	}` |
|    57 |  9225 | `	PH7_MemObjInit(pVm,&sInf);` |
|    57 |  9226 | `	if( nArg > 1 && (apArg[1]->iFlags & MEMOBJ_NULL) == 0 ){` |
|    45 |  9227 | `		PH7_MemObjStore(apArg[1],&sInf);` |
|     - |  9228 | ``		/* php's `string\|int` ZPP: a numeric string stays a string, everything else`` |
|     - |  9229 | `		 * that is not already one becomes an int. */` |
|    45 |  9230 | `		if( (sInf.iFlags & MEMOBJ_STRING) == 0 ){` |
|     9 |  9231 | `			PH7_MemObjToInteger(&sInf);` |
|     4 |  9232 | `		}` |
|    45 |  9233 | `		pMap = SosMap(pVm,pThis);` |
|    61 |  9234 | `		for( pNode = pMap ? pMap->pFirst : 0, n = 0 ; pNode && n < pMap->nEntry ; ++n ){` |
|    21 |  9235 | `			ph7_value *pPair = HashmapExtractNodeValue(pNode);` |
|    21 |  9236 | `			ph7_value *pOld = SosPart(pPair,"inf");` |
|    21 |  9237 | `			if( pOld && MitInfoSame(pOld,&sInf) ){` |
|     5 |  9238 | `				PH7_MemObjRelease(&sInf);` |
|     5 |  9239 | `				return PH7_VmThrowException(pCtx,"InvalidArgumentException",` |
|     - |  9240 | `					"Key duplication error");` |
|     - |  9241 | `			}` |
|    17 |  9242 | `			pNode = pNode->pPrev;` |
|     9 |  9243 | `		}` |
|    20 |  9244 | `	}` |
|    53 |  9245 | `	rc = SosAttach(pCtx,pThis,apArg[0],&sInf);` |
|    53 |  9246 | `	PH7_MemObjRelease(&sInf);` |
|    53 |  9247 | `	return rc;` |
|    29 |  9248 | `}` |
|     6 |  9249 | `static int vm_builtin_MultipleIterator_detachIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9250 | `{` |
|     7 |  9251 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     7 |  9252 | `	if( nArg < 1 \|\| pThis == 0 ){` |
|   ! 0 |  9253 | `		return PH7_OK;` |
|     - |  9254 | `	}` |
|     7 |  9255 | `	return SosDetach(pCtx,pThis,apArg[0],0);` |
|     4 |  9256 | `}` |
|     4 |  9257 | `static int vm_builtin_MultipleIterator_containsIterator(ph7_context *pCtx,int nArg,` |
|     - |  9258 | `	ph7_value **apArg)` |
|     1 |  9259 | `{` |
|     5 |  9260 | `	ph7_vm *pVm = pCtx->pVm;` |
|     5 |  9261 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  9262 | `	ph7_hashmap *pMap;` |
|     5 |  9263 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  9264 | `	ph7_value sObj,sKey;` |
|     - |  9265 | `	sxi32 rc;` |
|     5 |  9266 | `	if( nArg < 1 \|\| pThis == 0 ){` |
|   ! 0 |  9267 | `		return PH7_OK;` |
|     - |  9268 | `	}` |
|     5 |  9269 | `	PH7_MemObjInit(pVm,&sObj);` |
|     5 |  9270 | `	PH7_MemObjInit(pVm,&sKey);` |
|     5 |  9271 | `	PH7_MemObjStore(apArg[0],&sObj);` |
|     5 |  9272 | `	rc = SosKey(pCtx,pThis,&sObj,&sKey);` |
|     5 |  9273 | `	if( rc == PH7_OK ){` |
|     5 |  9274 | `		pMap = SosMap(pVm,pThis);` |
|     9 |  9275 | `		ph7_result_bool(pCtx,` |
|     4 |  9276 | `			pMap && PH7_HashmapLookup(pMap,&sKey,&pNode) == SXRET_OK);` |
|     2 |  9277 | `	}` |
|     5 |  9278 | `	PH7_MemObjRelease(&sObj);` |
|     5 |  9279 | `	PH7_MemObjRelease(&sKey);` |
|     5 |  9280 | `	return rc;` |
|     3 |  9281 | `}` |
|    12 |  9282 | `static int vm_builtin_MultipleIterator_countIterators(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9283 | `{` |
|     - |  9284 | `	ph7_hashmap *pMap;` |
|     6 |  9285 | `	SXUNUSED(nArg);` |
|     6 |  9286 | `	SXUNUSED(apArg);` |
|    13 |  9287 | `	pMap = SosMap(pCtx->pVm,PH7_ContextThis(pCtx));` |
|    13 |  9288 | `	ph7_result_int64(pCtx,pMap ? (sxi64)pMap->nEntry : 0);` |
|    13 |  9289 | `	return PH7_OK;` |
|     1 |  9290 | `}` |
|    22 |  9291 | `static int vm_builtin_MultipleIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9292 | `{` |
|    11 |  9293 | `	SXUNUSED(nArg);` |
|    11 |  9294 | `	SXUNUSED(apArg);` |
|    23 |  9295 | `	return MitCallAll(pCtx,"rewind",sizeof("rewind")-1);` |
|     1 |  9296 | `}` |
|    26 |  9297 | `static int vm_builtin_MultipleIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9298 | `{` |
|    13 |  9299 | `	SXUNUSED(nArg);` |
|    13 |  9300 | `	SXUNUSED(apArg);` |
|    27 |  9301 | `	return MitCallAll(pCtx,"next",sizeof("next")-1);` |
|     1 |  9302 | `}` |
|    26 |  9303 | `static int vm_builtin_MultipleIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9304 | `{` |
|    27 |  9305 | `	ph7_vm *pVm = pCtx->pVm;` |
|    27 |  9306 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    27 |  9307 | `	int bExpect = (MitFlags(pThis) & MIT_NEED_ALL) ? 1 : 0;` |
|     - |  9308 | `	ph7_value sShot;` |
|     - |  9309 | `	sxu32 i,nCount;` |
|     - |  9310 | `	sxi32 rc;` |
|    13 |  9311 | `	SXUNUSED(nArg);` |
|    13 |  9312 | `	SXUNUSED(apArg);` |
|    27 |  9313 | `	rc = MitSnapshot(pVm,pThis,&sShot);` |
|    27 |  9314 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  9315 | `		PH7_MemObjRelease(&sShot);` |
|   ! 0 |  9316 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  9317 | `	}` |
|    27 |  9318 | `	nCount = MitShotCount(&sShot);` |
|    27 |  9319 | `	if( nCount < 1 ){` |
|     - |  9320 | `		/* php: an empty set is never valid, whichever mode it is in. */` |
|     3 |  9321 | `		PH7_MemObjRelease(&sShot);` |
|     3 |  9322 | `		ph7_result_bool(pCtx,0);` |
|     3 |  9323 | `		return PH7_OK;` |
|     - |  9324 | `	}` |
|    45 |  9325 | `	for( i = 0 ; i < nCount ; ++i ){` |
|    37 |  9326 | `		int bValid = 0;` |
|    37 |  9327 | `		rc = MitSubValid(pVm,MitShotObj(&sShot,i,0),&bValid);` |
|    37 |  9328 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  9329 | `			PH7_MemObjRelease(&sShot);` |
|   ! 0 |  9330 | `			return rc;` |
|     - |  9331 | `		}` |
|    37 |  9332 | `		if( bValid != bExpect ){` |
|     - |  9333 | `			/* NEED_ALL stops at the first invalid one, NEED_ANY at the first valid` |
|     - |  9334 | `			 * one, and each answers the opposite of what it was looking for. */` |
|    17 |  9335 | `			PH7_MemObjRelease(&sShot);` |
|    17 |  9336 | `			ph7_result_bool(pCtx,!bExpect);` |
|    17 |  9337 | `			return PH7_OK;` |
|     - |  9338 | `		}` |
|    11 |  9339 | `	}` |
|     9 |  9340 | `	PH7_MemObjRelease(&sShot);` |
|     9 |  9341 | `	ph7_result_bool(pCtx,bExpect);` |
|     9 |  9342 | `	return PH7_OK;` |
|    14 |  9343 | `}` |
|     - |  9344 | `/*` |
|     - |  9345 | ` * php's spl_multiple_iterator_get_all, which current() and key() share. The` |
|     - |  9346 | ` * refusals differ by cause: an EMPTY set is "on an invalid iterator", an` |
|     - |  9347 | ` * exhausted member under NEED_ALL is "with non valid sub iterator", and a NULL` |
|     - |  9348 | ` * $info under MIT_KEYS_ASSOC is the InvalidArgumentException this is the only` |
|     - |  9349 | ` * site of.` |
|     - |  9350 | ` */` |
|    52 |  9351 | `static int MitGetAll(ph7_context *pCtx,int bKey)` |
|     1 |  9352 | `{` |
|    53 |  9353 | `	ph7_vm *pVm = pCtx->pVm;` |
|    53 |  9354 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    53 |  9355 | `	const char *zWhat = bKey ? "key" : "current";` |
|    53 |  9356 | `	sxi64 iFlags = MitFlags(pThis);` |
|     - |  9357 | `	ph7_value sShot,sOut,sVal;` |
|     - |  9358 | `	sxu32 i,nCount;` |
|     - |  9359 | `	sxi32 rc;` |
|    53 |  9360 | `	rc = MitSnapshot(pVm,pThis,&sShot);` |
|    53 |  9361 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  9362 | `		PH7_MemObjRelease(&sShot);` |
|   ! 0 |  9363 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  9364 | `	}` |
|    53 |  9365 | `	nCount = MitShotCount(&sShot);` |
|    53 |  9366 | `	if( nCount < 1 ){` |
|     5 |  9367 | `		PH7_MemObjRelease(&sShot);` |
|     7 |  9368 | `		return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     2 |  9369 | `			"Called %s() on an invalid iterator",zWhat);` |
|     - |  9370 | `	}` |
|    49 |  9371 | `	PH7_MemObjInit(pVm,&sOut);` |
|    49 |  9372 | `	if( PH7_MemObjToHashmap(&sOut) != SXRET_OK ){` |
|   ! 0 |  9373 | `		PH7_MemObjRelease(&sShot);` |
|   ! 0 |  9374 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 |  9375 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  9376 | `	}` |
|   125 |  9377 | `	for( i = 0 ; i < nCount ; ++i ){` |
|    85 |  9378 | `		ph7_value *pInf = 0;` |
|    85 |  9379 | `		ph7_class_instance *pIt = MitShotObj(&sShot,i,&pInf);` |
|    85 |  9380 | `		int bValid = 0;` |
|     - |  9381 | `		/* php asks the sub-iterator FIRST and only then looks at the key it would` |
|     - |  9382 | `		 * file the answer under, so an exhausted member under NEED_ALL reports the` |
|     - |  9383 | `		 * iterator rather than the missing $info. */` |
|    85 |  9384 | `		rc = MitSubValid(pVm,pIt,&bValid);` |
|    85 |  9385 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  9386 | `			goto fail;` |
|     - |  9387 | `		}` |
|    85 |  9388 | `		PH7_MemObjInit(pVm,&sVal);` |
|    85 |  9389 | `		if( bValid ){` |
|    55 |  9390 | `			rc = MitCallOn(pVm,pIt,bKey ? "key" : "current",bKey ? 3 : 7,&sVal);` |
|    55 |  9391 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  9392 | `				PH7_MemObjRelease(&sVal);` |
|   ! 0 |  9393 | `				goto fail;` |
|     1 |  9394 | `			}` |
|    58 |  9395 | `		}else if( iFlags & MIT_NEED_ALL ){` |
|     7 |  9396 | `			PH7_MemObjRelease(&sVal);` |
|     7 |  9397 | `			PH7_MemObjRelease(&sShot);` |
|     7 |  9398 | `			PH7_MemObjRelease(&sOut);` |
|    11 |  9399 | `			return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     3 |  9400 | `				"Called %s() with non valid sub iterator",zWhat);` |
|     - |  9401 | `		}` |
|     - |  9402 | `		/* NEED_ANY leaves the null sVal in place: an exhausted member contributes` |
|     - |  9403 | `		 * php's null and the walk carries on. */` |
|    79 |  9404 | `		if( iFlags & MIT_KEYS_ASSOC ){` |
|     - |  9405 | ``			/* The snapshot's own `inf` slot: re-read after the call above, since it`` |
|     - |  9406 | `			 * lives in a hashmap the call may have moved. */` |
|    33 |  9407 | `			MitShotObj(&sShot,i,&pInf);` |
|    33 |  9408 | `			if( pInf == 0 \|\| (pInf->iFlags & MEMOBJ_NULL) ){` |
|     3 |  9409 | `				PH7_MemObjRelease(&sVal);` |
|     3 |  9410 | `				PH7_MemObjRelease(&sShot);` |
|     3 |  9411 | `				PH7_MemObjRelease(&sOut);` |
|     3 |  9412 | `				return PH7_VmThrowException(pCtx,"InvalidArgumentException",` |
|     - |  9413 | `					"Sub-Iterator is associated with NULL");` |
|     - |  9414 | `			}` |
|    15 |  9415 | `		}` |
|    77 |  9416 | `		ph7_array_add_elem(&sOut,(iFlags & MIT_KEYS_ASSOC) ? pInf : 0,&sVal);` |
|    77 |  9417 | `		PH7_MemObjRelease(&sVal);` |
|    39 |  9418 | `	}` |
|    41 |  9419 | `	PH7_MemObjRelease(&sShot);` |
|    41 |  9420 | `	ph7_result_value(pCtx,&sOut);` |
|    41 |  9421 | `	PH7_MemObjRelease(&sOut);` |
|    41 |  9422 | `	return PH7_OK;` |
|   ! 0 |  9423 | `fail:` |
|   ! 0 |  9424 | `	PH7_MemObjRelease(&sShot);` |
|   ! 0 |  9425 | `	PH7_MemObjRelease(&sOut);` |
|   ! 0 |  9426 | `	return rc;` |
|    27 |  9427 | `}` |
|    24 |  9428 | `static int vm_builtin_MultipleIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9429 | `{` |
|    12 |  9430 | `	SXUNUSED(nArg);` |
|    12 |  9431 | `	SXUNUSED(apArg);` |
|    25 |  9432 | `	return MitGetAll(pCtx,FALSE);` |
|     1 |  9433 | `}` |
|    28 |  9434 | `static int vm_builtin_MultipleIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9435 | `{` |
|    14 |  9436 | `	SXUNUSED(nArg);` |
|    14 |  9437 | `	SXUNUSED(apArg);` |
|    29 |  9438 | `	return MitGetAll(pCtx,TRUE);` |
|     1 |  9439 | `}` |
|  5740 |  9440 | `static sxi32 VmInstallSplMultipleIterator(ph7_vm *pVm)` |
|     5 |  9441 | `{` |
|     - |  9442 | `	static const PH7_NativeConstDef aMitConst[] = {` |
|     - |  9443 | `		{ "MIT_NEED_ANY",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, MIT_NEED_ANY, 0, 0.0 },` |
|     - |  9444 | `		{ "MIT_NEED_ALL",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, MIT_NEED_ALL, 0, 0.0 },` |
|     - |  9445 | `		{ "MIT_KEYS_NUMERIC", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, MIT_KEYS_NUMERIC, 0, 0.0 },` |
|     - |  9446 | `		{ "MIT_KEYS_ASSOC",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, MIT_KEYS_ASSOC, 0, 0.0 },` |
|     - |  9447 | `	};` |
|     - |  9448 | `	static const PH7_NativePropDef aMitProp[] = {` |
|     - |  9449 | `		{ SOS_S,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  9450 | `		{ MIT_FL, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - |  9451 | `	};` |
|     - |  9452 | `	static const PH7_NativeMethodDef aMitMethod[] = {` |
|     - |  9453 | `		{ "__construct",      PH7_MOD_PUBLIC, "int $flags = 1", 0,` |
|     - |  9454 | `		  vm_builtin_MultipleIterator_construct },` |
|     - |  9455 | `		{ "getFlags",         PH7_MOD_PUBLIC, "", "@int", vm_builtin_MultipleIterator_getFlags },` |
|     - |  9456 | `		{ "setFlags",         PH7_MOD_PUBLIC, "int $flags", "@void",` |
|     - |  9457 | `		  vm_builtin_MultipleIterator_setFlags },` |
|     - |  9458 | `		{ "attachIterator",   PH7_MOD_PUBLIC, "Iterator $iterator, string\|int\|null $info = null",` |
|     - |  9459 | `		  "@void", vm_builtin_MultipleIterator_attachIterator },` |
|     - |  9460 | `		{ "detachIterator",   PH7_MOD_PUBLIC, "Iterator $iterator", "@void",` |
|     - |  9461 | `		  vm_builtin_MultipleIterator_detachIterator },` |
|     - |  9462 | `		{ "containsIterator", PH7_MOD_PUBLIC, "Iterator $iterator", "@bool",` |
|     - |  9463 | `		  vm_builtin_MultipleIterator_containsIterator },` |
|     - |  9464 | `		{ "countIterators",   PH7_MOD_PUBLIC, "", "@int",` |
|     - |  9465 | `		  vm_builtin_MultipleIterator_countIterators },` |
|     - |  9466 | `		{ "rewind",           PH7_MOD_PUBLIC, "", "@void", vm_builtin_MultipleIterator_rewind },` |
|     - |  9467 | `		{ "valid",            PH7_MOD_PUBLIC, "", "@bool", vm_builtin_MultipleIterator_valid },` |
|     - |  9468 | `		{ "key",              PH7_MOD_PUBLIC, "", "@array", vm_builtin_MultipleIterator_key },` |
|     - |  9469 | `		{ "current",          PH7_MOD_PUBLIC, "", "@array", vm_builtin_MultipleIterator_current },` |
|     - |  9470 | `		{ "next",             PH7_MOD_PUBLIC, "", "@void", vm_builtin_MultipleIterator_next },` |
|     - |  9471 | `		{ "__debugInfo",      PH7_MOD_PUBLIC, "", "@array",` |
|     - |  9472 | `		  vm_builtin_SplObjectStorage_debugInfo },` |
|     - |  9473 | `	};` |
|     - |  9474 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - |  9475 | `		/* No presenter: php declares no properties and shows none, and the storage` |
|     - |  9476 | `		 * is reachable only through __debugInfo() — which is SplObjectStorage's own,` |
|     - |  9477 | `		 * mangled key included, because the struct behind both classes is one. */` |
|     - |  9478 | `		{ "MultipleIterator", 0, "Iterator", 0,` |
|     - |  9479 | `		  aMitMethod, SX_ARRAYSIZE(aMitMethod), aMitConst, SX_ARRAYSIZE(aMitConst),` |
|     - |  9480 | `		  aMitProp, SX_ARRAYSIZE(aMitProp), 0, 0, 0 },` |
|     - |  9481 | `	};` |
|  5745 |  9482 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 |  9483 | `}` |
|     - |  9484 | `/*` |
|     - |  9485 | ` * ---------------------------------------------------------------------------` |
|     - |  9486 | ` * SplFileInfo.` |
|     - |  9487 | ` *` |
|     - |  9488 | `` * php's `spl_filesystem_object` keeps TWO strings for a path, and which one a`` |
|     - |  9489 | `` * method reads is the whole model: `file_name` is the pathname with any trailing`` |
|     - |  9490 | `` * slashes stripped, and `path` is everything before the LAST slash of it -- which`` |
|     - |  9491 | ` * is EMPTY when the name has no slash before its last component, so` |
|     - |  9492 | `` * `(new SplFileInfo('/a.txt'))->getPath()` is `''` and `getFilename()` answers the`` |
|     - |  9493 | `` * whole `/a.txt`. Every accessor is a slice of that pair (php's own`` |
|     - |  9494 | `` * `spl_filesystem_info_set_filename`), and the chunk, which called `basename()` and`` |
|     - |  9495 | `` * `dirname()` per method instead, disagreed on all of it.`` |
|     - |  9496 | ` *` |
|     - |  9497 | `` * The stat family is php's `FileInfoFunction` macro: `php_stat()` with the error`` |
|     - |  9498 | ` * handler REPLACED, so the warning a failed stat would print becomes a` |
|     - |  9499 | `` * `RuntimeException` instead -- `getSize()` on a missing file RAISES there and`` |
|     - |  9500 | ` * warned-then-answered-false here. Two of the fifteen lstat rather than stat` |
|     - |  9501 | `` * (`getType`, `isLink`), which is php's IS_LINK_OPERATION set.`` |
|     - |  9502 | ` *` |
|     - |  9503 | ` * The two slots are PRIVATE and PRESENTED: php declares no properties at all` |
|     - |  9504 | `` * (`getProperties()`, the `(array)` cast and `get_object_vars()` are empty) while`` |
|     - |  9505 | `` * `var_dump` shows `pathName`/`fileName` under their mangled private keys, and`` |
|     - |  9506 | `` * `__debugInfo()` hands back that same array. The class is `@not-serializable`.`` |
|     - |  9507 | ` *` |
|     - |  9508 | `` * `openFile()` and `setFileClass()` are the doors into SplFileObject and answer`` |
|     - |  9509 | `` * one; `setInfoClass()` and the `?string $class` argument of`` |
|     - |  9510 | `` * `getFileInfo()`/`getPathInfo()` name a class derived from THIS one. The two`` |
|     - |  9511 | ` * class names live in slots of their own, which is what makes them survive a` |
|     - |  9512 | ` * clone and travel to a directory iterator's children.` |
|     - |  9513 | ` */` |
|     - |  9514 | `#define SFI_N  "__n"   /* php's file_name: the pathname, trailing slashes stripped */` |
|     - |  9515 | `#define SFI_P  "__p"   /* php's path: everything before its last slash */` |
|     - |  9516 | `#define SFI_IC "__ic"  /* php's info_class */` |
|     - |  9517 | `#define SFI_FC "__fc"  /* php's file_class, what openFile() builds */` |
|     - |  9518 | `` /* The directory-iterator half of php's struct, on the same instance: its `u.dir` `` |
|     - |  9519 | ` * arm minus the handle, which cannot live in a php-visible slot (see VmDirHandle).` |
|     - |  9520 | `` * Declared by DirectoryIterator, so `SplDirIs()` is what tells the two apart. */`` |
|     - |  9521 | `#define SDI_E  "__e"   /* php's u.dir.entry.d_name; "" once the walk has run out */` |
|     - |  9522 | `#define SDI_I  "__i"   /* php's u.dir.index: what key() answers */` |
|     - |  9523 | `#define SDI_F  "__f"   /* php's flags */` |
|     - |  9524 | `#define SDI_S  "__s"   /* php's u.dir.sub_path (RecursiveDirectoryIterator) */` |
|     - |  9525 | ``/* And the FILE arm, php's `u.file`. Declared by SplFileObject; named up here`` |
|     - |  9526 | ` * because the shared presentation hook shows three of its slots. */` |
|     - |  9527 | `#define SFO_H  "__fh"  /* the open io_private, as a resource */` |
|     - |  9528 | `#define SFO_M  "__fo"  /* php's u.file.open_mode */` |
|     - |  9529 | `#define SFO_FL "__ff"  /* php's flags */` |
|     - |  9530 | `#define SFO_ML "__fm"  /* php's u.file.max_line_len */` |
|     - |  9531 | `#define SFO_D  "__fd"  /* php's u.file.delimiter */` |
|     - |  9532 | `#define SFO_EN "__fn"  /* php's u.file.enclosure */` |
|     - |  9533 | `#define SFO_ES "__fx"  /* php's u.file.escape (PH7_CSV_NO_ESCAPE = disabled) */` |
|     - |  9534 | `#define SFO_ED "__fq"  /* php's u.file.is_escape_default */` |
|     - |  9535 | `#define SFO_L  "__fl"  /* php's u.file.current_line */` |
|     - |  9536 | `#define SFO_Z  "__fz"  /* php's u.file.current_zval */` |
|     - |  9537 | `#define SFO_LS "__fs"  /* which of the two is live (SFO_HAS_*) */` |
|     - |  9538 | `#define SFO_K  "__fk"  /* php's u.file.current_line_num */` |
|     - |  9539 | `/* The open stream behind an SplFileObject, or 0 for one that has none. */` |
|     - |  9540 | `static io_private * SfoDev(ph7_class_instance *pThis);` |
|     - |  9541 |  |
|     - |  9542 | `/* php's IS_SLASH is PLATFORM-dependent: a backslash separates on Windows and is an` |
|     - |  9543 | ``  * ordinary filename byte everywhere else, which is why `new SplFileInfo('C:\\x\\y')` `` |
|     - |  9544 | ` * has an empty path on unix. PH7_ExtractDirName draws the same line. */` |
|     - |  9545 | `#ifdef __WINNT__` |
|     - |  9546 | `# define SFI_IS_SLASH(c) ((c) == '/' \|\| (c) == '\\')` |
|     - |  9547 | `#else` |
|     - |  9548 | `# define SFI_IS_SLASH(c) ((c) == '/')` |
|     - |  9549 | `#endif` |
|     - |  9550 |  |
|     - |  9551 | `/*` |
|     - |  9552 | ` * The directory-iterator half of this family, declared up here because php's` |
|     - |  9553 | `` * SplFileInfo bodies BRANCH on `spl_filesystem_object::type`: a DIR instance`` |
|     - |  9554 | ` * keeps its pathname lazily (path + slash + the current entry, rebuilt after` |
|     - |  9555 | ` * every read) and answers nothing at all once the walk has run out. Exactly` |
|     - |  9556 | ` * five accessors below ask, which is the same five php branches in.` |
|     - |  9557 | ` */` |
|     - |  9558 | `static int SplDirIs(ph7_vm *pVm,ph7_class_instance *pThis);` |
|     - |  9559 | `static const char * SplDirName(ph7_vm *pVm,ph7_class_instance *pThis,int *pnLen);` |
|     - |  9560 | `static int SplDirAtEnd(ph7_class_instance *pThis);` |
|     - |  9561 | `static VmDirHandle * SplDirState(ph7_vm *pVm,ph7_class_instance *pThis);` |
|     - |  9562 | `/* One of the two path slots, as bytes. */` |
|  2001 |  9563 | `static const char * SfiStr(ph7_class_instance *pThis,const char *zSlot,int *pnLen)` |
|     1 |  9564 | `{` |
|  2002 |  9565 | `	ph7_value *pVal = pThis ? PH7_NativeAttr(pThis,zSlot) : 0;` |
|  2002 |  9566 | `	*pnLen = 0;` |
|  2002 |  9567 | `	if( pVal == 0 ){` |
|   ! 0 |  9568 | `		return "";` |
|     - |  9569 | `	}` |
|  2002 |  9570 | `	return ph7_value_to_string(pVal,pnLen);` |
|  1008 |  9571 | `}` |
|     - |  9572 | `/*` |
|     - |  9573 | ` * php's spl_filesystem_info_set_filename: strip the trailing slashes (never the` |
|     - |  9574 | ` * only character), then cut the path at the last slash of what is left. A name` |
|     - |  9575 | ` * with no slash before its final component keeps an EMPTY path, which is what` |
|     - |  9576 | ` * makes getFilename() answer the whole thing.` |
|     - |  9577 | ` */` |
|   230 |  9578 | `static void SfiSetName(ph7_vm *pVm,ph7_class_instance *pThis,const char *zPath,int nPath)` |
|     1 |  9579 | `{` |
|   231 |  9580 | `	int nFile = nPath;` |
|     - |  9581 | `	int nDir;` |
|   231 |  9582 | `	if( nFile > 1 && SFI_IS_SLASH(zPath[nFile-1]) ){` |
|     4 |  9583 | `		do{` |
|     9 |  9584 | `			nFile--;` |
|     9 |  9585 | `		}while( nFile > 1 && SFI_IS_SLASH(zPath[nFile-1]) );` |
|     4 |  9586 | `	}` |
|   231 |  9587 | `	nDir = nFile;` |
|  1983 |  9588 | `	while( nDir > 1 && !SFI_IS_SLASH(zPath[nDir-1]) ){` |
|  1753 |  9589 | `		nDir--;` |
|     1 |  9590 | `	}` |
|   231 |  9591 | `	if( nDir > 0 ){` |
|   227 |  9592 | `		nDir--;` |
|   113 |  9593 | `	}` |
|   231 |  9594 | `	PH7_NativeSetAttrStr(pVm,pThis,SFI_N,zPath,nFile);` |
|   231 |  9595 | `	PH7_NativeSetAttrStr(pVm,pThis,SFI_P,zPath,nDir);` |
|   231 |  9596 | `}` |
|     - |  9597 | `/*` |
|     - |  9598 | `` * php's `file_name`: the slot for a plain SplFileInfo, and the lazily rebuilt`` |
|     - |  9599 | ` * path+slash+entry for a directory iterator. Every accessor that works on the` |
|     - |  9600 | ` * whole pathname goes through here.` |
|     - |  9601 | ` */` |
|   459 |  9602 | `static const char * SfiName(ph7_vm *pVm,ph7_class_instance *pThis,int *pnLen)` |
|     1 |  9603 | `{` |
|   460 |  9604 | `	if( SplDirIs(pVm,pThis) ){` |
|   140 |  9605 | `		return SplDirName(pVm,pThis,pnLen);` |
|     - |  9606 | `	}` |
|   321 |  9607 | `	return SfiStr(pThis,SFI_N,pnLen);` |
|   231 |  9608 | `}` |
|     - |  9609 | `/*` |
|     - |  9610 | `` * php's "the file name without the path": the slice after `path` + its slash when`` |
|     - |  9611 | ` * the path is a real prefix, and the whole name otherwise. getFilename(),` |
|     - |  9612 | ` * getBasename() and getExtension() all start here.` |
|     - |  9613 | ` */` |
|     - |  9614 | `/* A GlobIterator's path comes from its STREAM rather than from its slot` |
|     - |  9615 | ` * (defined with the directory machinery below); 0 for any other object. */` |
|     - |  9616 | `static const char * SplDirGlobPath(ph7_vm *pVm,ph7_class_instance *pThis,int *pnLen);` |
|   128 |  9617 | `static const char * SfiTail(ph7_vm *pVm,ph7_class_instance *pThis,int *pnLen)` |
|     1 |  9618 | `{` |
|   129 |  9619 | `	int nName = 0,nPath = 0;` |
|   129 |  9620 | `	const char *zName = SfiName(pVm,pThis,&nName);` |
|     - |  9621 | `	/* The name was JOINED from the walk's path, which for a glob handle is the` |
|     - |  9622 | ``	 * current match's directory and not the `glob://pattern` in the slot --`` |
|     - |  9623 | `	 * measuring against the slot left the whole joined name here. */` |
|   129 |  9624 | `	if( SplDirGlobPath(pVm,pThis,&nPath) == 0 ){` |
|   125 |  9625 | `		SfiStr(pThis,SFI_P,&nPath);` |
|    62 |  9626 | `	}` |
|   129 |  9627 | `	if( nPath > 0 && nPath < nName ){` |
|    65 |  9628 | `		*pnLen = nName - (nPath + 1);` |
|    65 |  9629 | `		return &zName[nPath + 1];` |
|     - |  9630 | `	}` |
|    65 |  9631 | `	*pnLen = nName;` |
|    65 |  9632 | `	return zName;` |
|    65 |  9633 | `}` |
|     - |  9634 | `/* The path this instance stands for, as a NUL-terminated buffer the VFS can take. */` |
|    83 |  9635 | `static sxi32 SfiPathBuf(ph7_vm *pVm,ph7_class_instance *pThis,char *zBuf,int nBuf)` |
|     1 |  9636 | `{` |
|    84 |  9637 | `	int nName = 0;` |
|    84 |  9638 | `	const char *zName = SfiName(pVm,pThis,&nName);` |
|    84 |  9639 | `	if( nName < 1 \|\| nName >= nBuf ){` |
|   ! 0 |  9640 | `		return SXERR_INVALID;` |
|     - |  9641 | `	}` |
|    84 |  9642 | `	SyMemcpy(zName,zBuf,(sxu32)nName);` |
|    84 |  9643 | `	zBuf[nName] = 0;` |
|    84 |  9644 | `	return SXRET_OK;` |
|    43 |  9645 | `}` |
|     - |  9646 | `/* The two refusals an accessor may owe before it reads anything (below). */` |
|     - |  9647 | `static int SfoChecked(ph7_context *pCtx,sxi32 *pRc);` |
|     - |  9648 | `/* The open the SplFileObject constructor and openFile() share (below). iCtxArg` |
|     - |  9649 | ` * is the php POSITION of the context argument, which differs between the two` |
|     - |  9650 | ` * spellings and is what a refused context is blamed on. */` |
|     - |  9651 | `static sxi32 SfoOpen(ph7_context *pCtx,ph7_class_instance *pThis,ph7_value *pPath,` |
|     - |  9652 | `	const char *zMode,int nMode,int bUseInclude,ph7_value *pCtxArg,int iCtxArg);` |
|     - |  9653 | `/*` |
|     - |  9654 | ` * php's get_file_name() ahead of an accessor that needs a path: an object whose` |
|     - |  9655 | ` * parent constructor never ran has no name AT ALL and raises Error rather than` |
|     - |  9656 | ` * failing a stat -- which for a directory iterator is the case where the open` |
|     - |  9657 | ` * never happened. Answers 0 when the caller must return *pRc.` |
|     - |  9658 | ` *` |
|     - |  9659 | ` * The SplFileObject family reaches these same accessors by inheritance and` |
|     - |  9660 | ` * refuses EARLIER and differently: php gives those classes a get_method handler` |
|     - |  9661 | ` * that stops every method on an uninitialized instance, so that check runs` |
|     - |  9662 | ` * first here too.` |
|     - |  9663 | ` */` |
|    83 |  9664 | `static int SfiDirReady(ph7_context *pCtx,sxi32 *pRc)` |
|     1 |  9665 | `{` |
|    84 |  9666 | `	ph7_vm *pVm = pCtx->pVm;` |
|    84 |  9667 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    84 |  9668 | `	if( !SfoChecked(pCtx,pRc) ){` |
|     3 |  9669 | `		return 0;` |
|     - |  9670 | `	}` |
|    82 |  9671 | `	if( SplDirIs(pVm,pThis) && SplDirState(pVm,pThis) == 0 ){` |
|     7 |  9672 | `		*pRc = PH7_VmThrowException(pCtx,"Error","Object not initialized");` |
|     7 |  9673 | `		return 0;` |
|     - |  9674 | `	}` |
|    76 |  9675 | `	return 1;` |
|    43 |  9676 | `}` |
|     - |  9677 | `/*` |
|     - |  9678 | ` * php's FileInfoFunction: the stat that backs one accessor, with the failure` |
|     - |  9679 | ` * promoted to a RuntimeException carrying the WARNING php would otherwise print.` |
|     - |  9680 | ` * The two lstat users say "Lstat failed" there, which is php's own text.` |
|     - |  9681 | ` */` |
|    42 |  9682 | `static sxi32 SfiStat(ph7_context *pCtx,const char *zMethod,int bLstat,ph7_value *pOut)` |
|     1 |  9683 | `{` |
|    43 |  9684 | `	ph7_vm *pVm = pCtx->pVm;` |
|    43 |  9685 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    43 |  9686 | `	const ph7_vfs *pVfs = pVm->pEngine->pVfs;` |
|     - |  9687 | `	ph7_value sWorker;` |
|     - |  9688 | `	char zPath[4096];` |
|    43 |  9689 | `	int rc = -1;` |
|     - |  9690 | `	sxi32 rcReady;` |
|    43 |  9691 | `	if( !SfiDirReady(pCtx,&rcReady) ){` |
|     5 |  9692 | `		return rcReady;` |
|     - |  9693 | `	}` |
|    39 |  9694 | `	if( PH7_MemObjToHashmap(pOut) != SXRET_OK ){` |
|   ! 0 |  9695 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  9696 | `	}` |
|    39 |  9697 | `	PH7_MemObjInit(pVm,&sWorker);` |
|    39 |  9698 | `	if( SfiPathBuf(pVm,pThis,zPath,(int)sizeof(zPath)) == SXRET_OK && pVfs ){` |
|    39 |  9699 | `		if( bLstat ){` |
|   ! 0 |  9700 | `			rc = pVfs->xlStat ? pVfs->xlStat(zPath,pOut,&sWorker) : -1;` |
|   ! 0 |  9701 | `		}else{` |
|    39 |  9702 | `			rc = pVfs->xStat ? pVfs->xStat(zPath,pOut,&sWorker) : -1;` |
|     - |  9703 | `		}` |
|    19 |  9704 | `	}` |
|    39 |  9705 | `	PH7_MemObjRelease(&sWorker);` |
|    39 |  9706 | `	if( rc != PH7_OK ){` |
|    17 |  9707 | `		int nName = 0;` |
|    17 |  9708 | `		const char *zName = SfiName(pVm,pThis,&nName);` |
|    25 |  9709 | `		return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     8 |  9710 | `			"SplFileInfo::%s(): %s failed for %.*s",zMethod,bLstat ? "Lstat" : "stat",` |
|     8 |  9711 | `			nName,zName);` |
|     - |  9712 | `	}` |
|    23 |  9713 | `	return PH7_OK;` |
|    22 |  9714 | `}` |
|     - |  9715 | `/* One field of a stat array, as php's int. */` |
|    42 |  9716 | `static int SfiStatField(ph7_context *pCtx,const char *zMethod,int bLstat,const char *zField,` |
|     - |  9717 | `	sxi64 *piOut)` |
|     1 |  9718 | `{` |
|     - |  9719 | `	ph7_value sStat,*pField;` |
|     - |  9720 | `	sxi32 rc;` |
|    43 |  9721 | `	*piOut = 0;` |
|    43 |  9722 | `	PH7_MemObjInit(pCtx->pVm,&sStat);` |
|    43 |  9723 | `	rc = SfiStat(pCtx,zMethod,bLstat,&sStat);` |
|    43 |  9724 | `	if( rc != PH7_OK ){` |
|    21 |  9725 | `		PH7_MemObjRelease(&sStat);` |
|    21 |  9726 | `		return rc;` |
|     - |  9727 | `	}` |
|    23 |  9728 | `	pField = ph7_array_fetch(&sStat,zField,(int)SyStrlen(zField));` |
|    23 |  9729 | `	if( pField ){` |
|    23 |  9730 | `		*piOut = ph7_value_to_int64(pField);` |
|    11 |  9731 | `	}` |
|    23 |  9732 | `	PH7_MemObjRelease(&sStat);` |
|    23 |  9733 | `	return PH7_OK;` |
|    22 |  9734 | `}` |
|     - |  9735 | `/* The eight stat accessors that answer an int, all with the same body. */` |
|    42 |  9736 | `static int SfiStatInt(ph7_context *pCtx,const char *zMethod,const char *zField)` |
|     1 |  9737 | `{` |
|    43 |  9738 | `	sxi64 iVal = 0;` |
|    43 |  9739 | `	sxi32 rc = SfiStatField(pCtx,zMethod,FALSE,zField,&iVal);` |
|    43 |  9740 | `	if( rc != PH7_OK ){` |
|    21 |  9741 | `		return rc;` |
|     - |  9742 | `	}` |
|    23 |  9743 | `	ph7_result_int64(pCtx,iVal);` |
|    23 |  9744 | `	return PH7_OK;` |
|    22 |  9745 | `}` |
|    92 |  9746 | `static int vm_builtin_SplFileInfo_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9747 | `{` |
|    93 |  9748 | `	ph7_vm *pVm = pCtx->pVm;` |
|    93 |  9749 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  9750 | `	const char *zPath;` |
|    93 |  9751 | `	int nPath = 0;` |
|    93 |  9752 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|   ! 0 |  9753 | `		return PH7_OK;` |
|     - |  9754 | `	}` |
|    93 |  9755 | `	zPath = ph7_value_to_string(apArg[0],&nPath);` |
|    93 |  9756 | `	SfiSetName(pVm,pThis,zPath,nPath);` |
|    93 |  9757 | `	return PH7_OK;` |
|    47 |  9758 | `}` |
|    54 |  9759 | `static int vm_builtin_SplFileInfo_getPath(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9760 | `{` |
|    55 |  9761 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  9762 | `	sxi32 rcChk;` |
|    55 |  9763 | `	int nPath = 0;` |
|     - |  9764 | `	const char *zPath;` |
|    27 |  9765 | `	SXUNUSED(nArg);` |
|    27 |  9766 | `	SXUNUSED(apArg);` |
|    55 |  9767 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|     3 |  9768 | `		return rcChk;` |
|     - |  9769 | `	}` |
|     - |  9770 | `	/* A GlobIterator answers the directory of the CURRENT match (see` |
|     - |  9771 | `	 * SplDirGlobPath): the pattern in its slot names no directory. */` |
|    53 |  9772 | `	zPath = SplDirGlobPath(pCtx->pVm,pThis,&nPath);` |
|    53 |  9773 | `	if( zPath == 0 ){` |
|    49 |  9774 | `		zPath = SfiStr(pThis,SFI_P,&nPath);` |
|    24 |  9775 | `	}` |
|    53 |  9776 | `	ph7_result_string(pCtx,zPath,nPath);` |
|    53 |  9777 | `	return PH7_OK;` |
|    28 |  9778 | `}` |
|     - |  9779 | `/*` |
|     - |  9780 | `` * php's getPathname() is `spl_filesystem_object_get_pathname`, and for a DIR it`` |
|     - |  9781 | ` * answers NOTHING once the walk has run out — the empty string, without` |
|     - |  9782 | ``  * materializing the lazy name the stat family would still build (`getSize()` `` |
|     - |  9783 | `` * past the end stats the directory itself, and `var_dump` shows the difference).`` |
|     - |  9784 | ` */` |
|    64 |  9785 | `static int vm_builtin_SplFileInfo_getPathname(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9786 | `{` |
|     - |  9787 | `	sxi32 rcChk;` |
|    65 |  9788 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    65 |  9789 | `	int nName = 0;` |
|     - |  9790 | `	const char *zName;` |
|    32 |  9791 | `	SXUNUSED(nArg);` |
|    32 |  9792 | `	SXUNUSED(apArg);` |
|    65 |  9793 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|     5 |  9794 | `		return rcChk;` |
|     - |  9795 | `	}` |
|    61 |  9796 | `	if( SplDirIs(pCtx->pVm,pThis) && SplDirAtEnd(pThis) ){` |
|     7 |  9797 | `		ph7_result_string(pCtx,"",0);` |
|     7 |  9798 | `		return PH7_OK;` |
|     - |  9799 | `	}` |
|    55 |  9800 | `	zName = SfiName(pCtx->pVm,pThis,&nName);` |
|    55 |  9801 | `	ph7_result_string(pCtx,zName,nName);` |
|    55 |  9802 | `	return PH7_OK;` |
|    33 |  9803 | `}` |
|    50 |  9804 | `static int vm_builtin_SplFileInfo_getFilename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9805 | `{` |
|     - |  9806 | `	sxi32 rcChk;` |
|    51 |  9807 | `	int nTail = 0;` |
|    51 |  9808 | `	const char *zTail = SfiTail(pCtx->pVm,PH7_ContextThis(pCtx),&nTail);` |
|    25 |  9809 | `	SXUNUSED(nArg);` |
|    25 |  9810 | `	SXUNUSED(apArg);` |
|    51 |  9811 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|     5 |  9812 | `		return rcChk;` |
|     - |  9813 | `	}` |
|    47 |  9814 | `	ph7_result_string(pCtx,zTail,nTail);` |
|    47 |  9815 | `	return PH7_OK;` |
|    26 |  9816 | `}` |
|     - |  9817 | `/* php's getBasename(): php_basename() of the tail, suffix rule included. */` |
|    32 |  9818 | `static int vm_builtin_SplFileInfo_getBasename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9819 | `{` |
|     - |  9820 | `	sxi32 rcChk;` |
|    33 |  9821 | `	int nTail = 0,nBase = 0;` |
|    33 |  9822 | `	const char *zTail = SfiTail(pCtx->pVm,PH7_ContextThis(pCtx),&nTail);` |
|    33 |  9823 | `	const char *zBase = PH7_ExtractBaseName(zTail,nTail,&nBase);` |
|    33 |  9824 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 |  9825 | `		return rcChk;` |
|     - |  9826 | `	}` |
|    33 |  9827 | `	if( nArg > 0 ){` |
|     5 |  9828 | `		int nSuffix = 0;` |
|     5 |  9829 | `		const char *zSuffix = ph7_value_to_string(apArg[0],&nSuffix);` |
|     4 |  9830 | `		if( nSuffix > 0 && nSuffix < nBase` |
|     4 |  9831 | `		 && SyMemcmp(&zBase[nBase - nSuffix],zSuffix,(sxu32)nSuffix) == 0 ){` |
|     3 |  9832 | `			nBase -= nSuffix;` |
|     1 |  9833 | `		}` |
|     2 |  9834 | `	}` |
|    33 |  9835 | `	ph7_result_string(pCtx,zBase,nBase);` |
|    33 |  9836 | `	return PH7_OK;` |
|    17 |  9837 | `}` |
|     - |  9838 | `/* php's getExtension(): everything after the LAST dot of the basename, and the` |
|     - |  9839 | ` * empty string when there is none -- a leading dot counts, so '.hidden' has the` |
|     - |  9840 | ` * extension 'hidden'. */` |
|    28 |  9841 | `static int vm_builtin_SplFileInfo_getExtension(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9842 | `{` |
|     - |  9843 | `	sxi32 rcChk;` |
|    29 |  9844 | `	int nTail = 0,nBase = 0,i;` |
|    29 |  9845 | `	const char *zTail = SfiTail(pCtx->pVm,PH7_ContextThis(pCtx),&nTail);` |
|    29 |  9846 | `	const char *zBase = PH7_ExtractBaseName(zTail,nTail,&nBase);` |
|    14 |  9847 | `	SXUNUSED(nArg);` |
|    14 |  9848 | `	SXUNUSED(apArg);` |
|    29 |  9849 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 |  9850 | `		return rcChk;` |
|     - |  9851 | `	}` |
|    89 |  9852 | `	for( i = nBase - 1 ; i >= 0 ; --i ){` |
|    77 |  9853 | `		if( zBase[i] == '.' ){` |
|    17 |  9854 | `			ph7_result_string(pCtx,&zBase[i+1],nBase - i - 1);` |
|    17 |  9855 | `			return PH7_OK;` |
|     - |  9856 | `		}` |
|    31 |  9857 | `	}` |
|    13 |  9858 | `	ph7_result_string(pCtx,"",0);` |
|    13 |  9859 | `	return PH7_OK;` |
|    15 |  9860 | `}` |
|     4 |  9861 | `static int vm_builtin_SplFileInfo_getPerms(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9862 | `{` |
|     2 |  9863 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 |  9864 | `	return SfiStatInt(pCtx,"getPerms","mode");` |
|     1 |  9865 | `}` |
|     4 |  9866 | `static int vm_builtin_SplFileInfo_getInode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9867 | `{` |
|     2 |  9868 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 |  9869 | `	return SfiStatInt(pCtx,"getInode","ino");` |
|     1 |  9870 | `}` |
|    14 |  9871 | `static int vm_builtin_SplFileInfo_getSize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9872 | `{` |
|     7 |  9873 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    15 |  9874 | `	return SfiStatInt(pCtx,"getSize","size");` |
|     1 |  9875 | `}` |
|     4 |  9876 | `static int vm_builtin_SplFileInfo_getOwner(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9877 | `{` |
|     2 |  9878 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 |  9879 | `	return SfiStatInt(pCtx,"getOwner","uid");` |
|     1 |  9880 | `}` |
|     4 |  9881 | `static int vm_builtin_SplFileInfo_getGroup(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9882 | `{` |
|     2 |  9883 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 |  9884 | `	return SfiStatInt(pCtx,"getGroup","gid");` |
|     1 |  9885 | `}` |
|     4 |  9886 | `static int vm_builtin_SplFileInfo_getATime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9887 | `{` |
|     2 |  9888 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 |  9889 | `	return SfiStatInt(pCtx,"getATime","atime");` |
|     1 |  9890 | `}` |
|     4 |  9891 | `static int vm_builtin_SplFileInfo_getMTime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9892 | `{` |
|     2 |  9893 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 |  9894 | `	return SfiStatInt(pCtx,"getMTime","mtime");` |
|     1 |  9895 | `}` |
|     4 |  9896 | `static int vm_builtin_SplFileInfo_getCTime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9897 | `{` |
|     2 |  9898 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 |  9899 | `	return SfiStatInt(pCtx,"getCTime","ctime");` |
|     1 |  9900 | `}` |
|     - |  9901 | `/*` |
|     - |  9902 | ` * php's getType() is FS_TYPE: an LSTAT, so a symlink answers "link" rather than` |
|     - |  9903 | ` * what it points at. The VFS's own xFiletype IS that question -- decoding a stat` |
|     - |  9904 | ` * mode here instead would have answered "unknown" on Windows, where the mode` |
|     - |  9905 | ` * field is not filled and the attributes are what carry the answer.` |
|     - |  9906 | ` */` |
|     8 |  9907 | `static int vm_builtin_SplFileInfo_getType(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9908 | `{` |
|     9 |  9909 | `	const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;` |
|     9 |  9910 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  9911 | `	char zPath[4096];` |
|     9 |  9912 | `	int rc = -1;` |
|     - |  9913 | `	sxi32 rcReady;` |
|     4 |  9914 | `	SXUNUSED(nArg);` |
|     4 |  9915 | `	SXUNUSED(apArg);` |
|     9 |  9916 | `	if( !SfiDirReady(pCtx,&rcReady) ){` |
|     3 |  9917 | `		return rcReady;` |
|     - |  9918 | `	}` |
|     6 |  9919 | `	if( pVfs && pVfs->xFiletype` |
|     7 |  9920 | `	 && SfiPathBuf(pCtx->pVm,pThis,zPath,(int)sizeof(zPath)) == SXRET_OK ){` |
|     7 |  9921 | `		rc = pVfs->xFiletype(zPath,pCtx);` |
|     3 |  9922 | `	}` |
|     7 |  9923 | `	if( rc != PH7_OK ){` |
|     3 |  9924 | `		int nName = 0;` |
|     3 |  9925 | `		const char *zName = SfiName(pCtx->pVm,pThis,&nName);` |
|     3 |  9926 | `		if( pCtx->pRet ){` |
|     3 |  9927 | `			PH7_MemObjRelease(pCtx->pRet);   /* xFiletype wrote "unknown" (rule 54) */` |
|     1 |  9928 | `		}` |
|     4 |  9929 | `		return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     1 |  9930 | `			"SplFileInfo::getType(): Lstat failed for %.*s",nName,zName);` |
|     - |  9931 | `	}` |
|     5 |  9932 | `	return PH7_OK;` |
|     5 |  9933 | `}` |
|     - |  9934 | `/* The six predicates: a VFS question each, and never a diagnostic -- php answers` |
|     - |  9935 | ` * false for a path that does not exist. */` |
|    31 |  9936 | `static int SfiPredicate(ph7_context *pCtx,int (*xTest)(const char *))` |
|     1 |  9937 | `{` |
|     - |  9938 | `	char zPath[4096];` |
|    32 |  9939 | `	int bYes = 0;` |
|     - |  9940 | `	sxi32 rcReady;` |
|    32 |  9941 | `	if( !SfiDirReady(pCtx,&rcReady) ){` |
|     3 |  9942 | `		return rcReady;` |
|     - |  9943 | `	}` |
|    29 |  9944 | `	if( xTest` |
|    30 |  9945 | `	 && SfiPathBuf(pCtx->pVm,PH7_ContextThis(pCtx),zPath,(int)sizeof(zPath)) == SXRET_OK ){` |
|    30 |  9946 | `		bYes = xTest(zPath) == PH7_OK;` |
|    15 |  9947 | `	}` |
|    30 |  9948 | `	ph7_result_bool(pCtx,bYes);` |
|    30 |  9949 | `	return PH7_OK;` |
|    17 |  9950 | `}` |
|     4 |  9951 | `static int vm_builtin_SplFileInfo_isWritable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9952 | `{` |
|     2 |  9953 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 |  9954 | `	return SfiPredicate(pCtx,pCtx->pVm->pEngine->pVfs ? pCtx->pVm->pEngine->pVfs->xWritable : 0);` |
|     1 |  9955 | `}` |
|     4 |  9956 | `static int vm_builtin_SplFileInfo_isReadable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9957 | `{` |
|     2 |  9958 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 |  9959 | `	return SfiPredicate(pCtx,pCtx->pVm->pEngine->pVfs ? pCtx->pVm->pEngine->pVfs->xReadable : 0);` |
|     1 |  9960 | `}` |
|     2 |  9961 | `static int vm_builtin_SplFileInfo_isExecutable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9962 | `{` |
|     1 |  9963 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     3 |  9964 | `	return SfiPredicate(pCtx,pCtx->pVm->pEngine->pVfs ? pCtx->pVm->pEngine->pVfs->xExecutable : 0);` |
|     1 |  9965 | `}` |
|    13 |  9966 | `static int vm_builtin_SplFileInfo_isFile(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9967 | `{` |
|     7 |  9968 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    14 |  9969 | `	return SfiPredicate(pCtx,pCtx->pVm->pEngine->pVfs ? pCtx->pVm->pEngine->pVfs->xIsfile : 0);` |
|     1 |  9970 | `}` |
|     4 |  9971 | `static int vm_builtin_SplFileInfo_isDir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9972 | `{` |
|     2 |  9973 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 |  9974 | `	return SfiPredicate(pCtx,pCtx->pVm->pEngine->pVfs ? pCtx->pVm->pEngine->pVfs->xIsdir : 0);` |
|     1 |  9975 | `}` |
|     4 |  9976 | `static int vm_builtin_SplFileInfo_isLink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9977 | `{` |
|     2 |  9978 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 |  9979 | `	return SfiPredicate(pCtx,pCtx->pVm->pEngine->pVfs ? pCtx->pVm->pEngine->pVfs->xIslink : 0);` |
|     1 |  9980 | `}` |
|     - |  9981 | `/* php's getLinkTarget(): readlink(), and a RuntimeException naming the errno text` |
|     - |  9982 | ` * when it fails -- which includes asking a plain file for its target. */` |
|     2 |  9983 | `static int vm_builtin_SplFileInfo_getLinkTarget(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  9984 | `{` |
|     3 |  9985 | `	const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;` |
|     3 |  9986 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  9987 | `	char zPath[4096];` |
|     3 |  9988 | `	int rc = -1;` |
|     - |  9989 | `	sxi32 rcReady;` |
|     1 |  9990 | `	SXUNUSED(nArg);` |
|     1 |  9991 | `	SXUNUSED(apArg);` |
|     3 |  9992 | `	if( !SfiDirReady(pCtx,&rcReady) ){` |
|   ! 0 |  9993 | `		return rcReady;` |
|     - |  9994 | `	}` |
|     2 |  9995 | `	if( pVfs && pVfs->xReadlink` |
|     3 |  9996 | `	 && SfiPathBuf(pCtx->pVm,pThis,zPath,(int)sizeof(zPath)) == SXRET_OK ){` |
|     3 |  9997 | `		rc = pVfs->xReadlink(zPath,pCtx);` |
|     1 |  9998 | `	}` |
|     3 |  9999 | `	if( rc != PH7_OK ){` |
|     2 | 10000 | `		int nName = 0;` |
|     2 | 10001 | `		const char *zName = SfiName(pCtx->pVm,pThis,&nName);` |
|     3 | 10002 | `		return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     2 | 10003 | `			"Unable to read link %.*s, error: %s",nName,zName,VfsStrerror(errno));` |
|     - | 10004 | `	}` |
|     1 | 10005 | `	return PH7_OK;` |
|     2 | 10006 | `}` |
|     6 | 10007 | `static int vm_builtin_SplFileInfo_getRealPath(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10008 | `{` |
|     - | 10009 | `	sxi32 rcChk;` |
|     7 | 10010 | `	const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;` |
|     - | 10011 | `	char zPath[4096];` |
|     7 | 10012 | `	int rc = -1;` |
|     3 | 10013 | `	SXUNUSED(nArg);` |
|     3 | 10014 | `	SXUNUSED(apArg);` |
|     7 | 10015 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 10016 | `		return rcChk;` |
|     - | 10017 | `	}` |
|     6 | 10018 | `	if( pVfs && pVfs->xRealpath` |
|     7 | 10019 | `	 && SfiPathBuf(pCtx->pVm,PH7_ContextThis(pCtx),zPath,(int)sizeof(zPath)) == SXRET_OK ){` |
|     7 | 10020 | `		rc = pVfs->xRealpath(zPath,pCtx);` |
|     3 | 10021 | `	}` |
|     7 | 10022 | `	if( rc != PH7_OK ){` |
|     5 | 10023 | `		ph7_result_bool(pCtx,0);   /* php answers false, with no diagnostic */` |
|     2 | 10024 | `	}` |
|     7 | 10025 | `	return PH7_OK;` |
|     4 | 10026 | `}` |
|     - | 10027 | `/*` |
|     - | 10028 | ` * The class getFileInfo()/getPathInfo() build with: the argument when it names` |
|     - | 10029 | ` * one, this instance's info_class otherwise. php refuses anything not derived` |
|     - | 10030 | ` * from SplFileInfo, and words the refusal from the ARGUMENT position.` |
|     - | 10031 | ` */` |
|    84 | 10032 | `static sxi32 SfiInfoClass(ph7_context *pCtx,const char *zMethod,ph7_value *pArg,` |
|     - | 10033 | `	ph7_class **ppOut)` |
|     1 | 10034 | `{` |
|    85 | 10035 | `	ph7_vm *pVm = pCtx->pVm;` |
|    85 | 10036 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    85 | 10037 | `	ph7_class *pBase = PH7_VmExtractClass(pVm,"SplFileInfo",sizeof("SplFileInfo")-1,FALSE,0);` |
|    85 | 10038 | `	ph7_class *pClass = 0;` |
|     - | 10039 | `	const char *zName;` |
|    85 | 10040 | `	int nName = 0;` |
|    85 | 10041 | `	if( pArg && (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|     9 | 10042 | `		zName = ph7_value_to_string(pArg,&nName);` |
|     9 | 10043 | `		pClass = PH7_VmExtractClass(pVm,zName,(sxu32)nName,TRUE,0);` |
|     9 | 10044 | `		if( pClass == 0 \|\| pBase == 0 \|\| !PH7_VmInstanceOf(pClass,pBase) ){` |
|     4 | 10045 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 10046 | `				"SplFileInfo::%s(): Argument #1 ($class) must be a class name derived "` |
|     1 | 10047 | `				"from SplFileInfo or null, %.*s given",zMethod,nName,zName);` |
|     - | 10048 | `		}` |
|     4 | 10049 | `	}else{` |
|    77 | 10050 | `		int nCur = 0;` |
|    77 | 10051 | `		const char *zCur = SfiStr(pThis,SFI_IC,&nCur);` |
|    77 | 10052 | `		pClass = PH7_VmExtractClass(pVm,zCur,(sxu32)nCur,TRUE,0);` |
|     - | 10053 | `	}` |
|    83 | 10054 | `	if( pClass == 0 ){` |
|   ! 0 | 10055 | `		pClass = pBase;` |
|   ! 0 | 10056 | `	}` |
|    83 | 10057 | `	*ppOut = pClass;` |
|    83 | 10058 | `	return pClass ? PH7_OK : PH7_ContextMemoryError(pCtx);` |
|    43 | 10059 | `}` |
|     - | 10060 | `/*` |
|     - | 10061 | ` * Build one of these for a path. php calls the CONSTRUCTOR when the class` |
|     - | 10062 | ` * declares its own (a subclass may want it) and fills the slots directly when it` |
|     - | 10063 | ` * does not -- reproduced here, because a subclass constructor is user code and` |
|     - | 10064 | ` * skipping it would be visible.` |
|     - | 10065 | ` */` |
|    74 | 10066 | `static sxi32 SfiMakeInfoEx(ph7_context *pCtx,ph7_class *pClass,const char *zPath,int nPath,` |
|     - | 10067 | `	const char *zDir,int nDir)` |
|     1 | 10068 | `{` |
|    75 | 10069 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 10070 | `	ph7_class_instance *pNew;` |
|     - | 10071 | `	ph7_class_method *pCons;` |
|    75 | 10072 | `	sxi32 rc = SXRET_OK;` |
|    75 | 10073 | `	pNew = PH7_NewClassInstance(pVm,pClass);` |
|    75 | 10074 | `	if( pNew == 0 ){` |
|   ! 0 | 10075 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 10076 | `	}` |
|    75 | 10077 | `	pNew->iRef++;` |
|    75 | 10078 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|    76 | 10079 | `	if( pCons && (pCons->sFunc.iFlags & VM_FUNC_NATIVE) == 0 ){` |
|     - | 10080 | `		ph7_value sArg,*apArg[1];` |
|     3 | 10081 | `		PH7_MemObjInitFromString(pVm,&sArg,0);` |
|     3 | 10082 | `		PH7_MemObjStringAppend(&sArg,zPath,(sxu32)nPath);` |
|     3 | 10083 | `		apArg[0] = &sArg;` |
|     3 | 10084 | `		rc = PH7_VmCallClassMethod(pVm,pNew,pCons,0,1,apArg);` |
|     3 | 10085 | `		PH7_MemObjRelease(&sArg);` |
|    74 | 10086 | `	}else if( zDir ){` |
|     - | 10087 | `		/* php's create_type for a DIR source hands the child BOTH strings rather` |
|     - | 10088 | `		 * than re-deriving the second: the path is the directory being walked, so` |
|     - | 10089 | ``		 * `new DirectoryIterator('/')`'s entry keeps the path `/` and the name`` |
|     - | 10090 | ``		 * `//x` that the walk itself produced. */`` |
|    59 | 10091 | `		PH7_NativeSetAttrStr(pVm,pNew,SFI_N,zPath,nPath);` |
|    59 | 10092 | `		PH7_NativeSetAttrStr(pVm,pNew,SFI_P,zDir,nDir);` |
|    30 | 10093 | `	}else{` |
|    15 | 10094 | `		SfiSetName(pVm,pNew,zPath,nPath);` |
|     - | 10095 | `	}` |
|    75 | 10096 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 10097 | `		PH7_ClassInstanceUnref(pNew);` |
|   ! 0 | 10098 | `		return rc;` |
|     - | 10099 | `	}` |
|    75 | 10100 | `	PH7_NativeResultObject(pCtx,pNew);` |
|    75 | 10101 | `	PH7_ClassInstanceUnref(pNew);` |
|    75 | 10102 | `	return PH7_OK;` |
|    38 | 10103 | `}` |
|     6 | 10104 | `static sxi32 SfiMakeInfo(ph7_context *pCtx,ph7_class *pClass,const char *zPath,int nPath)` |
|     1 | 10105 | `{` |
|     7 | 10106 | `	return SfiMakeInfoEx(pCtx,pClass,zPath,nPath,0,0);` |
|     1 | 10107 | `}` |
|    20 | 10108 | `static int vm_builtin_SplFileInfo_getFileInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10109 | `{` |
|     - | 10110 | `	sxi32 rcChk,rc;` |
|    21 | 10111 | `	ph7_vm *pVm = pCtx->pVm;` |
|    21 | 10112 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    21 | 10113 | `	ph7_class *pClass = 0;` |
|    21 | 10114 | `	int nName = 0,nDir = 0;` |
|    21 | 10115 | `	const char *zName,*zDir = 0;` |
|    21 | 10116 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 10117 | `		return rcChk;` |
|     - | 10118 | `	}` |
|    21 | 10119 | `	rc = SfiInfoClass(pCtx,"getFileInfo",nArg > 0 ? apArg[0] : 0,&pClass);` |
|    21 | 10120 | `	if( rc != PH7_OK ){` |
|     3 | 10121 | `		return rc;` |
|     - | 10122 | `	}` |
|    19 | 10123 | `	if( SplDirIs(pVm,pThis) ){` |
|     9 | 10124 | `		if( SplDirState(pVm,pThis) == 0 ){` |
|     3 | 10125 | `			return PH7_VmThrowException(pCtx,"Error","Object not initialized");` |
|     - | 10126 | `		}` |
|     - | 10127 | `		/* php's create_type refuses to describe an entry that is not there —` |
|     - | 10128 | `		 * the same RuntimeException a FilesystemIterator::current() past the end` |
|     - | 10129 | `		 * raises, because it goes through this. */` |
|     7 | 10130 | `		if( SplDirAtEnd(pThis) ){` |
|     3 | 10131 | `			return PH7_VmThrowException(pCtx,"RuntimeException","Could not open file");` |
|     - | 10132 | `		}` |
|     5 | 10133 | `		zDir = SfiStr(pThis,SFI_P,&nDir);` |
|     2 | 10134 | `	}` |
|    15 | 10135 | `	zName = SfiName(pVm,pThis,&nName);` |
|    15 | 10136 | `	return SfiMakeInfoEx(pCtx,pClass,zName,nName,zDir,nDir);` |
|    11 | 10137 | `}` |
|     - | 10138 | `/* php's getPathInfo(): the DIRNAME of the pathname, and nothing at all (null) for` |
|     - | 10139 | ` * an empty one — which for a directory iterator includes one that has run out. */` |
|    10 | 10140 | `static int vm_builtin_SplFileInfo_getPathInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10141 | `{` |
|     - | 10142 | `	sxi32 rcChk,rc;` |
|    11 | 10143 | `	ph7_vm *pVm = pCtx->pVm;` |
|    11 | 10144 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    11 | 10145 | `	ph7_class *pClass = 0;` |
|    11 | 10146 | `	int nName = 0,nDir = 0;` |
|     - | 10147 | `	const char *zName,*zDir;` |
|    11 | 10148 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 10149 | `		return rcChk;` |
|     - | 10150 | `	}` |
|    11 | 10151 | `	rc = SfiInfoClass(pCtx,"getPathInfo",nArg > 0 ? apArg[0] : 0,&pClass);` |
|    11 | 10152 | `	if( rc != PH7_OK ){` |
|   ! 0 | 10153 | `		return rc;` |
|     - | 10154 | `	}` |
|    11 | 10155 | `	if( SplDirIs(pVm,pThis) && SplDirAtEnd(pThis) ){` |
|     3 | 10156 | `		ph7_result_null(pCtx);` |
|     3 | 10157 | `		return PH7_OK;` |
|     - | 10158 | `	}` |
|     9 | 10159 | `	zName = SfiName(pVm,pThis,&nName);` |
|     9 | 10160 | `	if( nName < 1 ){` |
|     3 | 10161 | `		ph7_result_null(pCtx);` |
|     3 | 10162 | `		return PH7_OK;` |
|     - | 10163 | `	}` |
|     7 | 10164 | `	zDir = PH7_ExtractDirName(zName,nName,&nDir);` |
|     7 | 10165 | `	return SfiMakeInfo(pCtx,pClass,zDir,nDir);` |
|     6 | 10166 | `}` |
|     8 | 10167 | `static int vm_builtin_SplFileInfo_setInfoClass(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10168 | `{` |
|     - | 10169 | `	sxi32 rcChk;` |
|     9 | 10170 | `	ph7_vm *pVm = pCtx->pVm;` |
|     9 | 10171 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     9 | 10172 | `	ph7_class *pBase = PH7_VmExtractClass(pVm,"SplFileInfo",sizeof("SplFileInfo")-1,FALSE,0);` |
|     - | 10173 | `	ph7_class *pClass;` |
|     9 | 10174 | `	const char *zName = "SplFileInfo";` |
|     9 | 10175 | `	int nName = (int)sizeof("SplFileInfo")-1;` |
|     9 | 10176 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 10177 | `		return rcChk;` |
|     - | 10178 | `	}` |
|     9 | 10179 | `	if( nArg > 0 ){` |
|     9 | 10180 | `		zName = ph7_value_to_string(apArg[0],&nName);` |
|     4 | 10181 | `	}` |
|     9 | 10182 | `	pClass = PH7_VmExtractClass(pVm,zName,(sxu32)nName,TRUE,0);` |
|     9 | 10183 | `	if( pClass == 0 \|\| pBase == 0 \|\| !PH7_VmInstanceOf(pClass,pBase) ){` |
|     - | 10184 | `		/* php words this one WITHOUT the "or null" half getFileInfo() has: the` |
|     - | 10185 | `		 * parameter is not nullable here. */` |
|     7 | 10186 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 10187 | `			"SplFileInfo::setInfoClass(): Argument #1 ($class) must be a class name "` |
|     2 | 10188 | `			"derived from SplFileInfo, %.*s given",nName,zName);` |
|     - | 10189 | `	}` |
|     5 | 10190 | `	PH7_NativeSetAttrStr(pVm,pThis,SFI_IC,zName,nName);` |
|     5 | 10191 | `	return PH7_OK;` |
|     5 | 10192 | `}` |
|     - | 10193 | `/*` |
|     - | 10194 | ` * php's setFileClass(): the class openFile() will build. Worded WITHOUT the` |
|     - | 10195 | ` * "or null" half getFileInfo() has, like its info_class twin -- the parameter` |
|     - | 10196 | ` * is not nullable, and a null coerces to the empty name the refusal prints.` |
|     - | 10197 | ` */` |
|    20 | 10198 | `static int vm_builtin_SplFileInfo_setFileClass(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10199 | `{` |
|     - | 10200 | `	sxi32 rcChk;` |
|    21 | 10201 | `	ph7_vm *pVm = pCtx->pVm;` |
|    21 | 10202 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    21 | 10203 | `	ph7_class *pBase = PH7_VmExtractClass(pVm,"SplFileObject",sizeof("SplFileObject")-1,FALSE,0);` |
|     - | 10204 | `	ph7_class *pClass;` |
|    21 | 10205 | `	const char *zName = "SplFileObject";` |
|    21 | 10206 | `	int nName = (int)sizeof("SplFileObject")-1;` |
|    21 | 10207 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 10208 | `		return rcChk;` |
|     - | 10209 | `	}` |
|    21 | 10210 | `	if( nArg > 0 ){` |
|    19 | 10211 | `		zName = ph7_value_to_string(apArg[0],&nName);` |
|     9 | 10212 | `	}` |
|    21 | 10213 | `	pClass = PH7_VmExtractClass(pVm,zName,(sxu32)nName,TRUE,0);` |
|    21 | 10214 | `	if( pClass == 0 \|\| pBase == 0 \|\| !PH7_VmInstanceOf(pClass,pBase) ){` |
|    10 | 10215 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 10216 | `			"SplFileInfo::setFileClass(): Argument #1 ($class) must be a class name "` |
|     3 | 10217 | `			"derived from SplFileObject, %.*s given",nName,zName);` |
|     - | 10218 | `	}` |
|    15 | 10219 | `	PH7_NativeSetAttrStr(pVm,pThis,SFI_FC,zName,nName);` |
|    15 | 10220 | `	return PH7_OK;` |
|    11 | 10221 | `}` |
|     - | 10222 | `/*` |
|     - | 10223 | ` * php's openFile(): spl_filesystem_object_create_type for SPL_FS_FILE. The` |
|     - | 10224 | ` * class is this instance's file_class, and php CALLS its constructor when the` |
|     - | 10225 | ` * class declares one of its own -- with the pathname and the MODE, two` |
|     - | 10226 | ` * arguments, which is how a subclass gets to see what it was opened as. When it` |
|     - | 10227 | ` * does not, php fills the slots and opens directly, which is what SfoOpen()` |
|     - | 10228 | ` * does here; the warning that open would print is promoted to a` |
|     - | 10229 | ` * RuntimeException worded from THIS method's name.` |
|     - | 10230 | ` */` |
|    30 | 10231 | `static int vm_builtin_SplFileInfo_openFile(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10232 | `{` |
|     - | 10233 | `	sxi32 rcChk,rc;` |
|    31 | 10234 | `	ph7_vm *pVm = pCtx->pVm;` |
|    31 | 10235 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 10236 | `	ph7_class_instance *pNew;` |
|     - | 10237 | `	ph7_class_method *pCons;` |
|     - | 10238 | `	ph7_class *pClass;` |
|     - | 10239 | `	ph7_value sPath;` |
|     - | 10240 | `	SyBlob sCls,sDir;` |
|    31 | 10241 | `	const char *zMode = "r",*zName,*zDir,*zCls;` |
|    31 | 10242 | `	int nMode = 1,nName = 0,nDir = 0,nCls = 0;` |
|    31 | 10243 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 10244 | `		return rcChk;` |
|     - | 10245 | `	}` |
|    31 | 10246 | `	if( SplDirIs(pVm,pThis) ){` |
|     5 | 10247 | `		if( SplDirState(pVm,pThis) == 0 ){` |
|   ! 0 | 10248 | `			return PH7_VmThrowException(pCtx,"Error","Object not initialized");` |
|     - | 10249 | `		}` |
|     - | 10250 | `		/* php's create_type refuses to describe an entry that is not there. */` |
|     5 | 10251 | `		if( SplDirAtEnd(pThis) ){` |
|     3 | 10252 | `			return PH7_VmThrowException(pCtx,"RuntimeException","Could not open file");` |
|     - | 10253 | `		}` |
|     1 | 10254 | `	}` |
|     - | 10255 | `	/* Both strings are copied OUT before anything allocates or runs user code:` |
|     - | 10256 | `	 * they are slots of the object being read, and a subclass constructor below` |
|     - | 10257 | `	 * can rewrite or unset either one (rule 22). */` |
|    29 | 10258 | `	zCls = SfiStr(pThis,SFI_FC,&nCls);` |
|    29 | 10259 | `	SyBlobInit(&sCls,&pVm->sAllocator);` |
|    29 | 10260 | `	SyBlobAppend(&sCls,zCls,(sxu32)nCls);` |
|    29 | 10261 | `	zDir = SfiStr(pThis,SFI_P,&nDir);` |
|    29 | 10262 | `	SyBlobInit(&sDir,&pVm->sAllocator);` |
|    29 | 10263 | `	SyBlobAppend(&sDir,zDir,(sxu32)nDir);` |
|    43 | 10264 | `	pClass = PH7_VmExtractClass(pVm,(const char *)SyBlobData(&sCls),` |
|    14 | 10265 | `		SyBlobLength(&sCls),TRUE,0);` |
|    29 | 10266 | `	if( pClass == 0 ){` |
|   ! 0 | 10267 | `		pClass = PH7_VmExtractClass(pVm,"SplFileObject",sizeof("SplFileObject")-1,FALSE,0);` |
|   ! 0 | 10268 | `	}` |
|    29 | 10269 | `	if( pClass == 0 ){` |
|   ! 0 | 10270 | `		SyBlobRelease(&sCls);` |
|   ! 0 | 10271 | `		SyBlobRelease(&sDir);` |
|   ! 0 | 10272 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 10273 | `	}` |
|    29 | 10274 | `	if( nArg > 0 ){` |
|    11 | 10275 | `		zMode = ph7_value_to_string(apArg[0],&nMode);` |
|     5 | 10276 | `	}` |
|     - | 10277 | `	/* The path is handed on as a VALUE: the opener needs one, and the slot it` |
|     - | 10278 | `	 * would otherwise borrow belongs to an object about to be written to. */` |
|    29 | 10279 | `	zName = SfiName(pVm,pThis,&nName);` |
|    29 | 10280 | `	PH7_MemObjInitFromString(pVm,&sPath,0);` |
|    29 | 10281 | `	PH7_MemObjStringAppend(&sPath,zName,(sxu32)nName);` |
|    29 | 10282 | `	pNew = PH7_NewClassInstance(pVm,pClass);` |
|    29 | 10283 | `	if( pNew == 0 ){` |
|   ! 0 | 10284 | `		PH7_MemObjRelease(&sPath);` |
|   ! 0 | 10285 | `		SyBlobRelease(&sCls);` |
|   ! 0 | 10286 | `		SyBlobRelease(&sDir);` |
|   ! 0 | 10287 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 10288 | `	}` |
|    29 | 10289 | `	pNew->iRef++;` |
|    29 | 10290 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|    30 | 10291 | `	if( pCons && (pCons->sFunc.iFlags & VM_FUNC_NATIVE) == 0 ){` |
|     - | 10292 | `		ph7_value sMode,*apCtor[2];` |
|     3 | 10293 | `		PH7_MemObjInitFromString(pVm,&sMode,0);` |
|     3 | 10294 | `		PH7_MemObjStringAppend(&sMode,zMode,(sxu32)nMode);` |
|     3 | 10295 | `		apCtor[0] = &sPath;` |
|     3 | 10296 | `		apCtor[1] = &sMode;` |
|     3 | 10297 | `		rc = PH7_VmCallClassMethod(pVm,pNew,pCons,0,2,apCtor);` |
|     3 | 10298 | `		PH7_MemObjRelease(&sMode);` |
|     2 | 10299 | `	}else{` |
|    44 | 10300 | `		rc = SfoOpen(pCtx,pNew,&sPath,zMode,nMode,` |
|    15 | 10301 | `			nArg > 1 ? ph7_value_to_bool(apArg[1]) : FALSE,` |
|    15 | 10302 | `			nArg > 2 && (apArg[2]->iFlags & MEMOBJ_NULL) == 0 ? apArg[2] : 0,3);` |
|    27 | 10303 | `		if( rc == PH7_OK ){` |
|     - | 10304 | `			/* php hands the child the SOURCE's path rather than re-deriving one` |
|     - | 10305 | `			 * from the name: a directory entry's getPath() keeps pointing at the` |
|     - | 10306 | ``			 * directory being walked, and a `php://temp` source keeps the EMPTY`` |
|     - | 10307 | ``			 * path a URI's last slash would otherwise cut to `php:/`. */`` |
|    31 | 10308 | `			PH7_NativeSetAttrStr(pVm,pNew,SFI_P,` |
|    20 | 10309 | `				(const char *)SyBlobData(&sDir),(int)SyBlobLength(&sDir));` |
|    10 | 10310 | `		}` |
|     - | 10311 | `	}` |
|    29 | 10312 | `	PH7_MemObjRelease(&sPath);` |
|    29 | 10313 | `	SyBlobRelease(&sCls);` |
|    29 | 10314 | `	SyBlobRelease(&sDir);` |
|    29 | 10315 | `	if( rc != PH7_OK ){` |
|     7 | 10316 | `		PH7_ClassInstanceUnref(pNew);` |
|     7 | 10317 | `		return rc;` |
|     - | 10318 | `	}` |
|    23 | 10319 | `	PH7_NativeResultObject(pCtx,pNew);` |
|    23 | 10320 | `	PH7_ClassInstanceUnref(pNew);` |
|    23 | 10321 | `	return PH7_OK;` |
|    16 | 10322 | `}` |
|     - | 10323 | ``/* One `"\0Class\0member" => <string>` entry of a debug array. */`` |
|    72 | 10324 | `static void SfiDebugStr(ph7_vm *pVm,ph7_value *pOut,const char *zKey,int nKey,` |
|     - | 10325 | `	const char *zVal,int nVal)` |
|     1 | 10326 | `{` |
|     - | 10327 | `	ph7_value sKey,sVal;` |
|    73 | 10328 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|    73 | 10329 | `	PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKey);` |
|    73 | 10330 | `	PH7_MemObjInitFromString(pVm,&sVal,0);` |
|    73 | 10331 | `	PH7_MemObjStringAppend(&sVal,zVal,(sxu32)nVal);` |
|    73 | 10332 | `	ph7_array_add_elem(pOut,&sKey,&sVal);` |
|    73 | 10333 | `	PH7_MemObjRelease(&sKey);` |
|    73 | 10334 | `	PH7_MemObjRelease(&sVal);` |
|    73 | 10335 | `}` |
|     - | 10336 | `/*` |
|     - | 10337 | ` * php's get_debug_info: the two slots under their MANGLED private names, which is` |
|     - | 10338 | ` * how a class with no declared properties still shows something. __debugInfo()` |
|     - | 10339 | ` * hands back the same array.` |
|     - | 10340 | ` *` |
|     - | 10341 | `` * A DIRECTORY iterator shows two more (`glob`, always false here — PHL has no`` |
|     - | 10342 | `` * GlobIterator — and `subPathName`), and shows `fileName` only if the pathname`` |
|     - | 10343 | `` * has been MATERIALIZED: php's `if (intern->file_name)` is the lazy name's`` |
|     - | 10344 | ` * presence, so an exhausted iterator has one key fewer until something asks it` |
|     - | 10345 | ` * for a path.` |
|     - | 10346 | ` */` |
|    24 | 10347 | `static sxi32 SfiFillDebug(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)` |
|     1 | 10348 | `{` |
|     - | 10349 | ``	/* php's test is `type == SPL_FS_DIR`, which an object whose constructor never`` |
|     - | 10350 | ``	 * ran does NOT satisfy: it shows the single `pathName` key a bare SplFileInfo`` |
|     - | 10351 | `	 * would, and neither of the two directory ones. */` |
|    25 | 10352 | `	int bDir = SplDirIs(pVm,pThis) && SplDirState(pVm,pThis) != 0;` |
|    25 | 10353 | `	int nName = 0,nTail = 0,nSub = 0;` |
|     - | 10354 | `	const char *zName;` |
|    25 | 10355 | `	int bLive = bDir ? !SplDirAtEnd(pThis) : !SplDirIs(pVm,pThis);` |
|    25 | 10356 | `	if( bLive ){` |
|    19 | 10357 | `		zName = SfiName(pVm,pThis,&nName);` |
|    10 | 10358 | `	}else{` |
|     7 | 10359 | `		zName = SfiStr(pThis,SFI_N,&nName);   /* whatever a stat left behind, or "" */` |
|     7 | 10360 | `		nName = 0;` |
|     - | 10361 | `	}` |
|    37 | 10362 | `	SfiDebugStr(pVm,pOut,"\0SplFileInfo\0pathName",` |
|    12 | 10363 | `		(int)sizeof("\0SplFileInfo\0pathName")-1,zName,nName);` |
|     - | 10364 | `	/* Re-read: the append above may have moved the slot the first read borrowed. */` |
|    25 | 10365 | `	SfiStr(pThis,SFI_N,&nName);` |
|    25 | 10366 | `	if( bLive \|\| (bDir && nName > 0) ){` |
|    19 | 10367 | `		const char *zTail = SfiTail(pVm,pThis,&nTail);` |
|    28 | 10368 | `		SfiDebugStr(pVm,pOut,"\0SplFileInfo\0fileName",` |
|     9 | 10369 | `			(int)sizeof("\0SplFileInfo\0fileName")-1,zTail,nTail);` |
|     9 | 10370 | `	}` |
|    25 | 10371 | `	if( bDir ){` |
|     - | 10372 | `		ph7_value sKey,sVal;` |
|     - | 10373 | `		const char *zSub;` |
|    13 | 10374 | `		int nSlot = 0;` |
|    13 | 10375 | `		const char *zSlot = SfiStr(pThis,SFI_P,&nSlot);` |
|    13 | 10376 | `		PH7_MemObjInitFromString(pVm,&sKey,0);` |
|    13 | 10377 | `		PH7_MemObjStringAppend(&sKey,"\0DirectoryIterator\0glob",` |
|     - | 10378 | `			sizeof("\0DirectoryIterator\0glob")-1);` |
|     - | 10379 | `		/* php's own test, on the slot rather than on the stream: the whole` |
|     - | 10380 | ``		 * `glob://pattern` when the path carries that prefix, and FALSE for an`` |
|     - | 10381 | `		 * ordinary directory. A GlobIterator's constructor puts the prefix on` |
|     - | 10382 | `		 * whether or not the caller wrote it, so this is always the pattern. */` |
|    12 | 10383 | `		if( nSlot >= (int)sizeof("glob://")-1` |
|    13 | 10384 | `		 && SyMemcmp(zSlot,"glob://",sizeof("glob://")-1) == 0 ){` |
|     7 | 10385 | `			PH7_MemObjInitFromString(pVm,&sVal,0);` |
|     7 | 10386 | `			PH7_MemObjStringAppend(&sVal,zSlot,(sxu32)nSlot);` |
|     4 | 10387 | `		}else{` |
|     7 | 10388 | `			PH7_MemObjInitFromBool(pVm,&sVal,0);` |
|     - | 10389 | `		}` |
|    13 | 10390 | `		ph7_array_add_elem(pOut,&sKey,&sVal);` |
|    13 | 10391 | `		PH7_MemObjRelease(&sKey);` |
|    13 | 10392 | `		PH7_MemObjRelease(&sVal);` |
|    13 | 10393 | `		zSub = SfiStr(pThis,SDI_S,&nSub);` |
|    19 | 10394 | `		SfiDebugStr(pVm,pOut,"\0RecursiveDirectoryIterator\0subPathName",` |
|     6 | 10395 | `			(int)sizeof("\0RecursiveDirectoryIterator\0subPathName")-1,zSub,nSub);` |
|     6 | 10396 | `	}` |
|    25 | 10397 | `	if( SfoDev(pThis) ){` |
|     - | 10398 | ``		/* php's `type == SPL_FS_FILE` arm: the open mode and the two CSV`` |
|     - | 10399 | `		 * characters, which is the only place any of the three is visible. An` |
|     - | 10400 | `		 * SplFileObject whose constructor never ran is still SPL_FS_INFO and` |
|     - | 10401 | `		 * shows none of them. */` |
|     7 | 10402 | `		int nMode = 0,c;` |
|     7 | 10403 | `		const char *zMode = SfiStr(pThis,SFO_M,&nMode);` |
|    10 | 10404 | `		SfiDebugStr(pVm,pOut,"\0SplFileObject\0openMode",` |
|     3 | 10405 | `			(int)sizeof("\0SplFileObject\0openMode")-1,zMode,nMode);` |
|     7 | 10406 | `		c = (int)PH7_NativeAttrInt(pThis,SFO_D);` |
|     - | 10407 | `		{` |
|     7 | 10408 | `			char zChar = (char)c;` |
|     7 | 10409 | `			SfiDebugStr(pVm,pOut,"\0SplFileObject\0delimiter",` |
|     - | 10410 | `				(int)sizeof("\0SplFileObject\0delimiter")-1,&zChar,1);` |
|     7 | 10411 | `			zChar = (char)PH7_NativeAttrInt(pThis,SFO_EN);` |
|     7 | 10412 | `			SfiDebugStr(pVm,pOut,"\0SplFileObject\0enclosure",` |
|     - | 10413 | `				(int)sizeof("\0SplFileObject\0enclosure")-1,&zChar,1);` |
|     - | 10414 | `		}` |
|     3 | 10415 | `	}` |
|    25 | 10416 | `	return PH7_OK;` |
|     1 | 10417 | `}` |
|    12 | 10418 | `static sxi32 SfiPresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|     1 | 10419 | `{` |
|    13 | 10420 | `	if( !bDebug ){` |
|     7 | 10421 | `		return PH7_OK;   /* php's (array) cast and var_export show nothing */` |
|     - | 10422 | `	}` |
|     7 | 10423 | `	return SfiFillDebug(pVm,pThis,pOut);` |
|     7 | 10424 | `}` |
|    18 | 10425 | `static int vm_builtin_SplFileInfo_debugInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10426 | `{` |
|     - | 10427 | `	sxi32 rcChk;` |
|    19 | 10428 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 10429 | `	ph7_value sOut;` |
|     9 | 10430 | `	SXUNUSED(nArg);` |
|     9 | 10431 | `	SXUNUSED(apArg);` |
|    19 | 10432 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 10433 | `		return rcChk;` |
|     - | 10434 | `	}` |
|    19 | 10435 | `	PH7_MemObjInit(pVm,&sOut);` |
|    19 | 10436 | `	if( PH7_MemObjToHashmap(&sOut) != SXRET_OK ){` |
|   ! 0 | 10437 | `		PH7_MemObjRelease(&sOut);` |
|   ! 0 | 10438 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 10439 | `	}` |
|    19 | 10440 | `	SfiFillDebug(pVm,PH7_ContextThis(pCtx),&sOut);` |
|    19 | 10441 | `	ph7_result_value(pCtx,&sOut);` |
|    19 | 10442 | `	PH7_MemObjRelease(&sOut);` |
|    19 | 10443 | `	return PH7_OK;` |
|    10 | 10444 | `}` |
|     - | 10445 | `/* php's own escape hatch for a subclass that forgot to call parent::__construct.` |
|     - | 10446 | ` * It exists to be THROWN, and php marks it deprecated rather than removing it. */` |
|   ! 0 | 10447 | `static int vm_builtin_SplFileInfo_badState(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 10448 | `{` |
|   ! 0 | 10449 | `	SXUNUSED(nArg);` |
|   ! 0 | 10450 | `	SXUNUSED(apArg);` |
|   ! 0 | 10451 | `	return PH7_VmThrowException(pCtx,"Error",` |
|     - | 10452 | `		"The parent constructor was not called: the object is in an invalid state");` |
|   ! 0 | 10453 | `}` |
|     - | 10454 | `/*` |
|     - | 10455 | ` * The declaration. Method ORDER is spl_directory.stub.php's; openFile() and` |
|     - | 10456 | ` * setFileClass() are absent because SplFileObject is (§7), and everything else is` |
|     - | 10457 | ` * php's, tentative return types included.` |
|     - | 10458 | ` */` |
|  5740 | 10459 | `static sxi32 VmInstallSplFileInfo(ph7_vm *pVm)` |
|     5 | 10460 | `{` |
|     - | 10461 | `	static const PH7_NativePropDef aSfiProp[] = {` |
|     - | 10462 | `		{ SFI_N,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|     - | 10463 | `		{ SFI_P,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|     - | 10464 | `		{ SFI_IC, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN,` |
|     - | 10465 | `		  { 0, 0, PH7_NATIVE_VAL_STRING, 0, "SplFileInfo", 0.0 }, 0 },` |
|     - | 10466 | `		{ SFI_FC, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN,` |
|     - | 10467 | `		  { 0, 0, PH7_NATIVE_VAL_STRING, 0, "SplFileObject", 0.0 }, 0 },` |
|     - | 10468 | `	};` |
|     - | 10469 | `	static const PH7_NativeMethodDef aSfiMethod[] = {` |
|     - | 10470 | `		{ "__construct",   PH7_MOD_PUBLIC, "string $filename", 0,` |
|     - | 10471 | `		  vm_builtin_SplFileInfo_construct },` |
|     - | 10472 | `		{ "getPath",       PH7_MOD_PUBLIC, "", "@string", vm_builtin_SplFileInfo_getPath },` |
|     - | 10473 | `		{ "getFilename",   PH7_MOD_PUBLIC, "", "@string", vm_builtin_SplFileInfo_getFilename },` |
|     - | 10474 | `		{ "getExtension",  PH7_MOD_PUBLIC, "", "@string", vm_builtin_SplFileInfo_getExtension },` |
|     - | 10475 | `		{ "getBasename",   PH7_MOD_PUBLIC, "string $suffix = \"\"", "@string",` |
|     - | 10476 | `		  vm_builtin_SplFileInfo_getBasename },` |
|     - | 10477 | `		{ "getPathname",   PH7_MOD_PUBLIC, "", "@string", vm_builtin_SplFileInfo_getPathname },` |
|     - | 10478 | `		{ "getPerms",      PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_SplFileInfo_getPerms },` |
|     - | 10479 | `		{ "getInode",      PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_SplFileInfo_getInode },` |
|     - | 10480 | `		{ "getSize",       PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_SplFileInfo_getSize },` |
|     - | 10481 | `		{ "getOwner",      PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_SplFileInfo_getOwner },` |
|     - | 10482 | `		{ "getGroup",      PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_SplFileInfo_getGroup },` |
|     - | 10483 | `		{ "getATime",      PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_SplFileInfo_getATime },` |
|     - | 10484 | `		{ "getMTime",      PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_SplFileInfo_getMTime },` |
|     - | 10485 | `		{ "getCTime",      PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_SplFileInfo_getCTime },` |
|     - | 10486 | `		{ "getType",       PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_SplFileInfo_getType },` |
|     - | 10487 | `		{ "isWritable",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplFileInfo_isWritable },` |
|     - | 10488 | `		{ "isReadable",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplFileInfo_isReadable },` |
|     - | 10489 | `		{ "isExecutable",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplFileInfo_isExecutable },` |
|     - | 10490 | `		{ "isFile",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplFileInfo_isFile },` |
|     - | 10491 | `		{ "isDir",         PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplFileInfo_isDir },` |
|     - | 10492 | `		{ "isLink",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplFileInfo_isLink },` |
|     - | 10493 | `		{ "getLinkTarget", PH7_MOD_PUBLIC, "", "@string\|false",` |
|     - | 10494 | `		  vm_builtin_SplFileInfo_getLinkTarget },` |
|     - | 10495 | `		{ "getRealPath",   PH7_MOD_PUBLIC, "", "@string\|false",` |
|     - | 10496 | `		  vm_builtin_SplFileInfo_getRealPath },` |
|     - | 10497 | `		{ "getFileInfo",   PH7_MOD_PUBLIC, "?string $class = null", "@SplFileInfo",` |
|     - | 10498 | `		  vm_builtin_SplFileInfo_getFileInfo },` |
|     - | 10499 | `		{ "getPathInfo",   PH7_MOD_PUBLIC, "?string $class = null", "@?SplFileInfo",` |
|     - | 10500 | `		  vm_builtin_SplFileInfo_getPathInfo },` |
|     - | 10501 | `		/* spl_directory.stub.php's order, which is what every method-enumeration` |
|     - | 10502 | `		 * surface reports: openFile, setFileClass, setInfoClass. */` |
|     - | 10503 | `		{ "openFile",      PH7_MOD_PUBLIC,` |
|     - | 10504 | `		  "string $mode = \"r\", bool $useIncludePath = false, $context = null",` |
|     - | 10505 | `		  "@SplFileObject", vm_builtin_SplFileInfo_openFile },` |
|     - | 10506 | `		{ "setFileClass",  PH7_MOD_PUBLIC, "~string $class = SplFileObject::class", "@void",` |
|     - | 10507 | `		  vm_builtin_SplFileInfo_setFileClass },` |
|     - | 10508 | `		{ "setInfoClass",  PH7_MOD_PUBLIC, "~string $class = SplFileInfo::class", "@void",` |
|     - | 10509 | `		  vm_builtin_SplFileInfo_setInfoClass },` |
|     - | 10510 | `		{ "__toString",    PH7_MOD_PUBLIC, "", "string", vm_builtin_SplFileInfo_getPathname },` |
|     - | 10511 | `		{ "__debugInfo",   PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplFileInfo_debugInfo },` |
|     - | 10512 | `		/* php does NOT mark this one tentative -- it is the only method here that` |
|     - | 10513 | ``		 * prints `Return [ void ]` rather than `Tentative return [ void ]`. */`` |
|     - | 10514 | `		{ "_bad_state_ex", PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "void",` |
|     - | 10515 | `		  vm_builtin_SplFileInfo_badState },` |
|     - | 10516 | `	};` |
|     - | 10517 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - | 10518 | `		{ "SplFileInfo", 0, "Stringable", PH7_CLASS_NOSERIALIZE,` |
|     - | 10519 | `		  aSfiMethod, SX_ARRAYSIZE(aSfiMethod), 0, 0,` |
|     - | 10520 | `		  aSfiProp, SX_ARRAYSIZE(aSfiProp), 0, 0, SfiPresent },` |
|     - | 10521 | `	};` |
|  5745 | 10522 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 | 10523 | `}` |
|     - | 10524 | `/*` |
|     - | 10525 | ` * ---------------------------------------------------------------------------` |
|     - | 10526 | ` * DirectoryIterator, FilesystemIterator and RecursiveDirectoryIterator.` |
|     - | 10527 | ` *` |
|     - | 10528 | `` * php's `spl_filesystem_object` holds an OPEN directory stream and ONE entry at`` |
|     - | 10529 | `` * a time (`u.dir.dirp`, `u.dir.entry`, `u.dir.index`); the chunk read the whole`` |
|     - | 10530 | ` * directory into an array at construction, and every difference followed from` |
|     - | 10531 | `` * that one choice (rule 52). php's `rewind()` re-opens the directory and SEES A`` |
|     - | 10532 | `` * FILE CREATED SINCE, its `key()` is the read index rather than an array offset,`` |
|     - | 10533 | `` * its `seek()` walks FORWARD through the object's own valid()/next() — so a`` |
|     - | 10534 | `` * subclass overriding either is obeyed — and a `clone` opens the directory again`` |
|     - | 10535 | ` * and reads forward to the same index rather than sharing a cursor.` |
|     - | 10536 | ` *` |
|     - | 10537 | ` * The handle cannot live in a property slot, because CLONE copies slots: two` |
|     - | 10538 | ` * objects would share one directory stream and close it twice. It lives in` |
|     - | 10539 | `` * `pVm->hDirHandle` keyed by the instance, with the class's xRelease closing it,`` |
|     - | 10540 | ` * and a clone — finding no entry of its own — re-opens on first use, which IS` |
|     - | 10541 | ` * php's clone handler, deferred. The one thing that deferral costs is a clone` |
|     - | 10542 | ` * whose directory is removed before it is first used: php has the stream open` |
|     - | 10543 | ` * already and answers, PHL raises "Object not initialized" (§7).` |
|     - | 10544 | ` *` |
|     - | 10545 | `` * `file_name` is LAZY here as it is in php: the path, a slash and the current`` |
|     - | 10546 | ` * entry, invalidated by every read and rebuilt on demand. That is php-visible` |
|     - | 10547 | `` * twice over -- `getPathname()` answers "" past the end while `getSize()` stats`` |
|     - | 10548 | `` * the DIRECTORY (the join with an empty entry), and `var_dump` shows one key`` |
|     - | 10549 | ` * fewer until something has asked.` |
|     - | 10550 | ` *` |
|     - | 10551 | `` * The chunk had also INVENTED `DirectoryIterator::getFlags()` (php has no such`` |
|     - | 10552 | ``  * method; only FilesystemIterator does), inherited SplFileInfo's `__toString()` `` |
|     - | 10553 | ` * where php aliases getFilename(), and mis-stated two constants:` |
|     - | 10554 | ` * FOLLOW_SYMLINKS is 16384 (it said 512, colliding with nothing but reading as` |
|     - | 10555 | ` * false for every real flags value) and OTHER_MODE_MASK is 28672.` |
|     - | 10556 | ` * ---------------------------------------------------------------------------` |
|     - | 10557 | ` */` |
|     - | 10558 | `/* php's spl_directory.h flag set, verbatim -- the values the class constants` |
|     - | 10559 | ` * publish and the masks its accessors compare with. */` |
|     - | 10560 | `#define SDI_CURRENT_AS_FILEINFO 0x0000` |
|     - | 10561 | `#define SDI_CURRENT_AS_SELF     0x0010` |
|     - | 10562 | `#define SDI_CURRENT_AS_PATHNAME 0x0020` |
|     - | 10563 | `#define SDI_CURRENT_MODE_MASK   0x00F0` |
|     - | 10564 | `#define SDI_KEY_AS_PATHNAME     0x0000` |
|     - | 10565 | `#define SDI_KEY_AS_FILENAME     0x0100` |
|     - | 10566 | `#define SDI_KEY_MODE_MASK       0x0F00` |
|     - | 10567 | `#define SDI_SKIPDOTS            0x1000` |
|     - | 10568 | `#define SDI_UNIXPATHS           0x2000` |
|     - | 10569 | `#define SDI_FOLLOW_SYMLINKS     0x4000` |
|     - | 10570 | `#define SDI_OTHERS_MASK         0x7000` |
|     - | 10571 | `#define SDI_FLAGS_MASK (SDI_KEY_MODE_MASK\|SDI_CURRENT_MODE_MASK\|SDI_OTHERS_MASK)` |
|     - | 10572 |  |
|     - | 10573 | `/* php's DEFAULT_SLASH, and the UNIX_PATHS flag that overrides it. */` |
|    91 | 10574 | `static char SplDirSlash(sxi64 iFlags)` |
|     1 | 10575 | `{` |
|     - | 10576 | `#ifdef __WINNT__` |
|     1 | 10577 | `	return (iFlags & SDI_UNIXPATHS) ? '/' : '\\';` |
|     - | 10578 | `#else` |
|    46 | 10579 | `	SXUNUSED(iFlags);` |
|    91 | 10580 | `	return '/';` |
|     - | 10581 | `#endif` |
|     1 | 10582 | `}` |
|     - | 10583 | `/* php's spl_filesystem_is_dot. */` |
|   198 | 10584 | `static int SplDirIsDot(const char *zName,int nName)` |
|     1 | 10585 | `{` |
|   204 | 10586 | `	return (nName == 1 && zName[0] == '.')` |
|   224 | 10587 | `		\|\| (nName == 2 && zName[0] == '.' && zName[1] == '.');` |
|     1 | 10588 | `}` |
|     - | 10589 | ``/* Does this instance carry php's `u.dir` arm? Asked by the five SplFileInfo`` |
|     - | 10590 | ` * bodies that branch on the object TYPE, so it has to be the class question and` |
|     - | 10591 | ` * not "does it have a __e slot" — a user class may declare anything. */` |
|   694 | 10592 | `static int SplDirIs(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 10593 | `{` |
|     - | 10594 | `	ph7_class *pDir;` |
|   695 | 10595 | `	if( pThis == 0 ){` |
|   ! 0 | 10596 | `		return 0;` |
|     - | 10597 | `	}` |
|   695 | 10598 | `	pDir = PH7_VmExtractClass(pVm,"DirectoryIterator",sizeof("DirectoryIterator")-1,FALSE,0);` |
|   695 | 10599 | `	return pDir && PH7_VmInstanceOf(pThis->pClass,pDir);` |
|   349 | 10600 | `}` |
|     - | 10601 | ``/* php's `!intern->u.dir.entry.d_name[0]`: the walk has nothing to describe. */`` |
|   240 | 10602 | `static int SplDirAtEnd(ph7_class_instance *pThis)` |
|     1 | 10603 | `{` |
|   241 | 10604 | `	int nEntry = 0;` |
|   241 | 10605 | `	SfiStr(pThis,SDI_E,&nEntry);` |
|   241 | 10606 | `	return nEntry < 1;` |
|     1 | 10607 | `}` |
|     - | 10608 | `/* The registry entry for this instance, or 0. */` |
|  1078 | 10609 | `static VmDirHandle * SplDirFind(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 10610 | `{` |
|     - | 10611 | `	SyHashEntry *pEntry;` |
|  1079 | 10612 | `	if( pThis == 0 \|\| SyHashTotalEntry(&pVm->hDirHandle) < 1 ){` |
|     9 | 10613 | `		return 0;` |
|     - | 10614 | `	}` |
|  1071 | 10615 | `	pEntry = SyHashGet(&pVm->hDirHandle,(const void *)&pThis,sizeof(void *));` |
|  1071 | 10616 | `	return pEntry ? (VmDirHandle *)pEntry->pUserData : 0;` |
|   544 | 10617 | `}` |
|     - | 10618 | `/* Is this a GlobIterator? The CLASS question, asked where there may be no` |
|     - | 10619 | ` * handle to ask -- an instance whose parent constructor never ran has one. */` |
|   166 | 10620 | `static int SplGlobIs(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 10621 | `{` |
|     - | 10622 | `	ph7_class *pGlob;` |
|   167 | 10623 | `	if( pThis == 0 ){` |
|   ! 0 | 10624 | `		return 0;` |
|     - | 10625 | `	}` |
|   167 | 10626 | `	pGlob = PH7_VmExtractClass(pVm,"GlobIterator",sizeof("GlobIterator")-1,FALSE,0);` |
|   167 | 10627 | `	return pGlob && PH7_VmInstanceOf(pThis->pClass,pGlob) ? 1 : 0;` |
|    84 | 10628 | `}` |
|     - | 10629 | `/*` |
|     - | 10630 | ` * php's spl_filesystem_object_get_path for a GLOB handle: the directory of the` |
|     - | 10631 | ` * match the last read handed out, which the STREAM tracks and the object does` |
|     - | 10632 | `` * not. It moves with the walk -- `glob://a/` + `*` + `/` + `*.txt` reports `a/sub1`, then`` |
|     - | 10633 | `` * `a/sub2` -- it is the EMPTY string for a match with no slash in it, and it is`` |
|     - | 10634 | ` * cleared when the walk runs out, which is what makes getPathname() answer ""` |
|     - | 10635 | ` * past the end.` |
|     - | 10636 | ` *` |
|     - | 10637 | ` * Answers 0 for any other handle, whose path is the slot the constructor wrote.` |
|     - | 10638 | ` * Asked through SplDirFind() rather than SplDirState(): a handle that is not` |
|     - | 10639 | ` * open has no current match to have a directory OF, and GlobIterator is` |
|     - | 10640 | ` * uncloneable, so the re-open SplDirState() exists for cannot arise here.` |
|     - | 10641 | ` */` |
|   325 | 10642 | `static const char * SplDirGlobPath(ph7_vm *pVm,ph7_class_instance *pThis,int *pnLen)` |
|     1 | 10643 | `{` |
|   326 | 10644 | `	VmDirHandle *pH = SplDirFind(pVm,pThis);` |
|   326 | 10645 | `	*pnLen = 0;` |
|   326 | 10646 | `	if( pH == 0 \|\| pH->pStream == 0 \|\| !PH7_GlobStreamIs(pH->pStream) ){` |
|   266 | 10647 | `		return 0;` |
|     - | 10648 | `	}` |
|    61 | 10649 | `	return PH7_GlobStreamPath(pH->pHandle,pnLen);` |
|   164 | 10650 | `}` |
|     - | 10651 | `/* Close the handle this instance owns, if any. The class's xRelease, and the` |
|     - | 10652 | ` * first half of a re-open. */` |
|    72 | 10653 | `static void SplDirClose(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 10654 | `{` |
|    73 | 10655 | `	void *pData = 0;` |
|    72 | 10656 | `	if( SyHashDeleteEntry(&pVm->hDirHandle,(const void *)&pThis,sizeof(void *),&pData) == SXRET_OK` |
|    68 | 10657 | `	 && pData ){` |
|    63 | 10658 | `		VmDirHandle *pH = (VmDirHandle *)pData;` |
|    63 | 10659 | `		if( pH->pStream && pH->pStream->xCloseDir ){` |
|    63 | 10660 | `			pH->pStream->xCloseDir(pH->pHandle);` |
|    31 | 10661 | `		}` |
|    63 | 10662 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|    31 | 10663 | `	}` |
|    73 | 10664 | `}` |
|     - | 10665 | `/*` |
|     - | 10666 | ` * Close every DIR the program still held at VM shutdown. xRelease (SplDirClose` |
|     - | 10667 | ` * above) covers an instance the program DESTROYED; an iterator alive at script` |
|     - | 10668 | ` * end reaches PH7_VmRelease with its handle still open, and the OS stream` |
|     - | 10669 | ` * behind it lives outside SyMemBackend -- the wholesale release frees the` |
|     - | 10670 | ` * VmDirHandle record and leaks the DIR (the leak checker is what noticed:` |
|     - | 10671 | ` * glibc's opendir buffer, ~32KB per survivor). Called from PH7_VmRelease` |
|     - | 10672 | ` * before the backend goes; the records themselves are backend memory.` |
|     - | 10673 | ` */` |
|  4958 | 10674 | `PH7_PRIVATE void PH7_SplDirVmRelease(ph7_vm *pVm)` |
|     5 | 10675 | `{` |
|     - | 10676 | `	SyHashEntry *pEntry;` |
|  4963 | 10677 | `	SyHashResetLoopCursor(&pVm->hDirHandle);` |
|  4989 | 10678 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hDirHandle)) != 0 ){` |
|    27 | 10679 | `		VmDirHandle *pH = (VmDirHandle *)pEntry->pUserData;` |
|    27 | 10680 | `		if( pH && pH->pStream && pH->pStream->xCloseDir ){` |
|    27 | 10681 | `			pH->pStream->xCloseDir(pH->pHandle);` |
|    13 | 10682 | `		}` |
|     1 | 10683 | `	}` |
|  4963 | 10684 | `}` |
|     - | 10685 | `/*` |
|     - | 10686 | ` * php's spl_filesystem_dir_read: invalidate the lazy name, then take ONE entry` |
|     - | 10687 | ` * from the stream; running out leaves the entry empty, which is what valid()` |
|     - | 10688 | ` * reports. The read goes through a scratch call context because the VFS reports` |
|     - | 10689 | ` * a name by writing a RESULT -- borrowing the method's own return slot would` |
|     - | 10690 | ` * append to whatever the body is about to answer (rule 54).` |
|     - | 10691 | ` */` |
|   334 | 10692 | `static void SplDirRead(ph7_vm *pVm,ph7_class_instance *pThis,VmDirHandle *pH)` |
|     1 | 10693 | `{` |
|     - | 10694 | `	ph7_context sCtx;` |
|     - | 10695 | `	ph7_value sOut;` |
|   335 | 10696 | `	int rc = -1;` |
|   335 | 10697 | `	PH7_NativeSetAttrStr(pVm,pThis,SFI_N,"",0);` |
|   335 | 10698 | `	PH7_MemObjInit(pVm,&sOut);` |
|   335 | 10699 | `	VmInitCallContext(&sCtx,pVm,0,&sOut,0);` |
|   335 | 10700 | `	if( pH && pH->pStream && pH->pStream->xReadDir ){` |
|   335 | 10701 | `		rc = pH->pStream->xReadDir(pH->pHandle,&sCtx);` |
|   171 | 10702 | `	}` |
|   335 | 10703 | `	if( rc == PH7_OK ){` |
|   297 | 10704 | `		int nName = 0;` |
|   297 | 10705 | `		const char *zName = ph7_value_to_string(&sOut,&nName);` |
|   297 | 10706 | `		PH7_NativeSetAttrStr(pVm,pThis,SDI_E,zName,nName);` |
|   153 | 10707 | `	}else{` |
|    39 | 10708 | `		PH7_NativeSetAttrStr(pVm,pThis,SDI_E,"",0);` |
|     - | 10709 | `	}` |
|   335 | 10710 | `	VmReleaseCallContext(&sCtx);` |
|   335 | 10711 | `	PH7_MemObjRelease(&sOut);` |
|   335 | 10712 | `}` |
|     - | 10713 | `/* php's read loop: one entry, then more while SKIP_DOTS and this is a dot. */` |
|   232 | 10714 | `static void SplDirReadSkip(ph7_vm *pVm,ph7_class_instance *pThis,VmDirHandle *pH)` |
|     1 | 10715 | `{` |
|   233 | 10716 | `	sxi64 iFlags = PH7_NativeAttrInt(pThis,SDI_F);` |
|   211 | 10717 | `	for(;;){` |
|   327 | 10718 | `		int nEntry = 0;` |
|     - | 10719 | `		const char *zEntry;` |
|   327 | 10720 | `		SplDirRead(pVm,pThis,pH);` |
|   327 | 10721 | `		if( (iFlags & SDI_SKIPDOTS) == 0 ){` |
|   188 | 10722 | `			return;` |
|     - | 10723 | `		}` |
|   185 | 10724 | `		zEntry = SfiStr(pThis,SDI_E,&nEntry);` |
|   185 | 10725 | `		if( !SplDirIsDot(zEntry,nEntry) ){` |
|    91 | 10726 | `			return;` |
|     - | 10727 | `		}` |
|     1 | 10728 | `	}` |
|   118 | 10729 | `}` |
|     - | 10730 | `/*` |
|     - | 10731 | ` * php's spl_filesystem_dir_open: open the directory, remember it under the path` |
|     - | 10732 | ` * MINUS one trailing slash, and read the first entry. Answers 0 when the open` |
|     - | 10733 | ` * failed, having still written the path (php sets it either way, so a caught` |
|     - | 10734 | ` * constructor failure leaves the same shape behind).` |
|     - | 10735 | ` */` |
|    90 | 10736 | `static VmDirHandle * SplDirOpen(ph7_vm *pVm,ph7_class_instance *pThis,` |
|     - | 10737 | `	const char *zPath,int nPath)` |
|     1 | 10738 | `{` |
|     - | 10739 | `	const ph7_io_stream *pStream;` |
|     - | 10740 | `	const char *zDevice;` |
|     - | 10741 | `	VmDirHandle *pH;` |
|     - | 10742 | `	char zBuf[4096];` |
|    91 | 10743 | `	void *pHandle = 0;` |
|    91 | 10744 | `	int nKeep = nPath;` |
|    91 | 10745 | `	if( nPath < 1 \|\| nPath >= (int)sizeof(zBuf) ){` |
|   ! 0 | 10746 | `		return 0;` |
|     - | 10747 | `	}` |
|    91 | 10748 | `	SyMemcpy(zPath,zBuf,(sxu32)nPath);` |
|    91 | 10749 | `	zBuf[nPath] = 0;` |
|    91 | 10750 | `	zDevice = zBuf;` |
|    91 | 10751 | `	pStream = PH7_VmGetStreamDevice(pVm,&zDevice,nPath);` |
|    91 | 10752 | `	if( nKeep > 1 && SFI_IS_SLASH(zPath[nKeep-1]) ){` |
|     3 | 10753 | `		nKeep--;` |
|     1 | 10754 | `	}` |
|    91 | 10755 | `	PH7_NativeSetAttrStr(pVm,pThis,SFI_P,zPath,nKeep);` |
|    91 | 10756 | `	PH7_NativeSetAttrInt(pVm,pThis,SDI_I,0);` |
|    91 | 10757 | `	PH7_NativeSetAttrStr(pVm,pThis,SDI_E,"",0);` |
|    91 | 10758 | `	PH7_NativeSetAttrStr(pVm,pThis,SFI_N,"",0);` |
|    91 | 10759 | `	if( pStream == 0 \|\| pStream->xOpenDir == 0 ){` |
|   ! 0 | 10760 | `		return 0;` |
|     - | 10761 | `	}` |
|     - | 10762 | `	{` |
|     - | 10763 | `		/* The device takes the VM through this argument (see opendir). */` |
|     - | 10764 | `		ph7_value sDummy;` |
|     - | 10765 | `		int rc;` |
|    91 | 10766 | `		PH7_MemObjInit(pVm,&sDummy);` |
|    91 | 10767 | `		rc = pStream->xOpenDir(zDevice,&sDummy,&pHandle);` |
|    91 | 10768 | `		PH7_MemObjRelease(&sDummy);` |
|    91 | 10769 | `		if( rc != PH7_OK ){` |
|     3 | 10770 | `			return 0;` |
|     - | 10771 | `		}` |
|     - | 10772 | `	}` |
|    89 | 10773 | `	pH = (VmDirHandle *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmDirHandle));` |
|    89 | 10774 | `	if( pH == 0 ){` |
|   ! 0 | 10775 | `		if( pStream->xCloseDir ){` |
|   ! 0 | 10776 | `			pStream->xCloseDir(pHandle);` |
|   ! 0 | 10777 | `		}` |
|   ! 0 | 10778 | `		return 0;` |
|     - | 10779 | `	}` |
|    89 | 10780 | `	pH->pStream = pStream;` |
|    89 | 10781 | `	pH->pHandle = pHandle;` |
|    89 | 10782 | `	pH->pThis = pThis;` |
|     - | 10783 | `	/* SyHashInsert BORROWS the key bytes: key off the record's own field, which` |
|     - | 10784 | `	 * lives exactly as long as the entry does (rule 22). */` |
|    89 | 10785 | `	if( SyHashInsert(&pVm->hDirHandle,(const void *)&pH->pThis,sizeof(void *),pH) != SXRET_OK ){` |
|   ! 0 | 10786 | `		if( pStream->xCloseDir ){` |
|   ! 0 | 10787 | `			pStream->xCloseDir(pHandle);` |
|   ! 0 | 10788 | `		}` |
|   ! 0 | 10789 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|   ! 0 | 10790 | `		return 0;` |
|     - | 10791 | `	}` |
|    89 | 10792 | `	return pH;` |
|    46 | 10793 | `}` |
|     - | 10794 | `/*` |
|     - | 10795 | ` * The open handle behind this instance, RE-OPENING it for a fresh clone.` |
|     - | 10796 | ` *` |
|     - | 10797 | ` * php's clone handler opens the directory again and reads forward to the` |
|     - | 10798 | ` * source's index, because a directory stream cannot be duplicated; PHL does the` |
|     - | 10799 | ` * same work on first use instead, which is what keeps the handle out of every` |
|     - | 10800 | ``  * php-visible surface — a property slot carrying it would make `$a == clone $a` `` |
|     - | 10801 | ` * false, and php says true.` |
|     - | 10802 | ` */` |
|   563 | 10803 | `static VmDirHandle * SplDirState(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 10804 | `{` |
|   564 | 10805 | `	VmDirHandle *pH = SplDirFind(pVm,pThis);` |
|     - | 10806 | `	sxi64 iIndex;` |
|   564 | 10807 | `	int nPath = 0;` |
|     - | 10808 | `	const char *zPath;` |
|     - | 10809 | `	SyBlob sPath;` |
|   564 | 10810 | `	if( pH ){` |
|   538 | 10811 | `		return pH;` |
|     - | 10812 | `	}` |
|    27 | 10813 | `	zPath = SfiStr(pThis,SFI_P,&nPath);` |
|    27 | 10814 | `	if( nPath < 1 ){` |
|    25 | 10815 | `		return 0;   /* never constructed: php's "Object not initialized" */` |
|     - | 10816 | `	}` |
|     - | 10817 | `	/* The path slot is about to be rewritten by the open, so copy it out first. */` |
|     3 | 10818 | `	SyBlobInit(&sPath,&pVm->sAllocator);` |
|     3 | 10819 | `	SyBlobAppend(&sPath,zPath,(sxu32)nPath);` |
|     3 | 10820 | `	iIndex = PH7_NativeAttrInt(pThis,SDI_I);` |
|     3 | 10821 | `	pH = SplDirOpen(pVm,pThis,(const char *)SyBlobData(&sPath),(int)SyBlobLength(&sPath));` |
|     3 | 10822 | `	SyBlobRelease(&sPath);` |
|     3 | 10823 | `	if( pH == 0 ){` |
|   ! 0 | 10824 | `		return 0;` |
|     - | 10825 | `	}` |
|     3 | 10826 | `	SplDirReadSkip(pVm,pThis,pH);` |
|     - | 10827 | `	{` |
|     3 | 10828 | `		sxi64 iAt = iIndex;` |
|     5 | 10829 | `		while( iAt-- > 0 ){` |
|     3 | 10830 | `			SplDirReadSkip(pVm,pThis,pH);` |
|     1 | 10831 | `		}` |
|     - | 10832 | `	}` |
|     - | 10833 | `	/* The open above reset the index; the clone stands where the source stood. */` |
|     3 | 10834 | `	PH7_NativeSetAttrInt(pVm,pThis,SDI_I,iIndex);` |
|     3 | 10835 | `	return pH;` |
|   286 | 10836 | `}` |
|     - | 10837 | `/*` |
|     - | 10838 | ` * php's spl_filesystem_object_get_file_name for a DIR: the path, a slash and the` |
|     - | 10839 | ` * current entry, cached until the next read drops it. Called through SfiName(),` |
|     - | 10840 | ` * so every SplFileInfo accessor sees the same lazy value php's do.` |
|     - | 10841 | ` */` |
|   139 | 10842 | `static const char * SplDirName(ph7_vm *pVm,ph7_class_instance *pThis,int *pnLen)` |
|     1 | 10843 | `{` |
|   140 | 10844 | `	int nName = 0,nPath = 0,nEntry = 0;` |
|   140 | 10845 | `	const char *zName = SfiStr(pThis,SFI_N,&nName);` |
|     - | 10846 | `	const char *zPath,*zEntry;` |
|     - | 10847 | `	SyBlob sName;` |
|   140 | 10848 | `	if( nName > 0 ){` |
|    49 | 10849 | `		*pnLen = nName;` |
|    49 | 10850 | `		return zName;` |
|     - | 10851 | `	}` |
|     - | 10852 | `	/* A glob handle's path is the current match's directory, not the slot --` |
|     - | 10853 | ``	 * the slot holds the whole `glob://pattern`, which is not a directory at`` |
|     - | 10854 | `	 * all. php's join then has a branch PHL never needed: when the path is` |
|     - | 10855 | `	 * EMPTY the name is the entry ALONE, which is what makes` |
|     - | 10856 | ``	 * `new GlobIterator('d')` answer `d` for getPathname() rather than `/d`. */`` |
|    92 | 10857 | `	zPath = SplDirGlobPath(pVm,pThis,&nPath);` |
|    92 | 10858 | `	if( zPath == 0 ){` |
|    62 | 10859 | `		zPath = SfiStr(pThis,SFI_P,&nPath);` |
|    62 | 10860 | `		if( nPath < 1 ){` |
|   ! 0 | 10861 | `			*pnLen = 0;` |
|   ! 0 | 10862 | `			return "";` |
|     - | 10863 | `		}` |
|    31 | 10864 | `	}` |
|    92 | 10865 | `	SyBlobInit(&sName,&pVm->sAllocator);` |
|    92 | 10866 | `	if( nPath > 0 ){` |
|    90 | 10867 | `		char cSlash = SplDirSlash(PH7_NativeAttrInt(pThis,SDI_F));` |
|    90 | 10868 | `		SyBlobAppend(&sName,zPath,(sxu32)nPath);` |
|    90 | 10869 | `		SyBlobAppend(&sName,(const void *)&cSlash,sizeof(char));` |
|    45 | 10870 | `	}` |
|    92 | 10871 | `	zEntry = SfiStr(pThis,SDI_E,&nEntry);` |
|    92 | 10872 | `	SyBlobAppend(&sName,zEntry,(sxu32)nEntry);` |
|   138 | 10873 | `	PH7_NativeSetAttrStr(pVm,pThis,SFI_N,` |
|    91 | 10874 | `		(const char *)SyBlobData(&sName),(int)SyBlobLength(&sName));` |
|    92 | 10875 | `	SyBlobRelease(&sName);` |
|    92 | 10876 | `	return SfiStr(pThis,SFI_N,pnLen);` |
|    71 | 10877 | `}` |
|     - | 10878 | `/* php's CHECK_DIRECTORY_ITERATOR_IS_INITIALIZED: every DirectoryIterator method` |
|     - | 10879 | ` * refuses an object whose parent constructor never ran. */` |
|   390 | 10880 | `static VmDirHandle * SplDirChecked(ph7_context *pCtx,sxi32 *pRc)` |
|     1 | 10881 | `{` |
|   391 | 10882 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 10883 | `	VmDirHandle *pH;` |
|     - | 10884 | ``	/* php's `check` object handlers run BEFORE the method does, so a`` |
|     - | 10885 | `	 * GlobIterator whose parent constructor never ran refuses with THAT` |
|     - | 10886 | `	 * sentence rather than this one -- and refuses methods this check would` |
|     - | 10887 | `	 * have let through. A class without those handlers passes straight by. */` |
|   391 | 10888 | `	if( !SfoChecked(pCtx,pRc) ){` |
|     7 | 10889 | `		return 0;` |
|     - | 10890 | `	}` |
|   385 | 10891 | `	pH = SplDirState(pCtx->pVm,pThis);` |
|   385 | 10892 | `	if( pH == 0 ){` |
|    15 | 10893 | `		*pRc = PH7_VmThrowException(pCtx,"Error","Object not initialized");` |
|     7 | 10894 | `	}` |
|   385 | 10895 | `	return pH;` |
|   199 | 10896 | `}` |
|     - | 10897 | `/*` |
|     - | 10898 | ` * The shared constructor: php's spl_filesystem_object_construct, whose two` |
|     - | 10899 | ` * refusals are a ValueError for an empty path and an UnexpectedValueException` |
|     - | 10900 | ` * carrying the OPEN's own errno text (php promotes the opendir warning, so the` |
|     - | 10901 | ` * message is the warning's, prefixed with the constructor that raised it).` |
|     - | 10902 | ` */` |
|    94 | 10903 | `static int SplDirConstructVal(ph7_context *pCtx,const char *zClass,const char *zArg,` |
|     - | 10904 | `	ph7_value *pPath,sxi64 iFlags)` |
|     1 | 10905 | `{` |
|    95 | 10906 | `	ph7_vm *pVm = pCtx->pVm;` |
|    95 | 10907 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 10908 | `	const char *zPath;` |
|    95 | 10909 | `	int nPath = 0;` |
|    95 | 10910 | `	if( pThis == 0 ){` |
|   ! 0 | 10911 | `		return PH7_OK;` |
|     - | 10912 | `	}` |
|    95 | 10913 | `	zPath = ph7_value_to_string(pPath,&nPath);` |
|    95 | 10914 | `	if( nPath < 1 ){` |
|     7 | 10915 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     2 | 10916 | `			"%s::__construct(): Argument #1 ($%s) must not be empty",zClass,zArg);` |
|     - | 10917 | `	}` |
|    91 | 10918 | `	if( SplDirFind(pVm,pThis) ){` |
|     3 | 10919 | `		return PH7_VmThrowException(pCtx,"Error","Directory object is already initialized");` |
|     - | 10920 | `	}` |
|    89 | 10921 | `	PH7_NativeSetAttrInt(pVm,pThis,SDI_F,iFlags);` |
|    89 | 10922 | `	if( SplDirOpen(pVm,pThis,zPath,nPath) == 0 ){` |
|     4 | 10923 | `		return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     1 | 10924 | `			"%s::__construct(%.*s): Failed to open directory: %s",zClass,nPath,zPath,` |
|     2 | 10925 | `			VfsStrerror(errno));` |
|     - | 10926 | `	}` |
|    87 | 10927 | `	SplDirReadSkip(pVm,pThis,SplDirFind(pVm,pThis));` |
|    87 | 10928 | `	return PH7_OK;` |
|    48 | 10929 | `}` |
|     - | 10930 | `/* The three directory classes take their path straight from the argument; only` |
|     - | 10931 | ` * GlobIterator rewrites it first, which is why the open takes a VALUE. */` |
|    66 | 10932 | `static int SplDirConstruct(ph7_context *pCtx,const char *zClass,int nArg,ph7_value **apArg,` |
|     - | 10933 | `	sxi64 iFlags)` |
|     1 | 10934 | `{` |
|    67 | 10935 | `	if( nArg < 1 ){` |
|   ! 0 | 10936 | `		return PH7_OK;` |
|     - | 10937 | `	}` |
|    67 | 10938 | `	return SplDirConstructVal(pCtx,zClass,"directory",apArg[0],iFlags);` |
|    34 | 10939 | `}` |
|     - | 10940 | `/* DirectoryIterator::__construct(string $directory) — php's flags for this one` |
|     - | 10941 | ` * are KEY_AS_PATHNAME\|CURRENT_AS_SELF, and it takes no flags argument. */` |
|    34 | 10942 | `static int vm_builtin_DirectoryIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10943 | `{` |
|    35 | 10944 | `	return SplDirConstruct(pCtx,"DirectoryIterator",nArg,apArg,` |
|     - | 10945 | `		SDI_KEY_AS_PATHNAME\|SDI_CURRENT_AS_SELF);` |
|     1 | 10946 | `}` |
|     - | 10947 | `/* The flags argument the two subclasses share: php's ZPP overwrites the whole` |
|     - | 10948 | ` * default when one is given, so SKIP_DOTS is NOT implied by passing flags. */` |
|    60 | 10949 | `static sxi64 SplDirFlagArg(int nArg,ph7_value **apArg,sxi64 iDefault)` |
|     1 | 10950 | `{` |
|    61 | 10951 | `	return nArg > 1 ? ph7_value_to_int64(apArg[1]) : iDefault;` |
|     1 | 10952 | `}` |
|    22 | 10953 | `static int vm_builtin_FilesystemIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10954 | `{` |
|    34 | 10955 | `	return SplDirConstruct(pCtx,"FilesystemIterator",nArg,apArg,` |
|    11 | 10956 | `		SplDirFlagArg(nArg,apArg,SDI_KEY_AS_PATHNAME\|SDI_CURRENT_AS_FILEINFO\|SDI_SKIPDOTS));` |
|     1 | 10957 | `}` |
|    10 | 10958 | `static int vm_builtin_RecursiveDirectoryIterator_construct(ph7_context *pCtx,int nArg,` |
|     - | 10959 | `	ph7_value **apArg)` |
|     1 | 10960 | `{` |
|    16 | 10961 | `	return SplDirConstruct(pCtx,"RecursiveDirectoryIterator",nArg,apArg,` |
|     5 | 10962 | `		SplDirFlagArg(nArg,apArg,SDI_KEY_AS_PATHNAME\|SDI_CURRENT_AS_FILEINFO));` |
|     1 | 10963 | `}` |
|     - | 10964 | `/* DirectoryIterator::rewind(): php re-opens nothing — it rewinds the STREAM and` |
|     - | 10965 | ` * takes one entry, with no dot skipping at this level. */` |
|    10 | 10966 | `static int vm_builtin_DirectoryIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10967 | `{` |
|     - | 10968 | `	sxi32 rc;` |
|    11 | 10969 | `	VmDirHandle *pH = SplDirChecked(pCtx,&rc);` |
|     5 | 10970 | `	SXUNUSED(nArg);` |
|     5 | 10971 | `	SXUNUSED(apArg);` |
|    11 | 10972 | `	if( pH == 0 ){` |
|     3 | 10973 | `		return rc;` |
|     - | 10974 | `	}` |
|     9 | 10975 | `	PH7_NativeSetAttrInt(pCtx->pVm,PH7_ContextThis(pCtx),SDI_I,0);` |
|     9 | 10976 | `	if( pH->pStream->xRewindDir ){` |
|     9 | 10977 | `		pH->pStream->xRewindDir(pH->pHandle);` |
|     4 | 10978 | `	}` |
|     9 | 10979 | `	SplDirRead(pCtx->pVm,PH7_ContextThis(pCtx),pH);` |
|     9 | 10980 | `	return PH7_OK;` |
|     6 | 10981 | `}` |
|     - | 10982 | `/* FilesystemIterator::rewind(): the same, plus the dot skipping, and php does` |
|     - | 10983 | ` * NOT check the handle here (an uninitialized object simply rewinds to nothing). */` |
|    36 | 10984 | `static int vm_builtin_FilesystemIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 10985 | `{` |
|    37 | 10986 | `	ph7_vm *pVm = pCtx->pVm;` |
|    37 | 10987 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 10988 | `	VmDirHandle *pH;` |
|     - | 10989 | `	sxi32 rcChk;` |
|    18 | 10990 | `	SXUNUSED(nArg);` |
|    18 | 10991 | `	SXUNUSED(apArg);` |
|    37 | 10992 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|     3 | 10993 | `		return rcChk;` |
|     - | 10994 | `	}` |
|    35 | 10995 | `	pH = SplDirState(pVm,pThis);` |
|    35 | 10996 | `	PH7_NativeSetAttrInt(pVm,pThis,SDI_I,0);` |
|    35 | 10997 | `	if( pH && pH->pStream->xRewindDir ){` |
|    35 | 10998 | `		pH->pStream->xRewindDir(pH->pHandle);` |
|    17 | 10999 | `	}` |
|    35 | 11000 | `	SplDirReadSkip(pVm,pThis,pH);` |
|    35 | 11001 | `	return PH7_OK;` |
|    19 | 11002 | `}` |
|   110 | 11003 | `static int vm_builtin_DirectoryIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11004 | `{` |
|   111 | 11005 | `	ph7_vm *pVm = pCtx->pVm;` |
|   111 | 11006 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 11007 | `	sxi32 rc;` |
|   111 | 11008 | `	VmDirHandle *pH = SplDirChecked(pCtx,&rc);` |
|    56 | 11009 | `	SXUNUSED(nArg);` |
|    56 | 11010 | `	SXUNUSED(apArg);` |
|   111 | 11011 | `	if( pH == 0 ){` |
|     3 | 11012 | `		return rc;` |
|     - | 11013 | `	}` |
|     - | 11014 | `	/* php advances the index PAST the end too, which is why key() keeps counting` |
|     - | 11015 | `	 * once valid() is false. */` |
|   109 | 11016 | `	PH7_NativeSetAttrInt(pVm,pThis,SDI_I,PH7_NativeAttrInt(pThis,SDI_I) + 1);` |
|   109 | 11017 | `	SplDirReadSkip(pVm,pThis,pH);` |
|   109 | 11018 | `	return PH7_OK;` |
|    57 | 11019 | `}` |
|   158 | 11020 | `static int vm_builtin_DirectoryIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11021 | `{` |
|     - | 11022 | `	sxi32 rc;` |
|   159 | 11023 | `	VmDirHandle *pH = SplDirChecked(pCtx,&rc);` |
|    80 | 11024 | `	SXUNUSED(nArg);` |
|    80 | 11025 | `	SXUNUSED(apArg);` |
|   159 | 11026 | `	if( pH == 0 ){` |
|     5 | 11027 | `		return rc;` |
|     - | 11028 | `	}` |
|   155 | 11029 | `	ph7_result_bool(pCtx,!SplDirAtEnd(PH7_ContextThis(pCtx)));` |
|   155 | 11030 | `	return PH7_OK;` |
|    81 | 11031 | `}` |
|    26 | 11032 | `static int vm_builtin_DirectoryIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11033 | `{` |
|     - | 11034 | `	sxi32 rc;` |
|    27 | 11035 | `	VmDirHandle *pH = SplDirChecked(pCtx,&rc);` |
|    13 | 11036 | `	SXUNUSED(nArg);` |
|    13 | 11037 | `	SXUNUSED(apArg);` |
|    27 | 11038 | `	if( pH == 0 ){` |
|     3 | 11039 | `		return rc;` |
|     - | 11040 | `	}` |
|    25 | 11041 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(PH7_ContextThis(pCtx),SDI_I));` |
|    25 | 11042 | `	return PH7_OK;` |
|    14 | 11043 | `}` |
|    15 | 11044 | `static int vm_builtin_DirectoryIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11045 | `{` |
|     - | 11046 | `	sxi32 rc;` |
|    16 | 11047 | `	VmDirHandle *pH = SplDirChecked(pCtx,&rc);` |
|     8 | 11048 | `	SXUNUSED(nArg);` |
|     8 | 11049 | `	SXUNUSED(apArg);` |
|    16 | 11050 | `	if( pH == 0 ){` |
|     3 | 11051 | `		return rc;` |
|     - | 11052 | `	}` |
|    14 | 11053 | `	SplResultBorrowed(pCtx,PH7_ContextThis(pCtx));` |
|    14 | 11054 | `	return PH7_OK;` |
|     9 | 11055 | `}` |
|    14 | 11056 | `static int vm_builtin_DirectoryIterator_isDot(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11057 | `{` |
|    15 | 11058 | `	int nEntry = 0;` |
|     - | 11059 | `	const char *zEntry;` |
|     - | 11060 | `	sxi32 rc;` |
|    15 | 11061 | `	VmDirHandle *pH = SplDirChecked(pCtx,&rc);` |
|     7 | 11062 | `	SXUNUSED(nArg);` |
|     7 | 11063 | `	SXUNUSED(apArg);` |
|    15 | 11064 | `	if( pH == 0 ){` |
|     5 | 11065 | `		return rc;` |
|     - | 11066 | `	}` |
|    11 | 11067 | `	zEntry = SfiStr(PH7_ContextThis(pCtx),SDI_E,&nEntry);` |
|    11 | 11068 | `	ph7_result_bool(pCtx,SplDirIsDot(zEntry,nEntry));` |
|    11 | 11069 | `	return PH7_OK;` |
|     8 | 11070 | `}` |
|     - | 11071 | `/*` |
|     - | 11072 | ` * php's seek(): rewind if the target is behind us, then walk forward through` |
|     - | 11073 | ` * the OBJECT's own valid()/next() — a subclass overriding either is obeyed, and` |
|     - | 11074 | ` * running out raises php's OutOfBoundsException with the iterator left standing` |
|     - | 11075 | ` * where the walk stopped.` |
|     - | 11076 | ` */` |
|    10 | 11077 | `static int vm_builtin_DirectoryIterator_seek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11078 | `{` |
|    11 | 11079 | `	ph7_vm *pVm = pCtx->pVm;` |
|    11 | 11080 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 11081 | `	ph7_class_method *pMethod;` |
|     - | 11082 | `	sxi64 iPos;` |
|     - | 11083 | `	sxi32 rc;` |
|    11 | 11084 | `	VmDirHandle *pH = SplDirChecked(pCtx,&rc);` |
|    11 | 11085 | `	if( pH == 0 ){` |
|   ! 0 | 11086 | `		return rc;` |
|     - | 11087 | `	}` |
|    11 | 11088 | `	iPos = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|    11 | 11089 | `	if( PH7_NativeAttrInt(pThis,SDI_I) > iPos ){` |
|     5 | 11090 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|     5 | 11091 | `		if( pMethod ){` |
|     5 | 11092 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,0,0,0);` |
|     5 | 11093 | `			if( rc != SXRET_OK ){` |
|   ! 0 | 11094 | `				return rc;` |
|     - | 11095 | `			}` |
|     2 | 11096 | `		}` |
|     2 | 11097 | `	}` |
|    27 | 11098 | `	while( PH7_NativeAttrInt(pThis,SDI_I) < iPos ){` |
|     - | 11099 | `		ph7_value sRet;` |
|     - | 11100 | `		int bValid;` |
|    19 | 11101 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|    19 | 11102 | `		if( pMethod == 0 ){` |
|   ! 0 | 11103 | `			break;` |
|     - | 11104 | `		}` |
|    19 | 11105 | `		PH7_MemObjInit(pVm,&sRet);` |
|    19 | 11106 | `		rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sRet,0,0);` |
|    19 | 11107 | `		bValid = rc == SXRET_OK && ph7_value_to_bool(&sRet);` |
|    19 | 11108 | `		PH7_MemObjRelease(&sRet);` |
|    19 | 11109 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 11110 | `			return rc;` |
|     - | 11111 | `		}` |
|    19 | 11112 | `		if( !bValid ){` |
|     4 | 11113 | `			return PH7_VmThrowException(pCtx,"OutOfBoundsException",` |
|     1 | 11114 | `				"Seek position %qd is out of range",iPos);` |
|     - | 11115 | `		}` |
|    17 | 11116 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|    17 | 11117 | `		if( pMethod == 0 ){` |
|   ! 0 | 11118 | `			break;` |
|     - | 11119 | `		}` |
|    17 | 11120 | `		rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,0,0,0);` |
|    17 | 11121 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 11122 | `			return rc;` |
|     - | 11123 | `		}` |
|     1 | 11124 | `	}` |
|     9 | 11125 | `	return PH7_OK;` |
|     6 | 11126 | `}` |
|     - | 11127 | `/* DirectoryIterator's three name accessors read the ENTRY, not the pathname —` |
|     - | 11128 | `` * which is why `getFilename()` answers `..` where SplFileInfo's would answer the`` |
|     - | 11129 | `` * whole path, and why `__toString()` is aliased to this one rather than to`` |
|     - | 11130 | ` * getPathname(). */` |
|    43 | 11131 | `static int vm_builtin_DirectoryIterator_getFilename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11132 | `{` |
|    44 | 11133 | `	int nEntry = 0;` |
|     - | 11134 | `	const char *zEntry;` |
|     - | 11135 | `	sxi32 rc;` |
|    44 | 11136 | `	VmDirHandle *pH = SplDirChecked(pCtx,&rc);` |
|    22 | 11137 | `	SXUNUSED(nArg);` |
|    22 | 11138 | `	SXUNUSED(apArg);` |
|    44 | 11139 | `	if( pH == 0 ){` |
|     5 | 11140 | `		return rc;` |
|     - | 11141 | `	}` |
|    40 | 11142 | `	zEntry = SfiStr(PH7_ContextThis(pCtx),SDI_E,&nEntry);` |
|    40 | 11143 | `	ph7_result_string(pCtx,zEntry,nEntry);` |
|    40 | 11144 | `	return PH7_OK;` |
|    23 | 11145 | `}` |
|     2 | 11146 | `static int vm_builtin_DirectoryIterator_getBasename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11147 | `{` |
|     3 | 11148 | `	int nEntry = 0,nBase = 0;` |
|     - | 11149 | `	const char *zEntry,*zBase;` |
|     - | 11150 | `	sxi32 rc;` |
|     3 | 11151 | `	VmDirHandle *pH = SplDirChecked(pCtx,&rc);` |
|     3 | 11152 | `	if( pH == 0 ){` |
|   ! 0 | 11153 | `		return rc;` |
|     - | 11154 | `	}` |
|     3 | 11155 | `	zEntry = SfiStr(PH7_ContextThis(pCtx),SDI_E,&nEntry);` |
|     3 | 11156 | `	zBase = PH7_ExtractBaseName(zEntry,nEntry,&nBase);` |
|     3 | 11157 | `	if( nArg > 0 ){` |
|     3 | 11158 | `		int nSuffix = 0;` |
|     3 | 11159 | `		const char *zSuffix = ph7_value_to_string(apArg[0],&nSuffix);` |
|     2 | 11160 | `		if( nSuffix > 0 && nSuffix < nBase` |
|     3 | 11161 | `		 && SyMemcmp(&zBase[nBase - nSuffix],zSuffix,(sxu32)nSuffix) == 0 ){` |
|     3 | 11162 | `			nBase -= nSuffix;` |
|     1 | 11163 | `		}` |
|     1 | 11164 | `	}` |
|     3 | 11165 | `	ph7_result_string(pCtx,zBase,nBase);` |
|     3 | 11166 | `	return PH7_OK;` |
|     2 | 11167 | `}` |
|     2 | 11168 | `static int vm_builtin_DirectoryIterator_getExtension(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11169 | `{` |
|     3 | 11170 | `	int nEntry = 0,nBase = 0,i;` |
|     - | 11171 | `	const char *zEntry,*zBase;` |
|     - | 11172 | `	sxi32 rc;` |
|     3 | 11173 | `	VmDirHandle *pH = SplDirChecked(pCtx,&rc);` |
|     1 | 11174 | `	SXUNUSED(nArg);` |
|     1 | 11175 | `	SXUNUSED(apArg);` |
|     3 | 11176 | `	if( pH == 0 ){` |
|   ! 0 | 11177 | `		return rc;` |
|     - | 11178 | `	}` |
|     3 | 11179 | `	zEntry = SfiStr(PH7_ContextThis(pCtx),SDI_E,&nEntry);` |
|     3 | 11180 | `	zBase = PH7_ExtractBaseName(zEntry,nEntry,&nBase);` |
|     9 | 11181 | `	for( i = nBase - 1 ; i >= 0 ; --i ){` |
|     9 | 11182 | `		if( zBase[i] == '.' ){` |
|     3 | 11183 | `			ph7_result_string(pCtx,&zBase[i+1],nBase - i - 1);` |
|     3 | 11184 | `			return PH7_OK;` |
|     - | 11185 | `		}` |
|     4 | 11186 | `	}` |
|   ! 0 | 11187 | `	ph7_result_string(pCtx,"",0);` |
|   ! 0 | 11188 | `	return PH7_OK;` |
|     2 | 11189 | `}` |
|     - | 11190 | `/*` |
|     - | 11191 | ` * FilesystemIterator::key()/current(): php compares the flag against its MASK` |
|     - | 11192 | `` * (`(flags & MODE_MASK) == mode`) rather than testing a bit, so a stray bit in`` |
|     - | 11193 | ``  * another field cannot change either answer — which the chunk's `& KEY_AS_FILENAME` `` |
|     - | 11194 | `` * and `=== CURRENT_AS_PATHNAME` both got wrong in one direction or the other.`` |
|     - | 11195 | ` */` |
|    50 | 11196 | `static int vm_builtin_FilesystemIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11197 | `{` |
|    51 | 11198 | `	ph7_vm *pVm = pCtx->pVm;` |
|    51 | 11199 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    51 | 11200 | `	sxi64 iFlags = PH7_NativeAttrInt(pThis,SDI_F);` |
|    51 | 11201 | `	int nOut = 0;` |
|     - | 11202 | `	const char *zOut;` |
|     - | 11203 | `	sxi32 rcChk;` |
|    25 | 11204 | `	SXUNUSED(nArg);` |
|    25 | 11205 | `	SXUNUSED(apArg);` |
|    51 | 11206 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|     3 | 11207 | `		return rcChk;` |
|     - | 11208 | `	}` |
|    49 | 11209 | `	if( (iFlags & SDI_KEY_MODE_MASK) == SDI_KEY_AS_FILENAME ){` |
|     7 | 11210 | `		zOut = SfiStr(pThis,SDI_E,&nOut);` |
|     7 | 11211 | `		ph7_result_string(pCtx,zOut,nOut);` |
|     7 | 11212 | `		return PH7_OK;` |
|     - | 11213 | `	}` |
|    43 | 11214 | `	if( SplDirState(pVm,pThis) == 0 ){` |
|   ! 0 | 11215 | `		return PH7_VmThrowException(pCtx,"Error","Object not initialized");` |
|     - | 11216 | `	}` |
|    43 | 11217 | `	zOut = SfiName(pVm,pThis,&nOut);` |
|    43 | 11218 | `	ph7_result_string(pCtx,zOut,nOut);` |
|    43 | 11219 | `	return PH7_OK;` |
|    26 | 11220 | `}` |
|    76 | 11221 | `static int vm_builtin_FilesystemIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11222 | `{` |
|    77 | 11223 | `	ph7_vm *pVm = pCtx->pVm;` |
|    77 | 11224 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    77 | 11225 | `	sxi64 iMode = PH7_NativeAttrInt(pThis,SDI_F) & SDI_CURRENT_MODE_MASK;` |
|     - | 11226 | `	sxi32 rcChk;` |
|    38 | 11227 | `	SXUNUSED(nArg);` |
|    38 | 11228 | `	SXUNUSED(apArg);` |
|    77 | 11229 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|     3 | 11230 | `		return rcChk;` |
|     - | 11231 | `	}` |
|    75 | 11232 | `	if( iMode == SDI_CURRENT_AS_PATHNAME \|\| iMode == SDI_CURRENT_AS_FILEINFO ){` |
|    61 | 11233 | `		if( SplDirState(pVm,pThis) == 0 ){` |
|   ! 0 | 11234 | `			return PH7_VmThrowException(pCtx,"Error","Object not initialized");` |
|     - | 11235 | `		}` |
|    30 | 11236 | `	}` |
|    75 | 11237 | `	if( iMode == SDI_CURRENT_AS_PATHNAME ){` |
|     7 | 11238 | `		int nName = 0;` |
|     7 | 11239 | `		const char *zName = SfiName(pVm,pThis,&nName);` |
|     7 | 11240 | `		ph7_result_string(pCtx,zName,nName);` |
|     7 | 11241 | `		return PH7_OK;` |
|     - | 11242 | `	}` |
|    69 | 11243 | `	if( iMode == SDI_CURRENT_AS_FILEINFO ){` |
|    55 | 11244 | `		ph7_class *pClass = 0;` |
|    55 | 11245 | `		int nName = 0,nDir = 0;` |
|     - | 11246 | `		const char *zName,*zDir;` |
|     - | 11247 | `		sxi32 rc;` |
|    55 | 11248 | `		if( SplDirAtEnd(pThis) ){` |
|     - | 11249 | `			/* php's create_type again: there is no entry to describe. */` |
|   ! 0 | 11250 | `			return PH7_VmThrowException(pCtx,"RuntimeException","Could not open file");` |
|     - | 11251 | `		}` |
|    55 | 11252 | `		rc = SfiInfoClass(pCtx,"current",0,&pClass);` |
|    55 | 11253 | `		if( rc != PH7_OK ){` |
|   ! 0 | 11254 | `			return rc;` |
|     - | 11255 | `		}` |
|     - | 11256 | `		/* The child's PATH is the directory the walk is in, which for a glob` |
|     - | 11257 | `		 * handle is the current match's own -- the slot holds the whole` |
|     - | 11258 | ``		 * `glob://pattern`, and handing THAT over made every SplFileInfo the`` |
|     - | 11259 | `		 * iterator produced answer the pattern for getPath() and the joined` |
|     - | 11260 | `		 * name for getFilename(). */` |
|    55 | 11261 | `		zDir = SplDirGlobPath(pVm,pThis,&nDir);` |
|    55 | 11262 | `		if( zDir == 0 ){` |
|    33 | 11263 | `			zDir = SfiStr(pThis,SFI_P,&nDir);` |
|    16 | 11264 | `		}` |
|    55 | 11265 | `		zName = SfiName(pVm,pThis,&nName);` |
|    55 | 11266 | `		return SfiMakeInfoEx(pCtx,pClass,zName,nName,zDir,nDir);` |
|     - | 11267 | `	}` |
|    15 | 11268 | `	SplResultBorrowed(pCtx,pThis);` |
|    15 | 11269 | `	return PH7_OK;` |
|    39 | 11270 | `}` |
|     - | 11271 | `/* php's getFlags()/setFlags() answer and accept only the three mode fields;` |
|     - | 11272 | ` * everything else in the word is engine state the class keeps to itself. */` |
|    12 | 11273 | `static int vm_builtin_FilesystemIterator_getFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11274 | `{` |
|     - | 11275 | `	sxi32 rcChk;` |
|     6 | 11276 | `	SXUNUSED(nArg);` |
|     6 | 11277 | `	SXUNUSED(apArg);` |
|    13 | 11278 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|     3 | 11279 | `		return rcChk;` |
|     - | 11280 | `	}` |
|    11 | 11281 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(PH7_ContextThis(pCtx),SDI_F) & SDI_FLAGS_MASK);` |
|    11 | 11282 | `	return PH7_OK;` |
|     7 | 11283 | `}` |
|     2 | 11284 | `static int vm_builtin_FilesystemIterator_setFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11285 | `{` |
|     3 | 11286 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 11287 | `	sxi32 rcChk;` |
|     3 | 11288 | `	sxi64 iFlags = PH7_NativeAttrInt(pThis,SDI_F);` |
|     3 | 11289 | `	sxi64 iNew = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|     3 | 11290 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 11291 | `		return rcChk;` |
|     - | 11292 | `	}` |
|     4 | 11293 | `	PH7_NativeSetAttrInt(pCtx->pVm,pThis,SDI_F,(iFlags & ~(sxi64)SDI_FLAGS_MASK)` |
|     2 | 11294 | `		\| (iNew & (sxi64)SDI_FLAGS_MASK));` |
|     3 | 11295 | `	return PH7_OK;` |
|     2 | 11296 | `}` |
|     - | 11297 | `/*` |
|     - | 11298 | ` * RecursiveDirectoryIterator::hasChildren(bool $allowLinks = false).` |
|     - | 11299 | ` *` |
|     - | 11300 | ` * php lstats the entry and then asks two separate questions of it: a plain` |
|     - | 11301 | ` * directory has children, and a SYMLINK has them only when the walk was told to` |
|     - | 11302 | ` * follow links. Asked of the VFS rather than of a mode word, because the mode is` |
|     - | 11303 | ` * not filled on Windows (the same lesson getType() learned).` |
|     - | 11304 | ` */` |
|     4 | 11305 | `static int vm_builtin_RecursiveDirectoryIterator_hasChildren(ph7_context *pCtx,int nArg,` |
|     - | 11306 | `	ph7_value **apArg)` |
|     1 | 11307 | `{` |
|     5 | 11308 | `	ph7_vm *pVm = pCtx->pVm;` |
|     5 | 11309 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     5 | 11310 | `	const ph7_vfs *pVfs = pVm->pEngine->pVfs;` |
|     5 | 11311 | `	int nEntry = 0;` |
|     5 | 11312 | `	const char *zEntry = SfiStr(pThis,SDI_E,&nEntry);` |
|     - | 11313 | `	char zPath[4096];` |
|     5 | 11314 | `	int bAllow = nArg > 0 ? ph7_value_to_bool(apArg[0]) : 0;` |
|     5 | 11315 | `	sxi64 iFlags = PH7_NativeAttrInt(pThis,SDI_F);` |
|     4 | 11316 | `	if( nEntry < 1 \|\| SplDirIsDot(zEntry,nEntry) \|\| pVfs == 0` |
|     3 | 11317 | `	 \|\| SfiPathBuf(pVm,pThis,zPath,(int)sizeof(zPath)) != SXRET_OK ){` |
|     3 | 11318 | `		ph7_result_bool(pCtx,0);` |
|     3 | 11319 | `		return PH7_OK;` |
|     - | 11320 | `	}` |
|     2 | 11321 | `	if( pVfs->xIslink && pVfs->xIslink(zPath) == PH7_OK` |
|     2 | 11322 | `	 && !bAllow && (iFlags & SDI_FOLLOW_SYMLINKS) == 0 ){` |
|   ! 0 | 11323 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 11324 | `		return PH7_OK;` |
|     - | 11325 | `	}` |
|     3 | 11326 | `	ph7_result_bool(pCtx,pVfs->xIsdir && pVfs->xIsdir(zPath) == PH7_OK);` |
|     3 | 11327 | `	return PH7_OK;` |
|     3 | 11328 | `}` |
|     - | 11329 | `/*` |
|     - | 11330 | ` * getChildren(): php builds an instance of the RUNTIME class through its` |
|     - | 11331 | ` * constructor with (pathname, flags), then hands it the sub path — which is what` |
|     - | 11332 | ` * makes getSubPathname() name the whole nested route rather than just the entry` |
|     - | 11333 | `` * (the chunk answered `''` and the filename, wrong at every depth below one).`` |
|     - | 11334 | ` */` |
|     2 | 11335 | `static int vm_builtin_RecursiveDirectoryIterator_getChildren(ph7_context *pCtx,int nArg,` |
|     - | 11336 | `	ph7_value **apArg)` |
|     1 | 11337 | `{` |
|     3 | 11338 | `	ph7_vm *pVm = pCtx->pVm;` |
|     3 | 11339 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 11340 | `	ph7_class_instance *pNew;` |
|     - | 11341 | `	ph7_class_method *pCons;` |
|     - | 11342 | `	ph7_value sPath,sFlags,*apCall[2];` |
|     3 | 11343 | `	int nName = 0,nSub = 0,nEntry = 0;` |
|     - | 11344 | `	const char *zName;` |
|     3 | 11345 | `	sxi64 iFlags = PH7_NativeAttrInt(pThis,SDI_F);` |
|     - | 11346 | `	sxi32 rc;` |
|     - | 11347 | `	SyBlob sSub;` |
|     1 | 11348 | `	SXUNUSED(nArg);` |
|     1 | 11349 | `	SXUNUSED(apArg);` |
|     3 | 11350 | `	if( SplDirState(pVm,pThis) == 0 ){` |
|   ! 0 | 11351 | `		return PH7_VmThrowException(pCtx,"Error","Object not initialized");` |
|     - | 11352 | `	}` |
|     3 | 11353 | `	pNew = PH7_NewClassInstance(pVm,pThis->pClass);` |
|     3 | 11354 | `	if( pNew == 0 ){` |
|   ! 0 | 11355 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 11356 | `	}` |
|     3 | 11357 | `	pNew->iRef++;` |
|     3 | 11358 | `	zName = SfiName(pVm,pThis,&nName);` |
|     3 | 11359 | `	PH7_MemObjInitFromString(pVm,&sPath,0);` |
|     3 | 11360 | `	PH7_MemObjStringAppend(&sPath,zName,(sxu32)nName);` |
|     3 | 11361 | `	PH7_MemObjInitFromInt(pVm,&sFlags,iFlags);` |
|     3 | 11362 | `	apCall[0] = &sPath;` |
|     3 | 11363 | `	apCall[1] = &sFlags;` |
|     3 | 11364 | `	pCons = PH7_ClassExtractMethod(pThis->pClass,"__construct",sizeof("__construct")-1);` |
|     3 | 11365 | `	rc = pCons ? PH7_VmCallClassMethod(pVm,pNew,pCons,0,2,apCall) : SXRET_OK;` |
|     3 | 11366 | `	PH7_MemObjRelease(&sPath);` |
|     3 | 11367 | `	PH7_MemObjRelease(&sFlags);` |
|     3 | 11368 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 11369 | `		PH7_ClassInstanceUnref(pNew);` |
|   ! 0 | 11370 | `		return rc;` |
|     - | 11371 | `	}` |
|     - | 11372 | `	/* php's sub_path: the parent's, this entry appended. */` |
|     3 | 11373 | `	SyBlobInit(&sSub,&pVm->sAllocator);` |
|     - | 11374 | `	{` |
|     3 | 11375 | `		const char *zSub = SfiStr(pThis,SDI_S,&nSub);` |
|     3 | 11376 | `		SyBlobAppend(&sSub,zSub,(sxu32)nSub);` |
|     - | 11377 | `	}` |
|     3 | 11378 | `	if( nSub > 0 ){` |
|   ! 0 | 11379 | `		char cSlash = SplDirSlash(iFlags);` |
|   ! 0 | 11380 | `		SyBlobAppend(&sSub,(const void *)&cSlash,sizeof(char));` |
|   ! 0 | 11381 | `	}` |
|     - | 11382 | `	{` |
|     3 | 11383 | `		const char *zEntry = SfiStr(pThis,SDI_E,&nEntry);` |
|     3 | 11384 | `		SyBlobAppend(&sSub,zEntry,(sxu32)nEntry);` |
|     - | 11385 | `	}` |
|     4 | 11386 | `	PH7_NativeSetAttrStr(pVm,pNew,SDI_S,` |
|     2 | 11387 | `		(const char *)SyBlobData(&sSub),(int)SyBlobLength(&sSub));` |
|     3 | 11388 | `	SyBlobRelease(&sSub);` |
|     - | 11389 | `	{` |
|     3 | 11390 | `		int nInfo = 0;` |
|     3 | 11391 | `		const char *zInfo = SfiStr(pThis,SFI_IC,&nInfo);` |
|     3 | 11392 | `		PH7_NativeSetAttrStr(pVm,pNew,SFI_IC,zInfo,nInfo);` |
|     - | 11393 | `	}` |
|     3 | 11394 | `	PH7_NativeResultObject(pCtx,pNew);` |
|     3 | 11395 | `	PH7_ClassInstanceUnref(pNew);` |
|     3 | 11396 | `	return PH7_OK;` |
|     2 | 11397 | `}` |
|     4 | 11398 | `static int vm_builtin_RecursiveDirectoryIterator_getSubPath(ph7_context *pCtx,int nArg,` |
|     - | 11399 | `	ph7_value **apArg)` |
|     1 | 11400 | `{` |
|     5 | 11401 | `	int nSub = 0;` |
|     5 | 11402 | `	const char *zSub = SfiStr(PH7_ContextThis(pCtx),SDI_S,&nSub);` |
|     2 | 11403 | `	SXUNUSED(nArg);` |
|     2 | 11404 | `	SXUNUSED(apArg);` |
|     5 | 11405 | `	ph7_result_string(pCtx,zSub,nSub);` |
|     5 | 11406 | `	return PH7_OK;` |
|     1 | 11407 | `}` |
|     4 | 11408 | `static int vm_builtin_RecursiveDirectoryIterator_getSubPathname(ph7_context *pCtx,int nArg,` |
|     - | 11409 | `	ph7_value **apArg)` |
|     1 | 11410 | `{` |
|     5 | 11411 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     5 | 11412 | `	int nSub = 0,nEntry = 0;` |
|     5 | 11413 | `	const char *zSub = SfiStr(pThis,SDI_S,&nSub);` |
|     - | 11414 | `	SyBlob sOut;` |
|     2 | 11415 | `	SXUNUSED(nArg);` |
|     2 | 11416 | `	SXUNUSED(apArg);` |
|     5 | 11417 | `	if( nSub < 1 ){` |
|     3 | 11418 | `		const char *zEntry = SfiStr(pThis,SDI_E,&nEntry);` |
|     3 | 11419 | `		ph7_result_string(pCtx,zEntry,nEntry);` |
|     3 | 11420 | `		return PH7_OK;` |
|     - | 11421 | `	}` |
|     3 | 11422 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|     3 | 11423 | `	SyBlobAppend(&sOut,zSub,(sxu32)nSub);` |
|     - | 11424 | `	{` |
|     3 | 11425 | `		char cSlash = SplDirSlash(PH7_NativeAttrInt(pThis,SDI_F));` |
|     3 | 11426 | `		SyBlobAppend(&sOut,(const void *)&cSlash,sizeof(char));` |
|     - | 11427 | `	}` |
|     - | 11428 | `	{` |
|     3 | 11429 | `		const char *zEntry = SfiStr(pThis,SDI_E,&nEntry);` |
|     3 | 11430 | `		SyBlobAppend(&sOut,zEntry,(sxu32)nEntry);` |
|     - | 11431 | `	}` |
|     3 | 11432 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     3 | 11433 | `	SyBlobRelease(&sOut);` |
|     3 | 11434 | `	return PH7_OK;` |
|     3 | 11435 | `}` |
|     - | 11436 | `/*` |
|     - | 11437 | ` * GlobIterator::__construct(string $pattern, int $flags = 0)` |
|     - | 11438 | ` *` |
|     - | 11439 | `` * php's DIT_CTOR_GLOB: the `glob://` prefix goes on when the caller did not`` |
|     - | 11440 | `` * write one, and the whole `glob://pattern` is what the path slot keeps -- so`` |
|     - | 11441 | `` * the `glob` debug key shows it, and getPath() has to ask the STREAM instead`` |
|     - | 11442 | ` * (SplDirGlobPath). The default flags are 0, which is` |
|     - | 11443 | ` * KEY_AS_PATHNAME\|CURRENT_AS_FILEINFO with SKIP_DOTS OFF where` |
|     - | 11444 | `` * FilesystemIterator has it on: `glob('d/.*')` yields `.` and `..` through the`` |
|     - | 11445 | ` * iterator exactly as it does through the function.` |
|     - | 11446 | ` */` |
|    30 | 11447 | `static int vm_builtin_GlobIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11448 | `{` |
|    31 | 11449 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 11450 | `	const char *zPat;` |
|    31 | 11451 | `	int nPat = 0;` |
|     - | 11452 | `	ph7_value sUri;` |
|     - | 11453 | `	sxi32 rc;` |
|    31 | 11454 | `	if( PH7_ContextThis(pCtx) == 0 \|\| nArg < 1 ){` |
|   ! 0 | 11455 | `		return PH7_OK;` |
|     - | 11456 | `	}` |
|    31 | 11457 | `	zPat = ph7_value_to_string(apArg[0],&nPat);` |
|    31 | 11458 | `	if( nPat < 1 ){` |
|     - | 11459 | `		/* php's empty check runs on the ARGUMENT, before the prefix goes on. */` |
|     3 | 11460 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 11461 | `			"GlobIterator::__construct(): Argument #1 ($pattern) must not be empty");` |
|     - | 11462 | `	}` |
|    29 | 11463 | `	PH7_MemObjInitFromString(pVm,&sUri,0);` |
|     - | 11464 | `	/* Both of php's prefix tests are CASE-SENSITIVE, where the wrapper LOOKUP` |
|     - | 11465 | ``	 * that follows is not: `GLOB://x` is prefixed again, so the pattern the`` |
|     - | 11466 | ``	 * device is finally handed still spells `GLOB://x` and matches nothing --`` |
|     - | 11467 | `	 * and the debug key shows the doubled string. */` |
|    28 | 11468 | `	if( nPat < (int)sizeof("glob://")-1` |
|    29 | 11469 | `	 \|\| SyMemcmp(zPat,"glob://",sizeof("glob://")-1) != 0 ){` |
|    27 | 11470 | `		PH7_MemObjStringAppend(&sUri,"glob://",sizeof("glob://")-1);` |
|    13 | 11471 | `	}` |
|    29 | 11472 | `	PH7_MemObjStringAppend(&sUri,zPat,(sxu32)nPat);` |
|    43 | 11473 | `	rc = SplDirConstructVal(pCtx,"GlobIterator","pattern",&sUri,` |
|    14 | 11474 | `		SplDirFlagArg(nArg,apArg,SDI_KEY_AS_PATHNAME\|SDI_CURRENT_AS_FILEINFO));` |
|    29 | 11475 | `	PH7_MemObjRelease(&sUri);` |
|    29 | 11476 | `	return rc;` |
|    16 | 11477 | `}` |
|     - | 11478 | `/*` |
|     - | 11479 | ` * GlobIterator::count(): php's php_glob_stream_get_count, which is the number` |
|     - | 11480 | ` * of MATCHES and does not move with the walk -- a pattern ending in a slash` |
|     - | 11481 | ` * counts its directories and yields none of them, because their entry is the` |
|     - | 11482 | ` * empty string a walk reads as the end.` |
|     - | 11483 | ` */` |
|    16 | 11484 | `static int vm_builtin_GlobIterator_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 11485 | `{` |
|    17 | 11486 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 11487 | `	VmDirHandle *pH;` |
|     - | 11488 | `	sxi32 rcChk;` |
|     8 | 11489 | `	SXUNUSED(nArg);` |
|     8 | 11490 | `	SXUNUSED(apArg);` |
|    17 | 11491 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|     3 | 11492 | `		return rcChk;` |
|     - | 11493 | `	}` |
|    15 | 11494 | `	pH = SplDirFind(pCtx->pVm,pThis);` |
|    15 | 11495 | `	if( pH == 0 \|\| pH->pStream == 0 \|\| !PH7_GlobStreamIs(pH->pStream) ){` |
|     - | 11496 | `		/* php's own "should not happen", raised as a fatal there. */` |
|   ! 0 | 11497 | `		return PH7_VmThrowException(pCtx,"Error","GlobIterator lost glob state");` |
|     - | 11498 | `	}` |
|    15 | 11499 | `	ph7_result_int64(pCtx,PH7_GlobStreamCount(pH->pHandle));` |
|    15 | 11500 | `	return PH7_OK;` |
|     9 | 11501 | `}` |
|     - | 11502 | `/*` |
|     - | 11503 | ` * The four declarations. Method ORDER, signatures and tentative return types` |
|     - | 11504 | `` * are spl_directory.stub.php's; the four slots are php's `u.dir` arm and carry`` |
|     - | 11505 | ` * PH7_MOD_HIDDEN because php declares no property at all here. Each class` |
|     - | 11506 | ` * restates NOSERIALIZE and the presentation hook: a native subclass inherits` |
|     - | 11507 | ` * neither (rule 29).` |
|     - | 11508 | ` */` |
|  5740 | 11509 | `static sxi32 VmInstallSplDirIterators(ph7_vm *pVm)` |
|     5 | 11510 | `{` |
|     - | 11511 | `	static const PH7_NativePropDef aDirProp[] = {` |
|     - | 11512 | `		{ SDI_E, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|     - | 11513 | `		{ SDI_I, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 11514 | `		{ SDI_F, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 11515 | `		{ SDI_S, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|     - | 11516 | `	};` |
|     - | 11517 | `	static const PH7_NativeMethodDef aDirMethod[] = {` |
|     - | 11518 | `		{ "__construct",  PH7_MOD_PUBLIC, "string $directory", 0,` |
|     - | 11519 | `		  vm_builtin_DirectoryIterator_construct },` |
|     - | 11520 | `		{ "getFilename",  PH7_MOD_PUBLIC, "", "@string",` |
|     - | 11521 | `		  vm_builtin_DirectoryIterator_getFilename },` |
|     - | 11522 | `		{ "getExtension", PH7_MOD_PUBLIC, "", "@string",` |
|     - | 11523 | `		  vm_builtin_DirectoryIterator_getExtension },` |
|     - | 11524 | `		{ "getBasename",  PH7_MOD_PUBLIC, "string $suffix = \"\"", "@string",` |
|     - | 11525 | `		  vm_builtin_DirectoryIterator_getBasename },` |
|     - | 11526 | `		{ "isDot",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_DirectoryIterator_isDot },` |
|     - | 11527 | `		{ "rewind",       PH7_MOD_PUBLIC, "", "@void", vm_builtin_DirectoryIterator_rewind },` |
|     - | 11528 | `		{ "valid",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_DirectoryIterator_valid },` |
|     - | 11529 | `		{ "key",          PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_DirectoryIterator_key },` |
|     - | 11530 | `		{ "current",      PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_DirectoryIterator_current },` |
|     - | 11531 | `		{ "next",         PH7_MOD_PUBLIC, "", "@void", vm_builtin_DirectoryIterator_next },` |
|     - | 11532 | `		{ "seek",         PH7_MOD_PUBLIC, "int $offset", "@void",` |
|     - | 11533 | `		  vm_builtin_DirectoryIterator_seek },` |
|     - | 11534 | ``		/* php aliases this one to getFilename(), so `echo $it` prints the ENTRY where`` |
|     - | 11535 | `		 * SplFileInfo's __toString prints the whole pathname. Not tentative. */` |
|     - | 11536 | `		{ "__toString",   PH7_MOD_PUBLIC, "", "string",` |
|     - | 11537 | `		  vm_builtin_DirectoryIterator_getFilename },` |
|     - | 11538 | `	};` |
|     - | 11539 | `	static const PH7_NativeConstDef aFsConst[] = {` |
|     - | 11540 | `		{ "CURRENT_MODE_MASK",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_CURRENT_MODE_MASK, 0, 0.0 },` |
|     - | 11541 | `		{ "CURRENT_AS_PATHNAME", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_CURRENT_AS_PATHNAME, 0, 0.0 },` |
|     - | 11542 | `		{ "CURRENT_AS_FILEINFO", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_CURRENT_AS_FILEINFO, 0, 0.0 },` |
|     - | 11543 | `		{ "CURRENT_AS_SELF",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_CURRENT_AS_SELF, 0, 0.0 },` |
|     - | 11544 | `		{ "KEY_MODE_MASK",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_KEY_MODE_MASK, 0, 0.0 },` |
|     - | 11545 | `		{ "KEY_AS_PATHNAME",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_KEY_AS_PATHNAME, 0, 0.0 },` |
|     - | 11546 | `		{ "FOLLOW_SYMLINKS",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_FOLLOW_SYMLINKS, 0, 0.0 },` |
|     - | 11547 | `		{ "KEY_AS_FILENAME",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_KEY_AS_FILENAME, 0, 0.0 },` |
|     - | 11548 | `		{ "NEW_CURRENT_AND_KEY", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_KEY_AS_FILENAME\|SDI_CURRENT_AS_FILEINFO, 0, 0.0 },` |
|     - | 11549 | `		{ "OTHER_MODE_MASK",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_OTHERS_MASK, 0, 0.0 },` |
|     - | 11550 | `		{ "SKIP_DOTS",           PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_SKIPDOTS, 0, 0.0 },` |
|     - | 11551 | `		{ "UNIX_PATHS",          PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_UNIXPATHS, 0, 0.0 },` |
|     - | 11552 | `	};` |
|     - | 11553 | `	static const PH7_NativeMethodDef aFsMethod[] = {` |
|     - | 11554 | `		{ "__construct", PH7_MOD_PUBLIC,` |
|     - | 11555 | `		  "string $directory, int $flags = FilesystemIterator::KEY_AS_PATHNAME \| "` |
|     - | 11556 | `		  "FilesystemIterator::CURRENT_AS_FILEINFO \| FilesystemIterator::SKIP_DOTS", 0,` |
|     - | 11557 | `		  vm_builtin_FilesystemIterator_construct },` |
|     - | 11558 | `		{ "rewind",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_FilesystemIterator_rewind },` |
|     - | 11559 | `		{ "key",         PH7_MOD_PUBLIC, "", "@string", vm_builtin_FilesystemIterator_key },` |
|     - | 11560 | `		{ "current",     PH7_MOD_PUBLIC, "", "@SplFileInfo\|FilesystemIterator\|string",` |
|     - | 11561 | `		  vm_builtin_FilesystemIterator_current },` |
|     - | 11562 | `		{ "getFlags",    PH7_MOD_PUBLIC, "", "@int", vm_builtin_FilesystemIterator_getFlags },` |
|     - | 11563 | `		{ "setFlags",    PH7_MOD_PUBLIC, "int $flags", "@void",` |
|     - | 11564 | `		  vm_builtin_FilesystemIterator_setFlags },` |
|     - | 11565 | `	};` |
|     - | 11566 | `	static const PH7_NativeMethodDef aRdiMethod[] = {` |
|     - | 11567 | `		{ "__construct",    PH7_MOD_PUBLIC,` |
|     - | 11568 | `		  "string $directory, int $flags = FilesystemIterator::KEY_AS_PATHNAME \| "` |
|     - | 11569 | `		  "FilesystemIterator::CURRENT_AS_FILEINFO", 0,` |
|     - | 11570 | `		  vm_builtin_RecursiveDirectoryIterator_construct },` |
|     - | 11571 | `		{ "hasChildren",    PH7_MOD_PUBLIC, "bool $allowLinks = false", "@bool",` |
|     - | 11572 | `		  vm_builtin_RecursiveDirectoryIterator_hasChildren },` |
|     - | 11573 | `		{ "getChildren",    PH7_MOD_PUBLIC, "", "@RecursiveDirectoryIterator",` |
|     - | 11574 | `		  vm_builtin_RecursiveDirectoryIterator_getChildren },` |
|     - | 11575 | `		{ "getSubPath",     PH7_MOD_PUBLIC, "", "@string",` |
|     - | 11576 | `		  vm_builtin_RecursiveDirectoryIterator_getSubPath },` |
|     - | 11577 | `		{ "getSubPathname", PH7_MOD_PUBLIC, "", "@string",` |
|     - | 11578 | `		  vm_builtin_RecursiveDirectoryIterator_getSubPathname },` |
|     - | 11579 | `	};` |
|     - | 11580 | `	static const PH7_NativeMethodDef aGlobMethod[] = {` |
|     - | 11581 | `		{ "__construct", PH7_MOD_PUBLIC,` |
|     - | 11582 | `		  "string $pattern, int $flags = FilesystemIterator::KEY_AS_PATHNAME \| "` |
|     - | 11583 | `		  "FilesystemIterator::CURRENT_AS_FILEINFO", 0,` |
|     - | 11584 | `		  vm_builtin_GlobIterator_construct },` |
|     - | 11585 | `		{ "count",       PH7_MOD_PUBLIC, "", "@int", vm_builtin_GlobIterator_count },` |
|     - | 11586 | `	};` |
|     - | 11587 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - | 11588 | `		{ "DirectoryIterator", "SplFileInfo", "SeekableIterator", PH7_CLASS_NOSERIALIZE,` |
|     - | 11589 | `		  aDirMethod, SX_ARRAYSIZE(aDirMethod), 0, 0,` |
|     - | 11590 | `		  aDirProp, SX_ARRAYSIZE(aDirProp), SplDirClose, 0, SfiPresent },` |
|     - | 11591 | `		{ "FilesystemIterator", "DirectoryIterator", 0, PH7_CLASS_NOSERIALIZE,` |
|     - | 11592 | `		  aFsMethod, SX_ARRAYSIZE(aFsMethod), aFsConst, SX_ARRAYSIZE(aFsConst),` |
|     - | 11593 | `		  0, 0, SplDirClose, 0, SfiPresent },` |
|     - | 11594 | `		{ "RecursiveDirectoryIterator", "FilesystemIterator", "RecursiveIterator",` |
|     - | 11595 | `		  PH7_CLASS_NOSERIALIZE,` |
|     - | 11596 | `		  aRdiMethod, SX_ARRAYSIZE(aRdiMethod), 0, 0,` |
|     - | 11597 | `		  0, 0, SplDirClose, 0, SfiPresent },` |
|     - | 11598 | ``		/* php gives this one the `check` object handlers SplFileObject has, so it`` |
|     - | 11599 | `		 * is UNCLONEABLE and refuses every method -- inherited ones included --` |
|     - | 11600 | `		 * on an instance whose parent constructor never ran (SfoIsChecked). A` |
|     - | 11601 | `		 * native class inherits neither the refusals nor the hooks (rule 29), so` |
|     - | 11602 | `		 * both are restated here. */` |
|     - | 11603 | `		{ "GlobIterator", "FilesystemIterator", "Countable",` |
|     - | 11604 | `		  PH7_CLASS_NOSERIALIZE\|PH7_CLASS_NOCLONE,` |
|     - | 11605 | `		  aGlobMethod, SX_ARRAYSIZE(aGlobMethod), 0, 0,` |
|     - | 11606 | `		  0, 0, SplDirClose, 0, SfiPresent },` |
|     - | 11607 | `	};` |
|  5745 | 11608 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 | 11609 | `}` |
|     - | 11610 | `/*` |
|     - | 11611 | ` * ---------------------------------------------------------------------------` |
|     - | 11612 | ` * SplFileObject.` |
|     - | 11613 | ` *` |
|     - | 11614 | `` * php's SPL_FS_FILE arm of the same `spl_filesystem_object`: an OPEN stream plus`` |
|     - | 11615 | ` * ONE line of look-ahead. The line is the whole model, and nearly every rule` |
|     - | 11616 | ` * below is about which of the two current values is live rather than about IO.` |
|     - | 11617 | `` * php keeps `current_line` (a string) and `current_zval` (the READ_CSV array)`` |
|     - | 11618 | `` * side by side, either or both present, and `current()` picks between them; the`` |
|     - | 11619 | ``  * line NUMBER is bumped by whatever read produced them, which is why `key()` `` |
|     - | 11620 | ``  * counts differently for `fgetc()` (only a `\n` advances it), for `fgets()` `` |
|     - | 11621 | `` * (always) and for `current()` (only when a line was already there).`` |
|     - | 11622 | ` *` |
|     - | 11623 | ` * The stream lives in a hidden slot as a RESOURCE rather than on a registry the` |
|     - | 11624 | ` * way a DirectoryIterator's DIR does, because php makes this class UNCLONEABLE` |
|     - | 11625 | `` * (its `check` object handlers null the clone handler out) -- so no second`` |
|     - | 11626 | ` * object can ever reach the same handle, and the class's xRelease is the only` |
|     - | 11627 | ` * teardown there is. That is also php's flush point: a file written through an` |
|     - | 11628 | ` * SplFileObject is finished when the OBJECT dies.` |
|     - | 11629 | ` *` |
|     - | 11630 | `` * Those same `check` handlers give the class a get_method that refuses EVERY`` |
|     - | 11631 | ` * method -- the ones inherited from SplFileInfo included -- on an instance whose` |
|     - | 11632 | ` * parent constructor never ran (a subclass that forgets it, a caught` |
|     - | 11633 | ` * constructor failure). SfiReady() below is where that check lives, so the` |
|     - | 11634 | ` * inherited accessors carry it too.` |
|     - | 11635 | ` * ---------------------------------------------------------------------------` |
|     - | 11636 | ` */` |
|     - | 11637 | `/* php's spl_directory.h SPL_FILE_OBJECT_* flags, and the mask getFlags() cuts` |
|     - | 11638 | ` * the stored word with. */` |
|     - | 11639 | `#define SFO_DROP_NEW_LINE 0x0001` |
|     - | 11640 | `#define SFO_READ_AHEAD    0x0002` |
|     - | 11641 | `#define SFO_SKIP_EMPTY    0x0004` |
|     - | 11642 | `#define SFO_READ_CSV      0x0008` |
|     - | 11643 | `#define SFO_FLAGS_MASK    0x000F` |
|     - | 11644 | `/* Which of php's two current values this instance is holding. Both may be live` |
|     - | 11645 | ` * at once: a CSV read keeps the raw line beside the parsed array. */` |
|     - | 11646 | `#define SFO_HAS_LINE 0x1` |
|     - | 11647 | `#define SFO_HAS_ZVAL 0x2` |
|     - | 11648 | ``/* The state slots are named beside SplFileInfo's, up with the `u.dir` arm: they`` |
|     - | 11649 | ` * are all hidden (php declares no property on this class either) and three of` |
|     - | 11650 | ` * them are what the shared presentation hook shows.` |
|     - | 11651 | ` *` |
|     - | 11652 | ` * What a read attempt did. php's zend_result plus the third case a THROW is:` |
|     - | 11653 | ` * the read routines run user code (a subclass's getCurrentLine) and raise` |
|     - | 11654 | ` * php's own RuntimeException, so a caller has to be able to abandon. */` |
|     - | 11655 | `#define SFO_READ_OK    0` |
|     - | 11656 | `#define SFO_READ_FAIL  1` |
|     - | 11657 | `#define SFO_READ_THROW 2` |
|     - | 11658 |  |
|     - | 11659 | `/* This class's own getCurrentLine body, named ahead of the read routine that` |
|     - | 11660 | ` * has to tell it apart from a subclass's override. */` |
|     - | 11661 | `static int vm_builtin_SplFileObject_fgets(ph7_context *pCtx,int nArg,ph7_value **apArg);` |
|     - | 11662 | ``/* php's `!intern->u.file.stream`: the uninitialized object. */`` |
|  1514 | 11663 | `static io_private * SfoDev(ph7_class_instance *pThis)` |
|     1 | 11664 | `{` |
|  1515 | 11665 | `	ph7_value *pVal = pThis ? PH7_NativeAttr(pThis,SFO_H) : 0;` |
|     - | 11666 | `	io_private *pDev;` |
|  1515 | 11667 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_RES) == 0 ){` |
|   343 | 11668 | `		return 0;` |
|     - | 11669 | `	}` |
|  1173 | 11670 | `	pDev = (io_private *)pVal->x.pOther;` |
|  1173 | 11671 | `	return IO_PRIVATE_INVALID(pDev) ? 0 : pDev;` |
|   758 | 11672 | `}` |
|     - | 11673 | ``/* Is this instance one of the classes php gives the `check` handlers to? Asked`` |
|     - | 11674 | ` * as a CLASS question for the same reason SplDirIs() is: a user class may` |
|     - | 11675 | ` * declare anything it likes. php gives them to TWO: SplFileObject and` |
|     - | 11676 | ` * GlobIterator, which is why an unconstructed GlobIterator refuses every method` |
|     - | 11677 | ` * with this sentence where an unconstructed FilesystemIterator answers` |
|     - | 11678 | `` * `Object not initialized` -- and why neither of the two can be cloned. */`` |
|  1305 | 11679 | `static int SfoIsChecked(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 11680 | `{` |
|     - | 11681 | `	ph7_class *pFile;` |
|  1306 | 11682 | `	if( pThis == 0 ){` |
|   ! 0 | 11683 | `		return 0;` |
|     - | 11684 | `	}` |
|  1306 | 11685 | `	pFile = PH7_VmExtractClass(pVm,"SplFileObject",sizeof("SplFileObject")-1,FALSE,0);` |
|  1306 | 11686 | `	if( pFile && PH7_VmInstanceOf(pThis->pClass,pFile) ){` |
|   341 | 11687 | `		return 1;` |
|     - | 11688 | `	}` |
|   966 | 11689 | `	pFile = PH7_VmExtractClass(pVm,"GlobIterator",sizeof("GlobIterator")-1,FALSE,0);` |
|   966 | 11690 | `	return pFile && PH7_VmInstanceOf(pThis->pClass,pFile) ? 1 : 0;` |
|   657 | 11691 | `}` |
|     - | 11692 | `/*` |
|     - | 11693 | ` * php's spl_filesystem_object_get_method_check, as a guard the bodies call:` |
|     - | 11694 | `` * an instance of a `check` class with nothing open answers NO method at all.`` |
|     - | 11695 | ` * Answers 0 when the caller must return *pRc.` |
|     - | 11696 | ` */` |
|     - | 11697 | `/*` |
|     - | 11698 | `` * php's `u.file.stream == NULL && orig_path == NULL`. The two classes that`` |
|     - | 11699 | ` * carry the check handlers fill different halves of it. SplFileObject has an` |
|     - | 11700 | ` * open BYTE stream -- and NO orig_path when its open FAILED, which is why a` |
|     - | 11701 | ` * subclass that catches its own parent constructor's exception is left` |
|     - | 11702 | ` * refusing every method. GlobIterator has a path instead: the whole` |
|     - | 11703 | `` * `glob://pattern`, written by the open whether the pattern matched anything`` |
|     - | 11704 | ` * or not, so an iterator over nothing is still a working object.` |
|     - | 11705 | ` */` |
|   374 | 11706 | `static int SfoCheckReady(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 11707 | `{` |
|   375 | 11708 | `	int nPath = 0;` |
|   375 | 11709 | `	if( SfoDev(pThis) != 0 ){` |
|   209 | 11710 | `		return 1;` |
|     - | 11711 | `	}` |
|   167 | 11712 | `	if( !SplGlobIs(pVm,pThis) ){` |
|    11 | 11713 | `		return 0;` |
|     - | 11714 | `	}` |
|   157 | 11715 | `	SfiStr(pThis,SFI_P,&nPath);` |
|   157 | 11716 | `	return nPath > 0;` |
|   188 | 11717 | `}` |
|  1183 | 11718 | `static int SfoChecked(ph7_context *pCtx,sxi32 *pRc)` |
|     1 | 11719 | `{` |
|  1184 | 11720 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|  1184 | 11721 | `	*pRc = PH7_OK;` |
|  1184 | 11722 | `	if( SfoIsChecked(pCtx->pVm,pThis) && !SfoCheckReady(pCtx->pVm,pThis) ){` |
|    31 | 11723 | `		*pRc = PH7_VmThrowException(pCtx,"Error",` |
|     - | 11724 | `			"The parent constructor was not called: the object is in an invalid state");` |
|    31 | 11725 | `		return 0;` |
|     - | 11726 | `	}` |
|  1154 | 11727 | `	return 1;` |
|   596 | 11728 | `}` |
|   804 | 11729 | `static sxi64 SfoFlags(ph7_class_instance *pThis)` |
|     1 | 11730 | `{` |
|   805 | 11731 | `	return PH7_NativeAttrInt(pThis,SFO_FL);` |
|     1 | 11732 | `}` |
|     - | 11733 | `/* php's spl_filesystem_file_free_line: BOTH current values go. */` |
|   360 | 11734 | `static void SfoFreeLine(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 11735 | `{` |
|     - | 11736 | `	ph7_value *pZ;` |
|   361 | 11737 | `	PH7_NativeSetAttrStr(pVm,pThis,SFO_L,"",0);` |
|   361 | 11738 | `	pZ = PH7_NativeAttr(pThis,SFO_Z);` |
|   361 | 11739 | `	if( pZ ){` |
|   361 | 11740 | `		PH7_MemObjRelease(pZ);` |
|   180 | 11741 | `	}` |
|   361 | 11742 | `	PH7_NativeSetAttrInt(pVm,pThis,SFO_LS,0);` |
|   361 | 11743 | `}` |
|     - | 11744 | `/* The current line's bytes. Only meaningful while SFO_HAS_LINE is set. */` |
|   212 | 11745 | `static const char * SfoLine(ph7_class_instance *pThis,int *pnLen)` |
|     1 | 11746 | `{` |
|   213 | 11747 | `	return SfiStr(pThis,SFO_L,pnLen);` |
|     1 | 11748 | `}` |
|     - | 11749 | `/*` |
|     - | 11750 | ` * php's spl_filesystem_file_read_ex: drop what is held, refuse at EOF (loudly` |
|     - | 11751 | ` * unless silent), take ONE line, apply DROP_NEW_LINE unless this is the CSV` |
|     - | 11752 | `` * path, and add `iLineAdd` to the line number.`` |
|     - | 11753 | ` */` |
|   176 | 11754 | `static int SfoReadEx(ph7_context *pCtx,int bSilent,int iLineAdd,int bCsv,sxi32 *pRc)` |
|     1 | 11755 | `{` |
|   177 | 11756 | `	ph7_vm *pVm = pCtx->pVm;` |
|   177 | 11757 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   177 | 11758 | `	io_private *pDev = SfoDev(pThis);` |
|   177 | 11759 | `	sxi64 iMax = PH7_NativeAttrInt(pThis,SFO_ML);` |
|   177 | 11760 | `	const char *zLine = 0;` |
|     - | 11761 | `	ph7_int64 n;` |
|   177 | 11762 | `	*pRc = PH7_OK;` |
|   177 | 11763 | `	SfoFreeLine(pVm,pThis);` |
|   177 | 11764 | `	if( pDev == 0 \|\| PH7_StreamAtEof(pDev) ){` |
|    17 | 11765 | `		if( !bSilent ){` |
|     3 | 11766 | `			int nName = 0;` |
|     3 | 11767 | `			const char *zName = SfiName(pVm,pThis,&nName);` |
|     4 | 11768 | `			*pRc = PH7_VmThrowException(pCtx,"RuntimeException",` |
|     1 | 11769 | `				"Cannot read from file %.*s",nName,zName);` |
|     3 | 11770 | `			return SFO_READ_THROW;` |
|     - | 11771 | `		}` |
|    15 | 11772 | `		return SFO_READ_FAIL;` |
|     - | 11773 | `	}` |
|   161 | 11774 | `	n = StreamReadLine(pDev,&zLine,iMax > 0 ? (ph7_int64)iMax : 0);` |
|   161 | 11775 | `	if( n < 1 ){` |
|     - | 11776 | `		/* The device's own notice, worded from THIS method: a read on a handle` |
|     - | 11777 | `		 * opened write-only says so here as it does from fgets(). */` |
|    27 | 11778 | `		StreamReportReadFailure(pCtx,pDev);` |
|     - | 11779 | `		/* php's buf == NULL: the line is the EMPTY string, not an absence. */` |
|    27 | 11780 | `		PH7_NativeSetAttrStr(pVm,pThis,SFO_L,"",0);` |
|    14 | 11781 | `	}else{` |
|   135 | 11782 | `		if( !bCsv && (SfoFlags(pThis) & SFO_DROP_NEW_LINE) ){` |
|    25 | 11783 | `			if( zLine[n-1] == '\n' ){` |
|    25 | 11784 | `				n--;` |
|    25 | 11785 | `				if( n > 0 && zLine[n-1] == '\r' ){` |
|   ! 0 | 11786 | `					n--;` |
|   ! 0 | 11787 | `				}` |
|    12 | 11788 | `			}` |
|    12 | 11789 | `		}` |
|   135 | 11790 | `		PH7_NativeSetAttrStr(pVm,pThis,SFO_L,zLine,(int)n);` |
|     - | 11791 | `	}` |
|   161 | 11792 | `	PH7_NativeSetAttrInt(pVm,pThis,SFO_LS,SFO_HAS_LINE);` |
|   161 | 11793 | `	PH7_NativeSetAttrInt(pVm,pThis,SFO_K,PH7_NativeAttrInt(pThis,SFO_K) + iLineAdd);` |
|   161 | 11794 | `	return SFO_READ_OK;` |
|    89 | 11795 | `}` |
|     - | 11796 | `/* php's spl_filesystem_file_read: the line number advances only when a line was` |
|     - | 11797 | ` * ALREADY there, which is what makes the first current() answer key 0. */` |
|   136 | 11798 | `static int SfoReadOne(ph7_context *pCtx,int bSilent,int bCsv,sxi32 *pRc)` |
|     1 | 11799 | `{` |
|   137 | 11800 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   137 | 11801 | `	int iAdd = (PH7_NativeAttrInt(pThis,SFO_LS) & SFO_HAS_LINE) ? 1 : 0;` |
|   137 | 11802 | `	return SfoReadEx(pCtx,bSilent,iAdd,bCsv,pRc);` |
|     1 | 11803 | `}` |
|     - | 11804 | `/* php's is_line_empty: an empty line, or -- under READ_CSV\|DROP_NEW_LINE, whose` |
|     - | 11805 | ` * combination does NOT strip the newline -- a line that is only one. */` |
|    56 | 11806 | `static int SfoLineEmpty(ph7_class_instance *pThis)` |
|     1 | 11807 | `{` |
|    57 | 11808 | `	int nLine = 0;` |
|    57 | 11809 | `	const char *zLine = SfoLine(pThis,&nLine);` |
|    57 | 11810 | `	sxi64 iFlags = SfoFlags(pThis);` |
|    57 | 11811 | `	if( nLine == 0 ){` |
|    13 | 11812 | `		return 1;` |
|     - | 11813 | `	}` |
|    45 | 11814 | `	if( (iFlags & SFO_READ_CSV) && (iFlags & SFO_DROP_NEW_LINE) ){` |
|    15 | 11815 | `		return (nLine == 1 && zLine[0] == '\n')` |
|    15 | 11816 | `			\|\| (nLine == 2 && zLine[0] == '\r' && zLine[1] == '\n');` |
|     - | 11817 | `	}` |
|    31 | 11818 | `	return 0;` |
|    29 | 11819 | `}` |
|     - | 11820 | `/*` |
|     - | 11821 | ` * php's spl_filesystem_file_read_csv: read lines until one is not empty (when` |
|     - | 11822 | ` * SKIP_EMPTY says so), then parse the RECORD -- which may run past the line,` |
|     - | 11823 | ` * because an open enclosure carries the newline inside the value.` |
|     - | 11824 | ` */` |
|    28 | 11825 | `static int SfoReadCsv(ph7_context *pCtx,int delim,int encl,int escape,ph7_value *pOut,` |
|     - | 11826 | `	int bSilent,sxi32 *pRc)` |
|     1 | 11827 | `{` |
|    29 | 11828 | `	ph7_vm *pVm = pCtx->pVm;` |
|    29 | 11829 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 11830 | `	io_private *pDev;` |
|     - | 11831 | `	ph7_value *pSlot;` |
|     - | 11832 | `	ph7_value sArray;` |
|     - | 11833 | `	SyBlob sRec;` |
|     - | 11834 | `	PH7_CsvScan sScan;` |
|    29 | 11835 | `	int nLine = 0,rc;` |
|     - | 11836 | `	const char *zLine;` |
|    14 | 11837 | `	do{` |
|    33 | 11838 | `		rc = SfoReadOne(pCtx,bSilent,TRUE,pRc);` |
|    33 | 11839 | `		if( rc != SFO_READ_OK ){` |
|     3 | 11840 | `			return rc;` |
|     - | 11841 | `		}` |
|    31 | 11842 | `	}while( SfoLineEmpty(pThis) && (SfoFlags(pThis) & SFO_SKIP_EMPTY) );` |
|    27 | 11843 | `	PH7_MemObjInit(pVm,&sArray);` |
|    27 | 11844 | `	if( PH7_MemObjToHashmap(&sArray) != SXRET_OK ){` |
|   ! 0 | 11845 | `		PH7_MemObjRelease(&sArray);` |
|   ! 0 | 11846 | `		*pRc = PH7_ContextMemoryError(pCtx);` |
|   ! 0 | 11847 | `		return SFO_READ_THROW;` |
|     - | 11848 | `	}` |
|    27 | 11849 | `	zLine = SfoLine(pThis,&nLine);` |
|    27 | 11850 | `	SyBlobInit(&sRec,&pVm->sAllocator);` |
|    27 | 11851 | `	SyBlobAppend(&sRec,(const void *)zLine,(sxu32)nLine);` |
|    27 | 11852 | `	pDev = SfoDev(pThis);` |
|    27 | 11853 | `	PH7_CsvScanInit(&sScan);` |
|    27 | 11854 | `	while( pDev && PH7_CsvScanOpen(&sScan,(const char *)SyBlobData(&sRec),` |
|    13 | 11855 | `			SyBlobLength(&sRec),delim,encl,escape) ){` |
|     - | 11856 | `		ph7_int64 n;` |
|   ! 0 | 11857 | `		if( SyBlobLength(&sRec) >= (sxu32)SXI32_HIGH ){` |
|   ! 0 | 11858 | `			break;   /* the parser measures in int; stop rather than wrap negative */` |
|     - | 11859 | `		}` |
|   ! 0 | 11860 | `		n = StreamReadLine(pDev,&zLine,0);` |
|   ! 0 | 11861 | `		if( n < 1 ){` |
|   ! 0 | 11862 | `			break;   /* EOF inside the enclosure: php answers what it has */` |
|     - | 11863 | `		}` |
|   ! 0 | 11864 | `		SyBlobAppend(&sRec,(const void *)zLine,(sxu32)n);` |
|   ! 0 | 11865 | `	}` |
|    40 | 11866 | `	PH7_ProcessCsv(&sArray,(const char *)SyBlobData(&sRec),(int)SyBlobLength(&sRec),` |
|    13 | 11867 | `		delim,encl,escape,0);` |
|    27 | 11868 | `	SyBlobRelease(&sRec);` |
|    27 | 11869 | `	pSlot = PH7_NativeAttr(pThis,SFO_Z);` |
|    27 | 11870 | `	if( pSlot ){` |
|    27 | 11871 | `		PH7_MemObjStore(&sArray,pSlot);` |
|    13 | 11872 | `	}` |
|    40 | 11873 | `	PH7_NativeSetAttrInt(pVm,pThis,SFO_LS,` |
|    26 | 11874 | `		(sxi64)(PH7_NativeAttrInt(pThis,SFO_LS) \| SFO_HAS_ZVAL));` |
|    27 | 11875 | `	if( pOut ){` |
|    11 | 11876 | `		ph7_value *pKeep = PH7_NativeAttr(pThis,SFO_Z);` |
|    11 | 11877 | `		if( pKeep ){` |
|    11 | 11878 | `			PH7_MemObjLoad(pKeep,pOut);` |
|     5 | 11879 | `		}` |
|     5 | 11880 | `	}` |
|    27 | 11881 | `	PH7_MemObjRelease(&sArray);` |
|    27 | 11882 | `	return SFO_READ_OK;` |
|    15 | 11883 | `}` |
|     - | 11884 | `/* This instance's three CSV settings. */` |
|    38 | 11885 | `static void SfoCsvControl(ph7_class_instance *pThis,int *pDelim,int *pEncl,int *pEsc)` |
|     1 | 11886 | `{` |
|    39 | 11887 | `	*pDelim = (int)PH7_NativeAttrInt(pThis,SFO_D);` |
|    39 | 11888 | `	*pEncl  = (int)PH7_NativeAttrInt(pThis,SFO_EN);` |
|    39 | 11889 | `	*pEsc   = (int)PH7_NativeAttrInt(pThis,SFO_ES);` |
|    39 | 11890 | `}` |
|     - | 11891 | `/*` |
|     - | 11892 | ` * php's spl_filesystem_file_read_line_ex, whose middle branch is the one only a` |
|     - | 11893 | ` * differential finds: a SUBCLASS that overrides getCurrentLine() drives the` |
|     - | 11894 | ` * iteration, so the line every accessor sees is whatever that method answered` |
|     - | 11895 | ` * -- and the line number is bumped once by the read the method made and again` |
|     - | 11896 | ` * here, which is why such a subclass counts in threes.` |
|     - | 11897 | ` */` |
|   136 | 11898 | `static int SfoReadLineEx(ph7_context *pCtx,int bSilent,sxi32 *pRc)` |
|     1 | 11899 | `{` |
|   137 | 11900 | `	ph7_vm *pVm = pCtx->pVm;` |
|   137 | 11901 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 11902 | `	ph7_class_method *pCur;` |
|     - | 11903 | `	int delim,encl,escape;` |
|   137 | 11904 | `	*pRc = PH7_OK;` |
|   137 | 11905 | `	if( SfoFlags(pThis) & SFO_READ_CSV ){` |
|    19 | 11906 | `		SfoCsvControl(pThis,&delim,&encl,&escape);` |
|    19 | 11907 | `		return SfoReadCsv(pCtx,delim,encl,escape,0,bSilent,pRc);` |
|     - | 11908 | `	}` |
|     - | 11909 | `	/* php compares the resolved method's declaring SCOPE with SplFileObject; a` |
|     - | 11910 | `	 * ph7_class_method carries no such pointer, and the C BODY answers the same` |
|     - | 11911 | `	 * question -- this class's getCurrentLine IS its fgets. */` |
|   119 | 11912 | `	pCur = PH7_ClassExtractMethod(pThis->pClass,"getCurrentLine",sizeof("getCurrentLine")-1);` |
|   119 | 11913 | `	if( pCur && (pCur->sFunc.pNative == 0` |
|   109 | 11914 | `	          \|\| pCur->sFunc.pNative->xFunc != vm_builtin_SplFileObject_fgets) ){` |
|     - | 11915 | `		io_private *pDev;` |
|     - | 11916 | `		ph7_value sRet;` |
|     - | 11917 | `		sxi32 rc;` |
|    19 | 11918 | `		SfoFreeLine(pVm,pThis);` |
|    19 | 11919 | `		pDev = SfoDev(pThis);` |
|    19 | 11920 | `		if( pDev == 0 \|\| PH7_StreamAtEof(pDev) ){` |
|   ! 0 | 11921 | `			if( !bSilent ){` |
|   ! 0 | 11922 | `				int nName = 0;` |
|   ! 0 | 11923 | `				const char *zName = SfiName(pVm,pThis,&nName);` |
|   ! 0 | 11924 | `				*pRc = PH7_VmThrowException(pCtx,"RuntimeException",` |
|   ! 0 | 11925 | `					"Cannot read from file %.*s",nName,zName);` |
|   ! 0 | 11926 | `				return SFO_READ_THROW;` |
|     - | 11927 | `			}` |
|   ! 0 | 11928 | `			return SFO_READ_FAIL;` |
|     - | 11929 | `		}` |
|    19 | 11930 | `		PH7_MemObjInit(pVm,&sRet);` |
|    19 | 11931 | `		rc = PH7_VmCallClassMethod(pVm,pThis,pCur,&sRet,0,0);` |
|    19 | 11932 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 11933 | `			PH7_MemObjRelease(&sRet);` |
|   ! 0 | 11934 | `			*pRc = rc;` |
|   ! 0 | 11935 | `			return SFO_READ_THROW;` |
|     - | 11936 | `		}` |
|    19 | 11937 | `		if( (sRet.iFlags & MEMOBJ_STRING) == 0 ){` |
|     - | 11938 | ``			/* php's own TypeError: the declared `: string` return is checked by`` |
|     - | 11939 | `			 * the CALLER here, because the method may have no declared type. */` |
|   ! 0 | 11940 | `			const char *zGot = PH7_MemObjTypeDump(&sRet);` |
|   ! 0 | 11941 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 11942 | `				"%z::getCurrentLine(): Return value must be of type string, %s returned",` |
|   ! 0 | 11943 | `				&pThis->pClass->sName,zGot);` |
|   ! 0 | 11944 | `			PH7_MemObjRelease(&sRet);` |
|   ! 0 | 11945 | `			return SFO_READ_THROW;` |
|     - | 11946 | `		}` |
|    19 | 11947 | `		if( PH7_NativeAttrInt(pThis,SFO_LS) != 0 ){` |
|    11 | 11948 | `			PH7_NativeSetAttrInt(pVm,pThis,SFO_K,PH7_NativeAttrInt(pThis,SFO_K) + 1);` |
|     5 | 11949 | `		}` |
|    19 | 11950 | `		SfoFreeLine(pVm,pThis);` |
|     - | 11951 | `		{` |
|    19 | 11952 | `			int nRet = 0;` |
|    19 | 11953 | `			const char *zRet = ph7_value_to_string(&sRet,&nRet);` |
|    19 | 11954 | `			PH7_NativeSetAttrStr(pVm,pThis,SFO_L,zRet,nRet);` |
|     - | 11955 | `		}` |
|    19 | 11956 | `		PH7_NativeSetAttrInt(pVm,pThis,SFO_LS,SFO_HAS_LINE);` |
|    19 | 11957 | `		PH7_MemObjRelease(&sRet);` |
|    19 | 11958 | `		return SFO_READ_OK;` |
|     - | 11959 | `	}` |
|   101 | 11960 | `	return SfoReadOne(pCtx,bSilent,FALSE,pRc);` |
|    69 | 11961 | `}` |
|     - | 11962 | `/* php's spl_filesystem_file_read_line: the SKIP_EMPTY loop around it. */` |
|   130 | 11963 | `static int SfoReadLine(ph7_context *pCtx,int bSilent,sxi32 *pRc)` |
|     1 | 11964 | `{` |
|   131 | 11965 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   131 | 11966 | `	int rc = SfoReadLineEx(pCtx,bSilent,pRc);` |
|   137 | 11967 | `	while( (SfoFlags(pThis) & SFO_SKIP_EMPTY) && rc == SFO_READ_OK && SfoLineEmpty(pThis) ){` |
|     7 | 11968 | `		SfoFreeLine(pCtx->pVm,pThis);` |
|     7 | 11969 | `		rc = SfoReadLineEx(pCtx,bSilent,pRc);` |
|     1 | 11970 | `	}` |
|   131 | 11971 | `	return rc;` |
|     1 | 11972 | `}` |
|     - | 11973 | `/* php's spl_filesystem_file_rewind: seek to 0, drop the line, reset the count,` |
|     - | 11974 | ` * and read one ahead when READ_AHEAD asks. */` |
|    30 | 11975 | `static sxi32 SfoRewind(ph7_context *pCtx)` |
|     1 | 11976 | `{` |
|    31 | 11977 | `	ph7_vm *pVm = pCtx->pVm;` |
|    31 | 11978 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    31 | 11979 | `	io_private *pDev = SfoDev(pThis);` |
|    31 | 11980 | `	sxi32 rc = PH7_OK;` |
|    31 | 11981 | `	if( pDev == 0 ){` |
|   ! 0 | 11982 | `		return PH7_VmThrowException(pCtx,"Error","Object not initialized");` |
|     - | 11983 | `	}` |
|    31 | 11984 | `	if( PH7_StreamSeekWrapped(pDev,0,0 /* SEEK_SET */) != PH7_OK ){` |
|   ! 0 | 11985 | `		int nName = 0;` |
|   ! 0 | 11986 | `		const char *zName = SfiName(pVm,pThis,&nName);` |
|   ! 0 | 11987 | `		return PH7_VmThrowException(pCtx,"RuntimeException","Cannot rewind file %.*s",` |
|   ! 0 | 11988 | `			nName,zName);` |
|     - | 11989 | `	}` |
|    31 | 11990 | `	SfoFreeLine(pVm,pThis);` |
|    31 | 11991 | `	PH7_NativeSetAttrInt(pVm,pThis,SFO_K,0);` |
|    31 | 11992 | `	if( SfoFlags(pThis) & SFO_READ_AHEAD ){` |
|     5 | 11993 | `		SfoReadLine(pCtx,TRUE,&rc);` |
|     2 | 11994 | `	}` |
|    31 | 11995 | `	return rc;` |
|    16 | 11996 | `}` |
|     - | 11997 | `/*` |
|     - | 11998 | ` * The open both SplFileObject::__construct and SplFileInfo::openFile() are.` |
|     - | 11999 | ` * php promotes the warning its stream opener would print to a RuntimeException` |
|     - | 12000 | ` * (zend_replace_error_handling), so the text is the warning's -- worded from` |
|     - | 12001 | ` * this context's own qualified name, which is what the caller reports.` |
|     - | 12002 | ` */` |
|   146 | 12003 | `static sxi32 SfoOpen(ph7_context *pCtx,ph7_class_instance *pThis,ph7_value *pPath,` |
|     - | 12004 | `	const char *zMode,int nMode,int bUseInclude,ph7_value *pCtxArg,int iCtxArg)` |
|     1 | 12005 | `{` |
|   147 | 12006 | `	ph7_vm *pVm = pCtx->pVm;` |
|   147 | 12007 | `	const ph7_vfs *pVfs = pVm->pEngine->pVfs;` |
|     - | 12008 | `	phl_stream_ctx *pCtxRes;` |
|     - | 12009 | `	io_private *pDev;` |
|     - | 12010 | `	ph7_value *pSlot;` |
|     - | 12011 | `	const char *zErrUri,*zPath;` |
|     - | 12012 | `	ph7_value *apCtx[4];` |
|   147 | 12013 | `	int nPath = 0,iErr = 0,bThrew = 0,i;` |
|   147 | 12014 | `	zPath = ph7_value_to_string(pPath,&nPath);` |
|     - | 12015 | `	/* PH7_StreamCtxFromArg words its refusal from an argument VECTOR position,` |
|     - | 12016 | `	 * so the context is presented at the index php blames. */` |
|   731 | 12017 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apCtx) ; ++i ){` |
|   585 | 12018 | `		apCtx[i] = pPath;` |
|   293 | 12019 | `	}` |
|   147 | 12020 | `	if( iCtxArg >= 1 && iCtxArg <= (int)SX_ARRAYSIZE(apCtx) ){` |
|   123 | 12021 | `		apCtx[iCtxArg - 1] = pCtxArg;` |
|    61 | 12022 | `	}` |
|     - | 12023 | `	/* php's order, and each step is observable from the one before it: the NUL` |
|     - | 12024 | `	 * is ZPP's and comes first, then the already-open refusal, then the` |
|     - | 12025 | `	 * directory stat, and only then the EMPTY path -- which is the stream` |
|     - | 12026 | `	 * opener's ValueError rather than the constructor's. */` |
|   147 | 12027 | `	if( SyByteFind(zPath,(sxu32)nPath,'\0',0) == SXRET_OK ){` |
|     4 | 12028 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 12029 | `			"%s(): Argument #1 ($filename) must not contain any null bytes",` |
|     1 | 12030 | `			ph7_function_name(pCtx));` |
|     - | 12031 | `	}` |
|   145 | 12032 | `	if( SfoDev(pThis) != 0 ){` |
|     5 | 12033 | `		return PH7_VmThrowException(pCtx,"Error","Cannot call constructor twice");` |
|     - | 12034 | `	}` |
|   141 | 12035 | `	if( pVfs && pVfs->xIsdir && nPath > 0 && nPath < 4096 ){` |
|     - | 12036 | `		char zBuf[4096];` |
|   139 | 12037 | `		SyMemcpy(zPath,zBuf,(sxu32)nPath);` |
|   139 | 12038 | `		zBuf[nPath] = 0;` |
|   139 | 12039 | `		if( pVfs->xIsdir(zBuf) == PH7_OK ){` |
|     5 | 12040 | `			return PH7_VmThrowException(pCtx,"LogicException",` |
|     - | 12041 | `				"Cannot use SplFileObject with directories");` |
|     - | 12042 | `		}` |
|    67 | 12043 | `	}` |
|   137 | 12044 | `	if( nPath < 1 ){` |
|     3 | 12045 | `		return PH7_VmThrowException(pCtx,"ValueError","Path must not be empty");` |
|     - | 12046 | `	}` |
|   135 | 12047 | `	pCtxRes = pCtxArg` |
|     6 | 12048 | `		? PH7_StreamCtxFromArg(pCtx,iCtxArg,apCtx,iCtxArg - 1,"$context",0,&bThrew)` |
|   131 | 12049 | `		: PH7_StreamCtxDefault(pVm);` |
|   135 | 12050 | `	if( bThrew ){` |
|     5 | 12051 | `		return PH7_OK;` |
|     - | 12052 | `	}` |
|   131 | 12053 | `	pDev = PH7_StreamOpenPath(pCtx,pPath,zMode,nMode,bUseInclude,pCtxRes,pCtxArg,` |
|     - | 12054 | `		&iErr,&zErrUri);` |
|   131 | 12055 | `	if( pDev == 0 ){` |
|     7 | 12056 | `		if( iErr == PH7_STREAM_OPEN_NODEVICE ){` |
|   ! 0 | 12057 | `			int nScheme = 0;` |
|   ! 0 | 12058 | `			if( PH7_VmStreamDeviceIsRemoteHost(zPath,nPath,&nScheme) ){` |
|   ! 0 | 12059 | `				return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     - | 12060 | `					"%s(%.*s): Failed to open stream: no suitable wrapper could be found",` |
|   ! 0 | 12061 | `					ph7_function_name(pCtx),nPath,zPath);` |
|     - | 12062 | `			}` |
|   ! 0 | 12063 | `			return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     - | 12064 | `				"%s(): Unable to find the wrapper \"%.*s\" - did you forget to enable it "` |
|   ! 0 | 12065 | `				"when you configured PHP?",ph7_function_name(pCtx),nScheme,zPath);` |
|     - | 12066 | `		}` |
|     7 | 12067 | `		if( iErr == PH7_STREAM_OPEN_BADMODE ){` |
|     3 | 12068 | `			return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     - | 12069 | ``				"%s(%s): Failed to open stream: `%.*s' is not a valid mode for fopen",`` |
|     2 | 12070 | `				ph7_function_name(pCtx),zErrUri ? zErrUri : "",nMode,zMode);` |
|     - | 12071 | `		}` |
|     5 | 12072 | `		if( iErr == PH7_STREAM_OPEN_NOMEM ){` |
|   ! 0 | 12073 | `			return PH7_ContextMemoryError(pCtx);` |
|     - | 12074 | `		}` |
|     5 | 12075 | `		return PH7_VmThrowException(pCtx,"RuntimeException",` |
|     2 | 12076 | `			"%s(%s): Failed to open stream: %s",ph7_function_name(pCtx),` |
|     4 | 12077 | `			zErrUri ? zErrUri : "",VfsStrerror(errno));` |
|     - | 12078 | `	}` |
|   125 | 12079 | `	pSlot = PH7_NativeAttr(pThis,SFO_H);` |
|   125 | 12080 | `	if( pSlot == 0 ){` |
|   ! 0 | 12081 | `		PH7_StreamCloseHandle(pDev->pStream,pDev->pHandle);` |
|   ! 0 | 12082 | `		MarkIOPrivateClosed(pDev);` |
|   ! 0 | 12083 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 12084 | `	}` |
|   125 | 12085 | `	PH7_MemObjRelease(pSlot);` |
|   125 | 12086 | `	pSlot->x.pOther = pDev;` |
|   125 | 12087 | `	MemObjSetType(pSlot,MEMOBJ_RES);` |
|   125 | 12088 | `	PH7_NativeSetAttrStr(pVm,pThis,SFO_M,zMode,nMode);` |
|   125 | 12089 | `	SfiSetName(pVm,pThis,zPath,nPath);` |
|   125 | 12090 | `	return PH7_OK;` |
|    74 | 12091 | `}` |
|     - | 12092 | `/* The class's teardown: php closes the stream with the OBJECT, which is when a` |
|     - | 12093 | ` * file written through one gets its last bytes. */` |
|   134 | 12094 | `static void SfoRelease(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 | 12095 | `{` |
|   135 | 12096 | `	io_private *pDev = SfoDev(pThis);` |
|    67 | 12097 | `	SXUNUSED(pVm);` |
|   135 | 12098 | `	if( pDev == 0 ){` |
|    15 | 12099 | `		return;` |
|     - | 12100 | `	}` |
|   121 | 12101 | `	PH7_StreamFilterReleaseChains(pDev);` |
|   121 | 12102 | `	PH7_StreamCloseHandle(pDev->pStream,pDev->pHandle);` |
|   121 | 12103 | `	MarkIOPrivateClosed(pDev);` |
|    68 | 12104 | `}` |
|     - | 12105 | `/*` |
|     - | 12106 | ` * SplFileObject::__construct(string $filename, string $mode = 'r',` |
|     - | 12107 | ` *                            bool $useIncludePath = false, $context = null)` |
|     - | 12108 | ` */` |
|    96 | 12109 | `static int vm_builtin_SplFileObject_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12110 | `{` |
|    97 | 12111 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    97 | 12112 | `	const char *zMode = "r";` |
|    97 | 12113 | `	int nMode = 1;` |
|    97 | 12114 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|   ! 0 | 12115 | `		return PH7_OK;` |
|     - | 12116 | `	}` |
|    97 | 12117 | `	if( nArg > 1 ){` |
|    13 | 12118 | `		zMode = ph7_value_to_string(apArg[1],&nMode);` |
|     6 | 12119 | `	}` |
|   147 | 12120 | `	return SfoOpen(pCtx,pThis,apArg[0],zMode,nMode,` |
|    49 | 12121 | `		nArg > 2 ? ph7_value_to_bool(apArg[2]) : FALSE,` |
|    49 | 12122 | `		nArg > 3 && (apArg[3]->iFlags & MEMOBJ_NULL) == 0 ? apArg[3] : 0,4);` |
|    49 | 12123 | `}` |
|     - | 12124 | `/*` |
|     - | 12125 | ` * SplTempFileObject::__construct(int $maxMemory = 2097152)` |
|     - | 12126 | ` *` |
|     - | 12127 | `` * php builds a php:// URI from the argument and opens it `wb`, and the three`` |
|     - | 12128 | ` * arms are visible from outside because getPathname() answers the URI: a` |
|     - | 12129 | `` * NEGATIVE budget is `php://memory` (never spilled to disk), a budget NAMED is`` |
|     - | 12130 | `` * `php://temp/maxmemory:N` -- including 0, which spills immediately -- and no`` |
|     - | 12131 | `` * argument at all is a plain `php://temp` carrying php's own default. The path`` |
|     - | 12132 | `` * is the EMPTY string rather than the `php:/` a URI's last slash would cut.`` |
|     - | 12133 | ` */` |
|    24 | 12134 | `static int vm_builtin_SplTempFileObject_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12135 | `{` |
|    25 | 12136 | `	ph7_vm *pVm = pCtx->pVm;` |
|    25 | 12137 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    25 | 12138 | `	sxi64 iMax = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|     - | 12139 | `	ph7_value sUri;` |
|     - | 12140 | `	sxi32 rc;` |
|    25 | 12141 | `	if( pThis == 0 ){` |
|   ! 0 | 12142 | `		return PH7_OK;` |
|     - | 12143 | `	}` |
|    25 | 12144 | `	PH7_MemObjInitFromString(pVm,&sUri,0);` |
|    25 | 12145 | `	if( iMax < 0 ){` |
|     3 | 12146 | `		PH7_MemObjStringAppend(&sUri,"php://memory",sizeof("php://memory")-1);` |
|    24 | 12147 | `	}else if( nArg > 0 ){` |
|     - | 12148 | `		char zBuf[64];` |
|     5 | 12149 | `		int n = SyBufferFormat(zBuf,sizeof(zBuf),"php://temp/maxmemory:%qd",iMax);` |
|     5 | 12150 | `		PH7_MemObjStringAppend(&sUri,zBuf,(sxu32)n);` |
|     3 | 12151 | `	}else{` |
|    19 | 12152 | `		PH7_MemObjStringAppend(&sUri,"php://temp",sizeof("php://temp")-1);` |
|     - | 12153 | `	}` |
|    25 | 12154 | `	rc = SfoOpen(pCtx,pThis,&sUri,"wb",2,FALSE,0,0);` |
|    25 | 12155 | `	PH7_MemObjRelease(&sUri);` |
|    25 | 12156 | `	if( rc == PH7_OK ){` |
|    23 | 12157 | `		PH7_NativeSetAttrStr(pVm,pThis,SFI_P,"",0);` |
|    11 | 12158 | `	}` |
|    25 | 12159 | `	return rc;` |
|    13 | 12160 | `}` |
|     - | 12161 | `/* The guard every method below opens with: the handle, or the refusal. */` |
|   466 | 12162 | `static io_private * SfoNeed(ph7_context *pCtx,sxi32 *pRc)` |
|     1 | 12163 | `{` |
|   467 | 12164 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   467 | 12165 | `	io_private *pDev = SfoDev(pThis);` |
|   467 | 12166 | `	*pRc = PH7_OK;` |
|   467 | 12167 | `	if( pDev == 0 ){` |
|     5 | 12168 | `		*pRc = PH7_VmThrowException(pCtx,"Error",` |
|     - | 12169 | `			"The parent constructor was not called: the object is in an invalid state");` |
|     2 | 12170 | `	}` |
|   467 | 12171 | `	return pDev;` |
|     1 | 12172 | `}` |
|    26 | 12173 | `static int vm_builtin_SplFileObject_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12174 | `{` |
|     - | 12175 | `	sxi32 rc;` |
|    13 | 12176 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    27 | 12177 | `	if( SfoNeed(pCtx,&rc) == 0 ){` |
|   ! 0 | 12178 | `		return rc;` |
|     - | 12179 | `	}` |
|    27 | 12180 | `	return SfoRewind(pCtx);` |
|    14 | 12181 | `}` |
|    18 | 12182 | `static int vm_builtin_SplFileObject_eof(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12183 | `{` |
|     - | 12184 | `	sxi32 rc;` |
|    19 | 12185 | `	io_private *pDev = SfoNeed(pCtx,&rc);` |
|     9 | 12186 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    19 | 12187 | `	if( pDev == 0 ){` |
|   ! 0 | 12188 | `		return rc;` |
|     - | 12189 | `	}` |
|    19 | 12190 | `	ph7_result_bool(pCtx,PH7_StreamAtEof(pDev) != 0);` |
|    19 | 12191 | `	return PH7_OK;` |
|    10 | 12192 | `}` |
|     - | 12193 | `/* php's valid(): the LINE decides under READ_AHEAD, the stream otherwise. */` |
|   122 | 12194 | `static int vm_builtin_SplFileObject_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12195 | `{` |
|   123 | 12196 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 12197 | `	sxi32 rc;` |
|     - | 12198 | `	io_private *pDev;` |
|    61 | 12199 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   123 | 12200 | `	if( SfoIsChecked(pCtx->pVm,pThis) && SfoDev(pThis) == 0 ){` |
|   ! 0 | 12201 | `		return PH7_VmThrowException(pCtx,"Error",` |
|     - | 12202 | `			"The parent constructor was not called: the object is in an invalid state");` |
|     - | 12203 | `	}` |
|   123 | 12204 | `	if( SfoFlags(pThis) & SFO_READ_AHEAD ){` |
|    25 | 12205 | `		ph7_result_bool(pCtx,PH7_NativeAttrInt(pThis,SFO_LS) != 0);` |
|    25 | 12206 | `		return PH7_OK;` |
|     - | 12207 | `	}` |
|    99 | 12208 | `	pDev = SfoNeed(pCtx,&rc);` |
|    99 | 12209 | `	if( pDev == 0 ){` |
|   ! 0 | 12210 | `		return rc;` |
|     - | 12211 | `	}` |
|    99 | 12212 | `	ph7_result_bool(pCtx,PH7_StreamAtEof(pDev) == 0);` |
|    99 | 12213 | `	return PH7_OK;` |
|    62 | 12214 | `}` |
|     - | 12215 | `/* php's fgets(), which is also this class's getCurrentLine(): a LOUD read that` |
|     - | 12216 | ` * always advances the line number. */` |
|    42 | 12217 | `static int vm_builtin_SplFileObject_fgets(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12218 | `{` |
|    43 | 12219 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 12220 | `	sxi32 rc;` |
|    43 | 12221 | `	int nLine = 0;` |
|     - | 12222 | `	const char *zLine;` |
|    21 | 12223 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    43 | 12224 | `	if( SfoNeed(pCtx,&rc) == 0 ){` |
|     3 | 12225 | `		return rc;` |
|     - | 12226 | `	}` |
|    41 | 12227 | `	if( SfoReadEx(pCtx,FALSE,1,FALSE,&rc) != SFO_READ_OK ){` |
|     3 | 12228 | `		return rc;` |
|     - | 12229 | `	}` |
|    39 | 12230 | `	zLine = SfoLine(pThis,&nLine);` |
|    39 | 12231 | `	ph7_result_string(pCtx,zLine,nLine);` |
|    39 | 12232 | `	return PH7_OK;` |
|    22 | 12233 | `}` |
|     - | 12234 | `/*` |
|     - | 12235 | ` * php's current(): read one line if nothing is held, then pick between the two` |
|     - | 12236 | ` * current values -- the STRING wins unless READ_CSV has an array beside it.` |
|     - | 12237 | ` */` |
|   110 | 12238 | `static int vm_builtin_SplFileObject_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12239 | `{` |
|   111 | 12240 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 12241 | `	sxi32 rc;` |
|     - | 12242 | `	sxi64 iHas;` |
|    55 | 12243 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   111 | 12244 | `	if( SfoNeed(pCtx,&rc) == 0 ){` |
|     3 | 12245 | `		return rc;` |
|     - | 12246 | `	}` |
|   109 | 12247 | `	if( PH7_NativeAttrInt(pThis,SFO_LS) == 0 ){` |
|    87 | 12248 | `		if( SfoReadLine(pCtx,TRUE,&rc) == SFO_READ_THROW ){` |
|   ! 0 | 12249 | `			return rc;` |
|     - | 12250 | `		}` |
|    43 | 12251 | `	}` |
|   109 | 12252 | `	iHas = PH7_NativeAttrInt(pThis,SFO_LS);` |
|   108 | 12253 | `	if( (iHas & SFO_HAS_LINE)` |
|   147 | 12254 | `	 && (!(SfoFlags(pThis) & SFO_READ_CSV) \|\| (iHas & SFO_HAS_ZVAL) == 0) ){` |
|    85 | 12255 | `		int nLine = 0;` |
|    85 | 12256 | `		const char *zLine = SfoLine(pThis,&nLine);` |
|    85 | 12257 | `		ph7_result_string(pCtx,zLine,nLine);` |
|    67 | 12258 | `	}else if( iHas & SFO_HAS_ZVAL ){` |
|    17 | 12259 | `		ph7_value *pZ = PH7_NativeAttr(pThis,SFO_Z);` |
|    17 | 12260 | `		if( pZ ){` |
|    17 | 12261 | `			ph7_result_value(pCtx,pZ);` |
|     9 | 12262 | `		}else{` |
|   ! 0 | 12263 | `			ph7_result_bool(pCtx,0);` |
|     - | 12264 | `		}` |
|     9 | 12265 | `	}else{` |
|     9 | 12266 | `		ph7_result_bool(pCtx,0);` |
|     - | 12267 | `	}` |
|   109 | 12268 | `	return PH7_OK;` |
|    56 | 12269 | `}` |
|     - | 12270 | `/* php's key(): the stored count, deliberately WITHOUT reading ahead -- which is` |
|     - | 12271 | ` * what lets fgetc() count newlines instead of lines. */` |
|   126 | 12272 | `static int vm_builtin_SplFileObject_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12273 | `{` |
|     - | 12274 | `	sxi32 rcChk;` |
|    63 | 12275 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   127 | 12276 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|     3 | 12277 | `		return rcChk;` |
|     - | 12278 | `	}` |
|   125 | 12279 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(PH7_ContextThis(pCtx),SFO_K));` |
|   125 | 12280 | `	return PH7_OK;` |
|    64 | 12281 | `}` |
|   100 | 12282 | `static int vm_builtin_SplFileObject_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12283 | `{` |
|   101 | 12284 | `	ph7_vm *pVm = pCtx->pVm;` |
|   101 | 12285 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 12286 | `	sxi32 rc;` |
|    50 | 12287 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   101 | 12288 | `	if( SfoNeed(pCtx,&rc) == 0 ){` |
|   ! 0 | 12289 | `		return rc;` |
|     - | 12290 | `	}` |
|   101 | 12291 | `	SfoFreeLine(pVm,pThis);` |
|   101 | 12292 | `	if( SfoFlags(pThis) & SFO_READ_AHEAD ){` |
|    21 | 12293 | `		if( SfoReadLine(pCtx,TRUE,&rc) == SFO_READ_THROW ){` |
|   ! 0 | 12294 | `			return rc;` |
|     - | 12295 | `		}` |
|    10 | 12296 | `	}` |
|   101 | 12297 | `	PH7_NativeSetAttrInt(pVm,pThis,SFO_K,PH7_NativeAttrInt(pThis,SFO_K) + 1);` |
|   101 | 12298 | `	return PH7_OK;` |
|    51 | 12299 | `}` |
|    18 | 12300 | `static int vm_builtin_SplFileObject_setFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12301 | `{` |
|     - | 12302 | `	sxi32 rcChk;` |
|    19 | 12303 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 12304 | `		return rcChk;` |
|     - | 12305 | `	}` |
|     - | 12306 | `	/* php stores the WHOLE word and masks only on the way out. */` |
|    37 | 12307 | `	PH7_NativeSetAttrInt(pCtx->pVm,PH7_ContextThis(pCtx),SFO_FL,` |
|    18 | 12308 | `		nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0);` |
|    19 | 12309 | `	return PH7_OK;` |
|    10 | 12310 | `}` |
|     4 | 12311 | `static int vm_builtin_SplFileObject_getFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12312 | `{` |
|     - | 12313 | `	sxi32 rcChk;` |
|     2 | 12314 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 | 12315 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 12316 | `		return rcChk;` |
|     - | 12317 | `	}` |
|     5 | 12318 | `	ph7_result_int64(pCtx,SfoFlags(PH7_ContextThis(pCtx)) & SFO_FLAGS_MASK);` |
|     5 | 12319 | `	return PH7_OK;` |
|     3 | 12320 | `}` |
|     4 | 12321 | `static int vm_builtin_SplFileObject_setMaxLineLen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12322 | `{` |
|     5 | 12323 | `	sxi64 iLen = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|     - | 12324 | `	sxi32 rcChk;` |
|     5 | 12325 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 12326 | `		return rcChk;` |
|     - | 12327 | `	}` |
|     5 | 12328 | `	if( iLen < 0 ){` |
|     4 | 12329 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 12330 | `			"%s(): Argument #1 ($maxLength) must be greater than or equal to 0",` |
|     1 | 12331 | `			ph7_function_name(pCtx));` |
|     - | 12332 | `	}` |
|     3 | 12333 | `	PH7_NativeSetAttrInt(pCtx->pVm,PH7_ContextThis(pCtx),SFO_ML,iLen);` |
|     3 | 12334 | `	return PH7_OK;` |
|     3 | 12335 | `}` |
|     2 | 12336 | `static int vm_builtin_SplFileObject_getMaxLineLen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12337 | `{` |
|     - | 12338 | `	sxi32 rcChk;` |
|     1 | 12339 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     3 | 12340 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 12341 | `		return rcChk;` |
|     - | 12342 | `	}` |
|     3 | 12343 | `	ph7_result_int64(pCtx,PH7_NativeAttrInt(PH7_ContextThis(pCtx),SFO_ML));` |
|     3 | 12344 | `	return PH7_OK;` |
|     2 | 12345 | `}` |
|     - | 12346 | `/* php's RecursiveIterator half: a file has no children and says so. */` |
|     2 | 12347 | `static int vm_builtin_SplFileObject_hasChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12348 | `{` |
|     - | 12349 | `	sxi32 rcChk;` |
|     1 | 12350 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     3 | 12351 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 12352 | `		return rcChk;` |
|     - | 12353 | `	}` |
|     3 | 12354 | `	ph7_result_bool(pCtx,0);` |
|     3 | 12355 | `	return PH7_OK;` |
|     2 | 12356 | `}` |
|     2 | 12357 | `static int vm_builtin_SplFileObject_getChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12358 | `{` |
|     - | 12359 | `	sxi32 rcChk;` |
|     1 | 12360 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     3 | 12361 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 12362 | `		return rcChk;` |
|     - | 12363 | `	}` |
|     3 | 12364 | `	ph7_result_null(pCtx);` |
|     3 | 12365 | `	return PH7_OK;` |
|     2 | 12366 | `}` |
|     - | 12367 | `/*` |
|     - | 12368 | ` * php's fgetcsv(): the three arguments override this instance's settings for` |
|     - | 12369 | ` * ONE call, and the argument NUMBERS are this spelling's own.` |
|     - | 12370 | ` */` |
|    12 | 12371 | `static int vm_builtin_SplFileObject_fgetcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12372 | `{` |
|    13 | 12373 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 12374 | `	int delim,encl,escape;` |
|     - | 12375 | `	sxi32 rc;` |
|    13 | 12376 | `	if( SfoNeed(pCtx,&rc) == 0 ){` |
|   ! 0 | 12377 | `		return rc;` |
|     - | 12378 | `	}` |
|    13 | 12379 | `	SfoCsvControl(pThis,&delim,&encl,&escape);` |
|    13 | 12380 | `	if( nArg > 0 ){` |
|    13 | 12381 | `		rc = PH7_CsvCharArg(pCtx,apArg[0],1,"separator",0,&delim);` |
|    13 | 12382 | `		if( rc != PH7_OK ){` |
|   ! 0 | 12383 | `			return rc;` |
|     - | 12384 | `		}` |
|     6 | 12385 | `	}` |
|    13 | 12386 | `	if( nArg > 1 ){` |
|    13 | 12387 | `		rc = PH7_CsvCharArg(pCtx,apArg[1],2,"enclosure",0,&encl);` |
|    13 | 12388 | `		if( rc != PH7_OK ){` |
|     3 | 12389 | `			return rc;` |
|     - | 12390 | `		}` |
|     5 | 12391 | `	}` |
|    11 | 12392 | `	if( nArg > 2 ){` |
|    11 | 12393 | `		rc = PH7_CsvCharArg(pCtx,apArg[2],3,"escape",1,&escape);` |
|    11 | 12394 | `		if( rc != PH7_OK ){` |
|   ! 0 | 12395 | `			return rc;` |
|     - | 12396 | `		}` |
|     5 | 12397 | `	}` |
|     - | 12398 | `	{` |
|     - | 12399 | `		ph7_value sOut;` |
|     - | 12400 | `		int r;` |
|    11 | 12401 | `		PH7_MemObjInit(pCtx->pVm,&sOut);` |
|    11 | 12402 | `		r = SfoReadCsv(pCtx,delim,encl,escape,&sOut,TRUE,&rc);` |
|    11 | 12403 | `		if( r == SFO_READ_OK ){` |
|    11 | 12404 | `			ph7_result_value(pCtx,&sOut);` |
|     5 | 12405 | `		}else if( r == SFO_READ_FAIL ){` |
|   ! 0 | 12406 | `			ph7_result_bool(pCtx,0);` |
|   ! 0 | 12407 | `		}` |
|    11 | 12408 | `		PH7_MemObjRelease(&sOut);` |
|    11 | 12409 | `		return r == SFO_READ_THROW ? rc : PH7_OK;` |
|     - | 12410 | `	}` |
|     7 | 12411 | `}` |
|     - | 12412 | `/* php's fputcsv(): the same overrides, at the positions THIS method numbers. */` |
|     4 | 12413 | `static int vm_builtin_SplFileObject_fputcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12414 | `{` |
|     5 | 12415 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 12416 | `	ph7_value *apOut[6];` |
|     - | 12417 | `	ph7_value sDelim,sEncl,sEsc;` |
|     - | 12418 | `	int delim,encl,escape,nOut,r;` |
|     - | 12419 | `	sxi32 rc;` |
|     5 | 12420 | `	if( SfoNeed(pCtx,&rc) == 0 ){` |
|   ! 0 | 12421 | `		return rc;` |
|     - | 12422 | `	}` |
|     5 | 12423 | `	if( nArg < 1 ){` |
|   ! 0 | 12424 | `		return PH7_OK;` |
|     - | 12425 | `	}` |
|     5 | 12426 | `	SfoCsvControl(pThis,&delim,&encl,&escape);` |
|     5 | 12427 | `	if( nArg > 1 ){` |
|     5 | 12428 | `		rc = PH7_CsvCharArg(pCtx,apArg[1],2,"separator",0,&delim);` |
|     5 | 12429 | `		if( rc != PH7_OK ){` |
|   ! 0 | 12430 | `			return rc;` |
|     - | 12431 | `		}` |
|     2 | 12432 | `	}` |
|     5 | 12433 | `	if( nArg > 2 ){` |
|     5 | 12434 | `		rc = PH7_CsvCharArg(pCtx,apArg[2],3,"enclosure",0,&encl);` |
|     5 | 12435 | `		if( rc != PH7_OK ){` |
|     3 | 12436 | `			return rc;` |
|     - | 12437 | `		}` |
|     1 | 12438 | `	}` |
|     3 | 12439 | `	if( nArg > 3 ){` |
|     3 | 12440 | `		rc = PH7_CsvCharArg(pCtx,apArg[3],4,"escape",1,&escape);` |
|     3 | 12441 | `		if( rc != PH7_OK ){` |
|   ! 0 | 12442 | `			return rc;` |
|     - | 12443 | `		}` |
|     1 | 12444 | `	}` |
|     - | 12445 | `	/* The writer lives in vfs_stream.c and takes the three as STRINGS, at` |
|     - | 12446 | `	 * fputcsv()'s own positions; the settings resolved above are handed over in` |
|     - | 12447 | `	 * that shape rather than re-parsed there. */` |
|     3 | 12448 | `	PH7_MemObjInitFromString(pCtx->pVm,&sDelim,0);` |
|     3 | 12449 | `	PH7_MemObjInitFromString(pCtx->pVm,&sEncl,0);` |
|     3 | 12450 | `	PH7_MemObjInitFromString(pCtx->pVm,&sEsc,0);` |
|     - | 12451 | `	{` |
|     3 | 12452 | `		char c = (char)delim;` |
|     3 | 12453 | `		PH7_MemObjStringAppend(&sDelim,&c,sizeof(char));` |
|     3 | 12454 | `		c = (char)encl;` |
|     3 | 12455 | `		PH7_MemObjStringAppend(&sEncl,&c,sizeof(char));` |
|     3 | 12456 | `		if( escape != PH7_CSV_NO_ESCAPE ){` |
|     3 | 12457 | `			c = (char)escape;` |
|     3 | 12458 | `			PH7_MemObjStringAppend(&sEsc,&c,sizeof(char));` |
|     1 | 12459 | `		}` |
|     - | 12460 | `	}` |
|     3 | 12461 | `	apOut[0] = PH7_NativeAttr(pThis,SFO_H);` |
|     3 | 12462 | `	apOut[1] = apArg[0];` |
|     3 | 12463 | `	apOut[2] = &sDelim;` |
|     3 | 12464 | `	apOut[3] = &sEncl;` |
|     3 | 12465 | `	apOut[4] = &sEsc;` |
|     3 | 12466 | `	nOut = 5;` |
|     3 | 12467 | `	if( nArg > 4 ){` |
|     3 | 12468 | `		apOut[5] = apArg[4];` |
|     3 | 12469 | `		nOut = 6;` |
|     1 | 12470 | `	}` |
|     3 | 12471 | `	r = PH7_builtin_fputcsv(pCtx,nOut,apOut);` |
|     3 | 12472 | `	PH7_MemObjRelease(&sDelim);` |
|     3 | 12473 | `	PH7_MemObjRelease(&sEncl);` |
|     3 | 12474 | `	PH7_MemObjRelease(&sEsc);` |
|     3 | 12475 | `	return r;` |
|     3 | 12476 | `}` |
|     4 | 12477 | `static int vm_builtin_SplFileObject_setCsvControl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12478 | `{` |
|     5 | 12479 | `	ph7_vm *pVm = pCtx->pVm;` |
|     5 | 12480 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     5 | 12481 | `	int delim = ',',encl = '"',escape = '\\';` |
|     - | 12482 | `	sxi32 rc,rcChk;` |
|     5 | 12483 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 12484 | `		return rcChk;` |
|     - | 12485 | `	}` |
|     5 | 12486 | `	if( nArg > 0 ){` |
|     5 | 12487 | `		rc = PH7_CsvCharArg(pCtx,apArg[0],1,"separator",0,&delim);` |
|     5 | 12488 | `		if( rc != PH7_OK ){` |
|     3 | 12489 | `			return rc;` |
|     - | 12490 | `		}` |
|     1 | 12491 | `	}` |
|     3 | 12492 | `	if( nArg > 1 ){` |
|     3 | 12493 | `		rc = PH7_CsvCharArg(pCtx,apArg[1],2,"enclosure",0,&encl);` |
|     3 | 12494 | `		if( rc != PH7_OK ){` |
|   ! 0 | 12495 | `			return rc;` |
|     - | 12496 | `		}` |
|     1 | 12497 | `	}` |
|     3 | 12498 | `	if( nArg > 2 ){` |
|     3 | 12499 | `		rc = PH7_CsvCharArg(pCtx,apArg[2],3,"escape",1,&escape);` |
|     3 | 12500 | `		if( rc != PH7_OK ){` |
|   ! 0 | 12501 | `			return rc;` |
|     - | 12502 | `		}` |
|     - | 12503 | `		/* Naming the escape at all is what stops php's deprecation notice. */` |
|     3 | 12504 | `		PH7_NativeSetAttrInt(pVm,pThis,SFO_ED,0);` |
|     1 | 12505 | `	}` |
|     3 | 12506 | `	PH7_NativeSetAttrInt(pVm,pThis,SFO_D,delim);` |
|     3 | 12507 | `	PH7_NativeSetAttrInt(pVm,pThis,SFO_EN,encl);` |
|     3 | 12508 | `	PH7_NativeSetAttrInt(pVm,pThis,SFO_ES,escape);` |
|     3 | 12509 | `	return PH7_OK;` |
|     3 | 12510 | `}` |
|     4 | 12511 | `static int vm_builtin_SplFileObject_getCsvControl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12512 | `{` |
|     5 | 12513 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 12514 | `	ph7_value *pArr,*pVal;` |
|     - | 12515 | `	int delim,encl,escape;` |
|     - | 12516 | `	char c;` |
|     - | 12517 | `	sxi32 rcChk;` |
|     2 | 12518 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 | 12519 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 12520 | `		return rcChk;` |
|     - | 12521 | `	}` |
|     5 | 12522 | `	SfoCsvControl(pThis,&delim,&encl,&escape);` |
|     5 | 12523 | `	pArr = ph7_context_new_array(pCtx);` |
|     5 | 12524 | `	pVal = ph7_context_new_scalar(pCtx);` |
|     5 | 12525 | `	if( pArr == 0 \|\| pVal == 0 ){` |
|   ! 0 | 12526 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 12527 | `	}` |
|     5 | 12528 | `	c = (char)delim;` |
|     5 | 12529 | `	ph7_value_string(pVal,&c,1);` |
|     5 | 12530 | `	ph7_array_add_elem(pArr,0,pVal);` |
|     5 | 12531 | `	ph7_value_reset_string_cursor(pVal);` |
|     5 | 12532 | `	c = (char)encl;` |
|     5 | 12533 | `	ph7_value_string(pVal,&c,1);` |
|     5 | 12534 | `	ph7_array_add_elem(pArr,0,pVal);` |
|     5 | 12535 | `	ph7_value_reset_string_cursor(pVal);` |
|     - | 12536 | `	/* A disabled escape is reported as the EMPTY string, not as a byte. */` |
|     5 | 12537 | `	if( escape != PH7_CSV_NO_ESCAPE ){` |
|     3 | 12538 | `		c = (char)escape;` |
|     3 | 12539 | `		ph7_value_string(pVal,&c,1);` |
|     2 | 12540 | `	}else{` |
|     3 | 12541 | `		ph7_value_string(pVal,"",0);` |
|     - | 12542 | `	}` |
|     5 | 12543 | `	ph7_array_add_elem(pArr,0,pVal);` |
|     5 | 12544 | `	ph7_result_value(pCtx,pArr);` |
|     5 | 12545 | `	return PH7_OK;` |
|     3 | 12546 | `}` |
|     - | 12547 | `/*` |
|     - | 12548 | ` * The eight methods that are the corresponding builtin over this object's own` |
|     - | 12549 | ` * handle. Each pre-validates what php validates IN THE METHOD -- the argument` |
|     - | 12550 | ` * numbers are the method's, one lower than the function's -- and then hands the` |
|     - | 12551 | ` * work to the one implementation there is. Every diagnostic the builtin raises` |
|     - | 12552 | ` * itself is worded from ph7_function_name(), which in here is the qualified` |
|     - | 12553 | ` * method name php prints.` |
|     - | 12554 | ` */` |
|    32 | 12555 | `static int SfoDelegate(ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|     - | 12556 | `	int (*xFunc)(ph7_context *,int,ph7_value **))` |
|     1 | 12557 | `{` |
|     - | 12558 | `	ph7_value *apOut[8];` |
|     - | 12559 | `	sxi32 rc;` |
|     - | 12560 | `	int i;` |
|    33 | 12561 | `	if( SfoNeed(pCtx,&rc) == 0 ){` |
|   ! 0 | 12562 | `		return rc;` |
|     - | 12563 | `	}` |
|    33 | 12564 | `	if( nArg > (int)(SX_ARRAYSIZE(apOut) - 1) ){` |
|   ! 0 | 12565 | `		nArg = (int)(SX_ARRAYSIZE(apOut) - 1);` |
|   ! 0 | 12566 | `	}` |
|    33 | 12567 | `	apOut[0] = PH7_NativeAttr(PH7_ContextThis(pCtx),SFO_H);` |
|    61 | 12568 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    29 | 12569 | `		apOut[i+1] = apArg[i];` |
|    15 | 12570 | `	}` |
|    33 | 12571 | `	return xFunc(pCtx,nArg + 1,apOut);` |
|    17 | 12572 | `}` |
|   ! 0 | 12573 | `static int vm_builtin_SplFileObject_fflush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 12574 | `{` |
|   ! 0 | 12575 | `	return SfoDelegate(pCtx,nArg,apArg,PH7_builtin_fflush);` |
|   ! 0 | 12576 | `}` |
|     6 | 12577 | `static int vm_builtin_SplFileObject_ftell(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12578 | `{` |
|     7 | 12579 | `	return SfoDelegate(pCtx,nArg,apArg,PH7_builtin_ftell);` |
|     1 | 12580 | `}` |
|     2 | 12581 | `static int vm_builtin_SplFileObject_fstat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12582 | `{` |
|     3 | 12583 | `	return SfoDelegate(pCtx,nArg,apArg,PH7_builtin_fstat);` |
|     1 | 12584 | `}` |
|     2 | 12585 | `static int vm_builtin_SplFileObject_fpassthru(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12586 | `{` |
|     3 | 12587 | `	return SfoDelegate(pCtx,nArg,apArg,PH7_builtin_fpassthru);` |
|     1 | 12588 | `}` |
|    14 | 12589 | `static int vm_builtin_SplFileObject_fwrite(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12590 | `{` |
|    15 | 12591 | `	return SfoDelegate(pCtx,nArg,apArg,PH7_builtin_fwrite);` |
|     1 | 12592 | `}` |
|     - | 12593 | `/* php validates the operation in the METHOD, so the refusal names argument #1. */` |
|     6 | 12594 | `static int vm_builtin_SplFileObject_flock(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12595 | `{` |
|     - | 12596 | `	sxi32 rcChk;` |
|     7 | 12597 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 12598 | `		return rcChk;` |
|     - | 12599 | `	}` |
|     7 | 12600 | `	if( nArg > 0 && (ph7_value_to_int(apArg[0]) & 3) == 0 ){` |
|     4 | 12601 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 12602 | `			"%s(): Argument #1 ($operation) must be one of LOCK_SH, LOCK_EX, or LOCK_UN",` |
|     1 | 12603 | `			ph7_function_name(pCtx));` |
|     - | 12604 | `	}` |
|     5 | 12605 | `	return SfoDelegate(pCtx,nArg,apArg,PH7_builtin_flock);` |
|     4 | 12606 | `}` |
|     2 | 12607 | `static int vm_builtin_SplFileObject_ftruncate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12608 | `{` |
|     - | 12609 | `	sxi32 rcChk;` |
|     3 | 12610 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 12611 | `		return rcChk;` |
|     - | 12612 | `	}` |
|     3 | 12613 | `	if( nArg > 0 && ph7_value_to_int64(apArg[0]) < 0 ){` |
|     4 | 12614 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 12615 | `			"%s(): Argument #1 ($size) must be greater than or equal to 0",` |
|     1 | 12616 | `			ph7_function_name(pCtx));` |
|     - | 12617 | `	}` |
|   ! 0 | 12618 | `	return SfoDelegate(pCtx,nArg,apArg,PH7_builtin_ftruncate);` |
|     2 | 12619 | `}` |
|     4 | 12620 | `static int vm_builtin_SplFileObject_fread(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12621 | `{` |
|     - | 12622 | `	sxi32 rcChk;` |
|     5 | 12623 | `	if( !SfoChecked(pCtx,&rcChk) ){` |
|   ! 0 | 12624 | `		return rcChk;` |
|     - | 12625 | `	}` |
|     5 | 12626 | `	if( nArg < 1 \|\| ph7_value_to_int64(apArg[0]) <= 0 ){` |
|     4 | 12627 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 12628 | `			"%s(): Argument #1 ($length) must be greater than 0",` |
|     1 | 12629 | `			ph7_function_name(pCtx));` |
|     - | 12630 | `	}` |
|     3 | 12631 | `	return SfoDelegate(pCtx,nArg,apArg,PH7_builtin_fread);` |
|     3 | 12632 | `}` |
|     - | 12633 | `/* php's fseek() drops the held line first: the position moved, so what was read` |
|     - | 12634 | ` * ahead no longer describes it. */` |
|     2 | 12635 | `static int vm_builtin_SplFileObject_fseek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12636 | `{` |
|     - | 12637 | `	sxi32 rc;` |
|     3 | 12638 | `	if( SfoNeed(pCtx,&rc) == 0 ){` |
|   ! 0 | 12639 | `		return rc;` |
|     - | 12640 | `	}` |
|     3 | 12641 | `	SfoFreeLine(pCtx->pVm,PH7_ContextThis(pCtx));` |
|     3 | 12642 | `	return SfoDelegate(pCtx,nArg,apArg,PH7_builtin_fseek);` |
|     2 | 12643 | `}` |
|     - | 12644 | `/*` |
|     - | 12645 | ` * php's fgetc(): one byte, the held line dropped, and the line number advanced` |
|     - | 12646 | ` * only when the byte IS a newline.` |
|     - | 12647 | ` */` |
|     8 | 12648 | `static int vm_builtin_SplFileObject_fgetc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12649 | `{` |
|     9 | 12650 | `	ph7_vm *pVm = pCtx->pVm;` |
|     9 | 12651 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 12652 | `	io_private *pDev;` |
|     - | 12653 | `	char c;` |
|     - | 12654 | `	sxi32 rc;` |
|     4 | 12655 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     9 | 12656 | `	pDev = SfoNeed(pCtx,&rc);` |
|     9 | 12657 | `	if( pDev == 0 ){` |
|   ! 0 | 12658 | `		return rc;` |
|     - | 12659 | `	}` |
|     9 | 12660 | `	SfoFreeLine(pVm,pThis);` |
|     9 | 12661 | `	if( PH7_StreamRead(pDev,&c,sizeof(char)) < 1 ){` |
|   ! 0 | 12662 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 12663 | `		return PH7_OK;` |
|     - | 12664 | `	}` |
|     9 | 12665 | `	if( c == '\n' ){` |
|     3 | 12666 | `		PH7_NativeSetAttrInt(pVm,pThis,SFO_K,PH7_NativeAttrInt(pThis,SFO_K) + 1);` |
|     1 | 12667 | `	}` |
|     9 | 12668 | `	ph7_result_string(pCtx,&c,sizeof(char));` |
|     9 | 12669 | `	return PH7_OK;` |
|     5 | 12670 | `}` |
|     - | 12671 | `/* php's fscanf(): the format is run over the NEXT LINE, read loudly. */` |
|     4 | 12672 | `static int vm_builtin_SplFileObject_fscanf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12673 | `{` |
|     5 | 12674 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 12675 | `	const char *zFmt,*zLine;` |
|     5 | 12676 | `	int nFmt = 0,nLine = 0;` |
|     - | 12677 | `	SyBlob sLine;` |
|     - | 12678 | `	sxi32 rc;` |
|     5 | 12679 | `	if( SfoNeed(pCtx,&rc) == 0 ){` |
|   ! 0 | 12680 | `		return rc;` |
|     - | 12681 | `	}` |
|     5 | 12682 | `	if( nArg < 1 ){` |
|   ! 0 | 12683 | `		return PH7_OK;` |
|     - | 12684 | `	}` |
|     5 | 12685 | `	if( SfoReadOne(pCtx,FALSE,FALSE,&rc) != SFO_READ_OK ){` |
|   ! 0 | 12686 | `		return rc;` |
|     - | 12687 | `	}` |
|     - | 12688 | `	/* The scan allocates, and allocating moves the slot the line lives in. The` |
|     - | 12689 | `	 * length is read in its own statement: as one argument beside the call that` |
|     - | 12690 | `	 * WRITES it, nothing orders the two. */` |
|     5 | 12691 | `	zLine = SfoLine(pThis,&nLine);` |
|     5 | 12692 | `	SyBlobInit(&sLine,&pCtx->pVm->sAllocator);` |
|     5 | 12693 | `	SyBlobAppend(&sLine,zLine,(sxu32)nLine);` |
|     5 | 12694 | `	zFmt = ph7_value_to_string(apArg[0],&nFmt);` |
|     7 | 12695 | `	rc = PH7_ScanfRun(pCtx,(const char *)SyBlobData(&sLine),(int)SyBlobLength(&sLine),` |
|     2 | 12696 | `		zFmt,nFmt,&apArg[1],nArg - 1);` |
|     5 | 12697 | `	SyBlobRelease(&sLine);` |
|     5 | 12698 | `	return (int)rc;` |
|     3 | 12699 | `}` |
|     - | 12700 | `/*` |
|     - | 12701 | ` * php's seek(): rewind, then walk FORWARD through the object's own read -- so a` |
|     - | 12702 | ` * subclass's getCurrentLine() is obeyed -- and, without READ_AHEAD, land one` |
|     - | 12703 | ` * past with nothing held.` |
|     - | 12704 | ` */` |
|     6 | 12705 | `static int vm_builtin_SplFileObject_seek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12706 | `{` |
|     7 | 12707 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 | 12708 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 12709 | `	sxi64 iLine,i;` |
|     - | 12710 | `	sxi32 rc;` |
|     7 | 12711 | `	if( SfoNeed(pCtx,&rc) == 0 ){` |
|   ! 0 | 12712 | `		return rc;` |
|     - | 12713 | `	}` |
|     7 | 12714 | `	iLine = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|     7 | 12715 | `	if( iLine < 0 ){` |
|     4 | 12716 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 12717 | `			"%s(): Argument #1 ($line) must be greater than or equal to 0",` |
|     1 | 12718 | `			ph7_function_name(pCtx));` |
|     - | 12719 | `	}` |
|     5 | 12720 | `	rc = SfoRewind(pCtx);` |
|     5 | 12721 | `	if( rc != PH7_OK ){` |
|   ! 0 | 12722 | `		return rc;` |
|     - | 12723 | `	}` |
|    19 | 12724 | `	for( i = 0 ; i < iLine ; ++i ){` |
|    17 | 12725 | `		int r = SfoReadLine(pCtx,TRUE,&rc);` |
|    17 | 12726 | `		if( r == SFO_READ_THROW ){` |
|   ! 0 | 12727 | `			return rc;` |
|     - | 12728 | `		}` |
|    17 | 12729 | `		if( r != SFO_READ_OK ){` |
|     3 | 12730 | `			return PH7_OK;` |
|     - | 12731 | `		}` |
|     8 | 12732 | `	}` |
|     3 | 12733 | `	if( iLine > 0 && (SfoFlags(pThis) & SFO_READ_AHEAD) == 0 ){` |
|     3 | 12734 | `		PH7_NativeSetAttrInt(pVm,pThis,SFO_K,PH7_NativeAttrInt(pThis,SFO_K) + 1);` |
|     3 | 12735 | `		SfoFreeLine(pVm,pThis);` |
|     1 | 12736 | `	}` |
|     3 | 12737 | `	return PH7_OK;` |
|     4 | 12738 | `}` |
|     - | 12739 | `/* php's __toString(): the current line, read LOUDLY when there is none. */` |
|     4 | 12740 | `static int vm_builtin_SplFileObject_toString(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 12741 | `{` |
|     5 | 12742 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     5 | 12743 | `	int nLine = 0;` |
|     - | 12744 | `	const char *zLine;` |
|     - | 12745 | `	sxi32 rc;` |
|     2 | 12746 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     5 | 12747 | `	if( SfoNeed(pCtx,&rc) == 0 ){` |
|   ! 0 | 12748 | `		return rc;` |
|     - | 12749 | `	}` |
|     5 | 12750 | `	if( (PH7_NativeAttrInt(pThis,SFO_LS) & SFO_HAS_LINE) == 0 ){` |
|     5 | 12751 | `		if( SfoReadLine(pCtx,FALSE,&rc) != SFO_READ_OK ){` |
|   ! 0 | 12752 | `			return rc;` |
|     - | 12753 | `		}` |
|     2 | 12754 | `	}` |
|     5 | 12755 | `	zLine = SfoLine(pThis,&nLine);` |
|     5 | 12756 | `	ph7_result_string(pCtx,zLine,nLine);` |
|     5 | 12757 | `	return PH7_OK;` |
|     3 | 12758 | `}` |
|     - | 12759 | `/*` |
|     - | 12760 | ` * The declaration. Method ORDER, signatures and tentative return types are` |
|     - | 12761 | ` * spl_directory.stub.php's. php gives this class NO clone handler at all, which` |
|     - | 12762 | ` * is its "uncloneable" -- and the same @not-serializable SplFileInfo carries.` |
|     - | 12763 | ` */` |
|  5740 | 12764 | `static sxi32 VmInstallSplFileObject(ph7_vm *pVm)` |
|     5 | 12765 | `{` |
|     - | 12766 | `	static const PH7_NativePropDef aFileProp[] = {` |
|     - | 12767 | `		{ SFO_H,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 12768 | `		{ SFO_M,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "r", 0.0 }, 0 },` |
|     - | 12769 | `		{ SFO_FL, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 12770 | `		{ SFO_ML, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 12771 | `		{ SFO_D,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, ',', 0, 0.0 }, 0 },` |
|     - | 12772 | `		{ SFO_EN, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, '"', 0, 0.0 }, 0 },` |
|     - | 12773 | `		{ SFO_ES, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, '\\', 0, 0.0 }, 0 },` |
|     - | 12774 | `		{ SFO_ED, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 1, 0, 0.0 }, 0 },` |
|     - | 12775 | `		{ SFO_L,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|     - | 12776 | `		{ SFO_Z,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 12777 | `		{ SFO_LS, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 12778 | `		{ SFO_K,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 12779 | `	};` |
|     - | 12780 | `	static const PH7_NativeConstDef aFileConst[] = {` |
|     - | 12781 | `		{ "DROP_NEW_LINE", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SFO_DROP_NEW_LINE, 0, 0.0 },` |
|     - | 12782 | `		{ "READ_AHEAD",    PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SFO_READ_AHEAD, 0, 0.0 },` |
|     - | 12783 | `		{ "SKIP_EMPTY",    PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SFO_SKIP_EMPTY, 0, 0.0 },` |
|     - | 12784 | `		{ "READ_CSV",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SFO_READ_CSV, 0, 0.0 },` |
|     - | 12785 | `	};` |
|     - | 12786 | `	static const PH7_NativeMethodDef aFileMethod[] = {` |
|     - | 12787 | `		{ "__construct",   PH7_MOD_PUBLIC,` |
|     - | 12788 | `		  "string $filename, string $mode = \"r\", bool $useIncludePath = false, "` |
|     - | 12789 | `		  "$context = null", 0, vm_builtin_SplFileObject_construct },` |
|     - | 12790 | `		{ "rewind",        PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplFileObject_rewind },` |
|     - | 12791 | `		{ "eof",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplFileObject_eof },` |
|     - | 12792 | `		{ "valid",         PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplFileObject_valid },` |
|     - | 12793 | `		{ "fgets",         PH7_MOD_PUBLIC, "", "@string", vm_builtin_SplFileObject_fgets },` |
|     - | 12794 | `		{ "fread",         PH7_MOD_PUBLIC, "int $length", "@string\|false",` |
|     - | 12795 | `		  vm_builtin_SplFileObject_fread },` |
|     - | 12796 | `		{ "fgetcsv",       PH7_MOD_PUBLIC,` |
|     - | 12797 | `		  "string $separator = \",\", string $enclosure = \"\\\"\", string $escape = \"\\\\\"",` |
|     - | 12798 | `		  "@array\|false", vm_builtin_SplFileObject_fgetcsv },` |
|     - | 12799 | `		{ "fputcsv",       PH7_MOD_PUBLIC,` |
|     - | 12800 | `		  "array $fields, string $separator = \",\", string $enclosure = \"\\\"\", "` |
|     - | 12801 | `		  "string $escape = \"\\\\\", string $eol = \"\\n\"", "@int\|false",` |
|     - | 12802 | `		  vm_builtin_SplFileObject_fputcsv },` |
|     - | 12803 | `		{ "setCsvControl", PH7_MOD_PUBLIC,` |
|     - | 12804 | `		  "string $separator = \",\", string $enclosure = \"\\\"\", string $escape = \"\\\\\"",` |
|     - | 12805 | `		  "@void", vm_builtin_SplFileObject_setCsvControl },` |
|     - | 12806 | `		{ "getCsvControl", PH7_MOD_PUBLIC, "", "@array",` |
|     - | 12807 | `		  vm_builtin_SplFileObject_getCsvControl },` |
|     - | 12808 | `		{ "flock",         PH7_MOD_PUBLIC, "int $operation, &$wouldBlock = null", "@bool",` |
|     - | 12809 | `		  vm_builtin_SplFileObject_flock },` |
|     - | 12810 | `		{ "fflush",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplFileObject_fflush },` |
|     - | 12811 | `		{ "ftell",         PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_SplFileObject_ftell },` |
|     - | 12812 | `		{ "fseek",         PH7_MOD_PUBLIC, "int $offset, int $whence = SEEK_SET", "@int",` |
|     - | 12813 | `		  vm_builtin_SplFileObject_fseek },` |
|     - | 12814 | `		{ "fgetc",         PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_SplFileObject_fgetc },` |
|     - | 12815 | `		{ "fpassthru",     PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplFileObject_fpassthru },` |
|     - | 12816 | `		{ "fscanf",        PH7_MOD_PUBLIC, "string $format, mixed &...$vars", "@array\|int\|null",` |
|     - | 12817 | `		  vm_builtin_SplFileObject_fscanf },` |
|     - | 12818 | `		{ "fwrite",        PH7_MOD_PUBLIC, "string $data, ?int $length = null", "@int\|false",` |
|     - | 12819 | `		  vm_builtin_SplFileObject_fwrite },` |
|     - | 12820 | `		{ "fstat",         PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplFileObject_fstat },` |
|     - | 12821 | `		{ "ftruncate",     PH7_MOD_PUBLIC, "int $size", "@bool",` |
|     - | 12822 | `		  vm_builtin_SplFileObject_ftruncate },` |
|     - | 12823 | `		{ "current",       PH7_MOD_PUBLIC, "", "@string\|array\|false",` |
|     - | 12824 | `		  vm_builtin_SplFileObject_current },` |
|     - | 12825 | `		{ "key",           PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplFileObject_key },` |
|     - | 12826 | `		{ "next",          PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplFileObject_next },` |
|     - | 12827 | `		{ "setFlags",      PH7_MOD_PUBLIC, "int $flags", "@void",` |
|     - | 12828 | `		  vm_builtin_SplFileObject_setFlags },` |
|     - | 12829 | `		{ "getFlags",      PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplFileObject_getFlags },` |
|     - | 12830 | `		{ "setMaxLineLen", PH7_MOD_PUBLIC, "int $maxLength", "@void",` |
|     - | 12831 | `		  vm_builtin_SplFileObject_setMaxLineLen },` |
|     - | 12832 | `		{ "getMaxLineLen", PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplFileObject_getMaxLineLen },` |
|     - | 12833 | `		{ "hasChildren",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplFileObject_hasChildren },` |
|     - | 12834 | `		{ "getChildren",   PH7_MOD_PUBLIC, "", "@?RecursiveIterator",` |
|     - | 12835 | `		  vm_builtin_SplFileObject_getChildren },` |
|     - | 12836 | `		{ "seek",          PH7_MOD_PUBLIC, "int $line", "@void", vm_builtin_SplFileObject_seek },` |
|     - | 12837 | `		/* php aliases getCurrentLine() to fgets(); the override branch in` |
|     - | 12838 | `		 * SfoReadLineEx() is what makes REPLACING it mean something. */` |
|     - | 12839 | `		{ "getCurrentLine",PH7_MOD_PUBLIC, "", "@string", vm_builtin_SplFileObject_fgets },` |
|     - | 12840 | `		{ "__toString",    PH7_MOD_PUBLIC, "", "string", vm_builtin_SplFileObject_toString },` |
|     - | 12841 | `	};` |
|     - | 12842 | `	static const PH7_NativeMethodDef aTempMethod[] = {` |
|     - | 12843 | `		{ "__construct", PH7_MOD_PUBLIC, "int $maxMemory = 2097152", 0,` |
|     - | 12844 | `		  vm_builtin_SplTempFileObject_construct },` |
|     - | 12845 | `	};` |
|     - | 12846 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - | 12847 | `		{ "SplFileObject", "SplFileInfo", "RecursiveIterator,SeekableIterator",` |
|     - | 12848 | `		  PH7_CLASS_NOSERIALIZE\|PH7_CLASS_NOCLONE,` |
|     - | 12849 | `		  aFileMethod, SX_ARRAYSIZE(aFileMethod), aFileConst, SX_ARRAYSIZE(aFileConst),` |
|     - | 12850 | `		  aFileProp, SX_ARRAYSIZE(aFileProp), SfoRelease, 0, SfiPresent },` |
|     - | 12851 | `		/* php gives SplTempFileObject no handlers of its own, so it INHERITS the` |
|     - | 12852 | `		 * check pair -- uncloneable, and every method refused on an instance` |
|     - | 12853 | `		 * whose parent constructor never ran. A native class here inherits` |
|     - | 12854 | `		 * neither the refusals nor the hooks (rule 29), so both are restated. */` |
|     - | 12855 | `		{ "SplTempFileObject", "SplFileObject", 0,` |
|     - | 12856 | `		  PH7_CLASS_NOSERIALIZE\|PH7_CLASS_NOCLONE,` |
|     - | 12857 | `		  aTempMethod, SX_ARRAYSIZE(aTempMethod), 0, 0,` |
|     - | 12858 | `		  0, 0, SfoRelease, 0, SfiPresent },` |
|     - | 12859 | `	};` |
|  5745 | 12860 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 | 12861 | `}` |
|  5740 | 12862 | `PH7_PRIVATE sxi32 PH7_VmInstallSpl(ph7_vm *pVm)` |
|     5 | 12863 | `{` |
|  5745 | 12864 | `	sxi32 rc = VmInstallWeak(&(*pVm));` |
|  5745 | 12865 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 12866 | `		return rc;` |
|     - | 12867 | `	}` |
|     - | 12868 | `	/* Ordering, now that zSplLib is gone: the remaining PHP in this subsystem is` |
|     - | 12869 | `	 * the tokenizer chunk's, so these only have to satisfy each OTHER. */` |
|  5745 | 12870 | `	rc = VmInstallSplStore(&(*pVm));` |
|  5745 | 12871 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 12872 | `		return rc;` |
|     - | 12873 | `	}` |
|  5745 | 12874 | `	rc = VmInstallSplDualIterators(&(*pVm));` |
|  5745 | 12875 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 12876 | `		return rc;` |
|     - | 12877 | `	}` |
|     - | 12878 | `	/* After the dual iterators: RecursiveIteratorIterator names OuterIterator and` |
|     - | 12879 | `	 * RecursiveIterator, both declared by that table. */` |
|  5745 | 12880 | `	rc = VmInstallSplRecursiveIt(&(*pVm));` |
|  5745 | 12881 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 12882 | `		return rc;` |
|     - | 12883 | `	}` |
|  5745 | 12884 | `	rc = VmInstallSplDllist(&(*pVm));` |
|  5745 | 12885 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 12886 | `		return rc;` |
|     - | 12887 | `	}` |
|  5745 | 12888 | `	rc = VmInstallSplHeap(&(*pVm));` |
|  5745 | 12889 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 12890 | `		return rc;` |
|     - | 12891 | `	}` |
|  5745 | 12892 | `	rc = VmInstallSplFixedArray(&(*pVm));` |
|  5745 | 12893 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 12894 | `		return rc;` |
|     - | 12895 | `	}` |
|  5745 | 12896 | `	rc = VmInstallSplObjectStorage(&(*pVm));` |
|  5745 | 12897 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 12898 | `		return rc;` |
|     - | 12899 | `	}` |
|     - | 12900 | `	/* After SplObjectStorage: MultipleIterator holds the same storage and reaches` |
|     - | 12901 | `	 * its attach/detach/debug routines. */` |
|  5745 | 12902 | `	rc = VmInstallSplMultipleIterator(&(*pVm));` |
|  5745 | 12903 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 12904 | `		return rc;` |
|     - | 12905 | `	}` |
|  5745 | 12906 | `	rc = VmInstallSplFileInfo(&(*pVm));` |
|  5745 | 12907 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 12908 | `		return rc;` |
|     - | 12909 | `	}` |
|     - | 12910 | `	/* After SplFileInfo: DirectoryIterator extends it, and PH7_ClassInherit copies` |
|     - | 12911 | `	 * the base's methods DOWN (rule 14). */` |
|  5745 | 12912 | `	rc = VmInstallSplDirIterators(&(*pVm));` |
|  5745 | 12913 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 12914 | `		return rc;` |
|     - | 12915 | `	}` |
|     - | 12916 | `	/* After SplFileInfo for the same reason; the dir iterators are ahead of it` |
|     - | 12917 | `	 * only because they are declared together. */` |
|  5745 | 12918 | `	return VmInstallSplFileObject(&(*pVm));` |
|  2875 | 12919 | `}` |
|     - | 12920 |  |
|     - | 12921 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     - | 12922 |  |
|     - | 12923 | `#ifdef PH7_DISABLE_BUILTIN_FUNC` |
|     - | 12924 | `/* Tiny build: no SPL (builtin layer disabled) */` |
|     - | 12925 | `PH7_PRIVATE sxi32 PH7_VmInstallSpl(ph7_vm *pVm){ (void)pVm; return SXRET_OK; }` |
|     - | 12926 | `/* No directory iterators either, so shutdown has nothing to close. */` |
|     - | 12927 | `PH7_PRIVATE void PH7_SplDirVmRelease(ph7_vm *pVm){ (void)pVm; }` |
|     - | 12928 | `/* The writable-container fast path is called unconditionally by OP_LOAD_IDX, and its` |
|     - | 12929 | ` * SXU32_HIGH answer already means "no slot available — take the ordinary offsetGet` |
|     - | 12930 | ` * dispatch". With no SPL classes in this build that is the only answer there is, so the` |
|     - | 12931 | ` * stub keeps the tiny target LINKING without a second #ifdef at the call site. */` |
|     - | 12932 | `PH7_PRIVATE sxu32 PH7_SplDimElemSlot(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pKey,int bCreate)` |
|     - | 12933 | `{` |
|     - | 12934 | `	(void)pVm; (void)pThis; (void)pKey; (void)bCreate;` |
|     - | 12935 | `	return SXU32_HIGH;` |
|     - | 12936 | `}` |
|     - | 12937 | `#endif` |
|     - | 12938 |  |
